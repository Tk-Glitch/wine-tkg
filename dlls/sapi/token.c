/*
 * Speech API (SAPI) token implementation.
 *
 * Copyright (C) 2017 Huw Davies
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
#include "objbase.h"
#include "sapiddk.h"
#include "sperror.h"

#include "wine/debug.h"

#include "sapi_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(sapi);

struct data_key
{
    ISpRegDataKey ISpRegDataKey_iface;
    LONG ref;

    HKEY key;
    BOOL read_only;
};

static struct data_key *impl_from_ISpRegDataKey( ISpRegDataKey *iface )
{
    return CONTAINING_RECORD( iface, struct data_key, ISpRegDataKey_iface );
}

struct object_token
{
    ISpObjectToken ISpObjectToken_iface;
    LONG ref;

    HKEY token_key;
    WCHAR *token_id;
};

static struct object_token *impl_from_ISpObjectToken( ISpObjectToken *iface )
{
    return CONTAINING_RECORD( iface, struct object_token, ISpObjectToken_iface );
}

static HRESULT WINAPI data_key_QueryInterface( ISpRegDataKey *iface, REFIID iid, void **obj )
{
    struct data_key *This = impl_from_ISpRegDataKey( iface );

    TRACE( "(%p)->(%s %p)\n", This, debugstr_guid( iid ), obj );

    if (IsEqualIID( iid, &IID_IUnknown ) ||
        IsEqualIID( iid, &IID_ISpDataKey ) ||
        IsEqualIID( iid, &IID_ISpRegDataKey ))
    {
        ISpRegDataKey_AddRef( iface );
        *obj = iface;
        return S_OK;
    }

    FIXME( "interface %s not implemented\n", debugstr_guid( iid ) );
    *obj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI data_key_AddRef( ISpRegDataKey *iface )
{
    struct data_key *This = impl_from_ISpRegDataKey( iface );
    ULONG ref = InterlockedIncrement( &This->ref );

    TRACE( "(%p) ref = %lu\n", This, ref );
    return ref;
}

static ULONG WINAPI data_key_Release( ISpRegDataKey *iface )
{
    struct data_key *This = impl_from_ISpRegDataKey( iface );
    ULONG ref = InterlockedDecrement(&This->ref);

    TRACE( "(%p) ref = %lu\n", This, ref );

    if (!ref)
    {
        if (This->key) RegCloseKey( This->key );
        free( This );
    }

    return ref;
}

static HRESULT WINAPI data_key_SetData( ISpRegDataKey *iface, LPCWSTR name,
                                        ULONG size, const BYTE *data )
{
    FIXME( "stub\n" );
    return E_NOTIMPL;
}

static HRESULT WINAPI data_key_GetData( ISpRegDataKey *iface, LPCWSTR name,
                                        ULONG *size, BYTE *data )
{
    FIXME( "stub\n" );
    return E_NOTIMPL;
}

static HRESULT WINAPI data_key_SetStringValue( ISpRegDataKey *iface,
                                               LPCWSTR name, LPCWSTR value )
{
    FIXME( "stub\n" );
    return E_NOTIMPL;
}

static HRESULT WINAPI data_key_GetStringValue( ISpRegDataKey *iface,
                                               LPCWSTR name, LPWSTR *value )
{
    struct data_key *This = impl_from_ISpRegDataKey( iface );
    DWORD ret, size;
    WCHAR *content;

    TRACE( "%p, %s, %p\n", This, debugstr_w(name), value);

    if (!This->key)
        return E_HANDLE;

    size = 0;
    ret = RegGetValueW( This->key, NULL, name, RRF_RT_REG_SZ, NULL, NULL, &size );
    if (ret != ERROR_SUCCESS)
        return SPERR_NOT_FOUND;

    content = CoTaskMemAlloc(size);
    if (!content)
        return E_OUTOFMEMORY;

    ret = RegGetValueW( This->key, NULL, name, RRF_RT_REG_SZ, NULL, content, &size );
    if (ret != ERROR_SUCCESS)
    {
        CoTaskMemFree(content);
        return HRESULT_FROM_WIN32(ret);
    }

    *value = content;
    return S_OK;
}

static HRESULT WINAPI data_key_SetDWORD( ISpRegDataKey *iface,
                                         LPCWSTR name, DWORD value )
{
    FIXME( "stub\n" );
    return E_NOTIMPL;
}

static HRESULT WINAPI data_key_GetDWORD( ISpRegDataKey *iface,
                                         LPCWSTR name, DWORD *pdwValue )
{
    FIXME( "stub\n" );
    return E_NOTIMPL;
}

static HRESULT WINAPI data_key_OpenKey( ISpRegDataKey *iface,
                                        LPCWSTR name, ISpDataKey **sub_key )
{
    FIXME( "stub\n" );
    return E_NOTIMPL;
}

static HRESULT WINAPI data_key_CreateKey( ISpRegDataKey *iface,
                                          LPCWSTR name, ISpDataKey **sub_key )
{
    struct data_key *This = impl_from_ISpRegDataKey( iface );
    ISpRegDataKey *spregkey;
    HRESULT hr;
    HKEY key;
    LONG res;

    TRACE( "%p, %s, %p\n", This, debugstr_w(name), sub_key );

    res = RegCreateKeyExW( This->key, name, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL );
    if (res != ERROR_SUCCESS)
        return HRESULT_FROM_WIN32(res);

    hr = data_key_create(NULL, &IID_ISpRegDataKey, (void**)&spregkey);
    if (SUCCEEDED(hr))
    {
        hr = ISpRegDataKey_SetKey(spregkey, key, FALSE);
        if (SUCCEEDED(hr))
            hr = ISpRegDataKey_QueryInterface(spregkey, &IID_ISpDataKey, (void**)sub_key);
        ISpRegDataKey_Release(spregkey);
    }

    return hr;
}

static HRESULT WINAPI data_key_DeleteKey( ISpRegDataKey *iface, LPCWSTR name )
{
    FIXME( "stub\n" );
    return E_NOTIMPL;
}

static HRESULT WINAPI data_key_DeleteValue( ISpRegDataKey *iface, LPCWSTR name )
{
    FIXME( "stub\n" );
    return E_NOTIMPL;
}

static HRESULT WINAPI data_key_EnumKeys( ISpRegDataKey *iface,
                                         ULONG index, LPWSTR *sub_key )
{
    FIXME( "stub\n" );
    return E_NOTIMPL;
}

static HRESULT WINAPI data_key_EnumValues( ISpRegDataKey *iface,
                                           ULONG index, LPWSTR *value )
{
    FIXME( "stub\n" );
    return E_NOTIMPL;
}

static HRESULT WINAPI data_key_SetKey( ISpRegDataKey *iface,
                                       HKEY key, BOOL read_only )
{
    struct data_key *This = impl_from_ISpRegDataKey( iface );

    TRACE( "(%p)->(%p %d)\n", This, key, read_only );

    if (This->key) return SPERR_ALREADY_INITIALIZED;

    This->key = key;
    This->read_only = read_only;
    return S_OK;
}

const struct ISpRegDataKeyVtbl data_key_vtbl =
{
    data_key_QueryInterface,
    data_key_AddRef,
    data_key_Release,
    data_key_SetData,
    data_key_GetData,
    data_key_SetStringValue,
    data_key_GetStringValue,
    data_key_SetDWORD,
    data_key_GetDWORD,
    data_key_OpenKey,
    data_key_CreateKey,
    data_key_DeleteKey,
    data_key_DeleteValue,
    data_key_EnumKeys,
    data_key_EnumValues,
    data_key_SetKey
};

HRESULT data_key_create( IUnknown *outer, REFIID iid, void **obj )
{
    struct data_key *This = malloc( sizeof(*This) );
    HRESULT hr;

    if (!This) return E_OUTOFMEMORY;
    This->ISpRegDataKey_iface.lpVtbl = &data_key_vtbl;
    This->ref = 1;
    This->key = NULL;
    This->read_only = FALSE;

    hr = ISpRegDataKey_QueryInterface( &This->ISpRegDataKey_iface, iid, obj );

    ISpRegDataKey_Release( &This->ISpRegDataKey_iface );
    return hr;
}

struct token_category
{
    ISpObjectTokenCategory ISpObjectTokenCategory_iface;
    LONG ref;

    ISpRegDataKey *data_key;
};

static struct token_category *impl_from_ISpObjectTokenCategory( ISpObjectTokenCategory *iface )
{
    return CONTAINING_RECORD( iface, struct token_category, ISpObjectTokenCategory_iface );
}

static HRESULT WINAPI token_category_QueryInterface( ISpObjectTokenCategory *iface,
                                                     REFIID iid, void **obj )
{
    struct token_category *This = impl_from_ISpObjectTokenCategory( iface );

    TRACE( "(%p)->(%s %p)\n", This, debugstr_guid( iid ), obj );

    if (IsEqualIID( iid, &IID_IUnknown ) ||
        IsEqualIID( iid, &IID_ISpDataKey ) ||
        IsEqualIID( iid, &IID_ISpObjectTokenCategory ))
    {
        ISpObjectTokenCategory_AddRef( iface );
        *obj = iface;
        return S_OK;
    }

    FIXME( "interface %s not implemented\n", debugstr_guid( iid ) );
    *obj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI token_category_AddRef( ISpObjectTokenCategory *iface )
{
    struct token_category *This = impl_from_ISpObjectTokenCategory( iface );
    ULONG ref = InterlockedIncrement( &This->ref );

    TRACE( "(%p) ref = %lu\n", This, ref );
    return ref;
}

static ULONG WINAPI token_category_Release( ISpObjectTokenCategory *iface )
{
    struct token_category *This = impl_from_ISpObjectTokenCategory( iface );
    ULONG ref = InterlockedDecrement(&This->ref);

    TRACE( "(%p) ref = %lu\n", This, ref );

    if (!ref)
    {
        if (This->data_key) ISpRegDataKey_Release( This->data_key );
        free( This );
    }
    return ref;
}

static HRESULT WINAPI token_category_SetData( ISpObjectTokenCategory *iface,
                                              LPCWSTR name, ULONG size,
                                              const BYTE *data )
{
    FIXME( "stub\n" );
    return E_NOTIMPL;
}

static HRESULT WINAPI token_category_GetData( ISpObjectTokenCategory *iface,
                                              LPCWSTR name, ULONG *size,
                                              BYTE *data )
{
    FIXME( "stub\n" );
    return E_NOTIMPL;
}

static HRESULT WINAPI token_category_SetStringValue( ISpObjectTokenCategory *iface,
                                                     LPCWSTR name, LPCWSTR value )
{
    FIXME( "stub\n" );
    return E_NOTIMPL;
}

static HRESULT WINAPI token_category_GetStringValue( ISpObjectTokenCategory *iface,
                                                     LPCWSTR name, LPWSTR *value )
{
    FIXME( "stub\n" );
    return E_NOTIMPL;
}

static HRESULT WINAPI token_category_SetDWORD( ISpObjectTokenCategory *iface,
                                               LPCWSTR name, DWORD value )
{
    FIXME( "stub\n" );
    return E_NOTIMPL;
}

static HRESULT WINAPI token_category_GetDWORD( ISpObjectTokenCategory *iface,
                                               LPCWSTR name, DWORD *pdwValue )
{
    FIXME( "stub\n" );
    return E_NOTIMPL;
}

static HRESULT WINAPI token_category_OpenKey( ISpObjectTokenCategory *iface,
                                              LPCWSTR name, ISpDataKey **sub_key )
{
    FIXME( "stub\n" );
    return E_NOTIMPL;
}

static HRESULT WINAPI token_category_CreateKey( ISpObjectTokenCategory *iface,
                                                LPCWSTR name, ISpDataKey **sub_key )
{
    FIXME( "stub\n" );
    return E_NOTIMPL;
}

static HRESULT WINAPI token_category_DeleteKey( ISpObjectTokenCategory *iface,
                                                LPCWSTR name )
{
    FIXME( "stub\n" );
    return E_NOTIMPL;
}

static HRESULT WINAPI token_category_DeleteValue( ISpObjectTokenCategory *iface,
                                                  LPCWSTR name )
{
    FIXME( "stub\n" );
    return E_NOTIMPL;
}

static HRESULT WINAPI token_category_EnumKeys( ISpObjectTokenCategory *iface,
                                               ULONG index, LPWSTR *sub_key )
{
    FIXME( "stub\n" );
    return E_NOTIMPL;
}

static HRESULT WINAPI token_category_EnumValues( ISpObjectTokenCategory *iface,
                                                 ULONG index, LPWSTR *value )
{
    FIXME( "stub\n" );
    return E_NOTIMPL;
}

static HRESULT parse_cat_id( const WCHAR *str, HKEY *root, const WCHAR **sub_key )
{
    struct table
    {
        const WCHAR *name;
        unsigned int len;
        HKEY key;
    } table[] =
    {
#define X(s)  s, ARRAY_SIZE(s) - 1
        { X(L"HKEY_LOCAL_MACHINE\\"), HKEY_LOCAL_MACHINE },
        { X(L"HKEY_CURRENT_USER\\"), HKEY_CURRENT_USER },
        { NULL }
#undef X
    };
    struct table *ptr;
    int len = lstrlenW( str );

    for (ptr = table; ptr->name; ptr++)
    {
        if (len >= ptr->len && !wcsncmp( str, ptr->name, ptr->len ))
        {
            *root = ptr->key;
            *sub_key = str + ptr->len;
            return S_OK;
        }
    }
    return S_FALSE;
}

static HRESULT WINAPI token_category_SetId( ISpObjectTokenCategory *iface,
                                            LPCWSTR id, BOOL create )
{
    struct token_category *This = impl_from_ISpObjectTokenCategory( iface );
    HKEY root, key;
    const WCHAR *subkey;
    LONG res;
    HRESULT hr;

    TRACE( "(%p)->(%s %d)\n", This, debugstr_w( id ), create );

    if (This->data_key) return SPERR_ALREADY_INITIALIZED;

    hr = parse_cat_id( id, &root, &subkey );
    if (hr != S_OK) return SPERR_INVALID_REGISTRY_KEY;

    if (create)
        res = RegCreateKeyExW( root, subkey, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL );
    else
        res = RegOpenKeyExW( root, subkey, 0, KEY_ALL_ACCESS, &key );
    if (res) return SPERR_INVALID_REGISTRY_KEY;

    hr = CoCreateInstance( &CLSID_SpDataKey, NULL, CLSCTX_ALL,
                           &IID_ISpRegDataKey, (void **)&This->data_key );
    if (FAILED(hr)) goto fail;

    hr = ISpRegDataKey_SetKey( This->data_key, key, FALSE );
    if (FAILED(hr)) goto fail;

    return hr;

fail:
    RegCloseKey( key );
    return hr;
}

static HRESULT WINAPI token_category_GetId( ISpObjectTokenCategory *iface,
                                            LPWSTR *id )
{
    FIXME( "stub\n" );
    return E_NOTIMPL;
}

static HRESULT WINAPI token_category_GetDataKey( ISpObjectTokenCategory *iface,
                                                 SPDATAKEYLOCATION location,
                                                 ISpDataKey **data_key )
{
    FIXME( "stub\n" );
    return E_NOTIMPL;
}

struct token_enum
{
    ISpObjectTokenEnumBuilder ISpObjectTokenEnumBuilder_iface;
    LONG ref;

    BOOL init;
    WCHAR *req, *opt;
    ULONG count;
    HKEY key;
    DWORD index;
};

static struct token_enum *impl_from_ISpObjectTokenEnumBuilder( ISpObjectTokenEnumBuilder *iface )
{
    return CONTAINING_RECORD( iface, struct token_enum, ISpObjectTokenEnumBuilder_iface );
}

static HRESULT WINAPI token_category_EnumTokens( ISpObjectTokenCategory *iface,
                                                 LPCWSTR req, LPCWSTR opt,
                                                 IEnumSpObjectTokens **enum_tokens )
{
    struct token_category *This = impl_from_ISpObjectTokenCategory( iface );
    ISpObjectTokenEnumBuilder *builder;
    struct token_enum *tokenenum;
    struct data_key *this_data_key;
    HRESULT hr;

    TRACE( "(%p)->(%s %s %p)\n", This, debugstr_w( req ), debugstr_w( opt ), enum_tokens );

    if (!This->data_key) return SPERR_UNINITIALIZED;

    hr = CoCreateInstance( &CLSID_SpObjectTokenEnum, NULL, CLSCTX_ALL,
                           &IID_ISpObjectTokenEnumBuilder, (void **)&builder );
    if (FAILED(hr)) return hr;

    hr = ISpObjectTokenEnumBuilder_SetAttribs( builder, req, opt );
    if (FAILED(hr)) goto fail;

    this_data_key = impl_from_ISpRegDataKey( This->data_key );

    tokenenum = impl_from_ISpObjectTokenEnumBuilder( builder );

    if (!RegOpenKeyExW( this_data_key->key, L"Tokens", 0, KEY_ALL_ACCESS, &tokenenum->key ))
    {
        RegQueryInfoKeyW(tokenenum->key, NULL, NULL, NULL, &tokenenum->count, NULL, NULL,
                NULL, NULL, NULL, NULL, NULL);
    }

    hr = ISpObjectTokenEnumBuilder_QueryInterface( builder, &IID_IEnumSpObjectTokens,
                                                   (void **)enum_tokens );

fail:
    ISpObjectTokenEnumBuilder_Release( builder );
    return hr;
}

static HRESULT WINAPI token_category_SetDefaultTokenId( ISpObjectTokenCategory *iface,
                                                        LPCWSTR id )
{
    FIXME( "stub\n" );
    return E_NOTIMPL;
}

static HRESULT WINAPI token_category_GetDefaultTokenId( ISpObjectTokenCategory *iface,
                                                        LPWSTR *id )
{
    struct token_category *This = impl_from_ISpObjectTokenCategory( iface );
    struct data_key *this_data_key;
    LONG res;
    WCHAR regvalue[512];
    DWORD regvalue_size = sizeof( regvalue );

    FIXME( "(%p)->(%p): semi-stub\n", iface, id );

    if (!This->data_key)
        return SPERR_UNINITIALIZED;

    if (!id)
        return E_POINTER;

    /* todo: check HKCU's DefaultTokenId before */

    this_data_key = impl_from_ISpRegDataKey( This->data_key );

    res = RegGetValueW( this_data_key->key, NULL, L"DefaultDefaultTokenId", RRF_RT_REG_SZ,
                        NULL, &regvalue, &regvalue_size);
    if (res == ERROR_FILE_NOT_FOUND) {
        return SPERR_NOT_FOUND;
    } else if (res != ERROR_SUCCESS) {
        /* probably not the correct return value */
        FIXME( "returning %08lx\n", res );
        return res;
    }

    *id = CoTaskMemAlloc( regvalue_size );
    wcscpy( *id, regvalue );

    return S_OK;
}

const struct ISpObjectTokenCategoryVtbl token_category_vtbl =
{
    token_category_QueryInterface,
    token_category_AddRef,
    token_category_Release,
    token_category_SetData,
    token_category_GetData,
    token_category_SetStringValue,
    token_category_GetStringValue,
    token_category_SetDWORD,
    token_category_GetDWORD,
    token_category_OpenKey,
    token_category_CreateKey,
    token_category_DeleteKey,
    token_category_DeleteValue,
    token_category_EnumKeys,
    token_category_EnumValues,
    token_category_SetId,
    token_category_GetId,
    token_category_GetDataKey,
    token_category_EnumTokens,
    token_category_SetDefaultTokenId,
    token_category_GetDefaultTokenId,
};

HRESULT token_category_create( IUnknown *outer, REFIID iid, void **obj )
{
    struct token_category *This = malloc( sizeof(*This) );
    HRESULT hr;

    if (!This) return E_OUTOFMEMORY;
    This->ISpObjectTokenCategory_iface.lpVtbl = &token_category_vtbl;
    This->ref = 1;
    This->data_key = NULL;

    hr = ISpObjectTokenCategory_QueryInterface( &This->ISpObjectTokenCategory_iface, iid, obj );

    ISpObjectTokenCategory_Release( &This->ISpObjectTokenCategory_iface );
    return hr;
}

static HRESULT WINAPI token_enum_QueryInterface( ISpObjectTokenEnumBuilder *iface,
                                                 REFIID iid, void **obj )
{
    struct token_enum *This = impl_from_ISpObjectTokenEnumBuilder( iface );

    TRACE( "(%p)->(%s %p)\n", This, debugstr_guid( iid ), obj );

    if (IsEqualIID( iid, &IID_IUnknown ) ||
        IsEqualIID( iid, &IID_IEnumSpObjectTokens ) ||
        IsEqualIID( iid, &IID_ISpObjectTokenEnumBuilder ))
    {
        ISpObjectTokenEnumBuilder_AddRef( iface );
        *obj = iface;
        return S_OK;
    }

    FIXME( "interface %s not implemented\n", debugstr_guid( iid ) );
    *obj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI token_enum_AddRef( ISpObjectTokenEnumBuilder *iface )
{
    struct token_enum *This = impl_from_ISpObjectTokenEnumBuilder( iface );
    ULONG ref = InterlockedIncrement( &This->ref );

    TRACE( "(%p) ref = %lu\n", This, ref );
    return ref;
}

static ULONG WINAPI token_enum_Release( ISpObjectTokenEnumBuilder *iface )
{
    struct token_enum *This = impl_from_ISpObjectTokenEnumBuilder( iface );
    ULONG ref = InterlockedDecrement(&This->ref);

    TRACE( "(%p) ref = %lu\n", This, ref );

    if (!ref)
    {
        if (This->key)
            RegCloseKey(This->key);
        free( This->req );
        free( This->opt );
        free( This );
    }

    return ref;
}

static HRESULT WINAPI token_enum_Next( ISpObjectTokenEnumBuilder *iface,
                                       ULONG num, ISpObjectToken **tokens,
                                       ULONG *fetched )
{
    struct token_enum *This = impl_from_ISpObjectTokenEnumBuilder( iface );
    struct object_token *object;
    HRESULT hr;
    DWORD retCode;
    WCHAR *subkey_name;
    HKEY sub_key;
    DWORD size;

    TRACE( "(%p)->(%lu %p %p)\n", This, num, tokens, fetched );

    if (!This->init) return SPERR_UNINITIALIZED;
    if (fetched) *fetched = 0;

    *tokens = NULL;

    RegQueryInfoKeyW( This->key, NULL, NULL, NULL, NULL, &size, NULL, NULL, NULL, NULL, NULL, NULL );
    size = (size+1) * sizeof(WCHAR);
    subkey_name = malloc( size );
    if (!subkey_name)
        return E_OUTOFMEMORY;

    retCode = RegEnumKeyExW( This->key, This->index, subkey_name, &size, NULL, NULL, NULL, NULL );
    if (retCode != ERROR_SUCCESS)
    {
        free( subkey_name );
        return S_FALSE;
    }

    This->index++;

    if (RegOpenKeyExW( This->key, subkey_name, 0, KEY_READ, &sub_key ) != ERROR_SUCCESS)
    {
        free( subkey_name );
        return E_FAIL;
    }

    hr = token_create( NULL, &IID_ISpObjectToken, (void**)tokens );
    if (FAILED(hr))
    {
        free( subkey_name );
        return hr;
    }

    object = impl_from_ISpObjectToken( *tokens );
    object->token_key = sub_key;
    object->token_id = subkey_name;

    if (fetched) *fetched = 1;
    return hr;
}

static HRESULT WINAPI token_enum_Skip( ISpObjectTokenEnumBuilder *iface,
                                       ULONG num )
{
    FIXME( "stub\n" );
    return E_NOTIMPL;
}

static HRESULT WINAPI token_enum_Reset( ISpObjectTokenEnumBuilder *iface)
{
    FIXME( "stub\n" );
    return E_NOTIMPL;
}

static HRESULT WINAPI token_enum_Clone( ISpObjectTokenEnumBuilder *iface,
                                        IEnumSpObjectTokens **clone )
{
    FIXME( "stub\n" );
    return E_NOTIMPL;
}

static HRESULT WINAPI token_enum_Item( ISpObjectTokenEnumBuilder *iface,
                                       ULONG index, ISpObjectToken **token )
{
    struct token_enum *This = impl_from_ISpObjectTokenEnumBuilder( iface );
    struct object_token *object;
    ISpObjectToken *subtoken;
    HRESULT hr;
    WCHAR *subkey;
    DWORD size;
    LONG ret;
    HKEY key;

    TRACE( "%p, %lu, %p\n", This, index, token );

    if (!This->init)
        return SPERR_UNINITIALIZED;

    RegQueryInfoKeyW(This->key, NULL, NULL, NULL, NULL, &size, NULL, NULL, NULL, NULL, NULL, NULL);
    size = (size+1) * sizeof(WCHAR);
    subkey = malloc( size );
    if (!subkey)
        return E_OUTOFMEMORY;

    ret = RegEnumKeyExW(This->key, index, subkey, &size, NULL, NULL, NULL, NULL);
    if (ret != ERROR_SUCCESS)
    {
        free( subkey );
        return HRESULT_FROM_WIN32(ret);
    }

    ret = RegOpenKeyExW (This->key, subkey, 0, KEY_READ, &key);
    if (ret != ERROR_SUCCESS)
    {
        free( subkey );
        return HRESULT_FROM_WIN32(ret);
    }

    hr = token_create( NULL, &IID_ISpObjectToken, (void**)&subtoken );
    if (FAILED(hr))
    {
        free( subkey );
        return hr;
    }

    object = impl_from_ISpObjectToken( subtoken );
    object->token_key = key;
    object->token_id = subkey;

    *token = subtoken;

    return hr;
}

static HRESULT WINAPI token_enum_GetCount( ISpObjectTokenEnumBuilder *iface,
                                           ULONG *count )
{
    struct token_enum *This = impl_from_ISpObjectTokenEnumBuilder( iface );

    TRACE( "(%p)->(%p)\n", This, count );

    if (!This->init) return SPERR_UNINITIALIZED;

    *count = This->count;
    return S_OK;
}

static HRESULT WINAPI token_enum_SetAttribs( ISpObjectTokenEnumBuilder *iface,
                                             LPCWSTR req, LPCWSTR opt)
{
    struct token_enum *This = impl_from_ISpObjectTokenEnumBuilder( iface );

    TRACE( "(%p)->(%s %s)\n", This, debugstr_w( req ), debugstr_w( opt ) );

    if (This->init) return SPERR_ALREADY_INITIALIZED;

    if (req)
    {
        This->req = wcsdup( req );
        if (!This->req) goto out_of_mem;
    }

    if (opt)
    {
        This->opt = wcsdup( opt );
        if (!This->opt) goto out_of_mem;
    }

    This->init = TRUE;
    return S_OK;

out_of_mem:
    free( This->req );
    return E_OUTOFMEMORY;
}

static HRESULT WINAPI token_enum_AddTokens( ISpObjectTokenEnumBuilder *iface,
                                            ULONG num, ISpObjectToken **tokens )
{
    FIXME( "stub\n" );
    return E_NOTIMPL;
}

static HRESULT WINAPI token_enum_AddTokensFromDataKey( ISpObjectTokenEnumBuilder *iface,
                                                       ISpDataKey *data_key,
                                                       LPCWSTR sub_key, LPCWSTR cat_id )
{
    FIXME( "stub\n" );
    return E_NOTIMPL;
}

static HRESULT WINAPI token_enum_AddTokensFromTokenEnum( ISpObjectTokenEnumBuilder *iface,
                                                         IEnumSpObjectTokens *token_enum )
{
    FIXME( "stub\n" );
    return E_NOTIMPL;
}

static HRESULT WINAPI token_enum_Sort( ISpObjectTokenEnumBuilder *iface,
                                       LPCWSTR first )
{
    FIXME( "stub\n" );
    return E_NOTIMPL;
}

const struct ISpObjectTokenEnumBuilderVtbl token_enum_vtbl =
{
    token_enum_QueryInterface,
    token_enum_AddRef,
    token_enum_Release,
    token_enum_Next,
    token_enum_Skip,
    token_enum_Reset,
    token_enum_Clone,
    token_enum_Item,
    token_enum_GetCount,
    token_enum_SetAttribs,
    token_enum_AddTokens,
    token_enum_AddTokensFromDataKey,
    token_enum_AddTokensFromTokenEnum,
    token_enum_Sort
};

HRESULT token_enum_create( IUnknown *outer, REFIID iid, void **obj )
{
    struct token_enum *This = malloc( sizeof(*This) );
    HRESULT hr;

    if (!This) return E_OUTOFMEMORY;
    This->ISpObjectTokenEnumBuilder_iface.lpVtbl = &token_enum_vtbl;
    This->ref = 1;
    This->req = NULL;
    This->opt = NULL;
    This->init = FALSE;
    This->count = 0;
    This->key = NULL;
    This->index = 0;

    hr = ISpObjectTokenEnumBuilder_QueryInterface( &This->ISpObjectTokenEnumBuilder_iface, iid, obj );

    ISpObjectTokenEnumBuilder_Release( &This->ISpObjectTokenEnumBuilder_iface );
    return hr;
}

static HRESULT WINAPI token_QueryInterface( ISpObjectToken *iface,
                                            REFIID iid, void **obj )
{
    struct object_token *This = impl_from_ISpObjectToken( iface );

    TRACE( "(%p)->(%s %p)\n", This, debugstr_guid( iid ), obj );

    if (IsEqualIID( iid, &IID_IUnknown ) ||
        IsEqualIID( iid, &IID_ISpDataKey ) ||
        IsEqualIID( iid, &IID_ISpObjectToken ))
    {
        ISpObjectToken_AddRef( iface );
        *obj = iface;
        return S_OK;
    }

    FIXME( "interface %s not implemented\n", debugstr_guid( iid ) );
    *obj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI token_AddRef( ISpObjectToken *iface )
{
    struct object_token *This = impl_from_ISpObjectToken( iface );
    ULONG ref = InterlockedIncrement( &This->ref );

    TRACE( "(%p) ref = %lu\n", This, ref );
    return ref;
}

static ULONG WINAPI token_Release( ISpObjectToken *iface )
{
    struct object_token *This = impl_from_ISpObjectToken( iface );
    ULONG ref = InterlockedDecrement(&This->ref);

    TRACE( "(%p) ref = %lu\n", This, ref );

    if (!ref)
    {
        if (This->token_key) RegCloseKey( This->token_key );
        free( This->token_id );
        free( This );
    }

    return ref;
}

static HRESULT WINAPI token_SetData( ISpObjectToken *iface,
                                     LPCWSTR name, ULONG size,
                                     const BYTE *data )
{
    FIXME( "stub\n" );
    return E_NOTIMPL;
}

static HRESULT WINAPI token_GetData( ISpObjectToken *iface,
                                     LPCWSTR name, ULONG *size,
                                     BYTE *data )
{
    FIXME( "stub\n" );
    return E_NOTIMPL;
}

static HRESULT WINAPI token_SetStringValue( ISpObjectToken *iface,
                                            LPCWSTR name, LPCWSTR value )
{
    FIXME( "stub\n" );
    return E_NOTIMPL;
}

static HRESULT WINAPI token_GetStringValue( ISpObjectToken *iface,
                                            LPCWSTR name, LPWSTR *value )
{
    FIXME( "stub\n" );
    return E_NOTIMPL;
}

static HRESULT WINAPI token_SetDWORD( ISpObjectToken *iface,
                                      LPCWSTR name, DWORD value )
{
    FIXME( "stub\n" );
    return E_NOTIMPL;
}

static HRESULT WINAPI token_GetDWORD( ISpObjectToken *iface,
                                      LPCWSTR name, DWORD *pdwValue )
{
    FIXME( "stub\n" );
    return E_NOTIMPL;
}

static HRESULT WINAPI token_OpenKey( ISpObjectToken *iface,
                                     LPCWSTR name, ISpDataKey **sub_key )
{
    struct object_token *This = impl_from_ISpObjectToken( iface );
    ISpRegDataKey *spregkey;
    HRESULT hr;
    HKEY key;
    LONG ret;

    TRACE( "%p, %s, %p\n", This, debugstr_w(name), sub_key );

    ret = RegOpenKeyExW (This->token_key, name, 0, KEY_ALL_ACCESS, &key);
    if (ret != ERROR_SUCCESS)
        return SPERR_NOT_FOUND;

    hr = data_key_create(NULL, &IID_ISpRegDataKey, (void**)&spregkey);
    if (FAILED(hr))
    {
        RegCloseKey(key);
        return hr;
    }

    hr = ISpRegDataKey_SetKey(spregkey, key, FALSE);
    if (FAILED(hr))
    {
        RegCloseKey(key);
        ISpRegDataKey_Release(spregkey);
        return hr;
    }

    hr = ISpRegDataKey_QueryInterface(spregkey, &IID_ISpDataKey, (void**)sub_key);
    ISpRegDataKey_Release(spregkey);

    return hr;
}

static HRESULT WINAPI token_CreateKey( ISpObjectToken *iface,
                                       LPCWSTR name, ISpDataKey **sub_key )
{
    FIXME( "stub\n" );
    return E_NOTIMPL;
}

static HRESULT WINAPI token_DeleteKey( ISpObjectToken *iface,
                                       LPCWSTR name )
{
    FIXME( "stub\n" );
    return E_NOTIMPL;
}

static HRESULT WINAPI token_DeleteValue( ISpObjectToken *iface,
                                         LPCWSTR name )
{
    FIXME( "stub\n" );
    return E_NOTIMPL;
}

static HRESULT WINAPI token_EnumKeys( ISpObjectToken *iface,
                                      ULONG index, LPWSTR *sub_key )
{
    FIXME( "stub\n" );
    return E_NOTIMPL;
}

static HRESULT WINAPI token_EnumValues( ISpObjectToken *iface,
                                        ULONG index, LPWSTR *value )
{
    FIXME( "stub\n" );
    return E_NOTIMPL;
}

static HRESULT WINAPI token_SetId( ISpObjectToken *iface,
                                   LPCWSTR category_id, LPCWSTR token_id,
                                   BOOL create )
{
    struct object_token *This = impl_from_ISpObjectToken( iface );
    BOOL res;
    HRESULT hr;
    HKEY root, key;
    const WCHAR *subkey;

    FIXME( "(%p)->(%s %s %d): semi-stub\n", This, debugstr_w( category_id ),
           debugstr_w(token_id), create );

    if (This->token_key) return SPERR_ALREADY_INITIALIZED;

    if (!token_id) return E_POINTER;

    hr = parse_cat_id( token_id, &root, &subkey );
    if (hr != S_OK) return SPERR_NOT_FOUND;

    if (create)
        res = RegCreateKeyExW( root, subkey, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL);
    else
        res = RegOpenKeyExW( root, subkey, 0, KEY_ALL_ACCESS, &key );
    if (res) return SPERR_NOT_FOUND;

    This->token_key = key;
    This->token_id = wcsdup(token_id);

    return S_OK;
}

static HRESULT WINAPI token_GetId( ISpObjectToken *iface,
                                   LPWSTR *token_id )
{
    struct object_token *This = impl_from_ISpObjectToken( iface );

    TRACE( "%p, %p\n", This, token_id);

    if (!This->token_key)
        return SPERR_UNINITIALIZED;

    if (!token_id)
        return E_POINTER;

    if (!This->token_id)
    {
        FIXME("Loading default category not supported.\n");
        return E_POINTER;
    }

    *token_id = CoTaskMemAlloc( (wcslen(This->token_id) + 1) * sizeof(WCHAR));
    if (!*token_id)
        return E_OUTOFMEMORY;

    wcscpy(*token_id, This->token_id);
    return S_OK;
}

static HRESULT WINAPI token_GetCategory( ISpObjectToken *iface,
                                         ISpObjectTokenCategory **category )
{
    FIXME( "stub\n" );
    return E_NOTIMPL;
}

struct speech_audio
{
    ISpAudio ISpAudio_iface;
    LONG ref;
};

static inline struct speech_audio *impl_from_ISpAudio(ISpAudio *iface)
{
    return CONTAINING_RECORD(iface, struct speech_audio, ISpAudio_iface);
}

static HRESULT WINAPI spaudio_QueryInterface(ISpAudio *iface, REFIID iid, void **obj)
{
    struct speech_audio *audio = impl_from_ISpAudio(iface);

    TRACE("(%p, %s %p).\n", audio, debugstr_guid(iid), obj);

    if (IsEqualIID(iid, &IID_IUnknown) ||
        IsEqualIID(iid, &IID_ISequentialStream) ||
        IsEqualIID(iid, &IID_IStream) ||
        IsEqualIID(iid, &IID_ISpStreamFormat) ||
        IsEqualIID(iid, &IID_ISpAudio))
        *obj = &audio->ISpAudio_iface;
    else
    {
        *obj = NULL;
        FIXME("interface %s not implemented.\n", debugstr_guid(iid));
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown *)*obj);
    return S_OK;
}

static ULONG WINAPI spaudio_AddRef(ISpAudio *iface)
{
    struct speech_audio *audio = impl_from_ISpAudio(iface);
    ULONG ref = InterlockedIncrement(&audio->ref);

    TRACE("(%p): ref=%u.\n", audio, ref);

    return ref;
}

static ULONG WINAPI spaudio_Release(ISpAudio *iface)
{
    struct speech_audio *audio = impl_from_ISpAudio(iface);
    ULONG ref = InterlockedDecrement(&audio->ref);

    TRACE("(%p): ref=%u.\n", audio, ref);

    if (!ref)
    {
        heap_free(audio);
    }

    return ref;
}

static HRESULT WINAPI spaudio_Read(ISpAudio *iface,void *pv, ULONG cb, ULONG *pcbRead)
{
    struct speech_audio *audio = impl_from_ISpAudio(iface);

    FIXME("%p, %p, %d %p\n", audio, pv, cb, pcbRead);

    return E_NOTIMPL;
}

static HRESULT WINAPI spaudio_Write(ISpAudio *iface, const void *pv, ULONG cb, ULONG *pcbWritten)
{
    struct speech_audio *audio = impl_from_ISpAudio(iface);

    FIXME("%p, %p, %d %p\n", audio, pv, cb, pcbWritten);

    return E_NOTIMPL;
}

static HRESULT WINAPI spaudio_Seek(ISpAudio *iface, LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER *plibNewPosition)
{
    struct speech_audio *audio = impl_from_ISpAudio(iface);
    FIXME("%p, %s, %d, %p\n", audio, wine_dbgstr_longlong(dlibMove.QuadPart), dwOrigin, plibNewPosition);
    return E_NOTIMPL;
}

static HRESULT WINAPI spaudio_SetSize(ISpAudio *iface, ULARGE_INTEGER libNewSize)
{
    struct speech_audio *audio = impl_from_ISpAudio(iface);
    FIXME("(%p, %s)\n", audio, wine_dbgstr_longlong(libNewSize.QuadPart));
    return E_NOTIMPL;
}

static HRESULT WINAPI spaudio_CopyTo(ISpAudio *iface,IStream *pstm, ULARGE_INTEGER cb,
        ULARGE_INTEGER *pcbRead, ULARGE_INTEGER *pcbWritten)
{
    struct speech_audio *audio = impl_from_ISpAudio(iface);
    FIXME("(%p, %p, %s, %p, %p)\n", audio, pstm, wine_dbgstr_longlong(cb.QuadPart), pcbRead, pcbWritten);
    return E_NOTIMPL;
}

static HRESULT WINAPI spaudio_Commit(ISpAudio *iface,DWORD grfCommitFlags)
{
    struct speech_audio *audio = impl_from_ISpAudio(iface);
    FIXME("(%p, %#x)\n", audio, grfCommitFlags);
    return E_NOTIMPL;
}

static HRESULT WINAPI spaudio_Revert(ISpAudio *iface)
{
    struct speech_audio *audio = impl_from_ISpAudio(iface);
    FIXME("(%p)\n", audio);
    return E_NOTIMPL;
}

static HRESULT WINAPI spaudio_LockRegion(ISpAudio *iface, ULARGE_INTEGER offset, ULARGE_INTEGER cb, DWORD dwLockType)
{
    struct speech_audio *audio = impl_from_ISpAudio(iface);
    FIXME("(%p, %s, %s, %d)\n", audio, wine_dbgstr_longlong(offset.QuadPart),
        wine_dbgstr_longlong(cb.QuadPart), dwLockType);
    return E_NOTIMPL;
}

static HRESULT WINAPI spaudio_UnlockRegion(ISpAudio *iface,ULARGE_INTEGER offset, ULARGE_INTEGER cb, DWORD dwLockType)
{
    struct speech_audio *audio = impl_from_ISpAudio(iface);
    FIXME("(%p, %s, %s, %d)\n", audio, wine_dbgstr_longlong(offset.QuadPart),
        wine_dbgstr_longlong(cb.QuadPart), dwLockType);
    return E_NOTIMPL;
}

static HRESULT WINAPI spaudio_Stat(ISpAudio *iface, STATSTG *stg, DWORD flag)
{
    struct speech_audio *audio = impl_from_ISpAudio(iface);
    FIXME("%p, %p, %d\n", audio, stg, flag);
    return E_NOTIMPL;
}

static HRESULT WINAPI spaudio_Clone(ISpAudio *iface, IStream **ppstm)
{
    struct speech_audio *audio = impl_from_ISpAudio(iface);
    FIXME("%p, %p\n", audio, ppstm);
    return E_NOTIMPL;
}

static HRESULT WINAPI spaudio_GetFormat(ISpAudio *iface, GUID *format, WAVEFORMATEX **wave)
{
    struct speech_audio *audio = impl_from_ISpAudio(iface);
    FIXME("%p, %p, %p\n", audio, format, wave);
    return E_NOTIMPL;
}

static HRESULT WINAPI spaudio_SetState(ISpAudio *iface, SPAUDIOSTATE state, ULONGLONG reserved)
{
    struct speech_audio *audio = impl_from_ISpAudio(iface);
    FIXME("%p, %d, %s\n", audio, state, wine_dbgstr_longlong(reserved));
    return E_NOTIMPL;
}

static HRESULT WINAPI spaudio_SetFormat(ISpAudio *iface, REFGUID guid, const WAVEFORMATEX *wave)
{
    struct speech_audio *audio = impl_from_ISpAudio(iface);
    FIXME("%p, %s, %p\n", audio, debugstr_guid(guid), wave);
    return E_NOTIMPL;
}

static HRESULT WINAPI spaudio_GetStatus(ISpAudio *iface, SPAUDIOSTATUS *status)
{
    struct speech_audio *audio = impl_from_ISpAudio(iface);
    FIXME("%p, %p\n", audio, status);
    return E_NOTIMPL;
}

static HRESULT WINAPI spaudio_SetBufferInfo(ISpAudio *iface,const SPAUDIOBUFFERINFO *buffer)
{
    struct speech_audio *audio = impl_from_ISpAudio(iface);
    FIXME("%p, %p\n", audio, buffer);
    return E_NOTIMPL;
}

static HRESULT WINAPI spaudio_GetBufferInfo(ISpAudio *iface, SPAUDIOBUFFERINFO *buffer)
{
    struct speech_audio *audio = impl_from_ISpAudio(iface);
    FIXME("%p, %p\n", audio, buffer);
    return E_NOTIMPL;
}

static HRESULT WINAPI spaudio_GetDefaultFormat(ISpAudio *iface, GUID *guid, WAVEFORMATEX **wave)
{
    struct speech_audio *audio = impl_from_ISpAudio(iface);
    FIXME("%p, %p, %p\n", audio, guid, wave);
    return E_NOTIMPL;
}

static HANDLE WINAPI spaudio_EventHandle(ISpAudio *iface)
{
    struct speech_audio *audio = impl_from_ISpAudio(iface);
    FIXME("%p\n", audio);
    return NULL;
}

static HRESULT WINAPI spaudio_GetVolumeLevel(ISpAudio *iface, ULONG *level)
{
    struct speech_audio *audio = impl_from_ISpAudio(iface);
    FIXME("%p, %p\n", audio, level);
    return E_NOTIMPL;
}

static HRESULT WINAPI spaudio_SetVolumeLevel(ISpAudio *iface, ULONG level)
{
    struct speech_audio *audio = impl_from_ISpAudio(iface);
    FIXME("%p, %d\n", audio, level);
    return E_NOTIMPL;
}

static HRESULT WINAPI spaudio_GetBufferNotifySize(ISpAudio *iface, ULONG *size)
{
    struct speech_audio *audio = impl_from_ISpAudio(iface);
    FIXME("%p, %p\n", audio, size);
    return E_NOTIMPL;
}

static HRESULT WINAPI spaudio_SetBufferNotifySize(ISpAudio *iface, ULONG size)
{
    struct speech_audio *audio = impl_from_ISpAudio(iface);
    FIXME("%p, %d\n", audio, size);
    return E_NOTIMPL;
}

const struct ISpAudioVtbl spaudio_vtbl =
{
    spaudio_QueryInterface,
    spaudio_AddRef,
    spaudio_Release,
    spaudio_Read,
    spaudio_Write,
    spaudio_Seek,
    spaudio_SetSize,
    spaudio_CopyTo,
    spaudio_Commit,
    spaudio_Revert,
    spaudio_LockRegion,
    spaudio_UnlockRegion,
    spaudio_Stat,
    spaudio_Clone,
    spaudio_GetFormat,
    spaudio_SetState,
    spaudio_SetFormat,
    spaudio_GetStatus,
    spaudio_SetBufferInfo,
    spaudio_GetBufferInfo,
    spaudio_GetDefaultFormat,
    spaudio_EventHandle,
    spaudio_GetVolumeLevel,
    spaudio_SetVolumeLevel,
    spaudio_GetBufferNotifySize,
    spaudio_SetBufferNotifySize
};

static HRESULT speech_audio_create(void **obj)
{
    struct speech_audio *This = heap_alloc(sizeof(*This));

    if (!This)
        return E_OUTOFMEMORY;
    This->ISpAudio_iface.lpVtbl = &spaudio_vtbl;
    This->ref = 1;

    *obj = &This->ISpAudio_iface;
    return S_OK;
}

static HRESULT WINAPI token_CreateInstance( ISpObjectToken *iface,
                                            IUnknown *outer,
                                            DWORD class_context,
                                            REFIID riid,
                                            void **object )
{
    struct object_token *This = impl_from_ISpObjectToken( iface );

    FIXME( "(%p)->(%p 0x%08x %s, %p): semi-stub\n", This, outer, class_context, debugstr_guid( riid ), object);

    if (IsEqualIID(riid, &IID_ISpAudio))
    {
        return speech_audio_create(object);
    }
    return E_NOTIMPL;
}

static HRESULT WINAPI token_GetStorageFileName( ISpObjectToken *iface,
                                                REFCLSID caller,
                                                LPCWSTR key_name,
                                                LPCWSTR filename,
                                                ULONG folder,
                                                LPWSTR *filepath )
{
    FIXME( "stub\n" );
    return E_NOTIMPL;
}

static HRESULT WINAPI token_RemoveStorageFileName( ISpObjectToken *iface,
                                                   REFCLSID caller,
                                                   LPCWSTR key_name,
                                                   BOOL delete_file )
{
    FIXME( "stub\n" );
    return E_NOTIMPL;
}

static HRESULT WINAPI token_Remove( ISpObjectToken *iface,
                                    REFCLSID caller )
{
    FIXME( "stub\n" );
    return E_NOTIMPL;
}

static HRESULT WINAPI token_IsUISupported( ISpObjectToken *iface,
                                           LPCWSTR ui_type,
                                           void *extra_data,
                                           ULONG extra_data_size,
                                           IUnknown *object,
                                           BOOL *supported )
{
    FIXME( "stub\n" );
    return E_NOTIMPL;
}

static HRESULT WINAPI token_DisplayUI( ISpObjectToken *iface,
                                       HWND parent,
                                       LPCWSTR title,
                                       LPCWSTR ui_type,
                                       void *extra_data,
                                       ULONG extra_data_size,
                                       IUnknown *object )
{
    FIXME( "stub\n" );
    return E_NOTIMPL;
}

static HRESULT WINAPI token_MatchesAttributes( ISpObjectToken *iface,
                                               LPCWSTR attributes,
                                               BOOL *matches )
{
    FIXME( "stub\n" );
    return E_NOTIMPL;
}

const struct ISpObjectTokenVtbl token_vtbl =
{
    token_QueryInterface,
    token_AddRef,
    token_Release,
    token_SetData,
    token_GetData,
    token_SetStringValue,
    token_GetStringValue,
    token_SetDWORD,
    token_GetDWORD,
    token_OpenKey,
    token_CreateKey,
    token_DeleteKey,
    token_DeleteValue,
    token_EnumKeys,
    token_EnumValues,
    token_SetId,
    token_GetId,
    token_GetCategory,
    token_CreateInstance,
    token_GetStorageFileName,
    token_RemoveStorageFileName,
    token_Remove,
    token_IsUISupported,
    token_DisplayUI,
    token_MatchesAttributes
};

HRESULT token_create( IUnknown *outer, REFIID iid, void **obj )
{
    struct object_token *This = malloc( sizeof(*This) );
    HRESULT hr;

    if (!This) return E_OUTOFMEMORY;
    This->ISpObjectToken_iface.lpVtbl = &token_vtbl;
    This->ref = 1;

    This->token_key = NULL;
    This->token_id = NULL;

    hr = ISpObjectToken_QueryInterface( &This->ISpObjectToken_iface, iid, obj );

    ISpObjectToken_Release( &This->ISpObjectToken_iface );
    return hr;
}
