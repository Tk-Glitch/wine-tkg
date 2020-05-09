/*
 *	Process synchronisation
 *
 * Copyright 1996, 1997, 1998 Marcus Meissner
 * Copyright 1997, 1999 Alexandre Julliard
 * Copyright 1999, 2000 Juergen Schmied
 * Copyright 2003 Eric Pouech
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
#include <errno.h>
#include <limits.h>
#include <signal.h>
#ifdef HAVE_SYS_SYSCALL_H
#include <sys/syscall.h>
#endif
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#ifdef HAVE_POLL_H
#include <poll.h>
#endif
#ifdef HAVE_SYS_POLL_H
# include <sys/poll.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_SCHED_H
# include <sched.h>
#endif
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#define NONAMELESSUNION
#include "windef.h"
#include "winternl.h"
#include "wine/server.h"
#include "wine/debug.h"

#include "ntdll_misc.h"
#include "esync.h"
#include "fsync.h"

WINE_DEFAULT_DEBUG_CHANNEL(sync);

HANDLE keyed_event = NULL;

static const LARGE_INTEGER zero_timeout;

#define TICKSPERSEC 10000000

#ifdef __linux__

#define FUTEX_WAIT 0
#define FUTEX_WAKE 1
#define FUTEX_WAIT_BITSET 9
#define FUTEX_WAKE_BITSET 10

static int futex_private = 128;

static inline int futex_wait( const int *addr, int val, struct timespec *timeout )
{
    return syscall( __NR_futex, addr, FUTEX_WAIT | futex_private, val, timeout, 0, 0 );
}

static inline int futex_wake( const int *addr, int val )
{
    return syscall( __NR_futex, addr, FUTEX_WAKE | futex_private, val, NULL, 0, 0 );
}

static inline int futex_wait_bitset( const int *addr, int val, struct timespec *timeout, int mask )
{
    return syscall( __NR_futex, addr, FUTEX_WAIT_BITSET | futex_private, val, timeout, 0, mask );
}

static inline int futex_wake_bitset( const int *addr, int val, int mask )
{
    return syscall( __NR_futex, addr, FUTEX_WAKE_BITSET | futex_private, val, NULL, 0, mask );
}

static inline int use_futexes(void)
{
    static int supported = -1;

    if (supported == -1)
    {
        futex_wait( &supported, 10, NULL );
        if (errno == ENOSYS)
        {
            futex_private = 0;
            futex_wait( &supported, 10, NULL );
        }
        supported = (errno != ENOSYS);
    }
    return supported;
}

static int *get_futex(void **ptr)
{
    if (sizeof(void *) == 8)
        return (int *)((((ULONG_PTR)ptr) + 3) & ~3);
    else if (!(((ULONG_PTR)ptr) & 3))
        return (int *)ptr;
    else
        return NULL;
}

static void timespec_from_timeout( struct timespec *timespec, const LARGE_INTEGER *timeout )
{
    LARGE_INTEGER now;
    timeout_t diff;

    if (timeout->QuadPart > 0)
    {
        NtQuerySystemTime( &now );
        diff = timeout->QuadPart - now.QuadPart;
    }
    else
        diff = -timeout->QuadPart;

    timespec->tv_sec  = diff / TICKSPERSEC;
    timespec->tv_nsec = (diff % TICKSPERSEC) * 100;
}
#endif

/* creates a struct security_descriptor and contained information in one contiguous piece of memory */
NTSTATUS alloc_object_attributes( const OBJECT_ATTRIBUTES *attr, struct object_attributes **ret,
                                  data_size_t *ret_len )
{
    unsigned int len = sizeof(**ret);
    PSID owner = NULL, group = NULL;
    ACL *dacl, *sacl;
    BOOLEAN dacl_present, sacl_present, defaulted;
    PSECURITY_DESCRIPTOR sd;
    NTSTATUS status;

    *ret = NULL;
    *ret_len = 0;

    if (!attr) return STATUS_SUCCESS;

    if (attr->Length != sizeof(*attr)) return STATUS_INVALID_PARAMETER;

    if ((sd = attr->SecurityDescriptor))
    {
        len += sizeof(struct security_descriptor);

        if ((status = RtlGetOwnerSecurityDescriptor( sd, &owner, &defaulted ))) return status;
        if ((status = RtlGetGroupSecurityDescriptor( sd, &group, &defaulted ))) return status;
        if ((status = RtlGetSaclSecurityDescriptor( sd, &sacl_present, &sacl, &defaulted ))) return status;
        if ((status = RtlGetDaclSecurityDescriptor( sd, &dacl_present, &dacl, &defaulted ))) return status;
        if (owner) len += RtlLengthSid( owner );
        if (group) len += RtlLengthSid( group );
        if (sacl_present && sacl) len += sacl->AclSize;
        if (dacl_present && dacl) len += dacl->AclSize;

        /* fix alignment for the Unicode name that follows the structure */
        len = (len + sizeof(WCHAR) - 1) & ~(sizeof(WCHAR) - 1);
    }

    if (attr->ObjectName)
    {
        if (attr->ObjectName->Length & (sizeof(WCHAR) - 1)) return STATUS_OBJECT_NAME_INVALID;
        len += attr->ObjectName->Length;
    }
    else if (attr->RootDirectory) return STATUS_OBJECT_NAME_INVALID;

    len = (len + 3) & ~3;  /* DWORD-align the entire structure */

    *ret = RtlAllocateHeap( GetProcessHeap(), HEAP_ZERO_MEMORY, len );
    if (!*ret) return STATUS_NO_MEMORY;

    (*ret)->rootdir = wine_server_obj_handle( attr->RootDirectory );
    (*ret)->attributes = attr->Attributes;

    if (attr->SecurityDescriptor)
    {
        struct security_descriptor *descr = (struct security_descriptor *)(*ret + 1);
        unsigned char *ptr = (unsigned char *)(descr + 1);

        descr->control = ((SECURITY_DESCRIPTOR *)sd)->Control & ~SE_SELF_RELATIVE;
        if (owner) descr->owner_len = RtlLengthSid( owner );
        if (group) descr->group_len = RtlLengthSid( group );
        if (sacl_present && sacl) descr->sacl_len = sacl->AclSize;
        if (dacl_present && dacl) descr->dacl_len = dacl->AclSize;

        memcpy( ptr, owner, descr->owner_len );
        ptr += descr->owner_len;
        memcpy( ptr, group, descr->group_len );
        ptr += descr->group_len;
        memcpy( ptr, sacl, descr->sacl_len );
        ptr += descr->sacl_len;
        memcpy( ptr, dacl, descr->dacl_len );
        (*ret)->sd_len = (sizeof(*descr) + descr->owner_len + descr->group_len + descr->sacl_len +
                          descr->dacl_len + sizeof(WCHAR) - 1) & ~(sizeof(WCHAR) - 1);
    }

    if (attr->ObjectName)
    {
        unsigned char *ptr = (unsigned char *)(*ret + 1) + (*ret)->sd_len;
        (*ret)->name_len = attr->ObjectName->Length;
        memcpy( ptr, attr->ObjectName->Buffer, (*ret)->name_len );
    }

    *ret_len = len;
    return STATUS_SUCCESS;
}

NTSTATUS validate_open_object_attributes( const OBJECT_ATTRIBUTES *attr )
{
    if (!attr || attr->Length != sizeof(*attr)) return STATUS_INVALID_PARAMETER;

    if (attr->ObjectName)
    {
        if (attr->ObjectName->Length & (sizeof(WCHAR) - 1)) return STATUS_OBJECT_NAME_INVALID;
    }
    else if (attr->RootDirectory) return STATUS_OBJECT_NAME_INVALID;

    return STATUS_SUCCESS;
}

/*
 *	Semaphores
 */

/******************************************************************************
 *  NtCreateSemaphore (NTDLL.@)
 */
NTSTATUS WINAPI NtCreateSemaphore( OUT PHANDLE SemaphoreHandle,
                                   IN ACCESS_MASK access,
                                   IN const OBJECT_ATTRIBUTES *attr OPTIONAL,
                                   IN LONG InitialCount,
                                   IN LONG MaximumCount )
{
    NTSTATUS ret;
    data_size_t len;
    struct object_attributes *objattr;

    if (MaximumCount <= 0 || InitialCount < 0 || InitialCount > MaximumCount)
        return STATUS_INVALID_PARAMETER;

    if (do_fsync())
        return fsync_create_semaphore( SemaphoreHandle, access, attr, InitialCount, MaximumCount );

    if (do_esync())
        return esync_create_semaphore( SemaphoreHandle, access, attr, InitialCount, MaximumCount );

    if ((ret = alloc_object_attributes( attr, &objattr, &len ))) return ret;

    SERVER_START_REQ( create_semaphore )
    {
        req->access  = access;
        req->initial = InitialCount;
        req->max     = MaximumCount;
        wine_server_add_data( req, objattr, len );
        ret = wine_server_call( req );
        *SemaphoreHandle = wine_server_ptr_handle( reply->handle );
    }
    SERVER_END_REQ;

    RtlFreeHeap( GetProcessHeap(), 0, objattr );
    return ret;
}

/******************************************************************************
 *  NtOpenSemaphore (NTDLL.@)
 */
NTSTATUS WINAPI NtOpenSemaphore( HANDLE *handle, ACCESS_MASK access, const OBJECT_ATTRIBUTES *attr )
{
    NTSTATUS ret;

    if ((ret = validate_open_object_attributes( attr ))) return ret;

    if (do_fsync())
        return fsync_open_semaphore( handle, access, attr );

    if (do_esync())
        return esync_open_semaphore( handle, access, attr );

    SERVER_START_REQ( open_semaphore )
    {
        req->access     = access;
        req->attributes = attr->Attributes;
        req->rootdir    = wine_server_obj_handle( attr->RootDirectory );
        if (attr->ObjectName)
            wine_server_add_data( req, attr->ObjectName->Buffer, attr->ObjectName->Length );
        ret = wine_server_call( req );
        *handle = wine_server_ptr_handle( reply->handle );
    }
    SERVER_END_REQ;
    return ret;
}

/******************************************************************************
 *  NtQuerySemaphore (NTDLL.@)
 */
NTSTATUS WINAPI NtQuerySemaphore( HANDLE handle, SEMAPHORE_INFORMATION_CLASS class,
                                  void *info, ULONG len, ULONG *ret_len )
{
    NTSTATUS ret;
    SEMAPHORE_BASIC_INFORMATION *out = info;

    if (do_fsync())
        return fsync_query_semaphore( handle, class, info, len, ret_len );

    if (do_esync())
        return esync_query_semaphore( handle, class, info, len, ret_len );

    TRACE("(%p, %u, %p, %u, %p)\n", handle, class, info, len, ret_len);

    if (class != SemaphoreBasicInformation)
    {
        FIXME("(%p,%d,%u) Unknown class\n", handle, class, len);
        return STATUS_INVALID_INFO_CLASS;
    }

    if (len != sizeof(SEMAPHORE_BASIC_INFORMATION)) return STATUS_INFO_LENGTH_MISMATCH;

    SERVER_START_REQ( query_semaphore )
    {
        req->handle = wine_server_obj_handle( handle );
        if (!(ret = wine_server_call( req )))
        {
            out->CurrentCount = reply->current;
            out->MaximumCount = reply->max;
            if (ret_len) *ret_len = sizeof(SEMAPHORE_BASIC_INFORMATION);
        }
    }
    SERVER_END_REQ;

    return ret;
}

/******************************************************************************
 *  NtReleaseSemaphore (NTDLL.@)
 */
NTSTATUS WINAPI NtReleaseSemaphore( HANDLE handle, ULONG count, PULONG previous )
{
    NTSTATUS ret;

    if (do_fsync())
        return fsync_release_semaphore( handle, count, previous );

    if (do_esync())
        return esync_release_semaphore( handle, count, previous );

    SERVER_START_REQ( release_semaphore )
    {
        req->handle = wine_server_obj_handle( handle );
        req->count  = count;
        if (!(ret = wine_server_call( req )))
        {
            if (previous) *previous = reply->prev_count;
        }
    }
    SERVER_END_REQ;
    return ret;
}

/*
 *	Events
 */

/**************************************************************************
 * NtCreateEvent (NTDLL.@)
 * ZwCreateEvent (NTDLL.@)
 */
NTSTATUS WINAPI NtCreateEvent( PHANDLE EventHandle, ACCESS_MASK DesiredAccess,
                               const OBJECT_ATTRIBUTES *attr, EVENT_TYPE type, BOOLEAN InitialState)
{
    NTSTATUS ret;
    data_size_t len;
    struct object_attributes *objattr;

    if (do_fsync())
        return fsync_create_event( EventHandle, DesiredAccess, attr, type, InitialState );

    if (do_esync())
        return esync_create_event( EventHandle, DesiredAccess, attr, type, InitialState );

    if ((ret = alloc_object_attributes( attr, &objattr, &len ))) return ret;

    SERVER_START_REQ( create_event )
    {
        req->access = DesiredAccess;
        req->manual_reset = (type == NotificationEvent);
        req->initial_state = InitialState;
        wine_server_add_data( req, objattr, len );
        ret = wine_server_call( req );
        *EventHandle = wine_server_ptr_handle( reply->handle );
    }
    SERVER_END_REQ;

    RtlFreeHeap( GetProcessHeap(), 0, objattr );
    return ret;
}

/******************************************************************************
 *  NtOpenEvent (NTDLL.@)
 *  ZwOpenEvent (NTDLL.@)
 */
NTSTATUS WINAPI NtOpenEvent( HANDLE *handle, ACCESS_MASK access, const OBJECT_ATTRIBUTES *attr )
{
    NTSTATUS ret;

    if ((ret = validate_open_object_attributes( attr ))) return ret;

    if (do_fsync())
        return fsync_open_event( handle, access, attr );

    if (do_esync())
        return esync_open_event( handle, access, attr );

    SERVER_START_REQ( open_event )
    {
        req->access     = access;
        req->attributes = attr->Attributes;
        req->rootdir    = wine_server_obj_handle( attr->RootDirectory );
        if (attr->ObjectName)
            wine_server_add_data( req, attr->ObjectName->Buffer, attr->ObjectName->Length );
        ret = wine_server_call( req );
        *handle = wine_server_ptr_handle( reply->handle );
    }
    SERVER_END_REQ;
    return ret;
}


/******************************************************************************
 *  NtSetEvent (NTDLL.@)
 *  ZwSetEvent (NTDLL.@)
 */
NTSTATUS WINAPI NtSetEvent( HANDLE handle, LONG *prev_state )
{
    NTSTATUS ret;

    if (do_fsync())
        return fsync_set_event( handle, prev_state );

    if (do_esync())
        return esync_set_event( handle, prev_state );

    SERVER_START_REQ( event_op )
    {
        req->handle = wine_server_obj_handle( handle );
        req->op     = SET_EVENT;
        ret = wine_server_call( req );
        if (!ret && prev_state) *prev_state = reply->state;
    }
    SERVER_END_REQ;
    return ret;
}

/******************************************************************************
 *  NtResetEvent (NTDLL.@)
 */
NTSTATUS WINAPI NtResetEvent( HANDLE handle, LONG *prev_state )
{
    NTSTATUS ret;

    if (do_fsync())
        return fsync_reset_event( handle, prev_state );

    if (do_esync())
        return esync_reset_event( handle, prev_state );

    SERVER_START_REQ( event_op )
    {
        req->handle = wine_server_obj_handle( handle );
        req->op     = RESET_EVENT;
        ret = wine_server_call( req );
        if (!ret && prev_state) *prev_state = reply->state;
    }
    SERVER_END_REQ;
    return ret;
}

/******************************************************************************
 *  NtClearEvent (NTDLL.@)
 *
 * FIXME
 *   same as NtResetEvent ???
 */
NTSTATUS WINAPI NtClearEvent ( HANDLE handle )
{
    return NtResetEvent( handle, NULL );
}

/******************************************************************************
 *  NtPulseEvent (NTDLL.@)
 *
 * FIXME
 *   PulseCount
 */
NTSTATUS WINAPI NtPulseEvent( HANDLE handle, LONG *prev_state )
{
    NTSTATUS ret;

    if (do_fsync())
        return fsync_pulse_event( handle, prev_state );

    if (do_esync())
        return esync_pulse_event( handle, prev_state );

    SERVER_START_REQ( event_op )
    {
        req->handle = wine_server_obj_handle( handle );
        req->op     = PULSE_EVENT;
        ret = wine_server_call( req );
        if (!ret && prev_state) *prev_state = reply->state;
    }
    SERVER_END_REQ;
    return ret;
}

/******************************************************************************
 *  NtQueryEvent (NTDLL.@)
 */
NTSTATUS WINAPI NtQueryEvent( HANDLE handle, EVENT_INFORMATION_CLASS class,
                              void *info, ULONG len, ULONG *ret_len )
{
    NTSTATUS ret;
    EVENT_BASIC_INFORMATION *out = info;

    if (do_fsync())
        return fsync_query_event( handle, class, info, len, ret_len );

    if (do_esync())
        return esync_query_event( handle, class, info, len, ret_len );

    TRACE("(%p, %u, %p, %u, %p)\n", handle, class, info, len, ret_len);

    if (class != EventBasicInformation)
    {
        FIXME("(%p, %d, %d) Unknown class\n",
              handle, class, len);
        return STATUS_INVALID_INFO_CLASS;
    }

    if (len != sizeof(EVENT_BASIC_INFORMATION)) return STATUS_INFO_LENGTH_MISMATCH;

    SERVER_START_REQ( query_event )
    {
        req->handle = wine_server_obj_handle( handle );
        if (!(ret = wine_server_call( req )))
        {
            out->EventType  = reply->manual_reset ? NotificationEvent : SynchronizationEvent;
            out->EventState = reply->state;
            if (ret_len) *ret_len = sizeof(EVENT_BASIC_INFORMATION);
        }
    }
    SERVER_END_REQ;

    return ret;
}

/*
 *	Mutants (known as Mutexes in Kernel32)
 */

/******************************************************************************
 *              NtCreateMutant                          [NTDLL.@]
 *              ZwCreateMutant                          [NTDLL.@]
 */
NTSTATUS WINAPI NtCreateMutant(OUT HANDLE* MutantHandle,
                               IN ACCESS_MASK access,
                               IN const OBJECT_ATTRIBUTES* attr OPTIONAL,
                               IN BOOLEAN InitialOwner)
{
    NTSTATUS status;
    data_size_t len;
    struct object_attributes *objattr;

    if (do_fsync())
        return fsync_create_mutex( MutantHandle, access, attr, InitialOwner );

    if (do_esync())
        return esync_create_mutex( MutantHandle, access, attr, InitialOwner );

    if ((status = alloc_object_attributes( attr, &objattr, &len ))) return status;

    SERVER_START_REQ( create_mutex )
    {
        req->access  = access;
        req->owned   = InitialOwner;
        wine_server_add_data( req, objattr, len );
        status = wine_server_call( req );
        *MutantHandle = wine_server_ptr_handle( reply->handle );
    }
    SERVER_END_REQ;

    RtlFreeHeap( GetProcessHeap(), 0, objattr );
    return status;
}

/**************************************************************************
 *		NtOpenMutant				[NTDLL.@]
 *		ZwOpenMutant				[NTDLL.@]
 */
NTSTATUS WINAPI NtOpenMutant( HANDLE *handle, ACCESS_MASK access, const OBJECT_ATTRIBUTES *attr )
{
    NTSTATUS    status;

    if ((status = validate_open_object_attributes( attr ))) return status;

    if (do_fsync())
        return fsync_open_mutex( handle, access, attr );

    if (do_esync())
        return esync_open_mutex( handle, access, attr );

    SERVER_START_REQ( open_mutex )
    {
        req->access  = access;
        req->attributes = attr->Attributes;
        req->rootdir = wine_server_obj_handle( attr->RootDirectory );
        if (attr->ObjectName)
            wine_server_add_data( req, attr->ObjectName->Buffer, attr->ObjectName->Length );
        status = wine_server_call( req );
        *handle = wine_server_ptr_handle( reply->handle );
    }
    SERVER_END_REQ;
    return status;
}

/**************************************************************************
 *		NtReleaseMutant				[NTDLL.@]
 *		ZwReleaseMutant				[NTDLL.@]
 */
NTSTATUS WINAPI NtReleaseMutant( IN HANDLE handle, OUT PLONG prev_count OPTIONAL)
{
    NTSTATUS    status;

    if (do_fsync())
        return fsync_release_mutex( handle, prev_count );

    if (do_esync())
        return esync_release_mutex( handle, prev_count );

    SERVER_START_REQ( release_mutex )
    {
        req->handle = wine_server_obj_handle( handle );
        status = wine_server_call( req );
        if (prev_count) *prev_count = 1 - reply->prev_count;
    }
    SERVER_END_REQ;
    return status;
}

/******************************************************************
 *		NtQueryMutant                   [NTDLL.@]
 *		ZwQueryMutant                   [NTDLL.@]
 */
NTSTATUS WINAPI NtQueryMutant( HANDLE handle, MUTANT_INFORMATION_CLASS class,
                               void *info, ULONG len, ULONG *ret_len )
{
    NTSTATUS ret;
    MUTANT_BASIC_INFORMATION *out = info;

    if (do_fsync())
        return fsync_query_mutex( handle, class, info, len, ret_len );

    if (do_esync())
        return esync_query_mutex( handle, class, info, len, ret_len );

    TRACE("(%p, %u, %p, %u, %p)\n", handle, class, info, len, ret_len);

    if (class != MutantBasicInformation)
    {
        FIXME("(%p, %d, %d) Unknown class\n",
              handle, class, len);
        return STATUS_INVALID_INFO_CLASS;
    }

    if (len != sizeof(MUTANT_BASIC_INFORMATION)) return STATUS_INFO_LENGTH_MISMATCH;

    SERVER_START_REQ( query_mutex )
    {
        req->handle = wine_server_obj_handle( handle );
        if (!(ret = wine_server_call( req )))
        {
            out->CurrentCount   = 1 - reply->count;
            out->OwnedByCaller  = reply->owned;
            out->AbandonedState = reply->abandoned;
            if (ret_len) *ret_len = sizeof(MUTANT_BASIC_INFORMATION);
        }
    }
    SERVER_END_REQ;

    return ret;
}

/*
 *	Jobs
 */

/******************************************************************************
 *              NtCreateJobObject   [NTDLL.@]
 *              ZwCreateJobObject   [NTDLL.@]
 */
NTSTATUS WINAPI NtCreateJobObject( PHANDLE handle, ACCESS_MASK access, const OBJECT_ATTRIBUTES *attr )
{
    NTSTATUS ret;
    data_size_t len;
    struct object_attributes *objattr;

    if ((ret = alloc_object_attributes( attr, &objattr, &len ))) return ret;

    SERVER_START_REQ( create_job )
    {
        req->access = access;
        wine_server_add_data( req, objattr, len );
        ret = wine_server_call( req );
        *handle = wine_server_ptr_handle( reply->handle );
    }
    SERVER_END_REQ;

    RtlFreeHeap( GetProcessHeap(), 0, objattr );
    return ret;
}

/******************************************************************************
 *              NtOpenJobObject   [NTDLL.@]
 *              ZwOpenJobObject   [NTDLL.@]
 */
NTSTATUS WINAPI NtOpenJobObject( HANDLE *handle, ACCESS_MASK access, const OBJECT_ATTRIBUTES *attr )
{
    NTSTATUS ret;

    if ((ret = validate_open_object_attributes( attr ))) return ret;

    SERVER_START_REQ( open_job )
    {
        req->access     = access;
        req->attributes = attr->Attributes;
        req->rootdir    = wine_server_obj_handle( attr->RootDirectory );
        if (attr->ObjectName)
            wine_server_add_data( req, attr->ObjectName->Buffer, attr->ObjectName->Length );
        ret = wine_server_call( req );
        *handle = wine_server_ptr_handle( reply->handle );
    }
    SERVER_END_REQ;
    return ret;
}

/******************************************************************************
 *              NtTerminateJobObject   [NTDLL.@]
 *              ZwTerminateJobObject   [NTDLL.@]
 */
NTSTATUS WINAPI NtTerminateJobObject( HANDLE handle, NTSTATUS status )
{
    NTSTATUS ret;

    TRACE( "(%p, %d)\n", handle, status );

    SERVER_START_REQ( terminate_job )
    {
        req->handle = wine_server_obj_handle( handle );
        req->status = status;
        ret = wine_server_call( req );
    }
    SERVER_END_REQ;

    return ret;
}

/******************************************************************************
 *              NtQueryInformationJobObject   [NTDLL.@]
 *              ZwQueryInformationJobObject   [NTDLL.@]
 */
NTSTATUS WINAPI NtQueryInformationJobObject( HANDLE handle, JOBOBJECTINFOCLASS class, PVOID info,
                                             ULONG len, PULONG ret_len )
{
    NTSTATUS ret;

    TRACE( "semi-stub: %p %u %p %u %p\n", handle, class, info, len, ret_len );

    if (class >= MaxJobObjectInfoClass)
        return STATUS_INVALID_PARAMETER;

    switch (class)
    {
    case JobObjectBasicAccountingInformation:
        {
            JOBOBJECT_BASIC_ACCOUNTING_INFORMATION *accounting;
            if (len < sizeof(*accounting))
                return STATUS_INFO_LENGTH_MISMATCH;

            accounting = (JOBOBJECT_BASIC_ACCOUNTING_INFORMATION *)info;

            SERVER_START_REQ(get_job_info)
            {
                req->handle = wine_server_obj_handle( handle );
                if ((ret = wine_server_call( req )) == STATUS_SUCCESS)
                {
                    memset(accounting, 0, sizeof(*accounting));
                    accounting->TotalProcesses = reply->total_processes;
                    accounting->ActiveProcesses = reply->active_processes;
                }
            }
            SERVER_END_REQ;

            if (ret_len) *ret_len = sizeof(*accounting);
            return ret;
        }

    case JobObjectBasicProcessIdList:
        {
            JOBOBJECT_BASIC_PROCESS_ID_LIST *process;
            if (len < sizeof(*process))
                return STATUS_INFO_LENGTH_MISMATCH;

            process = (JOBOBJECT_BASIC_PROCESS_ID_LIST *)info;
            memset(process, 0, sizeof(*process));
            if (ret_len) *ret_len = sizeof(*process);
            return STATUS_SUCCESS;
        }

    case JobObjectExtendedLimitInformation:
        {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION *extended_limit;
            if (len < sizeof(*extended_limit))
                return STATUS_INFO_LENGTH_MISMATCH;

            extended_limit = (JOBOBJECT_EXTENDED_LIMIT_INFORMATION *)info;
            memset(extended_limit, 0, sizeof(*extended_limit));
            if (ret_len) *ret_len = sizeof(*extended_limit);
            return STATUS_SUCCESS;
        }

    case JobObjectBasicLimitInformation:
        {
            JOBOBJECT_BASIC_LIMIT_INFORMATION *basic_limit;
            if (len < sizeof(*basic_limit))
                return STATUS_INFO_LENGTH_MISMATCH;

            basic_limit = (JOBOBJECT_BASIC_LIMIT_INFORMATION *)info;
            memset(basic_limit, 0, sizeof(*basic_limit));
            if (ret_len) *ret_len = sizeof(*basic_limit);
            return STATUS_SUCCESS;
        }

    default:
        return STATUS_NOT_IMPLEMENTED;
    }
}

/******************************************************************************
 *              NtSetInformationJobObject   [NTDLL.@]
 *              ZwSetInformationJobObject   [NTDLL.@]
 */
NTSTATUS WINAPI NtSetInformationJobObject( HANDLE handle, JOBOBJECTINFOCLASS class, PVOID info, ULONG len )
{
    NTSTATUS status = STATUS_NOT_IMPLEMENTED;
    JOBOBJECT_BASIC_LIMIT_INFORMATION *basic_limit;
    ULONG info_size = sizeof(JOBOBJECT_BASIC_LIMIT_INFORMATION);
    DWORD limit_flags = JOB_OBJECT_BASIC_LIMIT_VALID_FLAGS;

    TRACE( "(%p, %u, %p, %u)\n", handle, class, info, len );

    if (class >= MaxJobObjectInfoClass)
        return STATUS_INVALID_PARAMETER;

    switch (class)
    {

    case JobObjectExtendedLimitInformation:
        info_size = sizeof(JOBOBJECT_EXTENDED_LIMIT_INFORMATION);
        limit_flags = JOB_OBJECT_EXTENDED_LIMIT_VALID_FLAGS;
        /* fallthrough */
    case JobObjectBasicLimitInformation:
        if (len != info_size)
            return STATUS_INVALID_PARAMETER;

        basic_limit = info;
        if (basic_limit->LimitFlags & ~limit_flags)
            return STATUS_INVALID_PARAMETER;

        SERVER_START_REQ( set_job_limits )
        {
            req->handle = wine_server_obj_handle( handle );
            req->limit_flags = basic_limit->LimitFlags;
            status = wine_server_call( req );
        }
        SERVER_END_REQ;
        break;

    case JobObjectAssociateCompletionPortInformation:
        if (len != sizeof(JOBOBJECT_ASSOCIATE_COMPLETION_PORT))
            return STATUS_INVALID_PARAMETER;

        SERVER_START_REQ( set_job_completion_port )
        {
            JOBOBJECT_ASSOCIATE_COMPLETION_PORT *port_info = info;
            req->job = wine_server_obj_handle( handle );
            req->port = wine_server_obj_handle( port_info->CompletionPort );
            req->key = wine_server_client_ptr( port_info->CompletionKey );
            status = wine_server_call(req);
        }
        SERVER_END_REQ;
        break;

    case JobObjectBasicUIRestrictions:
        status = STATUS_SUCCESS;
        /* fallthrough */
    default:
        FIXME( "stub: %p %u %p %u\n", handle, class, info, len );
    }

    return status;
}

/******************************************************************************
 *              NtIsProcessInJob   [NTDLL.@]
 *              ZwIsProcessInJob   [NTDLL.@]
 */
NTSTATUS WINAPI NtIsProcessInJob( HANDLE process, HANDLE job )
{
    NTSTATUS status;

    TRACE( "(%p %p)\n", job, process );

    SERVER_START_REQ( process_in_job )
    {
        req->job     = wine_server_obj_handle( job );
        req->process = wine_server_obj_handle( process );
        status = wine_server_call( req );
    }
    SERVER_END_REQ;

    return status;
}

/******************************************************************************
 *              NtAssignProcessToJobObject   [NTDLL.@]
 *              ZwAssignProcessToJobObject   [NTDLL.@]
 */
NTSTATUS WINAPI NtAssignProcessToJobObject( HANDLE job, HANDLE process )
{
    NTSTATUS status;

    TRACE( "(%p %p)\n", job, process );

    SERVER_START_REQ( assign_job )
    {
        req->job     = wine_server_obj_handle( job );
        req->process = wine_server_obj_handle( process );
        status = wine_server_call( req );
    }
    SERVER_END_REQ;

    return status;
}

/*
 *	Timers
 */

/**************************************************************************
 *		NtCreateTimer				[NTDLL.@]
 *		ZwCreateTimer				[NTDLL.@]
 */
NTSTATUS WINAPI NtCreateTimer(OUT HANDLE *handle,
                              IN ACCESS_MASK access,
                              IN const OBJECT_ATTRIBUTES *attr OPTIONAL,
                              IN TIMER_TYPE timer_type)
{
    NTSTATUS status;
    data_size_t len;
    struct object_attributes *objattr;

    if (timer_type != NotificationTimer && timer_type != SynchronizationTimer)
        return STATUS_INVALID_PARAMETER;

    if ((status = alloc_object_attributes( attr, &objattr, &len ))) return status;

    SERVER_START_REQ( create_timer )
    {
        req->access  = access;
        req->manual  = (timer_type == NotificationTimer);
        wine_server_add_data( req, objattr, len );
        status = wine_server_call( req );
        *handle = wine_server_ptr_handle( reply->handle );
    }
    SERVER_END_REQ;

    RtlFreeHeap( GetProcessHeap(), 0, objattr );
    return status;

}

/**************************************************************************
 *		NtOpenTimer				[NTDLL.@]
 *		ZwOpenTimer				[NTDLL.@]
 */
NTSTATUS WINAPI NtOpenTimer( HANDLE *handle, ACCESS_MASK access, const OBJECT_ATTRIBUTES *attr )
{
    NTSTATUS status;

    if ((status = validate_open_object_attributes( attr ))) return status;

    SERVER_START_REQ( open_timer )
    {
        req->access     = access;
        req->attributes = attr->Attributes;
        req->rootdir    = wine_server_obj_handle( attr->RootDirectory );
        if (attr->ObjectName)
            wine_server_add_data( req, attr->ObjectName->Buffer, attr->ObjectName->Length );
        status = wine_server_call( req );
        *handle = wine_server_ptr_handle( reply->handle );
    }
    SERVER_END_REQ;
    return status;
}

/**************************************************************************
 *		NtSetTimer				[NTDLL.@]
 *		ZwSetTimer				[NTDLL.@]
 */
NTSTATUS WINAPI NtSetTimer(IN HANDLE handle,
                           IN const LARGE_INTEGER* when,
                           IN PTIMER_APC_ROUTINE callback,
                           IN PVOID callback_arg,
                           IN BOOLEAN resume,
                           IN ULONG period OPTIONAL,
                           OUT PBOOLEAN state OPTIONAL)
{
    NTSTATUS    status = STATUS_SUCCESS;

    TRACE("(%p,%p,%p,%p,%08x,0x%08x,%p)\n",
          handle, when, callback, callback_arg, resume, period, state);

    SERVER_START_REQ( set_timer )
    {
        req->handle   = wine_server_obj_handle( handle );
        req->period   = period;
        req->expire   = when->QuadPart;
        req->callback = wine_server_client_ptr( callback );
        req->arg      = wine_server_client_ptr( callback_arg );
        status = wine_server_call( req );
        if (state) *state = reply->signaled;
    }
    SERVER_END_REQ;

    /* set error but can still succeed */
    if (resume && status == STATUS_SUCCESS) return STATUS_TIMER_RESUME_IGNORED;
    return status;
}

/**************************************************************************
 *		NtCancelTimer				[NTDLL.@]
 *		ZwCancelTimer				[NTDLL.@]
 */
NTSTATUS WINAPI NtCancelTimer(IN HANDLE handle, OUT BOOLEAN* state)
{
    NTSTATUS    status;

    SERVER_START_REQ( cancel_timer )
    {
        req->handle = wine_server_obj_handle( handle );
        status = wine_server_call( req );
        if (state) *state = reply->signaled;
    }
    SERVER_END_REQ;
    return status;
}

/******************************************************************************
 *  NtQueryTimer (NTDLL.@)
 *
 * Retrieves information about a timer.
 *
 * PARAMS
 *  TimerHandle           [I] The timer to retrieve information about.
 *  TimerInformationClass [I] The type of information to retrieve.
 *  TimerInformation      [O] Pointer to buffer to store information in.
 *  Length                [I] The length of the buffer pointed to by TimerInformation.
 *  ReturnLength          [O] Optional. The size of buffer actually used.
 *
 * RETURNS
 *  Success: STATUS_SUCCESS
 *  Failure: STATUS_INFO_LENGTH_MISMATCH, if Length doesn't match the required data
 *           size for the class specified.
 *           STATUS_INVALID_INFO_CLASS, if an invalid TimerInformationClass was specified.
 *           STATUS_ACCESS_DENIED, if TimerHandle does not have TIMER_QUERY_STATE access
 *           to the timer.
 */
NTSTATUS WINAPI NtQueryTimer(
    HANDLE TimerHandle,
    TIMER_INFORMATION_CLASS TimerInformationClass,
    PVOID TimerInformation,
    ULONG Length,
    PULONG ReturnLength)
{
    TIMER_BASIC_INFORMATION * basic_info = TimerInformation;
    NTSTATUS status;
    LARGE_INTEGER now;

    TRACE("(%p,%d,%p,0x%08x,%p)\n", TimerHandle, TimerInformationClass,
       TimerInformation, Length, ReturnLength);

    switch (TimerInformationClass)
    {
    case TimerBasicInformation:
        if (Length < sizeof(TIMER_BASIC_INFORMATION))
            return STATUS_INFO_LENGTH_MISMATCH;

        SERVER_START_REQ(get_timer_info)
        {
            req->handle = wine_server_obj_handle( TimerHandle );
            status = wine_server_call(req);

            /* convert server time to absolute NTDLL time */
            basic_info->RemainingTime.QuadPart = reply->when;
            basic_info->TimerState = reply->signaled;
        }
        SERVER_END_REQ;

        /* convert into relative time */
        if (basic_info->RemainingTime.QuadPart > 0)
            NtQuerySystemTime(&now);
        else
        {
            RtlQueryPerformanceCounter(&now);
            basic_info->RemainingTime.QuadPart = -basic_info->RemainingTime.QuadPart;
        }

        if (now.QuadPart > basic_info->RemainingTime.QuadPart)
            basic_info->RemainingTime.QuadPart = 0;
        else
            basic_info->RemainingTime.QuadPart -= now.QuadPart;

        if (ReturnLength) *ReturnLength = sizeof(TIMER_BASIC_INFORMATION);

        return status;
    }

    FIXME("Unhandled class %d\n", TimerInformationClass);
    return STATUS_INVALID_INFO_CLASS;
}


/******************************************************************************
 * NtQueryTimerResolution [NTDLL.@]
 */
NTSTATUS WINAPI NtQueryTimerResolution(OUT ULONG* min_resolution,
                                       OUT ULONG* max_resolution,
                                       OUT ULONG* current_resolution)
{
    FIXME("(%p,%p,%p), stub!\n",
          min_resolution, max_resolution, current_resolution);

    return STATUS_NOT_IMPLEMENTED;
}

/******************************************************************************
 * NtSetTimerResolution [NTDLL.@]
 */
NTSTATUS WINAPI NtSetTimerResolution(IN ULONG resolution,
                                     IN BOOLEAN set_resolution,
                                     OUT ULONG* current_resolution )
{
    FIXME("(%u,%u,%p), stub!\n",
          resolution, set_resolution, current_resolution);

    return STATUS_NOT_IMPLEMENTED;
}



/* wait operations */

static NTSTATUS wait_objects( DWORD count, const HANDLE *handles,
                              BOOLEAN wait_any, BOOLEAN alertable,
                              const LARGE_INTEGER *timeout )
{
    select_op_t select_op;
    UINT i, flags = SELECT_INTERRUPTIBLE;

    if (!count || count > MAXIMUM_WAIT_OBJECTS) return STATUS_INVALID_PARAMETER_1;

    if (do_fsync())
    {
        NTSTATUS ret = fsync_wait_objects( count, handles, wait_any, alertable, timeout );
        if (ret != STATUS_NOT_IMPLEMENTED)
            return ret;
    }

    if (do_esync())
    {
        NTSTATUS ret = esync_wait_objects( count, handles, wait_any, alertable, timeout );
        if (ret != STATUS_NOT_IMPLEMENTED)
            return ret;
    }

    if (alertable) flags |= SELECT_ALERTABLE;
    select_op.wait.op = wait_any ? SELECT_WAIT : SELECT_WAIT_ALL;
    for (i = 0; i < count; i++) select_op.wait.handles[i] = wine_server_obj_handle( handles[i] );
    return server_wait( &select_op, offsetof( select_op_t, wait.handles[count] ), flags, timeout );
}


/******************************************************************
 *		NtWaitForMultipleObjects (NTDLL.@)
 */
NTSTATUS WINAPI NtWaitForMultipleObjects( DWORD count, const HANDLE *handles,
                                          BOOLEAN wait_any, BOOLEAN alertable,
                                          const LARGE_INTEGER *timeout )
{
    return wait_objects( count, handles, wait_any, alertable, timeout );
}


/******************************************************************
 *		NtWaitForSingleObject (NTDLL.@)
 */
NTSTATUS WINAPI NtWaitForSingleObject(HANDLE handle, BOOLEAN alertable, const LARGE_INTEGER *timeout )
{
    return wait_objects( 1, &handle, FALSE, alertable, timeout );
}


/******************************************************************
 *		NtSignalAndWaitForSingleObject (NTDLL.@)
 */
NTSTATUS WINAPI NtSignalAndWaitForSingleObject( HANDLE hSignalObject, HANDLE hWaitObject,
                                                BOOLEAN alertable, const LARGE_INTEGER *timeout )
{
    select_op_t select_op;
    UINT flags = SELECT_INTERRUPTIBLE;

    if (do_fsync())
        return fsync_signal_and_wait( hSignalObject, hWaitObject, alertable, timeout );

    if (do_esync())
        return esync_signal_and_wait( hSignalObject, hWaitObject, alertable, timeout );

    if (!hSignalObject) return STATUS_INVALID_HANDLE;

    if (alertable) flags |= SELECT_ALERTABLE;
    select_op.signal_and_wait.op = SELECT_SIGNAL_AND_WAIT;
    select_op.signal_and_wait.wait = wine_server_obj_handle( hWaitObject );
    select_op.signal_and_wait.signal = wine_server_obj_handle( hSignalObject );
    return server_wait( &select_op, sizeof(select_op.signal_and_wait), flags, timeout );
}


/******************************************************************
 *		NtYieldExecution (NTDLL.@)
 */
NTSTATUS WINAPI NtYieldExecution(void)
{
#ifdef HAVE_SCHED_YIELD
    sched_yield();
    return STATUS_SUCCESS;
#else
    return STATUS_NO_YIELD_PERFORMED;
#endif
}


/******************************************************************
 *		NtDelayExecution (NTDLL.@)
 */
NTSTATUS WINAPI NtDelayExecution( BOOLEAN alertable, const LARGE_INTEGER *timeout )
{
    /* if alertable, we need to query the server */
    if (alertable)
        return server_wait( NULL, 0, SELECT_INTERRUPTIBLE | SELECT_ALERTABLE, timeout );

    if (!timeout || timeout->QuadPart == TIMEOUT_INFINITE)  /* sleep forever */
    {
        for (;;) select( 0, NULL, NULL, NULL, NULL );
    }
    else
    {
        LARGE_INTEGER now;
        timeout_t when, diff;

        if ((when = timeout->QuadPart) < 0)
        {
            NtQuerySystemTime( &now );
            when = now.QuadPart - when;
        }

        /* Note that we yield after establishing the desired timeout */
        NtYieldExecution();
        if (!when) return STATUS_SUCCESS;

        for (;;)
        {
            struct timeval tv;
            NtQuerySystemTime( &now );
            diff = (when - now.QuadPart + 9) / 10;
            if (diff <= 0) break;
            tv.tv_sec  = diff / 1000000;
            tv.tv_usec = diff % 1000000;
            if (select( 0, NULL, NULL, NULL, &tv ) != -1) break;
        }
    }
    return STATUS_SUCCESS;
}


/******************************************************************************
 *              NtCreateKeyedEvent (NTDLL.@)
 */
NTSTATUS WINAPI NtCreateKeyedEvent( HANDLE *handle, ACCESS_MASK access,
                                    const OBJECT_ATTRIBUTES *attr, ULONG flags )
{
    NTSTATUS ret;
    data_size_t len;
    struct object_attributes *objattr;

    if ((ret = alloc_object_attributes( attr, &objattr, &len ))) return ret;

    SERVER_START_REQ( create_keyed_event )
    {
        req->access = access;
        wine_server_add_data( req, objattr, len );
        ret = wine_server_call( req );
        *handle = wine_server_ptr_handle( reply->handle );
    }
    SERVER_END_REQ;

    RtlFreeHeap( GetProcessHeap(), 0, objattr );
    return ret;
}

/******************************************************************************
 *              NtOpenKeyedEvent (NTDLL.@)
 */
NTSTATUS WINAPI NtOpenKeyedEvent( HANDLE *handle, ACCESS_MASK access, const OBJECT_ATTRIBUTES *attr )
{
    NTSTATUS ret;

    if ((ret = validate_open_object_attributes( attr ))) return ret;

    SERVER_START_REQ( open_keyed_event )
    {
        req->access     = access;
        req->attributes = attr->Attributes;
        req->rootdir    = wine_server_obj_handle( attr->RootDirectory );
        if (attr->ObjectName)
            wine_server_add_data( req, attr->ObjectName->Buffer, attr->ObjectName->Length );
        ret = wine_server_call( req );
        *handle = wine_server_ptr_handle( reply->handle );
    }
    SERVER_END_REQ;
    return ret;
}

/******************************************************************************
 *              NtWaitForKeyedEvent (NTDLL.@)
 */
NTSTATUS WINAPI NtWaitForKeyedEvent( HANDLE handle, const void *key,
                                     BOOLEAN alertable, const LARGE_INTEGER *timeout )
{
    select_op_t select_op;
    UINT flags = SELECT_INTERRUPTIBLE;

    if (!handle) handle = keyed_event;
    if ((ULONG_PTR)key & 1) return STATUS_INVALID_PARAMETER_1;
    if (alertable) flags |= SELECT_ALERTABLE;
    select_op.keyed_event.op     = SELECT_KEYED_EVENT_WAIT;
    select_op.keyed_event.handle = wine_server_obj_handle( handle );
    select_op.keyed_event.key    = wine_server_client_ptr( key );
    return server_wait( &select_op, sizeof(select_op.keyed_event), flags, timeout );
}

/******************************************************************************
 *              NtReleaseKeyedEvent (NTDLL.@)
 */
NTSTATUS WINAPI NtReleaseKeyedEvent( HANDLE handle, const void *key,
                                     BOOLEAN alertable, const LARGE_INTEGER *timeout )
{
    select_op_t select_op;
    UINT flags = SELECT_INTERRUPTIBLE;

    if (!handle) handle = keyed_event;
    if ((ULONG_PTR)key & 1) return STATUS_INVALID_PARAMETER_1;
    if (alertable) flags |= SELECT_ALERTABLE;
    select_op.keyed_event.op     = SELECT_KEYED_EVENT_RELEASE;
    select_op.keyed_event.handle = wine_server_obj_handle( handle );
    select_op.keyed_event.key    = wine_server_client_ptr( key );
    return server_wait( &select_op, sizeof(select_op.keyed_event), flags, timeout );
}

/******************************************************************
 *              NtCreateIoCompletion (NTDLL.@)
 *              ZwCreateIoCompletion (NTDLL.@)
 *
 * Creates I/O completion object.
 *
 * PARAMS
 *      CompletionPort            [O] created completion object handle will be placed there
 *      DesiredAccess             [I] desired access to a handle (combination of IO_COMPLETION_*)
 *      ObjectAttributes          [I] completion object attributes
 *      NumberOfConcurrentThreads [I] desired number of concurrent active worker threads
 *
 */
NTSTATUS WINAPI NtCreateIoCompletion( PHANDLE CompletionPort, ACCESS_MASK DesiredAccess,
                                      POBJECT_ATTRIBUTES attr, ULONG NumberOfConcurrentThreads )
{
    NTSTATUS status;
    data_size_t len;
    struct object_attributes *objattr;

    TRACE("(%p, %x, %p, %d)\n", CompletionPort, DesiredAccess, attr, NumberOfConcurrentThreads);

    if (!CompletionPort)
        return STATUS_INVALID_PARAMETER;

    if ((status = alloc_object_attributes( attr, &objattr, &len ))) return status;

    SERVER_START_REQ( create_completion )
    {
        req->access     = DesiredAccess;
        req->concurrent = NumberOfConcurrentThreads;
        wine_server_add_data( req, objattr, len );
        if (!(status = wine_server_call( req )))
            *CompletionPort = wine_server_ptr_handle( reply->handle );
    }
    SERVER_END_REQ;

    RtlFreeHeap( GetProcessHeap(), 0, objattr );
    return status;
}

/******************************************************************
 *              NtSetIoCompletion (NTDLL.@)
 *              ZwSetIoCompletion (NTDLL.@)
 *
 * Inserts completion message into queue
 *
 * PARAMS
 *      CompletionPort           [I] HANDLE to completion object
 *      CompletionKey            [I] completion key
 *      CompletionValue          [I] completion value (usually pointer to OVERLAPPED)
 *      Status                   [I] operation status
 *      NumberOfBytesTransferred [I] number of bytes transferred
 */
NTSTATUS WINAPI NtSetIoCompletion( HANDLE CompletionPort, ULONG_PTR CompletionKey,
                                   ULONG_PTR CompletionValue, NTSTATUS Status,
                                   SIZE_T NumberOfBytesTransferred )
{
    NTSTATUS status;

    TRACE("(%p, %lx, %lx, %x, %lx)\n", CompletionPort, CompletionKey,
          CompletionValue, Status, NumberOfBytesTransferred);

    SERVER_START_REQ( add_completion )
    {
        req->handle      = wine_server_obj_handle( CompletionPort );
        req->ckey        = CompletionKey;
        req->cvalue      = CompletionValue;
        req->status      = Status;
        req->information = NumberOfBytesTransferred;
        status = wine_server_call( req );
    }
    SERVER_END_REQ;
    return status;
}

/******************************************************************
 *              NtRemoveIoCompletion (NTDLL.@)
 *              ZwRemoveIoCompletion (NTDLL.@)
 *
 * (Wait for and) retrieve first completion message from completion object's queue
 *
 * PARAMS
 *      CompletionPort  [I] HANDLE to I/O completion object
 *      CompletionKey   [O] completion key
 *      CompletionValue [O] Completion value given in NtSetIoCompletion or in async operation
 *      iosb            [O] IO_STATUS_BLOCK of completed asynchronous operation
 *      WaitTime        [I] optional wait time in NTDLL format
 *
 */
NTSTATUS WINAPI NtRemoveIoCompletion( HANDLE CompletionPort, PULONG_PTR CompletionKey,
                                      PULONG_PTR CompletionValue, PIO_STATUS_BLOCK iosb,
                                      PLARGE_INTEGER WaitTime )
{
    NTSTATUS status;

    TRACE("(%p, %p, %p, %p, %p)\n", CompletionPort, CompletionKey,
          CompletionValue, iosb, WaitTime);

    for(;;)
    {
        SERVER_START_REQ( remove_completion )
        {
            req->handle = wine_server_obj_handle( CompletionPort );
            if (!(status = wine_server_call( req )))
            {
                *CompletionKey    = reply->ckey;
                *CompletionValue  = reply->cvalue;
                iosb->Information = reply->information;
                iosb->u.Status    = reply->status;
            }
        }
        SERVER_END_REQ;
        if (status != STATUS_PENDING) break;

        status = NtWaitForSingleObject( CompletionPort, FALSE, WaitTime );
        if (status != WAIT_OBJECT_0) break;
    }
    return status;
}

/******************************************************************
 *              NtRemoveIoCompletionEx (NTDLL.@)
 *              ZwRemoveIoCompletionEx (NTDLL.@)
 */
NTSTATUS WINAPI NtRemoveIoCompletionEx( HANDLE port, FILE_IO_COMPLETION_INFORMATION *info, ULONG count,
                                        ULONG *written, LARGE_INTEGER *timeout, BOOLEAN alertable )
{
    NTSTATUS ret;
    ULONG i = 0;

    TRACE("%p %p %u %p %p %u\n", port, info, count, written, timeout, alertable);

    for (;;)
    {
        while (i < count)
        {
            SERVER_START_REQ( remove_completion )
            {
                req->handle = wine_server_obj_handle( port );
                if (!(ret = wine_server_call( req )))
                {
                    info[i].CompletionKey             = reply->ckey;
                    info[i].CompletionValue           = reply->cvalue;
                    info[i].IoStatusBlock.Information = reply->information;
                    info[i].IoStatusBlock.u.Status    = reply->status;
                }
            }
            SERVER_END_REQ;

            if (ret != STATUS_SUCCESS) break;

            ++i;
        }

        if (i || ret != STATUS_PENDING)
        {
            if (ret == STATUS_PENDING)
                ret = STATUS_SUCCESS;
            break;
        }

        ret = NtWaitForSingleObject( port, alertable, timeout );
        if (ret != WAIT_OBJECT_0) break;
    }

    *written = i ? i : 1;
    return ret;
}

/******************************************************************
 *              NtOpenIoCompletion (NTDLL.@)
 *              ZwOpenIoCompletion (NTDLL.@)
 *
 * Opens I/O completion object
 *
 * PARAMS
 *      CompletionPort     [O] completion object handle will be placed there
 *      DesiredAccess      [I] desired access to a handle (combination of IO_COMPLETION_*)
 *      ObjectAttributes   [I] completion object name
 *
 */
NTSTATUS WINAPI NtOpenIoCompletion( HANDLE *handle, ACCESS_MASK access, const OBJECT_ATTRIBUTES *attr )
{
    NTSTATUS status;

    if (!handle) return STATUS_INVALID_PARAMETER;
    if ((status = validate_open_object_attributes( attr ))) return status;

    SERVER_START_REQ( open_completion )
    {
        req->access     = access;
        req->attributes = attr->Attributes;
        req->rootdir    = wine_server_obj_handle( attr->RootDirectory );
        if (attr->ObjectName)
            wine_server_add_data( req, attr->ObjectName->Buffer, attr->ObjectName->Length );
        status = wine_server_call( req );
        *handle = wine_server_ptr_handle( reply->handle );
    }
    SERVER_END_REQ;
    return status;
}

/******************************************************************
 *              NtQueryIoCompletion (NTDLL.@)
 *              ZwQueryIoCompletion (NTDLL.@)
 *
 * Requests information about given I/O completion object
 *
 * PARAMS
 *      CompletionPort        [I] HANDLE to completion port to request
 *      InformationClass      [I] information class
 *      CompletionInformation [O] user-provided buffer for data
 *      BufferLength          [I] buffer length
 *      RequiredLength        [O] required buffer length
 *
 */
NTSTATUS WINAPI NtQueryIoCompletion( HANDLE CompletionPort, IO_COMPLETION_INFORMATION_CLASS InformationClass,
                                     PVOID CompletionInformation, ULONG BufferLength, PULONG RequiredLength )
{
    NTSTATUS status;

    TRACE("(%p, %d, %p, 0x%x, %p)\n", CompletionPort, InformationClass, CompletionInformation,
          BufferLength, RequiredLength);

    if (!CompletionInformation) return STATUS_INVALID_PARAMETER;
    switch( InformationClass )
    {
        case IoCompletionBasicInformation:
            {
                ULONG *info = CompletionInformation;

                if (RequiredLength) *RequiredLength = sizeof(*info);
                if (BufferLength != sizeof(*info))
                    status = STATUS_INFO_LENGTH_MISMATCH;
                else
                {
                    SERVER_START_REQ( query_completion )
                    {
                        req->handle = wine_server_obj_handle( CompletionPort );
                        if (!(status = wine_server_call( req )))
                            *info = reply->depth;
                    }
                    SERVER_END_REQ;
                }
            }
            break;
        default:
            status = STATUS_INVALID_PARAMETER;
            break;
    }
    return status;
}

NTSTATUS NTDLL_AddCompletion( HANDLE hFile, ULONG_PTR CompletionValue,
                              NTSTATUS CompletionStatus, ULONG Information, BOOL async )
{
    NTSTATUS status;

    SERVER_START_REQ( add_fd_completion )
    {
        req->handle      = wine_server_obj_handle( hFile );
        req->cvalue      = CompletionValue;
        req->status      = CompletionStatus;
        req->information = Information;
        req->async       = async;
        status = wine_server_call( req );
    }
    SERVER_END_REQ;
    return status;
}

/******************************************************************
 *              RtlRunOnceInitialize (NTDLL.@)
 */
void WINAPI RtlRunOnceInitialize( RTL_RUN_ONCE *once )
{
    once->Ptr = NULL;
}

/******************************************************************
 *              RtlRunOnceBeginInitialize (NTDLL.@)
 */
DWORD WINAPI RtlRunOnceBeginInitialize( RTL_RUN_ONCE *once, ULONG flags, void **context )
{
    if (flags & RTL_RUN_ONCE_CHECK_ONLY)
    {
        ULONG_PTR val = (ULONG_PTR)once->Ptr;

        if (flags & RTL_RUN_ONCE_ASYNC) return STATUS_INVALID_PARAMETER;
        if ((val & 3) != 2) return STATUS_UNSUCCESSFUL;
        if (context) *context = (void *)(val & ~3);
        return STATUS_SUCCESS;
    }

    for (;;)
    {
        ULONG_PTR next, val = (ULONG_PTR)once->Ptr;

        switch (val & 3)
        {
        case 0:  /* first time */
            if (!InterlockedCompareExchangePointer( &once->Ptr,
                                                    (flags & RTL_RUN_ONCE_ASYNC) ? (void *)3 : (void *)1, 0 ))
                return STATUS_PENDING;
            break;

        case 1:  /* in progress, wait */
            if (flags & RTL_RUN_ONCE_ASYNC) return STATUS_INVALID_PARAMETER;
            next = val & ~3;
            if (InterlockedCompareExchangePointer( &once->Ptr, (void *)((ULONG_PTR)&next | 1),
                                                   (void *)val ) == (void *)val)
                NtWaitForKeyedEvent( 0, &next, FALSE, NULL );
            break;

        case 2:  /* done */
            if (context) *context = (void *)(val & ~3);
            return STATUS_SUCCESS;

        case 3:  /* in progress, async */
            if (!(flags & RTL_RUN_ONCE_ASYNC)) return STATUS_INVALID_PARAMETER;
            return STATUS_PENDING;
        }
    }
}

/******************************************************************
 *              RtlRunOnceComplete (NTDLL.@)
 */
DWORD WINAPI RtlRunOnceComplete( RTL_RUN_ONCE *once, ULONG flags, void *context )
{
    if ((ULONG_PTR)context & 3) return STATUS_INVALID_PARAMETER;

    if (flags & RTL_RUN_ONCE_INIT_FAILED)
    {
        if (context) return STATUS_INVALID_PARAMETER;
        if (flags & RTL_RUN_ONCE_ASYNC) return STATUS_INVALID_PARAMETER;
    }
    else context = (void *)((ULONG_PTR)context | 2);

    for (;;)
    {
        ULONG_PTR val = (ULONG_PTR)once->Ptr;

        switch (val & 3)
        {
        case 1:  /* in progress */
            if (InterlockedCompareExchangePointer( &once->Ptr, context, (void *)val ) != (void *)val) break;
            val &= ~3;
            while (val)
            {
                ULONG_PTR next = *(ULONG_PTR *)val;
                NtReleaseKeyedEvent( 0, (void *)val, FALSE, NULL );
                val = next;
            }
            return STATUS_SUCCESS;

        case 3:  /* in progress, async */
            if (!(flags & RTL_RUN_ONCE_ASYNC)) return STATUS_INVALID_PARAMETER;
            if (InterlockedCompareExchangePointer( &once->Ptr, context, (void *)val ) != (void *)val) break;
            return STATUS_SUCCESS;

        default:
            return STATUS_UNSUCCESSFUL;
        }
    }
}

/******************************************************************
 *              RtlRunOnceExecuteOnce (NTDLL.@)
 */
DWORD WINAPI RtlRunOnceExecuteOnce( RTL_RUN_ONCE *once, PRTL_RUN_ONCE_INIT_FN func,
                                    void *param, void **context )
{
    DWORD ret = RtlRunOnceBeginInitialize( once, 0, context );

    if (ret != STATUS_PENDING) return ret;

    if (!func( once, param, context ))
    {
        RtlRunOnceComplete( once, RTL_RUN_ONCE_INIT_FAILED, NULL );
        return STATUS_UNSUCCESSFUL;
    }

    return RtlRunOnceComplete( once, 0, context ? *context : NULL );
}

#ifdef __linux__

/* Futex-based SRW lock implementation:
 *
 * Since we can rely on the kernel to release all threads and don't need to
 * worry about NtReleaseKeyedEvent(), we can simplify the layout a bit. The
 * layout looks like this:
 *
 *    31 - Exclusive lock bit, set if the resource is owned exclusively.
 * 30-16 - Number of exclusive waiters. Unlike the fallback implementation,
 *         this does not include the thread owning the lock, or shared threads
 *         waiting on the lock.
 *    15 - Does this lock have any shared waiters? We use this as an
 *         optimization to avoid unnecessary FUTEX_WAKE_BITSET calls when
 *         releasing an exclusive lock.
 *  14-0 - Number of shared owners. Unlike the fallback implementation, this
 *         does not include the number of shared threads waiting on the lock.
 *         Thus the state [1, x, >=1] will never occur.
 */

#define SRWLOCK_FUTEX_EXCLUSIVE_LOCK_BIT        0x80000000
#define SRWLOCK_FUTEX_EXCLUSIVE_WAITERS_MASK    0x7fff0000
#define SRWLOCK_FUTEX_EXCLUSIVE_WAITERS_INC     0x00010000
#define SRWLOCK_FUTEX_SHARED_WAITERS_BIT        0x00008000
#define SRWLOCK_FUTEX_SHARED_OWNERS_MASK        0x00007fff
#define SRWLOCK_FUTEX_SHARED_OWNERS_INC         0x00000001

/* Futex bitmasks; these are independent from the bits in the lock itself. */
#define SRWLOCK_FUTEX_BITSET_EXCLUSIVE  1
#define SRWLOCK_FUTEX_BITSET_SHARED     2

static NTSTATUS fast_try_acquire_srw_exclusive( RTL_SRWLOCK *lock )
{
    int old, new, *futex;
    NTSTATUS ret;

    if (!use_futexes()) return STATUS_NOT_IMPLEMENTED;

    if (!(futex = get_futex( &lock->Ptr )))
        return STATUS_NOT_IMPLEMENTED;

    do
    {
        old = *futex;

        if (!(old & SRWLOCK_FUTEX_EXCLUSIVE_LOCK_BIT)
                && !(old & SRWLOCK_FUTEX_SHARED_OWNERS_MASK))
        {
            /* Not locked exclusive or shared. We can try to grab it. */
            new = old | SRWLOCK_FUTEX_EXCLUSIVE_LOCK_BIT;
            ret = STATUS_SUCCESS;
        }
        else
        {
            new = old;
            ret = STATUS_TIMEOUT;
        }
    } while (InterlockedCompareExchange( futex, new, old ) != old);

    return ret;
}

static NTSTATUS fast_acquire_srw_exclusive( RTL_SRWLOCK *lock )
{
    int old, new, *futex;
    BOOLEAN wait;

    if (!use_futexes()) return STATUS_NOT_IMPLEMENTED;

    if (!(futex = get_futex( &lock->Ptr )))
        return STATUS_NOT_IMPLEMENTED;

    /* Atomically increment the exclusive waiter count. */
    do
    {
        old = *futex;
        new = old + SRWLOCK_FUTEX_EXCLUSIVE_WAITERS_INC;
        assert(new & SRWLOCK_FUTEX_EXCLUSIVE_WAITERS_MASK);
    } while (InterlockedCompareExchange( futex, new, old ) != old);

    for (;;)
    {
        do
        {
            old = *futex;

            if (!(old & SRWLOCK_FUTEX_EXCLUSIVE_LOCK_BIT)
                    && !(old & SRWLOCK_FUTEX_SHARED_OWNERS_MASK))
            {
                /* Not locked exclusive or shared. We can try to grab it. */
                new = old | SRWLOCK_FUTEX_EXCLUSIVE_LOCK_BIT;
                assert(old & SRWLOCK_FUTEX_EXCLUSIVE_WAITERS_MASK);
                new -= SRWLOCK_FUTEX_EXCLUSIVE_WAITERS_INC;
                wait = FALSE;
            }
            else
            {
                new = old;
                wait = TRUE;
            }
        } while (InterlockedCompareExchange( futex, new, old ) != old);

        if (!wait)
            return STATUS_SUCCESS;

        futex_wait_bitset( futex, new, NULL, SRWLOCK_FUTEX_BITSET_EXCLUSIVE );
    }

    return STATUS_SUCCESS;
}

static NTSTATUS fast_try_acquire_srw_shared( RTL_SRWLOCK *lock )
{
    int new, old, *futex;
    NTSTATUS ret;

    if (!use_futexes()) return STATUS_NOT_IMPLEMENTED;

    if (!(futex = get_futex( &lock->Ptr )))
        return STATUS_NOT_IMPLEMENTED;

    do
    {
        old = *futex;

        if (!(old & SRWLOCK_FUTEX_EXCLUSIVE_LOCK_BIT)
                && !(old & SRWLOCK_FUTEX_EXCLUSIVE_WAITERS_MASK))
        {
            /* Not locked exclusive, and no exclusive waiters. We can try to
             * grab it. */
            new = old + SRWLOCK_FUTEX_SHARED_OWNERS_INC;
            assert(new & SRWLOCK_FUTEX_SHARED_OWNERS_MASK);
            ret = STATUS_SUCCESS;
        }
        else
        {
            new = old;
            ret = STATUS_TIMEOUT;
        }
    } while (InterlockedCompareExchange( futex, new, old ) != old);

    return ret;
}

static NTSTATUS fast_acquire_srw_shared( RTL_SRWLOCK *lock )
{
    int old, new, *futex;
    BOOLEAN wait;

    if (!use_futexes()) return STATUS_NOT_IMPLEMENTED;

    if (!(futex = get_futex( &lock->Ptr )))
        return STATUS_NOT_IMPLEMENTED;

    for (;;)
    {
        do
        {
            old = *futex;

            if (!(old & SRWLOCK_FUTEX_EXCLUSIVE_LOCK_BIT)
                    && !(old & SRWLOCK_FUTEX_EXCLUSIVE_WAITERS_MASK))
            {
                /* Not locked exclusive, and no exclusive waiters. We can try
                 * to grab it. */
                new = old + SRWLOCK_FUTEX_SHARED_OWNERS_INC;
                assert(new & SRWLOCK_FUTEX_SHARED_OWNERS_MASK);
                wait = FALSE;
            }
            else
            {
                new = old | SRWLOCK_FUTEX_SHARED_WAITERS_BIT;
                wait = TRUE;
            }
        } while (InterlockedCompareExchange( futex, new, old ) != old);

        if (!wait)
            return STATUS_SUCCESS;

        futex_wait_bitset( futex, new, NULL, SRWLOCK_FUTEX_BITSET_SHARED );
    }

    return STATUS_SUCCESS;
}

static NTSTATUS fast_release_srw_exclusive( RTL_SRWLOCK *lock )
{
    int old, new, *futex;

    if (!use_futexes()) return STATUS_NOT_IMPLEMENTED;

    if (!(futex = get_futex( &lock->Ptr )))
        return STATUS_NOT_IMPLEMENTED;

    do
    {
        old = *futex;

        if (!(old & SRWLOCK_FUTEX_EXCLUSIVE_LOCK_BIT))
        {
            ERR("Lock %p is not owned exclusive! (%#x)\n", lock, *futex);
            return STATUS_RESOURCE_NOT_OWNED;
        }

        new = old & ~SRWLOCK_FUTEX_EXCLUSIVE_LOCK_BIT;

        if (!(new & SRWLOCK_FUTEX_EXCLUSIVE_WAITERS_MASK))
            new &= ~SRWLOCK_FUTEX_SHARED_WAITERS_BIT;
    } while (InterlockedCompareExchange( futex, new, old ) != old);

    if (new & SRWLOCK_FUTEX_EXCLUSIVE_WAITERS_MASK)
        futex_wake_bitset( futex, 1, SRWLOCK_FUTEX_BITSET_EXCLUSIVE );
    else if (old & SRWLOCK_FUTEX_SHARED_WAITERS_BIT)
        futex_wake_bitset( futex, INT_MAX, SRWLOCK_FUTEX_BITSET_SHARED );

    return STATUS_SUCCESS;
}

static NTSTATUS fast_release_srw_shared( RTL_SRWLOCK *lock )
{
    int old, new, *futex;

    if (!use_futexes()) return STATUS_NOT_IMPLEMENTED;

    if (!(futex = get_futex( &lock->Ptr )))
        return STATUS_NOT_IMPLEMENTED;

    do
    {
        old = *futex;

        if (old & SRWLOCK_FUTEX_EXCLUSIVE_LOCK_BIT)
        {
            ERR("Lock %p is owned exclusive! (%#x)\n", lock, *futex);
            return STATUS_RESOURCE_NOT_OWNED;
        }
        else if (!(old & SRWLOCK_FUTEX_SHARED_OWNERS_MASK))
        {
            ERR("Lock %p is not owned shared! (%#x)\n", lock, *futex);
            return STATUS_RESOURCE_NOT_OWNED;
        }

        new = old - SRWLOCK_FUTEX_SHARED_OWNERS_INC;
    } while (InterlockedCompareExchange( futex, new, old ) != old);

    /* Optimization: only bother waking if there are actually exclusive waiters. */
    if (!(new & SRWLOCK_FUTEX_SHARED_OWNERS_MASK) && (new & SRWLOCK_FUTEX_EXCLUSIVE_WAITERS_MASK))
        futex_wake_bitset( futex, 1, SRWLOCK_FUTEX_BITSET_EXCLUSIVE );

    return STATUS_SUCCESS;
}

#else

static NTSTATUS fast_try_acquire_srw_exclusive( RTL_SRWLOCK *lock )
{
    return STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS fast_acquire_srw_exclusive( RTL_SRWLOCK *lock )
{
    return STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS fast_try_acquire_srw_shared( RTL_SRWLOCK *lock )
{
    return STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS fast_acquire_srw_shared( RTL_SRWLOCK *lock )
{
    return STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS fast_release_srw_exclusive( RTL_SRWLOCK *lock )
{
    return STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS fast_release_srw_shared( RTL_SRWLOCK *lock )
{
    return STATUS_NOT_IMPLEMENTED;
}

#endif

/* SRW locks implementation
 *
 * The memory layout used by the lock is:
 *
 * 32 31            16               0
 *  ________________ ________________
 * | X| #exclusive  |    #shared     |
 *  ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
 * Since there is no space left for a separate counter of shared access
 * threads inside the locked section the #shared field is used for multiple
 * purposes. The following table lists all possible states the lock can be
 * in, notation: [X, #exclusive, #shared]:
 *
 * [0,   0,   N] -> locked by N shared access threads, if N=0 it's unlocked
 * [0, >=1, >=1] -> threads are requesting exclusive locks, but there are
 * still shared access threads inside. #shared should not be incremented
 * anymore!
 * [1, >=1, >=0] -> lock is owned by an exclusive thread and the #shared
 * counter can be used again to count the number of threads waiting in the
 * queue for shared access.
 *
 * the following states are invalid and will never occur:
 * [0, >=1,   0], [1,   0, >=0]
 *
 * The main problem arising from the fact that we have no separate counter
 * of shared access threads inside the locked section is that in the state
 * [0, >=1, >=1] above we cannot add additional waiting threads to the
 * shared access queue - it wouldn't be possible to distinguish waiting
 * threads and those that are still inside. To solve this problem the lock
 * uses the following approach: a thread that isn't able to allocate a
 * shared lock just uses the exclusive queue instead. As soon as the thread
 * is woken up it is in the state [1, >=1, >=0]. In this state it's again
 * possible to use the shared access queue. The thread atomically moves
 * itself to the shared access queue and releases the exclusive lock, so
 * that the "real" exclusive access threads have a chance. As soon as they
 * are all ready the shared access threads are processed.
 */

#define SRWLOCK_MASK_IN_EXCLUSIVE     0x80000000
#define SRWLOCK_MASK_EXCLUSIVE_QUEUE  0x7fff0000
#define SRWLOCK_MASK_SHARED_QUEUE     0x0000ffff
#define SRWLOCK_RES_EXCLUSIVE         0x00010000
#define SRWLOCK_RES_SHARED            0x00000001

#ifdef WORDS_BIGENDIAN
#define srwlock_key_exclusive(lock)   ((void *)(((ULONG_PTR)&lock->Ptr + 1) & ~1))
#define srwlock_key_shared(lock)      ((void *)(((ULONG_PTR)&lock->Ptr + 3) & ~1))
#else
#define srwlock_key_exclusive(lock)   ((void *)(((ULONG_PTR)&lock->Ptr + 3) & ~1))
#define srwlock_key_shared(lock)      ((void *)(((ULONG_PTR)&lock->Ptr + 1) & ~1))
#endif

static inline void srwlock_check_invalid( unsigned int val )
{
    /* Throw exception if it's impossible to acquire/release this lock. */
    if ((val & SRWLOCK_MASK_EXCLUSIVE_QUEUE) == SRWLOCK_MASK_EXCLUSIVE_QUEUE ||
            (val & SRWLOCK_MASK_SHARED_QUEUE) == SRWLOCK_MASK_SHARED_QUEUE)
        RtlRaiseStatus(STATUS_RESOURCE_NOT_OWNED);
}

static inline unsigned int srwlock_lock_exclusive( unsigned int *dest, int incr )
{
    unsigned int val, tmp;
    /* Atomically modifies the value of *dest by adding incr. If the shared
     * queue is empty and there are threads waiting for exclusive access, then
     * sets the mark SRWLOCK_MASK_IN_EXCLUSIVE to signal other threads that
     * they are allowed again to use the shared queue counter. */
    for (val = *dest;; val = tmp)
    {
        tmp = val + incr;
        srwlock_check_invalid( tmp );
        if ((tmp & SRWLOCK_MASK_EXCLUSIVE_QUEUE) && !(tmp & SRWLOCK_MASK_SHARED_QUEUE))
            tmp |= SRWLOCK_MASK_IN_EXCLUSIVE;
        if ((tmp = InterlockedCompareExchange( (int *)dest, tmp, val )) == val)
            break;
    }
    return val;
}

static inline unsigned int srwlock_unlock_exclusive( unsigned int *dest, int incr )
{
    unsigned int val, tmp;
    /* Atomically modifies the value of *dest by adding incr. If the queue of
     * threads waiting for exclusive access is empty, then remove the
     * SRWLOCK_MASK_IN_EXCLUSIVE flag (only the shared queue counter will
     * remain). */
    for (val = *dest;; val = tmp)
    {
        tmp = val + incr;
        srwlock_check_invalid( tmp );
        if (!(tmp & SRWLOCK_MASK_EXCLUSIVE_QUEUE))
            tmp &= SRWLOCK_MASK_SHARED_QUEUE;
        if ((tmp = InterlockedCompareExchange( (int *)dest, tmp, val )) == val)
            break;
    }
    return val;
}

static inline void srwlock_leave_exclusive( RTL_SRWLOCK *lock, unsigned int val )
{
    /* Used when a thread leaves an exclusive section. If there are other
     * exclusive access threads they are processed first, followed by
     * the shared waiters. */
    if (val & SRWLOCK_MASK_EXCLUSIVE_QUEUE)
        NtReleaseKeyedEvent( 0, srwlock_key_exclusive(lock), FALSE, NULL );
    else
    {
        val &= SRWLOCK_MASK_SHARED_QUEUE; /* remove SRWLOCK_MASK_IN_EXCLUSIVE */
        while (val--)
            NtReleaseKeyedEvent( 0, srwlock_key_shared(lock), FALSE, NULL );
    }
}

static inline void srwlock_leave_shared( RTL_SRWLOCK *lock, unsigned int val )
{
    /* Wake up one exclusive thread as soon as the last shared access thread
     * has left. */
    if ((val & SRWLOCK_MASK_EXCLUSIVE_QUEUE) && !(val & SRWLOCK_MASK_SHARED_QUEUE))
        NtReleaseKeyedEvent( 0, srwlock_key_exclusive(lock), FALSE, NULL );
}

/***********************************************************************
 *              RtlInitializeSRWLock (NTDLL.@)
 *
 * NOTES
 *  Please note that SRWLocks do not keep track of the owner of a lock.
 *  It doesn't make any difference which thread for example unlocks an
 *  SRWLock (see corresponding tests). This implementation uses two
 *  keyed events (one for the exclusive waiters and one for the shared
 *  waiters) and is limited to 2^15-1 waiting threads.
 */
void WINAPI RtlInitializeSRWLock( RTL_SRWLOCK *lock )
{
    lock->Ptr = NULL;
}

/***********************************************************************
 *              RtlAcquireSRWLockExclusive (NTDLL.@)
 *
 * NOTES
 *  Unlike RtlAcquireResourceExclusive this function doesn't allow
 *  nested calls from the same thread. "Upgrading" a shared access lock
 *  to an exclusive access lock also doesn't seem to be supported.
 */
void WINAPI RtlAcquireSRWLockExclusive( RTL_SRWLOCK *lock )
{
    if (fast_acquire_srw_exclusive( lock ) != STATUS_NOT_IMPLEMENTED)
        return;

    if (srwlock_lock_exclusive( (unsigned int *)&lock->Ptr, SRWLOCK_RES_EXCLUSIVE ))
        NtWaitForKeyedEvent( 0, srwlock_key_exclusive(lock), FALSE, NULL );
}

/***********************************************************************
 *              RtlAcquireSRWLockShared (NTDLL.@)
 *
 * NOTES
 *   Do not call this function recursively - it will only succeed when
 *   there are no threads waiting for an exclusive lock!
 */
void WINAPI RtlAcquireSRWLockShared( RTL_SRWLOCK *lock )
{
    unsigned int val, tmp;

    if (fast_acquire_srw_shared( lock ) != STATUS_NOT_IMPLEMENTED)
        return;

    /* Acquires a shared lock. If it's currently not possible to add elements to
     * the shared queue, then request exclusive access instead. */
    for (val = *(unsigned int *)&lock->Ptr;; val = tmp)
    {
        if ((val & SRWLOCK_MASK_EXCLUSIVE_QUEUE) && !(val & SRWLOCK_MASK_IN_EXCLUSIVE))
            tmp = val + SRWLOCK_RES_EXCLUSIVE;
        else
            tmp = val + SRWLOCK_RES_SHARED;
        if ((tmp = InterlockedCompareExchange( (int *)&lock->Ptr, tmp, val )) == val)
            break;
    }

    /* Drop exclusive access again and instead requeue for shared access. */
    if ((val & SRWLOCK_MASK_EXCLUSIVE_QUEUE) && !(val & SRWLOCK_MASK_IN_EXCLUSIVE))
    {
        NtWaitForKeyedEvent( 0, srwlock_key_exclusive(lock), FALSE, NULL );
        val = srwlock_unlock_exclusive( (unsigned int *)&lock->Ptr, (SRWLOCK_RES_SHARED
                                        - SRWLOCK_RES_EXCLUSIVE) ) - SRWLOCK_RES_EXCLUSIVE;
        srwlock_leave_exclusive( lock, val );
    }

    if (val & SRWLOCK_MASK_EXCLUSIVE_QUEUE)
        NtWaitForKeyedEvent( 0, srwlock_key_shared(lock), FALSE, NULL );
}

/***********************************************************************
 *              RtlReleaseSRWLockExclusive (NTDLL.@)
 */
void WINAPI RtlReleaseSRWLockExclusive( RTL_SRWLOCK *lock )
{
    if (fast_release_srw_exclusive( lock ) != STATUS_NOT_IMPLEMENTED)
        return;

    srwlock_leave_exclusive( lock, srwlock_unlock_exclusive( (unsigned int *)&lock->Ptr,
                             - SRWLOCK_RES_EXCLUSIVE ) - SRWLOCK_RES_EXCLUSIVE );
}

/***********************************************************************
 *              RtlReleaseSRWLockShared (NTDLL.@)
 */
void WINAPI RtlReleaseSRWLockShared( RTL_SRWLOCK *lock )
{
    if (fast_release_srw_shared( lock ) != STATUS_NOT_IMPLEMENTED)
        return;

    srwlock_leave_shared( lock, srwlock_lock_exclusive( (unsigned int *)&lock->Ptr,
                          - SRWLOCK_RES_SHARED ) - SRWLOCK_RES_SHARED );
}

/***********************************************************************
 *              RtlTryAcquireSRWLockExclusive (NTDLL.@)
 *
 * NOTES
 *  Similarly to AcquireSRWLockExclusive, recursive calls are not allowed
 *  and will fail with a FALSE return value.
 */
BOOLEAN WINAPI RtlTryAcquireSRWLockExclusive( RTL_SRWLOCK *lock )
{
    NTSTATUS ret;

    if ((ret = fast_try_acquire_srw_exclusive( lock )) != STATUS_NOT_IMPLEMENTED)
        return (ret == STATUS_SUCCESS);

    return InterlockedCompareExchange( (int *)&lock->Ptr, SRWLOCK_MASK_IN_EXCLUSIVE |
                                       SRWLOCK_RES_EXCLUSIVE, 0 ) == 0;
}

/***********************************************************************
 *              RtlTryAcquireSRWLockShared (NTDLL.@)
 */
BOOLEAN WINAPI RtlTryAcquireSRWLockShared( RTL_SRWLOCK *lock )
{
    unsigned int val, tmp;
    NTSTATUS ret;

    if ((ret = fast_try_acquire_srw_shared( lock )) != STATUS_NOT_IMPLEMENTED)
        return (ret == STATUS_SUCCESS);

    for (val = *(unsigned int *)&lock->Ptr;; val = tmp)
    {
        if (val & SRWLOCK_MASK_EXCLUSIVE_QUEUE)
            return FALSE;
        if ((tmp = InterlockedCompareExchange( (int *)&lock->Ptr, val + SRWLOCK_RES_SHARED, val )) == val)
            break;
    }
    return TRUE;
}

#ifdef __linux__
static NTSTATUS fast_wait_cv( int *futex, int val, const LARGE_INTEGER *timeout )
{
    struct timespec timespec;
    int ret;

    if (timeout && timeout->QuadPart != TIMEOUT_INFINITE)
    {
        timespec_from_timeout( &timespec, timeout );
        ret = futex_wait( futex, val, &timespec );
    }
    else
        ret = futex_wait( futex, val, NULL );

    if (ret == -1 && errno == ETIMEDOUT)
        return STATUS_TIMEOUT;
    return STATUS_WAIT_0;
}

static NTSTATUS fast_sleep_cs_cv( RTL_CONDITION_VARIABLE *variable,
        RTL_CRITICAL_SECTION *cs, const LARGE_INTEGER *timeout )
{
    NTSTATUS status;
    int val, *futex;

    if (!use_futexes())
        return STATUS_NOT_IMPLEMENTED;

    if (!(futex = get_futex( &variable->Ptr )))
        return STATUS_NOT_IMPLEMENTED;

    val = *futex;

    RtlLeaveCriticalSection( cs );
    status = fast_wait_cv( futex, val, timeout );
    RtlEnterCriticalSection( cs );
    return status;
}

static NTSTATUS fast_sleep_srw_cv( RTL_CONDITION_VARIABLE *variable,
        RTL_SRWLOCK *lock, const LARGE_INTEGER *timeout, ULONG flags )
{
    NTSTATUS status;
    int val, *futex;

    if (!use_futexes())
        return STATUS_NOT_IMPLEMENTED;

    if (!(futex = get_futex( &variable->Ptr )))
        return STATUS_NOT_IMPLEMENTED;

    val = *futex;

    if (flags & RTL_CONDITION_VARIABLE_LOCKMODE_SHARED)
        RtlReleaseSRWLockShared( lock );
    else
        RtlReleaseSRWLockExclusive( lock );

    status = fast_wait_cv( futex, val, timeout );

    if (flags & RTL_CONDITION_VARIABLE_LOCKMODE_SHARED)
        RtlAcquireSRWLockShared( lock );
    else
        RtlAcquireSRWLockExclusive( lock );
    return status;
}

static NTSTATUS fast_wake_cv( RTL_CONDITION_VARIABLE *variable, int count )
{
    int *futex;

    if (!use_futexes()) return STATUS_NOT_IMPLEMENTED;

    if (!(futex = get_futex( &variable->Ptr )))
        return STATUS_NOT_IMPLEMENTED;

    InterlockedIncrement( futex );
    futex_wake( futex, count );
    return STATUS_SUCCESS;
}
#else
static NTSTATUS fast_sleep_srw_cv( RTL_CONDITION_VARIABLE *variable,
        RTL_SRWLOCK *lock, const LARGE_INTEGER *timeout, ULONG flags )
{
    return STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS fast_sleep_cs_cv( RTL_CONDITION_VARIABLE *variable,
        RTL_CRITICAL_SECTION *cs, const LARGE_INTEGER *timeout )
{
    return STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS fast_wake_cv( RTL_CONDITION_VARIABLE *variable, int count )
{
    return STATUS_NOT_IMPLEMENTED;
}
#endif

/***********************************************************************
 *           RtlInitializeConditionVariable   (NTDLL.@)
 *
 * Initializes the condition variable with NULL.
 *
 * PARAMS
 *  variable [O] condition variable
 *
 * RETURNS
 *  Nothing.
 */
void WINAPI RtlInitializeConditionVariable( RTL_CONDITION_VARIABLE *variable )
{
    variable->Ptr = NULL;
}

/***********************************************************************
 *           RtlWakeConditionVariable   (NTDLL.@)
 *
 * Wakes up one thread waiting on the condition variable.
 *
 * PARAMS
 *  variable [I/O] condition variable to wake up.
 *
 * RETURNS
 *  Nothing.
 *
 * NOTES
 *  The calling thread does not have to own any lock in order to call
 *  this function.
 */
void WINAPI RtlWakeConditionVariable( RTL_CONDITION_VARIABLE *variable )
{
    if (fast_wake_cv( variable, 1 ) == STATUS_NOT_IMPLEMENTED)
    {
        InterlockedIncrement( (int *)&variable->Ptr );
        RtlWakeAddressSingle( variable );
    }
}

/***********************************************************************
 *           RtlWakeAllConditionVariable   (NTDLL.@)
 *
 * See WakeConditionVariable, wakes up all waiting threads.
 */
void WINAPI RtlWakeAllConditionVariable( RTL_CONDITION_VARIABLE *variable )
{
    if (fast_wake_cv( variable, INT_MAX ) == STATUS_NOT_IMPLEMENTED)
    {
        InterlockedIncrement( (int *)&variable->Ptr );
        RtlWakeAddressAll( variable );
    }
}

/***********************************************************************
 *           RtlSleepConditionVariableCS   (NTDLL.@)
 *
 * Atomically releases the critical section and suspends the thread,
 * waiting for a Wake(All)ConditionVariable event. Afterwards it enters
 * the critical section again and returns.
 *
 * PARAMS
 *  variable  [I/O] condition variable
 *  crit      [I/O] critical section to leave temporarily
 *  timeout   [I]   timeout
 *
 * RETURNS
 *  see NtWaitForKeyedEvent for all possible return values.
 */
NTSTATUS WINAPI RtlSleepConditionVariableCS( RTL_CONDITION_VARIABLE *variable, RTL_CRITICAL_SECTION *crit,
                                             const LARGE_INTEGER *timeout )
{
    NTSTATUS status;
    int val;

    if ((status = fast_sleep_cs_cv( variable, crit, timeout )) != STATUS_NOT_IMPLEMENTED)
        return status;

    val = *(int *)&variable->Ptr;
    RtlLeaveCriticalSection( crit );
    status = RtlWaitOnAddress( &variable->Ptr, &val, sizeof(int), timeout );
    RtlEnterCriticalSection( crit );
    return status;
}

/***********************************************************************
 *           RtlSleepConditionVariableSRW   (NTDLL.@)
 *
 * Atomically releases the SRWLock and suspends the thread,
 * waiting for a Wake(All)ConditionVariable event. Afterwards it enters
 * the SRWLock again with the same access rights and returns.
 *
 * PARAMS
 *  variable  [I/O] condition variable
 *  lock      [I/O] SRWLock to leave temporarily
 *  timeout   [I]   timeout
 *  flags     [I]   type of the current lock (exclusive / shared)
 *
 * RETURNS
 *  see NtWaitForKeyedEvent for all possible return values.
 *
 * NOTES
 *  the behaviour is undefined if the thread doesn't own the lock.
 */
NTSTATUS WINAPI RtlSleepConditionVariableSRW( RTL_CONDITION_VARIABLE *variable, RTL_SRWLOCK *lock,
                                              const LARGE_INTEGER *timeout, ULONG flags )
{
    NTSTATUS status;
    int val;

    if ((status = fast_sleep_srw_cv( variable, lock, timeout, flags )) != STATUS_NOT_IMPLEMENTED)
        return status;

    val = *(int *)&variable->Ptr;

    if (flags & RTL_CONDITION_VARIABLE_LOCKMODE_SHARED)
        RtlReleaseSRWLockShared( lock );
    else
        RtlReleaseSRWLockExclusive( lock );

    status = RtlWaitOnAddress( &variable->Ptr, &val, sizeof(int), timeout );

    if (flags & RTL_CONDITION_VARIABLE_LOCKMODE_SHARED)
        RtlAcquireSRWLockShared( lock );
    else
        RtlAcquireSRWLockExclusive( lock );
    return status;
}

static RTL_CRITICAL_SECTION addr_section;
static RTL_CRITICAL_SECTION_DEBUG addr_section_debug =
{
    0, 0, &addr_section,
    { &addr_section_debug.ProcessLocksList, &addr_section_debug.ProcessLocksList },
      0, 0, { (DWORD_PTR)(__FILE__ ": addr_section") }
};
static RTL_CRITICAL_SECTION addr_section = { &addr_section_debug, -1, 0, 0, 0, 0 };

static BOOL compare_addr( const void *addr, const void *cmp, SIZE_T size )
{
    switch (size)
    {
        case 1:
            return (*(const UCHAR *)addr == *(const UCHAR *)cmp);
        case 2:
            return (*(const USHORT *)addr == *(const USHORT *)cmp);
        case 4:
            return (*(const ULONG *)addr == *(const ULONG *)cmp);
        case 8:
            return (*(const ULONG64 *)addr == *(const ULONG64 *)cmp);
    }

    return FALSE;
}

#ifdef __linux__
/* We can't map addresses to futex directly, because an application can wait on
 * 8 bytes, and we can't pass all 8 as the compare value to futex(). Instead we
 * map all addresses to a small fixed table of futexes. This may result in
 * spurious wakes, but the application is already expected to handle those. */

static int addr_futex_table[256];

static inline int *hash_addr( const void *addr )
{
    ULONG_PTR val = (ULONG_PTR)addr;

    return &addr_futex_table[(val >> 2) & 255];
}

static inline NTSTATUS fast_wait_addr( const void *addr, const void *cmp, SIZE_T size,
                                       const LARGE_INTEGER *timeout )
{
    int *futex;
    int val;
    struct timespec timespec;
    int ret;

    if (!use_futexes())
        return STATUS_NOT_IMPLEMENTED;

    futex = hash_addr( addr );

    /* We must read the previous value of the futex before checking the value
     * of the address being waited on. That way, if we receive a wake between
     * now and waiting on the futex, we know that val will have changed.
     * Use an atomic load so that memory accesses are ordered between this read
     * and the increment below. */
    val = InterlockedCompareExchange( futex, 0, 0 );
    if (!compare_addr( addr, cmp, size ))
        return STATUS_SUCCESS;

    if (timeout)
    {
        timespec_from_timeout( &timespec, timeout );
        ret = futex_wait( futex, val, &timespec );
    }
    else
        ret = futex_wait( futex, val, NULL );

    if (ret == -1 && errno == ETIMEDOUT)
        return STATUS_TIMEOUT;
    return STATUS_SUCCESS;
}

static inline NTSTATUS fast_wake_addr( const void *addr )
{
    int *futex;

    if (!use_futexes())
        return STATUS_NOT_IMPLEMENTED;

    futex = hash_addr( addr );

    InterlockedIncrement( futex );

    futex_wake( futex, INT_MAX );
    return STATUS_SUCCESS;
}
#else
static inline NTSTATUS fast_wait_addr( const void *addr, const void *cmp, SIZE_T size,
                                       const LARGE_INTEGER *timeout )
{
    return STATUS_NOT_IMPLEMENTED;
}

static inline NTSTATUS fast_wake_addr( const void *addr )
{
    return STATUS_NOT_IMPLEMENTED;
}
#endif

/***********************************************************************
 *           RtlWaitOnAddress   (NTDLL.@)
 */
NTSTATUS WINAPI RtlWaitOnAddress( const void *addr, const void *cmp, SIZE_T size,
                                  const LARGE_INTEGER *timeout )
{
    select_op_t select_op;
    NTSTATUS ret;
    timeout_t abs_timeout = timeout ? timeout->QuadPart : TIMEOUT_INFINITE;

    if (size != 1 && size != 2 && size != 4 && size != 8)
        return STATUS_INVALID_PARAMETER;

    if ((ret = fast_wait_addr( addr, cmp, size, timeout )) != STATUS_NOT_IMPLEMENTED)
        return ret;

    RtlEnterCriticalSection( &addr_section );
    if (!compare_addr( addr, cmp, size ))
    {
        RtlLeaveCriticalSection( &addr_section );
        return STATUS_SUCCESS;
    }

    if (abs_timeout < 0)
    {
        LARGE_INTEGER now;

        RtlQueryPerformanceCounter(&now);
        abs_timeout -= now.QuadPart;
    }

    select_op.keyed_event.op     = SELECT_KEYED_EVENT_WAIT;
    select_op.keyed_event.handle = wine_server_obj_handle( keyed_event );
    select_op.keyed_event.key    = wine_server_client_ptr( addr );

    return server_select( &select_op, sizeof(select_op.keyed_event), SELECT_INTERRUPTIBLE, abs_timeout, NULL, &addr_section, NULL );
}

/***********************************************************************
 *           RtlWakeAddressAll    (NTDLL.@)
 */
void WINAPI RtlWakeAddressAll( const void *addr )
{
    if (fast_wake_addr( addr ) != STATUS_NOT_IMPLEMENTED)
        return;

    RtlEnterCriticalSection( &addr_section );
    while (NtReleaseKeyedEvent( 0, addr, 0, &zero_timeout ) == STATUS_SUCCESS) {}
    RtlLeaveCriticalSection( &addr_section );
}

/***********************************************************************
 *           RtlWakeAddressSingle (NTDLL.@)
 */
void WINAPI RtlWakeAddressSingle( const void *addr )
{
    if (fast_wake_addr( addr ) != STATUS_NOT_IMPLEMENTED)
        return;

    RtlEnterCriticalSection( &addr_section );
    NtReleaseKeyedEvent( 0, addr, 0, &zero_timeout );
    RtlLeaveCriticalSection( &addr_section );
}
