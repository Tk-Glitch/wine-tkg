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

struct dmo_parameqfx
{
    IDirectSoundFXParamEq IDirectSoundFXParamEq_iface;
    IMediaObject        IMediaObject_iface;
    IMediaObjectInPlace IMediaObjectInPlace_iface;
    LONG ref;

    DSFXParamEq params;
};

static inline struct dmo_parameqfx *impl_from_IDirectSoundFXParamEq(IDirectSoundFXParamEq *iface)
{
    return CONTAINING_RECORD(iface, struct dmo_parameqfx, IDirectSoundFXParamEq_iface);
}

static inline struct dmo_parameqfx *impl_from_IMediaObject(IMediaObject *iface)
{
    return CONTAINING_RECORD(iface, struct dmo_parameqfx, IMediaObject_iface);
}

static inline struct dmo_parameqfx *impl_from_IMediaObjectInPlace(IMediaObjectInPlace *iface)
{
    return CONTAINING_RECORD(iface, struct dmo_parameqfx, IMediaObjectInPlace_iface);
}

static HRESULT WINAPI parameq_mediaobj_QueryInterface(IMediaObject *iface, REFIID riid, void **obj)
{
    struct dmo_parameqfx *This = impl_from_IMediaObject(iface);
    return IDirectSoundFXParamEq_QueryInterface(&This->IDirectSoundFXParamEq_iface, riid, obj);
}

static ULONG WINAPI parameq_mediaobj_AddRef(IMediaObject *iface)
{
    struct dmo_parameqfx *This = impl_from_IMediaObject(iface);
    return IDirectSoundFXParamEq_AddRef(&This->IDirectSoundFXParamEq_iface);
}

static ULONG WINAPI parameq_mediaobj_Release(IMediaObject *iface)
{
    struct dmo_parameqfx *This = impl_from_IMediaObject(iface);
    return IDirectSoundFXParamEq_Release(&This->IDirectSoundFXParamEq_iface);
}

static HRESULT WINAPI parameq_mediaobj_GetStreamCount(IMediaObject *iface, DWORD *inputs, DWORD *outputs)
{
    struct dmo_parameqfx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %p, %p\n", This, inputs, outputs);
    return E_NOTIMPL;
}

static HRESULT WINAPI parameq_mediaobj_GetInputStreamInfo(IMediaObject *iface, DWORD index, DWORD *flags)
{
    struct dmo_parameqfx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %d, %p\n", This, index, flags);
    return E_NOTIMPL;
}

static HRESULT WINAPI parameq_mediaobj_GetOutputStreamInfo(IMediaObject *iface, DWORD index, DWORD *flags)
{
    struct dmo_parameqfx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %d, %p\n", This, index, flags);
    return E_NOTIMPL;
}

static HRESULT WINAPI parameq_mediaobj_GetInputType(IMediaObject *iface, DWORD index, DWORD type, DMO_MEDIA_TYPE *pmt)
{
    struct dmo_parameqfx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %d, %d, %p\n", This, index, type, pmt);
    return E_NOTIMPL;
}

static HRESULT WINAPI parameq_mediaobj_GetOutputType(IMediaObject *iface, DWORD index, DWORD type, DMO_MEDIA_TYPE *pmt)
{
    struct dmo_parameqfx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %d, %d, %p\n", This, index, type, pmt);
    return E_NOTIMPL;
}

static HRESULT WINAPI parameq_mediaobj_SetInputType(IMediaObject *iface, DWORD index, const DMO_MEDIA_TYPE *pmt, DWORD flags)
{
    struct dmo_parameqfx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %d, %p, %x\n", This, index, pmt, flags);
    return S_OK;
}

static HRESULT WINAPI parameq_mediaobj_SetOutputType(IMediaObject *iface, DWORD index, const DMO_MEDIA_TYPE *pmt, DWORD flags)
{
    struct dmo_parameqfx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %d, %p, %x\n", This, index, pmt, flags);
    return S_OK;
}

static HRESULT WINAPI parameq_mediaobj_GetInputCurrentType(IMediaObject *iface, DWORD index, DMO_MEDIA_TYPE *pmt)
{
    struct dmo_parameqfx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %d, %p\n", This, index, pmt);
    return E_NOTIMPL;
}

static HRESULT WINAPI parameq_mediaobj_GetOutputCurrentType(IMediaObject *iface, DWORD index, DMO_MEDIA_TYPE *pmt)
{
    struct dmo_parameqfx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %d, %p\n", This, index, pmt);
    return E_NOTIMPL;
}

static HRESULT WINAPI parameq_mediaobj_GetInputSizeInfo(IMediaObject *iface, DWORD index, DWORD *size, DWORD *ahead, DWORD *alignment)
{
    struct dmo_parameqfx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %d, %p, %p, %p\n", This, index, size, ahead, alignment);
    return E_NOTIMPL;
}

static HRESULT WINAPI parameq_mediaobj_GetOutputSizeInfo(IMediaObject *iface, DWORD index, DWORD *size, DWORD *alignment)
{
    struct dmo_parameqfx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %d, %p, %p\n", This, index, size, alignment);
    return E_NOTIMPL;
}

static HRESULT WINAPI parameq_mediaobj_GetInputMaxLatency(IMediaObject *iface, DWORD index, REFERENCE_TIME *latency)
{
    struct dmo_parameqfx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %d, %p\n", This, index, latency);
    return E_NOTIMPL;
}

static HRESULT WINAPI parameq_mediaobj_SetInputMaxLatency(IMediaObject *iface, DWORD index, REFERENCE_TIME latency)
{
    struct dmo_parameqfx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %d, %s\n", This, index, wine_dbgstr_longlong(latency));
    return E_NOTIMPL;
}

static HRESULT WINAPI parameq_mediaobj_Flush(IMediaObject *iface)
{
    struct dmo_parameqfx *This = impl_from_IMediaObject(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI parameq_mediaobj_Discontinuity(IMediaObject *iface, DWORD index)
{
    struct dmo_parameqfx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %d\n", This, index);
    return E_NOTIMPL;
}

static HRESULT WINAPI parameq_mediaobj_AllocateStreamingResources(IMediaObject *iface)
{
    struct dmo_parameqfx *This = impl_from_IMediaObject(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI parameq_mediaobj_FreeStreamingResources(IMediaObject *iface)
{
    struct dmo_parameqfx *This = impl_from_IMediaObject(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI parameq_mediaobj_GetInputStatus(IMediaObject *iface, DWORD index, DWORD *flags)
{
    struct dmo_parameqfx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %d, %p\n", This, index, flags);
    return E_NOTIMPL;
}

static HRESULT WINAPI parameq_mediaobj_ProcessInput(IMediaObject *iface, DWORD index, IMediaBuffer *buffer,
                DWORD flags, REFERENCE_TIME timestamp, REFERENCE_TIME length)
{
    struct dmo_parameqfx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %d, %p, %x, %s, %s\n", This, index, buffer, flags, wine_dbgstr_longlong(timestamp), wine_dbgstr_longlong(length));
    return E_NOTIMPL;
}

static HRESULT WINAPI parameq_mediaobj_ProcessOutput(IMediaObject *iface, DWORD flags, DWORD count,
                DMO_OUTPUT_DATA_BUFFER *buffers, DWORD *status)
{
    struct dmo_parameqfx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %x, %d, %p, %p\n", This, flags, count, buffers, status);
    return E_NOTIMPL;
}

static HRESULT WINAPI parameq_mediaobj_Lock(IMediaObject *iface, LONG lock)
{
    struct dmo_parameqfx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %d\n", This, lock);
    return E_NOTIMPL;
}

static const IMediaObjectVtbl parameq_mediaobjectVtbl =
{
    parameq_mediaobj_QueryInterface,
    parameq_mediaobj_AddRef,
    parameq_mediaobj_Release,
    parameq_mediaobj_GetStreamCount,
    parameq_mediaobj_GetInputStreamInfo,
    parameq_mediaobj_GetOutputStreamInfo,
    parameq_mediaobj_GetInputType,
    parameq_mediaobj_GetOutputType,
    parameq_mediaobj_SetInputType,
    parameq_mediaobj_SetOutputType,
    parameq_mediaobj_GetInputCurrentType,
    parameq_mediaobj_GetOutputCurrentType,
    parameq_mediaobj_GetInputSizeInfo,
    parameq_mediaobj_GetOutputSizeInfo,
    parameq_mediaobj_GetInputMaxLatency,
    parameq_mediaobj_SetInputMaxLatency,
    parameq_mediaobj_Flush,
    parameq_mediaobj_Discontinuity,
    parameq_mediaobj_AllocateStreamingResources,
    parameq_mediaobj_FreeStreamingResources,
    parameq_mediaobj_GetInputStatus,
    parameq_mediaobj_ProcessInput,
    parameq_mediaobj_ProcessOutput,
    parameq_mediaobj_Lock
};

static HRESULT WINAPI parameq_mediainplace_QueryInterface(IMediaObjectInPlace *iface, REFIID riid, void **obj)
{
    struct dmo_parameqfx *This = impl_from_IMediaObjectInPlace(iface);
    return IDirectSoundFXParamEq_QueryInterface(&This->IDirectSoundFXParamEq_iface, riid, obj);
}

static ULONG WINAPI parameq_mediainplace_AddRef(IMediaObjectInPlace *iface)
{
    struct dmo_parameqfx *This = impl_from_IMediaObjectInPlace(iface);
    return IDirectSoundFXParamEq_AddRef(&This->IDirectSoundFXParamEq_iface);
}

static ULONG WINAPI parameq_mediainplace_Release(IMediaObjectInPlace *iface)
{
    struct dmo_parameqfx *This = impl_from_IMediaObjectInPlace(iface);
    return IDirectSoundFXParamEq_Release(&This->IDirectSoundFXParamEq_iface);
}

static HRESULT WINAPI parameq_mediainplace_Process(IMediaObjectInPlace *iface, ULONG size, BYTE *data, REFERENCE_TIME start, DWORD flags)
{
    struct dmo_parameqfx *This = impl_from_IMediaObjectInPlace(iface);
    static BOOL once = 0;
    if(!once++)
        FIXME("%p, %d, %p, %s, %x\n", This, size, data, wine_dbgstr_longlong(start), flags);
    else
        TRACE("%p, %d, %p, %s, %x\n", This, size, data, wine_dbgstr_longlong(start), flags);
    return E_NOTIMPL;
}

static HRESULT WINAPI parameq_mediainplace_Clone(IMediaObjectInPlace *iface, IMediaObjectInPlace **object)
{
    struct dmo_parameqfx *This = impl_from_IMediaObjectInPlace(iface);
    FIXME("%p, %p\n", This, object);
    return E_NOTIMPL;
}

static HRESULT WINAPI parameq_mediainplace_GetLatency(IMediaObjectInPlace *iface, REFERENCE_TIME *latency)
{
    struct dmo_parameqfx *This = impl_from_IMediaObjectInPlace(iface);
    FIXME("%p, %p\n", This, latency);
    return E_NOTIMPL;
}

static const IMediaObjectInPlaceVtbl parameq_mediainplaceVtbl =
{
    parameq_mediainplace_QueryInterface,
    parameq_mediainplace_AddRef,
    parameq_mediainplace_Release,
    parameq_mediainplace_Process,
    parameq_mediainplace_Clone,
    parameq_mediainplace_GetLatency
};

static HRESULT WINAPI parameqfx_QueryInterface(IDirectSoundFXParamEq *iface, REFIID riid, void **ppv)
{
    struct dmo_parameqfx *This = impl_from_IDirectSoundFXParamEq(iface);

    TRACE("(%p)->(%s %p)\n", This, debugstr_guid(riid), ppv);

    if (IsEqualGUID(riid, &IID_IUnknown) ||
        IsEqualGUID(riid, &IID_IDirectSoundFXParamEq))
    {
        *ppv = &This->IDirectSoundFXParamEq_iface;
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

static ULONG WINAPI parameqfx_AddRef(IDirectSoundFXParamEq *iface)
{
    struct dmo_parameqfx *This = impl_from_IDirectSoundFXParamEq(iface);
    ULONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref=%u\n", This, ref);

    return ref;
}

static ULONG WINAPI parameqfx_Release(IDirectSoundFXParamEq *iface)
{
    struct dmo_parameqfx *This = impl_from_IDirectSoundFXParamEq(iface);
    ULONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref=%u\n", This, ref);

    if (!ref)
    {
        heap_free(This);
    }
    return ref;
}

static HRESULT WINAPI parameqfx_SetAllParameters(IDirectSoundFXParamEq *iface, const DSFXParamEq *param)
{
    struct dmo_parameqfx *This = impl_from_IDirectSoundFXParamEq(iface);

    TRACE("(%p) %p\n", This, param);

    if(!param)
        return E_POINTER;

    if( (param->fCenter < DSFXPARAMEQ_CENTER_MIN || param->fCenter > DSFXPARAMEQ_CENTER_MAX) ||
        (param->fBandwidth < DSFXPARAMEQ_BANDWIDTH_MIN || param->fBandwidth > DSFXPARAMEQ_BANDWIDTH_MAX) ||
        (param->fGain < DSFXPARAMEQ_GAIN_MIN || param->fGain > DSFXPARAMEQ_GAIN_MAX) )
    {
        return E_INVALIDARG;
    }

    This->params = *param;

    return S_OK;
}

static HRESULT WINAPI parameqfx_GetAllParameters(IDirectSoundFXParamEq *iface, DSFXParamEq *param)
{
    struct dmo_parameqfx *This = impl_from_IDirectSoundFXParamEq(iface);

    TRACE("(%p) %p\n", This, param);

    if(!param)
        return E_INVALIDARG;

    *param = This->params;

    return S_OK;
}

static const struct IDirectSoundFXParamEqVtbl parameqfxVtbl =
{
    parameqfx_QueryInterface,
    parameqfx_AddRef,
    parameqfx_Release,
    parameqfx_SetAllParameters,
    parameqfx_GetAllParameters
};

HRESULT WINAPI ParamEqFactory_CreateInstance(IClassFactory *iface, IUnknown *outer, REFIID riid, void **ppv)
{
    struct dmo_parameqfx *object;
    HRESULT ret;

    TRACE("(%p, %s, %p)\n", outer, debugstr_guid(riid), ppv);

    *ppv = NULL;

    object = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*object));
    if (!object)
        return E_OUTOFMEMORY;

    object->IDirectSoundFXParamEq_iface.lpVtbl = &parameqfxVtbl;
    object->IMediaObject_iface.lpVtbl = &parameq_mediaobjectVtbl;
    object->IMediaObjectInPlace_iface.lpVtbl = &parameq_mediainplaceVtbl;
    object->ref = 1;

    object->params.fCenter    = 3675.0f;
    object->params.fBandwidth =   12.0f;
    object->params.fGain      =    0.0f;

    ret = parameqfx_QueryInterface(&object->IDirectSoundFXParamEq_iface, riid, ppv);
    parameqfx_Release(&object->IDirectSoundFXParamEq_iface);

    return ret;
}
