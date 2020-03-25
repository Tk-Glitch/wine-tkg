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
#include "commctrl.h"
#include "resources.h"

#include "wine/test.h"

#include "v6util.h"

#define TODO_COUNT 1

#define CTRL_ID 1995

static HWND hMainWnd;
static int g_nReceivedColorStatic;

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

static HWND create_static(DWORD style)
{
    return CreateWindowA(WC_STATICA, "Test", WS_VISIBLE|WS_CHILD|style, 5, 5, 100, 100, hMainWnd, (HMENU)CTRL_ID, NULL, 0);
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
    HWND hStatic = create_static(style);
    RECT r1 = {20, 20, 30, 30};
    int exp;

    flush_events();
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

    if ((style & SS_TYPEMASK) == SS_BITMAP)
    {
        HDC hdc = GetDC(hStatic);
        COLORREF colour = GetPixel(hdc, 10, 10);
    todo_wine
        ok(colour == 0, "Unexpected pixel color.\n");
        ReleaseDC(hStatic, hdc);
    }

    if (style != SS_ETCHEDHORZ && style != SS_ETCHEDVERT)
        exp = 4;
    else
        exp = 1; /* SS_ETCHED* seems to send WM_CTLCOLORSTATIC only sometimes */

    if (flags & TODO_COUNT)
    todo_wine
        ok(g_nReceivedColorStatic == exp, "Unexpected WM_CTLCOLORSTATIC value %d\n", g_nReceivedColorStatic);
    else if ((style & SS_TYPEMASK) == SS_ICON || (style & SS_TYPEMASK) == SS_BITMAP)
        ok(g_nReceivedColorStatic == exp, "Unexpected %u got %u\n", exp, g_nReceivedColorStatic);
    else
        ok(g_nReceivedColorStatic == exp, "Unexpected WM_CTLCOLORSTATIC value %d\n", g_nReceivedColorStatic);
    DestroyWindow(hStatic);
}

static void test_set_text(void)
{
    HWND hStatic = create_static(SS_SIMPLE);
    char buffA[10];

    GetWindowTextA(hStatic, buffA, sizeof(buffA));
    ok(!strcmp(buffA, "Test"), "got wrong text %s\n", buffA);

    SetWindowTextA(hStatic, NULL);
    GetWindowTextA(hStatic, buffA, sizeof(buffA));
    ok(buffA[0] == 0, "got wrong text %s\n", buffA);

    DestroyWindow(hStatic);
}

static void remove_alpha(HBITMAP hbitmap)
{
    BITMAP bm;
    BYTE *ptr;
    int i;

    GetObjectW(hbitmap, sizeof(bm), &bm);
    ok(bm.bmBits != NULL, "bmBits is NULL\n");

    for (i = 0, ptr = bm.bmBits; i < bm.bmWidth * bm.bmHeight; i++, ptr += 4)
        ptr[3] = 0;
}

static void test_image(HBITMAP image, BOOL is_dib, BOOL is_premult, BOOL is_alpha)
{
    BITMAP bm;
    HDC hdc;
    BITMAPINFO info;
    BYTE bits[4];

    GetObjectW(image, sizeof(bm), &bm);
    ok(bm.bmWidth == 1, "got %d\n", bm.bmWidth);
    ok(bm.bmHeight == 1, "got %d\n", bm.bmHeight);
    ok(bm.bmBitsPixel == 32, "got %d\n", bm.bmBitsPixel);
    if (is_dib)
    {
        ok(bm.bmBits != NULL, "bmBits is NULL\n");
        memcpy(bits, bm.bmBits, 4);
        if (is_premult)
todo_wine
            ok(bits[0] == 0x05 &&  bits[1] == 0x09 &&  bits[2] == 0x0e && bits[3] == 0x44,
               "bits: %02x %02x %02x %02x\n", bits[0], bits[1], bits[2], bits[3]);
        else if (is_alpha)
            ok(bits[0] == 0x11 &&  bits[1] == 0x22 &&  bits[2] == 0x33 && bits[3] == 0x44,
               "bits: %02x %02x %02x %02x\n", bits[0], bits[1], bits[2], bits[3]);
        else
            ok(bits[0] == 0x11 &&  bits[1] == 0x22 &&  bits[2] == 0x33 && bits[3] == 0,
               "bits: %02x %02x %02x %02x\n", bits[0], bits[1], bits[2], bits[3]);
    }
    else
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

    if (is_premult)
todo_wine
        ok(bits[0] == 0x05 &&  bits[1] == 0x09 &&  bits[2] == 0x0e && bits[3] == 0x44,
           "bits: %02x %02x %02x %02x\n", bits[0], bits[1], bits[2], bits[3]);
    else if (is_alpha)
        ok(bits[0] == 0x11 &&  bits[1] == 0x22 &&  bits[2] == 0x33 && bits[3] == 0x44,
           "bits: %02x %02x %02x %02x\n", bits[0], bits[1], bits[2], bits[3]);
    else
        ok(bits[0] == 0x11 &&  bits[1] == 0x22 &&  bits[2] == 0x33 && bits[3] == 0,
           "bits: %02x %02x %02x %02x\n", bits[0], bits[1], bits[2], bits[3]);
}

static void test_set_image(void)
{
    HWND hwnd = create_static(SS_BITMAP);
    HBITMAP bmp1, bmp2, image;

    image = LoadImageW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDB_BITMAP_1x1_32BPP), IMAGE_BITMAP, 0, 0, 0);
    ok(image != NULL, "LoadImage failed\n");

    test_image(image, FALSE, FALSE, TRUE);

    bmp1 = (HBITMAP)SendMessageW(hwnd, STM_GETIMAGE, IMAGE_BITMAP, 0);
    ok(bmp1 == NULL, "got not NULL\n");

    bmp1 = (HBITMAP)SendMessageW(hwnd, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)image);
    ok(bmp1 == NULL, "got not NULL\n");

    bmp1 = (HBITMAP)SendMessageW(hwnd, STM_GETIMAGE, IMAGE_BITMAP, 0);
    ok(bmp1 != NULL, "got NULL\n");
    ok(bmp1 != image, "bmp == image\n");
    test_image(bmp1, TRUE, TRUE, TRUE);

    bmp2 = (HBITMAP)SendMessageW(hwnd, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)image);
    ok(bmp2 != NULL, "got NULL\n");
    ok(bmp2 != image, "bmp == image\n");
    ok(bmp1 == bmp2, "bmp1 != bmp2\n");
    test_image(bmp2, TRUE, TRUE, TRUE);

    DeleteObject(image);

    image = LoadImageW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDB_BITMAP_1x1_32BPP), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
    ok(image != NULL, "LoadImage failed\n");

    test_image(image, TRUE, FALSE, TRUE);
    remove_alpha(image);
    test_image(image, TRUE, FALSE, FALSE);

    bmp1 = (HBITMAP)SendMessageW(hwnd, STM_GETIMAGE, IMAGE_BITMAP, 0);
    ok(bmp1 != NULL, "got NULL\n");
    ok(bmp1 != image, "bmp == image\n");
    test_image(bmp1, TRUE, TRUE, TRUE);

    bmp2 = (HBITMAP)SendMessageW(hwnd, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)image);
    ok(bmp2 != NULL, "got NULL\n");
    ok(bmp2 != image, "bmp == image\n");
    ok(bmp1 == bmp2, "bmp1 != bmp2\n");
    test_image(bmp1, TRUE, TRUE, FALSE);

    bmp2 = (HBITMAP)SendMessageW(hwnd, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)image);
    ok(bmp2 != NULL, "got NULL\n");
    ok(bmp2 == image, "bmp1 != image\n");
    test_image(bmp2, TRUE, FALSE, FALSE);

    DestroyWindow(hwnd);

    test_image(image, TRUE, FALSE, FALSE);

    DeleteObject(image);
}

START_TEST(static)
{
    static const char classname[] = "testclass";
    WNDCLASSEXA wndclass;
    ULONG_PTR ctx_cookie;
    HANDLE hCtx;

    if (!load_v6_module(&ctx_cookie, &hCtx))
        return;

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
    wndclass.lpszClassName  = classname;
    wndclass.lpszMenuName   = NULL;
    RegisterClassExA(&wndclass);

    hMainWnd = CreateWindowA(classname, "Test", WS_OVERLAPPEDWINDOW, 0, 0, 500, 500, NULL, NULL,
        GetModuleHandleA(NULL), NULL);
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

    unload_v6_module(ctx_cookie, hCtx);
}
