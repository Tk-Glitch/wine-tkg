/*
 * X11DRV desktop window handling
 *
 * Copyright 2001 Alexandre Julliard
 * Copyright 2020 Zhiyi Zhang for CodeWeavers
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
#include <X11/cursorfont.h>
#include <X11/Xlib.h>

#define NONAMELESSSTRUCT
#define NONAMELESSUNION

#include "x11drv.h"

/* avoid conflict with field names in included win32 headers */
#undef Status
#include "wine/debug.h"
#include "wine/heap.h"

WINE_DEFAULT_DEBUG_CHANNEL(x11drv);

static unsigned int max_width;
static unsigned int max_height;
static unsigned int desktop_width;
static unsigned int desktop_height;

static struct screen_size {
    unsigned int width;
    unsigned int height;
} screen_sizes[] = {
    /* 4:3 */
    { 320,  240},
    { 400,  300},
    { 512,  384},
    { 640,  480},
    { 768,  576},
    { 800,  600},
    {1024,  768},
    {1152,  864},
    {1280,  960},
    {1400, 1050},
    {1600, 1200},
    {2048, 1536},
    /* 5:4 */
    {1280, 1024},
    {2560, 2048},
    /* 16:9 */
    {1280,  720},
    {1366,  768},
    {1600,  900},
    {1920, 1080},
    {2560, 1440},
    {3840, 2160},
    /* 16:10 */
    { 320,  200},
    { 640,  400},
    {1280,  800},
    {1440,  900},
    {1680, 1050},
    {1920, 1200},
    {2560, 1600}
};

#define _NET_WM_STATE_REMOVE 0
#define _NET_WM_STATE_ADD 1

/* Return TRUE if Wine is currently in virtual desktop mode */
BOOL is_virtual_desktop(void)
{
    return root_window != DefaultRootWindow( gdi_display );
}

/* Virtual desktop display settings handler */
static BOOL X11DRV_desktop_get_id( const WCHAR *device_name, ULONG_PTR *id )
{
    WCHAR primary_adapter[CCHDEVICENAME];

    if (!get_primary_adapter( primary_adapter ) || lstrcmpiW( primary_adapter, device_name ))
        return FALSE;

    *id = 0;
    return TRUE;
}

static void add_desktop_mode( DEVMODEW *mode, DWORD depth, DWORD width, DWORD height )
{
    mode->dmSize = sizeof(*mode);
    mode->dmFields = DM_DISPLAYORIENTATION | DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT |
                     DM_DISPLAYFLAGS | DM_DISPLAYFREQUENCY;
    mode->u1.s2.dmDisplayOrientation = DMDO_DEFAULT;
    mode->dmBitsPerPel = depth;
    mode->dmPelsWidth = width;
    mode->dmPelsHeight = height;
    mode->u2.dmDisplayFlags = 0;
    mode->dmDisplayFrequency = 60;
}

static BOOL X11DRV_desktop_get_modes( ULONG_PTR id, DWORD flags, DEVMODEW **new_modes, UINT *mode_count )
{
    UINT depth_idx, size_idx, mode_idx = 0;
    UINT screen_width, screen_height;
    RECT primary_rect;
    DEVMODEW *modes;

    primary_rect = get_primary_monitor_rect();
    screen_width = primary_rect.right - primary_rect.left;
    screen_height = primary_rect.bottom - primary_rect.top;

    /* Allocate memory for modes in different color depths */
    if (!(modes = heap_calloc( (ARRAY_SIZE(screen_sizes) + 2) * DEPTH_COUNT, sizeof(*modes))) )
    {
        SetLastError( ERROR_NOT_ENOUGH_MEMORY );
        return FALSE;
    }

    for (depth_idx = 0; depth_idx < DEPTH_COUNT; ++depth_idx)
    {
        for (size_idx = 0; size_idx < ARRAY_SIZE(screen_sizes); ++size_idx)
        {
            if (screen_sizes[size_idx].width > max_width ||
                screen_sizes[size_idx].height > max_height)
                continue;

            if (screen_sizes[size_idx].width == max_width &&
                screen_sizes[size_idx].height == max_height)
                continue;

            if (screen_sizes[size_idx].width == screen_width &&
                screen_sizes[size_idx].height == screen_height)
                continue;

            add_desktop_mode( &modes[mode_idx++], depths[depth_idx], screen_sizes[size_idx].width,
                              screen_sizes[size_idx].height );
        }

        add_desktop_mode( &modes[mode_idx++], depths[depth_idx], screen_width, screen_height );
        if (max_width != screen_width || max_height != screen_height)
            add_desktop_mode( &modes[mode_idx++], depths[depth_idx], max_width, max_height );
    }

    *new_modes = modes;
    *mode_count = mode_idx;
    return TRUE;
}

static void X11DRV_desktop_free_modes( DEVMODEW *modes )
{
    heap_free( modes );
}

static BOOL X11DRV_desktop_get_current_mode( ULONG_PTR id, DEVMODEW *mode )
{
    RECT primary_rect = get_primary_monitor_rect();

    mode->dmFields = DM_DISPLAYORIENTATION | DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT |
                     DM_DISPLAYFLAGS | DM_DISPLAYFREQUENCY | DM_POSITION;
    mode->u1.s2.dmDisplayOrientation = DMDO_DEFAULT;
    mode->dmBitsPerPel = screen_bpp;
    mode->dmPelsWidth = primary_rect.right - primary_rect.left;
    mode->dmPelsHeight = primary_rect.bottom - primary_rect.top;
    mode->u2.dmDisplayFlags = 0;
    mode->dmDisplayFrequency = 60;
    mode->u1.s2.dmPosition.x = 0;
    mode->u1.s2.dmPosition.y = 0;
    return TRUE;
}

static LONG X11DRV_desktop_set_current_mode( ULONG_PTR id, DEVMODEW *mode )
{
    if (mode->dmFields & DM_BITSPERPEL && mode->dmBitsPerPel != screen_bpp)
        WARN("Cannot change screen color depth from %dbits to %dbits!\n", screen_bpp, mode->dmBitsPerPel);

    desktop_width = mode->dmPelsWidth;
    desktop_height = mode->dmPelsHeight;
    return DISP_CHANGE_SUCCESSFUL;
}

static void query_desktop_work_area( RECT *rc_work )
{
    static const WCHAR trayW[] = {'S','h','e','l','l','_','T','r','a','y','W','n','d',0};
    RECT rect;
    HWND hwnd = FindWindowW( trayW, NULL );

    if (!hwnd || !IsWindowVisible( hwnd )) return;
    if (!GetWindowRect( hwnd, &rect )) return;
    if (rect.top) rc_work->bottom = rect.top;
    else rc_work->top = rect.bottom;
    TRACE( "found tray %p %s work area %s\n", hwnd, wine_dbgstr_rect( &rect ), wine_dbgstr_rect( rc_work ) );
}

static BOOL X11DRV_desktop_get_gpus( struct x11drv_gpu **new_gpus, int *count )
{
    static const WCHAR wine_adapterW[] = {'W','i','n','e',' ','A','d','a','p','t','e','r',0};
    struct x11drv_gpu *gpu;

    gpu = heap_calloc( 1, sizeof(*gpu) );
    if (!gpu) return FALSE;

    if (!get_host_primary_gpu( gpu ))
    {
        WARN( "Failed to get host primary gpu.\n" );
        lstrcpyW( gpu->name, wine_adapterW );
    }

    *new_gpus = gpu;
    *count = 1;
    return TRUE;
}

static void X11DRV_desktop_free_gpus( struct x11drv_gpu *gpus )
{
    heap_free( gpus );
}

/* TODO: Support multi-head virtual desktop */
static BOOL X11DRV_desktop_get_adapters( ULONG_PTR gpu_id, struct x11drv_adapter **new_adapters, int *count )
{
    struct x11drv_adapter *adapter;

    adapter = heap_calloc( 1, sizeof(*adapter) );
    if (!adapter) return FALSE;

    adapter->state_flags = DISPLAY_DEVICE_PRIMARY_DEVICE;
    if (desktop_width && desktop_height)
        adapter->state_flags |= DISPLAY_DEVICE_ATTACHED_TO_DESKTOP;

    *new_adapters = adapter;
    *count = 1;
    return TRUE;
}

static void X11DRV_desktop_free_adapters( struct x11drv_adapter *adapters )
{
    heap_free( adapters );
}

static BOOL X11DRV_desktop_get_monitors( ULONG_PTR adapter_id, struct x11drv_monitor **new_monitors, int *count )
{
    static const WCHAR generic_nonpnp_monitorW[] = {
        'G','e','n','e','r','i','c',' ',
        'N','o','n','-','P','n','P',' ','M','o','n','i','t','o','r',0};
    struct x11drv_monitor *monitor;

    monitor = heap_calloc( 1, sizeof(*monitor) );
    if (!monitor) return FALSE;

    lstrcpyW( monitor->name, generic_nonpnp_monitorW );
    SetRect( &monitor->rc_monitor, 0, 0, desktop_width, desktop_height );
    SetRect( &monitor->rc_work, 0, 0, desktop_width, desktop_height );
    query_desktop_work_area( &monitor->rc_work );
    monitor->state_flags = DISPLAY_DEVICE_ATTACHED;
    if (desktop_width && desktop_height)
        monitor->state_flags |= DISPLAY_DEVICE_ACTIVE;

    *new_monitors = monitor;
    *count = 1;
    return TRUE;
}

static void X11DRV_desktop_free_monitors( struct x11drv_monitor *monitors )
{
    heap_free( monitors );
}

/***********************************************************************
 *		X11DRV_init_desktop
 *
 * Setup the desktop when not using the root window.
 */
void X11DRV_init_desktop( Window win, unsigned int width, unsigned int height )
{
    RECT primary_rect = get_host_primary_monitor_rect();
    struct x11drv_settings_handler settings_handler;

    root_window = win;
    managed_mode = FALSE;  /* no managed windows in desktop mode */
    desktop_width = width;
    desktop_height = height;
    max_width = primary_rect.right;
    max_height = primary_rect.bottom;

    /* Initialize virtual desktop mode display device handler */
    desktop_handler.name = "Virtual Desktop";
    desktop_handler.get_gpus = X11DRV_desktop_get_gpus;
    desktop_handler.get_adapters = X11DRV_desktop_get_adapters;
    desktop_handler.get_monitors = X11DRV_desktop_get_monitors;
    desktop_handler.free_gpus = X11DRV_desktop_free_gpus;
    desktop_handler.free_adapters = X11DRV_desktop_free_adapters;
    desktop_handler.free_monitors = X11DRV_desktop_free_monitors;
    desktop_handler.register_event_handlers = NULL;
    TRACE("Display device functions are now handled by: Virtual Desktop\n");
    X11DRV_DisplayDevices_Init( TRUE );

    /* Initialize virtual desktop display settings handler */
    settings_handler.name = "Virtual Desktop";
    settings_handler.priority = 1000;
    settings_handler.get_id = X11DRV_desktop_get_id;
    settings_handler.get_modes = X11DRV_desktop_get_modes;
    settings_handler.free_modes = X11DRV_desktop_free_modes;
    settings_handler.get_current_mode = X11DRV_desktop_get_current_mode;
    settings_handler.set_current_mode = X11DRV_desktop_set_current_mode;
    X11DRV_Settings_SetHandler( &settings_handler );
}


/***********************************************************************
 *		X11DRV_create_desktop
 *
 * Create the X11 desktop window for the desktop mode.
 */
BOOL CDECL X11DRV_create_desktop( UINT width, UINT height )
{
    static const WCHAR rootW[] = {'r','o','o','t',0};
    XSetWindowAttributes win_attr;
    Window win;
    Display *display = thread_init_display();
    WCHAR name[MAX_PATH];

    if (!GetUserObjectInformationW( GetThreadDesktop( GetCurrentThreadId() ),
                                    UOI_NAME, name, sizeof(name), NULL ))
        name[0] = 0;

    TRACE( "%s %ux%u\n", debugstr_w(name), width, height );

    /* magic: desktop "root" means use the root window */
    if (!lstrcmpiW( name, rootW )) return FALSE;

    /* Create window */
    win_attr.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask | EnterWindowMask |
                          PointerMotionMask | ButtonPressMask | ButtonReleaseMask | FocusChangeMask;
    win_attr.cursor = XCreateFontCursor( display, XC_top_left_arrow );

    if (default_visual.visual != DefaultVisual( display, DefaultScreen(display) ))
        win_attr.colormap = XCreateColormap( display, DefaultRootWindow(display),
                                             default_visual.visual, AllocNone );
    else
        win_attr.colormap = None;

    win = XCreateWindow( display, DefaultRootWindow(display),
                         0, 0, width, height, 0, default_visual.depth, InputOutput, default_visual.visual,
                         CWEventMask | CWCursor | CWColormap, &win_attr );
    if (!win) return FALSE;
    if (!create_desktop_win_data( win )) return FALSE;

    X11DRV_init_desktop( win, width, height );
    if (is_desktop_fullscreen())
    {
        TRACE("setting desktop to fullscreen\n");
        XChangeProperty( display, win, x11drv_atom(_NET_WM_STATE), XA_ATOM, 32,
            PropModeReplace, (unsigned char*)&x11drv_atom(_NET_WM_STATE_FULLSCREEN),
            1);
    }
    XFlush( display );
    return TRUE;
}

BOOL is_desktop_fullscreen(void)
{
    RECT primary_rect = get_primary_monitor_rect();
    return (primary_rect.right - primary_rect.left == max_width &&
            primary_rect.bottom - primary_rect.top == max_height);
}

static void update_desktop_fullscreen( unsigned int width, unsigned int height)
{
    Display *display = thread_display();
    XEvent xev;

    if (!display || !is_virtual_desktop()) return;

    xev.xclient.type = ClientMessage;
    xev.xclient.window = root_window;
    xev.xclient.message_type = x11drv_atom(_NET_WM_STATE);
    xev.xclient.serial = 0;
    xev.xclient.display = display;
    xev.xclient.send_event = True;
    xev.xclient.format = 32;
    if (width == max_width && height == max_height)
        xev.xclient.data.l[0] = _NET_WM_STATE_ADD;
    else
        xev.xclient.data.l[0] = _NET_WM_STATE_REMOVE;
    xev.xclient.data.l[1] = x11drv_atom(_NET_WM_STATE_FULLSCREEN);
    xev.xclient.data.l[2] = 0;
    xev.xclient.data.l[3] = 1;

    TRACE("action=%li\n", xev.xclient.data.l[0]);

    XSendEvent( display, DefaultRootWindow(display), False,
                SubstructureRedirectMask | SubstructureNotifyMask, &xev );

    xev.xclient.data.l[1] = x11drv_atom(_NET_WM_STATE_MAXIMIZED_VERT);
    xev.xclient.data.l[2] = x11drv_atom(_NET_WM_STATE_MAXIMIZED_HORZ);
    XSendEvent( display, DefaultRootWindow(display), False,
                SubstructureRedirectMask | SubstructureNotifyMask, &xev );
}

/***********************************************************************
 *		X11DRV_resize_desktop
 */
void X11DRV_resize_desktop( BOOL send_display_change )
{
    RECT primary_rect, virtual_rect;
    HWND hwnd = GetDesktopWindow();
    INT width, height;

    virtual_rect = get_virtual_screen_rect();
    primary_rect = get_primary_monitor_rect();
    width = primary_rect.right;
    height = primary_rect.bottom;

    if (GetWindowThreadProcessId( hwnd, NULL ) != GetCurrentThreadId())
    {
        SendMessageW( hwnd, WM_X11DRV_RESIZE_DESKTOP, 0, (LPARAM)send_display_change );
    }
    else
    {
        TRACE( "desktop %p change to (%dx%d)\n", hwnd, width, height );
        update_desktop_fullscreen( width, height );
        SetWindowPos( hwnd, 0, virtual_rect.left, virtual_rect.top,
                      virtual_rect.right - virtual_rect.left, virtual_rect.bottom - virtual_rect.top,
                      SWP_NOZORDER | SWP_NOACTIVATE | SWP_DEFERERASE );
        ungrab_clipping_window();

        if (send_display_change)
        {
            SendMessageTimeoutW( HWND_BROADCAST, WM_DISPLAYCHANGE, screen_bpp, MAKELPARAM( width, height ),
                                 SMTO_ABORTIFHUNG, 2000, NULL );
        }
    }
}
