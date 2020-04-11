/*
 * Registry functions
 *
 * Copyright (C) 1999 Juergen Schmied
 * Copyright (C) 2000 Alexandre Julliard
 * Copyright 2005 Ivan Leo Puoti, Laurent Pinchart
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
 *
 * NOTES:
 * 	HKEY_LOCAL_MACHINE	\\REGISTRY\\MACHINE
 *	HKEY_USERS		\\REGISTRY\\USER
 *	HKEY_CURRENT_CONFIG	\\REGISTRY\\MACHINE\\SYSTEM\\CURRENTCONTROLSET\\HARDWARE PROFILES\\CURRENT
  *	HKEY_CLASSES		\\REGISTRY\\MACHINE\\SOFTWARE\\CLASSES
 */

#include "config.h"
#include "wine/port.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "wine/library.h"
#include "ntdll_misc.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(reg);

/* maximum length of a value name in bytes (without terminating null) */
#define MAX_VALUE_LENGTH (16383 * sizeof(WCHAR))

/******************************************************************************
 * NtCreateKey [NTDLL.@]
 * ZwCreateKey [NTDLL.@]
 */
NTSTATUS WINAPI NtCreateKey( PHANDLE retkey, ACCESS_MASK access, const OBJECT_ATTRIBUTES *attr,
                             ULONG TitleIndex, const UNICODE_STRING *class, ULONG options,
                             PULONG dispos )
{
    NTSTATUS ret;
    data_size_t len;
    struct object_attributes *objattr;

    if (!retkey || !attr) return STATUS_ACCESS_VIOLATION;
    if (attr->Length > sizeof(OBJECT_ATTRIBUTES)) return STATUS_INVALID_PARAMETER;

    TRACE( "(%p,%s,%s,%x,%x,%p)\n", attr->RootDirectory, debugstr_us(attr->ObjectName),
           debugstr_us(class), options, access, retkey );

    if ((ret = alloc_object_attributes( attr, &objattr, &len ))) return ret;

    SERVER_START_REQ( create_key )
    {
        req->access     = access;
        req->options    = options;
        wine_server_add_data( req, objattr, len );
        if (class) wine_server_add_data( req, class->Buffer, class->Length );
        ret = wine_server_call( req );
        *retkey = wine_server_ptr_handle( reply->hkey );
        if (dispos && !ret) *dispos = reply->created ? REG_CREATED_NEW_KEY : REG_OPENED_EXISTING_KEY;
    }
    SERVER_END_REQ;

    TRACE("<- %p\n", *retkey);
    RtlFreeHeap( GetProcessHeap(), 0, objattr );
    return ret;
}

NTSTATUS WINAPI NtCreateKeyTransacted( PHANDLE retkey, ACCESS_MASK access, const OBJECT_ATTRIBUTES *attr,
                                       ULONG TitleIndex, const UNICODE_STRING *class, ULONG options,
                                       HANDLE transacted, ULONG *dispos )
{
    FIXME( "(%p,%s,%s,%x,%x,%p,%p)\n", attr->RootDirectory, debugstr_us(attr->ObjectName),
           debugstr_us(class), options, access, transacted, retkey );
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS WINAPI NtRenameKey( HANDLE handle, UNICODE_STRING *name )
{
    FIXME( "(%p %s)\n", handle, debugstr_us(name) );
    return STATUS_NOT_IMPLEMENTED;
}

/******************************************************************************
 *  RtlpNtCreateKey [NTDLL.@]
 *
 *  See NtCreateKey.
 */
NTSTATUS WINAPI RtlpNtCreateKey( PHANDLE retkey, ACCESS_MASK access, const OBJECT_ATTRIBUTES *attr,
                                 ULONG TitleIndex, const UNICODE_STRING *class, ULONG options,
                                 PULONG dispos )
{
    OBJECT_ATTRIBUTES oa;

    if (attr)
    {
        oa = *attr;
        oa.Attributes &= ~(OBJ_PERMANENT|OBJ_EXCLUSIVE);
        attr = &oa;
    }

    return NtCreateKey(retkey, access, attr, 0, NULL, 0, dispos);
}

static NTSTATUS open_key( PHANDLE retkey, ACCESS_MASK access, const OBJECT_ATTRIBUTES *attr, ULONG options )
{
    NTSTATUS ret;

    if (!retkey || !attr || !attr->ObjectName) return STATUS_ACCESS_VIOLATION;
    if ((ret = validate_open_object_attributes( attr ))) return ret;

    TRACE( "(%p,%s,%x,%p)\n", attr->RootDirectory,
           debugstr_us(attr->ObjectName), access, retkey );
    if (options & ~REG_OPTION_OPEN_LINK)
        FIXME("options %x not implemented\n", options);

    SERVER_START_REQ( open_key )
    {
        req->parent     = wine_server_obj_handle( attr->RootDirectory );
        req->access     = access;
        req->attributes = attr->Attributes;
        wine_server_add_data( req, attr->ObjectName->Buffer, attr->ObjectName->Length );
        ret = wine_server_call( req );
        *retkey = wine_server_ptr_handle( reply->hkey );
    }
    SERVER_END_REQ;
    TRACE("<- %p\n", *retkey);
    return ret;
}

/******************************************************************************
 * NtOpenKeyEx [NTDLL.@]
 * ZwOpenKeyEx [NTDLL.@]
 */
NTSTATUS WINAPI NtOpenKeyEx( PHANDLE retkey, ACCESS_MASK access, const OBJECT_ATTRIBUTES *attr, ULONG options )
{
    return open_key( retkey, access, attr, options );
}

/******************************************************************************
 * NtOpenKey [NTDLL.@]
 * ZwOpenKey [NTDLL.@]
 *
 *   OUT	HANDLE			retkey (returns 0 when failure)
 *   IN		ACCESS_MASK		access
 *   IN		POBJECT_ATTRIBUTES 	attr
 */
NTSTATUS WINAPI NtOpenKey( PHANDLE retkey, ACCESS_MASK access, const OBJECT_ATTRIBUTES *attr )
{
    return open_key( retkey, access, attr, 0 );
}

NTSTATUS WINAPI NtOpenKeyTransactedEx( PHANDLE retkey, ACCESS_MASK access, const OBJECT_ATTRIBUTES *attr,
                                       ULONG options, HANDLE transaction )
{
    FIXME( "(%p %x %p %x %p)\n", retkey, access, attr, options, transaction );
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS WINAPI NtOpenKeyTransacted( PHANDLE retkey, ACCESS_MASK access, const OBJECT_ATTRIBUTES *attr,
                                     HANDLE transaction )
{
    return NtOpenKeyTransactedEx( retkey, access, attr, 0, transaction );
}

/******************************************************************************
 * RtlpNtOpenKey [NTDLL.@]
 *
 * See NtOpenKey.
 */
NTSTATUS WINAPI RtlpNtOpenKey( PHANDLE retkey, ACCESS_MASK access, OBJECT_ATTRIBUTES *attr )
{
    if (attr)
        attr->Attributes &= ~(OBJ_PERMANENT|OBJ_EXCLUSIVE);
    return NtOpenKey(retkey, access, attr);
}

/******************************************************************************
 * NtDeleteKey [NTDLL.@]
 * ZwDeleteKey [NTDLL.@]
 */
NTSTATUS WINAPI NtDeleteKey( HANDLE hkey )
{
    NTSTATUS ret;

    TRACE( "(%p)\n", hkey );

    SERVER_START_REQ( delete_key )
    {
        req->hkey = wine_server_obj_handle( hkey );
        ret = wine_server_call( req );
    }
    SERVER_END_REQ;
    return ret;
}

/******************************************************************************
 * RtlpNtMakeTemporaryKey [NTDLL.@]
 *
 *  See NtDeleteKey.
 */
NTSTATUS WINAPI RtlpNtMakeTemporaryKey( HANDLE hkey )
{
    return NtDeleteKey(hkey);
}

/******************************************************************************
 * NtDeleteValueKey [NTDLL.@]
 * ZwDeleteValueKey [NTDLL.@]
 */
NTSTATUS WINAPI NtDeleteValueKey( HANDLE hkey, const UNICODE_STRING *name )
{
    NTSTATUS ret;

    TRACE( "(%p,%s)\n", hkey, debugstr_us(name) );
    if (name->Length > MAX_VALUE_LENGTH) return STATUS_OBJECT_NAME_NOT_FOUND;

    SERVER_START_REQ( delete_key_value )
    {
        req->hkey = wine_server_obj_handle( hkey );
        wine_server_add_data( req, name->Buffer, name->Length );
        ret = wine_server_call( req );
    }
    SERVER_END_REQ;
    return ret;
}


/******************************************************************************
 *     enumerate_key
 *
 * Implementation of NtQueryKey and NtEnumerateKey
 */
static NTSTATUS enumerate_key( HANDLE handle, int index, KEY_INFORMATION_CLASS info_class,
                               void *info, DWORD length, DWORD *result_len )

{
    NTSTATUS ret;
    void *data_ptr;
    size_t fixed_size;

    switch(info_class)
    {
    case KeyBasicInformation:  data_ptr = ((KEY_BASIC_INFORMATION *)info)->Name; break;
    case KeyFullInformation:   data_ptr = ((KEY_FULL_INFORMATION *)info)->Class; break;
    case KeyNodeInformation:   data_ptr = ((KEY_NODE_INFORMATION *)info)->Name;  break;
    case KeyNameInformation:   data_ptr = ((KEY_NAME_INFORMATION *)info)->Name;  break;
    case KeyCachedInformation: data_ptr = ((KEY_CACHED_INFORMATION *)info)+1;    break;
    default:
        FIXME( "Information class %d not implemented\n", info_class );
        return STATUS_INVALID_PARAMETER;
    }
    fixed_size = (char *)data_ptr - (char *)info;

    SERVER_START_REQ( enum_key )
    {
        req->hkey       = wine_server_obj_handle( handle );
        req->index      = index;
        req->info_class = info_class;
        if (length > fixed_size) wine_server_set_reply( req, data_ptr, length - fixed_size );
        if (!(ret = wine_server_call( req )))
        {
            switch(info_class)
            {
            case KeyBasicInformation:
                {
                    KEY_BASIC_INFORMATION keyinfo;
                    fixed_size = (char *)keyinfo.Name - (char *)&keyinfo;
                    keyinfo.LastWriteTime.QuadPart = reply->modif;
                    keyinfo.TitleIndex = 0;
                    keyinfo.NameLength = reply->namelen;
                    memcpy( info, &keyinfo, min( length, fixed_size ) );
                }
                break;
            case KeyFullInformation:
                {
                    KEY_FULL_INFORMATION keyinfo;
                    fixed_size = (char *)keyinfo.Class - (char *)&keyinfo;
                    keyinfo.LastWriteTime.QuadPart = reply->modif;
                    keyinfo.TitleIndex = 0;
                    keyinfo.ClassLength = wine_server_reply_size(reply);
                    keyinfo.ClassOffset = keyinfo.ClassLength ? fixed_size : -1;
                    keyinfo.SubKeys = reply->subkeys;
                    keyinfo.MaxNameLen = reply->max_subkey;
                    keyinfo.MaxClassLen = reply->max_class;
                    keyinfo.Values = reply->values;
                    keyinfo.MaxValueNameLen = reply->max_value;
                    keyinfo.MaxValueDataLen = reply->max_data;
                    memcpy( info, &keyinfo, min( length, fixed_size ) );
                }
                break;
            case KeyNodeInformation:
                {
                    KEY_NODE_INFORMATION keyinfo;
                    fixed_size = (char *)keyinfo.Name - (char *)&keyinfo;
                    keyinfo.LastWriteTime.QuadPart = reply->modif;
                    keyinfo.TitleIndex = 0;
                    if (reply->namelen < wine_server_reply_size(reply))
                    {
                        keyinfo.ClassLength = wine_server_reply_size(reply) - reply->namelen;
                        keyinfo.ClassOffset = fixed_size + reply->namelen;
                    }
                    else
                    {
                        keyinfo.ClassLength = 0;
                        keyinfo.ClassOffset = -1;
                    }
                    keyinfo.NameLength = reply->namelen;
                    memcpy( info, &keyinfo, min( length, fixed_size ) );
                }
                break;
            case KeyNameInformation:
                {
                    KEY_NAME_INFORMATION keyinfo;
                    fixed_size = (char *)keyinfo.Name - (char *)&keyinfo;
                    keyinfo.NameLength = reply->namelen;
                    memcpy( info, &keyinfo, min( length, fixed_size ) );
                }
                break;
            case KeyCachedInformation:
                {
                    KEY_CACHED_INFORMATION keyinfo;
                    fixed_size = sizeof(keyinfo);
                    keyinfo.LastWriteTime.QuadPart = reply->modif;
                    keyinfo.TitleIndex = 0;
                    keyinfo.SubKeys = reply->subkeys;
                    keyinfo.MaxNameLen = reply->max_subkey;
                    keyinfo.Values = reply->values;
                    keyinfo.MaxValueNameLen = reply->max_value;
                    keyinfo.MaxValueDataLen = reply->max_data;
                    keyinfo.NameLength = reply->namelen;
                    memcpy( info, &keyinfo, min( length, fixed_size ) );
                }
                break;
            default:
                break;
            }
            *result_len = fixed_size + reply->total;
            if (length < *result_len) ret = STATUS_BUFFER_OVERFLOW;
        }
    }
    SERVER_END_REQ;
    return ret;
}



/******************************************************************************
 * NtEnumerateKey [NTDLL.@]
 * ZwEnumerateKey [NTDLL.@]
 *
 * NOTES
 *  the name copied into the buffer is NOT 0-terminated
 */
NTSTATUS WINAPI NtEnumerateKey( HANDLE handle, ULONG index, KEY_INFORMATION_CLASS info_class,
                                void *info, DWORD length, DWORD *result_len )
{
    /* -1 means query key, so avoid it here */
    if (index == (ULONG)-1) return STATUS_NO_MORE_ENTRIES;
    return enumerate_key( handle, index, info_class, info, length, result_len );
}


/******************************************************************************
 * RtlpNtEnumerateSubKey [NTDLL.@]
 *
 */
NTSTATUS WINAPI RtlpNtEnumerateSubKey( HANDLE handle, UNICODE_STRING *out, ULONG index )
{
  KEY_BASIC_INFORMATION *info;
  DWORD dwLen, dwResultLen;
  NTSTATUS ret;

  if (out->Length)
  {
    dwLen = out->Length + sizeof(KEY_BASIC_INFORMATION);
    info = RtlAllocateHeap( GetProcessHeap(), 0, dwLen );
    if (!info)
      return STATUS_NO_MEMORY;
  }
  else
  {
    dwLen = 0;
    info = NULL;
  }

  ret = NtEnumerateKey( handle, index, KeyBasicInformation, info, dwLen, &dwResultLen );
  dwResultLen -= sizeof(KEY_BASIC_INFORMATION);

  if (ret == STATUS_BUFFER_OVERFLOW)
    out->Length = dwResultLen;
  else if (!ret)
  {
    if (out->Length < info->NameLength)
    {
      out->Length = dwResultLen;
      ret = STATUS_BUFFER_OVERFLOW;
    }
    else
    {
      out->Length = info->NameLength;
      memcpy(out->Buffer, info->Name, info->NameLength);
    }
  }

  RtlFreeHeap( GetProcessHeap(), 0, info );
  return ret;
}

/******************************************************************************
 * NtQueryKey [NTDLL.@]
 * ZwQueryKey [NTDLL.@]
 */
NTSTATUS WINAPI NtQueryKey( HANDLE handle, KEY_INFORMATION_CLASS info_class,
                            void *info, DWORD length, DWORD *result_len )
{
    return enumerate_key( handle, -1, info_class, info, length, result_len );
}


/* fill the key value info structure for a specific info class */
static void copy_key_value_info( KEY_VALUE_INFORMATION_CLASS info_class, void *info,
                                 DWORD length, int type, int name_len, int data_len )
{
    switch(info_class)
    {
    case KeyValueBasicInformation:
        {
            KEY_VALUE_BASIC_INFORMATION keyinfo;
            keyinfo.TitleIndex = 0;
            keyinfo.Type       = type;
            keyinfo.NameLength = name_len;
            length = min( length, (char *)keyinfo.Name - (char *)&keyinfo );
            memcpy( info, &keyinfo, length );
            break;
        }
    case KeyValueFullInformation:
        {
            KEY_VALUE_FULL_INFORMATION keyinfo;
            keyinfo.TitleIndex = 0;
            keyinfo.Type       = type;
            keyinfo.DataOffset = (char *)keyinfo.Name - (char *)&keyinfo + name_len;
            keyinfo.DataLength = data_len;
            keyinfo.NameLength = name_len;
            length = min( length, (char *)keyinfo.Name - (char *)&keyinfo );
            memcpy( info, &keyinfo, length );
            break;
        }
    case KeyValuePartialInformation:
        {
            KEY_VALUE_PARTIAL_INFORMATION keyinfo;
            keyinfo.TitleIndex = 0;
            keyinfo.Type       = type;
            keyinfo.DataLength = data_len;
            length = min( length, (char *)keyinfo.Data - (char *)&keyinfo );
            memcpy( info, &keyinfo, length );
            break;
        }
    default:
        break;
    }
}


/******************************************************************************
 *  NtEnumerateValueKey	[NTDLL.@]
 *  ZwEnumerateValueKey [NTDLL.@]
 */
NTSTATUS WINAPI NtEnumerateValueKey( HANDLE handle, ULONG index,
                                     KEY_VALUE_INFORMATION_CLASS info_class,
                                     void *info, DWORD length, DWORD *result_len )
{
    NTSTATUS ret;
    void *ptr;
    size_t fixed_size;

    TRACE( "(%p,%u,%d,%p,%d)\n", handle, index, info_class, info, length );

    /* compute the length we want to retrieve */
    switch(info_class)
    {
    case KeyValueBasicInformation:   ptr = ((KEY_VALUE_BASIC_INFORMATION *)info)->Name; break;
    case KeyValueFullInformation:    ptr = ((KEY_VALUE_FULL_INFORMATION *)info)->Name; break;
    case KeyValuePartialInformation: ptr = ((KEY_VALUE_PARTIAL_INFORMATION *)info)->Data; break;
    default:
        FIXME( "Information class %d not implemented\n", info_class );
        return STATUS_INVALID_PARAMETER;
    }
    fixed_size = (char *)ptr - (char *)info;

    SERVER_START_REQ( enum_key_value )
    {
        req->hkey       = wine_server_obj_handle( handle );
        req->index      = index;
        req->info_class = info_class;
        if (length > fixed_size) wine_server_set_reply( req, ptr, length - fixed_size );
        if (!(ret = wine_server_call( req )))
        {
            copy_key_value_info( info_class, info, length, reply->type, reply->namelen,
                                 wine_server_reply_size(reply) - reply->namelen );
            *result_len = fixed_size + reply->total;
            if (length < *result_len) ret = STATUS_BUFFER_OVERFLOW;
        }
    }
    SERVER_END_REQ;
    return ret;
}


/******************************************************************************
 * NtQueryValueKey [NTDLL.@]
 * ZwQueryValueKey [NTDLL.@]
 *
 * NOTES
 *  the name in the KeyValueInformation is never set
 */
NTSTATUS WINAPI DECLSPEC_HOTPATCH NtQueryValueKey( HANDLE handle, const UNICODE_STRING *name,
                                 KEY_VALUE_INFORMATION_CLASS info_class,
                                 void *info, DWORD length, DWORD *result_len )
{
    NTSTATUS ret;
    UCHAR *data_ptr;
    unsigned int fixed_size, min_size;

    TRACE( "(%p,%s,%d,%p,%d)\n", handle, debugstr_us(name), info_class, info, length );

    if (name->Length > MAX_VALUE_LENGTH) return STATUS_OBJECT_NAME_NOT_FOUND;

    /* compute the length we want to retrieve */
    switch(info_class)
    {
    case KeyValueBasicInformation:
    {
        KEY_VALUE_BASIC_INFORMATION *basic_info = info;
        min_size = FIELD_OFFSET(KEY_VALUE_BASIC_INFORMATION, Name);
        fixed_size = min_size + name->Length;
        if (min_size < length)
            memcpy(basic_info->Name, name->Buffer, min(length - min_size, name->Length));
        data_ptr = NULL;
        break;
    }
    case KeyValueFullInformation:
    {
        KEY_VALUE_FULL_INFORMATION *full_info = info;
        min_size = FIELD_OFFSET(KEY_VALUE_FULL_INFORMATION, Name);
        fixed_size = min_size + name->Length;
        if (min_size < length)
            memcpy(full_info->Name, name->Buffer, min(length - min_size, name->Length));
        data_ptr = (UCHAR *)full_info->Name + name->Length;
        break;
    }
    case KeyValuePartialInformation:
        min_size = fixed_size = FIELD_OFFSET(KEY_VALUE_PARTIAL_INFORMATION, Data);
        data_ptr = ((KEY_VALUE_PARTIAL_INFORMATION *)info)->Data;
        break;
    default:
        FIXME( "Information class %d not implemented\n", info_class );
        return STATUS_INVALID_PARAMETER;
    }

    SERVER_START_REQ( get_key_value )
    {
        req->hkey = wine_server_obj_handle( handle );
        wine_server_add_data( req, name->Buffer, name->Length );
        if (length > fixed_size && data_ptr) wine_server_set_reply( req, data_ptr, length - fixed_size );
        if (!(ret = wine_server_call( req )))
        {
            copy_key_value_info( info_class, info, length, reply->type,
                                 name->Length, reply->total );
            *result_len = fixed_size + (info_class == KeyValueBasicInformation ? 0 : reply->total);
            if (length < min_size) ret = STATUS_BUFFER_TOO_SMALL;
            else if (length < *result_len) ret = STATUS_BUFFER_OVERFLOW;
        }
    }
    SERVER_END_REQ;
    return ret;
}

/******************************************************************************
 * RtlpNtQueryValueKey [NTDLL.@]
 *
 */
NTSTATUS WINAPI RtlpNtQueryValueKey( HANDLE handle, ULONG *result_type, PBYTE dest,
                                     DWORD *result_len, void *unknown )
{
    KEY_VALUE_PARTIAL_INFORMATION *info;
    UNICODE_STRING name;
    NTSTATUS ret;
    DWORD dwResultLen;
    DWORD dwLen = sizeof (KEY_VALUE_PARTIAL_INFORMATION) + (result_len ? *result_len : 0);

    info = RtlAllocateHeap( GetProcessHeap(), 0, dwLen );
    if (!info)
      return STATUS_NO_MEMORY;

    name.Length = 0;
    ret = NtQueryValueKey( handle, &name, KeyValuePartialInformation, info, dwLen, &dwResultLen );

    if (!ret || ret == STATUS_BUFFER_OVERFLOW)
    {
        if (result_len)
            *result_len = info->DataLength;

        if (result_type)
            *result_type = info->Type;

        if (ret != STATUS_BUFFER_OVERFLOW)
            memcpy( dest, info->Data, info->DataLength );
    }

    RtlFreeHeap( GetProcessHeap(), 0, info );
    return ret;
}

/******************************************************************************
 *  NtFlushKey	[NTDLL.@]
 *  ZwFlushKey  [NTDLL.@]
 */
NTSTATUS WINAPI NtFlushKey(HANDLE key)
{
    NTSTATUS ret;

    TRACE("key=%p\n", key);

    SERVER_START_REQ( flush_key )
    {
	req->hkey = wine_server_obj_handle( key );
	ret = wine_server_call( req );
    }
    SERVER_END_REQ;
    
    return ret;
}

/******************************************************************************
 *  NtLoadKey	[NTDLL.@]
 *  ZwLoadKey   [NTDLL.@]
 */
NTSTATUS WINAPI NtLoadKey( const OBJECT_ATTRIBUTES *attr, OBJECT_ATTRIBUTES *file )
{
    NTSTATUS ret;
    HANDLE hive;
    IO_STATUS_BLOCK io;
    data_size_t len;
    struct object_attributes *objattr;

    TRACE("(%p,%p)\n", attr, file);

    ret = NtCreateFile(&hive, GENERIC_READ | SYNCHRONIZE, file, &io, NULL, FILE_ATTRIBUTE_NORMAL, 0,
                       FILE_OPEN, 0, NULL, 0);
    if (ret) return ret;

    if ((ret = alloc_object_attributes( attr, &objattr, &len ))) return ret;

    SERVER_START_REQ( load_registry )
    {
        req->file = wine_server_obj_handle( hive );
        wine_server_add_data( req, objattr, len );
        ret = wine_server_call( req );
    }
    SERVER_END_REQ;

    NtClose(hive);
    RtlFreeHeap( GetProcessHeap(), 0, objattr );
    return ret;
}

/******************************************************************************
 *  NtLoadKey2  [NTDLL.@]
 *  ZwLoadKey2  [NTDLL.@]
 */
NTSTATUS WINAPI NtLoadKey2(OBJECT_ATTRIBUTES *attr, OBJECT_ATTRIBUTES *file, ULONG flags)
{
    FIXME("(%p,%p,0x%08x) semi-stub: ignoring flags\n", attr, file, flags);
    return NtLoadKey(attr, file);
}

/******************************************************************************
 *  NtNotifyChangeMultipleKeys  [NTDLL.@]
 *  ZwNotifyChangeMultipleKeys  [NTDLL.@]
 */
NTSTATUS WINAPI NtNotifyChangeMultipleKeys(
        HANDLE KeyHandle,
        ULONG Count,
        OBJECT_ATTRIBUTES *SubordinateObjects,
        HANDLE Event,
        PIO_APC_ROUTINE ApcRoutine,
        PVOID ApcContext,
        PIO_STATUS_BLOCK IoStatusBlock,
        ULONG CompletionFilter,
        BOOLEAN WatchSubtree,
        PVOID ChangeBuffer,
        ULONG Length,
        BOOLEAN Asynchronous)
{
    NTSTATUS ret;

    TRACE("(%p,%u,%p,%p,%p,%p,%p,0x%08x, 0x%08x,%p,0x%08x,0x%08x)\n",
        KeyHandle, Count, SubordinateObjects, Event, ApcRoutine, ApcContext, IoStatusBlock, CompletionFilter,
        Asynchronous, ChangeBuffer, Length, WatchSubtree);

    if (Count || SubordinateObjects || ApcRoutine || ApcContext || ChangeBuffer || Length)
        FIXME("Unimplemented optional parameter\n");

    if (!Asynchronous)
    {
        OBJECT_ATTRIBUTES attr;
        InitializeObjectAttributes( &attr, NULL, 0, NULL, NULL );
        ret = NtCreateEvent( &Event, EVENT_ALL_ACCESS, &attr, SynchronizationEvent, FALSE );
        if (ret != STATUS_SUCCESS)
            return ret;
    }

    SERVER_START_REQ( set_registry_notification )
    {
        req->hkey    = wine_server_obj_handle( KeyHandle );
        req->event   = wine_server_obj_handle( Event );
        req->subtree = WatchSubtree;
        req->filter  = CompletionFilter;
        ret = wine_server_call( req );
    }
    SERVER_END_REQ;
 
    if (!Asynchronous)
    {
        if (ret == STATUS_PENDING)
            ret = NtWaitForSingleObject( Event, FALSE, NULL );
        NtClose( Event );
    }

    return ret;
}

/******************************************************************************
 *  NtNotifyChangeKey	[NTDLL.@]
 *  ZwNotifyChangeKey   [NTDLL.@]
 */
NTSTATUS WINAPI NtNotifyChangeKey(
	IN HANDLE KeyHandle,
	IN HANDLE Event,
	IN PIO_APC_ROUTINE ApcRoutine OPTIONAL,
	IN PVOID ApcContext OPTIONAL,
	OUT PIO_STATUS_BLOCK IoStatusBlock,
	IN ULONG CompletionFilter,
	IN BOOLEAN WatchSubtree,
	OUT PVOID ChangeBuffer,
	IN ULONG Length,
	IN BOOLEAN Asynchronous)
{
    return NtNotifyChangeMultipleKeys(KeyHandle, 0, NULL, Event, ApcRoutine, ApcContext,
                                      IoStatusBlock, CompletionFilter, WatchSubtree,
                                      ChangeBuffer, Length, Asynchronous);
}

/******************************************************************************
 * NtQueryMultipleValueKey [NTDLL]
 * ZwQueryMultipleValueKey
 */

NTSTATUS WINAPI NtQueryMultipleValueKey(
	HANDLE KeyHandle,
	PKEY_MULTIPLE_VALUE_INFORMATION ListOfValuesToQuery,
	ULONG NumberOfItems,
	PVOID MultipleValueInformation,
	ULONG Length,
	PULONG  ReturnLength)
{
	FIXME("(%p,%p,0x%08x,%p,0x%08x,%p) stub!\n",
	KeyHandle, ListOfValuesToQuery, NumberOfItems, MultipleValueInformation,
	Length,ReturnLength);
	return STATUS_SUCCESS;
}

/******************************************************************************
 * NtReplaceKey [NTDLL.@]
 * ZwReplaceKey [NTDLL.@]
 */
NTSTATUS WINAPI NtReplaceKey(
	IN POBJECT_ATTRIBUTES ObjectAttributes,
	IN HANDLE Key,
	IN POBJECT_ATTRIBUTES ReplacedObjectAttributes)
{
    FIXME("(%s,%p,%s),stub!\n", debugstr_ObjectAttributes(ObjectAttributes), Key,
          debugstr_ObjectAttributes(ReplacedObjectAttributes) );
    return STATUS_SUCCESS;
}
/******************************************************************************
 * NtRestoreKey [NTDLL.@]
 * ZwRestoreKey [NTDLL.@]
 */
NTSTATUS WINAPI NtRestoreKey(
	HANDLE KeyHandle,
	HANDLE FileHandle,
	ULONG RestoreFlags)
{
	FIXME("(%p,%p,0x%08x) stub\n",
	KeyHandle, FileHandle, RestoreFlags);
	return STATUS_SUCCESS;
}
/******************************************************************************
 * NtSaveKey [NTDLL.@]
 * ZwSaveKey [NTDLL.@]
 */
NTSTATUS WINAPI NtSaveKey(IN HANDLE KeyHandle, IN HANDLE FileHandle)
{
    NTSTATUS ret;

    TRACE("(%p,%p)\n", KeyHandle, FileHandle);

    SERVER_START_REQ( save_registry )
    {
        req->hkey = wine_server_obj_handle( KeyHandle );
        req->file = wine_server_obj_handle( FileHandle );
        ret = wine_server_call( req );
    }
    SERVER_END_REQ;

    return ret;
}
/******************************************************************************
 * NtSetInformationKey [NTDLL.@]
 * ZwSetInformationKey [NTDLL.@]
 */
NTSTATUS WINAPI NtSetInformationKey(
	IN HANDLE KeyHandle,
	IN const int KeyInformationClass,
	IN PVOID KeyInformation,
	IN ULONG KeyInformationLength)
{
	FIXME("(%p,0x%08x,%p,0x%08x) stub\n",
	KeyHandle, KeyInformationClass, KeyInformation, KeyInformationLength);
	return STATUS_SUCCESS;
}


/******************************************************************************
 * NtSetValueKey [NTDLL.@]
 * ZwSetValueKey [NTDLL.@]
 *
 * NOTES
 *   win95 does not care about count for REG_SZ and finds out the len by itself (js)
 *   NT does definitely care (aj)
 */
NTSTATUS WINAPI NtSetValueKey( HANDLE hkey, const UNICODE_STRING *name, ULONG TitleIndex,
                               ULONG type, const void *data, ULONG count )
{
    NTSTATUS ret;

    TRACE( "(%p,%s,%d,%p,%d)\n", hkey, debugstr_us(name), type, data, count );

    if (name->Length > MAX_VALUE_LENGTH) return STATUS_INVALID_PARAMETER;

    SERVER_START_REQ( set_key_value )
    {
        req->hkey    = wine_server_obj_handle( hkey );
        req->type    = type;
        req->namelen = name->Length;
        wine_server_add_data( req, name->Buffer, name->Length );
        wine_server_add_data( req, data, count );
        ret = wine_server_call( req );
    }
    SERVER_END_REQ;
    return ret;
}

/******************************************************************************
 * RtlpNtSetValueKey [NTDLL.@]
 *
 */
NTSTATUS WINAPI RtlpNtSetValueKey( HANDLE hkey, ULONG type, const void *data,
                                   ULONG count )
{
    UNICODE_STRING name;

    name.Length = 0;
    return NtSetValueKey( hkey, &name, 0, type, data, count );
}

/******************************************************************************
 * NtUnloadKey [NTDLL.@]
 * ZwUnloadKey [NTDLL.@]
 */
NTSTATUS WINAPI NtUnloadKey(IN POBJECT_ATTRIBUTES attr)
{
    NTSTATUS ret;

    TRACE("(%p)\n", attr);

    SERVER_START_REQ( unload_registry )
    {
        req->hkey = wine_server_obj_handle( attr->RootDirectory );
        ret = wine_server_call(req);
    }
    SERVER_END_REQ;

    return ret;
}

/******************************************************************************
 *  RtlFormatCurrentUserKeyPath		[NTDLL.@]
 *
 */
NTSTATUS WINAPI RtlFormatCurrentUserKeyPath( IN OUT PUNICODE_STRING KeyPath)
{
    static const WCHAR pathW[] = {'\\','R','e','g','i','s','t','r','y','\\','U','s','e','r','\\'};
    char buffer[sizeof(TOKEN_USER) + sizeof(SID) + sizeof(DWORD)*SID_MAX_SUB_AUTHORITIES];
    DWORD len = sizeof(buffer);
    NTSTATUS status;

    status = NtQueryInformationToken(GetCurrentThreadEffectiveToken(), TokenUser, buffer, len, &len);
    if (status == STATUS_SUCCESS)
    {
        KeyPath->MaximumLength = 0;
        status = RtlConvertSidToUnicodeString(KeyPath, ((TOKEN_USER *)buffer)->User.Sid, FALSE);
        if (status == STATUS_BUFFER_OVERFLOW)
        {
            PWCHAR buf = RtlAllocateHeap(GetProcessHeap(), 0,
                                         sizeof(pathW) + KeyPath->Length + sizeof(WCHAR));
            if (buf)
            {
                memcpy(buf, pathW, sizeof(pathW));
                KeyPath->MaximumLength = KeyPath->Length + sizeof(WCHAR);
                KeyPath->Buffer = (PWCHAR)((LPBYTE)buf + sizeof(pathW));
                status = RtlConvertSidToUnicodeString(KeyPath,
                                                      ((TOKEN_USER *)buffer)->User.Sid, FALSE);
                KeyPath->Buffer = buf;
                KeyPath->Length += sizeof(pathW);
                KeyPath->MaximumLength += sizeof(pathW);
            }
            else
                status = STATUS_NO_MEMORY;
        }
    }
    return status;
}

/******************************************************************************
 *  RtlOpenCurrentUser		[NTDLL.@]
 *
 * NOTES
 *  If we return just HKEY_CURRENT_USER the advapi tries to find a remote
 *  registry (odd handle) and fails.
 */
NTSTATUS WINAPI RtlOpenCurrentUser(
	IN ACCESS_MASK DesiredAccess, /* [in] */
	OUT PHANDLE KeyHandle)	      /* [out] handle of HKEY_CURRENT_USER */
{
	OBJECT_ATTRIBUTES ObjectAttributes;
	UNICODE_STRING ObjectName;
	NTSTATUS ret;

	TRACE("(0x%08x, %p)\n",DesiredAccess, KeyHandle);

        if ((ret = RtlFormatCurrentUserKeyPath(&ObjectName))) return ret;
	InitializeObjectAttributes(&ObjectAttributes,&ObjectName,OBJ_CASE_INSENSITIVE,0, NULL);
	ret = NtCreateKey(KeyHandle, DesiredAccess, &ObjectAttributes, 0, NULL, 0, NULL);
	RtlFreeUnicodeString(&ObjectName);
	return ret;
}


static NTSTATUS RTL_ReportRegistryValue(PKEY_VALUE_FULL_INFORMATION pInfo,
                                        PRTL_QUERY_REGISTRY_TABLE pQuery, PVOID pContext, PVOID pEnvironment)
{
    PUNICODE_STRING str;
    UNICODE_STRING src, dst;
    LONG *bin;
    ULONG offset;
    PWSTR wstr;
    DWORD res;
    NTSTATUS status = STATUS_SUCCESS;
    ULONG len;
    LPWSTR String;
    ULONG count = 0;

    if (pInfo == NULL)
    {
        if (pQuery->Flags & RTL_QUERY_REGISTRY_DIRECT)
            return STATUS_INVALID_PARAMETER;
        else
        {
            status = pQuery->QueryRoutine(pQuery->Name, pQuery->DefaultType, pQuery->DefaultData,
                                          pQuery->DefaultLength, pContext, pQuery->EntryContext);
        }
        return status;
    }
    len = pInfo->DataLength;

    if (pQuery->Flags & RTL_QUERY_REGISTRY_DIRECT)
    {
        str = pQuery->EntryContext;

        switch(pInfo->Type)
        {
        case REG_EXPAND_SZ:
            if (!(pQuery->Flags & RTL_QUERY_REGISTRY_NOEXPAND))
            {
                RtlInitUnicodeString(&src, (WCHAR*)(((CHAR*)pInfo) + pInfo->DataOffset));
                res = 0;
                dst.MaximumLength = 0;
                RtlExpandEnvironmentStrings_U(pEnvironment, &src, &dst, &res);
                dst.Length = 0;
                dst.MaximumLength = res;
                dst.Buffer = RtlAllocateHeap(GetProcessHeap(), 0, res * sizeof(WCHAR));
                RtlExpandEnvironmentStrings_U(pEnvironment, &src, &dst, &res);
                status = pQuery->QueryRoutine(pQuery->Name, pInfo->Type, dst.Buffer,
                                     dst.Length, pContext, pQuery->EntryContext);
                RtlFreeHeap(GetProcessHeap(), 0, dst.Buffer);
            }

        case REG_SZ:
        case REG_LINK:
            if (str->Buffer == NULL)
                RtlCreateUnicodeString(str, (WCHAR*)(((CHAR*)pInfo) + pInfo->DataOffset));
            else
                RtlAppendUnicodeToString(str, (WCHAR*)(((CHAR*)pInfo) + pInfo->DataOffset));
            break;

        case REG_MULTI_SZ:
            if (!(pQuery->Flags & RTL_QUERY_REGISTRY_NOEXPAND))
                return STATUS_INVALID_PARAMETER;

            if (str->Buffer == NULL)
            {
                str->Buffer = RtlAllocateHeap(GetProcessHeap(), 0, len);
                str->MaximumLength = len;
            }
            len = min(len, str->MaximumLength);
            memcpy(str->Buffer, ((CHAR*)pInfo) + pInfo->DataOffset, len);
            str->Length = len;
            break;

        default:
            bin = pQuery->EntryContext;
            if (pInfo->DataLength <= sizeof(ULONG))
                memcpy(bin, ((CHAR*)pInfo) + pInfo->DataOffset,
                    pInfo->DataLength);
            else
            {
                if (bin[0] <= sizeof(ULONG))
                {
                    memcpy(&bin[1], ((CHAR*)pInfo) + pInfo->DataOffset,
                    min(-bin[0], pInfo->DataLength));
                }
                else
                {
                   len = min(bin[0], pInfo->DataLength);
                    bin[1] = len;
                    bin[2] = pInfo->Type;
                    memcpy(&bin[3], ((CHAR*)pInfo) + pInfo->DataOffset, len);
                }
           }
           break;
        }
    }
    else
    {
        if((pQuery->Flags & RTL_QUERY_REGISTRY_NOEXPAND) ||
           (pInfo->Type != REG_EXPAND_SZ && pInfo->Type != REG_MULTI_SZ))
        {
            status = pQuery->QueryRoutine(pQuery->Name, pInfo->Type,
                ((CHAR*)pInfo) + pInfo->DataOffset, pInfo->DataLength,
                pContext, pQuery->EntryContext);
        }
        else if (pInfo->Type == REG_EXPAND_SZ)
        {
            RtlInitUnicodeString(&src, (WCHAR*)(((CHAR*)pInfo) + pInfo->DataOffset));
            res = 0;
            dst.MaximumLength = 0;
            RtlExpandEnvironmentStrings_U(pEnvironment, &src, &dst, &res);
            dst.Length = 0;
            dst.MaximumLength = res;
            dst.Buffer = RtlAllocateHeap(GetProcessHeap(), 0, res * sizeof(WCHAR));
            RtlExpandEnvironmentStrings_U(pEnvironment, &src, &dst, &res);
            status = pQuery->QueryRoutine(pQuery->Name, pInfo->Type, dst.Buffer,
                                          dst.Length, pContext, pQuery->EntryContext);
            RtlFreeHeap(GetProcessHeap(), 0, dst.Buffer);
        }
        else /* REG_MULTI_SZ */
        {
            if(pQuery->Flags & RTL_QUERY_REGISTRY_NOEXPAND)
            {
                for (offset = 0; offset <= pInfo->DataLength; offset += len + sizeof(WCHAR))
                    {
                    wstr = (WCHAR*)(((CHAR*)pInfo) + offset);
                    len = wcslen(wstr) * sizeof(WCHAR);
                    status = pQuery->QueryRoutine(pQuery->Name, pInfo->Type, wstr, len,
                        pContext, pQuery->EntryContext);
                    if(status != STATUS_SUCCESS && status != STATUS_BUFFER_TOO_SMALL)
                        return status;
                    }
            }
            else
            {
                while(count<=pInfo->DataLength)
                {
                    String = (WCHAR*)(((CHAR*)pInfo) + pInfo->DataOffset)+count;
                    count+=wcslen(String)+1;
                    RtlInitUnicodeString(&src, (WCHAR*)(((CHAR*)pInfo) + pInfo->DataOffset));
                    res = 0;
                    dst.MaximumLength = 0;
                    RtlExpandEnvironmentStrings_U(pEnvironment, &src, &dst, &res);
                    dst.Length = 0;
                    dst.MaximumLength = res;
                    dst.Buffer = RtlAllocateHeap(GetProcessHeap(), 0, res * sizeof(WCHAR));
                    RtlExpandEnvironmentStrings_U(pEnvironment, &src, &dst, &res);
                    status = pQuery->QueryRoutine(pQuery->Name, pInfo->Type, dst.Buffer,
                                                  dst.Length, pContext, pQuery->EntryContext);
                    RtlFreeHeap(GetProcessHeap(), 0, dst.Buffer);
                    if(status != STATUS_SUCCESS && status != STATUS_BUFFER_TOO_SMALL)
                        return status;
                }
            }
        }
    }
    return status;
}


static NTSTATUS RTL_KeyHandleCreateObject(ULONG RelativeTo, PCWSTR Path, POBJECT_ATTRIBUTES regkey, PUNICODE_STRING str)
{
    PCWSTR base;
    INT len;

    static const WCHAR empty[] = {0};
    static const WCHAR control[] = {'\\','R','e','g','i','s','t','r','y','\\','M','a','c','h','i','n','e',
    '\\','S','y','s','t','e','m','\\','C','u','r','r','e','n','t','C','o','n','t','r','o','l','S','e','t','\\',
    'C','o','n','t','r','o','l','\\',0};

    static const WCHAR devicemap[] = {'\\','R','e','g','i','s','t','r','y','\\','M','a','c','h','i','n','e','\\',
    'H','a','r','d','w','a','r','e','\\','D','e','v','i','c','e','M','a','p','\\',0};

    static const WCHAR services[] = {'\\','R','e','g','i','s','t','r','y','\\','M','a','c','h','i','n','e','\\',
    'S','y','s','t','e','m','\\','C','u','r','r','e','n','t','C','o','n','t','r','o','l','S','e','t','\\',
    'S','e','r','v','i','c','e','s','\\',0};

    static const WCHAR user[] = {'\\','R','e','g','i','s','t','r','y','\\','U','s','e','r','\\',
    'C','u','r','r','e','n','t','U','s','e','r','\\',0};

    static const WCHAR windows_nt[] = {'\\','R','e','g','i','s','t','r','y','\\','M','a','c','h','i','n','e','\\',
    'S','o','f','t','w','a','r','e','\\','M','i','c','r','o','s','o','f','t','\\',
    'W','i','n','d','o','w','s',' ','N','T','\\','C','u','r','r','e','n','t','V','e','r','s','i','o','n','\\',0};

    switch (RelativeTo & 0xff)
    {
    case RTL_REGISTRY_ABSOLUTE:
        base = empty;
        break;

    case RTL_REGISTRY_CONTROL:
        base = control;
        break;

    case RTL_REGISTRY_DEVICEMAP:
        base = devicemap;
        break;

    case RTL_REGISTRY_SERVICES:
        base = services;
        break;

    case RTL_REGISTRY_USER:
        base = user;
        break;

    case RTL_REGISTRY_WINDOWS_NT:
        base = windows_nt;
        break;

    default:
        return STATUS_INVALID_PARAMETER;
    }

    len = (wcslen(base) + wcslen(Path) + 1) * sizeof(WCHAR);
    str->Buffer = RtlAllocateHeap(GetProcessHeap(), 0, len);
    if (str->Buffer == NULL)
        return STATUS_NO_MEMORY;

    wcscpy(str->Buffer, base);
    wcscat(str->Buffer, Path);
    str->Length = len - sizeof(WCHAR);
    str->MaximumLength = len;
    InitializeObjectAttributes(regkey, str, OBJ_CASE_INSENSITIVE, NULL, NULL);
    return STATUS_SUCCESS;
}

static NTSTATUS RTL_GetKeyHandle(ULONG RelativeTo, PCWSTR Path, PHANDLE handle)
{
    OBJECT_ATTRIBUTES regkey;
    UNICODE_STRING string;
    NTSTATUS status;

    status = RTL_KeyHandleCreateObject(RelativeTo, Path, &regkey, &string);
    if(status != STATUS_SUCCESS)
	return status;

    status = NtOpenKey(handle, KEY_ALL_ACCESS, &regkey);
    RtlFreeUnicodeString( &string );
    return status;
}

/*************************************************************************
 * RtlQueryRegistryValues   [NTDLL.@]
 *
 * Query multiple registry values with a single call.
 *
 * PARAMS
 *  RelativeTo  [I] Registry path that Path refers to
 *  Path        [I] Path to key
 *  QueryTable  [I] Table of key values to query
 *  Context     [I] Parameter to pass to the application defined QueryRoutine function
 *  Environment [I] Optional parameter to use when performing expansion
 *
 * RETURNS
 *  STATUS_SUCCESS or an appropriate NTSTATUS error code.
 */
NTSTATUS WINAPI RtlQueryRegistryValues(IN ULONG RelativeTo, IN PCWSTR Path,
                                       IN PRTL_QUERY_REGISTRY_TABLE QueryTable, IN PVOID Context,
                                       IN PVOID Environment OPTIONAL)
{
    UNICODE_STRING Value;
    HANDLE handle, topkey;
    PKEY_VALUE_FULL_INFORMATION pInfo = NULL;
    ULONG len, buflen = 0;
    NTSTATUS status=STATUS_SUCCESS, ret = STATUS_SUCCESS;
    INT i;

    TRACE("(%d, %s, %p, %p, %p)\n", RelativeTo, debugstr_w(Path), QueryTable, Context, Environment);

    if(Path == NULL)
        return STATUS_INVALID_PARAMETER;

    /* get a valid handle */
    if (RelativeTo & RTL_REGISTRY_HANDLE)
        topkey = handle = (HANDLE)Path;
    else
    {
        status = RTL_GetKeyHandle(RelativeTo, Path, &topkey);
        handle = topkey;
    }
    if(status != STATUS_SUCCESS)
        return status;

    /* Process query table entries */
    for (; QueryTable->QueryRoutine != NULL || QueryTable->Name != NULL; ++QueryTable)
    {
        if (QueryTable->Flags &
            (RTL_QUERY_REGISTRY_SUBKEY | RTL_QUERY_REGISTRY_TOPKEY))
        {
            /* topkey must be kept open just in case we will reuse it later */
            if (handle != topkey)
                NtClose(handle);

            if (QueryTable->Flags & RTL_QUERY_REGISTRY_SUBKEY)
            {
                handle = 0;
                status = RTL_GetKeyHandle(PtrToUlong(QueryTable->Name), Path, &handle);
                if(status != STATUS_SUCCESS)
                {
                    ret = status;
                    goto out;
                }
            }
            else
                handle = topkey;
        }

        if (QueryTable->Flags & RTL_QUERY_REGISTRY_NOVALUE)
        {
            QueryTable->QueryRoutine(QueryTable->Name, REG_NONE, NULL, 0,
                Context, QueryTable->EntryContext);
            continue;
        }

        if (!handle)
        {
            if (QueryTable->Flags & RTL_QUERY_REGISTRY_REQUIRED)
            {
                ret = STATUS_OBJECT_NAME_NOT_FOUND;
                goto out;
            }
            continue;
        }

        if (QueryTable->Name == NULL)
        {
            if (QueryTable->Flags & RTL_QUERY_REGISTRY_DIRECT)
            {
                ret = STATUS_INVALID_PARAMETER;
                goto out;
            }

            /* Report all subkeys */
            for (i = 0;; ++i)
            {
                status = NtEnumerateValueKey(handle, i,
                    KeyValueFullInformation, pInfo, buflen, &len);
                if (status == STATUS_NO_MORE_ENTRIES)
                    break;
                if (status == STATUS_BUFFER_OVERFLOW ||
                    status == STATUS_BUFFER_TOO_SMALL)
                {
                    buflen = len;
                    RtlFreeHeap(GetProcessHeap(), 0, pInfo);
                    pInfo = RtlAllocateHeap(GetProcessHeap(), 0, buflen);
                    NtEnumerateValueKey(handle, i, KeyValueFullInformation,
                        pInfo, buflen, &len);
                }

                status = RTL_ReportRegistryValue(pInfo, QueryTable, Context, Environment);
                if(status != STATUS_SUCCESS && status != STATUS_BUFFER_TOO_SMALL)
                {
                    ret = status;
                    goto out;
                }
                if (QueryTable->Flags & RTL_QUERY_REGISTRY_DELETE)
                {
                    RtlInitUnicodeString(&Value, pInfo->Name);
                    NtDeleteValueKey(handle, &Value);
                }
            }

            if (i == 0  && (QueryTable->Flags & RTL_QUERY_REGISTRY_REQUIRED))
            {
                ret = STATUS_OBJECT_NAME_NOT_FOUND;
                goto out;
            }
        }
        else
        {
            RtlInitUnicodeString(&Value, QueryTable->Name);
            status = NtQueryValueKey(handle, &Value, KeyValueFullInformation,
                pInfo, buflen, &len);
            if (status == STATUS_BUFFER_OVERFLOW ||
                status == STATUS_BUFFER_TOO_SMALL)
            {
                buflen = len;
                RtlFreeHeap(GetProcessHeap(), 0, pInfo);
                pInfo = RtlAllocateHeap(GetProcessHeap(), 0, buflen);
                status = NtQueryValueKey(handle, &Value,
                    KeyValueFullInformation, pInfo, buflen, &len);
            }
            if (status != STATUS_SUCCESS)
            {
                if (QueryTable->Flags & RTL_QUERY_REGISTRY_REQUIRED)
                {
                    ret = STATUS_OBJECT_NAME_NOT_FOUND;
                    goto out;
                }
                status = RTL_ReportRegistryValue(NULL, QueryTable, Context, Environment);
                if(status != STATUS_SUCCESS && status != STATUS_BUFFER_TOO_SMALL)
                {
                    ret = status;
                    goto out;
                }
            }
            else
            {
                status = RTL_ReportRegistryValue(pInfo, QueryTable, Context, Environment);
                if(status != STATUS_SUCCESS && status != STATUS_BUFFER_TOO_SMALL)
                {
                    ret = status;
                    goto out;
                }
                if (QueryTable->Flags & RTL_QUERY_REGISTRY_DELETE)
                    NtDeleteValueKey(handle, &Value);
            }
        }
    }

out:
    RtlFreeHeap(GetProcessHeap(), 0, pInfo);
    if (handle != topkey)
        NtClose(handle);
    NtClose(topkey);
    return ret;
}

/*************************************************************************
 * RtlCheckRegistryKey   [NTDLL.@]
 *
 * Query multiple registry values with a single call.
 *
 * PARAMS
 *  RelativeTo [I] Registry path that Path refers to
 *  Path       [I] Path to key
 *
 * RETURNS
 *  STATUS_SUCCESS if the specified key exists, or an NTSTATUS error code.
 */
NTSTATUS WINAPI RtlCheckRegistryKey(IN ULONG RelativeTo, IN PWSTR Path)
{
    HANDLE handle;
    NTSTATUS status;

    TRACE("(%d, %s)\n", RelativeTo, debugstr_w(Path));

    if(!RelativeTo && (Path == NULL || Path[0] == 0))
        return STATUS_OBJECT_PATH_SYNTAX_BAD;
    if(RelativeTo & RTL_REGISTRY_HANDLE)
        return STATUS_SUCCESS;
    if((RelativeTo <= RTL_REGISTRY_USER) && (Path == NULL || Path[0] == 0))
        return STATUS_SUCCESS;

    status = RTL_GetKeyHandle(RelativeTo, Path, &handle);
    if (handle) NtClose(handle);
    if (status == STATUS_INVALID_HANDLE) status = STATUS_OBJECT_NAME_NOT_FOUND;
    return status;
}

/*************************************************************************
 * RtlCreateRegistryKey   [NTDLL.@]
 *
 * Add a key to the registry given by absolute or relative path
 *
 * PARAMS
 *  RelativeTo  [I] Registry path that Path refers to
 *  path        [I] Path to key
 *
 * RETURNS
 *  STATUS_SUCCESS or an appropriate NTSTATUS error code.
 */
NTSTATUS WINAPI RtlCreateRegistryKey(ULONG RelativeTo, PWSTR path)
{
    OBJECT_ATTRIBUTES regkey;
    UNICODE_STRING string;
    HANDLE handle;
    NTSTATUS status;

    RelativeTo &= ~RTL_REGISTRY_OPTIONAL;

    if (!RelativeTo && (path == NULL || path[0] == 0))
        return STATUS_OBJECT_PATH_SYNTAX_BAD;
    if (RelativeTo <= RTL_REGISTRY_USER && (path == NULL || path[0] == 0))
        return STATUS_SUCCESS;
    status = RTL_KeyHandleCreateObject(RelativeTo, path, &regkey, &string);
    if(status != STATUS_SUCCESS)
	return status;

    status = NtCreateKey(&handle, KEY_ALL_ACCESS, &regkey, 0, NULL, REG_OPTION_NON_VOLATILE, NULL);
    if (handle) NtClose(handle);
    RtlFreeUnicodeString( &string );
    return status;
}

/*************************************************************************
 * RtlDeleteRegistryValue   [NTDLL.@]
 *
 * Query multiple registry values with a single call.
 *
 * PARAMS
 *  RelativeTo [I] Registry path that Path refers to
 *  Path       [I] Path to key
 *  ValueName  [I] Name of the value to delete
 *
 * RETURNS
 *  STATUS_SUCCESS if the specified key is successfully deleted, or an NTSTATUS error code.
 */
NTSTATUS WINAPI RtlDeleteRegistryValue(IN ULONG RelativeTo, IN PCWSTR Path, IN PCWSTR ValueName)
{
    NTSTATUS status;
    HANDLE handle;
    UNICODE_STRING Value;

    TRACE("(%d, %s, %s)\n", RelativeTo, debugstr_w(Path), debugstr_w(ValueName));

    RtlInitUnicodeString(&Value, ValueName);
    if(RelativeTo == RTL_REGISTRY_HANDLE)
    {
        return NtDeleteValueKey((HANDLE)Path, &Value);
    }
    status = RTL_GetKeyHandle(RelativeTo, Path, &handle);
    if (status) return status;
    status = NtDeleteValueKey(handle, &Value);
    NtClose(handle);
    return status;
}

/*************************************************************************
 * RtlWriteRegistryValue   [NTDLL.@]
 *
 * Sets the registry value with provided data.
 *
 * PARAMS
 *  RelativeTo [I] Registry path that path parameter refers to
 *  path       [I] Path to the key (or handle - see RTL_GetKeyHandle)
 *  name       [I] Name of the registry value to set
 *  type       [I] Type of the registry key to set
 *  data       [I] Pointer to the user data to be set
 *  length     [I] Length of the user data pointed by data
 *
 * RETURNS
 *  STATUS_SUCCESS if the specified key is successfully set,
 *  or an NTSTATUS error code.
 */
NTSTATUS WINAPI RtlWriteRegistryValue( ULONG RelativeTo, PCWSTR path, PCWSTR name,
                                       ULONG type, PVOID data, ULONG length )
{
    HANDLE hkey;
    NTSTATUS status;
    UNICODE_STRING str;

    TRACE( "(%d, %s, %s) -> %d: %p [%d]\n", RelativeTo, debugstr_w(path), debugstr_w(name),
           type, data, length );

    RtlInitUnicodeString( &str, name );

    if (RelativeTo == RTL_REGISTRY_HANDLE)
        return NtSetValueKey( (HANDLE)path, &str, 0, type, data, length );

    status = RTL_GetKeyHandle( RelativeTo, path, &hkey );
    if (status != STATUS_SUCCESS) return status;

    status = NtSetValueKey( hkey, &str, 0, type, data, length );
    NtClose( hkey );

    return status;
}

/*************************************************************************
 * NtQueryLicenseValue   [NTDLL.@]
 *
 * NOTES
 *  On Windows all license properties are stored in a single key, but
 *  unless there is some app which explicitly depends on that, there is
 *  no good reason to reproduce that.
 */
NTSTATUS WINAPI NtQueryLicenseValue( const UNICODE_STRING *name, ULONG *result_type,
                                     PVOID data, ULONG length, ULONG *result_len )
{
    static const WCHAR LicenseInformationW[] = {'M','a','c','h','i','n','e','\\',
                                                'S','o','f','t','w','a','r','e','\\',
                                                'W','i','n','e','\\','L','i','c','e','n','s','e',
                                                'I','n','f','o','r','m','a','t','i','o','n',0};
    KEY_VALUE_PARTIAL_INFORMATION *info;
    NTSTATUS status = STATUS_OBJECT_NAME_NOT_FOUND;
    DWORD info_length, count;
    OBJECT_ATTRIBUTES attr;
    UNICODE_STRING keyW;
    HANDLE hkey;

    if (!name || !name->Buffer || !name->Length || !result_len)
        return STATUS_INVALID_PARAMETER;

    info_length = FIELD_OFFSET(KEY_VALUE_PARTIAL_INFORMATION, Data) + length;
    info = RtlAllocateHeap( GetProcessHeap(), 0, info_length );
    if (!info) return STATUS_NO_MEMORY;

    attr.Length = sizeof(attr);
    attr.RootDirectory = 0;
    attr.ObjectName = &keyW;
    attr.Attributes = 0;
    attr.SecurityDescriptor = NULL;
    attr.SecurityQualityOfService = NULL;
    RtlInitUnicodeString( &keyW, LicenseInformationW );

    /* @@ Wine registry key: HKLM\Software\Wine\LicenseInformation */
    if (!NtOpenKey( &hkey, KEY_READ, &attr ))
    {
        status = NtQueryValueKey( hkey, name, KeyValuePartialInformation,
                                  info, info_length, &count );
        if (!status || status == STATUS_BUFFER_OVERFLOW)
        {
            if (result_type)
                *result_type = info->Type;

            *result_len = info->DataLength;

            if (status == STATUS_BUFFER_OVERFLOW)
                status = STATUS_BUFFER_TOO_SMALL;
            else
                memcpy( data, info->Data, info->DataLength );
        }
        NtClose( hkey );
    }

    if (status == STATUS_OBJECT_NAME_NOT_FOUND)
        FIXME( "License key %s not found\n", debugstr_w(name->Buffer) );

    RtlFreeHeap( GetProcessHeap(), 0, info );
    return status;
}
