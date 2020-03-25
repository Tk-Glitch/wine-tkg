/*
 *
 * wimgapi implementation
 *
 * Copyright 2015 Stefan Leichter
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
#include "wine/debug.h"
#include "wimgapi.h"

WINE_DEFAULT_DEBUG_CHANNEL(wimgapi);

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void *reserved)
{
    TRACE("(0x%p, %d, %p)\n", instance, reason, reserved);

    switch (reason)
    {
        case DLL_WINE_PREATTACH:
            return FALSE;    /* prefer native version */
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(instance);
            break;
    }

    return TRUE;
}

DWORD WINAPI WIMRegisterMessageCallback(HANDLE wim, FARPROC callback, PVOID data)
{
    FIXME("(%p %p %p) stub\n", wim, callback, data);
    return 0;
}

BOOL WINAPI WIMGetMountedImages(PWIM_MOUNT_LIST list, DWORD *length)
{
    FIXME("(%p %p) stub\n", list, length);
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return 0;
}

HANDLE WINAPI WIMCreateFile(WCHAR *path, DWORD access, DWORD creation, DWORD flags, DWORD compression, DWORD *result)
{
    FIXME("(%s %d %d %d %d %p) stub\n", debugstr_w(path), access, creation, flags, compression, result);
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return NULL;
}
