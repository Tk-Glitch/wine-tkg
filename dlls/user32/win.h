/*
 * Window definitions
 *
 * Copyright 1993 Alexandre Julliard
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

#ifndef __WINE_WIN_H
#define __WINE_WIN_H

#include <stdarg.h>
#include <windef.h>
#include <winbase.h>
#include <wingdi.h>
#include <winuser.h>

#include "user_private.h"
#include "wine/server_protocol.h"

struct tagCLASS;
struct tagDIALOGINFO;

  /* Window functions */
extern HWND get_hwnd_message_parent(void) DECLSPEC_HIDDEN;
extern BOOL is_desktop_window( HWND hwnd ) DECLSPEC_HIDDEN;
extern struct window_surface dummy_surface DECLSPEC_HIDDEN;
extern void register_window_surface( struct window_surface *old, struct window_surface *new ) DECLSPEC_HIDDEN;
extern void flush_window_surfaces( BOOL idle ) DECLSPEC_HIDDEN;
extern WND *WIN_GetPtr( HWND hwnd ) DECLSPEC_HIDDEN;
extern HWND WIN_GetFullHandle( HWND hwnd ) DECLSPEC_HIDDEN;
extern HWND WIN_IsCurrentProcess( HWND hwnd ) DECLSPEC_HIDDEN;
extern HWND WIN_IsCurrentThread( HWND hwnd ) DECLSPEC_HIDDEN;
extern UINT win_set_flags( HWND hwnd, UINT set_mask, UINT clear_mask ) DECLSPEC_HIDDEN;
extern ULONG WIN_SetStyle( HWND hwnd, ULONG set_bits, ULONG clear_bits ) DECLSPEC_HIDDEN;
extern BOOL WIN_GetRectangles( HWND hwnd, enum coords_relative relative, RECT *rectWindow, RECT *rectClient ) DECLSPEC_HIDDEN;
extern void map_window_region( HWND from, HWND to, HRGN hrgn ) DECLSPEC_HIDDEN;
extern LRESULT WIN_DestroyWindow( HWND hwnd ) DECLSPEC_HIDDEN;
extern void destroy_thread_windows(void) DECLSPEC_HIDDEN;
extern HWND WIN_CreateWindowEx( CREATESTRUCTW *cs, LPCWSTR className, HINSTANCE module, BOOL unicode ) DECLSPEC_HIDDEN;
extern BOOL WIN_IsWindowDrawable( HWND hwnd, BOOL ) DECLSPEC_HIDDEN;
extern HWND *WIN_ListChildren( HWND hwnd ) DECLSPEC_HIDDEN;
extern LONG_PTR WIN_SetWindowLong( HWND hwnd, INT offset, UINT size, LONG_PTR newval, BOOL unicode ) DECLSPEC_HIDDEN;
extern void MDI_CalcDefaultChildPos( HWND hwndClient, INT total, LPPOINT lpPos, INT delta, UINT *id ) DECLSPEC_HIDDEN;
extern HDESK open_winstation_desktop( HWINSTA hwinsta, LPCWSTR name, DWORD flags, BOOL inherit, ACCESS_MASK access ) DECLSPEC_HIDDEN;

/* user lock */
extern void USER_Lock(void) DECLSPEC_HIDDEN;
extern void USER_Unlock(void) DECLSPEC_HIDDEN;

/* to release pointers retrieved by WIN_GetPtr */
static inline void WIN_ReleasePtr( WND *ptr )
{
    USER_Unlock();
}

extern LRESULT HOOK_CallHooks( INT id, INT code, WPARAM wparam, LPARAM lparam, BOOL unicode ) DECLSPEC_HIDDEN;

extern MINMAXINFO WINPOS_GetMinMaxInfo( HWND hwnd ) DECLSPEC_HIDDEN;
extern LONG WINPOS_HandleWindowPosChanging(HWND hwnd, WINDOWPOS *winpos) DECLSPEC_HIDDEN;
extern HWND WINPOS_WindowFromPoint( HWND hwndScope, POINT pt, INT *hittest ) DECLSPEC_HIDDEN;
extern void WINPOS_ActivateOtherWindow( HWND hwnd ) DECLSPEC_HIDDEN;
extern UINT WINPOS_MinMaximize( HWND hwnd, UINT cmd, LPRECT rect ) DECLSPEC_HIDDEN;
extern void WINPOS_SysCommandSizeMove( HWND hwnd, WPARAM wParam ) DECLSPEC_HIDDEN;

extern UINT get_monitor_dpi( HMONITOR monitor ) DECLSPEC_HIDDEN;
extern UINT get_win_monitor_dpi( HWND hwnd ) DECLSPEC_HIDDEN;
extern UINT get_thread_dpi(void) DECLSPEC_HIDDEN;
extern POINT map_dpi_point( POINT pt, UINT dpi_from, UINT dpi_to ) DECLSPEC_HIDDEN;
extern POINT point_win_to_phys_dpi( HWND hwnd, POINT pt ) DECLSPEC_HIDDEN;
extern POINT point_phys_to_win_dpi( HWND hwnd, POINT pt ) DECLSPEC_HIDDEN;
extern POINT point_win_to_thread_dpi( HWND hwnd, POINT pt ) DECLSPEC_HIDDEN;
extern POINT point_thread_to_win_dpi( HWND hwnd, POINT pt ) DECLSPEC_HIDDEN;
extern RECT map_dpi_rect( RECT rect, UINT dpi_from, UINT dpi_to ) DECLSPEC_HIDDEN;
extern RECT rect_win_to_thread_dpi( HWND hwnd, RECT rect ) DECLSPEC_HIDDEN;
extern RECT rect_thread_to_win_dpi( HWND hwnd, RECT rect ) DECLSPEC_HIDDEN;

extern BOOL set_window_pos( HWND hwnd, HWND insert_after, UINT swp_flags,
                            const RECT *window_rect, const RECT *client_rect,
                            const RECT *valid_rects ) DECLSPEC_HIDDEN;

static inline void mirror_rect( const RECT *window_rect, RECT *rect )
{
    int width = window_rect->right - window_rect->left;
    int tmp = rect->left;
    rect->left = width - rect->right;
    rect->right = width - tmp;
}

static inline UINT win_get_flags( HWND hwnd )
{
    return win_set_flags( hwnd, 0, 0 );
}

#endif  /* __WINE_WIN_H */
