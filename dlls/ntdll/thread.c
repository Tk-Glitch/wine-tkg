/*
 * NT threads support
 *
 * Copyright 1996, 2003 Alexandre Julliard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "config.h"
#include "wine/port.h"

#include <assert.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <sys/types.h>
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#ifdef HAVE_SYS_TIMES_H
#include <sys/times.h>
#endif
#ifdef HAVE_SYS_SYSCALL_H
#include <sys/syscall.h>
#endif

#define NONAMELESSUNION
#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "winternl.h"
#include "wine/library.h"
#include "wine/server.h"
#include "wine/debug.h"
#include "ntdll_misc.h"
#include "ddk/wdm.h"
#include "wine/exception.h"

WINE_DEFAULT_DEBUG_CHANNEL(thread);

#ifndef PTHREAD_STACK_MIN
#define PTHREAD_STACK_MIN 16384
#endif

struct _KUSER_SHARED_DATA *user_shared_data = NULL;

extern void DECLSPEC_NORETURN __wine_syscall_dispatcher( void );

void (WINAPI *kernel32_start_process)(LPTHREAD_START_ROUTINE,void*) = NULL;

/* info passed to a starting thread */
struct startup_info
{
    TEB                            *teb;
    PRTL_THREAD_START_ROUTINE       entry_point;
    void                           *entry_arg;
};

static PEB *peb;
static PEB_LDR_DATA ldr;
static RTL_BITMAP tls_bitmap;
static RTL_BITMAP tls_expansion_bitmap;
static RTL_BITMAP fls_bitmap;
static API_SET_NAMESPACE_ARRAY apiset_map;
static int nb_threads = 1;

static RTL_CRITICAL_SECTION peb_lock;
static RTL_CRITICAL_SECTION_DEBUG critsect_debug =
{
    0, 0, &peb_lock,
    { &critsect_debug.ProcessLocksList, &critsect_debug.ProcessLocksList },
      0, 0, { (DWORD_PTR)(__FILE__ ": peb_lock") }
};
static RTL_CRITICAL_SECTION peb_lock = { &critsect_debug, -1, 0, 0, 0, 0 };

#ifdef __linux__

#ifdef HAVE_ELF_H
# include <elf.h>
#endif
#ifdef HAVE_LINK_H
# include <link.h>
#endif
#ifdef HAVE_SYS_AUXV_H
# include <sys/auxv.h>
#endif
#ifndef HAVE_GETAUXVAL
static unsigned long getauxval( unsigned long id )
{
    extern char **__wine_main_environ;
    char **ptr = __wine_main_environ;
    ElfW(auxv_t) *auxv;

    while (*ptr) ptr++;
    while (!*ptr) ptr++;
    for (auxv = (ElfW(auxv_t) *)ptr; auxv->a_type; auxv++)
        if (auxv->a_type == id) return auxv->a_un.a_val;
    return 0;
}
#endif

static ULONG_PTR get_image_addr(void)
{
    ULONG_PTR size, num, phdr_addr = getauxval( AT_PHDR );
    ElfW(Phdr) *phdr;

    if (!phdr_addr) return 0;
    phdr = (ElfW(Phdr) *)phdr_addr;
    size = getauxval( AT_PHENT );
    num = getauxval( AT_PHNUM );
    while (num--)
    {
        if (phdr->p_type == PT_PHDR) return phdr_addr - phdr->p_offset;
        phdr = (ElfW(Phdr) *)((char *)phdr + size);
    }
    return 0;
}

#elif defined(__APPLE__)
#include <mach/mach.h>
#include <mach/mach_error.h>

static ULONG_PTR get_image_addr(void)
{
    ULONG_PTR ret = 0;
#ifdef TASK_DYLD_INFO
    struct task_dyld_info dyld_info;
    mach_msg_type_number_t size = TASK_DYLD_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_DYLD_INFO, (task_info_t)&dyld_info, &size) == KERN_SUCCESS)
        ret = dyld_info.all_image_info_addr;
#endif
    return ret;
}

#else
static ULONG_PTR get_image_addr(void)
{
    return 0;
}
#endif



BOOL read_process_time(int unix_pid, int unix_tid, unsigned long clk_tck,
                       LARGE_INTEGER *kernel, LARGE_INTEGER *user)
{
#ifdef __linux__
    unsigned long usr, sys;
    char buf[512], *pos;
    FILE *fp;
    int i;

    /* based on https://github.com/torvalds/linux/blob/master/fs/proc/array.c */
    if (unix_tid != -1)
        sprintf( buf, "/proc/%u/task/%u/stat", unix_pid, unix_tid );
    else
        sprintf( buf, "/proc/%u/stat", unix_pid );
    if ((fp = fopen( buf, "r" )))
    {
        pos = fgets( buf, sizeof(buf), fp );
        fclose( fp );

        /* format of first chunk is "%d (%s) %c" - we have to skip to the last ')'
         * to avoid misinterpreting the string. */
        if (pos) pos = strrchr( pos, ')' );
        if (pos) pos = strchr( pos + 1, ' ' );
        if (pos) pos++;

        /* skip over the following fields: state, ppid, pgid, sid, tty_nr, tty_pgrp,
         * task->flags, min_flt, cmin_flt, maj_flt, cmaj_flt */
        for (i = 0; (i < 11) && pos; i++)
        {
            pos = strchr( pos + 1, ' ' );
            if (pos) pos++;
        }

        /* the next two values are user and system time */
        if (pos && (sscanf( pos, "%lu %lu", &usr, &sys ) == 2))
        {
            kernel->QuadPart = (ULONGLONG)sys * 10000000 / clk_tck;
            user->QuadPart   = (ULONGLONG)usr * 10000000 / clk_tck;
            return TRUE;
        }
    }
#endif
    return FALSE;
}


/***********************************************************************
 *		__wine_dbg_get_channel_flags  (NTDLL.@)
 *
 * Get the flags to use for a given channel, possibly setting them too in case of lazy init
 */
unsigned char __cdecl __wine_dbg_get_channel_flags( struct __wine_debug_channel *channel )
{
    return unix_funcs->dbg_get_channel_flags( channel );
}

/***********************************************************************
 *		__wine_dbg_strdup  (NTDLL.@)
 */
const char * __cdecl __wine_dbg_strdup( const char *str )
{
    return unix_funcs->dbg_strdup( str );
}

/***********************************************************************
 *		__wine_dbg_header  (NTDLL.@)
 */
int __cdecl __wine_dbg_header( enum __wine_debug_class cls, struct __wine_debug_channel *channel,
                               const char *function )
{
    return unix_funcs->dbg_header( cls, channel, function );
}

/***********************************************************************
 *		__wine_dbg_output  (NTDLL.@)
 */
int __cdecl __wine_dbg_output( const char *str )
{
    return unix_funcs->dbg_output( str );
}

static void fill_user_shared_data( struct _KUSER_SHARED_DATA *data )
{
    RTL_OSVERSIONINFOEXW version;
    SYSTEM_CPU_INFORMATION sci;
    SYSTEM_BASIC_INFORMATION sbi;
    BOOLEAN *features = data->ProcessorFeatures;

    version.dwOSVersionInfoSize = sizeof(version);
    RtlGetVersion( &version );
    virtual_get_system_info( &sbi );
    NtQuerySystemInformation( SystemCpuInformation, &sci, sizeof(sci), NULL );

    data->TickCountMultiplier         = 1 << 24;
    data->LargePageMinimum            = 2 * 1024 * 1024;
    data->NtBuildNumber               = version.dwBuildNumber;
    data->NtProductType               = version.wProductType;
    data->ProductTypeIsValid          = TRUE;
    data->NativeProcessorArchitecture = sci.Architecture;
    data->NtMajorVersion              = version.dwMajorVersion;
    data->NtMinorVersion              = version.dwMinorVersion;
    data->SuiteMask                   = version.wSuiteMask;
    data->NumberOfPhysicalPages       = sbi.MmNumberOfPhysicalPages;
    wcscpy( data->NtSystemRoot, windows_dir );

    switch (sci.Architecture)
    {
    case PROCESSOR_ARCHITECTURE_INTEL:
    case PROCESSOR_ARCHITECTURE_AMD64:
        features[PF_COMPARE_EXCHANGE_DOUBLE]              = !!(sci.FeatureSet & CPU_FEATURE_CX8);
        features[PF_MMX_INSTRUCTIONS_AVAILABLE]           = !!(sci.FeatureSet & CPU_FEATURE_MMX);
        features[PF_XMMI_INSTRUCTIONS_AVAILABLE]          = !!(sci.FeatureSet & CPU_FEATURE_SSE);
        features[PF_3DNOW_INSTRUCTIONS_AVAILABLE]         = !!(sci.FeatureSet & CPU_FEATURE_3DNOW);
        features[PF_RDTSC_INSTRUCTION_AVAILABLE]          = !!(sci.FeatureSet & CPU_FEATURE_TSC);
        features[PF_PAE_ENABLED]                          = !!(sci.FeatureSet & CPU_FEATURE_PAE);
        features[PF_XMMI64_INSTRUCTIONS_AVAILABLE]        = !!(sci.FeatureSet & CPU_FEATURE_SSE2);
        features[PF_SSE3_INSTRUCTIONS_AVAILABLE]          = !!(sci.FeatureSet & CPU_FEATURE_SSE3);
        features[PF_XSAVE_ENABLED]                        = !!(sci.FeatureSet & CPU_FEATURE_XSAVE);
        features[PF_COMPARE_EXCHANGE128]                  = !!(sci.FeatureSet & CPU_FEATURE_CX128);
        features[PF_SSE_DAZ_MODE_AVAILABLE]               = !!(sci.FeatureSet & CPU_FEATURE_DAZ);
        features[PF_NX_ENABLED]                           = !!(sci.FeatureSet & CPU_FEATURE_NX);
        features[PF_SECOND_LEVEL_ADDRESS_TRANSLATION]     = !!(sci.FeatureSet & CPU_FEATURE_2NDLEV);
        features[PF_VIRT_FIRMWARE_ENABLED]                = !!(sci.FeatureSet & CPU_FEATURE_VIRT);
        features[PF_RDWRFSGSBASE_AVAILABLE]               = !!(sci.FeatureSet & CPU_FEATURE_RDFS);
        features[PF_FASTFAIL_AVAILABLE]                   = TRUE;
        break;
    case PROCESSOR_ARCHITECTURE_ARM:
        features[PF_ARM_VFP_32_REGISTERS_AVAILABLE]       = !!(sci.FeatureSet & CPU_FEATURE_ARM_VFP_32);
        features[PF_ARM_NEON_INSTRUCTIONS_AVAILABLE]      = !!(sci.FeatureSet & CPU_FEATURE_ARM_NEON);
        features[PF_ARM_V8_INSTRUCTIONS_AVAILABLE]        = (sci.Level >= 8);
        break;
    case PROCESSOR_ARCHITECTURE_ARM64:
        features[PF_ARM_V8_INSTRUCTIONS_AVAILABLE]        = TRUE;
        features[PF_ARM_V8_CRC32_INSTRUCTIONS_AVAILABLE]  = !!(sci.FeatureSet & CPU_FEATURE_ARM_V8_CRC32);
        features[PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE] = !!(sci.FeatureSet & CPU_FEATURE_ARM_V8_CRYPTO);
        break;
    }
    data->ActiveProcessorCount = NtCurrentTeb()->Peb->NumberOfProcessors;
    data->ActiveGroupCount = 1;
}

HANDLE user_shared_data_init_done(void)
{
    static const WCHAR wine_usdW[] = {'\\','K','e','r','n','e','l','O','b','j','e','c','t','s',
                                      '\\','_','_','w','i','n','e','_','u','s','e','r','_','s','h','a','r','e','d','_','d','a','t','a',0};
    OBJECT_ATTRIBUTES attr = {sizeof(attr)};
    UNICODE_STRING wine_usd_str;
    LARGE_INTEGER section_size;
    NTSTATUS status;
    HANDLE section;
    SIZE_T size;
    void *addr;
    int res, fd, needs_close;

    section_size.QuadPart = sizeof(*user_shared_data);

    RtlInitUnicodeString( &wine_usd_str, wine_usdW );
    InitializeObjectAttributes( &attr, &wine_usd_str, OBJ_OPENIF, NULL, NULL );
    if ((status = NtCreateSection( &section, SECTION_ALL_ACCESS, &attr,
                                   &section_size, PAGE_READWRITE, SEC_COMMIT, NULL )) &&
        status != STATUS_OBJECT_NAME_EXISTS)
    {
        MESSAGE( "wine: failed to create or open the USD section: %08x\n", status );
        exit(1);
    }

    if (status != STATUS_OBJECT_NAME_EXISTS)
    {
        addr = NULL;
        size = sizeof(*user_shared_data);
        if ((status = NtMapViewOfSection( section, NtCurrentProcess(), &addr, 0, 0, 0,
                                          &size, 0, 0, PAGE_READWRITE )))
        {
            MESSAGE( "wine: failed to initialize the USD section: %08x\n", status );
            exit(1);
        }

        fill_user_shared_data( addr );
        NtUnmapViewOfSection( NtCurrentProcess(), addr );
    }

    if ((res = server_get_unix_fd( section, 0, &fd, &needs_close, NULL, NULL )) ||
        (user_shared_data != mmap( user_shared_data, sizeof(*user_shared_data),
                                   PROT_READ, MAP_SHARED | MAP_FIXED, fd, 0 )))
    {
        MESSAGE( "wine: failed to remap the process USD: %d\n", res );
        exit(1);
    }
    if (needs_close) close( fd );

    return section;
}

/***********************************************************************
 *           thread_init
 *
 * Setup the initial thread.
 *
 * NOTES: The first allocated TEB on NT is at 0x7ffde000.
 */
TEB *thread_init(void)
{
    TEB *teb;
    void *addr;
    SIZE_T size;
    NTSTATUS status;
    struct ntdll_thread_data *thread_data;

    virtual_init();
    signal_init_early();

    /* reserve space for shared user data */

    addr = (void *)0x7ffe0000;
    size = 0x2000;
    status = NtAllocateVirtualMemory( NtCurrentProcess(), &addr, 0, &size,
                                      MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE );
    if (status)
    {
        MESSAGE( "wine: failed to map the shared user data: %08x\n", status );
        exit(1);
    }
    user_shared_data = addr;

#if defined(__APPLE__) && defined(__x86_64__)
    *((DWORD*)((char*)user_shared_data + 0x1000)) = __wine_syscall_dispatcher;
#endif

    /* Init this field early for x86_64 syscall thunks. */
    user_shared_data->SystemCallPad[0] = 1;

    /* allocate and initialize the PEB and initial TEB */

    teb = virtual_alloc_first_teb();
    peb = teb->Peb;
    peb->FastPebLock        = &peb_lock;
    peb->ApiSetMap          = &apiset_map;
    peb->TlsBitmap          = &tls_bitmap;
    peb->TlsExpansionBitmap = &tls_expansion_bitmap;
    peb->FlsBitmap          = &fls_bitmap;
    peb->LdrData            = &ldr;
    peb->OSMajorVersion     = 5;
    peb->OSMinorVersion     = 1;
    peb->OSBuildNumber      = 0xA28;
    peb->OSPlatformId       = VER_PLATFORM_WIN32_NT;
    ldr.Length = sizeof(ldr);
    ldr.Initialized = TRUE;
    RtlInitializeBitMap( &tls_bitmap, peb->TlsBitmapBits, sizeof(peb->TlsBitmapBits) * 8 );
    RtlInitializeBitMap( &tls_expansion_bitmap, peb->TlsExpansionBitmapBits,
                         sizeof(peb->TlsExpansionBitmapBits) * 8 );
    RtlInitializeBitMap( &fls_bitmap, peb->FlsBitmapBits, sizeof(peb->FlsBitmapBits) * 8 );
    RtlSetBits( peb->TlsBitmap, 0, 1 ); /* TLS index 0 is reserved and should be initialized to NULL. */
    RtlSetBits( peb->FlsBitmap, 0, 1 );
    InitializeListHead( &peb->FlsListHead );
    InitializeListHead( &ldr.InLoadOrderModuleList );
    InitializeListHead( &ldr.InMemoryOrderModuleList );
    InitializeListHead( &ldr.InInitializationOrderModuleList );
    *(ULONG_PTR *)peb->Reserved = get_image_addr();

    /*
     * Starting with Vista, the first user to log on has session id 1.
     * Session id 0 is for processes that don't interact with the user (like services).
     */
    peb->SessionId = 1;

    thread_data = (struct ntdll_thread_data *)&teb->GdiTebBatch;
    thread_data->request_fd = -1;
    thread_data->reply_fd   = -1;
    thread_data->wait_fd[0] = -1;
    thread_data->wait_fd[1] = -1;
    thread_data->esync_queue_fd = -1;
    thread_data->esync_apc_fd = -1;
    thread_data->fsync_apc_futex = NULL;

    unix_funcs->dbg_init();
    unix_funcs->get_paths( &build_dir, &data_dir, &config_dir );
    fill_cpu_info();
    return teb;
}

BOOL read_process_memory_stats(int unix_pid, VM_COUNTERS *pvmi)
{
    BOOL ret = FALSE;
#ifdef __linux__
    unsigned long size, resident, shared, trs, drs, lrs, dt;
    char buf[512];
    FILE *fp;

    sprintf( buf, "/proc/%u/statm", unix_pid );
    if ((fp = fopen( buf, "r" )))
    {
        if (fscanf( fp, "%lu %lu %lu %lu %lu %lu %lu",
            &size, &resident, &shared, &trs, &drs, &lrs, &dt ) == 7)
        {
            pvmi->VirtualSize = size * page_size;
            pvmi->WorkingSetSize = resident * page_size;
            pvmi->PrivatePageCount = size - shared;

            /* these values are not available through /proc/pid/statm */
            pvmi->PeakVirtualSize = pvmi->VirtualSize;
            pvmi->PageFaultCount = 0;
            pvmi->PeakWorkingSetSize = pvmi->WorkingSetSize;
            pvmi->QuotaPagedPoolUsage = pvmi->VirtualSize;
            pvmi->QuotaPeakPagedPoolUsage = pvmi->QuotaPagedPoolUsage;
            pvmi->QuotaPeakNonPagedPoolUsage = 0;
            pvmi->QuotaNonPagedPoolUsage = 0;
            pvmi->PagefileUsage = 0;
            pvmi->PeakPagefileUsage = 0;

            ret = TRUE;
        }
        fclose( fp );
    }
#endif
    return ret;
}

/***********************************************************************
 *           abort_thread
 */
void abort_thread( int status )
{
    pthread_sigmask( SIG_BLOCK, &server_block_set, NULL );
    if (InterlockedDecrement( &nb_threads ) <= 0) _exit( get_unix_exit_code( status ));
    signal_exit_thread( status );
}


/***********************************************************************
 *           exit_thread
 */
void exit_thread( int status )
{
    close( ntdll_get_thread_data()->wait_fd[0] );
    close( ntdll_get_thread_data()->wait_fd[1] );
    close( ntdll_get_thread_data()->reply_fd );
    close( ntdll_get_thread_data()->request_fd );
    pthread_exit( UIntToPtr(status) );
}


/***********************************************************************
 *           RtlExitUserThread  (NTDLL.@)
 */
void WINAPI RtlExitUserThread( ULONG status )
{
    static void *prev_teb;
    shmlocal_t *shmlocal;
    sigset_t sigset;
    TEB *teb;

    if (status)  /* send the exit code to the server (0 is already the default) */
    {
        SERVER_START_REQ( terminate_thread )
        {
            req->handle    = wine_server_obj_handle( GetCurrentThread() );
            req->exit_code = status;
            wine_server_call( req );
        }
        SERVER_END_REQ;
    }

    if (InterlockedCompareExchange( &nb_threads, 0, 0 ) <= 0)
    {
        LdrShutdownProcess();
        pthread_sigmask( SIG_BLOCK, &server_block_set, NULL );
        signal_exit_process( get_unix_exit_code( status ));
    }

    LdrShutdownThread();
    RtlFreeThreadActivationContextStack();

    shmlocal = InterlockedExchangePointer( &NtCurrentTeb()->Reserved5[2], NULL );
    if (shmlocal) NtUnmapViewOfSection( NtCurrentProcess(), shmlocal );

    pthread_sigmask( SIG_BLOCK, &server_block_set, NULL );

    if ((teb = InterlockedExchangePointer( &prev_teb, NtCurrentTeb() )))
    {
        struct ntdll_thread_data *thread_data = (struct ntdll_thread_data *)&teb->GdiTebBatch;

        if (thread_data->pthread_id)
        {
            pthread_join( thread_data->pthread_id, NULL );
            virtual_free_teb( teb );
        }
    }

    sigemptyset( &sigset );
    sigaddset( &sigset, SIGQUIT );
    pthread_sigmask( SIG_BLOCK, &sigset, NULL );
    if (!InterlockedDecrement( &nb_threads )) _exit( status );

    signal_exit_thread( status );
}


/***********************************************************************
 *           start_thread
 *
 * Startup routine for a newly created thread.
 */
static void start_thread( struct startup_info *info )
{
    BOOL suspend;
    TEB *teb = info->teb;
    struct ntdll_thread_data *thread_data = (struct ntdll_thread_data *)&teb->GdiTebBatch;
    struct debug_info debug_info;

    debug_info.str_pos = debug_info.out_pos = 0;
    thread_data->debug_info = &debug_info;
    thread_data->pthread_id = pthread_self();

    signal_init_thread( teb );
    server_init_thread( info->entry_point, &suspend );
    signal_start_thread( (LPTHREAD_START_ROUTINE)info->entry_point, info->entry_arg, suspend );
}


/***********************************************************************
 *              NtCreateThreadEx   (NTDLL.@)
 */
NTSTATUS WINAPI NtCreateThreadEx( HANDLE *handle_ptr, ACCESS_MASK access, OBJECT_ATTRIBUTES *thread_attr,
                                  HANDLE process, LPTHREAD_START_ROUTINE start, void *param,
                                  ULONG flags, ULONG zero_bits, ULONG stack_commit,
                                  ULONG stack_reserve, PPS_ATTRIBUTE_LIST ps_attr_list )
{
    sigset_t sigset;
    pthread_t pthread_id;
    pthread_attr_t pthread_attr;
    struct ntdll_thread_data *thread_data;
    struct startup_info *info;
    BOOLEAN suspended = !!(flags & THREAD_CREATE_FLAGS_CREATE_SUSPENDED);
    CLIENT_ID *id = NULL;
    HANDLE handle = 0, actctx = 0;
    TEB *teb = NULL;
    DWORD tid = 0;
    int request_pipe[2];
    NTSTATUS status;
    SIZE_T extra_stack = PTHREAD_STACK_MIN;
    data_size_t len = 0;
    struct object_attributes *objattr = NULL;
    INITIAL_TEB stack;

    TRACE("(%p, %d, %p, %p, %p, %p, %u, %u, %u, %u, %p)\n",
          handle_ptr, access, thread_attr, process, start, param, flags,
          zero_bits, stack_commit, stack_reserve, ps_attr_list);

    if (ps_attr_list != NULL)
    {
        PS_ATTRIBUTE *ps_attr,
                     *ps_attr_end = (PS_ATTRIBUTE *)((UINT_PTR)ps_attr_list + ps_attr_list->TotalLength);
        for (ps_attr = &ps_attr_list->Attributes[0]; ps_attr < ps_attr_end; ps_attr++)
        {
            switch (ps_attr->Attribute)
            {
            case PS_ATTRIBUTE_CLIENT_ID:
                /* TODO validate ps_attr->Size == sizeof(CLIENT_ID) */
                /* TODO set *ps_attr->ReturnLength */
                id = ps_attr->ValuePtr;
                break;
            default:
                FIXME("Unsupported attribute %08X\n", ps_attr->Attribute);
                break;
            }
        }
    }

    if (access == (ACCESS_MASK)0)
        access = THREAD_ALL_ACCESS;

    if (process != NtCurrentProcess())
    {
        apc_call_t call;
        apc_result_t result;

        memset( &call, 0, sizeof(call) );

        call.create_thread.type    = APC_CREATE_THREAD;
        call.create_thread.func    = wine_server_client_ptr( start );
        call.create_thread.arg     = wine_server_client_ptr( param );
        call.create_thread.reserve = stack_reserve;
        call.create_thread.commit  = stack_commit;
        call.create_thread.suspend = suspended;
        status = server_queue_process_apc( process, &call, &result );
        if (status != STATUS_SUCCESS) return status;

        if (result.create_thread.status == STATUS_SUCCESS)
        {
            if (id) id->UniqueThread = ULongToHandle(result.create_thread.tid);
            if (handle_ptr) *handle_ptr = wine_server_ptr_handle( result.create_thread.handle );
            else NtClose( wine_server_ptr_handle( result.create_thread.handle ));
        }
        return result.create_thread.status;
    }

    if ((status = alloc_object_attributes( thread_attr, &objattr, &len ))) return status;

    if (server_pipe( request_pipe ) == -1)
    {
        RtlFreeHeap( GetProcessHeap(), 0, objattr );
        return STATUS_TOO_MANY_OPENED_FILES;
    }
    wine_server_send_fd( request_pipe[0] );

    SERVER_START_REQ( new_thread )
    {
        req->process    = wine_server_obj_handle( process );
        req->access     = access;
        req->suspend    = suspended;
        req->request_fd = request_pipe[0];
        wine_server_add_data( req, objattr, len );
        if (!(status = wine_server_call( req )))
        {
            handle = wine_server_ptr_handle( reply->handle );
            tid = reply->tid;
        }
        close( request_pipe[0] );
    }
    SERVER_END_REQ;

    RtlFreeHeap( GetProcessHeap(), 0, objattr );
    if (status)
    {
        close( request_pipe[1] );
        return status;
    }

    pthread_sigmask( SIG_BLOCK, &server_block_set, &sigset );

    if ((status = virtual_alloc_teb( &teb ))) goto error;

    teb->ClientId.UniqueProcess = ULongToHandle(GetCurrentProcessId());
    teb->ClientId.UniqueThread  = ULongToHandle(tid);

    /* create default activation context frame for new thread */
    RtlGetActiveActivationContext(&actctx);
    if (actctx)
    {
        RTL_ACTIVATION_CONTEXT_STACK_FRAME *frame;

        frame = RtlAllocateHeap(GetProcessHeap(), 0, sizeof(*frame));
        frame->Previous = NULL;
        frame->ActivationContext = actctx;
        frame->Flags = 0;
        teb->ActivationContextStack.ActiveFrame = frame;
    }

    info = (struct startup_info *)(teb + 1);
    info->teb         = teb;
    info->entry_point = start;
    info->entry_arg   = param;

    if ((status = virtual_alloc_thread_stack( &stack, stack_reserve, stack_commit, &extra_stack )))
        goto error;

    teb->Tib.StackBase = stack.StackBase;
    teb->Tib.StackLimit = stack.StackLimit;
    teb->DeallocationStack = stack.DeallocationStack;

    thread_data = (struct ntdll_thread_data *)&teb->GdiTebBatch;
    thread_data->request_fd  = request_pipe[1];
    thread_data->reply_fd    = -1;
    thread_data->wait_fd[0]  = -1;
    thread_data->wait_fd[1]  = -1;
    thread_data->start_stack = (char *)teb->Tib.StackBase;
    thread_data->esync_queue_fd = -1;
    thread_data->esync_apc_fd = -1;
    thread_data->fsync_apc_futex = NULL;

    pthread_attr_init( &pthread_attr );
    pthread_attr_setstack( &pthread_attr, teb->DeallocationStack,
                         (char *)teb->Tib.StackBase + extra_stack - (char *)teb->DeallocationStack );
    pthread_attr_setguardsize( &pthread_attr, 0 );
    pthread_attr_setscope( &pthread_attr, PTHREAD_SCOPE_SYSTEM ); /* force creating a kernel thread */
    InterlockedIncrement( &nb_threads );
    if (pthread_create( &pthread_id, &pthread_attr, (void * (*)(void *))start_thread, info ))
    {
        InterlockedDecrement( &nb_threads );
        pthread_attr_destroy( &pthread_attr );
        status = STATUS_NO_MEMORY;
        goto error;
    }
    pthread_attr_destroy( &pthread_attr );
    pthread_sigmask( SIG_SETMASK, &sigset, NULL );

    if (id) id->UniqueThread = ULongToHandle(tid);
    if (handle_ptr) *handle_ptr = handle;
    else NtClose( handle );

    return STATUS_SUCCESS;

error:
    if (teb) virtual_free_teb( teb );
    if (handle) NtClose( handle );
    pthread_sigmask( SIG_SETMASK, &sigset, NULL );
    close( request_pipe[1] );
    return status;
}

NTSTATUS WINAPI NtCreateThread( HANDLE *handle_ptr, ACCESS_MASK access, OBJECT_ATTRIBUTES *attr, HANDLE process,
                                CLIENT_ID *id, CONTEXT *context, INITIAL_TEB *teb, BOOLEAN suspended )
{
    LPTHREAD_START_ROUTINE entry;
    void *arg;
    ULONG flags = suspended ? THREAD_CREATE_FLAGS_CREATE_SUSPENDED : 0;
    PS_ATTRIBUTE_LIST attr_list, *pattr_list = NULL;

#if defined(__i386__)
        entry = (LPTHREAD_START_ROUTINE) context->Eax;
        arg = (void *)context->Ebx;
#elif defined(__x86_64__)
        entry = (LPTHREAD_START_ROUTINE) context->Rcx;
        arg = (void *)context->Rdx;
#elif defined(__arm__)
        entry = (LPTHREAD_START_ROUTINE) context->R0;
        arg = (void *)context->R1;
#elif defined(__aarch64__)
        entry = (LPTHREAD_START_ROUTINE) context->u.X0;
        arg = (void *)context->u.X1;
#elif defined(__powerpc__)
        entry = (LPTHREAD_START_ROUTINE) context->Gpr3;
        arg = (void *)context->Gpr4;
#endif

    if (id)
    {
        attr_list.TotalLength = sizeof(PS_ATTRIBUTE_LIST);
        attr_list.Attributes[0].Attribute = PS_ATTRIBUTE_CLIENT_ID;
        attr_list.Attributes[0].Size = sizeof(CLIENT_ID);
        attr_list.Attributes[0].ValuePtr = id;
        attr_list.Attributes[0].ReturnLength = NULL;
        pattr_list = &attr_list;
    }

    return NtCreateThreadEx(handle_ptr, access, attr, process, entry, arg, flags, 0, 0, 0, pattr_list);
}

NTSTATUS WINAPI __syscall_NtCreateThread( HANDLE *handle_ptr, ACCESS_MASK access, OBJECT_ATTRIBUTES *attr,
                                          HANDLE process, CLIENT_ID *id, CONTEXT *context, INITIAL_TEB *teb,
                                          BOOLEAN suspended );
NTSTATUS WINAPI __syscall_NtCreateThreadEx( HANDLE *handle_ptr, ACCESS_MASK access, OBJECT_ATTRIBUTES *attr,
                                            HANDLE process, LPTHREAD_START_ROUTINE start, void *param,
                                            ULONG flags, ULONG zero_bits, ULONG stack_commit,
                                            ULONG stack_reserve, PPS_ATTRIBUTE_LIST ps_attr_list );

/***********************************************************************
 *              RtlCreateUserThread   (NTDLL.@)
 */
NTSTATUS WINAPI RtlCreateUserThread( HANDLE process, SECURITY_DESCRIPTOR *descr,
                                     BOOLEAN suspended, void *stack_addr,
                                     SIZE_T stack_reserve, SIZE_T stack_commit,
                                     PRTL_THREAD_START_ROUTINE entry, void *arg,
                                     HANDLE *handle_ptr, CLIENT_ID *id )
{
    OBJECT_ATTRIBUTES thread_attr;
    InitializeObjectAttributes( &thread_attr, NULL, 0, NULL, descr );
    if (stack_addr)
        FIXME("stack_addr != NULL is unimplemented\n");

    if (NtCurrentTeb()->Peb->OSMajorVersion < 6)
    {
        /* Use old API. */
        CONTEXT context = { 0 };

        if (stack_commit)
            FIXME("stack_commit != 0 is unimplemented\n");
        if (stack_reserve)
            FIXME("stack_reserve != 0 is unimplemented\n");

        context.ContextFlags = CONTEXT_FULL;
#if defined(__i386__)
        context.Eax = (DWORD)entry;
        context.Ebx = (DWORD)arg;
#elif defined(__x86_64__)
        context.Rcx = (ULONG_PTR)entry;
        context.Rdx = (ULONG_PTR)arg;
#elif defined(__arm__)
        context.R0 = (DWORD)entry;
        context.R1 = (DWORD)arg;
#elif defined(__aarch64__)
        context.u.X0 = (DWORD_PTR)entry;
        context.u.X1 = (DWORD_PTR)arg;
#elif defined(__powerpc__)
        context.Gpr3 = (DWORD)entry;
        context.Gpr4 = (DWORD)arg;
#endif

#if defined(__i386__) || defined(__x86_64__)
        return __syscall_NtCreateThread(handle_ptr, (ACCESS_MASK)0, &thread_attr, process, id, &context, NULL, suspended);
#else
        return NtCreateThread(handle_ptr, (ACCESS_MASK)0, &thread_attr, process, id, &context, NULL, suspended);
#endif
    }
    else
    {
        /* Use new API from Vista+. */
        ULONG flags = suspended ? THREAD_CREATE_FLAGS_CREATE_SUSPENDED : 0;
        PS_ATTRIBUTE_LIST attr_list, *pattr_list = NULL;

        if (id)
        {
            attr_list.TotalLength = sizeof(PS_ATTRIBUTE_LIST);
            attr_list.Attributes[0].Attribute = PS_ATTRIBUTE_CLIENT_ID;
            attr_list.Attributes[0].Size = sizeof(CLIENT_ID);
            attr_list.Attributes[0].ValuePtr = id;
            attr_list.Attributes[0].ReturnLength = NULL;
            pattr_list = &attr_list;
        }

#if defined(__i386__) || defined(__x86_64__)
        return __syscall_NtCreateThreadEx(handle_ptr, (ACCESS_MASK)0, &thread_attr, process, (LPTHREAD_START_ROUTINE)entry, arg, flags, 0, stack_commit, stack_reserve, pattr_list);
#else
        return NtCreateThreadEx(handle_ptr, (ACCESS_MASK)0, &thread_attr, process, (LPTHREAD_START_ROUTINE)entry, arg, flags, 0, stack_commit, stack_reserve, pattr_list);
#endif
    }
}


/******************************************************************************
 *              RtlGetNtGlobalFlags   (NTDLL.@)
 */
ULONG WINAPI RtlGetNtGlobalFlags(void)
{
    if (!peb) return 0;  /* init not done yet */
    return peb->NtGlobalFlag;
}


/***********************************************************************
 *              NtOpenThread   (NTDLL.@)
 *              ZwOpenThread   (NTDLL.@)
 */
NTSTATUS WINAPI NtOpenThread( HANDLE *handle, ACCESS_MASK access,
                              const OBJECT_ATTRIBUTES *attr, const CLIENT_ID *id )
{
    NTSTATUS ret;

    SERVER_START_REQ( open_thread )
    {
        req->tid        = HandleToULong(id->UniqueThread);
        req->access     = access;
        req->attributes = attr ? attr->Attributes : 0;
        ret = wine_server_call( req );
        *handle = wine_server_ptr_handle( reply->handle );
    }
    SERVER_END_REQ;
    return ret;
}


/******************************************************************************
 *              NtSuspendThread   (NTDLL.@)
 *              ZwSuspendThread   (NTDLL.@)
 */
NTSTATUS WINAPI NtSuspendThread( HANDLE handle, PULONG count )
{
    NTSTATUS ret;

    SERVER_START_REQ( suspend_thread )
    {
        req->handle = wine_server_obj_handle( handle );
        if (!(ret = wine_server_call( req )))
        {
            if (count) *count = reply->count;
        }
    }
    SERVER_END_REQ;
    return ret;
}


/******************************************************************************
 *              NtResumeThread   (NTDLL.@)
 *              ZwResumeThread   (NTDLL.@)
 */
NTSTATUS WINAPI NtResumeThread( HANDLE handle, PULONG count )
{
    NTSTATUS ret;

    SERVER_START_REQ( resume_thread )
    {
        req->handle = wine_server_obj_handle( handle );
        if (!(ret = wine_server_call( req )))
        {
            if (count) *count = reply->count;
        }
    }
    SERVER_END_REQ;
    return ret;
}


/******************************************************************************
 *              NtAlertResumeThread   (NTDLL.@)
 *              ZwAlertResumeThread   (NTDLL.@)
 */
NTSTATUS WINAPI NtAlertResumeThread( HANDLE handle, PULONG count )
{
    FIXME( "stub: should alert thread %p\n", handle );
    return NtResumeThread( handle, count );
}


/******************************************************************************
 *              NtAlertThread   (NTDLL.@)
 *              ZwAlertThread   (NTDLL.@)
 */
NTSTATUS WINAPI NtAlertThread( HANDLE handle )
{
    FIXME( "stub: %p\n", handle );
    return STATUS_NOT_IMPLEMENTED;
}


/******************************************************************************
 *              NtTerminateThread  (NTDLL.@)
 *              ZwTerminateThread  (NTDLL.@)
 */
NTSTATUS WINAPI NtTerminateThread( HANDLE handle, LONG exit_code )
{
    NTSTATUS ret;
    BOOL self = (handle == GetCurrentThread());

    if (!self || exit_code)
    {
        SERVER_START_REQ( terminate_thread )
        {
            req->handle    = wine_server_obj_handle( handle );
            req->exit_code = exit_code;
            ret = wine_server_call( req );
            self = !ret && reply->self;
        }
        SERVER_END_REQ;
    }
    if (self) abort_thread( exit_code );
    return ret;
}


/******************************************************************************
 *              NtQueueApcThread  (NTDLL.@)
 */
NTSTATUS WINAPI NtQueueApcThread( HANDLE handle, PNTAPCFUNC func, ULONG_PTR arg1,
                                  ULONG_PTR arg2, ULONG_PTR arg3 )
{
    NTSTATUS ret;
    SERVER_START_REQ( queue_apc )
    {
        req->handle = wine_server_obj_handle( handle );
        if (func)
        {
            req->call.type              = APC_USER;
            req->call.user.user.func    = wine_server_client_ptr( func );
            req->call.user.user.args[0] = arg1;
            req->call.user.user.args[1] = arg2;
            req->call.user.user.args[2] = arg3;
        }
        else req->call.type = APC_NONE;  /* wake up only */
        ret = wine_server_call( req );
    }
    SERVER_END_REQ;
    return ret;
}


/******************************************************************************
 *              RtlPushFrame  (NTDLL.@)
 */
void WINAPI RtlPushFrame( TEB_ACTIVE_FRAME *frame )
{
    frame->Previous = NtCurrentTeb()->ActiveFrame;
    NtCurrentTeb()->ActiveFrame = frame;
}


/******************************************************************************
 *              RtlPopFrame  (NTDLL.@)
 */
void WINAPI RtlPopFrame( TEB_ACTIVE_FRAME *frame )
{
    NtCurrentTeb()->ActiveFrame = frame->Previous;
}


/******************************************************************************
 *              RtlGetFrame  (NTDLL.@)
 */
TEB_ACTIVE_FRAME * WINAPI RtlGetFrame(void)
{
    return NtCurrentTeb()->ActiveFrame;
}


/***********************************************************************
 *              set_thread_context
 */
NTSTATUS set_thread_context( HANDLE handle, const context_t *context, BOOL *self )
{
    NTSTATUS ret;

    SERVER_START_REQ( set_thread_context )
    {
        req->handle  = wine_server_obj_handle( handle );
        wine_server_add_data( req, context, sizeof(*context) );
        ret = wine_server_call( req );
        *self = reply->self;
    }
    SERVER_END_REQ;

    return ret;
}


/***********************************************************************
 *              get_thread_context
 */
NTSTATUS get_thread_context( HANDLE handle, context_t *context, unsigned int flags, BOOL *self )
{
    NTSTATUS ret;

    SERVER_START_REQ( get_thread_context )
    {
        req->handle  = wine_server_obj_handle( handle );
        req->flags   = flags;
        wine_server_set_reply( req, context, sizeof(*context) );
        ret = wine_server_call( req );
        *self = reply->self;
        handle = wine_server_ptr_handle( reply->handle );
    }
    SERVER_END_REQ;

    if (ret == STATUS_PENDING)
    {
        LARGE_INTEGER timeout;
        timeout.QuadPart = -1000000;
        if (NtWaitForSingleObject( handle, FALSE, &timeout ))
        {
            NtClose( handle );
            return STATUS_ACCESS_DENIED;
        }
        SERVER_START_REQ( get_thread_context )
        {
            req->handle  = wine_server_obj_handle( handle );
            req->flags   = flags;
            wine_server_set_reply( req, context, sizeof(*context) );
            ret = wine_server_call( req );
        }
        SERVER_END_REQ;
    }
    return ret;
}


/******************************************************************************
 *              NtQueryInformationThread  (NTDLL.@)
 *              ZwQueryInformationThread  (NTDLL.@)
 */
NTSTATUS WINAPI NtQueryInformationThread( HANDLE handle, THREADINFOCLASS class,
                                          void *data, ULONG length, ULONG *ret_len )
{
    NTSTATUS status;

    switch(class)
    {
    case ThreadBasicInformation:
        {
            THREAD_BASIC_INFORMATION info;
            const ULONG_PTR affinity_mask = get_system_affinity_mask();

            SERVER_START_REQ( get_thread_info )
            {
                req->handle = wine_server_obj_handle( handle );
                req->tid_in = 0;
                if (!(status = wine_server_call( req )))
                {
                    info.ExitStatus             = reply->exit_code;
                    info.TebBaseAddress         = wine_server_get_ptr( reply->teb );
                    info.ClientId.UniqueProcess = ULongToHandle(reply->pid);
                    info.ClientId.UniqueThread  = ULongToHandle(reply->tid);
                    info.AffinityMask           = reply->affinity & affinity_mask;
                    info.Priority               = reply->priority;
                    info.BasePriority           = reply->priority;  /* FIXME */
                }
            }
            SERVER_END_REQ;
            if (status == STATUS_SUCCESS)
            {
                if (data) memcpy( data, &info, min( length, sizeof(info) ));
                if (ret_len) *ret_len = min( length, sizeof(info) );
            }
        }
        return status;
    case ThreadAffinityMask:
        {
            const ULONG_PTR affinity_mask = get_system_affinity_mask();
            ULONG_PTR affinity = 0;

            SERVER_START_REQ( get_thread_info )
            {
                req->handle = wine_server_obj_handle( handle );
                req->tid_in = 0;
                if (!(status = wine_server_call( req )))
                    affinity = reply->affinity & affinity_mask;
            }
            SERVER_END_REQ;
            if (status == STATUS_SUCCESS)
            {
                if (data) memcpy( data, &affinity, min( length, sizeof(affinity) ));
                if (ret_len) *ret_len = min( length, sizeof(affinity) );
            }
        }
        return status;
    case ThreadTimes:
        {
            KERNEL_USER_TIMES   kusrt;
            int unix_pid, unix_tid;

            /* We need to do a server call to get the creation time, exit time, PID and TID */
            /* This works on any thread */
            SERVER_START_REQ( get_thread_times )
            {
                req->handle = wine_server_obj_handle( handle );
                status = wine_server_call( req );
                if (status == STATUS_SUCCESS)
                {
                    kusrt.CreateTime.QuadPart = reply->creation_time;
                    kusrt.ExitTime.QuadPart = reply->exit_time;
                    unix_pid = reply->unix_pid;
                    unix_tid = reply->unix_tid;
                }
            }
            SERVER_END_REQ;
            if (status == STATUS_SUCCESS)
            {
                unsigned long clk_tck = sysconf(_SC_CLK_TCK);
                BOOL filled_times = FALSE;

#ifdef __linux__
                /* only /proc provides exact values for a specific thread */
                if (unix_pid != -1 && unix_tid != -1)
                    filled_times = read_process_time(unix_pid, unix_tid, clk_tck, &kusrt.KernelTime, &kusrt.UserTime);
#endif

                /* get values for current process instead */
                if (!filled_times && handle == GetCurrentThread())
                {
                    struct tms time_buf;
                    times(&time_buf);

                    kusrt.KernelTime.QuadPart = (ULONGLONG)time_buf.tms_stime * 10000000 / clk_tck;
                    kusrt.UserTime.QuadPart   = (ULONGLONG)time_buf.tms_utime * 10000000 / clk_tck;
                    filled_times = TRUE;
                }

                /* unable to determine exact values, fill with zero */
                if (!filled_times)
                {
                    static int once;
                    if (!once++)
                        FIXME("Cannot get kerneltime or usertime of other threads\n");

                    kusrt.KernelTime.QuadPart = 0;
                    kusrt.UserTime.QuadPart   = 0;
                }

                if (data) memcpy( data, &kusrt, min( length, sizeof(kusrt) ));
                if (ret_len) *ret_len = min( length, sizeof(kusrt) );
            }
        }
        return status;

    case ThreadDescriptorTableEntry:
        return get_thread_ldt_entry( handle, data, length, ret_len );

    case ThreadAmILastThread:
        {
            SERVER_START_REQ(get_thread_info)
            {
                req->handle = wine_server_obj_handle( handle );
                req->tid_in = 0;
                status = wine_server_call( req );
                if (status == STATUS_SUCCESS)
                {
                    BOOLEAN last = reply->last;
                    if (data) memcpy( data, &last, min( length, sizeof(last) ));
                    if (ret_len) *ret_len = min( length, sizeof(last) );
                }
            }
            SERVER_END_REQ;
            return status;
        }
    case ThreadQuerySetWin32StartAddress:
        {
            SERVER_START_REQ( get_thread_info )
            {
                req->handle = wine_server_obj_handle( handle );
                req->tid_in = 0;
                status = wine_server_call( req );
                if (status == STATUS_SUCCESS)
                {
                    PRTL_THREAD_START_ROUTINE entry = wine_server_get_ptr( reply->entry_point );
                    if (data) memcpy( data, &entry, min( length, sizeof(entry) ) );
                    if (ret_len) *ret_len = min( length, sizeof(entry) );
                }
            }
            SERVER_END_REQ;
            return status;
        }
    case ThreadGroupInformation:
        {
            const ULONG_PTR affinity_mask = get_system_affinity_mask();
            GROUP_AFFINITY affinity;

            memset(&affinity, 0, sizeof(affinity));
            affinity.Group = 0; /* Wine only supports max 64 processors */

            SERVER_START_REQ( get_thread_info )
            {
                req->handle = wine_server_obj_handle( handle );
                req->tid_in = 0;
                if (!(status = wine_server_call( req )))
                    affinity.Mask = reply->affinity & affinity_mask;
            }
            SERVER_END_REQ;
            if (status == STATUS_SUCCESS)
            {
                if (data) memcpy( data, &affinity, min( length, sizeof(affinity) ));
                if (ret_len) *ret_len = min( length, sizeof(affinity) );
            }
        }
        return status;
    case ThreadIsIoPending:
        FIXME( "ThreadIsIoPending info class not supported yet\n" );
        if (length != sizeof(BOOL)) return STATUS_INFO_LENGTH_MISMATCH;
        if (!data) return STATUS_ACCESS_DENIED;

        *(BOOL*)data = FALSE;
        if (ret_len) *ret_len = sizeof(BOOL);
        return STATUS_SUCCESS;
    case ThreadSuspendCount:
        {
            ULONG count = 0;

            if (length != sizeof(ULONG)) return STATUS_INFO_LENGTH_MISMATCH;
            if (!data) return STATUS_ACCESS_VIOLATION;

            SERVER_START_REQ( get_thread_info )
            {
                req->handle = wine_server_obj_handle( handle );
                req->tid_in = 0;
                if (!(status = wine_server_call( req )))
                    count = reply->suspend_count;
            }
            SERVER_END_REQ;

            if (!status)
                *(ULONG *)data = count;

            return status;
        }
    case ThreadDescription:
        {
            THREAD_DESCRIPTION_INFORMATION *info = data;
            data_size_t len, desc_len = 0;
            WCHAR *ptr;

            len = length >= sizeof(*info) ? length - sizeof(*info) : 0;
            ptr = info ? (WCHAR *)(info + 1) : NULL;

            SERVER_START_REQ( get_thread_info )
            {
                req->handle = wine_server_obj_handle( handle );
                if (ptr) wine_server_set_reply( req, ptr, len );
                status = wine_server_call( req );
                desc_len = reply->desc_len;
            }
            SERVER_END_REQ;

            if (!info)
                status = STATUS_BUFFER_TOO_SMALL;
            else if (status == STATUS_SUCCESS)
            {
                info->Description.Length = info->Description.MaximumLength = desc_len;
                info->Description.Buffer = ptr;
            }

            if (ret_len && (status == STATUS_SUCCESS || status == STATUS_BUFFER_TOO_SMALL))
                *ret_len = sizeof(*info) + desc_len;
        }
        return status;
    case ThreadPriority:
    case ThreadBasePriority:
    case ThreadImpersonationToken:
    case ThreadEnableAlignmentFaultFixup:
    case ThreadEventPair_Reusable:
    case ThreadZeroTlsCell:
    case ThreadPerformanceCount:
    case ThreadIdealProcessor:
    case ThreadPriorityBoost:
    case ThreadSetTlsArrayAddress:
    default:
        FIXME( "info class %d not supported yet\n", class );
        return STATUS_NOT_IMPLEMENTED;
    }
}


/******************************************************************************
 *              NtSetInformationThread  (NTDLL.@)
 *              ZwSetInformationThread  (NTDLL.@)
 */
NTSTATUS WINAPI NtSetInformationThread( HANDLE handle, THREADINFOCLASS class,
                                        LPCVOID data, ULONG length )
{
    NTSTATUS status;
    switch(class)
    {
    case ThreadZeroTlsCell:
        if (handle == GetCurrentThread())
        {
            LIST_ENTRY *entry;
            DWORD index;

            if (length != sizeof(DWORD)) return STATUS_INVALID_PARAMETER;
            index = *(const DWORD *)data;
            if (index < TLS_MINIMUM_AVAILABLE)
            {
                RtlAcquirePebLock();
                for (entry = tls_links.Flink; entry != &tls_links; entry = entry->Flink)
                {
                    TEB *teb = CONTAINING_RECORD(entry, TEB, TlsLinks);
                    teb->TlsSlots[index] = 0;
                }
                RtlReleasePebLock();
            }
            else
            {
                index -= TLS_MINIMUM_AVAILABLE;
                if (index >= 8 * sizeof(NtCurrentTeb()->Peb->TlsExpansionBitmapBits))
                    return STATUS_INVALID_PARAMETER;
                RtlAcquirePebLock();
                for (entry = tls_links.Flink; entry != &tls_links; entry = entry->Flink)
                {
                    TEB *teb = CONTAINING_RECORD(entry, TEB, TlsLinks);
                    if (teb->TlsExpansionSlots) teb->TlsExpansionSlots[index] = 0;
                }
                RtlReleasePebLock();
            }
            return STATUS_SUCCESS;
        }
        FIXME( "ZeroTlsCell not supported on other threads\n" );
        return STATUS_NOT_IMPLEMENTED;

    case ThreadImpersonationToken:
        {
            const HANDLE *phToken = data;
            if (length != sizeof(HANDLE)) return STATUS_INVALID_PARAMETER;
            TRACE("Setting ThreadImpersonationToken handle to %p\n", *phToken );
            SERVER_START_REQ( set_thread_info )
            {
                req->handle   = wine_server_obj_handle( handle );
                req->token    = wine_server_obj_handle( *phToken );
                req->mask     = SET_THREAD_INFO_TOKEN;
                status = wine_server_call( req );
            }
            SERVER_END_REQ;
        }
        return status;
    case ThreadBasePriority:
        {
            const DWORD *pprio = data;
            if (length != sizeof(DWORD)) return STATUS_INVALID_PARAMETER;
            SERVER_START_REQ( set_thread_info )
            {
                req->handle   = wine_server_obj_handle( handle );
                req->priority = *pprio;
                req->mask     = SET_THREAD_INFO_PRIORITY;
                status = wine_server_call( req );
            }
            SERVER_END_REQ;
        }
        return status;
    case ThreadAffinityMask:
        {
            const ULONG_PTR affinity_mask = get_system_affinity_mask();
            ULONG_PTR req_aff;

            if (length != sizeof(ULONG_PTR)) return STATUS_INVALID_PARAMETER;
            req_aff = *(const ULONG_PTR *)data & affinity_mask;
            if (!req_aff) return STATUS_INVALID_PARAMETER;

            SERVER_START_REQ( set_thread_info )
            {
                req->handle   = wine_server_obj_handle( handle );
                req->affinity = req_aff;
                req->mask     = SET_THREAD_INFO_AFFINITY;
                status = wine_server_call( req );
            }
            SERVER_END_REQ;
        }
        return status;
    case ThreadHideFromDebugger:
        /* pretend the call succeeded to satisfy some code protectors */
        return STATUS_SUCCESS;
    case ThreadQuerySetWin32StartAddress:
        {
            const PRTL_THREAD_START_ROUTINE *entry = data;
            if (length != sizeof(PRTL_THREAD_START_ROUTINE)) return STATUS_INVALID_PARAMETER;
            SERVER_START_REQ( set_thread_info )
            {
                req->handle   = wine_server_obj_handle( handle );
                req->mask     = SET_THREAD_INFO_ENTRYPOINT;
                req->entry_point = wine_server_client_ptr( *entry );
                status = wine_server_call( req );
            }
            SERVER_END_REQ;
        }
        return status;
    case ThreadGroupInformation:
        {
            const ULONG_PTR affinity_mask = get_system_affinity_mask();
            const GROUP_AFFINITY *req_aff;

            if (length != sizeof(*req_aff)) return STATUS_INVALID_PARAMETER;
            if (!data) return STATUS_ACCESS_VIOLATION;
            req_aff = data;

            /* On Windows the request fails if the reserved fields are set */
            if (req_aff->Reserved[0] || req_aff->Reserved[1] || req_aff->Reserved[2])
                return STATUS_INVALID_PARAMETER;

            /* Wine only supports max 64 processors */
            if (req_aff->Group) return STATUS_INVALID_PARAMETER;
            if (req_aff->Mask & ~affinity_mask) return STATUS_INVALID_PARAMETER;
            if (!req_aff->Mask) return STATUS_INVALID_PARAMETER;
            SERVER_START_REQ( set_thread_info )
            {
                req->handle   = wine_server_obj_handle( handle );
                req->affinity = req_aff->Mask;
                req->mask     = SET_THREAD_INFO_AFFINITY;
                status = wine_server_call( req );
            }
            SERVER_END_REQ;
        }
        return status;
    case ThreadDescription:
        {
            const THREAD_DESCRIPTION_INFORMATION *info = data;

            if (length != sizeof(*info)) return STATUS_INFO_LENGTH_MISMATCH;
            if (!info) return STATUS_ACCESS_VIOLATION;

            if (info->Description.Length != info->Description.MaximumLength) return STATUS_INVALID_PARAMETER;
            if (info->Description.Length && !info->Description.Buffer) return STATUS_ACCESS_VIOLATION;

            SERVER_START_REQ( set_thread_info )
            {
                req->handle = wine_server_obj_handle( handle );
                req->mask   = SET_THREAD_INFO_DESCRIPTION;
                wine_server_add_data( req, info->Description.Buffer, info->Description.Length );
                status = wine_server_call( req );
            }
            SERVER_END_REQ;
        }
        return status;
    case ThreadBasicInformation:
    case ThreadTimes:
    case ThreadPriority:
    case ThreadDescriptorTableEntry:
    case ThreadEnableAlignmentFaultFixup:
    case ThreadEventPair_Reusable:
    case ThreadPerformanceCount:
    case ThreadAmILastThread:
    case ThreadIdealProcessor:
    case ThreadPriorityBoost:
    case ThreadSetTlsArrayAddress:
    case ThreadIsIoPending:
    default:
        FIXME( "info class %d not supported yet\n", class );
        return STATUS_NOT_IMPLEMENTED;
    }
}

/******************************************************************************
 * NtGetCurrentProcessorNumber (NTDLL.@)
 *
 * Return the processor, on which the thread is running
 *
 */
ULONG WINAPI NtGetCurrentProcessorNumber(void)
{
    ULONG processor;

#if defined(__linux__) && defined(__NR_getcpu)
    int res = syscall(__NR_getcpu, &processor, NULL, NULL);
    if (res != -1) return processor;
#endif

    if (NtCurrentTeb()->Peb->NumberOfProcessors > 1)
    {
        ULONG_PTR thread_mask, processor_mask;
        NTSTATUS status;

        status = NtQueryInformationThread(GetCurrentThread(), ThreadAffinityMask,
                                          &thread_mask, sizeof(thread_mask), NULL);
        if (status == STATUS_SUCCESS)
        {
            for (processor = 0; processor < NtCurrentTeb()->Peb->NumberOfProcessors; processor++)
            {
                processor_mask = (1 << processor);
                if (thread_mask & processor_mask)
                {
                    if (thread_mask != processor_mask)
                        FIXME("need multicore support (%d processors)\n",
                              NtCurrentTeb()->Peb->NumberOfProcessors);
                    return processor;
                }
            }
        }
    }

    /* fallback to the first processor */
    return 0;
}
