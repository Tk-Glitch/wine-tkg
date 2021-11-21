/*
 * Copyright 2009 Maarten Lankhorst
 * Copyright 2011 Andrew Eikum for CodeWeavers
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

#include <stdarg.h>

#define COBJMACROS
#include "windef.h"
#include "winbase.h"
#include "wingdi.h"

#include "ole2.h"
#include "olectl.h"
#include "rpcproxy.h"
#include "propsys.h"
#include "propkeydef.h"
#include "mmdeviceapi.h"
#include "mmsystem.h"
#include "dsound.h"
#include "audioclient.h"
#include "endpointvolume.h"
#include "audiopolicy.h"
#include "devpkey.h"
#include "winreg.h"
#include "spatialaudioclient.h"

#include "mmdevapi.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(mmdevapi);

DriverFuncs drvs;

const WCHAR drv_keyW[] = L"Software\\Wine\\Drivers";

static const char *get_priority_string(int prio)
{
    switch(prio){
    case Priority_Unavailable:
        return "Unavailable";
    case Priority_Low:
        return "Low";
    case Priority_Neutral:
        return "Neutral";
    case Priority_Preferred:
        return "Preferred";
    }
    return "Invalid";
}

static BOOL load_driver(const WCHAR *name, DriverFuncs *driver)
{
    WCHAR driver_module[264];

    lstrcpyW(driver_module, L"wine");
    lstrcatW(driver_module, name);
    lstrcatW(driver_module, L".drv");

    TRACE("Attempting to load %s\n", wine_dbgstr_w(driver_module));

    driver->module = LoadLibraryW(driver_module);
    if(!driver->module){
        TRACE("Unable to load %s: %u\n", wine_dbgstr_w(driver_module),
                GetLastError());
        return FALSE;
    }

#define LDFC(n) do { driver->p##n = (void*)GetProcAddress(driver->module, #n);\
        if(!driver->p##n) { FreeLibrary(driver->module); return FALSE; } } while(0)
    LDFC(GetPriority);
    LDFC(GetEndpointIDs);
    LDFC(GetAudioEndpoint);
    LDFC(GetAudioSessionManager);
#undef LDFC

    /* optional - do not fail if not found */
    driver->pGetPropValue = (void*)GetProcAddress(driver->module, "GetPropValue");

    driver->priority = driver->pGetPriority();
    lstrcpyW(driver->module_name, driver_module);

    TRACE("Successfully loaded %s with priority %s\n",
            wine_dbgstr_w(driver_module), get_priority_string(driver->priority));

    return TRUE;
}

static BOOL WINAPI init_driver(INIT_ONCE *once, void *param, void **context)
{
    static WCHAR default_list[] = L"pulse,alsa,oss,coreaudio,android";
    DriverFuncs driver;
    HKEY key;
    WCHAR reg_list[256], *p, *next, *driver_list = default_list;

    if(RegOpenKeyW(HKEY_CURRENT_USER, drv_keyW, &key) == ERROR_SUCCESS){
        DWORD size = sizeof(reg_list);

        if(RegQueryValueExW(key, L"Audio", 0, NULL, (BYTE*)reg_list, &size) == ERROR_SUCCESS){
            if(reg_list[0] == '\0'){
                TRACE("User explicitly chose no driver\n");
                RegCloseKey(key);
                return TRUE;
            }

            driver_list = reg_list;
        }

        RegCloseKey(key);
    }

    TRACE("Loading driver list %s\n", wine_dbgstr_w(driver_list));
    for(next = p = driver_list; next; p = next + 1){
        next = wcschr(p, ',');
        if(next)
            *next = '\0';

        driver.priority = Priority_Unavailable;
        if(load_driver(p, &driver)){
            if(driver.priority == Priority_Unavailable)
                FreeLibrary(driver.module);
            else if(!drvs.module || driver.priority > drvs.priority){
                TRACE("Selecting driver %s with priority %s\n",
                        wine_dbgstr_w(p), get_priority_string(driver.priority));
                if(drvs.module)
                    FreeLibrary(drvs.module);
                drvs = driver;
            }else
                FreeLibrary(driver.module);
        }else
            TRACE("Failed to load driver %s\n", wine_dbgstr_w(p));

        if(next)
            *next = ',';
    }

    if (drvs.module != 0){
        load_devices_from_reg();
        load_driver_devices(eRender);
        load_driver_devices(eCapture);
    }

    return drvs.module != 0;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    TRACE("(0x%p, %d, %p)\n", hinstDLL, fdwReason, lpvReserved);

    switch (fdwReason)
    {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hinstDLL);
            break;
        case DLL_PROCESS_DETACH:
            if(lpvReserved)
                break;
            MMDevEnum_Free();
            break;
    }

    return TRUE;
}

typedef HRESULT (*FnCreateInstance)(REFIID riid, LPVOID *ppobj);

typedef struct {
    IClassFactory IClassFactory_iface;
    REFCLSID rclsid;
    FnCreateInstance pfnCreateInstance;
} IClassFactoryImpl;

static inline IClassFactoryImpl *impl_from_IClassFactory(IClassFactory *iface)
{
    return CONTAINING_RECORD(iface, IClassFactoryImpl, IClassFactory_iface);
}

static HRESULT WINAPI
MMCF_QueryInterface(IClassFactory *iface, REFIID riid, void **ppobj)
{
    IClassFactoryImpl *This = impl_from_IClassFactory(iface);
    TRACE("(%p, %s, %p)\n", This, debugstr_guid(riid), ppobj);
    if (ppobj == NULL)
        return E_POINTER;
    if (IsEqualIID(riid, &IID_IUnknown) ||
        IsEqualIID(riid, &IID_IClassFactory))
    {
        *ppobj = iface;
        IClassFactory_AddRef(iface);
        return S_OK;
    }
    *ppobj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI MMCF_AddRef(LPCLASSFACTORY iface)
{
    return 2;
}

static ULONG WINAPI MMCF_Release(LPCLASSFACTORY iface)
{
    /* static class, won't be freed */
    return 1;
}

static HRESULT WINAPI MMCF_CreateInstance(
    LPCLASSFACTORY iface,
    LPUNKNOWN pOuter,
    REFIID riid,
    LPVOID *ppobj)
{
    IClassFactoryImpl *This = impl_from_IClassFactory(iface);
    TRACE("(%p, %p, %s, %p)\n", This, pOuter, debugstr_guid(riid), ppobj);

    if (pOuter)
        return CLASS_E_NOAGGREGATION;

    if (ppobj == NULL) {
        WARN("invalid parameter\n");
        return E_POINTER;
    }
    *ppobj = NULL;
    return This->pfnCreateInstance(riid, ppobj);
}

static HRESULT WINAPI MMCF_LockServer(LPCLASSFACTORY iface, BOOL dolock)
{
    IClassFactoryImpl *This = impl_from_IClassFactory(iface);
    FIXME("(%p, %d) stub!\n", This, dolock);
    return S_OK;
}

static const IClassFactoryVtbl MMCF_Vtbl = {
    MMCF_QueryInterface,
    MMCF_AddRef,
    MMCF_Release,
    MMCF_CreateInstance,
    MMCF_LockServer
};

static IClassFactoryImpl MMDEVAPI_CF[] = {
    { { &MMCF_Vtbl }, &CLSID_MMDeviceEnumerator, (FnCreateInstance)MMDevEnum_Create }
};

HRESULT WINAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppv)
{
    static INIT_ONCE init_once = INIT_ONCE_STATIC_INIT;
    unsigned int i = 0;
    TRACE("(%s, %s, %p)\n", debugstr_guid(rclsid), debugstr_guid(riid), ppv);

    if(!InitOnceExecuteOnce(&init_once, init_driver, NULL, NULL)) {
        ERR("Driver initialization failed\n");
        return E_FAIL;
    }

    if (ppv == NULL) {
        WARN("invalid parameter\n");
        return E_INVALIDARG;
    }

    *ppv = NULL;

    if (!IsEqualIID(riid, &IID_IClassFactory) &&
        !IsEqualIID(riid, &IID_IUnknown)) {
        WARN("no interface for %s\n", debugstr_guid(riid));
        return E_NOINTERFACE;
    }

    for (i = 0; i < ARRAY_SIZE(MMDEVAPI_CF); ++i)
    {
        if (IsEqualGUID(rclsid, MMDEVAPI_CF[i].rclsid)) {
            IClassFactory_AddRef(&MMDEVAPI_CF[i].IClassFactory_iface);
            *ppv = &MMDEVAPI_CF[i];
            return S_OK;
        }
    }

    WARN("(%s, %s, %p): no class found.\n", debugstr_guid(rclsid),
         debugstr_guid(riid), ppv);
    return CLASS_E_CLASSNOTAVAILABLE;
}

struct activate_async_op {
    IActivateAudioInterfaceAsyncOperation IActivateAudioInterfaceAsyncOperation_iface;
    LONG ref;

    IActivateAudioInterfaceCompletionHandler *callback;
    HRESULT result_hr;
    IUnknown *result_iface;
};

static struct activate_async_op *impl_from_IActivateAudioInterfaceAsyncOperation(IActivateAudioInterfaceAsyncOperation *iface)
{
    return CONTAINING_RECORD(iface, struct activate_async_op, IActivateAudioInterfaceAsyncOperation_iface);
}

static HRESULT WINAPI activate_async_op_QueryInterface(IActivateAudioInterfaceAsyncOperation *iface,
        REFIID riid, void **ppv)
{
    struct activate_async_op *This = impl_from_IActivateAudioInterfaceAsyncOperation(iface);

    TRACE("(%p)->(%s, %p)\n", This, debugstr_guid(riid), ppv);

    if (!ppv)
        return E_POINTER;

    if (IsEqualIID(riid, &IID_IUnknown) ||
            IsEqualIID(riid, &IID_IActivateAudioInterfaceAsyncOperation)) {
        *ppv = &This->IActivateAudioInterfaceAsyncOperation_iface;
    } else {
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown*)*ppv);
    return S_OK;
}

static ULONG WINAPI activate_async_op_AddRef(IActivateAudioInterfaceAsyncOperation *iface)
{
    struct activate_async_op *This = impl_from_IActivateAudioInterfaceAsyncOperation(iface);
    LONG ref = InterlockedIncrement(&This->ref);
    TRACE("(%p) refcount now %i\n", This, ref);
    return ref;
}

static ULONG WINAPI activate_async_op_Release(IActivateAudioInterfaceAsyncOperation *iface)
{
    struct activate_async_op *This = impl_from_IActivateAudioInterfaceAsyncOperation(iface);
    LONG ref = InterlockedDecrement(&This->ref);
    TRACE("(%p) refcount now %i\n", This, ref);
    if (!ref) {
        if(This->result_iface)
            IUnknown_Release(This->result_iface);
        IActivateAudioInterfaceCompletionHandler_Release(This->callback);
        HeapFree(GetProcessHeap(), 0, This);
    }
    return ref;
}

static HRESULT WINAPI activate_async_op_GetActivateResult(IActivateAudioInterfaceAsyncOperation *iface,
        HRESULT *result_hr, IUnknown **result_iface)
{
    struct activate_async_op *This = impl_from_IActivateAudioInterfaceAsyncOperation(iface);

    TRACE("(%p)->(%p, %p)\n", This, result_hr, result_iface);

    *result_hr = This->result_hr;

    if(This->result_hr == S_OK){
        *result_iface = This->result_iface;
        IUnknown_AddRef(*result_iface);
    }

    return S_OK;
}

static IActivateAudioInterfaceAsyncOperationVtbl IActivateAudioInterfaceAsyncOperation_vtbl = {
    activate_async_op_QueryInterface,
    activate_async_op_AddRef,
    activate_async_op_Release,
    activate_async_op_GetActivateResult,
};

static DWORD WINAPI activate_async_threadproc(void *user)
{
    struct activate_async_op *op = user;

    IActivateAudioInterfaceCompletionHandler_ActivateCompleted(op->callback, &op->IActivateAudioInterfaceAsyncOperation_iface);

    IActivateAudioInterfaceAsyncOperation_Release(&op->IActivateAudioInterfaceAsyncOperation_iface);

    return 0;
}

static HRESULT get_mmdevice_by_activatepath(const WCHAR *path, IMMDevice **mmdev)
{
    IMMDeviceEnumerator *devenum;
    HRESULT hr;

    static const WCHAR DEVINTERFACE_AUDIO_RENDER_WSTR[] = L"{E6327CAD-DCEC-4949-AE8A-991E976A79D2}";
    static const WCHAR DEVINTERFACE_AUDIO_CAPTURE_WSTR[] = L"{2EEF81BE-33FA-4800-9670-1CD474972C3F}";
    static const WCHAR MMDEV_PATH_PREFIX[] = L"\\\\?\\SWD#MMDEVAPI#";

    hr = MMDevEnum_Create(&IID_IMMDeviceEnumerator, (void**)&devenum);
    if (FAILED(hr)) {
        WARN("Failed to create MMDeviceEnumerator: %08x\n", hr);
        return hr;
    }

    if (!lstrcmpiW(path, DEVINTERFACE_AUDIO_RENDER_WSTR)){
        hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(devenum, eRender, eMultimedia, mmdev);
    } else if (!lstrcmpiW(path, DEVINTERFACE_AUDIO_CAPTURE_WSTR)){
        hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(devenum, eCapture, eMultimedia, mmdev);
    } else if (!memcmp(path, MMDEV_PATH_PREFIX, sizeof(MMDEV_PATH_PREFIX) - sizeof(WCHAR))) {
        WCHAR device_id[56]; /* == strlen("{0.0.1.00000000}.{fd47d9cc-4218-4135-9ce2-0c195c87405b}") + 1 */

        lstrcpynW(device_id, path + (ARRAY_SIZE(MMDEV_PATH_PREFIX) - 1), ARRAY_SIZE(device_id));

        hr = IMMDeviceEnumerator_GetDevice(devenum, device_id, mmdev);
    } else {
        FIXME("Unrecognized device id format: %s\n", debugstr_w(path));
        hr = HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    }

    if (FAILED(hr)) {
        WARN("Failed to get requested device (%s): %08x\n", debugstr_w(path), hr);
        *mmdev = NULL;
        hr = HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    }

    IMMDeviceEnumerator_Release(devenum);

    return hr;
}

/***********************************************************************
 *		ActivateAudioInterfaceAsync (MMDEVAPI.17)
 */
HRESULT WINAPI ActivateAudioInterfaceAsync(const WCHAR *path, REFIID riid,
        PROPVARIANT *params, IActivateAudioInterfaceCompletionHandler *done_handler,
        IActivateAudioInterfaceAsyncOperation **op_out)
{
    struct activate_async_op *op;
    HANDLE ht;
    IMMDevice *mmdev;

    TRACE("(%s, %s, %p, %p, %p)\n", debugstr_w(path), debugstr_guid(riid),
            params, done_handler, op_out);

    op = HeapAlloc(GetProcessHeap(), 0, sizeof(*op));
    if (!op)
        return E_OUTOFMEMORY;

    op->ref = 2; /* returned ref and threadproc ref */
    op->IActivateAudioInterfaceAsyncOperation_iface.lpVtbl = &IActivateAudioInterfaceAsyncOperation_vtbl;
    op->callback = done_handler;
    IActivateAudioInterfaceCompletionHandler_AddRef(done_handler);

    op->result_hr = get_mmdevice_by_activatepath(path, &mmdev);
    if (SUCCEEDED(op->result_hr)) {
        op->result_hr = IMMDevice_Activate(mmdev, riid, CLSCTX_INPROC_SERVER, params, (void**)&op->result_iface);
        IMMDevice_Release(mmdev);
    }else
        op->result_iface = NULL;

    ht = CreateThread(NULL, 0, &activate_async_threadproc, op, 0, NULL);
    CloseHandle(ht);

    *op_out = &op->IActivateAudioInterfaceAsyncOperation_iface;

    return S_OK;
}
