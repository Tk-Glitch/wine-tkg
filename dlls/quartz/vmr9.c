/*
 * Video Mixing Renderer for dx9
 *
 * Copyright 2004 Christian Costa
 * Copyright 2008 Maarten Lankhorst
 * Copyright 2012 Aric Stewart
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
#include "d3d9.h"
#include "vmr9.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(quartz);

struct quartz_vmr
{
    struct strmbase_renderer renderer;
    BaseControlWindow baseControlWindow;
    BaseControlVideo baseControlVideo;

    IAMCertifiedOutputProtection IAMCertifiedOutputProtection_iface;
    IAMFilterMiscFlags IAMFilterMiscFlags_iface;
    IVMRFilterConfig IVMRFilterConfig_iface;
    IVMRFilterConfig9 IVMRFilterConfig9_iface;
    IVMRMonitorConfig IVMRMonitorConfig_iface;
    IVMRMonitorConfig9 IVMRMonitorConfig9_iface;
    IVMRSurfaceAllocatorNotify IVMRSurfaceAllocatorNotify_iface;
    IVMRSurfaceAllocatorNotify9 IVMRSurfaceAllocatorNotify9_iface;
    IVMRWindowlessControl IVMRWindowlessControl_iface;
    IVMRWindowlessControl9 IVMRWindowlessControl9_iface;

    IOverlay IOverlay_iface;

    IVMRSurfaceAllocatorEx9 *allocator;
    IVMRImagePresenter9 *presenter;
    BOOL allocator_is_ex;

    /*
     * The Video Mixing Renderer supports 3 modes, renderless, windowless and windowed
     * What I do is implement windowless as a special case of renderless, and then
     * windowed also as a special case of windowless. This is probably the easiest way.
     */
    VMR9Mode mode;
    BITMAPINFOHEADER bmiheader;

    HMODULE hD3d9;

    /* Presentation related members */
    IDirect3DDevice9 *allocator_d3d9_dev;
    HMONITOR allocator_mon;
    IDirect3DSurface9 **surfaces;
    DWORD num_surfaces;
    DWORD cur_surface;
    DWORD_PTR cookie;

    /* for Windowless Mode */
    HWND hWndClippingWindow;

    RECT source_rect;
    RECT target_rect;
    LONG VideoWidth;
    LONG VideoHeight;

    HANDLE run_event;
};

static inline struct quartz_vmr *impl_from_BaseWindow(BaseWindow *wnd)
{
    return CONTAINING_RECORD(wnd, struct quartz_vmr, baseControlWindow.baseWindow);
}

static inline struct quartz_vmr *impl_from_BaseControlVideo(BaseControlVideo *cvid)
{
    return CONTAINING_RECORD(cvid, struct quartz_vmr, baseControlVideo);
}

static inline struct quartz_vmr *impl_from_IAMCertifiedOutputProtection(IAMCertifiedOutputProtection *iface)
{
    return CONTAINING_RECORD(iface, struct quartz_vmr, IAMCertifiedOutputProtection_iface);
}

static inline struct quartz_vmr *impl_from_IAMFilterMiscFlags(IAMFilterMiscFlags *iface)
{
    return CONTAINING_RECORD(iface, struct quartz_vmr, IAMFilterMiscFlags_iface);
}

static inline struct quartz_vmr *impl_from_IVMRFilterConfig(IVMRFilterConfig *iface)
{
    return CONTAINING_RECORD(iface, struct quartz_vmr, IVMRFilterConfig_iface);
}

static inline struct quartz_vmr *impl_from_IVMRFilterConfig9(IVMRFilterConfig9 *iface)
{
    return CONTAINING_RECORD(iface, struct quartz_vmr, IVMRFilterConfig9_iface);
}

static inline struct quartz_vmr *impl_from_IVMRMonitorConfig(IVMRMonitorConfig *iface)
{
    return CONTAINING_RECORD(iface, struct quartz_vmr, IVMRMonitorConfig_iface);
}

static inline struct quartz_vmr *impl_from_IVMRMonitorConfig9(IVMRMonitorConfig9 *iface)
{
    return CONTAINING_RECORD(iface, struct quartz_vmr, IVMRMonitorConfig9_iface);
}

static inline struct quartz_vmr *impl_from_IVMRSurfaceAllocatorNotify(IVMRSurfaceAllocatorNotify *iface)
{
    return CONTAINING_RECORD(iface, struct quartz_vmr, IVMRSurfaceAllocatorNotify_iface);
}

static inline struct quartz_vmr *impl_from_IVMRSurfaceAllocatorNotify9(IVMRSurfaceAllocatorNotify9 *iface)
{
    return CONTAINING_RECORD(iface, struct quartz_vmr, IVMRSurfaceAllocatorNotify9_iface);
}

static inline struct quartz_vmr *impl_from_IVMRWindowlessControl(IVMRWindowlessControl *iface)
{
    return CONTAINING_RECORD(iface, struct quartz_vmr, IVMRWindowlessControl_iface);
}

static inline struct quartz_vmr *impl_from_IVMRWindowlessControl9(IVMRWindowlessControl9 *iface)
{
    return CONTAINING_RECORD(iface, struct quartz_vmr, IVMRWindowlessControl9_iface);
}

typedef struct
{
    IVMRImagePresenter9 IVMRImagePresenter9_iface;
    IVMRSurfaceAllocatorEx9 IVMRSurfaceAllocatorEx9_iface;

    LONG refCount;

    IDirect3DDevice9 *d3d9_dev;
    IDirect3D9 *d3d9_ptr;
    IDirect3DSurface9 **d3d9_surfaces;
    IDirect3DVertexBuffer9 *d3d9_vertex;
    HMONITOR hMon;
    DWORD num_surfaces;

    BOOL reset;
    VMR9AllocationInfo info;

    struct quartz_vmr* pVMR9;
    IVMRSurfaceAllocatorNotify9 *SurfaceAllocatorNotify;
} VMR9DefaultAllocatorPresenterImpl;

static inline VMR9DefaultAllocatorPresenterImpl *impl_from_IVMRImagePresenter9( IVMRImagePresenter9 *iface)
{
    return CONTAINING_RECORD(iface, VMR9DefaultAllocatorPresenterImpl, IVMRImagePresenter9_iface);
}

static inline VMR9DefaultAllocatorPresenterImpl *impl_from_IVMRSurfaceAllocatorEx9( IVMRSurfaceAllocatorEx9 *iface)
{
    return CONTAINING_RECORD(iface, VMR9DefaultAllocatorPresenterImpl, IVMRSurfaceAllocatorEx9_iface);
}

static HRESULT VMR9DefaultAllocatorPresenterImpl_create(struct quartz_vmr *parent, LPVOID * ppv);

static inline struct quartz_vmr *impl_from_IBaseFilter(IBaseFilter *iface)
{
    return CONTAINING_RECORD(iface, struct quartz_vmr, renderer.filter.IBaseFilter_iface);
}

static DWORD VMR9_SendSampleData(struct quartz_vmr *This, VMR9PresentationInfo *info, LPBYTE data,
                                 DWORD size)
{
    AM_MEDIA_TYPE *amt;
    HRESULT hr = S_OK;
    int width;
    int height;
    BITMAPINFOHEADER *bmiHeader;
    D3DLOCKED_RECT lock;

    TRACE("%p %p %d\n", This, data, size);

    amt = &This->renderer.sink.pin.mt;

    if (IsEqualIID(&amt->formattype, &FORMAT_VideoInfo))
    {
        bmiHeader = &((VIDEOINFOHEADER *)amt->pbFormat)->bmiHeader;
    }
    else if (IsEqualIID(&amt->formattype, &FORMAT_VideoInfo2))
    {
        bmiHeader = &((VIDEOINFOHEADER2 *)amt->pbFormat)->bmiHeader;
    }
    else
    {
        FIXME("Unknown type %s\n", debugstr_guid(&amt->subtype));
        return VFW_E_RUNTIME_ERROR;
    }

    width = bmiHeader->biWidth;
    height = bmiHeader->biHeight;

    TRACE("Src Rect: %s\n", wine_dbgstr_rect(&This->source_rect));
    TRACE("Dst Rect: %s\n", wine_dbgstr_rect(&This->target_rect));

    hr = IDirect3DSurface9_LockRect(info->lpSurf, &lock, NULL, D3DLOCK_DISCARD);
    if (FAILED(hr))
    {
        ERR("IDirect3DSurface9_LockRect failed (%x)\n",hr);
        return hr;
    }

    if (height > 0) {
        /* Bottom up image needs inverting */
        lock.pBits = (char *)lock.pBits + (height * lock.Pitch);
        while (height--)
        {
            lock.pBits = (char *)lock.pBits - lock.Pitch;
            memcpy(lock.pBits, data, width * bmiHeader->biBitCount / 8);
            data = data + width * bmiHeader->biBitCount / 8;
        }
    }
    else if (lock.Pitch != width * bmiHeader->biBitCount / 8)
    {
        WARN("Slow path! %u/%u\n", lock.Pitch, width * bmiHeader->biBitCount/8);

        while (height--)
        {
            memcpy(lock.pBits, data, width * bmiHeader->biBitCount / 8);
            data = data + width * bmiHeader->biBitCount / 8;
            lock.pBits = (char *)lock.pBits + lock.Pitch;
        }
    }
    else memcpy(lock.pBits, data, size);

    IDirect3DSurface9_UnlockRect(info->lpSurf);

    hr = IVMRImagePresenter9_PresentImage(This->presenter, This->cookie, info);
    return hr;
}

static HRESULT WINAPI VMR9_DoRenderSample(struct strmbase_renderer *iface, IMediaSample *pSample)
{
    struct quartz_vmr *This = impl_from_IBaseFilter(&iface->filter.IBaseFilter_iface);
    const HANDLE events[2] = {This->run_event, This->renderer.flush_event};
    VMR9PresentationInfo info = {};
    LPBYTE pbSrcStream = NULL;
    long cbSrcStream = 0;
    REFERENCE_TIME tStart, tStop;
    HRESULT hr;

    TRACE("%p %p\n", iface, pSample);

    /* It is possible that there is no device at this point */

    if (!This->allocator || !This->presenter)
    {
        ERR("NO PRESENTER!!\n");
        return S_FALSE;
    }

    hr = IMediaSample_GetTime(pSample, &tStart, &tStop);
    if (FAILED(hr))
        info.dwFlags = VMR9Sample_SrcDstRectsValid;
    else
        info.dwFlags = VMR9Sample_SrcDstRectsValid | VMR9Sample_TimeValid;

    if (IMediaSample_IsDiscontinuity(pSample) == S_OK)
        info.dwFlags |= VMR9Sample_Discontinuity;

    if (IMediaSample_IsPreroll(pSample) == S_OK)
        info.dwFlags |= VMR9Sample_Preroll;

    if (IMediaSample_IsSyncPoint(pSample) == S_OK)
        info.dwFlags |= VMR9Sample_SyncPoint;

    /* If we render ourselves, and this is a preroll sample, discard it */
    if (info.dwFlags & VMR9Sample_Preroll)
    {
        return S_OK;
    }

    hr = IMediaSample_GetPointer(pSample, &pbSrcStream);
    if (FAILED(hr))
    {
        ERR("Cannot get pointer to sample data (%x)\n", hr);
        return hr;
    }

    cbSrcStream = IMediaSample_GetActualDataLength(pSample);

    info.rtStart = tStart;
    info.rtEnd = tStop;
    info.szAspectRatio.cx = This->bmiheader.biWidth;
    info.szAspectRatio.cy = This->bmiheader.biHeight;
    info.lpSurf = This->surfaces[(++This->cur_surface) % This->num_surfaces];

    VMR9_SendSampleData(This, &info, pbSrcStream, cbSrcStream);

    if (This->renderer.filter.state == State_Paused)
    {
        LeaveCriticalSection(&This->renderer.csRenderLock);
        WaitForMultipleObjects(2, events, FALSE, INFINITE);
        EnterCriticalSection(&This->renderer.csRenderLock);
    }

    return hr;
}

static HRESULT WINAPI VMR9_CheckMediaType(struct strmbase_renderer *iface, const AM_MEDIA_TYPE *mt)
{
    const VIDEOINFOHEADER *vih;

    if (!IsEqualIID(&mt->majortype, &MEDIATYPE_Video) || !mt->pbFormat)
        return S_FALSE;

    if (!IsEqualGUID(&mt->formattype, &FORMAT_VideoInfo)
            && !IsEqualGUID(&mt->formattype, &FORMAT_VideoInfo2))
        return S_FALSE;

    vih = (VIDEOINFOHEADER *)mt->pbFormat;

    if (vih->bmiHeader.biCompression != BI_RGB)
        return S_FALSE;
    return S_OK;
}

static HRESULT initialize_device(struct quartz_vmr *filter, VMR9AllocationInfo *info)
{
    DWORD buffer_count = 1, i;
    HRESULT hr;

    if (FAILED(hr = IVMRSurfaceAllocatorEx9_InitializeDevice(filter->allocator,
            filter->cookie, info, &buffer_count)))
    {
        WARN("Failed to initialize device (flags %#x), hr %#x.\n", info->dwFlags, hr);
        return hr;
    }

    if (!(filter->surfaces = calloc(buffer_count, sizeof(IDirect3DSurface9 *))))
    {
        IVMRSurfaceAllocatorEx9_TerminateDevice(filter->allocator, filter->cookie);
        return E_OUTOFMEMORY;
    }

    for (i = 0; i < buffer_count; ++i)
    {
        if (FAILED(hr = IVMRSurfaceAllocatorEx9_GetSurface(filter->allocator,
                filter->cookie, i, 0, &filter->surfaces[i])))
        {
            ERR("Failed to get surface %u, hr %#x.\n", i, hr);
            while (i--)
                IDirect3DSurface9_Release(filter->surfaces[i]);
            IVMRSurfaceAllocatorEx9_TerminateDevice(filter->allocator, filter->cookie);
            return hr;
        }
    }

    SetRect(&filter->source_rect, 0, 0, filter->bmiheader.biWidth, filter->bmiheader.biHeight);
    filter->num_surfaces = buffer_count;

    return hr;
}

static HRESULT VMR9_maybe_init(struct quartz_vmr *filter, BOOL force, const AM_MEDIA_TYPE *mt)
{
    VMR9AllocationInfo info = {};
    HRESULT hr = E_FAIL;
    unsigned int i;

    static const struct
    {
        const GUID *subtype;
        D3DFORMAT format;
        DWORD flags;
    }
    formats[] =
    {
        {&MEDIASUBTYPE_ARGB1555, D3DFMT_A1R5G5B5, VMR9AllocFlag_TextureSurface},
        {&MEDIASUBTYPE_ARGB32, D3DFMT_A8R8G8B8, VMR9AllocFlag_TextureSurface},
        {&MEDIASUBTYPE_ARGB4444, D3DFMT_A4R4G4B4, VMR9AllocFlag_TextureSurface},

        {&MEDIASUBTYPE_RGB24, D3DFMT_R8G8B8, VMR9AllocFlag_TextureSurface | VMR9AllocFlag_OffscreenSurface},
        {&MEDIASUBTYPE_RGB32, D3DFMT_X8R8G8B8, VMR9AllocFlag_TextureSurface | VMR9AllocFlag_OffscreenSurface},
        {&MEDIASUBTYPE_RGB555, D3DFMT_X1R5G5B5, VMR9AllocFlag_TextureSurface | VMR9AllocFlag_OffscreenSurface},
        {&MEDIASUBTYPE_RGB565, D3DFMT_R5G6B5, VMR9AllocFlag_TextureSurface | VMR9AllocFlag_OffscreenSurface},

        {&MEDIASUBTYPE_NV12, MAKEFOURCC('N','V','1','2'), VMR9AllocFlag_OffscreenSurface},
        {&MEDIASUBTYPE_UYVY, D3DFMT_UYVY, VMR9AllocFlag_OffscreenSurface},
        {&MEDIASUBTYPE_YUY2, D3DFMT_YUY2, VMR9AllocFlag_OffscreenSurface},
        {&MEDIASUBTYPE_YV12, MAKEFOURCC('Y','V','1','2'), VMR9AllocFlag_OffscreenSurface},
    };

    TRACE("Initializing in mode %u, our window %p, clipping window %p.\n",
            filter->mode, filter->baseControlWindow.baseWindow.hWnd, filter->hWndClippingWindow);
    if (filter->num_surfaces)
        return S_OK;

    if (filter->mode == VMR9Mode_Windowless && !filter->hWndClippingWindow)
        return (force ? VFW_E_RUNTIME_ERROR : S_OK);

    info.dwWidth = filter->source_rect.right;
    info.dwHeight = filter->source_rect.bottom;
    info.Pool = D3DPOOL_DEFAULT;
    info.MinBuffers = 1;
    info.szAspectRatio.cx = info.dwWidth;
    info.szAspectRatio.cy = info.dwHeight;
    info.szNativeSize.cx = filter->bmiheader.biWidth;
    info.szNativeSize.cy = filter->bmiheader.biHeight;

    filter->cur_surface = 0;
    if (filter->num_surfaces)
    {
        ERR("num_surfaces or d3d9_surfaces not 0\n");
        return E_FAIL;
    }

    if (IsEqualGUID(&filter->renderer.filter.clsid, &CLSID_VideoMixingRenderer))
    {
        switch (filter->bmiheader.biBitCount)
        {
            case 8: info.Format = D3DFMT_R3G3B2; break;
            case 15: info.Format = D3DFMT_X1R5G5B5; break;
            case 16: info.Format = D3DFMT_R5G6B5; break;
            case 24: info.Format = D3DFMT_R8G8B8; break;
            case 32: info.Format = D3DFMT_X8R8G8B8; break;
            default:
                FIXME("Unhandled bit depth %u.\n", filter->bmiheader.biBitCount);
                return E_INVALIDARG;
        }

        info.dwFlags = VMR9AllocFlag_TextureSurface;
        return initialize_device(filter, &info);
    }

    for (i = 0; i < ARRAY_SIZE(formats); ++i)
    {
        if (IsEqualGUID(&mt->subtype, formats[i].subtype))
        {
            info.Format = formats[i].format;

            if (formats[i].flags & VMR9AllocFlag_TextureSurface)
            {
                info.dwFlags = VMR9AllocFlag_TextureSurface;
                if (SUCCEEDED(hr = initialize_device(filter, &info)))
                    return hr;
            }

            if (formats[i].flags & VMR9AllocFlag_OffscreenSurface)
            {
                info.dwFlags = VMR9AllocFlag_OffscreenSurface;
                if (SUCCEEDED(hr = initialize_device(filter, &info)))
                    return hr;
            }
        }
    }

    return hr;
}

static void vmr_start_stream(struct strmbase_renderer *iface)
{
    struct quartz_vmr *This = impl_from_IBaseFilter(&iface->filter.IBaseFilter_iface);

    TRACE("(%p)\n", This);

    if (This->renderer.sink.pin.peer)
        VMR9_maybe_init(This, TRUE, &This->renderer.sink.pin.mt);
    IVMRImagePresenter9_StartPresenting(This->presenter, This->cookie);
    SetWindowPos(This->baseControlWindow.baseWindow.hWnd, NULL,
        This->source_rect.left,
        This->source_rect.top,
        This->source_rect.right - This->source_rect.left,
        This->source_rect.bottom - This->source_rect.top,
        SWP_NOZORDER|SWP_NOMOVE|SWP_DEFERERASE);
    ShowWindow(This->baseControlWindow.baseWindow.hWnd, SW_SHOW);
    GetClientRect(This->baseControlWindow.baseWindow.hWnd, &This->target_rect);
    SetEvent(This->run_event);
}

static void vmr_stop_stream(struct strmbase_renderer *iface)
{
    struct quartz_vmr *This = impl_from_IBaseFilter(&iface->filter.IBaseFilter_iface);

    TRACE("(%p)\n", This);

    if (This->renderer.filter.state == State_Running)
        IVMRImagePresenter9_StopPresenting(This->presenter, This->cookie);
    ResetEvent(This->run_event);
}

static HRESULT WINAPI VMR9_ShouldDrawSampleNow(struct strmbase_renderer *iface,
        IMediaSample *pSample, REFERENCE_TIME *start, REFERENCE_TIME *end)
{
    /* Preroll means the sample isn't shown, this is used for key frames and things like that */
    if (IMediaSample_IsPreroll(pSample) == S_OK)
        return E_FAIL;
    return S_FALSE;
}

static HRESULT vmr_connect(struct strmbase_renderer *iface, const AM_MEDIA_TYPE *mt)
{
    struct quartz_vmr *filter = impl_from_IBaseFilter(&iface->filter.IBaseFilter_iface);
    HRESULT hr;

    if (IsEqualGUID(&mt->formattype, &FORMAT_VideoInfo))
    {
        VIDEOINFOHEADER *format = (VIDEOINFOHEADER *)mt->pbFormat;

        filter->bmiheader = format->bmiHeader;
        filter->VideoWidth = format->bmiHeader.biWidth;
        filter->VideoHeight = format->bmiHeader.biHeight;
        SetRect(&filter->source_rect, 0, 0, filter->VideoWidth, filter->VideoHeight);
    }
    else if (IsEqualIID(&mt->formattype, &FORMAT_VideoInfo2))
    {
        VIDEOINFOHEADER2 *format = (VIDEOINFOHEADER2 *)mt->pbFormat;

        filter->bmiheader = format->bmiHeader;
        filter->VideoWidth = format->bmiHeader.biWidth;
        filter->VideoHeight = format->bmiHeader.biHeight;
        SetRect(&filter->source_rect, 0, 0, filter->VideoWidth, filter->VideoHeight);
    }

    if (filter->mode
            || SUCCEEDED(hr = IVMRFilterConfig9_SetRenderingMode(&filter->IVMRFilterConfig9_iface, VMR9Mode_Windowed)))
        hr = VMR9_maybe_init(filter, FALSE, mt);

    return hr;
}

static HRESULT WINAPI VMR9_BreakConnect(struct strmbase_renderer *This)
{
    struct quartz_vmr *pVMR9 = impl_from_IBaseFilter(&This->filter.IBaseFilter_iface);
    HRESULT hr = S_OK;
    DWORD i;

    if (!pVMR9->mode)
        return S_FALSE;
     if (This->sink.pin.peer && pVMR9->allocator && pVMR9->presenter)
    {
        if (pVMR9->renderer.filter.state != State_Stopped)
        {
            ERR("Disconnecting while not stopped! UNTESTED!!\n");
        }
        if (pVMR9->renderer.filter.state == State_Running)
            hr = IVMRImagePresenter9_StopPresenting(pVMR9->presenter, pVMR9->cookie);

        for (i = 0; i < pVMR9->num_surfaces; ++i)
            IDirect3DSurface9_Release(pVMR9->surfaces[i]);
        free(pVMR9->surfaces);
        IVMRSurfaceAllocatorEx9_TerminateDevice(pVMR9->allocator, pVMR9->cookie);
        pVMR9->num_surfaces = 0;
    }
    return hr;
}

static void vmr_destroy(struct strmbase_renderer *iface)
{
    struct quartz_vmr *filter = impl_from_IBaseFilter(&iface->filter.IBaseFilter_iface);

    BaseControlWindow_Destroy(&filter->baseControlWindow);

    if (filter->allocator)
        IVMRSurfaceAllocatorEx9_Release(filter->allocator);
    if (filter->presenter)
        IVMRImagePresenter9_Release(filter->presenter);

    filter->num_surfaces = 0;
    if (filter->allocator_d3d9_dev)
    {
        IDirect3DDevice9_Release(filter->allocator_d3d9_dev);
        filter->allocator_d3d9_dev = NULL;
    }

    CloseHandle(filter->run_event);
    FreeLibrary(filter->hD3d9);
    BaseControlWindow_Destroy(&filter->baseControlWindow);
    strmbase_renderer_cleanup(&filter->renderer);
    CoTaskMemFree(filter);

    InterlockedDecrement(&object_locks);
}

static HRESULT vmr_query_interface(struct strmbase_renderer *iface, REFIID iid, void **out)
{
    struct quartz_vmr *filter = impl_from_IBaseFilter(&iface->filter.IBaseFilter_iface);

    if (IsEqualGUID(iid, &IID_IVideoWindow))
        *out = &filter->baseControlWindow.IVideoWindow_iface;
    else if (IsEqualGUID(iid, &IID_IBasicVideo))
        *out = &filter->baseControlVideo.IBasicVideo_iface;
    else if (IsEqualGUID(iid, &IID_IAMCertifiedOutputProtection))
        *out = &filter->IAMCertifiedOutputProtection_iface;
    else if (IsEqualGUID(iid, &IID_IAMFilterMiscFlags))
        *out = &filter->IAMFilterMiscFlags_iface;
    else if (IsEqualGUID(iid, &IID_IVMRFilterConfig))
        *out = &filter->IVMRFilterConfig_iface;
    else if (IsEqualGUID(iid, &IID_IVMRFilterConfig9))
        *out = &filter->IVMRFilterConfig9_iface;
    else if (IsEqualGUID(iid, &IID_IVMRMonitorConfig))
        *out = &filter->IVMRMonitorConfig_iface;
    else if (IsEqualGUID(iid, &IID_IVMRMonitorConfig9))
        *out = &filter->IVMRMonitorConfig9_iface;
    else if (IsEqualGUID(iid, &IID_IVMRSurfaceAllocatorNotify) && filter->mode == (VMR9Mode)VMRMode_Renderless)
        *out = &filter->IVMRSurfaceAllocatorNotify_iface;
    else if (IsEqualGUID(iid, &IID_IVMRSurfaceAllocatorNotify9) && filter->mode == VMR9Mode_Renderless)
        *out = &filter->IVMRSurfaceAllocatorNotify9_iface;
    else if (IsEqualGUID(iid, &IID_IVMRWindowlessControl) && filter->mode == (VMR9Mode)VMRMode_Windowless)
        *out = &filter->IVMRWindowlessControl_iface;
    else if (IsEqualGUID(iid, &IID_IVMRWindowlessControl9) && filter->mode == VMR9Mode_Windowless)
        *out = &filter->IVMRWindowlessControl9_iface;
    else
        return E_NOINTERFACE;

    IUnknown_AddRef((IUnknown *)*out);
    return S_OK;
}

static HRESULT vmr_pin_query_interface(struct strmbase_renderer *iface, REFIID iid, void **out)
{
    struct quartz_vmr *filter = impl_from_IBaseFilter(&iface->filter.IBaseFilter_iface);

    if (IsEqualGUID(iid, &IID_IOverlay))
        *out = &filter->IOverlay_iface;
    else
        return E_NOINTERFACE;

    IUnknown_AddRef((IUnknown *)*out);
    return S_OK;
}

static const struct strmbase_renderer_ops renderer_ops =
{
    .pfnCheckMediaType = VMR9_CheckMediaType,
    .pfnDoRenderSample = VMR9_DoRenderSample,
    .renderer_start_stream = vmr_start_stream,
    .renderer_stop_stream = vmr_stop_stream,
    .pfnShouldDrawSampleNow = VMR9_ShouldDrawSampleNow,
    .renderer_connect = vmr_connect,
    .pfnBreakConnect = VMR9_BreakConnect,
    .renderer_destroy = vmr_destroy,
    .renderer_query_interface = vmr_query_interface,
    .renderer_pin_query_interface = vmr_pin_query_interface,
};

static RECT WINAPI VMR9_GetDefaultRect(BaseWindow *This)
{
    struct quartz_vmr* pVMR9 = impl_from_BaseWindow(This);
    static RECT defRect;

    SetRect(&defRect, 0, 0, pVMR9->VideoWidth, pVMR9->VideoHeight);

    return defRect;
}

static BOOL WINAPI VMR9_OnSize(BaseWindow *This, LONG Width, LONG Height)
{
    struct quartz_vmr* pVMR9 = impl_from_BaseWindow(This);

    TRACE("WM_SIZE %d %d\n", Width, Height);
    GetClientRect(This->hWnd, &pVMR9->target_rect);
    TRACE("WM_SIZING: DestRect=(%d,%d),(%d,%d)\n",
        pVMR9->target_rect.left,
        pVMR9->target_rect.top,
        pVMR9->target_rect.right - pVMR9->target_rect.left,
        pVMR9->target_rect.bottom - pVMR9->target_rect.top);

    return TRUE;
}

static const BaseWindowFuncTable renderer_BaseWindowFuncTable = {
    VMR9_GetDefaultRect,
    VMR9_OnSize,
};

static HRESULT WINAPI VMR9_GetSourceRect(BaseControlVideo* This, RECT *pSourceRect)
{
    struct quartz_vmr* pVMR9 = impl_from_BaseControlVideo(This);
    CopyRect(pSourceRect,&pVMR9->source_rect);
    return S_OK;
}

static HRESULT WINAPI VMR9_GetStaticImage(BaseControlVideo *iface, LONG *size, LONG *image)
{
    struct quartz_vmr *filter = impl_from_BaseControlVideo(iface);
    const AM_MEDIA_TYPE *mt = &filter->renderer.sink.pin.mt;
    IDirect3DSurface9 *rt = NULL, *surface = NULL;
    D3DLOCKED_RECT locked_rect;
    IDirect3DDevice9 *device;
    unsigned int row_size;
    BITMAPINFOHEADER bih;
    LONG i, size_left;
    char *dst;
    HRESULT hr;

    TRACE("filter %p, size %d, image %p.\n", filter, *size, image);

    EnterCriticalSection(&filter->renderer.csRenderLock);
    device = filter->allocator_d3d9_dev;

    if (IsEqualGUID(&mt->formattype, &FORMAT_VideoInfo))
        bih = ((VIDEOINFOHEADER *)mt->pbFormat)->bmiHeader;
    else if (IsEqualGUID(&mt->formattype, &FORMAT_VideoInfo2))
        bih = ((VIDEOINFOHEADER2 *)mt->pbFormat)->bmiHeader;
    bih.biSizeImage = bih.biWidth * bih.biHeight * bih.biBitCount / 8;

    if (!image)
    {
        *size = sizeof(BITMAPINFOHEADER) + bih.biSizeImage;
        LeaveCriticalSection(&filter->renderer.csRenderLock);
        return S_OK;
    }

    if (FAILED(hr = IDirect3DDevice9_GetRenderTarget(device, 0, &rt)))
        goto out;

    if (FAILED(hr = IDirect3DDevice9_CreateOffscreenPlainSurface(device, bih.biWidth,
            bih.biHeight, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &surface, NULL)))
        goto out;

    if (FAILED(hr = IDirect3DDevice9_GetRenderTargetData(device, rt, surface)))
        goto out;

    if (FAILED(hr = IDirect3DSurface9_LockRect(surface, &locked_rect, NULL, D3DLOCK_READONLY)))
        goto out;

    size_left = *size;
    memcpy(image, &bih, min(size_left, sizeof(BITMAPINFOHEADER)));
    size_left -= sizeof(BITMAPINFOHEADER);

    dst = (char *)image + sizeof(BITMAPINFOHEADER);
    row_size = bih.biWidth * bih.biBitCount / 8;

    for (i = 0; i < bih.biHeight && size_left > 0; ++i)
    {
        memcpy(dst, (char *)locked_rect.pBits + (i * locked_rect.Pitch), min(row_size, size_left));
        dst += row_size;
        size_left -= row_size;
    }

    IDirect3DSurface9_UnlockRect(surface);

out:
    if (surface) IDirect3DSurface9_Release(surface);
    if (rt) IDirect3DSurface9_Release(rt);
    LeaveCriticalSection(&filter->renderer.csRenderLock);
    return hr;
}

static HRESULT WINAPI VMR9_GetTargetRect(BaseControlVideo* This, RECT *pTargetRect)
{
    struct quartz_vmr* pVMR9 = impl_from_BaseControlVideo(This);
    CopyRect(pTargetRect,&pVMR9->target_rect);
    return S_OK;
}

static VIDEOINFOHEADER* WINAPI VMR9_GetVideoFormat(BaseControlVideo* This)
{
    struct quartz_vmr* pVMR9 = impl_from_BaseControlVideo(This);
    AM_MEDIA_TYPE *pmt;

    TRACE("(%p/%p)\n", pVMR9, This);

    pmt = &pVMR9->renderer.sink.pin.mt;
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

static HRESULT WINAPI VMR9_IsDefaultSourceRect(BaseControlVideo* This)
{
    struct quartz_vmr* pVMR9 = impl_from_BaseControlVideo(This);
    FIXME("(%p/%p)->(): stub !!!\n", pVMR9, This);

    return S_OK;
}

static HRESULT WINAPI VMR9_IsDefaultTargetRect(BaseControlVideo* This)
{
    struct quartz_vmr* pVMR9 = impl_from_BaseControlVideo(This);
    FIXME("(%p/%p)->(): stub !!!\n", pVMR9, This);

    return S_OK;
}

static HRESULT WINAPI VMR9_SetDefaultSourceRect(BaseControlVideo* This)
{
    struct quartz_vmr* pVMR9 = impl_from_BaseControlVideo(This);

    SetRect(&pVMR9->source_rect, 0, 0, pVMR9->VideoWidth, pVMR9->VideoHeight);

    return S_OK;
}

static HRESULT WINAPI VMR9_SetDefaultTargetRect(BaseControlVideo* This)
{
    RECT rect;
    struct quartz_vmr* pVMR9 = impl_from_BaseControlVideo(This);

    if (!GetClientRect(pVMR9->baseControlWindow.baseWindow.hWnd, &rect))
        return E_FAIL;

    SetRect(&pVMR9->target_rect, 0, 0, rect.right, rect.bottom);

    return S_OK;
}

static HRESULT WINAPI VMR9_SetSourceRect(BaseControlVideo* This, RECT *pSourceRect)
{
    struct quartz_vmr* pVMR9 = impl_from_BaseControlVideo(This);
    CopyRect(&pVMR9->source_rect,pSourceRect);
    return S_OK;
}

static HRESULT WINAPI VMR9_SetTargetRect(BaseControlVideo* This, RECT *pTargetRect)
{
    struct quartz_vmr* pVMR9 = impl_from_BaseControlVideo(This);
    CopyRect(&pVMR9->target_rect,pTargetRect);
    return S_OK;
}

static const BaseControlVideoFuncTable renderer_BaseControlVideoFuncTable = {
    VMR9_GetSourceRect,
    VMR9_GetStaticImage,
    VMR9_GetTargetRect,
    VMR9_GetVideoFormat,
    VMR9_IsDefaultSourceRect,
    VMR9_IsDefaultTargetRect,
    VMR9_SetDefaultSourceRect,
    VMR9_SetDefaultTargetRect,
    VMR9_SetSourceRect,
    VMR9_SetTargetRect
};

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
    BaseControlWindowImpl_get_FullScreenMode,
    BaseControlWindowImpl_put_FullScreenMode,
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

static HRESULT WINAPI AMCertifiedOutputProtection_QueryInterface(IAMCertifiedOutputProtection *iface,
                                                                 REFIID riid, void **ppv)
{
    struct quartz_vmr *This = impl_from_IAMCertifiedOutputProtection(iface);
    return IUnknown_QueryInterface(This->renderer.filter.outer_unk, riid, ppv);
}

static ULONG WINAPI AMCertifiedOutputProtection_AddRef(IAMCertifiedOutputProtection *iface)
{
    struct quartz_vmr *This = impl_from_IAMCertifiedOutputProtection(iface);
    return IUnknown_AddRef(This->renderer.filter.outer_unk);
}

static ULONG WINAPI AMCertifiedOutputProtection_Release(IAMCertifiedOutputProtection *iface)
{
    struct quartz_vmr *This = impl_from_IAMCertifiedOutputProtection(iface);
    return IUnknown_Release(This->renderer.filter.outer_unk);
}

static HRESULT WINAPI AMCertifiedOutputProtection_KeyExchange(IAMCertifiedOutputProtection *iface,
                                                              GUID* pRandom, BYTE** VarLenCertGH,
                                                              DWORD* pdwLengthCertGH)
{
    struct quartz_vmr *This = impl_from_IAMCertifiedOutputProtection(iface);

    FIXME("(%p/%p)->(%p, %p, %p) stub\n", iface, This, pRandom, VarLenCertGH, pdwLengthCertGH);
    return VFW_E_NO_COPP_HW;
}

static HRESULT WINAPI AMCertifiedOutputProtection_SessionSequenceStart(IAMCertifiedOutputProtection *iface,
                                                                       AMCOPPSignature* pSig)
{
    struct quartz_vmr *This = impl_from_IAMCertifiedOutputProtection(iface);

    FIXME("(%p/%p)->(%p) stub\n", iface, This, pSig);
    return VFW_E_NO_COPP_HW;
}

static HRESULT WINAPI AMCertifiedOutputProtection_ProtectionCommand(IAMCertifiedOutputProtection *iface,
                                                                    const AMCOPPCommand* cmd)
{
    struct quartz_vmr *This = impl_from_IAMCertifiedOutputProtection(iface);

    FIXME("(%p/%p)->(%p) stub\n", iface, This, cmd);
    return VFW_E_NO_COPP_HW;
}

static HRESULT WINAPI AMCertifiedOutputProtection_ProtectionStatus(IAMCertifiedOutputProtection *iface,
                                                                   const AMCOPPStatusInput* pStatusInput,
                                                                   AMCOPPStatusOutput* pStatusOutput)
{
    struct quartz_vmr *This = impl_from_IAMCertifiedOutputProtection(iface);

    FIXME("(%p/%p)->(%p, %p) stub\n", iface, This, pStatusInput, pStatusOutput);
    return VFW_E_NO_COPP_HW;
}

static const IAMCertifiedOutputProtectionVtbl IAMCertifiedOutputProtection_Vtbl =
{
    AMCertifiedOutputProtection_QueryInterface,
    AMCertifiedOutputProtection_AddRef,
    AMCertifiedOutputProtection_Release,
    AMCertifiedOutputProtection_KeyExchange,
    AMCertifiedOutputProtection_SessionSequenceStart,
    AMCertifiedOutputProtection_ProtectionCommand,
    AMCertifiedOutputProtection_ProtectionStatus
};

static HRESULT WINAPI AMFilterMiscFlags_QueryInterface(IAMFilterMiscFlags *iface, REFIID riid, void **ppv) {
    struct quartz_vmr *This = impl_from_IAMFilterMiscFlags(iface);
    return IUnknown_QueryInterface(This->renderer.filter.outer_unk, riid, ppv);
}

static ULONG WINAPI AMFilterMiscFlags_AddRef(IAMFilterMiscFlags *iface) {
    struct quartz_vmr *This = impl_from_IAMFilterMiscFlags(iface);
    return IUnknown_AddRef(This->renderer.filter.outer_unk);
}

static ULONG WINAPI AMFilterMiscFlags_Release(IAMFilterMiscFlags *iface) {
    struct quartz_vmr *This = impl_from_IAMFilterMiscFlags(iface);
    return IUnknown_Release(This->renderer.filter.outer_unk);
}

static ULONG WINAPI AMFilterMiscFlags_GetMiscFlags(IAMFilterMiscFlags *iface) {
    return AM_FILTER_MISC_FLAGS_IS_RENDERER;
}

static const IAMFilterMiscFlagsVtbl IAMFilterMiscFlags_Vtbl = {
    AMFilterMiscFlags_QueryInterface,
    AMFilterMiscFlags_AddRef,
    AMFilterMiscFlags_Release,
    AMFilterMiscFlags_GetMiscFlags
};

static HRESULT WINAPI VMR7FilterConfig_QueryInterface(IVMRFilterConfig *iface, REFIID riid,
                                                      void** ppv)
{
    struct quartz_vmr *This = impl_from_IVMRFilterConfig(iface);
    return IUnknown_QueryInterface(This->renderer.filter.outer_unk, riid, ppv);
}

static ULONG WINAPI VMR7FilterConfig_AddRef(IVMRFilterConfig *iface)
{
    struct quartz_vmr *This = impl_from_IVMRFilterConfig(iface);
    return IUnknown_AddRef(This->renderer.filter.outer_unk);
}

static ULONG WINAPI VMR7FilterConfig_Release(IVMRFilterConfig *iface)
{
    struct quartz_vmr *This = impl_from_IVMRFilterConfig(iface);
    return IUnknown_Release(This->renderer.filter.outer_unk);
}

static HRESULT WINAPI VMR7FilterConfig_SetImageCompositor(IVMRFilterConfig *iface,
                                                          IVMRImageCompositor *compositor)
{
    struct quartz_vmr *This = impl_from_IVMRFilterConfig(iface);

    FIXME("(%p/%p)->(%p) stub\n", iface, This, compositor);
    return E_NOTIMPL;
}

static HRESULT WINAPI VMR7FilterConfig_SetNumberOfStreams(IVMRFilterConfig *iface, DWORD max)
{
    struct quartz_vmr *This = impl_from_IVMRFilterConfig(iface);

    FIXME("(%p/%p)->(%u) stub\n", iface, This, max);
    return E_NOTIMPL;
}

static HRESULT WINAPI VMR7FilterConfig_GetNumberOfStreams(IVMRFilterConfig *iface, DWORD *max)
{
    struct quartz_vmr *This = impl_from_IVMRFilterConfig(iface);

    FIXME("(%p/%p)->(%p) stub\n", iface, This, max);
    return E_NOTIMPL;
}

static HRESULT WINAPI VMR7FilterConfig_SetRenderingPrefs(IVMRFilterConfig *iface, DWORD renderflags)
{
    struct quartz_vmr *This = impl_from_IVMRFilterConfig(iface);

    FIXME("(%p/%p)->(%u) stub\n", iface, This, renderflags);
    return E_NOTIMPL;
}

static HRESULT WINAPI VMR7FilterConfig_GetRenderingPrefs(IVMRFilterConfig *iface, DWORD *renderflags)
{
    struct quartz_vmr *This = impl_from_IVMRFilterConfig(iface);

    FIXME("(%p/%p)->(%p) stub\n", iface, This, renderflags);
    return E_NOTIMPL;
}

static HRESULT WINAPI VMR7FilterConfig_SetRenderingMode(IVMRFilterConfig *iface, DWORD mode)
{
    struct quartz_vmr *filter = impl_from_IVMRFilterConfig(iface);

    TRACE("iface %p, mode %#x.\n", iface, mode);

    return IVMRFilterConfig9_SetRenderingMode(&filter->IVMRFilterConfig9_iface, mode);
}

static HRESULT WINAPI VMR7FilterConfig_GetRenderingMode(IVMRFilterConfig *iface, DWORD *mode)
{
    struct quartz_vmr *This = impl_from_IVMRFilterConfig(iface);

    TRACE("(%p/%p)->(%p)\n", iface, This, mode);
    if (!mode) return E_POINTER;

    if (This->mode)
        *mode = This->mode;
    else
        *mode = VMRMode_Windowed;

    return S_OK;
}

static const IVMRFilterConfigVtbl VMR7_FilterConfig_Vtbl =
{
    VMR7FilterConfig_QueryInterface,
    VMR7FilterConfig_AddRef,
    VMR7FilterConfig_Release,
    VMR7FilterConfig_SetImageCompositor,
    VMR7FilterConfig_SetNumberOfStreams,
    VMR7FilterConfig_GetNumberOfStreams,
    VMR7FilterConfig_SetRenderingPrefs,
    VMR7FilterConfig_GetRenderingPrefs,
    VMR7FilterConfig_SetRenderingMode,
    VMR7FilterConfig_GetRenderingMode
};

struct get_available_monitors_args
{
    VMRMONITORINFO *info7;
    VMR9MonitorInfo *info9;
    DWORD arraysize;
    DWORD numdev;
};

static BOOL CALLBACK get_available_monitors_proc(HMONITOR hmon, HDC hdc, LPRECT lprc, LPARAM lparam)
{
    struct get_available_monitors_args *args = (struct get_available_monitors_args *)lparam;
    MONITORINFOEXW mi;

    if (args->info7 || args->info9)
    {

        if (!args->arraysize)
            return FALSE;

        mi.cbSize = sizeof(mi);
        if (!GetMonitorInfoW(hmon, (MONITORINFO*)&mi))
            return TRUE;

        /* fill VMRMONITORINFO struct */
        if (args->info7)
        {
            VMRMONITORINFO *info = args->info7++;
            memset(info, 0, sizeof(*info));

            if (args->numdev > 0)
            {
                info->guid.pGUID = &info->guid.GUID;
                info->guid.GUID.Data4[7] = args->numdev;
            }
            else
                info->guid.pGUID = NULL;

            info->rcMonitor     = mi.rcMonitor;
            info->hMon          = hmon;
            info->dwFlags       = mi.dwFlags;

            lstrcpynW(info->szDevice, mi.szDevice, ARRAY_SIZE(info->szDevice));

            /* FIXME: how to get these values? */
            info->szDescription[0] = 0;
        }

        /* fill VMR9MonitorInfo struct */
        if (args->info9)
        {
            VMR9MonitorInfo *info = args->info9++;
            memset(info, 0, sizeof(*info));

            info->uDevID        = 0; /* FIXME */
            info->rcMonitor     = mi.rcMonitor;
            info->hMon          = hmon;
            info->dwFlags       = mi.dwFlags;

            lstrcpynW(info->szDevice, mi.szDevice, ARRAY_SIZE(info->szDevice));

            /* FIXME: how to get these values? */
            info->szDescription[0] = 0;
            info->dwVendorId    = 0;
            info->dwDeviceId    = 0;
            info->dwSubSysId    = 0;
            info->dwRevision    = 0;
        }

        args->arraysize--;
    }

    args->numdev++;
    return TRUE;
}

static HRESULT WINAPI VMR7MonitorConfig_QueryInterface(IVMRMonitorConfig *iface, REFIID riid,
                                                       LPVOID * ppv)
{
    struct quartz_vmr *This = impl_from_IVMRMonitorConfig(iface);
    return IUnknown_QueryInterface(This->renderer.filter.outer_unk, riid, ppv);
}

static ULONG WINAPI VMR7MonitorConfig_AddRef(IVMRMonitorConfig *iface)
{
    struct quartz_vmr *This = impl_from_IVMRMonitorConfig(iface);
    return IUnknown_AddRef(This->renderer.filter.outer_unk);
}

static ULONG WINAPI VMR7MonitorConfig_Release(IVMRMonitorConfig *iface)
{
    struct quartz_vmr *This = impl_from_IVMRMonitorConfig(iface);
    return IUnknown_Release(This->renderer.filter.outer_unk);
}

static HRESULT WINAPI VMR7MonitorConfig_SetMonitor(IVMRMonitorConfig *iface, const VMRGUID *pGUID)
{
    struct quartz_vmr *This = impl_from_IVMRMonitorConfig(iface);

    FIXME("(%p/%p)->(%p) stub\n", iface, This, pGUID);

    if (!pGUID)
        return E_POINTER;

    return S_OK;
}

static HRESULT WINAPI VMR7MonitorConfig_GetMonitor(IVMRMonitorConfig *iface, VMRGUID *pGUID)
{
    struct quartz_vmr *This = impl_from_IVMRMonitorConfig(iface);

    FIXME("(%p/%p)->(%p) stub\n", iface, This, pGUID);

    if (!pGUID)
        return E_POINTER;

    pGUID->pGUID = NULL; /* default DirectDraw device */
    return S_OK;
}

static HRESULT WINAPI VMR7MonitorConfig_SetDefaultMonitor(IVMRMonitorConfig *iface,
                                                          const VMRGUID *pGUID)
{
    struct quartz_vmr *This = impl_from_IVMRMonitorConfig(iface);

    FIXME("(%p/%p)->(%p) stub\n", iface, This, pGUID);

    if (!pGUID)
        return E_POINTER;

    return S_OK;
}

static HRESULT WINAPI VMR7MonitorConfig_GetDefaultMonitor(IVMRMonitorConfig *iface, VMRGUID *pGUID)
{
    struct quartz_vmr *This = impl_from_IVMRMonitorConfig(iface);

    FIXME("(%p/%p)->(%p) stub\n", iface, This, pGUID);

    if (!pGUID)
        return E_POINTER;

    pGUID->pGUID = NULL; /* default DirectDraw device */
    return S_OK;
}

static HRESULT WINAPI VMR7MonitorConfig_GetAvailableMonitors(IVMRMonitorConfig *iface,
                                                             VMRMONITORINFO *info, DWORD arraysize,
                                                             DWORD *numdev)
{
    struct quartz_vmr *This = impl_from_IVMRMonitorConfig(iface);
    struct get_available_monitors_args args;

    FIXME("(%p/%p)->(%p, %u, %p) semi-stub\n", iface, This, info, arraysize, numdev);

    if (!numdev)
        return E_POINTER;

    if (info && arraysize == 0)
        return E_INVALIDARG;

    args.info7      = info;
    args.info9      = NULL;
    args.arraysize  = arraysize;
    args.numdev     = 0;
    EnumDisplayMonitors(NULL, NULL, get_available_monitors_proc, (LPARAM)&args);

    *numdev = args.numdev;
    return S_OK;
}

static const IVMRMonitorConfigVtbl VMR7_MonitorConfig_Vtbl =
{
    VMR7MonitorConfig_QueryInterface,
    VMR7MonitorConfig_AddRef,
    VMR7MonitorConfig_Release,
    VMR7MonitorConfig_SetMonitor,
    VMR7MonitorConfig_GetMonitor,
    VMR7MonitorConfig_SetDefaultMonitor,
    VMR7MonitorConfig_GetDefaultMonitor,
    VMR7MonitorConfig_GetAvailableMonitors
};

static HRESULT WINAPI VMR9MonitorConfig_QueryInterface(IVMRMonitorConfig9 *iface, REFIID riid,
                                                       LPVOID * ppv)
{
    struct quartz_vmr *This = impl_from_IVMRMonitorConfig9(iface);
    return IUnknown_QueryInterface(This->renderer.filter.outer_unk, riid, ppv);
}

static ULONG WINAPI VMR9MonitorConfig_AddRef(IVMRMonitorConfig9 *iface)
{
    struct quartz_vmr *This = impl_from_IVMRMonitorConfig9(iface);
    return IUnknown_AddRef(This->renderer.filter.outer_unk);
}

static ULONG WINAPI VMR9MonitorConfig_Release(IVMRMonitorConfig9 *iface)
{
    struct quartz_vmr *This = impl_from_IVMRMonitorConfig9(iface);
    return IUnknown_Release(This->renderer.filter.outer_unk);
}

static HRESULT WINAPI VMR9MonitorConfig_SetMonitor(IVMRMonitorConfig9 *iface, UINT uDev)
{
    struct quartz_vmr *This = impl_from_IVMRMonitorConfig9(iface);

    FIXME("(%p/%p)->(%u) stub\n", iface, This, uDev);

    return S_OK;
}

static HRESULT WINAPI VMR9MonitorConfig_GetMonitor(IVMRMonitorConfig9 *iface, UINT *uDev)
{
    struct quartz_vmr *This = impl_from_IVMRMonitorConfig9(iface);

    FIXME("(%p/%p)->(%p) stub\n", iface, This, uDev);

    if (!uDev)
        return E_POINTER;

    *uDev = 0;
    return S_OK;
}

static HRESULT WINAPI VMR9MonitorConfig_SetDefaultMonitor(IVMRMonitorConfig9 *iface, UINT uDev)
{
    struct quartz_vmr *This = impl_from_IVMRMonitorConfig9(iface);

    FIXME("(%p/%p)->(%u) stub\n", iface, This, uDev);

    return S_OK;
}

static HRESULT WINAPI VMR9MonitorConfig_GetDefaultMonitor(IVMRMonitorConfig9 *iface, UINT *uDev)
{
    struct quartz_vmr *This = impl_from_IVMRMonitorConfig9(iface);

    FIXME("(%p/%p)->(%p) stub\n", iface, This, uDev);

    if (!uDev)
        return E_POINTER;

    *uDev = 0;
    return S_OK;
}

static HRESULT WINAPI VMR9MonitorConfig_GetAvailableMonitors(IVMRMonitorConfig9 *iface,
                                                             VMR9MonitorInfo *info, DWORD arraysize,
                                                             DWORD *numdev)
{
    struct quartz_vmr *This = impl_from_IVMRMonitorConfig9(iface);
    struct get_available_monitors_args args;

    FIXME("(%p/%p)->(%p, %u, %p) semi-stub\n", iface, This, info, arraysize, numdev);

    if (!numdev)
        return E_POINTER;

    if (info && arraysize == 0)
        return E_INVALIDARG;

    args.info7      = NULL;
    args.info9      = info;
    args.arraysize  = arraysize;
    args.numdev     = 0;
    EnumDisplayMonitors(NULL, NULL, get_available_monitors_proc, (LPARAM)&args);

    *numdev = args.numdev;
    return S_OK;
}

static const IVMRMonitorConfig9Vtbl VMR9_MonitorConfig_Vtbl =
{
    VMR9MonitorConfig_QueryInterface,
    VMR9MonitorConfig_AddRef,
    VMR9MonitorConfig_Release,
    VMR9MonitorConfig_SetMonitor,
    VMR9MonitorConfig_GetMonitor,
    VMR9MonitorConfig_SetDefaultMonitor,
    VMR9MonitorConfig_GetDefaultMonitor,
    VMR9MonitorConfig_GetAvailableMonitors
};

static HRESULT WINAPI VMR9FilterConfig_QueryInterface(IVMRFilterConfig9 *iface, REFIID riid, LPVOID * ppv)
{
    struct quartz_vmr *This = impl_from_IVMRFilterConfig9(iface);
    return IUnknown_QueryInterface(This->renderer.filter.outer_unk, riid, ppv);
}

static ULONG WINAPI VMR9FilterConfig_AddRef(IVMRFilterConfig9 *iface)
{
    struct quartz_vmr *This = impl_from_IVMRFilterConfig9(iface);
    return IUnknown_AddRef(This->renderer.filter.outer_unk);
}

static ULONG WINAPI VMR9FilterConfig_Release(IVMRFilterConfig9 *iface)
{
    struct quartz_vmr *This = impl_from_IVMRFilterConfig9(iface);
    return IUnknown_Release(This->renderer.filter.outer_unk);
}

static HRESULT WINAPI VMR9FilterConfig_SetImageCompositor(IVMRFilterConfig9 *iface, IVMRImageCompositor9 *compositor)
{
    struct quartz_vmr *This = impl_from_IVMRFilterConfig9(iface);

    FIXME("(%p/%p)->(%p) stub\n", iface, This, compositor);
    return E_NOTIMPL;
}

static HRESULT WINAPI VMR9FilterConfig_SetNumberOfStreams(IVMRFilterConfig9 *iface, DWORD count)
{
    FIXME("iface %p, count %u, stub!\n", iface, count);
    if (count == 1)
        return S_OK;
    return E_NOTIMPL;
}

static HRESULT WINAPI VMR9FilterConfig_GetNumberOfStreams(IVMRFilterConfig9 *iface, DWORD *max)
{
    struct quartz_vmr *This = impl_from_IVMRFilterConfig9(iface);

    FIXME("(%p/%p)->(%p) stub\n", iface, This, max);
    return E_NOTIMPL;
}

static HRESULT WINAPI VMR9FilterConfig_SetRenderingPrefs(IVMRFilterConfig9 *iface, DWORD renderflags)
{
    struct quartz_vmr *This = impl_from_IVMRFilterConfig9(iface);

    FIXME("(%p/%p)->(%u) stub\n", iface, This, renderflags);
    return E_NOTIMPL;
}

static HRESULT WINAPI VMR9FilterConfig_GetRenderingPrefs(IVMRFilterConfig9 *iface, DWORD *renderflags)
{
    struct quartz_vmr *This = impl_from_IVMRFilterConfig9(iface);

    FIXME("(%p/%p)->(%p) stub\n", iface, This, renderflags);
    return E_NOTIMPL;
}

static HRESULT WINAPI VMR9FilterConfig_SetRenderingMode(IVMRFilterConfig9 *iface, DWORD mode)
{
    HRESULT hr = S_OK;
    struct quartz_vmr *This = impl_from_IVMRFilterConfig9(iface);

    TRACE("(%p/%p)->(%u)\n", iface, This, mode);

    EnterCriticalSection(&This->renderer.filter.csFilter);
    if (This->mode)
    {
        LeaveCriticalSection(&This->renderer.filter.csFilter);
        return VFW_E_WRONG_STATE;
    }

    if (This->allocator)
        IVMRSurfaceAllocatorEx9_Release(This->allocator);
    if (This->presenter)
        IVMRImagePresenter9_Release(This->presenter);

    This->allocator = NULL;
    This->presenter = NULL;

    switch (mode)
    {
    case VMR9Mode_Windowed:
    case VMR9Mode_Windowless:
        This->allocator_is_ex = 0;
        This->cookie = ~0;

        hr = VMR9DefaultAllocatorPresenterImpl_create(This, (LPVOID*)&This->presenter);
        if (SUCCEEDED(hr))
            hr = IVMRImagePresenter9_QueryInterface(This->presenter, &IID_IVMRSurfaceAllocatorEx9, (LPVOID*)&This->allocator);
        if (FAILED(hr))
        {
            ERR("Unable to find Presenter interface\n");
            IVMRImagePresenter9_Release(This->presenter);
            This->allocator = NULL;
            This->presenter = NULL;
        }
        else
            hr = IVMRSurfaceAllocatorEx9_AdviseNotify(This->allocator, &This->IVMRSurfaceAllocatorNotify9_iface);
        break;
    case VMR9Mode_Renderless:
        break;
    default:
        LeaveCriticalSection(&This->renderer.filter.csFilter);
        return E_INVALIDARG;
    }

    This->mode = mode;
    LeaveCriticalSection(&This->renderer.filter.csFilter);
    return hr;
}

static HRESULT WINAPI VMR9FilterConfig_GetRenderingMode(IVMRFilterConfig9 *iface, DWORD *mode)
{
    struct quartz_vmr *This = impl_from_IVMRFilterConfig9(iface);

    TRACE("(%p/%p)->(%p)\n", iface, This, mode);
    if (!mode)
        return E_POINTER;

    if (This->mode)
        *mode = This->mode;
    else
        *mode = VMR9Mode_Windowed;

    return S_OK;
}

static const IVMRFilterConfig9Vtbl VMR9_FilterConfig_Vtbl =
{
    VMR9FilterConfig_QueryInterface,
    VMR9FilterConfig_AddRef,
    VMR9FilterConfig_Release,
    VMR9FilterConfig_SetImageCompositor,
    VMR9FilterConfig_SetNumberOfStreams,
    VMR9FilterConfig_GetNumberOfStreams,
    VMR9FilterConfig_SetRenderingPrefs,
    VMR9FilterConfig_GetRenderingPrefs,
    VMR9FilterConfig_SetRenderingMode,
    VMR9FilterConfig_GetRenderingMode
};

static HRESULT WINAPI VMR7WindowlessControl_QueryInterface(IVMRWindowlessControl *iface, REFIID riid,
                                                           LPVOID * ppv)
{
    struct quartz_vmr *This = impl_from_IVMRWindowlessControl(iface);
    return IUnknown_QueryInterface(This->renderer.filter.outer_unk, riid, ppv);
}

static ULONG WINAPI VMR7WindowlessControl_AddRef(IVMRWindowlessControl *iface)
{
    struct quartz_vmr *This = impl_from_IVMRWindowlessControl(iface);
    return IUnknown_AddRef(This->renderer.filter.outer_unk);
}

static ULONG WINAPI VMR7WindowlessControl_Release(IVMRWindowlessControl *iface)
{
    struct quartz_vmr *This = impl_from_IVMRWindowlessControl(iface);
    return IUnknown_Release(This->renderer.filter.outer_unk);
}

static HRESULT WINAPI VMR7WindowlessControl_GetNativeVideoSize(IVMRWindowlessControl *iface,
                                                               LONG *width, LONG *height,
                                                               LONG *arwidth, LONG *arheight)
{
    struct quartz_vmr *This = impl_from_IVMRWindowlessControl(iface);
    TRACE("(%p/%p)->(%p, %p, %p, %p)\n", iface, This, width, height, arwidth, arheight);

    if (!width || !height || !arwidth || !arheight)
    {
        ERR("Got no pointer\n");
        return E_POINTER;
    }

    *width = This->bmiheader.biWidth;
    *height = This->bmiheader.biHeight;
    *arwidth = This->bmiheader.biWidth;
    *arheight = This->bmiheader.biHeight;

    return S_OK;
}

static HRESULT WINAPI VMR7WindowlessControl_GetMinIdealVideoSize(IVMRWindowlessControl *iface,
                                                                 LONG *width, LONG *height)
{
    struct quartz_vmr *This = impl_from_IVMRWindowlessControl(iface);

    FIXME("(%p/%p)->(...) stub\n", iface, This);
    return E_NOTIMPL;
}

static HRESULT WINAPI VMR7WindowlessControl_GetMaxIdealVideoSize(IVMRWindowlessControl *iface,
                                                                 LONG *width, LONG *height)
{
    struct quartz_vmr *This = impl_from_IVMRWindowlessControl(iface);

    FIXME("(%p/%p)->(...) stub\n", iface, This);
    return E_NOTIMPL;
}

static HRESULT WINAPI VMR7WindowlessControl_SetVideoPosition(IVMRWindowlessControl *iface,
                                                             const RECT *source, const RECT *dest)
{
    struct quartz_vmr *This = impl_from_IVMRWindowlessControl(iface);

    TRACE("(%p/%p)->(%p, %p)\n", iface, This, source, dest);

    EnterCriticalSection(&This->renderer.filter.csFilter);

    if (source)
        This->source_rect = *source;
    if (dest)
    {
        This->target_rect = *dest;
        FIXME("Output rectangle: %s.\n", wine_dbgstr_rect(dest));
        SetWindowPos(This->baseControlWindow.baseWindow.hWnd, NULL,
                dest->left, dest->top, dest->right - dest->left, dest->bottom-dest->top,
                SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_NOOWNERZORDER | SWP_NOREDRAW);
    }

    LeaveCriticalSection(&This->renderer.filter.csFilter);

    return S_OK;
}

static HRESULT WINAPI VMR7WindowlessControl_GetVideoPosition(IVMRWindowlessControl *iface,
                                                             RECT *source, RECT *dest)
{
    struct quartz_vmr *This = impl_from_IVMRWindowlessControl(iface);

    if (source)
        *source = This->source_rect;

    if (dest)
        *dest = This->target_rect;

    FIXME("(%p/%p)->(%p/%p) stub\n", iface, This, source, dest);
    return S_OK;
}

static HRESULT WINAPI VMR7WindowlessControl_GetAspectRatioMode(IVMRWindowlessControl *iface,
                                                               DWORD *mode)
{
    struct quartz_vmr *This = impl_from_IVMRWindowlessControl(iface);

    FIXME("(%p/%p)->(...) stub\n", iface, This);
    return E_NOTIMPL;
}

static HRESULT WINAPI VMR7WindowlessControl_SetAspectRatioMode(IVMRWindowlessControl *iface,
                                                               DWORD mode)
{
    struct quartz_vmr *This = impl_from_IVMRWindowlessControl(iface);

    FIXME("(%p/%p)->(...) stub\n", iface, This);
    return E_NOTIMPL;
}

static HRESULT WINAPI VMR7WindowlessControl_SetVideoClippingWindow(IVMRWindowlessControl *iface, HWND window)
{
    struct quartz_vmr *filter = impl_from_IVMRWindowlessControl(iface);

    TRACE("iface %p, window %p.\n", iface, window);

    return IVMRWindowlessControl9_SetVideoClippingWindow(&filter->IVMRWindowlessControl9_iface, window);
}

static HRESULT WINAPI VMR7WindowlessControl_RepaintVideo(IVMRWindowlessControl *iface,
                                                         HWND hwnd, HDC hdc)
{
    struct quartz_vmr *This = impl_from_IVMRWindowlessControl(iface);

    FIXME("(%p/%p)->(...) stub\n", iface, This);
    return E_NOTIMPL;
}

static HRESULT WINAPI VMR7WindowlessControl_DisplayModeChanged(IVMRWindowlessControl *iface)
{
    struct quartz_vmr *This = impl_from_IVMRWindowlessControl(iface);

    FIXME("(%p/%p)->(...) stub\n", iface, This);
    return E_NOTIMPL;
}

static HRESULT WINAPI VMR7WindowlessControl_GetCurrentImage(IVMRWindowlessControl *iface,
                                                            BYTE **dib)
{
    struct quartz_vmr *This = impl_from_IVMRWindowlessControl(iface);

    FIXME("(%p/%p)->(...) stub\n", iface, This);
    return E_NOTIMPL;
}

static HRESULT WINAPI VMR7WindowlessControl_SetBorderColor(IVMRWindowlessControl *iface,
                                                           COLORREF color)
{
    struct quartz_vmr *This = impl_from_IVMRWindowlessControl(iface);

    FIXME("(%p/%p)->(...) stub\n", iface, This);
    return E_NOTIMPL;
}

static HRESULT WINAPI VMR7WindowlessControl_GetBorderColor(IVMRWindowlessControl *iface,
                                                           COLORREF *color)
{
    struct quartz_vmr *This = impl_from_IVMRWindowlessControl(iface);

    FIXME("(%p/%p)->(...) stub\n", iface, This);
    return E_NOTIMPL;
}

static HRESULT WINAPI VMR7WindowlessControl_SetColorKey(IVMRWindowlessControl *iface, COLORREF color)
{
    struct quartz_vmr *This = impl_from_IVMRWindowlessControl(iface);

    FIXME("(%p/%p)->(...) stub\n", iface, This);
    return E_NOTIMPL;
}

static HRESULT WINAPI VMR7WindowlessControl_GetColorKey(IVMRWindowlessControl *iface, COLORREF *color)
{
    struct quartz_vmr *This = impl_from_IVMRWindowlessControl(iface);

    FIXME("(%p/%p)->(...) stub\n", iface, This);
    return E_NOTIMPL;
}

static const IVMRWindowlessControlVtbl VMR7_WindowlessControl_Vtbl =
{
    VMR7WindowlessControl_QueryInterface,
    VMR7WindowlessControl_AddRef,
    VMR7WindowlessControl_Release,
    VMR7WindowlessControl_GetNativeVideoSize,
    VMR7WindowlessControl_GetMinIdealVideoSize,
    VMR7WindowlessControl_GetMaxIdealVideoSize,
    VMR7WindowlessControl_SetVideoPosition,
    VMR7WindowlessControl_GetVideoPosition,
    VMR7WindowlessControl_GetAspectRatioMode,
    VMR7WindowlessControl_SetAspectRatioMode,
    VMR7WindowlessControl_SetVideoClippingWindow,
    VMR7WindowlessControl_RepaintVideo,
    VMR7WindowlessControl_DisplayModeChanged,
    VMR7WindowlessControl_GetCurrentImage,
    VMR7WindowlessControl_SetBorderColor,
    VMR7WindowlessControl_GetBorderColor,
    VMR7WindowlessControl_SetColorKey,
    VMR7WindowlessControl_GetColorKey
};

static HRESULT WINAPI VMR9WindowlessControl_QueryInterface(IVMRWindowlessControl9 *iface, REFIID riid, LPVOID * ppv)
{
    struct quartz_vmr *This = impl_from_IVMRWindowlessControl9(iface);
    return IUnknown_QueryInterface(This->renderer.filter.outer_unk, riid, ppv);
}

static ULONG WINAPI VMR9WindowlessControl_AddRef(IVMRWindowlessControl9 *iface)
{
    struct quartz_vmr *This = impl_from_IVMRWindowlessControl9(iface);
    return IUnknown_AddRef(This->renderer.filter.outer_unk);
}

static ULONG WINAPI VMR9WindowlessControl_Release(IVMRWindowlessControl9 *iface)
{
    struct quartz_vmr *This = impl_from_IVMRWindowlessControl9(iface);
    return IUnknown_Release(This->renderer.filter.outer_unk);
}

static HRESULT WINAPI VMR9WindowlessControl_GetNativeVideoSize(IVMRWindowlessControl9 *iface, LONG *width, LONG *height, LONG *arwidth, LONG *arheight)
{
    struct quartz_vmr *This = impl_from_IVMRWindowlessControl9(iface);
    TRACE("(%p/%p)->(%p, %p, %p, %p)\n", iface, This, width, height, arwidth, arheight);

    if (!width || !height || !arwidth || !arheight)
    {
        ERR("Got no pointer\n");
        return E_POINTER;
    }

    *width = This->bmiheader.biWidth;
    *height = This->bmiheader.biHeight;
    *arwidth = This->bmiheader.biWidth;
    *arheight = This->bmiheader.biHeight;

    return S_OK;
}

static HRESULT WINAPI VMR9WindowlessControl_GetMinIdealVideoSize(IVMRWindowlessControl9 *iface, LONG *width, LONG *height)
{
    struct quartz_vmr *This = impl_from_IVMRWindowlessControl9(iface);

    FIXME("(%p/%p)->(...) stub\n", iface, This);
    return E_NOTIMPL;
}

static HRESULT WINAPI VMR9WindowlessControl_GetMaxIdealVideoSize(IVMRWindowlessControl9 *iface, LONG *width, LONG *height)
{
    struct quartz_vmr *This = impl_from_IVMRWindowlessControl9(iface);

    FIXME("(%p/%p)->(...) stub\n", iface, This);
    return E_NOTIMPL;
}

static HRESULT WINAPI VMR9WindowlessControl_SetVideoPosition(IVMRWindowlessControl9 *iface, const RECT *source, const RECT *dest)
{
    struct quartz_vmr *This = impl_from_IVMRWindowlessControl9(iface);

    TRACE("(%p/%p)->(%p, %p)\n", iface, This, source, dest);

    EnterCriticalSection(&This->renderer.filter.csFilter);

    if (source)
        This->source_rect = *source;
    if (dest)
    {
        This->target_rect = *dest;
        FIXME("Output rectangle: %s.\n", wine_dbgstr_rect(dest));
        SetWindowPos(This->baseControlWindow.baseWindow.hWnd, NULL,
                dest->left, dest->top, dest->right - dest->left, dest->bottom - dest->top,
                SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_NOOWNERZORDER | SWP_NOREDRAW);
    }

    LeaveCriticalSection(&This->renderer.filter.csFilter);

    return S_OK;
}

static HRESULT WINAPI VMR9WindowlessControl_GetVideoPosition(IVMRWindowlessControl9 *iface, RECT *source, RECT *dest)
{
    struct quartz_vmr *This = impl_from_IVMRWindowlessControl9(iface);

    if (source)
        *source = This->source_rect;

    if (dest)
        *dest = This->target_rect;

    FIXME("(%p/%p)->(%p/%p) stub\n", iface, This, source, dest);
    return S_OK;
}

static HRESULT WINAPI VMR9WindowlessControl_GetAspectRatioMode(IVMRWindowlessControl9 *iface, DWORD *mode)
{
    struct quartz_vmr *This = impl_from_IVMRWindowlessControl9(iface);

    FIXME("(%p/%p)->(...) stub\n", iface, This);
    return E_NOTIMPL;
}

static HRESULT WINAPI VMR9WindowlessControl_SetAspectRatioMode(IVMRWindowlessControl9 *iface, DWORD mode)
{
    struct quartz_vmr *This = impl_from_IVMRWindowlessControl9(iface);

    FIXME("(%p/%p)->(...) stub\n", iface, This);
    return E_NOTIMPL;
}

static HRESULT WINAPI VMR9WindowlessControl_SetVideoClippingWindow(IVMRWindowlessControl9 *iface, HWND hwnd)
{
    struct quartz_vmr *This = impl_from_IVMRWindowlessControl9(iface);

    TRACE("(%p/%p)->(%p)\n", iface, This, hwnd);

    EnterCriticalSection(&This->renderer.filter.csFilter);
    This->hWndClippingWindow = hwnd;
    if (This->renderer.sink.pin.peer)
        VMR9_maybe_init(This, FALSE, &This->renderer.sink.pin.mt);
    if (!hwnd)
        IVMRSurfaceAllocatorEx9_TerminateDevice(This->allocator, This->cookie);
    LeaveCriticalSection(&This->renderer.filter.csFilter);
    return S_OK;
}

static HRESULT WINAPI VMR9WindowlessControl_RepaintVideo(IVMRWindowlessControl9 *iface, HWND hwnd, HDC hdc)
{
    struct quartz_vmr *This = impl_from_IVMRWindowlessControl9(iface);
    HRESULT hr;

    FIXME("(%p/%p)->(...) semi-stub\n", iface, This);

    EnterCriticalSection(&This->renderer.filter.csFilter);
    if (hwnd != This->hWndClippingWindow && hwnd != This->baseControlWindow.baseWindow.hWnd)
    {
        ERR("Not handling changing windows yet!!!\n");
        LeaveCriticalSection(&This->renderer.filter.csFilter);
        return S_OK;
    }

    if (!This->allocator_d3d9_dev)
    {
        ERR("No d3d9 device!\n");
        LeaveCriticalSection(&This->renderer.filter.csFilter);
        return VFW_E_WRONG_STATE;
    }

    /* Windowless extension */
    hr = IDirect3DDevice9_Present(This->allocator_d3d9_dev, NULL, NULL, This->baseControlWindow.baseWindow.hWnd, NULL);
    LeaveCriticalSection(&This->renderer.filter.csFilter);

    return hr;
}

static HRESULT WINAPI VMR9WindowlessControl_DisplayModeChanged(IVMRWindowlessControl9 *iface)
{
    struct quartz_vmr *This = impl_from_IVMRWindowlessControl9(iface);

    FIXME("(%p/%p)->(...) stub\n", iface, This);
    return E_NOTIMPL;
}

static HRESULT WINAPI VMR9WindowlessControl_GetCurrentImage(IVMRWindowlessControl9 *iface, BYTE **dib)
{
    struct quartz_vmr *This = impl_from_IVMRWindowlessControl9(iface);

    FIXME("(%p/%p)->(...) stub\n", iface, This);
    return E_NOTIMPL;
}

static HRESULT WINAPI VMR9WindowlessControl_SetBorderColor(IVMRWindowlessControl9 *iface, COLORREF color)
{
    struct quartz_vmr *This = impl_from_IVMRWindowlessControl9(iface);

    FIXME("(%p/%p)->(...) stub\n", iface, This);
    return E_NOTIMPL;
}

static HRESULT WINAPI VMR9WindowlessControl_GetBorderColor(IVMRWindowlessControl9 *iface, COLORREF *color)
{
    struct quartz_vmr *This = impl_from_IVMRWindowlessControl9(iface);

    FIXME("(%p/%p)->(...) stub\n", iface, This);
    return E_NOTIMPL;
}

static const IVMRWindowlessControl9Vtbl VMR9_WindowlessControl_Vtbl =
{
    VMR9WindowlessControl_QueryInterface,
    VMR9WindowlessControl_AddRef,
    VMR9WindowlessControl_Release,
    VMR9WindowlessControl_GetNativeVideoSize,
    VMR9WindowlessControl_GetMinIdealVideoSize,
    VMR9WindowlessControl_GetMaxIdealVideoSize,
    VMR9WindowlessControl_SetVideoPosition,
    VMR9WindowlessControl_GetVideoPosition,
    VMR9WindowlessControl_GetAspectRatioMode,
    VMR9WindowlessControl_SetAspectRatioMode,
    VMR9WindowlessControl_SetVideoClippingWindow,
    VMR9WindowlessControl_RepaintVideo,
    VMR9WindowlessControl_DisplayModeChanged,
    VMR9WindowlessControl_GetCurrentImage,
    VMR9WindowlessControl_SetBorderColor,
    VMR9WindowlessControl_GetBorderColor
};

static HRESULT WINAPI VMR7SurfaceAllocatorNotify_QueryInterface(IVMRSurfaceAllocatorNotify *iface,
                                                                REFIID riid, LPVOID * ppv)
{
    struct quartz_vmr *This = impl_from_IVMRSurfaceAllocatorNotify(iface);
    return IUnknown_QueryInterface(This->renderer.filter.outer_unk, riid, ppv);
}

static ULONG WINAPI VMR7SurfaceAllocatorNotify_AddRef(IVMRSurfaceAllocatorNotify *iface)
{
    struct quartz_vmr *This = impl_from_IVMRSurfaceAllocatorNotify(iface);
    return IUnknown_AddRef(This->renderer.filter.outer_unk);
}

static ULONG WINAPI VMR7SurfaceAllocatorNotify_Release(IVMRSurfaceAllocatorNotify *iface)
{
    struct quartz_vmr *This = impl_from_IVMRSurfaceAllocatorNotify(iface);
    return IUnknown_Release(This->renderer.filter.outer_unk);
}

static HRESULT WINAPI VMR7SurfaceAllocatorNotify_AdviseSurfaceAllocator(IVMRSurfaceAllocatorNotify *iface,
                                                                        DWORD_PTR id,
                                                                        IVMRSurfaceAllocator *alloc)
{
    struct quartz_vmr *This = impl_from_IVMRSurfaceAllocatorNotify(iface);

    FIXME("(%p/%p)->(...) stub\n", iface, This);
    return E_NOTIMPL;
}

static HRESULT WINAPI VMR7SurfaceAllocatorNotify_SetDDrawDevice(IVMRSurfaceAllocatorNotify *iface,
                                                                IDirectDraw7 *device, HMONITOR monitor)
{
    struct quartz_vmr *This = impl_from_IVMRSurfaceAllocatorNotify(iface);

    FIXME("(%p/%p)->(...) stub\n", iface, This);
    return E_NOTIMPL;
}

static HRESULT WINAPI VMR7SurfaceAllocatorNotify_ChangeDDrawDevice(IVMRSurfaceAllocatorNotify *iface,
                                                                   IDirectDraw7 *device, HMONITOR monitor)
{
    struct quartz_vmr *This = impl_from_IVMRSurfaceAllocatorNotify(iface);

    FIXME("(%p/%p)->(...) stub\n", iface, This);
    return E_NOTIMPL;
}

static HRESULT WINAPI VMR7SurfaceAllocatorNotify_RestoreDDrawSurfaces(IVMRSurfaceAllocatorNotify *iface)
{
    struct quartz_vmr *This = impl_from_IVMRSurfaceAllocatorNotify(iface);

    FIXME("(%p/%p)->(...) stub\n", iface, This);
    return E_NOTIMPL;
}

static HRESULT WINAPI VMR7SurfaceAllocatorNotify_NotifyEvent(IVMRSurfaceAllocatorNotify *iface, LONG code,
                                                             LONG_PTR param1, LONG_PTR param2)
{
    struct quartz_vmr *This = impl_from_IVMRSurfaceAllocatorNotify(iface);

    FIXME("(%p/%p)->(...) stub\n", iface, This);
    return E_NOTIMPL;
}

static HRESULT WINAPI VMR7SurfaceAllocatorNotify_SetBorderColor(IVMRSurfaceAllocatorNotify *iface,
                                                                COLORREF clrBorder)
{
    struct quartz_vmr *This = impl_from_IVMRSurfaceAllocatorNotify(iface);

    FIXME("(%p/%p)->(...) stub\n", iface, This);
    return E_NOTIMPL;
}

static const IVMRSurfaceAllocatorNotifyVtbl VMR7_SurfaceAllocatorNotify_Vtbl =
{
    VMR7SurfaceAllocatorNotify_QueryInterface,
    VMR7SurfaceAllocatorNotify_AddRef,
    VMR7SurfaceAllocatorNotify_Release,
    VMR7SurfaceAllocatorNotify_AdviseSurfaceAllocator,
    VMR7SurfaceAllocatorNotify_SetDDrawDevice,
    VMR7SurfaceAllocatorNotify_ChangeDDrawDevice,
    VMR7SurfaceAllocatorNotify_RestoreDDrawSurfaces,
    VMR7SurfaceAllocatorNotify_NotifyEvent,
    VMR7SurfaceAllocatorNotify_SetBorderColor
};

static HRESULT WINAPI VMR9SurfaceAllocatorNotify_QueryInterface(IVMRSurfaceAllocatorNotify9 *iface, REFIID riid, LPVOID * ppv)
{
    struct quartz_vmr *This = impl_from_IVMRSurfaceAllocatorNotify9(iface);
    return IUnknown_QueryInterface(This->renderer.filter.outer_unk, riid, ppv);
}

static ULONG WINAPI VMR9SurfaceAllocatorNotify_AddRef(IVMRSurfaceAllocatorNotify9 *iface)
{
    struct quartz_vmr *This = impl_from_IVMRSurfaceAllocatorNotify9(iface);
    return IUnknown_AddRef(This->renderer.filter.outer_unk);
}

static ULONG WINAPI VMR9SurfaceAllocatorNotify_Release(IVMRSurfaceAllocatorNotify9 *iface)
{
    struct quartz_vmr *This = impl_from_IVMRSurfaceAllocatorNotify9(iface);
    return IUnknown_Release(This->renderer.filter.outer_unk);
}

static HRESULT WINAPI VMR9SurfaceAllocatorNotify_AdviseSurfaceAllocator(IVMRSurfaceAllocatorNotify9 *iface, DWORD_PTR id, IVMRSurfaceAllocator9 *alloc)
{
    struct quartz_vmr *This = impl_from_IVMRSurfaceAllocatorNotify9(iface);

    /* FIXME: This code is not tested!!! */
    FIXME("(%p/%p)->(...) stub\n", iface, This);
    This->cookie = id;

    if (This->presenter)
        return VFW_E_WRONG_STATE;

    if (FAILED(IVMRSurfaceAllocator9_QueryInterface(alloc, &IID_IVMRImagePresenter9, (void **)&This->presenter)))
        return E_NOINTERFACE;

    if (SUCCEEDED(IVMRSurfaceAllocator9_QueryInterface(alloc, &IID_IVMRSurfaceAllocatorEx9, (void **)&This->allocator)))
        This->allocator_is_ex = 1;
    else
    {
        This->allocator = (IVMRSurfaceAllocatorEx9 *)alloc;
        IVMRSurfaceAllocator9_AddRef(alloc);
        This->allocator_is_ex = 0;
    }

    return S_OK;
}

static HRESULT WINAPI VMR9SurfaceAllocatorNotify_SetD3DDevice(IVMRSurfaceAllocatorNotify9 *iface, IDirect3DDevice9 *device, HMONITOR monitor)
{
    struct quartz_vmr *This = impl_from_IVMRSurfaceAllocatorNotify9(iface);

    FIXME("(%p/%p)->(...) semi-stub\n", iface, This);
    if (This->allocator_d3d9_dev)
        IDirect3DDevice9_Release(This->allocator_d3d9_dev);
    This->allocator_d3d9_dev = device;
    IDirect3DDevice9_AddRef(This->allocator_d3d9_dev);
    This->allocator_mon = monitor;

    return S_OK;
}

static HRESULT WINAPI VMR9SurfaceAllocatorNotify_ChangeD3DDevice(IVMRSurfaceAllocatorNotify9 *iface, IDirect3DDevice9 *device, HMONITOR monitor)
{
    struct quartz_vmr *This = impl_from_IVMRSurfaceAllocatorNotify9(iface);

    FIXME("(%p/%p)->(...) semi-stub\n", iface, This);
    if (This->allocator_d3d9_dev)
        IDirect3DDevice9_Release(This->allocator_d3d9_dev);
    This->allocator_d3d9_dev = device;
    IDirect3DDevice9_AddRef(This->allocator_d3d9_dev);
    This->allocator_mon = monitor;

    return S_OK;
}

static HRESULT WINAPI VMR9SurfaceAllocatorNotify_AllocateSurfaceHelper(IVMRSurfaceAllocatorNotify9 *iface, VMR9AllocationInfo *allocinfo, DWORD *numbuffers, IDirect3DSurface9 **surface)
{
    struct quartz_vmr *This = impl_from_IVMRSurfaceAllocatorNotify9(iface);
    DWORD i;
    HRESULT hr = S_OK;

    FIXME("(%p/%p)->(%p, %p => %u, %p) semi-stub\n", iface, This, allocinfo, numbuffers, (numbuffers ? *numbuffers : 0), surface);

    if (!allocinfo || !numbuffers || !surface)
        return E_POINTER;

    if (!*numbuffers || *numbuffers < allocinfo->MinBuffers)
    {
        ERR("Invalid number of buffers?\n");
        return E_INVALIDARG;
    }

    if (!This->allocator_d3d9_dev)
    {
        ERR("No direct3d device when requested to allocate a surface!\n");
        return VFW_E_WRONG_STATE;
    }

    if (allocinfo->dwFlags & VMR9AllocFlag_OffscreenSurface)
    {
        ERR("Creating offscreen surface\n");
        for (i = 0; i < *numbuffers; ++i)
        {
            hr = IDirect3DDevice9_CreateOffscreenPlainSurface(This->allocator_d3d9_dev,  allocinfo->dwWidth, allocinfo->dwHeight,
                                                             allocinfo->Format, allocinfo->Pool, &surface[i], NULL);
            if (FAILED(hr))
                break;
        }
    }
    else if (allocinfo->dwFlags & VMR9AllocFlag_TextureSurface)
    {
        TRACE("Creating texture surface\n");
        for (i = 0; i < *numbuffers; ++i)
        {
            IDirect3DTexture9 *texture;

            hr = IDirect3DDevice9_CreateTexture(This->allocator_d3d9_dev, allocinfo->dwWidth, allocinfo->dwHeight, 1, D3DUSAGE_DYNAMIC,
                                                allocinfo->Format, allocinfo->Pool, &texture, NULL);
            if (FAILED(hr))
                break;
            IDirect3DTexture9_GetSurfaceLevel(texture, 0, &surface[i]);
            IDirect3DTexture9_Release(texture);
        }
    }
    else
    {
         FIXME("Could not allocate for type %08x\n", allocinfo->dwFlags);
         return E_NOTIMPL;
    }

    if (i >= allocinfo->MinBuffers)
    {
        hr = S_OK;
        *numbuffers = i;
    }
    else
    {
        for ( ; i > 0; --i) IDirect3DSurface9_Release(surface[i - 1]);
        *numbuffers = 0;
    }
    return hr;
}

static HRESULT WINAPI VMR9SurfaceAllocatorNotify_NotifyEvent(IVMRSurfaceAllocatorNotify9 *iface, LONG code, LONG_PTR param1, LONG_PTR param2)
{
    struct quartz_vmr *This = impl_from_IVMRSurfaceAllocatorNotify9(iface);

    FIXME("(%p/%p)->(...) stub\n", iface, This);
    return E_NOTIMPL;
}

static const IVMRSurfaceAllocatorNotify9Vtbl VMR9_SurfaceAllocatorNotify_Vtbl =
{
    VMR9SurfaceAllocatorNotify_QueryInterface,
    VMR9SurfaceAllocatorNotify_AddRef,
    VMR9SurfaceAllocatorNotify_Release,
    VMR9SurfaceAllocatorNotify_AdviseSurfaceAllocator,
    VMR9SurfaceAllocatorNotify_SetD3DDevice,
    VMR9SurfaceAllocatorNotify_ChangeD3DDevice,
    VMR9SurfaceAllocatorNotify_AllocateSurfaceHelper,
    VMR9SurfaceAllocatorNotify_NotifyEvent
};

static inline struct quartz_vmr *impl_from_IOverlay(IOverlay *iface)
{
    return CONTAINING_RECORD(iface, struct quartz_vmr, IOverlay_iface);
}

static HRESULT WINAPI overlay_QueryInterface(IOverlay *iface, REFIID iid, void **out)
{
    struct quartz_vmr *filter = impl_from_IOverlay(iface);
    return IPin_QueryInterface(&filter->renderer.sink.pin.IPin_iface, iid, out);
}

static ULONG WINAPI overlay_AddRef(IOverlay *iface)
{
    struct quartz_vmr *filter = impl_from_IOverlay(iface);
    return IPin_AddRef(&filter->renderer.sink.pin.IPin_iface);
}

static ULONG WINAPI overlay_Release(IOverlay *iface)
{
    struct quartz_vmr *filter = impl_from_IOverlay(iface);
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
    struct quartz_vmr *filter = impl_from_IOverlay(iface);

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

static HRESULT vmr_create(IUnknown *outer, IUnknown **out, const CLSID *clsid)
{
    HRESULT hr;
    struct quartz_vmr* pVMR;

    *out = NULL;

    pVMR = CoTaskMemAlloc(sizeof(struct quartz_vmr));

    pVMR->hD3d9 = LoadLibraryA("d3d9.dll");
    if (!pVMR->hD3d9 )
    {
        WARN("Could not load d3d9.dll\n");
        CoTaskMemFree(pVMR);
        return VFW_E_DDRAW_CAPS_NOT_SUITABLE;
    }

    pVMR->IAMCertifiedOutputProtection_iface.lpVtbl = &IAMCertifiedOutputProtection_Vtbl;
    pVMR->IAMFilterMiscFlags_iface.lpVtbl = &IAMFilterMiscFlags_Vtbl;

    pVMR->mode = 0;
    pVMR->allocator_d3d9_dev = NULL;
    pVMR->allocator_mon= NULL;
    pVMR->num_surfaces = pVMR->cur_surface = 0;
    pVMR->allocator = NULL;
    pVMR->presenter = NULL;
    pVMR->hWndClippingWindow = NULL;
    pVMR->IVMRFilterConfig_iface.lpVtbl = &VMR7_FilterConfig_Vtbl;
    pVMR->IVMRFilterConfig9_iface.lpVtbl = &VMR9_FilterConfig_Vtbl;
    pVMR->IVMRMonitorConfig_iface.lpVtbl = &VMR7_MonitorConfig_Vtbl;
    pVMR->IVMRMonitorConfig9_iface.lpVtbl = &VMR9_MonitorConfig_Vtbl;
    pVMR->IVMRSurfaceAllocatorNotify_iface.lpVtbl = &VMR7_SurfaceAllocatorNotify_Vtbl;
    pVMR->IVMRSurfaceAllocatorNotify9_iface.lpVtbl = &VMR9_SurfaceAllocatorNotify_Vtbl;
    pVMR->IVMRWindowlessControl_iface.lpVtbl = &VMR7_WindowlessControl_Vtbl;
    pVMR->IVMRWindowlessControl9_iface.lpVtbl = &VMR9_WindowlessControl_Vtbl;
    pVMR->IOverlay_iface.lpVtbl = &overlay_vtbl;

    hr = strmbase_renderer_init(&pVMR->renderer, outer, clsid, L"VMR Input0", &renderer_ops);
    if (FAILED(hr))
        goto fail;

    hr = video_window_init(&pVMR->baseControlWindow, &IVideoWindow_VTable,
            &pVMR->renderer.filter, &pVMR->renderer.sink.pin, &renderer_BaseWindowFuncTable);
    if (FAILED(hr))
        goto fail;

    if (FAILED(hr = BaseWindowImpl_PrepareWindow(&pVMR->baseControlWindow.baseWindow)))
        goto fail;

    hr = basic_video_init(&pVMR->baseControlVideo, &pVMR->renderer.filter,
            &pVMR->renderer.sink.pin, &renderer_BaseControlVideoFuncTable);
    if (FAILED(hr))
        goto fail;

    pVMR->run_event = CreateEventW(NULL, TRUE, FALSE, NULL);

    *out = &pVMR->renderer.filter.IUnknown_inner;
    ZeroMemory(&pVMR->source_rect, sizeof(RECT));
    ZeroMemory(&pVMR->target_rect, sizeof(RECT));
    TRACE("Created at %p\n", pVMR);
    return hr;

fail:
    BaseWindowImpl_DoneWithWindow(&pVMR->baseControlWindow.baseWindow);
    strmbase_renderer_cleanup(&pVMR->renderer);
    FreeLibrary(pVMR->hD3d9);
    CoTaskMemFree(pVMR);
    return hr;
}

HRESULT vmr7_create(IUnknown *outer, IUnknown **out)
{
    return vmr_create(outer, out, &CLSID_VideoMixingRenderer);
}

HRESULT vmr9_create(IUnknown *outer, IUnknown **out)
{
    return vmr_create(outer, out, &CLSID_VideoMixingRenderer9);
}


static HRESULT WINAPI VMR9_ImagePresenter_QueryInterface(IVMRImagePresenter9 *iface, REFIID riid, void **ppv)
{
    VMR9DefaultAllocatorPresenterImpl *This = impl_from_IVMRImagePresenter9(iface);
    TRACE("(%p/%p)->(%s, %p)\n", This, iface, qzdebugstr_guid(riid), ppv);

    *ppv = NULL;

    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IVMRImagePresenter9))
        *ppv = &This->IVMRImagePresenter9_iface;
    else if (IsEqualIID(riid, &IID_IVMRSurfaceAllocatorEx9))
        *ppv = &This->IVMRSurfaceAllocatorEx9_iface;

    if (*ppv)
    {
        IUnknown_AddRef((IUnknown *)(*ppv));
        return S_OK;
    }

    FIXME("No interface for %s\n", debugstr_guid(riid));

    return E_NOINTERFACE;
}

static ULONG WINAPI VMR9_ImagePresenter_AddRef(IVMRImagePresenter9 *iface)
{
    VMR9DefaultAllocatorPresenterImpl *This = impl_from_IVMRImagePresenter9(iface);
    ULONG refCount = InterlockedIncrement(&This->refCount);

    TRACE("(%p)->() AddRef from %d\n", iface, refCount - 1);

    return refCount;
}

static ULONG WINAPI VMR9_ImagePresenter_Release(IVMRImagePresenter9 *iface)
{
    VMR9DefaultAllocatorPresenterImpl *This = impl_from_IVMRImagePresenter9(iface);
    ULONG refCount = InterlockedDecrement(&This->refCount);

    TRACE("(%p)->() Release from %d\n", iface, refCount + 1);

    if (!refCount)
    {
        DWORD i;
        TRACE("Destroying\n");
        IDirect3D9_Release(This->d3d9_ptr);

        TRACE("Number of surfaces: %u\n", This->num_surfaces);
        for (i = 0; i < This->num_surfaces; ++i)
        {
            IDirect3DSurface9 *surface = This->d3d9_surfaces[i];
            TRACE("Releasing surface %p\n", surface);
            if (surface)
                IDirect3DSurface9_Release(surface);
        }

        CoTaskMemFree(This->d3d9_surfaces);
        This->d3d9_surfaces = NULL;
        This->num_surfaces = 0;
        if (This->d3d9_vertex)
        {
            IDirect3DVertexBuffer9_Release(This->d3d9_vertex);
            This->d3d9_vertex = NULL;
        }
        CoTaskMemFree(This);
        return 0;
    }
    return refCount;
}

static HRESULT WINAPI VMR9_ImagePresenter_StartPresenting(IVMRImagePresenter9 *iface, DWORD_PTR id)
{
    VMR9DefaultAllocatorPresenterImpl *This = impl_from_IVMRImagePresenter9(iface);

    TRACE("(%p/%p/%p)->(...) stub\n", iface, This,This->pVMR9);
    return S_OK;
}

static HRESULT WINAPI VMR9_ImagePresenter_StopPresenting(IVMRImagePresenter9 *iface, DWORD_PTR id)
{
    VMR9DefaultAllocatorPresenterImpl *This = impl_from_IVMRImagePresenter9(iface);

    TRACE("(%p/%p/%p)->(...) stub\n", iface, This,This->pVMR9);
    return S_OK;
}

#define USED_FVF (D3DFVF_XYZRHW | D3DFVF_TEX1)
struct VERTEX { float x, y, z, rhw, u, v; };

static HRESULT VMR9_ImagePresenter_PresentTexture(VMR9DefaultAllocatorPresenterImpl *This, IDirect3DSurface9 *surface)
{
    IDirect3DTexture9 *texture = NULL;
    HRESULT hr;

    hr = IDirect3DDevice9_SetFVF(This->d3d9_dev, USED_FVF);
    if (FAILED(hr))
    {
        FIXME("SetFVF: %08x\n", hr);
        return hr;
    }

    hr = IDirect3DDevice9_SetStreamSource(This->d3d9_dev, 0, This->d3d9_vertex, 0, sizeof(struct VERTEX));
    if (FAILED(hr))
    {
        FIXME("SetStreamSource: %08x\n", hr);
        return hr;
    }

    hr = IDirect3DSurface9_GetContainer(surface, &IID_IDirect3DTexture9, (void **) &texture);
    if (FAILED(hr))
    {
        FIXME("IDirect3DSurface9_GetContainer failed\n");
        return hr;
    }
    hr = IDirect3DDevice9_SetTexture(This->d3d9_dev, 0, (IDirect3DBaseTexture9 *)texture);
    IDirect3DTexture9_Release(texture);
    if (FAILED(hr))
    {
        FIXME("SetTexture: %08x\n", hr);
        return hr;
    }

    hr = IDirect3DDevice9_DrawPrimitive(This->d3d9_dev, D3DPT_TRIANGLESTRIP, 0, 2);
    if (FAILED(hr))
    {
        FIXME("DrawPrimitive: %08x\n", hr);
        return hr;
    }

    return S_OK;
}

static HRESULT VMR9_ImagePresenter_PresentOffscreenSurface(VMR9DefaultAllocatorPresenterImpl *This, IDirect3DSurface9 *surface)
{
    HRESULT hr;
    IDirect3DSurface9 *target = NULL;
    RECT target_rect;

    hr = IDirect3DDevice9_GetBackBuffer(This->d3d9_dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &target);
    if (FAILED(hr))
    {
        ERR("IDirect3DDevice9_GetBackBuffer -- %08x\n", hr);
        return hr;
    }

    /* Move rect to origin and flip it */
    SetRect(&target_rect, 0, This->pVMR9->target_rect.bottom - This->pVMR9->target_rect.top,
            This->pVMR9->target_rect.right - This->pVMR9->target_rect.left, 0);

    hr = IDirect3DDevice9_StretchRect(This->d3d9_dev, surface, &This->pVMR9->source_rect, target, &target_rect, D3DTEXF_LINEAR);
    if (FAILED(hr))
        ERR("IDirect3DDevice9_StretchRect -- %08x\n", hr);
    IDirect3DSurface9_Release(target);

    return hr;
}

static HRESULT WINAPI VMR9_ImagePresenter_PresentImage(IVMRImagePresenter9 *iface, DWORD_PTR id, VMR9PresentationInfo *info)
{
    VMR9DefaultAllocatorPresenterImpl *This = impl_from_IVMRImagePresenter9(iface);
    HRESULT hr;
    RECT output;
    BOOL render = FALSE;

    TRACE("(%p/%p/%p)->(...) stub\n", iface, This, This->pVMR9);
    GetWindowRect(This->pVMR9->baseControlWindow.baseWindow.hWnd, &output);
    TRACE("Output rectangle: %s\n", wine_dbgstr_rect(&output));

    /* This might happen if we don't have active focus (eg on a different virtual desktop) */
    if (!This->d3d9_dev)
        return S_OK;

    /* Display image here */
    hr = IDirect3DDevice9_Clear(This->d3d9_dev, 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
    if (FAILED(hr))
        FIXME("hr: %08x\n", hr);
    hr = IDirect3DDevice9_BeginScene(This->d3d9_dev);
    if (SUCCEEDED(hr))
    {
        if (This->d3d9_vertex)
            hr = VMR9_ImagePresenter_PresentTexture(This, info->lpSurf);
        else
            hr = VMR9_ImagePresenter_PresentOffscreenSurface(This, info->lpSurf);
        render = SUCCEEDED(hr);
    }
    else
        FIXME("BeginScene: %08x\n", hr);
    hr = IDirect3DDevice9_EndScene(This->d3d9_dev);
    if (render && SUCCEEDED(hr))
    {
        hr = IDirect3DDevice9_Present(This->d3d9_dev, NULL, NULL, This->pVMR9->baseControlWindow.baseWindow.hWnd, NULL);
        if (FAILED(hr))
            FIXME("Presenting image: %08x\n", hr);
    }

    return S_OK;
}

static const IVMRImagePresenter9Vtbl VMR9_ImagePresenter =
{
    VMR9_ImagePresenter_QueryInterface,
    VMR9_ImagePresenter_AddRef,
    VMR9_ImagePresenter_Release,
    VMR9_ImagePresenter_StartPresenting,
    VMR9_ImagePresenter_StopPresenting,
    VMR9_ImagePresenter_PresentImage
};

static HRESULT WINAPI VMR9_SurfaceAllocator_QueryInterface(IVMRSurfaceAllocatorEx9 *iface, REFIID riid, LPVOID * ppv)
{
    VMR9DefaultAllocatorPresenterImpl *This = impl_from_IVMRSurfaceAllocatorEx9(iface);

    return VMR9_ImagePresenter_QueryInterface(&This->IVMRImagePresenter9_iface, riid, ppv);
}

static ULONG WINAPI VMR9_SurfaceAllocator_AddRef(IVMRSurfaceAllocatorEx9 *iface)
{
    VMR9DefaultAllocatorPresenterImpl *This = impl_from_IVMRSurfaceAllocatorEx9(iface);

    return VMR9_ImagePresenter_AddRef(&This->IVMRImagePresenter9_iface);
}

static ULONG WINAPI VMR9_SurfaceAllocator_Release(IVMRSurfaceAllocatorEx9 *iface)
{
    VMR9DefaultAllocatorPresenterImpl *This = impl_from_IVMRSurfaceAllocatorEx9(iface);

    return VMR9_ImagePresenter_Release(&This->IVMRImagePresenter9_iface);
}

static HRESULT VMR9_SurfaceAllocator_SetAllocationSettings(VMR9DefaultAllocatorPresenterImpl *This, VMR9AllocationInfo *allocinfo)
{
    D3DCAPS9 caps;
    UINT width, height;
    HRESULT hr;

    if (!(allocinfo->dwFlags & VMR9AllocFlag_TextureSurface))
        /* Only needed for texture surfaces */
        return S_OK;

    hr = IDirect3DDevice9_GetDeviceCaps(This->d3d9_dev, &caps);
    if (FAILED(hr))
        return hr;

    if (!(caps.TextureCaps & D3DPTEXTURECAPS_POW2) || (caps.TextureCaps & D3DPTEXTURECAPS_SQUAREONLY))
    {
        width = allocinfo->dwWidth;
        height = allocinfo->dwHeight;
    }
    else
    {
        width = height = 1;
        while (width < allocinfo->dwWidth)
            width *= 2;

        while (height < allocinfo->dwHeight)
            height *= 2;
        FIXME("NPOW2 support missing, not using proper surfaces!\n");
    }

    if (caps.TextureCaps & D3DPTEXTURECAPS_SQUAREONLY)
    {
        if (height > width)
            width = height;
        else
            height = width;
        FIXME("Square texture support required..\n");
    }

    hr = IDirect3DDevice9_CreateVertexBuffer(This->d3d9_dev, 4 * sizeof(struct VERTEX), D3DUSAGE_WRITEONLY, USED_FVF, allocinfo->Pool, &This->d3d9_vertex, NULL);
    if (FAILED(hr))
    {
        ERR("Couldn't create vertex buffer: %08x\n", hr);
        return hr;
    }

    This->reset = TRUE;
    allocinfo->dwHeight = height;
    allocinfo->dwWidth = width;

    return hr;
}

static UINT d3d9_adapter_from_hwnd(IDirect3D9 *d3d9, HWND hwnd, HMONITOR *mon_out)
{
    UINT d3d9_adapter;
    HMONITOR mon;

    mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONULL);
    if (!mon)
        d3d9_adapter = 0;
    else
    {
        for (d3d9_adapter = 0; d3d9_adapter < IDirect3D9_GetAdapterCount(d3d9); ++d3d9_adapter)
        {
            if (mon == IDirect3D9_GetAdapterMonitor(d3d9, d3d9_adapter))
                break;
        }
        if (d3d9_adapter >= IDirect3D9_GetAdapterCount(d3d9))
            d3d9_adapter = 0;
    }
    if (mon_out)
        *mon_out = mon;
    return d3d9_adapter;
}

static BOOL CreateRenderingWindow(VMR9DefaultAllocatorPresenterImpl *This, VMR9AllocationInfo *info, DWORD *numbuffers)
{
    D3DPRESENT_PARAMETERS d3dpp;
    DWORD d3d9_adapter;
    HRESULT hr;

    TRACE("(%p)->()\n", This);

    /* Obtain a monitor and d3d9 device */
    d3d9_adapter = d3d9_adapter_from_hwnd(This->d3d9_ptr, This->pVMR9->baseControlWindow.baseWindow.hWnd, &This->hMon);

    /* Now try to create the d3d9 device */
    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Windowed = TRUE;
    d3dpp.hDeviceWindow = This->pVMR9->baseControlWindow.baseWindow.hWnd;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferHeight = This->pVMR9->target_rect.bottom - This->pVMR9->target_rect.top;
    d3dpp.BackBufferWidth = This->pVMR9->target_rect.right - This->pVMR9->target_rect.left;

    hr = IDirect3D9_CreateDevice(This->d3d9_ptr, d3d9_adapter, D3DDEVTYPE_HAL, NULL, D3DCREATE_MIXED_VERTEXPROCESSING, &d3dpp, &This->d3d9_dev);
    if (FAILED(hr))
    {
        ERR("Could not create device: %08x\n", hr);
        return FALSE;
    }
    IVMRSurfaceAllocatorNotify9_SetD3DDevice(This->SurfaceAllocatorNotify, This->d3d9_dev, This->hMon);

    This->d3d9_surfaces = CoTaskMemAlloc(*numbuffers * sizeof(IDirect3DSurface9 *));
    ZeroMemory(This->d3d9_surfaces, *numbuffers * sizeof(IDirect3DSurface9 *));

    hr = VMR9_SurfaceAllocator_SetAllocationSettings(This, info);
    if (FAILED(hr))
        ERR("Setting allocation settings failed: %08x\n", hr);

    if (SUCCEEDED(hr))
    {
        hr = IVMRSurfaceAllocatorNotify9_AllocateSurfaceHelper(This->SurfaceAllocatorNotify, info, numbuffers, This->d3d9_surfaces);
        if (FAILED(hr))
            ERR("Allocating surfaces failed: %08x\n", hr);
    }

    if (FAILED(hr))
    {
        IVMRSurfaceAllocatorEx9_TerminateDevice(This->pVMR9->allocator, This->pVMR9->cookie);
        return FALSE;
    }

    This->num_surfaces = *numbuffers;

    return TRUE;
}

static HRESULT WINAPI VMR9_SurfaceAllocator_InitializeDevice(IVMRSurfaceAllocatorEx9 *iface, DWORD_PTR id, VMR9AllocationInfo *allocinfo, DWORD *numbuffers)
{
    VMR9DefaultAllocatorPresenterImpl *This = impl_from_IVMRSurfaceAllocatorEx9(iface);

    if (This->pVMR9->mode != VMR9Mode_Windowed && !This->pVMR9->hWndClippingWindow)
    {
        ERR("No window set\n");
        return VFW_E_WRONG_STATE;
    }

    This->info = *allocinfo;

    if (!CreateRenderingWindow(This, allocinfo, numbuffers))
    {
        ERR("Failed to create rendering window, expect no output!\n");
        return VFW_E_WRONG_STATE;
    }

    return S_OK;
}

static HRESULT WINAPI VMR9_SurfaceAllocator_TerminateDevice(IVMRSurfaceAllocatorEx9 *iface, DWORD_PTR id)
{
    TRACE("iface %p, id %#lx.\n", iface, id);

    return S_OK;
}

/* Recreate all surfaces (If allocated as D3DPOOL_DEFAULT) and survive! */
static HRESULT VMR9_SurfaceAllocator_UpdateDeviceReset(VMR9DefaultAllocatorPresenterImpl *This)
{
    struct VERTEX t_vert[4];
    UINT width, height;
    unsigned int i;
    void *bits = NULL;
    D3DPRESENT_PARAMETERS d3dpp;
    HRESULT hr;

    if (!This->d3d9_surfaces || !This->reset)
        return S_OK;

    This->reset = FALSE;
    TRACE("RESETTING\n");
    if (This->d3d9_vertex)
    {
        IDirect3DVertexBuffer9_Release(This->d3d9_vertex);
        This->d3d9_vertex = NULL;
    }

    for (i = 0; i < This->num_surfaces; ++i)
    {
        IDirect3DSurface9 *surface = This->d3d9_surfaces[i];
        TRACE("Releasing surface %p\n", surface);
        if (surface)
            IDirect3DSurface9_Release(surface);
    }
    ZeroMemory(This->d3d9_surfaces, sizeof(IDirect3DSurface9 *) * This->num_surfaces);

    /* Now try to create the d3d9 device */
    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Windowed = TRUE;
    d3dpp.hDeviceWindow = This->pVMR9->baseControlWindow.baseWindow.hWnd;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;

    if (This->d3d9_dev)
        IDirect3DDevice9_Release(This->d3d9_dev);
    This->d3d9_dev = NULL;
    hr = IDirect3D9_CreateDevice(This->d3d9_ptr, d3d9_adapter_from_hwnd(This->d3d9_ptr, This->pVMR9->baseControlWindow.baseWindow.hWnd, &This->hMon), D3DDEVTYPE_HAL, NULL, D3DCREATE_HARDWARE_VERTEXPROCESSING, &d3dpp, &This->d3d9_dev);
    if (FAILED(hr))
    {
        hr = IDirect3D9_CreateDevice(This->d3d9_ptr, d3d9_adapter_from_hwnd(This->d3d9_ptr, This->pVMR9->baseControlWindow.baseWindow.hWnd, &This->hMon), D3DDEVTYPE_HAL, NULL, D3DCREATE_MIXED_VERTEXPROCESSING, &d3dpp, &This->d3d9_dev);
        if (FAILED(hr))
        {
            ERR("--> Creating device: %08x\n", hr);
            return S_OK;
        }
    }
    IVMRSurfaceAllocatorNotify9_ChangeD3DDevice(This->SurfaceAllocatorNotify, This->d3d9_dev, This->hMon);

    IVMRSurfaceAllocatorNotify9_AllocateSurfaceHelper(This->SurfaceAllocatorNotify, &This->info, &This->num_surfaces, This->d3d9_surfaces);

    This->reset = FALSE;

    if (!(This->info.dwFlags & VMR9AllocFlag_TextureSurface))
        return S_OK;

    hr = IDirect3DDevice9_CreateVertexBuffer(This->d3d9_dev, 4 * sizeof(struct VERTEX), D3DUSAGE_WRITEONLY, USED_FVF,
                                             This->info.Pool, &This->d3d9_vertex, NULL);

    width = This->info.dwWidth;
    height = This->info.dwHeight;

    for (i = 0; i < ARRAY_SIZE(t_vert); ++i)
    {
        if (i % 2)
        {
            t_vert[i].x = (float)This->pVMR9->target_rect.right - (float)This->pVMR9->target_rect.left - 0.5f;
            t_vert[i].u = (float)This->pVMR9->source_rect.right / (float)width;
        }
        else
        {
            t_vert[i].x = -0.5f;
            t_vert[i].u = (float)This->pVMR9->source_rect.left / (float)width;
        }

        if (i % 4 < 2)
        {
            t_vert[i].y = -0.5f;
            t_vert[i].v = (float)This->pVMR9->source_rect.bottom / (float)height;
        }
        else
        {
            t_vert[i].y = (float)This->pVMR9->target_rect.bottom - (float)This->pVMR9->target_rect.top - 0.5f;
            t_vert[i].v = (float)This->pVMR9->source_rect.top / (float)height;
        }
        t_vert[i].z = 0.0f;
        t_vert[i].rhw = 1.0f;
    }

    FIXME("Vertex rectangle:\n");
    FIXME("X, Y: %f, %f\n", t_vert[0].x, t_vert[0].y);
    FIXME("X, Y: %f, %f\n", t_vert[3].x, t_vert[3].y);
    FIXME("TOP, LEFT: %f, %f\n", t_vert[0].u, t_vert[0].v);
    FIXME("DOWN, BOTTOM: %f, %f\n", t_vert[3].u, t_vert[3].v);

    IDirect3DVertexBuffer9_Lock(This->d3d9_vertex, 0, sizeof(t_vert), &bits, 0);
    memcpy(bits, t_vert, sizeof(t_vert));
    IDirect3DVertexBuffer9_Unlock(This->d3d9_vertex);

    return S_OK;
}

static HRESULT WINAPI VMR9_SurfaceAllocator_GetSurface(IVMRSurfaceAllocatorEx9 *iface, DWORD_PTR id, DWORD surfaceindex, DWORD flags, IDirect3DSurface9 **surface)
{
    VMR9DefaultAllocatorPresenterImpl *This = impl_from_IVMRSurfaceAllocatorEx9(iface);

    /* Update everything first, this is needed because the surface might be destroyed in the reset */
    if (!This->d3d9_dev)
    {
        TRACE("Device has left me!\n");
        return E_FAIL;
    }

    VMR9_SurfaceAllocator_UpdateDeviceReset(This);

    if (surfaceindex >= This->num_surfaces)
    {
        ERR("surfaceindex is greater than num_surfaces\n");
        return E_FAIL;
    }
    *surface = This->d3d9_surfaces[surfaceindex];
    IDirect3DSurface9_AddRef(*surface);

    return S_OK;
}

static HRESULT WINAPI VMR9_SurfaceAllocator_AdviseNotify(IVMRSurfaceAllocatorEx9 *iface, IVMRSurfaceAllocatorNotify9 *allocnotify)
{
    VMR9DefaultAllocatorPresenterImpl *This = impl_from_IVMRSurfaceAllocatorEx9(iface);

    TRACE("(%p/%p)->(...)\n", iface, This);

    /* No AddRef taken here or the base VMR9 filter would never be destroyed */
    This->SurfaceAllocatorNotify = allocnotify;
    return S_OK;
}

static const IVMRSurfaceAllocatorEx9Vtbl VMR9_SurfaceAllocator =
{
    VMR9_SurfaceAllocator_QueryInterface,
    VMR9_SurfaceAllocator_AddRef,
    VMR9_SurfaceAllocator_Release,
    VMR9_SurfaceAllocator_InitializeDevice,
    VMR9_SurfaceAllocator_TerminateDevice,
    VMR9_SurfaceAllocator_GetSurface,
    VMR9_SurfaceAllocator_AdviseNotify,
    NULL /* This isn't the SurfaceAllocatorEx type yet, working on it */
};

static IDirect3D9 *init_d3d9(HMODULE d3d9_handle)
{
    IDirect3D9 * (__stdcall * d3d9_create)(UINT SDKVersion);

    d3d9_create = (void *)GetProcAddress(d3d9_handle, "Direct3DCreate9");
    if (!d3d9_create) return NULL;

    return d3d9_create(D3D_SDK_VERSION);
}

static HRESULT VMR9DefaultAllocatorPresenterImpl_create(struct quartz_vmr *parent, LPVOID * ppv)
{
    HRESULT hr = S_OK;
    int i;
    VMR9DefaultAllocatorPresenterImpl* This;

    This = CoTaskMemAlloc(sizeof(VMR9DefaultAllocatorPresenterImpl));
    if (!This)
        return E_OUTOFMEMORY;

    This->d3d9_ptr = init_d3d9(parent->hD3d9);
    if (!This->d3d9_ptr)
    {
        WARN("Could not initialize d3d9.dll\n");
        CoTaskMemFree(This);
        return VFW_E_DDRAW_CAPS_NOT_SUITABLE;
    }

    i = 0;
    do
    {
        D3DDISPLAYMODE mode;

        hr = IDirect3D9_EnumAdapterModes(This->d3d9_ptr, i++, D3DFMT_X8R8G8B8, 0, &mode);
	if (hr == D3DERR_INVALIDCALL) break; /* out of adapters */
    } while (FAILED(hr));
    if (FAILED(hr))
        ERR("HR: %08x\n", hr);
    if (hr == D3DERR_NOTAVAILABLE)
    {
        ERR("Format not supported\n");
        IDirect3D9_Release(This->d3d9_ptr);
        CoTaskMemFree(This);
        return VFW_E_DDRAW_CAPS_NOT_SUITABLE;
    }

    This->IVMRImagePresenter9_iface.lpVtbl = &VMR9_ImagePresenter;
    This->IVMRSurfaceAllocatorEx9_iface.lpVtbl = &VMR9_SurfaceAllocator;

    This->refCount = 1;
    This->pVMR9 = parent;
    This->d3d9_surfaces = NULL;
    This->d3d9_dev = NULL;
    This->hMon = 0;
    This->d3d9_vertex = NULL;
    This->num_surfaces = 0;
    This->SurfaceAllocatorNotify = NULL;
    This->reset = FALSE;

    *ppv = &This->IVMRImagePresenter9_iface;
    return S_OK;
}
