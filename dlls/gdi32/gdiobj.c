/*
 * GDI functions
 *
 * Copyright 1993 Alexandre Julliard
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

#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winreg.h"
#include "winnls.h"
#include "winerror.h"
#include "winternl.h"

#include "ntgdi_private.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(gdi);

#define FIRST_GDI_HANDLE 32

static GDI_SHARED_MEMORY gdi_shared;
static GDI_HANDLE_ENTRY *next_free;
static GDI_HANDLE_ENTRY *next_unused = gdi_shared.Handles + FIRST_GDI_HANDLE;
static LONG debug_count;
HMODULE gdi32_module = 0;
SYSTEM_BASIC_INFORMATION system_info;

static inline HGDIOBJ entry_to_handle( GDI_HANDLE_ENTRY *entry )
{
    unsigned int idx = entry - gdi_shared.Handles;
    return LongToHandle( idx | (entry->Unique << NTGDI_HANDLE_TYPE_SHIFT) );
}

static inline GDI_HANDLE_ENTRY *handle_entry( HGDIOBJ handle )
{
    unsigned int idx = LOWORD(handle);

    if (idx < GDI_MAX_HANDLE_COUNT && gdi_shared.Handles[idx].Type)
    {
        if (!HIWORD( handle ) || HIWORD( handle ) == gdi_shared.Handles[idx].Unique)
            return &gdi_shared.Handles[idx];
    }
    if (handle) WARN( "invalid handle %p\n", handle );
    return NULL;
}

static inline struct gdi_obj_header *entry_obj( GDI_HANDLE_ENTRY *entry )
{
    return (struct gdi_obj_header *)(ULONG_PTR)entry->Object;
}

/***********************************************************************
 *          GDI stock objects
 */

static const LOGBRUSH WhiteBrush = { BS_SOLID, RGB(255,255,255), 0 };
static const LOGBRUSH BlackBrush = { BS_SOLID, RGB(0,0,0), 0 };
static const LOGBRUSH NullBrush  = { BS_NULL, 0, 0 };

static const LOGBRUSH LtGrayBrush = { BS_SOLID, RGB(192,192,192), 0 };
static const LOGBRUSH GrayBrush   = { BS_SOLID, RGB(128,128,128), 0 };
static const LOGBRUSH DkGrayBrush = { BS_SOLID, RGB(64,64,64), 0 };

static const LOGBRUSH DCBrush = { BS_SOLID, RGB(255,255,255), 0 };

static CRITICAL_SECTION gdi_section;
static CRITICAL_SECTION_DEBUG critsect_debug =
{
    0, 0, &gdi_section,
    { &critsect_debug.ProcessLocksList, &critsect_debug.ProcessLocksList },
      0, 0, { (DWORD_PTR)(__FILE__ ": gdi_section") }
};
static CRITICAL_SECTION gdi_section = { &critsect_debug, -1, 0, 0, 0, 0 };


/****************************************************************************
 *
 *	language-independent stock fonts
 *
 */

static const LOGFONTW OEMFixedFont =
{ 12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, OEM_CHARSET,
  0, 0, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"" };

static const LOGFONTW AnsiFixedFont =
{ 12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
  0, 0, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Courier" };

static const LOGFONTW AnsiVarFont =
{ 12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
  0, 0, DEFAULT_QUALITY, VARIABLE_PITCH | FF_SWISS, L"MS Sans Serif" };

/******************************************************************************
 *
 *      language-dependent stock fonts
 *
 *      'ANSI' charset and 'DEFAULT' charset is not same.
 *      The chars in CP_ACP should be drawn with 'DEFAULT' charset.
 *      'ANSI' charset seems to be identical with ISO-8859-1.
 *      'DEFAULT' charset is a language-dependent charset.
 *
 *      'System' font seems to be an alias for language-dependent font.
 */

/*
 * language-dependent stock fonts for all known charsets
 * please see TranslateCharsetInfo (dlls/gdi/font.c) and
 * CharsetBindingInfo (dlls/x11drv/xfont.c),
 * and modify entries for your language if needed.
 */
struct DefaultFontInfo
{
        UINT            charset;
        LOGFONTW        SystemFont;
        LOGFONTW        DeviceDefaultFont;
        LOGFONTW        SystemFixedFont;
        LOGFONTW        DefaultGuiFont;
};

static const struct DefaultFontInfo default_fonts[] =
{
    {   ANSI_CHARSET,
        { /* System */
          16, 7, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET,
           0, 0, DEFAULT_QUALITY, VARIABLE_PITCH | FF_SWISS, L"System"
        },
        { /* Device Default */
          16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET,
           0, 0, DEFAULT_QUALITY, VARIABLE_PITCH | FF_SWISS, L"System"
        },
        { /* System Fixed */
          16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
           0, 0, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Courier"
        },
        { /* DefaultGuiFont */
          -11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
           0, 0, DEFAULT_QUALITY, VARIABLE_PITCH | FF_SWISS, L"MS Shell Dlg"
        },
    },
    {   EASTEUROPE_CHARSET,
        { /* System */
          16, 7, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, EASTEUROPE_CHARSET,
           0, 0, DEFAULT_QUALITY, VARIABLE_PITCH | FF_SWISS, L"System"
        },
        { /* Device Default */
          16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, EASTEUROPE_CHARSET,
           0, 0, DEFAULT_QUALITY, VARIABLE_PITCH | FF_SWISS, L"System"
        },
        { /* System Fixed */
          16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, EASTEUROPE_CHARSET,
           0, 0, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Courier"
        },
        { /* DefaultGuiFont */
          -11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, EASTEUROPE_CHARSET,
           0, 0, DEFAULT_QUALITY, VARIABLE_PITCH | FF_SWISS, L"MS Shell Dlg"
        },
    },
    {   RUSSIAN_CHARSET,
        { /* System */
          16, 7, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, RUSSIAN_CHARSET,
           0, 0, DEFAULT_QUALITY, VARIABLE_PITCH | FF_SWISS, L"System"
        },
        { /* Device Default */
          16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, RUSSIAN_CHARSET,
           0, 0, DEFAULT_QUALITY, VARIABLE_PITCH | FF_SWISS, L"System"
        },
        { /* System Fixed */
          16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, RUSSIAN_CHARSET,
           0, 0, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Courier"
        },
        { /* DefaultGuiFont */
          -11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, RUSSIAN_CHARSET,
           0, 0, DEFAULT_QUALITY, VARIABLE_PITCH | FF_SWISS, L"MS Shell Dlg"
        },
    },
    {   GREEK_CHARSET,
        { /* System */
          16, 7, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, GREEK_CHARSET,
           0, 0, DEFAULT_QUALITY, VARIABLE_PITCH | FF_SWISS, L"System"
        },
        { /* Device Default */
          16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, GREEK_CHARSET,
           0, 0, DEFAULT_QUALITY, VARIABLE_PITCH | FF_SWISS, L"System"
        },
        { /* System Fixed */
          16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, GREEK_CHARSET,
           0, 0, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Courier"
        },
        { /* DefaultGuiFont */
          -11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, GREEK_CHARSET,
           0, 0, DEFAULT_QUALITY, VARIABLE_PITCH | FF_SWISS, L"MS Shell Dlg"
        },
    },
    {   TURKISH_CHARSET,
        { /* System */
          16, 7, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, TURKISH_CHARSET,
           0, 0, DEFAULT_QUALITY, VARIABLE_PITCH | FF_SWISS, L"System"
        },
        { /* Device Default */
          16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, TURKISH_CHARSET,
           0, 0, DEFAULT_QUALITY, VARIABLE_PITCH | FF_SWISS, L"System"
        },
        { /* System Fixed */
          16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, TURKISH_CHARSET,
           0, 0, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Courier"
        },
        { /* DefaultGuiFont */
          -11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, TURKISH_CHARSET,
           0, 0, DEFAULT_QUALITY, VARIABLE_PITCH | FF_SWISS, L"MS Shell Dlg"
        },
    },
    {   HEBREW_CHARSET,
        { /* System */
          16, 7, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, HEBREW_CHARSET,
           0, 0, DEFAULT_QUALITY, VARIABLE_PITCH | FF_SWISS, L"System"
        },
        { /* Device Default */
          16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, HEBREW_CHARSET,
           0, 0, DEFAULT_QUALITY, VARIABLE_PITCH | FF_SWISS, L"System"
        },
        { /* System Fixed */
          16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, HEBREW_CHARSET,
           0, 0, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Courier"
        },
        { /* DefaultGuiFont */
          -11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, HEBREW_CHARSET,
           0, 0, DEFAULT_QUALITY, VARIABLE_PITCH | FF_SWISS, L"MS Shell Dlg"
        },
    },
    {   ARABIC_CHARSET,
        { /* System */
          16, 7, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ARABIC_CHARSET,
           0, 0, DEFAULT_QUALITY, VARIABLE_PITCH | FF_SWISS, L"System"
        },
        { /* Device Default */
          16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ARABIC_CHARSET,
           0, 0, DEFAULT_QUALITY, VARIABLE_PITCH | FF_SWISS, L"System"
        },
        { /* System Fixed */
          16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ARABIC_CHARSET,
           0, 0, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Courier"
        },
        { /* DefaultGuiFont */
          -11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ARABIC_CHARSET,
           0, 0, DEFAULT_QUALITY, VARIABLE_PITCH | FF_SWISS, L"MS Shell Dlg"
        },
    },
    {   BALTIC_CHARSET,
        { /* System */
          16, 7, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, BALTIC_CHARSET,
           0, 0, DEFAULT_QUALITY, VARIABLE_PITCH | FF_SWISS, L"System"
        },
        { /* Device Default */
          16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, BALTIC_CHARSET,
           0, 0, DEFAULT_QUALITY, VARIABLE_PITCH | FF_SWISS, L"System"
        },
        { /* System Fixed */
          16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, BALTIC_CHARSET,
           0, 0, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Courier"
        },
        { /* DefaultGuiFont */
          -11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, BALTIC_CHARSET,
           0, 0, DEFAULT_QUALITY, VARIABLE_PITCH | FF_SWISS, L"MS Shell Dlg"
        },
    },
    {   THAI_CHARSET,
        { /* System */
          16, 7, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, THAI_CHARSET,
           0, 0, DEFAULT_QUALITY, VARIABLE_PITCH | FF_SWISS, L"System"
        },
        { /* Device Default */
          16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, THAI_CHARSET,
           0, 0, DEFAULT_QUALITY, VARIABLE_PITCH | FF_SWISS, L"System"
        },
        { /* System Fixed */
          16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, THAI_CHARSET,
           0, 0, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Courier"
        },
        { /* DefaultGuiFont */
          -11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, THAI_CHARSET,
           0, 0, DEFAULT_QUALITY, VARIABLE_PITCH | FF_SWISS, L"MS Shell Dlg"
        },
    },
    {   SHIFTJIS_CHARSET,
        { /* System */
          18, 8, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, SHIFTJIS_CHARSET,
           0, 0, DEFAULT_QUALITY, VARIABLE_PITCH | FF_SWISS, L"System"
        },
        { /* Device Default */
          18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, SHIFTJIS_CHARSET,
           0, 0, DEFAULT_QUALITY, VARIABLE_PITCH | FF_SWISS, L"System"
        },
        { /* System Fixed */
          16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, SHIFTJIS_CHARSET,
           0, 0, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Courier"
        },
        { /* DefaultGuiFont */
          -12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, SHIFTJIS_CHARSET,
           0, 0, DEFAULT_QUALITY, VARIABLE_PITCH | FF_SWISS, L"MS Shell Dlg"
        },
    },
    {   GB2312_CHARSET,
        { /* System */
          16, 7, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, GB2312_CHARSET,
           0, 0, DEFAULT_QUALITY, VARIABLE_PITCH | FF_SWISS, L"System"
        },
        { /* Device Default */
          16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, GB2312_CHARSET,
           0, 0, DEFAULT_QUALITY, VARIABLE_PITCH | FF_SWISS, L"System"
        },
        { /* System Fixed */
          16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, GB2312_CHARSET,
           0, 0, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Courier"
        },
        { /* DefaultGuiFont */
          -12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, GB2312_CHARSET,
           0, 0, DEFAULT_QUALITY, VARIABLE_PITCH | FF_SWISS, L"MS Shell Dlg"
        },
    },
    {   HANGEUL_CHARSET,
        { /* System */
          16, 8, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, HANGEUL_CHARSET,
           0, 0, DEFAULT_QUALITY, VARIABLE_PITCH | FF_SWISS, L"System"
        },
        { /* Device Default */
          16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, HANGEUL_CHARSET,
           0, 0, DEFAULT_QUALITY, VARIABLE_PITCH | FF_SWISS, L"System"
        },
        { /* System Fixed */
          16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, HANGEUL_CHARSET,
           0, 0, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Courier"
        },
        { /* DefaultGuiFont */
          -12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, HANGEUL_CHARSET,
           0, 0, DEFAULT_QUALITY, VARIABLE_PITCH | FF_SWISS, L"MS Shell Dlg"
        },
    },
    {   CHINESEBIG5_CHARSET,
        { /* System */
          16, 7, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, CHINESEBIG5_CHARSET,
           0, 0, DEFAULT_QUALITY, VARIABLE_PITCH | FF_SWISS, L"System"
        },
        { /* Device Default */
          16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, CHINESEBIG5_CHARSET,
           0, 0, DEFAULT_QUALITY, VARIABLE_PITCH | FF_SWISS, L"System"
        },
        { /* System Fixed */
          16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, CHINESEBIG5_CHARSET,
           0, 0, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Courier"
        },
        { /* DefaultGuiFont */
          -12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, CHINESEBIG5_CHARSET,
           0, 0, DEFAULT_QUALITY, VARIABLE_PITCH | FF_SWISS, L"MS Shell Dlg"
        },
    },
    {   JOHAB_CHARSET,
        { /* System */
          16, 7, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, JOHAB_CHARSET,
           0, 0, DEFAULT_QUALITY, VARIABLE_PITCH | FF_SWISS, L"System"
        },
        { /* Device Default */
          16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, JOHAB_CHARSET,
           0, 0, DEFAULT_QUALITY, VARIABLE_PITCH | FF_SWISS, L"System"
        },
        { /* System Fixed */
          16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, JOHAB_CHARSET,
           0, 0, DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Courier"
        },
        { /* DefaultGuiFont */
          -12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, JOHAB_CHARSET,
           0, 0, DEFAULT_QUALITY, VARIABLE_PITCH | FF_SWISS, L"MS Shell Dlg"
        },
    },
};


/*************************************************************************
 * __wine_make_gdi_object_system    (win32u.@)
 *
 * USER has to tell GDI that its system brushes and pens are non-deletable.
 * For a description of the GDI object magics and their flags,
 * see "Undocumented Windows" (wrong about the OBJECT_NOSYSTEM flag, though).
 */
void CDECL __wine_make_gdi_object_system( HGDIOBJ handle, BOOL set)
{
    GDI_HANDLE_ENTRY *entry;

    EnterCriticalSection( &gdi_section );
    if ((entry = handle_entry( handle ))) entry_obj( entry )->system = !!set;
    LeaveCriticalSection( &gdi_section );
}

/******************************************************************************
 *      get_default_fonts
 */
static const struct DefaultFontInfo* get_default_fonts(UINT charset)
{
        unsigned int n;

        for(n = 0; n < ARRAY_SIZE( default_fonts ); n++)
        {
                if ( default_fonts[n].charset == charset )
                        return &default_fonts[n];
        }

        FIXME( "unhandled charset 0x%08x - use ANSI_CHARSET for default stock objects\n", charset );
        return &default_fonts[0];
}


/******************************************************************************
 *      get_default_charset    (internal)
 *
 * get the language-dependent charset that can handle CP_ACP correctly.
 */
static UINT get_default_charset( void )
{
    CHARSETINFO     csi;
    UINT    uACP;

    uACP = GetACP();
    csi.ciCharset = ANSI_CHARSET;
    if ( !translate_charset_info( ULongToPtr(uACP), &csi, TCI_SRCCODEPAGE ) )
    {
        FIXME( "unhandled codepage %u - use ANSI_CHARSET for default stock objects\n", uACP );
        return ANSI_CHARSET;
    }

    return csi.ciCharset;
}


/***********************************************************************
 *           GDI_get_ref_count
 *
 * Retrieve the reference count of a GDI object.
 * Note: the object must be locked otherwise the count is meaningless.
 */
UINT GDI_get_ref_count( HGDIOBJ handle )
{
    GDI_HANDLE_ENTRY *entry;
    UINT ret = 0;

    EnterCriticalSection( &gdi_section );
    if ((entry = handle_entry( handle ))) ret = entry_obj( entry )->selcount;
    LeaveCriticalSection( &gdi_section );
    return ret;
}


/***********************************************************************
 *           GDI_inc_ref_count
 *
 * Increment the reference count of a GDI object.
 */
HGDIOBJ GDI_inc_ref_count( HGDIOBJ handle )
{
    GDI_HANDLE_ENTRY *entry;

    EnterCriticalSection( &gdi_section );
    if ((entry = handle_entry( handle ))) entry_obj( entry )->selcount++;
    else handle = 0;
    LeaveCriticalSection( &gdi_section );
    return handle;
}


/***********************************************************************
 *           GDI_dec_ref_count
 *
 * Decrement the reference count of a GDI object.
 */
BOOL GDI_dec_ref_count( HGDIOBJ handle )
{
    GDI_HANDLE_ENTRY *entry;

    EnterCriticalSection( &gdi_section );
    if ((entry = handle_entry( handle )))
    {
        assert( entry_obj( entry )->selcount );
        if (!--entry_obj( entry )->selcount && entry_obj( entry )->deleted)
        {
            /* handle delayed DeleteObject*/
            entry_obj( entry )->deleted = 0;
            LeaveCriticalSection( &gdi_section );
            TRACE( "executing delayed DeleteObject for %p\n", handle );
            NtGdiDeleteObjectApp( handle );
            return TRUE;
        }
    }
    LeaveCriticalSection( &gdi_section );
    return entry != NULL;
}


/******************************************************************************
 *              get_reg_dword
 *
 * Read a DWORD value from the registry
 */
static BOOL get_reg_dword(HKEY base, const WCHAR *key_name, const WCHAR *value_name, DWORD *value)
{
    HKEY key;
    DWORD type, data, size = sizeof(data);
    BOOL ret = FALSE;

    if (RegOpenKeyW(base, key_name, &key) == ERROR_SUCCESS)
    {
        if (RegQueryValueExW(key, value_name, NULL, &type, (void *)&data, &size) == ERROR_SUCCESS &&
            type == REG_DWORD)
        {
            *value = data;
            ret = TRUE;
        }
        RegCloseKey(key);
    }
    return ret;
}

/******************************************************************************
 *              get_dpi
 *
 * get the dpi from the registry
 */
DWORD get_dpi(void)
{
    DWORD dpi;

    if (get_reg_dword(HKEY_CURRENT_USER, L"Control Panel\\Desktop", L"LogPixels", &dpi))
        return dpi;
    if (get_reg_dword(HKEY_CURRENT_CONFIG, L"Software\\Fonts", L"LogPixels", &dpi))
        return dpi;
    return 0;
}

/******************************************************************************
 *              get_system_dpi
 *
 * Get the system DPI, based on the DPI awareness mode.
 */
DWORD get_system_dpi(void)
{
    static UINT (WINAPI *pGetDpiForSystem)(void);

    if (!pGetDpiForSystem)
    {
        HMODULE user = GetModuleHandleW( L"user32.dll" );
        if (user) pGetDpiForSystem = (void *)GetProcAddress( user, "GetDpiForSystem" );
    }
    return pGetDpiForSystem ? pGetDpiForSystem() : 96;
}

static HFONT create_font( const LOGFONTW *deffont )
{
    ENUMLOGFONTEXDVW lf;

    memset( &lf, 0, sizeof(lf) );
    lf.elfEnumLogfontEx.elfLogFont = *deffont;
    return NtGdiHfontCreate( &lf, sizeof(lf), 0, 0, NULL );
}

static HFONT create_scaled_font( const LOGFONTW *deffont )
{
    LOGFONTW lf;
    static DWORD dpi;

    if (!dpi)
    {
        dpi = get_dpi();
        if (!dpi) dpi = 96;
    }

    lf = *deffont;
    lf.lfHeight = muldiv( lf.lfHeight, dpi, 96 );
    return create_font( &lf );
}

static void set_gdi_shared(void)
{
#ifndef _WIN64
    if (NtCurrentTeb()->GdiBatchCount)
    {
        TEB64 *teb64 = (TEB64 *)(UINT_PTR)NtCurrentTeb()->GdiBatchCount;
        PEB64 *peb64 = (PEB64 *)(UINT_PTR)teb64->Peb;
        peb64->GdiSharedHandleTable = (UINT_PTR)&gdi_shared;
        return;
    }
#endif
    /* NOTE: Windows uses 32-bit for 32-bit kernel */
    NtCurrentTeb()->Peb->GdiSharedHandleTable = &gdi_shared;
}

HGDIOBJ get_stock_object( INT obj )
{
    assert( obj >= 0 && obj <= STOCK_LAST + 1 && obj != 9 );

    switch (obj)
    {
    case OEM_FIXED_FONT:
        if (get_system_dpi() != 96) obj = 9;
        break;
    case SYSTEM_FONT:
        if (get_system_dpi() != 96) obj = STOCK_LAST + 2;
        break;
    case SYSTEM_FIXED_FONT:
        if (get_system_dpi() != 96) obj = STOCK_LAST + 3;
        break;
    case DEFAULT_GUI_FONT:
        if (get_system_dpi() != 96) obj = STOCK_LAST + 4;
        break;
    }

    return entry_to_handle( handle_entry( ULongToHandle( obj + FIRST_GDI_HANDLE )));
}

static void init_stock_objects(void)
{
    const struct DefaultFontInfo *deffonts;
    unsigned int i;
    HGDIOBJ obj;

    /* Create stock objects in order matching stock object macros,
     * so that they use predictable handle slots. Our GetStockObject
     * depends on it. */
    create_brush( &WhiteBrush );
    create_brush( &LtGrayBrush );
    create_brush( &GrayBrush );
    create_brush( &DkGrayBrush );
    create_brush( &BlackBrush );
    create_brush( &NullBrush );

    create_pen( PS_SOLID, 0, RGB(255,255,255) );
    create_pen( PS_SOLID, 0, RGB(0,0,0) );
    create_pen( PS_NULL,  0, RGB(0,0,0) );

    /* slot 9 is not used for non-scaled stock objects */
    create_scaled_font( &OEMFixedFont );

    /* language-independent stock fonts */
    create_font( &OEMFixedFont );
    create_font( &AnsiFixedFont );
    create_font( &AnsiVarFont );

    /* language-dependent stock fonts */
    deffonts = get_default_fonts(get_default_charset());
    create_font( &deffonts->SystemFont );
    create_font( &deffonts->DeviceDefaultFont );

    PALETTE_Init();

    create_font( &deffonts->SystemFixedFont );
    create_font( &deffonts->DefaultGuiFont );

    create_brush( &DCBrush );
    NtGdiCreatePen( PS_SOLID, 0, RGB(0,0,0), NULL );

    obj = NtGdiCreateBitmap( 1, 1, 1, 1, NULL );

    assert( (HandleToULong( obj ) & 0xffff) == FIRST_GDI_HANDLE + DEFAULT_BITMAP );

    create_scaled_font( &deffonts->SystemFont );
    create_scaled_font( &deffonts->SystemFixedFont );
    create_scaled_font( &deffonts->DefaultGuiFont );

    /* clear the NOSYSTEM bit on all stock objects*/
    for (i = 0; i < STOCK_LAST + 5; i++)
    {
        GDI_HANDLE_ENTRY *entry = &gdi_shared.Handles[FIRST_GDI_HANDLE + i];
        entry_obj( entry )->system = TRUE;
        entry->StockFlag = 1;
    }
}

/***********************************************************************
 *           DllMain
 *
 * GDI initialization.
 */
BOOL WINAPI DllMain( HINSTANCE inst, DWORD reason, LPVOID reserved )
{
    if (reason != DLL_PROCESS_ATTACH) return TRUE;

    gdi32_module = inst;
    DisableThreadLibraryCalls( inst );
    NtQuerySystemInformation( SystemBasicInformation, &system_info, sizeof(system_info), NULL );
    set_gdi_shared();
    font_init();
    init_stock_objects();
    return TRUE;
}


static const char *gdi_obj_type( unsigned type )
{
    switch ( type )
    {
        case NTGDI_OBJ_PEN: return "NTGDI_OBJ_PEN";
        case NTGDI_OBJ_BRUSH: return "NTGDI_OBJ_BRUSH";
        case NTGDI_OBJ_DC: return "NTGDI_OBJ_DC";
        case NTGDI_OBJ_METADC: return "NTGDI_OBJ_METADC";
        case NTGDI_OBJ_PAL: return "NTGDI_OBJ_PAL";
        case NTGDI_OBJ_FONT: return "NTGDI_OBJ_FONT";
        case NTGDI_OBJ_BITMAP: return "NTGDI_OBJ_BITMAP";
        case NTGDI_OBJ_REGION: return "NTGDI_OBJ_REGION";
        case NTGDI_OBJ_METAFILE: return "NTGDI_OBJ_METAFILE";
        case NTGDI_OBJ_MEMDC: return "NTGDI_OBJ_MEMDC";
        case NTGDI_OBJ_EXTPEN: return "NTGDI_OBJ_EXTPEN";
        case NTGDI_OBJ_ENHMETADC: return "NTGDI_OBJ_ENHMETADC";
        case NTGDI_OBJ_ENHMETAFILE: return "NTGDI_OBJ_ENHMETAFILE";
        default: return "UNKNOWN";
    }
}

static void dump_gdi_objects( void )
{
    GDI_HANDLE_ENTRY *entry;

    TRACE( "%u objects:\n", GDI_MAX_HANDLE_COUNT );

    EnterCriticalSection( &gdi_section );
    for (entry = gdi_shared.Handles; entry < next_unused; entry++)
    {
        if (!entry->Type)
            TRACE( "handle %p FREE\n", entry_to_handle( entry ));
        else
            TRACE( "handle %p obj %s type %s selcount %u deleted %u\n",
                   entry_to_handle( entry ), wine_dbgstr_longlong( entry->Object ),
                   gdi_obj_type( entry->ExtType << NTGDI_HANDLE_TYPE_SHIFT ),
                   entry_obj( entry )->selcount, entry_obj( entry )->deleted );
    }
    LeaveCriticalSection( &gdi_section );
}

/***********************************************************************
 *           alloc_gdi_handle
 *
 * Allocate a GDI handle for an object, which must have been allocated on the process heap.
 */
HGDIOBJ alloc_gdi_handle( struct gdi_obj_header *obj, DWORD type, const struct gdi_obj_funcs *funcs )
{
    GDI_HANDLE_ENTRY *entry;
    HGDIOBJ ret;

    assert( type );  /* type 0 is reserved to mark free entries */

    EnterCriticalSection( &gdi_section );

    entry = next_free;
    if (entry)
        next_free = (GDI_HANDLE_ENTRY *)(UINT_PTR)entry->Object;
    else if (next_unused < gdi_shared.Handles + GDI_MAX_HANDLE_COUNT)
        entry = next_unused++;
    else
    {
        LeaveCriticalSection( &gdi_section );
        ERR( "out of GDI object handles, expect a crash\n" );
        if (TRACE_ON(gdi)) dump_gdi_objects();
        return 0;
    }
    obj->funcs    = funcs;
    obj->selcount = 0;
    obj->system   = 0;
    obj->deleted  = 0;
    entry->Object  = (UINT_PTR)obj;
    entry->ExtType = type >> NTGDI_HANDLE_TYPE_SHIFT;
    entry->Type    = entry->ExtType & 0x1f;
    if (++entry->Generation == 0xff) entry->Generation = 1;
    ret = entry_to_handle( entry );
    LeaveCriticalSection( &gdi_section );
    TRACE( "allocated %s %p %u/%u\n", gdi_obj_type(type), ret,
           InterlockedIncrement( &debug_count ), GDI_MAX_HANDLE_COUNT );
    return ret;
}


/***********************************************************************
 *           free_gdi_handle
 *
 * Free a GDI handle and return a pointer to the object.
 */
void *free_gdi_handle( HGDIOBJ handle )
{
    void *object = NULL;
    GDI_HANDLE_ENTRY *entry;

    EnterCriticalSection( &gdi_section );
    if ((entry = handle_entry( handle )))
    {
        TRACE( "freed %s %p %u/%u\n", gdi_obj_type( entry->ExtType << NTGDI_HANDLE_TYPE_SHIFT ),
               handle, InterlockedDecrement( &debug_count ) + 1, GDI_MAX_HANDLE_COUNT );
        object = entry_obj( entry );
        entry->Type = 0;
        entry->Object = (UINT_PTR)next_free;
        next_free = entry;
    }
    LeaveCriticalSection( &gdi_section );
    return object;
}

DWORD get_gdi_object_type( HGDIOBJ obj )
{
    GDI_HANDLE_ENTRY *entry = handle_entry( obj );
    return entry ? entry->ExtType << NTGDI_HANDLE_TYPE_SHIFT : 0;
}

/***********************************************************************
 *           get_any_obj_ptr
 *
 * Return a pointer to, and the type of, the GDI object
 * associated with the handle.
 * The object must be released with GDI_ReleaseObj.
 */
void *get_any_obj_ptr( HGDIOBJ handle, DWORD *type )
{
    void *ptr = NULL;
    GDI_HANDLE_ENTRY *entry;

    EnterCriticalSection( &gdi_section );

    if ((entry = handle_entry( handle )))
    {
        ptr = entry_obj( entry );
        *type = entry->ExtType << NTGDI_HANDLE_TYPE_SHIFT;
    }

    if (!ptr) LeaveCriticalSection( &gdi_section );
    return ptr;
}

/***********************************************************************
 *           GDI_GetObjPtr
 *
 * Return a pointer to the GDI object associated with the handle.
 * Return NULL if the object has the wrong type.
 * The object must be released with GDI_ReleaseObj.
 */
void *GDI_GetObjPtr( HGDIOBJ handle, DWORD type )
{
    DWORD ret_type;
    void *ptr = get_any_obj_ptr( handle, &ret_type );
    if (ptr && ret_type != type)
    {
        GDI_ReleaseObj( handle );
        ptr = NULL;
    }
    return ptr;
}

/***********************************************************************
 *           GDI_ReleaseObj
 *
 */
void GDI_ReleaseObj( HGDIOBJ handle )
{
    LeaveCriticalSection( &gdi_section );
}


/***********************************************************************
 *           GDI_CheckNotLock
 */
void GDI_CheckNotLock(void)
{
    if (RtlIsCriticalSectionLockedByThread(&gdi_section))
    {
        ERR( "BUG: holding GDI lock\n" );
        assert( 0 );
    }
}


/***********************************************************************
 *           NtGdiDeleteObjectApp    (win32u.@)
 *
 * Delete a Gdi object.
 *
 * PARAMS
 *  obj [I] Gdi object to delete
 *
 * RETURNS
 *  Success: TRUE. If obj was not returned from GetStockObject(), any resources
 *           it consumed are released.
 *  Failure: FALSE, if obj is not a valid Gdi object, or is currently selected
 *           into a DC.
 */
BOOL WINAPI NtGdiDeleteObjectApp( HGDIOBJ obj )
{
    GDI_HANDLE_ENTRY *entry;
    const struct gdi_obj_funcs *funcs = NULL;
    struct gdi_obj_header *header;

    EnterCriticalSection( &gdi_section );
    if (!(entry = handle_entry( obj )))
    {
        LeaveCriticalSection( &gdi_section );
        return FALSE;
    }

    header = entry_obj( entry );
    if (header->system)
    {
	TRACE("Preserving system object %p\n", obj);
        LeaveCriticalSection( &gdi_section );
	return TRUE;
    }

    obj = entry_to_handle( entry );  /* make it a full handle */

    if (header->selcount)
    {
        TRACE("delayed for %p because object in use, count %u\n", obj, header->selcount );
        header->deleted = 1;  /* mark for delete */
    }
    else funcs = header->funcs;

    LeaveCriticalSection( &gdi_section );

    TRACE("%p\n", obj );

    if (funcs && funcs->pDeleteObject) return funcs->pDeleteObject( obj );
    return TRUE;
}

/***********************************************************************
 *           NtGdiCreateClientObj    (win32u.@)
 */
HANDLE WINAPI NtGdiCreateClientObj( ULONG type )
{
    struct gdi_obj_header *obj;
    HGDIOBJ handle;

    if (!(obj = HeapAlloc( GetProcessHeap(), 0, sizeof(*obj) )))
        return 0;

    handle = alloc_gdi_handle( obj, type, NULL );
    if (!handle) HeapFree( GetProcessHeap(), 0, obj );
    return handle;
}

/***********************************************************************
 *           NtGdiDeleteClientObj    (win32u.@)
 */
BOOL WINAPI NtGdiDeleteClientObj( HGDIOBJ handle )
{
    void *obj;
    if (!(obj = free_gdi_handle( handle ))) return FALSE;
    HeapFree( GetProcessHeap(), 0, obj );
    return TRUE;
}


/***********************************************************************
 *           NtGdiExtGetObjectW    (win32u.@)
 */
INT WINAPI NtGdiExtGetObjectW( HGDIOBJ handle, INT count, void *buffer )
{
    GDI_HANDLE_ENTRY *entry;
    const struct gdi_obj_funcs *funcs = NULL;
    INT result = 0;

    TRACE("%p %d %p\n", handle, count, buffer );

    EnterCriticalSection( &gdi_section );
    if ((entry = handle_entry( handle )))
    {
        funcs = entry_obj( entry )->funcs;
        handle = entry_to_handle( entry );  /* make it a full handle */
    }
    LeaveCriticalSection( &gdi_section );

    if (funcs && funcs->pGetObjectW)
    {
        if (buffer && ((ULONG_PTR)buffer >> 16) == 0) /* catch apps getting argument order wrong */
            SetLastError( ERROR_NOACCESS );
        else
            result = funcs->pGetObjectW( handle, count, buffer );
    }
    return result;
}

/***********************************************************************
 *           NtGdiGetDCObject    (win32u.@)
 *
 * Get the currently selected object of a given type in a device context.
 */
HANDLE WINAPI NtGdiGetDCObject( HDC hdc, UINT type )
{
    HGDIOBJ ret = 0;
    DC *dc;

    if (!(dc = get_dc_ptr( hdc ))) return 0;

    switch (type)
    {
    case NTGDI_OBJ_EXTPEN: /* fall through */
    case NTGDI_OBJ_PEN:    ret = dc->hPen; break;
    case NTGDI_OBJ_BRUSH:  ret = dc->hBrush; break;
    case NTGDI_OBJ_PAL:    ret = dc->hPalette; break;
    case NTGDI_OBJ_FONT:   ret = dc->hFont; break;
    case NTGDI_OBJ_SURF:   ret = dc->hBitmap; break;
    default:
        FIXME( "(%p, %d): unknown type.\n", hdc, type );
        break;
    }
    release_dc_ptr( dc );
    return ret;
}


/***********************************************************************
 *           NtGdiUnrealizeObject    (win32u.@)
 */
BOOL WINAPI NtGdiUnrealizeObject( HGDIOBJ obj )
{
    const struct gdi_obj_funcs *funcs = NULL;
    GDI_HANDLE_ENTRY *entry;

    EnterCriticalSection( &gdi_section );
    if ((entry = handle_entry( obj )))
    {
        funcs = entry_obj( entry )->funcs;
        obj = entry_to_handle( entry );  /* make it a full handle */
    }
    LeaveCriticalSection( &gdi_section );

    if (funcs && funcs->pUnrealizeObject) return funcs->pUnrealizeObject( obj );
    return funcs != NULL;
}


/***********************************************************************
 *           NtGdiFlush    (win32u.@)
 */
BOOL WINAPI NtGdiFlush(void)
{
    return TRUE;  /* FIXME */
}


/*******************************************************************
 *           NtGdiGetColorAdjustment    (win32u.@)
 */
BOOL WINAPI NtGdiGetColorAdjustment( HDC hdc, COLORADJUSTMENT *ca )
{
    FIXME( "stub\n" );
    return FALSE;
}

/*******************************************************************
 *           NtGdiSetColorAdjustment    (win32u.@)
 */
BOOL WINAPI NtGdiSetColorAdjustment( HDC hdc, const COLORADJUSTMENT *ca )
{
    FIXME( "stub\n" );
    return FALSE;
}
