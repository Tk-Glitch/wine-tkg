/******************************************************************************
 * winspool internal include file
 *
 *
 * Copyright 2005  Huw Davies
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

#include <windef.h>
#include <winuser.h>
#include <winternl.h>

extern HINSTANCE WINSPOOL_hInstance DECLSPEC_HIDDEN;

extern PRINTPROVIDOR * backend DECLSPEC_HIDDEN;
extern BOOL load_backend(void) DECLSPEC_HIDDEN;

extern void WINSPOOL_LoadSystemPrinters(void) DECLSPEC_HIDDEN;

#define IDS_CAPTION       10
#define IDS_FILE_EXISTS   11
#define IDS_CANNOT_OPEN   12

#define FILENAME_DIALOG  100
#define EDITBOX 201

struct printer_info
{
    WCHAR *name;
    WCHAR *comment;
    WCHAR *location;
    BOOL is_default;
};

struct enum_printers_params
{
    struct printer_info *printers;
    unsigned int *size;
    unsigned int *num;
};

struct get_default_page_size_params
{
    WCHAR *name;
    unsigned int *name_size;
};

struct get_ppd_params
{
    const WCHAR *printer; /* set to NULL to unlink */
    const WCHAR *ppd;
};

struct schedule_job_params
{
    const WCHAR *filename;
    const WCHAR *port;
    const WCHAR *document_title;
    const WCHAR *wine_port;
};

extern unixlib_handle_t winspool_handle DECLSPEC_HIDDEN;

#define UNIX_CALL( func, params ) __wine_unix_call( winspool_handle, unix_ ## func, params )

enum cups_funcs
{
    unix_process_attach,
    unix_enum_printers,
    unix_get_default_page_size,
    unix_get_ppd,
    unix_schedule_job,
};
