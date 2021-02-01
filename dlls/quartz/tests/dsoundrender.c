/*
 * DirectSound renderer filter unit tests
 *
 * Copyright (C) 2010 Maarten Lankhorst for CodeWeavers
 * Copyright (C) 2007 Google (Lei Zhang)
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
#include <math.h>
#include "dshow.h"
#include "initguid.h"
#include "dsound.h"
#include "amaudio.h"
#include "mmreg.h"
#include "wine/strmbase.h"
#include "wine/test.h"

static const WCHAR sink_id[] = L"Audio Input pin (rendered)";

static IBaseFilter *create_dsound_render(void)
{
    IBaseFilter *filter = NULL;
    HRESULT hr = CoCreateInstance(&CLSID_DSoundRender, NULL, CLSCTX_INPROC_SERVER,
            &IID_IBaseFilter, (void **)&filter);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    return filter;
}

static inline BOOL compare_media_types(const AM_MEDIA_TYPE *a, const AM_MEDIA_TYPE *b)
{
    return !memcmp(a, b, offsetof(AM_MEDIA_TYPE, pbFormat))
        && !memcmp(a->pbFormat, b->pbFormat, a->cbFormat);
}

static ULONG get_refcount(void *iface)
{
    IUnknown *unknown = iface;
    IUnknown_AddRef(unknown);
    return IUnknown_Release(unknown);
}

static HRESULT WINAPI property_bag_QueryInterface(IPropertyBag *iface, REFIID iid, void **out)
{
    ok(0, "Unexpected call (iid %s).\n", wine_dbgstr_guid(iid));
    return E_NOINTERFACE;
}

static ULONG WINAPI property_bag_AddRef(IPropertyBag *iface)
{
    ok(0, "Unexpected call.\n");
    return 2;
}

static ULONG WINAPI property_bag_Release(IPropertyBag *iface)
{
    ok(0, "Unexpected call.\n");
    return 1;
}

static HRESULT WINAPI property_bag_Read(IPropertyBag *iface, const WCHAR *name, VARIANT *var, IErrorLog *log)
{
    WCHAR guidstr[39];

    ok(!wcscmp(name, L"DSGuid"), "Got unexpected name %s.\n", wine_dbgstr_w(name));
    ok(V_VT(var) == VT_BSTR, "Got unexpected type %u.\n", V_VT(var));
    StringFromGUID2(&DSDEVID_DefaultPlayback, guidstr, ARRAY_SIZE(guidstr));
    V_BSTR(var) = SysAllocString(guidstr);
    return S_OK;
}

static HRESULT WINAPI property_bag_Write(IPropertyBag *iface, const WCHAR *name, VARIANT *var)
{
    ok(0, "Unexpected call (name %s).\n", wine_dbgstr_w(name));
    return E_FAIL;
}

static const IPropertyBagVtbl property_bag_vtbl =
{
    property_bag_QueryInterface,
    property_bag_AddRef,
    property_bag_Release,
    property_bag_Read,
    property_bag_Write,
};

static void test_property_bag(void)
{
    IPropertyBag property_bag = {&property_bag_vtbl};
    IPersistPropertyBag *ppb;
    HRESULT hr;
    ULONG ref;

    hr = CoCreateInstance(&CLSID_DSoundRender, NULL, CLSCTX_INPROC_SERVER,
            &IID_IPersistPropertyBag, (void **)&ppb);
    todo_wine ok(hr == S_OK, "Got hr %#x.\n", hr);
    if (hr != S_OK) return;

    hr = IPersistPropertyBag_InitNew(ppb);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IPersistPropertyBag_Load(ppb, &property_bag, NULL);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    ref = IPersistPropertyBag_Release(ppb);
    ok(!ref, "Got unexpected refcount %d.\n", ref);
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
    IBaseFilter *filter = create_dsound_render();
    IPin *pin;

    check_interface(filter, &IID_IAMDirectSound, TRUE);
    check_interface(filter, &IID_IBaseFilter, TRUE);
    check_interface(filter, &IID_IBasicAudio, TRUE);
    todo_wine check_interface(filter, &IID_IDirectSound3DBuffer, TRUE);
    check_interface(filter, &IID_IMediaFilter, TRUE);
    check_interface(filter, &IID_IMediaPosition, TRUE);
    check_interface(filter, &IID_IMediaSeeking, TRUE);
    check_interface(filter, &IID_IPersist, TRUE);
    todo_wine check_interface(filter, &IID_IPersistPropertyBag, TRUE);
    check_interface(filter, &IID_IQualityControl, TRUE);
    check_interface(filter, &IID_IReferenceClock, TRUE);
    check_interface(filter, &IID_IUnknown, TRUE);

    check_interface(filter, &IID_IAMFilterMiscFlags, FALSE);
    check_interface(filter, &IID_IBasicVideo, FALSE);
    check_interface(filter, &IID_IDispatch, FALSE);
    check_interface(filter, &IID_IKsPropertySet, FALSE);
    check_interface(filter, &IID_IPin, FALSE);
    check_interface(filter, &IID_IQualProp, FALSE);
    check_interface(filter, &IID_IVideoWindow, FALSE);

    IBaseFilter_FindPin(filter, sink_id, &pin);

    check_interface(pin, &IID_IPin, TRUE);
    check_interface(pin, &IID_IMemInputPin, TRUE);
    todo_wine check_interface(pin, &IID_IQualityControl, TRUE);
    check_interface(pin, &IID_IUnknown, TRUE);

    check_interface(pin, &IID_IAsyncReader, FALSE);
    check_interface(pin, &IID_IMediaPosition, FALSE);
    check_interface(pin, &IID_IMediaSeeking, FALSE);

    IPin_Release(pin);
    IBaseFilter_Release(filter);
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
    IUnknown *unk, *unk2;
    HRESULT hr;
    ULONG ref;

    filter = (IBaseFilter *)0xdeadbeef;
    hr = CoCreateInstance(&CLSID_DSoundRender, &test_outer, CLSCTX_INPROC_SERVER,
            &IID_IBaseFilter, (void **)&filter);
    ok(hr == E_NOINTERFACE, "Got hr %#x.\n", hr);
    ok(!filter, "Got interface %p.\n", filter);

    hr = CoCreateInstance(&CLSID_DSoundRender, &test_outer, CLSCTX_INPROC_SERVER,
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
}

static void test_enum_pins(void)
{
    IBaseFilter *filter = create_dsound_render();
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
    IBaseFilter *filter = create_dsound_render();
    IEnumPins *enum_pins;
    IPin *pin, *pin2;
    HRESULT hr;
    ULONG ref;

    hr = IBaseFilter_FindPin(filter, L"In", &pin);
    ok(hr == VFW_E_NOT_FOUND, "Got hr %#x.\n", hr);

    hr = IBaseFilter_FindPin(filter, L"input pin", &pin);
    ok(hr == VFW_E_NOT_FOUND, "Got hr %#x.\n", hr);

    hr = IBaseFilter_FindPin(filter, sink_id, &pin);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IBaseFilter_EnumPins(filter, &enum_pins);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IEnumPins_Next(enum_pins, 1, &pin2, NULL);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(pin == pin2, "Expected pin %p, got %p.\n", pin2, pin);
    IPin_Release(pin);
    IPin_Release(pin2);

    IEnumPins_Release(enum_pins);
    ref = IBaseFilter_Release(filter);
    ok(!ref, "Got outstanding refcount %d.\n", ref);
}

static void test_pin_info(void)
{
    IBaseFilter *filter = create_dsound_render();
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
    ok(!wcscmp(info.achName, sink_id), "Got name %s.\n", wine_dbgstr_w(info.achName));
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
    ok(!wcscmp(id, sink_id), "Got id %s.\n", wine_dbgstr_w(id));
    CoTaskMemFree(id);

    hr = IPin_QueryInternalConnections(pin, NULL, NULL);
    ok(hr == E_NOTIMPL, "Got hr %#x.\n", hr);

    IPin_Release(pin);
    ref = IBaseFilter_Release(filter);
    ok(!ref, "Got outstanding refcount %d.\n", ref);
}

static void test_basic_audio(void)
{
    IBaseFilter *filter = create_dsound_render();
    LONG balance, volume;
    ITypeInfo *typeinfo;
    IBasicAudio *audio;
    TYPEATTR *typeattr;
    ULONG ref, count;
    HRESULT hr;

    hr = IBaseFilter_QueryInterface(filter, &IID_IBasicAudio, (void **)&audio);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IBasicAudio_get_Balance(audio, NULL);
    ok(hr == E_POINTER, "Got hr %#x.\n", hr);

    hr = IBasicAudio_get_Balance(audio, &balance);
    if (hr != VFW_E_MONO_AUDIO_HW)
    {
        ok(hr == S_OK, "Got hr %#x.\n", hr);
        ok(balance == 0, "Got balance %d.\n", balance);

        hr = IBasicAudio_put_Balance(audio, DSBPAN_LEFT - 1);
        ok(hr == E_INVALIDARG, "Got hr %#x.\n", hr);

        hr = IBasicAudio_put_Balance(audio, DSBPAN_LEFT);
        ok(hr == S_OK, "Got hr %#x.\n", hr);

        hr = IBasicAudio_get_Balance(audio, &balance);
        ok(hr == S_OK, "Got hr %#x.\n", hr);
        ok(balance == DSBPAN_LEFT, "Got balance %d.\n", balance);
    }

    hr = IBasicAudio_get_Volume(audio, &volume);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(volume == 0, "Got volume %d.\n", volume);

    hr = IBasicAudio_put_Volume(audio, DSBVOLUME_MIN - 1);
    ok(hr == E_INVALIDARG, "Got hr %#x.\n", hr);

    hr = IBasicAudio_put_Volume(audio, DSBVOLUME_MIN);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IBasicAudio_get_Volume(audio, &volume);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(volume == DSBVOLUME_MIN, "Got volume %d.\n", volume);

    hr = IBasicAudio_GetTypeInfoCount(audio, &count);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(count == 1, "Got count %u.\n", count);

    hr = IBasicAudio_GetTypeInfo(audio, 0, 0, &typeinfo);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    hr = ITypeInfo_GetTypeAttr(typeinfo, &typeattr);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(typeattr->typekind == TKIND_DISPATCH, "Got kind %u.\n", typeattr->typekind);
    ok(IsEqualGUID(&typeattr->guid, &IID_IBasicAudio), "Got IID %s.\n", wine_dbgstr_guid(&typeattr->guid));
    ITypeInfo_ReleaseTypeAttr(typeinfo, typeattr);
    ITypeInfo_Release(typeinfo);

    IBasicAudio_Release(audio);
    ref = IBaseFilter_Release(filter);
    ok(!ref, "Got outstanding refcount %d.\n", ref);
}

static void test_enum_media_types(void)
{
    IBaseFilter *filter = create_dsound_render();
    IEnumMediaTypes *enum1, *enum2;
    AM_MEDIA_TYPE *mts[2];
    ULONG ref, count;
    HRESULT hr;
    IPin *pin;

    IBaseFilter_FindPin(filter, sink_id, &pin);

    hr = IPin_EnumMediaTypes(pin, &enum1);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IEnumMediaTypes_Next(enum1, 1, mts, NULL);
    todo_wine ok(hr == S_OK, "Got hr %#x.\n", hr);
    if (hr == S_OK) CoTaskMemFree(mts[0]->pbFormat);
    if (hr == S_OK) CoTaskMemFree(mts[0]);

    hr = IEnumMediaTypes_Next(enum1, 1, mts, NULL);
    ok(hr == S_FALSE, "Got hr %#x.\n", hr);

    hr = IEnumMediaTypes_Reset(enum1);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IEnumMediaTypes_Next(enum1, 1, mts, &count);
    todo_wine ok(hr == S_OK, "Got hr %#x.\n", hr);
    todo_wine ok(count == 1, "Got count %u.\n", count);
    if (hr == S_OK) CoTaskMemFree(mts[0]->pbFormat);
    if (hr == S_OK) CoTaskMemFree(mts[0]);

    hr = IEnumMediaTypes_Next(enum1, 1, mts, &count);
    ok(hr == S_FALSE, "Got hr %#x.\n", hr);
    ok(!count, "Got count %u.\n", count);

    hr = IEnumMediaTypes_Reset(enum1);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IEnumMediaTypes_Next(enum1, 2, mts, &count);
    ok(hr == S_FALSE, "Got hr %#x.\n", hr);
    todo_wine ok(count == 1, "Got count %u.\n", count);
    if (count > 0) CoTaskMemFree(mts[0]->pbFormat);
    if (count > 0) CoTaskMemFree(mts[0]);

    hr = IEnumMediaTypes_Reset(enum1);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IEnumMediaTypes_Clone(enum1, &enum2);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IEnumMediaTypes_Skip(enum1, 2);
    ok(hr == S_FALSE, "Got hr %#x.\n", hr);

    hr = IEnumMediaTypes_Skip(enum1, 1);
    ok(hr == S_FALSE, "Got hr %#x.\n", hr);

    hr = IEnumMediaTypes_Reset(enum1);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IEnumMediaTypes_Skip(enum1, 1);
    todo_wine ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IEnumMediaTypes_Skip(enum1, 1);
    ok(hr == S_FALSE, "Got hr %#x.\n", hr);

    hr = IEnumMediaTypes_Next(enum1, 1, mts, NULL);
    ok(hr == S_FALSE, "Got hr %#x.\n", hr);

    hr = IEnumMediaTypes_Next(enum2, 1, mts, NULL);
    todo_wine ok(hr == S_OK, "Got hr %#x.\n", hr);
    if (hr == S_OK) CoTaskMemFree(mts[0]->pbFormat);
    if (hr == S_OK) CoTaskMemFree(mts[0]);

    IEnumMediaTypes_Release(enum1);
    IEnumMediaTypes_Release(enum2);
    IPin_Release(pin);

    ref = IBaseFilter_Release(filter);
    ok(!ref, "Got outstanding refcount %d.\n", ref);
}

struct testfilter
{
    struct strmbase_filter filter;
    struct strmbase_source source;
};

static inline struct testfilter *impl_from_strmbase_filter(struct strmbase_filter *iface)
{
    return CONTAINING_RECORD(iface, struct testfilter, filter);
}

static struct strmbase_pin *testfilter_get_pin(struct strmbase_filter *iface, unsigned int index)
{
    struct testfilter *filter = impl_from_strmbase_filter(iface);
    if (!index)
        return &filter->source.pin;
    return NULL;
}

static void testfilter_destroy(struct strmbase_filter *iface)
{
    struct testfilter *filter = impl_from_strmbase_filter(iface);
    strmbase_source_cleanup(&filter->source);
    strmbase_filter_cleanup(&filter->filter);
}

static const struct strmbase_filter_ops testfilter_ops =
{
    .filter_get_pin = testfilter_get_pin,
    .filter_destroy = testfilter_destroy,
};

static HRESULT testsource_query_interface(struct strmbase_pin *iface, REFIID iid, void **out)
{
    ok(!IsEqualGUID(iid, &IID_IQualityControl), "Unexpected query for IQualityControl.\n");
    return E_NOINTERFACE;
}

static HRESULT WINAPI testsource_DecideAllocator(struct strmbase_source *iface,
        IMemInputPin *peer, IMemAllocator **allocator)
{
    return S_OK;
}

static const struct strmbase_source_ops testsource_ops =
{
    .base.pin_query_interface = testsource_query_interface,
    .pfnAttemptConnection = BaseOutputPinImpl_AttemptConnection,
    .pfnDecideAllocator = testsource_DecideAllocator,
};

static void testfilter_init(struct testfilter *filter)
{
    static const GUID clsid = {0xabacab};
    strmbase_filter_init(&filter->filter, NULL, &clsid, &testfilter_ops);
    strmbase_source_init(&filter->source, &filter->filter, L"", &testsource_ops);
}

static void test_allocator(IMemInputPin *input)
{
    IMemAllocator *req_allocator, *ret_allocator;
    ALLOCATOR_PROPERTIES props;
    HRESULT hr;

    hr = IMemInputPin_GetAllocatorRequirements(input, &props);
    ok(hr == E_NOTIMPL, "Got hr %#x.\n", hr);

    hr = IMemInputPin_GetAllocator(input, &ret_allocator);
    todo_wine ok(hr == S_OK, "Got hr %#x.\n", hr);

    if (hr == S_OK)
    {
        hr = IMemAllocator_GetProperties(ret_allocator, &props);
        ok(hr == S_OK, "Got hr %#x.\n", hr);
        ok(!props.cBuffers, "Got %d buffers.\n", props.cBuffers);
        ok(!props.cbBuffer, "Got size %d.\n", props.cbBuffer);
        ok(!props.cbAlign, "Got alignment %d.\n", props.cbAlign);
        ok(!props.cbPrefix, "Got prefix %d.\n", props.cbPrefix);

        hr = IMemInputPin_NotifyAllocator(input, ret_allocator, TRUE);
        ok(hr == S_OK, "Got hr %#x.\n", hr);
        IMemAllocator_Release(ret_allocator);
    }

    hr = IMemInputPin_NotifyAllocator(input, NULL, TRUE);
    ok(hr == E_POINTER, "Got hr %#x.\n", hr);

    CoCreateInstance(&CLSID_MemoryAllocator, NULL, CLSCTX_INPROC_SERVER,
            &IID_IMemAllocator, (void **)&req_allocator);

    hr = IMemInputPin_NotifyAllocator(input, req_allocator, TRUE);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IMemInputPin_GetAllocator(input, &ret_allocator);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(ret_allocator == req_allocator, "Allocators didn't match.\n");

    IMemAllocator_Release(req_allocator);
    IMemAllocator_Release(ret_allocator);
}

struct frame_thread_params
{
    IMemInputPin *sink;
    IMediaSample *sample;
};

static DWORD WINAPI frame_thread(void *arg)
{
    struct frame_thread_params *params = arg;
    HRESULT hr;

    if (winetest_debug > 1) trace("%04x: Sending frame.\n", GetCurrentThreadId());
    hr = IMemInputPin_Receive(params->sink, params->sample);
    if (winetest_debug > 1) trace("%04x: Returned %#x.\n", GetCurrentThreadId(), hr);
    IMediaSample_Release(params->sample);
    free(params);
    return hr;
}

static HRESULT send_frame(IMemInputPin *sink)
{
    struct frame_thread_params *params = malloc(sizeof(*params));
    REFERENCE_TIME start_time, end_time;
    IMemAllocator *allocator;
    unsigned short *words;
    IMediaSample *sample;
    unsigned int i;
    HANDLE thread;
    HRESULT hr;
    BYTE *data;
    DWORD ret;

    hr = IMemInputPin_GetAllocator(sink, &allocator);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IMemAllocator_GetBuffer(allocator, &sample, NULL, NULL, 0);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IMediaSample_GetPointer(sample, &data);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    words = (unsigned short *)data;
    for (i = 0; i < 44100 * 2; i += 2)
        words[i] = words[i+1] = sinf(i / 20.0f) * 0x7fff;

    hr = IMediaSample_SetActualDataLength(sample, 44100 * 4);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    start_time = 0;
    end_time = start_time + 10000000;
    hr = IMediaSample_SetTime(sample, &start_time, &end_time);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    params->sink = sink;
    params->sample = sample;
    thread = CreateThread(NULL, 0, frame_thread, params, 0, NULL);
    ret = WaitForSingleObject(thread, 500);
    todo_wine_if (ret) ok(!ret, "Wait failed.\n");
    GetExitCodeThread(thread, &ret);
    CloseHandle(thread);

    IMemAllocator_Release(allocator);
    return ret;
}

static void test_filter_state(IMemInputPin *input, IMediaControl *control)
{
    OAFilterState state;
    HRESULT hr;

    hr = send_frame(input);
    ok(hr == VFW_E_WRONG_STATE, "Got hr %#x.\n", hr);

    /* The renderer is not fully paused until it receives a sample. The
     * DirectSound renderer never blocks in Receive(), despite returning S_OK
     * from ReceiveCanBlock(). Instead it holds on to each sample until its
     * presentation time, then writes it into the buffer. This is more work
     * than it's worth to emulate, so for now, we'll ignore this behaviour. */

    hr = IMediaControl_Pause(control);
    ok(hr == S_FALSE, "Got hr %#x.\n", hr);

    /* It's possible to queue multiple samples while paused. The number of
     * samples that can be queued depends on the length of each sample, but
     * it's not particularly clear how. */

    hr = IMediaControl_GetState(control, 0, &state);
    ok(hr == VFW_S_STATE_INTERMEDIATE, "Got hr %#x.\n", hr);

    hr = send_frame(input);
    todo_wine ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IMediaControl_GetState(control, 1000, &state);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IMediaControl_Stop(control);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = send_frame(input);
    ok(hr == VFW_E_WRONG_STATE, "Got hr %#x.\n", hr);

    hr = IMediaControl_Pause(control);
    ok(hr == S_FALSE, "Got hr %#x.\n", hr);

    hr = IMediaControl_GetState(control, 0, &state);
    ok(hr == VFW_S_STATE_INTERMEDIATE, "Got hr %#x.\n", hr);

    hr = send_frame(input);
    todo_wine ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IMediaControl_GetState(control, 1000, &state);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IMediaControl_Run(control);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IMediaControl_GetState(control, 0, &state);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = send_frame(input);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = send_frame(input);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IMediaControl_GetState(control, 0, &state);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IMediaControl_Run(control);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IMediaControl_GetState(control, 0, &state);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IMediaControl_Pause(control);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IMediaControl_GetState(control, 0, &state);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IMediaControl_Stop(control);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IMediaControl_GetState(control, 0, &state);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    /* The DirectSound renderer will silently refuse to transition to running
     * if it hasn't finished pausing yet. Once it does it reports itself as
     * completely paused. */
}

static void test_flushing(IPin *pin, IMemInputPin *input, IMediaControl *control)
{
    OAFilterState state;
    HRESULT hr;

    hr = IMediaControl_Pause(control);
    ok(hr == S_FALSE, "Got hr %#x.\n", hr);

    hr = send_frame(input);
    todo_wine ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IMediaControl_GetState(control, 0, &state);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IPin_BeginFlush(pin);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = send_frame(input);
    ok(hr == S_FALSE, "Got hr %#x.\n", hr);

    hr = IPin_EndFlush(pin);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IMediaControl_GetState(control, 0, &state);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IMediaControl_Run(control);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IPin_BeginFlush(pin);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = send_frame(input);
    ok(hr == S_FALSE, "Got hr %#x.\n", hr);

    hr = IPin_EndFlush(pin);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = send_frame(input);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IMediaControl_Stop(control);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
}

static unsigned int check_ec_complete(IMediaEvent *eventsrc, DWORD timeout)
{
    LONG_PTR param1, param2;
    unsigned int ret = 0;
    HRESULT hr;
    LONG code;

    while ((hr = IMediaEvent_GetEvent(eventsrc, &code, &param1, &param2, timeout)) == S_OK)
    {
        if (code == EC_COMPLETE)
        {
            ok(param1 == S_OK, "Got param1 %#lx.\n", param1);
            ok(!param2, "Got param2 %#lx.\n", param2);
            ret++;
        }
        IMediaEvent_FreeEventParams(eventsrc, code, param1, param2);
        timeout = 0;
    }
    ok(hr == E_ABORT, "Got hr %#x.\n", hr);

    return ret;
}

static void test_eos(IPin *pin, IMemInputPin *input, IMediaControl *control)
{
    IMediaEvent *eventsrc;
    OAFilterState state;
    HRESULT hr;
    BOOL ret;

    IMediaControl_QueryInterface(control, &IID_IMediaEvent, (void **)&eventsrc);

    hr = IMediaControl_Pause(control);
    ok(hr == S_FALSE, "Got hr %#x.\n", hr);
    ret = check_ec_complete(eventsrc, 0);
    ok(!ret, "Got unexpected EC_COMPLETE.\n");

    hr = IPin_EndOfStream(pin);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IMediaControl_GetState(control, 1000, &state);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ret = check_ec_complete(eventsrc, 0);
    ok(!ret, "Got unexpected EC_COMPLETE.\n");

    hr = send_frame(input);
    todo_wine ok(hr == VFW_E_SAMPLE_REJECTED_EOS, "Got hr %#x.\n", hr);

    hr = IMediaControl_Run(control);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ret = check_ec_complete(eventsrc, 0);
    todo_wine ok(ret == 1, "Expected EC_COMPLETE.\n");

    hr = IMediaControl_Stop(control);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ret = check_ec_complete(eventsrc, 0);
    ok(!ret, "Got unexpected EC_COMPLETE.\n");

    /* We do not receive an EC_COMPLETE notification until the last sample is
     * done rendering. */

    hr = IMediaControl_Pause(control);
    ok(hr == S_FALSE, "Got hr %#x.\n", hr);
    hr = send_frame(input);
    todo_wine ok(hr == S_OK, "Got hr %#x.\n", hr);
    hr = IMediaControl_GetState(control, 1000, &state);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    hr = IMediaControl_Run(control);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ret = check_ec_complete(eventsrc, 0);
    ok(!ret, "Got unexpected EC_COMPLETE.\n");

    hr = IPin_EndOfStream(pin);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ret = check_ec_complete(eventsrc, 0);
    ok(!ret, "Got unexpected EC_COMPLETE.\n");
    ret = check_ec_complete(eventsrc, 2000);
    todo_wine ok(ret == 1, "Expected EC_COMPLETE.\n");

    hr = IMediaControl_Stop(control);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ret = check_ec_complete(eventsrc, 0);
    ok(!ret, "Got unexpected EC_COMPLETE.\n");

    /* Test sending EOS while flushing. */

    hr = IMediaControl_Run(control);
    ok(hr == S_FALSE, "Got hr %#x.\n", hr);
    hr = send_frame(input);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IPin_BeginFlush(pin);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    hr = IPin_EndOfStream(pin);
    todo_wine ok(hr == S_FALSE, "Got hr %#x.\n", hr);
    hr = IPin_EndFlush(pin);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IMediaControl_Stop(control);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ret = check_ec_complete(eventsrc, 0);
    ok(!ret, "Got unexpected EC_COMPLETE.\n");

    /* Test sending EOS and then flushing or stopping. */

    hr = IMediaControl_Pause(control);
    ok(hr == S_FALSE, "Got hr %#x.\n", hr);
    hr = send_frame(input);
    todo_wine ok(hr == S_OK, "Got hr %#x.\n", hr);
    hr = IMediaControl_GetState(control, 1000, &state);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    hr = IMediaControl_Run(control);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ret = check_ec_complete(eventsrc, 0);
    ok(!ret, "Got unexpected EC_COMPLETE.\n");

    hr = IPin_EndOfStream(pin);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ret = check_ec_complete(eventsrc, 0);
    ok(!ret, "Got unexpected EC_COMPLETE.\n");

    hr = IPin_BeginFlush(pin);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    hr = IPin_EndFlush(pin);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = send_frame(input);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    hr = IPin_EndOfStream(pin);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ret = check_ec_complete(eventsrc, 0);
    ok(!ret, "Got unexpected EC_COMPLETE.\n");

    hr = IMediaControl_Stop(control);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ret = check_ec_complete(eventsrc, 0);
    ok(!ret, "Got unexpected EC_COMPLETE.\n");

    IMediaEvent_Release(eventsrc);
}

static void test_connect_pin(void)
{
    ALLOCATOR_PROPERTIES req_props = {1, 4 * 44100, 1, 0}, ret_props;
    WAVEFORMATEX wfx =
    {
        .wFormatTag = WAVE_FORMAT_PCM,
        .nChannels = 2,
        .nSamplesPerSec = 44100,
        .nAvgBytesPerSec = 44100 * 4,
        .nBlockAlign = 4,
        .wBitsPerSample = 16,
    };
    AM_MEDIA_TYPE req_mt =
    {
        .majortype = MEDIATYPE_Audio,
        .subtype = MEDIASUBTYPE_PCM,
        .formattype = FORMAT_WaveFormatEx,
        .cbFormat = sizeof(wfx),
        .pbFormat = (BYTE *)&wfx,
    };
    IBaseFilter *filter = create_dsound_render();
    struct testfilter source;
    IMemAllocator *allocator;
    IMediaControl *control;
    IFilterGraph2 *graph;
    IMemInputPin *input;
    AM_MEDIA_TYPE mt;
    IPin *pin, *peer;
    HRESULT hr;
    ULONG ref;

    testfilter_init(&source);

    CoCreateInstance(&CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, &IID_IFilterGraph2, (void **)&graph);
    IFilterGraph2_AddFilter(graph, &source.filter.IBaseFilter_iface, L"source");
    IFilterGraph2_AddFilter(graph, filter, L"sink");
    IFilterGraph2_QueryInterface(graph, &IID_IMediaControl, (void **)&control);

    IBaseFilter_FindPin(filter, sink_id, &pin);

    peer = (IPin *)0xdeadbeef;
    hr = IPin_ConnectedTo(pin, &peer);
    ok(hr == VFW_E_NOT_CONNECTED, "Got hr %#x.\n", hr);
    ok(!peer, "Got peer %p.\n", peer);

    hr = IPin_ConnectionMediaType(pin, &mt);
    ok(hr == VFW_E_NOT_CONNECTED, "Got hr %#x.\n", hr);

    hr = IMediaControl_Pause(control);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    hr = IFilterGraph2_ConnectDirect(graph, &source.source.pin.IPin_iface, pin, &req_mt);
    ok(hr == VFW_E_NOT_STOPPED, "Got hr %#x.\n", hr);
    hr = IMediaControl_Stop(control);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IFilterGraph2_ConnectDirect(graph, &source.source.pin.IPin_iface, pin, &req_mt);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IPin_ConnectedTo(pin, &peer);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(peer == &source.source.pin.IPin_iface, "Got peer %p.\n", peer);
    IPin_Release(peer);

    hr = IPin_ConnectionMediaType(pin, &mt);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(compare_media_types(&mt, &req_mt), "Media types didn't match.\n");

    hr = IMediaControl_Pause(control);
    ok(hr == S_FALSE, "Got hr %#x.\n", hr);
    hr = IFilterGraph2_Disconnect(graph, pin);
    ok(hr == VFW_E_NOT_STOPPED, "Got hr %#x.\n", hr);
    hr = IMediaControl_Stop(control);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    IPin_QueryInterface(pin, &IID_IMemInputPin, (void **)&input);

    test_allocator(input);

    hr = CoCreateInstance(&CLSID_MemoryAllocator, NULL, CLSCTX_INPROC_SERVER,
            &IID_IMemAllocator, (void **)&allocator);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    hr = IMemAllocator_SetProperties(allocator, &req_props, &ret_props);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(!memcmp(&ret_props, &req_props, sizeof(req_props)), "Properties did not match.\n");
    hr = IMemInputPin_NotifyAllocator(input, allocator, TRUE);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    hr = IMemAllocator_Commit(allocator);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IMemInputPin_ReceiveCanBlock(input);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    test_filter_state(input, control);
    test_flushing(pin, input, control);
    test_eos(pin, input, control);

    hr = IFilterGraph2_Disconnect(graph, pin);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    hr = IFilterGraph2_Disconnect(graph, pin);
    ok(hr == S_FALSE, "Got hr %#x.\n", hr);
    ok(source.source.pin.peer == pin, "Got peer %p.\n", source.source.pin.peer);
    IFilterGraph2_Disconnect(graph, &source.source.pin.IPin_iface);

    peer = (IPin *)0xdeadbeef;
    hr = IPin_ConnectedTo(pin, &peer);
    ok(hr == VFW_E_NOT_CONNECTED, "Got hr %#x.\n", hr);
    ok(!peer, "Got peer %p.\n", peer);

    hr = IPin_ConnectionMediaType(pin, &mt);
    ok(hr == VFW_E_NOT_CONNECTED, "Got hr %#x.\n", hr);

    ref = IMemAllocator_Release(allocator);
    ok(!ref, "Got outstanding refcount %d.\n", ref);
    IMemInputPin_Release(input);
    IPin_Release(pin);
    IMediaControl_Release(control);
    ref = IFilterGraph2_Release(graph);
    ok(!ref, "Got outstanding refcount %d.\n", ref);
    ref = IBaseFilter_Release(filter);
    ok(!ref, "Got outstanding refcount %d.\n", ref);
    ref = IBaseFilter_Release(&source.filter.IBaseFilter_iface);
    ok(!ref, "Got outstanding refcount %d.\n", ref);
}

static void test_unconnected_filter_state(void)
{
    IBaseFilter *filter = create_dsound_render();
    FILTER_STATE state;
    HRESULT hr;
    ULONG ref;

    hr = IBaseFilter_GetState(filter, 0, &state);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(state == State_Stopped, "Got state %u.\n", state);

    hr = IBaseFilter_Pause(filter);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IBaseFilter_GetState(filter, 0, &state);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(state == State_Paused, "Got state %u.\n", state);

    hr = IBaseFilter_Run(filter, 0);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IBaseFilter_GetState(filter, 0, &state);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(state == State_Running, "Got state %u.\n", state);

    hr = IBaseFilter_Pause(filter);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IBaseFilter_GetState(filter, 0, &state);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(state == State_Paused, "Got state %u.\n", state);

    hr = IBaseFilter_Stop(filter);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IBaseFilter_GetState(filter, 0, &state);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(state == State_Stopped, "Got state %u.\n", state);

    hr = IBaseFilter_Run(filter, 0);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IBaseFilter_GetState(filter, 0, &state);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(state == State_Running, "Got state %u.\n", state);

    hr = IBaseFilter_Stop(filter);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IBaseFilter_GetState(filter, 0, &state);
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    ok(state == State_Stopped, "Got state %u.\n", state);

    ref = IBaseFilter_Release(filter);
    ok(!ref, "Got outstanding refcount %d.\n", ref);
}

static HRESULT does_dsound_support_format(WAVEFORMATEX *format)
{
    const DSBUFFERDESC desc =
    {
        .dwSize = sizeof(DSBUFFERDESC),
        .dwBufferBytes = format->nAvgBytesPerSec,
        .lpwfxFormat = format,
    };
    IDirectSoundBuffer *buffer;
    IDirectSound *dsound;
    HRESULT hr;

    hr = DirectSoundCreate(NULL, &dsound, NULL);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IDirectSound_CreateSoundBuffer(dsound, &desc, &buffer, NULL);
    if (hr == S_OK)
        IDirectSoundBuffer_Release(buffer);
    IDirectSound_Release(dsound);

    return hr == S_OK ? S_OK : S_FALSE;
}

static void test_media_types(void)
{
    IBaseFilter *filter = create_dsound_render();
    AM_MEDIA_TYPE *mt, req_mt = {{0}};
    IEnumMediaTypes *enummt;
    WAVEFORMATEX wfx = {0};
    HRESULT hr, expect_hr;
    unsigned int i, j;
    WORD channels;
    ULONG ref;
    IPin *pin;

    static const DWORD sample_rates[] = {8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000, 96000};

    static const struct
    {
        WORD tag;
        WORD depth;
    } formats[] =
    {
        {WAVE_FORMAT_PCM, 8},
        {WAVE_FORMAT_PCM, 16},
        {WAVE_FORMAT_PCM, 24},
        {WAVE_FORMAT_PCM, 32},
        {WAVE_FORMAT_IEEE_FLOAT, 32},
        {WAVE_FORMAT_IEEE_FLOAT, 64},
    };

    IBaseFilter_FindPin(filter, sink_id, &pin);

    hr = IPin_EnumMediaTypes(pin, &enummt);
    ok(hr == S_OK, "Got hr %#x.\n", hr);

    hr = IEnumMediaTypes_Next(enummt, 1, &mt, NULL);
    todo_wine ok(hr == S_OK, "Got hr %#x.\n", hr);
    if (hr == S_OK)
    {
        ok(IsEqualGUID(&mt->majortype, &MEDIATYPE_Audio), "Got major type %s.\n", wine_dbgstr_guid(&mt->majortype));
        ok(IsEqualGUID(&mt->subtype, &GUID_NULL), "Got subtype %s.\n", wine_dbgstr_guid(&mt->subtype));
        ok(mt->bFixedSizeSamples == TRUE, "Got fixed size %d.\n", mt->bFixedSizeSamples);
        ok(!mt->bTemporalCompression, "Got temporal compression %d.\n", mt->bTemporalCompression);
        ok(mt->lSampleSize == 1, "Got sample size %u.\n", mt->lSampleSize);
        ok(IsEqualGUID(&mt->formattype, &GUID_NULL), "Got format type %s.\n", wine_dbgstr_guid(&mt->formattype));
        ok(!mt->pUnk, "Got pUnk %p.\n", mt->pUnk);
        ok(!mt->cbFormat, "Got format size %u.\n", mt->cbFormat);
        ok(!mt->pbFormat, "Got unexpected format block.\n");

        hr = IPin_QueryAccept(pin, mt);
        ok(hr == S_FALSE, "Got hr %#x.\n", hr);

        CoTaskMemFree(mt);
    }

    hr = IEnumMediaTypes_Next(enummt, 1, &mt, NULL);
    ok(hr == S_FALSE, "Got hr %#x.\n", hr);

    req_mt.majortype = MEDIATYPE_Audio;
    req_mt.formattype = FORMAT_WaveFormatEx;
    req_mt.cbFormat = sizeof(WAVEFORMATEX);
    req_mt.pbFormat = (BYTE *)&wfx;

    IEnumMediaTypes_Release(enummt);

    for (channels = 1; channels <= 2; ++channels)
    {
        wfx.nChannels = channels;

        for (i = 0; i < ARRAY_SIZE(formats); ++i)
        {
            wfx.wFormatTag = formats[i].tag;
            wfx.wBitsPerSample = formats[i].depth;
            wfx.nBlockAlign = wfx.nChannels * wfx.wBitsPerSample / 8;
            for (j = 0; j < ARRAY_SIZE(sample_rates); ++j)
            {
                wfx.nSamplesPerSec = sample_rates[j];
                wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

                expect_hr = does_dsound_support_format(&wfx);

                hr = IPin_QueryAccept(pin, &req_mt);
                ok(hr == expect_hr, "Expected hr %#x, got %#x, for %d channels, %d-bit %s, %d Hz.\n",
                        expect_hr, hr, channels, formats[i].depth,
                        formats[i].tag == WAVE_FORMAT_PCM ? "integer" : "float", sample_rates[j]);
            }
        }
    }

    IPin_Release(pin);
    ref = IBaseFilter_Release(filter);
    ok(!ref, "Got outstanding refcount %d.\n", ref);
}

START_TEST(dsoundrender)
{
    IBaseFilter *filter;
    HRESULT hr;

    CoInitialize(NULL);

    hr = CoCreateInstance(&CLSID_DSoundRender, NULL, CLSCTX_INPROC_SERVER,
            &IID_IBaseFilter, (void **)&filter);
    if (hr == VFW_E_NO_AUDIO_HARDWARE)
    {
        skip("No audio hardware.\n");
        CoUninitialize();
        return;
    }
    ok(hr == S_OK, "Got hr %#x.\n", hr);
    IBaseFilter_Release(filter);

    test_property_bag();
    test_interfaces();
    test_aggregation();
    test_enum_pins();
    test_find_pin();
    test_pin_info();
    test_basic_audio();
    test_enum_media_types();
    test_unconnected_filter_state();
    test_media_types();
    test_connect_pin();

    CoUninitialize();
}
