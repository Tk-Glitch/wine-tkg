/*
 * WDM video capture filter unit tests
 *
 * Copyright 2019 Damjan Jovanovic
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
#include "dshow.h"
#include "wine/test.h"
#include "wine/strmbase.h"

static BOOL compare_media_types(const AM_MEDIA_TYPE *a, const AM_MEDIA_TYPE *b)
{
    return !memcmp(a, b, offsetof(AM_MEDIA_TYPE, pbFormat))
            && !memcmp(a->pbFormat, b->pbFormat, a->cbFormat);
}

#define check_interface(a, b, c) check_interface_(__LINE__, a, b, c)
static void check_interface_(unsigned int line, void *iface_ptr, REFIID iid, BOOL supported)
{
    IUnknown *iface = iface_ptr;
    HRESULT hr, expected_hr;
    IUnknown *unk;

    expected_hr = supported ? S_OK : E_NOINTERFACE;

    hr = IUnknown_QueryInterface(iface, iid, (void **)&unk);
    ok_(__FILE__, line)(hr == expected_hr, "Got hr %#x, expected %#x.\n", hr, expected_hr);
    if (SUCCEEDED(hr))
        IUnknown_Release(unk);
}

static void test_media_types(IPin *pin)
{
    IEnumMediaTypes *enum_media_types;
    AM_MEDIA_TYPE mt, *pmt;
    HRESULT hr;

    hr = IPin_EnumMediaTypes(pin, &enum_media_types);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    while (IEnumMediaTypes_Next(enum_media_types, 1, &pmt, NULL) == S_OK)
    {
        hr = IPin_QueryAccept(pin, pmt);
        ok(hr == S_OK, "Got hr %#x.\n", hr);
        CoTaskMemFree(pmt);
    }
    IEnumMediaTypes_Release(enum_media_types);

    hr = IPin_QueryAccept(pin, NULL);
    todo_wine ok(hr == E_POINTER, "Got hr %#x.\n", hr);

    memset(&mt, 0, sizeof(mt));
    hr = IPin_QueryAccept(pin, &mt);
    ok(hr != S_OK, "Got hr %#x.\n", hr);

    mt.majortype = MEDIATYPE_Video;
    hr = IPin_QueryAccept(pin, &mt);
    ok(hr != S_OK, "Got hr %#x.\n", hr);

    mt.formattype = FORMAT_VideoInfo;
    hr = IPin_QueryAccept(pin, &mt);
    ok(hr != S_OK, "Got hr %#x.\n", hr);

    mt.formattype = FORMAT_None;
    hr = IPin_QueryAccept(pin, &mt);
    ok(hr != S_OK, "Got hr %#x.\n", hr);
}

static void test_stream_config(IPin *pin)
{
    VIDEOINFOHEADER *video_info, *video_info2;
    LONG depth, compression, count, size, i;
    IEnumMediaTypes *enum_media_types;
    AM_MEDIA_TYPE *format, *format2;
    IAMStreamConfig *stream_config;
    VIDEO_STREAM_CONFIG_CAPS vscc;
    HRESULT hr;

    hr = IPin_QueryInterface(pin, &IID_IAMStreamConfig, (void **)&stream_config);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IAMStreamConfig_GetFormat(stream_config, &format);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(IsEqualGUID(&format->majortype, &MEDIATYPE_Video), "Got wrong majortype: %s.\n",
            debugstr_guid(&format->majortype));

    hr = IAMStreamConfig_SetFormat(stream_config, format);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    /* After setting the format, a single media type is enumerated.
     * This persists until the filter is released. */
    IPin_EnumMediaTypes(pin, &enum_media_types);
    hr = IEnumMediaTypes_Next(enum_media_types, 1, &format2, NULL);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    DeleteMediaType(format2);
    hr = IEnumMediaTypes_Next(enum_media_types, 1, &format2, NULL);
    ok(hr == S_FALSE, "Got hr %#x.\n", hr);
    IEnumMediaTypes_Release(enum_media_types);

    format->majortype = MEDIATYPE_Audio;
    hr = IAMStreamConfig_SetFormat(stream_config, format);
    ok(hr == E_FAIL, "Got hr %#x.\n", hr);

    format->majortype = MEDIATYPE_Video;
    video_info = (VIDEOINFOHEADER *)format->pbFormat;
    video_info->bmiHeader.biWidth--;
    video_info->bmiHeader.biHeight--;
    hr = IAMStreamConfig_SetFormat(stream_config, format);
    ok(hr == E_FAIL, "Got hr %#x.\n", hr);

    depth = video_info->bmiHeader.biBitCount;
    compression = video_info->bmiHeader.biCompression;
    video_info->bmiHeader.biWidth++;
    video_info->bmiHeader.biHeight++;
    video_info->bmiHeader.biBitCount = 0;
    video_info->bmiHeader.biCompression = 0;
    hr = IAMStreamConfig_SetFormat(stream_config, format);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IAMStreamConfig_GetFormat(stream_config, &format2);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(IsEqualGUID(&format2->majortype, &MEDIATYPE_Video), "Got wrong majortype: %s.\n",
            debugstr_guid(&format2->majortype));
    video_info2 = (VIDEOINFOHEADER *)format2->pbFormat;
    ok(video_info2->bmiHeader.biBitCount == depth, "Got wrong depth: %d.\n",
            video_info2->bmiHeader.biBitCount);
    ok(video_info2->bmiHeader.biCompression == compression,
            "Got wrong compression: %d.\n", video_info2->bmiHeader.biCompression);
    FreeMediaType(format2);

    video_info->bmiHeader.biWidth = 10000000;
    video_info->bmiHeader.biHeight = 10000000;
    hr = IAMStreamConfig_SetFormat(stream_config, format);
    ok(hr == E_FAIL, "Got hr %#x.\n", hr);
    FreeMediaType(format);

    count = 0xdeadbeef;
    size = 0xdeadbeef;
    /* Crash on Windows */
    if (0)
    {
        hr = IAMStreamConfig_GetNumberOfCapabilities(stream_config, &count, NULL);
        ok(hr == E_POINTER, "Got hr %#x.\n", hr);

        hr = IAMStreamConfig_GetNumberOfCapabilities(stream_config, NULL, &size);
        ok(hr == E_POINTER, "Got hr %#x.\n", hr);

        hr = IAMStreamConfig_GetStreamCaps(stream_config, 0, NULL, (BYTE *)&vscc);
        ok(hr == E_POINTER, "Got hr %#x.\n", hr);

        hr = IAMStreamConfig_GetStreamCaps(stream_config, 0, &format, NULL);
        ok(hr == E_POINTER, "Got hr %#x.\n", hr);
    }

    hr = IAMStreamConfig_GetNumberOfCapabilities(stream_config, &count, &size);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(count != 0xdeadbeef, "Got wrong count: %d.\n", count);
    ok(size == sizeof(VIDEO_STREAM_CONFIG_CAPS), "Got wrong size: %d.\n", size);

    hr = IAMStreamConfig_GetStreamCaps(stream_config, 100000, NULL, NULL);
    ok(hr == S_FALSE, "Got hr %#x.\n", hr);

    hr = IAMStreamConfig_GetStreamCaps(stream_config, 100000, &format, (BYTE *)&vscc);
    ok(hr == S_FALSE, "Got hr %#x.\n", hr);

    for (i = 0; i < count; ++i)
    {
        hr = IAMStreamConfig_GetStreamCaps(stream_config, i, &format, (BYTE *)&vscc);
        ok(hr == S_OK, "Got hr %#x.\n", hr);
        ok(IsEqualGUID(&format->majortype, &MEDIATYPE_Video), "Got wrong majortype: %s.\n",
                debugstr_guid(&MEDIATYPE_Video));
        ok(IsEqualGUID(&vscc.guid, &FORMAT_VideoInfo)
                || IsEqualGUID(&vscc.guid, &FORMAT_VideoInfo2), "Got wrong guid: %s.\n",
                debugstr_guid(&vscc.guid));

        hr = IAMStreamConfig_SetFormat(stream_config, format);
        ok(hr == S_OK, "Got hr %#x.\n", hr);

        hr = IAMStreamConfig_GetFormat(stream_config, &format2);
        ok(hr == S_OK, "Got hr %#x.\n", hr);
        ok(compare_media_types(format, format2), "Media types didn't match.\n");
        DeleteMediaType(format2);

        hr = IPin_EnumMediaTypes(pin, &enum_media_types);
        ok(hr == S_OK, "Got hr %#x.\n", hr);
        hr = IEnumMediaTypes_Next(enum_media_types, 1, &format2, NULL);
        ok(hr == S_OK, "Got hr %#x.\n", hr);
        ok(compare_media_types(format, format2), "Media types didn't match.\n");
        DeleteMediaType(format2);
        IEnumMediaTypes_Release(enum_media_types);

        DeleteMediaType(format);
    }

    IAMStreamConfig_Release(stream_config);
}

static void test_pin_interfaces(IPin *pin)
{
    todo_wine check_interface(pin, &IID_IAMBufferNegotiation, TRUE);
    check_interface(pin, &IID_IAMStreamConfig, TRUE);
    todo_wine check_interface(pin, &IID_IAMStreamControl, TRUE);
    todo_wine check_interface(pin, &IID_IKsPin, TRUE);
    check_interface(pin, &IID_IKsPropertySet, TRUE);
    todo_wine check_interface(pin, &IID_IMediaSeeking, TRUE);
    check_interface(pin, &IID_IPin, TRUE);
    todo_wine check_interface(pin, &IID_IQualityControl, TRUE);
    todo_wine check_interface(pin, &IID_ISpecifyPropertyPages, TRUE);

    check_interface(pin, &IID_IAMCrossbar, FALSE);
    check_interface(pin, &IID_IAMDroppedFrames, FALSE);
    check_interface(pin, &IID_IAMFilterMiscFlags, FALSE);
    check_interface(pin, &IID_IAMPushSource, FALSE);
    check_interface(pin, &IID_IAMTVTuner, FALSE);
    check_interface(pin, &IID_IAMVideoCompression, FALSE);
    check_interface(pin, &IID_IAMVideoProcAmp, FALSE);
    check_interface(pin, &IID_IPersistPropertyBag, FALSE);
    check_interface(pin, &IID_IStreamBuilder, FALSE);
}

static void test_pins(IBaseFilter *filter)
{
    IEnumPins *enum_pins;
    IPin *pin;
    HRESULT hr;

    hr = IBaseFilter_EnumPins(filter, &enum_pins);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    while ((hr = IEnumPins_Next(enum_pins, 1, &pin, NULL)) == S_OK)
    {
        PIN_DIRECTION pin_direction;
        IPin_QueryDirection(pin, &pin_direction);
        if (pin_direction == PINDIR_OUTPUT)
        {
            test_pin_interfaces(pin);
            test_media_types(pin);
            test_stream_config(pin);
        }
        IPin_Release(pin);
    }

    IEnumPins_Release(enum_pins);
}

static void test_filter_interfaces(IBaseFilter *filter)
{
    check_interface(filter, &IID_IAMFilterMiscFlags, TRUE);
    check_interface(filter, &IID_IAMVideoControl, TRUE);
    check_interface(filter, &IID_IAMVideoProcAmp, TRUE);
    check_interface(filter, &IID_IBaseFilter, TRUE);
    todo_wine check_interface(filter, &IID_IKsPropertySet, TRUE);
    todo_wine check_interface(filter, &IID_IMediaSeeking, TRUE);
    check_interface(filter, &IID_IPersistPropertyBag, TRUE);
    todo_wine check_interface(filter, &IID_ISpecifyPropertyPages, TRUE);

    check_interface(filter, &IID_IAMCrossbar, FALSE);
    check_interface(filter, &IID_IAMPushSource, FALSE);
    check_interface(filter, &IID_IAMStreamConfig, FALSE);
    check_interface(filter, &IID_IAMTVTuner, FALSE);
    check_interface(filter, &IID_IAMVideoCompression, FALSE);
    check_interface(filter, &IID_IAMVfwCaptureDialogs, FALSE);
    check_interface(filter, &IID_IPin, FALSE);
    check_interface(filter, &IID_IReferenceClock, FALSE);
    check_interface(filter, &IID_IOverlayNotify, FALSE);
}

static void test_misc_flags(IBaseFilter *filter)
{
    IAMFilterMiscFlags *misc_flags;
    ULONG flags;
    HRESULT hr;

    hr = IBaseFilter_QueryInterface(filter, &IID_IAMFilterMiscFlags, (void **)&misc_flags);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    flags = IAMFilterMiscFlags_GetMiscFlags(misc_flags);
    ok(flags == AM_FILTER_MISC_FLAGS_IS_SOURCE
            || broken(!flags) /* win7 */, "Got wrong flags: %#x.\n", flags);

    IAMFilterMiscFlags_Release(misc_flags);
}

struct testfilter
{
    struct strmbase_filter filter;
    struct strmbase_sink sink;
};

static inline struct testfilter *impl_from_strmbase_filter(struct strmbase_filter *iface)
{
    return CONTAINING_RECORD(iface, struct testfilter, filter);
}

static struct strmbase_pin *testfilter_get_pin(struct strmbase_filter *iface, unsigned int index)
{
    struct testfilter *filter = impl_from_strmbase_filter(iface);
    if (!index)
        return &filter->sink.pin;
    return NULL;
}

static void testfilter_destroy(struct strmbase_filter *iface)
{
    struct testfilter *filter = impl_from_strmbase_filter(iface);
    strmbase_sink_cleanup(&filter->sink);
    strmbase_filter_cleanup(&filter->filter);
}

static const struct strmbase_filter_ops testfilter_ops =
{
    .filter_get_pin = testfilter_get_pin,
    .filter_destroy = testfilter_destroy,
};

static HRESULT testsink_query_interface(struct strmbase_pin *iface, REFIID iid, void **out)
{
    struct testfilter *filter = impl_from_strmbase_filter(iface->filter);

    if (IsEqualGUID(iid, &IID_IMemInputPin))
        *out = &filter->sink.IMemInputPin_iface;
    else
        return E_NOINTERFACE;

    IUnknown_AddRef((IUnknown *)*out);
    return S_OK;
}

static HRESULT WINAPI testsink_Receive(struct strmbase_sink *iface, IMediaSample *sample)
{
    return S_OK;
}

static const struct strmbase_sink_ops testsink_ops =
{
    .base.pin_query_interface = testsink_query_interface,
    .pfnReceive = testsink_Receive,
};

static void testfilter_init(struct testfilter *filter)
{
    static const GUID clsid = {0xabacab};
    memset(filter, 0, sizeof(*filter));
    strmbase_filter_init(&filter->filter, NULL, &clsid, &testfilter_ops);
    strmbase_sink_init(&filter->sink, &filter->filter, L"sink", &testsink_ops, NULL);
}

static void test_connect_pin(IBaseFilter *filter, IPin *source)
{
    AM_MEDIA_TYPE req_mt, default_mt, mt, *mts[2];
    IAMStreamConfig *stream_config;
    struct testfilter testsink;
    IEnumMediaTypes *enummt;
    IFilterGraph2 *graph;
    ULONG count, ref;
    HRESULT hr;
    IPin *peer;

    CoCreateInstance(&CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER,
            &IID_IFilterGraph2, (void **)&graph);
    testfilter_init(&testsink);
    IFilterGraph2_AddFilter(graph, &testsink.filter.IBaseFilter_iface, L"sink");
    IFilterGraph2_AddFilter(graph, filter, L"source");
    hr = IPin_QueryInterface(source, &IID_IAMStreamConfig, (void **)&stream_config);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    peer = (IPin *)0xdeadbeef;
    hr = IPin_ConnectedTo(source, &peer);
    ok(hr == VFW_E_NOT_CONNECTED, "Got hr %#x.\n", hr);
    ok(!peer, "Got peer %p.\n", peer);

    hr = IPin_ConnectionMediaType(source, &mt);
    ok(hr == VFW_E_NOT_CONNECTED, "Got hr %#x.\n", hr);

    hr = IPin_EnumMediaTypes(source, &enummt);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    hr = IEnumMediaTypes_Next(enummt, 2, mts, &count);
    ok(SUCCEEDED(hr), "Got hr %#x.\n", hr);
    CopyMediaType(&req_mt, mts[count - 1]);
    CopyMediaType(&default_mt, mts[0]);
    DeleteMediaType(mts[0]);
    if (count > 1)
        DeleteMediaType(mts[1]);
    IEnumMediaTypes_Release(enummt);

    hr = IAMStreamConfig_GetFormat(stream_config, &mts[0]);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(compare_media_types(mts[0], &default_mt), "Media types didn't match.\n");
    DeleteMediaType(mts[0]);

    hr = IFilterGraph2_ConnectDirect(graph, source, &testsink.sink.pin.IPin_iface, &req_mt);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IPin_ConnectedTo(source, &peer);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(peer == &testsink.sink.pin.IPin_iface, "Got peer %p.\n", peer);
    IPin_Release(peer);

    hr = IPin_ConnectionMediaType(source, &mt);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(compare_media_types(&mt, &req_mt), "Media types didn't match.\n");
    ok(compare_media_types(&testsink.sink.pin.mt, &req_mt), "Media types didn't match.\n");
    FreeMediaType(&mt);

    hr = IAMStreamConfig_GetFormat(stream_config, &mts[0]);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(compare_media_types(mts[0], &req_mt), "Media types didn't match.\n");
    DeleteMediaType(mts[0]);

    hr = IPin_EnumMediaTypes(source, &enummt);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    hr = IEnumMediaTypes_Next(enummt, 1, mts, NULL);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(compare_media_types(mts[0], &default_mt), "Media types didn't match.\n");
    DeleteMediaType(mts[0]);
    IEnumMediaTypes_Release(enummt);

    hr = IFilterGraph2_Disconnect(graph, source);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    hr = IFilterGraph2_Disconnect(graph, source);
    ok(hr == S_FALSE, "Got hr %#x.\n", hr);
    ok(testsink.sink.pin.peer == source, "Got peer %p.\n", testsink.sink.pin.peer);
    IFilterGraph2_Disconnect(graph, &testsink.sink.pin.IPin_iface);

    hr = IAMStreamConfig_GetFormat(stream_config, &mts[0]);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(compare_media_types(mts[0], &default_mt), "Media types didn't match.\n");
    DeleteMediaType(mts[0]);

    FreeMediaType(&req_mt);
    FreeMediaType(&default_mt);
    IAMStreamConfig_Release(stream_config);
    ref = IFilterGraph2_Release(graph);
    ok(!ref, "Got outstanding refcount %d.\n", ref);
    ref = IBaseFilter_Release(&testsink.filter.IBaseFilter_iface);
    ok(!ref, "Got outstanding refcount %d.\n", ref);
}

static void test_connection(IMoniker *moniker)
{
    IEnumPins *enum_pins;
    IBaseFilter *filter;
    HRESULT hr;
    ULONG ref;
    IPin *pin;

    hr = IMoniker_BindToObject(moniker, NULL, NULL, &IID_IBaseFilter, (void **)&filter);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IBaseFilter_EnumPins(filter, &enum_pins);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    while (IEnumPins_Next(enum_pins, 1, &pin, NULL) == S_OK)
    {
        PIN_DIRECTION dir;
        IPin_QueryDirection(pin, &dir);
        if (dir == PINDIR_OUTPUT)
        {
            test_connect_pin(filter, pin);
        }
        IPin_Release(pin);
    }

    IEnumPins_Release(enum_pins);
    ref = IBaseFilter_Release(filter);
    ok(!ref, "Got outstanding refcount %d.\n", ref);
}

START_TEST(videocapture)
{
    ICreateDevEnum *dev_enum;
    IEnumMoniker *class_enum;
    IBaseFilter *filter;
    IMoniker *moniker;
    WCHAR *name;
    HRESULT hr;
    ULONG ref;

    CoInitialize(NULL);

    hr = CoCreateInstance(&CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC,
            &IID_ICreateDevEnum, (void **)&dev_enum);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = ICreateDevEnum_CreateClassEnumerator(dev_enum, &CLSID_VideoInputDeviceCategory, &class_enum, 0);
    if (hr == S_FALSE)
    {
        skip("No video capture devices present.\n");
        ICreateDevEnum_Release(dev_enum);
        CoUninitialize();
        return;
    }
    ok(hr == S_OK, "Got hr=%#x.\n", hr);

    while (IEnumMoniker_Next(class_enum, 1, &moniker, NULL) == S_OK)
    {
        hr = IMoniker_GetDisplayName(moniker, NULL, NULL, &name);
        ok(hr == S_OK, "Got hr %#x.\n", hr);
        trace("Testing device %s.\n", wine_dbgstr_w(name));
        CoTaskMemFree(name);

        if (FAILED(hr = IMoniker_BindToObject(moniker, NULL, NULL, &IID_IBaseFilter, (void **)&filter)))
        {
            skip("Failed to open device %s, hr %#x.\n", debugstr_w(name), hr);
            IMoniker_Release(moniker);
            continue;
        }

        test_filter_interfaces(filter);
        test_pins(filter);
        test_misc_flags(filter);

        ref = IBaseFilter_Release(filter);
        ok(!ref, "Got outstanding refcount %d.\n", ref);

        test_connection(moniker);

        IMoniker_Release(moniker);
    }

    ICreateDevEnum_Release(dev_enum);
    IEnumMoniker_Release(class_enum);
    CoUninitialize();
}
