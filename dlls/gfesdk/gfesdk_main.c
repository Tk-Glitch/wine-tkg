#include <windef.h>

#include "wine/debug.h"

#include "nvgsdk.h"

WINE_DEFAULT_DEBUG_CHANNEL(gfesdk);

NVGSDK_RetCode CDECL NVGSDK_Create(NVGSDK_HANDLE **handle, NVGSDK_CreateInputParams const *params, NVGSDK_CreateResponse *response)
{
    FIXME("stub(%p %p %p %p)\n", handle, params, response);

    /* SDK specifies "On fatal error, this will be NULL", which I assume means we clean it? */
    if (response)
    {
        //memset(response, 0, sizeof(*response));
    }

    return NVGSDK_ERR_LOAD_LIBRARY;
}

NVGSDK_RetCode CDECL NVGSDK_Release(NVGSDK_HANDLE *handle)
{
    FIXME("stub(%p)\n", handle);

    return NVGSDK_ERR_INVALID_PARAMETER;
}

NVGSDK_RetCode CDECL NVGSDK_Poll(NVGSDK_HANDLE *handle)
{
    FIXME("stub(%p)\n", handle);

    /* SDK doesn't document failure of this function */
    return NVGSDK_ERR_INVALID_PARAMETER;
}

NVGSDK_RetCode CDECL NVGSDK_SetLogLevel(NVGSDK_LogLevel lvl)
{
    FIXME("stub(%u)\n", lvl);

    return NVGSDK_SUCCESS;
}

NVGSDK_RetCode CDECL NVGSDK_AttachLogListener(NVGSDK_LoggingCallback callback)
{
    FIXME("stub(%p)\n");

    return NVGSDK_SUCCESS;
}

NVGSDK_RetCode CDECL NVGSDK_SetListenerLogLevel(NVGSDK_LogLevel lvl)
{
    FIXME("stub(%u)\n");

    return NVGSDK_SUCCESS;
}

void CDECL NVGSDK_RequestPermissionsAsync(NVGSDK_HANDLE *handle, NVGSDK_RequestPermissionsParams const *params, NVGSDK_EmptyCallback callback, void *context)
{
    FIXME("stub(%p %p %p %p)\n", handle, params, callback, context);

    callback(NVGSDK_SUCCESS, context);
}

void CDECL NVGSDK_GetUILanguageAsync(NVGSDK_HANDLE *handle, NVGSDK_GetUILanguageCallback callback, void *context)
{
    FIXME("stub(%p %p %p)\n", handle, callback, context);

    callback(NVGSDK_ERR_NOT_SET, NULL, context);
}
