/*
 * Raw Input
 *
 * Copyright 2012 Henri Verbeet
 * Copyright 2018 Zebediah Figura for CodeWeavers
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

#include "config.h"
#include <stdarg.h>

#define NONAMELESSUNION
#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winnls.h"
#include "winreg.h"
#include "winuser.h"
#include "setupapi.h"
#include "ddk/hidsdi.h"
#include "wine/debug.h"
#include "wine/server.h"
#include "wine/hid.h"

#include "user_private.h"

#include "initguid.h"
#include "ntddmou.h"

WINE_DEFAULT_DEBUG_CHANNEL(rawinput);

struct device
{
    WCHAR *path;
    HANDLE file;
    HANDLE handle;
    RID_DEVICE_INFO info;
    PHIDP_PREPARSED_DATA data;
};

static struct device *rawinput_devices;
static unsigned int rawinput_devices_count, rawinput_devices_max;

static CRITICAL_SECTION rawinput_devices_cs;
static CRITICAL_SECTION_DEBUG rawinput_devices_cs_debug =
{
    0, 0, &rawinput_devices_cs,
    { &rawinput_devices_cs_debug.ProcessLocksList, &rawinput_devices_cs_debug.ProcessLocksList },
      0, 0, { (DWORD_PTR)(__FILE__ ": rawinput_devices_cs") }
};
static CRITICAL_SECTION rawinput_devices_cs = { &rawinput_devices_cs_debug, -1, 0, 0, 0, 0 };

static BOOL array_reserve(void **elements, unsigned int *capacity, unsigned int count, unsigned int size)
{
    unsigned int new_capacity, max_capacity;
    void *new_elements;

    if (count <= *capacity)
        return TRUE;

    max_capacity = ~(SIZE_T)0 / size;
    if (count > max_capacity)
        return FALSE;

    new_capacity = max(4, *capacity);
    while (new_capacity < count && new_capacity <= max_capacity / 2)
        new_capacity *= 2;
    if (new_capacity < count)
        new_capacity = max_capacity;

    if (!(new_elements = heap_realloc(*elements, new_capacity * size)))
        return FALSE;

    *elements = new_elements;
    *capacity = new_capacity;

    return TRUE;
}

static struct device *add_device(HDEVINFO set, SP_DEVICE_INTERFACE_DATA *iface)
{
    SP_DEVICE_INTERFACE_DETAIL_DATA_W *detail;
    struct device *device;
    HANDLE file;
    WCHAR *path;
    DWORD size;

    SetupDiGetDeviceInterfaceDetailW(set, iface, NULL, 0, &size, NULL);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
    {
        ERR("Failed to get device path, error %#x.\n", GetLastError());
        return FALSE;
    }
    if (!(detail = heap_alloc(size)))
    {
        ERR("Failed to allocate memory.\n");
        return FALSE;
    }
    detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
    SetupDiGetDeviceInterfaceDetailW(set, iface, detail, size, NULL, NULL);

    TRACE("Found HID device %s.\n", debugstr_w(detail->DevicePath));

    if (!(path = heap_strdupW(detail->DevicePath)))
    {
        ERR("Failed to allocate memory.\n");
        heap_free(detail);
        return NULL;
    }
    heap_free(detail);

    file = CreateFileW(path, GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);
    if (file == INVALID_HANDLE_VALUE)
    {
        ERR("Failed to open device file %s, error %u.\n", debugstr_w(path), GetLastError());
        heap_free(path);
        return NULL;
    }

    if (!array_reserve((void **)&rawinput_devices, &rawinput_devices_max,
            rawinput_devices_count + 1, sizeof(*rawinput_devices)))
    {
        ERR("Failed to allocate memory.\n");
        CloseHandle(file);
        heap_free(path);
        return NULL;
    }

    device = &rawinput_devices[rawinput_devices_count++];
    device->path = path;
    device->file = file;
    device->info.cbSize = sizeof(RID_DEVICE_INFO);
    device->handle = INVALID_HANDLE_VALUE;

    return device;
}

extern DWORD WINAPI GetFinalPathNameByHandleW(HANDLE file, LPWSTR path, DWORD charcount, DWORD flags);
static void find_devices(BOOL);

HANDLE rawinput_handle_from_device_handle(HANDLE device, BOOL rescan)
{
    WCHAR buffer[sizeof(OBJECT_NAME_INFORMATION) + MAX_PATH + 1];
    OBJECT_NAME_INFORMATION *info = (OBJECT_NAME_INFORMATION*)&buffer;
    ULONG dummy;
    unsigned int i;
 
    for (i = 0; i < rawinput_devices_count; ++i)
    {
        if (rawinput_devices[i].handle == device)
            return &rawinput_devices[i];
    }

    if (NtQueryObject( device, ObjectNameInformation, &buffer, sizeof(buffer) - sizeof(WCHAR), &dummy ) || !info->Name.Buffer)
        return NULL;

    /* replace \??\ with \\?\ to match rawinput_devices paths */
    if (info->Name.Length > 1 && info->Name.Buffer[0] == '\\' && info->Name.Buffer[1] == '?')
        info->Name.Buffer[1] = '\\';

    for (i = 0; i < rawinput_devices_count; ++i)
    {
        if (strcmpW(rawinput_devices[i].path, info->Name.Buffer) == 0)
        {
            rawinput_devices[i].handle = device;
            return &rawinput_devices[i];
        }
    }

    if (!rescan)
        return NULL;

    find_devices(TRUE);

    return rawinput_handle_from_device_handle(device, FALSE);
}

static void find_devices_by_guid(const GUID *guid)
{
    SP_DEVICE_INTERFACE_DATA iface = { sizeof(iface) };
    struct device *device;
    HIDD_ATTRIBUTES attr;
    HIDP_CAPS caps;
    HDEVINFO set;
    DWORD idx;

    set = SetupDiGetClassDevsW(guid, NULL, NULL, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);

    for (idx = 0; SetupDiEnumDeviceInterfaces(set, NULL, guid, idx, &iface); ++idx)
    {
        if (!(device = add_device(set, &iface)))
            continue;

        attr.Size = sizeof(HIDD_ATTRIBUTES);
        if (!HidD_GetAttributes(device->file, &attr))
            WARN("Failed to get attributes.\n");

        device->info.dwType = RIM_TYPEHID;
        device->info.u.hid.dwVendorId = attr.VendorID;
        device->info.u.hid.dwProductId = attr.ProductID;
        device->info.u.hid.dwVersionNumber = attr.VersionNumber;

        if (!HidD_GetPreparsedData(device->file, &device->data))
            WARN("Failed to get preparsed data.\n");

        if (!HidP_GetCaps(device->data, &caps))
            WARN("Failed to get caps.\n");

        device->info.u.hid.usUsagePage = caps.UsagePage;
        device->info.u.hid.usUsage = caps.Usage;
    }

    SetupDiDestroyDeviceInfoList(set);

    set = SetupDiGetClassDevsW(&GUID_DEVINTERFACE_MOUSE, NULL, NULL, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);

    for (idx = 0; SetupDiEnumDeviceInterfaces(set, NULL, &GUID_DEVINTERFACE_MOUSE, idx, &iface); ++idx)
    {
        static const RID_DEVICE_INFO_MOUSE mouse_info = {1, 5, 0, FALSE};

        if (!(device = add_device(set, &iface)))
            continue;

        device->info.dwType = RIM_TYPEMOUSE;
        device->info.u.mouse = mouse_info;
    }

    SetupDiDestroyDeviceInfoList(set);

}

static void find_devices(BOOL force)
{
    static ULONGLONG last_check;

    DWORD idx;
    GUID hid_guid;

    if (!force && GetTickCount64() - last_check < 2000)
        return;

    HidD_GetHidGuid(&hid_guid);

    EnterCriticalSection(&rawinput_devices_cs);

    if (!force && GetTickCount64() - last_check < 2000)
    {
        LeaveCriticalSection(&rawinput_devices_cs);
        return;
    }

    last_check = GetTickCount64();

    /* destroy previous list */
    for (idx = 0; idx < rawinput_devices_count; ++idx)
    {
        CloseHandle(rawinput_devices[idx].file);
        heap_free(rawinput_devices[idx].path);
    }

    rawinput_devices_count = 0;

    find_devices_by_guid(&hid_guid);

    /* HACK: also look up the xinput-specific devices */
    hid_guid.Data4[7]++;
    find_devices_by_guid(&hid_guid);

    LeaveCriticalSection(&rawinput_devices_cs);
}

/***********************************************************************
 *              GetRawInputDeviceList   (USER32.@)
 */
UINT WINAPI GetRawInputDeviceList(RAWINPUTDEVICELIST *devices, UINT *device_count, UINT size)
{
    UINT i;

    TRACE("devices %p, device_count %p, size %u.\n", devices, device_count, size);

    if (size != sizeof(*devices))
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return ~0U;
    }

    if (!device_count)
    {
        SetLastError(ERROR_NOACCESS);
        return ~0U;
    }

    find_devices(FALSE);

    if (!devices)
    {
        *device_count = 2 + rawinput_devices_count;
        return 0;
    }

    if (*device_count < 2 + rawinput_devices_count)
    {
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        *device_count = 2 + rawinput_devices_count;
        return ~0U;
    }

    devices[0].hDevice = WINE_MOUSE_HANDLE;
    devices[0].dwType = RIM_TYPEMOUSE;
    devices[1].hDevice = WINE_KEYBOARD_HANDLE;
    devices[1].dwType = RIM_TYPEKEYBOARD;

    for (i = 0; i < rawinput_devices_count; ++i)
    {
        devices[2 + i].hDevice = &rawinput_devices[i];
        devices[2 + i].dwType = rawinput_devices[i].info.dwType;
    }

    return 2 + rawinput_devices_count;
}

/***********************************************************************
 *              RegisterRawInputDevices   (USER32.@)
 */
BOOL WINAPI DECLSPEC_HOTPATCH RegisterRawInputDevices(RAWINPUTDEVICE *devices, UINT device_count, UINT size)
{
    struct rawinput_device *d;
    BOOL ret;
    UINT i;

    TRACE("devices %p, device_count %u, size %u.\n", devices, device_count, size);

    if (size != sizeof(*devices))
    {
        WARN("Invalid structure size %u.\n", size);
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    for (i = 0; i < device_count; ++i)
    {
        if ((devices[i].dwFlags & RIDEV_INPUTSINK) &&
            (devices[i].hwndTarget == NULL))
        {
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
        }

        if ((devices[i].dwFlags & RIDEV_REMOVE) &&
            (devices[i].hwndTarget != NULL))
        {
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
        }
    }

    if (!(d = HeapAlloc( GetProcessHeap(), 0, device_count * sizeof(*d) ))) return FALSE;

    for (i = 0; i < device_count; ++i)
    {
        TRACE("device %u: page %#x, usage %#x, flags %#x, target %p.\n",
                i, devices[i].usUsagePage, devices[i].usUsage,
                devices[i].dwFlags, devices[i].hwndTarget);
        if (devices[i].dwFlags & ~(RIDEV_REMOVE|RIDEV_NOLEGACY|RIDEV_INPUTSINK))
            FIXME("Unhandled flags %#x for device %u.\n", devices[i].dwFlags, i);

        d[i].usage_page = devices[i].usUsagePage;
        d[i].usage = devices[i].usUsage;
        d[i].flags = devices[i].dwFlags;
        d[i].target = wine_server_user_handle( devices[i].hwndTarget );
    }

    SERVER_START_REQ( update_rawinput_devices )
    {
        wine_server_add_data( req, d, device_count * sizeof(*d) );
        ret = !wine_server_call( req );
    }
    SERVER_END_REQ;

    HeapFree( GetProcessHeap(), 0, d );

    return ret;
}

/***********************************************************************
 *              GetRawInputData   (USER32.@)
 */
UINT WINAPI GetRawInputData(HRAWINPUT rawinput, UINT command, void *data, UINT *data_size, UINT header_size)
{
    RAWINPUT *ri = (RAWINPUT *)rawinput;
    UINT s;

    TRACE("rawinput %p, command %#x, data %p, data_size %p, header_size %u.\n",
            rawinput, command, data, data_size, header_size);

    if (!ri)
        return ~0U;

    if (header_size != sizeof(RAWINPUTHEADER))
    {
        WARN("Invalid structure size %u.\n", header_size);
        return ~0U;
    }

    switch (command)
    {
    case RID_INPUT:
        s = ri->header.dwSize;
        break;
    case RID_HEADER:
        s = sizeof(RAWINPUTHEADER);
        break;
    default:
        return ~0U;
    }

    if (!data)
    {
        *data_size = s;
        return 0;
    }

    if (*data_size < s) return ~0U;
    memcpy(data, ri, s);
    return s;
}

/***********************************************************************
 *              GetRawInputBuffer   (USER32.@)
 */
UINT WINAPI DECLSPEC_HOTPATCH GetRawInputBuffer(RAWINPUT *data, UINT *data_size, UINT header_size)
{
    FIXME("data %p, data_size %p, header_size %u stub!\n", data, data_size, header_size);

    return 0;
}

/***********************************************************************
 *              GetRawInputDeviceInfoA   (USER32.@)
 */
UINT WINAPI GetRawInputDeviceInfoA(HANDLE device, UINT command, void *data, UINT *data_size)
{
    TRACE("device %p, command %#x, data %p, data_size %p.\n",
            device, command, data, data_size);

    /* RIDI_DEVICENAME data_size is in chars, not bytes */
    if (command == RIDI_DEVICENAME)
    {
        WCHAR *nameW;
        UINT ret, nameW_sz;

        if (!data_size) return ~0U;

        nameW_sz = *data_size;

        if (data && nameW_sz > 0)
            nameW = HeapAlloc(GetProcessHeap(), 0, sizeof(WCHAR) * nameW_sz);
        else
            nameW = NULL;

        ret = GetRawInputDeviceInfoW(device, command, nameW, &nameW_sz);

        if (ret && ret != ~0U)
            WideCharToMultiByte(CP_ACP, 0, nameW, -1, data, *data_size, NULL, NULL);

        *data_size = nameW_sz;

        HeapFree(GetProcessHeap(), 0, nameW);

        return ret;
    }

    return GetRawInputDeviceInfoW(device, command, data, data_size);
}

/***********************************************************************
 *              GetRawInputDeviceInfoW   (USER32.@)
 */
UINT WINAPI GetRawInputDeviceInfoW(HANDLE handle, UINT command, void *data, UINT *data_size)
{
    /* FIXME: Most of this is made up. */
    static const WCHAR keyboard_name[] = {'\\','\\','?','\\','W','I','N','E','_','K','E','Y','B','O','A','R','D',0};
    static const WCHAR mouse_name[] = {'\\','\\','?','\\','W','I','N','E','_','M','O','U','S','E',0};
    static const RID_DEVICE_INFO_KEYBOARD keyboard_info = {0, 0, 1, 12, 3, 101};
    static const RID_DEVICE_INFO_MOUSE mouse_info = {1, 5, 0, FALSE};

    RID_DEVICE_INFO info;
    struct device *device = handle;
    const void *to_copy;
    UINT to_copy_bytes, avail_bytes;

    TRACE("handle %p, command %#x, data %p, data_size %p.\n",
            handle, command, data, data_size);

    if (!data_size) return ~0U;
    if (!device) return ~0U;

    /* each case below must set:
     *     *data_size: length (meaning defined by command) of data we want to copy
     *     avail_bytes: number of bytes available in user buffer
     *     to_copy_bytes: number of bytes we want to copy into user buffer
     *     to_copy: pointer to data we want to copy into user buffer
     */
    switch (command)
    {
    case RIDI_DEVICENAME:
        /* for RIDI_DEVICENAME, data_size is in characters, not bytes */
        avail_bytes = *data_size * sizeof(WCHAR);
        if (handle == WINE_MOUSE_HANDLE)
        {
            *data_size = ARRAY_SIZE(mouse_name);
            to_copy = mouse_name;
        }
        else if (handle == WINE_KEYBOARD_HANDLE)
        {
            *data_size = ARRAY_SIZE(keyboard_name);
            to_copy = keyboard_name;
        }
        else
        {
            *data_size = strlenW(device->path) + 1;
            to_copy = device->path;
        }
        to_copy_bytes = *data_size * sizeof(WCHAR);
        break;

    case RIDI_DEVICEINFO:
        avail_bytes = *data_size;
        info.cbSize = sizeof(info);
        if (handle == WINE_MOUSE_HANDLE)
        {
            info.dwType = RIM_TYPEMOUSE;
            info.u.mouse = mouse_info;
        }
        else if (handle == WINE_KEYBOARD_HANDLE)
        {
            info.dwType = RIM_TYPEKEYBOARD;
            info.u.keyboard = keyboard_info;
        }
        else
        {
            info = device->info;
        }
        to_copy_bytes = sizeof(info);
        *data_size = to_copy_bytes;
        to_copy = &info;
        break;

    case RIDI_PREPARSEDDATA:
        avail_bytes = *data_size;
        if (handle == WINE_MOUSE_HANDLE || handle == WINE_KEYBOARD_HANDLE ||
            device->info.dwType != RIM_TYPEHID)
        {
            to_copy_bytes = 0;
            *data_size = 0;
            to_copy = NULL;
        }
        else
        {
            to_copy_bytes = ((WINE_HIDP_PREPARSED_DATA*)device->data)->dwSize;
            *data_size = to_copy_bytes;
            to_copy = device->data;
        }
        break;

    default:
        FIXME("command %#x not supported\n", command);
        return ~0U;
    }

    if (!data)
        return 0;

    if (avail_bytes < to_copy_bytes)
        return ~0U;

    memcpy(data, to_copy, to_copy_bytes);

    return *data_size;
}

static int compare_raw_input_devices(const void *ap, const void *bp)
{
    const RAWINPUTDEVICE a = *(const RAWINPUTDEVICE *)ap;
    const RAWINPUTDEVICE b = *(const RAWINPUTDEVICE *)bp;

    if (a.usUsagePage != b.usUsagePage) return a.usUsagePage - b.usUsagePage;
    if (a.usUsage != b.usUsage) return a.usUsage - b.usUsage;
    return 0;
}

/***********************************************************************
 *              GetRegisteredRawInputDevices   (USER32.@)
 */
UINT WINAPI DECLSPEC_HOTPATCH GetRegisteredRawInputDevices(RAWINPUTDEVICE *devices, UINT *device_count, UINT size)
{
    struct rawinput_device *d = NULL;
    unsigned int count = ~0U;

    TRACE("devices %p, device_count %p, size %u\n", devices, device_count, size);

    if (!device_count)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return ~0U;
    }

    if (devices && !(d = HeapAlloc( GetProcessHeap(), 0, *device_count * sizeof(*d) )))
        return ~0U;

    SERVER_START_REQ( get_rawinput_devices )
    {
        if (d)
            wine_server_set_reply( req, d, *device_count * sizeof(*d) );

        if (wine_server_call( req ))
            goto done;

        if (!d || reply->device_count > *device_count)
        {
            *device_count = reply->device_count;
            SetLastError( ERROR_INSUFFICIENT_BUFFER );
            goto done;
        }

        for (count = 0; count < reply->device_count; ++count)
        {
            devices[count].usUsagePage = d[count].usage_page;
            devices[count].usUsage = d[count].usage;
            devices[count].dwFlags = d[count].flags;
            devices[count].hwndTarget = wine_server_ptr_handle(d[count].target);
        }
    }
    SERVER_END_REQ;

    qsort(devices, count, sizeof(*devices), compare_raw_input_devices);

done:
    if (d) HeapFree( GetProcessHeap(), 0, d );
    return count;
}


/***********************************************************************
 *              DefRawInputProc   (USER32.@)
 */
LRESULT WINAPI DefRawInputProc(RAWINPUT **data, INT data_count, UINT header_size)
{
    FIXME("data %p, data_count %d, header_size %u stub!\n", data, data_count, header_size);

    return 0;
}
