/*
 * Enhanced Video Renderer filter unit tests
 *
 * Copyright 2018 Zebediah Figura
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
#include "d3d9.h"
#include "evr.h"
#include "mferror.h"
#include "mfapi.h"
#include "initguid.h"
#include "evr9.h"

static const WCHAR sink_id[] = {'E','V','R',' ','I','n','p','u','t','0',0};

static HRESULT (WINAPI *pMFCreateVideoMediaTypeFromSubtype)(const GUID *subtype, IMFVideoMediaType **video_type);

static HWND create_window(void)
{
    RECT r = {0, 0, 640, 480};

    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW | WS_VISIBLE, FALSE);

    return CreateWindowA("static", "d3d9_test", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            0, 0, r.right - r.left, r.bottom - r.top, NULL, NULL, NULL, NULL);
}

static IDirect3DDevice9 *create_device(IDirect3D9 *d3d9, HWND focus_window)
{
    D3DPRESENT_PARAMETERS present_parameters = {0};
    IDirect3DDevice9 *device = NULL;

    present_parameters.BackBufferWidth = 640;
    present_parameters.BackBufferHeight = 480;
    present_parameters.BackBufferFormat = D3DFMT_A8R8G8B8;
    present_parameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
    present_parameters.hDeviceWindow = focus_window;
    present_parameters.Windowed = TRUE;
    present_parameters.EnableAutoDepthStencil = TRUE;
    present_parameters.AutoDepthStencilFormat = D3DFMT_D24S8;

    IDirect3D9_CreateDevice(d3d9, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, focus_window,
            D3DCREATE_HARDWARE_VERTEXPROCESSING, &present_parameters, &device);

    return device;
}

static IBaseFilter *create_evr(void)
{
    IBaseFilter *filter = NULL;
    HRESULT hr = CoCreateInstance(&CLSID_EnhancedVideoRenderer, NULL, CLSCTX_INPROC_SERVER,
            &IID_IBaseFilter, (void **)&filter);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    return filter;
}

static ULONG get_refcount(void *iface)
{
    IUnknown *unknown = iface;
    IUnknown_AddRef(unknown);
    return IUnknown_Release(unknown);
}

static const GUID test_iid = {0x33333333};
static LONG outer_ref = 1;

static HRESULT WINAPI outer_QueryInterface(IUnknown *iface, REFIID iid, void **out)
{
    if (IsEqualGUID(iid, &IID_IUnknown)
            || IsEqualGUID(iid, &IID_IBaseFilter)
            || IsEqualGUID(iid, &test_iid))
    {
        *out = (IUnknown *)0xdeadbeef;
        return S_OK;
    }
    ok(0, "unexpected call %s\n", wine_dbgstr_guid(iid));
    return E_NOINTERFACE;
}

static ULONG WINAPI outer_AddRef(IUnknown *iface)
{
    return InterlockedIncrement(&outer_ref);
}

static ULONG WINAPI outer_Release(IUnknown *iface)
{
    return InterlockedDecrement(&outer_ref);
}

static const IUnknownVtbl outer_vtbl =
{
    outer_QueryInterface,
    outer_AddRef,
    outer_Release,
};

static IUnknown test_outer = {&outer_vtbl};

static void test_aggregation(void)
{
    IBaseFilter *filter, *filter2;
    IMFVideoPresenter *presenter;
    IUnknown *unk, *unk2;
    IMFTransform *mixer;
    HRESULT hr;
    ULONG ref;

    filter = (IBaseFilter *)0xdeadbeef;
    hr = CoCreateInstance(&CLSID_EnhancedVideoRenderer, &test_outer, CLSCTX_INPROC_SERVER,
            &IID_IBaseFilter, (void **)&filter);
    ok(hr == E_NOINTERFACE, "Got hr %#x.\n", hr);
    ok(!filter, "Got interface %p.\n", filter);

    hr = CoCreateInstance(&CLSID_EnhancedVideoRenderer, &test_outer, CLSCTX_INPROC_SERVER,
            &IID_IUnknown, (void **)&unk);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(outer_ref == 1, "Got unexpected refcount %d.\n", outer_ref);
    ok(unk != &test_outer, "Returned IUnknown should not be outer IUnknown.\n");
    ref = get_refcount(unk);
    ok(ref == 1, "Got unexpected refcount %d.\n", ref);

    ref = IUnknown_AddRef(unk);
    ok(ref == 2, "Got unexpected refcount %d.\n", ref);
    ok(outer_ref == 1, "Got unexpected refcount %d.\n", outer_ref);

    ref = IUnknown_Release(unk);
    ok(ref == 1, "Got unexpected refcount %d.\n", ref);
    ok(outer_ref == 1, "Got unexpected refcount %d.\n", outer_ref);

    hr = IUnknown_QueryInterface(unk, &IID_IUnknown, (void **)&unk2);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(unk2 == unk, "Got unexpected IUnknown %p.\n", unk2);
    IUnknown_Release(unk2);

    hr = IUnknown_QueryInterface(unk, &IID_IBaseFilter, (void **)&filter);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IBaseFilter_QueryInterface(filter, &IID_IUnknown, (void **)&unk2);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(unk2 == (IUnknown *)0xdeadbeef, "Got unexpected IUnknown %p.\n", unk2);

    hr = IBaseFilter_QueryInterface(filter, &IID_IBaseFilter, (void **)&filter2);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(filter2 == (IBaseFilter *)0xdeadbeef, "Got unexpected IBaseFilter %p.\n", filter2);

    hr = IUnknown_QueryInterface(unk, &test_iid, (void **)&unk2);
    ok(hr == E_NOINTERFACE, "Got hr %#x.\n", hr);
    ok(!unk2, "Got unexpected IUnknown %p.\n", unk2);

    hr = IBaseFilter_QueryInterface(filter, &test_iid, (void **)&unk2);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(unk2 == (IUnknown *)0xdeadbeef, "Got unexpected IUnknown %p.\n", unk2);

    IBaseFilter_Release(filter);
    ref = IUnknown_Release(unk);
    ok(!ref, "Got unexpected refcount %d.\n", ref);
    ok(outer_ref == 1, "Got unexpected refcount %d.\n", outer_ref);

    /* Default presenter. */
    presenter = (void *)0xdeadbeef;
    hr = CoCreateInstance(&CLSID_MFVideoPresenter9, &test_outer, CLSCTX_INPROC_SERVER, &IID_IMFVideoPresenter,
            (void **)&presenter);
    ok(hr == E_NOINTERFACE, "Unexpected hr %#x.\n", hr);
    ok(!presenter, "Got interface %p.\n", presenter);

    hr = CoCreateInstance(&CLSID_MFVideoPresenter9, &test_outer, CLSCTX_INPROC_SERVER, &IID_IUnknown, (void **)&unk);
    ok(hr == S_OK || broken(hr == E_FAIL) /* WinXP */, "Unexpected hr %#x.\n", hr);
    if (SUCCEEDED(hr))
    {
        ok(outer_ref == 1, "Got unexpected refcount %d.\n", outer_ref);
        ok(unk != &test_outer, "Returned IUnknown should not be outer IUnknown.\n");
        ref = get_refcount(unk);
        ok(ref == 1, "Got unexpected refcount %d.\n", ref);

        IUnknown_Release(unk);
    }

    /* Default mixer. */
    presenter = (void *)0xdeadbeef;
    hr = CoCreateInstance(&CLSID_MFVideoMixer9, &test_outer, CLSCTX_INPROC_SERVER, &IID_IMFTransform,
            (void **)&mixer);
    ok(hr == E_NOINTERFACE, "Unexpected hr %#x.\n", hr);
    ok(!mixer, "Got interface %p.\n", mixer);

    hr = CoCreateInstance(&CLSID_MFVideoMixer9, &test_outer, CLSCTX_INPROC_SERVER, &IID_IUnknown, (void **)&unk);
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);
    ok(outer_ref == 1, "Got unexpected refcount %d.\n", outer_ref);
    ok(unk != &test_outer, "Returned IUnknown should not be outer IUnknown.\n");
    ref = get_refcount(unk);
    ok(ref == 1, "Got unexpected refcount %d.\n", ref);

    IUnknown_Release(unk);
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

static void test_interfaces(void)
{
    IBaseFilter *filter = create_evr();
    ULONG ref;

    todo_wine check_interface(filter, &IID_IAMFilterMiscFlags, TRUE);
    check_interface(filter, &IID_IBaseFilter, TRUE);
    check_interface(filter, &IID_IMediaFilter, TRUE);
    check_interface(filter, &IID_IMediaPosition, TRUE);
    check_interface(filter, &IID_IMediaSeeking, TRUE);
    check_interface(filter, &IID_IPersist, TRUE);
    check_interface(filter, &IID_IUnknown, TRUE);

    check_interface(filter, &IID_IBasicAudio, FALSE);
    check_interface(filter, &IID_IBasicVideo, FALSE);
    check_interface(filter, &IID_IDirectXVideoMemoryConfiguration, FALSE);
    check_interface(filter, &IID_IMemInputPin, FALSE);
    check_interface(filter, &IID_IPersistPropertyBag, FALSE);
    check_interface(filter, &IID_IPin, FALSE);
    check_interface(filter, &IID_IReferenceClock, FALSE);
    check_interface(filter, &IID_IVideoWindow, FALSE);

    ref = IBaseFilter_Release(filter);
    ok(!ref, "Got unexpected refcount %d.\n", ref);
}

static void test_enum_pins(void)
{
    IBaseFilter *filter = create_evr();
    IEnumPins *enum1, *enum2;
    ULONG count, ref;
    IPin *pins[2];
    HRESULT hr;

    ref = get_refcount(filter);
    ok(ref == 1, "Got unexpected refcount %d.\n", ref);

    hr = IBaseFilter_EnumPins(filter, NULL);
    ok(hr == E_POINTER, "Got hr %#x.\n", hr);

    hr = IBaseFilter_EnumPins(filter, &enum1);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ref = get_refcount(filter);
    ok(ref == 2, "Got unexpected refcount %d.\n", ref);
    ref = get_refcount(enum1);
    ok(ref == 1, "Got unexpected refcount %d.\n", ref);

    hr = IEnumPins_Next(enum1, 1, NULL, NULL);
    ok(hr == E_POINTER, "Got hr %#x.\n", hr);

    hr = IEnumPins_Next(enum1, 1, pins, NULL);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ref = get_refcount(filter);
    ok(ref == 3, "Got unexpected refcount %d.\n", ref);
    ref = get_refcount(pins[0]);
    ok(ref == 3, "Got unexpected refcount %d.\n", ref);
    ref = get_refcount(enum1);
    ok(ref == 1, "Got unexpected refcount %d.\n", ref);
    IPin_Release(pins[0]);
    ref = get_refcount(filter);
    ok(ref == 2, "Got unexpected refcount %d.\n", ref);

    hr = IEnumPins_Next(enum1, 1, pins, NULL);
    ok(hr == S_FALSE, "Got hr %#x.\n", hr);

    hr = IEnumPins_Reset(enum1);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IEnumPins_Next(enum1, 1, pins, &count);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(count == 1, "Got count %u.\n", count);
    IPin_Release(pins[0]);

    hr = IEnumPins_Next(enum1, 1, pins, &count);
    ok(hr == S_FALSE, "Got hr %#x.\n", hr);
    ok(!count, "Got count %u.\n", count);

    hr = IEnumPins_Reset(enum1);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IEnumPins_Next(enum1, 2, pins, NULL);
    ok(hr == E_INVALIDARG, "Got hr %#x.\n", hr);

    hr = IEnumPins_Next(enum1, 2, pins, &count);
    ok(hr == S_FALSE, "Got hr %#x.\n", hr);
    ok(count == 1, "Got count %u.\n", count);
    IPin_Release(pins[0]);

    hr = IEnumPins_Reset(enum1);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IEnumPins_Clone(enum1, &enum2);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IEnumPins_Skip(enum1, 2);
    ok(hr == S_FALSE, "Got hr %#x.\n", hr);

    hr = IEnumPins_Skip(enum1, 1);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IEnumPins_Skip(enum1, 1);
    ok(hr == S_FALSE, "Got hr %#x.\n", hr);

    hr = IEnumPins_Next(enum1, 1, pins, NULL);
    ok(hr == S_FALSE, "Got hr %#x.\n", hr);

    hr = IEnumPins_Next(enum2, 1, pins, NULL);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    IPin_Release(pins[0]);

    IEnumPins_Release(enum2);
    IEnumPins_Release(enum1);
    ref = IBaseFilter_Release(filter);
    ok(!ref, "Got outstanding refcount %d.\n", ref);
}

static void test_find_pin(void)
{
    IBaseFilter *filter = create_evr();
    IEnumPins *enum_pins;
    IPin *pin, *pin2;
    HRESULT hr;
    ULONG ref;

    hr = IBaseFilter_EnumPins(filter, &enum_pins);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IBaseFilter_FindPin(filter, sink_id, &pin);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    hr = IEnumPins_Next(enum_pins, 1, &pin2, NULL);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(pin2 == pin, "Expected pin %p, got %p.\n", pin, pin2);
    IPin_Release(pin2);
    IPin_Release(pin);

    IEnumPins_Release(enum_pins);
    ref = IBaseFilter_Release(filter);
    ok(!ref, "Got outstanding refcount %d.\n", ref);
}

static void test_pin_info(void)
{
    IBaseFilter *filter = create_evr();
    PIN_DIRECTION dir;
    PIN_INFO info;
    HRESULT hr;
    WCHAR *id;
    ULONG ref;
    IPin *pin;

    hr = IBaseFilter_FindPin(filter, sink_id, &pin);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ref = get_refcount(filter);
    ok(ref == 2, "Got unexpected refcount %d.\n", ref);
    ref = get_refcount(pin);
    ok(ref == 2, "Got unexpected refcount %d.\n", ref);

    hr = IPin_QueryPinInfo(pin, &info);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(info.pFilter == filter, "Expected filter %p, got %p.\n", filter, info.pFilter);
    ok(info.dir == PINDIR_INPUT, "Got direction %d.\n", info.dir);
    ok(!lstrcmpW(info.achName, sink_id), "Got name %s.\n", wine_dbgstr_w(info.achName));
    ref = get_refcount(filter);
    ok(ref == 3, "Got unexpected refcount %d.\n", ref);
    ref = get_refcount(pin);
    ok(ref == 3, "Got unexpected refcount %d.\n", ref);
    IBaseFilter_Release(info.pFilter);

    hr = IPin_QueryDirection(pin, &dir);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(dir == PINDIR_INPUT, "Got direction %d.\n", dir);

    hr = IPin_QueryId(pin, &id);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(!lstrcmpW(id, sink_id), "Got id %s.\n", wine_dbgstr_w(id));
    CoTaskMemFree(id);

    hr = IPin_QueryInternalConnections(pin, NULL, NULL);
    ok(hr == E_NOTIMPL, "Got hr %#x.\n", hr);

    IPin_Release(pin);
    ref = IBaseFilter_Release(filter);
    ok(!ref, "Got outstanding refcount %d.\n", ref);
}

static void test_default_mixer(void)
{
    DWORD input_min, input_max, output_min, output_max;
    IMFAttributes *attributes, *attributes2;
    MFT_OUTPUT_STREAM_INFO output_info;
    MFT_INPUT_STREAM_INFO input_info;
    DWORD input_count, output_count;
    IMFVideoProcessor *processor;
    IMFVideoDeviceID *deviceid;
    DWORD input_id, output_id;
    IMFTransform *transform;
    DXVA2_ValueRange range;
    DXVA2_Fixed32 value;
    IMFGetService *gs;
    COLORREF color;
    unsigned int i;
    DWORD ids[16];
    IUnknown *unk;
    DWORD count;
    GUID *guids;
    HRESULT hr;
    IID iid;

    hr = MFCreateVideoMixer(NULL, &IID_IDirect3DDevice9, &IID_IMFTransform, (void **)&transform);
    ok(hr == S_OK, "Failed to create default mixer, hr %#x.\n", hr);

    hr = IMFTransform_QueryInterface(transform, &IID_IMFTopologyServiceLookupClient, (void **)&unk);
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);
    IUnknown_Release(unk);

    hr = IMFTransform_QueryInterface(transform, &IID_IMFGetService, (void **)&gs);
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);

    hr = IMFGetService_GetService(gs, &MR_VIDEO_MIXER_SERVICE, &IID_IMFVideoMixerBitmap, (void **)&unk);
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);
    IUnknown_Release(unk);

    hr = IMFGetService_GetService(gs, &MR_VIDEO_MIXER_SERVICE, &IID_IMFVideoProcessor, (void **)&processor);
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);

    hr = IMFVideoProcessor_GetBackgroundColor(processor, NULL);
    ok(hr == E_POINTER, "Unexpected hr %#x.\n", hr);

    color = 1;
    hr = IMFVideoProcessor_GetBackgroundColor(processor, &color);
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);
    ok(!color, "Unexpected color %#x.\n", color);

    hr = IMFVideoProcessor_SetBackgroundColor(processor, 0x00121212);
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);

    hr = IMFVideoProcessor_GetBackgroundColor(processor, &color);
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);
    ok(color == 0x121212, "Unexpected color %#x.\n", color);

    hr = IMFVideoProcessor_GetFilteringRange(processor, DXVA2_DetailFilterChromaLevel, &range);
todo_wine
    ok(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "Unexpected hr %#x.\n", hr);

    hr = IMFVideoProcessor_GetFilteringValue(processor, DXVA2_DetailFilterChromaLevel, &value);
todo_wine
    ok(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "Unexpected hr %#x.\n", hr);

    hr = IMFVideoProcessor_GetAvailableVideoProcessorModes(processor, &count, &guids);
todo_wine
    ok(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "Unexpected hr %#x.\n", hr);

    IMFVideoProcessor_Release(processor);

    hr = IMFGetService_GetService(gs, &MR_VIDEO_MIXER_SERVICE, &IID_IMFVideoPositionMapper, (void **)&unk);
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);
    IUnknown_Release(unk);

    IMFGetService_Release(gs);

    hr = IMFTransform_QueryInterface(transform, &IID_IMFVideoMixerBitmap, (void **)&unk);
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);
    IUnknown_Release(unk);

    hr = IMFTransform_QueryInterface(transform, &IID_IMFVideoPositionMapper, (void **)&unk);
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);
    IUnknown_Release(unk);

    hr = IMFTransform_QueryInterface(transform, &IID_IMFVideoProcessor, (void **)&unk);
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);
    IUnknown_Release(unk);

    hr = IMFTransform_QueryInterface(transform, &IID_IMFVideoMixerControl, (void **)&unk);
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);
    IUnknown_Release(unk);

    hr = IMFTransform_QueryInterface(transform, &IID_IMFVideoDeviceID, (void **)&deviceid);
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);

    hr = IMFVideoDeviceID_GetDeviceID(deviceid, NULL);
    ok(hr == E_POINTER, "Unexpected hr %#x.\n", hr);

    hr = IMFVideoDeviceID_GetDeviceID(deviceid, &iid);
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);
    ok(IsEqualIID(&iid, &IID_IDirect3DDevice9), "Unexpected id %s.\n", wine_dbgstr_guid(&iid));

    IMFVideoDeviceID_Release(deviceid);

    /* Stream configuration. */
    input_count = output_count = 0;
    hr = IMFTransform_GetStreamCount(transform, &input_count, &output_count);
    ok(hr == S_OK, "Failed to get stream count, hr %#x.\n", hr);
    ok(input_count == 1 && output_count == 1, "Unexpected stream count %u/%u.\n", input_count, output_count);

    hr = IMFTransform_GetStreamLimits(transform, &input_min, &input_max, &output_min, &output_max);
    ok(hr == S_OK, "Failed to get stream limits, hr %#x.\n", hr);
    ok(input_min == 1 && input_max == 16 && output_min == 1 && output_max == 1, "Unexpected stream limits %u/%u, %u/%u.\n",
            input_min, input_max, output_min, output_max);

    hr = IMFTransform_GetInputStreamInfo(transform, 1, &input_info);
    ok(hr == MF_E_INVALIDSTREAMNUMBER, "Unexpected hr %#x.\n", hr);

    hr = IMFTransform_GetOutputStreamInfo(transform, 1, &output_info);
    ok(hr == MF_E_INVALIDSTREAMNUMBER, "Unexpected hr %#x.\n", hr);

    memset(&input_info, 0xcc, sizeof(input_info));
    hr = IMFTransform_GetInputStreamInfo(transform, 0, &input_info);
    ok(hr == S_OK, "Failed to get input info, hr %#x.\n", hr);

    memset(&output_info, 0xcc, sizeof(output_info));
    hr = IMFTransform_GetOutputStreamInfo(transform, 0, &output_info);
    ok(hr == S_OK, "Failed to get input info, hr %#x.\n", hr);
    ok(!(output_info.dwFlags & (MFT_OUTPUT_STREAM_PROVIDES_SAMPLES | MFT_OUTPUT_STREAM_CAN_PROVIDE_SAMPLES)),
            "Unexpected output flags %#x.\n", output_info.dwFlags);

    hr = IMFTransform_GetStreamIDs(transform, 1, &input_id, 1, &output_id);
    ok(hr == S_OK, "Failed to get input info, hr %#x.\n", hr);
    ok(input_id == 0 && output_id == 0, "Unexpected stream ids.\n");

    hr = IMFTransform_GetInputStreamAttributes(transform, 1, &attributes);
    ok(hr == MF_E_INVALIDSTREAMNUMBER, "Unexpected hr %#x.\n", hr);

    hr = IMFTransform_GetOutputStreamAttributes(transform, 1, &attributes);
    ok(hr == E_NOTIMPL, "Unexpected hr %#x.\n", hr);

    hr = IMFTransform_GetOutputStreamAttributes(transform, 0, &attributes);
    ok(hr == E_NOTIMPL, "Unexpected hr %#x.\n", hr);

    hr = IMFTransform_AddInputStreams(transform, 16, NULL);
    ok(hr == E_POINTER, "Unexpected hr %#x.\n", hr);

    hr = IMFTransform_AddInputStreams(transform, 16, ids);
    ok(hr == E_INVALIDARG, "Unexpected hr %#x.\n", hr);

    memset(ids, 0, sizeof(ids));
    hr = IMFTransform_AddInputStreams(transform, 15, ids);
    ok(hr == E_INVALIDARG, "Unexpected hr %#x.\n", hr);

    for (i = 0; i < ARRAY_SIZE(ids); ++i)
        ids[i] = i + 1;

    hr = IMFTransform_AddInputStreams(transform, 15, ids);
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);

    input_count = output_count = 0;
    hr = IMFTransform_GetStreamCount(transform, &input_count, &output_count);
    ok(hr == S_OK, "Failed to get stream count, hr %#x.\n", hr);
    ok(input_count == 16 && output_count == 1, "Unexpected stream count %u/%u.\n", input_count, output_count);

    memset(&input_info, 0, sizeof(input_info));
    hr = IMFTransform_GetInputStreamInfo(transform, 1, &input_info);
    ok(hr == S_OK, "Failed to get input info, hr %#x.\n", hr);
    ok((input_info.dwFlags & (MFT_INPUT_STREAM_REMOVABLE | MFT_INPUT_STREAM_OPTIONAL)) ==
            (MFT_INPUT_STREAM_REMOVABLE | MFT_INPUT_STREAM_OPTIONAL), "Unexpected flags %#x.\n", input_info.dwFlags);

    attributes = NULL;
    hr = IMFTransform_GetInputStreamAttributes(transform, 0, &attributes);
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);
    ok(!!attributes, "Unexpected attributes.\n");

    attributes2 = NULL;
    hr = IMFTransform_GetInputStreamAttributes(transform, 0, &attributes2);
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);
    ok(attributes == attributes2, "Unexpected instance.\n");

    IMFAttributes_Release(attributes2);
    IMFAttributes_Release(attributes);

    attributes = NULL;
    hr = IMFTransform_GetInputStreamAttributes(transform, 1, &attributes);
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);
    ok(!!attributes, "Unexpected attributes.\n");
    IMFAttributes_Release(attributes);

    hr = IMFTransform_DeleteInputStream(transform, 0);
    ok(hr == MF_E_INVALIDSTREAMNUMBER, "Unexpected hr %#x.\n", hr);

    hr = IMFTransform_DeleteInputStream(transform, 1);
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);

    input_count = output_count = 0;
    hr = IMFTransform_GetStreamCount(transform, &input_count, &output_count);
    ok(hr == S_OK, "Failed to get stream count, hr %#x.\n", hr);
    ok(input_count == 15 && output_count == 1, "Unexpected stream count %u/%u.\n", input_count, output_count);

    IMFTransform_Release(transform);

    hr = MFCreateVideoMixer(NULL, &IID_IMFTransform, &IID_IMFTransform, (void **)&transform);
    ok(hr == E_INVALIDARG, "Unexpected hr %#x.\n", hr);

    hr = CoCreateInstance(&CLSID_MFVideoMixer9, NULL, CLSCTX_INPROC_SERVER, &IID_IMFTransform, (void **)&transform);
    ok(hr == S_OK, "Failed to create default mixer, hr %#x.\n", hr);
    IMFTransform_Release(transform);
}

static void test_surface_sample(void)
{
    IDirect3DSurface9 *backbuffer = NULL;
    IMFDesiredSample *desired_sample;
    IMFMediaBuffer *buffer, *buffer2;
    IDirect3DSwapChain9 *swapchain;
    IDirect3DDevice9 *device;
    DWORD count, length;
    IMFSample *sample;
    LONGLONG duration;
    IDirect3D9 *d3d;
    IUnknown *unk;
    HWND window;
    HRESULT hr;
    BYTE *data;

    window = create_window();
    d3d = Direct3DCreate9(D3D_SDK_VERSION);
    ok(!!d3d, "Failed to create a D3D object.\n");
    if (!(device = create_device(d3d, window)))
    {
        skip("Failed to create a D3D device, skipping tests.\n");
        goto done;
    }

    hr = IDirect3DDevice9_GetSwapChain(device, 0, &swapchain);
    ok(SUCCEEDED(hr), "Failed to get the implicit swapchain (%08x)\n", hr);

    hr = IDirect3DSwapChain9_GetBackBuffer(swapchain, 0, D3DBACKBUFFER_TYPE_MONO, &backbuffer);
    ok(SUCCEEDED(hr), "Failed to get the back buffer (%08x)\n", hr);
    ok(backbuffer != NULL, "The back buffer is NULL\n");

    IDirect3DSwapChain9_Release(swapchain);

    hr = MFCreateVideoSampleFromSurface((IUnknown *)backbuffer, &sample);
todo_wine
    ok(hr == S_OK, "Failed to create surface sample, hr %#x.\n", hr);
    if (FAILED(hr)) goto done;

    hr = IMFSample_QueryInterface(sample, &IID_IMFTrackedSample, (void **)&unk);
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);
    IUnknown_Release(unk);

    hr = IMFSample_QueryInterface(sample, &IID_IMFDesiredSample, (void **)&desired_sample);
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);

    hr = IMFSample_GetCount(sample, &count);
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);
    ok(!count, "Unexpected attribute count %u.\n", count);

    IMFDesiredSample_SetDesiredSampleTimeAndDuration(desired_sample, 123, 456);

    hr = IMFSample_GetCount(sample, &count);
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);
    ok(!count, "Unexpected attribute count %u.\n", count);

    IMFDesiredSample_Release(desired_sample);

    hr = IMFSample_GetCount(sample, &count);
    ok(hr == S_OK, "Failed to get attribute count, hr %#x.\n", hr);
    ok(!count, "Unexpected attribute count.\n");

    hr = IMFSample_GetBufferCount(sample, &count);
    ok(hr == S_OK, "Failed to get buffer count, hr %#x.\n", hr);
    ok(count == 1, "Unexpected attribute count.\n");

    hr = IMFSample_GetTotalLength(sample, &length);
    ok(hr == S_OK, "Failed to get length, hr %#x.\n", hr);
    ok(!length, "Unexpected length %u.\n", length);

    hr = IMFSample_GetSampleDuration(sample, &duration);
    ok(hr == MF_E_NO_SAMPLE_DURATION, "Unexpected hr %#x.\n", hr);

    hr = IMFSample_GetSampleTime(sample, &duration);
    ok(hr == MF_E_NO_SAMPLE_TIMESTAMP, "Unexpected hr %#x.\n", hr);

    hr = IMFSample_GetBufferByIndex(sample, 0, &buffer);
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);

    hr = IMFMediaBuffer_GetMaxLength(buffer, &length);
    ok(hr == E_NOTIMPL, "Unexpected hr %#x.\n", hr);

    hr = IMFMediaBuffer_GetCurrentLength(buffer, &length);
    ok(hr == S_OK, "Failed to get length, hr %#x.\n", hr);
    ok(!length, "Unexpected length %u.\n", length);

    hr = IMFMediaBuffer_Lock(buffer, &data, NULL, NULL);
    ok(hr == E_NOTIMPL, "Unexpected hr %#x.\n", hr);

    hr = IMFMediaBuffer_QueryInterface(buffer, &IID_IMF2DBuffer, (void **)&unk);
    ok(hr == E_NOINTERFACE, "Unexpected hr %#x.\n", hr);

    hr = IMFSample_AddBuffer(sample, buffer);
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);

    hr = IMFSample_GetBufferCount(sample, &count);
    ok(hr == S_OK, "Failed to get buffer count, hr %#x.\n", hr);
    ok(count == 2, "Unexpected attribute count.\n");

    hr = IMFSample_ConvertToContiguousBuffer(sample, &buffer2);
    ok(hr == E_NOTIMPL, "Unexpected hr %#x.\n", hr);

    hr = IMFSample_CopyToBuffer(sample, buffer);
    ok(hr == E_NOTIMPL, "Unexpected hr %#x.\n", hr);

    hr = IMFSample_RemoveAllBuffers(sample);
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);

    hr = IMFSample_GetBufferCount(sample, &count);
    ok(hr == S_OK, "Failed to get buffer count, hr %#x.\n", hr);
    ok(!count, "Unexpected attribute count.\n");

    IMFMediaBuffer_Release(buffer);

    IMFSample_Release(sample);

done:
    if (backbuffer)
        IDirect3DSurface9_Release(backbuffer);
    IDirect3D9_Release(d3d);
    DestroyWindow(window);
}

static void test_default_mixer_type_negotiation(void)
{
    IDirect3DDeviceManager9 *manager;
    DXVA2_VideoProcessorCaps caps;
    IMFVideoMediaType *video_type;
    IMFVideoProcessor *processor;
    IMFMediaType *media_type;
    IDirect3DDevice9 *device;
    IMFTransform *transform;
    GUID guid, *guids;
    IDirect3D9 *d3d;
    IUnknown *unk;
    HWND window;
    DWORD count;
    HRESULT hr;
    UINT token;

    if (!pMFCreateVideoMediaTypeFromSubtype)
    {
        win_skip("Skipping mixer types tests.\n");
        return;
    }

    hr = MFCreateVideoMixer(NULL, &IID_IDirect3DDevice9, &IID_IMFTransform, (void **)&transform);
    ok(hr == S_OK, "Failed to create default mixer, hr %#x.\n", hr);

    hr = IMFTransform_GetInputAvailableType(transform, 0, 0, &media_type);
    ok(hr == E_NOTIMPL, "Unexpected hr %#x.\n", hr);

    hr = IMFTransform_GetInputCurrentType(transform, 0, &media_type);
    ok(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "Unexpected hr %#x.\n", hr);

    hr = MFCreateMediaType(&media_type);
    ok(hr == S_OK, "Failed to create media type, hr %#x.\n", hr);

    hr = IMFMediaType_SetGUID(media_type, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
    ok(hr == S_OK, "Failed to set attribute, hr %#x.\n", hr);

    hr = IMFMediaType_SetGUID(media_type, &MF_MT_SUBTYPE, &MFVideoFormat_RGB32);
    ok(hr == S_OK, "Failed to set attribute, hr %#x.\n", hr);

    hr = IMFTransform_SetInputType(transform, 0, media_type, 0);
    ok(hr == MF_E_NOT_INITIALIZED, "Unexpected hr %#x.\n", hr);

    /* Now try with device manager. */

    window = create_window();
    d3d = Direct3DCreate9(D3D_SDK_VERSION);
    ok(!!d3d, "Failed to create a D3D object.\n");
    if (!(device = create_device(d3d, window)))
    {
        skip("Failed to create a D3D device, skipping tests.\n");
        goto done;
    }

    hr = DXVA2CreateDirect3DDeviceManager9(&token, &manager);
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);

    hr = IMFTransform_ProcessMessage(transform, MFT_MESSAGE_SET_D3D_MANAGER, (ULONG_PTR)manager);
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);

    /* Now manager is not initialized. */
    hr = IMFTransform_SetInputType(transform, 0, media_type, 0);
    ok(hr == DXVA2_E_NOT_INITIALIZED, "Unexpected hr %#x.\n", hr);

    hr = IDirect3DDeviceManager9_ResetDevice(manager, device, token);
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);

    /* And now type description is incomplete. */
    hr = IMFTransform_SetInputType(transform, 0, media_type, 0);
    ok(hr == MF_E_INVALIDMEDIATYPE, "Unexpected hr %#x.\n", hr);
    IMFMediaType_Release(media_type);

    hr = pMFCreateVideoMediaTypeFromSubtype(&MFVideoFormat_RGB32, &video_type);
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);

    /* Partially initialized type. */
    hr = IMFTransform_SetInputType(transform, 0, (IMFMediaType *)video_type, 0);
    ok(hr == MF_E_INVALIDMEDIATYPE, "Unexpected hr %#x.\n", hr);

    /* Only required data - frame size and uncompressed marker. */
    hr = IMFVideoMediaType_SetUINT64(video_type, &MF_MT_FRAME_SIZE, (UINT64)640 << 32 | 480);
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);
    hr = IMFVideoMediaType_SetUINT32(video_type, &MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);

    hr = IMFTransform_SetInputType(transform, 0, (IMFMediaType *)video_type, 0);
todo_wine
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);

    hr = IMFTransform_QueryInterface(transform, &IID_IMFVideoProcessor, (void **)&processor);
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);

    hr = IMFVideoProcessor_GetVideoProcessorMode(processor, &guid);
todo_wine
    ok(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "Unexpected hr %#x.\n", hr);

    hr = IMFVideoProcessor_GetVideoProcessorCaps(processor, (GUID *)&DXVA2_VideoProcSoftwareDevice, &caps);
todo_wine
    ok(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "Unexpected hr %#x.\n", hr);

    hr = IMFTransform_GetInputCurrentType(transform, 0, &media_type);
todo_wine
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);
    if (SUCCEEDED(hr))
    {
        ok(media_type != (IMFMediaType *)video_type, "Unexpected pointer.\n");
        hr = IMFMediaType_QueryInterface(media_type, &IID_IMFVideoMediaType, (void **)&unk);
        ok(hr == S_OK, "Unexpected hr %#x.\n", hr);
        IUnknown_Release(unk);
        IMFMediaType_Release(media_type);
    }

    hr = IMFVideoProcessor_GetAvailableVideoProcessorModes(processor, &count, &guids);
todo_wine
    ok(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "Unexpected hr %#x.\n", hr);

    hr = IMFTransform_GetOutputAvailableType(transform, 0, 0, &media_type);
todo_wine
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);

    if (SUCCEEDED(hr))
    {
        hr = IMFTransform_SetOutputType(transform, 0, media_type, 0);
        ok(hr == S_OK, "Unexpected hr %#x.\n", hr);
        IMFMediaType_Release(media_type);
    }

    hr = IMFVideoProcessor_GetVideoProcessorMode(processor, &guid);
todo_wine
    ok(hr == S_FALSE, "Unexpected hr %#x.\n", hr);

    hr = IMFVideoProcessor_GetAvailableVideoProcessorModes(processor, &count, &guids);
todo_wine
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);
    if (SUCCEEDED(hr))
        CoTaskMemFree(guids);

    IMFVideoProcessor_Release(processor);

    IMFVideoMediaType_Release(video_type);

    IDirect3DDeviceManager9_Release(manager);

    IDirect3DDevice9_Release(device);

done:
    IMFTransform_Release(transform);
    IDirect3D9_Release(d3d);
    DestroyWindow(window);
}

static void test_default_presenter(void)
{
    IMFVideoPresenter *presenter;
    IMFRateSupport *rate_support;
    IMFVideoDeviceID *deviceid;
    IUnknown *unk;
    float rate;
    HRESULT hr;
    GUID iid;

    hr = MFCreateVideoPresenter(NULL, &IID_IMFVideoPresenter, &IID_IMFVideoPresenter, (void **)&presenter);
    ok(hr == E_INVALIDARG, "Unexpected hr %#x.\n", hr);

    hr = MFCreateVideoPresenter(NULL, &IID_IDirect3DDevice9, &IID_IMFVideoPresenter, (void **)&presenter);
    ok(hr == S_OK || broken(hr == E_FAIL) /* WinXP */, "Failed to create default presenter, hr %#x.\n", hr);
    if (FAILED(hr))
        return;

    hr = IMFVideoPresenter_QueryInterface(presenter, &IID_IMFVideoDeviceID, (void **)&deviceid);
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);

    hr = IMFVideoDeviceID_GetDeviceID(deviceid, NULL);
    ok(hr == E_POINTER, "Unexpected hr %#x.\n", hr);

    hr = IMFVideoDeviceID_GetDeviceID(deviceid, &iid);
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);
    ok(IsEqualIID(&iid, &IID_IDirect3DDevice9), "Unexpected id %s.\n", wine_dbgstr_guid(&iid));

    IMFVideoDeviceID_Release(deviceid);

    hr = IMFVideoPresenter_QueryInterface(presenter, &IID_IMFTopologyServiceLookupClient, (void **)&unk);
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);
    IUnknown_Release(unk);

    hr = IMFVideoPresenter_QueryInterface(presenter, &IID_IMFVideoDisplayControl, (void **)&unk);
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);
    IUnknown_Release(unk);

    /* Rate support. */
    hr = IMFVideoPresenter_QueryInterface(presenter, &IID_IMFRateSupport, (void **)&rate_support);
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);

    rate = 1.0f;
    hr = IMFRateSupport_GetSlowestRate(rate_support, MFRATE_FORWARD, FALSE, &rate);
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);
    ok(rate == 0.0f, "Unexpected rate %f.\n", rate);

    rate = 1.0f;
    hr = IMFRateSupport_GetSlowestRate(rate_support, MFRATE_FORWARD, TRUE, &rate);
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);
    ok(rate == 0.0f, "Unexpected rate %f.\n", rate);

    rate = 1.0f;
    hr = IMFRateSupport_GetSlowestRate(rate_support, MFRATE_REVERSE, FALSE, &rate);
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);
    ok(rate == 0.0f, "Unexpected rate %f.\n", rate);

    rate = 1.0f;
    hr = IMFRateSupport_GetSlowestRate(rate_support, MFRATE_REVERSE, TRUE, &rate);
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);
    ok(rate == 0.0f, "Unexpected rate %f.\n", rate);

    IMFRateSupport_Release(rate_support);

    IMFVideoPresenter_Release(presenter);
}

static void test_MFCreateVideoMixerAndPresenter(void)
{
    IUnknown *mixer, *presenter;
    HRESULT hr;

    hr = MFCreateVideoMixerAndPresenter(NULL, NULL, &IID_IUnknown, (void **)&mixer, &IID_IUnknown, (void **)&presenter);
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);

    IUnknown_Release(mixer);
    IUnknown_Release(presenter);
}

static void test_MFCreateVideoSampleAllocator(void)
{
    IMFVideoSampleAllocatorCallback *allocator_cb;
    IMFVideoSampleAllocator *allocator;
    IMFVideoMediaType *video_type;
    IMFSample *sample, *sample2;
    IDirect3DSurface9 *surface;
    IMFMediaType *media_type;
    IMFMediaBuffer *buffer;
    IMFGetService *gs;
    IUnknown *unk;
    HRESULT hr;
    LONG count;

    hr = MFCreateVideoSampleAllocator(&IID_IUnknown, (void **)&unk);
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);
    IUnknown_Release(unk);

    hr = MFCreateVideoSampleAllocator(&IID_IMFVideoSampleAllocator, (void **)&allocator);
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);

    hr = IMFVideoSampleAllocator_QueryInterface(allocator, &IID_IMFVideoSampleAllocatorCallback, (void **)&allocator_cb);
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);

    count = 10;
    hr = IMFVideoSampleAllocatorCallback_GetFreeSampleCount(allocator_cb, &count);
todo_wine {
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);
    ok(!count, "Unexpected count %d.\n", count);
}
    hr = IMFVideoSampleAllocator_UninitializeSampleAllocator(allocator);
todo_wine
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);

    hr = IMFVideoSampleAllocator_AllocateSample(allocator, &sample);
todo_wine
    ok(hr == MF_E_NOT_INITIALIZED, "Unexpected hr %#x.\n", hr);

    hr = MFCreateMediaType(&media_type);
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);

    /* It expects IMFVideoMediaType. */
    hr = IMFVideoSampleAllocator_InitializeSampleAllocator(allocator, 2, media_type);
todo_wine
    ok(hr == E_NOINTERFACE, "Unexpected hr %#x.\n", hr);

    hr = MFCreateVideoMediaTypeFromSubtype(&MFVideoFormat_RGB32, &video_type);
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);

    hr = IMFVideoSampleAllocator_InitializeSampleAllocator(allocator, 2, (IMFMediaType *)video_type);
todo_wine
    ok(hr == MF_E_INVALIDMEDIATYPE, "Unexpected hr %#x.\n", hr);

    /* Frame size is required. */
    hr = IMFVideoMediaType_SetUINT64(video_type, &MF_MT_FRAME_SIZE, (UINT64) 320 << 32 | 240);
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);
    hr = IMFVideoSampleAllocator_InitializeSampleAllocator(allocator, 0, (IMFMediaType *)video_type);
todo_wine
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);

    hr = IMFVideoSampleAllocatorCallback_GetFreeSampleCount(allocator_cb, &count);
todo_wine {
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);
    ok(count == 1, "Unexpected count %d.\n", count);
}
    sample = NULL;
    hr = IMFVideoSampleAllocator_AllocateSample(allocator, &sample);
todo_wine
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);
    if (SUCCEEDED(hr))
        ok(get_refcount(sample) == 3, "Unexpected refcount %u.\n", get_refcount(sample));

    hr = IMFVideoSampleAllocator_AllocateSample(allocator, &sample2);
todo_wine
    ok(hr == MF_E_SAMPLEALLOCATOR_EMPTY, "Unexpected hr %#x.\n", hr);

    /* Reinitialize with active sample. */
    hr = IMFVideoSampleAllocator_InitializeSampleAllocator(allocator, 4, (IMFMediaType *)video_type);
todo_wine
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);
    if (sample)
        ok(get_refcount(sample) == 3, "Unexpected refcount %u.\n", get_refcount(sample));

    hr = IMFVideoSampleAllocatorCallback_GetFreeSampleCount(allocator_cb, &count);
todo_wine {
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);
    ok(count == 4, "Unexpected count %d.\n", count);
}
    if (sample)
    {
        hr = IMFSample_QueryInterface(sample, &IID_IMFDesiredSample, (void **)&unk);
        ok(hr == S_OK, "Unexpected hr %#x.\n", hr);
        IUnknown_Release(unk);

        hr = IMFSample_QueryInterface(sample, &IID_IMFTrackedSample, (void **)&unk);
        ok(hr == S_OK, "Unexpected hr %#x.\n", hr);
        IUnknown_Release(unk);

        hr = IMFSample_GetBufferByIndex(sample, 0, &buffer);
        ok(hr == S_OK, "Unexpected hr %#x.\n", hr);

        hr = IMFMediaBuffer_QueryInterface(buffer, &IID_IMFGetService, (void **)&gs);
        ok(hr == S_OK, "Unexpected hr %#x.\n", hr);

        /* Device manager wasn't set, sample get regular memory buffers. */
        hr = IMFGetService_GetService(gs, &MR_BUFFER_SERVICE, &IID_IDirect3DSurface9, (void **)&surface);
        ok(hr == E_NOTIMPL, "Unexpected hr %#x.\n", hr);

        IMFMediaBuffer_Release(buffer);

        IMFGetService_Release(gs);
        IMFSample_Release(sample);
    }

    IMFVideoSampleAllocatorCallback_Release(allocator_cb);

    IMFMediaType_Release(media_type);

    IMFVideoSampleAllocator_Release(allocator);

    hr = MFCreateVideoSampleAllocator(&IID_IMFVideoSampleAllocatorCallback, (void **)&unk);
    ok(hr == S_OK, "Unexpected hr %#x.\n", hr);
    IUnknown_Release(unk);
}

START_TEST(evr)
{
    CoInitialize(NULL);

    pMFCreateVideoMediaTypeFromSubtype = (void *)GetProcAddress(GetModuleHandleA("mfplat.dll"), "MFCreateVideoMediaTypeFromSubtype");

    test_aggregation();
    test_interfaces();
    test_enum_pins();
    test_find_pin();
    test_pin_info();
    test_default_mixer();
    test_default_mixer_type_negotiation();
    test_surface_sample();
    test_default_presenter();
    test_MFCreateVideoMixerAndPresenter();
    test_MFCreateVideoSampleAllocator();

    CoUninitialize();
}
