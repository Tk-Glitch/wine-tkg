/*
 * Android driver definitions
 *
 * Copyright 2013 Alexandre Julliard
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

#ifndef __WINE_ANDROID_H
#define __WINE_ANDROID_H

#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <jni.h>
#include <android/log.h>
#include <android/input.h>
#include <android/native_window_jni.h>

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "wine/gdi_driver.h"
#include "android_native.h"


/**************************************************************************
 * Android interface
 */

#define DECL_FUNCPTR(f) extern typeof(f) * p##f DECLSPEC_HIDDEN
DECL_FUNCPTR( __android_log_print );
DECL_FUNCPTR( ANativeWindow_fromSurface );
DECL_FUNCPTR( ANativeWindow_release );
#undef DECL_FUNCPTR


/**************************************************************************
 * OpenGL driver
 */

extern void update_gl_drawable( HWND hwnd ) DECLSPEC_HIDDEN;
extern void destroy_gl_drawable( HWND hwnd ) DECLSPEC_HIDDEN;
extern struct opengl_funcs *get_wgl_driver( UINT version ) DECLSPEC_HIDDEN;


/**************************************************************************
 * Android pseudo-device
 */

extern void start_android_device(void) DECLSPEC_HIDDEN;
extern void register_native_window( HWND hwnd, struct ANativeWindow *win, BOOL client ) DECLSPEC_HIDDEN;
extern struct ANativeWindow *create_ioctl_window( HWND hwnd, BOOL opengl, float scale ) DECLSPEC_HIDDEN;
extern struct ANativeWindow *grab_ioctl_window( struct ANativeWindow *window ) DECLSPEC_HIDDEN;
extern void release_ioctl_window( struct ANativeWindow *window ) DECLSPEC_HIDDEN;
extern void destroy_ioctl_window( HWND hwnd, BOOL opengl ) DECLSPEC_HIDDEN;
extern int ioctl_window_pos_changed( HWND hwnd, const RECT *window_rect, const RECT *client_rect,
                                     const RECT *visible_rect, UINT style, UINT flags,
                                     HWND after, HWND owner ) DECLSPEC_HIDDEN;
extern int ioctl_set_window_parent( HWND hwnd, HWND parent, float scale ) DECLSPEC_HIDDEN;
extern int ioctl_set_capture( HWND hwnd ) DECLSPEC_HIDDEN;
extern int ioctl_set_cursor( int id, int width, int height,
                             int hotspotx, int hotspoty, const unsigned int *bits ) DECLSPEC_HIDDEN;


/**************************************************************************
 * USER driver
 */

extern unsigned int screen_width DECLSPEC_HIDDEN;
extern unsigned int screen_height DECLSPEC_HIDDEN;
extern RECT virtual_screen_rect DECLSPEC_HIDDEN;
extern MONITORINFOEXW default_monitor DECLSPEC_HIDDEN;

enum android_window_messages
{
    WM_ANDROID_REFRESH = 0x80001000,
};

extern void init_gralloc( const struct hw_module_t *module ) DECLSPEC_HIDDEN;
extern HWND get_capture_window(void) DECLSPEC_HIDDEN;
extern void init_monitors( int width, int height ) DECLSPEC_HIDDEN;
extern void set_screen_dpi( DWORD dpi ) DECLSPEC_HIDDEN;
extern void update_keyboard_lock_state( WORD vkey, UINT state ) DECLSPEC_HIDDEN;

/* JNI entry points */
extern void desktop_changed( JNIEnv *env, jobject obj, jint width, jint height ) DECLSPEC_HIDDEN;
extern void config_changed( JNIEnv *env, jobject obj, jint dpi ) DECLSPEC_HIDDEN;
extern void surface_changed( JNIEnv *env, jobject obj, jint win, jobject surface,
                             jboolean client ) DECLSPEC_HIDDEN;
extern jboolean motion_event( JNIEnv *env, jobject obj, jint win, jint action,
                              jint x, jint y, jint state, jint vscroll ) DECLSPEC_HIDDEN;
extern jboolean keyboard_event( JNIEnv *env, jobject obj, jint win, jint action,
                                jint keycode, jint state ) DECLSPEC_HIDDEN;

enum event_type
{
    DESKTOP_CHANGED,
    CONFIG_CHANGED,
    SURFACE_CHANGED,
    MOTION_EVENT,
    KEYBOARD_EVENT,
};

union event_data
{
    enum event_type type;
    struct
    {
        enum event_type type;
        unsigned int    width;
        unsigned int    height;
    } desktop;
    struct
    {
        enum event_type type;
        unsigned int    dpi;
    } cfg;
    struct
    {
        enum event_type type;
        HWND            hwnd;
        ANativeWindow  *window;
        BOOL            client;
        unsigned int    width;
        unsigned int    height;
    } surface;
    struct
    {
        enum event_type type;
        HWND            hwnd;
        INPUT           input;
    } motion;
    struct
    {
        enum event_type type;
        HWND            hwnd;
        UINT            lock_state;
        INPUT           input;
    } kbd;
};

int send_event( const union event_data *data ) DECLSPEC_HIDDEN;

extern JavaVM **p_java_vm;
extern jobject *p_java_object;
extern unsigned short *p_java_gdt_sel;

#endif  /* __WINE_ANDROID_H */
