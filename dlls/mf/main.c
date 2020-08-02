/*
 * Copyright (C) 2015 Austin English
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
#include "mfidl.h"
#include "rpcproxy.h"

#include "mf_private.h"
#include "handler.h"

#include "wine/debug.h"
#include "wine/heap.h"
#include "wine/list.h"

WINE_DEFAULT_DEBUG_CHANNEL(mfplat);

static HINSTANCE mf_instance;
extern const GUID CLSID_FileSchemePlugin;

struct activate_object
{
    IMFActivate IMFActivate_iface;
    LONG refcount;
    IMFAttributes *attributes;
    IUnknown *object;
    const struct activate_funcs *funcs;
    void *context;
};

static struct activate_object *impl_from_IMFActivate(IMFActivate *iface)
{
    return CONTAINING_RECORD(iface, struct activate_object, IMFActivate_iface);
}

static HRESULT WINAPI activate_object_QueryInterface(IMFActivate *iface, REFIID riid, void **obj)
{
    TRACE("%p, %s, %p.\n", iface, debugstr_guid(riid), obj);

    if (IsEqualIID(riid, &IID_IMFActivate) ||
            IsEqualIID(riid, &IID_IMFAttributes) ||
            IsEqualIID(riid, &IID_IUnknown))
    {
        *obj = iface;
        IMFActivate_AddRef(iface);
        return S_OK;
    }

    WARN("Unsupported %s.\n", debugstr_guid(riid));
    *obj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI activate_object_AddRef(IMFActivate *iface)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);
    ULONG refcount = InterlockedIncrement(&activate->refcount);

    TRACE("%p, refcount %u.\n", iface, refcount);

    return refcount;
}

static ULONG WINAPI activate_object_Release(IMFActivate *iface)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);
    ULONG refcount = InterlockedDecrement(&activate->refcount);

    TRACE("%p, refcount %u.\n", iface, refcount);

    if (!refcount)
    {
        if (activate->funcs->free_private)
            activate->funcs->free_private(activate->context);
        if (activate->object)
            IUnknown_Release(activate->object);
        IMFAttributes_Release(activate->attributes);
        heap_free(activate);
    }

    return refcount;
}

static HRESULT WINAPI activate_object_GetItem(IMFActivate *iface, REFGUID key, PROPVARIANT *value)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %p.\n", iface, debugstr_guid(key), value);

    return IMFAttributes_GetItem(activate->attributes, key, value);
}

static HRESULT WINAPI activate_object_GetItemType(IMFActivate *iface, REFGUID key, MF_ATTRIBUTE_TYPE *type)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %p.\n", iface, debugstr_guid(key), type);

    return IMFAttributes_GetItemType(activate->attributes, key, type);
}

static HRESULT WINAPI activate_object_CompareItem(IMFActivate *iface, REFGUID key, REFPROPVARIANT value, BOOL *result)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %p, %p.\n", iface, debugstr_guid(key), value, result);

    return IMFAttributes_CompareItem(activate->attributes, key, value, result);
}

static HRESULT WINAPI activate_object_Compare(IMFActivate *iface, IMFAttributes *theirs, MF_ATTRIBUTES_MATCH_TYPE type,
        BOOL *result)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %p, %d, %p.\n", iface, theirs, type, result);

    return IMFAttributes_Compare(activate->attributes, theirs, type, result);
}

static HRESULT WINAPI activate_object_GetUINT32(IMFActivate *iface, REFGUID key, UINT32 *value)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %p.\n", iface, debugstr_guid(key), value);

    return IMFAttributes_GetUINT32(activate->attributes, key, value);
}

static HRESULT WINAPI activate_object_GetUINT64(IMFActivate *iface, REFGUID key, UINT64 *value)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %p.\n", iface, debugstr_guid(key), value);

    return IMFAttributes_GetUINT64(activate->attributes, key, value);
}

static HRESULT WINAPI activate_object_GetDouble(IMFActivate *iface, REFGUID key, double *value)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %p.\n", iface, debugstr_guid(key), value);

    return IMFAttributes_GetDouble(activate->attributes, key, value);
}

static HRESULT WINAPI activate_object_GetGUID(IMFActivate *iface, REFGUID key, GUID *value)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %p.\n", iface, debugstr_guid(key), value);

    return IMFAttributes_GetGUID(activate->attributes, key, value);
}

static HRESULT WINAPI activate_object_GetStringLength(IMFActivate *iface, REFGUID key, UINT32 *length)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %p.\n", iface, debugstr_guid(key), length);

    return IMFAttributes_GetStringLength(activate->attributes, key, length);
}

static HRESULT WINAPI activate_object_GetString(IMFActivate *iface, REFGUID key, WCHAR *value,
        UINT32 size, UINT32 *length)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %p, %d, %p.\n", iface, debugstr_guid(key), value, size, length);

    return IMFAttributes_GetString(activate->attributes, key, value, size, length);
}

static HRESULT WINAPI activate_object_GetAllocatedString(IMFActivate *iface, REFGUID key,
        WCHAR **value, UINT32 *length)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %p, %p.\n", iface, debugstr_guid(key), value, length);

    return IMFAttributes_GetAllocatedString(activate->attributes, key, value, length);
}

static HRESULT WINAPI activate_object_GetBlobSize(IMFActivate *iface, REFGUID key, UINT32 *size)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %p.\n", iface, debugstr_guid(key), size);

    return IMFAttributes_GetBlobSize(activate->attributes, key, size);
}

static HRESULT WINAPI activate_object_GetBlob(IMFActivate *iface, REFGUID key, UINT8 *buf,
        UINT32 bufsize, UINT32 *blobsize)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %p, %d, %p.\n", iface, debugstr_guid(key), buf, bufsize, blobsize);

    return IMFAttributes_GetBlob(activate->attributes, key, buf, bufsize, blobsize);
}

static HRESULT WINAPI activate_object_GetAllocatedBlob(IMFActivate *iface, REFGUID key, UINT8 **buf, UINT32 *size)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %p, %p.\n", iface, debugstr_guid(key), buf, size);

    return IMFAttributes_GetAllocatedBlob(activate->attributes, key, buf, size);
}

static HRESULT WINAPI activate_object_GetUnknown(IMFActivate *iface, REFGUID key, REFIID riid, void **ppv)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %s, %p.\n", iface, debugstr_guid(key), debugstr_guid(riid), ppv);

    return IMFAttributes_GetUnknown(activate->attributes, key, riid, ppv);
}

static HRESULT WINAPI activate_object_SetItem(IMFActivate *iface, REFGUID key, REFPROPVARIANT value)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %p.\n", iface, debugstr_guid(key), value);

    return IMFAttributes_SetItem(activate->attributes, key, value);
}

static HRESULT WINAPI activate_object_DeleteItem(IMFActivate *iface, REFGUID key)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s.\n", iface, debugstr_guid(key));

    return IMFAttributes_DeleteItem(activate->attributes, key);
}

static HRESULT WINAPI activate_object_DeleteAllItems(IMFActivate *iface)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p.\n", iface);

    return IMFAttributes_DeleteAllItems(activate->attributes);
}

static HRESULT WINAPI activate_object_SetUINT32(IMFActivate *iface, REFGUID key, UINT32 value)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %d.\n", iface, debugstr_guid(key), value);

    return IMFAttributes_SetUINT32(activate->attributes, key, value);
}

static HRESULT WINAPI activate_object_SetUINT64(IMFActivate *iface, REFGUID key, UINT64 value)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %s.\n", iface, debugstr_guid(key), wine_dbgstr_longlong(value));

    return IMFAttributes_SetUINT64(activate->attributes, key, value);
}

static HRESULT WINAPI activate_object_SetDouble(IMFActivate *iface, REFGUID key, double value)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %f.\n", iface, debugstr_guid(key), value);

    return IMFAttributes_SetDouble(activate->attributes, key, value);
}

static HRESULT WINAPI activate_object_SetGUID(IMFActivate *iface, REFGUID key, REFGUID value)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %s.\n", iface, debugstr_guid(key), debugstr_guid(value));

    return IMFAttributes_SetGUID(activate->attributes, key, value);
}

static HRESULT WINAPI activate_object_SetString(IMFActivate *iface, REFGUID key, const WCHAR *value)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %s.\n", iface, debugstr_guid(key), debugstr_w(value));

    return IMFAttributes_SetString(activate->attributes, key, value);
}

static HRESULT WINAPI activate_object_SetBlob(IMFActivate *iface, REFGUID key, const UINT8 *buf, UINT32 size)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %s, %p, %d.\n", iface, debugstr_guid(key), buf, size);

    return IMFAttributes_SetBlob(activate->attributes, key, buf, size);
}

static HRESULT WINAPI activate_object_SetUnknown(IMFActivate *iface, REFGUID key, IUnknown *unknown)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("(%p)->(%s, %p)\n", iface, debugstr_guid(key), unknown);

    return IMFAttributes_SetUnknown(activate->attributes, key, unknown);
}

static HRESULT WINAPI activate_object_LockStore(IMFActivate *iface)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p.\n", iface);

    return IMFAttributes_LockStore(activate->attributes);
}

static HRESULT WINAPI activate_object_UnlockStore(IMFActivate *iface)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p.\n", iface);

    return IMFAttributes_UnlockStore(activate->attributes);
}

static HRESULT WINAPI activate_object_GetCount(IMFActivate *iface, UINT32 *count)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %p.\n", iface, count);

    return IMFAttributes_GetCount(activate->attributes, count);
}

static HRESULT WINAPI activate_object_GetItemByIndex(IMFActivate *iface, UINT32 index, GUID *key, PROPVARIANT *value)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %u, %p, %p.\n", iface, index, key, value);

    return IMFAttributes_GetItemByIndex(activate->attributes, index, key, value);
}

static HRESULT WINAPI activate_object_CopyAllItems(IMFActivate *iface, IMFAttributes *dest)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);

    TRACE("%p, %p.\n", iface, dest);

    return IMFAttributes_CopyAllItems(activate->attributes, dest);
}

static HRESULT WINAPI activate_object_ActivateObject(IMFActivate *iface, REFIID riid, void **obj)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);
    IUnknown *object;
    HRESULT hr;

    TRACE("%p, %s, %p.\n", iface, debugstr_guid(riid), obj);

    if (!activate->object)
    {
        if (FAILED(hr = activate->funcs->create_object((IMFAttributes *)iface, activate->context, &object)))
            return hr;

        if (InterlockedCompareExchangePointer((void **)&activate->object, object, NULL))
            IUnknown_Release(object);
    }

    return IUnknown_QueryInterface(activate->object, riid, obj);
}

static HRESULT WINAPI activate_object_ShutdownObject(IMFActivate *iface)
{
    struct activate_object *activate = impl_from_IMFActivate(iface);
    IUnknown *object;

    TRACE("%p.\n", iface);

    if ((object = InterlockedCompareExchangePointer((void **)&activate->object, NULL, activate->object)))
    {
        activate->funcs->shutdown_object(activate->context, object);
        IUnknown_Release(object);
    }

    return S_OK;
}

static HRESULT WINAPI activate_object_DetachObject(IMFActivate *iface)
{
    TRACE("%p.\n", iface);

    return E_NOTIMPL;
}

static const IMFActivateVtbl activate_object_vtbl =
{
    activate_object_QueryInterface,
    activate_object_AddRef,
    activate_object_Release,
    activate_object_GetItem,
    activate_object_GetItemType,
    activate_object_CompareItem,
    activate_object_Compare,
    activate_object_GetUINT32,
    activate_object_GetUINT64,
    activate_object_GetDouble,
    activate_object_GetGUID,
    activate_object_GetStringLength,
    activate_object_GetString,
    activate_object_GetAllocatedString,
    activate_object_GetBlobSize,
    activate_object_GetBlob,
    activate_object_GetAllocatedBlob,
    activate_object_GetUnknown,
    activate_object_SetItem,
    activate_object_DeleteItem,
    activate_object_DeleteAllItems,
    activate_object_SetUINT32,
    activate_object_SetUINT64,
    activate_object_SetDouble,
    activate_object_SetGUID,
    activate_object_SetString,
    activate_object_SetBlob,
    activate_object_SetUnknown,
    activate_object_LockStore,
    activate_object_UnlockStore,
    activate_object_GetCount,
    activate_object_GetItemByIndex,
    activate_object_CopyAllItems,
    activate_object_ActivateObject,
    activate_object_ShutdownObject,
    activate_object_DetachObject,
};

HRESULT create_activation_object(void *context, const struct activate_funcs *funcs, IMFActivate **ret)
{
    struct activate_object *object;
    HRESULT hr;

    object = heap_alloc_zero(sizeof(*object));
    if (!object)
        return E_OUTOFMEMORY;

    object->IMFActivate_iface.lpVtbl = &activate_object_vtbl;
    object->refcount = 1;
    if (FAILED(hr = MFCreateAttributes(&object->attributes, 0)))
    {
        heap_free(object);
        return hr;
    }
    object->funcs = funcs;
    object->context = context;

    *ret = &object->IMFActivate_iface;

    return S_OK;
}

struct class_factory
{
    IClassFactory IClassFactory_iface;
    HRESULT (*create_instance)(REFIID riid, void **obj);
};

static inline struct class_factory *impl_from_IClassFactory(IClassFactory *iface)
{
    return CONTAINING_RECORD(iface, struct class_factory, IClassFactory_iface);
}

static HRESULT WINAPI class_factory_QueryInterface(IClassFactory *iface, REFIID riid, void **obj)
{
    TRACE("%p, %s, %p.\n", iface, debugstr_guid(riid), obj);

    if (IsEqualGUID(riid, &IID_IClassFactory) ||
            IsEqualGUID(riid, &IID_IUnknown))
    {
        *obj = iface;
        IClassFactory_AddRef(iface);
        return S_OK;
    }

    WARN("%s is not supported.\n", debugstr_guid(riid));
    *obj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI class_factory_AddRef(IClassFactory *iface)
{
    return 2;
}

static ULONG WINAPI class_factory_Release(IClassFactory *iface)
{
    return 1;
}

static HRESULT WINAPI class_factory_CreateInstance(IClassFactory *iface, IUnknown *outer, REFIID riid, void **obj)
{
    struct class_factory *factory = impl_from_IClassFactory(iface);

    TRACE("%p, %p, %s, %p.\n", iface, outer, debugstr_guid(riid), obj);

    if (outer)
    {
        *obj = NULL;
        return CLASS_E_NOAGGREGATION;
    }

    return factory->create_instance(riid, obj);
}

static HRESULT WINAPI class_factory_LockServer(IClassFactory *iface, BOOL dolock)
{
    FIXME("%d.\n", dolock);

    return S_OK;
}

static const IClassFactoryVtbl class_factory_vtbl =
{
    class_factory_QueryInterface,
    class_factory_AddRef,
    class_factory_Release,
    class_factory_CreateInstance,
    class_factory_LockServer,
};

struct file_scheme_handler
{
    IMFSchemeHandler IMFSchemeHandler_iface;
    LONG refcount;
    IMFSourceResolver *resolver;
    struct handler handler;
};

static struct file_scheme_handler *impl_from_IMFSchemeHandler(IMFSchemeHandler *iface)
{
    return CONTAINING_RECORD(iface, struct file_scheme_handler, IMFSchemeHandler_iface);
}

static HRESULT WINAPI file_scheme_handler_QueryInterface(IMFSchemeHandler *iface, REFIID riid, void **obj)
{
    TRACE("%p, %s, %p.\n", iface, debugstr_guid(riid), obj);

    if (IsEqualIID(riid, &IID_IMFSchemeHandler) ||
            IsEqualIID(riid, &IID_IUnknown))
    {
        *obj = iface;
        IMFSchemeHandler_AddRef(iface);
        return S_OK;
    }

    WARN("Unsupported %s.\n", debugstr_guid(riid));
    *obj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI file_scheme_handler_AddRef(IMFSchemeHandler *iface)
{
    struct file_scheme_handler *handler = impl_from_IMFSchemeHandler(iface);
    ULONG refcount = InterlockedIncrement(&handler->refcount);

    TRACE("%p, refcount %u.\n", handler, refcount);

    return refcount;
}

static ULONG WINAPI file_scheme_handler_Release(IMFSchemeHandler *iface)
{
    struct file_scheme_handler *this = impl_from_IMFSchemeHandler(iface);
    ULONG refcount = InterlockedDecrement(&this->refcount);

    TRACE("%p, refcount %u.\n", iface, refcount);

    if (!refcount)
    {
        if (this->resolver)
            IMFSourceResolver_Release(this->resolver);
        handler_destruct(&this->handler);
    }

    return refcount;
}

static HRESULT WINAPI file_scheme_handler_BeginCreateObject(IMFSchemeHandler *iface, const WCHAR *url, DWORD flags,
        IPropertyStore *props, IUnknown **cancel_cookie, IMFAsyncCallback *callback, IUnknown *state)
{
    struct file_scheme_handler *this = impl_from_IMFSchemeHandler(iface);

    TRACE("%p, %s, %#x, %p, %p, %p, %p.\n", iface, debugstr_w(url), flags, props, cancel_cookie, callback, state);
    return handler_begin_create_object(&this->handler, NULL, url, flags, props, cancel_cookie, callback, state);
}

static HRESULT WINAPI file_scheme_handler_EndCreateObject(IMFSchemeHandler *iface, IMFAsyncResult *result,
        MF_OBJECT_TYPE *obj_type, IUnknown **object)
{
    struct file_scheme_handler *this = impl_from_IMFSchemeHandler(iface);

    TRACE("%p, %p, %p, %p.\n", iface, result, obj_type, object);
    return handler_end_create_object(&this->handler, result, obj_type, object);
}

static HRESULT WINAPI file_scheme_handler_CancelObjectCreation(IMFSchemeHandler *iface, IUnknown *cancel_cookie)
{
    struct file_scheme_handler *this = impl_from_IMFSchemeHandler(iface);

    TRACE("%p, %p.\n", iface, cancel_cookie);
    return handler_cancel_object_creation(&this->handler, cancel_cookie);
}

static const IMFSchemeHandlerVtbl file_scheme_handler_vtbl =
{
    file_scheme_handler_QueryInterface,
    file_scheme_handler_AddRef,
    file_scheme_handler_Release,
    file_scheme_handler_BeginCreateObject,
    file_scheme_handler_EndCreateObject,
    file_scheme_handler_CancelObjectCreation,
};

static HRESULT file_scheme_handler_get_resolver(struct file_scheme_handler *handler, IMFSourceResolver **resolver)
{
    HRESULT hr;

    if (!handler->resolver)
    {
        IMFSourceResolver *resolver;

        if (FAILED(hr = MFCreateSourceResolver(&resolver)))
            return hr;

        if (InterlockedCompareExchangePointer((void **)&handler->resolver, resolver, NULL))
            IMFSourceResolver_Release(resolver);
    }

    *resolver = handler->resolver;
    IMFSourceResolver_AddRef(*resolver);

    return S_OK;
}

static HRESULT file_scheme_handler_create_object(struct handler *handler, WCHAR *url, IMFByteStream *stream, DWORD flags,
                                         IPropertyStore *props, IUnknown **out_object, MF_OBJECT_TYPE *out_obj_type)
{
    static const WCHAR schemeW[] = {'f','i','l','e',':','/','/'};
    HRESULT hr = S_OK;
    WCHAR *path;
    IMFByteStream *file_byte_stream;
    struct file_scheme_handler *this = CONTAINING_RECORD(handler, struct file_scheme_handler, handler);
    IMFSourceResolver *resolver;

    /* Strip from scheme, MFCreateFile() won't be expecting it. */
    path = url;
    if (!wcsnicmp(url, schemeW, ARRAY_SIZE(schemeW)))
        path += ARRAY_SIZE(schemeW);

    hr = MFCreateFile(flags & MF_RESOLUTION_WRITE ? MF_ACCESSMODE_READWRITE : MF_ACCESSMODE_READ,
            MF_OPENMODE_FAIL_IF_NOT_EXIST, MF_FILEFLAGS_NONE, path, &file_byte_stream);
    if (SUCCEEDED(hr))
    {
        if (flags & MF_RESOLUTION_MEDIASOURCE)
        {
            if (SUCCEEDED(hr = file_scheme_handler_get_resolver(this, &resolver)))
            {
                hr = IMFSourceResolver_CreateObjectFromByteStream(resolver, file_byte_stream, url, flags,
                        props, out_obj_type, out_object);
                IMFSourceResolver_Release(resolver);
                IMFByteStream_Release(file_byte_stream);
            }
        }
        else
        {
            *out_object = (IUnknown *)file_byte_stream;
            *out_obj_type = MF_OBJECT_BYTESTREAM;
        }
    }

    return hr;
}

static HRESULT file_scheme_handler_construct(REFIID riid, void **obj)
{
    struct file_scheme_handler *this;
    HRESULT hr;

    TRACE("%s, %p.\n", debugstr_guid(riid), obj);

    this = heap_alloc_zero(sizeof(*this));
    if (!this)
        return E_OUTOFMEMORY;

    handler_construct(&this->handler, file_scheme_handler_create_object);

    this->IMFSchemeHandler_iface.lpVtbl = &file_scheme_handler_vtbl;
    this->refcount = 1;

    hr = IMFSchemeHandler_QueryInterface(&this->IMFSchemeHandler_iface, riid, obj);
    IMFSchemeHandler_Release(&this->IMFSchemeHandler_iface);

    return hr;
}

static struct class_factory file_scheme_handler_factory = { { &class_factory_vtbl }, file_scheme_handler_construct };

static const struct class_object
{
    const GUID *clsid;
    IClassFactory *factory;
}
class_objects[] =
{
    { &CLSID_FileSchemePlugin, &file_scheme_handler_factory.IClassFactory_iface },
};

/*******************************************************************************
 *      DllGetClassObject (mf.@)
 */
HRESULT WINAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void **obj)
{
    unsigned int i;

    TRACE("%s, %s, %p.\n", debugstr_guid(rclsid), debugstr_guid(riid), obj);

    for (i = 0; i < ARRAY_SIZE(class_objects); ++i)
    {
        if (IsEqualGUID(class_objects[i].clsid, rclsid))
            return IClassFactory_QueryInterface(class_objects[i].factory, riid, obj);
    }

    WARN("%s: class not found.\n", debugstr_guid(rclsid));
    return CLASS_E_CLASSNOTAVAILABLE;
}

/******************************************************************
 *              DllCanUnloadNow (mf.@)
 */
HRESULT WINAPI DllCanUnloadNow(void)
{
    return S_FALSE;
}

/***********************************************************************
 *          DllRegisterServer (mf.@)
 */
HRESULT WINAPI DllRegisterServer(void)
{
    return __wine_register_resources( mf_instance );
}

/***********************************************************************
 *          DllUnregisterServer (mf.@)
 */
HRESULT WINAPI DllUnregisterServer(void)
{
    return __wine_unregister_resources( mf_instance );
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    switch (reason)
    {
        case DLL_WINE_PREATTACH:
            return FALSE;    /* prefer native version */
        case DLL_PROCESS_ATTACH:
            mf_instance = instance;
            DisableThreadLibraryCalls(instance);
            break;
    }

    return TRUE;
}

static HRESULT prop_string_vector_append(PROPVARIANT *vector, unsigned int *capacity, BOOL unique, const WCHAR *str)
{
    WCHAR *ptrW;
    int len, i;

    if (unique)
    {
        for (i = 0; i < vector->calpwstr.cElems; ++i)
        {
            if (!lstrcmpW(vector->calpwstr.pElems[i], str))
                return S_OK;
        }
    }

    if (!*capacity || *capacity - 1 < vector->calpwstr.cElems)
    {
        unsigned int new_count;
        WCHAR **ptr;

        new_count = *capacity ? *capacity * 2 : 10;
        ptr = CoTaskMemRealloc(vector->calpwstr.pElems, new_count * sizeof(*vector->calpwstr.pElems));
        if (!ptr)
            return E_OUTOFMEMORY;
        vector->calpwstr.pElems = ptr;
        *capacity = new_count;
    }

    len = lstrlenW(str);
    if (!(vector->calpwstr.pElems[vector->calpwstr.cElems] = ptrW = CoTaskMemAlloc((len + 1) * sizeof(WCHAR))))
        return E_OUTOFMEMORY;

    lstrcpyW(ptrW, str);
    vector->calpwstr.cElems++;

    return S_OK;
}

static int __cdecl qsort_string_compare(const void *a, const void *b)
{
    const WCHAR *left = *(const WCHAR **)a, *right = *(const WCHAR **)b;
    return lstrcmpW(left, right);
}

static HRESULT mf_get_handler_strings(const WCHAR *path, WCHAR filter, unsigned int maxlen, PROPVARIANT *dst)
{
    static const HKEY hkey_roots[2] = { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE };
    unsigned int capacity = 0, count, size;
    HRESULT hr = S_OK;
    int i, index;
    WCHAR *buffW;

    buffW = heap_calloc(maxlen, sizeof(*buffW));
    if (!buffW)
        return E_OUTOFMEMORY;

    memset(dst, 0, sizeof(*dst));
    dst->vt = VT_VECTOR | VT_LPWSTR;

    for (i = 0; i < ARRAY_SIZE(hkey_roots); ++i)
    {
        HKEY hkey;

        if (RegOpenKeyW(hkey_roots[i], path, &hkey))
            continue;

        index = 0;
        size = maxlen;
        count = dst->calpwstr.cElems;
        while (!RegEnumKeyExW(hkey, index++, buffW, &size, NULL, NULL, NULL, NULL))
        {
            if (filter && !wcschr(buffW, filter))
                continue;

            if (FAILED(hr = prop_string_vector_append(dst, &capacity, i > 0, buffW)))
                break;
            size = maxlen;
        }

        /* Sort last pass results. */
        qsort(&dst->calpwstr.pElems[count], dst->calpwstr.cElems - count, sizeof(*dst->calpwstr.pElems),
                qsort_string_compare);

        RegCloseKey(hkey);
    }

    if (FAILED(hr))
        PropVariantClear(dst);

    heap_free(buffW);

    return hr;
}

/***********************************************************************
 *      MFGetSupportedMimeTypes (mf.@)
 */
HRESULT WINAPI MFGetSupportedMimeTypes(PROPVARIANT *dst)
{
    unsigned int maxlen;

    TRACE("%p.\n", dst);

    if (!dst)
        return E_POINTER;

    /* According to RFC4288 it's 127/127 characters. */
    maxlen = 127 /* type */ + 1 /* / */ + 127 /* subtype */ + 1;
    return mf_get_handler_strings(L"Software\\Microsoft\\Windows Media Foundation\\ByteStreamHandlers", '/',
            maxlen,  dst);
}

/***********************************************************************
 *      MFGetSupportedSchemes (mf.@)
 */
HRESULT WINAPI MFGetSupportedSchemes(PROPVARIANT *dst)
{
    TRACE("%p.\n", dst);

    if (!dst)
        return E_POINTER;

    return mf_get_handler_strings(L"Software\\Microsoft\\Windows Media Foundation\\SchemeHandlers", 0, 64, dst);
}

/***********************************************************************
 *      MFGetService (mf.@)
 */
HRESULT WINAPI MFGetService(IUnknown *object, REFGUID service, REFIID riid, void **obj)
{
    IMFGetService *gs;
    HRESULT hr;

    TRACE("(%p, %s, %s, %p)\n", object, debugstr_guid(service), debugstr_guid(riid), obj);

    if (!object)
        return E_POINTER;

    if (FAILED(hr = IUnknown_QueryInterface(object, &IID_IMFGetService, (void **)&gs)))
        return hr;

    hr = IMFGetService_GetService(gs, service, riid, obj);
    IMFGetService_Release(gs);
    return hr;
}

/***********************************************************************
 *      MFShutdownObject (mf.@)
 */
HRESULT WINAPI MFShutdownObject(IUnknown *object)
{
    IMFShutdown *shutdown;

    TRACE("%p.\n", object);

    if (object && SUCCEEDED(IUnknown_QueryInterface(object, &IID_IMFShutdown, (void **)&shutdown)))
    {
        IMFShutdown_Shutdown(shutdown);
        IMFShutdown_Release(shutdown);
    }

    return S_OK;
}

/***********************************************************************
 *      MFEnumDeviceSources (mf.@)
 */
HRESULT WINAPI MFEnumDeviceSources(IMFAttributes *attributes, IMFActivate ***sources, UINT32 *count)
{
    FIXME("%p, %p, %p.\n", attributes, sources, count);

    if (!attributes || !sources || !count)
        return E_INVALIDARG;

    *count = 0;

    return S_OK;
}

struct simple_type_handler
{
    IMFMediaTypeHandler IMFMediaTypeHandler_iface;
    LONG refcount;
    IMFMediaType *media_type;
    CRITICAL_SECTION cs;
};

static struct simple_type_handler *impl_from_IMFMediaTypeHandler(IMFMediaTypeHandler *iface)
{
    return CONTAINING_RECORD(iface, struct simple_type_handler, IMFMediaTypeHandler_iface);
}

static HRESULT WINAPI simple_type_handler_QueryInterface(IMFMediaTypeHandler *iface, REFIID riid, void **obj)
{
    TRACE("%p, %s, %p.\n", iface, debugstr_guid(riid), obj);

    if (IsEqualIID(riid, &IID_IMFMediaTypeHandler) ||
            IsEqualIID(riid, &IID_IUnknown))
    {
        *obj = iface;
        IMFMediaTypeHandler_AddRef(iface);
        return S_OK;
    }

    *obj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI simple_type_handler_AddRef(IMFMediaTypeHandler *iface)
{
    struct simple_type_handler *handler = impl_from_IMFMediaTypeHandler(iface);
    ULONG refcount = InterlockedIncrement(&handler->refcount);

    TRACE("%p, refcount %u.\n", iface, refcount);

    return refcount;
}

static ULONG WINAPI simple_type_handler_Release(IMFMediaTypeHandler *iface)
{
    struct simple_type_handler *handler = impl_from_IMFMediaTypeHandler(iface);
    ULONG refcount = InterlockedDecrement(&handler->refcount);

    TRACE("%p, refcount %u.\n", iface, refcount);

    if (!refcount)
    {
        if (handler->media_type)
            IMFMediaType_Release(handler->media_type);
        DeleteCriticalSection(&handler->cs);
        heap_free(handler);
    }

    return refcount;
}

static HRESULT WINAPI simple_type_handler_IsMediaTypeSupported(IMFMediaTypeHandler *iface, IMFMediaType *in_type,
        IMFMediaType **out_type)
{
    struct simple_type_handler *handler = impl_from_IMFMediaTypeHandler(iface);
    DWORD flags = 0;
    HRESULT hr;

    TRACE("%p, %p, %p.\n", iface, in_type, out_type);

    if (out_type)
        *out_type = NULL;

    EnterCriticalSection(&handler->cs);
    if (!handler->media_type)
        hr = MF_E_UNEXPECTED;
    else
    {
        if (SUCCEEDED(hr = IMFMediaType_IsEqual(handler->media_type, in_type, &flags)))
            hr = (flags & (MF_MEDIATYPE_EQUAL_MAJOR_TYPES | MF_MEDIATYPE_EQUAL_FORMAT_TYPES)) ==
                    (MF_MEDIATYPE_EQUAL_MAJOR_TYPES | MF_MEDIATYPE_EQUAL_FORMAT_TYPES) ? S_OK : E_FAIL;
    }
    LeaveCriticalSection(&handler->cs);

    return hr;
}

static HRESULT WINAPI simple_type_handler_GetMediaTypeCount(IMFMediaTypeHandler *iface, DWORD *count)
{
    TRACE("%p, %p.\n", iface, count);

    if (!count)
        return E_POINTER;

    *count = 1;

    return S_OK;
}

static HRESULT WINAPI simple_type_handler_GetMediaTypeByIndex(IMFMediaTypeHandler *iface, DWORD index,
        IMFMediaType **type)
{
    struct simple_type_handler *handler = impl_from_IMFMediaTypeHandler(iface);

    TRACE("%p, %u, %p.\n", iface, index, type);

    if (index > 0)
        return MF_E_NO_MORE_TYPES;

    EnterCriticalSection(&handler->cs);
    *type = handler->media_type;
    if (*type)
        IMFMediaType_AddRef(*type);
    LeaveCriticalSection(&handler->cs);

    return S_OK;
}

static HRESULT WINAPI simple_type_handler_SetCurrentMediaType(IMFMediaTypeHandler *iface, IMFMediaType *media_type)
{
    struct simple_type_handler *handler = impl_from_IMFMediaTypeHandler(iface);

    TRACE("%p, %p.\n", iface, media_type);

    EnterCriticalSection(&handler->cs);
    if (handler->media_type)
        IMFMediaType_Release(handler->media_type);
    handler->media_type = media_type;
    if (handler->media_type)
        IMFMediaType_AddRef(handler->media_type);
    LeaveCriticalSection(&handler->cs);

    return S_OK;
}

static HRESULT WINAPI simple_type_handler_GetCurrentMediaType(IMFMediaTypeHandler *iface, IMFMediaType **media_type)
{
    struct simple_type_handler *handler = impl_from_IMFMediaTypeHandler(iface);

    TRACE("%p, %p.\n", iface, media_type);

    if (!media_type)
        return E_POINTER;

    EnterCriticalSection(&handler->cs);
    *media_type = handler->media_type;
    if (*media_type)
        IMFMediaType_AddRef(*media_type);
    LeaveCriticalSection(&handler->cs);

    return S_OK;
}

static HRESULT WINAPI simple_type_handler_GetMajorType(IMFMediaTypeHandler *iface, GUID *type)
{
    struct simple_type_handler *handler = impl_from_IMFMediaTypeHandler(iface);
    HRESULT hr;

    TRACE("%p, %p.\n", iface, type);

    EnterCriticalSection(&handler->cs);
    if (handler->media_type)
        hr = IMFMediaType_GetGUID(handler->media_type, &MF_MT_MAJOR_TYPE, type);
    else
        hr = MF_E_NOT_INITIALIZED;
    LeaveCriticalSection(&handler->cs);

    return hr;
}

static const IMFMediaTypeHandlerVtbl simple_type_handler_vtbl =
{
    simple_type_handler_QueryInterface,
    simple_type_handler_AddRef,
    simple_type_handler_Release,
    simple_type_handler_IsMediaTypeSupported,
    simple_type_handler_GetMediaTypeCount,
    simple_type_handler_GetMediaTypeByIndex,
    simple_type_handler_SetCurrentMediaType,
    simple_type_handler_GetCurrentMediaType,
    simple_type_handler_GetMajorType,
};

HRESULT WINAPI MFCreateSimpleTypeHandler(IMFMediaTypeHandler **handler)
{
    struct simple_type_handler *object;

    TRACE("%p.\n", handler);

    object = heap_alloc_zero(sizeof(*object));
    if (!object)
        return E_OUTOFMEMORY;

    object->IMFMediaTypeHandler_iface.lpVtbl = &simple_type_handler_vtbl;
    object->refcount = 1;
    InitializeCriticalSection(&object->cs);

    *handler = &object->IMFMediaTypeHandler_iface;

    return S_OK;
}

enum sample_copier_flags
{
    SAMPLE_COPIER_INPUT_TYPE_SET = 0x1,
    SAMPLE_COPIER_OUTPUT_TYPE_SET = 0x2
};

struct sample_copier
{
    IMFTransform IMFTransform_iface;
    LONG refcount;

    IMFAttributes *attributes;
    IMFMediaType *buffer_type;
    DWORD buffer_size;
    IMFSample *sample;
    DWORD flags;
    CRITICAL_SECTION cs;
};

static struct sample_copier *impl_copier_from_IMFTransform(IMFTransform *iface)
{
    return CONTAINING_RECORD(iface, struct sample_copier, IMFTransform_iface);
}

static HRESULT WINAPI sample_copier_transform_QueryInterface(IMFTransform *iface, REFIID riid, void **obj)
{
    TRACE("%p, %s, %p.\n", iface, debugstr_guid(riid), obj);

    if (IsEqualIID(riid, &IID_IMFTransform) ||
            IsEqualIID(riid, &IID_IUnknown))
    {
        *obj = iface;
        IMFTransform_AddRef(iface);
        return S_OK;
    }

    WARN("Unsupported interface %s.\n", debugstr_guid(riid));
    *obj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI sample_copier_transform_AddRef(IMFTransform *iface)
{
    struct sample_copier *transform = impl_copier_from_IMFTransform(iface);
    ULONG refcount = InterlockedIncrement(&transform->refcount);

    TRACE("%p, refcount %u.\n", iface, refcount);

    return refcount;
}

static ULONG WINAPI sample_copier_transform_Release(IMFTransform *iface)
{
    struct sample_copier *transform = impl_copier_from_IMFTransform(iface);
    ULONG refcount = InterlockedDecrement(&transform->refcount);

    TRACE("%p, refcount %u.\n", iface, refcount);

    if (!refcount)
    {
        IMFAttributes_Release(transform->attributes);
        if (transform->buffer_type)
            IMFMediaType_Release(transform->buffer_type);
        DeleteCriticalSection(&transform->cs);
        heap_free(transform);
    }

    return refcount;
}

static HRESULT WINAPI sample_copier_transform_GetStreamLimits(IMFTransform *iface, DWORD *input_minimum,
        DWORD *input_maximum, DWORD *output_minimum, DWORD *output_maximum)
{
    TRACE("%p, %p, %p, %p, %p.\n", iface, input_minimum, input_maximum, output_minimum, output_maximum);

    *input_minimum = *input_maximum = *output_minimum = *output_maximum = 1;

    return S_OK;
}

static HRESULT WINAPI sample_copier_transform_GetStreamCount(IMFTransform *iface, DWORD *inputs, DWORD *outputs)
{
    TRACE("%p, %p, %p.\n", iface, inputs, outputs);

    *inputs = 1;
    *outputs = 1;

    return S_OK;
}

static HRESULT WINAPI sample_copier_transform_GetStreamIDs(IMFTransform *iface, DWORD input_size, DWORD *inputs,
        DWORD output_size, DWORD *outputs)
{
    TRACE("%p, %u, %p, %u, %p.\n", iface, input_size, inputs, output_size, outputs);

    return E_NOTIMPL;
}

static HRESULT WINAPI sample_copier_transform_GetInputStreamInfo(IMFTransform *iface, DWORD id, MFT_INPUT_STREAM_INFO *info)
{
    struct sample_copier *transform = impl_copier_from_IMFTransform(iface);

    TRACE("%p, %u, %p.\n", iface, id, info);

    memset(info, 0, sizeof(*info));

    EnterCriticalSection(&transform->cs);
    info->cbSize = transform->buffer_size;
    LeaveCriticalSection(&transform->cs);

    return S_OK;
}

static HRESULT WINAPI sample_copier_transform_GetOutputStreamInfo(IMFTransform *iface, DWORD id,
        MFT_OUTPUT_STREAM_INFO *info)
{
    struct sample_copier *transform = impl_copier_from_IMFTransform(iface);

    TRACE("%p, %u, %p.\n", iface, id, info);

    memset(info, 0, sizeof(*info));

    EnterCriticalSection(&transform->cs);
    info->cbSize = transform->buffer_size;
    LeaveCriticalSection(&transform->cs);

    return S_OK;
}

static HRESULT WINAPI sample_copier_transform_GetAttributes(IMFTransform *iface, IMFAttributes **attributes)
{
    struct sample_copier *transform = impl_copier_from_IMFTransform(iface);

    TRACE("%p, %p.\n", iface, attributes);

    *attributes = transform->attributes;
    IMFAttributes_AddRef(*attributes);

    return S_OK;
}

static HRESULT WINAPI sample_copier_transform_GetInputStreamAttributes(IMFTransform *iface, DWORD id,
        IMFAttributes **attributes)
{
    TRACE("%p, %u, %p.\n", iface, id, attributes);

    return E_NOTIMPL;
}

static HRESULT WINAPI sample_copier_transform_GetOutputStreamAttributes(IMFTransform *iface, DWORD id,
        IMFAttributes **attributes)
{
    TRACE("%p, %u, %p.\n", iface, id, attributes);

    return E_NOTIMPL;
}

static HRESULT WINAPI sample_copier_transform_DeleteInputStream(IMFTransform *iface, DWORD id)
{
    TRACE("%p, %u.\n", iface, id);

    return E_NOTIMPL;
}

static HRESULT WINAPI sample_copier_transform_AddInputStreams(IMFTransform *iface, DWORD streams, DWORD *ids)
{
    TRACE("%p, %u, %p.\n", iface, streams, ids);

    return E_NOTIMPL;
}

static HRESULT WINAPI sample_copier_transform_GetInputAvailableType(IMFTransform *iface, DWORD id, DWORD index,
        IMFMediaType **type)
{
    static const GUID *types[] = { &MFMediaType_Video, &MFMediaType_Audio };
    HRESULT hr;

    TRACE("%p, %u, %u, %p.\n", iface, id, index, type);

    if (id)
        return MF_E_INVALIDSTREAMNUMBER;

    if (index > ARRAY_SIZE(types) - 1)
        return MF_E_NO_MORE_TYPES;

    if (SUCCEEDED(hr = MFCreateMediaType(type)))
        hr = IMFMediaType_SetGUID(*type, &MF_MT_MAJOR_TYPE, types[index]);

    return hr;
}

static HRESULT WINAPI sample_copier_transform_GetOutputAvailableType(IMFTransform *iface, DWORD id, DWORD index,
        IMFMediaType **type)
{
    struct sample_copier *transform = impl_copier_from_IMFTransform(iface);
    IMFMediaType *cloned_type = NULL;
    HRESULT hr = S_OK;

    TRACE("%p, %u, %u, %p.\n", iface, id, index, type);

    EnterCriticalSection(&transform->cs);
    if (transform->buffer_type)
    {
        if (SUCCEEDED(hr = MFCreateMediaType(&cloned_type)))
            hr = IMFMediaType_CopyAllItems(transform->buffer_type, (IMFAttributes *)cloned_type);
    }
    else if (id)
        hr = MF_E_INVALIDSTREAMNUMBER;
    else
        hr = MF_E_NO_MORE_TYPES;
    LeaveCriticalSection(&transform->cs);

    if (SUCCEEDED(hr))
        *type = cloned_type;
    else if (cloned_type)
        IMFMediaType_Release(cloned_type);

    return hr;
}

static HRESULT sample_copier_get_buffer_size(IMFMediaType *type, DWORD *size)
{
    GUID major, subtype;
    UINT64 frame_size;
    HRESULT hr;

    *size = 0;

    if (FAILED(hr = IMFMediaType_GetMajorType(type, &major)))
        return hr;

    if (IsEqualGUID(&major, &MFMediaType_Video))
    {
        if (SUCCEEDED(hr = IMFMediaType_GetGUID(type, &MF_MT_SUBTYPE, &subtype)))
        {
            if (SUCCEEDED(hr = IMFMediaType_GetUINT64(type, &MF_MT_FRAME_SIZE, &frame_size)))
            {
                if (FAILED(hr = MFCalculateImageSize(&subtype, (UINT32)(frame_size >> 32), (UINT32)frame_size, size)))
                    WARN("Failed to get image size for video format %s.\n", debugstr_guid(&subtype));
            }
        }
    }
    else if (IsEqualGUID(&major, &MFMediaType_Audio))
    {
        FIXME("Audio formats are not handled.\n");
        hr = E_NOTIMPL;
    }

    return hr;
}

static HRESULT sample_copier_set_media_type(struct sample_copier *transform, BOOL input, DWORD id, IMFMediaType *type,
        DWORD flags)
{
    DWORD buffer_size;
    HRESULT hr = S_OK;

    if (id)
        return MF_E_INVALIDSTREAMNUMBER;

    EnterCriticalSection(&transform->cs);
    if (type)
    {
        hr = sample_copier_get_buffer_size(type, &buffer_size);
        if (!(flags & MFT_SET_TYPE_TEST_ONLY) && SUCCEEDED(hr))
        {
            if (!transform->buffer_type)
                hr = MFCreateMediaType(&transform->buffer_type);
            if (SUCCEEDED(hr))
                hr = IMFMediaType_CopyAllItems(type, (IMFAttributes *)transform->buffer_type);
            if (SUCCEEDED(hr))
                transform->buffer_size = buffer_size;

            if (SUCCEEDED(hr))
            {
                if (input)
                {
                    transform->flags |= SAMPLE_COPIER_INPUT_TYPE_SET;
                    transform->flags &= ~SAMPLE_COPIER_OUTPUT_TYPE_SET;
                }
                else
                    transform->flags |= SAMPLE_COPIER_OUTPUT_TYPE_SET;
            }
        }
    }
    else if (transform->buffer_type)
    {
        IMFMediaType_Release(transform->buffer_type);
        transform->buffer_type = NULL;
    }
    LeaveCriticalSection(&transform->cs);

    return hr;
}

static HRESULT WINAPI sample_copier_transform_SetInputType(IMFTransform *iface, DWORD id, IMFMediaType *type, DWORD flags)
{
    struct sample_copier *transform = impl_copier_from_IMFTransform(iface);

    TRACE("%p, %u, %p, %#x.\n", iface, id, type, flags);

    return sample_copier_set_media_type(transform, TRUE, id, type, flags);
}

static HRESULT WINAPI sample_copier_transform_SetOutputType(IMFTransform *iface, DWORD id, IMFMediaType *type, DWORD flags)
{
    struct sample_copier *transform = impl_copier_from_IMFTransform(iface);

    TRACE("%p, %u, %p, %#x.\n", iface, id, type, flags);

    return sample_copier_set_media_type(transform, FALSE, id, type, flags);
}

static HRESULT sample_copier_get_current_type(struct sample_copier *transform, DWORD id, DWORD flags,
        IMFMediaType **ret)
{
    IMFMediaType *cloned_type = NULL;
    HRESULT hr;

    if (id)
        return MF_E_INVALIDSTREAMNUMBER;

    EnterCriticalSection(&transform->cs);
    if (transform->flags & flags)
    {
        if (SUCCEEDED(hr = MFCreateMediaType(&cloned_type)))
            hr = IMFMediaType_CopyAllItems(transform->buffer_type, (IMFAttributes *)cloned_type);
    }
    else
        hr = MF_E_TRANSFORM_TYPE_NOT_SET;
    LeaveCriticalSection(&transform->cs);

    if (SUCCEEDED(hr))
        *ret = cloned_type;
    else if (cloned_type)
        IMFMediaType_Release(cloned_type);

    return hr;
}

static HRESULT WINAPI sample_copier_transform_GetInputCurrentType(IMFTransform *iface, DWORD id, IMFMediaType **type)
{
    struct sample_copier *transform = impl_copier_from_IMFTransform(iface);

    TRACE("%p, %u, %p.\n", iface, id, type);

    return sample_copier_get_current_type(transform, id, SAMPLE_COPIER_INPUT_TYPE_SET, type);
}

static HRESULT WINAPI sample_copier_transform_GetOutputCurrentType(IMFTransform *iface, DWORD id, IMFMediaType **type)
{
    struct sample_copier *transform = impl_copier_from_IMFTransform(iface);

    TRACE("%p, %u, %p.\n", iface, id, type);

    return sample_copier_get_current_type(transform, id, SAMPLE_COPIER_OUTPUT_TYPE_SET, type);
}

static HRESULT WINAPI sample_copier_transform_GetInputStatus(IMFTransform *iface, DWORD id, DWORD *flags)
{
    struct sample_copier *transform = impl_copier_from_IMFTransform(iface);
    HRESULT hr = S_OK;

    TRACE("%p, %u, %p.\n", iface, id, flags);

    if (id)
        return MF_E_INVALIDSTREAMNUMBER;

    EnterCriticalSection(&transform->cs);
    if (!(transform->flags & SAMPLE_COPIER_INPUT_TYPE_SET))
        hr = MF_E_TRANSFORM_TYPE_NOT_SET;
    else
        *flags = transform->sample ? 0 : MFT_INPUT_STATUS_ACCEPT_DATA;
    LeaveCriticalSection(&transform->cs);

    return hr;
}

static HRESULT WINAPI sample_copier_transform_GetOutputStatus(IMFTransform *iface, DWORD *flags)
{
    struct sample_copier *transform = impl_copier_from_IMFTransform(iface);
    HRESULT hr = S_OK;

    TRACE("%p, %p.\n", iface, flags);

    EnterCriticalSection(&transform->cs);
    if (!(transform->flags & SAMPLE_COPIER_OUTPUT_TYPE_SET))
        hr = MF_E_TRANSFORM_TYPE_NOT_SET;
    else
        *flags = transform->sample ? MFT_OUTPUT_STATUS_SAMPLE_READY : 0;
    LeaveCriticalSection(&transform->cs);

    return hr;
}

static HRESULT WINAPI sample_copier_transform_SetOutputBounds(IMFTransform *iface, LONGLONG lower, LONGLONG upper)
{
    TRACE("%p, %s, %s.\n", iface, debugstr_time(lower), debugstr_time(upper));

    return E_NOTIMPL;
}

static HRESULT WINAPI sample_copier_transform_ProcessEvent(IMFTransform *iface, DWORD id, IMFMediaEvent *event)
{
    FIXME("%p, %u, %p.\n", iface, id, event);

    return E_NOTIMPL;
}

static HRESULT WINAPI sample_copier_transform_ProcessMessage(IMFTransform *iface, MFT_MESSAGE_TYPE message, ULONG_PTR param)
{
    struct sample_copier *transform = impl_copier_from_IMFTransform(iface);

    TRACE("%p, %#x, %p.\n", iface, message, (void *)param);

    EnterCriticalSection(&transform->cs);

    if (message == MFT_MESSAGE_COMMAND_FLUSH)
    {
        if (transform->sample)
        {
            IMFSample_Release(transform->sample);
            transform->sample = NULL;
        }
    }

    LeaveCriticalSection(&transform->cs);

    return S_OK;
}

static HRESULT WINAPI sample_copier_transform_ProcessInput(IMFTransform *iface, DWORD id, IMFSample *sample, DWORD flags)
{
    struct sample_copier *transform = impl_copier_from_IMFTransform(iface);
    HRESULT hr = S_OK;

    TRACE("%p, %u, %p, %#x.\n", iface, id, sample, flags);

    if (id)
        return MF_E_INVALIDSTREAMNUMBER;

    EnterCriticalSection(&transform->cs);
    if (!transform->buffer_type)
        hr = MF_E_TRANSFORM_TYPE_NOT_SET;
    else if (transform->sample)
        hr = MF_E_NOTACCEPTING;
    else
    {
        transform->sample = sample;
        IMFSample_AddRef(transform->sample);
    }
    LeaveCriticalSection(&transform->cs);

    return hr;
}

static HRESULT WINAPI sample_copier_transform_ProcessOutput(IMFTransform *iface, DWORD flags, DWORD count,
        MFT_OUTPUT_DATA_BUFFER *buffers, DWORD *status)
{
    struct sample_copier *transform = impl_copier_from_IMFTransform(iface);
    IMFMediaBuffer *buffer;
    DWORD sample_flags;
    HRESULT hr = S_OK;
    LONGLONG time;

    TRACE("%p, %#x, %u, %p, %p.\n", iface, flags, count, buffers, status);

    EnterCriticalSection(&transform->cs);
    if (!(transform->flags & SAMPLE_COPIER_OUTPUT_TYPE_SET))
        hr = MF_E_TRANSFORM_TYPE_NOT_SET;
    else if (!transform->sample)
        hr = MF_E_TRANSFORM_NEED_MORE_INPUT;
    else
    {
        IMFSample_CopyAllItems(transform->sample, (IMFAttributes *)buffers->pSample);

        if (SUCCEEDED(IMFSample_GetSampleDuration(transform->sample, &time)))
            IMFSample_SetSampleDuration(buffers->pSample, time);

        if (SUCCEEDED(IMFSample_GetSampleTime(transform->sample, &time)))
            IMFSample_SetSampleTime(buffers->pSample, time);

        if (SUCCEEDED(IMFSample_GetSampleFlags(transform->sample, &sample_flags)))
            IMFSample_SetSampleFlags(buffers->pSample, sample_flags);

        if (SUCCEEDED(IMFSample_ConvertToContiguousBuffer(transform->sample, NULL)))
        {
            if (SUCCEEDED(IMFSample_GetBufferByIndex(buffers->pSample, 0, &buffer)))
            {
                if (FAILED(IMFSample_CopyToBuffer(transform->sample, buffer)))
                    hr = MF_E_UNEXPECTED;
                IMFMediaBuffer_Release(buffer);
            }
        }

        IMFSample_Release(transform->sample);
        transform->sample = NULL;
    }
    LeaveCriticalSection(&transform->cs);

    return hr;
}

static const IMFTransformVtbl sample_copier_transform_vtbl =
{
    sample_copier_transform_QueryInterface,
    sample_copier_transform_AddRef,
    sample_copier_transform_Release,
    sample_copier_transform_GetStreamLimits,
    sample_copier_transform_GetStreamCount,
    sample_copier_transform_GetStreamIDs,
    sample_copier_transform_GetInputStreamInfo,
    sample_copier_transform_GetOutputStreamInfo,
    sample_copier_transform_GetAttributes,
    sample_copier_transform_GetInputStreamAttributes,
    sample_copier_transform_GetOutputStreamAttributes,
    sample_copier_transform_DeleteInputStream,
    sample_copier_transform_AddInputStreams,
    sample_copier_transform_GetInputAvailableType,
    sample_copier_transform_GetOutputAvailableType,
    sample_copier_transform_SetInputType,
    sample_copier_transform_SetOutputType,
    sample_copier_transform_GetInputCurrentType,
    sample_copier_transform_GetOutputCurrentType,
    sample_copier_transform_GetInputStatus,
    sample_copier_transform_GetOutputStatus,
    sample_copier_transform_SetOutputBounds,
    sample_copier_transform_ProcessEvent,
    sample_copier_transform_ProcessMessage,
    sample_copier_transform_ProcessInput,
    sample_copier_transform_ProcessOutput,
};

HRESULT WINAPI MFCreateSampleCopierMFT(IMFTransform **transform)
{
    struct sample_copier *object;

    TRACE("%p.\n", transform);

    object = heap_alloc_zero(sizeof(*object));
    if (!object)
        return E_OUTOFMEMORY;

    object->IMFTransform_iface.lpVtbl = &sample_copier_transform_vtbl;
    object->refcount = 1;
    MFCreateAttributes(&object->attributes, 0);
    InitializeCriticalSection(&object->cs);

    *transform = &object->IMFTransform_iface;

    return S_OK;
}
