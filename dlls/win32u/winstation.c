/*
 * Window stations and desktops
 *
 * Copyright 2002 Alexandre Julliard
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

#if 0
#pragma makedep unix
#endif

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include <stdarg.h>
#include "windef.h"
#include "winbase.h"
#include "ntuser.h"
#include "ddk/wdm.h"
#include "ntgdi_private.h"
#include "wine/server.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(winstation);


#define DESKTOP_ALL_ACCESS 0x01ff

/***********************************************************************
 *           NtUserCreateWindowStation  (win32u.@)
 */
HWINSTA WINAPI NtUserCreateWindowStation( OBJECT_ATTRIBUTES *attr, ACCESS_MASK access, ULONG arg3,
                                          ULONG arg4, ULONG arg5, ULONG arg6, ULONG arg7 )
{
    HANDLE ret;

    if (attr->ObjectName->Length >= MAX_PATH * sizeof(WCHAR))
    {
        SetLastError( ERROR_FILENAME_EXCED_RANGE );
        return 0;
    }

    SERVER_START_REQ( create_winstation )
    {
        req->flags      = 0;
        req->access     = access;
        req->attributes = attr->Attributes;
        req->rootdir    = wine_server_obj_handle( attr->RootDirectory );
        wine_server_add_data( req, attr->ObjectName->Buffer, attr->ObjectName->Length );
        wine_server_call_err( req );
        ret = wine_server_ptr_handle( reply->handle );
    }
    SERVER_END_REQ;
    return ret;
}

/******************************************************************************
 *           NtUserOpenWindowStation  (win32u.@)
 */
HWINSTA WINAPI NtUserOpenWindowStation( OBJECT_ATTRIBUTES *attr, ACCESS_MASK access )
{
    HANDLE ret = 0;

    SERVER_START_REQ( open_winstation )
    {
        req->access     = access;
        req->attributes = attr->Attributes;
        req->rootdir    = wine_server_obj_handle( attr->RootDirectory );
        wine_server_add_data( req, attr->ObjectName->Buffer, attr->ObjectName->Length );
        if (!wine_server_call_err( req )) ret = wine_server_ptr_handle( reply->handle );
    }
    SERVER_END_REQ;
    return ret;
}

/***********************************************************************
 *           NtUserCloseWindowStation  (win32u.@)
 */
BOOL WINAPI NtUserCloseWindowStation( HWINSTA handle )
{
    BOOL ret;
    SERVER_START_REQ( close_winstation )
    {
        req->handle = wine_server_obj_handle( handle );
        ret = !wine_server_call_err( req );
    }
    SERVER_END_REQ;
    return ret;
}

/***********************************************************************
 *           NtUSerGetProcessWindowStation  (win32u.@)
 */
HWINSTA WINAPI NtUserGetProcessWindowStation(void)
{
    HWINSTA ret = 0;

    SERVER_START_REQ( get_process_winstation )
    {
        if (!wine_server_call_err( req ))
            ret = wine_server_ptr_handle( reply->handle );
    }
    SERVER_END_REQ;
    return ret;
}

/***********************************************************************
 *           NtUserSetProcessWindowStation  (win32u.@)
 */
BOOL WINAPI NtUserSetProcessWindowStation( HWINSTA handle )
{
    BOOL ret;

    SERVER_START_REQ( set_process_winstation )
    {
        req->handle = wine_server_obj_handle( handle );
        ret = !wine_server_call_err( req );
    }
    SERVER_END_REQ;
    return ret;
}

/***********************************************************************
 *           NtUserCreateDesktopEx   (win32u.@)
 */
HDESK WINAPI NtUserCreateDesktopEx( OBJECT_ATTRIBUTES *attr, UNICODE_STRING *device,
                                    DEVMODEW *devmode, DWORD flags, ACCESS_MASK access,
                                    ULONG heap_size )
{
    HANDLE ret;

    if ((device && device->Length) || devmode)
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return 0;
    }
    if (attr->ObjectName->Length >= MAX_PATH * sizeof(WCHAR))
    {
        SetLastError( ERROR_FILENAME_EXCED_RANGE );
        return 0;
    }
    SERVER_START_REQ( create_desktop )
    {
        req->flags      = flags;
        req->access     = access;
        req->attributes = attr->Attributes;
        wine_server_add_data( req, attr->ObjectName->Buffer, attr->ObjectName->Length );
        wine_server_call_err( req );
        ret = wine_server_ptr_handle( reply->handle );
    }
    SERVER_END_REQ;
    return ret;
}

/***********************************************************************
 *           NtUserOpenDesktop   (win32u.@)
 */
HDESK WINAPI NtUserOpenDesktop( OBJECT_ATTRIBUTES *attr, DWORD flags, ACCESS_MASK access )
{
    HANDLE ret = 0;
    if (attr->ObjectName->Length >= MAX_PATH * sizeof(WCHAR))
    {
        SetLastError( ERROR_FILENAME_EXCED_RANGE );
        return 0;
    }
    SERVER_START_REQ( open_desktop )
    {
        req->winsta     = wine_server_obj_handle( attr->RootDirectory );
        req->flags      = flags;
        req->access     = access;
        req->attributes = attr->Attributes;
        wine_server_add_data( req, attr->ObjectName->Buffer, attr->ObjectName->Length );
        if (!wine_server_call_err( req )) ret = wine_server_ptr_handle( reply->handle );
    }
    SERVER_END_REQ;
    return ret;
 }

/***********************************************************************
 *           NtUserCloseDesktop  (win32u.@)
 */
BOOL WINAPI NtUserCloseDesktop( HDESK handle )
{
    BOOL ret;
    SERVER_START_REQ( close_desktop )
    {
        req->handle = wine_server_obj_handle( handle );
        ret = !wine_server_call_err( req );
    }
    SERVER_END_REQ;
    return ret;
}

/***********************************************************************
 *           NtUserGetThreadDesktop   (win32u.@)
 */
HDESK WINAPI NtUserGetThreadDesktop( DWORD thread )
{
    HDESK ret = 0;

    SERVER_START_REQ( get_thread_desktop )
    {
        req->tid = thread;
        if (!wine_server_call_err( req )) ret = wine_server_ptr_handle( reply->handle );
    }
    SERVER_END_REQ;
    return ret;
}

/***********************************************************************
 *           NtUserSetThreadDesktop   (win32u.@)
 */
BOOL WINAPI NtUserSetThreadDesktop( HDESK handle )
{
    BOOL ret;

    SERVER_START_REQ( set_thread_desktop )
    {
        req->handle = wine_server_obj_handle( handle );
        ret = !wine_server_call_err( req );
    }
    SERVER_END_REQ;

    /* FIXME: reset uset thread info */
    return ret;
}

/***********************************************************************
 *           NtUserOpenInputDesktop   (win32u.@)
 */
HDESK WINAPI NtUserOpenInputDesktop( DWORD flags, BOOL inherit, ACCESS_MASK access )
{
    HANDLE ret = 0;

    TRACE( "(%x,%i,%x)\n", flags, inherit, access );

    if (flags)
        FIXME( "partial stub flags %08x\n", flags );

    SERVER_START_REQ( open_input_desktop )
    {
        req->flags      = flags;
        req->access     = access;
        req->attributes = inherit ? OBJ_INHERIT : 0;
        if (!wine_server_call_err( req )) ret = wine_server_ptr_handle( reply->handle );
    }
    SERVER_END_REQ;

    return ret;
}

/***********************************************************************
 *           NtUserGetObjectInformation   (win32u.@)
 */
BOOL WINAPI NtUserGetObjectInformation( HANDLE handle, INT index, void *info,
                                        DWORD len, DWORD *needed )
{
    BOOL ret;

    static const WCHAR desktopW[] = {'D','e','s','k','t','o','p',0};
    static const WCHAR window_stationW[] = {'W','i','n','d','o','w','S','t','a','t','i','o','n',0};

    switch(index)
    {
    case UOI_FLAGS:
        {
            USEROBJECTFLAGS *obj_flags = info;
            if (needed) *needed = sizeof(*obj_flags);
            if (len < sizeof(*obj_flags))
            {
                SetLastError( ERROR_BUFFER_OVERFLOW );
                return FALSE;
            }
            SERVER_START_REQ( set_user_object_info )
            {
                req->handle = wine_server_obj_handle( handle );
                req->flags  = 0;
                ret = !wine_server_call_err( req );
                if (ret)
                {
                    /* FIXME: inherit flag */
                    obj_flags->dwFlags = reply->old_obj_flags;
                }
            }
            SERVER_END_REQ;
        }
        return ret;

    case UOI_TYPE:
        SERVER_START_REQ( set_user_object_info )
        {
            req->handle = wine_server_obj_handle( handle );
            req->flags  = 0;
            ret = !wine_server_call_err( req );
            if (ret)
            {
                size_t size = reply->is_desktop ? sizeof(desktopW) : sizeof(window_stationW);
                if (needed) *needed = size;
                if (len < size)
                {
                    SetLastError( ERROR_INSUFFICIENT_BUFFER );
                    ret = FALSE;
                }
                else memcpy( info, reply->is_desktop ? desktopW : window_stationW, size );
            }
        }
        SERVER_END_REQ;
        return ret;

    case UOI_NAME:
        {
            WCHAR buffer[MAX_PATH];
            SERVER_START_REQ( set_user_object_info )
            {
                req->handle = wine_server_obj_handle( handle );
                req->flags  = 0;
                wine_server_set_reply( req, buffer, sizeof(buffer) - sizeof(WCHAR) );
                ret = !wine_server_call_err( req );
                if (ret)
                {
                    size_t size = wine_server_reply_size( reply );
                    buffer[size / sizeof(WCHAR)] = 0;
                    size += sizeof(WCHAR);
                    if (needed) *needed = size;
                    if (len < size)
                    {
                        SetLastError( ERROR_INSUFFICIENT_BUFFER );
                        ret = FALSE;
                    }
                    else memcpy( info, buffer, size );
                }
            }
            SERVER_END_REQ;
        }
        return ret;

    case UOI_USER_SID:
        FIXME( "not supported index %d\n", index );
        /* fall through */
    default:
        SetLastError( ERROR_INVALID_PARAMETER );
        return FALSE;
    }
}

/***********************************************************************
 *           NtUserSetObjectInformation   (win32u.@)
 */
BOOL WINAPI NtUserSetObjectInformation( HANDLE handle, INT index, void *info, DWORD len )
{
    BOOL ret;
    const USEROBJECTFLAGS *obj_flags = info;

    if (index != UOI_FLAGS || !info || len < sizeof(*obj_flags))
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return FALSE;
    }
    /* FIXME: inherit flag */
    SERVER_START_REQ( set_user_object_info )
    {
        req->handle    = wine_server_obj_handle( handle );
        req->flags     = SET_USER_OBJECT_SET_FLAGS;
        req->obj_flags = obj_flags->dwFlags;
        ret = !wine_server_call_err( req );
    }
    SERVER_END_REQ;
    return ret;
}

static HANDLE get_winstations_dir_handle(void)
{
    char bufferA[64];
    WCHAR buffer[64];
    UNICODE_STRING str;
    OBJECT_ATTRIBUTES attr;
    NTSTATUS status;
    HANDLE dir;

    sprintf( bufferA, "\\Sessions\\%u\\Windows\\WindowStations", NtCurrentTeb()->Peb->SessionId );
    str.Buffer = buffer;
    str.Length = str.MaximumLength = asciiz_to_unicode( buffer, bufferA ) - sizeof(WCHAR);
    InitializeObjectAttributes( &attr, &str, 0, 0, NULL );
    status = NtOpenDirectoryObject( &dir, DIRECTORY_CREATE_OBJECT | DIRECTORY_TRAVERSE, &attr );
    return status ? 0 : dir;
}

/***********************************************************************
 *           get_default_desktop
 *
 * Get the name of the desktop to use for this app if not specified explicitly.
 */
static const WCHAR *get_default_desktop( void *buf, size_t buf_size )
{
    const WCHAR *p, *appname = NtCurrentTeb()->Peb->ProcessParameters->ImagePathName.Buffer;
    KEY_VALUE_PARTIAL_INFORMATION *info = buf;
    WCHAR *buffer = buf;
    HKEY tmpkey, appkey;
    DWORD len;

    static const WCHAR defaultW[] = {'D','e','f','a','u','l','t',0};

    if ((p = wcsrchr( appname, '/' ))) appname = p + 1;
    if ((p = wcsrchr( appname, '\\' ))) appname = p + 1;
    len = lstrlenW(appname);
    if (len > MAX_PATH) return defaultW;
    memcpy( buffer, appname, len * sizeof(WCHAR) );
    asciiz_to_unicode( buffer + len, "\\Explorer" );

    /* @@ Wine registry key: HKCU\Software\Wine\AppDefaults\app.exe\Explorer */
    if ((tmpkey = reg_open_hkcu_key( "Software\\Wine\\AppDefaults" )))
    {
        appkey = reg_open_key( tmpkey, buffer, lstrlenW(buffer) * sizeof(WCHAR) );
        NtClose( tmpkey );
        if (appkey)
        {
            len = query_reg_ascii_value( appkey, "Desktop", info, buf_size );
            NtClose( appkey );
            if (len) return (const WCHAR *)info->Data;
        }
    }

    /* @@ Wine registry key: HKCU\Software\Wine\Explorer */
    if ((appkey = reg_open_hkcu_key( "Software\\Wine\\Explorer" )))
    {
        len = query_reg_ascii_value( appkey, "Desktop", info, buf_size );
        NtClose( appkey );
        if (len) return (const WCHAR *)info->Data;
    }

    return defaultW;
}

/***********************************************************************
 *           winstation_init
 *
 * Connect to the process window station and desktop.
 */
void winstation_init(void)
{
    RTL_USER_PROCESS_PARAMETERS *params = NtCurrentTeb()->Peb->ProcessParameters;
    WCHAR *winstation = NULL, *desktop = NULL, *buffer = NULL;
    HANDLE handle, dir = NULL;
    OBJECT_ATTRIBUTES attr;
    UNICODE_STRING str;

    static const WCHAR winsta0[] = {'W','i','n','S','t','a','0',0};

    if (params->Desktop.Length)
    {
        buffer = malloc( params->Desktop.Length + sizeof(WCHAR) );
        memcpy( buffer, params->Desktop.Buffer, params->Desktop.Length );
        buffer[params->Desktop.Length / sizeof(WCHAR)] = 0;
        if ((desktop = wcschr( buffer, '\\' )))
        {
            *desktop++ = 0;
            winstation = buffer;
        }
        else desktop = buffer;
    }

    /* set winstation if explicitly specified, or if we don't have one yet */
    if (buffer || !NtUserGetProcessWindowStation())
    {
        str.Buffer = (WCHAR *)(winstation ? winstation : winsta0);
        str.Length = str.MaximumLength = lstrlenW( str.Buffer ) * sizeof(WCHAR);
        dir = get_winstations_dir_handle();
        InitializeObjectAttributes( &attr, &str, OBJ_CASE_INSENSITIVE | OBJ_OPENIF,
                                    dir, NULL );

        handle = NtUserCreateWindowStation( &attr, WINSTA_ALL_ACCESS, 0, 0, 0, 0, 0 );
        if (handle)
        {
            NtUserSetProcessWindowStation( handle );
            /* only WinSta0 is visible */
            if (!winstation || !wcsicmp( winstation, winsta0 ))
            {
                USEROBJECTFLAGS flags;
                flags.fInherit  = FALSE;
                flags.fReserved = FALSE;
                flags.dwFlags   = WSF_VISIBLE;
                NtUserSetObjectInformation( handle, UOI_FLAGS, &flags, sizeof(flags) );
            }
        }
    }
    if (buffer || !NtUserGetThreadDesktop( GetCurrentThreadId() ))
    {
        char buffer[4096];
        str.Buffer = (WCHAR *)(desktop ? desktop : get_default_desktop( buffer, sizeof(buffer) ));
        str.Length = str.MaximumLength = lstrlenW( str.Buffer ) * sizeof(WCHAR);
        if (!dir) dir = get_winstations_dir_handle();
        InitializeObjectAttributes( &attr, &str, OBJ_CASE_INSENSITIVE | OBJ_OPENIF,
                                    dir, NULL );

        handle = NtUserCreateDesktopEx( &attr, NULL, NULL, 0, DESKTOP_ALL_ACCESS, 0 );
        if (handle) NtUserSetThreadDesktop( handle );
    }
    NtClose( dir );
    free( buffer );
}
