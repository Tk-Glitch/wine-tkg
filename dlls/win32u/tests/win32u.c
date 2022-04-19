/*
 * Copyright 2021 Jacek Caban for CodeWeavers
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

#include "ntstatus.h"
#define WIN32_NO_STATUS

#include "wine/test.h"

#include "winbase.h"
#include "ntuser.h"


static void test_NtUserEnumDisplayDevices(void)
{
    NTSTATUS ret;
    DISPLAY_DEVICEW info = { sizeof(DISPLAY_DEVICEW) };

    SetLastError( 0xdeadbeef );
    ret = NtUserEnumDisplayDevices( NULL, 0, &info, 0 );
    ok( !ret && GetLastError() == 0xdeadbeef,
        "NtUserEnumDisplayDevices returned %#lx %lu\n", ret, GetLastError() );

    SetLastError( 0xdeadbeef );
    ret = NtUserEnumDisplayDevices( NULL, 12345, &info, 0 );
    ok( ret == STATUS_UNSUCCESSFUL && GetLastError() == 0xdeadbeef,
                  "NtUserEnumDisplayDevices returned %#lx %lu\n", ret,
                  GetLastError() );

    info.cb = 0;

    SetLastError( 0xdeadbeef );
    ret = NtUserEnumDisplayDevices( NULL, 0, &info, 0 );
    ok( ret == STATUS_UNSUCCESSFUL && GetLastError() == 0xdeadbeef,
        "NtUserEnumDisplayDevices returned %#lx %lu\n", ret, GetLastError() );

    SetLastError( 0xdeadbeef );
    ret = NtUserEnumDisplayDevices( NULL, 12345, &info, 0 );
    ok( ret == STATUS_UNSUCCESSFUL && GetLastError() == 0xdeadbeef,
        "NtUserEnumDisplayDevices returned %#lx %lu\n", ret, GetLastError() );

    SetLastError( 0xdeadbeef );
    ret = NtUserEnumDisplayDevices( NULL, 0, NULL, 0 );
    ok( ret == STATUS_UNSUCCESSFUL && GetLastError() == 0xdeadbeef,
        "NtUserEnumDisplayDevices returned %#lx %lu\n", ret, GetLastError() );

    SetLastError( 0xdeadbeef );
    ret = NtUserEnumDisplayDevices( NULL, 12345, NULL, 0 );
    ok( ret == STATUS_UNSUCCESSFUL && GetLastError() == 0xdeadbeef,
        "NtUserEnumDisplayDevices returned %#lx %lu\n", ret, GetLastError() );
}

static void test_NtUserCloseWindowStation(void)
{
    BOOL ret;

    SetLastError( 0xdeadbeef );
    ret = NtUserCloseWindowStation( 0 );
    ok( !ret && GetLastError() == ERROR_INVALID_HANDLE,
        "NtUserCloseWindowStation returned %x %lu\n", ret, GetLastError() );
}

static void test_window_props(void)
{
    HANDLE prop;
    ATOM atom;
    HWND hwnd;
    BOOL ret;

    hwnd = CreateWindowExA( 0, "static", NULL, WS_POPUP, 0,0,0,0,0,0,0, NULL );

    atom = GlobalAddAtomW( L"test" );

    ret = NtUserSetProp( hwnd, UlongToPtr(atom), UlongToHandle(0xdeadbeef) );
    ok( ret, "NtUserSetProp failed: %lu\n", GetLastError() );

    prop = GetPropW( hwnd, L"test" );
    ok( prop == UlongToHandle(0xdeadbeef), "prop = %p\n", prop );

    prop = NtUserGetProp( hwnd, UlongToPtr(atom) );
    ok( prop == UlongToHandle(0xdeadbeef), "prop = %p\n", prop );

    prop = NtUserRemoveProp( hwnd, UlongToPtr(atom) );
    ok( prop == UlongToHandle(0xdeadbeef), "prop = %p\n", prop );

    prop = GetPropW(hwnd, L"test");
    ok(!prop, "prop = %p\n", prop);

    GlobalDeleteAtom( atom );
    DestroyWindow( hwnd );
}

static void test_class(void)
{
    UNICODE_STRING name;
    WCHAR buf[64];
    WNDCLASSW cls;
    ATOM class;
    HWND hwnd;
    ULONG ret;

    memset( &cls, 0, sizeof(cls) );
    cls.style         = CS_HREDRAW | CS_VREDRAW;
    cls.lpfnWndProc   = DefWindowProcW;
    cls.hInstance     = GetModuleHandleW( NULL );
    cls.hbrBackground = GetStockObject( WHITE_BRUSH );
    cls.lpszMenuName  = 0;
    cls.lpszClassName = L"test";

    class = RegisterClassW( &cls );
    ok( class, "RegisterClassW failed: %lu\n", GetLastError() );

    hwnd = CreateWindowW( L"test", L"test name", WS_OVERLAPPEDWINDOW | WS_HSCROLL | WS_VSCROLL,
                          CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, 0, 0, NULL, 0 );

    memset( buf, 0xcc, sizeof(buf) );
    name.Buffer = buf;
    name.Length = 0xdead;
    name.MaximumLength = sizeof(buf);
    ret = NtUserGetAtomName( class, &name );
    ok( ret == 4, "NtUserGetAtomName returned %lu\n", ret );
    ok( name.Length == 0xdead, "Length = %u\n", name.Length );
    ok( name.MaximumLength == sizeof(buf), "MaximumLength = %u\n", name.MaximumLength );
    ok( !wcscmp( buf, L"test" ), "buf = %s\n", debugstr_w(buf) );

    memset( buf, 0xcc, sizeof(buf) );
    name.Buffer = buf;
    name.Length = 0xdead;
    name.MaximumLength = 8;
    ret = NtUserGetAtomName( class, &name );
    ok( ret == 3, "NtUserGetAtomName returned %lu\n", ret );
    ok( name.Length == 0xdead, "Length = %u\n", name.Length );
    ok( name.MaximumLength == 8, "MaximumLength = %u\n", name.MaximumLength );
    ok( !wcscmp( buf, L"tes" ), "buf = %s\n", debugstr_w(buf) );

    memset( buf, 0xcc, sizeof(buf) );
    name.Buffer = buf;
    name.MaximumLength = 1;
    SetLastError( 0xdeadbeef );
    ret = NtUserGetAtomName( class, &name );
    ok( !ret && GetLastError() == ERROR_INSUFFICIENT_BUFFER,
        "NtUserGetAtomName returned %lx %lu\n", ret, GetLastError() );

    memset( buf, 0xcc, sizeof(buf) );
    name.Buffer = buf;
    name.Length = 0xdead;
    name.MaximumLength = sizeof(buf);
    ret = NtUserGetClassName( hwnd, FALSE, &name );
    ok( ret == 4, "NtUserGetClassName returned %lu\n", ret );
    ok( name.Length == 0xdead, "Length = %u\n", name.Length );
    ok( name.MaximumLength == sizeof(buf), "MaximumLength = %u\n", name.MaximumLength );
    ok( !wcscmp( buf, L"test" ), "buf = %s\n", debugstr_w(buf) );

    memset( buf, 0xcc, sizeof(buf) );
    name.Buffer = buf;
    name.Length = 0xdead;
    name.MaximumLength = 8;
    ret = NtUserGetClassName( hwnd, FALSE, &name );
    ok( ret == 3, "NtUserGetClassName returned %lu\n", ret );
    ok( name.Length == 0xdead, "Length = %u\n", name.Length );
    ok( name.MaximumLength == 8, "MaximumLength = %u\n", name.MaximumLength );
    ok( !wcscmp( buf, L"tes" ), "buf = %s\n", debugstr_w(buf) );

    memset( buf, 0xcc, sizeof(buf) );
    name.Buffer = buf;
    name.MaximumLength = 1;
    SetLastError( 0xdeadbeef );
    ret = NtUserGetClassName( hwnd, FALSE, &name );
    ok( !ret && GetLastError() == ERROR_INSUFFICIENT_BUFFER,
        "NtUserGetClassName returned %lx %lu\n", ret, GetLastError() );

    DestroyWindow( hwnd );

    ret = UnregisterClassW( L"test", GetModuleHandleW(NULL) );
    ok( ret, "UnregisterClassW failed: %lu\n", GetLastError() );

    memset( buf, 0xcc, sizeof(buf) );
    name.Buffer = buf;
    name.MaximumLength = sizeof(buf);
    SetLastError( 0xdeadbeef );
    ret = NtUserGetAtomName( class, &name );
    ok( !ret && GetLastError() == ERROR_INVALID_HANDLE,
        "NtUserGetAtomName returned %lx %lu\n", ret, GetLastError() );
    ok( buf[0] == 0xcccc, "buf = %s\n", debugstr_w(buf) );

}

static BOOL WINAPI count_win( HWND hwnd, LPARAM lparam )
{
    ULONG *cnt = (ULONG *)lparam;
    (*cnt)++;
    return TRUE;
}

static void test_NtUserBuildHwndList(void)
{
    ULONG size, desktop_windows_cnt;
    HWND buf[512], hwnd;
    NTSTATUS status;

    size = 0;
    status = NtUserBuildHwndList( 0, 0, 0, 0, GetCurrentThreadId(), ARRAYSIZE(buf), buf, &size );
    ok( !status, "NtUserBuildHwndList failed: %#lx\n", status );
    ok( size == 1, "size = %lu\n", size );
    ok( buf[0] == HWND_BOTTOM, "buf[0] = %p\n", buf[0] );

    hwnd = CreateWindowExA( 0, "static", NULL, WS_POPUP, 0,0,0,0,GetDesktopWindow(),0,0, NULL );

    size = 0;
    status = NtUserBuildHwndList( 0, 0, 0, 0, GetCurrentThreadId(), ARRAYSIZE(buf), buf, &size );
    ok( !status, "NtUserBuildHwndList failed: %#lx\n", status );
    ok( size == 3, "size = %lu\n", size );
    ok( buf[0] == hwnd, "buf[0] = %p\n", buf[0] );
    ok( buf[2] == HWND_BOTTOM, "buf[0] = %p\n", buf[2] );

    size = 0;
    status = NtUserBuildHwndList( 0, 0, 0, 0, GetCurrentThreadId(), 3, buf, &size );
    ok( !status, "NtUserBuildHwndList failed: %#lx\n", status );
    ok( size == 3, "size = %lu\n", size );

    size = 0;
    status = NtUserBuildHwndList( 0, 0, 0, 0, GetCurrentThreadId(), 2, buf, &size );
    ok( status == STATUS_BUFFER_TOO_SMALL, "NtUserBuildHwndList failed: %#lx\n", status );
    ok( size == 3, "size = %lu\n", size );

    size = 0;
    status = NtUserBuildHwndList( 0, 0, 0, 0, GetCurrentThreadId(), 1, buf, &size );
    ok( status == STATUS_BUFFER_TOO_SMALL, "NtUserBuildHwndList failed: %#lx\n", status );
    ok( size == 3, "size = %lu\n", size );

    desktop_windows_cnt = 0;
    EnumDesktopWindows( 0, count_win, (LPARAM)&desktop_windows_cnt );

    size = 0;
    status = NtUserBuildHwndList( 0, 0, 0, 1, 0, ARRAYSIZE(buf), buf, &size );
    ok( !status, "NtUserBuildHwndList failed: %#lx\n", status );
    ok( size == desktop_windows_cnt + 1, "size = %lu, expected %lu\n", size, desktop_windows_cnt + 1 );

    desktop_windows_cnt = 0;
    EnumDesktopWindows( GetThreadDesktop( GetCurrentThreadId() ), count_win, (LPARAM)&desktop_windows_cnt );

    size = 0;
    status = NtUserBuildHwndList( GetThreadDesktop(GetCurrentThreadId()), 0, 0, 1, 0,
                                  ARRAYSIZE(buf), buf, &size );
    ok( !status, "NtUserBuildHwndList failed: %#lx\n", status );
    ok( size == desktop_windows_cnt + 1, "size = %lu, expected %lu\n", size, desktop_windows_cnt + 1 );

    size = 0;
    status = NtUserBuildHwndList( GetThreadDesktop(GetCurrentThreadId()), 0, 0, 0, 0,
                                  ARRAYSIZE(buf), buf, &size );
    ok( !status, "NtUserBuildHwndList failed: %#lx\n", status );
    todo_wine
    ok( size > desktop_windows_cnt + 1, "size = %lu, expected %lu\n", size, desktop_windows_cnt + 1 );

    size = 0xdeadbeef;
    status = NtUserBuildHwndList( UlongToHandle(0xdeadbeef), 0, 0, 0, 0,
                                  ARRAYSIZE(buf), buf, &size );
    ok( status == STATUS_INVALID_HANDLE, "NtUserBuildHwndList failed: %#lx\n", status );
    ok( size == 0xdeadbeef, "size = %lu\n", size );

    DestroyWindow( hwnd );
}

static void test_cursoricon(void)
{
    WCHAR module[MAX_PATH], res_buf[MAX_PATH];
    UNICODE_STRING module_str, res_str;
    BYTE bmp_bits[1024];
    LONG width, height;
    DWORD rate, steps;
    HCURSOR frame;
    HANDLE handle;
    ICONINFO info;
    unsigned int i;
    BOOL ret;

    for (i = 0; i < sizeof(bmp_bits); ++i)
        bmp_bits[i] = 111 * i;

    handle = CreateIcon( 0, 16, 16, 1, 1, bmp_bits, &bmp_bits[16 * 16 / 8] );
    ok(handle != 0, "CreateIcon failed\n");

    ret = NtUserGetIconSize( handle, 0, &width, &height );
    ok( ret, "NtUserGetIconSize failed: %lu\n", GetLastError() );
    ok( width == 16, "width = %ld\n", width );
    ok( height == 32, "height = %ld\n", height );

    ret = NtUserGetIconSize( handle, 6, &width, &height );
    ok( ret, "NtUserGetIconSize failed: %lu\n", GetLastError() );
    ok( width == 16, "width = %ld\n", width );
    ok( height == 32, "height = %ld\n", height );

    frame = NtUserGetCursorFrameInfo( handle, 0, &rate, &steps );
    ok( frame != NULL, "NtUserGetCursorFrameInfo failed: %lu\n", GetLastError() );
    ok( frame == handle, "frame != handle\n" );
    ok( rate == 0, "rate = %lu\n", rate );
    ok( steps == 1, "steps = %lu\n", steps );

    ret = NtUserGetIconInfo( handle, &info, NULL, NULL, NULL, 0 );
    ok( ret, "NtUserGetIconInfo failed: %lu\n", GetLastError() );
    ok( info.fIcon == TRUE, "fIcon = %x\n", info.fIcon );
    ok( info.xHotspot == 8, "xHotspot = %lx\n", info.xHotspot );
    ok( info.yHotspot == 8, "yHotspot = %lx\n", info.yHotspot );
    DeleteObject( info.hbmColor );
    DeleteObject( info.hbmMask );

    memset( module, 0xcc, sizeof(module) );
    module_str.Buffer = module;
    module_str.Length = 0xdead;
    module_str.MaximumLength = sizeof(module);
    memset( res_buf, 0xcc, sizeof(res_buf) );
    res_str.Buffer = res_buf;
    res_str.Length = 0xdead;
    res_str.MaximumLength = sizeof(res_buf);
    ret = NtUserGetIconInfo( handle, &info, &module_str, &res_str, NULL, 0 );
    ok( ret, "NtUserGetIconInfo failed: %lu\n", GetLastError() );
    ok( info.fIcon == TRUE, "fIcon = %x\n", info.fIcon );
    ok( !module_str.Length, "module_str.Length = %u\n", module_str.Length );
    ok( !res_str.Length, "res_str.Length = %u\n", res_str.Length );
    ok( module_str.Buffer == module, "module_str.Buffer = %p\n", module_str.Buffer );
    ok( !res_str.Buffer, "res_str.Buffer = %p\n", res_str.Buffer );
    ok( module[0] == 0xcccc, "module[0] = %x\n", module[0] );
    ok( res_buf[0] == 0xcccc, "res_buf[0] = %x\n", res_buf[0] );
    DeleteObject( info.hbmColor );
    DeleteObject( info.hbmMask );

    ret = NtUserDestroyCursor( handle, 0 );
    ok( ret, "NtUserDestroyIcon failed: %lu\n", GetLastError() );

    SetLastError( 0xdeadbeef );
    ret = NtUserGetIconSize( handle, 0, &width, &height );
    ok( !ret && GetLastError() == ERROR_INVALID_CURSOR_HANDLE,
        "NtUserGetIconSize returned %x %lu\n", ret, GetLastError() );

    /* Test a system icon */
    handle = LoadIconA( 0, (LPCSTR)IDI_HAND );
    ok( handle != NULL, "LoadIcon icon failed, error %lu\n", GetLastError() );

    ret = NtUserGetIconSize( handle, 0, &width, &height );
    ok( width == 32, "width = %ld\n", width );
    ok( height == 64, "height = %ld\n", height );
    ok( ret, "NtUserGetIconSize failed: %lu\n", GetLastError() );

    memset( module, 0xcc, sizeof(module) );
    module_str.Buffer = module;
    module_str.Length = 0xdead;
    module_str.MaximumLength = sizeof(module);
    memset( res_buf, 0xcc, sizeof(res_buf) );
    res_str.Buffer = res_buf;
    res_str.Length = 0xdead;
    res_str.MaximumLength = sizeof(res_buf);
    ret = NtUserGetIconInfo( handle, &info, &module_str, &res_str, NULL, 0 );
    ok( ret, "NtUserGetIconInfo failed: %lu\n", GetLastError() );
    ok( info.fIcon == TRUE, "fIcon = %x\n", info.fIcon );
    ok( module_str.Length, "module_str.Length = 0\n" );
    ok( !res_str.Length, "res_str.Length = %u\n", res_str.Length );
    ok( module_str.Buffer == module, "module_str.Buffer = %p\n", module_str.Buffer );
    ok( res_str.Buffer == (WCHAR *)IDI_HAND, "res_str.Buffer = %p\n", res_str.Buffer );
    DeleteObject( info.hbmColor );
    DeleteObject( info.hbmMask );

    module[module_str.Length] = 0;
    ok( GetModuleHandleW(module) == GetModuleHandleW(L"user32.dll"),
        "GetIconInfoEx wrong module %s\n", wine_dbgstr_w(module) );

    ret = DestroyIcon(handle);
    ok(ret, "Destroy icon failed, error %lu.\n", GetLastError());
}

START_TEST(win32u)
{
    /* native win32u.dll fails if user32 is not loaded, so make sure it's fully initialized */
    GetDesktopWindow();

    test_NtUserEnumDisplayDevices();
    test_window_props();
    test_class();
    test_NtUserBuildHwndList();
    test_cursoricon();

    test_NtUserCloseWindowStation();
}
