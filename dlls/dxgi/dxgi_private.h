/*
 * Copyright 2008 Henri Verbeet for CodeWeavers
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

#ifndef __WINE_DXGI_PRIVATE_H
#define __WINE_DXGI_PRIVATE_H

#include "wine/debug.h"
#include "wine/heap.h"

#include <assert.h>

#define COBJMACROS
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "objbase.h"
#include "winnls.h"

#include "dxgi1_6.h"
#include "d3d10_1.h"
#include "d3d12.h"
#ifdef DXGI_INIT_GUID
#include "initguid.h"
#endif
#include "wine/wined3d.h"
#include "wine/winedxgi.h"

enum dxgi_frame_latency
{
    DXGI_FRAME_LATENCY_MAX     = 16,
};

/* Layered device */
enum dxgi_device_layer_id
{
    DXGI_DEVICE_LAYER_DEBUG1        = 0x8,
    DXGI_DEVICE_LAYER_THREAD_SAFE   = 0x10,
    DXGI_DEVICE_LAYER_DEBUG2        = 0x20,
    DXGI_DEVICE_LAYER_SWITCH_TO_REF = 0x30,
    DXGI_DEVICE_LAYER_D3D10_DEVICE  = 0xffffffff,
};

struct layer_get_size_args
{
    DWORD unknown0;
    DWORD unknown1;
    DWORD *unknown2;
    DWORD *unknown3;
    IDXGIAdapter *adapter;
    WORD interface_major;
    WORD interface_minor;
    WORD version_build;
    WORD version_revision;
};

struct dxgi_device_layer
{
    enum dxgi_device_layer_id id;
    HRESULT (WINAPI *init)(enum dxgi_device_layer_id id, DWORD *count, DWORD *values);
    UINT (WINAPI *get_size)(enum dxgi_device_layer_id id, struct layer_get_size_args *args, DWORD unknown0);
    HRESULT (WINAPI *create)(enum dxgi_device_layer_id id, void **layer_base, DWORD unknown0,
            void *device_object, REFIID riid, void **device_layer);
};

/* TRACE helper functions */
const char *debug_dxgi_format(DXGI_FORMAT format) DECLSPEC_HIDDEN;
const char *debug_dxgi_mode(const DXGI_MODE_DESC *desc) DECLSPEC_HIDDEN;
const char *debug_dxgi_mode1(const DXGI_MODE_DESC1 *desc) DECLSPEC_HIDDEN;
void dump_feature_levels(const D3D_FEATURE_LEVEL *feature_levels, unsigned int level_count) DECLSPEC_HIDDEN;

DXGI_FORMAT dxgi_format_from_wined3dformat(enum wined3d_format_id format) DECLSPEC_HIDDEN;
enum wined3d_format_id wined3dformat_from_dxgi_format(DXGI_FORMAT format) DECLSPEC_HIDDEN;
void dxgi_sample_desc_from_wined3d(DXGI_SAMPLE_DESC *desc,
        enum wined3d_multisample_type wined3d_type, unsigned int wined3d_quality) DECLSPEC_HIDDEN;
void wined3d_sample_desc_from_dxgi(enum wined3d_multisample_type *wined3d_type,
        unsigned int *wined3d_quality, const DXGI_SAMPLE_DESC *dxgi_desc) DECLSPEC_HIDDEN;
void wined3d_display_mode_from_dxgi(struct wined3d_display_mode *wined3d_mode,
        const DXGI_MODE_DESC *mode) DECLSPEC_HIDDEN;
void wined3d_display_mode_from_dxgi1(struct wined3d_display_mode *wined3d_mode,
        const DXGI_MODE_DESC1 *mode) DECLSPEC_HIDDEN;
DXGI_USAGE dxgi_usage_from_wined3d_bind_flags(unsigned int wined3d_bind_flags) DECLSPEC_HIDDEN;
unsigned int wined3d_bind_flags_from_dxgi_usage(DXGI_USAGE usage) DECLSPEC_HIDDEN;
unsigned int dxgi_swapchain_flags_from_wined3d(unsigned int wined3d_flags) DECLSPEC_HIDDEN;
HRESULT dxgi_get_output_from_window(IWineDXGIFactory *factory, HWND window, IDXGIOutput **dxgi_output)
        DECLSPEC_HIDDEN;
HRESULT wined3d_swapchain_desc_from_dxgi(struct wined3d_swapchain_desc *wined3d_desc,
        IDXGIOutput *dxgi_containing_output, HWND window, const DXGI_SWAP_CHAIN_DESC1 *dxgi_desc,
        const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *dxgi_fullscreen_desc) DECLSPEC_HIDDEN;

HRESULT dxgi_get_private_data(struct wined3d_private_store *store,
        REFGUID guid, UINT *data_size, void *data) DECLSPEC_HIDDEN;
HRESULT dxgi_set_private_data(struct wined3d_private_store *store,
        REFGUID guid, UINT data_size, const void *data) DECLSPEC_HIDDEN;
HRESULT dxgi_set_private_data_interface(struct wined3d_private_store *store,
        REFGUID guid, const IUnknown *object) DECLSPEC_HIDDEN;

/* IDXGIFactory */
struct dxgi_factory
{
    IWineDXGIFactory IWineDXGIFactory_iface;
    LONG refcount;
    struct wined3d_private_store private_store;
    struct wined3d *wined3d;
    BOOL extended;
    HWND device_window;
};

HRESULT dxgi_factory_create(REFIID riid, void **factory, BOOL extended) DECLSPEC_HIDDEN;
HWND dxgi_factory_get_device_window(struct dxgi_factory *factory) DECLSPEC_HIDDEN;
struct dxgi_factory *unsafe_impl_from_IDXGIFactory(IDXGIFactory *iface) DECLSPEC_HIDDEN;

/* IDXGIDevice */
struct dxgi_device
{
    IWineDXGIDevice IWineDXGIDevice_iface;
    IWineDXGISwapChainFactory IWineDXGISwapChainFactory_iface;
    IUnknown *child_layer;
    LONG refcount;
    struct wined3d_private_store private_store;
    struct wined3d_device *wined3d_device;
    struct wined3d_swapchain *implicit_swapchain;
    IWineDXGIAdapter *adapter;
};

HRESULT dxgi_device_init(struct dxgi_device *device, struct dxgi_device_layer *layer,
        IDXGIFactory *factory, IDXGIAdapter *adapter,
        const D3D_FEATURE_LEVEL *feature_levels, unsigned int level_count) DECLSPEC_HIDDEN;

/* IDXGIOutput */
struct dxgi_output
{
    IDXGIOutput6 IDXGIOutput6_iface;
    LONG refcount;
    struct wined3d_output *wined3d_output;
    struct wined3d_private_store private_store;
    struct dxgi_adapter *adapter;
};

HRESULT dxgi_output_create(struct dxgi_adapter *adapter, unsigned int output_idx,
        struct dxgi_output **output) DECLSPEC_HIDDEN;
struct dxgi_output *unsafe_impl_from_IDXGIOutput(IDXGIOutput *iface) DECLSPEC_HIDDEN;

/* IDXGIAdapter */
struct dxgi_adapter
{
    IWineDXGIAdapter IWineDXGIAdapter_iface;
    LONG refcount;
    struct wined3d_adapter *wined3d_adapter;
    struct wined3d_private_store private_store;
    UINT ordinal;
    struct dxgi_factory *factory;
};

HRESULT dxgi_adapter_create(struct dxgi_factory *factory, UINT ordinal,
        struct dxgi_adapter **adapter) DECLSPEC_HIDDEN;
struct dxgi_adapter *unsafe_impl_from_IDXGIAdapter(IDXGIAdapter *iface) DECLSPEC_HIDDEN;

/* IDXGISwapChain */
struct d3d11_swapchain
{
    IDXGISwapChain1 IDXGISwapChain1_iface;
    LONG refcount;
    struct wined3d_private_store private_store;
    struct wined3d_swapchain *wined3d_swapchain;
    struct wined3d_swapchain_state_parent state_parent;
    IWineDXGIDevice *device;
    IWineDXGIFactory *factory;

    IDXGIOutput *target;
};

HRESULT d3d11_swapchain_init(struct d3d11_swapchain *swapchain, struct dxgi_device *device,
        struct wined3d_swapchain_desc *desc) DECLSPEC_HIDDEN;

HRESULT d3d12_swapchain_create(IWineDXGIFactory *factory, ID3D12CommandQueue *queue, HWND window,
        const DXGI_SWAP_CHAIN_DESC1 *swapchain_desc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *fullscreen_desc,
        IDXGISwapChain1 **swapchain) DECLSPEC_HIDDEN;

BOOL dxgi_validate_swapchain_desc(const DXGI_SWAP_CHAIN_DESC1 *desc) DECLSPEC_HIDDEN;

/* IDXGISurface */
struct dxgi_surface
{
    IDXGISurface1 IDXGISurface1_iface;
    IUnknown IUnknown_iface;
    IUnknown *outer_unknown;
    LONG refcount;
    struct wined3d_private_store private_store;
    IDXGIDevice *device;
    struct wined3d_texture *wined3d_texture;
    HDC dc;
};

HRESULT dxgi_surface_init(struct dxgi_surface *surface, IDXGIDevice *device,
        IUnknown *outer, struct wined3d_texture *wined3d_texture) DECLSPEC_HIDDEN;

HRESULT get_re8_dxgi_factory(REFIID riid, void **factory) DECLSPEC_HIDDEN;

#endif /* __WINE_DXGI_PRIVATE_H */
