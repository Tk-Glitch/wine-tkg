/*
 * GStreamer YUV color info fixup filter
 *
 * Copyright 2020 Akihiro Sagawa
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

#include "gst_private.h"
#include <gst/base/gstbasetransform.h>

typedef struct _GstWineYuvFixup GstWineYuvFixup;
typedef struct _GstWineYuvFixupClass GstWineYuvFixupClass;

#define GST_TYPE_WINE_YUVFIXUP                  \
    (gst_wine_yuvfixup_get_type())

struct _GstWineYuvFixup {
    GstBaseTransform basetransform;
};

struct _GstWineYuvFixupClass {
    GstBaseTransformClass parent_class;
};

#define GST_CAT_DEFAULT wine_yuvfixup
GST_DEBUG_CATEGORY_STATIC(GST_CAT_DEFAULT);

static GstCaps *
gst_wine_yuvfixup_transform_caps(GstBaseTransform *trans,
                                 GstPadDirection direction,
                                 GstCaps *caps,
                                 GstCaps *filter);

static GstStaticPadTemplate gst_wine_yuvfixup_sink_template =
    GST_STATIC_PAD_TEMPLATE("sink",
                            GST_PAD_SINK,
                            GST_PAD_ALWAYS,
                            GST_STATIC_CAPS(GST_VIDEO_CAPS_MAKE(GST_VIDEO_FORMATS_ALL)));
static GstStaticPadTemplate gst_wine_yuvfixup_src_template =
    GST_STATIC_PAD_TEMPLATE("src",
                            GST_PAD_SRC,
                            GST_PAD_ALWAYS,
                            GST_STATIC_CAPS(GST_VIDEO_CAPS_MAKE(GST_VIDEO_FORMATS_ALL)));

#define gst_wine_yuvfixup_parent_class parent_class
G_DEFINE_TYPE(GstWineYuvFixup, gst_wine_yuvfixup, GST_TYPE_BASE_TRANSFORM);

static void
gst_wine_yuvfixup_class_init(GstWineYuvFixupClass *klass)
{
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
    GstBaseTransformClass *base_transform_class =
        GST_BASE_TRANSFORM_CLASS(klass);

    GST_DEBUG_CATEGORY_INIT(wine_yuvfixup, "yuvfixup", 0, "YUV Fixup");
    GST_DEBUG("gst_wine_yuvfixup_class_init");

    gst_element_class_add_static_pad_template(element_class,
                                              &gst_wine_yuvfixup_src_template);
    gst_element_class_add_static_pad_template(element_class,
                                              &gst_wine_yuvfixup_sink_template);

    gst_element_class_set_static_metadata(element_class,
                                          "Wine YUV color info fixup",
                                          "Filter/Converter/Video",
                                          "Fix up YUV color info caps",
                                          "Wine Team <wine-devel@winehq.org");

    base_transform_class->transform_caps =
        GST_DEBUG_FUNCPTR(gst_wine_yuvfixup_transform_caps);
}

static void
gst_wine_yuvfixup_init(GstWineYuvFixup *yuvfixup)
{
}

static GstCaps * get_upstream_caps(GstBaseTransform *trans)
{
    GstPad *peer_pad = NULL, *upstream_sink = NULL, *upstream_src = NULL;
    GstElement *peer_element = NULL;
    GstCaps *upstream_caps = NULL;

    if (!(peer_pad = gst_pad_get_peer(GST_BASE_TRANSFORM_SINK_PAD(trans))))
    {
        GST_DEBUG_OBJECT(trans, "peer pad not found");
        goto exit;
    }
    if (!(peer_element = gst_pad_get_parent_element(peer_pad)))
    {
        GST_DEBUG_OBJECT(trans, "peer element not found");
        goto exit;
    }
    if (!(upstream_sink = gst_element_get_static_pad(peer_element, "sink")))
    {
        GST_DEBUG_OBJECT(trans, "can't retrieve %" GST_PTR_FORMAT "'s sink pad",
                         peer_element);
        goto exit;
    }
    if (!(upstream_src = gst_pad_get_peer(upstream_sink)))
    {
        GST_DEBUG_OBJECT(trans, "can't retrieve %" GST_PTR_FORMAT "'s upstream src pad",
                         upstream_sink);
        goto exit;
    }
    if (!gst_pad_has_current_caps(upstream_src))
    {
        GST_DEBUG_OBJECT(trans, "upstream_src %" GST_PTR_FORMAT " doesn't have current caps",
                         upstream_src);
        goto exit;
    }
    upstream_caps = gst_pad_get_current_caps(upstream_src);
    GST_DEBUG_OBJECT(trans, "gotcha! %" GST_PTR_FORMAT " has %" GST_PTR_FORMAT,
                     upstream_src, upstream_caps);

exit:
    if (peer_pad)
        gst_object_unref(peer_pad);
    if (peer_element)
        gst_object_unref(peer_element);
    if (upstream_sink)
        gst_object_unref(upstream_sink);
    if (upstream_src)
        gst_object_unref(upstream_src);
    return upstream_caps;
}

static const GstVideoFormatInfo *
get_caps_video_format_info(const GstCaps *caps)
{
    GstStructure *structure;
    const gchar *str;
    GstVideoFormat format;

    if (!gst_caps_is_fixed(caps))
        return NULL;
    structure = gst_caps_get_structure(caps, 0);
    if (!gst_structure_has_name(structure, "video/x-raw"))
        return NULL;
    str = gst_structure_get_string(structure, "format");
    if (!str)
        return NULL;
    format = gst_video_format_from_string(str);
    return gst_video_format_get_info(format);
}

static GstCaps *
gst_wine_yuvfixup_transform_caps(GstBaseTransform *trans,
                                 GstPadDirection direction,
                                 GstCaps *caps,
                                 GstCaps *filter)
{
    GstCaps *upstream_caps = NULL;
    GstCaps *res_caps = NULL;
    const GstVideoFormatInfo *info;
    GstStructure *structure;

    if ((info = get_caps_video_format_info(caps))
        && !GST_VIDEO_FORMAT_INFO_IS_YUV(info))
    {
        /* remove YUV color info if we use other than YUV format */
        structure = gst_caps_get_structure(caps, 0);
        if (gst_structure_has_field(structure, "colorimetry")
            || gst_structure_has_field(structure, "chroma-site"))
        {
            GST_DEBUG_OBJECT(trans, "remove color info from %" GST_PTR_FORMAT, caps);
            res_caps = gst_caps_new_empty();
            structure = gst_structure_copy(structure);
            gst_structure_remove_fields(structure, "colorimetry", "chroma-site", NULL);
            gst_caps_append_structure(res_caps, structure);
        }
    }
    else if (direction == GST_PAD_SINK
             && (upstream_caps = get_upstream_caps(trans))
             && (info = get_caps_video_format_info(upstream_caps))
             && GST_VIDEO_FORMAT_INFO_IS_YUV(info))
    {
        GValue colorimetry = G_VALUE_INIT, chroma_site = G_VALUE_INIT;
        const gchar *str;

        /* get YUV color info from upstream */
        structure = gst_caps_get_structure(upstream_caps, 0);
        if ((str = gst_structure_get_string(structure, "colorimetry")))
        {
            g_value_init(&colorimetry, G_TYPE_STRING);
            g_value_set_string(&colorimetry, str);
        }
        if ((str = gst_structure_get_string(structure, "chroma-site")))
        {
            g_value_init(&chroma_site, G_TYPE_STRING);
            g_value_set_string(&chroma_site, str);
        }

        /* if found, copy them to our caps candiates */
        if (G_VALUE_HOLDS_STRING(&colorimetry)
            || G_VALUE_HOLDS_STRING(&chroma_site))
        {
            guint i, n;

            res_caps = gst_caps_new_empty();
            n = gst_caps_get_size(caps);
            for (i = 0; i < n; i++) {
                structure = gst_caps_get_structure(caps, i);
                structure = gst_structure_copy(structure);
                if (G_VALUE_HOLDS_STRING(&colorimetry)
                    && !gst_structure_has_field(structure, "colorimetry"))
                    gst_structure_set_value(structure, "colorimetry", &colorimetry);
                if (G_VALUE_HOLDS_STRING(&chroma_site)
                    && !gst_structure_has_field(structure, "chroma-site"))
                    gst_structure_set_value(structure, "chroma-site", &chroma_site);
                gst_caps_append_structure(res_caps, structure);
            }
            g_value_unset(&colorimetry);
            g_value_unset(&chroma_site);
        }
    }

    if (upstream_caps)
        gst_caps_unref(upstream_caps);
    if (!res_caps)
        res_caps = gst_caps_copy(caps);

    if (filter) {
        GstCaps *tmp_caps = res_caps;
        res_caps = gst_caps_intersect_full(filter, tmp_caps, GST_CAPS_INTERSECT_FIRST);
        gst_caps_unref(tmp_caps);
    }
    return res_caps;
}

gboolean
gst_wine_yuvfixup_plugin_init(void)
{
    return gst_element_register(NULL, "yuvfixup", GST_RANK_NONE, GST_TYPE_WINE_YUVFIXUP);
}
