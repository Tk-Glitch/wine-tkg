/*
 * OpenCL.dll proxy for native OpenCL implementation.
 *
 * Copyright 2010 Peter Urbanec
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

#include "opencl_private.h"
#include "opencl_types.h"
#include "unixlib.h"

WINE_DEFAULT_DEBUG_CHANNEL(opencl);

const struct opencl_funcs *opencl_funcs = NULL;

static cl_int filter_extensions( const char *unix_exts, SIZE_T size, char *win_exts, size_t *ret_size )
{
    char *p = win_exts;
    const char *ext;
    SIZE_T win_size;

    TRACE( "got host extension string %s\n", debugstr_a( unix_exts ) );

    ext = unix_exts;
    win_size = 0;
    while (*ext)
    {
        const char *end = strchr( ext, ' ' );

        if (!end) end = ext + strlen( ext );

        if (extension_is_supported( ext, end - ext ))
            win_size += strlen( ext ) + 1;

        if (*end == ' ') ++end;
        ext = end;
    }

    if (ret_size) *ret_size = win_size;
    if (!win_exts) return CL_SUCCESS;
    if (size < win_size) return CL_INVALID_VALUE;

    win_exts[0] = 0;
    ext = unix_exts;
    while (*ext)
    {
        const char *end = strchr( ext, ' ' );
        size_t len;

        if (!end) end = ext + strlen( ext );
        len = end - ext;

        if (extension_is_supported( ext, len ))
        {
            if (p != win_exts) *p++ = ' ';
            memcpy( p, ext, len );
            p += len;
        }

        if (*end == ' ') ++end;
        ext = end;
    }
    *p = 0;

    TRACE( "returning extension string %s\n", debugstr_a(win_exts) );

    return CL_SUCCESS;
}

cl_int WINAPI clGetPlatformInfo( cl_platform_id platform, cl_platform_info name,
                                 SIZE_T size, void *value, size_t *ret_size )
{
    cl_int ret;

    TRACE( "(%p, %#x, %ld, %p, %p)\n", platform, name, size, value, ret_size );

    if (name == CL_PLATFORM_EXTENSIONS)
    {
        size_t unix_size;
        char *unix_exts;

        ret = opencl_funcs->pclGetPlatformInfo( platform, name, 0, NULL, &unix_size );
        if (ret != CL_SUCCESS)
            return ret;

        if (!(unix_exts = malloc( unix_size )))
            return CL_OUT_OF_HOST_MEMORY;
        ret = opencl_funcs->pclGetPlatformInfo( platform, name, unix_size, unix_exts, NULL );
        if (ret != CL_SUCCESS)
        {
            free( unix_exts );
            return ret;
        }

        ret = filter_extensions( unix_exts, size, value, ret_size );

        free( unix_exts );
    }
    else
    {
        ret = opencl_funcs->pclGetPlatformInfo( platform, name, size, value, ret_size );
    }

    return ret;
}


cl_int WINAPI clGetDeviceInfo( cl_device_id device, cl_device_info name,
                               SIZE_T size, void *value, size_t *ret_size )
{
    cl_int ret;

    TRACE( "(%p, %#x, %ld, %p, %p)\n", device, name, size, value, ret_size );

    if (name == CL_DEVICE_EXTENSIONS)
    {
        size_t unix_size;
        char *unix_exts;

        ret = opencl_funcs->pclGetDeviceInfo( device, name, 0, NULL, &unix_size );
        if (ret != CL_SUCCESS)
            return ret;

        if (!(unix_exts = malloc( unix_size )))
            return CL_OUT_OF_HOST_MEMORY;
        ret = opencl_funcs->pclGetDeviceInfo( device, name, unix_size, unix_exts, NULL );
        if (ret != CL_SUCCESS)
        {
            free( unix_exts );
            return ret;
        }

        ret = filter_extensions( unix_exts, size, value, ret_size );

        free( unix_exts );
    }
    else
    {
        ret = opencl_funcs->pclGetDeviceInfo( device, name, size, value, ret_size );
    }

    /* Filter out the CL_EXEC_NATIVE_KERNEL flag */
    if (name == CL_DEVICE_EXECUTION_CAPABILITIES)
    {
        cl_device_exec_capabilities *caps = value;
        *caps &= ~CL_EXEC_NATIVE_KERNEL;
    }

    return ret;
}


void * WINAPI clGetExtensionFunctionAddress( const char *func_name )
{
    void * ret = 0;
    TRACE("(%s)\n",func_name);
#if 0
    ret = clGetExtensionFunctionAddress(func_name);
#else
    FIXME("extensions not implemented\n");
#endif
    TRACE("(%s)=%p\n",func_name, ret);
    return ret;
}


cl_int WINAPI clSetCommandQueueProperty( cl_command_queue command_queue, cl_command_queue_properties properties,
        cl_bool enable, cl_command_queue_properties *old_properties )
{
    FIXME( "(%p, %s, %u, %p) deprecated\n", command_queue, wine_dbgstr_longlong(properties), enable, old_properties );
    return CL_INVALID_QUEUE_PROPERTIES;
}


void * WINAPI clGetExtensionFunctionAddressForPlatform( cl_platform_id platform, const char *func_name )
{
    FIXME( "(%p, %s) stub!\n", platform, debugstr_a(func_name) );
    return NULL;
}


BOOL WINAPI DllMain( HINSTANCE instance, DWORD reason, void *reserved )
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls( instance );
        return !__wine_init_unix_lib( instance, reason, NULL, &opencl_funcs );
    }
    return TRUE;
}
