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

#include <stdarg.h>
#include <stdlib.h>
#include "windef.h"
#include "winbase.h"
#include "ntgdi.h"
#include "ntuser.h"
#include "wine/gdi_driver.h"
#include "wine/unixlib.h"
#include "wine/debug.h"

extern const struct user_callbacks *user_callbacks DECLSPEC_HIDDEN;

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
    HKL      (WINAPI *pNtUserActivateKeyboardLayout)( HKL layout, UINT flags );
    HDC      (WINAPI *pNtUserBeginPaint)( HWND hwnd, PAINTSTRUCT *ps );
    ULONG_PTR (WINAPI *pNtUserCallHwnd)( HWND hwnd, DWORD code );
    ULONG_PTR (WINAPI *pNtUserCallHwndParam)( HWND hwnd, DWORD_PTR param, DWORD code );
    LRESULT  (WINAPI *pNtUserCallNextHookEx)( HHOOK hhook, INT code, WPARAM wparam, LPARAM lparam );
    ULONG_PTR (WINAPI *pNtUserCallNoParam)( ULONG code );
    ULONG_PTR (WINAPI *pNtUserCallOneParam)( ULONG_PTR arg, ULONG code );
    ULONG_PTR (WINAPI *pNtUserCallTwoParam)( ULONG_PTR arg1, ULONG_PTR arg2, ULONG code );
    LONG     (WINAPI *pNtUserChangeDisplaySettings)( UNICODE_STRING *devname, DEVMODEW *devmode, HWND hwnd,
                                                     DWORD flags, void *lparam );
    BOOL     (WINAPI *pNtUserClipCursor)( const RECT *rect );
    INT      (WINAPI *pNtUserCountClipboardFormats)(void);
    HDWP     (WINAPI *pNtUserDeferWindowPosAndBand)( HDWP hdwp, HWND hwnd, HWND after,
                                                     INT x, INT y, INT cx, INT cy,
                                                     UINT flags, UINT unk1, UINT unk2 );
    BOOL     (WINAPI *pNtUserDestroyCursor)( HCURSOR cursor, ULONG arg );
    BOOL     (WINAPI *pNtUserDestroyWindow)( HWND hwnd );
    BOOL     (WINAPI *pNtUserDrawIconEx)( HDC hdc, INT x0, INT y0, HICON icon, INT width,
                                          INT height, UINT istep, HBRUSH hbr, UINT flags );
    BOOL     (WINAPI *pNtUserEndDeferWindowPosEx)( HDWP hdwp, BOOL async );
    BOOL     (WINAPI *pNtUserEndPaint)( HWND hwnd, const PAINTSTRUCT *ps );
    NTSTATUS (WINAPI *pNtUserEnumDisplayDevices)( UNICODE_STRING *device, DWORD index,
                                                  DISPLAY_DEVICEW *info, DWORD flags );
    BOOL     (WINAPI *pNtUserEnumDisplayMonitors)( HDC hdc, RECT *rect, MONITORENUMPROC proc, LPARAM lp );
    BOOL     (WINAPI *pNtUserEnumDisplaySettings)( UNICODE_STRING *device, DWORD mode,
                                                   DEVMODEW *dev_mode, DWORD flags );
    BOOL     (WINAPI *pNtUserFlashWindowEx)( FLASHWINFO *info );
    SHORT    (WINAPI *pNtUserGetAsyncKeyState)( INT key );
    ATOM     (WINAPI *pNtUserGetClassInfoEx)( HINSTANCE instance, UNICODE_STRING *name, WNDCLASSEXW *wc,
                                              struct client_menu_name *menu_name, BOOL ansi );
    BOOL     (WINAPI *pNtUserGetCursorInfo)( CURSORINFO *info );
    HDC      (WINAPI *pNtUserGetDCEx)( HWND hwnd, HRGN clip_rgn, DWORD flags );
    LONG     (WINAPI *pNtUserGetDisplayConfigBufferSizes)( UINT32 flags, UINT32 *num_path_info,
                                                           UINT32 *num_mode_info );
    BOOL     (WINAPI *pNtUserGetIconInfo)( HICON icon, ICONINFO *info, UNICODE_STRING *module,
                                           UNICODE_STRING *res_name, DWORD *bpp, LONG unk );
    INT      (WINAPI *pNtUserGetKeyNameText)( LONG lparam, WCHAR *buffer, INT size );
    UINT     (WINAPI *pNtUserGetKeyboardLayoutList)( INT size, HKL *layouts );
    INT      (WINAPI *pNtUserGetPriorityClipboardFormat)( UINT *list, INT count );
    DWORD    (WINAPI *pNtUserGetQueueStatus)( UINT flags );
    BOOL     (WINAPI *pNtUserGetUpdateRect)( HWND hwnd, RECT *rect, BOOL erase );
    INT      (WINAPI *pNtUserGetUpdateRgn)( HWND hwnd, HRGN hrgn, BOOL erase );
    BOOL     (WINAPI *pNtUserGetUpdatedClipboardFormats)( UINT *formats, UINT size, UINT *out_size );
    BOOL     (WINAPI *pNtUserIsClipboardFormatAvailable)( UINT format );
    UINT     (WINAPI *pNtUserMapVirtualKeyEx)( UINT code, UINT type, HKL layout );
    BOOL     (WINAPI *pNtUserMoveWindow)( HWND hwnd, INT x, INT y, INT cx, INT cy, BOOL repaint );
    BOOL     (WINAPI *pNtUserRedrawWindow)( HWND hwnd, const RECT *rect, HRGN hrgn, UINT flags );
    ATOM     (WINAPI *pNtUserRegisterClassExWOW)( const WNDCLASSEXW *wc, UNICODE_STRING *name,
                                                  UNICODE_STRING *version,
                                                  struct client_menu_name *client_menu_name,
                                                  DWORD fnid, DWORD flags, DWORD *wow );
    BOOL     (WINAPI *pNtUserRegisterHotKey)( HWND hwnd, INT id, UINT modifiers, UINT vk );
    INT      (WINAPI *pNtUserReleaseDC)( HWND hwnd, HDC hdc );
    BOOL     (WINAPI *pNtUserScrollDC)( HDC hdc, INT dx, INT dy, const RECT *scroll, const RECT *clip,
                                        HRGN ret_update_rgn, RECT *update_rect );
    HPALETTE (WINAPI *pNtUserSelectPalette)( HDC hdc, HPALETTE hpal, WORD bkg );
    HWND     (WINAPI *pNtUserSetActiveWindow)( HWND hwnd );
    HWND     (WINAPI *pNtUserSetCapture)( HWND hwnd );
    DWORD    (WINAPI *pNtUserSetClassLong)( HWND hwnd, INT offset, LONG newval, BOOL ansi );
    ULONG_PTR (WINAPI *pNtUserSetClassLongPtr)( HWND hwnd, INT offset, LONG_PTR newval, BOOL ansi );
    WORD     (WINAPI *pNtUserSetClassWord)( HWND hwnd, INT offset, WORD newval );
    HCURSOR  (WINAPI *pNtUserSetCursor)( HCURSOR cursor );
    BOOL     (WINAPI *pNtUserSetCursorIconData)( HCURSOR cursor, UNICODE_STRING *module,
                                                 UNICODE_STRING *res_name, struct cursoricon_desc *desc );
    BOOL     (WINAPI *pNtUserSetCursorPos)( INT x, INT y );
    HWND     (WINAPI *pNtUserSetFocus)( HWND hwnd );
    BOOL     (WINAPI *pNtUserSetLayeredWindowAttributes)( HWND hwnd, COLORREF key, BYTE alpha, DWORD flags );
    HWND     (WINAPI *pNtUserSetParent)( HWND hwnd, HWND parent );
    BOOL     (WINAPI *pNtUserSetSysColors)( INT count, const INT *colors, const COLORREF *values );
    LONG     (WINAPI *pNtUserSetWindowLong)( HWND hwnd, INT offset, LONG newval, BOOL ansi );
    LONG_PTR (WINAPI *pNtUserSetWindowLongPtr)( HWND hwnd, INT offset, LONG_PTR newval, BOOL ansi );
    BOOL     (WINAPI *pNtUserSetWindowPos)( HWND hwnd, HWND after, INT x, INT y, INT cx, INT cy, UINT flags );
    int      (WINAPI *pNtUserSetWindowRgn)( HWND hwnd, HRGN hrgn, BOOL redraw );
    WORD     (WINAPI *pNtUserSetWindowWord)( HWND hwnd, INT offset, WORD newval );
    INT      (WINAPI *pNtUserShowCursor)( BOOL show );
    BOOL     (WINAPI *pNtUserShowWindow)( HWND hwnd, INT cmd );
    BOOL     (WINAPI *pNtUserShowWindowAsync)( HWND hwnd, INT cmd );
    BOOL     (WINAPI *pNtUserSystemParametersInfo)( UINT action, UINT val, PVOID ptr, UINT winini );
    BOOL     (WINAPI *pNtUserSystemParametersInfoForDpi)( UINT action, UINT val, PVOID ptr,
                                                         UINT winini, UINT dpi );
    INT      (WINAPI *pNtUserToUnicodeEx)( UINT virt, UINT scan, const BYTE *state,
                                           WCHAR *str, int size, UINT flags, HKL layout );
    BOOL     (WINAPI *pNtUserUnregisterClass)( UNICODE_STRING *name, HINSTANCE instance,
                                               struct client_menu_name *client_menu_name );
    BOOL     (WINAPI *pNtUserUnregisterHotKey)( HWND hwnd, INT id );
    BOOL     (WINAPI *pNtUserUpdateLayeredWindow)( HWND hwnd, HDC hdc_dst, const POINT *pts_dst,
                                                   const SIZE *size, HDC hdc_src, const POINT *pts_src,
                                                   COLORREF key, const BLENDFUNCTION *blend,
                                                   DWORD flags, const RECT *dirty );
    WORD     (WINAPI *pNtUserVkKeyScanEx)( WCHAR chr, HKL layout );
    HWND     (WINAPI *pNtUserWindowFromPoint)( LONG x, LONG y );

    /* Wine-specific functions */
    INT (WINAPI *pSetDIBits)( HDC hdc, HBITMAP hbitmap, UINT startscan,
                              UINT lines, const void *bits, const BITMAPINFO *info,
                              UINT coloruse );
    BOOL (CDECL *get_brush_bitmap_info)( HBRUSH handle, BITMAPINFO *info, void *bits, UINT *usage );
    BOOL (CDECL *get_file_outline_text_metric)( const WCHAR *path, OUTLINETEXTMETRICW *otm );
    BOOL (CDECL *get_icm_profile)( HDC hdc, BOOL allow_default, DWORD *size, WCHAR *filename );
    const struct vulkan_funcs * (CDECL *get_vulkan_driver)( UINT version );
    struct opengl_funcs * (CDECL *get_wgl_driver)( HDC hdc, UINT version );
    void (CDECL *set_display_driver)( struct user_driver_funcs *funcs, UINT version );
    void (CDECL *set_visible_region)( HDC hdc, HRGN hrgn, const RECT *vis_rect, const RECT *device_rect,
                                      struct window_surface *surface );
};

/* clipboard.c */
extern void release_clipboard_owner( HWND hwnd ) DECLSPEC_HIDDEN;

/* cursoricon.c */
extern HICON alloc_cursoricon_handle( BOOL is_icon ) DECLSPEC_HIDDEN;
extern BOOL get_clip_cursor( RECT *rect ) DECLSPEC_HIDDEN;
extern ULONG_PTR get_icon_param( HICON handle ) DECLSPEC_HIDDEN;
extern ULONG_PTR set_icon_param( HICON handle, ULONG_PTR param ) DECLSPEC_HIDDEN;

/* dce.c */
extern struct window_surface dummy_surface DECLSPEC_HIDDEN;
extern void flush_window_surfaces( BOOL idle ) DECLSPEC_HIDDEN;
extern void register_window_surface( struct window_surface *old,
                                     struct window_surface *new ) DECLSPEC_HIDDEN;

/* hook.c */
extern LRESULT call_current_hook( HHOOK hhook, INT code, WPARAM wparam, LPARAM lparam ) DECLSPEC_HIDDEN;
extern LRESULT call_hooks( INT id, INT code, WPARAM wparam, LPARAM lparam, BOOL unicode ) DECLSPEC_HIDDEN;
extern BOOL unhook_windows_hook( INT id, HOOKPROC proc ) DECLSPEC_HIDDEN;

/* input.c */
extern LONG global_key_state_counter DECLSPEC_HIDDEN;
extern HWND get_active_window(void) DECLSPEC_HIDDEN;
extern BOOL get_cursor_pos( POINT *pt ) DECLSPEC_HIDDEN;
extern HWND get_focus(void) DECLSPEC_HIDDEN;
extern DWORD get_input_state(void) DECLSPEC_HIDDEN;
extern BOOL WINAPI release_capture(void) DECLSPEC_HIDDEN;
extern BOOL set_capture_window( HWND hwnd, UINT gui_flags, HWND *prev_ret ) DECLSPEC_HIDDEN;
extern BOOL set_foreground_window( HWND hwnd, BOOL mouse ) DECLSPEC_HIDDEN;

/* menu.c */
extern HMENU get_menu( HWND hwnd ) DECLSPEC_HIDDEN;

/* message.c */
extern BOOL kill_system_timer( HWND hwnd, UINT_PTR id ) DECLSPEC_HIDDEN;
extern LRESULT post_message( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam ) DECLSPEC_HIDDEN;
extern LRESULT send_message( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam ) DECLSPEC_HIDDEN;

/* sysparams.c */
extern RECT get_display_rect( const WCHAR *display ) DECLSPEC_HIDDEN;
extern UINT get_monitor_dpi( HMONITOR monitor ) DECLSPEC_HIDDEN;
extern BOOL get_monitor_info( HMONITOR handle, MONITORINFO *info ) DECLSPEC_HIDDEN;
extern UINT get_win_monitor_dpi( HWND hwnd ) DECLSPEC_HIDDEN;
extern RECT get_primary_monitor_rect(void) DECLSPEC_HIDDEN;
extern UINT get_system_dpi(void) DECLSPEC_HIDDEN;
extern int get_system_metrics( int index ) DECLSPEC_HIDDEN;
extern UINT get_thread_dpi(void) DECLSPEC_HIDDEN;
extern RECT get_virtual_screen_rect( UINT dpi ) DECLSPEC_HIDDEN;
extern BOOL is_exiting_thread( DWORD tid ) DECLSPEC_HIDDEN;
extern POINT map_dpi_point( POINT pt, UINT dpi_from, UINT dpi_to ) DECLSPEC_HIDDEN;
extern RECT map_dpi_rect( RECT rect, UINT dpi_from, UINT dpi_to ) DECLSPEC_HIDDEN;
extern HMONITOR monitor_from_point( POINT pt, DWORD flags, UINT dpi ) DECLSPEC_HIDDEN;
extern HMONITOR monitor_from_rect( const RECT *rect, DWORD flags, UINT dpi ) DECLSPEC_HIDDEN;
extern HMONITOR monitor_from_window( HWND hwnd, DWORD flags, UINT dpi ) DECLSPEC_HIDDEN;
extern DPI_AWARENESS_CONTEXT set_thread_dpi_awareness_context( DPI_AWARENESS_CONTEXT context ) DECLSPEC_HIDDEN;
extern void user_lock(void) DECLSPEC_HIDDEN;
extern void user_unlock(void) DECLSPEC_HIDDEN;
extern void user_check_not_lock(void) DECLSPEC_HIDDEN;

/* windc.c */
extern void erase_now( HWND hwnd, UINT rdw_flags ) DECLSPEC_HIDDEN;

/* window.c */
struct tagWND;
extern HDWP begin_defer_window_pos( INT count ) DECLSPEC_HIDDEN;
extern void destroy_thread_windows(void) DECLSPEC_HIDDEN;
extern LRESULT destroy_window( HWND hwnd ) DECLSPEC_HIDDEN;
extern BOOL get_client_rect( HWND hwnd, RECT *rect ) DECLSPEC_HIDDEN;
extern HWND get_desktop_window(void) DECLSPEC_HIDDEN;
extern UINT get_dpi_for_window( HWND hwnd ) DECLSPEC_HIDDEN;
extern HWND get_full_window_handle( HWND hwnd ) DECLSPEC_HIDDEN;
extern HWND get_hwnd_message_parent(void) DECLSPEC_HIDDEN;
extern DPI_AWARENESS_CONTEXT get_window_dpi_awareness_context( HWND hwnd ) DECLSPEC_HIDDEN;
extern BOOL get_window_placement( HWND hwnd, WINDOWPLACEMENT *placement ) DECLSPEC_HIDDEN;
extern DWORD get_window_thread( HWND hwnd, DWORD *process ) DECLSPEC_HIDDEN;
extern HWND is_current_process_window( HWND hwnd ) DECLSPEC_HIDDEN;
extern HWND is_current_thread_window( HWND hwnd ) DECLSPEC_HIDDEN;
extern BOOL is_desktop_window( HWND hwnd ) DECLSPEC_HIDDEN;
extern BOOL is_iconic( HWND hwnd ) DECLSPEC_HIDDEN;
extern DWORD get_window_long( HWND hwnd, INT offset ) DECLSPEC_HIDDEN;
extern BOOL get_window_rect( HWND hwnd, RECT *rect, UINT dpi ) DECLSPEC_HIDDEN;
enum coords_relative;
extern BOOL get_window_rects( HWND hwnd, enum coords_relative relative, RECT *window_rect,
                              RECT *client_rect, UINT dpi ) DECLSPEC_HIDDEN;
extern HWND *list_window_children( HDESK desktop, HWND hwnd, UNICODE_STRING *class,
                                   DWORD tid ) DECLSPEC_HIDDEN;
extern int map_window_points( HWND hwnd_from, HWND hwnd_to, POINT *points, UINT count,
                              UINT dpi ) DECLSPEC_HIDDEN;
extern void map_window_region( HWND from, HWND to, HRGN hrgn ) DECLSPEC_HIDDEN;
extern LONG_PTR set_window_long( HWND hwnd, INT offset, UINT size, LONG_PTR newval,
                                 BOOL ansi ) DECLSPEC_HIDDEN;
extern BOOL set_window_pos( WINDOWPOS *winpos, int parent_x, int parent_y ) DECLSPEC_HIDDEN;
extern ULONG set_window_style( HWND hwnd, ULONG set_bits, ULONG clear_bits ) DECLSPEC_HIDDEN;
extern void update_window_state( HWND hwnd ) DECLSPEC_HIDDEN;

/* to release pointers retrieved by win_get_ptr */
static inline void release_win_ptr( struct tagWND *ptr )
{
    user_unlock();
}

extern void wrappers_init( unixlib_handle_t handle ) DECLSPEC_HIDDEN;
extern NTSTATUS gdi_init(void) DECLSPEC_HIDDEN;
extern NTSTATUS callbacks_init( void *args ) DECLSPEC_HIDDEN;
extern void winstation_init(void) DECLSPEC_HIDDEN;
extern void sysparams_init(void) DECLSPEC_HIDDEN;

extern HKEY reg_create_key( HKEY root, const WCHAR *name, ULONG name_len,
                            DWORD options, DWORD *disposition ) DECLSPEC_HIDDEN;
extern HKEY reg_open_hkcu_key( const char *name ) DECLSPEC_HIDDEN;
extern HKEY reg_open_key( HKEY root, const WCHAR *name, ULONG name_len ) DECLSPEC_HIDDEN;
extern ULONG query_reg_value( HKEY hkey, const WCHAR *name,
                              KEY_VALUE_PARTIAL_INFORMATION *info, ULONG size ) DECLSPEC_HIDDEN;
extern ULONG query_reg_ascii_value( HKEY hkey, const char *name,
                                    KEY_VALUE_PARTIAL_INFORMATION *info, ULONG size ) DECLSPEC_HIDDEN;
extern void set_reg_ascii_value( HKEY hkey, const char *name, const char *value ) DECLSPEC_HIDDEN;
extern BOOL set_reg_value( HKEY hkey, const WCHAR *name, UINT type, const void *value,
                           DWORD count ) DECLSPEC_HIDDEN;
extern BOOL reg_delete_tree( HKEY parent, const WCHAR *name, ULONG name_len ) DECLSPEC_HIDDEN;
extern void reg_delete_value( HKEY hkey, const WCHAR *name ) DECLSPEC_HIDDEN;

extern HKEY hkcu_key DECLSPEC_HIDDEN;

static inline struct user_thread_info *get_user_thread_info(void)
{
    return (struct user_thread_info *)NtCurrentTeb()->Win32ClientInfo;
}

extern const struct user_driver_funcs *user_driver DECLSPEC_HIDDEN;

static inline NTSTATUS user32_call( ULONG id, void *args, ULONG len )
{
    /* FIXME: use KeUserModeCallback instead */
    NTSTATUS (WINAPI *func)(void *, ULONG) = ((void **)NtCurrentTeb()->Peb->KernelCallbackTable)[id];
    return func( args, len );
}

static inline BOOL set_ntstatus( NTSTATUS status )
{
    if (status) SetLastError( RtlNtStatusToDosError( status ));
    return !status;
}

static inline WCHAR win32u_towupper( WCHAR ch )
{
    return RtlUpcaseUnicodeChar( ch );
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

static inline ULONG win32u_wcstoul( const WCHAR *s, WCHAR **end, int base )
{
    BOOL negative = FALSE, empty = TRUE;
    ULONG ret = 0;

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
        s++;
        empty = FALSE;

        if (ret > MAXDWORD / base || ret * base > MAXDWORD - v)
            ret = MAXDWORD;
        else
            ret = ret * base + v;
    }

    if (end && !empty) *end = (WCHAR *)s;
    return negative ? -ret : ret;
}

#define towupper(c)     win32u_towupper(c)
#define wcstol(s,e,b)   win32u_wcstol(s,e,b)
#define wcstoul(s,e,b)  win32u_wcstoul(s,e,b)

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

static inline BOOL is_win9x(void)
{
    return NtCurrentTeb()->Peb->OSPlatformId == VER_PLATFORM_WIN32s;
}

static inline void init_unicode_string( UNICODE_STRING *str, const WCHAR *data )
{
    str->Length = lstrlenW(data) * sizeof(WCHAR);
    str->MaximumLength = str->Length + sizeof(WCHAR);
    str->Buffer = (WCHAR *)data;
}

static inline const char *debugstr_us( const UNICODE_STRING *us )
{
    if (!us) return "<null>";
    return debugstr_wn( us->Buffer, us->Length / sizeof(WCHAR) );
}

#endif /* __WINE_WIN32U_PRIVATE */
