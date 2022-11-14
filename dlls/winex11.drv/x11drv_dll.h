/*
 * X11 driver definitions
 *
 * Copyright 2022 Jacek Caban for CodeWeavers
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

#ifndef __WINE_X11DRV_DLL_H
#define __WINE_X11DRV_DLL_H

#include <stdarg.h>
#include "windef.h"
#include "winbase.h"
#include "ntgdi.h"
#include "unixlib.h"

extern NTSTATUS WINAPI x11drv_dnd_enter_event( void *params, ULONG size ) DECLSPEC_HIDDEN;
extern NTSTATUS WINAPI x11drv_dnd_position_event( void *params, ULONG size ) DECLSPEC_HIDDEN;
extern NTSTATUS WINAPI x11drv_dnd_post_drop( void *data, ULONG size ) DECLSPEC_HIDDEN;
extern NTSTATUS WINAPI x11drv_ime_set_composition_string( void *params, ULONG size ) DECLSPEC_HIDDEN;
extern NTSTATUS WINAPI x11drv_ime_set_result( void *params, ULONG size ) DECLSPEC_HIDDEN;
extern NTSTATUS WINAPI x11drv_systray_change_owner( void *params, ULONG size ) DECLSPEC_HIDDEN;

extern NTSTATUS x11drv_dnd_drop_event( UINT arg ) DECLSPEC_HIDDEN;
extern NTSTATUS x11drv_dnd_leave_event( UINT arg ) DECLSPEC_HIDDEN;
extern NTSTATUS x11drv_ime_get_cursor_pos( UINT arg ) DECLSPEC_HIDDEN;
extern NTSTATUS x11drv_ime_set_composition_status( UINT arg ) DECLSPEC_HIDDEN;
extern NTSTATUS x11drv_ime_set_cursor_pos( UINT pos ) DECLSPEC_HIDDEN;
extern NTSTATUS x11drv_ime_set_open_status( UINT open ) DECLSPEC_HIDDEN;
extern NTSTATUS x11drv_ime_update_association( UINT arg ) DECLSPEC_HIDDEN;

extern LRESULT WINAPI foreign_window_proc( HWND hwnd, UINT msg, WPARAM wparam,
                                           LPARAM lparam ) DECLSPEC_HIDDEN;

extern BOOL show_systray DECLSPEC_HIDDEN;
extern HMODULE x11drv_module DECLSPEC_HIDDEN;

#endif /* __WINE_X11DRV_DLL_H */
