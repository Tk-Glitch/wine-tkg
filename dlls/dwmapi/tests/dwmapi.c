/*
 * Unit tests for dwmapi
 *
 * Copyright 2018 Louis Lenders
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
 */

#include <windows.h>
#include "dwmapi.h"
#include "wine/test.h"

static HRESULT (WINAPI *pDwmIsCompositionEnabled)(BOOL*);
static HRESULT (WINAPI *pDwmEnableComposition)(UINT);
static HRESULT (WINAPI *pDwmGetTransportAttributes)(BOOL*,BOOL*,DWORD*);
static HWND test_wnd;
static LRESULT WINAPI test_wndproc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    return DefWindowProcA(hwnd, message, wParam, lParam);
}

static void test_DwmGetWindowAttribute(void)
{
    BOOL nc_rendering;
    RECT rc, rc2;
    HRESULT hr;

    hr = DwmGetWindowAttribute(NULL, DWMWA_NCRENDERING_ENABLED, &nc_rendering, sizeof(nc_rendering));
    ok(hr == E_HANDLE || broken(hr == E_INVALIDARG) /* Vista */, "DwmGetWindowAttribute(DWMWA_NCRENDERING_ENABLED) returned 0x%08x.\n", hr);
    hr = DwmGetWindowAttribute(test_wnd, DWMWA_NCRENDERING_ENABLED, NULL, sizeof(nc_rendering));
    ok(hr == E_INVALIDARG, "DwmGetWindowAttribute(DWMWA_NCRENDERING_ENABLED) returned 0x%08x.\n", hr);
    hr = DwmGetWindowAttribute(test_wnd, DWMWA_NCRENDERING_ENABLED, &nc_rendering, 0);
    ok(hr == E_INVALIDARG, "DwmGetWindowAttribute(DWMWA_NCRENDERING_ENABLED) returned 0x%08x.\n", hr);
    nc_rendering = FALSE;
    hr = DwmGetWindowAttribute(test_wnd, 0xdeadbeef, &nc_rendering, sizeof(nc_rendering));
    ok(hr == E_INVALIDARG, "DwmGetWindowAttribute(0xdeadbeef) returned 0x%08x.\n", hr);

    nc_rendering = 0xdeadbeef;
    hr = DwmGetWindowAttribute(test_wnd, DWMWA_NCRENDERING_ENABLED, &nc_rendering, sizeof(nc_rendering));
    ok(hr == S_OK, "DwmGetWindowAttribute(DWMWA_NCRENDERING_ENABLED) failed 0x%08x.\n", hr);
    ok(nc_rendering == FALSE || nc_rendering == TRUE, "non-boolean value 0x%x.\n", nc_rendering);

    hr = DwmGetWindowAttribute(test_wnd, DWMWA_EXTENDED_FRAME_BOUNDS, &rc, sizeof(rc) - 1);
    ok(hr == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER) || broken(hr == E_INVALIDARG) /* Vista */,
       "DwmGetWindowAttribute(DWMWA_EXTENDED_FRAME_BOUNDS) returned 0x%08x.\n", hr);
    hr = DwmGetWindowAttribute(test_wnd, DWMWA_EXTENDED_FRAME_BOUNDS, &rc, sizeof(rc));
    if (hr != E_HANDLE && hr != DWM_E_COMPOSITIONDISABLED /* Vista */)  /* composition is on */
    {
        /* For top-level Windows, the returned rect is always at least as large as GetWindowRect */
        GetWindowRect(test_wnd, &rc2);
        ok(hr == S_OK, "DwmGetWindowAttribute(DWMWA_EXTENDED_FRAME_BOUNDS) failed 0x%08x.\n", hr);
        ok(rc.left >= rc2.left && rc.right <= rc2.right && rc.top >= rc2.top && rc.bottom <= rc2.bottom,
           "returned rect %s not enclosed in window rect %s.\n", wine_dbgstr_rect(&rc), wine_dbgstr_rect(&rc2));
    }
}


BOOL dwmenabled;

static void test_isdwmenabled(void)
{
    HRESULT res;
    BOOL ret;

    ret = -1;
    res = pDwmIsCompositionEnabled(&ret);
    ok((res == S_OK && ret == TRUE) || (res == S_OK && ret == FALSE), "got %x and %d\n", res, ret);

    if (res == S_OK && ret == TRUE)
        dwmenabled = TRUE;
    else
        dwmenabled = FALSE;
    /*tested on win7 by enabling/disabling DWM service via services.msc*/
    if (dwmenabled)
    {
        res = pDwmEnableComposition(DWM_EC_DISABLECOMPOSITION); /* try disable and reenable dwm*/
        ok(res == S_OK, "got %x expected S_OK\n", res);

        ret = -1;
        res = pDwmIsCompositionEnabled(&ret);
        ok((res == S_OK && ret == FALSE) /*wvista win7*/ || (res == S_OK && ret == TRUE) /*>win7*/, "got %x and %d\n", res, ret);

        res = pDwmEnableComposition(DWM_EC_ENABLECOMPOSITION);
        ok(res == S_OK, "got %x\n", res);

        ret = -1;
        res = pDwmIsCompositionEnabled(&ret);
        todo_wine ok(res == S_OK && ret == TRUE, "got %x and %d\n", res, ret);
    }
    else
    {
        res = pDwmEnableComposition(DWM_EC_ENABLECOMPOSITION); /*cannot enable DWM composition this way*/
        ok(res == S_OK /*win7 testbot*/ || res == DWM_E_COMPOSITIONDISABLED /*win7 laptop*/, "got %x\n", res);
        if (winetest_debug > 1)
            trace("returning %x\n", res);

        ret = -1;
        res = pDwmIsCompositionEnabled(&ret);
        ok(res == S_OK && ret == FALSE, "got %x  and %d\n", res, ret);
    }
}

static void test_dwm_get_transport_attributes(void)
{
    BOOL isremoting, isconnected;
    DWORD generation;
    HRESULT res;

    res = pDwmGetTransportAttributes(&isremoting, &isconnected, &generation);
    if (dwmenabled)
        ok(res == S_OK, "got %x\n", res);
    else
    {
        ok(res == S_OK /*win7 testbot*/ || res == DWM_E_COMPOSITIONDISABLED /*win7 laptop*/, "got %x\n", res);
        if (winetest_debug > 1)
            trace("returning %x\n", res);
    }
}

START_TEST(dwmapi)
{
    HMODULE hmod = LoadLibraryA("dwmapi.dll");
    HINSTANCE inst = GetModuleHandleA(NULL);
    WNDCLASSA cls;

    cls.style = 0;
    cls.lpfnWndProc = test_wndproc;
    cls.cbClsExtra = 0;
    cls.cbWndExtra = 0;
    cls.hInstance = inst;
    cls.hIcon = 0;
    cls.hCursor = LoadCursorA(0, (LPCSTR)IDC_ARROW);
    cls.hbrBackground = GetStockObject(WHITE_BRUSH);
    cls.lpszMenuName = NULL;
    cls.lpszClassName = "Test";
    RegisterClassA(&cls);


    if (!hmod)
    {
        trace("dwmapi not found, skipping tests\n");
        return;
    }

    pDwmIsCompositionEnabled = (void *)GetProcAddress(hmod, "DwmIsCompositionEnabled");
    pDwmEnableComposition = (void *)GetProcAddress(hmod, "DwmEnableComposition");
    pDwmGetTransportAttributes = (void *)GetProcAddress(hmod, "DwmGetTransportAttributes");

    test_isdwmenabled();
    test_dwm_get_transport_attributes();

    test_wnd = CreateWindowExA(0, "Test", "Test Window", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                               100, 100, 200, 200, 0, 0, 0, NULL);
    ok(test_wnd != NULL, "Failed to create test window.\n");

    test_DwmGetWindowAttribute();

    DestroyWindow(test_wnd);
    UnregisterClassA("Test", inst);

}
