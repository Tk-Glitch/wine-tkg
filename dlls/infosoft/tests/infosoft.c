/*
 * tests for the language neutral word breaker
 *
 * Copyright 2006 Mike McCormack for CodeWeavers
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
#define CONST_VTABLE

#include <stdio.h>
#include <ole2.h>
#include "indexsrv.h"

#include "wine/test.h"

#include <initguid.h>

DEFINE_GUID(CLSID_wb_neutral,  0x369647e0,0x17b0,0x11ce,0x99,0x50,0x00,0xaa,0x00,0x4b,0xbb,0x1f);
DEFINE_GUID(_IID_IWordBreaker, 0xD53552C8,0x77E3,0x101A,0xB5,0x52,0x08,0x00,0x2B,0x33,0xB0,0xE6);

static const WCHAR teststring[] = {
    's','q','u','a','r','e',' ',
    'c','i','r','c','l','e',' ',
    't','r','i','a','n','g','l','e',' ',
    'b','o','x',0
};

struct expected {
    UINT ofs;
    UINT len;
    WCHAR data[10];
};

static int wordnum;

static struct expected testres[] = {
    { 0, 6, {'s','q','u','a','r','e',0 }},
    { 7, 6, {'c','i','r','c','l','e',0 }},
    { 14, 8, {'t','r','i','a','n','g','l','e',0 }},
    { 23, 3, {'b','o','x',0}},
};

static HRESULT WINAPI ws_QueryInterface(IWordSink *iface, REFIID riid, void **ppvObject)
{
    ok(0, "not expected\n");
    return E_NOINTERFACE;
}

static ULONG WINAPI ws_AddRef(IWordSink *iface)
{
    ok(0, "not expected\n");
    return 2;
}

static ULONG WINAPI ws_Release(IWordSink *iface)
{
    ok(0, "not expected\n");
    return 1;
}

static HRESULT WINAPI ws_PutWord(IWordSink *iface, ULONG cwc, const WCHAR *pwcInBuf,
                                 ULONG cwcSrcLen, ULONG cwcSrcPos)
{
    ok(testres[wordnum].len == cwcSrcLen, "wrong length\n");
    ok(!cwcSrcPos ||(testres[wordnum].ofs == cwcSrcPos), "wrong offset\n");
    ok(!memcmp(testres[wordnum].data, pwcInBuf, cwcSrcLen), "wrong data\n");
    wordnum++;
    return S_OK;
}

static HRESULT WINAPI ws_PutAltWord(IWordSink *iface, ULONG cwc, const WCHAR *pwcInBuf,
                                    ULONG cwcSrcLen, ULONG cwcSrcPos)
{
    ok(0, "not expected\n");
    return S_OK;
}

static HRESULT WINAPI ws_StartAltPhrase(IWordSink *iface)
{
    ok(0, "not expected\n");
    return S_OK;
}

static HRESULT WINAPI ws_EndAltPhrase(IWordSink *iface)
{
    ok(0, "not expected\n");
    return S_OK;
}

static HRESULT WINAPI ws_PutBreak(IWordSink *iface, WORDREP_BREAK_TYPE breakType)
{
    ok(0, "not expected\n");
    return S_OK;
}

static const IWordSinkVtbl wsvt =
{
    ws_QueryInterface,
    ws_AddRef,
    ws_Release,
    ws_PutWord,
    ws_PutAltWord,
    ws_StartAltPhrase,
    ws_EndAltPhrase,
    ws_PutBreak,
};

typedef struct _wordsink_impl
{
    IWordSink IWordSink_iface;
} wordsink_impl;

static wordsink_impl wordsink = { { &wsvt } };

static HRESULT WINAPI fillbuf_none(TEXT_SOURCE *ts)
{
    return E_FAIL;
}

static HRESULT WINAPI fillbuf_many(TEXT_SOURCE *ts)
{
    int i;

    if (ts->awcBuffer == NULL)
        ts->awcBuffer = teststring;
    else
        ts->awcBuffer += ts->iCur;

    if (!ts->awcBuffer[0])
        return E_FAIL;

    for( i=0; ts->awcBuffer[i] && ts->awcBuffer[i] != ' '; i++)
        ;
    if (ts->awcBuffer[i] == ' ')
        i++;

    ts->iCur = 0;
    ts->iEnd = i;

    return S_OK;
}

START_TEST(infosoft)
{
    HRESULT r;
    BOOL license;
    IWordBreaker *wb = NULL;
    TEXT_SOURCE ts;

    r = CoInitialize(NULL);
    ok( r == S_OK, "failed\n");

    r = CoCreateInstance( &CLSID_wb_neutral, NULL, CLSCTX_INPROC_SERVER,
                        &_IID_IWordBreaker, (LPVOID)&wb);

    if (FAILED(r))
        return;

    r = IWordBreaker_Init( wb, FALSE, 0x100, &license );
    ok( r == S_OK, "failed to init the wordbreaker\n");
    /* ok( license == TRUE, "should be no license\n"); */

    wordnum = 0;
    ts.pfnFillTextBuffer = fillbuf_none;
    ts.awcBuffer = teststring;
    ts.iEnd = lstrlenW(ts.awcBuffer);
    ts.iCur = 0;
    r = IWordBreaker_BreakText(wb, &ts, &wordsink.IWordSink_iface, NULL);
    ok( r == S_OK, "failed\n");

    ok(wordnum == 4, "words not processed\n");

    wordnum = 0;
    ts.pfnFillTextBuffer = fillbuf_many;
    ts.awcBuffer = teststring;
    ts.iEnd = 0;
    ts.iCur = 0;

    r = fillbuf_many(&ts);
    ok( r == S_OK, "failed\n");

    r = IWordBreaker_BreakText(wb, &ts, &wordsink.IWordSink_iface, NULL);
    ok( r == S_OK, "failed\n");

    ok(wordnum == 4, "words not processed\n");
    IWordBreaker_Release( wb );

    CoUninitialize();
}
