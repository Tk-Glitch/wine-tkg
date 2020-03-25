/*
 * Copyright 2019 Alistair Leslie-Hughes
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
#define COBJMACROS

#include "dsdmo_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(dsdmo);

struct dmo_garglefx
{
    IDirectSoundFXGargle IDirectSoundFXGargle_iface;
    IMediaObject        IMediaObject_iface;
    IMediaObjectInPlace IMediaObjectInPlace_iface;
    LONG ref;

    DSFXGargle params;
};

static inline struct dmo_garglefx *impl_from_IDirectSoundFXGargle(IDirectSoundFXGargle *iface)
{
    return CONTAINING_RECORD(iface, struct dmo_garglefx, IDirectSoundFXGargle_iface);
}

static inline struct dmo_garglefx *impl_from_IMediaObject(IMediaObject *iface)
{
    return CONTAINING_RECORD(iface, struct dmo_garglefx, IMediaObject_iface);
}

static inline struct dmo_garglefx *impl_from_IMediaObjectInPlace(IMediaObjectInPlace *iface)
{
    return CONTAINING_RECORD(iface, struct dmo_garglefx, IMediaObjectInPlace_iface);
}

static HRESULT WINAPI gargle_mediaobj_QueryInterface(IMediaObject *iface, REFIID riid, void **obj)
{
    struct dmo_garglefx *This = impl_from_IMediaObject(iface);
    return IDirectSoundFXGargle_QueryInterface(&This->IDirectSoundFXGargle_iface, riid, obj);
}

static ULONG WINAPI gargle_mediaobj_AddRef(IMediaObject *iface)
{
    struct dmo_garglefx *This = impl_from_IMediaObject(iface);
    return IDirectSoundFXGargle_AddRef(&This->IDirectSoundFXGargle_iface);
}

static ULONG WINAPI gargle_mediaobj_Release(IMediaObject *iface)
{
    struct dmo_garglefx *This = impl_from_IMediaObject(iface);
    return IDirectSoundFXGargle_Release(&This->IDirectSoundFXGargle_iface);
}

static HRESULT WINAPI gargle_mediaobj_GetStreamCount(IMediaObject *iface, DWORD *inputs, DWORD *outputs)
{
    struct dmo_garglefx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %p, %p\n", This, inputs, outputs);
    return E_NOTIMPL;
}

static HRESULT WINAPI gargle_mediaobj_GetInputStreamInfo(IMediaObject *iface, DWORD index, DWORD *flags)
{
    struct dmo_garglefx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %d, %p\n", This, index, flags);
    return E_NOTIMPL;
}

static HRESULT WINAPI gargle_mediaobj_GetOutputStreamInfo(IMediaObject *iface, DWORD index, DWORD *flags)
{
    struct dmo_garglefx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %d, %p\n", This, index, flags);
    return E_NOTIMPL;
}

static HRESULT WINAPI gargle_mediaobj_GetInputType(IMediaObject *iface, DWORD index, DWORD type, DMO_MEDIA_TYPE *pmt)
{
    struct dmo_garglefx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %d, %d, %p\n", This, index, type, pmt);
    return E_NOTIMPL;
}

static HRESULT WINAPI gargle_mediaobj_GetOutputType(IMediaObject *iface, DWORD index, DWORD type, DMO_MEDIA_TYPE *pmt)
{
    struct dmo_garglefx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %d, %d, %p\n", This, index, type, pmt);
    return E_NOTIMPL;
}

static HRESULT WINAPI gargle_mediaobj_SetInputType(IMediaObject *iface, DWORD index, const DMO_MEDIA_TYPE *pmt, DWORD flags)
{
    struct dmo_garglefx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %d, %p, %x\n", This, index, pmt, flags);
    return S_OK;
}

static HRESULT WINAPI gargle_mediaobj_SetOutputType(IMediaObject *iface, DWORD index, const DMO_MEDIA_TYPE *pmt, DWORD flags)
{
    struct dmo_garglefx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %d, %p, %x\n", This, index, pmt, flags);
    return S_OK;
}

static HRESULT WINAPI gargle_mediaobj_GetInputCurrentType(IMediaObject *iface, DWORD index, DMO_MEDIA_TYPE *pmt)
{
    struct dmo_garglefx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %d, %p\n", This, index, pmt);
    return E_NOTIMPL;
}

static HRESULT WINAPI gargle_mediaobj_GetOutputCurrentType(IMediaObject *iface, DWORD index, DMO_MEDIA_TYPE *pmt)
{
    struct dmo_garglefx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %d, %p\n", This, index, pmt);
    return E_NOTIMPL;
}

static HRESULT WINAPI gargle_mediaobj_GetInputSizeInfo(IMediaObject *iface, DWORD index, DWORD *size, DWORD *ahead, DWORD *alignment)
{
    struct dmo_garglefx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %d, %p, %p, %p\n", This, index, size, ahead, alignment);
    return E_NOTIMPL;
}

static HRESULT WINAPI gargle_mediaobj_GetOutputSizeInfo(IMediaObject *iface, DWORD index, DWORD *size, DWORD *alignment)
{
    struct dmo_garglefx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %d, %p, %p\n", This, index, size, alignment);
    return E_NOTIMPL;
}

static HRESULT WINAPI gargle_mediaobj_GetInputMaxLatency(IMediaObject *iface, DWORD index, REFERENCE_TIME *latency)
{
    struct dmo_garglefx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %d, %p\n", This, index, latency);
    return E_NOTIMPL;
}

static HRESULT WINAPI gargle_mediaobj_SetInputMaxLatency(IMediaObject *iface, DWORD index, REFERENCE_TIME latency)
{
    struct dmo_garglefx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %d, %s\n", This, index, wine_dbgstr_longlong(latency));
    return E_NOTIMPL;
}

static HRESULT WINAPI gargle_mediaobj_Flush(IMediaObject *iface)
{
    struct dmo_garglefx *This = impl_from_IMediaObject(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI gargle_mediaobj_Discontinuity(IMediaObject *iface, DWORD index)
{
    struct dmo_garglefx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %d\n", This, index);
    return E_NOTIMPL;
}

static HRESULT WINAPI gargle_mediaobj_AllocateStreamingResources(IMediaObject *iface)
{
    struct dmo_garglefx *This = impl_from_IMediaObject(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI gargle_mediaobj_FreeStreamingResources(IMediaObject *iface)
{
    struct dmo_garglefx *This = impl_from_IMediaObject(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI gargle_mediaobj_GetInputStatus(IMediaObject *iface, DWORD index, DWORD *flags)
{
    struct dmo_garglefx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %d, %p\n", This, index, flags);
    return E_NOTIMPL;
}

static HRESULT WINAPI gargle_mediaobj_ProcessInput(IMediaObject *iface, DWORD index, IMediaBuffer *buffer,
                DWORD flags, REFERENCE_TIME timestamp, REFERENCE_TIME length)
{
    struct dmo_garglefx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %d, %p, %x, %s, %s\n", This, index, buffer, flags, wine_dbgstr_longlong(timestamp), wine_dbgstr_longlong(length));
    return E_NOTIMPL;
}

static HRESULT WINAPI gargle_mediaobj_ProcessOutput(IMediaObject *iface, DWORD flags, DWORD count,
                DMO_OUTPUT_DATA_BUFFER *buffers, DWORD *status)
{
    struct dmo_garglefx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %x, %d, %p, %p\n", This, flags, count, buffers, status);
    return E_NOTIMPL;
}

static HRESULT WINAPI gargle_mediaobj_Lock(IMediaObject *iface, LONG lock)
{
    struct dmo_garglefx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %d\n", This, lock);
    return E_NOTIMPL;
}

static const IMediaObjectVtbl gargle_mediaobjectVtbl =
{
    gargle_mediaobj_QueryInterface,
    gargle_mediaobj_AddRef,
    gargle_mediaobj_Release,
    gargle_mediaobj_GetStreamCount,
    gargle_mediaobj_GetInputStreamInfo,
    gargle_mediaobj_GetOutputStreamInfo,
    gargle_mediaobj_GetInputType,
    gargle_mediaobj_GetOutputType,
    gargle_mediaobj_SetInputType,
    gargle_mediaobj_SetOutputType,
    gargle_mediaobj_GetInputCurrentType,
    gargle_mediaobj_GetOutputCurrentType,
    gargle_mediaobj_GetInputSizeInfo,
    gargle_mediaobj_GetOutputSizeInfo,
    gargle_mediaobj_GetInputMaxLatency,
    gargle_mediaobj_SetInputMaxLatency,
    gargle_mediaobj_Flush,
    gargle_mediaobj_Discontinuity,
    gargle_mediaobj_AllocateStreamingResources,
    gargle_mediaobj_FreeStreamingResources,
    gargle_mediaobj_GetInputStatus,
    gargle_mediaobj_ProcessInput,
    gargle_mediaobj_ProcessOutput,
    gargle_mediaobj_Lock
};

static HRESULT WINAPI gargle_mediainplace_QueryInterface(IMediaObjectInPlace *iface, REFIID riid, void **obj)
{
    struct dmo_garglefx *This = impl_from_IMediaObjectInPlace(iface);
    return IDirectSoundFXGargle_QueryInterface(&This->IDirectSoundFXGargle_iface, riid, obj);
}

static ULONG WINAPI gargle_mediainplace_AddRef(IMediaObjectInPlace *iface)
{
    struct dmo_garglefx *This = impl_from_IMediaObjectInPlace(iface);
    return IDirectSoundFXGargle_AddRef(&This->IDirectSoundFXGargle_iface);
}

static ULONG WINAPI gargle_mediainplace_Release(IMediaObjectInPlace *iface)
{
    struct dmo_garglefx *This = impl_from_IMediaObjectInPlace(iface);
    return IDirectSoundFXGargle_Release(&This->IDirectSoundFXGargle_iface);
}

static HRESULT WINAPI gargle_mediainplace_Process(IMediaObjectInPlace *iface, ULONG size, BYTE *data, REFERENCE_TIME start, DWORD flags)
{
    struct dmo_garglefx *This = impl_from_IMediaObjectInPlace(iface);
    static BOOL once = 0;
    if(!once++)
        FIXME("%p, %d, %p, %s, %x\n", This, size, data, wine_dbgstr_longlong(start), flags);
    else
        TRACE("%p, %d, %p, %s, %x\n", This, size, data, wine_dbgstr_longlong(start), flags);
    return E_NOTIMPL;
}

static HRESULT WINAPI gargle_mediainplace_Clone(IMediaObjectInPlace *iface, IMediaObjectInPlace **object)
{
    struct dmo_garglefx *This = impl_from_IMediaObjectInPlace(iface);
    FIXME("%p, %p\n", This, object);
    return E_NOTIMPL;
}

static HRESULT WINAPI gargle_mediainplace_GetLatency(IMediaObjectInPlace *iface, REFERENCE_TIME *latency)
{
    struct dmo_garglefx *This = impl_from_IMediaObjectInPlace(iface);
    FIXME("%p, %p\n", This, latency);
    return E_NOTIMPL;
}

static const IMediaObjectInPlaceVtbl gargle_mediainplaceVtbl =
{
    gargle_mediainplace_QueryInterface,
    gargle_mediainplace_AddRef,
    gargle_mediainplace_Release,
    gargle_mediainplace_Process,
    gargle_mediainplace_Clone,
    gargle_mediainplace_GetLatency
};

static HRESULT WINAPI garglefx_QueryInterface(IDirectSoundFXGargle *iface, REFIID riid, void **ppv)
{
    struct dmo_garglefx *This = impl_from_IDirectSoundFXGargle(iface);

    TRACE("(%p)->(%s %p)\n", This, debugstr_guid(riid), ppv);

    if (IsEqualGUID(riid, &IID_IUnknown) ||
        IsEqualGUID(riid, &IID_IDirectSoundFXGargle))
    {
        *ppv = &This->IDirectSoundFXGargle_iface;
    }
    else if(IsEqualGUID(riid, &IID_IMediaObject))
    {
        *ppv = &This->IMediaObject_iface;
    }
    else if(IsEqualGUID(riid, &IID_IMediaObjectInPlace))
    {
        *ppv = &This->IMediaObjectInPlace_iface;
    }

    if(!*ppv)
    {
        FIXME("(%p)->(%s,%p),not found\n", This, debugstr_guid(riid), ppv);
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown*)*ppv);

    return S_OK;
}

static ULONG WINAPI garglefx_AddRef(IDirectSoundFXGargle *iface)
{
    struct dmo_garglefx *This = impl_from_IDirectSoundFXGargle(iface);
    ULONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref=%u\n", This, ref);

    return ref;
}

static ULONG WINAPI garglefx_Release(IDirectSoundFXGargle *iface)
{
    struct dmo_garglefx *This = impl_from_IDirectSoundFXGargle(iface);
    ULONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref=%u\n", This, ref);

    if (!ref)
    {
        heap_free(This);
    }
    return ref;
}

static HRESULT WINAPI garglefx_SetAllParameters(IDirectSoundFXGargle *iface, const DSFXGargle *gargle)
{
    struct dmo_garglefx *This = impl_from_IDirectSoundFXGargle(iface);

    TRACE("(%p) %p\n", This, gargle);

    if(!gargle)
        return E_POINTER;

    /* Out of Range values */
    if( (gargle->dwRateHz < DSFXGARGLE_RATEHZ_MIN || gargle->dwRateHz > DSFXGARGLE_RATEHZ_MAX) ||
        (gargle->dwWaveShape != DSFXGARGLE_WAVE_SQUARE && gargle->dwWaveShape != DSFXGARGLE_WAVE_TRIANGLE) )
    {
        return E_INVALIDARG;
    }

    This->params = *gargle;

    return S_OK;
}

static HRESULT WINAPI garglefx_GetAllParameters(IDirectSoundFXGargle *iface, DSFXGargle *gargle)
{
    struct dmo_garglefx *This = impl_from_IDirectSoundFXGargle(iface);

    TRACE("(%p) %p\n", This, gargle);

    if(!gargle)
        return E_INVALIDARG;

    *gargle = This->params;

    return S_OK;
}

static const struct IDirectSoundFXGargleVtbl garglefxVtbl =
{
    garglefx_QueryInterface,
    garglefx_AddRef,
    garglefx_Release,
    garglefx_SetAllParameters,
    garglefx_GetAllParameters
};

HRESULT WINAPI GargleFactory_CreateInstance(IClassFactory *iface, IUnknown *outer, REFIID riid, void **ppv)
{
    struct dmo_garglefx *object;
    HRESULT ret;

    TRACE("(%p, %s, %p)\n", outer, debugstr_guid(riid), ppv);

    *ppv = NULL;

    object = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*object));
    if (!object)
        return E_OUTOFMEMORY;

    object->IDirectSoundFXGargle_iface.lpVtbl = &garglefxVtbl;
    object->IMediaObject_iface.lpVtbl = &gargle_mediaobjectVtbl;
    object->IMediaObjectInPlace_iface.lpVtbl = &gargle_mediainplaceVtbl;
    object->ref = 1;

    object->params.dwRateHz    = 20;
    object->params.dwWaveShape = DSFXGARGLE_WAVE_TRIANGLE;

    ret = garglefx_QueryInterface(&object->IDirectSoundFXGargle_iface, riid, ppv);
    garglefx_Release(&object->IDirectSoundFXGargle_iface);

    return ret;
}
