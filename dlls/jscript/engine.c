/*
 * Copyright 2008,2011 Jacek Caban for CodeWeavers
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


#include <math.h>
#include <assert.h>

#include "jscript.h"
#include "engine.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(jscript);

struct _except_frame_t {
    unsigned stack_top;
    scope_chain_t *scope;
    unsigned catch_off;
    unsigned finally_off;

    except_frame_t *next;
};

typedef struct {
    enum {
        EXPRVAL_JSVAL,
        EXPRVAL_IDREF,
        EXPRVAL_STACK_REF,
        EXPRVAL_INVALID
    } type;
    union {
        jsval_t val;
        struct {
            IDispatch *disp;
            DISPID id;
        } idref;
        unsigned off;
        HRESULT hres;
    } u;
} exprval_t;

static const size_t stack_size = 0x40000;

static HRESULT stack_push(script_ctx_t *ctx, jsval_t v)
{
    if(ctx->stack_top == stack_size)
        return JS_E_STACK_OVERFLOW;

    ctx->stack[ctx->stack_top++] = v;
    return S_OK;
}

static inline HRESULT stack_push_string(script_ctx_t *ctx, const WCHAR *str)
{
    jsstr_t *v;

    v = jsstr_alloc(str);
    if(!v)
        return E_OUTOFMEMORY;

    return stack_push(ctx, jsval_string(v));
}

static inline jsval_t stack_top(script_ctx_t *ctx)
{
    assert(ctx->stack_top > ctx->call_ctx->stack_base);
    return ctx->stack[ctx->stack_top-1];
}

static inline jsval_t *stack_top_ref(script_ctx_t *ctx, unsigned n)
{
    assert(ctx->stack_top > ctx->call_ctx->stack_base+n);
    return ctx->stack+ctx->stack_top-1-n;
}

static inline jsval_t stack_topn(script_ctx_t *ctx, unsigned n)
{
    return *stack_top_ref(ctx, n);
}

static inline jsval_t *stack_args(script_ctx_t *ctx, unsigned n)
{
    if(!n)
        return NULL;
    assert(ctx->stack_top > ctx->call_ctx->stack_base+n-1);
    return ctx->stack + ctx->stack_top-n;
}

static inline jsval_t stack_pop(script_ctx_t *ctx)
{
    assert(ctx->stack_top > ctx->call_ctx->stack_base);
    return ctx->stack[--ctx->stack_top];
}

static void stack_popn(script_ctx_t *ctx, unsigned n)
{
    while(n--)
        jsval_release(stack_pop(ctx));
}

static HRESULT stack_pop_number(script_ctx_t *ctx, double *r)
{
    jsval_t v;
    HRESULT hres;

    v = stack_pop(ctx);
    hres = to_number(ctx, v, r);
    jsval_release(v);
    return hres;
}

static HRESULT stack_pop_object(script_ctx_t *ctx, IDispatch **r)
{
    jsval_t v;
    HRESULT hres;

    v = stack_pop(ctx);
    if(is_object_instance(v)) {
        if(!get_object(v))
            return JS_E_OBJECT_REQUIRED;
        *r = get_object(v);
        return S_OK;
    }

    hres = to_object(ctx, v, r);
    jsval_release(v);
    return hres;
}

static inline HRESULT stack_pop_int(script_ctx_t *ctx, INT *r)
{
    return to_int32(ctx, stack_pop(ctx), r);
}

static inline HRESULT stack_pop_uint(script_ctx_t *ctx, DWORD *r)
{
    return to_uint32(ctx, stack_pop(ctx), r);
}

static inline unsigned local_off(call_frame_t *frame, int ref)
{
    return ref < 0
        ? frame->arguments_off - ref-1
        : frame->variables_off + ref;
}

static inline BSTR local_name(call_frame_t *frame, int ref)
{
    return ref < 0 ? frame->function->params[-ref-1] : frame->function->variables[ref].name;
}

/* Steals input reference even on failure. */
static HRESULT stack_push_exprval(script_ctx_t *ctx, exprval_t *val)
{
    HRESULT hres;

    switch(val->type) {
    case EXPRVAL_JSVAL:
        hres = stack_push(ctx, jsval_null());
        if(SUCCEEDED(hres))
            hres = stack_push(ctx, val->u.val);
        return hres;
    case EXPRVAL_IDREF:
        hres = stack_push(ctx, jsval_disp(val->u.idref.disp));
        if(SUCCEEDED(hres))
            hres = stack_push(ctx, jsval_number(val->u.idref.id));
        else
            IDispatch_Release(val->u.idref.disp);
        return hres;
    case EXPRVAL_STACK_REF:
        hres = stack_push(ctx, jsval_number(val->u.off));
        if(SUCCEEDED(hres))
            hres = stack_push(ctx, jsval_undefined());
        return hres;
    case EXPRVAL_INVALID:
        hres = stack_push(ctx, jsval_undefined());
        if(SUCCEEDED(hres))
            hres = stack_push(ctx, jsval_number(val->u.hres));
        return hres;
    }

    assert(0);
    return E_FAIL;
}

static BOOL stack_topn_exprval(script_ctx_t *ctx, unsigned n, exprval_t *r)
{
    jsval_t v = stack_topn(ctx, n+1);

    switch(jsval_type(v)) {
    case JSV_NUMBER: {
        call_frame_t *frame = ctx->call_ctx;
        scope_chain_t *scope;
        unsigned off = get_number(v);

        if(!frame->base_scope->frame && off >= frame->arguments_off) {
            DISPID id;
            BSTR name;
            HRESULT hres = E_FAIL;

            /* Got stack reference in deoptimized code. Need to convert it back to variable object reference. */

            assert(off < frame->variables_off + frame->function->var_cnt);
            if (off >= frame->variables_off)
            {
                name = frame->function->variables[off - frame->variables_off].name;
                scope = frame->scope;
            }
            else
            {
                name = frame->function->params[off - frame->arguments_off];
                scope = frame->base_scope;
            }

            while (1)
            {
                if (scope->jsobj && SUCCEEDED(hres = jsdisp_get_id(scope->jsobj, name, 0, &id)))
                    break;
                if (scope == frame->base_scope)
                {
                    r->type = EXPRVAL_INVALID;
                    r->u.hres = hres;
                    return FALSE;
                }
                scope = scope->next;
            }

            *stack_top_ref(ctx, n+1) = jsval_obj(jsdisp_addref(scope->jsobj));
            *stack_top_ref(ctx, n) = jsval_number(id);
            r->type = EXPRVAL_IDREF;
            r->u.idref.disp = scope->obj;
            r->u.idref.id = id;
            return TRUE;
        }

        r->type = EXPRVAL_STACK_REF;
        r->u.off = off;
        return TRUE;
    }
    case JSV_OBJECT:
        r->type = EXPRVAL_IDREF;
        r->u.idref.disp = get_object(v);
        assert(is_number(stack_topn(ctx, n)));
        r->u.idref.id = get_number(stack_topn(ctx, n));
        return TRUE;
    case JSV_UNDEFINED:
        r->type = EXPRVAL_INVALID;
        assert(is_number(stack_topn(ctx, n)));
        r->u.hres = get_number(stack_topn(ctx, n));
        return FALSE;
    case JSV_NULL:
        r->type = EXPRVAL_JSVAL;
        r->u.val = stack_topn(ctx, n);
        return TRUE;
    default:
        assert(0);
        return FALSE;
    }
}

static inline BOOL stack_pop_exprval(script_ctx_t *ctx, exprval_t *r)
{
    BOOL ret = stack_topn_exprval(ctx, 0, r);
    ctx->stack_top -= 2;
    return ret;
}

static HRESULT exprval_propput(script_ctx_t *ctx, exprval_t *ref, jsval_t v)
{
    switch(ref->type) {
    case EXPRVAL_STACK_REF: {
        jsval_t *r = ctx->stack + ref->u.off;
        jsval_release(*r);
        return jsval_copy(v, r);
    }
    case EXPRVAL_IDREF:
        return disp_propput(ctx, ref->u.idref.disp, ref->u.idref.id, v);
    case EXPRVAL_JSVAL:
        WARN("ignoring an attempt to set value reference\n");
        return S_OK;
    default:
        assert(0);
        return E_FAIL;
    }
}

static HRESULT exprval_propget(script_ctx_t *ctx, exprval_t *ref, jsval_t *r)
{
    switch(ref->type) {
    case EXPRVAL_STACK_REF:
        return jsval_copy(ctx->stack[ref->u.off], r);
    case EXPRVAL_IDREF:
        return disp_propget(ctx, ref->u.idref.disp, ref->u.idref.id, r);
    case EXPRVAL_JSVAL:
        return jsval_copy(ref->u.val, r);
    default:
        assert(0);
        return E_FAIL;
    }
}

static HRESULT exprval_call(script_ctx_t *ctx, exprval_t *ref, WORD flags, unsigned argc, jsval_t *argv, jsval_t *r)
{
    switch(ref->type) {
    case EXPRVAL_STACK_REF: {
        jsval_t v = ctx->stack[ref->u.off];

        if(!is_object_instance(v)) {
            FIXME("invoke %s\n", debugstr_jsval(v));
            return E_FAIL;
        }

        return disp_call_value(ctx, get_object(v), NULL, flags, argc, argv, r);
    }
    case EXPRVAL_IDREF:
        return disp_call(ctx, ref->u.idref.disp, ref->u.idref.id, flags, argc, argv, r);
    case EXPRVAL_JSVAL: {
        IDispatch *obj;
        HRESULT hres;

        hres = to_object(ctx, ref->u.val, &obj);
        if(SUCCEEDED(hres)) {
            hres = disp_call_value(ctx, obj, NULL, flags, argc, argv, r);
            IDispatch_Release(obj);
        }
        return hres;
    }
    default:
        assert(0);
        return E_FAIL;
    }
}

/* ECMA-262 3rd Edition    8.7.1 */
/* Steals input reference. */
static HRESULT exprval_to_value(script_ctx_t *ctx, exprval_t *ref, jsval_t *r)
{
    HRESULT hres;

    if(ref->type == EXPRVAL_JSVAL) {
        *r = ref->u.val;
        return S_OK;
    }

    hres = exprval_propget(ctx, ref, r);

    if(ref->type == EXPRVAL_IDREF)
        IDispatch_Release(ref->u.idref.disp);
    return hres;
}

static void exprval_release(exprval_t *val)
{
    switch(val->type) {
    case EXPRVAL_JSVAL:
        jsval_release(val->u.val);
        return;
    case EXPRVAL_IDREF:
        if(val->u.idref.disp)
            IDispatch_Release(val->u.idref.disp);
        return;
    case EXPRVAL_STACK_REF:
    case EXPRVAL_INVALID:
        return;
    }
}

static inline void exprval_set_exception(exprval_t *val, HRESULT hres)
{
    val->type = EXPRVAL_INVALID;
    val->u.hres = hres;
}

static inline void exprval_set_disp_ref(exprval_t *ref, IDispatch *obj, DISPID id)
{
    ref->type = EXPRVAL_IDREF;
    IDispatch_AddRef(ref->u.idref.disp = obj);
    ref->u.idref.id = id;
}

static inline jsval_t steal_ret(call_frame_t *frame)
{
    jsval_t r = frame->ret;
    frame->ret = jsval_undefined();
    return r;
}

static inline void clear_acc(script_ctx_t *ctx)
{
    jsval_release(ctx->acc);
    ctx->acc = jsval_undefined();
}

static HRESULT scope_push(scope_chain_t *scope, jsdisp_t *jsobj, IDispatch *obj, scope_chain_t **ret)
{
    scope_chain_t *new_scope;

    new_scope = heap_alloc(sizeof(scope_chain_t));
    if(!new_scope)
        return E_OUTOFMEMORY;

    new_scope->ref = 1;

    if (obj)
        IDispatch_AddRef(obj);
    new_scope->jsobj = jsobj;
    new_scope->obj = obj;
    new_scope->frame = NULL;
    new_scope->next = scope ? scope_addref(scope) : NULL;
    new_scope->scope_index = 0;

    *ret = new_scope;
    return S_OK;
}

static void scope_pop(scope_chain_t **scope)
{
    scope_chain_t *tmp;

    tmp = *scope;
    *scope = tmp->next;
    scope_release(tmp);
}

void scope_release(scope_chain_t *scope)
{
    if(--scope->ref)
        return;

    if(scope->next)
        scope_release(scope->next);

    if (scope->obj)
        IDispatch_Release(scope->obj);
    heap_free(scope);
}

static HRESULT disp_get_id(script_ctx_t *ctx, IDispatch *disp, const WCHAR *name, BSTR name_bstr, DWORD flags, DISPID *id)
{
    IDispatchEx *dispex;
    jsdisp_t *jsdisp;
    BSTR bstr;
    HRESULT hres;

    jsdisp = iface_to_jsdisp(disp);
    if(jsdisp) {
        hres = jsdisp_get_id(jsdisp, name, flags, id);
        jsdisp_release(jsdisp);
        return hres;
    }

    if(name_bstr) {
        bstr = name_bstr;
    }else {
        bstr = SysAllocString(name);
        if(!bstr)
            return E_OUTOFMEMORY;
    }

    *id = 0;
    hres = IDispatch_QueryInterface(disp, &IID_IDispatchEx, (void**)&dispex);
    if(SUCCEEDED(hres)) {
        hres = IDispatchEx_GetDispID(dispex, bstr, make_grfdex(ctx, flags|fdexNameCaseSensitive), id);
        IDispatchEx_Release(dispex);
    }else {
        TRACE("using IDispatch\n");
        hres = IDispatch_GetIDsOfNames(disp, &IID_NULL, &bstr, 1, 0, id);
    }

    if(name_bstr != bstr)
        SysFreeString(bstr);
    return hres;
}

static HRESULT disp_cmp(IDispatch *disp1, IDispatch *disp2, BOOL *ret)
{
    IObjectIdentity *identity;
    IUnknown *unk1, *unk2;
    HRESULT hres;

    if(disp1 == disp2) {
        *ret = TRUE;
        return S_OK;
    }

    if(!disp1 || !disp2) {
        *ret = FALSE;
        return S_OK;
    }

    hres = IDispatch_QueryInterface(disp1, &IID_IUnknown, (void**)&unk1);
    if(FAILED(hres))
        return hres;

    hres = IDispatch_QueryInterface(disp2, &IID_IUnknown, (void**)&unk2);
    if(FAILED(hres)) {
        IUnknown_Release(unk1);
        return hres;
    }

    if(unk1 == unk2) {
        *ret = TRUE;
    }else {
        hres = IUnknown_QueryInterface(unk1, &IID_IObjectIdentity, (void**)&identity);
        if(SUCCEEDED(hres)) {
            hres = IObjectIdentity_IsEqualObject(identity, unk2);
            IObjectIdentity_Release(identity);
            *ret = hres == S_OK;
        }else {
            *ret = FALSE;
        }
    }

    IUnknown_Release(unk1);
    IUnknown_Release(unk2);
    return S_OK;
}

/* ECMA-262 3rd Edition    11.9.6 */
HRESULT jsval_strict_equal(jsval_t lval, jsval_t rval, BOOL *ret)
{
    jsval_type_t type = jsval_type(lval);

    TRACE("\n");

    if(type != jsval_type(rval)) {
        if(is_null_instance(lval))
            *ret = is_null_instance(rval);
        else
            *ret = FALSE;
        return S_OK;
    }

    switch(type) {
    case JSV_UNDEFINED:
    case JSV_NULL:
        *ret = TRUE;
        break;
    case JSV_OBJECT:
        return disp_cmp(get_object(lval), get_object(rval), ret);
    case JSV_STRING:
        *ret = jsstr_eq(get_string(lval), get_string(rval));
        break;
    case JSV_NUMBER:
        *ret = get_number(lval) == get_number(rval);
        break;
    case JSV_BOOL:
        *ret = !get_bool(lval) == !get_bool(rval);
        break;
    case JSV_VARIANT:
        WARN("VARIANT type, returning false\n");
        *ret = FALSE;
        return S_OK;
    }

    return S_OK;
}

static HRESULT detach_scope(script_ctx_t *ctx, call_frame_t *frame, scope_chain_t *scope)
{
    function_code_t *func = frame->function;
    unsigned int i, index;
    HRESULT hres;

    if (!scope->frame)
        return S_OK;

    assert(scope->frame == frame);
    scope->frame = NULL;

    if (!scope->jsobj)
    {
        assert(!scope->obj);

        if (FAILED(hres = create_object(ctx, NULL, &scope->jsobj)))
            return hres;
        scope->obj = to_disp(scope->jsobj);
    }

    if (scope == frame->base_scope && func->name && ctx->version >= SCRIPTLANGUAGEVERSION_ES5)
        jsdisp_propput_name(scope->jsobj, func->name, jsval_obj(jsdisp_addref(frame->function_instance)));

    index = scope->scope_index;
    for(i = 0; i < frame->function->local_scopes[index].locals_cnt; i++)
    {
        WCHAR *name = frame->function->local_scopes[index].locals[i].name;
        int ref = frame->function->local_scopes[index].locals[i].ref;

        if (FAILED(hres = jsdisp_propput_name(scope->jsobj, name, ctx->stack[local_off(frame, ref)])))
            return hres;
        if (frame->function->variables[ref].func_id != -1 && scope != frame->base_scope
                && FAILED(hres = jsdisp_propput_name(frame->variable_obj, name, ctx->stack[local_off(frame, ref)])))
            return hres;
    }
    return S_OK;
}

static HRESULT detach_scope_chain(script_ctx_t *ctx, call_frame_t *frame, scope_chain_t *scope)
{
    HRESULT hres;

    if (scope != frame->base_scope && FAILED(hres = detach_scope_chain(ctx, frame, scope->next)))
        return hres;
    return detach_scope(ctx, frame, scope);
}

/*
 * Transfers local variables from stack to variable object.
 * It's slow, so we want to avoid it as much as possible.
 */
static HRESULT detach_variable_object(script_ctx_t *ctx, call_frame_t *frame, BOOL from_release)
{
    HRESULT hres;

    if(!frame->base_scope || !frame->base_scope->frame)
        return S_OK;

    TRACE("detaching %p\n", frame);

    assert(frame == frame->base_scope->frame);
    assert(frame->variable_obj == frame->base_scope->jsobj);

    if(!from_release && !frame->arguments_obj) {
        hres = setup_arguments_object(ctx, frame);
        if(FAILED(hres))
            return hres;
    }

    TRACE("detaching scope chain %p, frame %p.\n", ctx->call_ctx->scope, frame);
    return detach_scope_chain(ctx, frame, ctx->call_ctx->scope);
}

static BOOL lookup_global_members(script_ctx_t *ctx, BSTR identifier, exprval_t *ret)
{
    named_item_t *item;
    DISPID id;
    HRESULT hres;

    LIST_FOR_EACH_ENTRY(item, &ctx->named_items, named_item_t, entry) {
        if(item->flags & SCRIPTITEM_GLOBALMEMBERS) {
            hres = disp_get_id(ctx, item->disp, identifier, identifier, 0, &id);
            if(SUCCEEDED(hres)) {
                if(ret)
                    exprval_set_disp_ref(ret, item->disp, id);
                return TRUE;
            }
        }
    }

    return FALSE;
}

IDispatch *lookup_global_host(script_ctx_t *ctx)
{
    IDispatch *disp = NULL;
    named_item_t *item;

    LIST_FOR_EACH_ENTRY(item, &ctx->named_items, named_item_t, entry) {
        if(!(item->flags & SCRIPTITEM_GLOBALMEMBERS)) continue;
        disp = item->disp;
        break;
    }
    if(!disp) disp = to_disp(ctx->global);

    return disp;
}

static int __cdecl local_ref_cmp(const void *key, const void *ref)
{
    return wcscmp((const WCHAR*)key, ((const local_ref_t*)ref)->name);
}

local_ref_t *lookup_local(const function_code_t *function, const WCHAR *identifier, unsigned int scope)
{
    return bsearch(identifier, function->local_scopes[scope].locals, function->local_scopes[scope].locals_cnt,
            sizeof(*function->local_scopes[scope].locals), local_ref_cmp);
}

/* ECMA-262 3rd Edition    10.1.4 */
static HRESULT identifier_eval(script_ctx_t *ctx, BSTR identifier, exprval_t *ret)
{
    scope_chain_t *scope;
    named_item_t *item;
    DISPID id = 0;
    HRESULT hres;

    TRACE("%s\n", debugstr_w(identifier));

    if(ctx->call_ctx) {
        for(scope = ctx->call_ctx->scope; scope; scope = scope->next) {
            if(scope->frame) {
                function_code_t *func = scope->frame->function;
                local_ref_t *ref = lookup_local(func, identifier, scope->scope_index);

                if(ref) {
                    ret->type = EXPRVAL_STACK_REF;
                    ret->u.off = local_off(scope->frame, ref->ref);
                    TRACE("returning ref %d for %d\n", ret->u.off, ref->ref);
                    return S_OK;
                }

                if(!wcscmp(identifier, L"arguments")) {
                    hres = detach_variable_object(ctx, scope->frame, FALSE);
                    if(FAILED(hres))
                        return hres;
                }

                /* ECMA-262 5.1 Edition    13 */
                if(func->name && ctx->version >= SCRIPTLANGUAGEVERSION_ES5 && !wcscmp(identifier, func->name)) {
                    TRACE("returning a function from scope chain\n");
                    ret->type = EXPRVAL_JSVAL;
                    ret->u.val = jsval_obj(jsdisp_addref(scope->frame->function_instance));
                    return S_OK;
                }
            }

            if (!scope->jsobj && !scope->obj)
                continue;

            if(scope->jsobj)
                hres = jsdisp_get_id(scope->jsobj, identifier, fdexNameImplicit, &id);
            else
                hres = disp_get_id(ctx, scope->obj, identifier, identifier, fdexNameImplicit, &id);
            if(SUCCEEDED(hres)) {
                exprval_set_disp_ref(ret, scope->obj, id);
                return S_OK;
            }
        }

        item = ctx->call_ctx->bytecode->named_item;
        if(item) {
            hres = jsdisp_get_id(item->script_obj, identifier, 0, &id);
            if(SUCCEEDED(hres)) {
                exprval_set_disp_ref(ret, to_disp(item->script_obj), id);
                return S_OK;
            }
            if(!(item->flags & SCRIPTITEM_CODEONLY)) {
                hres = disp_get_id(ctx, item->disp, identifier, identifier, 0, &id);
                if(SUCCEEDED(hres)) {
                    exprval_set_disp_ref(ret, item->disp, id);
                    return S_OK;
                }
            }
        }
    }

    hres = jsdisp_get_id(ctx->global, identifier, 0, &id);
    if(SUCCEEDED(hres)) {
        exprval_set_disp_ref(ret, to_disp(ctx->global), id);
        return S_OK;
    }

    item = lookup_named_item(ctx, identifier, SCRIPTITEM_ISVISIBLE);
    if(item) {
        IDispatch_AddRef(item->disp);
        ret->type = EXPRVAL_JSVAL;
        ret->u.val = jsval_disp(item->disp);
        return S_OK;
    }

    if(lookup_global_members(ctx, identifier, ret))
        return S_OK;

    exprval_set_exception(ret, JS_E_UNDEFINED_VARIABLE);
    return S_OK;
}

static inline BSTR get_op_bstr(script_ctx_t *ctx, int i)
{
    call_frame_t *frame = ctx->call_ctx;
    return frame->bytecode->instrs[frame->ip].u.arg[i].bstr;
}

static inline unsigned get_op_uint(script_ctx_t *ctx, int i)
{
    call_frame_t *frame = ctx->call_ctx;
    return frame->bytecode->instrs[frame->ip].u.arg[i].uint;
}

static inline unsigned get_op_int(script_ctx_t *ctx, int i)
{
    call_frame_t *frame = ctx->call_ctx;
    return frame->bytecode->instrs[frame->ip].u.arg[i].lng;
}

static inline jsstr_t *get_op_str(script_ctx_t *ctx, int i)
{
    call_frame_t *frame = ctx->call_ctx;
    return frame->bytecode->instrs[frame->ip].u.arg[i].str;
}

static inline double get_op_double(script_ctx_t *ctx)
{
    call_frame_t *frame = ctx->call_ctx;
    return frame->bytecode->instrs[frame->ip].u.dbl;
}

static inline void jmp_next(script_ctx_t *ctx)
{
    ctx->call_ctx->ip++;
}

static inline void jmp_abs(script_ctx_t *ctx, unsigned dst)
{
    ctx->call_ctx->ip = dst;
}

/* ECMA-262 3rd Edition    12.6.4 */
static HRESULT interp_forin(script_ctx_t *ctx)
{
    const HRESULT arg = get_op_uint(ctx, 0);
    IDispatch *obj = NULL;
    IDispatchEx *dispex;
    exprval_t prop_ref;
    DISPID id;
    BSTR name = NULL;
    HRESULT hres;

    TRACE("\n");

    assert(is_number(stack_top(ctx)));
    id = get_number(stack_top(ctx));

    if(!stack_topn_exprval(ctx, 1, &prop_ref)) {
        FIXME("invalid ref: %08x\n", prop_ref.u.hres);
        return E_FAIL;
    }

    if(is_object_instance(stack_topn(ctx, 3)))
        obj = get_object(stack_topn(ctx, 3));

    if(obj) {
        hres = IDispatch_QueryInterface(obj, &IID_IDispatchEx, (void**)&dispex);
        if(SUCCEEDED(hres)) {
            hres = IDispatchEx_GetNextDispID(dispex, fdexEnumDefault, id, &id);
            if(hres == S_OK)
                hres = IDispatchEx_GetMemberName(dispex, id, &name);
            IDispatchEx_Release(dispex);
            if(FAILED(hres))
                return hres;
        }else {
            TRACE("No IDispatchEx\n");
        }
    }

    if(name) {
        jsstr_t *str;

        str = jsstr_alloc_len(name, SysStringLen(name));
        SysFreeString(name);
        if(!str)
            return E_OUTOFMEMORY;

        stack_pop(ctx);
        stack_push(ctx, jsval_number(id)); /* safe, just after pop() */

        hres = exprval_propput(ctx, &prop_ref, jsval_string(str));
        jsstr_release(str);
        if(FAILED(hres))
            return hres;

        jmp_next(ctx);
    }else {
        stack_popn(ctx, 4);
        jmp_abs(ctx, arg);
    }
    return S_OK;
}

static HRESULT scope_init_locals(script_ctx_t *ctx)
{
    call_frame_t *frame = ctx->call_ctx;
    unsigned int i, off, index;
    scope_chain_t *scope;
    BOOL detached_vars;
    HRESULT hres;

    scope = frame->scope;
    index = scope->scope_index;
    detached_vars = !(frame->base_scope && frame->base_scope->frame);

    if (!detached_vars)
    {
        assert(frame->base_scope->frame == frame);
        frame->scope->frame = ctx->call_ctx;
    }
    else if (!scope->jsobj)
    {
        assert(!scope->obj);
        if (FAILED(hres = create_object(ctx, NULL, &scope->jsobj)))
            return hres;
        scope->obj = to_disp(scope->jsobj);
    }

    for(i = 0; i < frame->function->local_scopes[index].locals_cnt; i++)
    {
        WCHAR *name = frame->function->local_scopes[index].locals[i].name;
        int ref = frame->function->local_scopes[index].locals[i].ref;
        jsdisp_t *func_obj;
        jsval_t val;

        if (frame->function->variables[ref].func_id != -1)
        {
            TRACE("function %s %d\n", debugstr_w(name), i);

            if (FAILED(hres = create_source_function(ctx, frame->bytecode, frame->function->funcs
                    + frame->function->variables[ref].func_id, ctx->call_ctx->scope, &func_obj)))
                return hres;
            val = jsval_obj(func_obj);
            if (detached_vars && FAILED(hres = jsdisp_propput_name(frame->variable_obj, name, jsval_obj(func_obj))))
                return hres;
        }
        else
        {
            val = jsval_undefined();
        }

        if (detached_vars)
        {
            if (FAILED(hres = jsdisp_propput_name(scope->jsobj, name, val)))
                return hres;
        }
        else
        {
            off = local_off(frame, ref);
            jsval_release(ctx->stack[off]);
            ctx->stack[off] = val;
        }
    }
    return S_OK;
}

/* ECMA-262 3rd Edition    12.10 */
static HRESULT interp_push_with_scope(script_ctx_t *ctx)
{
    IDispatch *disp;
    jsval_t v;
    HRESULT hres;

    TRACE("\n");

    v = stack_pop(ctx);
    hres = to_object(ctx, v, &disp);
    jsval_release(v);
    if(FAILED(hres))
        return hres;

    hres = scope_push(ctx->call_ctx->scope, to_jsdisp(disp), disp, &ctx->call_ctx->scope);
    IDispatch_Release(disp);
    return hres;
}

/* ECMA-262 10th Edition   13.3.1 */
static HRESULT interp_push_block_scope(script_ctx_t *ctx)
{
    unsigned int scope_index = get_op_uint(ctx, 0);
    call_frame_t *frame = ctx->call_ctx;
    HRESULT hres;

    TRACE("scope_index %u.\n", scope_index);

    hres = scope_push(ctx->call_ctx->scope, NULL, NULL, &frame->scope);

    if (FAILED(hres) || !scope_index)
        return hres;

    frame->scope->scope_index = scope_index;

    return scope_init_locals(ctx);
}

/* ECMA-262 3rd Edition    12.10 */
static HRESULT interp_pop_scope(script_ctx_t *ctx)
{
    TRACE("\n");

    if(ctx->call_ctx->scope->ref > 1) {
        HRESULT hres = detach_variable_object(ctx, ctx->call_ctx, FALSE);
        if(FAILED(hres))
            ERR("Failed to detach variable object: %08x\n", hres);
    }

    scope_pop(&ctx->call_ctx->scope);
    return S_OK;
}

/* ECMA-262 3rd Edition    12.13 */
static HRESULT interp_case(script_ctx_t *ctx)
{
    const unsigned arg = get_op_uint(ctx, 0);
    jsval_t v;
    BOOL b;
    HRESULT hres;

    TRACE("\n");

    v = stack_pop(ctx);
    hres = jsval_strict_equal(stack_top(ctx), v, &b);
    jsval_release(v);
    if(FAILED(hres))
        return hres;

    if(b) {
        stack_popn(ctx, 1);
        jmp_abs(ctx, arg);
    }else {
        jmp_next(ctx);
    }
    return S_OK;
}

static void set_error_value(script_ctx_t *ctx, jsval_t value)
{
    jsexcept_t *ei = ctx->ei;
    jsdisp_t *obj;

    reset_ei(ei);
    ei->error = JS_E_EXCEPTION_THROWN;
    ei->valid_value = TRUE;
    ei->value = value;

    if(is_object_instance(value) && get_object(value) && (obj = to_jsdisp(get_object(value)))) {
        UINT32 number;
        jsstr_t *str;
        jsval_t v;
        HRESULT hres;

        /* FIXME: We should check if object is an error instance */

        hres = jsdisp_propget_name(obj, L"number", &v);
        if(SUCCEEDED(hres)) {
            hres = to_uint32(ctx, v, &number);
            if(SUCCEEDED(hres))
                ei->error = FAILED(number) ? number : E_FAIL;
            jsval_release(v);
        }

        hres = jsdisp_propget_name(obj, L"description", &v);
        if(SUCCEEDED(hres)) {
            hres = to_string(ctx, v, &str);
            if(SUCCEEDED(hres))
                ei->message = str;
            jsval_release(v);
        }
    }
}

/* ECMA-262 3rd Edition    12.13 */
static HRESULT interp_throw(script_ctx_t *ctx)
{
    TRACE("\n");

    set_error_value(ctx, stack_pop(ctx));
    return DISP_E_EXCEPTION;
}

static HRESULT interp_throw_ref(script_ctx_t *ctx)
{
    const HRESULT arg = get_op_uint(ctx, 0);

    TRACE("%08x\n", arg);

    return arg;
}

static HRESULT interp_throw_type(script_ctx_t *ctx)
{
    const HRESULT hres = get_op_uint(ctx, 0);
    jsstr_t *str = get_op_str(ctx, 1);
    const WCHAR *ptr;

    TRACE("%08x %s\n", hres, debugstr_jsstr(str));

    ptr = jsstr_flatten(str);
    return ptr ? throw_error(ctx, hres, ptr) : E_OUTOFMEMORY;
}

/* ECMA-262 3rd Edition    12.14 */
static HRESULT interp_push_except(script_ctx_t *ctx)
{
    const unsigned catch_off = get_op_uint(ctx, 0);
    const unsigned finally_off = get_op_uint(ctx, 1);
    call_frame_t *frame = ctx->call_ctx;
    except_frame_t *except;

    TRACE("\n");

    except = heap_alloc(sizeof(*except));
    if(!except)
        return E_OUTOFMEMORY;

    except->stack_top = ctx->stack_top;
    except->scope = frame->scope;
    except->catch_off = catch_off;
    except->finally_off = finally_off;
    except->next = frame->except_frame;
    frame->except_frame = except;
    return S_OK;
}

/* ECMA-262 3rd Edition    12.14 */
static HRESULT interp_pop_except(script_ctx_t *ctx)
{
    const unsigned ret_off = get_op_uint(ctx, 0);
    call_frame_t *frame = ctx->call_ctx;
    except_frame_t *except;
    unsigned finally_off;

    TRACE("%u\n", ret_off);

    except = frame->except_frame;
    assert(except != NULL);

    finally_off = except->finally_off;
    frame->except_frame = except->next;
    heap_free(except);

    if(finally_off) {
        HRESULT hres;

        hres = stack_push(ctx, jsval_number(ret_off));
        if(FAILED(hres))
            return hres;
        hres = stack_push(ctx, jsval_bool(TRUE));
        if(FAILED(hres))
            return hres;
        frame->ip = finally_off;
    }else {
        frame->ip = ret_off;
    }

    return S_OK;
}

/* ECMA-262 3rd Edition    12.14 */
static HRESULT interp_end_finally(script_ctx_t *ctx)
{
    call_frame_t *frame = ctx->call_ctx;
    jsval_t v;

    TRACE("\n");

    v = stack_pop(ctx);
    assert(is_bool(v));

    if(!get_bool(v)) {
        TRACE("passing exception\n");

        set_error_value(ctx, stack_pop(ctx));
        return DISP_E_EXCEPTION;
    }

    v = stack_pop(ctx);
    assert(is_number(v));
    frame->ip = get_number(v);
    return S_OK;
}

static HRESULT interp_enter_catch(script_ctx_t *ctx)
{
    const BSTR ident = get_op_bstr(ctx, 0);
    jsdisp_t *scope_obj;
    jsval_t v;
    HRESULT hres;

    hres = create_dispex(ctx, NULL, NULL, &scope_obj);
    if(FAILED(hres))
        return hres;

    v = stack_pop(ctx);
    hres = jsdisp_propput_name(scope_obj, ident, v);
    jsval_release(v);
    if(SUCCEEDED(hres))
        hres = scope_push(ctx->call_ctx->scope, scope_obj, to_disp(scope_obj), &ctx->call_ctx->scope);
    jsdisp_release(scope_obj);
    return hres;
}

/* ECMA-262 3rd Edition    13 */
static HRESULT interp_func(script_ctx_t *ctx)
{
    unsigned func_idx = get_op_uint(ctx, 0);
    call_frame_t *frame = ctx->call_ctx;
    jsdisp_t *dispex;
    HRESULT hres;

    TRACE("%d\n", func_idx);

    hres = create_source_function(ctx, frame->bytecode, frame->function->funcs+func_idx,
            frame->scope, &dispex);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, jsval_obj(dispex));
}

/* ECMA-262 3rd Edition    11.2.1 */
static HRESULT interp_array(script_ctx_t *ctx)
{
    jsstr_t *name_str;
    const WCHAR *name;
    jsval_t v, namev;
    IDispatch *obj;
    DISPID id;
    HRESULT hres;

    TRACE("\n");

    namev = stack_pop(ctx);

    hres = stack_pop_object(ctx, &obj);
    if(FAILED(hres)) {
        jsval_release(namev);
        return hres;
    }

    hres = to_flat_string(ctx, namev, &name_str, &name);
    jsval_release(namev);
    if(FAILED(hres)) {
        IDispatch_Release(obj);
        return hres;
    }

    hres = disp_get_id(ctx, obj, name, NULL, 0, &id);
    jsstr_release(name_str);
    if(SUCCEEDED(hres)) {
        hres = disp_propget(ctx, obj, id, &v);
    }else if(hres == DISP_E_UNKNOWNNAME) {
        v = jsval_undefined();
        hres = S_OK;
    }
    IDispatch_Release(obj);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, v);
}

/* ECMA-262 3rd Edition    11.2.1 */
static HRESULT interp_member(script_ctx_t *ctx)
{
    const BSTR arg = get_op_bstr(ctx, 0);
    IDispatch *obj;
    jsval_t v;
    DISPID id;
    HRESULT hres;

    TRACE("\n");

    hres = stack_pop_object(ctx, &obj);
    if(FAILED(hres))
        return hres;

    hres = disp_get_id(ctx, obj, arg, arg, 0, &id);
    if(SUCCEEDED(hres)) {
        hres = disp_propget(ctx, obj, id, &v);
    }else if(hres == DISP_E_UNKNOWNNAME) {
        v = jsval_undefined();
        hres = S_OK;
    }
    IDispatch_Release(obj);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, v);
}

/* ECMA-262 3rd Edition    11.2.1 */
static HRESULT interp_memberid(script_ctx_t *ctx)
{
    const unsigned arg = get_op_uint(ctx, 0);
    jsval_t objv, namev;
    const WCHAR *name;
    jsstr_t *name_str;
    IDispatch *obj;
    exprval_t ref;
    DISPID id;
    HRESULT hres;

    TRACE("%x\n", arg);

    namev = stack_pop(ctx);
    objv = stack_pop(ctx);

    hres = to_object(ctx, objv, &obj);
    jsval_release(objv);
    if(SUCCEEDED(hres)) {
        hres = to_flat_string(ctx, namev, &name_str, &name);
        if(FAILED(hres))
            IDispatch_Release(obj);
    }
    jsval_release(namev);
    if(FAILED(hres))
        return hres;

    hres = disp_get_id(ctx, obj, name, NULL, arg, &id);
    jsstr_release(name_str);
    if(SUCCEEDED(hres)) {
        ref.type = EXPRVAL_IDREF;
        ref.u.idref.disp = obj;
        ref.u.idref.id = id;
    }else {
        IDispatch_Release(obj);
        if(hres == DISP_E_UNKNOWNNAME && !(arg & fdexNameEnsure)) {
            exprval_set_exception(&ref, JS_E_INVALID_PROPERTY);
            hres = S_OK;
        }else {
            ERR("failed %08x\n", hres);
            return hres;
        }
    }

    return stack_push_exprval(ctx, &ref);
}

/* ECMA-262 3rd Edition    11.2.1 */
static HRESULT interp_refval(script_ctx_t *ctx)
{
    exprval_t ref;
    jsval_t v;
    HRESULT hres;

    TRACE("\n");

    if(!stack_topn_exprval(ctx, 0, &ref))
        return JS_E_ILLEGAL_ASSIGN;

    hres = exprval_propget(ctx, &ref, &v);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, v);
}

/* ECMA-262 3rd Edition    11.2.2 */
static HRESULT interp_new(script_ctx_t *ctx)
{
    const unsigned argc = get_op_uint(ctx, 0);
    jsval_t constr;

    TRACE("%d\n", argc);

    constr = stack_topn(ctx, argc);

    /* NOTE: Should use to_object here */

    if(is_null(constr))
        return JS_E_OBJECT_EXPECTED;
    else if(!is_object_instance(constr))
        return JS_E_INVALID_ACTION;
    else if(!get_object(constr))
        return JS_E_INVALID_PROPERTY;

    clear_acc(ctx);
    return disp_call_value(ctx, get_object(constr), NULL, DISPATCH_CONSTRUCT | DISPATCH_JSCRIPT_CALLEREXECSSOURCE,
                           argc, stack_args(ctx, argc), &ctx->acc);
}

/* ECMA-262 3rd Edition    11.2.3 */
static HRESULT interp_call(script_ctx_t *ctx)
{
    const unsigned argn = get_op_uint(ctx, 0);
    const int do_ret = get_op_int(ctx, 1);
    jsval_t obj;

    TRACE("%d %d\n", argn, do_ret);

    obj = stack_topn(ctx, argn);
    if(!is_object_instance(obj))
        return JS_E_INVALID_PROPERTY;

    clear_acc(ctx);
    return disp_call_value(ctx, get_object(obj), NULL, DISPATCH_METHOD | DISPATCH_JSCRIPT_CALLEREXECSSOURCE,
                           argn, stack_args(ctx, argn), do_ret ? &ctx->acc : NULL);
}

/* ECMA-262 3rd Edition    11.2.3 */
static HRESULT interp_call_member(script_ctx_t *ctx)
{
    const unsigned argn = get_op_uint(ctx, 0);
    const int do_ret = get_op_int(ctx, 1);
    exprval_t ref;

    TRACE("%d %d\n", argn, do_ret);

    if(!stack_topn_exprval(ctx, argn, &ref))
        return ref.u.hres;

    clear_acc(ctx);
    return exprval_call(ctx, &ref, DISPATCH_METHOD | DISPATCH_JSCRIPT_CALLEREXECSSOURCE,
            argn, stack_args(ctx, argn), do_ret ? &ctx->acc : NULL);
}

/* ECMA-262 3rd Edition    11.1.1 */
static HRESULT interp_this(script_ctx_t *ctx)
{
    IDispatch *this_obj = ctx->call_ctx->this_obj;

    TRACE("\n");

    if(!this_obj) {
        named_item_t *item = ctx->call_ctx->bytecode->named_item;

        if(item)
            this_obj = (item->flags & SCRIPTITEM_CODEONLY) ? to_disp(item->script_obj) : item->disp;
        else
            this_obj = lookup_global_host(ctx);
    }

    IDispatch_AddRef(this_obj);
    return stack_push(ctx, jsval_disp(this_obj));
}

static HRESULT interp_identifier_ref(script_ctx_t *ctx, BSTR identifier, unsigned flags)
{
    exprval_t exprval;
    HRESULT hres;

    hres = identifier_eval(ctx, identifier, &exprval);
    if(FAILED(hres))
        return hres;

    if(exprval.type == EXPRVAL_INVALID && (flags & fdexNameEnsure)) {
        jsdisp_t *script_obj = ctx->global;
        DISPID id;

        if(ctx->call_ctx->bytecode->named_item)
            script_obj = ctx->call_ctx->bytecode->named_item->script_obj;

        hres = jsdisp_get_id(script_obj, identifier, fdexNameEnsure, &id);
        if(FAILED(hres))
            return hres;

        exprval_set_disp_ref(&exprval, to_disp(script_obj), id);
    }

    if(exprval.type == EXPRVAL_INVALID ||
       (exprval.type == EXPRVAL_JSVAL && ctx->version < SCRIPTLANGUAGEVERSION_ES5)) {
        WARN("invalid ref\n");
        exprval_release(&exprval);
        exprval_set_exception(&exprval, JS_E_OBJECT_EXPECTED);
    }

    return stack_push_exprval(ctx, &exprval);
}

static HRESULT identifier_value(script_ctx_t *ctx, BSTR identifier)
{
    exprval_t exprval;
    jsval_t v;
    HRESULT hres;

    hres = identifier_eval(ctx, identifier, &exprval);
    if(FAILED(hres))
        return hres;

    if(exprval.type == EXPRVAL_INVALID)
        return throw_error(ctx, exprval.u.hres, identifier);

    hres = exprval_to_value(ctx, &exprval, &v);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, v);
}

static HRESULT interp_local_ref(script_ctx_t *ctx)
{
    const int arg = get_op_int(ctx, 0);
    const unsigned flags = get_op_uint(ctx, 1);
    call_frame_t *frame = ctx->call_ctx;
    exprval_t ref;

    TRACE("%s\n", debugstr_w(local_name(frame, arg)));

    if(!frame->base_scope || !frame->base_scope->frame)
        return interp_identifier_ref(ctx, local_name(frame, arg), flags);

    ref.type = EXPRVAL_STACK_REF;
    ref.u.off = local_off(frame, arg);
    return stack_push_exprval(ctx, &ref);
}

static HRESULT interp_local(script_ctx_t *ctx)
{
    const int arg = get_op_int(ctx, 0);
    call_frame_t *frame = ctx->call_ctx;
    jsval_t copy;
    HRESULT hres;

    if(!frame->base_scope || !frame->base_scope->frame) {
        TRACE("%s\n", debugstr_w(local_name(frame, arg)));
        return identifier_value(ctx, local_name(frame, arg));
    }

    hres = jsval_copy(ctx->stack[local_off(frame, arg)], &copy);
    if(FAILED(hres))
        return hres;

    TRACE("%s: %s\n", debugstr_w(local_name(frame, arg)), debugstr_jsval(copy));
    return stack_push(ctx, copy);
}

/* ECMA-262 3rd Edition    10.1.4 */
static HRESULT interp_ident(script_ctx_t *ctx)
{
    const BSTR arg = get_op_bstr(ctx, 0);

    TRACE("%s\n", debugstr_w(arg));

    return identifier_value(ctx, arg);
}

/* ECMA-262 3rd Edition    10.1.4 */
static HRESULT interp_identid(script_ctx_t *ctx)
{
    const BSTR arg = get_op_bstr(ctx, 0);
    const unsigned flags = get_op_uint(ctx, 1);

    TRACE("%s %x\n", debugstr_w(arg), flags);

    return interp_identifier_ref(ctx, arg, flags);
}

/* ECMA-262 3rd Edition    7.8.1 */
static HRESULT interp_null(script_ctx_t *ctx)
{
    TRACE("\n");

    return stack_push(ctx, jsval_null());
}

/* ECMA-262 3rd Edition    7.8.2 */
static HRESULT interp_bool(script_ctx_t *ctx)
{
    const int arg = get_op_int(ctx, 0);

    TRACE("%s\n", arg ? "true" : "false");

    return stack_push(ctx, jsval_bool(arg));
}

/* ECMA-262 3rd Edition    7.8.3 */
static HRESULT interp_int(script_ctx_t *ctx)
{
    const int arg = get_op_int(ctx, 0);

    TRACE("%d\n", arg);

    return stack_push(ctx, jsval_number(arg));
}

/* ECMA-262 3rd Edition    7.8.3 */
static HRESULT interp_double(script_ctx_t *ctx)
{
    const double arg = get_op_double(ctx);

    TRACE("%lf\n", arg);

    return stack_push(ctx, jsval_number(arg));
}

/* ECMA-262 3rd Edition    7.8.4 */
static HRESULT interp_str(script_ctx_t *ctx)
{
    jsstr_t *str = get_op_str(ctx, 0);

    TRACE("%s\n", debugstr_jsstr(str));

    return stack_push(ctx, jsval_string(jsstr_addref(str)));
}

/* ECMA-262 3rd Edition    7.8 */
static HRESULT interp_regexp(script_ctx_t *ctx)
{
    jsstr_t *source = get_op_str(ctx, 0);
    const unsigned flags = get_op_uint(ctx, 1);
    jsdisp_t *regexp;
    HRESULT hres;

    TRACE("%s %x\n", debugstr_jsstr(source), flags);

    hres = create_regexp(ctx, source, flags, &regexp);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, jsval_obj(regexp));
}

/* ECMA-262 3rd Edition    11.1.4 */
static HRESULT interp_carray(script_ctx_t *ctx)
{
    const unsigned arg = get_op_uint(ctx, 0);
    jsdisp_t *array;
    HRESULT hres;

    TRACE("%u\n", arg);

    hres = create_array(ctx, arg, &array);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, jsval_obj(array));
}

static HRESULT interp_carray_set(script_ctx_t *ctx)
{
    const unsigned index = get_op_uint(ctx, 0);
    jsval_t value, array;
    HRESULT hres;

    value = stack_pop(ctx);

    TRACE("[%u] = %s\n", index, debugstr_jsval(value));

    array = stack_top(ctx);
    assert(is_object_instance(array));

    hres = jsdisp_propput_idx(iface_to_jsdisp(get_object(array)), index, value);
    jsval_release(value);
    return hres;
}

/* ECMA-262 3rd Edition    11.1.5 */
static HRESULT interp_new_obj(script_ctx_t *ctx)
{
    jsdisp_t *obj;
    HRESULT hres;

    TRACE("\n");

    hres = create_object(ctx, NULL, &obj);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, jsval_obj(obj));
}

/* ECMA-262 3rd Edition    11.1.5 */
static HRESULT interp_obj_prop(script_ctx_t *ctx)
{
    jsstr_t *name_arg = get_op_str(ctx, 0);
    unsigned type = get_op_uint(ctx, 1);
    const WCHAR *name;
    jsdisp_t *obj;
    jsval_t val;
    HRESULT hres;

    TRACE("%s\n", debugstr_jsstr(name_arg));

    val = stack_pop(ctx);

    /* FIXME: we should pass it as jsstr_t */
    name = jsstr_flatten(name_arg);

    assert(is_object_instance(stack_top(ctx)));
    obj = as_jsdisp(get_object(stack_top(ctx)));

    if(type == PROPERTY_DEFINITION_VALUE) {
        hres = jsdisp_propput_name(obj, name, val);
    }else {
        property_desc_t desc = {PROPF_ENUMERABLE | PROPF_CONFIGURABLE};
        jsdisp_t *func;

        assert(is_object_instance(val));
        func = iface_to_jsdisp(get_object(val));

        desc.mask = desc.flags;
        if(type == PROPERTY_DEFINITION_GETTER) {
            desc.explicit_getter = TRUE;
            desc.getter = func;
        }else {
            desc.explicit_setter = TRUE;
            desc.setter = func;
        }

        hres = jsdisp_define_property(obj, name, &desc);
        jsdisp_release(func);
    }

    jsval_release(val);
    return hres;
}

/* ECMA-262 3rd Edition    11.11 */
static HRESULT interp_cnd_nz(script_ctx_t *ctx)
{
    const unsigned arg = get_op_uint(ctx, 0);
    BOOL b;
    HRESULT hres;

    TRACE("\n");

    hres = to_boolean(stack_top(ctx), &b);
    if(FAILED(hres))
        return hres;

    if(b) {
        jmp_abs(ctx, arg);
    }else {
        stack_popn(ctx, 1);
        jmp_next(ctx);
    }
    return S_OK;
}

/* ECMA-262 3rd Edition    11.11 */
static HRESULT interp_cnd_z(script_ctx_t *ctx)
{
    const unsigned arg = get_op_uint(ctx, 0);
    BOOL b;
    HRESULT hres;

    TRACE("\n");

    hres = to_boolean(stack_top(ctx), &b);
    if(FAILED(hres))
        return hres;

    if(b) {
        stack_popn(ctx, 1);
        jmp_next(ctx);
    }else {
        jmp_abs(ctx, arg);
    }
    return S_OK;
}

/* ECMA-262 3rd Edition    11.10 */
static HRESULT interp_or(script_ctx_t *ctx)
{
    INT l, r;
    HRESULT hres;

    TRACE("\n");

    hres = stack_pop_int(ctx, &r);
    if(FAILED(hres))
        return hres;

    hres = stack_pop_int(ctx, &l);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, jsval_number(l|r));
}

/* ECMA-262 3rd Edition    11.10 */
static HRESULT interp_xor(script_ctx_t *ctx)
{
    INT l, r;
    HRESULT hres;

    TRACE("\n");

    hres = stack_pop_int(ctx, &r);
    if(FAILED(hres))
        return hres;

    hres = stack_pop_int(ctx, &l);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, jsval_number(l^r));
}

/* ECMA-262 3rd Edition    11.10 */
static HRESULT interp_and(script_ctx_t *ctx)
{
    INT l, r;
    HRESULT hres;

    TRACE("\n");

    hres = stack_pop_int(ctx, &r);
    if(FAILED(hres))
        return hres;

    hres = stack_pop_int(ctx, &l);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, jsval_number(l&r));
}

/* ECMA-262 3rd Edition    11.8.6 */
static HRESULT interp_instanceof(script_ctx_t *ctx)
{
    jsdisp_t *obj, *iter, *tmp = NULL;
    jsval_t prot, v;
    BOOL ret = FALSE;
    HRESULT hres;

    v = stack_pop(ctx);
    if(!is_object_instance(v) || !get_object(v)) {
        jsval_release(v);
        return JS_E_FUNCTION_EXPECTED;
    }

    obj = iface_to_jsdisp(get_object(v));
    IDispatch_Release(get_object(v));
    if(!obj) {
        FIXME("non-jsdisp objects not supported\n");
        return E_FAIL;
    }

    if(is_class(obj, JSCLASS_FUNCTION)) {
        hres = jsdisp_propget_name(obj, L"prototype", &prot);
    }else {
        hres = JS_E_FUNCTION_EXPECTED;
    }
    jsdisp_release(obj);
    if(FAILED(hres))
        return hres;

    v = stack_pop(ctx);

    if(is_object_instance(prot)) {
        if(is_object_instance(v))
            tmp = iface_to_jsdisp(get_object(v));
        for(iter = tmp; !ret && iter; iter = iter->prototype) {
            hres = disp_cmp(get_object(prot), to_disp(iter), &ret);
            if(FAILED(hres))
                break;
        }

        if(tmp)
            jsdisp_release(tmp);
    }else {
        FIXME("prototype is not an object\n");
        hres = E_FAIL;
    }

    jsval_release(prot);
    jsval_release(v);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, jsval_bool(ret));
}

/* ECMA-262 3rd Edition    11.8.7 */
static HRESULT interp_in(script_ctx_t *ctx)
{
    const WCHAR *str;
    jsstr_t *jsstr;
    jsval_t obj, v;
    DISPID id = 0;
    BOOL ret;
    HRESULT hres;

    TRACE("\n");

    obj = stack_pop(ctx);
    if(!is_object_instance(obj) || !get_object(obj)) {
        jsval_release(obj);
        return JS_E_OBJECT_EXPECTED;
    }

    v = stack_pop(ctx);
    hres = to_flat_string(ctx, v, &jsstr, &str);
    jsval_release(v);
    if(FAILED(hres)) {
        IDispatch_Release(get_object(obj));
        return hres;
    }

    hres = disp_get_id(ctx, get_object(obj), str, NULL, 0, &id);
    IDispatch_Release(get_object(obj));
    jsstr_release(jsstr);
    if(SUCCEEDED(hres))
        ret = TRUE;
    else if(hres == DISP_E_UNKNOWNNAME)
        ret = FALSE;
    else
        return hres;

    return stack_push(ctx, jsval_bool(ret));
}

/* ECMA-262 3rd Edition    11.6.1 */
static HRESULT interp_add(script_ctx_t *ctx)
{
    jsval_t l, r, lval, rval, ret;
    HRESULT hres;

    rval = stack_pop(ctx);
    lval = stack_pop(ctx);

    TRACE("%s + %s\n", debugstr_jsval(lval), debugstr_jsval(rval));

    hres = to_primitive(ctx, lval, &l, NO_HINT);
    if(SUCCEEDED(hres)) {
        hres = to_primitive(ctx, rval, &r, NO_HINT);
        if(FAILED(hres))
            jsval_release(l);
    }
    jsval_release(lval);
    jsval_release(rval);
    if(FAILED(hres))
        return hres;

    if(is_string(l) || is_string(r)) {
        jsstr_t *lstr, *rstr = NULL;

        hres = to_string(ctx, l, &lstr);
        if(SUCCEEDED(hres))
            hres = to_string(ctx, r, &rstr);

        if(SUCCEEDED(hres)) {
            jsstr_t *ret_str;

            ret_str = jsstr_concat(lstr, rstr);
            if(ret_str)
                ret = jsval_string(ret_str);
            else
                hres = E_OUTOFMEMORY;
        }

        jsstr_release(lstr);
        if(rstr)
            jsstr_release(rstr);
    }else {
        double nl, nr;

        hres = to_number(ctx, l, &nl);
        if(SUCCEEDED(hres)) {
            hres = to_number(ctx, r, &nr);
            if(SUCCEEDED(hres))
                ret = jsval_number(nl+nr);
        }
    }

    jsval_release(r);
    jsval_release(l);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, ret);
}

/* ECMA-262 3rd Edition    11.6.2 */
static HRESULT interp_sub(script_ctx_t *ctx)
{
    double l, r;
    HRESULT hres;

    TRACE("\n");

    hres = stack_pop_number(ctx, &r);
    if(FAILED(hres))
        return hres;

    hres = stack_pop_number(ctx, &l);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, jsval_number(l-r));
}

/* ECMA-262 3rd Edition    11.5.1 */
static HRESULT interp_mul(script_ctx_t *ctx)
{
    double l, r;
    HRESULT hres;

    TRACE("\n");

    hres = stack_pop_number(ctx, &r);
    if(FAILED(hres))
        return hres;

    hres = stack_pop_number(ctx, &l);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, jsval_number(l*r));
}

/* ECMA-262 3rd Edition    11.5.2 */
static HRESULT interp_div(script_ctx_t *ctx)
{
    double l, r;
    HRESULT hres;

    TRACE("\n");

    hres = stack_pop_number(ctx, &r);
    if(FAILED(hres))
        return hres;

    hres = stack_pop_number(ctx, &l);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, jsval_number(l/r));
}

/* ECMA-262 3rd Edition    11.5.3 */
static HRESULT interp_mod(script_ctx_t *ctx)
{
    double l, r;
    HRESULT hres;

    TRACE("\n");

    hres = stack_pop_number(ctx, &r);
    if(FAILED(hres))
        return hres;

    hres = stack_pop_number(ctx, &l);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, jsval_number(fmod(l, r)));
}

/* ECMA-262 3rd Edition    11.4.2 */
static HRESULT interp_delete(script_ctx_t *ctx)
{
    jsval_t objv, namev;
    IDispatch *obj;
    jsstr_t *name;
    BOOL ret;
    HRESULT hres;

    TRACE("\n");

    namev = stack_pop(ctx);
    objv = stack_pop(ctx);

    hres = to_object(ctx, objv, &obj);
    jsval_release(objv);
    if(FAILED(hres)) {
        jsval_release(namev);
        return hres;
    }

    hres = to_string(ctx, namev, &name);
    jsval_release(namev);
    if(FAILED(hres)) {
        IDispatch_Release(obj);
        return hres;
    }

    hres = disp_delete_name(ctx, obj, name, &ret);
    IDispatch_Release(obj);
    jsstr_release(name);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, jsval_bool(ret));
}

/* ECMA-262 3rd Edition    11.4.2 */
static HRESULT interp_delete_ident(script_ctx_t *ctx)
{
    const BSTR arg = get_op_bstr(ctx, 0);
    exprval_t exprval;
    BOOL ret;
    HRESULT hres;

    TRACE("%s\n", debugstr_w(arg));

    hres = identifier_eval(ctx, arg, &exprval);
    if(FAILED(hres))
        return hres;

    switch(exprval.type) {
    case EXPRVAL_STACK_REF:
        ret = FALSE;
        break;
    case EXPRVAL_IDREF:
        hres = disp_delete(exprval.u.idref.disp, exprval.u.idref.id, &ret);
        IDispatch_Release(exprval.u.idref.disp);
        if(FAILED(hres))
            return hres;
        break;
    case EXPRVAL_INVALID:
        ret = TRUE;
        break;
    default:
        FIXME("Unsupported exprval\n");
        exprval_release(&exprval);
        return E_NOTIMPL;
    }


    return stack_push(ctx, jsval_bool(ret));
}

/* ECMA-262 3rd Edition    11.4.2 */
static HRESULT interp_void(script_ctx_t *ctx)
{
    TRACE("\n");

    stack_popn(ctx, 1);
    return stack_push(ctx, jsval_undefined());
}

/* ECMA-262 3rd Edition    11.4.3 */
static HRESULT typeof_string(jsval_t v, const WCHAR **ret)
{
    switch(jsval_type(v)) {
    case JSV_UNDEFINED:
        *ret = L"undefined";
        break;
    case JSV_NULL:
        *ret = L"object";
        break;
    case JSV_OBJECT: {
        jsdisp_t *dispex;

        if(get_object(v) && (dispex = iface_to_jsdisp(get_object(v)))) {
            *ret = is_class(dispex, JSCLASS_FUNCTION) ? L"function" : L"object";
            jsdisp_release(dispex);
        }else {
            *ret = L"object";
        }
        break;
    }
    case JSV_STRING:
        *ret = L"string";
        break;
    case JSV_NUMBER:
        *ret = L"number";
        break;
    case JSV_BOOL:
        *ret = L"boolean";
        break;
    case JSV_VARIANT:
        FIXME("unhandled variant %s\n", debugstr_variant(get_variant(v)));
        return E_NOTIMPL;
    }

    return S_OK;
}

/* ECMA-262 3rd Edition    11.4.3 */
static HRESULT interp_typeofid(script_ctx_t *ctx)
{
    const WCHAR *ret;
    exprval_t ref;
    jsval_t v;
    HRESULT hres;

    TRACE("\n");

    if(!stack_pop_exprval(ctx, &ref))
        return stack_push(ctx, jsval_string(jsstr_undefined()));

    hres = exprval_propget(ctx, &ref, &v);
    exprval_release(&ref);
    if(FAILED(hres))
        return stack_push_string(ctx, L"unknown");

    hres = typeof_string(v, &ret);
    jsval_release(v);
    if(FAILED(hres))
        return hres;

    return stack_push_string(ctx, ret);
}

/* ECMA-262 3rd Edition    11.4.3 */
static HRESULT interp_typeofident(script_ctx_t *ctx)
{
    const BSTR arg = get_op_bstr(ctx, 0);
    exprval_t exprval;
    const WCHAR *ret;
    jsval_t v;
    HRESULT hres;

    TRACE("%s\n", debugstr_w(arg));

    hres = identifier_eval(ctx, arg, &exprval);
    if(FAILED(hres))
        return hres;

    if(exprval.type == EXPRVAL_INVALID)
        return stack_push(ctx, jsval_string(jsstr_undefined()));

    hres = exprval_to_value(ctx, &exprval, &v);
    if(FAILED(hres))
        return hres;

    hres = typeof_string(v, &ret);
    jsval_release(v);
    if(FAILED(hres))
        return hres;

    return stack_push_string(ctx, ret);
}

/* ECMA-262 3rd Edition    11.4.3 */
static HRESULT interp_typeof(script_ctx_t *ctx)
{
    const WCHAR *ret;
    jsval_t v;
    HRESULT hres;

    TRACE("\n");

    v = stack_pop(ctx);
    hres = typeof_string(v, &ret);
    jsval_release(v);
    if(FAILED(hres))
        return hres;

    return stack_push_string(ctx, ret);
}

/* ECMA-262 3rd Edition    11.4.7 */
static HRESULT interp_minus(script_ctx_t *ctx)
{
    double n;
    HRESULT hres;

    TRACE("\n");

    hres = stack_pop_number(ctx, &n);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, jsval_number(-n));
}

/* ECMA-262 3rd Edition    11.4.6 */
static HRESULT interp_tonum(script_ctx_t *ctx)
{
    jsval_t v;
    double n;
    HRESULT hres;

    TRACE("\n");

    v = stack_pop(ctx);
    hres = to_number(ctx, v, &n);
    jsval_release(v);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, jsval_number(n));
}

/* ECMA-262 3rd Edition    11.3.1 */
static HRESULT interp_postinc(script_ctx_t *ctx)
{
    const int arg = get_op_int(ctx, 0);
    exprval_t ref;
    jsval_t v;
    HRESULT hres;

    TRACE("%d\n", arg);

    if(!stack_pop_exprval(ctx, &ref))
        return JS_E_OBJECT_EXPECTED;

    hres = exprval_propget(ctx, &ref, &v);
    if(SUCCEEDED(hres)) {
        double n;

        hres = to_number(ctx, v, &n);
        if(SUCCEEDED(hres))
            hres = exprval_propput(ctx, &ref, jsval_number(n+(double)arg));
        if(FAILED(hres))
            jsval_release(v);
    }
    exprval_release(&ref);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, v);
}

/* ECMA-262 3rd Edition    11.4.4, 11.4.5 */
static HRESULT interp_preinc(script_ctx_t *ctx)
{
    const int arg = get_op_int(ctx, 0);
    exprval_t ref;
    double ret;
    jsval_t v;
    HRESULT hres;

    TRACE("%d\n", arg);

    if(!stack_pop_exprval(ctx, &ref))
        return JS_E_OBJECT_EXPECTED;

    hres = exprval_propget(ctx, &ref, &v);
    if(SUCCEEDED(hres)) {
        double n;

        hres = to_number(ctx, v, &n);
        jsval_release(v);
        if(SUCCEEDED(hres)) {
            ret = n+(double)arg;
            hres = exprval_propput(ctx, &ref, jsval_number(ret));
        }
    }
    exprval_release(&ref);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, jsval_number(ret));
}

/* ECMA-262 3rd Edition    11.9.3 */
static HRESULT equal_values(script_ctx_t *ctx, jsval_t lval, jsval_t rval, BOOL *ret)
{
    if(jsval_type(lval) == jsval_type(rval) || (is_number(lval) && is_number(rval)))
       return jsval_strict_equal(lval, rval, ret);

    /* FIXME: NULL disps should be handled in more general way */
    if(is_object_instance(lval) && !get_object(lval))
        return equal_values(ctx, jsval_null(), rval, ret);
    if(is_object_instance(rval) && !get_object(rval))
        return equal_values(ctx, lval, jsval_null(), ret);

    if((is_null(lval) && is_undefined(rval)) || (is_undefined(lval) && is_null(rval))) {
        *ret = TRUE;
        return S_OK;
    }

    if(is_string(lval) && is_number(rval)) {
        double n;
        HRESULT hres;

        hres = to_number(ctx, lval, &n);
        if(FAILED(hres))
            return hres;

        /* FIXME: optimize */
        return equal_values(ctx, jsval_number(n), rval, ret);
    }

    if(is_string(rval) && is_number(lval)) {
        double n;
        HRESULT hres;

        hres = to_number(ctx, rval, &n);
        if(FAILED(hres))
            return hres;

        /* FIXME: optimize */
        return equal_values(ctx, lval, jsval_number(n), ret);
    }

    if(is_bool(rval))
        return equal_values(ctx, lval, jsval_number(get_bool(rval) ? 1 : 0), ret);

    if(is_bool(lval))
        return equal_values(ctx, jsval_number(get_bool(lval) ? 1 : 0), rval, ret);


    if(is_object_instance(rval) && (is_string(lval) || is_number(lval))) {
        jsval_t prim;
        HRESULT hres;

        hres = to_primitive(ctx, rval, &prim, NO_HINT);
        if(FAILED(hres))
            return hres;

        hres = equal_values(ctx, lval, prim, ret);
        jsval_release(prim);
        return hres;
    }


    if(is_object_instance(lval) && (is_string(rval) || is_number(rval))) {
        jsval_t prim;
        HRESULT hres;

        hres = to_primitive(ctx, lval, &prim, NO_HINT);
        if(FAILED(hres))
            return hres;

        hres = equal_values(ctx, prim, rval, ret);
        jsval_release(prim);
        return hres;
    }


    *ret = FALSE;
    return S_OK;
}

/* ECMA-262 3rd Edition    11.9.1 */
static HRESULT interp_eq(script_ctx_t *ctx)
{
    jsval_t l, r;
    BOOL b;
    HRESULT hres;

    r = stack_pop(ctx);
    l = stack_pop(ctx);

    TRACE("%s == %s\n", debugstr_jsval(l), debugstr_jsval(r));

    hres = equal_values(ctx, l, r, &b);
    jsval_release(l);
    jsval_release(r);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, jsval_bool(b));
}

/* ECMA-262 3rd Edition    11.9.2 */
static HRESULT interp_neq(script_ctx_t *ctx)
{
    jsval_t l, r;
    BOOL b;
    HRESULT hres;

    r = stack_pop(ctx);
    l = stack_pop(ctx);

    TRACE("%s != %s\n", debugstr_jsval(l), debugstr_jsval(r));

    hres = equal_values(ctx, l, r, &b);
    jsval_release(l);
    jsval_release(r);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, jsval_bool(!b));
}

/* ECMA-262 3rd Edition    11.9.4 */
static HRESULT interp_eq2(script_ctx_t *ctx)
{
    jsval_t l, r;
    BOOL b;
    HRESULT hres;

    r = stack_pop(ctx);
    l = stack_pop(ctx);

    TRACE("%s === %s\n", debugstr_jsval(l), debugstr_jsval(r));

    hres = jsval_strict_equal(r, l, &b);
    jsval_release(l);
    jsval_release(r);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, jsval_bool(b));
}

/* ECMA-262 3rd Edition    11.9.5 */
static HRESULT interp_neq2(script_ctx_t *ctx)
{
    jsval_t l, r;
    BOOL b;
    HRESULT hres;

    TRACE("\n");

    r = stack_pop(ctx);
    l = stack_pop(ctx);

    hres = jsval_strict_equal(r, l, &b);
    jsval_release(l);
    jsval_release(r);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, jsval_bool(!b));
}

/* ECMA-262 3rd Edition    11.8.5 */
static HRESULT less_eval(script_ctx_t *ctx, jsval_t lval, jsval_t rval, BOOL greater, BOOL *ret)
{
    double ln, rn;
    jsval_t l, r;
    HRESULT hres;

    hres = to_primitive(ctx, lval, &l, NO_HINT);
    if(FAILED(hres))
        return hres;

    hres = to_primitive(ctx, rval, &r, NO_HINT);
    if(FAILED(hres)) {
        jsval_release(l);
        return hres;
    }

    if(is_string(l) && is_string(r)) {
        *ret = (jsstr_cmp(get_string(l), get_string(r)) < 0) ^ greater;
        jsstr_release(get_string(l));
        jsstr_release(get_string(r));
        return S_OK;
    }

    hres = to_number(ctx, l, &ln);
    jsval_release(l);
    if(SUCCEEDED(hres))
        hres = to_number(ctx, r, &rn);
    jsval_release(r);
    if(FAILED(hres))
        return hres;

    *ret = !isnan(ln) && !isnan(rn) && ((ln < rn) ^ greater);
    return S_OK;
}

/* ECMA-262 3rd Edition    11.8.1 */
static HRESULT interp_lt(script_ctx_t *ctx)
{
    jsval_t l, r;
    BOOL b;
    HRESULT hres;

    r = stack_pop(ctx);
    l = stack_pop(ctx);

    TRACE("%s < %s\n", debugstr_jsval(l), debugstr_jsval(r));

    hres = less_eval(ctx, l, r, FALSE, &b);
    jsval_release(l);
    jsval_release(r);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, jsval_bool(b));
}

/* ECMA-262 3rd Edition    11.8.1 */
static HRESULT interp_lteq(script_ctx_t *ctx)
{
    jsval_t l, r;
    BOOL b;
    HRESULT hres;

    r = stack_pop(ctx);
    l = stack_pop(ctx);

    TRACE("%s <= %s\n", debugstr_jsval(l), debugstr_jsval(r));

    hres = less_eval(ctx, r, l, TRUE, &b);
    jsval_release(l);
    jsval_release(r);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, jsval_bool(b));
}

/* ECMA-262 3rd Edition    11.8.2 */
static HRESULT interp_gt(script_ctx_t *ctx)
{
    jsval_t l, r;
    BOOL b;
    HRESULT hres;

    r = stack_pop(ctx);
    l = stack_pop(ctx);

    TRACE("%s > %s\n", debugstr_jsval(l), debugstr_jsval(r));

    hres = less_eval(ctx, r, l, FALSE, &b);
    jsval_release(l);
    jsval_release(r);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, jsval_bool(b));
}

/* ECMA-262 3rd Edition    11.8.4 */
static HRESULT interp_gteq(script_ctx_t *ctx)
{
    jsval_t l, r;
    BOOL b;
    HRESULT hres;

    r = stack_pop(ctx);
    l = stack_pop(ctx);

    TRACE("%s >= %s\n", debugstr_jsval(l), debugstr_jsval(r));

    hres = less_eval(ctx, l, r, TRUE, &b);
    jsval_release(l);
    jsval_release(r);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, jsval_bool(b));
}

/* ECMA-262 3rd Edition    11.4.8 */
static HRESULT interp_bneg(script_ctx_t *ctx)
{
    jsval_t v;
    INT i;
    HRESULT hres;

    TRACE("\n");

    v = stack_pop(ctx);
    hres = to_int32(ctx, v, &i);
    jsval_release(v);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, jsval_number(~i));
}

/* ECMA-262 3rd Edition    11.4.9 */
static HRESULT interp_neg(script_ctx_t *ctx)
{
    jsval_t v;
    BOOL b;
    HRESULT hres;

    TRACE("\n");

    v = stack_pop(ctx);
    hres = to_boolean(v, &b);
    jsval_release(v);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, jsval_bool(!b));
}

/* ECMA-262 3rd Edition    11.7.1 */
static HRESULT interp_lshift(script_ctx_t *ctx)
{
    DWORD r;
    INT l;
    HRESULT hres;

    hres = stack_pop_uint(ctx, &r);
    if(FAILED(hres))
        return hres;

    hres = stack_pop_int(ctx, &l);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, jsval_number(l << (r&0x1f)));
}

/* ECMA-262 3rd Edition    11.7.2 */
static HRESULT interp_rshift(script_ctx_t *ctx)
{
    DWORD r;
    INT l;
    HRESULT hres;

    hres = stack_pop_uint(ctx, &r);
    if(FAILED(hres))
        return hres;

    hres = stack_pop_int(ctx, &l);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, jsval_number(l >> (r&0x1f)));
}

/* ECMA-262 3rd Edition    11.7.3 */
static HRESULT interp_rshift2(script_ctx_t *ctx)
{
    DWORD r, l;
    HRESULT hres;

    hres = stack_pop_uint(ctx, &r);
    if(FAILED(hres))
        return hres;

    hres = stack_pop_uint(ctx, &l);
    if(FAILED(hres))
        return hres;

    return stack_push(ctx, jsval_number(l >> (r&0x1f)));
}

/* ECMA-262 3rd Edition    9.8 */
static HRESULT interp_to_string(script_ctx_t *ctx)
{
    jsstr_t *str;
    jsval_t v;
    HRESULT hres;

    v = stack_pop(ctx);
    TRACE("%s\n", debugstr_jsval(v));
    hres = to_string(ctx, v, &str);
    jsval_release(v);
    if(FAILED(hres)) {
        WARN("failed %08x\n", hres);
        return hres;
    }

    return stack_push(ctx, jsval_string(str));
}

/* ECMA-262 3rd Edition    11.13.1 */
static HRESULT interp_assign(script_ctx_t *ctx)
{
    exprval_t ref;
    jsval_t v;
    HRESULT hres;

    TRACE("\n");

    v = stack_pop(ctx);

    if(!stack_pop_exprval(ctx, &ref)) {
        jsval_release(v);
        return JS_E_ILLEGAL_ASSIGN;
    }

    hres = exprval_propput(ctx, &ref, v);
    exprval_release(&ref);
    if(FAILED(hres)) {
        jsval_release(v);
        return hres;
    }

    return stack_push(ctx, v);
}

/* ECMA-262 3rd Edition    11.13.1 */
static HRESULT interp_set_member(script_ctx_t *ctx)
{
    jsval_t objv, namev, value;
    const WCHAR *name;
    IDispatch *obj;
    HRESULT hres;

    value = stack_pop(ctx);
    namev = stack_pop(ctx);
    assert(is_string(namev));
    objv = stack_pop(ctx);

    TRACE("%s.%s = %s\n", debugstr_jsval(objv), debugstr_jsval(namev), debugstr_jsval(value));

    hres = to_object(ctx, objv, &obj);
    jsval_release(objv);
    if(SUCCEEDED(hres) && !(name = jsstr_flatten(get_string(namev)))) {
        IDispatch_Release(obj);
        hres = E_OUTOFMEMORY;
    }
    if(SUCCEEDED(hres)) {
        hres = disp_propput_name(ctx, obj, name, value);
        IDispatch_Release(obj);
        jsstr_release(get_string(namev));
    }
    if(FAILED(hres)) {
        WARN("failed %08x\n", hres);
        jsval_release(value);
        return hres;
    }

    return stack_push(ctx, value);
}

/* JScript extension */
static HRESULT interp_assign_call(script_ctx_t *ctx)
{
    const unsigned argc = get_op_uint(ctx, 0);
    exprval_t ref;
    jsval_t v;
    HRESULT hres;

    TRACE("%u\n", argc);

    if(!stack_topn_exprval(ctx, argc+1, &ref))
        return JS_E_ILLEGAL_ASSIGN;

    hres = exprval_call(ctx, &ref, DISPATCH_PROPERTYPUT, argc+1, stack_args(ctx, argc+1), NULL);
    if(FAILED(hres))
        return hres;

    v = stack_pop(ctx);
    stack_popn(ctx, argc+2);
    return stack_push(ctx, v);
}

static HRESULT interp_undefined(script_ctx_t *ctx)
{
    TRACE("\n");

    return stack_push(ctx, jsval_undefined());
}

static HRESULT interp_jmp(script_ctx_t *ctx)
{
    const unsigned arg = get_op_uint(ctx, 0);

    TRACE("%u\n", arg);

    jmp_abs(ctx, arg);
    return S_OK;
}

static HRESULT interp_jmp_z(script_ctx_t *ctx)
{
    const unsigned arg = get_op_uint(ctx, 0);
    BOOL b;
    jsval_t v;
    HRESULT hres;

    TRACE("\n");

    v = stack_pop(ctx);
    hres = to_boolean(v, &b);
    jsval_release(v);
    if(FAILED(hres))
        return hres;

    if(b)
        jmp_next(ctx);
    else
        jmp_abs(ctx, arg);
    return S_OK;
}

static HRESULT interp_pop(script_ctx_t *ctx)
{
    const unsigned arg = get_op_uint(ctx, 0);

    TRACE("%u\n", arg);

    stack_popn(ctx, arg);
    return S_OK;
}

static HRESULT interp_ret(script_ctx_t *ctx)
{
    const unsigned clear_ret = get_op_uint(ctx, 0);
    call_frame_t *frame = ctx->call_ctx;

    TRACE("\n");

    if(clear_ret)
        jsval_release(steal_ret(frame));

    if((frame->flags & EXEC_CONSTRUCTOR) && !is_object_instance(frame->ret)) {
        jsval_release(frame->ret);
        IDispatch_AddRef(frame->this_obj);
        frame->ret = jsval_disp(frame->this_obj);
    }

    jmp_abs(ctx, -1);
    return S_OK;
}

static HRESULT interp_setret(script_ctx_t *ctx)
{
    call_frame_t *frame = ctx->call_ctx;

    TRACE("\n");

    jsval_release(frame->ret);
    frame->ret = stack_pop(ctx);
    return S_OK;
}

static HRESULT interp_push_acc(script_ctx_t *ctx)
{
    HRESULT hres;

    TRACE("\n");

    hres = stack_push(ctx, ctx->acc);
    if(SUCCEEDED(hres))
        ctx->acc = jsval_undefined();
    return hres;
}

typedef HRESULT (*op_func_t)(script_ctx_t*);

static const op_func_t op_funcs[] = {
#define X(x,a,b,c) interp_##x,
OP_LIST
#undef X
};

static const unsigned op_move[] = {
#define X(a,x,b,c) x,
OP_LIST
#undef X
};

static void pop_call_frame(script_ctx_t *ctx)
{
    call_frame_t *frame = ctx->call_ctx;

    frame->stack_base -= frame->pop_locals + frame->pop_variables;

    assert(frame->scope == frame->base_scope);

    /* If current scope will be kept alive, we need to transfer local variables to its variable object. */
    if(frame->scope && frame->scope->ref > 1) {
        HRESULT hres = detach_variable_object(ctx, frame, TRUE);
        if(FAILED(hres))
            ERR("Failed to detach variable object: %08x\n", hres);
    }

    if(frame->arguments_obj)
        detach_arguments_object(frame->arguments_obj);
    if(frame->scope)
        scope_release(frame->scope);

    if(frame->pop_variables)
        stack_popn(ctx, frame->pop_variables);
    stack_popn(ctx, frame->pop_locals);

    ctx->call_ctx = frame->prev_frame;

    if(frame->function_instance)
        jsdisp_release(frame->function_instance);
    if(frame->variable_obj)
        jsdisp_release(frame->variable_obj);
    if(frame->this_obj)
        IDispatch_Release(frame->this_obj);
    jsval_release(frame->ret);
    release_bytecode(frame->bytecode);
    heap_free(frame);
}

static void print_backtrace(script_ctx_t *ctx)
{
    unsigned depth = 0, i, line, char_pos;
    call_frame_t *frame;

    for(frame = ctx->call_ctx; frame; frame = frame->prev_frame) {
        WARN("%u\t", depth);
        depth++;

        if(frame->this_obj)
            WARN("%p->", frame->this_obj);
        WARN("%s(", frame->function->name ? debugstr_w(frame->function->name) : "[unnamed]");
        if(frame->base_scope && frame->base_scope->frame) {
            for(i=0; i < frame->argc; i++) {
                if(i < frame->function->param_cnt)
                    WARN("%s%s=%s", i ? ", " : "", debugstr_w(frame->function->params[i]),
                         debugstr_jsval(ctx->stack[local_off(frame, -i-1)]));
                else
                    WARN("%s%s", i ? ", " : "", debugstr_jsval(ctx->stack[local_off(frame, -i-1)]));
            }
        }else {
            WARN("[detached frame]");
        }
        line = get_location_line(frame->bytecode, frame->bytecode->instrs[frame->ip].loc, &char_pos);
        WARN(") context %s line %u char %u\n", wine_dbgstr_longlong(frame->bytecode->source_context), line, char_pos);

        if(!(frame->flags & EXEC_RETURN_TO_INTERP)) {
            WARN("%u\t[native code]\n", depth);
            depth++;
        }
    }
}

static HRESULT unwind_exception(script_ctx_t *ctx, HRESULT exception_hres)
{
    except_frame_t *except_frame;
    jsexcept_t *ei = ctx->ei;
    call_frame_t *frame;
    jsval_t except_val;
    unsigned catch_off;
    HRESULT hres;

    if(WARN_ON(jscript)) {
        jsdisp_t *error_obj;
        jsval_t msg;

        WARN("Exception %08x %s", exception_hres, debugstr_jsval(ei->valid_value ? ei->value : jsval_undefined()));
        if(ei->valid_value && jsval_type(ei->value) == JSV_OBJECT) {
            error_obj = to_jsdisp(get_object(ei->value));
            if(error_obj) {
                hres = jsdisp_propget_name(error_obj, L"message", &msg);
                if(SUCCEEDED(hres)) {
                    WARN(" (message %s)", debugstr_jsval(msg));
                    jsval_release(msg);
                }
            }
        }
        WARN(" in:\n");

        print_backtrace(ctx);
    }

    frame = ctx->call_ctx;
    if(exception_hres != DISP_E_EXCEPTION)
        throw_error(ctx, exception_hres, NULL);
    set_error_location(ei, frame->bytecode, frame->bytecode->instrs[frame->ip].loc, IDS_RUNTIME_ERROR, NULL);

    while(!frame->except_frame) {
        DWORD flags;

        while(frame->scope != frame->base_scope)
            scope_pop(&frame->scope);

        stack_popn(ctx, ctx->stack_top-frame->stack_base);

        flags = frame->flags;
        pop_call_frame(ctx);
        if(!(flags & EXEC_RETURN_TO_INTERP))
            return DISP_E_EXCEPTION;
        frame = ctx->call_ctx;
    }

    except_frame = frame->except_frame;
    catch_off = except_frame->catch_off;

    assert(except_frame->stack_top <= ctx->stack_top);
    stack_popn(ctx, ctx->stack_top - except_frame->stack_top);

    while(except_frame->scope != frame->scope)
        scope_pop(&frame->scope);

    frame->ip = catch_off ? catch_off : except_frame->finally_off;
    assert(!catch_off || frame->bytecode->instrs[frame->ip].op == OP_enter_catch);

    if(ei->valid_value) {
        except_val = ctx->ei->value;
        ei->valid_value = FALSE;
    }else {
        jsdisp_t *err;
        if(!(err = create_builtin_error(ctx)))
            return E_OUTOFMEMORY;
        except_val = jsval_obj(err);
    }

    /* keep current except_frame if we're entering catch block with finally block associated */
    if(catch_off && except_frame->finally_off) {
        except_frame->catch_off = 0;
    }else {
        frame->except_frame = except_frame->next;
        heap_free(except_frame);
    }

    hres = stack_push(ctx, except_val);
    if(FAILED(hres))
        return hres;

    if(!catch_off)
        hres = stack_push(ctx, jsval_bool(FALSE));
    return hres;
}

static HRESULT enter_bytecode(script_ctx_t *ctx, jsval_t *r)
{
    call_frame_t *frame;
    jsop_t op;
    HRESULT hres = S_OK;

    TRACE("\n");

    while(1) {
        frame = ctx->call_ctx;
        op = frame->bytecode->instrs[frame->ip].op;
        hres = op_funcs[op](ctx);
        if(FAILED(hres)) {
            hres = unwind_exception(ctx, hres);
            if(FAILED(hres))
                return hres;
        }else if(frame->ip == -1) {
            const DWORD return_to_interp = frame->flags & EXEC_RETURN_TO_INTERP;

            assert(ctx->stack_top == frame->stack_base);
            assert(frame->scope == frame->base_scope);

            if(return_to_interp) {
                jsval_release(ctx->acc);
                ctx->acc = steal_ret(frame);
            }else if(r) {
                *r = steal_ret(frame);
            }
            pop_call_frame(ctx);
            if(!return_to_interp)
                break;
        }else {
            frame->ip += op_move[op];
        }
    }

    return S_OK;
}

static HRESULT bind_event_target(script_ctx_t *ctx, function_code_t *func, jsdisp_t *func_obj)
{
    IBindEventHandler *target;
    exprval_t exprval;
    IDispatch *disp;
    jsval_t v;
    HRESULT hres;

    hres = identifier_eval(ctx, func->event_target, &exprval);
    if(FAILED(hres))
        return hres;

    hres = exprval_to_value(ctx, &exprval, &v);
    if(FAILED(hres))
        return hres;

    if(!is_object_instance(v)) {
        FIXME("Can't bind to %s\n", debugstr_jsval(v));
        jsval_release(v);
    }

    disp = get_object(v);
    hres = IDispatch_QueryInterface(disp, &IID_IBindEventHandler, (void**)&target);
    if(SUCCEEDED(hres)) {
        hres = IBindEventHandler_BindHandler(target, func->name, (IDispatch*)&func_obj->IDispatchEx_iface);
        IBindEventHandler_Release(target);
        if(FAILED(hres))
            WARN("BindEvent failed: %08x\n", hres);
    }else {
        FIXME("No IBindEventHandler, not yet supported binding\n");
    }

    IDispatch_Release(disp);
    return hres;
}

static HRESULT setup_scope(script_ctx_t *ctx, call_frame_t *frame, scope_chain_t *scope_chain, jsdisp_t *variable_object, unsigned argc, jsval_t *argv)
{
    const unsigned orig_stack = ctx->stack_top;
    scope_chain_t *scope;
    unsigned i;
    jsval_t v;
    HRESULT hres;

    /* If arguments are already on the stack, we may use them. */
    if(argv + argc == ctx->stack + ctx->stack_top) {
        frame->arguments_off = argv - ctx->stack;
        i = argc;
    }else {
        frame->arguments_off = ctx->stack_top;
        for(i = 0; i < argc; i++) {
            hres = jsval_copy(argv[i], &v);
            if(SUCCEEDED(hres))
                hres = stack_push(ctx, v);
            if(FAILED(hres)) {
                stack_popn(ctx, i);
                return hres;
            }
        }
    }

    /* If fewer than declared arguments were passed, fill remaining with undefined value. */
    for(; i < frame->function->param_cnt; i++) {
        hres = stack_push(ctx, jsval_undefined());
        if(FAILED(hres)) {
            stack_popn(ctx, ctx->stack_top - orig_stack);
            return hres;
        }
    }

    frame->pop_locals = ctx->stack_top - orig_stack;

    frame->variables_off = ctx->stack_top;

    for(i = 0; i < frame->function->var_cnt; i++) {
        hres = stack_push(ctx, jsval_undefined());
        if(FAILED(hres)) {
            stack_popn(ctx, ctx->stack_top - orig_stack);
            return hres;
        }
    }

    frame->pop_variables = i;

    hres = scope_push(scope_chain, variable_object, to_disp(variable_object), &scope);
    if(FAILED(hres)) {
        stack_popn(ctx, ctx->stack_top - orig_stack);
        return hres;
    }

    for(i = 0; i < frame->function->func_cnt; i++) {
        if(frame->function->funcs[i].local_ref != INVALID_LOCAL_REF
                && !frame->function->funcs[i].scope_index)
        {
            jsdisp_t *func_obj;
            unsigned off;

            hres = create_source_function(ctx, frame->bytecode, frame->function->funcs+i, scope, &func_obj);
            if(FAILED(hres)) {
                stack_popn(ctx, ctx->stack_top - orig_stack);
                scope_release(scope);
                return hres;
            }

            off = local_off(frame, frame->function->funcs[i].local_ref);
            jsval_release(ctx->stack[off]);
            ctx->stack[off] = jsval_obj(func_obj);
        }
    }

    scope->frame = frame;
    frame->base_scope = frame->scope = scope;
    return S_OK;
}

HRESULT exec_source(script_ctx_t *ctx, DWORD flags, bytecode_t *bytecode, function_code_t *function, scope_chain_t *scope,
        IDispatch *this_obj, jsdisp_t *function_instance, unsigned argc, jsval_t *argv, jsval_t *r)
{
    jsdisp_t *variable_obj;
    call_frame_t *frame;
    unsigned i;
    HRESULT hres;

    if(!ctx->stack) {
        ctx->stack = heap_alloc(stack_size * sizeof(*ctx->stack));
        if(!ctx->stack)
            return E_OUTOFMEMORY;
    }

    if(bytecode->named_item) {
        if(!bytecode->named_item->script_obj) {
            hres = create_named_item_script_obj(ctx, bytecode->named_item);
            if(FAILED(hres)) return hres;
        }
    }

    if(!ctx->ei->enter_notified) {
        ctx->ei->enter_notified = TRUE;
        IActiveScriptSite_OnEnterScript(ctx->site);
    }

    for(i = 0; i < function->func_cnt; i++) {
        jsdisp_t *func_obj;

        if(!function->funcs[i].event_target)
            continue;

        if (function->funcs[i].scope_index)
        {
            /* TODO: Add tests and handle in interp_push_scope(). */
            FIXME("Event target with scope index are not properly handled.\n");
        }

        hres = create_source_function(ctx, bytecode, function->funcs+i, scope, &func_obj);
        if(FAILED(hres))
            return hres;

        hres = bind_event_target(ctx, function->funcs+i, func_obj);
        jsdisp_release(func_obj);
        if(FAILED(hres))
            return hres;
    }

    if((flags & EXEC_EVAL) && ctx->call_ctx) {
        variable_obj = jsdisp_addref(ctx->call_ctx->variable_obj);
    }else if(!(flags & (EXEC_GLOBAL | EXEC_EVAL))) {
        hres = create_dispex(ctx, NULL, NULL, &variable_obj);
        if(FAILED(hres)) return hres;
    }else if(bytecode->named_item) {
        variable_obj = jsdisp_addref(bytecode->named_item->script_obj);
    }else {
        variable_obj = jsdisp_addref(ctx->global);
    }

    if(flags & (EXEC_GLOBAL | EXEC_EVAL)) {
        named_item_t *item = bytecode->named_item;
        DISPID id;

        for(i=0; i < function->var_cnt; i++) {
            TRACE("[%d] %s %d\n", i, debugstr_w(function->variables[i].name), function->variables[i].func_id);
            if(function->variables[i].func_id != -1) {
                jsdisp_t *func_obj;

                if (function->funcs[function->variables[i].func_id].scope_index && flags & EXEC_EVAL)
                {
                    /* TODO: Add tests and handle in interp_push_scope(). */
                    FIXME("Functions with scope index inside eval() are not properly handled.\n");
                }

                hres = create_source_function(ctx, bytecode, function->funcs+function->variables[i].func_id, scope, &func_obj);
                if(FAILED(hres))
                    goto fail;

                hres = jsdisp_propput_name(variable_obj, function->variables[i].name, jsval_obj(func_obj));
                jsdisp_release(func_obj);
                continue;
            }

            if(item && !(item->flags & SCRIPTITEM_CODEONLY)
                && SUCCEEDED(disp_get_id(ctx, item->disp, function->variables[i].name, function->variables[i].name, 0, &id)))
                    continue;

            if(!item && (flags & EXEC_GLOBAL) && lookup_global_members(ctx, function->variables[i].name, NULL))
                continue;

            hres = jsdisp_get_id(variable_obj, function->variables[i].name, fdexNameEnsure, &id);
            if(FAILED(hres))
                goto fail;
        }
    }

    /* ECMA-262 3rd Edition    11.2.3.7 */
    if(this_obj) {
        jsdisp_t *jsthis;

        jsthis = iface_to_jsdisp(this_obj);
        if(jsthis) {
            if(jsthis->builtin_info->class == JSCLASS_GLOBAL || jsthis->builtin_info->class == JSCLASS_NONE)
                this_obj = NULL;
            jsdisp_release(jsthis);
        }
    }

    if(ctx->call_ctx && (flags & EXEC_EVAL)) {
        hres = detach_variable_object(ctx, ctx->call_ctx, FALSE);
        if(FAILED(hres))
            goto fail;
    }

    frame = heap_alloc_zero(sizeof(*frame));
    if(!frame) {
        hres = E_OUTOFMEMORY;
        goto fail;
    }

    frame->function = function;
    frame->ret = jsval_undefined();
    frame->argc = argc;
    frame->bytecode = bytecode_addref(bytecode);

    if(!(flags & (EXEC_GLOBAL|EXEC_EVAL))) {
        hres = setup_scope(ctx, frame, scope, variable_obj, argc, argv);
        if(FAILED(hres)) {
            release_bytecode(frame->bytecode);
            heap_free(frame);
            goto fail;
        }
    }else if(scope) {
        frame->base_scope = frame->scope = scope_addref(scope);
    }

    frame->ip = function->instr_off;
    frame->stack_base = ctx->stack_top;
    if(this_obj) {
        frame->this_obj = this_obj;
        IDispatch_AddRef(frame->this_obj);
    }

    if(function_instance)
        frame->function_instance = jsdisp_addref(function_instance);

    frame->flags = flags;
    frame->variable_obj = variable_obj;

    frame->prev_frame = ctx->call_ctx;
    ctx->call_ctx = frame;

    if(flags & EXEC_RETURN_TO_INTERP) {
        /*
         * We're called directly from interpreter, so we may just setup call frame and return.
         * Already running interpreter will take care of execution.
         */
        if(r)
            *r = jsval_undefined();
        return S_OK;
    }

    return enter_bytecode(ctx, r);

fail:
    jsdisp_release(variable_obj);
    return hres;
}
