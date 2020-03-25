/*
 * Copyright 2019 Hans Leidekker for CodeWeavers
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
#include "windef.h"
#include "winbase.h"
#define COBJMACROS
#include "objbase.h"
#include "msado15_backcompat.h"

#include "wine/debug.h"
#include "wine/heap.h"

#include "msado15_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(msado15);

struct connection
{
    _Connection       Connection_iface;
    ISupportErrorInfo ISupportErrorInfo_iface;
    LONG              refs;
    ObjectStateEnum   state;
    LONG              timeout;
    WCHAR            *datasource;
};

static inline struct connection *impl_from_Connection( _Connection *iface )
{
    return CONTAINING_RECORD( iface, struct connection, Connection_iface );
}

static inline struct connection *impl_from_ISupportErrorInfo( ISupportErrorInfo *iface )
{
    return CONTAINING_RECORD( iface, struct connection, ISupportErrorInfo_iface );
}

static ULONG WINAPI connection_AddRef( _Connection *iface )
{
    struct connection *connection = impl_from_Connection( iface );
    return InterlockedIncrement( &connection->refs );
}

static ULONG WINAPI connection_Release( _Connection *iface )
{
    struct connection *connection = impl_from_Connection( iface );
    LONG refs = InterlockedDecrement( &connection->refs );
    if (!refs)
    {
        TRACE( "destroying %p\n", connection );
        heap_free( connection->datasource );
        heap_free( connection );
    }
    return refs;
}

static HRESULT WINAPI connection_QueryInterface( _Connection *iface, REFIID riid, void **obj )
{
    struct connection *connection = impl_from_Connection( iface );
    TRACE( "%p, %s, %p\n", connection, debugstr_guid(riid), obj );

    if (IsEqualGUID( riid, &IID__Connection ) || IsEqualGUID( riid, &IID_IDispatch ) ||
        IsEqualGUID( riid, &IID_IUnknown ))
    {
        *obj = iface;
    }
    else if(IsEqualGUID( riid, &IID_ISupportErrorInfo ))
    {
        *obj = &connection->ISupportErrorInfo_iface;
    }
    else
    {
        FIXME( "interface %s not implemented\n", debugstr_guid(riid) );
        return E_NOINTERFACE;
    }
    connection_AddRef( iface );
    return S_OK;
}

static HRESULT WINAPI connection_GetTypeInfoCount( _Connection *iface, UINT *count )
{
    FIXME( "%p, %p\n", iface, count );
    return E_NOTIMPL;
}

static HRESULT WINAPI connection_GetTypeInfo( _Connection *iface, UINT index, LCID lcid, ITypeInfo **info )
{
    FIXME( "%p, %u, %u, %p\n", iface, index, lcid, info );
    return E_NOTIMPL;
}

static HRESULT WINAPI connection_GetIDsOfNames( _Connection *iface, REFIID riid, LPOLESTR *names, UINT count,
                                                LCID lcid, DISPID *dispid )
{
    FIXME( "%p, %s, %p, %u, %u, %p\n", iface, debugstr_guid(riid), names, count, lcid, dispid );
    return E_NOTIMPL;
}

static HRESULT WINAPI connection_Invoke( _Connection *iface, DISPID member, REFIID riid, LCID lcid, WORD flags,
                                         DISPPARAMS *params, VARIANT *result, EXCEPINFO *excep_info, UINT *arg_err )
{
    FIXME( "%p, %d, %s, %d, %d, %p, %p, %p, %p\n", iface, member, debugstr_guid(riid), lcid, flags, params,
           result, excep_info, arg_err );
    return E_NOTIMPL;
}

static HRESULT WINAPI connection_get_Properties( _Connection *iface, Properties **obj )
{
    FIXME( "%p, %p\n", iface, obj );
    return E_NOTIMPL;
}

static HRESULT WINAPI connection_get_ConnectionString( _Connection *iface, BSTR *str )
{
    struct connection *connection = impl_from_Connection( iface );
    BSTR source = NULL;

    TRACE( "%p, %p\n", connection, str );

    if (connection->datasource && !(source = SysAllocString( connection->datasource ))) return E_OUTOFMEMORY;
    *str = source;
    return S_OK;
}

static HRESULT WINAPI connection_put_ConnectionString( _Connection *iface, BSTR str )
{
    struct connection *connection = impl_from_Connection( iface );
    WCHAR *source = NULL;

    TRACE( "%p, %s\n", connection, debugstr_w( !wcsstr( str, L"Password" ) ? L"<hidden>" : str ) );

    if (str && !(source = strdupW( str ))) return E_OUTOFMEMORY;
    heap_free( connection->datasource );
    connection->datasource = source;
    return S_OK;
}

static HRESULT WINAPI connection_get_CommandTimeout( _Connection *iface, LONG *timeout )
{
    struct connection *connection = impl_from_Connection( iface );
    TRACE( "%p, %p\n", connection, timeout );
    *timeout = connection->timeout;
    return S_OK;
}

static HRESULT WINAPI connection_put_CommandTimeout( _Connection *iface, LONG timeout )
{
    struct connection *connection = impl_from_Connection( iface );
    TRACE( "%p, %d\n", connection, timeout );
    connection->timeout = timeout;
    return S_OK;
}

static HRESULT WINAPI connection_get_ConnectionTimeout( _Connection *iface, LONG *timeout )
{
    FIXME( "%p, %p\n", iface, timeout );
    return E_NOTIMPL;
}

static HRESULT WINAPI connection_put_ConnectionTimeout( _Connection *iface, LONG timeout )
{
    FIXME( "%p, %d\n", iface, timeout );
    return E_NOTIMPL;
}

static HRESULT WINAPI connection_get_Version( _Connection *iface, BSTR *str )
{
    FIXME( "%p, %p\n", iface, str );
    return E_NOTIMPL;
}

static HRESULT WINAPI connection_Close( _Connection *iface )
{
    FIXME( "%p\n", iface );
    return E_NOTIMPL;
}

static HRESULT WINAPI connection_Execute( _Connection *iface, BSTR command, VARIANT *records_affected,
                                          LONG options, _Recordset **record_set )
{
    FIXME( "%p, %s, %p, %08x, %p\n", iface, debugstr_w(command), records_affected, options, record_set );
    return E_NOTIMPL;
}

static HRESULT WINAPI connection_BeginTrans( _Connection *iface, LONG *transaction_level )
{
    FIXME( "%p, %p\n", iface, transaction_level );
    return E_NOTIMPL;
}

static HRESULT WINAPI connection_CommitTrans( _Connection *iface )
{
    FIXME( "%p\n", iface );
    return E_NOTIMPL;
}

static HRESULT WINAPI connection_RollbackTrans( _Connection *iface )
{
    FIXME( "%p\n", iface );
    return E_NOTIMPL;
}

static HRESULT WINAPI connection_Open( _Connection *iface, BSTR connect_str, BSTR userid, BSTR password,
                                       LONG options )
{
    FIXME( "%p, %s, %s, %p, %08x\n", iface, debugstr_w(connect_str), debugstr_w(userid),
           password, options );
    return E_NOTIMPL;
}

static HRESULT WINAPI connection_get_Errors( _Connection *iface, Errors **obj )
{
    FIXME( "%p, %p\n", iface, obj );
    return E_NOTIMPL;
}

static HRESULT WINAPI connection_get_DefaultDatabase( _Connection *iface, BSTR *str )
{
    FIXME( "%p, %p\n", iface, str );
    return E_NOTIMPL;
}

static HRESULT WINAPI connection_put_DefaultDatabase( _Connection *iface, BSTR str )
{
    FIXME( "%p, %s\n", iface, debugstr_w(str) );
    return E_NOTIMPL;
}

static HRESULT WINAPI connection_get_IsolationLevel( _Connection *iface, IsolationLevelEnum *level )
{
    FIXME( "%p, %p\n", iface, level );
    return E_NOTIMPL;
}

static HRESULT WINAPI connection_put_IsolationLevel( _Connection *iface, IsolationLevelEnum level )
{
    FIXME( "%p, %d\n", iface, level );
    return E_NOTIMPL;
}

static HRESULT WINAPI connection_get_Attributes( _Connection *iface, LONG *attr )
{
    FIXME( "%p, %p\n", iface, attr );
    return E_NOTIMPL;
}

static HRESULT WINAPI connection_put_Attributes( _Connection *iface, LONG attr )
{
    FIXME( "%p, %d\n", iface, attr );
    return E_NOTIMPL;
}

static HRESULT WINAPI connection_get_CursorLocation( _Connection *iface, CursorLocationEnum *cursor_loc )
{
    FIXME( "%p, %p\n", iface, cursor_loc );
    return E_NOTIMPL;
}

static HRESULT WINAPI connection_put_CursorLocation( _Connection *iface, CursorLocationEnum cursor_loc )
{
    FIXME( "%p, %u\n", iface, cursor_loc );
    return E_NOTIMPL;
}

static HRESULT WINAPI connection_get_Mode( _Connection *iface, ConnectModeEnum *mode )
{
    FIXME( "%p, %p\n", iface, mode );
    return E_NOTIMPL;
}

static HRESULT WINAPI connection_put_Mode( _Connection *iface, ConnectModeEnum mode )
{
    FIXME( "%p, %u\n", iface, mode );
    return E_NOTIMPL;
}

static HRESULT WINAPI connection_get_Provider( _Connection *iface, BSTR *str )
{
    FIXME( "%p, %p\n", iface, str );
    return E_NOTIMPL;
}

static HRESULT WINAPI connection_put_Provider( _Connection *iface, BSTR str )
{
    FIXME( "%p, %s\n", iface, debugstr_w(str) );
    return E_NOTIMPL;
}

static HRESULT WINAPI connection_get_State( _Connection *iface, LONG *state )
{
    struct connection *connection = impl_from_Connection( iface );
    TRACE( "%p, %p\n", connection, state );
    *state = connection->state;
    return S_OK;
}

static HRESULT WINAPI connection_OpenSchema( _Connection *iface, SchemaEnum schema, VARIANT restrictions,
                                             VARIANT schema_id, _Recordset **record_set )
{
    FIXME( "%p, %d, %s, %s, %p\n", iface, schema, debugstr_variant(&restrictions),
           debugstr_variant(&schema_id), record_set );
    return E_NOTIMPL;
}

static HRESULT WINAPI connection_Cancel( _Connection *iface )
{
    FIXME( "%p\n", iface );
    return E_NOTIMPL;
}

static const struct _ConnectionVtbl connection_vtbl =
{
    connection_QueryInterface,
    connection_AddRef,
    connection_Release,
    connection_GetTypeInfoCount,
    connection_GetTypeInfo,
    connection_GetIDsOfNames,
    connection_Invoke,
    connection_get_Properties,
    connection_get_ConnectionString,
    connection_put_ConnectionString,
    connection_get_CommandTimeout,
    connection_put_CommandTimeout,
    connection_get_ConnectionTimeout,
    connection_put_ConnectionTimeout,
    connection_get_Version,
    connection_Close,
    connection_Execute,
    connection_BeginTrans,
    connection_CommitTrans,
    connection_RollbackTrans,
    connection_Open,
    connection_get_Errors,
    connection_get_DefaultDatabase,
    connection_put_DefaultDatabase,
    connection_get_IsolationLevel,
    connection_put_IsolationLevel,
    connection_get_Attributes,
    connection_put_Attributes,
    connection_get_CursorLocation,
    connection_put_CursorLocation,
    connection_get_Mode,
    connection_put_Mode,
    connection_get_Provider,
    connection_put_Provider,
    connection_get_State,
    connection_OpenSchema,
    connection_Cancel
};

static HRESULT WINAPI supporterror_QueryInterface( ISupportErrorInfo *iface, REFIID riid, void **obj )
{
    struct connection *connection = impl_from_ISupportErrorInfo( iface );
    return connection_QueryInterface( &connection->Connection_iface, riid, obj );
}

static ULONG WINAPI supporterror_AddRef( ISupportErrorInfo *iface )
{
    struct connection *connection = impl_from_ISupportErrorInfo( iface );
    return connection_AddRef( &connection->Connection_iface );
}

static ULONG WINAPI supporterror_Release( ISupportErrorInfo *iface )
{
    struct connection *connection = impl_from_ISupportErrorInfo( iface );
    return connection_Release( &connection->Connection_iface );
}

static HRESULT WINAPI supporterror_InterfaceSupportsErrorInfo( ISupportErrorInfo *iface, REFIID riid )
{
    struct connection *connection = impl_from_ISupportErrorInfo( iface );
    FIXME( "%p, %s\n", connection, debugstr_guid(riid) );
    return S_FALSE;
}

static const struct ISupportErrorInfoVtbl support_error_vtbl =
{
    supporterror_QueryInterface,
    supporterror_AddRef,
    supporterror_Release,
    supporterror_InterfaceSupportsErrorInfo
};

HRESULT Connection_create( void **obj )
{
    struct connection *connection;

    if (!(connection = heap_alloc( sizeof(*connection) ))) return E_OUTOFMEMORY;
    connection->Connection_iface.lpVtbl = &connection_vtbl;
    connection->ISupportErrorInfo_iface.lpVtbl = &support_error_vtbl;
    connection->refs = 1;
    connection->state = adStateClosed;
    connection->timeout = 30;
    connection->datasource = NULL;

    *obj = &connection->Connection_iface;
    TRACE( "returning iface %p\n", *obj );
    return S_OK;
}
