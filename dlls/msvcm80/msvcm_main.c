/*
 * Copyright 2010 Vincent Povirk for CodeWeavers
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

WINE_DEFAULT_DEBUG_CHANNEL(msvcm);

BOOL WINAPI DllMain(HINSTANCE hdll, DWORD reason, LPVOID reserved)
{
    switch (reason)
    {
        case DLL_WINE_PREATTACH:
            return FALSE;  /* prefer native version */

        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hdll);
    }
    return TRUE;
}

/* void __cdecl <CrtImplementationDetails>::DoDllLanguageSupportValidation(void) */
void __cdecl CrtImplementationDetails_DoDllLanguageSupportValidation(void)
{
    FIXME("stub\n");
}

/* void __cdecl <CrtImplementationDetails>::RegisterModuleUninitializer(System.EventHandler^) */
void __cdecl CrtImplementationDetails_RegisterModuleUninitializer(void* handler)
{
    FIXME("%p: stub\n", handler);
}

/* handler is a "method" with signature int32 (*handler)(_exception*), but I'm
 * not sure what that means */
void __cdecl __setusermatherr_m(void *handler)
{
    FIXME("%p: stub\n", handler);
}
