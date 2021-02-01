/*
 * Copyright 2015 Andrew Eikum for CodeWeavers
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

#ifndef GST_CBS_H
#define GST_CBS_H

#include "wine/list.h"
#include "windef.h"
#include <pthread.h>

typedef enum {
  GST_AUTOPLUG_SELECT_TRY,
  GST_AUTOPLUG_SELECT_EXPOSE,
  GST_AUTOPLUG_SELECT_SKIP
} GstAutoplugSelectResult;

enum CB_TYPE {
    EXISTING_NEW_PAD,
    QUERY_SINK,
    GSTDEMUX_MAX,
    BYTESTREAM_WRAPPER_PULL,
    BYTESTREAM_QUERY,
    BYTESTREAM_PAD_MODE_ACTIVATE,
    BYTESTREAM_PAD_EVENT_PROCESS,
    MF_SRC_BUS_WATCH,
    MF_SRC_STREAM_ADDED,
    MF_SRC_STREAM_REMOVED,
    MF_SRC_NO_MORE_PADS,
    MEDIA_SOURCE_MAX,
    ACTIVATE_PUSH_MODE,
    QUERY_INPUT_SRC,
    DECODER_NEW_SAMPLE,
    WATCH_DECODER_BUS,
    DECODER_PAD_ADDED,
    MF_DECODE_MAX,
};

struct cb_data {
    enum CB_TYPE type;
    union {
        struct watch_bus_data {
            GstBus *bus;
            GstMessage *msg;
            gpointer user;
            GstBusSyncReply ret;
        } watch_bus_data;
        struct pad_added_data {
            GstElement *element;
            GstPad *pad;
            gpointer user;
        } pad_added_data;
        struct query_function_data {
            GstPad *pad;
            GstObject *parent;
            GstQuery *query;
            gboolean ret;
        } query_function_data;
        struct activate_mode_data {
            GstPad *pad;
            GstObject *parent;
            GstPadMode mode;
            gboolean activate;
            gboolean ret;
        } activate_mode_data;
        struct no_more_pads_data {
            GstElement *element;
            gpointer user;
        } no_more_pads_data;
        struct getrange_data {
            GstPad *pad;
            GstObject *parent;
            guint64 ofs;
            guint len;
            GstBuffer **buf;
            GstFlowReturn ret;
        } getrange_data;
        struct event_src_data {
            GstPad *pad;
            GstObject *parent;
            GstEvent *event;
            gboolean ret;
        } event_src_data;
        struct pad_removed_data {
            GstElement *element;
            GstPad *pad;
            gpointer user;
        } pad_removed_data;
        struct query_sink_data {
            GstPad *pad;
            GstObject *parent;
            GstQuery *query;
            gboolean ret;
        } query_sink_data;
        struct chain_data {
            GstPad *pad;
            GstObject *parent;
            GstBuffer *buffer;
            GstFlowReturn ret;
        } chain_data;
        struct new_sample_data {
            GstElement *appsink;
            gpointer user;
            GstFlowReturn ret;
        } new_sample_data;
    } u;

    int finished;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    struct list entry;
};

void mark_wine_thread(void) DECLSPEC_HIDDEN;
void perform_cb_gstdemux(struct cb_data *data) DECLSPEC_HIDDEN;
void perform_cb_media_source(struct cb_data *data) DECLSPEC_HIDDEN;
void perform_cb_mf_decode(struct cb_data *data) DECLSPEC_HIDDEN;

void existing_new_pad_wrapper(GstElement *bin, GstPad *pad, gpointer user) DECLSPEC_HIDDEN;
GstFlowReturn got_data_wrapper(GstPad *pad, GstObject *parent, GstBuffer *buf) DECLSPEC_HIDDEN;
void Gstreamer_transform_pad_added_wrapper(GstElement *filter, GstPad *pad, gpointer user) DECLSPEC_HIDDEN;
gboolean query_sink_wrapper(GstPad *pad, GstObject *parent, GstQuery *query) DECLSPEC_HIDDEN;
GstFlowReturn bytestream_wrapper_pull_wrapper(GstPad *pad, GstObject *parent, guint64 ofs, guint len, GstBuffer **buf) DECLSPEC_HIDDEN;
gboolean bytestream_query_wrapper(GstPad *pad, GstObject *parent, GstQuery *query) DECLSPEC_HIDDEN;
gboolean bytestream_pad_mode_activate_wrapper(GstPad *pad, GstObject *parent, GstPadMode mode, gboolean activate) DECLSPEC_HIDDEN;
gboolean bytestream_pad_event_process_wrapper(GstPad *pad, GstObject *parent, GstEvent *event) DECLSPEC_HIDDEN;
GstBusSyncReply mf_src_bus_watch_wrapper(GstBus *bus, GstMessage *message, gpointer user) DECLSPEC_HIDDEN;
void mf_src_stream_added_wrapper(GstElement *bin, GstPad *pad, gpointer user) DECLSPEC_HIDDEN;
void mf_src_stream_removed_wrapper(GstElement *element, GstPad *pad, gpointer user) DECLSPEC_HIDDEN;
void mf_src_no_more_pads_wrapper(GstElement *element, gpointer user) DECLSPEC_HIDDEN;
gboolean activate_push_mode_wrapper(GstPad *pad, GstObject *parent, GstPadMode mode, gboolean activate) DECLSPEC_HIDDEN;
gboolean query_input_src_wrapper(GstPad *pad, GstObject *parent, GstQuery *query) DECLSPEC_HIDDEN;
GstBusSyncReply watch_decoder_bus_wrapper(GstBus *bus, GstMessage *message, gpointer user) DECLSPEC_HIDDEN;
GstFlowReturn decoder_new_sample_wrapper(GstElement *appsink, gpointer user) DECLSPEC_HIDDEN;

#endif
