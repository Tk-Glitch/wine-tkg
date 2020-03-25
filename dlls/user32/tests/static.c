/* Unit test suite for static controls.
 *
 * Copyright 2007 Google (Mikolaj Zalewski)
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
#include <stdio.h>

#define STRICT
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "wine/test.h"

#define TODO_COUNT 1

#define CTRL_ID 1995

static HWND hMainWnd;

#define expect_eq(expr, value, type, fmt) { type val = expr; ok(val == (value), #expr " expected " fmt " got " fmt "\n", (value), val); }

static int g_nReceivedColorStatic = 0;

/* try to make sure pending X events have been processed before continuing */
static void flush_events(void)
{
    MSG msg;
    int diff = 200;
    int min_timeout = 100;
    DWORD time = GetTickCount() + diff;

    while (diff > 0)
    {
        if (MsgWaitForMultipleObjects( 0, NULL, FALSE, min_timeout, QS_ALLINPUT ) == WAIT_TIMEOUT) break;
        while (PeekMessageA( &msg, 0, 0, 0, PM_REMOVE )) DispatchMessageA( &msg );
        diff = time - GetTickCount();
    }
}

static HWND build_static(DWORD style)
{
    return CreateWindowA("static", "Test", WS_VISIBLE|WS_CHILD|style, 5, 5, 100, 100, hMainWnd, (HMENU)CTRL_ID, NULL, 0);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
    case WM_CTLCOLORSTATIC:
        {
            HDC hdc = (HDC)wparam;
            HRGN hrgn = CreateRectRgn(0, 0, 1, 1);
            ok(GetClipRgn(hdc, hrgn) == 1, "Static controls during a WM_CTLCOLORSTATIC must have a clipping region\n");
            DeleteObject(hrgn);
            g_nReceivedColorStatic++;
            return (LRESULT) GetStockObject(BLACK_BRUSH);
        }
        break;
    }

    return DefWindowProcA(hwnd, msg, wparam, lparam);
}

static void test_updates(int style, int flags)
{
    RECT r1 = {20, 20, 30, 30};
    HWND hStatic = build_static(style);
    int exp;

    flush_events();
    trace("Testing style 0x%x\n", style);
    g_nReceivedColorStatic = 0;
    /* during each update parent WndProc will test the WM_CTLCOLORSTATIC message */
    InvalidateRect(hMainWnd, NULL, FALSE);
    UpdateWindow(hMainWnd);
    InvalidateRect(hMainWnd, &r1, FALSE);
    UpdateWindow(hMainWnd);
    InvalidateRect(hStatic, &r1, FALSE);
    UpdateWindow(hStatic);
    InvalidateRect(hStatic, NULL, FALSE);
    UpdateWindow(hStatic);

    if( (style & SS_TYPEMASK) == SS_BITMAP) {
        HDC hdc = GetDC( hStatic);
        COLORREF colour = GetPixel( hdc, 10, 10);
        ok ( colour != 0, "pixel should NOT be painted black!\n");
        ReleaseDC(hStatic, hdc);
    }
    if (style != SS_ETCHEDHORZ && style != SS_ETCHEDVERT)
        exp = 4;
    else
        exp = 1; /* SS_ETCHED* seems to send WM_CTLCOLORSTATIC only sometimes */

    if (flags & TODO_COUNT)
        todo_wine { expect_eq(g_nReceivedColorStatic, exp, int, "%d"); }
    else if ((style & SS_TYPEMASK) == SS_ICON || (style & SS_TYPEMASK) == SS_BITMAP)
        ok( g_nReceivedColorStatic == exp, "expected %u got %u\n", exp, g_nReceivedColorStatic );
    else
        expect_eq(g_nReceivedColorStatic, exp, int, "%d");
    DestroyWindow(hStatic);
}

static void test_set_text(void)
{
    HWND hStatic = build_static(SS_SIMPLE);
    char buffA[10];

    GetWindowTextA(hStatic, buffA, sizeof(buffA));
    ok(!strcmp(buffA, "Test"), "got wrong text %s\n", buffA);

    SetWindowTextA(hStatic, NULL);
    GetWindowTextA(hStatic, buffA, sizeof(buffA));
    ok(buffA[0] == 0, "got wrong text %s\n", buffA);

    DestroyWindow(hStatic);
}

static void test_image(HBITMAP image)
{
    BITMAP bm;
    HDC hdc;
    BITMAPINFO info;
    BYTE bits[4];

    GetObjectW(image, sizeof(bm), &bm);
    ok(bm.bmWidth == 1, "got %d\n", bm.bmWidth);
    ok(bm.bmHeight == 1, "got %d\n", bm.bmHeight);
    ok(bm.bmBitsPixel == 32, "got %d\n", bm.bmBitsPixel);
    ok(bm.bmBits == NULL, "bmBits is not NULL\n");

    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = bm.bmWidth;
    info.bmiHeader.biHeight = bm.bmHeight;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    info.bmiHeader.biSizeImage = 4;
    info.bmiHeader.biXPelsPerMeter = 0;
    info.bmiHeader.biYPelsPerMeter = 0;
    info.bmiHeader.biClrUsed = 0;
    info.bmiHeader.biClrImportant = 0;

    hdc = CreateCompatibleDC(0);
    GetDIBits(hdc, image, 0, bm.bmHeight, bits, &info, DIB_RGB_COLORS);
    DeleteDC(hdc);

    ok(bits[0] == 0x11 &&  bits[1] == 0x22 &&  bits[2] == 0x33 && bits[3] == 0x44,
       "bits: %02x %02x %02x %02x\n", bits[0], bits[1], bits[2], bits[3]);
}

static void test_set_image(void)
{
    HWND hwnd = build_static(SS_BITMAP);
    HBITMAP bmp, image;

    image = LoadImageW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(101), IMAGE_BITMAP, 0, 0, 0);
    ok(image != NULL, "LoadImage failed\n");

    test_image(image);

    bmp = (HBITMAP)SendMessageW(hwnd, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)image);
    ok(bmp == NULL, "got not NULL\n");

    bmp = (HBITMAP)SendMessageW(hwnd, STM_GETIMAGE, IMAGE_BITMAP, 0);
    ok(bmp != NULL, "got NULL\n");
    ok(bmp == image, "bmp != image\n");

    bmp = (HBITMAP)SendMessageW(hwnd, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)image);
    ok(bmp != NULL, "got NULL\n");
    ok(bmp == image, "bmp != image\n");
    test_image(image);

    DestroyWindow(hwnd);
    DeleteObject(image);
}

START_TEST(static)
{
    static const char szClassName[] = "testclass";
    WNDCLASSEXA  wndclass;

    wndclass.cbSize         = sizeof(wndclass);
    wndclass.style          = CS_HREDRAW | CS_VREDRAW;
    wndclass.lpfnWndProc    = WndProc;
    wndclass.cbClsExtra     = 0;
    wndclass.cbWndExtra     = 0;
    wndclass.hInstance      = GetModuleHandleA(NULL);
    wndclass.hIcon          = LoadIconA(NULL, (LPCSTR)IDI_APPLICATION);
    wndclass.hIconSm        = LoadIconA(NULL, (LPCSTR)IDI_APPLICATION);
    wndclass.hCursor        = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);
    wndclass.hbrBackground  = GetStockObject(WHITE_BRUSH);
    wndclass.lpszClassName  = szClassName;
    wndclass.lpszMenuName   = NULL;
    RegisterClassExA(&wndclass);

    hMainWnd = CreateWindowA(szClassName, "Test", WS_OVERLAPPEDWINDOW, 0, 0, 500, 500, NULL, NULL, GetModuleHandleA(NULL), NULL);
    ShowWindow(hMainWnd, SW_SHOW);

    test_updates(0, 0);
    test_updates(SS_SIMPLE, 0);
    test_updates(SS_ICON, 0);
    test_updates(SS_BITMAP, 0);
    test_updates(SS_BITMAP | SS_CENTERIMAGE, 0);
    test_updates(SS_BLACKRECT, TODO_COUNT);
    test_updates(SS_WHITERECT, TODO_COUNT);
    test_updates(SS_ETCHEDHORZ, TODO_COUNT);
    test_updates(SS_ETCHEDVERT, TODO_COUNT);
    test_set_text();
    test_set_image();

    DestroyWindow(hMainWnd);
}
