/*
 * Copyright 2008-2009 Henri Verbeet for CodeWeavers
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

#ifndef __WINE_D3D10_PRIVATE_H
#define __WINE_D3D10_PRIVATE_H

#include <math.h>

#include "wine/debug.h"
#include "wine/rbtree.h"
#include "wine/heap.h"

#define COBJMACROS
#include "winbase.h"
#include "winuser.h"
#include "objbase.h"

#include "d3d10.h"
#include "d3dcompiler.h"

/*
 * This doesn't belong here, but for some functions it is possible to return that value,
 * see http://msdn.microsoft.com/en-us/library/bb205278%28v=VS.85%29.aspx
 * The original definition is in D3DX10core.h.
 */
#define D3DERR_INVALIDCALL 0x8876086c

enum d3d10_effect_object_type
{
    D3D10_EOT_RASTERIZER_STATE = 0x0,
    D3D10_EOT_DEPTH_STENCIL_STATE = 0x1,
    D3D10_EOT_BLEND_STATE = 0x2,
    D3D10_EOT_VERTEXSHADER = 0x6,
    D3D10_EOT_PIXELSHADER = 0x7,
    D3D10_EOT_GEOMETRYSHADER = 0x8,
    D3D10_EOT_STENCIL_REF = 0x9,
    D3D10_EOT_BLEND_FACTOR = 0xa,
    D3D10_EOT_SAMPLE_MASK = 0xb,
};

enum d3d10_effect_object_operation
{
    D3D10_EOO_VALUE = 1,
    D3D10_EOO_PARSED_OBJECT = 2,
    D3D10_EOO_PARSED_OBJECT_INDEX = 3,
    D3D10_EOO_ANONYMOUS_SHADER = 7,
};

struct d3d10_matrix
{
    float m[4][4];
};

struct d3d10_effect_object
{
    struct d3d10_effect_pass *pass;
    enum d3d10_effect_object_type type;
    union
    {
        ID3D10RasterizerState *rs;
        ID3D10DepthStencilState *ds;
        ID3D10BlendState *bs;
        ID3D10VertexShader *vs;
        ID3D10PixelShader *ps;
        ID3D10GeometryShader *gs;
    } object;
};

struct d3d10_effect_shader_resource
{
    D3D10_SHADER_INPUT_TYPE in_type;
    unsigned int bind_point;
    unsigned int bind_count;

    struct d3d10_effect_variable *variable;
};

struct d3d10_effect_shader_signature
{
    char *signature;
    UINT signature_size;
    UINT element_count;
    D3D10_SIGNATURE_PARAMETER_DESC *elements;
};

struct d3d10_effect_shader_variable
{
    struct d3d10_effect_shader_signature input_signature;
    struct d3d10_effect_shader_signature output_signature;
    union
    {
        ID3D10VertexShader *vs;
        ID3D10PixelShader *ps;
        ID3D10GeometryShader *gs;
    } shader;

    unsigned int resource_count;
    struct d3d10_effect_shader_resource *resources;
};

struct d3d10_effect_state_object_variable
{
    union
    {
        D3D10_RASTERIZER_DESC rasterizer;
        D3D10_DEPTH_STENCIL_DESC depth_stencil;
        D3D10_BLEND_DESC blend;
        D3D10_SAMPLER_DESC sampler;
    } desc;
    union
    {
        ID3D10RasterizerState *rasterizer;
        ID3D10DepthStencilState *depth_stencil;
        ID3D10BlendState *blend;
        ID3D10SamplerState *sampler;
    } object;
};

struct d3d10_effect_resource_variable
{
    ID3D10ShaderResourceView **srv;
    BOOL parent;
};

struct d3d10_effect_buffer_variable
{
    ID3D10Buffer *buffer;
    ID3D10ShaderResourceView *resource_view;

    BOOL changed;
    BYTE *local_buffer;
};

/* ID3D10EffectType */
struct d3d10_effect_type
{
    ID3D10EffectType ID3D10EffectType_iface;

    char *name;
    D3D10_SHADER_VARIABLE_TYPE basetype;
    D3D10_SHADER_VARIABLE_CLASS type_class;

    DWORD id;
    struct wine_rb_entry entry;
    struct d3d10_effect *effect;

    DWORD element_count;
    DWORD size_unpacked;
    DWORD stride;
    DWORD size_packed;
    DWORD member_count;
    DWORD column_count;
    DWORD row_count;
    struct d3d10_effect_type *elementtype;
    struct d3d10_effect_type_member *members;
};

struct d3d10_effect_type_member
{
    char *name;
    char *semantic;
    DWORD buffer_offset;
    struct d3d10_effect_type *type;
};

/* ID3D10EffectVariable */
struct d3d10_effect_variable
{
    ID3D10EffectVariable ID3D10EffectVariable_iface;

    struct d3d10_effect_variable *buffer;
    struct d3d10_effect_type *type;

    char *name;
    char *semantic;
    DWORD buffer_offset;
    DWORD annotation_count;
    DWORD flag;
    DWORD data_size;
    struct d3d10_effect *effect;
    struct d3d10_effect_variable *elements;
    struct d3d10_effect_variable *members;
    struct d3d10_effect_variable *annotations;

    union
    {
        struct d3d10_effect_state_object_variable state;
        struct d3d10_effect_shader_variable shader;
        struct d3d10_effect_buffer_variable buffer;
        struct d3d10_effect_resource_variable resource;
    } u;
};

/* ID3D10EffectPass */
struct d3d10_effect_pass
{
    ID3D10EffectPass ID3D10EffectPass_iface;

    struct d3d10_effect_technique *technique;
    char *name;
    DWORD start;
    DWORD object_count;
    DWORD annotation_count;
    struct d3d10_effect_object *objects;
    struct d3d10_effect_variable *annotations;

    D3D10_PASS_SHADER_DESC vs;
    D3D10_PASS_SHADER_DESC ps;
    D3D10_PASS_SHADER_DESC gs;
    UINT stencil_ref;
    UINT sample_mask;
    float blend_factor[4];
};

/* ID3D10EffectTechnique */
struct d3d10_effect_technique
{
    ID3D10EffectTechnique ID3D10EffectTechnique_iface;

    struct d3d10_effect *effect;
    char *name;
    DWORD pass_count;
    DWORD annotation_count;
    struct d3d10_effect_pass *passes;
    struct d3d10_effect_variable *annotations;
};

struct d3d10_effect_anonymous_shader
{
    struct d3d10_effect_variable shader;
    struct d3d10_effect_type type;
};

/* ID3D10Effect */
extern const struct ID3D10EffectVtbl d3d10_effect_vtbl DECLSPEC_HIDDEN;
struct d3d10_effect
{
    ID3D10Effect ID3D10Effect_iface;
    LONG refcount;

    ID3D10Device *device;
    DWORD version;
    DWORD local_buffer_count;
    DWORD variable_count;
    DWORD local_variable_count;
    DWORD sharedbuffers_count;
    DWORD sharedobjects_count;
    DWORD technique_count;
    DWORD index_offset;
    DWORD texture_count;
    DWORD depthstencilstate_count;
    DWORD blendstate_count;
    DWORD rasterizerstate_count;
    DWORD samplerstate_count;
    DWORD rendertargetview_count;
    DWORD depthstencilview_count;
    DWORD used_shader_count;
    DWORD anonymous_shader_count;

    DWORD used_shader_current;
    DWORD anonymous_shader_current;

    struct wine_rb_tree types;
    struct d3d10_effect_variable *local_buffers;
    struct d3d10_effect_variable *local_variables;
    struct d3d10_effect_anonymous_shader *anonymous_shaders;
    struct d3d10_effect_variable **used_shaders;
    struct d3d10_effect_technique *techniques;
};

HRESULT d3d10_effect_parse(struct d3d10_effect *This, const void *data, SIZE_T data_size) DECLSPEC_HIDDEN;

/* D3D10Core */
HRESULT WINAPI D3D10CoreCreateDevice(IDXGIFactory *factory, IDXGIAdapter *adapter,
        unsigned int flags, D3D_FEATURE_LEVEL feature_level, ID3D10Device **device);

#endif /* __WINE_D3D10_PRIVATE_H */
