/*
 * Copyright 2022 Julian Klemann for CodeWeavers
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

#define COBJMACROS
#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "winstring.h"

#include "initguid.h"
#include "roapi.h"

#define WIDL_using_Windows_Foundation
#define WIDL_using_Windows_Foundation_Collections
#include "windows.foundation.h"
#define WIDL_using_Windows_Devices_Enumeration
#include "windows.devices.enumeration.h"

#include "wine/test.h"

#define IDeviceInformationStatics2_CreateWatcher IDeviceInformationStatics2_CreateWatcherWithKindAqsFilterAndAdditionalProperties
#define check_interface( obj, iid, exp ) check_interface_( __LINE__, obj, iid, exp, FALSE )
#define check_optional_interface( obj, iid, exp ) check_interface_( __LINE__, obj, iid, exp, TRUE )
static void check_interface_(unsigned int line, void *obj, const IID *iid, BOOL supported, BOOL optional)
{
    IUnknown *iface = obj;
    HRESULT hr, expected_hr;
    IUnknown *unk;

    expected_hr = supported ? S_OK : E_NOINTERFACE;

    hr = IUnknown_QueryInterface(iface, iid, (void **)&unk);
    ok_(__FILE__, line)(hr == expected_hr || broken(hr == E_NOINTERFACE && optional), "Got hr %#lx, expected %#lx.\n", hr, expected_hr);
    if (SUCCEEDED(hr))
        IUnknown_Release(unk);
}

struct device_watcher_handler
{
    ITypedEventHandler_DeviceWatcher_IInspectable ITypedEventHandler_DeviceWatcher_IInspectable_iface;
    LONG ref;

    HANDLE event;
    BOOL invoked;
    IInspectable *args;
};

static inline struct device_watcher_handler *impl_from_ITypedEventHandler_DeviceWatcher_IInspectable(
        ITypedEventHandler_DeviceWatcher_IInspectable *iface )
{
    return CONTAINING_RECORD( iface, struct device_watcher_handler, ITypedEventHandler_DeviceWatcher_IInspectable_iface );
}

static HRESULT WINAPI device_watcher_handler_QueryInterface(
        ITypedEventHandler_DeviceWatcher_IInspectable *iface, REFIID iid, void **out )
{
    struct device_watcher_handler *impl = impl_from_ITypedEventHandler_DeviceWatcher_IInspectable( iface );

    if (IsEqualGUID( iid, &IID_IUnknown ) ||
        IsEqualGUID( iid, &IID_ITypedEventHandler_DeviceWatcher_IInspectable ))
    {
        IUnknown_AddRef( &impl->ITypedEventHandler_DeviceWatcher_IInspectable_iface );
        *out = &impl->ITypedEventHandler_DeviceWatcher_IInspectable_iface;
        return S_OK;
    }

    trace( "%s not implemented, returning E_NO_INTERFACE.\n", debugstr_guid( iid ) );
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI device_watcher_handler_AddRef( ITypedEventHandler_DeviceWatcher_IInspectable *iface )
{
    struct device_watcher_handler *impl = impl_from_ITypedEventHandler_DeviceWatcher_IInspectable( iface );
    ULONG ref = InterlockedIncrement( &impl->ref );
    return ref;
}

static ULONG WINAPI device_watcher_handler_Release( ITypedEventHandler_DeviceWatcher_IInspectable *iface )
{
    struct device_watcher_handler *impl = impl_from_ITypedEventHandler_DeviceWatcher_IInspectable( iface );
    ULONG ref = InterlockedDecrement( &impl->ref );
    return ref;
}

static HRESULT WINAPI device_watcher_handler_Invoke( ITypedEventHandler_DeviceWatcher_IInspectable *iface,
                                                     IDeviceWatcher *sender, IInspectable *args )
{
    struct device_watcher_handler *impl = impl_from_ITypedEventHandler_DeviceWatcher_IInspectable( iface );
    ULONG ref;
    trace( "iface %p, sender %p, args %p\n", iface, sender, args );

    impl->invoked = TRUE;
    impl->args = args;

    IDeviceWatcher_AddRef( sender );
    ref = IDeviceWatcher_Release( sender );
    ok( ref == 3, "got ref %lu\n", ref );

    SetEvent( impl->event );

    return S_OK;
}

static const ITypedEventHandler_DeviceWatcher_IInspectableVtbl device_watcher_handler_vtbl =
{
    device_watcher_handler_QueryInterface,
    device_watcher_handler_AddRef,
    device_watcher_handler_Release,
    /* ITypedEventHandler<DeviceWatcher*,IInspectable*> methods */
    device_watcher_handler_Invoke,
};

static void device_watcher_handler_create( struct device_watcher_handler *impl )
{
    impl->ITypedEventHandler_DeviceWatcher_IInspectable_iface.lpVtbl = &device_watcher_handler_vtbl;
    impl->invoked = FALSE;
    impl->ref = 1;
}

static void test_DeviceInformation( void )
{
    static const WCHAR *device_info_name = L"Windows.Devices.Enumeration.DeviceInformation";

    static struct device_watcher_handler stopped_handler, added_handler;
    EventRegistrationToken stopped_token, added_token;
    IInspectable *inspectable, *inspectable2;
    IActivationFactory *factory;
    IDeviceInformationStatics2 *device_info_statics2;
    IDeviceWatcher *device_watcher;
    DeviceWatcherStatus status = 0xdeadbeef;
    ULONG ref;
    HSTRING str;
    HRESULT hr;

    device_watcher_handler_create( &added_handler );
    device_watcher_handler_create( &stopped_handler );
    stopped_handler.event = CreateEventW( NULL, FALSE, FALSE, NULL );
    ok( !!stopped_handler.event, "failed to create event, got error %lu\n", GetLastError() );

    hr = WindowsCreateString( device_info_name, wcslen( device_info_name ), &str );
    ok( hr == S_OK, "got hr %#lx\n", hr );
    hr = RoGetActivationFactory( str, &IID_IActivationFactory, (void **)&factory );
    ok( hr == S_OK || broken( hr == REGDB_E_CLASSNOTREG ), "got hr %#lx\n", hr );
    if ( hr == REGDB_E_CLASSNOTREG )
    {
        win_skip( "%s runtimeclass, not registered.\n", wine_dbgstr_w( device_info_name ) );
        goto done;
    }

    hr = IActivationFactory_QueryInterface( factory, &IID_IInspectable, (void **)&inspectable );
    ok( hr == S_OK, "got hr %#lx\n", hr );
    check_interface( factory, &IID_IAgileObject, FALSE );

    hr = IActivationFactory_QueryInterface( factory, &IID_IDeviceInformationStatics2, (void **)&device_info_statics2 );
    ok( hr == S_OK || broken( hr == E_NOINTERFACE ), "got hr %#lx\n", hr );
    if (FAILED( hr ))
    {
        win_skip( "IDeviceInformationStatics2 not supported.\n" );
        goto skip_device_statics2;
    }

    hr = IDeviceInformationStatics2_QueryInterface( device_info_statics2, &IID_IInspectable, (void **)&inspectable2 );
    ok( hr == S_OK, "got hr %#lx\n", hr );
    ok( inspectable == inspectable2, "got inspectable %p, inspectable2 %p\n", inspectable, inspectable2 );

    hr = IDeviceInformationStatics2_CreateWatcher( device_info_statics2, NULL, NULL, DeviceInformationKind_AssociationEndpoint, &device_watcher );
    check_interface( device_watcher, &IID_IUnknown, TRUE );
    check_interface( device_watcher, &IID_IInspectable, TRUE );
    check_interface( device_watcher, &IID_IAgileObject, TRUE );
    check_interface( device_watcher, &IID_IDeviceWatcher, TRUE );

    hr = IDeviceWatcher_add_Added(
            device_watcher,
            (ITypedEventHandler_DeviceWatcher_DeviceInformation *)&added_handler.ITypedEventHandler_DeviceWatcher_IInspectable_iface,
            &added_token );
    ok( hr == S_OK, "got hr %#lx\n", hr );
    hr = IDeviceWatcher_add_Stopped(
            device_watcher, &stopped_handler.ITypedEventHandler_DeviceWatcher_IInspectable_iface,
            &stopped_token );
    ok( hr == S_OK, "got hr %#lx\n", hr );

    hr = IDeviceWatcher_get_Status( device_watcher, &status );
    todo_wine ok( hr == S_OK, "got hr %#lx\n", hr );
    todo_wine ok( status == DeviceWatcherStatus_Created, "got status %u\n", status );

    hr = IDeviceWatcher_Start( device_watcher );
    ok( hr == S_OK, "got hr %#lx\n", hr );
    hr = IDeviceWatcher_get_Status( device_watcher, &status );
    todo_wine ok( hr == S_OK, "got hr %#lx\n", hr );
    todo_wine ok( status == DeviceWatcherStatus_Started, "got status %u\n", status );

    ref = IDeviceWatcher_AddRef( device_watcher );
    ok( ref == 2, "got ref %lu\n", ref );
    hr = IDeviceWatcher_Stop( device_watcher );
    ok( hr == S_OK, "got hr %#lx\n", hr );
    ok( !WaitForSingleObject( stopped_handler.event, 1000 ), "wait for stopped_handler.event failed\n" );

    hr = IDeviceWatcher_get_Status( device_watcher, &status );
    todo_wine ok( hr == S_OK, "got hr %#lx\n", hr );
    todo_wine ok( status == DeviceWatcherStatus_Stopped, "got status %u\n", status );
    ok( stopped_handler.invoked, "stopped_handler not invoked\n" );
    ok( stopped_handler.args == NULL, "stopped_handler not invoked\n" );

    IDeviceWatcher_Release( device_watcher );
    IInspectable_Release( inspectable2 );
    IDeviceInformationStatics2_Release( device_info_statics2 );

skip_device_statics2:
    IInspectable_Release( inspectable );
    ref = IActivationFactory_Release( factory );
    ok( ref == 1, "got ref %lu\n", ref );

done:
    WindowsDeleteString( str );
    CloseHandle( stopped_handler.event );
}

static void test_DeviceAccessInformation( void )
{
    static const WCHAR *device_access_info_name = L"Windows.Devices.Enumeration.DeviceAccessInformation";
    static const WCHAR *device_info_name = L"Windows.Devices.Enumeration.DeviceInformation";
    IDeviceAccessInformationStatics *statics;
    IActivationFactory *factory, *factory2;
    IDeviceAccessInformation *access_info;
    enum DeviceAccessStatus access_status;
    HSTRING str;
    HRESULT hr;
    ULONG ref;

    hr = WindowsCreateString( device_access_info_name, wcslen( device_access_info_name ), &str );
    ok( hr == S_OK, "got hr %#lx\n", hr );
    hr = RoGetActivationFactory( str, &IID_IActivationFactory, (void **)&factory );
    ok( hr == S_OK || broken( hr == REGDB_E_CLASSNOTREG ), "got hr %#lx\n", hr );
    WindowsDeleteString( str );

    if (hr == REGDB_E_CLASSNOTREG)
    {
        win_skip( "%s runtimeclass not registered.\n", wine_dbgstr_w(device_access_info_name) );
        return;
    }

    hr = WindowsCreateString( device_info_name, wcslen( device_info_name ), &str );
    ok( hr == S_OK, "got hr %#lx\n", hr );
    hr = RoGetActivationFactory( str, &IID_IActivationFactory, (void **)&factory2 );
    ok( hr == S_OK, "got hr %#lx\n", hr );
    WindowsDeleteString( str );

    ok( factory != factory2, "Got the same factory.\n" );
    IActivationFactory_Release( factory2 );

    check_interface( factory, &IID_IAgileObject, FALSE );
    check_interface( factory, &IID_IDeviceAccessInformation, FALSE );

    hr = IActivationFactory_QueryInterface( factory, &IID_IDeviceAccessInformationStatics, (void **)&statics );
    ok( hr == S_OK, "got hr %#lx\n", hr );

    hr = IDeviceAccessInformationStatics_CreateFromDeviceClass( statics, DeviceClass_AudioCapture, &access_info );
    ok( hr == S_OK || broken( hr == RPC_E_CALL_COMPLETE ) /* broken on some Testbot machines */, "got hr %#lx\n", hr );

    if (hr == S_OK)
    {
        hr = IDeviceAccessInformation_get_CurrentStatus( access_info, &access_status );
        ok( hr == S_OK, "got hr %#lx\n", hr );
        ok( access_status == DeviceAccessStatus_Allowed, "got %d.\n", access_status );
        ref = IDeviceAccessInformation_Release( access_info );
        ok( !ref, "got ref %lu\n", ref );
    }

    ref = IDeviceAccessInformationStatics_Release( statics );
    ok( ref == 2, "got ref %lu\n", ref );
    ref = IActivationFactory_Release( factory );
    ok( ref == 1, "got ref %lu\n", ref );
}

START_TEST( devices )
{
    HRESULT hr;

    hr = RoInitialize( RO_INIT_MULTITHREADED );
    ok( hr == S_OK, "got hr %#lx\n", hr );

    test_DeviceInformation();
    test_DeviceAccessInformation();

    RoUninitialize();
}
