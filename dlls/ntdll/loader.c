/*
 * Loader functions
 *
 * Copyright 1995, 2003 Alexandre Julliard
 * Copyright 2002 Dmitry Timoshkov for CodeWeavers
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
#ifdef HAVE_LINK_H
# include <link.h>
#endif
#ifdef HAVE_SYS_MMAN_H
# include <sys/mman.h>
#endif

#include "ntstatus.h"
#define WIN32_NO_STATUS
#define NONAMELESSUNION
#define NONAMELESSSTRUCT
#include "windef.h"
#include "winnt.h"
#include "winternl.h"
#include "delayloadhandler.h"

#include "wine/exception.h"
#include "wine/library.h"
#include "wine/debug.h"
#include "wine/list.h"
#include "wine/server.h"
#include "ntdll_misc.h"
#include "ddk/wdm.h"
#include "esync.h"
#include "fsync.h"

WINE_DEFAULT_DEBUG_CHANNEL(module);
WINE_DECLARE_DEBUG_CHANNEL(relay);
WINE_DECLARE_DEBUG_CHANNEL(snoop);
WINE_DECLARE_DEBUG_CHANNEL(loaddll);
WINE_DECLARE_DEBUG_CHANNEL(imports);

#ifdef _WIN64
#define DEFAULT_SECURITY_COOKIE_64  (((ULONGLONG)0x00002b99 << 32) | 0x2ddfa232)
#endif
#define DEFAULT_SECURITY_COOKIE_32  0xbb40e64e
#define DEFAULT_SECURITY_COOKIE_16  (DEFAULT_SECURITY_COOKIE_32 >> 16)

/* we don't want to include winuser.h */
#define RT_MANIFEST                         ((ULONG_PTR)24)
#define ISOLATIONAWARE_MANIFEST_RESOURCE_ID ((ULONG_PTR)2)

typedef DWORD (CALLBACK *DLLENTRYPROC)(HMODULE,DWORD,LPVOID);
typedef void  (CALLBACK *LDRENUMPROC)(LDR_DATA_TABLE_ENTRY *, void *, BOOLEAN *);

const struct unix_funcs *unix_funcs = NULL;

/* windows directory */
const WCHAR windows_dir[] = {'C',':','\\','w','i','n','d','o','w','s',0};
/* system directory with trailing backslash */
const WCHAR system_dir[] = {'C',':','\\','w','i','n','d','o','w','s','\\',
                            's','y','s','t','e','m','3','2','\\',0};
const WCHAR syswow64_dir[] = {'C',':','\\','w','i','n','d','o','w','s','\\',
                              's','y','s','w','o','w','6','4','\\',0};


/* system search path */
static const WCHAR system_path[] =
    {'C',':','\\','w','i','n','d','o','w','s','\\','s','y','s','t','e','m','3','2',';',
     'C',':','\\','w','i','n','d','o','w','s','\\','s','y','s','t','e','m',';',
     'C',':','\\','w','i','n','d','o','w','s',0};

static const WCHAR dotW[] = {'.',0};

#define IS_OPTION_TRUE(ch) ((ch) == 'y' || (ch) == 'Y' || (ch) == 't' || (ch) == 'T' || (ch) == '1')


static BOOL imports_fixup_done = FALSE;  /* set once the imports have been fixed up, before attaching them */
static BOOL process_detaching = FALSE;  /* set on process detach to avoid deadlocks with thread detach */
static int free_lib_count;   /* recursion depth of LdrUnloadDll calls */
static ULONG path_safe_mode;  /* path mode set by RtlSetSearchPathMode */
static ULONG dll_safe_mode = 1;  /* dll search mode */
static UNICODE_STRING dll_directory;  /* extra path for LdrSetDllDirectory */
static DWORD default_search_flags;  /* default flags set by LdrSetDefaultDllDirectories */

struct dll_dir_entry
{
    struct list entry;
    WCHAR       dir[1];
};

static struct list dll_dir_list = LIST_INIT( dll_dir_list );  /* extra dirs from LdrAddDllDirectory */

static BOOL hide_wine_exports = FALSE;  /* try to hide ntdll wine exports from applications */

struct ldr_notification
{
    struct list                    entry;
    PLDR_DLL_NOTIFICATION_FUNCTION callback;
    void                           *context;
};

static struct list ldr_notifications = LIST_INIT( ldr_notifications );

static const char * const reason_names[] =
{
    "PROCESS_DETACH",
    "PROCESS_ATTACH",
    "THREAD_ATTACH",
    "THREAD_DETACH",
    NULL, NULL, NULL, NULL,
    "WINE_PREATTACH"
};

static const WCHAR dllW[] = {'.','d','l','l',0};

#define HASH_MAP_SIZE 32
static LIST_ENTRY hash_table[HASH_MAP_SIZE];

/* internal representation of 32bit modules. per process. */
typedef struct _wine_modref
{
    LDR_DATA_TABLE_ENTRY  ldr;
    dev_t                 dev;
    ino_t                 ino;
    void                 *so_handle;
    int                   alloc_deps;
    int                   nDeps;
    struct _wine_modref **deps;
} WINE_MODREF;

/* info about the current builtin dll load */
/* used to keep track of things across the register_dll constructor call */
struct builtin_load_info
{
    const WCHAR          *load_path;
    const UNICODE_STRING *filename;
    NTSTATUS              status;
    WINE_MODREF          *wm;
};

static struct builtin_load_info default_load_info;
static struct builtin_load_info *builtin_load_info = &default_load_info;

static UINT tls_module_count;      /* number of modules with TLS directory */
static IMAGE_TLS_DIRECTORY *tls_dirs;  /* array of TLS directories */
LIST_ENTRY tls_links = { &tls_links, &tls_links };

static RTL_CRITICAL_SECTION loader_section;
static RTL_CRITICAL_SECTION_DEBUG critsect_debug =
{
    0, 0, &loader_section,
    { &critsect_debug.ProcessLocksList, &critsect_debug.ProcessLocksList },
      0, 0, { (DWORD_PTR)(__FILE__ ": loader_section") }
};
static RTL_CRITICAL_SECTION loader_section = { &critsect_debug, -1, 0, 0, 0, 0 };

static CRITICAL_SECTION dlldir_section;
static CRITICAL_SECTION_DEBUG dlldir_critsect_debug =
{
    0, 0, &dlldir_section,
    { &dlldir_critsect_debug.ProcessLocksList, &dlldir_critsect_debug.ProcessLocksList },
      0, 0, { (DWORD_PTR)(__FILE__ ": dlldir_section") }
};
static CRITICAL_SECTION dlldir_section = { &dlldir_critsect_debug, -1, 0, 0, 0, 0 };

static CRITICAL_SECTION fls_section;
static CRITICAL_SECTION_DEBUG fls_critsect_debug =
{
    0, 0, &fls_section,
    { &fls_critsect_debug.ProcessLocksList, &fls_critsect_debug.ProcessLocksList },
      0, 0, { (DWORD_PTR)(__FILE__ ": fls_section") }
};
static CRITICAL_SECTION fls_section = { &fls_critsect_debug, -1, 0, 0, 0, 0 };;

static WINE_MODREF *cached_modref;
static WINE_MODREF *current_modref;
static WINE_MODREF *last_failed_modref;

static NTSTATUS load_dll( const WCHAR *load_path, const WCHAR *libname, const WCHAR *default_ext,
                          DWORD flags, WINE_MODREF** pwm );
static NTSTATUS process_attach( WINE_MODREF *wm, LPVOID lpReserved );
static FARPROC find_ordinal_export( HMODULE module, const IMAGE_EXPORT_DIRECTORY *exports,
                                    DWORD exp_size, DWORD ordinal, LPCWSTR load_path );
static FARPROC find_named_export( HMODULE module, const IMAGE_EXPORT_DIRECTORY *exports,
                                  DWORD exp_size, const char *name, int hint, LPCWSTR load_path );

/* convert PE image VirtualAddress to Real Address */
static inline void *get_rva( HMODULE module, DWORD va )
{
    return (void *)((char *)module + va);
}

/* check whether the file name contains a path */
static inline BOOL contains_path( LPCWSTR name )
{
    return ((*name && (name[1] == ':')) || wcschr(name, '/') || wcschr(name, '\\'));
}

static WCHAR *strcasestrW( const WCHAR *str, const WCHAR *sub )
{
    while (*str)
    {
        const WCHAR *p1 = str, *p2 = sub;
        while (*p1 && *p2 && RtlDowncaseUnicodeChar(*p1) == RtlDowncaseUnicodeChar(*p2)) { p1++; p2++; }
        if (!*p2) return (WCHAR *)str;
        str++;
    }
    return NULL;
}

#define RTL_UNLOAD_EVENT_TRACE_NUMBER 64

typedef struct _RTL_UNLOAD_EVENT_TRACE
{
    void *BaseAddress;
    SIZE_T SizeOfImage;
    ULONG Sequence;
    ULONG TimeDateStamp;
    ULONG CheckSum;
    WCHAR ImageName[32];
} RTL_UNLOAD_EVENT_TRACE, *PRTL_UNLOAD_EVENT_TRACE;

static RTL_UNLOAD_EVENT_TRACE unload_traces[RTL_UNLOAD_EVENT_TRACE_NUMBER];
static RTL_UNLOAD_EVENT_TRACE *unload_trace_ptr;
static unsigned int unload_trace_seq;

static void WINAPI lock_fls_section( PVOID dummy )
{
    RtlEnterCriticalSection( &fls_section );
}

static void WINAPI unlock_fls_section( PVOID dummy )
{
    RtlLeaveCriticalSection( &fls_section );
}

static void module_push_unload_trace( const LDR_DATA_TABLE_ENTRY *ldr )
{
    RTL_UNLOAD_EVENT_TRACE *ptr = &unload_traces[unload_trace_seq];
    unsigned int len = min(sizeof(ptr->ImageName) - sizeof(WCHAR), ldr->BaseDllName.Length);

    ptr->BaseAddress = ldr->DllBase;
    ptr->SizeOfImage = ldr->SizeOfImage;
    ptr->Sequence = unload_trace_seq;
    ptr->TimeDateStamp = ldr->TimeDateStamp;
    ptr->CheckSum = ldr->CheckSum;
    memcpy(ptr->ImageName, ldr->BaseDllName.Buffer, len);
    ptr->ImageName[len / sizeof(*ptr->ImageName)] = 0;

    unload_trace_seq = (unload_trace_seq + 1) % ARRAY_SIZE(unload_traces);
    unload_trace_ptr = unload_traces;
}

/*********************************************************************
 *           RtlGetUnloadEventTrace [NTDLL.@]
 */
RTL_UNLOAD_EVENT_TRACE * WINAPI RtlGetUnloadEventTrace(void)
{
    return unload_traces;
}

/*********************************************************************
 *           RtlGetUnloadEventTraceEx [NTDLL.@]
 */
void WINAPI RtlGetUnloadEventTraceEx(ULONG **size, ULONG **count, void **trace)
{
    static unsigned int element_size = sizeof(*unload_traces);
    static unsigned int element_count = ARRAY_SIZE(unload_traces);

    *size = &element_size;
    *count = &element_count;
    *trace = &unload_trace_ptr;
}

/*************************************************************************
 *		call_dll_entry_point
 *
 * Some brain-damaged dlls (ir32_32.dll for instance) modify ebx in
 * their entry point, so we need a small asm wrapper. Testing indicates
 * that only modifying esi leads to a crash, so use this one to backup
 * ebp while running the dll entry proc.
 */
#ifdef __i386__
extern BOOL call_dll_entry_point( DLLENTRYPROC proc, void *module, UINT reason, void *reserved );
__ASM_GLOBAL_FUNC(call_dll_entry_point,
                  "pushl %ebp\n\t"
                  __ASM_CFI(".cfi_adjust_cfa_offset 4\n\t")
                  __ASM_CFI(".cfi_rel_offset %ebp,0\n\t")
                  "movl %esp,%ebp\n\t"
                  __ASM_CFI(".cfi_def_cfa_register %ebp\n\t")
                  "pushl %ebx\n\t"
                  __ASM_CFI(".cfi_rel_offset %ebx,-4\n\t")
                  "pushl %esi\n\t"
                  __ASM_CFI(".cfi_rel_offset %esi,-8\n\t")
                  "pushl %edi\n\t"
                  __ASM_CFI(".cfi_rel_offset %edi,-12\n\t")
                  "movl %ebp,%esi\n\t"
                  __ASM_CFI(".cfi_def_cfa_register %esi\n\t")
                  "pushl 20(%ebp)\n\t"
                  "pushl 16(%ebp)\n\t"
                  "pushl 12(%ebp)\n\t"
                  "movl 8(%ebp),%eax\n\t"
                  "call *%eax\n\t"
                  "movl %esi,%ebp\n\t"
                  __ASM_CFI(".cfi_def_cfa_register %ebp\n\t")
                  "leal -12(%ebp),%esp\n\t"
                  "popl %edi\n\t"
                  __ASM_CFI(".cfi_same_value %edi\n\t")
                  "popl %esi\n\t"
                  __ASM_CFI(".cfi_same_value %esi\n\t")
                  "popl %ebx\n\t"
                  __ASM_CFI(".cfi_same_value %ebx\n\t")
                  "popl %ebp\n\t"
                  __ASM_CFI(".cfi_def_cfa %esp,4\n\t")
                  __ASM_CFI(".cfi_same_value %ebp\n\t")
                  "ret" )
#else /* __i386__ */
static inline BOOL call_dll_entry_point( DLLENTRYPROC proc, void *module,
                                         UINT reason, void *reserved )
{
    return proc( module, reason, reserved );
}
#endif /* __i386__ */


#if defined(__i386__) || defined(__x86_64__) || defined(__arm__) || defined(__aarch64__)
/*************************************************************************
 *		stub_entry_point
 *
 * Entry point for stub functions.
 */
static void stub_entry_point( const char *dll, const char *name, void *ret_addr )
{
    EXCEPTION_RECORD rec;

    rec.ExceptionCode           = EXCEPTION_WINE_STUB;
    rec.ExceptionFlags          = EH_NONCONTINUABLE;
    rec.ExceptionRecord         = NULL;
    rec.ExceptionAddress        = ret_addr;
    rec.NumberParameters        = 2;
    rec.ExceptionInformation[0] = (ULONG_PTR)dll;
    rec.ExceptionInformation[1] = (ULONG_PTR)name;
    for (;;) RtlRaiseException( &rec );
}


#include "pshpack1.h"
#ifdef __i386__
struct stub
{
    BYTE        pushl1;     /* pushl $name */
    const char *name;
    BYTE        pushl2;     /* pushl $dll */
    const char *dll;
    BYTE        call;       /* call stub_entry_point */
    DWORD       entry;
};
#elif defined(__arm__)
struct stub
{
    DWORD ldr_r0;        /* ldr r0, $dll */
    DWORD ldr_r1;        /* ldr r1, $name */
    DWORD mov_r2_lr;     /* mov r2, lr */
    DWORD ldr_pc_pc;     /* ldr pc, [pc, #4] */
    const char *dll;
    const char *name;
    const void* entry;
};
#elif defined(__aarch64__)
struct stub
{
    DWORD ldr_x0;        /* ldr x0, $dll */
    DWORD ldr_x1;        /* ldr x1, $name */
    DWORD mov_x2_lr;     /* mov x2, lr */
    DWORD ldr_x16;       /* ldr x16, $entry */
    DWORD br_x16;        /* br x16 */
    const char *dll;
    const char *name;
    const void *entry;
};
#else
struct stub
{
    BYTE movq_rdi[2];      /* movq $dll,%rdi */
    const char *dll;
    BYTE movq_rsi[2];      /* movq $name,%rsi */
    const char *name;
    BYTE movq_rsp_rdx[4];  /* movq (%rsp),%rdx */
    BYTE movq_rax[2];      /* movq $entry, %rax */
    const void* entry;
    BYTE jmpq_rax[2];      /* jmp %rax */
};
#endif
#include "poppack.h"

/*************************************************************************
 *		allocate_stub
 *
 * Allocate a stub entry point.
 */
static ULONG_PTR allocate_stub( const char *dll, const char *name )
{
#define MAX_SIZE 65536
    static struct stub *stubs;
    static unsigned int nb_stubs;
    struct stub *stub;

    if (nb_stubs >= MAX_SIZE / sizeof(*stub)) return 0xdeadbeef;

    if (!stubs)
    {
        SIZE_T size = MAX_SIZE;
        if (NtAllocateVirtualMemory( NtCurrentProcess(), (void **)&stubs, 0, &size,
                                     MEM_COMMIT, PAGE_EXECUTE_READWRITE ) != STATUS_SUCCESS)
            return 0xdeadbeef;
    }
    stub = &stubs[nb_stubs++];
#ifdef __i386__
    stub->pushl1    = 0x68;  /* pushl $name */
    stub->name      = name;
    stub->pushl2    = 0x68;  /* pushl $dll */
    stub->dll       = dll;
    stub->call      = 0xe8;  /* call stub_entry_point */
    stub->entry     = (BYTE *)stub_entry_point - (BYTE *)(&stub->entry + 1);
#elif defined(__arm__)
    stub->ldr_r0    = 0xe59f0008;   /* ldr r0, [pc, #8] ($dll) */
    stub->ldr_r1    = 0xe59f1008;   /* ldr r1, [pc, #8] ($name) */
    stub->mov_r2_lr = 0xe1a0200e;   /* mov r2, lr */
    stub->ldr_pc_pc = 0xe59ff004;   /* ldr pc, [pc, #4] */
    stub->dll       = dll;
    stub->name      = name;
    stub->entry     = stub_entry_point;
#elif defined(__aarch64__)
    stub->ldr_x0    = 0x580000a0; /* ldr x0, #20 ($dll) */
    stub->ldr_x1    = 0x580000c1; /* ldr x1, #24 ($name) */
    stub->mov_x2_lr = 0xaa1e03e2; /* mov x2, lr */
    stub->ldr_x16   = 0x580000d0; /* ldr x16, #24 ($entry) */
    stub->br_x16    = 0xd61f0200; /* br x16 */
    stub->dll       = dll;
    stub->name      = name;
    stub->entry     = stub_entry_point;
#else
    stub->movq_rdi[0]     = 0x48;  /* movq $dll,%rdi */
    stub->movq_rdi[1]     = 0xbf;
    stub->dll             = dll;
    stub->movq_rsi[0]     = 0x48;  /* movq $name,%rsi */
    stub->movq_rsi[1]     = 0xbe;
    stub->name            = name;
    stub->movq_rsp_rdx[0] = 0x48;  /* movq (%rsp),%rdx */
    stub->movq_rsp_rdx[1] = 0x8b;
    stub->movq_rsp_rdx[2] = 0x14;
    stub->movq_rsp_rdx[3] = 0x24;
    stub->movq_rax[0]     = 0x48;  /* movq $entry, %rax */
    stub->movq_rax[1]     = 0xb8;
    stub->entry           = stub_entry_point;
    stub->jmpq_rax[0]     = 0xff;  /* jmp %rax */
    stub->jmpq_rax[1]     = 0xe0;
#endif
    return (ULONG_PTR)stub;
}

#else  /* __i386__ */
static inline ULONG_PTR allocate_stub( const char *dll, const char *name ) { return 0xdeadbeef; }
#endif  /* __i386__ */

/* call ldr notifications */
static void call_ldr_notifications( ULONG reason, LDR_DATA_TABLE_ENTRY *module )
{
    struct ldr_notification *notify, *notify_next;
    LDR_DLL_NOTIFICATION_DATA data;

    data.Loaded.Flags       = 0;
    data.Loaded.FullDllName = &module->FullDllName;
    data.Loaded.BaseDllName = &module->BaseDllName;
    data.Loaded.DllBase     = module->DllBase;
    data.Loaded.SizeOfImage = module->SizeOfImage;

    LIST_FOR_EACH_ENTRY_SAFE( notify, notify_next, &ldr_notifications, struct ldr_notification, entry )
    {
        TRACE_(relay)("\1Call LDR notification callback (proc=%p,reason=%u,data=%p,context=%p)\n",
                notify->callback, reason, &data, notify->context );

        notify->callback(reason, &data, notify->context);

        TRACE_(relay)("\1Ret  LDR notification callback (proc=%p,reason=%u,data=%p,context=%p)\n",
                notify->callback, reason, &data, notify->context );
    }
}

/*************************************************************************
 *      hash_basename
 *
 * Calculates the bucket index of a dll using the basename.
 */
static ULONG hash_basename(const WCHAR *basename)
{
    WORD version = MAKEWORD(NtCurrentTeb()->Peb->OSMinorVersion,
                            NtCurrentTeb()->Peb->OSMajorVersion);
    ULONG hash = 0;

    if (version >= 0x0602)
    {
        for (; *basename; basename++)
            hash = hash * 65599 + towupper(*basename);
    }
    else if (version == 0x0601)
    {
        for (; *basename; basename++)
            hash = hash + 65599 * towupper(*basename);
    }
    else
        hash = towupper(basename[0]) - 'A';

    return hash & (HASH_MAP_SIZE-1);
}

/*************************************************************************
 *      recompute_hash_maps
 *
 * Recomputes the LDR hash map (necessary when windows version changes).
 */
static void recompute_hash_map(void)
{
    LIST_ENTRY *mark, *entry;
    LDR_DATA_TABLE_ENTRY *mod;

    mark = &NtCurrentTeb()->Peb->LdrData->InLoadOrderModuleList;
    for (entry = mark->Flink; entry != mark; entry = entry->Flink)
    {
        mod = CONTAINING_RECORD(entry, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);
        RemoveEntryList( &mod->HashLinks );
        InsertTailList( &hash_table[hash_basename(mod->BaseDllName.Buffer)], &mod->HashLinks );
    }
}

/*************************************************************************
 *		get_modref
 *
 * Looks for the referenced HMODULE in the current process
 * The loader_section must be locked while calling this function.
 */
static WINE_MODREF *get_modref( HMODULE hmod )
{
    PLIST_ENTRY mark, entry;
    PLDR_DATA_TABLE_ENTRY mod;

    if (cached_modref && cached_modref->ldr.DllBase == hmod) return cached_modref;

    mark = &NtCurrentTeb()->Peb->LdrData->InMemoryOrderModuleList;
    for (entry = mark->Flink; entry != mark; entry = entry->Flink)
    {
        mod = CONTAINING_RECORD(entry, LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks);
        if (mod->DllBase == hmod)
            return cached_modref = CONTAINING_RECORD(mod, WINE_MODREF, ldr);
    }
    return NULL;
}


/**********************************************************************
 *	    find_basename_module
 *
 * Find a module from its base name.
 * The loader_section must be locked while calling this function
 */
static WINE_MODREF *find_basename_module( LPCWSTR name )
{
    PLIST_ENTRY mark, entry;
    UNICODE_STRING name_str;

    RtlInitUnicodeString( &name_str, name );

    if (cached_modref && RtlEqualUnicodeString( &name_str, &cached_modref->ldr.BaseDllName, TRUE ))
        return cached_modref;

    mark = &hash_table[hash_basename(name)];
    for (entry = mark->Flink; entry != mark; entry = entry->Flink)
    {
        LDR_DATA_TABLE_ENTRY *mod = CONTAINING_RECORD(entry, LDR_DATA_TABLE_ENTRY, HashLinks);
        if (RtlEqualUnicodeString( &name_str, &mod->BaseDllName, TRUE ))
        {
            cached_modref = CONTAINING_RECORD(mod, WINE_MODREF, ldr);
            return cached_modref;
        }
    }
    return NULL;
}


/**********************************************************************
 *	    find_fullname_module
 *
 * Find a module from its full path name.
 * The loader_section must be locked while calling this function
 */
static WINE_MODREF *find_fullname_module( const UNICODE_STRING *nt_name )
{
    PLIST_ENTRY mark, entry;
    UNICODE_STRING name = *nt_name;

    if (name.Length <= 4 * sizeof(WCHAR)) return NULL;
    name.Length -= 4 * sizeof(WCHAR);  /* for \??\ prefix */
    name.Buffer += 4;

    if (cached_modref && RtlEqualUnicodeString( &name, &cached_modref->ldr.FullDllName, TRUE ))
        return cached_modref;

    mark = &NtCurrentTeb()->Peb->LdrData->InLoadOrderModuleList;
    for (entry = mark->Flink; entry != mark; entry = entry->Flink)
    {
        LDR_DATA_TABLE_ENTRY *mod = CONTAINING_RECORD(entry, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);
        if (RtlEqualUnicodeString( &name, &mod->FullDllName, TRUE ))
        {
            cached_modref = CONTAINING_RECORD(mod, WINE_MODREF, ldr);
            return cached_modref;
        }
    }
    return NULL;
}


/**********************************************************************
 *	    find_fileid_module
 *
 * Find a module from its file id.
 * The loader_section must be locked while calling this function
 */
static WINE_MODREF *find_fileid_module( struct stat *st )
{
    LIST_ENTRY *mark, *entry;

    if (cached_modref && cached_modref->dev == st->st_dev && cached_modref->ino == st->st_ino)
        return cached_modref;

    mark = &NtCurrentTeb()->Peb->LdrData->InLoadOrderModuleList;
    for (entry = mark->Flink; entry != mark; entry = entry->Flink)
    {
        LDR_DATA_TABLE_ENTRY *mod = CONTAINING_RECORD( entry, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks );
        WINE_MODREF *wm = CONTAINING_RECORD( mod, WINE_MODREF, ldr );

        if (wm->dev == st->st_dev && wm->ino == st->st_ino)
        {
            cached_modref = wm;
            return wm;
        }
    }
    return NULL;
}


/**********************************************************************
 *	    find_so_module
 *
 * Find a module from its so file handle.
 * The loader_section must be locked while calling this function
 */
static WINE_MODREF *find_so_module( void *handle )
{
    LIST_ENTRY *mark, *entry;

    mark = &NtCurrentTeb()->Peb->LdrData->InLoadOrderModuleList;
    for (entry = mark->Flink; entry != mark; entry = entry->Flink)
    {
        LDR_DATA_TABLE_ENTRY *mod = CONTAINING_RECORD( entry, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks );
        WINE_MODREF *wm = CONTAINING_RECORD( mod, WINE_MODREF, ldr );

        if (wm->so_handle == handle) return wm;
    }
    return NULL;
}


/*************************************************************************
 *		grow_module_deps
 */
static WINE_MODREF **grow_module_deps( WINE_MODREF *wm, int count )
{
    WINE_MODREF **deps;

    if (wm->alloc_deps)
        deps = RtlReAllocateHeap( GetProcessHeap(), HEAP_ZERO_MEMORY, wm->deps,
                                  (wm->alloc_deps + count) * sizeof(*deps) );
    else
        deps = RtlAllocateHeap( GetProcessHeap(), HEAP_ZERO_MEMORY, count * sizeof(*deps) );

    if (deps)
    {
        wm->deps = deps;
        wm->alloc_deps += count;
    }
    return deps;
}

/*************************************************************************
 *		find_forwarded_export
 *
 * Find the final function pointer for a forwarded function.
 * The loader_section must be locked while calling this function.
 */
static FARPROC find_forwarded_export( HMODULE module, const char *forward, LPCWSTR load_path )
{
    const IMAGE_EXPORT_DIRECTORY *exports;
    DWORD exp_size;
    WINE_MODREF *wm;
    WCHAR mod_name[32];
    const char *end = strrchr(forward, '.');
    FARPROC proc = NULL;

    if (!end) return NULL;
    if ((end - forward) * sizeof(WCHAR) >= sizeof(mod_name)) return NULL;
    ascii_to_unicode( mod_name, forward, end - forward );
    mod_name[end - forward] = 0;
    if (!wcschr( mod_name, '.' ))
    {
        if ((end - forward) * sizeof(WCHAR) >= sizeof(mod_name) - sizeof(dllW)) return NULL;
        memcpy( mod_name + (end - forward), dllW, sizeof(dllW) );
    }

    if (!(wm = find_basename_module( mod_name )))
    {
        TRACE( "delay loading %s for '%s'\n", debugstr_w(mod_name), forward );
        if (load_dll( load_path, mod_name, dllW, 0, &wm ) == STATUS_SUCCESS &&
            !(wm->ldr.Flags & LDR_DONT_RESOLVE_REFS))
        {
            if (!imports_fixup_done && current_modref)
            {
                WINE_MODREF **deps = grow_module_deps( current_modref, 1 );
                if (deps) deps[current_modref->nDeps++] = wm;
            }
            else if (process_attach( wm, NULL ) != STATUS_SUCCESS)
            {
                LdrUnloadDll( wm->ldr.DllBase );
                wm = NULL;
            }
        }

        if (!wm)
        {
            ERR( "module not found for forward '%s' used by %s\n",
                 forward, debugstr_w(get_modref(module)->ldr.FullDllName.Buffer) );
            return NULL;
        }
    }
    if ((exports = RtlImageDirectoryEntryToData( wm->ldr.DllBase, TRUE,
                                                 IMAGE_DIRECTORY_ENTRY_EXPORT, &exp_size )))
    {
        const char *name = end + 1;
        if (*name == '#')  /* ordinal */
            proc = find_ordinal_export( wm->ldr.DllBase, exports, exp_size, atoi(name+1), load_path );
        else
            proc = find_named_export( wm->ldr.DllBase, exports, exp_size, name, -1, load_path );
    }

    if (!proc)
    {
        ERR("function not found for forward '%s' used by %s."
            " If you are using builtin %s, try using the native one instead.\n",
            forward, debugstr_w(get_modref(module)->ldr.FullDllName.Buffer),
            debugstr_w(get_modref(module)->ldr.BaseDllName.Buffer) );
    }
    return proc;
}


/*************************************************************************
 *		find_ordinal_export
 *
 * Find an exported function by ordinal.
 * The exports base must have been subtracted from the ordinal already.
 * The loader_section must be locked while calling this function.
 */
static FARPROC find_ordinal_export( HMODULE module, const IMAGE_EXPORT_DIRECTORY *exports,
                                    DWORD exp_size, DWORD ordinal, LPCWSTR load_path )
{
    FARPROC proc;
    const DWORD *functions = get_rva( module, exports->AddressOfFunctions );

    if (ordinal >= exports->NumberOfFunctions)
    {
        TRACE("	ordinal %d out of range!\n", ordinal + exports->Base );
        return NULL;
    }
    if (!functions[ordinal]) return NULL;

    proc = get_rva( module, functions[ordinal] );

    /* if the address falls into the export dir, it's a forward */
    if (((const char *)proc >= (const char *)exports) && 
        ((const char *)proc < (const char *)exports + exp_size))
        return find_forwarded_export( module, (const char *)proc, load_path );

    if (TRACE_ON(snoop))
    {
        const WCHAR *user = current_modref ? current_modref->ldr.BaseDllName.Buffer : NULL;
        proc = SNOOP_GetProcAddress( module, exports, exp_size, proc, ordinal, user );
    }
    if (TRACE_ON(relay))
    {
        const WCHAR *user = current_modref ? current_modref->ldr.BaseDllName.Buffer : NULL;
        proc = RELAY_GetProcAddress( module, exports, exp_size, proc, ordinal, user );
    }
    return proc;
}


/*************************************************************************
 *		find_named_export
 *
 * Find an exported function by name.
 * The loader_section must be locked while calling this function.
 */
static FARPROC find_named_export( HMODULE module, const IMAGE_EXPORT_DIRECTORY *exports,
                                  DWORD exp_size, const char *name, int hint, LPCWSTR load_path )
{
    const WORD *ordinals = get_rva( module, exports->AddressOfNameOrdinals );
    const DWORD *names = get_rva( module, exports->AddressOfNames );
    int min = 0, max = exports->NumberOfNames - 1;

    /* first check the hint */
    if (hint >= 0 && hint <= max)
    {
        char *ename = get_rva( module, names[hint] );
        if (!strcmp( ename, name ))
            return find_ordinal_export( module, exports, exp_size, ordinals[hint], load_path );
    }

    /* then do a binary search */
    while (min <= max)
    {
        int res, pos = (min + max) / 2;
        char *ename = get_rva( module, names[pos] );
        if (!(res = strcmp( ename, name )))
            return find_ordinal_export( module, exports, exp_size, ordinals[pos], load_path );
        if (res > 0) max = pos - 1;
        else min = pos + 1;
    }
    return NULL;

}


/*************************************************************************
 *		import_dll
 *
 * Import the dll specified by the given import descriptor.
 * The loader_section must be locked while calling this function.
 */
static BOOL import_dll( HMODULE module, const IMAGE_IMPORT_DESCRIPTOR *descr, LPCWSTR load_path, WINE_MODREF **pwm )
{
    NTSTATUS status;
    WINE_MODREF *wmImp;
    HMODULE imp_mod;
    const IMAGE_EXPORT_DIRECTORY *exports;
    DWORD exp_size;
    const IMAGE_THUNK_DATA *import_list;
    IMAGE_THUNK_DATA *thunk_list;
    WCHAR buffer[32];
    const char *name = get_rva( module, descr->Name );
    DWORD len = strlen(name);
    PVOID protect_base;
    SIZE_T protect_size = 0;
    DWORD protect_old;

    thunk_list = get_rva( module, (DWORD)descr->FirstThunk );
    if (descr->u.OriginalFirstThunk)
        import_list = get_rva( module, (DWORD)descr->u.OriginalFirstThunk );
    else
        import_list = thunk_list;

    if (!import_list->u1.Ordinal)
    {
        WARN( "Skipping unused import %s\n", name );
        *pwm = NULL;
        return TRUE;
    }

    while (len && name[len-1] == ' ') len--;  /* remove trailing spaces */

    if (len * sizeof(WCHAR) < sizeof(buffer))
    {
        ascii_to_unicode( buffer, name, len );
        buffer[len] = 0;
        status = load_dll( load_path, buffer, dllW, 0, &wmImp );
    }
    else  /* need to allocate a larger buffer */
    {
        WCHAR *ptr = RtlAllocateHeap( GetProcessHeap(), 0, (len + 1) * sizeof(WCHAR) );
        if (!ptr) return FALSE;
        ascii_to_unicode( ptr, name, len );
        ptr[len] = 0;
        status = load_dll( load_path, ptr, dllW, 0, &wmImp );
        RtlFreeHeap( GetProcessHeap(), 0, ptr );
    }

    if (status)
    {
        if (status == STATUS_DLL_NOT_FOUND)
            ERR("Library %s (which is needed by %s) not found\n",
                name, debugstr_w(current_modref->ldr.FullDllName.Buffer));
        else
            ERR("Loading library %s (which is needed by %s) failed (error %x).\n",
                name, debugstr_w(current_modref->ldr.FullDllName.Buffer), status);
        return FALSE;
    }

    /* unprotect the import address table since it can be located in
     * readonly section */
    while (import_list[protect_size].u1.Ordinal) protect_size++;
    protect_base = thunk_list;
    protect_size *= sizeof(*thunk_list);
    NtProtectVirtualMemory( NtCurrentProcess(), &protect_base,
                            &protect_size, PAGE_READWRITE, &protect_old );

    imp_mod = wmImp->ldr.DllBase;
    exports = RtlImageDirectoryEntryToData( imp_mod, TRUE, IMAGE_DIRECTORY_ENTRY_EXPORT, &exp_size );

    if (!exports)
    {
        /* set all imported function to deadbeef */
        while (import_list->u1.Ordinal)
        {
            if (IMAGE_SNAP_BY_ORDINAL(import_list->u1.Ordinal))
            {
                int ordinal = IMAGE_ORDINAL(import_list->u1.Ordinal);
                WARN("No implementation for %s.%d", name, ordinal );
                thunk_list->u1.Function = allocate_stub( name, IntToPtr(ordinal) );
            }
            else
            {
                IMAGE_IMPORT_BY_NAME *pe_name = get_rva( module, (DWORD)import_list->u1.AddressOfData );
                WARN("No implementation for %s.%s", name, pe_name->Name );
                thunk_list->u1.Function = allocate_stub( name, (const char*)pe_name->Name );
            }
            WARN(" imported from %s, allocating stub %p\n",
                 debugstr_w(current_modref->ldr.FullDllName.Buffer),
                 (void *)thunk_list->u1.Function );
            import_list++;
            thunk_list++;
        }
        goto done;
    }

    while (import_list->u1.Ordinal)
    {
        if (IMAGE_SNAP_BY_ORDINAL(import_list->u1.Ordinal))
        {
            int ordinal = IMAGE_ORDINAL(import_list->u1.Ordinal);

            thunk_list->u1.Function = (ULONG_PTR)find_ordinal_export( imp_mod, exports, exp_size,
                                                                      ordinal - exports->Base, load_path );
            if (!thunk_list->u1.Function)
            {
                thunk_list->u1.Function = allocate_stub( name, IntToPtr(ordinal) );
                WARN("No implementation for %s.%d imported from %s, setting to %p\n",
                     name, ordinal, debugstr_w(current_modref->ldr.FullDllName.Buffer),
                     (void *)thunk_list->u1.Function );
            }
            TRACE_(imports)("--- Ordinal %s.%d = %p\n", name, ordinal, (void *)thunk_list->u1.Function );
        }
        else  /* import by name */
        {
            IMAGE_IMPORT_BY_NAME *pe_name;
            pe_name = get_rva( module, (DWORD)import_list->u1.AddressOfData );
            thunk_list->u1.Function = (ULONG_PTR)find_named_export( imp_mod, exports, exp_size,
                                                                    (const char*)pe_name->Name,
                                                                    pe_name->Hint, load_path );
            if (!thunk_list->u1.Function)
            {
                thunk_list->u1.Function = allocate_stub( name, (const char*)pe_name->Name );
                WARN("No implementation for %s.%s imported from %s, setting to %p\n",
                     name, pe_name->Name, debugstr_w(current_modref->ldr.FullDllName.Buffer),
                     (void *)thunk_list->u1.Function );
            }
            TRACE_(imports)("--- %s %s.%d = %p\n",
                            pe_name->Name, name, pe_name->Hint, (void *)thunk_list->u1.Function);
        }
        import_list++;
        thunk_list++;
    }

done:
    /* restore old protection of the import address table */
    NtProtectVirtualMemory( NtCurrentProcess(), &protect_base, &protect_size, protect_old, &protect_old );
    *pwm = wmImp;
    return TRUE;
}


/***********************************************************************
 *           create_module_activation_context
 */
static NTSTATUS create_module_activation_context( LDR_DATA_TABLE_ENTRY *module )
{
    NTSTATUS status;
    LDR_RESOURCE_INFO info;
    const IMAGE_RESOURCE_DATA_ENTRY *entry;

    info.Type = RT_MANIFEST;
    info.Name = ISOLATIONAWARE_MANIFEST_RESOURCE_ID;
    info.Language = 0;
    if (!(status = LdrFindResource_U( module->DllBase, &info, 3, &entry )))
    {
        ACTCTXW ctx;
        ctx.cbSize   = sizeof(ctx);
        ctx.lpSource = NULL;
        ctx.dwFlags  = ACTCTX_FLAG_RESOURCE_NAME_VALID | ACTCTX_FLAG_HMODULE_VALID;
        ctx.hModule  = module->DllBase;
        ctx.lpResourceName = (LPCWSTR)ISOLATIONAWARE_MANIFEST_RESOURCE_ID;
        status = RtlCreateActivationContext( &module->ActivationContext, &ctx );
    }
    return status;
}


/*************************************************************************
 *		is_dll_native_subsystem
 *
 * Check if dll is a proper native driver.
 * Some dlls (corpol.dll from IE6 for instance) are incorrectly marked as native
 * while being perfectly normal DLLs.  This heuristic should catch such breakages.
 */
static BOOL is_dll_native_subsystem( LDR_DATA_TABLE_ENTRY *mod, const IMAGE_NT_HEADERS *nt, LPCWSTR filename )
{
    static const WCHAR ntdllW[]    = {'n','t','d','l','l','.','d','l','l',0};
    static const WCHAR kernel32W[] = {'k','e','r','n','e','l','3','2','.','d','l','l',0};
    const IMAGE_IMPORT_DESCRIPTOR *imports;
    DWORD i, size;
    WCHAR buffer[16];

    if (nt->OptionalHeader.Subsystem != IMAGE_SUBSYSTEM_NATIVE) return FALSE;
    if (nt->OptionalHeader.SectionAlignment < page_size) return TRUE;
    if (mod->Flags & LDR_WINE_INTERNAL) return TRUE;

    if ((imports = RtlImageDirectoryEntryToData( mod->DllBase, TRUE,
                                                 IMAGE_DIRECTORY_ENTRY_IMPORT, &size )))
    {
        for (i = 0; imports[i].Name; i++)
        {
            const char *name = get_rva( mod->DllBase, imports[i].Name );
            DWORD len = strlen(name);
            if (len * sizeof(WCHAR) >= sizeof(buffer)) continue;
            ascii_to_unicode( buffer, name, len + 1 );
            if (!wcsicmp( buffer, ntdllW ) || !wcsicmp( buffer, kernel32W ))
            {
                TRACE( "%s imports %s, assuming not native\n", debugstr_w(filename), debugstr_w(buffer) );
                return FALSE;
            }
        }
    }
    return TRUE;
}

/*************************************************************************
 *		alloc_tls_slot
 *
 * Allocate a TLS slot for a newly-loaded module.
 * The loader_section must be locked while calling this function.
 */
static SHORT alloc_tls_slot( LDR_DATA_TABLE_ENTRY *mod )
{
    const IMAGE_TLS_DIRECTORY *dir;
    ULONG i, size;
    void *new_ptr;
    LIST_ENTRY *entry;

    if (!(dir = RtlImageDirectoryEntryToData( mod->DllBase, TRUE, IMAGE_DIRECTORY_ENTRY_TLS, &size )))
        return -1;

    size = dir->EndAddressOfRawData - dir->StartAddressOfRawData;
    if (!size && !dir->SizeOfZeroFill && !dir->AddressOfCallBacks) return -1;

    for (i = 0; i < tls_module_count; i++)
    {
        if (!tls_dirs[i].StartAddressOfRawData && !tls_dirs[i].EndAddressOfRawData &&
            !tls_dirs[i].SizeOfZeroFill && !tls_dirs[i].AddressOfCallBacks)
            break;
    }

    TRACE( "module %p data %p-%p zerofill %u index %p callback %p flags %x -> slot %u\n", mod->DllBase,
           (void *)dir->StartAddressOfRawData, (void *)dir->EndAddressOfRawData, dir->SizeOfZeroFill,
           (void *)dir->AddressOfIndex, (void *)dir->AddressOfCallBacks, dir->Characteristics, i );

    if (i == tls_module_count)
    {
        UINT new_count = max( 32, tls_module_count * 2 );

        if (!tls_dirs)
            new_ptr = RtlAllocateHeap( GetProcessHeap(), HEAP_ZERO_MEMORY, new_count * sizeof(*tls_dirs) );
        else
            new_ptr = RtlReAllocateHeap( GetProcessHeap(), HEAP_ZERO_MEMORY, tls_dirs,
                                         new_count * sizeof(*tls_dirs) );
        if (!new_ptr) return -1;

        /* resize the pointer block in all running threads */
        for (entry = tls_links.Flink; entry != &tls_links; entry = entry->Flink)
        {
            TEB *teb = CONTAINING_RECORD( entry, TEB, TlsLinks );
            void **old = teb->ThreadLocalStoragePointer;
            void **new = RtlAllocateHeap( GetProcessHeap(), HEAP_ZERO_MEMORY, new_count * sizeof(*new));

            if (!new) return -1;
            if (old) memcpy( new, old, tls_module_count * sizeof(*new) );
            teb->ThreadLocalStoragePointer = new;
#if defined(__APPLE__) && defined(__x86_64__)
            if (teb->Reserved5[0])
                ((TEB*)teb->Reserved5[0])->ThreadLocalStoragePointer = new;
#endif
            TRACE( "thread %04lx tls block %p -> %p\n", (ULONG_PTR)teb->ClientId.UniqueThread, old, new );
            /* FIXME: can't free old block here, should be freed at thread exit */
        }

        tls_dirs = new_ptr;
        tls_module_count = new_count;
    }

    /* allocate the data block in all running threads */
    for (entry = tls_links.Flink; entry != &tls_links; entry = entry->Flink)
    {
        TEB *teb = CONTAINING_RECORD( entry, TEB, TlsLinks );

        if (!(new_ptr = RtlAllocateHeap( GetProcessHeap(), 0, size + dir->SizeOfZeroFill ))) return -1;
        memcpy( new_ptr, (void *)dir->StartAddressOfRawData, size );
        memset( (char *)new_ptr + size, 0, dir->SizeOfZeroFill );

        TRACE( "thread %04lx slot %u: %u/%u bytes at %p\n",
               (ULONG_PTR)teb->ClientId.UniqueThread, i, size, dir->SizeOfZeroFill, new_ptr );

        RtlFreeHeap( GetProcessHeap(), 0,
                     InterlockedExchangePointer( (void **)teb->ThreadLocalStoragePointer + i, new_ptr ));
    }

    *(DWORD *)dir->AddressOfIndex = i;
    tls_dirs[i] = *dir;
    return i;
}


/*************************************************************************
 *		free_tls_slot
 *
 * Free the module TLS slot on unload.
 * The loader_section must be locked while calling this function.
 */
static void free_tls_slot( LDR_DATA_TABLE_ENTRY *mod )
{
    ULONG i = (USHORT)mod->TlsIndex;

    if (mod->TlsIndex == -1) return;
    assert( i < tls_module_count );
    memset( &tls_dirs[i], 0, sizeof(tls_dirs[i]) );
}


/****************************************************************
 *       fixup_imports_ilonly
 *
 * Fixup imports for an IL-only module. All we do is import mscoree.
 * The loader_section must be locked while calling this function.
 */
static NTSTATUS fixup_imports_ilonly( WINE_MODREF *wm, LPCWSTR load_path, void **entry )
{
    static const WCHAR mscoreeW[] = {'m','s','c','o','r','e','e','.','d','l','l',0};
    IMAGE_EXPORT_DIRECTORY *exports;
    DWORD exp_size;
    NTSTATUS status;
    void *proc = NULL;
    WINE_MODREF *prev, *imp;

    if (!(wm->ldr.Flags & LDR_DONT_RESOLVE_REFS)) return STATUS_SUCCESS;  /* already done */
    wm->ldr.Flags &= ~LDR_DONT_RESOLVE_REFS;

    if (!grow_module_deps( wm, 1 )) return STATUS_NO_MEMORY;
    wm->nDeps = 1;

    prev = current_modref;
    current_modref = wm;
    if (!(status = load_dll( load_path, mscoreeW, NULL, 0, &imp ))) wm->deps[0] = imp;
    current_modref = prev;
    if (status)
    {
        ERR( "mscoree.dll not found, IL-only binary %s cannot be loaded\n",
             debugstr_w(wm->ldr.BaseDllName.Buffer) );
        return status;
    }

    TRACE( "loaded mscoree for %s\n", debugstr_w(wm->ldr.FullDllName.Buffer) );

    if ((exports = RtlImageDirectoryEntryToData( imp->ldr.DllBase, TRUE,
                                                 IMAGE_DIRECTORY_ENTRY_EXPORT, &exp_size )))
    {
        const char *name = (wm->ldr.Flags & LDR_IMAGE_IS_DLL) ? "_CorDllMain" : "_CorExeMain";
        proc = find_named_export( imp->ldr.DllBase, exports, exp_size, name, -1, load_path );
    }
    if (!proc) return STATUS_PROCEDURE_NOT_FOUND;
    *entry = proc;
    return STATUS_SUCCESS;
}


/****************************************************************
 *       fixup_imports
 *
 * Fixup all imports of a given module.
 * The loader_section must be locked while calling this function.
 */
static NTSTATUS fixup_imports( WINE_MODREF *wm, LPCWSTR load_path )
{
    int i, dep, nb_imports;
    const IMAGE_IMPORT_DESCRIPTOR *imports;
    WINE_MODREF *prev, *imp;
    DWORD size;
    NTSTATUS status;
    ULONG_PTR cookie;

    if (!(wm->ldr.Flags & LDR_DONT_RESOLVE_REFS)) return STATUS_SUCCESS;  /* already done */
    wm->ldr.Flags &= ~LDR_DONT_RESOLVE_REFS;

    wm->ldr.TlsIndex = alloc_tls_slot( &wm->ldr );

    if (!(imports = RtlImageDirectoryEntryToData( wm->ldr.DllBase, TRUE,
                                                  IMAGE_DIRECTORY_ENTRY_IMPORT, &size )))
        return STATUS_SUCCESS;

    nb_imports = 0;
    while (imports[nb_imports].Name && imports[nb_imports].FirstThunk) nb_imports++;

    if (!nb_imports) return STATUS_SUCCESS;  /* no imports */
    if (!grow_module_deps( wm, nb_imports )) return STATUS_NO_MEMORY;

    if (!create_module_activation_context( &wm->ldr ))
        RtlActivateActivationContext( 0, wm->ldr.ActivationContext, &cookie );

    /* load the imported modules. They are automatically
     * added to the modref list of the process.
     */
    prev = current_modref;
    current_modref = wm;
    status = STATUS_SUCCESS;
    for (i = 0; i < nb_imports; i++)
    {
        dep = wm->nDeps++;

        if (!import_dll( wm->ldr.DllBase, &imports[i], load_path, &imp ))
        {
            imp = NULL;
            status = STATUS_DLL_NOT_FOUND;
        }
        wm->deps[dep] = imp;
    }
    current_modref = prev;
    if (wm->ldr.ActivationContext) RtlDeactivateActivationContext( 0, cookie );
    return status;
}


/*************************************************************************
 *		alloc_module
 *
 * Allocate a WINE_MODREF structure and add it to the process list
 * The loader_section must be locked while calling this function.
 */
static WINE_MODREF *alloc_module( HMODULE hModule, const UNICODE_STRING *nt_name, BOOL builtin )
{
    WCHAR *buffer;
    WINE_MODREF *wm;
    const WCHAR *p;
    const IMAGE_NT_HEADERS *nt = RtlImageNtHeader(hModule);

    if (!(wm = RtlAllocateHeap( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*wm) ))) return NULL;

    wm->ldr.DllBase       = hModule;
    wm->ldr.SizeOfImage   = nt->OptionalHeader.SizeOfImage;
    wm->ldr.Flags         = LDR_DONT_RESOLVE_REFS | (builtin ? LDR_WINE_INTERNAL : 0);
    wm->ldr.TlsIndex      = -1;
    wm->ldr.LoadCount     = 1;

    if (!(buffer = RtlAllocateHeap( GetProcessHeap(), 0, nt_name->Length - 3 * sizeof(WCHAR) )))
    {
        RtlFreeHeap( GetProcessHeap(), 0, wm );
        return NULL;
    }
    memcpy( buffer, nt_name->Buffer + 4 /* \??\ prefix */, nt_name->Length - 4 * sizeof(WCHAR) );
    buffer[nt_name->Length/sizeof(WCHAR) - 4] = 0;
    if ((p = wcsrchr( buffer, '\\' ))) p++;
    else p = buffer;
    RtlInitUnicodeString( &wm->ldr.FullDllName, buffer );
    RtlInitUnicodeString( &wm->ldr.BaseDllName, p );

    if (!is_dll_native_subsystem( &wm->ldr, nt, p ))
    {
        if (nt->FileHeader.Characteristics & IMAGE_FILE_DLL)
            wm->ldr.Flags |= LDR_IMAGE_IS_DLL;
        if (nt->OptionalHeader.AddressOfEntryPoint)
            wm->ldr.EntryPoint = (char *)hModule + nt->OptionalHeader.AddressOfEntryPoint;
    }

    InsertTailList(&NtCurrentTeb()->Peb->LdrData->InLoadOrderModuleList,
                   &wm->ldr.InLoadOrderLinks);
    InsertTailList(&NtCurrentTeb()->Peb->LdrData->InMemoryOrderModuleList,
                   &wm->ldr.InMemoryOrderLinks);
    InsertTailList(&hash_table[hash_basename(wm->ldr.BaseDllName.Buffer)],
                   &wm->ldr.HashLinks);

    /* wait until init is called for inserting into InInitializationOrderModuleList */
    wm->ldr.InInitializationOrderLinks.Flink = NULL;
    wm->ldr.InInitializationOrderLinks.Blink = NULL;

    if (!(nt->OptionalHeader.DllCharacteristics & IMAGE_DLLCHARACTERISTICS_NX_COMPAT))
    {
        ULONG flags = MEM_EXECUTE_OPTION_ENABLE;
        WARN( "disabling no-exec because of %s\n", debugstr_w(wm->ldr.BaseDllName.Buffer) );
        NtSetInformationProcess( GetCurrentProcess(), ProcessExecuteFlags, &flags, sizeof(flags) );
    }
    return wm;
}


/*************************************************************************
 *              alloc_thread_tls
 *
 * Allocate the per-thread structure for module TLS storage.
 */
static NTSTATUS alloc_thread_tls(void)
{
    void **pointers;
    UINT i, size;

    if (!tls_module_count) return STATUS_SUCCESS;

    if (!(pointers = RtlAllocateHeap( GetProcessHeap(), HEAP_ZERO_MEMORY,
                                      tls_module_count * sizeof(*pointers) )))
        return STATUS_NO_MEMORY;

    for (i = 0; i < tls_module_count; i++)
    {
        const IMAGE_TLS_DIRECTORY *dir = &tls_dirs[i];

        if (!dir) continue;
        size = dir->EndAddressOfRawData - dir->StartAddressOfRawData;
        if (!size && !dir->SizeOfZeroFill) continue;

        if (!(pointers[i] = RtlAllocateHeap( GetProcessHeap(), 0, size + dir->SizeOfZeroFill )))
        {
            while (i) RtlFreeHeap( GetProcessHeap(), 0, pointers[--i] );
            RtlFreeHeap( GetProcessHeap(), 0, pointers );
            return STATUS_NO_MEMORY;
        }
        memcpy( pointers[i], (void *)dir->StartAddressOfRawData, size );
        memset( (char *)pointers[i] + size, 0, dir->SizeOfZeroFill );

        TRACE( "thread %04x slot %u: %u/%u bytes at %p\n",
               GetCurrentThreadId(), i, size, dir->SizeOfZeroFill, pointers[i] );
    }
    NtCurrentTeb()->ThreadLocalStoragePointer = pointers;
#if defined(__APPLE__) && defined(__x86_64__)
    __asm__ volatile (".byte 0x65\n\tmovq %0,%c1"
                      :
                      : "r" (pointers), "n" (FIELD_OFFSET(TEB, ThreadLocalStoragePointer)));
#endif
    return STATUS_SUCCESS;
}


/*************************************************************************
 *              call_tls_callbacks
 */
static void call_tls_callbacks( HMODULE module, UINT reason )
{
    const IMAGE_TLS_DIRECTORY *dir;
    const PIMAGE_TLS_CALLBACK *callback;
    ULONG dirsize;

    if (reason == DLL_WINE_PREATTACH) return;

    dir = RtlImageDirectoryEntryToData( module, TRUE, IMAGE_DIRECTORY_ENTRY_TLS, &dirsize );
    if (!dir || !dir->AddressOfCallBacks) return;

    for (callback = (const PIMAGE_TLS_CALLBACK *)dir->AddressOfCallBacks; *callback; callback++)
    {
        TRACE_(relay)("\1Call TLS callback (proc=%p,module=%p,reason=%s,reserved=0)\n",
                      *callback, module, reason_names[reason] );
        __TRY
        {
            call_dll_entry_point( (DLLENTRYPROC)*callback, module, reason, NULL );
        }
        __EXCEPT_ALL
        {
            TRACE_(relay)("\1exception %08x in TLS callback (proc=%p,module=%p,reason=%s,reserved=0)\n",
                          GetExceptionCode(), callback, module, reason_names[reason] );
            return;
        }
        __ENDTRY
        TRACE_(relay)("\1Ret  TLS callback (proc=%p,module=%p,reason=%s,reserved=0)\n",
                      *callback, module, reason_names[reason] );
    }
}

#ifdef __FreeBSD__
/* The PT_LOAD segments are sorted in increasing order, and the first
 * starts at the beginning of the ELF file. By parsing the file, we can
 * find that first PT_LOAD segment, from which we can find the base
 * address it wanted, and knowing mapbase where the binary was actually
 * loaded, use them to work out the relocbase offset. */
static BOOL get_relocbase(caddr_t mapbase, caddr_t *relocbase)
{
    Elf_Half i;
#ifdef _WIN64
    const Elf64_Ehdr *elf_header = (Elf64_Ehdr*) mapbase;
#else
    const Elf32_Ehdr *elf_header = (Elf32_Ehdr*) mapbase;
#endif
    const Elf_Phdr *prog_header = (const Elf_Phdr *)(mapbase + elf_header->e_phoff);

    for (i = 0; i < elf_header->e_phnum; i++)
    {
         if (prog_header->p_type == PT_LOAD)
         {
             caddr_t desired_base = (caddr_t)((prog_header->p_vaddr / prog_header->p_align) * prog_header->p_align);
             *relocbase = (caddr_t) (mapbase - desired_base);
             return TRUE;
         }
         prog_header++;
    }
    return FALSE;
}
#endif

/*************************************************************************
 *              call_constructors
 */
static void call_constructors( WINE_MODREF *wm )
{
#ifdef HAVE_DLINFO
    struct link_map *map;
    void (*init_func)(int, char **, char **) = NULL;
    void (**init_array)(int, char **, char **) = NULL;
    ULONG_PTR i, init_arraysz = 0;
    int argc;
    char **argv, **envp;
#ifdef _WIN64
    const Elf64_Dyn *dyn;
#else
    const Elf32_Dyn *dyn;
#endif

    if (dlinfo( wm->so_handle, RTLD_DI_LINKMAP, &map ) == -1) return;
    for (dyn = map->l_ld; dyn->d_tag; dyn++)
    {
        caddr_t relocbase = (caddr_t)map->l_addr;

#ifdef __FreeBSD__
        /* On older FreeBSD versions, l_addr was the absolute load address, now it's the relocation offset. */
        if (!dlsym(RTLD_DEFAULT, "_rtld_version_laddr_offset"))
            if (!get_relocbase(map->l_addr, &relocbase)) return;
#endif
        switch (dyn->d_tag)
        {
        case 0x60009990: init_array = (void *)(relocbase + dyn->d_un.d_val); break;
        case 0x60009991: init_arraysz = dyn->d_un.d_val; break;
        case 0x60009992: init_func = (void *)(relocbase + dyn->d_un.d_val); break;
        }
    }

    TRACE( "%s: got init_func %p init_array %p %lu\n", debugstr_us( &wm->ldr.BaseDllName ),
           init_func, init_array, init_arraysz );

    unix_funcs->get_main_args( &argc, &argv, &envp );

    if (init_func) init_func( argc, argv, envp );

    if (init_array)
        for (i = 0; i < init_arraysz / sizeof(*init_array); i++) init_array[i]( argc, argv, envp );
#endif
}


/*************************************************************************
 *              MODULE_InitDLL
 */
static NTSTATUS MODULE_InitDLL( WINE_MODREF *wm, UINT reason, LPVOID lpReserved )
{
    WCHAR mod_name[32];
    NTSTATUS status = STATUS_SUCCESS;
    DLLENTRYPROC entry = wm->ldr.EntryPoint;
    void *module = wm->ldr.DllBase;
    BOOL retv = FALSE;

    /* Skip calls for modules loaded with special load flags */

    if (wm->ldr.Flags & LDR_DONT_RESOLVE_REFS) return STATUS_SUCCESS;
    if (wm->ldr.TlsIndex != -1) call_tls_callbacks( wm->ldr.DllBase, reason );
    if (wm->so_handle && reason == DLL_PROCESS_ATTACH) call_constructors( wm );
    if (!entry) return STATUS_SUCCESS;

    memset( mod_name, 0, sizeof(mod_name) );

    if (TRACE_ON(relay))
    {
        size_t len = min( wm->ldr.BaseDllName.Length, sizeof(mod_name)-sizeof(WCHAR) );
        memcpy( mod_name, wm->ldr.BaseDllName.Buffer, len );
        mod_name[len / sizeof(WCHAR)] = 0;
        TRACE_(relay)("\1Call PE DLL (proc=%p,module=%p %s,reason=%s,res=%p)\n",
                      entry, module, debugstr_w(mod_name), reason_names[reason], lpReserved );
    }
    else TRACE("(%p %s,%s,%p) - CALL\n", module, debugstr_w(wm->ldr.BaseDllName.Buffer),
               reason_names[reason], lpReserved );

    __TRY
    {
        retv = call_dll_entry_point( entry, module, reason, lpReserved );
        if (!retv)
            status = STATUS_DLL_INIT_FAILED;
    }
    __EXCEPT_ALL
    {
        status = GetExceptionCode();
        TRACE_(relay)("\1exception %08x in PE entry point (proc=%p,module=%p,reason=%s,res=%p)\n",
                      status, entry, module, reason_names[reason], lpReserved );
    }
    __ENDTRY

    /* The state of the module list may have changed due to the call
       to the dll. We cannot assume that this module has not been
       deleted.  */
    if (TRACE_ON(relay))
        TRACE_(relay)("\1Ret  PE DLL (proc=%p,module=%p %s,reason=%s,res=%p) retval=%x\n",
                      entry, module, debugstr_w(mod_name), reason_names[reason], lpReserved, retv );
    else
        TRACE("(%p,%s,%p) - RETURN %d\n", module, reason_names[reason], lpReserved, retv );

    return status;
}


/*************************************************************************
 *		process_attach
 *
 * Send the process attach notification to all DLLs the given module
 * depends on (recursively). This is somewhat complicated due to the fact that
 *
 * - we have to respect the module dependencies, i.e. modules implicitly
 *   referenced by another module have to be initialized before the module
 *   itself can be initialized
 *
 * - the initialization routine of a DLL can itself call LoadLibrary,
 *   thereby introducing a whole new set of dependencies (even involving
 *   the 'old' modules) at any time during the whole process
 *
 * (Note that this routine can be recursively entered not only directly
 *  from itself, but also via LoadLibrary from one of the called initialization
 *  routines.)
 *
 * Furthermore, we need to rearrange the main WINE_MODREF list to allow
 * the process *detach* notifications to be sent in the correct order.
 * This must not only take into account module dependencies, but also
 * 'hidden' dependencies created by modules calling LoadLibrary in their
 * attach notification routine.
 *
 * The strategy is rather simple: we move a WINE_MODREF to the head of the
 * list after the attach notification has returned.  This implies that the
 * detach notifications are called in the reverse of the sequence the attach
 * notifications *returned*.
 *
 * The loader_section must be locked while calling this function.
 */
static NTSTATUS process_attach( WINE_MODREF *wm, LPVOID lpReserved )
{
    NTSTATUS status = STATUS_SUCCESS;
    ULONG_PTR cookie;
    int i;

    if (process_detaching) return status;

    /* prevent infinite recursion in case of cyclical dependencies */
    if (    ( wm->ldr.Flags & LDR_LOAD_IN_PROGRESS )
         || ( wm->ldr.Flags & LDR_PROCESS_ATTACHED ) )
        return status;

    TRACE("(%s,%p) - START\n", debugstr_w(wm->ldr.BaseDllName.Buffer), lpReserved );

    /* Tag current MODREF to prevent recursive loop */
    wm->ldr.Flags |= LDR_LOAD_IN_PROGRESS;
    if (lpReserved) wm->ldr.LoadCount = -1;  /* pin it if imported by the main exe */
    if (wm->ldr.ActivationContext) RtlActivateActivationContext( 0, wm->ldr.ActivationContext, &cookie );

    /* Recursively attach all DLLs this one depends on */
    for ( i = 0; i < wm->nDeps; i++ )
    {
        if (!wm->deps[i]) continue;
        if ((status = process_attach( wm->deps[i], lpReserved )) != STATUS_SUCCESS) break;
    }

    if (!wm->ldr.InInitializationOrderLinks.Flink)
        InsertTailList(&NtCurrentTeb()->Peb->LdrData->InInitializationOrderModuleList,
                &wm->ldr.InInitializationOrderLinks);

    /* Call DLL entry point */
    if (status == STATUS_SUCCESS)
    {
        WINE_MODREF *prev = current_modref;
        current_modref = wm;

        call_ldr_notifications( LDR_DLL_NOTIFICATION_REASON_LOADED, &wm->ldr );
        status = MODULE_InitDLL( wm, DLL_PROCESS_ATTACH, lpReserved );
        if (status == STATUS_SUCCESS)
        {
            wm->ldr.Flags |= LDR_PROCESS_ATTACHED;
        }
        else
        {
            MODULE_InitDLL( wm, DLL_PROCESS_DETACH, lpReserved );
            call_ldr_notifications( LDR_DLL_NOTIFICATION_REASON_UNLOADED, &wm->ldr );

            /* point to the name so LdrInitializeThunk can print it */
            last_failed_modref = wm;
            WARN("Initialization of %s failed\n", debugstr_w(wm->ldr.BaseDllName.Buffer));
        }
        current_modref = prev;
    }

    if (wm->ldr.ActivationContext) RtlDeactivateActivationContext( 0, cookie );
    /* Remove recursion flag */
    wm->ldr.Flags &= ~LDR_LOAD_IN_PROGRESS;

    TRACE("(%s,%p) - END\n", debugstr_w(wm->ldr.BaseDllName.Buffer), lpReserved );
    return status;
}


/**********************************************************************
 *	    attach_implicitly_loaded_dlls
 *
 * Attach to the (builtin) dlls that have been implicitly loaded because
 * of a dependency at the Unix level, but not imported at the Win32 level.
 */
static void attach_implicitly_loaded_dlls( LPVOID reserved )
{
    for (;;)
    {
        PLIST_ENTRY mark, entry;

        mark = &NtCurrentTeb()->Peb->LdrData->InLoadOrderModuleList;
        for (entry = mark->Flink; entry != mark; entry = entry->Flink)
        {
            LDR_DATA_TABLE_ENTRY *mod = CONTAINING_RECORD(entry, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);

            if (!(mod->Flags & LDR_IMAGE_IS_DLL)) continue;
            if (mod->Flags & (LDR_LOAD_IN_PROGRESS | LDR_PROCESS_ATTACHED)) continue;
            TRACE( "found implicitly loaded %s, attaching to it\n",
                   debugstr_w(mod->BaseDllName.Buffer));
            process_attach( CONTAINING_RECORD(mod, WINE_MODREF, ldr), reserved );
            break;  /* restart the search from the start */
        }
        if (entry == mark) break;  /* nothing found */
    }
}

static void call_fls_callbacks(void)
{
    PFLS_CALLBACK_FUNCTION *fls_callbacks;
    PRTL_BITMAP fls_bitmap;
    void **fls_slot_data;
    DWORD fls_index;

    if ((fls_callbacks = (PFLS_CALLBACK_FUNCTION *)NtCurrentTeb()->Peb->FlsCallback) && NtCurrentTeb()->FlsSlots)
    {
        fls_slot_data = (void **)((BYTE *)NtCurrentTeb()->FlsSlots + sizeof(LIST_ENTRY));
        fls_bitmap = NtCurrentTeb()->Peb->FlsBitmap;
        fls_index = 0;
        for (fls_index = 0; fls_index < fls_bitmap->SizeOfBitMap; ++fls_index)
        {
            if (!RtlAreBitsSet( fls_bitmap, fls_index, 1 ))
                continue;

            if (fls_callbacks[fls_index + 2] && fls_slot_data[fls_index])
                fls_callbacks[fls_index + 2](fls_slot_data[fls_index]);

            fls_slot_data[fls_index] = NULL;
        }
    }
}

/*************************************************************************
 *		process_detach
 *
 * Send DLL process detach notifications.  See the comment about calling
 * sequence at process_attach.
 */
static void process_detach(void)
{
    PLIST_ENTRY mark, entry;
    PLDR_DATA_TABLE_ENTRY mod;

    mark = &NtCurrentTeb()->Peb->LdrData->InInitializationOrderModuleList;
    do
    {
        for (entry = mark->Blink; entry != mark; entry = entry->Blink)
        {
            mod = CONTAINING_RECORD(entry, LDR_DATA_TABLE_ENTRY,
                                    InInitializationOrderLinks);
            /* Check whether to detach this DLL */
            if ( !(mod->Flags & LDR_PROCESS_ATTACHED) )
                continue;
            if ( mod->LoadCount && !process_detaching )
                continue;

            /* Call detach notification */
            mod->Flags &= ~LDR_PROCESS_ATTACHED;
            MODULE_InitDLL( CONTAINING_RECORD(mod, WINE_MODREF, ldr), 
                            DLL_PROCESS_DETACH, ULongToPtr(process_detaching) );
            call_ldr_notifications( LDR_DLL_NOTIFICATION_REASON_UNLOADED, mod );

            /* Restart at head of WINE_MODREF list, as entries might have
               been added and/or removed while performing the call ... */
            break;
        }
    } while (entry != mark);
}

/*************************************************************************
 *		thread_attach
 *
 * Send DLL thread attach notifications. These are sent in the
 * reverse sequence of process detach notification.
 * The loader_section must be locked while calling this function.
 */
static void thread_attach(void)
{
    PLIST_ENTRY mark, entry;
    PLDR_DATA_TABLE_ENTRY mod;

    mark = &NtCurrentTeb()->Peb->LdrData->InInitializationOrderModuleList;
    for (entry = mark->Flink; entry != mark; entry = entry->Flink)
    {
        mod = CONTAINING_RECORD(entry, LDR_DATA_TABLE_ENTRY,
                                InInitializationOrderLinks);
        if ( !(mod->Flags & LDR_PROCESS_ATTACHED) )
            continue;
        if ( mod->Flags & LDR_NO_DLL_CALLS )
            continue;

        MODULE_InitDLL( CONTAINING_RECORD(mod, WINE_MODREF, ldr), DLL_THREAD_ATTACH, NULL );
    }
}

/******************************************************************
 *		LdrDisableThreadCalloutsForDll (NTDLL.@)
 *
 */
NTSTATUS WINAPI LdrDisableThreadCalloutsForDll(HMODULE hModule)
{
    WINE_MODREF *wm;
    NTSTATUS    ret = STATUS_SUCCESS;

    RtlEnterCriticalSection( &loader_section );

    wm = get_modref( hModule );
    if (!wm || wm->ldr.TlsIndex != -1)
        ret = STATUS_DLL_NOT_FOUND;
    else
        wm->ldr.Flags |= LDR_NO_DLL_CALLS;

    RtlLeaveCriticalSection( &loader_section );

    return ret;
}

/******************************************************************
 *              LdrFindEntryForAddress (NTDLL.@)
 *
 * The loader_section must be locked while calling this function
 */
NTSTATUS WINAPI LdrFindEntryForAddress( const void *addr, PLDR_DATA_TABLE_ENTRY *pmod )
{
    PLIST_ENTRY mark, entry;
    PLDR_DATA_TABLE_ENTRY mod;

    mark = &NtCurrentTeb()->Peb->LdrData->InMemoryOrderModuleList;
    for (entry = mark->Flink; entry != mark; entry = entry->Flink)
    {
        mod = CONTAINING_RECORD(entry, LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks);
        if (mod->DllBase <= addr &&
            (const char *)addr < (char*)mod->DllBase + mod->SizeOfImage)
        {
            *pmod = mod;
            return STATUS_SUCCESS;
        }
    }
    return STATUS_NO_MORE_ENTRIES;
}

/******************************************************************
 *              LdrEnumerateLoadedModules (NTDLL.@)
 */
NTSTATUS WINAPI LdrEnumerateLoadedModules( void *unknown, LDRENUMPROC callback, void *context )
{
    LIST_ENTRY *mark, *entry;
    LDR_DATA_TABLE_ENTRY *mod;
    BOOLEAN stop = FALSE;

    TRACE( "(%p, %p, %p)\n", unknown, callback, context );

    if (unknown || !callback)
        return STATUS_INVALID_PARAMETER;

    RtlEnterCriticalSection( &loader_section );

    mark = &NtCurrentTeb()->Peb->LdrData->InMemoryOrderModuleList;
    for (entry = mark->Flink; entry != mark; entry = entry->Flink)
    {
        mod = CONTAINING_RECORD( entry, LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks );
        callback( mod, context, &stop );
        if (stop) break;
    }

    RtlLeaveCriticalSection( &loader_section );
    return STATUS_SUCCESS;
}

/******************************************************************
 *              LdrRegisterDllNotification (NTDLL.@)
 */
NTSTATUS WINAPI LdrRegisterDllNotification(ULONG flags, PLDR_DLL_NOTIFICATION_FUNCTION callback,
                                           void *context, void **cookie)
{
    struct ldr_notification *notify;

    TRACE( "(%x, %p, %p, %p)\n", flags, callback, context, cookie );

    if (!callback || !cookie)
        return STATUS_INVALID_PARAMETER;

    if (flags)
        FIXME( "ignoring flags %x\n", flags );

    notify = RtlAllocateHeap( GetProcessHeap(), 0, sizeof(*notify) );
    if (!notify) return STATUS_NO_MEMORY;
    notify->callback = callback;
    notify->context = context;

    RtlEnterCriticalSection( &loader_section );
    list_add_tail( &ldr_notifications, &notify->entry );
    RtlLeaveCriticalSection( &loader_section );

    *cookie = notify;
    return STATUS_SUCCESS;
}

/******************************************************************
 *              LdrUnregisterDllNotification (NTDLL.@)
 */
NTSTATUS WINAPI LdrUnregisterDllNotification( void *cookie )
{
    struct ldr_notification *notify = cookie;

    TRACE( "(%p)\n", cookie );

    if (!notify) return STATUS_INVALID_PARAMETER;

    RtlEnterCriticalSection( &loader_section );
    list_remove( &notify->entry );
    RtlLeaveCriticalSection( &loader_section );

    RtlFreeHeap( GetProcessHeap(), 0, notify );
    return STATUS_SUCCESS;
}

/******************************************************************
 *		LdrLockLoaderLock  (NTDLL.@)
 *
 * Note: some flags are not implemented.
 * Flag 0x01 is used to raise exceptions on errors.
 */
NTSTATUS WINAPI LdrLockLoaderLock( ULONG flags, ULONG *result, ULONG_PTR *magic )
{
    if (flags & ~0x2) FIXME( "flags %x not supported\n", flags );

    if (result) *result = 0;
    if (magic) *magic = 0;
    if (flags & ~0x3) return STATUS_INVALID_PARAMETER_1;
    if (!result && (flags & 0x2)) return STATUS_INVALID_PARAMETER_2;
    if (!magic) return STATUS_INVALID_PARAMETER_3;

    if (flags & 0x2)
    {
        if (!RtlTryEnterCriticalSection( &loader_section ))
        {
            *result = 2;
            return STATUS_SUCCESS;
        }
        *result = 1;
    }
    else
    {
        RtlEnterCriticalSection( &loader_section );
        if (result) *result = 1;
    }
    *magic = GetCurrentThreadId();
    return STATUS_SUCCESS;
}


/******************************************************************
 *		LdrUnlockLoaderUnlock  (NTDLL.@)
 */
NTSTATUS WINAPI LdrUnlockLoaderLock( ULONG flags, ULONG_PTR magic )
{
    if (magic)
    {
        if (magic != GetCurrentThreadId()) return STATUS_INVALID_PARAMETER_2;
        RtlLeaveCriticalSection( &loader_section );
    }
    return STATUS_SUCCESS;
}


/***********************************************************************
 *           hidden_exports_init
 *
 * Initializes the hide_wine_exports options.
 */
static void hidden_exports_init( const WCHAR *appname )
{
    static const WCHAR configW[] = {'S','o','f','t','w','a','r','e','\\','W','i','n','e',0};
    static const WCHAR appdefaultsW[] = {'A','p','p','D','e','f','a','u','l','t','s','\\',0};
    static const WCHAR hideWineExports[] = {'H','i','d','e','W','i','n','e','E','x','p','o','r','t','s',0};
    OBJECT_ATTRIBUTES attr;
    UNICODE_STRING nameW;
    HANDLE root, config_key, hkey;
    BOOL got_hide_wine_exports = FALSE;
    char tmp[80];
    DWORD dummy;

    RtlOpenCurrentUser( KEY_ALL_ACCESS, &root );
    attr.Length = sizeof(attr);
    attr.RootDirectory = root;
    attr.ObjectName = &nameW;
    attr.Attributes = OBJ_CASE_INSENSITIVE;
    attr.SecurityDescriptor = NULL;
    attr.SecurityQualityOfService = NULL;
    RtlInitUnicodeString( &nameW, configW );

    /* @@ Wine registry key: HKCU\Software\Wine */
    if (NtOpenKey( &config_key, KEY_QUERY_VALUE, &attr )) config_key = 0;
    NtClose( root );
    if (!config_key) return;

    if (appname && *appname)
    {
        const WCHAR *p;
        WCHAR appversion[MAX_PATH+20];

        if ((p = wcsrchr( appname, '/' ))) appname = p + 1;
        if ((p = wcsrchr( appname, '\\' ))) appname = p + 1;

        wcscpy( appversion, appdefaultsW );
        wcscat( appversion, appname );
        RtlInitUnicodeString( &nameW, appversion );
        attr.RootDirectory = config_key;

        /* @@ Wine registry key: HKCU\Software\Wine\AppDefaults\app.exe */
        if (!NtOpenKey( &hkey, KEY_QUERY_VALUE, &attr ))
        {
            TRACE( "getting HideWineExports from %s\n", debugstr_w(appversion) );

            RtlInitUnicodeString( &nameW, hideWineExports );
            if (!NtQueryValueKey( hkey, &nameW, KeyValuePartialInformation, tmp, sizeof(tmp), &dummy ))
            {
                WCHAR *str = (WCHAR *)((KEY_VALUE_PARTIAL_INFORMATION *)tmp)->Data;
                hide_wine_exports = IS_OPTION_TRUE( str[0] );
                got_hide_wine_exports = TRUE;
            }

            NtClose( hkey );
        }
    }

    if (!got_hide_wine_exports)
    {
        TRACE( "getting default HideWineExports\n" );

        RtlInitUnicodeString( &nameW, hideWineExports );
        if (!NtQueryValueKey( config_key, &nameW, KeyValuePartialInformation, tmp, sizeof(tmp), &dummy ))
        {
            WCHAR *str = (WCHAR *)((KEY_VALUE_PARTIAL_INFORMATION *)tmp)->Data;
            hide_wine_exports = IS_OPTION_TRUE( str[0] );
        }
    }

    NtClose( config_key );
}


/***********************************************************************
 *           is_hidden_export
 *
 * Checks if a specific export should be hidden.
 */
static BOOL is_hidden_export( void *proc )
{
    return hide_wine_exports && (proc == &NTDLL_wine_get_version ||
                                 proc == &NTDLL_wine_get_build_id ||
                                 proc == &NTDLL_wine_get_host_version);
}


/******************************************************************
 *		LdrGetProcedureAddress  (NTDLL.@)
 */
NTSTATUS WINAPI LdrGetProcedureAddress(HMODULE module, const ANSI_STRING *name,
                                       ULONG ord, PVOID *address)
{
    IMAGE_EXPORT_DIRECTORY *exports;
    DWORD exp_size;
    NTSTATUS ret = STATUS_PROCEDURE_NOT_FOUND;

    RtlEnterCriticalSection( &loader_section );

    /* check if the module itself is invalid to return the proper error */
    if (!get_modref( module )) ret = STATUS_DLL_NOT_FOUND;
    else if ((exports = RtlImageDirectoryEntryToData( module, TRUE,
                                                      IMAGE_DIRECTORY_ENTRY_EXPORT, &exp_size )))
    {
        LPCWSTR load_path = NtCurrentTeb()->Peb->ProcessParameters->DllPath.Buffer;
        void *proc = name ? find_named_export( module, exports, exp_size, name->Buffer, -1, load_path )
                          : find_ordinal_export( module, exports, exp_size, ord - exports->Base, load_path );
        if (proc && !is_hidden_export( proc ))
        {
            *address = proc;
            ret = STATUS_SUCCESS;
        }
    }

    RtlLeaveCriticalSection( &loader_section );
    return ret;
}


/*************************************************************************
 *		is_16bit_builtin
 */
static BOOL is_16bit_builtin( HMODULE module )
{
    const IMAGE_EXPORT_DIRECTORY *exports;
    DWORD exp_size;

    if (!(exports = RtlImageDirectoryEntryToData( module, TRUE,
                                                  IMAGE_DIRECTORY_ENTRY_EXPORT, &exp_size )))
        return FALSE;

    return find_named_export( module, exports, exp_size, "__wine_spec_dos_header", -1, NULL ) != NULL;
}


/*************************************************************************
 *		build_so_dll_module
 *
 * Build the module for a .so builtin library.
 */
static NTSTATUS build_so_dll_module( const WCHAR *load_path, const UNICODE_STRING *nt_name,
                                     HMODULE module, DWORD flags, WINE_MODREF **pwm )
{
    IMAGE_NT_HEADERS *nt;
    WINE_MODREF *wm;
    NTSTATUS status;

    if (!(nt = RtlImageNtHeader( module ))) return STATUS_INVALID_IMAGE_FORMAT;

    if (!(wm = alloc_module( module, nt_name, TRUE ))) return STATUS_NO_MEMORY;

    virtual_create_builtin_view( module );

    if (!(flags & DONT_RESOLVE_DLL_REFERENCES) &&
        ((nt->FileHeader.Characteristics & IMAGE_FILE_DLL) ||
         nt->OptionalHeader.Subsystem == IMAGE_SUBSYSTEM_NATIVE ||
         is_16bit_builtin( module )))
    {
        if ((status = fixup_imports( wm, load_path )))
        {
            /* the module has only been inserted in the load & memory order lists */
            RemoveEntryList(&wm->ldr.InLoadOrderLinks);
            RemoveEntryList(&wm->ldr.InMemoryOrderLinks);
            RemoveEntryList(&wm->ldr.HashLinks);
            /* FIXME: free the modref */
            return status;
        }
    }

    TRACE( "loaded %s %p %p\n", debugstr_us(nt_name), wm, module );

    /* send the DLL load event */

    SERVER_START_REQ( load_dll )
    {
        req->base       = wine_server_client_ptr( module );
        req->dbg_offset = nt->FileHeader.PointerToSymbolTable;
        req->dbg_size   = nt->FileHeader.NumberOfSymbols;
        req->name       = wine_server_client_ptr( &wm->ldr.FullDllName.Buffer );
        wine_server_add_data( req, wm->ldr.FullDllName.Buffer, wm->ldr.FullDllName.Length );
        wine_server_call( req );
    }
    SERVER_END_REQ;

    /* setup relay debugging entry points */
#ifdef __aarch64__
    /* Always enable relay entry points on aarch64, to allow restoring
     * the TEB to x18. */
#else
    if (TRACE_ON(relay))
#endif
        RELAY_SetupDLL( module );

    *pwm = wm;
    return STATUS_SUCCESS;
}


/***********************************************************************
 *           load_builtin_callback
 *
 * Load a library in memory; callback function for wine_dll_register
 */
static void load_builtin_callback( void *module, const char *filename )
{
    if (!module)
    {
        ERR("could not map image for %s\n", debugstr_us(builtin_load_info->filename) );
        builtin_load_info->status = STATUS_NO_MEMORY;
        return;
    }
    builtin_load_info->status = build_so_dll_module( builtin_load_info->load_path,
                                                     builtin_load_info->filename, module,
                                                     0, &builtin_load_info->wm );
}


/***********************************************************************
 *           set_security_cookie
 *
 * Create a random security cookie for buffer overflow protection. Make
 * sure it does not accidentally match the default cookie value.
 */
static void set_security_cookie( void *module, SIZE_T len )
{
    static ULONG seed;
    IMAGE_LOAD_CONFIG_DIRECTORY *loadcfg;
    ULONG loadcfg_size;
    ULONG_PTR *cookie;

    loadcfg = RtlImageDirectoryEntryToData( module, TRUE, IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG, &loadcfg_size );
    if (!loadcfg) return;
    if (loadcfg_size < offsetof(IMAGE_LOAD_CONFIG_DIRECTORY, SecurityCookie) + sizeof(loadcfg->SecurityCookie)) return;
    if (!loadcfg->SecurityCookie) return;
    if (loadcfg->SecurityCookie < (ULONG_PTR)module ||
        loadcfg->SecurityCookie > (ULONG_PTR)module + len - sizeof(ULONG_PTR))
    {
        WARN( "security cookie %p outside of image %p-%p\n",
              (void *)loadcfg->SecurityCookie, module, (char *)module + len );
        return;
    }

    cookie = (ULONG_PTR *)loadcfg->SecurityCookie;
    TRACE( "initializing security cookie %p\n", cookie );

    if (!seed) seed = NtGetTickCount() ^ GetCurrentProcessId();
    for (;;)
    {
        if (*cookie == DEFAULT_SECURITY_COOKIE_16)
            *cookie = RtlRandom( &seed ) >> 16; /* leave the high word clear */
        else if (*cookie == DEFAULT_SECURITY_COOKIE_32)
            *cookie = RtlRandom( &seed );
#ifdef DEFAULT_SECURITY_COOKIE_64
        else if (*cookie == DEFAULT_SECURITY_COOKIE_64)
        {
            *cookie = RtlRandom( &seed );
            /* fill up, but keep the highest word clear */
            *cookie ^= (ULONG_PTR)RtlRandom( &seed ) << 16;
        }
#endif
        else
            break;
    }
}

static NTSTATUS perform_relocations( void *module, IMAGE_NT_HEADERS *nt, SIZE_T len )
{
    char *base;
    IMAGE_BASE_RELOCATION *rel, *end;
    const IMAGE_DATA_DIRECTORY *relocs;
    const IMAGE_SECTION_HEADER *sec;
    INT_PTR delta;
    ULONG protect_old[96], i;

    base = (char *)nt->OptionalHeader.ImageBase;
    if (module == base) return STATUS_SUCCESS;  /* nothing to do */

    /* no relocations are performed on non page-aligned binaries */
    if (nt->OptionalHeader.SectionAlignment < page_size)
        return STATUS_SUCCESS;

    if (!(nt->FileHeader.Characteristics & IMAGE_FILE_DLL) && NtCurrentTeb()->Peb->ImageBaseAddress)
        return STATUS_SUCCESS;

    relocs = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];

    if (nt->FileHeader.Characteristics & IMAGE_FILE_RELOCS_STRIPPED)
    {
        WARN( "Need to relocate module from %p to %p, but there are no relocation records\n",
              base, module );
        return STATUS_CONFLICTING_ADDRESSES;
    }

    if (!relocs->Size) return STATUS_SUCCESS;
    if (!relocs->VirtualAddress) return STATUS_CONFLICTING_ADDRESSES;

    if (nt->FileHeader.NumberOfSections > ARRAY_SIZE( protect_old ))
        return STATUS_INVALID_IMAGE_FORMAT;

    sec = (const IMAGE_SECTION_HEADER *)((const char *)&nt->OptionalHeader +
                                         nt->FileHeader.SizeOfOptionalHeader);
    for (i = 0; i < nt->FileHeader.NumberOfSections; i++)
    {
        void *addr = get_rva( module, sec[i].VirtualAddress );
        SIZE_T size = sec[i].SizeOfRawData;
        NtProtectVirtualMemory( NtCurrentProcess(), &addr,
                                &size, PAGE_READWRITE, &protect_old[i] );
    }

    TRACE( "relocating from %p-%p to %p-%p\n",
           base, base + len, module, (char *)module + len );

    rel = get_rva( module, relocs->VirtualAddress );
    end = get_rva( module, relocs->VirtualAddress + relocs->Size );
    delta = (char *)module - base;

    while (rel < end - 1 && rel->SizeOfBlock)
    {
        if (rel->VirtualAddress >= len)
        {
            WARN( "invalid address %p in relocation %p\n", get_rva( module, rel->VirtualAddress ), rel );
            return STATUS_ACCESS_VIOLATION;
        }
        rel = LdrProcessRelocationBlock( get_rva( module, rel->VirtualAddress ),
                                         (rel->SizeOfBlock - sizeof(*rel)) / sizeof(USHORT),
                                         (USHORT *)(rel + 1), delta );
        if (!rel) return STATUS_INVALID_IMAGE_FORMAT;
    }

    for (i = 0; i < nt->FileHeader.NumberOfSections; i++)
    {
        void *addr = get_rva( module, sec[i].VirtualAddress );
        SIZE_T size = sec[i].SizeOfRawData;
        NtProtectVirtualMemory( NtCurrentProcess(), &addr,
                                &size, protect_old[i], &protect_old[i] );
    }

    return STATUS_SUCCESS;
}

#ifdef _WIN64
/* convert PE header to 64-bit when loading a 32-bit IL-only module into a 64-bit process */
static BOOL convert_to_pe64( HMODULE module, const pe_image_info_t *info )
{
    static const ULONG copy_dirs[] = { IMAGE_DIRECTORY_ENTRY_RESOURCE,
                                       IMAGE_DIRECTORY_ENTRY_SECURITY,
                                       IMAGE_DIRECTORY_ENTRY_BASERELOC,
                                       IMAGE_DIRECTORY_ENTRY_DEBUG,
                                       IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR };
    IMAGE_OPTIONAL_HEADER32 hdr32 = { IMAGE_NT_OPTIONAL_HDR32_MAGIC };
    IMAGE_OPTIONAL_HEADER64 hdr64 = { IMAGE_NT_OPTIONAL_HDR64_MAGIC };
    IMAGE_NT_HEADERS *nt = RtlImageNtHeader( module );
    SIZE_T hdr_size = min( sizeof(hdr32), nt->FileHeader.SizeOfOptionalHeader );
    IMAGE_SECTION_HEADER *sec = (IMAGE_SECTION_HEADER *)((char *)&nt->OptionalHeader + hdr_size);
    SIZE_T size = info->header_size;
    void *addr = module;
    ULONG i, old_prot;

    TRACE( "%p\n", module );

    if (NtProtectVirtualMemory( NtCurrentProcess(), &addr, &size, PAGE_READWRITE, &old_prot ))
        return FALSE;

    if ((char *)module + size < (char *)(nt + 1) + nt->FileHeader.NumberOfSections * sizeof(*sec))
    {
        NtProtectVirtualMemory( NtCurrentProcess(), &addr, &size, old_prot, &old_prot );
        return FALSE;
    }

    memcpy( &hdr32, &nt->OptionalHeader, hdr_size );
    memcpy( &hdr64, &hdr32, offsetof( IMAGE_OPTIONAL_HEADER64, SizeOfStackReserve ));
    hdr64.Magic               = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
    hdr64.AddressOfEntryPoint = 0;
    hdr64.ImageBase           = hdr32.ImageBase;
    hdr64.SizeOfStackReserve  = hdr32.SizeOfStackReserve;
    hdr64.SizeOfStackCommit   = hdr32.SizeOfStackCommit;
    hdr64.SizeOfHeapReserve   = hdr32.SizeOfHeapReserve;
    hdr64.SizeOfHeapCommit    = hdr32.SizeOfHeapCommit;
    hdr64.LoaderFlags         = hdr32.LoaderFlags;
    hdr64.NumberOfRvaAndSizes = hdr32.NumberOfRvaAndSizes;
    for (i = 0; i < ARRAY_SIZE( copy_dirs ); i++)
        hdr64.DataDirectory[copy_dirs[i]] = hdr32.DataDirectory[copy_dirs[i]];

    memmove( nt + 1, sec, nt->FileHeader.NumberOfSections * sizeof(*sec) );
    nt->FileHeader.SizeOfOptionalHeader = sizeof(hdr64);
    nt->OptionalHeader = hdr64;
    NtProtectVirtualMemory( NtCurrentProcess(), &addr, &size, old_prot, &old_prot );
    return TRUE;
}
#endif

/* On WoW64 setups, an image mapping can also be created for the other 32/64 CPU */
/* but it cannot necessarily be loaded as a dll, so we need some additional checks */
static BOOL is_valid_binary( HMODULE module, const pe_image_info_t *info )
{
#ifdef __i386__
    return info->machine == IMAGE_FILE_MACHINE_I386;
#elif defined(__arm__)
    return info->machine == IMAGE_FILE_MACHINE_ARM ||
           info->machine == IMAGE_FILE_MACHINE_THUMB ||
           info->machine == IMAGE_FILE_MACHINE_ARMNT;
#elif defined(_WIN64)  /* support 32-bit IL-only images on 64-bit */
#ifdef __x86_64__
    if (info->machine == IMAGE_FILE_MACHINE_AMD64) return TRUE;
#else
    if (info->machine == IMAGE_FILE_MACHINE_ARM64) return TRUE;
#endif
    if (!info->contains_code) return TRUE;
    if (!(info->image_flags & IMAGE_FLAGS_ComPlusNativeReady))
    {
        /* check COM header directly, ignoring runtime version */
        DWORD size;
        const IMAGE_COR20_HEADER *cor_header = RtlImageDirectoryEntryToData( module, TRUE,
                                                          IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR, &size );
        if (!cor_header || !(cor_header->Flags & COMIMAGE_FLAGS_ILONLY)) return FALSE;
    }
    return convert_to_pe64( module, info );
#else
    return FALSE;  /* no wow64 support on other platforms */
#endif
}


/******************************************************************
 *		get_module_path_end
 *
 * Returns the end of the directory component of the module path.
 */
static inline const WCHAR *get_module_path_end( const WCHAR *module )
{
    const WCHAR *p;
    const WCHAR *mod_end = module;

    if ((p = wcsrchr( mod_end, '\\' ))) mod_end = p;
    if ((p = wcsrchr( mod_end, '/' ))) mod_end = p;
    if (mod_end == module + 2 && module[1] == ':') mod_end++;
    if (mod_end == module && module[0] && module[1] == ':') mod_end += 2;
    return mod_end;
}


/******************************************************************
 *		append_path
 *
 * Append a counted string to the load path. Helper for get_dll_load_path.
 */
static inline WCHAR *append_path( WCHAR *p, const WCHAR *str, int len )
{
    if (len == -1) len = wcslen(str);
    if (!len) return p;
    memcpy( p, str, len * sizeof(WCHAR) );
    p[len] = ';';
    return p + len + 1;
}


/******************************************************************
 *           get_dll_load_path
 */
static NTSTATUS get_dll_load_path( LPCWSTR module, LPCWSTR dll_dir, ULONG safe_mode, WCHAR **path )
{
    static const WCHAR pathW[] = {'P','A','T','H',0};

    const WCHAR *mod_end = module;
    UNICODE_STRING name, value;
    WCHAR *p, *ret;
    int len = ARRAY_SIZE(system_path) + 1, path_len = 0;

    if (module)
    {
        mod_end = get_module_path_end( module );
        len += (mod_end - module) + 1;
    }

    RtlInitUnicodeString( &name, pathW );
    value.Length = 0;
    value.MaximumLength = 0;
    value.Buffer = NULL;
    if (RtlQueryEnvironmentVariable_U( NULL, &name, &value ) == STATUS_BUFFER_TOO_SMALL)
        path_len = value.Length;

    if (dll_dir) len += wcslen( dll_dir ) + 1;
    else len += 2;  /* current directory */
    if (!(p = ret = RtlAllocateHeap( GetProcessHeap(), 0, path_len + len * sizeof(WCHAR) )))
        return STATUS_NO_MEMORY;

    p = append_path( p, module, mod_end - module );
    if (dll_dir) p = append_path( p, dll_dir, -1 );
    else if (!safe_mode) p = append_path( p, dotW, -1 );
    p = append_path( p, system_path, -1 );
    if (!dll_dir && safe_mode) p = append_path( p, dotW, -1 );

    value.Buffer = p;
    value.MaximumLength = path_len;

    while (RtlQueryEnvironmentVariable_U( NULL, &name, &value ) == STATUS_BUFFER_TOO_SMALL)
    {
        WCHAR *new_ptr;

        /* grow the buffer and retry */
        path_len = value.Length;
        if (!(new_ptr = RtlReAllocateHeap( GetProcessHeap(), 0, ret, path_len + len * sizeof(WCHAR) )))
        {
            RtlFreeHeap( GetProcessHeap(), 0, ret );
            return STATUS_NO_MEMORY;
        }
        value.Buffer = new_ptr + (value.Buffer - ret);
        value.MaximumLength = path_len;
        ret = new_ptr;
    }
    value.Buffer[value.Length / sizeof(WCHAR)] = 0;
    *path = ret;
    return STATUS_SUCCESS;
}


/******************************************************************
 *		get_dll_load_path_search_flags
 */
static NTSTATUS get_dll_load_path_search_flags( LPCWSTR module, DWORD flags, WCHAR **path )
{
    const WCHAR *image = NULL, *mod_end, *image_end;
    struct dll_dir_entry *dir;
    WCHAR *p, *ret;
    int len = 1;

    if (flags & LOAD_LIBRARY_SEARCH_DEFAULT_DIRS)
        flags |= (LOAD_LIBRARY_SEARCH_APPLICATION_DIR |
                  LOAD_LIBRARY_SEARCH_USER_DIRS |
                  LOAD_LIBRARY_SEARCH_SYSTEM32);

    if (flags & LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR)
    {
        DWORD type = RtlDetermineDosPathNameType_U( module );
        if (type != ABSOLUTE_DRIVE_PATH && type != ABSOLUTE_PATH && type != DEVICE_PATH)
            return STATUS_INVALID_PARAMETER;
        mod_end = get_module_path_end( module );
        len += (mod_end - module) + 1;
    }
    else module = NULL;

    if (flags & LOAD_LIBRARY_SEARCH_APPLICATION_DIR)
    {
        image = NtCurrentTeb()->Peb->ProcessParameters->ImagePathName.Buffer;
        image_end = get_module_path_end( image );
        len += (image_end - image) + 1;
    }

    if (flags & LOAD_LIBRARY_SEARCH_USER_DIRS)
    {
        LIST_FOR_EACH_ENTRY( dir, &dll_dir_list, struct dll_dir_entry, entry )
            len += wcslen( dir->dir + 4 /* \??\ */ ) + 1;
        if (dll_directory.Length) len += dll_directory.Length / sizeof(WCHAR) + 1;
    }

    if (flags & LOAD_LIBRARY_SEARCH_SYSTEM32) len += wcslen( system_dir );

    if ((p = ret = RtlAllocateHeap( GetProcessHeap(), 0, len * sizeof(WCHAR) )))
    {
        if (module) p = append_path( p, module, mod_end - module );
        if (image) p = append_path( p, image, image_end - image );
        if (flags & LOAD_LIBRARY_SEARCH_USER_DIRS)
        {
            LIST_FOR_EACH_ENTRY( dir, &dll_dir_list, struct dll_dir_entry, entry )
                p = append_path( p, dir->dir + 4 /* \??\ */, -1 );
            p = append_path( p, dll_directory.Buffer, dll_directory.Length / sizeof(WCHAR) );
        }
        if (flags & LOAD_LIBRARY_SEARCH_SYSTEM32) wcscpy( p, system_dir );
        else
        {
            if (p > ret) p--;
            *p = 0;
        }
    }
    *path = ret;
    return STATUS_SUCCESS;
}


/***********************************************************************
 *	open_dll_file
 *
 * Open a file for a new dll. Helper for find_dll_file.
 */
static NTSTATUS open_dll_file( UNICODE_STRING *nt_name, WINE_MODREF **pwm,
                               void **module, pe_image_info_t *image_info, struct stat *st )
{
    FILE_BASIC_INFORMATION info;
    OBJECT_ATTRIBUTES attr;
    IO_STATUS_BLOCK io;
    LARGE_INTEGER size;
    SIZE_T len = 0;
    NTSTATUS status;
    HANDLE handle, mapping;
    int fd, needs_close;

    if ((*pwm = find_fullname_module( nt_name )))
    {
        NtUnmapViewOfSection( NtCurrentProcess(), *module );
        *module = NULL;
        return STATUS_SUCCESS;
    }

    attr.Length = sizeof(attr);
    attr.RootDirectory = 0;
    attr.Attributes = OBJ_CASE_INSENSITIVE;
    attr.ObjectName = nt_name;
    attr.SecurityDescriptor = NULL;
    attr.SecurityQualityOfService = NULL;
    if ((status = __syscall_NtOpenFile( &handle, GENERIC_READ | SYNCHRONIZE, &attr, &io,
                              FILE_SHARE_READ | FILE_SHARE_DELETE,
                              FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE )))
    {
        if (status != STATUS_OBJECT_PATH_NOT_FOUND &&
            status != STATUS_OBJECT_NAME_NOT_FOUND &&
            !NtQueryAttributesFile( &attr, &info ))
        {
            /* if the file exists but failed to open, report the error */
            return status;
        }
        /* otherwise continue searching */
        return STATUS_DLL_NOT_FOUND;
    }

    if (!server_get_unix_fd( handle, 0, &fd, &needs_close, NULL, NULL ))
    {
        fstat( fd, st );
        if (needs_close) close( fd );
        if ((*pwm = find_fileid_module( st )))
        {
            TRACE( "%s is the same file as existing module %p %s\n", debugstr_w( nt_name->Buffer ),
                   (*pwm)->ldr.DllBase, debugstr_w( (*pwm)->ldr.FullDllName.Buffer ));
            NtClose( handle );
            NtUnmapViewOfSection( NtCurrentProcess(), *module );
            *module = NULL;
            return STATUS_SUCCESS;
        }
    }

    size.QuadPart = 0;
    status = NtCreateSection( &mapping, STANDARD_RIGHTS_REQUIRED | SECTION_QUERY |
                              SECTION_MAP_READ | SECTION_MAP_EXECUTE,
                              NULL, &size, PAGE_EXECUTE_READ, SEC_IMAGE, handle );
    NtClose( handle );

    if (!status)
    {
        if (*module)
        {
            NtUnmapViewOfSection( NtCurrentProcess(), *module );
            *module = NULL;
        }
        status = virtual_map_section( mapping, module, 0, 0, NULL, &len,
                                      0, PAGE_EXECUTE_READ, image_info );
        if (status == STATUS_IMAGE_NOT_AT_BASE) status = STATUS_SUCCESS;
        NtClose( mapping );
    }
    if (!status && !is_valid_binary( *module, image_info ))
    {
        TRACE( "%s is for arch %x, continuing search\n", debugstr_us(nt_name), image_info->machine );
        NtUnmapViewOfSection( NtCurrentProcess(), *module );
        *module = NULL;
        status = STATUS_IMAGE_MACHINE_TYPE_MISMATCH;
    }
    return status;
}

/***************************************************************************
 *	get_basename
 *
 * Return the base name of a file name (i.e. remove the path components).
 */
static const WCHAR *get_basename( const WCHAR *name )
{
    const WCHAR *ptr;

    if (name[0] && name[1] == ':') name += 2;  /* strip drive specification */
    if ((ptr = wcsrchr( name, '\\' ))) name = ptr + 1;
    if ((ptr = wcsrchr( name, '/' ))) name = ptr + 1;
    return name;
}


/***************************************************************************
 *	large_address_aware_env
 *
 * Checks for override for LARGE_ADDRESS_AWARE bit in environment. Takes
 * precedence over any AppDefaults registry entry.
 *
 * Returns:
 *    -1 if not defined in environment (prefer registry entries)
 *     0 if disabled
 *     1 if enabled
 */
static int large_address_aware_env( void )
{
    static int large_address_aware_cached = -2;

    if (large_address_aware_cached == -2) {
        const char *envvar = getenv("WINE_LARGE_ADDRESS_AWARE");
        if (envvar)
            large_address_aware_cached = atoi(envvar);
        else
            large_address_aware_cached = -1;
    }

    return large_address_aware_cached;
}

/***************************************************************************
 *	needs_override_large_address_aware
 *
 * Checks for AppDefaults override for LARGE_ADDRESS_AWARE bit.
 *
 * Returns:
 *    TRUE  if override was found in the registry, or environment variable
 *          explicitly enabled this override
 *    FALSE if no override was found in the registry, or environment variable
 *          explicitly disabled this override
 */
static BOOL needs_override_large_address_aware(const WCHAR *exename)
{
    BOOL result;
    OBJECT_ATTRIBUTES attr;
    UNICODE_STRING nameW;
    UNICODE_STRING valueNameW;
    HANDLE root;
    WCHAR *str;
    static const WCHAR AppDefaultsW[] = {'S','o','f','t','w','a','r','e','\\','W','i','n','e','\\',
                                         'A','p','p','D','e','f','a','u','l','t','s','\\',0};
    static const WCHAR LargeAddressAwareW[] = {'L','a','r','g','e','A','d','d','r','e','s','s','A','w','a','r','e',0};
    char tmp[64];
    KEY_VALUE_PARTIAL_INFORMATION *info = (KEY_VALUE_PARTIAL_INFORMATION *)tmp;
    DWORD count;
    int env_override;
    HANDLE app_key = (HANDLE)-1;

    /* Environment variable takes precedence. */
    env_override = large_address_aware_env();
    if (large_address_aware_env() >= 0)
        return env_override;

    exename = get_basename(exename);

    if (app_key != (HANDLE)-1) return TRUE;

    str = RtlAllocateHeap( GetProcessHeap(), 0,
                           sizeof(AppDefaultsW) + wcslen(exename) * sizeof(WCHAR) );
    if (!str) return FALSE;
    wcscpy( str, AppDefaultsW );
    wcscat( str, exename );

    RtlOpenCurrentUser( KEY_ALL_ACCESS, &root );
    attr.Length = sizeof(attr);
    attr.RootDirectory = root;
    attr.ObjectName = &nameW;
    attr.Attributes = 0;
    attr.SecurityDescriptor = NULL;
    attr.SecurityQualityOfService = NULL;
    RtlInitUnicodeString( &nameW, str );
    RtlInitUnicodeString( &valueNameW, LargeAddressAwareW );

    result = TRUE;

    /* @@ Wine registry key: HKCU\Software\Wine\AppDefaults\app.exe */
    if (!NtOpenKey( &app_key, KEY_ALL_ACCESS, &attr ))
    {
        if (!NtQueryValueKey( app_key, &valueNameW, KeyValuePartialInformation, tmp, sizeof(tmp)-1, &count))
        {
            if (info->DataLength >= sizeof(DWORD))
            {
                if ((*(DWORD *)info->Data) == 0)
                    result = FALSE;
            }
        }
        NtClose( app_key );
    }
    NtClose( root );
    RtlFreeHeap( GetProcessHeap(), 0, str );
    return result;
}

BOOL CDECL __wine_needs_override_large_address_aware(void)
{
    PEB *peb = NtCurrentTeb()->Peb;
    return needs_override_large_address_aware(peb->ProcessParameters->ImagePathName.Buffer);
}


/******************************************************************************
 *	load_native_dll  (internal)
 */
static NTSTATUS load_native_dll( LPCWSTR load_path, const UNICODE_STRING *nt_name, void **module,
                                 const pe_image_info_t *image_info, DWORD flags, WINE_MODREF** pwm,
                                 struct stat *st )
{
    IMAGE_NT_HEADERS *nt = RtlImageNtHeader( *module );
    WINE_MODREF *wm;
    NTSTATUS status;
    const char *dll_type = (image_info->image_flags & IMAGE_FLAGS_WineBuiltin) ? "PE builtin" : "native";

    TRACE("Trying %s dll %s\n", dll_type, debugstr_us(nt_name) );

    /* perform base relocation, if necessary */

    if ((status = perform_relocations( *module, nt, image_info->map_size ))) return status;

    /* create the MODREF */

    if (!(wm = alloc_module( *module, nt_name, (image_info->image_flags & IMAGE_FLAGS_WineBuiltin) )))
        return STATUS_NO_MEMORY;

    wm->dev = st->st_dev;
    wm->ino = st->st_ino;
    if (image_info->loader_flags) wm->ldr.Flags |= LDR_COR_IMAGE;
    if (image_info->image_flags & IMAGE_FLAGS_ComPlusILOnly) wm->ldr.Flags |= LDR_COR_ILONLY;

    set_security_cookie( *module, image_info->map_size );

    /* fixup imports */

    if (!(flags & DONT_RESOLVE_DLL_REFERENCES) &&
        ((nt->FileHeader.Characteristics & IMAGE_FILE_DLL) ||
         nt->OptionalHeader.Subsystem == IMAGE_SUBSYSTEM_NATIVE))
    {
        if (wm->ldr.Flags & LDR_COR_ILONLY)
            status = fixup_imports_ilonly( wm, load_path, &wm->ldr.EntryPoint );
        else
            status = fixup_imports( wm, load_path );
        if (status != STATUS_SUCCESS)
        {
            /* the module has only be inserted in the load & memory order lists */
            RemoveEntryList(&wm->ldr.InLoadOrderLinks);
            RemoveEntryList(&wm->ldr.InMemoryOrderLinks);
            RemoveEntryList(&wm->ldr.HashLinks);

            /* FIXME: there are several more dangling references
             * left. Including dlls loaded by this dll before the
             * failed one. Unrolling is rather difficult with the
             * current structure and we can leave them lying
             * around with no problems, so we don't care.
             * As these might reference our wm, we don't free it.
             */
            *module = NULL;
            return status;
        }
    }

    /* send DLL load event */

    SERVER_START_REQ( load_dll )
    {
        req->base       = wine_server_client_ptr( *module );
        req->dbg_offset = nt->FileHeader.PointerToSymbolTable;
        req->dbg_size   = nt->FileHeader.NumberOfSymbols;
        req->name       = wine_server_client_ptr( &wm->ldr.FullDllName.Buffer );
        wine_server_add_data( req, wm->ldr.FullDllName.Buffer, wm->ldr.FullDllName.Length );
        wine_server_call( req );
    }
    SERVER_END_REQ;

    if (image_info->image_flags & IMAGE_FLAGS_WineBuiltin)
    {
        if (TRACE_ON(relay)) RELAY_SetupDLL( *module );
    }
    else
    {
        if ((wm->ldr.Flags & LDR_IMAGE_IS_DLL) && TRACE_ON(snoop)) SNOOP_SetupDLL( *module );
    }

    TRACE_(loaddll)( "Loaded %s at %p: %s\n", debugstr_w(wm->ldr.FullDllName.Buffer), *module, dll_type );

    wm->ldr.LoadCount = 1;
    *pwm = wm;
    *module = NULL;
    return STATUS_SUCCESS;
}


/* check if the library is the correct architecture */
/* only returns false for a valid library of the wrong arch */
static int check_library_arch( int fd )
{
#ifdef __APPLE__
    struct  /* Mach-O header */
    {
        unsigned int magic;
        unsigned int cputype;
    } header;

    if (read( fd, &header, sizeof(header) ) != sizeof(header)) return 1;
    if (header.magic != 0xfeedface) return 1;
    if (sizeof(void *) == sizeof(int)) return !(header.cputype >> 24);
    else return (header.cputype >> 24) == 1; /* CPU_ARCH_ABI64 */
#else
    struct  /* ELF header */
    {
        unsigned char magic[4];
        unsigned char class;
        unsigned char data;
        unsigned char version;
    } header;

    if (read( fd, &header, sizeof(header) ) != sizeof(header)) return 1;
    if (memcmp( header.magic, "\177ELF", 4 )) return 1;
    if (header.version != 1 /* EV_CURRENT */) return 1;
#ifdef WORDS_BIGENDIAN
    if (header.data != 2 /* ELFDATA2MSB */) return 1;
#else
    if (header.data != 1 /* ELFDATA2LSB */) return 1;
#endif
    if (sizeof(void *) == sizeof(int)) return header.class == 1; /* ELFCLASS32 */
    else return header.class == 2; /* ELFCLASS64 */
#endif
}

static inline char *prepend( char *buffer, const char *str, size_t len )
{
    return memcpy( buffer - len, str, len );
}

/***********************************************************************
 *           open_builtin_file
 */
static NTSTATUS open_builtin_file( char *name, WINE_MODREF **pwm, void **module,
                                   pe_image_info_t *image_info, struct stat *st, char **so_name )
{
    ANSI_STRING strA;
    UNICODE_STRING nt_name;
    NTSTATUS status;
    int fd;

    nt_name.Buffer = NULL;
    RtlInitString( &strA, name );
    if ((status = wine_unix_to_nt_file_name( &strA, &nt_name ))) return status;

    status = open_dll_file( &nt_name, pwm, module, image_info, st );
    RtlFreeUnicodeString( &nt_name );

    /* ignore non-builtins */
    if (!status && !*pwm && !(image_info->image_flags & IMAGE_FLAGS_WineBuiltin))
    {
        WARN( "%s found in WINEDLLPATH but not a builtin, ignoring\n", debugstr_a(name) );
        NtUnmapViewOfSection( NtCurrentProcess(), *module );
        *module = NULL;
        status = STATUS_DLL_NOT_FOUND;
    }

    if (status != STATUS_DLL_NOT_FOUND) return status;

    /* try .so file */

    strcat( name, ".so" );
    if ((fd = open( name, O_RDONLY )) != -1)
    {
        if (check_library_arch( fd ))
        {
            if ((*so_name = RtlAllocateHeap( GetProcessHeap(), 0, strlen(name) + 1 )))
                strcpy( *so_name, name );
            NtUnmapViewOfSection( NtCurrentProcess(), *module );
            *module = NULL;
            status = STATUS_SUCCESS;
        }
        else status = STATUS_IMAGE_MACHINE_TYPE_MISMATCH;
        close( fd );
    }
    return status;
}


/***********************************************************************
 *           find_builtin_dll
 */
static NTSTATUS find_builtin_dll( const WCHAR *name, WINE_MODREF **pwm,
                                  void **module, pe_image_info_t *image_info, struct stat *st,
                                  char **so_name )
{
    unsigned int i, pos, len, namelen, maxlen = 0;
    char *ptr, *file;
    NTSTATUS status = STATUS_DLL_NOT_FOUND;
    BOOL found_image = FALSE;
    const char **dll_paths;
    SIZE_T dll_path_maxlen;

    unix_funcs->get_dll_path( &dll_paths, &dll_path_maxlen );

    len = wcslen( name );
    if (build_dir) maxlen = strlen(build_dir) + sizeof("/programs/") + len;
    maxlen = max( maxlen, dll_path_maxlen + 1 ) + len + sizeof(".so");

    if (!(file = RtlAllocateHeap( GetProcessHeap(), 0, maxlen ))) return STATUS_NO_MEMORY;

    pos = maxlen - len - sizeof(".so");
    /* we don't want to depend on the current codepage here */
    for (i = 0; i < len; i++)
    {
        if (name[i] > 127) goto done;
        file[pos + i] = (char)name[i];
        if (file[pos + i] >= 'A' && file[pos + i] <= 'Z') file[pos + i] += 'a' - 'A';
    }
    file[--pos] = '/';

    if (build_dir)
    {
        /* try as a dll */
        ptr = file + pos;
        namelen = len + 1;
        file[pos + len + 1] = 0;
        if (namelen > 4 && !memcmp( ptr + namelen - 4, ".dll", 4 )) namelen -= 4;
        ptr = prepend( ptr, ptr, namelen );
        ptr = prepend( ptr, "/dlls", sizeof("/dlls") - 1 );
        ptr = prepend( ptr, build_dir, strlen(build_dir) );
        status = open_builtin_file( ptr, pwm, module, image_info, st, so_name );
        if (status != STATUS_DLL_NOT_FOUND) goto done;

        /* now as a program */
        ptr = file + pos;
        namelen = len + 1;
        file[pos + len + 1] = 0;
        if (namelen > 4 && !memcmp( ptr + namelen - 4, ".exe", 4 )) namelen -= 4;
        ptr = prepend( ptr, ptr, namelen );
        ptr = prepend( ptr, "/programs", sizeof("/programs") - 1 );
        ptr = prepend( ptr, build_dir, strlen(build_dir) );
        status = open_builtin_file( ptr, pwm, module, image_info, st, so_name );
        if (status != STATUS_DLL_NOT_FOUND) goto done;
    }

    for (i = 0; dll_paths[i]; i++)
    {
        file[pos + len + 1] = 0;
        ptr = prepend( file + pos, dll_paths[i], strlen(dll_paths[i]) );
        status = open_builtin_file( ptr, pwm, module, image_info, st, so_name );
        if (status == STATUS_IMAGE_MACHINE_TYPE_MISMATCH) found_image = TRUE;
        else if (status != STATUS_DLL_NOT_FOUND) goto done;
    }

    if (found_image) status = STATUS_IMAGE_MACHINE_TYPE_MISMATCH;
    WARN( "cannot find builtin library for %s\n", debugstr_w(name) );

done:
    RtlFreeHeap( GetProcessHeap(), 0, file );
    return status;
}


/***********************************************************************
 *           load_so_dll
 */
static NTSTATUS load_so_dll( LPCWSTR load_path, const UNICODE_STRING *nt_name,
                             const char *so_name, DWORD flags, WINE_MODREF** pwm )
{
    static const WCHAR soW[] = {'.','s','o',0};
    DWORD len;
    void *handle;
    const IMAGE_NT_HEADERS *nt;
    struct builtin_load_info info, *prev_info;
    ANSI_STRING unix_name;
    UNICODE_STRING win_name = *nt_name;

    unix_name.Buffer = NULL;
    info.load_path = load_path;
    info.filename  = &win_name;
    info.status    = STATUS_SUCCESS;
    info.wm        = NULL;

    if (!so_name)
    {
        if (wine_nt_to_unix_file_name( nt_name, &unix_name, FILE_OPEN ))
            return STATUS_DLL_NOT_FOUND;

        /* remove .so extension from Windows name */
        len = nt_name->Length / sizeof(WCHAR);
        if (len > 3 && !wcsicmp( nt_name->Buffer + len - 3, soW )) win_name.Length -= 3 * sizeof(WCHAR);
    }

    TRACE( "loading %s from so lib %s\n", debugstr_us(&win_name),
           debugstr_a( so_name ? so_name : unix_name.Buffer ));

    prev_info = builtin_load_info;
    builtin_load_info = &info;
    handle = dlopen( so_name ? so_name : unix_name.Buffer, RTLD_NOW );
    builtin_load_info = prev_info;
    RtlFreeHeap( GetProcessHeap(), 0, unix_name.Buffer );

    if (!handle)
    {
        if (so_name)
        {
            ERR("failed to load .so lib %s: %s\n", debugstr_a(so_name), dlerror() );
            info.status = STATUS_PROCEDURE_NOT_FOUND;
        }
        else
        {
            WARN( "failed to load .so lib %s: %s\n", debugstr_us(nt_name), dlerror() );
            info.status = STATUS_INVALID_IMAGE_FORMAT;
        }
    }

    if (info.status != STATUS_SUCCESS) goto failed;

    if (!info.wm && (nt = dlsym( handle, "__wine_spec_nt_header" )))
    {
        HMODULE module = (HMODULE)((nt->OptionalHeader.ImageBase + 0xffff) & ~0xffff);
        if ((info.wm = get_modref( module )))  /* already loaded */
        {
            TRACE( "Found %s at %p for builtin %s\n",
                   debugstr_w(info.wm->ldr.FullDllName.Buffer), info.wm->ldr.DllBase,
                   debugstr_us(nt_name) );
            if (info.wm->ldr.LoadCount != -1) info.wm->ldr.LoadCount++;
            dlclose( handle );
        }
        else
        {
            if ((info.status = unix_funcs->map_so_dll( nt, module ))) goto failed;
            if ((info.status = build_so_dll_module( load_path, &win_name, module, flags, &info.wm )))
                goto failed;
            TRACE_(loaddll)( "Loaded %s at %p: builtin\n",
                             debugstr_w(info.wm->ldr.FullDllName.Buffer), info.wm->ldr.DllBase );
            info.wm->ldr.LoadCount = 1;
            info.wm->so_handle = handle;
        }
    }
    else if (!info.wm)
    {
        /* The constructor wasn't called, this means the .so is already
         * loaded under a different name. Try to find the wm for it. */

        if (!(info.wm = find_so_module( handle )))
        {
            info.status = STATUS_INVALID_IMAGE_FORMAT;
            goto failed;
        }
        TRACE( "Found %s at %p for builtin %s\n",
               debugstr_w(info.wm->ldr.FullDllName.Buffer), info.wm->ldr.DllBase,
               debugstr_us(nt_name) );
        if (info.wm->ldr.LoadCount != -1) info.wm->ldr.LoadCount++;
        dlclose( handle ); /* release the libdl refcount */
    }
    else
    {
        TRACE_(loaddll)( "Loaded %s at %p: builtin\n", debugstr_w(info.wm->ldr.FullDllName.Buffer), info.wm->ldr.DllBase );
        info.wm->ldr.LoadCount = 1;
        info.wm->so_handle = handle;
    }

    *pwm = info.wm;
    return STATUS_SUCCESS;

failed:
    if (handle) dlclose( handle );
    return info.status;
}


/***********************************************************************
 *           load_builtin_dll
 */
static NTSTATUS load_builtin_dll( LPCWSTR load_path, const UNICODE_STRING *nt_name, void **module_ptr,
                                  DWORD flags, WINE_MODREF** pwm )
{
    const WCHAR *name, *p;
    NTSTATUS status;
    void *module = NULL;
    pe_image_info_t image_info;
    struct stat st;
    char *so_name;

    /* Fix the name in case we have a full path and extension */
    name = nt_name->Buffer;
    if ((p = wcsrchr( name, '\\' ))) name = p + 1;
    if ((p = wcsrchr( name, '/' ))) name = p + 1;

    TRACE("Trying built-in %s\n", debugstr_w(name));

    if (!module_ptr) module_ptr = &module;

    status = find_builtin_dll( name, pwm, module_ptr, &image_info, &st, &so_name );
    if (status) return status;

    if (*pwm)
    {
        if ((*pwm)->ldr.LoadCount != -1) (*pwm)->ldr.LoadCount++;
        TRACE( "Found %s for %s at %p, count=%d\n",
               debugstr_w((*pwm)->ldr.FullDllName.Buffer), debugstr_w(name),
               (*pwm)->ldr.DllBase, (*pwm)->ldr.LoadCount);
        return STATUS_SUCCESS;
    }

    if (*module_ptr)
    {
        TRACE( "loading %s from PE builtin %s\n", debugstr_w(name), debugstr_us(nt_name) );
        return load_native_dll( load_path, nt_name, module_ptr, &image_info, flags, pwm, &st );
    }

    status = load_so_dll( load_path, nt_name, so_name, flags, pwm );
    RtlFreeHeap( GetProcessHeap(), 0, so_name );
    return status;
}


/***********************************************************************
 *	find_actctx_dll
 *
 * Find the full path (if any) of the dll from the activation context.
 */
static NTSTATUS find_actctx_dll( LPCWSTR libname, LPWSTR *fullname )
{
    static const WCHAR winsxsW[] = {'\\','w','i','n','s','x','s','\\'};
    static const WCHAR dotManifestW[] = {'.','m','a','n','i','f','e','s','t',0};

    ACTIVATION_CONTEXT_ASSEMBLY_DETAILED_INFORMATION *info;
    ACTCTX_SECTION_KEYED_DATA data;
    UNICODE_STRING nameW;
    NTSTATUS status;
    SIZE_T needed, size = 1024;
    WCHAR *p;

    RtlInitUnicodeString( &nameW, libname );
    data.cbSize = sizeof(data);
    status = RtlFindActivationContextSectionString( FIND_ACTCTX_SECTION_KEY_RETURN_HACTCTX, NULL,
                                                    ACTIVATION_CONTEXT_SECTION_DLL_REDIRECTION,
                                                    &nameW, &data );
    if (status != STATUS_SUCCESS) return status;

    for (;;)
    {
        if (!(info = RtlAllocateHeap( GetProcessHeap(), 0, size )))
        {
            status = STATUS_NO_MEMORY;
            goto done;
        }
        status = RtlQueryInformationActivationContext( 0, data.hActCtx, &data.ulAssemblyRosterIndex,
                                                       AssemblyDetailedInformationInActivationContext,
                                                       info, size, &needed );
        if (status == STATUS_SUCCESS) break;
        if (status != STATUS_BUFFER_TOO_SMALL) goto done;
        RtlFreeHeap( GetProcessHeap(), 0, info );
        size = needed;
        /* restart with larger buffer */
    }

    if (!info->lpAssemblyManifestPath)
    {
        status = STATUS_SXS_KEY_NOT_FOUND;
        goto done;
    }

    if ((p = wcsrchr( info->lpAssemblyManifestPath, '\\' )))
    {
        DWORD len, dirlen = info->ulAssemblyDirectoryNameLength / sizeof(WCHAR);
        p++;
        len = wcslen( p );
        if (!dirlen || len <= dirlen ||
            RtlCompareUnicodeStrings( p, dirlen, info->lpAssemblyDirectoryName, dirlen, TRUE ) ||
            wcsicmp( p + dirlen, dotManifestW ))
        {
            /* manifest name does not match directory name, so it's not a global
             * windows/winsxs manifest; use the manifest directory name instead */
            dirlen = p - info->lpAssemblyManifestPath;
            needed = (dirlen + 1) * sizeof(WCHAR) + nameW.Length;
            if (!(*fullname = p = RtlAllocateHeap( GetProcessHeap(), 0, needed )))
            {
                status = STATUS_NO_MEMORY;
                goto done;
            }
            memcpy( p, info->lpAssemblyManifestPath, dirlen * sizeof(WCHAR) );
            p += dirlen;
            wcscpy( p, libname );
            goto done;
        }
    }

    if (!info->lpAssemblyDirectoryName)
    {
        status = STATUS_SXS_KEY_NOT_FOUND;
        goto done;
    }

    needed = (wcslen(windows_dir) * sizeof(WCHAR) +
              sizeof(winsxsW) + info->ulAssemblyDirectoryNameLength + nameW.Length + 2*sizeof(WCHAR));

    if (!(*fullname = p = RtlAllocateHeap( GetProcessHeap(), 0, needed )))
    {
        status = STATUS_NO_MEMORY;
        goto done;
    }
    wcscpy( p, windows_dir );
    p += wcslen(p);
    memcpy( p, winsxsW, sizeof(winsxsW) );
    p += ARRAY_SIZE( winsxsW );
    memcpy( p, info->lpAssemblyDirectoryName, info->ulAssemblyDirectoryNameLength );
    p += info->ulAssemblyDirectoryNameLength / sizeof(WCHAR);
    *p++ = '\\';
    wcscpy( p, libname );
done:
    RtlFreeHeap( GetProcessHeap(), 0, info );
    RtlReleaseActivationContext( data.hActCtx );
    return status;
}


/***********************************************************************
 *	search_dll_file
 *
 * Search for dll in the specified paths.
 */
static NTSTATUS search_dll_file( LPCWSTR paths, LPCWSTR search, UNICODE_STRING *nt_name,
                                 WINE_MODREF **pwm, void **module, pe_image_info_t *image_info,
                                 struct stat *st )
{
    WCHAR *name;
    BOOL found_image = FALSE;
    NTSTATUS status = STATUS_DLL_NOT_FOUND;
    ULONG len = wcslen( paths );

    if (len < wcslen( system_dir )) len = wcslen( system_dir );
    len += wcslen( search ) + 2;

    if (!(name = RtlAllocateHeap( GetProcessHeap(), 0, len * sizeof(WCHAR) )))
        return STATUS_NO_MEMORY;

    while (*paths)
    {
        LPCWSTR ptr = paths;

        while (*ptr && *ptr != ';') ptr++;
        len = ptr - paths;
        if (*ptr == ';') ptr++;
        memcpy( name, paths, len * sizeof(WCHAR) );
        if (len && name[len - 1] != '\\') name[len++] = '\\';
        wcscpy( name + len, search );

        nt_name->Buffer = NULL;
        if ((status = RtlDosPathNameToNtPathName_U_WithStatus( name, nt_name, NULL, NULL ))) goto done;

        status = open_dll_file( nt_name, pwm, module, image_info, st );
        if (status == STATUS_IMAGE_MACHINE_TYPE_MISMATCH) found_image = TRUE;
        else if (status != STATUS_DLL_NOT_FOUND) goto done;
        RtlFreeUnicodeString( nt_name );
        paths = ptr;
    }

    if (!found_image)
    {
        /* not found, return file in the system dir to be loaded as builtin */
        wcscpy( name, system_dir );
        wcscat( name, search );
        if (!RtlDosPathNameToNtPathName_U( name, nt_name, NULL, NULL )) status = STATUS_NO_MEMORY;
    }
    else status = STATUS_IMAGE_MACHINE_TYPE_MISMATCH;

done:
    RtlFreeHeap( GetProcessHeap(), 0, name );
    return status;
}


/***********************************************************************
 *	find_dll_file
 *
 * Find the file (or already loaded module) for a given dll name.
 */
static NTSTATUS find_dll_file( const WCHAR *load_path, const WCHAR *libname, const WCHAR *default_ext,
                               UNICODE_STRING *nt_name, WINE_MODREF **pwm,
                               void **module, pe_image_info_t *image_info, struct stat *st )
{
    WCHAR *ext, *dllname;
    NTSTATUS status;
    ULONG wow64_old_value = 0;

    *pwm = NULL;
    *module = NULL;
    dllname = NULL;

    if (default_ext)  /* first append default extension */
    {
        if (!(ext = wcsrchr( libname, '.')) || wcschr( ext, '/' ) || wcschr( ext, '\\'))
        {
            if (!(dllname = RtlAllocateHeap( GetProcessHeap(), 0,
                                             (wcslen(libname)+wcslen(default_ext)+1) * sizeof(WCHAR))))
                return STATUS_NO_MEMORY;
            wcscpy( dllname, libname );
            wcscat( dllname, default_ext );
            libname = dllname;
        }
    }

    /* Win 7/2008R2 and up seem to re-enable WoW64 FS redirection when loading libraries */
    if (is_wow64) RtlWow64EnableFsRedirectionEx( 0, &wow64_old_value );

    nt_name->Buffer = NULL;

    if (!contains_path( libname ))
    {
        WCHAR *fullname = NULL;

        status = find_actctx_dll( libname, &fullname );
        if (status == STATUS_SUCCESS)
        {
            TRACE ("found %s for %s\n", debugstr_w(fullname), debugstr_w(libname) );
            RtlFreeHeap( GetProcessHeap(), 0, dllname );
            libname = dllname = fullname;
        }
        else
        {
            if (status != STATUS_SXS_KEY_NOT_FOUND) goto done;
            if ((*pwm = find_basename_module( libname )) != NULL)
            {
                status = STATUS_SUCCESS;
                goto done;
            }
        }
    }

    if (RtlDetermineDosPathNameType_U( libname ) == RELATIVE_PATH)
        status = search_dll_file( load_path, libname, nt_name, pwm, module, image_info, st );
    else if (!(status = RtlDosPathNameToNtPathName_U_WithStatus( libname, nt_name, NULL, NULL )))
        status = open_dll_file( nt_name, pwm, module, image_info, st );

    if (status == STATUS_IMAGE_MACHINE_TYPE_MISMATCH) status = STATUS_INVALID_IMAGE_FORMAT;

done:
    RtlFreeHeap( GetProcessHeap(), 0, dllname );
    if (wow64_old_value) RtlWow64EnableFsRedirectionEx( 1, &wow64_old_value );

    if (status != STATUS_SUCCESS)
    {
        /* HACK for Proton issue #17
         *
         * Some games try to load mfc42.dll, but then proceed to not use it.
         * Just return a handle to kernel32 in that case.
         */
        static const WCHAR mfc42W[] = {'m','f','c','4','2',0};
        static const WCHAR kernel32W[] = {'k','e','r','n','e','l','3','2','.','d','l','l',0};
        const char *sgi = getenv( "SteamGameId" );
        if (sgi &&
            !strcmp( sgi, "105450" ) && /* AoE3 */
            strcasestrW( libname, mfc42W ))
        {
            WARN_(loaddll)( "Using a fake mfc42 handle\n" );
            status = find_dll_file( load_path, kernel32W, nt_name, pwm, mfc42W, module, image_info, st );
        }
    }
    return status;
}


/***********************************************************************
 *	load_dll  (internal)
 *
 * Load a PE style module according to the load order.
 * The loader_section must be locked while calling this function.
 */
static NTSTATUS load_dll( const WCHAR *load_path, const WCHAR *libname, const WCHAR *default_ext,
                          DWORD flags, WINE_MODREF** pwm )
{
    enum loadorder loadorder;
    WINE_MODREF *main_exe;
    UNICODE_STRING nt_name;
    struct stat st;
    void *module;
    pe_image_info_t image_info;
    NTSTATUS nts;

    TRACE( "looking for %s in %s\n", debugstr_w(libname), debugstr_w(load_path) );

    nts = find_dll_file( load_path, libname, default_ext, &nt_name, pwm, &module, &image_info, &st );

    if (*pwm)  /* found already loaded module */
    {
        if ((*pwm)->ldr.LoadCount != -1) (*pwm)->ldr.LoadCount++;

        TRACE("Found %s for %s at %p, count=%d\n",
              debugstr_w((*pwm)->ldr.FullDllName.Buffer), debugstr_w(libname),
              (*pwm)->ldr.DllBase, (*pwm)->ldr.LoadCount);
        RtlFreeUnicodeString( &nt_name );
        return STATUS_SUCCESS;
    }

    if (nts && nts != STATUS_DLL_NOT_FOUND && nts != STATUS_INVALID_IMAGE_NOT_MZ) goto done;

    main_exe = get_modref( NtCurrentTeb()->Peb->ImageBaseAddress );
    loadorder = get_load_order( main_exe ? main_exe->ldr.BaseDllName.Buffer : NULL, &nt_name );

    switch (nts)
    {
    case STATUS_INVALID_IMAGE_NOT_MZ:  /* not in PE format, maybe it's a .so file */
        switch (loadorder)
        {
        case LO_NATIVE:
        case LO_NATIVE_BUILTIN:
        case LO_BUILTIN:
        case LO_BUILTIN_NATIVE:
        case LO_DEFAULT:
            if (!load_so_dll( load_path, &nt_name, NULL, flags, pwm )) nts = STATUS_SUCCESS;
            break;
        default:
            nts = STATUS_DLL_NOT_FOUND;
            break;
        }
        break;

    case STATUS_SUCCESS:  /* valid PE file */
        if (image_info.image_flags & IMAGE_FLAGS_WineBuiltin)
        {
            switch (loadorder)
            {
            case LO_NATIVE_BUILTIN:
            case LO_BUILTIN:
            case LO_BUILTIN_NATIVE:
            case LO_DEFAULT:
                nts = load_builtin_dll( load_path, &nt_name, &module, flags, pwm );
                if (nts == STATUS_DLL_NOT_FOUND)
                    nts = load_native_dll( load_path, &nt_name, &module, &image_info, flags, pwm, &st );
                break;
            default:
                nts = STATUS_DLL_NOT_FOUND;
                break;
            }
            if (module) NtUnmapViewOfSection( NtCurrentProcess(), module );
            break;
        }
        if (!(image_info.image_flags & IMAGE_FLAGS_WineFakeDll))
        {
            switch (loadorder)
            {
            case LO_NATIVE:
            case LO_NATIVE_BUILTIN:
                nts = load_native_dll( load_path, &nt_name, &module, &image_info, flags, pwm, &st );
                break;
            case LO_BUILTIN:
                nts = load_builtin_dll( load_path, &nt_name, &module, flags, pwm );
                break;
            case LO_BUILTIN_NATIVE:
            case LO_DEFAULT:
                nts = load_builtin_dll( load_path, &nt_name, &module, flags, pwm );
                if (nts == STATUS_SUCCESS && loadorder == LO_DEFAULT &&
                    (MODULE_InitDLL( *pwm, DLL_WINE_PREATTACH, NULL ) != STATUS_SUCCESS))
                {
                    /* stub-only dll, try native */
                    TRACE( "%s pre-attach returned FALSE, preferring native\n", debugstr_us(&nt_name) );
                    LdrUnloadDll( (*pwm)->ldr.DllBase );
                    nts = STATUS_DLL_NOT_FOUND;
                    /* map the dll again if it was unmapped */
                    if (!module && open_dll_file( &nt_name, pwm, &module, &image_info, &st )) break;
                }
                if (nts == STATUS_DLL_NOT_FOUND)
                    nts = load_native_dll( load_path, &nt_name, &module, &image_info, flags, pwm, &st );
                break;
            default:
                nts = STATUS_DLL_NOT_FOUND;
                break;
            }
            if (module) NtUnmapViewOfSection( NtCurrentProcess(), module );
            break;
        }
        TRACE( "%s is a fake Wine dll\n", debugstr_us(&nt_name) );
        NtUnmapViewOfSection( NtCurrentProcess(), module );
        /* fall through */

    case STATUS_DLL_NOT_FOUND:  /* no file found, try builtin */
        switch (loadorder)
        {
        case LO_NATIVE_BUILTIN:
        case LO_BUILTIN:
        case LO_BUILTIN_NATIVE:
        case LO_DEFAULT:
            nts = load_builtin_dll( load_path, &nt_name, NULL, flags, pwm );
            break;
        default:
            nts = STATUS_DLL_NOT_FOUND;
            break;
        }
        break;
    }

done:
    if (nts == STATUS_SUCCESS)
        TRACE("Loaded module %s at %p\n", debugstr_us(&nt_name), (*pwm)->ldr.DllBase);
    else
        WARN("Failed to load module %s; status=%x\n", debugstr_w(libname), nts);

    RtlFreeUnicodeString( &nt_name );
    return nts;
}

/******************************************************************
 *		LdrLoadDll (NTDLL.@)
 */
NTSTATUS WINAPI DECLSPEC_HOTPATCH LdrLoadDll(LPCWSTR path_name, DWORD flags,
                                             const UNICODE_STRING *libname, HMODULE* hModule)
{
    WINE_MODREF *wm;
    NTSTATUS nts;

    RtlEnterCriticalSection( &loader_section );

    if (!path_name) path_name = NtCurrentTeb()->Peb->ProcessParameters->DllPath.Buffer;
    nts = load_dll( path_name, libname->Buffer, dllW, flags, &wm );

    if (nts == STATUS_SUCCESS && !(wm->ldr.Flags & LDR_DONT_RESOLVE_REFS))
    {
        nts = process_attach( wm, NULL );
        if (nts != STATUS_SUCCESS)
        {
            LdrUnloadDll(wm->ldr.DllBase);
            wm = NULL;
        }
    }
    *hModule = (wm) ? wm->ldr.DllBase : NULL;

    RtlLeaveCriticalSection( &loader_section );
    return nts;
}


/******************************************************************
 *		LdrGetDllHandle (NTDLL.@)
 */
NTSTATUS WINAPI LdrGetDllHandle( LPCWSTR load_path, ULONG flags, const UNICODE_STRING *name, HMODULE *base )
{
    NTSTATUS status;
    UNICODE_STRING nt_name;
    WINE_MODREF *wm;
    void *module;
    pe_image_info_t image_info;
    struct stat st;

    RtlEnterCriticalSection( &loader_section );

    if (!load_path) load_path = NtCurrentTeb()->Peb->ProcessParameters->DllPath.Buffer;

    status = find_dll_file( load_path, name->Buffer, dllW, &nt_name, &wm, &module, &image_info, &st );

    if (wm) *base = wm->ldr.DllBase;
    else
    {
        if (status == STATUS_SUCCESS) NtUnmapViewOfSection( NtCurrentProcess(), module );
        status = STATUS_DLL_NOT_FOUND;
    }
    RtlFreeUnicodeString( &nt_name );

    RtlLeaveCriticalSection( &loader_section );
    TRACE( "%s -> %p (load path %s)\n", debugstr_us(name), status ? NULL : *base, debugstr_w(load_path) );
    return status;
}


/******************************************************************
 *		LdrAddRefDll (NTDLL.@)
 */
NTSTATUS WINAPI LdrAddRefDll( ULONG flags, HMODULE module )
{
    NTSTATUS ret = STATUS_SUCCESS;
    WINE_MODREF *wm;

    if (flags & ~LDR_ADDREF_DLL_PIN) FIXME( "%p flags %x not implemented\n", module, flags );

    RtlEnterCriticalSection( &loader_section );

    if ((wm = get_modref( module )))
    {
        if (flags & LDR_ADDREF_DLL_PIN)
            wm->ldr.LoadCount = -1;
        else
            if (wm->ldr.LoadCount != -1) wm->ldr.LoadCount++;
        TRACE( "(%s) ldr.LoadCount: %d\n", debugstr_w(wm->ldr.BaseDllName.Buffer), wm->ldr.LoadCount );
    }
    else ret = STATUS_INVALID_PARAMETER;

    RtlLeaveCriticalSection( &loader_section );
    return ret;
}


/***********************************************************************
 *           LdrProcessRelocationBlock  (NTDLL.@)
 *
 * Apply relocations to a given page of a mapped PE image.
 */
IMAGE_BASE_RELOCATION * WINAPI LdrProcessRelocationBlock( void *page, UINT count,
                                                          USHORT *relocs, INT_PTR delta )
{
    while (count--)
    {
        USHORT offset = *relocs & 0xfff;
        int type = *relocs >> 12;
        switch(type)
        {
        case IMAGE_REL_BASED_ABSOLUTE:
            break;
        case IMAGE_REL_BASED_HIGH:
            *(short *)((char *)page + offset) += HIWORD(delta);
            break;
        case IMAGE_REL_BASED_LOW:
            *(short *)((char *)page + offset) += LOWORD(delta);
            break;
        case IMAGE_REL_BASED_HIGHLOW:
            *(int *)((char *)page + offset) += delta;
            break;
#ifdef _WIN64
        case IMAGE_REL_BASED_DIR64:
            *(INT_PTR *)((char *)page + offset) += delta;
            break;
#elif defined(__arm__)
        case IMAGE_REL_BASED_THUMB_MOV32:
        {
            DWORD inst = *(INT_PTR *)((char *)page + offset);
            DWORD imm16 = ((inst << 1) & 0x0800) + ((inst << 12) & 0xf000) +
                          ((inst >> 20) & 0x0700) + ((inst >> 16) & 0x00ff);
            DWORD hi_delta;

            if ((inst & 0x8000fbf0) != 0x0000f240)
                ERR("wrong Thumb2 instruction %08x, expected MOVW\n", inst);

            imm16 += LOWORD(delta);
            hi_delta = HIWORD(delta) + HIWORD(imm16);
            *(INT_PTR *)((char *)page + offset) = (inst & 0x8f00fbf0) + ((imm16 >> 1) & 0x0400) +
                                                  ((imm16 >> 12) & 0x000f) +
                                                  ((imm16 << 20) & 0x70000000) +
                                                  ((imm16 << 16) & 0xff0000);

            if (hi_delta != 0)
            {
                inst = *(INT_PTR *)((char *)page + offset + 4);
                imm16 = ((inst << 1) & 0x0800) + ((inst << 12) & 0xf000) +
                        ((inst >> 20) & 0x0700) + ((inst >> 16) & 0x00ff);

                if ((inst & 0x8000fbf0) != 0x0000f2c0)
                    ERR("wrong Thumb2 instruction %08x, expected MOVT\n", inst);

                imm16 += hi_delta;
                if (imm16 > 0xffff)
                    ERR("resulting immediate value won't fit: %08x\n", imm16);
                *(INT_PTR *)((char *)page + offset + 4) = (inst & 0x8f00fbf0) +
                                                          ((imm16 >> 1) & 0x0400) +
                                                          ((imm16 >> 12) & 0x000f) +
                                                          ((imm16 << 20) & 0x70000000) +
                                                          ((imm16 << 16) & 0xff0000);
            }
        }
            break;
#endif
        default:
            FIXME("Unknown/unsupported fixup type %x.\n", type);
            return NULL;
        }
        relocs++;
    }
    return (IMAGE_BASE_RELOCATION *)relocs;  /* return address of next block */
}


/******************************************************************
 *		LdrQueryProcessModuleInformation
 *
 */
NTSTATUS WINAPI LdrQueryProcessModuleInformation(PSYSTEM_MODULE_INFORMATION smi, 
                                                 ULONG buf_size, ULONG* req_size)
{
    SYSTEM_MODULE*      sm = &smi->Modules[0];
    ULONG               size = sizeof(ULONG);
    NTSTATUS            nts = STATUS_SUCCESS;
    ANSI_STRING         str;
    char*               ptr;
    PLIST_ENTRY         mark, entry;
    LDR_DATA_TABLE_ENTRY *mod;
    WORD id = 0;

    smi->ModulesCount = 0;

    RtlEnterCriticalSection( &loader_section );
    mark = &NtCurrentTeb()->Peb->LdrData->InLoadOrderModuleList;
    for (entry = mark->Flink; entry != mark; entry = entry->Flink)
    {
        mod = CONTAINING_RECORD(entry, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);
        size += sizeof(*sm);
        if (size <= buf_size)
        {
            sm->Section = 0; /* FIXME */
            sm->MappedBaseAddress = mod->DllBase;
            sm->ImageBaseAddress = mod->DllBase;
            sm->ImageSize = mod->SizeOfImage;
            sm->Flags = mod->Flags;
            sm->LoadOrderIndex = id++;
            sm->InitOrderIndex = 0; /* FIXME */
            sm->LoadCount = mod->LoadCount;
            str.Length = 0;
            str.MaximumLength = MAXIMUM_FILENAME_LENGTH;
            str.Buffer = (char*)sm->Name;
            RtlUnicodeStringToAnsiString(&str, &mod->FullDllName, FALSE);
            ptr = strrchr(str.Buffer, '\\');
            sm->NameOffset = (ptr != NULL) ? (ptr - str.Buffer + 1) : 0;

            smi->ModulesCount++;
            sm++;
        }
        else nts = STATUS_INFO_LENGTH_MISMATCH;
    }
    RtlLeaveCriticalSection( &loader_section );

    if (req_size) *req_size = size;

    return nts;
}


static NTSTATUS query_dword_option( HANDLE hkey, LPCWSTR name, ULONG *value )
{
    NTSTATUS status;
    UNICODE_STRING str;
    ULONG size;
    WCHAR buffer[64];
    KEY_VALUE_PARTIAL_INFORMATION *info = (KEY_VALUE_PARTIAL_INFORMATION *)buffer;

    RtlInitUnicodeString( &str, name );

    size = sizeof(buffer) - sizeof(WCHAR);
    if ((status = NtQueryValueKey( hkey, &str, KeyValuePartialInformation, buffer, size, &size )))
        return status;

    if (info->Type != REG_DWORD)
    {
        buffer[size / sizeof(WCHAR)] = 0;
        *value = wcstoul( (WCHAR *)info->Data, 0, 16 );
    }
    else memcpy( value, info->Data, sizeof(*value) );
    return status;
}

static NTSTATUS query_string_option( HANDLE hkey, LPCWSTR name, ULONG type,
                                     void *data, ULONG in_size, ULONG *out_size )
{
    NTSTATUS status;
    UNICODE_STRING str;
    ULONG size;
    char *buffer;
    KEY_VALUE_PARTIAL_INFORMATION *info;
    static const int info_size = FIELD_OFFSET( KEY_VALUE_PARTIAL_INFORMATION, Data );

    RtlInitUnicodeString( &str, name );

    size = info_size + in_size;
    if (!(buffer = RtlAllocateHeap( GetProcessHeap(), 0, size ))) return STATUS_NO_MEMORY;
    info = (KEY_VALUE_PARTIAL_INFORMATION *)buffer;
    status = NtQueryValueKey( hkey, &str, KeyValuePartialInformation, buffer, size, &size );
    if (!status || status == STATUS_BUFFER_OVERFLOW)
    {
        if (out_size) *out_size = info->DataLength;
        if (data && !status) memcpy( data, info->Data, info->DataLength );
    }
    RtlFreeHeap( GetProcessHeap(), 0, buffer );
    return status;
}


/******************************************************************
 *		LdrQueryImageFileExecutionOptions  (NTDLL.@)
 */
NTSTATUS WINAPI LdrQueryImageFileExecutionOptions( const UNICODE_STRING *key, LPCWSTR value, ULONG type,
                                                   void *data, ULONG in_size, ULONG *out_size )
{
    static const WCHAR optionsW[] = {'M','a','c','h','i','n','e','\\',
                                     'S','o','f','t','w','a','r','e','\\',
                                     'M','i','c','r','o','s','o','f','t','\\',
                                     'W','i','n','d','o','w','s',' ','N','T','\\',
                                     'C','u','r','r','e','n','t','V','e','r','s','i','o','n','\\',
                                     'I','m','a','g','e',' ','F','i','l','e',' ',
                                     'E','x','e','c','u','t','i','o','n',' ','O','p','t','i','o','n','s','\\'};
    WCHAR path[MAX_PATH + ARRAY_SIZE( optionsW )];
    OBJECT_ATTRIBUTES attr;
    UNICODE_STRING name_str;
    HANDLE hkey;
    NTSTATUS status;
    ULONG len;
    WCHAR *p;

    attr.Length = sizeof(attr);
    attr.RootDirectory = 0;
    attr.ObjectName = &name_str;
    attr.Attributes = OBJ_CASE_INSENSITIVE;
    attr.SecurityDescriptor = NULL;
    attr.SecurityQualityOfService = NULL;

    p = key->Buffer + key->Length / sizeof(WCHAR);
    while (p > key->Buffer && p[-1] != '\\') p--;
    len = key->Length - (p - key->Buffer) * sizeof(WCHAR);
    name_str.Buffer = path;
    name_str.Length = sizeof(optionsW) + len;
    name_str.MaximumLength = name_str.Length;
    memcpy( path, optionsW, sizeof(optionsW) );
    memcpy( path + ARRAY_SIZE( optionsW ), p, len );
    if ((status = NtOpenKey( &hkey, KEY_QUERY_VALUE, &attr ))) return status;

    if (type == REG_DWORD)
    {
        if (out_size) *out_size = sizeof(ULONG);
        if (in_size >= sizeof(ULONG)) status = query_dword_option( hkey, value, data );
        else status = STATUS_BUFFER_OVERFLOW;
    }
    else status = query_string_option( hkey, value, type, data, in_size, out_size );

    NtClose( hkey );
    return status;
}


/******************************************************************
 *		RtlDllShutdownInProgress  (NTDLL.@)
 */
BOOLEAN WINAPI RtlDllShutdownInProgress(void)
{
    return process_detaching;
}

/****************************************************************************
 *              LdrResolveDelayLoadedAPI   (NTDLL.@)
 */
void* WINAPI LdrResolveDelayLoadedAPI( void* base, const IMAGE_DELAYLOAD_DESCRIPTOR* desc,
                                       PDELAYLOAD_FAILURE_DLL_CALLBACK dllhook,
                                       PDELAYLOAD_FAILURE_SYSTEM_ROUTINE syshook,
                                       IMAGE_THUNK_DATA* addr, ULONG flags )
{
    IMAGE_THUNK_DATA *pIAT, *pINT;
    DELAYLOAD_INFO delayinfo;
    UNICODE_STRING mod;
    const CHAR* name;
    HMODULE *phmod;
    NTSTATUS nts;
    FARPROC fp;
    DWORD id;

    TRACE( "(%p, %p, %p, %p, %p, 0x%08x)\n", base, desc, dllhook, syshook, addr, flags );

    phmod = get_rva(base, desc->ModuleHandleRVA);
    pIAT = get_rva(base, desc->ImportAddressTableRVA);
    pINT = get_rva(base, desc->ImportNameTableRVA);
    name = get_rva(base, desc->DllNameRVA);
    id = addr - pIAT;

    if (!*phmod)
    {
        if (!RtlCreateUnicodeStringFromAsciiz(&mod, name))
        {
            nts = STATUS_NO_MEMORY;
            goto fail;
        }
        nts = LdrLoadDll(NULL, 0, &mod, phmod);
        RtlFreeUnicodeString(&mod);
        if (nts) goto fail;
    }

    if (IMAGE_SNAP_BY_ORDINAL(pINT[id].u1.Ordinal))
        nts = LdrGetProcedureAddress(*phmod, NULL, LOWORD(pINT[id].u1.Ordinal), (void**)&fp);
    else
    {
        const IMAGE_IMPORT_BY_NAME* iibn = get_rva(base, pINT[id].u1.AddressOfData);
        ANSI_STRING fnc;

        RtlInitAnsiString(&fnc, (char*)iibn->Name);
        nts = LdrGetProcedureAddress(*phmod, &fnc, 0, (void**)&fp);
    }
    if (!nts)
    {
        pIAT[id].u1.Function = (ULONG_PTR)fp;
        return fp;
    }

fail:
    delayinfo.Size = sizeof(delayinfo);
    delayinfo.DelayloadDescriptor = desc;
    delayinfo.ThunkAddress = addr;
    delayinfo.TargetDllName = name;
    delayinfo.TargetApiDescriptor.ImportDescribedByName = !IMAGE_SNAP_BY_ORDINAL(pINT[id].u1.Ordinal);
    delayinfo.TargetApiDescriptor.Description.Ordinal = LOWORD(pINT[id].u1.Ordinal);
    delayinfo.TargetModuleBase = *phmod;
    delayinfo.Unused = NULL;
    delayinfo.LastError = nts;

    if (dllhook)
        return dllhook(4, &delayinfo);

    if (IMAGE_SNAP_BY_ORDINAL(pINT[id].u1.Ordinal))
    {
        DWORD_PTR ord = LOWORD(pINT[id].u1.Ordinal);
        return syshook(name, (const char *)ord);
    }
    else
    {
        const IMAGE_IMPORT_BY_NAME* iibn = get_rva(base, pINT[id].u1.AddressOfData);
        return syshook(name, (const char *)iibn->Name);
    }
}

/******************************************************************
 *		LdrShutdownProcess (NTDLL.@)
 *
 */
void WINAPI LdrShutdownProcess(void)
{
    TRACE("()\n");

    lock_fls_section(NULL);
    call_fls_callbacks();
    unlock_fls_section(NULL);

    process_detaching = TRUE;
    process_detach();
}


/******************************************************************
 *		RtlExitUserProcess (NTDLL.@)
 */
void WINAPI RtlExitUserProcess( DWORD status )
{
    RtlEnterCriticalSection( &loader_section );
    RtlAcquirePebLock();
    NtTerminateProcess( 0, status );
    LdrShutdownProcess();
    NtTerminateProcess( GetCurrentProcess(), status );
    exit( get_unix_exit_code( status ));
}

/******************************************************************
 *		LdrShutdownThread (NTDLL.@)
 *
 */
void WINAPI LdrShutdownThread(void)
{
    PLIST_ENTRY mark, entry;
    LDR_DATA_TABLE_ENTRY *mod;
    WINE_MODREF *wm;
    UINT i;
    void **pointers;

    TRACE("()\n");

    /* don't do any detach calls if process is exiting */
    if (process_detaching) return;

    if (NtCurrentTeb()->FlsSlots)
    {
        lock_fls_section( NULL );
        RemoveEntryList( (LIST_ENTRY *)NtCurrentTeb()->FlsSlots );
        call_fls_callbacks();
        unlock_fls_section( NULL );
    }

    RtlEnterCriticalSection( &loader_section );
    wm = get_modref( NtCurrentTeb()->Peb->ImageBaseAddress );

    mark = &NtCurrentTeb()->Peb->LdrData->InInitializationOrderModuleList;
    for (entry = mark->Blink; entry != mark; entry = entry->Blink)
    {
        mod = CONTAINING_RECORD(entry, LDR_DATA_TABLE_ENTRY,
                                InInitializationOrderLinks);
        if ( !(mod->Flags & LDR_PROCESS_ATTACHED) )
            continue;
        if ( mod->Flags & LDR_NO_DLL_CALLS )
            continue;

        MODULE_InitDLL( CONTAINING_RECORD(mod, WINE_MODREF, ldr), 
                        DLL_THREAD_DETACH, NULL );
    }

    if (wm->ldr.TlsIndex != -1) call_tls_callbacks( wm->ldr.DllBase, DLL_THREAD_DETACH );

    RtlAcquirePebLock();
    RemoveEntryList( &NtCurrentTeb()->TlsLinks );
    RtlReleasePebLock();

    if ((pointers = NtCurrentTeb()->ThreadLocalStoragePointer))
    {
        for (i = 0; i < tls_module_count; i++) RtlFreeHeap( GetProcessHeap(), 0, pointers[i] );
        RtlFreeHeap( GetProcessHeap(), 0, pointers );
    }
    RtlFreeHeap( GetProcessHeap(), 0, NtCurrentTeb()->FlsSlots );
    RtlFreeHeap( GetProcessHeap(), 0, NtCurrentTeb()->TlsExpansionSlots );
    RtlLeaveCriticalSection( &loader_section );
}


/***********************************************************************
 *           free_modref
 *
 */
static void free_modref( WINE_MODREF *wm )
{
    RemoveEntryList(&wm->ldr.InLoadOrderLinks);
    RemoveEntryList(&wm->ldr.InMemoryOrderLinks);
    RemoveEntryList(&wm->ldr.HashLinks);
    if (wm->ldr.InInitializationOrderLinks.Flink)
        RemoveEntryList(&wm->ldr.InInitializationOrderLinks);

    TRACE(" unloading %s\n", debugstr_w(wm->ldr.FullDllName.Buffer));
    if (!TRACE_ON(module))
        TRACE_(loaddll)("Unloaded module %s : %s\n",
                        debugstr_w(wm->ldr.FullDllName.Buffer),
                        (wm->ldr.Flags & LDR_WINE_INTERNAL) ? "builtin" : "native" );

    SERVER_START_REQ( unload_dll )
    {
        req->base = wine_server_client_ptr( wm->ldr.DllBase );
        wine_server_call( req );
    }
    SERVER_END_REQ;

    free_tls_slot( &wm->ldr );
    RtlReleaseActivationContext( wm->ldr.ActivationContext );
    if (wm->so_handle) dlclose( wm->so_handle );
    NtUnmapViewOfSection( NtCurrentProcess(), wm->ldr.DllBase );
    if (cached_modref == wm) cached_modref = NULL;
    RtlFreeUnicodeString( &wm->ldr.FullDllName );
    RtlFreeHeap( GetProcessHeap(), 0, wm->deps );
    RtlFreeHeap( GetProcessHeap(), 0, wm );
}

/***********************************************************************
 *           MODULE_FlushModrefs
 *
 * Remove all unused modrefs and call the internal unloading routines
 * for the library type.
 *
 * The loader_section must be locked while calling this function.
 */
static void MODULE_FlushModrefs(void)
{
    PLIST_ENTRY mark, entry, prev;
    LDR_DATA_TABLE_ENTRY *mod;
    WINE_MODREF*wm;

    mark = &NtCurrentTeb()->Peb->LdrData->InInitializationOrderModuleList;
    for (entry = mark->Blink; entry != mark; entry = prev)
    {
        mod = CONTAINING_RECORD(entry, LDR_DATA_TABLE_ENTRY, InInitializationOrderLinks);
        wm = CONTAINING_RECORD(mod, WINE_MODREF, ldr);
        prev = entry->Blink;
        if (!mod->LoadCount) free_modref( wm );
    }

    /* check load order list too for modules that haven't been initialized yet */
    mark = &NtCurrentTeb()->Peb->LdrData->InLoadOrderModuleList;
    for (entry = mark->Blink; entry != mark; entry = prev)
    {
        mod = CONTAINING_RECORD(entry, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);
        wm = CONTAINING_RECORD(mod, WINE_MODREF, ldr);
        prev = entry->Blink;
        if (!mod->LoadCount) free_modref( wm );
    }
}

/***********************************************************************
 *           MODULE_DecRefCount
 *
 * The loader_section must be locked while calling this function.
 */
static void MODULE_DecRefCount( WINE_MODREF *wm )
{
    int i;

    if ( wm->ldr.Flags & LDR_UNLOAD_IN_PROGRESS )
        return;

    if ( wm->ldr.LoadCount <= 0 )
        return;

    --wm->ldr.LoadCount;
    TRACE("(%s) ldr.LoadCount: %d\n", debugstr_w(wm->ldr.BaseDllName.Buffer), wm->ldr.LoadCount );

    if ( wm->ldr.LoadCount == 0 )
    {
        wm->ldr.Flags |= LDR_UNLOAD_IN_PROGRESS;

        for ( i = 0; i < wm->nDeps; i++ )
            if ( wm->deps[i] )
                MODULE_DecRefCount( wm->deps[i] );

        wm->ldr.Flags &= ~LDR_UNLOAD_IN_PROGRESS;

        module_push_unload_trace( &wm->ldr );
    }
}

/******************************************************************
 *		LdrUnloadDll (NTDLL.@)
 *
 *
 */
NTSTATUS WINAPI LdrUnloadDll( HMODULE hModule )
{
    WINE_MODREF *wm;
    NTSTATUS retv = STATUS_SUCCESS;

    if (process_detaching) return retv;

    TRACE("(%p)\n", hModule);

    RtlEnterCriticalSection( &loader_section );

    free_lib_count++;
    if ((wm = get_modref( hModule )) != NULL)
    {
        TRACE("(%s) - START\n", debugstr_w(wm->ldr.BaseDllName.Buffer));

        /* Recursively decrement reference counts */
        MODULE_DecRefCount( wm );

        /* Call process detach notifications */
        if ( free_lib_count <= 1 )
        {
            process_detach();
            MODULE_FlushModrefs();
        }

        TRACE("END\n");
    }
    else
        retv = STATUS_DLL_NOT_FOUND;

    free_lib_count--;

    RtlLeaveCriticalSection( &loader_section );

    return retv;
}

/***********************************************************************
 *           RtlImageNtHeader   (NTDLL.@)
 */
PIMAGE_NT_HEADERS WINAPI RtlImageNtHeader(HMODULE hModule)
{
    IMAGE_NT_HEADERS *ret;

    __TRY
    {
        IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)hModule;

        ret = NULL;
        if (dos->e_magic == IMAGE_DOS_SIGNATURE)
        {
            ret = (IMAGE_NT_HEADERS *)((char *)dos + dos->e_lfanew);
            if (ret->Signature != IMAGE_NT_SIGNATURE) ret = NULL;
        }
    }
    __EXCEPT_PAGE_FAULT
    {
        return NULL;
    }
    __ENDTRY
    return ret;
}


/******************************************************************
 *		LdrInitializeThunk (NTDLL.@)
 *
 * Attach to all the loaded dlls.
 * If this is the first time, perform the full process initialization.
 */
void WINAPI LdrInitializeThunk( CONTEXT *context, void **entry, ULONG_PTR unknown3, ULONG_PTR unknown4 )
{
    static const unsigned int fls_slot_count = 8 * sizeof(NtCurrentTeb()->Peb->FlsBitmapBits);
    static const LARGE_INTEGER zero;
    static int attach_done;
    int i;
    NTSTATUS status;
    ULONG_PTR cookie;
    WINE_MODREF *wm;
    LPCWSTR load_path = NtCurrentTeb()->Peb->ProcessParameters->DllPath.Buffer;

    pthread_sigmask( SIG_UNBLOCK, &server_block_set, NULL );

    if (process_detaching) return;

    RtlEnterCriticalSection( &loader_section );

    wm = get_modref( NtCurrentTeb()->Peb->ImageBaseAddress );
    assert( wm );

    if (!imports_fixup_done)
    {
        actctx_init();
        if (wm->ldr.Flags & LDR_COR_ILONLY)
            status = fixup_imports_ilonly( wm, load_path, entry );
        else
            status = fixup_imports( wm, load_path );

        if (status)
        {
            ERR( "Importing dlls for %s failed, status %x\n",
                 debugstr_w(NtCurrentTeb()->Peb->ProcessParameters->ImagePathName.Buffer), status );
            NtTerminateProcess( GetCurrentProcess(), status );
        }
        imports_fixup_done = TRUE;
    }

    RtlAcquirePebLock();
    InsertHeadList( &tls_links, &NtCurrentTeb()->TlsLinks );
    RtlReleasePebLock();

    if (!NtCurrentTeb()->Peb->FlsCallback)
    {
        NtCurrentTeb()->Peb->FlsCallback = RtlAllocateHeap( GetProcessHeap(), HEAP_ZERO_MEMORY,
                 (fls_slot_count + 2) * sizeof(void*) );
        if (!NtCurrentTeb()->Peb->FlsCallback)
            ERR( "No memory for FLS callbacks.\n" );

        if (NtCurrentTeb()->Peb->FlsCallback)
        {
            NtCurrentTeb()->Peb->FlsCallback[0] = lock_fls_section;
            NtCurrentTeb()->Peb->FlsCallback[1] = unlock_fls_section;
        }
    }
    NtCurrentTeb()->FlsSlots = RtlAllocateHeap( GetProcessHeap(), HEAP_ZERO_MEMORY,
             sizeof(LIST_ENTRY) + fls_slot_count * sizeof(void*) );
    lock_fls_section( NULL );
    InsertTailList(&NtCurrentTeb()->Peb->FlsListHead, (LIST_ENTRY *)NtCurrentTeb()->FlsSlots);
    unlock_fls_section( NULL );

    if (!attach_done)  /* first time around */
    {
        attach_done = 1;
        if ((status = alloc_thread_tls()) != STATUS_SUCCESS)
        {
            ERR( "TLS init  failed when loading %s, status %x\n",
                 debugstr_w(NtCurrentTeb()->Peb->ProcessParameters->ImagePathName.Buffer), status );
            NtTerminateProcess( GetCurrentProcess(), status );
        }
        wm->ldr.LoadCount = -1;
        wm->ldr.Flags |= LDR_PROCESS_ATTACHED;  /* don't try to attach again */
        if (wm->ldr.ActivationContext)
            RtlActivateActivationContext( 0, wm->ldr.ActivationContext, &cookie );

        for (i = 0; i < wm->nDeps; i++)
        {
            if (!wm->deps[i]) continue;
            if ((status = process_attach( wm->deps[i], context )) != STATUS_SUCCESS)
            {
                if (last_failed_modref)
                    ERR( "%s failed to initialize, aborting\n",
                         debugstr_w(last_failed_modref->ldr.BaseDllName.Buffer) + 1 );
                ERR( "Initializing dlls for %s failed, status %x\n",
                     debugstr_w(NtCurrentTeb()->Peb->ProcessParameters->ImagePathName.Buffer), status );
                NtTerminateProcess( GetCurrentProcess(), status );
            }
        }
        attach_implicitly_loaded_dlls( context );
        virtual_release_address_space();
        if (wm->ldr.TlsIndex != -1) call_tls_callbacks( wm->ldr.DllBase, DLL_PROCESS_ATTACH );
        if (wm->so_handle) call_constructors( wm );
        if (wm->ldr.ActivationContext) RtlDeactivateActivationContext( 0, cookie );
    }
    else
    {
        if ((status = alloc_thread_tls()) != STATUS_SUCCESS)
            NtTerminateThread( GetCurrentThread(), status );
        thread_attach();
        if (wm->ldr.TlsIndex != -1) call_tls_callbacks( wm->ldr.DllBase, DLL_THREAD_ATTACH );
    }

    RtlLeaveCriticalSection( &loader_section );

    NtDelayExecution( TRUE, &zero );
}


/***********************************************************************
 *           elevate_process
 */
static void elevate_process( void )
{
    NTSTATUS status;
    HANDLE token;

    if (!(token = __wine_create_default_token( TRUE )))
    {
        ERR( "Failed to create admin token\n" );
        return;
    }

    SERVER_START_REQ( replace_process_token )
    {
        req->token = wine_server_obj_handle( token );
        if ((status = wine_server_call( req )))
            ERR( "Failed to replace process token: %08x\n", status );
    }
    SERVER_END_REQ;

    NtClose( token );
}


/***********************************************************************
 *           load_global_options
 */
static void load_global_options(void)
{
    static const WCHAR sessionW[] = {'M','a','c','h','i','n','e','\\',
                                     'S','y','s','t','e','m','\\',
                                     'C','u','r','r','e','n','t','C','o','n','t','r','o','l','S','e','t','\\',
                                     'C','o','n','t','r','o','l','\\',
                                     'S','e','s','s','i','o','n',' ','M','a','n','a','g','e','r',0};
    static const WCHAR globalflagW[] = {'G','l','o','b','a','l','F','l','a','g',0};
    static const WCHAR safesearchW[] = {'S','a','f','e','P','r','o','c','e','s','s','S','e','a','r','c','h','M','o','d','e',0};
    static const WCHAR safedllmodeW[] = {'S','a','f','e','D','l','l','S','e','a','r','c','h','M','o','d','e',0};
    static const WCHAR critsectW[] = {'C','r','i','t','i','c','a','l','S','e','c','t','i','o','n','T','i','m','e','o','u','t',0};
    static const WCHAR heapresW[] = {'H','e','a','p','S','e','g','m','e','n','t','R','e','s','e','r','v','e',0};
    static const WCHAR heapcommitW[] = {'H','e','a','p','S','e','g','m','e','n','t','C','o','m','m','i','t',0};
    static const WCHAR decommittotalW[] = {'H','e','a','p','D','e','C','o','m','m','i','t','T','o','t','a','l','F','r','e','e','T','h','r','e','s','h','o','l','d',0};
    static const WCHAR decommitfreeW[] = {'H','e','a','p','D','e','C','o','m','m','i','t','F','r','e','e','B','l','o','c','k','T','h','r','e','s','h','o','l','d',0};

    OBJECT_ATTRIBUTES attr;
    UNICODE_STRING name_str;
    HANDLE hkey;
    ULONG value;

    attr.Length = sizeof(attr);
    attr.RootDirectory = 0;
    attr.ObjectName = &name_str;
    attr.Attributes = OBJ_CASE_INSENSITIVE;
    attr.SecurityDescriptor = NULL;
    attr.SecurityQualityOfService = NULL;
    RtlInitUnicodeString( &name_str, sessionW );

    if (!NtOpenKey( &hkey, KEY_QUERY_VALUE, &attr ))
    {
        query_dword_option( hkey, globalflagW, &NtCurrentTeb()->Peb->NtGlobalFlag );
        query_dword_option( hkey, safesearchW, &path_safe_mode );
        query_dword_option( hkey, safedllmodeW, &dll_safe_mode );

        if (!query_dword_option( hkey, critsectW, &value ))
            NtCurrentTeb()->Peb->CriticalSectionTimeout.QuadPart = (ULONGLONG)value * -10000000;

        if (!query_dword_option( hkey, heapresW, &value ))
            NtCurrentTeb()->Peb->HeapSegmentReserve = value;

        if (!query_dword_option( hkey, heapcommitW, &value ))
            NtCurrentTeb()->Peb->HeapSegmentCommit = value;

        if (!query_dword_option( hkey, decommittotalW, &value ))
            NtCurrentTeb()->Peb->HeapDeCommitTotalFreeThreshold = value;

        if (!query_dword_option( hkey, decommitfreeW, &value ))
            NtCurrentTeb()->Peb->HeapDeCommitFreeBlockThreshold = value;

        NtClose( hkey );
    }
    LdrQueryImageFileExecutionOptions( &NtCurrentTeb()->Peb->ProcessParameters->ImagePathName,
                                       globalflagW, REG_DWORD, &NtCurrentTeb()->Peb->NtGlobalFlag,
                                       sizeof(DWORD), NULL );
    heap_set_debug_flags( GetProcessHeap() );
}


/***********************************************************************
 *           RtlImageDirectoryEntryToData   (NTDLL.@)
 */
PVOID WINAPI RtlImageDirectoryEntryToData( HMODULE module, BOOL image, WORD dir, ULONG *size )
{
    const IMAGE_NT_HEADERS *nt;
    DWORD addr;

    if ((ULONG_PTR)module & 1) image = FALSE;  /* mapped as data file */
    module = (HMODULE)((ULONG_PTR)module & ~3);
    if (!(nt = RtlImageNtHeader( module ))) return NULL;
    if (nt->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
    {
        const IMAGE_NT_HEADERS64 *nt64 = (const IMAGE_NT_HEADERS64 *)nt;

        if (dir >= nt64->OptionalHeader.NumberOfRvaAndSizes) return NULL;
        if (!(addr = nt64->OptionalHeader.DataDirectory[dir].VirtualAddress)) return NULL;
        *size = nt64->OptionalHeader.DataDirectory[dir].Size;
        if (image || addr < nt64->OptionalHeader.SizeOfHeaders) return (char *)module + addr;
    }
    else if (nt->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC)
    {
        const IMAGE_NT_HEADERS32 *nt32 = (const IMAGE_NT_HEADERS32 *)nt;

        if (dir >= nt32->OptionalHeader.NumberOfRvaAndSizes) return NULL;
        if (!(addr = nt32->OptionalHeader.DataDirectory[dir].VirtualAddress)) return NULL;
        *size = nt32->OptionalHeader.DataDirectory[dir].Size;
        if (image || addr < nt32->OptionalHeader.SizeOfHeaders) return (char *)module + addr;
    }
    else return NULL;

    /* not mapped as image, need to find the section containing the virtual address */
    return RtlImageRvaToVa( nt, module, addr, NULL );
}


/***********************************************************************
 *           RtlImageRvaToSection   (NTDLL.@)
 */
PIMAGE_SECTION_HEADER WINAPI RtlImageRvaToSection( const IMAGE_NT_HEADERS *nt,
                                                   HMODULE module, DWORD rva )
{
    int i;
    const IMAGE_SECTION_HEADER *sec;

    sec = (const IMAGE_SECTION_HEADER*)((const char*)&nt->OptionalHeader +
                                        nt->FileHeader.SizeOfOptionalHeader);
    for (i = 0; i < nt->FileHeader.NumberOfSections; i++, sec++)
    {
        if ((sec->VirtualAddress <= rva) && (sec->VirtualAddress + sec->SizeOfRawData > rva))
            return (PIMAGE_SECTION_HEADER)sec;
    }
    return NULL;
}


/***********************************************************************
 *           RtlImageRvaToVa   (NTDLL.@)
 */
PVOID WINAPI RtlImageRvaToVa( const IMAGE_NT_HEADERS *nt, HMODULE module,
                              DWORD rva, IMAGE_SECTION_HEADER **section )
{
    IMAGE_SECTION_HEADER *sec;

    if (section && *section)  /* try this section first */
    {
        sec = *section;
        if ((sec->VirtualAddress <= rva) && (sec->VirtualAddress + sec->SizeOfRawData > rva))
            goto found;
    }
    if (!(sec = RtlImageRvaToSection( nt, module, rva ))) return NULL;
 found:
    if (section) *section = sec;
    return (char *)module + sec->PointerToRawData + (rva - sec->VirtualAddress);
}


/***********************************************************************
 *           RtlPcToFileHeader   (NTDLL.@)
 */
PVOID WINAPI RtlPcToFileHeader( PVOID pc, PVOID *address )
{
    LDR_DATA_TABLE_ENTRY *module;
    PVOID ret = NULL;

    RtlEnterCriticalSection( &loader_section );
    if (!LdrFindEntryForAddress( pc, &module )) ret = module->DllBase;
    RtlLeaveCriticalSection( &loader_section );
    *address = ret;
    return ret;
}


/****************************************************************************
 *		LdrGetDllDirectory  (NTDLL.@)
 */
NTSTATUS WINAPI LdrGetDllDirectory( UNICODE_STRING *dir )
{
    NTSTATUS status = STATUS_SUCCESS;

    RtlEnterCriticalSection( &dlldir_section );
    dir->Length = dll_directory.Length + sizeof(WCHAR);
    if (dir->MaximumLength >= dir->Length) RtlCopyUnicodeString( dir, &dll_directory );
    else
    {
        status = STATUS_BUFFER_TOO_SMALL;
        if (dir->MaximumLength) dir->Buffer[0] = 0;
    }
    RtlLeaveCriticalSection( &dlldir_section );
    return status;
}


/****************************************************************************
 *		LdrSetDllDirectory  (NTDLL.@)
 */
NTSTATUS WINAPI LdrSetDllDirectory( const UNICODE_STRING *dir )
{
    NTSTATUS status = STATUS_SUCCESS;
    UNICODE_STRING new;

    if (!dir->Buffer) RtlInitUnicodeString( &new, NULL );
    else if ((status = RtlDuplicateUnicodeString( 1, dir, &new ))) return status;

    RtlEnterCriticalSection( &dlldir_section );
    RtlFreeUnicodeString( &dll_directory );
    dll_directory = new;
    RtlLeaveCriticalSection( &dlldir_section );
    return status;
}


/****************************************************************************
 *		LdrAddDllDirectory  (NTDLL.@)
 */
NTSTATUS WINAPI LdrAddDllDirectory( const UNICODE_STRING *dir, void **cookie )
{
    FILE_BASIC_INFORMATION info;
    UNICODE_STRING nt_name;
    NTSTATUS status;
    OBJECT_ATTRIBUTES attr;
    DWORD len;
    struct dll_dir_entry *ptr;
    DOS_PATHNAME_TYPE type = RtlDetermineDosPathNameType_U( dir->Buffer );

    if (type != ABSOLUTE_PATH && type != ABSOLUTE_DRIVE_PATH)
        return STATUS_INVALID_PARAMETER;

    status = RtlDosPathNameToNtPathName_U_WithStatus( dir->Buffer, &nt_name, NULL, NULL );
    if (status) return status;
    len = nt_name.Length / sizeof(WCHAR);
    if (!(ptr = RtlAllocateHeap( GetProcessHeap(), 0, offsetof(struct dll_dir_entry, dir[++len] ))))
        return STATUS_NO_MEMORY;
    memcpy( ptr->dir, nt_name.Buffer, len * sizeof(WCHAR) );

    attr.Length = sizeof(attr);
    attr.RootDirectory = 0;
    attr.Attributes = OBJ_CASE_INSENSITIVE;
    attr.ObjectName = &nt_name;
    attr.SecurityDescriptor = NULL;
    attr.SecurityQualityOfService = NULL;
    status = NtQueryAttributesFile( &attr, &info );
    RtlFreeUnicodeString( &nt_name );

    if (!status)
    {
        TRACE( "%s\n", debugstr_w( ptr->dir ));
        RtlEnterCriticalSection( &dlldir_section );
        list_add_head( &dll_dir_list, &ptr->entry );
        RtlLeaveCriticalSection( &dlldir_section );
        *cookie = ptr;
    }
    else RtlFreeHeap( GetProcessHeap(), 0, ptr );
    return status;
}


/****************************************************************************
 *		LdrRemoveDllDirectory  (NTDLL.@)
 */
NTSTATUS WINAPI LdrRemoveDllDirectory( void *cookie )
{
    struct dll_dir_entry *ptr = cookie;

    TRACE( "%s\n", debugstr_w( ptr->dir ));

    RtlEnterCriticalSection( &dlldir_section );
    list_remove( &ptr->entry );
    RtlFreeHeap( GetProcessHeap(), 0, ptr );
    RtlLeaveCriticalSection( &dlldir_section );
    return STATUS_SUCCESS;
}


/*************************************************************************
 *		LdrSetDefaultDllDirectories  (NTDLL.@)
 */
NTSTATUS WINAPI LdrSetDefaultDllDirectories( ULONG flags )
{
    /* LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR doesn't make sense in default dirs */
    const ULONG load_library_search_flags = (LOAD_LIBRARY_SEARCH_APPLICATION_DIR |
                                             LOAD_LIBRARY_SEARCH_USER_DIRS |
                                             LOAD_LIBRARY_SEARCH_SYSTEM32 |
                                             LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);

    if (!flags || (flags & ~load_library_search_flags)) return STATUS_INVALID_PARAMETER;
    default_search_flags = flags;
    return STATUS_SUCCESS;
}


/******************************************************************
 *		LdrGetDllPath  (NTDLL.@)
 */
NTSTATUS WINAPI LdrGetDllPath( PCWSTR module, ULONG flags, PWSTR *path, PWSTR *unknown )
{
    NTSTATUS status;
    const ULONG load_library_search_flags = (LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR |
                                             LOAD_LIBRARY_SEARCH_APPLICATION_DIR |
                                             LOAD_LIBRARY_SEARCH_USER_DIRS |
                                             LOAD_LIBRARY_SEARCH_SYSTEM32 |
                                             LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);

    if (flags & LOAD_WITH_ALTERED_SEARCH_PATH)
    {
        if (flags & load_library_search_flags) return STATUS_INVALID_PARAMETER;
        if (default_search_flags) flags |= default_search_flags | LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR;
    }
    else if (!(flags & load_library_search_flags)) flags |= default_search_flags;

    RtlEnterCriticalSection( &dlldir_section );

    if (flags & load_library_search_flags)
    {
        status = get_dll_load_path_search_flags( module, flags, path );
    }
    else
    {
        const WCHAR *dlldir = dll_directory.Length ? dll_directory.Buffer : NULL;
        if (!(flags & LOAD_WITH_ALTERED_SEARCH_PATH))
            module = NtCurrentTeb()->Peb->ProcessParameters->ImagePathName.Buffer;
        status = get_dll_load_path( module, dlldir, dll_safe_mode, path );
    }

    RtlLeaveCriticalSection( &dlldir_section );
    *unknown = NULL;
    return status;
}


/*************************************************************************
 *		RtlSetSearchPathMode (NTDLL.@)
 */
NTSTATUS WINAPI RtlSetSearchPathMode( ULONG flags )
{
    int val;

    switch (flags)
    {
    case BASE_SEARCH_PATH_ENABLE_SAFE_SEARCHMODE:
        val = 1;
        break;
    case BASE_SEARCH_PATH_DISABLE_SAFE_SEARCHMODE:
        val = 0;
        break;
    case BASE_SEARCH_PATH_ENABLE_SAFE_SEARCHMODE | BASE_SEARCH_PATH_PERMANENT:
        InterlockedExchange( (int *)&path_safe_mode, 2 );
        return STATUS_SUCCESS;
    default:
        return STATUS_INVALID_PARAMETER;
    }

    for (;;)
    {
        int prev = path_safe_mode;
        if (prev == 2) break;  /* permanently set */
        if (InterlockedCompareExchange( (int *)&path_safe_mode, val, prev ) == prev) return STATUS_SUCCESS;
    }
    return STATUS_ACCESS_DENIED;
}


/******************************************************************
 *           RtlGetExePath   (NTDLL.@)
 */
NTSTATUS WINAPI RtlGetExePath( PCWSTR name, PWSTR *path )
{
    static const WCHAR emptyW[1];
    const WCHAR *dlldir = dotW;
    const WCHAR *module = NtCurrentTeb()->Peb->ProcessParameters->ImagePathName.Buffer;

    /* same check as NeedCurrentDirectoryForExePathW */
    if (!wcschr( name, '\\' ))
    {
        static const WCHAR env_name[] = {'N','o','D','e','f','a','u','l','t','C','u','r','r','e','n','t',
                                         'D','i','r','e','c','t','o','r','y','I','n',
                                         'E','x','e','P','a','t','h',0};
        UNICODE_STRING name, value = { 0 };

        RtlInitUnicodeString( &name, env_name );
        if (RtlQueryEnvironmentVariable_U( NULL, &name, &value ) != STATUS_VARIABLE_NOT_FOUND)
            dlldir = emptyW;
    }
    return get_dll_load_path( module, dlldir, FALSE, path );
}


/******************************************************************
 *           RtlGetSearchPath   (NTDLL.@)
 */
NTSTATUS WINAPI RtlGetSearchPath( PWSTR *path )
{
    const WCHAR *module = NtCurrentTeb()->Peb->ProcessParameters->ImagePathName.Buffer;
    return get_dll_load_path( module, NULL, path_safe_mode, path );
}


/******************************************************************
 *           RtlReleasePath   (NTDLL.@)
 */
void WINAPI RtlReleasePath( PWSTR path )
{
    RtlFreeHeap( GetProcessHeap(), 0, path );
}


/***********************************************************************
 *           NtLoadDriver   (NTDLL.@)
 *           ZwLoadDriver   (NTDLL.@)
 */
NTSTATUS WINAPI NtLoadDriver( const UNICODE_STRING *DriverServiceName )
{
    FIXME("(%p), stub!\n",DriverServiceName);
    return STATUS_NOT_IMPLEMENTED;
}

void *Wow64Transition;

/***********************************************************************
 *           NtUnloadDriver   (NTDLL.@)
 *           ZwUnloadDriver   (NTDLL.@)
 */
NTSTATUS WINAPI NtUnloadDriver( const UNICODE_STRING *DriverServiceName )
{
    FIXME("(%p), stub!\n",DriverServiceName);
    return STATUS_NOT_IMPLEMENTED;
}


/******************************************************************
 *		DllMain   (NTDLL.@)
 */
BOOL WINAPI DllMain( HINSTANCE inst, DWORD reason, LPVOID reserved )
{
    if (reason == DLL_PROCESS_ATTACH) LdrDisableThreadCalloutsForDll( inst );
    return TRUE;
}


static NTSTATUS load_ntdll_so( HMODULE module, const IMAGE_NT_HEADERS *nt )
{
    NTSTATUS (__cdecl *init_func)( HMODULE module, const void *ptr_in, void *ptr_out );
    Dl_info info;
    char *name;
    void *handle;

    if (!dladdr( load_ntdll_so, &info ))
    {
        fprintf( stderr, "cannot get path to ntdll.dll.so\n" );
        exit(1);
    }
    name = strdup( info.dli_fname );
    strcpy( name + strlen(name) - strlen(".dll.so"), ".so" );
    if (!(handle = dlopen( name, RTLD_NOW )))
    {
        fprintf( stderr, "failed to load %s: %s\n", name, dlerror() );
        exit(1);
    }
    if (!(init_func = dlsym( handle, "__wine_init_unix_lib" )))
    {
        fprintf( stderr, "init func not found in %s\n", name );
        exit(1);
    }
    return init_func( module, nt, &unix_funcs );
}

/***********************************************************************
 *           __wine_process_init
 */
void __wine_process_init(void)
{
    extern IMAGE_NT_HEADERS __wine_spec_nt_header;
    static const WCHAR ntdllW[] = {'\\','?','?','\\','C',':','\\','w','i','n','d','o','w','s','\\',
                                   's','y','s','t','e','m','3','2','\\',
                                   'n','t','d','l','l','.','d','l','l',0};
    static const WCHAR wow64cpuW[] = {'w','o','w','6','4','c','p','u','.','d','l','l',0};
    static const WCHAR kernel32W[] = {'\\','?','?','\\','C',':','\\','w','i','n','d','o','w','s','\\',
                                      's','y','s','t','e','m','3','2','\\',
                                      'k','e','r','n','e','l','3','2','.','d','l','l',0};
    RTL_USER_PROCESS_PARAMETERS *params;
    ACTIVATION_CONTEXT_RUN_LEVEL_INFORMATION runlevel;
    WINE_MODREF *wm, *wow64cpu_wm;
    NTSTATUS status;
    ANSI_STRING func_name;
    UNICODE_STRING nt_name;
    HMODULE ntdll_module = (HMODULE)((__wine_spec_nt_header.OptionalHeader.ImageBase + 0xffff) & ~0xffff);
    INITIAL_TEB stack;
    BOOL suspend;
    SIZE_T info_size;
    TEB *teb;
    PEB *peb;
    DWORD i;

    if (!unix_funcs) load_ntdll_so( ntdll_module, &__wine_spec_nt_header );

    teb = thread_init();
    peb = teb->Peb;

    /* setup the server connection */
    server_init_process();
    info_size = server_init_thread( peb, &suspend );

    peb->ProcessHeap = RtlCreateHeap( HEAP_GROWABLE, NULL, 0, 0, NULL, NULL );
    peb->LoaderLock = &loader_section;

    fsync_init();
    esync_init();

    init_unix_codepage();
    init_directories();
    init_user_process_params( info_size );
    params = peb->ProcessParameters;

    NtCreateKeyedEvent( &keyed_event, GENERIC_READ | GENERIC_WRITE, NULL, 0 );

    /* retrieve current umask */
    FILE_umask = umask(0777);
    umask( FILE_umask );

    load_global_options();
    version_init();

    /* initialize hash table */
    for (i = 0; i < HASH_MAP_SIZE; i++)
        InitializeListHead(&hash_table[i]);

    /* setup the load callback and create ntdll modref */
    RtlInitUnicodeString( &nt_name, ntdllW );
    status = build_so_dll_module( params->DllPath.Buffer, &nt_name, ntdll_module, 0, &wm );
    assert( !status );

    wine_dll_set_callback( load_builtin_callback );

    RtlInitUnicodeString( &nt_name, kernel32W );
    if ((status = load_builtin_dll( params->DllPath.Buffer, &nt_name, NULL, 0, &wm )) != STATUS_SUCCESS)
    {
        MESSAGE( "wine: could not load kernel32.dll, status %x\n", status );
        exit(1);
    }

    RtlInitUnicodeString( &nt_name, wow64cpuW );
    if ((status = load_builtin_dll( NULL, &nt_name, 0, 0, &wow64cpu_wm )) == STATUS_SUCCESS)
        Wow64Transition = wow64cpu_wm->ldr.DllBase;
    else
        WARN( "could not load wow64cpu.dll, status %#x\n", status );

    RtlInitAnsiString( &func_name, "__wine_start_process" );
    if ((status = LdrGetProcedureAddress( wm->ldr.DllBase, &func_name,
                                          0, (void **)&kernel32_start_process )) != STATUS_SUCCESS)
    {
        MESSAGE( "wine: could not find __wine_start_process in kernel32.dll, status %x\n", status );
        exit(1);
    }

    init_locale( wm->ldr.DllBase );

    if (!(status = load_dll( params->DllPath.Buffer, params->ImagePathName.Buffer, NULL,
                             DONT_RESOLVE_DLL_REFERENCES, &wm )))
    {
        peb->ImageBaseAddress = wm->ldr.DllBase;
        TRACE( "main exe loaded %s at %p\n", debugstr_us(&params->ImagePathName), peb->ImageBaseAddress );
        if (wm->ldr.Flags & LDR_IMAGE_IS_DLL)
        {
            MESSAGE( "wine: %s is a dll, not an executable\n", debugstr_w(wm->ldr.FullDllName.Buffer) );
            NtTerminateProcess( GetCurrentProcess(), STATUS_INVALID_IMAGE_FORMAT );
        }
    }
    else
    {
        if (!info_size) status = restart_process( params, status );
        switch (status)
        {
        case STATUS_INVALID_IMAGE_WIN_64:
            ERR( "%s 64-bit application not supported in 32-bit prefix\n",
                 debugstr_us(&params->ImagePathName) );
            break;
        case STATUS_INVALID_IMAGE_WIN_16:
        case STATUS_INVALID_IMAGE_NE_FORMAT:
        case STATUS_INVALID_IMAGE_PROTECT:
            ERR( "%s 16-bit application not supported on this system\n",
                 debugstr_us(&params->ImagePathName) );
            break;
        case STATUS_INVALID_IMAGE_FORMAT:
            ERR( "%s not supported on this system\n", debugstr_us(&params->ImagePathName) );
            break;
        case STATUS_DLL_NOT_FOUND:
            ERR( "%s not found\n", debugstr_us(&params->ImagePathName) );
            break;
        default:
            ERR( "failed to load %s, error %x\n", debugstr_us(&params->ImagePathName), status );
            break;
        }
        NtTerminateProcess( GetCurrentProcess(), status );
    }

    hidden_exports_init( wm->ldr.FullDllName.Buffer );

    virtual_set_large_address_space(needs_override_large_address_aware(NtCurrentTeb()->Peb->ProcessParameters->ImagePathName.Buffer));

    /* elevate process if necessary */
    status = RtlQueryInformationActivationContext( 0, NULL, 0, RunlevelInformationInActivationContext,
                                                   &runlevel, sizeof(runlevel), NULL );
    if (!status && (runlevel.RunLevel == ACTCTX_RUN_LEVEL_HIGHEST_AVAILABLE ||
                    runlevel.RunLevel == ACTCTX_RUN_LEVEL_REQUIRE_ADMIN))
    {
        TRACE( "Application requested admin rights (run level %d)\n", runlevel.RunLevel );
        elevate_process();  /* FIXME: the process exists with a wrong token for a short time */
    }

    /* the main exe needs to be the first in the load order list */
    RemoveEntryList( &wm->ldr.InLoadOrderLinks );
    InsertHeadList( &peb->LdrData->InLoadOrderModuleList, &wm->ldr.InLoadOrderLinks );
    RemoveEntryList( &wm->ldr.InMemoryOrderLinks );
    InsertHeadList( &peb->LdrData->InMemoryOrderModuleList, &wm->ldr.InMemoryOrderLinks );

    virtual_alloc_thread_stack( &stack, 0, 0, NULL );
    teb->Tib.StackBase = stack.StackBase;
    teb->Tib.StackLimit = stack.StackLimit;
    teb->DeallocationStack = stack.DeallocationStack;

    /* the windows version was not set yet when ntdll and kernel32 were loaded */
    recompute_hash_map();

    server_init_process_done();
}

/***********************************************************************
 *           __wine_set_unix_funcs
 */
void CDECL __wine_set_unix_funcs( int version, const struct unix_funcs *funcs )
{
    assert( version == NTDLL_UNIXLIB_VERSION );
    unix_funcs = funcs;
    __wine_process_init();
}
