/*
 * fltmgr.sys
 *
 * Copyright 2015 Austin English
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

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winternl.h"
#include "ddk/fltkernel.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(fltmgr);

NTSTATUS WINAPI DriverEntry( DRIVER_OBJECT *driver, UNICODE_STRING *path )
{
    TRACE( "(%p, %s)\n", driver, debugstr_w(path->Buffer) );

    return STATUS_SUCCESS;
}

void WINAPI FltInitializePushLock( EX_PUSH_LOCK *lock )
{
    FIXME( "(%p): stub\n", lock );
}

void WINAPI FltDeletePushLock( EX_PUSH_LOCK *lock )
{
    FIXME( "(%p): stub\n", lock );
}

void WINAPI FltAcquirePushLockExclusive( EX_PUSH_LOCK *lock )
{
    FIXME( "(%p): stub\n", lock );
}

void WINAPI FltReleasePushLock( EX_PUSH_LOCK *lock )
{
    FIXME( "(%p): stub\n", lock );
}

NTSTATUS WINAPI FltRegisterFilter( PDRIVER_OBJECT driver, const FLT_REGISTRATION *reg, PFLT_FILTER *filter )
{
    FIXME( "(%p,%p,%p): stub\n", driver, reg, filter );

    if(filter)
        *filter = UlongToHandle(0xdeadbeaf);

    return STATUS_SUCCESS;
}

NTSTATUS WINAPI FltStartFiltering( PFLT_FILTER filter )
{
    FIXME( "(%p): stub\n", filter );

    return STATUS_SUCCESS;
}

void WINAPI FltUnregisterFilter( PFLT_FILTER filter )
{
    FIXME( "(%p): stub\n", filter );
}

void* WINAPI FltGetRoutineAddress(LPCSTR name)
{
    HMODULE mod = GetModuleHandleW(L"fltmgr.sys");
    void *func;

    func = GetProcAddress(mod, name);
    if (func)
        TRACE( "%s -> %p\n", debugstr_a(name), func );
    else
        FIXME( "%s not found\n", debugstr_a(name) );

    return func;
}

NTSTATUS WINAPI FltBuildDefaultSecurityDescriptor(PSECURITY_DESCRIPTOR *descriptor, ACCESS_MASK access)
{
    PACL dacl;
    NTSTATUS ret = STATUS_INSUFFICIENT_RESOURCES;
    ULONG sid_len;
    PSID sid;
    PSID sid_system;
    PSECURITY_DESCRIPTOR sec_desc = NULL;
    SID_IDENTIFIER_AUTHORITY auth = { SECURITY_NULL_SID_AUTHORITY };

    *descriptor = NULL;

    ret = RtlAllocateAndInitializeSid(&auth, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_GROUP_RID_ADMINS,
                                         0, 0, 0, 0, 0, 0, &sid);
    if (ret != STATUS_SUCCESS)
        goto done;

    ret = RtlAllocateAndInitializeSid(&auth, 1, SECURITY_LOCAL_SYSTEM_RID, 0, 0, 0, 0, 0, 0, 0, &sid_system);
    if (ret != STATUS_SUCCESS)
        goto done;

    sid_len = SECURITY_DESCRIPTOR_MIN_LENGTH + sizeof(ACL) +
            sizeof(ACCESS_ALLOWED_ACE) + RtlLengthSid(sid) +
            sizeof(ACCESS_ALLOWED_ACE) + RtlLengthSid(sid_system);

    sec_desc = RtlAllocateHeap(GetProcessHeap(), HEAP_ZERO_MEMORY, sid_len);
    if (!sec_desc)
    {
        ret = STATUS_NO_MEMORY;
        goto done;
    }

    ret = RtlCreateSecurityDescriptor(sec_desc, SECURITY_DESCRIPTOR_REVISION);
    if (ret != STATUS_SUCCESS)
        goto done;

    dacl = (PACL)((char*)sec_desc + SECURITY_DESCRIPTOR_MIN_LENGTH);
    ret = RtlCreateAcl(dacl, sid_len - SECURITY_DESCRIPTOR_MIN_LENGTH, ACL_REVISION);
    if (ret != STATUS_SUCCESS)
        goto done;

    ret = RtlAddAccessAllowedAce(dacl, ACL_REVISION, access, sid);
    if (ret != STATUS_SUCCESS)
        goto done;

    ret = RtlAddAccessAllowedAce(dacl, ACL_REVISION, access, sid_system);
    if (ret != STATUS_SUCCESS)
        goto done;

    ret = RtlSetDaclSecurityDescriptor(sec_desc, 1, dacl, 0);
    if (ret == STATUS_SUCCESS)
        *descriptor = sec_desc;

done:
    if (ret != STATUS_SUCCESS && sec_desc != NULL)
        RtlFreeHeap(GetProcessHeap(), 0, sec_desc);

    if (sid != NULL)
        RtlFreeHeap(GetProcessHeap(), 0, sid);

    if (sid_system != NULL)
        RtlFreeHeap(GetProcessHeap(), 0, sid_system);

    return ret;
}

void WINAPI FltFreeSecurityDescriptor(PSECURITY_DESCRIPTOR descriptor)
{
    RtlFreeHeap(GetProcessHeap(), 0, descriptor);
}