/*
 * MAPISendMail implementation
 *
 * Copyright 2005 Hans Leidekker
 * Copyright 2009 Owen Rudge for CodeWeavers
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

#define COBJMACROS

#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "mapi.h"
#include "winreg.h"
#include "shellapi.h"
#include "shlwapi.h"
#include "winternl.h"
#include "wine/debug.h"

#include "winemapi_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(winemapi);

/* Escapes a string for use in mailto: URL */
static char *escape_string(char *in, char *empty_string)
{
    HRESULT res;
    DWORD size;
    char *escaped = NULL;

    if (!in)
        return empty_string;

    size = 1;
    res = UrlEscapeA(in, empty_string, &size, URL_ESCAPE_PERCENT | URL_ESCAPE_SEGMENT_ONLY);

    if (res == E_POINTER)
    {
        escaped = HeapAlloc(GetProcessHeap(), 0, size);

        if (!escaped)
            return in;

        /* If for some reason UrlEscape fails, just send the original text */
        if (UrlEscapeA(in, escaped, &size, URL_ESCAPE_PERCENT | URL_ESCAPE_SEGMENT_ONLY) != S_OK)
        {
            HeapFree(GetProcessHeap(), 0, escaped);
            escaped = in;
        }
    }

    return escaped ? escaped : empty_string;
}

/**************************************************************************
 *  BrowserSendMail
 *
 * Send an email by forming a mailto uri and invoking a browser.
 *
 */
static ULONG BrowserSendMail(LHANDLE session, ULONG_PTR uiparam,
    lpMapiMessage message, FLAGS flags, ULONG reserved)
{
    ULONG ret = MAPI_E_FAILURE;
    unsigned int i, to_count = 0, cc_count = 0, bcc_count = 0;
    unsigned int to_size = 0, cc_size = 0, bcc_size = 0, subj_size, body_size;

    char *to = NULL, *cc = NULL, *bcc = NULL, *subject = NULL, *body = NULL;
    const char *address;
    static const char format[] =
        "mailto:\"%s\"?subject=\"%s\"&cc=\"%s\"&bcc=\"%s\"&body=\"%s\"";
    static const char smtp[] = "smtp:";
    char *mailto = NULL, *escape = NULL;
    char empty_string[] = "";
    HRESULT res;
    DWORD size;

    TRACE("(0x%08lx 0x%08lx %p 0x%08x 0x%08x)\n", session, uiparam,
           message, flags, reserved);

    if (!message)
        return MAPI_E_FAILURE;

    for (i = 0; i < message->nRecipCount; i++)
    {
        if (!message->lpRecips)
        {
            WARN("No recipients found\n");
            return MAPI_E_FAILURE;
        }

        address = message->lpRecips[i].lpszAddress;

        if (address)
        {
            if (!_strnicmp(address, smtp, sizeof(smtp) - 1))
                address += sizeof(smtp) - 1;

            switch (message->lpRecips[i].ulRecipClass)
            {
                case MAPI_ORIG:
                    TRACE("From: %s\n", debugstr_a(address));
                    break;

                case MAPI_TO:
                    TRACE("To: %s\n", debugstr_a(address));
                    to_size += lstrlenA(address) + 1;
                    break;

                case MAPI_CC:
                    TRACE("Cc: %s\n", debugstr_a(address));
                    cc_size += lstrlenA(address) + 1;
                    break;

                case MAPI_BCC:
                    TRACE("Bcc: %s\n", debugstr_a(address));
                    bcc_size += lstrlenA(address) + 1;
                    break;

                default:
                    TRACE("Unknown recipient class: %d\n",
                           message->lpRecips[i].ulRecipClass);
            }
        }
        else
            FIXME("Name resolution and entry identifiers not supported\n");
    }

    if (message->nFileCount)
    {
        FIXME("Ignoring %u attachments:\n", message->nFileCount);
        for (i = 0; i < message->nFileCount; i++)
            FIXME("\t%s (%s)\n", debugstr_a(message->lpFiles[i].lpszPathName),
                  debugstr_a(message->lpFiles[i].lpszFileName));
    }

    /* Escape subject and body */
    subject = escape_string(message->lpszSubject, empty_string);
    body = escape_string(message->lpszNoteText, empty_string);

    TRACE("Subject: %s\n", debugstr_a(subject));
    TRACE("Body: %s\n", debugstr_a(body));

    subj_size = lstrlenA(subject);
    body_size = lstrlenA(body);

    ret = MAPI_E_INSUFFICIENT_MEMORY;

    if (to_size)
    {
        to = HeapAlloc(GetProcessHeap(), 0, to_size);

        if (!to)
            goto exit;

        to[0] = 0;
    }

    if (cc_size)
    {
        cc = HeapAlloc(GetProcessHeap(), 0, cc_size);

        if (!cc)
            goto exit;

        cc[0] = 0;
    }

    if (bcc_size)
    {
        bcc = HeapAlloc(GetProcessHeap(), 0, bcc_size);

        if (!bcc)
            goto exit;

        bcc[0] = 0;
    }

    if (message->lpOriginator)
        TRACE("From: %s\n", debugstr_a(message->lpOriginator->lpszAddress));

    for (i = 0; i < message->nRecipCount; i++)
    {
        address = message->lpRecips[i].lpszAddress;

        if (address)
        {
            if (!_strnicmp(address, smtp, sizeof(smtp) - 1))
                address += sizeof(smtp) - 1;

            switch (message->lpRecips[i].ulRecipClass)
            {
                case MAPI_TO:
                    if (to_count)
                        lstrcatA(to, ",");

                    lstrcatA(to, address);
                    to_count++;
                    break;

                case MAPI_CC:
                    if (cc_count)
                        lstrcatA(cc, ",");

                    lstrcatA(cc, address);
                    cc_count++;
                    break;

                case MAPI_BCC:
                    if (bcc_count)
                        lstrcatA(bcc, ",");

                    lstrcatA(bcc, address);
                    bcc_count++;
                    break;
            }
        }
    }
    ret = MAPI_E_FAILURE;
    size = sizeof(format) + to_size + cc_size + bcc_size + subj_size + body_size;

    mailto = HeapAlloc(GetProcessHeap(), 0, size);

    if (!mailto)
        goto exit;

    sprintf(mailto, format, to ? to : "", subject, cc ? cc : "", bcc ? bcc : "", body);

    size = 1;
    res = UrlEscapeA(mailto, empty_string, &size, URL_ESCAPE_SPACES_ONLY);

    if (res != E_POINTER)
        goto exit;

    escape = HeapAlloc(GetProcessHeap(), 0, size);

    if (!escape)
        goto exit;

    res = UrlEscapeA(mailto, escape, &size, URL_ESCAPE_SPACES_ONLY);

    if (res != S_OK)
        goto exit;

    TRACE("Executing winebrowser.exe with parameters '%s'\n", debugstr_a(escape));

    if ((UINT_PTR) ShellExecuteA(NULL, "open", "winebrowser.exe", escape, NULL, 0) > 32)
        ret = SUCCESS_SUCCESS;

exit:
    HeapFree(GetProcessHeap(), 0, to);
    HeapFree(GetProcessHeap(), 0, cc);
    HeapFree(GetProcessHeap(), 0, bcc);
    HeapFree(GetProcessHeap(), 0, mailto);
    HeapFree(GetProcessHeap(), 0, escape);

    if (subject != empty_string)
        HeapFree(GetProcessHeap(), 0, subject);

    if (body != empty_string)
        HeapFree(GetProcessHeap(), 0, body);

    return ret;
}

/**************************************************************************
 *  MAPISendMail
 *
 * Send a message using a native mail client.
 *
 * PARAMS
 *  session  [I] Handle to a MAPI session.
 *  uiparam  [I] Parent window handle.
 *  message  [I] Pointer to a MAPIMessage structure.
 *  flags    [I] Flags.
 *  reserved [I] Reserved, pass 0.
 *
 * RETURNS
 *  Success: SUCCESS_SUCCESS
 *  Failure: MAPI_E_FAILURE
 *
 */
ULONG WINAPI MAPISendMail(LHANDLE session, ULONG_PTR uiparam,
    lpMapiMessage message, FLAGS flags, ULONG reserved)
{
    TRACE("(0x%08lx 0x%08lx %p 0x%08x 0x%08x)\n", session, uiparam,
           message, flags, reserved);

    if (XDGMailAvailable())
        return XDGSendMail(session, uiparam, message, flags, reserved);

    return BrowserSendMail(session, uiparam, message, flags, reserved);
}

ULONG WINAPI MAPISendDocuments(ULONG_PTR uiparam, LPSTR delim, LPSTR paths,
    LPSTR filenames, ULONG reserved)
{
    return MAPI_E_NOT_SUPPORTED;
}
