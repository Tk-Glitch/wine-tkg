/*
 * Copyright 2018 Nikolay Sivov for CodeWeavers
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

#include "d2d1_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(d2d);

static inline struct d2d_transform_graph *impl_from_ID2D1TransformGraph(ID2D1TransformGraph *iface)
{
    return CONTAINING_RECORD(iface, struct d2d_transform_graph, ID2D1TransformGraph_iface);
}

static HRESULT STDMETHODCALLTYPE d2d_transform_graph_QueryInterface(ID2D1TransformGraph *iface, REFIID iid, void **out)
{
    TRACE("iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_ID2D1TransformGraph)
            || IsEqualGUID(iid, &IID_IUnknown))
    {
        ID2D1TransformGraph_AddRef(iface);
        *out = iface;
        return S_OK;
    }

    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE d2d_transform_graph_AddRef(ID2D1TransformGraph *iface)
{
    struct d2d_transform_graph *graph =impl_from_ID2D1TransformGraph(iface);
    ULONG refcount = InterlockedIncrement(&graph->refcount);

    TRACE("%p increasing refcount to %lu.\n", iface, refcount);

    return refcount;
}

static ULONG STDMETHODCALLTYPE d2d_transform_graph_Release(ID2D1TransformGraph *iface)
{
    struct d2d_transform_graph *graph = impl_from_ID2D1TransformGraph(iface);
    ULONG refcount = InterlockedDecrement(&graph->refcount);

    TRACE("%p decreasing refcount to %lu.\n", iface, refcount);

    if (!refcount)
        free(graph);

    return refcount;
}

static UINT32 STDMETHODCALLTYPE d2d_transform_graph_GetInputCount(ID2D1TransformGraph *iface)
{
    FIXME("iface %p stub!\n", iface);

    return 0;
}

static HRESULT STDMETHODCALLTYPE d2d_transform_graph_SetSingleTransformNode(ID2D1TransformGraph *iface,
        ID2D1TransformNode *node)
{
    FIXME("iface %p, node %p stub!\n", iface, node);

    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE d2d_transform_graph_AddNode(ID2D1TransformGraph *iface, ID2D1TransformNode *node)
{
    FIXME("iface %p, node %p stub!\n", iface, node);

    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE d2d_transform_graph_RemoveNode(ID2D1TransformGraph *iface, ID2D1TransformNode *node)
{
    FIXME("iface %p, node %p stub!\n", iface, node);

    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE d2d_transform_graph_SetOutputNode(ID2D1TransformGraph *iface, ID2D1TransformNode *node)
{
    FIXME("iface %p, node %p stub!\n", iface, node);

    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE d2d_transform_graph_ConnectNode(ID2D1TransformGraph *iface,
        ID2D1TransformNode *from_node, ID2D1TransformNode *to_node, UINT32 index)
{
    FIXME("iface %p, from_node %p, to_node %p, index %u stub!\n", iface, from_node, to_node, index);

    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE d2d_transform_graph_ConnectToEffectInput(ID2D1TransformGraph *iface,
        UINT32 input_index, ID2D1TransformNode *node, UINT32 node_index)
{
    FIXME("iface %p, input_index %u, node %p, node_index %u stub!\n", iface, input_index, node, node_index);

    return E_NOTIMPL;
}

static void STDMETHODCALLTYPE d2d_transform_graph_Clear(ID2D1TransformGraph *iface)
{
    FIXME("iface %p stub!\n", iface);
}

static HRESULT STDMETHODCALLTYPE d2d_transform_graph_SetPassthroughGraph(ID2D1TransformGraph *iface, UINT32 index)
{
    FIXME("iface %p, index %u stub!\n", iface, index);

    return E_NOTIMPL;
}

static const ID2D1TransformGraphVtbl d2d_transform_graph_vtbl =
{
    d2d_transform_graph_QueryInterface,
    d2d_transform_graph_AddRef,
    d2d_transform_graph_Release,
    d2d_transform_graph_GetInputCount,
    d2d_transform_graph_SetSingleTransformNode,
    d2d_transform_graph_AddNode,
    d2d_transform_graph_RemoveNode,
    d2d_transform_graph_SetOutputNode,
    d2d_transform_graph_ConnectNode,
    d2d_transform_graph_ConnectToEffectInput,
    d2d_transform_graph_Clear,
    d2d_transform_graph_SetPassthroughGraph,
};

static void d2d_transform_graph_init(struct d2d_transform_graph *graph)
{
    graph->ID2D1TransformGraph_iface.lpVtbl = &d2d_transform_graph_vtbl;
    graph->refcount = 1;
}

static HRESULT STDMETHODCALLTYPE d2d_effect_impl_QueryInterface(ID2D1EffectImpl *iface, REFIID iid, void **out)
{
    TRACE("iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_ID2D1EffectImpl)
            || IsEqualGUID(iid, &IID_IUnknown))
    {
        ID2D1EffectImpl_AddRef(iface);
        *out = iface;
        return S_OK;
    }

    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE d2d_effect_impl_AddRef(ID2D1EffectImpl *iface)
{
    return 2;
}

static ULONG STDMETHODCALLTYPE d2d_effect_impl_Release(ID2D1EffectImpl *iface)
{
    return 1;
}

static HRESULT STDMETHODCALLTYPE d2d_effect_impl_Initialize(ID2D1EffectImpl *iface,
        ID2D1EffectContext *context, ID2D1TransformGraph *graph)
{
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE d2d_effect_impl_PrepareForRender(ID2D1EffectImpl *iface, D2D1_CHANGE_TYPE type)
{
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE d2d_effect_impl_SetGraph(ID2D1EffectImpl *iface, ID2D1TransformGraph *graph)
{
    return E_NOTIMPL;
}

static const ID2D1EffectImplVtbl d2d_effect_impl_vtbl =
{
    d2d_effect_impl_QueryInterface,
    d2d_effect_impl_AddRef,
    d2d_effect_impl_Release,
    d2d_effect_impl_Initialize,
    d2d_effect_impl_PrepareForRender,
    d2d_effect_impl_SetGraph,
};

static HRESULT STDMETHODCALLTYPE builtin_factory_stub(IUnknown **effect_impl)
{
    static ID2D1EffectImpl builtin_stub = { &d2d_effect_impl_vtbl };

    *effect_impl = (IUnknown *)&builtin_stub;

    return S_OK;
}

struct d2d_effect_info
{
    const CLSID *clsid;
    UINT32 default_input_count;
    UINT32 min_inputs;
    UINT32 max_inputs;
};

static const struct d2d_effect_info builtin_effects[] =
{
    {&CLSID_D2D12DAffineTransform,      1, 1, 1},
    {&CLSID_D2D13DPerspectiveTransform, 1, 1, 1},
    {&CLSID_D2D1Composite,              2, 1, 0xffffffff},
    {&CLSID_D2D1Crop,                   1, 1, 1},
    {&CLSID_D2D1Shadow,                 1, 1, 1},
    {&CLSID_D2D1Grayscale,              1, 1, 1},
};

void d2d_effects_init_builtins(struct d2d_factory *factory)
{
    struct d2d_effect_registration *effect;
    unsigned int i;

    for (i = 0; i < ARRAY_SIZE(builtin_effects); ++i)
    {
        const struct d2d_effect_info *info = &builtin_effects[i];
        WCHAR max_inputs[32];

        if (!(effect = calloc(1, sizeof(*effect))))
            return;

        swprintf(max_inputs, ARRAY_SIZE(max_inputs), L"%lu", info->max_inputs);
        d2d_effect_properties_add(&effect->properties, L"MinInputs", D2D1_PROPERTY_MIN_INPUTS,
                D2D1_PROPERTY_TYPE_UINT32, L"1");
        d2d_effect_properties_add(&effect->properties, L"MaxInputs", D2D1_PROPERTY_MAX_INPUTS,
                D2D1_PROPERTY_TYPE_UINT32, max_inputs);

        memcpy(&effect->id, info->clsid, sizeof(*info->clsid));
        effect->default_input_count = info->default_input_count;
        effect->factory = builtin_factory_stub;
        effect->builtin = TRUE;
        d2d_factory_register_effect(factory, effect);
    }
}

/* Same syntax is used for value and default values. */
static HRESULT d2d_effect_parse_float_array(D2D1_PROPERTY_TYPE type, const WCHAR *value,
        float *vec)
{
    unsigned int i, num_components;
    WCHAR *end_ptr;

    /* Type values are sequential. */
    switch (type)
    {
        case D2D1_PROPERTY_TYPE_VECTOR2:
        case D2D1_PROPERTY_TYPE_VECTOR3:
        case D2D1_PROPERTY_TYPE_VECTOR4:
            num_components = (type - D2D1_PROPERTY_TYPE_VECTOR2) + 2;
            break;
        case D2D1_PROPERTY_TYPE_MATRIX_3X2:
            num_components = 6;
            break;
        case D2D1_PROPERTY_TYPE_MATRIX_4X3:
        case D2D1_PROPERTY_TYPE_MATRIX_4X4:
        case D2D1_PROPERTY_TYPE_MATRIX_5X4:
            num_components = (type - D2D1_PROPERTY_TYPE_MATRIX_4X3) * 4 + 12;
            break;
        default:
            return E_UNEXPECTED;
    }

    if (*(value++) != '(') return E_INVALIDARG;

    for (i = 0; i < num_components; ++i)
    {
        vec[i] = wcstof(value, &end_ptr);
        if (value == end_ptr) return E_INVALIDARG;
        value = end_ptr;

        /* Trailing characters after last component are ignored. */
        if (i == num_components - 1) continue;
        if (*(value++) != ',') return E_INVALIDARG;
    }

    return S_OK;
}

static HRESULT d2d_effect_properties_internal_add(struct d2d_effect_properties *props,
        const WCHAR *name, UINT32 index, BOOL subprop, D2D1_PROPERTY_TYPE type, const WCHAR *value)
{
    static const UINT32 sizes[] =
    {
        0,                   /* D2D1_PROPERTY_TYPE_UNKNOWN */
        0,                   /* D2D1_PROPERTY_TYPE_STRING */
        sizeof(BOOL),        /* D2D1_PROPERTY_TYPE_BOOL */
        sizeof(UINT32),      /* D2D1_PROPERTY_TYPE_UINT32 */
        sizeof(INT32),       /* D2D1_PROPERTY_TYPE_INT32 */
        sizeof(float),       /* D2D1_PROPERTY_TYPE_FLOAT */
        2 * sizeof(float),   /* D2D1_PROPERTY_TYPE_VECTOR2 */
        3 * sizeof(float),   /* D2D1_PROPERTY_TYPE_VECTOR3 */
        4 * sizeof(float),   /* D2D1_PROPERTY_TYPE_VECTOR4 */
        0,                   /* FIXME: D2D1_PROPERTY_TYPE_BLOB */
        sizeof(void *),      /* D2D1_PROPERTY_TYPE_IUNKNOWN */
        sizeof(UINT32),      /* D2D1_PROPERTY_TYPE_ENUM */
        sizeof(UINT32),      /* D2D1_PROPERTY_TYPE_ARRAY */
        sizeof(CLSID),       /* D2D1_PROPERTY_TYPE_CLSID */
        6 * sizeof(float),   /* D2D1_PROPERTY_TYPE_MATRIX_3X2 */
        12 * sizeof(float),  /* D2D1_PROPERTY_TYPE_MATRIX_4X3 */
        16 * sizeof(float),  /* D2D1_PROPERTY_TYPE_MATRIX_4X4 */
        20 * sizeof(float),  /* D2D1_PROPERTY_TYPE_MATRIX_5X4 */
        sizeof(void *),      /* D2D1_PROPERTY_TYPE_COLOR_CONTEXT */
    };
    struct d2d_effect_property *p;
    HRESULT hr;

    assert(type >= D2D1_PROPERTY_TYPE_STRING && type <= D2D1_PROPERTY_TYPE_COLOR_CONTEXT);

    if (type == D2D1_PROPERTY_TYPE_BLOB)
    {
        FIXME("Ignoring property %s of type %u.\n", wine_dbgstr_w(name), type);
        return S_OK;
    }

    if (!d2d_array_reserve((void **)&props->properties, &props->size, props->count + 1,
            sizeof(*props->properties)))
    {
        return E_OUTOFMEMORY;
    }

    /* TODO: we could save some space for properties that have both getter and setter. */
    if (!d2d_array_reserve((void **)&props->data.ptr, &props->data.size,
            props->data.count + sizes[type], sizeof(*props->data.ptr)))
    {
        return E_OUTOFMEMORY;
    }
    props->data.count += sizes[type];

    p = &props->properties[props->count++];
    memset(p, 0, sizeof(*p));
    p->index = index;
    if (p->index < 0x80000000)
    {
        props->custom_count++;
        /* FIXME: this should probably be controlled by subproperty */
        p->readonly = FALSE;
    }
    else if (subprop)
        p->readonly = TRUE;
    else
        p->readonly = index != D2D1_PROPERTY_CACHED && index != D2D1_PROPERTY_PRECISION;
    p->name = wcsdup(name);
    p->type = type;
    if (p->type == D2D1_PROPERTY_TYPE_STRING && value)
    {
        p->data.ptr = wcsdup(value);
        p->size = (wcslen(value) + 1) * sizeof(WCHAR);
    }
    else
    {
        void *src = NULL;
        UINT32 _uint32;
        float _vec[20];
        CLSID _clsid;
        BOOL _bool;

        p->data.offset = props->offset;
        p->size = sizes[type];
        props->offset += p->size;

        if (value)
        {
            switch (p->type)
            {
                case D2D1_PROPERTY_TYPE_UINT32:
                case D2D1_PROPERTY_TYPE_INT32:
                case D2D1_PROPERTY_TYPE_ENUM:
                    _uint32 = wcstoul(value, NULL, 10);
                    src = &_uint32;
                    break;
                case D2D1_PROPERTY_TYPE_BOOL:
                    if (!wcscmp(value, L"true")) _bool = TRUE;
                    else if (!wcscmp(value, L"false")) _bool = FALSE;
                    else return E_INVALIDARG;
                    src = &_bool;
                    break;
                case D2D1_PROPERTY_TYPE_CLSID:
                    CLSIDFromString(value, &_clsid);
                    src = &_clsid;
                    break;
                case D2D1_PROPERTY_TYPE_VECTOR2:
                case D2D1_PROPERTY_TYPE_VECTOR3:
                case D2D1_PROPERTY_TYPE_VECTOR4:
                case D2D1_PROPERTY_TYPE_MATRIX_3X2:
                case D2D1_PROPERTY_TYPE_MATRIX_4X3:
                case D2D1_PROPERTY_TYPE_MATRIX_4X4:
                case D2D1_PROPERTY_TYPE_MATRIX_5X4:
                    if (FAILED(hr = d2d_effect_parse_float_array(p->type, value, _vec)))
                    {
                        WARN("Failed to parse float array %s for type %u.\n",
                                wine_dbgstr_w(value), p->type);
                        return hr;
                    }
                    src = _vec;
                    break;
                case D2D1_PROPERTY_TYPE_IUNKNOWN:
                case D2D1_PROPERTY_TYPE_COLOR_CONTEXT:
                    break;
                default:
                    FIXME("Initial value for property type %u is not handled.\n", p->type);
            }

            if (src && p->size) memcpy(props->data.ptr + p->data.offset, src, p->size);
        }
        else if (p->size)
            memset(props->data.ptr + p->data.offset, 0, p->size);
    }

    return S_OK;
}

HRESULT d2d_effect_properties_add(struct d2d_effect_properties *props, const WCHAR *name,
        UINT32 index, D2D1_PROPERTY_TYPE type, const WCHAR *value)
{
    return d2d_effect_properties_internal_add(props, name, index, FALSE, type, value);
}

HRESULT d2d_effect_subproperties_add(struct d2d_effect_properties *props, const WCHAR *name,
        UINT32 index, D2D1_PROPERTY_TYPE type, const WCHAR *value)
{
    return d2d_effect_properties_internal_add(props, name, index, TRUE, type, value);
}

static HRESULT d2d_effect_duplicate_properties(struct d2d_effect_properties *dst,
        const struct d2d_effect_properties *src)
{
    HRESULT hr;
    size_t i;

    memset(dst, 0, sizeof(*dst));
    dst->offset = src->offset;
    dst->size = src->count;
    dst->count = src->count;
    dst->custom_count = src->custom_count;
    dst->data.size = src->data.count;
    dst->data.count = src->data.count;

    if (!(dst->data.ptr = malloc(dst->data.size)))
        return E_OUTOFMEMORY;
    memcpy(dst->data.ptr, src->data.ptr, dst->data.size);

    if (!(dst->properties = calloc(dst->size, sizeof(*dst->properties))))
        return E_OUTOFMEMORY;

    for (i = 0; i < dst->count; ++i)
    {
        struct d2d_effect_property *d = &dst->properties[i];
        const struct d2d_effect_property *s = &src->properties[i];

        *d = *s;
        d->name = wcsdup(s->name);
        if (d->type == D2D1_PROPERTY_TYPE_STRING)
            d->data.ptr = wcsdup((WCHAR *)s->data.ptr);

        if (s->subproperties)
        {
            if (!(d->subproperties = calloc(1, sizeof(*d->subproperties))))
                return E_OUTOFMEMORY;
            if (FAILED(hr = d2d_effect_duplicate_properties(d->subproperties, s->subproperties)))
                return hr;
        }
    }

    return S_OK;
}

static struct d2d_effect_property * d2d_effect_properties_get_property_by_index(
        const struct d2d_effect_properties *properties, UINT32 index)
{
    unsigned int i;

    for (i = 0; i < properties->count; ++i)
    {
        if (properties->properties[i].index == index)
            return &properties->properties[i];
    }

    return NULL;
}

struct d2d_effect_property * d2d_effect_properties_get_property_by_name(
        const struct d2d_effect_properties *properties, const WCHAR *name)
{
    unsigned int i;

    for (i = 0; i < properties->count; ++i)
    {
        if (!wcscmp(properties->properties[i].name, name))
            return &properties->properties[i];
    }

    return NULL;
}

static UINT32 d2d_effect_properties_get_value_size(const struct d2d_effect_properties *properties,
        UINT32 index)
{
    struct d2d_effect *effect = properties->effect;
    struct d2d_effect_property *prop;
    UINT32 size;

    if (!(prop = d2d_effect_properties_get_property_by_index(properties, index)))
        return 0;

    if (prop->get_function)
    {
        if (FAILED(prop->get_function((IUnknown *)effect->impl, NULL, 0, &size))) return 0;
        return size;
    }

    return prop->size;
}

static HRESULT d2d_effect_return_string(const WCHAR *str, WCHAR *buffer, UINT32 buffer_size)
{
    UINT32 size = str ? wcslen(str) : 0;
    if (size >= buffer_size) return D2DERR_INSUFFICIENT_BUFFER;
    if (str) memcpy(buffer, str, (size + 1) * sizeof(*buffer));
    else *buffer = 0;
    return S_OK;
}

static HRESULT d2d_effect_property_get_value(const struct d2d_effect_properties *properties,
        const struct d2d_effect_property *prop, D2D1_PROPERTY_TYPE type, BYTE *value, UINT32 size)
{
    struct d2d_effect *effect = properties->effect;
    UINT32 actual_size;

    if (type != D2D1_PROPERTY_TYPE_UNKNOWN && prop->type != type) return E_INVALIDARG;
    if (prop->type != D2D1_PROPERTY_TYPE_STRING && prop->size != size) return E_INVALIDARG;

    if (prop->get_function)
        return prop->get_function((IUnknown *)effect->impl, value, size, &actual_size);

    switch (prop->type)
    {
        case D2D1_PROPERTY_TYPE_BLOB:
            FIXME("Unimplemented for type %u.\n", prop->type);
            return E_NOTIMPL;
        case D2D1_PROPERTY_TYPE_STRING:
            return d2d_effect_return_string(prop->data.ptr, (WCHAR *)value, size / sizeof(WCHAR));
        default:
            memcpy(value, properties->data.ptr + prop->data.offset, size);
            break;
    }

    return S_OK;
}

static HRESULT d2d_effect_property_set_value(struct d2d_effect_properties *properties,
        struct d2d_effect_property *prop, D2D1_PROPERTY_TYPE type, const BYTE *value, UINT32 size)
{
    struct d2d_effect *effect = properties->effect;
    if (prop->readonly) return E_INVALIDARG;
    if (type != D2D1_PROPERTY_TYPE_UNKNOWN && prop->type != type) return E_INVALIDARG;
    if (prop->get_function && !prop->set_function) return E_INVALIDARG;
    if (prop->index < 0x80000000 && !prop->set_function) return E_INVALIDARG;

    if (prop->set_function)
        return prop->set_function((IUnknown *)effect->impl, value, size);

    if (prop->size != size) return E_INVALIDARG;

    switch (prop->type)
    {
        case D2D1_PROPERTY_TYPE_BOOL:
        case D2D1_PROPERTY_TYPE_UINT32:
        case D2D1_PROPERTY_TYPE_ENUM:
            memcpy(properties->data.ptr + prop->data.offset, value, size);
            break;
        default:
            FIXME("Unhandled type %u.\n", prop->type);
    }

    return S_OK;
}

void d2d_effect_properties_cleanup(struct d2d_effect_properties *props)
{
    struct d2d_effect_property *p;
    size_t i;

    for (i = 0; i < props->count; ++i)
    {
        p = &props->properties[i];
        free(p->name);
        if (p->type == D2D1_PROPERTY_TYPE_STRING)
            free(p->data.ptr);
        if (p->subproperties)
        {
            d2d_effect_properties_cleanup(p->subproperties);
            free(p->subproperties);
        }
    }
    free(props->properties);
    free(props->data.ptr);
}

static inline struct d2d_effect_context *impl_from_ID2D1EffectContext(ID2D1EffectContext *iface)
{
    return CONTAINING_RECORD(iface, struct d2d_effect_context, ID2D1EffectContext_iface);
}

static void d2d_effect_context_cleanup(struct d2d_effect_context *effect_context)
{
    size_t i;

    for (i = 0; i < effect_context->shader_count; ++i)
        IUnknown_Release(effect_context->shaders[i].shader);
    free(effect_context->shaders);

    ID2D1DeviceContext1_Release(&effect_context->device_context->ID2D1DeviceContext1_iface);
}

static HRESULT d2d_effect_context_add_shader(struct d2d_effect_context *effect_context,
        REFGUID shader_id, void *shader)
{
    struct d2d_shader *entry;

    if (!d2d_array_reserve((void **)&effect_context->shaders, &effect_context->shaders_size,
            effect_context->shader_count + 1, sizeof(*effect_context->shaders)))
    {
        WARN("Failed to resize shaders array.\n");
        return E_OUTOFMEMORY;
    }

    entry = &effect_context->shaders[effect_context->shader_count++];
    entry->id = *shader_id;
    entry->shader = shader;
    IUnknown_AddRef(entry->shader);

    return S_OK;
}

static HRESULT STDMETHODCALLTYPE d2d_effect_context_QueryInterface(ID2D1EffectContext *iface, REFIID iid, void **out)
{
    TRACE("iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_ID2D1EffectContext)
            || IsEqualGUID(iid, &IID_IUnknown))
    {
        ID2D1EffectContext_AddRef(iface);
        *out = iface;
        return S_OK;
    }

    WARN("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));

    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE d2d_effect_context_AddRef(ID2D1EffectContext *iface)
{
    struct d2d_effect_context *effect_context = impl_from_ID2D1EffectContext(iface);
    ULONG refcount = InterlockedIncrement(&effect_context->refcount);

    TRACE("%p increasing refcount to %lu.\n", iface, refcount);

    return refcount;
}

static ULONG STDMETHODCALLTYPE d2d_effect_context_Release(ID2D1EffectContext *iface)
{
    struct d2d_effect_context *effect_context = impl_from_ID2D1EffectContext(iface);
    ULONG refcount = InterlockedDecrement(&effect_context->refcount);

    TRACE("%p decreasing refcount to %lu.\n", iface, refcount);

    if (!refcount)
    {
        d2d_effect_context_cleanup(effect_context);
        free(effect_context);
    }

    return refcount;
}

static void STDMETHODCALLTYPE d2d_effect_context_GetDpi(ID2D1EffectContext *iface, float *dpi_x, float *dpi_y)
{
    struct d2d_effect_context *effect_context = impl_from_ID2D1EffectContext(iface);

    TRACE("iface %p, dpi_x %p, dpi_y %p.\n", iface, dpi_x, dpi_y);

    ID2D1DeviceContext1_GetDpi(&effect_context->device_context->ID2D1DeviceContext1_iface, dpi_x, dpi_y);
}

static HRESULT STDMETHODCALLTYPE d2d_effect_context_CreateEffect(ID2D1EffectContext *iface,
        REFCLSID clsid, ID2D1Effect **effect)
{
    struct d2d_effect_context *effect_context = impl_from_ID2D1EffectContext(iface);

    TRACE("iface %p, clsid %s, effect %p.\n", iface, debugstr_guid(clsid), effect);

    return ID2D1DeviceContext1_CreateEffect(&effect_context->device_context->ID2D1DeviceContext1_iface,
            clsid, effect);
}

static HRESULT STDMETHODCALLTYPE d2d_effect_context_GetMaximumSupportedFeatureLevel(ID2D1EffectContext *iface,
        const D3D_FEATURE_LEVEL *levels, UINT32 level_count, D3D_FEATURE_LEVEL *max_level)
{
    FIXME("iface %p, levels %p, level_count %u, max_level %p stub!\n", iface, levels, level_count, max_level);

    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE d2d_effect_context_CreateTransformNodeFromEffect(ID2D1EffectContext *iface,
        ID2D1Effect *effect, ID2D1TransformNode **node)
{
    FIXME("iface %p, effect %p, node %p stub!\n", iface, effect, node);

    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE d2d_effect_context_CreateBlendTransform(ID2D1EffectContext *iface,
        UINT32 num_inputs, const D2D1_BLEND_DESCRIPTION *description, ID2D1BlendTransform **transform)
{
    FIXME("iface %p, num_inputs %u, description %p, transform %p stub!\n", iface, num_inputs, description, transform);

    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE d2d_effect_context_CreateBorderTransform(ID2D1EffectContext *iface,
        D2D1_EXTEND_MODE mode_x, D2D1_EXTEND_MODE mode_y, ID2D1BorderTransform **transform)
{
    FIXME("iface %p, mode_x %#x, mode_y %#x, transform %p stub!\n", iface, mode_x, mode_y, transform);

    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE d2d_effect_context_CreateOffsetTransform(ID2D1EffectContext *iface,
        D2D1_POINT_2L offset, ID2D1OffsetTransform **transform)
{
    FIXME("iface %p, offset %s, transform %p stub!\n", iface, debug_d2d_point_2l(&offset), transform);

    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE d2d_effect_context_CreateBoundsAdjustmentTransform(ID2D1EffectContext *iface,
        const D2D1_RECT_L *output_rect, ID2D1BoundsAdjustmentTransform **transform)
{
    FIXME("iface %p, output_rect %s, transform %p stub!\n", iface, debug_d2d_rect_l(output_rect), transform);

    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE d2d_effect_context_LoadPixelShader(ID2D1EffectContext *iface,
        REFGUID shader_id, const BYTE *buffer, UINT32 buffer_size)
{
    struct d2d_effect_context *effect_context = impl_from_ID2D1EffectContext(iface);
    ID3D11PixelShader *shader;
    HRESULT hr;

    TRACE("iface %p, shader_id %s, buffer %p, buffer_size %u.\n",
            iface, debugstr_guid(shader_id), buffer, buffer_size);

    if (ID2D1EffectContext_IsShaderLoaded(iface, shader_id))
        return S_OK;

    if (FAILED(hr = ID3D11Device1_CreatePixelShader(effect_context->device_context->d3d_device,
            buffer, buffer_size, NULL, &shader)))
    {
        WARN("Failed to create a pixel shader, hr %#lx.\n", hr);
        return hr;
    }

    hr = d2d_effect_context_add_shader(effect_context, shader_id, shader);
    ID3D11PixelShader_Release(shader);

    return hr;
}

static HRESULT STDMETHODCALLTYPE d2d_effect_context_LoadVertexShader(ID2D1EffectContext *iface,
        REFGUID shader_id, const BYTE *buffer, UINT32 buffer_size)
{
    struct d2d_effect_context *effect_context = impl_from_ID2D1EffectContext(iface);
    ID3D11VertexShader *vertex_shader;
    HRESULT hr;

    TRACE("iface %p, shader_id %s, buffer %p, buffer_size %u.\n",
            iface, debugstr_guid(shader_id), buffer, buffer_size);

    if (ID2D1EffectContext_IsShaderLoaded(iface, shader_id))
        return S_OK;

    if (FAILED(hr = ID3D11Device1_CreateVertexShader(effect_context->device_context->d3d_device,
            buffer, buffer_size, NULL, &vertex_shader)))
    {
        WARN("Failed to create vertex shader, hr %#lx.\n", hr);
        return hr;
    }

    hr = d2d_effect_context_add_shader(effect_context, shader_id, vertex_shader);
    ID3D11VertexShader_Release(vertex_shader);

    return hr;
}

static HRESULT STDMETHODCALLTYPE d2d_effect_context_LoadComputeShader(ID2D1EffectContext *iface,
        REFGUID shader_id, const BYTE *buffer, UINT32 buffer_size)
{
    struct d2d_effect_context *effect_context = impl_from_ID2D1EffectContext(iface);
    ID3D11ComputeShader *shader;
    HRESULT hr;

    TRACE("iface %p, shader_id %s, buffer %p, buffer_size %u.\n",
            iface, debugstr_guid(shader_id), buffer, buffer_size);

    if (ID2D1EffectContext_IsShaderLoaded(iface, shader_id))
        return S_OK;

    if (FAILED(hr = ID3D11Device1_CreateComputeShader(effect_context->device_context->d3d_device,
            buffer, buffer_size, NULL, &shader)))
    {
        WARN("Failed to create a compute shader, hr %#lx.\n", hr);
        return hr;
    }

    hr = d2d_effect_context_add_shader(effect_context, shader_id, shader);
    ID3D11ComputeShader_Release(shader);

    return hr;
}

static BOOL STDMETHODCALLTYPE d2d_effect_context_IsShaderLoaded(ID2D1EffectContext *iface, REFGUID shader_id)
{
    struct d2d_effect_context *effect_context = impl_from_ID2D1EffectContext(iface);
    size_t i;

    TRACE("iface %p, shader_id %s.\n", iface, debugstr_guid(shader_id));

    for (i = 0; i < effect_context->shader_count; ++i)
    {
        if (IsEqualGUID(shader_id, &effect_context->shaders[i].id))
            return TRUE;
    }

    return FALSE;
}

static HRESULT STDMETHODCALLTYPE d2d_effect_context_CreateResourceTexture(ID2D1EffectContext *iface,
        const GUID *id,  const D2D1_RESOURCE_TEXTURE_PROPERTIES *texture_properties,
        const BYTE *data, const UINT32 *strides, UINT32 data_size, ID2D1ResourceTexture **texture)
{
    FIXME("iface %p, id %s, texture_properties %p, data %p, strides %p, data_size %u, texture %p stub!\n",
            iface, debugstr_guid(id), texture_properties, data, strides, data_size, texture);

    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE d2d_effect_context_FindResourceTexture(ID2D1EffectContext *iface,
        const GUID *id, ID2D1ResourceTexture **texture)
{
    FIXME("iface %p, id %s, texture %p stub!\n", iface, debugstr_guid(id), texture);

    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE d2d_effect_context_CreateVertexBuffer(ID2D1EffectContext *iface,
        const D2D1_VERTEX_BUFFER_PROPERTIES *buffer_properties, const GUID *id,
        const D2D1_CUSTOM_VERTEX_BUFFER_PROPERTIES *custom_buffer_properties, ID2D1VertexBuffer **buffer)
{
    FIXME("iface %p, buffer_properties %p, id %s, custom_buffer_properties %p, buffer %p stub!\n",
            iface, buffer_properties, debugstr_guid(id), custom_buffer_properties, buffer);

    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE d2d_effect_context_FindVertexBuffer(ID2D1EffectContext *iface,
        const GUID *id, ID2D1VertexBuffer **buffer)
{
    FIXME("iface %p, id %s, buffer %p stub!\n", iface, debugstr_guid(id), buffer);

    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE d2d_effect_context_CreateColorContext(ID2D1EffectContext *iface,
        D2D1_COLOR_SPACE space, const BYTE *profile, UINT32 profile_size, ID2D1ColorContext **color_context)
{
    struct d2d_effect_context *effect_context = impl_from_ID2D1EffectContext(iface);

    TRACE("iface %p, space %#x, profile %p, profile_size %u, color_context %p.\n",
            iface, space, profile, profile_size, color_context);

    return ID2D1DeviceContext1_CreateColorContext(&effect_context->device_context->ID2D1DeviceContext1_iface,
            space, profile, profile_size, color_context);
}

static HRESULT STDMETHODCALLTYPE d2d_effect_context_CreateColorContextFromFilename(ID2D1EffectContext *iface,
        const WCHAR *filename, ID2D1ColorContext **color_context)
{
    struct d2d_effect_context *effect_context = impl_from_ID2D1EffectContext(iface);

    TRACE("iface %p, filename %s, color_context %p.\n", iface, debugstr_w(filename), color_context);

    return ID2D1DeviceContext1_CreateColorContextFromFilename(&effect_context->device_context->ID2D1DeviceContext1_iface,
            filename, color_context);
}

static HRESULT STDMETHODCALLTYPE d2d_effect_context_CreateColorContextFromWicColorContext(ID2D1EffectContext *iface,
        IWICColorContext *wic_color_context, ID2D1ColorContext **color_context)
{
    struct d2d_effect_context *effect_context = impl_from_ID2D1EffectContext(iface);

    TRACE("iface %p, wic_color_context %p, color_context %p.\n", iface, wic_color_context, color_context);

    return ID2D1DeviceContext1_CreateColorContextFromWicColorContext(&effect_context->device_context->ID2D1DeviceContext1_iface,
            wic_color_context, color_context);
}

static HRESULT STDMETHODCALLTYPE d2d_effect_context_CheckFeatureSupport(ID2D1EffectContext *iface,
        D2D1_FEATURE feature, void *data, UINT32 data_size)
{
    struct d2d_effect_context *effect_context = impl_from_ID2D1EffectContext(iface);
    D3D11_FEATURE d3d11_feature;

    TRACE("iface %p, feature %#x, data %p, data_size %u.\n", iface, feature, data, data_size);

    /* Data structures are compatible. */
    switch (feature)
    {
        case D2D1_FEATURE_DOUBLES: d3d11_feature = D3D11_FEATURE_DOUBLES; break;
        case D2D1_FEATURE_D3D10_X_HARDWARE_OPTIONS: d3d11_feature = D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS; break;
        default:
            WARN("Unexpected feature index %d.\n", feature);
            return E_INVALIDARG;
    }

    return ID3D11Device1_CheckFeatureSupport(effect_context->device_context->d3d_device,
            d3d11_feature, data, data_size);
}

static BOOL STDMETHODCALLTYPE d2d_effect_context_IsBufferPrecisionSupported(ID2D1EffectContext *iface,
        D2D1_BUFFER_PRECISION precision)
{
    struct d2d_effect_context *effect_context = impl_from_ID2D1EffectContext(iface);

    TRACE("iface %p, precision %u.\n", iface, precision);

    return ID2D1DeviceContext1_IsBufferPrecisionSupported(&effect_context->device_context->ID2D1DeviceContext1_iface,
            precision);
}

static const ID2D1EffectContextVtbl d2d_effect_context_vtbl =
{
    d2d_effect_context_QueryInterface,
    d2d_effect_context_AddRef,
    d2d_effect_context_Release,
    d2d_effect_context_GetDpi,
    d2d_effect_context_CreateEffect,
    d2d_effect_context_GetMaximumSupportedFeatureLevel,
    d2d_effect_context_CreateTransformNodeFromEffect,
    d2d_effect_context_CreateBlendTransform,
    d2d_effect_context_CreateBorderTransform,
    d2d_effect_context_CreateOffsetTransform,
    d2d_effect_context_CreateBoundsAdjustmentTransform,
    d2d_effect_context_LoadPixelShader,
    d2d_effect_context_LoadVertexShader,
    d2d_effect_context_LoadComputeShader,
    d2d_effect_context_IsShaderLoaded,
    d2d_effect_context_CreateResourceTexture,
    d2d_effect_context_FindResourceTexture,
    d2d_effect_context_CreateVertexBuffer,
    d2d_effect_context_FindVertexBuffer,
    d2d_effect_context_CreateColorContext,
    d2d_effect_context_CreateColorContextFromFilename,
    d2d_effect_context_CreateColorContextFromWicColorContext,
    d2d_effect_context_CheckFeatureSupport,
    d2d_effect_context_IsBufferPrecisionSupported,
};

void d2d_effect_context_init(struct d2d_effect_context *effect_context, struct d2d_device_context *device_context)
{
    effect_context->ID2D1EffectContext_iface.lpVtbl = &d2d_effect_context_vtbl;
    effect_context->refcount = 1;
    effect_context->device_context = device_context;
    ID2D1DeviceContext1_AddRef(&device_context->ID2D1DeviceContext1_iface);
}

static inline struct d2d_effect *impl_from_ID2D1Effect(ID2D1Effect *iface)
{
    return CONTAINING_RECORD(iface, struct d2d_effect, ID2D1Effect_iface);
}

static void d2d_effect_cleanup(struct d2d_effect *effect)
{
    unsigned int i;

    for (i = 0; i < effect->input_count; ++i)
    {
        if (effect->inputs[i])
            ID2D1Image_Release(effect->inputs[i]);
    }
    free(effect->inputs);
    ID2D1EffectContext_Release(&effect->effect_context->ID2D1EffectContext_iface);
    ID2D1TransformGraph_Release(&effect->graph->ID2D1TransformGraph_iface);
    d2d_effect_properties_cleanup(&effect->properties);
    if (effect->impl)
        ID2D1EffectImpl_Release(effect->impl);
}

static HRESULT STDMETHODCALLTYPE d2d_effect_QueryInterface(ID2D1Effect *iface, REFIID iid, void **out)
{
    struct d2d_effect *effect = impl_from_ID2D1Effect(iface);
    TRACE("iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out);

    if (IsEqualGUID(iid, &IID_ID2D1Effect)
            || IsEqualGUID(iid, &IID_ID2D1Properties)
            || IsEqualGUID(iid, &IID_IUnknown))
    {
        ID2D1Effect_AddRef(iface);
        *out = iface;
        return S_OK;
    }

    if (IsEqualGUID(iid, &IID_ID2D1Image)
            || IsEqualGUID(iid, &IID_ID2D1Resource))
    {
        ID2D1Image_AddRef(&effect->ID2D1Image_iface);
        *out = &effect->ID2D1Image_iface;
        return S_OK;
    }

    WARN("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));

    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE d2d_effect_AddRef(ID2D1Effect *iface)
{
    struct d2d_effect *effect = impl_from_ID2D1Effect(iface);
    ULONG refcount = InterlockedIncrement(&effect->refcount);

    TRACE("%p increasing refcount to %lu.\n", iface, refcount);

    return refcount;
}

static ULONG STDMETHODCALLTYPE d2d_effect_Release(ID2D1Effect *iface)
{
    struct d2d_effect *effect = impl_from_ID2D1Effect(iface);
    ULONG refcount = InterlockedDecrement(&effect->refcount);

    TRACE("%p decreasing refcount to %lu.\n", iface, refcount);

    if (!refcount)
    {
        d2d_effect_cleanup(effect);
        free(effect);
    }

    return refcount;
}

static UINT32 STDMETHODCALLTYPE d2d_effect_GetPropertyCount(ID2D1Effect *iface)
{
    struct d2d_effect *effect = impl_from_ID2D1Effect(iface);

    TRACE("iface %p.\n", iface);

    return ID2D1Properties_GetPropertyCount(&effect->properties.ID2D1Properties_iface);
}

static HRESULT STDMETHODCALLTYPE d2d_effect_GetPropertyName(ID2D1Effect *iface, UINT32 index,
        WCHAR *name, UINT32 name_count)
{
    struct d2d_effect *effect = impl_from_ID2D1Effect(iface);

    TRACE("iface %p, index %u, name %p, name_count %u.\n", iface, index, name, name_count);

    return ID2D1Properties_GetPropertyName(&effect->properties.ID2D1Properties_iface,
            index, name, name_count);
}

static UINT32 STDMETHODCALLTYPE d2d_effect_GetPropertyNameLength(ID2D1Effect *iface, UINT32 index)
{
    struct d2d_effect *effect = impl_from_ID2D1Effect(iface);

    TRACE("iface %p, index %u.\n", iface, index);

    return ID2D1Properties_GetPropertyNameLength(&effect->properties.ID2D1Properties_iface, index);
}

static D2D1_PROPERTY_TYPE STDMETHODCALLTYPE d2d_effect_GetType(ID2D1Effect *iface, UINT32 index)
{
    struct d2d_effect *effect = impl_from_ID2D1Effect(iface);

    TRACE("iface %p, index %#x.\n", iface, index);

    return ID2D1Properties_GetType(&effect->properties.ID2D1Properties_iface, index);
}

static UINT32 STDMETHODCALLTYPE d2d_effect_GetPropertyIndex(ID2D1Effect *iface, const WCHAR *name)
{
    struct d2d_effect *effect = impl_from_ID2D1Effect(iface);

    TRACE("iface %p, name %s.\n", iface, debugstr_w(name));

    return ID2D1Properties_GetPropertyIndex(&effect->properties.ID2D1Properties_iface, name);
}

static HRESULT STDMETHODCALLTYPE d2d_effect_SetValueByName(ID2D1Effect *iface, const WCHAR *name,
        D2D1_PROPERTY_TYPE type, const BYTE *value, UINT32 value_size)
{
    struct d2d_effect *effect = impl_from_ID2D1Effect(iface);

    TRACE("iface %p, name %s, type %u, value %p, value_size %u.\n", iface, debugstr_w(name),
            type, value, value_size);

    return ID2D1Properties_SetValueByName(&effect->properties.ID2D1Properties_iface, name,
            type, value, value_size);
}

static HRESULT STDMETHODCALLTYPE d2d_effect_SetValue(ID2D1Effect *iface, UINT32 index, D2D1_PROPERTY_TYPE type,
        const BYTE *value, UINT32 value_size)
{
    struct d2d_effect *effect = impl_from_ID2D1Effect(iface);

    TRACE("iface %p, index %#x, type %u, value %p, value_size %u.\n", iface, index, type, value, value_size);

    return ID2D1Properties_SetValue(&effect->properties.ID2D1Properties_iface, index, type,
            value, value_size);
}

static HRESULT STDMETHODCALLTYPE d2d_effect_GetValueByName(ID2D1Effect *iface, const WCHAR *name,
        D2D1_PROPERTY_TYPE type, BYTE *value, UINT32 value_size)
{
    struct d2d_effect *effect = impl_from_ID2D1Effect(iface);

    TRACE("iface %p, name %s, type %#x, value %p, value_size %u.\n", iface, debugstr_w(name), type,
            value, value_size);

    return ID2D1Properties_GetValueByName(&effect->properties.ID2D1Properties_iface, name, type,
            value, value_size);
}

static HRESULT STDMETHODCALLTYPE d2d_effect_GetValue(ID2D1Effect *iface, UINT32 index, D2D1_PROPERTY_TYPE type,
        BYTE *value, UINT32 value_size)
{
    struct d2d_effect *effect = impl_from_ID2D1Effect(iface);

    TRACE("iface %p, index %#x, type %u, value %p, value_size %u.\n", iface, index, type, value, value_size);

    return ID2D1Properties_GetValue(&effect->properties.ID2D1Properties_iface, index, type,
            value, value_size);
}

static UINT32 STDMETHODCALLTYPE d2d_effect_GetValueSize(ID2D1Effect *iface, UINT32 index)
{
    struct d2d_effect *effect = impl_from_ID2D1Effect(iface);

    TRACE("iface %p, index %#x.\n", iface, index);

    return ID2D1Properties_GetValueSize(&effect->properties.ID2D1Properties_iface, index);
}

static HRESULT STDMETHODCALLTYPE d2d_effect_GetSubProperties(ID2D1Effect *iface, UINT32 index,
        ID2D1Properties **props)
{
    struct d2d_effect *effect = impl_from_ID2D1Effect(iface);

    TRACE("iface %p, index %u, props %p.\n", iface, index, props);

    return ID2D1Properties_GetSubProperties(&effect->properties.ID2D1Properties_iface, index, props);
}

static void STDMETHODCALLTYPE d2d_effect_SetInput(ID2D1Effect *iface, UINT32 index, ID2D1Image *input, BOOL invalidate)
{
    struct d2d_effect *effect = impl_from_ID2D1Effect(iface);

    TRACE("iface %p, index %u, input %p, invalidate %#x.\n", iface, index, input, invalidate);

    if (index >= effect->input_count)
        return;

    ID2D1Image_AddRef(input);
    if (effect->inputs[index])
        ID2D1Image_Release(effect->inputs[index]);
    effect->inputs[index] = input;
}

static HRESULT STDMETHODCALLTYPE d2d_effect_SetInputCount(ID2D1Effect *iface, UINT32 count)
{
    struct d2d_effect *effect = impl_from_ID2D1Effect(iface);
    unsigned int i, min_inputs, max_inputs;

    TRACE("iface %p, count %u.\n", iface, count);

    d2d_effect_GetValue(iface, D2D1_PROPERTY_MIN_INPUTS, D2D1_PROPERTY_TYPE_UINT32,
            (BYTE *)&min_inputs, sizeof(min_inputs));
    d2d_effect_GetValue(iface, D2D1_PROPERTY_MAX_INPUTS, D2D1_PROPERTY_TYPE_UINT32,
            (BYTE *)&max_inputs, sizeof(max_inputs));

    if (count < min_inputs || count > max_inputs)
        return E_INVALIDARG;
    if (count == effect->input_count)
        return S_OK;

    if (count < effect->input_count)
    {
        for (i = count; i < effect->input_count; ++i)
        {
            if (effect->inputs[i])
                ID2D1Image_Release(effect->inputs[i]);
        }
        effect->input_count = count;
        return S_OK;
    }

    if (!d2d_array_reserve((void **)&effect->inputs, &effect->inputs_size,
            count, sizeof(*effect->inputs)))
    {
        ERR("Failed to resize inputs array.\n");
        return E_OUTOFMEMORY;
    }

    memset(&effect->inputs[effect->input_count], 0, sizeof(*effect->inputs) * (count - effect->input_count));
    effect->input_count = count;

    return S_OK;
}

static void STDMETHODCALLTYPE d2d_effect_GetInput(ID2D1Effect *iface, UINT32 index, ID2D1Image **input)
{
    struct d2d_effect *effect = impl_from_ID2D1Effect(iface);

    TRACE("iface %p, index %u, input %p.\n", iface, index, input);

    if (index < effect->input_count && effect->inputs[index])
        ID2D1Image_AddRef(*input = effect->inputs[index]);
    else
        *input = NULL;
}

static UINT32 STDMETHODCALLTYPE d2d_effect_GetInputCount(ID2D1Effect *iface)
{
    struct d2d_effect *effect = impl_from_ID2D1Effect(iface);

    TRACE("iface %p.\n", iface);

    return effect->input_count;
}

static void STDMETHODCALLTYPE d2d_effect_GetOutput(ID2D1Effect *iface, ID2D1Image **output)
{
    struct d2d_effect *effect = impl_from_ID2D1Effect(iface);

    TRACE("iface %p, output %p.\n", iface, output);

    ID2D1Image_AddRef(*output = &effect->ID2D1Image_iface);
}

static const ID2D1EffectVtbl d2d_effect_vtbl =
{
    d2d_effect_QueryInterface,
    d2d_effect_AddRef,
    d2d_effect_Release,
    d2d_effect_GetPropertyCount,
    d2d_effect_GetPropertyName,
    d2d_effect_GetPropertyNameLength,
    d2d_effect_GetType,
    d2d_effect_GetPropertyIndex,
    d2d_effect_SetValueByName,
    d2d_effect_SetValue,
    d2d_effect_GetValueByName,
    d2d_effect_GetValue,
    d2d_effect_GetValueSize,
    d2d_effect_GetSubProperties,
    d2d_effect_SetInput,
    d2d_effect_SetInputCount,
    d2d_effect_GetInput,
    d2d_effect_GetInputCount,
    d2d_effect_GetOutput,
};

static inline struct d2d_effect *impl_from_ID2D1Image(ID2D1Image *iface)
{
    return CONTAINING_RECORD(iface, struct d2d_effect, ID2D1Image_iface);
}

static HRESULT STDMETHODCALLTYPE d2d_effect_image_QueryInterface(ID2D1Image *iface, REFIID iid, void **out)
{
    struct d2d_effect *effect = impl_from_ID2D1Image(iface);

    TRACE("iface %p, iid %s, out %p.\n", iface, debugstr_guid(iid), out);

    return d2d_effect_QueryInterface(&effect->ID2D1Effect_iface, iid, out);
}

static ULONG STDMETHODCALLTYPE d2d_effect_image_AddRef(ID2D1Image *iface)
{
    struct d2d_effect *effect = impl_from_ID2D1Image(iface);

    TRACE("iface %p.\n", iface);

    return d2d_effect_AddRef(&effect->ID2D1Effect_iface);
}

static ULONG STDMETHODCALLTYPE d2d_effect_image_Release(ID2D1Image *iface)
{
    struct d2d_effect *effect = impl_from_ID2D1Image(iface);

    TRACE("iface %p.\n", iface);

    return d2d_effect_Release(&effect->ID2D1Effect_iface);
}

static void STDMETHODCALLTYPE d2d_effect_image_GetFactory(ID2D1Image *iface, ID2D1Factory **factory)
{
    struct d2d_effect *effect = impl_from_ID2D1Image(iface);

    TRACE("iface %p, factory %p.\n", iface, factory);

    ID2D1Factory_AddRef(*factory = effect->effect_context->device_context->factory);
}

static const ID2D1ImageVtbl d2d_effect_image_vtbl =
{
    d2d_effect_image_QueryInterface,
    d2d_effect_image_AddRef,
    d2d_effect_image_Release,
    d2d_effect_image_GetFactory,
};

static inline struct d2d_effect_properties *impl_from_ID2D1Properties(ID2D1Properties *iface)
{
    return CONTAINING_RECORD(iface, struct d2d_effect_properties, ID2D1Properties_iface);
}

static HRESULT STDMETHODCALLTYPE d2d_effect_properties_QueryInterface(ID2D1Properties *iface,
        REFIID riid, void **obj)
{
    if (IsEqualGUID(riid, &IID_ID2D1Properties) ||
            IsEqualGUID(riid, &IID_IUnknown))
    {
        *obj = iface;
        ID2D1Properties_AddRef(iface);
        return S_OK;
    }

    *obj = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE d2d_effect_properties_AddRef(ID2D1Properties *iface)
{
    struct d2d_effect_properties *properties = impl_from_ID2D1Properties(iface);
    return ID2D1Effect_AddRef(&properties->effect->ID2D1Effect_iface);
}

static ULONG STDMETHODCALLTYPE d2d_effect_properties_Release(ID2D1Properties *iface)
{
    struct d2d_effect_properties *properties = impl_from_ID2D1Properties(iface);
    return ID2D1Effect_Release(&properties->effect->ID2D1Effect_iface);
}

static UINT32 STDMETHODCALLTYPE d2d_effect_properties_GetPropertyCount(ID2D1Properties *iface)
{
    struct d2d_effect_properties *properties = impl_from_ID2D1Properties(iface);

    TRACE("iface %p.\n", iface);

    return properties->custom_count;
}

static HRESULT STDMETHODCALLTYPE d2d_effect_properties_GetPropertyName(ID2D1Properties *iface,
        UINT32 index, WCHAR *name, UINT32 name_count)
{
    struct d2d_effect_properties *properties = impl_from_ID2D1Properties(iface);
    struct d2d_effect_property *prop;

    TRACE("iface %p, index %u, name %p, name_count %u.\n", iface, index, name, name_count);

    if (!(prop = d2d_effect_properties_get_property_by_index(properties, index)))
        return D2DERR_INVALID_PROPERTY;

    return d2d_effect_return_string(prop->name, name, name_count);
}

static UINT32 STDMETHODCALLTYPE d2d_effect_properties_GetPropertyNameLength(ID2D1Properties *iface,
        UINT32 index)
{
    struct d2d_effect_properties *properties = impl_from_ID2D1Properties(iface);
    struct d2d_effect_property *prop;

    TRACE("iface %p, index %u.\n", iface, index);

    if (!(prop = d2d_effect_properties_get_property_by_index(properties, index)))
        return D2DERR_INVALID_PROPERTY;

    return wcslen(prop->name) + 1;
}

static D2D1_PROPERTY_TYPE STDMETHODCALLTYPE d2d_effect_properties_GetType(ID2D1Properties *iface,
        UINT32 index)
{
    struct d2d_effect_properties *properties = impl_from_ID2D1Properties(iface);
    struct d2d_effect_property *prop;

    TRACE("iface %p, index %#x.\n", iface, index);

    if (!(prop = d2d_effect_properties_get_property_by_index(properties, index)))
        return D2D1_PROPERTY_TYPE_UNKNOWN;

    return prop->type;
}

static UINT32 STDMETHODCALLTYPE d2d_effect_properties_GetPropertyIndex(ID2D1Properties *iface,
        const WCHAR *name)
{
    struct d2d_effect_properties *properties = impl_from_ID2D1Properties(iface);
    struct d2d_effect_property *prop;

    TRACE("iface %p, name %s.\n", iface, debugstr_w(name));

    if (!(prop = d2d_effect_properties_get_property_by_name(properties, name)))
        return D2D1_INVALID_PROPERTY_INDEX;

    return prop->index;
}

static HRESULT STDMETHODCALLTYPE d2d_effect_properties_SetValueByName(ID2D1Properties *iface,
        const WCHAR *name, D2D1_PROPERTY_TYPE type, const BYTE *value, UINT32 value_size)
{
    struct d2d_effect_properties *properties = impl_from_ID2D1Properties(iface);
    struct d2d_effect_property *prop;

    TRACE("iface %p, name %s, type %u, value %p, value_size %u.\n", iface, debugstr_w(name),
            type, value, value_size);

    if (!(prop = d2d_effect_properties_get_property_by_name(properties, name)))
        return D2DERR_INVALID_PROPERTY;

    return d2d_effect_property_set_value(properties, prop, type, value, value_size);
}

static HRESULT STDMETHODCALLTYPE d2d_effect_properties_SetValue(ID2D1Properties *iface,
        UINT32 index, D2D1_PROPERTY_TYPE type, const BYTE *value, UINT32 value_size)
{
    struct d2d_effect_properties *properties = impl_from_ID2D1Properties(iface);
    struct d2d_effect_property *prop;

    TRACE("iface %p, index %#x, type %u, value %p, value_size %u.\n", iface, index, type, value, value_size);

    if (!(prop = d2d_effect_properties_get_property_by_index(properties, index)))
        return D2DERR_INVALID_PROPERTY;

    return d2d_effect_property_set_value(properties, prop, type, value, value_size);
}

static HRESULT STDMETHODCALLTYPE d2d_effect_properties_GetValueByName(ID2D1Properties *iface,
        const WCHAR *name, D2D1_PROPERTY_TYPE type, BYTE *value, UINT32 value_size)
{
    struct d2d_effect_properties *properties = impl_from_ID2D1Properties(iface);
    struct d2d_effect_property *prop;

    TRACE("iface %p, name %s, type %#x, value %p, value_size %u.\n", iface, debugstr_w(name), type,
            value, value_size);

    if (!(prop = d2d_effect_properties_get_property_by_name(properties, name)))
        return D2DERR_INVALID_PROPERTY;

    return d2d_effect_property_get_value(properties, prop, type, value, value_size);
}

static HRESULT STDMETHODCALLTYPE d2d_effect_properties_GetValue(ID2D1Properties *iface,
        UINT32 index, D2D1_PROPERTY_TYPE type, BYTE *value, UINT32 value_size)
{
    struct d2d_effect_properties *properties = impl_from_ID2D1Properties(iface);
    struct d2d_effect_property *prop;

    TRACE("iface %p, index %#x, type %u, value %p, value_size %u.\n", iface, index, type, value, value_size);

    if (!(prop = d2d_effect_properties_get_property_by_index(properties, index)))
        return D2DERR_INVALID_PROPERTY;

    return d2d_effect_property_get_value(properties, prop, type, value, value_size);
}

static UINT32 STDMETHODCALLTYPE d2d_effect_properties_GetValueSize(ID2D1Properties *iface,
        UINT32 index)
{
    struct d2d_effect_properties *properties = impl_from_ID2D1Properties(iface);

    TRACE("iface %p, index %#x.\n", iface, index);

    return d2d_effect_properties_get_value_size(properties, index);
}

static HRESULT STDMETHODCALLTYPE d2d_effect_properties_GetSubProperties(ID2D1Properties *iface,
        UINT32 index, ID2D1Properties **props)
{
    struct d2d_effect_properties *properties = impl_from_ID2D1Properties(iface);
    struct d2d_effect_property *prop;

    TRACE("iface %p, index %u, props %p.\n", iface, index, props);

    if (!(prop = d2d_effect_properties_get_property_by_index(properties, index)))
        return D2DERR_INVALID_PROPERTY;

    if (!prop->subproperties) return D2DERR_NO_SUBPROPERTIES;

    *props = &prop->subproperties->ID2D1Properties_iface;
    ID2D1Properties_AddRef(*props);
    return S_OK;
}

static const ID2D1PropertiesVtbl d2d_effect_properties_vtbl =
{
    d2d_effect_properties_QueryInterface,
    d2d_effect_properties_AddRef,
    d2d_effect_properties_Release,
    d2d_effect_properties_GetPropertyCount,
    d2d_effect_properties_GetPropertyName,
    d2d_effect_properties_GetPropertyNameLength,
    d2d_effect_properties_GetType,
    d2d_effect_properties_GetPropertyIndex,
    d2d_effect_properties_SetValueByName,
    d2d_effect_properties_SetValue,
    d2d_effect_properties_GetValueByName,
    d2d_effect_properties_GetValue,
    d2d_effect_properties_GetValueSize,
    d2d_effect_properties_GetSubProperties,
};

static void d2d_effect_init_properties_vtbls(struct d2d_effect *effect)
{
    unsigned int i;

    effect->properties.ID2D1Properties_iface.lpVtbl = &d2d_effect_properties_vtbl;
    effect->properties.effect = effect;

    for (i = 0; i < effect->properties.count; ++i)
    {
        struct d2d_effect_property *prop = &effect->properties.properties[i];
        if (!prop->subproperties) continue;
        prop->subproperties->ID2D1Properties_iface.lpVtbl = &d2d_effect_properties_vtbl;
        prop->subproperties->effect = effect;
    }
}

HRESULT d2d_effect_create(struct d2d_device_context *context, const CLSID *effect_id,
        ID2D1Effect **effect)
{
    struct d2d_effect_context *effect_context;
    const struct d2d_effect_registration *reg;
    struct d2d_transform_graph *graph;
    struct d2d_effect *object;
    WCHAR clsidW[39];
    HRESULT hr;

    if (!(reg = d2d_factory_get_registered_effect(context->factory, effect_id)))
    {
        WARN("Effect id %s not found.\n", wine_dbgstr_guid(effect_id));
        return D2DERR_EFFECT_IS_NOT_REGISTERED;
    }

    if (!(effect_context = calloc(1, sizeof(*effect_context))))
        return E_OUTOFMEMORY;
    d2d_effect_context_init(effect_context, context);

    if (!(graph = calloc(1, sizeof(*graph))))
    {
        ID2D1EffectContext_Release(&effect_context->ID2D1EffectContext_iface);
        return E_OUTOFMEMORY;
    }
    d2d_transform_graph_init(graph);

    if (!(object = calloc(1, sizeof(*object))))
    {
        ID2D1TransformGraph_Release(&graph->ID2D1TransformGraph_iface);
        ID2D1EffectContext_Release(&effect_context->ID2D1EffectContext_iface);
        return E_OUTOFMEMORY;
    }

    object->ID2D1Effect_iface.lpVtbl = &d2d_effect_vtbl;
    object->ID2D1Image_iface.lpVtbl = &d2d_effect_image_vtbl;
    object->refcount = 1;
    object->effect_context = effect_context;
    object->graph = graph;

    /* Create properties */
    d2d_effect_duplicate_properties(&object->properties, &reg->properties);

    StringFromGUID2(effect_id, clsidW, ARRAY_SIZE(clsidW));
    d2d_effect_properties_add(&object->properties, L"CLSID", D2D1_PROPERTY_CLSID, D2D1_PROPERTY_TYPE_CLSID, clsidW);
    d2d_effect_properties_add(&object->properties, L"Cached", D2D1_PROPERTY_CACHED, D2D1_PROPERTY_TYPE_BOOL, L"false");
    d2d_effect_properties_add(&object->properties, L"Precision", D2D1_PROPERTY_PRECISION, D2D1_PROPERTY_TYPE_ENUM, L"0");
    d2d_effect_init_properties_vtbls(object);

    d2d_effect_SetInputCount(&object->ID2D1Effect_iface, reg->default_input_count);

    if (FAILED(hr = reg->factory((IUnknown **)&object->impl)))
    {
        WARN("Failed to create implementation object, hr %#lx.\n", hr);
        ID2D1Effect_Release(&object->ID2D1Effect_iface);
        return hr;
    }

    if (FAILED(hr = ID2D1EffectImpl_Initialize(object->impl, &effect_context->ID2D1EffectContext_iface,
            &graph->ID2D1TransformGraph_iface)))
    {
        WARN("Failed to initialize effect, hr %#lx.\n", hr);
        ID2D1Effect_Release(&object->ID2D1Effect_iface);
        return hr;
    }

    *effect = &object->ID2D1Effect_iface;

    TRACE("Created effect %p.\n", *effect);

    return S_OK;
}
