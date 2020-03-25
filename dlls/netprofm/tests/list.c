/*
 * Copyright 2014 Hans Leidekker for CodeWeavers
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

#include <stdio.h>
#include "windows.h"
#define COBJMACROS
#include "initguid.h"
#include "objbase.h"
#include "ocidl.h"
#include "netlistmgr.h"
#include "wine/test.h"

static void test_INetwork( INetwork *network )
{
    NLM_NETWORK_CATEGORY category;
    NLM_CONNECTIVITY connectivity;
    NLM_DOMAIN_TYPE domain_type;
    VARIANT_BOOL connected;
    GUID id;
    BSTR str;
    HRESULT hr;

    str = NULL;
    hr = INetwork_GetName( network, &str );
    todo_wine ok( hr == S_OK, "got %08x\n", hr );
    todo_wine ok( str != NULL, "str not set\n" );
    if (str) trace( "name %s\n", wine_dbgstr_w(str) );
    SysFreeString( str );

    str = NULL;
    hr = INetwork_GetDescription( network, &str );
    todo_wine ok( hr == S_OK, "got %08x\n", hr );
    todo_wine ok( str != NULL, "str not set\n" );
    if (str) trace( "description %s\n", wine_dbgstr_w(str) );
    SysFreeString( str );

    memset( &id, 0, sizeof(id) );
    hr = INetwork_GetNetworkId( network, &id );
    ok( hr == S_OK, "got %08x\n", hr );
    trace("network id %s\n", wine_dbgstr_guid(&id));

    domain_type = 0xdeadbeef;
    hr = INetwork_GetDomainType( network, &domain_type );
    ok( hr == S_OK, "got %08x\n", hr );
    ok( domain_type != 0xdeadbeef, "domain_type not set\n" );
    trace( "domain type %08x\n", domain_type );

    category = 0xdeadbeef;
    hr = INetwork_GetCategory( network, &category );
    ok( hr == S_OK, "got %08x\n", hr );
    ok( category != 0xdeadbeef, "category not set\n" );
    trace( "category %08x\n", category );

    connectivity = 0xdeadbeef;
    hr = INetwork_GetConnectivity( network, &connectivity );
    ok( hr == S_OK, "got %08x\n", hr );
    ok( connectivity != 0xdeadbeef, "connectivity not set\n" );
    trace( "connectivity %08x\n", connectivity );

    connected = 0xdead;
    hr = INetwork_get_IsConnected( network, &connected );
    ok( hr == S_OK, "got %08x\n", hr );
    trace("connected %d\n", connected);

    connected = 0xdead;
    hr = INetwork_get_IsConnectedToInternet( network, &connected );
    ok( hr == S_OK, "got %08x\n", hr );
    trace("connected to internet %d\n", connected);
}

static void test_INetworkConnection( INetworkConnection *conn )
{
    INetwork *network;
    INetworkConnectionCost *conn_cost;
    NLM_CONNECTIVITY connectivity;
    NLM_DOMAIN_TYPE domain_type;
    VARIANT_BOOL connected;
    GUID id;
    HRESULT hr;

    memset( &id, 0, sizeof(id) );
    hr = INetworkConnection_GetAdapterId( conn, &id );
    ok( hr == S_OK, "got %08x\n", hr );
    trace("adapter id %s\n", wine_dbgstr_guid(&id));

    memset( &id, 0, sizeof(id) );
    hr = INetworkConnection_GetConnectionId( conn, &id );
    ok( hr == S_OK, "got %08x\n", hr );
    trace("connection id %s\n", wine_dbgstr_guid(&id));

    connectivity = 0xdeadbeef;
    hr = INetworkConnection_GetConnectivity( conn, &connectivity );
    ok( hr == S_OK, "got %08x\n", hr );
    ok( connectivity != 0xdeadbeef, "connectivity not set\n" );
    trace( "connectivity %08x\n", connectivity );

    domain_type = 0xdeadbeef;
    hr = INetworkConnection_GetDomainType( conn, &domain_type );
    ok( hr == S_OK, "got %08x\n", hr );
    ok( domain_type != 0xdeadbeef, "domain_type not set\n" );
    trace( "domain type %08x\n", domain_type );

    connected = 0xdead;
    hr = INetworkConnection_get_IsConnected( conn, &connected );
    ok( hr == S_OK, "got %08x\n", hr );
    trace("connected %d\n", connected);

    connected = 0xdead;
    hr = INetworkConnection_get_IsConnectedToInternet( conn, &connected );
    ok( hr == S_OK, "got %08x\n", hr );
    trace("connected to internet %d\n", connected);

    network = NULL;
    hr = INetworkConnection_GetNetwork( conn, &network );
    ok( hr == S_OK, "got %08x\n", hr );
    if (network)
    {
        test_INetwork( network );
        INetwork_Release( network );
    }

    conn_cost = NULL;
    hr = INetworkConnection_QueryInterface( conn, &IID_INetworkConnectionCost, (void **)&conn_cost );
    ok( hr == S_OK || broken(hr == E_NOINTERFACE), "got %08x\n", hr );
    if (conn_cost)
    {
        DWORD cost;
        NLM_DATAPLAN_STATUS status;

        cost = 0xdeadbeef;
        hr = INetworkConnectionCost_GetCost( conn_cost, &cost );
        ok( hr == S_OK, "got %08x\n", hr );
        ok( cost != 0xdeadbeef, "cost not set\n" );
        trace("cost %08x\n", cost);

        memset( &status, 0,sizeof(status) );
        hr = INetworkConnectionCost_GetDataPlanStatus( conn_cost, &status );
        ok( hr == S_OK, "got %08x\n", hr );
        trace("InterfaceGuid %s\n", wine_dbgstr_guid(&status.InterfaceGuid));

        INetworkConnectionCost_Release( conn_cost );
    }
}

static HRESULT WINAPI Unknown_QueryInterface(INetworkListManagerEvents *iface, REFIID riid, void **ppv)
{
    if(IsEqualGUID(riid, &IID_IUnknown)) {
        *ppv = iface;
        return S_OK;
    }

    *ppv = NULL;
    return E_NOINTERFACE;
}

static HRESULT WINAPI NetworkListManagerEvents_QueryInterface(INetworkListManagerEvents *iface,
                                                              REFIID riid, void **ppv)
{
    if(IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_INetworkListManagerEvents)) {
        *ppv = iface;
        return S_OK;
    }

    *ppv = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI NetworkListManagerEvents_AddRef(INetworkListManagerEvents *iface)
{
    return 2;
}

static ULONG WINAPI NetworkListManagerEvents_Release(INetworkListManagerEvents *iface)
{
    return 1;
}

static HRESULT WINAPI NetworkListManagerEvents_ConnectivityChanged(INetworkListManagerEvents *iface,
        NLM_CONNECTIVITY newConnectivity)
{
    ok(0, "unexpected call\n");
    return E_NOTIMPL;
}

static const INetworkListManagerEventsVtbl mgr_sink_vtbl = {
    NetworkListManagerEvents_QueryInterface,
    NetworkListManagerEvents_AddRef,
    NetworkListManagerEvents_Release,
    NetworkListManagerEvents_ConnectivityChanged
};

static INetworkListManagerEvents mgr_sink = { &mgr_sink_vtbl };

static const INetworkListManagerEventsVtbl mgr_sink_unk_vtbl = {
    Unknown_QueryInterface,
    NetworkListManagerEvents_AddRef,
    NetworkListManagerEvents_Release,
    NetworkListManagerEvents_ConnectivityChanged
};

static INetworkListManagerEvents mgr_sink_unk = { &mgr_sink_unk_vtbl };

static void test_INetworkListManager( void )
{
    IConnectionPointContainer *cpc, *cpc2;
    INetworkListManager *mgr;
    INetworkCostManager *cost_mgr;
    NLM_CONNECTIVITY connectivity;
    VARIANT_BOOL connected;
    IConnectionPoint *pt, *pt2;
    IEnumNetworks *network_iter;
    INetwork *network;
    IEnumNetworkConnections *conn_iter;
    INetworkConnection *conn;
    DWORD cookie;
    HRESULT hr;
    ULONG ref1, ref2;
    IID iid;

    hr = CoCreateInstance( &CLSID_NetworkListManager, NULL, CLSCTX_INPROC_SERVER,
                           &IID_INetworkListManager, (void **)&mgr );
    if (hr != S_OK)
    {
        win_skip( "can't create instance of NetworkListManager\n" );
        return;
    }

    connectivity = 0xdeadbeef;
    hr = INetworkListManager_GetConnectivity( mgr, &connectivity );
    ok( hr == S_OK, "got %08x\n", hr );
    ok( connectivity != 0xdeadbeef, "unchanged value\n" );
    trace( "GetConnectivity: %08x\n", connectivity );

    connected = 0xdead;
    hr = INetworkListManager_IsConnected( mgr, &connected );
    ok( hr == S_OK, "got %08x\n", hr );
    ok( connected == VARIANT_TRUE || connected == VARIANT_FALSE, "expected boolean value\n" );

    connected = 0xdead;
    hr = INetworkListManager_IsConnectedToInternet( mgr, &connected );
    ok( hr == S_OK, "got %08x\n", hr );
    ok( connected == VARIANT_TRUE || connected == VARIANT_FALSE, "expected boolean value\n" );

    /* INetworkCostManager is supported starting Win8 */
    cost_mgr = NULL;
    hr = INetworkListManager_QueryInterface( mgr, &IID_INetworkCostManager, (void **)&cost_mgr );
    ok( hr == S_OK || broken(hr == E_NOINTERFACE), "got %08x\n", hr );
    if (cost_mgr)
    {
        DWORD cost;
        NLM_DATAPLAN_STATUS status;

        hr = INetworkCostManager_GetCost( cost_mgr, NULL, NULL );
        ok( hr == E_POINTER, "got %08x\n", hr );

        cost = 0xdeadbeef;
        hr = INetworkCostManager_GetCost( cost_mgr, &cost, NULL );
        ok( hr == S_OK, "got %08x\n", hr );
        ok( cost != 0xdeadbeef, "cost not set\n" );

        hr = INetworkCostManager_GetDataPlanStatus( cost_mgr, NULL, NULL );
        ok( hr == E_POINTER, "got %08x\n", hr );

        hr = INetworkCostManager_GetDataPlanStatus( cost_mgr, &status, NULL );
        ok( hr == S_OK, "got %08x\n", hr );

        INetworkCostManager_Release( cost_mgr );
    }

    hr = INetworkListManager_QueryInterface( mgr, &IID_IConnectionPointContainer, (void**)&cpc );
    ok( hr == S_OK, "got %08x\n", hr );

    ref1 = IConnectionPointContainer_AddRef( cpc );

    hr = IConnectionPointContainer_FindConnectionPoint( cpc, &IID_INetworkListManagerEvents, &pt );
    ok( hr == S_OK, "got %08x\n", hr );

    ref2 = IConnectionPointContainer_AddRef( cpc );
    ok( ref2 == ref1 + 2, "Expected refcount %d, got %d\n", ref1 + 2, ref2 );

    IConnectionPointContainer_Release( cpc );
    IConnectionPointContainer_Release( cpc );

    hr = IConnectionPoint_GetConnectionPointContainer( pt, &cpc2 );
    ok( hr == S_OK, "got %08x\n", hr );
    ok( cpc2 == cpc, "Expected cpc2 == %p, but got %p\n", cpc, cpc2 );
    IConnectionPointContainer_Release( cpc2 );

    memset( &iid, 0, sizeof(iid) );
    hr = IConnectionPoint_GetConnectionInterface( pt, &iid );
    ok( hr == S_OK, "got %08x\n", hr );
    ok( !memcmp( &iid, &IID_INetworkListManagerEvents, sizeof(iid) ),
        "Expected iid to be IID_INetworkListManagerEvents\n" );

    hr = IConnectionPoint_Advise( pt, (IUnknown*)&mgr_sink_unk, &cookie);
    ok( hr == CO_E_FAILEDTOOPENTHREADTOKEN, "Advise failed: %08x\n", hr );

    hr = IConnectionPoint_Advise( pt, (IUnknown*)&mgr_sink, &cookie);
    ok( hr == S_OK, "Advise failed: %08x\n", hr );

    hr = IConnectionPoint_Unadvise( pt, 0xdeadbeef );
    ok( hr == OLE_E_NOCONNECTION || hr == CO_E_FAILEDTOIMPERSONATE, "Unadvise failed: %08x\n", hr );

    hr = IConnectionPoint_Unadvise( pt, cookie );
    ok( hr == S_OK, "Unadvise failed: %08x\n", hr );

    hr = IConnectionPointContainer_FindConnectionPoint( cpc, &IID_INetworkListManagerEvents, &pt2 );
    ok( hr == S_OK, "got %08x\n", hr );
    ok( pt == pt2, "pt != pt2\n");
    IConnectionPoint_Release( pt2 );

    hr = IConnectionPointContainer_FindConnectionPoint( cpc, &IID_INetworkCostManagerEvents, &pt );
    ok( hr == S_OK || hr == CO_E_FAILEDTOIMPERSONATE, "got %08x\n", hr );
    if (hr == S_OK) IConnectionPoint_Release( pt );

    hr = IConnectionPointContainer_FindConnectionPoint( cpc, &IID_INetworkConnectionEvents, &pt );
    ok( hr == S_OK || hr == CO_E_FAILEDTOIMPERSONATE, "got %08x\n", hr );
    if (hr == S_OK) IConnectionPoint_Release( pt );
    IConnectionPointContainer_Release( cpc );

    network_iter = NULL;
    hr = INetworkListManager_GetNetworks( mgr, NLM_ENUM_NETWORK_ALL, &network_iter );
    ok( hr == S_OK, "got %08x\n", hr );
    if (network_iter)
    {
        while ((hr = IEnumNetworks_Next( network_iter, 1, &network, NULL )) == S_OK)
        {
            test_INetwork( network );
            INetwork_Release( network );
        }
        IEnumNetworks_Release( network_iter );
    }

    conn_iter = NULL;
    hr = INetworkListManager_GetNetworkConnections( mgr, &conn_iter );
    ok( hr == S_OK, "got %08x\n", hr );
    if (conn_iter)
    {
        while ((hr = IEnumNetworkConnections_Next( conn_iter, 1, &conn, NULL )) == S_OK)
        {
            test_INetworkConnection( conn );
            INetworkConnection_Release( conn );
        }
        IEnumNetworkConnections_Release( conn_iter );
    }

    /* cps and their container share the same ref count */
    IConnectionPoint_AddRef( pt );
    IConnectionPoint_AddRef( pt );

    ref1 = IConnectionPoint_Release( pt );
    ref2 = INetworkListManager_Release( mgr );
    ok( ref2 == ref1 - 1, "ref = %u\n", ref1 );

    IConnectionPoint_Release( pt );
    ref1 = IConnectionPoint_Release( pt );
    ok( !ref1, "ref = %u\n", ref1 );
}

START_TEST( list )
{
    CoInitialize( NULL );
    test_INetworkListManager();
    CoUninitialize();
}
