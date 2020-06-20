/*
 * Copyright 2002-2005 Jason Edmeades
 * Copyright 2002-2005 Raphael Junqueira
 * Copyright 2005 Oliver Stieber
 * Copyright 2007-2009, 2013 Stefan Dösinger for CodeWeavers
 * Copyright 2009-2011 Henri Verbeet for CodeWeavers
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

#include "config.h"
#include "wine/port.h"
#include "wined3d_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(d3d);
WINE_DECLARE_DEBUG_CHANNEL(d3d_perf);
WINE_DECLARE_DEBUG_CHANNEL(winediag);

#define WINED3D_TEXTURE_DYNAMIC_MAP_THRESHOLD 50

static const uint32_t wined3d_texture_sysmem_locations = WINED3D_LOCATION_SYSMEM | WINED3D_LOCATION_BUFFER;
static const struct wined3d_texture_ops texture_gl_ops;

struct wined3d_texture_idx
{
    struct wined3d_texture *texture;
    unsigned int sub_resource_idx;
};

struct wined3d_rect_f
{
    float l;
    float t;
    float r;
    float b;
};

static BOOL wined3d_texture_use_pbo(const struct wined3d_texture *texture, const struct wined3d_gl_info *gl_info)
{
    if (!gl_info->supported[ARB_PIXEL_BUFFER_OBJECT]
            || texture->resource.format->conv_byte_count
            || (texture->flags & (WINED3D_TEXTURE_PIN_SYSMEM | WINED3D_TEXTURE_COND_NP2_EMULATED)))
        return FALSE;

    /* Use a PBO for dynamic textures and read-only staging textures. */
    return (!(texture->resource.access & WINED3D_RESOURCE_ACCESS_CPU)
                && texture->resource.usage & WINED3DUSAGE_DYNAMIC)
            || texture->resource.access == (WINED3D_RESOURCE_ACCESS_CPU | WINED3D_RESOURCE_ACCESS_MAP_R);
}

static BOOL wined3d_texture_use_immutable_storage(const struct wined3d_texture *texture,
        const struct wined3d_gl_info *gl_info)
{
    /* We don't expect to create texture views for textures with height-scaled formats.
     * Besides, ARB_texture_storage doesn't allow specifying exact sizes for all levels. */
    return gl_info->supported[ARB_TEXTURE_STORAGE]
            && !(texture->resource.format_flags & WINED3DFMT_FLAG_HEIGHT_SCALE);
}

/* Front buffer coordinates are always full screen coordinates, but our GL
 * drawable is limited to the window's client area. The sysmem and texture
 * copies do have the full screen size. Note that GL has a bottom-left
 * origin, while D3D has a top-left origin. */
void wined3d_texture_translate_drawable_coords(const struct wined3d_texture *texture, HWND window, RECT *rect)
{
    unsigned int drawable_height;
    POINT offset = {0, 0};
    RECT windowsize;

    if (!texture->swapchain)
        return;

    if (texture == texture->swapchain->front_buffer)
    {
        ScreenToClient(window, &offset);
        OffsetRect(rect, offset.x, offset.y);
    }

    GetClientRect(window, &windowsize);
    drawable_height = windowsize.bottom - windowsize.top;

    rect->top = drawable_height - rect->top;
    rect->bottom = drawable_height - rect->bottom;
}

GLenum wined3d_texture_get_gl_buffer(const struct wined3d_texture *texture)
{
    const struct wined3d_swapchain *swapchain = texture->swapchain;

    TRACE("texture %p.\n", texture);

    if (!swapchain)
    {
        ERR("Texture %p is not part of a swapchain.\n", texture);
        return GL_NONE;
    }

    if (texture == swapchain->front_buffer)
    {
        TRACE("Returning GL_FRONT.\n");
        return GL_FRONT;
    }

    if (texture == swapchain->back_buffers[0])
    {
        TRACE("Returning GL_BACK.\n");
        return GL_BACK;
    }

    FIXME("Higher back buffer, returning GL_BACK.\n");
    return GL_BACK;
}

static DWORD wined3d_resource_access_from_location(DWORD location)
{
    switch (location)
    {
        case WINED3D_LOCATION_DISCARDED:
            return 0;

        case WINED3D_LOCATION_SYSMEM:
            return WINED3D_RESOURCE_ACCESS_CPU;

        case WINED3D_LOCATION_BUFFER:
        case WINED3D_LOCATION_DRAWABLE:
        case WINED3D_LOCATION_TEXTURE_RGB:
        case WINED3D_LOCATION_TEXTURE_SRGB:
        case WINED3D_LOCATION_RB_MULTISAMPLE:
        case WINED3D_LOCATION_RB_RESOLVED:
            return WINED3D_RESOURCE_ACCESS_GPU;

        default:
            FIXME("Unhandled location %#x.\n", location);
            return 0;
    }
}

static inline void cube_coords_float(const RECT *r, UINT w, UINT h, struct wined3d_rect_f *f)
{
    f->l = ((r->left * 2.0f) / w) - 1.0f;
    f->t = ((r->top * 2.0f) / h) - 1.0f;
    f->r = ((r->right * 2.0f) / w) - 1.0f;
    f->b = ((r->bottom * 2.0f) / h) - 1.0f;
}

void texture2d_get_blt_info(const struct wined3d_texture_gl *texture_gl,
        unsigned int sub_resource_idx, const RECT *rect, struct wined3d_blt_info *info)
{
    struct wined3d_vec3 *coords = info->texcoords;
    struct wined3d_rect_f f;
    unsigned int level;
    GLenum target;
    GLsizei w, h;

    level = sub_resource_idx % texture_gl->t.level_count;
    w = wined3d_texture_get_level_pow2_width(&texture_gl->t, level);
    h = wined3d_texture_get_level_pow2_height(&texture_gl->t, level);
    target = wined3d_texture_gl_get_sub_resource_target(texture_gl, sub_resource_idx);

    switch (target)
    {
        default:
            FIXME("Unsupported texture target %#x.\n", target);
            /* Fall back to GL_TEXTURE_2D */
        case GL_TEXTURE_2D:
            info->bind_target = GL_TEXTURE_2D;
            coords[0].x = (float)rect->left / w;
            coords[0].y = (float)rect->top / h;
            coords[0].z = 0.0f;

            coords[1].x = (float)rect->right / w;
            coords[1].y = (float)rect->top / h;
            coords[1].z = 0.0f;

            coords[2].x = (float)rect->left / w;
            coords[2].y = (float)rect->bottom / h;
            coords[2].z = 0.0f;

            coords[3].x = (float)rect->right / w;
            coords[3].y = (float)rect->bottom / h;
            coords[3].z = 0.0f;
            break;

        case GL_TEXTURE_RECTANGLE_ARB:
            info->bind_target = GL_TEXTURE_RECTANGLE_ARB;
            coords[0].x = rect->left;  coords[0].y = rect->top;    coords[0].z = 0.0f;
            coords[1].x = rect->right; coords[1].y = rect->top;    coords[1].z = 0.0f;
            coords[2].x = rect->left;  coords[2].y = rect->bottom; coords[2].z = 0.0f;
            coords[3].x = rect->right; coords[3].y = rect->bottom; coords[3].z = 0.0f;
            break;

        case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
            info->bind_target = GL_TEXTURE_CUBE_MAP_ARB;
            cube_coords_float(rect, w, h, &f);

            coords[0].x =  1.0f;   coords[0].y = -f.t;   coords[0].z = -f.l;
            coords[1].x =  1.0f;   coords[1].y = -f.t;   coords[1].z = -f.r;
            coords[2].x =  1.0f;   coords[2].y = -f.b;   coords[2].z = -f.l;
            coords[3].x =  1.0f;   coords[3].y = -f.b;   coords[3].z = -f.r;
            break;

        case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
            info->bind_target = GL_TEXTURE_CUBE_MAP_ARB;
            cube_coords_float(rect, w, h, &f);

            coords[0].x = -1.0f;   coords[0].y = -f.t;   coords[0].z = f.l;
            coords[1].x = -1.0f;   coords[1].y = -f.t;   coords[1].z = f.r;
            coords[2].x = -1.0f;   coords[2].y = -f.b;   coords[2].z = f.l;
            coords[3].x = -1.0f;   coords[3].y = -f.b;   coords[3].z = f.r;
            break;

        case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
            info->bind_target = GL_TEXTURE_CUBE_MAP_ARB;
            cube_coords_float(rect, w, h, &f);

            coords[0].x = f.l;   coords[0].y =  1.0f;   coords[0].z = f.t;
            coords[1].x = f.r;   coords[1].y =  1.0f;   coords[1].z = f.t;
            coords[2].x = f.l;   coords[2].y =  1.0f;   coords[2].z = f.b;
            coords[3].x = f.r;   coords[3].y =  1.0f;   coords[3].z = f.b;
            break;

        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
            info->bind_target = GL_TEXTURE_CUBE_MAP_ARB;
            cube_coords_float(rect, w, h, &f);

            coords[0].x = f.l;   coords[0].y = -1.0f;   coords[0].z = -f.t;
            coords[1].x = f.r;   coords[1].y = -1.0f;   coords[1].z = -f.t;
            coords[2].x = f.l;   coords[2].y = -1.0f;   coords[2].z = -f.b;
            coords[3].x = f.r;   coords[3].y = -1.0f;   coords[3].z = -f.b;
            break;

        case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
            info->bind_target = GL_TEXTURE_CUBE_MAP_ARB;
            cube_coords_float(rect, w, h, &f);

            coords[0].x = f.l;   coords[0].y = -f.t;   coords[0].z =  1.0f;
            coords[1].x = f.r;   coords[1].y = -f.t;   coords[1].z =  1.0f;
            coords[2].x = f.l;   coords[2].y = -f.b;   coords[2].z =  1.0f;
            coords[3].x = f.r;   coords[3].y = -f.b;   coords[3].z =  1.0f;
            break;

        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
            info->bind_target = GL_TEXTURE_CUBE_MAP_ARB;
            cube_coords_float(rect, w, h, &f);

            coords[0].x = -f.l;   coords[0].y = -f.t;   coords[0].z = -1.0f;
            coords[1].x = -f.r;   coords[1].y = -f.t;   coords[1].z = -1.0f;
            coords[2].x = -f.l;   coords[2].y = -f.b;   coords[2].z = -1.0f;
            coords[3].x = -f.r;   coords[3].y = -f.b;   coords[3].z = -1.0f;
            break;
    }
}

static bool fbo_blitter_supported(enum wined3d_blit_op blit_op, const struct wined3d_gl_info *gl_info,
        const struct wined3d_resource *src_resource, DWORD src_location,
        const struct wined3d_resource *dst_resource, DWORD dst_location)
{
    const struct wined3d_format *src_format = src_resource->format;
    const struct wined3d_format *dst_format = dst_resource->format;

    if ((wined3d_settings.offscreen_rendering_mode != ORM_FBO) || !gl_info->fbo_ops.glBlitFramebuffer)
        return false;

    /* Source and/or destination need to be on the GL side. */
    if (!(src_resource->access & dst_resource->access & WINED3D_RESOURCE_ACCESS_GPU))
        return false;

    if (src_resource->type != WINED3D_RTYPE_TEXTURE_2D)
        return false;

    switch (blit_op)
    {
        case WINED3D_BLIT_OP_COLOR_BLIT:
            if (!((src_format->flags[WINED3D_GL_RES_TYPE_TEX_2D] & WINED3DFMT_FLAG_FBO_ATTACHABLE)
                    || (src_resource->bind_flags & WINED3D_BIND_RENDER_TARGET)))
                return false;
            if (!((dst_format->flags[WINED3D_GL_RES_TYPE_TEX_2D] & WINED3DFMT_FLAG_FBO_ATTACHABLE)
                    || (dst_resource->bind_flags & WINED3D_BIND_RENDER_TARGET)))
                return false;
            if ((src_format->id != dst_format->id || dst_location == WINED3D_LOCATION_DRAWABLE)
                    && (!is_identity_fixup(src_format->color_fixup) || !is_identity_fixup(dst_format->color_fixup)))
                return false;
            break;

        case WINED3D_BLIT_OP_DEPTH_BLIT:
            if (!(src_format->flags[WINED3D_GL_RES_TYPE_TEX_2D] & WINED3DFMT_FLAG_DEPTH_STENCIL))
                return false;
            if (!(dst_format->flags[WINED3D_GL_RES_TYPE_TEX_2D] & WINED3DFMT_FLAG_DEPTH_STENCIL))
                return false;
            /* Accept pure swizzle fixups for depth formats. In general we
             * ignore the stencil component (if present) at the moment and the
             * swizzle is not relevant with just the depth component. */
            if (is_complex_fixup(src_format->color_fixup) || is_complex_fixup(dst_format->color_fixup)
                    || is_scaling_fixup(src_format->color_fixup) || is_scaling_fixup(dst_format->color_fixup))
                return false;
            break;

        default:
            return false;
    }

    return true;
}

/* Blit between surface locations. Onscreen on different swapchains is not supported.
 * Depth / stencil is not supported. Context activation is done by the caller. */
static void texture2d_blt_fbo(struct wined3d_device *device, struct wined3d_context *context,
        enum wined3d_texture_filter_type filter, struct wined3d_texture *src_texture,
        unsigned int src_sub_resource_idx, DWORD src_location, const RECT *src_rect,
        struct wined3d_texture *dst_texture, unsigned int dst_sub_resource_idx, DWORD dst_location,
        const RECT *dst_rect)
{
    struct wined3d_texture *required_texture, *restore_texture;
    const struct wined3d_gl_info *gl_info;
    struct wined3d_context_gl *context_gl;
    unsigned int restore_idx;
    bool scaled_resolve;
    GLenum gl_filter;
    GLenum buffer;
    RECT s, d;

    TRACE("device %p, context %p, filter %s, src_texture %p, src_sub_resource_idx %u, src_location %s, "
            "src_rect %s, dst_texture %p, dst_sub_resource_idx %u, dst_location %s, dst_rect %s.\n",
            device, context, debug_d3dtexturefiltertype(filter), src_texture, src_sub_resource_idx,
            wined3d_debug_location(src_location), wine_dbgstr_rect(src_rect), dst_texture,
            dst_sub_resource_idx, wined3d_debug_location(dst_location), wine_dbgstr_rect(dst_rect));

    scaled_resolve = wined3d_texture_gl_is_multisample_location(wined3d_texture_gl(src_texture), src_location)
            && (abs(src_rect->bottom - src_rect->top) != abs(dst_rect->bottom - dst_rect->top)
            || abs(src_rect->right - src_rect->left) != abs(dst_rect->right - dst_rect->left));

    if (filter == WINED3D_TEXF_LINEAR)
        gl_filter = scaled_resolve ? GL_SCALED_RESOLVE_NICEST_EXT : GL_LINEAR;
    else
        gl_filter = scaled_resolve ? GL_SCALED_RESOLVE_FASTEST_EXT : GL_NEAREST;

    /* Make sure the locations are up-to-date. Loading the destination
     * surface isn't required if the entire surface is overwritten. (And is
     * in fact harmful if we're being called by surface_load_location() with
     * the purpose of loading the destination surface.) */
    wined3d_texture_load_location(src_texture, src_sub_resource_idx, context, src_location);
    if (!wined3d_texture_is_full_rect(dst_texture, dst_sub_resource_idx % dst_texture->level_count, dst_rect))
        wined3d_texture_load_location(dst_texture, dst_sub_resource_idx, context, dst_location);
    else
        wined3d_texture_prepare_location(dst_texture, dst_sub_resource_idx, context, dst_location);

    /* Acquire a context for the front-buffer, even though we may be blitting
     * to/from a back-buffer. Since context_acquire() doesn't take the
     * resource location into account, it may consider the back-buffer to be
     * offscreen. */
    if (src_location == WINED3D_LOCATION_DRAWABLE)
        required_texture = src_texture->swapchain->front_buffer;
    else if (dst_location == WINED3D_LOCATION_DRAWABLE)
        required_texture = dst_texture->swapchain->front_buffer;
    else
        required_texture = NULL;

    restore_texture = context->current_rt.texture;
    restore_idx = context->current_rt.sub_resource_idx;
    if (restore_texture != required_texture)
        context = context_acquire(device, required_texture, 0);
    else
        restore_texture = NULL;

    context_gl = wined3d_context_gl(context);
    if (!context_gl->valid)
    {
        context_release(context);
        WARN("Invalid context, skipping blit.\n");
        return;
    }

    gl_info = context_gl->gl_info;

    if (src_location == WINED3D_LOCATION_DRAWABLE)
    {
        TRACE("Source texture %p is onscreen.\n", src_texture);
        buffer = wined3d_texture_get_gl_buffer(src_texture);
        s = *src_rect;
        wined3d_texture_translate_drawable_coords(src_texture, context_gl->window, &s);
        src_rect = &s;
    }
    else
    {
        TRACE("Source texture %p is offscreen.\n", src_texture);
        buffer = GL_COLOR_ATTACHMENT0;
    }

    wined3d_context_gl_apply_fbo_state_blit(context_gl, GL_READ_FRAMEBUFFER,
            &src_texture->resource, src_sub_resource_idx, NULL, 0, src_location);
    gl_info->gl_ops.gl.p_glReadBuffer(buffer);
    checkGLcall("glReadBuffer()");
    wined3d_context_gl_check_fbo_status(context_gl, GL_READ_FRAMEBUFFER);

    if (dst_location == WINED3D_LOCATION_DRAWABLE)
    {
        TRACE("Destination texture %p is onscreen.\n", dst_texture);
        buffer = wined3d_texture_get_gl_buffer(dst_texture);
        d = *dst_rect;
        wined3d_texture_translate_drawable_coords(dst_texture, context_gl->window, &d);
        dst_rect = &d;
    }
    else
    {
        TRACE("Destination texture %p is offscreen.\n", dst_texture);
        buffer = GL_COLOR_ATTACHMENT0;
    }

    wined3d_context_gl_apply_fbo_state_blit(context_gl, GL_DRAW_FRAMEBUFFER,
            &dst_texture->resource, dst_sub_resource_idx, NULL, 0, dst_location);
    wined3d_context_gl_set_draw_buffer(context_gl, buffer);
    wined3d_context_gl_check_fbo_status(context_gl, GL_DRAW_FRAMEBUFFER);
    context_invalidate_state(context, STATE_FRAMEBUFFER);

    gl_info->gl_ops.gl.p_glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    context_invalidate_state(context, STATE_BLEND);

    gl_info->gl_ops.gl.p_glDisable(GL_SCISSOR_TEST);
    context_invalidate_state(context, STATE_RASTERIZER);

    gl_info->fbo_ops.glBlitFramebuffer(src_rect->left, src_rect->top, src_rect->right, src_rect->bottom,
            dst_rect->left, dst_rect->top, dst_rect->right, dst_rect->bottom, GL_COLOR_BUFFER_BIT, gl_filter);
    checkGLcall("glBlitFramebuffer()");

    if (dst_location == WINED3D_LOCATION_DRAWABLE && dst_texture->swapchain->front_buffer == dst_texture)
        gl_info->gl_ops.gl.p_glFlush();

    if (restore_texture)
        context_restore(context, restore_texture, restore_idx);
}

static void texture2d_depth_blt_fbo(const struct wined3d_device *device, struct wined3d_context *context,
        struct wined3d_texture *src_texture, unsigned int src_sub_resource_idx, DWORD src_location,
        const RECT *src_rect, struct wined3d_texture *dst_texture, unsigned int dst_sub_resource_idx,
        DWORD dst_location, const RECT *dst_rect)
{
    struct wined3d_context_gl *context_gl = wined3d_context_gl(context);
    const struct wined3d_gl_info *gl_info = context_gl->gl_info;
    GLbitfield src_mask, dst_mask;
    GLbitfield gl_mask;

    TRACE("device %p, src_texture %p, src_sub_resource_idx %u, src_location %s, src_rect %s, "
            "dst_texture %p, dst_sub_resource_idx %u, dst_location %s, dst_rect %s.\n", device,
            src_texture, src_sub_resource_idx, wined3d_debug_location(src_location), wine_dbgstr_rect(src_rect),
            dst_texture, dst_sub_resource_idx, wined3d_debug_location(dst_location), wine_dbgstr_rect(dst_rect));

    src_mask = 0;
    if (src_texture->resource.format->depth_size)
        src_mask |= GL_DEPTH_BUFFER_BIT;
    if (src_texture->resource.format->stencil_size)
        src_mask |= GL_STENCIL_BUFFER_BIT;

    dst_mask = 0;
    if (dst_texture->resource.format->depth_size)
        dst_mask |= GL_DEPTH_BUFFER_BIT;
    if (dst_texture->resource.format->stencil_size)
        dst_mask |= GL_STENCIL_BUFFER_BIT;

    if (src_mask != dst_mask)
    {
        ERR("Incompatible formats %s and %s.\n",
                debug_d3dformat(src_texture->resource.format->id),
                debug_d3dformat(dst_texture->resource.format->id));
        return;
    }

    if (!src_mask)
    {
        ERR("Not a depth / stencil format: %s.\n",
                debug_d3dformat(src_texture->resource.format->id));
        return;
    }
    gl_mask = src_mask;

    /* Make sure the locations are up-to-date. Loading the destination
     * surface isn't required if the entire surface is overwritten. */
    wined3d_texture_load_location(src_texture, src_sub_resource_idx, context, src_location);
    if (!wined3d_texture_is_full_rect(dst_texture, dst_sub_resource_idx % dst_texture->level_count, dst_rect))
        wined3d_texture_load_location(dst_texture, dst_sub_resource_idx, context, dst_location);
    else
        wined3d_texture_prepare_location(dst_texture, dst_sub_resource_idx, context, dst_location);

    wined3d_context_gl_apply_fbo_state_blit(context_gl, GL_READ_FRAMEBUFFER, NULL, 0,
            &src_texture->resource, src_sub_resource_idx, src_location);
    wined3d_context_gl_check_fbo_status(context_gl, GL_READ_FRAMEBUFFER);

    wined3d_context_gl_apply_fbo_state_blit(context_gl, GL_DRAW_FRAMEBUFFER, NULL, 0,
            &dst_texture->resource, dst_sub_resource_idx, dst_location);
    wined3d_context_gl_set_draw_buffer(context_gl, GL_NONE);
    wined3d_context_gl_check_fbo_status(context_gl, GL_DRAW_FRAMEBUFFER);
    context_invalidate_state(context, STATE_FRAMEBUFFER);

    if (gl_mask & GL_DEPTH_BUFFER_BIT)
    {
        gl_info->gl_ops.gl.p_glDepthMask(GL_TRUE);
        context_invalidate_state(context, STATE_RENDER(WINED3D_RS_ZWRITEENABLE));
    }
    if (gl_mask & GL_STENCIL_BUFFER_BIT)
    {
        if (gl_info->supported[EXT_STENCIL_TWO_SIDE])
        {
            gl_info->gl_ops.gl.p_glDisable(GL_STENCIL_TEST_TWO_SIDE_EXT);
            context_invalidate_state(context, STATE_RENDER(WINED3D_RS_TWOSIDEDSTENCILMODE));
        }
        gl_info->gl_ops.gl.p_glStencilMask(~0U);
        context_invalidate_state(context, STATE_RENDER(WINED3D_RS_STENCILWRITEMASK));
    }

    gl_info->gl_ops.gl.p_glDisable(GL_SCISSOR_TEST);
    context_invalidate_state(context, STATE_RASTERIZER);

    gl_info->fbo_ops.glBlitFramebuffer(src_rect->left, src_rect->top, src_rect->right, src_rect->bottom,
            dst_rect->left, dst_rect->top, dst_rect->right, dst_rect->bottom, gl_mask, GL_NEAREST);
    checkGLcall("glBlitFramebuffer()");
}

static void wined3d_texture_evict_sysmem(struct wined3d_texture *texture)
{
    struct wined3d_texture_sub_resource *sub_resource;
    unsigned int i, sub_count;

    if (texture->flags & (WINED3D_TEXTURE_CONVERTED | WINED3D_TEXTURE_PIN_SYSMEM)
            || texture->download_count > WINED3D_TEXTURE_DYNAMIC_MAP_THRESHOLD)
    {
        TRACE("Not evicting system memory for texture %p.\n", texture);
        return;
    }

    TRACE("Evicting system memory for texture %p.\n", texture);

    sub_count = texture->level_count * texture->layer_count;
    for (i = 0; i < sub_count; ++i)
    {
        sub_resource = &texture->sub_resources[i];
        if (sub_resource->locations == WINED3D_LOCATION_SYSMEM)
            ERR("WINED3D_LOCATION_SYSMEM is the only location for sub-resource %u of texture %p.\n",
                    i, texture);
        sub_resource->locations &= ~WINED3D_LOCATION_SYSMEM;
    }
    wined3d_resource_free_sysmem(&texture->resource);
}

void wined3d_texture_validate_location(struct wined3d_texture *texture,
        unsigned int sub_resource_idx, DWORD location)
{
    struct wined3d_texture_sub_resource *sub_resource;
    DWORD previous_locations;

    TRACE("texture %p, sub_resource_idx %u, location %s.\n",
            texture, sub_resource_idx, wined3d_debug_location(location));

    sub_resource = &texture->sub_resources[sub_resource_idx];
    previous_locations = sub_resource->locations;
    sub_resource->locations |= location;
    if (previous_locations == WINED3D_LOCATION_SYSMEM && location != WINED3D_LOCATION_SYSMEM
            && !--texture->sysmem_count)
        wined3d_texture_evict_sysmem(texture);

    TRACE("New locations flags are %s.\n", wined3d_debug_location(sub_resource->locations));
}

static void wined3d_texture_set_dirty(struct wined3d_texture *texture)
{
    texture->flags &= ~(WINED3D_TEXTURE_RGB_VALID | WINED3D_TEXTURE_SRGB_VALID);
}

void wined3d_texture_invalidate_location(struct wined3d_texture *texture,
        unsigned int sub_resource_idx, DWORD location)
{
    struct wined3d_texture_sub_resource *sub_resource;
    DWORD previous_locations;

    TRACE("texture %p, sub_resource_idx %u, location %s.\n",
            texture, sub_resource_idx, wined3d_debug_location(location));

    if (location & (WINED3D_LOCATION_TEXTURE_RGB | WINED3D_LOCATION_TEXTURE_SRGB))
        wined3d_texture_set_dirty(texture);

    sub_resource = &texture->sub_resources[sub_resource_idx];
    previous_locations = sub_resource->locations;
    sub_resource->locations &= ~location;
    if (previous_locations != WINED3D_LOCATION_SYSMEM && sub_resource->locations == WINED3D_LOCATION_SYSMEM)
        ++texture->sysmem_count;

    TRACE("New locations flags are %s.\n", wined3d_debug_location(sub_resource->locations));

    if (!sub_resource->locations)
        ERR("Sub-resource %u of texture %p does not have any up to date location.\n",
                sub_resource_idx, texture);
}

void wined3d_texture_clear_dirty_regions(struct wined3d_texture *texture)
{
    unsigned int i;

    TRACE("texture %p\n", texture);

    if (!texture->dirty_regions)
        return;

    for (i = 0; i < texture->layer_count; ++i)
    {
        texture->dirty_regions[i].box_count = 0;
    }
}

static BOOL wined3d_texture_copy_sysmem_location(struct wined3d_texture *texture,
        unsigned int sub_resource_idx, struct wined3d_context *context, DWORD location)
{
    unsigned int size = texture->sub_resources[sub_resource_idx].size;
    struct wined3d_device *device = texture->resource.device;
    const struct wined3d_gl_info *gl_info;
    struct wined3d_bo_gl *src_bo, *dst_bo;
    struct wined3d_bo_address dst, src;

    if (!wined3d_texture_prepare_location(texture, sub_resource_idx, context, location))
        return FALSE;

    wined3d_texture_get_memory(texture, sub_resource_idx, &dst, location);
    wined3d_texture_get_memory(texture, sub_resource_idx, &src,
            texture->sub_resources[sub_resource_idx].locations);

    if ((dst_bo = (struct wined3d_bo_gl *)dst.buffer_object))
    {
        context = context_acquire(device, NULL, 0);
        gl_info = wined3d_context_gl(context)->gl_info;
        GL_EXTCALL(glBindBuffer(GL_PIXEL_UNPACK_BUFFER, dst_bo->id));
        GL_EXTCALL(glBufferSubData(GL_PIXEL_UNPACK_BUFFER, 0, size, src.addr));
        GL_EXTCALL(glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0));
        checkGLcall("PBO upload");
        context_release(context);
        return TRUE;
    }

    if ((src_bo = (struct wined3d_bo_gl *)src.buffer_object))
    {
        context = context_acquire(device, NULL, 0);
        gl_info = wined3d_context_gl(context)->gl_info;
        GL_EXTCALL(glBindBuffer(GL_PIXEL_PACK_BUFFER, src_bo->id));
        GL_EXTCALL(glGetBufferSubData(GL_PIXEL_PACK_BUFFER, 0, size, dst.addr));
        GL_EXTCALL(glBindBuffer(GL_PIXEL_PACK_BUFFER, 0));
        checkGLcall("PBO download");
        context_release(context);
        return TRUE;
    }

    memcpy(dst.addr, src.addr, size);
    return TRUE;
}

/* Context activation is done by the caller. Context may be NULL in
 * WINED3D_NO3D mode. */
BOOL wined3d_texture_load_location(struct wined3d_texture *texture,
        unsigned int sub_resource_idx, struct wined3d_context *context, DWORD location)
{
    DWORD current = texture->sub_resources[sub_resource_idx].locations;
    BOOL ret;

    TRACE("texture %p, sub_resource_idx %u, context %p, location %s.\n",
            texture, sub_resource_idx, context, wined3d_debug_location(location));

    TRACE("Current resource location %s.\n", wined3d_debug_location(current));

    if (current & location)
    {
        TRACE("Location %s is already up to date.\n", wined3d_debug_location(location));
        return TRUE;
    }

    if (WARN_ON(d3d))
    {
        DWORD required_access = wined3d_resource_access_from_location(location);
        if ((texture->resource.access & required_access) != required_access)
            WARN("Operation requires %#x access, but texture only has %#x.\n",
                    required_access, texture->resource.access);
    }

    if (current & WINED3D_LOCATION_DISCARDED)
    {
        TRACE("Sub-resource previously discarded, nothing to do.\n");
        if (!wined3d_texture_prepare_location(texture, sub_resource_idx, context, location))
            return FALSE;
        wined3d_texture_validate_location(texture, sub_resource_idx, location);
        wined3d_texture_invalidate_location(texture, sub_resource_idx, WINED3D_LOCATION_DISCARDED);
        return TRUE;
    }

    if (!current)
    {
        ERR("Sub-resource %u of texture %p does not have any up to date location.\n",
                sub_resource_idx, texture);
        wined3d_texture_validate_location(texture, sub_resource_idx, WINED3D_LOCATION_DISCARDED);
        return wined3d_texture_load_location(texture, sub_resource_idx, context, location);
    }

    if ((location & wined3d_texture_sysmem_locations) && (current & wined3d_texture_sysmem_locations))
        ret = wined3d_texture_copy_sysmem_location(texture, sub_resource_idx, context, location);
    else
        ret = texture->texture_ops->texture_load_location(texture, sub_resource_idx, context, location);

    if (ret)
        wined3d_texture_validate_location(texture, sub_resource_idx, location);

    return ret;
}

void wined3d_texture_get_memory(struct wined3d_texture *texture, unsigned int sub_resource_idx,
        struct wined3d_bo_address *data, DWORD locations)
{
    struct wined3d_texture_sub_resource *sub_resource;

    TRACE("texture %p, sub_resource_idx %u, data %p, locations %s.\n",
            texture, sub_resource_idx, data, wined3d_debug_location(locations));

    sub_resource = &texture->sub_resources[sub_resource_idx];
    if (locations & WINED3D_LOCATION_BUFFER)
    {
        data->addr = NULL;
        data->buffer_object = (uintptr_t)&sub_resource->bo;
        return;
    }

    if (locations & WINED3D_LOCATION_SYSMEM)
    {
        if (texture->sub_resources[sub_resource_idx].user_memory)
        {
            data->addr = texture->sub_resources[sub_resource_idx].user_memory;
        }
        else
        {
            data->addr = texture->resource.heap_memory;
            data->addr += sub_resource->offset;
        }
        data->buffer_object = 0;
        return;
    }

    ERR("Unexpected locations %s.\n", wined3d_debug_location(locations));
    data->addr = NULL;
    data->buffer_object = 0;
}

/* Context activation is done by the caller. */
static void wined3d_texture_remove_buffer_object(struct wined3d_texture *texture,
        unsigned int sub_resource_idx, const struct wined3d_gl_info *gl_info)
{
    struct wined3d_bo_gl *bo = &texture->sub_resources[sub_resource_idx].bo;

    GL_EXTCALL(glDeleteBuffers(1, &bo->id));
    checkGLcall("glDeleteBuffers");

    TRACE("Deleted buffer object %u for texture %p, sub-resource %u.\n",
            bo->id, texture, sub_resource_idx);

    wined3d_texture_invalidate_location(texture, sub_resource_idx, WINED3D_LOCATION_BUFFER);
    bo->id = 0;
}

static void wined3d_texture_update_map_binding(struct wined3d_texture *texture)
{
    unsigned int sub_count = texture->level_count * texture->layer_count;
    struct wined3d_device *device = texture->resource.device;
    DWORD map_binding = texture->update_map_binding;
    struct wined3d_context *context;
    unsigned int i;

    context = context_acquire(device, NULL, 0);

    for (i = 0; i < sub_count; ++i)
    {
        if (texture->sub_resources[i].locations == texture->resource.map_binding
                && !wined3d_texture_load_location(texture, i, context, map_binding))
            ERR("Failed to load location %s.\n", wined3d_debug_location(map_binding));
        if (texture->resource.map_binding == WINED3D_LOCATION_BUFFER)
            wined3d_texture_remove_buffer_object(texture, i, wined3d_context_gl(context)->gl_info);
    }

    context_release(context);

    texture->resource.map_binding = map_binding;
    texture->update_map_binding = 0;
}

void wined3d_texture_set_map_binding(struct wined3d_texture *texture, DWORD map_binding)
{
    texture->update_map_binding = map_binding;
    if (!texture->resource.map_count)
        wined3d_texture_update_map_binding(texture);
}

/* A GL context is provided by the caller */
static void gltexture_delete(struct wined3d_device *device, const struct wined3d_gl_info *gl_info,
        struct gl_texture *tex)
{
    context_gl_resource_released(device, tex->name, FALSE);
    gl_info->gl_ops.gl.p_glDeleteTextures(1, &tex->name);
    tex->name = 0;
}

/* Context activation is done by the caller. */
/* The caller is responsible for binding the correct texture. */
static void wined3d_texture_gl_allocate_mutable_storage(struct wined3d_texture_gl *texture_gl,
        GLenum gl_internal_format, const struct wined3d_format_gl *format,
        const struct wined3d_gl_info *gl_info)
{
    unsigned int level, level_count, layer, layer_count;
    GLsizei width, height, depth;
    GLenum target;

    level_count = texture_gl->t.level_count;
    if (texture_gl->target == GL_TEXTURE_1D_ARRAY || texture_gl->target == GL_TEXTURE_2D_ARRAY)
        layer_count = 1;
    else
        layer_count = texture_gl->t.layer_count;

    for (layer = 0; layer < layer_count; ++layer)
    {
        target = wined3d_texture_gl_get_sub_resource_target(texture_gl, layer * level_count);

        for (level = 0; level < level_count; ++level)
        {
            width = wined3d_texture_get_level_pow2_width(&texture_gl->t, level);
            height = wined3d_texture_get_level_pow2_height(&texture_gl->t, level);
            if (texture_gl->t.resource.format_flags & WINED3DFMT_FLAG_HEIGHT_SCALE)
            {
                height *= format->f.height_scale.numerator;
                height /= format->f.height_scale.denominator;
            }

            TRACE("texture_gl %p, layer %u, level %u, target %#x, width %u, height %u.\n",
                    texture_gl, layer, level, target, width, height);

            if (target == GL_TEXTURE_3D || target == GL_TEXTURE_2D_ARRAY)
            {
                depth = wined3d_texture_get_level_depth(&texture_gl->t, level);
                GL_EXTCALL(glTexImage3D(target, level, gl_internal_format, width, height,
                        target == GL_TEXTURE_2D_ARRAY ? texture_gl->t.layer_count : depth, 0,
                        format->format, format->type, NULL));
                checkGLcall("glTexImage3D");
            }
            else if (target == GL_TEXTURE_1D)
            {
                gl_info->gl_ops.gl.p_glTexImage1D(target, level, gl_internal_format,
                        width, 0, format->format, format->type, NULL);
            }
            else
            {
                gl_info->gl_ops.gl.p_glTexImage2D(target, level, gl_internal_format, width,
                        target == GL_TEXTURE_1D_ARRAY ? texture_gl->t.layer_count : height, 0,
                        format->format, format->type, NULL);
                checkGLcall("glTexImage2D");
            }
        }
    }
}

/* Context activation is done by the caller. */
/* The caller is responsible for binding the correct texture. */
static void wined3d_texture_gl_allocate_immutable_storage(struct wined3d_texture_gl *texture_gl,
        GLenum gl_internal_format, const struct wined3d_gl_info *gl_info)
{
    unsigned int samples = wined3d_resource_get_sample_count(&texture_gl->t.resource);
    GLsizei height = wined3d_texture_get_level_pow2_height(&texture_gl->t, 0);
    GLsizei width = wined3d_texture_get_level_pow2_width(&texture_gl->t, 0);
    GLboolean standard_pattern = texture_gl->t.resource.multisample_type != WINED3D_MULTISAMPLE_NON_MASKABLE
            && texture_gl->t.resource.multisample_quality == WINED3D_STANDARD_MULTISAMPLE_PATTERN;

    switch (texture_gl->target)
    {
        case GL_TEXTURE_3D:
            GL_EXTCALL(glTexStorage3D(texture_gl->target, texture_gl->t.level_count,
                    gl_internal_format, width, height, wined3d_texture_get_level_depth(&texture_gl->t, 0)));
            break;
        case GL_TEXTURE_2D_ARRAY:
            GL_EXTCALL(glTexStorage3D(texture_gl->target, texture_gl->t.level_count,
                    gl_internal_format, width, height, texture_gl->t.layer_count));
            break;
        case GL_TEXTURE_2D_MULTISAMPLE:
            GL_EXTCALL(glTexStorage2DMultisample(texture_gl->target, samples,
                    gl_internal_format, width, height, standard_pattern));
            break;
        case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
            GL_EXTCALL(glTexStorage3DMultisample(texture_gl->target, samples,
                    gl_internal_format, width, height, texture_gl->t.layer_count, standard_pattern));
            break;
        case GL_TEXTURE_1D_ARRAY:
            GL_EXTCALL(glTexStorage2D(texture_gl->target, texture_gl->t.level_count,
                    gl_internal_format, width, texture_gl->t.layer_count));
            break;
        case GL_TEXTURE_1D:
            GL_EXTCALL(glTexStorage1D(texture_gl->target, texture_gl->t.level_count, gl_internal_format, width));
            break;
        default:
            GL_EXTCALL(glTexStorage2D(texture_gl->target, texture_gl->t.level_count,
                    gl_internal_format, width, height));
            break;
    }

    checkGLcall("allocate immutable storage");
}

void wined3d_texture_sub_resources_destroyed(struct wined3d_texture *texture)
{
    unsigned int sub_count = texture->level_count * texture->layer_count;
    struct wined3d_texture_sub_resource *sub_resource;
    unsigned int i;

    for (i = 0; i < sub_count; ++i)
    {
        sub_resource = &texture->sub_resources[i];
        if (sub_resource->parent)
        {
            TRACE("sub-resource %u.\n", i);
            sub_resource->parent_ops->wined3d_object_destroyed(sub_resource->parent);
            sub_resource->parent = NULL;
        }
    }
}

static void wined3d_texture_create_dc(void *object)
{
    const struct wined3d_texture_idx *idx = object;
    struct wined3d_context *context = NULL;
    unsigned int sub_resource_idx, level;
    const struct wined3d_format *format;
    unsigned int row_pitch, slice_pitch;
    struct wined3d_texture *texture;
    struct wined3d_dc_info *dc_info;
    struct wined3d_bo_address data;
    D3DKMT_CREATEDCFROMMEMORY desc;
    struct wined3d_device *device;
    NTSTATUS status;

    TRACE("texture %p, sub_resource_idx %u.\n", idx->texture, idx->sub_resource_idx);

    texture = idx->texture;
    sub_resource_idx = idx->sub_resource_idx;
    level = sub_resource_idx % texture->level_count;
    device = texture->resource.device;

    format = texture->resource.format;
    if (!format->ddi_format)
    {
        WARN("Cannot create a DC for format %s.\n", debug_d3dformat(format->id));
        return;
    }

    if (!texture->dc_info)
    {
        unsigned int sub_count = texture->level_count * texture->layer_count;

        if (!(texture->dc_info = heap_calloc(sub_count, sizeof(*texture->dc_info))))
        {
            ERR("Failed to allocate DC info.\n");
            return;
        }
    }

    if (!(texture->sub_resources[sub_resource_idx].locations & texture->resource.map_binding))
    {
        context = context_acquire(device, NULL, 0);
        wined3d_texture_load_location(texture, sub_resource_idx, context, texture->resource.map_binding);
    }
    wined3d_texture_invalidate_location(texture, sub_resource_idx, ~texture->resource.map_binding);
    wined3d_texture_get_pitch(texture, level, &row_pitch, &slice_pitch);
    wined3d_texture_get_memory(texture, sub_resource_idx, &data, texture->resource.map_binding);
    if (data.buffer_object)
    {
        if (!context)
            context = context_acquire(device, NULL, 0);
        desc.pMemory = wined3d_context_map_bo_address(context, &data,
                texture->sub_resources[sub_resource_idx].size, WINED3D_MAP_READ | WINED3D_MAP_WRITE);
    }
    else
    {
        desc.pMemory = data.addr;
    }

    if (context)
        context_release(context);

    desc.Format = format->ddi_format;
    desc.Width = wined3d_texture_get_level_width(texture, level);
    desc.Height = wined3d_texture_get_level_height(texture, level);
    desc.Pitch = row_pitch;
    desc.hDeviceDc = CreateCompatibleDC(NULL);
    desc.pColorTable = NULL;

    status = D3DKMTCreateDCFromMemory(&desc);
    DeleteDC(desc.hDeviceDc);
    if (status)
    {
        WARN("Failed to create DC, status %#x.\n", status);
        return;
    }

    dc_info = &texture->dc_info[sub_resource_idx];
    dc_info->dc = desc.hDc;
    dc_info->bitmap = desc.hBitmap;

    TRACE("Created DC %p, bitmap %p for texture %p, %u.\n", dc_info->dc, dc_info->bitmap, texture, sub_resource_idx);
}

static void wined3d_texture_destroy_dc(void *object)
{
    const struct wined3d_texture_idx *idx = object;
    D3DKMT_DESTROYDCFROMMEMORY destroy_desc;
    struct wined3d_context *context;
    struct wined3d_texture *texture;
    struct wined3d_dc_info *dc_info;
    struct wined3d_bo_address data;
    unsigned int sub_resource_idx;
    struct wined3d_device *device;
    struct wined3d_range range;
    NTSTATUS status;

    texture = idx->texture;
    sub_resource_idx = idx->sub_resource_idx;
    device = texture->resource.device;
    dc_info = &texture->dc_info[sub_resource_idx];

    if (!dc_info->dc)
    {
        ERR("Sub-resource {%p, %u} has no DC.\n", texture, sub_resource_idx);
        return;
    }

    TRACE("dc %p, bitmap %p.\n", dc_info->dc, dc_info->bitmap);

    destroy_desc.hDc = dc_info->dc;
    destroy_desc.hBitmap = dc_info->bitmap;
    if ((status = D3DKMTDestroyDCFromMemory(&destroy_desc)))
        ERR("Failed to destroy dc, status %#x.\n", status);
    dc_info->dc = NULL;
    dc_info->bitmap = NULL;

    wined3d_texture_get_memory(texture, sub_resource_idx, &data, texture->resource.map_binding);
    if (data.buffer_object)
    {
        context = context_acquire(device, NULL, 0);
        range.offset = 0;
        range.size = texture->sub_resources[sub_resource_idx].size;
        wined3d_context_unmap_bo_address(context, &data, 1, &range);
        context_release(context);
    }
}

void wined3d_texture_set_swapchain(struct wined3d_texture *texture, struct wined3d_swapchain *swapchain)
{
    texture->swapchain = swapchain;
    wined3d_resource_update_draw_binding(&texture->resource);
}

void wined3d_gl_texture_swizzle_from_color_fixup(GLint swizzle[4], struct color_fixup_desc fixup)
{
    static const GLenum swizzle_source[] =
    {
        GL_ZERO,  /* CHANNEL_SOURCE_ZERO */
        GL_ONE,   /* CHANNEL_SOURCE_ONE */
        GL_RED,   /* CHANNEL_SOURCE_X */
        GL_GREEN, /* CHANNEL_SOURCE_Y */
        GL_BLUE,  /* CHANNEL_SOURCE_Z */
        GL_ALPHA, /* CHANNEL_SOURCE_W */
    };

    swizzle[0] = swizzle_source[fixup.x_source];
    swizzle[1] = swizzle_source[fixup.y_source];
    swizzle[2] = swizzle_source[fixup.z_source];
    swizzle[3] = swizzle_source[fixup.w_source];
}

/* Context activation is done by the caller. */
void wined3d_texture_gl_bind(struct wined3d_texture_gl *texture_gl,
        struct wined3d_context_gl *context_gl, BOOL srgb)
{
    const struct wined3d_format *format = texture_gl->t.resource.format;
    const struct wined3d_gl_info *gl_info = context_gl->gl_info;
    const struct color_fixup_desc fixup = format->color_fixup;
    struct gl_texture *gl_tex;
    GLenum target;

    TRACE("texture_gl %p, context_gl %p, srgb %#x.\n", texture_gl, context_gl, srgb);

    if (!needs_separate_srgb_gl_texture(&context_gl->c, &texture_gl->t))
        srgb = FALSE;

    /* sRGB mode cache for preload() calls outside drawprim. */
    if (srgb)
        texture_gl->t.flags |= WINED3D_TEXTURE_IS_SRGB;
    else
        texture_gl->t.flags &= ~WINED3D_TEXTURE_IS_SRGB;

    gl_tex = wined3d_texture_gl_get_gl_texture(texture_gl, srgb);
    target = texture_gl->target;

    if (gl_tex->name)
    {
        wined3d_context_gl_bind_texture(context_gl, target, gl_tex->name);
        return;
    }

    gl_info->gl_ops.gl.p_glGenTextures(1, &gl_tex->name);
    checkGLcall("glGenTextures");
    TRACE("Generated texture %d.\n", gl_tex->name);

    if (!gl_tex->name)
    {
        ERR("Failed to generate a texture name.\n");
        return;
    }

    /* Initialise the state of the texture object to the OpenGL defaults, not
     * the wined3d defaults. */
    gl_tex->sampler_desc.address_u = WINED3D_TADDRESS_WRAP;
    gl_tex->sampler_desc.address_v = WINED3D_TADDRESS_WRAP;
    gl_tex->sampler_desc.address_w = WINED3D_TADDRESS_WRAP;
    memset(gl_tex->sampler_desc.border_color, 0, sizeof(gl_tex->sampler_desc.border_color));
    gl_tex->sampler_desc.mag_filter = WINED3D_TEXF_LINEAR;
    gl_tex->sampler_desc.min_filter = WINED3D_TEXF_POINT; /* GL_NEAREST_MIPMAP_LINEAR */
    gl_tex->sampler_desc.mip_filter = WINED3D_TEXF_LINEAR; /* GL_NEAREST_MIPMAP_LINEAR */
    gl_tex->sampler_desc.lod_bias = 0.0f;
    gl_tex->sampler_desc.min_lod = -1000.0f;
    gl_tex->sampler_desc.max_lod = 1000.0f;
    gl_tex->sampler_desc.max_anisotropy = 1;
    gl_tex->sampler_desc.compare = FALSE;
    gl_tex->sampler_desc.comparison_func = WINED3D_CMP_LESSEQUAL;
    if (gl_info->supported[EXT_TEXTURE_SRGB_DECODE])
        gl_tex->sampler_desc.srgb_decode = TRUE;
    else
        gl_tex->sampler_desc.srgb_decode = srgb;
    gl_tex->base_level = 0;
    wined3d_texture_set_dirty(&texture_gl->t);

    wined3d_context_gl_bind_texture(context_gl, target, gl_tex->name);

    /* For a new texture we have to set the texture levels after binding the
     * texture. Beware that texture rectangles do not support mipmapping, but
     * set the maxmiplevel if we're relying on the partial
     * GL_ARB_texture_non_power_of_two emulation with texture rectangles.
     * (I.e., do not care about cond_np2 here, just look for
     * GL_TEXTURE_RECTANGLE_ARB.) */
    if (target != GL_TEXTURE_RECTANGLE_ARB)
    {
        TRACE("Setting GL_TEXTURE_MAX_LEVEL to %u.\n", texture_gl->t.level_count - 1);
        gl_info->gl_ops.gl.p_glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, texture_gl->t.level_count - 1);
        checkGLcall("glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, texture->level_count)");
    }

    if (target == GL_TEXTURE_CUBE_MAP_ARB)
    {
        /* Cubemaps are always set to clamp, regardless of the sampler state. */
        gl_info->gl_ops.gl.p_glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl_info->gl_ops.gl.p_glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        gl_info->gl_ops.gl.p_glTexParameteri(target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    }

    if (texture_gl->t.flags & WINED3D_TEXTURE_COND_NP2)
    {
        /* Conditinal non power of two textures use a different clamping
         * default. If we're using the GL_WINE_normalized_texrect partial
         * driver emulation, we're dealing with a GL_TEXTURE_2D texture which
         * has the address mode set to repeat - something that prevents us
         * from hitting the accelerated codepath. Thus manually set the GL
         * state. The same applies to filtering. Even if the texture has only
         * one mip level, the default LINEAR_MIPMAP_LINEAR filter causes a SW
         * fallback on macos. */
        gl_info->gl_ops.gl.p_glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl_info->gl_ops.gl.p_glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        gl_info->gl_ops.gl.p_glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        gl_info->gl_ops.gl.p_glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        checkGLcall("glTexParameteri");
        gl_tex->sampler_desc.address_u = WINED3D_TADDRESS_CLAMP;
        gl_tex->sampler_desc.address_v = WINED3D_TADDRESS_CLAMP;
        gl_tex->sampler_desc.mag_filter = WINED3D_TEXF_POINT;
        gl_tex->sampler_desc.min_filter = WINED3D_TEXF_POINT;
        gl_tex->sampler_desc.mip_filter = WINED3D_TEXF_NONE;
    }

    if (gl_info->supported[WINED3D_GL_LEGACY_CONTEXT] && gl_info->supported[ARB_DEPTH_TEXTURE])
    {
        gl_info->gl_ops.gl.p_glTexParameteri(target, GL_DEPTH_TEXTURE_MODE_ARB, GL_INTENSITY);
        checkGLcall("glTexParameteri(GL_DEPTH_TEXTURE_MODE_ARB, GL_INTENSITY)");
    }

    if (!is_identity_fixup(fixup) && can_use_texture_swizzle(context_gl->c.d3d_info, format))
    {
        GLint swizzle[4];

        wined3d_gl_texture_swizzle_from_color_fixup(swizzle, fixup);
        gl_info->gl_ops.gl.p_glTexParameteriv(target, GL_TEXTURE_SWIZZLE_RGBA, swizzle);
        checkGLcall("set format swizzle");
    }
}

/* Context activation is done by the caller. */
void wined3d_texture_gl_bind_and_dirtify(struct wined3d_texture_gl *texture_gl,
        struct wined3d_context_gl *context_gl, BOOL srgb)
{
    /* We don't need a specific texture unit, but after binding the texture
     * the current unit is dirty. Read the unit back instead of switching to
     * 0, this avoids messing around with the state manager's GL states. The
     * current texture unit should always be a valid one.
     *
     * To be more specific, this is tricky because we can implicitly be
     * called from sampler() in state.c. This means we can't touch anything
     * other than whatever happens to be the currently active texture, or we
     * would risk marking already applied sampler states dirty again. */
    if (context_gl->active_texture < ARRAY_SIZE(context_gl->rev_tex_unit_map))
    {
        unsigned int active_sampler = context_gl->rev_tex_unit_map[context_gl->active_texture];
        if (active_sampler != WINED3D_UNMAPPED_STAGE)
            context_invalidate_state(&context_gl->c, STATE_SAMPLER(active_sampler));
    }
    /* FIXME: Ideally we'd only do this when touching a binding that's used by
     * a shader. */
    context_invalidate_compute_state(&context_gl->c, STATE_COMPUTE_SHADER_RESOURCE_BINDING);
    context_invalidate_state(&context_gl->c, STATE_GRAPHICS_SHADER_RESOURCE_BINDING);

    wined3d_texture_gl_bind(texture_gl, context_gl, srgb);
}

/* Context activation is done by the caller (state handler). */
/* This function relies on the correct texture being bound and loaded. */
void wined3d_texture_gl_apply_sampler_desc(struct wined3d_texture_gl *texture_gl,
        const struct wined3d_sampler_desc *sampler_desc, const struct wined3d_context_gl *context_gl)
{
    const struct wined3d_gl_info *gl_info = context_gl->gl_info;
    GLenum target = texture_gl->target;
    struct gl_texture *gl_tex;
    DWORD state;

    TRACE("texture_gl %p, sampler_desc %p, context_gl %p.\n", texture_gl, sampler_desc, context_gl);

    gl_tex = wined3d_texture_gl_get_gl_texture(texture_gl, texture_gl->t.flags & WINED3D_TEXTURE_IS_SRGB);

    state = sampler_desc->address_u;
    if (state != gl_tex->sampler_desc.address_u)
    {
        gl_info->gl_ops.gl.p_glTexParameteri(target, GL_TEXTURE_WRAP_S,
                gl_info->wrap_lookup[state - WINED3D_TADDRESS_WRAP]);
        gl_tex->sampler_desc.address_u = state;
    }

    state = sampler_desc->address_v;
    if (state != gl_tex->sampler_desc.address_v)
    {
        gl_info->gl_ops.gl.p_glTexParameteri(target, GL_TEXTURE_WRAP_T,
                gl_info->wrap_lookup[state - WINED3D_TADDRESS_WRAP]);
        gl_tex->sampler_desc.address_v = state;
    }

    state = sampler_desc->address_w;
    if (state != gl_tex->sampler_desc.address_w)
    {
        gl_info->gl_ops.gl.p_glTexParameteri(target, GL_TEXTURE_WRAP_R,
                gl_info->wrap_lookup[state - WINED3D_TADDRESS_WRAP]);
        gl_tex->sampler_desc.address_w = state;
    }

    if (memcmp(gl_tex->sampler_desc.border_color, sampler_desc->border_color,
            sizeof(gl_tex->sampler_desc.border_color)))
    {
        gl_info->gl_ops.gl.p_glTexParameterfv(target, GL_TEXTURE_BORDER_COLOR, &sampler_desc->border_color[0]);
        memcpy(gl_tex->sampler_desc.border_color, sampler_desc->border_color,
                sizeof(gl_tex->sampler_desc.border_color));
    }

    state = sampler_desc->mag_filter;
    if (state != gl_tex->sampler_desc.mag_filter)
    {
        gl_info->gl_ops.gl.p_glTexParameteri(target, GL_TEXTURE_MAG_FILTER, wined3d_gl_mag_filter(state));
        gl_tex->sampler_desc.mag_filter = state;
    }

    if (sampler_desc->min_filter != gl_tex->sampler_desc.min_filter
            || sampler_desc->mip_filter != gl_tex->sampler_desc.mip_filter)
    {
        gl_info->gl_ops.gl.p_glTexParameteri(target, GL_TEXTURE_MIN_FILTER,
                wined3d_gl_min_mip_filter(sampler_desc->min_filter, sampler_desc->mip_filter));
        gl_tex->sampler_desc.min_filter = sampler_desc->min_filter;
        gl_tex->sampler_desc.mip_filter = sampler_desc->mip_filter;
    }

    state = sampler_desc->max_anisotropy;
    if (state != gl_tex->sampler_desc.max_anisotropy)
    {
        if (gl_info->supported[ARB_TEXTURE_FILTER_ANISOTROPIC])
            gl_info->gl_ops.gl.p_glTexParameteri(target, GL_TEXTURE_MAX_ANISOTROPY, state);
        else
            WARN("Anisotropic filtering not supported.\n");
        gl_tex->sampler_desc.max_anisotropy = state;
    }

    if (!sampler_desc->srgb_decode != !gl_tex->sampler_desc.srgb_decode
            && (context_gl->c.d3d_info->wined3d_creation_flags & WINED3D_SRGB_READ_WRITE_CONTROL)
            && gl_info->supported[EXT_TEXTURE_SRGB_DECODE])
    {
        gl_info->gl_ops.gl.p_glTexParameteri(target, GL_TEXTURE_SRGB_DECODE_EXT,
                sampler_desc->srgb_decode ? GL_DECODE_EXT : GL_SKIP_DECODE_EXT);
        gl_tex->sampler_desc.srgb_decode = sampler_desc->srgb_decode;
    }

    if (!sampler_desc->compare != !gl_tex->sampler_desc.compare)
    {
        if (sampler_desc->compare)
            gl_info->gl_ops.gl.p_glTexParameteri(target, GL_TEXTURE_COMPARE_MODE_ARB, GL_COMPARE_R_TO_TEXTURE_ARB);
        else
            gl_info->gl_ops.gl.p_glTexParameteri(target, GL_TEXTURE_COMPARE_MODE_ARB, GL_NONE);
        gl_tex->sampler_desc.compare = sampler_desc->compare;
    }

    checkGLcall("Texture parameter application");

    if (gl_info->supported[EXT_TEXTURE_LOD_BIAS])
    {
        gl_info->gl_ops.gl.p_glTexEnvf(GL_TEXTURE_FILTER_CONTROL_EXT,
                GL_TEXTURE_LOD_BIAS_EXT, sampler_desc->lod_bias);
        checkGLcall("glTexEnvf(GL_TEXTURE_LOD_BIAS_EXT, ...)");
    }
}

ULONG CDECL wined3d_texture_incref(struct wined3d_texture *texture)
{
    ULONG refcount;

    TRACE("texture %p, swapchain %p.\n", texture, texture->swapchain);

    if (texture->swapchain)
        return wined3d_swapchain_incref(texture->swapchain);

    refcount = InterlockedIncrement(&texture->resource.ref);
    TRACE("%p increasing refcount to %u.\n", texture, refcount);

    return refcount;
}

static void wined3d_texture_destroy_object(void *object)
{
    struct wined3d_texture *texture = object;
    struct wined3d_resource *resource;
    struct wined3d_dc_info *dc_info;
    unsigned int sub_count;
    unsigned int i;

    TRACE("texture %p.\n", texture);

    resource = &texture->resource;
    sub_count = texture->level_count * texture->layer_count;

    if ((dc_info = texture->dc_info))
    {
        for (i = 0; i < sub_count; ++i)
        {
            if (dc_info[i].dc)
            {
                struct wined3d_texture_idx texture_idx = {texture, i};

                wined3d_texture_destroy_dc(&texture_idx);
            }
        }
        heap_free(dc_info);
    }

    if (texture->overlay_info)
    {
        for (i = 0; i < sub_count; ++i)
        {
            struct wined3d_overlay_info *info = &texture->overlay_info[i];
            struct wined3d_overlay_info *overlay, *cur;

            list_remove(&info->entry);
            LIST_FOR_EACH_ENTRY_SAFE(overlay, cur, &info->overlays, struct wined3d_overlay_info, entry)
            {
                list_remove(&overlay->entry);
            }
        }
        heap_free(texture->overlay_info);
    }

    if (texture->dirty_regions)
    {
        for (i = 0; i < texture->layer_count; ++i)
        {
            heap_free(texture->dirty_regions[i].boxes);
        }
        heap_free(texture->dirty_regions);
    }

    resource->resource_ops->resource_unload(resource);
}

void wined3d_texture_cleanup(struct wined3d_texture *texture)
{
    wined3d_cs_destroy_object(texture->resource.device->cs, wined3d_texture_destroy_object, texture);
    resource_cleanup(&texture->resource);
}

static void wined3d_texture_cleanup_sync(struct wined3d_texture *texture)
{
    wined3d_texture_sub_resources_destroyed(texture);
    wined3d_texture_cleanup(texture);
    wined3d_resource_wait_idle(&texture->resource);
}

ULONG CDECL wined3d_texture_decref(struct wined3d_texture *texture)
{
    unsigned int i, sub_resource_count;
    ULONG refcount;

    TRACE("texture %p, swapchain %p.\n", texture, texture->swapchain);

    if (texture->swapchain)
        return wined3d_swapchain_decref(texture->swapchain);

    refcount = InterlockedDecrement(&texture->resource.ref);
    TRACE("%p decreasing refcount to %u.\n", texture, refcount);

    if (!refcount)
    {
        /* Wait for the texture to become idle if it's using user memory,
         * since the application is allowed to free that memory once the
         * texture is destroyed. Note that this implies that
         * the destroy handler can't access that memory either. */
        sub_resource_count = texture->layer_count * texture->level_count;
        for (i = 0; i < sub_resource_count; ++i)
        {
            if (texture->sub_resources[i].user_memory)
            {
                wined3d_resource_wait_idle(&texture->resource);
                break;
            }
        }
        texture->resource.device->adapter->adapter_ops->adapter_destroy_texture(texture);
    }

    return refcount;
}

struct wined3d_resource * CDECL wined3d_texture_get_resource(struct wined3d_texture *texture)
{
    TRACE("texture %p.\n", texture);

    return &texture->resource;
}

static BOOL color_key_equal(const struct wined3d_color_key *c1, struct wined3d_color_key *c2)
{
    return c1->color_space_low_value == c2->color_space_low_value
            && c1->color_space_high_value == c2->color_space_high_value;
}

/* Context activation is done by the caller */
void wined3d_texture_load(struct wined3d_texture *texture,
        struct wined3d_context *context, BOOL srgb)
{
    UINT sub_count = texture->level_count * texture->layer_count;
    const struct wined3d_d3d_info *d3d_info = context->d3d_info;
    DWORD flag;
    UINT i;

    TRACE("texture %p, context %p, srgb %#x.\n", texture, context, srgb);

    if (!needs_separate_srgb_gl_texture(context, texture))
        srgb = FALSE;

    if (srgb)
        flag = WINED3D_TEXTURE_SRGB_VALID;
    else
        flag = WINED3D_TEXTURE_RGB_VALID;

    if (!d3d_info->shader_color_key
            && (!(texture->async.flags & WINED3D_TEXTURE_ASYNC_COLOR_KEY)
            != !(texture->async.color_key_flags & WINED3D_CKEY_SRC_BLT)
            || (texture->async.flags & WINED3D_TEXTURE_ASYNC_COLOR_KEY
            && !color_key_equal(&texture->async.gl_color_key, &texture->async.src_blt_color_key))))
    {
        unsigned int sub_count = texture->level_count * texture->layer_count;
        unsigned int i;

        TRACE("Reloading because of color key value change.\n");
        for (i = 0; i < sub_count; i++)
        {
            if (!wined3d_texture_load_location(texture, i, context, texture->resource.map_binding))
                ERR("Failed to load location %s.\n", wined3d_debug_location(texture->resource.map_binding));
            else
                wined3d_texture_invalidate_location(texture, i, ~texture->resource.map_binding);
        }

        texture->async.gl_color_key = texture->async.src_blt_color_key;
    }

    if (texture->flags & flag)
    {
        TRACE("Texture %p not dirty, nothing to do.\n", texture);
        return;
    }

    /* Reload the surfaces if the texture is marked dirty. */
    for (i = 0; i < sub_count; ++i)
    {
        if (!wined3d_texture_load_location(texture, i, context,
                srgb ? WINED3D_LOCATION_TEXTURE_SRGB : WINED3D_LOCATION_TEXTURE_RGB))
            ERR("Failed to load location (srgb %#x).\n", srgb);
    }
    texture->flags |= flag;
}

void * CDECL wined3d_texture_get_parent(const struct wined3d_texture *texture)
{
    TRACE("texture %p.\n", texture);

    return texture->resource.parent;
}

HRESULT wined3d_texture_check_box_dimensions(const struct wined3d_texture *texture,
        unsigned int level, const struct wined3d_box *box)
{
    const struct wined3d_format *format = texture->resource.format;
    unsigned int width_mask, height_mask, width, height, depth;

    width = wined3d_texture_get_level_width(texture, level);
    height = wined3d_texture_get_level_height(texture, level);
    depth = wined3d_texture_get_level_depth(texture, level);

    if (box->left >= box->right || box->right > width
            || box->top >= box->bottom || box->bottom > height
            || box->front >= box->back || box->back > depth)
    {
        WARN("Box %s is invalid.\n", debug_box(box));
        return WINEDDERR_INVALIDRECT;
    }

    if (texture->resource.format_flags & WINED3DFMT_FLAG_BLOCKS)
    {
        /* This assumes power of two block sizes, but NPOT block sizes would
         * be silly anyway.
         *
         * This also assumes that the format's block depth is 1. */
        width_mask = format->block_width - 1;
        height_mask = format->block_height - 1;

        if ((box->left & width_mask) || (box->top & height_mask)
                || (box->right & width_mask && box->right != width)
                || (box->bottom & height_mask && box->bottom != height))
        {
            WARN("Box %s is misaligned for %ux%u blocks.\n",
                    debug_box(box), format->block_width, format->block_height);
            return WINED3DERR_INVALIDCALL;
        }
    }

    return WINED3D_OK;
}

void CDECL wined3d_texture_get_pitch(const struct wined3d_texture *texture,
        unsigned int level, unsigned int *row_pitch, unsigned int *slice_pitch)
{
    const struct wined3d_resource *resource = &texture->resource;
    unsigned int width = wined3d_texture_get_level_width(texture, level);
    unsigned int height = wined3d_texture_get_level_height(texture, level);

    if (texture->row_pitch)
    {
        *row_pitch = texture->row_pitch;
        *slice_pitch = texture->slice_pitch;
        return;
    }

    wined3d_format_calculate_pitch(resource->format, resource->device->surface_alignment,
            width, height, row_pitch, slice_pitch);
}

DWORD CDECL wined3d_texture_set_lod(struct wined3d_texture *texture, DWORD lod)
{
    struct wined3d_resource *resource;
    DWORD old = texture->lod;

    TRACE("texture %p, lod %u.\n", texture, lod);

    /* The d3d9:texture test shows that SetLOD is ignored on non-managed
     * textures. The call always returns 0, and GetLOD always returns 0. */
    resource = &texture->resource;
    if (!wined3d_resource_access_is_managed(resource->access))
    {
        TRACE("Ignoring LOD on texture with resource access %s.\n",
                wined3d_debug_resource_access(resource->access));
        return 0;
    }

    if (lod >= texture->level_count)
        lod = texture->level_count - 1;

    if (texture->lod != lod)
    {
        struct wined3d_device *device = resource->device;

        wined3d_resource_wait_idle(resource);
        texture->lod = lod;

        wined3d_texture_gl(texture)->texture_rgb.base_level = ~0u;
        wined3d_texture_gl(texture)->texture_srgb.base_level = ~0u;
        if (resource->bind_count)
            wined3d_cs_emit_set_sampler_state(device->cs, texture->sampler, WINED3D_SAMP_MAX_MIP_LEVEL,
                    device->state.sampler_states[texture->sampler][WINED3D_SAMP_MAX_MIP_LEVEL]);
    }

    return old;
}

DWORD CDECL wined3d_texture_get_lod(const struct wined3d_texture *texture)
{
    TRACE("texture %p, returning %u.\n", texture, texture->lod);

    return texture->lod;
}

DWORD CDECL wined3d_texture_get_level_count(const struct wined3d_texture *texture)
{
    TRACE("texture %p, returning %u.\n", texture, texture->level_count);

    return texture->level_count;
}

HRESULT CDECL wined3d_texture_set_color_key(struct wined3d_texture *texture,
        DWORD flags, const struct wined3d_color_key *color_key)
{
    struct wined3d_device *device = texture->resource.device;
    static const DWORD all_flags = WINED3D_CKEY_DST_BLT | WINED3D_CKEY_DST_OVERLAY
            | WINED3D_CKEY_SRC_BLT | WINED3D_CKEY_SRC_OVERLAY;

    TRACE("texture %p, flags %#x, color_key %p.\n", texture, flags, color_key);

    if (flags & ~all_flags)
    {
        WARN("Invalid flags passed, returning WINED3DERR_INVALIDCALL.\n");
        return WINED3DERR_INVALIDCALL;
    }

    wined3d_cs_emit_set_color_key(device->cs, texture, flags, color_key);

    return WINED3D_OK;
}

/* In D3D the depth stencil dimensions have to be greater than or equal to the
 * render target dimensions. With FBOs, the dimensions have to be an exact match. */
/* TODO: We should synchronize the renderbuffer's content with the texture's content. */
/* Context activation is done by the caller. */
void wined3d_texture_gl_set_compatible_renderbuffer(struct wined3d_texture_gl *texture_gl,
        struct wined3d_context_gl *context_gl, unsigned int level, const struct wined3d_rendertarget_info *rt)
{
    const struct wined3d_gl_info *gl_info = context_gl->gl_info;
    struct wined3d_renderbuffer_entry *entry;
    unsigned int src_width, src_height;
    unsigned int width, height;
    GLuint renderbuffer = 0;

    if (gl_info->supported[ARB_FRAMEBUFFER_OBJECT])
        return;

    if (rt && rt->resource->format->id != WINED3DFMT_NULL)
    {
        struct wined3d_texture *rt_texture;
        unsigned int rt_level;

        if (rt->resource->type == WINED3D_RTYPE_BUFFER)
        {
            FIXME("Unsupported resource type %s.\n", debug_d3dresourcetype(rt->resource->type));
            return;
        }
        rt_texture = wined3d_texture_from_resource(rt->resource);
        rt_level = rt->sub_resource_idx % rt_texture->level_count;

        width = wined3d_texture_get_level_pow2_width(rt_texture, rt_level);
        height = wined3d_texture_get_level_pow2_height(rt_texture, rt_level);
    }
    else
    {
        width = wined3d_texture_get_level_pow2_width(&texture_gl->t, level);
        height = wined3d_texture_get_level_pow2_height(&texture_gl->t, level);
    }

    src_width = wined3d_texture_get_level_pow2_width(&texture_gl->t, level);
    src_height = wined3d_texture_get_level_pow2_height(&texture_gl->t, level);

    /* A depth stencil smaller than the render target is not valid */
    if (width > src_width || height > src_height)
        return;

    /* Remove any renderbuffer set if the sizes match */
    if (width == src_width && height == src_height)
    {
        texture_gl->current_renderbuffer = NULL;
        return;
    }

    /* Look if we've already got a renderbuffer of the correct dimensions */
    LIST_FOR_EACH_ENTRY(entry, &texture_gl->renderbuffers, struct wined3d_renderbuffer_entry, entry)
    {
        if (entry->width == width && entry->height == height)
        {
            renderbuffer = entry->id;
            texture_gl->current_renderbuffer = entry;
            break;
        }
    }

    if (!renderbuffer)
    {
        const struct wined3d_format_gl *format_gl;

        format_gl = wined3d_format_gl(texture_gl->t.resource.format);
        gl_info->fbo_ops.glGenRenderbuffers(1, &renderbuffer);
        gl_info->fbo_ops.glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
        gl_info->fbo_ops.glRenderbufferStorage(GL_RENDERBUFFER, format_gl->internal, width, height);

        entry = heap_alloc(sizeof(*entry));
        entry->width = width;
        entry->height = height;
        entry->id = renderbuffer;
        list_add_head(&texture_gl->renderbuffers, &entry->entry);

        texture_gl->current_renderbuffer = entry;
    }

    checkGLcall("set compatible renderbuffer");
}

HRESULT CDECL wined3d_texture_update_desc(struct wined3d_texture *texture, unsigned int sub_resource_idx,
        UINT width, UINT height, enum wined3d_format_id format_id,
        enum wined3d_multisample_type multisample_type, UINT multisample_quality, void *mem, UINT pitch)
{
    struct wined3d_texture_sub_resource *sub_resource;
    unsigned int i, level, sub_resource_count;
    const struct wined3d_d3d_info *d3d_info;
    const struct wined3d_gl_info *gl_info;
    const struct wined3d_format *format;
    struct wined3d_device *device;
    unsigned int resource_size;
    const struct wined3d *d3d;
    unsigned int slice_pitch;
    bool update_memory_only;
    bool create_dib = false;

    TRACE("texture %p, width %u, height %u, format %s, multisample_type %#x, multisample_quality %u, "
            "mem %p, pitch %u, sub_resource_idx %u.\n",
            texture, width, height, debug_d3dformat(format_id), multisample_type, multisample_quality, mem, pitch,
            sub_resource_idx);

    device = texture->resource.device;
    d3d = device->wined3d;
    gl_info = &device->adapter->gl_info;
    d3d_info = &device->adapter->d3d_info;
    format = wined3d_get_format(device->adapter, format_id, texture->resource.bind_flags);
    resource_size = wined3d_format_calculate_size(format, device->surface_alignment, width, height, 1);
    level = sub_resource_idx % texture->level_count;
    sub_resource_count = texture->level_count * texture->layer_count;

    update_memory_only = width == wined3d_texture_get_level_width(texture, level)
            && height == wined3d_texture_get_level_height(texture, level)
            && format_id == texture->resource.format->id && multisample_type == texture->resource.multisample_type
            && multisample_quality == texture->resource.multisample_quality;

    if (pitch)
        slice_pitch = height * pitch;
    else
        wined3d_format_calculate_pitch(format, 1, width, height, &pitch, &slice_pitch);

    if (update_memory_only)
    {
        unsigned int current_row_pitch, current_slice_pitch;

        wined3d_texture_get_pitch(texture, level, &current_row_pitch, &current_slice_pitch);
        update_memory_only = pitch == current_row_pitch && slice_pitch == current_slice_pitch;
    }

    if (!resource_size)
        return WINED3DERR_INVALIDCALL;

    if (sub_resource_count > 1 && !update_memory_only)
    {
        FIXME("Texture has multiple sub-resources, not supported.\n");
        return WINED3DERR_INVALIDCALL;
    }

    if (texture->resource.type != WINED3D_RTYPE_TEXTURE_2D)
    {
        WARN("Not supported on %s.\n", debug_d3dresourcetype(texture->resource.type));
        return WINED3DERR_INVALIDCALL;
    }

    if (texture->resource.map_count)
    {
        WARN("Texture is mapped.\n");
        return WINED3DERR_INVALIDCALL;
    }

    /* We have no way of supporting a pitch that is not a multiple of the pixel
     * byte width short of uploading the texture row-by-row.
     * Fortunately that's not an issue since D3D9Ex doesn't allow a custom pitch
     * for user-memory textures (it always expects packed data) while DirectDraw
     * requires a 4-byte aligned pitch and doesn't support texture formats
     * larger than 4 bytes per pixel nor any format using 3 bytes per pixel.
     * This check is here to verify that the assumption holds. */
    if (pitch % texture->resource.format->byte_count)
    {
        WARN("Pitch unsupported, not a multiple of the texture format byte width.\n");
        return WINED3DERR_INVALIDCALL;
    }

    if (device->d3d_initialized)
        wined3d_cs_emit_unload_resource(device->cs, &texture->resource);
    wined3d_resource_wait_idle(&texture->resource);

    if (texture->dc_info && texture->dc_info[0].dc)
    {
        struct wined3d_texture_idx texture_idx = {texture, sub_resource_idx};

        wined3d_cs_destroy_object(device->cs, wined3d_texture_destroy_dc, &texture_idx);
        wined3d_cs_finish(device->cs, WINED3D_CS_QUEUE_DEFAULT);
        create_dib = true;
    }

    texture->sub_resources[sub_resource_idx].user_memory = mem;

    if (update_memory_only)
    {
        for (i = 0; i < sub_resource_count; ++i)
            if (!texture->sub_resources[i].user_memory)
                break;

        if (i == sub_resource_count)
            wined3d_resource_free_sysmem(&texture->resource);
    }
    else
    {
        wined3d_resource_free_sysmem(&texture->resource);

        sub_resource = &texture->sub_resources[sub_resource_idx];

        texture->row_pitch = pitch;
        texture->slice_pitch = slice_pitch;

        texture->resource.format = format;
        texture->resource.multisample_type = multisample_type;
        texture->resource.multisample_quality = multisample_quality;
        texture->resource.width = width;
        texture->resource.height = height;
        if (!(texture->resource.access & WINED3D_RESOURCE_ACCESS_CPU) && d3d->flags & WINED3D_VIDMEM_ACCOUNTING)
            adapter_adjust_memory(device->adapter,  (INT64)texture->slice_pitch - texture->resource.size);
        texture->resource.size = texture->slice_pitch;
        sub_resource->size = texture->slice_pitch;
        sub_resource->locations = WINED3D_LOCATION_DISCARDED;

        if (texture->texture_ops == &texture_gl_ops)
        {
            if (multisample_type && gl_info->supported[ARB_TEXTURE_MULTISAMPLE])
            {
                wined3d_texture_gl(texture)->target = GL_TEXTURE_2D_MULTISAMPLE;
                texture->flags &= ~WINED3D_TEXTURE_DOWNLOADABLE;
            }
            else
            {
                wined3d_texture_gl(texture)->target = GL_TEXTURE_2D;
                texture->flags |= WINED3D_TEXTURE_DOWNLOADABLE;
            }
        }

        if (((width & (width - 1)) || (height & (height - 1))) && !d3d_info->texture_npot
                && !d3d_info->texture_npot_conditional)
        {
            texture->flags |= WINED3D_TEXTURE_COND_NP2_EMULATED;
            texture->pow2_width = texture->pow2_height = 1;
            while (texture->pow2_width < width)
                texture->pow2_width <<= 1;
            while (texture->pow2_height < height)
                texture->pow2_height <<= 1;
        }
        else
        {
            texture->flags &= ~WINED3D_TEXTURE_COND_NP2_EMULATED;
            texture->pow2_width = width;
            texture->pow2_height = height;
        }
    }

    if (!mem && !wined3d_resource_prepare_sysmem(&texture->resource))
        ERR("Failed to allocate resource memory.\n");

    /* The format might be changed to a format that needs conversion.
     * If the surface didn't use PBOs previously but could now, don't
     * change it - whatever made us not use PBOs might come back, e.g.
     * color keys. */
    if (texture->resource.map_binding == WINED3D_LOCATION_BUFFER && !wined3d_texture_use_pbo(texture, gl_info))
        texture->resource.map_binding = WINED3D_LOCATION_SYSMEM;

    wined3d_texture_validate_location(texture, sub_resource_idx, WINED3D_LOCATION_SYSMEM);
    wined3d_texture_invalidate_location(texture, sub_resource_idx, ~WINED3D_LOCATION_SYSMEM);

    if (create_dib)
    {
        struct wined3d_texture_idx texture_idx = {texture, sub_resource_idx};

        wined3d_cs_init_object(device->cs, wined3d_texture_create_dc, &texture_idx);
        wined3d_cs_finish(device->cs, WINED3D_CS_QUEUE_DEFAULT);
    }

    return WINED3D_OK;
}

/* Context activation is done by the caller. */
static void wined3d_texture_prepare_buffer_object(struct wined3d_texture *texture,
        unsigned int sub_resource_idx, const struct wined3d_gl_info *gl_info)
{
    struct wined3d_texture_sub_resource *sub_resource;
    struct wined3d_bo_gl *bo;

    sub_resource = &texture->sub_resources[sub_resource_idx];
    bo = &sub_resource->bo;
    if (bo->id)
        return;

    GL_EXTCALL(glGenBuffers(1, &bo->id));
    bo->binding = GL_PIXEL_UNPACK_BUFFER;
    GL_EXTCALL(glBindBuffer(bo->binding, bo->id));
    GL_EXTCALL(glBufferData(bo->binding, sub_resource->size, NULL, GL_STREAM_DRAW));
    GL_EXTCALL(glBindBuffer(bo->binding, 0));
    checkGLcall("Create buffer object");

    TRACE("Created buffer object %u for texture %p, sub-resource %u.\n", bo->id, texture, sub_resource_idx);
}

static void wined3d_texture_force_reload(struct wined3d_texture *texture)
{
    unsigned int sub_count = texture->level_count * texture->layer_count;
    unsigned int i;

    texture->flags &= ~(WINED3D_TEXTURE_RGB_ALLOCATED | WINED3D_TEXTURE_SRGB_ALLOCATED
            | WINED3D_TEXTURE_CONVERTED);
    texture->async.flags &= ~WINED3D_TEXTURE_ASYNC_COLOR_KEY;
    for (i = 0; i < sub_count; ++i)
    {
        wined3d_texture_invalidate_location(texture, i,
                WINED3D_LOCATION_TEXTURE_RGB | WINED3D_LOCATION_TEXTURE_SRGB);
    }
}

/* Context activation is done by the caller. */
void wined3d_texture_gl_prepare_texture(struct wined3d_texture_gl *texture_gl,
        struct wined3d_context_gl *context_gl, BOOL srgb)
{
    DWORD alloc_flag = srgb ? WINED3D_TEXTURE_SRGB_ALLOCATED : WINED3D_TEXTURE_RGB_ALLOCATED;
    const struct wined3d_d3d_info *d3d_info = context_gl->c.d3d_info;
    const struct wined3d_gl_info *gl_info = context_gl->gl_info;
    struct wined3d_resource *resource = &texture_gl->t.resource;
    const struct wined3d_device *device = resource->device;
    const struct wined3d_format *format = resource->format;
    const struct wined3d_color_key_conversion *conversion;
    const struct wined3d_format_gl *format_gl;
    GLenum internal;

    TRACE("texture_gl %p, context_gl %p, format %s.\n", texture_gl, context_gl, debug_d3dformat(format->id));

    if (!d3d_info->shader_color_key
            && !(texture_gl->t.async.flags & WINED3D_TEXTURE_ASYNC_COLOR_KEY)
            != !(texture_gl->t.async.color_key_flags & WINED3D_CKEY_SRC_BLT))
    {
        wined3d_texture_force_reload(&texture_gl->t);

        if (texture_gl->t.async.color_key_flags & WINED3D_CKEY_SRC_BLT)
            texture_gl->t.async.flags |= WINED3D_TEXTURE_ASYNC_COLOR_KEY;
    }

    if (texture_gl->t.flags & alloc_flag)
        return;

    if (resource->format_flags & WINED3DFMT_FLAG_DECOMPRESS)
    {
        TRACE("WINED3DFMT_FLAG_DECOMPRESS set.\n");
        texture_gl->t.flags |= WINED3D_TEXTURE_CONVERTED;
        format = wined3d_resource_get_decompress_format(resource);
    }
    else if (format->conv_byte_count)
    {
        texture_gl->t.flags |= WINED3D_TEXTURE_CONVERTED;
    }
    else if ((conversion = wined3d_format_get_color_key_conversion(&texture_gl->t, TRUE)))
    {
        texture_gl->t.flags |= WINED3D_TEXTURE_CONVERTED;
        format = wined3d_get_format(device->adapter, conversion->dst_format, resource->bind_flags);
        TRACE("Using format %s for color key conversion.\n", debug_d3dformat(format->id));
    }
    format_gl = wined3d_format_gl(format);

    wined3d_texture_gl_bind_and_dirtify(texture_gl, context_gl, srgb);

    if (srgb)
        internal = format_gl->srgb_internal;
    else if (resource->bind_flags & WINED3D_BIND_RENDER_TARGET && wined3d_resource_is_offscreen(resource))
        internal = format_gl->rt_internal;
    else
        internal = format_gl->internal;

    if (!internal)
        FIXME("No GL internal format for format %s.\n", debug_d3dformat(format->id));

    TRACE("internal %#x, format %#x, type %#x.\n", internal, format_gl->format, format_gl->type);

    if (wined3d_texture_use_immutable_storage(&texture_gl->t, gl_info))
        wined3d_texture_gl_allocate_immutable_storage(texture_gl, internal, gl_info);
    else
        wined3d_texture_gl_allocate_mutable_storage(texture_gl, internal, format_gl, gl_info);
    texture_gl->t.flags |= alloc_flag;
}

static void wined3d_texture_gl_prepare_rb(struct wined3d_texture_gl *texture_gl,
        const struct wined3d_gl_info *gl_info, BOOL multisample)
{
    const struct wined3d_format_gl *format_gl;

    format_gl = wined3d_format_gl(texture_gl->t.resource.format);
    if (multisample)
    {
        DWORD samples;

        if (texture_gl->rb_multisample)
            return;

        samples = wined3d_resource_get_sample_count(&texture_gl->t.resource);

        gl_info->fbo_ops.glGenRenderbuffers(1, &texture_gl->rb_multisample);
        gl_info->fbo_ops.glBindRenderbuffer(GL_RENDERBUFFER, texture_gl->rb_multisample);
        gl_info->fbo_ops.glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples,
                format_gl->internal, texture_gl->t.resource.width, texture_gl->t.resource.height);
        checkGLcall("glRenderbufferStorageMultisample()");
        TRACE("Created multisample rb %u.\n", texture_gl->rb_multisample);
    }
    else
    {
        if (texture_gl->rb_resolved)
            return;

        gl_info->fbo_ops.glGenRenderbuffers(1, &texture_gl->rb_resolved);
        gl_info->fbo_ops.glBindRenderbuffer(GL_RENDERBUFFER, texture_gl->rb_resolved);
        gl_info->fbo_ops.glRenderbufferStorage(GL_RENDERBUFFER, format_gl->internal,
                texture_gl->t.resource.width, texture_gl->t.resource.height);
        checkGLcall("glRenderbufferStorage()");
        TRACE("Created resolved rb %u.\n", texture_gl->rb_resolved);
    }
}

BOOL wined3d_texture_prepare_location(struct wined3d_texture *texture,
        unsigned int sub_resource_idx, struct wined3d_context *context, unsigned int location)
{
    return texture->texture_ops->texture_prepare_location(texture, sub_resource_idx, context, location);
}

static void wined3d_texture_unload_location(struct wined3d_texture *texture,
        struct wined3d_context *context, unsigned int location)
{
    texture->texture_ops->texture_unload_location(texture, context, location);
}

static struct wined3d_texture_sub_resource *wined3d_texture_get_sub_resource(struct wined3d_texture *texture,
        unsigned int sub_resource_idx)
{
    UINT sub_count = texture->level_count * texture->layer_count;

    TRACE("texture %p, sub_resource_idx %u.\n", texture, sub_resource_idx);

    if (sub_resource_idx >= sub_count)
    {
        WARN("sub_resource_idx %u >= sub_count %u.\n", sub_resource_idx, sub_count);
        return NULL;
    }

    return &texture->sub_resources[sub_resource_idx];
}

static void wined3d_texture_dirty_region_add(struct wined3d_texture *texture,
        unsigned int layer, const struct wined3d_box *box)
{
    struct wined3d_dirty_regions *regions;
    unsigned int count;

    if (!texture->dirty_regions)
        return;

    regions = &texture->dirty_regions[layer];
    count = regions->box_count + 1;
    if (count >= WINED3D_MAX_DIRTY_REGION_COUNT || !box
            || (!box->left && !box->top && !box->front
            && box->right == texture->resource.width
            && box->bottom == texture->resource.height
            && box->back == texture->resource.depth))
    {
        regions->box_count = WINED3D_MAX_DIRTY_REGION_COUNT;
        return;
    }

    if (!wined3d_array_reserve((void **)&regions->boxes, &regions->boxes_size, count, sizeof(*regions->boxes)))
    {
        WARN("Failed to grow boxes array, marking entire texture dirty.\n");
        regions->box_count = WINED3D_MAX_DIRTY_REGION_COUNT;
        return;
    }

    regions->boxes[regions->box_count++] = *box;
}

HRESULT CDECL wined3d_texture_add_dirty_region(struct wined3d_texture *texture,
        UINT layer, const struct wined3d_box *dirty_region)
{
    TRACE("texture %p, layer %u, dirty_region %s.\n", texture, layer, debug_box(dirty_region));

    if (layer >= texture->layer_count)
    {
        WARN("Invalid layer %u specified.\n", layer);
        return WINED3DERR_INVALIDCALL;
    }

    if (dirty_region && FAILED(wined3d_texture_check_box_dimensions(texture, 0, dirty_region)))
    {
        WARN("Invalid dirty_region %s specified.\n", debug_box(dirty_region));
        return WINED3DERR_INVALIDCALL;
    }

    wined3d_texture_dirty_region_add(texture, layer, dirty_region);
    wined3d_cs_emit_add_dirty_texture_region(texture->resource.device->cs, texture, layer);

    return WINED3D_OK;
}

static void wined3d_texture_gl_upload_bo(const struct wined3d_format *src_format, GLenum target,
        unsigned int level, unsigned int src_row_pitch, unsigned int dst_x, unsigned int dst_y,
        unsigned int dst_z, unsigned int update_w, unsigned int update_h, unsigned int update_d,
        const BYTE *addr, BOOL srgb, struct wined3d_texture *dst_texture,
        const struct wined3d_gl_info *gl_info)
{
    const struct wined3d_format_gl *format_gl = wined3d_format_gl(src_format);

    if (src_format->flags[WINED3D_GL_RES_TYPE_TEX_2D] & WINED3DFMT_FLAG_COMPRESSED)
    {
        unsigned int dst_row_pitch, dst_slice_pitch;
        GLenum internal;

        if (srgb)
            internal = format_gl->srgb_internal;
        else if (dst_texture->resource.bind_flags & WINED3D_BIND_RENDER_TARGET
                && wined3d_resource_is_offscreen(&dst_texture->resource))
            internal = format_gl->rt_internal;
        else
            internal = format_gl->internal;

        wined3d_format_calculate_pitch(src_format, 1, update_w, update_h, &dst_row_pitch, &dst_slice_pitch);

        TRACE("Uploading compressed data, target %#x, level %u, x %u, y %u, z %u, "
                "w %u, h %u, d %u, format %#x, image_size %#x, addr %p.\n",
                target, level, dst_x, dst_y, dst_z, update_w, update_h,
                update_d, internal, dst_slice_pitch, addr);

        if (target == GL_TEXTURE_1D)
        {
            GL_EXTCALL(glCompressedTexSubImage1D(target, level, dst_x,
                    update_w, internal, dst_row_pitch, addr));
        }
        else if (dst_row_pitch == src_row_pitch)
        {
            if (target == GL_TEXTURE_2D_ARRAY || target == GL_TEXTURE_3D)
            {
                GL_EXTCALL(glCompressedTexSubImage3D(target, level, dst_x, dst_y, dst_z,
                        update_w, update_h, update_d, internal, dst_slice_pitch * update_d, addr));
            }
            else
            {
                GL_EXTCALL(glCompressedTexSubImage2D(target, level, dst_x, dst_y,
                        update_w, update_h, internal, dst_slice_pitch, addr));
            }
        }
        else
        {
            unsigned int row_count = (update_h + src_format->block_height - 1) / src_format->block_height;
            unsigned int row, y, z;

            /* glCompressedTexSubImage2D() ignores pixel store state, so we
             * can't use the unpack row length like for glTexSubImage2D. */
            for (z = dst_z; z < dst_z + update_d; ++z)
            {
                for (row = 0, y = dst_y; row < row_count; ++row)
                {
                    if (target == GL_TEXTURE_2D_ARRAY || target == GL_TEXTURE_3D)
                    {
                        GL_EXTCALL(glCompressedTexSubImage3D(target, level, dst_x, y, z,
                                update_w, src_format->block_height, 1, internal, dst_row_pitch, addr));
                    }
                    else
                    {
                        GL_EXTCALL(glCompressedTexSubImage2D(target, level, dst_x, y,
                                update_w, src_format->block_height, internal, dst_row_pitch, addr));
                    }

                    y += src_format->block_height;
                    addr += src_row_pitch;
                }
            }
        }
        checkGLcall("Upload compressed texture data");
    }
    else
    {
        unsigned int y, y_count;

        TRACE("Uploading data, target %#x, level %u, x %u, y %u, z %u, "
                "w %u, h %u, d %u, format %#x, type %#x, addr %p.\n",
                target, level, dst_x, dst_y, dst_z, update_w, update_h,
                update_d, format_gl->format, format_gl->type, addr);

        if (src_row_pitch)
        {
            gl_info->gl_ops.gl.p_glPixelStorei(GL_UNPACK_ROW_LENGTH, src_row_pitch / src_format->byte_count);
            y_count = 1;
        }
        else
        {
            y_count = update_h;
            update_h = 1;
        }

        for (y = 0; y < y_count; ++y)
        {
            if (target == GL_TEXTURE_2D_ARRAY || target == GL_TEXTURE_3D)
            {
                GL_EXTCALL(glTexSubImage3D(target, level, dst_x, dst_y + y, dst_z,
                        update_w, update_h, update_d, format_gl->format, format_gl->type, addr));
            }
            else if (target == GL_TEXTURE_1D)
            {
                gl_info->gl_ops.gl.p_glTexSubImage1D(target, level, dst_x,
                        update_w, format_gl->format, format_gl->type, addr);
            }
            else
            {
                gl_info->gl_ops.gl.p_glTexSubImage2D(target, level, dst_x, dst_y + y,
                        update_w, update_h, format_gl->format, format_gl->type, addr);
            }
        }
        gl_info->gl_ops.gl.p_glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        checkGLcall("Upload texture data");
    }
}

static const struct d3dfmt_alpha_fixup
{
    enum wined3d_format_id format_id, conv_format_id;
}
formats_src_alpha_fixup[] =
{
    {WINED3DFMT_B8G8R8X8_UNORM, WINED3DFMT_B8G8R8A8_UNORM},
    {WINED3DFMT_B5G5R5X1_UNORM, WINED3DFMT_B5G5R5A1_UNORM},
    {WINED3DFMT_B4G4R4X4_UNORM, WINED3DFMT_B4G4R4A4_UNORM},
};

static enum wined3d_format_id wined3d_get_alpha_fixup_format(enum wined3d_format_id format_id,
        const struct wined3d_format *dst_format)
{
    unsigned int i;

    if (!(dst_format->flags[WINED3D_GL_RES_TYPE_TEX_2D] & WINED3DFMT_FLAG_COMPRESSED)
            && !dst_format->alpha_size)
        return WINED3DFMT_UNKNOWN;

    for (i = 0; i < ARRAY_SIZE(formats_src_alpha_fixup); ++i)
    {
        if (formats_src_alpha_fixup[i].format_id == format_id)
            return formats_src_alpha_fixup[i].conv_format_id;
    }

    return WINED3DFMT_UNKNOWN;
}

static void wined3d_fixup_alpha(const struct wined3d_format *format, const uint8_t *src,
        unsigned int src_row_pitch, uint8_t *dst, unsigned int dst_row_pitch,
        unsigned int width, unsigned int height)
{
    unsigned int byte_count, alpha_mask;
    unsigned int x, y;

    byte_count = format->byte_count;
    alpha_mask = ((1u << format->alpha_size) - 1) << format->alpha_offset;

    switch (byte_count)
    {
        case 2:
            for (y = 0; y < height; ++y)
            {
                const uint16_t *src_row = (const uint16_t *)&src[y * src_row_pitch];
                uint16_t *dst_row = (uint16_t *)&dst[y * dst_row_pitch];

                for (x = 0; x < width; ++x)
                {
                    dst_row[x] = src_row[x] | alpha_mask;
                }
            }
            break;

        case 4:
            for (y = 0; y < height; ++y)
            {
                const uint32_t *src_row = (const uint32_t *)&src[y * src_row_pitch];
                uint32_t *dst_row = (uint32_t *)&dst[y * dst_row_pitch];

                for (x = 0; x < width; ++x)
                {
                    dst_row[x] = src_row[x] | alpha_mask;
                }
            }
            break;

        default:
            ERR("Unsupported byte count %u.\n", byte_count);
            break;
    }
}

static void wined3d_texture_gl_upload_data(struct wined3d_context *context,
        const struct wined3d_const_bo_address *src_bo_addr, const struct wined3d_format *src_format,
        const struct wined3d_box *src_box, unsigned int src_row_pitch, unsigned int src_slice_pitch,
        struct wined3d_texture *dst_texture, unsigned int dst_sub_resource_idx, unsigned int dst_location,
        unsigned int dst_x, unsigned int dst_y, unsigned int dst_z)
{
    struct wined3d_context_gl *context_gl = wined3d_context_gl(context);
    enum wined3d_format_id alpha_fixup_format_id = WINED3DFMT_UNKNOWN;
    const struct wined3d_gl_info *gl_info = context_gl->gl_info;
    unsigned int update_w = src_box->right - src_box->left;
    unsigned int update_h = src_box->bottom - src_box->top;
    unsigned int update_d = src_box->back - src_box->front;
    struct wined3d_bo_address bo;
    unsigned int level;
    BOOL srgb = FALSE;
    BOOL decompress;
    GLenum target;

    TRACE("context %p, src_bo_addr %s, src_format %s, src_box %s, src_row_pitch %u, src_slice_pitch %u, "
            "dst_texture %p, dst_sub_resource_idx %u, dst_location %s, dst_x %u, dst_y %u, dst_z %u.\n",
            context, debug_const_bo_address(src_bo_addr), debug_d3dformat(src_format->id), debug_box(src_box),
            src_row_pitch, src_slice_pitch, dst_texture, dst_sub_resource_idx,
            wined3d_debug_location(dst_location), dst_x, dst_y, dst_z);

    if (dst_location == WINED3D_LOCATION_TEXTURE_SRGB)
    {
        srgb = TRUE;
    }
    else if (dst_location != WINED3D_LOCATION_TEXTURE_RGB)
    {
        FIXME("Unhandled location %s.\n", wined3d_debug_location(dst_location));
        return;
    }

    wined3d_texture_gl_bind_and_dirtify(wined3d_texture_gl(dst_texture), wined3d_context_gl(context), srgb);

    if (dst_texture->sub_resources[dst_sub_resource_idx].map_count)
    {
        WARN("Uploading a texture that is currently mapped, setting WINED3D_TEXTURE_PIN_SYSMEM.\n");
        dst_texture->flags |= WINED3D_TEXTURE_PIN_SYSMEM;
    }

    if (src_format->flags[WINED3D_GL_RES_TYPE_TEX_2D] & WINED3DFMT_FLAG_HEIGHT_SCALE)
    {
        update_h *= src_format->height_scale.numerator;
        update_h /= src_format->height_scale.denominator;
    }

    target = wined3d_texture_gl_get_sub_resource_target(wined3d_texture_gl(dst_texture), dst_sub_resource_idx);
    level = dst_sub_resource_idx % dst_texture->level_count;

    switch (target)
    {
        case GL_TEXTURE_1D_ARRAY:
            dst_y = dst_sub_resource_idx / dst_texture->level_count;
            update_h = 1;
            break;
        case GL_TEXTURE_2D_ARRAY:
            dst_z = dst_sub_resource_idx / dst_texture->level_count;
            update_d = 1;
            break;
        case GL_TEXTURE_2D_MULTISAMPLE:
        case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
            FIXME("Not supported for multisample textures.\n");
            return;
    }

    bo.buffer_object = src_bo_addr->buffer_object;
    bo.addr = (BYTE *)src_bo_addr->addr + src_box->front * src_slice_pitch;
    if (dst_texture->resource.format_flags & WINED3DFMT_FLAG_BLOCKS)
    {
        bo.addr += (src_box->top / src_format->block_height) * src_row_pitch;
        bo.addr += (src_box->left / src_format->block_width) * src_format->block_byte_count;
    }
    else
    {
        bo.addr += src_box->top * src_row_pitch;
        bo.addr += src_box->left * src_format->byte_count;
    }

    decompress = (dst_texture->resource.format_flags & WINED3DFMT_FLAG_DECOMPRESS)
            || (src_format->decompress && src_format->id != dst_texture->resource.format->id);

    if (src_format->upload || decompress
            || (alpha_fixup_format_id = wined3d_get_alpha_fixup_format(src_format->id,
            dst_texture->resource.format)) != WINED3DFMT_UNKNOWN)
    {
        const struct wined3d_format *compressed_format = src_format;
        unsigned int dst_row_pitch, dst_slice_pitch;
        struct wined3d_format_gl f;
        void *converted_mem;
        unsigned int z;
        BYTE *src_mem;

        if (decompress)
        {
            src_format = wined3d_resource_get_decompress_format(&dst_texture->resource);
        }
        else if (alpha_fixup_format_id != WINED3DFMT_UNKNOWN)
        {
            src_format = wined3d_get_format(context->device->adapter, alpha_fixup_format_id, 0);
            assert(!!src_format);
        }
        else
        {
            if (dst_texture->resource.format_flags & WINED3DFMT_FLAG_BLOCKS)
                ERR("Converting a block-based format.\n");

            f = *wined3d_format_gl(src_format);
            f.f.byte_count = src_format->conv_byte_count;
            src_format = &f.f;
        }

        wined3d_format_calculate_pitch(src_format, 1, update_w, update_h, &dst_row_pitch, &dst_slice_pitch);

        if (!(converted_mem = heap_alloc(dst_slice_pitch)))
        {
            ERR("Failed to allocate upload buffer.\n");
            return;
        }

        src_mem = wined3d_context_gl_map_bo_address(context_gl, &bo, src_slice_pitch * update_d, WINED3D_MAP_READ);

        for (z = 0; z < update_d; ++z, src_mem += src_slice_pitch)
        {
            if (decompress)
                compressed_format->decompress(src_mem, converted_mem, src_row_pitch, src_slice_pitch,
                        dst_row_pitch, dst_slice_pitch, update_w, update_h, 1);
            else if (alpha_fixup_format_id != WINED3DFMT_UNKNOWN)
                wined3d_fixup_alpha(src_format, src_mem, src_row_pitch, converted_mem, dst_row_pitch,
                        update_w, update_h);
            else
                src_format->upload(src_mem, converted_mem, src_row_pitch, src_slice_pitch,
                        dst_row_pitch, dst_slice_pitch, update_w, update_h, 1);

            wined3d_texture_gl_upload_bo(src_format, target, level, dst_row_pitch, dst_x, dst_y,
                    dst_z + z, update_w, update_h, 1, converted_mem, srgb, dst_texture, gl_info);
        }

        wined3d_context_gl_unmap_bo_address(context_gl, &bo, 0, NULL);
        heap_free(converted_mem);
    }
    else
    {
        if (bo.buffer_object)
        {
            GL_EXTCALL(glBindBuffer(GL_PIXEL_UNPACK_BUFFER, ((struct wined3d_bo_gl *)bo.buffer_object)->id));
            checkGLcall("glBindBuffer");
        }

        wined3d_texture_gl_upload_bo(src_format, target, level, src_row_pitch, dst_x, dst_y,
                dst_z, update_w, update_h, update_d, bo.addr, srgb, dst_texture, gl_info);

        if (bo.buffer_object)
        {
            GL_EXTCALL(glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0));
            checkGLcall("glBindBuffer");
        }
    }

    if (gl_info->quirks & WINED3D_QUIRK_FBO_TEX_UPDATE)
    {
        struct wined3d_device *device = dst_texture->resource.device;
        unsigned int i;

        for (i = 0; i < device->context_count; ++i)
        {
            wined3d_context_gl_texture_update(wined3d_context_gl(device->contexts[i]), wined3d_texture_gl(dst_texture));
        }
    }
}

static void wined3d_texture_gl_download_data_slow_path(struct wined3d_texture_gl *texture_gl,
        unsigned int sub_resource_idx, struct wined3d_context_gl *context_gl, const struct wined3d_bo_address *data)
{
    const struct wined3d_bo_gl *bo = (const struct wined3d_bo_gl *)data->buffer_object;
    const struct wined3d_gl_info *gl_info = context_gl->gl_info;
    struct wined3d_texture_sub_resource *sub_resource;
    unsigned int dst_row_pitch, dst_slice_pitch;
    unsigned int src_row_pitch, src_slice_pitch;
    const struct wined3d_format_gl *format_gl;
    BYTE *temporary_mem = NULL;
    unsigned int level;
    GLenum target;
    void *mem;

    format_gl = wined3d_format_gl(texture_gl->t.resource.format);

    /* Only support read back of converted P8 textures. */
    if (texture_gl->t.flags & WINED3D_TEXTURE_CONVERTED && format_gl->f.id != WINED3DFMT_P8_UINT
            && !format_gl->f.download)
    {
        ERR("Trying to read back converted texture %p, %u with format %s.\n",
                texture_gl, sub_resource_idx, debug_d3dformat(format_gl->f.id));
        return;
    }

    sub_resource = &texture_gl->t.sub_resources[sub_resource_idx];
    target = wined3d_texture_gl_get_sub_resource_target(texture_gl, sub_resource_idx);
    level = sub_resource_idx % texture_gl->t.level_count;

    if (target == GL_TEXTURE_1D_ARRAY || target == GL_TEXTURE_2D_ARRAY)
    {
        if (format_gl->f.download)
        {
            FIXME("Reading back converted array texture %p is not supported.\n", texture_gl);
            return;
        }

        /* NP2 emulation is not allowed on array textures. */
        if (texture_gl->t.flags & WINED3D_TEXTURE_COND_NP2_EMULATED)
            ERR("Array texture %p uses NP2 emulation.\n", texture_gl);

        WARN_(d3d_perf)("Downloading all miplevel layers to get the data for a single sub-resource.\n");

        if (!(temporary_mem = heap_calloc(texture_gl->t.layer_count, sub_resource->size)))
        {
            ERR("Out of memory.\n");
            return;
        }
    }

    if (texture_gl->t.flags & WINED3D_TEXTURE_COND_NP2_EMULATED)
    {
        if (format_gl->f.download)
        {
            FIXME("Reading back converted texture %p with NP2 emulation is not supported.\n", texture_gl);
            return;
        }

        wined3d_texture_get_pitch(&texture_gl->t, level, &dst_row_pitch, &dst_slice_pitch);
        wined3d_format_calculate_pitch(&format_gl->f, texture_gl->t.resource.device->surface_alignment,
                wined3d_texture_get_level_pow2_width(&texture_gl->t, level),
                wined3d_texture_get_level_pow2_height(&texture_gl->t, level),
                &src_row_pitch, &src_slice_pitch);
        if (!(temporary_mem = heap_alloc(src_slice_pitch)))
        {
            ERR("Out of memory.\n");
            return;
        }

        if (bo)
            ERR("NP2 emulated texture uses PBO unexpectedly.\n");
        if (texture_gl->t.resource.format_flags & WINED3DFMT_FLAG_COMPRESSED)
            ERR("Unexpected compressed format for NP2 emulated texture.\n");
    }

    if (format_gl->f.download)
    {
        struct wined3d_format f;

        if (bo)
            ERR("Converted texture %p uses PBO unexpectedly.\n", texture_gl);

        WARN_(d3d_perf)("Downloading converted texture %p, %u with format %s.\n",
                texture_gl, sub_resource_idx, debug_d3dformat(format_gl->f.id));

        f = format_gl->f;
        f.byte_count = format_gl->f.conv_byte_count;
        wined3d_texture_get_pitch(&texture_gl->t, level, &dst_row_pitch, &dst_slice_pitch);
        wined3d_format_calculate_pitch(&f, texture_gl->t.resource.device->surface_alignment,
                wined3d_texture_get_level_width(&texture_gl->t, level),
                wined3d_texture_get_level_height(&texture_gl->t, level),
                &src_row_pitch, &src_slice_pitch);

        if (!(temporary_mem = heap_alloc(src_slice_pitch)))
        {
            ERR("Failed to allocate memory.\n");
            return;
        }
    }

    if (temporary_mem)
    {
        mem = temporary_mem;
    }
    else if (bo)
    {
        GL_EXTCALL(glBindBuffer(GL_PIXEL_PACK_BUFFER, bo->id));
        checkGLcall("glBindBuffer");
        mem = data->addr;
    }
    else
    {
        mem = data->addr;
    }

    if (texture_gl->t.resource.format_flags & WINED3DFMT_FLAG_COMPRESSED)
    {
        TRACE("Downloading compressed texture %p, %u, level %u, format %#x, type %#x, data %p.\n",
                texture_gl, sub_resource_idx, level, format_gl->format, format_gl->type, mem);

        GL_EXTCALL(glGetCompressedTexImage(target, level, mem));
        checkGLcall("glGetCompressedTexImage");
    }
    else
    {
        TRACE("Downloading texture %p, %u, level %u, format %#x, type %#x, data %p.\n",
                texture_gl, sub_resource_idx, level, format_gl->format, format_gl->type, mem);

        gl_info->gl_ops.gl.p_glGetTexImage(target, level, format_gl->format, format_gl->type, mem);
        checkGLcall("glGetTexImage");
    }

    if (format_gl->f.download)
    {
        format_gl->f.download(mem, data->addr, src_row_pitch, src_slice_pitch, dst_row_pitch, dst_slice_pitch,
                wined3d_texture_get_level_width(&texture_gl->t, level),
                wined3d_texture_get_level_height(&texture_gl->t, level), 1);
    }
    else if (texture_gl->t.flags & WINED3D_TEXTURE_COND_NP2_EMULATED)
    {
        const BYTE *src_data;
        unsigned int h, y;
        BYTE *dst_data;
        /* Some games (e.g. Warhammer 40,000) don't properly handle texture
         * pitches, preventing us from using the texture pitch to box NPOT
         * textures. Instead, we repack the texture's CPU copy so that its
         * pitch equals bpp * width instead of bpp * pow2width.
         *
         * Instead of boxing the texture:
         *
         * │<── texture width ──>│ pow2 width ──>│
         * ├─────────────────────┼───────────────┼─
         * │111111111111111111111│               │ʌ
         * │222222222222222222222│               ││
         * │333333333333333333333│    padding    │texture height
         * │444444444444444444444│               ││
         * │555555555555555555555│               │v
         * ├─────────────────────┘               ├─
         * │                                     │pow2 height
         * │       padding            padding    ││
         * │                                     │v
         * └─────────────────────────────────────┴─
         *
         * we're repacking the data to the expected texture width
         *
         * │<── texture width ──>│ pow2 width ──>│
         * ├─────────────────────┴───────────────┼─
         * │1111111111111111111112222222222222222│ʌ
         * │2222233333333333333333333344444444444││
         * │4444444444555555555555555555555      │texture height
         * │                                     ││
         * │        padding       padding        │v
         * │                                     ├─
         * │                                     │pow2 height
         * │        padding       padding        ││
         * │                                     │v
         * └─────────────────────────────────────┴─
         *
         * == is the same as
         *
         * │<── texture width ──>│
         * ├─────────────────────┼─
         * │111111111111111111111│ʌ
         * │222222222222222222222││
         * │333333333333333333333│texture height
         * │444444444444444444444││
         * │555555555555555555555│v
         * └─────────────────────┴─
         *
         * This also means that any references to surface memory should work
         * with the data as if it were a standard texture with a NPOT width
         * instead of a texture boxed up to be a power-of-two texture. */
        src_data = mem;
        dst_data = data->addr;
        TRACE("Repacking the surface data from pitch %u to pitch %u.\n", src_row_pitch, dst_row_pitch);
        h = wined3d_texture_get_level_height(&texture_gl->t, level);
        for (y = 0; y < h; ++y)
        {
            memcpy(dst_data, src_data, dst_row_pitch);
            src_data += src_row_pitch;
            dst_data += dst_row_pitch;
        }
    }
    else if (temporary_mem)
    {
        unsigned int layer = sub_resource_idx / texture_gl->t.level_count;
        void *src_data = temporary_mem + layer * sub_resource->size;
        if (bo)
        {
            GL_EXTCALL(glBindBuffer(GL_PIXEL_PACK_BUFFER, bo->id));
            checkGLcall("glBindBuffer");
            GL_EXTCALL(glBufferSubData(GL_PIXEL_PACK_BUFFER, 0, sub_resource->size, src_data));
            checkGLcall("glBufferSubData");
        }
        else
        {
            memcpy(data->addr, src_data, sub_resource->size);
        }
    }

    if (bo)
    {
        GL_EXTCALL(glBindBuffer(GL_PIXEL_PACK_BUFFER, 0));
        checkGLcall("glBindBuffer");
    }

    heap_free(temporary_mem);
}

static void wined3d_texture_gl_download_data(struct wined3d_context *context,
        struct wined3d_texture *src_texture, unsigned int src_sub_resource_idx, unsigned int src_location,
        const struct wined3d_box *src_box, const struct wined3d_bo_address *dst_bo_addr,
        const struct wined3d_format *dst_format, unsigned int dst_x, unsigned int dst_y, unsigned int dst_z,
        unsigned int dst_row_pitch, unsigned int dst_slice_pitch)
{
    struct wined3d_texture_gl *src_texture_gl = wined3d_texture_gl(src_texture);
    struct wined3d_context_gl *context_gl = wined3d_context_gl(context);
    const struct wined3d_gl_info *gl_info = context_gl->gl_info;
    unsigned int src_level, src_width, src_height, src_depth;
    unsigned int src_row_pitch, src_slice_pitch;
    const struct wined3d_format_gl *format_gl;
    struct wined3d_bo_gl *dst_bo;
    BOOL srgb = FALSE;
    GLenum target;
    struct wined3d_texture_sub_resource *sub_resource;

    TRACE("context %p, src_texture %p, src_sub_resource_idx %u, src_location %s, src_box %s, dst_bo_addr %s, "
            "dst_format %s, dst_x %u, dst_y %u, dst_z %u, dst_row_pitch %u, dst_slice_pitch %u.\n",
            context, src_texture, src_sub_resource_idx, wined3d_debug_location(src_location),
            debug_box(src_box), debug_bo_address(dst_bo_addr), debug_d3dformat(dst_format->id),
            dst_x, dst_y, dst_z, dst_row_pitch, dst_slice_pitch);

    if (src_location == WINED3D_LOCATION_TEXTURE_SRGB)
    {
        srgb = TRUE;
    }
    else if (src_location != WINED3D_LOCATION_TEXTURE_RGB)
    {
        FIXME("Unhandled location %s.\n", wined3d_debug_location(src_location));
        return;
    }

    src_level = src_sub_resource_idx % src_texture->level_count;
    src_width = wined3d_texture_get_level_width(src_texture, src_level);
    src_height = wined3d_texture_get_level_height(src_texture, src_level);
    src_depth = wined3d_texture_get_level_depth(src_texture, src_level);
    if (src_box->left || src_box->top || src_box->right != src_width || src_box->bottom != src_height
            || src_box->front || src_box->back != src_depth)
    {
        FIXME("Unhandled source box %s.\n", debug_box(src_box));
        return;
    }

    if (dst_x || dst_y || dst_z)
    {
        FIXME("Unhandled destination (%u, %u, %u).\n", dst_x, dst_y, dst_z);
        return;
    }

    if (dst_format->id != src_texture->resource.format->id)
    {
        FIXME("Unhandled format conversion (%s -> %s).\n",
                debug_d3dformat(src_texture->resource.format->id),
                debug_d3dformat(dst_format->id));
        return;
    }

    wined3d_texture_get_pitch(src_texture, src_level, &src_row_pitch, &src_slice_pitch);
    if (src_row_pitch != dst_row_pitch || src_slice_pitch != dst_slice_pitch)
    {
        FIXME("Unhandled destination pitches %u/%u (source pitches %u/%u).\n",
                dst_row_pitch, dst_slice_pitch, src_row_pitch, src_slice_pitch);
        return;
    }

    wined3d_texture_gl_bind_and_dirtify(src_texture_gl, context_gl, srgb);

    format_gl = wined3d_format_gl(src_texture->resource.format);
    target = wined3d_texture_gl_get_sub_resource_target(src_texture_gl, src_sub_resource_idx);
    sub_resource = &src_texture->sub_resources[src_sub_resource_idx];

    if ((src_texture->resource.type == WINED3D_RTYPE_TEXTURE_2D
            && (target == GL_TEXTURE_2D_ARRAY || format_gl->f.conv_byte_count
            || src_texture->flags & (WINED3D_TEXTURE_CONVERTED | WINED3D_TEXTURE_COND_NP2_EMULATED)))
            || target == GL_TEXTURE_1D_ARRAY)
    {
        wined3d_texture_gl_download_data_slow_path(src_texture_gl, src_sub_resource_idx, context_gl, dst_bo_addr);
        return;
    }

    if (format_gl->f.conv_byte_count)
    {
        FIXME("Attempting to download a converted texture, type %s format %s.\n",
                debug_d3dresourcetype(src_texture->resource.type),
                debug_d3dformat(format_gl->f.id));
        return;
    }

    if ((dst_bo = (struct wined3d_bo_gl *)dst_bo_addr->buffer_object))
    {
        GL_EXTCALL(glBindBuffer(GL_PIXEL_PACK_BUFFER, dst_bo->id));
        checkGLcall("glBindBuffer");
    }

    if (src_texture->resource.format_flags & WINED3DFMT_FLAG_COMPRESSED)
    {
        TRACE("Downloading compressed texture %p, %u, level %u, format %#x, type %#x, data %p.\n",
                src_texture, src_sub_resource_idx, src_level, format_gl->format, format_gl->type, dst_bo_addr->addr);

        GL_EXTCALL(glGetCompressedTexImage(target, src_level, dst_bo_addr->addr));
        checkGLcall("glGetCompressedTexImage");
    }
    else if (dst_bo_addr->buffer_object && src_texture->resource.bind_flags & WINED3D_BIND_RENDER_TARGET)
    {
        /* PBO texture download is not accelerated on Mesa. Use glReadPixels if possible. */
        TRACE("Downloading (glReadPixels) texture %p, %u, level %u, format %#x, type %#x, data %p.\n",
                src_texture, src_sub_resource_idx, src_level, format_gl->format, format_gl->type, dst_bo_addr->addr);

        wined3d_context_gl_apply_fbo_state_blit(context_gl, GL_READ_FRAMEBUFFER, &src_texture->resource, src_sub_resource_idx, NULL,
                0, sub_resource->locations & (WINED3D_LOCATION_TEXTURE_RGB | WINED3D_LOCATION_TEXTURE_SRGB));
        wined3d_context_gl_check_fbo_status(context_gl, GL_READ_FRAMEBUFFER);
        context_invalidate_state(context, STATE_FRAMEBUFFER);
        gl_info->gl_ops.gl.p_glReadBuffer(GL_COLOR_ATTACHMENT0);
        checkGLcall("glReadBuffer()");

        gl_info->gl_ops.gl.p_glReadPixels(0, 0, wined3d_texture_get_level_width(src_texture, src_level),
                wined3d_texture_get_level_height(src_texture, src_level), format_gl->format, format_gl->type, dst_bo_addr->addr);
        checkGLcall("glReadPixels");
    }
    else
    {
        TRACE("Downloading texture %p, %u, level %u, format %#x, type %#x, data %p.\n",
                src_texture, src_sub_resource_idx, src_level, format_gl->format, format_gl->type, dst_bo_addr->addr);

        gl_info->gl_ops.gl.p_glGetTexImage(target, src_level, format_gl->format, format_gl->type, dst_bo_addr->addr);
        checkGLcall("glGetTexImage");
    }

    if (dst_bo)
    {
        GL_EXTCALL(glBindBuffer(GL_PIXEL_PACK_BUFFER, 0));
        checkGLcall("glBindBuffer");
    }
}

/* Context activation is done by the caller. */
static BOOL wined3d_texture_gl_load_sysmem(struct wined3d_texture_gl *texture_gl,
        unsigned int sub_resource_idx, struct wined3d_context_gl *context_gl, DWORD dst_location)
{
    struct wined3d_texture_sub_resource *sub_resource;

    sub_resource = &texture_gl->t.sub_resources[sub_resource_idx];

    /* We cannot download data from multisample textures directly. */
    if (wined3d_texture_gl_is_multisample_location(texture_gl, WINED3D_LOCATION_TEXTURE_RGB))
    {
        wined3d_texture_load_location(&texture_gl->t, sub_resource_idx, &context_gl->c, WINED3D_LOCATION_RB_RESOLVED);
        texture2d_read_from_framebuffer(&texture_gl->t, sub_resource_idx, &context_gl->c,
                WINED3D_LOCATION_RB_RESOLVED, dst_location);
        return TRUE;
    }

    if (sub_resource->locations & (WINED3D_LOCATION_RB_MULTISAMPLE | WINED3D_LOCATION_RB_RESOLVED))
        wined3d_texture_load_location(&texture_gl->t, sub_resource_idx, &context_gl->c, WINED3D_LOCATION_TEXTURE_RGB);

    /* Download the sub-resource to system memory. */
    if (sub_resource->locations & (WINED3D_LOCATION_TEXTURE_RGB | WINED3D_LOCATION_TEXTURE_SRGB))
    {
        unsigned int row_pitch, slice_pitch, level;
        struct wined3d_bo_address data;
        struct wined3d_box src_box;
        unsigned int src_location;

        level = sub_resource_idx % texture_gl->t.level_count;
        wined3d_texture_get_memory(&texture_gl->t, sub_resource_idx, &data, dst_location);
        src_location = sub_resource->locations & WINED3D_LOCATION_TEXTURE_RGB
                ? WINED3D_LOCATION_TEXTURE_RGB : WINED3D_LOCATION_TEXTURE_SRGB;
        wined3d_texture_get_level_box(&texture_gl->t, level, &src_box);
        wined3d_texture_get_pitch(&texture_gl->t, level, &row_pitch, &slice_pitch);
        wined3d_texture_gl_download_data(&context_gl->c, &texture_gl->t, sub_resource_idx, src_location,
                &src_box, &data, texture_gl->t.resource.format, 0, 0, 0, row_pitch, slice_pitch);

        ++texture_gl->t.download_count;
        return TRUE;
    }

    if (!(texture_gl->t.resource.bind_flags & WINED3D_BIND_DEPTH_STENCIL)
            && (sub_resource->locations & WINED3D_LOCATION_DRAWABLE))
    {
        texture2d_read_from_framebuffer(&texture_gl->t, sub_resource_idx, &context_gl->c,
                texture_gl->t.resource.draw_binding, dst_location);
        return TRUE;
    }

    FIXME("Can't load texture %p, %u with location flags %s into sysmem.\n",
            texture_gl, sub_resource_idx, wined3d_debug_location(sub_resource->locations));

    return FALSE;
}

static BOOL wined3d_texture_load_drawable(struct wined3d_texture *texture,
        unsigned int sub_resource_idx, struct wined3d_context *context)
{
    struct wined3d_device *device;
    unsigned int level;
    RECT r;

    if (texture->resource.bind_flags & WINED3D_BIND_DEPTH_STENCIL)
    {
        DWORD current = texture->sub_resources[sub_resource_idx].locations;
        FIXME("Unimplemented copy from %s for depth/stencil buffers.\n",
                wined3d_debug_location(current));
        return FALSE;
    }

    if (wined3d_settings.offscreen_rendering_mode == ORM_FBO
            && wined3d_resource_is_offscreen(&texture->resource))
    {
        ERR("Trying to load offscreen texture into WINED3D_LOCATION_DRAWABLE.\n");
        return FALSE;
    }

    device = texture->resource.device;
    level = sub_resource_idx % texture->level_count;
    SetRect(&r, 0, 0, wined3d_texture_get_level_width(texture, level),
            wined3d_texture_get_level_height(texture, level));
    wined3d_texture_load_location(texture, sub_resource_idx, context, WINED3D_LOCATION_TEXTURE_RGB);
    device->blitter->ops->blitter_blit(device->blitter, WINED3D_BLIT_OP_COLOR_BLIT, context,
            texture, sub_resource_idx, WINED3D_LOCATION_TEXTURE_RGB, &r,
            texture, sub_resource_idx, WINED3D_LOCATION_DRAWABLE, &r,
            NULL, WINED3D_TEXF_POINT);

    return TRUE;
}

static BOOL wined3d_texture_load_renderbuffer(struct wined3d_texture *texture,
        unsigned int sub_resource_idx, struct wined3d_context *context, DWORD dst_location)
{
    unsigned int level = sub_resource_idx % texture->level_count;
    const RECT rect = {0, 0,
            wined3d_texture_get_level_width(texture, level),
            wined3d_texture_get_level_height(texture, level)};
    struct wined3d_texture_sub_resource *sub_resource;
    DWORD src_location, locations;

    sub_resource = &texture->sub_resources[sub_resource_idx];
    locations = sub_resource->locations;
    if (texture->resource.bind_flags & WINED3D_BIND_DEPTH_STENCIL)
    {
        FIXME("Unimplemented copy from %s for depth/stencil buffers.\n",
                wined3d_debug_location(locations));
        return FALSE;
    }

    if (locations & WINED3D_LOCATION_RB_MULTISAMPLE)
        src_location = WINED3D_LOCATION_RB_MULTISAMPLE;
    else if (locations & WINED3D_LOCATION_RB_RESOLVED)
        src_location = WINED3D_LOCATION_RB_RESOLVED;
    else if (locations & WINED3D_LOCATION_TEXTURE_SRGB)
        src_location = WINED3D_LOCATION_TEXTURE_SRGB;
    else if (locations & WINED3D_LOCATION_TEXTURE_RGB)
        src_location = WINED3D_LOCATION_TEXTURE_RGB;
    else if (locations & WINED3D_LOCATION_DRAWABLE)
        src_location = WINED3D_LOCATION_DRAWABLE;
    else /* texture2d_blt_fbo() will load the source location if necessary. */
        src_location = WINED3D_LOCATION_TEXTURE_RGB;

    texture2d_blt_fbo(texture->resource.device, context, WINED3D_TEXF_POINT, texture,
            sub_resource_idx, src_location, &rect, texture, sub_resource_idx, dst_location, &rect);

    return TRUE;
}

static BOOL wined3d_texture_gl_load_texture(struct wined3d_texture_gl *texture_gl,
        unsigned int sub_resource_idx, struct wined3d_context_gl *context_gl, BOOL srgb)
{
    unsigned int width, height, level, src_row_pitch, src_slice_pitch, dst_row_pitch, dst_slice_pitch;
    struct wined3d_device *device = texture_gl->t.resource.device;
    const struct wined3d_gl_info *gl_info = context_gl->gl_info;
    const struct wined3d_color_key_conversion *conversion;
    struct wined3d_texture_sub_resource *sub_resource;
    const struct wined3d_format *format;
    struct wined3d_bo_address data;
    BYTE *src_mem, *dst_mem = NULL;
    struct wined3d_box src_box;
    DWORD dst_location;
    BOOL depth;

    depth = texture_gl->t.resource.bind_flags & WINED3D_BIND_DEPTH_STENCIL;
    sub_resource = &texture_gl->t.sub_resources[sub_resource_idx];

    if (!depth && wined3d_settings.offscreen_rendering_mode != ORM_FBO
            && wined3d_resource_is_offscreen(&texture_gl->t.resource)
            && (sub_resource->locations & WINED3D_LOCATION_DRAWABLE))
    {
        texture2d_load_fb_texture(texture_gl, sub_resource_idx, srgb, &context_gl->c);

        return TRUE;
    }

    level = sub_resource_idx % texture_gl->t.level_count;
    wined3d_texture_get_level_box(&texture_gl->t, level, &src_box);

    if (!depth && sub_resource->locations & (WINED3D_LOCATION_TEXTURE_SRGB | WINED3D_LOCATION_TEXTURE_RGB)
            && (texture_gl->t.resource.format_flags & WINED3DFMT_FLAG_FBO_ATTACHABLE_SRGB)
            && fbo_blitter_supported(WINED3D_BLIT_OP_COLOR_BLIT, gl_info,
                    &texture_gl->t.resource, WINED3D_LOCATION_TEXTURE_RGB,
                    &texture_gl->t.resource, WINED3D_LOCATION_TEXTURE_SRGB))
    {
        RECT src_rect;

        SetRect(&src_rect, src_box.left, src_box.top, src_box.right, src_box.bottom);
        if (srgb)
            texture2d_blt_fbo(device, &context_gl->c, WINED3D_TEXF_POINT,
                    &texture_gl->t, sub_resource_idx, WINED3D_LOCATION_TEXTURE_RGB, &src_rect,
                    &texture_gl->t, sub_resource_idx, WINED3D_LOCATION_TEXTURE_SRGB, &src_rect);
        else
            texture2d_blt_fbo(device, &context_gl->c, WINED3D_TEXF_POINT,
                    &texture_gl->t, sub_resource_idx, WINED3D_LOCATION_TEXTURE_SRGB, &src_rect,
                    &texture_gl->t, sub_resource_idx, WINED3D_LOCATION_TEXTURE_RGB, &src_rect);

        return TRUE;
    }

    if (!depth && sub_resource->locations & (WINED3D_LOCATION_RB_MULTISAMPLE | WINED3D_LOCATION_RB_RESOLVED)
            && (!srgb || (texture_gl->t.resource.format_flags & WINED3DFMT_FLAG_FBO_ATTACHABLE_SRGB)))
    {
        DWORD src_location = sub_resource->locations & WINED3D_LOCATION_RB_RESOLVED ?
                WINED3D_LOCATION_RB_RESOLVED : WINED3D_LOCATION_RB_MULTISAMPLE;
        RECT src_rect;

        SetRect(&src_rect, src_box.left, src_box.top, src_box.right, src_box.bottom);
        dst_location = srgb ? WINED3D_LOCATION_TEXTURE_SRGB : WINED3D_LOCATION_TEXTURE_RGB;
        if (fbo_blitter_supported(WINED3D_BLIT_OP_COLOR_BLIT, gl_info,
                &texture_gl->t.resource, src_location, &texture_gl->t.resource, dst_location))
            texture2d_blt_fbo(device, &context_gl->c, WINED3D_TEXF_POINT, &texture_gl->t, sub_resource_idx,
                    src_location, &src_rect, &texture_gl->t, sub_resource_idx, dst_location, &src_rect);

        return TRUE;
    }

    /* Upload from system memory */

    if (srgb)
    {
        dst_location = WINED3D_LOCATION_TEXTURE_SRGB;
        if ((sub_resource->locations & (WINED3D_LOCATION_TEXTURE_RGB | texture_gl->t.resource.map_binding))
                == WINED3D_LOCATION_TEXTURE_RGB)
        {
            FIXME_(d3d_perf)("Downloading RGB texture %p, %u to reload it as sRGB.\n", texture_gl, sub_resource_idx);
            wined3d_texture_load_location(&texture_gl->t, sub_resource_idx,
                    &context_gl->c, texture_gl->t.resource.map_binding);
        }
    }
    else
    {
        dst_location = WINED3D_LOCATION_TEXTURE_RGB;
        if ((sub_resource->locations & (WINED3D_LOCATION_TEXTURE_SRGB | texture_gl->t.resource.map_binding))
                == WINED3D_LOCATION_TEXTURE_SRGB)
        {
            FIXME_(d3d_perf)("Downloading sRGB texture %p, %u to reload it as RGB.\n", texture_gl, sub_resource_idx);
            wined3d_texture_load_location(&texture_gl->t, sub_resource_idx,
                    &context_gl->c, texture_gl->t.resource.map_binding);
        }
    }

    if (!(sub_resource->locations & wined3d_texture_sysmem_locations))
    {
        WARN("Trying to load a texture from sysmem, but no simple location is valid.\n");
        /* Lets hope we get it from somewhere... */
        wined3d_texture_load_location(&texture_gl->t, sub_resource_idx, &context_gl->c, WINED3D_LOCATION_SYSMEM);
    }

    wined3d_texture_get_pitch(&texture_gl->t, level, &src_row_pitch, &src_slice_pitch);

    format = texture_gl->t.resource.format;
    if ((conversion = wined3d_format_get_color_key_conversion(&texture_gl->t, TRUE)))
        format = wined3d_get_format(device->adapter, conversion->dst_format, texture_gl->t.resource.bind_flags);

    /* Don't use PBOs for converted surfaces. During PBO conversion we look at
     * WINED3D_TEXTURE_CONVERTED but it isn't set (yet) in all cases it is
     * getting called. */
    if (conversion && sub_resource->bo.id)
    {
        TRACE("Removing the pbo attached to texture %p, %u.\n", texture_gl, sub_resource_idx);

        wined3d_texture_load_location(&texture_gl->t, sub_resource_idx, &context_gl->c, WINED3D_LOCATION_SYSMEM);
        wined3d_texture_set_map_binding(&texture_gl->t, WINED3D_LOCATION_SYSMEM);
    }

    wined3d_texture_get_memory(&texture_gl->t, sub_resource_idx, &data, sub_resource->locations);
    if (conversion)
    {
        width = src_box.right - src_box.left;
        height = src_box.bottom - src_box.top;
        wined3d_format_calculate_pitch(format, device->surface_alignment,
                width, height, &dst_row_pitch, &dst_slice_pitch);

        src_mem = wined3d_context_gl_map_bo_address(context_gl, &data, src_slice_pitch, WINED3D_MAP_READ);
        if (!(dst_mem = heap_alloc(dst_slice_pitch)))
        {
            ERR("Out of memory (%u).\n", dst_slice_pitch);
            return FALSE;
        }
        conversion->convert(src_mem, src_row_pitch, dst_mem, dst_row_pitch,
                width, height, &texture_gl->t.async.gl_color_key);
        src_row_pitch = dst_row_pitch;
        src_slice_pitch = dst_slice_pitch;
        wined3d_context_gl_unmap_bo_address(context_gl, &data, 0, NULL);

        data.buffer_object = 0;
        data.addr = dst_mem;
    }

    wined3d_texture_gl_upload_data(&context_gl->c, wined3d_const_bo_address(&data), format, &src_box,
            src_row_pitch, src_slice_pitch, &texture_gl->t, sub_resource_idx, dst_location, 0, 0, 0);

    heap_free(dst_mem);

    return TRUE;
}

static BOOL wined3d_texture_gl_prepare_location(struct wined3d_texture *texture,
        unsigned int sub_resource_idx, struct wined3d_context *context, unsigned int location)
{
    struct wined3d_texture_gl *texture_gl = wined3d_texture_gl(texture);
    struct wined3d_context_gl *context_gl = wined3d_context_gl(context);

    switch (location)
    {
        case WINED3D_LOCATION_SYSMEM:
            return texture->sub_resources[sub_resource_idx].user_memory ? TRUE
                    : wined3d_resource_prepare_sysmem(&texture->resource);

        case WINED3D_LOCATION_BUFFER:
            wined3d_texture_prepare_buffer_object(texture, sub_resource_idx, context_gl->gl_info);
            return TRUE;

        case WINED3D_LOCATION_TEXTURE_RGB:
            wined3d_texture_gl_prepare_texture(texture_gl, context_gl, FALSE);
            return TRUE;

        case WINED3D_LOCATION_TEXTURE_SRGB:
            wined3d_texture_gl_prepare_texture(texture_gl, context_gl, TRUE);
            return TRUE;

        case WINED3D_LOCATION_DRAWABLE:
            if (!texture->swapchain && wined3d_settings.offscreen_rendering_mode != ORM_BACKBUFFER)
                ERR("Texture %p does not have a drawable.\n", texture);
            return TRUE;

        case WINED3D_LOCATION_RB_MULTISAMPLE:
            wined3d_texture_gl_prepare_rb(texture_gl, context_gl->gl_info, TRUE);
            return TRUE;

        case WINED3D_LOCATION_RB_RESOLVED:
            wined3d_texture_gl_prepare_rb(texture_gl, context_gl->gl_info, FALSE);
            return TRUE;

        default:
            ERR("Invalid location %s.\n", wined3d_debug_location(location));
            return FALSE;
    }
}

/* Context activation is done by the caller. */
static BOOL wined3d_texture_gl_load_location(struct wined3d_texture *texture,
        unsigned int sub_resource_idx, struct wined3d_context *context, DWORD location)
{
    struct wined3d_texture_gl *texture_gl = wined3d_texture_gl(texture);
    struct wined3d_context_gl *context_gl = wined3d_context_gl(context);

    TRACE("texture %p, sub_resource_idx %u, context %p, location %s.\n",
            texture, sub_resource_idx, context, wined3d_debug_location(location));

    if (!wined3d_texture_gl_prepare_location(texture, sub_resource_idx, context, location))
        return FALSE;

    switch (location)
    {
        case WINED3D_LOCATION_SYSMEM:
        case WINED3D_LOCATION_BUFFER:
            return wined3d_texture_gl_load_sysmem(texture_gl, sub_resource_idx, context_gl, location);

        case WINED3D_LOCATION_DRAWABLE:
            return wined3d_texture_load_drawable(texture, sub_resource_idx, context);

        case WINED3D_LOCATION_RB_RESOLVED:
        case WINED3D_LOCATION_RB_MULTISAMPLE:
            return wined3d_texture_load_renderbuffer(texture, sub_resource_idx, context, location);

        case WINED3D_LOCATION_TEXTURE_RGB:
        case WINED3D_LOCATION_TEXTURE_SRGB:
            return wined3d_texture_gl_load_texture(texture_gl, sub_resource_idx,
                    context_gl, location == WINED3D_LOCATION_TEXTURE_SRGB);

        default:
            FIXME("Unhandled %s load from %s.\n", wined3d_debug_location(location),
                    wined3d_debug_location(texture->sub_resources[sub_resource_idx].locations));
            return FALSE;
    }
}

static void wined3d_texture_gl_unload_location(struct wined3d_texture *texture,
        struct wined3d_context *context, unsigned int location)
{
    struct wined3d_texture_gl *texture_gl = wined3d_texture_gl(texture);
    struct wined3d_context_gl *context_gl = wined3d_context_gl(context);
    struct wined3d_renderbuffer_entry *entry, *entry2;
    unsigned int i, sub_count;

    TRACE("texture %p, context %p, location %s.\n", texture, context, wined3d_debug_location(location));

    switch (location)
    {
        case WINED3D_LOCATION_BUFFER:
            sub_count = texture->level_count * texture->layer_count;
            for (i = 0; i < sub_count; ++i)
            {
                if (texture_gl->t.sub_resources[i].bo.id)
                    wined3d_texture_remove_buffer_object(&texture_gl->t, i, context_gl->gl_info);
            }
            break;

        case WINED3D_LOCATION_TEXTURE_RGB:
            if (texture_gl->texture_rgb.name)
                gltexture_delete(texture_gl->t.resource.device, context_gl->gl_info, &texture_gl->texture_rgb);
            break;

        case WINED3D_LOCATION_TEXTURE_SRGB:
            if (texture_gl->texture_srgb.name)
                gltexture_delete(texture_gl->t.resource.device, context_gl->gl_info, &texture_gl->texture_srgb);
            break;

        case WINED3D_LOCATION_RB_MULTISAMPLE:
            if (texture_gl->rb_multisample)
            {
                TRACE("Deleting multisample renderbuffer %u.\n", texture_gl->rb_multisample);
                context_gl_resource_released(texture_gl->t.resource.device, texture_gl->rb_multisample, TRUE);
                context_gl->gl_info->fbo_ops.glDeleteRenderbuffers(1, &texture_gl->rb_multisample);
                texture_gl->rb_multisample = 0;
            }
            break;

        case WINED3D_LOCATION_RB_RESOLVED:
            LIST_FOR_EACH_ENTRY_SAFE(entry, entry2, &texture_gl->renderbuffers,
                    struct wined3d_renderbuffer_entry, entry)
            {
                context_gl_resource_released(texture_gl->t.resource.device, entry->id, TRUE);
                context_gl->gl_info->fbo_ops.glDeleteRenderbuffers(1, &entry->id);
                list_remove(&entry->entry);
                heap_free(entry);
            }
            list_init(&texture_gl->renderbuffers);
            texture_gl->current_renderbuffer = NULL;

            if (texture_gl->rb_resolved)
            {
                TRACE("Deleting resolved renderbuffer %u.\n", texture_gl->rb_resolved);
                context_gl_resource_released(texture_gl->t.resource.device, texture_gl->rb_resolved, TRUE);
                context_gl->gl_info->fbo_ops.glDeleteRenderbuffers(1, &texture_gl->rb_resolved);
                texture_gl->rb_resolved = 0;
            }
            break;

        default:
            ERR("Unhandled location %s.\n", wined3d_debug_location(location));
            break;
    }
}

static const struct wined3d_texture_ops texture_gl_ops =
{
    wined3d_texture_gl_prepare_location,
    wined3d_texture_gl_load_location,
    wined3d_texture_gl_unload_location,
    wined3d_texture_gl_upload_data,
    wined3d_texture_gl_download_data,
};

struct wined3d_texture * __cdecl wined3d_texture_from_resource(struct wined3d_resource *resource)
{
    return texture_from_resource(resource);
}

static ULONG texture_resource_incref(struct wined3d_resource *resource)
{
    return wined3d_texture_incref(texture_from_resource(resource));
}

static ULONG texture_resource_decref(struct wined3d_resource *resource)
{
    return wined3d_texture_decref(texture_from_resource(resource));
}

static void texture_resource_preload(struct wined3d_resource *resource)
{
    struct wined3d_texture *texture = texture_from_resource(resource);
    struct wined3d_context *context;

    context = context_acquire(resource->device, NULL, 0);
    wined3d_texture_load(texture, context, texture->flags & WINED3D_TEXTURE_IS_SRGB);
    context_release(context);
}

static void texture_resource_unload(struct wined3d_resource *resource)
{
    struct wined3d_texture *texture = texture_from_resource(resource);
    struct wined3d_device *device = resource->device;
    unsigned int location = resource->map_binding;
    struct wined3d_context *context;
    unsigned int sub_count, i;

    TRACE("resource %p.\n", resource);

    /* D3D is not initialised, so no GPU locations should currently exist.
     * Moreover, we may not be able to acquire a valid context. */
    if (!device->d3d_initialized)
        return;

    context = context_acquire(device, NULL, 0);

    if (location == WINED3D_LOCATION_BUFFER)
        location = WINED3D_LOCATION_SYSMEM;

    sub_count = texture->level_count * texture->layer_count;
    for (i = 0; i < sub_count; ++i)
    {
        if (resource->access & WINED3D_RESOURCE_ACCESS_CPU
                && wined3d_texture_load_location(texture, i, context, location))
        {
            wined3d_texture_invalidate_location(texture, i, ~location);
        }
        else
        {
            if (resource->access & WINED3D_RESOURCE_ACCESS_CPU)
                ERR("Discarding %s %p sub-resource %u with resource access %s.\n",
                        debug_d3dresourcetype(resource->type), resource, i,
                        wined3d_debug_resource_access(resource->access));
            wined3d_texture_validate_location(texture, i, WINED3D_LOCATION_DISCARDED);
            wined3d_texture_invalidate_location(texture, i, ~WINED3D_LOCATION_DISCARDED);
        }
    }

    wined3d_texture_unload_location(texture, context, WINED3D_LOCATION_BUFFER);
    wined3d_texture_unload_location(texture, context, WINED3D_LOCATION_TEXTURE_RGB);
    wined3d_texture_unload_location(texture, context, WINED3D_LOCATION_TEXTURE_SRGB);
    wined3d_texture_unload_location(texture, context, WINED3D_LOCATION_RB_MULTISAMPLE);
    wined3d_texture_unload_location(texture, context, WINED3D_LOCATION_RB_RESOLVED);

    context_release(context);

    wined3d_texture_force_reload(texture);
    if (texture->resource.bind_count)
        device_invalidate_state(device, STATE_SAMPLER(texture->sampler));
    wined3d_texture_set_dirty(texture);

    resource_unload(&texture->resource);
}

static HRESULT texture_resource_sub_resource_map(struct wined3d_resource *resource, unsigned int sub_resource_idx,
        struct wined3d_map_desc *map_desc, const struct wined3d_box *box, DWORD flags)
{
    const struct wined3d_format *format = resource->format;
    struct wined3d_texture_sub_resource *sub_resource;
    struct wined3d_device *device = resource->device;
    unsigned int fmt_flags = resource->format_flags;
    struct wined3d_context *context;
    struct wined3d_texture *texture;
    struct wined3d_bo_address data;
    unsigned int texture_level;
    BYTE *base_memory;
    BOOL ret;

    TRACE("resource %p, sub_resource_idx %u, map_desc %p, box %s, flags %#x.\n",
            resource, sub_resource_idx, map_desc, debug_box(box), flags);

    texture = texture_from_resource(resource);
    if (!(sub_resource = wined3d_texture_get_sub_resource(texture, sub_resource_idx)))
        return E_INVALIDARG;

    texture_level = sub_resource_idx % texture->level_count;
    if (box && FAILED(wined3d_texture_check_box_dimensions(texture, texture_level, box)))
    {
        WARN("Map box is invalid.\n");
        if (((fmt_flags & WINED3DFMT_FLAG_BLOCKS) && !(resource->access & WINED3D_RESOURCE_ACCESS_CPU))
                || resource->type != WINED3D_RTYPE_TEXTURE_2D)
            return WINED3DERR_INVALIDCALL;
    }

    if (texture->flags & WINED3D_TEXTURE_DC_IN_USE)
    {
        WARN("DC is in use.\n");
        return WINED3DERR_INVALIDCALL;
    }

    if (sub_resource->map_count)
    {
        WARN("Sub-resource is already mapped.\n");
        return WINED3DERR_INVALIDCALL;
    }

    context = context_acquire(device, NULL, 0);

    if (flags & WINED3D_MAP_DISCARD)
    {
        TRACE("WINED3D_MAP_DISCARD flag passed, marking %s as up to date.\n",
                wined3d_debug_location(resource->map_binding));
        if ((ret = wined3d_texture_prepare_location(texture, sub_resource_idx, context, resource->map_binding)))
            wined3d_texture_validate_location(texture, sub_resource_idx, resource->map_binding);
    }
    else
    {
        if (resource->usage & WINED3DUSAGE_DYNAMIC)
            WARN_(d3d_perf)("Mapping a dynamic texture without WINED3D_MAP_DISCARD.\n");
        ret = wined3d_texture_load_location(texture, sub_resource_idx, context, resource->map_binding);
    }

    if (!ret)
    {
        ERR("Failed to prepare location.\n");
        context_release(context);
        return E_OUTOFMEMORY;
    }

    /* We only record dirty regions for the top-most level. */
    if (texture->dirty_regions && flags & WINED3D_MAP_WRITE
            && !(flags & WINED3D_MAP_NO_DIRTY_UPDATE) && !texture_level)
        wined3d_texture_dirty_region_add(texture, sub_resource_idx / texture->level_count, box);

    if (flags & WINED3D_MAP_WRITE
            && (!(flags & WINED3D_MAP_NO_DIRTY_UPDATE) || (resource->usage & WINED3DUSAGE_DYNAMIC)))
        wined3d_texture_invalidate_location(texture, sub_resource_idx, ~resource->map_binding);

    wined3d_texture_get_memory(texture, sub_resource_idx, &data, resource->map_binding);
    base_memory = wined3d_context_map_bo_address(context, &data, sub_resource->size, flags);
    sub_resource->map_flags = flags;
    TRACE("Base memory pointer %p.\n", base_memory);

    context_release(context);

    if (fmt_flags & WINED3DFMT_FLAG_BROKEN_PITCH)
    {
        map_desc->row_pitch = wined3d_texture_get_level_width(texture, texture_level) * format->byte_count;
        map_desc->slice_pitch = wined3d_texture_get_level_height(texture, texture_level) * map_desc->row_pitch;
    }
    else
    {
        wined3d_texture_get_pitch(texture, texture_level, &map_desc->row_pitch, &map_desc->slice_pitch);
    }

    if (!box)
    {
        map_desc->data = base_memory;
    }
    else
    {
        if ((fmt_flags & (WINED3DFMT_FLAG_BLOCKS | WINED3DFMT_FLAG_BROKEN_PITCH)) == WINED3DFMT_FLAG_BLOCKS)
        {
            /* Compressed textures are block based, so calculate the offset of
             * the block that contains the top-left pixel of the mapped box. */
            map_desc->data = base_memory
                    + (box->front * map_desc->slice_pitch)
                    + ((box->top / format->block_height) * map_desc->row_pitch)
                    + ((box->left / format->block_width) * format->block_byte_count);
        }
        else
        {
            map_desc->data = base_memory
                    + (box->front * map_desc->slice_pitch)
                    + (box->top * map_desc->row_pitch)
                    + (box->left * format->byte_count);
        }
    }

    if (texture->swapchain && texture->swapchain->front_buffer == texture)
    {
        RECT *r = &texture->swapchain->front_buffer_update;

        if (!box)
            SetRect(r, 0, 0, resource->width, resource->height);
        else
            SetRect(r, box->left, box->top, box->right, box->bottom);
        TRACE("Mapped front buffer %s.\n", wine_dbgstr_rect(r));
    }

    ++resource->map_count;
    ++sub_resource->map_count;

    TRACE("Returning memory %p, row pitch %u, slice pitch %u.\n",
            map_desc->data, map_desc->row_pitch, map_desc->slice_pitch);

    return WINED3D_OK;
}

static HRESULT texture_resource_sub_resource_map_info(struct wined3d_resource *resource, unsigned int sub_resource_idx,
        struct wined3d_map_info *info, DWORD flags)
{
    const struct wined3d_format *format = resource->format;
    struct wined3d_texture_sub_resource *sub_resource;
    unsigned int fmt_flags = resource->format_flags;
    struct wined3d_texture *texture;
    unsigned int texture_level;

    texture = texture_from_resource(resource);
    if (!(sub_resource = wined3d_texture_get_sub_resource(texture, sub_resource_idx)))
        return E_INVALIDARG;

    texture_level = sub_resource_idx % texture->level_count;

    if (fmt_flags & WINED3DFMT_FLAG_BROKEN_PITCH)
    {
        info->row_pitch = wined3d_texture_get_level_width(texture, texture_level) * format->byte_count;
        info->slice_pitch = wined3d_texture_get_level_height(texture, texture_level) * info->row_pitch;
    }
    else
    {
        wined3d_texture_get_pitch(texture, texture_level, &info->row_pitch, &info->slice_pitch);
    }

    info->size = info->slice_pitch * wined3d_texture_get_level_depth(texture, texture_level);

    return WINED3D_OK;
}

static HRESULT texture_resource_sub_resource_unmap(struct wined3d_resource *resource, unsigned int sub_resource_idx)
{
    struct wined3d_texture_sub_resource *sub_resource;
    struct wined3d_device *device = resource->device;
    struct wined3d_context *context;
    struct wined3d_texture *texture;
    struct wined3d_bo_address data;
    struct wined3d_range range;

    TRACE("resource %p, sub_resource_idx %u.\n", resource, sub_resource_idx);

    texture = texture_from_resource(resource);
    if (!(sub_resource = wined3d_texture_get_sub_resource(texture, sub_resource_idx)))
        return E_INVALIDARG;

    if (!sub_resource->map_count)
    {
        WARN("Trying to unmap unmapped sub-resource.\n");
        if (texture->flags & WINED3D_TEXTURE_DC_IN_USE)
            return WINED3D_OK;
        return WINEDDERR_NOTLOCKED;
    }

    context = context_acquire(device, NULL, 0);

    wined3d_texture_get_memory(texture, sub_resource_idx, &data, texture->resource.map_binding);
    range.offset = 0;
    range.size = sub_resource->size;
    wined3d_context_unmap_bo_address(context, &data, !!(sub_resource->map_flags & WINED3D_MAP_WRITE), &range);

    context_release(context);

    if (texture->swapchain && texture->swapchain->front_buffer == texture)
    {
        if (!(sub_resource->locations & (WINED3D_LOCATION_DRAWABLE | WINED3D_LOCATION_TEXTURE_RGB)))
            texture->swapchain->swapchain_ops->swapchain_frontbuffer_updated(texture->swapchain);
    }

    --sub_resource->map_count;
    if (!--resource->map_count && texture->update_map_binding)
        wined3d_texture_update_map_binding(texture);

    return WINED3D_OK;
}

static const struct wined3d_resource_ops texture_resource_ops =
{
    texture_resource_incref,
    texture_resource_decref,
    texture_resource_preload,
    texture_resource_unload,
    texture_resource_sub_resource_map,
    texture_resource_sub_resource_map_info,
    texture_resource_sub_resource_unmap,
};

static HRESULT wined3d_texture_init(struct wined3d_texture *texture, const struct wined3d_resource_desc *desc,
        unsigned int layer_count, unsigned int level_count, DWORD flags, struct wined3d_device *device,
        void *parent, const struct wined3d_parent_ops *parent_ops, void *sub_resources,
        const struct wined3d_texture_ops *texture_ops)
{
    const struct wined3d_d3d_info *d3d_info = &device->adapter->d3d_info;
    struct wined3d_device_parent *device_parent = device->device_parent;
    const struct wined3d_gl_info *gl_info = &device->adapter->gl_info;
    unsigned int sub_count, i, j, size, offset = 0;
    unsigned int pow2_width, pow2_height;
    const struct wined3d_format *format;
    HRESULT hr;

    TRACE("texture %p, resource_type %s, format %s, multisample_type %#x, multisample_quality %#x, "
            "usage %s, access %s, width %u, height %u, depth %u, layer_count %u, level_count %u, "
            "flags %#x, device %p, parent %p, parent_ops %p, sub_resources %p, texture_ops %p.\n",
            texture, debug_d3dresourcetype(desc->resource_type), debug_d3dformat(desc->format),
            desc->multisample_type, desc->multisample_quality, debug_d3dusage(desc->usage),
            wined3d_debug_resource_access(desc->access), desc->width, desc->height, desc->depth,
            layer_count, level_count, flags, device, parent, parent_ops, sub_resources, texture_ops);

    if (!desc->width || !desc->height || !desc->depth)
        return WINED3DERR_INVALIDCALL;

    if (desc->resource_type == WINED3D_RTYPE_TEXTURE_3D && layer_count != 1)
    {
        ERR("Invalid layer count for volume texture.\n");
        return E_INVALIDARG;
    }

    texture->sub_resources = sub_resources;

    /* TODO: It should only be possible to create textures for formats
     * that are reported as supported. */
    if (WINED3DFMT_UNKNOWN >= desc->format)
    {
        WARN("Texture cannot be created with a format of WINED3DFMT_UNKNOWN.\n");
        return WINED3DERR_INVALIDCALL;
    }
    format = wined3d_get_format(device->adapter, desc->format, desc->bind_flags);

    if (desc->usage & WINED3DUSAGE_DYNAMIC && (wined3d_resource_access_is_managed(desc->access)
            || desc->usage & WINED3DUSAGE_SCRATCH))
    {
        WARN("Attempted to create a dynamic texture with access %s and usage %s.\n",
                wined3d_debug_resource_access(desc->access), debug_d3dusage(desc->usage));
        return WINED3DERR_INVALIDCALL;
    }

    pow2_width = desc->width;
    pow2_height = desc->height;
    if (((desc->width & (desc->width - 1)) || (desc->height & (desc->height - 1)) || (desc->depth & (desc->depth - 1)))
            && !d3d_info->texture_npot)
    {
        /* level_count == 0 returns an error as well. */
        if (level_count != 1 || layer_count != 1 || desc->resource_type == WINED3D_RTYPE_TEXTURE_3D)
        {
            if (!(desc->usage & WINED3DUSAGE_SCRATCH))
            {
                WARN("Attempted to create a mipmapped/cube/array/volume NPOT "
                        "texture without unconditional NPOT support.\n");
                return WINED3DERR_INVALIDCALL;
            }

            WARN("Creating a scratch mipmapped/cube/array NPOT texture despite lack of HW support.\n");
        }
        texture->flags |= WINED3D_TEXTURE_COND_NP2;

        if (desc->resource_type != WINED3D_RTYPE_TEXTURE_3D && !d3d_info->texture_npot_conditional)
        {
            /* TODO: Add support for non-power-of-two compressed textures. */
            if (format->flags[WINED3D_GL_RES_TYPE_TEX_2D]
                    & (WINED3DFMT_FLAG_COMPRESSED | WINED3DFMT_FLAG_HEIGHT_SCALE))
            {
                FIXME("Compressed or height scaled non-power-of-two (%ux%u) textures are not supported.\n",
                        desc->width, desc->height);
                return WINED3DERR_NOTAVAILABLE;
            }

            /* Find the nearest pow2 match. */
            pow2_width = pow2_height = 1;
            while (pow2_width < desc->width)
                pow2_width <<= 1;
            while (pow2_height < desc->height)
                pow2_height <<= 1;
            texture->flags |= WINED3D_TEXTURE_COND_NP2_EMULATED;
        }
    }
    texture->pow2_width = pow2_width;
    texture->pow2_height = pow2_height;

    if ((pow2_width > d3d_info->limits.texture_size || pow2_height > d3d_info->limits.texture_size)
            && (desc->bind_flags & WINED3D_BIND_SHADER_RESOURCE))
    {
        /* One of four options:
         * 1: Do the same as we do with NPOT and scale the texture. (Any
         *    texture ops would require the texture to be scaled which is
         *    potentially slow.)
         * 2: Set the texture to the maximum size (bad idea).
         * 3: WARN and return WINED3DERR_NOTAVAILABLE.
         * 4: Create the surface, but allow it to be used only for DirectDraw
         *    Blts. Some apps (e.g. Swat 3) create textures with a height of
         *    16 and a width > 3000 and blt 16x16 letter areas from them to
         *    the render target. */
        if (desc->access & WINED3D_RESOURCE_ACCESS_GPU)
        {
            WARN("Dimensions (%ux%u) exceed the maximum texture size.\n", pow2_width, pow2_height);
            return WINED3DERR_NOTAVAILABLE;
        }

        /* We should never use this surface in combination with OpenGL. */
        TRACE("Creating an oversized (%ux%u) surface.\n", pow2_width, pow2_height);
    }

    for (i = 0; i < layer_count; ++i)
    {
        for (j = 0; j < level_count; ++j)
        {
            unsigned int idx = i * level_count + j;

            size = wined3d_format_calculate_size(format, device->surface_alignment,
                    max(1, desc->width >> j), max(1, desc->height >> j), max(1, desc->depth >> j));
            texture->sub_resources[idx].offset = offset;
            texture->sub_resources[idx].size = size;
            offset += size;
        }
        offset = (offset + (RESOURCE_ALIGNMENT - 1)) & ~(RESOURCE_ALIGNMENT - 1);
    }

    if (!offset)
        return WINED3DERR_INVALIDCALL;

    if (FAILED(hr = resource_init(&texture->resource, device, desc->resource_type, format,
            desc->multisample_type, desc->multisample_quality, desc->usage, desc->bind_flags, desc->access,
            desc->width, desc->height, desc->depth, offset, parent, parent_ops, &texture_resource_ops)))
    {
        static unsigned int once;

        /* DXTn 3D textures are not supported. Do not write the ERR for them. */
        if ((desc->format == WINED3DFMT_DXT1 || desc->format == WINED3DFMT_DXT2 || desc->format == WINED3DFMT_DXT3
                || desc->format == WINED3DFMT_DXT4 || desc->format == WINED3DFMT_DXT5)
                && !(format->flags[WINED3D_GL_RES_TYPE_TEX_2D] & WINED3DFMT_FLAG_TEXTURE)
                && desc->resource_type != WINED3D_RTYPE_TEXTURE_3D && !once++)
            ERR_(winediag)("The application tried to create a DXTn texture, but the driver does not support them.\n");

        WARN("Failed to initialize resource, returning %#x\n", hr);
        return hr;
    }
    wined3d_resource_update_draw_binding(&texture->resource);

    texture->texture_ops = texture_ops;

    texture->layer_count = layer_count;
    texture->level_count = level_count;
    texture->lod = 0;
    texture->flags |= WINED3D_TEXTURE_POW2_MAT_IDENT | WINED3D_TEXTURE_NORMALIZED_COORDS
            | WINED3D_TEXTURE_DOWNLOADABLE;
    if (flags & WINED3D_TEXTURE_CREATE_GET_DC_LENIENT)
        texture->flags |= WINED3D_TEXTURE_PIN_SYSMEM | WINED3D_TEXTURE_GET_DC_LENIENT;
    if (flags & (WINED3D_TEXTURE_CREATE_GET_DC | WINED3D_TEXTURE_CREATE_GET_DC_LENIENT))
        texture->flags |= WINED3D_TEXTURE_GET_DC;
    if (flags & WINED3D_TEXTURE_CREATE_DISCARD)
        texture->flags |= WINED3D_TEXTURE_DISCARD;
    if (flags & WINED3D_TEXTURE_CREATE_GENERATE_MIPMAPS)
    {
        if (!(texture->resource.format_flags & WINED3DFMT_FLAG_GEN_MIPMAP))
            WARN("Format doesn't support mipmaps generation, "
                    "ignoring WINED3D_TEXTURE_CREATE_GENERATE_MIPMAPS flag.\n");
        else
            texture->flags |= WINED3D_TEXTURE_GENERATE_MIPMAPS;
    }

    if (flags & WINED3D_TEXTURE_CREATE_RECORD_DIRTY_REGIONS
            && !(texture->dirty_regions = heap_calloc(texture->layer_count, sizeof(*texture->dirty_regions))))
    {
        wined3d_texture_cleanup_sync(texture);
        return E_OUTOFMEMORY;
    }

    /* Precalculated scaling for 'faked' non power of two texture coords. */
    if (texture->resource.gl_type == WINED3D_GL_RES_TYPE_TEX_RECT)
    {
        texture->pow2_matrix[0] = (float)desc->width;
        texture->pow2_matrix[5] = (float)desc->height;
        texture->flags &= ~(WINED3D_TEXTURE_POW2_MAT_IDENT | WINED3D_TEXTURE_NORMALIZED_COORDS);
    }
    else if (texture->flags & WINED3D_TEXTURE_COND_NP2_EMULATED)
    {
        texture->pow2_matrix[0] = (((float)desc->width) / ((float)pow2_width));
        texture->pow2_matrix[5] = (((float)desc->height) / ((float)pow2_height));
        texture->flags &= ~WINED3D_TEXTURE_POW2_MAT_IDENT;
    }
    else
    {
        texture->pow2_matrix[0] = 1.0f;
        texture->pow2_matrix[5] = 1.0f;
    }
    texture->pow2_matrix[10] = 1.0f;
    texture->pow2_matrix[15] = 1.0f;
    TRACE("x scale %.8e, y scale %.8e.\n", texture->pow2_matrix[0], texture->pow2_matrix[5]);

    if (wined3d_texture_use_pbo(texture, gl_info))
        texture->resource.map_binding = WINED3D_LOCATION_BUFFER;

    if (desc->resource_type != WINED3D_RTYPE_TEXTURE_3D
            || !wined3d_texture_use_pbo(texture, gl_info))
    {
        if (!wined3d_resource_prepare_sysmem(&texture->resource))
        {
            wined3d_texture_cleanup_sync(texture);
            return E_OUTOFMEMORY;
        }
    }

    sub_count = level_count * layer_count;
    if (sub_count / layer_count != level_count)
    {
        wined3d_texture_cleanup_sync(texture);
        return E_OUTOFMEMORY;
    }

    if (desc->usage & WINED3DUSAGE_OVERLAY)
    {
        if (!(texture->overlay_info = heap_calloc(sub_count, sizeof(*texture->overlay_info))))
        {
            wined3d_texture_cleanup_sync(texture);
            return E_OUTOFMEMORY;
        }

        for (i = 0; i < sub_count; ++i)
        {
            list_init(&texture->overlay_info[i].entry);
            list_init(&texture->overlay_info[i].overlays);
        }
    }

    /* Generate all sub-resources. */
    for (i = 0; i < sub_count; ++i)
    {
        struct wined3d_texture_sub_resource *sub_resource;

        sub_resource = &texture->sub_resources[i];
        sub_resource->locations = WINED3D_LOCATION_DISCARDED;
        if (desc->resource_type != WINED3D_RTYPE_TEXTURE_3D)
        {
            wined3d_texture_validate_location(texture, i, WINED3D_LOCATION_SYSMEM);
            wined3d_texture_invalidate_location(texture, i, ~WINED3D_LOCATION_SYSMEM);
        }

        if (FAILED(hr = device_parent->ops->texture_sub_resource_created(device_parent,
                desc->resource_type, texture, i, &sub_resource->parent, &sub_resource->parent_ops)))
        {
            WARN("Failed to create sub-resource parent, hr %#x.\n", hr);
            sub_resource->parent = NULL;
            wined3d_texture_cleanup_sync(texture);
            return hr;
        }

        TRACE("parent %p, parent_ops %p.\n", sub_resource->parent, sub_resource->parent_ops);

        TRACE("Created sub-resource %u (level %u, layer %u).\n",
                i, i % texture->level_count, i / texture->level_count);

        if (desc->usage & WINED3DUSAGE_OWNDC)
        {
            struct wined3d_texture_idx texture_idx = {texture, i};

            wined3d_cs_init_object(device->cs, wined3d_texture_create_dc, &texture_idx);
            wined3d_cs_finish(device->cs, WINED3D_CS_QUEUE_DEFAULT);
            if (!texture->dc_info || !texture->dc_info[i].dc)
            {
                wined3d_texture_cleanup_sync(texture);
                return WINED3DERR_INVALIDCALL;
            }
        }
    }

    return WINED3D_OK;
}

HRESULT CDECL wined3d_texture_blt(struct wined3d_texture *dst_texture, unsigned int dst_sub_resource_idx,
        const RECT *dst_rect, struct wined3d_texture *src_texture, unsigned int src_sub_resource_idx,
        const RECT *src_rect, DWORD flags, const struct wined3d_blt_fx *fx, enum wined3d_texture_filter_type filter)
{
    struct wined3d_box src_box = {src_rect->left, src_rect->top, src_rect->right, src_rect->bottom, 0, 1};
    struct wined3d_box dst_box = {dst_rect->left, dst_rect->top, dst_rect->right, dst_rect->bottom, 0, 1};
    HRESULT hr;

    TRACE("dst_texture %p, dst_sub_resource_idx %u, dst_rect %s, src_texture %p, "
            "src_sub_resource_idx %u, src_rect %s, flags %#x, fx %p, filter %s.\n",
            dst_texture, dst_sub_resource_idx, wine_dbgstr_rect(dst_rect), src_texture,
            src_sub_resource_idx, wine_dbgstr_rect(src_rect), flags, fx, debug_d3dtexturefiltertype(filter));

    if (dst_sub_resource_idx >= dst_texture->level_count * dst_texture->layer_count
            || dst_texture->resource.type != WINED3D_RTYPE_TEXTURE_2D)
        return WINED3DERR_INVALIDCALL;

    if (src_sub_resource_idx >= src_texture->level_count * src_texture->layer_count
            || src_texture->resource.type != WINED3D_RTYPE_TEXTURE_2D)
        return WINED3DERR_INVALIDCALL;

    if (filter != WINED3D_TEXF_NONE && filter != WINED3D_TEXF_POINT
            && filter != WINED3D_TEXF_LINEAR)
        return WINED3DERR_INVALIDCALL;

    if (FAILED(hr = wined3d_texture_check_box_dimensions(dst_texture,
            dst_sub_resource_idx % dst_texture->level_count, &dst_box)))
        return hr;

    if (FAILED(hr = wined3d_texture_check_box_dimensions(src_texture,
            src_sub_resource_idx % src_texture->level_count, &src_box)))
        return hr;

    if (dst_texture->sub_resources[dst_sub_resource_idx].map_count
            || src_texture->sub_resources[src_sub_resource_idx].map_count)
    {
        WARN("Sub-resource is busy, returning WINEDDERR_SURFACEBUSY.\n");
        return WINEDDERR_SURFACEBUSY;
    }

    if (!src_texture->resource.format->depth_size != !dst_texture->resource.format->depth_size
            || !src_texture->resource.format->stencil_size != !dst_texture->resource.format->stencil_size)
    {
        WARN("Rejecting depth/stencil blit between incompatible formats.\n");
        return WINED3DERR_INVALIDCALL;
    }

    if (dst_texture->resource.device != src_texture->resource.device)
    {
        FIXME("Rejecting cross-device blit.\n");
        return E_NOTIMPL;
    }

    wined3d_cs_emit_blt_sub_resource(dst_texture->resource.device->cs, &dst_texture->resource, dst_sub_resource_idx,
            &dst_box, &src_texture->resource, src_sub_resource_idx, &src_box, flags, fx, filter);

    return WINED3D_OK;
}

HRESULT CDECL wined3d_texture_get_overlay_position(const struct wined3d_texture *texture,
        unsigned int sub_resource_idx, LONG *x, LONG *y)
{
    struct wined3d_overlay_info *overlay;

    TRACE("texture %p, sub_resource_idx %u, x %p, y %p.\n", texture, sub_resource_idx, x, y);

    if (!(texture->resource.usage & WINED3DUSAGE_OVERLAY)
            || sub_resource_idx >= texture->level_count * texture->layer_count)
    {
        WARN("Invalid sub-resource specified.\n");
        return WINEDDERR_NOTAOVERLAYSURFACE;
    }

    overlay = &texture->overlay_info[sub_resource_idx];
    if (!overlay->dst_texture)
    {
        TRACE("Overlay not visible.\n");
        *x = 0;
        *y = 0;
        return WINEDDERR_OVERLAYNOTVISIBLE;
    }

    *x = overlay->dst_rect.left;
    *y = overlay->dst_rect.top;

    TRACE("Returning position %d, %d.\n", *x, *y);

    return WINED3D_OK;
}

HRESULT CDECL wined3d_texture_set_overlay_position(struct wined3d_texture *texture,
        unsigned int sub_resource_idx, LONG x, LONG y)
{
    struct wined3d_overlay_info *overlay;
    LONG w, h;

    TRACE("texture %p, sub_resource_idx %u, x %d, y %d.\n", texture, sub_resource_idx, x, y);

    if (!(texture->resource.usage & WINED3DUSAGE_OVERLAY)
            || sub_resource_idx >= texture->level_count * texture->layer_count)
    {
        WARN("Invalid sub-resource specified.\n");
        return WINEDDERR_NOTAOVERLAYSURFACE;
    }

    overlay = &texture->overlay_info[sub_resource_idx];
    w = overlay->dst_rect.right - overlay->dst_rect.left;
    h = overlay->dst_rect.bottom - overlay->dst_rect.top;
    SetRect(&overlay->dst_rect, x, y, x + w, y + h);

    return WINED3D_OK;
}

HRESULT CDECL wined3d_texture_update_overlay(struct wined3d_texture *texture, unsigned int sub_resource_idx,
        const RECT *src_rect, struct wined3d_texture *dst_texture, unsigned int dst_sub_resource_idx,
        const RECT *dst_rect, DWORD flags)
{
    struct wined3d_overlay_info *overlay;
    unsigned int level, dst_level;

    TRACE("texture %p, sub_resource_idx %u, src_rect %s, dst_texture %p, "
            "dst_sub_resource_idx %u, dst_rect %s, flags %#x.\n",
            texture, sub_resource_idx, wine_dbgstr_rect(src_rect), dst_texture,
            dst_sub_resource_idx, wine_dbgstr_rect(dst_rect), flags);

    if (!(texture->resource.usage & WINED3DUSAGE_OVERLAY) || texture->resource.type != WINED3D_RTYPE_TEXTURE_2D
            || sub_resource_idx >= texture->level_count * texture->layer_count)
    {
        WARN("Invalid sub-resource specified.\n");
        return WINEDDERR_NOTAOVERLAYSURFACE;
    }

    if (!dst_texture || dst_texture->resource.type != WINED3D_RTYPE_TEXTURE_2D
            || dst_sub_resource_idx >= dst_texture->level_count * dst_texture->layer_count)
    {
        WARN("Invalid destination sub-resource specified.\n");
        return WINED3DERR_INVALIDCALL;
    }

    overlay = &texture->overlay_info[sub_resource_idx];

    level = sub_resource_idx % texture->level_count;
    if (src_rect)
        overlay->src_rect = *src_rect;
    else
        SetRect(&overlay->src_rect, 0, 0,
                wined3d_texture_get_level_width(texture, level),
                wined3d_texture_get_level_height(texture, level));

    dst_level = dst_sub_resource_idx % dst_texture->level_count;
    if (dst_rect)
        overlay->dst_rect = *dst_rect;
    else
        SetRect(&overlay->dst_rect, 0, 0,
                wined3d_texture_get_level_width(dst_texture, dst_level),
                wined3d_texture_get_level_height(dst_texture, dst_level));

    if (overlay->dst_texture && (overlay->dst_texture != dst_texture
            || overlay->dst_sub_resource_idx != dst_sub_resource_idx || flags & WINEDDOVER_HIDE))
    {
        overlay->dst_texture = NULL;
        list_remove(&overlay->entry);
    }

    if (flags & WINEDDOVER_SHOW)
    {
        if (overlay->dst_texture != dst_texture || overlay->dst_sub_resource_idx != dst_sub_resource_idx)
        {
            overlay->dst_texture = dst_texture;
            overlay->dst_sub_resource_idx = dst_sub_resource_idx;
            list_add_tail(&texture->overlay_info[dst_sub_resource_idx].overlays, &overlay->entry);
        }
    }
    else if (flags & WINEDDOVER_HIDE)
    {
        /* Tests show that the rectangles are erased on hide. */
        SetRectEmpty(&overlay->src_rect);
        SetRectEmpty(&overlay->dst_rect);
        overlay->dst_texture = NULL;
    }

    return WINED3D_OK;
}

void * CDECL wined3d_texture_get_sub_resource_parent(struct wined3d_texture *texture, unsigned int sub_resource_idx)
{
    unsigned int sub_count = texture->level_count * texture->layer_count;

    TRACE("texture %p, sub_resource_idx %u.\n", texture, sub_resource_idx);

    if (sub_resource_idx >= sub_count)
    {
        WARN("sub_resource_idx %u >= sub_count %u.\n", sub_resource_idx, sub_count);
        return NULL;
    }

    return texture->sub_resources[sub_resource_idx].parent;
}

void CDECL wined3d_texture_set_sub_resource_parent(struct wined3d_texture *texture,
        unsigned int sub_resource_idx, void *parent)
{
    unsigned int sub_count = texture->level_count * texture->layer_count;

    TRACE("texture %p, sub_resource_idx %u, parent %p.\n", texture, sub_resource_idx, parent);

    if (sub_resource_idx >= sub_count)
    {
        WARN("sub_resource_idx %u >= sub_count %u.\n", sub_resource_idx, sub_count);
        return;
    }

    texture->sub_resources[sub_resource_idx].parent = parent;
}

HRESULT CDECL wined3d_texture_get_sub_resource_desc(const struct wined3d_texture *texture,
        unsigned int sub_resource_idx, struct wined3d_sub_resource_desc *desc)
{
    unsigned int sub_count = texture->level_count * texture->layer_count;
    const struct wined3d_resource *resource;
    unsigned int level_idx;

    TRACE("texture %p, sub_resource_idx %u, desc %p.\n", texture, sub_resource_idx, desc);

    if (sub_resource_idx >= sub_count)
    {
        WARN("sub_resource_idx %u >= sub_count %u.\n", sub_resource_idx, sub_count);
        return WINED3DERR_INVALIDCALL;
    }

    resource = &texture->resource;
    desc->format = resource->format->id;
    desc->multisample_type = resource->multisample_type;
    desc->multisample_quality = resource->multisample_quality;
    desc->usage = resource->usage;
    desc->bind_flags = resource->bind_flags;
    desc->access = resource->access;

    level_idx = sub_resource_idx % texture->level_count;
    desc->width = wined3d_texture_get_level_width(texture, level_idx);
    desc->height = wined3d_texture_get_level_height(texture, level_idx);
    desc->depth = wined3d_texture_get_level_depth(texture, level_idx);
    desc->size = texture->sub_resources[sub_resource_idx].size;

    return WINED3D_OK;
}

HRESULT wined3d_texture_gl_init(struct wined3d_texture_gl *texture_gl, struct wined3d_device *device,
        const struct wined3d_resource_desc *desc, unsigned int layer_count, unsigned int level_count,
        uint32_t flags, void *parent, const struct wined3d_parent_ops *parent_ops)
{
    const struct wined3d_gl_info *gl_info = &device->adapter->gl_info;
    HRESULT hr;

    TRACE("texture_gl %p, device %p, desc %p, layer_count %u, "
            "level_count %u, flags %#x, parent %p, parent_ops %p.\n",
            texture_gl, device, desc, layer_count,
            level_count, flags, parent, parent_ops);

    if (!(desc->usage & WINED3DUSAGE_LEGACY_CUBEMAP) && layer_count > 1
            && !gl_info->supported[EXT_TEXTURE_ARRAY])
    {
        WARN("OpenGL implementation does not support array textures.\n");
        return WINED3DERR_INVALIDCALL;
    }

    switch (desc->resource_type)
    {
        case WINED3D_RTYPE_TEXTURE_1D:
            if (layer_count > 1)
                texture_gl->target = GL_TEXTURE_1D_ARRAY;
            else
                texture_gl->target = GL_TEXTURE_1D;
            break;

        case WINED3D_RTYPE_TEXTURE_2D:
            if (desc->usage & WINED3DUSAGE_LEGACY_CUBEMAP)
            {
                texture_gl->target = GL_TEXTURE_CUBE_MAP_ARB;
            }
            else if (desc->multisample_type && gl_info->supported[ARB_TEXTURE_MULTISAMPLE])
            {
                if (layer_count > 1)
                    texture_gl->target = GL_TEXTURE_2D_MULTISAMPLE_ARRAY;
                else
                    texture_gl->target = GL_TEXTURE_2D_MULTISAMPLE;
            }
            else
            {
                if (layer_count > 1)
                    texture_gl->target = GL_TEXTURE_2D_ARRAY;
                else
                    texture_gl->target = GL_TEXTURE_2D;
            }
            break;

        case WINED3D_RTYPE_TEXTURE_3D:
            if (!gl_info->supported[EXT_TEXTURE3D])
            {
                WARN("OpenGL implementation does not support 3D textures.\n");
                return WINED3DERR_INVALIDCALL;
            }
            texture_gl->target = GL_TEXTURE_3D;
            break;

        default:
            ERR("Invalid resource type %s.\n", debug_d3dresourcetype(desc->resource_type));
            return WINED3DERR_INVALIDCALL;
    }

    list_init(&texture_gl->renderbuffers);

    if (FAILED(hr = wined3d_texture_init(&texture_gl->t, desc, layer_count, level_count,
            flags, device, parent, parent_ops, &texture_gl[1], &texture_gl_ops)))
        return hr;

    if (texture_gl->t.resource.gl_type == WINED3D_GL_RES_TYPE_TEX_RECT)
        texture_gl->target = GL_TEXTURE_RECTANGLE_ARB;

    if (texture_gl->target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY || texture_gl->target == GL_TEXTURE_2D_MULTISAMPLE)
        texture_gl->t.flags &= ~WINED3D_TEXTURE_DOWNLOADABLE;

    return WINED3D_OK;
}

HRESULT CDECL wined3d_texture_create(struct wined3d_device *device, const struct wined3d_resource_desc *desc,
        UINT layer_count, UINT level_count, DWORD flags, const struct wined3d_sub_resource_data *data,
        void *parent, const struct wined3d_parent_ops *parent_ops, struct wined3d_texture **texture)
{
    unsigned int sub_count = level_count * layer_count;
    unsigned int i;
    HRESULT hr;

    TRACE("device %p, desc %p, layer_count %u, level_count %u, flags %#x, data %p, "
            "parent %p, parent_ops %p, texture %p.\n",
            device, desc, layer_count, level_count, flags, data, parent, parent_ops, texture);

    if (!layer_count)
    {
        WARN("Invalid layer count.\n");
        return E_INVALIDARG;
    }
    if ((desc->usage & WINED3DUSAGE_LEGACY_CUBEMAP) && layer_count != 6)
    {
        ERR("Invalid layer count %u for legacy cubemap.\n", layer_count);
        layer_count = 6;
    }

    if (!level_count)
    {
        WARN("Invalid level count.\n");
        return WINED3DERR_INVALIDCALL;
    }

    if (desc->multisample_type != WINED3D_MULTISAMPLE_NONE)
    {
        const struct wined3d_format *format = wined3d_get_format(device->adapter, desc->format, desc->bind_flags);

        if (desc->multisample_type == WINED3D_MULTISAMPLE_NON_MASKABLE
                && desc->multisample_quality >= wined3d_popcount(format->multisample_types))
        {
            WARN("Unsupported quality level %u requested for WINED3D_MULTISAMPLE_NON_MASKABLE.\n",
                    desc->multisample_quality);
            return WINED3DERR_NOTAVAILABLE;
        }
        if (desc->multisample_type != WINED3D_MULTISAMPLE_NON_MASKABLE
                && (!(format->multisample_types & 1u << (desc->multisample_type - 1))
                || (desc->multisample_quality && desc->multisample_quality != WINED3D_STANDARD_MULTISAMPLE_PATTERN)))
        {
            WARN("Unsupported multisample type %u quality %u requested.\n", desc->multisample_type,
                    desc->multisample_quality);
            return WINED3DERR_NOTAVAILABLE;
        }
    }

    if (data)
    {
        for (i = 0; i < sub_count; ++i)
        {
            if (data[i].data)
                continue;

            WARN("Invalid sub-resource data specified for sub-resource %u.\n", i);
            return E_INVALIDARG;
        }
    }

    if (FAILED(hr = device->adapter->adapter_ops->adapter_create_texture(device, desc,
            layer_count, level_count, flags, parent, parent_ops, texture)))
        return hr;

    /* FIXME: We'd like to avoid ever allocating system memory for the texture
     * in this case. */
    if (data)
    {
        struct wined3d_box box;

        for (i = 0; i < sub_count; ++i)
        {
            wined3d_texture_get_level_box(*texture, i % (*texture)->level_count, &box);
            wined3d_cs_emit_update_sub_resource(device->cs, &(*texture)->resource,
                    i, &box, data[i].data, data[i].row_pitch, data[i].slice_pitch);
        }
    }

    TRACE("Created texture %p.\n", *texture);

    return WINED3D_OK;
}

HRESULT CDECL wined3d_texture_get_dc(struct wined3d_texture *texture, unsigned int sub_resource_idx, HDC *dc)
{
    struct wined3d_device *device = texture->resource.device;
    struct wined3d_texture_sub_resource *sub_resource;
    struct wined3d_dc_info *dc_info;

    TRACE("texture %p, sub_resource_idx %u, dc %p.\n", texture, sub_resource_idx, dc);

    if (!(texture->flags & WINED3D_TEXTURE_GET_DC))
    {
        WARN("Texture does not support GetDC\n");
        /* Don't touch the DC */
        return WINED3DERR_INVALIDCALL;
    }

    if (!(sub_resource = wined3d_texture_get_sub_resource(texture, sub_resource_idx)))
        return WINED3DERR_INVALIDCALL;

    if (texture->resource.type != WINED3D_RTYPE_TEXTURE_2D)
    {
        WARN("Not supported on %s resources.\n", debug_d3dresourcetype(texture->resource.type));
        return WINED3DERR_INVALIDCALL;
    }

    if (texture->resource.map_count && !(texture->flags & WINED3D_TEXTURE_GET_DC_LENIENT))
        return WINED3DERR_INVALIDCALL;

    if (!(dc_info = texture->dc_info) || !dc_info[sub_resource_idx].dc)
    {
        struct wined3d_texture_idx texture_idx = {texture, sub_resource_idx};

        wined3d_cs_init_object(device->cs, wined3d_texture_create_dc, &texture_idx);
        wined3d_cs_finish(device->cs, WINED3D_CS_QUEUE_DEFAULT);
        if (!(dc_info = texture->dc_info) || !dc_info[sub_resource_idx].dc)
            return WINED3DERR_INVALIDCALL;
    }

    if (!(texture->flags & WINED3D_TEXTURE_GET_DC_LENIENT))
        texture->flags |= WINED3D_TEXTURE_DC_IN_USE;
    ++texture->resource.map_count;
    ++sub_resource->map_count;

    *dc = dc_info[sub_resource_idx].dc;
    TRACE("Returning dc %p.\n", *dc);

    return WINED3D_OK;
}

HRESULT CDECL wined3d_texture_release_dc(struct wined3d_texture *texture, unsigned int sub_resource_idx, HDC dc)
{
    struct wined3d_device *device = texture->resource.device;
    struct wined3d_texture_sub_resource *sub_resource;
    struct wined3d_dc_info *dc_info;

    TRACE("texture %p, sub_resource_idx %u, dc %p.\n", texture, sub_resource_idx, dc);

    if (!(sub_resource = wined3d_texture_get_sub_resource(texture, sub_resource_idx)))
        return WINED3DERR_INVALIDCALL;

    if (texture->resource.type != WINED3D_RTYPE_TEXTURE_2D)
    {
        WARN("Not supported on %s resources.\n", debug_d3dresourcetype(texture->resource.type));
        return WINED3DERR_INVALIDCALL;
    }

    if (!(texture->flags & (WINED3D_TEXTURE_GET_DC_LENIENT | WINED3D_TEXTURE_DC_IN_USE)))
        return WINED3DERR_INVALIDCALL;

    if (!(dc_info = texture->dc_info) || dc_info[sub_resource_idx].dc != dc)
    {
        WARN("Application tries to release invalid DC %p, sub-resource DC is %p.\n",
                dc, dc_info ? dc_info[sub_resource_idx].dc : NULL);
        return WINED3DERR_INVALIDCALL;
    }

    if (!(texture->resource.usage & WINED3DUSAGE_OWNDC))
    {
        struct wined3d_texture_idx texture_idx = {texture, sub_resource_idx};

        wined3d_cs_destroy_object(device->cs, wined3d_texture_destroy_dc, &texture_idx);
        wined3d_cs_finish(device->cs, WINED3D_CS_QUEUE_DEFAULT);
    }

    --sub_resource->map_count;
    if (!--texture->resource.map_count && texture->update_map_binding)
        wined3d_texture_update_map_binding(texture);
    if (!(texture->flags & WINED3D_TEXTURE_GET_DC_LENIENT))
        texture->flags &= ~WINED3D_TEXTURE_DC_IN_USE;

    return WINED3D_OK;
}

void wined3d_texture_upload_from_texture(struct wined3d_texture *dst_texture, unsigned int dst_sub_resource_idx,
        unsigned int dst_x, unsigned int dst_y, unsigned int dst_z, struct wined3d_texture *src_texture,
        unsigned int src_sub_resource_idx, const struct wined3d_box *src_box)
{
    unsigned int src_row_pitch, src_slice_pitch;
    unsigned int update_w, update_h, update_d;
    unsigned int src_level, dst_level;
    struct wined3d_context *context;
    struct wined3d_bo_address data;

    TRACE("dst_texture %p, dst_sub_resource_idx %u, dst_x %u, dst_y %u, dst_z %u, "
            "src_texture %p, src_sub_resource_idx %u, src_box %s.\n",
            dst_texture, dst_sub_resource_idx, dst_x, dst_y, dst_z,
            src_texture, src_sub_resource_idx, debug_box(src_box));

    context = context_acquire(dst_texture->resource.device, NULL, 0);

    /* Only load the sub-resource for partial updates. For newly allocated
     * textures the texture wouldn't be the current location, and we'd upload
     * zeroes just to overwrite them again. */
    update_w = src_box->right - src_box->left;
    update_h = src_box->bottom - src_box->top;
    update_d = src_box->back - src_box->front;
    dst_level = dst_sub_resource_idx % dst_texture->level_count;
    if (update_w == wined3d_texture_get_level_width(dst_texture, dst_level)
            && update_h == wined3d_texture_get_level_height(dst_texture, dst_level)
            && update_d == wined3d_texture_get_level_depth(dst_texture, dst_level))
        wined3d_texture_prepare_location(dst_texture, dst_sub_resource_idx, context, WINED3D_LOCATION_TEXTURE_RGB);
    else
        wined3d_texture_load_location(dst_texture, dst_sub_resource_idx, context, WINED3D_LOCATION_TEXTURE_RGB);

    src_level = src_sub_resource_idx % src_texture->level_count;
    wined3d_texture_get_memory(src_texture, src_sub_resource_idx, &data,
            src_texture->sub_resources[src_sub_resource_idx].locations);
    wined3d_texture_get_pitch(src_texture, src_level, &src_row_pitch, &src_slice_pitch);

    dst_texture->texture_ops->texture_upload_data(context, wined3d_const_bo_address(&data),
            src_texture->resource.format, src_box, src_row_pitch, src_slice_pitch, dst_texture,
            dst_sub_resource_idx, WINED3D_LOCATION_TEXTURE_RGB, dst_x, dst_y, dst_z);

    context_release(context);

    wined3d_texture_validate_location(dst_texture, dst_sub_resource_idx, WINED3D_LOCATION_TEXTURE_RGB);
    wined3d_texture_invalidate_location(dst_texture, dst_sub_resource_idx, ~WINED3D_LOCATION_TEXTURE_RGB);
}

/* Partial downloads are not supported. */
void wined3d_texture_download_from_texture(struct wined3d_texture *dst_texture, unsigned int dst_sub_resource_idx,
        struct wined3d_texture *src_texture, unsigned int src_sub_resource_idx)
{
    unsigned int src_level, dst_level, dst_row_pitch, dst_slice_pitch;
    unsigned int dst_location = dst_texture->resource.map_binding;
    struct wined3d_context *context;
    struct wined3d_bo_address data;
    struct wined3d_box src_box;
    unsigned int src_location;

    context = context_acquire(src_texture->resource.device, NULL, 0);

    wined3d_texture_prepare_location(dst_texture, dst_sub_resource_idx, context, dst_location);
    wined3d_texture_get_memory(dst_texture, dst_sub_resource_idx, &data, dst_location);

    if (src_texture->sub_resources[src_sub_resource_idx].locations & WINED3D_LOCATION_TEXTURE_RGB)
        src_location = WINED3D_LOCATION_TEXTURE_RGB;
    else
        src_location = WINED3D_LOCATION_TEXTURE_SRGB;
    src_level = src_sub_resource_idx % src_texture->level_count;
    wined3d_texture_get_level_box(src_texture, src_level, &src_box);

    dst_level = dst_sub_resource_idx % dst_texture->level_count;
    wined3d_texture_get_pitch(dst_texture, dst_level, &dst_row_pitch, &dst_slice_pitch);

    src_texture->texture_ops->texture_download_data(context, src_texture, src_sub_resource_idx, src_location,
            &src_box, &data, dst_texture->resource.format, 0, 0, 0, dst_row_pitch, dst_slice_pitch);

    context_release(context);

    wined3d_texture_validate_location(dst_texture, dst_sub_resource_idx, dst_location);
    wined3d_texture_invalidate_location(dst_texture, dst_sub_resource_idx, ~dst_location);
}

static void wined3d_texture_no3d_upload_data(struct wined3d_context *context,
        const struct wined3d_const_bo_address *src_bo_addr, const struct wined3d_format *src_format,
        const struct wined3d_box *src_box, unsigned int src_row_pitch, unsigned int src_slice_pitch,
        struct wined3d_texture *dst_texture, unsigned int dst_sub_resource_idx, unsigned int dst_location,
        unsigned int dst_x, unsigned int dst_y, unsigned int dst_z)
{
    FIXME("Not implemented.\n");
}

static void wined3d_texture_no3d_download_data(struct wined3d_context *context,
        struct wined3d_texture *src_texture, unsigned int src_sub_resource_idx, unsigned int src_location,
        const struct wined3d_box *src_box, const struct wined3d_bo_address *dst_bo_addr,
        const struct wined3d_format *dst_format, unsigned int dst_x, unsigned int dst_y, unsigned int dst_z,
        unsigned int dst_row_pitch, unsigned int dst_slice_pitch)
{
    FIXME("Not implemented.\n");
}

static BOOL wined3d_texture_no3d_prepare_location(struct wined3d_texture *texture,
        unsigned int sub_resource_idx, struct wined3d_context *context, unsigned int location)
{
    if (location == WINED3D_LOCATION_SYSMEM)
        return texture->sub_resources[sub_resource_idx].user_memory ? TRUE
                : wined3d_resource_prepare_sysmem(&texture->resource);

    FIXME("Unhandled location %s.\n", wined3d_debug_location(location));
    return FALSE;
}

static BOOL wined3d_texture_no3d_load_location(struct wined3d_texture *texture,
        unsigned int sub_resource_idx, struct wined3d_context *context, DWORD location)
{
    TRACE("texture %p, sub_resource_idx %u, context %p, location %s.\n",
            texture, sub_resource_idx, context, wined3d_debug_location(location));

    if (location == WINED3D_LOCATION_SYSMEM)
        return TRUE;

    ERR("Unhandled location %s.\n", wined3d_debug_location(location));

    return FALSE;
}

static void wined3d_texture_no3d_unload_location(struct wined3d_texture *texture,
        struct wined3d_context *context, unsigned int location)
{
    TRACE("texture %p, context %p, location %s.\n", texture, context, wined3d_debug_location(location));
}

static const struct wined3d_texture_ops wined3d_texture_no3d_ops =
{
    wined3d_texture_no3d_prepare_location,
    wined3d_texture_no3d_load_location,
    wined3d_texture_no3d_unload_location,
    wined3d_texture_no3d_upload_data,
    wined3d_texture_no3d_download_data,
};

HRESULT wined3d_texture_no3d_init(struct wined3d_texture *texture_no3d, struct wined3d_device *device,
        const struct wined3d_resource_desc *desc, unsigned int layer_count, unsigned int level_count,
        uint32_t flags, void *parent, const struct wined3d_parent_ops *parent_ops)
{
    TRACE("texture_no3d %p, device %p, desc %p, layer_count %u, "
            "level_count %u, flags %#x, parent %p, parent_ops %p.\n",
            texture_no3d, device, desc, layer_count,
            level_count, flags, parent, parent_ops);

    return wined3d_texture_init(texture_no3d, desc, layer_count, level_count,
            flags, device, parent, parent_ops, &texture_no3d[1], &wined3d_texture_no3d_ops);
}

void wined3d_vk_swizzle_from_color_fixup(VkComponentMapping *mapping, struct color_fixup_desc fixup)
{
    static const VkComponentSwizzle swizzle_source[] =
    {
        VK_COMPONENT_SWIZZLE_ZERO,  /* CHANNEL_SOURCE_ZERO */
        VK_COMPONENT_SWIZZLE_ONE,   /* CHANNEL_SOURCE_ONE */
        VK_COMPONENT_SWIZZLE_R,     /* CHANNEL_SOURCE_X */
        VK_COMPONENT_SWIZZLE_G,     /* CHANNEL_SOURCE_Y */
        VK_COMPONENT_SWIZZLE_B,     /* CHANNEL_SOURCE_Z */
        VK_COMPONENT_SWIZZLE_A,     /* CHANNEL_SOURCE_W */
    };

    mapping->r = swizzle_source[fixup.x_source];
    mapping->g = swizzle_source[fixup.y_source];
    mapping->b = swizzle_source[fixup.z_source];
    mapping->a = swizzle_source[fixup.w_source];
}

const VkDescriptorImageInfo *wined3d_texture_vk_get_default_image_info(struct wined3d_texture_vk *texture_vk,
        struct wined3d_context_vk *context_vk)
{
    const struct wined3d_format_vk *format_vk;
    const struct wined3d_vk_info *vk_info;
    struct wined3d_device_vk *device_vk;
    VkImageViewCreateInfo create_info;
    struct color_fixup_desc fixup;
    uint32_t flags = 0;
    VkResult vr;

    if (texture_vk->default_image_info.imageView)
        return &texture_vk->default_image_info;

    format_vk = wined3d_format_vk(texture_vk->t.resource.format);
    device_vk = wined3d_device_vk(texture_vk->t.resource.device);
    vk_info = context_vk->vk_info;

    if (texture_vk->t.layer_count > 1)
        flags |= WINED3D_VIEW_TEXTURE_ARRAY;

    wined3d_texture_vk_prepare_texture(texture_vk, context_vk);
    create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    create_info.pNext = NULL;
    create_info.flags = 0;
    create_info.image = texture_vk->vk_image;
    create_info.viewType = vk_image_view_type_from_wined3d(texture_vk->t.resource.type, flags);
    create_info.format = format_vk->vk_format;
    fixup = format_vk->f.color_fixup;
    if (is_identity_fixup(fixup) || !can_use_texture_swizzle(context_vk->c.d3d_info, &format_vk->f))
    {
        create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    }
    else
    {
        wined3d_vk_swizzle_from_color_fixup(&create_info.components, fixup);
    }
    create_info.subresourceRange.aspectMask = vk_aspect_mask_from_format(&format_vk->f);
    create_info.subresourceRange.baseMipLevel = 0;
    create_info.subresourceRange.levelCount = texture_vk->t.level_count;
    create_info.subresourceRange.baseArrayLayer = 0;
    create_info.subresourceRange.layerCount = texture_vk->t.layer_count;
    if ((vr = VK_CALL(vkCreateImageView(device_vk->vk_device, &create_info,
            NULL, &texture_vk->default_image_info.imageView))) < 0)
    {
        ERR("Failed to create Vulkan image view, vr %s.\n", wined3d_debug_vkresult(vr));
        return NULL;
    }

    TRACE("Created image view 0x%s.\n", wine_dbgstr_longlong(texture_vk->default_image_info.imageView));

    texture_vk->default_image_info.sampler = VK_NULL_HANDLE;
    texture_vk->default_image_info.imageLayout = texture_vk->layout;

    return &texture_vk->default_image_info;
}

static void wined3d_texture_vk_upload_data(struct wined3d_context *context,
        const struct wined3d_const_bo_address *src_bo_addr, const struct wined3d_format *src_format,
        const struct wined3d_box *src_box, unsigned int src_row_pitch, unsigned int src_slice_pitch,
        struct wined3d_texture *dst_texture, unsigned int dst_sub_resource_idx, unsigned int dst_location,
        unsigned int dst_x, unsigned int dst_y, unsigned int dst_z)
{
    struct wined3d_texture_vk *dst_texture_vk = wined3d_texture_vk(dst_texture);
    struct wined3d_context_vk *context_vk = wined3d_context_vk(context);
    unsigned int dst_level, dst_row_pitch, dst_slice_pitch;
    struct wined3d_texture_sub_resource *sub_resource;
    struct wined3d_bo_address staging_bo_addr;
    const struct wined3d_vk_info *vk_info;
    VkCommandBuffer vk_command_buffer;
    struct wined3d_bo_vk staging_bo;
    VkImageAspectFlags aspect_mask;
    size_t src_offset, dst_offset;
    struct wined3d_range range;
    VkBufferImageCopy region;
    void *map_ptr;

    TRACE("context %p, src_bo_addr %s, src_format %s, src_box %s, src_row_pitch %u, src_slice_pitch %u, "
            "dst_texture %p, dst_sub_resource_idx %u, dst_location %s, dst_x %u, dst_y %u, dst_z %u.\n",
            context, debug_const_bo_address(src_bo_addr), debug_d3dformat(src_format->id), debug_box(src_box),
            src_row_pitch, src_slice_pitch, dst_texture, dst_sub_resource_idx,
            wined3d_debug_location(dst_location), dst_x, dst_y, dst_z);

    if (src_bo_addr->buffer_object)
    {
        FIXME("Unhandled buffer object %#lx.\n", src_bo_addr->buffer_object);
        return;
    }

    if (src_format->id != dst_texture->resource.format->id)
    {
        FIXME("Unhandled format conversion (%s -> %s).\n",
                debug_d3dformat(src_format->id),
                debug_d3dformat(dst_texture->resource.format->id));
        return;
    }

    dst_level = dst_sub_resource_idx % dst_texture->level_count;
    wined3d_texture_get_pitch(dst_texture, dst_level, &dst_row_pitch, &dst_slice_pitch);
    if (dst_texture->resource.type == WINED3D_RTYPE_TEXTURE_1D)
        src_row_pitch = dst_row_pitch = 0;
    if (dst_texture->resource.type != WINED3D_RTYPE_TEXTURE_3D)
        src_slice_pitch = dst_slice_pitch = 0;

    if (dst_location != WINED3D_LOCATION_TEXTURE_RGB)
    {
        FIXME("Unhandled location %s.\n", wined3d_debug_location(dst_location));
        return;
    }

    if (wined3d_resource_get_sample_count(&dst_texture_vk->t.resource) > 1)
    {
        FIXME("Not supported for multisample textures.\n");
        return;
    }

    aspect_mask = vk_aspect_mask_from_format(dst_texture->resource.format);
    if (wined3d_popcount(aspect_mask) > 1)
    {
        FIXME("Unhandled multi-aspect format %s.\n", debug_d3dformat(dst_texture->resource.format->id));
        return;
    }

    sub_resource = &dst_texture_vk->t.sub_resources[dst_sub_resource_idx];
    vk_info = context_vk->vk_info;

    src_offset = src_box->front * src_slice_pitch
            + (src_box->top / src_format->block_height) * src_row_pitch
            + (src_box->left / src_format->block_width) * src_format->block_byte_count;
    dst_offset = dst_z * src_slice_pitch
            + (dst_y / src_format->block_height) * src_row_pitch
            + (dst_x / src_format->block_width) * src_format->block_byte_count;

    if (!wined3d_context_vk_create_bo(context_vk, sub_resource->size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &staging_bo))
    {
        ERR("Failed to create staging bo.\n");
        return;
    }

    staging_bo_addr.buffer_object = (uintptr_t)&staging_bo;
    staging_bo_addr.addr = NULL;
    if (!(map_ptr = wined3d_context_map_bo_address(context, &staging_bo_addr,
            sub_resource->size, WINED3D_MAP_DISCARD | WINED3D_MAP_WRITE)))
    {
        ERR("Failed to map staging bo.\n");
        wined3d_context_vk_destroy_bo(context_vk, &staging_bo);
        return;
    }

    wined3d_format_copy_data(src_format, src_bo_addr->addr + src_offset, src_row_pitch, src_slice_pitch,
            (uint8_t *)map_ptr + dst_offset, dst_row_pitch, dst_slice_pitch, src_box->right - src_box->left,
            src_box->bottom - src_box->top, src_box->back - src_box->front);

    range.offset = 0;
    range.size = sub_resource->size;
    wined3d_context_unmap_bo_address(context, &staging_bo_addr, 1, &range);

    if (!(vk_command_buffer = wined3d_context_vk_get_command_buffer(context_vk)))
    {
        ERR("Failed to get command buffer.\n");
        return;
    }

    wined3d_context_vk_image_barrier(context_vk, vk_command_buffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            vk_access_mask_from_bind_flags(dst_texture_vk->t.resource.bind_flags),
            VK_ACCESS_TRANSFER_WRITE_BIT,
            dst_texture_vk->layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            dst_texture_vk->vk_image, aspect_mask);

    region.bufferOffset = staging_bo.buffer_offset + dst_offset;
    region.bufferRowLength = (dst_row_pitch / src_format->block_byte_count) * src_format->block_width;
    if (dst_row_pitch)
        region.bufferImageHeight = (dst_slice_pitch / dst_row_pitch) * src_format->block_height;
    else
        region.bufferImageHeight = 1;
    region.imageSubresource.aspectMask = aspect_mask;
    region.imageSubresource.mipLevel = dst_level;
    region.imageSubresource.baseArrayLayer = dst_sub_resource_idx / dst_texture_vk->t.level_count;
    region.imageSubresource.layerCount = 1;
    region.imageOffset.x = dst_x;
    region.imageOffset.y = dst_y;
    region.imageOffset.z = dst_z;
    region.imageExtent.width = src_box->right - src_box->left;
    region.imageExtent.height = src_box->bottom - src_box->top;
    region.imageExtent.depth = src_box->back - src_box->front;

    VK_CALL(vkCmdCopyBufferToImage(vk_command_buffer, staging_bo.vk_buffer,
            dst_texture_vk->vk_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region));

    wined3d_context_vk_image_barrier(context_vk, vk_command_buffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            vk_access_mask_from_bind_flags(dst_texture_vk->t.resource.bind_flags),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, dst_texture_vk->layout,
            dst_texture_vk->vk_image, aspect_mask);
    wined3d_context_vk_reference_texture(context_vk, dst_texture_vk);
    wined3d_context_vk_reference_bo(context_vk, &staging_bo);
    wined3d_context_vk_destroy_bo(context_vk, &staging_bo);
}

static void wined3d_texture_vk_download_data(struct wined3d_context *context,
        struct wined3d_texture *src_texture, unsigned int src_sub_resource_idx, unsigned int src_location,
        const struct wined3d_box *src_box, const struct wined3d_bo_address *dst_bo_addr,
        const struct wined3d_format *dst_format, unsigned int dst_x, unsigned int dst_y, unsigned int dst_z,
        unsigned int dst_row_pitch, unsigned int dst_slice_pitch)
{
    struct wined3d_texture_vk *src_texture_vk = wined3d_texture_vk(src_texture);
    struct wined3d_context_vk *context_vk = wined3d_context_vk(context);
    unsigned int src_level, src_width, src_height, src_depth;
    struct wined3d_texture_sub_resource *sub_resource;
    unsigned int src_row_pitch, src_slice_pitch;
    struct wined3d_bo_address staging_bo_addr;
    const struct wined3d_vk_info *vk_info;
    VkCommandBuffer vk_command_buffer;
    struct wined3d_bo_vk staging_bo;
    VkImageAspectFlags aspect_mask;
    VkBufferImageCopy region;
    void *map_ptr;

    TRACE("context %p, src_texture %p, src_sub_resource_idx %u, src_location %s, src_box %s, dst_bo_addr %s, "
            "dst_format %s, dst_x %u, dst_y %u, dst_z %u, dst_row_pitch %u, dst_slice_pitch %u.\n",
            context, src_texture, src_sub_resource_idx, wined3d_debug_location(src_location),
            debug_box(src_box), debug_bo_address(dst_bo_addr), debug_d3dformat(dst_format->id),
            dst_x, dst_y, dst_z, dst_row_pitch, dst_slice_pitch);

    if (src_location != WINED3D_LOCATION_TEXTURE_RGB)
    {
        FIXME("Unhandled location %s.\n", wined3d_debug_location(src_location));
        return;
    }

    src_level = src_sub_resource_idx % src_texture->level_count;
    src_width = wined3d_texture_get_level_width(src_texture, src_level);
    src_height = wined3d_texture_get_level_height(src_texture, src_level);
    src_depth = wined3d_texture_get_level_depth(src_texture, src_level);
    if (src_box->left || src_box->top || src_box->right != src_width || src_box->bottom != src_height
            || src_box->front || src_box->back != src_depth)
    {
        FIXME("Unhandled source box %s.\n", debug_box(src_box));
        return;
    }

    if (dst_bo_addr->buffer_object)
    {
        FIXME("Unhandled buffer object %#lx.\n", dst_bo_addr->buffer_object);
        return;
    }

    if (dst_format->id != src_texture->resource.format->id)
    {
        FIXME("Unhandled format conversion (%s -> %s).\n",
                debug_d3dformat(dst_format->id),
                debug_d3dformat(src_texture->resource.format->id));
        return;
    }

    if (dst_x || dst_y || dst_z)
    {
        FIXME("Unhandled destination (%u, %u, %u).\n", dst_x, dst_y, dst_z);
        return;
    }

    if (wined3d_resource_get_sample_count(&src_texture_vk->t.resource) > 1)
    {
        FIXME("Not supported for multisample textures.\n");
        return;
    }

    aspect_mask = vk_aspect_mask_from_format(src_texture->resource.format);
    if (wined3d_popcount(aspect_mask) > 1)
    {
        FIXME("Unhandled multi-aspect format %s.\n", debug_d3dformat(src_texture->resource.format->id));
        return;
    }

    wined3d_texture_get_pitch(src_texture, src_level, &src_row_pitch, &src_slice_pitch);
    if (src_texture->resource.type == WINED3D_RTYPE_TEXTURE_1D)
        src_row_pitch = dst_row_pitch = 0;
    if (src_texture->resource.type != WINED3D_RTYPE_TEXTURE_3D)
        src_slice_pitch = dst_slice_pitch = 0;

    sub_resource = &src_texture_vk->t.sub_resources[src_sub_resource_idx];
    vk_info = context_vk->vk_info;
    if (!(vk_command_buffer = wined3d_context_vk_get_command_buffer(context_vk)))
    {
        ERR("Failed to get command buffer.\n");
        return;
    }

    if (!wined3d_context_vk_create_bo(context_vk, sub_resource->size,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &staging_bo))
    {
        ERR("Failed to create staging bo.\n");
        return;
    }

    wined3d_context_vk_image_barrier(context_vk, vk_command_buffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            vk_access_mask_from_bind_flags(src_texture_vk->t.resource.bind_flags),
            VK_ACCESS_TRANSFER_READ_BIT,
            src_texture_vk->layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            src_texture_vk->vk_image, aspect_mask);

    region.bufferOffset = staging_bo.buffer_offset;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = aspect_mask;
    region.imageSubresource.mipLevel = src_level;
    region.imageSubresource.baseArrayLayer = src_sub_resource_idx / src_texture_vk->t.level_count;
    region.imageSubresource.layerCount = 1;
    region.imageOffset.x = 0;
    region.imageOffset.y = 0;
    region.imageOffset.z = 0;
    region.imageExtent.width = src_width;
    region.imageExtent.height = src_height;
    region.imageExtent.depth = src_depth;

    VK_CALL(vkCmdCopyImageToBuffer(vk_command_buffer, src_texture_vk->vk_image,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging_bo.vk_buffer, 1, &region));

    wined3d_context_vk_image_barrier(context_vk, vk_command_buffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VK_ACCESS_TRANSFER_READ_BIT,
            vk_access_mask_from_bind_flags(src_texture_vk->t.resource.bind_flags),
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,  src_texture_vk->layout,
            src_texture_vk->vk_image, aspect_mask);

    wined3d_context_vk_reference_texture(context_vk, src_texture_vk);
    wined3d_context_vk_reference_bo(context_vk, &staging_bo);
    wined3d_context_vk_submit_command_buffer(context_vk, 0, NULL, NULL, 0, NULL);
    wined3d_context_vk_wait_command_buffer(context_vk, src_texture_vk->command_buffer_id);

    staging_bo_addr.buffer_object = (uintptr_t)&staging_bo;
    staging_bo_addr.addr = (uint8_t *)NULL;
    if (!(map_ptr = wined3d_context_map_bo_address(context, &staging_bo_addr,
            sub_resource->size, WINED3D_MAP_READ)))
    {
        ERR("Failed to map staging bo.\n");
        wined3d_context_vk_destroy_bo(context_vk, &staging_bo);
        return;
    }

    wined3d_format_copy_data(dst_format, map_ptr, src_row_pitch, src_slice_pitch,
            dst_bo_addr->addr, dst_row_pitch, dst_slice_pitch, src_box->right - src_box->left,
            src_box->bottom - src_box->top, src_box->back - src_box->front);

    wined3d_context_unmap_bo_address(context, &staging_bo_addr, 0, NULL);
    wined3d_context_vk_destroy_bo(context_vk, &staging_bo);
}

static BOOL wined3d_texture_vk_load_texture(struct wined3d_texture_vk *texture_vk,
        unsigned int sub_resource_idx, struct wined3d_context *context)
{
    struct wined3d_texture_sub_resource *sub_resource;
    unsigned int level, row_pitch, slice_pitch;
    struct wined3d_bo_address data;
    struct wined3d_box src_box;

    sub_resource = &texture_vk->t.sub_resources[sub_resource_idx];
    if (!(sub_resource->locations & WINED3D_LOCATION_SYSMEM))
    {
        ERR("Unimplemented load from %s.\n", wined3d_debug_location(sub_resource->locations));
        return FALSE;
    }

    level = sub_resource_idx % texture_vk->t.level_count;
    wined3d_texture_get_memory(&texture_vk->t, sub_resource_idx, &data, WINED3D_LOCATION_SYSMEM);
    wined3d_texture_get_level_box(&texture_vk->t, level, &src_box);
    wined3d_texture_get_pitch(&texture_vk->t, level, &row_pitch, &slice_pitch);
    wined3d_texture_vk_upload_data(context, wined3d_const_bo_address(&data), texture_vk->t.resource.format,
            &src_box, row_pitch, slice_pitch, &texture_vk->t, sub_resource_idx,
            WINED3D_LOCATION_TEXTURE_RGB, 0, 0, 0);

    return TRUE;
}

static BOOL wined3d_texture_vk_load_sysmem(struct wined3d_texture_vk *texture_vk,
        unsigned int sub_resource_idx, struct wined3d_context *context)
{
    struct wined3d_texture_sub_resource *sub_resource;
    unsigned int level, row_pitch, slice_pitch;
    struct wined3d_bo_address data;
    struct wined3d_box src_box;

    sub_resource = &texture_vk->t.sub_resources[sub_resource_idx];
    if (!(sub_resource->locations & WINED3D_LOCATION_TEXTURE_RGB))
    {
        ERR("Unimplemented load from %s.\n", wined3d_debug_location(sub_resource->locations));
        return FALSE;
    }

    level = sub_resource_idx % texture_vk->t.level_count;
    wined3d_texture_get_memory(&texture_vk->t, sub_resource_idx, &data, WINED3D_LOCATION_SYSMEM);
    wined3d_texture_get_level_box(&texture_vk->t, level, &src_box);
    wined3d_texture_get_pitch(&texture_vk->t, level, &row_pitch, &slice_pitch);
    wined3d_texture_vk_download_data(context, &texture_vk->t, sub_resource_idx, WINED3D_LOCATION_TEXTURE_RGB,
            &src_box, &data, texture_vk->t.resource.format, 0, 0, 0, row_pitch, slice_pitch);

    return TRUE;
}

BOOL wined3d_texture_vk_prepare_texture(struct wined3d_texture_vk *texture_vk,
        struct wined3d_context_vk *context_vk)
{
    const struct wined3d_format_vk *format_vk;
    VkMemoryRequirements memory_requirements;
    const struct wined3d_vk_info *vk_info;
    struct wined3d_adapter_vk *adapter_vk;
    struct wined3d_device_vk *device_vk;
    struct wined3d_resource *resource;
    VkCommandBuffer vk_command_buffer;
    VkImageCreateInfo create_info;
    unsigned int memory_type_idx;
    VkResult vr;

    if (texture_vk->t.flags & WINED3D_TEXTURE_RGB_ALLOCATED)
        return TRUE;

    if (!(vk_command_buffer = wined3d_context_vk_get_command_buffer(context_vk)))
    {
        ERR("Failed to get command buffer.\n");
        return FALSE;
    }

    resource = &texture_vk->t.resource;
    device_vk = wined3d_device_vk(resource->device);
    adapter_vk = wined3d_adapter_vk(device_vk->d.adapter);
    format_vk = wined3d_format_vk(resource->format);
    vk_info = context_vk->vk_info;

    create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    create_info.pNext = NULL;

    create_info.flags = 0;
    if (wined3d_format_is_typeless(&format_vk->f) || texture_vk->t.swapchain)
        create_info.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;

    switch (resource->type)
    {
        case WINED3D_RTYPE_TEXTURE_1D:
            create_info.imageType = VK_IMAGE_TYPE_1D;
            break;
        case WINED3D_RTYPE_TEXTURE_2D:
            create_info.imageType = VK_IMAGE_TYPE_2D;
            if (texture_vk->t.layer_count >= 6 && resource->width == resource->height)
                create_info.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
            break;
        case WINED3D_RTYPE_TEXTURE_3D:
            create_info.imageType = VK_IMAGE_TYPE_3D;
            if (resource->bind_flags & (WINED3D_BIND_RENDER_TARGET | WINED3D_BIND_UNORDERED_ACCESS))
                create_info.flags |= VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT;
            break;
        default:
            ERR("Invalid resource type %s.\n", debug_d3dresourcetype(resource->type));
            create_info.imageType = VK_IMAGE_TYPE_2D;
            break;
    }

    create_info.format = format_vk->vk_format;
    create_info.extent.width = resource->width;
    create_info.extent.height = resource->height;
    create_info.extent.depth = resource->depth;
    create_info.mipLevels = texture_vk->t.level_count;
    create_info.arrayLayers = texture_vk->t.layer_count;
    create_info.samples = max(1, wined3d_resource_get_sample_count(resource));
    create_info.tiling = VK_IMAGE_TILING_OPTIMAL;

    create_info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (resource->bind_flags & WINED3D_BIND_SHADER_RESOURCE)
        create_info.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if (resource->bind_flags & WINED3D_BIND_RENDER_TARGET)
        create_info.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (resource->bind_flags & WINED3D_BIND_DEPTH_STENCIL)
        create_info.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if (resource->bind_flags & WINED3D_BIND_UNORDERED_ACCESS)
        create_info.usage |= VK_IMAGE_USAGE_STORAGE_BIT;

    texture_vk->layout = VK_IMAGE_LAYOUT_GENERAL;
    if (wined3d_popcount(resource->bind_flags == 1))
    {
        switch (resource->bind_flags)
        {
            case WINED3D_BIND_RENDER_TARGET:
                texture_vk->layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                break;

            case WINED3D_BIND_DEPTH_STENCIL:
                texture_vk->layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                break;

            case WINED3D_BIND_SHADER_RESOURCE:
                texture_vk->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                break;

            default:
                break;
        }
    }

    create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.queueFamilyIndexCount = 0;
    create_info.pQueueFamilyIndices = NULL;
    create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if ((vr = VK_CALL(vkCreateImage(device_vk->vk_device, &create_info, NULL, &texture_vk->vk_image))) < 0)
    {
        ERR("Failed to create Vulkan image, vr %s.\n", wined3d_debug_vkresult(vr));
        return FALSE;
    }

    VK_CALL(vkGetImageMemoryRequirements(device_vk->vk_device, texture_vk->vk_image, &memory_requirements));

    memory_type_idx = wined3d_adapter_vk_get_memory_type_index(adapter_vk,
            memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (memory_type_idx == ~0u)
    {
        ERR("Failed to find suitable memory type.\n");
        VK_CALL(vkDestroyImage(device_vk->vk_device, texture_vk->vk_image, NULL));
        texture_vk->vk_image = VK_NULL_HANDLE;
        return FALSE;
    }

    texture_vk->memory = wined3d_context_vk_allocate_memory(context_vk,
            memory_type_idx, memory_requirements.size, &texture_vk->vk_memory);
    if (!texture_vk->vk_memory)
    {
        ERR("Failed to allocate image memory.\n");
        VK_CALL(vkDestroyImage(device_vk->vk_device, texture_vk->vk_image, NULL));
        texture_vk->vk_image = VK_NULL_HANDLE;
        return FALSE;
    }

    if ((vr = VK_CALL(vkBindImageMemory(device_vk->vk_device, texture_vk->vk_image,
            texture_vk->vk_memory, texture_vk->memory ? texture_vk->memory->offset : 0))) < 0)
    {
        WARN("Failed to bind memory, vr %s.\n", wined3d_debug_vkresult(vr));
        if (texture_vk->memory)
            wined3d_allocator_block_free(texture_vk->memory);
        else
            VK_CALL(vkFreeMemory(device_vk->vk_device, texture_vk->vk_memory, NULL));
        texture_vk->vk_memory = VK_NULL_HANDLE;
        VK_CALL(vkDestroyImage(device_vk->vk_device, texture_vk->vk_image, NULL));
        texture_vk->vk_image = VK_NULL_HANDLE;
        return FALSE;
    }

    wined3d_context_vk_reference_texture(context_vk, texture_vk);
    wined3d_context_vk_image_barrier(context_vk, vk_command_buffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0, 0,
            VK_IMAGE_LAYOUT_UNDEFINED, texture_vk->layout,
            texture_vk->vk_image, vk_aspect_mask_from_format(&format_vk->f));

    texture_vk->t.flags |= WINED3D_TEXTURE_RGB_ALLOCATED;

    TRACE("Created image 0x%s, memory 0x%s for texture %p.\n",
            wine_dbgstr_longlong(texture_vk->vk_image), wine_dbgstr_longlong(texture_vk->vk_memory), texture_vk);

    return TRUE;
}

static BOOL wined3d_texture_vk_prepare_location(struct wined3d_texture *texture,
        unsigned int sub_resource_idx, struct wined3d_context *context, unsigned int location)
{
    switch (location)
    {
        case WINED3D_LOCATION_SYSMEM:
            return texture->sub_resources[sub_resource_idx].user_memory ? TRUE
                    : wined3d_resource_prepare_sysmem(&texture->resource);

        case WINED3D_LOCATION_TEXTURE_RGB:
            return wined3d_texture_vk_prepare_texture(wined3d_texture_vk(texture), wined3d_context_vk(context));

        default:
            FIXME("Unhandled location %s.\n", wined3d_debug_location(location));
            return FALSE;
    }
}

static BOOL wined3d_texture_vk_load_location(struct wined3d_texture *texture,
        unsigned int sub_resource_idx, struct wined3d_context *context, DWORD location)
{
    if (!wined3d_texture_vk_prepare_location(texture, sub_resource_idx, context, location))
        return FALSE;

    switch (location)
    {
        case WINED3D_LOCATION_TEXTURE_RGB:
            return wined3d_texture_vk_load_texture(wined3d_texture_vk(texture), sub_resource_idx, context);

        case WINED3D_LOCATION_SYSMEM:
            return wined3d_texture_vk_load_sysmem(wined3d_texture_vk(texture), sub_resource_idx, context);

        default:
            FIXME("Unimplemented location %s.\n", wined3d_debug_location(location));
            return FALSE;
    }
}

static void wined3d_texture_vk_unload_location(struct wined3d_texture *texture,
        struct wined3d_context *context, unsigned int location)
{
    struct wined3d_texture_vk *texture_vk = wined3d_texture_vk(texture);
    struct wined3d_context_vk *context_vk = wined3d_context_vk(context);

    TRACE("texture %p, context %p, location %s.\n", texture, context, wined3d_debug_location(location));

    switch (location)
    {
        case WINED3D_LOCATION_TEXTURE_RGB:
            if (texture_vk->default_image_info.imageView)
            {
                wined3d_context_vk_destroy_image_view(context_vk,
                        texture_vk->default_image_info.imageView, texture_vk->command_buffer_id);
                texture_vk->default_image_info.imageView = VK_NULL_HANDLE;
            }

            if (texture_vk->vk_image)
            {
                wined3d_context_vk_destroy_image(context_vk, texture_vk->vk_image, texture_vk->command_buffer_id);
                texture_vk->vk_image = VK_NULL_HANDLE;
                if (texture_vk->memory)
                    wined3d_context_vk_destroy_allocator_block(context_vk,
                            texture_vk->memory, texture_vk->command_buffer_id);
                else
                    wined3d_context_vk_destroy_memory(context_vk,
                            texture_vk->vk_memory, texture_vk->command_buffer_id);
                texture_vk->vk_memory = VK_NULL_HANDLE;
                texture_vk->memory = NULL;
            }
            break;

        case WINED3D_LOCATION_BUFFER:
        case WINED3D_LOCATION_TEXTURE_SRGB:
        case WINED3D_LOCATION_RB_MULTISAMPLE:
        case WINED3D_LOCATION_RB_RESOLVED:
            break;

        default:
            ERR("Unhandled location %s.\n", wined3d_debug_location(location));
            break;
    }
}

static const struct wined3d_texture_ops wined3d_texture_vk_ops =
{
    wined3d_texture_vk_prepare_location,
    wined3d_texture_vk_load_location,
    wined3d_texture_vk_unload_location,
    wined3d_texture_vk_upload_data,
    wined3d_texture_vk_download_data,
};

HRESULT wined3d_texture_vk_init(struct wined3d_texture_vk *texture_vk, struct wined3d_device *device,
        const struct wined3d_resource_desc *desc, unsigned int layer_count, unsigned int level_count,
        uint32_t flags, void *parent, const struct wined3d_parent_ops *parent_ops)
{
    TRACE("texture_vk %p, device %p, desc %p, layer_count %u, "
            "level_count %u, flags %#x, parent %p, parent_ops %p.\n",
            texture_vk, device, desc, layer_count,
            level_count, flags, parent, parent_ops);

    return wined3d_texture_init(&texture_vk->t, desc, layer_count, level_count,
            flags, device, parent, parent_ops, &texture_vk[1], &wined3d_texture_vk_ops);
}

static void ffp_blitter_destroy(struct wined3d_blitter *blitter, struct wined3d_context *context)
{
    struct wined3d_blitter *next;

    if ((next = blitter->next))
        next->ops->blitter_destroy(next, context);

    heap_free(blitter);
}

static bool ffp_blit_supported(enum wined3d_blit_op blit_op, const struct wined3d_context *context,
        const struct wined3d_resource *src_resource, DWORD src_location,
        const struct wined3d_resource *dst_resource, DWORD dst_location)
{
    const struct wined3d_format *src_format = src_resource->format;
    const struct wined3d_format *dst_format = dst_resource->format;
    bool decompress;

    if (src_resource->type != WINED3D_RTYPE_TEXTURE_2D)
        return false;

    decompress = (src_format->flags[WINED3D_GL_RES_TYPE_TEX_2D] & WINED3DFMT_FLAG_COMPRESSED)
            && !(dst_format->flags[WINED3D_GL_RES_TYPE_TEX_2D] & WINED3DFMT_FLAG_COMPRESSED);
    if (!decompress && !(src_resource->access & dst_resource->access & WINED3D_RESOURCE_ACCESS_GPU))
    {
        TRACE("Source or destination resource is not GPU accessible.\n");
        return false;
    }

    if (blit_op == WINED3D_BLIT_OP_RAW_BLIT && dst_format->id == src_format->id)
    {
        if (dst_format->depth_size || dst_format->stencil_size)
            blit_op = WINED3D_BLIT_OP_DEPTH_BLIT;
        else
            blit_op = WINED3D_BLIT_OP_COLOR_BLIT;
    }

    switch (blit_op)
    {
        case WINED3D_BLIT_OP_COLOR_BLIT_CKEY:
            if (context->d3d_info->shader_color_key)
            {
                TRACE("Colour keying requires converted textures.\n");
                return false;
            }
        case WINED3D_BLIT_OP_COLOR_BLIT:
        case WINED3D_BLIT_OP_COLOR_BLIT_ALPHATEST:
            if (!wined3d_context_gl_const(context)->gl_info->supported[WINED3D_GL_LEGACY_CONTEXT])
                return false;

            if (TRACE_ON(d3d))
            {
                TRACE("Checking support for fixup:\n");
                dump_color_fixup_desc(src_format->color_fixup);
            }

            /* We only support identity conversions. */
            if (!is_identity_fixup(src_format->color_fixup)
                    || !is_identity_fixup(dst_format->color_fixup))
            {
                if (wined3d_settings.offscreen_rendering_mode == ORM_BACKBUFFER
                        && dst_format->id == src_format->id && dst_location == WINED3D_LOCATION_DRAWABLE)
                {
                    WARN("Claiming fixup support because of ORM_BACKBUFFER.\n");
                }
                else
                {
                    TRACE("Fixups are not supported.\n");
                    return false;
                }
            }

            if (!(dst_resource->bind_flags & WINED3D_BIND_RENDER_TARGET))
            {
                TRACE("Can only blit to render targets.\n");
                return false;
            }
            return true;

        default:
            TRACE("Unsupported blit operation %#x.\n", blit_op);
            return false;
    }
}

static bool is_full_clear(const struct wined3d_rendertarget_view *rtv, const RECT *draw_rect, const RECT *clear_rect)
{
    unsigned int height = rtv->height;
    unsigned int width = rtv->width;

    /* partial draw rect */
    if (draw_rect->left || draw_rect->top || draw_rect->right < width || draw_rect->bottom < height)
        return false;

    /* partial clear rect */
    if (clear_rect && (clear_rect->left > 0 || clear_rect->top > 0
            || clear_rect->right < width || clear_rect->bottom < height))
        return false;

    return true;
}

static void ffp_blitter_clear_rendertargets(struct wined3d_device *device, unsigned int rt_count,
        const struct wined3d_fb_state *fb, unsigned int rect_count, const RECT *clear_rect, const RECT *draw_rect,
        uint32_t flags, const struct wined3d_color *colour, float depth, unsigned int stencil)
{
    struct wined3d_rendertarget_view *rtv = rt_count ? fb->render_targets[0] : NULL;
    struct wined3d_rendertarget_view *dsv = fb->depth_stencil;
    const struct wined3d_state *state = &device->cs->state;
    struct wined3d_texture *depth_stencil = NULL;
    unsigned int drawable_width, drawable_height;
    const struct wined3d_gl_info *gl_info;
    struct wined3d_context_gl *context_gl;
    struct wined3d_texture *target = NULL;
    struct wined3d_color colour_srgb;
    struct wined3d_context *context;
    GLbitfield clear_mask = 0;
    bool render_offscreen;
    unsigned int i;

    if (rtv && rtv->resource->type != WINED3D_RTYPE_BUFFER)
    {
        target = texture_from_resource(rtv->resource);
        context = context_acquire(device, target, rtv->sub_resource_idx);
    }
    else
    {
        context = context_acquire(device, NULL, 0);
    }
    context_gl = wined3d_context_gl(context);

    if (dsv && dsv->resource->type != WINED3D_RTYPE_BUFFER)
        depth_stencil = texture_from_resource(dsv->resource);

    if (!context_gl->valid)
    {
        context_release(context);
        WARN("Invalid context, skipping clear.\n");
        return;
    }
    gl_info = context_gl->gl_info;

    /* When we're clearing parts of the drawable, make sure that the target
     * surface is well up to date in the drawable. After the clear we'll mark
     * the drawable up to date, so we have to make sure that this is true for
     * the cleared parts, and the untouched parts.
     *
     * If we're clearing the whole target there is no need to copy it into the
     * drawable, it will be overwritten anyway. If we're not clearing the
     * colour buffer we don't have to copy either since we're not going to set
     * the drawable up to date. We have to check all settings that limit the
     * clear area though. Do not bother checking all this if the destination
     * surface is in the drawable anyway. */
    for (i = 0; i < rt_count; ++i)
    {
        struct wined3d_rendertarget_view *rtv = fb->render_targets[i];

        if (rtv && rtv->format->id != WINED3DFMT_NULL)
        {
            struct wined3d_texture *rt = wined3d_texture_from_resource(rtv->resource);

            if (flags & WINED3DCLEAR_TARGET && !is_full_clear(rtv, draw_rect, rect_count ? clear_rect : NULL))
                wined3d_texture_load_location(rt, rtv->sub_resource_idx, context, rtv->resource->draw_binding);
            else
                wined3d_texture_prepare_location(rt, rtv->sub_resource_idx, context, rtv->resource->draw_binding);
        }
    }

    if (target)
    {
        render_offscreen = context->render_offscreen;
        wined3d_rendertarget_view_get_drawable_size(rtv, context, &drawable_width, &drawable_height);
    }
    else
    {
        unsigned int ds_level = dsv->sub_resource_idx % depth_stencil->level_count;

        render_offscreen = true;
        drawable_width = wined3d_texture_get_level_pow2_width(depth_stencil, ds_level);
        drawable_height = wined3d_texture_get_level_pow2_height(depth_stencil, ds_level);
    }

    if (depth_stencil)
    {
        DWORD ds_location = render_offscreen ? dsv->resource->draw_binding : WINED3D_LOCATION_DRAWABLE;
        struct wined3d_texture *ds = wined3d_texture_from_resource(dsv->resource);

        if (flags & (WINED3DCLEAR_ZBUFFER | WINED3DCLEAR_STENCIL)
                && !is_full_clear(dsv, draw_rect, rect_count ? clear_rect : NULL))
            wined3d_texture_load_location(ds, dsv->sub_resource_idx, context, ds_location);
        else
            wined3d_texture_prepare_location(ds, dsv->sub_resource_idx, context, ds_location);

        if (flags & (WINED3DCLEAR_ZBUFFER | WINED3DCLEAR_STENCIL))
        {
            wined3d_texture_validate_location(ds, dsv->sub_resource_idx, ds_location);
            wined3d_texture_invalidate_location(ds, dsv->sub_resource_idx, ~ds_location);
        }
    }

    if (!wined3d_context_gl_apply_clear_state(context_gl, state, rt_count, fb))
    {
        context_release(context);
        WARN("Failed to apply clear state, skipping clear.\n");
        return;
    }

    /* Only set the values up once, as they are not changing. */
    if (flags & WINED3DCLEAR_STENCIL)
    {
        if (gl_info->supported[EXT_STENCIL_TWO_SIDE])
        {
            gl_info->gl_ops.gl.p_glDisable(GL_STENCIL_TEST_TWO_SIDE_EXT);
            context_invalidate_state(context, STATE_RENDER(WINED3D_RS_TWOSIDEDSTENCILMODE));
        }
        gl_info->gl_ops.gl.p_glStencilMask(~0u);
        context_invalidate_state(context, STATE_RENDER(WINED3D_RS_STENCILWRITEMASK));
        gl_info->gl_ops.gl.p_glClearStencil(stencil);
        checkGLcall("glClearStencil");
        clear_mask = clear_mask | GL_STENCIL_BUFFER_BIT;
    }

    if (flags & WINED3DCLEAR_ZBUFFER)
    {
        gl_info->gl_ops.gl.p_glDepthMask(GL_TRUE);
        context_invalidate_state(context, STATE_RENDER(WINED3D_RS_ZWRITEENABLE));
        gl_info->gl_ops.gl.p_glClearDepth(depth);
        checkGLcall("glClearDepth");
        clear_mask = clear_mask | GL_DEPTH_BUFFER_BIT;
    }

    if (flags & WINED3DCLEAR_TARGET)
    {
        for (i = 0; i < rt_count; ++i)
        {
            struct wined3d_rendertarget_view *rtv = fb->render_targets[i];
            struct wined3d_texture *texture;

            if (!rtv)
                continue;

            if (rtv->resource->type == WINED3D_RTYPE_BUFFER)
            {
                FIXME("Not supported on buffer resources.\n");
                continue;
            }

            texture = texture_from_resource(rtv->resource);
            wined3d_texture_validate_location(texture, rtv->sub_resource_idx, rtv->resource->draw_binding);
            wined3d_texture_invalidate_location(texture, rtv->sub_resource_idx, ~rtv->resource->draw_binding);
        }

        if (!gl_info->supported[ARB_FRAMEBUFFER_SRGB] && needs_srgb_write(context->d3d_info, state, fb))
        {
            if (rt_count > 1)
                WARN("Clearing multiple sRGB render targets without GL_ARB_framebuffer_sRGB "
                        "support, this might cause graphical issues.\n");

            wined3d_colour_srgb_from_linear(&colour_srgb, colour);
            colour = &colour_srgb;
        }

        gl_info->gl_ops.gl.p_glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        context_invalidate_state(context, STATE_BLEND);
        gl_info->gl_ops.gl.p_glClearColor(colour->r, colour->g, colour->b, colour->a);
        checkGLcall("glClearColor");
        clear_mask = clear_mask | GL_COLOR_BUFFER_BIT;
    }

    if (!rect_count)
    {
        if (render_offscreen)
        {
            gl_info->gl_ops.gl.p_glScissor(draw_rect->left, draw_rect->top,
                    draw_rect->right - draw_rect->left, draw_rect->bottom - draw_rect->top);
        }
        else
        {
            gl_info->gl_ops.gl.p_glScissor(draw_rect->left, drawable_height - draw_rect->bottom,
                        draw_rect->right - draw_rect->left, draw_rect->bottom - draw_rect->top);
        }
        gl_info->gl_ops.gl.p_glClear(clear_mask);
    }
    else
    {
        RECT current_rect;

        /* Now process each rect in turn. */
        for (i = 0; i < rect_count; ++i)
        {
            /* Note that GL uses lower left, width/height. */
            IntersectRect(&current_rect, draw_rect, &clear_rect[i]);

            TRACE("clear_rect[%u] %s, current_rect %s.\n", i,
                    wine_dbgstr_rect(&clear_rect[i]),
                    wine_dbgstr_rect(&current_rect));

            /* Tests show that rectangles where x1 > x2 or y1 > y2 are ignored
             * silently. The rectangle is not cleared, no error is returned,
             * but further rectangles are still cleared if they are valid. */
            if (current_rect.left > current_rect.right || current_rect.top > current_rect.bottom)
            {
                TRACE("Rectangle with negative dimensions, ignoring.\n");
                continue;
            }

            if (render_offscreen)
            {
                gl_info->gl_ops.gl.p_glScissor(current_rect.left, current_rect.top,
                        current_rect.right - current_rect.left, current_rect.bottom - current_rect.top);
            }
            else
            {
                gl_info->gl_ops.gl.p_glScissor(current_rect.left, drawable_height - current_rect.bottom,
                          current_rect.right - current_rect.left, current_rect.bottom - current_rect.top);
            }
            gl_info->gl_ops.gl.p_glClear(clear_mask);
        }
    }
    context->scissor_rect_count = WINED3D_MAX_VIEWPORTS;
    checkGLcall("clear");

    if (flags & WINED3DCLEAR_TARGET && target->swapchain && target->swapchain->front_buffer == target)
        gl_info->gl_ops.gl.p_glFlush();

    context_release(context);
}

static bool blitter_use_cpu_clear(struct wined3d_rendertarget_view *view)
{
    struct wined3d_resource *resource;
    struct wined3d_texture *texture;
    DWORD locations;

    resource = view->resource;
    if (resource->type == WINED3D_RTYPE_BUFFER)
        return !(resource->access & WINED3D_RESOURCE_ACCESS_GPU);

    texture = texture_from_resource(resource);
    locations = texture->sub_resources[view->sub_resource_idx].locations;
    if (locations & (resource->map_binding | WINED3D_LOCATION_DISCARDED))
        return !(resource->access & WINED3D_RESOURCE_ACCESS_GPU)
                || (texture->flags & WINED3D_TEXTURE_PIN_SYSMEM);

    return !(resource->access & WINED3D_RESOURCE_ACCESS_GPU)
            && !(texture->flags & WINED3D_TEXTURE_CONVERTED);
}

static void ffp_blitter_clear(struct wined3d_blitter *blitter, struct wined3d_device *device,
        unsigned int rt_count, const struct wined3d_fb_state *fb, unsigned int rect_count, const RECT *clear_rects,
        const RECT *draw_rect, DWORD flags, const struct wined3d_color *colour, float depth, DWORD stencil)
{
    struct wined3d_rendertarget_view *view, *previous = NULL;
    bool have_identical_size = TRUE;
    struct wined3d_fb_state tmp_fb;
    unsigned int next_rt_count = 0;
    struct wined3d_blitter *next;
    DWORD next_flags = 0;
    unsigned int i;

    if (flags & WINED3DCLEAR_TARGET)
    {
        for (i = 0; i < rt_count; ++i)
        {
            if (!(view = fb->render_targets[i]))
                continue;

            if (blitter_use_cpu_clear(view)
                    || (!(view->resource->bind_flags & WINED3D_BIND_RENDER_TARGET)
                    && (wined3d_settings.offscreen_rendering_mode != ORM_FBO
                    || !(view->format_flags & WINED3DFMT_FLAG_FBO_ATTACHABLE))))
            {
                next_flags |= WINED3DCLEAR_TARGET;
                flags &= ~WINED3DCLEAR_TARGET;
                next_rt_count = rt_count;
                rt_count = 0;
                break;
            }

            /* FIXME: We should reject colour fills on formats with fixups,
             * but this would break P8 colour fills for example. */
        }
    }

    if ((flags & (WINED3DCLEAR_ZBUFFER | WINED3DCLEAR_STENCIL)) && (view = fb->depth_stencil)
            && (!view->format->depth_size || (flags & WINED3DCLEAR_ZBUFFER))
            && (!view->format->stencil_size || (flags & WINED3DCLEAR_STENCIL))
            && blitter_use_cpu_clear(view))
    {
        next_flags |= flags & (WINED3DCLEAR_ZBUFFER | WINED3DCLEAR_STENCIL);
        flags &= ~(WINED3DCLEAR_ZBUFFER | WINED3DCLEAR_STENCIL);
    }

    if (flags)
    {
        for (i = 0; i < rt_count; ++i)
        {
            if (!(view = fb->render_targets[i]))
                continue;

            if (previous && (previous->width != view->width || previous->height != view->height))
                have_identical_size = false;
            previous = view;
        }
        if (flags & (WINED3DCLEAR_ZBUFFER | WINED3DCLEAR_STENCIL))
        {
            view = fb->depth_stencil;

            if (previous && (previous->width != view->width || previous->height != view->height))
                have_identical_size = false;
        }

        if (have_identical_size)
        {
            ffp_blitter_clear_rendertargets(device, rt_count, fb, rect_count,
                    clear_rects, draw_rect, flags, colour, depth, stencil);
        }
        else
        {
            for (i = 0; i < rt_count; ++i)
            {
                if (!(view = fb->render_targets[i]))
                    continue;

                tmp_fb.render_targets[0] = view;
                tmp_fb.depth_stencil = NULL;
                ffp_blitter_clear_rendertargets(device, 1, &tmp_fb, rect_count,
                        clear_rects, draw_rect, WINED3DCLEAR_TARGET, colour, depth, stencil);
            }
            if (flags & (WINED3DCLEAR_ZBUFFER | WINED3DCLEAR_STENCIL))
            {
                tmp_fb.render_targets[0] = NULL;
                tmp_fb.depth_stencil = fb->depth_stencil;
                ffp_blitter_clear_rendertargets(device, 0, &tmp_fb, rect_count,
                        clear_rects, draw_rect, flags & ~WINED3DCLEAR_TARGET, colour, depth, stencil);
            }
        }
    }

    if (next_flags && (next = blitter->next))
        next->ops->blitter_clear(next, device, next_rt_count, fb, rect_count,
                clear_rects, draw_rect, next_flags, colour, depth, stencil);
}

static DWORD ffp_blitter_blit(struct wined3d_blitter *blitter, enum wined3d_blit_op op,
        struct wined3d_context *context, struct wined3d_texture *src_texture, unsigned int src_sub_resource_idx,
        DWORD src_location, const RECT *src_rect, struct wined3d_texture *dst_texture,
        unsigned int dst_sub_resource_idx, DWORD dst_location, const RECT *dst_rect,
        const struct wined3d_color_key *colour_key, enum wined3d_texture_filter_type filter)
{
    struct wined3d_texture_gl *src_texture_gl = wined3d_texture_gl(src_texture);
    struct wined3d_context_gl *context_gl = wined3d_context_gl(context);
    const struct wined3d_gl_info *gl_info = context_gl->gl_info;
    struct wined3d_resource *src_resource, *dst_resource;
    struct wined3d_texture *staging_texture = NULL;
    struct wined3d_color_key old_blt_key;
    struct wined3d_device *device;
    struct wined3d_blitter *next;
    DWORD old_colour_key_flags;
    RECT r;

    src_resource = &src_texture->resource;
    dst_resource = &dst_texture->resource;
    device = dst_resource->device;

    if (!ffp_blit_supported(op, context, src_resource, src_location, dst_resource, dst_location))
    {
        if ((next = blitter->next))
            return next->ops->blitter_blit(next, op, context, src_texture, src_sub_resource_idx, src_location,
                    src_rect, dst_texture, dst_sub_resource_idx, dst_location, dst_rect, colour_key, filter);
    }

    TRACE("Blt from texture %p, %u to rendertarget %p, %u.\n",
            src_texture, src_sub_resource_idx, dst_texture, dst_sub_resource_idx);

    old_blt_key = src_texture->async.src_blt_color_key;
    old_colour_key_flags = src_texture->async.color_key_flags;
    wined3d_texture_set_color_key(src_texture, WINED3D_CKEY_SRC_BLT, colour_key);

    if (!(src_texture->resource.access & WINED3D_RESOURCE_ACCESS_GPU))
    {
        struct wined3d_resource_desc desc;
        struct wined3d_box upload_box;
        unsigned int src_level;
        HRESULT hr;

        TRACE("Source texture is not GPU accessible, creating a staging texture.\n");

        src_level = src_sub_resource_idx % src_texture->level_count;
        desc.resource_type = WINED3D_RTYPE_TEXTURE_2D;
        desc.format = src_texture->resource.format->id;
        desc.multisample_type = src_texture->resource.multisample_type;
        desc.multisample_quality = src_texture->resource.multisample_quality;
        desc.usage = WINED3DUSAGE_PRIVATE;
        desc.bind_flags = 0;
        desc.access = WINED3D_RESOURCE_ACCESS_GPU;
        desc.width = wined3d_texture_get_level_width(src_texture, src_level);
        desc.height = wined3d_texture_get_level_height(src_texture, src_level);
        desc.depth = 1;
        desc.size = 0;

        if (FAILED(hr = wined3d_texture_create(device, &desc, 1, 1, 0,
                NULL, NULL, &wined3d_null_parent_ops, &staging_texture)))
        {
            ERR("Failed to create staging texture, hr %#x.\n", hr);
            return dst_location;
        }

        wined3d_box_set(&upload_box, 0, 0, desc.width, desc.height, 0, desc.depth);
        wined3d_texture_upload_from_texture(staging_texture, 0, 0, 0, 0,
                src_texture, src_sub_resource_idx, &upload_box);

        src_texture = staging_texture;
        src_texture_gl = wined3d_texture_gl(src_texture);
        src_sub_resource_idx = 0;
    }
    else
    {
        /* Make sure the surface is up-to-date. This should probably use
         * surface_load_location() and worry about the destination surface
         * too, unless we're overwriting it completely. */
        wined3d_texture_load(src_texture, context, FALSE);
    }

    wined3d_context_gl_apply_ffp_blit_state(context_gl, device);

    if (dst_location == WINED3D_LOCATION_DRAWABLE)
    {
        r = *dst_rect;
        wined3d_texture_translate_drawable_coords(dst_texture, context_gl->window, &r);
        dst_rect = &r;
    }

    if (wined3d_settings.offscreen_rendering_mode == ORM_FBO)
    {
        GLenum buffer;

        if (dst_location == WINED3D_LOCATION_DRAWABLE)
        {
            TRACE("Destination texture %p is onscreen.\n", dst_texture);
            buffer = wined3d_texture_get_gl_buffer(dst_texture);
        }
        else
        {
            TRACE("Destination texture %p is offscreen.\n", dst_texture);
            buffer = GL_COLOR_ATTACHMENT0;
        }
        wined3d_context_gl_apply_fbo_state_blit(context_gl, GL_DRAW_FRAMEBUFFER,
                dst_resource, dst_sub_resource_idx, NULL, 0, dst_location);
        wined3d_context_gl_set_draw_buffer(context_gl, buffer);
        wined3d_context_gl_check_fbo_status(context_gl, GL_DRAW_FRAMEBUFFER);
        context_invalidate_state(context, STATE_FRAMEBUFFER);
    }

    gl_info->gl_ops.gl.p_glEnable(src_texture_gl->target);
    checkGLcall("glEnable(target)");

    if (op == WINED3D_BLIT_OP_COLOR_BLIT_ALPHATEST || colour_key)
    {
        gl_info->gl_ops.gl.p_glEnable(GL_ALPHA_TEST);
        checkGLcall("glEnable(GL_ALPHA_TEST)");
    }

    if (colour_key)
    {
        /* For P8 surfaces, the alpha component contains the palette index.
         * Which means that the colourkey is one of the palette entries. In
         * other cases pixels that should be masked away have alpha set to 0. */
        if (src_texture->resource.format->id == WINED3DFMT_P8_UINT)
            gl_info->gl_ops.gl.p_glAlphaFunc(GL_NOTEQUAL,
                    (float)src_texture->async.src_blt_color_key.color_space_low_value / 255.0f);
        else
            gl_info->gl_ops.gl.p_glAlphaFunc(GL_NOTEQUAL, 0.0f);
        checkGLcall("glAlphaFunc");
    }

    wined3d_context_gl_draw_textured_quad(context_gl, src_texture_gl,
            src_sub_resource_idx, src_rect, dst_rect, filter);

    if (op == WINED3D_BLIT_OP_COLOR_BLIT_ALPHATEST || colour_key)
    {
        gl_info->gl_ops.gl.p_glDisable(GL_ALPHA_TEST);
        checkGLcall("glDisable(GL_ALPHA_TEST)");
    }

    gl_info->gl_ops.gl.p_glDisable(GL_TEXTURE_2D);
    checkGLcall("glDisable(GL_TEXTURE_2D)");
    if (gl_info->supported[ARB_TEXTURE_CUBE_MAP])
    {
        gl_info->gl_ops.gl.p_glDisable(GL_TEXTURE_CUBE_MAP_ARB);
        checkGLcall("glDisable(GL_TEXTURE_CUBE_MAP_ARB)");
    }
    if (gl_info->supported[ARB_TEXTURE_RECTANGLE])
    {
        gl_info->gl_ops.gl.p_glDisable(GL_TEXTURE_RECTANGLE_ARB);
        checkGLcall("glDisable(GL_TEXTURE_RECTANGLE_ARB)");
    }

    if (dst_texture->swapchain && dst_texture->swapchain->front_buffer == dst_texture)
        gl_info->gl_ops.gl.p_glFlush();

    /* Restore the colour key parameters */
    wined3d_texture_set_color_key(src_texture, WINED3D_CKEY_SRC_BLT,
            (old_colour_key_flags & WINED3D_CKEY_SRC_BLT) ? &old_blt_key : NULL);

    if (staging_texture)
        wined3d_texture_decref(staging_texture);

    return dst_location;
}

static const struct wined3d_blitter_ops ffp_blitter_ops =
{
    ffp_blitter_destroy,
    ffp_blitter_clear,
    ffp_blitter_blit,
};

void wined3d_ffp_blitter_create(struct wined3d_blitter **next, const struct wined3d_gl_info *gl_info)
{
    struct wined3d_blitter *blitter;

    if (!(blitter = heap_alloc(sizeof(*blitter))))
        return;

    TRACE("Created blitter %p.\n", blitter);

    blitter->ops = &ffp_blitter_ops;
    blitter->next = *next;
    *next = blitter;
}

static void fbo_blitter_destroy(struct wined3d_blitter *blitter, struct wined3d_context *context)
{
    struct wined3d_blitter *next;

    if ((next = blitter->next))
        next->ops->blitter_destroy(next, context);

    heap_free(blitter);
}

static void fbo_blitter_clear(struct wined3d_blitter *blitter, struct wined3d_device *device,
        unsigned int rt_count, const struct wined3d_fb_state *fb, unsigned int rect_count, const RECT *clear_rects,
        const RECT *draw_rect, DWORD flags, const struct wined3d_color *colour, float depth, DWORD stencil)
{
    struct wined3d_blitter *next;

    if ((next = blitter->next))
        next->ops->blitter_clear(next, device, rt_count, fb, rect_count,
                clear_rects, draw_rect, flags, colour, depth, stencil);
}

static DWORD fbo_blitter_blit(struct wined3d_blitter *blitter, enum wined3d_blit_op op,
        struct wined3d_context *context, struct wined3d_texture *src_texture, unsigned int src_sub_resource_idx,
        DWORD src_location, const RECT *src_rect, struct wined3d_texture *dst_texture,
        unsigned int dst_sub_resource_idx, DWORD dst_location, const RECT *dst_rect,
        const struct wined3d_color_key *colour_key, enum wined3d_texture_filter_type filter)
{
    struct wined3d_context_gl *context_gl = wined3d_context_gl(context);
    struct wined3d_resource *src_resource, *dst_resource;
    enum wined3d_blit_op blit_op = op;
    struct wined3d_device *device;
    struct wined3d_blitter *next;

    TRACE("blitter %p, op %#x, context %p, src_texture %p, src_sub_resource_idx %u, src_location %s, src_rect %s, "
            "dst_texture %p, dst_sub_resource_idx %u, dst_location %s, dst_rect %s, colour_key %p, filter %s.\n",
            blitter, op, context, src_texture, src_sub_resource_idx, wined3d_debug_location(src_location),
            wine_dbgstr_rect(src_rect), dst_texture, dst_sub_resource_idx, wined3d_debug_location(dst_location),
            wine_dbgstr_rect(dst_rect), colour_key, debug_d3dtexturefiltertype(filter));

    src_resource = &src_texture->resource;
    dst_resource = &dst_texture->resource;

    device = dst_resource->device;

    if (blit_op == WINED3D_BLIT_OP_RAW_BLIT && dst_resource->format->id == src_resource->format->id)
    {
        if (dst_resource->format->depth_size || dst_resource->format->stencil_size)
            blit_op = WINED3D_BLIT_OP_DEPTH_BLIT;
        else
            blit_op = WINED3D_BLIT_OP_COLOR_BLIT;
    }

    if (!fbo_blitter_supported(blit_op, context_gl->gl_info,
            src_resource, src_location, dst_resource, dst_location))
    {
        if (!(next = blitter->next))
        {
            ERR("No blitter to handle blit op %#x.\n", op);
            return dst_location;
        }

        TRACE("Forwarding to blitter %p.\n", next);
        return next->ops->blitter_blit(next, op, context, src_texture, src_sub_resource_idx, src_location,
                src_rect, dst_texture, dst_sub_resource_idx, dst_location, dst_rect, colour_key, filter);
    }

    if (blit_op == WINED3D_BLIT_OP_COLOR_BLIT)
    {
        TRACE("Colour blit.\n");
        texture2d_blt_fbo(device, context, filter, src_texture, src_sub_resource_idx, src_location,
                src_rect, dst_texture, dst_sub_resource_idx, dst_location, dst_rect);
        return dst_location;
    }

    if (blit_op == WINED3D_BLIT_OP_DEPTH_BLIT)
    {
        TRACE("Depth/stencil blit.\n");
        texture2d_depth_blt_fbo(device, context, src_texture, src_sub_resource_idx, src_location,
                src_rect, dst_texture, dst_sub_resource_idx, dst_location, dst_rect);
        return dst_location;
    }

    ERR("This blitter does not implement blit op %#x.\n", blit_op);
    return dst_location;
}

static const struct wined3d_blitter_ops fbo_blitter_ops =
{
    fbo_blitter_destroy,
    fbo_blitter_clear,
    fbo_blitter_blit,
};

void wined3d_fbo_blitter_create(struct wined3d_blitter **next, const struct wined3d_gl_info *gl_info)
{
    struct wined3d_blitter *blitter;

    if ((wined3d_settings.offscreen_rendering_mode != ORM_FBO) || !gl_info->fbo_ops.glBlitFramebuffer)
        return;

    if (!(blitter = heap_alloc(sizeof(*blitter))))
        return;

    TRACE("Created blitter %p.\n", blitter);

    blitter->ops = &fbo_blitter_ops;
    blitter->next = *next;
    *next = blitter;
}

static void raw_blitter_destroy(struct wined3d_blitter *blitter, struct wined3d_context *context)
{
    struct wined3d_blitter *next;

    if ((next = blitter->next))
        next->ops->blitter_destroy(next, context);

    heap_free(blitter);
}

static void raw_blitter_clear(struct wined3d_blitter *blitter, struct wined3d_device *device,
        unsigned int rt_count, const struct wined3d_fb_state *fb, unsigned int rect_count, const RECT *clear_rects,
        const RECT *draw_rect, DWORD flags, const struct wined3d_color *colour, float depth, DWORD stencil)
{
    struct wined3d_blitter *next;

    if (!(next = blitter->next))
    {
        ERR("No blitter to handle clear.\n");
        return;
    }

    TRACE("Forwarding to blitter %p.\n", next);
    next->ops->blitter_clear(next, device, rt_count, fb, rect_count,
            clear_rects, draw_rect, flags, colour, depth, stencil);
}

static DWORD raw_blitter_blit(struct wined3d_blitter *blitter, enum wined3d_blit_op op,
        struct wined3d_context *context, struct wined3d_texture *src_texture, unsigned int src_sub_resource_idx,
        DWORD src_location, const RECT *src_rect, struct wined3d_texture *dst_texture,
        unsigned int dst_sub_resource_idx, DWORD dst_location, const RECT *dst_rect,
        const struct wined3d_color_key *colour_key, enum wined3d_texture_filter_type filter)
{
    struct wined3d_texture_gl *src_texture_gl = wined3d_texture_gl(src_texture);
    struct wined3d_texture_gl *dst_texture_gl = wined3d_texture_gl(dst_texture);
    struct wined3d_context_gl *context_gl = wined3d_context_gl(context);
    const struct wined3d_gl_info *gl_info = context_gl->gl_info;
    unsigned int src_level, src_layer, dst_level, dst_layer;
    struct wined3d_blitter *next;
    GLuint src_name, dst_name;
    DWORD location;

    /* If we would need to copy from a renderbuffer or drawable, we'd probably
     * be better off using the FBO blitter directly, since we'd need to use it
     * to copy the resource contents to the texture anyway. */
    if (op != WINED3D_BLIT_OP_RAW_BLIT
            || (src_texture->resource.format->id == dst_texture->resource.format->id
            && (!(src_location & (WINED3D_LOCATION_TEXTURE_RGB | WINED3D_LOCATION_TEXTURE_SRGB))
            || !(dst_location & (WINED3D_LOCATION_TEXTURE_RGB | WINED3D_LOCATION_TEXTURE_SRGB)))))
    {
        if (!(next = blitter->next))
        {
            ERR("No blitter to handle blit op %#x.\n", op);
            return dst_location;
        }

        TRACE("Forwarding to blitter %p.\n", next);
        return next->ops->blitter_blit(next, op, context, src_texture, src_sub_resource_idx, src_location,
                src_rect, dst_texture, dst_sub_resource_idx, dst_location, dst_rect, colour_key, filter);
    }

    TRACE("Blit using ARB_copy_image.\n");

    src_level = src_sub_resource_idx % src_texture->level_count;
    src_layer = src_sub_resource_idx / src_texture->level_count;

    dst_level = dst_sub_resource_idx % dst_texture->level_count;
    dst_layer = dst_sub_resource_idx / dst_texture->level_count;

    location = src_location & (WINED3D_LOCATION_TEXTURE_RGB | WINED3D_LOCATION_TEXTURE_SRGB);
    if (!location)
        location = src_texture->flags & WINED3D_TEXTURE_IS_SRGB
                ? WINED3D_LOCATION_TEXTURE_SRGB : WINED3D_LOCATION_TEXTURE_RGB;
    if (!wined3d_texture_load_location(src_texture, src_sub_resource_idx, context, location))
        ERR("Failed to load the source sub-resource into %s.\n", wined3d_debug_location(location));
    src_name = wined3d_texture_gl_get_texture_name(src_texture_gl,
            context, location == WINED3D_LOCATION_TEXTURE_SRGB);

    location = dst_location & (WINED3D_LOCATION_TEXTURE_RGB | WINED3D_LOCATION_TEXTURE_SRGB);
    if (!location)
        location = dst_texture->flags & WINED3D_TEXTURE_IS_SRGB
                ? WINED3D_LOCATION_TEXTURE_SRGB : WINED3D_LOCATION_TEXTURE_RGB;
    if (wined3d_texture_is_full_rect(dst_texture, dst_level, dst_rect))
    {
        if (!wined3d_texture_prepare_location(dst_texture, dst_sub_resource_idx, context, location))
            ERR("Failed to prepare the destination sub-resource into %s.\n", wined3d_debug_location(location));
    }
    else
    {
        if (!wined3d_texture_load_location(dst_texture, dst_sub_resource_idx, context, location))
            ERR("Failed to load the destination sub-resource into %s.\n", wined3d_debug_location(location));
    }
    dst_name = wined3d_texture_gl_get_texture_name(dst_texture_gl,
            context, location == WINED3D_LOCATION_TEXTURE_SRGB);

    GL_EXTCALL(glCopyImageSubData(src_name, src_texture_gl->target, src_level,
            src_rect->left, src_rect->top, src_layer, dst_name, dst_texture_gl->target, dst_level,
            dst_rect->left, dst_rect->top, dst_layer, src_rect->right - src_rect->left,
            src_rect->bottom - src_rect->top, 1));
    checkGLcall("copy image data");

    wined3d_texture_validate_location(dst_texture, dst_sub_resource_idx, location);
    wined3d_texture_invalidate_location(dst_texture, dst_sub_resource_idx, ~location);
    if (!wined3d_texture_load_location(dst_texture, dst_sub_resource_idx, context, dst_location))
        ERR("Failed to load the destination sub-resource into %s.\n", wined3d_debug_location(dst_location));

    return dst_location | location;
}

static const struct wined3d_blitter_ops raw_blitter_ops =
{
    raw_blitter_destroy,
    raw_blitter_clear,
    raw_blitter_blit,
};

void wined3d_raw_blitter_create(struct wined3d_blitter **next, const struct wined3d_gl_info *gl_info)
{
    struct wined3d_blitter *blitter;

    if (!gl_info->supported[ARB_COPY_IMAGE])
        return;

    if (!(blitter = heap_alloc(sizeof(*blitter))))
        return;

    TRACE("Created blitter %p.\n", blitter);

    blitter->ops = &raw_blitter_ops;
    blitter->next = *next;
    *next = blitter;
}

static void vk_blitter_destroy(struct wined3d_blitter *blitter, struct wined3d_context *context)
{
    struct wined3d_blitter *next;

    TRACE("blitter %p, context %p.\n", blitter, context);

    if ((next = blitter->next))
        next->ops->blitter_destroy(next, context);

    heap_free(blitter);
}

static void vk_blitter_clear_rendertargets(struct wined3d_context_vk *context_vk, unsigned int rt_count,
        const struct wined3d_fb_state *fb, unsigned int rect_count, const RECT *clear_rects, const RECT *draw_rect,
        uint32_t flags, const struct wined3d_color *colour, float depth, unsigned int stencil)
{
    VkClearValue clear_values[WINED3D_MAX_RENDER_TARGETS + 1];
    VkImageView views[WINED3D_MAX_RENDER_TARGETS + 1];
    struct wined3d_rendertarget_view_vk *rtv_vk;
    struct wined3d_rendertarget_view *view;
    const struct wined3d_vk_info *vk_info;
    struct wined3d_texture_vk *texture_vk;
    struct wined3d_device_vk *device_vk;
    VkCommandBuffer vk_command_buffer;
    VkRenderPassBeginInfo begin_desc;
    unsigned int i, attachment_count;
    VkFramebufferCreateInfo fb_desc;
    VkFramebuffer vk_framebuffer;
    VkRenderPass vk_render_pass;
    bool depth_stencil = false;
    unsigned int layer_count;
    VkClearColorValue *c;
    VkResult vr;
    RECT r;

    TRACE("context_vk %p, rt_count %u, fb %p, rect_count %u, clear_rects %p, "
            "draw_rect %s, flags %#x, colour %s, depth %.8e, stencil %#x.\n",
            context_vk, rt_count, fb, rect_count, clear_rects,
            wine_dbgstr_rect(draw_rect), flags, debug_color(colour), depth, stencil);

    device_vk = wined3d_device_vk(context_vk->c.device);
    vk_info = context_vk->vk_info;

    if (!(flags & WINED3DCLEAR_TARGET))
        rt_count = 0;

    for (i = 0, attachment_count = 0, layer_count = 1; i < rt_count; ++i)
    {
        if (!(view = fb->render_targets[i]))
            continue;

        if (!is_full_clear(view, draw_rect, clear_rects))
            wined3d_rendertarget_view_load_location(view, &context_vk->c, view->resource->draw_binding);
        else
            wined3d_rendertarget_view_prepare_location(view, &context_vk->c, view->resource->draw_binding);
        wined3d_rendertarget_view_validate_location(view, view->resource->draw_binding);
        wined3d_rendertarget_view_invalidate_location(view, ~view->resource->draw_binding);

        rtv_vk = wined3d_rendertarget_view_vk(view);
        views[attachment_count] = wined3d_rendertarget_view_vk_get_image_view(rtv_vk, context_vk);

        c = &clear_values[attachment_count].color;
        if (view->format_flags & WINED3DFMT_FLAG_INTEGER)
        {
            c->int32[0] = colour->r;
            c->int32[1] = colour->g;
            c->int32[2] = colour->b;
            c->int32[3] = colour->a;
        }
        else
        {
            c->float32[0] = colour->r;
            c->float32[1] = colour->g;
            c->float32[2] = colour->b;
            c->float32[3] = colour->a;
        }

        if (view->layer_count > layer_count)
            layer_count = view->layer_count;

        ++attachment_count;
    }

    if (flags & (WINED3DCLEAR_ZBUFFER | WINED3DCLEAR_STENCIL) && (view = fb->depth_stencil))
    {
        if (!is_full_clear(view, draw_rect, clear_rects))
            wined3d_rendertarget_view_load_location(view, &context_vk->c, view->resource->draw_binding);
        else
            wined3d_rendertarget_view_prepare_location(view, &context_vk->c, view->resource->draw_binding);
        wined3d_rendertarget_view_validate_location(view, view->resource->draw_binding);
        wined3d_rendertarget_view_invalidate_location(view, ~view->resource->draw_binding);

        rtv_vk = wined3d_rendertarget_view_vk(view);
        views[attachment_count] = wined3d_rendertarget_view_vk_get_image_view(rtv_vk, context_vk);

        clear_values[attachment_count].depthStencil.depth = depth;
        clear_values[attachment_count].depthStencil.stencil = stencil;

        if (view->layer_count > layer_count)
            layer_count = view->layer_count;

        depth_stencil = true;
        ++attachment_count;
    }

    if (!attachment_count)
        return;

    if (!(vk_render_pass = wined3d_context_vk_get_render_pass(context_vk, fb,
            rt_count, flags & (WINED3DCLEAR_ZBUFFER | WINED3DCLEAR_STENCIL), flags)))
    {
        ERR("Failed to get render pass.\n");
        return;
    }

    if (!(vk_command_buffer = wined3d_context_vk_get_command_buffer(context_vk)))
    {
        ERR("Failed to get command buffer.\n");
        return;
    }

    fb_desc.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fb_desc.pNext = NULL;
    fb_desc.flags = 0;
    fb_desc.renderPass = vk_render_pass;
    fb_desc.attachmentCount = attachment_count;
    fb_desc.pAttachments = views;
    fb_desc.width = draw_rect->right - draw_rect->left;
    fb_desc.height = draw_rect->bottom - draw_rect->top;
    fb_desc.layers = layer_count;
    if ((vr = VK_CALL(vkCreateFramebuffer(device_vk->vk_device, &fb_desc, NULL, &vk_framebuffer))) < 0)
    {
        ERR("Failed to create Vulkan framebuffer, vr %s.\n", wined3d_debug_vkresult(vr));
        return;
    }

    begin_desc.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin_desc.pNext = NULL;
    begin_desc.renderPass = vk_render_pass;
    begin_desc.framebuffer = vk_framebuffer;
    begin_desc.clearValueCount = attachment_count;
    begin_desc.pClearValues = clear_values;

    wined3d_context_vk_end_current_render_pass(context_vk);

    for (i = 0; i < rect_count; ++i)
    {
        r.left = max(clear_rects[i].left, draw_rect->left);
        r.top = max(clear_rects[i].top, draw_rect->top);
        r.right = min(clear_rects[i].right, draw_rect->right);
        r.bottom = min(clear_rects[i].bottom, draw_rect->bottom);

        if (r.left >= r.right || r.top >= r.bottom)
            continue;

        begin_desc.renderArea.offset.x = r.left;
        begin_desc.renderArea.offset.y = r.top;
        begin_desc.renderArea.extent.width = r.right - r.left;
        begin_desc.renderArea.extent.height = r.bottom - r.top;
        VK_CALL(vkCmdBeginRenderPass(vk_command_buffer, &begin_desc, VK_SUBPASS_CONTENTS_INLINE));
        VK_CALL(vkCmdEndRenderPass(vk_command_buffer));
    }

    wined3d_context_vk_destroy_framebuffer(context_vk, vk_framebuffer, context_vk->current_command_buffer.id);

    for (i = 0; i < rt_count; ++i)
    {
        if (!(view = fb->render_targets[i]))
            continue;

        wined3d_context_vk_reference_rendertarget_view(context_vk, wined3d_rendertarget_view_vk(view));
        texture_vk = wined3d_texture_vk(wined3d_texture_from_resource(view->resource));
        wined3d_context_vk_image_barrier(context_vk, vk_command_buffer,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                vk_access_mask_from_bind_flags(texture_vk->t.resource.bind_flags),
                texture_vk->layout, texture_vk->layout,
                texture_vk->vk_image, VK_IMAGE_ASPECT_COLOR_BIT);
    }

    if (depth_stencil)
    {
        view = fb->depth_stencil;
        wined3d_context_vk_reference_rendertarget_view(context_vk, wined3d_rendertarget_view_vk(view));
        texture_vk = wined3d_texture_vk(wined3d_texture_from_resource(view->resource));
        wined3d_context_vk_image_barrier(context_vk, vk_command_buffer,
                VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                vk_access_mask_from_bind_flags(texture_vk->t.resource.bind_flags),
                texture_vk->layout, texture_vk->layout,
                texture_vk->vk_image, vk_aspect_mask_from_format(texture_vk->t.resource.format));
    }
}

static void vk_blitter_clear(struct wined3d_blitter *blitter, struct wined3d_device *device,
        unsigned int rt_count, const struct wined3d_fb_state *fb, unsigned int rect_count, const RECT *clear_rects,
        const RECT *draw_rect, DWORD flags, const struct wined3d_color *colour, float depth, DWORD stencil)
{
    struct wined3d_device_vk *device_vk = wined3d_device_vk(device);
    struct wined3d_rendertarget_view *view, *previous = NULL;
    struct wined3d_context_vk *context_vk;
    bool have_identical_size = true;
    struct wined3d_fb_state tmp_fb;
    unsigned int next_rt_count = 0;
    struct wined3d_blitter *next;
    uint32_t next_flags = 0;
    unsigned int i;

    TRACE("blitter %p, device %p, rt_count %u, fb %p, rect_count %u, clear_rects %p, "
            "draw_rect %s, flags %#x, colour %s, depth %.8e, stencil %#x.\n",
            blitter, device, rt_count, fb, rect_count, clear_rects,
            wine_dbgstr_rect(draw_rect), flags, debug_color(colour), depth, stencil);

    if (!rect_count)
    {
        rect_count = 1;
        clear_rects = draw_rect;
    }

    if (flags & WINED3DCLEAR_TARGET)
    {
        for (i = 0; i < rt_count; ++i)
        {
            if (!(view = fb->render_targets[i]))
                continue;

            if (blitter_use_cpu_clear(view))
            {
                next_flags |= WINED3DCLEAR_TARGET;
                flags &= ~WINED3DCLEAR_TARGET;
                next_rt_count = rt_count;
                rt_count = 0;
                break;
            }
        }
    }

    if ((flags & (WINED3DCLEAR_ZBUFFER | WINED3DCLEAR_STENCIL)) && (view = fb->depth_stencil)
            && (!view->format->depth_size || (flags & WINED3DCLEAR_ZBUFFER))
            && (!view->format->stencil_size || (flags & WINED3DCLEAR_STENCIL))
            && blitter_use_cpu_clear(view))
    {
        next_flags |= flags & (WINED3DCLEAR_ZBUFFER | WINED3DCLEAR_STENCIL);
        flags &= ~(WINED3DCLEAR_ZBUFFER | WINED3DCLEAR_STENCIL);
    }

    if (flags)
    {
        context_vk = wined3d_context_vk(context_acquire(&device_vk->d, NULL, 0));

        for (i = 0; i < rt_count; ++i)
        {
            if (!(view = fb->render_targets[i]))
                continue;

            if (previous && (previous->width != view->width || previous->height != view->height))
                have_identical_size = false;
            previous = view;
        }
        if (flags & (WINED3DCLEAR_ZBUFFER | WINED3DCLEAR_STENCIL))
        {
            view = fb->depth_stencil;

            if (previous && (previous->width != view->width || previous->height != view->height))
                have_identical_size = false;
        }

        if (have_identical_size)
        {
            vk_blitter_clear_rendertargets(context_vk, rt_count, fb, rect_count,
                    clear_rects, draw_rect, flags, colour, depth, stencil);
        }
        else
        {
            for (i = 0; i < rt_count; ++i)
            {
                if (!(view = fb->render_targets[i]))
                    continue;

                tmp_fb.render_targets[0] = view;
                tmp_fb.depth_stencil = NULL;
                vk_blitter_clear_rendertargets(context_vk, 1, &tmp_fb, rect_count,
                        clear_rects, draw_rect, WINED3DCLEAR_TARGET, colour, depth, stencil);
            }
            if (flags & (WINED3DCLEAR_ZBUFFER | WINED3DCLEAR_STENCIL))
            {
                tmp_fb.render_targets[0] = NULL;
                tmp_fb.depth_stencil = fb->depth_stencil;
                vk_blitter_clear_rendertargets(context_vk, 0, &tmp_fb, rect_count,
                        clear_rects, draw_rect, flags & ~WINED3DCLEAR_TARGET, colour, depth, stencil);
            }
        }

        context_release(&context_vk->c);
    }

    if (!next_flags)
        return;

    if (!(next = blitter->next))
    {
        ERR("No blitter to handle clear.\n");
        return;
    }

    TRACE("Forwarding to blitter %p.\n", next);
    next->ops->blitter_clear(next, device, next_rt_count, fb, rect_count,
            clear_rects, draw_rect, next_flags, colour, depth, stencil);
}

static bool vk_blitter_blit_supported(enum wined3d_blit_op op, const struct wined3d_context *context,
        const struct wined3d_resource *src_resource, const RECT *src_rect,
        const struct wined3d_resource *dst_resource, const RECT *dst_rect)
{
    const struct wined3d_format *src_format = src_resource->format;
    const struct wined3d_format *dst_format = dst_resource->format;

    if (!(dst_resource->access & WINED3D_RESOURCE_ACCESS_GPU))
    {
        TRACE("Destination resource does not have GPU access.\n");
        return false;
    }

    if (!(src_resource->access & WINED3D_RESOURCE_ACCESS_GPU))
    {
        TRACE("Source resource does not have GPU access.\n");
        return false;
    }

    if (dst_format->id != src_format->id)
    {
        if (!is_identity_fixup(dst_format->color_fixup))
        {
            TRACE("Destination fixups are not supported.\n");
            return false;
        }

        if (!is_identity_fixup(src_format->color_fixup))
        {
            TRACE("Source fixups are not supported.\n");
            return false;
        }

        if (op != WINED3D_BLIT_OP_RAW_BLIT
                && wined3d_format_vk(src_format)->vk_format != wined3d_format_vk(dst_format)->vk_format)
        {
            TRACE("Format conversion not supported.\n");
            return false;
        }
    }

    if (wined3d_resource_get_sample_count(dst_resource) > 1)
    {
        TRACE("Multi-sample destination resource not supported.\n");
        return false;
    }

    if (op == WINED3D_BLIT_OP_RAW_BLIT)
        return true;

    if (op != WINED3D_BLIT_OP_COLOR_BLIT)
    {
        TRACE("Unsupported blit operation %#x.\n", op);
        return false;
    }

    if ((src_rect->right - src_rect->left != dst_rect->right - dst_rect->left)
            || (src_rect->bottom - src_rect->top != dst_rect->bottom - dst_rect->top))
    {
        TRACE("Scaling not supported.\n");
        return false;
    }

    return true;
}

static DWORD vk_blitter_blit(struct wined3d_blitter *blitter, enum wined3d_blit_op op,
        struct wined3d_context *context, struct wined3d_texture *src_texture, unsigned int src_sub_resource_idx,
        DWORD src_location, const RECT *src_rect, struct wined3d_texture *dst_texture,
        unsigned int dst_sub_resource_idx, DWORD dst_location, const RECT *dst_rect,
        const struct wined3d_color_key *colour_key, enum wined3d_texture_filter_type filter)
{
    struct wined3d_texture_vk *src_texture_vk = wined3d_texture_vk(src_texture);
    struct wined3d_texture_vk *dst_texture_vk = wined3d_texture_vk(dst_texture);
    struct wined3d_context_vk *context_vk = wined3d_context_vk(context);
    const struct wined3d_vk_info *vk_info = context_vk->vk_info;
    unsigned int src_level, src_layer, dst_level, dst_layer;
    VkImageAspectFlags src_aspect, dst_aspect;
    VkCommandBuffer vk_command_buffer;
    struct wined3d_blitter *next;
    bool resolve = false;

    TRACE("blitter %p, op %#x, context %p, src_texture %p, src_sub_resource_idx %u, src_location %s, src_rect %s, "
            "dst_texture %p, dst_sub_resource_idx %u, dst_location %s, dst_rect %s, colour_key %p, filter %s.\n",
            blitter, op, context, src_texture, src_sub_resource_idx, wined3d_debug_location(src_location),
            wine_dbgstr_rect(src_rect), dst_texture, dst_sub_resource_idx, wined3d_debug_location(dst_location),
            wine_dbgstr_rect(dst_rect), colour_key, debug_d3dtexturefiltertype(filter));

    if (!vk_blitter_blit_supported(op, context, &src_texture->resource, src_rect, &dst_texture->resource, dst_rect))
        goto next;

    src_aspect = vk_aspect_mask_from_format(src_texture_vk->t.resource.format);
    dst_aspect = vk_aspect_mask_from_format(dst_texture_vk->t.resource.format);
    if ((src_aspect | dst_aspect) & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))
    {
        TRACE("Depth/stencil blits not supported.\n");
        goto next;
    }

    if (wined3d_resource_get_sample_count(&src_texture_vk->t.resource) > 1)
        resolve = true;

    src_level = src_sub_resource_idx % src_texture->level_count;
    src_layer = src_sub_resource_idx / src_texture->level_count;

    dst_level = dst_sub_resource_idx % dst_texture->level_count;
    dst_layer = dst_sub_resource_idx / dst_texture->level_count;

    if (!wined3d_texture_load_location(src_texture, src_sub_resource_idx, context, WINED3D_LOCATION_TEXTURE_RGB))
        ERR("Failed to load the source sub-resource.\n");

    if (wined3d_texture_is_full_rect(dst_texture, dst_level, dst_rect))
    {
        if (!wined3d_texture_prepare_location(dst_texture,
                dst_sub_resource_idx, context, WINED3D_LOCATION_TEXTURE_RGB))
        {
            ERR("Failed to prepare the destination sub-resource.\n");
            goto next;
        }
    }
    else
    {
        if (!wined3d_texture_load_location(dst_texture,
                dst_sub_resource_idx, context, WINED3D_LOCATION_TEXTURE_RGB))
        {
            ERR("Failed to load the destination sub-resource.\n");
            goto next;
        }
    }

    if (!(vk_command_buffer = wined3d_context_vk_get_command_buffer(context_vk)))
    {
        ERR("Failed to get command buffer.\n");
        goto next;
    }

    wined3d_context_vk_image_barrier(context_vk, vk_command_buffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            vk_access_mask_from_bind_flags(src_texture_vk->t.resource.bind_flags),
            VK_ACCESS_TRANSFER_READ_BIT,
            src_texture_vk->layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            src_texture_vk->vk_image, src_aspect);
    wined3d_context_vk_image_barrier(context_vk, vk_command_buffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            vk_access_mask_from_bind_flags(dst_texture_vk->t.resource.bind_flags),
            VK_ACCESS_TRANSFER_WRITE_BIT,
            dst_texture_vk->layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            dst_texture_vk->vk_image, dst_aspect);

    if (resolve)
    {
        VkImageResolve region;

        region.srcSubresource.aspectMask = src_aspect;
        region.srcSubresource.mipLevel = src_level;
        region.srcSubresource.baseArrayLayer = src_layer;
        region.srcSubresource.layerCount = 1;
        region.srcOffset.x = src_rect->left;
        region.srcOffset.y = src_rect->top;
        region.srcOffset.z = 0;
        region.dstSubresource.aspectMask = dst_aspect;
        region.dstSubresource.mipLevel = dst_level;
        region.dstSubresource.baseArrayLayer = dst_layer;
        region.dstSubresource.layerCount = 1;
        region.dstOffset.x = dst_rect->left;
        region.dstOffset.y = dst_rect->top;
        region.dstOffset.z = 0;
        region.extent.width = src_rect->right - src_rect->left;
        region.extent.height = src_rect->bottom - src_rect->top;
        region.extent.depth = 1;

        VK_CALL(vkCmdResolveImage(vk_command_buffer, src_texture_vk->vk_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                dst_texture_vk->vk_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region));
    }
    else
    {
        VkImageCopy region;

        region.srcSubresource.aspectMask = src_aspect;
        region.srcSubresource.mipLevel = src_level;
        region.srcSubresource.baseArrayLayer = src_layer;
        region.srcSubresource.layerCount = 1;
        region.srcOffset.x = src_rect->left;
        region.srcOffset.y = src_rect->top;
        region.srcOffset.z = 0;
        region.dstSubresource.aspectMask = dst_aspect;
        region.dstSubresource.mipLevel = dst_level;
        region.dstSubresource.baseArrayLayer = dst_layer;
        region.dstSubresource.layerCount = 1;
        region.dstOffset.x = dst_rect->left;
        region.dstOffset.y = dst_rect->top;
        region.dstOffset.z = 0;
        region.extent.width = src_rect->right - src_rect->left;
        region.extent.height = src_rect->bottom - src_rect->top;
        region.extent.depth = 1;

        VK_CALL(vkCmdCopyImage(vk_command_buffer, src_texture_vk->vk_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                dst_texture_vk->vk_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region));
    }

    wined3d_context_vk_image_barrier(context_vk, vk_command_buffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            vk_access_mask_from_bind_flags(dst_texture_vk->t.resource.bind_flags),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,  dst_texture_vk->layout,
            dst_texture_vk->vk_image, dst_aspect);
    wined3d_context_vk_image_barrier(context_vk, vk_command_buffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VK_ACCESS_TRANSFER_READ_BIT,
            vk_access_mask_from_bind_flags(src_texture_vk->t.resource.bind_flags),
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,  src_texture_vk->layout,
            src_texture_vk->vk_image, src_aspect);

    wined3d_texture_validate_location(dst_texture, dst_sub_resource_idx, WINED3D_LOCATION_TEXTURE_RGB);
    wined3d_texture_invalidate_location(dst_texture, dst_sub_resource_idx, ~WINED3D_LOCATION_TEXTURE_RGB);
    if (!wined3d_texture_load_location(dst_texture, dst_sub_resource_idx, context, dst_location))
        ERR("Failed to load the destination sub-resource into %s.\n", wined3d_debug_location(dst_location));

    wined3d_context_vk_reference_texture(context_vk, src_texture_vk);
    wined3d_context_vk_reference_texture(context_vk, dst_texture_vk);

    return dst_location | WINED3D_LOCATION_TEXTURE_RGB;

next:
    if (!(next = blitter->next))
    {
        ERR("No blitter to handle blit op %#x.\n", op);
        return dst_location;
    }

    TRACE("Forwarding to blitter %p.\n", next);
    return next->ops->blitter_blit(next, op, context, src_texture, src_sub_resource_idx, src_location,
            src_rect, dst_texture, dst_sub_resource_idx, dst_location, dst_rect, colour_key, filter);
}

static const struct wined3d_blitter_ops vk_blitter_ops =
{
    .blitter_destroy = vk_blitter_destroy,
    .blitter_clear = vk_blitter_clear,
    .blitter_blit = vk_blitter_blit,
};

void wined3d_vk_blitter_create(struct wined3d_blitter **next)
{
    struct wined3d_blitter *blitter;

    if (!(blitter = heap_alloc(sizeof(*blitter))))
        return;

    TRACE("Created blitter %p.\n", blitter);

    blitter->ops = &vk_blitter_ops;
    blitter->next = *next;
    *next = blitter;
}
