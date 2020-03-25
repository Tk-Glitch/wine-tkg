/*
 * Default entry point for an exe
 *
 * Copyright 2005 Alexandre Julliard
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

#include <stdarg.h>
#include "windef.h"
#include "winbase.h"
#include "winnls.h"
#include "winternl.h"
#include "wine/library.h"
#include "crt0_private.h"

extern int __cdecl main( int argc, char *argv[] );

static char **build_argv( WCHAR **wargv )
{
    int argc;
    char *p, **argv;
    DWORD total = 0;

    for (argc = 0; wargv[argc]; argc++)
        total += WideCharToMultiByte( CP_ACP, 0, wargv[argc], -1, NULL, 0, NULL, NULL );

    argv = HeapAlloc( GetProcessHeap(), 0, total + (argc + 1) * sizeof(*argv) );
    p = (char *)(argv + argc + 1);
    for (argc = 0; wargv[argc]; argc++)
    {
        DWORD reslen = WideCharToMultiByte( CP_ACP, 0, wargv[argc], -1, p, total, NULL, NULL );
        argv[argc] = p;
        p += reslen;
        total -= reslen;
    }
    argv[argc] = NULL;
    return argv;
}

DWORD WINAPI DECLSPEC_HIDDEN __wine_spec_exe_entry( PEB *peb )
{
    BOOL needs_init = (__wine_spec_init_state != CONSTRUCTORS_DONE);
    char **argv = build_argv( __wine_main_wargv );
    DWORD ret;

    if (needs_init) _init( __wine_main_argc, argv, NULL );
    ret = main( __wine_main_argc, argv );
    if (needs_init) _fini();
    ExitProcess( ret );
}
