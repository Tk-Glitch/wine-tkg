/*
 * Font Cache service
 *
 * Copyright 2014 Nikolay Sivov for CodeWeavers
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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "winsvc.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(fontcache);

static HINSTANCE hInst;
static SERVICE_STATUS_HANDLE service_handle;
static HANDLE stop_event;

/***********************************************************************
 *		DllMain (fntcache.@)
 */
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    TRACE("(%p, %d, %p)\n", hinstDLL, fdwReason, lpvReserved);

    switch (fdwReason)
    {
        case DLL_WINE_PREATTACH:
            return FALSE;  /* prefer native version */
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hinstDLL);
            hInst = hinstDLL;
            break;
    }

    return TRUE;
}

static DWORD WINAPI service_handler( DWORD ctrl, DWORD event_type, void *event_data, void *context )
{
    SERVICE_STATUS status;

    status.dwServiceType             = SERVICE_WIN32;
    status.dwControlsAccepted        = SERVICE_ACCEPT_STOP;
    status.dwWin32ExitCode           = 0;
    status.dwServiceSpecificExitCode = 0;
    status.dwCheckPoint              = 0;
    status.dwWaitHint                = 0;

    switch(ctrl)
    {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
        TRACE( "shutting down\n" );
        status.dwCurrentState     = SERVICE_STOP_PENDING;
        status.dwControlsAccepted = 0;
        SetServiceStatus( service_handle, &status );
        SetEvent( stop_event );
        return NO_ERROR;
    default:
        FIXME( "got service ctrl %x\n", ctrl );
        status.dwCurrentState = SERVICE_RUNNING;
        SetServiceStatus( service_handle, &status );
        return NO_ERROR;
    }
}

/***********************************************************************
 *		ServiceMain (fntcache.@)
 */
VOID WINAPI ServiceMain(DWORD argc, LPWSTR *argv)
{
    static const WCHAR fontcacheW[] = {'F','o','n','t','C','a','c','h','e',0};
    SERVICE_STATUS status;

    TRACE( "starting service\n" );

    stop_event = CreateEventW( NULL, TRUE, FALSE, NULL );

    service_handle = RegisterServiceCtrlHandlerExW( fontcacheW, service_handler, NULL );
    if (!service_handle)
        return;

    status.dwServiceType             = SERVICE_WIN32;
    status.dwCurrentState            = SERVICE_RUNNING;
    status.dwControlsAccepted        = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    status.dwWin32ExitCode           = 0;
    status.dwServiceSpecificExitCode = 0;
    status.dwCheckPoint              = 0;
    status.dwWaitHint                = 10000;
    SetServiceStatus( service_handle, &status );

    WaitForSingleObject( stop_event, INFINITE );

    status.dwCurrentState     = SERVICE_STOPPED;
    status.dwControlsAccepted = 0;
    SetServiceStatus( service_handle, &status );
    TRACE( "service stopped\n" );
}
