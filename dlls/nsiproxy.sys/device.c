/*
 * nsiproxy.sys
 *
 * Copyright 2021 Huw Davies
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
#include "winternl.h"
#include "winioctl.h"
#include "ddk/wdm.h"
#include "ifdef.h"
#include "netiodef.h"
#include "wine/nsi.h"
#include "wine/debug.h"
#include "wine/unixlib.h"

#include "nsiproxy_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(nsi);

static unixlib_handle_t nsiproxy_handle;
static HANDLE request_event;

#define DECLARE_CRITICAL_SECTION(cs)                                    \
    static CRITICAL_SECTION cs;                                         \
    static CRITICAL_SECTION_DEBUG cs##_debug =                          \
    { 0, 0, &cs, { &cs##_debug.ProcessLocksList, &cs##_debug.ProcessLocksList }, \
      0, 0, { (DWORD_PTR)(__FILE__ ": " # cs) }};                       \
    static CRITICAL_SECTION cs = { &cs##_debug, -1, 0, 0, 0, 0 };
DECLARE_CRITICAL_SECTION( nsiproxy_cs );

#define LIST_ENTRY_INIT( list )  { .Flink = &(list), .Blink = &(list) }
static LIST_ENTRY request_queue = LIST_ENTRY_INIT( request_queue );

static NTSTATUS nsiproxy_call( unsigned int code, void *args )
{
    return __wine_unix_call( nsiproxy_handle, code, args );
}

enum unix_calls
{
    icmp_cancel_listen,
    icmp_close,
    icmp_listen,
    icmp_send_echo,
    nsi_enumerate_all_ex,
    nsi_get_all_parameters_ex,
    nsi_get_parameter_ex,
};

static NTSTATUS nsiproxy_enumerate_all( IRP *irp )
{
    IO_STACK_LOCATION *irpsp = IoGetCurrentIrpStackLocation( irp );
    struct nsiproxy_enumerate_all *in = (struct nsiproxy_enumerate_all *)irp->AssociatedIrp.SystemBuffer;
    DWORD in_len = irpsp->Parameters.DeviceIoControl.InputBufferLength;
    void *out = irp->AssociatedIrp.SystemBuffer;
    DWORD out_len = irpsp->Parameters.DeviceIoControl.OutputBufferLength;
    struct nsi_enumerate_all_ex enum_all;
    NTSTATUS status;

    if (in_len != sizeof(*in)) return STATUS_INVALID_PARAMETER;

    if (out_len < sizeof(DWORD) + (in->key_size + in->rw_size + in->dynamic_size + in->static_size) * in->count)
        return STATUS_INVALID_PARAMETER;

    enum_all.unknown[0] = 0;
    enum_all.unknown[1] = 0;
    enum_all.first_arg = in->first_arg;
    enum_all.second_arg = in->second_arg;
    enum_all.module = &in->module;
    enum_all.table = in->table;
    enum_all.key_data = (BYTE *)out + sizeof(DWORD);
    enum_all.key_size = in->key_size;
    enum_all.rw_data = (BYTE *)enum_all.key_data + in->key_size * in->count;
    enum_all.rw_size = in->rw_size;
    enum_all.dynamic_data = (BYTE *)enum_all.rw_data + in->rw_size * in->count;
    enum_all.dynamic_size = in->dynamic_size;
    enum_all.static_data = (BYTE *)enum_all.dynamic_data + in->dynamic_size * in->count;
    enum_all.static_size = in->static_size;
    enum_all.count = in->count;

    status = nsiproxy_call( nsi_enumerate_all_ex, &enum_all );
    if (status == STATUS_SUCCESS || status == STATUS_BUFFER_OVERFLOW)
    {
        irp->IoStatus.Information = out_len;
        *(DWORD *)out = enum_all.count;
    }
    else irp->IoStatus.Information = 0;

    return status;
}

static NTSTATUS nsiproxy_get_all_parameters( IRP *irp )
{
    IO_STACK_LOCATION *irpsp = IoGetCurrentIrpStackLocation( irp );
    struct nsiproxy_get_all_parameters *in = (struct nsiproxy_get_all_parameters *)irp->AssociatedIrp.SystemBuffer;
    DWORD in_len = irpsp->Parameters.DeviceIoControl.InputBufferLength;
    BYTE *out = irp->AssociatedIrp.SystemBuffer;
    DWORD out_len = irpsp->Parameters.DeviceIoControl.OutputBufferLength;
    struct nsi_get_all_parameters_ex get_all;
    NTSTATUS status;

    if (in_len < offsetof(struct nsiproxy_get_all_parameters, key[0]) ||
        in_len < offsetof(struct nsiproxy_get_all_parameters, key[in->key_size]))
        return STATUS_INVALID_PARAMETER;

    if (out_len < in->rw_size + in->dynamic_size + in->static_size)
        return STATUS_INVALID_PARAMETER;

    get_all.unknown[0] = 0;
    get_all.unknown[1] = 0;
    get_all.first_arg = in->first_arg;
    get_all.unknown2 = 0;
    get_all.module = &in->module;
    get_all.table = in->table;
    get_all.key = in->key;
    get_all.key_size = in->key_size;
    get_all.rw_data = out;
    get_all.rw_size = in->rw_size;
    get_all.dynamic_data = out + in->rw_size;
    get_all.dynamic_size = in->dynamic_size;
    get_all.static_data = out + in->rw_size + in->dynamic_size;
    get_all.static_size = in->static_size;

    status = nsiproxy_call( nsi_get_all_parameters_ex, &get_all );
    irp->IoStatus.Information = (status == STATUS_SUCCESS) ? out_len : 0;

    return status;
}

static NTSTATUS nsiproxy_get_parameter( IRP *irp )
{
    IO_STACK_LOCATION *irpsp = IoGetCurrentIrpStackLocation( irp );
    struct nsiproxy_get_parameter *in = (struct nsiproxy_get_parameter *)irp->AssociatedIrp.SystemBuffer;
    DWORD in_len = irpsp->Parameters.DeviceIoControl.InputBufferLength;
    void *out = irp->AssociatedIrp.SystemBuffer;
    DWORD out_len = irpsp->Parameters.DeviceIoControl.OutputBufferLength;
    struct nsi_get_parameter_ex get_param;
    NTSTATUS status;

    if (in_len < offsetof(struct nsiproxy_get_parameter, key[0]) ||
        in_len < offsetof(struct nsiproxy_get_parameter, key[in->key_size]))
        return STATUS_INVALID_PARAMETER;

    get_param.unknown[0] = 0;
    get_param.unknown[1] = 0;
    get_param.first_arg = in->first_arg;
    get_param.unknown2 = 0;
    get_param.module = &in->module;
    get_param.table = in->table;
    get_param.key = in->key;
    get_param.key_size = in->key_size;
    get_param.param_type = in->param_type;
    get_param.data = out;
    get_param.data_size = out_len;
    get_param.data_offset = in->data_offset;

    status = nsiproxy_call( nsi_get_parameter_ex, &get_param );
    irp->IoStatus.Information = (status == STATUS_SUCCESS) ? out_len : 0;

    return status;
}

static inline HANDLE irp_get_icmp_handle( IRP *irp )
{
    return irp->Tail.Overlay.DriverContext[0];
}

static inline HANDLE irp_set_icmp_handle( IRP *irp, HANDLE handle )
{
    return InterlockedExchangePointer( irp->Tail.Overlay.DriverContext, handle );
}

static void WINAPI icmp_echo_cancel( DEVICE_OBJECT *device, IRP *irp )
{
    HANDLE handle;

    TRACE( "device %p, irp %p.\n", device, irp );

    IoReleaseCancelSpinLock( irp->CancelIrql );

    EnterCriticalSection( &nsiproxy_cs );

    /* If the handle is not set, either the irp is still
       in the request queue, in which case the request thread will
       cancel it, or the irp has already finished.  If the handle
       does exist then notify the listen thread.  In all cases the irp
       will be completed elsewhere. */
    handle = irp_get_icmp_handle( irp );
    if (handle) nsiproxy_call( icmp_cancel_listen, handle );

    LeaveCriticalSection( &nsiproxy_cs );
}

static NTSTATUS nsiproxy_icmp_echo( IRP *irp )
{
    IO_STACK_LOCATION *irpsp = IoGetCurrentIrpStackLocation( irp );
    struct nsiproxy_icmp_echo *in = (struct nsiproxy_icmp_echo *)irp->AssociatedIrp.SystemBuffer;
    DWORD in_len = irpsp->Parameters.DeviceIoControl.InputBufferLength;
    DWORD out_len = irpsp->Parameters.DeviceIoControl.OutputBufferLength;

    TRACE( "\n" );

    if (in_len < offsetof(struct nsiproxy_icmp_echo, data[0]) ||
        in_len < offsetof(struct nsiproxy_icmp_echo, data[((in->opt_size + 3) & ~3) + in->req_size]) ||
        out_len < sizeof(struct nsiproxy_icmp_echo_reply))
        return STATUS_INVALID_PARAMETER;

    switch (in->dst.si_family)
    {
    case AF_INET:
        if (in->dst.Ipv4.sin_addr.s_addr == INADDR_ANY) return STATUS_INVALID_ADDRESS_WILDCARD;
        break;
    default:
        return STATUS_INVALID_PARAMETER;
    }

    EnterCriticalSection( &nsiproxy_cs );

    IoSetCancelRoutine( irp, icmp_echo_cancel );
    if (irp->Cancel && !IoSetCancelRoutine( irp, NULL ))
    {
        /* IRP was canceled before we set cancel routine */
        InitializeListHead( &irp->Tail.Overlay.ListEntry );
        LeaveCriticalSection( &nsiproxy_cs );
        return STATUS_CANCELLED;
    }

    InsertTailList( &request_queue, &irp->Tail.Overlay.ListEntry );
    IoMarkIrpPending( irp );

    LeaveCriticalSection( &nsiproxy_cs );
    SetEvent( request_event );

    return STATUS_PENDING;
}

static NTSTATUS WINAPI nsi_ioctl( DEVICE_OBJECT *device, IRP *irp )
{
    IO_STACK_LOCATION *irpsp = IoGetCurrentIrpStackLocation( irp );
    NTSTATUS status;

    TRACE( "ioctl %x insize %u outsize %u\n",
           irpsp->Parameters.DeviceIoControl.IoControlCode,
           irpsp->Parameters.DeviceIoControl.InputBufferLength,
           irpsp->Parameters.DeviceIoControl.OutputBufferLength );

    switch (irpsp->Parameters.DeviceIoControl.IoControlCode)
    {
    case IOCTL_NSIPROXY_WINE_ENUMERATE_ALL:
        status = nsiproxy_enumerate_all( irp );
        break;

    case IOCTL_NSIPROXY_WINE_GET_ALL_PARAMETERS:
        status = nsiproxy_get_all_parameters( irp );
        break;

    case IOCTL_NSIPROXY_WINE_GET_PARAMETER:
        status = nsiproxy_get_parameter( irp );
        break;

    case IOCTL_NSIPROXY_WINE_ICMP_ECHO:
        status = nsiproxy_icmp_echo( irp );
        break;

    default:
        FIXME( "ioctl %x not supported\n", irpsp->Parameters.DeviceIoControl.IoControlCode );
        status = STATUS_NOT_SUPPORTED;
        break;
    }

    if (status != STATUS_PENDING)
    {
        irp->IoStatus.Status = status;
        IoCompleteRequest( irp, IO_NO_INCREMENT );
    }
    return status;
}

static int add_device( DRIVER_OBJECT *driver )
{
    UNICODE_STRING name, link;
    DEVICE_OBJECT *device;
    NTSTATUS status;

    RtlInitUnicodeString( &name, L"\\Device\\Nsi" );
    RtlInitUnicodeString( &link, L"\\??\\Nsi" );

    if (!(status = IoCreateDevice( driver, 0, &name, FILE_DEVICE_NETWORK, FILE_DEVICE_SECURE_OPEN, FALSE, &device )))
        status = IoCreateSymbolicLink( &link, &name );
    if (status)
    {
        FIXME( "failed to create device error %x\n", status );
        return 0;
    }

    return 1;
}

static DWORD WINAPI listen_thread_proc( void *arg )
{
    IRP *irp = arg;
    IO_STACK_LOCATION *irpsp = IoGetCurrentIrpStackLocation( irp );
    struct nsiproxy_icmp_echo *in = irp->AssociatedIrp.SystemBuffer;
    struct icmp_listen_params params;
    NTSTATUS status;

    TRACE( "\n" );

    params.handle = irp_get_icmp_handle( irp );
    params.timeout = in->timeout;
    params.reply = irp->AssociatedIrp.SystemBuffer;
    params.reply_len = irpsp->Parameters.DeviceIoControl.OutputBufferLength;

    status = nsiproxy_call( icmp_listen, &params );
    TRACE( "icmp_listen rets %08x\n", status );

    EnterCriticalSection( &nsiproxy_cs );

    nsiproxy_call( icmp_close, irp_set_icmp_handle( irp, NULL ) );

    irp->IoStatus.Status = status;
    if (status == STATUS_SUCCESS)
        irp->IoStatus.Information = params.reply_len;
    else
        irp->IoStatus.Information = 0;
    IoCompleteRequest( irp, IO_NO_INCREMENT );

    LeaveCriticalSection( &nsiproxy_cs );

    return 0;
}

static void handle_queued_send_echo( IRP *irp )
{
    struct nsiproxy_icmp_echo *in = (struct nsiproxy_icmp_echo *)irp->AssociatedIrp.SystemBuffer;
    struct nsiproxy_icmp_echo_reply *reply = (struct nsiproxy_icmp_echo_reply *)irp->AssociatedIrp.SystemBuffer;
    struct icmp_send_echo_params params;
    NTSTATUS status;

    TRACE( "\n" );
    params.request = in->data + ((in->opt_size + 3) & ~3);
    params.request_size = in->req_size;
    params.ttl = in->ttl;
    params.tos = in->tos;
    params.dst = &in->dst;

    status = nsiproxy_call( icmp_send_echo, &params );
    TRACE( "icmp_send_echo rets %08x\n", status );

    if (status != STATUS_PENDING)
    {
        irp->IoStatus.Status = status;
        if (status == STATUS_SUCCESS)
        {
            memset( reply, 0, sizeof(*reply) );
            reply->status = params.ip_status;
            irp->IoStatus.Information = sizeof(*reply);
        }
        IoCompleteRequest( irp, IO_NO_INCREMENT );
    }
    else
    {
        irp_set_icmp_handle( irp, params.handle );
        RtlQueueWorkItem( listen_thread_proc, irp, WT_EXECUTELONGFUNCTION );
    }
}

static DWORD WINAPI request_thread_proc( void *arg )
{
    LIST_ENTRY *entry;

    while (WaitForSingleObject( request_event, INFINITE ) == WAIT_OBJECT_0)
    {
        TRACE( "request_event triggered\n" );
        EnterCriticalSection( &nsiproxy_cs );
        while ((entry = RemoveHeadList( &request_queue )) != &request_queue )
        {
            IRP *irp = CONTAINING_RECORD( entry, IRP, Tail.Overlay.ListEntry );

            if (irp->Cancel)
            {
                irp->IoStatus.Status = STATUS_CANCELLED;
                TRACE( "already cancelled\n" );
                IoCompleteRequest( irp, IO_NO_INCREMENT );
                continue;
            }

            handle_queued_send_echo( irp );
        }
        LeaveCriticalSection( &nsiproxy_cs );
    }
    return 0;
}

NTSTATUS WINAPI DriverEntry( DRIVER_OBJECT *driver, UNICODE_STRING *path )
{
    HMODULE instance;
    NTSTATUS status;
    HANDLE thread;

    TRACE( "(%p, %s)\n", driver, debugstr_w( path->Buffer ) );

    RtlPcToFileHeader( &DriverEntry, (void *)&instance );
    status = NtQueryVirtualMemory( GetCurrentProcess(), instance, MemoryWineUnixFuncs,
                                   &nsiproxy_handle, sizeof(nsiproxy_handle), NULL );
    if (status) return status;

    driver->MajorFunction[IRP_MJ_DEVICE_CONTROL] = nsi_ioctl;

    add_device( driver );

    request_event = CreateEventW( NULL, FALSE, FALSE, NULL );
    thread = CreateThread( NULL, 0, request_thread_proc, NULL, 0, NULL );
    CloseHandle( thread );

    return STATUS_SUCCESS;
}
