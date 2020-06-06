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


#include <math.h>
#include <assert.h>

#include "jscript.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(jscript);

typedef struct {
    jsdisp_t dispex;

    DWORD length;
} ArrayInstance;

static inline ArrayInstance *array_from_jsdisp(jsdisp_t *jsdisp)
{
    return CONTAINING_RECORD(jsdisp, ArrayInstance, dispex);
}

static inline ArrayInstance *array_from_vdisp(vdisp_t *vdisp)
{
    return array_from_jsdisp(vdisp->u.jsdisp);
}

static inline ArrayInstance *array_this(vdisp_t *jsthis)
{
    return is_vclass(jsthis, JSCLASS_ARRAY) ? array_from_vdisp(jsthis) : NULL;
}

unsigned array_get_length(jsdisp_t *array)
{
    assert(is_class(array, JSCLASS_ARRAY));
    return array_from_jsdisp(array)->length;
}

static HRESULT get_length(script_ctx_t *ctx, vdisp_t *vdisp, jsdisp_t **jsthis, DWORD *ret)
{
    ArrayInstance *array;
    jsval_t val;
    HRESULT hres;

    array = array_this(vdisp);
    if(array) {
        *jsthis = &array->dispex;
        *ret = array->length;
        return S_OK;
    }

    if(!is_jsdisp(vdisp))
        return JS_E_JSCRIPT_EXPECTED;

    hres = jsdisp_propget_name(vdisp->u.jsdisp, L"length", &val);
    if(FAILED(hres))
        return hres;

    hres = to_uint32(ctx, val, ret);
    jsval_release(val);
    if(FAILED(hres))
        return hres;

    *jsthis = vdisp->u.jsdisp;
    return S_OK;
}

static HRESULT set_length(jsdisp_t *obj, DWORD length)
{
    if(is_class(obj, JSCLASS_ARRAY)) {
        array_from_jsdisp(obj)->length = length;
        return S_OK;
    }

    return jsdisp_propput_name(obj, L"length", jsval_number(length));
}

static WCHAR *idx_to_str(DWORD idx, WCHAR *ptr)
{
    if(!idx) {
        *ptr = '0';
        return ptr;
    }

    while(idx) {
        *ptr-- = '0' + (idx%10);
        idx /= 10;
    }

    return ptr+1;
}

static HRESULT Array_get_length(script_ctx_t *ctx, jsdisp_t *jsthis, jsval_t *r)
{
    TRACE("%p\n", jsthis);

    *r = jsval_number(array_from_jsdisp(jsthis)->length);
    return S_OK;
}

static HRESULT Array_set_length(script_ctx_t *ctx, jsdisp_t *jsthis, jsval_t value)
{
    ArrayInstance *This = array_from_jsdisp(jsthis);
    DOUBLE len = -1;
    DWORD i;
    HRESULT hres;

    TRACE("%p %d\n", This, This->length);

    hres = to_number(ctx, value, &len);
    if(FAILED(hres))
        return hres;

    len = floor(len);
    if(len!=(DWORD)len)
        return JS_E_INVALID_LENGTH;

    for(i=len; i < This->length; i++) {
        hres = jsdisp_delete_idx(&This->dispex, i);
        if(FAILED(hres))
            return hres;
    }

    This->length = len;
    return S_OK;
}

static HRESULT concat_array(jsdisp_t *array, ArrayInstance *obj, DWORD *len)
{
    jsval_t val;
    DWORD i;
    HRESULT hres;

    for(i=0; i < obj->length; i++) {
        hres = jsdisp_get_idx(&obj->dispex, i, &val);
        if(hres == DISP_E_UNKNOWNNAME)
            continue;
        if(FAILED(hres))
            return hres;

        hres = jsdisp_propput_idx(array, *len+i, val);
        jsval_release(val);
        if(FAILED(hres))
            return hres;
    }

    *len += obj->length;
    return S_OK;
}

static HRESULT concat_obj(jsdisp_t *array, IDispatch *obj, DWORD *len)
{
    jsdisp_t *jsobj;
    HRESULT hres;

    jsobj = iface_to_jsdisp(obj);
    if(jsobj) {
        if(is_class(jsobj, JSCLASS_ARRAY)) {
            hres = concat_array(array, array_from_jsdisp(jsobj), len);
            jsdisp_release(jsobj);
            return hres;
        }
        jsdisp_release(jsobj);
    }

    return jsdisp_propput_idx(array, (*len)++, jsval_disp(obj));
}

static HRESULT Array_concat(script_ctx_t *ctx, vdisp_t *jsthis, WORD flags, unsigned argc, jsval_t *argv,
        jsval_t *r)
{
    jsdisp_t *ret;
    DWORD len = 0;
    HRESULT hres;

    TRACE("\n");

    hres = create_array(ctx, 0, &ret);
    if(FAILED(hres))
        return hres;

    hres = concat_obj(ret, jsthis->u.disp, &len);
    if(SUCCEEDED(hres)) {
        DWORD i;

        for(i=0; i < argc; i++) {
            if(is_object_instance(argv[i]))
                hres = concat_obj(ret, get_object(argv[i]), &len);
            else
                hres = jsdisp_propput_idx(ret, len++, argv[i]);
            if(FAILED(hres))
                break;
        }
    }

    if(FAILED(hres))
        return hres;

    if(r)
        *r = jsval_obj(ret);
    else
        jsdisp_release(ret);
    return S_OK;
}

static HRESULT array_join(script_ctx_t *ctx, jsdisp_t *array, DWORD length, const WCHAR *sep,
        unsigned seplen, jsval_t *r)
{
    jsstr_t **str_tab, *ret = NULL;
    jsval_t val;
    DWORD i;
    HRESULT hres = E_FAIL;

    if(!length) {
        if(r)
            *r = jsval_string(jsstr_empty());
        return S_OK;
    }

    str_tab = heap_alloc_zero(length * sizeof(*str_tab));
    if(!str_tab)
        return E_OUTOFMEMORY;

    for(i=0; i < length; i++) {
        hres = jsdisp_get_idx(array, i, &val);
        if(hres == DISP_E_UNKNOWNNAME) {
            hres = S_OK;
            continue;
        } else if(FAILED(hres))
            break;

        if(!is_undefined(val) && !is_null(val)) {
            hres = to_string(ctx, val, str_tab+i);
            jsval_release(val);
            if(FAILED(hres))
                break;
        }
    }

    if(SUCCEEDED(hres)) {
        DWORD len = 0;

        if(str_tab[0])
            len = jsstr_length(str_tab[0]);
        for(i=1; i < length; i++) {
            len += seplen;
            if(str_tab[i])
                len += jsstr_length(str_tab[i]);
            if(len > JSSTR_MAX_LENGTH) {
                hres = E_OUTOFMEMORY;
                break;
            }
        }

        if(SUCCEEDED(hres)) {
            WCHAR *ptr = NULL;

            ret = jsstr_alloc_buf(len, &ptr);
            if(ret) {
                if(str_tab[0])
                    ptr += jsstr_flush(str_tab[0], ptr);

                for(i=1; i < length; i++) {
                    if(seplen) {
                        memcpy(ptr, sep, seplen*sizeof(WCHAR));
                        ptr += seplen;
                    }

                    if(str_tab[i])
                        ptr += jsstr_flush(str_tab[i], ptr);
                }
            }else {
                hres = E_OUTOFMEMORY;
            }
        }
    }

    for(i=0; i < length; i++) {
        if(str_tab[i])
            jsstr_release(str_tab[i]);
    }
    heap_free(str_tab);
    if(FAILED(hres))
        return hres;

    TRACE("= %s\n", debugstr_jsstr(ret));

    if(r)
        *r = jsval_string(ret);
    else
        jsstr_release(ret);
    return S_OK;
}

/* ECMA-262 3rd Edition    15.4.4.5 */
static HRESULT Array_join(script_ctx_t *ctx, vdisp_t *vthis, WORD flags, unsigned argc, jsval_t *argv,
        jsval_t *r)
{
    jsdisp_t *jsthis;
    DWORD length;
    HRESULT hres;

    TRACE("\n");

    hres = get_length(ctx, vthis, &jsthis, &length);
    if(FAILED(hres))
        return hres;

    if(argc) {
        const WCHAR *sep;
        jsstr_t *sep_str;

        hres = to_flat_string(ctx, argv[0], &sep_str, &sep);
        if(FAILED(hres))
            return hres;

        hres = array_join(ctx, jsthis, length, sep, jsstr_length(sep_str), r);

        jsstr_release(sep_str);
    }else {
        hres = array_join(ctx, jsthis, length, L",", 1, r);
    }

    return hres;
}

static HRESULT Array_pop(script_ctx_t *ctx, vdisp_t *vthis, WORD flags, unsigned argc, jsval_t *argv,
        jsval_t *r)
{
    jsdisp_t *jsthis;
    jsval_t val;
    DWORD length;
    HRESULT hres;

    TRACE("\n");

    hres = get_length(ctx, vthis, &jsthis, &length);
    if(FAILED(hres))
        return hres;

    if(!length) {
        hres = set_length(jsthis, 0);
        if(FAILED(hres))
            return hres;

        if(r)
            *r = jsval_undefined();
        return S_OK;
    }

    length--;
    hres = jsdisp_get_idx(jsthis, length, &val);
    if(SUCCEEDED(hres))
        hres = jsdisp_delete_idx(jsthis, length);
    else if(hres == DISP_E_UNKNOWNNAME) {
        val = jsval_undefined();
        hres = S_OK;
    }else
        return hres;

    if(SUCCEEDED(hres))
        hres = set_length(jsthis, length);

    if(FAILED(hres)) {
        jsval_release(val);
        return hres;
    }

    if(r)
        *r = val;
    else
        jsval_release(val);
    return hres;
}

/* ECMA-262 3rd Edition    15.4.4.7 */
static HRESULT Array_push(script_ctx_t *ctx, vdisp_t *vthis, WORD flags, unsigned argc, jsval_t *argv,
        jsval_t *r)
{
    jsdisp_t *jsthis;
    DWORD length = 0;
    unsigned i;
    HRESULT hres;

    TRACE("\n");

    hres = get_length(ctx, vthis, &jsthis, &length);
    if(FAILED(hres))
        return hres;

    for(i=0; i < argc; i++) {
        hres = jsdisp_propput_idx(jsthis, length+i, argv[i]);
        if(FAILED(hres))
            return hres;
    }

    hres = set_length(jsthis, length+argc);
    if(FAILED(hres))
        return hres;

    if(r)
        *r = jsval_number(length+argc);
    return S_OK;
}

static HRESULT Array_reverse(script_ctx_t *ctx, vdisp_t *vthis, WORD flags, unsigned argc, jsval_t *argv,
        jsval_t *r)
{
    jsdisp_t *jsthis;
    DWORD length, k, l;
    jsval_t v1, v2;
    HRESULT hres1, hres2;

    TRACE("\n");

    hres1 = get_length(ctx, vthis, &jsthis, &length);
    if(FAILED(hres1))
        return hres1;

    for(k=0; k<length/2; k++) {
        l = length-k-1;

        hres1 = jsdisp_get_idx(jsthis, k, &v1);
        if(FAILED(hres1) && hres1!=DISP_E_UNKNOWNNAME)
            return hres1;

        hres2 = jsdisp_get_idx(jsthis, l, &v2);
        if(FAILED(hres2) && hres2!=DISP_E_UNKNOWNNAME) {
            jsval_release(v1);
            return hres2;
        }

        if(hres1 == DISP_E_UNKNOWNNAME)
            hres1 = jsdisp_delete_idx(jsthis, l);
        else
            hres1 = jsdisp_propput_idx(jsthis, l, v1);

        if(FAILED(hres1)) {
            jsval_release(v1);
            jsval_release(v2);
            return hres1;
        }

        if(hres2 == DISP_E_UNKNOWNNAME)
            hres2 = jsdisp_delete_idx(jsthis, k);
        else
            hres2 = jsdisp_propput_idx(jsthis, k, v2);

        if(FAILED(hres2)) {
            jsval_release(v2);
            return hres2;
        }
    }

    if(r)
        *r = jsval_obj(jsdisp_addref(jsthis));
    return S_OK;
}

/* ECMA-262 3rd Edition    15.4.4.9 */
static HRESULT Array_shift(script_ctx_t *ctx, vdisp_t *vthis, WORD flags, unsigned argc, jsval_t *argv,
        jsval_t *r)
{
    jsdisp_t *jsthis;
    DWORD length = 0, i;
    jsval_t v, ret;
    HRESULT hres;

    TRACE("\n");

    hres = get_length(ctx, vthis, &jsthis, &length);
    if(FAILED(hres))
        return hres;

    if(!length) {
        hres = set_length(jsthis, 0);
        if(FAILED(hres))
            return hres;

        if(r)
            *r = jsval_undefined();
        return S_OK;
    }

    hres = jsdisp_get_idx(jsthis, 0, &ret);
    if(hres == DISP_E_UNKNOWNNAME) {
        ret = jsval_undefined();
        hres = S_OK;
    }

    for(i=1; SUCCEEDED(hres) && i<length; i++) {
        hres = jsdisp_get_idx(jsthis, i, &v);
        if(hres == DISP_E_UNKNOWNNAME)
            hres = jsdisp_delete_idx(jsthis, i-1);
        else if(SUCCEEDED(hres))
            hres = jsdisp_propput_idx(jsthis, i-1, v);
    }

    if(SUCCEEDED(hres)) {
        hres = jsdisp_delete_idx(jsthis, length-1);
        if(SUCCEEDED(hres))
            hres = set_length(jsthis, length-1);
    }

    if(FAILED(hres))
        return hres;

    if(r)
        *r = ret;
    else
        jsval_release(ret);
    return hres;
}

/* ECMA-262 3rd Edition    15.4.4.10 */
static HRESULT Array_slice(script_ctx_t *ctx, vdisp_t *vthis, WORD flags, unsigned argc, jsval_t *argv, jsval_t *r)
{
    jsdisp_t *arr, *jsthis;
    DOUBLE range;
    DWORD length, start, end, idx;
    HRESULT hres;

    TRACE("\n");

    hres = get_length(ctx, vthis, &jsthis, &length);
    if(FAILED(hres))
        return hres;

    if(argc) {
        hres = to_number(ctx, argv[0], &range);
        if(FAILED(hres))
            return hres;

        range = floor(range);
        if(-range>length || isnan(range)) start = 0;
        else if(range < 0) start = range+length;
        else if(range <= length) start = range;
        else start = length;
    }
    else start = 0;

    if(argc > 1) {
        hres = to_number(ctx, argv[1], &range);
        if(FAILED(hres))
            return hres;

        range = floor(range);
        if(-range>length) end = 0;
        else if(range < 0) end = range+length;
        else if(range <= length) end = range;
        else end = length;
    }
    else end = length;

    hres = create_array(ctx, (end>start)?end-start:0, &arr);
    if(FAILED(hres))
        return hres;

    for(idx=start; idx<end; idx++) {
        jsval_t v;

        hres = jsdisp_get_idx(jsthis, idx, &v);
        if(hres == DISP_E_UNKNOWNNAME)
            continue;

        if(SUCCEEDED(hres)) {
            hres = jsdisp_propput_idx(arr, idx-start, v);
            jsval_release(v);
        }

        if(FAILED(hres)) {
            jsdisp_release(arr);
            return hres;
        }
    }

    if(r)
        *r = jsval_obj(arr);
    else
        jsdisp_release(arr);

    return S_OK;
}

static HRESULT sort_cmp(script_ctx_t *ctx, jsdisp_t *cmp_func, jsval_t v1, jsval_t v2, INT *cmp)
{
    HRESULT hres;

    if(cmp_func) {
        jsval_t args[2] = {v1, v2};
        jsval_t res;
        double n;

        hres = jsdisp_call_value(cmp_func, NULL, DISPATCH_METHOD, 2, args, &res);
        if(FAILED(hres))
            return hres;

        hres = to_number(ctx, res, &n);
        jsval_release(res);
        if(FAILED(hres))
            return hres;

        if(n == 0)
            *cmp = 0;
        *cmp = n > 0.0 ? 1 : -1;
    }else if(is_undefined(v1)) {
        *cmp = is_undefined(v2) ? 0 : 1;
    }else if(is_undefined(v2)) {
        *cmp = -1;
    }else if(is_number(v1) && is_number(v2)) {
        double d = get_number(v1)-get_number(v2);
        if(d > 0.0)
            *cmp = 1;
        else
            *cmp = d < -0.0 ? -1 : 0;
    }else {
        jsstr_t *x, *y;

        hres = to_string(ctx, v1, &x);
        if(FAILED(hres))
            return hres;

        hres = to_string(ctx, v2, &y);
        if(SUCCEEDED(hres)) {
            *cmp = jsstr_cmp(x, y);
            jsstr_release(y);
        }
        jsstr_release(x);
        if(FAILED(hres))
            return hres;
    }

    return S_OK;
}

/* ECMA-262 3rd Edition    15.4.4.11 */
static HRESULT Array_sort(script_ctx_t *ctx, vdisp_t *vthis, WORD flags, unsigned argc, jsval_t *argv,
        jsval_t *r)
{
    jsdisp_t *jsthis, *cmp_func = NULL;
    jsval_t *vtab, **sorttab = NULL;
    DWORD length;
    DWORD i;
    HRESULT hres = S_OK;

    TRACE("\n");

    hres = get_length(ctx, vthis, &jsthis, &length);
    if(FAILED(hres))
        return hres;

    if(argc >= 1) {
        if(is_object_instance(argv[0])) {
            if(argc > 1 && ctx->version < SCRIPTLANGUAGEVERSION_ES5) {
                WARN("invalid arg_cnt %d\n", argc);
                return JS_E_JSCRIPT_EXPECTED;
            }
            cmp_func = iface_to_jsdisp(get_object(argv[0]));
            if(!cmp_func || !is_class(cmp_func, JSCLASS_FUNCTION)) {
                WARN("cmp_func is not a function\n");
                if(cmp_func)
                    jsdisp_release(cmp_func);
                return JS_E_JSCRIPT_EXPECTED;
            }
        }else if(ctx->version >= SCRIPTLANGUAGEVERSION_ES5 ? !is_undefined(argv[0]) : !is_null(argv[0])) {
            WARN("invalid arg %s\n", debugstr_jsval(argv[0]));
            return JS_E_JSCRIPT_EXPECTED;
        }
    }

    if(!length) {
        if(cmp_func)
            jsdisp_release(cmp_func);
        if(r)
            *r = jsval_obj(jsdisp_addref(jsthis));
        return S_OK;
    }

    vtab = heap_alloc_zero(length * sizeof(*vtab));
    if(vtab) {
        for(i=0; i<length; i++) {
            hres = jsdisp_get_idx(jsthis, i, vtab+i);
            if(hres == DISP_E_UNKNOWNNAME) {
                vtab[i] = jsval_undefined();
                hres = S_OK;
            } else if(FAILED(hres)) {
                WARN("Could not get elem %d: %08x\n", i, hres);
                break;
            }
        }
    }else {
        hres = E_OUTOFMEMORY;
    }

    if(SUCCEEDED(hres)) {
        sorttab = heap_alloc(length*2*sizeof(*sorttab));
        if(!sorttab)
            hres = E_OUTOFMEMORY;
    }

    /* merge-sort */
    if(SUCCEEDED(hres)) {
        jsval_t *tmpv, **tmpbuf;
        INT cmp;

        tmpbuf = sorttab + length;
        for(i=0; i < length; i++)
            sorttab[i] = vtab+i;

        for(i=0; i < length/2; i++) {
            hres = sort_cmp(ctx, cmp_func, *sorttab[2*i+1], *sorttab[2*i], &cmp);
            if(FAILED(hres))
                break;

            if(cmp < 0) {
                tmpv = sorttab[2*i];
                sorttab[2*i] = sorttab[2*i+1];
                sorttab[2*i+1] = tmpv;
            }
        }

        if(SUCCEEDED(hres)) {
            DWORD k, a, b, bend;

            for(k=2; k < length; k *= 2) {
                for(i=0; i+k < length; i += 2*k) {
                    a = b = 0;
                    if(i+2*k <= length)
                        bend = k;
                    else
                        bend = length - (i+k);

                    memcpy(tmpbuf, sorttab+i, k*sizeof(jsval_t*));

                    while(a < k && b < bend) {
                        hres = sort_cmp(ctx, cmp_func, *tmpbuf[a], *sorttab[i+k+b], &cmp);
                        if(FAILED(hres))
                            break;

                        if(cmp < 0) {
                            sorttab[i+a+b] = tmpbuf[a];
                            a++;
                        }else {
                            sorttab[i+a+b] = sorttab[i+k+b];
                            b++;
                        }
                    }

                    if(FAILED(hres))
                        break;

                    if(a < k)
                        memcpy(sorttab+i+a+b, tmpbuf+a, (k-a)*sizeof(jsval_t*));
                }

                if(FAILED(hres))
                    break;
            }
        }

        for(i=0; SUCCEEDED(hres) && i < length; i++)
            hres = jsdisp_propput_idx(jsthis, i, *sorttab[i]);
    }

    if(vtab) {
        for(i=0; i < length; i++)
            jsval_release(vtab[i]);
        heap_free(vtab);
    }
    heap_free(sorttab);
    if(cmp_func)
        jsdisp_release(cmp_func);

    if(FAILED(hres))
        return hres;

    if(r)
        *r = jsval_obj(jsdisp_addref(jsthis));
    return S_OK;
}

/* ECMA-262 3rd Edition    15.4.4.12 */
static HRESULT Array_splice(script_ctx_t *ctx, vdisp_t *vthis, WORD flags, unsigned argc, jsval_t *argv,
        jsval_t *r)
{
    DWORD length, start=0, delete_cnt=0, i, add_args = 0;
    jsdisp_t *ret_array = NULL, *jsthis;
    jsval_t val;
    double d;
    int n;
    HRESULT hres = S_OK;

    TRACE("\n");

    hres = get_length(ctx, vthis, &jsthis, &length);
    if(FAILED(hres))
        return hres;

    if(argc) {
        hres = to_integer(ctx, argv[0], &d);
        if(FAILED(hres))
            return hres;

        if(is_int32(d)) {
            if((n = d) >= 0)
                start = min(n, length);
            else
                start = -n > length ? 0 : length + n;
        }else {
            start = d < 0.0 ? 0 : length;
        }
    }

    if(argc >= 2) {
        hres = to_integer(ctx, argv[1], &d);
        if(FAILED(hres))
            return hres;

        if(is_int32(d)) {
            if((n = d) > 0)
                delete_cnt = min(n, length-start);
        }else if(d > 0.0) {
            delete_cnt = length-start;
        }

        add_args = argc-2;
    }

    if(r) {
        hres = create_array(ctx, 0, &ret_array);
        if(FAILED(hres))
            return hres;

        for(i=0; SUCCEEDED(hres) && i < delete_cnt; i++) {
            hres = jsdisp_get_idx(jsthis, start+i, &val);
            if(hres == DISP_E_UNKNOWNNAME) {
                hres = S_OK;
            }else if(SUCCEEDED(hres)) {
                hres = jsdisp_propput_idx(ret_array, i, val);
                jsval_release(val);
            }
        }

        if(SUCCEEDED(hres))
            hres = jsdisp_propput_name(ret_array, L"length", jsval_number(delete_cnt));
    }

    if(add_args < delete_cnt) {
        for(i = start; SUCCEEDED(hres) && i < length-delete_cnt; i++) {
            hres = jsdisp_get_idx(jsthis, i+delete_cnt, &val);
            if(hres == DISP_E_UNKNOWNNAME) {
                hres = jsdisp_delete_idx(jsthis, i+add_args);
            }else if(SUCCEEDED(hres)) {
                hres = jsdisp_propput_idx(jsthis, i+add_args, val);
                jsval_release(val);
            }
        }

        for(i=length; SUCCEEDED(hres) && i != length-delete_cnt+add_args; i--)
            hres = jsdisp_delete_idx(jsthis, i-1);
    }else if(add_args > delete_cnt) {
        for(i=length-delete_cnt; SUCCEEDED(hres) && i != start; i--) {
            hres = jsdisp_get_idx(jsthis, i+delete_cnt-1, &val);
            if(hres == DISP_E_UNKNOWNNAME) {
                hres = jsdisp_delete_idx(jsthis, i+add_args-1);
            }else if(SUCCEEDED(hres)) {
                hres = jsdisp_propput_idx(jsthis, i+add_args-1, val);
                jsval_release(val);
            }
        }
    }

    for(i=0; SUCCEEDED(hres) && i < add_args; i++)
        hres = jsdisp_propput_idx(jsthis, start+i, argv[i+2]);

    if(SUCCEEDED(hres))
        hres = jsdisp_propput_name(jsthis, L"length", jsval_number(length-delete_cnt+add_args));

    if(FAILED(hres)) {
        if(ret_array)
            jsdisp_release(ret_array);
        return hres;
    }

    if(r)
        *r = jsval_obj(ret_array);
    return S_OK;
}

/* ECMA-262 3rd Edition    15.4.4.2 */
static HRESULT Array_toString(script_ctx_t *ctx, vdisp_t *jsthis, WORD flags, unsigned argc, jsval_t *argv,
        jsval_t *r)
{
    ArrayInstance *array;

    TRACE("\n");

    array = array_this(jsthis);
    if(!array)
        return JS_E_ARRAY_EXPECTED;

    return array_join(ctx, &array->dispex, array->length, L",", 1, r);
}

static HRESULT Array_toLocaleString(script_ctx_t *ctx, vdisp_t *vthis, WORD flags, unsigned argc, jsval_t *argv,
        jsval_t *r)
{
    FIXME("\n");
    return E_NOTIMPL;
}

static HRESULT Array_forEach(script_ctx_t *ctx, vdisp_t *vthis, WORD flags, unsigned argc, jsval_t *argv,
        jsval_t *r)
{
    IDispatch *context_obj = NULL, *callback;
    jsval_t value, args[3], res;
    jsdisp_t *jsthis;
    unsigned length, i;
    HRESULT hres;

    TRACE("\n");

    hres = get_length(ctx, vthis, &jsthis, &length);
    if(FAILED(hres))
        return hres;

    /* Fixme check IsCallable */
    if(!argc || !is_object_instance(argv[0]) || !get_object(argv[0])) {
        FIXME("Invalid arg %s\n", debugstr_jsval(argc ? argv[0] : jsval_undefined()));
        return E_INVALIDARG;
    }
    callback = get_object(argv[0]);

    if(argc > 1 && !is_undefined(argv[1])) {
        if(!is_object_instance(argv[1]) || !get_object(argv[1])) {
            FIXME("Unsupported context this %s\n", debugstr_jsval(argv[1]));
            return E_NOTIMPL;
        }
        context_obj = get_object(argv[1]);
    }

    for(i = 0; i < length; i++) {
        hres = jsdisp_get_idx(jsthis, i, &value);
        if(hres == DISP_E_UNKNOWNNAME)
            continue;
        if(FAILED(hres))
            return hres;

        args[0] = value;
        args[1] = jsval_number(i);
        args[2] = jsval_obj(jsthis);
        hres = disp_call_value(ctx, callback, context_obj, DISPATCH_METHOD, ARRAY_SIZE(args), args, &res);
        jsval_release(value);
        if(FAILED(hres))
            return hres;
        jsval_release(res);
    }

    if(r) *r = jsval_undefined();
    return S_OK;
}

static HRESULT Array_indexOf(script_ctx_t *ctx, vdisp_t *vthis, WORD flags, unsigned argc, jsval_t *argv,
        jsval_t *r)
{
    jsdisp_t *jsthis;
    unsigned length, i, from = 0;
    jsval_t search, value;
    BOOL eq;
    HRESULT hres;

    TRACE("\n");

    hres = get_length(ctx, vthis, &jsthis, &length);
    if(FAILED(hres))
        return hres;
    if(!length) {
        if(r) *r = jsval_number(-1);
        return S_OK;
    }

    search = argc ? argv[0] : jsval_undefined();

    if(argc > 1) {
        double from_arg;

        hres = to_integer(ctx, argv[1], &from_arg);
        if(FAILED(hres))
            return hres;

        if(from_arg >= 0)
            from = min(from_arg, length);
        else
            from = max(from_arg + length, 0);
    }

    for(i = from; i < length; i++) {
        hres = jsdisp_get_idx(jsthis, i, &value);
        if(hres == DISP_E_UNKNOWNNAME)
            continue;
        if(FAILED(hres))
            return hres;

        hres = jsval_strict_equal(value, search, &eq);
        jsval_release(value);
        if(FAILED(hres))
            return hres;
        if(eq) {
            if(r) *r = jsval_number(i);
            return S_OK;
        }
    }

    if(r) *r = jsval_number(-1);
    return S_OK;
}

static HRESULT Array_map(script_ctx_t *ctx, vdisp_t *vthis, WORD flags, unsigned argc, jsval_t *argv, jsval_t *r)
{
    IDispatch *context_this = NULL, *callback;
    jsval_t callback_args[3], mapped_value;
    jsdisp_t *jsthis, *array;
    DWORD length, k;
    HRESULT hres;

    TRACE("\n");

    hres = get_length(ctx, vthis, &jsthis, &length);
    if(FAILED(hres)) {
        FIXME("Could not get length\n");
        return hres;
    }

    /* FIXME: check IsCallable */
    if(!argc || !is_object_instance(argv[0]) || !get_object(argv[0])) {
        FIXME("Invalid arg %s\n", debugstr_jsval(argc ? argv[0] : jsval_undefined()));
        return E_INVALIDARG;
    }
    callback = get_object(argv[0]);

    if(argc > 1) {
        if(is_object_instance(argv[1]) && get_object(argv[1])) {
            context_this = get_object(argv[1]);
        }else if(!is_undefined(argv[1])) {
            FIXME("Unsupported context this %s\n", debugstr_jsval(argv[1]));
            return E_NOTIMPL;
        }
    }

    hres = create_array(ctx, length, &array);
    if(FAILED(hres))
        return hres;

    for(k = 0; k < length; k++) {
        hres = jsdisp_get_idx(jsthis, k, &callback_args[0]);
        if(hres == DISP_E_UNKNOWNNAME)
            continue;
        if(FAILED(hres))
            break;

        callback_args[1] = jsval_number(k);
        callback_args[2] = jsval_obj(jsthis);
        hres = disp_call_value(ctx, callback, context_this, DISPATCH_METHOD, 3, callback_args, &mapped_value);
        jsval_release(callback_args[0]);
        if(FAILED(hres))
            break;

        hres = jsdisp_propput_idx(array, k, mapped_value);
        if(FAILED(hres))
            break;
    }

    if(SUCCEEDED(hres) && r)
        *r = jsval_obj(array);
    else
        jsdisp_release(array);
    return hres;
}

static HRESULT Array_reduce(script_ctx_t *ctx, vdisp_t *vthis, WORD flags, unsigned argc, jsval_t *argv, jsval_t *r)
{
    IDispatch *context_this = NULL, *callback;
    jsval_t callback_args[4], acc, new_acc;
    BOOL have_value = FALSE;
    jsdisp_t *jsthis;
    DWORD length, k;
    HRESULT hres;

    TRACE("\n");

    hres = get_length(ctx, vthis, &jsthis, &length);
    if(FAILED(hres)) {
        FIXME("Could not get length\n");
        return hres;
    }

    /* Fixme check IsCallable */
    if(!argc || !is_object_instance(argv[0]) || !get_object(argv[0])) {
        FIXME("Invalid arg %s\n", debugstr_jsval(argc ? argv[0] : jsval_undefined()));
        return E_INVALIDARG;
    }
    callback = get_object(argv[0]);

    if(argc > 1) {
        have_value = TRUE;
        hres = jsval_copy(argv[1], &acc);
        if(FAILED(hres))
            return hres;
    }

    for(k = 0; k < length; k++) {
        hres = jsdisp_get_idx(jsthis, k, &callback_args[1]);
        if(hres == DISP_E_UNKNOWNNAME)
            continue;
        if(FAILED(hres))
            break;

        if(!have_value) {
            have_value = TRUE;
            acc = callback_args[1];
            continue;
        }

        callback_args[0] = acc;
        callback_args[2] = jsval_number(k);
        callback_args[3] = jsval_obj(jsthis);
        hres = disp_call_value(ctx, callback, context_this, DISPATCH_METHOD, ARRAY_SIZE(callback_args), callback_args, &new_acc);
        jsval_release(callback_args[1]);
        if(FAILED(hres))
            break;

        jsval_release(acc);
        acc = new_acc;
    }

    if(SUCCEEDED(hres) && !have_value) {
        WARN("No array element\n");
        hres = JS_E_INVALID_ACTION;
    }

    if(SUCCEEDED(hres) && r)
        *r = acc;
    else if(have_value)
        jsval_release(acc);
    return hres;
}

/* ECMA-262 3rd Edition    15.4.4.13 */
static HRESULT Array_unshift(script_ctx_t *ctx, vdisp_t *vthis, WORD flags, unsigned argc, jsval_t *argv,
        jsval_t *r)
{
    jsdisp_t *jsthis;
    WCHAR buf[14], *buf_end, *str;
    DWORD i, length;
    jsval_t val;
    DISPID id;
    HRESULT hres;

    TRACE("\n");

    hres = get_length(ctx, vthis, &jsthis, &length);
    if(FAILED(hres))
        return hres;

    if(argc) {
        buf_end = buf + ARRAY_SIZE(buf)-1;
        *buf_end-- = 0;
        i = length;

        while(i--) {
            str = idx_to_str(i, buf_end);

            hres = jsdisp_get_id(jsthis, str, 0, &id);
            if(SUCCEEDED(hres)) {
                hres = jsdisp_propget(jsthis, id, &val);
                if(FAILED(hres))
                    return hres;

                hres = jsdisp_propput_idx(jsthis, i+argc, val);
                jsval_release(val);
            }else if(hres == DISP_E_UNKNOWNNAME) {
                hres = IDispatchEx_DeleteMemberByDispID(vthis->u.dispex, id);
            }
        }

        if(FAILED(hres))
            return hres;
    }

    for(i=0; i<argc; i++) {
        hres = jsdisp_propput_idx(jsthis, i, argv[i]);
        if(FAILED(hres))
            return hres;
    }

    if(argc) {
        length += argc;
        hres = set_length(jsthis, length);
        if(FAILED(hres))
            return hres;
    }

    if(r)
        *r = ctx->version < 2 ? jsval_undefined() : jsval_number(length);
    return S_OK;
}

static HRESULT Array_get_value(script_ctx_t *ctx, jsdisp_t *jsthis, jsval_t *r)
{
    ArrayInstance *array = array_from_jsdisp(jsthis);

    TRACE("\n");

    return array_join(ctx, &array->dispex, array->length, L",", 1, r);
}

static void Array_destructor(jsdisp_t *dispex)
{
    heap_free(dispex);
}

static void Array_on_put(jsdisp_t *dispex, const WCHAR *name)
{
    ArrayInstance *array = array_from_jsdisp(dispex);
    const WCHAR *ptr = name;
    DWORD id = 0;

    if(!is_digit(*ptr))
        return;

    while(*ptr && is_digit(*ptr)) {
        id = id*10 + (*ptr-'0');
        ptr++;
    }

    if(*ptr)
        return;

    if(id >= array->length)
        array->length = id+1;
}

static const builtin_prop_t Array_props[] = {
    {L"concat",                Array_concat,               PROPF_METHOD|1},
    {L"forEach",               Array_forEach,              PROPF_METHOD|PROPF_ES5|1},
    {L"indexOf",               Array_indexOf,              PROPF_METHOD|PROPF_ES5|1},
    {L"join",                  Array_join,                 PROPF_METHOD|1},
    {L"length",                NULL,0,                     Array_get_length, Array_set_length},
    {L"map",                   Array_map,                  PROPF_METHOD|PROPF_ES5|1},
    {L"pop",                   Array_pop,                  PROPF_METHOD},
    {L"push",                  Array_push,                 PROPF_METHOD|1},
    {L"reduce",                Array_reduce,               PROPF_METHOD|PROPF_ES5|1},
    {L"reverse",               Array_reverse,              PROPF_METHOD},
    {L"shift",                 Array_shift,                PROPF_METHOD},
    {L"slice",                 Array_slice,                PROPF_METHOD|2},
    {L"sort",                  Array_sort,                 PROPF_METHOD|1},
    {L"splice",                Array_splice,               PROPF_METHOD|2},
    {L"toLocaleString",        Array_toLocaleString,       PROPF_METHOD},
    {L"toString",              Array_toString,             PROPF_METHOD},
    {L"unshift",               Array_unshift,              PROPF_METHOD|1},
};

static const builtin_info_t Array_info = {
    JSCLASS_ARRAY,
    {NULL, NULL,0, Array_get_value},
    ARRAY_SIZE(Array_props),
    Array_props,
    Array_destructor,
    Array_on_put
};

static const builtin_prop_t ArrayInst_props[] = {
    {L"length",                NULL,0,                     Array_get_length, Array_set_length}
};

static const builtin_info_t ArrayInst_info = {
    JSCLASS_ARRAY,
    {NULL, NULL,0, Array_get_value},
    ARRAY_SIZE(ArrayInst_props),
    ArrayInst_props,
    Array_destructor,
    Array_on_put
};

/* ECMA-262 5.1 Edition    15.4.3.2 */
static HRESULT ArrayConstr_isArray(script_ctx_t *ctx, vdisp_t *vthis, WORD flags, unsigned argc, jsval_t *argv, jsval_t *r)
{
    jsdisp_t *obj;

    TRACE("\n");

    if(!argc || !is_object_instance(argv[0])) {
        if(r) *r = jsval_bool(FALSE);
        return S_OK;
    }

    obj = iface_to_jsdisp(get_object(argv[0]));
    if(r) *r = jsval_bool(obj && is_class(obj, JSCLASS_ARRAY));
    if(obj) jsdisp_release(obj);
    return S_OK;
}

static HRESULT ArrayConstr_value(script_ctx_t *ctx, vdisp_t *vthis, WORD flags, unsigned argc, jsval_t *argv,
        jsval_t *r)
{
    jsdisp_t *obj;
    DWORD i;
    HRESULT hres;

    TRACE("\n");

    switch(flags) {
    case DISPATCH_METHOD:
    case DISPATCH_CONSTRUCT: {
        if(argc == 1 && is_number(argv[0])) {
            double n = get_number(argv[0]);

            if(n < 0 || !is_int32(n))
                return JS_E_INVALID_LENGTH;

            hres = create_array(ctx, n, &obj);
            if(FAILED(hres))
                return hres;

            *r = jsval_obj(obj);
            return S_OK;
        }

        hres = create_array(ctx, argc, &obj);
        if(FAILED(hres))
            return hres;

        for(i=0; i < argc; i++) {
            hres = jsdisp_propput_idx(obj, i, argv[i]);
            if(FAILED(hres))
                break;
        }
        if(FAILED(hres)) {
            jsdisp_release(obj);
            return hres;
        }

        *r = jsval_obj(obj);
        break;
    }
    default:
        FIXME("unimplemented flags: %x\n", flags);
        return E_NOTIMPL;
    }

    return S_OK;
}

static HRESULT alloc_array(script_ctx_t *ctx, jsdisp_t *object_prototype, ArrayInstance **ret)
{
    ArrayInstance *array;
    HRESULT hres;

    array = heap_alloc_zero(sizeof(ArrayInstance));
    if(!array)
        return E_OUTOFMEMORY;

    if(object_prototype)
        hres = init_dispex(&array->dispex, ctx, &Array_info, object_prototype);
    else
        hres = init_dispex_from_constr(&array->dispex, ctx, &ArrayInst_info, ctx->array_constr);

    if(FAILED(hres)) {
        heap_free(array);
        return hres;
    }

    *ret = array;
    return S_OK;
}

static const builtin_prop_t ArrayConstr_props[] = {
    {L"isArray",    ArrayConstr_isArray,    PROPF_ES5|PROPF_METHOD|1}
};

static const builtin_info_t ArrayConstr_info = {
    JSCLASS_FUNCTION,
    DEFAULT_FUNCTION_VALUE,
    ARRAY_SIZE(ArrayConstr_props),
    ArrayConstr_props,
    NULL,
    NULL
};

HRESULT create_array_constr(script_ctx_t *ctx, jsdisp_t *object_prototype, jsdisp_t **ret)
{
    ArrayInstance *array;
    HRESULT hres;

    hres = alloc_array(ctx, object_prototype, &array);
    if(FAILED(hres))
        return hres;

    hres = create_builtin_constructor(ctx, ArrayConstr_value, L"Array", &ArrayConstr_info, PROPF_CONSTR|1, &array->dispex, ret);

    jsdisp_release(&array->dispex);
    return hres;
}

HRESULT create_array(script_ctx_t *ctx, DWORD length, jsdisp_t **ret)
{
    ArrayInstance *array;
    HRESULT hres;

    hres = alloc_array(ctx, NULL, &array);
    if(FAILED(hres))
        return hres;

    array->length = length;

    *ret = &array->dispex;
    return S_OK;
}
