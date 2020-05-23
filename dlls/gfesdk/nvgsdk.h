#include <windef.h>

typedef struct _NVGSDK_HANDLE NVGSDK_HANDLE;

typedef struct _NVGSDK_CreateInputParams NVGSDK_CreateInputParams;

typedef struct _NVGSDK_CreateResponse NVGSDK_CreateResponse;

typedef struct _NVGSDK_RequestPermissionsParams NVGSDK_RequestPermissionsParams;

typedef struct _NVGSDK_Language NVGSDK_Language;

typedef enum _NVGSDK_LogLevel
{
  NVGSDK_LOG_NONE = 0,
} NVGSDK_LogLevel;

typedef enum _NVGSDK_RetCode
{
    NVGSDK_SUCCESS = 0,
    NVGSDK_ERR_INVALID_PARAMETER = -1005,
    NVGSDK_ERR_NOT_SET = -1006,
    NVGSDK_ERR_LOAD_LIBRARY = -1022,
} NVGSDK_RetCode;

typedef void (CALLBACK *NVGSDK_LoggingCallback)(NVGSDK_LogLevel lvl, char const *msg);

typedef void(CALLBACK *NVGSDK_EmptyCallback)(NVGSDK_RetCode ret, void *context);

typedef void(CALLBACK *NVGSDK_GetUILanguageCallback)(NVGSDK_RetCode ret, NVGSDK_Language const *lang, void *context);
