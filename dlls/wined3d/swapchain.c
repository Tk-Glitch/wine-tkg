/*
 * Copyright 2002-2003 Jason Edmeades
 * Copyright 2002-2003 Raphael Junqueira
 * Copyright 2005 Oliver Stieber
 * Copyright 2007-2008 Stefan Dösinger for CodeWeavers
 * Copyright 2011 Henri Verbeet for CodeWeavers
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

void wined3d_swapchain_cleanup(struct wined3d_swapchain *swapchain)
{
    HRESULT hr;
    UINT i;

    TRACE("Destroying swapchain %p.\n", swapchain);

    wined3d_swapchain_state_cleanup(&swapchain->state);
    wined3d_swapchain_set_gamma_ramp(swapchain, 0, &swapchain->orig_gamma);

    /* Release the swapchain's draw buffers. Make sure swapchain->back_buffers[0]
     * is the last buffer to be destroyed, FindContext() depends on that. */
    if (swapchain->front_buffer)
    {
        wined3d_texture_set_swapchain(swapchain->front_buffer, NULL);
        if (wined3d_texture_decref(swapchain->front_buffer))
            WARN("Something's still holding the front buffer (%p).\n", swapchain->front_buffer);
        swapchain->front_buffer = NULL;
    }

    if (swapchain->back_buffers)
    {
        i = swapchain->state.desc.backbuffer_count;

        while (i--)
        {
            wined3d_texture_set_swapchain(swapchain->back_buffers[i], NULL);
            if (wined3d_texture_decref(swapchain->back_buffers[i]))
                WARN("Something's still holding back buffer %u (%p).\n", i, swapchain->back_buffers[i]);
        }
        heap_free(swapchain->back_buffers);
        swapchain->back_buffers = NULL;
    }

    /* Restore the screen resolution if we rendered in fullscreen.
     * This will restore the screen resolution to what it was before creating
     * the swapchain. In case of d3d8 and d3d9 this will be the original
     * desktop resolution. In case of d3d7 this will be a NOP because ddraw
     * sets the resolution before starting up Direct3D, thus orig_width and
     * orig_height will be equal to the modes in the presentation params. */
    if (!swapchain->state.desc.windowed)
    {
        if (swapchain->state.desc.auto_restore_display_mode)
        {
            if (FAILED(hr = wined3d_restore_display_modes(swapchain->device->wined3d)))
                ERR("Failed to restore display mode, hr %#x.\n", hr);

            if (swapchain->state.desc.flags & WINED3D_SWAPCHAIN_RESTORE_WINDOW_RECT)
            {
                wined3d_swapchain_state_restore_from_fullscreen(&swapchain->state,
                        swapchain->state.device_window, &swapchain->state.original_window_rect);
                wined3d_device_release_focus_window(swapchain->device);
            }
        }
        else
        {
            wined3d_swapchain_state_restore_from_fullscreen(&swapchain->state, swapchain->state.device_window, NULL);
        }
    }
}

static void wined3d_swapchain_gl_destroy_object(void *object)
{
    wined3d_swapchain_gl_destroy_contexts(object);
}

void wined3d_swapchain_gl_cleanup(struct wined3d_swapchain_gl *swapchain_gl)
{
    struct wined3d_cs *cs = swapchain_gl->s.device->cs;

    wined3d_swapchain_cleanup(&swapchain_gl->s);

    wined3d_cs_destroy_object(cs, wined3d_swapchain_gl_destroy_object, swapchain_gl);
    wined3d_cs_finish(cs, WINED3D_CS_QUEUE_DEFAULT);

    if (swapchain_gl->backup_dc)
    {
        TRACE("Destroying backup wined3d window %p, dc %p.\n", swapchain_gl->backup_wnd, swapchain_gl->backup_dc);

        wined3d_release_dc(swapchain_gl->backup_wnd, swapchain_gl->backup_dc);
        DestroyWindow(swapchain_gl->backup_wnd);
    }
}

static void wined3d_swapchain_vk_destroy_vulkan_swapchain(struct wined3d_swapchain_vk *swapchain_vk)
{
    struct wined3d_device_vk *device_vk = wined3d_device_vk(swapchain_vk->s.device);
    const struct wined3d_vk_info *vk_info;
    unsigned int i;
    VkResult vr;

    TRACE("swapchain_vk %p.\n", swapchain_vk);

    vk_info = &wined3d_adapter_vk(device_vk->d.adapter)->vk_info;

    if ((vr = VK_CALL(vkQueueWaitIdle(device_vk->vk_queue))) < 0)
        ERR("Failed to wait on queue, vr %s.\n", wined3d_debug_vkresult(vr));
    heap_free(swapchain_vk->vk_images);
    for (i = 0; i < swapchain_vk->image_count; ++i)
    {
        VK_CALL(vkDestroySemaphore(device_vk->vk_device, swapchain_vk->vk_semaphores[i].available, NULL));
        VK_CALL(vkDestroySemaphore(device_vk->vk_device, swapchain_vk->vk_semaphores[i].presentable, NULL));
    }
    heap_free(swapchain_vk->vk_semaphores);
    VK_CALL(vkDestroySwapchainKHR(device_vk->vk_device, swapchain_vk->vk_swapchain, NULL));
    VK_CALL(vkDestroySurfaceKHR(vk_info->instance, swapchain_vk->vk_surface, NULL));
}

static void wined3d_swapchain_vk_destroy_object(void *object)
{
    wined3d_swapchain_vk_destroy_vulkan_swapchain(object);
}

void wined3d_swapchain_vk_cleanup(struct wined3d_swapchain_vk *swapchain_vk)
{
    struct wined3d_cs *cs = swapchain_vk->s.device->cs;

    wined3d_cs_destroy_object(cs, wined3d_swapchain_vk_destroy_object, swapchain_vk);
    wined3d_cs_finish(cs, WINED3D_CS_QUEUE_DEFAULT);

    wined3d_swapchain_cleanup(&swapchain_vk->s);
}

ULONG CDECL wined3d_swapchain_incref(struct wined3d_swapchain *swapchain)
{
    ULONG refcount = InterlockedIncrement(&swapchain->ref);

    TRACE("%p increasing refcount to %u.\n", swapchain, refcount);

    return refcount;
}

ULONG CDECL wined3d_swapchain_decref(struct wined3d_swapchain *swapchain)
{
    ULONG refcount = InterlockedDecrement(&swapchain->ref);

    TRACE("%p decreasing refcount to %u.\n", swapchain, refcount);

    if (!refcount)
    {
        struct wined3d_device *device;

        wined3d_mutex_lock();

        device = swapchain->device;
        if (device->swapchain_count && device->swapchains[0] == swapchain)
            wined3d_device_uninit_3d(device);
        wined3d_cs_finish(device->cs, WINED3D_CS_QUEUE_DEFAULT);

        swapchain->parent_ops->wined3d_object_destroyed(swapchain->parent);
        swapchain->device->adapter->adapter_ops->adapter_destroy_swapchain(swapchain);

        wined3d_mutex_unlock();
    }

    return refcount;
}

void * CDECL wined3d_swapchain_get_parent(const struct wined3d_swapchain *swapchain)
{
    TRACE("swapchain %p.\n", swapchain);

    return swapchain->parent;
}

void CDECL wined3d_swapchain_set_window(struct wined3d_swapchain *swapchain, HWND window)
{
    if (!window)
        window = swapchain->state.device_window;
    if (window == swapchain->win_handle)
        return;

    TRACE("Setting swapchain %p window from %p to %p.\n",
            swapchain, swapchain->win_handle, window);

    wined3d_cs_finish(swapchain->device->cs, WINED3D_CS_QUEUE_DEFAULT);

    swapchain->win_handle = window;
}

HRESULT CDECL wined3d_swapchain_present(struct wined3d_swapchain *swapchain,
        const RECT *src_rect, const RECT *dst_rect, HWND dst_window_override,
        unsigned int swap_interval, DWORD flags)
{
    RECT s, d;

    TRACE("swapchain %p, src_rect %s, dst_rect %s, dst_window_override %p, swap_interval %u, flags %#x.\n",
            swapchain, wine_dbgstr_rect(src_rect), wine_dbgstr_rect(dst_rect),
            dst_window_override, swap_interval, flags);

    if (flags)
        FIXME("Ignoring flags %#x.\n", flags);

    wined3d_mutex_lock();

    if (!swapchain->back_buffers)
    {
        WARN("Swapchain doesn't have a backbuffer, returning WINED3DERR_INVALIDCALL.\n");
        wined3d_mutex_unlock();
        return WINED3DERR_INVALIDCALL;
    }

    if (!src_rect)
    {
        SetRect(&s, 0, 0, swapchain->state.desc.backbuffer_width,
                swapchain->state.desc.backbuffer_height);
        src_rect = &s;
    }

    if (!dst_rect)
    {
        GetClientRect(swapchain->win_handle, &d);
        dst_rect = &d;
    }

    wined3d_cs_emit_present(swapchain->device->cs, swapchain, src_rect,
            dst_rect, dst_window_override, swap_interval, flags);

    wined3d_mutex_unlock();

    return WINED3D_OK;
}

HRESULT CDECL wined3d_swapchain_get_front_buffer_data(const struct wined3d_swapchain *swapchain,
        struct wined3d_texture *dst_texture, unsigned int sub_resource_idx)
{
    RECT src_rect, dst_rect;

    TRACE("swapchain %p, dst_texture %p, sub_resource_idx %u.\n", swapchain, dst_texture, sub_resource_idx);

    SetRect(&src_rect, 0, 0, swapchain->front_buffer->resource.width, swapchain->front_buffer->resource.height);
    dst_rect = src_rect;

    if (swapchain->state.desc.windowed)
    {
        MapWindowPoints(swapchain->win_handle, NULL, (POINT *)&dst_rect, 2);
        FIXME("Using destination rect %s in windowed mode, this is likely wrong.\n",
                wine_dbgstr_rect(&dst_rect));
    }

    return wined3d_texture_blt(dst_texture, sub_resource_idx, &dst_rect,
            swapchain->front_buffer, 0, &src_rect, 0, NULL, WINED3D_TEXF_POINT);
}

struct wined3d_texture * CDECL wined3d_swapchain_get_back_buffer(const struct wined3d_swapchain *swapchain,
        UINT back_buffer_idx)
{
    TRACE("swapchain %p, back_buffer_idx %u.\n",
            swapchain, back_buffer_idx);

    /* Return invalid if there is no backbuffer array, otherwise it will
     * crash when ddraw is used (there swapchain->back_buffers is always
     * NULL). We need this because this function is called from
     * stateblock_init_default_state() to get the default scissorrect
     * dimensions. */
    if (!swapchain->back_buffers || back_buffer_idx >= swapchain->state.desc.backbuffer_count)
    {
        WARN("Invalid back buffer index.\n");
        /* Native d3d9 doesn't set NULL here, just as wine's d3d9. But set it
         * here in wined3d to avoid problems in other libs. */
        return NULL;
    }

    TRACE("Returning back buffer %p.\n", swapchain->back_buffers[back_buffer_idx]);

    return swapchain->back_buffers[back_buffer_idx];
}

struct wined3d_output * wined3d_swapchain_get_output(const struct wined3d_swapchain *swapchain)
{
    TRACE("swapchain %p.\n", swapchain);

    return swapchain->state.desc.output;
}

HRESULT CDECL wined3d_swapchain_get_raster_status(const struct wined3d_swapchain *swapchain,
        struct wined3d_raster_status *raster_status)
{
    struct wined3d_output *output;

    TRACE("swapchain %p, raster_status %p.\n", swapchain, raster_status);

    output = wined3d_swapchain_get_output(swapchain);
    if (!output)
    {
        ERR("Failed to get output from swapchain %p.\n", swapchain);
        return E_FAIL;
    }

    return wined3d_output_get_raster_status(output, raster_status);
}

struct wined3d_swapchain_state * CDECL wined3d_swapchain_get_state(struct wined3d_swapchain *swapchain)
{
    return &swapchain->state;
}

HRESULT CDECL wined3d_swapchain_get_display_mode(const struct wined3d_swapchain *swapchain,
        struct wined3d_display_mode *mode, enum wined3d_display_rotation *rotation)
{
    struct wined3d_output *output;
    HRESULT hr;

    TRACE("swapchain %p, mode %p, rotation %p.\n", swapchain, mode, rotation);

    if (!(output = wined3d_swapchain_get_output(swapchain)))
    {
        ERR("Failed to get output from swapchain %p.\n", swapchain);
        return E_FAIL;
    }

    hr = wined3d_output_get_display_mode(output, mode, rotation);

    TRACE("Returning w %u, h %u, refresh rate %u, format %s.\n",
            mode->width, mode->height, mode->refresh_rate, debug_d3dformat(mode->format_id));

    return hr;
}

struct wined3d_device * CDECL wined3d_swapchain_get_device(const struct wined3d_swapchain *swapchain)
{
    TRACE("swapchain %p.\n", swapchain);

    return swapchain->device;
}

void CDECL wined3d_swapchain_get_desc(const struct wined3d_swapchain *swapchain,
        struct wined3d_swapchain_desc *desc)
{
    TRACE("swapchain %p, desc %p.\n", swapchain, desc);

    *desc = swapchain->state.desc;
}

HRESULT CDECL wined3d_swapchain_set_gamma_ramp(const struct wined3d_swapchain *swapchain,
        DWORD flags, const struct wined3d_gamma_ramp *ramp)
{
    HDC dc;

    TRACE("swapchain %p, flags %#x, ramp %p.\n", swapchain, flags, ramp);

    if (flags)
        FIXME("Ignoring flags %#x.\n", flags);

    dc = GetDCEx(swapchain->state.device_window, 0, DCX_USESTYLE | DCX_CACHE);
    SetDeviceGammaRamp(dc, (void *)ramp);
    ReleaseDC(swapchain->state.device_window, dc);

    return WINED3D_OK;
}

void CDECL wined3d_swapchain_set_palette(struct wined3d_swapchain *swapchain, struct wined3d_palette *palette)
{
    TRACE("swapchain %p, palette %p.\n", swapchain, palette);

    wined3d_cs_finish(swapchain->device->cs, WINED3D_CS_QUEUE_DEFAULT);

    swapchain->palette = palette;
}

HRESULT CDECL wined3d_swapchain_get_gamma_ramp(const struct wined3d_swapchain *swapchain,
        struct wined3d_gamma_ramp *ramp)
{
    HDC dc;

    TRACE("swapchain %p, ramp %p.\n", swapchain, ramp);

    dc = GetDCEx(swapchain->state.device_window, 0, DCX_USESTYLE | DCX_CACHE);
    GetDeviceGammaRamp(dc, ramp);
    ReleaseDC(swapchain->state.device_window, dc);

    return WINED3D_OK;
}

/* A GL context is provided by the caller */
static void swapchain_blit(const struct wined3d_swapchain *swapchain,
        struct wined3d_context *context, const RECT *src_rect, const RECT *dst_rect)
{
    struct wined3d_texture *texture = swapchain->back_buffers[0];
    struct wined3d_device *device = swapchain->device;
    enum wined3d_texture_filter_type filter;
    DWORD location;

    TRACE("swapchain %p, context %p, src_rect %s, dst_rect %s.\n",
            swapchain, context, wine_dbgstr_rect(src_rect), wine_dbgstr_rect(dst_rect));

    if ((src_rect->right - src_rect->left == dst_rect->right - dst_rect->left
            && src_rect->bottom - src_rect->top == dst_rect->bottom - dst_rect->top)
            || is_complex_fixup(texture->resource.format->color_fixup))
        filter = WINED3D_TEXF_NONE;
    else
        filter = WINED3D_TEXF_LINEAR;

    location = WINED3D_LOCATION_TEXTURE_RGB;
    if (texture->resource.multisample_type)
        location = WINED3D_LOCATION_RB_RESOLVED;

    wined3d_texture_validate_location(texture, 0, WINED3D_LOCATION_DRAWABLE);
    device->blitter->ops->blitter_blit(device->blitter, WINED3D_BLIT_OP_COLOR_BLIT, context, texture, 0,
            location, src_rect, texture, 0, WINED3D_LOCATION_DRAWABLE, dst_rect, NULL, filter);
    wined3d_texture_invalidate_location(texture, 0, WINED3D_LOCATION_DRAWABLE);
}

static void swapchain_gl_set_swap_interval(struct wined3d_swapchain *swapchain,
        struct wined3d_context_gl *context_gl, unsigned int swap_interval)
{
    const struct wined3d_gl_info *gl_info = context_gl->gl_info;

    swap_interval = swap_interval <= 4 ? swap_interval : 1;
    if (swapchain->swap_interval == swap_interval)
        return;

    swapchain->swap_interval = swap_interval;

    if (!gl_info->supported[WGL_EXT_SWAP_CONTROL])
        return;

    if (!GL_EXTCALL(wglSwapIntervalEXT(swap_interval)))
    {
        ERR("Failed to set swap interval %u for context %p, last error %#x.\n",
                swap_interval, context_gl, GetLastError());
    }
}

/* Context activation is done by the caller. */
static void wined3d_swapchain_gl_rotate(struct wined3d_swapchain *swapchain, struct wined3d_context *context)
{
    struct wined3d_texture_sub_resource *sub_resource;
    struct wined3d_texture_gl *texture, *texture_prev;
    struct gl_texture tex0;
    GLuint rb0;
    DWORD locations0;
    unsigned int i;
    static const DWORD supported_locations = WINED3D_LOCATION_TEXTURE_RGB | WINED3D_LOCATION_RB_MULTISAMPLE;

    if (swapchain->state.desc.backbuffer_count < 2 || !swapchain->render_to_fbo)
        return;

    texture_prev = wined3d_texture_gl(swapchain->back_buffers[0]);

    /* Back buffer 0 is already in the draw binding. */
    tex0 = texture_prev->texture_rgb;
    rb0 = texture_prev->rb_multisample;
    locations0 = texture_prev->t.sub_resources[0].locations;

    for (i = 1; i < swapchain->state.desc.backbuffer_count; ++i)
    {
        texture = wined3d_texture_gl(swapchain->back_buffers[i]);
        sub_resource = &texture->t.sub_resources[0];

        if (!(sub_resource->locations & supported_locations))
            wined3d_texture_load_location(&texture->t, 0, context, texture->t.resource.draw_binding);

        texture_prev->texture_rgb = texture->texture_rgb;
        texture_prev->rb_multisample = texture->rb_multisample;

        wined3d_texture_validate_location(&texture_prev->t, 0, sub_resource->locations & supported_locations);
        wined3d_texture_invalidate_location(&texture_prev->t, 0, ~(sub_resource->locations & supported_locations));

        texture_prev = texture;
    }

    texture_prev->texture_rgb = tex0;
    texture_prev->rb_multisample = rb0;

    wined3d_texture_validate_location(&texture_prev->t, 0, locations0 & supported_locations);
    wined3d_texture_invalidate_location(&texture_prev->t, 0, ~(locations0 & supported_locations));

    device_invalidate_state(swapchain->device, STATE_FRAMEBUFFER);
}

static void swapchain_gl_present(struct wined3d_swapchain *swapchain,
        const RECT *src_rect, const RECT *dst_rect, unsigned int swap_interval, DWORD flags)
{
    struct wined3d_swapchain_gl *swapchain_gl = wined3d_swapchain_gl(swapchain);
    const struct wined3d_swapchain_desc *desc = &swapchain->state.desc;
    struct wined3d_texture *back_buffer = swapchain->back_buffers[0];
    const struct wined3d_gl_info *gl_info;
    struct wined3d_context_gl *context_gl;
    struct wined3d_context *context;
    BOOL render_to_fbo;

    context = context_acquire(swapchain->device, swapchain->front_buffer, 0);
    context_gl = wined3d_context_gl(context);
    if (!context_gl->valid)
    {
        context_release(context);
        WARN("Invalid context, skipping present.\n");
        return;
    }

    gl_info = context_gl->gl_info;

    swapchain_gl_set_swap_interval(swapchain, context_gl, swap_interval);

    TRACE("Presenting DC %p.\n", context_gl->dc);

    if (!(render_to_fbo = swapchain->render_to_fbo)
            && (src_rect->left || src_rect->top
            || src_rect->right != desc->backbuffer_width
            || src_rect->bottom != desc->backbuffer_height
            || dst_rect->left || dst_rect->top
            || dst_rect->right != desc->backbuffer_width
            || dst_rect->bottom != desc->backbuffer_height))
        render_to_fbo = TRUE;

    /* Rendering to a window of different size, presenting partial rectangles,
     * or rendering to a different window needs help from FBO_blit or a textured
     * draw. Render the swapchain to a FBO in the future.
     *
     * Note that FBO_blit from the backbuffer to the frontbuffer cannot solve
     * all these issues - this fails if the window is smaller than the backbuffer.
     */
    if (!swapchain->render_to_fbo && render_to_fbo && wined3d_settings.offscreen_rendering_mode == ORM_FBO)
    {
        wined3d_texture_load_location(back_buffer, 0, context, WINED3D_LOCATION_TEXTURE_RGB);
        wined3d_texture_invalidate_location(back_buffer, 0, WINED3D_LOCATION_DRAWABLE);
        swapchain->render_to_fbo = TRUE;
        swapchain_update_draw_bindings(swapchain);
    }
    else
    {
        wined3d_texture_load_location(back_buffer, 0, context, back_buffer->resource.draw_binding);
    }

    if (swapchain->render_to_fbo)
        swapchain_blit(swapchain, context, src_rect, dst_rect);

    if (swapchain_gl->context_count > 1)
        gl_info->gl_ops.gl.p_glFinish();

    /* call wglSwapBuffers through the gl table to avoid confusing the Steam overlay */
    gl_info->gl_ops.wgl.p_wglSwapBuffers(context_gl->dc);
    wined3d_context_gl_submit_command_fence(context_gl);

    wined3d_swapchain_gl_rotate(swapchain, context);

    TRACE("SwapBuffers called, Starting new frame\n");

    wined3d_texture_validate_location(swapchain->front_buffer, 0, WINED3D_LOCATION_DRAWABLE);
    wined3d_texture_invalidate_location(swapchain->front_buffer, 0, ~WINED3D_LOCATION_DRAWABLE);

    context_release(context);
}

static void swapchain_frontbuffer_updated(struct wined3d_swapchain *swapchain)
{
    struct wined3d_texture *front_buffer = swapchain->front_buffer;
    struct wined3d_context *context;

    context = context_acquire(swapchain->device, front_buffer, 0);
    wined3d_texture_load_location(front_buffer, 0, context, front_buffer->resource.draw_binding);
    context_release(context);
    SetRectEmpty(&swapchain->front_buffer_update);
}

static const struct wined3d_swapchain_ops swapchain_gl_ops =
{
    swapchain_gl_present,
    swapchain_frontbuffer_updated,
};

static bool wined3d_swapchain_vk_present_mode_supported(struct wined3d_swapchain_vk *swapchain_vk,
        VkPresentModeKHR vk_present_mode)
{
    struct wined3d_device_vk *device_vk = wined3d_device_vk(swapchain_vk->s.device);
    const struct wined3d_vk_info *vk_info;
    struct wined3d_adapter_vk *adapter_vk;
    VkPhysicalDevice vk_physical_device;
    VkPresentModeKHR *vk_modes;
    bool supported = false;
    uint32_t count, i;
    VkResult vr;

    adapter_vk = wined3d_adapter_vk(device_vk->d.adapter);
    vk_physical_device = adapter_vk->physical_device;
    vk_info = &adapter_vk->vk_info;

    if ((vr = VK_CALL(vkGetPhysicalDeviceSurfacePresentModesKHR(vk_physical_device,
            swapchain_vk->vk_surface, &count, NULL))) < 0)
    {
        ERR("Failed to get supported present mode count, vr %s.\n", wined3d_debug_vkresult(vr));
        return false;
    }

    if (!(vk_modes = heap_calloc(count, sizeof(*vk_modes))))
        return false;

    if ((vr = VK_CALL(vkGetPhysicalDeviceSurfacePresentModesKHR(vk_physical_device,
            swapchain_vk->vk_surface, &count, vk_modes))) < 0)
    {
        ERR("Failed to get supported present modes, vr %s.\n", wined3d_debug_vkresult(vr));
        goto done;
    }

    for (i = 0; i < count; ++i)
    {
        if (vk_modes[i] == vk_present_mode)
        {
            supported = true;
            goto done;
        }
    }

done:
    heap_free(vk_modes);
    return supported;
}

static VkFormat get_swapchain_fallback_format(VkFormat vk_format)
{
    switch (vk_format)
    {
        case VK_FORMAT_R8G8B8A8_SRGB:
            return VK_FORMAT_B8G8R8A8_SRGB;
        case VK_FORMAT_R8G8B8A8_UNORM:
        case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
        case VK_FORMAT_R16G16B16A16_SFLOAT:
            return VK_FORMAT_B8G8R8A8_UNORM;
        default:
            WARN("Unhandled format %#x.\n", vk_format);
            return VK_FORMAT_UNDEFINED;
    }
}

static VkFormat wined3d_swapchain_vk_select_vk_format(struct wined3d_swapchain_vk *swapchain_vk,
        VkSurfaceKHR vk_surface)
{
    struct wined3d_device_vk *device_vk = wined3d_device_vk(swapchain_vk->s.device);
    const struct wined3d_swapchain_desc *desc = &swapchain_vk->s.state.desc;
    const struct wined3d_vk_info *vk_info;
    struct wined3d_adapter_vk *adapter_vk;
    const struct wined3d_format *format;
    VkPhysicalDevice vk_physical_device;
    VkSurfaceFormatKHR *vk_formats;
    uint32_t format_count, i;
    VkFormat vk_format;
    VkResult vr;

    adapter_vk = wined3d_adapter_vk(device_vk->d.adapter);
    vk_physical_device = adapter_vk->physical_device;
    vk_info = &adapter_vk->vk_info;

    if ((format = wined3d_get_format(&adapter_vk->a, desc->backbuffer_format, WINED3D_BIND_RENDER_TARGET)))
        vk_format = wined3d_format_vk(format)->vk_format;
    else
        vk_format = VK_FORMAT_B8G8R8A8_UNORM;

    vr = VK_CALL(vkGetPhysicalDeviceSurfaceFormatsKHR(vk_physical_device, vk_surface, &format_count, NULL));
    if (vr < 0 || !format_count)
    {
        WARN("Failed to get supported surface format count, vr %s.\n", wined3d_debug_vkresult(vr));
        return VK_FORMAT_UNDEFINED;
    }

    if (!(vk_formats = heap_calloc(format_count, sizeof(*vk_formats))))
        return VK_FORMAT_UNDEFINED;

    if ((vr = VK_CALL(vkGetPhysicalDeviceSurfaceFormatsKHR(vk_physical_device,
            vk_surface, &format_count, vk_formats))) < 0)
    {
        WARN("Failed to get supported surface formats, vr %s.\n", wined3d_debug_vkresult(vr));
        heap_free(vk_formats);
        return VK_FORMAT_UNDEFINED;
    }

    for (i = 0; i < format_count; ++i)
    {
        if (vk_formats[i].format == vk_format && vk_formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            break;
    }
    if (i == format_count)
    {
        /* Try to create a swapchain with format conversion. */
        vk_format = get_swapchain_fallback_format(vk_format);
        WARN("Failed to find Vulkan swapchain format for %s.\n", debug_d3dformat(desc->backbuffer_format));
        for (i = 0; i < format_count; ++i)
        {
            if (vk_formats[i].format == vk_format && vk_formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                break;
        }
    }
    heap_free(vk_formats);
    if (i == format_count)
    {
        FIXME("Failed to find Vulkan swapchain format for %s.\n", debug_d3dformat(desc->backbuffer_format));
        return VK_FORMAT_UNDEFINED;
    }

    TRACE("Using Vulkan swapchain format %#x.\n", vk_format);

    return vk_format;
}

static bool wined3d_swapchain_vk_create_vulkan_swapchain_images(struct wined3d_swapchain_vk *swapchain_vk,
        VkSwapchainKHR vk_swapchain)
{
    struct wined3d_device_vk *device_vk = wined3d_device_vk(swapchain_vk->s.device);
    const struct wined3d_vk_info *vk_info;
    VkSemaphoreCreateInfo semaphore_info;
    uint32_t image_count, i;
    VkResult vr;

    vk_info = &wined3d_adapter_vk(device_vk->d.adapter)->vk_info;

    if ((vr = VK_CALL(vkGetSwapchainImagesKHR(device_vk->vk_device, vk_swapchain, &image_count, NULL))) < 0)
    {
        ERR("Failed to get image count, vr %s\n", wined3d_debug_vkresult(vr));
        return false;
    }

    if (!(swapchain_vk->vk_images = heap_calloc(image_count, sizeof(*swapchain_vk->vk_images))))
    {
        ERR("Failed to allocate images array.\n");
        return false;
    }

    if ((vr = VK_CALL(vkGetSwapchainImagesKHR(device_vk->vk_device,
            vk_swapchain, &image_count, swapchain_vk->vk_images))) < 0)
    {
        ERR("Failed to get swapchain images, vr %s.\n", wined3d_debug_vkresult(vr));
        heap_free(swapchain_vk->vk_images);
        return false;
    }

    if (!(swapchain_vk->vk_semaphores = heap_calloc(image_count, sizeof(*swapchain_vk->vk_semaphores))))
    {
        ERR("Failed to allocate semaphores array.\n");
        heap_free(swapchain_vk->vk_images);
        return false;
    }

    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphore_info.pNext = NULL;
    semaphore_info.flags = 0;
    for (i = 0; i < image_count; ++i)
    {
        if ((vr = VK_CALL(vkCreateSemaphore(device_vk->vk_device,
                &semaphore_info, NULL, &swapchain_vk->vk_semaphores[i].available))) < 0)
        {
            ERR("Failed to create semaphore, vr %s.\n", wined3d_debug_vkresult(vr));
            goto fail;
        }

        if ((vr = VK_CALL(vkCreateSemaphore(device_vk->vk_device,
                &semaphore_info, NULL, &swapchain_vk->vk_semaphores[i].presentable))) < 0)
        {
            ERR("Failed to create semaphore, vr %s.\n", wined3d_debug_vkresult(vr));
            goto fail;
        }
    }
    swapchain_vk->image_count = image_count;

    return true;

fail:
    for (i = 0; i < image_count; ++i)
    {
        if (swapchain_vk->vk_semaphores[i].available)
            VK_CALL(vkDestroySemaphore(device_vk->vk_device, swapchain_vk->vk_semaphores[i].available, NULL));
        if (swapchain_vk->vk_semaphores[i].presentable)
            VK_CALL(vkDestroySemaphore(device_vk->vk_device, swapchain_vk->vk_semaphores[i].presentable, NULL));
    }
    heap_free(swapchain_vk->vk_semaphores);
    heap_free(swapchain_vk->vk_images);
    return false;
}

static HRESULT wined3d_swapchain_vk_create_vulkan_swapchain(struct wined3d_swapchain_vk *swapchain_vk)
{
    struct wined3d_device_vk *device_vk = wined3d_device_vk(swapchain_vk->s.device);
    const struct wined3d_swapchain_desc *desc = &swapchain_vk->s.state.desc;
    VkSwapchainCreateInfoKHR vk_swapchain_desc;
    VkWin32SurfaceCreateInfoKHR surface_desc;
    unsigned int width, height, image_count;
    const struct wined3d_vk_info *vk_info;
    VkSurfaceCapabilitiesKHR surface_caps;
    struct wined3d_adapter_vk *adapter_vk;
    VkPresentModeKHR vk_present_mode;
    VkSwapchainKHR vk_swapchain;
    VkImageUsageFlags usage;
    VkSurfaceKHR vk_surface;
    VkBool32 supported;
    VkFormat vk_format;
    VkResult vr;

    adapter_vk = wined3d_adapter_vk(device_vk->d.adapter);
    vk_info = &adapter_vk->vk_info;

    surface_desc.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surface_desc.pNext = NULL;
    surface_desc.flags = 0;
    surface_desc.hinstance = (HINSTANCE)GetWindowLongPtrW(swapchain_vk->s.win_handle, GWLP_HINSTANCE);
    surface_desc.hwnd = swapchain_vk->s.win_handle;
    if ((vr = VK_CALL(vkCreateWin32SurfaceKHR(vk_info->instance, &surface_desc, NULL, &vk_surface))) < 0)
    {
        ERR("Failed to create Vulkan surface, vr %s.\n", wined3d_debug_vkresult(vr));
        return E_FAIL;
    }
    swapchain_vk->vk_surface = vk_surface;

    if ((vr = VK_CALL(vkGetPhysicalDeviceSurfaceSupportKHR(adapter_vk->physical_device,
            device_vk->vk_queue_family_index, vk_surface, &supported))) < 0 || !supported)
    {
        ERR("Queue family does not support presentation on this surface, vr %s.\n", wined3d_debug_vkresult(vr));
        goto fail;
    }

    if ((vk_format = wined3d_swapchain_vk_select_vk_format(swapchain_vk, vk_surface)) == VK_FORMAT_UNDEFINED)
    {
        ERR("Failed to select swapchain format.\n");
        goto fail;
    }

    if ((vr = VK_CALL(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(adapter_vk->physical_device,
            swapchain_vk->vk_surface, &surface_caps))) < 0)
    {
        ERR("Failed to get surface capabilities, vr %s.\n", wined3d_debug_vkresult(vr));
        goto fail;
    }

    image_count = desc->backbuffer_count;
    if (image_count < surface_caps.minImageCount)
        image_count = surface_caps.minImageCount;
    else if (surface_caps.maxImageCount && image_count > surface_caps.maxImageCount)
        image_count = surface_caps.maxImageCount;

    if (image_count != desc->backbuffer_count)
        WARN("Image count %u is not supported (%u-%u).\n", desc->backbuffer_count,
                surface_caps.minImageCount, surface_caps.maxImageCount);

    width = desc->backbuffer_width;
    if (width < surface_caps.minImageExtent.width)
        width = surface_caps.minImageExtent.width;
    else if (width > surface_caps.maxImageExtent.width)
        width = surface_caps.maxImageExtent.width;

    height = desc->backbuffer_height;
    if (height < surface_caps.minImageExtent.height)
        height = surface_caps.minImageExtent.height;
    else if (height > surface_caps.maxImageExtent.height)
        height = surface_caps.maxImageExtent.height;

    if (width != desc->backbuffer_width || height != desc->backbuffer_height)
        WARN("Swapchain dimensions %ux%u are not supported (%u-%u x %u-%u).\n",
                desc->backbuffer_width, desc->backbuffer_height,
                surface_caps.minImageExtent.width, surface_caps.maxImageExtent.width,
                surface_caps.minImageExtent.height, surface_caps.maxImageExtent.height);

    usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    usage |= surface_caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    usage |= surface_caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (!(usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) || !(usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
        WARN("Transfer not supported for swapchain images.\n");

    if (!(surface_caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR))
    {
        FIXME("Unsupported alpha mode, %#x.\n", surface_caps.supportedCompositeAlpha);
        goto fail;
    }

    vk_present_mode = VK_PRESENT_MODE_FIFO_KHR;
    if (!swapchain_vk->s.swap_interval)
    {
        if (wined3d_swapchain_vk_present_mode_supported(swapchain_vk, VK_PRESENT_MODE_IMMEDIATE_KHR))
            vk_present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        else
            FIXME("Unsupported swap interval %u.\n", swapchain_vk->s.swap_interval);
    }

    vk_swapchain_desc.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    vk_swapchain_desc.pNext = NULL;
    vk_swapchain_desc.flags = 0;
    vk_swapchain_desc.surface = vk_surface;
    vk_swapchain_desc.minImageCount = image_count;
    vk_swapchain_desc.imageFormat = vk_format;
    vk_swapchain_desc.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    vk_swapchain_desc.imageExtent.width = width;
    vk_swapchain_desc.imageExtent.height = height;
    vk_swapchain_desc.imageArrayLayers = 1;
    vk_swapchain_desc.imageUsage = usage;
    vk_swapchain_desc.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vk_swapchain_desc.queueFamilyIndexCount = 0;
    vk_swapchain_desc.pQueueFamilyIndices = NULL;
    vk_swapchain_desc.preTransform = surface_caps.currentTransform;
    vk_swapchain_desc.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    vk_swapchain_desc.presentMode = vk_present_mode;
    vk_swapchain_desc.clipped = VK_TRUE;
    vk_swapchain_desc.oldSwapchain = VK_NULL_HANDLE;
    if ((vr = VK_CALL(vkCreateSwapchainKHR(device_vk->vk_device, &vk_swapchain_desc, NULL, &vk_swapchain))) < 0)
    {
        ERR("Failed to create Vulkan swapchain, vr %s.\n", wined3d_debug_vkresult(vr));
        goto fail;
    }
    swapchain_vk->vk_swapchain = vk_swapchain;

    if (!wined3d_swapchain_vk_create_vulkan_swapchain_images(swapchain_vk, vk_swapchain))
    {
        VK_CALL(vkDestroySwapchainKHR(device_vk->vk_device, vk_swapchain, NULL));
        goto fail;
    }

    return WINED3D_OK;

fail:
    VK_CALL(vkDestroySurfaceKHR(vk_info->instance, vk_surface, NULL));
    return E_FAIL;
}

static HRESULT wined3d_swapchain_vk_recreate(struct wined3d_swapchain_vk *swapchain_vk)
{
    TRACE("swapchain_vk %p.\n", swapchain_vk);

    wined3d_swapchain_vk_destroy_vulkan_swapchain(swapchain_vk);

    return wined3d_swapchain_vk_create_vulkan_swapchain(swapchain_vk);
}

static void wined3d_swapchain_vk_set_swap_interval(struct wined3d_swapchain_vk *swapchain_vk,
        unsigned int swap_interval)
{
    if (swap_interval > 1)
    {
        if (swap_interval <= 4)
            FIXME("Unsupported swap interval %u.\n", swap_interval);
        swap_interval = 1;
    }

    if (swapchain_vk->s.swap_interval == swap_interval)
        return;

    swapchain_vk->s.swap_interval = swap_interval;
    wined3d_swapchain_vk_recreate(swapchain_vk);
}

static void wined3d_swapchain_vk_blit(struct wined3d_swapchain_vk *swapchain_vk,
        struct wined3d_context_vk *context_vk, const RECT *src_rect, const RECT *dst_rect, unsigned int swap_interval)
{
    struct wined3d_texture_vk *back_buffer_vk = wined3d_texture_vk(swapchain_vk->s.back_buffers[0]);
    struct wined3d_device_vk *device_vk = wined3d_device_vk(swapchain_vk->s.device);
    const struct wined3d_swapchain_desc *desc = &swapchain_vk->s.state.desc;
    const struct wined3d_vk_info *vk_info = context_vk->vk_info;
    VkCommandBuffer vk_command_buffer;
    VkPresentInfoKHR present_desc;
    unsigned int present_idx;
    VkImageLayout vk_layout;
    uint32_t image_idx;
    VkImageBlit blit;
    VkResult vr;
    HRESULT hr;

    static const VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    wined3d_swapchain_vk_set_swap_interval(swapchain_vk, swap_interval);

    present_idx = swapchain_vk->current++ % swapchain_vk->image_count;
    wined3d_context_vk_wait_command_buffer(context_vk, swapchain_vk->vk_semaphores[present_idx].command_buffer_id);
    vr = VK_CALL(vkAcquireNextImageKHR(device_vk->vk_device, swapchain_vk->vk_swapchain, UINT64_MAX,
            swapchain_vk->vk_semaphores[present_idx].available, VK_NULL_HANDLE, &image_idx));
    if (vr == VK_ERROR_OUT_OF_DATE_KHR)
    {
        if (FAILED(hr = wined3d_swapchain_vk_recreate(swapchain_vk)))
        {
            ERR("Failed to recreate swapchain, hr %#x.\n", hr);
            return;
        }
        vr = VK_CALL(vkAcquireNextImageKHR(device_vk->vk_device, swapchain_vk->vk_swapchain, UINT64_MAX,
                swapchain_vk->vk_semaphores[present_idx].available, VK_NULL_HANDLE, &image_idx));
    }
    if (vr < 0)
    {
        ERR("Failed to acquire next Vulkan image, vr %s.\n", wined3d_debug_vkresult(vr));
        return;
    }

    vk_command_buffer = wined3d_context_vk_get_command_buffer(context_vk);

    wined3d_context_vk_end_current_render_pass(context_vk);

    wined3d_context_vk_image_barrier(context_vk, vk_command_buffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            vk_access_mask_from_bind_flags(back_buffer_vk->t.resource.bind_flags),
            VK_ACCESS_TRANSFER_READ_BIT,
            back_buffer_vk->layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            back_buffer_vk->vk_image, VK_IMAGE_ASPECT_COLOR_BIT);

    wined3d_context_vk_image_barrier(context_vk, vk_command_buffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            swapchain_vk->vk_images[image_idx], VK_IMAGE_ASPECT_COLOR_BIT);

    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.mipLevel = 0;
    blit.srcSubresource.baseArrayLayer = 0;
    blit.srcSubresource.layerCount = 1;
    blit.srcOffsets[0].x = src_rect->left;
    blit.srcOffsets[0].y = src_rect->top;
    blit.srcOffsets[0].z = 0;
    blit.srcOffsets[1].x = src_rect->right;
    blit.srcOffsets[1].y = src_rect->bottom;
    blit.srcOffsets[1].z = 1;
    blit.dstSubresource = blit.srcSubresource;
    blit.dstOffsets[0].x = dst_rect->left;
    blit.dstOffsets[0].y = dst_rect->top;
    blit.dstOffsets[0].z = 0;
    blit.dstOffsets[1].x = dst_rect->right;
    blit.dstOffsets[1].y = dst_rect->bottom;
    blit.dstOffsets[1].z = 1;
    VK_CALL(vkCmdBlitImage(vk_command_buffer,
            back_buffer_vk->vk_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            swapchain_vk->vk_images[image_idx], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &blit, VK_FILTER_NEAREST));

    wined3d_context_vk_reference_texture(context_vk, back_buffer_vk);
    wined3d_context_vk_image_barrier(context_vk, vk_command_buffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT, 0,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            swapchain_vk->vk_images[image_idx], VK_IMAGE_ASPECT_COLOR_BIT);

    if (desc->swap_effect == WINED3D_SWAP_EFFECT_DISCARD || desc->swap_effect == WINED3D_SWAP_EFFECT_FLIP_DISCARD)
        vk_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    else
        vk_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    wined3d_context_vk_image_barrier(context_vk, vk_command_buffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VK_ACCESS_TRANSFER_READ_BIT,
            vk_access_mask_from_bind_flags(back_buffer_vk->t.resource.bind_flags),
            vk_layout, back_buffer_vk->layout,
            back_buffer_vk->vk_image, VK_IMAGE_ASPECT_COLOR_BIT);
    back_buffer_vk->bind_mask = 0;

    swapchain_vk->vk_semaphores[present_idx].command_buffer_id = context_vk->current_command_buffer.id;
    wined3d_context_vk_submit_command_buffer(context_vk,
            1, &swapchain_vk->vk_semaphores[present_idx].available, &wait_stage,
            1, &swapchain_vk->vk_semaphores[present_idx].presentable);

    present_desc.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_desc.pNext = NULL;
    present_desc.waitSemaphoreCount = 1;
    present_desc.pWaitSemaphores = &swapchain_vk->vk_semaphores[present_idx].presentable;
    present_desc.swapchainCount = 1;
    present_desc.pSwapchains = &swapchain_vk->vk_swapchain;
    present_desc.pImageIndices = &image_idx;
    present_desc.pResults = NULL;
    if ((vr = VK_CALL(vkQueuePresentKHR(device_vk->vk_queue, &present_desc))))
        ERR("Present returned vr %s.\n", wined3d_debug_vkresult(vr));
}

static void wined3d_swapchain_vk_rotate(struct wined3d_swapchain *swapchain, struct wined3d_context_vk *context_vk)
{
    struct wined3d_texture_sub_resource *sub_resource;
    struct wined3d_texture_vk *texture, *texture_prev;
    struct wined3d_allocator_block *memory0;
    VkDescriptorImageInfo vk_info0;
    VkDeviceMemory vk_memory0;
    VkImageLayout vk_layout0;
    VkImage vk_image0;
    DWORD locations0;
    unsigned int i;
    uint64_t id0;

    static const DWORD supported_locations = WINED3D_LOCATION_TEXTURE_RGB | WINED3D_LOCATION_RB_MULTISAMPLE;

    if (swapchain->state.desc.backbuffer_count < 2)
        return;

    texture_prev = wined3d_texture_vk(swapchain->back_buffers[0]);

    /* Back buffer 0 is already in the draw binding. */
    vk_image0 = texture_prev->vk_image;
    memory0 = texture_prev->memory;
    vk_memory0 = texture_prev->vk_memory;
    vk_layout0 = texture_prev->layout;
    id0 = texture_prev->command_buffer_id;
    vk_info0 = texture_prev->default_image_info;
    locations0 = texture_prev->t.sub_resources[0].locations;

    for (i = 1; i < swapchain->state.desc.backbuffer_count; ++i)
    {
        texture = wined3d_texture_vk(swapchain->back_buffers[i]);
        sub_resource = &texture->t.sub_resources[0];

        if (!(sub_resource->locations & supported_locations))
            wined3d_texture_load_location(&texture->t, 0, &context_vk->c, texture->t.resource.draw_binding);

        texture_prev->vk_image = texture->vk_image;
        texture_prev->memory = texture->memory;
        texture_prev->vk_memory = texture->vk_memory;
        texture_prev->layout = texture->layout;
        texture_prev->command_buffer_id = texture->command_buffer_id;
        texture_prev->default_image_info = texture->default_image_info;

        wined3d_texture_validate_location(&texture_prev->t, 0, sub_resource->locations & supported_locations);
        wined3d_texture_invalidate_location(&texture_prev->t, 0, ~(sub_resource->locations & supported_locations));

        texture_prev = texture;
    }

    texture_prev->vk_image = vk_image0;
    texture_prev->memory = memory0;
    texture_prev->vk_memory = vk_memory0;
    texture_prev->layout = vk_layout0;
    texture_prev->command_buffer_id = id0;
    texture_prev->default_image_info = vk_info0;

    wined3d_texture_validate_location(&texture_prev->t, 0, locations0 & supported_locations);
    wined3d_texture_invalidate_location(&texture_prev->t, 0, ~(locations0 & supported_locations));

    device_invalidate_state(swapchain->device, STATE_FRAMEBUFFER);
}

static void swapchain_vk_present(struct wined3d_swapchain *swapchain, const RECT *src_rect,
        const RECT *dst_rect, unsigned int swap_interval, uint32_t flags)
{
    struct wined3d_swapchain_vk *swapchain_vk = wined3d_swapchain_vk(swapchain);
    struct wined3d_texture *back_buffer = swapchain->back_buffers[0];
    struct wined3d_context_vk *context_vk;

    context_vk = wined3d_context_vk(context_acquire(swapchain->device, back_buffer, 0));

    wined3d_texture_load_location(back_buffer, 0, &context_vk->c, back_buffer->resource.draw_binding);

    if (swapchain_vk->vk_swapchain)
        wined3d_swapchain_vk_blit(swapchain_vk, context_vk, src_rect, dst_rect, swap_interval);

    wined3d_swapchain_vk_rotate(swapchain, context_vk);

    wined3d_texture_validate_location(swapchain->front_buffer, 0, WINED3D_LOCATION_DRAWABLE);
    wined3d_texture_invalidate_location(swapchain->front_buffer, 0, ~WINED3D_LOCATION_DRAWABLE);

    TRACE("Starting new frame.\n");

    context_release(&context_vk->c);
}

static const struct wined3d_swapchain_ops swapchain_vk_ops =
{
    swapchain_vk_present,
    swapchain_frontbuffer_updated,
};

static void swapchain_gdi_frontbuffer_updated(struct wined3d_swapchain *swapchain)
{
    struct wined3d_dc_info *front;
    POINT offset = {0, 0};
    HDC src_dc, dst_dc;
    RECT draw_rect;
    HWND window;

    TRACE("swapchain %p.\n", swapchain);

    front = &swapchain->front_buffer->dc_info[0];
    if (swapchain->palette)
        wined3d_palette_apply_to_dc(swapchain->palette, front->dc);

    if (swapchain->front_buffer->resource.map_count)
        ERR("Trying to blit a mapped surface.\n");

    TRACE("Copying surface %p to screen.\n", front);

    src_dc = front->dc;
    window = swapchain->win_handle;
    dst_dc = GetDCEx(window, 0, DCX_CLIPSIBLINGS | DCX_CACHE);

    /* Front buffer coordinates are screen coordinates. Map them to the
     * destination window if not fullscreened. */
    if (swapchain->state.desc.windowed)
        ClientToScreen(window, &offset);

    TRACE("offset %s.\n", wine_dbgstr_point(&offset));

    SetRect(&draw_rect, 0, 0, swapchain->front_buffer->resource.width,
            swapchain->front_buffer->resource.height);
    IntersectRect(&draw_rect, &draw_rect, &swapchain->front_buffer_update);

    BitBlt(dst_dc, draw_rect.left - offset.x, draw_rect.top - offset.y,
            draw_rect.right - draw_rect.left, draw_rect.bottom - draw_rect.top,
            src_dc, draw_rect.left, draw_rect.top, SRCCOPY);
    ReleaseDC(window, dst_dc);

    SetRectEmpty(&swapchain->front_buffer_update);
}

static void swapchain_gdi_present(struct wined3d_swapchain *swapchain,
        const RECT *src_rect, const RECT *dst_rect, unsigned int swap_interval, DWORD flags)
{
    struct wined3d_dc_info *front, *back;
    HBITMAP bitmap;
    void *data;
    HDC dc;

    front = &swapchain->front_buffer->dc_info[0];
    back = &swapchain->back_buffers[0]->dc_info[0];

    /* Flip the surface data. */
    dc = front->dc;
    bitmap = front->bitmap;
    data = swapchain->front_buffer->resource.heap_memory;

    front->dc = back->dc;
    front->bitmap = back->bitmap;
    swapchain->front_buffer->resource.heap_memory = swapchain->back_buffers[0]->resource.heap_memory;

    back->dc = dc;
    back->bitmap = bitmap;
    swapchain->back_buffers[0]->resource.heap_memory = data;

    SetRect(&swapchain->front_buffer_update, 0, 0,
            swapchain->front_buffer->resource.width,
            swapchain->front_buffer->resource.height);
    swapchain_gdi_frontbuffer_updated(swapchain);
}

static const struct wined3d_swapchain_ops swapchain_no3d_ops =
{
    swapchain_gdi_present,
    swapchain_gdi_frontbuffer_updated,
};

static void swapchain_update_render_to_fbo(struct wined3d_swapchain *swapchain)
{
    if (wined3d_settings.offscreen_rendering_mode != ORM_FBO)
        return;

    if (!swapchain->state.desc.backbuffer_count)
    {
        TRACE("Single buffered rendering.\n");
        swapchain->render_to_fbo = FALSE;
        return;
    }

    TRACE("Rendering to FBO.\n");
    swapchain->render_to_fbo = TRUE;
}

static void wined3d_swapchain_apply_sample_count_override(const struct wined3d_swapchain *swapchain,
        enum wined3d_format_id format_id, enum wined3d_multisample_type *type, DWORD *quality)
{
    const struct wined3d_adapter *adapter;
    const struct wined3d_gl_info *gl_info;
    const struct wined3d_format *format;
    enum wined3d_multisample_type t;

    if (wined3d_settings.sample_count == ~0u)
        return;

    adapter = swapchain->device->adapter;
    gl_info = &adapter->gl_info;
    if (!(format = wined3d_get_format(adapter, format_id, WINED3D_BIND_RENDER_TARGET)))
        return;

    if ((t = min(wined3d_settings.sample_count, gl_info->limits.samples)))
        while (!(format->multisample_types & 1u << (t - 1)))
            ++t;
    TRACE("Using sample count %u.\n", t);
    *type = t;
    *quality = 0;
}

void swapchain_set_max_frame_latency(struct wined3d_swapchain *swapchain, const struct wined3d_device *device)
{
    /* Subtract 1 for the implicit OpenGL latency. */
    swapchain->max_frame_latency = device->max_frame_latency >= 2 ? device->max_frame_latency - 1 : 1;
}

static enum wined3d_format_id adapter_format_from_backbuffer_format(const struct wined3d_adapter *adapter,
        enum wined3d_format_id format_id)
{
    const struct wined3d_format *backbuffer_format;

    backbuffer_format = wined3d_get_format(adapter, format_id, WINED3D_BIND_RENDER_TARGET);
    return pixelformat_for_depth(backbuffer_format->byte_count * CHAR_BIT);
}

static HRESULT wined3d_swapchain_state_init(struct wined3d_swapchain_state *state,
        const struct wined3d_swapchain_desc *desc, HWND window, struct wined3d *wined3d,
        struct wined3d_swapchain_state_parent *parent)
{
    HRESULT hr;

    state->desc = *desc;

    if (FAILED(hr = wined3d_output_get_display_mode(desc->output, &state->original_mode, NULL)))
    {
        ERR("Failed to get current display mode, hr %#x.\n", hr);
        return hr;
    }

    if (!desc->windowed)
    {
        if (desc->flags & WINED3D_SWAPCHAIN_ALLOW_MODE_SWITCH)
        {
            state->d3d_mode.width = desc->backbuffer_width;
            state->d3d_mode.height = desc->backbuffer_height;
            state->d3d_mode.format_id = adapter_format_from_backbuffer_format(desc->output->adapter,
                    desc->backbuffer_format);
            state->d3d_mode.refresh_rate = desc->refresh_rate;
            state->d3d_mode.scanline_ordering = WINED3D_SCANLINE_ORDERING_UNKNOWN;
        }
        else
        {
            state->d3d_mode = state->original_mode;
        }
    }

    GetWindowRect(window, &state->original_window_rect);
    state->wined3d = wined3d;
    state->device_window = window;
    state->parent = parent;

    if (desc->flags & WINED3D_SWAPCHAIN_REGISTER_STATE)
        wined3d_swapchain_state_register(state);

    return hr;
}

static HRESULT wined3d_swapchain_init(struct wined3d_swapchain *swapchain, struct wined3d_device *device,
        struct wined3d_swapchain_desc *desc, struct wined3d_swapchain_state_parent *state_parent,
        void *parent, const struct wined3d_parent_ops *parent_ops,
        const struct wined3d_swapchain_ops *swapchain_ops)
{
    struct wined3d_resource_desc texture_desc;
    struct wined3d_output_desc output_desc;
    BOOL displaymode_set = FALSE;
    DWORD texture_flags = 0;
    HRESULT hr = E_FAIL;
    RECT client_rect;
    unsigned int i;
    HWND window;

    wined3d_mutex_lock();

    if (desc->backbuffer_count > 1)
    {
        FIXME("The application requested more than one back buffer, this is not properly supported.\n"
                "Please configure the application to use double buffering (1 back buffer) if possible.\n");
    }

    if (desc->swap_effect != WINED3D_SWAP_EFFECT_DISCARD
            && desc->swap_effect != WINED3D_SWAP_EFFECT_SEQUENTIAL
            && desc->swap_effect != WINED3D_SWAP_EFFECT_COPY)
        FIXME("Unimplemented swap effect %#x.\n", desc->swap_effect);

    window = desc->device_window ? desc->device_window : device->create_parms.focus_window;
    if (FAILED(hr = wined3d_swapchain_state_init(&swapchain->state, desc, window, device->wined3d, state_parent)))
    {
        ERR("Failed to initialise swapchain state, hr %#x.\n", hr);
        wined3d_mutex_unlock();
        return hr;
    }

    swapchain->swapchain_ops = swapchain_ops;
    swapchain->device = device;
    swapchain->parent = parent;
    swapchain->parent_ops = parent_ops;
    swapchain->ref = 1;
    swapchain->win_handle = window;
    swapchain->swap_interval = WINED3D_SWAP_INTERVAL_DEFAULT;
    swapchain_set_max_frame_latency(swapchain, device);

    GetClientRect(window, &client_rect);
    if (desc->windowed)
    {
        TRACE("Client rect %s.\n", wine_dbgstr_rect(&client_rect));

        if (!desc->backbuffer_width)
        {
            desc->backbuffer_width = client_rect.right ? client_rect.right : 8;
            TRACE("Updating width to %u.\n", desc->backbuffer_width);
        }
        if (!desc->backbuffer_height)
        {
            desc->backbuffer_height = client_rect.bottom ? client_rect.bottom : 8;
            TRACE("Updating height to %u.\n", desc->backbuffer_height);
        }

        if (desc->backbuffer_format == WINED3DFMT_UNKNOWN)
        {
            desc->backbuffer_format = swapchain->state.original_mode.format_id;
            TRACE("Updating format to %s.\n", debug_d3dformat(swapchain->state.original_mode.format_id));
        }
    }
    else
    {
        if (FAILED(hr = wined3d_output_get_desc(desc->output, &output_desc)))
        {
            ERR("Failed to get output description, hr %#x.\n", hr);
            goto err;
        }

        wined3d_swapchain_state_setup_fullscreen(&swapchain->state, window,
                output_desc.desktop_rect.left, output_desc.desktop_rect.top, desc->backbuffer_width,
                desc->backbuffer_height);
    }
    swapchain->state.desc = *desc;
    wined3d_swapchain_apply_sample_count_override(swapchain, swapchain->state.desc.backbuffer_format,
            &swapchain->state.desc.multisample_type, &swapchain->state.desc.multisample_quality);
    swapchain_update_render_to_fbo(swapchain);

    TRACE("Creating front buffer.\n");

    texture_desc.resource_type = WINED3D_RTYPE_TEXTURE_2D;
    texture_desc.format = swapchain->state.desc.backbuffer_format;
    texture_desc.multisample_type = swapchain->state.desc.multisample_type;
    texture_desc.multisample_quality = swapchain->state.desc.multisample_quality;
    texture_desc.usage = 0;
    if (device->wined3d->flags & WINED3D_NO3D)
        texture_desc.usage |= WINED3DUSAGE_OWNDC;
    texture_desc.bind_flags = 0;
    if (device->wined3d->flags & WINED3D_NO3D)
        texture_desc.access = WINED3D_RESOURCE_ACCESS_CPU;
    else
        texture_desc.access = WINED3D_RESOURCE_ACCESS_GPU;
    if (swapchain->state.desc.flags & WINED3D_SWAPCHAIN_LOCKABLE_BACKBUFFER)
        texture_desc.access |= WINED3D_RESOURCE_ACCESS_MAP_R | WINED3D_RESOURCE_ACCESS_MAP_W;
    texture_desc.width = swapchain->state.desc.backbuffer_width;
    texture_desc.height = swapchain->state.desc.backbuffer_height;
    texture_desc.depth = 1;
    texture_desc.size = 0;

    if (swapchain->state.desc.flags & WINED3D_SWAPCHAIN_GDI_COMPATIBLE)
        texture_flags |= WINED3D_TEXTURE_CREATE_GET_DC;

    if (FAILED(hr = device->device_parent->ops->create_swapchain_texture(device->device_parent,
            parent, &texture_desc, texture_flags, &swapchain->front_buffer)))
    {
        WARN("Failed to create front buffer, hr %#x.\n", hr);
        goto err;
    }

    wined3d_texture_set_swapchain(swapchain->front_buffer, swapchain);
    if (!(device->wined3d->flags & WINED3D_NO3D))
    {
        wined3d_texture_validate_location(swapchain->front_buffer, 0, WINED3D_LOCATION_DRAWABLE);
        wined3d_texture_invalidate_location(swapchain->front_buffer, 0, ~WINED3D_LOCATION_DRAWABLE);
    }

    /* MSDN says we're only allowed a single fullscreen swapchain per device,
     * so we should really check to see if there is a fullscreen swapchain
     * already. Does a single head count as full screen? */
    if (!desc->windowed && desc->flags & WINED3D_SWAPCHAIN_ALLOW_MODE_SWITCH)
    {
        /* Change the display settings */
        if (FAILED(hr = wined3d_output_set_display_mode(desc->output,
                &swapchain->state.d3d_mode)))
        {
            WARN("Failed to set display mode, hr %#x.\n", hr);
            goto err;
        }
        displaymode_set = TRUE;
    }

    if (swapchain->state.desc.backbuffer_count > 0)
    {
        if (!(swapchain->back_buffers = heap_calloc(swapchain->state.desc.backbuffer_count,
                sizeof(*swapchain->back_buffers))))
        {
            ERR("Failed to allocate backbuffer array memory.\n");
            hr = E_OUTOFMEMORY;
            goto err;
        }

        texture_desc.bind_flags = swapchain->state.desc.backbuffer_bind_flags;
        texture_desc.usage = 0;
        if (device->wined3d->flags & WINED3D_NO3D)
            texture_desc.usage |= WINED3DUSAGE_OWNDC;
        for (i = 0; i < swapchain->state.desc.backbuffer_count; ++i)
        {
            TRACE("Creating back buffer %u.\n", i);
            if (FAILED(hr = device->device_parent->ops->create_swapchain_texture(device->device_parent,
                    parent, &texture_desc, texture_flags, &swapchain->back_buffers[i])))
            {
                WARN("Failed to create back buffer %u, hr %#x.\n", i, hr);
                swapchain->state.desc.backbuffer_count = i;
                goto err;
            }
            wined3d_texture_set_swapchain(swapchain->back_buffers[i], swapchain);
        }
    }

    /* Swapchains share the depth/stencil buffer, so only create a single depthstencil surface. */
    if (desc->enable_auto_depth_stencil)
    {
        TRACE("Creating depth/stencil buffer.\n");
        if (!device->auto_depth_stencil_view)
        {
            struct wined3d_view_desc desc;
            struct wined3d_texture *ds;

            texture_desc.format = swapchain->state.desc.auto_depth_stencil_format;
            texture_desc.usage = 0;
            texture_desc.bind_flags = WINED3D_BIND_DEPTH_STENCIL;
            if (device->wined3d->flags & WINED3D_NO3D)
                texture_desc.access = WINED3D_RESOURCE_ACCESS_CPU;
            else
                texture_desc.access = WINED3D_RESOURCE_ACCESS_GPU;

            if (FAILED(hr = device->device_parent->ops->create_swapchain_texture(device->device_parent,
                    device->device_parent, &texture_desc, 0, &ds)))
            {
                WARN("Failed to create the auto depth/stencil surface, hr %#x.\n", hr);
                goto err;
            }

            desc.format_id = ds->resource.format->id;
            desc.flags = 0;
            desc.u.texture.level_idx = 0;
            desc.u.texture.level_count = 1;
            desc.u.texture.layer_idx = 0;
            desc.u.texture.layer_count = 1;
            hr = wined3d_rendertarget_view_create(&desc, &ds->resource, NULL, &wined3d_null_parent_ops,
                    &device->auto_depth_stencil_view);
            wined3d_texture_decref(ds);
            if (FAILED(hr))
            {
                ERR("Failed to create rendertarget view, hr %#x.\n", hr);
                goto err;
            }
        }
    }

    wined3d_swapchain_get_gamma_ramp(swapchain, &swapchain->orig_gamma);

    wined3d_mutex_unlock();

    return WINED3D_OK;

err:
    if (displaymode_set)
    {
        if (FAILED(wined3d_restore_display_modes(device->wined3d)))
            ERR("Failed to restore display mode.\n");
    }

    if (swapchain->back_buffers)
    {
        for (i = 0; i < swapchain->state.desc.backbuffer_count; ++i)
        {
            if (swapchain->back_buffers[i])
            {
                wined3d_texture_set_swapchain(swapchain->back_buffers[i], NULL);
                wined3d_texture_decref(swapchain->back_buffers[i]);
            }
        }
        heap_free(swapchain->back_buffers);
    }

    if (swapchain->front_buffer)
    {
        wined3d_texture_set_swapchain(swapchain->front_buffer, NULL);
        wined3d_texture_decref(swapchain->front_buffer);
    }

    wined3d_swapchain_state_cleanup(&swapchain->state);
    wined3d_mutex_unlock();

    return hr;
}

HRESULT wined3d_swapchain_no3d_init(struct wined3d_swapchain *swapchain_no3d, struct wined3d_device *device,
        struct wined3d_swapchain_desc *desc, struct wined3d_swapchain_state_parent *state_parent,
        void *parent, const struct wined3d_parent_ops *parent_ops)
{
    TRACE("swapchain_no3d %p, device %p, desc %p, state_parent %p, parent %p, parent_ops %p.\n",
            swapchain_no3d, device, desc, state_parent, parent, parent_ops);

    return wined3d_swapchain_init(swapchain_no3d, device, desc, state_parent, parent, parent_ops,
            &swapchain_no3d_ops);
}

HRESULT wined3d_swapchain_gl_init(struct wined3d_swapchain_gl *swapchain_gl, struct wined3d_device *device,
        struct wined3d_swapchain_desc *desc, struct wined3d_swapchain_state_parent *state_parent,
        void *parent, const struct wined3d_parent_ops *parent_ops)
{
    HRESULT hr;

    TRACE("swapchain_gl %p, device %p, desc %p, state_parent %p, parent %p, parent_ops %p.\n",
            swapchain_gl, device, desc, state_parent, parent, parent_ops);

    if (FAILED(hr = wined3d_swapchain_init(&swapchain_gl->s, device, desc, state_parent, parent,
            parent_ops, &swapchain_gl_ops)))
    {
        /* Cleanup any context that may have been created for the swapchain. */
        wined3d_cs_destroy_object(device->cs, wined3d_swapchain_gl_destroy_object, swapchain_gl);
        wined3d_cs_finish(device->cs, WINED3D_CS_QUEUE_DEFAULT);
    }

    return hr;
}

HRESULT wined3d_swapchain_vk_init(struct wined3d_swapchain_vk *swapchain_vk, struct wined3d_device *device,
        struct wined3d_swapchain_desc *desc, struct wined3d_swapchain_state_parent *state_parent,
        void *parent, const struct wined3d_parent_ops *parent_ops)
{
    HRESULT hr;

    TRACE("swapchain_vk %p, device %p, desc %p, parent %p, parent_ops %p.\n",
            swapchain_vk, device, desc, parent, parent_ops);

    if (FAILED(hr = wined3d_swapchain_init(&swapchain_vk->s, device, desc, state_parent, parent,
            parent_ops, &swapchain_vk_ops)))
        return hr;

    if (swapchain_vk->s.win_handle == GetDesktopWindow())
    {
        WARN("Creating a desktop window swapchain.\n");
        return hr;
    }

    if (FAILED(hr = wined3d_swapchain_vk_create_vulkan_swapchain(swapchain_vk)))
    {
        wined3d_swapchain_cleanup(&swapchain_vk->s);
        return hr;
    }

    return hr;
}

HRESULT CDECL wined3d_swapchain_create(struct wined3d_device *device,
        struct wined3d_swapchain_desc *desc, struct wined3d_swapchain_state_parent *state_parent,
        void *parent, const struct wined3d_parent_ops *parent_ops,
        struct wined3d_swapchain **swapchain)
{
    struct wined3d_swapchain *object;
    HRESULT hr;

    if (FAILED(hr = device->adapter->adapter_ops->adapter_create_swapchain(device,
            desc, state_parent, parent, parent_ops, &object)))
        return hr;

    if (desc->flags & WINED3D_SWAPCHAIN_IMPLICIT)
    {
        wined3d_mutex_lock();
        if (FAILED(hr = wined3d_device_set_implicit_swapchain(device, object)))
        {
            wined3d_cs_finish(device->cs, WINED3D_CS_QUEUE_DEFAULT);
            device->adapter->adapter_ops->adapter_destroy_swapchain(object);
            wined3d_mutex_unlock();
            return hr;
        }
        wined3d_mutex_unlock();
    }

    *swapchain = object;

    return hr;
}

static struct wined3d_context_gl *wined3d_swapchain_gl_create_context(struct wined3d_swapchain_gl *swapchain_gl)
{
    struct wined3d_device *device = swapchain_gl->s.device;
    struct wined3d_context_gl *context_gl;

    TRACE("Creating a new context for swapchain %p, thread %u.\n", swapchain_gl, GetCurrentThreadId());

    wined3d_from_cs(device->cs);

    if (!(context_gl = heap_alloc_zero(sizeof(*context_gl))))
    {
        ERR("Failed to allocate context memory.\n");
        return NULL;
    }

    if (FAILED(wined3d_context_gl_init(context_gl, swapchain_gl)))
    {
        WARN("Failed to initialise context.\n");
        heap_free(context_gl);
        return NULL;
    }

    if (!device_context_add(device, &context_gl->c))
    {
        ERR("Failed to add the newly created context to the context list.\n");
        wined3d_context_gl_destroy(context_gl);
        return NULL;
    }

    TRACE("Created context %p.\n", context_gl);

    context_release(&context_gl->c);

    if (!wined3d_array_reserve((void **)&swapchain_gl->contexts, &swapchain_gl->contexts_size,
            swapchain_gl->context_count + 1, sizeof(*swapchain_gl->contexts)))
    {
        ERR("Failed to allocate new context array memory.\n");
        wined3d_context_gl_destroy(context_gl);
        return NULL;
    }
    swapchain_gl->contexts[swapchain_gl->context_count++] = context_gl;

    return context_gl;
}

void wined3d_swapchain_gl_destroy_contexts(struct wined3d_swapchain_gl *swapchain_gl)
{
    unsigned int i;

    TRACE("swapchain_gl %p.\n", swapchain_gl);

    for (i = 0; i < swapchain_gl->context_count; ++i)
    {
        wined3d_context_gl_destroy(swapchain_gl->contexts[i]);
    }
    heap_free(swapchain_gl->contexts);
    swapchain_gl->contexts_size = 0;
    swapchain_gl->context_count = 0;
    swapchain_gl->contexts = NULL;
}

struct wined3d_context_gl *wined3d_swapchain_gl_get_context(struct wined3d_swapchain_gl *swapchain_gl)
{
    DWORD tid = GetCurrentThreadId();
    unsigned int i;

    for (i = 0; i < swapchain_gl->context_count; ++i)
    {
        if (swapchain_gl->contexts[i]->tid == tid)
            return swapchain_gl->contexts[i];
    }

    /* Create a new context for the thread. */
    return wined3d_swapchain_gl_create_context(swapchain_gl);
}

HDC wined3d_swapchain_gl_get_backup_dc(struct wined3d_swapchain_gl *swapchain_gl)
{
    if (!swapchain_gl->backup_dc)
    {
        TRACE("Creating the backup window for swapchain %p.\n", swapchain_gl);

        if (!(swapchain_gl->backup_wnd = CreateWindowA(WINED3D_OPENGL_WINDOW_CLASS_NAME, "WineD3D fake window",
                WS_OVERLAPPEDWINDOW, 10, 10, 10, 10, NULL, NULL, NULL, NULL)))
        {
            ERR("Failed to create a window.\n");
            return NULL;
        }

        if (!(swapchain_gl->backup_dc = GetDC(swapchain_gl->backup_wnd)))
        {
            ERR("Failed to get a DC.\n");
            DestroyWindow(swapchain_gl->backup_wnd);
            swapchain_gl->backup_wnd = NULL;
            return NULL;
        }
    }

    return swapchain_gl->backup_dc;
}

void swapchain_update_draw_bindings(struct wined3d_swapchain *swapchain)
{
    UINT i;

    wined3d_resource_update_draw_binding(&swapchain->front_buffer->resource);

    for (i = 0; i < swapchain->state.desc.backbuffer_count; ++i)
    {
        wined3d_resource_update_draw_binding(&swapchain->back_buffers[i]->resource);
    }
}

void wined3d_swapchain_activate(struct wined3d_swapchain *swapchain, BOOL activate)
{
    struct wined3d_device *device = swapchain->device;
    HWND window = swapchain->state.device_window;
    struct wined3d_output_desc output_desc;
    unsigned int screensaver_active;
    struct wined3d_output *output;
    BOOL focus_messages, filter;
    HRESULT hr;

    /* This code is not protected by the wined3d mutex, so it may run while
     * wined3d_device_reset is active. Testing on Windows shows that changing
     * focus during resets and resetting during focus change events causes
     * the application to crash with an invalid memory access. */

    if (!(focus_messages = device->wined3d->flags & WINED3D_FOCUS_MESSAGES))
        filter = wined3d_filter_messages(window, TRUE);

    if (activate)
    {
        SystemParametersInfoW(SPI_GETSCREENSAVEACTIVE, 0, &screensaver_active, 0);
        if ((device->restore_screensaver = !!screensaver_active))
            SystemParametersInfoW(SPI_SETSCREENSAVEACTIVE, FALSE, NULL, 0);

        if (!(device->create_parms.flags & WINED3DCREATE_NOWINDOWCHANGES))
        {
            /* The d3d versions do not agree on the exact messages here. D3d8 restores
             * the window but leaves the size untouched, d3d9 sets the size on an
             * invisible window, generates messages but doesn't change the window
             * properties. The implementation follows d3d9.
             *
             * Guild Wars 1 wants a WINDOWPOSCHANGED message on the device window to
             * resume drawing after a focus loss. */
            output = wined3d_swapchain_get_output(swapchain);
            if (!output)
            {
                ERR("Failed to get output from swapchain %p.\n", swapchain);
                return;
            }

            if (SUCCEEDED(hr = wined3d_output_get_desc(output, &output_desc)))
                SetWindowPos(window, NULL, output_desc.desktop_rect.left,
                        output_desc.desktop_rect.top, swapchain->state.desc.backbuffer_width,
                        swapchain->state.desc.backbuffer_height, SWP_NOACTIVATE | SWP_NOZORDER);
            else
                ERR("Failed to get output description, hr %#x.\n", hr);
        }

        if (device->wined3d->flags & WINED3D_RESTORE_MODE_ON_ACTIVATE)
        {
            output = wined3d_swapchain_get_output(swapchain);
            if (!output)
            {
                ERR("Failed to get output from swapchain %p.\n", swapchain);
                return;
            }

            if (FAILED(hr = wined3d_output_set_display_mode(output,
                    &swapchain->state.d3d_mode)))
                ERR("Failed to set display mode, hr %#x.\n", hr);
        }

        if (swapchain == device->swapchains[0])
            device->device_parent->ops->activate(device->device_parent, TRUE);
    }
    else
    {
        if (device->restore_screensaver)
        {
            SystemParametersInfoW(SPI_SETSCREENSAVEACTIVE, TRUE, NULL, 0);
            device->restore_screensaver = FALSE;
        }

        if (FAILED(hr = wined3d_restore_display_modes(device->wined3d)))
            ERR("Failed to restore display modes, hr %#x.\n", hr);

        swapchain->reapply_mode = TRUE;

        /* Some DDraw apps (Deus Ex: GOTY, and presumably all UT 1 based games) destroy the device
         * during window minimization. Do our housekeeping now, as the device may not exist after
         * the ShowWindow call.
         *
         * In d3d9, the device is marked lost after the window is minimized. If we find an app
         * that needs this behavior (e.g. because it calls TestCooperativeLevel in the window proc)
         * we'll have to control this via a create flag. Note that the device and swapchain are not
         * safe to access after the ShowWindow call. */
        if (swapchain == device->swapchains[0])
            device->device_parent->ops->activate(device->device_parent, FALSE);

        if (!(device->create_parms.flags & WINED3DCREATE_NOWINDOWCHANGES) && IsWindowVisible(window))
            ShowWindow(window, SW_MINIMIZE);
    }

    if (!focus_messages)
        wined3d_filter_messages(window, filter);
}

HRESULT CDECL wined3d_swapchain_resize_buffers(struct wined3d_swapchain *swapchain, unsigned int buffer_count,
        unsigned int width, unsigned int height, enum wined3d_format_id format_id,
        enum wined3d_multisample_type multisample_type, unsigned int multisample_quality)
{
    struct wined3d_swapchain_desc *desc = &swapchain->state.desc;
    BOOL update_desc = FALSE;

    TRACE("swapchain %p, buffer_count %u, width %u, height %u, format %s, "
            "multisample_type %#x, multisample_quality %#x.\n",
            swapchain, buffer_count, width, height, debug_d3dformat(format_id),
            multisample_type, multisample_quality);

    wined3d_swapchain_apply_sample_count_override(swapchain, format_id, &multisample_type, &multisample_quality);

    if (buffer_count && buffer_count != desc->backbuffer_count)
        FIXME("Cannot change the back buffer count yet.\n");

    wined3d_cs_finish(swapchain->device->cs, WINED3D_CS_QUEUE_DEFAULT);

    if (!width || !height)
    {
        /* The application is requesting that either the swapchain width or
         * height be set to the corresponding dimension in the window's
         * client rect. */

        RECT client_rect;

        if (!desc->windowed)
            return WINED3DERR_INVALIDCALL;

        if (!GetClientRect(swapchain->state.device_window, &client_rect))
        {
            ERR("Failed to get client rect, last error %#x.\n", GetLastError());
            return WINED3DERR_INVALIDCALL;
        }

        if (!width)
            width = client_rect.right;

        if (!height)
            height = client_rect.bottom;
    }

    if (width != desc->backbuffer_width || height != desc->backbuffer_height)
    {
        desc->backbuffer_width = width;
        desc->backbuffer_height = height;
        update_desc = TRUE;
    }

    if (format_id == WINED3DFMT_UNKNOWN)
    {
        if (!desc->windowed)
            return WINED3DERR_INVALIDCALL;
        format_id = swapchain->state.original_mode.format_id;
    }

    if (format_id != desc->backbuffer_format)
    {
        desc->backbuffer_format = format_id;
        update_desc = TRUE;
    }

    if (multisample_type != desc->multisample_type
            || multisample_quality != desc->multisample_quality)
    {
        desc->multisample_type = multisample_type;
        desc->multisample_quality = multisample_quality;
        update_desc = TRUE;
    }

    if (update_desc)
    {
        HRESULT hr;
        UINT i;

        if (FAILED(hr = wined3d_texture_update_desc(swapchain->front_buffer, 0, desc->backbuffer_width,
                desc->backbuffer_height, desc->backbuffer_format,
                desc->multisample_type, desc->multisample_quality, NULL, 0)))
            return hr;

        for (i = 0; i < desc->backbuffer_count; ++i)
        {
            if (FAILED(hr = wined3d_texture_update_desc(swapchain->back_buffers[i], 0, desc->backbuffer_width,
                    desc->backbuffer_height, desc->backbuffer_format,
                    desc->multisample_type, desc->multisample_quality, NULL, 0)))
                return hr;
        }
    }

    swapchain_update_render_to_fbo(swapchain);
    swapchain_update_draw_bindings(swapchain);

    return WINED3D_OK;
}

static HRESULT wined3d_swapchain_state_set_display_mode(struct wined3d_swapchain_state *state,
        struct wined3d_output *output, struct wined3d_display_mode *mode)
{
    HRESULT hr;

    if (state->desc.flags & WINED3D_SWAPCHAIN_USE_CLOSEST_MATCHING_MODE)
    {
        if (FAILED(hr = wined3d_output_find_closest_matching_mode(output, mode)))
        {
            WARN("Failed to find closest matching mode, hr %#x.\n", hr);
        }
    }

    if (output != state->desc.output)
    {
        if (FAILED(hr = wined3d_restore_display_modes(state->wined3d)))
        {
            WARN("Failed to restore display modes, hr %#x.\n", hr);
            return hr;
        }

        if (FAILED(hr = wined3d_output_get_display_mode(output, &state->original_mode, NULL)))
        {
            WARN("Failed to get current display mode, hr %#x.\n", hr);
            return hr;
        }
    }

    if (FAILED(hr = wined3d_output_set_display_mode(output, mode)))
    {
        WARN("Failed to set display mode, hr %#x.\n", hr);
        return WINED3DERR_INVALIDCALL;
    }

    return WINED3D_OK;
}

HRESULT CDECL wined3d_swapchain_state_resize_target(struct wined3d_swapchain_state *state,
        const struct wined3d_display_mode *mode)
{
    struct wined3d_display_mode actual_mode;
    struct wined3d_output_desc output_desc;
    RECT original_window_rect, window_rect;
    int x, y, width, height;
    HWND window;
    HRESULT hr;

    TRACE("state %p, mode %p.\n", state, mode);

    wined3d_mutex_lock();

    window = state->device_window;

    if (state->desc.windowed)
    {
        SetRect(&window_rect, 0, 0, mode->width, mode->height);
        AdjustWindowRectEx(&window_rect,
                GetWindowLongW(window, GWL_STYLE), FALSE,
                GetWindowLongW(window, GWL_EXSTYLE));
        GetWindowRect(window, &original_window_rect);

        x = original_window_rect.left;
        y = original_window_rect.top;
        width = window_rect.right - window_rect.left;
        height = window_rect.bottom - window_rect.top;
    }
    else
    {
        if (state->desc.flags & WINED3D_SWAPCHAIN_ALLOW_MODE_SWITCH)
        {
            actual_mode = *mode;
            if (FAILED(hr = wined3d_swapchain_state_set_display_mode(state, state->desc.output,
                    &actual_mode)))
            {
                ERR("Failed to set display mode, hr %#x.\n", hr);
                wined3d_mutex_unlock();
                return hr;
            }
        }

        if (FAILED(hr = wined3d_output_get_desc(state->desc.output, &output_desc)))
        {
            ERR("Failed to get output description, hr %#x.\n", hr);
            wined3d_mutex_unlock();
            return hr;
        }

        x = output_desc.desktop_rect.left;
        y = output_desc.desktop_rect.top;
        width = output_desc.desktop_rect.right - output_desc.desktop_rect.left;
        height = output_desc.desktop_rect.bottom - output_desc.desktop_rect.top;
    }

    wined3d_mutex_unlock();

    MoveWindow(window, x, y, width, height, TRUE);

    return WINED3D_OK;
}

static LONG fullscreen_style(LONG style)
{
    /* Make sure the window is managed, otherwise we won't get keyboard input. */
    style |= WS_POPUP | WS_SYSMENU;
    style &= ~(WS_CAPTION | WS_THICKFRAME);

    return style;
}

static LONG fullscreen_exstyle(LONG exstyle)
{
    /* Filter out window decorations. */
    exstyle &= ~(WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE);

    return exstyle;
}

HRESULT wined3d_swapchain_state_setup_fullscreen(struct wined3d_swapchain_state *state,
        HWND window, int x, int y, int width, int height)
{
    unsigned int window_pos_flags = SWP_FRAMECHANGED | SWP_NOACTIVATE;
    LONG style, exstyle;
    BOOL filter;

    TRACE("Setting up window %p for fullscreen mode.\n", window);

    if (!IsWindow(window))
    {
        WARN("%p is not a valid window.\n", window);
        return WINED3DERR_NOTAVAILABLE;
    }

    if (state->style || state->exstyle)
    {
        ERR("Changing the window style for window %p, but another style (%08x, %08x) is already stored.\n",
                window, state->style, state->exstyle);
    }

    if (state->desc.flags & WINED3D_SWAPCHAIN_NO_WINDOW_CHANGES)
        window_pos_flags |= SWP_NOZORDER;
    else
        window_pos_flags |= SWP_SHOWWINDOW;

    state->style = GetWindowLongW(window, GWL_STYLE);
    state->exstyle = GetWindowLongW(window, GWL_EXSTYLE);

    style = fullscreen_style(state->style);
    exstyle = fullscreen_exstyle(state->exstyle);

    TRACE("Old style was %08x, %08x, setting to %08x, %08x.\n",
            state->style, state->exstyle, style, exstyle);

    filter = wined3d_filter_messages(window, TRUE);

    SetWindowLongW(window, GWL_STYLE, style);
    SetWindowLongW(window, GWL_EXSTYLE, exstyle);
    SetWindowPos(window, HWND_TOPMOST, x, y, width, height, window_pos_flags);

    wined3d_filter_messages(window, filter);

    return WINED3D_OK;
}

void wined3d_swapchain_state_restore_from_fullscreen(struct wined3d_swapchain_state *state,
        HWND window, const RECT *window_rect)
{
    unsigned int window_pos_flags = SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOACTIVATE;
    HWND window_pos_after = NULL;
    LONG style, exstyle;
    RECT rect = {0};
    BOOL filter;

    if (!state->style && !state->exstyle)
        return;

    if ((state->desc.flags & WINED3D_SWAPCHAIN_RESTORE_WINDOW_STATE)
            && !(state->desc.flags & WINED3D_SWAPCHAIN_NO_WINDOW_CHANGES))
    {
        window_pos_after = (state->exstyle & WS_EX_TOPMOST) ? HWND_TOPMOST : HWND_NOTOPMOST;
        window_pos_flags |= (state->style & WS_VISIBLE) ? SWP_SHOWWINDOW : SWP_HIDEWINDOW;
        window_pos_flags &= ~SWP_NOZORDER;
    }

    style = GetWindowLongW(window, GWL_STYLE);
    exstyle = GetWindowLongW(window, GWL_EXSTYLE);

    /* These flags are set by wined3d_device_setup_fullscreen_window, not the
     * application, and we want to ignore them in the test below, since it's
     * not the application's fault that they changed. Additionally, we want to
     * preserve the current status of these flags (i.e. don't restore them) to
     * more closely emulate the behavior of Direct3D, which leaves these flags
     * alone when returning to windowed mode. */
    state->style ^= (state->style ^ style) & WS_VISIBLE;
    state->exstyle ^= (state->exstyle ^ exstyle) & WS_EX_TOPMOST;

    TRACE("Restoring window style of window %p to %08x, %08x.\n",
            window, state->style, state->exstyle);

    filter = wined3d_filter_messages(window, TRUE);

    /* Only restore the style if the application didn't modify it during the
     * fullscreen phase. Some applications change it before calling Reset()
     * when switching between windowed and fullscreen modes (HL2), some
     * depend on the original style (Eve Online). */
    if (style == fullscreen_style(state->style) && exstyle == fullscreen_exstyle(state->exstyle))
    {
        SetWindowLongW(window, GWL_STYLE, state->style);
        SetWindowLongW(window, GWL_EXSTYLE, state->exstyle);
    }

    if (window_rect)
        rect = *window_rect;
    else
        window_pos_flags |= (SWP_NOMOVE | SWP_NOSIZE);
    SetWindowPos(window, window_pos_after, rect.left, rect.top,
            rect.right - rect.left, rect.bottom - rect.top, window_pos_flags);

    wined3d_filter_messages(window, filter);

    /* Delete the old values. */
    state->style = 0;
    state->exstyle = 0;
}

HRESULT CDECL wined3d_swapchain_state_set_fullscreen(struct wined3d_swapchain_state *state,
        const struct wined3d_swapchain_desc *swapchain_desc,
        const struct wined3d_display_mode *mode)
{
    struct wined3d_display_mode actual_mode;
    struct wined3d_output_desc output_desc;
    BOOL windowed = state->desc.windowed;
    HRESULT hr;

    TRACE("state %p, swapchain_desc %p, mode %p.\n", state, swapchain_desc, mode);

    if (state->desc.flags & WINED3D_SWAPCHAIN_ALLOW_MODE_SWITCH)
    {
        if (mode)
        {
            actual_mode = *mode;
            if (FAILED(hr = wined3d_swapchain_state_set_display_mode(state, swapchain_desc->output,
                    &actual_mode)))
                return hr;
        }
        else
        {
            if (!swapchain_desc->windowed)
            {
                actual_mode.width = swapchain_desc->backbuffer_width;
                actual_mode.height = swapchain_desc->backbuffer_height;
                actual_mode.refresh_rate = swapchain_desc->refresh_rate;
                actual_mode.format_id = adapter_format_from_backbuffer_format(swapchain_desc->output->adapter,
                        swapchain_desc->backbuffer_format);
                actual_mode.scanline_ordering = WINED3D_SCANLINE_ORDERING_UNKNOWN;
                if (FAILED(hr = wined3d_swapchain_state_set_display_mode(state, swapchain_desc->output,
                        &actual_mode)))
                    return hr;
            }
            else
            {
                if (FAILED(hr = wined3d_restore_display_modes(state->wined3d)))
                {
                    WARN("Failed to restore display modes for all outputs, hr %#x.\n", hr);
                    return hr;
                }
            }
        }
    }
    else
    {
        if (mode)
            WARN("WINED3D_SWAPCHAIN_ALLOW_MODE_SWITCH is not set, ignoring mode.\n");

        if (FAILED(hr = wined3d_output_get_display_mode(swapchain_desc->output, &actual_mode,
                NULL)))
        {
            ERR("Failed to get display mode, hr %#x.\n", hr);
            return WINED3DERR_INVALIDCALL;
        }
    }

    if (!swapchain_desc->windowed)
    {
        unsigned int width = actual_mode.width;
        unsigned int height = actual_mode.height;

        if (FAILED(hr = wined3d_output_get_desc(swapchain_desc->output, &output_desc)))
        {
            ERR("Failed to get output description, hr %#x.\n", hr);
            return hr;
        }

        if (state->desc.windowed)
        {
            /* Switch from windowed to fullscreen */
            if (FAILED(hr = wined3d_swapchain_state_setup_fullscreen(state, state->device_window,
                    output_desc.desktop_rect.left, output_desc.desktop_rect.top, width, height)))
                return hr;
        }
        else
        {
            HWND window = state->device_window;
            BOOL filter;

            /* Fullscreen -> fullscreen mode change */
            filter = wined3d_filter_messages(window, TRUE);
            MoveWindow(window, output_desc.desktop_rect.left, output_desc.desktop_rect.top, width,
                    height, TRUE);
            ShowWindow(window, SW_SHOW);
            wined3d_filter_messages(window, filter);
        }
        state->d3d_mode = actual_mode;
    }
    else if (!state->desc.windowed)
    {
        /* Fullscreen -> windowed switch */
        RECT *window_rect = NULL;
        if (state->desc.flags & WINED3D_SWAPCHAIN_RESTORE_WINDOW_RECT)
            window_rect = &state->original_window_rect;
        wined3d_swapchain_state_restore_from_fullscreen(state, state->device_window, window_rect);
    }

    state->desc.output = swapchain_desc->output;
    state->desc.windowed = swapchain_desc->windowed;

    if (windowed != state->desc.windowed)
        state->parent->ops->windowed_state_changed(state->parent, state->desc.windowed);

    return WINED3D_OK;
}

BOOL CDECL wined3d_swapchain_state_is_windowed(const struct wined3d_swapchain_state *state)
{
    TRACE("state %p.\n", state);

    return state->desc.windowed;
}

void CDECL wined3d_swapchain_state_destroy(struct wined3d_swapchain_state *state)
{
    wined3d_swapchain_state_cleanup(state);
    heap_free(state);
}

HRESULT CDECL wined3d_swapchain_state_create(const struct wined3d_swapchain_desc *desc,
        HWND window, struct wined3d *wined3d, struct wined3d_swapchain_state_parent *state_parent,
        struct wined3d_swapchain_state **state)
{
    struct wined3d_swapchain_state *s;
    HRESULT hr;

    TRACE("desc %p, window %p, wined3d %p, state %p.\n", desc, window, wined3d, state);

    if (!(s = heap_alloc_zero(sizeof(*s))))
        return E_OUTOFMEMORY;

    if (FAILED(hr = wined3d_swapchain_state_init(s, desc, window, wined3d, state_parent)))
    {
        heap_free(s);
        return hr;
    }

    *state = s;

    return hr;
}
