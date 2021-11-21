/*
 * HID Plug and Play test driver
 *
 * Copyright 2021 Zebediah Figura
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
#include <stdio.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winternl.h"
#include "winioctl.h"
#include "ddk/wdm.h"
#include "hidusage.h"
#include "ddk/hidpi.h"
#include "ddk/hidport.h"

#include "wine/list.h"

#include "driver_hid.h"

static UNICODE_STRING control_symlink;

static unsigned int got_start_device;
static HID_DEVICE_ATTRIBUTES attributes;
static char report_descriptor_buf[4096];
static DWORD report_descriptor_len;
static HIDP_CAPS caps;
static DWORD report_id;
static DWORD polled;

#define EXPECT_QUEUE_BUFFER_SIZE (64 * sizeof(struct hid_expect))

struct expect_queue
{
    KSPIN_LOCK lock;
    struct hid_expect *pos;
    struct hid_expect *end;
    struct hid_expect spurious;
    struct hid_expect *buffer;
};

static void expect_queue_init( struct expect_queue *queue )
{
    KeInitializeSpinLock( &queue->lock );
    queue->buffer = ExAllocatePool( PagedPool, EXPECT_QUEUE_BUFFER_SIZE );
    RtlSecureZeroMemory( queue->buffer, EXPECT_QUEUE_BUFFER_SIZE );
    queue->pos = queue->buffer;
    queue->end = queue->buffer;
}

static void expect_queue_cleanup( struct expect_queue *queue )
{
    ExFreePool( queue->buffer );
}

static void expect_queue_reset( struct expect_queue *queue, void *buffer, unsigned int size )
{
    struct hid_expect *missing, *missing_end, *tmp;
    KIRQL irql;

    missing = ExAllocatePool( PagedPool, EXPECT_QUEUE_BUFFER_SIZE );
    RtlSecureZeroMemory( missing, EXPECT_QUEUE_BUFFER_SIZE );
    missing_end = missing;

    KeAcquireSpinLock( &queue->lock, &irql );
    tmp = queue->pos;
    while (tmp < queue->end) *missing_end++ = *tmp++;

    queue->pos = queue->buffer;
    queue->end = queue->buffer;

    if (size) memcpy( queue->end, buffer, size );
    queue->end = queue->end + size / sizeof(struct hid_expect);
    KeReleaseSpinLock( &queue->lock, irql );

    tmp = missing;
    while (tmp != missing_end)
    {
        winetest_push_context( "%s expect[%d]", tmp->context, tmp - missing );
        if (tmp->broken)
        {
            todo_wine_if( tmp->todo )
            win_skip( "broken (code %#x id %u len %u)\n", tmp->code, tmp->report_id, tmp->report_len );
        }
        else
        {
            todo_wine_if( tmp->todo )
            ok( 0, "missing (code %#x id %u len %u)\n", tmp->code, tmp->report_id, tmp->report_len );
        }
        winetest_pop_context();
        tmp++;
    }

    ExFreePool( missing );
}

static void expect_queue_next( struct expect_queue *queue, ULONG code, HID_XFER_PACKET *packet,
                               LONG *index, struct hid_expect *expect, BOOL compare_buf )
{
    struct hid_expect *missing, *missing_end, *tmp;
    ULONG len = packet->reportBufferLen;
    BYTE *buf = packet->reportBuffer;
    BYTE id = packet->reportId;
    KIRQL irql;

    missing = ExAllocatePool( PagedPool, EXPECT_QUEUE_BUFFER_SIZE );
    RtlSecureZeroMemory( missing, EXPECT_QUEUE_BUFFER_SIZE );
    missing_end = missing;

    KeAcquireSpinLock( &queue->lock, &irql );
    tmp = queue->pos;
    while (tmp < queue->end)
    {
        if (running_under_wine && !tmp->todo) break;
        if (!running_under_wine && !tmp->broken) break;
        if (tmp->code == code && tmp->report_id == id && tmp->report_len == len &&
            (!compare_buf || RtlCompareMemory( tmp->report_buf, buf, len ) == len))
            break;
        *missing_end++ = *tmp++;
    }
    *index = tmp - queue->buffer;
    if (tmp < queue->end) queue->pos = tmp + 1;
    else tmp = &queue->spurious;
    *expect = *tmp;
    KeReleaseSpinLock( &queue->lock, irql );

    ok( tmp != &queue->spurious, "got spurious packet\n" );

    winetest_push_context( "%s expect[%d]", tmp->context, tmp - queue->buffer );
    todo_wine_if( tmp->todo )
    ok( 1, "found code %#x id %u len %u\n", tmp->code, tmp->report_id, tmp->report_len );
    winetest_pop_context();

    tmp = missing;
    while (tmp != missing_end)
    {
        winetest_push_context( "%s expect[%d]", tmp->context, tmp - missing );
        if (tmp->broken)
        {
            todo_wine_if( tmp->todo )
            win_skip( "broken (code %#x id %u len %u)\n", tmp->code, tmp->report_id, tmp->report_len );
        }
        else
        {
            todo_wine_if( tmp->todo )
            ok( 0, "missing (code %#x id %u len %u)\n", tmp->code, tmp->report_id, tmp->report_len );
        }
        winetest_pop_context();
        tmp++;
    }

    ExFreePool( missing );
}

static struct expect_queue expect_queue;

struct irp_queue
{
    KSPIN_LOCK lock;
    LIST_ENTRY list;
};

static IRP *irp_queue_pop( struct irp_queue *queue )
{
    KIRQL irql;
    IRP *irp;

    KeAcquireSpinLock( &queue->lock, &irql );
    if (IsListEmpty( &queue->list )) irp = NULL;
    else irp = CONTAINING_RECORD( RemoveHeadList( &queue->list ), IRP, Tail.Overlay.ListEntry );
    KeReleaseSpinLock( &queue->lock, irql );

    return irp;
}

static void irp_queue_push( struct irp_queue *queue, IRP *irp )
{
    KIRQL irql;

    KeAcquireSpinLock( &queue->lock, &irql );
    InsertTailList( &queue->list, &irp->Tail.Overlay.ListEntry );
    KeReleaseSpinLock( &queue->lock, irql );
}

static void irp_queue_clear( struct irp_queue *queue )
{
    IRP *irp;

    while ((irp = irp_queue_pop( queue )))
    {
        irp->IoStatus.Status = STATUS_DELETE_PENDING;
        IoCompleteRequest( irp, IO_NO_INCREMENT );
    }
}

static void irp_queue_init( struct irp_queue *queue )
{
    KeInitializeSpinLock( &queue->lock );
    InitializeListHead( &queue->list );
}

struct input_queue
{
    KSPIN_LOCK lock;
    struct hid_expect *pos;
    struct hid_expect *end;
    struct hid_expect *buffer;
    struct irp_queue pending;
};

static void input_queue_init( struct input_queue *queue )
{
    KeInitializeSpinLock( &queue->lock );
    queue->buffer = ExAllocatePool( PagedPool, EXPECT_QUEUE_BUFFER_SIZE );
    RtlSecureZeroMemory( queue->buffer, EXPECT_QUEUE_BUFFER_SIZE );
    queue->pos = queue->buffer;
    queue->end = queue->buffer;
    irp_queue_init( &queue->pending );
}

static void input_queue_cleanup( struct input_queue *queue )
{
    ExFreePool( queue->buffer );
}

static BOOL input_queue_read_locked( struct input_queue *queue, IRP *irp )
{
    IO_STACK_LOCATION *stack = IoGetCurrentIrpStackLocation( irp );
    ULONG out_size = stack->Parameters.DeviceIoControl.OutputBufferLength;
    struct hid_expect *tmp = queue->pos;

    if (tmp >= queue->end) return FALSE;
    if (tmp->ret_length) out_size = tmp->ret_length;

    memcpy( irp->UserBuffer, tmp->report_buf, out_size );
    irp->IoStatus.Information = out_size;
    irp->IoStatus.Status = tmp->ret_status;
    if (tmp < queue->end) queue->pos = tmp + 1;

    /* loop on the queue data in polled mode */
    if (polled && queue->pos == queue->end) queue->pos = queue->buffer;
    return TRUE;
}

static NTSTATUS input_queue_read( struct input_queue *queue, IRP *irp )
{
    NTSTATUS status;
    KIRQL irql;

    KeAcquireSpinLock( &queue->lock, &irql );
    if (input_queue_read_locked( queue, irp )) status = STATUS_SUCCESS;
    else
    {
        IoMarkIrpPending( irp );
        irp_queue_push( &queue->pending, irp );
        status = STATUS_PENDING;
    }
    KeReleaseSpinLock( &queue->lock, irql );

    return status;
}

static void input_queue_reset( struct input_queue *queue, void *in_buf, ULONG in_size )
{
    struct irp_queue completed;
    ULONG remaining;
    KIRQL irql;
    IRP *irp;

    irp_queue_init( &completed );

    KeAcquireSpinLock( &queue->lock, &irql );
    remaining = queue->end - queue->pos;
    queue->pos = queue->buffer;
    queue->end = queue->buffer;
    memcpy( queue->end, in_buf, in_size );
    queue->end += in_size / sizeof(struct hid_expect);

    while (!polled && queue->pos < queue->end && (irp = irp_queue_pop( &queue->pending )))
    {
        input_queue_read_locked( queue, irp );
        irp_queue_push( &completed, irp );
    }
    KeReleaseSpinLock( &queue->lock, irql );

    if (!polled) ok( !remaining, "unread input\n" );

    while ((irp = irp_queue_pop( &completed ))) IoCompleteRequest( irp, IO_NO_INCREMENT );
}

static struct input_queue input_queue;

struct hid_device
{
    BOOL removed;
    KSPIN_LOCK lock;
};

static NTSTATUS WINAPI driver_pnp( DEVICE_OBJECT *device, IRP *irp )
{
    IO_STACK_LOCATION *stack = IoGetCurrentIrpStackLocation( irp );
    HID_DEVICE_EXTENSION *ext = device->DeviceExtension;
    struct hid_device *impl = ext->MiniDeviceExtension;
    KIRQL irql;

    if (winetest_debug > 1) trace( "pnp %#x\n", stack->MinorFunction );

    switch (stack->MinorFunction)
    {
    case IRP_MN_START_DEVICE:
        ++got_start_device;
        impl->removed = FALSE;
        KeInitializeSpinLock( &impl->lock );
        IoSetDeviceInterfaceState( &control_symlink, TRUE );
        irp->IoStatus.Status = STATUS_SUCCESS;
        break;

    case IRP_MN_SURPRISE_REMOVAL:
    case IRP_MN_QUERY_REMOVE_DEVICE:
        KeAcquireSpinLock( &impl->lock, &irql );
        impl->removed = TRUE;
        KeReleaseSpinLock( &impl->lock, irql );
        irp_queue_clear( &input_queue.pending );
        irp->IoStatus.Status = STATUS_SUCCESS;
        break;

    case IRP_MN_CANCEL_REMOVE_DEVICE:
        KeAcquireSpinLock( &impl->lock, &irql );
        impl->removed = FALSE;
        KeReleaseSpinLock( &impl->lock, irql );
        irp->IoStatus.Status = STATUS_SUCCESS;
        break;

    case IRP_MN_STOP_DEVICE:
        irp->IoStatus.Status = STATUS_SUCCESS;
        break;

    case IRP_MN_REMOVE_DEVICE:
        irp_queue_clear( &input_queue.pending );
        IoSetDeviceInterfaceState( &control_symlink, FALSE );
        irp->IoStatus.Status = STATUS_SUCCESS;
        break;
    }

    IoSkipCurrentIrpStackLocation( irp );
    return IoCallDriver( ext->NextDeviceObject, irp );
}

static NTSTATUS WINAPI driver_power( DEVICE_OBJECT *device, IRP *irp )
{
    HID_DEVICE_EXTENSION *ext = device->DeviceExtension;

    /* We do not expect power IRPs as part of normal operation. */
    ok( 0, "unexpected call\n" );

    PoStartNextPowerIrp( irp );
    IoSkipCurrentIrpStackLocation( irp );
    return PoCallDriver( ext->NextDeviceObject, irp );
}

static NTSTATUS WINAPI driver_internal_ioctl( DEVICE_OBJECT *device, IRP *irp )
{
    IO_STACK_LOCATION *stack = IoGetCurrentIrpStackLocation( irp );
    HID_DEVICE_EXTENSION *ext = device->DeviceExtension;
    struct hid_device *impl = ext->MiniDeviceExtension;
    const ULONG in_size = stack->Parameters.DeviceIoControl.InputBufferLength;
    ULONG out_size = stack->Parameters.DeviceIoControl.OutputBufferLength;
    const ULONG code = stack->Parameters.DeviceIoControl.IoControlCode;
    struct hid_expect expect = {0};
    NTSTATUS ret;
    BOOL removed;
    KIRQL irql;
    LONG index;

    if (winetest_debug > 1) trace( "ioctl %#x\n", code );

    ok( got_start_device, "expected IRP_MN_START_DEVICE before any ioctls\n" );

    irp->IoStatus.Information = 0;

    KeAcquireSpinLock( &impl->lock, &irql );
    removed = impl->removed;
    KeReleaseSpinLock( &impl->lock, irql );

    if (removed)
    {
        irp->IoStatus.Status = STATUS_DELETE_PENDING;
        IoCompleteRequest( irp, IO_NO_INCREMENT );
        return STATUS_DELETE_PENDING;
    }

    winetest_push_context( "id %d%s", report_id, polled ? " poll" : "" );

    switch (code)
    {
    case IOCTL_HID_GET_DEVICE_DESCRIPTOR:
    {
        HID_DESCRIPTOR *desc = irp->UserBuffer;

        ok( !in_size, "got input size %u\n", in_size );
        ok( out_size == sizeof(*desc), "got output size %u\n", out_size );

        if (out_size == sizeof(*desc))
        {
            ok( !desc->bLength, "got size %u\n", desc->bLength );

            desc->bLength = sizeof(*desc);
            desc->bDescriptorType = HID_HID_DESCRIPTOR_TYPE;
            desc->bcdHID = HID_REVISION;
            desc->bCountry = 0;
            desc->bNumDescriptors = 1;
            desc->DescriptorList[0].bReportType = HID_REPORT_DESCRIPTOR_TYPE;
            desc->DescriptorList[0].wReportLength = report_descriptor_len;
            irp->IoStatus.Information = sizeof(*desc);
        }
        ret = STATUS_SUCCESS;
        break;
    }

    case IOCTL_HID_GET_REPORT_DESCRIPTOR:
        ok( !in_size, "got input size %u\n", in_size );
        ok( out_size == report_descriptor_len, "got output size %u\n", out_size );

        if (out_size == report_descriptor_len)
        {
            memcpy( irp->UserBuffer, report_descriptor_buf, report_descriptor_len );
            irp->IoStatus.Information = report_descriptor_len;
        }
        ret = STATUS_SUCCESS;
        break;

    case IOCTL_HID_GET_DEVICE_ATTRIBUTES:
        ok( !in_size, "got input size %u\n", in_size );
        ok( out_size == sizeof(attributes), "got output size %u\n", out_size );

        if (out_size == sizeof(attributes))
        {
            memcpy( irp->UserBuffer, &attributes, sizeof(attributes) );
            irp->IoStatus.Information = sizeof(attributes);
        }
        ret = STATUS_SUCCESS;
        break;

    case IOCTL_HID_READ_REPORT:
    {
        ULONG expected_size = caps.InputReportByteLength - (report_id ? 0 : 1);
        ok( !in_size, "got input size %u\n", in_size );
        ok( out_size == expected_size, "got output size %u\n", out_size );
        ret = input_queue_read( &input_queue, irp );
        break;
    }

    case IOCTL_HID_WRITE_REPORT:
    {
        HID_XFER_PACKET *packet = irp->UserBuffer;
        ULONG expected_size = caps.OutputReportByteLength - (report_id ? 0 : 1);

        ok( in_size == sizeof(*packet), "got input size %u\n", in_size );
        ok( !out_size, "got output size %u\n", out_size );
        ok( packet->reportBufferLen >= expected_size, "got report size %u\n", packet->reportBufferLen );

        expect_queue_next( &expect_queue, code, packet, &index, &expect, TRUE );
        winetest_push_context( "%s expect[%d]", expect.context, index );
        ok( expect.code == code, "got %#x, expected %#x\n", expect.code, code );
        ok( packet->reportId == expect.report_id, "got id %u\n", packet->reportId );
        ok( packet->reportBufferLen == expect.report_len, "got len %u\n", packet->reportBufferLen );
        ok( RtlCompareMemory( packet->reportBuffer, expect.report_buf, expect.report_len ) == expect.report_len,
            "unexpected data\n" );
        winetest_pop_context();

        irp->IoStatus.Information = expect.ret_length ? expect.ret_length : expect.report_len;
        ret = expect.ret_status;
        break;
    }

    case IOCTL_HID_GET_INPUT_REPORT:
    {
        HID_XFER_PACKET *packet = irp->UserBuffer;
        ULONG expected_size = caps.InputReportByteLength - (report_id ? 0 : 1);
        ok( !in_size, "got input size %u\n", in_size );
        ok( out_size == sizeof(*packet), "got output size %u\n", out_size );

        ok( packet->reportBufferLen >= expected_size, "got len %u\n", packet->reportBufferLen );
        ok( !!packet->reportBuffer, "got buffer %p\n", packet->reportBuffer );

        expect_queue_next( &expect_queue, code, packet, &index, &expect, FALSE );
        winetest_push_context( "%s expect[%d]", expect.context, index );
        ok( expect.code == code, "got %#x, expected %#x\n", expect.code, code );
        ok( packet->reportId == expect.report_id, "got id %u\n", packet->reportId );
        ok( packet->reportBufferLen == expect.report_len, "got len %u\n", packet->reportBufferLen );
        winetest_pop_context();

        irp->IoStatus.Information = expect.ret_length ? expect.ret_length : expect.report_len;
        memcpy( packet->reportBuffer, expect.report_buf, irp->IoStatus.Information );
        ret = expect.ret_status;
        break;
    }

    case IOCTL_HID_SET_OUTPUT_REPORT:
    {
        HID_XFER_PACKET *packet = irp->UserBuffer;
        ULONG expected_size = caps.OutputReportByteLength - (report_id ? 0 : 1);
        ok( in_size == sizeof(*packet), "got input size %u\n", in_size );
        ok( !out_size, "got output size %u\n", out_size );

        ok( packet->reportBufferLen >= expected_size, "got len %u\n", packet->reportBufferLen );
        ok( !!packet->reportBuffer, "got buffer %p\n", packet->reportBuffer );

        expect_queue_next( &expect_queue, code, packet, &index, &expect, TRUE );
        winetest_push_context( "%s expect[%d]", expect.context, index );
        ok( expect.code == code, "got %#x, expected %#x\n", expect.code, code );
        ok( packet->reportId == expect.report_id, "got id %u\n", packet->reportId );
        ok( packet->reportBufferLen == expect.report_len, "got len %u\n", packet->reportBufferLen );
        ok( RtlCompareMemory( packet->reportBuffer, expect.report_buf, expect.report_len ) == expect.report_len,
            "unexpected data\n" );
        winetest_pop_context();

        irp->IoStatus.Information = expect.ret_length ? expect.ret_length : expect.report_len;
        ret = expect.ret_status;
        break;
    }

    case IOCTL_HID_GET_FEATURE:
    {
        HID_XFER_PACKET *packet = irp->UserBuffer;
        ULONG expected_size = caps.FeatureReportByteLength - (report_id ? 0 : 1);
        ok( !in_size, "got input size %u\n", in_size );
        ok( out_size == sizeof(*packet), "got output size %u\n", out_size );

        ok( packet->reportBufferLen >= expected_size, "got len %u\n", packet->reportBufferLen );
        ok( !!packet->reportBuffer, "got buffer %p\n", packet->reportBuffer );

        expect_queue_next( &expect_queue, code, packet, &index, &expect, FALSE );
        winetest_push_context( "%s expect[%d]", expect.context, index );
        ok( expect.code == code, "got %#x, expected %#x\n", expect.code, code );
        ok( packet->reportId == expect.report_id, "got id %u\n", packet->reportId );
        ok( packet->reportBufferLen == expect.report_len, "got len %u\n", packet->reportBufferLen );
        winetest_pop_context();

        irp->IoStatus.Information = expect.ret_length ? expect.ret_length : expect.report_len;
        memcpy( packet->reportBuffer, expect.report_buf, irp->IoStatus.Information );
        ret = expect.ret_status;
        break;
    }

    case IOCTL_HID_SET_FEATURE:
    {
        HID_XFER_PACKET *packet = irp->UserBuffer;
        ULONG expected_size = caps.FeatureReportByteLength - (report_id ? 0 : 1);
        ok( in_size == sizeof(*packet), "got input size %u\n", in_size );
        ok( !out_size, "got output size %u\n", out_size );

        ok( packet->reportBufferLen >= expected_size, "got len %u\n", packet->reportBufferLen );
        ok( !!packet->reportBuffer, "got buffer %p\n", packet->reportBuffer );

        expect_queue_next( &expect_queue, code, packet, &index, &expect, TRUE );
        winetest_push_context( "%s expect[%d]", expect.context, index );
        ok( expect.code == code, "got %#x, expected %#x\n", expect.code, code );
        ok( packet->reportId == expect.report_id, "got id %u\n", packet->reportId );
        ok( packet->reportBufferLen == expect.report_len, "got len %u\n", packet->reportBufferLen );
        ok( RtlCompareMemory( packet->reportBuffer, expect.report_buf, expect.report_len ) == expect.report_len,
            "unexpected data\n" );
        winetest_pop_context();

        irp->IoStatus.Information = expect.ret_length ? expect.ret_length : expect.report_len;
        ret = expect.ret_status;
        break;
    }

    case IOCTL_HID_GET_STRING:
        todo_wine
        ok( 0, "unexpected IOCTL_HID_GET_STRING\n" );
        memcpy( irp->UserBuffer, L"Wine Test", sizeof(L"Wine Test") );
        irp->IoStatus.Information = sizeof(L"Wine Test");
        ret = STATUS_SUCCESS;
        break;

    default:
        ok( 0, "unexpected ioctl %#x\n", code );
        ret = STATUS_NOT_IMPLEMENTED;
    }

    winetest_pop_context();

    if (ret != STATUS_PENDING)
    {
        irp->IoStatus.Status = ret;
        IoCompleteRequest( irp, IO_NO_INCREMENT );
    }
    return ret;
}

static NTSTATUS( WINAPI *hidclass_driver_ioctl )(DEVICE_OBJECT *device, IRP *irp);
static NTSTATUS WINAPI driver_ioctl( DEVICE_OBJECT *device, IRP *irp )
{
    IO_STACK_LOCATION *stack = IoGetCurrentIrpStackLocation( irp );
    ULONG in_size = stack->Parameters.DeviceIoControl.InputBufferLength;
    ULONG code = stack->Parameters.DeviceIoControl.IoControlCode;

    switch (code)
    {
    case IOCTL_WINETEST_HID_SET_EXPECT:
        expect_queue_reset( &expect_queue, irp->AssociatedIrp.SystemBuffer, in_size );
        irp->IoStatus.Status = STATUS_SUCCESS;
        IoCompleteRequest( irp, IO_NO_INCREMENT );
        return STATUS_SUCCESS;
    case IOCTL_WINETEST_HID_SEND_INPUT:
        input_queue_reset( &input_queue, irp->AssociatedIrp.SystemBuffer, in_size );
        irp->IoStatus.Status = STATUS_SUCCESS;
        IoCompleteRequest( irp, IO_NO_INCREMENT );
        return STATUS_SUCCESS;
    }

    return hidclass_driver_ioctl( device, irp );
}

static NTSTATUS WINAPI driver_add_device( DRIVER_OBJECT *driver, DEVICE_OBJECT *fdo )
{
    HID_DEVICE_EXTENSION *ext = fdo->DeviceExtension;
    NTSTATUS ret;

    /* We should be given the FDO, not the PDO. */
    ok( !!ext->PhysicalDeviceObject, "expected non-NULL pdo\n" );
    ok( ext->NextDeviceObject == ext->PhysicalDeviceObject, "got pdo %p, next %p\n",
        ext->PhysicalDeviceObject, ext->NextDeviceObject );
    todo_wine
    ok( ext->NextDeviceObject->AttachedDevice == fdo, "wrong attached device\n" );

    ret = IoRegisterDeviceInterface( ext->PhysicalDeviceObject, &control_class, NULL, &control_symlink );
    ok( !ret, "got %#x\n", ret );

    fdo->Flags &= ~DO_DEVICE_INITIALIZING;
    return STATUS_SUCCESS;
}

static NTSTATUS WINAPI driver_create( DEVICE_OBJECT *device, IRP *irp )
{
    ok( 0, "unexpected call\n" );
    irp->IoStatus.Status = STATUS_SUCCESS;
    IoCompleteRequest( irp, IO_NO_INCREMENT );
    return STATUS_SUCCESS;
}

static NTSTATUS WINAPI driver_close( DEVICE_OBJECT *device, IRP *irp )
{
    ok( 0, "unexpected call\n" );
    irp->IoStatus.Status = STATUS_SUCCESS;
    IoCompleteRequest( irp, IO_NO_INCREMENT );
    return STATUS_SUCCESS;
}

static void WINAPI driver_unload( DRIVER_OBJECT *driver )
{
    input_queue_cleanup( &input_queue );
    expect_queue_cleanup( &expect_queue );
    winetest_cleanup();
}

NTSTATUS WINAPI DriverEntry( DRIVER_OBJECT *driver, UNICODE_STRING *registry )
{
    static const int info_size = offsetof( KEY_VALUE_PARTIAL_INFORMATION, Data );
    HID_MINIDRIVER_REGISTRATION params =
    {
        .Revision = HID_REVISION,
        .DriverObject = driver,
        .DeviceExtensionSize = sizeof(struct hid_device),
        .RegistryPath = registry,
    };
    UNICODE_STRING name_str;
    OBJECT_ATTRIBUTES attr;
    NTSTATUS ret;
    char *buffer;
    HANDLE hkey;
    DWORD size;

    if ((ret = winetest_init())) return ret;

    InitializeObjectAttributes( &attr, registry, 0, NULL, NULL );
    ret = ZwOpenKey( &hkey, KEY_ALL_ACCESS, &attr );
    ok( !ret, "ZwOpenKey returned %#x\n", ret );

    buffer = ExAllocatePool( PagedPool, info_size + EXPECT_QUEUE_BUFFER_SIZE );
    ok( buffer != NULL, "ExAllocatePool failed\n" );
    if (!buffer) return STATUS_NO_MEMORY;

    RtlInitUnicodeString( &name_str, L"ReportID" );
    size = info_size + sizeof(report_id);
    ret = ZwQueryValueKey( hkey, &name_str, KeyValuePartialInformation, buffer, size, &size );
    ok( !ret, "ZwQueryValueKey returned %#x\n", ret );
    memcpy( &report_id, buffer + info_size, size - info_size );

    RtlInitUnicodeString( &name_str, L"PolledMode" );
    size = info_size + sizeof(polled);
    ret = ZwQueryValueKey( hkey, &name_str, KeyValuePartialInformation, buffer, size, &size );
    ok( !ret, "ZwQueryValueKey returned %#x\n", ret );
    memcpy( &polled, buffer + info_size, size - info_size );
    params.DevicesArePolled = polled;

    RtlInitUnicodeString( &name_str, L"Descriptor" );
    size = info_size + sizeof(report_descriptor_buf);
    ret = ZwQueryValueKey( hkey, &name_str, KeyValuePartialInformation, buffer, size, &size );
    ok( !ret, "ZwQueryValueKey returned %#x\n", ret );
    memcpy( report_descriptor_buf, buffer + info_size, size - info_size );
    report_descriptor_len = size - info_size;

    RtlInitUnicodeString( &name_str, L"Attributes" );
    size = info_size + sizeof(attributes);
    ret = ZwQueryValueKey( hkey, &name_str, KeyValuePartialInformation, buffer, size, &size );
    ok( !ret, "ZwQueryValueKey returned %#x\n", ret );
    memcpy( &attributes, buffer + info_size, size - info_size );

    RtlInitUnicodeString( &name_str, L"Caps" );
    size = info_size + sizeof(caps);
    ret = ZwQueryValueKey( hkey, &name_str, KeyValuePartialInformation, buffer, size, &size );
    ok( !ret, "ZwQueryValueKey returned %#x\n", ret );
    memcpy( &caps, buffer + info_size, size - info_size );

    expect_queue_init( &expect_queue );
    RtlInitUnicodeString( &name_str, L"Expect" );
    size = info_size + EXPECT_QUEUE_BUFFER_SIZE;
    ret = ZwQueryValueKey( hkey, &name_str, KeyValuePartialInformation, buffer, size, &size );
    ok( !ret, "ZwQueryValueKey returned %#x\n", ret );
    expect_queue_reset( &expect_queue, buffer + info_size, size - info_size );

    input_queue_init( &input_queue );
    RtlInitUnicodeString( &name_str, L"Input" );
    size = info_size + EXPECT_QUEUE_BUFFER_SIZE;
    ret = ZwQueryValueKey( hkey, &name_str, KeyValuePartialInformation, buffer, size, &size );
    ok( !ret, "ZwQueryValueKey returned %#x\n", ret );
    input_queue_reset( &input_queue, buffer + info_size, size - info_size );

    driver->DriverExtension->AddDevice = driver_add_device;
    driver->DriverUnload = driver_unload;
    driver->MajorFunction[IRP_MJ_PNP] = driver_pnp;
    driver->MajorFunction[IRP_MJ_POWER] = driver_power;
    driver->MajorFunction[IRP_MJ_DEVICE_CONTROL] = driver_ioctl;
    driver->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = driver_internal_ioctl;
    driver->MajorFunction[IRP_MJ_CREATE] = driver_create;
    driver->MajorFunction[IRP_MJ_CLOSE] = driver_close;

    ExFreePool( buffer );

    ret = HidRegisterMinidriver( &params );
    ok( !ret, "got %#x\n", ret );

    hidclass_driver_ioctl = driver->MajorFunction[IRP_MJ_DEVICE_CONTROL];
    driver->MajorFunction[IRP_MJ_DEVICE_CONTROL] = driver_ioctl;

    return STATUS_SUCCESS;
}
