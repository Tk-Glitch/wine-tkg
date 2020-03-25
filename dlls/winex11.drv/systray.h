/*
 * X11 driver systray-specific definitions
 *
 * Copyright (C) 2004 Mike Hearn, for CodeWeavers
 * Copyright (C) 2005 Robert Shearman
 * Copyright (C) 2008 Alexandre Julliard
 * Copyright (C) 2015 Alexey Minnekhanov
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

#ifndef __WINE_SYSTRAY_H
#define __WINE_SYSTRAY_H


#include "windef.h"
#include "wine/list.h"


/* This file contains  declarations shared between systray.c and
 * systray_dbus.c (to avoid modification of x11drv.h) */


#define BALLOON_CREATE_TIMEOUT   2000
#define BALLOON_SHOW_MIN_TIMEOUT 10000
#define BALLOON_SHOW_MAX_TIMEOUT 30000

#define TOOL_TIP_LEN 128

/* an individual systray icon */
struct tray_icon
{
    struct list    entry;
    HICON          image;    /* the image to render */
    HWND           owner;    /* the HWND passed in to the Shell_NotifyIcon call */
    HWND           window;   /* the adaptor window */
    BOOL           layered;  /* whether we are using a layered window */
    HWND           tooltip;  /* Icon tooltip */
    UINT           state;    /* state flags */
    UINT           id;       /* the unique id given by the app */
    UINT           callback_message;
    int            display;  /* display index, or -1 if hidden */
    WCHAR          tiptext[TOOL_TIP_LEN]; /* tooltip text */
    WCHAR          info_text[256];  /* info balloon text */
    WCHAR          info_title[64];  /* info balloon title */
    UINT           info_flags;      /* flags for info balloon */
    UINT           info_timeout;    /* timeout for info balloon */
    HICON          info_icon;       /* info balloon icon */
    /* DBus SNI specific members */
    char           dbus_name[64];  /* name of dbus object */
    char           sni_title[64];  /* title of status notifier */
    /* For SNI we must keep icon image this way, as an array of bitmap bits */
    int            icon_w;     /* tray icon width */
    int            icon_h;     /* tray icon height */
    unsigned char *icon_data;  /* tray icon pixels */
    UINT           version;         /* notify icon api version */
};


extern BOOL can_use_dbus_sni_systray(void) DECLSPEC_HIDDEN;

extern BOOL add_sni_icon(NOTIFYICONDATAW *nid) DECLSPEC_HIDDEN;
extern BOOL modify_sni_icon(NOTIFYICONDATAW *nid) DECLSPEC_HIDDEN;
extern BOOL delete_sni_icon(NOTIFYICONDATAW *nid) DECLSPEC_HIDDEN;


#endif /* __WINE_SYSTRAY_H */
