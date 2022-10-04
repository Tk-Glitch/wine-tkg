/*
 * Copyright 2017 Nikolay Sivov
 * Copyright 2022 Rémi Bernon for CodeWeavers
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

#include <stdarg.h>
#include <string.h>

#define COBJMACROS
#include "windef.h"
#include "winbase.h"

#include "control.h"
#include "d3d9types.h"
#include "dmoreg.h"
#include "mferror.h"
#include "mfidl.h"
#include "mftransform.h"
#include "propvarutil.h"
#include "uuids.h"
#include "wmcodecdsp.h"

#include "mf_test.h"

#include "wine/test.h"

#include "initguid.h"

DEFINE_GUID(DMOVideoFormat_RGB24,D3DFMT_R8G8B8,0x524f,0x11ce,0x9f,0x53,0x00,0x20,0xaf,0x0b,0xa7,0x70);
DEFINE_GUID(DMOVideoFormat_RGB32,D3DFMT_X8R8G8B8,0x524f,0x11ce,0x9f,0x53,0x00,0x20,0xaf,0x0b,0xa7,0x70);
DEFINE_GUID(DMOVideoFormat_RGB555,D3DFMT_X1R5G5B5,0x524f,0x11ce,0x9f,0x53,0x00,0x20,0xaf,0x0b,0xa7,0x70);
DEFINE_GUID(DMOVideoFormat_RGB565,D3DFMT_R5G6B5,0x524f,0x11ce,0x9f,0x53,0x00,0x20,0xaf,0x0b,0xa7,0x70);
DEFINE_GUID(DMOVideoFormat_RGB8,D3DFMT_P8,0x524f,0x11ce,0x9f,0x53,0x00,0x20,0xaf,0x0b,0xa7,0x70);
DEFINE_GUID(MFAudioFormat_RAW_AAC1,WAVE_FORMAT_RAW_AAC1,0x0000,0x0010,0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71);
DEFINE_GUID(MFVideoFormat_ABGR32,0x00000020,0x0000,0x0010,0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71);
DEFINE_GUID(MFVideoFormat_P208,0x38303250,0x0000,0x0010,0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71);

DEFINE_GUID(mft_output_sample_incomplete,0xffffff,0xffff,0xffff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff);

static void load_resource(const WCHAR *filename, const BYTE **data, DWORD *length)
{
    HRSRC resource = FindResourceW(NULL, filename, (const WCHAR *)RT_RCDATA);
    ok(resource != 0, "FindResourceW failed, error %lu\n", GetLastError());
    *data = LockResource(LoadResource(GetModuleHandleW(NULL), resource));
    *length = SizeofResource(GetModuleHandleW(NULL), resource);
}

#define EXPECT_REF(obj,ref) _expect_ref((IUnknown*)obj, ref, __LINE__)
static void _expect_ref(IUnknown* obj, ULONG expected_refcount, int line)
{
    ULONG refcount;
    IUnknown_AddRef(obj);
    refcount = IUnknown_Release(obj);
    ok_(__FILE__, line)(refcount == expected_refcount, "Unexpected refcount %ld, expected %ld.\n", refcount,
            expected_refcount);
}

#define check_interface(a, b, c) check_interface_(__LINE__, a, b, c)
static void check_interface_(unsigned int line, void *iface_ptr, REFIID iid, BOOL supported)
{
    IUnknown *iface = iface_ptr;
    HRESULT hr, expected_hr;
    IUnknown *unk;

    expected_hr = supported ? S_OK : E_NOINTERFACE;

    hr = IUnknown_QueryInterface(iface, iid, (void **)&unk);
    ok_(__FILE__, line)(hr == expected_hr, "Got hr %#lx, expected %#lx.\n", hr, expected_hr);
    if (SUCCEEDED(hr))
        IUnknown_Release(unk);
}

void check_attributes_(int line, IMFAttributes *attributes, const struct attribute_desc *desc, ULONG limit)
{
    char buffer[256], *buf = buffer;
    PROPVARIANT value;
    int i, j, ret;
    HRESULT hr;

    for (i = 0; i < limit && desc[i].key; ++i)
    {
        hr = IMFAttributes_GetItem(attributes, desc[i].key, &value);
        todo_wine_if(desc[i].todo)
        ok_(__FILE__, line)(hr == S_OK, "%s missing, hr %#lx\n", debugstr_a(desc[i].name), hr);
        if (hr != S_OK) continue;

        switch (value.vt)
        {
        default: sprintf(buffer, "??"); break;
        case VT_CLSID: sprintf(buffer, "%s", debugstr_guid(value.puuid)); break;
        case VT_UI4: sprintf(buffer, "%lu", value.ulVal); break;
        case VT_UI8:
            if (desc[i].ratio)
                sprintf(buffer, "%lu:%lu", value.uhVal.HighPart, value.uhVal.LowPart);
            else
                sprintf(buffer, "%I64u", value.uhVal.QuadPart);
            break;
        case VT_VECTOR | VT_UI1:
            buf += sprintf(buf, "size %lu, data {", value.caub.cElems);
            for (j = 0; j < 16 && j < value.caub.cElems; ++j)
                buf += sprintf(buf, "0x%02x,", value.caub.pElems[j]);
            if (value.caub.cElems > 16)
                buf += sprintf(buf, "...}");
            else
                buf += sprintf(buf - (j ? 1 : 0), "}");
            break;
        }

        ret = PropVariantCompareEx(&value, &desc[i].value, 0, 0);
        todo_wine_if(desc[i].todo_value)
        ok_(__FILE__, line)(ret == 0, "%s mismatch, type %u, value %s\n",
                debugstr_a(desc[i].name), value.vt, buffer);
    }
}

struct transform_info
{
    const WCHAR *name;
    const GUID *major_type;
    struct
    {
        const GUID *subtype;
        BOOL broken;
    } inputs[32], input_end, outputs[32], output_end;
};

static BOOL check_mft_enum(GUID category, MFT_REGISTER_TYPE_INFO *input_type,
        MFT_REGISTER_TYPE_INFO *output_type, const GUID *expect_class_id)
{
    GUID *class_ids = NULL;
    UINT32 count = 0, i;
    HRESULT hr;

    hr = MFTEnum(category, 0, input_type, output_type, NULL, &class_ids, &count);
    if (FAILED(hr) || count == 0)
    {
        todo_wine
        win_skip("MFTEnum returned %#lx, count %u, skipping tests.\n", hr, count);
        return FALSE;
    }

    ok(hr == S_OK, "MFTEnum returned %#lx\n", hr);
    for (i = 0; i < count; ++i)
        if (IsEqualGUID(expect_class_id, class_ids + i))
            break;
    ok(i < count, "Failed to find transform.\n");
    CoTaskMemFree(class_ids);

    return i < count;
}

static void check_mft_get_info(const GUID *class_id, const struct transform_info *expect)
{
    MFT_REGISTER_TYPE_INFO *input_types = NULL, *output_types = NULL;
    UINT32 input_count = 0, output_count = 0, i;
    WCHAR *name;
    HRESULT hr;

    hr = MFTGetInfo(*class_id, &name, &input_types, &input_count, &output_types, &output_count, NULL);
    ok(hr == S_OK, "MFTEnum returned %#lx\n", hr);
    ok(!wcscmp(name, expect->name), "got name %s\n", debugstr_w(name));

    for (i = 0; i < input_count && expect->inputs[i].subtype; ++i)
    {
        ok(IsEqualGUID(&input_types[i].guidMajorType, expect->major_type),
                "got input[%u] major %s\n", i, debugstr_guid(&input_types[i].guidMajorType));
        ok(IsEqualGUID(&input_types[i].guidSubtype, expect->inputs[i].subtype),
                "got input[%u] subtype %s\n", i, debugstr_guid(&input_types[i].guidSubtype));
    }
    for (; expect->inputs[i].subtype; ++i)
        ok(broken(expect->inputs[i].broken), "missing input[%u] subtype %s\n",
                i, debugstr_guid(expect->inputs[i].subtype));
    for (; i < input_count; ++i)
        ok(0, "extra input[%u] subtype %s\n", i, debugstr_guid(&input_types[i].guidSubtype));

    for (i = 0; expect->outputs[i].subtype; ++i)
    {
        ok(IsEqualGUID(&output_types[i].guidMajorType, expect->major_type),
                "got output[%u] major %s\n", i, debugstr_guid(&output_types[i].guidMajorType));
        ok(IsEqualGUID(&output_types[i].guidSubtype, expect->outputs[i].subtype),
                "got output[%u] subtype %s\n", i, debugstr_guid(&output_types[i].guidSubtype));
    }
    for (; expect->outputs[i].subtype; ++i)
        ok(0, "missing output[%u] subtype %s\n", i, debugstr_guid(expect->outputs[i].subtype));
    for (; i < output_count; ++i)
        ok(0, "extra output[%u] subtype %s\n", i, debugstr_guid(&output_types[i].guidSubtype));

    CoTaskMemFree(output_types);
    CoTaskMemFree(input_types);
    CoTaskMemFree(name);
}

static void check_dmo_get_info(const GUID *class_id, const struct transform_info *expect)
{
    DWORD input_count = 0, output_count = 0;
    DMO_PARTIAL_MEDIATYPE output[32] = {{{0}}};
    DMO_PARTIAL_MEDIATYPE input[32] = {{{0}}};
    WCHAR name[80];
    HRESULT hr;
    int i;

    hr = DMOGetName(class_id, name);
    ok(hr == S_OK, "DMOGetName returned %#lx\n", hr);
    ok(!wcscmp(name, expect->name), "got name %s\n", debugstr_w(name));

    hr = DMOGetTypes(class_id, ARRAY_SIZE(input), &input_count, input,
            ARRAY_SIZE(output), &output_count, output);
    ok(hr == S_OK, "DMOGetTypes returned %#lx\n", hr);

    for (i = 0; i < input_count && expect->inputs[i].subtype; ++i)
    {
        ok(IsEqualGUID(&input[i].type, expect->major_type),
                "got input[%u] major %s\n", i, debugstr_guid(&input[i].type));
        ok(IsEqualGUID(&input[i].subtype, expect->inputs[i].subtype),
                "got input[%u] subtype %s\n", i, debugstr_guid(&input[i].subtype));
    }
    for (; expect->inputs[i].subtype; ++i)
        ok(0, "missing input[%u] subtype %s\n", i, debugstr_guid(expect->inputs[i].subtype));
    for (; i < input_count; ++i)
        ok(0, "extra input[%u] subtype %s\n", i, debugstr_guid(&input[i].subtype));

    for (i = 0; expect->outputs[i].subtype; ++i)
    {
        ok(IsEqualGUID(&output[i].type, expect->major_type),
                "got output[%u] major %s\n", i, debugstr_guid(&output[i].type));
        ok(IsEqualGUID(&output[i].subtype, expect->outputs[i].subtype),
                "got output[%u] subtype %s\n", i, debugstr_guid(&output[i].subtype));
    }
    for (; expect->outputs[i].subtype; ++i)
        ok(0, "missing output[%u] subtype %s\n", i, debugstr_guid(expect->outputs[i].subtype));
    for (; i < output_count; ++i)
        ok(0, "extra output[%u] subtype %s\n", i, debugstr_guid(&output[i].subtype));
}

void init_media_type(IMFMediaType *mediatype, const struct attribute_desc *desc, ULONG limit)
{
    HRESULT hr;
    ULONG i;

    hr = IMFMediaType_DeleteAllItems(mediatype);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);

    for (i = 0; i < limit && desc[i].key; ++i)
    {
        hr = IMFMediaType_SetItem(mediatype, desc[i].key, &desc[i].value);
        ok(hr == S_OK, "SetItem %s returned %#lx\n", debugstr_a(desc[i].name), hr);
    }
}

#define check_mft_set_input_type_required(a, b) check_mft_set_input_type_required_(__LINE__, a, b)
static void check_mft_set_input_type_required_(int line, IMFTransform *transform, const struct attribute_desc *attributes)
{
    const struct attribute_desc *attr;
    IMFMediaType *media_type;
    HRESULT hr;
    ULONG ref;

    hr = MFCreateMediaType(&media_type);
    ok_(__FILE__, line)(hr == S_OK, "MFCreateMediaType returned hr %#lx.\n", hr);
    init_media_type(media_type, attributes, -1);

    for (attr = attributes; attr && attr->key; attr++)
    {
        winetest_push_context("%s", debugstr_a(attr->name));
        hr = IMFMediaType_DeleteItem(media_type, attr->key);
        ok_(__FILE__, line)(hr == S_OK, "DeleteItem returned %#lx\n", hr);
        hr = IMFTransform_SetInputType(transform, 0, media_type, MFT_SET_TYPE_TEST_ONLY);
        ok_(__FILE__, line)(FAILED(hr) == attr->required, "SetInputType returned %#lx.\n", hr);
        hr = IMFMediaType_SetItem(media_type, attr->key, &attr->value);
        ok_(__FILE__, line)(hr == S_OK, "SetItem returned %#lx\n", hr);
        winetest_pop_context();
    }

    hr = IMFTransform_SetInputType(transform, 0, media_type, MFT_SET_TYPE_TEST_ONLY);
    ok_(__FILE__, line)(hr == S_OK, "SetInputType returned %#lx.\n", hr);
    ref = IMFMediaType_Release(media_type);
    ok_(__FILE__, line)(!ref, "Release returned %lu\n", ref);
}

#define check_mft_set_output_type_required(a, b) check_mft_set_output_type_required_(__LINE__, a, b)
static void check_mft_set_output_type_required_(int line, IMFTransform *transform, const struct attribute_desc *attributes)
{
    const struct attribute_desc *attr;
    IMFMediaType *media_type;
    HRESULT hr;
    ULONG ref;

    hr = MFCreateMediaType(&media_type);
    ok_(__FILE__, line)(hr == S_OK, "MFCreateMediaType returned hr %#lx.\n", hr);
    init_media_type(media_type, attributes, -1);

    for (attr = attributes; attr && attr->key; attr++)
    {
        winetest_push_context("%s", debugstr_a(attr->name));
        hr = IMFMediaType_DeleteItem(media_type, attr->key);
        ok_(__FILE__, line)(hr == S_OK, "DeleteItem returned %#lx\n", hr);
        hr = IMFTransform_SetOutputType(transform, 0, media_type, MFT_SET_TYPE_TEST_ONLY);
        ok_(__FILE__, line)(FAILED(hr) == attr->required, "SetOutputType returned %#lx.\n", hr);
        hr = IMFMediaType_SetItem(media_type, attr->key, &attr->value);
        ok_(__FILE__, line)(hr == S_OK, "SetItem returned %#lx\n", hr);
        winetest_pop_context();
    }

    hr = IMFTransform_SetOutputType(transform, 0, media_type, MFT_SET_TYPE_TEST_ONLY);
    ok_(__FILE__, line)(hr == S_OK, "SetOutputType returned %#lx.\n", hr);
    ref = IMFMediaType_Release(media_type);
    ok_(__FILE__, line)(!ref, "Release returned %lu\n", ref);
}

#define check_mft_process_output(a, b, c) check_mft_process_output_(__LINE__, a, b, c)
static HRESULT check_mft_process_output_(int line, IMFTransform *transform, IMFSample *output_sample, DWORD *output_status)
{
    static const DWORD expect_flags = MFT_OUTPUT_DATA_BUFFER_INCOMPLETE | MFT_OUTPUT_DATA_BUFFER_FORMAT_CHANGE
            | MFT_OUTPUT_DATA_BUFFER_STREAM_END | MFT_OUTPUT_DATA_BUFFER_NO_SAMPLE;
    MFT_OUTPUT_DATA_BUFFER output[2];
    HRESULT hr, ret;
    DWORD status;

    status = 0;
    memset(&output, 0, sizeof(output));
    output[0].pSample = output_sample;
    output[0].dwStreamID = 0;
    ret = IMFTransform_ProcessOutput(transform, 0, 1, output, &status);
    ok_(__FILE__, line)(output[0].dwStreamID == 0, "got dwStreamID %#lx\n", output[0].dwStreamID);
    ok_(__FILE__, line)(output[0].pEvents == NULL, "got pEvents %p\n", output[0].pEvents);
    ok_(__FILE__, line)(output[0].pSample == output_sample, "got pSample %p\n", output[0].pSample);
    ok_(__FILE__, line)((output[0].dwStatus & ~expect_flags) == 0
            || broken((output[0].dwStatus & ~expect_flags) == 6) /* Win7 */
            || broken((output[0].dwStatus & ~expect_flags) == 7) /* Win7 */,
            "got dwStatus %#lx\n", output[0].dwStatus);
    *output_status = output[0].dwStatus & expect_flags;

    if (!output_sample)
        ok_(__FILE__, line)(status == 0, "got status %#lx\n", status);
    else if (ret == MF_E_TRANSFORM_STREAM_CHANGE)
        ok_(__FILE__, line)(status == MFT_PROCESS_OUTPUT_STATUS_NEW_STREAMS,
                "got status %#lx\n", status);
    else
    {
        if (*output_status & MFT_OUTPUT_DATA_BUFFER_INCOMPLETE)
        {
            hr = IMFSample_SetUINT32(output_sample, &mft_output_sample_incomplete, 1);
            ok_(__FILE__, line)(hr == S_OK, "SetUINT32 returned %#lx\n", hr);
        }
        else
        {
            hr = IMFSample_DeleteItem(output_sample, &mft_output_sample_incomplete);
            ok_(__FILE__, line)(hr == S_OK, "DeleteItem returned %#lx\n", hr);
        }
        ok_(__FILE__, line)(status == 0, "got status %#lx\n", status);
    }

    return ret;
}

typedef DWORD (*compare_cb)(const BYTE *data, DWORD *length, const RECT *rect, const BYTE *expect);

static DWORD compare_nv12(const BYTE *data, DWORD *length, const RECT *rect, const BYTE *expect)
{
    DWORD x, y, size, diff = 0, width = (rect->right + 0xf) & ~0xf, height = (rect->bottom + 0xf) & ~0xf;

    /* skip BMP header and RGB data from the dump */
    size = *(DWORD *)(expect + 2);
    *length = *length + size;
    expect = expect + size;

    for (y = 0; y < height; y++, data += width, expect += width)
    {
        if (y < rect->top || y >= rect->bottom) continue;
        for (x = 0; x < width; x++)
        {
            if (x < rect->left || x >= rect->right) continue;
            diff += abs((int)expect[x] - (int)data[x]);
        }
    }

    for (y = 0; y < height; y += 2, data += width, expect += width)
    {
        if (y < rect->top || y >= rect->bottom) continue;
        for (x = 0; x < width; x += 2)
        {
            if (x < rect->left || x >= rect->right) continue;
            diff += abs((int)expect[x + 0] - (int)data[x + 0]);
            diff += abs((int)expect[x + 1] - (int)data[x + 1]);
        }
    }

    size = (rect->right - rect->left) * (rect->bottom - rect->top) * 3 / 2;
    return diff * 100 / 256 / size;
}

static DWORD compare_i420(const BYTE *data, DWORD *length, const RECT *rect, const BYTE *expect)
{
    DWORD i, x, y, size, diff = 0, width = (rect->right + 0xf) & ~0xf, height = (rect->bottom + 0xf) & ~0xf;

    /* skip BMP header and RGB data from the dump */
    size = *(DWORD *)(expect + 2);
    *length = *length + size;
    expect = expect + size;

    for (y = 0; y < height; y++, data += width, expect += width)
    {
        if (y < rect->top || y >= rect->bottom) continue;
        for (x = 0; x < width; x++)
        {
            if (x < rect->left || x >= rect->right) continue;
            diff += abs((int)expect[x] - (int)data[x]);
        }
    }

    for (i = 0; i < 2; ++i) for (y = 0; y < height; y += 2, data += width / 2, expect += width / 2)
    {
        if (y < rect->top || y >= rect->bottom) continue;
        for (x = 0; x < width; x += 2)
        {
            if (x < rect->left || x >= rect->right) continue;
            diff += abs((int)expect[x / 2] - (int)data[x / 2]);
        }
    }

    size = (rect->right - rect->left) * (rect->bottom - rect->top) * 3 / 2;
    return diff * 100 / 256 / size;
}

static DWORD compare_rgb32(const BYTE *data, DWORD *length, const RECT *rect, const BYTE *expect)
{
    DWORD x, y, size, diff = 0, width = (rect->right + 0xf) & ~0xf, height = (rect->bottom + 0xf) & ~0xf;

    /* skip BMP header from the dump */
    size = *(DWORD *)(expect + 2 + 2 * sizeof(DWORD));
    *length = *length + size;
    expect = expect + size;

    for (y = 0; y < height; y++, data += width * 4, expect += width * 4)
    {
        if (y < rect->top || y >= rect->bottom) continue;
        for (x = 0; x < width; x++)
        {
            if (x < rect->left || x >= rect->right) continue;
            diff += abs((int)expect[4 * x + 0] - (int)data[4 * x + 0]);
            diff += abs((int)expect[4 * x + 1] - (int)data[4 * x + 1]);
            diff += abs((int)expect[4 * x + 2] - (int)data[4 * x + 2]);
        }
    }

    size = (rect->right - rect->left) * (rect->bottom - rect->top) * 3;
    return diff * 100 / 256 / size;
}

static DWORD compare_pcm16(const BYTE *data, DWORD *length, const RECT *rect, const BYTE *expect)
{
    const INT16 *data_pcm = (INT16 *)data, *expect_pcm = (INT16 *)expect;
    DWORD i, size = *length / 2, diff = 0;

    for (i = 0; i < size; i++)
        diff += abs((int)*expect_pcm++ - (int)*data_pcm++);

    return diff * 100 / 65536 / size;
}

static DWORD compare_bytes(const BYTE *data, DWORD *length, const RECT *rect, const BYTE *expect)
{
    DWORD i, size = *length, diff = 0;

    for (i = 0; i < size; i++)
        diff += abs((int)*expect++ - (int)*data++);

    return diff * 100 / 256 / size;
}

typedef void (*dump_cb)(const BYTE *data, DWORD length, const RECT *rect, HANDLE output);

static void dump_rgb32(const BYTE *data, DWORD length, const RECT *rect, HANDLE output)
{
    DWORD width = (rect->right + 0xf) & ~0xf, height = (rect->bottom + 0xf) & ~0xf;
    static const char magic[2] = "BM";
    struct
    {
        DWORD length;
        DWORD reserved;
        DWORD offset;
        BITMAPINFOHEADER biHeader;
    } header =
    {
        .length = length + sizeof(header) + 2, .offset = sizeof(header) + 2,
        .biHeader =
        {
            .biSize = sizeof(BITMAPINFOHEADER), .biWidth = width, .biHeight = height, .biPlanes = 1,
            .biBitCount = 32, .biCompression = BI_RGB, .biSizeImage = width * height * 4,
        },
    };
    DWORD written;
    BOOL ret;

    ret = WriteFile(output, magic, sizeof(magic), &written, NULL);
    ok(ret, "WriteFile failed, error %lu\n", GetLastError());
    ok(written == sizeof(magic), "written %lu bytes\n", written);
    ret = WriteFile(output, &header, sizeof(header), &written, NULL);
    ok(ret, "WriteFile failed, error %lu\n", GetLastError());
    ok(written == sizeof(header), "written %lu bytes\n", written);
    ret = WriteFile(output, data, length, &written, NULL);
    ok(ret, "WriteFile failed, error %lu\n", GetLastError());
    ok(written == length, "written %lu bytes\n", written);
}

static void dump_nv12(const BYTE *data, DWORD length, const RECT *rect, HANDLE output)
{
    DWORD written, x, y, width = (rect->right + 0xf) & ~0xf, height = (rect->bottom + 0xf) & ~0xf;
    BYTE *rgb32_data = malloc(width * height * 4), *rgb32 = rgb32_data;
    BOOL ret;

    for (y = 0; y < height; y++) for (x = 0; x < width; x++)
    {
        *rgb32++ = data[width * y + x];
        *rgb32++ = data[width * height + width * (y / 2) + (x & ~1) + 0];
        *rgb32++ = data[width * height + width * (y / 2) + (x & ~1) + 1];
        *rgb32++ = 0xff;
    }

    dump_rgb32(rgb32_data, width * height * 4, rect, output);
    free(rgb32_data);

    ret = WriteFile(output, data, length, &written, NULL);
    ok(ret, "WriteFile failed, error %lu\n", GetLastError());
    ok(written == length, "written %lu bytes\n", written);
}

static void dump_i420(const BYTE *data, DWORD length, const RECT *rect, HANDLE output)
{
    DWORD written, x, y, width = (rect->right + 0xf) & ~0xf, height = (rect->bottom + 0xf) & ~0xf;
    BYTE *rgb32_data = malloc(width * height * 4), *rgb32 = rgb32_data;
    BOOL ret;

    for (y = 0; y < height; y++) for (x = 0; x < width; x++)
    {
        *rgb32++ = data[width * y + x];
        *rgb32++ = data[width * height + (width / 2) * (y / 2) + x / 2];
        *rgb32++ = data[width * height + (width / 2) * (y / 2) + (width / 2) * (height / 2) + x / 2];
        *rgb32++ = 0xff;
    }

    dump_rgb32(rgb32_data, width * height * 4, rect, output);
    free(rgb32_data);

    ret = WriteFile(output, data, length, &written, NULL);
    ok(ret, "WriteFile failed, error %lu\n", GetLastError());
    ok(written == length, "written %lu bytes\n", written);
}

struct buffer_desc
{
    DWORD length;
    BOOL todo_length;
    compare_cb compare;
    dump_cb dump;
    RECT rect;
};

struct sample_desc
{
    const struct attribute_desc *attributes;
    LONGLONG sample_time;
    LONGLONG sample_duration;
    DWORD buffer_count;
    const struct buffer_desc *buffers;
    DWORD repeat_count;
    BOOL todo_length;
    LONGLONG todo_time;
};

typedef void (*enum_mf_media_buffers_cb)(IMFMediaBuffer *buffer, const struct buffer_desc *desc, void *context);
static void enum_mf_media_buffers(IMFSample *sample, const struct sample_desc *sample_desc,
        enum_mf_media_buffers_cb callback, void *context)
{
    IMFMediaBuffer *buffer;
    HRESULT hr;
    DWORD i;

    for (i = 0; SUCCEEDED(hr = IMFSample_GetBufferByIndex(sample, i, &buffer)); i++)
    {
        winetest_push_context("buffer %lu", i);
        ok(hr == S_OK, "GetBufferByIndex returned %#lx\n", hr);
        ok(i < sample_desc->buffer_count, "got unexpected buffer\n");

        callback(buffer, sample_desc->buffers + i, context);

        IMFMediaBuffer_Release(buffer);
        winetest_pop_context();
    }
    ok(hr == E_INVALIDARG, "GetBufferByIndex returned %#lx\n", hr);
}

struct enum_mf_sample_state
{
    const struct sample_desc *next_sample;
    struct sample_desc sample;
};

typedef void (*enum_mf_sample_cb)(IMFSample *sample, const struct sample_desc *sample_desc, void *context);
static void enum_mf_samples(IMFCollection *samples, const struct sample_desc *collection_desc,
        enum_mf_sample_cb callback, void *context)
{
    struct enum_mf_sample_state state = {.next_sample = collection_desc};
    IMFSample *sample;
    HRESULT hr;
    DWORD i;

    for (i = 0; SUCCEEDED(hr = IMFCollection_GetElement(samples, i, (IUnknown **)&sample)); i++)
    {
        winetest_push_context("sample %lu", i);
        ok(hr == S_OK, "GetElement returned %#lx\n", hr);

        state.sample.sample_time += state.sample.sample_duration;
        if (!state.sample.repeat_count--)
            state.sample = *state.next_sample++;

        callback(sample, &state.sample, context);

        IMFSample_Release(sample);
        winetest_pop_context();
    }
    ok(hr == E_INVALIDARG, "GetElement returned %#lx\n", hr);
}

static void dump_mf_media_buffer(IMFMediaBuffer *buffer, const struct buffer_desc *buffer_desc, HANDLE output)
{
    DWORD length, written;
    HRESULT hr;
    BYTE *data;
    BOOL ret;

    hr = IMFMediaBuffer_Lock(buffer, &data, NULL, &length);
    ok(hr == S_OK, "Lock returned %#lx\n", hr);

    if (buffer_desc->dump)
        buffer_desc->dump(data, length, &buffer_desc->rect, output);
    else
    {
        ret = WriteFile(output, data, length, &written, NULL);
        ok(ret, "WriteFile failed, error %lu\n", GetLastError());
        ok(written == length, "written %lu bytes\n", written);
    }

    hr = IMFMediaBuffer_Unlock(buffer);
    ok(hr == S_OK, "Unlock returned %#lx\n", hr);
}

static void dump_mf_sample(IMFSample *sample, const struct sample_desc *sample_desc, HANDLE output)
{
    enum_mf_media_buffers(sample, sample_desc, dump_mf_media_buffer, output);
}

static void dump_mf_sample_collection(IMFCollection *samples, const struct sample_desc *collection_desc,
        const WCHAR *output_filename)
{
    WCHAR path[MAX_PATH];
    HANDLE output;

    GetTempPathW(ARRAY_SIZE(path), path);
    lstrcatW(path, output_filename);

    output = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, 0);
    ok(output != INVALID_HANDLE_VALUE, "CreateFileW failed, error %lu\n", GetLastError());

    enum_mf_samples(samples, collection_desc, dump_mf_sample, output);

    trace("created %s\n", debugstr_w(path));
    CloseHandle(output);
}

#define check_mf_media_buffer(a, b, c) check_mf_media_buffer_(__LINE__, a, b, c)
static DWORD check_mf_media_buffer_(int line, IMFMediaBuffer *buffer, const struct buffer_desc *expect,
        const BYTE **expect_data, DWORD *expect_data_len)
{
    DWORD length, diff = 0;
    HRESULT hr;
    BYTE *data;

    hr = IMFMediaBuffer_Lock(buffer, &data, NULL, &length);
    ok_(__FILE__, line)(hr == S_OK, "Lock returned %#lx\n", hr);
    todo_wine_if(expect->todo_length)
    ok_(__FILE__, line)(length == expect->length, "got length %#lx\n", length);

    if (*expect_data_len < length)
        todo_wine_if(expect->todo_length)
        ok_(__FILE__, line)(0, "missing %#lx bytes\n", length - *expect_data_len);
    else if (!expect->compare)
        diff = compare_bytes(data, &length, NULL, *expect_data);
    else
        diff = expect->compare(data, &length, &expect->rect, *expect_data);

    hr = IMFMediaBuffer_Unlock(buffer);
    ok_(__FILE__, line)(hr == S_OK, "Unlock returned %#lx\n", hr);

    *expect_data = *expect_data + min(length, *expect_data_len);
    *expect_data_len = *expect_data_len - min(length, *expect_data_len);

    return diff;
}

struct check_mf_sample_context
{
    DWORD total_length;
    const BYTE *data;
    DWORD data_len;
    DWORD diff;
    int line;
};

static void check_mf_sample_buffer(IMFMediaBuffer *buffer, const struct buffer_desc *expect, void *context)
{
    struct check_mf_sample_context *ctx = context;
    ctx->diff += check_mf_media_buffer_(ctx->line, buffer, expect, &ctx->data, &ctx->data_len);
    ctx->total_length += expect->length;
}

#define check_mf_sample(a, b, c, d) check_mf_sample_(__LINE__, a, b, c, d)
static DWORD check_mf_sample_(int line, IMFSample *sample, const struct sample_desc *expect,
        const BYTE **expect_data, DWORD *expect_data_len)
{
    struct check_mf_sample_context ctx = {.data = *expect_data, .data_len = *expect_data_len, .line = line};
    DWORD buffer_count, total_length, sample_flags;
    LONGLONG timestamp;
    HRESULT hr;

    if (expect->attributes)
        check_attributes_(line, (IMFAttributes *)sample, expect->attributes, -1);

    buffer_count = 0xdeadbeef;
    hr = IMFSample_GetBufferCount(sample, &buffer_count);
    ok_(__FILE__, line)(hr == S_OK, "GetBufferCount returned %#lx\n", hr);
    ok_(__FILE__, line)(buffer_count == expect->buffer_count,
            "got %lu buffers\n", buffer_count);

    sample_flags = 0xdeadbeef;
    hr = IMFSample_GetSampleFlags(sample, &sample_flags);
    ok_(__FILE__, line)(hr == S_OK, "GetSampleFlags returned %#lx\n", hr);
    ok_(__FILE__, line)(sample_flags == 0,
            "got sample flags %#lx\n", sample_flags);

    timestamp = 0xdeadbeef;
    hr = IMFSample_GetSampleTime(sample, &timestamp);
    ok_(__FILE__, line)(hr == S_OK, "GetSampleTime returned %#lx\n", hr);
    todo_wine_if(expect->todo_time && timestamp == expect->todo_time)
    ok_(__FILE__, line)(llabs(timestamp - expect->sample_time) <= 50,
            "got sample time %I64d\n", timestamp);

    timestamp = 0xdeadbeef;
    hr = IMFSample_GetSampleDuration(sample, &timestamp);
    ok_(__FILE__, line)(hr == S_OK, "GetSampleDuration returned %#lx\n", hr);
    todo_wine_if(expect->todo_length)
    ok_(__FILE__, line)(llabs(timestamp - expect->sample_duration) <= 1,
            "got sample duration %I64d\n", timestamp);

    enum_mf_media_buffers(sample, expect, check_mf_sample_buffer, &ctx);

    total_length = 0xdeadbeef;
    hr = IMFSample_GetTotalLength(sample, &total_length);
    ok_(__FILE__, line)(hr == S_OK, "GetTotalLength returned %#lx\n", hr);
    todo_wine_if(expect->todo_length)
    ok_(__FILE__, line)(total_length == ctx.total_length,
            "got total length %#lx\n", total_length);
    ok_(__FILE__, line)(*expect_data_len >= ctx.total_length,
            "missing %#lx data\n", ctx.total_length - *expect_data_len);

    *expect_data = *expect_data + min(total_length, *expect_data_len);
    *expect_data_len = *expect_data_len - min(total_length, *expect_data_len);

    return ctx.diff / buffer_count;
}

static void check_mf_sample_collection_enum(IMFSample *sample, const struct sample_desc *expect, void *context)
{
    struct check_mf_sample_context *ctx = context;
    ctx->diff += check_mf_sample_(ctx->line, sample, expect, &ctx->data, &ctx->data_len);
}

#define check_mf_sample_collection(a, b, c) check_mf_sample_collection_(__LINE__, a, b, c)
static DWORD check_mf_sample_collection_(int line, IMFCollection *samples,
        const struct sample_desc *expect_samples, const WCHAR *expect_data_filename)
{
    struct check_mf_sample_context ctx = {.line = line};
    DWORD count;
    HRESULT hr;

    load_resource(expect_data_filename, &ctx.data, &ctx.data_len);
    enum_mf_samples(samples, expect_samples, check_mf_sample_collection_enum, &ctx);

    dump_mf_sample_collection(samples, expect_samples, expect_data_filename);

    hr = IMFCollection_GetElementCount(samples, &count);
    ok_(__FILE__, line)(hr == S_OK, "GetElementCount returned %#lx\n", hr);

    return ctx.diff / count;
}

static HRESULT WINAPI test_unk_QueryInterface(IUnknown *iface, REFIID riid, void **obj)
{
    if (IsEqualIID(riid, &IID_IUnknown))
    {
        *obj = iface;
        IUnknown_AddRef(iface);
        return S_OK;
    }

    *obj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI test_unk_AddRef(IUnknown *iface)
{
    return 2;
}

static ULONG WINAPI test_unk_Release(IUnknown *iface)
{
    return 1;
}

static const IUnknownVtbl test_unk_vtbl =
{
    test_unk_QueryInterface,
    test_unk_AddRef,
    test_unk_Release,
};

static BOOL is_supported_video_type(const GUID *guid)
{
    return IsEqualGUID(guid, &MFVideoFormat_L8)
            || IsEqualGUID(guid, &MFVideoFormat_L16)
            || IsEqualGUID(guid, &MFVideoFormat_D16)
            || IsEqualGUID(guid, &MFVideoFormat_IYUV)
            || IsEqualGUID(guid, &MFVideoFormat_YV12)
            || IsEqualGUID(guid, &MFVideoFormat_NV12)
            || IsEqualGUID(guid, &MFVideoFormat_NV21)
            || IsEqualGUID(guid, &MFVideoFormat_420O)
            || IsEqualGUID(guid, &MFVideoFormat_P010)
            || IsEqualGUID(guid, &MFVideoFormat_P016)
            || IsEqualGUID(guid, &MFVideoFormat_UYVY)
            || IsEqualGUID(guid, &MFVideoFormat_YUY2)
            || IsEqualGUID(guid, &MFVideoFormat_P208)
            || IsEqualGUID(guid, &MFVideoFormat_NV11)
            || IsEqualGUID(guid, &MFVideoFormat_AYUV)
            || IsEqualGUID(guid, &MFVideoFormat_ARGB32)
            || IsEqualGUID(guid, &MFVideoFormat_RGB32)
            || IsEqualGUID(guid, &MFVideoFormat_A2R10G10B10)
            || IsEqualGUID(guid, &MFVideoFormat_A16B16G16R16F)
            || IsEqualGUID(guid, &MFVideoFormat_RGB24)
            || IsEqualGUID(guid, &MFVideoFormat_I420)
            || IsEqualGUID(guid, &MFVideoFormat_YVYU)
            || IsEqualGUID(guid, &MFVideoFormat_RGB555)
            || IsEqualGUID(guid, &MFVideoFormat_RGB565)
            || IsEqualGUID(guid, &MFVideoFormat_RGB8)
            || IsEqualGUID(guid, &MFVideoFormat_Y216)
            || IsEqualGUID(guid, &MFVideoFormat_v410)
            || IsEqualGUID(guid, &MFVideoFormat_Y41P)
            || IsEqualGUID(guid, &MFVideoFormat_Y41T)
            || IsEqualGUID(guid, &MFVideoFormat_Y42T)
            || IsEqualGUID(guid, &MFVideoFormat_ABGR32);
}

static BOOL is_sample_copier_available_type(IMFMediaType *type)
{
    GUID major = { 0 };
    UINT32 count;
    HRESULT hr;

    hr = IMFMediaType_GetMajorType(type, &major);
    ok(hr == S_OK, "Failed to get major type, hr %#lx.\n", hr);

    hr = IMFMediaType_GetCount(type, &count);
    ok(hr == S_OK, "Failed to get attribute count, hr %#lx.\n", hr);
    ok(count == 1, "Unexpected attribute count %u.\n", count);

    return IsEqualGUID(&major, &MFMediaType_Video) || IsEqualGUID(&major, &MFMediaType_Audio);
}

static void test_sample_copier(void)
{
    DWORD input_count, output_count, output_status;
    IMFAttributes *attributes, *attributes2;
    DWORD in_min, in_max, out_min, out_max;
    IMFMediaType *mediatype, *mediatype2;
    MFT_OUTPUT_STREAM_INFO output_info;
    IMFSample *sample, *client_sample;
    MFT_INPUT_STREAM_INFO input_info;
    IMFMediaBuffer *media_buffer;
    IMFTransform *copier;
    UINT32 value, count;
    DWORD flags;
    HRESULT hr;
    LONG ref;

    if (!pMFCreateSampleCopierMFT)
    {
        win_skip("MFCreateSampleCopierMFT() is not available.\n");
        return;
    }

    winetest_push_context("copier");

    hr = pMFCreateSampleCopierMFT(&copier);
    ok(hr == S_OK, "Failed to create sample copier, hr %#lx.\n", hr);

    hr = IMFTransform_GetAttributes(copier, &attributes);
    ok(hr == S_OK, "Failed to get transform attributes, hr %#lx.\n", hr);
    hr = IMFTransform_GetAttributes(copier, &attributes2);
    ok(hr == S_OK, "Failed to get transform attributes, hr %#lx.\n", hr);
    ok(attributes == attributes2, "Unexpected instance.\n");
    IMFAttributes_Release(attributes2);
    hr = IMFAttributes_GetCount(attributes, &count);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
    ok(count == 1, "Unexpected attribute count %u.\n", count);
    hr = IMFAttributes_GetUINT32(attributes, &MFT_SUPPORT_DYNAMIC_FORMAT_CHANGE, &value);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
    ok(!!value, "Unexpected value %u.\n", value);
    ref = IMFAttributes_Release(attributes);
    ok(ref == 1, "Release returned %ld\n", ref);

    hr = IMFTransform_GetInputStreamAttributes(copier, 0, &attributes);
    ok(hr == E_NOTIMPL, "Unexpected hr %#lx.\n", hr);

    hr = IMFTransform_GetInputStreamAttributes(copier, 1, &attributes);
    ok(hr == E_NOTIMPL, "Unexpected hr %#lx.\n", hr);

    hr = IMFTransform_GetOutputStreamAttributes(copier, 0, &attributes);
    ok(hr == E_NOTIMPL, "Unexpected hr %#lx.\n", hr);

    hr = IMFTransform_GetOutputStreamAttributes(copier, 1, &attributes);
    ok(hr == E_NOTIMPL, "Unexpected hr %#lx.\n", hr);

    hr = IMFTransform_SetOutputBounds(copier, 0, 0);
    ok(hr == E_NOTIMPL, "Unexpected hr %#lx.\n", hr);

    /* No dynamic streams. */
    input_count = output_count = 0;
    hr = IMFTransform_GetStreamCount(copier, &input_count, &output_count);
    ok(hr == S_OK, "Failed to get stream count, hr %#lx.\n", hr);
    ok(input_count == 1 && output_count == 1, "Unexpected streams count.\n");

    hr = IMFTransform_GetStreamLimits(copier, &in_min, &in_max, &out_min, &out_max);
    ok(hr == S_OK, "Failed to get stream limits, hr %#lx.\n", hr);
    ok(in_min == in_max && in_min == 1 && out_min == out_max && out_min == 1, "Unexpected stream limits.\n");

    hr = IMFTransform_GetStreamIDs(copier, 1, &input_count, 1, &output_count);
    ok(hr == E_NOTIMPL, "Unexpected hr %#lx.\n", hr);

    hr = IMFTransform_DeleteInputStream(copier, 0);
    ok(hr == E_NOTIMPL, "Unexpected hr %#lx.\n", hr);

    /* Available types. */
    hr = IMFTransform_GetInputAvailableType(copier, 0, 0, &mediatype);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
    ok(is_sample_copier_available_type(mediatype), "Unexpected type.\n");
    IMFMediaType_Release(mediatype);

    hr = IMFTransform_GetInputAvailableType(copier, 0, 1, &mediatype);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
    ok(is_sample_copier_available_type(mediatype), "Unexpected type.\n");
    IMFMediaType_Release(mediatype);

    hr = IMFTransform_GetInputAvailableType(copier, 0, 2, &mediatype);
    ok(hr == MF_E_NO_MORE_TYPES, "Unexpected hr %#lx.\n", hr);

    hr = IMFTransform_GetInputAvailableType(copier, 1, 0, &mediatype);
    ok(hr == MF_E_INVALIDSTREAMNUMBER, "Unexpected hr %#lx.\n", hr);

    hr = IMFTransform_GetOutputAvailableType(copier, 0, 0, &mediatype);
    ok(hr == MF_E_NO_MORE_TYPES, "Unexpected hr %#lx.\n", hr);

    hr = IMFTransform_GetOutputAvailableType(copier, 1, 0, &mediatype);
    ok(hr == MF_E_INVALIDSTREAMNUMBER, "Unexpected hr %#lx.\n", hr);

    hr = IMFTransform_GetInputCurrentType(copier, 0, &mediatype);
    ok(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "Unexpected hr %#lx.\n", hr);

    hr = IMFTransform_GetInputCurrentType(copier, 1, &mediatype);
    ok(hr == MF_E_INVALIDSTREAMNUMBER, "Unexpected hr %#lx.\n", hr);

    hr = IMFTransform_GetOutputCurrentType(copier, 0, &mediatype);
    ok(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "Unexpected hr %#lx.\n", hr);

    hr = IMFTransform_GetOutputCurrentType(copier, 1, &mediatype);
    ok(hr == MF_E_INVALIDSTREAMNUMBER, "Unexpected hr %#lx.\n", hr);

    hr = MFCreateSample(&sample);
    ok(hr == S_OK, "Failed to create a sample, hr %#lx.\n", hr);

    hr = IMFTransform_ProcessInput(copier, 0, sample, 0);
    ok(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "Unexpected hr %#lx.\n", hr);

    hr = MFCreateMediaType(&mediatype);
    ok(hr == S_OK, "Failed to create media type, hr %#lx.\n", hr);

    hr = IMFTransform_SetOutputType(copier, 0, mediatype, 0);
    ok(hr == MF_E_ATTRIBUTENOTFOUND, "Unexpected hr %#lx.\n", hr);

    hr = IMFMediaType_SetGUID(mediatype, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
    ok(hr == S_OK, "Failed to set attribute, hr %#lx.\n", hr);

    hr = IMFMediaType_SetGUID(mediatype, &MF_MT_SUBTYPE, &MFVideoFormat_RGB8);
    ok(hr == S_OK, "Failed to set attribute, hr %#lx.\n", hr);

    hr = IMFMediaType_SetUINT64(mediatype, &MF_MT_FRAME_SIZE, ((UINT64)16) << 32 | 16);
    ok(hr == S_OK, "Failed to set attribute, hr %#lx.\n", hr);

    hr = IMFTransform_GetOutputStreamInfo(copier, 0, &output_info);
    ok(hr == S_OK, "Failed to get stream info, hr %#lx.\n", hr);
    ok(!output_info.dwFlags, "Unexpected flags %#lx.\n", output_info.dwFlags);
    ok(!output_info.cbSize, "Unexpected size %lu.\n", output_info.cbSize);
    ok(!output_info.cbAlignment, "Unexpected alignment %lu.\n", output_info.cbAlignment);

    hr = IMFTransform_GetInputStreamInfo(copier, 0, &input_info);
    ok(hr == S_OK, "Failed to get stream info, hr %#lx.\n", hr);

    ok(!input_info.hnsMaxLatency, "Unexpected latency %s.\n", wine_dbgstr_longlong(input_info.hnsMaxLatency));
    ok(!input_info.dwFlags, "Unexpected flags %#lx.\n", input_info.dwFlags);
    ok(!input_info.cbSize, "Unexpected size %lu.\n", input_info.cbSize);
    ok(!input_info.cbMaxLookahead, "Unexpected lookahead size %lu.\n", input_info.cbMaxLookahead);
    ok(!input_info.cbAlignment, "Unexpected alignment %lu.\n", input_info.cbAlignment);

    hr = IMFTransform_SetOutputType(copier, 0, mediatype, 0);
    ok(hr == S_OK, "Failed to set input type, hr %#lx.\n", hr);

    hr = IMFTransform_GetOutputStreamInfo(copier, 0, &output_info);
    ok(hr == S_OK, "Failed to get stream info, hr %#lx.\n", hr);
    ok(!output_info.dwFlags, "Unexpected flags %#lx.\n", output_info.dwFlags);
    ok(output_info.cbSize == 16 * 16, "Unexpected size %lu.\n", output_info.cbSize);
    ok(!output_info.cbAlignment, "Unexpected alignment %lu.\n", output_info.cbAlignment);

    hr = IMFTransform_GetOutputCurrentType(copier, 0, &mediatype2);
    ok(hr == S_OK, "Failed to get current type, hr %#lx.\n", hr);
    IMFMediaType_Release(mediatype2);

    hr = IMFTransform_GetInputCurrentType(copier, 0, &mediatype2);
    ok(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "Unexpected hr %#lx.\n", hr);

    hr = IMFTransform_GetInputStatus(copier, 0, &flags);
    ok(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "Unexpected hr %#lx.\n", hr);

    /* Setting input type resets output type. */
    hr = IMFTransform_GetOutputCurrentType(copier, 0, &mediatype2);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
    IMFMediaType_Release(mediatype2);

    hr = IMFTransform_SetInputType(copier, 0, mediatype, 0);
    ok(hr == S_OK, "Failed to set input type, hr %#lx.\n", hr);

    hr = IMFTransform_GetOutputCurrentType(copier, 0, &mediatype2);
    ok(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "Unexpected hr %#lx.\n", hr);

    hr = IMFTransform_GetInputAvailableType(copier, 0, 1, &mediatype2);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
    ok(is_sample_copier_available_type(mediatype2), "Unexpected type.\n");
    IMFMediaType_Release(mediatype2);

    hr = IMFTransform_GetInputStreamInfo(copier, 0, &input_info);
    ok(hr == S_OK, "Failed to get stream info, hr %#lx.\n", hr);
    ok(!input_info.hnsMaxLatency, "Unexpected latency %s.\n", wine_dbgstr_longlong(input_info.hnsMaxLatency));
    ok(!input_info.dwFlags, "Unexpected flags %#lx.\n", input_info.dwFlags);
    ok(input_info.cbSize == 16 * 16, "Unexpected size %lu.\n", input_info.cbSize);
    ok(!input_info.cbMaxLookahead, "Unexpected lookahead size %lu.\n", input_info.cbMaxLookahead);
    ok(!input_info.cbAlignment, "Unexpected alignment %lu.\n", input_info.cbAlignment);

    hr = IMFTransform_GetOutputAvailableType(copier, 0, 0, &mediatype2);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
    hr = IMFMediaType_IsEqual(mediatype2, mediatype, &flags);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
    IMFMediaType_Release(mediatype2);

    hr = IMFTransform_GetInputStatus(copier, 0, &flags);
    ok(hr == S_OK, "Failed to get input status, hr %#lx.\n", hr);
    ok(flags == MFT_INPUT_STATUS_ACCEPT_DATA, "Unexpected flags %#lx.\n", flags);

    hr = IMFTransform_GetInputCurrentType(copier, 0, &mediatype2);
    ok(hr == S_OK, "Failed to get current type, hr %#lx.\n", hr);
    IMFMediaType_Release(mediatype2);

    hr = IMFTransform_GetOutputStatus(copier, &flags);
    ok(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "Unexpected hr %#lx.\n", hr);

    hr = IMFTransform_SetOutputType(copier, 0, mediatype, 0);
    ok(hr == S_OK, "Failed to set output type, hr %#lx.\n", hr);

    hr = IMFTransform_GetOutputStatus(copier, &flags);
    ok(hr == S_OK, "Failed to get output status, hr %#lx.\n", hr);
    ok(!flags, "Unexpected flags %#lx.\n", flags);

    /* Pushing samples. */
    hr = MFCreateAlignedMemoryBuffer(output_info.cbSize, output_info.cbAlignment, &media_buffer);
    ok(hr == S_OK, "Failed to create media buffer, hr %#lx.\n", hr);

    hr = IMFSample_AddBuffer(sample, media_buffer);
    ok(hr == S_OK, "Failed to add a buffer, hr %#lx.\n", hr);
    IMFMediaBuffer_Release(media_buffer);

    EXPECT_REF(sample, 1);
    hr = IMFTransform_ProcessInput(copier, 0, sample, 0);
    ok(hr == S_OK, "Failed to process input, hr %#lx.\n", hr);
    EXPECT_REF(sample, 2);

    hr = IMFTransform_GetInputStatus(copier, 0, &flags);
    ok(hr == S_OK, "Failed to get input status, hr %#lx.\n", hr);
    ok(!flags, "Unexpected flags %#lx.\n", flags);

    hr = IMFTransform_GetOutputStatus(copier, &flags);
    ok(hr == S_OK, "Failed to get output status, hr %#lx.\n", hr);
    ok(flags == MFT_OUTPUT_STATUS_SAMPLE_READY, "Unexpected flags %#lx.\n", flags);

    hr = IMFTransform_ProcessInput(copier, 0, sample, 0);
    ok(hr == MF_E_NOTACCEPTING, "Unexpected hr %#lx.\n", hr);

    hr = IMFTransform_GetOutputStreamInfo(copier, 0, &output_info);
    ok(hr == S_OK, "Failed to get output info, hr %#lx.\n", hr);

    hr = MFCreateAlignedMemoryBuffer(output_info.cbSize, output_info.cbAlignment, &media_buffer);
    ok(hr == S_OK, "Failed to create media buffer, hr %#lx.\n", hr);

    hr = MFCreateSample(&client_sample);
    ok(hr == S_OK, "Failed to create a sample, hr %#lx.\n", hr);

    hr = IMFSample_AddBuffer(client_sample, media_buffer);
    ok(hr == S_OK, "Failed to add a buffer, hr %#lx.\n", hr);
    IMFMediaBuffer_Release(media_buffer);

    hr = check_mft_process_output(copier, client_sample, &output_status);
    ok(hr == S_OK, "Failed to get output, hr %#lx.\n", hr);
    ok(output_status == 0, "got output[0].dwStatus %#lx\n", output_status);
    EXPECT_REF(sample, 1);

    hr = check_mft_process_output(copier, client_sample, &output_status);
    ok(hr == MF_E_TRANSFORM_NEED_MORE_INPUT, "Failed to get output, hr %#lx.\n", hr);
    ok(output_status == 0, "got output[0].dwStatus %#lx\n", output_status);

    /* Flushing. */
    hr = IMFTransform_ProcessInput(copier, 0, sample, 0);
    ok(hr == S_OK, "Failed to process input, hr %#lx.\n", hr);
    EXPECT_REF(sample, 2);

    hr = IMFTransform_ProcessMessage(copier, MFT_MESSAGE_COMMAND_FLUSH, 0);
    ok(hr == S_OK, "Failed to flush, hr %#lx.\n", hr);

    ref = IMFSample_Release(sample);
    ok(ref == 0, "Release returned %ld\n", ref);
    ref = IMFSample_Release(client_sample);
    ok(ref == 0, "Release returned %ld\n", ref);

    ref = IMFTransform_Release(copier);
    ok(ref == 0, "Release returned %ld\n", ref);
    ref = IMFMediaType_Release(mediatype);
    ok(ref == 0, "Release returned %ld\n", ref);

    winetest_pop_context();
}

struct sample_metadata
{
    unsigned int flags;
    LONGLONG duration;
    LONGLONG time;
};

static void sample_copier_process(IMFTransform *copier, IMFMediaBuffer *input_buffer,
        IMFMediaBuffer *output_buffer, const struct sample_metadata *md)
{
    static const struct sample_metadata zero_md = { 0, ~0u, ~0u };
    IMFSample *input_sample, *output_sample;
    DWORD flags, output_status;
    LONGLONG time;
    HRESULT hr;
    LONG ref;

    hr = MFCreateSample(&input_sample);
    ok(hr == S_OK, "Failed to create a sample, hr %#lx.\n", hr);

    if (md)
    {
        hr = IMFSample_SetSampleFlags(input_sample, md->flags);
        ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);

        hr = IMFSample_SetSampleTime(input_sample, md->time);
        ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);

        hr = IMFSample_SetSampleDuration(input_sample, md->duration);
        ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
    }

    hr = MFCreateSample(&output_sample);
    ok(hr == S_OK, "Failed to create a sample, hr %#lx.\n", hr);

    hr = IMFSample_SetSampleFlags(output_sample, ~0u);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);

    hr = IMFSample_SetSampleTime(output_sample, ~0u);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);

    hr = IMFSample_SetSampleDuration(output_sample, ~0u);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);

    hr = IMFSample_AddBuffer(input_sample, input_buffer);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);

    hr = IMFSample_AddBuffer(output_sample, output_buffer);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);

    hr = IMFTransform_ProcessInput(copier, 0, input_sample, 0);
    ok(hr == S_OK, "Failed to process input, hr %#lx.\n", hr);

    hr = check_mft_process_output(copier, output_sample, &output_status);
    ok(hr == S_OK, "Failed to get output, hr %#lx.\n", hr);
    ok(output_status == 0, "got output[0].dwStatus %#lx\n", output_status);

    if (!md) md = &zero_md;

    hr = IMFSample_GetSampleFlags(output_sample, &flags);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
    ok(md->flags == flags, "Unexpected flags.\n");
    hr = IMFSample_GetSampleTime(output_sample, &time);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
    ok(md->time == time, "Unexpected time.\n");
    hr = IMFSample_GetSampleDuration(output_sample, &time);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
    ok(md->duration == time, "Unexpected duration.\n");

    ref = IMFSample_Release(input_sample);
    ok(ref == 0, "Release returned %ld\n", ref);
    ref = IMFSample_Release(output_sample);
    ok(ref == 0, "Release returned %ld\n", ref);
}

static void test_sample_copier_output_processing(void)
{
    IMFMediaBuffer *input_buffer, *output_buffer;
    MFT_OUTPUT_STREAM_INFO output_info;
    struct sample_metadata md;
    IMFMediaType *mediatype;
    IMFTransform *copier;
    DWORD max_length;
    HRESULT hr;
    BYTE *ptr;
    LONG ref;

    if (!pMFCreateSampleCopierMFT)
        return;

    hr = pMFCreateSampleCopierMFT(&copier);
    ok(hr == S_OK, "Failed to create sample copier, hr %#lx.\n", hr);

    /* Configure for 16 x 16 of D3DFMT_X8R8G8B8. */
    hr = MFCreateMediaType(&mediatype);
    ok(hr == S_OK, "Failed to create media type, hr %#lx.\n", hr);

    hr = IMFMediaType_SetGUID(mediatype, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
    ok(hr == S_OK, "Failed to set attribute, hr %#lx.\n", hr);

    hr = IMFMediaType_SetGUID(mediatype, &MF_MT_SUBTYPE, &MFVideoFormat_RGB32);
    ok(hr == S_OK, "Failed to set attribute, hr %#lx.\n", hr);

    hr = IMFMediaType_SetUINT64(mediatype, &MF_MT_FRAME_SIZE, ((UINT64)16) << 32 | 16);
    ok(hr == S_OK, "Failed to set attribute, hr %#lx.\n", hr);

    hr = IMFTransform_SetInputType(copier, 0, mediatype, 0);
    ok(hr == S_OK, "Failed to set input type, hr %#lx.\n", hr);

    hr = IMFTransform_SetOutputType(copier, 0, mediatype, 0);
    ok(hr == S_OK, "Failed to set input type, hr %#lx.\n", hr);

    /* Source and destination are linear buffers, destination is twice as large. */
    hr = IMFTransform_GetOutputStreamInfo(copier, 0, &output_info);
    ok(hr == S_OK, "Failed to get output info, hr %#lx.\n", hr);

    hr = MFCreateAlignedMemoryBuffer(output_info.cbSize, output_info.cbAlignment, &output_buffer);
    ok(hr == S_OK, "Failed to create media buffer, hr %#lx.\n", hr);

    hr = IMFMediaBuffer_Lock(output_buffer, &ptr, &max_length, NULL);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
    memset(ptr, 0xcc, max_length);
    hr = IMFMediaBuffer_Unlock(output_buffer);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);

    hr = MFCreateAlignedMemoryBuffer(output_info.cbSize, output_info.cbAlignment, &input_buffer);
    ok(hr == S_OK, "Failed to create media buffer, hr %#lx.\n", hr);

    hr = IMFMediaBuffer_Lock(input_buffer, &ptr, &max_length, NULL);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
    memset(ptr, 0xaa, max_length);
    hr = IMFMediaBuffer_Unlock(input_buffer);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
    hr = IMFMediaBuffer_SetCurrentLength(input_buffer, 4);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);

    sample_copier_process(copier, input_buffer, output_buffer, NULL);

    hr = IMFMediaBuffer_Lock(output_buffer, &ptr, &max_length, NULL);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
    ok(ptr[0] == 0xaa && ptr[4] == 0xcc, "Unexpected buffer contents.\n");

    hr = IMFMediaBuffer_Unlock(output_buffer);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);

    md.flags = 123;
    md.time = 10;
    md.duration = 2;
    sample_copier_process(copier, input_buffer, output_buffer, &md);

    ref = IMFMediaBuffer_Release(input_buffer);
    ok(ref == 0, "Release returned %ld\n", ref);
    ref = IMFMediaBuffer_Release(output_buffer);
    ok(ref == 0, "Release returned %ld\n", ref);

    ref = IMFTransform_Release(copier);
    ok(ref == 0, "Release returned %ld\n", ref);
    ref = IMFMediaType_Release(mediatype);
    ok(ref == 0, "Release returned %ld\n", ref);
}

static IMFSample *create_sample(const BYTE *data, ULONG size)
{
    IMFMediaBuffer *media_buffer;
    IMFSample *sample;
    DWORD length;
    BYTE *buffer;
    HRESULT hr;
    ULONG ret;

    hr = MFCreateSample(&sample);
    ok(hr == S_OK, "MFCreateSample returned %#lx\n", hr);
    hr = MFCreateMemoryBuffer(size, &media_buffer);
    ok(hr == S_OK, "MFCreateMemoryBuffer returned %#lx\n", hr);
    hr = IMFMediaBuffer_Lock(media_buffer, &buffer, NULL, &length);
    ok(hr == S_OK, "Lock returned %#lx\n", hr);
    ok(length == 0, "got length %lu\n", length);
    if (!data) memset(buffer, 0xcd, size);
    else memcpy(buffer, data, size);
    hr = IMFMediaBuffer_Unlock(media_buffer);
    ok(hr == S_OK, "Unlock returned %#lx\n", hr);
    hr = IMFMediaBuffer_SetCurrentLength(media_buffer, data ? size : 0);
    ok(hr == S_OK, "SetCurrentLength returned %#lx\n", hr);
    hr = IMFSample_AddBuffer(sample, media_buffer);
    ok(hr == S_OK, "AddBuffer returned %#lx\n", hr);
    ret = IMFMediaBuffer_Release(media_buffer);
    ok(ret == 1, "Release returned %lu\n", ret);

    return sample;
}

static const BYTE aac_codec_data[14] = {0x00,0x00,0x29,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x12,0x08};

static void test_aac_encoder(void)
{
    const GUID *const class_id = &CLSID_AACMFTEncoder;
    const struct transform_info expect_mft_info =
    {
        .name = L"Microsoft AAC Audio Encoder MFT",
        .major_type = &MFMediaType_Audio,
        .inputs =
        {
            {.subtype = &MFAudioFormat_PCM},
        },
        .outputs =
        {
            {.subtype = &MFAudioFormat_AAC},
        },
    };

    static const struct attribute_desc input_type_desc[] =
    {
        ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio, .required = TRUE),
        ATTR_GUID(MF_MT_SUBTYPE, MFAudioFormat_PCM, .required = TRUE),
        ATTR_UINT32(MF_MT_AUDIO_NUM_CHANNELS, 1, .required = TRUE),
        ATTR_UINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16, .required = TRUE),
        ATTR_UINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 44100, .required = TRUE),
        {0},
    };
    const struct attribute_desc output_type_desc[] =
    {
        ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio, .required = TRUE),
        ATTR_GUID(MF_MT_SUBTYPE, MFAudioFormat_AAC, .required = TRUE),
        ATTR_UINT32(MF_MT_AUDIO_NUM_CHANNELS, 1, .required = TRUE),
        ATTR_UINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16, .required = TRUE),
        ATTR_UINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 44100, .required = TRUE),
        ATTR_UINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 12000, .required = TRUE),
        {0},
    };

    static const struct attribute_desc expect_input_type_desc[] =
    {
        ATTR_GUID(MF_MT_AM_FORMAT_TYPE, FORMAT_WaveFormatEx),
        ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio),
        ATTR_GUID(MF_MT_SUBTYPE, MFAudioFormat_PCM),
        ATTR_UINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16),
        ATTR_UINT32(MF_MT_AUDIO_NUM_CHANNELS, 1),
        ATTR_UINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 2),
        ATTR_UINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 44100),
        ATTR_UINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 44100 * 2),
        ATTR_UINT32(MF_MT_AVG_BITRATE, 44100 * 2 * 8),
        ATTR_UINT32(MF_MT_COMPRESSED, 0),
        ATTR_UINT32(MF_MT_FIXED_SIZE_SAMPLES, 1),
        ATTR_UINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, 1),
        ATTR_UINT32(MF_MT_AUDIO_PREFER_WAVEFORMATEX, 1),
        {0},
    };
    const struct attribute_desc expect_output_type_desc[] =
    {
        ATTR_GUID(MF_MT_AM_FORMAT_TYPE, FORMAT_WaveFormatEx),
        ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio),
        ATTR_GUID(MF_MT_SUBTYPE, MFAudioFormat_AAC),
        ATTR_UINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16),
        ATTR_UINT32(MF_MT_AUDIO_NUM_CHANNELS, 1),
        ATTR_UINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 1),
        ATTR_UINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 44100),
        ATTR_UINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 12000),
        ATTR_UINT32(MF_MT_AVG_BITRATE, 96000),
        ATTR_UINT32(MF_MT_COMPRESSED, 1),
        ATTR_UINT32(MF_MT_FIXED_SIZE_SAMPLES, 0),
        ATTR_UINT32(MF_MT_AUDIO_PREFER_WAVEFORMATEX, 1),
        ATTR_UINT32(MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION, 41),
        ATTR_UINT32(MF_MT_AAC_PAYLOAD_TYPE, 0),
        ATTR_BLOB(MF_MT_USER_DATA, aac_codec_data, sizeof(aac_codec_data)),
        {0},
    };

    MFT_REGISTER_TYPE_INFO output_type = {MFMediaType_Audio, MFAudioFormat_AAC};
    MFT_REGISTER_TYPE_INFO input_type = {MFMediaType_Audio, MFAudioFormat_PCM};
    MFT_OUTPUT_STREAM_INFO output_info;
    MFT_INPUT_STREAM_INFO input_info;
    IMFMediaType *media_type;
    IMFTransform *transform;
    HRESULT hr;
    ULONG ret;

    hr = CoInitialize(NULL);
    ok(hr == S_OK, "Failed to initialize, hr %#lx.\n", hr);

    winetest_push_context("aacenc");

    if (!check_mft_enum(MFT_CATEGORY_AUDIO_ENCODER, &input_type, &output_type, class_id))
        goto failed;
    check_mft_get_info(class_id, &expect_mft_info);

    if (FAILED(hr = CoCreateInstance(class_id, NULL, CLSCTX_INPROC_SERVER,
            &IID_IMFTransform, (void **)&transform)))
        goto failed;

    check_interface(transform, &IID_IMFTransform, TRUE);
    check_interface(transform, &IID_IMediaObject, FALSE);

    check_mft_set_input_type_required(transform, input_type_desc);

    hr = MFCreateMediaType(&media_type);
    ok(hr == S_OK, "MFCreateMediaType returned %#lx\n", hr);
    init_media_type(media_type, input_type_desc, -1);
    hr = IMFTransform_SetInputType(transform, 0, media_type, 0);
    ok(hr == S_OK, "SetInputType returned %#lx.\n", hr);
    ret = IMFMediaType_Release(media_type);
    ok(ret == 1, "Release returned %lu\n", ret);

    check_mft_set_output_type_required(transform, output_type_desc);

    hr = MFCreateMediaType(&media_type);
    ok(hr == S_OK, "MFCreateMediaType returned %#lx\n", hr);
    init_media_type(media_type, output_type_desc, -1);
    hr = IMFTransform_SetOutputType(transform, 0, media_type, 0);
    ok(hr == S_OK, "SetOutputType returned %#lx.\n", hr);
    ret = IMFMediaType_Release(media_type);
    ok(ret == 1, "Release returned %lu\n", ret);

    hr = IMFTransform_GetInputCurrentType(transform, 0, &media_type);
    ok(hr == S_OK, "GetInputCurrentType returned %#lx.\n", hr);
    check_media_type(media_type, expect_input_type_desc, -1);
    ret = IMFMediaType_Release(media_type);
    ok(ret <= 1, "Release returned %lu\n", ret);

    hr = IMFTransform_GetOutputCurrentType(transform, 0, &media_type);
    ok(hr == S_OK, "GetOutputCurrentType returned %#lx.\n", hr);
    check_media_type(media_type, expect_output_type_desc, -1);
    ret = IMFMediaType_Release(media_type);
    ok(ret <= 1, "Release returned %lu\n", ret);

    memset(&input_info, 0xcd, sizeof(input_info));
    hr = IMFTransform_GetInputStreamInfo(transform, 0, &input_info);
    ok(hr == S_OK, "GetInputStreamInfo returned %#lx\n", hr);
    ok(input_info.hnsMaxLatency == 0, "got hnsMaxLatency %I64d\n", input_info.hnsMaxLatency);
    ok(input_info.dwFlags == 0, "got dwFlags %#lx\n", input_info.dwFlags);
    ok(input_info.cbSize == 0, "got cbSize %lu\n", input_info.cbSize);
    ok(input_info.cbMaxLookahead == 0, "got cbMaxLookahead %#lx\n", input_info.cbMaxLookahead);
    ok(input_info.cbAlignment == 0, "got cbAlignment %#lx\n", input_info.cbAlignment);

    memset(&output_info, 0xcd, sizeof(output_info));
    hr = IMFTransform_GetOutputStreamInfo(transform, 0, &output_info);
    ok(hr == S_OK, "GetOutputStreamInfo returned %#lx\n", hr);
    ok(output_info.dwFlags == 0, "got dwFlags %#lx\n", output_info.dwFlags);
    ok(output_info.cbSize == 0x600, "got cbSize %#lx\n", output_info.cbSize);
    ok(output_info.cbAlignment == 0, "got cbAlignment %#lx\n", output_info.cbAlignment);

    ret = IMFTransform_Release(transform);
    ok(ret == 0, "Release returned %lu\n", ret);

failed:
    winetest_pop_context();
    CoUninitialize();
}

static void test_aac_decoder(void)
{
    const GUID *const class_id = &CLSID_MSAACDecMFT;
    const struct transform_info expect_mft_info =
    {
        .name = L"Microsoft AAC Audio Decoder MFT",
        .major_type = &MFMediaType_Audio,
        .inputs =
        {
            {.subtype = &MFAudioFormat_AAC},
            {.subtype = &MFAudioFormat_RAW_AAC1},
            {.subtype = &MFAudioFormat_ADTS, .broken = TRUE /* <= w8 */},
        },
        .outputs =
        {
            {.subtype = &MFAudioFormat_Float},
            {.subtype = &MFAudioFormat_PCM},
        },
    };

    static const struct attribute_desc expect_input_attributes[] =
    {
        ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio),
        ATTR_UINT32(MF_MT_AUDIO_PREFER_WAVEFORMATEX, 1),
        ATTR_UINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 32),
        ATTR_UINT32(MF_MT_AUDIO_NUM_CHANNELS, 6),
        ATTR_UINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 24),
        ATTR_UINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 48000),
        ATTR_UINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 1152000),
        {0},
    };
    static const media_type_desc expect_available_inputs[] =
    {
        {
            ATTR_GUID(MF_MT_SUBTYPE, MFAudioFormat_AAC),
            ATTR_UINT32(MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION, 0),
            ATTR_UINT32(MF_MT_AAC_PAYLOAD_TYPE, 0),
            /* MF_MT_USER_DATA with some AAC codec data */
        },
        {
            ATTR_GUID(MF_MT_SUBTYPE, MFAudioFormat_RAW_AAC1),
        },
        {
            ATTR_GUID(MF_MT_SUBTYPE, MFAudioFormat_AAC),
            ATTR_UINT32(MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION, 0),
            ATTR_UINT32(MF_MT_AAC_PAYLOAD_TYPE, 1),
            /* MF_MT_USER_DATA with some AAC codec data */
        },
        {
            ATTR_GUID(MF_MT_SUBTYPE, MFAudioFormat_AAC),
            ATTR_UINT32(MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION, 0),
            ATTR_UINT32(MF_MT_AAC_PAYLOAD_TYPE, 3),
            /* MF_MT_USER_DATA with some AAC codec data */
        },
        {
            ATTR_GUID(MF_MT_SUBTYPE, MFAudioFormat_ADTS),
        },
    };
    static const struct attribute_desc expect_output_attributes[] =
    {
        ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio),
        ATTR_UINT32(MF_MT_AUDIO_PREFER_WAVEFORMATEX, 1),
        ATTR_UINT32(MF_MT_AUDIO_NUM_CHANNELS, 1),
        ATTR_UINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 44100),
        ATTR_UINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, 1),
        {0},
    };
    static const media_type_desc expect_available_outputs[] =
    {
        {
            ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio),
            ATTR_GUID(MF_MT_SUBTYPE, MFAudioFormat_Float),
            ATTR_UINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 32),
            ATTR_UINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 4),
            ATTR_UINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 4 * 44100),
        },
        {
            ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio),
            ATTR_GUID(MF_MT_SUBTYPE, MFAudioFormat_PCM),
            ATTR_UINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16),
            ATTR_UINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 2),
            ATTR_UINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 2 * 44100),
        },
    };

    const struct attribute_desc input_type_desc[] =
    {
        ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio, .required = TRUE),
        ATTR_GUID(MF_MT_SUBTYPE, MFAudioFormat_AAC, .required = TRUE),
        ATTR_UINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 44100, .required = TRUE),
        ATTR_BLOB(MF_MT_USER_DATA, aac_codec_data, sizeof(aac_codec_data), .required = TRUE),
        ATTR_UINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16),
        ATTR_UINT32(MF_MT_AUDIO_NUM_CHANNELS, 1),
        ATTR_UINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 12000),
        ATTR_UINT32(MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION, 41),
        ATTR_UINT32(MF_MT_AAC_PAYLOAD_TYPE, 0),
        {0},
    };
    static const struct attribute_desc output_type_desc[] =
    {
        ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio, .required = TRUE),
        ATTR_GUID(MF_MT_SUBTYPE, MFAudioFormat_PCM, .required = TRUE),
        ATTR_UINT32(MF_MT_AUDIO_NUM_CHANNELS, 1, .required = TRUE),
        ATTR_UINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16, .required = TRUE),
        ATTR_UINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 44100, .required = TRUE),
        {0},
    };

    MFT_REGISTER_TYPE_INFO output_type = {MFMediaType_Audio, MFAudioFormat_Float};
    MFT_REGISTER_TYPE_INFO input_type = {MFMediaType_Audio, MFAudioFormat_AAC};
    MFT_OUTPUT_STREAM_INFO output_info;
    MFT_INPUT_STREAM_INFO input_info;
    IMFMediaType *media_type;
    IMFTransform *transform;
    ULONG i, ret;
    DWORD flags;
    HRESULT hr;

    hr = CoInitialize(NULL);
    ok(hr == S_OK, "Failed to initialize, hr %#lx.\n", hr);

    winetest_push_context("aacdec");

    if (!check_mft_enum(MFT_CATEGORY_AUDIO_DECODER, &input_type, &output_type, class_id))
        goto failed;
    check_mft_get_info(class_id, &expect_mft_info);

    if (FAILED(hr = CoCreateInstance(class_id, NULL, CLSCTX_INPROC_SERVER,
            &IID_IMFTransform, (void **)&transform)))
        goto failed;

    check_interface(transform, &IID_IMFTransform, TRUE);
    check_interface(transform, &IID_IMediaObject, FALSE);

    /* check default media types */

    flags = MFT_INPUT_STREAM_WHOLE_SAMPLES | MFT_INPUT_STREAM_SINGLE_SAMPLE_PER_BUFFER | MFT_INPUT_STREAM_FIXED_SAMPLE_SIZE | MFT_INPUT_STREAM_HOLDS_BUFFERS;
    memset(&input_info, 0xcd, sizeof(input_info));
    hr = IMFTransform_GetInputStreamInfo(transform, 0, &input_info);
    ok(hr == S_OK, "GetInputStreamInfo returned %#lx\n", hr);
    ok(input_info.hnsMaxLatency == 0, "got hnsMaxLatency %s\n", wine_dbgstr_longlong(input_info.hnsMaxLatency));
    ok(input_info.dwFlags == flags, "got dwFlags %#lx\n", input_info.dwFlags);
    ok(input_info.cbSize == 0, "got cbSize %lu\n", input_info.cbSize);
    ok(input_info.cbMaxLookahead == 0, "got cbMaxLookahead %#lx\n", input_info.cbMaxLookahead);
    ok(input_info.cbAlignment == 0, "got cbAlignment %#lx\n", input_info.cbAlignment);

    flags = MFT_INPUT_STREAM_WHOLE_SAMPLES;
    memset(&output_info, 0xcd, sizeof(output_info));
    hr = IMFTransform_GetOutputStreamInfo(transform, 0, &output_info);
    ok(hr == S_OK, "GetOutputStreamInfo returned %#lx\n", hr);
    ok(output_info.dwFlags == flags, "got dwFlags %#lx\n", output_info.dwFlags);
    ok(output_info.cbSize == 0xc000, "got cbSize %#lx\n", output_info.cbSize);
    ok(output_info.cbAlignment == 0, "got cbAlignment %#lx\n", output_info.cbAlignment);

    hr = IMFTransform_GetOutputAvailableType(transform, 0, 0, &media_type);
    ok(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "GetOutputAvailableType returned %#lx\n", hr);

    i = -1;
    while (SUCCEEDED(hr = IMFTransform_GetInputAvailableType(transform, 0, ++i, &media_type)))
    {
        winetest_push_context("in %lu", i);
        ok(hr == S_OK, "GetInputAvailableType returned %#lx\n", hr);
        check_media_type(media_type, expect_input_attributes, -1);
        check_media_type(media_type, expect_available_inputs[i], -1);
        hr = IMFTransform_SetInputType(transform, 0, media_type, MFT_SET_TYPE_TEST_ONLY);
        if (i != 1) ok(hr == S_OK, "SetInputType returned %#lx.\n", hr);
        else ok(hr == MF_E_INVALIDMEDIATYPE, "SetInputType returned %#lx.\n", hr);
        ret = IMFMediaType_Release(media_type);
        ok(ret <= 1, "Release returned %lu\n", ret);
        winetest_pop_context();
    }
    ok(hr == MF_E_NO_MORE_TYPES, "GetInputAvailableType returned %#lx\n", hr);
    ok(i == ARRAY_SIZE(expect_available_inputs)
            || broken(i == 2) /* w7 */ || broken(i == 4) /* w8 */,
            "%lu input media types\n", i);

    /* setting output media type first doesn't work */

    hr = MFCreateMediaType(&media_type);
    ok(hr == S_OK, "MFCreateMediaType returned %#lx\n", hr);
    init_media_type(media_type, output_type_desc, -1);
    hr = IMFTransform_SetOutputType(transform, 0, media_type, 0);
    ok(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "SetOutputType returned %#lx.\n", hr);
    ret = IMFMediaType_Release(media_type);
    ok(ret == 0, "Release returned %lu\n", ret);

    check_mft_set_input_type_required(transform, input_type_desc);

    hr = MFCreateMediaType(&media_type);
    ok(hr == S_OK, "MFCreateMediaType returned %#lx\n", hr);
    init_media_type(media_type, input_type_desc, -1);
    hr = IMFTransform_SetInputType(transform, 0, media_type, 0);
    ok(hr == S_OK, "SetInputType returned %#lx.\n", hr);
    ret = IMFMediaType_Release(media_type);
    ok(ret == 1, "Release returned %lu\n", ret);

    /* check new output media types */

    i = -1;
    while (SUCCEEDED(hr = IMFTransform_GetOutputAvailableType(transform, 0, ++i, &media_type)))
    {
        winetest_push_context("out %lu", i);
        ok(hr == S_OK, "GetOutputAvailableType returned %#lx\n", hr);
        check_media_type(media_type, expect_output_attributes, -1);
        check_media_type(media_type, expect_available_outputs[i], -1);
        hr = IMFTransform_SetOutputType(transform, 0, media_type, MFT_SET_TYPE_TEST_ONLY);
        ok(hr == S_OK, "SetInputType returned %#lx.\n", hr);
        ret = IMFMediaType_Release(media_type);
        ok(ret <= 1, "Release returned %lu\n", ret);
        winetest_pop_context();
    }
    ok(hr == MF_E_NO_MORE_TYPES, "GetOutputAvailableType returned %#lx\n", hr);
    ok(i == ARRAY_SIZE(expect_available_outputs), "%lu input media types\n", i);

    check_mft_set_output_type_required(transform, output_type_desc);

    hr = MFCreateMediaType(&media_type);
    ok(hr == S_OK, "MFCreateMediaType returned %#lx\n", hr);
    init_media_type(media_type, output_type_desc, -1);
    hr = IMFTransform_SetOutputType(transform, 0, media_type, 0);
    ok(hr == S_OK, "SetOutputType returned %#lx.\n", hr);
    ret = IMFMediaType_Release(media_type);
    ok(ret == 1, "Release returned %lu\n", ret);

    flags = MFT_INPUT_STREAM_WHOLE_SAMPLES | MFT_INPUT_STREAM_SINGLE_SAMPLE_PER_BUFFER | MFT_INPUT_STREAM_FIXED_SAMPLE_SIZE | MFT_INPUT_STREAM_HOLDS_BUFFERS;
    memset(&input_info, 0xcd, sizeof(input_info));
    hr = IMFTransform_GetInputStreamInfo(transform, 0, &input_info);
    ok(hr == S_OK, "GetInputStreamInfo returned %#lx\n", hr);
    ok(input_info.hnsMaxLatency == 0, "got hnsMaxLatency %s\n", wine_dbgstr_longlong(input_info.hnsMaxLatency));
    ok(input_info.dwFlags == flags, "got dwFlags %#lx\n", input_info.dwFlags);
    ok(input_info.cbSize == 0, "got cbSize %lu\n", input_info.cbSize);
    ok(input_info.cbMaxLookahead == 0, "got cbMaxLookahead %#lx\n", input_info.cbMaxLookahead);
    ok(input_info.cbAlignment == 0, "got cbAlignment %#lx\n", input_info.cbAlignment);

    flags = MFT_INPUT_STREAM_WHOLE_SAMPLES;
    memset(&output_info, 0xcd, sizeof(output_info));
    hr = IMFTransform_GetOutputStreamInfo(transform, 0, &output_info);
    ok(hr == S_OK, "GetOutputStreamInfo returned %#lx\n", hr);
    ok(output_info.dwFlags == flags, "got dwFlags %#lx\n", output_info.dwFlags);
    ok(output_info.cbSize == 0xc000, "got cbSize %#lx\n", output_info.cbSize);
    ok(output_info.cbAlignment == 0, "got cbAlignment %#lx\n", output_info.cbAlignment);

    ret = IMFTransform_Release(transform);
    ok(ret == 0, "Release returned %lu\n", ret);

failed:
    winetest_pop_context();
    CoUninitialize();
}

static const BYTE wma_codec_data[10] = {0, 0x44, 0, 0, 0x17, 0, 0, 0, 0, 0};
static const ULONG wmaenc_block_size = 1487;
static const ULONG wmadec_block_size = 0x2000;

static void test_wma_encoder(void)
{
    const GUID *const class_id = &CLSID_CWMAEncMediaObject;
    const struct transform_info expect_mft_info =
    {
        .name = L"WMAudio Encoder MFT",
        .major_type = &MFMediaType_Audio,
        .inputs =
        {
            {.subtype = &MFAudioFormat_PCM},
            {.subtype = &MFAudioFormat_Float},
        },
        .outputs =
        {
            {.subtype = &MFAudioFormat_WMAudioV8},
            {.subtype = &MFAudioFormat_WMAudioV9},
            {.subtype = &MFAudioFormat_WMAudio_Lossless},
        },
    };
    const struct transform_info expect_dmo_info =
    {
        .name = L"WMAudio Encoder DMO",
        .major_type = &MEDIATYPE_Audio,
        .inputs =
        {
            {.subtype = &MEDIASUBTYPE_PCM},
        },
        .outputs =
        {
            {.subtype = &MEDIASUBTYPE_WMAUDIO2},
            {.subtype = &MEDIASUBTYPE_WMAUDIO3},
            {.subtype = &MEDIASUBTYPE_WMAUDIO_LOSSLESS},
        },
    };

    static const struct attribute_desc input_type_desc[] =
    {
        ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio, .required = TRUE),
        ATTR_GUID(MF_MT_SUBTYPE, MFAudioFormat_Float, .required = TRUE),
        ATTR_UINT32(MF_MT_AUDIO_NUM_CHANNELS, 2, .required = TRUE),
        ATTR_UINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 32, .required = TRUE),
        ATTR_UINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 22050, .required = TRUE),
        ATTR_UINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 2 * (32 / 8), .required = TRUE),
        ATTR_UINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 2 * (32 / 8) * 22050, .required = TRUE),
        {0},
    };
    const struct attribute_desc output_type_desc[] =
    {
        ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio, .required = TRUE),
        ATTR_GUID(MF_MT_SUBTYPE, MFAudioFormat_WMAudioV8, .required = TRUE),
        ATTR_UINT32(MF_MT_AUDIO_NUM_CHANNELS, 2, .required = TRUE),
        ATTR_UINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 22050, .required = TRUE),
        ATTR_UINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 4003, .required = TRUE),
        ATTR_UINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, wmaenc_block_size, .required = TRUE),
        ATTR_BLOB(MF_MT_USER_DATA, wma_codec_data, sizeof(wma_codec_data), .required = TRUE),
        {0},
    };

    const struct buffer_desc output_buffer_desc[] =
    {
        {.length = wmaenc_block_size},
    };
    const struct attribute_desc output_sample_attributes[] =
    {
        ATTR_UINT32(mft_output_sample_incomplete, 1),
        ATTR_UINT32(MFSampleExtension_CleanPoint, 1),
        {0},
    };
    const struct sample_desc output_sample_desc[] =
    {
        {
            .attributes = output_sample_attributes,
            .sample_time = 0, .sample_duration = 3250794,
            .buffer_count = 1, .buffers = output_buffer_desc,
        },
        {
            .attributes = output_sample_attributes,
            .sample_time = 3250794, .sample_duration = 3715193,
            .buffer_count = 1, .buffers = output_buffer_desc,
        },
        {
            .attributes = output_sample_attributes,
            .sample_time = 6965986, .sample_duration = 3366893,
            .buffer_count = 1, .buffers = output_buffer_desc,
        },
    };

    MFT_REGISTER_TYPE_INFO output_type = {MFMediaType_Audio, MFAudioFormat_WMAudioV8};
    MFT_REGISTER_TYPE_INFO input_type = {MFMediaType_Audio, MFAudioFormat_Float};
    IMFSample *input_sample, *output_sample;
    MFT_OUTPUT_STREAM_INFO output_info;
    MFT_INPUT_STREAM_INFO input_info;
    IMFCollection *output_samples;
    DWORD length, output_status;
    IMFMediaType *media_type;
    IMFTransform *transform;
    const BYTE *audio_data;
    ULONG audio_data_len;
    ULONG i, ret;
    HRESULT hr;
    LONG ref;

    hr = CoInitialize(NULL);
    ok(hr == S_OK, "Failed to initialize, hr %#lx.\n", hr);

    winetest_push_context("wmaenc");

    if (!check_mft_enum(MFT_CATEGORY_AUDIO_ENCODER, &input_type, &output_type, class_id))
        goto failed;
    check_mft_get_info(class_id, &expect_mft_info);
    check_dmo_get_info(class_id, &expect_dmo_info);

    if (FAILED(hr = CoCreateInstance(class_id, NULL, CLSCTX_INPROC_SERVER,
            &IID_IMFTransform, (void **)&transform)))
        goto failed;

    check_interface(transform, &IID_IMFTransform, TRUE);
    check_interface(transform, &IID_IMediaObject, TRUE);
    check_interface(transform, &IID_IPropertyStore, TRUE);
    check_interface(transform, &IID_IPropertyBag, TRUE);

    check_mft_set_input_type_required(transform, input_type_desc);

    hr = MFCreateMediaType(&media_type);
    ok(hr == S_OK, "MFCreateMediaType returned %#lx\n", hr);
    init_media_type(media_type, input_type_desc, -1);
    hr = IMFTransform_SetInputType(transform, 0, media_type, 0);
    ok(hr == S_OK, "SetInputType returned %#lx.\n", hr);
    ret = IMFMediaType_Release(media_type);
    ok(ret == 0, "Release returned %lu\n", ret);

    check_mft_set_output_type_required(transform, output_type_desc);

    hr = MFCreateMediaType(&media_type);
    ok(hr == S_OK, "MFCreateMediaType returned %#lx\n", hr);
    init_media_type(media_type, output_type_desc, -1);
    hr = IMFTransform_SetOutputType(transform, 0, media_type, 0);
    ok(hr == S_OK, "SetOutputType returned %#lx.\n", hr);
    ret = IMFMediaType_Release(media_type);
    ok(ret == 0, "Release returned %lu\n", ret);

    memset(&input_info, 0xcd, sizeof(input_info));
    hr = IMFTransform_GetInputStreamInfo(transform, 0, &input_info);
    ok(hr == S_OK, "GetInputStreamInfo returned %#lx\n", hr);
    ok(input_info.hnsMaxLatency == 19969161, "got hnsMaxLatency %s\n",
            wine_dbgstr_longlong(input_info.hnsMaxLatency));
    ok(input_info.dwFlags == 0, "got dwFlags %#lx\n", input_info.dwFlags);
    ok(input_info.cbSize == 8, "got cbSize %lu\n", input_info.cbSize);
    ok(input_info.cbMaxLookahead == 0, "got cbMaxLookahead %#lx\n", input_info.cbMaxLookahead);
    ok(input_info.cbAlignment == 1, "got cbAlignment %#lx\n", input_info.cbAlignment);

    memset(&output_info, 0xcd, sizeof(output_info));
    hr = IMFTransform_GetOutputStreamInfo(transform, 0, &output_info);
    ok(hr == S_OK, "GetOutputStreamInfo returned %#lx\n", hr);
    ok(output_info.dwFlags == 0, "got dwFlags %#lx\n", output_info.dwFlags);
    ok(output_info.cbSize == wmaenc_block_size, "got cbSize %#lx\n", output_info.cbSize);
    ok(output_info.cbAlignment == 1, "got cbAlignment %#lx\n", output_info.cbAlignment);

    load_resource(L"audiodata.bin", &audio_data, &audio_data_len);
    ok(audio_data_len == 179928, "got length %lu\n", audio_data_len);

    input_sample = create_sample(audio_data, audio_data_len);
    hr = IMFSample_SetSampleTime(input_sample, 0);
    ok(hr == S_OK, "SetSampleTime returned %#lx\n", hr);
    hr = IMFSample_SetSampleDuration(input_sample, 10000000);
    ok(hr == S_OK, "SetSampleDuration returned %#lx\n", hr);
    hr = IMFTransform_ProcessInput(transform, 0, input_sample, 0);
    ok(hr == S_OK, "ProcessInput returned %#lx\n", hr);
    hr = IMFTransform_ProcessMessage(transform, MFT_MESSAGE_COMMAND_DRAIN, 0);
    ok(hr == S_OK, "ProcessMessage returned %#lx\n", hr);
    hr = IMFTransform_ProcessInput(transform, 0, input_sample, 0);
    ok(hr == MF_E_NOTACCEPTING, "ProcessInput returned %#lx\n", hr);
    ref = IMFSample_Release(input_sample);
    ok(ref <= 1, "Release returned %ld\n", ref);

    hr = MFCreateCollection(&output_samples);
    ok(hr == S_OK, "MFCreateCollection returned %#lx\n", hr);

    output_sample = create_sample(NULL, output_info.cbSize);
    for (i = 0; SUCCEEDED(hr = check_mft_process_output(transform, output_sample, &output_status)); i++)
    {
        winetest_push_context("%lu", i);
        ok(hr == S_OK, "ProcessOutput returned %#lx\n", hr);
        ok(output_status == MFT_OUTPUT_DATA_BUFFER_INCOMPLETE, "got output[0].dwStatus %#lx\n", output_status);
        hr = IMFCollection_AddElement(output_samples, (IUnknown *)output_sample);
        ok(hr == S_OK, "AddElement returned %#lx\n", hr);
        ref = IMFSample_Release(output_sample);
        ok(ref == 1, "Release returned %ld\n", ref);
        output_sample = create_sample(NULL, output_info.cbSize);
        winetest_pop_context();
    }
    ok(hr == MF_E_TRANSFORM_NEED_MORE_INPUT, "ProcessOutput returned %#lx\n", hr);
    ok(output_status == 0, "got output[0].dwStatus %#lx\n", output_status);
    ret = IMFSample_Release(output_sample);
    ok(ret == 0, "Release returned %lu\n", ret);
    ok(i == 3, "got %lu output samples\n", i);

    ret = check_mf_sample_collection(output_samples, output_sample_desc, L"wmaencdata.bin");
    ok(ret == 0, "got %lu%% diff\n", ret);
    IMFCollection_Release(output_samples);

    output_sample = create_sample(NULL, output_info.cbSize);
    hr = check_mft_process_output(transform, output_sample, &output_status);
    ok(hr == MF_E_TRANSFORM_NEED_MORE_INPUT, "ProcessOutput returned %#lx\n", hr);
    ok(output_status == 0, "got output[0].dwStatus %#lx\n", output_status);
    hr = IMFSample_GetTotalLength(output_sample, &length);
    ok(hr == S_OK, "GetTotalLength returned %#lx\n", hr);
    ok(length == 0, "got length %lu\n", length);
    ret = IMFSample_Release(output_sample);
    ok(ret == 0, "Release returned %lu\n", ret);

    ret = IMFTransform_Release(transform);
    ok(ret == 0, "Release returned %lu\n", ret);

failed:
    winetest_pop_context();
    CoUninitialize();
}

static void test_wma_decoder(void)
{
    const GUID *const class_id = &CLSID_CWMADecMediaObject;
    const struct transform_info expect_mft_info =
    {
        .name = L"WMAudio Decoder MFT",
        .major_type = &MFMediaType_Audio,
        .inputs =
        {
            {.subtype = &MEDIASUBTYPE_MSAUDIO1},
            {.subtype = &MFAudioFormat_WMAudioV8},
            {.subtype = &MFAudioFormat_WMAudioV9},
            {.subtype = &MFAudioFormat_WMAudio_Lossless},
        },
        .outputs =
        {
            {.subtype = &MFAudioFormat_PCM},
            {.subtype = &MFAudioFormat_Float},
        },
    };
    const struct transform_info expect_dmo_info =
    {
        .name = L"WMAudio Decoder DMO",
        .major_type = &MEDIATYPE_Audio,
        .inputs =
        {
            {.subtype = &MEDIASUBTYPE_MSAUDIO1},
            {.subtype = &MEDIASUBTYPE_WMAUDIO2},
            {.subtype = &MEDIASUBTYPE_WMAUDIO3},
            {.subtype = &MEDIASUBTYPE_WMAUDIO_LOSSLESS},
        },
        .outputs =
        {
            {.subtype = &MEDIASUBTYPE_PCM},
            {.subtype = &MEDIASUBTYPE_IEEE_FLOAT},
        },
    };

    static const media_type_desc expect_available_inputs[] =
    {
        {
            ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio),
            ATTR_GUID(MF_MT_SUBTYPE, MEDIASUBTYPE_MSAUDIO1),
            ATTR_UINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, 1),
        },
        {
            ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio),
            ATTR_GUID(MF_MT_SUBTYPE, MFAudioFormat_WMAudioV8),
            ATTR_UINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, 1),
        },
        {
            ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio),
            ATTR_GUID(MF_MT_SUBTYPE, MFAudioFormat_WMAudioV9),
            ATTR_UINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, 1),
        },
        {
            ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio),
            ATTR_GUID(MF_MT_SUBTYPE, MFAudioFormat_WMAudio_Lossless),
            ATTR_UINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, 1),
        },
    };
    static const media_type_desc expect_available_outputs[] =
    {
        {
            ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio),
            ATTR_GUID(MF_MT_SUBTYPE, MFAudioFormat_Float),
            ATTR_UINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 32),
            ATTR_UINT32(MF_MT_AUDIO_NUM_CHANNELS, 2),
            ATTR_UINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 22050),
            ATTR_UINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 176400),
            ATTR_UINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 8),
            ATTR_UINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, 1),
            ATTR_UINT32(MF_MT_FIXED_SIZE_SAMPLES, 1),
            ATTR_UINT32(MF_MT_AUDIO_PREFER_WAVEFORMATEX, 1),
        },
        {
            ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio),
            ATTR_GUID(MF_MT_SUBTYPE, MFAudioFormat_PCM),
            ATTR_UINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16),
            ATTR_UINT32(MF_MT_AUDIO_NUM_CHANNELS, 2),
            ATTR_UINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 22050),
            ATTR_UINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 88200),
            ATTR_UINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 4),
            ATTR_UINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, 1),
            ATTR_UINT32(MF_MT_FIXED_SIZE_SAMPLES, 1),
            ATTR_UINT32(MF_MT_AUDIO_PREFER_WAVEFORMATEX, 1),
        },
    };

    const struct attribute_desc input_type_desc[] =
    {
        ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio, .required = TRUE),
        ATTR_GUID(MF_MT_SUBTYPE, MFAudioFormat_WMAudioV8, .required = TRUE),
        ATTR_BLOB(MF_MT_USER_DATA, wma_codec_data, sizeof(wma_codec_data), .required = TRUE),
        ATTR_UINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, wmaenc_block_size, .required = TRUE),
        ATTR_UINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 22050, .required = TRUE),
        ATTR_UINT32(MF_MT_AUDIO_NUM_CHANNELS, 2, .required = TRUE),
        ATTR_UINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 4003), /* not required by SetInputType, but needed for the transform to work */
        {0},
    };
    static const struct attribute_desc output_type_desc[] =
    {
        ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio, .required = TRUE),
        ATTR_GUID(MF_MT_SUBTYPE, MFAudioFormat_PCM, .required = TRUE),
        ATTR_UINT32(MF_MT_AUDIO_NUM_CHANNELS, 2, .required = TRUE),
        ATTR_UINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16, .required = TRUE),
        ATTR_UINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 22050, .required = TRUE),
        ATTR_UINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 2 * (16 / 8), .required = TRUE),
        ATTR_UINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 2 * (16 / 8) * 22050, .required = TRUE),
        {0},
    };

    const struct buffer_desc output_buffer_desc[] =
    {
        {.length = wmadec_block_size, .compare = compare_pcm16},
        {.length = wmadec_block_size / 2, .compare = compare_pcm16, .todo_length = TRUE},
    };
    const struct attribute_desc output_sample_attributes[] =
    {
        ATTR_UINT32(mft_output_sample_incomplete, 1),
        ATTR_UINT32(MFSampleExtension_CleanPoint, 1),
        {0},
    };
    const struct attribute_desc output_sample_attributes_todo[] =
    {
        ATTR_UINT32(mft_output_sample_incomplete, 1, .todo = TRUE),
        ATTR_UINT32(MFSampleExtension_CleanPoint, 1),
        {0},
    };
    struct sample_desc output_sample_desc[] =
    {
        {
            .attributes = output_sample_attributes + 0,
            .sample_time = 0, .sample_duration = 928798,
            .buffer_count = 1, .buffers = output_buffer_desc + 0, .repeat_count = 1,
        },
        {
            .attributes = output_sample_attributes + 0,
            .sample_time = 1857596, .sample_duration = 928798,
            .buffer_count = 1, .buffers = output_buffer_desc + 0,
        },
        {
            .attributes = output_sample_attributes + 1, /* not MFT_OUTPUT_DATA_BUFFER_INCOMPLETE */
            .sample_time = 2786394, .sample_duration = 464399,
            .buffer_count = 1, .buffers = output_buffer_desc + 1, .todo_length = TRUE,
        },
    };

    MFT_REGISTER_TYPE_INFO input_type = {MFMediaType_Audio, MFAudioFormat_WMAudioV8};
    MFT_REGISTER_TYPE_INFO output_type = {MFMediaType_Audio, MFAudioFormat_Float};
    IUnknown *unknown, *tmp_unknown, outer = {&test_unk_vtbl};
    IMFSample *input_sample, *output_sample;
    MFT_OUTPUT_STREAM_INFO output_info;
    MFT_INPUT_STREAM_INFO input_info;
    IMFCollection *output_samples;
    DWORD length, output_status;
    IMediaObject *media_object;
    IPropertyBag *property_bag;
    IMFMediaType *media_type;
    IMFTransform *transform;
    const BYTE *wmaenc_data;
    ULONG wmaenc_data_len;
    ULONG i, ret, ref;
    HRESULT hr;

    hr = CoInitialize(NULL);
    ok(hr == S_OK, "Failed to initialize, hr %#lx.\n", hr);

    winetest_push_context("wmadec");

    if (!check_mft_enum(MFT_CATEGORY_AUDIO_DECODER, &input_type, &output_type, class_id))
        goto failed;
    check_mft_get_info(class_id, &expect_mft_info);
    check_dmo_get_info(class_id, &expect_dmo_info);

    if (FAILED(hr = CoCreateInstance(class_id, NULL, CLSCTX_INPROC_SERVER,
            &IID_IMFTransform, (void **)&transform)))
        goto failed;

    check_interface(transform, &IID_IMFTransform, TRUE);
    check_interface(transform, &IID_IMediaObject, TRUE);
    todo_wine
    check_interface(transform, &IID_IPropertyStore, TRUE);
    check_interface(transform, &IID_IPropertyBag, TRUE);

    /* check default media types */

    hr = IMFTransform_GetInputStreamInfo(transform, 0, &input_info);
    ok(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "GetInputStreamInfo returned %#lx\n", hr);
    hr = IMFTransform_GetOutputStreamInfo(transform, 0, &output_info);
    ok(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "GetOutputStreamInfo returned %#lx\n", hr);
    hr = IMFTransform_GetOutputAvailableType(transform, 0, 0, &media_type);
    ok(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "GetOutputAvailableType returned %#lx\n", hr);

    i = -1;
    while (SUCCEEDED(hr = IMFTransform_GetInputAvailableType(transform, 0, ++i, &media_type)))
    {
        winetest_push_context("in %lu", i);
        ok(hr == S_OK, "GetInputAvailableType returned %#lx\n", hr);
        check_media_type(media_type, expect_available_inputs[i], -1);
        hr = IMFTransform_SetInputType(transform, 0, media_type, 0);
        ok(hr == MF_E_INVALIDMEDIATYPE, "SetInputType returned %#lx.\n", hr);
        ret = IMFMediaType_Release(media_type);
        ok(ret == 0, "Release returned %lu\n", ret);
        winetest_pop_context();
    }
    todo_wine
    ok(hr == MF_E_NO_MORE_TYPES, "GetInputAvailableType returned %#lx\n", hr);
    todo_wine
    ok(i == 4, "%lu input media types\n", i);

    /* setting output media type first doesn't work */

    hr = MFCreateMediaType(&media_type);
    ok(hr == S_OK, "MFCreateMediaType returned %#lx\n", hr);
    init_media_type(media_type, output_type_desc, -1);
    hr = IMFTransform_SetOutputType(transform, 0, media_type, 0);
    ok(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "SetOutputType returned %#lx.\n", hr);
    ret = IMFMediaType_Release(media_type);
    ok(ret == 0, "Release returned %lu\n", ret);

    check_mft_set_input_type_required(transform, input_type_desc);

    hr = MFCreateMediaType(&media_type);
    ok(hr == S_OK, "MFCreateMediaType returned %#lx\n", hr);
    init_media_type(media_type, input_type_desc, -1);
    hr = IMFTransform_SetInputType(transform, 0, media_type, 0);
    ok(hr == S_OK, "SetInputType returned %#lx.\n", hr);
    ret = IMFMediaType_Release(media_type);
    ok(ret == 0, "Release returned %lu\n", ret);

    hr = IMFTransform_GetInputStreamInfo(transform, 0, &input_info);
    ok(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "GetInputStreamInfo returned %#lx\n", hr);
    hr = IMFTransform_GetOutputStreamInfo(transform, 0, &output_info);
    ok(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "GetOutputStreamInfo returned %#lx\n", hr);

    /* check new output media types */

    i = -1;
    while (SUCCEEDED(hr = IMFTransform_GetOutputAvailableType(transform, 0, ++i, &media_type)))
    {
        winetest_push_context("out %lu", i);
        ok(hr == S_OK, "GetOutputAvailableType returned %#lx\n", hr);
        check_media_type(media_type, expect_available_outputs[i], -1);
        ret = IMFMediaType_Release(media_type);
        ok(ret == 0, "Release returned %lu\n", ret);
        winetest_pop_context();
    }
    ok(hr == MF_E_NO_MORE_TYPES, "GetOutputAvailableType returned %#lx\n", hr);
    ok(i == 2, "%lu output media types\n", i);

    check_mft_set_output_type_required(transform, output_type_desc);

    hr = MFCreateMediaType(&media_type);
    ok(hr == S_OK, "MFCreateMediaType returned %#lx\n", hr);
    init_media_type(media_type, output_type_desc, -1);
    hr = IMFTransform_SetOutputType(transform, 0, media_type, 0);
    ok(hr == S_OK, "SetOutputType returned %#lx.\n", hr);
    ret = IMFMediaType_Release(media_type);
    ok(ret == 0, "Release returned %lu\n", ret);

    memset(&input_info, 0xcd, sizeof(input_info));
    hr = IMFTransform_GetInputStreamInfo(transform, 0, &input_info);
    ok(hr == S_OK, "GetInputStreamInfo returned %#lx\n", hr);
    ok(input_info.hnsMaxLatency == 0, "got hnsMaxLatency %s\n", wine_dbgstr_longlong(input_info.hnsMaxLatency));
    ok(input_info.dwFlags == 0, "got dwFlags %#lx\n", input_info.dwFlags);
    ok(input_info.cbSize == wmaenc_block_size, "got cbSize %lu\n", input_info.cbSize);
    ok(input_info.cbMaxLookahead == 0, "got cbMaxLookahead %#lx\n", input_info.cbMaxLookahead);
    ok(input_info.cbAlignment == 1, "got cbAlignment %#lx\n", input_info.cbAlignment);

    memset(&output_info, 0xcd, sizeof(output_info));
    hr = IMFTransform_GetOutputStreamInfo(transform, 0, &output_info);
    ok(hr == S_OK, "GetOutputStreamInfo returned %#lx\n", hr);
    ok(output_info.dwFlags == 0, "got dwFlags %#lx\n", output_info.dwFlags);
    ok(output_info.cbSize == wmadec_block_size, "got cbSize %#lx\n", output_info.cbSize);
    ok(output_info.cbAlignment == 1, "got cbAlignment %#lx\n", output_info.cbAlignment);

    load_resource(L"wmaencdata.bin", &wmaenc_data, &wmaenc_data_len);
    ok(wmaenc_data_len % wmaenc_block_size == 0, "got length %lu\n", wmaenc_data_len);

    input_sample = create_sample(wmaenc_data, wmaenc_block_size / 2);
    hr = IMFTransform_ProcessInput(transform, 0, input_sample, 0);
    ok(hr == S_OK, "ProcessInput returned %#lx\n", hr);
    ret = IMFSample_Release(input_sample);
    ok(ret == 0, "Release returned %lu\n", ret);
    input_sample = create_sample(wmaenc_data, wmaenc_block_size + 1);
    hr = IMFTransform_ProcessInput(transform, 0, input_sample, 0);
    ok(hr == S_OK, "ProcessInput returned %#lx\n", hr);
    ret = IMFSample_Release(input_sample);
    ok(ret == 0, "Release returned %lu\n", ret);
    input_sample = create_sample(wmaenc_data, wmaenc_block_size);
    hr = IMFTransform_ProcessInput(transform, 0, input_sample, 0);
    ok(hr == S_OK, "ProcessInput returned %#lx\n", hr);
    hr = IMFTransform_ProcessInput(transform, 0, input_sample, 0);
    ok(hr == MF_E_NOTACCEPTING, "ProcessInput returned %#lx\n", hr);
    ret = IMFSample_Release(input_sample);
    ok(ret == 1, "Release returned %lu\n", ret);

    /* As output_info.dwFlags doesn't have MFT_OUTPUT_STREAM_CAN_PROVIDE_SAMPLES
     * IMFTransform_ProcessOutput needs a sample or returns MF_E_TRANSFORM_NEED_MORE_INPUT */

    hr = check_mft_process_output(transform, NULL, &output_status);
    ok(hr == MF_E_TRANSFORM_NEED_MORE_INPUT, "ProcessOutput returned %#lx\n", hr);
    ok(output_status == MFT_OUTPUT_DATA_BUFFER_NO_SAMPLE
            || broken(output_status == (MFT_OUTPUT_DATA_BUFFER_INCOMPLETE|MFT_OUTPUT_DATA_BUFFER_NO_SAMPLE)) /* Win7 */,
            "got output[0].dwStatus %#lx\n", output_status);

    input_sample = create_sample(wmaenc_data, wmaenc_block_size);
    hr = IMFTransform_ProcessInput(transform, 0, input_sample, 0);
    ok(hr == MF_E_NOTACCEPTING, "ProcessInput returned %#lx\n", hr);
    ret = IMFSample_Release(input_sample);
    ok(ret == 0, "Release returned %lu\n", ret);

    hr = check_mft_process_output(transform, NULL, &output_status);
    ok(hr == MF_E_TRANSFORM_NEED_MORE_INPUT, "ProcessOutput returned %#lx\n", hr);
    ok(output_status == MFT_OUTPUT_DATA_BUFFER_NO_SAMPLE
            || broken(output_status == (MFT_OUTPUT_DATA_BUFFER_INCOMPLETE|MFT_OUTPUT_DATA_BUFFER_NO_SAMPLE)) /* Win7 */,
            "got output[0].dwStatus %#lx\n", output_status);

    hr = MFCreateCollection(&output_samples);
    ok(hr == S_OK, "MFCreateCollection returned %#lx\n", hr);

    output_sample = create_sample(NULL, output_info.cbSize);
    for (i = 0; SUCCEEDED(hr = check_mft_process_output(transform, output_sample, &output_status)); i++)
    {
        winetest_push_context("%lu", i);
        ok(!(output_status & ~MFT_OUTPUT_DATA_BUFFER_INCOMPLETE), "got output[0].dwStatus %#lx\n", output_status);
        ok(hr == S_OK, "ProcessOutput returned %#lx\n", hr);
        hr = IMFCollection_AddElement(output_samples, (IUnknown *)output_sample);
        ok(hr == S_OK, "AddElement returned %#lx\n", hr);
        ref = IMFSample_Release(output_sample);
        ok(ref == 1, "Release returned %ld\n", ref);
        output_sample = create_sample(NULL, output_info.cbSize);
        winetest_pop_context();
    }
    ok(hr == MF_E_TRANSFORM_NEED_MORE_INPUT, "ProcessOutput returned %#lx\n", hr);
    ok(output_status == 0, "got output[0].dwStatus %#lx\n", output_status);
    ret = IMFSample_Release(output_sample);
    ok(ret == 0, "Release returned %lu\n", ret);
    todo_wine_if(i == 3) /* wmadec output depends on ffmpeg version used */
    ok(i == 4, "got %lu output samples\n", i);

    if (!strcmp(winetest_platform, "wine") && i == 3)
        output_sample_desc[1].attributes = output_sample_attributes_todo;

    ret = check_mf_sample_collection(output_samples, output_sample_desc, L"wmadecdata.bin");
    todo_wine_if(ret > 0 && ret <= 10)  /* ffmpeg sometimes offsets the decoded data */
    ok(ret == 0, "got %lu%% diff\n", ret);
    IMFCollection_Release(output_samples);

    output_sample = create_sample(NULL, output_info.cbSize);
    hr = check_mft_process_output(transform, output_sample, &output_status);
    ok(hr == MF_E_TRANSFORM_NEED_MORE_INPUT, "ProcessOutput returned %#lx\n", hr);
    ok(output_status == 0, "got output[0].dwStatus %#lx\n", output_status);
    hr = IMFSample_GetTotalLength(output_sample, &length);
    ok(hr == S_OK, "GetTotalLength returned %#lx\n", hr);
    ok(length == 0, "got length %lu\n", length);
    ret = IMFSample_Release(output_sample);
    ok(ret == 0, "Release returned %lu\n", ret);

    input_sample = create_sample(wmaenc_data, wmaenc_block_size);
    hr = IMFTransform_ProcessInput(transform, 0, input_sample, 0);
    ok(hr == S_OK, "ProcessInput returned %#lx\n", hr);

    ret = IMFTransform_Release(transform);
    ok(ret == 0, "Release returned %lu\n", ret);
    ret = IMFSample_Release(input_sample);
    ok(ret == 0, "Release returned %lu\n", ret);

    hr = CoCreateInstance( &CLSID_CWMADecMediaObject, &outer, CLSCTX_INPROC_SERVER, &IID_IUnknown,
                           (void **)&unknown );
    ok( hr == S_OK, "CoCreateInstance returned %#lx\n", hr );
    hr = IUnknown_QueryInterface( unknown, &IID_IMFTransform, (void **)&transform );
    ok( hr == S_OK, "QueryInterface returned %#lx\n", hr );
    hr = IUnknown_QueryInterface( unknown, &IID_IMediaObject, (void **)&media_object );
    ok( hr == S_OK, "QueryInterface returned %#lx\n", hr );
    hr = IUnknown_QueryInterface( unknown, &IID_IPropertyBag, (void **)&property_bag );
    ok( hr == S_OK, "QueryInterface returned %#lx\n", hr );
    hr = IUnknown_QueryInterface( media_object, &IID_IUnknown, (void **)&tmp_unknown );
    ok( hr == S_OK, "QueryInterface returned %#lx\n", hr );

    ok( unknown != &outer, "got outer IUnknown\n" );
    ok( transform != (void *)unknown, "got IUnknown == IMFTransform\n" );
    ok( media_object != (void *)unknown, "got IUnknown == IMediaObject\n" );
    ok( property_bag != (void *)unknown, "got IUnknown == IPropertyBag\n" );
    ok( tmp_unknown != unknown, "got inner IUnknown\n" );

    check_interface( unknown, &IID_IPersistPropertyBag, FALSE );
    check_interface( unknown, &IID_IAMFilterMiscFlags, FALSE );
    check_interface( unknown, &IID_IMediaSeeking, FALSE );
    check_interface( unknown, &IID_IMediaPosition, FALSE );
    check_interface( unknown, &IID_IReferenceClock, FALSE );
    check_interface( unknown, &IID_IBasicAudio, FALSE );

    ref = IUnknown_Release( tmp_unknown );
    ok( ref == 1, "Release returned %lu\n", ref );
    ref = IPropertyBag_Release( property_bag );
    ok( ref == 1, "Release returned %lu\n", ref );
    ref = IMediaObject_Release( media_object );
    ok( ref == 1, "Release returned %lu\n", ref );
    ref = IMFTransform_Release( transform );
    ok( ref == 1, "Release returned %lu\n", ref );
    ref = IUnknown_Release( unknown );
    ok( ref == 0, "Release returned %lu\n", ref );

failed:
    winetest_pop_context();
    CoUninitialize();
}

#define next_h264_sample(a, b) next_h264_sample_(__LINE__, a, b)
static IMFSample *next_h264_sample_(int line, const BYTE **h264_buf, ULONG *h264_len)
{
    const BYTE *sample_data;

    ok_(__FILE__, line)(*h264_len > 4, "invalid h264 length\n");
    ok_(__FILE__, line)(*(UINT32 *)*h264_buf == 0x01000000, "invalid h264 buffer\n");
    sample_data = *h264_buf;

    (*h264_len) -= 4;
    (*h264_buf) += 4;

    while (*h264_len >= 4 && *(UINT32 *)*h264_buf != 0x01000000)
    {
        (*h264_len)--;
        (*h264_buf)++;
    }

    return create_sample(sample_data, *h264_buf - sample_data);
}

static void test_h264_decoder(void)
{
    const GUID *const class_id = &CLSID_MSH264DecoderMFT;
    const struct transform_info expect_mft_info =
    {
        .name = L"Microsoft H264 Video Decoder MFT",
        .major_type = &MFMediaType_Video,
        .inputs =
        {
            {.subtype = &MFVideoFormat_H264},
            {.subtype = &MFVideoFormat_H264_ES},
        },
        .outputs =
        {
            {.subtype = &MFVideoFormat_NV12},
            {.subtype = &MFVideoFormat_YV12},
            {.subtype = &MFVideoFormat_IYUV},
            {.subtype = &MFVideoFormat_I420},
            {.subtype = &MFVideoFormat_YUY2},
        },
    };
    static const media_type_desc default_inputs[] =
    {
        {
            ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Video),
            ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_H264),
        },
        {
            ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Video),
            ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_H264_ES),
        },
    };
    static const DWORD input_width = 120, input_height = 248;
    const media_type_desc default_outputs[] =
    {
        {
            ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Video),
            ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_NV12),
            ATTR_RATIO(MF_MT_PIXEL_ASPECT_RATIO, 1, 1),
            ATTR_RATIO(MF_MT_FRAME_RATE, 30000, 1001),
            ATTR_UINT32(MF_MT_DEFAULT_STRIDE, input_width),
            ATTR_UINT32(MF_MT_INTERLACE_MODE, 7),
            ATTR_UINT32(MF_MT_FIXED_SIZE_SAMPLES, 1),
            ATTR_UINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, 1),
            ATTR_RATIO(MF_MT_FRAME_SIZE, input_width, input_height),
            ATTR_UINT32(MF_MT_SAMPLE_SIZE, input_width * input_height * 3 / 2),
            /* ATTR_UINT32(MF_MT_VIDEO_ROTATION, 0), missing on Win7 */
        },
        {
            ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Video),
            ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_YV12),
            ATTR_RATIO(MF_MT_PIXEL_ASPECT_RATIO, 1, 1),
            ATTR_RATIO(MF_MT_FRAME_RATE, 30000, 1001),
            ATTR_UINT32(MF_MT_DEFAULT_STRIDE, input_width),
            ATTR_UINT32(MF_MT_INTERLACE_MODE, 7),
            ATTR_UINT32(MF_MT_FIXED_SIZE_SAMPLES, 1),
            ATTR_UINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, 1),
            ATTR_RATIO(MF_MT_FRAME_SIZE, input_width, input_height),
            ATTR_UINT32(MF_MT_SAMPLE_SIZE, input_width * input_height * 3 / 2),
            /* ATTR_UINT32(MF_MT_VIDEO_ROTATION, 0), missing on Win7 */
        },
        {
            ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Video),
            ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_IYUV),
            ATTR_RATIO(MF_MT_PIXEL_ASPECT_RATIO, 1, 1),
            ATTR_RATIO(MF_MT_FRAME_RATE, 30000, 1001),
            ATTR_UINT32(MF_MT_DEFAULT_STRIDE, input_width),
            ATTR_UINT32(MF_MT_INTERLACE_MODE, 7),
            ATTR_UINT32(MF_MT_FIXED_SIZE_SAMPLES, 1),
            ATTR_UINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, 1),
            ATTR_RATIO(MF_MT_FRAME_SIZE, input_width, input_height),
            ATTR_UINT32(MF_MT_SAMPLE_SIZE, input_width * input_height * 3 / 2),
            /* ATTR_UINT32(MF_MT_VIDEO_ROTATION, 0), missing on Win7 */
        },
        {
            ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Video),
            ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_I420),
            ATTR_RATIO(MF_MT_PIXEL_ASPECT_RATIO, 1, 1),
            ATTR_RATIO(MF_MT_FRAME_RATE, 30000, 1001),
            ATTR_UINT32(MF_MT_DEFAULT_STRIDE, input_width),
            ATTR_UINT32(MF_MT_INTERLACE_MODE, 7),
            ATTR_UINT32(MF_MT_FIXED_SIZE_SAMPLES, 1),
            ATTR_UINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, 1),
            ATTR_RATIO(MF_MT_FRAME_SIZE, input_width, input_height),
            ATTR_UINT32(MF_MT_SAMPLE_SIZE, input_width * input_height * 3 / 2),
            /* ATTR_UINT32(MF_MT_VIDEO_ROTATION, 0), missing on Win7 */
        },
        {
            ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Video),
            ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_YUY2),
            ATTR_RATIO(MF_MT_PIXEL_ASPECT_RATIO, 1, 1),
            ATTR_RATIO(MF_MT_FRAME_RATE, 30000, 1001),
            ATTR_UINT32(MF_MT_DEFAULT_STRIDE, input_width * 2),
            ATTR_UINT32(MF_MT_INTERLACE_MODE, 7),
            ATTR_UINT32(MF_MT_FIXED_SIZE_SAMPLES, 1),
            ATTR_UINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, 1),
            ATTR_RATIO(MF_MT_FRAME_SIZE, input_width, input_height),
            ATTR_UINT32(MF_MT_SAMPLE_SIZE, input_width * input_height * 2),
            /* ATTR_UINT32(MF_MT_VIDEO_ROTATION, 0), missing on Win7 */
        },
    };
    const struct attribute_desc input_type_desc[] =
    {
        ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Video, .required = TRUE),
        ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_H264, .required = TRUE),
        ATTR_RATIO(MF_MT_FRAME_SIZE, input_width, input_height),
        {0},
    };
    const struct attribute_desc output_type_desc[] =
    {
        ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Video, .required = TRUE),
        ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_NV12, .required = TRUE),
        ATTR_RATIO(MF_MT_FRAME_SIZE, input_width, input_height, .required = TRUE),
        ATTR_RATIO(MF_MT_FRAME_RATE, 60000, 1000),
        ATTR_RATIO(MF_MT_PIXEL_ASPECT_RATIO, 2, 1),
        ATTR_UINT32(MF_MT_DEFAULT_STRIDE, 3840),
        ATTR_UINT32(MF_MT_SAMPLE_SIZE, 3840 * input_height * 3 / 2),
        ATTR_UINT32(MF_MT_VIDEO_ROTATION, 0),
        {0},
    };
    static const struct attribute_desc new_output_type_desc[] =
    {
        ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Video),
        ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_I420),
        ATTR_RATIO(MF_MT_FRAME_SIZE, 96, 96),
        ATTR_RATIO(MF_MT_FRAME_RATE, 1, 1),
        ATTR_RATIO(MF_MT_PIXEL_ASPECT_RATIO, 1, 2),
        {0},
    };
    static const MFVideoArea actual_aperture = {.Area={82,84}};
    static const DWORD actual_width = 96, actual_height = 96;
    const media_type_desc actual_outputs[] =
    {
        {
            ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Video),
            ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_NV12),
            ATTR_RATIO(MF_MT_PIXEL_ASPECT_RATIO, 1, 1),
            ATTR_RATIO(MF_MT_FRAME_RATE, 60000, 1000),
            ATTR_RATIO(MF_MT_FRAME_SIZE, actual_width, actual_height),
            ATTR_UINT32(MF_MT_SAMPLE_SIZE, actual_width * actual_height * 3 / 2),
            ATTR_UINT32(MF_MT_DEFAULT_STRIDE, actual_width),
            /* ATTR_UINT32(MF_MT_VIDEO_ROTATION, 0), missing on Win7 */
            ATTR_UINT32(MF_MT_INTERLACE_MODE, 7),
            ATTR_UINT32(MF_MT_FIXED_SIZE_SAMPLES, 1),
            ATTR_UINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, 1),
            ATTR_BLOB(MF_MT_MINIMUM_DISPLAY_APERTURE, &actual_aperture, 16),
        },
        {
            ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Video),
            ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_YV12),
            ATTR_RATIO(MF_MT_PIXEL_ASPECT_RATIO, 1, 1),
            ATTR_RATIO(MF_MT_FRAME_RATE, 60000, 1000),
            ATTR_RATIO(MF_MT_FRAME_SIZE, actual_width, actual_height),
            ATTR_UINT32(MF_MT_SAMPLE_SIZE, actual_width * actual_height * 3 / 2),
            ATTR_UINT32(MF_MT_DEFAULT_STRIDE, actual_width),
            /* ATTR_UINT32(MF_MT_VIDEO_ROTATION, 0), missing on Win7 */
            ATTR_UINT32(MF_MT_INTERLACE_MODE, 7),
            ATTR_UINT32(MF_MT_FIXED_SIZE_SAMPLES, 1),
            ATTR_UINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, 1),
            ATTR_BLOB(MF_MT_MINIMUM_DISPLAY_APERTURE, &actual_aperture, 16),
        },
        {
            ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Video),
            ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_IYUV),
            ATTR_RATIO(MF_MT_PIXEL_ASPECT_RATIO, 1, 1),
            ATTR_RATIO(MF_MT_FRAME_RATE, 60000, 1000),
            ATTR_RATIO(MF_MT_FRAME_SIZE, actual_width, actual_height),
            ATTR_UINT32(MF_MT_SAMPLE_SIZE, actual_width * actual_height * 3 / 2),
            ATTR_UINT32(MF_MT_DEFAULT_STRIDE, actual_width),
            /* ATTR_UINT32(MF_MT_VIDEO_ROTATION, 0), missing on Win7 */
            ATTR_UINT32(MF_MT_INTERLACE_MODE, 7),
            ATTR_UINT32(MF_MT_FIXED_SIZE_SAMPLES, 1),
            ATTR_UINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, 1),
            ATTR_BLOB(MF_MT_MINIMUM_DISPLAY_APERTURE, &actual_aperture, 16),
        },
        {
            ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Video),
            ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_I420),
            ATTR_RATIO(MF_MT_PIXEL_ASPECT_RATIO, 1, 1),
            ATTR_RATIO(MF_MT_FRAME_RATE, 60000, 1000),
            ATTR_RATIO(MF_MT_FRAME_SIZE, actual_width, actual_height),
            ATTR_UINT32(MF_MT_SAMPLE_SIZE, actual_width * actual_height * 3 / 2),
            ATTR_UINT32(MF_MT_DEFAULT_STRIDE, actual_width),
            /* ATTR_UINT32(MF_MT_VIDEO_ROTATION, 0), missing on Win7 */
            ATTR_UINT32(MF_MT_INTERLACE_MODE, 7),
            ATTR_UINT32(MF_MT_FIXED_SIZE_SAMPLES, 1),
            ATTR_UINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, 1),
            ATTR_BLOB(MF_MT_MINIMUM_DISPLAY_APERTURE, &actual_aperture, 16),
        },
        {
            ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Video),
            ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_YUY2),
            ATTR_RATIO(MF_MT_PIXEL_ASPECT_RATIO, 1, 1),
            ATTR_RATIO(MF_MT_FRAME_RATE, 60000, 1000),
            ATTR_RATIO(MF_MT_FRAME_SIZE, actual_width, actual_height),
            ATTR_UINT32(MF_MT_SAMPLE_SIZE, actual_width * actual_height * 2),
            ATTR_UINT32(MF_MT_DEFAULT_STRIDE, actual_width * 2),
            /* ATTR_UINT32(MF_MT_VIDEO_ROTATION, 0), missing on Win7 */
            ATTR_UINT32(MF_MT_INTERLACE_MODE, 7),
            ATTR_UINT32(MF_MT_FIXED_SIZE_SAMPLES, 1),
            ATTR_UINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, 1),
            ATTR_BLOB(MF_MT_MINIMUM_DISPLAY_APERTURE, &actual_aperture, 16),
        },
    };

    const struct attribute_desc output_sample_attributes[] =
    {
        ATTR_UINT32(MFSampleExtension_CleanPoint, 1),
        {0},
    };
    const struct buffer_desc output_buffer_desc_nv12 =
    {
        .length = actual_width * actual_height * 3 / 2,
        .compare = compare_nv12, .dump = dump_nv12, .rect = {.right = 82, .bottom = 84},
    };
    const struct sample_desc output_sample_desc_nv12 =
    {
        .attributes = output_sample_attributes,
        .sample_time = 0, .sample_duration = 333667,
        .buffer_count = 1, .buffers = &output_buffer_desc_nv12,
    };
    const struct buffer_desc output_buffer_desc_i420 =
    {
        .length = actual_width * actual_height * 3 / 2,
        .compare = compare_i420, .dump = dump_i420, .rect = {.right = 82, .bottom = 84},
    };
    const struct sample_desc expect_output_sample_i420 =
    {
        .attributes = output_sample_attributes,
        .sample_time = 333667, .sample_duration = 333667, .todo_time = 1334666 /* with VA-API */,
        .buffer_count = 1, .buffers = &output_buffer_desc_i420,
    };

    MFT_REGISTER_TYPE_INFO input_type = {MFMediaType_Video, MFVideoFormat_H264};
    MFT_REGISTER_TYPE_INFO output_type = {MFMediaType_Video, MFVideoFormat_NV12};
    DWORD input_id, output_id, input_count, output_count;
    IMFSample *input_sample, *output_sample;
    MFT_OUTPUT_STREAM_INFO output_info;
    MFT_INPUT_STREAM_INFO input_info;
    IMFCollection *output_samples;
    const BYTE *h264_encoded_data;
    ULONG h264_encoded_data_len;
    DWORD length, output_status;
    IMFAttributes *attributes;
    ULONG i, ret, ref, flags;
    IMFMediaType *media_type;
    IMFTransform *transform;
    HRESULT hr;

    hr = CoInitialize(NULL);
    ok(hr == S_OK, "Failed to initialize, hr %#lx.\n", hr);

    winetest_push_context("h264dec");

    if (!check_mft_enum(MFT_CATEGORY_VIDEO_DECODER, &input_type, &output_type, class_id))
        goto failed;
    check_mft_get_info(class_id, &expect_mft_info);

    if (FAILED(hr = CoCreateInstance(class_id, NULL, CLSCTX_INPROC_SERVER,
            &IID_IMFTransform, (void **)&transform)))
        goto failed;

    hr = IMFTransform_GetAttributes(transform, &attributes);
    ok(hr == S_OK, "GetAttributes returned %#lx\n", hr);
    hr = IMFAttributes_SetUINT32(attributes, &MF_LOW_LATENCY, 1);
    ok(hr == S_OK, "SetUINT32 returned %#lx\n", hr);
    ret = IMFAttributes_Release(attributes);
    ok(ret == 1, "Release returned %ld\n", ret);

    /* no output type is available before an input type is set */

    hr = IMFTransform_GetOutputAvailableType(transform, 0, 0, &media_type);
    ok(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "GetOutputAvailableType returned %#lx\n", hr);
    hr = IMFTransform_GetOutputCurrentType(transform, 0, &media_type);
    ok(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "GetOutputCurrentType returned %#lx\n", hr);

    /* setting output media type first doesn't work */

    hr = MFCreateMediaType(&media_type);
    ok(hr == S_OK, "MFCreateMediaType returned %#lx\n", hr);
    init_media_type(media_type, default_outputs[0], -1);
    hr = IMFTransform_SetOutputType(transform, 0, media_type, 0);
    ok(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "SetOutputType returned %#lx.\n", hr);
    ret = IMFMediaType_Release(media_type);
    ok(ret == 0, "Release returned %lu\n", ret);

    /* check available input types */

    flags = MFT_INPUT_STREAM_WHOLE_SAMPLES | MFT_INPUT_STREAM_SINGLE_SAMPLE_PER_BUFFER | MFT_INPUT_STREAM_FIXED_SAMPLE_SIZE;
    memset(&input_info, 0xcd, sizeof(input_info));
    hr = IMFTransform_GetInputStreamInfo(transform, 0, &input_info);
    todo_wine
    ok(hr == S_OK, "GetInputStreamInfo returned %#lx\n", hr);
    todo_wine
    ok(input_info.hnsMaxLatency == 0, "got hnsMaxLatency %s\n", wine_dbgstr_longlong(input_info.hnsMaxLatency));
    todo_wine
    ok(input_info.dwFlags == flags, "got dwFlags %#lx\n", input_info.dwFlags);
    todo_wine
    ok(input_info.cbSize == 0x1000, "got cbSize %lu\n", input_info.cbSize);
    todo_wine
    ok(input_info.cbMaxLookahead == 0, "got cbMaxLookahead %#lx\n", input_info.cbMaxLookahead);
    todo_wine
    ok(input_info.cbAlignment == 0, "got cbAlignment %#lx\n", input_info.cbAlignment);

    flags = MFT_OUTPUT_STREAM_WHOLE_SAMPLES | MFT_OUTPUT_STREAM_SINGLE_SAMPLE_PER_BUFFER | MFT_OUTPUT_STREAM_FIXED_SAMPLE_SIZE;
    memset(&output_info, 0xcd, sizeof(output_info));
    hr = IMFTransform_GetOutputStreamInfo(transform, 0, &output_info);
    ok(hr == S_OK, "GetOutputStreamInfo returned %#lx\n", hr);
    ok(output_info.dwFlags == flags, "got dwFlags %#lx\n", output_info.dwFlags);
    ok(output_info.cbSize == 1920 * 1088 * 2, "got cbSize %#lx\n", output_info.cbSize);
    ok(output_info.cbAlignment == 0, "got cbAlignment %#lx\n", output_info.cbAlignment);

    i = -1;
    while (SUCCEEDED(hr = IMFTransform_GetInputAvailableType(transform, 0, ++i, &media_type)))
    {
        winetest_push_context("in %lu", i);
        ok(hr == S_OK, "GetInputAvailableType returned %#lx\n", hr);
        check_media_type(media_type, default_inputs[i], -1);
        ret = IMFMediaType_Release(media_type);
        ok(ret == 0, "Release returned %lu\n", ret);
        winetest_pop_context();
    }
    ok(hr == MF_E_NO_MORE_TYPES, "GetInputAvailableType returned %#lx\n", hr);
    ok(i == 2 || broken(i == 1) /* Win7 */, "%lu input media types\n", i);

    check_mft_set_input_type_required(transform, input_type_desc);

    hr = MFCreateMediaType(&media_type);
    ok(hr == S_OK, "MFCreateMediaType returned %#lx\n", hr);
    init_media_type(media_type, input_type_desc, -1);
    hr = IMFTransform_SetInputType(transform, 0, media_type, 0);
    ok(hr == S_OK, "SetInputType returned %#lx.\n", hr);
    ret = IMFMediaType_Release(media_type);
    ok(ret == 1, "Release returned %lu\n", ret);

    flags = MFT_OUTPUT_STREAM_WHOLE_SAMPLES | MFT_OUTPUT_STREAM_SINGLE_SAMPLE_PER_BUFFER | MFT_OUTPUT_STREAM_FIXED_SAMPLE_SIZE;
    memset(&output_info, 0xcd, sizeof(output_info));
    hr = IMFTransform_GetOutputStreamInfo(transform, 0, &output_info);
    ok(hr == S_OK, "GetOutputStreamInfo returned %#lx\n", hr);
    ok(output_info.dwFlags == flags, "got dwFlags %#lx\n", output_info.dwFlags);
    todo_wine
    ok(output_info.cbSize == input_width * input_height * 2, "got cbSize %#lx\n", output_info.cbSize);
    ok(output_info.cbAlignment == 0, "got cbAlignment %#lx\n", output_info.cbAlignment);

    /* output types can now be enumerated (though they are actually the same for all input types) */

    i = -1;
    while (SUCCEEDED(hr = IMFTransform_GetOutputAvailableType(transform, 0, ++i, &media_type)))
    {
        winetest_push_context("out %lu", i);
        ok(hr == S_OK, "GetOutputAvailableType returned %#lx\n", hr);
        check_media_type(media_type, default_outputs[i], -1);
        ret = IMFMediaType_Release(media_type);
        ok(ret == 0, "Release returned %lu\n", ret);
        winetest_pop_context();
    }
    ok(hr == MF_E_NO_MORE_TYPES, "GetOutputAvailableType returned %#lx\n", hr);
    ok(i == 5, "%lu output media types\n", i);

    check_mft_set_output_type_required(transform, output_type_desc);

    hr = MFCreateMediaType(&media_type);
    ok(hr == S_OK, "MFCreateMediaType returned %#lx\n", hr);
    init_media_type(media_type, output_type_desc, -1);
    hr = IMFTransform_SetOutputType(transform, 0, media_type, 0);
    ok(hr == S_OK, "SetOutputType returned %#lx.\n", hr);
    ret = IMFMediaType_Release(media_type);
    ok(ret == 1, "Release returned %lu\n", ret);

    hr = IMFTransform_GetOutputCurrentType(transform, 0, &media_type);
    ok(hr == S_OK, "GetOutputCurrentType returned %#lx\n", hr);
    check_media_type(media_type, output_type_desc, -1);
    ret = IMFMediaType_Release(media_type);
    ok(ret == 0, "Release returned %lu\n", ret);

    /* check that the output media type we've selected don't change the enumeration */

    i = -1;
    while (SUCCEEDED(hr = IMFTransform_GetOutputAvailableType(transform, 0, ++i, &media_type)))
    {
        winetest_push_context("out %lu", i);
        ok(hr == S_OK, "GetOutputAvailableType returned %#lx\n", hr);
        check_media_type(media_type, default_outputs[i], -1);
        ret = IMFMediaType_Release(media_type);
        ok(ret == 0, "Release returned %lu\n", ret);
        winetest_pop_context();
    }
    ok(hr == MF_E_NO_MORE_TYPES, "GetOutputAvailableType returned %#lx\n", hr);
    ok(i == 5, "%lu output media types\n", i);

    flags = MFT_INPUT_STREAM_WHOLE_SAMPLES | MFT_INPUT_STREAM_SINGLE_SAMPLE_PER_BUFFER | MFT_INPUT_STREAM_FIXED_SAMPLE_SIZE;
    memset(&input_info, 0xcd, sizeof(input_info));
    hr = IMFTransform_GetInputStreamInfo(transform, 0, &input_info);
    ok(hr == S_OK, "GetInputStreamInfo returned %#lx\n", hr);
    ok(input_info.hnsMaxLatency == 0, "got hnsMaxLatency %s\n", wine_dbgstr_longlong(input_info.hnsMaxLatency));
    ok(input_info.dwFlags == flags, "got dwFlags %#lx\n", input_info.dwFlags);
    ok(input_info.cbSize == 0x1000, "got cbSize %lu\n", input_info.cbSize);
    ok(input_info.cbMaxLookahead == 0, "got cbMaxLookahead %#lx\n", input_info.cbMaxLookahead);
    ok(input_info.cbAlignment == 0, "got cbAlignment %#lx\n", input_info.cbAlignment);

    flags = MFT_OUTPUT_STREAM_WHOLE_SAMPLES | MFT_OUTPUT_STREAM_SINGLE_SAMPLE_PER_BUFFER | MFT_OUTPUT_STREAM_FIXED_SAMPLE_SIZE;
    memset(&output_info, 0xcd, sizeof(output_info));
    hr = IMFTransform_GetOutputStreamInfo(transform, 0, &output_info);
    ok(hr == S_OK, "GetOutputStreamInfo returned %#lx\n", hr);
    ok(output_info.dwFlags == flags, "got dwFlags %#lx\n", output_info.dwFlags);
    todo_wine
    ok(output_info.cbSize == input_width * input_height * 2, "got cbSize %#lx\n", output_info.cbSize);
    ok(output_info.cbAlignment == 0, "got cbAlignment %#lx\n", output_info.cbAlignment);

    input_count = output_count = 0xdeadbeef;
    hr = IMFTransform_GetStreamCount(transform, &input_count, &output_count);
    todo_wine
    ok(hr == S_OK, "GetStreamCount returned %#lx\n", hr);
    todo_wine
    ok(input_count == 1, "got input_count %lu\n", input_count);
    todo_wine
    ok(output_count == 1, "got output_count %lu\n", output_count);
    hr = IMFTransform_GetStreamIDs(transform, 1, &input_id, 1, &output_id);
    ok(hr == E_NOTIMPL, "GetStreamIDs returned %#lx\n", hr);

    load_resource(L"h264data.bin", &h264_encoded_data, &h264_encoded_data_len);

    /* As output_info.dwFlags doesn't have MFT_OUTPUT_STREAM_CAN_PROVIDE_SAMPLES
     * IMFTransform_ProcessOutput needs a sample or returns an error */

    hr = check_mft_process_output(transform, NULL, &output_status);
    ok(hr == E_INVALIDARG || hr == MF_E_TRANSFORM_NEED_MORE_INPUT, "ProcessOutput returned %#lx\n", hr);
    ok(output_status == 0, "got output[0].dwStatus %#lx\n", output_status);

    i = 0;
    input_sample = next_h264_sample(&h264_encoded_data, &h264_encoded_data_len);
    while (1)
    {
        output_sample = create_sample(NULL, output_info.cbSize);
        hr = check_mft_process_output(transform, output_sample, &output_status);
        if (hr != MF_E_TRANSFORM_NEED_MORE_INPUT) break;

        ok(hr == MF_E_TRANSFORM_NEED_MORE_INPUT, "ProcessOutput returned %#lx\n", hr);
        ok(output_status == 0, "got output[0].dwStatus %#lx\n", output_status);
        hr = IMFSample_GetTotalLength(output_sample, &length);
        ok(hr == S_OK, "GetTotalLength returned %#lx\n", hr);
        ok(length == 0, "got length %lu\n", length);
        ret = IMFSample_Release(output_sample);
        ok(ret == 0, "Release returned %lu\n", ret);

        hr = IMFTransform_ProcessInput(transform, 0, input_sample, 0);
        ok(hr == S_OK, "ProcessInput returned %#lx\n", hr);
        ret = IMFSample_Release(input_sample);
        ok(ret <= 1, "Release returned %lu\n", ret);
        input_sample = next_h264_sample(&h264_encoded_data, &h264_encoded_data_len);

        hr = IMFTransform_ProcessInput(transform, 0, input_sample, 0);
        ok(hr == S_OK, "ProcessInput returned %#lx\n", hr);
        ret = IMFSample_Release(input_sample);
        ok(ret <= 1, "Release returned %lu\n", ret);
        input_sample = next_h264_sample(&h264_encoded_data, &h264_encoded_data_len);
        i++;

        hr = IMFTransform_ProcessMessage(transform, MFT_MESSAGE_COMMAND_DRAIN, 0);
        ok(hr == S_OK, "ProcessMessage returned %#lx\n", hr);
    }
    todo_wine
    ok(i == 2, "got %lu iterations\n", i);
    todo_wine
    ok(h264_encoded_data_len == 1180, "got h264_encoded_data_len %lu\n", h264_encoded_data_len);
    ok(hr == MF_E_TRANSFORM_STREAM_CHANGE, "ProcessOutput returned %#lx\n", hr);
    ok(output_status == MFT_OUTPUT_DATA_BUFFER_FORMAT_CHANGE, "got output[0].dwStatus %#lx\n", output_status);
    hr = IMFSample_GetTotalLength(output_sample, &length);
    ok(hr == S_OK, "GetTotalLength returned %#lx\n", hr);
    ok(length == 0, "got length %lu\n", length);
    ret = IMFSample_Release(output_sample);
    ok(ret == 0, "Release returned %lu\n", ret);

    flags = MFT_OUTPUT_STREAM_WHOLE_SAMPLES | MFT_OUTPUT_STREAM_SINGLE_SAMPLE_PER_BUFFER | MFT_OUTPUT_STREAM_FIXED_SAMPLE_SIZE;
    memset(&output_info, 0xcd, sizeof(output_info));
    hr = IMFTransform_GetOutputStreamInfo(transform, 0, &output_info);
    ok(hr == S_OK, "GetOutputStreamInfo returned %#lx\n", hr);
    ok(output_info.dwFlags == flags, "got dwFlags %#lx\n", output_info.dwFlags);
    ok(output_info.cbSize == actual_width * actual_height * 2, "got cbSize %#lx\n", output_info.cbSize);
    ok(output_info.cbAlignment == 0, "got cbAlignment %#lx\n", output_info.cbAlignment);

    i = -1;
    while (SUCCEEDED(hr = IMFTransform_GetOutputAvailableType(transform, 0, ++i, &media_type)))
    {
        winetest_push_context("out %lu", i);
        ok(hr == S_OK, "GetOutputAvailableType returned %#lx\n", hr);
        check_media_type(media_type, actual_outputs[i], -1);
        ret = IMFMediaType_Release(media_type);
        ok(ret == 0, "Release returned %lu\n", ret);
        winetest_pop_context();
    }
    ok(hr == MF_E_NO_MORE_TYPES, "GetOutputAvailableType returned %#lx\n", hr);
    ok(i == 5, "%lu output media types\n", i);

    /* current output type is still the one we selected */
    hr = IMFTransform_GetOutputCurrentType(transform, 0, &media_type);
    ok(hr == S_OK, "GetOutputCurrentType returned %#lx\n", hr);
    check_media_type(media_type, output_type_desc, -1);
    hr = IMFMediaType_GetItemType(media_type, &MF_MT_MINIMUM_DISPLAY_APERTURE, NULL);
    ok(hr == MF_E_ATTRIBUTENOTFOUND, "GetItemType returned %#lx\n", hr);
    ret = IMFMediaType_Release(media_type);
    ok(ret == 0, "Release returned %lu\n", ret);

    hr = MFCreateCollection(&output_samples);
    ok(hr == S_OK, "MFCreateCollection returned %#lx\n", hr);

    output_sample = create_sample(NULL, output_info.cbSize);
    hr = check_mft_process_output(transform, output_sample, &output_status);
    ok(hr == S_OK, "ProcessOutput returned %#lx\n", hr);
    ok(output_status == 0, "got output[0].dwStatus %#lx\n", output_status);
    hr = IMFCollection_AddElement(output_samples, (IUnknown *)output_sample);
    ok(hr == S_OK, "AddElement returned %#lx\n", hr);
    ref = IMFSample_Release(output_sample);
    ok(ref == 1, "Release returned %ld\n", ref);

    ret = check_mf_sample_collection(output_samples, &output_sample_desc_nv12, L"nv12frame.bmp");
    ok(ret == 0, "got %lu%% diff\n", ret);
    IMFCollection_Release(output_samples);

    /* we can change it, but only with the correct frame size */
    hr = MFCreateMediaType(&media_type);
    ok(hr == S_OK, "MFCreateMediaType returned %#lx\n", hr);
    init_media_type(media_type, output_type_desc, -1);
    hr = IMFTransform_SetOutputType(transform, 0, media_type, 0);
    ok(hr == MF_E_INVALIDMEDIATYPE, "SetOutputType returned %#lx.\n", hr);
    init_media_type(media_type, new_output_type_desc, -1);
    hr = IMFTransform_SetOutputType(transform, 0, media_type, 0);
    ok(hr == S_OK, "SetOutputType returned %#lx.\n", hr);
    ret = IMFMediaType_Release(media_type);
    ok(ret == 1, "Release returned %lu\n", ret);

    hr = IMFTransform_GetOutputCurrentType(transform, 0, &media_type);
    ok(hr == S_OK, "GetOutputCurrentType returned %#lx\n", hr);
    check_media_type(media_type, new_output_type_desc, -1);
    hr = IMFMediaType_GetItemType(media_type, &MF_MT_MINIMUM_DISPLAY_APERTURE, NULL);
    ok(hr == MF_E_ATTRIBUTENOTFOUND, "GetItemType returned %#lx\n", hr);
    ret = IMFMediaType_Release(media_type);
    ok(ret == 0, "Release returned %lu\n", ret);

    output_sample = create_sample(NULL, actual_width * actual_height * 2);
    hr = check_mft_process_output(transform, output_sample, &output_status);
    todo_wine
    ok(hr == MF_E_TRANSFORM_STREAM_CHANGE, "ProcessOutput returned %#lx\n", hr);
    todo_wine
    ok(output_status == MFT_OUTPUT_DATA_BUFFER_FORMAT_CHANGE, "got output[0].dwStatus %#lx\n", output_status);

    while (hr == MF_E_TRANSFORM_NEED_MORE_INPUT)
    {
        ok(output_status == 0, "got output[0].dwStatus %#lx\n", output_status);
        hr = IMFTransform_ProcessInput(transform, 0, input_sample, 0);
        ok(hr == S_OK, "ProcessInput returned %#lx\n", hr);
        ret = IMFSample_Release(input_sample);
        ok(ret <= 1, "Release returned %lu\n", ret);
        input_sample = next_h264_sample(&h264_encoded_data, &h264_encoded_data_len);
        hr = check_mft_process_output(transform, output_sample, &output_status);
    }

    ok(hr == MF_E_TRANSFORM_STREAM_CHANGE, "ProcessOutput returned %#lx\n", hr);
    ok(output_status == MFT_OUTPUT_DATA_BUFFER_FORMAT_CHANGE, "got output[0].dwStatus %#lx\n", output_status);

    hr = IMFSample_GetTotalLength(output_sample, &length);
    ok(hr == S_OK, "GetTotalLength returned %#lx\n", hr);
    ok(length == 0, "got length %lu\n", length);
    ret = IMFSample_Release(output_sample);
    ok(ret == 0, "Release returned %lu\n", ret);

    hr = MFCreateCollection(&output_samples);
    ok(hr == S_OK, "MFCreateCollection returned %#lx\n", hr);

    output_sample = create_sample(NULL, actual_width * actual_height * 2);
    hr = check_mft_process_output(transform, output_sample, &output_status);
    ok(hr == S_OK, "ProcessOutput returned %#lx\n", hr);
    ok(output_status == 0, "got output[0].dwStatus %#lx\n", output_status);
    hr = IMFCollection_AddElement(output_samples, (IUnknown *)output_sample);
    ok(hr == S_OK, "AddElement returned %#lx\n", hr);
    ref = IMFSample_Release(output_sample);
    ok(ref == 1, "Release returned %ld\n", ref);

    ret = check_mf_sample_collection(output_samples, &expect_output_sample_i420, L"i420frame.bmp");
    ok(ret == 0, "got %lu%% diff\n", ret);
    IMFCollection_Release(output_samples);

    output_sample = create_sample(NULL, actual_width * actual_height * 2);
    hr = check_mft_process_output(transform, output_sample, &output_status);
    todo_wine_if(hr == S_OK)  /* when VA-API plugin is used */
    ok(hr == MF_E_TRANSFORM_NEED_MORE_INPUT, "ProcessOutput returned %#lx\n", hr);
    ok(output_status == 0, "got output[0].dwStatus %#lx\n", output_status);
    ret = IMFSample_Release(output_sample);
    ok(ret == 0, "Release returned %lu\n", ret);

    ret = IMFTransform_Release(transform);
    ok(ret == 0, "Release returned %lu\n", ret);
    ret = IMFSample_Release(input_sample);
    ok(ret == 0, "Release returned %lu\n", ret);

failed:
    winetest_pop_context();
    CoUninitialize();
}

static void test_audio_convert(void)
{
    const GUID *const class_id = &CLSID_CResamplerMediaObject;
    const struct transform_info expect_mft_info =
    {
        .name = L"Resampler MFT",
        .major_type = &MFMediaType_Audio,
        .inputs =
        {
            {.subtype = &MFAudioFormat_PCM},
            {.subtype = &MFAudioFormat_Float},
        },
        .outputs =
        {
            {.subtype = &MFAudioFormat_PCM},
            {.subtype = &MFAudioFormat_Float},
        },
    };
    const struct transform_info expect_dmo_info =
    {
        .name = L"Resampler DMO",
        .major_type = &MEDIATYPE_Audio,
        .inputs =
        {
            {.subtype = &MEDIASUBTYPE_PCM},
            {.subtype = &MEDIASUBTYPE_IEEE_FLOAT},
        },
        .outputs =
        {
            {.subtype = &MEDIASUBTYPE_PCM},
            {.subtype = &MEDIASUBTYPE_IEEE_FLOAT},
        },
    };

    static const media_type_desc expect_available_inputs[] =
    {
        {
            ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio),
            ATTR_GUID(MF_MT_SUBTYPE, MFAudioFormat_Float),
            ATTR_UINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, 1),
        },
        {
            ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio),
            ATTR_GUID(MF_MT_SUBTYPE, MFAudioFormat_PCM),
            ATTR_UINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, 1),
        },
    };
    static const media_type_desc expect_available_outputs[] =
    {
        {
            ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio),
            ATTR_GUID(MF_MT_SUBTYPE, MFAudioFormat_Float),
            ATTR_UINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, 1),
        },
        {
            ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio),
            ATTR_GUID(MF_MT_SUBTYPE, MFAudioFormat_PCM),
            ATTR_UINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, 1),
        },
        {
            ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio),
            ATTR_GUID(MF_MT_SUBTYPE, MFAudioFormat_Float),
            ATTR_UINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 32),
            ATTR_UINT32(MF_MT_AUDIO_NUM_CHANNELS, 2),
            ATTR_UINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 48000),
            ATTR_UINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 384000),
            ATTR_UINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 8),
            ATTR_UINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, 1),
            ATTR_UINT32(MF_MT_AUDIO_PREFER_WAVEFORMATEX, 1),
        },
        {
            ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio),
            ATTR_GUID(MF_MT_SUBTYPE, MFAudioFormat_PCM),
            ATTR_UINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16),
            ATTR_UINT32(MF_MT_AUDIO_NUM_CHANNELS, 2),
            ATTR_UINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 48000),
            ATTR_UINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 192000),
            ATTR_UINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 4),
            ATTR_UINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, 1),
            ATTR_UINT32(MF_MT_AUDIO_PREFER_WAVEFORMATEX, 1),
        },
    };

    static const struct attribute_desc input_type_desc[] =
    {
        ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio, .required = TRUE),
        ATTR_GUID(MF_MT_SUBTYPE, MFAudioFormat_Float, .required = TRUE),
        ATTR_UINT32(MF_MT_AUDIO_NUM_CHANNELS, 2, .required = TRUE),
        ATTR_UINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 32, .required = TRUE),
        ATTR_UINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 22050, .required = TRUE),
        ATTR_UINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 2 * (32 / 8), .required = TRUE),
        ATTR_UINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 2 * (32 / 8) * 22050, .required = TRUE),
        {0},
    };
    const struct attribute_desc output_type_desc[] =
    {
        ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio, .required = TRUE),
        ATTR_GUID(MF_MT_SUBTYPE, MFAudioFormat_PCM, .required = TRUE),
        ATTR_UINT32(MF_MT_AUDIO_NUM_CHANNELS, 2, .required = TRUE),
        ATTR_UINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16, .required = TRUE),
        ATTR_UINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 44100, .required = TRUE),
        ATTR_UINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 2 * (16 / 8), .required = TRUE),
        ATTR_UINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 2 * (16 / 8) * 44100, .required = TRUE),
        {0},
    };

    static const ULONG audioconv_block_size = 0x4000;
    const struct buffer_desc output_buffer_desc[] =
    {
        {.length = audioconv_block_size, .compare = compare_pcm16},
        {.length = 0x3dd8, .compare = compare_pcm16, .todo_length = TRUE},
        {.length = 0xfc, .compare = compare_pcm16},
    };
    const struct attribute_desc output_sample_attributes[] =
    {
        ATTR_UINT32(mft_output_sample_incomplete, 1),
        ATTR_UINT32(MFSampleExtension_CleanPoint, has_video_processor /* 0 on Win7 */, .todo = TRUE),
        {0},
    };
    const struct sample_desc output_sample_desc[] =
    {
        {
            .attributes = output_sample_attributes + 0,
            .sample_time = 0, .sample_duration = 928798,
            .buffer_count = 1, .buffers = output_buffer_desc + 0, .repeat_count = 9,
        },
        {
            .attributes = output_sample_attributes + 1, /* not MFT_OUTPUT_DATA_BUFFER_INCOMPLETE */
            .sample_time = 9287980, .sample_duration = 897506,
            .buffer_count = 1, .buffers = output_buffer_desc + 1, .todo_length = TRUE,
        },
        {
            .attributes = output_sample_attributes + 0,
            .sample_time = 10185486, .sample_duration = 14286,
            .buffer_count = 1, .buffers = output_buffer_desc + 2,
        },
    };

    MFT_REGISTER_TYPE_INFO output_type = {MFMediaType_Audio, MFAudioFormat_PCM};
    MFT_REGISTER_TYPE_INFO input_type = {MFMediaType_Audio, MFAudioFormat_Float};
    IMFSample *input_sample, *output_sample;
    MFT_OUTPUT_STREAM_INFO output_info;
    MFT_INPUT_STREAM_INFO input_info;
    IMFCollection *output_samples;
    DWORD length, output_status;
    IMFMediaType *media_type;
    IMFTransform *transform;
    const BYTE *audio_data;
    ULONG audio_data_len;
    ULONG i, ret, ref;
    HRESULT hr;

    hr = CoInitialize(NULL);
    ok(hr == S_OK, "Failed to initialize, hr %#lx.\n", hr);

    winetest_push_context("resampler");

    if (!check_mft_enum(MFT_CATEGORY_AUDIO_EFFECT, &input_type, &output_type, class_id))
        goto failed;
    check_mft_get_info(class_id, &expect_mft_info);
    check_dmo_get_info(class_id, &expect_dmo_info);

    if (FAILED(hr = CoCreateInstance(class_id, NULL, CLSCTX_INPROC_SERVER,
            &IID_IMFTransform, (void **)&transform)))
        goto failed;

    check_interface(transform, &IID_IMFTransform, TRUE);
    check_interface(transform, &IID_IMediaObject, TRUE);
    check_interface(transform, &IID_IPropertyStore, TRUE);
    check_interface(transform, &IID_IPropertyBag, TRUE);
    /* check_interface(transform, &IID_IWMResamplerProps, TRUE); */

    /* check default media types */

    hr = IMFTransform_GetInputStreamInfo(transform, 0, &input_info);
    ok(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "GetInputStreamInfo returned %#lx\n", hr);
    hr = IMFTransform_GetOutputStreamInfo(transform, 0, &output_info);
    ok(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "GetOutputStreamInfo returned %#lx\n", hr);

    i = -1;
    while (SUCCEEDED(hr = IMFTransform_GetOutputAvailableType(transform, 0, ++i, &media_type)))
    {
        winetest_push_context("out %lu", i);
        ok(hr == S_OK, "GetOutputAvailableType returned %#lx\n", hr);
        check_media_type(media_type, expect_available_outputs[i], -1);
        ret = IMFMediaType_Release(media_type);
        ok(ret == 0, "Release returned %lu\n", ret);
        winetest_pop_context();
    }
    ok(hr == MF_E_NO_MORE_TYPES, "GetOutputAvailableType returned %#lx\n", hr);
    ok(i == 4, "%lu output media types\n", i);

    i = -1;
    while (SUCCEEDED(hr = IMFTransform_GetInputAvailableType(transform, 0, ++i, &media_type)))
    {
        winetest_push_context("in %lu", i);
        ok(hr == S_OK, "GetInputAvailableType returned %#lx\n", hr);
        check_media_type(media_type, expect_available_inputs[i], -1);
        hr = IMFTransform_SetInputType(transform, 0, media_type, 0);
        ok(hr == MF_E_INVALIDMEDIATYPE, "SetInputType returned %#lx.\n", hr);
        ret = IMFMediaType_Release(media_type);
        ok(ret == 0, "Release returned %lu\n", ret);
        winetest_pop_context();
    }
    ok(hr == MF_E_NO_MORE_TYPES, "GetInputAvailableType returned %#lx\n", hr);
    ok(i == 2, "%lu input media types\n", i);

    /* setting output media type first doesn't work */

    hr = MFCreateMediaType(&media_type);
    ok(hr == S_OK, "MFCreateMediaType returned %#lx\n", hr);
    init_media_type(media_type, output_type_desc, -1);
    hr = IMFTransform_SetOutputType(transform, 0, media_type, 0);
    ok(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "SetOutputType returned %#lx.\n", hr);
    ret = IMFMediaType_Release(media_type);
    ok(ret == 0, "Release returned %lu\n", ret);

    check_mft_set_input_type_required(transform, input_type_desc);

    hr = MFCreateMediaType(&media_type);
    ok(hr == S_OK, "MFCreateMediaType returned %#lx\n", hr);
    init_media_type(media_type, input_type_desc, -1);
    hr = IMFTransform_SetInputType(transform, 0, media_type, 0);
    ok(hr == S_OK, "SetInputType returned %#lx.\n", hr);
    ret = IMFMediaType_Release(media_type);
    ok(ret == 0, "Release returned %lu\n", ret);

    hr = IMFTransform_GetInputStreamInfo(transform, 0, &input_info);
    ok(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "GetInputStreamInfo returned %#lx\n", hr);
    hr = IMFTransform_GetOutputStreamInfo(transform, 0, &output_info);
    ok(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "GetOutputStreamInfo returned %#lx\n", hr);

    /* check new output media types */

    i = -1;
    while (SUCCEEDED(hr = IMFTransform_GetOutputAvailableType(transform, 0, ++i, &media_type)))
    {
        winetest_push_context("out %lu", i);
        ok(hr == S_OK, "GetOutputAvailableType returned %#lx\n", hr);
        check_media_type(media_type, expect_available_outputs[i], -1);
        ret = IMFMediaType_Release(media_type);
        ok(ret == 0, "Release returned %lu\n", ret);
        winetest_pop_context();
    }
    ok(hr == MF_E_NO_MORE_TYPES, "GetOutputAvailableType returned %#lx\n", hr);
    ok(i == 4, "%lu output media types\n", i);

    check_mft_set_output_type_required(transform, output_type_desc);

    hr = MFCreateMediaType(&media_type);
    ok(hr == S_OK, "MFCreateMediaType returned %#lx\n", hr);
    init_media_type(media_type, output_type_desc, -1);
    hr = IMFTransform_SetOutputType(transform, 0, media_type, 0);
    ok(hr == S_OK, "SetOutputType returned %#lx.\n", hr);
    ret = IMFMediaType_Release(media_type);
    ok(ret == 0, "Release returned %lu\n", ret);

    memset(&input_info, 0xcd, sizeof(input_info));
    hr = IMFTransform_GetInputStreamInfo(transform, 0, &input_info);
    ok(hr == S_OK, "GetInputStreamInfo returned %#lx\n", hr);
    ok(input_info.hnsMaxLatency == 0, "got hnsMaxLatency %s\n", wine_dbgstr_longlong(input_info.hnsMaxLatency));
    ok(input_info.dwFlags == 0, "got dwFlags %#lx\n", input_info.dwFlags);
    ok(input_info.cbSize == 8, "got cbSize %lu\n", input_info.cbSize);
    ok(input_info.cbMaxLookahead == 0, "got cbMaxLookahead %#lx\n", input_info.cbMaxLookahead);
    ok(input_info.cbAlignment == 1, "got cbAlignment %#lx\n", input_info.cbAlignment);

    memset(&output_info, 0xcd, sizeof(output_info));
    hr = IMFTransform_GetOutputStreamInfo(transform, 0, &output_info);
    ok(hr == S_OK, "GetOutputStreamInfo returned %#lx\n", hr);
    ok(output_info.dwFlags == 0, "got dwFlags %#lx\n", output_info.dwFlags);
    ok(output_info.cbSize == 4, "got cbSize %#lx\n", output_info.cbSize);
    ok(output_info.cbAlignment == 1, "got cbAlignment %#lx\n", output_info.cbAlignment);

    load_resource(L"audiodata.bin", &audio_data, &audio_data_len);
    ok(audio_data_len == 179928, "got length %lu\n", audio_data_len);

    input_sample = create_sample(audio_data, audio_data_len);
    hr = IMFSample_SetSampleTime(input_sample, 0);
    ok(hr == S_OK, "SetSampleTime returned %#lx\n", hr);
    hr = IMFSample_SetSampleDuration(input_sample, 10000000);
    ok(hr == S_OK, "SetSampleDuration returned %#lx\n", hr);
    hr = IMFTransform_ProcessInput(transform, 0, input_sample, 0);
    ok(hr == S_OK, "ProcessInput returned %#lx\n", hr);
    hr = IMFTransform_ProcessMessage(transform, MFT_MESSAGE_COMMAND_DRAIN, 0);
    ok(hr == S_OK, "ProcessMessage returned %#lx\n", hr);
    hr = IMFTransform_ProcessInput(transform, 0, input_sample, 0);
    ok(hr == MF_E_NOTACCEPTING, "ProcessInput returned %#lx\n", hr);
    ret = IMFSample_Release(input_sample);
    ok(ret <= 1, "Release returned %ld\n", ret);

    hr = MFCreateCollection(&output_samples);
    ok(hr == S_OK, "MFCreateCollection returned %#lx\n", hr);

    output_sample = create_sample(NULL, audioconv_block_size);
    for (i = 0; SUCCEEDED(hr = check_mft_process_output(transform, output_sample, &output_status)); i++)
    {
        winetest_push_context("%lu", i);
        ok(hr == S_OK, "ProcessOutput returned %#lx\n", hr);
        ok(!(output_status & ~MFT_OUTPUT_DATA_BUFFER_INCOMPLETE), "got output[0].dwStatus %#lx\n", output_status);
        hr = IMFCollection_AddElement(output_samples, (IUnknown *)output_sample);
        ok(hr == S_OK, "AddElement returned %#lx\n", hr);
        ref = IMFSample_Release(output_sample);
        ok(ref == 1, "Release returned %ld\n", ref);
        output_sample = create_sample(NULL, audioconv_block_size);
        winetest_pop_context();
    }
    ok(hr == MF_E_TRANSFORM_NEED_MORE_INPUT, "ProcessOutput returned %#lx\n", hr);
    ok(output_status == 0, "got output[0].dwStatus %#lx\n", output_status);
    ret = IMFSample_Release(output_sample);
    ok(ret == 0, "Release returned %lu\n", ret);
    todo_wine
    ok(i == 12 || broken(i == 11) /* Win7 */, "got %lu output samples\n", i);

    ret = check_mf_sample_collection(output_samples, output_sample_desc, L"audioconvdata.bin");
    ok(ret == 0, "got %lu%% diff\n", ret);
    IMFCollection_Release(output_samples);

    output_sample = create_sample(NULL, audioconv_block_size);
    hr = check_mft_process_output(transform, output_sample, &output_status);
    ok(hr == MF_E_TRANSFORM_NEED_MORE_INPUT, "ProcessOutput returned %#lx\n", hr);
    ok(output_status == 0, "got output[0].dwStatus %#lx\n", output_status);
    hr = IMFSample_GetTotalLength(output_sample, &length);
    ok(hr == S_OK, "GetTotalLength returned %#lx\n", hr);
    ok(length == 0, "got length %lu\n", length);
    ret = IMFSample_Release(output_sample);
    ok(ret == 0, "Release returned %lu\n", ret);

    ret = IMFTransform_Release(transform);
    ok(ret == 0, "Release returned %lu\n", ret);

failed:
    winetest_pop_context();
    CoUninitialize();
}

static void test_color_convert(void)
{
    const GUID *const class_id = &CLSID_CColorConvertDMO;
    const struct transform_info expect_mft_info =
    {
        .name = L"Color Converter MFT",
        .major_type = &MFMediaType_Video,
        .inputs =
        {
            {.subtype = &MFVideoFormat_YV12},
            {.subtype = &MFVideoFormat_YUY2},
            {.subtype = &MFVideoFormat_UYVY},
            {.subtype = &MFVideoFormat_AYUV},
            {.subtype = &MFVideoFormat_NV12},
            {.subtype = &DMOVideoFormat_RGB32},
            {.subtype = &DMOVideoFormat_RGB565},
            {.subtype = &MFVideoFormat_I420},
            {.subtype = &MFVideoFormat_IYUV},
            {.subtype = &MFVideoFormat_YVYU},
            {.subtype = &DMOVideoFormat_RGB24},
            {.subtype = &DMOVideoFormat_RGB555},
            {.subtype = &DMOVideoFormat_RGB8},
            {.subtype = &MEDIASUBTYPE_V216},
            {.subtype = &MEDIASUBTYPE_V410},
            {.subtype = &MFVideoFormat_NV11},
            {.subtype = &MFVideoFormat_Y41P},
            {.subtype = &MFVideoFormat_Y41T},
            {.subtype = &MFVideoFormat_Y42T},
            {.subtype = &MFVideoFormat_YVU9},
        },
        .outputs =
        {
            {.subtype = &MFVideoFormat_YV12},
            {.subtype = &MFVideoFormat_YUY2},
            {.subtype = &MFVideoFormat_UYVY},
            {.subtype = &MFVideoFormat_AYUV},
            {.subtype = &MFVideoFormat_NV12},
            {.subtype = &DMOVideoFormat_RGB32},
            {.subtype = &DMOVideoFormat_RGB565},
            {.subtype = &MFVideoFormat_I420},
            {.subtype = &MFVideoFormat_IYUV},
            {.subtype = &MFVideoFormat_YVYU},
            {.subtype = &DMOVideoFormat_RGB24},
            {.subtype = &DMOVideoFormat_RGB555},
            {.subtype = &DMOVideoFormat_RGB8},
            {.subtype = &MEDIASUBTYPE_V216},
            {.subtype = &MEDIASUBTYPE_V410},
            {.subtype = &MFVideoFormat_NV11},
        },
    };
    const struct transform_info expect_dmo_info =
    {
        .name = L"Color Converter DMO",
        .major_type = &MEDIATYPE_Video,
        .inputs =
        {
            {.subtype = &MEDIASUBTYPE_YV12},
            {.subtype = &MEDIASUBTYPE_YUY2},
            {.subtype = &MEDIASUBTYPE_UYVY},
            {.subtype = &MEDIASUBTYPE_AYUV},
            {.subtype = &MEDIASUBTYPE_NV12},
            {.subtype = &MEDIASUBTYPE_RGB32},
            {.subtype = &MEDIASUBTYPE_RGB565},
            {.subtype = &MEDIASUBTYPE_I420},
            {.subtype = &MEDIASUBTYPE_IYUV},
            {.subtype = &MEDIASUBTYPE_YVYU},
            {.subtype = &MEDIASUBTYPE_RGB24},
            {.subtype = &MEDIASUBTYPE_RGB555},
            {.subtype = &MEDIASUBTYPE_RGB8},
            {.subtype = &MEDIASUBTYPE_V216},
            {.subtype = &MEDIASUBTYPE_V410},
            {.subtype = &MEDIASUBTYPE_NV11},
            {.subtype = &MEDIASUBTYPE_Y41P},
            {.subtype = &MEDIASUBTYPE_Y41T},
            {.subtype = &MEDIASUBTYPE_Y42T},
            {.subtype = &MEDIASUBTYPE_YVU9},
        },
        .outputs =
        {
            {.subtype = &MEDIASUBTYPE_YV12},
            {.subtype = &MEDIASUBTYPE_YUY2},
            {.subtype = &MEDIASUBTYPE_UYVY},
            {.subtype = &MEDIASUBTYPE_AYUV},
            {.subtype = &MEDIASUBTYPE_NV12},
            {.subtype = &MEDIASUBTYPE_RGB32},
            {.subtype = &MEDIASUBTYPE_RGB565},
            {.subtype = &MEDIASUBTYPE_I420},
            {.subtype = &MEDIASUBTYPE_IYUV},
            {.subtype = &MEDIASUBTYPE_YVYU},
            {.subtype = &MEDIASUBTYPE_RGB24},
            {.subtype = &MEDIASUBTYPE_RGB555},
            {.subtype = &MEDIASUBTYPE_RGB8},
            {.subtype = &MEDIASUBTYPE_V216},
            {.subtype = &MEDIASUBTYPE_V410},
            {.subtype = &MEDIASUBTYPE_NV11},
        },
    };

    static const media_type_desc expect_available_inputs[20] =
    {
        { ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_YV12), },
        { ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_YUY2), },
        { ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_UYVY), },
        { ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_AYUV), },
        { ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_NV12), },
        { ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32), },
        { ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_RGB565), },
        { ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_I420), },
        { ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_IYUV), },
        { ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_YVYU), },
        { ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_RGB24), },
        { ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_RGB555), },
        { ATTR_GUID(MF_MT_SUBTYPE, MEDIASUBTYPE_RGB8), },
        { ATTR_GUID(MF_MT_SUBTYPE, MEDIASUBTYPE_V216), },
        { ATTR_GUID(MF_MT_SUBTYPE, MEDIASUBTYPE_V410), },
        { ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_NV11), },
        { ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_Y41P), },
        { ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_Y41T), },
        { ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_Y42T), },
        { ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_YVU9), },
    };
    static const media_type_desc expect_available_outputs[16] =
    {
        { ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_YV12), },
        { ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_YUY2), },
        { ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_UYVY), },
        { ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_AYUV), },
        { ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_NV12), },
        { ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32), },
        { ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_RGB565), },
        { ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_I420), },
        { ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_IYUV), },
        { ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_YVYU), },
        { ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_RGB24), },
        { ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_RGB555), },
        { ATTR_GUID(MF_MT_SUBTYPE, MEDIASUBTYPE_RGB8), },
        { ATTR_GUID(MF_MT_SUBTYPE, MEDIASUBTYPE_V216), },
        { ATTR_GUID(MF_MT_SUBTYPE, MEDIASUBTYPE_V410), },
        { ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_NV11), },
    };
    static const media_type_desc expect_available_common =
    {
        ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Video),
        ATTR_UINT32(MF_MT_FIXED_SIZE_SAMPLES, 1),
        ATTR_UINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, 1),
    };

    static const MFVideoArea actual_aperture = {.Area={82,84}};
    static const DWORD actual_width = 96, actual_height = 96;
    const struct attribute_desc input_type_desc[] =
    {
        ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Video, .required = TRUE),
        ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_NV12, .required = TRUE),
        ATTR_RATIO(MF_MT_FRAME_SIZE, actual_width, actual_height, .required = TRUE),
        ATTR_BLOB(MF_MT_MINIMUM_DISPLAY_APERTURE, &actual_aperture, 16),
        {0},
    };
    const struct attribute_desc output_type_desc[] =
    {
        ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Video, .required = TRUE),
        ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32, .required = TRUE),
        ATTR_RATIO(MF_MT_FRAME_SIZE, actual_width, actual_height, .required = TRUE),
        {0},
    };

    const struct buffer_desc output_buffer_desc =
    {
        .length = actual_width * actual_height * 4,
        .compare = compare_rgb32, .dump = dump_rgb32, .rect = {.right = 82, .bottom = 84},
    };
    const struct attribute_desc output_sample_attributes[] =
    {
        ATTR_UINT32(MFSampleExtension_CleanPoint, 0, .todo = TRUE),
        {0},
    };
    const struct sample_desc output_sample_desc =
    {
        .attributes = output_sample_attributes,
        .sample_time = 0, .sample_duration = 10000000,
        .buffer_count = 1, .buffers = &output_buffer_desc,
    };

    MFT_REGISTER_TYPE_INFO output_type = {MFMediaType_Video, MFVideoFormat_NV12};
    MFT_REGISTER_TYPE_INFO input_type = {MFMediaType_Video, MFVideoFormat_I420};
    IMFSample *input_sample, *output_sample;
    MFT_OUTPUT_STREAM_INFO output_info;
    MFT_INPUT_STREAM_INFO input_info;
    IMFCollection *output_samples;
    DWORD length, output_status;
    const BYTE *nv12frame_data;
    ULONG nv12frame_data_len;
    IMFMediaType *media_type;
    IMFTransform *transform;
    ULONG i, ret, ref;
    HRESULT hr;

    hr = CoInitialize(NULL);
    ok(hr == S_OK, "Failed to initialize, hr %#lx.\n", hr);

    winetest_push_context("colorconv");

    if (!check_mft_enum(MFT_CATEGORY_VIDEO_EFFECT, &input_type, &output_type, class_id))
        goto failed;
    check_mft_get_info(class_id, &expect_mft_info);
    check_dmo_get_info(class_id, &expect_dmo_info);

    if (FAILED(hr = CoCreateInstance(class_id, NULL, CLSCTX_INPROC_SERVER,
            &IID_IMFTransform, (void **)&transform)))
        goto failed;

    check_interface(transform, &IID_IMFTransform, TRUE);
    check_interface(transform, &IID_IMediaObject, TRUE);
    check_interface(transform, &IID_IPropertyStore, TRUE);
    todo_wine
    check_interface(transform, &IID_IMFRealTimeClient, TRUE);
    /* check_interface(transform, &IID_IWMColorConvProps, TRUE); */

    /* check default media types */

    hr = IMFTransform_GetInputStreamInfo(transform, 0, &input_info);
    ok(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "GetInputStreamInfo returned %#lx\n", hr);
    hr = IMFTransform_GetOutputStreamInfo(transform, 0, &output_info);
    ok(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "GetOutputStreamInfo returned %#lx\n", hr);

    i = -1;
    while (SUCCEEDED(hr = IMFTransform_GetOutputAvailableType(transform, 0, ++i, &media_type)))
    {
        winetest_push_context("out %lu", i);
        ok(hr == S_OK, "GetOutputAvailableType returned %#lx\n", hr);
        check_media_type(media_type, expect_available_common, -1);
        check_media_type(media_type, expect_available_outputs[i], -1);
        ret = IMFMediaType_Release(media_type);
        ok(ret == 0, "Release returned %lu\n", ret);
        winetest_pop_context();
    }
    ok(hr == MF_E_NO_MORE_TYPES, "GetOutputAvailableType returned %#lx\n", hr);
    ok(i == 16, "%lu output media types\n", i);

    i = -1;
    while (SUCCEEDED(hr = IMFTransform_GetInputAvailableType(transform, 0, ++i, &media_type)))
    {
        winetest_push_context("in %lu", i);
        ok(hr == S_OK, "GetInputAvailableType returned %#lx\n", hr);
        check_media_type(media_type, expect_available_common, -1);
        check_media_type(media_type, expect_available_inputs[i], -1);
        hr = IMFTransform_SetInputType(transform, 0, media_type, 0);
        if (i == 12)
        {
            todo_wine
            ok(hr == MF_E_INVALIDMEDIATYPE, "SetInputType returned %#lx.\n", hr);
        }
        else
            ok(hr == E_INVALIDARG, "SetInputType returned %#lx.\n", hr);
        ret = IMFMediaType_Release(media_type);
        ok(ret == 0, "Release returned %lu\n", ret);
        winetest_pop_context();
    }
    ok(hr == MF_E_NO_MORE_TYPES, "GetInputAvailableType returned %#lx\n", hr);
    ok(i == 20, "%lu input media types\n", i);

    check_mft_set_output_type_required(transform, output_type_desc);

    hr = MFCreateMediaType(&media_type);
    ok(hr == S_OK, "MFCreateMediaType returned %#lx\n", hr);
    init_media_type(media_type, output_type_desc, -1);
    hr = IMFTransform_SetOutputType(transform, 0, media_type, 0);
    ok(hr == S_OK, "SetOutputType returned %#lx.\n", hr);
    ret = IMFMediaType_Release(media_type);
    ok(ret == 0, "Release returned %lu\n", ret);

    check_mft_set_input_type_required(transform, input_type_desc);

    hr = MFCreateMediaType(&media_type);
    ok(hr == S_OK, "MFCreateMediaType returned %#lx\n", hr);
    init_media_type(media_type, input_type_desc, -1);
    hr = IMFTransform_SetInputType(transform, 0, media_type, 0);
    ok(hr == S_OK, "SetInputType returned %#lx.\n", hr);
    ret = IMFMediaType_Release(media_type);
    ok(ret == 0, "Release returned %lu\n", ret);

    memset(&input_info, 0xcd, sizeof(input_info));
    hr = IMFTransform_GetInputStreamInfo(transform, 0, &input_info);
    ok(hr == S_OK, "GetInputStreamInfo returned %#lx\n", hr);
    ok(input_info.hnsMaxLatency == 0, "got hnsMaxLatency %s\n", wine_dbgstr_longlong(input_info.hnsMaxLatency));
    ok(input_info.dwFlags == 0, "got dwFlags %#lx\n", input_info.dwFlags);
    ok(input_info.cbSize == actual_width * actual_height * 3 / 2, "got cbSize %#lx\n", input_info.cbSize);
    ok(input_info.cbMaxLookahead == 0, "got cbMaxLookahead %#lx\n", input_info.cbMaxLookahead);
    ok(input_info.cbAlignment == 1, "got cbAlignment %#lx\n", input_info.cbAlignment);

    memset(&output_info, 0xcd, sizeof(output_info));
    hr = IMFTransform_GetOutputStreamInfo(transform, 0, &output_info);
    ok(hr == S_OK, "GetOutputStreamInfo returned %#lx\n", hr);
    ok(output_info.dwFlags == 0, "got dwFlags %#lx\n", output_info.dwFlags);
    ok(output_info.cbSize == actual_width * actual_height * 4, "got cbSize %#lx\n", output_info.cbSize);
    ok(output_info.cbAlignment == 1, "got cbAlignment %#lx\n", output_info.cbAlignment);

    load_resource(L"nv12frame.bmp", &nv12frame_data, &nv12frame_data_len);
    /* skip BMP header and RGB data from the dump */
    length = *(DWORD *)(nv12frame_data + 2);
    nv12frame_data_len = nv12frame_data_len - length;
    nv12frame_data = nv12frame_data + length;
    ok(nv12frame_data_len == 13824, "got length %lu\n", nv12frame_data_len);

    input_sample = create_sample(nv12frame_data, nv12frame_data_len);
    hr = IMFSample_SetSampleTime(input_sample, 0);
    ok(hr == S_OK, "SetSampleTime returned %#lx\n", hr);
    hr = IMFSample_SetSampleDuration(input_sample, 10000000);
    ok(hr == S_OK, "SetSampleDuration returned %#lx\n", hr);
    hr = IMFTransform_ProcessInput(transform, 0, input_sample, 0);
    ok(hr == S_OK, "ProcessInput returned %#lx\n", hr);
    hr = IMFTransform_ProcessInput(transform, 0, input_sample, 0);
    ok(hr == MF_E_NOTACCEPTING, "ProcessInput returned %#lx\n", hr);
    hr = IMFTransform_ProcessMessage(transform, MFT_MESSAGE_COMMAND_DRAIN, 0);
    ok(hr == S_OK, "ProcessMessage returned %#lx\n", hr);
    ret = IMFSample_Release(input_sample);
    ok(ret <= 1, "Release returned %ld\n", ret);

    hr = MFCreateCollection(&output_samples);
    ok(hr == S_OK, "MFCreateCollection returned %#lx\n", hr);

    output_sample = create_sample(NULL, output_info.cbSize);
    hr = check_mft_process_output(transform, output_sample, &output_status);
    ok(hr == S_OK, "ProcessOutput returned %#lx\n", hr);
    ok(output_status == 0, "got output[0].dwStatus %#lx\n", output_status);
    hr = IMFCollection_AddElement(output_samples, (IUnknown *)output_sample);
    ok(hr == S_OK, "AddElement returned %#lx\n", hr);
    ref = IMFSample_Release(output_sample);
    ok(ref == 1, "Release returned %ld\n", ref);

    ret = check_mf_sample_collection(output_samples, &output_sample_desc, L"rgb32frame.bmp");
    ok(ret <= 4 /* small and harmless diff in Wine vs Windows */, "got %lu%% diff\n", ret);
    IMFCollection_Release(output_samples);

    output_sample = create_sample(NULL, output_info.cbSize);
    hr = check_mft_process_output(transform, output_sample, &output_status);
    ok(hr == MF_E_TRANSFORM_NEED_MORE_INPUT, "ProcessOutput returned %#lx\n", hr);
    ok(output_status == 0, "got output[0].dwStatus %#lx\n", output_status);
    hr = IMFSample_GetTotalLength(output_sample, &length);
    ok(hr == S_OK, "GetTotalLength returned %#lx\n", hr);
    ok(length == 0, "got length %lu\n", length);
    ret = IMFSample_Release(output_sample);
    ok(ret == 0, "Release returned %lu\n", ret);

    ret = IMFTransform_Release(transform);
    ok(ret == 0, "Release returned %ld\n", ret);

failed:
    winetest_pop_context();
    CoUninitialize();
}

static void test_video_processor(void)
{
    const GUID *const class_id = &CLSID_VideoProcessorMFT;
    const struct transform_info expect_mft_info =
    {
        .name = L"Microsoft Video Processor MFT",
        .major_type = &MFMediaType_Video,
        .inputs =
        {
            {.subtype = &MFVideoFormat_IYUV},
            {.subtype = &MFVideoFormat_YV12},
            {.subtype = &MFVideoFormat_NV12},
            {.subtype = &MFVideoFormat_YUY2},
            {.subtype = &MFVideoFormat_ARGB32},
            {.subtype = &MFVideoFormat_RGB32},
            {.subtype = &MFVideoFormat_NV11},
            {.subtype = &MFVideoFormat_AYUV},
            {.subtype = &MFVideoFormat_UYVY},
            {.subtype = &MEDIASUBTYPE_P208},
            {.subtype = &MFVideoFormat_RGB24},
            {.subtype = &MFVideoFormat_RGB555},
            {.subtype = &MFVideoFormat_RGB565},
            {.subtype = &MFVideoFormat_RGB8},
            {.subtype = &MFVideoFormat_I420},
            {.subtype = &MFVideoFormat_Y216},
            {.subtype = &MFVideoFormat_v410},
            {.subtype = &MFVideoFormat_Y41P},
            {.subtype = &MFVideoFormat_Y41T},
            {.subtype = &MFVideoFormat_Y42T},
            {.subtype = &MFVideoFormat_YVYU},
            {.subtype = &MFVideoFormat_420O},
        },
        .outputs =
        {
            {.subtype = &MFVideoFormat_IYUV},
            {.subtype = &MFVideoFormat_YV12},
            {.subtype = &MFVideoFormat_NV12},
            {.subtype = &MFVideoFormat_YUY2},
            {.subtype = &MFVideoFormat_ARGB32},
            {.subtype = &MFVideoFormat_RGB32},
            {.subtype = &MFVideoFormat_NV11},
            {.subtype = &MFVideoFormat_AYUV},
            {.subtype = &MFVideoFormat_UYVY},
            {.subtype = &MEDIASUBTYPE_P208},
            {.subtype = &MFVideoFormat_RGB24},
            {.subtype = &MFVideoFormat_RGB555},
            {.subtype = &MFVideoFormat_RGB565},
            {.subtype = &MFVideoFormat_RGB8},
            {.subtype = &MFVideoFormat_I420},
            {.subtype = &MFVideoFormat_Y216},
            {.subtype = &MFVideoFormat_v410},
            {.subtype = &MFVideoFormat_Y41P},
            {.subtype = &MFVideoFormat_Y41T},
            {.subtype = &MFVideoFormat_Y42T},
            {.subtype = &MFVideoFormat_YVYU},
        },
    };
    const GUID expect_available_inputs_w8[] =
    {
        MFVideoFormat_IYUV,
        MFVideoFormat_YV12,
        MFVideoFormat_NV12,
        MFVideoFormat_420O,
        MFVideoFormat_UYVY,
        MFVideoFormat_YUY2,
        MFVideoFormat_P208,
        MFVideoFormat_NV11,
        MFVideoFormat_AYUV,
        MFVideoFormat_ARGB32,
        MFVideoFormat_RGB32,
        MFVideoFormat_RGB24,
        MFVideoFormat_I420,
        MFVideoFormat_YVYU,
        MFVideoFormat_RGB555,
        MFVideoFormat_RGB565,
        MFVideoFormat_RGB8,
        MFVideoFormat_Y216,
        MFVideoFormat_v410,
        MFVideoFormat_Y41P,
        MFVideoFormat_Y41T,
        MFVideoFormat_Y42T,
    };
    const GUID expect_available_inputs_w10[] =
    {
        MFVideoFormat_L8,
        MFVideoFormat_L16,
        MFAudioFormat_MPEG,
        MFVideoFormat_IYUV,
        MFVideoFormat_YV12,
        MFVideoFormat_NV12,
        MFVideoFormat_420O,
        MFVideoFormat_P010,
        MFVideoFormat_P016,
        MFVideoFormat_UYVY,
        MFVideoFormat_YUY2,
        MFVideoFormat_P208,
        MFVideoFormat_NV11,
        MFVideoFormat_AYUV,
        MFVideoFormat_ARGB32,
        MFVideoFormat_ABGR32,
        MFVideoFormat_RGB32,
        MFVideoFormat_A2R10G10B10,
        MFVideoFormat_A16B16G16R16F,
        MFVideoFormat_RGB24,
        MFVideoFormat_I420,
        MFVideoFormat_YVYU,
        MFVideoFormat_RGB555,
        MFVideoFormat_RGB565,
        MFVideoFormat_RGB8,
        MFVideoFormat_Y216,
        MFVideoFormat_v410,
        MFVideoFormat_Y41P,
        MFVideoFormat_Y41T,
        MFVideoFormat_Y42T,
    };
    const GUID expect_available_outputs[] =
    {
        MFVideoFormat_A2R10G10B10, /* enumerated with MFVideoFormat_P010 input */
        MFVideoFormat_P010, /* enumerated with MFVideoFormat_A2R10G10B10 input */
        MFVideoFormat_YUY2,
        MFVideoFormat_IYUV,
        MFVideoFormat_I420,
        MFVideoFormat_NV12,
        MFVideoFormat_RGB24,
        MFVideoFormat_ARGB32,
        MFVideoFormat_RGB32,
        MFVideoFormat_YV12,
        MFVideoFormat_Y216, /* enumerated with some input formats */
        MFVideoFormat_UYVY, /* enumerated with some input formats */
        MFVideoFormat_YVYU, /* enumerated with some input formats */
        MFVideoFormat_AYUV,
        MFVideoFormat_RGB555,
        MFVideoFormat_RGB565,
        MFVideoFormat_AYUV, /* some inputs enumerate MFVideoFormat_AYUV after RGB565 */
        MFVideoFormat_NV12, /* P010 enumerates NV12 after (A)RGB32 formats */
        MFVideoFormat_A16B16G16R16F, /* enumerated with MFVideoFormat_P010 input */
    };
    static const media_type_desc expect_available_common =
    {
        ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Video),
    };

    static const MFVideoArea actual_aperture = {.Area={82,84}};
    static const DWORD actual_width = 96, actual_height = 96;
    const struct attribute_desc input_type_desc[] =
    {
        ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Video, .required = TRUE),
        ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_NV12, .required = TRUE),
        ATTR_RATIO(MF_MT_FRAME_SIZE, actual_width, actual_height, .required = TRUE),
        ATTR_BLOB(MF_MT_MINIMUM_DISPLAY_APERTURE, &actual_aperture, 16),
        {0},
    };
    const struct attribute_desc output_type_desc[] =
    {
        ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Video, .required = TRUE),
        ATTR_GUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32, .required = TRUE),
        ATTR_RATIO(MF_MT_FRAME_SIZE, actual_width, actual_height, .required = TRUE),
        ATTR_BLOB(MF_MT_MINIMUM_DISPLAY_APERTURE, &actual_aperture, 16),
        {0},
    };

    const struct buffer_desc output_buffer_desc =
    {
        .length = actual_width * actual_height * 4,
        .compare = compare_rgb32, .dump = dump_rgb32, .rect = {.top = 12, .right = 82, .bottom = 96},
    };
    const struct attribute_desc output_sample_attributes[] =
    {
        ATTR_UINT32(MFSampleExtension_CleanPoint, 1, .todo = TRUE),
        {0},
    };
    const struct sample_desc output_sample_desc =
    {
        .attributes = output_sample_attributes,
        .sample_time = 0, .sample_duration = 10000000,
        .buffer_count = 1, .buffers = &output_buffer_desc,
    };

    DWORD input_count, output_count, input_id, output_id, flags, length, output_status;
    MFT_REGISTER_TYPE_INFO output_type = {MFMediaType_Video, MFVideoFormat_NV12};
    MFT_REGISTER_TYPE_INFO input_type = {MFMediaType_Video, MFVideoFormat_I420};
    DWORD input_min, input_max, output_min, output_max, i, j, k;
    IMFAttributes *attributes, *attributes2;
    IMFSample *input_sample, *output_sample;
    IMFMediaType *media_type, *media_type2;
    const GUID *expect_available_inputs;
    MFT_OUTPUT_STREAM_INFO output_info;
    MFT_INPUT_STREAM_INFO input_info;
    IMFCollection *output_samples;
    const BYTE *nv12frame_data;
    ULONG nv12frame_data_len;
    IMFTransform *transform;
    IMFMediaBuffer *buffer;
    IMFMediaEvent *event;
    unsigned int value;
    UINT32 count;
    HRESULT hr;
    ULONG ret;
    GUID guid;
    LONG ref;

    hr = CoInitialize(NULL);
    ok(hr == S_OK, "Failed to initialize, hr %#lx.\n", hr);

    winetest_push_context("videoproc");

    if (!check_mft_enum(MFT_CATEGORY_VIDEO_PROCESSOR, &input_type, &output_type, class_id))
        goto failed;
    check_mft_get_info(class_id, &expect_mft_info);

    if (FAILED(hr = CoCreateInstance(class_id, NULL, CLSCTX_INPROC_SERVER,
            &IID_IMFTransform, (void **)&transform)))
        goto failed;

    todo_wine
    check_interface(transform, &IID_IMFVideoProcessorControl, TRUE);
    todo_wine
    check_interface(transform, &IID_IMFRealTimeClientEx, TRUE);
    check_interface(transform, &IID_IMFMediaEventGenerator, FALSE);
    check_interface(transform, &IID_IMFShutdown, FALSE);

    /* Transform global attributes. */
    hr = IMFTransform_GetAttributes(transform, &attributes);
    ok(hr == S_OK, "Failed to get attributes, hr %#lx.\n", hr);

    hr = IMFAttributes_GetCount(attributes, &count);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
    todo_wine
    ok(!!count, "Unexpected attribute count %u.\n", count);

    value = 0;
    hr = IMFAttributes_GetUINT32(attributes, &MF_SA_D3D11_AWARE, &value);
todo_wine {
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);
    ok(value == 1, "Unexpected attribute value %u.\n", value);
}
    hr = IMFTransform_GetAttributes(transform, &attributes2);
    ok(hr == S_OK, "Failed to get attributes, hr %#lx.\n", hr);
    ok(attributes == attributes2, "Unexpected instance.\n");
    IMFAttributes_Release(attributes);
    IMFAttributes_Release(attributes2);

    hr = IMFTransform_GetStreamLimits(transform, &input_min, &input_max, &output_min, &output_max);
    ok(hr == S_OK, "Failed to get stream limits, hr %#lx.\n", hr);
    ok(input_min == input_max && input_min == 1 && output_min == output_max && output_min == 1,
            "Unexpected stream limits.\n");

    hr = IMFTransform_GetStreamCount(transform, &input_count, &output_count);
    ok(hr == S_OK, "Failed to get stream count, hr %#lx.\n", hr);
    ok(input_count == 1 && output_count == 1, "Unexpected stream count %lu, %lu.\n", input_count, output_count);

    hr = IMFTransform_GetStreamIDs(transform, 1, &input_id, 1, &output_id);
    ok(hr == E_NOTIMPL, "Unexpected hr %#lx.\n", hr);

    input_id = 100;
    hr = IMFTransform_AddInputStreams(transform, 1, &input_id);
    ok(hr == E_NOTIMPL, "Unexpected hr %#lx.\n", hr);

    hr = IMFTransform_DeleteInputStream(transform, 0);
    ok(hr == E_NOTIMPL, "Unexpected hr %#lx.\n", hr);

    hr = IMFTransform_GetInputStatus(transform, 0, &flags);
    ok(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "Unexpected hr %#lx.\n", hr);

    hr = IMFTransform_GetInputStreamAttributes(transform, 0, &attributes);
    ok(hr == E_NOTIMPL, "Unexpected hr %#lx.\n", hr);

    hr = IMFTransform_GetOutputStatus(transform, &flags);
    ok(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "Unexpected hr %#lx.\n", hr);

    hr = IMFTransform_GetOutputStreamAttributes(transform, 0, &attributes);
    ok(hr == S_OK, "Failed to get output attributes, hr %#lx.\n", hr);
    hr = IMFTransform_GetOutputStreamAttributes(transform, 0, &attributes2);
    ok(hr == S_OK, "Failed to get output attributes, hr %#lx.\n", hr);
    ok(attributes == attributes2, "Unexpected instance.\n");
    IMFAttributes_Release(attributes);
    IMFAttributes_Release(attributes2);

    hr = IMFTransform_GetOutputAvailableType(transform, 0, 0, &media_type);
    ok(hr == MF_E_NO_MORE_TYPES, "Unexpected hr %#lx.\n", hr);

    hr = IMFTransform_GetInputCurrentType(transform, 0, &media_type);
    ok(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "Unexpected hr %#lx.\n", hr);

    hr = IMFTransform_GetInputCurrentType(transform, 1, &media_type);
    ok(hr == MF_E_INVALIDSTREAMNUMBER, "Unexpected hr %#lx.\n", hr);

    hr = IMFTransform_GetOutputCurrentType(transform, 0, &media_type);
    ok(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "Unexpected hr %#lx.\n", hr);

    hr = IMFTransform_GetOutputCurrentType(transform, 1, &media_type);
    ok(hr == MF_E_INVALIDSTREAMNUMBER, "Unexpected hr %#lx.\n", hr);

    hr = IMFTransform_GetInputStreamInfo(transform, 1, &input_info);
    ok(hr == MF_E_INVALIDSTREAMNUMBER, "Unexpected hr %#lx.\n", hr);

    memset(&input_info, 0xcc, sizeof(input_info));
    hr = IMFTransform_GetInputStreamInfo(transform, 0, &input_info);
    ok(hr == S_OK, "Failed to get stream info, hr %#lx.\n", hr);
    ok(input_info.dwFlags == 0, "Unexpected flag %#lx.\n", input_info.dwFlags);
    ok(input_info.cbSize == 0, "Unexpected size %lu.\n", input_info.cbSize);
    ok(input_info.cbMaxLookahead == 0, "Unexpected lookahead length %lu.\n", input_info.cbMaxLookahead);
    ok(input_info.cbAlignment == 0, "Unexpected alignment %lu.\n", input_info.cbAlignment);
    hr = MFCreateMediaEvent(MEUnknown, &GUID_NULL, S_OK, NULL, &event);
    ok(hr == S_OK, "Failed to create event object, hr %#lx.\n", hr);
    hr = IMFTransform_ProcessEvent(transform, 0, event);
    ok(hr == E_NOTIMPL, "Unexpected hr %#lx.\n", hr);
    hr = IMFTransform_ProcessEvent(transform, 1, event);
    ok(hr == E_NOTIMPL, "Unexpected hr %#lx.\n", hr);
    ref = IMFMediaEvent_Release(event);
    ok(ref == 0, "Release returned %ld\n", ref);

    /* Configure stream types. */
    for (i = 0;;++i)
    {
        if (FAILED(hr = IMFTransform_GetInputAvailableType(transform, 0, i, &media_type)))
        {
            ok(hr == MF_E_NO_MORE_TYPES, "Unexpected hr %#lx.\n", hr);
            break;
        }

        hr = IMFTransform_GetInputAvailableType(transform, 0, i, &media_type2);
        ok(hr == S_OK, "Failed to get available type, hr %#lx.\n", hr);
        ok(media_type != media_type2, "Unexpected instance.\n");
        ref = IMFMediaType_Release(media_type2);
        ok(ref == 0, "Release returned %ld\n", ref);

        hr = IMFMediaType_GetMajorType(media_type, &guid);
        ok(hr == S_OK, "Failed to get major type, hr %#lx.\n", hr);
        ok(IsEqualGUID(&guid, &MFMediaType_Video), "Unexpected major type.\n");

        hr = IMFMediaType_GetCount(media_type, &count);
        ok(hr == S_OK, "Failed to get attributes count, hr %#lx.\n", hr);
        ok(count == 2, "Unexpected count %u.\n", count);

        hr = IMFMediaType_GetGUID(media_type, &MF_MT_SUBTYPE, &guid);
        ok(hr == S_OK, "Failed to get subtype, hr %#lx.\n", hr);
        ok(is_supported_video_type(&guid), "Unexpected media type %s.\n", wine_dbgstr_guid(&guid));

        hr = IMFTransform_SetInputType(transform, 0, media_type, MFT_SET_TYPE_TEST_ONLY);
        ok(FAILED(hr), "Unexpected hr %#lx.\n", hr);

        hr = IMFTransform_SetInputType(transform, 0, media_type, 0);
        ok(FAILED(hr), "Unexpected hr %#lx.\n", hr);

        hr = IMFTransform_GetOutputCurrentType(transform, 0, &media_type2);
        ok(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "Unexpected hr %#lx.\n", hr);

        /* FIXME: figure out if those require additional attributes or simply advertised but not supported */
        if (IsEqualGUID(&guid, &MFVideoFormat_L8) || IsEqualGUID(&guid, &MFVideoFormat_L16)
                || IsEqualGUID(&guid, &MFVideoFormat_D16) || IsEqualGUID(&guid, &MFVideoFormat_420O)
                || IsEqualGUID(&guid, &MFVideoFormat_A16B16G16R16F))
        {
            ref = IMFMediaType_Release(media_type);
            ok(ref == 0, "Release returned %ld\n", ref);
            continue;
        }

        hr = IMFMediaType_SetUINT64(media_type, &MF_MT_FRAME_SIZE, ((UINT64)16 << 32) | 16);
        ok(hr == S_OK, "Failed to set attribute, hr %#lx.\n", hr);

        hr = IMFTransform_SetInputType(transform, 0, media_type, MFT_SET_TYPE_TEST_ONLY);
        ok(hr == S_OK, "Failed to test input type %s, hr %#lx.\n", wine_dbgstr_guid(&guid), hr);

        hr = IMFTransform_SetInputType(transform, 0, media_type, 0);
        ok(hr == S_OK, "Failed to test input type, hr %#lx.\n", hr);

        hr = IMFTransform_GetInputCurrentType(transform, 0, &media_type2);
        ok(hr == S_OK, "Failed to get current type, hr %#lx.\n", hr);
        ok(media_type != media_type2, "Unexpected instance.\n");
        IMFMediaType_Release(media_type2);

        hr = IMFTransform_GetInputStatus(transform, 0, &flags);
        ok(hr == S_OK, "Failed to get input status, hr %#lx.\n", hr);
        ok(flags == MFT_INPUT_STATUS_ACCEPT_DATA, "Unexpected input status %#lx.\n", flags);

        hr = IMFTransform_GetInputStreamInfo(transform, 0, &input_info);
        ok(hr == S_OK, "Failed to get stream info, hr %#lx.\n", hr);
        ok(input_info.dwFlags == 0, "Unexpected flags %#lx.\n", input_info.dwFlags);
        ok(input_info.cbMaxLookahead == 0, "Unexpected lookahead length %lu.\n", input_info.cbMaxLookahead);
        ok(input_info.cbAlignment == 0, "Unexpected alignment %lu.\n", input_info.cbAlignment);

        IMFMediaType_Release(media_type);
    }

    /* IYUV -> RGB32 */
    hr = MFCreateMediaType(&media_type);
    ok(hr == S_OK, "Failed to create media type, hr %#lx.\n", hr);

    hr = IMFMediaType_SetGUID(media_type, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
    ok(hr == S_OK, "Failed to set attribute, hr %#lx.\n", hr);

    hr = IMFMediaType_SetGUID(media_type, &MF_MT_SUBTYPE, &MFVideoFormat_IYUV);
    ok(hr == S_OK, "Failed to set attribute, hr %#lx.\n", hr);

    hr = IMFMediaType_SetUINT64(media_type, &MF_MT_FRAME_SIZE, ((UINT64)16 << 32) | 16);
    ok(hr == S_OK, "Failed to set attribute, hr %#lx.\n", hr);

    hr = IMFTransform_SetInputType(transform, 0, media_type, 0);
    ok(hr == S_OK, "Failed to set input type, hr %#lx.\n", hr);

    hr = IMFMediaType_SetGUID(media_type, &MF_MT_SUBTYPE, &MFVideoFormat_RGB32);
    ok(hr == S_OK, "Failed to set attribute, hr %#lx.\n", hr);

    hr = IMFTransform_SetOutputType(transform, 0, media_type, 0);
    ok(hr == S_OK, "Failed to set output type, hr %#lx.\n", hr);

    memset(&output_info, 0, sizeof(output_info));
    hr = IMFTransform_GetOutputStreamInfo(transform, 0, &output_info);
    ok(hr == S_OK, "Failed to get stream info, hr %#lx.\n", hr);
    ok(output_info.dwFlags == 0, "Unexpected flags %#lx.\n", output_info.dwFlags);
    ok(output_info.cbSize > 0, "Unexpected size %lu.\n", output_info.cbSize);
    ok(output_info.cbAlignment == 0, "Unexpected alignment %lu.\n", output_info.cbAlignment);

    hr = MFCreateSample(&input_sample);
    ok(hr == S_OK, "Failed to create a sample, hr %#lx.\n", hr);

    hr = MFCreateSample(&output_sample);
    ok(hr == S_OK, "Failed to create a sample, hr %#lx.\n", hr);

    hr = check_mft_process_output(transform, output_sample, &output_status);
    todo_wine
    ok(hr == MF_E_TRANSFORM_NEED_MORE_INPUT, "Unexpected hr %#lx.\n", hr);

    hr = IMFTransform_ProcessInput(transform, 0, input_sample, 0);
    todo_wine
    ok(hr == S_OK, "Failed to push a sample, hr %#lx.\n", hr);

    hr = IMFTransform_ProcessInput(transform, 0, input_sample, 0);
    todo_wine
    ok(hr == MF_E_NOTACCEPTING, "Unexpected hr %#lx.\n", hr);

    hr = check_mft_process_output(transform, output_sample, &output_status);
    todo_wine
    ok(hr == MF_E_NO_SAMPLE_TIMESTAMP, "Unexpected hr %#lx.\n", hr);

    hr = IMFSample_SetSampleTime(input_sample, 0);
    ok(hr == S_OK, "Failed to set sample time, hr %#lx.\n", hr);
    hr = check_mft_process_output(transform, output_sample, &output_status);
    todo_wine
    ok(hr == E_INVALIDARG, "Unexpected hr %#lx.\n", hr);

    hr = MFCreateMemoryBuffer(1024 * 1024, &buffer);
    ok(hr == S_OK, "Failed to create a buffer, hr %#lx.\n", hr);

    hr = IMFSample_AddBuffer(input_sample, buffer);
    ok(hr == S_OK, "Failed to add a buffer, hr %#lx.\n", hr);

    hr = IMFSample_AddBuffer(output_sample, buffer);
    ok(hr == S_OK, "Failed to add a buffer, hr %#lx.\n", hr);

    hr = check_mft_process_output(transform, output_sample, &output_status);
    todo_wine
    ok(hr == S_OK || broken(FAILED(hr)) /* Win8 */, "Failed to get output buffer, hr %#lx.\n", hr);

    if (SUCCEEDED(hr))
    {
        hr = check_mft_process_output(transform, output_sample, &output_status);
        ok(hr == MF_E_TRANSFORM_NEED_MORE_INPUT, "Unexpected hr %#lx.\n", hr);
    }

    ref = IMFTransform_Release(transform);
    ok(ref == 0, "Release returned %ld\n", ref);

    ref = IMFMediaType_Release(media_type);
    ok(ref == 0, "Release returned %ld\n", ref);
    ref = IMFSample_Release(input_sample);
    ok(ref == 0, "Release returned %ld\n", ref);
    ref = IMFSample_Release(output_sample);
    ok(ref == 0, "Release returned %ld\n", ref);
    ref = IMFMediaBuffer_Release(buffer);
    ok(ref == 0, "Release returned %ld\n", ref);


    hr = CoCreateInstance(class_id, NULL, CLSCTX_INPROC_SERVER, &IID_IMFTransform, (void **)&transform);
    ok(hr == S_OK, "Unexpected hr %#lx.\n", hr);

    /* check default media types */

    memset(&input_info, 0xcd, sizeof(input_info));
    hr = IMFTransform_GetInputStreamInfo(transform, 0, &input_info);
    ok(hr == S_OK, "GetInputStreamInfo returned %#lx\n", hr);
    ok(input_info.hnsMaxLatency == 0, "got hnsMaxLatency %s\n", wine_dbgstr_longlong(input_info.hnsMaxLatency));
    ok(input_info.dwFlags == 0, "got dwFlags %#lx\n", input_info.dwFlags);
    ok(input_info.cbSize == 0, "got cbSize %#lx\n", input_info.cbSize);
    ok(input_info.cbMaxLookahead == 0, "got cbMaxLookahead %#lx\n", input_info.cbMaxLookahead);
    ok(input_info.cbAlignment == 0, "got cbAlignment %#lx\n", input_info.cbAlignment);

    memset(&output_info, 0xcd, sizeof(output_info));
    hr = IMFTransform_GetOutputStreamInfo(transform, 0, &output_info);
    ok(hr == S_OK, "GetOutputStreamInfo returned %#lx\n", hr);
    ok(output_info.dwFlags == 0, "got dwFlags %#lx\n", output_info.dwFlags);
    ok(output_info.cbSize == 0, "got cbSize %#lx\n", output_info.cbSize);
    ok(output_info.cbAlignment == 0, "got cbAlignment %#lx\n", output_info.cbAlignment);

    hr = IMFTransform_GetOutputAvailableType(transform, 0, 0, &media_type);
    ok(hr == MF_E_NO_MORE_TYPES, "GetOutputAvailableType returned %#lx\n", hr);

    hr = IMFTransform_GetInputAvailableType(transform, 0, 23, &media_type);
    ok(hr == S_OK || hr == MF_E_NO_MORE_TYPES /* w8 */, "GetOutputAvailableType returned %#lx\n", hr);
    if (hr == MF_E_NO_MORE_TYPES)
        expect_available_inputs = expect_available_inputs_w8;
    else
    {
        hr = IMFTransform_GetInputAvailableType(transform, 0, 27, &media_type);
        ok(hr == S_OK || broken(hr == MF_E_NO_MORE_TYPES) /* w1064v1507 */, "GetOutputAvailableType returned %#lx\n", hr);
        if (hr == MF_E_NO_MORE_TYPES)
            expect_available_inputs = expect_available_inputs_w10 + 3;
        else
            expect_available_inputs = expect_available_inputs_w10;
    }

    i = -1;
    while (SUCCEEDED(hr = IMFTransform_GetInputAvailableType(transform, 0, ++i, &media_type)))
    {
        /* FIXME: Skip exotic input types which aren't directly accepted */
        if (IsEqualGUID(&expect_available_inputs[i], &MFVideoFormat_L8)
                || IsEqualGUID(&expect_available_inputs[i], &MFVideoFormat_L16)
                || IsEqualGUID(&expect_available_inputs[i], &MFAudioFormat_MPEG)
                || IsEqualGUID(&expect_available_inputs[i], &MFVideoFormat_420O)
                || IsEqualGUID(&expect_available_inputs[i], &MFVideoFormat_A16B16G16R16F) /* w1064v1507 */)
            continue;

        winetest_push_context("in %lu", i);
        ok(hr == S_OK, "GetInputAvailableType returned %#lx\n", hr);
        check_media_type(media_type, expect_available_common, -1);

        hr = IMFMediaType_GetGUID(media_type, &MF_MT_SUBTYPE, &guid);
        ok(hr == S_OK, "GetGUID returned %#lx\n", hr);

        /* w1064v1507 doesn't expose MFVideoFormat_ABGR32 input */
        if (broken(IsEqualGUID(&expect_available_inputs[i], &MFVideoFormat_ABGR32)
                && IsEqualGUID(&guid, &MFVideoFormat_RGB32)))
            expect_available_inputs++;

        ok(IsEqualGUID(&expect_available_inputs[i], &guid), "got subtype %s\n", debugstr_guid(&guid));

        hr = IMFTransform_SetInputType(transform, 0, media_type, 0);
        ok(hr == MF_E_ATTRIBUTENOTFOUND, "SetInputType returned %#lx.\n", hr);

        hr = IMFMediaType_SetUINT64(media_type, &MF_MT_FRAME_SIZE, (UINT64)actual_width << 32 | actual_height);
        ok(hr == S_OK, "SetUINT64 returned %#lx.\n", hr);
        hr = IMFTransform_SetInputType(transform, 0, media_type, 0);
        ok(hr == S_OK, "SetInputType returned %#lx.\n", hr);

        hr = IMFTransform_GetOutputAvailableType(transform, 0, 0, &media_type2);
        ok(hr == S_OK, "GetOutputAvailableType returned %#lx.\n", hr);
        hr = IMFMediaType_IsEqual(media_type, media_type2, &flags);
        ok(hr == S_OK, "IsEqual returned %#lx.\n", hr);
        IMFMediaType_Release(media_type2);

        ret = IMFMediaType_Release(media_type);
        ok(ret == 1, "Release returned %lu\n", ret);

        j = k = 0;
        while (SUCCEEDED(hr = IMFTransform_GetOutputAvailableType(transform, 0, ++j, &media_type)))
        {
            winetest_push_context("out %lu", j);
            ok(hr == S_OK, "GetOutputAvailableType returned %#lx\n", hr);
            check_media_type(media_type, expect_available_common, -1);

            hr = IMFMediaType_GetGUID(media_type, &MF_MT_SUBTYPE, &guid);
            ok(hr == S_OK, "GetGUID returned %#lx\n", hr);

            for (; k < ARRAY_SIZE(expect_available_outputs); k++)
                if (IsEqualGUID(&expect_available_outputs[k], &guid))
                    break;
            ok(k < ARRAY_SIZE(expect_available_outputs), "got subtype %s\n", debugstr_guid(&guid));

            ret = IMFMediaType_Release(media_type);
            ok(ret == 0, "Release returned %lu\n", ret);
            winetest_pop_context();
        }
        ok(hr == MF_E_NO_MORE_TYPES, "GetOutputAvailableType returned %#lx\n", hr);

        winetest_pop_context();
    }
    ok(hr == MF_E_NO_MORE_TYPES, "GetInputAvailableType returned %#lx\n", hr);
    ok(i == 22 || i == 30 || broken(i == 26) /* w1064v1507 */, "%lu input media types\n", i);

    check_mft_set_input_type_required(transform, input_type_desc);

    hr = MFCreateMediaType(&media_type);
    ok(hr == S_OK, "MFCreateMediaType returned %#lx\n", hr);
    init_media_type(media_type, input_type_desc, -1);
    hr = IMFTransform_SetInputType(transform, 0, media_type, 0);
    ok(hr == S_OK, "SetInputType returned %#lx.\n", hr);
    ret = IMFMediaType_Release(media_type);
    ok(ret == 1, "Release returned %lu\n", ret);

    check_mft_set_output_type_required(transform, output_type_desc);

    hr = MFCreateMediaType(&media_type);
    ok(hr == S_OK, "MFCreateMediaType returned %#lx\n", hr);
    init_media_type(media_type, output_type_desc, -1);
    hr = IMFTransform_SetOutputType(transform, 0, media_type, 0);
    ok(hr == S_OK, "SetOutputType returned %#lx.\n", hr);
    ret = IMFMediaType_Release(media_type);
    ok(ret == 1, "Release returned %lu\n", ret);

    memset(&input_info, 0xcd, sizeof(input_info));
    hr = IMFTransform_GetInputStreamInfo(transform, 0, &input_info);
    ok(hr == S_OK, "GetInputStreamInfo returned %#lx\n", hr);
    ok(input_info.hnsMaxLatency == 0, "got hnsMaxLatency %s\n", wine_dbgstr_longlong(input_info.hnsMaxLatency));
    ok(input_info.dwFlags == 0, "got dwFlags %#lx\n", input_info.dwFlags);
    ok(input_info.cbSize == actual_width * actual_height * 3 / 2, "got cbSize %#lx\n", input_info.cbSize);
    ok(input_info.cbMaxLookahead == 0, "got cbMaxLookahead %#lx\n", input_info.cbMaxLookahead);
    ok(input_info.cbAlignment == 0, "got cbAlignment %#lx\n", input_info.cbAlignment);

    memset(&output_info, 0xcd, sizeof(output_info));
    hr = IMFTransform_GetOutputStreamInfo(transform, 0, &output_info);
    ok(hr == S_OK, "GetOutputStreamInfo returned %#lx\n", hr);
    ok(output_info.dwFlags == 0, "got dwFlags %#lx\n", output_info.dwFlags);
    ok(output_info.cbSize == actual_width * actual_height * 4, "got cbSize %#lx\n", output_info.cbSize);
    ok(output_info.cbAlignment == 0, "got cbAlignment %#lx\n", output_info.cbAlignment);

    load_resource(L"nv12frame.bmp", &nv12frame_data, &nv12frame_data_len);
    /* skip BMP header and RGB data from the dump */
    length = *(DWORD *)(nv12frame_data + 2);
    nv12frame_data_len = nv12frame_data_len - length;
    nv12frame_data = nv12frame_data + length;
    ok(nv12frame_data_len == 13824, "got length %lu\n", nv12frame_data_len);

    input_sample = create_sample(nv12frame_data, nv12frame_data_len);
    hr = IMFSample_SetSampleTime(input_sample, 0);
    ok(hr == S_OK, "SetSampleTime returned %#lx\n", hr);
    hr = IMFSample_SetSampleDuration(input_sample, 10000000);
    ok(hr == S_OK, "SetSampleDuration returned %#lx\n", hr);
    hr = IMFTransform_ProcessInput(transform, 0, input_sample, 0);
    ok(hr == S_OK, "ProcessInput returned %#lx\n", hr);
    hr = IMFTransform_ProcessInput(transform, 0, input_sample, 0);
    ok(hr == MF_E_NOTACCEPTING, "ProcessInput returned %#lx\n", hr);
    hr = IMFTransform_ProcessMessage(transform, MFT_MESSAGE_COMMAND_DRAIN, 0);
    ok(hr == S_OK, "ProcessMessage returned %#lx\n", hr);
    ret = IMFSample_Release(input_sample);
    ok(ret <= 1, "Release returned %ld\n", ret);

    hr = MFCreateCollection(&output_samples);
    ok(hr == S_OK, "MFCreateCollection returned %#lx\n", hr);

    output_sample = create_sample(NULL, output_info.cbSize);
    hr = check_mft_process_output(transform, output_sample, &output_status);
    ok(hr == S_OK || broken(hr == MF_E_SHUTDOWN) /* w8 */, "ProcessOutput returned %#lx\n", hr);
    if (hr != S_OK)
    {
        win_skip("ProcessOutput returned MF_E_SHUTDOWN, skipping tests.\n");
        goto skip_output;
    }
    ok(output_status == 0, "got output[0].dwStatus %#lx\n", output_status);

    hr = IMFCollection_AddElement(output_samples, (IUnknown *)output_sample);
    ok(hr == S_OK, "AddElement returned %#lx\n", hr);
    ref = IMFSample_Release(output_sample);
    ok(ref == 1, "Release returned %ld\n", ref);

    ret = check_mf_sample_collection(output_samples, &output_sample_desc, L"rgb32frame-vp.bmp");
    todo_wine
    ok(ret == 0 || broken(ret == 25) /* w1064v1507 / w1064v1809 incorrectly rescale */, "got %lu%% diff\n", ret);
    IMFCollection_Release(output_samples);

    output_sample = create_sample(NULL, output_info.cbSize);
    hr = check_mft_process_output(transform, output_sample, &output_status);
    ok(hr == MF_E_TRANSFORM_NEED_MORE_INPUT, "ProcessOutput returned %#lx\n", hr);
    ok(output_status == 0, "got output[0].dwStatus %#lx\n", output_status);
    hr = IMFSample_GetTotalLength(output_sample, &length);
    ok(hr == S_OK, "GetTotalLength returned %#lx\n", hr);
    ok(length == 0, "got length %lu\n", length);

skip_output:
    ret = IMFSample_Release(output_sample);
    ok(ret == 0, "Release returned %lu\n", ret);

    ret = IMFTransform_Release(transform);
    ok(ret == 0, "Release returned %ld\n", ret);

failed:
    winetest_pop_context();
    CoUninitialize();
}

static void test_mp3_decoder(void)
{
    const GUID *const class_id = &CLSID_CMP3DecMediaObject;
    const struct transform_info expect_mft_info =
    {
        .name = L"MP3 Decoder MFT",
        .major_type = &MFMediaType_Audio,
        .inputs =
        {
            {.subtype = &MFAudioFormat_MP3},
        },
        .outputs =
        {
            {.subtype = &MFAudioFormat_PCM},
        },
    };
    const struct transform_info expect_dmo_info =
    {
        .name = L"MP3 Decoder DMO",
        .major_type = &MEDIATYPE_Audio,
        .inputs =
        {
            {.subtype = &MFAudioFormat_MP3},
        },
        .outputs =
        {
            {.subtype = &MEDIASUBTYPE_PCM},
        },
    };

    static const media_type_desc expect_available_inputs[] =
    {
        {
            ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio),
            ATTR_GUID(MF_MT_SUBTYPE, MFAudioFormat_MP3),
            ATTR_UINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, 1),
        },
    };
    static const media_type_desc expect_available_outputs[] =
    {
        {
            ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio),
            ATTR_GUID(MF_MT_SUBTYPE, MFAudioFormat_Float),
            ATTR_UINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 32),
            ATTR_UINT32(MF_MT_AUDIO_NUM_CHANNELS, 2),
            ATTR_UINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 22050),
            ATTR_UINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 176400),
            ATTR_UINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 8),
            ATTR_UINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, 1),
        },
        {
            ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio),
            ATTR_GUID(MF_MT_SUBTYPE, MFAudioFormat_PCM),
            ATTR_UINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16),
            ATTR_UINT32(MF_MT_AUDIO_NUM_CHANNELS, 2),
            ATTR_UINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 22050),
            ATTR_UINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 88200),
            ATTR_UINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 4),
            ATTR_UINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, 1),
        },
        {
            ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio),
            ATTR_GUID(MF_MT_SUBTYPE, MFAudioFormat_PCM),
            ATTR_UINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 8),
            ATTR_UINT32(MF_MT_AUDIO_NUM_CHANNELS, 2),
            ATTR_UINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 22050),
            ATTR_UINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 44100),
            ATTR_UINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 2),
            ATTR_UINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, 1),
        },
        {
            ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio),
            ATTR_GUID(MF_MT_SUBTYPE, MFAudioFormat_Float),
            ATTR_UINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 32),
            ATTR_UINT32(MF_MT_AUDIO_NUM_CHANNELS, 1),
            ATTR_UINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 22050),
            ATTR_UINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 88200),
            ATTR_UINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 4),
            ATTR_UINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, 1),
        },
        {
            ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio),
            ATTR_GUID(MF_MT_SUBTYPE, MFAudioFormat_PCM),
            ATTR_UINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16),
            ATTR_UINT32(MF_MT_AUDIO_NUM_CHANNELS, 1),
            ATTR_UINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 22050),
            ATTR_UINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 44100),
            ATTR_UINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 2),
            ATTR_UINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, 1),
        },
        {
            ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio),
            ATTR_GUID(MF_MT_SUBTYPE, MFAudioFormat_PCM),
            ATTR_UINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 8),
            ATTR_UINT32(MF_MT_AUDIO_NUM_CHANNELS, 1),
            ATTR_UINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 22050),
            ATTR_UINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 22050),
            ATTR_UINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 1),
            ATTR_UINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, 1),
        },
    };

    const struct attribute_desc input_type_desc[] =
    {
        ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio, .required = TRUE),
        ATTR_GUID(MF_MT_SUBTYPE, MFAudioFormat_MP3, .required = TRUE),
        ATTR_UINT32(MF_MT_AUDIO_NUM_CHANNELS, 2, .required = TRUE),
        ATTR_UINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 22050),
        {0},
    };
    static const struct attribute_desc output_type_desc[] =
    {
        ATTR_GUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio, .required = TRUE),
        ATTR_GUID(MF_MT_SUBTYPE, MFAudioFormat_PCM, .required = TRUE),
        ATTR_UINT32(MF_MT_AUDIO_NUM_CHANNELS, 2, .required = TRUE),
        ATTR_UINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16, .required = TRUE),
        ATTR_UINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 22050, .required = TRUE),
        ATTR_UINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 2 * (16 / 8), .required = TRUE),
        ATTR_UINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 2 * (16 / 8) * 22050, .required = TRUE),
        {0},
    };

    static const ULONG mp3dec_block_size = 0x1200;
    const struct buffer_desc output_buffer_desc[] =
    {
        {.length = 0x9c0, .compare = compare_pcm16},
        {.length = mp3dec_block_size, .compare = compare_pcm16},
    };
    const struct attribute_desc output_sample_attributes[] =
    {
        ATTR_UINT32(mft_output_sample_incomplete, 1),
        ATTR_UINT32(MFSampleExtension_CleanPoint, 1),
        {0},
    };
    const struct sample_desc output_sample_desc[] =
    {
        {
            .attributes = output_sample_attributes + 0,
            .sample_time = 0, .sample_duration = 282993,
            .buffer_count = 1, .buffers = output_buffer_desc + 0,
        },
        {
            .attributes = output_sample_attributes + 0,
            .sample_time = 282993, .sample_duration = 522449,
            .buffer_count = 1, .buffers = output_buffer_desc + 1, .repeat_count = 18,
        },
        {
            .attributes = output_sample_attributes + 1, /* not MFT_OUTPUT_DATA_BUFFER_INCOMPLETE */
            .sample_time = 10209524, .sample_duration = 522449,
            .buffer_count = 1, .buffers = output_buffer_desc + 1,
        },
    };

    MFT_REGISTER_TYPE_INFO output_type = {MFMediaType_Audio, MFAudioFormat_PCM};
    MFT_REGISTER_TYPE_INFO input_type = {MFMediaType_Audio, MFAudioFormat_MP3};
    IMFSample *input_sample, *output_sample;
    MFT_OUTPUT_STREAM_INFO output_info;
    MFT_INPUT_STREAM_INFO input_info;
    IMFCollection *output_samples;
    DWORD length, output_status;
    IMFMediaType *media_type;
    IMFTransform *transform;
    const BYTE *mp3enc_data;
    ULONG mp3enc_data_len;
    ULONG i, ret, ref;
    HRESULT hr;

    hr = CoInitialize(NULL);
    ok(hr == S_OK, "Failed to initialize, hr %#lx.\n", hr);

    winetest_push_context("mp3dec");

    if (!check_mft_enum(MFT_CATEGORY_AUDIO_DECODER, &input_type, &output_type, class_id))
        goto failed;
    check_mft_get_info(class_id, &expect_mft_info);
    check_dmo_get_info(class_id, &expect_dmo_info);

    if (FAILED(hr = CoCreateInstance(class_id, NULL, CLSCTX_INPROC_SERVER,
            &IID_IMFTransform, (void **)&transform)))
        goto failed;

    check_interface(transform, &IID_IMFTransform, TRUE);
    check_interface(transform, &IID_IMediaObject, TRUE);
    todo_wine
    check_interface(transform, &IID_IPropertyStore, TRUE);
    check_interface(transform, &IID_IPropertyBag, FALSE);

    /* check default media types */

    hr = IMFTransform_GetInputStreamInfo(transform, 0, &input_info);
    ok(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "GetInputStreamInfo returned %#lx\n", hr);
    hr = IMFTransform_GetOutputStreamInfo(transform, 0, &output_info);
    ok(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "GetOutputStreamInfo returned %#lx\n", hr);
    hr = IMFTransform_GetOutputAvailableType(transform, 0, 0, &media_type);
    ok(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "GetOutputAvailableType returned %#lx\n", hr);

    i = -1;
    while (SUCCEEDED(hr = IMFTransform_GetInputAvailableType(transform, 0, ++i, &media_type)))
    {
        winetest_push_context("in %lu", i);
        ok(hr == S_OK, "GetInputAvailableType returned %#lx\n", hr);
        check_media_type(media_type, expect_available_inputs[i], -1);
        hr = IMFTransform_SetInputType(transform, 0, media_type, 0);
        ok(hr == MF_E_INVALIDMEDIATYPE, "SetInputType returned %#lx.\n", hr);
        ret = IMFMediaType_Release(media_type);
        ok(ret == 0, "Release returned %lu\n", ret);
        winetest_pop_context();
    }
    todo_wine
    ok(hr == MF_E_NO_MORE_TYPES, "GetInputAvailableType returned %#lx\n", hr);
    todo_wine
    ok(i == ARRAY_SIZE(expect_available_inputs), "%lu input media types\n", i);

    /* setting output media type first doesn't work */

    hr = MFCreateMediaType(&media_type);
    ok(hr == S_OK, "MFCreateMediaType returned %#lx\n", hr);
    init_media_type(media_type, output_type_desc, -1);
    hr = IMFTransform_SetOutputType(transform, 0, media_type, 0);
    ok(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "SetOutputType returned %#lx.\n", hr);
    ret = IMFMediaType_Release(media_type);
    ok(ret == 0, "Release returned %lu\n", ret);

    check_mft_set_input_type_required(transform, input_type_desc);

    hr = MFCreateMediaType(&media_type);
    ok(hr == S_OK, "MFCreateMediaType returned %#lx\n", hr);
    init_media_type(media_type, input_type_desc, -1);
    hr = IMFTransform_SetInputType(transform, 0, media_type, 0);
    ok(hr == S_OK, "SetInputType returned %#lx.\n", hr);
    ret = IMFMediaType_Release(media_type);
    ok(ret == 0, "Release returned %lu\n", ret);

    hr = IMFTransform_GetInputStreamInfo(transform, 0, &input_info);
    ok(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "GetInputStreamInfo returned %#lx\n", hr);
    hr = IMFTransform_GetOutputStreamInfo(transform, 0, &output_info);
    ok(hr == MF_E_TRANSFORM_TYPE_NOT_SET, "GetOutputStreamInfo returned %#lx\n", hr);

    /* check new output media types */

    i = -1;
    while (SUCCEEDED(hr = IMFTransform_GetOutputAvailableType(transform, 0, ++i, &media_type)))
    {
        winetest_push_context("out %lu", i);
        ok(hr == S_OK, "GetOutputAvailableType returned %#lx\n", hr);
        check_media_type(media_type, expect_available_outputs[i], -1);
        ret = IMFMediaType_Release(media_type);
        ok(ret == 0, "Release returned %lu\n", ret);
        winetest_pop_context();
    }
    ok(hr == MF_E_NO_MORE_TYPES, "GetOutputAvailableType returned %#lx\n", hr);
    ok(i == ARRAY_SIZE(expect_available_outputs), "%lu output media types\n", i);

    check_mft_set_output_type_required(transform, output_type_desc);

    hr = MFCreateMediaType(&media_type);
    ok(hr == S_OK, "MFCreateMediaType returned %#lx\n", hr);
    init_media_type(media_type, output_type_desc, -1);
    hr = IMFTransform_SetOutputType(transform, 0, media_type, 0);
    ok(hr == S_OK, "SetOutputType returned %#lx.\n", hr);
    ret = IMFMediaType_Release(media_type);
    ok(ret == 0, "Release returned %lu\n", ret);

    memset(&input_info, 0xcd, sizeof(input_info));
    hr = IMFTransform_GetInputStreamInfo(transform, 0, &input_info);
    ok(hr == S_OK, "GetInputStreamInfo returned %#lx\n", hr);
    ok(input_info.hnsMaxLatency == 0, "got hnsMaxLatency %s\n", wine_dbgstr_longlong(input_info.hnsMaxLatency));
    ok(input_info.dwFlags == 0, "got dwFlags %#lx\n", input_info.dwFlags);
    ok(input_info.cbSize == 0, "got cbSize %lu\n", input_info.cbSize);
    ok(input_info.cbMaxLookahead == 0, "got cbMaxLookahead %#lx\n", input_info.cbMaxLookahead);
    ok(input_info.cbAlignment == 1, "got cbAlignment %#lx\n", input_info.cbAlignment);

    memset(&output_info, 0xcd, sizeof(output_info));
    hr = IMFTransform_GetOutputStreamInfo(transform, 0, &output_info);
    ok(hr == S_OK, "GetOutputStreamInfo returned %#lx\n", hr);
    ok(output_info.dwFlags == 0, "got dwFlags %#lx\n", output_info.dwFlags);
    todo_wine
    ok(output_info.cbSize == mp3dec_block_size, "got cbSize %#lx\n", output_info.cbSize);
    ok(output_info.cbAlignment == 1, "got cbAlignment %#lx\n", output_info.cbAlignment);

    load_resource(L"mp3encdata.bin", &mp3enc_data, &mp3enc_data_len);
    ok(mp3enc_data_len == 6295, "got length %lu\n", mp3enc_data_len);

    input_sample = create_sample(mp3enc_data, mp3enc_data_len);
    hr = IMFTransform_ProcessInput(transform, 0, input_sample, 0);
    ok(hr == S_OK, "ProcessInput returned %#lx\n", hr);
    ret = IMFSample_Release(input_sample);
    ok(ret == 1, "Release returned %lu\n", ret);

    input_sample = create_sample(mp3enc_data, mp3enc_data_len);
    hr = IMFTransform_ProcessInput(transform, 0, input_sample, 0);
    ok(hr == MF_E_NOTACCEPTING, "ProcessInput returned %#lx\n", hr);
    ret = IMFSample_Release(input_sample);
    ok(ret == 0, "Release returned %lu\n", ret);

    hr = IMFTransform_ProcessMessage(transform, MFT_MESSAGE_COMMAND_DRAIN, 0);
    ok(hr == S_OK, "ProcessMessage returned %#lx\n", hr);

    hr = MFCreateCollection(&output_samples);
    ok(hr == S_OK, "MFCreateCollection returned %#lx\n", hr);

    /* first sample is broken */
    output_sample = create_sample(NULL, output_info.cbSize);
    hr = check_mft_process_output(transform, output_sample, &output_status);
    ok(hr == S_OK, "ProcessOutput returned %#lx\n", hr);
    ok(output_status == MFT_OUTPUT_DATA_BUFFER_INCOMPLETE, "got output[0].dwStatus %#lx\n", output_status);
    hr = IMFSample_GetTotalLength(output_sample, &length);
    ok(hr == S_OK, "GetTotalLength returned %#lx\n", hr);
    ok(length == mp3dec_block_size /* Win8 */ || length == 0x9c0 /* Win10 */ || length == 0x900 /* Win7 */,
            "got length %lu\n", length);
    hr = IMFCollection_AddElement(output_samples, (IUnknown *)output_sample);
    ok(hr == S_OK, "AddElement returned %#lx\n", hr);
    ref = IMFSample_Release(output_sample);
    ok(ref == 1, "Release returned %ld\n", ref);

    output_sample = create_sample(NULL, output_info.cbSize);
    for (i = 0; SUCCEEDED(hr = check_mft_process_output(transform, output_sample, &output_status)); i++)
    {
        winetest_push_context("%lu", i);
        ok(hr == S_OK, "ProcessOutput returned %#lx\n", hr);
        ok(!(output_status & ~MFT_OUTPUT_DATA_BUFFER_INCOMPLETE), "got output[0].dwStatus %#lx\n", output_status);
        hr = IMFCollection_AddElement(output_samples, (IUnknown *)output_sample);
        ok(hr == S_OK, "AddElement returned %#lx\n", hr);
        ref = IMFSample_Release(output_sample);
        ok(ref == 1, "Release returned %ld\n", ref);
        output_sample = create_sample(NULL, output_info.cbSize);
        winetest_pop_context();
    }
    ok(hr == MF_E_TRANSFORM_NEED_MORE_INPUT, "ProcessOutput returned %#lx\n", hr);
    ok(output_status == 0, "got output[0].dwStatus %#lx\n", output_status);
    ret = IMFSample_Release(output_sample);
    ok(ret == 0, "Release returned %lu\n", ret);
    ok(i == 20 || broken(i == 41) /* Win7 */, "got %lu output samples\n", i);

    if (broken(length != 0x9c0))
        win_skip("Skipping MP3 decoder output sample checks on Win7 / Win8\n");
    else
    {
        ret = check_mf_sample_collection(output_samples, output_sample_desc, L"mp3decdata.bin");
        ok(ret == 0, "got %lu%% diff\n", ret);
    }
    IMFCollection_Release(output_samples);

    output_sample = create_sample(NULL, mp3dec_block_size);
    hr = check_mft_process_output(transform, output_sample, &output_status);
    ok(hr == MF_E_TRANSFORM_NEED_MORE_INPUT, "ProcessOutput returned %#lx\n", hr);
    ok(output_status == 0, "got output[0].dwStatus %#lx\n", output_status);
    hr = IMFSample_GetTotalLength(output_sample, &length);
    ok(hr == S_OK, "GetTotalLength returned %#lx\n", hr);
    ok(length == 0, "got length %lu\n", length);
    ret = IMFSample_Release(output_sample);
    ok(ret == 0, "Release returned %lu\n", ret);

    ret = IMFTransform_Release(transform);
    ok(ret == 0, "Release returned %lu\n", ret);

failed:
    winetest_pop_context();
    CoUninitialize();
}

START_TEST(transform)
{
    init_functions();

    test_sample_copier();
    test_sample_copier_output_processing();
    test_aac_encoder();
    test_aac_decoder();
    test_wma_encoder();
    test_wma_decoder();
    test_h264_decoder();
    test_audio_convert();
    test_color_convert();
    test_video_processor();
    test_mp3_decoder();
}
