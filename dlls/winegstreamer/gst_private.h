/*
 * GStreamer splitter + decoder, adapted from parser.c
 *
 * Copyright 2010 Maarten Lankhorst for CodeWeavers
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

#ifndef __GST_PRIVATE_INCLUDED__
#define __GST_PRIVATE_INCLUDED__

#include <stdarg.h>
#include <stdio.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/audio/audio.h>

#define COBJMACROS
#define NONAMELESSSTRUCT
#define NONAMELESSUNION
#include "windef.h"
#include "winbase.h"
#include "wtypes.h"
#include "winuser.h"
#include "dshow.h"
#include "strmif.h"
#include "mfobjects.h"
#include "wine/heap.h"
#include "wine/strmbase.h"

#define MEDIATIME_FROM_BYTES(x) ((LONGLONG)(x) * 10000000)

extern LONG object_locks;

HRESULT avi_splitter_create(IUnknown *outer, IUnknown **out) DECLSPEC_HIDDEN;
HRESULT gstdemux_create(IUnknown *outer, IUnknown **out) DECLSPEC_HIDDEN;
HRESULT mpeg_splitter_create(IUnknown *outer, IUnknown **out) DECLSPEC_HIDDEN;
HRESULT wave_parser_create(IUnknown *outer, IUnknown **out) DECLSPEC_HIDDEN;

BOOL init_gstreamer(void) DECLSPEC_HIDDEN;

void start_dispatch_thread(void) DECLSPEC_HIDDEN;

extern HRESULT mfplat_DllRegisterServer(void) DECLSPEC_HIDDEN;
extern HRESULT mfplat_get_class_object(REFCLSID rclsid, REFIID riid, void **obj) DECLSPEC_HIDDEN;

GstCaps *make_mf_compatible_caps(GstCaps *caps);
IMFMediaType *mf_media_type_from_caps(GstCaps *caps);
GstCaps *caps_from_mf_media_type(IMFMediaType *type);
IMFSample *mf_sample_from_gst_buffer(GstBuffer *in);
GstBuffer *gst_buffer_from_mf_sample(IMFSample *in);

enum decoder_type
{
    DECODER_TYPE_H264,
    DECODER_TYPE_AAC,
    DECODER_TYPE_WMV,
};
HRESULT generic_decoder_construct(REFIID riid, void **obj, enum decoder_type);
enum source_type
{
    SOURCE_TYPE_MPEG_4,
    SOURCE_TYPE_ASF,
};
HRESULT container_stream_handler_construct(REFIID riid, void **obj, enum source_type);

HRESULT color_converter_create(REFIID riid, void **ret);

#endif /* __GST_PRIVATE_INCLUDED__ */
