/*
 * an application for displaying Win32 console
 *
 * Copyright 2001, 2002 Eric Pouech
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

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include "windef.h"
#include "winbase.h"
#include "wincon.h"

#include "wineconsole_res.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(console);

int WINAPI wWinMain( HINSTANCE inst, HINSTANCE prev, WCHAR *cmdline, INT show )
{
    STARTUPINFOW startup = { sizeof(startup) };
    PROCESS_INFORMATION info;
    WCHAR *cmd = cmdline;
    DWORD exit_code;

    static WCHAR default_cmd[] = L"cmd";

    FreeConsole(); /* make sure we're not connected to inherited console */
    if (!AllocConsole())
    {
        ERR( "failed to allocate console: %u\n", GetLastError() );
        return 1;
    }

    if (!*cmd) cmd = default_cmd;

    startup.dwFlags    = STARTF_USESTDHANDLES;
    startup.hStdInput  = CreateFileW( L"CONIN$",  GENERIC_READ | GENERIC_WRITE, 0, NULL,
                                      OPEN_EXISTING, 0, 0 );
    startup.hStdOutput = CreateFileW( L"CONOUT$", GENERIC_READ | GENERIC_WRITE, 0, NULL,
                                      OPEN_EXISTING, 0, 0 );
    startup.hStdError  = startup.hStdOutput;

    if (!CreateProcessW( NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &startup, &info ))
    {
        WCHAR format[256], *buf;
        INPUT_RECORD ir;
        DWORD len;
        exit_code = GetLastError();
        WARN( "CreateProcess failed: %u\n", exit_code );
        LoadStringW( GetModuleHandleW( NULL ), IDS_CMD_LAUNCH_FAILED, format, ARRAY_SIZE(format) );
        len = wcslen( format ) + wcslen( cmd );
        if ((buf = malloc( len * sizeof(WCHAR) )))
        {
            swprintf( buf, len, format, cmd );
            WriteConsoleW( startup.hStdOutput, buf, wcslen(buf), &len, NULL);
            while (ReadConsoleInputW( startup.hStdInput, &ir, 1, &len ) && ir.EventType == MOUSE_EVENT);
        }
        return exit_code;
    }

    CloseHandle( info.hThread );
    WaitForSingleObject( info.hProcess, INFINITE );
    return GetExitCodeProcess( info.hProcess, &exit_code ) ? exit_code : GetLastError();
}
