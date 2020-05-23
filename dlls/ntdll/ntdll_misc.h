/*
 * Copyright 2000 Juergen Schmied
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

#ifndef __WINE_NTDLL_MISC_H
#define __WINE_NTDLL_MISC_H

#include <stdarg.h>
#include <signal.h>
#include <sys/types.h>
#include <pthread.h>

#include "windef.h"
#include "winnt.h"
#include "winternl.h"
#include "unixlib.h"
#include "wine/debug.h"
#include "wine/server.h"
#include "wine/asm.h"

#define DECLARE_CRITICAL_SECTION(cs) \
    static RTL_CRITICAL_SECTION cs; \
    static RTL_CRITICAL_SECTION_DEBUG cs##_debug = \
    { 0, 0, &cs, { &cs##_debug.ProcessLocksList, &cs##_debug.ProcessLocksList }, \
      0, 0, { (DWORD_PTR)(__FILE__ ": " # cs) }}; \
    static RTL_CRITICAL_SECTION cs = { &cs##_debug, -1, 0, 0, 0, 0 };

#define MAX_NT_PATH_LENGTH 277

#define MAX_DOS_DRIVES 26

#if defined(__i386__) || defined(__x86_64__) || defined(__arm__) || defined(__aarch64__)
static const UINT_PTR page_size = 0x1000;
#else
extern UINT_PTR page_size DECLSPEC_HIDDEN;
#endif

struct drive_info
{
    dev_t dev;
    ino_t ino;
};

extern NTSTATUS close_handle( HANDLE ) DECLSPEC_HIDDEN;
extern ULONG_PTR get_system_affinity_mask(void) DECLSPEC_HIDDEN;

/* exceptions */
extern void wait_suspend( CONTEXT *context ) DECLSPEC_HIDDEN;
extern NTSTATUS send_debug_event( EXCEPTION_RECORD *rec, int first_chance, CONTEXT *context ) DECLSPEC_HIDDEN;
extern LONG call_vectored_handlers( EXCEPTION_RECORD *rec, CONTEXT *context ) DECLSPEC_HIDDEN;
extern void DECLSPEC_NORETURN raise_status( NTSTATUS status, EXCEPTION_RECORD *rec ) DECLSPEC_HIDDEN;
extern NTSTATUS context_to_server( context_t *to, const CONTEXT *from ) DECLSPEC_HIDDEN;
extern NTSTATUS context_from_server( CONTEXT *to, const context_t *from ) DECLSPEC_HIDDEN;
extern NTSTATUS set_thread_context( HANDLE handle, const context_t *context, BOOL *self ) DECLSPEC_HIDDEN;
extern NTSTATUS get_thread_context( HANDLE handle, context_t *context, unsigned int flags, BOOL *self ) DECLSPEC_HIDDEN;
extern NTSTATUS get_thread_ldt_entry( HANDLE handle, void *data, ULONG len, ULONG *ret_len ) DECLSPEC_HIDDEN;
extern LONG WINAPI call_unhandled_exception_filter( PEXCEPTION_POINTERS eptr ) DECLSPEC_HIDDEN;

#if defined(__x86_64__) || defined(__arm__) || defined(__aarch64__)
extern RUNTIME_FUNCTION *lookup_function_info( ULONG_PTR pc, ULONG_PTR *base, LDR_DATA_TABLE_ENTRY **module ) DECLSPEC_HIDDEN;
#endif

/* debug helpers */
extern LPCSTR debugstr_us( const UNICODE_STRING *str ) DECLSPEC_HIDDEN;
extern LPCSTR debugstr_ObjectAttributes(const OBJECT_ATTRIBUTES *oa) DECLSPEC_HIDDEN;

/* init routines */
extern SIZE_T signal_stack_size DECLSPEC_HIDDEN;
extern SIZE_T signal_stack_mask DECLSPEC_HIDDEN;
extern void signal_init_threading(void) DECLSPEC_HIDDEN;
extern NTSTATUS signal_alloc_thread( TEB *teb ) DECLSPEC_HIDDEN;
extern void signal_free_thread( TEB *teb ) DECLSPEC_HIDDEN;
extern void signal_init_thread( TEB *teb ) DECLSPEC_HIDDEN;
extern void signal_init_process(void) DECLSPEC_HIDDEN;
extern void signal_init_early(void) DECLSPEC_HIDDEN;
extern void signal_start_thread( LPTHREAD_START_ROUTINE entry, void *arg, BOOL suspend ) DECLSPEC_HIDDEN;
extern void signal_start_process( LPTHREAD_START_ROUTINE entry, BOOL suspend ) DECLSPEC_HIDDEN;
extern void DECLSPEC_NORETURN signal_exit_thread( int status ) DECLSPEC_HIDDEN;
extern void DECLSPEC_NORETURN signal_exit_process( int status ) DECLSPEC_HIDDEN;
extern void version_init(void) DECLSPEC_HIDDEN;
extern void debug_init(void) DECLSPEC_HIDDEN;
extern TEB *thread_init(void) DECLSPEC_HIDDEN;
extern void actctx_init(void) DECLSPEC_HIDDEN;
extern void virtual_init(void) DECLSPEC_HIDDEN;
extern void fill_cpu_info(void) DECLSPEC_HIDDEN;
extern void heap_set_debug_flags( HANDLE handle ) DECLSPEC_HIDDEN;
extern void init_unix_codepage(void) DECLSPEC_HIDDEN;
extern void init_locale( HMODULE module ) DECLSPEC_HIDDEN;
extern void init_user_process_params( SIZE_T data_size ) DECLSPEC_HIDDEN;
extern char **build_envp( const WCHAR *envW ) DECLSPEC_HIDDEN;
extern NTSTATUS restart_process( RTL_USER_PROCESS_PARAMETERS *params, NTSTATUS status ) DECLSPEC_HIDDEN;

extern int __wine_main_argc;
extern char **__wine_main_argv;
extern WCHAR **__wine_main_wargv;

/* token */
extern HANDLE CDECL __wine_create_default_token(BOOL admin);

/* server support */
extern const char *build_dir DECLSPEC_HIDDEN;
extern const char *data_dir DECLSPEC_HIDDEN;
extern const char *config_dir DECLSPEC_HIDDEN;
extern timeout_t server_start_time DECLSPEC_HIDDEN;
extern unsigned int server_cpus DECLSPEC_HIDDEN;
extern BOOL is_wow64 DECLSPEC_HIDDEN;
extern NTSTATUS exec_wineloader( char **argv, int socketfd, const pe_image_info_t *pe_info ) DECLSPEC_HIDDEN;
extern void server_init_process(void) DECLSPEC_HIDDEN;
extern void server_init_process_done(void) DECLSPEC_HIDDEN;
extern size_t server_init_thread( void *entry_point, BOOL *suspend ) DECLSPEC_HIDDEN;
extern void DECLSPEC_NORETURN abort_thread( int status ) DECLSPEC_HIDDEN;
extern void DECLSPEC_NORETURN exit_thread( int status ) DECLSPEC_HIDDEN;
extern sigset_t server_block_set DECLSPEC_HIDDEN;
extern unsigned int server_call_unlocked( void *req_ptr ) DECLSPEC_HIDDEN;
extern void server_enter_uninterrupted_section( RTL_CRITICAL_SECTION *cs, sigset_t *sigset ) DECLSPEC_HIDDEN;
extern void server_leave_uninterrupted_section( RTL_CRITICAL_SECTION *cs, sigset_t *sigset ) DECLSPEC_HIDDEN;
extern unsigned int server_select( const select_op_t *select_op, data_size_t size, UINT flags,
                                   timeout_t abs_timeout, CONTEXT *context, RTL_CRITICAL_SECTION *cs, user_apc_t *user_apc ) DECLSPEC_HIDDEN;
extern unsigned int server_wait( const select_op_t *select_op, data_size_t size,
                                 UINT flags, const LARGE_INTEGER *timeout ) DECLSPEC_HIDDEN;
extern unsigned int server_queue_process_apc( HANDLE process, const apc_call_t *call, apc_result_t *result ) DECLSPEC_HIDDEN;
extern int server_remove_fd_from_cache( HANDLE handle ) DECLSPEC_HIDDEN;
extern int server_get_unix_fd( HANDLE handle, unsigned int access, int *unix_fd,
                               int *needs_close, enum server_fd_type *type, unsigned int *options ) DECLSPEC_HIDDEN;
extern int receive_fd( obj_handle_t *handle ) DECLSPEC_HIDDEN;
extern int server_pipe( int fd[2] ) DECLSPEC_HIDDEN;
extern NTSTATUS alloc_object_attributes( const OBJECT_ATTRIBUTES *attr, struct object_attributes **ret,
                                         data_size_t *ret_len ) DECLSPEC_HIDDEN;
extern NTSTATUS validate_open_object_attributes( const OBJECT_ATTRIBUTES *attr ) DECLSPEC_HIDDEN;
extern void *server_get_shared_memory( HANDLE thread ) DECLSPEC_HIDDEN;

/* module handling */
extern LIST_ENTRY tls_links DECLSPEC_HIDDEN;
extern FARPROC RELAY_GetProcAddress( HMODULE module, const IMAGE_EXPORT_DIRECTORY *exports,
                                     DWORD exp_size, FARPROC proc, DWORD ordinal, const WCHAR *user ) DECLSPEC_HIDDEN;
extern FARPROC SNOOP_GetProcAddress( HMODULE hmod, const IMAGE_EXPORT_DIRECTORY *exports, DWORD exp_size,
                                     FARPROC origfun, DWORD ordinal, const WCHAR *user ) DECLSPEC_HIDDEN;
extern void RELAY_SetupDLL( HMODULE hmod ) DECLSPEC_HIDDEN;
extern void SNOOP_SetupDLL( HMODULE hmod ) DECLSPEC_HIDDEN;
extern const WCHAR windows_dir[] DECLSPEC_HIDDEN;
extern const WCHAR system_dir[] DECLSPEC_HIDDEN;
extern const WCHAR syswow64_dir[] DECLSPEC_HIDDEN;

extern const struct unix_funcs *unix_funcs DECLSPEC_HIDDEN;
extern void (WINAPI *kernel32_start_process)(LPTHREAD_START_ROUTINE,void*) DECLSPEC_HIDDEN;

/* Device IO */
extern NTSTATUS CDROM_DeviceIoControl(HANDLE hDevice, 
                                      HANDLE hEvent, PIO_APC_ROUTINE UserApcRoutine,
                                      PVOID UserApcContext, 
                                      PIO_STATUS_BLOCK piosb, 
                                      ULONG IoControlCode,
                                      LPVOID lpInBuffer, DWORD nInBufferSize,
                                      LPVOID lpOutBuffer, DWORD nOutBufferSize) DECLSPEC_HIDDEN;
extern NTSTATUS COMM_DeviceIoControl(HANDLE hDevice, 
                                     HANDLE hEvent, PIO_APC_ROUTINE UserApcRoutine,
                                     PVOID UserApcContext, 
                                     PIO_STATUS_BLOCK piosb, 
                                     ULONG IoControlCode,
                                     LPVOID lpInBuffer, DWORD nInBufferSize,
                                     LPVOID lpOutBuffer, DWORD nOutBufferSize) DECLSPEC_HIDDEN;
extern NTSTATUS TAPE_DeviceIoControl(HANDLE hDevice, 
                                     HANDLE hEvent, PIO_APC_ROUTINE UserApcRoutine,
                                     PVOID UserApcContext, 
                                     PIO_STATUS_BLOCK piosb, 
                                     ULONG IoControlCode,
                                     LPVOID lpInBuffer, DWORD nInBufferSize,
                                     LPVOID lpOutBuffer, DWORD nOutBufferSize) DECLSPEC_HIDDEN;
extern NTSTATUS COMM_FlushBuffersFile( int fd ) DECLSPEC_HIDDEN;

/* file I/O */
struct stat;
extern NTSTATUS FILE_GetNtStatus(void) DECLSPEC_HIDDEN;
extern int get_file_info( const char *path, struct stat *st, ULONG *attr ) DECLSPEC_HIDDEN;
extern NTSTATUS fill_file_info( const struct stat *st, ULONG attr, void *ptr,
                                FILE_INFORMATION_CLASS class ) DECLSPEC_HIDDEN;
extern NTSTATUS server_get_unix_name( HANDLE handle, ANSI_STRING *unix_name ) DECLSPEC_HIDDEN;
extern void init_directories(void) DECLSPEC_HIDDEN;
extern BOOL DIR_is_hidden_file( const char *name ) DECLSPEC_HIDDEN;
extern NTSTATUS DIR_unmount_device( HANDLE handle ) DECLSPEC_HIDDEN;
extern NTSTATUS DIR_get_unix_cwd( char **cwd ) DECLSPEC_HIDDEN;
extern unsigned int DIR_get_drives_info( struct drive_info info[MAX_DOS_DRIVES] ) DECLSPEC_HIDDEN;
extern NTSTATUS file_id_to_unix_file_name( const OBJECT_ATTRIBUTES *attr, ANSI_STRING *unix_name_ret ) DECLSPEC_HIDDEN;
extern NTSTATUS nt_to_unix_file_name_attr( const OBJECT_ATTRIBUTES *attr, ANSI_STRING *unix_name_ret,
                                           UINT disposition ) DECLSPEC_HIDDEN;

/* virtual memory */
extern NTSTATUS virtual_alloc( PVOID *ret, unsigned short zero_bits_64, SIZE_T *size_ptr,
                               ULONG type, ULONG protect ) DECLSPEC_HIDDEN;
extern NTSTATUS read_nt_symlink( HANDLE root, UNICODE_STRING *name, WCHAR *target, size_t length ) DECLSPEC_HIDDEN;
extern NTSTATUS virtual_map_section( HANDLE handle, PVOID *addr_ptr, unsigned short zero_bits_64, SIZE_T commit_size,
                                     const LARGE_INTEGER *offset_ptr, SIZE_T *size_ptr, ULONG alloc_type,
                                     ULONG protect, pe_image_info_t *image_info ) DECLSPEC_HIDDEN;
extern void virtual_get_system_info( SYSTEM_BASIC_INFORMATION *info ) DECLSPEC_HIDDEN;
extern NTSTATUS virtual_create_builtin_view( void *base ) DECLSPEC_HIDDEN;
extern TEB * virtual_alloc_first_teb(void) DECLSPEC_HIDDEN;
extern NTSTATUS virtual_alloc_teb( TEB **teb ) DECLSPEC_HIDDEN;
extern void virtual_free_teb( TEB *teb ) DECLSPEC_HIDDEN;
extern NTSTATUS virtual_alloc_thread_stack( INITIAL_TEB *stack, SIZE_T reserve_size,
                                            SIZE_T commit_size, SIZE_T *pthread_size ) DECLSPEC_HIDDEN;
extern NTSTATUS virtual_map_shared_memory( int fd, PVOID *addr_ptr, ULONG zero_bits, SIZE_T *size_ptr, ULONG protect ) DECLSPEC_HIDDEN;
extern void virtual_clear_thread_stack( void *stack_end ) DECLSPEC_HIDDEN;
extern int virtual_handle_stack_fault( void *addr ) DECLSPEC_HIDDEN;
extern BOOL virtual_is_valid_code_address( const void *addr, SIZE_T size ) DECLSPEC_HIDDEN;
extern NTSTATUS virtual_handle_fault( LPCVOID addr, DWORD err, BOOL on_signal_stack ) DECLSPEC_HIDDEN;
extern unsigned int virtual_locked_server_call( void *req_ptr ) DECLSPEC_HIDDEN;
extern ssize_t virtual_locked_read( int fd, void *addr, size_t size ) DECLSPEC_HIDDEN;
extern ssize_t virtual_locked_pread( int fd, void *addr, size_t size, off_t offset ) DECLSPEC_HIDDEN;
extern BOOL virtual_check_buffer_for_read( const void *ptr, SIZE_T size ) DECLSPEC_HIDDEN;
extern BOOL virtual_check_buffer_for_write( void *ptr, SIZE_T size ) DECLSPEC_HIDDEN;
extern SIZE_T virtual_uninterrupted_read_memory( const void *addr, void *buffer, SIZE_T size ) DECLSPEC_HIDDEN;
extern NTSTATUS virtual_uninterrupted_write_memory( void *addr, const void *buffer, SIZE_T size ) DECLSPEC_HIDDEN;
extern void VIRTUAL_SetForceExec( BOOL enable ) DECLSPEC_HIDDEN;
extern void virtual_release_address_space(void) DECLSPEC_HIDDEN;
extern void virtual_set_large_address_space( BOOL force ) DECLSPEC_HIDDEN;
extern void virtual_fill_image_information( const pe_image_info_t *pe_info,
                                            SECTION_IMAGE_INFORMATION *info ) DECLSPEC_HIDDEN;
extern struct _KUSER_SHARED_DATA *user_shared_data DECLSPEC_HIDDEN;
extern HANDLE user_shared_data_init_done(void) DECLSPEC_HIDDEN;

/* completion */
extern NTSTATUS NTDLL_AddCompletion( HANDLE hFile, ULONG_PTR CompletionValue,
                                     NTSTATUS CompletionStatus, ULONG Information, BOOL async) DECLSPEC_HIDDEN;

/* locale */
extern LCID user_lcid, system_lcid;
extern DWORD ntdll_umbstowcs( const char* src, DWORD srclen, WCHAR* dst, DWORD dstlen ) DECLSPEC_HIDDEN;
extern int ntdll_wcstoumbs( const WCHAR* src, DWORD srclen, char* dst, DWORD dstlen, BOOL strict ) DECLSPEC_HIDDEN;

extern int CDECL NTDLL__vsnprintf( char *str, SIZE_T len, const char *format, __ms_va_list args ) DECLSPEC_HIDDEN;
extern int CDECL NTDLL__vsnwprintf( WCHAR *str, SIZE_T len, const WCHAR *format, __ms_va_list args ) DECLSPEC_HIDDEN;

#ifdef __WINE_WINE_PORT_H

/* inline version of RtlEnterCriticalSection */
static inline void enter_critical_section( RTL_CRITICAL_SECTION *crit )
{
    if (InterlockedIncrement( &crit->LockCount ))
    {
        if (crit->OwningThread == ULongToHandle(GetCurrentThreadId()))
        {
            crit->RecursionCount++;
            return;
        }
        RtlpWaitForCriticalSection( crit );
    }
    crit->OwningThread   = ULongToHandle(GetCurrentThreadId());
    crit->RecursionCount = 1;
}

/* inline version of RtlLeaveCriticalSection */
static inline void leave_critical_section( RTL_CRITICAL_SECTION *crit )
{
    WINE_DECLARE_DEBUG_CHANNEL(ntdll);
    if (--crit->RecursionCount)
    {
        if (crit->RecursionCount > 0) InterlockedDecrement( &crit->LockCount );
        else ERR_(ntdll)( "section %p is not acquired\n", crit );
    }
    else
    {
        crit->OwningThread = 0;
        if (InterlockedDecrement( &crit->LockCount ) >= 0)
            RtlpUnWaitCriticalSection( crit );
    }
}

#endif  /* __WINE_WINE_PORT_H */

/* load order */

enum loadorder
{
    LO_INVALID,
    LO_DISABLED,
    LO_NATIVE,
    LO_BUILTIN,
    LO_NATIVE_BUILTIN,  /* native then builtin */
    LO_BUILTIN_NATIVE,  /* builtin then native */
    LO_DEFAULT          /* nothing specified, use default strategy */
};

extern enum loadorder get_load_order( const WCHAR *app_name, const UNICODE_STRING *nt_name ) DECLSPEC_HIDDEN;

struct debug_info
{
    unsigned int str_pos;       /* current position in strings buffer */
    unsigned int out_pos;       /* current position in output buffer */
    char         strings[1024]; /* buffer for temporary strings */
    char         output[1024];  /* current output line */
};

/* thread private data, stored in NtCurrentTeb()->GdiTebBatch */
struct ntdll_thread_data
{
    struct debug_info *debug_info;    /* info for debugstr functions */
    int                esync_queue_fd;/* fd to wait on for driver events */
    int                esync_apc_fd;  /* fd to wait on for user APCs */
    int               *fsync_apc_futex;
    void              *start_stack;   /* stack for thread startup */
    int                request_fd;    /* fd for sending server requests */
    int                reply_fd;      /* fd for receiving server replies */
    int                wait_fd[2];    /* fd for sleeping server requests */
    BOOL               wow64_redir;   /* Wow64 filesystem redirection flag */
    pthread_t          pthread_id;    /* pthread thread id */
    void              *pthread_stack; /* pthread stack */
};

C_ASSERT( sizeof(struct ntdll_thread_data) <= sizeof(((TEB *)0)->GdiTebBatch) );

static inline struct ntdll_thread_data *ntdll_get_thread_data(void)
{
    return (struct ntdll_thread_data *)&NtCurrentTeb()->GdiTebBatch;
}

static inline int get_unix_exit_code( NTSTATUS status )
{
    /* prevent a nonzero exit code to end up truncated to zero in unix */
    if (status && !(status & 0xff)) return 1;
    return status;
}

extern mode_t FILE_umask DECLSPEC_HIDDEN;
extern HANDLE keyed_event DECLSPEC_HIDDEN;
extern SYSTEM_CPU_INFORMATION cpu_info DECLSPEC_HIDDEN;

#define HASH_STRING_ALGORITHM_DEFAULT  0
#define HASH_STRING_ALGORITHM_X65599   1
#define HASH_STRING_ALGORITHM_INVALID  0xffffffff

NTSTATUS WINAPI RtlHashUnicodeString(PCUNICODE_STRING,BOOLEAN,ULONG,ULONG*);
void     WINAPI LdrInitializeThunk(CONTEXT*,void**,ULONG_PTR,ULONG_PTR);

#ifndef __GCC_HAVE_SYNC_COMPARE_AND_SWAP_8
#define InterlockedCompareExchange64(dest,xchg,cmp) RtlInterlockedCompareExchange64(dest,xchg,cmp)
#endif

/* version */
extern const char * CDECL NTDLL_wine_get_version(void);
extern const char * CDECL NTDLL_wine_get_build_id(void);
extern void CDECL NTDLL_wine_get_host_version( const char **sysname, const char **release );

/* process / thread time */
extern BOOL read_process_time(int unix_pid, int unix_tid, unsigned long clk_tck,
                              LARGE_INTEGER *kernel, LARGE_INTEGER *user) DECLSPEC_HIDDEN;
extern BOOL read_process_memory_stats(int unix_pid, VM_COUNTERS *pvmi) DECLSPEC_HIDDEN;

/* string functions */
int    __cdecl NTDLL_tolower( int c );
int    __cdecl _stricmp( LPCSTR str1, LPCSTR str2 );
int    __cdecl NTDLL__wcsicmp( LPCWSTR str1, LPCWSTR str2 );
int    __cdecl NTDLL__wcsnicmp( LPCWSTR str1, LPCWSTR str2, size_t n );
int    __cdecl NTDLL_wcscmp( LPCWSTR str1, LPCWSTR str2 );
int    __cdecl NTDLL_wcsncmp( LPCWSTR str1, LPCWSTR str2, size_t n );
WCHAR  __cdecl NTDLL_towlower( WCHAR ch );
WCHAR  __cdecl NTDLL_towupper( WCHAR ch );
LPWSTR __cdecl NTDLL__wcslwr( LPWSTR str );
LPWSTR __cdecl NTDLL__wcsupr( LPWSTR str );
LPWSTR __cdecl NTDLL_wcscpy( LPWSTR dst, LPCWSTR src );
LPWSTR __cdecl NTDLL_wcscat( LPWSTR dst, LPCWSTR src );
LPWSTR __cdecl NTDLL_wcschr( LPCWSTR str, WCHAR ch );
size_t __cdecl NTDLL_wcslen( LPCWSTR str );
size_t __cdecl NTDLL_wcscspn( LPCWSTR str, LPCWSTR reject );
LPWSTR __cdecl NTDLL_wcsncat( LPWSTR s1, LPCWSTR s2, size_t n );
LPWSTR __cdecl NTDLL_wcsncpy( LPWSTR s1, LPCWSTR s2, size_t n );
LPWSTR __cdecl NTDLL_wcspbrk( LPCWSTR str, LPCWSTR accept );
LPWSTR __cdecl NTDLL_wcsrchr( LPCWSTR str, WCHAR ch );
size_t __cdecl NTDLL_wcsspn( LPCWSTR str, LPCWSTR accept );
LPWSTR __cdecl NTDLL_wcsstr( LPCWSTR str, LPCWSTR sub );
LPWSTR __cdecl NTDLL_wcstok( LPWSTR str, LPCWSTR delim );
LONG   __cdecl NTDLL_wcstol( LPCWSTR s, LPWSTR *end, INT base );
ULONG  __cdecl NTDLL_wcstoul( LPCWSTR s, LPWSTR *end, INT base );
int    WINAPIV NTDLL_swprintf( WCHAR *str, const WCHAR *format, ... );
int    WINAPIV _snwprintf_s( WCHAR *str, SIZE_T size, SIZE_T len, const WCHAR *format, ... );

#define wcsicmp(s1,s2) NTDLL__wcsicmp(s1,s2)
#define wcsnicmp(s1,s2,n) NTDLL__wcsnicmp(s1,s2,n)
#define towupper(c) NTDLL_towupper(c)
#define wcslwr(s) NTDLL__wcslwr(s)
#define wcsupr(s) NTDLL__wcsupr(s)
#define wcscpy(d,s) NTDLL_wcscpy(d,s)
#define wcscat(d,s) NTDLL_wcscat(d,s)
#define wcschr(s,c) NTDLL_wcschr(s,c)
#define wcspbrk(s,a) NTDLL_wcspbrk(s,a)
#define wcsrchr(s,c) NTDLL_wcsrchr(s,c)
#define wcstoul(s,e,b) NTDLL_wcstoul(s,e,b)
#define wcslen(s) NTDLL_wcslen(s)
#define wcscspn(s,r) NTDLL_wcscspn(s,r)
#define wcsspn(s,a) NTDLL_wcsspn(s,a)
#define wcscmp(s1,s2) NTDLL_wcscmp(s1,s2)
#define wcsncmp(s1,s2,n) NTDLL_wcsncmp(s1,s2,n)

/* convert from straight ASCII to Unicode without depending on the current codepage */
static inline void ascii_to_unicode( WCHAR *dst, const char *src, size_t len )
{
    while (len--) *dst++ = (unsigned char)*src++;
}

#if defined(__i386__) || defined(__x86_64__)
NTSTATUS WINAPI __syscall_NtOpenFile( PHANDLE handle, ACCESS_MASK access,
                            POBJECT_ATTRIBUTES attr, PIO_STATUS_BLOCK io,
                            ULONG sharing, ULONG options );
#else
#define __syscall_NtOpenFile NtOpenFile
#endif

#endif
