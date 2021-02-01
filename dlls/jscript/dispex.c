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
#include "engine.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(jscript);

static const GUID GUID_JScriptTypeInfo = {0xc59c6b12,0xf6c1,0x11cf,{0x88,0x35,0x00,0xa0,0xc9,0x11,0xe8,0xb2}};

#define FDEX_VERSION_MASK 0xf0000000
#define GOLDEN_RATIO 0x9E3779B9U

typedef enum {
    PROP_JSVAL,
    PROP_BUILTIN,
    PROP_PROTREF,
    PROP_ACCESSOR,
    PROP_DELETED,
    PROP_IDX
} prop_type_t;

struct _dispex_prop_t {
    WCHAR *name;
    unsigned hash;
    prop_type_t type;
    DWORD flags;

    union {
        jsval_t val;
        const builtin_prop_t *p;
        DWORD ref;
        unsigned idx;
        struct {
            jsdisp_t *getter;
            jsdisp_t *setter;
        } accessor;
    } u;

    int bucket_head;
    int bucket_next;
};

static inline DISPID prop_to_id(jsdisp_t *This, dispex_prop_t *prop)
{
    return prop - This->props;
}

static inline dispex_prop_t *get_prop(jsdisp_t *This, DISPID id)
{
    if(id < 0 || id >= This->prop_cnt || This->props[id].type == PROP_DELETED)
        return NULL;

    return This->props+id;
}

static inline BOOL is_function_prop(dispex_prop_t *prop)
{
    BOOL ret = FALSE;

    if (is_object_instance(prop->u.val))
    {
        jsdisp_t *jsdisp = iface_to_jsdisp(get_object(prop->u.val));

        if (jsdisp) ret = is_class(jsdisp, JSCLASS_FUNCTION);
        jsdisp_release(jsdisp);
    }
    return ret;
}

static DWORD get_flags(jsdisp_t *This, dispex_prop_t *prop)
{
    if(prop->type == PROP_PROTREF) {
        dispex_prop_t *parent = get_prop(This->prototype, prop->u.ref);
        if(!parent) {
            prop->type = PROP_DELETED;
            return 0;
        }

        return get_flags(This->prototype, parent);
    }

    return prop->flags;
}

static const builtin_prop_t *find_builtin_prop(jsdisp_t *This, const WCHAR *name)
{
    int min = 0, max, i, r;

    max = This->builtin_info->props_cnt-1;
    while(min <= max) {
        i = (min+max)/2;

        r = wcscmp(name, This->builtin_info->props[i].name);
        if(!r) {
            /* Skip prop if it's available only in higher compatibility mode. */
            unsigned version = (This->builtin_info->props[i].flags & PROPF_VERSION_MASK)
                >> PROPF_VERSION_SHIFT;
            if(version && version > This->ctx->version)
                return NULL;

            /* Skip prop if it's available only in HTML mode and we're not running in HTML mode. */
            if((This->builtin_info->props[i].flags & PROPF_HTML) && !This->ctx->html_mode)
                return NULL;

            return This->builtin_info->props + i;
        }

        if(r < 0)
            max = i-1;
        else
            min = i+1;
    }

    return NULL;
}

static inline unsigned string_hash(const WCHAR *name)
{
    unsigned h = 0;
    for(; *name; name++)
        h = (h>>(sizeof(unsigned)*8-4)) ^ (h<<4) ^ towlower(*name);
    return h;
}

static inline unsigned get_props_idx(jsdisp_t *This, unsigned hash)
{
    return (hash*GOLDEN_RATIO) & (This->buf_size-1);
}

static inline HRESULT resize_props(jsdisp_t *This)
{
    dispex_prop_t *props;
    int i, bucket;

    if(This->buf_size != This->prop_cnt)
        return S_FALSE;

    props = heap_realloc(This->props, sizeof(dispex_prop_t)*This->buf_size*2);
    if(!props)
        return E_OUTOFMEMORY;
    This->buf_size *= 2;
    This->props = props;

    for(i=0; i<This->buf_size; i++) {
        This->props[i].bucket_head = 0;
        This->props[i].bucket_next = 0;
    }

    for(i=1; i<This->prop_cnt; i++) {
        props = This->props+i;

        bucket = get_props_idx(This, props->hash);
        props->bucket_next = This->props[bucket].bucket_head;
        This->props[bucket].bucket_head = i;
    }

    return S_OK;
}

static inline dispex_prop_t* alloc_prop(jsdisp_t *This, const WCHAR *name, prop_type_t type, DWORD flags)
{
    dispex_prop_t *prop;
    unsigned bucket;

    if(FAILED(resize_props(This)))
        return NULL;

    prop = &This->props[This->prop_cnt];
    prop->name = heap_strdupW(name);
    if(!prop->name)
        return NULL;
    prop->type = type;
    prop->flags = flags;
    prop->hash = string_hash(name);

    bucket = get_props_idx(This, prop->hash);
    prop->bucket_next = This->props[bucket].bucket_head;
    This->props[bucket].bucket_head = This->prop_cnt++;
    return prop;
}

static dispex_prop_t *alloc_protref(jsdisp_t *This, const WCHAR *name, DWORD ref)
{
    dispex_prop_t *ret;

    ret = alloc_prop(This, name, PROP_PROTREF, 0);
    if(!ret)
        return NULL;

    ret->u.ref = ref;
    return ret;
}

static HRESULT find_prop_name(jsdisp_t *This, unsigned hash, const WCHAR *name, dispex_prop_t **ret)
{
    const builtin_prop_t *builtin;
    unsigned bucket, pos, prev = 0;
    dispex_prop_t *prop;

    bucket = get_props_idx(This, hash);
    pos = This->props[bucket].bucket_head;
    while(pos != 0) {
        if(!wcscmp(name, This->props[pos].name)) {
            if(prev != 0) {
                This->props[prev].bucket_next = This->props[pos].bucket_next;
                This->props[pos].bucket_next = This->props[bucket].bucket_head;
                This->props[bucket].bucket_head = pos;
            }

            *ret = &This->props[pos];
            return S_OK;
        }

        prev = pos;
        pos = This->props[pos].bucket_next;
    }

    builtin = find_builtin_prop(This, name);
    if(builtin) {
        unsigned flags = builtin->flags;
        if(flags & PROPF_METHOD)
            flags |= PROPF_WRITABLE | PROPF_CONFIGURABLE;
        else if(builtin->setter)
            flags |= PROPF_WRITABLE;
        flags &= PROPF_ENUMERABLE | PROPF_WRITABLE | PROPF_CONFIGURABLE;
        prop = alloc_prop(This, name, PROP_BUILTIN, flags);
        if(!prop)
            return E_OUTOFMEMORY;

        prop->u.p = builtin;
        *ret = prop;
        return S_OK;
    }

    if(This->builtin_info->idx_length) {
        const WCHAR *ptr;
        unsigned idx = 0;

        for(ptr = name; is_digit(*ptr) && idx < 0x10000; ptr++)
            idx = idx*10 + (*ptr-'0');
        if(!*ptr && idx < This->builtin_info->idx_length(This)) {
            prop = alloc_prop(This, name, PROP_IDX, This->builtin_info->idx_put ? PROPF_WRITABLE : 0);
            if(!prop)
                return E_OUTOFMEMORY;

            prop->u.idx = idx;
            *ret = prop;
            return S_OK;
        }
    }

    *ret = NULL;
    return S_OK;
}

static HRESULT find_prop_name_prot(jsdisp_t *This, unsigned hash, const WCHAR *name, dispex_prop_t **ret)
{
    dispex_prop_t *prop, *del=NULL;
    HRESULT hres;

    hres = find_prop_name(This, hash, name, &prop);
    if(FAILED(hres))
        return hres;
    if(prop && prop->type==PROP_DELETED) {
        del = prop;
    } else if(prop) {
        *ret = prop;
        return S_OK;
    }

    if(This->prototype) {
        hres = find_prop_name_prot(This->prototype, hash, name, &prop);
        if(FAILED(hres))
            return hres;
        if(prop) {
            if(del) {
                del->type = PROP_PROTREF;
                del->u.ref = prop - This->prototype->props;
                prop = del;
            }else {
                prop = alloc_protref(This, prop->name, prop - This->prototype->props);
                if(!prop)
                    return E_OUTOFMEMORY;
            }

            *ret = prop;
            return S_OK;
        }
    }

    *ret = del;
    return S_OK;
}

static HRESULT ensure_prop_name(jsdisp_t *This, const WCHAR *name, DWORD create_flags, dispex_prop_t **ret)
{
    dispex_prop_t *prop;
    HRESULT hres;

    hres = find_prop_name_prot(This, string_hash(name), name, &prop);
    if(SUCCEEDED(hres) && (!prop || prop->type == PROP_DELETED)) {
        TRACE("creating prop %s flags %x\n", debugstr_w(name), create_flags);

        if(prop) {
            prop->type = PROP_JSVAL;
            prop->flags = create_flags;
            prop->u.val = jsval_undefined();
        }else {
            prop = alloc_prop(This, name, PROP_JSVAL, create_flags);
            if(!prop)
                return E_OUTOFMEMORY;
        }

        prop->u.val = jsval_undefined();
    }

    *ret = prop;
    return hres;
}

static IDispatch *get_this(DISPPARAMS *dp)
{
    DWORD i;

    for(i=0; i < dp->cNamedArgs; i++) {
        if(dp->rgdispidNamedArgs[i] == DISPID_THIS) {
            if(V_VT(dp->rgvarg+i) == VT_DISPATCH)
                return V_DISPATCH(dp->rgvarg+i);

            WARN("This is not VT_DISPATCH\n");
            return NULL;
        }
    }

    TRACE("no this passed\n");
    return NULL;
}

static HRESULT convert_params(const DISPPARAMS *dp, jsval_t *buf, unsigned *argc, jsval_t **ret)
{
    jsval_t *argv;
    unsigned cnt;
    unsigned i;
    HRESULT hres;

    cnt = dp->cArgs - dp->cNamedArgs;

    if(cnt > 6) {
        argv = heap_alloc(cnt * sizeof(*argv));
        if(!argv)
            return E_OUTOFMEMORY;
    }else {
        argv = buf;
    }

    for(i = 0; i < cnt; i++) {
        hres = variant_to_jsval(dp->rgvarg+dp->cArgs-i-1, argv+i);
        if(FAILED(hres)) {
            while(i--)
                jsval_release(argv[i]);
            if(argv != buf)
                heap_free(argv);
            return hres;
        }
    }

    *argc = cnt;
    *ret = argv;
    return S_OK;
}

static HRESULT invoke_prop_func(jsdisp_t *This, IDispatch *jsthis, dispex_prop_t *prop, WORD flags,
        unsigned argc, jsval_t *argv, jsval_t *r, IServiceProvider *caller)
{
    HRESULT hres;

    switch(prop->type) {
    case PROP_BUILTIN: {
        if(flags == DISPATCH_CONSTRUCT && (prop->flags & PROPF_METHOD)) {
            WARN("%s is not a constructor\n", debugstr_w(prop->name));
            return E_INVALIDARG;
        }

        if(prop->name || This->builtin_info->class != JSCLASS_FUNCTION) {
            vdisp_t vthis;

            if(This->builtin_info->class != JSCLASS_FUNCTION && prop->u.p->invoke != JSGlobal_eval)
                flags &= ~DISPATCH_JSCRIPT_INTERNAL_MASK;
            if(jsthis)
                set_disp(&vthis, jsthis);
            else
                set_jsdisp(&vthis, This);
            hres = prop->u.p->invoke(This->ctx, &vthis, flags, argc, argv, r);
            vdisp_release(&vthis);
        }else {
            /* Function object calls are special case */
            hres = Function_invoke(This, jsthis, flags, argc, argv, r);
        }
        return hres;
    }
    case PROP_PROTREF:
        return invoke_prop_func(This->prototype, jsthis ? jsthis : (IDispatch *)&This->IDispatchEx_iface,
                                This->prototype->props+prop->u.ref, flags, argc, argv, r, caller);
    case PROP_JSVAL: {
        if(!is_object_instance(prop->u.val)) {
            FIXME("invoke %s\n", debugstr_jsval(prop->u.val));
            return E_FAIL;
        }

        TRACE("call %s %p\n", debugstr_w(prop->name), get_object(prop->u.val));

        return disp_call_value(This->ctx, get_object(prop->u.val),
                               jsthis ? jsthis : (IDispatch*)&This->IDispatchEx_iface,
                               flags, argc, argv, r);
    }
    case PROP_ACCESSOR:
        FIXME("accessor\n");
        return E_NOTIMPL;
    case PROP_IDX:
        FIXME("Invoking PROP_IDX not yet supported\n");
        return E_NOTIMPL;
    case PROP_DELETED:
        assert(0);
    }

    assert(0);
    return E_FAIL;
}

static HRESULT prop_get(jsdisp_t *This, dispex_prop_t *prop,  jsval_t *r)
{
    jsdisp_t *prop_obj = This;
    HRESULT hres;

    while(prop->type == PROP_PROTREF) {
        prop_obj = prop_obj->prototype;
        prop = prop_obj->props + prop->u.ref;
    }

    switch(prop->type) {
    case PROP_BUILTIN:
        if(prop->u.p->getter) {
            hres = prop->u.p->getter(This->ctx, This, r);
        }else {
            jsdisp_t *obj;

            assert(prop->u.p->invoke != NULL);
            hres = create_builtin_function(This->ctx, prop->u.p->invoke, prop->u.p->name, NULL,
                    prop->u.p->flags, NULL, &obj);
            if(FAILED(hres))
                break;

            prop->type = PROP_JSVAL;
            prop->u.val = jsval_obj(obj);

            jsdisp_addref(obj);
            *r = jsval_obj(obj);
        }
        break;
    case PROP_JSVAL:
        hres = jsval_copy(prop->u.val, r);
        break;
    case PROP_ACCESSOR:
        if(prop->u.accessor.getter) {
            hres = jsdisp_call_value(prop->u.accessor.getter, to_disp(This),
                                     DISPATCH_METHOD, 0, NULL, r);
        }else {
            *r = jsval_undefined();
            hres = S_OK;
        }
        break;
    case PROP_IDX:
        hres = prop_obj->builtin_info->idx_get(prop_obj, prop->u.idx, r);
        break;
    default:
        ERR("type %d\n", prop->type);
        return E_FAIL;
    }

    if(FAILED(hres)) {
        TRACE("fail %08x\n", hres);
        return hres;
    }

    TRACE("%s ret %s\n", debugstr_w(prop->name), debugstr_jsval(*r));
    return hres;
}

static HRESULT prop_put(jsdisp_t *This, dispex_prop_t *prop, jsval_t val)
{
    HRESULT hres;

    if(prop->type == PROP_PROTREF) {
        dispex_prop_t *prop_iter = prop;
        jsdisp_t *prototype_iter = This;

        do {
            prototype_iter = prototype_iter->prototype;
            prop_iter = prototype_iter->props + prop_iter->u.ref;
        } while(prop_iter->type == PROP_PROTREF);

        if(prop_iter->type == PROP_ACCESSOR)
            prop = prop_iter;
    }

    switch(prop->type) {
    case PROP_BUILTIN:
        if(!prop->u.p->setter) {
            TRACE("getter with no setter\n");
            return S_OK;
        }
        return prop->u.p->setter(This->ctx, This, val);
    case PROP_PROTREF:
    case PROP_DELETED:
        prop->type = PROP_JSVAL;
        prop->flags = PROPF_ENUMERABLE | PROPF_CONFIGURABLE | PROPF_WRITABLE;
        prop->u.val = jsval_undefined();
        break;
    case PROP_JSVAL:
        if(!(prop->flags & PROPF_WRITABLE))
            return S_OK;

        jsval_release(prop->u.val);
        break;
    case PROP_ACCESSOR:
        if(!prop->u.accessor.setter) {
            TRACE("no setter\n");
            return S_OK;
        }
        return jsdisp_call_value(prop->u.accessor.setter, to_disp(This), DISPATCH_METHOD, 1, &val, NULL);
    case PROP_IDX:
        if(!This->builtin_info->idx_put) {
            TRACE("no put_idx\n");
            return S_OK;
        }
        return This->builtin_info->idx_put(This, prop->u.idx, val);
    default:
        ERR("type %d\n", prop->type);
        return E_FAIL;
    }

    TRACE("%s = %s\n", debugstr_w(prop->name), debugstr_jsval(val));

    hres = jsval_copy(val, &prop->u.val);
    if(FAILED(hres))
        return hres;

    if(This->builtin_info->on_put)
        This->builtin_info->on_put(This, prop->name);

    return S_OK;
}

HRESULT builtin_set_const(script_ctx_t *ctx, jsdisp_t *jsthis, jsval_t value)
{
    TRACE("%p %s\n", jsthis, debugstr_jsval(value));
    return S_OK;
}

static HRESULT fill_protrefs(jsdisp_t *This)
{
    dispex_prop_t *iter, *prop;
    HRESULT hres;

    if(!This->prototype)
        return S_OK;

    fill_protrefs(This->prototype);

    for(iter = This->prototype->props; iter < This->prototype->props+This->prototype->prop_cnt; iter++) {
        if(!iter->name)
            continue;
        hres = find_prop_name(This, iter->hash, iter->name, &prop);
        if(FAILED(hres))
            return hres;
        if(!prop || prop->type==PROP_DELETED) {
            if(prop) {
                prop->type = PROP_PROTREF;
                prop->flags = 0;
                prop->u.ref = iter - This->prototype->props;
            }else {
                prop = alloc_protref(This, iter->name, iter - This->prototype->props);
                if(!prop)
                    return E_OUTOFMEMORY;
            }
        }
    }

    return S_OK;
}

struct typeinfo_func {
    dispex_prop_t *prop;
    function_code_t *code;
};

typedef struct {
    ITypeInfo ITypeInfo_iface;
    ITypeComp ITypeComp_iface;
    LONG ref;

    UINT num_funcs;
    UINT num_vars;
    struct typeinfo_func *funcs;
    dispex_prop_t **vars;

    jsdisp_t *jsdisp;
} ScriptTypeInfo;

static struct typeinfo_func *get_func_from_memid(const ScriptTypeInfo *typeinfo, MEMBERID memid)
{
    UINT a = 0, b = typeinfo->num_funcs;

    while (a < b)
    {
        UINT i = (a + b - 1) / 2;
        MEMBERID func_memid = prop_to_id(typeinfo->jsdisp, typeinfo->funcs[i].prop);

        if (memid == func_memid)
            return &typeinfo->funcs[i];
        else if (memid < func_memid)
            b = i;
        else
            a = i + 1;
    }
    return NULL;
}

static dispex_prop_t *get_var_from_memid(const ScriptTypeInfo *typeinfo, MEMBERID memid)
{
    UINT a = 0, b = typeinfo->num_vars;

    while (a < b)
    {
        UINT i = (a + b - 1) / 2;
        MEMBERID var_memid = prop_to_id(typeinfo->jsdisp, typeinfo->vars[i]);

        if (memid == var_memid)
            return typeinfo->vars[i];
        else if (memid < var_memid)
            b = i;
        else
            a = i + 1;
    }
    return NULL;
}

static inline ScriptTypeInfo *ScriptTypeInfo_from_ITypeInfo(ITypeInfo *iface)
{
    return CONTAINING_RECORD(iface, ScriptTypeInfo, ITypeInfo_iface);
}

static inline ScriptTypeInfo *ScriptTypeInfo_from_ITypeComp(ITypeComp *iface)
{
    return CONTAINING_RECORD(iface, ScriptTypeInfo, ITypeComp_iface);
}

static HRESULT WINAPI ScriptTypeInfo_QueryInterface(ITypeInfo *iface, REFIID riid, void **ppv)
{
    ScriptTypeInfo *This = ScriptTypeInfo_from_ITypeInfo(iface);

    if (IsEqualGUID(&IID_IUnknown, riid) || IsEqualGUID(&IID_ITypeInfo, riid))
        *ppv = &This->ITypeInfo_iface;
    else if (IsEqualGUID(&IID_ITypeComp, riid))
        *ppv = &This->ITypeComp_iface;
    else
    {
        WARN("(%p)->(%s %p)\n", This, debugstr_guid(riid), ppv);
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    TRACE("(%p)->(%s %p)\n", This, debugstr_guid(riid), ppv);
    IUnknown_AddRef((IUnknown*)*ppv);
    return S_OK;
}

static ULONG WINAPI ScriptTypeInfo_AddRef(ITypeInfo *iface)
{
    ScriptTypeInfo *This = ScriptTypeInfo_from_ITypeInfo(iface);
    LONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref=%d\n", This, ref);

    return ref;
}

static ULONG WINAPI ScriptTypeInfo_Release(ITypeInfo *iface)
{
    ScriptTypeInfo *This = ScriptTypeInfo_from_ITypeInfo(iface);
    LONG ref = InterlockedDecrement(&This->ref);
    UINT i;

    TRACE("(%p) ref=%d\n", This, ref);

    if (!ref)
    {
        for (i = This->num_funcs; i--;)
            release_bytecode(This->funcs[i].code->bytecode);
        IDispatchEx_Release(&This->jsdisp->IDispatchEx_iface);
        heap_free(This->funcs);
        heap_free(This->vars);
        heap_free(This);
    }
    return ref;
}

static HRESULT WINAPI ScriptTypeInfo_GetTypeAttr(ITypeInfo *iface, TYPEATTR **ppTypeAttr)
{
    ScriptTypeInfo *This = ScriptTypeInfo_from_ITypeInfo(iface);
    TYPEATTR *attr;

    TRACE("(%p)->(%p)\n", This, ppTypeAttr);

    if (!ppTypeAttr) return E_INVALIDARG;

    attr = heap_alloc_zero(sizeof(*attr));
    if (!attr) return E_OUTOFMEMORY;

    attr->guid = GUID_JScriptTypeInfo;
    attr->lcid = LOCALE_USER_DEFAULT;
    attr->memidConstructor = MEMBERID_NIL;
    attr->memidDestructor = MEMBERID_NIL;
    attr->cbSizeInstance = 4;
    attr->typekind = TKIND_DISPATCH;
    attr->cFuncs = This->num_funcs;
    attr->cVars = This->num_vars;
    attr->cImplTypes = 1;
    attr->cbSizeVft = sizeof(IDispatchVtbl);
    attr->cbAlignment = 4;
    attr->wTypeFlags = TYPEFLAG_FDISPATCHABLE;
    attr->wMajorVerNum = JSCRIPT_MAJOR_VERSION;
    attr->wMinorVerNum = JSCRIPT_MINOR_VERSION;

    *ppTypeAttr = attr;
    return S_OK;
}

static HRESULT WINAPI ScriptTypeInfo_GetTypeComp(ITypeInfo *iface, ITypeComp **ppTComp)
{
    ScriptTypeInfo *This = ScriptTypeInfo_from_ITypeInfo(iface);

    TRACE("(%p)->(%p)\n", This, ppTComp);

    if (!ppTComp) return E_INVALIDARG;

    *ppTComp = &This->ITypeComp_iface;
    ITypeInfo_AddRef(iface);
    return S_OK;
}

static HRESULT WINAPI ScriptTypeInfo_GetFuncDesc(ITypeInfo *iface, UINT index, FUNCDESC **ppFuncDesc)
{
    ScriptTypeInfo *This = ScriptTypeInfo_from_ITypeInfo(iface);
    struct typeinfo_func *func;
    FUNCDESC *desc;
    unsigned i;

    TRACE("(%p)->(%u %p)\n", This, index, ppFuncDesc);

    if (!ppFuncDesc) return E_INVALIDARG;
    if (index >= This->num_funcs) return TYPE_E_ELEMENTNOTFOUND;
    func = &This->funcs[index];

    /* Store the parameter array after the FUNCDESC structure */
    desc = heap_alloc_zero(sizeof(*desc) + sizeof(ELEMDESC) * func->code->param_cnt);
    if (!desc) return E_OUTOFMEMORY;

    desc->memid = prop_to_id(This->jsdisp, func->prop);
    desc->funckind = FUNC_DISPATCH;
    desc->invkind = INVOKE_FUNC;
    desc->callconv = CC_STDCALL;
    desc->cParams = func->code->param_cnt;
    desc->elemdescFunc.tdesc.vt = VT_VARIANT;

    if (func->code->param_cnt) desc->lprgelemdescParam = (ELEMDESC*)(desc + 1);
    for (i = 0; i < func->code->param_cnt; i++)
        desc->lprgelemdescParam[i].tdesc.vt = VT_VARIANT;

    *ppFuncDesc = desc;
    return S_OK;
}

static HRESULT WINAPI ScriptTypeInfo_GetVarDesc(ITypeInfo *iface, UINT index, VARDESC **ppVarDesc)
{
    ScriptTypeInfo *This = ScriptTypeInfo_from_ITypeInfo(iface);
    VARDESC *desc;

    TRACE("(%p)->(%u %p)\n", This, index, ppVarDesc);

    if (!ppVarDesc) return E_INVALIDARG;
    if (index >= This->num_vars) return TYPE_E_ELEMENTNOTFOUND;

    desc = heap_alloc_zero(sizeof(*desc));
    if (!desc) return E_OUTOFMEMORY;

    desc->memid = prop_to_id(This->jsdisp, This->vars[index]);
    desc->varkind = VAR_DISPATCH;
    desc->elemdescVar.tdesc.vt = VT_VARIANT;

    *ppVarDesc = desc;
    return S_OK;
}

static HRESULT WINAPI ScriptTypeInfo_GetNames(ITypeInfo *iface, MEMBERID memid, BSTR *rgBstrNames,
        UINT cMaxNames, UINT *pcNames)
{
    ScriptTypeInfo *This = ScriptTypeInfo_from_ITypeInfo(iface);
    struct typeinfo_func *func;
    ITypeInfo *disp_typeinfo;
    dispex_prop_t *var;
    HRESULT hr;
    UINT i = 0;

    TRACE("(%p)->(%d %p %u %p)\n", This, memid, rgBstrNames, cMaxNames, pcNames);

    if (!rgBstrNames || !pcNames) return E_INVALIDARG;
    if (memid <= 0) return TYPE_E_ELEMENTNOTFOUND;

    func = get_func_from_memid(This, memid);
    if (!func)
    {
        var = get_var_from_memid(This, memid);
        if (!var)
        {
            hr = get_dispatch_typeinfo(&disp_typeinfo);
            if (FAILED(hr)) return hr;

            return ITypeInfo_GetNames(disp_typeinfo, memid, rgBstrNames, cMaxNames, pcNames);
        }
    }

    *pcNames = 0;
    if (!cMaxNames) return S_OK;

    rgBstrNames[0] = SysAllocString(func ? func->prop->name : var->name);
    if (!rgBstrNames[0]) return E_OUTOFMEMORY;
    i++;

    if (func)
    {
        unsigned num = min(cMaxNames, func->code->param_cnt + 1);

        for (; i < num; i++)
        {
            if (!(rgBstrNames[i] = SysAllocString(func->code->params[i - 1])))
            {
                do SysFreeString(rgBstrNames[--i]); while (i);
                return E_OUTOFMEMORY;
            }
        }
    }

    *pcNames = i;
    return S_OK;
}

static HRESULT WINAPI ScriptTypeInfo_GetRefTypeOfImplType(ITypeInfo *iface, UINT index, HREFTYPE *pRefType)
{
    ScriptTypeInfo *This = ScriptTypeInfo_from_ITypeInfo(iface);

    TRACE("(%p)->(%u %p)\n", This, index, pRefType);

    /* We only inherit from IDispatch */
    if (!pRefType) return E_INVALIDARG;
    if (index != 0) return TYPE_E_ELEMENTNOTFOUND;

    *pRefType = 1;
    return S_OK;
}

static HRESULT WINAPI ScriptTypeInfo_GetImplTypeFlags(ITypeInfo *iface, UINT index, INT *pImplTypeFlags)
{
    ScriptTypeInfo *This = ScriptTypeInfo_from_ITypeInfo(iface);

    TRACE("(%p)->(%u %p)\n", This, index, pImplTypeFlags);

    if (!pImplTypeFlags) return E_INVALIDARG;
    if (index != 0) return TYPE_E_ELEMENTNOTFOUND;

    *pImplTypeFlags = 0;
    return S_OK;
}

static HRESULT WINAPI ScriptTypeInfo_GetIDsOfNames(ITypeInfo *iface, LPOLESTR *rgszNames, UINT cNames,
        MEMBERID *pMemId)
{
    ScriptTypeInfo *This = ScriptTypeInfo_from_ITypeInfo(iface);
    ITypeInfo *disp_typeinfo;
    const WCHAR *name;
    HRESULT hr = S_OK;
    int i, j, arg;

    TRACE("(%p)->(%p %u %p)\n", This, rgszNames, cNames, pMemId);

    if (!rgszNames || !cNames || !pMemId) return E_INVALIDARG;

    for (i = 0; i < cNames; i++) pMemId[i] = MEMBERID_NIL;
    name = rgszNames[0];

    for (i = 0; i < This->num_funcs; i++)
    {
        struct typeinfo_func *func = &This->funcs[i];

        if (wcsicmp(name, func->prop->name)) continue;
        pMemId[0] = prop_to_id(This->jsdisp, func->prop);

        for (j = 1; j < cNames; j++)
        {
            name = rgszNames[j];
            for (arg = func->code->param_cnt; --arg >= 0;)
                if (!wcsicmp(name, func->code->params[arg]))
                    break;
            if (arg >= 0)
                pMemId[j] = arg;
            else
                hr = DISP_E_UNKNOWNNAME;
        }
        return hr;
    }

    for (i = 0; i < This->num_vars; i++)
    {
        dispex_prop_t *var = This->vars[i];

        if (wcsicmp(name, var->name)) continue;
        pMemId[0] = prop_to_id(This->jsdisp, var);
        return S_OK;
    }

    /* Look into the inherited IDispatch */
    hr = get_dispatch_typeinfo(&disp_typeinfo);
    if (FAILED(hr)) return hr;

    return ITypeInfo_GetIDsOfNames(disp_typeinfo, rgszNames, cNames, pMemId);
}

static HRESULT WINAPI ScriptTypeInfo_Invoke(ITypeInfo *iface, PVOID pvInstance, MEMBERID memid, WORD wFlags,
        DISPPARAMS *pDispParams, VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    ScriptTypeInfo *This = ScriptTypeInfo_from_ITypeInfo(iface);
    ITypeInfo *disp_typeinfo;
    IDispatch *disp;
    HRESULT hr;

    TRACE("(%p)->(%p %d %d %p %p %p %p)\n", This, pvInstance, memid, wFlags,
          pDispParams, pVarResult, pExcepInfo, puArgErr);

    if (!pvInstance) return E_INVALIDARG;
    if (memid <= 0) return TYPE_E_ELEMENTNOTFOUND;

    if (!get_func_from_memid(This, memid) && !get_var_from_memid(This, memid))
    {
        hr = get_dispatch_typeinfo(&disp_typeinfo);
        if (FAILED(hr)) return hr;

        return ITypeInfo_Invoke(disp_typeinfo, pvInstance, memid, wFlags, pDispParams,
                                pVarResult, pExcepInfo, puArgErr);
    }

    hr = IUnknown_QueryInterface((IUnknown*)pvInstance, &IID_IDispatch, (void**)&disp);
    if (FAILED(hr)) return hr;

    hr = IDispatch_Invoke(disp, memid, &IID_NULL, LOCALE_USER_DEFAULT, wFlags,
                          pDispParams, pVarResult, pExcepInfo, puArgErr);
    IDispatch_Release(disp);

    return hr;
}

static HRESULT WINAPI ScriptTypeInfo_GetDocumentation(ITypeInfo *iface, MEMBERID memid, BSTR *pBstrName,
        BSTR *pBstrDocString, DWORD *pdwHelpContext, BSTR *pBstrHelpFile)
{
    ScriptTypeInfo *This = ScriptTypeInfo_from_ITypeInfo(iface);
    struct typeinfo_func *func;
    ITypeInfo *disp_typeinfo;
    dispex_prop_t *var;
    HRESULT hr;

    TRACE("(%p)->(%d %p %p %p %p)\n", This, memid, pBstrName, pBstrDocString, pdwHelpContext, pBstrHelpFile);

    if (pBstrDocString) *pBstrDocString = NULL;
    if (pdwHelpContext) *pdwHelpContext = 0;
    if (pBstrHelpFile) *pBstrHelpFile = NULL;

    if (memid == MEMBERID_NIL)
    {
        if (pBstrName && !(*pBstrName = SysAllocString(L"JScriptTypeInfo")))
            return E_OUTOFMEMORY;
        if (pBstrDocString &&
            !(*pBstrDocString = SysAllocString(L"JScript Type Info")))
        {
            if (pBstrName) SysFreeString(*pBstrName);
            return E_OUTOFMEMORY;
        }
        return S_OK;
    }
    if (memid <= 0) return TYPE_E_ELEMENTNOTFOUND;

    func = get_func_from_memid(This, memid);
    if (!func)
    {
        var = get_var_from_memid(This, memid);
        if (!var)
        {
            hr = get_dispatch_typeinfo(&disp_typeinfo);
            if (FAILED(hr)) return hr;

            return ITypeInfo_GetDocumentation(disp_typeinfo, memid, pBstrName, pBstrDocString,
                                              pdwHelpContext, pBstrHelpFile);
        }
    }

    if (pBstrName)
    {
        *pBstrName = SysAllocString(func ? func->prop->name : var->name);

        if (!*pBstrName)
            return E_OUTOFMEMORY;
    }
    return S_OK;
}

static HRESULT WINAPI ScriptTypeInfo_GetDllEntry(ITypeInfo *iface, MEMBERID memid, INVOKEKIND invKind,
        BSTR *pBstrDllName, BSTR *pBstrName, WORD *pwOrdinal)
{
    ScriptTypeInfo *This = ScriptTypeInfo_from_ITypeInfo(iface);
    ITypeInfo *disp_typeinfo;
    HRESULT hr;

    TRACE("(%p)->(%d %d %p %p %p)\n", This, memid, invKind, pBstrDllName, pBstrName, pwOrdinal);

    if (pBstrDllName) *pBstrDllName = NULL;
    if (pBstrName) *pBstrName = NULL;
    if (pwOrdinal) *pwOrdinal = 0;

    if (!get_func_from_memid(This, memid) && !get_var_from_memid(This, memid))
    {
        hr = get_dispatch_typeinfo(&disp_typeinfo);
        if (FAILED(hr)) return hr;

        return ITypeInfo_GetDllEntry(disp_typeinfo, memid, invKind, pBstrDllName, pBstrName, pwOrdinal);
    }
    return TYPE_E_BADMODULEKIND;
}

static HRESULT WINAPI ScriptTypeInfo_GetRefTypeInfo(ITypeInfo *iface, HREFTYPE hRefType, ITypeInfo **ppTInfo)
{
    ScriptTypeInfo *This = ScriptTypeInfo_from_ITypeInfo(iface);
    HRESULT hr;

    TRACE("(%p)->(%x %p)\n", This, hRefType, ppTInfo);

    if (!ppTInfo || (INT)hRefType < 0) return E_INVALIDARG;

    if (hRefType & ~3) return E_FAIL;
    if (hRefType & 1)
    {
        hr = get_dispatch_typeinfo(ppTInfo);
        if (FAILED(hr)) return hr;
    }
    else
        *ppTInfo = iface;

    ITypeInfo_AddRef(*ppTInfo);
    return S_OK;
}

static HRESULT WINAPI ScriptTypeInfo_AddressOfMember(ITypeInfo *iface, MEMBERID memid, INVOKEKIND invKind, PVOID *ppv)
{
    ScriptTypeInfo *This = ScriptTypeInfo_from_ITypeInfo(iface);
    ITypeInfo *disp_typeinfo;
    HRESULT hr;

    TRACE("(%p)->(%d %d %p)\n", This, memid, invKind, ppv);

    if (!ppv) return E_INVALIDARG;
    *ppv = NULL;

    if (!get_func_from_memid(This, memid) && !get_var_from_memid(This, memid))
    {
        hr = get_dispatch_typeinfo(&disp_typeinfo);
        if (FAILED(hr)) return hr;

        return ITypeInfo_AddressOfMember(disp_typeinfo, memid, invKind, ppv);
    }
    return TYPE_E_BADMODULEKIND;
}

static HRESULT WINAPI ScriptTypeInfo_CreateInstance(ITypeInfo *iface, IUnknown *pUnkOuter, REFIID riid, PVOID *ppvObj)
{
    ScriptTypeInfo *This = ScriptTypeInfo_from_ITypeInfo(iface);

    TRACE("(%p)->(%p %s %p)\n", This, pUnkOuter, debugstr_guid(riid), ppvObj);

    if (!ppvObj) return E_INVALIDARG;

    *ppvObj = NULL;
    return TYPE_E_BADMODULEKIND;
}

static HRESULT WINAPI ScriptTypeInfo_GetMops(ITypeInfo *iface, MEMBERID memid, BSTR *pBstrMops)
{
    ScriptTypeInfo *This = ScriptTypeInfo_from_ITypeInfo(iface);
    ITypeInfo *disp_typeinfo;
    HRESULT hr;

    TRACE("(%p)->(%d %p)\n", This, memid, pBstrMops);

    if (!pBstrMops) return E_INVALIDARG;

    if (!get_func_from_memid(This, memid) && !get_var_from_memid(This, memid))
    {
        hr = get_dispatch_typeinfo(&disp_typeinfo);
        if (FAILED(hr)) return hr;

        return ITypeInfo_GetMops(disp_typeinfo, memid, pBstrMops);
    }

    *pBstrMops = NULL;
    return S_OK;
}

static HRESULT WINAPI ScriptTypeInfo_GetContainingTypeLib(ITypeInfo *iface, ITypeLib **ppTLib, UINT *pIndex)
{
    ScriptTypeInfo *This = ScriptTypeInfo_from_ITypeInfo(iface);

    FIXME("(%p)->(%p %p)\n", This, ppTLib, pIndex);

    return E_NOTIMPL;
}

static void WINAPI ScriptTypeInfo_ReleaseTypeAttr(ITypeInfo *iface, TYPEATTR *pTypeAttr)
{
    ScriptTypeInfo *This = ScriptTypeInfo_from_ITypeInfo(iface);

    TRACE("(%p)->(%p)\n", This, pTypeAttr);

    heap_free(pTypeAttr);
}

static void WINAPI ScriptTypeInfo_ReleaseFuncDesc(ITypeInfo *iface, FUNCDESC *pFuncDesc)
{
    ScriptTypeInfo *This = ScriptTypeInfo_from_ITypeInfo(iface);

    TRACE("(%p)->(%p)\n", This, pFuncDesc);

    heap_free(pFuncDesc);
}

static void WINAPI ScriptTypeInfo_ReleaseVarDesc(ITypeInfo *iface, VARDESC *pVarDesc)
{
    ScriptTypeInfo *This = ScriptTypeInfo_from_ITypeInfo(iface);

    TRACE("(%p)->(%p)\n", This, pVarDesc);

    heap_free(pVarDesc);
}

static const ITypeInfoVtbl ScriptTypeInfoVtbl = {
    ScriptTypeInfo_QueryInterface,
    ScriptTypeInfo_AddRef,
    ScriptTypeInfo_Release,
    ScriptTypeInfo_GetTypeAttr,
    ScriptTypeInfo_GetTypeComp,
    ScriptTypeInfo_GetFuncDesc,
    ScriptTypeInfo_GetVarDesc,
    ScriptTypeInfo_GetNames,
    ScriptTypeInfo_GetRefTypeOfImplType,
    ScriptTypeInfo_GetImplTypeFlags,
    ScriptTypeInfo_GetIDsOfNames,
    ScriptTypeInfo_Invoke,
    ScriptTypeInfo_GetDocumentation,
    ScriptTypeInfo_GetDllEntry,
    ScriptTypeInfo_GetRefTypeInfo,
    ScriptTypeInfo_AddressOfMember,
    ScriptTypeInfo_CreateInstance,
    ScriptTypeInfo_GetMops,
    ScriptTypeInfo_GetContainingTypeLib,
    ScriptTypeInfo_ReleaseTypeAttr,
    ScriptTypeInfo_ReleaseFuncDesc,
    ScriptTypeInfo_ReleaseVarDesc
};

static HRESULT WINAPI ScriptTypeComp_QueryInterface(ITypeComp *iface, REFIID riid, void **ppv)
{
    ScriptTypeInfo *This = ScriptTypeInfo_from_ITypeComp(iface);
    return ITypeInfo_QueryInterface(&This->ITypeInfo_iface, riid, ppv);
}

static ULONG WINAPI ScriptTypeComp_AddRef(ITypeComp *iface)
{
    ScriptTypeInfo *This = ScriptTypeInfo_from_ITypeComp(iface);
    return ITypeInfo_AddRef(&This->ITypeInfo_iface);
}

static ULONG WINAPI ScriptTypeComp_Release(ITypeComp *iface)
{
    ScriptTypeInfo *This = ScriptTypeInfo_from_ITypeComp(iface);
    return ITypeInfo_Release(&This->ITypeInfo_iface);
}

static HRESULT WINAPI ScriptTypeComp_Bind(ITypeComp *iface, LPOLESTR szName, ULONG lHashVal, WORD wFlags,
        ITypeInfo **ppTInfo, DESCKIND *pDescKind, BINDPTR *pBindPtr)
{
    ScriptTypeInfo *This = ScriptTypeInfo_from_ITypeComp(iface);
    UINT flags = wFlags ? wFlags : ~0;
    ITypeInfo *disp_typeinfo;
    ITypeComp *disp_typecomp;
    HRESULT hr;
    UINT i;

    TRACE("(%p)->(%s %08x %d %p %p %p)\n", This, debugstr_w(szName), lHashVal,
          wFlags, ppTInfo, pDescKind, pBindPtr);

    if (!szName || !ppTInfo || !pDescKind || !pBindPtr)
        return E_INVALIDARG;

    for (i = 0; i < This->num_funcs; i++)
    {
        if (wcsicmp(szName, This->funcs[i].prop->name)) continue;
        if (!(flags & INVOKE_FUNC)) return TYPE_E_TYPEMISMATCH;

        hr = ITypeInfo_GetFuncDesc(&This->ITypeInfo_iface, i, &pBindPtr->lpfuncdesc);
        if (FAILED(hr)) return hr;

        *pDescKind = DESCKIND_FUNCDESC;
        *ppTInfo = &This->ITypeInfo_iface;
        ITypeInfo_AddRef(*ppTInfo);
        return S_OK;
    }

    for (i = 0; i < This->num_vars; i++)
    {
        if (wcsicmp(szName, This->vars[i]->name)) continue;
        if (!(flags & INVOKE_PROPERTYGET)) return TYPE_E_TYPEMISMATCH;

        hr = ITypeInfo_GetVarDesc(&This->ITypeInfo_iface, i, &pBindPtr->lpvardesc);
        if (FAILED(hr)) return hr;

        *pDescKind = DESCKIND_VARDESC;
        *ppTInfo = &This->ITypeInfo_iface;
        ITypeInfo_AddRef(*ppTInfo);
        return S_OK;
    }

    /* Look into the inherited IDispatch */
    hr = get_dispatch_typeinfo(&disp_typeinfo);
    if (FAILED(hr)) return hr;

    hr = ITypeInfo_GetTypeComp(disp_typeinfo, &disp_typecomp);
    if (FAILED(hr)) return hr;

    hr = ITypeComp_Bind(disp_typecomp, szName, lHashVal, wFlags, ppTInfo, pDescKind, pBindPtr);
    ITypeComp_Release(disp_typecomp);
    return hr;
}

static HRESULT WINAPI ScriptTypeComp_BindType(ITypeComp *iface, LPOLESTR szName, ULONG lHashVal,
        ITypeInfo **ppTInfo, ITypeComp **ppTComp)
{
    ScriptTypeInfo *This = ScriptTypeInfo_from_ITypeComp(iface);
    ITypeInfo *disp_typeinfo;
    ITypeComp *disp_typecomp;
    HRESULT hr;

    TRACE("(%p)->(%s %08x %p %p)\n", This, debugstr_w(szName), lHashVal, ppTInfo, ppTComp);

    if (!szName || !ppTInfo || !ppTComp)
        return E_INVALIDARG;

    /* Look into the inherited IDispatch */
    hr = get_dispatch_typeinfo(&disp_typeinfo);
    if (FAILED(hr)) return hr;

    hr = ITypeInfo_GetTypeComp(disp_typeinfo, &disp_typecomp);
    if (FAILED(hr)) return hr;

    hr = ITypeComp_BindType(disp_typecomp, szName, lHashVal, ppTInfo, ppTComp);
    ITypeComp_Release(disp_typecomp);
    return hr;
}

static const ITypeCompVtbl ScriptTypeCompVtbl = {
    ScriptTypeComp_QueryInterface,
    ScriptTypeComp_AddRef,
    ScriptTypeComp_Release,
    ScriptTypeComp_Bind,
    ScriptTypeComp_BindType
};

static inline jsdisp_t *impl_from_IDispatchEx(IDispatchEx *iface)
{
    return CONTAINING_RECORD(iface, jsdisp_t, IDispatchEx_iface);
}

static HRESULT WINAPI DispatchEx_QueryInterface(IDispatchEx *iface, REFIID riid, void **ppv)
{
    jsdisp_t *This = impl_from_IDispatchEx(iface);

    if(IsEqualGUID(&IID_IUnknown, riid)) {
        TRACE("(%p)->(IID_IUnknown %p)\n", This, ppv);
        *ppv = &This->IDispatchEx_iface;
    }else if(IsEqualGUID(&IID_IDispatch, riid)) {
        TRACE("(%p)->(IID_IDispatch %p)\n", This, ppv);
        *ppv = &This->IDispatchEx_iface;
    }else if(IsEqualGUID(&IID_IDispatchEx, riid)) {
        TRACE("(%p)->(IID_IDispatchEx %p)\n", This, ppv);
        *ppv = &This->IDispatchEx_iface;
    }else {
        WARN("(%p)->(%s %p)\n", This, debugstr_guid(riid), ppv);
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    jsdisp_addref(This);
    return S_OK;
}

static ULONG WINAPI DispatchEx_AddRef(IDispatchEx *iface)
{
    jsdisp_t *This = impl_from_IDispatchEx(iface);
    jsdisp_addref(This);
    return This->ref;
}

static ULONG WINAPI DispatchEx_Release(IDispatchEx *iface)
{
    jsdisp_t *This = impl_from_IDispatchEx(iface);
    ULONG ref = --This->ref;
    TRACE("(%p) ref=%d\n", This, ref);
    if(!ref)
        jsdisp_free(This);
    return ref;
}

static HRESULT WINAPI DispatchEx_GetTypeInfoCount(IDispatchEx *iface, UINT *pctinfo)
{
    jsdisp_t *This = impl_from_IDispatchEx(iface);

    TRACE("(%p)->(%p)\n", This, pctinfo);

    *pctinfo = 1;
    return S_OK;
}

static HRESULT WINAPI DispatchEx_GetTypeInfo(IDispatchEx *iface, UINT iTInfo, LCID lcid,
                                              ITypeInfo **ppTInfo)
{
    jsdisp_t *This = impl_from_IDispatchEx(iface);
    dispex_prop_t *prop, *cur, *end, **typevar;
    UINT num_funcs = 0, num_vars = 0;
    struct typeinfo_func *typefunc;
    function_code_t *func_code;
    ScriptTypeInfo *typeinfo;
    unsigned pos;

    TRACE("(%p)->(%u %u %p)\n", This, iTInfo, lcid, ppTInfo);

    if (iTInfo != 0) return DISP_E_BADINDEX;

    for (prop = This->props, end = prop + This->prop_cnt; prop != end; prop++)
    {
        if (!prop->name || prop->type != PROP_JSVAL || !(prop->flags & PROPF_ENUMERABLE))
            continue;

        /* If two identifiers differ only by case, the TypeInfo fails */
        pos = This->props[get_props_idx(This, prop->hash)].bucket_head;
        while (pos)
        {
            cur = This->props + pos;

            if (prop->hash == cur->hash && prop != cur &&
                cur->type == PROP_JSVAL && (cur->flags & PROPF_ENUMERABLE) &&
                !wcsicmp(prop->name, cur->name))
            {
                return TYPE_E_AMBIGUOUSNAME;
            }
            pos = cur->bucket_next;
        }

        if (is_function_prop(prop))
        {
            if (Function_get_code(as_jsdisp(get_object(prop->u.val))))
                num_funcs++;
        }
        else num_vars++;
    }

    if (!(typeinfo = heap_alloc(sizeof(*typeinfo))))
        return E_OUTOFMEMORY;

    typeinfo->ITypeInfo_iface.lpVtbl = &ScriptTypeInfoVtbl;
    typeinfo->ITypeComp_iface.lpVtbl = &ScriptTypeCompVtbl;
    typeinfo->ref = 1;
    typeinfo->num_vars = num_vars;
    typeinfo->num_funcs = num_funcs;
    typeinfo->jsdisp = This;

    typeinfo->funcs = heap_alloc(sizeof(*typeinfo->funcs) * num_funcs);
    if (!typeinfo->funcs)
    {
        heap_free(typeinfo);
        return E_OUTOFMEMORY;
    }

    typeinfo->vars = heap_alloc(sizeof(*typeinfo->vars) * num_vars);
    if (!typeinfo->vars)
    {
        heap_free(typeinfo->funcs);
        heap_free(typeinfo);
        return E_OUTOFMEMORY;
    }

    typefunc = typeinfo->funcs;
    typevar = typeinfo->vars;
    for (prop = This->props; prop != end; prop++)
    {
        if (!prop->name || prop->type != PROP_JSVAL || !(prop->flags & PROPF_ENUMERABLE))
            continue;

        if (is_function_prop(prop))
        {
            func_code = Function_get_code(as_jsdisp(get_object(prop->u.val)));
            if (!func_code) continue;

            typefunc->prop = prop;
            typefunc->code = func_code;
            typefunc++;

            /* The function may be deleted, so keep a ref */
            bytecode_addref(func_code->bytecode);
        }
        else
            *typevar++ = prop;
    }

    /* Keep a ref to the props and their names */
    IDispatchEx_AddRef(&This->IDispatchEx_iface);

    *ppTInfo = &typeinfo->ITypeInfo_iface;
    return S_OK;
}

static HRESULT WINAPI DispatchEx_GetIDsOfNames(IDispatchEx *iface, REFIID riid,
                                                LPOLESTR *rgszNames, UINT cNames, LCID lcid,
                                                DISPID *rgDispId)
{
    jsdisp_t *This = impl_from_IDispatchEx(iface);
    UINT i;
    HRESULT hres;

    TRACE("(%p)->(%s %p %u %u %p)\n", This, debugstr_guid(riid), rgszNames, cNames,
          lcid, rgDispId);

    for(i=0; i < cNames; i++) {
        hres = IDispatchEx_GetDispID(&This->IDispatchEx_iface, rgszNames[i], 0, rgDispId+i);
        if(FAILED(hres))
            return hres;
    }

    return S_OK;
}

static HRESULT WINAPI DispatchEx_Invoke(IDispatchEx *iface, DISPID dispIdMember,
                                        REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
                            VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    jsdisp_t *This = impl_from_IDispatchEx(iface);

    TRACE("(%p)->(%d %s %d %d %p %p %p %p)\n", This, dispIdMember, debugstr_guid(riid),
          lcid, wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);

    return IDispatchEx_InvokeEx(&This->IDispatchEx_iface, dispIdMember, lcid, wFlags,
            pDispParams, pVarResult, pExcepInfo, NULL);
}

static HRESULT WINAPI DispatchEx_GetDispID(IDispatchEx *iface, BSTR bstrName, DWORD grfdex, DISPID *pid)
{
    jsdisp_t *This = impl_from_IDispatchEx(iface);

    TRACE("(%p)->(%s %x %p)\n", This, debugstr_w(bstrName), grfdex, pid);

    if(grfdex & ~(fdexNameCaseSensitive|fdexNameEnsure|fdexNameImplicit|FDEX_VERSION_MASK)) {
        FIXME("Unsupported grfdex %x\n", grfdex);
        return E_NOTIMPL;
    }

    return jsdisp_get_id(This, bstrName, grfdex, pid);
}

static HRESULT WINAPI DispatchEx_InvokeEx(IDispatchEx *iface, DISPID id, LCID lcid, WORD wFlags, DISPPARAMS *pdp,
        VARIANT *pvarRes, EXCEPINFO *pei, IServiceProvider *pspCaller)
{
    jsdisp_t *This = impl_from_IDispatchEx(iface);
    dispex_prop_t *prop;
    jsexcept_t ei;
    HRESULT hres;

    TRACE("(%p)->(%x %x %x %p %p %p %p)\n", This, id, lcid, wFlags, pdp, pvarRes, pei, pspCaller);

    if(pvarRes)
        V_VT(pvarRes) = VT_EMPTY;

    prop = get_prop(This, id);
    if(!prop || prop->type == PROP_DELETED) {
        TRACE("invalid id\n");
        return DISP_E_MEMBERNOTFOUND;
    }

    enter_script(This->ctx, &ei);

    switch(wFlags) {
    case DISPATCH_METHOD|DISPATCH_PROPERTYGET:
        wFlags = DISPATCH_METHOD;
        /* fall through */
    case DISPATCH_METHOD:
    case DISPATCH_CONSTRUCT: {
        jsval_t *argv, buf[6], r;
        unsigned argc;

        hres = convert_params(pdp, buf, &argc, &argv);
        if(FAILED(hres))
            break;

        hres = invoke_prop_func(This, get_this(pdp), prop, wFlags, argc, argv, pvarRes ? &r : NULL, pspCaller);
        if(argv != buf)
            heap_free(argv);
        if(SUCCEEDED(hres) && pvarRes) {
            hres = jsval_to_variant(r, pvarRes);
            jsval_release(r);
        }
        break;
    }
    case DISPATCH_PROPERTYGET: {
        jsval_t r;

        hres = prop_get(This, prop, &r);
        if(SUCCEEDED(hres)) {
            hres = jsval_to_variant(r, pvarRes);
            jsval_release(r);
        }
        break;
    }
    case DISPATCH_PROPERTYPUT: {
        jsval_t val;
        DWORD i;

        for(i=0; i < pdp->cNamedArgs; i++) {
            if(pdp->rgdispidNamedArgs[i] == DISPID_PROPERTYPUT)
                break;
        }

        if(i == pdp->cNamedArgs) {
            TRACE("no value to set\n");
            hres = DISP_E_PARAMNOTOPTIONAL;
            break;
        }

        hres = variant_to_jsval(pdp->rgvarg+i, &val);
        if(FAILED(hres))
            break;

        hres = prop_put(This, prop, val);
        jsval_release(val);
        break;
    }
    default:
        FIXME("Unimplemented flags %x\n", wFlags);
        hres = E_INVALIDARG;
        break;
    }

    return leave_script(This->ctx, hres);
}

static HRESULT delete_prop(dispex_prop_t *prop, BOOL *ret)
{
    if(!(prop->flags & PROPF_CONFIGURABLE)) {
        *ret = FALSE;
        return S_OK;
    }

    *ret = TRUE; /* FIXME: not exactly right */

    if(prop->type == PROP_JSVAL) {
        jsval_release(prop->u.val);
        prop->type = PROP_DELETED;
    }
    if(prop->type == PROP_ACCESSOR)
        FIXME("not supported on accessor property\n");
    return S_OK;
}

static HRESULT WINAPI DispatchEx_DeleteMemberByName(IDispatchEx *iface, BSTR bstrName, DWORD grfdex)
{
    jsdisp_t *This = impl_from_IDispatchEx(iface);
    dispex_prop_t *prop;
    BOOL b;
    HRESULT hres;

    TRACE("(%p)->(%s %x)\n", This, debugstr_w(bstrName), grfdex);

    if(grfdex & ~(fdexNameCaseSensitive|fdexNameEnsure|fdexNameImplicit|FDEX_VERSION_MASK))
        FIXME("Unsupported grfdex %x\n", grfdex);

    hres = find_prop_name(This, string_hash(bstrName), bstrName, &prop);
    if(FAILED(hres))
        return hres;
    if(!prop) {
        TRACE("not found\n");
        return S_OK;
    }

    return delete_prop(prop, &b);
}

static HRESULT WINAPI DispatchEx_DeleteMemberByDispID(IDispatchEx *iface, DISPID id)
{
    jsdisp_t *This = impl_from_IDispatchEx(iface);
    dispex_prop_t *prop;
    BOOL b;

    TRACE("(%p)->(%x)\n", This, id);

    prop = get_prop(This, id);
    if(!prop) {
        WARN("invalid id\n");
        return DISP_E_MEMBERNOTFOUND;
    }

    return delete_prop(prop, &b);
}

static HRESULT WINAPI DispatchEx_GetMemberProperties(IDispatchEx *iface, DISPID id, DWORD grfdexFetch, DWORD *pgrfdex)
{
    jsdisp_t *This = impl_from_IDispatchEx(iface);
    FIXME("(%p)->(%x %x %p)\n", This, id, grfdexFetch, pgrfdex);
    return E_NOTIMPL;
}

static HRESULT WINAPI DispatchEx_GetMemberName(IDispatchEx *iface, DISPID id, BSTR *pbstrName)
{
    jsdisp_t *This = impl_from_IDispatchEx(iface);
    dispex_prop_t *prop;

    TRACE("(%p)->(%x %p)\n", This, id, pbstrName);

    prop = get_prop(This, id);
    if(!prop || !prop->name || prop->type == PROP_DELETED)
        return DISP_E_MEMBERNOTFOUND;

    *pbstrName = SysAllocString(prop->name);
    if(!*pbstrName)
        return E_OUTOFMEMORY;

    return S_OK;
}

static HRESULT WINAPI DispatchEx_GetNextDispID(IDispatchEx *iface, DWORD grfdex, DISPID id, DISPID *pid)
{
    jsdisp_t *This = impl_from_IDispatchEx(iface);
    HRESULT hres;

    TRACE("(%p)->(%x %x %p)\n", This, grfdex, id, pid);

    hres = jsdisp_next_prop(This, id, FALSE, pid);
    if(hres == S_FALSE)
        *pid = DISPID_STARTENUM;
    return hres;
}

static HRESULT WINAPI DispatchEx_GetNameSpaceParent(IDispatchEx *iface, IUnknown **ppunk)
{
    jsdisp_t *This = impl_from_IDispatchEx(iface);
    FIXME("(%p)->(%p)\n", This, ppunk);
    return E_NOTIMPL;
}

static IDispatchExVtbl DispatchExVtbl = {
    DispatchEx_QueryInterface,
    DispatchEx_AddRef,
    DispatchEx_Release,
    DispatchEx_GetTypeInfoCount,
    DispatchEx_GetTypeInfo,
    DispatchEx_GetIDsOfNames,
    DispatchEx_Invoke,
    DispatchEx_GetDispID,
    DispatchEx_InvokeEx,
    DispatchEx_DeleteMemberByName,
    DispatchEx_DeleteMemberByDispID,
    DispatchEx_GetMemberProperties,
    DispatchEx_GetMemberName,
    DispatchEx_GetNextDispID,
    DispatchEx_GetNameSpaceParent
};

jsdisp_t *as_jsdisp(IDispatch *disp)
{
    assert(disp->lpVtbl == (IDispatchVtbl*)&DispatchExVtbl);
    return impl_from_IDispatchEx((IDispatchEx*)disp);
}

jsdisp_t *to_jsdisp(IDispatch *disp)
{
    return disp->lpVtbl == (IDispatchVtbl*)&DispatchExVtbl ? impl_from_IDispatchEx((IDispatchEx*)disp) : NULL;
}

HRESULT init_dispex(jsdisp_t *dispex, script_ctx_t *ctx, const builtin_info_t *builtin_info, jsdisp_t *prototype)
{
    TRACE("%p (%p)\n", dispex, prototype);

    dispex->IDispatchEx_iface.lpVtbl = &DispatchExVtbl;
    dispex->ref = 1;
    dispex->builtin_info = builtin_info;

    dispex->props = heap_alloc_zero(sizeof(dispex_prop_t)*(dispex->buf_size=4));
    if(!dispex->props)
        return E_OUTOFMEMORY;

    dispex->prototype = prototype;
    if(prototype)
        jsdisp_addref(prototype);

    dispex->prop_cnt = 1;
    if(builtin_info->value_prop.invoke || builtin_info->value_prop.getter) {
        dispex->props[0].type = PROP_BUILTIN;
        dispex->props[0].u.p = &builtin_info->value_prop;
    }else {
        dispex->props[0].type = PROP_DELETED;
    }

    script_addref(ctx);
    dispex->ctx = ctx;

    return S_OK;
}

static const builtin_info_t dispex_info = {
    JSCLASS_NONE,
    {NULL, NULL, 0},
    0, NULL,
    NULL,
    NULL
};

HRESULT create_dispex(script_ctx_t *ctx, const builtin_info_t *builtin_info, jsdisp_t *prototype, jsdisp_t **dispex)
{
    jsdisp_t *ret;
    HRESULT hres;

    ret = heap_alloc_zero(sizeof(jsdisp_t));
    if(!ret)
        return E_OUTOFMEMORY;

    hres = init_dispex(ret, ctx, builtin_info ? builtin_info : &dispex_info, prototype);
    if(FAILED(hres)) {
        heap_free(ret);
        return hres;
    }

    *dispex = ret;
    return S_OK;
}

void jsdisp_free(jsdisp_t *obj)
{
    dispex_prop_t *prop;

    TRACE("(%p)\n", obj);

    for(prop = obj->props; prop < obj->props+obj->prop_cnt; prop++) {
        switch(prop->type) {
        case PROP_JSVAL:
            jsval_release(prop->u.val);
            break;
        case PROP_ACCESSOR:
            if(prop->u.accessor.getter)
                jsdisp_release(prop->u.accessor.getter);
            if(prop->u.accessor.setter)
                jsdisp_release(prop->u.accessor.setter);
            break;
        default:
            break;
        };
        heap_free(prop->name);
    }
    heap_free(obj->props);
    script_release(obj->ctx);
    if(obj->prototype)
        jsdisp_release(obj->prototype);

    if(obj->builtin_info->destructor)
        obj->builtin_info->destructor(obj);
    else
        heap_free(obj);
}

#ifdef TRACE_REFCNT

jsdisp_t *jsdisp_addref(jsdisp_t *jsdisp)
{
    ULONG ref = ++jsdisp->ref;
    TRACE("(%p) ref=%d\n", jsdisp, ref);
    return jsdisp;
}

void jsdisp_release(jsdisp_t *jsdisp)
{
    ULONG ref = --jsdisp->ref;

    TRACE("(%p) ref=%d\n", jsdisp, ref);

    if(!ref)
        jsdisp_free(jsdisp);
}

#endif

HRESULT init_dispex_from_constr(jsdisp_t *dispex, script_ctx_t *ctx, const builtin_info_t *builtin_info, jsdisp_t *constr)
{
    jsdisp_t *prot = NULL;
    dispex_prop_t *prop;
    HRESULT hres;

    hres = find_prop_name_prot(constr, string_hash(L"prototype"), L"prototype", &prop);
    if(SUCCEEDED(hres) && prop && prop->type!=PROP_DELETED) {
        jsval_t val;

        hres = prop_get(constr, prop, &val);
        if(FAILED(hres)) {
            ERR("Could not get prototype\n");
            return hres;
        }

        if(is_object_instance(val))
            prot = iface_to_jsdisp(get_object(val));
        jsval_release(val);
    }

    hres = init_dispex(dispex, ctx, builtin_info, prot);

    if(prot)
        jsdisp_release(prot);
    return hres;
}

jsdisp_t *iface_to_jsdisp(IDispatch *iface)
{
    return iface->lpVtbl == (const IDispatchVtbl*)&DispatchExVtbl
        ? jsdisp_addref( impl_from_IDispatchEx((IDispatchEx*)iface))
        : NULL;
}

HRESULT jsdisp_get_id(jsdisp_t *jsdisp, const WCHAR *name, DWORD flags, DISPID *id)
{
    dispex_prop_t *prop;
    HRESULT hres;

    if(flags & fdexNameEnsure)
        hres = ensure_prop_name(jsdisp, name, PROPF_ENUMERABLE | PROPF_CONFIGURABLE | PROPF_WRITABLE,
                                &prop);
    else
        hres = find_prop_name_prot(jsdisp, string_hash(name), name, &prop);
    if(FAILED(hres))
        return hres;

    if(prop && prop->type!=PROP_DELETED) {
        *id = prop_to_id(jsdisp, prop);
        return S_OK;
    }

    TRACE("not found %s\n", debugstr_w(name));
    *id = DISPID_UNKNOWN;
    return DISP_E_UNKNOWNNAME;
}

HRESULT jsdisp_call_value(jsdisp_t *jsfunc, IDispatch *jsthis, WORD flags, unsigned argc, jsval_t *argv, jsval_t *r)
{
    HRESULT hres;

    assert(!(flags & ~(DISPATCH_METHOD|DISPATCH_CONSTRUCT|DISPATCH_JSCRIPT_INTERNAL_MASK)));

    if(is_class(jsfunc, JSCLASS_FUNCTION)) {
        hres = Function_invoke(jsfunc, jsthis, flags, argc, argv, r);
    }else {
        vdisp_t vdisp;

        if(!jsfunc->builtin_info->value_prop.invoke) {
            WARN("Not a function\n");
            return JS_E_FUNCTION_EXPECTED;
        }

        set_disp(&vdisp, jsthis);
        flags &= ~DISPATCH_JSCRIPT_INTERNAL_MASK;
        hres = jsfunc->builtin_info->value_prop.invoke(jsfunc->ctx, &vdisp, flags, argc, argv, r);
        vdisp_release(&vdisp);
    }
    return hres;
}

HRESULT jsdisp_call(jsdisp_t *disp, DISPID id, WORD flags, unsigned argc, jsval_t *argv, jsval_t *r)
{
    dispex_prop_t *prop;

    prop = get_prop(disp, id);
    if(!prop)
        return DISP_E_MEMBERNOTFOUND;

    return invoke_prop_func(disp, to_disp(disp), prop, flags, argc, argv, r, NULL);
}

HRESULT jsdisp_call_name(jsdisp_t *disp, const WCHAR *name, WORD flags, unsigned argc, jsval_t *argv, jsval_t *r)
{
    dispex_prop_t *prop;
    HRESULT hres;

    hres = find_prop_name_prot(disp, string_hash(name), name, &prop);
    if(FAILED(hres))
        return hres;

    return invoke_prop_func(disp, to_disp(disp), prop, flags, argc, argv, r, NULL);
}

static HRESULT disp_invoke(script_ctx_t *ctx, IDispatch *disp, DISPID id, WORD flags, DISPPARAMS *params, VARIANT *r)
{
    IDispatchEx *dispex;
    EXCEPINFO ei;
    HRESULT hres;

    memset(&ei, 0, sizeof(ei));
    hres = IDispatch_QueryInterface(disp, &IID_IDispatchEx, (void**)&dispex);
    if(SUCCEEDED(hres)) {
        hres = IDispatchEx_InvokeEx(dispex, id, ctx->lcid, flags, params, r, &ei, &ctx->jscaller->IServiceProvider_iface);
        IDispatchEx_Release(dispex);
    }else {
        UINT err = 0;

        if(flags == DISPATCH_CONSTRUCT) {
            WARN("IDispatch cannot be constructor\n");
            return DISP_E_MEMBERNOTFOUND;
        }

        if(params->cNamedArgs == 1 && params->rgdispidNamedArgs[0] == DISPID_THIS) {
            params->cNamedArgs = 0;
            params->rgdispidNamedArgs = NULL;
            params->cArgs--;
            params->rgvarg++;
        }

        TRACE("using IDispatch\n");
        hres = IDispatch_Invoke(disp, id, &IID_NULL, ctx->lcid, flags, params, r, &ei, &err);
    }

    if(hres == DISP_E_EXCEPTION) {
        TRACE("DISP_E_EXCEPTION: %08x %s %s\n", ei.scode, debugstr_w(ei.bstrSource), debugstr_w(ei.bstrDescription));
        reset_ei(ctx->ei);
        ctx->ei->error = (SUCCEEDED(ei.scode) || ei.scode == DISP_E_EXCEPTION) ? E_FAIL : ei.scode;
        if(ei.bstrSource)
            ctx->ei->source = jsstr_alloc_len(ei.bstrSource, SysStringLen(ei.bstrSource));
        if(ei.bstrDescription)
            ctx->ei->message = jsstr_alloc_len(ei.bstrDescription, SysStringLen(ei.bstrDescription));
        SysFreeString(ei.bstrSource);
        SysFreeString(ei.bstrDescription);
        SysFreeString(ei.bstrHelpFile);
    }

    return hres;
}

HRESULT disp_call(script_ctx_t *ctx, IDispatch *disp, DISPID id, WORD flags, unsigned argc, jsval_t *argv, jsval_t *ret)
{
    VARIANT buf[6], retv;
    jsdisp_t *jsdisp;
    DISPPARAMS dp;
    unsigned i;
    HRESULT hres;

    jsdisp = iface_to_jsdisp(disp);
    if(jsdisp && jsdisp->ctx == ctx) {
        if(flags & DISPATCH_PROPERTYPUT) {
            FIXME("disp_call(propput) on builtin object\n");
            return E_FAIL;
        }

        if(ctx != jsdisp->ctx)
            flags &= ~DISPATCH_JSCRIPT_INTERNAL_MASK;
        hres = jsdisp_call(jsdisp, id, flags, argc, argv, ret);
        jsdisp_release(jsdisp);
        return hres;
    }

    flags &= ~DISPATCH_JSCRIPT_INTERNAL_MASK;
    if(ret && argc)
        flags |= DISPATCH_PROPERTYGET;

    dp.cArgs = argc;

    if(flags & DISPATCH_PROPERTYPUT) {
        static DISPID propput_dispid = DISPID_PROPERTYPUT;

        dp.cNamedArgs = 1;
        dp.rgdispidNamedArgs = &propput_dispid;
    }else {
        dp.cNamedArgs = 0;
        dp.rgdispidNamedArgs = NULL;
    }

    if(dp.cArgs > ARRAY_SIZE(buf)) {
        dp.rgvarg = heap_alloc(argc*sizeof(VARIANT));
        if(!dp.rgvarg)
            return E_OUTOFMEMORY;
    }else {
        dp.rgvarg = buf;
    }

    for(i=0; i<argc; i++) {
        hres = jsval_to_variant(argv[i], dp.rgvarg+argc-i-1);
        if(FAILED(hres)) {
            while(i--)
                VariantClear(dp.rgvarg+argc-i-1);
            if(dp.rgvarg != buf)
                heap_free(dp.rgvarg);
            return hres;
        }
    }

    V_VT(&retv) = VT_EMPTY;
    hres = disp_invoke(ctx, disp, id, flags, &dp, ret ? &retv : NULL);

    for(i=0; i<argc; i++)
        VariantClear(dp.rgvarg+argc-i-1);
    if(dp.rgvarg != buf)
        heap_free(dp.rgvarg);

    if(SUCCEEDED(hres) && ret)
        hres = variant_to_jsval(&retv, ret);
    VariantClear(&retv);
    return hres;
}

HRESULT disp_call_value(script_ctx_t *ctx, IDispatch *disp, IDispatch *jsthis, WORD flags, unsigned argc, jsval_t *argv,
        jsval_t *r)
{
    VARIANT buf[6], retv, *args = buf;
    jsdisp_t *jsdisp;
    DISPPARAMS dp;
    unsigned i;
    HRESULT hres = S_OK;

    static DISPID this_id = DISPID_THIS;

    assert(!(flags & ~(DISPATCH_METHOD|DISPATCH_CONSTRUCT|DISPATCH_JSCRIPT_INTERNAL_MASK)));

    jsdisp = iface_to_jsdisp(disp);
    if(jsdisp && jsdisp->ctx == ctx) {
        if(ctx != jsdisp->ctx)
            flags &= ~DISPATCH_JSCRIPT_INTERNAL_MASK;
        hres = jsdisp_call_value(jsdisp, jsthis, flags, argc, argv, r);
        jsdisp_release(jsdisp);
        return hres;
    }

    flags &= ~DISPATCH_JSCRIPT_INTERNAL_MASK;
    if(r && argc && flags == DISPATCH_METHOD)
        flags |= DISPATCH_PROPERTYGET;

    if(jsthis) {
        dp.cArgs = argc + 1;
        dp.cNamedArgs = 1;
        dp.rgdispidNamedArgs = &this_id;
    }else {
        dp.cArgs = argc;
        dp.cNamedArgs = 0;
        dp.rgdispidNamedArgs = NULL;
    }

    if(dp.cArgs > ARRAY_SIZE(buf) && !(args = heap_alloc(dp.cArgs * sizeof(VARIANT))))
        return E_OUTOFMEMORY;
    dp.rgvarg = args;

    if(jsthis) {
        V_VT(dp.rgvarg) = VT_DISPATCH;
        V_DISPATCH(dp.rgvarg) = jsthis;
    }

    for(i=0; SUCCEEDED(hres) && i < argc; i++)
        hres = jsval_to_variant(argv[i], dp.rgvarg+dp.cArgs-i-1);

    if(SUCCEEDED(hres)) {
        V_VT(&retv) = VT_EMPTY;
        hres = disp_invoke(ctx, disp, DISPID_VALUE, flags, &dp, r ? &retv : NULL);
    }

    for(i = 0; i < argc; i++)
        VariantClear(dp.rgvarg + dp.cArgs - i - 1);
    if(args != buf)
        heap_free(args);

    if(!r)
        return S_OK;

    hres = variant_to_jsval(&retv, r);
    VariantClear(&retv);
    return hres;
}

HRESULT jsdisp_propput(jsdisp_t *obj, const WCHAR *name, DWORD flags, jsval_t val)
{
    dispex_prop_t *prop;
    HRESULT hres;

    hres = ensure_prop_name(obj, name, flags, &prop);
    if(FAILED(hres))
        return hres;

    return prop_put(obj, prop, val);
}

HRESULT jsdisp_propput_name(jsdisp_t *obj, const WCHAR *name, jsval_t val)
{
    return jsdisp_propput(obj, name, PROPF_ENUMERABLE | PROPF_CONFIGURABLE | PROPF_WRITABLE, val);
}

HRESULT jsdisp_propput_idx(jsdisp_t *obj, DWORD idx, jsval_t val)
{
    WCHAR buf[12];

    swprintf(buf, ARRAY_SIZE(buf), L"%d", idx);
    return jsdisp_propput_name(obj, buf, val);
}

HRESULT disp_propput(script_ctx_t *ctx, IDispatch *disp, DISPID id, jsval_t val)
{
    jsdisp_t *jsdisp;
    HRESULT hres;

    jsdisp = iface_to_jsdisp(disp);
    if(jsdisp && jsdisp->ctx == ctx) {
        dispex_prop_t *prop;

        prop = get_prop(jsdisp, id);
        if(prop)
            hres = prop_put(jsdisp, prop, val);
        else
            hres = DISP_E_MEMBERNOTFOUND;

        jsdisp_release(jsdisp);
    }else {
        DISPID dispid = DISPID_PROPERTYPUT;
        DWORD flags = DISPATCH_PROPERTYPUT;
        VARIANT var;
        DISPPARAMS dp  = {&var, &dispid, 1, 1};

        hres = jsval_to_variant(val, &var);
        if(FAILED(hres))
            return hres;

        if(V_VT(&var) == VT_DISPATCH)
            flags |= DISPATCH_PROPERTYPUTREF;

        hres = disp_invoke(ctx, disp, id, flags, &dp, NULL);
        VariantClear(&var);
    }

    return hres;
}

HRESULT jsdisp_propget_name(jsdisp_t *obj, const WCHAR *name, jsval_t *val)
{
    dispex_prop_t *prop;
    HRESULT hres;

    hres = find_prop_name_prot(obj, string_hash(name), name, &prop);
    if(FAILED(hres))
        return hres;

    if(!prop || prop->type==PROP_DELETED) {
        *val = jsval_undefined();
        return S_OK;
    }

    return prop_get(obj, prop, val);
}

HRESULT jsdisp_get_idx(jsdisp_t *obj, DWORD idx, jsval_t *r)
{
    WCHAR name[12];
    dispex_prop_t *prop;
    HRESULT hres;

    swprintf(name, ARRAY_SIZE(name), L"%d", idx);

    hres = find_prop_name_prot(obj, string_hash(name), name, &prop);
    if(FAILED(hres))
        return hres;

    if(!prop || prop->type==PROP_DELETED) {
        *r = jsval_undefined();
        return DISP_E_UNKNOWNNAME;
    }

    return prop_get(obj, prop, r);
}

HRESULT jsdisp_propget(jsdisp_t *jsdisp, DISPID id, jsval_t *val)
{
    dispex_prop_t *prop;

    prop = get_prop(jsdisp, id);
    if(!prop)
        return DISP_E_MEMBERNOTFOUND;

    return prop_get(jsdisp, prop, val);
}

HRESULT disp_propget(script_ctx_t *ctx, IDispatch *disp, DISPID id, jsval_t *val)
{
    DISPPARAMS dp  = {NULL,NULL,0,0};
    jsdisp_t *jsdisp;
    VARIANT var;
    HRESULT hres;

    jsdisp = iface_to_jsdisp(disp);
    if(jsdisp && jsdisp->ctx == ctx) {
        hres = jsdisp_propget(jsdisp, id, val);
        jsdisp_release(jsdisp);
        return hres;
    }

    V_VT(&var) = VT_EMPTY;
    hres = disp_invoke(ctx, disp, id, INVOKE_PROPERTYGET, &dp, &var);
    if(SUCCEEDED(hres)) {
        hres = variant_to_jsval(&var, val);
        VariantClear(&var);
    }
    return hres;
}

HRESULT jsdisp_delete_idx(jsdisp_t *obj, DWORD idx)
{
    WCHAR buf[12];
    dispex_prop_t *prop;
    BOOL b;
    HRESULT hres;

    swprintf(buf, ARRAY_SIZE(buf), L"%d", idx);

    hres = find_prop_name(obj, string_hash(buf), buf, &prop);
    if(FAILED(hres) || !prop)
        return hres;

    return delete_prop(prop, &b);
}

HRESULT disp_delete(IDispatch *disp, DISPID id, BOOL *ret)
{
    IDispatchEx *dispex;
    jsdisp_t *jsdisp;
    HRESULT hres;

    jsdisp = iface_to_jsdisp(disp);
    if(jsdisp) {
        dispex_prop_t *prop;

        prop = get_prop(jsdisp, id);
        if(prop)
            hres = delete_prop(prop, ret);
        else
            hres = DISP_E_MEMBERNOTFOUND;

        jsdisp_release(jsdisp);
        return hres;
    }

    hres = IDispatch_QueryInterface(disp, &IID_IDispatchEx, (void**)&dispex);
    if(FAILED(hres)) {
        *ret = FALSE;
        return S_OK;
    }

    hres = IDispatchEx_DeleteMemberByDispID(dispex, id);
    IDispatchEx_Release(dispex);
    if(FAILED(hres))
        return hres;

    *ret = hres == S_OK;
    return S_OK;
}

HRESULT jsdisp_next_prop(jsdisp_t *obj, DISPID id, BOOL own_only, DISPID *ret)
{
    dispex_prop_t *iter;
    HRESULT hres;

    if(id == DISPID_STARTENUM && !own_only) {
        hres = fill_protrefs(obj);
        if(FAILED(hres))
            return hres;
    }

    if(id + 1 < 0 || id+1 >= obj->prop_cnt)
        return S_FALSE;

    for(iter = &obj->props[id + 1]; iter < obj->props + obj->prop_cnt; iter++) {
        if(!iter->name || iter->type == PROP_DELETED)
            continue;
        if(own_only && iter->type == PROP_PROTREF)
            continue;
        if(!(get_flags(obj, iter) & PROPF_ENUMERABLE))
            continue;
        *ret = prop_to_id(obj, iter);
        return S_OK;
    }

    return S_FALSE;
}

HRESULT disp_delete_name(script_ctx_t *ctx, IDispatch *disp, jsstr_t *name, BOOL *ret)
{
    IDispatchEx *dispex;
    jsdisp_t *jsdisp;
    BSTR bstr;
    HRESULT hres;

    jsdisp = iface_to_jsdisp(disp);
    if(jsdisp) {
        dispex_prop_t *prop;
        const WCHAR *ptr;

        ptr = jsstr_flatten(name);
        if(!ptr) {
            jsdisp_release(jsdisp);
            return E_OUTOFMEMORY;
        }

        hres = find_prop_name(jsdisp, string_hash(ptr), ptr, &prop);
        if(prop) {
            hres = delete_prop(prop, ret);
        }else {
            *ret = TRUE;
            hres = S_OK;
        }

        jsdisp_release(jsdisp);
        return hres;
    }

    bstr = SysAllocStringLen(NULL, jsstr_length(name));
    if(!bstr)
        return E_OUTOFMEMORY;
    jsstr_flush(name, bstr);

    hres = IDispatch_QueryInterface(disp, &IID_IDispatchEx, (void**)&dispex);
    if(SUCCEEDED(hres)) {
        hres = IDispatchEx_DeleteMemberByName(dispex, bstr, make_grfdex(ctx, fdexNameCaseSensitive));
        if(SUCCEEDED(hres))
            *ret = hres == S_OK;
        IDispatchEx_Release(dispex);
    }else {
        DISPID id;

        hres = IDispatch_GetIDsOfNames(disp, &IID_NULL, &bstr, 1, 0, &id);
        if(SUCCEEDED(hres)) {
            /* Property exists and we can't delete it from pure IDispatch interface, so return false. */
            *ret = FALSE;
        }else if(hres == DISP_E_UNKNOWNNAME) {
            /* Property doesn't exist, so nothing to delete */
            *ret = TRUE;
            hres = S_OK;
        }
    }

    SysFreeString(bstr);
    return hres;
}

HRESULT jsdisp_get_own_property(jsdisp_t *obj, const WCHAR *name, BOOL flags_only,
                                property_desc_t *desc)
{
    dispex_prop_t *prop;
    HRESULT hres;

    hres = find_prop_name(obj, string_hash(name), name, &prop);
    if(FAILED(hres))
        return hres;

    if(!prop)
        return DISP_E_UNKNOWNNAME;

    memset(desc, 0, sizeof(*desc));

    switch(prop->type) {
    case PROP_BUILTIN:
    case PROP_JSVAL:
        desc->mask |= PROPF_WRITABLE;
        desc->explicit_value = TRUE;
        if(!flags_only) {
            hres = prop_get(obj, prop, &desc->value);
            if(FAILED(hres))
                return hres;
        }
        break;
    case PROP_ACCESSOR:
        desc->explicit_getter = desc->explicit_setter = TRUE;
        if(!flags_only) {
            desc->getter = prop->u.accessor.getter
                ? jsdisp_addref(prop->u.accessor.getter) : NULL;
            desc->setter = prop->u.accessor.setter
                ? jsdisp_addref(prop->u.accessor.setter) : NULL;
        }
        break;
    default:
        return DISP_E_UNKNOWNNAME;
    }

    desc->flags = prop->flags & (PROPF_ENUMERABLE | PROPF_WRITABLE | PROPF_CONFIGURABLE);
    desc->mask |= PROPF_ENUMERABLE | PROPF_CONFIGURABLE;
    return S_OK;
}

HRESULT jsdisp_define_property(jsdisp_t *obj, const WCHAR *name, property_desc_t *desc)
{
    dispex_prop_t *prop;
    HRESULT hres;

    hres = find_prop_name(obj, string_hash(name), name, &prop);
    if(FAILED(hres))
        return hres;

    if(!prop && !(prop = alloc_prop(obj, name, PROP_DELETED, 0)))
       return E_OUTOFMEMORY;

    if(prop->type == PROP_DELETED || prop->type == PROP_PROTREF) {
        prop->flags = desc->flags;
        if(desc->explicit_getter || desc->explicit_setter) {
            prop->type = PROP_ACCESSOR;
            prop->u.accessor.getter = desc->getter ? jsdisp_addref(desc->getter) : NULL;
            prop->u.accessor.setter = desc->setter ? jsdisp_addref(desc->setter) : NULL;
            TRACE("%s = accessor { get: %p set: %p }\n", debugstr_w(name),
                  prop->u.accessor.getter, prop->u.accessor.setter);
        }else {
            prop->type = PROP_JSVAL;
            if(desc->explicit_value) {
                hres = jsval_copy(desc->value, &prop->u.val);
                if(FAILED(hres))
                    return hres;
            }else {
                prop->u.val = jsval_undefined();
            }
            TRACE("%s = %s\n", debugstr_w(name), debugstr_jsval(prop->u.val));
        }
        return S_OK;
    }

    TRACE("existing prop %s prop flags %x desc flags %x desc mask %x\n", debugstr_w(name),
          prop->flags, desc->flags, desc->mask);

    if(!(prop->flags & PROPF_CONFIGURABLE)) {
        if(((desc->mask & PROPF_CONFIGURABLE) && (desc->flags & PROPF_CONFIGURABLE))
           || ((desc->mask & PROPF_ENUMERABLE)
               && ((desc->flags & PROPF_ENUMERABLE) != (prop->flags & PROPF_ENUMERABLE))))
            return throw_error(obj->ctx, JS_E_NONCONFIGURABLE_REDEFINED, name);
    }

    if(desc->explicit_value || (desc->mask & PROPF_WRITABLE)) {
        if(prop->type == PROP_ACCESSOR) {
            if(!(prop->flags & PROPF_CONFIGURABLE))
                return throw_error(obj->ctx, JS_E_NONCONFIGURABLE_REDEFINED, name);
            if(prop->u.accessor.getter)
                jsdisp_release(prop->u.accessor.getter);
            if(prop->u.accessor.setter)
                jsdisp_release(prop->u.accessor.setter);

            prop->type = PROP_JSVAL;
            hres = jsval_copy(desc->value, &prop->u.val);
            if(FAILED(hres)) {
                prop->u.val = jsval_undefined();
                return hres;
            }
        }else {
            if(!(prop->flags & PROPF_CONFIGURABLE) && !(prop->flags & PROPF_WRITABLE)) {
                if((desc->mask & PROPF_WRITABLE) && (desc->flags & PROPF_WRITABLE))
                    return throw_error(obj->ctx, JS_E_NONWRITABLE_MODIFIED, name);
                if(desc->explicit_value) {
                    if(prop->type == PROP_JSVAL) {
                        BOOL eq;
                        hres = jsval_strict_equal(desc->value, prop->u.val, &eq);
                        if(FAILED(hres))
                            return hres;
                        if(!eq)
                            return throw_error(obj->ctx, JS_E_NONWRITABLE_MODIFIED, name);
                    }else {
                        FIXME("redefinition of property type %d\n", prop->type);
                    }
                }
            }
            if(desc->explicit_value) {
                if(prop->type == PROP_JSVAL)
                    jsval_release(prop->u.val);
                else
                    prop->type = PROP_JSVAL;
                hres = jsval_copy(desc->value, &prop->u.val);
                if(FAILED(hres)) {
                    prop->u.val = jsval_undefined();
                    return hres;
                }
            }
        }
    }else if(desc->explicit_getter || desc->explicit_setter) {
        if(prop->type != PROP_ACCESSOR) {
            if(!(prop->flags & PROPF_CONFIGURABLE))
                return throw_error(obj->ctx, JS_E_NONCONFIGURABLE_REDEFINED, name);
            if(prop->type == PROP_JSVAL)
                jsval_release(prop->u.val);
            prop->type = PROP_ACCESSOR;
            prop->u.accessor.getter = prop->u.accessor.setter = NULL;
        }else if(!(prop->flags & PROPF_CONFIGURABLE)) {
            if((desc->explicit_getter && desc->getter != prop->u.accessor.getter)
               || (desc->explicit_setter && desc->setter != prop->u.accessor.setter))
                return throw_error(obj->ctx, JS_E_NONCONFIGURABLE_REDEFINED, name);
        }

        if(desc->explicit_getter) {
            if(prop->u.accessor.getter) {
                jsdisp_release(prop->u.accessor.getter);
                prop->u.accessor.getter = NULL;
            }
            if(desc->getter)
                prop->u.accessor.getter = jsdisp_addref(desc->getter);
        }
        if(desc->explicit_setter) {
            if(prop->u.accessor.setter) {
                jsdisp_release(prop->u.accessor.setter);
                prop->u.accessor.setter = NULL;
            }
            if(desc->setter)
                prop->u.accessor.setter = jsdisp_addref(desc->setter);
        }
    }

    prop->flags = (prop->flags & ~desc->mask) | (desc->flags & desc->mask);
    return S_OK;
}

HRESULT jsdisp_define_data_property(jsdisp_t *obj, const WCHAR *name, unsigned flags, jsval_t value)
{
    property_desc_t prop_desc = { flags, flags, TRUE };
    prop_desc.value = value;
    return jsdisp_define_property(obj, name, &prop_desc);
}

HRESULT jsdisp_get_prop_name(jsdisp_t *obj, DISPID id, jsstr_t **r)
{
    dispex_prop_t *prop = get_prop(obj, id);

    if(!prop || !prop->name || prop->type == PROP_DELETED)
        return DISP_E_MEMBERNOTFOUND;

    *r = jsstr_alloc(prop->name);
    return *r ? S_OK : E_OUTOFMEMORY;
}
