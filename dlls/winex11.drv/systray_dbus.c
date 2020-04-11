/*
 * DBus StatusNotifierItem system tray management,
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

/*
 * This file holds all functionality to implement all DBus-related
 * operations to implement StatusNotifierItem specification
 * (see http://www.freedesktop.org/wiki/Specifications/StatusNotifierItem/ )
 * Keep it separated from systray.c if possible.
 * Functions in this module will be called from systray.c,
 * from wine_notify_icon(). Module public module interface:
 *
 *  - BOOL can_use_dbus_sni_systray(void) - checks if SNI systray can be used
 *                                          in this system.
 *  - BOOL add_sni_icon( NOTIFYICONDATAW* )    - add new systray icon
 *  - BOOL modify_sni_icon( NOTIFYICONDATAW* ) - all edit icon operations
 *  - BOOL delete_sni_icon( NOTIFYICONDATAW* ) - remove status notifier item
 */

#include "config.h"
#include "wine/port.h"

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef SONAME_LIBDBUS_1
# include <dbus/dbus.h>
#endif

#define NONAMELESSUNION
#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "shellapi.h"

#include "wine/debug.h"
#include "wine/library.h"
#include "wine/list.h"
#include "wine/unicode.h"

#include "systray.h"
#include "x11drv.h"


WINE_DEFAULT_DEBUG_CHANNEL(systray);


#ifdef SONAME_LIBDBUS_1

/* Similar code to load DBus is used in dlls/mountmgr.sys/dbus.c  */

#define DBUS_FUNCS \
    DO_FUNC(dbus_bus_get); \
    DO_FUNC(dbus_bus_get_unique_name); \
    DO_FUNC(dbus_bus_name_has_owner); \
    DO_FUNC(dbus_bus_release_name); \
    DO_FUNC(dbus_bus_request_name); \
    DO_FUNC(dbus_connection_read_write_dispatch); \
    DO_FUNC(dbus_connection_send); \
    DO_FUNC(dbus_connection_send_with_reply_and_block); \
    DO_FUNC(dbus_connection_set_exit_on_disconnect); \
    DO_FUNC(dbus_connection_try_register_fallback); \
    DO_FUNC(dbus_connection_unref); \
    DO_FUNC(dbus_connection_unregister_object_path); \
    DO_FUNC(dbus_error_free); \
    DO_FUNC(dbus_error_init); \
    DO_FUNC(dbus_message_append_args); \
    DO_FUNC(dbus_message_get_args); \
    DO_FUNC(dbus_message_get_destination); \
    DO_FUNC(dbus_message_get_interface); \
    DO_FUNC(dbus_message_get_member); \
    DO_FUNC(dbus_message_get_path); \
    DO_FUNC(dbus_message_get_signature); \
    DO_FUNC(dbus_message_is_method_call); \
    DO_FUNC(dbus_message_iter_append_basic); \
    DO_FUNC(dbus_message_iter_close_container); \
    DO_FUNC(dbus_message_iter_get_arg_type); \
    DO_FUNC(dbus_message_iter_get_basic); \
    DO_FUNC(dbus_message_iter_init); \
    DO_FUNC(dbus_message_iter_init_append); \
    DO_FUNC(dbus_message_iter_next); \
    DO_FUNC(dbus_message_iter_open_container); \
    DO_FUNC(dbus_message_iter_recurse); \
    DO_FUNC(dbus_message_new_error); \
    DO_FUNC(dbus_message_new_method_call); \
    DO_FUNC(dbus_message_new_method_return); \
    DO_FUNC(dbus_message_new_signal); \
    DO_FUNC(dbus_message_unref);

/* libdbus function pointers */
#define DO_FUNC(f) static typeof(f) * p_##f
DBUS_FUNCS;
#undef DO_FUNC


/* DBus connection handle */
static DBusConnection *d_connection = NULL;

/* structure to keep all managed icons */
static struct list icon_list = LIST_INIT( icon_list );
/* we need our own list, different from that one in systray.c
 * just to make as little modifications to existing code as possible */
static int g_icons_count = 0;

/* global that indicates DBus message loop thread state */
static BOOL g_dbus_thread_running = FALSE;

/* Critical section guarding DBus connection object */
CRITICAL_SECTION g_dconn_cs;

/* local forwards */
static BOOL delete_icon(struct tray_icon *icon);


/* DBus introspection XMLs definitions */
/* For documentation of DBus introspection XML format,
 * see http://dbus.freedesktop.org/doc/dbus-specification.html#introspection-format */

/* root object XML */
static const char *g_dbus_introspect_xml_root_with_SNI = ""
"<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\""
" \"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
"<node name=\"/\">\n"
"  <interface name=\"org.freedesktop.DBus.Introspectable\">\n"
"    <method name=\"Introspect\">\n"
"      <arg name=\"xml_data\" type=\"s\" direction=\"out\"/>\n"
"    </method>\n"
"  </interface>\n"
"  <interface name=\"org.freedesktop.DBus.Properties\">\n"
"    <method name=\"Get\">\n"
"      <arg name=\"interface_name\" type=\"s\" direction=\"in\"/>\n"
"      <arg name=\"property_name\" type=\"s\" direction=\"in\"/>\n"
"      <arg name=\"value\" type=\"v\" direction=\"out\"/>\n"
"    </method>\n"
"    <method name=\"Set\">\n"
"      <arg name=\"interface_name\" type=\"s\" direction=\"in\"/>\n"
"      <arg name=\"property_name\" type=\"s\" direction=\"in\"/>\n"
"      <arg name=\"value\" type=\"v\" direction=\"in\"/>\n"
"    </method>\n"
"    <method name=\"GetAll\">\n"
"      <arg name=\"interface_name\" type=\"s\" direction=\"in\"/>\n"
"      <arg name=\"props\" type=\"{sv}\" direction=\"out\"/>\n"
"    </method>\n"
"  </interface>\n"
"  <node name=\"StatusNotifierItem\"/>\n"
"</node>";

/* /StatusNotifierItem XML */
static const char *g_dbus_introspect_xml_SNI = "\
<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\
 \"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\
<node name=\"/StatusNotifierItem\">\
  <interface name=\"org.freedesktop.DBus.Introspectable\">\
    <method name=\"Introspect\">\
      <arg name=\"xml_data\" type=\"s\" direction=\"out\"/>\
    </method>\
  </interface>\
  <interface name=\"org.freedesktop.DBus.Properties\">\
    <method name=\"Get\">\
      <arg name=\"interface_name\" type=\"s\" direction=\"in\"/>\
      <arg name=\"property_name\" type=\"s\" direction=\"in\"/>\
      <arg name=\"value\" type=\"v\" direction=\"out\"/>\
    </method>\
    <method name=\"Set\">\
      <arg name=\"interface_name\" type=\"s\" direction=\"in\"/>\
      <arg name=\"property_name\" type=\"s\" direction=\"in\"/>\
      <arg name=\"value\" type=\"v\" direction=\"in\"/>\
    </method>\
    <method name=\"GetAll\">\
      <arg name=\"interface_name\" type=\"s\" direction=\"in\"/>\
      <arg name=\"props\" type=\"{sv}\" direction=\"out\"/>\
    </method>\
  </interface>\
  <interface name=\"org.kde.StatusNotifierItem\">\
    <property name=\"Category\" type=\"s\" access=\"read\"/>\
    <property name=\"Id\" type=\"s\" access=\"read\"/>\
    <property name=\"Title\" type=\"s\" access=\"read\"/>\
    <property name=\"Status\" type=\"s\" access=\"read\"/>\
    <property name=\"WindowId\" type=\"u\" access=\"read\"/>\
    <property name=\"IconName\" type=\"s\" access=\"read\"/>\
    <property name=\"IconPixmap\" type=\"a(iiay)\" access=\"read\"/>\
    <property name=\"OverlayIconName\" type=\"s\" access=\"read\"/>\
    <property name=\"OverlayIconPixmap\" type=\"a(iiay)\" access=\"read\"/>\
    <property name=\"AttentionIconName\" type=\"s\" access=\"read\"/>\
    <property name=\"AttentionIconPixmap\" type=\"a(iiay)\" access=\"read\"/>\
    <property name=\"AttentionMovieName\" type=\"s\" access=\"read\"/>\
    <property name=\"ToolTip\" type=\"(sa(iiay)ss)\" access=\"read\"/>\
    <method name=\"ContextMenu\">\
      <arg name=\"x\" type=\"i\" direction=\"in\"/>\
      <arg name=\"y\" type=\"i\" direction=\"in\"/>\
    </method>\
    <method name=\"Activate\">\
      <arg name=\"x\" type=\"i\" direction=\"in\"/>\
      <arg name=\"y\" type=\"i\" direction=\"in\"/>\
    </method>\
    <method name=\"SecondaryActivate\">\
      <arg name=\"x\" type=\"i\" direction=\"in\"/>\
      <arg name=\"y\" type=\"i\" direction=\"in\"/>\
    </method>\
    <method name=\"Scroll\">\
      <arg name=\"delta\" type=\"i\" direction=\"in\"/>\
      <arg name=\"orientation\" type=\"s\" direction=\"in\"/>\
    </method>\
    <signal name=\"NewTitle\"></signal>\
    <signal name=\"NewIcon\"></signal>\
    <signal name=\"NewAttentionIcon\"></signal>\
    <signal name=\"NewOverlayIcon\"></signal>\
    <signal name=\"NewToolTip\"></signal>\
    <signal name=\"NewStatus\">\
      <arg name=\"status\" type=\"s\"/>\
    </signal>\
  </interface>\
</node>";


/**************************************************************
              icon_list management functions
***************************************************************/

/* Retrieves icon record by owner window and ID */
static struct tray_icon *get_icon_from_list(HWND owner, UINT id)
{
    struct tray_icon *this;

    LIST_FOR_EACH_ENTRY( this, &icon_list, struct tray_icon, entry )
        if ((this->id == id) && (this->owner == owner)) return this;
    return NULL;
}


/* Retrieves icon record by DBus name */
static struct tray_icon *get_icon_from_list_by_dname(const char *dname)
{
    struct tray_icon *this;

    LIST_FOR_EACH_ENTRY( this, &icon_list, struct tray_icon, entry )
        if (strcmp(this->dbus_name, dname) == 0) return this;
    return NULL;
}


/* Allocates memory and adds a new icon to the list */
static struct tray_icon *add_new_icon_to_list(void)
{
    struct tray_icon  *icon;

    /* allocate memory */
    if (!(icon = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*icon))))
    {
        ERR("out of memory\n");
        return FALSE;
    }
    ZeroMemory(icon, sizeof(struct tray_icon));

    /* add to list */
    list_add_tail(&icon_list, &icon->entry);
    g_icons_count++;
    return icon;
}


/* Removes icons from list and frees its memory */
static void delete_icon_from_list(struct tray_icon *icon)
{
    if (!icon) return;
    list_remove( &icon->entry );
    g_icons_count--;
    HeapFree( GetProcessHeap(), 0, icon );
}



/**************************************************************
              DBus initialization functions
***************************************************************/

static BOOL load_dbus_functions(void)
{
    void *handle;
    char error[128];

    if (!(handle = dlopen(SONAME_LIBDBUS_1, RTLD_NOW)))
        goto failed;

#define DO_FUNC(f) if (!(p_##f = dlsym( handle, #f ))) goto failed
    DBUS_FUNCS;
#undef DO_FUNC
    return TRUE;

failed:
    WARN( "failed to load DBus support: %s\n", error );
    return FALSE;
}


static BOOL initialize_dbus(void)
{
    static BOOL init_done = FALSE;
    if (init_done) return TRUE;

    if (load_dbus_functions())
    {
        InitializeCriticalSection( &g_dconn_cs );

        init_done = TRUE;
        d_connection = NULL;
    }

    TRACE( "Initialize DBUS OK\n" );
    return TRUE;
}


/*************************************************************
 *            DBus reply helper functions
**************************************************************/


/* Locks connection lock, sends message and unlocks the lock.
 * Safe to use with message loop thread running. */
static dbus_bool_t dbus_send_safe(DBusMessage *msg)
{
    dbus_bool_t resb;

    if (!d_connection) return FALSE;
    if (!msg) return FALSE;

    EnterCriticalSection( &g_dconn_cs );
    resb = p_dbus_connection_send(d_connection, msg, NULL);
    LeaveCriticalSection( &g_dconn_cs );

    if (resb == FALSE)
    {
        WARN("dbus_connection_send() failed!\n");
    }

    return resb;
}


/* this creates a response message with a simple string value */
static DBusMessage *create_introspect_reply(
        DBusMessage *msg,
        const char *reply_xml)
{
    dbus_bool_t retb;
    DBusMessage *reply;

    reply = p_dbus_message_new_method_return(msg);
    if( reply )
    {
        retb = p_dbus_message_append_args(reply,
            DBUS_TYPE_STRING, &reply_xml,
            DBUS_TYPE_INVALID);
        if( retb == FALSE )
        {
            p_dbus_message_unref(reply);
            reply = NULL;
        }
    }
    return reply;
}


/* this should reply DBUS_VARIANT type with internal STRING value */
static DBusMessage *create_reply_propget_s(DBusMessage *msg, const char *s)
{
    DBusMessage *reply;
    DBusMessageIter iter, sub_iter;

    reply = p_dbus_message_new_method_return(msg);
    if( !reply ) return FALSE;

    /* we can send VARIANT only with iterator */
    p_dbus_message_iter_init_append(reply, &iter);
    p_dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "s", &sub_iter);
    p_dbus_message_iter_append_basic(&sub_iter, DBUS_TYPE_STRING, &s);
    p_dbus_message_iter_close_container(&iter, &sub_iter);
    return reply;
}


/* this should reply DBUS_VARIANT type with internal UINT32 value */
static DBusMessage *create_reply_propget_u(DBusMessage *msg, dbus_uint32_t value)
{
    DBusMessage *reply;
    DBusMessageIter iter, sub_iter;

    reply = p_dbus_message_new_method_return(msg);
    if( !reply ) return FALSE;

    /* we can send VARIANT only with iterator */
    p_dbus_message_iter_init_append(reply, &iter);
    p_dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "u", &sub_iter);
    p_dbus_message_iter_append_basic(&sub_iter, DBUS_TYPE_UINT32, &value);
    p_dbus_message_iter_close_container(&iter, &sub_iter);
    return reply;
}


static DBusMessage *create_reply_propget_pixmap(
        DBusMessage *msg,
        int icon_width,
        int icon_height,
        const unsigned char *img_data )
{
    DBusMessage *reply;
    DBusMessageIter iter, v_iter, a_iter, st_iter, pixels_arr_iter;
    int num_pixels, i;
    unsigned char pixel_byte;

    reply = p_dbus_message_new_method_return(msg);
    if( !reply ) return FALSE;

    /* variant, containing array: a(iiay)
     * array of structures: (int, int, bytearray): width, height,
     *    bits data in ARGB32 format (network byte order)
     * Reply can contain array of pixmaps in different sizes, but we send only one */
    p_dbus_message_iter_init_append(reply, &iter);
    p_dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "a(iiay)", &v_iter);
    p_dbus_message_iter_open_container(&v_iter, DBUS_TYPE_ARRAY, "(iiay)", &a_iter);
    if( (icon_height > 0) && (icon_width > 0) && (img_data)) {
        /* open structure container */
        p_dbus_message_iter_open_container(&a_iter, DBUS_TYPE_STRUCT, NULL, &st_iter);
        /* write icon width, height */
        p_dbus_message_iter_append_basic(&st_iter, DBUS_TYPE_INT32, &icon_width);
        p_dbus_message_iter_append_basic(&st_iter, DBUS_TYPE_INT32, &icon_height);
        /* write icon bytes data as array */
        /* open array */
        p_dbus_message_iter_open_container(&st_iter, DBUS_TYPE_ARRAY, "y", &pixels_arr_iter);
        /* write pixels */
        num_pixels = icon_width * icon_height;
        for( i=0; i<num_pixels * 4; i++ )  /* ARGB32 = 4 bytes per pixel */
        {
            pixel_byte = img_data[i];
            p_dbus_message_iter_append_basic(&pixels_arr_iter, DBUS_TYPE_BYTE, &pixel_byte);
        }
        p_dbus_message_iter_close_container(&st_iter, &pixels_arr_iter);
        p_dbus_message_iter_close_container(&a_iter, &st_iter);
    }
    p_dbus_message_iter_close_container(&v_iter, &a_iter);
    p_dbus_message_iter_close_container(&iter, &v_iter);
    return reply;
}


static DBusMessage *create_reply_propget_tooltip(
        DBusMessage *msg,
        const char *icon_name,
        int icon_width,
        int icon_height,
        const unsigned char *img_data,
        const char *tip_title,
        const char *tip_text)
{
    /* Format: (sa(iiay)ss)
     * Components are:
     * STRING: Freedesktop-compliant name for an icon.
     * ARRAY(INT, INT, ARRAY BYTE): icon data
     * STRING: title for this tooltip
     * STRING: descriptive text for this tooltip. */
    DBusMessage *reply;
    DBusMessageIter iter, v_iter, st_iter, arr_iter, ast_iter, pixels_arr_iter;
    int num_pixels, i;
    unsigned char pixel_byte;

    reply = p_dbus_message_new_method_return(msg);
    if( !reply ) return FALSE;

    p_dbus_message_iter_init_append(reply, &iter);
    p_dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "(sa(iiay)ss)", &v_iter);
    p_dbus_message_iter_open_container(&v_iter, DBUS_TYPE_STRUCT, NULL, &st_iter);
    p_dbus_message_iter_append_basic(&st_iter, DBUS_TYPE_STRING, &icon_name);
    p_dbus_message_iter_open_container(&st_iter, DBUS_TYPE_ARRAY, "(iiay)", &arr_iter);
    /* write icon data, if set */
    if( (icon_height > 0) && (icon_width > 0) && (img_data)) {
        p_dbus_message_iter_open_container(&arr_iter, DBUS_TYPE_STRUCT, NULL, &ast_iter);
        /* write icon width, height */
        p_dbus_message_iter_append_basic(&ast_iter, DBUS_TYPE_INT32, &icon_width);
        p_dbus_message_iter_append_basic(&ast_iter, DBUS_TYPE_INT32, &icon_height);
        /* write icon bytes data as array */
        p_dbus_message_iter_open_container(&ast_iter, DBUS_TYPE_ARRAY, "y", &pixels_arr_iter);
        /* write pixels */
        num_pixels = icon_width * icon_height;
        for( i=0; i<num_pixels * 4; i++ ) /* ARGB32 = 4 bytes per pixel */
        {
            pixel_byte = img_data[i];
            p_dbus_message_iter_append_basic(&pixels_arr_iter, DBUS_TYPE_BYTE, &pixel_byte);
        }
        p_dbus_message_iter_close_container(&ast_iter, &pixels_arr_iter);
        p_dbus_message_iter_close_container(&arr_iter, &ast_iter);
    }
    p_dbus_message_iter_close_container(&st_iter, &arr_iter);
    /* write tooltip title and text */
    p_dbus_message_iter_append_basic(&st_iter, DBUS_TYPE_STRING, &tip_title);
    p_dbus_message_iter_append_basic(&st_iter, DBUS_TYPE_STRING, &tip_text);
    p_dbus_message_iter_close_container(&v_iter, &st_iter);
    p_dbus_message_iter_close_container(&iter, &v_iter);
    return reply;
}


/* Appends dict value [attr_name] = variant(string value) to DBus
 * reply for Properties.GetAll() */
static void reply_propgetall_append_s(
        DBusMessageIter *piter,
        const char *attr_name,
        const char *value)
{
    DBusMessageIter dict_iter, var_iter;
    p_dbus_message_iter_open_container(piter, DBUS_TYPE_DICT_ENTRY, NULL, &dict_iter); /* open dict */
    p_dbus_message_iter_append_basic(&dict_iter, DBUS_TYPE_STRING, &attr_name); /* write attr name as dict key */
    p_dbus_message_iter_open_container(&dict_iter, DBUS_TYPE_VARIANT, "s", &var_iter); /* open variant */
    p_dbus_message_iter_append_basic(&var_iter, DBUS_TYPE_STRING, &value); /* write attr value as Variant(string) */
    p_dbus_message_iter_close_container(&dict_iter, &var_iter); /* close variant */
    p_dbus_message_iter_close_container(piter, &dict_iter); /* close dict */
}


/* Appends dict value [attr_name] = variant(uint32 value) to DBus
 * reply for Properties.GetAll() */
static void reply_propgetall_append_u(
        DBusMessageIter *piter,
        const char *attr_name,
        dbus_uint32_t value)
{
    DBusMessageIter dict_iter, var_iter;
    p_dbus_message_iter_open_container(piter, DBUS_TYPE_DICT_ENTRY, NULL, &dict_iter); /* open dict */
    p_dbus_message_iter_append_basic(&dict_iter, DBUS_TYPE_STRING, &attr_name); /* write attr name as dict key */
    p_dbus_message_iter_open_container(&dict_iter, DBUS_TYPE_VARIANT, "u", &var_iter); /* open variant */
    p_dbus_message_iter_append_basic(&var_iter, DBUS_TYPE_UINT32, &value); /* write attr value as Variant(uint32) */
    p_dbus_message_iter_close_container(&dict_iter, &var_iter); /* close variant */
    p_dbus_message_iter_close_container(piter, &dict_iter); /* close dict */
}


/* Appends dict value [attr_name] = variant(pixmap structure) to DBus
 * reply for Properties.GetAll() */
static void reply_propgetall_append_pixmap(
        DBusMessageIter *piter,
        const char *attr_name,
        int width,
        int height,
        const unsigned char *img_data)
{
    DBusMessageIter dict_iter, var_iter, arr_iter, st_iter, pixels_arr_iter;
    int num_pixels, i;
    unsigned char pixel_byte;

    p_dbus_message_iter_open_container(piter, DBUS_TYPE_DICT_ENTRY, NULL, &dict_iter); /* open dict */
    p_dbus_message_iter_append_basic(&dict_iter, DBUS_TYPE_STRING, &attr_name); /* write attr name as dict key */
    p_dbus_message_iter_open_container(&dict_iter, DBUS_TYPE_VARIANT, "a(iiay)", &var_iter); /* open variant */
    /* key value - pixmap array inside Variant
     * example: "AttentionIconPixmap" = [Variant: [Argument: a(iiay) {}]] */
    p_dbus_message_iter_open_container(&var_iter, DBUS_TYPE_ARRAY, "(iiay)", &arr_iter);
    if( (width > 0) && (height > 0) && (img_data)) {
        /* open structure container */
        p_dbus_message_iter_open_container(&arr_iter, DBUS_TYPE_STRUCT, NULL, &st_iter);
        /* write icon size: width, height */
        p_dbus_message_iter_append_basic(&st_iter, DBUS_TYPE_INT32, &width);
        p_dbus_message_iter_append_basic(&st_iter, DBUS_TYPE_INT32, &height);
        /* write icon bytes data as array */
        /* open array */
        p_dbus_message_iter_open_container(&st_iter, DBUS_TYPE_ARRAY, "y", &pixels_arr_iter);
        /* write pixels */
        num_pixels = width * height;
        for( i=0; i<num_pixels * 4; i++ )  /* ARGB32 = 4 bytes per pixel */
        {
            pixel_byte = img_data[i];
            p_dbus_message_iter_append_basic(&pixels_arr_iter, DBUS_TYPE_BYTE, &pixel_byte);
        }
        /* close array */
        p_dbus_message_iter_close_container(&st_iter, &pixels_arr_iter);
        /* close structure container */
        p_dbus_message_iter_close_container(&arr_iter, &st_iter);
    }
    p_dbus_message_iter_close_container(&var_iter, &arr_iter); /* close pixmaps array */
    p_dbus_message_iter_close_container(&dict_iter, &var_iter); /* close variant */
    p_dbus_message_iter_close_container(piter, &dict_iter); /* close dict */
}


/* Appends dict value [attr_name] = variant(tooltip structure) to DBus
 * reply for Properties.GetAll() */
static void reply_propgetall_append_tooltip(
        DBusMessageIter *piter,
        const char *attr_name,
        const char *icon_name,
        int width,
        int height,
        const unsigned char *img_data,
        const char *tt_title,
        const char *tt_text)
{
    DBusMessageIter dict_iter, var_iter, st_iter, arr_iter, st2_iter, pixels_arr_iter;
    int num_pixels, i;
    unsigned char pixel_byte;

    p_dbus_message_iter_open_container(piter, DBUS_TYPE_DICT_ENTRY, NULL, &dict_iter); /* open dict */
    p_dbus_message_iter_append_basic(&dict_iter, DBUS_TYPE_STRING, &attr_name); /* write attr name as dict key */
    p_dbus_message_iter_open_container(&dict_iter, DBUS_TYPE_VARIANT, "(sa(iiay)ss)", &var_iter); /* open variant */
    /* key value - struct (sa(iiay)ss) */
    p_dbus_message_iter_open_container(&var_iter, DBUS_TYPE_STRUCT, NULL, &st_iter); /* open struct */
    p_dbus_message_iter_append_basic(&st_iter, DBUS_TYPE_STRING, &icon_name); /* icon name */
    p_dbus_message_iter_open_container(&st_iter, DBUS_TYPE_ARRAY, "(iiay)", &arr_iter); /* open array */
    /* icon(s), but here only one */
    if( (width > 0) && (height > 0) && (img_data)) {
        /* open structure container */
        p_dbus_message_iter_open_container(&arr_iter, DBUS_TYPE_STRUCT, NULL, &st2_iter);
        /* write icon size: width, height */
        p_dbus_message_iter_append_basic(&st2_iter, DBUS_TYPE_INT32, &width);
        p_dbus_message_iter_append_basic(&st2_iter, DBUS_TYPE_INT32, &height);
        /* write icon bytes data as array */
        /* open array */
        p_dbus_message_iter_open_container(&st2_iter, DBUS_TYPE_ARRAY, "y", &pixels_arr_iter);
        /* write pixels */
        num_pixels = width * height;
        for( i=0; i<num_pixels * 4; i++ ) /* ARGB32 = 4 bytes per pixel */
        {
            pixel_byte = img_data[i];
            p_dbus_message_iter_append_basic(&pixels_arr_iter, DBUS_TYPE_BYTE, &pixel_byte);
        }
        /* close array */
        p_dbus_message_iter_close_container(&st2_iter, &pixels_arr_iter);
        /* close structure container */
        p_dbus_message_iter_close_container(&arr_iter, &st2_iter);
    }
    /* end icon */
    p_dbus_message_iter_close_container(&st_iter, &arr_iter); /* close array */
    p_dbus_message_iter_append_basic(&st_iter, DBUS_TYPE_STRING, &tt_title); /* tooltip title */
    p_dbus_message_iter_append_basic(&st_iter, DBUS_TYPE_STRING, &tt_text); /* tooltip text */
    p_dbus_message_iter_close_container(&var_iter, &st_iter); /* close struct */

    p_dbus_message_iter_close_container(&dict_iter, &var_iter); /* close variant */
    p_dbus_message_iter_close_container(piter, &dict_iter); /* close dict */
}


/*  DBus error replies  */


static dbus_bool_t error_reply_failed(DBusMessage *message)
{
    DBusMessage *reply;
    dbus_bool_t ret;
    reply = p_dbus_message_new_error(message, DBUS_ERROR_FAILED, "operation failed");
    if (!reply) return FALSE;
    ret = dbus_send_safe(reply);
    p_dbus_message_unref(reply);
    return ret;
}


static dbus_bool_t error_reply_unknown_interface(DBusMessage *message, const char *iface)
{
    char error_message[256] = {0};
    DBusMessage *reply;
    dbus_bool_t ret;
    snprintf(error_message, sizeof(error_message)-1, "Unknown interface: [%s]", iface);
    reply = p_dbus_message_new_error(message, DBUS_ERROR_UNKNOWN_INTERFACE, error_message);
    if (!reply) return FALSE;
    ret = dbus_send_safe(reply);
    p_dbus_message_unref(reply);
    return ret;
}


static dbus_bool_t error_reply_invalid_args(DBusMessage *message)
{
    DBusMessage *reply;
    dbus_bool_t ret;
    reply = p_dbus_message_new_error(message, DBUS_ERROR_INVALID_ARGS, "Invalid arguments!");
    if (!reply) return FALSE;
    ret = dbus_send_safe(reply);
    p_dbus_message_unref(reply);
    return ret;
}


static dbus_bool_t error_reply_unknown_property(DBusMessage *message, const char *prop_name)
{
    DBusMessage *reply;
    dbus_bool_t ret;
    char error_message[256];
    snprintf(error_message, sizeof(error_message)-1, "Unknown property: [%s]", prop_name);
    reply = p_dbus_message_new_error(message, DBUS_ERROR_UNKNOWN_PROPERTY, error_message);
    if (!reply) return FALSE;
    ret = dbus_send_safe(reply);
    p_dbus_message_unref(reply);
    return ret;
}


static dbus_bool_t error_reply_read_only_prop(DBusMessage *message, const char *prop_name)
{
    char error_message[256];
    DBusMessage *reply;
    dbus_bool_t ret;
    snprintf(error_message, sizeof(error_message)-1, "property: %s", prop_name);
    reply = p_dbus_message_new_error(message, DBUS_ERROR_PROPERTY_READ_ONLY, error_message);
    if (!reply) return FALSE;
    ret = dbus_send_safe(reply);
    p_dbus_message_unref(reply);
    return ret;
}


/*  DBus signals  */
/* Emits DBus signal named "signal_name" */
static dbus_bool_t send_signal_noargs(const char *signal_name)
{
    dbus_bool_t retb;
    DBusMessage *msg;
    msg = p_dbus_message_new_signal(
            "/StatusNotifierItem",
            "org.kde.StatusNotifierItem",
            signal_name);
    if (!msg) return FALSE;
    retb = dbus_send_safe(msg);
    p_dbus_message_unref(msg);
    return retb;
}


/* Emits DBus signal "NetStatus(status)" */
static dbus_bool_t send_signal_NewStatus(const char *status)
{
    dbus_bool_t retb;
    DBusMessage *msg;
    msg = p_dbus_message_new_signal(
            "/StatusNotifierItem",
            "org.kde.StatusNotifierItem",
            "NewStatus");
    if (!msg) return FALSE;
    /* apppend one argument - new status */
    p_dbus_message_append_args(msg,
            DBUS_TYPE_STRING, &status,
            DBUS_TYPE_INVALID);
    retb = dbus_send_safe(msg);
    p_dbus_message_unref(msg);
    return retb;
}


/* Posts callback windows message to icon owner about tray mouse event */
static void relay_windows_message(struct tray_icon *icon, UINT uMsg)
{
    BOOL ret;
    DWORD cur_time;
    /* store the last time mouse button was pressed */
    static DWORD last_L_time = 0;
    static DWORD last_M_time = 0;
    static DWORD last_R_time = 0;

    if (!icon->callback_message) return;

    TRACE("DBus: relaying 0x%x\n", uMsg);

    /* uMsg - application-defined callback message,
     * wParam - icon id (defined by application,
     * lParam - mouse message, e.g. WM_LBUTTONDOWN */
    ret = PostMessageW(icon->owner, icon->callback_message, (WPARAM)icon->id, (LPARAM)uMsg);
    /* as in systray.c:469, check that window was valid */
    if (!ret && (GetLastError() == ERROR_INVALID_WINDOW_HANDLE))
    {
        WARN( "application window was destroyed, removing icon %s\n", icon->dbus_name );
        delete_icon( icon );
    }

    /* Emulate mouse double button clicks */
    /* StatusNotifierItem does not have a notification for double clicks, but
     * we can detect 2 fast single clicks as one double */
    cur_time = GetTickCount();
    switch( uMsg )
    {
        case WM_LBUTTONDOWN:
            if (cur_time - last_L_time < GetDoubleClickTime())
                PostMessageW(icon->owner, icon->callback_message,
                             (WPARAM)icon->id, (LPARAM)WM_LBUTTONDBLCLK);
            last_L_time = cur_time;
            break;
        case WM_RBUTTONDOWN:
            if (cur_time - last_R_time < GetDoubleClickTime())
                PostMessageW(icon->owner, icon->callback_message,
                             (WPARAM)icon->id, (LPARAM)WM_RBUTTONDBLCLK);
            last_R_time = cur_time;
            break;
        case WM_MBUTTONDOWN:
            if (cur_time - last_M_time < GetDoubleClickTime())
                PostMessageW(icon->owner, icon->callback_message,
                             (WPARAM)icon->id, (LPARAM)WM_MBUTTONDBLCLK);
            last_M_time = cur_time;
            break;
    }
}


/**************************************************************
              DBus message handlers
***************************************************************/

static void root_message_handler_unreg(DBusConnection *conn, void *user_data)
{
    UNREFERENCED_PARAMETER(conn); /* unused */
    UNREFERENCED_PARAMETER(user_data); /* unused */
    TRACE("DBus: root object was unregistered\n");
}


static DBusHandlerResult root_message_handler(
        DBusConnection *conn,
        DBusMessage *msg,
        void *user_data )
{
    DBusHandlerResult ret;
    DBusMessage *reply;
    dbus_bool_t resb;
    DBusError derror;
    const char *mpath, *mintf, *mmemb, *mdest, *msig;
    char *req_intf, *req_prop;
    struct tray_icon *icon;
    char tiptext_a[128];
    DBusMessageIter iter, arr_iter;

    UNREFERENCED_PARAMETER(user_data); /* unused */

    /* probably, default fallback handler should always
     * mark message as handled, otherwise it will cause errors */
    ret = DBUS_HANDLER_RESULT_HANDLED;

    /* Get message information */
    mpath = p_dbus_message_get_path(msg);
    mintf = p_dbus_message_get_interface(msg);
    mmemb = p_dbus_message_get_member(msg);
    mdest = p_dbus_message_get_destination(msg);
    msig  = p_dbus_message_get_signature(msg);

    TRACE("DBus: message for path [%s] dest [%s], intf [%s].[%s]\n",
            mpath, mdest, mintf, mmemb);

    p_dbus_error_init( &derror );

    if (p_dbus_message_is_method_call(msg, "org.freedesktop.DBus.Introspectable", "Introspect"))
    {
        reply = NULL;
        if (strcmp(mpath, "/") == 0) /* request for root object */
        {
            reply = create_introspect_reply(msg, g_dbus_introspect_xml_root_with_SNI);
        }
        else if (strcmp(mpath, "/StatusNotifierItem") == 0) /* request for SNI object */
        {
            reply = create_introspect_reply(msg, g_dbus_introspect_xml_SNI);
        }
        if (reply)
        {
            TRACE( "Introspect reply for path %s\n", mpath );
            dbus_send_safe(reply);
            p_dbus_message_unref(reply);
        }
        return ret;
    }

    if (strcmp(mpath, "/StatusNotifierItem") == 0)
    {
        /* all messages for /StatusNotifierItem go here */
        /* Example:
         * DBus: message for path [/StatusNotifierItem]
         * dest [org.kde.StatusNotifierItem-19965-1],
         * intf [org.freedesktop.DBus.Properties].[GetAll]:
         * this is SNI host requesting all our icon properties */

        /* try to find our icon that request was for */
        icon = get_icon_from_list_by_dname(mdest);
        if (!icon)
        {
            WARN( "DBus: cannot find icon struct for DBus name [%s]\n", mdest );
            error_reply_failed(msg);
            return ret;
        }

        /* tests for which method is called */
        if (p_dbus_message_is_method_call(msg, "org.freedesktop.DBus.Properties", "Get") )
        {
            if (strcmp(msig, "ss"))
            {
                /* incorrect call signature, must be 2 string arguments */
                error_reply_invalid_args(msg);
                return ret;
            }
            resb = p_dbus_message_get_args(msg, &derror,
                DBUS_TYPE_STRING, &req_intf,
                DBUS_TYPE_STRING, &req_prop,
                DBUS_TYPE_INVALID );
            if (resb == FALSE)
            {
                error_reply_invalid_args(msg);
                return ret;
            }
            TRACE("DBus: call [%s] Properties.Get([%s], [%s])\n", mintf, req_intf, req_prop);

            /* check that they request this interface */
            if (strcmp(req_intf, "org.kde.StatusNotifierItem"))
            {
                WARN( "DBus: Properties.Get(): unknown interface requested: [%s]\n", req_intf );
                error_reply_unknown_interface(msg, req_intf);
                return ret;
            }

            reply = NULL;

            if(strcmp(req_prop, "Category") == 0) {
                reply = create_reply_propget_s(msg, "ApplicationStatus");
                /* other categories: ApplicationStatus, Communications, SystemServices, Hardware */
            } else if(strcmp(req_prop, "Id") == 0) {
                reply = create_reply_propget_s(msg, icon->sni_title);
            } else if(strcmp(req_prop, "Title") == 0) {
                WideCharToMultiByte(CP_ACP, 0, icon->tiptext, -1, tiptext_a, sizeof(tiptext_a), NULL, NULL);
                reply = create_reply_propget_s(msg, tiptext_a);
            } else if(strcmp(req_prop, "Status") == 0) {
                reply = create_reply_propget_s(msg, "Active");
                /* other values: Active, Passive, NeedsAttention */
            } else if(strcmp(req_prop, "WindowId") == 0) {
                Window w = X11DRV_get_whole_window( icon->owner );
                reply = create_reply_propget_u(msg, (dbus_uint32_t)w);
            } else if(strcmp(req_prop, "IconName") == 0) {
                if (icon->icon_data && (icon->icon_w > 0) && (icon->icon_h > 0))
                {
                    /* If we have actual icon image data, send empty icon name */
                    reply = create_reply_propget_s(msg, "");
                }
                else
                {
                    /* otherwise, as fallback, use wine icon */
                    reply = create_reply_propget_s(msg, "wine");
                }
            } else if(strcmp(req_prop, "OverlayIconName") == 0) {
                reply = create_reply_propget_s(msg, "");
            } else if(strcmp(req_prop, "AttentionIconName") == 0) {
                reply = create_reply_propget_s(msg, "");
            } else if(strcmp(req_prop, "AttentionMovieName") == 0) {
                reply = create_reply_propget_s(msg, "");
            } else if(strcmp(req_prop, "IconPixmap") == 0) {
                /* send actual icon pixels */
                reply = create_reply_propget_pixmap(msg, icon->icon_w, icon->icon_h, icon->icon_data);
            } else if(strcmp(req_prop, "OverlayIconPixmap") == 0) {
                reply = create_reply_propget_pixmap(msg, 0, 0, NULL);
            } else if(strcmp(req_prop, "AttentionIconPixmap") == 0) {
                reply = create_reply_propget_pixmap(msg, 0, 0, NULL);
            } else if(strcmp(req_prop, "ToolTip") == 0) {
                WideCharToMultiByte(CP_ACP, 0, icon->tiptext, -1, tiptext_a, sizeof(tiptext_a), NULL, NULL);
                if (icon->icon_data && (icon->icon_w > 0) && (icon->icon_h > 0))
                {
                    /* If we have actual icon image data, send empty icon name and real image data */
                    reply = create_reply_propget_tooltip(msg,
                        "",            /* icon name */
                        icon->icon_w, icon->icon_h, icon->icon_data, /* icon data */
                        tiptext_a,  /* tip title (szTip member of NOTIFYICONDATA) */
                        "");        /* tip text, empty */
                }
                else
                {
                    reply = create_reply_propget_tooltip(msg,
                        "wine",           /* icon name */
                        0, 0, NULL,       /* icon data */
                        tiptext_a,  /* tip title (szTip member of NOTIFYICONDATA) */
                        "");        /* tip text, empty */
                }
            } else {
                WARN("DBus: unknown property requested: [%s]\n", req_prop);
                error_reply_unknown_property(msg, req_prop);
            }

            if (reply)
            {
                TRACE( "Propget reply for icon %s\n", icon->dbus_name );
                dbus_send_safe(reply);
                p_dbus_message_unref(reply);
                return ret;
            }
        }
        else if (p_dbus_message_is_method_call(msg, "org.freedesktop.DBus.Properties", "Set"))
        {
            if (strcmp(msig, "ssv"))
            {
                error_reply_invalid_args(msg);
                return ret;
            }

            resb = p_dbus_message_get_args(msg, &derror,
                DBUS_TYPE_STRING, &req_intf,
                DBUS_TYPE_STRING, &req_prop,
                DBUS_TYPE_INVALID );

            /* check that they request this interface */
            if (strcmp(req_intf, "org.kde.StatusNotifierItem"))
            {
                WARN( "DBus: Properties.Set(): unknown interface requested: [%s]\n", req_intf );
                error_reply_unknown_interface(msg, req_intf);
                return ret;
            }

            if (resb == TRUE)
            {
                TRACE( "DBus: call [%s] Properties.Set([%s], [%s]): replying \"read-only\"\n", mintf, req_intf, req_prop);
                error_reply_read_only_prop(msg, req_prop);
            }
            else
            {
                error_reply_read_only_prop(msg, "FAILED_TO_GET_PROP_NAME");
            }
            return ret;
        }
        else if (p_dbus_message_is_method_call(msg, "org.freedesktop.DBus.Properties", "GetAll"))
        {
            if (strcmp(msig, "s"))
            {
                error_reply_invalid_args(msg);
                return ret;
            }

            resb = p_dbus_message_get_args(msg, &derror,
                DBUS_TYPE_STRING, &req_intf,
                DBUS_TYPE_INVALID );
            if( resb == FALSE ) {
                ERR( "DBus: Properties.GetAll(): Failed to get message args!\n" );
                error_reply_failed(msg);
                return ret;
            }
            TRACE( "DBus: Properties.GetAll( \"%s\" )\n", req_intf );

            /* check that they request this interface */
            if (strcmp(req_intf, "org.kde.StatusNotifierItem"))
            {
                WARN( "DBus: unknown interface requested: [%s]\n", req_intf );
                error_reply_unknown_interface(msg, req_intf);
                return ret;
            }

            /* okay, respond with all our "properties" */
            reply = p_dbus_message_new_method_return(msg);
            if (!reply)
            {
                error_reply_failed(msg);
                return ret;
            }

            WideCharToMultiByte(CP_ACP, 0, icon->tiptext, -1, tiptext_a, sizeof(tiptext_a), NULL, NULL);

            /* create reply */
            p_dbus_message_iter_init_append(reply, &iter);
            p_dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &arr_iter); /* open array */
            /* insert all object properties, contained in Variants */
            reply_propgetall_append_s(&arr_iter, "Category", "ApplicationStatus");
            reply_propgetall_append_s(&arr_iter, "Id", icon->sni_title);  /* seems to be unused by KDE 5 */
            reply_propgetall_append_s(&arr_iter, "Title", tiptext_a);
            /* ^^ this is also an icon identifier used by KDE Plasma 5 systray setting utility */
            reply_propgetall_append_s(&arr_iter, "Status", "Active");
            reply_propgetall_append_u(&arr_iter, "WindowId", 0);
            /* ^^ KDE sends this as int32, not uint32... what? */
            if (icon->icon_data && (icon->icon_w > 0) && (icon->icon_h > 0))
            {
                /* we have actual icon image data, send empty icon name */
                reply_propgetall_append_s(&arr_iter, "IconName", "");
            }
            else
            {
                /* we don't have icon pixels, fallback: use standard wine icon */
                /* this is used as icon name in freedesktop icon theme */
                reply_propgetall_append_s(&arr_iter, "IconName", "wine");
            }
            reply_propgetall_append_s(&arr_iter, "OverlayIconName", "");
            reply_propgetall_append_s(&arr_iter, "AttentionIconName", "");
            reply_propgetall_append_s(&arr_iter, "AttentionMovieName", "");
            reply_propgetall_append_pixmap(&arr_iter, "IconPixmap",
                                           icon->icon_w,
                                           icon->icon_h,
                                           icon->icon_data);
            reply_propgetall_append_pixmap(&arr_iter, "OverlayIconPixmap", 0, 0, NULL);
            reply_propgetall_append_pixmap(&arr_iter, "AttentionIconPixmap", 0, 0, NULL);
            if (icon->icon_data && (icon->icon_w > 0) && (icon->icon_h > 0))
            {
                /* we have actual image data, send empty icon name and real pixels */
                reply_propgetall_append_tooltip(
                    &arr_iter,
                    "ToolTip", /* attribute name */
                    "",    /* icon name */
                    icon->icon_w, /* icon width, */
                    icon->icon_h, /* icon height, */
                    icon->icon_data, /* icon image pixels */
                    tiptext_a, /* tooltip title (this is szTip member if NOTIFYICONDATA) */
                    "" /* tooltip text, empty */
                );
            }
            else
            {
                /* we don't have actual image data, send "wine" icon name and no pixels */
                reply_propgetall_append_tooltip(
                    &arr_iter,
                    "ToolTip", /* attribute name */
                    "wine",    /* icon name */
                    0, /* icon width, */
                    0, /* icon height, */
                    NULL, /* icon image pixels */
                    tiptext_a, /* tooltip title (this is szTip member if NOTIFYICONDATA) */
                    "" /* tooltip text, empty */
                );
            }
            /* close array */
            p_dbus_message_iter_close_container(&iter, &arr_iter);

            TRACE( "Propgetall reply for icon %s\n", icon->dbus_name );
            dbus_send_safe(reply);
            p_dbus_message_unref(reply);
            return ret;
        }
        else if (p_dbus_message_is_method_call(msg, "org.kde.StatusNotifierItem", "Activate"))
        {
            /* Left click on status icon
             * VOID org.freedesktop.StatusNotifierItem.Activate (INT x, INT y); */
            TRACE( "org.kde.StatusNotifierItem:Activate()\n" );
            /* we ignore click coordinates here, because windows doesn't send it */
            relay_windows_message(icon, WM_LBUTTONDOWN);
            relay_windows_message(icon, WM_LBUTTONUP);
            /* TODO: should we emulate WM_MOUSEMOVE here? */
        }
        else if (p_dbus_message_is_method_call(msg, "org.kde.StatusNotifierItem", "ContextMenu"))
        {
            /* Right click on status icon
             * VOID org.freedesktop.StatusNotifierItem.ContextMenu (INT x, INT y); */
            TRACE( "org.kde.StatusNotifierItem:ContextMenu()\n" );
            relay_windows_message(icon, WM_RBUTTONDOWN);
            relay_windows_message(icon, WM_RBUTTONUP);
        }
        else if (p_dbus_message_is_method_call(msg, "org.kde.StatusNotifierItem", "SecondaryActivate"))
        {
            /* Middle click on status icon
             * VOID org.freedesktop.StatusNotifierItem.SecondaryActivate (INT x, INT y); */
            TRACE( "org.kde.StatusNotifierItem:SecondaryActivate()\n" );
            relay_windows_message(icon, WM_MBUTTONDOWN);
            relay_windows_message(icon, WM_MBUTTONUP);
        }
        else if (p_dbus_message_is_method_call(msg, "org.kde.StatusNotifierItem", "Scroll"))
        {
            /* Mouse scroll over StatusNotifierItem
             * VOID org.freedesktop.StatusNotifierItem.Scroll (INT delta, STRING orientation);
             * orientation can be one of: "horizontal", "vertical" */
            TRACE( "org.kde.StatusNotifierItem:Scroll()\n" );
            /* Windows does not send any mouse scroll events, or even WM_VSCROLL :( */
        }
    }

    /* Message handler can return one of these values:
     * from enum DBusHandlerResult
     * DBUS_HANDLER_RESULT_HANDLED - Message has had its effect,
                                     no need to run more handlers.
     * DBUS_HANDLER_RESULT_NOT_YET_HANDLED - Message has not had any effect,
                                             see if other handlers want it.
     * DBUS_HANDLER_RESULT_NEED_MEMORY - Please try again later with more memory.
     */
    return ret;
}


/* DBus message loop thread function */
/* Infinite loop?? dlls/mountmgr.sys/dbus.c uses a similar approach! */
static DWORD WINAPI dbus_thread( void *arg )
{
    dbus_bool_t retb;

    UNREFERENCED_PARAMETER(arg);
    if (!d_connection) return 1;

    retb = TRUE;
    while (retb)
    {
        /* We call dbus_connection_send() when adding new icon,
         * for example, this can lead to race condition. That's why
         * there is a critical section here */
        EnterCriticalSection( &g_dconn_cs );
        /* reads data from socket (wait 100 ms) and calls message handler */
        retb = p_dbus_connection_read_write_dispatch( d_connection, 100 );
        LeaveCriticalSection( &g_dconn_cs );
        /* We need to release lock at least sometimes, that's why
         * here is Sleep() call here with timeout of 100 ms */
        Sleep(100);
        /* TODO: tweak timeouts? */
    }
    /* This infinite loop can break if there was some DBus error
     * and connection was closed */
    WARN( "DBus: message loop has ended for some reason! Closing connection.\n" );

    /* if we are here, we are probably not connected already */
    p_dbus_connection_unref(d_connection);
    d_connection = NULL;

    /* mark as not running */
    g_dbus_thread_running = FALSE;
    return 0;
}


/* Starts dbus message loop thread, if it's not already running */
static void start_dbus_thread(void)
{
    HANDLE handle;
    if (g_dbus_thread_running) return;

    handle = CreateThread( NULL, 0, dbus_thread, NULL, 0, NULL );
    if (handle)
    {
        g_dbus_thread_running = TRUE;
        CloseHandle( handle );
        TRACE( "started dbus_thread\n" );
    }
}


/* Establishes connection to DBus session bus.
 * Returns TRUE if connection is OK, FALSE otherwise.
 * Returns TRUE immediately if already connected. */
static BOOL connect_dbus(void)
{
    DBusError error;
    DBusObjectPathVTable vtable;
    dbus_bool_t resb;

    if (d_connection) return TRUE;  /* already connected */
    p_dbus_error_init( &error );

    if (!(d_connection = p_dbus_bus_get( DBUS_BUS_SESSION, &error )))
    {
        WARN( "DBus: failed to connect to session bus: %s\n", error.message );
        p_dbus_error_free( &error );
        return FALSE;
    }

    /* we do not want to exit(1) on any DBus error! */
    p_dbus_connection_set_exit_on_disconnect(d_connection, FALSE);

    /* Register default message handler for root DBus object "/" */
    ZeroMemory(&vtable, sizeof(vtable));
    vtable.unregister_function = root_message_handler_unreg;
    vtable.message_function    = root_message_handler;
    resb = p_dbus_connection_try_register_fallback(
            d_connection,
            "/",           /* object path */
            &vtable,       /* message handler functions */
            NULL,          /* user_data */
            &error);
    if( resb == FALSE )
    {
        WARN("DBus: ERROR: Failed to register DBus root object "
             "message handler! Error: %s\n", error.message);
        p_dbus_error_free( &error );
        return FALSE;
    }

    /* Run read/write dispatcher. This will actually call message handler,
     * if there are any messages incoming (and they are there).
     * The first message will be signal 'NameAcquired' from DBus
     * example: "DBus: message for path [/org/freedesktop/DBus] dest [:1.58],
     *           intf [org.freedesktop.DBus].[NameAcquired]" */
    p_dbus_connection_read_write_dispatch(d_connection, 0);
    /* Of course, we will want real DBus message loop later, for real icons */

    TRACE( "DBus: connected to SESSION bus as '%s'.\n",
            p_dbus_bus_get_unique_name(d_connection) );
    return TRUE;
}


static void disconnect_dbus(void)
{
    if (d_connection) {
        p_dbus_connection_unregister_object_path(d_connection, "/");
        p_dbus_connection_read_write_dispatch(d_connection, 0);
        p_dbus_connection_unref(d_connection);
        d_connection = NULL;
        TRACE( "DBus: disconnected.\n" );
    }
}


/* Check if org.kde.StatusNotifierWatcher is running on DBus, and read its
 * property IsStatusNotifierHostRegistered to determine if we can
 * use DBus StatusNotifierItem tray. */
static BOOL is_statusnotifier_host_running(void)
{
    DBusError error;
    DBusMessage *msg, *reply;
    const char *iface_name;
    const char *property_name;
    DBusMessageIter iter;
    DBusMessageIter sub_iter;
    dbus_bool_t is_sni_host_running = FALSE;
    int arg_type;

    if (!d_connection) return FALSE;
    p_dbus_error_init( &error );

    /* Check that there is StatusNotifierWatcher on bus.
     * According to DBus docs, this method can cause race condition,
     * StatusNotifierWatcher can disappear any time after this method
     * was called, but in that case the following DBus method call
     * will simply fail anyway. */
    if (p_dbus_bus_name_has_owner(
        d_connection, "org.kde.StatusNotifierWatcher", &error))
    {
        TRACE( "DBus: detected that org.kde.StatusNotifierWatcher present, "
               "will ask him about StatusNotifierHost!\n" );
        /* We need to read property "IsStatusNotifierHostRegistered"
         * of interface "org.kde.StatusNotifierWatcher"
         * which is on path "/StatusNotifierWatcher"
         * So, we need to call org.freedesktop.DBus.Properties.Get(...) on that
         * interface */
        msg = p_dbus_message_new_method_call(
                "org.kde.StatusNotifierWatcher", /* bus name */
                "/StatusNotifierWatcher", /* path */
                "org.freedesktop.DBus.Properties", /* interface */
                "Get"); /* method */
        if (msg)
        {
            /* Get() method has 2 parameters - interface name and
             * property name, set them */
            iface_name = "org.kde.StatusNotifierWatcher";
            property_name = "IsStatusNotifierHostRegistered";
            p_dbus_message_append_args(msg,
                    DBUS_TYPE_STRING, &iface_name,
                    DBUS_TYPE_STRING, &property_name,
                    DBUS_TYPE_INVALID);
            /* send request and wait for reply for 1000 ms */
            reply = p_dbus_connection_send_with_reply_and_block(
                    d_connection, msg,
                    1000,   /* timeout in ms */
                    &error);
            if (reply)
            {
                /* reply from org.kde.StatusNotifierWatcher will contain
                 * Variant(bool), although in specification
                 * org.freedesktop.StatusNotifierWatcher should return
                 * simply bool, instead of Variant(bool) !!! */
                if (p_dbus_message_iter_init(reply, &iter))
                {
                    /* Read only first message argument. Either it is
                     * Bool, or Variant(bool) */
                    arg_type = p_dbus_message_iter_get_arg_type(&iter);
                    if (arg_type == DBUS_TYPE_VARIANT)
                    {
                        /* Variant(bool), recurse read iterator into Variant type */
                        p_dbus_message_iter_recurse(&iter, &sub_iter);
                        /* read its contained value */
                        arg_type = p_dbus_message_iter_get_arg_type(&sub_iter);
                        if (arg_type == DBUS_TYPE_BOOLEAN)
                        {
                            p_dbus_message_iter_get_basic(&sub_iter, &is_sni_host_running);
                            TRACE( "DBus: OK: Got prop.get reply, "
                                   "IsStatusNotifierHostRegistered = %u\n",
                                    (unsigned)is_sni_host_running );
                        }
                    }
                    else if (arg_type == DBUS_TYPE_BOOLEAN)
                    {
                        /* simply bool */
                        p_dbus_message_iter_get_basic(&iter, &is_sni_host_running);
                        TRACE( "DBus: OK: Got prop.get reply, "
                               "IsStatusNotifierHostRegistered = %u\n",
                                (unsigned)is_sni_host_running );
                    }
                }
                p_dbus_message_unref(reply);
                p_dbus_message_unref(msg);
                return (is_sni_host_running ? TRUE : FALSE);
            }
            /* reply == NULL here, some error has happened */
            WARN( "DBus: error requesting property from StatusNotifierWatcher: "
                   "%s", error.message );
            p_dbus_error_free( &error );
            p_dbus_message_unref(msg);
            return FALSE;
        }
        WARN( "DBus: Failed to create new DBus message!\n" );
        return FALSE;
    }
    TRACE( "DBus: name org.kde.StatusNotifierWatcher is not found on bus.\n" );
    return FALSE;
}


#endif  /*  SONAME_LIBDBUS_1  */


/* Check if Wine can use DBus StatusNotifierItem specification
 * to implement system tray icons. This should be called from
 * systray.c: wine_notify_icon() when performing tray icon operations.
 *
 * Returns TRUE, if compiled with DBus support and StatusNotifierHost
 * is detected running, FALSE otherwise. */
BOOL can_use_dbus_sni_systray(void)
{
#ifdef SONAME_LIBDBUS_1
    static int using_dbus_sni_systray = -1; /* -1 = unknown */

    /* This environment variable can be set to completely disable
     * DBus-powered StatusNotifierItem systray support */
    if (getenv("WINE_DISABLE_SNI_SYSTRAY"))
    {
        TRACE( "env: WINE_DISABLE_SNI_SYSTRAY: DBus SNI systray disabled.\n");
        return FALSE;
    }

    if (using_dbus_sni_systray == FALSE) return FALSE;
    if (using_dbus_sni_systray == TRUE)  return TRUE;

    /* check that we can at least load DBus library */
    if (!initialize_dbus())
    {
        using_dbus_sni_systray = FALSE;
        TRACE( "Not enabling DBus systray support: cannot load DBus library\n" );
        return FALSE;
    }

    /* check that we can connect to session bus */
    if (!connect_dbus())
    {
        using_dbus_sni_systray = FALSE;
        return FALSE;
    }

    /* check that there is a StatusNotifierHost present */
    if (is_statusnotifier_host_running())
    {
        TRACE( "DBus: detected that we can use DBus systray instead of XEmbed\n" );
        /* Do not disconnect from DBus here, we will probably use
         * this connection to manage status notifier items later ;) */
        using_dbus_sni_systray = TRUE;
        return TRUE;
    }

    /* No DBus/SNI system tray detected, disconnect DBus and return FALSE */
    disconnect_dbus();

    TRACE( "DBus: No DBus/SNI system tray was detected.\n" );
    return FALSE;
#else
    /* Compiled without DBus support? Cannot use DBus at all */
    TRACE( "Compiled without DBus support, cannot use DBus SNI systray\n" );
    return FALSE;
#endif
}


static void assign_tray_icon_data(struct tray_icon *icon, NOTIFYICONDATAW *nid)
{
    ICONINFO iconinfo;
    BITMAP bmColor;
    HBITMAP hbmColorCopy;
    DWORD icon_data_size;

    /* copy state flags */
    if (nid->uFlags & NIF_STATE)
    {
        icon->state = (icon->state & ~nid->dwStateMask) | (nid->dwState & nid->dwStateMask);
    }
    /* copy icon */
    if (nid->uFlags & NIF_ICON)
    {
        /* delete old icon, if any */
        if (icon->image) DestroyIcon(icon->image);
        if (icon->icon_data) HeapFree(GetProcessHeap(), 0, icon->icon_data);
        icon->image = NULL;
        icon->icon_data = NULL;
        icon->icon_w = icon->icon_h = 0;

        /* set new icon */
        if (nid->hIcon)
        {
            icon->image = CopyIcon(nid->hIcon);
            if (GetIconInfo(nid->hIcon, &iconinfo))
            {
                TRACE( "fIcon = %u, (hotspot %d, %d) mask = %p, color = %p\n", iconinfo.fIcon,
                       iconinfo.xHotspot, iconinfo.yHotspot,
                       iconinfo.hbmMask, iconinfo.hbmColor);
                TRACE( "System icon size: %d x %d\n", GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON) );

                ZeroMemory(&bmColor, sizeof(BITMAP));

                if (iconinfo.hbmColor)
                {
                    hbmColorCopy = CopyImage(iconinfo.hbmColor, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
                    if (hbmColorCopy)
                    {
                        GetObjectW(hbmColorCopy, sizeof(BITMAP), &bmColor);
                        /* MSDN: If hgdiobj is a handle to a bitmap created by any other means,
                         * (not by CreateDIBSection) GetObject returns only the width, height,
                         * and color format information of the bitmap. You can obtain the bitmap's
                         * bit values by calling the GetDIBits or GetBitmapBits function.
                         * But we created bitmap copy with LR_CREATEDIBSECTION, and we HAVE
                         * bits pointer! */
                        TRACE( "Got color bitmap: bmType = %d\n", bmColor.bmType );
                        TRACE( "                  size = %d x %d\n", bmColor.bmWidth, bmColor.bmHeight );
                        TRACE( "                  scanline bytes = %d\n", bmColor.bmWidthBytes );
                        TRACE( "                  color planes = %d\n", (int)bmColor.bmPlanes );
                        TRACE( "                  bits per pixel = %d\n", (int)bmColor.bmBitsPixel );
                        TRACE( "                  bits pointer = %p\n", bmColor.bmBits );

                        /* SNI spec says image format must be ARGB32, so we handle only 32(24)-bit image */
                        if (bmColor.bmBits && (bmColor.bmBitsPixel == 32))
                        {
                            /* image data size = w * h * (bits_per_pixel / 8) */
                            icon_data_size = bmColor.bmWidth * bmColor.bmHeight * bmColor.bmBitsPixel / 8;
                            icon->icon_data = (unsigned char *)HeapAlloc(GetProcessHeap(), 0, icon_data_size);
                            if (icon->icon_data)
                            {
                                unsigned char *row_dst, *row_src;
                                int irow, j;
                                ZeroMemory(icon->icon_data, icon_data_size);
                                row_dst = icon->icon_data;
                                row_src = (unsigned char *)bmColor.bmBits;
                                /* make row_src point to last row, to flip image bottom to top */
                                row_src += ((bmColor.bmHeight - 1) * bmColor.bmWidthBytes);
                                for (irow = 0; irow < bmColor.bmHeight; irow++)
                                {
                                    for (j=0; j<bmColor.bmWidthBytes; j+=4)
                                    {
                                        /* change byte order so that BGRA (as in RGBQUAD structure
                                         * becomes ARGB, as required by SNI spec */
                                        row_dst[j + 0] = row_src[j + 3]; /* B -> A */
                                        row_dst[j + 1] = row_src[j + 2]; /* G -> R */
                                        row_dst[j + 2] = row_src[j + 1]; /* R -> G */
                                        row_dst[j + 3] = row_src[j + 0]; /* A -> B */
                                    }
                                    row_src -= bmColor.bmWidthBytes; /* move 1 row backward */
                                    row_dst += bmColor.bmWidthBytes; /* move 1 row forward */
                                }
                                /* memcpy(icon->icon_data, bmColor.bmBits, icon_data_size); */
                                icon->icon_w = (int)bmColor.bmWidth;
                                icon->icon_h = (int)bmColor.bmHeight;
                                TRACE( "Saved icon bitmap, %u bytes\n", icon_data_size );
                            }
                        }

                        DeleteObject(hbmColorCopy);
                    }
                }
                if (iconinfo.hbmMask)  DeleteObject(iconinfo.hbmMask);
                if (iconinfo.hbmColor) DeleteObject(iconinfo.hbmColor);
            }
        }
    }
    /* copy callback message */
    if (nid->uFlags & NIF_MESSAGE)
    {
        icon->callback_message = nid->uCallbackMessage;
    }
    /* copy tooltip data */
    if (nid->uFlags & NIF_TIP)
    {
        lstrcpynW(icon->tiptext, nid->szTip, sizeof(icon->tiptext)/sizeof(WCHAR));
    }
    /* copy balloon popup data */
    if (nid->uFlags & NIF_INFO && nid->cbSize >= NOTIFYICONDATAA_V2_SIZE)
    {
        /* has balloon popup members */
        lstrcpynW( icon->info_text,  nid->szInfo,      sizeof(icon->info_text)/sizeof(WCHAR) );
        lstrcpynW( icon->info_title, nid->szInfoTitle, sizeof(icon->info_title)/sizeof(WCHAR) );
        icon->info_flags = nid->dwInfoFlags;
        icon->info_timeout = max(min(nid->u.uTimeout, BALLOON_SHOW_MAX_TIMEOUT), BALLOON_SHOW_MIN_TIMEOUT);
        icon->info_icon = nid->hBalloonIcon;
    }
}


BOOL add_sni_icon(NOTIFYICONDATAW *nid)
{
#ifdef SONAME_LIBDBUS_1
    struct tray_icon  *icon;
    int res;
    dbus_bool_t resb;
    DBusError derror;
    DBusMessage *req;
    char *NPE_FIX;
    DWORD owner_pid;

    TRACE("id=0x%x, hwnd=%p\n", nid->uID, nid->hWnd);

    /* check if icon already exists */
    if ((icon = get_icon_from_list(nid->hWnd, nid->uID)))
    {
        WARN("DBus: duplicate tray icon add, buggy app?\n");
        return FALSE;
    }

    /* allocate new icon struct and add it to list */
    icon = add_new_icon_to_list();
    if (!icon) return FALSE;

    /* init structure members */
    icon->id      = nid->uID;
    icon->owner   = nid->hWnd;
    icon->display = -1;  /* display index, or -1 if hidden */

    assign_tray_icon_data(icon, nid);

    /* Ensure that dbus message loop is running */
    start_dbus_thread();

    /* request some particular name on session bus */
    /* Format: org.kde.StatusNotifierItem-PID-ICONID */
    GetWindowThreadProcessId(icon->owner, &owner_pid);
    snprintf(icon->dbus_name, sizeof(icon->dbus_name)-1,
        "org.kde.StatusNotifierItem-%u-%u",
        owner_pid, nid->uID);

    p_dbus_error_init( &derror );

    TRACE( "Requesting name [%s]...\n", icon->dbus_name );
    EnterCriticalSection( &g_dconn_cs );
    res = p_dbus_bus_request_name(d_connection, icon->dbus_name,
        DBUS_NAME_FLAG_DO_NOT_QUEUE, &derror);
    LeaveCriticalSection( &g_dconn_cs );
    if( res != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER ) {
        ERR("DBus: Request name failed! Not primary owner! code=%d\n", res);
        ERR("DBus error: %s\n", derror.message);
        p_dbus_error_free( &derror );
        return FALSE;
    }
    TRACE( "Request name [%s] OK!\n", icon->dbus_name );

    /* calculate and store title that will be used in several places
     *  - as a tooltip title, popup title
     *  - as icon identifier, maybe
     * ideally it should be process name */
    /*HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, owner_pid);
    if (hProcess)
    {
        GetProcessImageFileNameA(hProcess, icon->sni_title, sizeof(icon->sni_title)-1)
        CloseHandle(hProcess);
    } */
    /* This requires linking to psapi.dll and include "psapi.h" :/ */

    /* GetModuleFileNameA(NULL, icon->sni_title, sizeof(icon->sni_title)-1); */
    /* ^^ This returns [C:\windows\system32\explorer.exe], because winex11.drv is loaded
     * by explorer.exe? But we cannot use it as icon identifier then :) */

    /* TRACE( "command line: [%s]\n", GetCommandLineA() ); */
    /* [C:\windows\system32\explorer.exe /desktop], the same, cannot use */

    lstrcpynA(icon->sni_title, "wine", sizeof(icon->sni_title)-1);

    TRACE( "Using [%s] as application name\n", icon->sni_title );

    /* Try to communicate with StatusNotifierWatcher:
     * tell him that we are here and we have a status icon! */
    req = p_dbus_message_new_method_call(
        "org.kde.StatusNotifierWatcher", /* dest */
        "/StatusNotifierWatcher",        /* path */
        "org.kde.StatusNotifierWatcher", /* iface */
        "RegisterStatusNotifierItem"     /* method */
    );
    /* we need to pass him our BUS NAME in message */
    NPE_FIX = strdup(icon->dbus_name);
    if( NPE_FIX ) {
        p_dbus_message_append_args(req,
                                 DBUS_TYPE_STRING, &NPE_FIX,
                                 DBUS_TYPE_INVALID);
        free(NPE_FIX);
    }
    resb = dbus_send_safe(req);
    if (resb == FALSE)
    {
        WARN("dbus_connection_send() failed: cannot register our SNI!\n");
        p_dbus_message_unref(req);
        return FALSE;
    }

    return TRUE;
#else
    /* complied without DBus support */
    UNREFERENCED_PARAMETER(nid);
    return FALSE;
#endif
}


BOOL modify_sni_icon(NOTIFYICONDATAW *nid)
{
#ifdef SONAME_LIBDBUS_1
    struct tray_icon *icon;

    TRACE("id=0x%x, hwnd=%p\n", nid->uID, nid->hWnd);

    icon = get_icon_from_list(nid->hWnd, nid->uID);
    if (!icon) return FALSE;

    /* update structure members */
    assign_tray_icon_data(icon, nid);

    /* icon has changed? */
    if (nid->uFlags & NIF_ICON)
    {
        /* icon data was already updated by assign_tray_icon_data() */
        /* Notify SNI host about it */
        send_signal_noargs( "NewIcon" );
    }

    /* szTip has changed? */
    if (nid->uFlags & NIF_TIP)
    {
        /* Notify SNI host about new tooltip */
        send_signal_noargs( "NewToolTip" );
    }

    /* balloon popup was updated? */
    if (nid->uFlags & NIF_INFO && nid->cbSize >= NOTIFYICONDATAA_V2_SIZE)
    {
        /* Notify SNI host that our icon needs attention */
        /* actually it was active all the time, but maybe it will change in future */
        send_signal_NewStatus( "NeedsAttention" ); /* possible values: Active, Passive, NeedsAttention */
        /* TODO: deal with balloon */
        /* update_balloon( icon ); */
    }

    /* TODO: implement hiding icon?
    if (icon->state & NIS_HIDDEN) hide_icon( icon );
    else show_icon( icon );
    */

    /* We don't use these SNI signals: NewTitle, NewAttentionIcon, NewOverlayIcon */

    return TRUE;
#else
    /* complied without DBus support */
    UNREFERENCED_PARAMETER(nid);
    return FALSE;
#endif
}


static BOOL delete_icon(struct tray_icon *icon)
{
    BOOL ret;
    int res;
    DBusError derror;

    if (!d_connection) return FALSE;

    /* Release dedicated SNI bus name */
    p_dbus_error_init( &derror );
    res = p_dbus_bus_release_name(d_connection, icon->dbus_name, &derror);
    if( res != DBUS_RELEASE_NAME_REPLY_RELEASED ) {
        WARN( "DBus: Release name failed! Err code=%d\n", res );
        WARN( "DBus error: %s\n", derror.message );
        ret = FALSE;
    } else {
        TRACE( "DBus: Name released [%s] OK!\n", icon->dbus_name );
        ret = TRUE;
    }

    /* Do not forget to delete hIcon ? */
    if (icon->image) DestroyIcon(icon->image);
    if (icon->icon_data) HeapFree(GetProcessHeap(), 0, icon->icon_data);
    icon->image = NULL;
    icon->icon_data = NULL;
    icon->icon_w = icon->icon_h = 0;

    delete_icon_from_list( icon );
    return ret;
}



BOOL delete_sni_icon(NOTIFYICONDATAW *nid)
{
#ifdef SONAME_LIBDBUS_1
    struct tray_icon *icon;

    TRACE("id=0x%x, hwnd=%p\n", nid->uID, nid->hWnd);

    icon = get_icon_from_list(nid->hWnd, nid->uID);
    if (!icon) return FALSE;

    return delete_icon( icon );
#else
    /* complied without DBus support */
    UNREFERENCED_PARAMETER(nid);
    return FALSE;
#endif
}
