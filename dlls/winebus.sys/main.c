/*
 * WINE Platform native bus driver
 *
 * Copyright 2016 Aric Stewart
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
#include <assert.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winnls.h"
#include "winternl.h"
#include "winioctl.h"
#include "hidusage.h"
#include "ddk/wdm.h"
#include "ddk/hidport.h"
#include "ddk/hidtypes.h"
#include "ddk/hidpddi.h"
#include "wine/asm.h"
#include "wine/debug.h"
#include "wine/list.h"
#include "wine/unixlib.h"

#include "unixlib.h"

WINE_DEFAULT_DEBUG_CHANNEL(hid);

static DRIVER_OBJECT *driver_obj;

static DEVICE_OBJECT *mouse_obj;
static DEVICE_OBJECT *keyboard_obj;

/* The root-enumerated device stack. */
static DEVICE_OBJECT *bus_pdo;
static DEVICE_OBJECT *bus_fdo;

static HANDLE driver_key;

struct hid_report
{
    struct list entry;
    ULONG length;
    BYTE buffer[1];
};

enum device_state
{
    DEVICE_STATE_STOPPED,
    DEVICE_STATE_STARTED,
    DEVICE_STATE_REMOVED,
};

struct device_extension
{
    struct list entry;
    DEVICE_OBJECT *device;

    CRITICAL_SECTION cs;
    enum device_state state;

    struct device_desc desc;
    DWORD index;

    BYTE *report_desc;
    UINT report_desc_length;
    HIDP_DEVICE_DESC collection_desc;

    struct hid_report *last_reports[256];
    struct list reports;
    IRP *pending_read;

    UINT64 unix_device;
};

static CRITICAL_SECTION device_list_cs;
static CRITICAL_SECTION_DEBUG critsect_debug =
{
    0, 0, &device_list_cs,
    { &critsect_debug.ProcessLocksList, &critsect_debug.ProcessLocksList },
      0, 0, { (DWORD_PTR)(__FILE__ ": device_list_cs") }
};
static CRITICAL_SECTION device_list_cs = { &critsect_debug, -1, 0, 0, 0, 0 };

static struct list device_list = LIST_INIT(device_list);

static NTSTATUS winebus_call(unsigned int code, void *args)
{
    return WINE_UNIX_CALL(code, args);
}

static void unix_device_remove(DEVICE_OBJECT *device)
{
    struct device_extension *ext = (struct device_extension *)device->DeviceExtension;
    struct device_remove_params params = {.device = ext->unix_device};
    winebus_call(device_remove, &params);
}

static NTSTATUS unix_device_start(DEVICE_OBJECT *device)
{
    struct device_extension *ext = (struct device_extension *)device->DeviceExtension;
    struct device_start_params params = {.device = ext->unix_device};
    return winebus_call(device_start, &params);
}

static NTSTATUS unix_device_get_report_descriptor(DEVICE_OBJECT *device, BYTE *buffer, UINT length, UINT *out_length)
{
    struct device_extension *ext = (struct device_extension *)device->DeviceExtension;
    struct device_descriptor_params params =
    {
        .device = ext->unix_device,
        .buffer = buffer,
        .length = length,
        .out_length = out_length
    };
    return winebus_call(device_get_report_descriptor, &params);
}

static void unix_device_set_output_report(DEVICE_OBJECT *device, HID_XFER_PACKET *packet, IO_STATUS_BLOCK *io)
{
    struct device_extension *ext = (struct device_extension *)device->DeviceExtension;
    struct device_report_params params =
    {
        .device = ext->unix_device,
        .packet = packet,
        .io = io,
    };
    winebus_call(device_set_output_report, &params);
}

static void unix_device_get_feature_report(DEVICE_OBJECT *device, HID_XFER_PACKET *packet, IO_STATUS_BLOCK *io)
{
    struct device_extension *ext = (struct device_extension *)device->DeviceExtension;
    struct device_report_params params =
    {
        .device = ext->unix_device,
        .packet = packet,
        .io = io,
    };
    winebus_call(device_get_feature_report, &params);
}

static void unix_device_set_feature_report(DEVICE_OBJECT *device, HID_XFER_PACKET *packet, IO_STATUS_BLOCK *io)
{
    struct device_extension *ext = (struct device_extension *)device->DeviceExtension;
    struct device_report_params params =
    {
        .device = ext->unix_device,
        .packet = packet,
        .io = io,
    };
    winebus_call(device_set_feature_report, &params);
}

static DWORD get_device_index(struct device_desc *desc)
{
    struct device_extension *ext;
    DWORD index = 0;

    LIST_FOR_EACH_ENTRY(ext, &device_list, struct device_extension, entry)
    {
        if (ext->desc.vid == desc->vid && ext->desc.pid == desc->pid && ext->desc.input == desc->input)
            index = max(ext->index + 1, index);
    }

    return index;
}

static WCHAR *get_instance_id(DEVICE_OBJECT *device)
{
    struct device_extension *ext = (struct device_extension *)device->DeviceExtension;
    DWORD len = wcslen(ext->desc.serialnumber) + 33;
    WCHAR *dst;

    if ((dst = ExAllocatePool(PagedPool, len * sizeof(WCHAR))))
        swprintf(dst, len, L"%i&%s&%x&%i", ext->desc.version, ext->desc.serialnumber, ext->desc.uid, ext->index);

    return dst;
}

static WCHAR *get_device_id(DEVICE_OBJECT *device)
{
    static const WCHAR input_format[] = L"&MI_%02u";
    static const WCHAR winebus_format[] = L"WINEBUS\\VID_%04X&PID_%04X";
    struct device_extension *ext = (struct device_extension *)device->DeviceExtension;
    DWORD pos = 0, len = 0, input_len = 0, winebus_len = 25;
    WCHAR *dst;

    if (ext->desc.input != -1) input_len = 14;

    len += winebus_len + input_len + 1;

    if ((dst = ExAllocatePool(PagedPool, len * sizeof(WCHAR))))
    {
        pos += swprintf(dst + pos, len - pos, winebus_format, ext->desc.vid, ext->desc.pid);
        if (input_len) pos += swprintf(dst + pos, len - pos, input_format, ext->desc.input);
    }

    return dst;
}

static WCHAR *get_hardware_ids(DEVICE_OBJECT *device)
{
    static const WCHAR input_format[] = L"&MI_%02u";
    static const WCHAR winebus_format[] = L"WINEBUS\\VID_%04X&PID_%04X";
    struct device_extension *ext = (struct device_extension *)device->DeviceExtension;
    DWORD pos = 0, len = 0, input_len = 0, winebus_len = 25;
    WCHAR *dst;

    if (ext->desc.input != -1) input_len = 14;

    len += winebus_len + input_len + 1;

    if ((dst = ExAllocatePool(PagedPool, (len + 1) * sizeof(WCHAR))))
    {
        pos += swprintf(dst + pos, len - pos, winebus_format, ext->desc.vid, ext->desc.pid);
        if (input_len) pos += swprintf(dst + pos, len - pos, input_format, ext->desc.input);
        pos += 1;
        dst[pos] = 0;
    }

    return dst;
}

static WCHAR *get_compatible_ids(DEVICE_OBJECT *device)
{
    static const WCHAR xinput_compat[] = L"WINEBUS\\WINE_COMP_XINPUT";
    static const WCHAR hid_compat[] = L"WINEBUS\\WINE_COMP_HID";
    struct device_extension *ext = (struct device_extension *)device->DeviceExtension;
    DWORD size = sizeof(hid_compat);
    WCHAR *dst;

    if (ext->desc.is_gamepad) size += sizeof(xinput_compat);

    if ((dst = ExAllocatePool(PagedPool, size + sizeof(WCHAR))))
    {
        if (ext->desc.is_gamepad) memcpy(dst, xinput_compat, sizeof(xinput_compat));
        memcpy((char *)dst + size - sizeof(hid_compat), hid_compat, sizeof(hid_compat));
        dst[size / sizeof(WCHAR)] = 0;
    }

    return dst;
}

static IRP *pop_pending_read(struct device_extension *ext)
{
    IRP *pending;

    RtlEnterCriticalSection(&ext->cs);
    pending = ext->pending_read;
    ext->pending_read = NULL;
    RtlLeaveCriticalSection(&ext->cs);

    return pending;
}

static void remove_pending_irps(DEVICE_OBJECT *device)
{
    struct device_extension *ext = device->DeviceExtension;
    IRP *pending;

    if ((pending = pop_pending_read(ext)))
    {
        pending->IoStatus.Status = STATUS_DELETE_PENDING;
        pending->IoStatus.Information = 0;
        IoCompleteRequest(pending, IO_NO_INCREMENT);
    }
}

static DEVICE_OBJECT *bus_create_hid_device(struct device_desc *desc, UINT64 unix_device)
{
    struct device_extension *ext;
    DEVICE_OBJECT *device;
    UNICODE_STRING nameW;
    WCHAR dev_name[256];
    NTSTATUS status;

    TRACE("desc %s, unix_device %#I64x\n", debugstr_device_desc(desc), unix_device);

    swprintf(dev_name, ARRAY_SIZE(dev_name), L"\\Device\\WINEBUS#%p", unix_device);
    RtlInitUnicodeString(&nameW, dev_name);
    status = IoCreateDevice(driver_obj, sizeof(struct device_extension), &nameW, 0, 0, FALSE, &device);
    if (status)
    {
        FIXME("failed to create device error %#lx\n", status);
        return NULL;
    }

    RtlEnterCriticalSection(&device_list_cs);

    /* fill out device_extension struct */
    ext = (struct device_extension *)device->DeviceExtension;
    ext->device             = device;
    ext->desc               = *desc;
    ext->index              = get_device_index(desc);
    ext->unix_device        = unix_device;
    list_init(&ext->reports);

    InitializeCriticalSection(&ext->cs);
    ext->cs.DebugInfo->Spare[0] = (DWORD_PTR)(__FILE__ ": cs");

    /* add to list of pnp devices */
    list_add_tail(&device_list, &ext->entry);

    RtlLeaveCriticalSection(&device_list_cs);

    TRACE("created device %p/%#I64x\n", device, unix_device);
    return device;
}

static DEVICE_OBJECT *bus_find_unix_device(UINT64 unix_device)
{
    struct device_extension *ext;

    LIST_FOR_EACH_ENTRY(ext, &device_list, struct device_extension, entry)
        if (ext->unix_device == unix_device) return ext->device;

    return NULL;
}

static void bus_unlink_hid_device(DEVICE_OBJECT *device)
{
    struct device_extension *ext = (struct device_extension *)device->DeviceExtension;

    RtlEnterCriticalSection(&device_list_cs);
    list_remove(&ext->entry);
    RtlLeaveCriticalSection(&device_list_cs);
}

#ifdef __ASM_USE_FASTCALL_WRAPPER
extern void * WINAPI wrap_fastcall_func1(void *func, const void *a);
__ASM_STDCALL_FUNC(wrap_fastcall_func1, 8,
                   "popl %ecx\n\t"
                   "popl %eax\n\t"
                   "xchgl (%esp),%ecx\n\t"
                   "jmp *%eax");
#define call_fastcall_func1(func,a) wrap_fastcall_func1(func,a)
#else
#define call_fastcall_func1(func,a) func(a)
#endif

static NTSTATUS build_device_relations(DEVICE_RELATIONS **devices)
{
    struct device_extension *ext;
    int i;

    RtlEnterCriticalSection(&device_list_cs);
    *devices = ExAllocatePool(PagedPool, offsetof(DEVICE_RELATIONS, Objects[list_count(&device_list)]));

    if (!*devices)
    {
        RtlLeaveCriticalSection(&device_list_cs);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    i = 0;
    LIST_FOR_EACH_ENTRY(ext, &device_list, struct device_extension, entry)
    {
        (*devices)->Objects[i] = ext->device;
        call_fastcall_func1(ObfReferenceObject, ext->device);
        i++;
    }
    RtlLeaveCriticalSection(&device_list_cs);
    (*devices)->Count = i;
    return STATUS_SUCCESS;
}

static DWORD check_bus_option(const WCHAR *option, DWORD default_value)
{
    char buffer[FIELD_OFFSET(KEY_VALUE_PARTIAL_INFORMATION, Data[sizeof(DWORD)])];
    KEY_VALUE_PARTIAL_INFORMATION *info = (KEY_VALUE_PARTIAL_INFORMATION *)buffer;
    UNICODE_STRING str;
    DWORD size;

    RtlInitUnicodeString(&str, option);

    if (NtQueryValueKey(driver_key, &str, KeyValuePartialInformation, info, sizeof(buffer), &size) == STATUS_SUCCESS)
    {
        if (info->Type == REG_DWORD) return *(DWORD *)info->Data;
    }

    return default_value;
}

static BOOL deliver_next_report(struct device_extension *ext, IRP *irp)
{
    struct hid_report *report;
    struct list *entry;
    ULONG i;

    if (!(entry = list_head(&ext->reports))) return FALSE;
    report = LIST_ENTRY(entry, struct hid_report, entry);
    list_remove(&report->entry);

    memcpy(irp->UserBuffer, report->buffer, report->length);
    irp->IoStatus.Information = report->length;
    irp->IoStatus.Status = STATUS_SUCCESS;

    if (TRACE_ON(hid))
    {
        TRACE("device %p/%#I64x input report length %lu:\n", ext->device, ext->unix_device, report->length);
        for (i = 0; i < report->length;)
        {
            char buffer[256], *buf = buffer;
            buf += sprintf(buf, "%08lx ", i);
            do { buf += sprintf(buf, " %02x", report->buffer[i]); }
            while (++i % 16 && i < report->length);
            TRACE("%s\n", buffer);
        }
    }

    RtlFreeHeap(GetProcessHeap(), 0, report);
    return TRUE;
}

static void process_hid_report(DEVICE_OBJECT *device, BYTE *report_buf, DWORD report_len)
{
    struct device_extension *ext = (struct device_extension *)device->DeviceExtension;
    ULONG size = offsetof(struct hid_report, buffer[report_len]);
    struct hid_report *report, *last_report;
    IRP *irp;

    if (!(report = RtlAllocateHeap(GetProcessHeap(), 0, size))) return;
    memcpy(report->buffer, report_buf, report_len);
    report->length = report_len;

    RtlEnterCriticalSection(&ext->cs);
    list_add_tail(&ext->reports, &report->entry);

    if (!ext->collection_desc.ReportIDs[0].ReportID) last_report = ext->last_reports[0];
    else last_report = ext->last_reports[report_buf[0]];
    memcpy(last_report->buffer, report_buf, report_len);

    if ((irp = pop_pending_read(ext)))
    {
        deliver_next_report(ext, irp);
        IoCompleteRequest(irp, IO_NO_INCREMENT);
    }
    RtlLeaveCriticalSection(&ext->cs);
}

static NTSTATUS handle_IRP_MN_QUERY_DEVICE_RELATIONS(IRP *irp)
{
    NTSTATUS status = irp->IoStatus.Status;
    IO_STACK_LOCATION *irpsp = IoGetCurrentIrpStackLocation( irp );

    TRACE("IRP_MN_QUERY_DEVICE_RELATIONS\n");
    switch (irpsp->Parameters.QueryDeviceRelations.Type)
    {
        case EjectionRelations:
        case RemovalRelations:
        case TargetDeviceRelation:
        case PowerRelations:
            FIXME("Unhandled Device Relation %x\n",irpsp->Parameters.QueryDeviceRelations.Type);
            break;
        case BusRelations:
            status = build_device_relations((DEVICE_RELATIONS**)&irp->IoStatus.Information);
            break;
        default:
            FIXME("Unknown Device Relation %x\n",irpsp->Parameters.QueryDeviceRelations.Type);
            break;
    }

    return status;
}

static NTSTATUS handle_IRP_MN_QUERY_ID(DEVICE_OBJECT *device, IRP *irp)
{
    NTSTATUS status = irp->IoStatus.Status;
    IO_STACK_LOCATION *irpsp = IoGetCurrentIrpStackLocation( irp );
    BUS_QUERY_ID_TYPE type = irpsp->Parameters.QueryId.IdType;

    TRACE("(%p, %p)\n", device, irp);

    switch (type)
    {
        case BusQueryHardwareIDs:
            TRACE("BusQueryHardwareIDs\n");
            irp->IoStatus.Information = (ULONG_PTR)get_hardware_ids(device);
            break;
        case BusQueryCompatibleIDs:
            TRACE("BusQueryCompatibleIDs\n");
            irp->IoStatus.Information = (ULONG_PTR)get_compatible_ids(device);
            break;
        case BusQueryDeviceID:
            TRACE("BusQueryDeviceID\n");
            irp->IoStatus.Information = (ULONG_PTR)get_device_id(device);
            break;
        case BusQueryInstanceID:
            TRACE("BusQueryInstanceID\n");
            irp->IoStatus.Information = (ULONG_PTR)get_instance_id(device);
            break;
        default:
            FIXME("Unhandled type %08x\n", type);
            return status;
    }

    status = irp->IoStatus.Information ? STATUS_SUCCESS : STATUS_NO_MEMORY;
    return status;
}

static void mouse_device_create(void)
{
    struct device_create_params params = {{0}};

    if (winebus_call(mouse_create, &params)) return;
    mouse_obj = bus_create_hid_device(&params.desc, params.device);
    IoInvalidateDeviceRelations(bus_pdo, BusRelations);
}

static void keyboard_device_create(void)
{
    struct device_create_params params = {{0}};

    if (winebus_call(keyboard_create, &params)) return;
    keyboard_obj = bus_create_hid_device(&params.desc, params.device);
    IoInvalidateDeviceRelations(bus_pdo, BusRelations);
}

static DWORD bus_count;
static HANDLE bus_thread[16];

struct bus_main_params
{
    const WCHAR *name;

    void *init_args;
    HANDLE init_done;
    unsigned int init_code;
    unsigned int wait_code;
    struct bus_event *bus_event;
};

static DWORD CALLBACK bus_main_thread(void *args)
{
    struct bus_main_params bus = *(struct bus_main_params *)args;
    DEVICE_OBJECT *device;
    NTSTATUS status;

    TRACE("%s main loop starting\n", debugstr_w(bus.name));
    status = winebus_call(bus.init_code, bus.init_args);
    SetEvent(bus.init_done);
    TRACE("%s main loop started\n", debugstr_w(bus.name));

    bus.bus_event->type = BUS_EVENT_TYPE_NONE;
    if (status) WARN("%s bus init returned status %#lx\n", debugstr_w(bus.name), status);
    else while ((status = winebus_call(bus.wait_code, bus.bus_event)) == STATUS_PENDING)
    {
        struct bus_event *event = bus.bus_event;
        switch (event->type)
        {
        case BUS_EVENT_TYPE_NONE: break;
        case BUS_EVENT_TYPE_DEVICE_REMOVED:
            RtlEnterCriticalSection(&device_list_cs);
            device = bus_find_unix_device(event->device);
            if (!device) WARN("could not find device for %s bus device %#I64x\n", debugstr_w(bus.name), event->device);
            else bus_unlink_hid_device(device);
            RtlLeaveCriticalSection(&device_list_cs);
            IoInvalidateDeviceRelations(bus_pdo, BusRelations);
            break;
        case BUS_EVENT_TYPE_DEVICE_CREATED:
            device = bus_create_hid_device(&event->device_created.desc, event->device);
            if (device) IoInvalidateDeviceRelations(bus_pdo, BusRelations);
            else
            {
                struct device_remove_params params = {.device = event->device};
                WARN("failed to create device for %s bus device %#I64x\n", debugstr_w(bus.name), event->device);
                winebus_call(device_remove, &params);
            }
            break;
        case BUS_EVENT_TYPE_INPUT_REPORT:
            RtlEnterCriticalSection(&device_list_cs);
            device = bus_find_unix_device(event->device);
            if (!device) WARN("could not find device for %s bus device %#I64x\n", debugstr_w(bus.name), event->device);
            else process_hid_report(device, event->input_report.buffer, event->input_report.length);
            RtlLeaveCriticalSection(&device_list_cs);
            break;
        }
    }

    if (status) WARN("%s bus wait returned status %#lx\n", debugstr_w(bus.name), status);
    else TRACE("%s main loop exited\n", debugstr_w(bus.name));
    RtlFreeHeap(GetProcessHeap(), 0, bus.bus_event);
    return status;
}

static NTSTATUS bus_main_thread_start(struct bus_main_params *bus)
{
    DWORD i = bus_count++, max_size;

    if (!(bus->init_done = CreateEventW(NULL, FALSE, FALSE, NULL)))
    {
        ERR("failed to create %s bus init done event.\n", debugstr_w(bus->name));
        bus_count--;
        return STATUS_UNSUCCESSFUL;
    }

    max_size = offsetof(struct bus_event, input_report.buffer[0x10000]);
    if (!(bus->bus_event = RtlAllocateHeap(GetProcessHeap(), 0, max_size)))
    {
        ERR("failed to allocate %s bus event.\n", debugstr_w(bus->name));
        CloseHandle(bus->init_done);
        bus_count--;
        return STATUS_UNSUCCESSFUL;
    }

    if (!(bus_thread[i] = CreateThread(NULL, 0, bus_main_thread, bus, 0, NULL)))
    {
        ERR("failed to create %s bus thread.\n", debugstr_w(bus->name));
        CloseHandle(bus->init_done);
        bus_count--;
        return STATUS_UNSUCCESSFUL;
    }

    WaitForSingleObject(bus->init_done, INFINITE);
    CloseHandle(bus->init_done);
    return STATUS_SUCCESS;
}

static void sdl_bus_free_mappings(struct sdl_bus_options *options)
{
    DWORD count = options->mappings_count;
    char **mappings = options->mappings;

    while (count) RtlFreeHeap(GetProcessHeap(), 0, mappings[--count]);
    RtlFreeHeap(GetProcessHeap(), 0, mappings);
}

static void sdl_bus_load_mappings(struct sdl_bus_options *options)
{
    ULONG idx = 0, len, count = 0, capacity, info_size, info_max_size;
    KEY_VALUE_FULL_INFORMATION *info;
    OBJECT_ATTRIBUTES attr = {0};
    char **mappings = NULL;
    UNICODE_STRING path;
    NTSTATUS status;
    HANDLE key;

    options->mappings_count = 0;
    options->mappings = NULL;

    RtlInitUnicodeString(&path, L"map");
    InitializeObjectAttributes(&attr, &path, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, driver_key, NULL);
    status = NtOpenKey(&key, KEY_ALL_ACCESS, &attr);
    if (status) return;

    capacity = 1024;
    mappings = RtlAllocateHeap(GetProcessHeap(), 0, capacity * sizeof(*mappings));
    info_max_size = offsetof(KEY_VALUE_FULL_INFORMATION, Name) + 512;
    info = RtlAllocateHeap(GetProcessHeap(), 0, info_max_size);

    while (!status && info && mappings)
    {
        status = NtEnumerateValueKey(key, idx, KeyValueFullInformation, info, info_max_size, &info_size);
        while (status == STATUS_BUFFER_OVERFLOW)
        {
            info_max_size = info_size;
            if (!(info = RtlReAllocateHeap(GetProcessHeap(), 0, info, info_max_size))) break;
            status = NtEnumerateValueKey(key, idx, KeyValueFullInformation, info, info_max_size, &info_size);
        }

        if (status == STATUS_NO_MORE_ENTRIES)
        {
            options->mappings_count = count;
            options->mappings = mappings;
            goto done;
        }

        idx++;
        if (status) break;
        if (info->Type != REG_SZ) continue;

        RtlUnicodeToMultiByteSize(&len, (WCHAR *)((char *)info + info->DataOffset), info_size - info->DataOffset);
        if (!len) continue;

        if (!(mappings[count++] = RtlAllocateHeap(GetProcessHeap(), 0, len + 1))) break;
        if (count > capacity)
        {
            capacity = capacity * 3 / 2;
            if (!(mappings = RtlReAllocateHeap(GetProcessHeap(), 0, mappings, capacity * sizeof(*mappings))))
                break;
        }

        RtlUnicodeToMultiByteN(mappings[count], len, NULL, (WCHAR *)((char *)info + info->DataOffset),
                               info_size - info->DataOffset);
        if (mappings[len - 1]) mappings[len] = 0;
    }

    if (mappings) while (count) RtlFreeHeap(GetProcessHeap(), 0, mappings[--count]);
    RtlFreeHeap(GetProcessHeap(), 0, mappings);

done:
    RtlFreeHeap(GetProcessHeap(), 0, info);
    NtClose(key);
}

static NTSTATUS sdl_driver_init(void)
{
    struct sdl_bus_options bus_options;
    struct bus_main_params bus =
    {
        .name = L"SDL",
        .init_args = &bus_options,
        .init_code = sdl_init,
        .wait_code = sdl_wait,
    };
    NTSTATUS status;

    bus_options.split_controllers = check_bus_option(L"Split Controllers", 0);
    if (bus_options.split_controllers) TRACE("SDL controller splitting enabled\n");
    bus_options.map_controllers = check_bus_option(L"Map Controllers", 1);
    if (!bus_options.map_controllers) TRACE("SDL controller to XInput HID gamepad mapping disabled\n");
    sdl_bus_load_mappings(&bus_options);

    status = bus_main_thread_start(&bus);
    sdl_bus_free_mappings(&bus_options);
    return status;
}

static NTSTATUS udev_driver_init(void)
{
    struct udev_bus_options bus_options;
    struct bus_main_params bus =
    {
        .name = L"UDEV",
        .init_args = &bus_options,
        .init_code = udev_init,
        .wait_code = udev_wait,
    };

    bus_options.disable_hidraw = check_bus_option(L"DisableHidraw", 0);
    if (bus_options.disable_hidraw) TRACE("UDEV hidraw devices disabled in registry\n");
    bus_options.disable_input = check_bus_option(L"DisableInput", 0);
    if (bus_options.disable_input) TRACE("UDEV input devices disabled in registry\n");
    bus_options.disable_udevd = check_bus_option(L"DisableUdevd", 0);
    if (bus_options.disable_udevd) TRACE("UDEV udevd use disabled in registry\n");

    return bus_main_thread_start(&bus);
}

static NTSTATUS iohid_driver_init(void)
{
    struct iohid_bus_options bus_options;
    struct bus_main_params bus =
    {
        .name = L"IOHID",
        .init_args = &bus_options,
        .init_code = iohid_init,
        .wait_code = iohid_wait,
    };

    return bus_main_thread_start(&bus);
}

static NTSTATUS fdo_pnp_dispatch(DEVICE_OBJECT *device, IRP *irp)
{
    IO_STACK_LOCATION *irpsp = IoGetCurrentIrpStackLocation(irp);
    NTSTATUS ret;

    switch (irpsp->MinorFunction)
    {
    case IRP_MN_QUERY_DEVICE_RELATIONS:
        irp->IoStatus.Status = handle_IRP_MN_QUERY_DEVICE_RELATIONS(irp);
        break;
    case IRP_MN_START_DEVICE:
        mouse_device_create();
        keyboard_device_create();

        if (!check_bus_option(L"Enable SDL", 1) || sdl_driver_init())
        {
            udev_driver_init();
            iohid_driver_init();
        }

        irp->IoStatus.Status = STATUS_SUCCESS;
        break;
    case IRP_MN_SURPRISE_REMOVAL:
        irp->IoStatus.Status = STATUS_SUCCESS;
        break;
    case IRP_MN_REMOVE_DEVICE:
        winebus_call(sdl_stop, NULL);
        winebus_call(udev_stop, NULL);
        winebus_call(iohid_stop, NULL);

        WaitForMultipleObjects(bus_count, bus_thread, TRUE, INFINITE);
        while (bus_count--) CloseHandle(bus_thread[bus_count]);

        irp->IoStatus.Status = STATUS_SUCCESS;
        IoSkipCurrentIrpStackLocation(irp);
        ret = IoCallDriver(bus_pdo, irp);
        IoDetachDevice(bus_pdo);
        IoDeleteDevice(device);
        return ret;
    default:
        FIXME("Unhandled minor function %#x.\n", irpsp->MinorFunction);
    }

    IoSkipCurrentIrpStackLocation(irp);
    return IoCallDriver(bus_pdo, irp);
}

static NTSTATUS pdo_pnp_dispatch(DEVICE_OBJECT *device, IRP *irp)
{
    struct device_extension *ext = device->DeviceExtension;
    NTSTATUS status = irp->IoStatus.Status;
    IO_STACK_LOCATION *irpsp = IoGetCurrentIrpStackLocation(irp);
    struct hid_report *report, *next;
    HIDP_REPORT_IDS *reports;
    ULONG i, size;

    TRACE("device %p, irp %p, minor function %#x.\n", device, irp, irpsp->MinorFunction);

    switch (irpsp->MinorFunction)
    {
        case IRP_MN_QUERY_ID:
            status = handle_IRP_MN_QUERY_ID(device, irp);
            break;

        case IRP_MN_QUERY_CAPABILITIES:
            status = STATUS_SUCCESS;
            break;

        case IRP_MN_START_DEVICE:
            RtlEnterCriticalSection(&ext->cs);
            if (ext->state != DEVICE_STATE_STOPPED) status = STATUS_SUCCESS;
            else if (ext->state == DEVICE_STATE_REMOVED) status = STATUS_DELETE_PENDING;
            else if ((status = unix_device_start(device)))
                ERR("Failed to start device %p, status %#lx\n", device, status);
            else
            {
                status = unix_device_get_report_descriptor(device, NULL, 0, &ext->report_desc_length);
                if (status != STATUS_SUCCESS && status != STATUS_BUFFER_TOO_SMALL)
                    ERR("Failed to get device %p report descriptor, status %#lx\n", device, status);
                else if (!(ext->report_desc = RtlAllocateHeap(GetProcessHeap(), 0, ext->report_desc_length)))
                    status = STATUS_NO_MEMORY;
                else if ((status = unix_device_get_report_descriptor(device, ext->report_desc, ext->report_desc_length,
                                                                     &ext->report_desc_length)))
                    ERR("Failed to get device %p report descriptor, status %#lx\n", device, status);
                else if ((status = HidP_GetCollectionDescription(ext->report_desc, ext->report_desc_length,
                                                                 PagedPool, &ext->collection_desc)) != HIDP_STATUS_SUCCESS)
                    ERR("Failed to parse device %p report descriptor, status %#lx\n", device, status);
                else
                {
                    status = STATUS_SUCCESS;
                    reports = ext->collection_desc.ReportIDs;
                    for (i = 0; i < ext->collection_desc.ReportIDsLength; ++i)
                    {
                        if (!(size = reports[i].InputLength)) continue;
                        size = offsetof( struct hid_report, buffer[size] );
                        if (!(report = RtlAllocateHeap(GetProcessHeap(), HEAP_ZERO_MEMORY, size))) status = STATUS_NO_MEMORY;
                        else
                        {
                            report->length = reports[i].InputLength;
                            report->buffer[0] = reports[i].ReportID;
                            ext->last_reports[reports[i].ReportID] = report;
                        }
                    }
                    if (!status) ext->state = DEVICE_STATE_STARTED;
                }
            }
            RtlLeaveCriticalSection(&ext->cs);
            break;

        case IRP_MN_SURPRISE_REMOVAL:
            RtlEnterCriticalSection(&ext->cs);
            remove_pending_irps(device);
            ext->state = DEVICE_STATE_REMOVED;
            RtlLeaveCriticalSection(&ext->cs);
            status = STATUS_SUCCESS;
            break;

        case IRP_MN_REMOVE_DEVICE:
            remove_pending_irps(device);

            bus_unlink_hid_device(device);
            unix_device_remove(device);

            ext->cs.DebugInfo->Spare[0] = 0;
            DeleteCriticalSection(&ext->cs);

            irp->IoStatus.Status = STATUS_SUCCESS;
            IoCompleteRequest(irp, IO_NO_INCREMENT);

            LIST_FOR_EACH_ENTRY_SAFE(report, next, &ext->reports, struct hid_report, entry)
                RtlFreeHeap(GetProcessHeap(), 0, report);

            reports = ext->collection_desc.ReportIDs;
            for (i = 0; i < ext->collection_desc.ReportIDsLength; ++i)
            {
                if (!reports[i].InputLength) continue;
                RtlFreeHeap(GetProcessHeap(), 0, ext->last_reports[reports[i].ReportID]);
            }
            HidP_FreeCollectionDescription(&ext->collection_desc);
            RtlFreeHeap(GetProcessHeap(), 0, ext->report_desc);

            IoDeleteDevice(device);
            return STATUS_SUCCESS;

        default:
            FIXME("Unhandled function %08x\n", irpsp->MinorFunction);
            /* fall through */

        case IRP_MN_QUERY_DEVICE_RELATIONS:
            break;
    }

    irp->IoStatus.Status = status;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return status;
}

static NTSTATUS WINAPI common_pnp_dispatch(DEVICE_OBJECT *device, IRP *irp)
{
    if (device == bus_fdo)
        return fdo_pnp_dispatch(device, irp);
    return pdo_pnp_dispatch(device, irp);
}

static NTSTATUS hid_get_device_string(DEVICE_OBJECT *device, DWORD index, WCHAR *buffer, DWORD buffer_len)
{
    struct device_extension *ext = (struct device_extension *)device->DeviceExtension;
    DWORD len;

    switch (index)
    {
    case HID_STRING_ID_IMANUFACTURER:
        len = (wcslen(ext->desc.manufacturer) + 1) * sizeof(WCHAR);
        if (len > buffer_len) return STATUS_BUFFER_TOO_SMALL;
        else memcpy(buffer, ext->desc.manufacturer, len);
        return STATUS_SUCCESS;
    case HID_STRING_ID_IPRODUCT:
        len = (wcslen(ext->desc.product) + 1) * sizeof(WCHAR);
        if (len > buffer_len) return STATUS_BUFFER_TOO_SMALL;
        else memcpy(buffer, ext->desc.product, len);
        return STATUS_SUCCESS;
    case HID_STRING_ID_ISERIALNUMBER:
        len = (wcslen(ext->desc.serialnumber) + 1) * sizeof(WCHAR);
        if (len > buffer_len) return STATUS_BUFFER_TOO_SMALL;
        else memcpy(buffer, ext->desc.serialnumber, len);
        return STATUS_SUCCESS;
    }

    return STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS WINAPI hid_internal_dispatch(DEVICE_OBJECT *device, IRP *irp)
{
    IO_STACK_LOCATION *irpsp = IoGetCurrentIrpStackLocation(irp);
    struct device_extension *ext = (struct device_extension *)device->DeviceExtension;
    ULONG i, code, buffer_len = irpsp->Parameters.DeviceIoControl.OutputBufferLength;
    NTSTATUS status;

    if (device == bus_fdo)
    {
        IoSkipCurrentIrpStackLocation(irp);
        return IoCallDriver(bus_pdo, irp);
    }

    RtlEnterCriticalSection(&ext->cs);

    if (ext->state == DEVICE_STATE_REMOVED)
    {
        RtlLeaveCriticalSection(&ext->cs);
        irp->IoStatus.Status = STATUS_DELETE_PENDING;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
        return STATUS_DELETE_PENDING;
    }

    switch ((code = irpsp->Parameters.DeviceIoControl.IoControlCode))
    {
        case IOCTL_HID_GET_DEVICE_ATTRIBUTES:
        {
            HID_DEVICE_ATTRIBUTES *attr = (HID_DEVICE_ATTRIBUTES *)irp->UserBuffer;
            TRACE("IOCTL_HID_GET_DEVICE_ATTRIBUTES\n");

            if (buffer_len < sizeof(*attr))
            {
                irp->IoStatus.Status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            memset(attr, 0, sizeof(*attr));
            attr->Size = sizeof(*attr);
            attr->VendorID = ext->desc.vid;
            attr->ProductID = ext->desc.pid;
            attr->VersionNumber = ext->desc.version;

            irp->IoStatus.Status = STATUS_SUCCESS;
            irp->IoStatus.Information = sizeof(*attr);
            break;
        }
        case IOCTL_HID_GET_DEVICE_DESCRIPTOR:
        {
            HID_DESCRIPTOR *descriptor = (HID_DESCRIPTOR *)irp->UserBuffer;
            irp->IoStatus.Information = sizeof(*descriptor);
            if (buffer_len < sizeof(*descriptor)) irp->IoStatus.Status = STATUS_BUFFER_TOO_SMALL;
            else
            {
                memset(descriptor, 0, sizeof(*descriptor));
                descriptor->bLength = sizeof(*descriptor);
                descriptor->bDescriptorType = HID_HID_DESCRIPTOR_TYPE;
                descriptor->bcdHID = HID_REVISION;
                descriptor->bCountry = 0;
                descriptor->bNumDescriptors = 1;
                descriptor->DescriptorList[0].bReportType = HID_REPORT_DESCRIPTOR_TYPE;
                descriptor->DescriptorList[0].wReportLength = ext->report_desc_length;
                irp->IoStatus.Status = STATUS_SUCCESS;
            }
            break;
        }
        case IOCTL_HID_GET_REPORT_DESCRIPTOR:
            irp->IoStatus.Information = ext->report_desc_length;
            if (buffer_len < irp->IoStatus.Information)
                irp->IoStatus.Status = STATUS_BUFFER_TOO_SMALL;
            else
            {
                memcpy(irp->UserBuffer, ext->report_desc, ext->report_desc_length);
                irp->IoStatus.Status = STATUS_SUCCESS;
            }
            break;
        case IOCTL_HID_GET_STRING:
        {
            UINT index = (UINT_PTR)irpsp->Parameters.DeviceIoControl.Type3InputBuffer;
            TRACE("IOCTL_HID_GET_STRING[%08x]\n", index);

            irp->IoStatus.Status = hid_get_device_string(device, index, (WCHAR *)irp->UserBuffer, buffer_len);
            if (irp->IoStatus.Status == STATUS_SUCCESS)
                irp->IoStatus.Information = (wcslen((WCHAR *)irp->UserBuffer) + 1) * sizeof(WCHAR);
            break;
        }
        case IOCTL_HID_GET_INPUT_REPORT:
        {
            HID_XFER_PACKET *packet = (HID_XFER_PACKET *)irp->UserBuffer;
            struct hid_report *last_report = ext->last_reports[packet->reportId];
            memcpy(packet->reportBuffer, last_report->buffer, last_report->length);
            packet->reportBufferLen = last_report->length;
            irp->IoStatus.Information = packet->reportBufferLen;
            irp->IoStatus.Status = STATUS_SUCCESS;
            if (TRACE_ON(hid))
            {
                TRACE("read input report id %u length %lu:\n", packet->reportId, packet->reportBufferLen);
                for (i = 0; i < packet->reportBufferLen;)
                {
                    char buffer[256], *buf = buffer;
                    buf += sprintf(buf, "%08lx ", i);
                    do { buf += sprintf(buf, " %02x", packet->reportBuffer[i]); }
                    while (++i % 16 && i < packet->reportBufferLen);
                    TRACE("%s\n", buffer);
                }
            }
            break;
        }
        case IOCTL_HID_READ_REPORT:
        {
            if (!deliver_next_report(ext, irp))
            {
                /* hidclass.sys should guarantee this */
                assert(!ext->pending_read);
                ext->pending_read = irp;
                IoMarkIrpPending(irp);
                irp->IoStatus.Status = STATUS_PENDING;
            }
            break;
        }
        case IOCTL_HID_SET_OUTPUT_REPORT:
        case IOCTL_HID_WRITE_REPORT:
        {
            HID_XFER_PACKET *packet = (HID_XFER_PACKET *)irp->UserBuffer;
            if (TRACE_ON(hid))
            {
                TRACE("write output report id %u length %lu:\n", packet->reportId, packet->reportBufferLen);
                for (i = 0; i < packet->reportBufferLen;)
                {
                    char buffer[256], *buf = buffer;
                    buf += sprintf(buf, "%08lx ", i);
                    do { buf += sprintf(buf, " %02x", packet->reportBuffer[i]); }
                    while (++i % 16 && i < packet->reportBufferLen);
                    TRACE("%s\n", buffer);
                }
            }
            unix_device_set_output_report(device, packet, &irp->IoStatus);
            break;
        }
        case IOCTL_HID_GET_FEATURE:
        {
            HID_XFER_PACKET *packet = (HID_XFER_PACKET *)irp->UserBuffer;
            unix_device_get_feature_report(device, packet, &irp->IoStatus);
            if (!irp->IoStatus.Status && TRACE_ON(hid))
            {
                TRACE("read feature report id %u length %lu:\n", packet->reportId, packet->reportBufferLen);
                for (i = 0; i < packet->reportBufferLen;)
                {
                    char buffer[256], *buf = buffer;
                    buf += sprintf(buf, "%08lx ", i);
                    do { buf += sprintf(buf, " %02x", packet->reportBuffer[i]); }
                    while (++i % 16 && i < packet->reportBufferLen);
                    TRACE("%s\n", buffer);
                }
            }
            break;
        }
        case IOCTL_HID_SET_FEATURE:
        {
            HID_XFER_PACKET *packet = (HID_XFER_PACKET *)irp->UserBuffer;
            if (TRACE_ON(hid))
            {
                TRACE("write feature report id %u length %lu:\n", packet->reportId, packet->reportBufferLen);
                for (i = 0; i < packet->reportBufferLen;)
                {
                    char buffer[256], *buf = buffer;
                    buf += sprintf(buf, "%08lx ", i);
                    do { buf += sprintf(buf, " %02x", packet->reportBuffer[i]); }
                    while (++i % 16 && i < packet->reportBufferLen);
                    TRACE("%s\n", buffer);
                }
            }
            unix_device_set_feature_report(device, packet, &irp->IoStatus);
            break;
        }
        default:
            FIXME("Unsupported ioctl %lx (device=%lx access=%lx func=%lx method=%lx)\n",
                  code, code >> 16, (code >> 14) & 3, (code >> 2) & 0xfff, code & 3);
            break;
    }

    status = irp->IoStatus.Status;
    RtlLeaveCriticalSection(&ext->cs);

    if (status != STATUS_PENDING) IoCompleteRequest(irp, IO_NO_INCREMENT);
    return status;
}

static NTSTATUS WINAPI driver_add_device(DRIVER_OBJECT *driver, DEVICE_OBJECT *pdo)
{
    NTSTATUS ret;

    TRACE("driver %p, pdo %p.\n", driver, pdo);

    if ((ret = IoCreateDevice(driver, 0, NULL, FILE_DEVICE_BUS_EXTENDER, 0, FALSE, &bus_fdo)))
    {
        ERR("Failed to create FDO, status %#lx.\n", ret);
        return ret;
    }

    IoAttachDeviceToDeviceStack(bus_fdo, pdo);
    bus_pdo = pdo;

    bus_fdo->Flags &= ~DO_DEVICE_INITIALIZING;

    return STATUS_SUCCESS;
}

static void WINAPI driver_unload(DRIVER_OBJECT *driver)
{
    NtClose(driver_key);
}

NTSTATUS WINAPI DriverEntry( DRIVER_OBJECT *driver, UNICODE_STRING *path )
{
    OBJECT_ATTRIBUTES attr = {0};
    NTSTATUS ret;

    TRACE( "(%p, %s)\n", driver, debugstr_w(path->Buffer) );

    if ((ret = __wine_init_unix_call())) return ret;

    attr.Length = sizeof(attr);
    attr.ObjectName = path;
    attr.Attributes = OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE;
    if ((ret = NtOpenKey(&driver_key, KEY_ALL_ACCESS, &attr)) != STATUS_SUCCESS)
        ERR("Failed to open driver key, status %#lx.\n", ret);

    driver_obj = driver;

    driver->MajorFunction[IRP_MJ_PNP] = common_pnp_dispatch;
    driver->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = hid_internal_dispatch;
    driver->DriverExtension->AddDevice = driver_add_device;
    driver->DriverUnload = driver_unload;

    return STATUS_SUCCESS;
}
