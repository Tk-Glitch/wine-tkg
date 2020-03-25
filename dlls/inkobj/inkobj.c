/* Copyright (C) 2007 C John Klehm
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

#include "inkobj_internal.h"

WINE_DEFAULT_DEBUG_CHANNEL(inkobj);

static LONG INKOBJ_refCount;

/*****************************************************
 *    DllMain (INKOBJ.init)
 */
BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID reserved)
{
    TRACE("(%p, %d, %p)\n", hinst, reason, reserved);

    switch(reason)
    {
    case DLL_WINE_PREATTACH:
        return FALSE; /* prefer native version */
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls( hinst );
        break;
    }
    return TRUE;
}

/*****************************************************
 *    DllCanUnloadNow (INKOBJ.@)
 */
HRESULT WINAPI DllCanUnloadNow(void)
{
    return (INKOBJ_refCount != 0)? S_FALSE : S_OK;
}

/*****************************************************
 *    DllGetClassObject [INKOBJ.@]
 */
HRESULT WINAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv)
{
    FIXME("Not implemented. Requested class was:%s\n", debugstr_guid(rclsid));
    return CLASS_E_CLASSNOTAVAILABLE;
}

/*****************************************************
 *    DllRegisterServer (INKOBJ.@)
 */
HRESULT WINAPI DllRegisterServer(void)
{
    FIXME("Not implemented.\n");
    return E_UNEXPECTED; /* unable to register */
}

/*****************************************************
 *    DllUnregisterServer (INKOBJ.@)
 */
HRESULT WINAPI DllUnregisterServer(void)
{
    FIXME("Not implemented.\n");
    return E_UNEXPECTED; /* unable to register */
}
