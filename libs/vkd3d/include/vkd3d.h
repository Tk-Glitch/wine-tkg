/*
 * Copyright 2016 Józef Kucia for CodeWeavers
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

#ifndef __VKD3D_H
#define __VKD3D_H

#include <vkd3d_types.h>

#ifndef VKD3D_NO_WIN32_TYPES
# include <windows.h>
# include <d3d12.h>
#endif  /* VKD3D_NO_WIN32_TYPES */

#ifndef VKD3D_NO_VULKAN_H
# include <wine/vulkan.h>
#endif  /* VKD3D_NO_VULKAN_H */

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

enum vkd3d_structure_type
{
    /* 1.0 */
    VKD3D_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    VKD3D_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
    VKD3D_STRUCTURE_TYPE_IMAGE_RESOURCE_CREATE_INFO,

    /* 1.1 */
    VKD3D_STRUCTURE_TYPE_OPTIONAL_INSTANCE_EXTENSIONS_INFO,

    /* 1.2 */
    VKD3D_STRUCTURE_TYPE_OPTIONAL_DEVICE_EXTENSIONS_INFO,
    VKD3D_STRUCTURE_TYPE_APPLICATION_INFO,

    /* 1.3 */
    VKD3D_STRUCTURE_TYPE_HOST_TIME_DOMAIN_INFO,

    VKD3D_FORCE_32_BIT_ENUM(VKD3D_STRUCTURE_TYPE),
};

enum vkd3d_api_version
{
    VKD3D_API_VERSION_1_0,
    VKD3D_API_VERSION_1_1,
    VKD3D_API_VERSION_1_2,
    VKD3D_API_VERSION_1_3,
    VKD3D_API_VERSION_1_4,
};

typedef HRESULT (*PFN_vkd3d_signal_event)(HANDLE event);

typedef void * (*PFN_vkd3d_thread)(void *data);

typedef void * (*PFN_vkd3d_create_thread)(PFN_vkd3d_thread thread_main, void *data);
typedef HRESULT (*PFN_vkd3d_join_thread)(void *thread);

struct vkd3d_instance;

struct vkd3d_instance_create_info
{
    enum vkd3d_structure_type type;
    const void *next;

    PFN_vkd3d_signal_event pfn_signal_event;
    PFN_vkd3d_create_thread pfn_create_thread;
    PFN_vkd3d_join_thread pfn_join_thread;
    size_t wchar_size;

    /* If set to NULL, libvkd3d loads libvulkan. */
    PFN_vkGetInstanceProcAddr pfn_vkGetInstanceProcAddr;

    const char * const *instance_extensions;
    uint32_t instance_extension_count;
};

/* Extends vkd3d_instance_create_info. Available since 1.1. */
struct vkd3d_optional_instance_extensions_info
{
    enum vkd3d_structure_type type;
    const void *next;

    const char * const *extensions;
    uint32_t extension_count;
};

/* Extends vkd3d_instance_create_info. Available since 1.2. */
struct vkd3d_application_info
{
    enum vkd3d_structure_type type;
    const void *next;

    const char *application_name;
    uint32_t application_version;

    const char *engine_name; /* "vkd3d" if NULL */
    uint32_t engine_version; /* vkd3d version if engine_name is NULL */

    enum vkd3d_api_version api_version;
};

/* Extends vkd3d_instance_create_info. Available since 1.3. */
struct vkd3d_host_time_domain_info
{
    enum vkd3d_structure_type type;
    const void *next;

    uint64_t ticks_per_second;
};

struct vkd3d_device_create_info
{
    enum vkd3d_structure_type type;
    const void *next;

    D3D_FEATURE_LEVEL minimum_feature_level;

    struct vkd3d_instance *instance;
    const struct vkd3d_instance_create_info *instance_create_info;

    VkPhysicalDevice vk_physical_device;

    const char * const *device_extensions;
    uint32_t device_extension_count;

    IUnknown *parent;
    LUID adapter_luid;
};

/* Extends vkd3d_device_create_info. Available since 1.2. */
struct vkd3d_optional_device_extensions_info
{
    enum vkd3d_structure_type type;
    const void *next;

    const char * const *extensions;
    uint32_t extension_count;
};

/* vkd3d_image_resource_create_info flags */
#define VKD3D_RESOURCE_INITIAL_STATE_TRANSITION 0x00000001
#define VKD3D_RESOURCE_PRESENT_STATE_TRANSITION 0x00000002

struct vkd3d_image_resource_create_info
{
    enum vkd3d_structure_type type;
    const void *next;

    VkImage vk_image;
    D3D12_RESOURCE_DESC desc;
    unsigned int flags;
    D3D12_RESOURCE_STATES present_state;
};

#ifdef LIBVKD3D_SOURCE
# define VKD3D_API VKD3D_EXPORT
#else
# define VKD3D_API VKD3D_IMPORT
#endif

#ifndef VKD3D_NO_PROTOTYPES

VKD3D_API HRESULT vkd3d_create_instance(const struct vkd3d_instance_create_info *create_info,
        struct vkd3d_instance **instance);
VKD3D_API ULONG vkd3d_instance_decref(struct vkd3d_instance *instance);
VKD3D_API VkInstance vkd3d_instance_get_vk_instance(struct vkd3d_instance *instance);
VKD3D_API ULONG vkd3d_instance_incref(struct vkd3d_instance *instance);

VKD3D_API HRESULT vkd3d_create_device(const struct vkd3d_device_create_info *create_info,
        REFIID iid, void **device);
VKD3D_API IUnknown *vkd3d_get_device_parent(ID3D12Device *device);
VKD3D_API VkDevice vkd3d_get_vk_device(ID3D12Device *device);
VKD3D_API VkPhysicalDevice vkd3d_get_vk_physical_device(ID3D12Device *device);
VKD3D_API struct vkd3d_instance *vkd3d_instance_from_device(ID3D12Device *device);

VKD3D_API uint32_t vkd3d_get_vk_queue_family_index(ID3D12CommandQueue *queue);
VKD3D_API VkQueue vkd3d_acquire_vk_queue(ID3D12CommandQueue *queue);
VKD3D_API void vkd3d_release_vk_queue(ID3D12CommandQueue *queue);

VKD3D_API HRESULT vkd3d_create_image_resource(ID3D12Device *device,
        const struct vkd3d_image_resource_create_info *create_info, ID3D12Resource **resource);
VKD3D_API ULONG vkd3d_resource_decref(ID3D12Resource *resource);
VKD3D_API ULONG vkd3d_resource_incref(ID3D12Resource *resource);

VKD3D_API HRESULT vkd3d_serialize_root_signature(const D3D12_ROOT_SIGNATURE_DESC *desc,
        D3D_ROOT_SIGNATURE_VERSION version, ID3DBlob **blob, ID3DBlob **error_blob);
VKD3D_API HRESULT vkd3d_create_root_signature_deserializer(const void *data, SIZE_T data_size,
        REFIID iid, void **deserializer);

VKD3D_API VkFormat vkd3d_get_vk_format(DXGI_FORMAT format);

/* 1.1 */
VKD3D_API DXGI_FORMAT vkd3d_get_dxgi_format(VkFormat format);

/* 1.2 */
VKD3D_API HRESULT vkd3d_serialize_versioned_root_signature(const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *desc,
        ID3DBlob **blob, ID3DBlob **error_blob);
VKD3D_API HRESULT vkd3d_create_versioned_root_signature_deserializer(const void *data, SIZE_T data_size,
        REFIID iid, void **deserializer);

/**
 * Set a callback to be called when vkd3d outputs debug logging.
 *
 * If NULL, or if this function has not been called, libvkd3d will print all
 * enabled log output to stderr.
 *
 * Calling this function will also set the log callback for libvkd3d-shader.
 *
 * \param callback Callback function to set.
 *
 * \since 1.4
 */
VKD3D_API void vkd3d_set_log_callback(PFN_vkd3d_log callback);

#endif  /* VKD3D_NO_PROTOTYPES */

/*
 * Function pointer typedefs for vkd3d functions.
 */
typedef HRESULT (*PFN_vkd3d_create_instance)(const struct vkd3d_instance_create_info *create_info,
        struct vkd3d_instance **instance);
typedef ULONG (*PFN_vkd3d_instance_decref)(struct vkd3d_instance *instance);
typedef VkInstance (*PFN_vkd3d_instance_get_vk_instance)(struct vkd3d_instance *instance);
typedef ULONG (*PFN_vkd3d_instance_incref)(struct vkd3d_instance *instance);

typedef HRESULT (*PFN_vkd3d_create_device)(const struct vkd3d_device_create_info *create_info,
        REFIID iid, void **device);
typedef IUnknown * (*PFN_vkd3d_get_device_parent)(ID3D12Device *device);
typedef VkDevice (*PFN_vkd3d_get_vk_device)(ID3D12Device *device);
typedef VkPhysicalDevice (*PFN_vkd3d_get_vk_physical_device)(ID3D12Device *device);
typedef struct vkd3d_instance * (*PFN_vkd3d_instance_from_device)(ID3D12Device *device);

typedef uint32_t (*PFN_vkd3d_get_vk_queue_family_index)(ID3D12CommandQueue *queue);
typedef VkQueue (*PFN_vkd3d_acquire_vk_queue)(ID3D12CommandQueue *queue);
typedef void (*PFN_vkd3d_release_vk_queue)(ID3D12CommandQueue *queue);

typedef HRESULT (*PFN_vkd3d_create_image_resource)(ID3D12Device *device,
        const struct vkd3d_image_resource_create_info *create_info, ID3D12Resource **resource);
typedef ULONG (*PFN_vkd3d_resource_decref)(ID3D12Resource *resource);
typedef ULONG (*PFN_vkd3d_resource_incref)(ID3D12Resource *resource);

typedef HRESULT (*PFN_vkd3d_serialize_root_signature)(const D3D12_ROOT_SIGNATURE_DESC *desc,
        D3D_ROOT_SIGNATURE_VERSION version, ID3DBlob **blob, ID3DBlob **error_blob);
typedef HRESULT (*PFN_vkd3d_create_root_signature_deserializer)(const void *data, SIZE_T data_size,
        REFIID iid, void **deserializer);

typedef VkFormat (*PFN_vkd3d_get_vk_format)(DXGI_FORMAT format);

/* 1.1 */
typedef DXGI_FORMAT (*PFN_vkd3d_get_dxgi_format)(VkFormat format);

/* 1.2 */
typedef HRESULT (*PFN_vkd3d_serialize_versioned_root_signature)(const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *desc,
        ID3DBlob **blob, ID3DBlob **error_blob);
typedef HRESULT (*PFN_vkd3d_create_versioned_root_signature_deserializer)(const void *data, SIZE_T data_size,
        REFIID iid, void **deserializer);

/** Type of vkd3d_set_log_callback(). \since 1.4 */
typedef void (*PFN_vkd3d_set_log_callback)(PFN_vkd3d_log callback);

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif  /* __VKD3D_H */
