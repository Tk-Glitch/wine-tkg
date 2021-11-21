/*
 * GStreamer parser backend
 *
 * Copyright 2010 Maarten Lankhorst for CodeWeavers
 * Copyright 2010 Aric Stewart for CodeWeavers
 * Copyright 2019-2020 Zebediah Figura
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

#if 0
#pragma makedep unix
#endif

#include "config.h"
#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "gst_private.h"
#include "winternl.h"

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/audio/audio.h>

WINE_DEFAULT_DEBUG_CHANNEL(gstreamer);

GST_DEBUG_CATEGORY_STATIC(wine);
#define GST_CAT_DEFAULT wine

struct wg_parser
{
    BOOL (*init_gst)(struct wg_parser *parser);

    struct wg_parser_stream **streams;
    unsigned int stream_count, expected_stream_count;

    GstElement *container, *decodebin;
    GstBus *bus;
    GstPad *my_src, *their_sink;

    guint64 file_size, start_offset, next_offset, stop_offset;
    guint64 next_pull_offset;

    pthread_t push_thread;

    pthread_mutex_t mutex;

    pthread_cond_t init_cond;
    bool no_more_pads, has_duration, error, pull_mode, seekable;

    pthread_cond_t read_cond, read_done_cond;
    struct
    {
        void *data;
        uint64_t offset;
        uint32_t size;
        bool done;
        enum wg_read_result ret;
    } read_request;

    bool flushing, sink_connected, draining;

    struct wg_format input_format;
};

struct wg_parser_stream
{
    struct wg_parser *parser;

    GstPad *their_src, *post_sink, *post_src, *my_sink;
    GstElement *flip, *box;
    GstSegment segment;
    struct wg_format preferred_format, current_format;
    struct wg_rect aperture;

    pthread_cond_t event_cond, event_empty_cond;
    struct wg_parser_event event;
    GstBuffer *buffer;
    GstMapInfo map_info;

    bool flushing, eos, enabled, has_caps;

    uint64_t duration;
    gchar *language_code;
};

static enum wg_audio_format wg_audio_format_from_gst(GstAudioFormat format)
{
    switch (format)
    {
        case GST_AUDIO_FORMAT_U8:
            return WG_AUDIO_FORMAT_U8;
        case GST_AUDIO_FORMAT_S16LE:
            return WG_AUDIO_FORMAT_S16LE;
        case GST_AUDIO_FORMAT_S24LE:
            return WG_AUDIO_FORMAT_S24LE;
        case GST_AUDIO_FORMAT_S32LE:
            return WG_AUDIO_FORMAT_S32LE;
        case GST_AUDIO_FORMAT_F32LE:
            return WG_AUDIO_FORMAT_F32LE;
        case GST_AUDIO_FORMAT_F64LE:
            return WG_AUDIO_FORMAT_F64LE;
        default:
            return WG_AUDIO_FORMAT_UNKNOWN;
    }
}

static uint32_t wg_channel_position_from_gst(GstAudioChannelPosition position)
{
    static const uint32_t position_map[] =
    {
        SPEAKER_FRONT_LEFT,
        SPEAKER_FRONT_RIGHT,
        SPEAKER_FRONT_CENTER,
        SPEAKER_LOW_FREQUENCY,
        SPEAKER_BACK_LEFT,
        SPEAKER_BACK_RIGHT,
        SPEAKER_FRONT_LEFT_OF_CENTER,
        SPEAKER_FRONT_RIGHT_OF_CENTER,
        SPEAKER_BACK_CENTER,
        0,
        SPEAKER_SIDE_LEFT,
        SPEAKER_SIDE_RIGHT,
        SPEAKER_TOP_FRONT_LEFT,
        SPEAKER_TOP_FRONT_RIGHT,
        SPEAKER_TOP_FRONT_CENTER,
        SPEAKER_TOP_CENTER,
        SPEAKER_TOP_BACK_LEFT,
        SPEAKER_TOP_BACK_RIGHT,
        0,
        0,
        SPEAKER_TOP_BACK_CENTER,
    };

    if (position < ARRAY_SIZE(position_map))
        return position_map[position];
    return 0;
}

static uint32_t wg_channel_mask_from_gst(const GstAudioInfo *info)
{
    uint32_t mask = 0, position;
    unsigned int i;

    for (i = 0; i < GST_AUDIO_INFO_CHANNELS(info); ++i)
    {
        if (!(position = wg_channel_position_from_gst(GST_AUDIO_INFO_POSITION(info, i))))
        {
            GST_WARNING("Unsupported channel %#x.", GST_AUDIO_INFO_POSITION(info, i));
            return 0;
        }
        /* Make sure it's also in WinMM order. WinMM mandates that channels be
         * ordered, as it were, from least to most significant SPEAKER_* bit.
         * Hence we fail if the current channel was already specified, or if any
         * higher bit was already specified. */
        if (mask & ~(position - 1))
        {
            GST_WARNING("Unsupported channel order.");
            return 0;
        }
        mask |= position;
    }
    return mask;
}

static void wg_format_from_audio_info(struct wg_format *format, const GstAudioInfo *info)
{
    format->major_type = WG_MAJOR_TYPE_AUDIO;
    format->u.audio.format = wg_audio_format_from_gst(GST_AUDIO_INFO_FORMAT(info));
    format->u.audio.channels = GST_AUDIO_INFO_CHANNELS(info);
    format->u.audio.channel_mask = wg_channel_mask_from_gst(info);
    format->u.audio.rate = GST_AUDIO_INFO_RATE(info);
}

static enum wg_video_format wg_video_format_from_gst(GstVideoFormat format)
{
    switch (format)
    {
        case GST_VIDEO_FORMAT_BGRA:
            return WG_VIDEO_FORMAT_BGRA;
        case GST_VIDEO_FORMAT_BGRx:
            return WG_VIDEO_FORMAT_BGRx;
        case GST_VIDEO_FORMAT_BGR:
            return WG_VIDEO_FORMAT_BGR;
        case GST_VIDEO_FORMAT_RGB15:
            return WG_VIDEO_FORMAT_RGB15;
        case GST_VIDEO_FORMAT_RGB16:
            return WG_VIDEO_FORMAT_RGB16;
        case GST_VIDEO_FORMAT_AYUV:
            return WG_VIDEO_FORMAT_AYUV;
        case GST_VIDEO_FORMAT_I420:
            return WG_VIDEO_FORMAT_I420;
        case GST_VIDEO_FORMAT_NV12:
            return WG_VIDEO_FORMAT_NV12;
        case GST_VIDEO_FORMAT_UYVY:
            return WG_VIDEO_FORMAT_UYVY;
        case GST_VIDEO_FORMAT_YUY2:
            return WG_VIDEO_FORMAT_YUY2;
        case GST_VIDEO_FORMAT_YV12:
            return WG_VIDEO_FORMAT_YV12;
        case GST_VIDEO_FORMAT_YVYU:
            return WG_VIDEO_FORMAT_YVYU;
        default:
            return WG_VIDEO_FORMAT_UNKNOWN;
    }
}

static void wg_format_from_video_info(struct wg_format *format, const GstVideoInfo *info)
{
    format->major_type = WG_MAJOR_TYPE_VIDEO;
    format->u.video.format = wg_video_format_from_gst(GST_VIDEO_INFO_FORMAT(info));
    format->u.video.width = GST_VIDEO_INFO_WIDTH(info);
    format->u.video.height = GST_VIDEO_INFO_HEIGHT(info);
    format->u.video.fps_n = GST_VIDEO_INFO_FPS_N(info);
    format->u.video.fps_d = GST_VIDEO_INFO_FPS_D(info);
}

static void wg_format_from_caps_audio_mpeg(struct wg_format *format, const GstCaps *caps)
{
    const GstStructure *structure = gst_caps_get_structure(caps, 0);
    gint layer, channels, rate;

    if (!gst_structure_get_int(structure, "layer", &layer))
    {
        GST_WARNING("Missing \"layer\" value.");
        return;
    }
    if (!gst_structure_get_int(structure, "channels", &channels))
    {
        GST_WARNING("Missing \"channels\" value.");
        return;
    }
    if (!gst_structure_get_int(structure, "rate", &rate))
    {
        GST_WARNING("Missing \"rate\" value.");
        return;
    }

    format->major_type = WG_MAJOR_TYPE_AUDIO;

    if (layer == 1)
        format->u.audio.format = WG_AUDIO_FORMAT_MPEG1_LAYER1;
    else if (layer == 2)
        format->u.audio.format = WG_AUDIO_FORMAT_MPEG1_LAYER2;
    else if (layer == 3)
        format->u.audio.format = WG_AUDIO_FORMAT_MPEG1_LAYER3;

    format->u.audio.channels = channels;
    format->u.audio.rate = rate;
}

static void wg_format_from_caps_video_cinepak(struct wg_format *format, const GstCaps *caps)
{
    const GstStructure *structure = gst_caps_get_structure(caps, 0);
    gint width, height, fps_n, fps_d;

    if (!gst_structure_get_int(structure, "width", &width))
    {
        GST_WARNING("Missing \"width\" value.");
        return;
    }
    if (!gst_structure_get_int(structure, "height", &height))
    {
        GST_WARNING("Missing \"height\" value.");
        return;
    }
    if (!gst_structure_get_fraction(structure, "framerate", &fps_n, &fps_d))
    {
        fps_n = 0;
        fps_d = 1;
    }

    format->major_type = WG_MAJOR_TYPE_VIDEO;
    format->u.video.format = WG_VIDEO_FORMAT_CINEPAK;
    format->u.video.width = width;
    format->u.video.height = height;
    format->u.video.fps_n = fps_n;
    format->u.video.fps_d = fps_d;
}

static void wg_format_from_caps(struct wg_format *format, const GstCaps *caps)
{
    const GstStructure *structure = gst_caps_get_structure(caps, 0);
    const char *name = gst_structure_get_name(structure);

    memset(format, 0, sizeof(*format));

    if (!strcmp(name, "audio/x-raw"))
    {
        GstAudioInfo info;

        if (gst_audio_info_from_caps(&info, caps))
            wg_format_from_audio_info(format, &info);
    }
    else if (!strcmp(name, "video/x-raw"))
    {
        GstVideoInfo info;

        if (gst_video_info_from_caps(&info, caps))
            wg_format_from_video_info(format, &info);
    }
    else if (!strcmp(name, "audio/mpeg"))
    {
        wg_format_from_caps_audio_mpeg(format, caps);
    }
    else if (!strcmp(name, "video/x-cinepak"))
    {
        wg_format_from_caps_video_cinepak(format, caps);
    }
    else
    {
        gchar *str = gst_caps_to_string(caps);

        GST_FIXME("Unhandled caps %s.", str);
        g_free(str);
    }
}

static GstAudioFormat wg_audio_format_to_gst(enum wg_audio_format format)
{
    switch (format)
    {
        case WG_AUDIO_FORMAT_U8:    return GST_AUDIO_FORMAT_U8;
        case WG_AUDIO_FORMAT_S16LE: return GST_AUDIO_FORMAT_S16LE;
        case WG_AUDIO_FORMAT_S24LE: return GST_AUDIO_FORMAT_S24LE;
        case WG_AUDIO_FORMAT_S32LE: return GST_AUDIO_FORMAT_S32LE;
        case WG_AUDIO_FORMAT_F32LE: return GST_AUDIO_FORMAT_F32LE;
        case WG_AUDIO_FORMAT_F64LE: return GST_AUDIO_FORMAT_F64LE;
        default: return GST_AUDIO_FORMAT_UNKNOWN;
    }
}

static void wg_channel_mask_to_gst(GstAudioChannelPosition *positions, uint32_t mask, uint32_t channel_count)
{
    const uint32_t orig_mask = mask;
    unsigned int i;
    DWORD bit;

    static const GstAudioChannelPosition position_map[] =
    {
        GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
        GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT,
        GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER,
        GST_AUDIO_CHANNEL_POSITION_LFE1,
        GST_AUDIO_CHANNEL_POSITION_REAR_LEFT,
        GST_AUDIO_CHANNEL_POSITION_REAR_RIGHT,
        GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER,
        GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER,
        GST_AUDIO_CHANNEL_POSITION_REAR_CENTER,
        GST_AUDIO_CHANNEL_POSITION_SIDE_LEFT,
        GST_AUDIO_CHANNEL_POSITION_SIDE_RIGHT,
        GST_AUDIO_CHANNEL_POSITION_TOP_CENTER,
        GST_AUDIO_CHANNEL_POSITION_TOP_FRONT_LEFT,
        GST_AUDIO_CHANNEL_POSITION_TOP_FRONT_CENTER,
        GST_AUDIO_CHANNEL_POSITION_TOP_FRONT_RIGHT,
        GST_AUDIO_CHANNEL_POSITION_TOP_REAR_LEFT,
        GST_AUDIO_CHANNEL_POSITION_TOP_REAR_CENTER,
        GST_AUDIO_CHANNEL_POSITION_TOP_REAR_RIGHT,
    };

    for (i = 0; i < channel_count; ++i)
    {
        positions[i] = GST_AUDIO_CHANNEL_POSITION_NONE;
        if (BitScanForward(&bit, mask))
        {
            if (bit < ARRAY_SIZE(position_map))
                positions[i] = position_map[bit];
            else
                GST_WARNING("Invalid channel mask %#x.\n", orig_mask);
            mask &= ~(1 << bit);
        }
        else
        {
            GST_WARNING("Incomplete channel mask %#x.\n", orig_mask);
        }
    }
}

static void wg_set_caps_from_wg_format(GstCaps *caps, const struct wg_format *format)
{
    switch (format->major_type)
    {
        case WG_MAJOR_TYPE_VIDEO:
        {
            gst_caps_set_simple(caps, "width", G_TYPE_INT, format->u.video.width, NULL);
            gst_caps_set_simple(caps, "height", G_TYPE_INT, format->u.video.height, NULL);
            gst_caps_set_simple(caps, "framerate", GST_TYPE_FRACTION, format->u.video.fps_n, format->u.video.fps_d, NULL);
            break;
        }
        case WG_MAJOR_TYPE_AUDIO:
        {
            gst_caps_set_simple(caps, "rate", G_TYPE_INT, format->u.audio.rate, NULL);
            gst_caps_set_simple(caps, "channels", G_TYPE_INT, format->u.audio.channels, NULL);
            if (format->u.audio.channel_mask)
                gst_caps_set_simple(caps, "channel-mask", G_TYPE_INT, format->u.audio.channel_mask, NULL);
        }
        default:
            break;
    }
}

static GstCaps *wg_format_to_caps_audio(const struct wg_format *format)
{
    GstAudioChannelPosition positions[32];
    GstAudioFormat audio_format;
    GstAudioInfo info;

    /* compressed types */

    if (format->u.audio.format == WG_AUDIO_FORMAT_AAC)
    {
        const char *profile, *level;
        GstBuffer *audio_specific_config;
        GstCaps *caps = gst_caps_new_empty_simple("audio/mpeg");
        wg_set_caps_from_wg_format(caps, format);

        gst_caps_set_simple(caps, "mpegversion", G_TYPE_INT, 4, NULL);

        switch (format->u.audio.compressed.aac.payload_type)
        {
            case 0:
                gst_caps_set_simple(caps, "stream-format", G_TYPE_STRING, "raw", NULL);
                break;
            case 1:
                gst_caps_set_simple(caps, "stream-format", G_TYPE_STRING, "adts", NULL);
                break;
            case 2:
                gst_caps_set_simple(caps, "stream-format", G_TYPE_STRING, "adif", NULL);
                break;
            case 3:
                gst_caps_set_simple(caps, "stream-format", G_TYPE_STRING, "loas", NULL);
                break;
            default:
                gst_caps_set_simple(caps, "stream-format", G_TYPE_STRING, "raw", NULL);
        };

        switch (format->u.audio.compressed.aac.indication)
        {
            case 0x29: profile = "lc"; level = "2";  break;
            case 0x2A: profile = "lc"; level = "4"; break;
            case 0x2B: profile = "lc"; level = "5"; break;
            default:
                FIXME("Unrecognized profile-level-indication %u\n", format->u.audio.compressed.aac.indication);
                /* fallthrough */
            case 0x00: case 0xFE: profile = level = NULL; break; /* unspecified */
        }

        if (profile)
            gst_caps_set_simple(caps, "profile", G_TYPE_STRING, profile, NULL);
        if (level)
            gst_caps_set_simple(caps, "level", G_TYPE_STRING, level, NULL);

        audio_specific_config = gst_buffer_new_allocate(NULL, format->u.audio.compressed.aac.asp_size, NULL);
        gst_buffer_fill(audio_specific_config, 0, format->u.audio.compressed.aac.audio_specifc_config, format->u.audio.compressed.aac.asp_size);
        gst_caps_set_simple(caps, "codec_data", GST_TYPE_BUFFER, audio_specific_config, NULL);
        gst_buffer_unref(audio_specific_config);

        return caps;
    }

    /* uncompressed_types */

    if ((audio_format = wg_audio_format_to_gst(format->u.audio.format)) == GST_AUDIO_FORMAT_UNKNOWN)
        return NULL;

    wg_channel_mask_to_gst(positions, format->u.audio.channel_mask, format->u.audio.channels);
    gst_audio_info_set_format(&info, audio_format, format->u.audio.rate, format->u.audio.channels, positions);
    return gst_audio_info_to_caps(&info);
}

static GstVideoFormat wg_video_format_to_gst(enum wg_video_format format)
{
    switch (format)
    {
        case WG_VIDEO_FORMAT_BGRA:  return GST_VIDEO_FORMAT_BGRA;
        case WG_VIDEO_FORMAT_BGRx:  return GST_VIDEO_FORMAT_BGRx;
        case WG_VIDEO_FORMAT_BGR:   return GST_VIDEO_FORMAT_BGR;
        case WG_VIDEO_FORMAT_RGB15: return GST_VIDEO_FORMAT_RGB15;
        case WG_VIDEO_FORMAT_RGB16: return GST_VIDEO_FORMAT_RGB16;
        case WG_VIDEO_FORMAT_AYUV:  return GST_VIDEO_FORMAT_AYUV;
        case WG_VIDEO_FORMAT_I420:  return GST_VIDEO_FORMAT_I420;
        case WG_VIDEO_FORMAT_NV12:  return GST_VIDEO_FORMAT_NV12;
        case WG_VIDEO_FORMAT_UYVY:  return GST_VIDEO_FORMAT_UYVY;
        case WG_VIDEO_FORMAT_YUY2:  return GST_VIDEO_FORMAT_YUY2;
        case WG_VIDEO_FORMAT_YV12:  return GST_VIDEO_FORMAT_YV12;
        case WG_VIDEO_FORMAT_YVYU:  return GST_VIDEO_FORMAT_YVYU;
        default: return GST_VIDEO_FORMAT_UNKNOWN;
    }
}

static GstCaps *wg_format_to_caps_video(const struct wg_format *format)
{
    GstVideoFormat video_format;
    GstVideoInfo info;
    unsigned int i;
    GstCaps *caps;

    /* compressed types */

    if (format->u.video.format == WG_VIDEO_FORMAT_H264)
    {
        const char *profile;
        const char *level;

        caps = gst_caps_new_empty_simple("video/x-h264");
        wg_set_caps_from_wg_format(caps, format);

        gst_caps_set_simple(caps, "stream-format", G_TYPE_STRING, "byte-stream", NULL);
        gst_caps_set_simple(caps, "alignment", G_TYPE_STRING, "au", NULL);

        switch (format->u.video.compressed.h264.profile)
        {
            case /* eAVEncH264VProfile_Main */ 77:  profile = "main"; break;
            case /* eAVEncH264VProfile_High */ 100: profile = "high"; break;
            case /* eAVEncH264VProfile_444 */  244: profile = "high-4:4:4"; break;
            default:
                ERR("Unrecognized H.264 profile attribute %u\n", format->u.video.compressed.h264.profile);
                /* fallthrough */
            case 0: profile = NULL;
        }

        switch (format->u.video.compressed.h264.level)
        {
            case /* eAVEncH264VLevel1 */   10: level = "1";   break;
            case /* eAVEncH264VLevel1_1 */ 11: level = "1.1"; break;
            case /* eAVEncH264VLevel1_2 */ 12: level = "1.2"; break;
            case /* eAVEncH264VLevel1_3 */ 13: level = "1.3"; break;
            case /* eAVEncH264VLevel2 */   20: level = "2";   break;
            case /* eAVEncH264VLevel2_1 */ 21: level = "2.1"; break;
            case /* eAVEncH264VLevel2_2 */ 22: level = "2.2"; break;
            case /* eAVEncH264VLevel3 */   30: level = "3";   break;
            case /* eAVEncH264VLevel3_1 */ 31: level = "3.1"; break;
            case /* eAVEncH264VLevel3_2 */ 32: level = "3.2"; break;
            case /* eAVEncH264VLevel4 */   40: level = "4";   break;
            case /* eAVEncH264VLevel4_1 */ 41: level = "4.1"; break;
            case /* eAVEncH264VLevel4_2 */ 42: level = "4.2"; break;
            case /* eAVEncH264VLevel5 */   50: level = "5";   break;
            case /* eAVEncH264VLevel5_1 */ 51: level = "5.1"; break;
            case /* eAVEncH264VLevel5_2 */ 52: level = "5.2"; break;
            default:
                ERR("Unrecognized H.264 level attribute %u\n", format->u.video.compressed.h264.level);
                /* fallthrough */
            case 0: level = NULL;
        }

        if (profile)
            gst_caps_set_simple(caps, "profile", G_TYPE_STRING, profile, NULL);

        if (level)
            gst_caps_set_simple(caps, "level", G_TYPE_STRING, level, NULL);

        return caps;
    }

    /* uncompressed types */

    if ((video_format = wg_video_format_to_gst(format->u.video.format)) == GST_VIDEO_FORMAT_UNKNOWN)
        return NULL;

    gst_video_info_set_format(&info, video_format, format->u.video.width, format->u.video.height);
    if ((caps = gst_video_info_to_caps(&info)))
    {
        /* Clear some fields that shouldn't prevent us from connecting. */
        for (i = 0; i < gst_caps_get_size(caps); ++i)
        {
            gst_structure_remove_fields(gst_caps_get_structure(caps, i),
                    "framerate", "pixel-aspect-ratio", "colorimetry", "chroma-site", NULL);
        }
    }
    return caps;
}

static GstCaps *wg_format_to_caps(const struct wg_format *format)
{
    switch (format->major_type)
    {
        case WG_MAJOR_TYPE_UNKNOWN:
            return NULL;
        case WG_MAJOR_TYPE_AUDIO:
            return wg_format_to_caps_audio(format);
        case WG_MAJOR_TYPE_VIDEO:
            return wg_format_to_caps_video(format);
    }
    assert(0);
    return NULL;
}

static bool wg_format_compare(const struct wg_format *a, const struct wg_format *b)
{
    if (a->major_type != b->major_type)
        return false;

    switch (a->major_type)
    {
        case WG_MAJOR_TYPE_UNKNOWN:
            return false;

        case WG_MAJOR_TYPE_AUDIO:
            return a->u.audio.format == b->u.audio.format
                    && a->u.audio.channels == b->u.audio.channels
                    && a->u.audio.rate == b->u.audio.rate;

        case WG_MAJOR_TYPE_VIDEO:
            /* Do not compare FPS. */
            return a->u.video.format == b->u.video.format
                    && a->u.video.width == b->u.video.width
                    && a->u.video.height == b->u.video.height;
    }

    assert(0);
    return false;
}

static uint32_t CDECL wg_parser_get_stream_count(struct wg_parser *parser)
{
    return parser->stream_count;
}

static struct wg_parser_stream * CDECL wg_parser_get_stream(struct wg_parser *parser, uint32_t index)
{
    return parser->streams[index];
}

static void CDECL wg_parser_begin_flush(struct wg_parser *parser)
{
    unsigned int i;

    if (!parser->seekable)
        return;

    pthread_mutex_lock(&parser->mutex);
    parser->flushing = true;
    pthread_mutex_unlock(&parser->mutex);

    for (i = 0; i < parser->stream_count; ++i)
    {
        if (parser->streams[i]->enabled)
            pthread_cond_signal(&parser->streams[i]->event_cond);
    }
}

static void CDECL wg_parser_end_flush(struct wg_parser *parser)
{
    if (!parser->seekable)
        return;

    pthread_mutex_lock(&parser->mutex);
    parser->flushing = false;
    pthread_mutex_unlock(&parser->mutex);
}

static bool CDECL wg_parser_get_read_request(struct wg_parser *parser,
        void **data, uint64_t *offset, uint32_t *size)
{
    pthread_mutex_lock(&parser->mutex);

    while (parser->sink_connected && (!parser->read_request.data || parser->read_request.done))
        pthread_cond_wait(&parser->read_cond, &parser->mutex);

    if (!parser->sink_connected)
    {
        pthread_mutex_unlock(&parser->mutex);
        return false;
    }

    *data = parser->read_request.data;
    *offset = parser->read_request.offset;
    *size = parser->read_request.size;

    pthread_mutex_unlock(&parser->mutex);
    return true;
}

static void CDECL wg_parser_complete_read_request(struct wg_parser *parser, enum wg_read_result ret, uint32_t bytes_available)
{
    pthread_mutex_lock(&parser->mutex);
    parser->read_request.ret = ret;
    if (ret == WG_READ_SUCCESS && bytes_available < parser->read_request.size)
    {
        parser->read_request.offset += bytes_available;
        parser->read_request.size -= bytes_available;
        parser->read_request.data = (unsigned char *)parser->read_request.data + bytes_available;
    }
    else
    {
        parser->read_request.data = NULL;
        /* reuse size parameter to indicate how many more bytes are left to write */
        parser->read_request.size = bytes_available;
    }
    parser->read_request.done = true;
    pthread_cond_signal(&parser->read_done_cond);
    pthread_mutex_unlock(&parser->mutex);
}

static void CDECL wg_parser_set_unlimited_buffering(struct wg_parser *parser)
{
    g_object_set(parser->decodebin, "max-size-buffers", G_MAXUINT, NULL);
    g_object_set(parser->decodebin, "max-size-time", G_MAXUINT64, NULL);
    g_object_set(parser->decodebin, "max-size-bytes", G_MAXUINT, NULL);
}

static void CDECL wg_parser_stream_get_preferred_format(struct wg_parser_stream *stream, struct wg_format *format)
{
    if (stream->has_caps)
        *format = stream->preferred_format;
}

static void CDECL wg_parser_stream_enable(struct wg_parser_stream *stream, const struct wg_format *format, const struct wg_rect *aperture)
{
    if (!stream->parser->seekable)
        return;

    stream->current_format = *format;
    stream->enabled = true;

    if (format->major_type == WG_MAJOR_TYPE_VIDEO)
    {
        switch (format->u.video.format)
        {
            case WG_VIDEO_FORMAT_BGRA:
            case WG_VIDEO_FORMAT_BGRx:
            case WG_VIDEO_FORMAT_BGR:
            case WG_VIDEO_FORMAT_RGB15:
            case WG_VIDEO_FORMAT_RGB16:
                gst_util_set_object_arg(G_OBJECT(stream->flip), "method", "vertical-flip");
                break;

            case WG_VIDEO_FORMAT_AYUV:
            case WG_VIDEO_FORMAT_I420:
            case WG_VIDEO_FORMAT_NV12:
            case WG_VIDEO_FORMAT_UYVY:
            case WG_VIDEO_FORMAT_YUY2:
            case WG_VIDEO_FORMAT_YV12:
            case WG_VIDEO_FORMAT_YVYU:
            case WG_VIDEO_FORMAT_UNKNOWN:
            case WG_VIDEO_FORMAT_CINEPAK:
            case WG_VIDEO_FORMAT_H264:
                gst_util_set_object_arg(G_OBJECT(stream->flip), "method", "none");
                break;
        }

        if (aperture)
        {
            if (!stream->box && (stream->aperture.left || stream->aperture.top ||
                (stream->aperture.right && stream->aperture.right != stream->current_format.u.video.width) ||
                (stream->aperture.bottom && stream->aperture.bottom != stream->current_format.u.video.height)))
            {
                fprintf(stderr, "winegstreamer: failed to create videobox, are %u-bit GStreamer \"good\" plugins installed?\n",
                    8 * (int)sizeof(void *));
                return;
            }

            if (aperture->left)
                g_object_set(G_OBJECT(stream->box), "left", -aperture->left, NULL);
            if (aperture->top)
                g_object_set(G_OBJECT(stream->box), "top", -aperture->top, NULL);
            if (aperture->right)
                g_object_set(G_OBJECT(stream->box), "right", aperture->right - format->u.video.width, NULL);
            if (aperture->bottom)
                g_object_set(G_OBJECT(stream->box), "bottom", aperture->bottom - format->u.video.height, NULL);
        }
    }

    gst_pad_push_event(stream->my_sink, gst_event_new_reconfigure());
}

static void CDECL wg_parser_stream_disable(struct wg_parser_stream *stream)
{
    stream->enabled = false;
}

static bool CDECL wg_parser_stream_get_event(struct wg_parser_stream *stream, struct wg_parser_event *event)
{
    struct wg_parser *parser = stream->parser;

    pthread_mutex_lock(&parser->mutex);

    while (!parser->flushing && stream->event.type == WG_PARSER_EVENT_NONE)
        pthread_cond_wait(&stream->event_cond, &parser->mutex);

    if (parser->flushing)
    {
        pthread_mutex_unlock(&parser->mutex);
        TRACE("Filter is flushing.\n");
        return false;
    }

    *event = stream->event;

    /* Set to ensure that drain isn't called on an EOS stream, causing a lock-up
       due to pull_data never being called again */
    if (stream->event.type == WG_PARSER_EVENT_EOS)
        stream->eos = true;

    if (stream->event.type != WG_PARSER_EVENT_BUFFER)
    {
        stream->event.type = WG_PARSER_EVENT_NONE;
        pthread_cond_signal(&stream->event_empty_cond);
    }
    pthread_mutex_unlock(&parser->mutex);

    return true;
}

static bool CDECL wg_parser_stream_copy_buffer(struct wg_parser_stream *stream,
        void *data, uint32_t offset, uint32_t size)
{
    struct wg_parser *parser = stream->parser;

    pthread_mutex_lock(&parser->mutex);

    if (!stream->buffer)
    {
        pthread_mutex_unlock(&parser->mutex);
        return false;
    }

    assert(stream->event.type == WG_PARSER_EVENT_BUFFER);
    assert(offset < stream->map_info.size);
    assert(offset + size <= stream->map_info.size);
    memcpy(data, stream->map_info.data + offset, size);

    pthread_mutex_unlock(&parser->mutex);
    return true;
}

static void CDECL wg_parser_stream_release_buffer(struct wg_parser_stream *stream)
{
    struct wg_parser *parser = stream->parser;

    pthread_mutex_lock(&parser->mutex);

    assert(stream->event.type == WG_PARSER_EVENT_BUFFER);

    gst_buffer_unmap(stream->buffer, &stream->map_info);
    gst_buffer_unref(stream->buffer);
    stream->buffer = NULL;
    stream->event.type = WG_PARSER_EVENT_NONE;

    pthread_mutex_unlock(&parser->mutex);
    pthread_cond_signal(&stream->event_empty_cond);
}

static uint64_t CDECL wg_parser_stream_get_duration(struct wg_parser_stream *stream)
{
    return stream->duration;
}

static char * CDECL wg_parser_stream_get_language(struct wg_parser_stream *stream)
{
    return stream->language_code;
}

static bool CDECL wg_parser_stream_seek(struct wg_parser_stream *stream, double rate,
        uint64_t start_pos, uint64_t stop_pos, DWORD start_flags, DWORD stop_flags)
{
    GstSeekType start_type = GST_SEEK_TYPE_SET, stop_type = GST_SEEK_TYPE_SET;
    GstSeekFlags flags = 0;

    if (!stream->parser->seekable)
        return false;

    if (start_flags & AM_SEEKING_SeekToKeyFrame)
        flags |= GST_SEEK_FLAG_KEY_UNIT;
    if (start_flags & AM_SEEKING_Segment)
        flags |= GST_SEEK_FLAG_SEGMENT;
    if (!(start_flags & AM_SEEKING_NoFlush))
        flags |= GST_SEEK_FLAG_FLUSH;

    if ((start_flags & AM_SEEKING_PositioningBitsMask) == AM_SEEKING_NoPositioning)
        start_type = GST_SEEK_TYPE_NONE;
    if ((stop_flags & AM_SEEKING_PositioningBitsMask) == AM_SEEKING_NoPositioning)
        stop_type = GST_SEEK_TYPE_NONE;

    return gst_pad_push_event(stream->my_sink, gst_event_new_seek(rate,
            GST_FORMAT_TIME, flags, start_type, start_pos * 100, stop_type, stop_pos * 100));
}

static bool CDECL wg_parser_stream_drain(struct wg_parser_stream *stream)
{
    struct wg_parser *parser = stream->parser;
    bool ret;

    pthread_mutex_lock(&parser->mutex);

    /* Sanity check making sure caller didn't try to drain an already-EOS or unselected stream.
       There's no reason for a caller to do this, but it could be an accident in which case we
       should indicate that the stream is drained instead of locking-up. */
    if (!stream->enabled || stream->eos)
    {
        pthread_mutex_unlock(&parser->mutex);
        return true;
    }

    parser->draining = true;
    pthread_cond_signal(&parser->read_done_cond);

    /* We must wait for either an event to occur or the drain to complete.
       Since drains are blocking, we assign this responsibility to the thread
       pulling data, as the pipeline will not need to pull more data until
       the drain completes.  If one input buffer yields more than one output
       buffer, the chain callback blocks on the wg_parser_stream_buffer_release
       for the first buffer, which would never be called if the drain function
       hadn't completed. */
    while (!parser->flushing && parser->draining && stream->event.type == WG_PARSER_EVENT_NONE)
        pthread_cond_wait(&stream->event_cond, &parser->mutex);

    ret = stream->event.type == WG_PARSER_EVENT_NONE;
    parser->draining = false;

    pthread_mutex_unlock(&stream->parser->mutex);

    return ret;
}

static void CDECL wg_parser_stream_notify_qos(struct wg_parser_stream *stream,
        bool underflow, double proportion, int64_t diff, uint64_t timestamp)
{
    GstClockTime stream_time;
    GstEvent *event;

    /* We return timestamps in stream time, i.e. relative to the start of the
     * file (or other medium), but gst_event_new_qos() expects the timestamp in
     * running time. */
    stream_time = gst_segment_to_running_time(&stream->segment, GST_FORMAT_TIME, timestamp * 100);
    if (stream_time == -1)
    {
        /* This can happen legitimately if the sample falls outside of the
         * segment bounds. GStreamer elements shouldn't present the sample in
         * that case, but DirectShow doesn't care. */
        TRACE("Ignoring QoS event.\n");
        return;
    }

    if (!(event = gst_event_new_qos(underflow ? GST_QOS_TYPE_UNDERFLOW : GST_QOS_TYPE_OVERFLOW,
            proportion, diff * 100, stream_time)))
        ERR("Failed to create QOS event.\n");
    gst_pad_push_event(stream->my_sink, event);
}

static GstAutoplugSelectResult autoplug_select_cb(GstElement *bin, GstPad *pad,
        GstCaps *caps, GstElementFactory *fact, gpointer user)
{
    const char *name = gst_element_factory_get_longname(fact);

    GST_TRACE("Using \"%s\".", name);

    if (strstr(name, "Player protection"))
    {
        GST_WARNING("Blacklisted a/52 decoder because it only works in Totem.");
        return GST_AUTOPLUG_SELECT_SKIP;
    }
    if (!strcmp(name, "Fluendo Hardware Accelerated Video Decoder"))
    {
        GST_WARNING("Disabled video acceleration since it breaks in wine.");
        return GST_AUTOPLUG_SELECT_SKIP;
    }
    return GST_AUTOPLUG_SELECT_TRY;
}

static void no_more_pads_cb(GstElement *element, gpointer user)
{
    struct wg_parser *parser = user;

    GST_DEBUG("parser %p.", parser);

    pthread_mutex_lock(&parser->mutex);
    parser->no_more_pads = true;
    pthread_mutex_unlock(&parser->mutex);
    pthread_cond_signal(&parser->init_cond);
}

static GstFlowReturn queue_stream_event(struct wg_parser_stream *stream,
        const struct wg_parser_event *event, GstBuffer *buffer)
{
    struct wg_parser *parser = stream->parser;

    /* Unlike request_buffer_src() [q.v.], we need to watch for GStreamer
     * flushes here. The difference is that we can be blocked by the streaming
     * thread not running (or itself flushing on the DirectShow side).
     * request_buffer_src() can only be blocked by the upstream source, and that
     * is solved by flushing the upstream source. */

    pthread_mutex_lock(&parser->mutex);
    while (!stream->flushing && stream->event.type != WG_PARSER_EVENT_NONE)
        pthread_cond_wait(&stream->event_empty_cond, &parser->mutex);
    if (stream->flushing)
    {
        pthread_mutex_unlock(&parser->mutex);
        GST_DEBUG("Filter is flushing; discarding event.");
        return GST_FLOW_FLUSHING;
    }
    if (buffer)
    {
        assert(GST_IS_BUFFER(buffer));
        if (!gst_buffer_map(buffer, &stream->map_info, GST_MAP_READ))
        {
            pthread_mutex_unlock(&parser->mutex);
            GST_ERROR("Failed to map buffer.\n");
            return GST_FLOW_ERROR;
        }
    }
    stream->event = *event;
    stream->buffer = buffer;
    pthread_mutex_unlock(&parser->mutex);
    pthread_cond_signal(&stream->event_cond);
    GST_LOG("Event queued.");
    return GST_FLOW_OK;
}

static gboolean sink_event_cb(GstPad *pad, GstObject *parent, GstEvent *event)
{
    struct wg_parser_stream *stream = gst_pad_get_element_private(pad);
    struct wg_parser *parser = stream->parser;

    GST_LOG("stream %p, type \"%s\".", stream, GST_EVENT_TYPE_NAME(event));

    switch (event->type)
    {
        case GST_EVENT_SEGMENT:
            if (stream->enabled)
            {
                struct wg_parser_event stream_event;
                const GstSegment *segment;

                gst_event_parse_segment(event, &segment);

                if (segment->format != GST_FORMAT_TIME)
                {
                    GST_FIXME("Unhandled format \"%s\".", gst_format_get_name(segment->format));
                    break;
                }

                gst_segment_copy_into(segment, &stream->segment);

                stream_event.type = WG_PARSER_EVENT_SEGMENT;
                stream_event.u.segment.position = segment->position / 100;
                stream_event.u.segment.stop = segment->stop / 100;
                stream_event.u.segment.rate = segment->rate * segment->applied_rate;
                queue_stream_event(stream, &stream_event, NULL);
            }
            break;

        case GST_EVENT_EOS:
            if (stream->enabled)
            {
                struct wg_parser_event stream_event;

                stream_event.type = WG_PARSER_EVENT_EOS;
                queue_stream_event(stream, &stream_event, NULL);
            }
            else
            {
                pthread_mutex_lock(&parser->mutex);
                stream->eos = true;
                pthread_mutex_unlock(&parser->mutex);
                pthread_cond_signal(&parser->init_cond);
            }
            break;

        case GST_EVENT_FLUSH_START:
            if (stream->enabled)
            {
                pthread_mutex_lock(&parser->mutex);

                stream->flushing = true;
                pthread_cond_signal(&stream->event_empty_cond);

                if (stream->event.type == WG_PARSER_EVENT_BUFFER)
                {
                    gst_buffer_unmap(stream->buffer, &stream->map_info);
                    gst_buffer_unref(stream->buffer);
                    stream->buffer = NULL;
                }
                stream->event.type = WG_PARSER_EVENT_NONE;

                pthread_mutex_unlock(&parser->mutex);
            }
            break;

        case GST_EVENT_FLUSH_STOP:
        {
            gboolean reset_time;

            gst_event_parse_flush_stop(event, &reset_time);

            if (reset_time)
                gst_segment_init(&stream->segment, GST_FORMAT_UNDEFINED);

            if (stream->enabled)
            {
                pthread_mutex_lock(&parser->mutex);
                stream->flushing = false;
                pthread_mutex_unlock(&parser->mutex);
            }
            break;
        }

        case GST_EVENT_CAPS:
        {
            GstCaps *caps;

            gst_event_parse_caps(event, &caps);
            pthread_mutex_lock(&parser->mutex);
            wg_format_from_caps(&stream->preferred_format, caps);
            stream->has_caps = true;
            pthread_mutex_unlock(&parser->mutex);
            pthread_cond_signal(&parser->init_cond);
            break;
        }

        default:
            GST_WARNING("Ignoring \"%s\" event.", GST_EVENT_TYPE_NAME(event));
    }
    gst_event_unref(event);
    return TRUE;
}

static GstFlowReturn sink_chain_cb(GstPad *pad, GstObject *parent, GstBuffer *buffer)
{
    struct wg_parser_stream *stream = gst_pad_get_element_private(pad);
    struct wg_parser_event stream_event;
    GstFlowReturn ret;

    GST_LOG("stream %p, buffer %p.", stream, buffer);

    if (!stream->enabled)
    {
        gst_buffer_unref(buffer);
        return GST_FLOW_OK;
    }

    stream_event.type = WG_PARSER_EVENT_BUFFER;

    /* FIXME: Should we use gst_segment_to_stream_time_full()? Under what
     * circumstances is the stream time not equal to the buffer PTS? Note that
     * this will need modification to wg_parser_stream_notify_qos() as well. */

    if ((stream_event.u.buffer.has_pts = GST_BUFFER_PTS_IS_VALID(buffer)))
        stream_event.u.buffer.pts = GST_BUFFER_PTS(buffer) / 100;
    if ((stream_event.u.buffer.has_duration = GST_BUFFER_DURATION_IS_VALID(buffer)))
        stream_event.u.buffer.duration = GST_BUFFER_DURATION(buffer) / 100;
    stream_event.u.buffer.discontinuity = GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_DISCONT);
    stream_event.u.buffer.preroll = GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_LIVE);
    stream_event.u.buffer.delta = GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_DELTA_UNIT);
    stream_event.u.buffer.size = gst_buffer_get_size(buffer);

    /* Transfer our reference to the buffer to the stream object. */
    if ((ret = queue_stream_event(stream, &stream_event, buffer)) != GST_FLOW_OK)
        gst_buffer_unref(buffer);
    return ret;
}

static gboolean sink_query_cb(GstPad *pad, GstObject *parent, GstQuery *query)
{
    struct wg_parser_stream *stream = gst_pad_get_element_private(pad);

    GST_LOG("stream %p, type \"%s\".", stream, gst_query_type_get_name(query->type));

    switch (query->type)
    {
        case GST_QUERY_CAPS:
        {
            GstCaps *caps, *filter, *temp;

            gst_query_parse_caps(query, &filter);

            if (stream->enabled)
                caps = wg_format_to_caps(&stream->current_format);
            else
                caps = gst_caps_new_any();
            if (!caps)
                return FALSE;

            if (filter)
            {
                temp = gst_caps_intersect(caps, filter);
                gst_caps_unref(caps);
                caps = temp;
            }

            gst_query_set_caps_result(query, caps);
            gst_caps_unref(caps);
            return TRUE;
        }

        case GST_QUERY_ACCEPT_CAPS:
        {
            struct wg_format format;
            gboolean ret = TRUE;
            GstCaps *caps;

            if (!stream->enabled)
            {
                gst_query_set_accept_caps_result(query, TRUE);
                return TRUE;
            }

            gst_query_parse_accept_caps(query, &caps);
            wg_format_from_caps(&format, caps);
            ret = wg_format_compare(&format, &stream->current_format);
            if (!ret && WARN_ON(gstreamer))
            {
                gchar *str = gst_caps_to_string(caps);
                GST_WARNING("Rejecting caps \"%s\".", str);
                g_free(str);
            }
            gst_query_set_accept_caps_result(query, ret);
            return TRUE;
        }

        default:
            return gst_pad_query_default (pad, parent, query);
    }
}

static struct wg_parser_stream *create_stream(struct wg_parser *parser)
{
    struct wg_parser_stream *stream, **new_array;
    unsigned int i;
    char pad_name[19];

    for (i = 0; i < parser->expected_stream_count; i++)
    {
        if (!parser->streams[i]->parser)
        {
            stream = parser->streams[i];
            break;
        }
    }

    if (i == parser->expected_stream_count)
    {
        if (!(new_array = realloc(parser->streams, (parser->stream_count + 1) * sizeof(*parser->streams))))
            return NULL;
        parser->streams = new_array;

        if (!(stream = calloc(1, sizeof(*stream))))
            return NULL;
    }

    gst_segment_init(&stream->segment, GST_FORMAT_UNDEFINED);

    stream->parser = parser;
    pthread_cond_init(&stream->event_cond, NULL);
    pthread_cond_init(&stream->event_empty_cond, NULL);

    sprintf(pad_name, "wine_sink_%u", parser->stream_count);
    stream->my_sink = gst_pad_new(pad_name, GST_PAD_SINK);
    gst_pad_set_element_private(stream->my_sink, stream);
    gst_pad_set_chain_function(stream->my_sink, sink_chain_cb);
    gst_pad_set_event_function(stream->my_sink, sink_event_cb);
    gst_pad_set_query_function(stream->my_sink, sink_query_cb);

    parser->streams[parser->stream_count++] = stream;
    return stream;
}

static void pad_added_cb(GstElement *element, GstPad *pad, gpointer user)
{
    struct wg_parser *parser = user;
    struct wg_parser_stream *stream;
    const char *name;
    GstCaps *caps;
    int ret;

    GST_LOG("parser %p, element %p, pad %p.", parser, element, pad);

    if (gst_pad_is_linked(pad))
        return;

    caps = gst_pad_query_caps(pad, NULL);
    name = gst_structure_get_name(gst_caps_get_structure(caps, 0));

    if (!(stream = create_stream(parser)))
        goto out;

    if (!strcmp(name, "video/x-raw"))
    {
        GstElement *deinterlace, *vconv, *flip, *videobox, *vconv2;

        /* DirectShow can express interlaced video, but downstream filters can't
         * necessarily consume it. In particular, the video renderer can't. */
        if (!(deinterlace = gst_element_factory_make("deinterlace", NULL)))
        {
            fprintf(stderr, "winegstreamer: failed to create deinterlace, are %u-bit GStreamer \"good\" plugins installed?\n",
                    8 * (int)sizeof(void *));
            goto out;
        }

        /* decodebin considers many YUV formats to be "raw", but some quartz
         * filters can't handle those. Also, videoflip can't handle all "raw"
         * formats either. Add a videoconvert to swap color spaces. */
        if (!(vconv = gst_element_factory_make("videoconvert", NULL)))
        {
            fprintf(stderr, "winegstreamer: failed to create videoconvert, are %u-bit GStreamer \"base\" plugins installed?\n",
                    8 * (int)sizeof(void *));
            goto out;
        }

        /* GStreamer outputs RGB video top-down, but DirectShow expects bottom-up. */
        if (!(flip = gst_element_factory_make("videoflip", NULL)))
        {
            fprintf(stderr, "winegstreamer: failed to create videoflip, are %u-bit GStreamer \"good\" plugins installed?\n",
                    8 * (int)sizeof(void *));
            goto out;
        }

        videobox = gst_element_factory_make("videobox", NULL);

        /* videoflip does not support 15 and 16-bit RGB so add a second videoconvert
         * to do the final conversion. */
        if (!(vconv2 = gst_element_factory_make("videoconvert", NULL)))
        {
            fprintf(stderr, "winegstreamer: failed to create videoconvert, are %u-bit GStreamer \"base\" plugins installed?\n",
                    8 * (int)sizeof(void *));
            goto out;
        }

        if (!parser->seekable)
        {
            if (!videobox && (stream->aperture.left || stream->aperture.top ||
                (stream->aperture.right && stream->aperture.right != stream->current_format.u.video.width) ||
                (stream->aperture.bottom && stream->aperture.bottom != stream->current_format.u.video.height)))
            {
                fprintf(stderr, "winegstreamer: failed to create videobox, are %u-bit GStreamer \"good\" plugins installed?\n",
                    8 * (int)sizeof(void *));
                goto out;
            }
            if (stream->aperture.left)
                g_object_set(G_OBJECT(videobox), "left", -stream->aperture.left, NULL);
            if (stream->aperture.bottom)
                g_object_set(G_OBJECT(videobox), "top", -stream->aperture.top, NULL);
            if (stream->aperture.right)
                g_object_set(G_OBJECT(videobox), "right", stream->aperture.right - stream->current_format.u.video.width, NULL);
            if (stream->aperture.bottom)
                g_object_set(G_OBJECT(videobox), "bottom", stream->aperture.bottom - stream->current_format.u.video.height, NULL);
        }

        /* The bin takes ownership of these elements. */
        gst_bin_add(GST_BIN(parser->container), deinterlace);
        gst_element_sync_state_with_parent(deinterlace);
        gst_bin_add(GST_BIN(parser->container), vconv);
        gst_element_sync_state_with_parent(vconv);
        gst_bin_add(GST_BIN(parser->container), flip);
        gst_element_sync_state_with_parent(flip);
        if (videobox)
        {
            gst_bin_add(GST_BIN(parser->container), videobox);
            gst_element_sync_state_with_parent(videobox);
        }
        gst_bin_add(GST_BIN(parser->container), vconv2);
        gst_element_sync_state_with_parent(vconv2);

        gst_element_link(deinterlace, vconv);
        gst_element_link(vconv, flip);
        if (videobox)
        {
            gst_element_link(flip, videobox);
            gst_element_link(videobox, vconv2);
        }
        else
        {
            gst_element_link(flip, vconv2);
        }

        stream->post_sink = gst_element_get_static_pad(deinterlace, "sink");
        stream->post_src = gst_element_get_static_pad(vconv2, "src");
        stream->flip = flip;
        stream->box = videobox;
    }
    else if (!strcmp(name, "audio/x-raw"))
    {
        GstElement *convert;

        /* Currently our dsound can't handle 64-bit formats or all
         * surround-sound configurations. Native dsound can't always handle
         * 64-bit formats either. Add an audioconvert to allow changing bit
         * depth and channel count. */
        if (!(convert = gst_element_factory_make("audioconvert", NULL)))
        {
            fprintf(stderr, "winegstreamer: failed to create audioconvert, are %u-bit GStreamer \"base\" plugins installed?\n",
                    8 * (int)sizeof(void *));
            goto out;
        }

        gst_bin_add(GST_BIN(parser->container), convert);
        gst_element_sync_state_with_parent(convert);

        stream->post_sink = gst_element_get_static_pad(convert, "sink");
        stream->post_src = gst_element_get_static_pad(convert, "src");
    }

    if (stream->post_sink)
    {
        if ((ret = gst_pad_link(pad, stream->post_sink)) < 0)
        {
            GST_ERROR("Failed to link decodebin source pad to post-processing elements, error %s.",
                    gst_pad_link_get_name(ret));
            gst_object_unref(stream->post_sink);
            stream->post_sink = NULL;
            goto out;
        }

        if ((ret = gst_pad_link(stream->post_src, stream->my_sink)) < 0)
        {
            GST_ERROR("Failed to link post-processing elements to our sink pad, error %s.",
                    gst_pad_link_get_name(ret));
            gst_object_unref(stream->post_src);
            stream->post_src = NULL;
            gst_object_unref(stream->post_sink);
            stream->post_sink = NULL;
            goto out;
        }
    }
    else if ((ret = gst_pad_link(pad, stream->my_sink)) < 0)
    {
        GST_ERROR("Failed to link decodebin source pad to our sink pad, error %s.",
                gst_pad_link_get_name(ret));
        goto out;
    }

    gst_pad_set_active(stream->my_sink, 1);
    gst_object_ref(stream->their_src = pad);
out:
    gst_caps_unref(caps);
}

static void pad_removed_cb(GstElement *element, GstPad *pad, gpointer user)
{
    struct wg_parser *parser = user;
    unsigned int i;
    char *name;

    GST_LOG("parser %p, element %p, pad %p.", parser, element, pad);

    for (i = 0; i < parser->stream_count; ++i)
    {
        struct wg_parser_stream *stream = parser->streams[i];

        if (stream->their_src == pad)
        {
            if (stream->post_sink)
                gst_pad_unlink(stream->their_src, stream->post_sink);
            else
                gst_pad_unlink(stream->their_src, stream->my_sink);
            gst_object_unref(stream->their_src);
            stream->their_src = NULL;
            return;
        }
    }

    name = gst_pad_get_name(pad);
    GST_WARNING("No pin matching pad \"%s\" found.", name);
    g_free(name);
}

static GstFlowReturn pull_data(struct wg_parser *parser, guint64 offset, guint size, guint *size_read, GstBuffer **buffer)
{
    GstBuffer *new_buffer = NULL;
    enum wg_read_result ret;
    GstMapInfo map_info;
    unsigned int i;

    GST_LOG("pad %p, offset %" G_GINT64_MODIFIER "u, length %u, buffer %p.", parser->my_src, offset, size, *buffer);

    if (offset == GST_BUFFER_OFFSET_NONE)
        offset = parser->next_pull_offset;
    parser->next_pull_offset = offset + size;
    if (parser->seekable)
    {
        if (offset >= parser->file_size)
            return GST_FLOW_EOS;
        if (offset + size >= parser->file_size)
            size = parser->file_size - offset;
    }

    if (!*buffer)
        *buffer = new_buffer = gst_buffer_new_and_alloc(size);

    gst_buffer_map(*buffer, &map_info, GST_MAP_WRITE);

    pthread_mutex_lock(&parser->mutex);

    if (parser->draining)
    {
        gst_pad_peer_query(parser->my_src, gst_query_new_drain());
        parser->draining = false;
        for (i = 0; i < parser->stream_count; i++)
            pthread_cond_signal(&parser->streams[i]->event_cond);
    }

    assert(!parser->read_request.data);
    parser->read_request.data = map_info.data;
    parser->read_request.offset = offset;
    parser->read_request.size = size;
    parser->read_request.done = false;
    pthread_cond_signal(&parser->read_cond);

    /* Note that we don't unblock this wait on GST_EVENT_FLUSH_START. We expect
     * the upstream pin to flush if necessary. We should never be blocked on
     * read_thread() not running. */

    while (!parser->read_request.done && !parser->draining)
    {
        pthread_cond_wait(&parser->read_done_cond, &parser->mutex);
        if (parser->draining)
        {
            gst_pad_peer_query(parser->my_src, gst_query_new_drain());
            parser->draining = false;
            for (i = 0; i < parser->stream_count; i++)
                pthread_cond_signal(&parser->streams[i]->event_cond);
        }
        if (size != parser->read_request.size && parser->read_request.data)
        {
            if (size_read)
            {
                *size_read = size - parser->read_request.size;
                parser->read_request.done = true;
                break;
            }
            else
            {
                parser->read_request.done = false;
                pthread_cond_signal(&parser->read_cond);
            }
        }
    }

    parser->read_request.data = NULL;

    if (size_read && parser->read_request.size > size)
        *size_read = parser->read_request.size;

    ret = parser->read_request.ret;

    pthread_mutex_unlock(&parser->mutex);

    gst_buffer_unmap(*buffer, &map_info);

    if (size_read)
        size = min(*size_read, size);

    gst_buffer_set_size(*buffer, size);

    GST_LOG("Request returned %d.", ret);

    if (ret != WG_READ_SUCCESS && new_buffer)
        gst_buffer_unref(new_buffer);

    return ret == WG_READ_SUCCESS ? GST_FLOW_OK :
           ret == WG_READ_FAILURE ? GST_FLOW_ERROR :
           ret == WG_READ_FLUSHING ? GST_FLOW_FLUSHING :
           ret == WG_READ_EOS ? GST_FLOW_EOS :
           GST_FLOW_ERROR;
}

static GstFlowReturn src_getrange_cb(GstPad *pad, GstObject *parent,
        guint64 offset, guint size, GstBuffer **buffer)
{
    struct wg_parser *parser = gst_pad_get_element_private(pad);

    return pull_data(parser, offset, size, NULL, buffer);
}

static gboolean src_query_cb(GstPad *pad, GstObject *parent, GstQuery *query)
{
    struct wg_parser *parser = gst_pad_get_element_private(pad);
    GstFormat format;

    GST_LOG("parser %p, type %s.", parser, GST_QUERY_TYPE_NAME(query));

    switch (GST_QUERY_TYPE(query))
    {
        case GST_QUERY_DURATION:
            gst_query_parse_duration(query, &format, NULL);
            if (format == GST_FORMAT_PERCENT)
            {
                gst_query_set_duration(query, GST_FORMAT_PERCENT, GST_FORMAT_PERCENT_MAX);
                return TRUE;
            }
            else if (format == GST_FORMAT_BYTES && parser->seekable)
            {
                gst_query_set_duration(query, GST_FORMAT_BYTES, parser->file_size);
                return TRUE;
            }
            return FALSE;

        case GST_QUERY_SEEKING:
            gst_query_parse_seeking (query, &format, NULL, NULL, NULL);
            if (format != GST_FORMAT_BYTES)
            {
                GST_WARNING("Cannot seek using format \"%s\".", gst_format_get_name(format));
                return FALSE;
            }
            if (!parser->seekable)
                return FALSE;
            gst_query_set_seeking(query, GST_FORMAT_BYTES, 1, 0, parser->file_size);
            return TRUE;

        case GST_QUERY_SCHEDULING:
            gst_query_set_scheduling(query, parser->seekable ? GST_SCHEDULING_FLAG_SEEKABLE : GST_SCHEDULING_FLAG_SEQUENTIAL, 1, -1, 0);
            gst_query_add_scheduling_mode(query, GST_PAD_MODE_PUSH);
            gst_query_add_scheduling_mode(query, GST_PAD_MODE_PULL);
            return TRUE;

        case GST_QUERY_CAPS:
        {
            GstCaps *caps, *filter, *temp;

            gst_query_parse_caps(query, &filter);

            if (parser->input_format.major_type)
                caps = wg_format_to_caps(&parser->input_format);
            else
                caps = gst_caps_new_any();
            if (!caps)
                return FALSE;

            if (filter)
            {
                temp = gst_caps_intersect(caps, filter);
                gst_caps_unref(caps);
                caps = temp;
            }

            gst_query_set_caps_result(query, caps);
            gst_caps_unref(caps);
            return TRUE;
        }

        default:
            GST_WARNING("Unhandled query type %s.", GST_QUERY_TYPE_NAME(query));
            return FALSE;
    }
}

static void *push_data(void *arg)
{
    struct wg_parser *parser = arg;
    GstBuffer *last_buffer = NULL;
    ULONG alloc_size = 16384;
    GstSegment *segment;
    guint max_size;

    GST_DEBUG("Starting push thread.");

    max_size = parser->stop_offset ? parser->stop_offset : parser->file_size;

    gst_pad_push_event(parser->my_src, gst_event_new_stream_start("wg_stream"));

    if (parser->input_format.major_type)
        gst_pad_push_event(parser->my_src, gst_event_new_caps(wg_format_to_caps(&parser->input_format)));

    segment = gst_segment_new();
    gst_segment_init(segment, GST_FORMAT_BYTES);
    gst_pad_push_event(parser->my_src, gst_event_new_segment(segment));

    assert(!(GST_PAD_IS_FLUSHING(parser->my_src)));

    for (;;)
    {
        GstBuffer *buffer = NULL;
        unsigned int i;
        ULONG size;
        int ret;

        if (parser->seekable && parser->next_offset >= max_size)
            break;
        size = parser->seekable ? min(alloc_size, max_size - parser->next_offset) : alloc_size;

        ret = pull_data(parser, parser->next_offset, size, &size, &buffer);

        /* When we are in unseekable push mode, the pushing pad is responsible for handling flushing.  */
        if (!parser->seekable && ret == GST_FLOW_FLUSHING)
        {
            if (last_buffer)
            {
                gst_buffer_unref(last_buffer);
                last_buffer = NULL;
            }

            gst_pad_push_event(parser->my_src, gst_event_new_seek(1.0f,
                GST_FORMAT_BYTES, GST_SEEK_FLAG_FLUSH, GST_SEEK_TYPE_NONE, 0, GST_SEEK_TYPE_NONE, 0));

            continue;
        }

        if (!parser->seekable && ret == GST_FLOW_EOS)
        {
            if (last_buffer)
            {
                gst_buffer_unref(last_buffer);
                last_buffer = NULL;
            }

            gst_pad_push_event(parser->my_src, gst_event_new_eos());
            pthread_mutex_lock(&parser->mutex);
            for (i = 0; i < parser->stream_count; i++)
            {
                if (!parser->streams[i]->enabled)
                    continue;
                while (!parser->streams[i]->flushing && !parser->streams[i]->eos)
                    pthread_cond_wait(&parser->streams[i]->event_empty_cond, &parser->mutex);
                parser->streams[i]->eos = false;
            }

            if (parser->flushing)
            {
                pthread_mutex_unlock(&parser->mutex);
                continue;
            }

            pthread_mutex_unlock(&parser->mutex);

            segment = gst_segment_new();
            gst_segment_init(segment, GST_FORMAT_BYTES);
            gst_pad_push_event(parser->my_src, gst_event_new_segment(segment));

            continue;
        }

        if (ret < 0)
        {
            if (last_buffer)
            {
                gst_buffer_unref(last_buffer);
                last_buffer = NULL;
            }

            GST_ERROR("Failed to read data, ret %s.", gst_flow_get_name(ret));
            break;
        }

        parser->next_offset += gst_buffer_get_size(buffer);

        if (last_buffer)
        {
            buffer = gst_buffer_append(last_buffer, buffer);
            last_buffer = NULL;
            assert(alloc_size >= size);
        }

        if (size > gst_buffer_get_size(buffer))
        {
            last_buffer = buffer;
            alloc_size = (size + 0xfff) & ~0xfff;
            continue;
        }

        buffer->duration = buffer->pts = -1;
        if ((ret = gst_pad_push(parser->my_src, buffer)) < 0)
        {
            GST_ERROR("Failed to push data, ret %s.", gst_flow_get_name(ret));
            break;
        }
    }

    gst_pad_push_event(parser->my_src, gst_event_new_eos());

    GST_DEBUG("Stopping push thread.");

    return NULL;
}

static gboolean activate_push(GstPad *pad, gboolean activate)
{
    struct wg_parser *parser = gst_pad_get_element_private(pad);

    if (!activate)
    {
        if (parser->push_thread)
        {
            pthread_mutex_lock(&parser->mutex);
            parser->read_request.ret = WG_READ_FAILURE;
            parser->read_request.data = NULL;
            parser->read_request.done = true;
            pthread_mutex_unlock(&parser->mutex);
            pthread_cond_signal(&parser->read_done_cond);
            pthread_join(parser->push_thread, NULL);
            parser->push_thread = 0;
        }
    }
    else if (!parser->push_thread)
    {
        int ret;

        if ((ret = pthread_create(&parser->push_thread, NULL, push_data, parser)))
        {
            GST_ERROR("Failed to create push thread: %s", strerror(errno));
            parser->push_thread = 0;
            return FALSE;
        }
    }
    return TRUE;
}

static gboolean src_activate_mode_cb(GstPad *pad, GstObject *parent, GstPadMode mode, gboolean activate)
{
    struct wg_parser *parser = gst_pad_get_element_private(pad);

    GST_DEBUG("%s source pad for parser %p in %s mode.",
            activate ? "Activating" : "Deactivating", parser, gst_pad_mode_get_name(mode));

    parser->pull_mode = false;

    switch (mode)
    {
        case GST_PAD_MODE_PULL:
            parser->pull_mode = activate;
            return TRUE;
        case GST_PAD_MODE_PUSH:
            return activate_push(pad, activate);
        case GST_PAD_MODE_NONE:
            break;
    }
    return FALSE;
}

static GstBusSyncReply bus_handler_cb(GstBus *bus, GstMessage *msg, gpointer user)
{
    struct wg_parser *parser = user;
    gchar *dbg_info = NULL;
    GError *err = NULL;

    GST_DEBUG("parser %p, message type %s.", parser, GST_MESSAGE_TYPE_NAME(msg));

    switch (msg->type)
    {
    case GST_MESSAGE_ERROR:
        gst_message_parse_error(msg, &err, &dbg_info);
        fprintf(stderr, "winegstreamer error: %s: %s\n", GST_OBJECT_NAME(msg->src), err->message);
        fprintf(stderr, "winegstreamer error: %s: %s\n", GST_OBJECT_NAME(msg->src), dbg_info);
        g_error_free(err);
        g_free(dbg_info);
        pthread_mutex_lock(&parser->mutex);
        parser->error = true;
        pthread_mutex_unlock(&parser->mutex);
        pthread_cond_signal(&parser->init_cond);
        break;

    case GST_MESSAGE_WARNING:
        gst_message_parse_warning(msg, &err, &dbg_info);
        fprintf(stderr, "winegstreamer warning: %s: %s\n", GST_OBJECT_NAME(msg->src), err->message);
        fprintf(stderr, "winegstreamer warning: %s: %s\n", GST_OBJECT_NAME(msg->src), dbg_info);
        g_error_free(err);
        g_free(dbg_info);
        break;

    case GST_MESSAGE_DURATION_CHANGED:
        pthread_mutex_lock(&parser->mutex);
        parser->has_duration = true;
        pthread_mutex_unlock(&parser->mutex);
        pthread_cond_signal(&parser->init_cond);
        break;

    default:
        break;
    }
    gst_message_unref(msg);
    return GST_BUS_DROP;
}

static gboolean src_perform_seek(struct wg_parser *parser, GstEvent *event)
{
    BOOL thread = !!parser->push_thread;
    GstSeekType cur_type, stop_type;
    GstFormat seek_format;
    GstEvent *flush_event;
    GstSeekFlags flags;
    gint64 cur, stop;
    GstSegment *seg;
    guint32 seqnum;
    gdouble rate;

    gst_event_parse_seek(event, &rate, &seek_format, &flags,
                         &cur_type, &cur, &stop_type, &stop);

    if (seek_format != GST_FORMAT_BYTES)
    {
        GST_FIXME("Unhandled format \"%s\".", gst_format_get_name(seek_format));
        return FALSE;
    }

    seqnum = gst_event_get_seqnum(event);

    /* send flush start */
    if (flags & GST_SEEK_FLAG_FLUSH)
    {
        flush_event = gst_event_new_flush_start();
        gst_event_set_seqnum(flush_event, seqnum);
        gst_pad_push_event(parser->my_src, flush_event);
        if (thread)
            gst_pad_set_active(parser->my_src, 1);
    }

    parser->next_offset = parser->start_offset = cur;

    /* and prepare to continue streaming */
    if (flags & GST_SEEK_FLAG_FLUSH)
    {
        flush_event = gst_event_new_flush_stop(TRUE);
        gst_event_set_seqnum(flush_event, seqnum);
        gst_pad_push_event(parser->my_src, flush_event);
        if (thread)
        {
            gst_pad_set_active(parser->my_src, 1);
            seg = gst_segment_new();
            gst_segment_init(seg, GST_FORMAT_BYTES);
            gst_pad_push_event(parser->my_src, gst_event_new_segment(seg));
        }
    }

    return TRUE;
}

static gboolean src_event_cb(GstPad *pad, GstObject *parent, GstEvent *event)
{
    struct wg_parser *parser = gst_pad_get_element_private(pad);
    gboolean ret = TRUE;

    GST_LOG("parser %p, type \"%s\".", parser, GST_EVENT_TYPE_NAME(event));

    switch (event->type)
    {
        case GST_EVENT_SEEK:
            ret = src_perform_seek(parser, event);
            break;

        case GST_EVENT_FLUSH_START:
        case GST_EVENT_FLUSH_STOP:
        case GST_EVENT_QOS:
        case GST_EVENT_RECONFIGURE:
            break;

        default:
            GST_WARNING("Ignoring \"%s\" event.", GST_EVENT_TYPE_NAME(event));
            ret = FALSE;
            break;
    }
    gst_event_unref(event);
    return ret;
}

static LONGLONG query_duration(GstPad *pad)
{
    gint64 duration, byte_length;

    if (gst_pad_query_duration(pad, GST_FORMAT_TIME, &duration))
        return duration / 100;

    WARN("Failed to query time duration; trying to convert from byte length.\n");

    /* To accurately get a duration for the stream, we want to only consider the
     * length of that stream. Hence, query for the pad duration, instead of
     * using the file duration. */
    if (gst_pad_query_duration(pad, GST_FORMAT_BYTES, &byte_length)
            && gst_pad_query_convert(pad, GST_FORMAT_BYTES, byte_length, GST_FORMAT_TIME, &duration))
        return duration / 100;

    ERR("Failed to query duration.\n");
    return 0;
}

static gchar *query_language(GstPad *pad)
{
    GstTagList *tag_list;
    GstEvent *tag_event;
    gchar *ret = NULL;

    if ((tag_event = gst_pad_get_sticky_event(pad, GST_EVENT_TAG, 0)))
    {
        gst_event_parse_tag(tag_event, &tag_list);
        gst_tag_list_get_string(tag_list, "language-code", &ret);
        gst_event_unref(tag_event);
    }

    return ret;
}

static HRESULT wg_parser_connect_inner(struct wg_parser *parser)
{
    GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE("wine_src",
            GST_PAD_SRC, GST_PAD_ALWAYS, GST_STATIC_CAPS_ANY);

    parser->sink_connected = true;

    if (!parser->bus)
    {
        parser->bus = gst_bus_new();
        gst_bus_set_sync_handler(parser->bus, bus_handler_cb, parser, NULL);
    }

    parser->container = gst_bin_new(NULL);
    gst_element_set_bus(parser->container, parser->bus);

    parser->my_src = gst_pad_new_from_static_template(&src_template, "wine-src");
    gst_pad_set_getrange_function(parser->my_src, src_getrange_cb);
    gst_pad_set_query_function(parser->my_src, src_query_cb);
    gst_pad_set_activatemode_function(parser->my_src, src_activate_mode_cb);
    gst_pad_set_event_function(parser->my_src, src_event_cb);
    gst_pad_set_element_private(parser->my_src, parser);

    parser->start_offset = parser->next_offset = parser->stop_offset = 0;
    parser->next_pull_offset = 0;

    return S_OK;
}

static HRESULT CDECL wg_parser_connect(struct wg_parser *parser, uint64_t file_size)
{
    unsigned int i;
    HRESULT hr;

    parser->seekable = true;
    parser->file_size = file_size;

    if ((hr = wg_parser_connect_inner(parser)))
        return hr;

    if (!parser->init_gst(parser))
        return E_FAIL;

    pthread_mutex_lock(&parser->mutex);

    for (i = 0; i < parser->stream_count; ++i)
    {
        struct wg_parser_stream *stream = parser->streams[i];

        stream->language_code = query_language(stream->their_src);
        while (!stream->has_caps && !parser->error)
            pthread_cond_wait(&parser->init_cond, &parser->mutex);
        if (parser->error)
        {
            pthread_mutex_unlock(&parser->mutex);
            return E_FAIL;
        }
        /* GStreamer doesn't actually provide any guarantees about when duration
         * is available, even for seekable streams. However, many elements (e.g.
         * avidemux, wavparse, qtdemux) in practice record duration before
         * fixing caps, so as a heuristic, wait until we get caps before trying
         * to query for duration. */
        stream->duration = query_duration(stream->their_src);
    }

    pthread_mutex_unlock(&parser->mutex);

    parser->next_offset = 0;
    return S_OK;
}

static HRESULT CDECL wg_parser_connect_unseekable(struct wg_parser *parser,
            const struct wg_format *in_format, uint32_t stream_count, const struct wg_format *out_formats, const struct wg_rect *apertures)
{
    unsigned int i;
    HRESULT hr;

    parser->seekable = false;
    parser->flushing = false;
    /* since typefind is not available here, we must have an input_format */
    parser->input_format = *in_format;

    if ((hr = wg_parser_connect_inner(parser)))
        return hr;

    parser->stop_offset = -1;

    parser->expected_stream_count = stream_count;
    parser->streams = calloc(stream_count, sizeof(*parser->streams));

    for (i = 0; i < stream_count; i++)
    {
        parser->streams[i] = calloc(1, sizeof(*parser->streams[i]));
        parser->streams[i]->current_format = out_formats[i];
        if (apertures)
            parser->streams[i]->aperture = apertures[i];
        parser->streams[i]->enabled = true;
    }

    if (!parser->init_gst(parser))
        return E_FAIL;

    if (parser->stream_count < parser->expected_stream_count)
        return E_FAIL;

    return S_OK;
}

static void free_stream(struct wg_parser_stream *stream)
{
    if (stream->their_src)
    {
        if (stream->post_sink)
        {
            gst_pad_unlink(stream->their_src, stream->post_sink);
            gst_pad_unlink(stream->post_src, stream->my_sink);
            gst_object_unref(stream->post_src);
            gst_object_unref(stream->post_sink);
            stream->post_src = stream->post_sink = NULL;
        }
        else
            gst_pad_unlink(stream->their_src, stream->my_sink);
        gst_object_unref(stream->their_src);
    }
    gst_object_unref(stream->my_sink);

    pthread_cond_destroy(&stream->event_cond);
    pthread_cond_destroy(&stream->event_empty_cond);

    if (stream->language_code)
        g_free(stream->language_code);

    free(stream);
}

static void CDECL wg_parser_disconnect(struct wg_parser *parser)
{
    unsigned int i;

    /* Unblock all of our streams. */
    pthread_mutex_lock(&parser->mutex);
    for (i = 0; i < parser->stream_count; ++i)
    {
        parser->streams[i]->flushing = true;
        pthread_cond_signal(&parser->streams[i]->event_empty_cond);
    }
    pthread_mutex_unlock(&parser->mutex);

    gst_element_set_state(parser->container, GST_STATE_NULL);
    if (!parser->pull_mode)
        gst_pad_set_active(parser->my_src, 0);
    gst_pad_unlink(parser->my_src, parser->their_sink);
    gst_object_unref(parser->my_src);
    gst_object_unref(parser->their_sink);
    parser->my_src = parser->their_sink = NULL;

    pthread_mutex_lock(&parser->mutex);
    parser->sink_connected = false;
    pthread_mutex_unlock(&parser->mutex);
    pthread_cond_signal(&parser->read_cond);

    for (i = 0; i < parser->stream_count; ++i)
        free_stream(parser->streams[i]);

    parser->stream_count = 0;
    free(parser->streams);
    parser->streams = NULL;

    gst_element_set_bus(parser->container, NULL);
    gst_object_unref(parser->container);
    parser->container = NULL;
}

static BOOL decodebin_parser_init_gst(struct wg_parser *parser)
{
    GstElement *element = gst_element_factory_make("decodebin", NULL);
    int ret;

    if (!element)
    {
        ERR("Failed to create decodebin; are %u-bit GStreamer \"base\" plugins installed?\n",
                8 * (int)sizeof(void*));
        return FALSE;
    }

    if (parser->input_format.major_type)
        g_object_set(G_OBJECT(element), "sink-caps", wg_format_to_caps(&parser->input_format), NULL);

    gst_bin_add(GST_BIN(parser->container), element);
    parser->decodebin = element;

    g_signal_connect(element, "pad-added", G_CALLBACK(pad_added_cb), parser);
    g_signal_connect(element, "pad-removed", G_CALLBACK(pad_removed_cb), parser);
    g_signal_connect(element, "autoplug-select", G_CALLBACK(autoplug_select_cb), parser);
    g_signal_connect(element, "no-more-pads", G_CALLBACK(no_more_pads_cb), parser);

    parser->their_sink = gst_element_get_static_pad(element, "sink");

    pthread_mutex_lock(&parser->mutex);
    parser->no_more_pads = parser->error = false;
    pthread_mutex_unlock(&parser->mutex);

    if ((ret = gst_pad_link(parser->my_src, parser->their_sink)) < 0)
    {
        ERR("Failed to link pads, error %d.\n", ret);
        return FALSE;
    }

    gst_element_set_state(parser->container, GST_STATE_PAUSED);
    if (!parser->pull_mode)
        gst_pad_set_active(parser->my_src, 1);
    ret = gst_element_get_state(parser->container, NULL, NULL, -1);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        ERR("Failed to play stream.\n");
        return FALSE;
    }

    pthread_mutex_lock(&parser->mutex);
    while (!parser->no_more_pads && !parser->error)
        pthread_cond_wait(&parser->init_cond, &parser->mutex);
    if (parser->error)
    {
        pthread_mutex_unlock(&parser->mutex);
        return FALSE;
    }
    pthread_mutex_unlock(&parser->mutex);

    return TRUE;
}

static BOOL avi_parser_init_gst(struct wg_parser *parser)
{
    GstElement *element = gst_element_factory_make("avidemux", NULL);
    int ret;

    if (!element)
    {
        ERR("Failed to create avidemux; are %u-bit GStreamer \"good\" plugins installed?\n",
                8 * (int)sizeof(void*));
        return FALSE;
    }

    gst_bin_add(GST_BIN(parser->container), element);

    g_signal_connect(element, "pad-added", G_CALLBACK(pad_added_cb), parser);
    g_signal_connect(element, "pad-removed", G_CALLBACK(pad_removed_cb), parser);
    g_signal_connect(element, "no-more-pads", G_CALLBACK(no_more_pads_cb), parser);

    parser->their_sink = gst_element_get_static_pad(element, "sink");

    pthread_mutex_lock(&parser->mutex);
    parser->no_more_pads = parser->error = false;
    pthread_mutex_unlock(&parser->mutex);

    if ((ret = gst_pad_link(parser->my_src, parser->their_sink)) < 0)
    {
        ERR("Failed to link pads, error %d.\n", ret);
        return FALSE;
    }

    gst_element_set_state(parser->container, GST_STATE_PAUSED);
    if (!parser->pull_mode)
        gst_pad_set_active(parser->my_src, 1);
    ret = gst_element_get_state(parser->container, NULL, NULL, -1);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        ERR("Failed to play stream.\n");
        return FALSE;
    }

    pthread_mutex_lock(&parser->mutex);
    while (!parser->no_more_pads && !parser->error)
        pthread_cond_wait(&parser->init_cond, &parser->mutex);
    if (parser->error)
    {
        pthread_mutex_unlock(&parser->mutex);
        return FALSE;
    }
    pthread_mutex_unlock(&parser->mutex);

    return TRUE;
}

static BOOL mpeg_audio_parser_init_gst(struct wg_parser *parser)
{
    struct wg_parser_stream *stream;
    GstElement *element;
    int ret;

    if (!(element = gst_element_factory_make("mpegaudioparse", NULL)))
    {
        ERR("Failed to create mpegaudioparse; are %u-bit GStreamer \"good\" plugins installed?\n",
                8 * (int)sizeof(void*));
        return FALSE;
    }

    gst_bin_add(GST_BIN(parser->container), element);

    parser->their_sink = gst_element_get_static_pad(element, "sink");
    if ((ret = gst_pad_link(parser->my_src, parser->their_sink)) < 0)
    {
        ERR("Failed to link sink pads, error %d.\n", ret);
        return FALSE;
    }

    if (!(stream = create_stream(parser)))
        return FALSE;

    gst_object_ref(stream->their_src = gst_element_get_static_pad(element, "src"));
    if ((ret = gst_pad_link(stream->their_src, stream->my_sink)) < 0)
    {
        ERR("Failed to link source pads, error %d.\n", ret);
        return FALSE;
    }

    gst_pad_set_active(stream->my_sink, 1);
    gst_element_set_state(parser->container, GST_STATE_PAUSED);
    if (!parser->pull_mode)
        gst_pad_set_active(parser->my_src, 1);
    ret = gst_element_get_state(parser->container, NULL, NULL, -1);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        ERR("Failed to play stream.\n");
        return FALSE;
    }

    pthread_mutex_lock(&parser->mutex);
    while (!parser->has_duration && !parser->error && !stream->eos)
        pthread_cond_wait(&parser->init_cond, &parser->mutex);
    if (parser->error)
    {
        pthread_mutex_unlock(&parser->mutex);
        return FALSE;
    }
    pthread_mutex_unlock(&parser->mutex);
    return TRUE;
}

static BOOL wave_parser_init_gst(struct wg_parser *parser)
{
    struct wg_parser_stream *stream;
    GstElement *element;
    int ret;

    if (!(element = gst_element_factory_make("wavparse", NULL)))
    {
        ERR("Failed to create wavparse; are %u-bit GStreamer \"good\" plugins installed?\n",
                8 * (int)sizeof(void*));
        return FALSE;
    }

    gst_bin_add(GST_BIN(parser->container), element);

    parser->their_sink = gst_element_get_static_pad(element, "sink");
    if ((ret = gst_pad_link(parser->my_src, parser->their_sink)) < 0)
    {
        ERR("Failed to link sink pads, error %d.\n", ret);
        return FALSE;
    }

    if (!(stream = create_stream(parser)))
        return FALSE;

    stream->their_src = gst_element_get_static_pad(element, "src");
    gst_object_ref(stream->their_src);
    if ((ret = gst_pad_link(stream->their_src, stream->my_sink)) < 0)
    {
        ERR("Failed to link source pads, error %d.\n", ret);
        return FALSE;
    }

    gst_pad_set_active(stream->my_sink, 1);
    gst_element_set_state(parser->container, GST_STATE_PAUSED);
    if (!parser->pull_mode)
        gst_pad_set_active(parser->my_src, 1);
    ret = gst_element_get_state(parser->container, NULL, NULL, -1);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        ERR("Failed to play stream.\n");
        return FALSE;
    }

    return TRUE;
}

static BOOL raw_media_converter_init_gst(struct wg_parser *parser)
{
    BOOL video = parser->input_format.major_type == WG_MAJOR_TYPE_VIDEO;
    struct wg_parser_stream *stream;
    GstElement *convert, *resampler;
    GstPad *their_src;
    int ret;

    if (parser->seekable)
        return FALSE;

    if (parser->expected_stream_count != 1)
        return FALSE;

    if (video)
    {
        if (!(convert = gst_element_factory_make("videoconvert", NULL)))
        {
            ERR("Failed to create videoconvert; are %u-bit GStreamer \"base\" plugins installed?\n",
                    8 * (int)sizeof(void*));
            return FALSE;
        }

        gst_bin_add(GST_BIN(parser->container), convert);

        parser->their_sink = gst_element_get_static_pad(convert, "sink");
        their_src = gst_element_get_static_pad(convert, "src");
    }
    else
    {
        if (!(convert = gst_element_factory_make("audioconvert", NULL)))
        {
            ERR("Failed to create audioconvert; are %u-bit GStreamer \"base\" plugins installed?\n",
                    8 * (int)sizeof(void*));
            return FALSE;
        }

        gst_bin_add(GST_BIN(parser->container), convert);

        if (!(resampler = gst_element_factory_make("audioresample", NULL)))
        {
            ERR("Failed to create audioresample; are %u-bit GStreamer \"base\" plugins installed?\n",
                    8 * (int)sizeof(void*));
            return FALSE;
        }

        gst_bin_add(GST_BIN(parser->container), resampler);

        gst_element_link(convert, resampler);
        parser->their_sink = gst_element_get_static_pad(convert, "sink");
        their_src = gst_element_get_static_pad(resampler, "src");
    }

    if ((ret = gst_pad_link(parser->my_src, parser->their_sink)) < 0)
    {
        ERR("Failed to link sink pads, error %d.\n", ret);
        return FALSE;
    }

    if (!(stream = create_stream(parser)))
        return FALSE;

    stream->their_src = their_src;
    gst_object_ref(stream->their_src);
    if ((ret = gst_pad_link(stream->their_src, stream->my_sink)) < 0)
    {
        ERR("Failed to link source pads, error %d.\n", ret);
        return FALSE;
    }

    gst_pad_set_active(stream->my_sink, 1);
    gst_element_set_state(parser->container, GST_STATE_PAUSED);
    gst_pad_set_active(parser->my_src, 1);
    ret = gst_element_get_state(parser->container, NULL, NULL, -1);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        ERR("Failed to play stream.\n");
        return FALSE;
    }

    return TRUE;
}

static struct wg_parser *wg_parser_create(void)
{
    struct wg_parser *parser;

    if (!(parser = calloc(1, sizeof(*parser))))
        return NULL;

    pthread_mutex_init(&parser->mutex, NULL);
    pthread_cond_init(&parser->init_cond, NULL);
    pthread_cond_init(&parser->read_cond, NULL);
    pthread_cond_init(&parser->read_done_cond, NULL);
    parser->flushing = true;

    TRACE("Created winegstreamer parser %p.\n", parser);
    return parser;
}

static struct wg_parser * CDECL wg_decodebin_parser_create(void)
{
    struct wg_parser *parser;

    if ((parser = wg_parser_create()))
        parser->init_gst = decodebin_parser_init_gst;
    return parser;
}

static struct wg_parser * CDECL wg_avi_parser_create(void)
{
    struct wg_parser *parser;

    if ((parser = wg_parser_create()))
        parser->init_gst = avi_parser_init_gst;
    return parser;
}

static struct wg_parser * CDECL wg_mpeg_audio_parser_create(void)
{
    struct wg_parser *parser;

    if ((parser = wg_parser_create()))
        parser->init_gst = mpeg_audio_parser_init_gst;
    return parser;
}

static struct wg_parser * CDECL wg_wave_parser_create(void)
{
    struct wg_parser *parser;

    if ((parser = wg_parser_create()))
        parser->init_gst = wave_parser_init_gst;
    return parser;
}

static struct wg_parser * CDECL wg_raw_media_converter_create(void)
{
    struct wg_parser *parser;

    if ((parser = wg_parser_create()))
        parser->init_gst = raw_media_converter_init_gst;
    return parser;
}

static void CDECL wg_parser_destroy(struct wg_parser *parser)
{
    if (parser->bus)
    {
        gst_bus_set_sync_handler(parser->bus, NULL, NULL, NULL);
        gst_object_unref(parser->bus);
    }

    pthread_mutex_destroy(&parser->mutex);
    pthread_cond_destroy(&parser->init_cond);
    pthread_cond_destroy(&parser->read_cond);
    pthread_cond_destroy(&parser->read_done_cond);

    free(parser);
}

static const struct unix_funcs funcs =
{
    wg_decodebin_parser_create,
    wg_avi_parser_create,
    wg_mpeg_audio_parser_create,
    wg_wave_parser_create,
    wg_raw_media_converter_create,
    wg_parser_destroy,

    wg_parser_connect,
    wg_parser_connect_unseekable,
    wg_parser_disconnect,

    wg_parser_begin_flush,
    wg_parser_end_flush,

    wg_parser_get_read_request,
    wg_parser_complete_read_request,

    wg_parser_set_unlimited_buffering,

    wg_parser_get_stream_count,
    wg_parser_get_stream,

    wg_parser_stream_get_preferred_format,
    wg_parser_stream_enable,
    wg_parser_stream_disable,

    wg_parser_stream_get_event,
    wg_parser_stream_copy_buffer,
    wg_parser_stream_release_buffer,
    wg_parser_stream_notify_qos,

    wg_parser_stream_get_duration,
    wg_parser_stream_get_language,
    wg_parser_stream_seek,
    wg_parser_stream_drain,
};

NTSTATUS CDECL __wine_init_unix_lib(HMODULE module, DWORD reason, const void *ptr_in, void *ptr_out)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        char arg0[] = "wine";
        char arg1[] = "--gst-disable-registry-fork";
        char *args[] = {arg0, arg1, NULL};
        int argc = ARRAY_SIZE(args) - 1;
        char **argv = args;
        GError *err;

        if (!gst_init_check(&argc, &argv, &err))
        {
            ERR("Failed to initialize GStreamer: %s\n", debugstr_a(err->message));
            g_error_free(err);
            return STATUS_UNSUCCESSFUL;
        }
        TRACE("GStreamer library version %s; wine built with %d.%d.%d.\n",
                gst_version_string(), GST_VERSION_MAJOR, GST_VERSION_MINOR, GST_VERSION_MICRO);

        GST_DEBUG_CATEGORY_INIT(wine, "WINE", GST_DEBUG_FG_RED, "Wine GStreamer support");

        *(const struct unix_funcs **)ptr_out = &funcs;
    }
    return STATUS_SUCCESS;
}
