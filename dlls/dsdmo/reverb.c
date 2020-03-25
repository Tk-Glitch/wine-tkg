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

struct dmo_reverbfx
{
    IDirectSoundFXWavesReverb IDirectSoundFXWavesReverb_iface;
    IMediaObject        IMediaObject_iface;
    IMediaObjectInPlace IMediaObjectInPlace_iface;
    LONG ref;
};

static inline struct dmo_reverbfx *impl_from_IDirectSoundFXWavesReverb(IDirectSoundFXWavesReverb *iface)
{
    return CONTAINING_RECORD(iface, struct dmo_reverbfx, IDirectSoundFXWavesReverb_iface);
}

static inline struct dmo_reverbfx *impl_from_IMediaObject(IMediaObject *iface)
{
    return CONTAINING_RECORD(iface, struct dmo_reverbfx, IMediaObject_iface);
}

static inline struct dmo_reverbfx *impl_from_IMediaObjectInPlace(IMediaObjectInPlace *iface)
{
    return CONTAINING_RECORD(iface, struct dmo_reverbfx, IMediaObjectInPlace_iface);
}

static HRESULT WINAPI reverb_mediaobj_QueryInterface(IMediaObject *iface, REFIID riid, void **obj)
{
    struct dmo_reverbfx *This = impl_from_IMediaObject(iface);
    return IDirectSoundFXWavesReverb_QueryInterface(&This->IDirectSoundFXWavesReverb_iface, riid, obj);
}

static ULONG WINAPI reverb_mediaobj_AddRef(IMediaObject *iface)
{
    struct dmo_reverbfx *This = impl_from_IMediaObject(iface);
    return IDirectSoundFXWavesReverb_AddRef(&This->IDirectSoundFXWavesReverb_iface);
}

static ULONG WINAPI reverb_mediaobj_Release(IMediaObject *iface)
{
    struct dmo_reverbfx *This = impl_from_IMediaObject(iface);
    return IDirectSoundFXWavesReverb_Release(&This->IDirectSoundFXWavesReverb_iface);
}

static HRESULT WINAPI reverb_mediaobj_GetStreamCount(IMediaObject *iface, DWORD *inputs, DWORD *outputs)
{
    struct dmo_reverbfx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %p, %p\n", This, inputs, outputs);
    return E_NOTIMPL;
}

static HRESULT WINAPI reverb_mediaobj_GetInputStreamInfo(IMediaObject *iface, DWORD index, DWORD *flags)
{
    struct dmo_reverbfx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %d, %p\n", This, index, flags);
    return E_NOTIMPL;
}

static HRESULT WINAPI reverb_mediaobj_GetOutputStreamInfo(IMediaObject *iface, DWORD index, DWORD *flags)
{
    struct dmo_reverbfx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %d, %p\n", This, index, flags);
    return E_NOTIMPL;
}

static HRESULT WINAPI reverb_mediaobj_GetInputType(IMediaObject *iface, DWORD index, DWORD type, DMO_MEDIA_TYPE *pmt)
{
    struct dmo_reverbfx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %d, %d, %p\n", This, index, type, pmt);
    return E_NOTIMPL;
}

static HRESULT WINAPI reverb_mediaobj_GetOutputType(IMediaObject *iface, DWORD index, DWORD type, DMO_MEDIA_TYPE *pmt)
{
    struct dmo_reverbfx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %d, %d, %p\n", This, index, type, pmt);
    return E_NOTIMPL;
}

static HRESULT WINAPI reverb_mediaobj_SetInputType(IMediaObject *iface, DWORD index, const DMO_MEDIA_TYPE *pmt, DWORD flags)
{
    struct dmo_reverbfx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %d, %p, %x\n", This, index, pmt, flags);
    return S_OK;
}

static HRESULT WINAPI reverb_mediaobj_SetOutputType(IMediaObject *iface, DWORD index, const DMO_MEDIA_TYPE *pmt, DWORD flags)
{
    struct dmo_reverbfx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %d, %p, %x\n", This, index, pmt, flags);
    return S_OK;
}

static HRESULT WINAPI reverb_mediaobj_GetInputCurrentType(IMediaObject *iface, DWORD index, DMO_MEDIA_TYPE *pmt)
{
    struct dmo_reverbfx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %d, %p\n", This, index, pmt);
    return E_NOTIMPL;
}

static HRESULT WINAPI reverb_mediaobj_GetOutputCurrentType(IMediaObject *iface, DWORD index, DMO_MEDIA_TYPE *pmt)
{
    struct dmo_reverbfx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %d, %p\n", This, index, pmt);
    return E_NOTIMPL;
}

static HRESULT WINAPI reverb_mediaobj_GetInputSizeInfo(IMediaObject *iface, DWORD index, DWORD *size, DWORD *ahead, DWORD *alignment)
{
    struct dmo_reverbfx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %d, %p, %p, %p\n", This, index, size, ahead, alignment);
    return E_NOTIMPL;
}

static HRESULT WINAPI reverb_mediaobj_GetOutputSizeInfo(IMediaObject *iface, DWORD index, DWORD *size, DWORD *alignment)
{
    struct dmo_reverbfx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %d, %p, %p\n", This, index, size, alignment);
    return E_NOTIMPL;
}

static HRESULT WINAPI reverb_mediaobj_GetInputMaxLatency(IMediaObject *iface, DWORD index, REFERENCE_TIME *latency)
{
    struct dmo_reverbfx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %d, %p\n", This, index, latency);
    return E_NOTIMPL;
}

static HRESULT WINAPI reverb_mediaobj_SetInputMaxLatency(IMediaObject *iface, DWORD index, REFERENCE_TIME latency)
{
    struct dmo_reverbfx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %d, %s\n", This, index, wine_dbgstr_longlong(latency));
    return E_NOTIMPL;
}

static HRESULT WINAPI reverb_mediaobj_Flush(IMediaObject *iface)
{
    struct dmo_reverbfx *This = impl_from_IMediaObject(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI reverb_mediaobj_Discontinuity(IMediaObject *iface, DWORD index)
{
    struct dmo_reverbfx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %d\n", This, index);
    return E_NOTIMPL;
}

static HRESULT WINAPI reverb_mediaobj_AllocateStreamingResources(IMediaObject *iface)
{
    struct dmo_reverbfx *This = impl_from_IMediaObject(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI reverb_mediaobj_FreeStreamingResources(IMediaObject *iface)
{
    struct dmo_reverbfx *This = impl_from_IMediaObject(iface);
    FIXME("%p\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI reverb_mediaobj_GetInputStatus(IMediaObject *iface, DWORD index, DWORD *flags)
{
    struct dmo_reverbfx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %d, %p\n", This, index, flags);
    return E_NOTIMPL;
}

static HRESULT WINAPI reverb_mediaobj_ProcessInput(IMediaObject *iface, DWORD index, IMediaBuffer *buffer,
                DWORD flags, REFERENCE_TIME timestamp, REFERENCE_TIME length)
{
    struct dmo_reverbfx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %d, %p, %x, %s, %s\n", This, index, buffer, flags, wine_dbgstr_longlong(timestamp), wine_dbgstr_longlong(length));
    return E_NOTIMPL;
}

static HRESULT WINAPI reverb_mediaobj_ProcessOutput(IMediaObject *iface, DWORD flags, DWORD count,
                DMO_OUTPUT_DATA_BUFFER *buffers, DWORD *status)
{
    struct dmo_reverbfx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %x, %d, %p, %p\n", This, flags, count, buffers, status);
    return E_NOTIMPL;
}

static HRESULT WINAPI reverb_mediaobj_Lock(IMediaObject *iface, LONG lock)
{
    struct dmo_reverbfx *This = impl_from_IMediaObject(iface);
    FIXME("%p, %d\n", This, lock);
    return E_NOTIMPL;
}

static const IMediaObjectVtbl reverb_mediaobjectVtbl =
{
    reverb_mediaobj_QueryInterface,
    reverb_mediaobj_AddRef,
    reverb_mediaobj_Release,
    reverb_mediaobj_GetStreamCount,
    reverb_mediaobj_GetInputStreamInfo,
    reverb_mediaobj_GetOutputStreamInfo,
    reverb_mediaobj_GetInputType,
    reverb_mediaobj_GetOutputType,
    reverb_mediaobj_SetInputType,
    reverb_mediaobj_SetOutputType,
    reverb_mediaobj_GetInputCurrentType,
    reverb_mediaobj_GetOutputCurrentType,
    reverb_mediaobj_GetInputSizeInfo,
    reverb_mediaobj_GetOutputSizeInfo,
    reverb_mediaobj_GetInputMaxLatency,
    reverb_mediaobj_SetInputMaxLatency,
    reverb_mediaobj_Flush,
    reverb_mediaobj_Discontinuity,
    reverb_mediaobj_AllocateStreamingResources,
    reverb_mediaobj_FreeStreamingResources,
    reverb_mediaobj_GetInputStatus,
    reverb_mediaobj_ProcessInput,
    reverb_mediaobj_ProcessOutput,
    reverb_mediaobj_Lock
};

static HRESULT WINAPI reverb_mediainplace_QueryInterface(IMediaObjectInPlace *iface, REFIID riid, void **obj)
{
    struct dmo_reverbfx *This = impl_from_IMediaObjectInPlace(iface);
    return IDirectSoundFXWavesReverb_QueryInterface(&This->IDirectSoundFXWavesReverb_iface, riid, obj);
}

static ULONG WINAPI reverb_mediainplace_AddRef(IMediaObjectInPlace *iface)
{
    struct dmo_reverbfx *This = impl_from_IMediaObjectInPlace(iface);
    return IDirectSoundFXWavesReverb_AddRef(&This->IDirectSoundFXWavesReverb_iface);
}

static ULONG WINAPI reverb_mediainplace_Release(IMediaObjectInPlace *iface)
{
    struct dmo_reverbfx *This = impl_from_IMediaObjectInPlace(iface);
    return IDirectSoundFXWavesReverb_Release(&This->IDirectSoundFXWavesReverb_iface);
}

static HRESULT WINAPI reverb_mediainplace_Process(IMediaObjectInPlace *iface, ULONG size, BYTE *data, REFERENCE_TIME start, DWORD flags)
{
    struct dmo_reverbfx *This = impl_from_IMediaObjectInPlace(iface);
    static BOOL once = 0;
    if(!once++)
        FIXME("%p, %d, %p, %s, %x\n", This, size, data, wine_dbgstr_longlong(start), flags);
    else
        TRACE("%p, %d, %p, %s, %x\n", This, size, data, wine_dbgstr_longlong(start), flags);
    return E_NOTIMPL;
}

static HRESULT WINAPI reverb_mediainplace_Clone(IMediaObjectInPlace *iface, IMediaObjectInPlace **object)
{
    struct dmo_reverbfx *This = impl_from_IMediaObjectInPlace(iface);
    FIXME("%p, %p\n", This, object);
    return E_NOTIMPL;
}

static HRESULT WINAPI reverb_mediainplace_GetLatency(IMediaObjectInPlace *iface, REFERENCE_TIME *latency)
{
    struct dmo_reverbfx *This = impl_from_IMediaObjectInPlace(iface);
    FIXME("%p, %p\n", This, latency);
    return E_NOTIMPL;
}

static const IMediaObjectInPlaceVtbl reverb_mediainplaceVtbl =
{
    reverb_mediainplace_QueryInterface,
    reverb_mediainplace_AddRef,
    reverb_mediainplace_Release,
    reverb_mediainplace_Process,
    reverb_mediainplace_Clone,
    reverb_mediainplace_GetLatency
};

static HRESULT WINAPI reverbfx_QueryInterface(IDirectSoundFXWavesReverb *iface, REFIID riid, void **ppv)
{
    struct dmo_reverbfx *This = impl_from_IDirectSoundFXWavesReverb(iface);

    TRACE("(%p)->(%s %p)\n", This, debugstr_guid(riid), ppv);

    if (IsEqualGUID(riid, &IID_IUnknown) ||
        IsEqualGUID(riid, &IID_IDirectSoundFXWavesReverb))
    {
        *ppv = &This->IDirectSoundFXWavesReverb_iface;
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

static ULONG WINAPI reverbfx_AddRef(IDirectSoundFXWavesReverb *iface)
{
    struct dmo_reverbfx *This = impl_from_IDirectSoundFXWavesReverb(iface);
    ULONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref=%u\n", This, ref);

    return ref;
}

static ULONG WINAPI reverbfx_Release(IDirectSoundFXWavesReverb *iface)
{
    struct dmo_reverbfx *This = impl_from_IDirectSoundFXWavesReverb(iface);
    ULONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref=%u\n", This, ref);

    if (!ref)
    {
        heap_free(This);
    }
    return ref;
}

static HRESULT WINAPI reverbfx_SetAllParameters(IDirectSoundFXWavesReverb *iface, const DSFXWavesReverb *reverb)
{
    struct dmo_reverbfx *This = impl_from_IDirectSoundFXWavesReverb(iface);
    FIXME("(%p) %p\n", This, reverb);

    return E_NOTIMPL;
}

static HRESULT WINAPI reverbfx_GetAllParameters(IDirectSoundFXWavesReverb *iface, DSFXWavesReverb *reverb)
{
    struct dmo_reverbfx *This = impl_from_IDirectSoundFXWavesReverb(iface);
    FIXME("(%p) %p\n", This, reverb);

    return E_NOTIMPL;
}

static const struct IDirectSoundFXWavesReverbVtbl reverbfxVtbl =
{
    reverbfx_QueryInterface,
    reverbfx_AddRef,
    reverbfx_Release,
    reverbfx_SetAllParameters,
    reverbfx_GetAllParameters
};

HRESULT WINAPI ReverbFactory_CreateInstance(IClassFactory *iface, IUnknown *outer, REFIID riid, void **ppv)
{
    struct dmo_reverbfx *object;
    HRESULT ret;

    TRACE("(%p, %s, %p)\n", outer, debugstr_guid(riid), ppv);

    *ppv = NULL;

    object = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*object));
    if (!object)
        return E_OUTOFMEMORY;

    object->IDirectSoundFXWavesReverb_iface.lpVtbl = &reverbfxVtbl;
    object->IMediaObject_iface.lpVtbl = &reverb_mediaobjectVtbl;
    object->IMediaObjectInPlace_iface.lpVtbl = &reverb_mediainplaceVtbl;
    object->ref = 1;

    ret = reverbfx_QueryInterface(&object->IDirectSoundFXWavesReverb_iface, riid, ppv);
    reverbfx_Release(&object->IDirectSoundFXWavesReverb_iface);

    return ret;
}
