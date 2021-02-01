/*
 * Unit test suite for kernel mode graphics driver
 *
 * Copyright 2019 Zhiyi Zhang
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

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "winternl.h"
#include "dwmapi.h"
#include "ddk/d3dkmthk.h"

#include "wine/test.h"

static const WCHAR display1W[] = L"\\\\.\\DISPLAY1";

static NTSTATUS (WINAPI *pD3DKMTCheckOcclusion)(const D3DKMT_CHECKOCCLUSION *);
static NTSTATUS (WINAPI *pD3DKMTCheckVidPnExclusiveOwnership)(const D3DKMT_CHECKVIDPNEXCLUSIVEOWNERSHIP *);
static NTSTATUS (WINAPI *pD3DKMTCloseAdapter)(const D3DKMT_CLOSEADAPTER *);
static NTSTATUS (WINAPI *pD3DKMTCreateDevice)(D3DKMT_CREATEDEVICE *);
static NTSTATUS (WINAPI *pD3DKMTDestroyDevice)(const D3DKMT_DESTROYDEVICE *);
static NTSTATUS (WINAPI *pD3DKMTOpenAdapterFromGdiDisplayName)(D3DKMT_OPENADAPTERFROMGDIDISPLAYNAME *);
static NTSTATUS (WINAPI *pD3DKMTOpenAdapterFromHdc)(D3DKMT_OPENADAPTERFROMHDC *);
static NTSTATUS (WINAPI *pD3DKMTSetVidPnSourceOwner)(const D3DKMT_SETVIDPNSOURCEOWNER *);
static HRESULT  (WINAPI *pDwmEnableComposition)(UINT);

static void test_D3DKMTOpenAdapterFromGdiDisplayName(void)
{
    D3DKMT_OPENADAPTERFROMGDIDISPLAYNAME open_adapter_gdi_desc;
    D3DKMT_CLOSEADAPTER close_adapter_desc;
    DISPLAY_DEVICEW display_device = {sizeof(display_device)};
    NTSTATUS status;
    DWORD i;

    lstrcpyW(open_adapter_gdi_desc.DeviceName, display1W);
    if (!pD3DKMTOpenAdapterFromGdiDisplayName
        || pD3DKMTOpenAdapterFromGdiDisplayName(&open_adapter_gdi_desc) == STATUS_PROCEDURE_NOT_FOUND)
    {
        win_skip("D3DKMTOpenAdapterFromGdiDisplayName() is unavailable.\n");
        return;
    }

    close_adapter_desc.hAdapter = open_adapter_gdi_desc.hAdapter;
    status = pD3DKMTCloseAdapter(&close_adapter_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#x.\n", status);

    /* Invalid parameters */
    status = pD3DKMTOpenAdapterFromGdiDisplayName(NULL);
    ok(status == STATUS_UNSUCCESSFUL, "Got unexpected return code %#x.\n", status);

    memset(&open_adapter_gdi_desc, 0, sizeof(open_adapter_gdi_desc));
    status = pD3DKMTOpenAdapterFromGdiDisplayName(&open_adapter_gdi_desc);
    ok(status == STATUS_UNSUCCESSFUL, "Got unexpected return code %#x.\n", status);

    /* Open adapter */
    for (i = 0; EnumDisplayDevicesW(NULL, i, &display_device, 0); ++i)
    {
        lstrcpyW(open_adapter_gdi_desc.DeviceName, display_device.DeviceName);
        status = pD3DKMTOpenAdapterFromGdiDisplayName(&open_adapter_gdi_desc);
        if (display_device.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)
            ok(status == STATUS_SUCCESS, "Got unexpected return code %#x.\n", status);
        else
        {
            ok(status == STATUS_UNSUCCESSFUL, "Got unexpected return code %#x.\n", status);
            continue;
        }

        ok(open_adapter_gdi_desc.hAdapter, "Expect not null.\n");
        ok(open_adapter_gdi_desc.AdapterLuid.LowPart || open_adapter_gdi_desc.AdapterLuid.HighPart,
           "Expect LUID not zero.\n");

        close_adapter_desc.hAdapter = open_adapter_gdi_desc.hAdapter;
        status = pD3DKMTCloseAdapter(&close_adapter_desc);
        ok(status == STATUS_SUCCESS, "Got unexpected return code %#x.\n", status);
    }
}

static void test_D3DKMTOpenAdapterFromHdc(void)
{
    DISPLAY_DEVICEW display_device = {sizeof(display_device)};
    D3DKMT_OPENADAPTERFROMHDC open_adapter_hdc_desc;
    D3DKMT_CLOSEADAPTER close_adapter_desc;
    INT adapter_count = 0;
    NTSTATUS status;
    HDC hdc;
    DWORD i;

    if (!pD3DKMTOpenAdapterFromHdc)
    {
        win_skip("D3DKMTOpenAdapterFromHdc() is missing.\n");
        return;
    }

    /* Invalid parameters */
    /* Passing a NULL pointer crashes on Windows 10 >= 2004 */
    if (0) status = pD3DKMTOpenAdapterFromHdc(NULL);

    memset(&open_adapter_hdc_desc, 0, sizeof(open_adapter_hdc_desc));
    status = pD3DKMTOpenAdapterFromHdc(&open_adapter_hdc_desc);
    if (status == STATUS_PROCEDURE_NOT_FOUND)
    {
        win_skip("D3DKMTOpenAdapterFromHdc() is not supported.\n");
        return;
    }
    todo_wine ok(status == STATUS_INVALID_PARAMETER, "Got unexpected return code %#x.\n", status);

    /* Open adapter */
    for (i = 0; EnumDisplayDevicesW(NULL, i, &display_device, 0); ++i)
    {
        if (!(display_device.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP))
            continue;

        adapter_count++;

        hdc = CreateDCW(0, display_device.DeviceName, 0, NULL);
        open_adapter_hdc_desc.hDc = hdc;
        status = pD3DKMTOpenAdapterFromHdc(&open_adapter_hdc_desc);
        todo_wine ok(status == STATUS_SUCCESS, "Got unexpected return code %#x.\n", status);
        todo_wine ok(open_adapter_hdc_desc.hAdapter, "Expect not null.\n");
        DeleteDC(hdc);

        if (status == STATUS_SUCCESS)
        {
            close_adapter_desc.hAdapter = open_adapter_hdc_desc.hAdapter;
            status = pD3DKMTCloseAdapter(&close_adapter_desc);
            ok(status == STATUS_SUCCESS, "Got unexpected return code %#x.\n", status);
        }
    }

    /* HDC covering more than two adapters is invalid for D3DKMTOpenAdapterFromHdc */
    hdc = GetDC(0);
    open_adapter_hdc_desc.hDc = hdc;
    status = pD3DKMTOpenAdapterFromHdc(&open_adapter_hdc_desc);
    ReleaseDC(0, hdc);
    todo_wine ok(status == (adapter_count > 1 ? STATUS_INVALID_PARAMETER : STATUS_SUCCESS),
                 "Got unexpected return code %#x.\n", status);
    if (status == STATUS_SUCCESS)
    {
        close_adapter_desc.hAdapter = open_adapter_hdc_desc.hAdapter;
        status = pD3DKMTCloseAdapter(&close_adapter_desc);
        ok(status == STATUS_SUCCESS, "Got unexpected return code %#x.\n", status);
    }
}

static void test_D3DKMTCloseAdapter(void)
{
    D3DKMT_CLOSEADAPTER close_adapter_desc;
    NTSTATUS status;

    if (!pD3DKMTCloseAdapter || pD3DKMTCloseAdapter(NULL) == STATUS_PROCEDURE_NOT_FOUND)
    {
        win_skip("D3DKMTCloseAdapter() is unavailable.\n");
        return;
    }

    /* Invalid parameters */
    status = pD3DKMTCloseAdapter(NULL);
    ok(status == STATUS_INVALID_PARAMETER, "Got unexpected return code %#x.\n", status);

    memset(&close_adapter_desc, 0, sizeof(close_adapter_desc));
    status = pD3DKMTCloseAdapter(&close_adapter_desc);
    ok(status == STATUS_INVALID_PARAMETER, "Got unexpected return code %#x.\n", status);
}

static void test_D3DKMTCreateDevice(void)
{
    D3DKMT_OPENADAPTERFROMGDIDISPLAYNAME open_adapter_gdi_desc;
    D3DKMT_CREATEDEVICE create_device_desc;
    D3DKMT_CLOSEADAPTER close_adapter_desc;
    D3DKMT_DESTROYDEVICE destroy_device_desc;
    NTSTATUS status;

    if (!pD3DKMTCreateDevice || pD3DKMTCreateDevice(NULL) == STATUS_PROCEDURE_NOT_FOUND)
    {
        win_skip("D3DKMTCreateDevice() is unavailable.\n");
        return;
    }

    /* Invalid parameters */
    status = pD3DKMTCreateDevice(NULL);
    ok(status == STATUS_INVALID_PARAMETER, "Got unexpected return code %#x.\n", status);

    memset(&create_device_desc, 0, sizeof(create_device_desc));
    status = pD3DKMTCreateDevice(&create_device_desc);
    ok(status == STATUS_INVALID_PARAMETER, "Got unexpected return code %#x.\n", status);

    lstrcpyW(open_adapter_gdi_desc.DeviceName, display1W);
    status = pD3DKMTOpenAdapterFromGdiDisplayName(&open_adapter_gdi_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#x.\n", status);

    /* Create device */
    create_device_desc.hAdapter = open_adapter_gdi_desc.hAdapter;
    status = pD3DKMTCreateDevice(&create_device_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#x.\n", status);
    ok(create_device_desc.hDevice, "Expect not null.\n");
    ok(create_device_desc.pCommandBuffer == NULL, "Expect null.\n");
    ok(create_device_desc.CommandBufferSize == 0, "Got wrong value %#x.\n", create_device_desc.CommandBufferSize);
    ok(create_device_desc.pAllocationList == NULL, "Expect null.\n");
    ok(create_device_desc.AllocationListSize == 0, "Got wrong value %#x.\n", create_device_desc.AllocationListSize);
    ok(create_device_desc.pPatchLocationList == NULL, "Expect null.\n");
    ok(create_device_desc.PatchLocationListSize == 0, "Got wrong value %#x.\n", create_device_desc.PatchLocationListSize);

    destroy_device_desc.hDevice = create_device_desc.hDevice;
    status = pD3DKMTDestroyDevice(&destroy_device_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#x.\n", status);

    close_adapter_desc.hAdapter = open_adapter_gdi_desc.hAdapter;
    status = pD3DKMTCloseAdapter(&close_adapter_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#x.\n", status);
}

static void test_D3DKMTDestroyDevice(void)
{
    D3DKMT_DESTROYDEVICE destroy_device_desc;
    NTSTATUS status;

    if (!pD3DKMTDestroyDevice || pD3DKMTDestroyDevice(NULL) == STATUS_PROCEDURE_NOT_FOUND)
    {
        win_skip("D3DKMTDestroyDevice() is unavailable.\n");
        return;
    }

    /* Invalid parameters */
    status = pD3DKMTDestroyDevice(NULL);
    ok(status == STATUS_INVALID_PARAMETER, "Got unexpected return code %#x.\n", status);

    memset(&destroy_device_desc, 0, sizeof(destroy_device_desc));
    status = pD3DKMTDestroyDevice(&destroy_device_desc);
    ok(status == STATUS_INVALID_PARAMETER, "Got unexpected return code %#x.\n", status);
}

static void test_D3DKMTCheckVidPnExclusiveOwnership(void)
{
    static const DWORD timeout = 1000;
    static const DWORD wait_step = 100;
    D3DKMT_CREATEDEVICE create_device_desc, create_device_desc2;
    D3DKMT_OPENADAPTERFROMGDIDISPLAYNAME open_adapter_gdi_desc;
    D3DKMT_CHECKVIDPNEXCLUSIVEOWNERSHIP check_owner_desc;
    D3DKMT_DESTROYDEVICE destroy_device_desc;
    D3DKMT_CLOSEADAPTER close_adapter_desc;
    D3DKMT_VIDPNSOURCEOWNER_TYPE owner_type;
    D3DKMT_SETVIDPNSOURCEOWNER set_owner_desc;
    DWORD total_time;
    NTSTATUS status;
    INT i;

    /* Test cases using single device */
    static const struct test_data1
    {
        D3DKMT_VIDPNSOURCEOWNER_TYPE owner_type;
        NTSTATUS expected_set_status;
        NTSTATUS expected_check_status;
    } tests1[] = {
        /* 0 */
        {-1, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_UNOWNED, STATUS_SUCCESS, STATUS_SUCCESS},
        {-1, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_SHARED, STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE, STATUS_SUCCESS},
        {-1, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE, STATUS_SUCCESS, STATUS_GRAPHICS_PRESENT_OCCLUDED},
        {-1, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVEGDI, STATUS_INVALID_PARAMETER, STATUS_SUCCESS},
        {-1, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_EMULATED, STATUS_SUCCESS, STATUS_SUCCESS},
        /* 10 */
        {-1, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_UNOWNED, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_UNOWNED, STATUS_SUCCESS, STATUS_SUCCESS},
        {-1, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_UNOWNED, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_SHARED, STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE, STATUS_SUCCESS},
        {-1, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_UNOWNED, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE, STATUS_SUCCESS, STATUS_GRAPHICS_PRESENT_OCCLUDED},
        {-1, STATUS_SUCCESS, STATUS_SUCCESS},
        /* 20 */
        {D3DKMT_VIDPNSOURCEOWNER_UNOWNED, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_EMULATED, STATUS_SUCCESS, STATUS_SUCCESS},
        {-1, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_SHARED, STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_UNOWNED, STATUS_SUCCESS, STATUS_SUCCESS},
        {-1, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_SHARED, STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_SHARED, STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE, STATUS_SUCCESS},
        {-1, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_SHARED, STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE, STATUS_SUCCESS},
        /* 30 */
        {D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE, STATUS_SUCCESS, STATUS_GRAPHICS_PRESENT_OCCLUDED},
        {-1, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_SHARED, STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_EMULATED, STATUS_SUCCESS, STATUS_SUCCESS},
        {-1, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE, STATUS_SUCCESS, STATUS_GRAPHICS_PRESENT_OCCLUDED},
        {D3DKMT_VIDPNSOURCEOWNER_UNOWNED, STATUS_SUCCESS, STATUS_SUCCESS},
        {-1, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE, STATUS_SUCCESS, STATUS_GRAPHICS_PRESENT_OCCLUDED},
        {D3DKMT_VIDPNSOURCEOWNER_SHARED, STATUS_INVALID_PARAMETER, STATUS_GRAPHICS_PRESENT_OCCLUDED},
        /* 40 */
        {-1, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE, STATUS_SUCCESS, STATUS_GRAPHICS_PRESENT_OCCLUDED},
        {D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE, STATUS_SUCCESS, STATUS_GRAPHICS_PRESENT_OCCLUDED},
        {-1, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE, STATUS_SUCCESS, STATUS_GRAPHICS_PRESENT_OCCLUDED},
        {D3DKMT_VIDPNSOURCEOWNER_EMULATED, STATUS_INVALID_PARAMETER, STATUS_GRAPHICS_PRESENT_OCCLUDED},
        {-1, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_EMULATED, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_UNOWNED, STATUS_SUCCESS, STATUS_SUCCESS},
        {-1, STATUS_SUCCESS, STATUS_SUCCESS},
        /* 50 */
        {D3DKMT_VIDPNSOURCEOWNER_EMULATED, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_SHARED, STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE, STATUS_SUCCESS},
        {-1, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_EMULATED, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE, STATUS_INVALID_PARAMETER, STATUS_SUCCESS},
        {-1, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_EMULATED, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_EMULATED, STATUS_SUCCESS, STATUS_SUCCESS},
        {-1, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_EMULATED + 1, STATUS_INVALID_PARAMETER, STATUS_SUCCESS},
    };

    /* Test cases using two devices consecutively */
    static const struct test_data2
    {
        D3DKMT_VIDPNSOURCEOWNER_TYPE set_owner_type1;
        D3DKMT_VIDPNSOURCEOWNER_TYPE set_owner_type2;
        NTSTATUS expected_set_status1;
        NTSTATUS expected_set_status2;
        NTSTATUS expected_check_status;
    } tests2[] = {
        /* 0 */
        {D3DKMT_VIDPNSOURCEOWNER_UNOWNED, D3DKMT_VIDPNSOURCEOWNER_UNOWNED, STATUS_SUCCESS, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_UNOWNED, D3DKMT_VIDPNSOURCEOWNER_SHARED, STATUS_SUCCESS, STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_UNOWNED, D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE, STATUS_SUCCESS, STATUS_SUCCESS, STATUS_GRAPHICS_PRESENT_OCCLUDED},
        {D3DKMT_VIDPNSOURCEOWNER_UNOWNED, D3DKMT_VIDPNSOURCEOWNER_EMULATED, STATUS_SUCCESS, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_SHARED, D3DKMT_VIDPNSOURCEOWNER_UNOWNED, STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_SHARED, D3DKMT_VIDPNSOURCEOWNER_SHARED, STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE, STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_SHARED, D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE, STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE, STATUS_SUCCESS, STATUS_GRAPHICS_PRESENT_OCCLUDED},
        {D3DKMT_VIDPNSOURCEOWNER_SHARED, D3DKMT_VIDPNSOURCEOWNER_EMULATED, STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE, D3DKMT_VIDPNSOURCEOWNER_UNOWNED, STATUS_SUCCESS, STATUS_SUCCESS, STATUS_GRAPHICS_PRESENT_OCCLUDED},
        {D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE, D3DKMT_VIDPNSOURCEOWNER_SHARED, STATUS_SUCCESS, STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE, STATUS_GRAPHICS_PRESENT_OCCLUDED},
        /* 10 */
        {D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE, D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE, STATUS_SUCCESS, STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE, STATUS_GRAPHICS_PRESENT_OCCLUDED},
        {D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE, D3DKMT_VIDPNSOURCEOWNER_EMULATED, STATUS_SUCCESS, STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE, STATUS_GRAPHICS_PRESENT_OCCLUDED},
        {D3DKMT_VIDPNSOURCEOWNER_EMULATED, D3DKMT_VIDPNSOURCEOWNER_UNOWNED, STATUS_SUCCESS, STATUS_SUCCESS, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_EMULATED, D3DKMT_VIDPNSOURCEOWNER_SHARED, STATUS_SUCCESS, STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_EMULATED, D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE, STATUS_SUCCESS, STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE, STATUS_SUCCESS},
        {D3DKMT_VIDPNSOURCEOWNER_EMULATED, D3DKMT_VIDPNSOURCEOWNER_EMULATED, STATUS_SUCCESS, STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE, STATUS_SUCCESS},
        {-1, D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE, -1, STATUS_SUCCESS, STATUS_GRAPHICS_PRESENT_OCCLUDED},
        {D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE, -1, STATUS_SUCCESS, STATUS_SUCCESS, STATUS_GRAPHICS_PRESENT_OCCLUDED},
    };

    if (!pD3DKMTCheckVidPnExclusiveOwnership || pD3DKMTCheckVidPnExclusiveOwnership(NULL) == STATUS_PROCEDURE_NOT_FOUND)
    {
        skip("D3DKMTCheckVidPnExclusiveOwnership() is unavailable.\n");
        return;
    }

    /* Invalid parameters */
    status = pD3DKMTCheckVidPnExclusiveOwnership(NULL);
    ok(status == STATUS_INVALID_PARAMETER, "Got unexpected return code %#x.\n", status);

    memset(&check_owner_desc, 0, sizeof(check_owner_desc));
    status = pD3DKMTCheckVidPnExclusiveOwnership(&check_owner_desc);
    ok(status == STATUS_INVALID_PARAMETER, "Got unexpected return code %#x.\n", status);

    /* Test cases */
    lstrcpyW(open_adapter_gdi_desc.DeviceName, display1W);
    status = pD3DKMTOpenAdapterFromGdiDisplayName(&open_adapter_gdi_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#x.\n", status);

    memset(&create_device_desc, 0, sizeof(create_device_desc));
    create_device_desc.hAdapter = open_adapter_gdi_desc.hAdapter;
    status = pD3DKMTCreateDevice(&create_device_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#x.\n", status);

    check_owner_desc.hAdapter = open_adapter_gdi_desc.hAdapter;
    check_owner_desc.VidPnSourceId = open_adapter_gdi_desc.VidPnSourceId;
    for (i = 0; i < ARRAY_SIZE(tests1); ++i)
    {
        set_owner_desc.hDevice = create_device_desc.hDevice;
        if (tests1[i].owner_type != -1)
        {
            owner_type = tests1[i].owner_type;
            set_owner_desc.pType = &owner_type;
            set_owner_desc.pVidPnSourceId = &open_adapter_gdi_desc.VidPnSourceId;
            set_owner_desc.VidPnSourceCount = 1;
        }
        else
        {
            set_owner_desc.pType = NULL;
            set_owner_desc.pVidPnSourceId = NULL;
            set_owner_desc.VidPnSourceCount = 0;
        }
        status = pD3DKMTSetVidPnSourceOwner(&set_owner_desc);
        ok(status == tests1[i].expected_set_status ||
               /* win8 doesn't support D3DKMT_VIDPNSOURCEOWNER_EMULATED */
               (status == STATUS_INVALID_PARAMETER && tests1[i].owner_type == D3DKMT_VIDPNSOURCEOWNER_EMULATED)
               || (status == STATUS_SUCCESS && tests1[i].owner_type == D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE
                   && tests1[i - 1].owner_type == D3DKMT_VIDPNSOURCEOWNER_EMULATED),
           "Got unexpected return code %#x at test %d.\n", status, i);

        status = pD3DKMTCheckVidPnExclusiveOwnership(&check_owner_desc);
        /* If don't sleep, D3DKMTCheckVidPnExclusiveOwnership may get STATUS_GRAPHICS_PRESENT_UNOCCLUDED instead
         * of STATUS_SUCCESS */
        if ((tests1[i].expected_check_status == STATUS_SUCCESS && status == STATUS_GRAPHICS_PRESENT_UNOCCLUDED))
        {
            total_time = 0;
            do
            {
                Sleep(wait_step);
                total_time += wait_step;
                status = pD3DKMTCheckVidPnExclusiveOwnership(&check_owner_desc);
            } while (status == STATUS_GRAPHICS_PRESENT_UNOCCLUDED && total_time < timeout);
        }
        ok(status == tests1[i].expected_check_status
               || (status == STATUS_GRAPHICS_PRESENT_OCCLUDED                               /* win8 */
                   && tests1[i].owner_type == D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE
                   && tests1[i - 1].owner_type == D3DKMT_VIDPNSOURCEOWNER_EMULATED),
           "Got unexpected return code %#x at test %d.\n", status, i);
    }

    /* Set owner and unset owner using different devices */
    memset(&create_device_desc2, 0, sizeof(create_device_desc2));
    create_device_desc2.hAdapter = open_adapter_gdi_desc.hAdapter;
    status = pD3DKMTCreateDevice(&create_device_desc2);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#x.\n", status);

    /* Set owner with the first device */
    set_owner_desc.hDevice = create_device_desc.hDevice;
    owner_type = D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE;
    set_owner_desc.pType = &owner_type;
    set_owner_desc.pVidPnSourceId = &open_adapter_gdi_desc.VidPnSourceId;
    set_owner_desc.VidPnSourceCount = 1;
    status = pD3DKMTSetVidPnSourceOwner(&set_owner_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#x.\n", status);
    status = pD3DKMTCheckVidPnExclusiveOwnership(&check_owner_desc);
    ok(status == STATUS_GRAPHICS_PRESENT_OCCLUDED, "Got unexpected return code %#x.\n", status);

    /* Unset owner with the second device */
    set_owner_desc.hDevice = create_device_desc2.hDevice;
    set_owner_desc.pType = NULL;
    set_owner_desc.pVidPnSourceId = NULL;
    set_owner_desc.VidPnSourceCount = 0;
    status = pD3DKMTSetVidPnSourceOwner(&set_owner_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#x.\n", status);
    status = pD3DKMTCheckVidPnExclusiveOwnership(&check_owner_desc);
    /* No effect */
    ok(status == STATUS_GRAPHICS_PRESENT_OCCLUDED, "Got unexpected return code %#x.\n", status);

    /* Unset owner with the first device */
    set_owner_desc.hDevice = create_device_desc.hDevice;
    set_owner_desc.pType = NULL;
    set_owner_desc.pVidPnSourceId = NULL;
    set_owner_desc.VidPnSourceCount = 0;
    status = pD3DKMTSetVidPnSourceOwner(&set_owner_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#x.\n", status);
    status = pD3DKMTCheckVidPnExclusiveOwnership(&check_owner_desc);
    /* Proves that the correct device is needed to unset owner */
    ok(status == STATUS_SUCCESS || status == STATUS_GRAPHICS_PRESENT_UNOCCLUDED, "Got unexpected return code %#x.\n",
       status);

    /* Set owner with the first device, set owner again with the second device */
    for (i = 0; i < ARRAY_SIZE(tests2); ++i)
    {
        if (tests2[i].set_owner_type1 != -1)
        {
            set_owner_desc.hDevice = create_device_desc.hDevice;
            owner_type = tests2[i].set_owner_type1;
            set_owner_desc.pType = &owner_type;
            set_owner_desc.pVidPnSourceId = &open_adapter_gdi_desc.VidPnSourceId;
            set_owner_desc.VidPnSourceCount = 1;
            /* If don't sleep, D3DKMTSetVidPnSourceOwner may return STATUS_OK for D3DKMT_VIDPNSOURCEOWNER_SHARED.
             * Other owner type doesn't seems to be affected. */
            if (tests2[i].set_owner_type1 == D3DKMT_VIDPNSOURCEOWNER_SHARED)
                Sleep(timeout);
            status = pD3DKMTSetVidPnSourceOwner(&set_owner_desc);
            ok(status == tests2[i].expected_set_status1
                   || (status == STATUS_INVALID_PARAMETER                                    /* win8 */
                       && tests2[i].set_owner_type1 == D3DKMT_VIDPNSOURCEOWNER_EMULATED),
               "Got unexpected return code %#x at test %d.\n", status, i);
        }

        if (tests2[i].set_owner_type2 != -1)
        {
            set_owner_desc.hDevice = create_device_desc2.hDevice;
            owner_type = tests2[i].set_owner_type2;
            set_owner_desc.pType = &owner_type;
            set_owner_desc.pVidPnSourceId = &open_adapter_gdi_desc.VidPnSourceId;
            set_owner_desc.VidPnSourceCount = 1;
            status = pD3DKMTSetVidPnSourceOwner(&set_owner_desc);
            ok(status == tests2[i].expected_set_status2
                   || (status == STATUS_INVALID_PARAMETER                                   /* win8 */
                       && tests2[i].set_owner_type2 == D3DKMT_VIDPNSOURCEOWNER_EMULATED)
                   || (status == STATUS_SUCCESS && tests2[i].set_owner_type1 == D3DKMT_VIDPNSOURCEOWNER_EMULATED
                       && tests2[i].set_owner_type2 == D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE),
               "Got unexpected return code %#x at test %d.\n", status, i);
        }

        status = pD3DKMTCheckVidPnExclusiveOwnership(&check_owner_desc);
        if ((tests2[i].expected_check_status == STATUS_SUCCESS && status == STATUS_GRAPHICS_PRESENT_UNOCCLUDED))
        {
            total_time = 0;
            do
            {
                Sleep(wait_step);
                total_time += wait_step;
                status = pD3DKMTCheckVidPnExclusiveOwnership(&check_owner_desc);
            } while (status == STATUS_GRAPHICS_PRESENT_UNOCCLUDED && total_time < timeout);
        }
        ok(status == tests2[i].expected_check_status
               || (status == STATUS_GRAPHICS_PRESENT_OCCLUDED                               /* win8 */
                   && tests2[i].set_owner_type2 == D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE
                   && tests2[i].set_owner_type1 == D3DKMT_VIDPNSOURCEOWNER_EMULATED),
           "Got unexpected return code %#x at test %d.\n", status, i);

        /* Unset owner with first device */
        if (tests2[i].set_owner_type1 != -1)
        {
            set_owner_desc.hDevice = create_device_desc.hDevice;
            set_owner_desc.pType = NULL;
            set_owner_desc.pVidPnSourceId = NULL;
            set_owner_desc.VidPnSourceCount = 0;
            status = pD3DKMTSetVidPnSourceOwner(&set_owner_desc);
            ok(status == STATUS_SUCCESS, "Got unexpected return code %#x at test %d.\n", status, i);
        }

        /* Unset owner with second device */
        if (tests2[i].set_owner_type2 != -1)
        {
            set_owner_desc.hDevice = create_device_desc2.hDevice;
            set_owner_desc.pType = NULL;
            set_owner_desc.pVidPnSourceId = NULL;
            set_owner_desc.VidPnSourceCount = 0;
            status = pD3DKMTSetVidPnSourceOwner(&set_owner_desc);
            ok(status == STATUS_SUCCESS, "Got unexpected return code %#x at test %d.\n", status, i);
        }
    }

    /* Destroy devices holding ownership */
    set_owner_desc.hDevice = create_device_desc.hDevice;
    owner_type = D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE;
    set_owner_desc.pType = &owner_type;
    set_owner_desc.pVidPnSourceId = &open_adapter_gdi_desc.VidPnSourceId;
    set_owner_desc.VidPnSourceCount = 1;
    status = pD3DKMTSetVidPnSourceOwner(&set_owner_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#x.\n", status);

    destroy_device_desc.hDevice = create_device_desc.hDevice;
    status = pD3DKMTDestroyDevice(&destroy_device_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#x.\n", status);

    set_owner_desc.hDevice = create_device_desc2.hDevice;
    owner_type = D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE;
    set_owner_desc.pType = &owner_type;
    set_owner_desc.pVidPnSourceId = &open_adapter_gdi_desc.VidPnSourceId;
    set_owner_desc.VidPnSourceCount = 1;
    status = pD3DKMTSetVidPnSourceOwner(&set_owner_desc);
    /* So ownership is released when device is destroyed. otherwise the return code should be
     * STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE */
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#x.\n", status);

    destroy_device_desc.hDevice = create_device_desc2.hDevice;
    status = pD3DKMTDestroyDevice(&destroy_device_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#x.\n", status);

    close_adapter_desc.hAdapter = open_adapter_gdi_desc.hAdapter;
    status = pD3DKMTCloseAdapter(&close_adapter_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#x.\n", status);
}

static void test_D3DKMTSetVidPnSourceOwner(void)
{
    D3DKMT_SETVIDPNSOURCEOWNER set_owner_desc = {0};
    NTSTATUS status;

    if (!pD3DKMTSetVidPnSourceOwner || pD3DKMTSetVidPnSourceOwner(&set_owner_desc) == STATUS_PROCEDURE_NOT_FOUND)
    {
        skip("D3DKMTSetVidPnSourceOwner() is unavailable.\n");
        return;
    }

    /* Invalid parameters */
    status = pD3DKMTSetVidPnSourceOwner(&set_owner_desc);
    ok(status == STATUS_INVALID_PARAMETER, "Got unexpected return code %#x.\n", status);
}

static void test_D3DKMTCheckOcclusion(void)
{
    DISPLAY_DEVICEW display_device = {sizeof(display_device)};
    D3DKMT_OPENADAPTERFROMGDIDISPLAYNAME open_adapter_gdi_desc;
    D3DKMT_CHECKVIDPNEXCLUSIVEOWNERSHIP check_owner_desc;
    D3DKMT_SETVIDPNSOURCEOWNER set_owner_desc;
    D3DKMT_DESTROYDEVICE destroy_device_desc;
    D3DKMT_VIDPNSOURCEOWNER_TYPE owner_type;
    D3DKMT_CLOSEADAPTER close_adapter_desc;
    D3DKMT_CREATEDEVICE create_device_desc;
    D3DKMT_CHECKOCCLUSION occlusion_desc;
    NTSTATUS expected_occlusion, status;
    INT i, adapter_count = 0;
    HWND hwnd, hwnd2;
    HRESULT hr;

    if (!pD3DKMTCheckOcclusion || pD3DKMTCheckOcclusion(NULL) == STATUS_PROCEDURE_NOT_FOUND)
    {
        skip("D3DKMTCheckOcclusion() is unavailable.\n");
        return;
    }

    /* NULL parameter check */
    status = pD3DKMTCheckOcclusion(NULL);
    ok(status == STATUS_INVALID_PARAMETER, "Got unexpected return code %#x.\n", status);

    occlusion_desc.hWnd = NULL;
    status = pD3DKMTCheckOcclusion(&occlusion_desc);
    ok(status == STATUS_INVALID_PARAMETER, "Got unexpected return code %#x.\n", status);

    hwnd = CreateWindowA("static", "static1", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 200, 200, 0, 0, 0, 0);
    ok(hwnd != NULL, "Failed to create window.\n");

    occlusion_desc.hWnd = hwnd;
    status = pD3DKMTCheckOcclusion(&occlusion_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#x.\n", status);

    /* Minimized state doesn't affect D3DKMTCheckOcclusion */
    ShowWindow(hwnd, SW_MINIMIZE);
    occlusion_desc.hWnd = hwnd;
    status = pD3DKMTCheckOcclusion(&occlusion_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#x.\n", status);
    ShowWindow(hwnd, SW_SHOWNORMAL);

    /* Invisible state doesn't affect D3DKMTCheckOcclusion */
    ShowWindow(hwnd, SW_HIDE);
    occlusion_desc.hWnd = hwnd;
    status = pD3DKMTCheckOcclusion(&occlusion_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#x.\n", status);
    ShowWindow(hwnd, SW_SHOW);

    /* hwnd2 covers hwnd */
    hwnd2 = CreateWindowA("static", "static2", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100, 200, 200, 0, 0, 0, 0);
    ok(hwnd2 != NULL, "Failed to create window.\n");

    occlusion_desc.hWnd = hwnd;
    status = pD3DKMTCheckOcclusion(&occlusion_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#x.\n", status);

    occlusion_desc.hWnd = hwnd2;
    status = pD3DKMTCheckOcclusion(&occlusion_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#x.\n", status);

    /* Composition doesn't affect D3DKMTCheckOcclusion */
    if (pDwmEnableComposition)
    {
        hr = pDwmEnableComposition(DWM_EC_DISABLECOMPOSITION);
        ok(hr == S_OK, "Failed to disable composition.\n");

        occlusion_desc.hWnd = hwnd;
        status = pD3DKMTCheckOcclusion(&occlusion_desc);
        /* This result means that D3DKMTCheckOcclusion doesn't check composition status despite MSDN says it will */
        ok(status == STATUS_SUCCESS, "Got unexpected return code %#x.\n", status);

        occlusion_desc.hWnd = hwnd2;
        status = pD3DKMTCheckOcclusion(&occlusion_desc);
        ok(status == STATUS_SUCCESS, "Got unexpected return code %#x.\n", status);

        ShowWindow(hwnd, SW_MINIMIZE);
        occlusion_desc.hWnd = hwnd;
        status = pD3DKMTCheckOcclusion(&occlusion_desc);
        ok(status == STATUS_SUCCESS, "Got unexpected return code %#x.\n", status);
        ShowWindow(hwnd, SW_SHOWNORMAL);

        ShowWindow(hwnd, SW_HIDE);
        occlusion_desc.hWnd = hwnd;
        status = pD3DKMTCheckOcclusion(&occlusion_desc);
        ok(status == STATUS_SUCCESS, "Got unexpected return code %#x.\n", status);
        ShowWindow(hwnd, SW_SHOW);

        hr = pDwmEnableComposition(DWM_EC_ENABLECOMPOSITION);
        ok(hr == S_OK, "Failed to enable composition.\n");
    }
    else
        skip("Skip testing composition.\n");

    lstrcpyW(open_adapter_gdi_desc.DeviceName, display1W);
    status = pD3DKMTOpenAdapterFromGdiDisplayName(&open_adapter_gdi_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#x.\n", status);

    memset(&create_device_desc, 0, sizeof(create_device_desc));
    create_device_desc.hAdapter = open_adapter_gdi_desc.hAdapter;
    status = pD3DKMTCreateDevice(&create_device_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#x.\n", status);

    check_owner_desc.hAdapter = open_adapter_gdi_desc.hAdapter;
    check_owner_desc.VidPnSourceId = open_adapter_gdi_desc.VidPnSourceId;
    status = pD3DKMTCheckVidPnExclusiveOwnership(&check_owner_desc);
    /* D3DKMTCheckVidPnExclusiveOwnership gets STATUS_GRAPHICS_PRESENT_UNOCCLUDED sometimes and with some delay,
     * it will always return STATUS_SUCCESS. So there are some timing issues here. */
    ok(status == STATUS_SUCCESS || status == STATUS_GRAPHICS_PRESENT_UNOCCLUDED, "Got unexpected return code %#x.\n", status);

    /* Test D3DKMTCheckOcclusion relationship with video present source owner */
    set_owner_desc.hDevice = create_device_desc.hDevice;
    owner_type = D3DKMT_VIDPNSOURCEOWNER_EXCLUSIVE;
    set_owner_desc.pType = &owner_type;
    set_owner_desc.pVidPnSourceId = &open_adapter_gdi_desc.VidPnSourceId;
    set_owner_desc.VidPnSourceCount = 1;
    status = pD3DKMTSetVidPnSourceOwner(&set_owner_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#x.\n", status);

    for (i = 0; EnumDisplayDevicesW(NULL, i, &display_device, 0); ++i)
    {
        if ((display_device.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP))
            adapter_count++;
    }
    /* STATUS_GRAPHICS_PRESENT_OCCLUDED on single monitor system. STATUS_SUCCESS on multiple monitor system. */
    expected_occlusion = adapter_count > 1 ? STATUS_SUCCESS : STATUS_GRAPHICS_PRESENT_OCCLUDED;

    occlusion_desc.hWnd = hwnd;
    status = pD3DKMTCheckOcclusion(&occlusion_desc);
    ok(status == expected_occlusion, "Got unexpected return code %#x.\n", status);

    /* Note hwnd2 is not actually occluded but D3DKMTCheckOcclusion reports STATUS_GRAPHICS_PRESENT_OCCLUDED as well */
    SetWindowPos(hwnd2, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    ShowWindow(hwnd2, SW_SHOW);
    occlusion_desc.hWnd = hwnd2;
    status = pD3DKMTCheckOcclusion(&occlusion_desc);
    ok(status == expected_occlusion, "Got unexpected return code %#x.\n", status);

    /* Now hwnd is HWND_TOPMOST. Still reports STATUS_GRAPHICS_PRESENT_OCCLUDED */
    ok(SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE), "Failed to SetWindowPos.\n");
    ok(GetWindowLongW(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST, "No WS_EX_TOPMOST style.\n");
    occlusion_desc.hWnd = hwnd;
    status = pD3DKMTCheckOcclusion(&occlusion_desc);
    ok(status == expected_occlusion, "Got unexpected return code %#x.\n", status);

    DestroyWindow(hwnd2);
    occlusion_desc.hWnd = hwnd;
    status = pD3DKMTCheckOcclusion(&occlusion_desc);
    ok(status == expected_occlusion, "Got unexpected return code %#x.\n", status);

    check_owner_desc.hAdapter = open_adapter_gdi_desc.hAdapter;
    check_owner_desc.VidPnSourceId = open_adapter_gdi_desc.VidPnSourceId;
    status = pD3DKMTCheckVidPnExclusiveOwnership(&check_owner_desc);
    ok(status == STATUS_GRAPHICS_PRESENT_OCCLUDED, "Got unexpected return code %#x.\n", status);

    /* Unset video present source owner */
    set_owner_desc.hDevice = create_device_desc.hDevice;
    set_owner_desc.pType = NULL;
    set_owner_desc.pVidPnSourceId = NULL;
    set_owner_desc.VidPnSourceCount = 0;
    status = pD3DKMTSetVidPnSourceOwner(&set_owner_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#x.\n", status);

    occlusion_desc.hWnd = hwnd;
    status = pD3DKMTCheckOcclusion(&occlusion_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#x.\n", status);

    check_owner_desc.hAdapter = open_adapter_gdi_desc.hAdapter;
    check_owner_desc.VidPnSourceId = open_adapter_gdi_desc.VidPnSourceId;
    status = pD3DKMTCheckVidPnExclusiveOwnership(&check_owner_desc);
    ok(status == STATUS_SUCCESS || status == STATUS_GRAPHICS_PRESENT_UNOCCLUDED, "Got unexpected return code %#x.\n", status);

    destroy_device_desc.hDevice = create_device_desc.hDevice;
    status = pD3DKMTDestroyDevice(&destroy_device_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#x.\n", status);

    close_adapter_desc.hAdapter = open_adapter_gdi_desc.hAdapter;
    status = pD3DKMTCloseAdapter(&close_adapter_desc);
    ok(status == STATUS_SUCCESS, "Got unexpected return code %#x.\n", status);
    DestroyWindow(hwnd);
}

START_TEST(driver)
{
    HMODULE gdi32 = GetModuleHandleA("gdi32.dll");
    HMODULE dwmapi = LoadLibraryA("dwmapi.dll");

    pD3DKMTCheckOcclusion = (void *)GetProcAddress(gdi32, "D3DKMTCheckOcclusion");
    pD3DKMTCheckVidPnExclusiveOwnership = (void *)GetProcAddress(gdi32, "D3DKMTCheckVidPnExclusiveOwnership");
    pD3DKMTCloseAdapter = (void *)GetProcAddress(gdi32, "D3DKMTCloseAdapter");
    pD3DKMTCreateDevice = (void *)GetProcAddress(gdi32, "D3DKMTCreateDevice");
    pD3DKMTDestroyDevice = (void *)GetProcAddress(gdi32, "D3DKMTDestroyDevice");
    pD3DKMTOpenAdapterFromGdiDisplayName = (void *)GetProcAddress(gdi32, "D3DKMTOpenAdapterFromGdiDisplayName");
    pD3DKMTOpenAdapterFromHdc = (void *)GetProcAddress(gdi32, "D3DKMTOpenAdapterFromHdc");
    pD3DKMTSetVidPnSourceOwner = (void *)GetProcAddress(gdi32, "D3DKMTSetVidPnSourceOwner");

    if (dwmapi)
        pDwmEnableComposition = (void *)GetProcAddress(dwmapi, "DwmEnableComposition");

    test_D3DKMTOpenAdapterFromGdiDisplayName();
    test_D3DKMTOpenAdapterFromHdc();
    test_D3DKMTCloseAdapter();
    test_D3DKMTCreateDevice();
    test_D3DKMTDestroyDevice();
    test_D3DKMTCheckVidPnExclusiveOwnership();
    test_D3DKMTSetVidPnSourceOwner();
    test_D3DKMTCheckOcclusion();

    FreeLibrary(dwmapi);
}
