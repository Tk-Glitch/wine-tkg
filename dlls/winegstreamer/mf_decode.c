#include "config.h"

#include <gst/gst.h>

#include "gst_private.h"
#include "gst_cbs.h"

#include <stdarg.h>

#define COBJMACROS
#define NONAMELESSUNION

#include "mfapi.h"
#include "mferror.h"
#include "mfobjects.h"
#include "mftransform.h"

#include "wine/debug.h"
#include "wine/heap.h"

WINE_DEFAULT_DEBUG_CHANNEL(mfplat);

/* keep in sync with mfplat.c's mft registrations */

const GUID *h264_input_types[] = {&MFVideoFormat_H264};
const GUID *h264_output_types[] = {&MFVideoFormat_NV12, &MFVideoFormat_I420, &MFVideoFormat_IYUV, &MFVideoFormat_YUY2, &MFVideoFormat_YV12};

const GUID *aac_input_types[] = {&MFAudioFormat_AAC};
const GUID *aac_output_types[] = {&MFAudioFormat_Float, &MFAudioFormat_PCM};

const GUID *wmv_input_types[] = {&MFVideoFormat_WMV3, &MFVideoFormat_WVC1};
const GUID *wmv_output_types[] = {&MFVideoFormat_NV12, &MFVideoFormat_YV12, &MFVideoFormat_YUY2, &MFVideoFormat_UYVY, &MFVideoFormat_YVYU, &MFVideoFormat_NV11, &MFVideoFormat_RGB32, &MFVideoFormat_RGB24, &MFVideoFormat_RGB555, &MFVideoFormat_RGB8};

const GUID *wma_input_types[] = {&MFAudioFormat_WMAudioV8, &MFAudioFormat_WMAudioV9};
const GUID *wma_output_types[] = {&MFAudioFormat_Float, &MFAudioFormat_PCM};

const GUID *m4s2_input_types[] = {&MFVideoFormat_MPEG2};
const GUID *m4s2_output_types[] = {&MFVideoFormat_I420, &MFVideoFormat_IYUV, &MFVideoFormat_NV12, &MFVideoFormat_YUY2, &MFVideoFormat_YV12};

static struct decoder_desc
{
    const GUID *major_type;
    const GUID **input_types;
    unsigned int input_types_count;
    const GUID **output_types;
    unsigned int output_types_count;
} decoder_descs[] =
{
    { /* DECODER_TYPE_H264 */
        &MFMediaType_Video,
        h264_input_types,
        ARRAY_SIZE(h264_input_types),
        h264_output_types,
        ARRAY_SIZE(h264_output_types),
    },
    { /* DECODER_TYPE_AAC */
        &MFMediaType_Audio,
        aac_input_types,
        ARRAY_SIZE(aac_input_types),
        aac_output_types,
        ARRAY_SIZE(aac_output_types),
    },
    { /* DECODER_TYPE_WMV */
        &MFMediaType_Video,
        wmv_input_types,
        ARRAY_SIZE(wmv_input_types),
        wmv_output_types,
        ARRAY_SIZE(wmv_output_types),
    },
    { /* DECODER_TYPE_WMA */
        &MFMediaType_Audio,
        wma_input_types,
        ARRAY_SIZE(wma_input_types),
        wma_output_types,
        ARRAY_SIZE(wma_output_types),
    },
    { /* DECODER_TYPE_M4S2 */
        &MFMediaType_Video,
        m4s2_input_types,
        ARRAY_SIZE(m4s2_input_types),
        m4s2_output_types,
        ARRAY_SIZE(m4s2_output_types),
    }
};

struct mf_decoder
{
    IMFTransform IMFTransform_iface;
    IMFAsyncCallback process_message_callback;
    LONG refcount;
    enum decoder_type type;
    BOOL video;
    IMFMediaType *input_type, *output_type;
    BOOL valid_state;
    GstBus *bus;
    GstElement *container;
    GstElement *parser, *decoder, *post_process_start, *videobox, *appsink;
    GstPad *input_src, *their_sink;
    unsigned int output_counter;
    BOOL flushing, draining;
    CRITICAL_SECTION state_cs;
    CONDITION_VARIABLE state_cv;
    DWORD message_queue;
};

static struct mf_decoder *impl_mf_decoder_from_IMFTransform(IMFTransform *iface)
{
    return CONTAINING_RECORD(iface, struct mf_decoder, IMFTransform_iface);
}

static struct mf_decoder *impl_from_message_callback_IMFAsyncCallback(IMFAsyncCallback *iface)
{
    return CONTAINING_RECORD(iface, struct mf_decoder, process_message_callback);
}

static HRESULT WINAPI mf_decoder_QueryInterface (IMFTransform *iface, REFIID riid, void **out)
{
    TRACE("%p, %s, %p.\n", iface, debugstr_guid(riid), out);

    if (IsEqualIID(riid, &IID_IMFTransform) ||
            IsEqualIID(riid, &IID_IUnknown))
    {
        *out = iface;
        IMFTransform_AddRef(iface);
        return S_OK;
    }

    WARN("Unsupported %s.\n", debugstr_guid(riid));
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI mf_decoder_AddRef(IMFTransform *iface)
{
    struct mf_decoder *decoder = impl_mf_decoder_from_IMFTransform(iface);
    ULONG refcount = InterlockedIncrement(&decoder->refcount);

    TRACE("%p, refcount %u.\n", iface, refcount);

    return refcount;
}

static void mf_decoder_destroy(struct mf_decoder *decoder);
static ULONG WINAPI mf_decoder_Release(IMFTransform *iface)
{
    struct mf_decoder *decoder = impl_mf_decoder_from_IMFTransform(iface);
    ULONG refcount = InterlockedDecrement(&decoder->refcount);

    TRACE("%p, refcount %u.\n", iface, refcount);

    if (!refcount)
    {
        mf_decoder_destroy(decoder);
    }

    return refcount;
}

static HRESULT WINAPI mf_decoder_GetStreamLimits(IMFTransform *iface, DWORD *input_minimum, DWORD *input_maximum,
        DWORD *output_minimum, DWORD *output_maximum)
{
    TRACE("%p, %p, %p, %p, %p.\n", iface, input_minimum, input_maximum, output_minimum, output_maximum);

    *input_minimum = *input_maximum = *output_minimum = *output_maximum = 1;

    return S_OK;
}

static HRESULT WINAPI mf_decoder_GetStreamCount(IMFTransform *iface, DWORD *inputs, DWORD *outputs)
{
    TRACE("%p %p %p.\n", iface, inputs, outputs);

    *inputs = *outputs = 1;

    return S_OK;
}

static HRESULT WINAPI mf_decoder_GetStreamIDs(IMFTransform *iface, DWORD input_size, DWORD *inputs,
        DWORD output_size, DWORD *outputs)
{
    TRACE("%p %u %p %u %p.\n", iface, input_size, inputs, output_size, outputs);

    return E_NOTIMPL;
}


static HRESULT WINAPI mf_decoder_GetInputStreamInfo(IMFTransform *iface, DWORD id, MFT_INPUT_STREAM_INFO *info)
{
    struct mf_decoder *decoder = impl_mf_decoder_from_IMFTransform(iface);

    TRACE("%p %u %p\n", decoder, id, info);

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

static HRESULT WINAPI mf_decoder_GetOutputStreamInfo(IMFTransform *iface, DWORD id, MFT_OUTPUT_STREAM_INFO *info)
{
    struct mf_decoder *decoder = impl_mf_decoder_from_IMFTransform(iface);
    MFT_OUTPUT_STREAM_INFO stream_info = {};

    TRACE("%p %u %p\n", decoder, id, info);

    if (id != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    stream_info.dwFlags = MFT_OUTPUT_STREAM_PROVIDES_SAMPLES;
    stream_info.cbSize = 0;
    stream_info.cbAlignment = 0;

    *info = stream_info;

    return S_OK;
}

static HRESULT WINAPI mf_decoder_GetAttributes(IMFTransform *iface, IMFAttributes **attributes)
{
    FIXME("%p, %p. semi-stub!\n", iface, attributes);

    return MFCreateAttributes(attributes, 0);
}

static HRESULT WINAPI mf_decoder_GetInputStreamAttributes(IMFTransform *iface, DWORD id,
        IMFAttributes **attributes)
{
    FIXME("%p, %u, %p. stub!\n", iface, id, attributes);

    return E_NOTIMPL;
}

static HRESULT WINAPI mf_decoder_GetOutputStreamAttributes(IMFTransform *iface, DWORD id,
        IMFAttributes **attributes)
{
    FIXME("%p, %u, %p. stub!\n", iface, id, attributes);

    return E_NOTIMPL;
}

static HRESULT WINAPI mf_decoder_DeleteInputStream(IMFTransform *iface, DWORD id)
{
    FIXME("%p, %u. stub!\n", iface, id);

    return E_NOTIMPL;
}

static HRESULT WINAPI mf_decoder_AddInputStreams(IMFTransform *iface, DWORD streams, DWORD *ids)
{
    FIXME("%p, %u, %p. stub!\n", iface, streams, ids);

    return E_NOTIMPL;
}

static HRESULT WINAPI mf_decoder_GetInputAvailableType(IMFTransform *iface, DWORD id, DWORD index,
        IMFMediaType **type)
{
    struct mf_decoder *decoder = impl_mf_decoder_from_IMFTransform(iface);
    IMFMediaType *input_type;
    HRESULT hr;

    TRACE("%p, %u, %u, %p\n", decoder, id, index, type);

    if (id != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    if (index >= decoder_descs[decoder->type].input_types_count)
        return MF_E_NO_MORE_TYPES;

    if (FAILED(hr = MFCreateMediaType(&input_type)))
        return hr;

    if (FAILED(hr = IMFMediaType_SetGUID(input_type, &MF_MT_MAJOR_TYPE, decoder_descs[decoder->type].major_type)))
    {
        IMFMediaType_Release(input_type);
        return hr;
    }

    if (FAILED(hr = IMFMediaType_SetGUID(input_type, &MF_MT_SUBTYPE, decoder_descs[decoder->type].input_types[index])))
    {
        IMFMediaType_Release(input_type);
        return hr;
    }

    *type = input_type;

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

static HRESULT WINAPI mf_decoder_GetOutputAvailableType(IMFTransform *iface, DWORD id, DWORD index,
        IMFMediaType **type)
{
    struct mf_decoder *decoder = impl_mf_decoder_from_IMFTransform(iface);
    IMFMediaType *output_type;
    HRESULT hr;

    TRACE("%p, %u, %u, %p\n", decoder, id, index, type);

    if (id != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    if (!(decoder->input_type))
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    if (index >= decoder_descs[decoder->type].output_types_count)
        return MF_E_NO_MORE_TYPES;

    if (FAILED(hr = MFCreateMediaType(&output_type)))
        return hr;

    copy_attr(output_type, decoder->input_type, &MF_MT_FRAME_SIZE);
    copy_attr(output_type, decoder->input_type, &MF_MT_FRAME_RATE);
    copy_attr(output_type, decoder->input_type, &MF_MT_AUDIO_NUM_CHANNELS);
    copy_attr(output_type, decoder->input_type, &MF_MT_AUDIO_SAMPLES_PER_SECOND);

    if (FAILED(hr = IMFMediaType_SetGUID(output_type, &MF_MT_MAJOR_TYPE, decoder_descs[decoder->type].major_type)))
    {
        IMFMediaType_Release(output_type);
        return hr;
    }

    if (FAILED(hr = IMFMediaType_SetGUID(output_type, &MF_MT_SUBTYPE, decoder_descs[decoder->type].output_types[index])))
    {
        IMFMediaType_Release(output_type);
        return hr;
    }

    if (IsEqualGUID(decoder_descs[decoder->type].output_types[index], &MFAudioFormat_PCM))
    {
        IMFMediaType_SetUINT32(output_type, &MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    }

    *type = output_type;

    return S_OK;
}

static gboolean activate_push_mode(GstPad *pad, GstObject *parent, GstPadMode mode, gboolean activate)
{
    TRACE("%s mft input pad in %s mode.\n",
            activate ? "Activating" : "Deactivating", gst_pad_mode_get_name(mode));

    switch (mode) {
        case GST_PAD_MODE_PUSH:
            return TRUE;
        default:
            return FALSE;
    }
}

static gboolean query_input_src(GstPad *pad, GstObject *parent, GstQuery *query)
{
    struct mf_decoder *decoder = gst_pad_get_element_private(pad);

    TRACE("GStreamer queries MFT Input Pad %p for %s\n", decoder, GST_QUERY_TYPE_NAME(query));

    switch (GST_QUERY_TYPE(query))
    {
        case GST_QUERY_CAPS:
        {
            gst_query_set_caps_result(query, caps_from_mf_media_type(decoder->input_type));
            return TRUE;
        }
        case GST_QUERY_SCHEDULING:
        {
            gst_query_add_scheduling_mode(query, GST_PAD_MODE_PUSH);
            return TRUE;
        }
        case GST_QUERY_SEEKING:
        {
            GstFormat format;
            gboolean seekable;
            gint64 segment_start, segment_end;

            gst_query_parse_seeking(query, &format, &seekable, &segment_start, &segment_end);
            gst_query_set_seeking(query, format, 0, segment_start, segment_end);
            return TRUE;
        }
        case GST_QUERY_DURATION:
        {
            return FALSE;
        }
        case GST_QUERY_LATENCY:
        {
            return FALSE;
        }
        default:
        {
            ERR("Unhandled query type %s on MFT Input Pad %p\n", GST_QUERY_TYPE_NAME(query), decoder);
            return gst_pad_query_default (pad, parent, query);
        }
    }
}

static GstFlowReturn decoder_new_sample(GstElement *appsink, gpointer user)
{
    struct mf_decoder *decoder = (struct mf_decoder *) user;

    TRACE("new sample decoder=%p appsink=%p\n", decoder, appsink);

    if (decoder->flushing)
    {
        GstSample *sample;
        g_signal_emit_by_name(decoder->appsink, "pull-sample", &sample);
        gst_sample_unref(sample);
        return GST_FLOW_OK;
    }

    decoder->output_counter++;

    return GST_FLOW_OK;
}

static BOOL find_decoder_from_caps(GstCaps *input_caps, GstElement **decoder, GstElement **parser)
{
    GList *parser_list_one, *parser_list_two;
    GList *walk;
    BOOL ret = TRUE;

    TRACE("input caps: %s\n", gst_caps_to_string(input_caps));

    parser_list_one = gst_element_factory_list_get_elements(GST_ELEMENT_FACTORY_TYPE_PARSER, 1);
    parser_list_two = gst_element_factory_list_filter(parser_list_one, input_caps, GST_PAD_SINK, 0);
    gst_plugin_feature_list_free(parser_list_one);
    parser_list_one = parser_list_two;
    if (!(g_list_length(parser_list_one)))
    {
        GList *decoder_list_one, *decoder_list_two;
        decoder_list_one = gst_element_factory_list_get_elements(GST_ELEMENT_FACTORY_TYPE_DECODER, 1);
        decoder_list_two = gst_element_factory_list_filter(decoder_list_one, input_caps, GST_PAD_SINK, 0);
        gst_plugin_feature_list_free(decoder_list_one);
        decoder_list_one = decoder_list_two;
        if (!(g_list_length(decoder_list_one)) ||
            !(*decoder = gst_element_factory_create(g_list_first(decoder_list_one)->data, NULL)))
        {
            gst_plugin_feature_list_free(decoder_list_one);
            ERR("Failed to create decoder\n");
            ret = FALSE;
            goto done;
        }
        TRACE("Found decoder %s\n", GST_ELEMENT_NAME(g_list_first(decoder_list_one)->data));
    }
    else
    {
        for (walk = (GList *) parser_list_one; walk; walk = g_list_next(walk))
        {
            GstElementFactory *parser_factory = walk->data;
            const GList *templates, *walk_templ;

            templates = gst_element_factory_get_static_pad_templates(parser_factory);

            for (walk_templ = (GList *)templates; walk_templ; walk_templ = g_list_next(walk_templ))
            {
                GList *decoder_list_one, *decoder_list_two;
                GstStaticPadTemplate *templ = walk_templ->data;
                GstCaps *templ_caps;

                if (templ->direction != GST_PAD_SRC)
                    continue;

                templ_caps = gst_static_pad_template_get_caps(templ);

                TRACE("Matching parser src caps %s to decoder.\n", gst_caps_to_string(templ_caps));

                decoder_list_one = gst_element_factory_list_get_elements(GST_ELEMENT_FACTORY_TYPE_DECODER, 1);
                decoder_list_two = gst_element_factory_list_filter(decoder_list_one, templ_caps, GST_PAD_SINK, 0);
                gst_plugin_feature_list_free(decoder_list_one);
                decoder_list_one = decoder_list_two;
                gst_caps_unref(templ_caps);

                if (!(g_list_length(decoder_list_one)))
                    continue;

                if (!(*parser = gst_element_factory_create(parser_factory, NULL)))
                {
                    gst_plugin_feature_list_free(decoder_list_one);
                    ERR("Failed to create parser\n");
                    ret = FALSE;
                    goto done;
                }

                if (!(*decoder = gst_element_factory_create(g_list_first(decoder_list_one)->data, NULL)))
                {
                    gst_plugin_feature_list_free(decoder_list_one);
                    ERR("Failed to create decoder\n");
                    ret = FALSE;
                    goto done;
                }

                TRACE("Found decoder %s parser %s\n",
                GST_ELEMENT_NAME(g_list_first(decoder_list_one)->data), GST_ELEMENT_NAME(parser_factory));
                gst_plugin_feature_list_free(decoder_list_one);

                goto done;
            }
        }
    }
    done:
    gst_plugin_feature_list_free(parser_list_one);
    return ret;
}

static void decoder_update_pipeline(struct mf_decoder *decoder)
{
    GstCaps *input_caps = NULL;
    RECT target_size = {0};
    MFVideoArea *aperture;
    UINT32 aperture_size;
    GstSegment *segment;

    decoder->valid_state = FALSE;

    /* tear down current pipeline */
    gst_element_set_state(decoder->container, GST_STATE_READY);
    if (gst_element_get_state(decoder->container, NULL, NULL, -1) == GST_STATE_CHANGE_FAILURE)
    {
        ERR("Failed to stop container\n");
    }

    g_object_set(decoder->appsink, "caps", gst_caps_new_empty(), NULL);

    if (decoder->input_src)
    {
        gst_pad_unlink(decoder->input_src, decoder->their_sink);
        gst_object_unref(G_OBJECT(decoder->input_src));
        decoder->input_src = NULL;
    }

    if (decoder->their_sink)
    {
        gst_object_unref(G_OBJECT(decoder->their_sink));
        decoder->their_sink = NULL;
    }

    if (decoder->parser)
    {
        gst_element_unlink(decoder->parser, decoder->decoder);
        gst_bin_remove(GST_BIN(decoder->container), decoder->parser);
        decoder->parser = NULL;
    }
    if (decoder->decoder)
    {
        gst_element_unlink(decoder->decoder, decoder->post_process_start);
        gst_bin_remove(GST_BIN(decoder->container), decoder->decoder);
        decoder->decoder = NULL;
    }

    /* we can only have a valid state if an input and output type is present */
    if (!decoder->input_type || !decoder->output_type)
        return;

    /* We do leave a lot of unfreed objects here when we failure,
       but it will be cleaned up on the next call */

    input_caps = caps_from_mf_media_type(decoder->input_type);

    if (!(decoder->input_src = gst_pad_new_from_template(gst_pad_template_new(
        "mf_src",
        GST_PAD_SRC,
        GST_PAD_ALWAYS,
        input_caps
        ), "input_src")))
    {
        ERR("Failed to create input source\n");
        goto done;
    }

    gst_pad_set_activatemode_function(decoder->input_src, activate_push_mode_wrapper);
    gst_pad_set_query_function(decoder->input_src, query_input_src_wrapper);
    gst_pad_set_element_private(decoder->input_src, decoder);

    if (!(find_decoder_from_caps(input_caps, &decoder->decoder, &decoder->parser)))
    {
        goto done;
    }

    gst_bin_add(GST_BIN(decoder->container), decoder->decoder);
    if (decoder->parser)
    {
        gst_bin_add(GST_BIN(decoder->container), decoder->parser);
    }

    if (!(decoder->their_sink = gst_element_get_static_pad(decoder->parser ? decoder->parser : decoder->decoder, "sink")))
    {
        goto done;
    }

    if (SUCCEEDED(IMFMediaType_GetAllocatedBlob(decoder->output_type, &MF_MT_MINIMUM_DISPLAY_APERTURE, (UINT8 **) &aperture, &aperture_size)))
    {
        UINT64 frame_size;

        TRACE("x: %u %u/65536, y: %u %u/65536, area: %u x %u\n", aperture->OffsetX.value, aperture->OffsetX.fract,
            aperture->OffsetY.value, aperture->OffsetY.fract, aperture->Area.cx, aperture->Area.cy);

        if (SUCCEEDED(IMFMediaType_GetUINT64(decoder->output_type, &MF_MT_FRAME_SIZE, &frame_size)))
        {
            DWORD width = frame_size >> 32;
            DWORD height = frame_size;

            target_size.left = -aperture->OffsetX.value;
            target_size.top = -aperture->OffsetY.value;
            target_size.right = aperture->Area.cx - width;
            target_size.bottom = aperture->Area.cy - height;
        }
        else
            ERR("missing frame size\n");

        CoTaskMemFree(aperture);
    }

    if (decoder->videobox)
    {
        g_object_set(decoder->videobox, "top", target_size.top, NULL);
        g_object_set(decoder->videobox, "bottom", target_size.bottom, NULL);
        g_object_set(decoder->videobox, "left", target_size.left, NULL);
        g_object_set(decoder->videobox, "right", target_size.right, NULL);
    }

    g_object_set(decoder->appsink, "caps", caps_from_mf_media_type(decoder->output_type), NULL);

    if (gst_pad_link(decoder->input_src, decoder->their_sink) != GST_PAD_LINK_OK)
    {
        ERR("Failed to link input source to decoder sink\n");
        return;
    }

    if (decoder->parser && !(gst_element_link(decoder->parser, decoder->decoder)))
    {
        ERR("Failed to link parser to decoder\n");
        goto done;
    }

    if (!(gst_element_link(decoder->decoder, decoder->post_process_start)))
    {
        ERR("Failed to link decoder to first element in post processing chain\n");
        goto done;
    }

    gst_element_set_state(decoder->container, GST_STATE_PLAYING);

    gst_pad_set_active(decoder->input_src, 1);
    gst_pad_push_event(decoder->input_src, gst_event_new_stream_start("decoder-stream"));
    gst_pad_push_event(decoder->input_src, gst_event_new_caps(caps_from_mf_media_type(decoder->input_type)));
    segment = gst_segment_new();
    gst_segment_init(segment, GST_FORMAT_TIME);
    gst_pad_push_event(decoder->input_src, gst_event_new_segment(segment));

    gst_element_get_state(decoder->container, NULL, NULL, -1);

    decoder->valid_state = TRUE;
    done:
    if (input_caps)
        gst_caps_unref(input_caps);
    return;
}

static HRESULT WINAPI mf_decoder_SetInputType(IMFTransform *iface, DWORD id, IMFMediaType *type, DWORD flags)
{
    struct mf_decoder *decoder = impl_mf_decoder_from_IMFTransform(iface);
    HRESULT hr = S_OK;

    TRACE("%p, %u, %p, %#x\n", decoder, id, type, flags);

    if (id != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    if (type)
    {
        GUID major_type, subtype;
        UINT64 unused;

        if (FAILED(hr = IMFMediaType_GetGUID(type, &MF_MT_MAJOR_TYPE, &major_type)))
            return hr;
        if (FAILED(hr = IMFMediaType_GetGUID(type, &MF_MT_SUBTYPE, &subtype)))
            return hr;

        if (!(IsEqualGUID(&major_type, decoder_descs[decoder->type].major_type)))
            return MF_E_INVALIDTYPE;

        if (decoder->video)
        {
            if (FAILED(hr = IMFMediaType_GetUINT64(type, &MF_MT_FRAME_SIZE, &unused)))
                return hr;
        }

        for (unsigned int i = 0; i < decoder_descs[decoder->type].input_types_count; i++)
        {
            if (IsEqualGUID(&subtype, decoder_descs[decoder->type].input_types[i]))
                break;
            if (i == decoder_descs[decoder->type].input_types_count)
                return MF_E_INVALIDTYPE;
        }
    }

    if (flags & MFT_SET_TYPE_TEST_ONLY)
    {
        return S_OK;
    }

    EnterCriticalSection(&decoder->state_cs);

    if (type)
    {
        if (!decoder->input_type)
            if (FAILED(hr = MFCreateMediaType(&decoder->input_type)))
                goto done;

        if (FAILED(hr = IMFMediaType_CopyAllItems(type, (IMFAttributes*) decoder->input_type)))
            goto done;
    }
    else if (decoder->input_type)
    {
        IMFMediaType_Release(decoder->input_type);
        decoder->input_type = NULL;
    }

    decoder_update_pipeline(decoder);

    done:
    LeaveCriticalSection(&decoder->state_cs);
    WakeAllConditionVariable(&decoder->state_cv);
    return hr;
}

static HRESULT WINAPI mf_decoder_SetOutputType(IMFTransform *iface, DWORD id, IMFMediaType *type, DWORD flags)
{
    struct mf_decoder *decoder = impl_mf_decoder_from_IMFTransform(iface);
    HRESULT hr = S_OK;

    TRACE("%p, %u, %p, %#x\n", decoder, id, type, flags);

    if (id != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    if (type)
    {
        GUID major_type, subtype;
        UINT64 unused;

        /* validate the type */
        if (FAILED(hr = IMFMediaType_GetGUID(type, &MF_MT_MAJOR_TYPE, &major_type)))
            return MF_E_INVALIDTYPE;
        if (FAILED(hr = IMFMediaType_GetGUID(type, &MF_MT_SUBTYPE, &subtype)))
            return MF_E_INVALIDTYPE;

        if (!(IsEqualGUID(&major_type, decoder_descs[decoder->type].major_type)))
            return MF_E_INVALIDTYPE;

        if (decoder->video)
        {
            if (FAILED(hr = IMFMediaType_GetUINT64(type, &MF_MT_FRAME_SIZE, &unused)))
                return MF_E_INVALIDTYPE;
        }

        for (unsigned int i = 0; i < decoder_descs[decoder->type].output_types_count; i++)
        {
            if (IsEqualGUID(&subtype, decoder_descs[decoder->type].output_types[i]))
                break;
            if (i == decoder_descs[decoder->type].output_types_count)
                return MF_E_INVALIDTYPE;
        }
    }

    if (flags & MFT_SET_TYPE_TEST_ONLY)
    {
        return S_OK;
    }

    EnterCriticalSection(&decoder->state_cs);
    if (type)
    {
        if (!decoder->output_type)
            if (FAILED(hr = MFCreateMediaType(&decoder->output_type)))
                goto done;

        if (FAILED(hr = IMFMediaType_CopyAllItems(type, (IMFAttributes*) decoder->output_type)))
            goto done;
    }
    else if (decoder->output_type)
    {
        IMFMediaType_Release(decoder->output_type);
        decoder->output_type = NULL;
    }

    decoder_update_pipeline(decoder);

    done:
    LeaveCriticalSection(&decoder->state_cs);
    WakeAllConditionVariable(&decoder->state_cv);
    return hr;
}

static HRESULT WINAPI mf_decoder_GetInputCurrentType(IMFTransform *iface, DWORD id, IMFMediaType **type)
{
    FIXME("%p, %u, %p. stub!\n", iface, id, type);

    return E_NOTIMPL;
}

static HRESULT WINAPI mf_decoder_GetOutputCurrentType(IMFTransform *iface, DWORD id, IMFMediaType **type)
{
    FIXME("%p, %u, %p. stub!\n", iface, id, type);

    return E_NOTIMPL;
}

static HRESULT WINAPI mf_decoder_GetInputStatus(IMFTransform *iface, DWORD id, DWORD *flags)
{
    struct mf_decoder *decoder = impl_mf_decoder_from_IMFTransform(iface);

    TRACE("%p, %u, %p\n", decoder, id, flags);

    *flags = decoder->output_counter ? MFT_INPUT_STATUS_ACCEPT_DATA : 0;

    return S_OK;
}

static HRESULT WINAPI mf_decoder_GetOutputStatus(IMFTransform *iface, DWORD *flags)
{
    struct mf_decoder *decoder = impl_mf_decoder_from_IMFTransform(iface);

    TRACE("%p, %p.\n", decoder, flags);

    *flags = decoder->output_counter ? MFT_OUTPUT_STATUS_SAMPLE_READY : 0;

    return S_OK;
}

static HRESULT WINAPI mf_decoder_SetOutputBounds(IMFTransform *iface, LONGLONG lower, LONGLONG upper)
{
    FIXME("%p, %s, %s. stub!\n", iface, wine_dbgstr_longlong(lower), wine_dbgstr_longlong(upper));

    return E_NOTIMPL;
}

static HRESULT WINAPI mf_decoder_ProcessEvent(IMFTransform *iface, DWORD id, IMFMediaEvent *event)
{
    FIXME("%p, %u, %p. stub!\n", iface, id, event);

    return E_NOTIMPL;
}

static HRESULT WINAPI decoder_process_message_callback_QueryInterface(IMFAsyncCallback *iface,
        REFIID riid, void **obj)
{
    TRACE("%p, %s, %p.\n", iface, debugstr_guid(riid), obj);

    if (IsEqualIID(riid, &IID_IMFAsyncCallback) ||
            IsEqualIID(riid, &IID_IUnknown))
    {
        *obj = iface;
        IMFAsyncCallback_AddRef(iface);
        return S_OK;
    }

    WARN("Unsupported %s.\n", debugstr_guid(riid));
    *obj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI decoder_process_message_callback_AddRef(IMFAsyncCallback *iface)
{
    struct mf_decoder *decoder = impl_from_message_callback_IMFAsyncCallback(iface);
    return IMFTransform_AddRef(&decoder->IMFTransform_iface);
}

static ULONG WINAPI decoder_process_message_callback_Release(IMFAsyncCallback *iface)
{
    struct mf_decoder *decoder = impl_from_message_callback_IMFAsyncCallback(iface);
    return IMFTransform_Release(&decoder->IMFTransform_iface);
}

static HRESULT WINAPI decoder_process_message_callback_GetParameters(IMFAsyncCallback *iface,
        DWORD *flags, DWORD *queue)
{
    return E_NOTIMPL;
}

const GUID WINE_MFT_MESSAGE_TYPE = {0xd09998bf, 0x102f, 0x4efa, {0x8f,0x84,0x06,0x1f,0xa4,0x10,0xf2,0x64}};

static HRESULT WINAPI decoder_process_message_callback_Invoke(IMFAsyncCallback *iface, IMFAsyncResult *result)
{
    struct mf_decoder *decoder = impl_from_message_callback_IMFAsyncCallback(iface);
    IUnknown *state;
    IMFAttributes *async_param;
    MFT_MESSAGE_TYPE message_type;
    HRESULT hr;

    state = IMFAsyncResult_GetStateNoAddRef(result);
    if (!state)
        return E_FAIL;
    if (FAILED(hr = IUnknown_QueryInterface(state, &IID_IMFAttributes, (void **)&async_param)))
        return hr;
    if (FAILED(hr = IMFAttributes_GetUINT32(async_param, &WINE_MFT_MESSAGE_TYPE, &message_type)))
    {
        IMFAttributes_Release(async_param);
        return hr;
    }
    IMFAttributes_Release(async_param);

    switch (message_type)
    {
        case MFT_MESSAGE_COMMAND_DRAIN:
        {
            GstSegment *segment = gst_segment_new();
            gst_segment_init(segment, GST_FORMAT_TIME);

            EnterCriticalSection(&decoder->state_cs);
            decoder->draining = TRUE;
            WakeAllConditionVariable(&decoder->state_cv);
            LeaveCriticalSection(&decoder->state_cs);
            gst_pad_push_event(decoder->input_src, gst_event_new_eos());

            EnterCriticalSection(&decoder->state_cs);
            while(decoder->draining)
                SleepConditionVariableCS(&decoder->state_cv, &decoder->state_cs, INFINITE);
            gst_pad_push_event(decoder->input_src, gst_event_new_flush_stop(0));
            gst_pad_push_event(decoder->input_src, gst_event_new_segment(segment));
            LeaveCriticalSection(&decoder->state_cs);
            return S_OK;
        }
        default:
            return E_FAIL;
    }
}

static const IMFAsyncCallbackVtbl process_message_callback_vtbl =
{
    decoder_process_message_callback_QueryInterface,
    decoder_process_message_callback_AddRef,
    decoder_process_message_callback_Release,
    decoder_process_message_callback_GetParameters,
    decoder_process_message_callback_Invoke,
};

static HRESULT WINAPI mf_decoder_ProcessMessage(IMFTransform *iface, MFT_MESSAGE_TYPE message, ULONG_PTR param)
{
    struct mf_decoder *decoder = impl_mf_decoder_from_IMFTransform(iface);
    IMFAttributes *async_param;
    HRESULT hr;

    TRACE("%p, %x %lu.\n", decoder, message, param);

    if (FAILED(hr = MFCreateAttributes(&async_param, 1)))
        return hr;

    switch (message)
    {
        case MFT_MESSAGE_COMMAND_FLUSH:
        {
            GstSegment *segment = gst_segment_new();
            gst_segment_init(segment, GST_FORMAT_TIME);

            EnterCriticalSection(&decoder->state_cs);
            decoder->flushing = TRUE;

            while (decoder->output_counter)
            {
                GstSample *sample;
                g_signal_emit_by_name(decoder->appsink, "pull-sample", &sample);
                gst_sample_unref(sample);
                decoder->output_counter--;
            }

            gst_pad_push_event(decoder->input_src, gst_event_new_flush_start());
            gst_pad_push_event(decoder->input_src, gst_event_new_flush_stop(0));
            gst_pad_push_event(decoder->input_src, gst_event_new_segment(segment));
            gst_element_set_state(decoder->container, GST_STATE_PLAYING);

            decoder->flushing = FALSE;
            LeaveCriticalSection(&decoder->state_cs);

            hr = S_OK;
            break;
        }
        case MFT_MESSAGE_COMMAND_DRAIN:
        {
            if (decoder->draining)
            {
                hr = S_OK;
                break;
            }

            IMFAttributes_SetUINT32(async_param, &WINE_MFT_MESSAGE_TYPE, message);

            EnterCriticalSection(&decoder->state_cs);
            MFPutWorkItem(decoder->message_queue, &decoder->process_message_callback, (IUnknown *)async_param);
            while (!decoder->draining)
                SleepConditionVariableCS(&decoder->state_cv, &decoder->state_cs, INFINITE);
            LeaveCriticalSection(&decoder->state_cs);

            hr = S_OK;
            break;
        }
        case MFT_MESSAGE_NOTIFY_BEGIN_STREAMING:
        {
            hr = S_OK;
            break;
        }
        default:
        {
            ERR("Unhandled message type %x.\n", message);
            hr = E_FAIL;
            break;
        }
    }

    IMFAttributes_Release(async_param);
    return hr;
}

static HRESULT WINAPI mf_decoder_ProcessInput(IMFTransform *iface, DWORD id, IMFSample *sample, DWORD flags)
{
    struct mf_decoder *decoder = impl_mf_decoder_from_IMFTransform(iface);
    GstBuffer *gst_buffer;
    GstFlowReturn ret;
    HRESULT hr = S_OK;
    GstQuery *drain;

    TRACE("%p, %u, %p, %#x\n", decoder, id, sample, flags);

    if (flags)
        WARN("Unsupported flags %#x\n", flags);

    if (id != 0)
        return MF_E_INVALIDSTREAMNUMBER;

    if (!decoder->valid_state)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

    EnterCriticalSection(&decoder->state_cs);

    drain = gst_query_new_drain();
    gst_pad_peer_query(decoder->input_src, drain);

    if (decoder->output_counter || decoder->draining)
    {
        hr = MF_E_NOTACCEPTING;
        goto done;
    }

    if (!(gst_buffer = gst_buffer_from_mf_sample(sample)))
    {
        hr = E_FAIL;
        goto done;
    }

    ret = gst_pad_push(decoder->input_src, gst_buffer);
    if (ret != GST_FLOW_OK)
    {
        ERR("Couldn't process input ret = %d\n", ret);
        hr =  E_FAIL;
        goto done;
    }

    done:
    LeaveCriticalSection(&decoder->state_cs);
    return hr;
}

static HRESULT WINAPI mf_decoder_ProcessOutput(IMFTransform *iface, DWORD flags, DWORD count,
        MFT_OUTPUT_DATA_BUFFER *samples, DWORD *status)
{
    struct mf_decoder *decoder = impl_mf_decoder_from_IMFTransform(iface);
    MFT_OUTPUT_DATA_BUFFER *relevant_buffer = NULL;
    GstSample *buffer;

    TRACE("%p, %#x, %u, %p, %p,\n", iface, flags, count, samples, status);

    if (flags)
        WARN("Unsupported flags %#x\n", flags);

    if (!decoder->valid_state)
        return MF_E_TRANSFORM_TYPE_NOT_SET;

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

    EnterCriticalSection(&decoder->state_cs);

    if (!decoder->output_counter && !decoder->draining)
    {
        LeaveCriticalSection(&decoder->state_cs);
        return MF_E_TRANSFORM_NEED_MORE_INPUT;
    }
    TRACE("%u\n", decoder->output_counter);

    g_signal_emit_by_name(decoder->appsink, "pull-sample", &buffer);
    if (decoder->draining && !buffer)
    {
        decoder->output_counter = 0;
        decoder->draining = FALSE;
        LeaveCriticalSection(&decoder->state_cs);
        WakeAllConditionVariable(&decoder->state_cv);
        return MF_E_TRANSFORM_NEED_MORE_INPUT;
    }
    decoder->output_counter--;

    LeaveCriticalSection(&decoder->state_cs);

    relevant_buffer->pSample = mf_sample_from_gst_buffer(gst_sample_get_buffer(buffer));
    gst_sample_unref(buffer);
    relevant_buffer->dwStatus = S_OK;
    relevant_buffer->pEvents = NULL;
    *status = 0;
    return S_OK;
}

static const IMFTransformVtbl mf_decoder_vtbl =
{
    mf_decoder_QueryInterface,
    mf_decoder_AddRef,
    mf_decoder_Release,
    mf_decoder_GetStreamLimits,
    mf_decoder_GetStreamCount,
    mf_decoder_GetStreamIDs,
    mf_decoder_GetInputStreamInfo,
    mf_decoder_GetOutputStreamInfo,
    mf_decoder_GetAttributes,
    mf_decoder_GetInputStreamAttributes,
    mf_decoder_GetOutputStreamAttributes,
    mf_decoder_DeleteInputStream,
    mf_decoder_AddInputStreams,
    mf_decoder_GetInputAvailableType,
    mf_decoder_GetOutputAvailableType,
    mf_decoder_SetInputType,
    mf_decoder_SetOutputType,
    mf_decoder_GetInputCurrentType,
    mf_decoder_GetOutputCurrentType,
    mf_decoder_GetInputStatus,
    mf_decoder_GetOutputStatus,
    mf_decoder_SetOutputBounds,
    mf_decoder_ProcessEvent,
    mf_decoder_ProcessMessage,
    mf_decoder_ProcessInput,
    mf_decoder_ProcessOutput,
};

GstBusSyncReply watch_decoder_bus(GstBus *bus, GstMessage *message, gpointer user_data)
{
    struct mf_decoder *decoder = user_data;
    GError *err = NULL;
    gchar *dbg_info = NULL;

    TRACE("decoder %p message type %s\n", decoder, GST_MESSAGE_TYPE_NAME(message));

    switch (message->type)
    {
        case GST_MESSAGE_ERROR:
            gst_message_parse_error(message, &err, &dbg_info);
            ERR("%s: %s\n", GST_OBJECT_NAME(message->src), err->message);
            ERR("%s\n", dbg_info);
            g_error_free(err);
            g_free(dbg_info);
            break;
        case GST_MESSAGE_WARNING:
            gst_message_parse_warning(message, &err, &dbg_info);
            WARN("%s: %s\n", GST_OBJECT_NAME(message->src), err->message);
            WARN("%s\n", dbg_info);
            g_error_free(err);
            g_free(dbg_info);
            break;
        case GST_MESSAGE_EOS:
            break;
        default:
            break;
    }

    return GST_BUS_DROP;
}

static void mf_decoder_destroy(struct mf_decoder *decoder)
{
    if (decoder->input_type)
    {
        IMFMediaType_Release(decoder->input_type);
        decoder->input_type = NULL;
    }

    if (decoder->output_type)
    {
        IMFMediaType_Release(decoder->output_type);
        decoder->output_type = NULL;
    }

    decoder_update_pipeline(decoder);

    if (decoder->their_sink)
        gst_object_unref(G_OBJECT(decoder->their_sink));

    if (decoder->container)
        gst_object_unref(G_OBJECT(decoder->container));

    if (decoder->bus)
        gst_object_unref(G_OBJECT(decoder->bus));

    DeleteCriticalSection(&decoder->state_cs);

    MFUnlockWorkQueue(decoder->message_queue);

    heap_free(decoder);
}

HRESULT generic_decoder_construct(REFIID riid, void **obj, enum decoder_type type)
{
    struct mf_decoder *object;
    GstElement *converter;
    HRESULT hr = S_OK;

    TRACE("%s, %p %u.\n", debugstr_guid(riid), obj, type);

    if (!(object = heap_alloc_zero(sizeof(*object))))
        return E_OUTOFMEMORY;
    object->type = type;
    object->video = decoder_descs[type].major_type == &MFMediaType_Video;
    MFAllocateWorkQueue(&object->message_queue);

    InitializeCriticalSection(&object->state_cs);
    InitializeConditionVariable(&object->state_cv);

    object->container = gst_bin_new(NULL);
    object->bus = gst_bus_new();
    gst_bus_set_sync_handler(object->bus, watch_decoder_bus_wrapper, object, NULL);
    gst_element_set_bus(object->container, object->bus);

    if (object->video)
    {
        if (!(object->videobox = gst_element_factory_make("videobox", NULL)))
        {
            ERR("Failed to create videobox\n");
            hr = E_FAIL;
            goto fail;
        }
        gst_bin_add(GST_BIN(object->container), object->videobox);
    }

    if (!(converter = gst_element_factory_make(object->video ? "videoconvert" : "audioconvert", NULL)))
    {
        ERR("Failed to create converter\n");
        hr = E_FAIL;
        goto fail;
    }
    gst_bin_add(GST_BIN(object->container), converter);

    if (!(object->appsink = gst_element_factory_make("appsink", NULL)))
    {
        ERR("Failed to create appsink\n");
        hr = E_FAIL;
        goto fail;
    }
    gst_bin_add(GST_BIN(object->container), object->appsink);

    g_object_set(object->appsink, "emit-signals", TRUE, NULL);
    g_object_set(object->appsink, "sync", FALSE, NULL);
    g_object_set(object->appsink, "async", FALSE, NULL);
    g_signal_connect(object->appsink, "new-sample", G_CALLBACK(decoder_new_sample_wrapper), object);


    if (object->videobox)
    {
        if (!(gst_element_link(object->videobox, converter)))
        {
            ERR("Failed to link videobox to converter\n");
            hr = E_FAIL;
            goto fail;
        }
    }

    if (!(gst_element_link(converter, object->appsink)))
    {
        ERR("Failed to link converter appsink");
        hr = E_FAIL;
        goto fail;
    }

    object->post_process_start = object->videobox ? object->videobox : converter;

    object->process_message_callback.lpVtbl = &process_message_callback_vtbl;

    object->IMFTransform_iface.lpVtbl = &mf_decoder_vtbl;
    object->refcount = 1;

    *obj = object;
    return S_OK;

    fail:
    ERR("Failed to create Decoder MFT type %u, hr = %#x\n", type, hr);
    mf_decoder_destroy(object);
    return hr;
}

void perform_cb_mf_decode(struct cb_data *cbdata)
{
    switch (cbdata->type)
    {
    case ACTIVATE_PUSH_MODE:
        {
            struct activate_mode_data *data = &cbdata->u.activate_mode_data;
            cbdata->u.activate_mode_data.ret = activate_push_mode(data->pad, data->parent, data->mode, data->activate);
            break;
        }
    case QUERY_INPUT_SRC:
        {
            struct query_function_data *data = &cbdata->u.query_function_data;
            cbdata->u.query_function_data.ret = query_input_src(data->pad, data->parent, data->query);
            break;
        }
    case DECODER_NEW_SAMPLE:
        {
            struct new_sample_data *data = &cbdata->u.new_sample_data;
            cbdata->u.new_sample_data.ret = decoder_new_sample(data->appsink, data->user);
            break;
        }
    case WATCH_DECODER_BUS:
        {
            struct watch_bus_data *data = &cbdata->u.watch_bus_data;
            cbdata->u.watch_bus_data.ret = watch_decoder_bus(data->bus, data->msg, data->user);
            break;
        }
    default:
        {
            ERR("Wrong callback forwarder called\n");
            return;
        }
    }
}