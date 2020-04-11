/*
 * winebrowser - winelib app to launch native OS browser or mail client.
 *
 * Copyright (C) 2004 Chris Morgan
 * Copyright (C) 2005 Hans Leidekker
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
 *
 * NOTES:
 *  Winebrowser is a winelib application that will start the appropriate
 *  native browser or mail client for a wine installation that lacks a 
 *  windows browser/mail client. For example, you will be able to open
 *  URLs via native mozilla if no browser has yet been installed in wine.
 *
 *  The application to launch is chosen from a default set or, if set,
 *  taken from a registry key.
 *  
 *  The argument may be a regular Windows file name, a file URL, an
 *  URL or a mailto URL. In the first three cases the argument
 *  will be fed to a web browser. In the last case the argument is fed
 *  to a mail client. A mailto URL is composed as follows:
 *
 *   mailto:[E-MAIL]?subject=[TOPIC]&cc=[E-MAIL]&bcc=[E-MAIL]&body=[TEXT]
 */

#define WIN32_LEAN_AND_MEAN
#define COBJMACROS

#include "config.h"
#include "wine/port.h"
#include "wine/debug.h"
#include "wine/unicode.h"

#include <windows.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <urlmon.h>
#include <ddeml.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

WINE_DEFAULT_DEBUG_CHANNEL(winebrowser);

typedef LPSTR (*CDECL wine_get_unix_file_name_t)(LPCWSTR unixname);

static const WCHAR browser_key[] =
    {'S','o','f','t','w','a','r','e','\\','W','i','n','e','\\',
     'W','i','n','e','B','r','o','w','s','e','r',0};

static char *strdup_unixcp( const WCHAR *str )
{
    char *ret;
    int len = WideCharToMultiByte( CP_UNIXCP, 0, str, -1, NULL, 0, NULL, NULL );
    if ((ret = HeapAlloc( GetProcessHeap(), 0, len )))
        WideCharToMultiByte( CP_UNIXCP, 0, str, -1, ret, len, NULL, NULL );
    return ret;
}

static void restore_system_environment(void)
{
    const char* orig_ld_path = getenv("ORIG_LD_LIBRARY_PATH");

    if (orig_ld_path)
    {
        setenv("LD_LIBRARY_PATH", orig_ld_path, 1);
        unsetenv("ORIG_LD_LIBRARY_PATH");
    }
}

/* try to launch a unix app from a comma separated string of app names */
static int launch_app( const WCHAR *candidates, const WCHAR *argv1 )
{
    char *cmdline;
    int i, count;
    char **argv_new;

    if (!(cmdline = strdup_unixcp( argv1 ))) return 1;

    /* PROTON HACK: Restore ORIG_LD_LIBRARY_PATH to LD_LIBRARY_PATH.
     * System programs may not work correctly with our libraries, in
     * particular gio on Ubuntu 19.04 is broken by our libgio. */
    restore_system_environment();

    while (*candidates)
    {
        WCHAR **args = CommandLineToArgvW( candidates, &count );

        if (!(argv_new = HeapAlloc( GetProcessHeap(), 0, (count + 2) * sizeof(*argv_new) ))) break;
        for (i = 0; i < count; i++) argv_new[i] = strdup_unixcp( args[i] );
        argv_new[count] = cmdline;
        argv_new[count + 1] = NULL;

        TRACE( "Trying" );
        for (i = 0; i <= count; i++) TRACE( " %s", wine_dbgstr_a( argv_new[i] ));
        TRACE( "\n" );

        _spawnvp( _P_OVERLAY, argv_new[0], (const char **)argv_new );  /* only returns on error */
        for (i = 0; i < count; i++) HeapFree( GetProcessHeap(), 0, argv_new[i] );
        HeapFree( GetProcessHeap(), 0, argv_new );
        candidates += strlenW( candidates ) + 1;  /* grab the next app */
    }
    WINE_ERR( "could not find a suitable app to open %s\n", debugstr_w( argv1 ));

    HeapFree( GetProcessHeap(), 0, cmdline );
    return 1;
}

static LSTATUS get_commands( HKEY key, const WCHAR *value, WCHAR *buffer, DWORD size )
{
    DWORD type;
    LSTATUS res;

    size -= sizeof(WCHAR);
    if (!(res = RegQueryValueExW( key, value, 0, &type, (LPBYTE)buffer, &size )) && (type == REG_SZ))
    {
        /* convert to REG_MULTI_SZ type */
        WCHAR *p = buffer;
        p[strlenW(p) + 1] = 0;
        while ((p = strchrW( p, ',' ))) *p++ = 0;
    }
    return res;
}

static int open_http_url( const WCHAR *url )
{
#ifdef __APPLE__
    static const WCHAR defaultbrowsers[] =
        { '/','u','s','r','/','b','i','n','/','o','p','e','n',0,0 };
#else
    static const WCHAR defaultbrowsers[] =
        {'x','d','g','-','o','p','e','n',0,
         'f','i','r','e','f','o','x',0,
         'k','o','n','q','u','e','r','o','r',0,
         'm','o','z','i','l','l','a',0,
         'n','e','t','s','c','a','p','e',0,
         'g','a','l','e','o','n',0,
         'o','p','e','r','a',0,
         'd','i','l','l','o',0,0};
#endif
    static const WCHAR browsersW[] =
        {'B','r','o','w','s','e','r','s',0};

    WCHAR browsers[256];
    HKEY key;
    LONG r;

    /* @@ Wine registry key: HKCU\Software\Wine\WineBrowser */
    if  (!(r = RegOpenKeyW( HKEY_CURRENT_USER, browser_key, &key )))
    {
        r = get_commands( key, browsersW, browsers, sizeof(browsers) );
        RegCloseKey( key );
    }
    if (r != ERROR_SUCCESS)
        memcpy( browsers, defaultbrowsers, sizeof(defaultbrowsers) );

    return launch_app( browsers, url );
}

static int open_mailto_url( const WCHAR *url )
{
#ifdef __APPLE__
    static const WCHAR defaultmailers[] =
        { '/','u','s','r','/','b','i','n','/','o','p','e','n',0,0 };
#else
    static const WCHAR defaultmailers[] =
        {'x','d','g','-','e','m','a','i','l',0,
         'm','o','z','i','l','l','a','-','t','h','u','n','d','e','r','b','i','r','d',0,
         't','h','u','n','d','e','r','b','i','r','d',0,
         'e','v','o','l','u','t','i','o','n',0,0 };
#endif
    static const WCHAR mailersW[] =
        {'M','a','i','l','e','r','s',0};

    WCHAR mailers[256];
    HKEY key;
    LONG r;

    /* @@ Wine registry key: HKCU\Software\Wine\WineBrowser */
    if (!(r = RegOpenKeyW( HKEY_CURRENT_USER, browser_key, &key )))
    {
        r = get_commands( key, mailersW, mailers, sizeof(mailers) );
        RegCloseKey( key );
    }
    if (r != ERROR_SUCCESS)
        memcpy( mailers, defaultmailers, sizeof(defaultmailers) );

    return launch_app( mailers, url );
}

/*****************************************************************************
 * DDE helper functions.
 */

static WCHAR *ddeString = NULL;
static HSZ hszTopic = 0, hszReturn = 0;
static DWORD ddeInst = 0;

/* Dde callback, save the execute or request string for processing */
static HDDEDATA CALLBACK ddeCb(UINT uType, UINT uFmt, HCONV hConv,
                                HSZ hsz1, HSZ hsz2, HDDEDATA hData,
                                ULONG_PTR dwData1, ULONG_PTR dwData2)
{
    DWORD size = 0, ret = 0;

    WINE_TRACE("dde_cb: %04x, %04x, %p, %p, %p, %p, %08lx, %08lx\n",
               uType, uFmt, hConv, hsz1, hsz2, hData, dwData1, dwData2);

    switch (uType)
    {
        case XTYP_CONNECT:
            if (!DdeCmpStringHandles(hsz1, hszTopic))
                return (HDDEDATA)TRUE;
            return (HDDEDATA)FALSE;

        case XTYP_EXECUTE:
            if (!(size = DdeGetData(hData, NULL, 0, 0)))
                WINE_ERR("DdeGetData returned zero size of execute string\n");
            else if (!(ddeString = HeapAlloc(GetProcessHeap(), 0, size)))
                WINE_ERR("Out of memory\n");
            else if (DdeGetData(hData, (LPBYTE)ddeString, size, 0) != size)
                WINE_WARN("DdeGetData did not return %d bytes\n", size);
            DdeFreeDataHandle(hData);
            return (HDDEDATA)DDE_FACK;

        case XTYP_REQUEST:
            ret = -3; /* error */
            if (!(size = DdeQueryStringW(ddeInst, hsz2, NULL, 0, CP_WINUNICODE)))
                WINE_ERR("DdeQueryString returned zero size of request string\n");
            else if (!(ddeString = HeapAlloc(GetProcessHeap(), 0, (size + 1) * sizeof(WCHAR))))
                WINE_ERR("Out of memory\n");
            else if (DdeQueryStringW(ddeInst, hsz2, ddeString, size + 1, CP_WINUNICODE) != size)
                WINE_WARN("DdeQueryString did not return %d characters\n", size);
            else
                ret = -2; /* acknowledgment */
            return DdeCreateDataHandle(ddeInst, (LPBYTE)&ret, sizeof(ret), 0,
                                       hszReturn, CF_TEXT, 0);

        default:
            return NULL;
    }
}

static WCHAR *get_url_from_dde(void)
{
    static const WCHAR szApplication[] = {'I','E','x','p','l','o','r','e',0};
    static const WCHAR szTopic[] = {'W','W','W','_','O','p','e','n','U','R','L',0};
    static const WCHAR szReturn[] = {'R','e','t','u','r','n',0};

    HSZ hszApplication = 0;
    UINT_PTR timer = 0;
    int rc;
    WCHAR *ret = NULL;

    rc = DdeInitializeW(&ddeInst, ddeCb, CBF_SKIP_ALLNOTIFICATIONS | CBF_FAIL_ADVISES | CBF_FAIL_POKES, 0);
    if (rc != DMLERR_NO_ERROR)
    {
        WINE_ERR("Unable to initialize DDE, DdeInitialize returned %d\n", rc);
        goto done;
    }

    hszApplication = DdeCreateStringHandleW(ddeInst, szApplication, CP_WINUNICODE);
    if (!hszApplication)
    {
        WINE_ERR("Unable to initialize DDE, DdeCreateStringHandle failed\n");
        goto done;
    }

    hszTopic = DdeCreateStringHandleW(ddeInst, szTopic, CP_WINUNICODE);
    if (!hszTopic)
    {
        WINE_ERR("Unable to initialize DDE, DdeCreateStringHandle failed\n");
        goto done;
    }

    hszReturn = DdeCreateStringHandleW(ddeInst, szReturn, CP_WINUNICODE);
    if (!hszReturn)
    {
        WINE_ERR("Unable to initialize DDE, DdeCreateStringHandle failed\n");
        goto done;
    }

    if (!DdeNameService(ddeInst, hszApplication, 0, DNS_REGISTER))
    {
        WINE_ERR("Unable to initialize DDE, DdeNameService failed\n");
        goto done;
    }

    timer = SetTimer(NULL, 0, 5000, NULL);
    if (!timer)
    {
        WINE_ERR("SetTimer failed to create timer\n");
        goto done;
    }

    while (!ddeString)
    {
        MSG msg;
        if (!GetMessageW(&msg, NULL, 0, 0)) break;
        if (msg.message == WM_TIMER) break;
        DispatchMessageW(&msg);
    }

    if (ddeString)
    {
        if (*ddeString == '"')
        {
            WCHAR *endquote = strchrW(ddeString + 1, '"');
            if (!endquote)
            {
                WINE_ERR("Unable to retrieve URL from string %s\n", wine_dbgstr_w(ddeString));
                goto done;
            }
            *endquote = 0;
            ret = ddeString+1;
        }
        else
            ret = ddeString;
    }

done:
    if (timer) KillTimer(NULL, timer);
    if (ddeInst)
    {
        if (hszTopic && hszApplication) DdeNameService(ddeInst, hszApplication, 0, DNS_UNREGISTER);
        if (hszReturn) DdeFreeStringHandle(ddeInst, hszReturn);
        if (hszTopic) DdeFreeStringHandle(ddeInst, hszTopic);
        if (hszApplication) DdeFreeStringHandle(ddeInst, hszApplication);
        DdeUninitialize(ddeInst);
    }
    return ret;
}

static WCHAR *encode_unix_path(const char *src)
{
    const char *tmp_src;
    WCHAR *dst, *tmp_dst;
    const char safe_chars[] = "/-_.~@&=+$,:";
    const char hex_digits[] = "0123456789ABCDEF";
    const WCHAR schema[] = {'f','i','l','e',':','/','/',0};
    int len = ARRAY_SIZE(schema);

    tmp_src = src;

    while (*tmp_src != 0)
    {
        if ((*tmp_src >= 'a' && *tmp_src <= 'z') ||
            (*tmp_src >= 'A' && *tmp_src <= 'Z') ||
            (*tmp_src >= '0' && *tmp_src <= '9') ||
            strchr(safe_chars, *tmp_src))
            len += 1;
        else
            len += 3;
        tmp_src++;
    }

    dst = HeapAlloc(GetProcessHeap(), 0, len*sizeof(WCHAR));

    if (!dst)
        return NULL;

    strcpyW(dst, schema);

    tmp_src = src;
    tmp_dst = dst + strlenW(dst);

    while (*tmp_src != 0)
    {
        if ((*tmp_src >= 'a' && *tmp_src <= 'z') ||
            (*tmp_src >= 'A' && *tmp_src <= 'Z') ||
            (*tmp_src >= '0' && *tmp_src <= '9') ||
            strchr(safe_chars, *tmp_src))
        {
            *tmp_dst++ = *tmp_src;
        }
        else
        {
            *tmp_dst++ = '%';
            *tmp_dst++ = hex_digits[*(unsigned char*)(tmp_src) / 16];
            *tmp_dst++ = hex_digits[*tmp_src & 0xf];
        }
        tmp_src++;
    }

    *tmp_dst = 0;

    return dst;
}

static WCHAR *convert_file_uri(IUri *uri)
{
    wine_get_unix_file_name_t wine_get_unix_file_name_ptr;
    struct stat dummy;
    WCHAR *new_path;
    char *unixpath;
    BSTR filename;
    HRESULT hres;

    /* check if the argument is a local file */
    wine_get_unix_file_name_ptr = (wine_get_unix_file_name_t)
           GetProcAddress( GetModuleHandleA( "KERNEL32" ), "wine_get_unix_file_name" );
    if(!wine_get_unix_file_name_ptr)
        return NULL;

    hres = IUri_GetPath(uri, &filename);
    if(FAILED(hres))
        return NULL;

    WINE_TRACE("Windows path: %s\n", wine_dbgstr_w(filename));

    unixpath = wine_get_unix_file_name_ptr(filename);
    SysFreeString(filename);
    if(unixpath && stat(unixpath, &dummy) >= 0) {
        WINE_TRACE("Unix path: %s\n", wine_dbgstr_a(unixpath));
        new_path = encode_unix_path(unixpath);
        HeapFree(GetProcessHeap(), 0, unixpath);
    }else {
        WINE_WARN("File %s does not exist\n", wine_dbgstr_a(unixpath));
        HeapFree(GetProcessHeap(), 0, unixpath);
        new_path = NULL;
    }

    WINE_TRACE("New path: %s\n", wine_dbgstr_w(new_path));

    return new_path;
}

/*****************************************************************************
 * Main entry point. This is a console application so we have a wmain() not a
 * winmain().
 */
int __cdecl wmain(int argc, WCHAR *argv[])
{
    static const WCHAR nohomeW[] = {'-','n','o','h','o','m','e',0};

    WCHAR *url = argv[1];
    BSTR display_uri = NULL;
    DWORD scheme;
    IUri *uri;
    HRESULT hres;
    int ret = 1;

    /* DDE used only if -nohome is specified; avoids delay in printing usage info
     * when no parameters are passed */
    if (url && !strcmpiW( url, nohomeW ))
        url = argc > 2 ? argv[2] : get_url_from_dde();

    if (!url) {
        WINE_ERR( "Usage: winebrowser URL\n" );
        return -1;
    }

    hres = CreateUri(url, Uri_CREATE_ALLOW_IMPLICIT_FILE_SCHEME|Uri_CREATE_FILE_USE_DOS_PATH, 0, &uri);
    if(FAILED(hres)) {
        WINE_ERR("Failed to parse URL\n");
        ret = open_http_url(url);
        HeapFree(GetProcessHeap(), 0, ddeString);
        return ret;
    }

    HeapFree(GetProcessHeap(), 0, ddeString);
    IUri_GetScheme(uri, &scheme);

    if(scheme == URL_SCHEME_FILE) {
        display_uri = convert_file_uri(uri);
        if(!display_uri) {
            WINE_ERR("Failed to convert file URL to unix path\n");
        }
    }

    if (!display_uri)
        hres = IUri_GetDisplayUri(uri, &display_uri);
    IUri_Release(uri);
    if(FAILED(hres))
        return -1;

    WINE_TRACE("opening %s\n", wine_dbgstr_w(display_uri));

    if(scheme == URL_SCHEME_MAILTO)
        ret = open_mailto_url(display_uri);
    else
        /* let the browser decide how to handle the given url */
        ret = open_http_url(display_uri);

    SysFreeString(display_uri);
    return ret;
}
