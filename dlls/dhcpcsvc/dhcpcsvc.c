/*
 * Copyright 2011 Stefan Leichter
 * Copyright 2019 Hans Leidekker for CodeWeavers
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
#include "windef.h"
#include "winbase.h"
#include "dhcpcsdk.h"
#include "winioctl.h"
#define WINE_MOUNTMGR_EXTENSIONS
#include "ddk/mountmgr.h"

#include "wine/debug.h"
#include "wine/heap.h"

WINE_DEFAULT_DEBUG_CHANNEL(dhcpcsvc);

BOOL WINAPI DllMain( HINSTANCE hinst, DWORD reason, LPVOID reserved )
{
    TRACE("%p, %u, %p\n", hinst, reason, reserved);

    switch (reason)
    {
        case DLL_WINE_PREATTACH:
            return FALSE;    /* prefer native version */
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls( hinst );
            break;
    }
    return TRUE;
}

void WINAPI DhcpCApiCleanup(void)
{
    FIXME(": stub\n");
}

DWORD WINAPI DhcpCApiInitialize(LPDWORD version)
{
    *version = 2; /* 98, XP, and 8 */
    FIXME(": stub\n");
    return ERROR_SUCCESS;
}

DWORD WINAPI DhcpRequestParams( DWORD flags, void *reserved, WCHAR *adapter, DHCPCAPI_CLASSID *class_id,
                                DHCPCAPI_PARAMS_ARRAY send_params, DHCPCAPI_PARAMS_ARRAY recv_params, BYTE *buf,
                                DWORD *buflen, WCHAR *request_id )
{
    struct mountmgr_dhcp_request_params *query;
    DWORD i, size, err = ERROR_OUTOFMEMORY;
    BYTE *src, *dst;
    HANDLE mgr;

    TRACE( "(%08x, %p, %s, %p, %u, %u, %p, %p, %s)\n", flags, reserved, debugstr_w(adapter), class_id,
           send_params.nParams, recv_params.nParams, buf, buflen, debugstr_w(request_id) );

    if (!adapter || lstrlenW(adapter) > IF_MAX_STRING_SIZE || !buflen) return ERROR_INVALID_PARAMETER;
    if (flags != DHCPCAPI_REQUEST_SYNCHRONOUS) FIXME( "unsupported flags %08x\n", flags );

    for (i = 0; i < send_params.nParams; i++)
        FIXME( "send option %u not supported\n", send_params.Params->OptionId );

    mgr = CreateFileW( MOUNTMGR_DOS_DEVICE_NAME, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL,
                       OPEN_EXISTING, 0, 0 );
    if (mgr == INVALID_HANDLE_VALUE) return GetLastError();

    size = FIELD_OFFSET(struct mountmgr_dhcp_request_params, params[recv_params.nParams]) + *buflen;
    if (!(query = heap_alloc_zero( size ))) goto done;

    for (i = 0; i < recv_params.nParams; i++) query->params[i].id = recv_params.Params[i].OptionId;
    query->count = recv_params.nParams;
    lstrcpyW( query->adapter, adapter );

    if (!DeviceIoControl( mgr, IOCTL_MOUNTMGR_QUERY_DHCP_REQUEST_PARAMS, query, size, query, size, NULL, NULL ))
    {
        err = GetLastError();
        if (err == ERROR_MORE_DATA) *buflen = query->size - (size - *buflen);
        goto done;
    }

    dst = buf;
    for (i = 0; i < query->count; i++)
    {
        if (buf)
        {
            recv_params.Params[i].OptionId = query->params[i].id;
            recv_params.Params[i].IsVendor = FALSE; /* FIXME */
            if (query->params[i].size)
            {
                src = (BYTE *)query + query->params[i].offset;
                memcpy( dst, src, query->params[i].size );

                recv_params.Params[i].Data = dst;
                recv_params.Params[i].nBytesData = query->params[i].size;
            }
            else
            {
                recv_params.Params[i].Data = NULL;
                recv_params.Params[i].nBytesData = 0;
            }
        }
        dst += query->params[i].size;
    }

    *buflen = dst - buf;
    err = ERROR_SUCCESS;

done:
    heap_free( query );
    CloseHandle( mgr );
    return err;
}
