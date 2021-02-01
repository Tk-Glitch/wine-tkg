/*
 * MACDRV windowing driver
 *
 * Copyright 1993, 1994, 1995, 1996, 2001 Alexandre Julliard
 * Copyright 1993 David Metcalfe
 * Copyright 1995, 1996 Alex Korobka
 * Copyright 2011, 2012, 2013 Ken Thomases for CodeWeavers Inc.
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

#include "config.h"

#include <IOKit/pwr_mgt/IOPMLib.h>
#define GetCurrentThread Mac_GetCurrentThread
#define LoadResource Mac_LoadResource
#include <CoreServices/CoreServices.h>
#undef GetCurrentThread
#undef LoadResource

#include "macdrv.h"
#include "winuser.h"
#include "wine/unicode.h"
#include "wine/server.h"

WINE_DEFAULT_DEBUG_CHANNEL(macdrv);


static CRITICAL_SECTION win_data_section;
static CRITICAL_SECTION_DEBUG critsect_debug =
{
    0, 0, &win_data_section,
    { &critsect_debug.ProcessLocksList, &critsect_debug.ProcessLocksList },
      0, 0, { (DWORD_PTR)(__FILE__ ": win_data_section") }
};
static CRITICAL_SECTION win_data_section = { &critsect_debug, -1, 0, 0, 0, 0 };

static CFMutableDictionaryRef win_datas;

static DWORD activate_on_focus_time;


/***********************************************************************
 *              get_cocoa_window_features
 */
static void get_cocoa_window_features(struct macdrv_win_data *data,
                                      DWORD style, DWORD ex_style,
                                      struct macdrv_window_features* wf,
                                      const RECT *window_rect,
                                      const RECT *client_rect)
{
    memset(wf, 0, sizeof(*wf));

    if (disable_window_decorations) return;
    if (IsRectEmpty(window_rect)) return;
    if (EqualRect(window_rect, client_rect)) return;

    if ((style & WS_CAPTION) == WS_CAPTION && !(ex_style & WS_EX_LAYERED))
    {
        wf->shadow = TRUE;
        if (!data->shaped)
        {
            wf->title_bar = TRUE;
            if (style & WS_SYSMENU) wf->close_button = TRUE;
            if (style & WS_MINIMIZEBOX) wf->minimize_button = TRUE;
            if (style & WS_MAXIMIZEBOX) wf->maximize_button = TRUE;
            if (ex_style & WS_EX_TOOLWINDOW) wf->utility = TRUE;
        }
    }
    if (style & WS_THICKFRAME)
    {
        wf->shadow = TRUE;
        if (!data->shaped) wf->resizable = TRUE;
    }
    else if (ex_style & WS_EX_DLGMODALFRAME) wf->shadow = TRUE;
    else if ((style & (WS_DLGFRAME|WS_BORDER)) == WS_DLGFRAME) wf->shadow = TRUE;
}


/*******************************************************************
 *              can_activate_window
 *
 * Check if we can activate the specified window.
 */
static inline BOOL can_activate_window(HWND hwnd)
{
    LONG style = GetWindowLongW(hwnd, GWL_STYLE);

    if (!(style & WS_VISIBLE)) return FALSE;
    if ((style & (WS_POPUP|WS_CHILD)) == WS_CHILD) return FALSE;
    if (GetWindowLongW(hwnd, GWL_EXSTYLE) & WS_EX_NOACTIVATE) return FALSE;
    if (hwnd == GetDesktopWindow()) return FALSE;
    return !(style & WS_DISABLED);
}


/***********************************************************************
 *              get_cocoa_window_state
 */
static void get_cocoa_window_state(struct macdrv_win_data *data,
                                   DWORD style, DWORD ex_style,
                                   struct macdrv_window_state* state)
{
    memset(state, 0, sizeof(*state));
    state->disabled = (style & WS_DISABLED) != 0;
    state->no_activate = !can_activate_window(data->hwnd);
    state->floating = (ex_style & WS_EX_TOPMOST) != 0;
    state->excluded_by_expose = state->excluded_by_cycle =
        (!(ex_style & WS_EX_APPWINDOW) &&
         (GetWindow(data->hwnd, GW_OWNER) || (ex_style & (WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE))));
    if (IsRectEmpty(&data->window_rect))
        state->excluded_by_expose = TRUE;
    state->minimized = (style & WS_MINIMIZE) != 0;
    state->minimized_valid = state->minimized != data->minimized;
    state->maximized = (style & WS_MAXIMIZE) != 0;
}


/***********************************************************************
 *              get_mac_rect_offset
 *
 * Helper for macdrv_window_to_mac_rect and macdrv_mac_to_window_rect.
 */
static void get_mac_rect_offset(struct macdrv_win_data *data, DWORD style, RECT *rect,
                                const RECT *window_rect, const RECT *client_rect)
{
    DWORD ex_style, style_mask = 0, ex_style_mask = 0;

    rect->top = rect->bottom = rect->left = rect->right = 0;

    ex_style = GetWindowLongW(data->hwnd, GWL_EXSTYLE);

    if (!data->shaped)
    {
        struct macdrv_window_features wf;
        get_cocoa_window_features(data, style, ex_style, &wf, window_rect, client_rect);

        if (wf.title_bar)
        {
            style_mask |= WS_CAPTION;
            ex_style_mask |= WS_EX_TOOLWINDOW;
        }
        if (wf.shadow)
        {
            style_mask |= WS_DLGFRAME | WS_THICKFRAME;
            ex_style_mask |= WS_EX_DLGMODALFRAME;
        }
    }

    AdjustWindowRectEx(rect, style & style_mask, FALSE, ex_style & ex_style_mask);

    TRACE("%p/%p style %08x ex_style %08x shaped %d -> %s\n", data->hwnd, data->cocoa_window,
          style, ex_style, data->shaped, wine_dbgstr_rect(rect));
}


/***********************************************************************
 *              macdrv_window_to_mac_rect
 *
 * Convert a rect from client to Mac window coordinates
 */
static void macdrv_window_to_mac_rect(struct macdrv_win_data *data, DWORD style, RECT *rect,
                                      const RECT *window_rect, const RECT *client_rect)
{
    RECT rc;

    if ((style & (WS_POPUP|WS_CHILD)) == WS_CHILD) return;
    if (IsRectEmpty(rect)) return;

    get_mac_rect_offset(data, style, &rc, window_rect, client_rect);

    rect->left   -= rc.left;
    rect->right  -= rc.right;
    rect->top    -= rc.top;
    rect->bottom -= rc.bottom;
    if (rect->top >= rect->bottom) rect->bottom = rect->top + 1;
    if (rect->left >= rect->right) rect->right = rect->left + 1;
}


/***********************************************************************
 *              macdrv_mac_to_window_rect
 *
 * Opposite of macdrv_window_to_mac_rect
 */
static void macdrv_mac_to_window_rect(struct macdrv_win_data *data, RECT *rect)
{
    RECT rc;
    DWORD style = GetWindowLongW(data->hwnd, GWL_STYLE);

    if ((style & (WS_POPUP|WS_CHILD)) == WS_CHILD) return;
    if (IsRectEmpty(rect)) return;

    get_mac_rect_offset(data, style, &rc, &data->window_rect, &data->client_rect);

    rect->left   += rc.left;
    rect->right  += rc.right;
    rect->top    += rc.top;
    rect->bottom += rc.bottom;
    if (rect->top >= rect->bottom) rect->bottom = rect->top + 1;
    if (rect->left >= rect->right) rect->right = rect->left + 1;
}


/***********************************************************************
 *              constrain_window_frame
 *
 * Alter a window frame rectangle to fit within a) Cocoa's documented
 * limits, and b) sane sizes, like twice the desktop rect.
 */
static void constrain_window_frame(CGRect* frame)
{
    CGRect desktop_rect = macdrv_get_desktop_rect();
    int max_width, max_height;

    max_width = min(32000, 2 * CGRectGetWidth(desktop_rect));
    max_height = min(32000, 2 * CGRectGetHeight(desktop_rect));

    if (frame->origin.x < -16000) frame->origin.x = -16000;
    if (frame->origin.y < -16000) frame->origin.y = -16000;
    if (frame->origin.x > 16000) frame->origin.x = 16000;
    if (frame->origin.y > 16000) frame->origin.y = 16000;
    if (frame->size.width > max_width) frame->size.width = max_width;
    if (frame->size.height > max_height) frame->size.height = max_height;
}


/***********************************************************************
 *              alloc_win_data
 */
static struct macdrv_win_data *alloc_win_data(HWND hwnd)
{
    struct macdrv_win_data *data;

    if ((data = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*data))))
    {
        data->hwnd = hwnd;
        data->color_key = CLR_INVALID;
        data->swap_interval = 1;
        EnterCriticalSection(&win_data_section);
        if (!win_datas)
            win_datas = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
        CFDictionarySetValue(win_datas, hwnd, data);
    }
    return data;
}


/***********************************************************************
 *              get_win_data
 *
 * Lock and return the data structure associated with a window.
 */
struct macdrv_win_data *get_win_data(HWND hwnd)
{
    struct macdrv_win_data *data;

    if (!hwnd) return NULL;
    EnterCriticalSection(&win_data_section);
    if (win_datas && (data = (struct macdrv_win_data*)CFDictionaryGetValue(win_datas, hwnd)))
        return data;
    LeaveCriticalSection(&win_data_section);
    return NULL;
}


/***********************************************************************
 *              release_win_data
 *
 * Release the data returned by get_win_data.
 */
void release_win_data(struct macdrv_win_data *data)
{
    if (data) LeaveCriticalSection(&win_data_section);
}


/***********************************************************************
 *              macdrv_get_cocoa_window
 *
 * Return the Mac window associated with the full area of a window
 */
macdrv_window macdrv_get_cocoa_window(HWND hwnd, BOOL require_on_screen)
{
    struct macdrv_win_data *data = get_win_data(hwnd);
    macdrv_window ret = NULL;
    if (data && (data->on_screen || !require_on_screen))
        ret = data->cocoa_window;
    release_win_data(data);
    return ret;
}


/***********************************************************************
 *              macdrv_get_cocoa_view
 *
 * Return the Cocoa view associated with a window
 */
macdrv_view macdrv_get_cocoa_view(HWND hwnd)
{
    struct macdrv_win_data *data = get_win_data(hwnd);
    macdrv_view ret = data ? data->cocoa_view : NULL;

    release_win_data(data);
    return ret;
}


/***********************************************************************
 *              macdrv_get_client_cocoa_view
 *
 * Return the Cocoa view associated with a window's client area
 */
macdrv_view macdrv_get_client_cocoa_view(HWND hwnd)
{
    struct macdrv_win_data *data = get_win_data(hwnd);
    macdrv_view ret = data ? data->client_cocoa_view : NULL;

    release_win_data(data);
    return ret;
}


/***********************************************************************
 *              set_cocoa_window_properties
 *
 * Set the window properties for a Cocoa window based on its Windows
 * properties.
 */
static void set_cocoa_window_properties(struct macdrv_win_data *data)
{
    DWORD style, ex_style;
    HWND owner;
    macdrv_window owner_win;
    struct macdrv_window_features wf;
    struct macdrv_window_state state;

    style = GetWindowLongW(data->hwnd, GWL_STYLE);
    ex_style = GetWindowLongW(data->hwnd, GWL_EXSTYLE);

    owner = GetWindow(data->hwnd, GW_OWNER);
    if (owner)
        owner = GetAncestor(owner, GA_ROOT);
    owner_win = macdrv_get_cocoa_window(owner, TRUE);
    macdrv_set_cocoa_parent_window(data->cocoa_window, owner_win);

    get_cocoa_window_features(data, style, ex_style, &wf, &data->window_rect, &data->client_rect);
    macdrv_set_cocoa_window_features(data->cocoa_window, &wf);

    get_cocoa_window_state(data, style, ex_style, &state);
    macdrv_set_cocoa_window_state(data->cocoa_window, &state);
    if (state.minimized_valid)
        data->minimized = state.minimized;
}


/***********************************************************************
 *              sync_window_region
 *
 * Update the window region.
 */
static void sync_window_region(struct macdrv_win_data *data, HRGN win_region)
{
    HRGN hrgn = win_region;
    RGNDATA *region_data;
    const CGRect* rects;
    int count;

    if (!data->cocoa_window) return;
    data->shaped = FALSE;

    if (IsRectEmpty(&data->window_rect))  /* set an empty shape */
    {
        TRACE("win %p/%p setting empty shape for zero-sized window\n", data->hwnd, data->cocoa_window);
        macdrv_set_window_shape(data->cocoa_window, &CGRectZero, 1);
        return;
    }

    if (hrgn == (HRGN)1)  /* hack: win_region == 1 means retrieve region from server */
    {
        if (!(hrgn = CreateRectRgn(0, 0, 0, 0))) return;
        if (GetWindowRgn(data->hwnd, hrgn) == ERROR)
        {
            DeleteObject(hrgn);
            hrgn = 0;
        }
    }

    if (hrgn && GetWindowLongW(data->hwnd, GWL_EXSTYLE) & WS_EX_LAYOUTRTL)
        MirrorRgn(data->hwnd, hrgn);
    if (hrgn)
    {
        OffsetRgn(hrgn, data->window_rect.left - data->whole_rect.left,
                  data->window_rect.top - data->whole_rect.top);
    }
    region_data = get_region_data(hrgn, 0);
    if (region_data)
    {
        rects = (CGRect*)region_data->Buffer;
        count = region_data->rdh.nCount;
        /* Special case optimization.  If the region entirely encloses the Cocoa
           window, it's the same as there being no region.  It's potentially
           hard/slow to test this for arbitrary regions, so we just check for
           very simple regions. */
        if (count == 1 && CGRectContainsRect(rects[0],
            CGRectOffset(cgrect_from_rect(data->whole_rect), -data->whole_rect.left, -data->whole_rect.top)))
        {
            TRACE("optimizing for simple region that contains Cocoa content rect\n");
            rects = NULL;
            count = 0;
        }
    }
    else
    {
        rects = NULL;
        count = 0;
    }

    TRACE("win %p/%p win_region %p rects %p count %d\n", data->hwnd, data->cocoa_window, win_region, rects, count);
    macdrv_set_window_shape(data->cocoa_window, rects, count);

    HeapFree(GetProcessHeap(), 0, region_data);
    data->shaped = (region_data != NULL);

    if (hrgn && hrgn != win_region) DeleteObject(hrgn);
}


/***********************************************************************
 *              add_bounds_rect
 */
static inline void add_bounds_rect(RECT *bounds, const RECT *rect)
{
    if (rect->left >= rect->right || rect->top >= rect->bottom) return;
    bounds->left   = min(bounds->left, rect->left);
    bounds->top    = min(bounds->top, rect->top);
    bounds->right  = max(bounds->right, rect->right);
    bounds->bottom = max(bounds->bottom, rect->bottom);
}


/***********************************************************************
 *              sync_window_opacity
 */
static void sync_window_opacity(struct macdrv_win_data *data, COLORREF key, BYTE alpha,
                                BOOL per_pixel_alpha, DWORD flags)
{
    CGFloat opacity = 1.0;
    BOOL needs_flush = FALSE;

    if (flags & LWA_ALPHA) opacity = alpha / 255.0;

    TRACE("setting window %p/%p alpha to %g\n", data->hwnd, data->cocoa_window, opacity);
    macdrv_set_window_alpha(data->cocoa_window, opacity);

    if (flags & LWA_COLORKEY)
    {
        /* FIXME: treat PALETTEINDEX and DIBINDEX as black */
        if ((key & (1 << 24)) || key >> 16 == 0x10ff)
            key = RGB(0, 0, 0);
    }
    else
        key = CLR_INVALID;

    if (data->color_key != key)
    {
        if (key == CLR_INVALID)
        {
            TRACE("clearing color-key for window %p/%p\n", data->hwnd, data->cocoa_window);
            macdrv_clear_window_color_key(data->cocoa_window);
        }
        else
        {
            TRACE("setting color-key for window %p/%p to RGB %d,%d,%d\n", data->hwnd, data->cocoa_window,
                  GetRValue(key), GetGValue(key), GetBValue(key));
            macdrv_set_window_color_key(data->cocoa_window, GetRValue(key), GetGValue(key), GetBValue(key));
        }

        data->color_key = key;
        needs_flush = TRUE;
    }

    if (!data->per_pixel_alpha != !per_pixel_alpha)
    {
        TRACE("setting window %p/%p per-pixel-alpha to %d\n", data->hwnd, data->cocoa_window, per_pixel_alpha);
        macdrv_window_use_per_pixel_alpha(data->cocoa_window, per_pixel_alpha);
        data->per_pixel_alpha = per_pixel_alpha;
        needs_flush = TRUE;
    }

    if (needs_flush && data->surface)
    {
        RECT *bounds;
        RECT rect;

        rect = data->whole_rect;
        OffsetRect(&rect, -data->whole_rect.left, -data->whole_rect.top);
        data->surface->funcs->lock(data->surface);
        bounds = data->surface->funcs->get_bounds(data->surface);
        add_bounds_rect(bounds, &rect);
        data->surface->funcs->unlock(data->surface);
    }
}


/***********************************************************************
 *              sync_window_min_max_info
 */
static void sync_window_min_max_info(HWND hwnd)
{
    LONG style = GetWindowLongW(hwnd, GWL_STYLE);
    LONG exstyle = GetWindowLongW(hwnd, GWL_EXSTYLE);
    RECT win_rect, primary_monitor_rect;
    MINMAXINFO minmax;
    LONG adjustedStyle;
    INT xinc, yinc;
    WINDOWPLACEMENT wpl;
    HMONITOR monitor;
    struct macdrv_win_data *data;

    TRACE("win %p\n", hwnd);

    if (!macdrv_get_cocoa_window(hwnd, FALSE)) return;

    GetWindowRect(hwnd, &win_rect);
    minmax.ptReserved.x = win_rect.left;
    minmax.ptReserved.y = win_rect.top;

    if ((style & WS_CAPTION) == WS_CAPTION)
        adjustedStyle = style & ~WS_BORDER; /* WS_CAPTION = WS_DLGFRAME | WS_BORDER */
    else
        adjustedStyle = style;

    primary_monitor_rect.left = primary_monitor_rect.top = 0;
    primary_monitor_rect.right = GetSystemMetrics(SM_CXSCREEN);
    primary_monitor_rect.bottom = GetSystemMetrics(SM_CYSCREEN);
    AdjustWindowRectEx(&primary_monitor_rect, adjustedStyle, ((style & WS_POPUP) && GetMenu(hwnd)), exstyle);

    xinc = -primary_monitor_rect.left;
    yinc = -primary_monitor_rect.top;

    minmax.ptMaxSize.x = primary_monitor_rect.right - primary_monitor_rect.left;
    minmax.ptMaxSize.y = primary_monitor_rect.bottom - primary_monitor_rect.top;
    minmax.ptMaxPosition.x = -xinc;
    minmax.ptMaxPosition.y = -yinc;
    if (style & (WS_DLGFRAME | WS_BORDER))
    {
        minmax.ptMinTrackSize.x = GetSystemMetrics(SM_CXMINTRACK);
        minmax.ptMinTrackSize.y = GetSystemMetrics(SM_CYMINTRACK);
    }
    else
    {
        minmax.ptMinTrackSize.x = 2 * xinc;
        minmax.ptMinTrackSize.y = 2 * yinc;
    }
    minmax.ptMaxTrackSize.x = GetSystemMetrics(SM_CXMAXTRACK);
    minmax.ptMaxTrackSize.y = GetSystemMetrics(SM_CYMAXTRACK);

    wpl.length = sizeof(wpl);
    if (GetWindowPlacement(hwnd, &wpl) && (wpl.ptMaxPosition.x != -1 || wpl.ptMaxPosition.y != -1))
    {
        minmax.ptMaxPosition = wpl.ptMaxPosition;

        /* Convert from GetWindowPlacement's workspace coordinates to screen coordinates. */
        minmax.ptMaxPosition.x -= wpl.rcNormalPosition.left - win_rect.left;
        minmax.ptMaxPosition.y -= wpl.rcNormalPosition.top - win_rect.top;
    }

    TRACE("initial ptMaxSize %s ptMaxPosition %s ptMinTrackSize %s ptMaxTrackSize %s\n", wine_dbgstr_point(&minmax.ptMaxSize),
          wine_dbgstr_point(&minmax.ptMaxPosition), wine_dbgstr_point(&minmax.ptMinTrackSize), wine_dbgstr_point(&minmax.ptMaxTrackSize));

    SendMessageW(hwnd, WM_GETMINMAXINFO, 0, (LPARAM)&minmax);

    TRACE("app's ptMaxSize %s ptMaxPosition %s ptMinTrackSize %s ptMaxTrackSize %s\n", wine_dbgstr_point(&minmax.ptMaxSize),
          wine_dbgstr_point(&minmax.ptMaxPosition), wine_dbgstr_point(&minmax.ptMinTrackSize), wine_dbgstr_point(&minmax.ptMaxTrackSize));

    /* if the app didn't change the values, adapt them for the window's monitor */
    if ((monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY)))
    {
        MONITORINFO mon_info;
        RECT monitor_rect;

        mon_info.cbSize = sizeof(mon_info);
        GetMonitorInfoW(monitor, &mon_info);

        if ((style & WS_MAXIMIZEBOX) && ((style & WS_CAPTION) == WS_CAPTION || !(style & WS_POPUP)))
            monitor_rect = mon_info.rcWork;
        else
            monitor_rect = mon_info.rcMonitor;

        if (minmax.ptMaxSize.x == primary_monitor_rect.right - primary_monitor_rect.left &&
            minmax.ptMaxSize.y == primary_monitor_rect.bottom - primary_monitor_rect.top)
        {
            minmax.ptMaxSize.x = (monitor_rect.right - monitor_rect.left) + 2 * xinc;
            minmax.ptMaxSize.y = (monitor_rect.bottom - monitor_rect.top) + 2 * yinc;
        }
        if (minmax.ptMaxPosition.x == -xinc && minmax.ptMaxPosition.y == -yinc)
        {
            minmax.ptMaxPosition.x = monitor_rect.left - xinc;
            minmax.ptMaxPosition.y = monitor_rect.top - yinc;
        }
    }

    minmax.ptMaxTrackSize.x = max(minmax.ptMaxTrackSize.x, minmax.ptMinTrackSize.x);
    minmax.ptMaxTrackSize.y = max(minmax.ptMaxTrackSize.y, minmax.ptMinTrackSize.y);

    TRACE("adjusted ptMaxSize %s ptMaxPosition %s ptMinTrackSize %s ptMaxTrackSize %s\n", wine_dbgstr_point(&minmax.ptMaxSize),
          wine_dbgstr_point(&minmax.ptMaxPosition), wine_dbgstr_point(&minmax.ptMinTrackSize), wine_dbgstr_point(&minmax.ptMaxTrackSize));

    if ((data = get_win_data(hwnd)) && data->cocoa_window)
    {
        RECT min_rect, max_rect;
        CGSize min_size, max_size;

        SetRect(&min_rect, 0, 0, minmax.ptMinTrackSize.x, minmax.ptMinTrackSize.y);
        macdrv_window_to_mac_rect(data, style, &min_rect, &data->window_rect, &data->client_rect);
        min_size = CGSizeMake(min_rect.right - min_rect.left, min_rect.bottom - min_rect.top);

        if (minmax.ptMaxTrackSize.x == GetSystemMetrics(SM_CXMAXTRACK) &&
            minmax.ptMaxTrackSize.y == GetSystemMetrics(SM_CYMAXTRACK))
            max_size = CGSizeMake(CGFLOAT_MAX, CGFLOAT_MAX);
        else
        {
            SetRect(&max_rect, 0, 0, minmax.ptMaxTrackSize.x, minmax.ptMaxTrackSize.y);
            macdrv_window_to_mac_rect(data, style, &max_rect, &data->window_rect, &data->client_rect);
            max_size = CGSizeMake(max_rect.right - max_rect.left, max_rect.bottom - max_rect.top);
        }

        TRACE("min_size (%g,%g) max_size (%g,%g)\n", min_size.width, min_size.height, max_size.width, max_size.height);
        macdrv_set_window_min_max_sizes(data->cocoa_window, min_size, max_size);
    }

    release_win_data(data);
}


/**********************************************************************
 *              create_client_cocoa_view
 *
 * Create the Cocoa view for a window's client area
 */
static void create_client_cocoa_view(struct macdrv_win_data *data)
{
    RECT rect = data->client_rect;
    OffsetRect(&rect, -data->whole_rect.left, -data->whole_rect.top);

    if (data->client_cocoa_view)
        macdrv_set_view_frame(data->client_cocoa_view, cgrect_from_rect(rect));
    else
    {
        data->client_cocoa_view = macdrv_create_view(cgrect_from_rect(rect));
        macdrv_set_view_hidden(data->client_cocoa_view, FALSE);
    }
    macdrv_set_view_superview(data->client_cocoa_view, data->cocoa_view, data->cocoa_window, NULL, NULL);
}


/**********************************************************************
 *              create_cocoa_window
 *
 * Create the whole Mac window for a given window
 */
static void create_cocoa_window(struct macdrv_win_data *data)
{
    struct macdrv_thread_data *thread_data = macdrv_init_thread_data();
    WCHAR text[1024];
    struct macdrv_window_features wf;
    CGRect frame;
    DWORD style, ex_style;
    HRGN win_rgn;
    COLORREF key;
    BYTE alpha;
    DWORD layered_flags;

    if ((win_rgn = CreateRectRgn(0, 0, 0, 0)) &&
        GetWindowRgn(data->hwnd, win_rgn) == ERROR)
    {
        DeleteObject(win_rgn);
        win_rgn = 0;
    }
    data->shaped = (win_rgn != 0);

    style = GetWindowLongW(data->hwnd, GWL_STYLE);
    ex_style = GetWindowLongW(data->hwnd, GWL_EXSTYLE);

    data->whole_rect = data->window_rect;
    macdrv_window_to_mac_rect(data, style, &data->whole_rect, &data->window_rect, &data->client_rect);

    get_cocoa_window_features(data, style, ex_style, &wf, &data->window_rect, &data->client_rect);

    frame = cgrect_from_rect(data->whole_rect);
    constrain_window_frame(&frame);
    if (frame.size.width < 1 || frame.size.height < 1)
        frame.size.width = frame.size.height = 1;

    TRACE("creating %p window %s whole %s client %s\n", data->hwnd, wine_dbgstr_rect(&data->window_rect),
          wine_dbgstr_rect(&data->whole_rect), wine_dbgstr_rect(&data->client_rect));

    data->cocoa_window = macdrv_create_cocoa_window(&wf, frame, data->hwnd, thread_data->queue);
    if (!data->cocoa_window) goto done;
    create_client_cocoa_view(data);

    set_cocoa_window_properties(data);

    /* set the window text */
    if (!InternalGetWindowText(data->hwnd, text, ARRAY_SIZE(text))) text[0] = 0;
    macdrv_set_cocoa_window_title(data->cocoa_window, text, strlenW(text));

    /* set the window region */
    if (win_rgn || IsRectEmpty(&data->window_rect)) sync_window_region(data, win_rgn);

    /* set the window opacity */
    if (!GetLayeredWindowAttributes(data->hwnd, &key, &alpha, &layered_flags)) layered_flags = 0;
    sync_window_opacity(data, key, alpha, FALSE, layered_flags);

done:
    if (win_rgn) DeleteObject(win_rgn);
}


/**********************************************************************
 *              destroy_cocoa_window
 *
 * Destroy the whole Mac window for a given window.
 */
static void destroy_cocoa_window(struct macdrv_win_data *data)
{
    if (!data->cocoa_window) return;

    TRACE("win %p Cocoa win %p\n", data->hwnd, data->cocoa_window);

    macdrv_destroy_cocoa_window(data->cocoa_window);
    data->cocoa_window = 0;
    data->on_screen = FALSE;
    data->color_key = CLR_INVALID;
    if (data->surface) window_surface_release(data->surface);
    data->surface = NULL;
    if (data->unminimized_surface) window_surface_release(data->unminimized_surface);
    data->unminimized_surface = NULL;
}


/**********************************************************************
 *              create_cocoa_view
 *
 * Create the Cocoa view for a given Windows child window
 */
static void create_cocoa_view(struct macdrv_win_data *data)
{
    BOOL equal = EqualRect(&data->window_rect, &data->client_rect);
    CGRect frame = cgrect_from_rect(data->window_rect);

    data->shaped = FALSE;
    data->whole_rect = data->window_rect;

    TRACE("creating %p window %s whole %s client %s\n", data->hwnd, wine_dbgstr_rect(&data->window_rect),
          wine_dbgstr_rect(&data->whole_rect), wine_dbgstr_rect(&data->client_rect));

    if (!equal)
        data->cocoa_view = macdrv_create_view(frame);
    create_client_cocoa_view(data);
    if (equal)
    {
        data->cocoa_view = data->client_cocoa_view;
        macdrv_set_view_hidden(data->cocoa_view, TRUE);
        macdrv_set_view_frame(data->cocoa_view, frame);
    }
}


/**********************************************************************
 *              destroy_cocoa_view
 *
 * Destroy the Cocoa view for a given window.
 */
static void destroy_cocoa_view(struct macdrv_win_data *data)
{
    if (!data->cocoa_view) return;

    TRACE("win %p Cocoa view %p\n", data->hwnd, data->cocoa_view);

    if (data->cocoa_view != data->client_cocoa_view)
        macdrv_dispose_view(data->cocoa_view);
    data->cocoa_view = NULL;
    data->on_screen = FALSE;
}


/***********************************************************************
 *              set_cocoa_view_parent
 */
static void set_cocoa_view_parent(struct macdrv_win_data *data, HWND parent)
{
    struct macdrv_win_data *parent_data = get_win_data(parent);
    macdrv_window cocoa_window = parent_data ? parent_data->cocoa_window : NULL;
    macdrv_view superview = parent_data ? parent_data->client_cocoa_view : NULL;

    TRACE("win %p/%p parent %p/%p\n", data->hwnd, data->cocoa_view, parent, cocoa_window ? (void*)cocoa_window : (void*)superview);

    if (!cocoa_window && !superview)
        WARN("hwnd %p new parent %p has no Cocoa window or view in this process\n", data->hwnd, parent);

    macdrv_set_view_superview(data->cocoa_view, superview, cocoa_window, NULL, NULL);
    release_win_data(parent_data);
}


/***********************************************************************
 *              macdrv_create_win_data
 *
 * Create a Mac data window structure for an existing window.
 */
static struct macdrv_win_data *macdrv_create_win_data(HWND hwnd, const RECT *window_rect,
                                                      const RECT *client_rect)
{
    struct macdrv_win_data *data;
    HWND parent;

    if (GetWindowThreadProcessId(hwnd, NULL) != GetCurrentThreadId()) return NULL;

    if (!(parent = GetAncestor(hwnd, GA_PARENT)))  /* desktop */
    {
        macdrv_init_thread_data();
        return NULL;
    }

    /* don't create win data for HWND_MESSAGE windows */
    if (parent != GetDesktopWindow() && !GetAncestor(parent, GA_PARENT)) return NULL;

    if (!(data = alloc_win_data(hwnd))) return NULL;

    data->whole_rect = data->window_rect = *window_rect;
    data->client_rect = *client_rect;

    if (parent == GetDesktopWindow())
    {
        create_cocoa_window(data);
        TRACE("win %p/%p window %s whole %s client %s\n",
               hwnd, data->cocoa_window, wine_dbgstr_rect(&data->window_rect),
               wine_dbgstr_rect(&data->whole_rect), wine_dbgstr_rect(&data->client_rect));
    }
    else
    {
        create_cocoa_view(data);
        TRACE("win %p/%p window %s whole %s client %s\n",
               hwnd, data->cocoa_view, wine_dbgstr_rect(&data->window_rect),
               wine_dbgstr_rect(&data->whole_rect), wine_dbgstr_rect(&data->client_rect));

        set_cocoa_view_parent(data, parent);
    }

    return data;
}


/**********************************************************************
 *              is_owned_by
 */
static BOOL is_owned_by(HWND hwnd, HWND maybe_owner)
{
    while (1)
    {
        HWND hwnd2 = GetWindow(hwnd, GW_OWNER);
        if (!hwnd2)
            hwnd2 = GetAncestor(hwnd, GA_ROOT);
        if (!hwnd2 || hwnd2 == hwnd)
            break;
        if (hwnd2 == maybe_owner)
            return TRUE;
        hwnd = hwnd2;
    }

    return FALSE;
}


/**********************************************************************
 *              is_all_the_way_front
 */
static BOOL is_all_the_way_front(HWND hwnd)
{
    BOOL topmost = (GetWindowLongW(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
    HWND prev = hwnd;

    while ((prev = GetWindow(prev, GW_HWNDPREV)))
    {
        if (!topmost && (GetWindowLongW(prev, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0)
            return TRUE;
        if (!is_owned_by(prev, hwnd))
            return FALSE;
    }

    return TRUE;
}


/***********************************************************************
 *              set_focus
 */
static void set_focus(HWND hwnd, BOOL raise)
{
    struct macdrv_win_data *data;

    if (!(hwnd = GetAncestor(hwnd, GA_ROOT))) return;

    if (raise && hwnd == GetForegroundWindow() && hwnd != GetDesktopWindow() && !is_all_the_way_front(hwnd))
        SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);

    if (!(data = get_win_data(hwnd))) return;

    if (data->cocoa_window && data->on_screen)
    {
        BOOL activate = activate_on_focus_time && (GetTickCount() - activate_on_focus_time < 2000);
        /* Set Mac focus */
        macdrv_give_cocoa_window_focus(data->cocoa_window, activate);
        activate_on_focus_time = 0;
    }

    release_win_data(data);
}

/***********************************************************************
 *              show_window
 */
static void show_window(struct macdrv_win_data *data)
{
    if (data->cocoa_window)
    {
        HWND prev = NULL;
        HWND next = NULL;
        macdrv_window prev_window = NULL;
        macdrv_window next_window = NULL;
        BOOL activate = FALSE;
        GUITHREADINFO info;

        /* find window that this one must be after */
        prev = GetWindow(data->hwnd, GW_HWNDPREV);
        while (prev && !((GetWindowLongW(prev, GWL_STYLE) & (WS_VISIBLE | WS_MINIMIZE)) == WS_VISIBLE &&
                         (prev_window = macdrv_get_cocoa_window(prev, TRUE))))
            prev = GetWindow(prev, GW_HWNDPREV);
        if (!prev_window)
        {
            /* find window that this one must be before */
            next = GetWindow(data->hwnd, GW_HWNDNEXT);
            while (next && !((GetWindowLongW(next, GWL_STYLE) & (WS_VISIBLE | WS_MINIMIZE)) == WS_VISIBLE &&
                             (next_window = macdrv_get_cocoa_window(next, TRUE))))
                next = GetWindow(next, GW_HWNDNEXT);
        }

        TRACE("win %p/%p below %p/%p above %p/%p\n",
              data->hwnd, data->cocoa_window, prev, prev_window, next, next_window);

        if (!prev_window)
            activate = activate_on_focus_time && (GetTickCount() - activate_on_focus_time < 2000);
        macdrv_order_cocoa_window(data->cocoa_window, prev_window, next_window, activate);
        data->on_screen = TRUE;

        info.cbSize = sizeof(info);
        if (GetGUIThreadInfo(GetWindowThreadProcessId(data->hwnd, NULL), &info) && info.hwndFocus &&
            (data->hwnd == info.hwndFocus || IsChild(data->hwnd, info.hwndFocus)))
            set_focus(info.hwndFocus, FALSE);
        if (activate)
            activate_on_focus_time = 0;
    }
    else
    {
        TRACE("win %p/%p showing view\n", data->hwnd, data->cocoa_view);

        macdrv_set_view_hidden(data->cocoa_view, FALSE);
        data->on_screen = TRUE;
    }
}


/***********************************************************************
 *              hide_window
 */
static void hide_window(struct macdrv_win_data *data)
{
    TRACE("win %p/%p\n", data->hwnd, data->cocoa_window);

    if (data->cocoa_window)
        macdrv_hide_cocoa_window(data->cocoa_window);
    else
        macdrv_set_view_hidden(data->cocoa_view, TRUE);
    data->on_screen = FALSE;
}


/***********************************************************************
 *              sync_window_z_order
 */
static void sync_window_z_order(struct macdrv_win_data *data)
{
    if (data->cocoa_view)
    {
        HWND parent = GetAncestor(data->hwnd, GA_PARENT);
        macdrv_view superview = macdrv_get_client_cocoa_view(parent);
        macdrv_window window = NULL;
        HWND prev;
        HWND next = NULL;
        macdrv_view prev_view = NULL;
        macdrv_view next_view = NULL;

        if (!superview)
        {
            window = macdrv_get_cocoa_window(parent, FALSE);
            if (!window)
                WARN("hwnd %p/%p parent %p has no Cocoa window or view in this process\n", data->hwnd, data->cocoa_view, parent);
        }

        /* find window that this one must be after */
        prev = GetWindow(data->hwnd, GW_HWNDPREV);
        while (prev && !(prev_view = macdrv_get_cocoa_view(prev)))
            prev = GetWindow(prev, GW_HWNDPREV);
        if (!prev_view)
        {
            /* find window that this one must be before */
            next = GetWindow(data->hwnd, GW_HWNDNEXT);
            while (next && !(next_view = macdrv_get_cocoa_view(next)))
                next = GetWindow(next, GW_HWNDNEXT);
        }

        TRACE("win %p/%p below %p/%p above %p/%p\n",
              data->hwnd, data->cocoa_view, prev, prev_view, next, next_view);

        macdrv_set_view_superview(data->cocoa_view, superview, window, prev_view, next_view);
    }
    else if (data->on_screen)
        show_window(data);
}


/***********************************************************************
 *              get_region_data
 *
 * Calls GetRegionData on the given region and converts the rectangle
 * array to CGRect format. The returned buffer must be freed by
 * caller using HeapFree(GetProcessHeap(),...).
 * If hdc_lptodp is not 0, the rectangles are converted through LPtoDP.
 */
RGNDATA *get_region_data(HRGN hrgn, HDC hdc_lptodp)
{
    RGNDATA *data;
    DWORD size;
    int i;
    RECT *rect;
    CGRect *cgrect;

    if (!hrgn || !(size = GetRegionData(hrgn, 0, NULL))) return NULL;
    if (sizeof(CGRect) > sizeof(RECT))
    {
        /* add extra size for CGRect array */
        int count = (size - sizeof(RGNDATAHEADER)) / sizeof(RECT);
        size += count * (sizeof(CGRect) - sizeof(RECT));
    }
    if (!(data = HeapAlloc(GetProcessHeap(), 0, size))) return NULL;
    if (!GetRegionData(hrgn, size, data))
    {
        HeapFree(GetProcessHeap(), 0, data);
        return NULL;
    }

    rect = (RECT *)data->Buffer;
    cgrect = (CGRect *)data->Buffer;
    if (hdc_lptodp)  /* map to device coordinates */
    {
        LPtoDP(hdc_lptodp, (POINT *)rect, data->rdh.nCount * 2);
        for (i = 0; i < data->rdh.nCount; i++)
        {
            if (rect[i].right < rect[i].left)
            {
                INT tmp = rect[i].right;
                rect[i].right = rect[i].left;
                rect[i].left = tmp;
            }
            if (rect[i].bottom < rect[i].top)
            {
                INT tmp = rect[i].bottom;
                rect[i].bottom = rect[i].top;
                rect[i].top = tmp;
            }
        }
    }

    if (sizeof(CGRect) > sizeof(RECT))
    {
        /* need to start from the end */
        for (i = data->rdh.nCount-1; i >= 0; i--)
            cgrect[i] = cgrect_from_rect(rect[i]);
    }
    else
    {
        for (i = 0; i < data->rdh.nCount; i++)
            cgrect[i] = cgrect_from_rect(rect[i]);
    }
    return data;
}


/***********************************************************************
 *              sync_client_view_position
 */
static void sync_client_view_position(struct macdrv_win_data *data)
{
    if (data->cocoa_view != data->client_cocoa_view)
    {
        RECT rect = data->client_rect;
        OffsetRect(&rect, -data->whole_rect.left, -data->whole_rect.top);
        macdrv_set_view_frame(data->client_cocoa_view, cgrect_from_rect(rect));
        TRACE("win %p/%p client %s\n", data->hwnd, data->client_cocoa_view, wine_dbgstr_rect(&rect));
    }
}


/***********************************************************************
 *              sync_window_position
 *
 * Synchronize the Mac window position with the Windows one
 */
static void sync_window_position(struct macdrv_win_data *data, UINT swp_flags, const RECT *old_window_rect,
                                 const RECT *old_whole_rect)
{
    CGRect frame = cgrect_from_rect(data->whole_rect);
    BOOL force_z_order = FALSE;

    if (data->cocoa_window)
    {
        if (data->minimized) return;

        constrain_window_frame(&frame);
        if (frame.size.width < 1 || frame.size.height < 1)
            frame.size.width = frame.size.height = 1;

        macdrv_set_cocoa_window_frame(data->cocoa_window, &frame);
    }
    else
    {
        BOOL were_equal = (data->cocoa_view == data->client_cocoa_view);
        BOOL now_equal = EqualRect(&data->whole_rect, &data->client_rect);

        if (were_equal && !now_equal)
        {
            data->cocoa_view = macdrv_create_view(frame);
            macdrv_set_view_hidden(data->cocoa_view, !data->on_screen);
            macdrv_set_view_superview(data->client_cocoa_view, data->cocoa_view, NULL, NULL, NULL);
            macdrv_set_view_hidden(data->client_cocoa_view, FALSE);
            force_z_order = TRUE;
        }
        else if (!were_equal && now_equal)
        {
            macdrv_dispose_view(data->cocoa_view);
            data->cocoa_view = data->client_cocoa_view;
            macdrv_set_view_hidden(data->cocoa_view, !data->on_screen);
            macdrv_set_view_frame(data->cocoa_view, frame);
            force_z_order = TRUE;
        }
        else if (!EqualRect(&data->whole_rect, old_whole_rect))
            macdrv_set_view_frame(data->cocoa_view, frame);
    }

    sync_client_view_position(data);

    if (old_window_rect && old_whole_rect &&
        (IsRectEmpty(old_window_rect) != IsRectEmpty(&data->window_rect) ||
         old_window_rect->left - old_whole_rect->left != data->window_rect.left - data->whole_rect.left ||
         old_window_rect->top - old_whole_rect->top != data->window_rect.top - data->whole_rect.top))
        sync_window_region(data, (HRGN)1);

    TRACE("win %p/%p whole_rect %s frame %s\n", data->hwnd,
          data->cocoa_window ? (void*)data->cocoa_window : (void*)data->cocoa_view,
          wine_dbgstr_rect(&data->whole_rect), wine_dbgstr_cgrect(frame));

    if (force_z_order || !(swp_flags & SWP_NOZORDER) || (swp_flags & SWP_SHOWWINDOW))
        sync_window_z_order(data);
}


/***********************************************************************
 *              move_window_bits
 *
 * Move the window bits when a window is moved.
 */
static void move_window_bits(HWND hwnd, macdrv_window window, const RECT *old_rect, const RECT *new_rect,
                             const RECT *old_client_rect, const RECT *new_client_rect,
                             const RECT *new_window_rect)
{
    RECT src_rect = *old_rect;
    RECT dst_rect = *new_rect;
    HDC hdc_src, hdc_dst;
    HRGN rgn;
    HWND parent = 0;

    if (!window)
    {
        OffsetRect(&dst_rect, -new_window_rect->left, -new_window_rect->top);
        parent = GetAncestor(hwnd, GA_PARENT);
        hdc_src = GetDCEx(parent, 0, DCX_CACHE);
        hdc_dst = GetDCEx(hwnd, 0, DCX_CACHE | DCX_WINDOW);
    }
    else
    {
        OffsetRect(&dst_rect, -new_client_rect->left, -new_client_rect->top);
        /* make src rect relative to the old position of the window */
        OffsetRect(&src_rect, -old_client_rect->left, -old_client_rect->top);
        if (dst_rect.left == src_rect.left && dst_rect.top == src_rect.top) return;
        hdc_src = hdc_dst = GetDCEx(hwnd, 0, DCX_CACHE);
    }

    rgn = CreateRectRgnIndirect(&dst_rect);
    SelectClipRgn(hdc_dst, rgn);
    DeleteObject(rgn);
    ExcludeUpdateRgn(hdc_dst, hwnd);

    TRACE("copying bits for win %p/%p %s -> %s\n", hwnd, window,
          wine_dbgstr_rect(&src_rect), wine_dbgstr_rect(&dst_rect));
    BitBlt(hdc_dst, dst_rect.left, dst_rect.top,
           dst_rect.right - dst_rect.left, dst_rect.bottom - dst_rect.top,
           hdc_src, src_rect.left, src_rect.top, SRCCOPY);

    ReleaseDC(hwnd, hdc_dst);
    if (hdc_src != hdc_dst) ReleaseDC(parent, hdc_src);
}


/**********************************************************************
 *              activate_on_following_focus
 */
void activate_on_following_focus(void)
{
    activate_on_focus_time = GetTickCount();
    if (!activate_on_focus_time) activate_on_focus_time = 1;
}


/***********************************************************************
 *              set_app_icon
 */
static void set_app_icon(void)
{
    CFArrayRef images = create_app_icon_images();
    if (images)
    {
        macdrv_set_application_icon(images);
        CFRelease(images);
    }
}


/**********************************************************************
 *		        set_capture_window_for_move
 */
static BOOL set_capture_window_for_move(HWND hwnd)
{
    HWND previous = 0;
    BOOL ret;

    SERVER_START_REQ(set_capture_window)
    {
        req->handle = wine_server_user_handle(hwnd);
        req->flags  = CAPTURE_MOVESIZE;
        if ((ret = !wine_server_call_err(req)))
        {
            previous = wine_server_ptr_handle(reply->previous);
            hwnd = wine_server_ptr_handle(reply->full_handle);
        }
    }
    SERVER_END_REQ;

    if (ret)
    {
        macdrv_SetCapture(hwnd, GUI_INMOVESIZE);

        if (previous && previous != hwnd)
            SendMessageW(previous, WM_CAPTURECHANGED, 0, (LPARAM)hwnd);
    }
    return ret;
}


/***********************************************************************
 *              move_window
 *
 * Based on user32's WINPOS_SysCommandSizeMove() specialized just for
 * moving top-level windows and enforcing Mac-style constraints like
 * keeping the top of the window within the work area.
 */
static LRESULT move_window(HWND hwnd, WPARAM wparam)
{
    MSG msg;
    RECT origRect, movedRect, desktopRect;
    LONG hittest = (LONG)(wparam & 0x0f);
    POINT capturePoint;
    LONG style = GetWindowLongW(hwnd, GWL_STYLE);
    BOOL moved = FALSE;
    DWORD dwPoint = GetMessagePos();
    INT captionHeight;
    HMONITOR mon = 0;
    MONITORINFO info;

    if ((style & (WS_MINIMIZE | WS_MAXIMIZE)) || !IsWindowVisible(hwnd)) return -1;
    if (hittest && hittest != HTCAPTION) return -1;

    capturePoint.x = (short)LOWORD(dwPoint);
    capturePoint.y = (short)HIWORD(dwPoint);
    ClipCursor(NULL);

    TRACE("hwnd %p hittest %d, pos %d,%d\n", hwnd, hittest, capturePoint.x, capturePoint.y);

    origRect.left = origRect.right = origRect.top = origRect.bottom = 0;
    if (AdjustWindowRectEx(&origRect, style, FALSE, GetWindowLongW(hwnd, GWL_EXSTYLE)))
        captionHeight = -origRect.top;
    else
        captionHeight = 0;

    GetWindowRect(hwnd, &origRect);
    movedRect = origRect;

    if (!hittest)
    {
        /* Move pointer to the center of the caption */
        RECT rect = origRect;

        /* Note: to be exactly centered we should take the different types
         * of border into account, but it shouldn't make more than a few pixels
         * of difference so let's not bother with that */
        rect.top += GetSystemMetrics(SM_CYBORDER);
        if (style & WS_SYSMENU)
            rect.left += GetSystemMetrics(SM_CXSIZE) + 1;
        if (style & WS_MINIMIZEBOX)
            rect.right -= GetSystemMetrics(SM_CXSIZE) + 1;
        if (style & WS_MAXIMIZEBOX)
            rect.right -= GetSystemMetrics(SM_CXSIZE) + 1;
        capturePoint.x = (rect.right + rect.left) / 2;
        capturePoint.y = rect.top + GetSystemMetrics(SM_CYSIZE)/2;

        SetCursorPos(capturePoint.x, capturePoint.y);
        SendMessageW(hwnd, WM_SETCURSOR, (WPARAM)hwnd, MAKELONG(HTCAPTION, WM_MOUSEMOVE));
    }

    desktopRect = rect_from_cgrect(macdrv_get_desktop_rect());
    mon = MonitorFromPoint(capturePoint, MONITOR_DEFAULTTONEAREST);
    info.cbSize = sizeof(info);
    if (mon && !GetMonitorInfoW(mon, &info))
        mon = 0;

    /* repaint the window before moving it around */
    RedrawWindow(hwnd, NULL, 0, RDW_UPDATENOW | RDW_ALLCHILDREN);

    SendMessageW(hwnd, WM_ENTERSIZEMOVE, 0, 0);
    set_capture_window_for_move(hwnd);

    while(1)
    {
        POINT pt;
        int dx = 0, dy = 0;
        HMONITOR newmon;

        if (!GetMessageW(&msg, 0, 0, 0)) break;
        if (CallMsgFilterW(&msg, MSGF_SIZE)) continue;

        /* Exit on button-up, Return, or Esc */
        if (msg.message == WM_LBUTTONUP ||
            (msg.message == WM_KEYDOWN && (msg.wParam == VK_RETURN || msg.wParam == VK_ESCAPE)))
            break;

        if (msg.message != WM_KEYDOWN && msg.message != WM_MOUSEMOVE)
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            continue;  /* We are not interested in other messages */
        }

        pt = msg.pt;

        if (msg.message == WM_KEYDOWN) switch(msg.wParam)
        {
        case VK_UP:    pt.y -= 8; break;
        case VK_DOWN:  pt.y += 8; break;
        case VK_LEFT:  pt.x -= 8; break;
        case VK_RIGHT: pt.x += 8; break;
        }

        pt.x = max(pt.x, desktopRect.left);
        pt.x = min(pt.x, desktopRect.right - 1);
        pt.y = max(pt.y, desktopRect.top);
        pt.y = min(pt.y, desktopRect.bottom - 1);

        if ((newmon = MonitorFromPoint(pt, MONITOR_DEFAULTTONULL)) && newmon != mon)
        {
            if (GetMonitorInfoW(newmon, &info))
                mon = newmon;
            else
                mon = 0;
        }

        if (mon)
        {
            /* wineserver clips the cursor position to the virtual desktop rect but,
               if the display configuration is non-rectangular, that could still
               leave the logical cursor position outside of any display.  The window
               could keep moving as you push the cursor against a display edge, even
               though the visible cursor doesn't keep moving. The following keeps
               the window movement in sync with the visible cursor. */
            pt.x = max(pt.x, info.rcMonitor.left);
            pt.x = min(pt.x, info.rcMonitor.right - 1);
            pt.y = max(pt.y, info.rcMonitor.top);
            pt.y = min(pt.y, info.rcMonitor.bottom - 1);

            /* Assuming that dx will be calculated below as pt.x - capturePoint.x,
               dy will be pt.y - capturePoint.y, and movedRect will be offset by those,
               we want to enforce these constraints:
                    movedRect.left + dx < info.rcWork.right
                    movedRect.right + dx > info.rcWork.left
                    movedRect.top + captionHeight + dy < info.rcWork.bottom
                    movedRect.bottom + dy > info.rcWork.top
                    movedRect.top + dy >= info.rcWork.top
               The first four keep at least one edge barely in the work area.
               The last keeps the top (i.e. the title bar) in the work area.
               The fourth is redundant with the last, so can be ignored.

               Substituting for dx and dy and rearranging gives us...
             */
            pt.x = min(pt.x, info.rcWork.right - 1 + capturePoint.x - movedRect.left);
            pt.x = max(pt.x, info.rcWork.left + 1 + capturePoint.x - movedRect.right);
            pt.y = min(pt.y, info.rcWork.bottom - 1 + capturePoint.y - movedRect.top - captionHeight);
            pt.y = max(pt.y, info.rcWork.top + capturePoint.y - movedRect.top);
        }

        dx = pt.x - capturePoint.x;
        dy = pt.y - capturePoint.y;

        if (dx || dy)
        {
            moved = TRUE;

            if (msg.message == WM_KEYDOWN) SetCursorPos(pt.x, pt.y);
            else
            {
                OffsetRect(&movedRect, dx, dy);
                capturePoint = pt;

                SendMessageW(hwnd, WM_MOVING, 0, (LPARAM)&movedRect);
                SetWindowPos(hwnd, 0, movedRect.left, movedRect.top, 0, 0,
                             SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER);
            }
        }
    }

    set_capture_window_for_move(0);

    SendMessageW(hwnd, WM_EXITSIZEMOVE, 0, 0);
    SendMessageW(hwnd, WM_SETVISIBLE, TRUE, 0L);

    /* if the move is canceled, restore the previous position */
    if (moved && msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE)
    {
        SetWindowPos(hwnd, 0, origRect.left, origRect.top, 0, 0,
                     SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER);
    }

    return 0;
}


/***********************************************************************
 *              perform_window_command
 */
static void perform_window_command(HWND hwnd, DWORD style_any, DWORD style_none, WORD command, WORD hittest)
{
    DWORD style;

    TRACE("win %p style_any 0x%08x style_none 0x%08x command 0x%04x hittest 0x%04x\n",
          hwnd, style_any, style_none, command, hittest);

    style = GetWindowLongW(hwnd, GWL_STYLE);
    if ((style_any && !(style & style_any)) || (style & (WS_DISABLED | style_none)))
    {
        TRACE("not changing win %p style 0x%08x\n", hwnd, style);
        return;
    }

    if (GetActiveWindow() != hwnd)
    {
        LRESULT ma = SendMessageW(hwnd, WM_MOUSEACTIVATE, (WPARAM)GetAncestor(hwnd, GA_ROOT),
                                  MAKELPARAM(hittest, WM_NCLBUTTONDOWN));
        switch (ma)
        {
            case MA_NOACTIVATEANDEAT:
            case MA_ACTIVATEANDEAT:
                TRACE("not changing win %p mouse-activate result %ld\n", hwnd, ma);
                return;
            case MA_NOACTIVATE:
                break;
            case MA_ACTIVATE:
            case 0:
                SetActiveWindow(hwnd);
                break;
            default:
                WARN("unknown WM_MOUSEACTIVATE code %ld\n", ma);
                break;
        }
    }

    TRACE("changing win %p\n", hwnd);
    PostMessageW(hwnd, WM_SYSCOMMAND, command, 0);
}


/**********************************************************************
 *              CreateDesktopWindow   (MACDRV.@)
 */
BOOL CDECL macdrv_CreateDesktopWindow(HWND hwnd)
{
    unsigned int width, height;

    TRACE("%p\n", hwnd);

    /* retrieve the real size of the desktop */
    SERVER_START_REQ(get_window_rectangles)
    {
        req->handle = wine_server_user_handle(hwnd);
        req->relative = COORDS_CLIENT;
        wine_server_call(req);
        width  = reply->window.right;
        height = reply->window.bottom;
    }
    SERVER_END_REQ;

    if (!width && !height)  /* not initialized yet */
    {
        CGRect rect = macdrv_get_desktop_rect();

        SERVER_START_REQ(set_window_pos)
        {
            req->handle        = wine_server_user_handle(hwnd);
            req->previous      = 0;
            req->swp_flags     = SWP_NOZORDER;
            req->window.left   = CGRectGetMinX(rect);
            req->window.top    = CGRectGetMinY(rect);
            req->window.right  = CGRectGetMaxX(rect);
            req->window.bottom = CGRectGetMaxY(rect);
            req->client        = req->window;
            wine_server_call(req);
        }
        SERVER_END_REQ;
    }

    set_app_icon();
    return TRUE;
}


static WNDPROC desktop_orig_wndproc;

#define WM_WINE_NOTIFY_ACTIVITY WM_USER

static LRESULT CALLBACK desktop_wndproc_wrapper( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    switch (msg)
    {
    case WM_WINE_NOTIFY_ACTIVITY:
    {
        /* This wakes from display sleep, but doesn't affect the screen saver. */
        static IOPMAssertionID assertion;
        IOPMAssertionDeclareUserActivity(CFSTR("Wine user input"), kIOPMUserActiveLocal, &assertion);

        /* This prevents the screen saver, but doesn't wake from display sleep. */
        /* It's deprecated, but there's no better alternative. */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        UpdateSystemActivity(UsrActivity);
#pragma clang diagnostic pop
        break;
    }
    }
    return desktop_orig_wndproc( hwnd, msg, wp, lp );
}

/**********************************************************************
 *              CreateWindow   (MACDRV.@)
 */
BOOL CDECL macdrv_CreateWindow(HWND hwnd)
{
    if (hwnd == GetDesktopWindow())
    {
        desktop_orig_wndproc = (WNDPROC)SetWindowLongPtrW( GetDesktopWindow(),
            GWLP_WNDPROC, (LONG_PTR)desktop_wndproc_wrapper );

        macdrv_init_clipboard();
    }
    return TRUE;
}


/***********************************************************************
 *              DestroyWindow   (MACDRV.@)
 */
void CDECL macdrv_DestroyWindow(HWND hwnd)
{
    struct macdrv_win_data *data;

    TRACE("%p\n", hwnd);

    if (!(data = get_win_data(hwnd))) return;

    if (hwnd == GetCapture()) macdrv_SetCapture(0, 0);
    if (data->drag_event) SetEvent(data->drag_event);

    destroy_cocoa_window(data);
    destroy_cocoa_view(data);
    if (data->client_cocoa_view) macdrv_dispose_view(data->client_cocoa_view);

    CFDictionaryRemoveValue(win_datas, hwnd);
    release_win_data(data);
    HeapFree(GetProcessHeap(), 0, data);
}


/*****************************************************************
 *              SetFocus   (MACDRV.@)
 *
 * Set the Mac focus.
 */
void CDECL macdrv_SetFocus(HWND hwnd)
{
    struct macdrv_thread_data *thread_data = macdrv_thread_data();

    TRACE("%p\n", hwnd);

    if (!thread_data) return;
    thread_data->dead_key_state = 0;
    set_focus(hwnd, TRUE);
}


/***********************************************************************
 *              SetLayeredWindowAttributes  (MACDRV.@)
 *
 * Set transparency attributes for a layered window.
 */
void CDECL macdrv_SetLayeredWindowAttributes(HWND hwnd, COLORREF key, BYTE alpha, DWORD flags)
{
    struct macdrv_win_data *data = get_win_data(hwnd);

    TRACE("hwnd %p key %#08x alpha %#02x flags %x\n", hwnd, key, alpha, flags);

    if (data)
    {
        data->layered = TRUE;
        data->ulw_layered = FALSE;
        if (data->surface) set_surface_use_alpha(data->surface, FALSE);
        if (data->cocoa_window)
        {
            sync_window_opacity(data, key, alpha, FALSE, flags);
            /* since layered attributes are now set, can now show the window */
            if ((GetWindowLongW(hwnd, GWL_STYLE) & WS_VISIBLE) && !data->on_screen)
                show_window(data);
        }
        release_win_data(data);
    }
    else
        FIXME("setting layered attributes on window %p of other process not supported\n", hwnd);
}


/*****************************************************************
 *              SetParent   (MACDRV.@)
 */
void CDECL macdrv_SetParent(HWND hwnd, HWND parent, HWND old_parent)
{
    struct macdrv_win_data *data;

    TRACE("%p, %p, %p\n", hwnd, parent, old_parent);

    if (parent == old_parent) return;
    if (!(data = get_win_data(hwnd))) return;

    if (parent != GetDesktopWindow()) /* a child window */
    {
        if (old_parent == GetDesktopWindow())
        {
            /* destroy the old Mac window */
            destroy_cocoa_window(data);
            create_cocoa_view(data);
        }

        set_cocoa_view_parent(data, parent);
    }
    else  /* new top level window */
    {
        destroy_cocoa_view(data);
        create_cocoa_window(data);
    }
    release_win_data(data);
}


/***********************************************************************
 *              SetWindowRgn  (MACDRV.@)
 *
 * Assign specified region to window (for non-rectangular windows)
 */
void CDECL macdrv_SetWindowRgn(HWND hwnd, HRGN hrgn, BOOL redraw)
{
    struct macdrv_win_data *data;

    TRACE("%p, %p, %d\n", hwnd, hrgn, redraw);

    if ((data = get_win_data(hwnd)))
    {
        sync_window_region(data, hrgn);
        release_win_data(data);
    }
    else
    {
        DWORD procid;

        GetWindowThreadProcessId(hwnd, &procid);
        if (procid != GetCurrentProcessId())
            SendMessageW(hwnd, WM_MACDRV_SET_WIN_REGION, 0, 0);
    }
}


/***********************************************************************
 *              SetWindowStyle   (MACDRV.@)
 *
 * Update the state of the Cocoa window to reflect a style change
 */
void CDECL macdrv_SetWindowStyle(HWND hwnd, INT offset, STYLESTRUCT *style)
{
    struct macdrv_win_data *data;

    TRACE("hwnd %p offset %d styleOld 0x%08x styleNew 0x%08x\n", hwnd, offset, style->styleOld, style->styleNew);

    if (hwnd == GetDesktopWindow()) return;
    if (!(data = get_win_data(hwnd))) return;

    if (data->cocoa_window)
    {
        DWORD changed = style->styleNew ^ style->styleOld;

        set_cocoa_window_properties(data);

        if (offset == GWL_EXSTYLE && (changed & WS_EX_LAYERED)) /* changing WS_EX_LAYERED resets attributes */
        {
            data->layered = FALSE;
            data->ulw_layered = FALSE;
            sync_window_opacity(data, 0, 0, FALSE, 0);
            if (data->surface) set_surface_use_alpha(data->surface, FALSE);
        }

        if (offset == GWL_EXSTYLE && (changed & WS_EX_LAYOUTRTL))
            sync_window_region(data, (HRGN)1);
    }

    release_win_data(data);
}


/*****************************************************************
 *              SetWindowText   (MACDRV.@)
 */
void CDECL macdrv_SetWindowText(HWND hwnd, LPCWSTR text)
{
    macdrv_window win;

    TRACE("%p, %s\n", hwnd, debugstr_w(text));

    if ((win = macdrv_get_cocoa_window(hwnd, FALSE)))
        macdrv_set_cocoa_window_title(win, text, strlenW(text));
}


/***********************************************************************
 *              ShowWindow   (MACDRV.@)
 */
UINT CDECL macdrv_ShowWindow(HWND hwnd, INT cmd, RECT *rect, UINT swp)
{
    struct macdrv_thread_data *thread_data = macdrv_thread_data();
    struct macdrv_win_data *data = get_win_data(hwnd);
    CGRect frame;

    TRACE("win %p/%p cmd %d at %s flags %08x\n",
          hwnd, data ? data->cocoa_window : NULL, cmd, wine_dbgstr_rect(rect), swp);

    if (!data || !data->cocoa_window) goto done;
    if (IsRectEmpty(rect)) goto done;
    if (GetWindowLongW(hwnd, GWL_STYLE) & WS_MINIMIZE)
    {
        if (rect->left != -32000 || rect->top != -32000)
        {
            OffsetRect(rect, -32000 - rect->left, -32000 - rect->top);
            swp &= ~(SWP_NOMOVE | SWP_NOCLIENTMOVE);
        }
        goto done;
    }
    if (!data->on_screen) goto done;

    /* only fetch the new rectangle if the ShowWindow was a result of an external event */

    if (!thread_data->current_event || thread_data->current_event->window != data->cocoa_window)
        goto done;

    if (thread_data->current_event->type != WINDOW_FRAME_CHANGED &&
        thread_data->current_event->type != WINDOW_DID_UNMINIMIZE)
        goto done;

    macdrv_get_cocoa_window_frame(data->cocoa_window, &frame);
    *rect = rect_from_cgrect(frame);
    macdrv_mac_to_window_rect(data, rect);
    TRACE("rect %s -> %s\n", wine_dbgstr_cgrect(frame), wine_dbgstr_rect(rect));
    swp &= ~(SWP_NOMOVE | SWP_NOCLIENTMOVE | SWP_NOSIZE | SWP_NOCLIENTSIZE);

done:
    release_win_data(data);
    return swp;
}


/***********************************************************************
 *              SysCommand   (MACDRV.@)
 *
 * Perform WM_SYSCOMMAND handling.
 */
LRESULT CDECL macdrv_SysCommand(HWND hwnd, WPARAM wparam, LPARAM lparam)
{
    struct macdrv_win_data *data;
    LRESULT ret = -1;
    WPARAM command = wparam & 0xfff0;

    TRACE("%p, %x, %lx\n", hwnd, (unsigned)wparam, lparam);

    if (!(data = get_win_data(hwnd))) goto done;
    if (!data->cocoa_window || !data->on_screen) goto done;

    /* prevent a simple ALT press+release from activating the system menu,
       as that can get confusing */
    if (command == SC_KEYMENU && !(WCHAR)lparam && !GetMenu(hwnd) &&
        (GetWindowLongW(hwnd, GWL_STYLE) & WS_SYSMENU))
    {
        TRACE("ignoring SC_KEYMENU wp %lx lp %lx\n", wparam, lparam);
        ret = 0;
    }

    if (command == SC_MOVE)
    {
        release_win_data(data);
        return move_window(hwnd, wparam);
    }

done:
    release_win_data(data);
    return ret;
}


/***********************************************************************
 *              UpdateLayeredWindow   (MACDRV.@)
 */
BOOL CDECL macdrv_UpdateLayeredWindow(HWND hwnd, const UPDATELAYEREDWINDOWINFO *info,
                                      const RECT *window_rect)
{
    struct window_surface *surface;
    struct macdrv_win_data *data;
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, 0 };
    BYTE alpha;
    char buffer[FIELD_OFFSET(BITMAPINFO, bmiColors[256])];
    BITMAPINFO *bmi = (BITMAPINFO *)buffer;
    void *src_bits, *dst_bits;
    RECT rect, src_rect;
    HDC hdc = 0;
    HBITMAP dib;
    BOOL ret = FALSE;

    if (!(data = get_win_data(hwnd))) return FALSE;

    data->layered = TRUE;
    data->ulw_layered = TRUE;

    rect = *window_rect;
    OffsetRect(&rect, -window_rect->left, -window_rect->top);

    surface = data->surface;
    if (!surface || !EqualRect(&surface->rect, &rect))
    {
        data->surface = create_surface(data->cocoa_window, &rect, NULL, TRUE);
        set_window_surface(data->cocoa_window, data->surface);
        if (surface) window_surface_release(surface);
        surface = data->surface;
        if (data->unminimized_surface)
        {
            window_surface_release(data->unminimized_surface);
            data->unminimized_surface = NULL;
        }
    }
    else set_surface_use_alpha(surface, TRUE);

    if (surface) window_surface_add_ref(surface);
    release_win_data(data);

    if (!surface) return FALSE;
    if (!info->hdcSrc)
    {
        window_surface_release(surface);
        return TRUE;
    }

    if (info->dwFlags & ULW_ALPHA)
    {
        /* Apply SourceConstantAlpha via window alpha, not blend. */
        alpha = info->pblend->SourceConstantAlpha;
        blend = *info->pblend;
        blend.SourceConstantAlpha = 0xff;
    }
    else
        alpha = 0xff;

    dst_bits = surface->funcs->get_info(surface, bmi);

    if (!(dib = CreateDIBSection(info->hdcDst, bmi, DIB_RGB_COLORS, &src_bits, NULL, 0))) goto done;
    if (!(hdc = CreateCompatibleDC(0))) goto done;

    SelectObject(hdc, dib);
    if (info->prcDirty)
    {
        IntersectRect(&rect, &rect, info->prcDirty);
        surface->funcs->lock(surface);
        memcpy(src_bits, dst_bits, bmi->bmiHeader.biSizeImage);
        surface->funcs->unlock(surface);
        PatBlt(hdc, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, BLACKNESS);
    }
    src_rect = rect;
    if (info->pptSrc) OffsetRect( &src_rect, info->pptSrc->x, info->pptSrc->y );
    DPtoLP( info->hdcSrc, (POINT *)&src_rect, 2 );

    if (!(ret = GdiAlphaBlend(hdc, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
                              info->hdcSrc, src_rect.left, src_rect.top,
                              src_rect.right - src_rect.left, src_rect.bottom - src_rect.top,
                              blend)))
        goto done;

    if ((data = get_win_data(hwnd)))
    {
        if (surface == data->surface)
        {
            surface->funcs->lock(surface);
            memcpy(dst_bits, src_bits, bmi->bmiHeader.biSizeImage);
            add_bounds_rect(surface->funcs->get_bounds(surface), &rect);
            surface->funcs->unlock(surface);
            surface->funcs->flush(surface);
        }

        /* The ULW flags are a superset of the LWA flags. */
        sync_window_opacity(data, info->crKey, alpha, TRUE, info->dwFlags);

        release_win_data(data);
    }

done:
    window_surface_release(surface);
    if (hdc) DeleteDC(hdc);
    if (dib) DeleteObject(dib);
    return ret;
}


/**********************************************************************
 *              WindowMessage   (MACDRV.@)
 */
LRESULT CDECL macdrv_WindowMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    struct macdrv_win_data *data;

    TRACE("%p, %u, %u, %lu\n", hwnd, msg, (unsigned)wp, lp);

    switch(msg)
    {
    case WM_MACDRV_SET_WIN_REGION:
        if ((data = get_win_data(hwnd)))
        {
            sync_window_region(data, (HRGN)1);
            release_win_data(data);
        }
        return 0;
    case WM_MACDRV_UPDATE_DESKTOP_RECT:
        if (hwnd == GetDesktopWindow())
        {
            CGRect new_desktop_rect;
            RECT current_desktop_rect;

            macdrv_reset_device_metrics();
            new_desktop_rect = macdrv_get_desktop_rect();
            if (!GetWindowRect(hwnd, &current_desktop_rect) ||
                !CGRectEqualToRect(cgrect_from_rect(current_desktop_rect), new_desktop_rect))
            {
                SendMessageTimeoutW(HWND_BROADCAST, WM_MACDRV_RESET_DEVICE_METRICS, 0, 0,
                                    SMTO_ABORTIFHUNG, 2000, NULL);
                SetWindowPos(hwnd, 0, CGRectGetMinX(new_desktop_rect), CGRectGetMinY(new_desktop_rect),
                             CGRectGetWidth(new_desktop_rect), CGRectGetHeight(new_desktop_rect),
                             SWP_NOZORDER | SWP_NOACTIVATE | SWP_DEFERERASE);
                SendMessageTimeoutW(HWND_BROADCAST, WM_MACDRV_DISPLAYCHANGE, wp, lp,
                                    SMTO_ABORTIFHUNG, 2000, NULL);
            }
        }
        return 0;
    case WM_MACDRV_RESET_DEVICE_METRICS:
        macdrv_reset_device_metrics();
        return 0;
    case WM_MACDRV_DISPLAYCHANGE:
        macdrv_reassert_window_position(hwnd);
        SendMessageW(hwnd, WM_DISPLAYCHANGE, wp, lp);
        return 0;
    case WM_MACDRV_ACTIVATE_ON_FOLLOWING_FOCUS:
        activate_on_following_focus();
        TRACE("WM_MACDRV_ACTIVATE_ON_FOLLOWING_FOCUS time %u\n", activate_on_focus_time);
        return 0;
    }

    FIXME("unrecognized window msg %x hwnd %p wp %lx lp %lx\n", msg, hwnd, wp, lp);
    return 0;
}


static inline RECT get_surface_rect(const RECT *visible_rect)
{
    RECT rect;
    RECT desktop_rect = rect_from_cgrect(macdrv_get_desktop_rect());

    IntersectRect(&rect, visible_rect, &desktop_rect);
    OffsetRect(&rect, -visible_rect->left, -visible_rect->top);
    rect.left &= ~127;
    rect.top  &= ~127;
    rect.right  = max(rect.left + 128, (rect.right + 127) & ~127);
    rect.bottom = max(rect.top + 128, (rect.bottom + 127) & ~127);
    return rect;
}


/***********************************************************************
 *              WindowPosChanging   (MACDRV.@)
 */
void CDECL macdrv_WindowPosChanging(HWND hwnd, HWND insert_after, UINT swp_flags,
                                    const RECT *window_rect, const RECT *client_rect,
                                    RECT *visible_rect, struct window_surface **surface)
{
    struct macdrv_win_data *data = get_win_data(hwnd);
    DWORD style = GetWindowLongW(hwnd, GWL_STYLE);
    RECT surface_rect;

    TRACE("%p after %p swp %04x window %s client %s visible %s surface %p\n", hwnd, insert_after,
          swp_flags, wine_dbgstr_rect(window_rect), wine_dbgstr_rect(client_rect),
          wine_dbgstr_rect(visible_rect), surface);

    if (!data && !(data = macdrv_create_win_data(hwnd, window_rect, client_rect))) return;

    *visible_rect = *window_rect;
    macdrv_window_to_mac_rect(data, style, visible_rect, window_rect, client_rect);
    TRACE("visible_rect %s -> %s\n", wine_dbgstr_rect(window_rect),
          wine_dbgstr_rect(visible_rect));

    /* create the window surface if necessary */
    if (!data->cocoa_window) goto done;
    if (swp_flags & SWP_HIDEWINDOW) goto done;
    if (data->ulw_layered) goto done;

    if (*surface) window_surface_release(*surface);
    *surface = NULL;

    surface_rect = get_surface_rect(visible_rect);
    if (data->surface)
    {
        if (EqualRect(&data->surface->rect, &surface_rect))
        {
            /* existing surface is good enough */
            surface_clip_to_visible_rect(data->surface, visible_rect);
            window_surface_add_ref(data->surface);
            *surface = data->surface;
            goto done;
        }
    }
    else if (!(swp_flags & SWP_SHOWWINDOW) && !(style & WS_VISIBLE)) goto done;

    *surface = create_surface(data->cocoa_window, &surface_rect, data->surface, FALSE);

done:
    release_win_data(data);
}


/***********************************************************************
 *              WindowPosChanged   (MACDRV.@)
 */
void CDECL macdrv_WindowPosChanged(HWND hwnd, HWND insert_after, UINT swp_flags,
                                   const RECT *window_rect, const RECT *client_rect,
                                   const RECT *visible_rect, const RECT *valid_rects,
                                   struct window_surface *surface)
{
    struct macdrv_thread_data *thread_data;
    struct macdrv_win_data *data;
    DWORD new_style = GetWindowLongW(hwnd, GWL_STYLE);
    RECT old_window_rect, old_whole_rect, old_client_rect;

    if (!(data = get_win_data(hwnd))) return;

    thread_data = macdrv_thread_data();

    old_window_rect = data->window_rect;
    old_whole_rect  = data->whole_rect;
    old_client_rect = data->client_rect;
    data->window_rect = *window_rect;
    data->whole_rect  = *visible_rect;
    data->client_rect = *client_rect;
    if (data->cocoa_window && !data->ulw_layered)
    {
        if (surface) window_surface_add_ref(surface);
        if (new_style & WS_MINIMIZE)
        {
            if (!data->unminimized_surface && data->surface)
            {
                data->unminimized_surface = data->surface;
                window_surface_add_ref(data->unminimized_surface);
            }
        }
        else
        {
            set_window_surface(data->cocoa_window, surface);
            if (data->unminimized_surface)
            {
                window_surface_release(data->unminimized_surface);
                data->unminimized_surface = NULL;
            }
        }
        if (data->surface) window_surface_release(data->surface);
        data->surface = surface;
    }

    TRACE("win %p/%p window %s whole %s client %s style %08x flags %08x surface %p\n",
           hwnd, data->cocoa_window, wine_dbgstr_rect(window_rect),
           wine_dbgstr_rect(visible_rect), wine_dbgstr_rect(client_rect),
           new_style, swp_flags, surface);

    if (!IsRectEmpty(&valid_rects[0]))
    {
        macdrv_window window = data->cocoa_window;
        int x_offset = old_whole_rect.left - data->whole_rect.left;
        int y_offset = old_whole_rect.top - data->whole_rect.top;

        /* if all that happened is that the whole window moved, copy everything */
        if (!(swp_flags & SWP_FRAMECHANGED) &&
            old_whole_rect.right   - data->whole_rect.right   == x_offset &&
            old_whole_rect.bottom  - data->whole_rect.bottom  == y_offset &&
            old_client_rect.left   - data->client_rect.left   == x_offset &&
            old_client_rect.right  - data->client_rect.right  == x_offset &&
            old_client_rect.top    - data->client_rect.top    == y_offset &&
            old_client_rect.bottom - data->client_rect.bottom == y_offset &&
            EqualRect(&valid_rects[0], &data->client_rect))
        {
            /* A Cocoa window's bits are moved automatically */
            if (!window && (x_offset != 0 || y_offset != 0))
            {
                release_win_data(data);
                move_window_bits(hwnd, window, &old_whole_rect, visible_rect,
                                 &old_client_rect, client_rect, window_rect);
                if (!(data = get_win_data(hwnd))) return;
            }
        }
        else
        {
            release_win_data(data);
            move_window_bits(hwnd, window, &valid_rects[1], &valid_rects[0],
                             &old_client_rect, client_rect, window_rect);
            if (!(data = get_win_data(hwnd))) return;
        }
    }

    sync_gl_view(data, &old_whole_rect, &old_client_rect);

    if (!data->cocoa_window && !data->cocoa_view) goto done;

    if (data->on_screen)
    {
        if ((swp_flags & SWP_HIDEWINDOW) && !(new_style & WS_VISIBLE))
            hide_window(data);
    }

    /* check if we are currently processing an event relevant to this window */
    if (thread_data && thread_data->current_event &&
        data->cocoa_window && thread_data->current_event->window == data->cocoa_window &&
        (thread_data->current_event->type == WINDOW_FRAME_CHANGED ||
         thread_data->current_event->type == WINDOW_DID_UNMINIMIZE))
    {
        if (thread_data->current_event->type == WINDOW_FRAME_CHANGED)
            sync_client_view_position(data);
    }
    else
    {
        sync_window_position(data, swp_flags, &old_window_rect, &old_whole_rect);
        if (data->cocoa_window)
            set_cocoa_window_properties(data);
    }

    if (new_style & WS_VISIBLE)
    {
        if (data->cocoa_window)
        {
            if (!data->on_screen || (swp_flags & (SWP_FRAMECHANGED|SWP_STATECHANGED)))
                set_cocoa_window_properties(data);

            /* layered windows are not shown until their attributes are set */
            if (!data->on_screen &&
                (data->layered || !(GetWindowLongW( hwnd, GWL_EXSTYLE ) & WS_EX_LAYERED)))
                show_window(data);
        }
        else if (!data->on_screen)
            show_window(data);
    }

done:
    release_win_data(data);
}


/***********************************************************************
 *              macdrv_window_close_requested
 *
 * Handler for WINDOW_CLOSE_REQUESTED events.
 */
void macdrv_window_close_requested(HWND hwnd)
{
    HMENU sysmenu;

    if (GetClassLongW(hwnd, GCL_STYLE) & CS_NOCLOSE)
    {
        TRACE("not closing win %p class style CS_NOCLOSE\n", hwnd);
        return;
    }

    sysmenu = GetSystemMenu(hwnd, FALSE);
    if (sysmenu)
    {
        UINT state = GetMenuState(sysmenu, SC_CLOSE, MF_BYCOMMAND);
        if (state == 0xFFFFFFFF || (state & (MF_DISABLED | MF_GRAYED)))
        {
            TRACE("not closing win %p menu state 0x%08x\n", hwnd, state);
            return;
        }
    }

    perform_window_command(hwnd, 0, 0, SC_CLOSE, HTCLOSE);
}


/***********************************************************************
 *              macdrv_window_frame_changed
 *
 * Handler for WINDOW_FRAME_CHANGED events.
 */
void macdrv_window_frame_changed(HWND hwnd, const macdrv_event *event)
{
    struct macdrv_win_data *data;
    RECT rect;
    HWND parent;
    UINT flags = SWP_NOACTIVATE | SWP_NOZORDER;
    int width, height;
    BOOL being_dragged;

    if (!hwnd) return;
    if (!(data = get_win_data(hwnd))) return;
    if (!data->on_screen || data->minimized)
    {
        release_win_data(data);
        return;
    }

    /* Get geometry */

    parent = GetAncestor(hwnd, GA_PARENT);

    TRACE("win %p/%p new Cocoa frame %s fullscreen %d in_resize %d\n", hwnd, data->cocoa_window,
          wine_dbgstr_cgrect(event->window_frame_changed.frame),
          event->window_frame_changed.fullscreen, event->window_frame_changed.in_resize);

    rect = rect_from_cgrect(event->window_frame_changed.frame);
    macdrv_mac_to_window_rect(data, &rect);
    MapWindowPoints(0, parent, (POINT *)&rect, 2);

    width = rect.right - rect.left;
    height = rect.bottom - rect.top;

    if (data->window_rect.left == rect.left && data->window_rect.top == rect.top)
        flags |= SWP_NOMOVE;
    else
        TRACE("%p moving from (%d,%d) to (%d,%d)\n", hwnd, data->window_rect.left,
              data->window_rect.top, rect.left, rect.top);

    if ((data->window_rect.right - data->window_rect.left == width &&
         data->window_rect.bottom - data->window_rect.top == height) ||
        (IsRectEmpty(&data->window_rect) && width == 1 && height == 1))
        flags |= SWP_NOSIZE;
    else
        TRACE("%p resizing from (%dx%d) to (%dx%d)\n", hwnd, data->window_rect.right - data->window_rect.left,
              data->window_rect.bottom - data->window_rect.top, width, height);

    being_dragged = data->drag_event != NULL;
    release_win_data(data);

    if (event->window_frame_changed.fullscreen)
        flags |= SWP_NOSENDCHANGING;
    if (!(flags & SWP_NOSIZE) || !(flags & SWP_NOMOVE))
    {
        if (!event->window_frame_changed.in_resize && !being_dragged)
            SendMessageW(hwnd, WM_ENTERSIZEMOVE, 0, 0);
        SetWindowPos(hwnd, 0, rect.left, rect.top, width, height, flags);
        if (!event->window_frame_changed.in_resize && !being_dragged)
            SendMessageW(hwnd, WM_EXITSIZEMOVE, 0, 0);
    }
}


/***********************************************************************
 *              macdrv_window_got_focus
 *
 * Handler for WINDOW_GOT_FOCUS events.
 */
void macdrv_window_got_focus(HWND hwnd, const macdrv_event *event)
{
    LONG style = GetWindowLongW(hwnd, GWL_STYLE);

    if (!hwnd) return;

    TRACE("win %p/%p serial %lu enabled %d visible %d style %08x focus %p active %p fg %p\n",
          hwnd, event->window, event->window_got_focus.serial, IsWindowEnabled(hwnd),
          IsWindowVisible(hwnd), style, GetFocus(), GetActiveWindow(), GetForegroundWindow());

    if (can_activate_window(hwnd) && !(style & WS_MINIMIZE))
    {
        /* simulate a mouse click on the menu to find out
         * whether the window wants to be activated */
        LRESULT ma = SendMessageW(hwnd, WM_MOUSEACTIVATE,
                                  (WPARAM)GetAncestor(hwnd, GA_ROOT),
                                  MAKELONG(HTMENU, WM_LBUTTONDOWN));
        if (ma != MA_NOACTIVATEANDEAT && ma != MA_NOACTIVATE)
        {
            TRACE("setting foreground window to %p\n", hwnd);
            SetForegroundWindow(hwnd);
            return;
        }
    }

    TRACE("win %p/%p rejecting focus\n", hwnd, event->window);
    macdrv_window_rejected_focus(event);
}


/***********************************************************************
 *              macdrv_window_lost_focus
 *
 * Handler for WINDOW_LOST_FOCUS events.
 */
void macdrv_window_lost_focus(HWND hwnd, const macdrv_event *event)
{
    if (!hwnd) return;

    TRACE("win %p/%p fg %p\n", hwnd, event->window, GetForegroundWindow());

    if (hwnd == GetForegroundWindow())
    {
        SendMessageW(hwnd, WM_CANCELMODE, 0, 0);
        if (hwnd == GetForegroundWindow())
            SetForegroundWindow(GetDesktopWindow());
    }
}


/***********************************************************************
 *              macdrv_app_activated
 *
 * Handler for APP_ACTIVATED events.
 */
void macdrv_app_activated(void)
{
    TRACE("\n");
    macdrv_UpdateClipboard();
}


/***********************************************************************
 *              macdrv_app_deactivated
 *
 * Handler for APP_DEACTIVATED events.
 */
void macdrv_app_deactivated(void)
{
    if (GetActiveWindow() == GetForegroundWindow())
    {
        TRACE("setting fg to desktop\n");
        SetForegroundWindow(GetDesktopWindow());
    }
}


/***********************************************************************
 *              macdrv_window_maximize_requested
 *
 * Handler for WINDOW_MAXIMIZE_REQUESTED events.
 */
void macdrv_window_maximize_requested(HWND hwnd)
{
    perform_window_command(hwnd, WS_MAXIMIZEBOX, WS_MAXIMIZE, SC_MAXIMIZE, HTMAXBUTTON);
}


/***********************************************************************
 *              macdrv_window_minimize_requested
 *
 * Handler for WINDOW_MINIMIZE_REQUESTED events.
 */
void macdrv_window_minimize_requested(HWND hwnd)
{
    perform_window_command(hwnd, WS_MINIMIZEBOX, WS_MINIMIZE, SC_MINIMIZE, HTMINBUTTON);
}


/***********************************************************************
 *              macdrv_window_did_unminimize
 *
 * Handler for WINDOW_DID_UNMINIMIZE events.
 */
void macdrv_window_did_unminimize(HWND hwnd)
{
    struct macdrv_win_data *data;
    DWORD style;

    TRACE("win %p\n", hwnd);

    if (!(data = get_win_data(hwnd))) return;
    if (!data->minimized) goto done;

    style = GetWindowLongW(hwnd, GWL_STYLE);

    data->minimized = FALSE;
    if ((style & (WS_MINIMIZE | WS_VISIBLE)) == (WS_MINIMIZE | WS_VISIBLE))
    {
        TRACE("restoring win %p/%p\n", hwnd, data->cocoa_window);
        release_win_data(data);
        SetActiveWindow(hwnd);
        SendMessageW(hwnd, WM_SYSCOMMAND, SC_RESTORE, 0);
        return;
    }

    TRACE("not restoring win %p/%p style %08x\n", hwnd, data->cocoa_window, style);

done:
    release_win_data(data);
}


/***********************************************************************
 *              macdrv_window_brought_forward
 *
 * Handler for WINDOW_BROUGHT_FORWARD events.
 */
void macdrv_window_brought_forward(HWND hwnd)
{
    TRACE("win %p\n", hwnd);
    SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}


/***********************************************************************
 *              macdrv_window_resize_ended
 *
 * Handler for WINDOW_RESIZE_ENDED events.
 */
void macdrv_window_resize_ended(HWND hwnd)
{
    TRACE("hwnd %p\n", hwnd);
    SendMessageW(hwnd, WM_EXITSIZEMOVE, 0, 0);
}


/***********************************************************************
 *              macdrv_window_restore_requested
 *
 * Handler for WINDOW_RESTORE_REQUESTED events.  This is specifically
 * for restoring from maximized, not from minimized.
 */
void macdrv_window_restore_requested(HWND hwnd, const macdrv_event *event)
{
    if (event->window_restore_requested.keep_frame && hwnd)
    {
        DWORD style = GetWindowLongW(hwnd, GWL_STYLE);
        struct macdrv_win_data *data;

        if ((style & WS_MAXIMIZE) && (style & WS_VISIBLE) && (data = get_win_data(hwnd)))
        {
            RECT rect;
            HWND parent = GetAncestor(hwnd, GA_PARENT);

            rect = rect_from_cgrect(event->window_restore_requested.frame);
            macdrv_mac_to_window_rect(data, &rect);
            MapWindowPoints(0, parent, (POINT *)&rect, 2);

            release_win_data(data);

            SetInternalWindowPos(hwnd, SW_SHOW, &rect, NULL);
        }
    }

    perform_window_command(hwnd, WS_MAXIMIZE, 0, SC_RESTORE, HTMAXBUTTON);
}


/***********************************************************************
 *              macdrv_window_drag_begin
 *
 * Handler for WINDOW_DRAG_BEGIN events.
 */
void macdrv_window_drag_begin(HWND hwnd, const macdrv_event *event)
{
    DWORD style = GetWindowLongW(hwnd, GWL_STYLE);
    struct macdrv_win_data *data;
    HANDLE drag_event = NULL;
    BOOL loop = TRUE;
    MSG msg;

    TRACE("win %p\n", hwnd);

    if (style & (WS_DISABLED | WS_MAXIMIZE | WS_MINIMIZE)) return;
    if (!(style & WS_VISIBLE)) return;

    if (!(data = get_win_data(hwnd))) return;
    if (data->drag_event) goto done;

    drag_event = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!drag_event) goto done;

    data->drag_event = drag_event;
    release_win_data(data);

    if (!event->window_drag_begin.no_activate && can_activate_window(hwnd) && GetForegroundWindow() != hwnd)
    {
        /* ask whether the window wants to be activated */
        LRESULT ma = SendMessageW(hwnd, WM_MOUSEACTIVATE, (WPARAM)GetAncestor(hwnd, GA_ROOT),
                                  MAKELONG(HTCAPTION, WM_LBUTTONDOWN));
        if (ma != MA_NOACTIVATEANDEAT && ma != MA_NOACTIVATE)
        {
            TRACE("setting foreground window to %p\n", hwnd);
            SetForegroundWindow(hwnd);
        }
    }

    ClipCursor(NULL);
    SendMessageW(hwnd, WM_ENTERSIZEMOVE, 0, 0);
    ReleaseCapture();

    while (loop)
    {
        while (!PeekMessageW(&msg, 0, 0, 0, PM_REMOVE))
        {
            DWORD result = MsgWaitForMultipleObjectsEx(1, &drag_event, INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
            if (result == WAIT_OBJECT_0)
            {
                loop = FALSE;
                break;
            }
        }
        if (!loop)
            break;

        if (msg.message == WM_QUIT)
            break;

        if (!CallMsgFilterW(&msg, MSGF_SIZE) && msg.message != WM_KEYDOWN &&
            msg.message != WM_MOUSEMOVE && msg.message != WM_LBUTTONDOWN && msg.message != WM_LBUTTONUP)
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    SendMessageW(hwnd, WM_EXITSIZEMOVE, 0, 0);

    TRACE("done\n");

    if ((data = get_win_data(hwnd)))
        data->drag_event = NULL;

done:
    release_win_data(data);
    if (drag_event) CloseHandle(drag_event);
}


/***********************************************************************
 *              macdrv_window_drag_end
 *
 * Handler for WINDOW_DRAG_END events.
 */
void macdrv_window_drag_end(HWND hwnd)
{
    struct macdrv_win_data *data;

    TRACE("win %p\n", hwnd);

    if (!(data = get_win_data(hwnd))) return;
    if (data->drag_event)
        SetEvent(data->drag_event);
    release_win_data(data);
}


/***********************************************************************
 *              macdrv_reassert_window_position
 *
 * Handler for REASSERT_WINDOW_POSITION events.
 */
void macdrv_reassert_window_position(HWND hwnd)
{
    struct macdrv_win_data *data = get_win_data(hwnd);
    if (data)
    {
        if (data->cocoa_window && data->on_screen)
            sync_window_position(data, SWP_NOZORDER | SWP_NOACTIVATE, NULL, NULL);
        release_win_data(data);
    }
}


struct quit_info {
    HWND               *wins;
    UINT                capacity;
    UINT                count;
    UINT                done;
    DWORD               flags;
    BOOL                result;
    BOOL                replied;
};


static BOOL CALLBACK get_process_windows(HWND hwnd, LPARAM lp)
{
    struct quit_info *qi = (struct quit_info*)lp;
    DWORD pid;

    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == GetCurrentProcessId())
    {
        if (qi->count >= qi->capacity)
        {
            UINT new_cap = qi->capacity * 2;
            HWND *new_wins = HeapReAlloc(GetProcessHeap(), 0, qi->wins,
                                         new_cap * sizeof(*qi->wins));
            if (!new_wins) return FALSE;
            qi->wins = new_wins;
            qi->capacity = new_cap;
        }

        qi->wins[qi->count++] = hwnd;
    }

    return TRUE;
}


static void CALLBACK quit_callback(HWND hwnd, UINT msg, ULONG_PTR data, LRESULT result)
{
    struct quit_info *qi = (struct quit_info*)data;

    qi->done++;

    if (msg == WM_QUERYENDSESSION)
    {
        TRACE("got WM_QUERYENDSESSION result %ld from win %p (%u of %u done)\n", result,
              hwnd, qi->done, qi->count);

        if (!result && !IsWindow(hwnd))
        {
            TRACE("win %p no longer exists; ignoring apparent refusal\n", hwnd);
            result = TRUE;
        }

        if (!result && qi->result)
        {
            qi->result = FALSE;

            /* On the first FALSE from WM_QUERYENDSESSION, we already know the
               ultimate reply.  Might as well tell Cocoa now. */
            if (!qi->replied)
            {
                qi->replied = TRUE;
                TRACE("giving quit reply %d\n", qi->result);
                macdrv_quit_reply(qi->result);
            }
        }

        if (qi->done >= qi->count)
        {
            UINT i;

            qi->done = 0;
            for (i = 0; i < qi->count; i++)
            {
                TRACE("sending WM_ENDSESSION to win %p result %d flags 0x%08x\n", qi->wins[i],
                      qi->result, qi->flags);
                if (!SendMessageCallbackW(qi->wins[i], WM_ENDSESSION, qi->result, qi->flags,
                                          quit_callback, (ULONG_PTR)qi))
                {
                    WARN("failed to send WM_ENDSESSION to win %p; error 0x%08x\n",
                         qi->wins[i], GetLastError());
                    quit_callback(qi->wins[i], WM_ENDSESSION, (ULONG_PTR)qi, 0);
                }
            }
        }
    }
    else /* WM_ENDSESSION */
    {
        TRACE("finished WM_ENDSESSION for win %p (%u of %u done)\n", hwnd, qi->done, qi->count);

        if (qi->done >= qi->count)
        {
            if (!qi->replied)
            {
                TRACE("giving quit reply %d\n", qi->result);
                macdrv_quit_reply(qi->result);
            }

            TRACE("%sterminating process\n", qi->result ? "" : "not ");
            if (qi->result)
                TerminateProcess(GetCurrentProcess(), 0);

            HeapFree(GetProcessHeap(), 0, qi->wins);
            HeapFree(GetProcessHeap(), 0, qi);
        }
    }
}


/***********************************************************************
 *              macdrv_app_quit_requested
 *
 * Handler for APP_QUIT_REQUESTED events.
 */
void macdrv_app_quit_requested(const macdrv_event *event)
{
    struct quit_info *qi;
    UINT i;

    TRACE("reason %d\n", event->app_quit_requested.reason);

    qi = HeapAlloc(GetProcessHeap(), 0, sizeof(*qi));
    if (!qi)
        goto fail;

    qi->capacity = 32;
    qi->wins = HeapAlloc(GetProcessHeap(), 0, qi->capacity * sizeof(*qi->wins));
    qi->count = qi->done = 0;

    if (!qi->wins || !EnumWindows(get_process_windows, (LPARAM)qi))
        goto fail;

    switch (event->app_quit_requested.reason)
    {
        case QUIT_REASON_LOGOUT:
        default:
            qi->flags = ENDSESSION_LOGOFF;
            break;
        case QUIT_REASON_RESTART:
        case QUIT_REASON_SHUTDOWN:
            qi->flags = 0;
            break;
    }

    qi->result = TRUE;
    qi->replied = FALSE;

    for (i = 0; i < qi->count; i++)
    {
        TRACE("sending WM_QUERYENDSESSION to win %p\n", qi->wins[i]);
        if (!SendMessageCallbackW(qi->wins[i], WM_QUERYENDSESSION, 0, qi->flags,
                                  quit_callback, (ULONG_PTR)qi))
        {
            DWORD error = GetLastError();
            BOOL invalid = (error == ERROR_INVALID_WINDOW_HANDLE);
            if (invalid)
                TRACE("failed to send WM_QUERYENDSESSION to win %p because it's invalid; assuming success\n",
                     qi->wins[i]);
            else
                WARN("failed to send WM_QUERYENDSESSION to win %p; error 0x%08x; assuming refusal\n",
                     qi->wins[i], error);
            quit_callback(qi->wins[i], WM_QUERYENDSESSION, (ULONG_PTR)qi, invalid);
        }
    }

    /* quit_callback() will clean up qi */
    return;

fail:
    WARN("failed to allocate window list\n");
    if (qi)
    {
        HeapFree(GetProcessHeap(), 0, qi->wins);
        HeapFree(GetProcessHeap(), 0, qi);
    }
    macdrv_quit_reply(FALSE);
}


/***********************************************************************
 *              query_resize_size
 *
 * Handler for QUERY_RESIZE_SIZE query.
 */
BOOL query_resize_size(HWND hwnd, macdrv_query *query)
{
    struct macdrv_win_data *data = get_win_data(hwnd);
    RECT rect = rect_from_cgrect(query->resize_size.rect);
    int corner;
    BOOL ret = FALSE;

    if (!data) return FALSE;

    macdrv_mac_to_window_rect(data, &rect);

    if (query->resize_size.from_left)
    {
        if (query->resize_size.from_top)
            corner = WMSZ_TOPLEFT;
        else
            corner = WMSZ_BOTTOMLEFT;
    }
    else if (query->resize_size.from_top)
        corner = WMSZ_TOPRIGHT;
    else
        corner = WMSZ_BOTTOMRIGHT;

    if (SendMessageW(hwnd, WM_SIZING, corner, (LPARAM)&rect))
    {
        macdrv_window_to_mac_rect(data, GetWindowLongW(hwnd, GWL_STYLE), &rect,
                                  &data->window_rect, &data->client_rect);
        query->resize_size.rect = cgrect_from_rect(rect);
        ret = TRUE;
    }

    release_win_data(data);
    return ret;
}


/***********************************************************************
 *              query_resize_start
 *
 * Handler for QUERY_RESIZE_START query.
 */
BOOL query_resize_start(HWND hwnd)
{
    TRACE("hwnd %p\n", hwnd);

    sync_window_min_max_info(hwnd);
    SendMessageW(hwnd, WM_ENTERSIZEMOVE, 0, 0);

    return TRUE;
}


/***********************************************************************
 *              query_min_max_info
 *
 * Handler for QUERY_MIN_MAX_INFO query.
 */
BOOL query_min_max_info(HWND hwnd)
{
    TRACE("hwnd %p\n", hwnd);
    sync_window_min_max_info(hwnd);
    return TRUE;
}
