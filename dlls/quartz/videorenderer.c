/*
 * Video Renderer (Fullscreen and Windowed using Direct Draw)
 *
 * Copyright 2004 Christian Costa
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

#include "quartz_private.h"

#include "uuids.h"
#include "vfwmsgs.h"
#include "amvideo.h"
#include "windef.h"
#include "winbase.h"
#include "dshow.h"
#include "evcode.h"
#include "strmif.h"
#include "ddraw.h"
#include "dvdmedia.h"

#include <assert.h>
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(quartz);

typedef struct VideoRendererImpl
{
    struct strmbase_renderer renderer;
    BaseControlWindow baseControlWindow;
    BaseControlVideo baseControlVideo;

    IOverlay IOverlay_iface;

    BOOL init;

    RECT SourceRect;
    RECT DestRect;
    RECT WindowPos;
    LONG VideoWidth;
    LONG VideoHeight;
    LONG FullScreenMode;

    DWORD saved_style;

    HANDLE run_event;
    IMediaSample *current_sample;
} VideoRendererImpl;

static inline VideoRendererImpl *impl_from_BaseWindow(BaseWindow *iface)
{
    return CONTAINING_RECORD(iface, VideoRendererImpl, baseControlWindow.baseWindow);
}

static inline VideoRendererImpl *impl_from_strmbase_renderer(struct strmbase_renderer *iface)
{
    return CONTAINING_RECORD(iface, VideoRendererImpl, renderer);
}

static inline VideoRendererImpl *impl_from_IVideoWindow(IVideoWindow *iface)
{
    return CONTAINING_RECORD(iface, VideoRendererImpl, baseControlWindow.IVideoWindow_iface);
}

static inline VideoRendererImpl *impl_from_BaseControlVideo(BaseControlVideo *iface)
{
    return CONTAINING_RECORD(iface, VideoRendererImpl, baseControlVideo);
}

static void VideoRenderer_AutoShowWindow(VideoRendererImpl *This)
{
    if (!This->init && (!This->WindowPos.right || !This->WindowPos.top))
    {
        DWORD style = GetWindowLongW(This->baseControlWindow.baseWindow.hWnd, GWL_STYLE);
        DWORD style_ex = GetWindowLongW(This->baseControlWindow.baseWindow.hWnd, GWL_EXSTYLE);

        if (!This->WindowPos.right)
        {
            if (This->DestRect.right)
            {
                This->WindowPos.left = This->DestRect.left;
                This->WindowPos.right = This->DestRect.right;
            }
            else
            {
                This->WindowPos.left = This->SourceRect.left;
                This->WindowPos.right = This->SourceRect.right;
            }
        }
        if (!This->WindowPos.bottom)
        {
            if (This->DestRect.bottom)
            {
                This->WindowPos.top = This->DestRect.top;
                This->WindowPos.bottom = This->DestRect.bottom;
            }
            else
            {
                This->WindowPos.top = This->SourceRect.top;
                This->WindowPos.bottom = This->SourceRect.bottom;
            }
        }

        AdjustWindowRectEx(&This->WindowPos, style, FALSE, style_ex);

        TRACE("WindowPos: %s\n", wine_dbgstr_rect(&This->WindowPos));
        SetWindowPos(This->baseControlWindow.baseWindow.hWnd, NULL,
            This->WindowPos.left,
            This->WindowPos.top,
            This->WindowPos.right - This->WindowPos.left,
            This->WindowPos.bottom - This->WindowPos.top,
            SWP_NOZORDER|SWP_NOMOVE|SWP_DEFERERASE);

        GetClientRect(This->baseControlWindow.baseWindow.hWnd, &This->DestRect);
    }
    else if (!This->init)
        This->DestRect = This->WindowPos;
    This->init = TRUE;
    if (This->baseControlWindow.AutoShow)
        ShowWindow(This->baseControlWindow.baseWindow.hWnd, SW_SHOW);
}

static HRESULT WINAPI VideoRenderer_ShouldDrawSampleNow(struct strmbase_renderer *filter,
        IMediaSample *pSample, REFERENCE_TIME *start, REFERENCE_TIME *end)
{
    /* Preroll means the sample isn't shown, this is used for key frames and things like that */
    if (IMediaSample_IsPreroll(pSample) == S_OK)
        return E_FAIL;
    return S_FALSE;
}

static HRESULT WINAPI VideoRenderer_DoRenderSample(struct strmbase_renderer *iface, IMediaSample *pSample)
{
    VideoRendererImpl *filter = impl_from_strmbase_renderer(iface);
    const AM_MEDIA_TYPE *mt = &filter->renderer.sink.pin.mt;
    LPBYTE pbSrcStream = NULL;
    BITMAPINFOHEADER *bih;
    HRESULT hr;
    HDC dc;

    TRACE("filter %p, sample %p.\n", filter, pSample);

    hr = IMediaSample_GetPointer(pSample, &pbSrcStream);
    if (FAILED(hr))
    {
        ERR("Cannot get pointer to sample data (%x)\n", hr);
        return hr;
    }

    if (IsEqualGUID(&mt->formattype, &FORMAT_VideoInfo))
        bih = &((VIDEOINFOHEADER *)mt->pbFormat)->bmiHeader;
    else
        bih = &((VIDEOINFOHEADER2 *)mt->pbFormat)->bmiHeader;

    dc = GetDC(filter->baseControlWindow.baseWindow.hWnd);
    StretchDIBits(dc, filter->DestRect.left, filter->DestRect.top,
            filter->DestRect.right - filter->DestRect.left,
            filter->DestRect.bottom - filter->DestRect.top,
            filter->SourceRect.left, filter->SourceRect.top,
            filter->SourceRect.right - filter->SourceRect.left,
            filter->SourceRect.bottom - filter->SourceRect.top,
            pbSrcStream, (BITMAPINFO *)bih, DIB_RGB_COLORS, SRCCOPY);
    ReleaseDC(filter->baseControlWindow.baseWindow.hWnd, dc);

    if (filter->renderer.filter.state == State_Paused)
    {
        const HANDLE events[2] = {filter->run_event, filter->renderer.flush_event};

        filter->current_sample = pSample;

        LeaveCriticalSection(&filter->renderer.csRenderLock);
        WaitForMultipleObjects(2, events, FALSE, INFINITE);
        EnterCriticalSection(&filter->renderer.csRenderLock);

        filter->current_sample = NULL;
    }

    return S_OK;
}

static HRESULT WINAPI VideoRenderer_CheckMediaType(struct strmbase_renderer *iface, const AM_MEDIA_TYPE *pmt)
{
    VideoRendererImpl *This = impl_from_strmbase_renderer(iface);

    if (!IsEqualIID(&pmt->majortype, &MEDIATYPE_Video))
        return S_FALSE;

    if (IsEqualIID(&pmt->subtype, &MEDIASUBTYPE_RGB32) ||
        IsEqualIID(&pmt->subtype, &MEDIASUBTYPE_RGB24) ||
        IsEqualIID(&pmt->subtype, &MEDIASUBTYPE_RGB565) ||
        IsEqualIID(&pmt->subtype, &MEDIASUBTYPE_RGB8))
    {
        LONG height;

        if (IsEqualIID(&pmt->formattype, &FORMAT_VideoInfo))
        {
            VIDEOINFOHEADER *format = (VIDEOINFOHEADER *)pmt->pbFormat;
            This->SourceRect.left = 0;
            This->SourceRect.top = 0;
            This->SourceRect.right = This->VideoWidth = format->bmiHeader.biWidth;
            height = format->bmiHeader.biHeight;
            if (height < 0)
                This->SourceRect.bottom = This->VideoHeight = -height;
            else
                This->SourceRect.bottom = This->VideoHeight = height;
        }
        else if (IsEqualIID(&pmt->formattype, &FORMAT_VideoInfo2))
        {
            VIDEOINFOHEADER2 *format2 = (VIDEOINFOHEADER2 *)pmt->pbFormat;

            This->SourceRect.left = 0;
            This->SourceRect.top = 0;
            This->SourceRect.right = This->VideoWidth = format2->bmiHeader.biWidth;
            height = format2->bmiHeader.biHeight;
            if (height < 0)
                This->SourceRect.bottom = This->VideoHeight = -height;
            else
                This->SourceRect.bottom = This->VideoHeight = height;
        }
        else
        {
            WARN("Format type %s not supported\n", debugstr_guid(&pmt->formattype));
            return S_FALSE;
        }
        return S_OK;
    }
    return S_FALSE;
}

static void video_renderer_destroy(struct strmbase_renderer *iface)
{
    VideoRendererImpl *filter = impl_from_strmbase_renderer(iface);

    BaseControlWindow_Destroy(&filter->baseControlWindow);
    BaseControlVideo_Destroy(&filter->baseControlVideo);
    CloseHandle(filter->run_event);
    strmbase_renderer_cleanup(&filter->renderer);
    CoTaskMemFree(filter);

    InterlockedDecrement(&object_locks);
}

static HRESULT video_renderer_query_interface(struct strmbase_renderer *iface, REFIID iid, void **out)
{
    VideoRendererImpl *filter = impl_from_strmbase_renderer(iface);

    if (IsEqualGUID(iid, &IID_IBasicVideo))
        *out = &filter->baseControlVideo.IBasicVideo_iface;
    else if (IsEqualGUID(iid, &IID_IVideoWindow))
        *out = &filter->baseControlWindow.IVideoWindow_iface;
    else
        return E_NOINTERFACE;

    IUnknown_AddRef((IUnknown *)*out);
    return S_OK;
}

static HRESULT video_renderer_pin_query_interface(struct strmbase_renderer *iface, REFIID iid, void **out)
{
    VideoRendererImpl *filter = impl_from_strmbase_renderer(iface);

    if (IsEqualGUID(iid, &IID_IOverlay))
        *out = &filter->IOverlay_iface;
    else
        return E_NOINTERFACE;

    IUnknown_AddRef((IUnknown *)*out);
    return S_OK;
}

static void video_renderer_start_stream(struct strmbase_renderer *iface)
{
    VideoRendererImpl *filter = impl_from_strmbase_renderer(iface);

    SetEvent(filter->run_event);
}

static void video_renderer_stop_stream(struct strmbase_renderer *iface)
{
    VideoRendererImpl *This = impl_from_strmbase_renderer(iface);

    TRACE("(%p)->()\n", This);

    if (This->baseControlWindow.AutoShow)
        /* Black it out */
        RedrawWindow(This->baseControlWindow.baseWindow.hWnd, NULL, NULL, RDW_INVALIDATE|RDW_ERASE);

    ResetEvent(This->run_event);
}

static void video_renderer_init_stream(struct strmbase_renderer *iface)
{
    VideoRendererImpl *filter = impl_from_strmbase_renderer(iface);

    VideoRenderer_AutoShowWindow(filter);
}

static RECT WINAPI VideoRenderer_GetDefaultRect(BaseWindow *iface)
{
    VideoRendererImpl *This = impl_from_BaseWindow(iface);
    static RECT defRect;

    SetRect(&defRect, 0, 0, This->VideoWidth, This->VideoHeight);

    return defRect;
}

static BOOL WINAPI VideoRenderer_OnSize(BaseWindow *iface, LONG Width, LONG Height)
{
    VideoRendererImpl *This = impl_from_BaseWindow(iface);

    TRACE("WM_SIZE %d %d\n", Width, Height);
    GetClientRect(iface->hWnd, &This->DestRect);
    TRACE("WM_SIZING: DestRect=(%d,%d),(%d,%d)\n",
        This->DestRect.left,
        This->DestRect.top,
        This->DestRect.right - This->DestRect.left,
        This->DestRect.bottom - This->DestRect.top);

    return TRUE;
}

static const struct strmbase_renderer_ops renderer_ops =
{
    .pfnCheckMediaType = VideoRenderer_CheckMediaType,
    .pfnDoRenderSample = VideoRenderer_DoRenderSample,
    .renderer_init_stream = video_renderer_init_stream,
    .renderer_start_stream = video_renderer_start_stream,
    .renderer_stop_stream = video_renderer_stop_stream,
    .pfnShouldDrawSampleNow = VideoRenderer_ShouldDrawSampleNow,
    .renderer_destroy = video_renderer_destroy,
    .renderer_query_interface = video_renderer_query_interface,
    .renderer_pin_query_interface = video_renderer_pin_query_interface,
};

static const BaseWindowFuncTable renderer_BaseWindowFuncTable = {
    VideoRenderer_GetDefaultRect,
    VideoRenderer_OnSize
};

static HRESULT WINAPI VideoRenderer_GetSourceRect(BaseControlVideo* iface, RECT *pSourceRect)
{
    VideoRendererImpl *This = impl_from_BaseControlVideo(iface);
    CopyRect(pSourceRect,&This->SourceRect);
    return S_OK;
}

static HRESULT WINAPI VideoRenderer_GetStaticImage(BaseControlVideo *iface, LONG *size, LONG *image)
{
    VideoRendererImpl *filter = impl_from_BaseControlVideo(iface);
    const AM_MEDIA_TYPE *mt = &filter->renderer.sink.pin.mt;
    const BITMAPINFOHEADER *bih;
    size_t image_size;
    BYTE *sample_data;

    TRACE("filter %p, size %p, image %p.\n", filter, size, image);

    EnterCriticalSection(&filter->renderer.csRenderLock);

    if (IsEqualGUID(&mt->formattype, &FORMAT_VideoInfo))
        bih = &((VIDEOINFOHEADER *)mt->pbFormat)->bmiHeader;
    else /* if (IsEqualGUID(&mt->formattype, &FORMAT_VideoInfo2)) */
        bih = &((VIDEOINFOHEADER2 *)mt->pbFormat)->bmiHeader;
    image_size = bih->biWidth * bih->biHeight * bih->biBitCount / 8;

    if (!image)
    {
        LeaveCriticalSection(&filter->renderer.csRenderLock);
        *size = sizeof(BITMAPINFOHEADER) + image_size;
        return S_OK;
    }

    if (filter->renderer.filter.state != State_Paused)
    {
        LeaveCriticalSection(&filter->renderer.csRenderLock);
        return VFW_E_NOT_PAUSED;
    }

    if (!filter->current_sample)
    {
        LeaveCriticalSection(&filter->renderer.csRenderLock);
        return E_UNEXPECTED;
    }

    if (*size < sizeof(BITMAPINFOHEADER) + image_size)
    {
        LeaveCriticalSection(&filter->renderer.csRenderLock);
        return E_OUTOFMEMORY;
    }

    memcpy(image, bih, sizeof(BITMAPINFOHEADER));
    IMediaSample_GetPointer(filter->current_sample, &sample_data);
    memcpy((char *)image + sizeof(BITMAPINFOHEADER), sample_data, image_size);

    LeaveCriticalSection(&filter->renderer.csRenderLock);
    return S_OK;
}

static HRESULT WINAPI VideoRenderer_GetTargetRect(BaseControlVideo* iface, RECT *pTargetRect)
{
    VideoRendererImpl *This = impl_from_BaseControlVideo(iface);
    CopyRect(pTargetRect,&This->DestRect);
    return S_OK;
}

static VIDEOINFOHEADER* WINAPI VideoRenderer_GetVideoFormat(BaseControlVideo* iface)
{
    VideoRendererImpl *This = impl_from_BaseControlVideo(iface);
    AM_MEDIA_TYPE *pmt;

    TRACE("(%p/%p)\n", This, iface);

    pmt = &This->renderer.sink.pin.mt;
    if (IsEqualIID(&pmt->formattype, &FORMAT_VideoInfo)) {
        return (VIDEOINFOHEADER*)pmt->pbFormat;
    } else if (IsEqualIID(&pmt->formattype, &FORMAT_VideoInfo2)) {
        static VIDEOINFOHEADER vih;
        VIDEOINFOHEADER2 *vih2 = (VIDEOINFOHEADER2*)pmt->pbFormat;
        memcpy(&vih,vih2,sizeof(VIDEOINFOHEADER));
        memcpy(&vih.bmiHeader, &vih2->bmiHeader, sizeof(BITMAPINFOHEADER));
        return &vih;
    } else {
        ERR("Unknown format type %s\n", qzdebugstr_guid(&pmt->formattype));
        return NULL;
    }
}

static HRESULT WINAPI VideoRenderer_IsDefaultSourceRect(BaseControlVideo* iface)
{
    VideoRendererImpl *This = impl_from_BaseControlVideo(iface);
    FIXME("(%p/%p)->(): stub !!!\n", This, iface);

    return S_OK;
}

static HRESULT WINAPI VideoRenderer_IsDefaultTargetRect(BaseControlVideo* iface)
{
    VideoRendererImpl *This = impl_from_BaseControlVideo(iface);
    FIXME("(%p/%p)->(): stub !!!\n", This, iface);

    return S_OK;
}

static HRESULT WINAPI VideoRenderer_SetDefaultSourceRect(BaseControlVideo* iface)
{
    VideoRendererImpl *This = impl_from_BaseControlVideo(iface);

    SetRect(&This->SourceRect, 0, 0, This->VideoWidth, This->VideoHeight);

    return S_OK;
}

static HRESULT WINAPI VideoRenderer_SetDefaultTargetRect(BaseControlVideo* iface)
{
    VideoRendererImpl *This = impl_from_BaseControlVideo(iface);
    RECT rect;

    if (!GetClientRect(This->baseControlWindow.baseWindow.hWnd, &rect))
        return E_FAIL;

    SetRect(&This->DestRect, 0, 0, rect.right, rect.bottom);

    return S_OK;
}

static HRESULT WINAPI VideoRenderer_SetSourceRect(BaseControlVideo* iface, RECT *pSourceRect)
{
    VideoRendererImpl *This = impl_from_BaseControlVideo(iface);
    CopyRect(&This->SourceRect,pSourceRect);
    return S_OK;
}

static HRESULT WINAPI VideoRenderer_SetTargetRect(BaseControlVideo* iface, RECT *pTargetRect)
{
    VideoRendererImpl *This = impl_from_BaseControlVideo(iface);
    CopyRect(&This->DestRect,pTargetRect);
    return S_OK;
}

static const BaseControlVideoFuncTable renderer_BaseControlVideoFuncTable = {
    VideoRenderer_GetSourceRect,
    VideoRenderer_GetStaticImage,
    VideoRenderer_GetTargetRect,
    VideoRenderer_GetVideoFormat,
    VideoRenderer_IsDefaultSourceRect,
    VideoRenderer_IsDefaultTargetRect,
    VideoRenderer_SetDefaultSourceRect,
    VideoRenderer_SetDefaultTargetRect,
    VideoRenderer_SetSourceRect,
    VideoRenderer_SetTargetRect
};

static HRESULT WINAPI VideoWindow_get_FullScreenMode(IVideoWindow *iface,
                                                     LONG *FullScreenMode)
{
    VideoRendererImpl *This = impl_from_IVideoWindow(iface);

    TRACE("(%p/%p)->(%p): %d\n", This, iface, FullScreenMode, This->FullScreenMode);

    if (!FullScreenMode)
        return E_POINTER;

    *FullScreenMode = This->FullScreenMode;

    return S_OK;
}

static HRESULT WINAPI VideoWindow_put_FullScreenMode(IVideoWindow *iface,
                                                     LONG FullScreenMode)
{
    VideoRendererImpl *This = impl_from_IVideoWindow(iface);

    FIXME("(%p/%p)->(%d): stub !!!\n", This, iface, FullScreenMode);

    if (FullScreenMode) {
        This->saved_style = GetWindowLongW(This->baseControlWindow.baseWindow.hWnd, GWL_STYLE);
        ShowWindow(This->baseControlWindow.baseWindow.hWnd, SW_HIDE);
        SetParent(This->baseControlWindow.baseWindow.hWnd, 0);
        SetWindowLongW(This->baseControlWindow.baseWindow.hWnd, GWL_STYLE, WS_POPUP);
        SetWindowPos(This->baseControlWindow.baseWindow.hWnd,HWND_TOP,0,0,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN),SWP_SHOWWINDOW);
        GetWindowRect(This->baseControlWindow.baseWindow.hWnd, &This->DestRect);
        This->WindowPos = This->DestRect;
    } else {
        ShowWindow(This->baseControlWindow.baseWindow.hWnd, SW_HIDE);
        SetParent(This->baseControlWindow.baseWindow.hWnd, This->baseControlWindow.hwndOwner);
        SetWindowLongW(This->baseControlWindow.baseWindow.hWnd, GWL_STYLE, This->saved_style);
        GetClientRect(This->baseControlWindow.baseWindow.hWnd, &This->DestRect);
        SetWindowPos(This->baseControlWindow.baseWindow.hWnd,0,This->DestRect.left,This->DestRect.top,This->DestRect.right,This->DestRect.bottom,SWP_NOZORDER|SWP_SHOWWINDOW);
        This->WindowPos = This->DestRect;
    }
    This->FullScreenMode = FullScreenMode;

    return S_OK;
}

static const IVideoWindowVtbl IVideoWindow_VTable =
{
    BaseControlWindowImpl_QueryInterface,
    BaseControlWindowImpl_AddRef,
    BaseControlWindowImpl_Release,
    BaseControlWindowImpl_GetTypeInfoCount,
    BaseControlWindowImpl_GetTypeInfo,
    BaseControlWindowImpl_GetIDsOfNames,
    BaseControlWindowImpl_Invoke,
    BaseControlWindowImpl_put_Caption,
    BaseControlWindowImpl_get_Caption,
    BaseControlWindowImpl_put_WindowStyle,
    BaseControlWindowImpl_get_WindowStyle,
    BaseControlWindowImpl_put_WindowStyleEx,
    BaseControlWindowImpl_get_WindowStyleEx,
    BaseControlWindowImpl_put_AutoShow,
    BaseControlWindowImpl_get_AutoShow,
    BaseControlWindowImpl_put_WindowState,
    BaseControlWindowImpl_get_WindowState,
    BaseControlWindowImpl_put_BackgroundPalette,
    BaseControlWindowImpl_get_BackgroundPalette,
    BaseControlWindowImpl_put_Visible,
    BaseControlWindowImpl_get_Visible,
    BaseControlWindowImpl_put_Left,
    BaseControlWindowImpl_get_Left,
    BaseControlWindowImpl_put_Width,
    BaseControlWindowImpl_get_Width,
    BaseControlWindowImpl_put_Top,
    BaseControlWindowImpl_get_Top,
    BaseControlWindowImpl_put_Height,
    BaseControlWindowImpl_get_Height,
    BaseControlWindowImpl_put_Owner,
    BaseControlWindowImpl_get_Owner,
    BaseControlWindowImpl_put_MessageDrain,
    BaseControlWindowImpl_get_MessageDrain,
    BaseControlWindowImpl_get_BorderColor,
    BaseControlWindowImpl_put_BorderColor,
    VideoWindow_get_FullScreenMode,
    VideoWindow_put_FullScreenMode,
    BaseControlWindowImpl_SetWindowForeground,
    BaseControlWindowImpl_NotifyOwnerMessage,
    BaseControlWindowImpl_SetWindowPosition,
    BaseControlWindowImpl_GetWindowPosition,
    BaseControlWindowImpl_GetMinIdealImageSize,
    BaseControlWindowImpl_GetMaxIdealImageSize,
    BaseControlWindowImpl_GetRestorePosition,
    BaseControlWindowImpl_HideCursor,
    BaseControlWindowImpl_IsCursorHidden
};

static inline VideoRendererImpl *impl_from_IOverlay(IOverlay *iface)
{
    return CONTAINING_RECORD(iface, VideoRendererImpl, IOverlay_iface);
}

static HRESULT WINAPI overlay_QueryInterface(IOverlay *iface, REFIID iid, void **out)
{
    VideoRendererImpl *filter = impl_from_IOverlay(iface);
    return IPin_QueryInterface(&filter->renderer.sink.pin.IPin_iface, iid, out);
}

static ULONG WINAPI overlay_AddRef(IOverlay *iface)
{
    VideoRendererImpl *filter = impl_from_IOverlay(iface);
    return IPin_AddRef(&filter->renderer.sink.pin.IPin_iface);
}

static ULONG WINAPI overlay_Release(IOverlay *iface)
{
    VideoRendererImpl *filter = impl_from_IOverlay(iface);
    return IPin_Release(&filter->renderer.sink.pin.IPin_iface);
}

static HRESULT WINAPI overlay_GetPalette(IOverlay *iface, DWORD *count, PALETTEENTRY **palette)
{
    FIXME("iface %p, count %p, palette %p, stub!\n", iface, count, palette);
    return E_NOTIMPL;
}

static HRESULT WINAPI overlay_SetPalette(IOverlay *iface, DWORD count, PALETTEENTRY *palette)
{
    FIXME("iface %p, count %u, palette %p, stub!\n", iface, count, palette);
    return E_NOTIMPL;
}

static HRESULT WINAPI overlay_GetDefaultColorKey(IOverlay *iface, COLORKEY *key)
{
    FIXME("iface %p, key %p, stub!\n", iface, key);
    return E_NOTIMPL;
}

static HRESULT WINAPI overlay_GetColorKey(IOverlay *iface, COLORKEY *key)
{
    FIXME("iface %p, key %p, stub!\n", iface, key);
    return E_NOTIMPL;
}

static HRESULT WINAPI overlay_SetColorKey(IOverlay *iface, COLORKEY *key)
{
    FIXME("iface %p, key %p, stub!\n", iface, key);
    return E_NOTIMPL;
}

static HRESULT WINAPI overlay_GetWindowHandle(IOverlay *iface, HWND *window)
{
    VideoRendererImpl *filter = impl_from_IOverlay(iface);

    TRACE("filter %p, window %p.\n", filter, window);

    *window = filter->baseControlWindow.baseWindow.hWnd;
    return S_OK;
}

static HRESULT WINAPI overlay_GetClipList(IOverlay *iface, RECT *source, RECT *dest, RGNDATA **region)
{
    FIXME("iface %p, source %p, dest %p, region %p, stub!\n", iface, source, dest, region);
    return E_NOTIMPL;
}

static HRESULT WINAPI overlay_GetVideoPosition(IOverlay *iface, RECT *source, RECT *dest)
{
    FIXME("iface %p, source %p, dest %p, stub!\n", iface, source, dest);
    return E_NOTIMPL;
}

static HRESULT WINAPI overlay_Advise(IOverlay *iface, IOverlayNotify *sink, DWORD flags)
{
    FIXME("iface %p, sink %p, flags %#x, stub!\n", iface, sink, flags);
    return E_NOTIMPL;
}

static HRESULT WINAPI overlay_Unadvise(IOverlay *iface)
{
    FIXME("iface %p, stub!\n", iface);
    return E_NOTIMPL;
}

static const IOverlayVtbl overlay_vtbl =
{
    overlay_QueryInterface,
    overlay_AddRef,
    overlay_Release,
    overlay_GetPalette,
    overlay_SetPalette,
    overlay_GetDefaultColorKey,
    overlay_GetColorKey,
    overlay_SetColorKey,
    overlay_GetWindowHandle,
    overlay_GetClipList,
    overlay_GetVideoPosition,
    overlay_Advise,
    overlay_Unadvise,
};

HRESULT video_renderer_create(IUnknown *outer, IUnknown **out)
{
    HRESULT hr;
    VideoRendererImpl * pVideoRenderer;

    *out = NULL;

    pVideoRenderer = CoTaskMemAlloc(sizeof(VideoRendererImpl));

    pVideoRenderer->init = FALSE;
    ZeroMemory(&pVideoRenderer->SourceRect, sizeof(RECT));
    ZeroMemory(&pVideoRenderer->DestRect, sizeof(RECT));
    ZeroMemory(&pVideoRenderer->WindowPos, sizeof(RECT));
    pVideoRenderer->FullScreenMode = OAFALSE;

    pVideoRenderer->IOverlay_iface.lpVtbl = &overlay_vtbl;

    hr = strmbase_renderer_init(&pVideoRenderer->renderer, outer,
            &CLSID_VideoRenderer, L"In", &renderer_ops);

    if (FAILED(hr))
        goto fail;

    hr = video_window_init(&pVideoRenderer->baseControlWindow, &IVideoWindow_VTable,
            &pVideoRenderer->renderer.filter, &pVideoRenderer->renderer.sink.pin,
            &renderer_BaseWindowFuncTable);
    if (FAILED(hr))
        goto fail;

    hr = basic_video_init(&pVideoRenderer->baseControlVideo, &pVideoRenderer->renderer.filter,
            &pVideoRenderer->renderer.sink.pin, &renderer_BaseControlVideoFuncTable);
    if (FAILED(hr))
        goto fail;

    if (FAILED(hr = BaseWindowImpl_PrepareWindow(&pVideoRenderer->baseControlWindow.baseWindow)))
        goto fail;

    pVideoRenderer->run_event = CreateEventW(NULL, TRUE, FALSE, NULL);

    *out = &pVideoRenderer->renderer.filter.IUnknown_inner;
    return S_OK;

fail:
    strmbase_renderer_cleanup(&pVideoRenderer->renderer);
    CoTaskMemFree(pVideoRenderer);
    return hr;
}

HRESULT video_renderer_default_create(IUnknown *outer, IUnknown **out)
{
    /* TODO: Attempt to use the VMR-7 renderer instead when possible */
    return video_renderer_create(outer, out);
}
