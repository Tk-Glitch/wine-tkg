/*
 * USB root device enumerator using libusb
 *
 * Copyright 2020 Zebediah Figura
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

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <libusb.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winioctl.h"
#include "winternl.h"
#include "ddk/wdm.h"
#include "ddk/usb.h"
#include "ddk/usbioctl.h"
#include "wine/asm.h"
#include "wine/debug.h"
#include "wine/list.h"
#include "wine/unicode.h"

WINE_DEFAULT_DEBUG_CHANNEL(wineusb);

#if defined(__i386__) && !defined(_WIN32)

extern void * WINAPI wrap_fastcall_func1( void *func, const void *a );
__ASM_STDCALL_FUNC( wrap_fastcall_func1, 8,
                   "popl %ecx\n\t"
                   "popl %eax\n\t"
                   "xchgl (%esp),%ecx\n\t"
                   "jmp *%eax" );

#define call_fastcall_func1(func,a) wrap_fastcall_func1(func,a)

#else

#define call_fastcall_func1(func,a) func(a)

#endif

#define DECLARE_CRITICAL_SECTION(cs) \
    static CRITICAL_SECTION cs; \
    static CRITICAL_SECTION_DEBUG cs##_debug = \
    { 0, 0, &cs, { &cs##_debug.ProcessLocksList, &cs##_debug.ProcessLocksList }, \
      0, 0, { (DWORD_PTR)(__FILE__ ": " # cs) }}; \
    static CRITICAL_SECTION cs = { &cs##_debug, -1, 0, 0, 0, 0 };

DECLARE_CRITICAL_SECTION(wineusb_cs);

static struct list device_list = LIST_INIT(device_list);

struct usb_device
{
    struct list entry;
    BOOL removed;

    DEVICE_OBJECT *device_obj;

    libusb_device *libusb_device;
    libusb_device_handle *handle;

    LIST_ENTRY irp_list;
};

static DRIVER_OBJECT *driver_obj;
static DEVICE_OBJECT *bus_fdo, *bus_pdo;

static libusb_hotplug_callback_handle hotplug_cb_handle;

static void add_usb_device(libusb_device *libusb_device)
{
    static const WCHAR formatW[] = {'\\','D','e','v','i','c','e','\\','U','S','B','P','D','O','-','%','u',0};
    struct libusb_device_descriptor device_desc;
    static unsigned int name_index;
    libusb_device_handle *handle;
    struct usb_device *device;
    DEVICE_OBJECT *device_obj;
    UNICODE_STRING string;
    NTSTATUS status;
    WCHAR name[20];
    int ret;

    libusb_get_device_descriptor(libusb_device, &device_desc);

    TRACE("Adding new device %p, vendor %04x, product %04x.\n", libusb_device,
            device_desc.idVendor, device_desc.idProduct);

    if ((ret = libusb_open(libusb_device, &handle)))
    {
        WARN("Failed to open device: %s\n", libusb_strerror(ret));
        return;
    }

    sprintfW(name, formatW, name_index++);
    RtlInitUnicodeString(&string, name);
    if ((status = IoCreateDevice(driver_obj, sizeof(*device), &string,
            FILE_DEVICE_USB, 0, FALSE, &device_obj)))
    {
        ERR("Failed to create device, status %#x.\n", status);
        LeaveCriticalSection(&wineusb_cs);
        libusb_close(handle);
        return;
    }

    device = device_obj->DeviceExtension;
    device->device_obj = device_obj;
    device->libusb_device = libusb_ref_device(libusb_device);
    device->handle = handle;
    InitializeListHead(&device->irp_list);

    EnterCriticalSection(&wineusb_cs);
    list_add_tail(&device_list, &device->entry);
    device->removed = FALSE;
    LeaveCriticalSection(&wineusb_cs);

    IoInvalidateDeviceRelations(bus_pdo, BusRelations);
}

static void remove_usb_device(libusb_device *libusb_device)
{
    struct usb_device *device;

    TRACE("Removing device %p.\n", libusb_device);

    EnterCriticalSection(&wineusb_cs);
    LIST_FOR_EACH_ENTRY(device, &device_list, struct usb_device, entry)
    {
        if (device->libusb_device == libusb_device)
        {
            list_remove(&device->entry);
            device->removed = TRUE;
            break;
        }
    }
    LeaveCriticalSection(&wineusb_cs);

    IoInvalidateDeviceRelations(bus_pdo, BusRelations);
}

static BOOL thread_shutdown;
static HANDLE event_thread;

static int LIBUSB_CALL hotplug_cb(libusb_context *context, libusb_device *device,
        libusb_hotplug_event event, void *user_data)
{
    if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED)
        add_usb_device(device);
    else
        remove_usb_device(device);

    return 0;
}

static DWORD CALLBACK event_thread_proc(void *arg)
{
    int ret;

    TRACE("Starting event thread.\n");

    while (!thread_shutdown)
    {
        if ((ret = libusb_handle_events(NULL)))
            ERR("Error handling events: %s\n", libusb_strerror(ret));
    }

    TRACE("Shutting down event thread.\n");
    return 0;
}

static NTSTATUS fdo_pnp(IRP *irp)
{
    IO_STACK_LOCATION *stack = IoGetCurrentIrpStackLocation(irp);
    NTSTATUS ret;

    TRACE("irp %p, minor function %#x.\n", irp, stack->MinorFunction);

    switch (stack->MinorFunction)
    {
        case IRP_MN_QUERY_DEVICE_RELATIONS:
        {
            struct usb_device *device;
            DEVICE_RELATIONS *devices;
            unsigned int i = 0;

            if (stack->Parameters.QueryDeviceRelations.Type != BusRelations)
            {
                FIXME("Unhandled device relations type %#x.\n", stack->Parameters.QueryDeviceRelations.Type);
                break;
            }

            EnterCriticalSection(&wineusb_cs);

            if (!(devices = ExAllocatePool(PagedPool,
                    offsetof(DEVICE_RELATIONS, Objects[list_count(&device_list)]))))
            {
                LeaveCriticalSection(&wineusb_cs);
                irp->IoStatus.Status = STATUS_NO_MEMORY;
                break;
            }

            LIST_FOR_EACH_ENTRY(device, &device_list, struct usb_device, entry)
            {
                devices->Objects[i++] = device->device_obj;
                call_fastcall_func1(ObfReferenceObject, device->device_obj);
            }

            LeaveCriticalSection(&wineusb_cs);

            devices->Count = i;
            irp->IoStatus.Information = (ULONG_PTR)devices;
            irp->IoStatus.Status = STATUS_SUCCESS;
            break;
        }

        case IRP_MN_START_DEVICE:
            if ((ret = libusb_hotplug_register_callback(NULL,
                    LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT,
                    LIBUSB_HOTPLUG_ENUMERATE, LIBUSB_HOTPLUG_MATCH_ANY, LIBUSB_HOTPLUG_MATCH_ANY,
                    LIBUSB_HOTPLUG_MATCH_ANY, hotplug_cb, NULL, &hotplug_cb_handle)))
            {
                ERR("Failed to register callback: %s\n", libusb_strerror(ret));
                irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
                break;
            }
            irp->IoStatus.Status = STATUS_SUCCESS;
            break;

        case IRP_MN_SURPRISE_REMOVAL:
            irp->IoStatus.Status = STATUS_SUCCESS;
            break;

        case IRP_MN_REMOVE_DEVICE:
        {
            struct usb_device *device, *cursor;

            libusb_hotplug_deregister_callback(NULL, hotplug_cb_handle);
            thread_shutdown = TRUE;
            libusb_interrupt_event_handler(NULL);
            WaitForSingleObject(event_thread, INFINITE);
            CloseHandle(event_thread);

            EnterCriticalSection(&wineusb_cs);
            LIST_FOR_EACH_ENTRY_SAFE(device, cursor, &device_list, struct usb_device, entry)
            {
                libusb_unref_device(device->libusb_device);
                libusb_close(device->handle);
                list_remove(&device->entry);
                IoDeleteDevice(device->device_obj);
            }
            LeaveCriticalSection(&wineusb_cs);

            irp->IoStatus.Status = STATUS_SUCCESS;
            IoSkipCurrentIrpStackLocation(irp);
            ret = IoCallDriver(bus_pdo, irp);
            IoDetachDevice(bus_pdo);
            IoDeleteDevice(bus_fdo);
            return ret;
        }

        default:
            FIXME("Unhandled minor function %#x.\n", stack->MinorFunction);
    }

    IoSkipCurrentIrpStackLocation(irp);
    return IoCallDriver(bus_pdo, irp);
}

static void get_device_id(const struct usb_device *device, WCHAR *buffer)
{
    static const WCHAR formatW[] = {'U','S','B','\\','V','I','D','_','%','0','4','X',
            '&','P','I','D','_','%','0','4','X',0};
    struct libusb_device_descriptor desc;

    libusb_get_device_descriptor(device->libusb_device, &desc);
    sprintfW(buffer, formatW, desc.idVendor, desc.idProduct);
}

static void get_hardware_ids(const struct usb_device *device, WCHAR *buffer)
{
    static const WCHAR formatW[] = {'U','S','B','\\','V','I','D','_','%','0','4','X',
                '&','P','I','D','_','%','0','4','X','&','R','E','V','_','%','0','4','X',0};
    struct libusb_device_descriptor desc;

    libusb_get_device_descriptor(device->libusb_device, &desc);
    buffer += sprintfW(buffer, formatW, desc.idVendor, desc.idProduct, desc.bcdDevice) + 1;
    get_device_id(device, buffer);
    buffer += strlenW(buffer) + 1;
    *buffer = 0;
}

static void get_compatible_ids(const struct usb_device *device, WCHAR *buffer)
{
    static const WCHAR prot_format[] = {'U','S','B','\\','C','l','a','s','s','_','%','0','2','x',
            '&','S','u','b','C','l','a','s','s','_','%','0','2','x',
            '&','P','r','o','t','_','%','0','2','x',0};
    static const WCHAR subclass_format[] = {'U','S','B','\\','C','l','a','s','s','_','%','0','2','x',
            '&','S','u','b','C','l','a','s','s','_','%','0','2','x',0};
    static const WCHAR class_format[] = {'U','S','B','\\','C','l','a','s','s','_','%','0','2','x',0};

    struct libusb_device_descriptor device_desc;

    libusb_get_device_descriptor(device->libusb_device, &device_desc);

    buffer += sprintfW(buffer, prot_format, device_desc.bDeviceClass, device_desc.bDeviceSubClass,
            device_desc.bDeviceProtocol) + 1;
    buffer += sprintfW(buffer, subclass_format, device_desc.bDeviceClass, device_desc.bDeviceSubClass) + 1;
    buffer += sprintfW(buffer, class_format, device_desc.bDeviceClass) + 1;
    *buffer = 0;
}

static NTSTATUS query_id(const struct usb_device *device, IRP *irp, BUS_QUERY_ID_TYPE type)
{
    WCHAR *id = NULL;

    switch (type)
    {
        case BusQueryDeviceID:
            if ((id = ExAllocatePool(PagedPool, 28 * sizeof(WCHAR))))
                get_device_id(device, id);
            break;

        case BusQueryInstanceID:
            if ((id = ExAllocatePool(PagedPool, 2 * sizeof(WCHAR))))
            {
                id[0] = '0';
                id[1] = 0;
            }
            break;

        case BusQueryHardwareIDs:
            if ((id = ExAllocatePool(PagedPool, (28 + 37 + 1) * sizeof(WCHAR))))
                get_hardware_ids(device, id);
            break;

        case BusQueryCompatibleIDs:
            if ((id = ExAllocatePool(PagedPool, (33 + 25 + 13 + 1) * sizeof(WCHAR))))
                get_compatible_ids(device, id);
            break;

        default:
            FIXME("Unhandled ID query type %#x.\n", type);
            return irp->IoStatus.Status;
    }

    irp->IoStatus.Information = (ULONG_PTR)id;
    return STATUS_SUCCESS;
}

static NTSTATUS pdo_pnp(DEVICE_OBJECT *device_obj, IRP *irp)
{
    IO_STACK_LOCATION *stack = IoGetCurrentIrpStackLocation(irp);
    struct usb_device *device = device_obj->DeviceExtension;
    NTSTATUS ret = irp->IoStatus.Status;

    TRACE("device_obj %p, irp %p, minor function %#x.\n", device_obj, irp, stack->MinorFunction);

    switch (stack->MinorFunction)
    {
        case IRP_MN_QUERY_ID:
            ret = query_id(device, irp, stack->Parameters.QueryId.IdType);
            break;

        case IRP_MN_START_DEVICE:
        case IRP_MN_QUERY_CAPABILITIES:
        case IRP_MN_SURPRISE_REMOVAL:
            ret = STATUS_SUCCESS;
            break;

        case IRP_MN_REMOVE_DEVICE:
        {
            LIST_ENTRY *entry;

            EnterCriticalSection(&wineusb_cs);
            while ((entry = RemoveHeadList(&device->irp_list)) != &device->irp_list)
            {
                irp = CONTAINING_RECORD(entry, IRP, Tail.Overlay.ListEntry);
                irp->IoStatus.Status = STATUS_CANCELLED;
                irp->IoStatus.Information = 0;
                IoCompleteRequest(irp, IO_NO_INCREMENT);
            }
            LeaveCriticalSection(&wineusb_cs);

            if (device->removed)
            {
                libusb_unref_device(device->libusb_device);
                libusb_close(device->handle);

                irp->IoStatus.Status = STATUS_SUCCESS;
                IoCompleteRequest(irp, IO_NO_INCREMENT);
                IoDeleteDevice(device->device_obj);
                return STATUS_SUCCESS;
            }

            ret = STATUS_SUCCESS;
            break;
        }

        default:
            FIXME("Unhandled minor function %#x.\n", stack->MinorFunction);
    }

    irp->IoStatus.Status = ret;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return ret;
}

static NTSTATUS WINAPI driver_pnp(DEVICE_OBJECT *device, IRP *irp)
{
    if (device == bus_fdo)
        return fdo_pnp(irp);
    return pdo_pnp(device, irp);
}

static NTSTATUS usbd_status_from_libusb(enum libusb_transfer_status status)
{
    switch (status)
    {
        case LIBUSB_TRANSFER_CANCELLED:
            return USBD_STATUS_CANCELED;
        case LIBUSB_TRANSFER_COMPLETED:
            return USBD_STATUS_SUCCESS;
        case LIBUSB_TRANSFER_NO_DEVICE:
            return USBD_STATUS_DEVICE_GONE;
        case LIBUSB_TRANSFER_STALL:
            return USBD_STATUS_ENDPOINT_HALTED;
        case LIBUSB_TRANSFER_TIMED_OUT:
            return USBD_STATUS_TIMEOUT;
        default:
            FIXME("Unhandled status %#x.\n", status);
        case LIBUSB_TRANSFER_ERROR:
            return USBD_STATUS_REQUEST_FAILED;
    }
}

static void transfer_cb(struct libusb_transfer *transfer)
{
    IRP *irp = transfer->user_data;
    URB *urb = IoGetCurrentIrpStackLocation(irp)->Parameters.Others.Argument1;

    TRACE("Completing IRP %p, status %#x.\n", irp, transfer->status);

    urb->UrbHeader.Status = usbd_status_from_libusb(transfer->status);

    if (transfer->status == LIBUSB_TRANSFER_COMPLETED)
    {
        switch (urb->UrbHeader.Function)
        {
            case URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER:
                urb->UrbBulkOrInterruptTransfer.TransferBufferLength = transfer->actual_length;
                break;

            case URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE:
            {
                struct _URB_CONTROL_DESCRIPTOR_REQUEST *req = &urb->UrbControlDescriptorRequest;
                req->TransferBufferLength = transfer->actual_length;
                memcpy(req->TransferBuffer, libusb_control_transfer_get_data(transfer), transfer->actual_length);
                break;
            }

            case URB_FUNCTION_VENDOR_INTERFACE:
            {
                struct _URB_CONTROL_VENDOR_OR_CLASS_REQUEST *req = &urb->UrbControlVendorClassRequest;
                req->TransferBufferLength = transfer->actual_length;
                if (req->TransferFlags & USBD_TRANSFER_DIRECTION_IN)
                    memcpy(req->TransferBuffer, libusb_control_transfer_get_data(transfer), transfer->actual_length);
                break;
            }

            default:
                ERR("Unexpected function %#x.\n", urb->UrbHeader.Function);
        }
    }

    EnterCriticalSection(&wineusb_cs);
    RemoveEntryList(&irp->Tail.Overlay.ListEntry);
    LeaveCriticalSection(&wineusb_cs);

    irp->IoStatus.Status = STATUS_SUCCESS;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
}

static void queue_irp(struct usb_device *device, IRP *irp, struct libusb_transfer *transfer)
{
    IoMarkIrpPending(irp);
    irp->Tail.Overlay.DriverContext[0] = transfer;
    EnterCriticalSection(&wineusb_cs);
    InsertTailList(&device->irp_list, &irp->Tail.Overlay.ListEntry);
    LeaveCriticalSection(&wineusb_cs);
}

struct pipe
{
    unsigned char endpoint;
    unsigned char type;
};

static HANDLE make_pipe_handle(unsigned char endpoint, USBD_PIPE_TYPE type)
{
    union
    {
        struct pipe pipe;
        HANDLE handle;
    } u;

    u.pipe.endpoint = endpoint;
    u.pipe.type = type;
    return u.handle;
}

static struct pipe get_pipe(HANDLE handle)
{
    union
    {
        struct pipe pipe;
        HANDLE handle;
    } u;

    u.handle = handle;
    return u.pipe;
}

static NTSTATUS usb_submit_urb(struct usb_device *device, IRP *irp)
{
    URB *urb = IoGetCurrentIrpStackLocation(irp)->Parameters.Others.Argument1;
    struct libusb_transfer *transfer;
    int ret;

    TRACE("type %#x.\n", urb->UrbHeader.Function);

    switch (urb->UrbHeader.Function)
    {
        case URB_FUNCTION_ABORT_PIPE:
        {
            LIST_ENTRY *entry, *mark;

            /* The documentation states that URB_FUNCTION_ABORT_PIPE may
             * complete before outstanding requests complete, so we don't need
             * to wait for them. */
            EnterCriticalSection(&wineusb_cs);
            mark = &device->irp_list;
            for (entry = mark->Flink; entry != mark; entry = entry->Flink)
            {
                IRP *queued_irp = CONTAINING_RECORD(entry, IRP, Tail.Overlay.ListEntry);

                if ((ret = libusb_cancel_transfer(queued_irp->Tail.Overlay.DriverContext[0])) < 0)
                    ERR("Failed to cancel transfer: %s\n", libusb_strerror(ret));
            }
            LeaveCriticalSection(&wineusb_cs);

            return STATUS_SUCCESS;
        }

        case URB_FUNCTION_SYNC_RESET_PIPE_AND_CLEAR_STALL:
        {
            struct _URB_PIPE_REQUEST *req = &urb->UrbPipeRequest;
            struct pipe pipe = get_pipe(req->PipeHandle);

            if ((ret = libusb_clear_halt(device->handle, pipe.endpoint)) < 0)
                ERR("Failed to clear halt: %s\n", libusb_strerror(ret));

            return STATUS_SUCCESS;
        }

        case URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER:
        {
            struct _URB_BULK_OR_INTERRUPT_TRANSFER *req = &urb->UrbBulkOrInterruptTransfer;
            struct pipe pipe = get_pipe(req->PipeHandle);

            if (req->TransferBufferMDL)
                FIXME("Unhandled MDL output buffer.\n");

            if (!(transfer = libusb_alloc_transfer(0)))
                return STATUS_NO_MEMORY;

            if (pipe.type == UsbdPipeTypeBulk)
            {
                libusb_fill_bulk_transfer(transfer, device->handle, pipe.endpoint,
                        req->TransferBuffer, req->TransferBufferLength, transfer_cb, irp, 0);
            }
            else if (pipe.type == UsbdPipeTypeInterrupt)
            {
                libusb_fill_interrupt_transfer(transfer, device->handle, pipe.endpoint,
                        req->TransferBuffer, req->TransferBufferLength, transfer_cb, irp, 0);
            }
            else
            {
                WARN("Invalid pipe type %#x.\n", pipe.type);
                libusb_free_transfer(transfer);
                return USBD_STATUS_INVALID_PIPE_HANDLE;
            }

            queue_irp(device, irp, transfer);
            transfer->flags = LIBUSB_TRANSFER_FREE_TRANSFER;
            ret = libusb_submit_transfer(transfer);
            if (ret < 0)
                ERR("Failed to submit bulk transfer: %s\n", libusb_strerror(ret));

            return STATUS_PENDING;
        }

        case URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE:
        {
            struct _URB_CONTROL_DESCRIPTOR_REQUEST *req = &urb->UrbControlDescriptorRequest;
            unsigned char *buffer;

            if (req->TransferBufferMDL)
                FIXME("Unhandled MDL output buffer.\n");

            if (!(transfer = libusb_alloc_transfer(0)))
                return STATUS_NO_MEMORY;

            if (!(buffer = malloc(sizeof(struct libusb_control_setup) + req->TransferBufferLength)))
            {
                libusb_free_transfer(transfer);
                return STATUS_NO_MEMORY;
            }

            queue_irp(device, irp, transfer);
            libusb_fill_control_setup(buffer,
                    LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_STANDARD | LIBUSB_RECIPIENT_DEVICE,
                    LIBUSB_REQUEST_GET_DESCRIPTOR, (req->DescriptorType << 8) | req->Index,
                    req->LanguageId, req->TransferBufferLength);
            libusb_fill_control_transfer(transfer, device->handle, buffer, transfer_cb, irp, 0);
            transfer->flags = LIBUSB_TRANSFER_FREE_BUFFER | LIBUSB_TRANSFER_FREE_TRANSFER;
            ret = libusb_submit_transfer(transfer);
            if (ret < 0)
                ERR("Failed to submit GET_DESCRIPTOR transfer: %s\n", libusb_strerror(ret));

            return STATUS_PENDING;
        }

        case URB_FUNCTION_SELECT_CONFIGURATION:
        {
            struct _URB_SELECT_CONFIGURATION *req = &urb->UrbSelectConfiguration;
            ULONG i;

            /* FIXME: In theory, we'd call libusb_set_configuration() here, but
             * the CASIO FX-9750GII (which has only one configuration) goes into
             * an error state if it receives a SET_CONFIGURATION request. Maybe
             * we should skip setting that if and only if the configuration is
             * already active? */

            for (i = 0; i < req->Interface.NumberOfPipes; ++i)
            {
                USBD_PIPE_INFORMATION *pipe = &req->Interface.Pipes[i];
                pipe->PipeHandle = make_pipe_handle(pipe->EndpointAddress, pipe->PipeType);
            }

            return STATUS_SUCCESS;
        }

        case URB_FUNCTION_VENDOR_INTERFACE:
        {
            struct _URB_CONTROL_VENDOR_OR_CLASS_REQUEST *req = &urb->UrbControlVendorClassRequest;
            uint8_t req_type = LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_INTERFACE;
            unsigned char *buffer;

            if (req->TransferFlags & USBD_TRANSFER_DIRECTION_IN)
                req_type |= LIBUSB_ENDPOINT_IN;
            if (req->TransferFlags & ~USBD_TRANSFER_DIRECTION_IN)
                FIXME("Unhandled flags %#x.\n", req->TransferFlags);

            if (req->TransferBufferMDL)
                FIXME("Unhandled MDL output buffer.\n");

            if (!(transfer = libusb_alloc_transfer(0)))
                return STATUS_NO_MEMORY;

            if (!(buffer = malloc(sizeof(struct libusb_control_setup) + req->TransferBufferLength)))
            {
                libusb_free_transfer(transfer);
                return STATUS_NO_MEMORY;
            }

            queue_irp(device, irp, transfer);
            libusb_fill_control_setup(buffer, req_type, req->Request,
                    req->Value, req->Index, req->TransferBufferLength);
            if (!(req->TransferFlags & USBD_TRANSFER_DIRECTION_IN))
                memcpy(buffer + LIBUSB_CONTROL_SETUP_SIZE, req->TransferBuffer, req->TransferBufferLength);
            libusb_fill_control_transfer(transfer, device->handle, buffer, transfer_cb, irp, 0);
            transfer->flags = LIBUSB_TRANSFER_FREE_BUFFER | LIBUSB_TRANSFER_FREE_TRANSFER;
            ret = libusb_submit_transfer(transfer);
            if (ret < 0)
                ERR("Failed to submit vendor-specific interface transfer: %s\n", libusb_strerror(ret));

            return STATUS_PENDING;
        }

        default:
            FIXME("Unhandled function %#x.\n", urb->UrbHeader.Function);
    }

    return STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS WINAPI driver_internal_ioctl(DEVICE_OBJECT *device_obj, IRP *irp)
{
    IO_STACK_LOCATION *stack = IoGetCurrentIrpStackLocation(irp);
    ULONG code = stack->Parameters.DeviceIoControl.IoControlCode;
    struct usb_device *device = device_obj->DeviceExtension;
    NTSTATUS status = STATUS_NOT_IMPLEMENTED;

    TRACE("device_obj %p, irp %p, code %#x.\n", device_obj, irp, code);

    switch (code)
    {
        case IOCTL_INTERNAL_USB_SUBMIT_URB:
            status = usb_submit_urb(device, irp);
            break;

        default:
            FIXME("Unhandled ioctl %#x (device %#x, access %#x, function %#x, method %#x).\n",
                    code, code >> 16, (code >> 14) & 3, (code >> 2) & 0xfff, code & 3);
    }

    if (status != STATUS_PENDING)
    {
        irp->IoStatus.Status = status;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
    }
    return status;
}

static NTSTATUS WINAPI driver_add_device(DRIVER_OBJECT *driver, DEVICE_OBJECT *pdo)
{
    NTSTATUS ret;

    TRACE("driver %p, pdo %p.\n", driver, pdo);

    if ((ret = IoCreateDevice(driver, 0, NULL, FILE_DEVICE_BUS_EXTENDER, 0, FALSE, &bus_fdo)))
    {
        ERR("Failed to create FDO, status %#x.\n", ret);
        return ret;
    }

    IoAttachDeviceToDeviceStack(bus_fdo, pdo);
    bus_pdo = pdo;
    bus_fdo->Flags &= ~DO_DEVICE_INITIALIZING;

    return STATUS_SUCCESS;
}

static void WINAPI driver_unload(DRIVER_OBJECT *driver)
{
    libusb_exit(NULL);
}

NTSTATUS WINAPI DriverEntry(DRIVER_OBJECT *driver, UNICODE_STRING *path)
{
    int err;

    TRACE("driver %p, path %s.\n", driver, debugstr_w(path->Buffer));

    driver_obj = driver;

    if ((err = libusb_init(NULL)))
    {
        ERR("Failed to initialize libusb: %s\n", libusb_strerror(err));
        return STATUS_UNSUCCESSFUL;
    }

    event_thread = CreateThread(NULL, 0, event_thread_proc, NULL, 0, NULL);

    driver->DriverExtension->AddDevice = driver_add_device;
    driver->DriverUnload = driver_unload;
    driver->MajorFunction[IRP_MJ_PNP] = driver_pnp;
    driver->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = driver_internal_ioctl;

    return STATUS_SUCCESS;
}
