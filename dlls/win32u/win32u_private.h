/*
 * Unix call wrappers
 *
 * Copyright 2021 Jacek Caban for CodeWeavers
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

#ifndef __WINE_WIN32U_PRIVATE
#define __WINE_WIN32U_PRIVATE

#include "winuser.h"
#include "wine/gdi_driver.h"
#include "wine/unixlib.h"

struct user_callbacks
{
    HWND (WINAPI *pGetDesktopWindow)(void);
    UINT (WINAPI *pGetDpiForSystem)(void);
    BOOL (WINAPI *pGetMonitorInfoW)( HMONITOR, LPMONITORINFO );
    INT (WINAPI *pGetSystemMetrics)(INT);
    BOOL (WINAPI *pGetWindowRect)( HWND hwnd, LPRECT rect );
    BOOL (WINAPI *pEnumDisplayMonitors)( HDC, LPRECT, MONITORENUMPROC, LPARAM );
    BOOL (WINAPI *pEnumDisplaySettingsW)(LPCWSTR, DWORD, LPDEVMODEW );
    BOOL (WINAPI *pRedrawWindow)( HWND, const RECT*, HRGN, UINT );
    DPI_AWARENESS_CONTEXT (WINAPI *pSetThreadDpiAwarenessContext)( DPI_AWARENESS_CONTEXT );
    HWND (WINAPI *pWindowFromDC)( HDC );
};

struct unix_funcs
{
    /* win32u functions */
    INT      (WINAPI *pNtGdiAbortDoc)( HDC hdc );
    BOOL     (WINAPI *pNtGdiAbortPath)( HDC hdc );
    BOOL     (WINAPI *pNtGdiAlphaBlend)( HDC hdc_dst, int x_dst, int y_dst, int width_dst, int height_dst,
                                         HDC hdc_src, int x_src, int y_src, int width_src, int height_src,
                                         BLENDFUNCTION blend_function, HANDLE xform );
    BOOL     (WINAPI *pNtGdiAngleArc)( HDC hdc, INT x, INT y, DWORD radius, FLOAT start_angle,
                                       FLOAT sweep_angle );
    BOOL     (WINAPI *pNtGdiArcInternal)( UINT type, HDC hdc, INT left, INT top, INT right, INT bottom,
                                          INT xstart, INT ystart, INT xend, INT yend );
    BOOL     (WINAPI *pNtGdiBeginPath)( HDC hdc );
    BOOL     (WINAPI *pNtGdiBitBlt)( HDC hdc_dst, INT x_dst, INT y_dst, INT width, INT height, HDC hdc_src,
                                     INT x_src, INT y_src, DWORD rop, DWORD bk_color, FLONG fl );
    BOOL     (WINAPI *pNtGdiCloseFigure)( HDC hdc );
    BOOL     (WINAPI *pNtGdiComputeXformCoefficients)( HDC hdc );
    HBITMAP  (WINAPI *pNtGdiCreateCompatibleBitmap)( HDC hdc, INT width, INT height );
    HDC      (WINAPI *pNtGdiCreateCompatibleDC)( HDC hdc );
    HBITMAP  (WINAPI *pNtGdiCreateDIBitmapInternal)( HDC hdc, INT width, INT height, DWORD init,
                                                     const void *bits, const BITMAPINFO *data,
                                                     UINT coloruse, UINT max_info, UINT max_bits,
                                                     ULONG flags, HANDLE xform );
    HDC      (WINAPI *pNtGdiCreateMetafileDC)( HDC hdc );
    NTSTATUS (WINAPI *pNtGdiDdDDICheckVidPnExclusiveOwnership)( const D3DKMT_CHECKVIDPNEXCLUSIVEOWNERSHIP *desc );
    NTSTATUS (WINAPI *pNtGdiDdDDICreateDCFromMemory)( D3DKMT_CREATEDCFROMMEMORY *desc );
    NTSTATUS (WINAPI *pNtGdiDdDDIDestroyDCFromMemory)( const D3DKMT_DESTROYDCFROMMEMORY *desc );
    NTSTATUS (WINAPI *pNtGdiDdDDIDestroyDevice)( const D3DKMT_DESTROYDEVICE *desc );
    NTSTATUS (WINAPI *pNtGdiDdDDIEscape)( const D3DKMT_ESCAPE *desc );
    NTSTATUS (WINAPI *pNtGdiDdDDISetVidPnSourceOwner)( const D3DKMT_SETVIDPNSOURCEOWNER *desc );
    BOOL     (WINAPI *pNtGdiDeleteObjectApp)( HGDIOBJ obj );
    LONG     (WINAPI *pNtGdiDoPalette)( HGDIOBJ handle, WORD start, WORD count, void *entries,
                                        DWORD func, BOOL inbound );
    BOOL     (WINAPI *pNtGdiEllipse)( HDC hdc, INT left, INT top, INT right, INT bottom );
    INT      (WINAPI *pNtGdiEndDoc)(HDC hdc);
    BOOL     (WINAPI *pNtGdiEndPath)( HDC hdc );
    INT      (WINAPI *pNtGdiEndPage)( HDC hdc );
    BOOL     (WINAPI *pNtGdiEnumFonts)( HDC hdc, ULONG type, ULONG win32_compat, ULONG face_name_len,
                                        const WCHAR *face_name, ULONG charset, ULONG *count, void *buf );
    INT      (WINAPI *pNtGdiExcludeClipRect)( HDC hdc, INT left, INT top, INT right, INT bottom );
    INT      (WINAPI *pNtGdiExtEscape)( HDC hdc, WCHAR *driver, INT driver_id, INT escape, INT input_size,
                                        const char *input, INT output_size, char *output );
    BOOL     (WINAPI *pNtGdiExtFloodFill)( HDC hdc, INT x, INT y, COLORREF color, UINT type );
    BOOL     (WINAPI *pNtGdiExtTextOutW)( HDC hdc, INT x, INT y, UINT flags, const RECT *rect,
                                          const WCHAR *str, UINT count, const INT *dx, DWORD cp );
    INT      (WINAPI *pNtGdiExtSelectClipRgn)( HDC hdc, HRGN region, INT mode );
    BOOL     (WINAPI *pNtGdiFillPath)( HDC hdc );
    BOOL     (WINAPI *pNtGdiFillRgn)( HDC hdc, HRGN hrgn, HBRUSH hbrush );
    BOOL     (WINAPI *pNtGdiFontIsLinked)( HDC hdc );
    BOOL     (WINAPI *pNtGdiFrameRgn)( HDC hdc, HRGN hrgn, HBRUSH brush, INT width, INT height );
    BOOL     (WINAPI *pNtGdiGetAndSetDCDword)( HDC hdc, UINT method, DWORD value, DWORD *result );
    INT      (WINAPI *pNtGdiGetAppClipBox)( HDC hdc, RECT *rect );
    UINT     (WINAPI *pNtGdiGetBoundsRect)( HDC hdc, RECT *rect, UINT flags );
    BOOL     (WINAPI *pNtGdiGetCharABCWidthsW)( HDC hdc, UINT first, UINT last, WCHAR *chars,
                                                ULONG flags, void *buffer );
    BOOL     (WINAPI *pNtGdiGetCharWidthW)( HDC hdc, UINT first_char, UINT last_char, WCHAR *chars,
                                            ULONG flags, void *buffer );
    BOOL     (WINAPI *pNtGdiGetCharWidthInfo)( HDC hdc, struct char_width_info *info );
    INT      (WINAPI *pNtGdiGetDIBitsInternal)( HDC hdc, HBITMAP hbitmap, UINT startscan, UINT lines,
                                                void *bits, BITMAPINFO *info, UINT coloruse,
                                                UINT max_bits, UINT max_info );
    INT      (WINAPI *pNtGdiGetDeviceCaps)( HDC hdc, INT cap );
    BOOL     (WINAPI *pNtGdiGetDeviceGammaRamp)( HDC hdc, void *ptr );
    DWORD    (WINAPI *pNtGdiGetFontData)( HDC hdc, DWORD table, DWORD offset, void *buffer, DWORD length );
    DWORD    (WINAPI *pNtGdiGetFontUnicodeRanges)( HDC hdc, GLYPHSET *lpgs );
    DWORD    (WINAPI *pNtGdiGetGlyphIndicesW)( HDC hdc, const WCHAR *str, INT count,
                                               WORD *indices, DWORD flags );
    DWORD    (WINAPI *pNtGdiGetGlyphOutline)( HDC hdc, UINT ch, UINT format, GLYPHMETRICS *metrics,
                                              DWORD size, void *buffer, const MAT2 *mat2,
                                              BOOL ignore_rotation );
    DWORD    (WINAPI *pNtGdiGetKerningPairs)( HDC hdc, DWORD count, KERNINGPAIR *kern_pair );
    COLORREF (WINAPI *pNtGdiGetNearestColor)( HDC hdc, COLORREF color );
    UINT     (WINAPI *pNtGdiGetOutlineTextMetricsInternalW)( HDC hdc, UINT cbData,
                                                             OUTLINETEXTMETRICW *otm, ULONG opts );
    COLORREF (WINAPI *pNtGdiGetPixel)( HDC hdc, INT x, INT y );
    INT      (WINAPI *pNtGdiGetRandomRgn)( HDC hdc, HRGN region, INT code );
    BOOL     (WINAPI *pNtGdiGetRasterizerCaps)( RASTERIZER_STATUS *status, UINT size );
    BOOL     (WINAPI *pNtGdiGetRealizationInfo)( HDC hdc, struct font_realization_info *info );
    UINT     (WINAPI *pNtGdiGetTextCharsetInfo)( HDC hdc, FONTSIGNATURE *fs, DWORD flags );
    BOOL     (WINAPI *pNtGdiGetTextExtentExW)( HDC hdc, const WCHAR *str, INT count, INT max_ext,
                                               INT *nfit, INT *dxs, SIZE *size, UINT flags );
    INT      (WINAPI *pNtGdiGetTextFaceW)( HDC hdc, INT count, WCHAR *name, BOOL alias_name );
    BOOL     (WINAPI *pNtGdiGetTextMetricsW)( HDC hdc, TEXTMETRICW *metrics, ULONG flags );
    BOOL     (WINAPI *pNtGdiGradientFill)( HDC hdc, TRIVERTEX *vert_array, ULONG nvert,
                                           void *grad_array, ULONG ngrad, ULONG mode );
    INT      (WINAPI *pNtGdiIntersectClipRect)( HDC hdc, INT left, INT top, INT right, INT bottom );
    BOOL     (WINAPI *pNtGdiInvertRgn)( HDC hdc, HRGN hrgn );
    BOOL     (WINAPI *pNtGdiLineTo)( HDC hdc, INT x, INT y );
    BOOL     (WINAPI *pNtGdiMaskBlt)( HDC hdc, INT x_dst, INT y_dst, INT width_dst, INT height_dst,
                                      HDC hdc_src, INT x_src, INT y_src, HBITMAP mask,
                                      INT x_mask, INT y_mask, DWORD rop, DWORD bk_color );
    BOOL     (WINAPI *pNtGdiModifyWorldTransform)( HDC hdc, const XFORM *xform, DWORD mode );
    BOOL     (WINAPI *pNtGdiMoveTo)( HDC hdc, INT x, INT y, POINT *pt );
    INT      (WINAPI *pNtGdiOffsetClipRgn)( HDC hdc, INT x, INT y );
    HDC      (WINAPI *pNtGdiOpenDCW)( UNICODE_STRING *device, const DEVMODEW *devmode,
                                      UNICODE_STRING *output, ULONG type, BOOL is_display,
                                      HANDLE hspool, DRIVER_INFO_2W *driver_info, void *pdev );
    BOOL     (WINAPI *pNtGdiPatBlt)( HDC hdc, INT left, INT top, INT width, INT height, DWORD rop );
    BOOL     (WINAPI *pNtGdiPlgBlt)( HDC hdc, const POINT *point, HDC hdc_src, INT x_src, INT y_src,
                                     INT width, INT height, HBITMAP mask, INT x_mask, INT y_mask,
                                     DWORD bk_color );
    BOOL     (WINAPI *pNtGdiPolyDraw)(HDC hdc, const POINT *points, const BYTE *types, DWORD count );
    ULONG    (WINAPI *pNtGdiPolyPolyDraw)( HDC hdc, const POINT *points, const UINT *counts,
                                           DWORD count, UINT function );
    BOOL     (WINAPI *pNtGdiPtVisible)( HDC hdc, INT x, INT y );
    BOOL     (WINAPI *pNtGdiRectVisible)( HDC hdc, const RECT *rect );
    BOOL     (WINAPI *pNtGdiRectangle)( HDC hdc, INT left, INT top, INT right, INT bottom );
    BOOL     (WINAPI *pNtGdiResetDC)( HDC hdc, const DEVMODEW *devmode, BOOL *banding,
                                      DRIVER_INFO_2W *driver_info, void *dev );
    BOOL     (WINAPI *pNtGdiResizePalette)( HPALETTE palette, UINT count );
    BOOL     (WINAPI *pNtGdiRestoreDC)( HDC hdc, INT level );
    BOOL     (WINAPI *pNtGdiRoundRect)( HDC hdc, INT left, INT top, INT right,
                                        INT bottom, INT ell_width, INT ell_height );
    BOOL     (WINAPI *pNtGdiScaleViewportExtEx)( HDC hdc, INT x_num, INT x_denom,
                                                 INT y_num, INT y_denom, SIZE *size );
    BOOL     (WINAPI *pNtGdiScaleWindowExtEx)( HDC hdc, INT x_num, INT x_denom,
                                               INT y_num, INT y_denom, SIZE *size );
    HGDIOBJ  (WINAPI *pNtGdiSelectBitmap)( HDC hdc, HGDIOBJ handle );
    HGDIOBJ  (WINAPI *pNtGdiSelectBrush)( HDC hdc, HGDIOBJ handle );
    BOOL     (WINAPI *pNtGdiSelectClipPath)( HDC hdc, INT mode );
    HGDIOBJ  (WINAPI *pNtGdiSelectFont)( HDC hdc, HGDIOBJ handle );
    HGDIOBJ  (WINAPI *pNtGdiSelectPen)( HDC hdc, HGDIOBJ handle );
    UINT     (WINAPI *pNtGdiSetBoundsRect)( HDC hdc, const RECT *rect, UINT flags );
    INT      (WINAPI *pNtGdiSetDIBitsToDeviceInternal)( HDC hdc, INT x_dst, INT y_dst, DWORD cx,
                                                        DWORD cy, INT x_src, INT y_src, UINT startscan,
                                                        UINT lines, const void *bits, const BITMAPINFO *bmi,
                                                        UINT coloruse, UINT max_bits, UINT max_info,
                                                        BOOL xform_coords, HANDLE xform );
    BOOL     (WINAPI *pNtGdiSetDeviceGammaRamp)( HDC hdc, void *ptr );
    DWORD    (WINAPI *pNtGdiSetLayout)( HDC hdc, LONG wox, DWORD layout );
    COLORREF (WINAPI *pNtGdiSetPixel)( HDC hdc, INT x, INT y, COLORREF color );
    UINT     (WINAPI *pNtGdiSetSystemPaletteUse)( HDC hdc, UINT use );
    INT      (WINAPI *pNtGdiStartDoc)( HDC hdc, const DOCINFOW *doc, BOOL *banding, INT job );
    INT      (WINAPI *pNtGdiStartPage)( HDC hdc );
    BOOL     (WINAPI *pNtGdiStretchBlt)( HDC hdc, INT x_dst, INT y_dst, INT width_dst, INT height_dst,
                                         HDC hdc_src, INT x_src, INT y_src, INT width_src, INT height_src,
                                         DWORD rop, COLORREF bk_color );
    INT      (WINAPI *pNtGdiStretchDIBitsInternal)( HDC hdc, INT x_dst, INT y_dst, INT width_dst,
                                                    INT height_dst, INT x_src, INT y_src, INT width_src,
                                                    INT height_src, const void *bits, const BITMAPINFO *bmi,
                                                    UINT coloruse, DWORD rop, UINT max_info, UINT max_bits,
                                                    HANDLE xform );
    BOOL     (WINAPI *pNtGdiStrokeAndFillPath)( HDC hdc );
    BOOL     (WINAPI *pNtGdiStrokePath)( HDC hdc );
    BOOL     (WINAPI *pNtGdiTransparentBlt)( HDC hdc, int x_dst, int y_dst, int width_dst, int height_dst,
                                             HDC hdc_src, int x_src, int y_src, int width_src, int height_src,
                                             UINT color );
    BOOL     (WINAPI *pNtGdiUnrealizeObject)( HGDIOBJ obj );
    BOOL     (WINAPI *pNtGdiUpdateColors)( HDC hdc );
    BOOL     (WINAPI *pNtGdiWidenPath)( HDC hdc );

    /* Wine-specific functions */
    UINT (WINAPI *pGDIRealizePalette)( HDC hdc );
    HPALETTE (WINAPI *pGDISelectPalette)( HDC hdc, HPALETTE hpal, WORD bkg );
    DWORD_PTR (WINAPI *pGetDCHook)( HDC hdc, DCHOOKPROC *proc );
    BOOL (WINAPI *pMirrorRgn)( HWND hwnd, HRGN hrgn );
    BOOL (WINAPI *pSetDCHook)( HDC hdc, DCHOOKPROC proc, DWORD_PTR data );
    INT (WINAPI *pSetDIBits)( HDC hdc, HBITMAP hbitmap, UINT startscan,
                              UINT lines, const void *bits, const BITMAPINFO *info,
                              UINT coloruse );
    WORD (WINAPI *pSetHookFlags)( HDC hdc, WORD flags );
    BOOL (CDECL *get_brush_bitmap_info)( HBRUSH handle, BITMAPINFO *info, void *bits, UINT *usage );
    BOOL (CDECL *get_file_outline_text_metric)( const WCHAR *path, OUTLINETEXTMETRICW *otm );
    BOOL (CDECL *get_icm_profile)( HDC hdc, BOOL allow_default, DWORD *size, WCHAR *filename );
    const struct vulkan_funcs * (CDECL *get_vulkan_driver)( HDC hdc, UINT version );
    struct opengl_funcs * (CDECL *get_wgl_driver)( HDC hdc, UINT version );
    void (CDECL *make_gdi_object_system)( HGDIOBJ handle, BOOL set );
    void (CDECL *set_display_driver)( void *proc );
    void (CDECL *set_visible_region)( HDC hdc, HRGN hrgn, const RECT *vis_rect, const RECT *device_rect,
                                      struct window_surface *surface );
};

UINT WINAPI GDIRealizePalette( HDC hdc );
HPALETTE WINAPI GDISelectPalette( HDC hdc, HPALETTE hpal, WORD wBkg );

extern void wrappers_init( unixlib_handle_t handle ) DECLSPEC_HIDDEN;
extern NTSTATUS gdi_init(void) DECLSPEC_HIDDEN;
extern NTSTATUS callbacks_init( void *args ) DECLSPEC_HIDDEN;
extern void winstation_init(void) DECLSPEC_HIDDEN;

extern HKEY reg_open_hkcu_key( const char *name ) DECLSPEC_HIDDEN;
extern HKEY reg_open_key( HKEY root, const WCHAR *name, ULONG name_len ) DECLSPEC_HIDDEN;
extern ULONG query_reg_ascii_value( HKEY hkey, const char *name,
                                    KEY_VALUE_PARTIAL_INFORMATION *info, ULONG size ) DECLSPEC_HIDDEN;

static inline WCHAR *win32u_wcsrchr( const WCHAR *str, WCHAR ch )
{
    WCHAR *ret = NULL;
    do { if (*str == ch) ret = (WCHAR *)(ULONG_PTR)str; } while (*str++);
    return ret;
}

static inline WCHAR win32u_towupper( WCHAR ch )
{
    return RtlUpcaseUnicodeChar( ch );
}

static inline WCHAR *win32u_wcschr( const WCHAR *str, WCHAR ch )
{
    do { if (*str == ch) return (WCHAR *)(ULONG_PTR)str; } while (*str++);
    return NULL;
}

static inline int win32u_wcsicmp( const WCHAR *str1, const WCHAR *str2 )
{
    int ret;
    for (;;)
    {
        if ((ret = win32u_towupper( *str1 ) - win32u_towupper( *str2 )) || !*str1) return ret;
        str1++;
        str2++;
    }
}

static inline int win32u_wcscmp( const WCHAR *str1, const WCHAR *str2 )
{
    while (*str1 && (*str1 == *str2)) { str1++; str2++; }
    return *str1 - *str2;
}

static inline LONG win32u_wcstol( LPCWSTR s, LPWSTR *end, INT base )
{
    BOOL negative = FALSE, empty = TRUE;
    LONG ret = 0;

    if (base < 0 || base == 1 || base > 36) return 0;
    if (end) *end = (WCHAR *)s;
    while (*s == ' ' || *s == '\t') s++;

    if (*s == '-')
    {
        negative = TRUE;
        s++;
    }
    else if (*s == '+') s++;

    if ((base == 0 || base == 16) && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
    {
        base = 16;
        s += 2;
    }
    if (base == 0) base = s[0] != '0' ? 10 : 8;

    while (*s)
    {
        int v;

        if ('0' <= *s && *s <= '9') v = *s - '0';
        else if ('A' <= *s && *s <= 'Z') v = *s - 'A' + 10;
        else if ('a' <= *s && *s <= 'z') v = *s - 'a' + 10;
        else break;
        if (v >= base) break;
        if (negative) v = -v;
        s++;
        empty = FALSE;

        if (!negative && (ret > MAXLONG / base || ret * base > MAXLONG - v))
            ret = MAXLONG;
        else if (negative && (ret < (LONG)MINLONG / base || ret * base < (LONG)(MINLONG - v)))
            ret = MINLONG;
        else
            ret = ret * base + v;
    }

    if (end && !empty) *end = (WCHAR *)s;
    return ret;
}

#define towupper(c)     win32u_towupper(c)
#define wcschr(s,c)     win32u_wcschr(s,c)
#define wcscmp(s1,s2)   win32u_wcscmp(s1,s2)
#define wcsicmp(s1,s2)  win32u_wcsicmp(s1,s2)
#define wcsrchr(s,c)    win32u_wcsrchr(s,c)
#define wcstol(s,e,b)   win32u_wcstol(s,e,b)

static inline void ascii_to_unicode( WCHAR *dst, const char *src, size_t len )
{
    while (len--) *dst++ = (unsigned char)*src++;
}

static inline UINT asciiz_to_unicode( WCHAR *dst, const char *src )
{
    WCHAR *p = dst;
    while ((*p++ = *src++));
    return (p - dst) * sizeof(WCHAR);
}

DWORD win32u_mbtowc( CPTABLEINFO *info, WCHAR *dst, DWORD dstlen, const char *src,
                     DWORD srclen ) DECLSPEC_HIDDEN;
DWORD win32u_wctomb( CPTABLEINFO *info, char *dst, DWORD dstlen, const WCHAR *src,
                     DWORD srclen ) DECLSPEC_HIDDEN;

#endif /* __WINE_WIN32U_PRIVATE */
