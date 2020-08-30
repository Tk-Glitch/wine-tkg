#include "config.h"
#include <gst/gst.h>

#include "gst_private.h"
#include "gst_cbs.h"

#include "mfapi.h"
#include "mferror.h"
#include "mfidl.h"

#include "wine/debug.h"
#include "wine/heap.h"

WINE_DEFAULT_DEBUG_CHANNEL(mfplat);

static const GUID *raw_types[] = {
    &MFVideoFormat_RGB24,
    &MFVideoFormat_RGB32,
    &MFVideoFormat_RGB555,
    &MFVideoFormat_RGB8,
    &MFVideoFormat_AYUV,
    &MFVideoFormat_I420,
    &MFVideoFormat_IYUV,
    &MFVideoFormat_NV11,
    &MFVideoFormat_NV12,
    &MFVideoFormat_UYVY,
    &MFVideoFormat_v216,
    &MFVideoFormat_v410,
    &MFVideoFormat_YUY2,
    &MFVideoFormat_YVYU,
    &MFVideoFormat_YVYU,
};

struct color_converter
{
    IMFTransform IMFTransform_iface;
    LONG refcount;
    IMFAttributes *attributes;
    IMFAttributes *output_attributes;
    IMFMediaType *input_type;
    IMFMediaType *output_type;
    BOOL valid_state, inflight;
    GstElement *container, *appsrc, *videoconvert, *appsink;
    GstBus *bus;
    CRITICAL_SECTION cs;
};

static struct color_converter *impl_color_converter_from_IMFTransform(IMFTransform *iface)
{
    return CONTAINING_RECORD(iface, struct color_converter, IMFTransform_iface);
}

static HRESULT WINAPI color_converter_QueryInterface(IMFTransform *iface, REFIID riid, void **obj)
{
    TRACE("%p, %s, %p.\n", iface, debugstr_guid(riid), obj);

    if (IsEqualIID(riid, &IID_IMFTransform) ||
            IsEqualIID(riid, &IID_IUnknown))
    {
        *obj = iface;
        IMFTransform_AddRef(iface);
        return S_OK;
    }

    WARN("Unsupported %s.\n", debugstr_guid(riid));
    *obj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI color_converter_AddRef(IMFTransform *iface)
{
    struct color_converter *transform = impl_color_converter_from_IMFTransform(iface);
    ULONG refcount = InterlockedIncrement(&transform->refcount);

    TRACE("%p, refcount %u.\n", iface, refcount);

    return refcount;
}

static ULONG WINAPI color_converter_Release(IMFTransform *iface)
{
    struct color_converter *transform = impl_color_converter_from_IMFTransform(iface);
    ULONG refcount = InterlockedDecrement(&transform->refcount);

    TRACE("%p, refcount %u.\n", iface, refcount);

    if (!refcount)
    {
        if (transform->attributes)
            IMFAttributes_Release(transform->attributes);
        if (transform->output_attributes)
            IMFAttributes_Release(transform->output_attributes);
        heap_free(transform);
    }

    return refcount;
}

static HRESULT WINAPI color_converter_GetStreamLimits(IMFTransform *iface, DWORD *input_minimum, DWORD *input_maximum,
        DWORD *output_minimum, DWORD *output_maximum)
{
    TRACE("%p, %p, %p, %p, %p.\n", iface, input_minimum, input_maximum, output_minimum, output_maximum);

    *input_minimum = *input_maximum = *output_minimum = *output_maximum = 1;

    return S_OK;
}

static HRESULT WINAPI color_converter_GetStreamCount(IMFTransform *iface, DWORD *inputs, DWORD *outputs)
{
    TRACE("%p, %p, %p.\n", iface, inputs, outputs);

    *inputs = *outputs = 1;

    return S_OK;
}

static HRESULT WINAPI color_converter_GetStreamIDs(IMFTransform *iface, DWORD input_size, DWORD *inputs,
        DWORD output_size, DWORD *outputs)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI color_converter_GetInputStreamInfo(IMFTransform *iface, DWORD id, MFT_INPUT_STREAM_INFO *info)
{
    struct color_converter *converter = impl_color_converter_from_IMFTransform(iface);

    TRACE("%p %u %p\n", converter, id, info);

    if (id != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    /* If we create a wrapped GstBuffer, remove MFT_INPUT_STREAM_DOES_NOT_ADDREF */
    info->dwFlags = MFT_INPUT_STREAM_WHOLE_SAMPLES | MFT_INPUT_STREAM_DOES_NOT_ADDREF;
    info->cbMaxLookahead = 0;
    info->cbAlignment = 0;
    /* this is incorrect */
    info->hnsMaxLatency = 0;
    return S_OK;
}

static HRESULT WINAPI color_converter_GetOutputStreamInfo(IMFTransform *iface, DWORD id, MFT_OUTPUT_STREAM_INFO *info)
{
    struct color_converter *converter = impl_color_converter_from_IMFTransform(iface);

    MFT_OUTPUT_STREAM_INFO stream_info = {};

    TRACE("%p %u %p\n", converter, id, info);

    if (id != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    stream_info.dwFlags = MFT_OUTPUT_STREAM_PROVIDES_SAMPLES;
    stream_info.cbSize = 0;
    stream_info.cbAlignment = 0;

    *info = stream_info;

    return S_OK;
}

static HRESULT WINAPI color_converter_GetAttributes(IMFTransform *iface, IMFAttributes **attributes)
{
    struct color_converter *transform = impl_color_converter_from_IMFTransform(iface);

    TRACE("%p, %p.\n", iface, attributes);

    *attributes = transform->attributes;
    IMFAttributes_AddRef(*attributes);

    return S_OK;
}

static HRESULT WINAPI color_converter_GetInputStreamAttributes(IMFTransform *iface, DWORD id,
        IMFAttributes **attributes)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI color_converter_GetOutputStreamAttributes(IMFTransform *iface, DWORD id,
        IMFAttributes **attributes)
{
    struct color_converter *transform = impl_color_converter_from_IMFTransform(iface);

    TRACE("%p, %u, %p.\n", iface, id, attributes);

    *attributes = transform->output_attributes;
    IMFAttributes_AddRef(*attributes);

    return S_OK;
}

static HRESULT WINAPI color_converter_DeleteInputStream(IMFTransform *iface, DWORD id)
{
    TRACE("%p, %u.\n", iface, id);

    return E_NOTIMPL;
}

static HRESULT WINAPI color_converter_AddInputStreams(IMFTransform *iface, DWORD streams, DWORD *ids)
{
    TRACE("%p, %u, %p.\n", iface, streams, ids);

    return E_NOTIMPL;
}

static HRESULT WINAPI color_converter_GetInputAvailableType(IMFTransform *iface, DWORD id, DWORD index,
        IMFMediaType **type)
{
    IMFMediaType *ret;
    HRESULT hr;

    TRACE("%p, %u, %u, %p.\n", iface, id, index, type);

    if (id != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    if (index >= ARRAY_SIZE(raw_types))
        return MF_E_NO_MORE_TYPES;

    if (FAILED(hr = MFCreateMediaType(&ret)))
        return hr;

    if (FAILED(hr = IMFMediaType_SetGUID(ret, &MF_MT_MAJOR_TYPE, &MFMediaType_Video)))
    {
        IMFMediaType_Release(ret);
        return hr;
    }

    if (FAILED(hr = IMFMediaType_SetGUID(ret, &MF_MT_SUBTYPE, raw_types[index])))
    {
        IMFMediaType_Release(ret);
        return hr;
    }

    *type = ret;

    return S_OK;
}

static void copy_attr(IMFMediaType *target, IMFMediaType *source, const GUID *key)
{
    PROPVARIANT val;

    if (SUCCEEDED(IMFAttributes_GetItem((IMFAttributes *)source, key, &val)))
    {
        IMFAttributes_SetItem((IMFAttributes* )target, key, &val);
    }
}

static HRESULT WINAPI color_converter_GetOutputAvailableType(IMFTransform *iface, DWORD id, DWORD index,
        IMFMediaType **type)
{
    struct color_converter *converter = impl_color_converter_from_IMFTransform(iface);
    IMFMediaType *output_type;
    HRESULT hr;

    TRACE("%p, %u, %u, %p.\n", iface, id, index, type);

    if (id != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    if (!(converter->input_type))
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    if (index >= ARRAY_SIZE(raw_types))
        return MF_E_NO_MORE_TYPES;

    if (FAILED(hr = MFCreateMediaType(&output_type)))
        return hr;

    copy_attr(output_type, converter->input_type, &MF_MT_MAJOR_TYPE);
    copy_attr(output_type, converter->input_type, &MF_MT_FRAME_SIZE);
    copy_attr(output_type, converter->input_type, &MF_MT_FRAME_RATE);

    if (FAILED(hr = IMFMediaType_SetGUID(output_type, &MF_MT_SUBTYPE, raw_types[index])))
    {
        IMFMediaType_Release(output_type);
        return hr;
    }

    *type = output_type;

    return S_OK;
}

static void color_converter_update_pipeline_state(struct color_converter *converter)
{
    converter->valid_state = converter->input_type && converter->output_type;

    if (!converter->valid_state)
    {
        gst_element_set_state(converter->container, GST_STATE_READY);
        return;
    }

    g_object_set(converter->appsrc, "caps", caps_from_mf_media_type(converter->input_type), NULL);
    g_object_set(converter->appsink, "caps", caps_from_mf_media_type(converter->output_type), NULL);

    gst_element_set_state(converter->container, GST_STATE_PLAYING);
    return;
}

static HRESULT WINAPI color_converter_SetInputType(IMFTransform *iface, DWORD id, IMFMediaType *type, DWORD flags)
{
    HRESULT hr;

    struct color_converter *converter = impl_color_converter_from_IMFTransform(iface);

    TRACE("%p, %u, %p, %#x.\n", iface, id, type, flags);

    if (id != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    if (type)
    {
        GUID major_type, subtype;
        BOOL found = FALSE;

        if (FAILED(IMFMediaType_GetGUID(type, &MF_MT_MAJOR_TYPE, &major_type)))
            return MF_E_INVALIDTYPE;
        if (FAILED(IMFMediaType_GetGUID(type, &MF_MT_SUBTYPE, &subtype)))
            return MF_E_INVALIDTYPE;

        if (!(IsEqualGUID(&major_type, &MFMediaType_Video)))
            return MF_E_INVALIDTYPE;

        for (unsigned int i = 0; i < ARRAY_SIZE(raw_types); i++)
        {
            UINT64 unused;

            if (IsEqualGUID(&subtype, raw_types[i]))
            {
                if (FAILED(IMFMediaType_GetUINT64(type, &MF_MT_FRAME_SIZE, &unused)))
                    return MF_E_INVALIDTYPE;
                found = TRUE;
                break;
            }
        }

        if (!found)
            return MF_E_INVALIDTYPE;
    }

    if (flags & MFT_SET_TYPE_TEST_ONLY)
    {
        return S_OK;
    }

    hr = S_OK;

    EnterCriticalSection(&converter->cs);

    if (type)
    {
        if (!converter->input_type)
            if (FAILED(hr = MFCreateMediaType(&converter->input_type)))
                goto done;

        if (FAILED(hr = IMFMediaType_CopyAllItems(type, (IMFAttributes *) converter->input_type)))
            goto done;
    }
    else if (converter->input_type)
    {
        IMFMediaType_Release(converter->input_type);
        converter->input_type = NULL;
    }

    done:
    if (hr == S_OK)
        color_converter_update_pipeline_state(converter);
    LeaveCriticalSection(&converter->cs);

    return S_OK;
}

static HRESULT WINAPI color_converter_SetOutputType(IMFTransform *iface, DWORD id, IMFMediaType *type, DWORD flags)
{
    struct color_converter *converter = impl_color_converter_from_IMFTransform(iface);
    HRESULT hr;
    GUID major_type, subtype;
    UINT64 unused;

    TRACE("%p, %u, %p, %#x.\n", iface, id, type, flags);

    if (id != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    if (type)
    {
        /* validate the type */

        if (FAILED(IMFMediaType_GetGUID(type, &MF_MT_MAJOR_TYPE, &major_type)))
            return MF_E_INVALIDTYPE;
        if (FAILED(IMFMediaType_GetGUID(type, &MF_MT_SUBTYPE, &subtype)))
            return MF_E_INVALIDTYPE;
        if (FAILED(IMFMediaType_GetUINT64(type, &MF_MT_FRAME_SIZE, &unused)))
            return MF_E_INVALIDTYPE;

        if (!(IsEqualGUID(&major_type, &MFMediaType_Video)))
            return MF_E_INVALIDTYPE;

        for (unsigned int i = 0; i < ARRAY_SIZE(raw_types); i++)
        {
            if (IsEqualGUID(&subtype, raw_types[i]))
                break;
            if (i == ARRAY_SIZE(raw_types))
                return MF_E_INVALIDTYPE;
        }
    }

    if (flags & MFT_SET_TYPE_TEST_ONLY)
    {
        return S_OK;
    }

    EnterCriticalSection(&converter->cs);

    hr = S_OK;

    if (type)
    {
        if (!converter->output_type)
            if (FAILED(hr = MFCreateMediaType(&converter->output_type)))
                goto done;

        if (FAILED(hr = IMFMediaType_CopyAllItems(type, (IMFAttributes *) converter->output_type)))
            goto done;
    }
    else if (converter->output_type)
    {
        IMFMediaType_Release(converter->output_type);
        converter->output_type = NULL;
    }

    done:
    if (hr == S_OK)
        color_converter_update_pipeline_state(converter);
    LeaveCriticalSection(&converter->cs);

    return hr;
}

static HRESULT WINAPI color_converter_GetInputCurrentType(IMFTransform *iface, DWORD id, IMFMediaType **type)
{
    FIXME("%p, %u, %p.\n", iface, id, type);

    return E_NOTIMPL;
}

static HRESULT WINAPI color_converter_GetOutputCurrentType(IMFTransform *iface, DWORD id, IMFMediaType **type)
{
    FIXME("%p, %u, %p.\n", iface, id, type);

    return E_NOTIMPL;
}

static HRESULT WINAPI color_converter_GetInputStatus(IMFTransform *iface, DWORD id, DWORD *flags)
{
    FIXME("%p, %u, %p.\n", iface, id, flags);

    return E_NOTIMPL;
}

static HRESULT WINAPI color_converter_GetOutputStatus(IMFTransform *iface, DWORD *flags)
{
    FIXME("%p, %p.\n", iface, flags);

    return E_NOTIMPL;
}

static HRESULT WINAPI color_converter_SetOutputBounds(IMFTransform *iface, LONGLONG lower, LONGLONG upper)
{
    FIXME("%p, %s, %s.\n", iface, wine_dbgstr_longlong(lower), wine_dbgstr_longlong(upper));

    return E_NOTIMPL;
}

static HRESULT WINAPI color_converter_ProcessEvent(IMFTransform *iface, DWORD id, IMFMediaEvent *event)
{
    TRACE("%p, %u, %p.\n", iface, id, event);

    return E_NOTIMPL;
}

static HRESULT WINAPI color_converter_ProcessMessage(IMFTransform *iface, MFT_MESSAGE_TYPE message, ULONG_PTR param)
{
    FIXME("%p, %u.\n", iface, message);

    return S_OK;
}

static HRESULT WINAPI color_converter_ProcessInput(IMFTransform *iface, DWORD id, IMFSample *sample, DWORD flags)
{
    struct color_converter *converter = impl_color_converter_from_IMFTransform(iface);
    GstBuffer *gst_buffer;
    HRESULT hr = S_OK;
    int ret;

    TRACE("%p, %u, %p, %#x.\n", iface, id, sample, flags);

    if (flags)
        WARN("Unsupported flags %#x\n", flags);

    if (id != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    EnterCriticalSection(&converter->cs);

    if (!converter->valid_state)
    {
        hr = MF_E_TRANSFORM_TYPE_NOT_SET;
        goto done;
    }

    if (converter->inflight)
    {
        hr = MF_E_NOTACCEPTING;
        goto done;
    }

    if (!(gst_buffer = gst_buffer_from_mf_sample(sample)))
    {
        hr = E_FAIL;
        goto done;
    }

    g_signal_emit_by_name(converter->appsrc, "push-buffer", gst_buffer, &ret);
    gst_buffer_unref(gst_buffer);
    if (ret != GST_FLOW_OK)
    {
        ERR("Couldn't push buffer ret = %d\n", ret);
        hr = E_FAIL;
        goto done;
    }

    converter->inflight = TRUE;

    done:
    LeaveCriticalSection(&converter->cs);

    return hr;
}

static HRESULT WINAPI color_converter_ProcessOutput(IMFTransform *iface, DWORD flags, DWORD count,
        MFT_OUTPUT_DATA_BUFFER *samples, DWORD *status)
{
    struct color_converter *converter = impl_color_converter_from_IMFTransform(iface);
    MFT_OUTPUT_DATA_BUFFER *relevant_buffer = NULL;
    GstSample *sample;
    HRESULT hr = S_OK;

    TRACE("%p, %#x, %u, %p, %p.\n", iface, flags, count, samples, status);

    if (flags)
        WARN("Unsupported flags %#x\n", flags);

    for (unsigned int i = 0; i < count; i++)
    {
        MFT_OUTPUT_DATA_BUFFER *out_buffer = &samples[i];

        if (out_buffer->dwStreamID != 0)
            return MF_E_INVALIDSTREAMNUMBER;

        if (relevant_buffer)
            return MF_E_INVALIDSTREAMNUMBER;

        relevant_buffer = out_buffer;
    }

    if (!relevant_buffer)
        return S_OK;

    EnterCriticalSection(&converter->cs);

    if (!converter->valid_state)
    {
        hr = MF_E_TRANSFORM_TYPE_NOT_SET;
        goto done;
    }

    if (!converter->inflight)
    {
        hr = MF_E_TRANSFORM_NEED_MORE_INPUT;
        goto done;
    }

    g_signal_emit_by_name(converter->appsink, "pull-sample", &sample);

    converter->inflight =  FALSE;

    relevant_buffer->pSample = mf_sample_from_gst_buffer(gst_sample_get_buffer(sample));
    gst_sample_unref(sample);
    relevant_buffer->dwStatus = S_OK;
    relevant_buffer->pEvents = NULL;
    *status = 0;

    done:
    LeaveCriticalSection(&converter->cs);

    return hr;
}

static const IMFTransformVtbl color_converter_vtbl =
{
    color_converter_QueryInterface,
    color_converter_AddRef,
    color_converter_Release,
    color_converter_GetStreamLimits,
    color_converter_GetStreamCount,
    color_converter_GetStreamIDs,
    color_converter_GetInputStreamInfo,
    color_converter_GetOutputStreamInfo,
    color_converter_GetAttributes,
    color_converter_GetInputStreamAttributes,
    color_converter_GetOutputStreamAttributes,
    color_converter_DeleteInputStream,
    color_converter_AddInputStreams,
    color_converter_GetInputAvailableType,
    color_converter_GetOutputAvailableType,
    color_converter_SetInputType,
    color_converter_SetOutputType,
    color_converter_GetInputCurrentType,
    color_converter_GetOutputCurrentType,
    color_converter_GetInputStatus,
    color_converter_GetOutputStatus,
    color_converter_SetOutputBounds,
    color_converter_ProcessEvent,
    color_converter_ProcessMessage,
    color_converter_ProcessInput,
    color_converter_ProcessOutput,
};

HRESULT color_converter_create(REFIID riid, void **ret)
{
    struct color_converter *object;
    HRESULT hr;

    TRACE("%s %p\n", debugstr_guid(riid), ret);

    if (!(object = heap_alloc_zero(sizeof(*object))))
        return E_OUTOFMEMORY;

    object->IMFTransform_iface.lpVtbl = &color_converter_vtbl;
    object->refcount = 1;

    InitializeCriticalSection(&object->cs);

    if (FAILED(hr = MFCreateAttributes(&object->attributes, 0)))
        goto failed;

    if (FAILED(hr = MFCreateAttributes(&object->output_attributes, 0)))
        goto failed;

    object->container = gst_bin_new(NULL);
    object->bus = gst_bus_new();
    gst_element_set_bus(object->container, object->bus);

    if (!(object->appsrc = gst_element_factory_make("appsrc", NULL)))
    {
        ERR("Failed to create appsrc");
        hr = E_FAIL;
        goto failed;
    }
    gst_bin_add(GST_BIN(object->container), object->appsrc);

    if (!(object->videoconvert = gst_element_factory_make("videoconvert", NULL)))
    {
        ERR("Failed to create converter\n");
        hr = E_FAIL;
        goto failed;
    }
    gst_bin_add(GST_BIN(object->container), object->videoconvert);

    if (!(object->appsink = gst_element_factory_make("appsink", NULL)))
    {
        ERR("Failed to create appsink\n");
        hr = E_FAIL;
        goto failed;
    }
    gst_bin_add(GST_BIN(object->container), object->appsink);

    if (!(gst_element_link(object->appsrc, object->videoconvert)))
    {
        ERR("Failed to link appsrc to videoconvert\n");
        hr = E_FAIL;
        goto failed;
    }

    if (!(gst_element_link(object->videoconvert, object->appsink)))
    {
        ERR("Failed to link videoconvert to appsink\n");
        hr = E_FAIL;
        goto failed;
    }

    *ret = &object->IMFTransform_iface;
    return S_OK;

failed:

    IMFTransform_Release(&object->IMFTransform_iface);
    return hr;
}