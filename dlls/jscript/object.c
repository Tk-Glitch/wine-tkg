/*
 * Copyright 2008 Jacek Caban for CodeWeavers
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

#include "jscript.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(jscript);

static HRESULT Object_toString(script_ctx_t *ctx, vdisp_t *jsthis, WORD flags, unsigned argc, jsval_t *argv,
        jsval_t *r)
{
    jsdisp_t *jsdisp;
    const WCHAR *str;

    /* Keep in sync with jsclass_t enum */
    static const WCHAR *names[] = {
        NULL,
        L"[object Array]",
        L"[object Boolean]",
        L"[object Date]",
        L"[object Object]",
        L"[object Error]",
        L"[object Function]",
        NULL,
        L"[object Math]",
        L"[object Number]",
        L"[object Object]",
        L"[object RegExp]",
        L"[object String]",
        L"[object Object]",
        L"[object Object]",
        L"[object Object]"
    };

    TRACE("\n");

    jsdisp = get_jsdisp(jsthis);
    if(!jsdisp) {
        str = L"[object Object]";
    }else if(names[jsdisp->builtin_info->class]) {
        str = names[jsdisp->builtin_info->class];
    }else {
        assert(jsdisp->builtin_info->class != JSCLASS_NONE);
        FIXME("jsdisp->builtin_info->class = %d\n", jsdisp->builtin_info->class);
        return E_FAIL;
    }

    if(r) {
        jsstr_t *ret;
        ret = jsstr_alloc(str);
        if(!ret)
            return E_OUTOFMEMORY;
        *r = jsval_string(ret);
    }

    return S_OK;
}

static HRESULT Object_toLocaleString(script_ctx_t *ctx, vdisp_t *jsthis, WORD flags, unsigned argc, jsval_t *argv,
        jsval_t *r)
{
    TRACE("\n");

    if(!is_jsdisp(jsthis)) {
        FIXME("Host object this\n");
        return E_FAIL;
    }

    return jsdisp_call_name(jsthis->u.jsdisp, L"toString", DISPATCH_METHOD, 0, NULL, r);
}

static HRESULT Object_valueOf(script_ctx_t *ctx, vdisp_t *jsthis, WORD flags, unsigned argc, jsval_t *argv,
        jsval_t *r)
{
    TRACE("\n");

    if(r) {
        IDispatch_AddRef(jsthis->u.disp);
        *r = jsval_disp(jsthis->u.disp);
    }
    return S_OK;
}

static HRESULT Object_hasOwnProperty(script_ctx_t *ctx, vdisp_t *jsthis, WORD flags, unsigned argc, jsval_t *argv,
        jsval_t *r)
{
    jsstr_t *name;
    DISPID id;
    BSTR bstr;
    HRESULT hres;

    TRACE("\n");

    if(!argc) {
        if(r)
            *r = jsval_bool(FALSE);
        return S_OK;
    }

    hres = to_string(ctx, argv[0], &name);
    if(FAILED(hres))
        return hres;

    if(is_jsdisp(jsthis)) {
        property_desc_t prop_desc;
        const WCHAR *name_str;

        name_str = jsstr_flatten(name);
        if(!name_str) {
            jsstr_release(name);
            return E_OUTOFMEMORY;
        }

        hres = jsdisp_get_own_property(jsthis->u.jsdisp, name_str, TRUE, &prop_desc);
        jsstr_release(name);
        if(FAILED(hres) && hres != DISP_E_UNKNOWNNAME)
            return hres;

        if(r) *r = jsval_bool(hres == S_OK);
        return S_OK;
    }


    bstr = SysAllocStringLen(NULL, jsstr_length(name));
    if(bstr)
        jsstr_flush(name, bstr);
    jsstr_release(name);
    if(!bstr)
        return E_OUTOFMEMORY;

    if(is_dispex(jsthis))
        hres = IDispatchEx_GetDispID(jsthis->u.dispex, bstr, make_grfdex(ctx, fdexNameCaseSensitive), &id);
    else
        hres = IDispatch_GetIDsOfNames(jsthis->u.disp, &IID_NULL, &bstr, 1, ctx->lcid, &id);

    SysFreeString(bstr);
    if(r)
        *r = jsval_bool(SUCCEEDED(hres));
    return S_OK;
}

static HRESULT Object_propertyIsEnumerable(script_ctx_t *ctx, vdisp_t *jsthis, WORD flags, unsigned argc, jsval_t *argv,
        jsval_t *r)
{
    property_desc_t prop_desc;
    const WCHAR *name;
    jsstr_t *name_str;
    HRESULT hres;

    TRACE("\n");

    if(argc != 1) {
        FIXME("argc %d not supported\n", argc);
        return E_NOTIMPL;
    }

    if(!is_jsdisp(jsthis)) {
        FIXME("Host object this\n");
        return E_FAIL;
    }

    hres = to_flat_string(ctx, argv[0], &name_str, &name);
    if(FAILED(hres))
        return hres;

    hres = jsdisp_get_own_property(jsthis->u.jsdisp, name, TRUE, &prop_desc);
    jsstr_release(name_str);
    if(FAILED(hres) && hres != DISP_E_UNKNOWNNAME)
        return hres;

    if(r)
        *r = jsval_bool(hres == S_OK && (prop_desc.flags & PROPF_ENUMERABLE) != 0);
    return S_OK;
}

static HRESULT Object_isPrototypeOf(script_ctx_t *ctx, vdisp_t *jsthis, WORD flags, unsigned argc, jsval_t *argv,
        jsval_t *r)
{
    FIXME("\n");
    return E_NOTIMPL;
}

static HRESULT Object_get_value(script_ctx_t *ctx, jsdisp_t *jsthis, jsval_t *r)
{
    jsstr_t *ret;

    TRACE("\n");

    ret = jsstr_alloc(L"[object Object]");
    if(!ret)
        return E_OUTOFMEMORY;

    *r = jsval_string(ret);
    return S_OK;
}

static void Object_destructor(jsdisp_t *dispex)
{
    heap_free(dispex);
}

static const builtin_prop_t Object_props[] = {
    {L"hasOwnProperty",        Object_hasOwnProperty,        PROPF_METHOD|1},
    {L"isPrototypeOf",         Object_isPrototypeOf,         PROPF_METHOD|1},
    {L"propertyIsEnumerable",  Object_propertyIsEnumerable,  PROPF_METHOD|1},
    {L"toLocaleString",        Object_toLocaleString,        PROPF_METHOD},
    {L"toString",              Object_toString,              PROPF_METHOD},
    {L"valueOf",               Object_valueOf,               PROPF_METHOD}
};

static const builtin_info_t Object_info = {
    JSCLASS_OBJECT,
    {NULL, NULL,0, Object_get_value},
    ARRAY_SIZE(Object_props),
    Object_props,
    Object_destructor,
    NULL
};

static const builtin_info_t ObjectInst_info = {
    JSCLASS_OBJECT,
    {NULL, NULL,0, Object_get_value},
    0, NULL,
    Object_destructor,
    NULL
};

static void release_property_descriptor(property_desc_t *desc)
{
    if(desc->explicit_value)
        jsval_release(desc->value);
    if(desc->getter)
        jsdisp_release(desc->getter);
    if(desc->setter)
        jsdisp_release(desc->setter);
}

static HRESULT to_property_descriptor(script_ctx_t *ctx, jsdisp_t *attr_obj, property_desc_t *desc)
{
    DISPID id;
    jsval_t v;
    BOOL b;
    HRESULT hres;

    memset(desc, 0, sizeof(*desc));
    desc->value = jsval_undefined();

    hres = jsdisp_get_id(attr_obj, L"enumerable", 0, &id);
    if(SUCCEEDED(hres)) {
        desc->mask |= PROPF_ENUMERABLE;
        hres = jsdisp_propget(attr_obj, id, &v);
        if(FAILED(hres))
            return hres;
        hres = to_boolean(v, &b);
        jsval_release(v);
        if(FAILED(hres))
            return hres;
        if(b)
            desc->flags |= PROPF_ENUMERABLE;
    }else if(hres != DISP_E_UNKNOWNNAME) {
        return hres;
    }

    hres = jsdisp_get_id(attr_obj, L"configurable", 0, &id);
    if(SUCCEEDED(hres)) {
        desc->mask |= PROPF_CONFIGURABLE;
        hres = jsdisp_propget(attr_obj, id, &v);
        if(FAILED(hres))
            return hres;
        hres = to_boolean(v, &b);
        jsval_release(v);
        if(FAILED(hres))
            return hres;
        if(b)
            desc->flags |= PROPF_CONFIGURABLE;
    }else if(hres != DISP_E_UNKNOWNNAME) {
        return hres;
    }

    hres = jsdisp_get_id(attr_obj, L"value", 0, &id);
    if(SUCCEEDED(hres)) {
        hres = jsdisp_propget(attr_obj, id, &desc->value);
        if(FAILED(hres))
            return hres;
        desc->explicit_value = TRUE;
    }else if(hres != DISP_E_UNKNOWNNAME) {
        return hres;
    }

    hres = jsdisp_get_id(attr_obj, L"writable", 0, &id);
    if(SUCCEEDED(hres)) {
        desc->mask |= PROPF_WRITABLE;
        hres = jsdisp_propget(attr_obj, id, &v);
        if(SUCCEEDED(hres)) {
            hres = to_boolean(v, &b);
            jsval_release(v);
            if(SUCCEEDED(hres) && b)
                desc->flags |= PROPF_WRITABLE;
        }
    }else if(hres == DISP_E_UNKNOWNNAME) {
        hres = S_OK;
    }
    if(FAILED(hres)) {
        release_property_descriptor(desc);
        return hres;
    }

    hres = jsdisp_get_id(attr_obj, L"get", 0, &id);
    if(SUCCEEDED(hres)) {
        desc->explicit_getter = TRUE;
        hres = jsdisp_propget(attr_obj, id, &v);
        if(SUCCEEDED(hres) && !is_undefined(v)) {
            if(!is_object_instance(v)) {
                FIXME("getter is not an object\n");
                jsval_release(v);
                hres = E_FAIL;
            }else {
                /* FIXME: Check IsCallable */
                desc->getter = to_jsdisp(get_object(v));
                if(!desc->getter)
                    FIXME("getter is not JS object\n");
            }
        }
    }else if(hres == DISP_E_UNKNOWNNAME) {
        hres = S_OK;
    }
    if(FAILED(hres)) {
        release_property_descriptor(desc);
        return hres;
    }

    hres = jsdisp_get_id(attr_obj, L"set", 0, &id);
    if(SUCCEEDED(hres)) {
        desc->explicit_setter = TRUE;
        hres = jsdisp_propget(attr_obj, id, &v);
        if(SUCCEEDED(hres) && !is_undefined(v)) {
            if(!is_object_instance(v)) {
                FIXME("setter is not an object\n");
                jsval_release(v);
                hres = E_FAIL;
            }else {
                /* FIXME: Check IsCallable */
                desc->setter = to_jsdisp(get_object(v));
                if(!desc->setter)
                    FIXME("setter is not JS object\n");
            }
        }
    }else if(hres == DISP_E_UNKNOWNNAME) {
        hres = S_OK;
    }
    if(FAILED(hres)) {
        release_property_descriptor(desc);
        return hres;
    }

    if(desc->explicit_getter || desc->explicit_setter) {
        if(desc->explicit_value)
            hres = JS_E_PROP_DESC_MISMATCH;
        else if(desc->mask & PROPF_WRITABLE)
            hres = JS_E_INVALID_WRITABLE_PROP_DESC;
    }

    if(FAILED(hres))
        release_property_descriptor(desc);
    return hres;
}

static HRESULT jsdisp_define_properties(script_ctx_t *ctx, jsdisp_t *obj, jsval_t list_val)
{
    DISPID id = DISPID_STARTENUM;
    property_desc_t prop_desc;
    IDispatch *list_disp;
    jsdisp_t *list_obj, *desc_obj;
    jsval_t desc_val;
    BSTR name;
    HRESULT hres;

    hres = to_object(ctx, list_val, &list_disp);
    if(FAILED(hres))
        return hres;

    if(!(list_obj = to_jsdisp(list_disp))) {
        FIXME("non-JS list obj\n");
        IDispatch_Release(list_disp);
        return E_NOTIMPL;
    }

    while(1) {
        hres = jsdisp_next_prop(list_obj, id, TRUE, &id);
        if(hres != S_OK)
            break;

        hres = jsdisp_propget(list_obj, id, &desc_val);
        if(FAILED(hres))
            break;

        if(!is_object_instance(desc_val) || !get_object(desc_val) || !(desc_obj = to_jsdisp(get_object(desc_val)))) {
            jsval_release(desc_val);
            break;
        }

        hres = to_property_descriptor(ctx, desc_obj, &prop_desc);
        jsdisp_release(desc_obj);
        if(FAILED(hres))
            break;

        hres = IDispatchEx_GetMemberName(&list_obj->IDispatchEx_iface, id, &name);
        if(SUCCEEDED(hres))
            hres = jsdisp_define_property(obj, name, &prop_desc);
        release_property_descriptor(&prop_desc);
        if(FAILED(hres))
            break;
    }

    jsdisp_release(list_obj);
    return FAILED(hres) ? hres : S_OK;
}

static HRESULT Object_defineProperty(script_ctx_t *ctx, vdisp_t *jsthis, WORD flags,
                                     unsigned argc, jsval_t *argv, jsval_t *r)
{
    property_desc_t prop_desc;
    jsdisp_t *obj, *attr_obj;
    const WCHAR *name;
    jsstr_t *name_str;
    HRESULT hres;

    TRACE("\n");

    if(argc < 1 || !is_object_instance(argv[0]))
        return JS_E_OBJECT_EXPECTED;
    obj = to_jsdisp(get_object(argv[0]));
    if(!obj) {
        FIXME("not implemented non-JS object\n");
        return E_NOTIMPL;
    }

    hres = to_flat_string(ctx, argc >= 2 ? argv[1] : jsval_undefined(), &name_str, &name);
    if(FAILED(hres))
        return hres;

    if(argc >= 3 && is_object_instance(argv[2])) {
        attr_obj = to_jsdisp(get_object(argv[2]));
        if(attr_obj) {
            hres = to_property_descriptor(ctx, attr_obj, &prop_desc);
        }else {
            FIXME("not implemented non-JS object\n");
            hres = E_NOTIMPL;
        }
    }else {
        hres = JS_E_OBJECT_EXPECTED;
    }
    jsstr_release(name_str);
    if(FAILED(hres))
        return hres;

    hres = jsdisp_define_property(obj, name, &prop_desc);
    release_property_descriptor(&prop_desc);
    if(SUCCEEDED(hres) && r)
        *r = jsval_obj(jsdisp_addref(obj));
    return hres;
}

static HRESULT Object_defineProperties(script_ctx_t *ctx, vdisp_t *jsthis, WORD flags,
                                     unsigned argc, jsval_t *argv, jsval_t *r)
{
    jsdisp_t *obj;
    HRESULT hres;

    if(argc < 1 || !is_object_instance(argv[0]) || !get_object(argv[0]) || !(obj = to_jsdisp(get_object(argv[0])))) {
        FIXME("not an object\n");
        return E_NOTIMPL;
    }

    TRACE("%p\n", obj);

    hres = jsdisp_define_properties(ctx, obj, argc >= 2 ? argv[1] : jsval_undefined());
    if(SUCCEEDED(hres) && r)
        *r = jsval_obj(jsdisp_addref(obj));
    return hres;
}

static HRESULT Object_getOwnPropertyDescriptor(script_ctx_t *ctx, vdisp_t *jsthis, WORD flags,
                                               unsigned argc, jsval_t *argv, jsval_t *r)
{
    property_desc_t prop_desc;
    jsdisp_t *obj, *desc_obj;
    const WCHAR *name;
    jsstr_t *name_str;
    HRESULT hres;

    TRACE("\n");

    if(argc < 1 || !is_object_instance(argv[0]))
        return JS_E_OBJECT_EXPECTED;
    obj = to_jsdisp(get_object(argv[0]));
    if(!obj) {
        FIXME("not implemented non-JS object\n");
        return E_NOTIMPL;
    }

    hres = to_flat_string(ctx, argc >= 2 ? argv[1] : jsval_undefined(), &name_str, &name);
    if(FAILED(hres))
        return hres;

    hres = jsdisp_get_own_property(obj, name, FALSE, &prop_desc);
    jsstr_release(name_str);
    if(hres == DISP_E_UNKNOWNNAME) {
        if(r) *r = jsval_undefined();
        return S_OK;
    }
    if(FAILED(hres))
        return hres;

    hres = create_object(ctx, NULL, &desc_obj);
    if(FAILED(hres))
        return hres;

    if(prop_desc.explicit_getter || prop_desc.explicit_setter) {
        hres = jsdisp_define_data_property(desc_obj, L"get", PROPF_ALL,
                prop_desc.getter ? jsval_obj(prop_desc.getter) : jsval_undefined());
        if(SUCCEEDED(hres))
            hres = jsdisp_define_data_property(desc_obj, L"set", PROPF_ALL,
                    prop_desc.setter ? jsval_obj(prop_desc.setter) : jsval_undefined());
    }else {
        hres = jsdisp_propput_name(desc_obj, L"value", prop_desc.value);
        if(SUCCEEDED(hres))
            hres = jsdisp_define_data_property(desc_obj, L"writable", PROPF_ALL,
                    jsval_bool(!!(prop_desc.flags & PROPF_WRITABLE)));
    }
    if(SUCCEEDED(hres))
        hres = jsdisp_define_data_property(desc_obj, L"enumerable", PROPF_ALL,
                jsval_bool(!!(prop_desc.flags & PROPF_ENUMERABLE)));
    if(SUCCEEDED(hres))
        hres = jsdisp_define_data_property(desc_obj, L"configurable", PROPF_ALL,
                jsval_bool(!!(prop_desc.flags & PROPF_CONFIGURABLE)));

    release_property_descriptor(&prop_desc);
    if(SUCCEEDED(hres) && r)
        *r = jsval_obj(desc_obj);
    else
        jsdisp_release(desc_obj);
    return hres;
}

static HRESULT Object_create(script_ctx_t *ctx, vdisp_t *jsthis, WORD flags,
                             unsigned argc, jsval_t *argv, jsval_t *r)
{
    jsdisp_t *proto = NULL, *obj;
    HRESULT hres;

    if(!argc || (!is_object_instance(argv[0]) && !is_null(argv[0]))) {
        FIXME("Invalid arg\n");
        return E_INVALIDARG;
    }

    TRACE("(%s)\n", debugstr_jsval(argv[0]));

    if(argc && is_object_instance(argv[0])) {
        if(get_object(argv[0]))
            proto = to_jsdisp(get_object(argv[0]));
        if(!proto) {
            FIXME("Non-JS prototype\n");
            return E_NOTIMPL;
        }
    }else if(!is_null(argv[0])) {
        FIXME("Invalid arg %s\n", debugstr_jsval(argc ? argv[0] : jsval_undefined()));
        return E_INVALIDARG;
    }

    hres = create_dispex(ctx, &ObjectInst_info, proto, &obj);
    if(FAILED(hres))
        return hres;

    if(argc >= 2 && !is_undefined(argv[1]))
        hres = jsdisp_define_properties(ctx, obj, argv[1]);

    if(SUCCEEDED(hres) && r)
        *r = jsval_obj(obj);
    else
        jsdisp_release(obj);
    return hres;
}

static HRESULT Object_getPrototypeOf(script_ctx_t *ctx, vdisp_t *jsthis, WORD flags,
                                     unsigned argc, jsval_t *argv, jsval_t *r)
{
    jsdisp_t *obj;

    if(!argc || !is_object_instance(argv[0])) {
        FIXME("invalid arguments\n");
        return E_NOTIMPL;
    }

    TRACE("(%s)\n", debugstr_jsval(argv[0]));

    obj = to_jsdisp(get_object(argv[0]));
    if(!obj) {
        FIXME("Non-JS object\n");
        return E_NOTIMPL;
    }

    if(r)
        *r = obj->prototype
            ? jsval_obj(jsdisp_addref(obj->prototype))
            : jsval_null();
    return S_OK;
}

static HRESULT Object_keys(script_ctx_t *ctx, vdisp_t *jsthis, WORD flags,
                           unsigned argc, jsval_t *argv, jsval_t *r)
{
    DISPID id = DISPID_STARTENUM;
    jsdisp_t *obj, *array;
    unsigned i = 0;
    jsstr_t *key;
    HRESULT hres;

    if(!argc || !is_object_instance(argv[0])) {
        FIXME("invalid arguments %s\n", debugstr_jsval(argv[0]));
        return E_NOTIMPL;
    }

    TRACE("(%s)\n", debugstr_jsval(argv[0]));

    obj = to_jsdisp(get_object(argv[0]));
    if(!obj) {
        FIXME("Non-JS object\n");
        return E_NOTIMPL;
    }

    hres = create_array(ctx, 0, &array);
    if(FAILED(hres))
        return hres;

    do {
        hres = jsdisp_next_prop(obj, id, TRUE, &id);
        if(hres != S_OK)
            break;

        hres = jsdisp_get_prop_name(obj, id, &key);
        if(FAILED(hres))
            break;

        hres = jsdisp_propput_idx(array, i++, jsval_string(key));
        jsstr_release(key);
    } while(hres == S_OK);

    if(SUCCEEDED(hres) && r)
        *r = jsval_obj(array);
    else
        jsdisp_release(array);
    return hres;
}

static HRESULT Object_preventExtensions(script_ctx_t *ctx, vdisp_t *jsthis, WORD flags, unsigned argc, jsval_t *argv, jsval_t *r)
{
    jsdisp_t *obj;

    if(!argc || !is_object_instance(argv[0]) || !get_object(argv[0])) {
        FIXME("invalid arguments\n");
        return E_NOTIMPL;
    }

    FIXME("(%s) semi-stub\n", debugstr_jsval(argv[0]));

    obj = to_jsdisp(get_object(argv[0]));
    if(!obj) {
        FIXME("Non-JS object\n");
        return E_NOTIMPL;
    }

    if(r) *r = jsval_obj(jsdisp_addref(obj));
    return S_OK;
}

static const builtin_prop_t ObjectConstr_props[] = {
    {L"create",                   Object_create,                      PROPF_ES5|PROPF_METHOD|2},
    {L"defineProperties",         Object_defineProperties,            PROPF_ES5|PROPF_METHOD|2},
    {L"defineProperty",           Object_defineProperty,              PROPF_ES5|PROPF_METHOD|2},
    {L"getOwnPropertyDescriptor", Object_getOwnPropertyDescriptor,    PROPF_ES5|PROPF_METHOD|2},
    {L"getPrototypeOf",           Object_getPrototypeOf,              PROPF_ES5|PROPF_METHOD|1},
    {L"keys",                     Object_keys,                        PROPF_ES5|PROPF_METHOD|1},
    {L"preventExtensions",        Object_preventExtensions,           PROPF_ES5|PROPF_METHOD|1},
};

static const builtin_info_t ObjectConstr_info = {
    JSCLASS_FUNCTION,
    DEFAULT_FUNCTION_VALUE,
    ARRAY_SIZE(ObjectConstr_props),
    ObjectConstr_props,
    NULL,
    NULL
};

static HRESULT ObjectConstr_value(script_ctx_t *ctx, vdisp_t *jsthis, WORD flags, unsigned argc, jsval_t *argv,
        jsval_t *r)
{
    HRESULT hres;

    TRACE("\n");

    switch(flags) {
    case DISPATCH_METHOD:
    case DISPATCH_CONSTRUCT: {
        jsdisp_t *obj;

        if(argc) {
            if(!is_undefined(argv[0]) && !is_null(argv[0]) && (!is_object_instance(argv[0]) || get_object(argv[0]))) {
                IDispatch *disp;

                hres = to_object(ctx, argv[0], &disp);
                if(FAILED(hres))
                    return hres;

                if(r)
                    *r = jsval_disp(disp);
                else
                    IDispatch_Release(disp);
                return S_OK;
            }
        }

        hres = create_object(ctx, NULL, &obj);
        if(FAILED(hres))
            return hres;

        if(r)
            *r = jsval_obj(obj);
        else
            jsdisp_release(obj);
        break;
    }

    default:
        FIXME("unimplemented flags: %x\n", flags);
        return E_NOTIMPL;
    }

    return S_OK;
}

HRESULT create_object_constr(script_ctx_t *ctx, jsdisp_t *object_prototype, jsdisp_t **ret)
{
    return create_builtin_constructor(ctx, ObjectConstr_value, L"Object", &ObjectConstr_info, PROPF_CONSTR,
            object_prototype, ret);
}

HRESULT create_object_prototype(script_ctx_t *ctx, jsdisp_t **ret)
{
    return create_dispex(ctx, &Object_info, NULL, ret);
}

HRESULT create_object(script_ctx_t *ctx, jsdisp_t *constr, jsdisp_t **ret)
{
    jsdisp_t *object;
    HRESULT hres;

    object = heap_alloc_zero(sizeof(jsdisp_t));
    if(!object)
        return E_OUTOFMEMORY;

    hres = init_dispex_from_constr(object, ctx, &ObjectInst_info, constr ? constr : ctx->object_constr);
    if(FAILED(hres)) {
        heap_free(object);
        return hres;
    }

    *ret = object;
    return S_OK;
}
