/*
 * MACDRV windowing driver
 *
 * Copyright 1996 Alexandre Julliard
 * Copyright 1999 Patrik Stridvall
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

#ifndef __WINE_MACDRV_H
#define __WINE_MACDRV_H

#ifndef __WINE_CONFIG_H
# error You must include config.h to use this header
#endif

#include "macdrv_cocoa.h"
#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "wine/debug.h"
#include "wine/gdi_driver.h"


extern BOOL skip_single_buffer_flushes DECLSPEC_HIDDEN;
extern BOOL allow_vsync DECLSPEC_HIDDEN;
extern BOOL allow_set_gamma DECLSPEC_HIDDEN;
extern BOOL allow_software_rendering DECLSPEC_HIDDEN;
extern BOOL disable_window_decorations DECLSPEC_HIDDEN;
extern HMODULE macdrv_module DECLSPEC_HIDDEN;


extern const char* debugstr_cf(CFTypeRef t) DECLSPEC_HIDDEN;

static inline CGRect cgrect_from_rect(RECT rect)
{
    if (rect.left >= rect.right || rect.top >= rect.bottom)
        return CGRectMake(rect.left, rect.top, 0, 0);
    return CGRectMake(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
}

static inline RECT rect_from_cgrect(CGRect cgrect)
{
    static const RECT empty;

    if (!CGRectIsNull(cgrect))
    {
        RECT rect = { CGRectGetMinX(cgrect), CGRectGetMinY(cgrect),
                      CGRectGetMaxX(cgrect), CGRectGetMaxY(cgrect) };
        return rect;
    }

    return empty;
}

static inline const char *wine_dbgstr_cgrect(CGRect cgrect)
{
    return wine_dbg_sprintf("(%g,%g)-(%g,%g)", CGRectGetMinX(cgrect), CGRectGetMinY(cgrect),
                            CGRectGetMaxX(cgrect), CGRectGetMaxY(cgrect));
}

extern const char* debugstr_cf(CFTypeRef t) DECLSPEC_HIDDEN;


/**************************************************************************
 * Mac GDI driver
 */

extern CGRect macdrv_get_desktop_rect(void) DECLSPEC_HIDDEN;
extern void macdrv_reset_device_metrics(void) DECLSPEC_HIDDEN;
extern BOOL CDECL macdrv_GetDeviceGammaRamp(PHYSDEV dev, LPVOID ramp) DECLSPEC_HIDDEN;
extern BOOL CDECL macdrv_SetDeviceGammaRamp(PHYSDEV dev, LPVOID ramp) DECLSPEC_HIDDEN;


/**************************************************************************
 * Mac USER driver
 */

/* Mac driver private messages, must be in the range 0x80001000..0x80001fff */
enum macdrv_window_messages
{
    WM_MACDRV_SET_WIN_REGION = 0x80001000,
    WM_MACDRV_UPDATE_DESKTOP_RECT,
    WM_MACDRV_RESET_DEVICE_METRICS,
    WM_MACDRV_DISPLAYCHANGE,
    WM_MACDRV_ACTIVATE_ON_FOLLOWING_FOCUS,
};

struct macdrv_thread_data
{
    macdrv_event_queue          queue;
    const macdrv_event         *current_event;
    macdrv_window               capture_window;
    CFDataRef                   keyboard_layout_uchr;
    CGEventSourceKeyboardType   keyboard_type;
    int                         iso_keyboard;
    CGEventFlags                last_modifiers;
    UInt32                      dead_key_state;
    HKL                         active_keyboard_layout;
    WORD                        keyc2vkey[128];
    WORD                        keyc2scan[128];
};

extern DWORD thread_data_tls_index DECLSPEC_HIDDEN;

extern struct macdrv_thread_data *macdrv_init_thread_data(void) DECLSPEC_HIDDEN;

static inline struct macdrv_thread_data *macdrv_thread_data(void)
{
    return TlsGetValue(thread_data_tls_index);
}


/* macdrv private window data */
struct macdrv_win_data
{
    HWND                hwnd;                   /* hwnd that this private data belongs to */
    macdrv_window       cocoa_window;
    macdrv_view         cocoa_view;
    macdrv_view         client_cocoa_view;
    RECT                window_rect;            /* USER window rectangle relative to parent */
    RECT                whole_rect;             /* Mac window rectangle for the whole window relative to parent */
    RECT                client_rect;            /* client area relative to parent */
    int                 pixel_format;           /* pixel format for GL */
    COLORREF            color_key;              /* color key for layered window; CLR_INVALID is not color keyed */
    HANDLE              drag_event;             /* event to signal that Cocoa-driven window dragging has ended */
    unsigned int        on_screen : 1;          /* is window ordered in? (minimized or not) */
    unsigned int        shaped : 1;             /* is window using a custom region shape? */
    unsigned int        layered : 1;            /* is window layered and with valid attributes? */
    unsigned int        ulw_layered : 1;        /* has UpdateLayeredWindow() been called for window? */
    unsigned int        per_pixel_alpha : 1;    /* is window using per-pixel alpha? */
    unsigned int        minimized : 1;          /* is window minimized? */
    unsigned int        swap_interval : 1;      /* GL swap interval for window */
    struct window_surface *surface;
    struct window_surface *unminimized_surface;
};

extern struct macdrv_win_data *get_win_data(HWND hwnd) DECLSPEC_HIDDEN;
extern void release_win_data(struct macdrv_win_data *data) DECLSPEC_HIDDEN;
extern macdrv_window macdrv_get_cocoa_window(HWND hwnd, BOOL require_on_screen) DECLSPEC_HIDDEN;
extern RGNDATA *get_region_data(HRGN hrgn, HDC hdc_lptodp) DECLSPEC_HIDDEN;
extern void activate_on_following_focus(void) DECLSPEC_HIDDEN;
extern struct window_surface *create_surface(macdrv_window window, const RECT *rect,
                                             struct window_surface *old_surface, BOOL use_alpha) DECLSPEC_HIDDEN;
extern void set_window_surface(macdrv_window window, struct window_surface *window_surface) DECLSPEC_HIDDEN;
extern void set_surface_use_alpha(struct window_surface *window_surface, BOOL use_alpha) DECLSPEC_HIDDEN;
extern void surface_clip_to_visible_rect(struct window_surface *window_surface, const RECT *visible_rect) DECLSPEC_HIDDEN;

extern void macdrv_handle_event(const macdrv_event *event) DECLSPEC_HIDDEN;

extern void macdrv_window_close_requested(HWND hwnd) DECLSPEC_HIDDEN;
extern void macdrv_window_frame_changed(HWND hwnd, const macdrv_event *event) DECLSPEC_HIDDEN;
extern void macdrv_window_got_focus(HWND hwnd, const macdrv_event *event) DECLSPEC_HIDDEN;
extern void macdrv_window_lost_focus(HWND hwnd, const macdrv_event *event) DECLSPEC_HIDDEN;
extern void macdrv_app_activated(void) DECLSPEC_HIDDEN;
extern void macdrv_app_deactivated(void) DECLSPEC_HIDDEN;
extern void macdrv_app_quit_requested(const macdrv_event *event) DECLSPEC_HIDDEN;
extern void macdrv_window_maximize_requested(HWND hwnd) DECLSPEC_HIDDEN;
extern void macdrv_window_minimize_requested(HWND hwnd) DECLSPEC_HIDDEN;
extern void macdrv_window_did_unminimize(HWND hwnd) DECLSPEC_HIDDEN;
extern void macdrv_window_brought_forward(HWND hwnd) DECLSPEC_HIDDEN;
extern void macdrv_window_resize_ended(HWND hwnd) DECLSPEC_HIDDEN;
extern void macdrv_window_restore_requested(HWND hwnd, const macdrv_event *event) DECLSPEC_HIDDEN;
extern void macdrv_window_drag_begin(HWND hwnd, const macdrv_event *event) DECLSPEC_HIDDEN;
extern void macdrv_window_drag_end(HWND hwnd) DECLSPEC_HIDDEN;
extern void macdrv_reassert_window_position(HWND hwnd) DECLSPEC_HIDDEN;
extern BOOL query_resize_size(HWND hwnd, macdrv_query *query) DECLSPEC_HIDDEN;
extern BOOL query_resize_start(HWND hwnd) DECLSPEC_HIDDEN;
extern BOOL query_min_max_info(HWND hwnd) DECLSPEC_HIDDEN;

extern void macdrv_mouse_button(HWND hwnd, const macdrv_event *event) DECLSPEC_HIDDEN;
extern void macdrv_mouse_moved(HWND hwnd, const macdrv_event *event) DECLSPEC_HIDDEN;
extern void macdrv_mouse_scroll(HWND hwnd, const macdrv_event *event) DECLSPEC_HIDDEN;
extern void macdrv_release_capture(HWND hwnd, const macdrv_event *event) DECLSPEC_HIDDEN;
extern void CDECL macdrv_SetCapture(HWND hwnd, UINT flags) DECLSPEC_HIDDEN;

extern void macdrv_compute_keyboard_layout(struct macdrv_thread_data *thread_data) DECLSPEC_HIDDEN;
extern void macdrv_keyboard_changed(const macdrv_event *event) DECLSPEC_HIDDEN;
extern void macdrv_key_event(HWND hwnd, const macdrv_event *event) DECLSPEC_HIDDEN;
extern void macdrv_hotkey_press(const macdrv_event *event) DECLSPEC_HIDDEN;
extern HKL macdrv_get_hkl_from_source(TISInputSourceRef input_source) DECLSPEC_HIDDEN;

extern void macdrv_displays_changed(const macdrv_event *event) DECLSPEC_HIDDEN;

extern void CDECL macdrv_UpdateClipboard(void) DECLSPEC_HIDDEN;
extern void macdrv_init_clipboard(void) DECLSPEC_HIDDEN;
extern BOOL query_pasteboard_data(HWND hwnd, CFStringRef type) DECLSPEC_HIDDEN;
extern void macdrv_lost_pasteboard_ownership(HWND hwnd) DECLSPEC_HIDDEN;
extern const char *debugstr_format(UINT id) DECLSPEC_HIDDEN;
extern HANDLE macdrv_get_pasteboard_data(CFTypeRef pasteboard, UINT desired_format) DECLSPEC_HIDDEN;
extern BOOL macdrv_pasteboard_has_format(CFTypeRef pasteboard, UINT desired_format) DECLSPEC_HIDDEN;
extern UINT* macdrv_get_pasteboard_formats(CFTypeRef pasteboard, UINT* num_formats) DECLSPEC_HIDDEN;

extern BOOL query_drag_operation(macdrv_query* query) DECLSPEC_HIDDEN;
extern BOOL query_drag_exited(macdrv_query* query) DECLSPEC_HIDDEN;
extern BOOL query_drag_drop(macdrv_query* query) DECLSPEC_HIDDEN;

extern struct opengl_funcs * CDECL macdrv_wine_get_wgl_driver(PHYSDEV dev, UINT version) DECLSPEC_HIDDEN;
extern const struct vulkan_funcs * CDECL macdrv_wine_get_vulkan_driver(PHYSDEV dev, UINT version) DECLSPEC_HIDDEN;
extern void sync_gl_view(struct macdrv_win_data* data, const RECT* old_whole_rect, const RECT* old_client_rect) DECLSPEC_HIDDEN;

extern CGImageRef create_cgimage_from_icon_bitmaps(HDC hdc, HANDLE icon, HBITMAP hbmColor,
                                                   unsigned char *color_bits, int color_size, HBITMAP hbmMask,
                                                   unsigned char *mask_bits, int mask_size, int width,
                                                   int height, int istep) DECLSPEC_HIDDEN;
extern CGImageRef create_cgimage_from_icon(HANDLE icon, int width, int height) DECLSPEC_HIDDEN;
extern CFArrayRef create_app_icon_images(void) DECLSPEC_HIDDEN;

extern void macdrv_status_item_mouse_button(const macdrv_event *event) DECLSPEC_HIDDEN;
extern void macdrv_status_item_mouse_move(const macdrv_event *event) DECLSPEC_HIDDEN;

extern void check_retina_status(void) DECLSPEC_HIDDEN;
extern void macdrv_init_display_devices(BOOL force) DECLSPEC_HIDDEN;

/**************************************************************************
 * Mac IME driver
 */

extern void macdrv_process_text_input(UINT vkey, UINT scan, UINT repeat, const BYTE *key_state,
                                      void *himc, int* done) DECLSPEC_HIDDEN;

extern void macdrv_im_set_text(const macdrv_event *event) DECLSPEC_HIDDEN;
extern void macdrv_sent_text_input(const macdrv_event *event) DECLSPEC_HIDDEN;
extern BOOL query_ime_char_rect(macdrv_query* query) DECLSPEC_HIDDEN;

#endif  /* __WINE_MACDRV_H */
