/*
 * Xinerama support
 *
 * Copyright 2006 Alexandre Julliard
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
#include "wine/port.h"

#include <stdarg.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#ifdef HAVE_X11_EXTENSIONS_XINERAMA_H
#include <X11/extensions/Xinerama.h>
#endif
#include "x11drv.h"
#include "wine/debug.h"
#include "wine/heap.h"
#include "wine/unicode.h"

WINE_DEFAULT_DEBUG_CHANNEL(x11drv);

static MONITORINFOEXW default_monitor =
{
    sizeof(default_monitor),    /* cbSize */
    { 0, 0, 0, 0 },             /* rcMonitor */
    { 0, 0, 0, 0 },             /* rcWork */
    MONITORINFOF_PRIMARY,       /* dwFlags */
    { '\\','\\','.','\\','D','I','S','P','L','A','Y','1',0 }   /* szDevice */
};

static MONITORINFOEXW *monitors;
static int nb_monitors;

static inline MONITORINFOEXW *get_primary(void)
{
    /* default to 0 if specified primary is invalid */
    int idx = primary_monitor;
    if (idx >= nb_monitors) idx = 0;
    return &monitors[idx];
}

#ifdef SONAME_LIBXINERAMA

#define MAKE_FUNCPTR(f) static typeof(f) * p##f

MAKE_FUNCPTR(XineramaQueryExtension);
MAKE_FUNCPTR(XineramaQueryScreens);

static void load_xinerama(void)
{
    void *handle;

    if (!(handle = dlopen(SONAME_LIBXINERAMA, RTLD_NOW)))
    {
        WARN( "failed to open %s\n", SONAME_LIBXINERAMA );
        return;
    }
    pXineramaQueryExtension = dlsym( handle, "XineramaQueryExtension" );
    if (!pXineramaQueryExtension) WARN( "XineramaQueryScreens not found\n" );
    pXineramaQueryScreens = dlsym( handle, "XineramaQueryScreens" );
    if (!pXineramaQueryScreens) WARN( "XineramaQueryScreens not found\n" );
}

static int query_screens(void)
{
    int i, count, event_base, error_base;
    XineramaScreenInfo *screens;

    if (!monitors)  /* first time around */
        load_xinerama();

    if (!pXineramaQueryExtension || !pXineramaQueryScreens ||
        !pXineramaQueryExtension( gdi_display, &event_base, &error_base ) ||
        !(screens = pXineramaQueryScreens( gdi_display, &count ))) return 0;

    if (monitors != &default_monitor) HeapFree( GetProcessHeap(), 0, monitors );
    if ((monitors = HeapAlloc( GetProcessHeap(), 0, count * sizeof(*monitors) )))
    {
        nb_monitors = count;
        for (i = 0; i < nb_monitors; i++)
        {
            monitors[i].cbSize = sizeof( monitors[i] );
            monitors[i].rcMonitor.left   = screens[i].x_org;
            monitors[i].rcMonitor.top    = screens[i].y_org;
            monitors[i].rcMonitor.right  = screens[i].x_org + screens[i].width;
            monitors[i].rcMonitor.bottom = screens[i].y_org + screens[i].height;
            monitors[i].dwFlags          = 0;
            monitors[i].rcWork           = get_work_area( &monitors[i].rcMonitor );
        }

        get_primary()->dwFlags |= MONITORINFOF_PRIMARY;
    }
    else count = 0;

    XFree( screens );
    return count;
}

#else  /* SONAME_LIBXINERAMA */

static inline int query_screens(void)
{
    return 0;
}

#endif  /* SONAME_LIBXINERAMA */

static BOOL xinerama_get_gpus( struct x11drv_gpu **new_gpus, int *count )
{
    static const WCHAR wine_adapterW[] = {'W','i','n','e',' ','A','d','a','p','t','e','r',0};
    struct x11drv_gpu *gpus;

    /* Xinerama has no support for GPU, faking one */
    gpus = heap_calloc( 1, sizeof(*gpus) );
    if (!gpus)
        return FALSE;

    lstrcpyW( gpus[0].name, wine_adapterW );

    *new_gpus = gpus;
    *count = 1;

    return TRUE;
}

static void xinerama_free_gpus( struct x11drv_gpu *gpus )
{
    heap_free( gpus );
}

static BOOL xinerama_get_adapters( ULONG_PTR gpu_id, struct x11drv_adapter **new_adapters, int *count )
{
    struct x11drv_adapter *adapters = NULL;
    INT index = 0;
    INT i, j;
    INT primary_index;
    BOOL mirrored;

    if (gpu_id)
        return FALSE;

    /* Being lazy, actual adapter count may be less */
    adapters = heap_calloc( nb_monitors, sizeof(*adapters) );
    if (!adapters)
        return FALSE;

    primary_index = primary_monitor;
    if (primary_index >= nb_monitors)
        primary_index = 0;

    for (i = 0; i < nb_monitors; i++)
    {
        mirrored = FALSE;
        for (j = 0; j < i; j++)
        {
            if (EqualRect( &monitors[i].rcMonitor, &monitors[j].rcMonitor) && !IsRectEmpty( &monitors[j].rcMonitor ))
            {
                mirrored = TRUE;
                break;
            }
        }

        /* Mirrored monitors share the same adapter */
        if (mirrored)
            continue;

        /* Use monitor index as id */
        adapters[index].id = (ULONG_PTR)i;

        if (i == primary_index)
            adapters[index].state_flags |= DISPLAY_DEVICE_PRIMARY_DEVICE;

        if (!IsRectEmpty( &monitors[i].rcMonitor ))
            adapters[index].state_flags |= DISPLAY_DEVICE_ATTACHED_TO_DESKTOP;

        index++;
    }

    /* Primary adapter has to be first */
    if (primary_index)
    {
        struct x11drv_adapter tmp;
        tmp = adapters[primary_index];
        adapters[primary_index] = adapters[0];
        adapters[0] = tmp;
    }

    *new_adapters = adapters;
    *count = index;
    return TRUE;
}

static void xinerama_free_adapters( struct x11drv_adapter *adapters )
{
    heap_free( adapters );
}

static BOOL xinerama_get_monitors( ULONG_PTR adapter_id, struct x11drv_monitor **new_monitors, int *count )
{
    static const WCHAR generic_nonpnp_monitorW[] = {
        'G','e','n','e','r','i','c',' ',
        'N','o','n','-','P','n','P',' ','M','o','n','i','t','o','r',0};
    struct x11drv_monitor *monitor;
    INT first = (INT)adapter_id;
    INT monitor_count = 0;
    INT index = 0;
    INT i;

    for (i = first; i < nb_monitors; i++)
    {
        if (i == first
            || (EqualRect( &monitors[i].rcMonitor, &monitors[first].rcMonitor )
                && !IsRectEmpty( &monitors[first].rcMonitor )))
            monitor_count++;
    }

    monitor = heap_calloc( monitor_count, sizeof(*monitor) );
    if (!monitor)
        return FALSE;

    for (i = first; i < nb_monitors; i++)
    {
        if (i == first
            || (EqualRect( &monitors[i].rcMonitor, &monitors[first].rcMonitor )
                && !IsRectEmpty( &monitors[first].rcMonitor )))
        {
            lstrcpyW( monitor[index].name, generic_nonpnp_monitorW );
            monitor[index].rc_monitor = monitors[i].rcMonitor;
            monitor[index].rc_work = monitors[i].rcWork;
            /* Xinerama only reports monitors already attached */
            monitor[index].state_flags = DISPLAY_DEVICE_ATTACHED;
            if (!IsRectEmpty( &monitors[i].rcMonitor ))
                monitor[index].state_flags |= DISPLAY_DEVICE_ACTIVE;

            index++;
        }
    }

    *new_monitors = monitor;
    *count = monitor_count;
    return TRUE;
}

static void xinerama_free_monitors( struct x11drv_monitor *monitors )
{
    heap_free( monitors );
}

void xinerama_init( unsigned int width, unsigned int height )
{
    struct x11drv_display_device_handler handler;
    MONITORINFOEXW *primary;
    int i;
    RECT rect;

    if (is_virtual_desktop())
        return;

    SetRect( &rect, 0, 0, width, height );
    if (!query_screens())
    {
        default_monitor.rcMonitor = rect;
        default_monitor.rcWork = get_work_area( &default_monitor.rcMonitor );
        nb_monitors = 1;
        monitors = &default_monitor;
    }

    primary = get_primary();

    /* coordinates (0,0) have to point to the primary monitor origin */
    OffsetRect( &rect, -primary->rcMonitor.left, -primary->rcMonitor.top );
    for (i = 0; i < nb_monitors; i++)
    {
        OffsetRect( &monitors[i].rcMonitor, rect.left, rect.top );
        OffsetRect( &monitors[i].rcWork, rect.left, rect.top );
        TRACE( "monitor 0x%x: %s work %s%s\n",
               i, wine_dbgstr_rect(&monitors[i].rcMonitor),
               wine_dbgstr_rect(&monitors[i].rcWork),
               (monitors[i].dwFlags & MONITORINFOF_PRIMARY) ? " (primary)" : "" );
    }

    handler.name = "Xinerama";
    handler.priority = 100;
    handler.get_gpus = xinerama_get_gpus;
    handler.get_adapters = xinerama_get_adapters;
    handler.get_monitors = xinerama_get_monitors;
    handler.free_gpus = xinerama_free_gpus;
    handler.free_adapters = xinerama_free_adapters;
    handler.free_monitors = xinerama_free_monitors;
    handler.register_event_handlers = NULL;
    X11DRV_DisplayDevices_SetHandler( &handler );
}
