/*
 * Copyright 2010 Piotr Caban for CodeWeavers
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

#include <stdarg.h>
#include <limits.h>
#include <errno.h>

#include "msvcp90.h"

#include "windef.h"
#include "winbase.h"
#include "winternl.h"
#include "wine/debug.h"
#include "wine/exception.h"
WINE_DEFAULT_DEBUG_CHANNEL(msvcp);

#if _MSVCP_VER >= 110
/* error strings generated with glibc strerror */
static const char str_EPERM[]           = "operation not permitted";
static const char str_ENOENT[]          = "no such file or directory";
static const char str_ESRCH[]           = "no such process";
static const char str_EINTR[]           = "interrupted system call";
static const char str_EIO[]             = "input/output error";
static const char str_ENXIO[]           = "no such device or address";
static const char str_E2BIG[]           = "argument list too long";
static const char str_ENOEXEC[]         = "exec format error";
static const char str_EBADF[]           = "bad file descriptor";
static const char str_ECHILD[]          = "no child processes";
static const char str_EAGAIN[]          = "resource temporarily unavailable";
static const char str_ENOMEM[]          = "cannot allocate memory";
static const char str_EACCES[]          = "permission denied";
static const char str_EFAULT[]          = "bad address";
static const char str_EBUSY[]           = "device or resource busy";
static const char str_EEXIST[]          = "file exists";
static const char str_EXDEV[]           = "invalid cross-device link";
static const char str_ENODEV[]          = "no such device";
static const char str_ENOTDIR[]         = "not a directory";
static const char str_EISDIR[]          = "is a directory";
static const char str_EINVAL[]          = "invalid argument";
static const char str_ENFILE[]          = "too many open files in system";
static const char str_EMFILE[]          = "too many open files";
static const char str_ENOTTY[]          = "inappropriate ioctl for device";
static const char str_EFBIG[]           = "file too large";
static const char str_ENOSPC[]          = "no space left on device";
static const char str_ESPIPE[]          = "illegal seek";
static const char str_EROFS[]           = "read-only file system";
static const char str_EMLINK[]          = "too many links";
static const char str_EPIPE[]           = "broken pipe";
static const char str_EDOM[]            = "numerical argument out of domain";
static const char str_ERANGE[]          = "numerical result out of range";
static const char str_EDEADLK[]         = "resource deadlock avoided";
static const char str_ENAMETOOLONG[]    = "file name too long";
static const char str_ENOLCK[]          = "no locks available";
static const char str_ENOSYS[]          = "function not implemented";
static const char str_ENOTEMPTY[]       = "directory not empty";
static const char str_EILSEQ[]          = "invalid or incomplete multibyte or wide character";
static const char str_EADDRINUSE[]      = "address already in use";
static const char str_EADDRNOTAVAIL[]   = "cannot assign requested address";
static const char str_EAFNOSUPPORT[]    = "address family not supported by protocol";
static const char str_EALREADY[]        = "operation already in progress";
static const char str_EBADMSG[]         = "not a data message";
static const char str_ECANCELED[]       = "operation Canceled";
static const char str_ECONNABORTED[]    = "software caused connection abort";
static const char str_ECONNREFUSED[]    = "connection refused";
static const char str_ECONNRESET[]      = "connection reset by peer";
static const char str_EDESTADDRREQ[]    = "destination address required";
static const char str_EHOSTUNREACH[]    = "no route to host";
static const char str_EIDRM[]           = "identifier removed";
static const char str_EINPROGRESS[]     = "operation now in progress";
static const char str_EISCONN[]         = "transport endpoint is already connected";
static const char str_ELOOP[]           = "too many symbolic links encountered";
static const char str_EMSGSIZE[]        = "message too long";
static const char str_ENETDOWN[]        = "network is down";
static const char str_ENETRESET[]       = "network dropped connection because of reset";
static const char str_ENETUNREACH[]     = "network is unreachable";
static const char str_ENOBUFS[]         = "no buffer space available";
static const char str_ENODATA[]         = "no data available";
static const char str_ENOLINK[]         = "link has been severed";
static const char str_ENOMSG[]          = "no message of desired type";
static const char str_ENOPROTOOPT[]     = "protocol not available";
static const char str_ENOSR[]           = "out of streams resources";
static const char str_ENOSTR[]          = "device not a stream";
static const char str_ENOTCONN[]        = "transport endpoint is not connected";
static const char str_ENOTRECOVERABLE[] = "state not recoverable";
static const char str_ENOTSOCK[]        = "socket operation on non-socket";
static const char str_ENOTSUP[]         = "not supported";
static const char str_EOPNOTSUPP[]      = "operation not supported on transport endpoint";
static const char str_EOVERFLOW[]       = "value too large for defined data type";
static const char str_EOWNERDEAD[]      = "owner died";
static const char str_EPROTO[]          = "protocol error";
static const char str_EPROTONOSUPPORT[] = "protocol not supported";
static const char str_EPROTOTYPE[]      = "protocol wrong type for socket";
static const char str_ETIME[]           = "timer expired";
static const char str_ETIMEDOUT[]       = "connection timed out";
static const char str_ETXTBSY[]         = "text file busy";
static const char str_EWOULDBLOCK[]     = "operation would block";

static const struct {
    int err;
    const char *str;
} syserror_map[] =
{
    {EPERM, str_EPERM},
    {ENOENT, str_ENOENT},
    {ESRCH, str_ESRCH},
    {EINTR, str_EINTR},
    {EIO, str_EIO},
    {ENXIO, str_ENXIO},
    {E2BIG, str_E2BIG},
    {ENOEXEC, str_ENOEXEC},
    {EBADF, str_EBADF},
    {ECHILD, str_ECHILD},
    {EAGAIN, str_EAGAIN},
    {ENOMEM, str_ENOMEM},
    {EACCES, str_EACCES},
    {EFAULT, str_EFAULT},
    {EBUSY, str_EBUSY},
    {EEXIST, str_EEXIST},
    {EXDEV, str_EXDEV},
    {ENODEV, str_ENODEV},
    {ENOTDIR, str_ENOTDIR},
    {EISDIR, str_EISDIR},
    {EINVAL, str_EINVAL},
    {ENFILE, str_ENFILE},
    {EMFILE, str_EMFILE},
    {ENOTTY, str_ENOTTY},
    {EFBIG, str_EFBIG},
    {ENOSPC, str_ENOSPC},
    {ESPIPE, str_ESPIPE},
    {EROFS, str_EROFS},
    {EMLINK, str_EMLINK},
    {EPIPE, str_EPIPE},
    {EDOM, str_EDOM},
    {ERANGE, str_ERANGE},
    {EDEADLK, str_EDEADLK},
    {ENAMETOOLONG, str_ENAMETOOLONG},
    {ENOLCK, str_ENOLCK},
    {ENOSYS, str_ENOSYS},
    {ENOTEMPTY, str_ENOTEMPTY},
    {EILSEQ, str_EILSEQ},
    {EADDRINUSE, str_EADDRINUSE},
    {EADDRNOTAVAIL, str_EADDRNOTAVAIL},
    {EAFNOSUPPORT, str_EAFNOSUPPORT},
    {EALREADY, str_EALREADY},
    {EBADMSG, str_EBADMSG},
    {ECANCELED, str_ECANCELED},
    {ECONNABORTED, str_ECONNABORTED},
    {ECONNREFUSED, str_ECONNREFUSED},
    {ECONNRESET, str_ECONNRESET},
    {EDESTADDRREQ, str_EDESTADDRREQ},
    {EHOSTUNREACH, str_EHOSTUNREACH},
    {EIDRM, str_EIDRM},
    {EINPROGRESS, str_EINPROGRESS},
    {EISCONN, str_EISCONN},
    {ELOOP, str_ELOOP},
    {EMSGSIZE, str_EMSGSIZE},
    {ENETDOWN, str_ENETDOWN},
    {ENETRESET, str_ENETRESET},
    {ENETUNREACH, str_ENETUNREACH},
    {ENOBUFS, str_ENOBUFS},
    {ENODATA, str_ENODATA},
    {ENOLINK, str_ENOLINK},
    {ENOMSG, str_ENOMSG},
    {ENOPROTOOPT, str_ENOPROTOOPT},
    {ENOSR, str_ENOSR},
    {ENOSTR, str_ENOSTR},
    {ENOTCONN, str_ENOTCONN},
    {ENOTRECOVERABLE, str_ENOTRECOVERABLE},
    {ENOTSOCK, str_ENOTSOCK},
    {ENOTSUP, str_ENOTSUP},
    {EOPNOTSUPP, str_EOPNOTSUPP},
    {EOVERFLOW, str_EOVERFLOW},
    {EOWNERDEAD, str_EOWNERDEAD},
    {EPROTO, str_EPROTO},
    {EPROTONOSUPPORT, str_EPROTONOSUPPORT},
    {EPROTOTYPE, str_EPROTOTYPE},
    {ETIME, str_ETIME},
    {ETIMEDOUT, str_ETIMEDOUT},
    {ETXTBSY, str_ETXTBSY},
    {EWOULDBLOCK, str_EWOULDBLOCK},
};
#endif

#if _MSVCP_VER >= 140
static const struct {
    int winerr;
    int doserr;
} winerror_map[] =
{
    {ERROR_INVALID_FUNCTION, ENOSYS}, {ERROR_FILE_NOT_FOUND, ENOENT},
    {ERROR_PATH_NOT_FOUND, ENOENT}, {ERROR_TOO_MANY_OPEN_FILES, EMFILE},
    {ERROR_ACCESS_DENIED, EACCES}, {ERROR_INVALID_HANDLE, EINVAL},
    {ERROR_NOT_ENOUGH_MEMORY, ENOMEM}, {ERROR_INVALID_ACCESS, EACCES},
    {ERROR_OUTOFMEMORY, ENOMEM}, {ERROR_INVALID_DRIVE, ENODEV},
    {ERROR_CURRENT_DIRECTORY, EACCES}, {ERROR_NOT_SAME_DEVICE, EXDEV},
    {ERROR_WRITE_PROTECT, EACCES}, {ERROR_BAD_UNIT, ENODEV},
    {ERROR_NOT_READY, EAGAIN}, {ERROR_SEEK, EIO}, {ERROR_WRITE_FAULT, EIO},
    {ERROR_READ_FAULT, EIO}, {ERROR_SHARING_VIOLATION, EACCES},
    {ERROR_LOCK_VIOLATION, ENOLCK}, {ERROR_HANDLE_DISK_FULL, ENOSPC},
    {ERROR_NOT_SUPPORTED, ENOTSUP}, {ERROR_DEV_NOT_EXIST, ENODEV},
    {ERROR_FILE_EXISTS, EEXIST}, {ERROR_CANNOT_MAKE, EACCES},
    {ERROR_INVALID_PARAMETER, EINVAL}, {ERROR_OPEN_FAILED, EIO},
    {ERROR_BUFFER_OVERFLOW, ENAMETOOLONG}, {ERROR_DISK_FULL, ENOSPC},
    {ERROR_INVALID_NAME, EINVAL}, {ERROR_NEGATIVE_SEEK, EINVAL},
    {ERROR_BUSY_DRIVE, EBUSY}, {ERROR_DIR_NOT_EMPTY, ENOTEMPTY},
    {ERROR_BUSY, EBUSY}, {ERROR_ALREADY_EXISTS, EEXIST},
    {ERROR_LOCKED, ENOLCK}, {ERROR_DIRECTORY, EINVAL},
    {ERROR_OPERATION_ABORTED, ECANCELED}, {ERROR_NOACCESS, EACCES},
    {ERROR_CANTOPEN, EIO}, {ERROR_CANTREAD, EIO}, {ERROR_CANTWRITE, EIO},
    {ERROR_RETRY, EAGAIN}, {ERROR_OPEN_FILES, EBUSY},
    {ERROR_DEVICE_IN_USE, EBUSY}, {ERROR_REPARSE_TAG_INVALID, EINVAL},
    {WSAEINTR, EINTR}, {WSAEBADF, EBADF}, {WSAEACCES, EACCES},
    {WSAEFAULT, EFAULT}, {WSAEINVAL, EINVAL}, {WSAEMFILE, EMFILE},
    {WSAEWOULDBLOCK, EWOULDBLOCK}, {WSAEINPROGRESS, EINPROGRESS},
    {WSAEALREADY, EALREADY}, {WSAENOTSOCK, ENOTSOCK},
    {WSAEDESTADDRREQ, EDESTADDRREQ}, {WSAEMSGSIZE, EMSGSIZE},
    {WSAEPROTOTYPE, EPROTOTYPE}, {WSAENOPROTOOPT, ENOPROTOOPT},
    {WSAEPROTONOSUPPORT, EPROTONOSUPPORT}, {WSAEOPNOTSUPP, EOPNOTSUPP},
    {WSAEAFNOSUPPORT, EAFNOSUPPORT}, {WSAEADDRINUSE, EADDRINUSE},
    {WSAEADDRNOTAVAIL, EADDRNOTAVAIL}, {WSAENETDOWN, ENETDOWN},
    {WSAENETUNREACH, ENETUNREACH}, {WSAENETRESET, ENETRESET},
    {WSAECONNABORTED, ECONNABORTED}, {WSAECONNRESET, ECONNRESET},
    {WSAENOBUFS, ENOBUFS}, {WSAEISCONN, EISCONN}, {WSAENOTCONN, ENOTCONN},
    {WSAETIMEDOUT, ETIMEDOUT}, {WSAECONNREFUSED, ECONNREFUSED},
    {WSAENAMETOOLONG, ENAMETOOLONG}, {WSAEHOSTUNREACH, EHOSTUNREACH}
};
#endif

struct __Container_proxy;

typedef struct {
    struct __Container_proxy *proxy;
} _Container_base12;

typedef struct __Iterator_base12 {
    struct __Container_proxy *proxy;
    struct __Iterator_base12 *next;
} _Iterator_base12;

typedef struct __Container_proxy {
    const _Container_base12 *cont;
    _Iterator_base12 *head;
} _Container_proxy;

/* ??0_Mutex@std@@QAE@XZ */
/* ??0_Mutex@std@@QEAA@XZ */
DEFINE_THISCALL_WRAPPER(mutex_ctor, 4)
mutex* __thiscall mutex_ctor(mutex *this)
{
    CRITICAL_SECTION *cs = MSVCRT_operator_new(sizeof(*cs));
    if(!cs) {
        ERR("Out of memory\n");
        throw_exception(EXCEPTION_BAD_ALLOC, NULL);
    }

    InitializeCriticalSection(cs);
    cs->DebugInfo->Spare[0] = (DWORD_PTR)(__FILE__ ": _Mutex critical section");
    this->mutex = cs;
    return this;
}

/* ??1_Mutex@std@@QAE@XZ */
/* ??1_Mutex@std@@QEAA@XZ */
DEFINE_THISCALL_WRAPPER(mutex_dtor, 4)
void __thiscall mutex_dtor(mutex *this)
{
    ((CRITICAL_SECTION*)this->mutex)->DebugInfo->Spare[0] = 0;
    DeleteCriticalSection(this->mutex);
    MSVCRT_operator_delete(this->mutex);
}

/* ?_Lock@_Mutex@std@@QAEXXZ */
/* ?_Lock@_Mutex@std@@QEAAXXZ */
DEFINE_THISCALL_WRAPPER(mutex_lock, 4)
void __thiscall mutex_lock(mutex *this)
{
    EnterCriticalSection(this->mutex);
}

/* ?_Unlock@_Mutex@std@@QAEXXZ */
/* ?_Unlock@_Mutex@std@@QEAAXXZ */
DEFINE_THISCALL_WRAPPER(mutex_unlock, 4)
void __thiscall mutex_unlock(mutex *this)
{
    LeaveCriticalSection(this->mutex);
}

/* ?_Mutex_Lock@_Mutex@std@@CAXPAV12@@Z */
/* ?_Mutex_Lock@_Mutex@std@@CAXPEAV12@@Z */
void CDECL mutex_mutex_lock(mutex *m)
{
    mutex_lock(m);
}

/* ?_Mutex_Unlock@_Mutex@std@@CAXPAV12@@Z */
/* ?_Mutex_Unlock@_Mutex@std@@CAXPEAV12@@Z */
void CDECL mutex_mutex_unlock(mutex *m)
{
    mutex_unlock(m);
}

/* ?_Mutex_ctor@_Mutex@std@@CAXPAV12@@Z */
/* ?_Mutex_ctor@_Mutex@std@@CAXPEAV12@@Z */
void CDECL mutex_mutex_ctor(mutex *m)
{
    mutex_ctor(m);
}

/* ?_Mutex_dtor@_Mutex@std@@CAXPAV12@@Z */
/* ?_Mutex_dtor@_Mutex@std@@CAXPEAV12@@Z */
void CDECL mutex_mutex_dtor(mutex *m)
{
    mutex_dtor(m);
}

static CRITICAL_SECTION lockit_cs[_MAX_LOCK];

static LONG init_locks;
static CRITICAL_SECTION init_locks_cs;
static CRITICAL_SECTION_DEBUG init_locks_cs_debug =
{
    0, 0, &init_locks_cs,
    { &init_locks_cs_debug.ProcessLocksList, &init_locks_cs_debug.ProcessLocksList },
    0, 0, { (DWORD_PTR)(__FILE__ ": init_locks_cs") }
};
static CRITICAL_SECTION init_locks_cs = { &init_locks_cs_debug, -1, 0, 0, 0, 0 };

/* ?_Init_locks_ctor@_Init_locks@std@@CAXPAV12@@Z */
/* ?_Init_locks_ctor@_Init_locks@std@@CAXPEAV12@@Z */
void __cdecl _Init_locks__Init_locks_ctor(_Init_locks *this)
{
    int i;

    EnterCriticalSection(&init_locks_cs);
    if (!init_locks)
    {
        for(i=0; i<_MAX_LOCK; i++)
        {
            InitializeCriticalSection(&lockit_cs[i]);
            lockit_cs[i].DebugInfo->Spare[0] = (DWORD_PTR)(__FILE__ ": _Lockit critical section");
        }
    }
    init_locks++;
    LeaveCriticalSection(&init_locks_cs);
}

/* ??0_Init_locks@std@@QAE@XZ */
/* ??0_Init_locks@std@@QEAA@XZ */
DEFINE_THISCALL_WRAPPER(_Init_locks_ctor, 4)
_Init_locks* __thiscall _Init_locks_ctor(_Init_locks *this)
{
    _Init_locks__Init_locks_ctor(this);
    return this;
}

/* ?_Init_locks_dtor@_Init_locks@std@@CAXPAV12@@Z */
/* ?_Init_locks_dtor@_Init_locks@std@@CAXPEAV12@@Z */
void __cdecl _Init_locks__Init_locks_dtor(_Init_locks *this)
{
    int i;

    EnterCriticalSection(&init_locks_cs);
    init_locks--;
    if (!init_locks)
    {
        for(i=0; i<_MAX_LOCK; i++)
        {
            lockit_cs[i].DebugInfo->Spare[0] = 0;
            DeleteCriticalSection(&lockit_cs[i]);
        }
    }
    LeaveCriticalSection(&init_locks_cs);
}

/* ??1_Init_locks@std@@QAE@XZ */
/* ??1_Init_locks@std@@QEAA@XZ */
DEFINE_THISCALL_WRAPPER(_Init_locks_dtor, 4)
void __thiscall _Init_locks_dtor(_Init_locks *this)
{
    _Init_locks__Init_locks_dtor(this);
}

#if _MSVCP_VER >= 70
static inline int get_locktype( _Lockit *lockit ) { return lockit->locktype; }
static inline void set_locktype( _Lockit *lockit, int type ) { lockit->locktype = type; }
#else
static inline int get_locktype( _Lockit *lockit ) { return 0; }
static inline void set_locktype( _Lockit *lockit, int type ) { }
#endif

/* ?_Lockit_ctor@_Lockit@std@@SAXH@Z */
void __cdecl _Lockit__Lockit_ctor_lock(int locktype)
{
    EnterCriticalSection(&lockit_cs[locktype]);
}

/* ?_Lockit_ctor@_Lockit@std@@CAXPAV12@H@Z */
/* ?_Lockit_ctor@_Lockit@std@@CAXPEAV12@H@Z */
void __cdecl _Lockit__Lockit_ctor_locktype(_Lockit *lockit, int locktype)
{
    set_locktype( lockit, locktype );
    _Lockit__Lockit_ctor_lock(locktype);
}

/* ?_Lockit_ctor@_Lockit@std@@CAXPAV12@@Z */
/* ?_Lockit_ctor@_Lockit@std@@CAXPEAV12@@Z */
void __cdecl _Lockit__Lockit_ctor(_Lockit *lockit)
{
    _Lockit__Lockit_ctor_locktype(lockit, 0);
}

/* ??0_Lockit@std@@QAE@H@Z */
/* ??0_Lockit@std@@QEAA@H@Z */
DEFINE_THISCALL_WRAPPER(_Lockit_ctor_locktype, 8)
_Lockit* __thiscall _Lockit_ctor_locktype(_Lockit *this, int locktype)
{
    _Lockit__Lockit_ctor_locktype(this, locktype);
    return this;
}

/* ??0_Lockit@std@@QAE@XZ */
/* ??0_Lockit@std@@QEAA@XZ */
DEFINE_THISCALL_WRAPPER(_Lockit_ctor, 4)
_Lockit* __thiscall _Lockit_ctor(_Lockit *this)
{
    _Lockit__Lockit_ctor_locktype(this, 0);
    return this;
}

/* ?_Lockit_dtor@_Lockit@std@@SAXH@Z */
void __cdecl _Lockit__Lockit_dtor_unlock(int locktype)
{
    LeaveCriticalSection(&lockit_cs[locktype]);
}

/* ?_Lockit_dtor@_Lockit@std@@CAXPAV12@@Z */
/* ?_Lockit_dtor@_Lockit@std@@CAXPEAV12@@Z */
void __cdecl _Lockit__Lockit_dtor(_Lockit *lockit)
{
    _Lockit__Lockit_dtor_unlock(get_locktype( lockit ));
}

/* ??1_Lockit@std@@QAE@XZ */
/* ??1_Lockit@std@@QEAA@XZ */
DEFINE_THISCALL_WRAPPER(_Lockit_dtor, 4)
void __thiscall _Lockit_dtor(_Lockit *this)
{
    _Lockit__Lockit_dtor(this);
}

/* wctype */
unsigned short __cdecl wctype(const char *property)
{
    static const struct {
        const char *name;
        unsigned short mask;
    } properties[] = {
        { "alnum", _DIGIT|_ALPHA },
        { "alpha", _ALPHA },
        { "cntrl", _CONTROL },
        { "digit", _DIGIT },
        { "graph", _DIGIT|_PUNCT|_ALPHA },
        { "lower", _LOWER },
        { "print", _DIGIT|_PUNCT|_BLANK|_ALPHA },
        { "punct", _PUNCT },
        { "space", _SPACE },
        { "upper", _UPPER },
        { "xdigit", _HEX }
    };
    unsigned int i;

    for(i = 0; i < ARRAY_SIZE(properties); i++)
        if(!strcmp(property, properties[i].name))
            return properties[i].mask;

    return 0;
}

typedef void (__cdecl *MSVCP_new_handler_func)(void);
static MSVCP_new_handler_func MSVCP_new_handler;
static int __cdecl new_handler_wrapper(size_t unused)
{
    MSVCP_new_handler();
    return 1;
}

/* ?set_new_handler@std@@YAP6AXXZP6AXXZ@Z */
MSVCP_new_handler_func __cdecl set_new_handler(MSVCP_new_handler_func new_handler)
{
    MSVCP_new_handler_func old_handler = MSVCP_new_handler;

    TRACE("%p\n", new_handler);

    MSVCP_new_handler = new_handler;
    MSVCRT_set_new_handler(new_handler ? new_handler_wrapper : NULL);
    return old_handler;
}

/* ?set_new_handler@std@@YAP6AXXZH@Z */
MSVCP_new_handler_func __cdecl set_new_handler_reset(int unused)
{
    return set_new_handler(NULL);
}

/* _Container_base0 is used by apps compiled without iterator checking
 * (i.e. with _ITERATOR_DEBUG_LEVEL=0 ).
 * It provides empty versions of methods used by visual c++'s stl's
 * iterator checking.
 * msvcr100 has to provide them in case apps are compiled with /Od
 * or the optimizer fails to inline those (empty) calls.
 */

/* ?_Orphan_all@_Container_base0@std@@QAEXXZ */
/* ?_Orphan_all@_Container_base0@std@@QEAAXXZ */
DEFINE_THISCALL_WRAPPER(Container_base0_Orphan_all, 4)
void __thiscall Container_base0_Orphan_all(void *this)
{
}

/* ?_Swap_all@_Container_base0@std@@QAEXAAU12@@Z */
/* ?_Swap_all@_Container_base0@std@@QEAAXAEAU12@@Z */
DEFINE_THISCALL_WRAPPER(Container_base0_Swap_all, 8)
void __thiscall Container_base0_Swap_all(void *this, void *that)
{
}

/* ??4_Container_base0@std@@QAEAAU01@ABU01@@Z */
/* ??4_Container_base0@std@@QEAAAEAU01@AEBU01@@Z */
DEFINE_THISCALL_WRAPPER(Container_base0_op_assign, 8)
void* __thiscall Container_base0_op_assign(void *this, const void *that)
{
    return this;
}

/* ??0_Container_base12@std@@QAE@ABU01@@Z */
/* ??0_Container_base12@std@@QEAA@AEBU01@@Z */
DEFINE_THISCALL_WRAPPER(_Container_base12_copy_ctor, 8)
_Container_base12* __thiscall _Container_base12_copy_ctor(
        _Container_base12 *this, _Container_base12 *that)
{
    this->proxy = NULL;
    return this;
}

/* ??0_Container_base12@std@@QAE@XZ */
/* ??0_Container_base12@std@@QEAA@XZ */
DEFINE_THISCALL_WRAPPER(_Container_base12_ctor, 4)
_Container_base12* __thiscall _Container_base12_ctor(_Container_base12 *this)
{
    this->proxy = NULL;
    return this;
}

/* ??1_Container_base12@std@@QAE@XZ */
/* ??1_Container_base12@std@@QEAA@XZ */
DEFINE_THISCALL_WRAPPER(_Container_base12_dtor, 4)
void __thiscall _Container_base12_dtor(_Container_base12 *this)
{
}

/* ??4_Container_base12@std@@QAEAAU01@ABU01@@Z */
/* ??4_Container_base12@std@@QEAAAEAU01@AEBU01@@ */
DEFINE_THISCALL_WRAPPER(_Container_base12_op_assign, 8)
_Container_base12* __thiscall _Container_base12_op_assign(
        _Container_base12 *this, const _Container_base12 *that)
{
    return this;
}

/* ?_Getpfirst@_Container_base12@std@@QBEPAPAU_Iterator_base12@2@XZ */
/* ?_Getpfirst@_Container_base12@std@@QEBAPEAPEAU_Iterator_base12@2@XZ */
DEFINE_THISCALL_WRAPPER(_Container_base12__Getpfirst, 4)
_Iterator_base12** __thiscall _Container_base12__Getpfirst(_Container_base12 *this)
{
    return this->proxy ? &this->proxy->head : NULL;
}

/* ?_Orphan_all@_Container_base12@std@@QAEXXZ */
/* ?_Orphan_all@_Container_base12@std@@QEAAXXZ */
DEFINE_THISCALL_WRAPPER(_Container_base12__Orphan_all, 4)
void __thiscall _Container_base12__Orphan_all(_Container_base12 *this)
{
}

/* ?_Swap_all@_Container_base12@std@@QAEXAAU12@@Z */
/* ?_Swap_all@_Container_base12@std@@QEAAXAEAU12@@Z */
DEFINE_THISCALL_WRAPPER(_Container_base12__Swap_all, 8)
void __thiscall _Container_base12__Swap_all(
        _Container_base12 *this, _Container_base12 *that)
{
    _Container_proxy *tmp;

    tmp = this->proxy;
    this->proxy = that->proxy;
    that->proxy = tmp;

    if(this->proxy)
        this->proxy->cont = this;
    if(that->proxy)
        that->proxy->cont = that;
}

#if _MSVCP_VER >= 110

#define SECSPERDAY 86400
/* 1601 to 1970 is 369 years plus 89 leap days */
#define SECS_1601_TO_1970 ((369 * 365 + 89) * (ULONGLONG)SECSPERDAY)
#define TICKSPERSEC 10000000
#define TICKS_1601_TO_1970 (SECS_1601_TO_1970 * TICKSPERSEC)
#define NANOSEC_PER_MILLISEC 1000000
#define MILLISEC_PER_SEC 1000
#define NANOSEC_PER_SEC (NANOSEC_PER_MILLISEC * MILLISEC_PER_SEC)

typedef int MSVCRT_long;

/* xtime */
typedef struct {
    __time64_t sec;
    MSVCRT_long nsec;
} xtime;

/* _Xtime_get_ticks */
LONGLONG __cdecl _Xtime_get_ticks(void)
{
    FILETIME ft;

    TRACE("\n");

    GetSystemTimeAsFileTime(&ft);
    return ((LONGLONG)ft.dwHighDateTime<<32) + ft.dwLowDateTime - TICKS_1601_TO_1970;
}

/* _xtime_get */
int __cdecl xtime_get(xtime* t, int unknown)
{
    LONGLONG ticks;

    TRACE("(%p)\n", t);

    if(unknown != 1)
        return 0;

    ticks = _Xtime_get_ticks();
    t->sec = ticks / TICKSPERSEC;
    t->nsec = ticks % TICKSPERSEC * 100;
    return 1;
}

/* _Xtime_diff_to_millis2 */
MSVCRT_long __cdecl _Xtime_diff_to_millis2(const xtime *t1, const xtime *t2)
{
    LONGLONG diff_sec, diff_nsec;

    TRACE("(%p, %p)\n", t1, t2);

    diff_sec = t1->sec - t2->sec;
    diff_nsec = t1->nsec - t2->nsec;

    diff_sec += diff_nsec / NANOSEC_PER_SEC;
    diff_nsec %= NANOSEC_PER_SEC;
    if (diff_nsec < 0) {
        diff_sec -= 1;
        diff_nsec += NANOSEC_PER_SEC;
    }

    if (diff_sec<0 || (diff_sec==0 && diff_nsec<0))
        return 0;
    return diff_sec * MILLISEC_PER_SEC +
        (diff_nsec + NANOSEC_PER_MILLISEC - 1) / NANOSEC_PER_MILLISEC;
}

/* _Xtime_diff_to_millis */
MSVCRT_long __cdecl _Xtime_diff_to_millis(const xtime *t)
{
    xtime now;

    TRACE("%p\n", t);

    xtime_get(&now, 1);
    return _Xtime_diff_to_millis2(t, &now);
}
#endif

#if _MSVCP_VER >= 90
unsigned int __cdecl _Random_device(void)
{
    unsigned int ret;

    TRACE("\n");

    /* TODO: throw correct exception in case of failure */
    if(rand_s(&ret))
        throw_exception(EXCEPTION, "random number generator failed\n");
    return ret;
}
#endif

#if _MSVCP_VER >= 110
#ifdef __ASM_USE_THISCALL_WRAPPER

extern void *call_thiscall_func;
__ASM_GLOBAL_FUNC(call_thiscall_func,
        "popl %eax\n\t"
        "popl %edx\n\t"
        "popl %ecx\n\t"
        "pushl %eax\n\t"
        "jmp *%edx\n\t")

#define call_func1(func,this) ((void* (WINAPI*)(void*,void*))&call_thiscall_func)(func,this)

#else /* __i386__ */

#define call_func1(func,this) func(this)

#endif /* __i386__ */

#define MTX_PLAIN 0x1
#define MTX_TRY 0x2
#define MTX_TIMED 0x4
#define MTX_RECURSIVE 0x100
#define MTX_LOCKED 3
typedef struct
{
    DWORD flags;
    critical_section cs;
    DWORD thread_id;
    DWORD count;
} *_Mtx_t;

#if _MSVCP_VER >= 140
typedef _Mtx_t _Mtx_arg_t;
#define MTX_T_FROM_ARG(m)   (m)
#define MTX_T_TO_ARG(m)     (m)
#else
typedef _Mtx_t *_Mtx_arg_t;
#define MTX_T_FROM_ARG(m)   (*(m))
#define MTX_T_TO_ARG(m)     (&(m))
#endif

void __cdecl _Mtx_init_in_situ(_Mtx_t mtx, int flags)
{
    if(flags & ~(MTX_PLAIN | MTX_TRY | MTX_TIMED | MTX_RECURSIVE))
        FIXME("unknown flags ignored: %x\n", flags);

    mtx->flags = flags;
    call_func1(critical_section_ctor, &mtx->cs);
    mtx->thread_id = -1;
    mtx->count = 0;
}

int __cdecl _Mtx_init(_Mtx_t *mtx, int flags)
{
    *mtx = MSVCRT_operator_new(sizeof(**mtx));
    _Mtx_init_in_situ(*mtx, flags);
    return 0;
}

void __cdecl _Mtx_destroy_in_situ(_Mtx_t mtx)
{
    call_func1(critical_section_dtor, &mtx->cs);
}

void __cdecl _Mtx_destroy(_Mtx_arg_t mtx)
{
    call_func1(critical_section_dtor, &MTX_T_FROM_ARG(mtx)->cs);
    MSVCRT_operator_delete(MTX_T_FROM_ARG(mtx));
}

int __cdecl _Mtx_current_owns(_Mtx_arg_t mtx)
{
    return MTX_T_FROM_ARG(mtx)->thread_id == GetCurrentThreadId();
}

int __cdecl _Mtx_lock(_Mtx_arg_t mtx)
{
    if(MTX_T_FROM_ARG(mtx)->thread_id != GetCurrentThreadId()) {
        call_func1(critical_section_lock, &MTX_T_FROM_ARG(mtx)->cs);
        MTX_T_FROM_ARG(mtx)->thread_id = GetCurrentThreadId();
    }else if(!(MTX_T_FROM_ARG(mtx)->flags & MTX_RECURSIVE)
            && MTX_T_FROM_ARG(mtx)->flags != MTX_PLAIN) {
        return MTX_LOCKED;
    }

    MTX_T_FROM_ARG(mtx)->count++;
    return 0;
}

int __cdecl _Mtx_unlock(_Mtx_arg_t mtx)
{
    if(--MTX_T_FROM_ARG(mtx)->count)
        return 0;

    MTX_T_FROM_ARG(mtx)->thread_id = -1;
    call_func1(critical_section_unlock, &MTX_T_FROM_ARG(mtx)->cs);
    return 0;
}

int __cdecl _Mtx_trylock(_Mtx_arg_t mtx)
{
    if(MTX_T_FROM_ARG(mtx)->thread_id != GetCurrentThreadId()) {
        if(!call_func1(critical_section_trylock, &MTX_T_FROM_ARG(mtx)->cs))
            return MTX_LOCKED;
        MTX_T_FROM_ARG(mtx)->thread_id = GetCurrentThreadId();
    }else if(!(MTX_T_FROM_ARG(mtx)->flags & MTX_RECURSIVE)
            && MTX_T_FROM_ARG(mtx)->flags != MTX_PLAIN) {
        return MTX_LOCKED;
    }

    MTX_T_FROM_ARG(mtx)->count++;
    return 0;
}

critical_section* __cdecl _Mtx_getconcrtcs(_Mtx_arg_t mtx)
{
    return &MTX_T_FROM_ARG(mtx)->cs;
}

static inline LONG interlocked_dec_if_nonzero( LONG *dest )
{
    LONG val, tmp;
    for (val = *dest;; val = tmp)
    {
        if (!val || (tmp = InterlockedCompareExchange( dest, val - 1, val )) == val)
            break;
    }
    return val;
}

#define CND_TIMEDOUT 2

typedef struct
{
    CONDITION_VARIABLE cv;
} *_Cnd_t;

#if _MSVCP_VER >= 140
typedef _Cnd_t _Cnd_arg_t;
#define CND_T_FROM_ARG(c)   (c)
#define CND_T_TO_ARG(c)     (c)
#else
typedef _Cnd_t *_Cnd_arg_t;
#define CND_T_FROM_ARG(c)   (*(c))
#define CND_T_TO_ARG(c)     (&(c))
#endif

static HANDLE keyed_event;

void __cdecl _Cnd_init_in_situ(_Cnd_t cnd)
{
    InitializeConditionVariable(&cnd->cv);

    if(!keyed_event) {
        HANDLE event;

        NtCreateKeyedEvent(&event, GENERIC_READ|GENERIC_WRITE, NULL, 0);
        if(InterlockedCompareExchangePointer(&keyed_event, event, NULL) != NULL)
            NtClose(event);
    }
}

int __cdecl _Cnd_init(_Cnd_t *cnd)
{
    *cnd = MSVCRT_operator_new(sizeof(**cnd));
    _Cnd_init_in_situ(*cnd);
    return 0;
}

int __cdecl _Cnd_wait(_Cnd_arg_t cnd, _Mtx_arg_t mtx)
{
    CONDITION_VARIABLE *cv = &CND_T_FROM_ARG(cnd)->cv;

    InterlockedExchangeAdd( (LONG *)&cv->Ptr, 1 );
    _Mtx_unlock(mtx);

    NtWaitForKeyedEvent(keyed_event, &cv->Ptr, FALSE, NULL);

    _Mtx_lock(mtx);
    return 0;
}

int __cdecl _Cnd_timedwait(_Cnd_arg_t cnd, _Mtx_arg_t mtx, const xtime *xt)
{
    CONDITION_VARIABLE *cv = &CND_T_FROM_ARG(cnd)->cv;
    LARGE_INTEGER timeout;
    NTSTATUS status;

    InterlockedExchangeAdd( (LONG *)&cv->Ptr, 1 );
    _Mtx_unlock(mtx);

    timeout.QuadPart = (ULONGLONG)(ULONG)_Xtime_diff_to_millis(xt) * -10000;
    status = NtWaitForKeyedEvent(keyed_event, &cv->Ptr, FALSE, &timeout);
    if (status)
    {
        if (!interlocked_dec_if_nonzero( (LONG *)&cv->Ptr ))
            status = NtWaitForKeyedEvent( keyed_event, &cv->Ptr, FALSE, NULL );
    }

    _Mtx_lock(mtx);
    return status ? CND_TIMEDOUT : 0;
}

int __cdecl _Cnd_broadcast(_Cnd_arg_t cnd)
{
    CONDITION_VARIABLE *cv = &CND_T_FROM_ARG(cnd)->cv;
    LONG val = InterlockedExchange( (LONG *)&cv->Ptr, 0 );
    while (val-- > 0)
        NtReleaseKeyedEvent( keyed_event, &cv->Ptr, FALSE, NULL );
    return 0;
}

int __cdecl _Cnd_signal(_Cnd_arg_t cnd)
{
    CONDITION_VARIABLE *cv = &CND_T_FROM_ARG(cnd)->cv;
    if (interlocked_dec_if_nonzero( (LONG *)&cv->Ptr ))
        NtReleaseKeyedEvent( keyed_event, &cv->Ptr, FALSE, NULL );
    return 0;
}

void __cdecl _Cnd_destroy_in_situ(_Cnd_t cnd)
{
    _Cnd_broadcast(CND_T_TO_ARG(cnd));
}

void __cdecl _Cnd_destroy(_Cnd_arg_t cnd)
{
    if(cnd) {
        _Cnd_broadcast(cnd);
        MSVCRT_operator_delete(CND_T_FROM_ARG(cnd));
    }
}

static struct {
    int used;
    int size;

    struct _to_broadcast {
        DWORD thread_id;
        _Cnd_arg_t cnd;
        _Mtx_arg_t mtx;
        int *p;
    } *to_broadcast;
} broadcast_at_thread_exit;

static CRITICAL_SECTION broadcast_at_thread_exit_cs;
static CRITICAL_SECTION_DEBUG broadcast_at_thread_exit_cs_debug =
{
        0, 0, &broadcast_at_thread_exit_cs,
        { &broadcast_at_thread_exit_cs_debug.ProcessLocksList, &broadcast_at_thread_exit_cs_debug.ProcessLocksList },
        0, 0, { (DWORD_PTR)(__FILE__ ": broadcast_at_thread_exit_cs") }
};
static CRITICAL_SECTION broadcast_at_thread_exit_cs = { &broadcast_at_thread_exit_cs_debug, -1, 0, 0, 0, 0 };

void __cdecl _Cnd_register_at_thread_exit(_Cnd_arg_t cnd, _Mtx_arg_t mtx, int *p)
{
    struct _to_broadcast *add;

    TRACE("(%p %p %p)\n", cnd, mtx, p);

    EnterCriticalSection(&broadcast_at_thread_exit_cs);
    if(!broadcast_at_thread_exit.size) {
        broadcast_at_thread_exit.to_broadcast = HeapAlloc(GetProcessHeap(),
                0, 8*sizeof(broadcast_at_thread_exit.to_broadcast[0]));
        if(!broadcast_at_thread_exit.to_broadcast) {
            LeaveCriticalSection(&broadcast_at_thread_exit_cs);
            return;
        }
        broadcast_at_thread_exit.size = 8;
    } else if(broadcast_at_thread_exit.size == broadcast_at_thread_exit.used) {
        add = HeapReAlloc(GetProcessHeap(), 0, broadcast_at_thread_exit.to_broadcast,
                broadcast_at_thread_exit.size*2*sizeof(broadcast_at_thread_exit.to_broadcast[0]));
        if(!add) {
            LeaveCriticalSection(&broadcast_at_thread_exit_cs);
            return;
        }
        broadcast_at_thread_exit.to_broadcast = add;
        broadcast_at_thread_exit.size *= 2;
    }

    add = broadcast_at_thread_exit.to_broadcast + broadcast_at_thread_exit.used++;
    add->thread_id = GetCurrentThreadId();
    add->cnd = cnd;
    add->mtx = mtx;
    add->p = p;
    LeaveCriticalSection(&broadcast_at_thread_exit_cs);
}

void __cdecl _Cnd_unregister_at_thread_exit(_Mtx_arg_t mtx)
{
    int i;

    TRACE("(%p)\n", mtx);

    EnterCriticalSection(&broadcast_at_thread_exit_cs);
    for(i=0; i<broadcast_at_thread_exit.used; i++) {
        if(broadcast_at_thread_exit.to_broadcast[i].mtx != mtx)
            continue;

        memmove(broadcast_at_thread_exit.to_broadcast+i, broadcast_at_thread_exit.to_broadcast+i+1,
                (broadcast_at_thread_exit.used-i-1)*sizeof(broadcast_at_thread_exit.to_broadcast[0]));
        broadcast_at_thread_exit.used--;
        i--;
    }
    LeaveCriticalSection(&broadcast_at_thread_exit_cs);
}

void __cdecl _Cnd_do_broadcast_at_thread_exit(void)
{
    int i, id = GetCurrentThreadId();

    TRACE("()\n");

    EnterCriticalSection(&broadcast_at_thread_exit_cs);
    for(i=0; i<broadcast_at_thread_exit.used; i++) {
        if(broadcast_at_thread_exit.to_broadcast[i].thread_id != id)
            continue;

        _Mtx_unlock(broadcast_at_thread_exit.to_broadcast[i].mtx);
        _Cnd_broadcast(broadcast_at_thread_exit.to_broadcast[i].cnd);
        if(broadcast_at_thread_exit.to_broadcast[i].p)
            *broadcast_at_thread_exit.to_broadcast[i].p = 1;

        memmove(broadcast_at_thread_exit.to_broadcast+i, broadcast_at_thread_exit.to_broadcast+i+1,
                (broadcast_at_thread_exit.used-i-1)*sizeof(broadcast_at_thread_exit.to_broadcast[0]));
        broadcast_at_thread_exit.used--;
        i--;
    }
    LeaveCriticalSection(&broadcast_at_thread_exit_cs);
}

#endif

#if _MSVCP_VER == 100
typedef struct {
    const vtable_ptr *vtable;
} error_category;

typedef struct {
    error_category base;
    const char *type;
} custom_category;
static custom_category iostream_category;

DEFINE_RTTI_DATA0(error_category, 0, ".?AVerror_category@std@@")
DEFINE_RTTI_DATA1(iostream_category, 0, &error_category_rtti_base_descriptor, ".?AV_Iostream_error_category@std@@")

extern const vtable_ptr MSVCP_iostream_category_vtable;

static void iostream_category_ctor(custom_category *this)
{
    this->base.vtable = &MSVCP_iostream_category_vtable;
    this->type = "iostream";
}

DEFINE_THISCALL_WRAPPER(custom_category_vector_dtor, 8)
custom_category* __thiscall custom_category_vector_dtor(custom_category *this, unsigned int flags)
{
    TRACE("(%p %x)\n", this, flags);
    if(flags & 2) {
        /* we have an array, with the number of elements stored before the first object */
        INT_PTR i, *ptr = (INT_PTR *)this-1;

        for(i=*ptr-1; i>=0; i--)
            MSVCRT_operator_delete(ptr);
    } else {
        if(flags & 1)
            MSVCRT_operator_delete(this);
    }

    return this;
}

DEFINE_THISCALL_WRAPPER(custom_category_name, 4)
const char* __thiscall custom_category_name(const custom_category *this)
{
    return this->type;
}

DEFINE_THISCALL_WRAPPER(custom_category_message, 12)
basic_string_char* __thiscall custom_category_message(const custom_category *this,
        basic_string_char *ret, int err)
{
    return MSVCP_basic_string_char_ctor_cstr(ret, strerror(err));
}

DEFINE_THISCALL_WRAPPER(custom_category_default_error_condition, 12)
/*error_condition*/void* __thiscall custom_category_default_error_condition(
        custom_category *this, /*error_condition*/void *ret, int code)
{
    FIXME("(%p %p %x) stub\n", this, ret, code);
    return NULL;
}

DEFINE_THISCALL_WRAPPER(custom_category_equivalent, 12)
bool __thiscall custom_category_equivalent(const custom_category *this,
        int code, const /*error_condition*/void *condition)
{
    FIXME("(%p %x %p) stub\n", this, code, condition);
    return FALSE;
}

DEFINE_THISCALL_WRAPPER(custom_category_equivalent_code, 12)
bool __thiscall custom_category_equivalent_code(custom_category *this,
        const /*error_code*/void *code, int condition)
{
    FIXME("(%p %p %x) stub\n", this, code, condition);
    return FALSE;
}

DEFINE_THISCALL_WRAPPER(iostream_category_message, 12)
basic_string_char* __thiscall iostream_category_message(const custom_category *this,
        basic_string_char *ret, int err)
{
    if(err == 1) return MSVCP_basic_string_char_ctor_cstr(ret, "iostream error");
    return MSVCP_basic_string_char_ctor_cstr(ret, strerror(err));
}

/* ?iostream_category@std@@YAABVerror_category@1@XZ */
/* ?iostream_category@std@@YAAEBVerror_category@1@XZ */
const error_category* __cdecl std_iostream_category(void)
{
    TRACE("()\n");
    return &iostream_category.base;
}

static custom_category system_category;
DEFINE_RTTI_DATA1(system_category, 0, &error_category_rtti_base_descriptor, ".?AV_System_error_category@std@@")

extern const vtable_ptr MSVCP_system_category_vtable;

static void system_category_ctor(custom_category *this)
{
    this->base.vtable = &MSVCP_system_category_vtable;
    this->type = "system";
}

/* ?system_category@std@@YAABVerror_category@1@XZ */
/* ?system_category@std@@YAAEBVerror_category@1@XZ */
const error_category* __cdecl std_system_category(void)
{
    TRACE("()\n");
    return &system_category.base;
}

static custom_category generic_category;
DEFINE_RTTI_DATA1(generic_category, 0, &error_category_rtti_base_descriptor, ".?AV_Generic_error_category@std@@")

extern const vtable_ptr MSVCP_generic_category_vtable;

static void generic_category_ctor(custom_category *this)
{
    this->base.vtable = &MSVCP_generic_category_vtable;
    this->type = "generic";
}

/* ?generic_category@std@@YAABVerror_category@1@XZ */
/* ?generic_category@std@@YAAEBVerror_category@1@XZ */
const error_category* __cdecl std_generic_category(void)
{
    TRACE("()\n");
    return &generic_category.base;
}
#endif

#if _MSVCP_VER >= 110
static CRITICAL_SECTION call_once_cs;
static CRITICAL_SECTION_DEBUG call_once_cs_debug =
{
    0, 0, &call_once_cs,
    { &call_once_cs_debug.ProcessLocksList, &call_once_cs_debug.ProcessLocksList },
    0, 0, { (DWORD_PTR)(__FILE__ ": call_once_cs") }
};
static CRITICAL_SECTION call_once_cs = { &call_once_cs_debug, -1, 0, 0, 0, 0 };

void __cdecl _Call_onceEx(int *once, void (__cdecl *func)(void*), void *argv)
{
    TRACE("%p %p %p\n", once, func, argv);

    EnterCriticalSection(&call_once_cs);
    if(!*once) {
        /* FIXME: handle exceptions */
        func(argv);
        *once = 1;
    }
    LeaveCriticalSection(&call_once_cs);
}

static void __cdecl call_once_func_wrapper(void *func)
{
    ((void (__cdecl*)(void))func)();
}

void __cdecl _Call_once(int *once, void (__cdecl *func)(void))
{
    TRACE("%p %p\n", once, func);
    _Call_onceEx(once, call_once_func_wrapper, func);
}

void __cdecl _Do_call(void *this)
{
    CALL_VTBL_FUNC(this, 0, void, (void*), (this));
}
#endif

#if _MSVCP_VER >= 110
typedef struct
{
    HANDLE hnd;
    DWORD  id;
} _Thrd_t;

typedef int (__cdecl *_Thrd_start_t)(void*);

#define _THRD_ERROR 4

int __cdecl _Thrd_equal(_Thrd_t a, _Thrd_t b)
{
    TRACE("(%p %u %p %u)\n", a.hnd, a.id, b.hnd, b.id);
    return a.id == b.id;
}

int __cdecl _Thrd_lt(_Thrd_t a, _Thrd_t b)
{
    TRACE("(%p %u %p %u)\n", a.hnd, a.id, b.hnd, b.id);
    return a.id < b.id;
}

void __cdecl _Thrd_sleep(const xtime *t)
{
    TRACE("(%p)\n", t);
    Sleep(_Xtime_diff_to_millis(t));
}

void __cdecl _Thrd_yield(void)
{
    TRACE("()\n");
    Sleep(0);
}

static _Thrd_t thread_current(void)
{
    _Thrd_t ret;

    if(DuplicateHandle(GetCurrentProcess(), GetCurrentThread(),
                       GetCurrentProcess(), &ret.hnd, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
        CloseHandle(ret.hnd);
    } else {
        ret.hnd = 0;
    }
    ret.id  = GetCurrentThreadId();

    TRACE("(%p %u)\n", ret.hnd, ret.id);
    return ret;
}

#ifndef __i386__
_Thrd_t __cdecl _Thrd_current(void)
{
    return thread_current();
}
#else
ULONGLONG __cdecl _Thrd_current(void)
{
    union {
        _Thrd_t thr;
        ULONGLONG ull;
    } ret;

    C_ASSERT(sizeof(_Thrd_t) <= sizeof(ULONGLONG));

    ret.thr = thread_current();
    return ret.ull;
}
#endif

int __cdecl _Thrd_join(_Thrd_t thr, int *code)
{
    TRACE("(%p %u %p)\n", thr.hnd, thr.id, code);
    if (WaitForSingleObject(thr.hnd, INFINITE))
        return _THRD_ERROR;

    if (code)
        GetExitCodeThread(thr.hnd, (DWORD *)code);

    CloseHandle(thr.hnd);
    return 0;
}

int __cdecl _Thrd_start(_Thrd_t *thr, LPTHREAD_START_ROUTINE proc, void *arg)
{
    TRACE("(%p %p %p)\n", thr, proc, arg);
    thr->hnd = CreateThread(NULL, 0, proc, arg, 0, &thr->id);
    return thr->hnd ? 0 : _THRD_ERROR;
}

typedef struct
{
    _Thrd_start_t proc;
    void *arg;
} thread_proc_arg;

static DWORD WINAPI thread_proc_wrapper(void *arg)
{
    thread_proc_arg wrapped_arg = *((thread_proc_arg*)arg);
    free(arg);
    return wrapped_arg.proc(wrapped_arg.arg);
}

int __cdecl _Thrd_create(_Thrd_t *thr, _Thrd_start_t proc, void *arg)
{
    thread_proc_arg *wrapped_arg;
    int ret;

    TRACE("(%p %p %p)\n", thr, proc, arg);

    wrapped_arg = malloc(sizeof(*wrapped_arg));
    if(!wrapped_arg)
        return _THRD_ERROR; /* TODO: probably different error should be returned here */

    wrapped_arg->proc = proc;
    wrapped_arg->arg = arg;
    ret = _Thrd_start(thr, thread_proc_wrapper, wrapped_arg);
    if(ret) free(wrapped_arg);
    return ret;
}

int __cdecl _Thrd_detach(_Thrd_t thr)
{
    return CloseHandle(thr.hnd) ? 0 : _THRD_ERROR;
}

typedef struct
{
    const vtable_ptr *vtable;
    _Cnd_t cnd;
    _Mtx_t mtx;
    bool launched;
} _Pad;

DEFINE_RTTI_DATA0(_Pad, 0, ".?AV_Pad@std@@")

/* ??_7_Pad@std@@6B@ */
extern const vtable_ptr MSVCP__Pad_vtable;

unsigned int __cdecl _Thrd_hardware_concurrency(void)
{
    static unsigned int val = -1;

    TRACE("()\n");

    if(val == -1) {
        SYSTEM_INFO si;

        GetSystemInfo(&si);
        val = si.dwNumberOfProcessors;
    }

    return val;
}

unsigned int __cdecl _Thrd_id(void)
{
    TRACE("()\n");
    return GetCurrentThreadId();
}

/* ??0_Pad@std@@QAE@XZ */
/* ??0_Pad@std@@QEAA@XZ */
DEFINE_THISCALL_WRAPPER(_Pad_ctor, 4)
_Pad* __thiscall _Pad_ctor(_Pad *this)
{
    TRACE("(%p)\n", this);

    this->vtable = &MSVCP__Pad_vtable;
    _Cnd_init(&this->cnd);
    _Mtx_init(&this->mtx, 0);
    this->launched = FALSE;
    _Mtx_lock(MTX_T_TO_ARG(this->mtx));
    return this;
}

/* ??4_Pad@std@@QAEAAV01@ABV01@@Z */
/* ??4_Pad@std@@QEAAAEAV01@AEBV01@@Z */
DEFINE_THISCALL_WRAPPER(_Pad_op_assign, 8)
_Pad* __thiscall _Pad_op_assign(_Pad *this, const _Pad *copy)
{
    TRACE("(%p %p)\n", this, copy);

    this->cnd = copy->cnd;
    this->mtx = copy->mtx;
    this->launched = copy->launched;
    return this;
}

/* ??0_Pad@std@@QAE@ABV01@@Z */
/* ??0_Pad@std@@QEAA@AEBV01@@Z */
DEFINE_THISCALL_WRAPPER(_Pad_copy_ctor, 8)
_Pad* __thiscall _Pad_copy_ctor(_Pad *this, const _Pad *copy)
{
    TRACE("(%p %p)\n", this, copy);

    this->vtable = &MSVCP__Pad_vtable;
    return _Pad_op_assign(this, copy);
}

/* ??1_Pad@std@@QAE@XZ */
/* ??1_Pad@std@@QEAA@XZ */
DEFINE_THISCALL_WRAPPER(_Pad_dtor, 4)
void __thiscall _Pad_dtor(_Pad *this)
{
    TRACE("(%p)\n", this);

    _Mtx_unlock(MTX_T_TO_ARG(this->mtx));
    _Mtx_destroy(MTX_T_TO_ARG(this->mtx));
    _Cnd_destroy(CND_T_TO_ARG(this->cnd));
}

DEFINE_THISCALL_WRAPPER(_Pad__Go, 4)
#define call__Pad__Go(this) CALL_VTBL_FUNC(this, 0, unsigned int, (_Pad*), (this))
unsigned int __thiscall _Pad__Go(_Pad *this)
{
    ERR("(%p) should not be called\n", this);
    return 0;
}

static DWORD WINAPI launch_thread_proc(void *arg)
{
    _Pad *this = arg;
    return call__Pad__Go(this);
}

/* ?_Launch@_Pad@std@@QAEXPAU_Thrd_imp_t@@@Z */
/* ?_Launch@_Pad@std@@QEAAXPEAU_Thrd_imp_t@@@Z */
DEFINE_THISCALL_WRAPPER(_Pad__Launch, 8)
void __thiscall _Pad__Launch(_Pad *this, _Thrd_t *thr)
{
    TRACE("(%p %p)\n", this, thr);

    _Thrd_start(thr, launch_thread_proc, this);
    _Cnd_wait(CND_T_TO_ARG(this->cnd), MTX_T_TO_ARG(this->mtx));
}

/* ?_Release@_Pad@std@@QAEXXZ */
/* ?_Release@_Pad@std@@QEAAXXZ */
DEFINE_THISCALL_WRAPPER(_Pad__Release, 4)
void __thiscall _Pad__Release(_Pad *this)
{
    TRACE("(%p)\n", this);

    _Mtx_lock(MTX_T_TO_ARG(this->mtx));
    this->launched = TRUE;
    _Cnd_signal(CND_T_TO_ARG(this->cnd));
    _Mtx_unlock(MTX_T_TO_ARG(this->mtx));
}
#endif

#if _MSVCP_VER >= 100
typedef struct _Page
{
    struct _Page *_Next;
    size_t _Mask;
    char data[1];
} _Page;

typedef struct
{
    LONG lock;
    _Page *head;
    _Page *tail;
    size_t head_pos;
    size_t tail_pos;
} threadsafe_queue;

#define QUEUES_NO 8
typedef struct
{
    size_t tail_pos;
    size_t head_pos;
    threadsafe_queue queues[QUEUES_NO];
} queue_data;

typedef struct
{
    const vtable_ptr *vtable;
    queue_data *data; /* queue_data structure is not binary compatible */
    size_t alloc_count;
    size_t item_size;
} _Concurrent_queue_base_v4;

extern const vtable_ptr MSVCP__Concurrent_queue_base_v4_vtable;
#if _MSVCP_VER == 100
#define call__Concurrent_queue_base_v4__Move_item call__Concurrent_queue_base_v4__Copy_item
#define call__Concurrent_queue_base_v4__Copy_item(this,dst,idx,src) CALL_VTBL_FUNC(this, \
        0, void, (_Concurrent_queue_base_v4*,_Page*,size_t,const void*), (this,dst,idx,src))
#define  call__Concurrent_queue_base_v4__Assign_and_destroy_item(this,dst,src,idx) CALL_VTBL_FUNC(this, \
        4, void, (_Concurrent_queue_base_v4*,void*,_Page*,size_t), (this,dst,src,idx))
#define call__Concurrent_queue_base_v4__Allocate_page(this) CALL_VTBL_FUNC(this, \
        12, _Page*, (_Concurrent_queue_base_v4*), (this))
#define call__Concurrent_queue_base_v4__Deallocate_page(this, page) CALL_VTBL_FUNC(this, \
        16, void, (_Concurrent_queue_base_v4*,_Page*), (this,page))
#else
#define call__Concurrent_queue_base_v4__Move_item(this,dst,idx,src) CALL_VTBL_FUNC(this, \
        0, void, (_Concurrent_queue_base_v4*,_Page*,size_t,void*), (this,dst,idx,src))
#define call__Concurrent_queue_base_v4__Copy_item(this,dst,idx,src) CALL_VTBL_FUNC(this, \
        4, void, (_Concurrent_queue_base_v4*,_Page*,size_t,const void*), (this,dst,idx,src))
#define  call__Concurrent_queue_base_v4__Assign_and_destroy_item(this,dst,src,idx) CALL_VTBL_FUNC(this, \
        8, void, (_Concurrent_queue_base_v4*,void*,_Page*,size_t), (this,dst,src,idx))
#define call__Concurrent_queue_base_v4__Allocate_page(this) CALL_VTBL_FUNC(this, \
        16, _Page*, (_Concurrent_queue_base_v4*), (this))
#define call__Concurrent_queue_base_v4__Deallocate_page(this, page) CALL_VTBL_FUNC(this, \
        20, void, (_Concurrent_queue_base_v4*,_Page*), (this,page))
#endif

/* ?_Internal_throw_exception@_Concurrent_queue_base_v4@details@Concurrency@@IBEXXZ */
/* ?_Internal_throw_exception@_Concurrent_queue_base_v4@details@Concurrency@@IEBAXXZ */
DEFINE_THISCALL_WRAPPER(_Concurrent_queue_base_v4__Internal_throw_exception, 4)
void __thiscall _Concurrent_queue_base_v4__Internal_throw_exception(
        const _Concurrent_queue_base_v4 *this)
{
    TRACE("(%p)\n", this);
    throw_exception(EXCEPTION_BAD_ALLOC, NULL);
}

/* ??0_Concurrent_queue_base_v4@details@Concurrency@@IAE@I@Z */
/* ??0_Concurrent_queue_base_v4@details@Concurrency@@IEAA@_K@Z */
DEFINE_THISCALL_WRAPPER(_Concurrent_queue_base_v4_ctor, 8)
_Concurrent_queue_base_v4* __thiscall _Concurrent_queue_base_v4_ctor(
        _Concurrent_queue_base_v4 *this, size_t size)
{
    TRACE("(%p %Iu)\n", this, size);

    this->data = MSVCRT_operator_new(sizeof(*this->data));
    memset(this->data, 0, sizeof(*this->data));

    this->vtable = &MSVCP__Concurrent_queue_base_v4_vtable;
    this->item_size = size;

    /* alloc_count needs to be power of 2 */
    this->alloc_count =
        size <= 8 ? 32 :
        size <= 16 ? 16 :
        size <= 32 ? 8 :
        size <= 64 ? 4 :
        size <= 128 ? 2 : 1;
    return this;
}

/* ??1_Concurrent_queue_base_v4@details@Concurrency@@MAE@XZ */
/* ??1_Concurrent_queue_base_v4@details@Concurrency@@MEAA@XZ */
DEFINE_THISCALL_WRAPPER(_Concurrent_queue_base_v4_dtor, 4)
void __thiscall _Concurrent_queue_base_v4_dtor(_Concurrent_queue_base_v4 *this)
{
    TRACE("(%p)\n", this);
    MSVCRT_operator_delete(this->data);
}

DEFINE_THISCALL_WRAPPER(_Concurrent_queue_base_v4_vector_dtor, 8)
_Concurrent_queue_base_v4* __thiscall _Concurrent_queue_base_v4_vector_dtor(
        _Concurrent_queue_base_v4 *this, unsigned int flags)
{
    TRACE("(%p %x)\n", this, flags);
    if(flags & 2) {
        /* we have an array, with the number of elements stored before the first object */
        INT_PTR i, *ptr = (INT_PTR *)this-1;

        for(i=*ptr-1; i>=0; i--)
            _Concurrent_queue_base_v4_dtor(this+i);
        MSVCRT_operator_delete(ptr);
    } else {
        if(flags & 1)
            _Concurrent_queue_base_v4_dtor(this);
        MSVCRT_operator_delete(this);
    }

    return this;
}

/* ?_Internal_finish_clear@_Concurrent_queue_base_v4@details@Concurrency@@IAEXXZ */
/* ?_Internal_finish_clear@_Concurrent_queue_base_v4@details@Concurrency@@IEAAXXZ */
DEFINE_THISCALL_WRAPPER(_Concurrent_queue_base_v4__Internal_finish_clear, 4)
void __thiscall _Concurrent_queue_base_v4__Internal_finish_clear(
        _Concurrent_queue_base_v4 *this)
{
    int i;

    TRACE("(%p)\n", this);

    for(i=0; i<QUEUES_NO; i++)
    {
        if(this->data->queues[i].tail)
            call__Concurrent_queue_base_v4__Deallocate_page(this, this->data->queues[i].tail);
    }
}

/* ?_Internal_empty@_Concurrent_queue_base_v4@details@Concurrency@@IBE_NXZ */
/* ?_Internal_empty@_Concurrent_queue_base_v4@details@Concurrency@@IEBA_NXZ */
DEFINE_THISCALL_WRAPPER(_Concurrent_queue_base_v4__Internal_empty, 4)
bool __thiscall _Concurrent_queue_base_v4__Internal_empty(
        const _Concurrent_queue_base_v4 *this)
{
    TRACE("(%p)\n", this);
    return this->data->head_pos == this->data->tail_pos;
}

/* ?_Internal_size@_Concurrent_queue_base_v4@details@Concurrency@@IBEIXZ */
/* ?_Internal_size@_Concurrent_queue_base_v4@details@Concurrency@@IEBA_KXZ */
DEFINE_THISCALL_WRAPPER(_Concurrent_queue_base_v4__Internal_size, 4)
size_t __thiscall _Concurrent_queue_base_v4__Internal_size(
        const _Concurrent_queue_base_v4 *this)
{
    TRACE("(%p)\n", this);
    return this->data->tail_pos - this->data->head_pos;
}

static void spin_wait(int *counter)
{
    static int spin_limit = -1;

    if(spin_limit == -1)
    {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        spin_limit = si.dwNumberOfProcessors>1 ? 4000 : 0;
    }

    if(*counter >= spin_limit)
    {
        *counter = 0;
        Sleep(0);
    }
    else
    {
        (*counter)++;
    }
}

#ifdef _WIN64
static size_t InterlockedIncrementSizeT(size_t volatile *dest)
{
    size_t v;

    do
    {
        v = *dest;
    } while(InterlockedCompareExchange64((LONGLONG*)dest, v+1, v) != v);

    return v+1;
}
#else
#define InterlockedIncrementSizeT(dest) InterlockedIncrement((LONG*)dest)
#endif

static void CALLBACK queue_push_finally(BOOL normal, void *ctx)
{
    threadsafe_queue *queue = ctx;
    InterlockedIncrementSizeT(&queue->tail_pos);
}

static void threadsafe_queue_push(threadsafe_queue *queue, size_t id,
        void *e, _Concurrent_queue_base_v4 *parent, BOOL copy)
{
    size_t page_id = id & ~(parent->alloc_count-1);
    int spin;
    _Page *p;

    spin = 0;
    while(queue->tail_pos != id)
        spin_wait(&spin);

    if(page_id == id)
    {
        /* TODO: Add exception handling */
        p = call__Concurrent_queue_base_v4__Allocate_page(parent);
        p->_Next = NULL;
        p->_Mask = 0;

        spin = 0;
        while(InterlockedCompareExchange(&queue->lock, 1, 0))
            spin_wait(&spin);
        if(queue->tail)
            queue->tail->_Next = p;
        queue->tail = p;
        if(!queue->head)
            queue->head = p;
        queue->lock = 0;
    }
    else
    {
        p = queue->tail;
    }

    __TRY
    {
        if(copy)
            call__Concurrent_queue_base_v4__Copy_item(parent, p, id-page_id, e);
        else
            call__Concurrent_queue_base_v4__Move_item(parent, p, id-page_id, e);
        p->_Mask |= 1 << (id - page_id);
    }
    __FINALLY_CTX(queue_push_finally, queue);
}

static BOOL threadsafe_queue_pop(threadsafe_queue *queue, size_t id,
        void *e, _Concurrent_queue_base_v4 *parent)
{
    size_t page_id = id & ~(parent->alloc_count-1);
    int spin;
    _Page *p;
    BOOL ret = FALSE;

    spin = 0;
    while(queue->tail_pos <= id)
        spin_wait(&spin);

    spin = 0;
    while(queue->head_pos != id)
        spin_wait(&spin);

    p = queue->head;
    if(p->_Mask & (1 << (id-page_id)))
    {
        /* TODO: Add exception handling */
        call__Concurrent_queue_base_v4__Assign_and_destroy_item(parent, e, p, id-page_id);
        ret = TRUE;
    }

    if(id == page_id+parent->alloc_count-1)
    {
        spin = 0;
        while(InterlockedCompareExchange(&queue->lock, 1, 0))
            spin_wait(&spin);
        queue->head = p->_Next;
        if(!queue->head)
            queue->tail = NULL;
        queue->lock = 0;

        /* TODO: Add exception handling */
        call__Concurrent_queue_base_v4__Deallocate_page(parent, p);
    }

    InterlockedIncrementSizeT(&queue->head_pos);
    return ret;
}

/* ?_Internal_push@_Concurrent_queue_base_v4@details@Concurrency@@IAEXPBX@Z */
/* ?_Internal_push@_Concurrent_queue_base_v4@details@Concurrency@@IEAAXPEBX@Z */
DEFINE_THISCALL_WRAPPER(_Concurrent_queue_base_v4__Internal_push, 8)
void __thiscall _Concurrent_queue_base_v4__Internal_push(
        _Concurrent_queue_base_v4 *this, void *e)
{
    size_t id;

    TRACE("(%p %p)\n", this, e);

    id = InterlockedIncrementSizeT(&this->data->tail_pos)-1;
    threadsafe_queue_push(this->data->queues + id % QUEUES_NO,
            id / QUEUES_NO, e, this, TRUE);
}

/* ?_Internal_move_push@_Concurrent_queue_base_v4@details@Concurrency@@IAEXPAX@Z */
/* ?_Internal_move_push@_Concurrent_queue_base_v4@details@Concurrency@@IEAAXPEAX@Z */
DEFINE_THISCALL_WRAPPER(_Concurrent_queue_base_v4__Internal_move_push, 8)
void __thiscall _Concurrent_queue_base_v4__Internal_move_push(
        _Concurrent_queue_base_v4 *this, void *e)
{
    size_t id;

    TRACE("(%p %p)\n", this, e);

    id = InterlockedIncrementSizeT(&this->data->tail_pos)-1;
    threadsafe_queue_push(this->data->queues + id % QUEUES_NO,
            id / QUEUES_NO, e, this, FALSE);
}

/* ?_Internal_pop_if_present@_Concurrent_queue_base_v4@details@Concurrency@@IAE_NPAX@Z */
/* ?_Internal_pop_if_present@_Concurrent_queue_base_v4@details@Concurrency@@IEAA_NPEAX@Z */
DEFINE_THISCALL_WRAPPER(_Concurrent_queue_base_v4__Internal_pop_if_present, 8)
bool __thiscall _Concurrent_queue_base_v4__Internal_pop_if_present(
        _Concurrent_queue_base_v4 *this, void *e)
{
    size_t id;

    TRACE("(%p %p)\n", this, e);

    do
    {
        do
        {
            id = this->data->head_pos;
            if(id == this->data->tail_pos) return FALSE;
        } while(InterlockedCompareExchangePointer((void**)&this->data->head_pos,
                    (void*)(id+1), (void*)id) != (void*)id);
    } while(!threadsafe_queue_pop(this->data->queues + id % QUEUES_NO,
                id / QUEUES_NO, e, this));
    return TRUE;
}

/* ?_Internal_swap@_Concurrent_queue_base_v4@details@Concurrency@@IAEXAAV123@@Z */
/* ?_Internal_swap@_Concurrent_queue_base_v4@details@Concurrency@@IEAAXAEAV123@@Z */
DEFINE_THISCALL_WRAPPER(_Concurrent_queue_base_v4__Internal_swap, 8)
void __thiscall _Concurrent_queue_base_v4__Internal_swap(
        _Concurrent_queue_base_v4 *this, _Concurrent_queue_base_v4 *r)
{
    FIXME("(%p %p) stub\n", this, r);
}

DEFINE_THISCALL_WRAPPER(_Concurrent_queue_base_v4_dummy, 4)
void __thiscall _Concurrent_queue_base_v4_dummy(_Concurrent_queue_base_v4 *this)
{
    ERR("unexpected call\n");
}

DEFINE_RTTI_DATA0(_Concurrent_queue_base_v4, 0, ".?AV_Concurrent_queue_base_v4@details@Concurrency@@")

static int _Runtime_object_id;

typedef struct
{
    const vtable_ptr *vtable;
    int id;
} _Runtime_object;

extern const vtable_ptr MSVCP__Runtime_object_vtable;

/* ??0_Runtime_object@details@Concurrency@@QAE@H@Z */
/* ??0_Runtime_object@details@Concurrency@@QEAA@H@Z */
DEFINE_THISCALL_WRAPPER(_Runtime_object_ctor_id, 8)
_Runtime_object* __thiscall _Runtime_object_ctor_id(_Runtime_object *this, int id)
{
    TRACE("(%p %d)\n", this, id);
    this->vtable = &MSVCP__Runtime_object_vtable;
    this->id = id;
    return this;
}

/* ??0_Runtime_object@details@Concurrency@@QAE@XZ */
/* ??0_Runtime_object@details@Concurrency@@QEAA@XZ */
DEFINE_THISCALL_WRAPPER(_Runtime_object_ctor, 4)
_Runtime_object* __thiscall _Runtime_object_ctor(_Runtime_object *this)
{
    TRACE("(%p)\n", this);
    this->vtable = &MSVCP__Runtime_object_vtable;
    this->id = InterlockedExchangeAdd(&_Runtime_object_id, 2);
    return this;
}

DEFINE_THISCALL_WRAPPER(_Runtime_object__GetId, 4)
int __thiscall _Runtime_object__GetId(_Runtime_object *this)
{
    TRACE("(%p)\n", this);
    return this->id;
}

DEFINE_RTTI_DATA0(_Runtime_object, 0, ".?AV_Runtime_object@details@Concurrency@@")

#endif

#if _MSVCP_VER >= 100
typedef struct __Concurrent_vector_base_v4
{
    void* (__cdecl *allocator)(struct __Concurrent_vector_base_v4 *, size_t);
    void *storage[3];
    size_t first_block;
    size_t early_size;
    void **segment;
} _Concurrent_vector_base_v4;

#define STORAGE_SIZE ARRAY_SIZE(this->storage)
#define SEGMENT_SIZE (sizeof(void*) * 8)

typedef struct compact_block
{
    size_t first_block;
    void *blocks[SEGMENT_SIZE];
    int size_check;
}compact_block;

/* Return the integer base-2 logarithm of (x|1). Result is 0 for x == 0. */
static inline unsigned int log2i(unsigned int x)
{
    unsigned int index;
    BitScanReverse(&index, x|1);
    return index;
}

/* ?_Segment_index_of@_Concurrent_vector_base_v4@details@Concurrency@@KAII@Z */
/* ?_Segment_index_of@_Concurrent_vector_base_v4@details@Concurrency@@KA_K_K@Z */
size_t __cdecl _vector_base_v4__Segment_index_of(size_t x)
{
    unsigned int half;

    TRACE("(%Iu)\n", x);

    if((sizeof(x) == 8) && (half = x >> 32))
        return log2i(half) + 32;

    return log2i(x);
}

/* ?_Internal_throw_exception@_Concurrent_vector_base_v4@details@Concurrency@@IBEXI@Z */
/* ?_Internal_throw_exception@_Concurrent_vector_base_v4@details@Concurrency@@IEBAX_K@Z */
DEFINE_THISCALL_WRAPPER(_vector_base_v4__Internal_throw_exception, 8)
void __thiscall _vector_base_v4__Internal_throw_exception(void/*_vector_base_v4*/ *this, size_t idx)
{
    static const struct {
        exception_type type;
        const char *msg;
    } exceptions[] = {
        { EXCEPTION_OUT_OF_RANGE, "Index out of range" },
        { EXCEPTION_OUT_OF_RANGE, "Index out of segments table range" },
        { EXCEPTION_RANGE_ERROR,  "Index is inside segment which failed to be allocated" },
    };

    TRACE("(%p %Iu)\n", this, idx);

    if(idx < ARRAY_SIZE(exceptions))
        throw_exception(exceptions[idx].type, exceptions[idx].msg);
}

#ifdef _WIN64
#define InterlockedCompareExchangeSizeT(dest, exchange, cmp) InterlockedCompareExchangeSize((size_t *)dest, (size_t)exchange, (size_t)cmp)
static size_t InterlockedCompareExchangeSize(size_t volatile *dest, size_t exchange, size_t cmp)
{
    size_t v;

    v = InterlockedCompareExchange64((LONGLONG*)dest, exchange, cmp);

    return v;
}
#else
#define InterlockedCompareExchangeSizeT(dest, exchange, cmp) InterlockedCompareExchange((LONG*)dest, (size_t)exchange, (size_t)cmp)
#endif

#define SEGMENT_ALLOC_MARKER ((void*)1)

static void concurrent_vector_alloc_segment(_Concurrent_vector_base_v4 *this,
        size_t seg, size_t element_size)
{
    int spin;

    while(!this->segment[seg] || this->segment[seg] == SEGMENT_ALLOC_MARKER)
    {
        spin = 0;
        while(this->segment[seg] == SEGMENT_ALLOC_MARKER)
            spin_wait(&spin);
        if(!InterlockedCompareExchangeSizeT((this->segment + seg),
                    SEGMENT_ALLOC_MARKER, 0))
        __TRY
        {
            if(seg == 0)
                this->segment[seg] = this->allocator(this, element_size * (1 << this->first_block));
            else if(seg < this->first_block)
                this->segment[seg] = (BYTE**)this->segment[0]
                        + element_size * (1 << seg);
            else
                this->segment[seg] = this->allocator(this, element_size * (1 << seg));
        }
        __EXCEPT_ALL
        {
            this->segment[seg] = NULL;
            throw_exception(EXCEPTION_RERAISE, NULL);
        }
        __ENDTRY
        if(!this->segment[seg])
            _vector_base_v4__Internal_throw_exception(this, 2);
    }
}

/* ??1_Concurrent_vector_base_v4@details@Concurrency@@IAE@XZ */
/* ??1_Concurrent_vector_base_v4@details@Concurrency@@IEAA@XZ */
DEFINE_THISCALL_WRAPPER(_Concurrent_vector_base_v4_dtor, 4)
void __thiscall _Concurrent_vector_base_v4_dtor(
        _Concurrent_vector_base_v4 *this)
{
    TRACE("(%p)\n", this);

    if(this->segment != this->storage)
        free(this->segment);
}

/* ?_Internal_capacity@_Concurrent_vector_base_v4@details@Concurrency@@IBEIXZ */
/* ?_Internal_capacity@_Concurrent_vector_base_v4@details@Concurrency@@IEBA_KXZ */
DEFINE_THISCALL_WRAPPER(_Concurrent_vector_base_v4__Internal_capacity, 4)
size_t __thiscall _Concurrent_vector_base_v4__Internal_capacity(
        const _Concurrent_vector_base_v4 *this)
{
    size_t last_block;
    int i;

    TRACE("(%p)\n", this);

    last_block = (this->segment == this->storage ? STORAGE_SIZE : SEGMENT_SIZE);
    for(i = 0; i < last_block; i++)
    {
        if(!this->segment[i])
            return !i ? 0 : 1 << i;
    }
    return 1 << i;
}

/* ?_Internal_reserve@_Concurrent_vector_base_v4@details@Concurrency@@IAEXIII@Z */
/* ?_Internal_reserve@_Concurrent_vector_base_v4@details@Concurrency@@IEAAX_K00@Z */
DEFINE_THISCALL_WRAPPER(_Concurrent_vector_base_v4__Internal_reserve, 16)
void __thiscall _Concurrent_vector_base_v4__Internal_reserve(
        _Concurrent_vector_base_v4 *this, size_t size,
        size_t element_size, size_t max_size)
{
    size_t block_idx, capacity;
    int i;
    void **new_segment;

    TRACE("(%p %Iu %Iu %Iu)\n", this, size, element_size, max_size);

    if(size > max_size) _vector_base_v4__Internal_throw_exception(this, 0);
    capacity = _Concurrent_vector_base_v4__Internal_capacity(this);
    if(size <= capacity) return;
    block_idx = _vector_base_v4__Segment_index_of(size - 1);
    if(!this->first_block)
        InterlockedCompareExchangeSizeT(&this->first_block, block_idx + 1, 0);
    i = _vector_base_v4__Segment_index_of(capacity);
    if(this->storage == this->segment) {
        for(; i <= block_idx && i < STORAGE_SIZE; i++)
            concurrent_vector_alloc_segment(this, i, element_size);
        if(block_idx >= STORAGE_SIZE) {
            new_segment = malloc(SEGMENT_SIZE * sizeof(void*));
            if(new_segment == NULL) _vector_base_v4__Internal_throw_exception(this, 2);
            memset(new_segment, 0, SEGMENT_SIZE * sizeof(*new_segment));
            memcpy(new_segment, this->storage, STORAGE_SIZE * sizeof(*new_segment));
            if(InterlockedCompareExchangePointer((void*)&this->segment, new_segment,
                        this->storage) != this->storage)
                free(new_segment);
        }
    }
    for(; i <= block_idx; i++)
        concurrent_vector_alloc_segment(this, i, element_size);
}

/* ?_Internal_clear@_Concurrent_vector_base_v4@details@Concurrency@@IAEIP6AXPAXI@Z@Z */
/* ?_Internal_clear@_Concurrent_vector_base_v4@details@Concurrency@@IEAA_KP6AXPEAX_K@Z@Z */
DEFINE_THISCALL_WRAPPER(_Concurrent_vector_base_v4__Internal_clear, 8)
size_t __thiscall _Concurrent_vector_base_v4__Internal_clear(
        _Concurrent_vector_base_v4 *this, void (__cdecl *clear)(void*, size_t))
{
    size_t seg_no, elems;
    int i;

    TRACE("(%p %p)\n", this, clear);

    seg_no = this->early_size  ? _vector_base_v4__Segment_index_of(this->early_size) + 1 : 0;
    for(i = seg_no - 1; i >= 0; i--) {
        elems = this->early_size - (1 << i & ~1);
        clear(this->segment[i], elems);
        this->early_size -= elems;
    }
    while(seg_no < (this->segment == this->storage ? STORAGE_SIZE : SEGMENT_SIZE)) {
        if(!this->segment[seg_no]) break;
        seg_no++;
    }
    return seg_no;
}

/* ?_Internal_compact@_Concurrent_vector_base_v4@details@Concurrency@@IAEPAXIPAXP6AX0I@ZP6AX0PBXI@Z@Z */
/* ?_Internal_compact@_Concurrent_vector_base_v4@details@Concurrency@@IEAAPEAX_KPEAXP6AX10@ZP6AX1PEBX0@Z@Z */
DEFINE_THISCALL_WRAPPER(_Concurrent_vector_base_v4__Internal_compact, 20)
void * __thiscall _Concurrent_vector_base_v4__Internal_compact(
        _Concurrent_vector_base_v4 *this, size_t element_size, void *v,
        void (__cdecl *clear)(void*, size_t),
        void (__cdecl *copy)(void*, const void*, size_t))
{
    compact_block *b;
    size_t size, alloc_size, seg_no, alloc_seg, copy_element, clear_element;
    int i;

    TRACE("(%p %Iu %p %p %p)\n", this, element_size, v, clear, copy);

    size = this->early_size;
    alloc_size = _Concurrent_vector_base_v4__Internal_capacity(this);
    if(alloc_size == 0) return NULL;
    alloc_seg = _vector_base_v4__Segment_index_of(alloc_size - 1);
    if(!size) {
        this->first_block = 0;
        b = v;
        b->first_block = alloc_seg + 1;
        memset(b->blocks, 0, sizeof(b->blocks));
        memcpy(b->blocks, this->segment,
                (alloc_seg + 1) * sizeof(this->segment[0]));
        memset(this->segment, 0, sizeof(this->segment[0]) * (alloc_seg + 1));
        return v;
    }
    seg_no = _vector_base_v4__Segment_index_of(size - 1);
    if(this->first_block == (seg_no + 1) && seg_no == alloc_seg) return NULL;
    b = v;
    b->first_block = this->first_block;
    memset(b->blocks, 0, sizeof(b->blocks));
    memcpy(b->blocks, this->segment,
            (alloc_seg + 1) * sizeof(this->segment[0]));
    if(this->first_block == (seg_no + 1) && seg_no != alloc_seg) {
        memset(b->blocks, 0, sizeof(b->blocks[0]) * (seg_no + 1));
        memset(&this->segment[seg_no + 1], 0, sizeof(this->segment[0]) * (alloc_seg - seg_no));
        return v;
    }
    memset(this->segment, 0,
            (alloc_seg + 1) * sizeof(this->segment[0]));
    this->first_block = 0;
    _Concurrent_vector_base_v4__Internal_reserve(this, size, element_size,
            MSVCP_SIZE_T_MAX / element_size);
    for(i = 0; i < seg_no; i++)
        copy(this->segment[i], b->blocks[i], i ? 1 << i : 2);
    copy_element = size - ((1 << seg_no) & ~1);
    if(copy_element > 0)
        copy(this->segment[seg_no], b->blocks[seg_no], copy_element);
    for(i = 0; i < seg_no; i++)
        clear(b->blocks[i], i ? 1 << i : 2);
    clear_element = size - ((1 << seg_no) & ~1);
    if(clear_element > 0)
        clear(b->blocks[seg_no], clear_element);
    return v;
}

/* ?_Internal_copy@_Concurrent_vector_base_v4@details@Concurrency@@IAEXABV123@IP6AXPAXPBXI@Z@Z */
/* ?_Internal_copy@_Concurrent_vector_base_v4@details@Concurrency@@IEAAXAEBV123@_KP6AXPEAXPEBX1@Z@Z */
DEFINE_THISCALL_WRAPPER(_Concurrent_vector_base_v4__Internal_copy, 16)
void __thiscall _Concurrent_vector_base_v4__Internal_copy(
        _Concurrent_vector_base_v4 *this, const _Concurrent_vector_base_v4 *v,
        size_t element_size, void (__cdecl *copy)(void*, const void*, size_t))
{
    size_t seg_no, v_size;
    int i;

    TRACE("(%p %p %Iu %p)\n", this, v, element_size, copy);

    v_size = v->early_size;
    if(!v_size) {
        this->early_size = 0;
       return;
    }
    _Concurrent_vector_base_v4__Internal_reserve(this, v_size,
            element_size, MSVCP_SIZE_T_MAX / element_size);
    seg_no = _vector_base_v4__Segment_index_of(v_size - 1);
    for(i = 0; i < seg_no; i++)
        copy(this->segment[i], v->segment[i], i ? 1 << i : 2);
    copy(this->segment[i], v->segment[i], v_size - (1 << i & ~1));
    this->early_size = v_size;
}

/* ?_Internal_assign@_Concurrent_vector_base_v4@details@Concurrency@@IAEXABV123@IP6AXPAXI@ZP6AX1PBXI@Z4@Z */
/* ?_Internal_assign@_Concurrent_vector_base_v4@details@Concurrency@@IEAAXAEBV123@_KP6AXPEAX1@ZP6AX2PEBX1@Z5@Z */
DEFINE_THISCALL_WRAPPER(_Concurrent_vector_base_v4__Internal_assign, 24)
void __thiscall _Concurrent_vector_base_v4__Internal_assign(
        _Concurrent_vector_base_v4 *this, const _Concurrent_vector_base_v4 *v,
        size_t element_size, void (__cdecl *clear)(void*, size_t),
        void (__cdecl *assign)(void*, const void*, size_t),
        void (__cdecl *copy)(void*, const void*, size_t))
{
    size_t v_size, seg_no, v_seg_no, remain_element;
    int i;

    TRACE("(%p %p %Iu %p %p %p)\n", this, v, element_size, clear, assign, copy);

    v_size = v->early_size;
    if(!v_size) {
        _Concurrent_vector_base_v4__Internal_clear(this, clear);
        return;
    }
    if(!this->early_size) {
        _Concurrent_vector_base_v4__Internal_copy(this, v, element_size, copy);
        return;
    }
    seg_no = _vector_base_v4__Segment_index_of(this->early_size - 1);
    v_seg_no = _vector_base_v4__Segment_index_of(v_size - 1);

    for(i = 0; i < min(seg_no, v_seg_no); i++)
        assign(this->segment[i], v->segment[i], i ? 1 << i : 2);
    remain_element = min(this->early_size, v_size) - (1 << i & ~1);
    if(remain_element != 0)
        assign(this->segment[i], v->segment[i], remain_element);

    if(this->early_size > v_size)
    {
        if((i ? 1 << i : 2) - remain_element > 0)
            clear((BYTE**)this->segment[i] + element_size * remain_element,
                    (i ? 1 << i : 2) - remain_element);
        if(i < seg_no)
        {
            for(i++; i < seg_no; i++)
                clear(this->segment[i], 1 << i);
            clear(this->segment[i], this->early_size - (1 << i));
        }
    }
    else if(this->early_size < v_size)
    {
        if((i ? 1 << i : 2) - remain_element > 0)
            copy((BYTE**)this->segment[i] + element_size * remain_element,
                    (BYTE**)v->segment[i] + element_size * remain_element,
                    (i ? 1 << i : 2) - remain_element);
        if(i < v_seg_no)
        {
            _Concurrent_vector_base_v4__Internal_reserve(this, v_size,
                    element_size, MSVCP_SIZE_T_MAX / element_size);
            for(i++; i < v_seg_no; i++)
                copy(this->segment[i], v->segment[i], 1 << i);
            copy(this->segment[i], v->segment[i], v->early_size - (1 << i));
        }
   }
    this->early_size = v_size;
}

/* ?_Internal_grow_by@_Concurrent_vector_base_v4@details@Concurrency@@IAEIIIP6AXPAXPBXI@Z1@Z */
/* ?_Internal_grow_by@_Concurrent_vector_base_v4@details@Concurrency@@IEAA_K_K0P6AXPEAXPEBX0@Z2@Z */
DEFINE_THISCALL_WRAPPER(_Concurrent_vector_base_v4__Internal_grow_by, 20)
size_t __thiscall _Concurrent_vector_base_v4__Internal_grow_by(
        _Concurrent_vector_base_v4 *this, size_t count, size_t element_size,
        void (__cdecl *copy)(void*, const void*, size_t), const void *v)
{
    size_t size, seg_no, last_seg_no, remain_size;

    TRACE("(%p %Iu %Iu %p %p)\n", this, count, element_size, copy, v);

    if(count == 0) return this->early_size;
    do {
        size = this->early_size;
        _Concurrent_vector_base_v4__Internal_reserve(this, size + count, element_size,
                MSVCP_SIZE_T_MAX / element_size);
    } while(InterlockedCompareExchangeSizeT(&this->early_size, size + count, size) != size);

    seg_no = size ? _vector_base_v4__Segment_index_of(size - 1) : 0;
    last_seg_no = _vector_base_v4__Segment_index_of(size + count - 1);
    remain_size = min(size + count, 1 << (seg_no + 1)) - size;
    if(remain_size > 0)
        copy(((BYTE**)this->segment[seg_no] + element_size * (size - ((1 << seg_no) & ~1))), v,
            remain_size);
    if(seg_no != last_seg_no)
    {
        for(seg_no++; seg_no < last_seg_no; seg_no++)
            copy(this->segment[seg_no], v, 1 << seg_no);
        copy(this->segment[last_seg_no], v, size + count - (1 << last_seg_no));
    }
    return size;
}

/* ?_Internal_grow_to_at_least_with_result@_Concurrent_vector_base_v4@details@Concurrency@@IAEIIIP6AXPAXPBXI@Z1@Z */
/* ?_Internal_grow_to_at_least_with_result@_Concurrent_vector_base_v4@details@Concurrency@@IEAA_K_K0P6AXPEAXPEBX0@Z2@Z */
DEFINE_THISCALL_WRAPPER(_Concurrent_vector_base_v4__Internal_grow_to_at_least_with_result, 20)
size_t __thiscall _Concurrent_vector_base_v4__Internal_grow_to_at_least_with_result(
        _Concurrent_vector_base_v4 *this, size_t count, size_t element_size,
        void (__cdecl *copy)(void*, const void*, size_t), const void *v)
{
    size_t size, seg_no, last_seg_no, remain_size;

    TRACE("(%p %Iu %Iu %p %p)\n", this, count, element_size, copy, v);

    _Concurrent_vector_base_v4__Internal_reserve(this, count, element_size,
            MSVCP_SIZE_T_MAX / element_size);
    do {
        size = this->early_size;
        if(size >= count) return size;
     } while(InterlockedCompareExchangeSizeT(&this->early_size, count, size) != size);

    seg_no = size ? _vector_base_v4__Segment_index_of(size - 1) : 0;
    last_seg_no = _vector_base_v4__Segment_index_of(count - 1);
    remain_size = min(count, 1 << (seg_no + 1)) - size;
    if(remain_size > 0)
        copy(((BYTE**)this->segment[seg_no] + element_size * (size - ((1 << seg_no) & ~1))), v,
            remain_size);
    if(seg_no != last_seg_no)
    {
        for(seg_no++; seg_no < last_seg_no; seg_no++)
            copy(this->segment[seg_no], v, 1 << seg_no);
        copy(this->segment[last_seg_no], v, count - (1 << last_seg_no));
    }
    return size;
}

/* ?_Internal_push_back@_Concurrent_vector_base_v4@details@Concurrency@@IAEPAXIAAI@Z */
/* ?_Internal_push_back@_Concurrent_vector_base_v4@details@Concurrency@@IEAAPEAX_KAEA_K@Z */
DEFINE_THISCALL_WRAPPER(_Concurrent_vector_base_v4__Internal_push_back, 12)
void * __thiscall _Concurrent_vector_base_v4__Internal_push_back(
       _Concurrent_vector_base_v4 *this, size_t element_size, size_t *idx)
{
    size_t index, seg, segment_base;
    void *data;

    TRACE("(%p %Iu %p)\n", this, element_size, idx);

    do {
        index = this->early_size;
        _Concurrent_vector_base_v4__Internal_reserve(this, index + 1,
                element_size, MSVCP_SIZE_T_MAX / element_size);
    } while(InterlockedCompareExchangeSizeT(&this->early_size, index + 1, index) != index);
    seg = _vector_base_v4__Segment_index_of(index);
    segment_base = (seg == 0) ? 0 : (1 << seg);
    data = (BYTE*)this->segment[seg] + element_size * (index - segment_base);
    *idx = index;

    return data;
}

/* ?_Internal_resize@_Concurrent_vector_base_v4@details@Concurrency@@IAEXIIIP6AXPAXI@ZP6AX0PBXI@Z2@Z */
/* ?_Internal_resize@_Concurrent_vector_base_v4@details@Concurrency@@IEAAX_K00P6AXPEAX0@ZP6AX1PEBX0@Z3@Z */
DEFINE_THISCALL_WRAPPER(_Concurrent_vector_base_v4__Internal_resize, 28)
void __thiscall _Concurrent_vector_base_v4__Internal_resize(
        _Concurrent_vector_base_v4 *this, size_t resize, size_t element_size,
        size_t max_size, void (__cdecl *clear)(void*, size_t),
        void (__cdecl *copy)(void*, const void*, size_t), const void *v)
{
    size_t size, seg_no, end_seg_no, clear_element;

    TRACE("(%p %Iu %Iu %Iu %p %p %p)\n", this, resize, element_size, max_size, clear, copy, v);

    if(resize > max_size) _vector_base_v4__Internal_throw_exception(this, 0);
    size = this->early_size;
    if(resize > size)
        _Concurrent_vector_base_v4__Internal_grow_to_at_least_with_result(this,
                resize, element_size, copy, v);
    else if(resize == 0)
        _Concurrent_vector_base_v4__Internal_clear(this, clear);
    else if(resize < size)
    {
        seg_no = _vector_base_v4__Segment_index_of(size - 1);
        end_seg_no = _vector_base_v4__Segment_index_of(resize - 1);
        clear_element = size - (seg_no ? 1 << seg_no : 2);
        if(clear_element > 0)
            clear(this->segment[seg_no], clear_element);
        if(seg_no) seg_no--;
        for(; seg_no > end_seg_no; seg_no--)
            clear(this->segment[seg_no], 1 << seg_no);
        clear_element = (1 << (end_seg_no + 1)) - resize;
        if(clear_element > 0)
            clear((BYTE**)this->segment[end_seg_no] + element_size * (resize - ((1 << end_seg_no) & ~1)),
                    clear_element);
        this->early_size = resize;
    }
}

/* ?_Internal_swap@_Concurrent_vector_base_v4@details@Concurrency@@IAEXAAV123@@Z */
/* ?_Internal_swap@_Concurrent_vector_base_v4@details@Concurrency@@IEAAXAEAV123@@Z */
DEFINE_THISCALL_WRAPPER(_Concurrent_vector_base_v4__Internal_swap, 8)
void __thiscall _Concurrent_vector_base_v4__Internal_swap(
        _Concurrent_vector_base_v4 *this, _Concurrent_vector_base_v4 *v)
{
    _Concurrent_vector_base_v4 temp;

    TRACE("(%p %p)\n", this, v);

    temp = *this;
    *this = *v;
    *v = temp;
    if(v->segment == this->storage)
        v->segment = v->storage;
    if(this->segment == v->storage)
        this->segment = this->storage;
}
#endif

__ASM_BLOCK_BEGIN(vtables)
#if _MSVCP_VER == 100
    __ASM_VTABLE(iostream_category,
            VTABLE_ADD_FUNC(custom_category_vector_dtor)
            VTABLE_ADD_FUNC(custom_category_name)
            VTABLE_ADD_FUNC(iostream_category_message)
            VTABLE_ADD_FUNC(custom_category_default_error_condition)
            VTABLE_ADD_FUNC(custom_category_equivalent)
            VTABLE_ADD_FUNC(custom_category_equivalent_code));
    __ASM_VTABLE(system_category,
            VTABLE_ADD_FUNC(custom_category_vector_dtor)
            VTABLE_ADD_FUNC(custom_category_name)
            VTABLE_ADD_FUNC(custom_category_message)
            VTABLE_ADD_FUNC(custom_category_default_error_condition)
            VTABLE_ADD_FUNC(custom_category_equivalent)
            VTABLE_ADD_FUNC(custom_category_equivalent_code));
    __ASM_VTABLE(generic_category,
            VTABLE_ADD_FUNC(custom_category_vector_dtor)
            VTABLE_ADD_FUNC(custom_category_name)
            VTABLE_ADD_FUNC(custom_category_message)
            VTABLE_ADD_FUNC(custom_category_default_error_condition)
            VTABLE_ADD_FUNC(custom_category_equivalent)
            VTABLE_ADD_FUNC(custom_category_equivalent_code));
#endif
#if _MSVCP_VER >= 100
    __ASM_VTABLE(_Concurrent_queue_base_v4,
#if _MSVCP_VER >= 110
            VTABLE_ADD_FUNC(_Concurrent_queue_base_v4_dummy)
#endif
            VTABLE_ADD_FUNC(_Concurrent_queue_base_v4_dummy)
            VTABLE_ADD_FUNC(_Concurrent_queue_base_v4_dummy)
            VTABLE_ADD_FUNC(_Concurrent_queue_base_v4_vector_dtor)
            VTABLE_ADD_FUNC(_Concurrent_queue_base_v4_dummy)
            VTABLE_ADD_FUNC(_Concurrent_queue_base_v4_dummy));
    __ASM_VTABLE(_Runtime_object,
            VTABLE_ADD_FUNC(_Runtime_object__GetId));
#endif
#if _MSVCP_VER >= 110
    __ASM_VTABLE(_Pad,
            VTABLE_ADD_FUNC(_Pad__Go));
#endif
__ASM_BLOCK_END

/*********************************************************************
 *  __crtInitializeCriticalSectionEx (MSVCP140.@)
 */
BOOL CDECL MSVCP__crtInitializeCriticalSectionEx(
        CRITICAL_SECTION *cs, DWORD spin_count, DWORD flags)
{
    TRACE("(%p %x %x)\n", cs, spin_count, flags);
    return InitializeCriticalSectionEx(cs, spin_count, flags);
}

/*********************************************************************
 *  __crtCreateEventExW (MSVCP140.@)
 */
HANDLE CDECL MSVCP__crtCreateEventExW(
        SECURITY_ATTRIBUTES *attribs, LPCWSTR name, DWORD flags, DWORD access)
{
    TRACE("(%p %s 0x%08x 0x%08x)\n", attribs, debugstr_w(name), flags, access);
    return CreateEventExW(attribs, name, flags, access);
}

/*********************************************************************
 *  __crtGetTickCount64 (MSVCP140.@)
 */
ULONGLONG CDECL MSVCP__crtGetTickCount64(void)
{
    return GetTickCount64();
}

/*********************************************************************
 *  __crtGetCurrentProcessorNumber (MSVCP140.@)
 */
DWORD CDECL MSVCP__crtGetCurrentProcessorNumber(void)
{
    return GetCurrentProcessorNumber();
}

/*********************************************************************
 *  __crtFlushProcessWriteBuffers (MSVCP140.@)
 */
VOID CDECL MSVCP__crtFlushProcessWriteBuffers(void)
{
    return FlushProcessWriteBuffers();
}

/*********************************************************************
 *  __crtCreateSemaphoreExW (MSVCP140.@)
 */
HANDLE CDECL MSVCP__crtCreateSemaphoreExW(
        SECURITY_ATTRIBUTES *attribs, LONG initial_count, LONG max_count, LPCWSTR name,
        DWORD flags, DWORD access)
{
    TRACE("(%p %d %d %s 0x%08x 0x%08x)\n", attribs, initial_count, max_count, debugstr_w(name),
            flags, access);
    return CreateSemaphoreExW(attribs, initial_count, max_count, name, flags, access);
}

/*********************************************************************
 *  __crtCreateThreadpoolTimer (MSVCP140.@)
 */
PTP_TIMER CDECL MSVCP__crtCreateThreadpoolTimer(PTP_TIMER_CALLBACK callback,
        PVOID userdata, TP_CALLBACK_ENVIRON *environment)
{
    TRACE("(%p %p %p)\n", callback, userdata, environment);
    return CreateThreadpoolTimer(callback, userdata, environment);
}

/*********************************************************************
 *  __crtCloseThreadpoolTimer (MSVCP140.@)
 */
VOID CDECL MSVCP__crtCloseThreadpoolTimer(TP_TIMER *timer)
{
    TRACE("(%p)\n", timer);
    CloseThreadpoolTimer(timer);
}

/*********************************************************************
 *  __crtSetThreadpoolTimer (MSVCP140.@)
 */
VOID CDECL MSVCP__crtSetThreadpoolTimer(TP_TIMER *timer,
        FILETIME *due_time, DWORD period, DWORD window_length)
{
    TRACE("(%p %p 0x%08x 0x%08x)\n", timer, due_time, period, window_length);
    return SetThreadpoolTimer(timer, due_time, period, window_length);
}

/*********************************************************************
 *  __crtWaitForThreadpoolTimerCallbacks (MSVCP140.@)
 */
VOID CDECL MSVCP__crtWaitForThreadpoolTimerCallbacks(TP_TIMER *timer, BOOL cancel)
{
    TRACE("(%p %d)\n", timer, cancel);
    WaitForThreadpoolTimerCallbacks(timer, cancel);
}

/*********************************************************************
 *  __crtCreateThreadpoolWait (MSVCP140.@)
 */
PTP_WAIT CDECL MSVCP__crtCreateThreadpoolWait(PTP_WAIT_CALLBACK callback,
        PVOID userdata, TP_CALLBACK_ENVIRON *environment)
{
    TRACE("(%p %p %p)\n", callback, userdata, environment);
    return CreateThreadpoolWait(callback, userdata, environment);
}

/*********************************************************************
 *  __crtCloseThreadpoolWait (MSVCP140.@)
 */
VOID CDECL MSVCP__crtCloseThreadpoolWait(TP_WAIT *wait)
{
    TRACE("(%p)\n", wait);
    CloseThreadpoolWait(wait);
}

/*********************************************************************
 *  __crtSetThreadpoolWait (MSVCP140.@)
 */
VOID CDECL MSVCP__crtSetThreadpoolWait(TP_WAIT *wait, HANDLE handle, FILETIME *due_time)
{
    TRACE("(%p %p %p)\n", wait, handle, due_time);
    return SetThreadpoolWait(wait, handle, due_time);
}

/*********************************************************************
 *  __crtFreeLibraryWhenCallbackReturns (MSVCP140.@)
 */
VOID CDECL MSVCP__crtFreeLibraryWhenCallbackReturns(PTP_CALLBACK_INSTANCE instance, HMODULE mod)
{
    TRACE("(%p %p)\n", instance, mod);
    FreeLibraryWhenCallbackReturns(instance, mod);
}

/* ?_Execute_once@std@@YAHAAUonce_flag@1@P6GHPAX1PAPAX@Z1@Z */
/* ?_Execute_once@std@@YAHAEAUonce_flag@1@P6AHPEAX1PEAPEAX@Z1@Z */
BOOL __cdecl _Execute_once(INIT_ONCE *flag, PINIT_ONCE_FN func, void *param)
{
    return InitOnceExecuteOnce(flag, func, param, NULL);
}

#if _MSVCP_VER >= 100
void init_misc(void *base)
{
#ifdef __x86_64__
#if _MSVCP_VER == 100
    init_error_category_rtti(base);
    init_iostream_category_rtti(base);
    init_system_category_rtti(base);
    init_generic_category_rtti(base);
#endif
#if _MSVCP_VER >= 100
    init__Concurrent_queue_base_v4_rtti(base);
    init__Runtime_object_rtti(base);
#endif
#if _MSVCP_VER >= 110
    init__Pad_rtti(base);
#endif
#endif

#if _MSVCP_VER == 100
    iostream_category_ctor(&iostream_category);
    system_category_ctor(&system_category);
    generic_category_ctor(&generic_category);
#endif
}

void free_misc(void)
{
#if _MSVCP_VER >= 110
    if(keyed_event)
        NtClose(keyed_event);
    HeapFree(GetProcessHeap(), 0, broadcast_at_thread_exit.to_broadcast);
#endif
}
#endif

#if _MSVCP_VER >= 140
LONGLONG __cdecl _Query_perf_counter(void)
{
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    return li.QuadPart;
}

LONGLONG __cdecl _Query_perf_frequency(void)
{
    LARGE_INTEGER li;
    QueryPerformanceFrequency(&li);
    return li.QuadPart;
}
#endif

void __cdecl threads__Mtx_new(void **mtx)
{
    *mtx = MSVCRT_operator_new(sizeof(CRITICAL_SECTION));
    InitializeCriticalSection(*mtx);
}

void __cdecl threads__Mtx_delete(void *mtx)
{
    DeleteCriticalSection(mtx);
}

void __cdecl threads__Mtx_lock(void *mtx)
{
    EnterCriticalSection(mtx);
}

void __cdecl threads__Mtx_unlock(void *mtx)
{
    LeaveCriticalSection(mtx);
}

#if _MSVCP_VER >= 110
static LONG shared_ptr_lock;

void __cdecl _Lock_shared_ptr_spin_lock(void)
{
    LONG l = 0;

    while(InterlockedCompareExchange(&shared_ptr_lock, 1, 0) != 0) {
        if(l++ == 1000) {
            Sleep(0);
            l = 0;
        }
    }
}

void __cdecl _Unlock_shared_ptr_spin_lock(void)
{
    shared_ptr_lock = 0;
}
#endif

#if _MSVCP_VER >= 100
/* ?is_current_task_group_canceling@Concurrency@@YA_NXZ */
bool __cdecl is_current_task_group_canceling(void)
{
    return Context_IsCurrentTaskCollectionCanceling();
}

/* ?_GetCombinableSize@details@Concurrency@@YAIXZ */
/* ?_GetCombinableSize@details@Concurrency@@YA_KXZ */
size_t __cdecl _GetCombinableSize(void)
{
    FIXME("() stub\n");
    return 11;
}
#endif

#if _MSVCP_VER >= 140
typedef struct {
    void *unk0;
    BYTE unk1;
} task_continuation_context;

/* ??0task_continuation_context@Concurrency@@AAE@XZ */
/* ??0task_continuation_context@Concurrency@@AEAA@XZ */
DEFINE_THISCALL_WRAPPER(task_continuation_context_ctor, 4)
task_continuation_context* __thiscall task_continuation_context_ctor(task_continuation_context *this)
{
    TRACE("(%p)\n", this);
    memset(this, 0, sizeof(*this));
    return this;
}

typedef struct {
    const vtable_ptr *vtable;
    void (__cdecl *func)(void);
    int unk[4];
    void *unk2[3];
    void *this;
} function_void_cdecl_void;

/* ?_Assign@_ContextCallback@details@Concurrency@@AAEXPAX@Z */
/* ?_Assign@_ContextCallback@details@Concurrency@@AEAAXPEAX@Z */
DEFINE_THISCALL_WRAPPER(_ContextCallback__Assign, 8)
void __thiscall _ContextCallback__Assign(void *this, void *v)
{
    TRACE("(%p %p)\n", this, v);
}

#define call_function_do_call(this) CALL_VTBL_FUNC(this, 8, void, (function_void_cdecl_void*), (this))
#define call_function_do_clean(this,b) CALL_VTBL_FUNC(this, 16, void, (function_void_cdecl_void*,bool), (this, b))
/* ?_CallInContext@_ContextCallback@details@Concurrency@@QBEXV?$function@$$A6AXXZ@std@@_N@Z */
/* ?_CallInContext@_ContextCallback@details@Concurrency@@QEBAXV?$function@$$A6AXXZ@std@@_N@Z */
DEFINE_THISCALL_WRAPPER(_ContextCallback__CallInContext, 48)
void __thiscall _ContextCallback__CallInContext(const void *this, function_void_cdecl_void func, bool b)
{
    TRACE("(%p %p %x)\n", this, func.func, b);
    call_function_do_call(func.this);
    call_function_do_clean(func.this, func.this!=&func);
}

/* ?_Capture@_ContextCallback@details@Concurrency@@AAEXXZ */
/* ?_Capture@_ContextCallback@details@Concurrency@@AEAAXXZ */
DEFINE_THISCALL_WRAPPER(_ContextCallback__Capture, 4)
void __thiscall _ContextCallback__Capture(void *this)
{
    TRACE("(%p)\n", this);
}

/* ?_Reset@_ContextCallback@details@Concurrency@@AAEXXZ */
/* ?_Reset@_ContextCallback@details@Concurrency@@AEAAXXZ */
DEFINE_THISCALL_WRAPPER(_ContextCallback__Reset, 4)
void __thiscall _ContextCallback__Reset(void *this)
{
    TRACE("(%p)\n", this);
}

/* ?_IsCurrentOriginSTA@_ContextCallback@details@Concurrency@@CA_NXZ */
bool __cdecl _ContextCallback__IsCurrentOriginSTA(void *this)
{
    TRACE("(%p)\n", this);
    return FALSE;
}

typedef struct {
    /*_Task_impl_base*/void *task;
    bool scheduled;
    bool started;
} _TaskEventLogger;

/* ?_LogCancelTask@_TaskEventLogger@details@Concurrency@@QAEXXZ */
/* ?_LogCancelTask@_TaskEventLogger@details@Concurrency@@QEAAXXZ */
DEFINE_THISCALL_WRAPPER(_TaskEventLogger__LogCancelTask, 4)
void __thiscall _TaskEventLogger__LogCancelTask(_TaskEventLogger *this)
{
    TRACE("(%p)\n", this);
}

/* ?_LogScheduleTask@_TaskEventLogger@details@Concurrency@@QAEX_N@Z */
/* ?_LogScheduleTask@_TaskEventLogger@details@Concurrency@@QEAAX_N@Z */
DEFINE_THISCALL_WRAPPER(_TaskEventLogger__LogScheduleTask, 8)
void __thiscall _TaskEventLogger__LogScheduleTask(_TaskEventLogger *this, bool continuation)
{
    TRACE("(%p %x)\n", this, continuation);
}

/* ?_LogTaskCompleted@_TaskEventLogger@details@Concurrency@@QAEXXZ */
/* ?_LogTaskCompleted@_TaskEventLogger@details@Concurrency@@QEAAXXZ */
DEFINE_THISCALL_WRAPPER(_TaskEventLogger__LogTaskCompleted, 4)
void __thiscall _TaskEventLogger__LogTaskCompleted(_TaskEventLogger *this)
{
    TRACE("(%p)\n", this);
}

/* ?_LogTaskExecutionCompleted@_TaskEventLogger@details@Concurrency@@QAEXXZ */
/* ?_LogTaskExecutionCompleted@_TaskEventLogger@details@Concurrency@@QEAAXXZ */
DEFINE_THISCALL_WRAPPER(_TaskEventLogger__LogTaskExecutionCompleted, 4)
void __thiscall _TaskEventLogger__LogTaskExecutionCompleted(_TaskEventLogger *this)
{
    TRACE("(%p)\n", this);
}

/* ?_LogWorkItemCompleted@_TaskEventLogger@details@Concurrency@@QAEXXZ */
/* ?_LogWorkItemCompleted@_TaskEventLogger@details@Concurrency@@QEAAXXZ */
DEFINE_THISCALL_WRAPPER(_TaskEventLogger__LogWorkItemCompleted, 4)
void __thiscall _TaskEventLogger__LogWorkItemCompleted(_TaskEventLogger *this)
{
    TRACE("(%p)\n", this);
}

/* ?_LogWorkItemStarted@_TaskEventLogger@details@Concurrency@@QAEXXZ */
/* ?_LogWorkItemStarted@_TaskEventLogger@details@Concurrency@@QEAAXXZ */
DEFINE_THISCALL_WRAPPER(_TaskEventLogger__LogWorkItemStarted, 4)
void __thiscall _TaskEventLogger__LogWorkItemStarted(_TaskEventLogger *this)
{
    TRACE("(%p)\n", this);
}

typedef struct {
    PTP_WORK work;
    void (__cdecl *callback)(void*);
    void *arg;
} _Threadpool_chore;

/* ?_Reschedule_chore@details@Concurrency@@YAHPBU_Threadpool_chore@12@@Z */
/* ?_Reschedule_chore@details@Concurrency@@YAHPEBU_Threadpool_chore@12@@Z */
int __cdecl _Reschedule_chore(const _Threadpool_chore *chore)
{
    TRACE("(%p)\n", chore);

    SubmitThreadpoolWork(chore->work);
    return 0;
}

static void WINAPI threadpool_callback(PTP_CALLBACK_INSTANCE instance, void *context, PTP_WORK work)
{
    _Threadpool_chore *chore = context;
    TRACE("calling chore callback: %p\n", chore);
    if (chore->callback)
        chore->callback(chore->arg);
}

/* ?_Schedule_chore@details@Concurrency@@YAHPAU_Threadpool_chore@12@@Z */
/* ?_Schedule_chore@details@Concurrency@@YAHPEAU_Threadpool_chore@12@@Z */
int __cdecl _Schedule_chore(_Threadpool_chore *chore)
{
    TRACE("(%p)\n", chore);

    chore->work = CreateThreadpoolWork(threadpool_callback, chore, NULL);
    /* FIXME: what should be returned in case of error */
    if(!chore->work)
        return -1;

    return _Reschedule_chore(chore);
}

/* ?_Release_chore@details@Concurrency@@YAXPAU_Threadpool_chore@12@@Z */
/* ?_Release_chore@details@Concurrency@@YAXPEAU_Threadpool_chore@12@@Z */
void __cdecl _Release_chore(_Threadpool_chore *chore)
{
    TRACE("(%p)\n", chore);

    if(!chore->work) return;
    CloseThreadpoolWork(chore->work);
    chore->work = NULL;
}
#endif

#if _MSVCP_VER >= 110 && _MSVCP_VER <= 120
typedef struct {
    char dummy;
} _Ph;

/* ?_1@placeholders@std@@3V?$_Ph@$00@2@A */
/* ?_20@placeholders@std@@3V?$_Ph@$0BE@@2@A */
_Ph _Ph_1 = {0}, _Ph_2 = {0}, _Ph_3 = {0}, _Ph_4 = {0}, _Ph_5 = {0};
_Ph _Ph_6 = {0}, _Ph_7 = {0}, _Ph_8 = {0}, _Ph_9 = {0}, _Ph_10 = {0};
_Ph _Ph_11 = {0}, _Ph_12 = {0}, _Ph_13 = {0}, _Ph_14 = {0}, _Ph_15 = {0};
_Ph _Ph_16 = {0}, _Ph_17 = {0}, _Ph_18 = {0}, _Ph_19 = {0}, _Ph_20 = {0};
#endif

#if _MSVCP_VER >= 140
/* ?_IsNonBlockingThread@_Task_impl_base@details@Concurrency@@SA_NXZ */
bool __cdecl _Task_impl_base__IsNonBlockingThread(void)
{
    FIXME("() stub\n");
    return FALSE;
}
#endif

#if _MSVCP_VER >= 110
/* ?_Syserror_map@std@@YAPBDH@Z */
/* ?_Syserror_map@std@@YAPEBDH@Z */
const char* __cdecl _Syserror_map(int err)
{
    int i;

    TRACE("(%d)\n", err);

    for(i = 0; i < ARRAY_SIZE(syserror_map); i++)
    {
        if(syserror_map[i].err == err)
            return syserror_map[i].str;
    }
    return NULL;
}
#endif

#if _MSVCP_VER >= 140
/* ?_Winerror_message@std@@YAKKPADK@Z */
/* ?_Winerror_message@std@@YAKKPEADK@Z */
ULONG __cdecl _Winerror_message(ULONG err, char *buf, ULONG size)
{
    TRACE("(%u %p %u)\n", err, buf, size);

    return FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, err, 0, buf, size, NULL);
}

/* ?_Winerror_map@std@@YAHH@Z */
int __cdecl _Winerror_map(int err)
{
    int low = 0, high = ARRAY_SIZE(winerror_map) - 1, mid;

    while(low <= high)
    {
        mid = (low + high) / 2;

        if(err == winerror_map[mid].winerr)
            return winerror_map[mid].doserr;
        if(err > winerror_map[mid].winerr)
            low = mid + 1;
        else
            high = mid - 1;
    }

    return 0;
}
#endif
