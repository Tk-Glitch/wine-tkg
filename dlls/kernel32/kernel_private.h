/*
 * Kernel32 undocumented and private functions definition
 *
 * Copyright 2003 Eric Pouech
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

#ifndef __WINE_KERNEL_PRIVATE_H
#define __WINE_KERNEL_PRIVATE_H

#include "wine/server.h"

NTSTATUS WINAPI BaseGetNamedObjectDirectory( HANDLE *dir );
BOOL           CONSOLE_Init(RTL_USER_PROCESS_PARAMETERS *params) DECLSPEC_HIDDEN;
BOOL           CONSOLE_Exit(void) DECLSPEC_HIDDEN;

static inline BOOL is_console_handle(HANDLE h)
{
    return h != INVALID_HANDLE_VALUE && ((UINT_PTR)h & 3) == 3;
}

/* map a real wineserver handle onto a kernel32 console handle */
static inline HANDLE console_handle_map(HANDLE h)
{
    return h != INVALID_HANDLE_VALUE ? (HANDLE)((UINT_PTR)h ^ 3) : INVALID_HANDLE_VALUE;
}

/* map a kernel32 console handle onto a real wineserver handle */
static inline obj_handle_t console_handle_unmap(HANDLE h)
{
    return wine_server_obj_handle( h != INVALID_HANDLE_VALUE ? (HANDLE)((UINT_PTR)h ^ 3) : INVALID_HANDLE_VALUE );
}

static inline BOOL set_ntstatus( NTSTATUS status )
{
    if (status) SetLastError( RtlNtStatusToDosError( status ));
    return !status;
}

/* Some Wine specific values for Console inheritance (params->ConsoleHandle) */
#define KERNEL32_CONSOLE_ALLOC          ((HANDLE)1)
#define KERNEL32_CONSOLE_SHELL          ((HANDLE)2)

extern HMODULE kernel32_handle DECLSPEC_HIDDEN;
extern SYSTEM_BASIC_INFORMATION system_info DECLSPEC_HIDDEN;

extern const WCHAR DIR_Windows[] DECLSPEC_HIDDEN;
extern const WCHAR DIR_System[] DECLSPEC_HIDDEN;

extern void FILE_SetDosError(void) DECLSPEC_HIDDEN;
extern WCHAR *FILE_name_AtoW( LPCSTR name, BOOL alloc ) DECLSPEC_HIDDEN;
extern DWORD FILE_name_WtoA( LPCWSTR src, INT srclen, LPSTR dest, INT destlen ) DECLSPEC_HIDDEN;

/* computername.c */
extern void COMPUTERNAME_Init(void) DECLSPEC_HIDDEN;

#endif
