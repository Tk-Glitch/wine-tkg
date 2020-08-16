/*
 * Copyright 2014 Austin English
 * Copyright 2012 Hans Leidekker for CodeWeavers
 * Copyright 2020 Louis Lenders
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

#define COBJMACROS

#include <stdio.h>
#include "windows.h"
#include "ocidl.h"
#include "initguid.h"
#include "objidl.h"
#include "wbemcli.h"

#include "wine/debug.h"
#include "wine/heap.h"

#define new_line  (i == (ARRAY_SIZE(pq) - 1) || wcslen( pq[i+1].row_name ) )

WINE_DEFAULT_DEBUG_CHANNEL(systeminfo);

typedef struct {
    const WCHAR *row_name;
    const WCHAR *prepend;
    const WCHAR *class;
    const WCHAR *prop;
    const WCHAR *append;
} print_query_prop;

static const print_query_prop pq[] = {
    /*row_name                      prepend prop                                                                   append prop          */
    /*(if any)                      with string (if any)                                                           with string (if any) */
    { L"OS Name",                   L"",                  L"Win32_OperatingSystem",      L"Caption",               L""   },
    { L"OS Version",                L"",                  L"Win32_OperatingSystem",      L"Version",               L""   },
    { L"",                          L"",                  L"Win32_OperatingSystem",      L"CSDVersion",            L""   },
    { L"",                          L"Build ",            L"Win32_OperatingSystem",      L"BuildNumber",           L""   },
    { L"Total Physical Memory",     L"",                  L"Win32_ComputerSystem",       L"TotalPhysicalMemory",   L""   },
    { L"BIOS Version",              L"",                  L"Win32_BIOS",                 L"Manufacturer",          L""   },
    { L"",                          L", ",                L"Win32_BIOS",                 L"ReleaseDate",           L""   },
    { L"Processor(s)",              L"",                  L"Win32_Processor",            L"Caption",               L""   },
    { L"",                          L"",                  L"Win32_Processor",            L"Manufacturer",          L""   },
    { L"",                          L"~",                 L"Win32_Processor",            L"MaxClockSpeed",         L"Mhz"}
};

static int sysinfo_vprintfW(const WCHAR *msg, __ms_va_list va_args)
{
    int wlen;
    DWORD count, ret;
    WCHAR msg_buffer[8192];

    wlen = vswprintf(msg_buffer, ARRAY_SIZE(msg_buffer), msg, va_args);

    ret = WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), msg_buffer, wlen, &count, NULL);
    if (!ret)
    {
        DWORD len;
        char *msgA;

        /* On Windows WriteConsoleW() fails if the output is redirected. So fall
         * back to WriteFile(), assuming the console encoding is still the right
         * one in that case.
         */
        len = WideCharToMultiByte(GetConsoleOutputCP(), 0, msg_buffer, wlen,
            NULL, 0, NULL, NULL);
        msgA = heap_alloc(len);
        if (!msgA)
            return 0;

        WideCharToMultiByte(GetConsoleOutputCP(), 0, msg_buffer, wlen, msgA, len,
            NULL, NULL);
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), msgA, len, &count, FALSE);
        heap_free(msgA);
    }

    return count;
}

static int WINAPIV sysinfo_printfW(const WCHAR *msg, ...)
{
    __ms_va_list va_args;
    int len;

    __ms_va_start(va_args, msg);
    len = sysinfo_vprintfW(msg, va_args);
    __ms_va_end(va_args);

    return len;
}

static WCHAR *find_prop( IWbemClassObject *class, const WCHAR *prop )
{
    SAFEARRAY *sa;
    WCHAR *ret = NULL;
    LONG i, last_index = 0;
    BSTR str;

    if (IWbemClassObject_GetNames( class, NULL, WBEM_FLAG_ALWAYS, NULL, &sa ) != S_OK) return NULL;

    SafeArrayGetUBound( sa, 1, &last_index );
    for (i = 0; i <= last_index; i++)
    {
        SafeArrayGetElement( sa, &i, &str );
        if (!wcsicmp( str, prop ))
        {
            ret = _wcsdup( str );
            break;
        }
    }
    SafeArrayDestroy( sa );
    return ret;
}

static int query_prop(  const WCHAR *class, const WCHAR *propname )
{
    static const WCHAR select_allW[] = {'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',0};
    static const WCHAR cimv2W[] = {'R','O','O','T','\\','C','I','M','V','2',0};
    static const WCHAR wqlW[] = {'W','Q','L',0};
    HRESULT hr;
    IWbemLocator *locator = NULL;
    IWbemServices *services = NULL;
    IEnumWbemClassObject *result = NULL;
    LONG flags = WBEM_FLAG_RETURN_IMMEDIATELY | WBEM_FLAG_FORWARD_ONLY;
    BSTR path = NULL, wql = NULL, query = NULL;
    WCHAR *prop = NULL;
    int len, ret = -1;
    IWbemClassObject *obj;
    ULONG count = 0;
    VARIANT v;

    WINE_TRACE("%s, %s\n", debugstr_w(class), debugstr_w(propname));

    CoInitialize( NULL );
    CoInitializeSecurity( NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT,
                          RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL );

    hr = CoCreateInstance( &CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER, &IID_IWbemLocator,
                           (void **)&locator );
    if (hr != S_OK) goto done;

    if (!(path = SysAllocString( cimv2W ))) goto done;
    hr = IWbemLocator_ConnectServer( locator, path, NULL, NULL, NULL, 0, NULL, NULL, &services );
    if (hr != S_OK) goto done;

    len = lstrlenW( class ) + ARRAY_SIZE(select_allW);
    if (!(query = SysAllocStringLen( NULL, len ))) goto done;
    lstrcpyW( query, select_allW );
    lstrcatW( query, class );

    if (!(wql = SysAllocString( wqlW ))) goto done;
    hr = IWbemServices_ExecQuery( services, wql, query, flags, NULL, &result );
    if (hr != S_OK) goto done;

    for (;;)
    {
        IEnumWbemClassObject_Next( result, WBEM_INFINITE, 1, &obj, &count );
        if (!count) break;

        if (!prop && !(prop = find_prop( obj, propname )))
        {
            ERR("Error: Invalid query\n");
            goto done;
        }

        if (IWbemClassObject_Get( obj, prop, 0, &v, NULL, NULL ) == WBEM_S_NO_ERROR)
        {
            VariantChangeType( &v, &v, 0, VT_BSTR );
            sysinfo_printfW( V_BSTR( &v ) );
            VariantClear( &v );
        }
        IWbemClassObject_Release( obj );
    }
    ret = 0;

done:
    if (result) IEnumWbemClassObject_Release( result );
    if (services) IWbemServices_Release( services );
    if (locator) IWbemLocator_Release( locator );
    SysFreeString( path );
    SysFreeString( query );
    SysFreeString( wql );
    HeapFree( GetProcessHeap(), 0, prop );
    CoUninitialize();
    return ret;
}

int __cdecl wmain(int argc, WCHAR *argv[])
{
    int i;
    BOOL csv = FALSE;

    for (i = 1; i < argc; i++)
    {
        if ( !wcsicmp( argv[i], L"/fo" ) && !wcsicmp( argv[i+1], L"csv" ) )
            csv = TRUE;
        else
            WINE_FIXME( "command line switch %s not supported\n", debugstr_w(argv[i]) );
    }

    if( !csv )
    {
        for ( i = 0; i < ARRAYSIZE(pq); i++ )
        {
            if( wcslen(pq[i].row_name) )
                sysinfo_printfW( L"%-*s", 44, pq[i].row_name );
            if ( wcslen(pq[i].prepend) )
                sysinfo_printfW( L"%s", pq[i].prepend );
            query_prop( pq[i].class, pq[i].prop );
            if ( wcslen(pq[i].append) )
                sysinfo_printfW( L"%s", pq[i].append );
            sysinfo_printfW( new_line ? L"\r\n" : L" " );
        }
    }
    else /* only option "systeminfo /fo csv" supported for now */
    {
        for (i = 0; i < ARRAYSIZE(pq); i++)
        {
            if( wcslen(pq[i].row_name) )
                sysinfo_printfW( i ? L",\"%s\"" : L"\"%s\"", pq[i].row_name );
        }
        sysinfo_printfW( L"\r\n" );

        for (i = 0; i < ARRAYSIZE(pq); i++)
        {
            if ( wcslen(pq[i].row_name) )
                sysinfo_printfW( i ? L",\"" : L"\"" );
            if ( wcslen(pq[i].prepend) )
                sysinfo_printfW( L"%s",pq[i].prepend );
            query_prop( pq[i].class, pq[i].prop );
            if ( wcslen(pq[i].append) )
                sysinfo_printfW( L"%s", pq[i].append );
            sysinfo_printfW( new_line ? L"\"" : L" " );
        }
        sysinfo_printfW( L"\r\n" );
    }
    return 0;
}
