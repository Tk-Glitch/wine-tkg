#include <stdarg.h>

#define COBJMACROS
#include "windef.h"
#include "winbase.h"
#include "winstring.h"
#include "wine/debug.h"
#include "activation.h"
#include "objbase.h"
#include "initguid.h"

WINE_DEFAULT_DEBUG_CHANNEL(network);

static const char *debugstr_hstring(HSTRING hstr)
{
    const WCHAR *str;
    UINT32 len;
    if (hstr && !((ULONG_PTR)hstr >> 16)) return "(invalid)";
    str = WindowsGetStringRawBuffer(hstr, &len);
    return wine_dbgstr_wn(str, len);
}

DEFINE_GUID(IID_INetworkInformationStatics,0x5074f851,0x950d,0x4165,0x9c,0x15,0x36,0x56,0x19,0x48,0x1e,0xea);

typedef struct EventRegistrationToken
{
    __int64 value;
} EventRegistrationToken;

typedef struct IVectorView IVectorView;

typedef struct IVectorViewVtbl
{
    /*** IUnknown methods ***/
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(
        IVectorView *This,
        REFIID riid,
        void **ppvObject);

    ULONG (STDMETHODCALLTYPE *AddRef)(
        IVectorView *This);

    ULONG (STDMETHODCALLTYPE *Release)(
        IVectorView *This);

    /*** IInspectable methods ***/
    HRESULT (STDMETHODCALLTYPE *GetIids)(
        IVectorView *This,
        ULONG *iidCount,
        IID **iids);

    HRESULT (STDMETHODCALLTYPE *GetRuntimeClassName)(
        IVectorView *This,
        HSTRING *className);

    HRESULT (STDMETHODCALLTYPE *GetTrustLevel)(
        IVectorView *This,
        TrustLevel *trustLevel);

    /*** IVectorView<T> methods ***/
    HRESULT (STDMETHODCALLTYPE *GetAt)(
        IVectorView *This,
        ULONG index,
        /* T */ void *out_value);

    HRESULT (STDMETHODCALLTYPE *get_Size)(
        IVectorView *This,
        ULONG *out_value);

    HRESULT (STDMETHODCALLTYPE *IndexOf)(
        IVectorView *This,
        /* T */ void *value,
        ULONG *index,
        BOOLEAN *out_value);

    HRESULT (STDMETHODCALLTYPE *GetMany)(
        IVectorView *This,
        ULONG start_index,
        /* T[] */ void **items,
        UINT *out_value);
} IVectorViewVtbl;

struct IVectorView
{
    CONST_VTBL IVectorViewVtbl* lpVtbl;
};

typedef struct INetworkInformationStatics INetworkInformationStatics;

typedef struct INetworkInformationStaticsVtbl
{
    BEGIN_INTERFACE

    /*** IUnknown methods ***/
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(
        INetworkInformationStatics *This,
        REFIID riid,
        void **ppvObject);

    ULONG (STDMETHODCALLTYPE *AddRef)(
        INetworkInformationStatics *This);

    ULONG (STDMETHODCALLTYPE *Release)(
        INetworkInformationStatics *This);

    /*** IInspectable methods ***/
    HRESULT (STDMETHODCALLTYPE *GetIids)(
        INetworkInformationStatics *This,
        ULONG *iidCount,
        IID **iids);

    HRESULT (STDMETHODCALLTYPE *GetRuntimeClassName)(
        INetworkInformationStatics *This,
        HSTRING *className);

    HRESULT (STDMETHODCALLTYPE *GetTrustLevel)(
        INetworkInformationStatics *This,
        TrustLevel *trustLevel);

    /*** INetworkInformationStatics methods ***/
    HRESULT (STDMETHODCALLTYPE *eventadd_NetworkStatusChanged)(
        INetworkInformationStatics *This,
        /* Windows.Foundation.EventHandler<Windows.Networking.Connectivity.NetworkInformation*> */
        void *value,
        EventRegistrationToken* token);

    HRESULT (STDMETHODCALLTYPE *eventremove_NetworkStatusChanged)(
        INetworkInformationStatics *This,
        EventRegistrationToken token);

    END_INTERFACE
} INetworkInformationStaticsVtbl;

struct INetworkInformationStatics
{
    CONST_VTBL INetworkInformationStaticsVtbl* lpVtbl;
};

struct windows_networking_connectivity
{
    IActivationFactory IActivationFactory_iface;
    INetworkInformationStatics INetworkInformationStatics_iface;
    IVectorView IVectorView_iface;
    LONG refcount;
};

static inline struct windows_networking_connectivity *impl_from_IActivationFactory(IActivationFactory *iface)
{
    return CONTAINING_RECORD(iface, struct windows_networking_connectivity, IActivationFactory_iface);
}

static inline struct windows_networking_connectivity *impl_from_INetworkInformationStatics(INetworkInformationStatics *iface)
{
    return CONTAINING_RECORD(iface, struct windows_networking_connectivity, INetworkInformationStatics_iface);
}

static inline struct windows_networking_connectivity *impl_from_IVectorView(IVectorView *iface)
{
    return CONTAINING_RECORD(iface, struct windows_networking_connectivity, IVectorView_iface);
}

static HRESULT STDMETHODCALLTYPE vector_view_QueryInterface(
        IVectorView *iface, REFIID iid, void **object)
{
    TRACE("iface %p, iid %s, object %p stub!\n", iface, debugstr_guid(iid), object);
    IUnknown_AddRef(iface);
    *object = iface;
    return S_OK;
}

static ULONG STDMETHODCALLTYPE vector_view_AddRef(
        IVectorView *iface)
{
    struct windows_networking_connectivity *impl = impl_from_IVectorView(iface);
    ULONG rc = InterlockedIncrement(&impl->refcount);
    TRACE("%p increasing refcount to %u.\n", impl, rc);
    return rc;
}

static ULONG STDMETHODCALLTYPE vector_view_Release(
        IVectorView *iface)
{
    struct windows_networking_connectivity *impl = impl_from_IVectorView(iface);
    ULONG rc = InterlockedDecrement(&impl->refcount);
    TRACE("%p decreasing refcount to %u.\n", impl, rc);
    return rc;
}

static HRESULT STDMETHODCALLTYPE vector_view_GetIids(
        IVectorView *iface, ULONG *iid_count, IID **iids)
{
    FIXME("iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE vector_view_GetRuntimeClassName(
        IVectorView *iface, HSTRING *class_name)
{
    FIXME("iface %p, class_name %p stub!\n", iface, class_name);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE vector_view_GetTrustLevel(
        IVectorView *iface, TrustLevel *trust_level)
{
    FIXME("iface %p, trust_level %p stub!\n", iface, trust_level);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE vector_view_GetAt(
    IVectorView *iface, ULONG index, void *out_value)
{
    FIXME("iface %p, index %#x, out_value %p stub!\n", iface, index, out_value);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE vector_view_get_Size(
    IVectorView *iface, ULONG *out_value)
{
    FIXME("iface %p, out_value %p stub!\n", iface, out_value);
    *out_value = 0;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE vector_view_IndexOf(
    IVectorView *iface, void *value, ULONG *index, BOOLEAN *out_value)
{
    FIXME("iface %p, value %p, index %p, out_value %p stub!\n", iface, value, index, out_value);
    *out_value = FALSE;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE vector_view_GetMany(
    IVectorView *iface, ULONG start_index, void **items, UINT *out_value)
{
    FIXME("iface %p, start_index %#x, items %p, out_value %p stub!\n", iface, start_index, items, out_value);
    *out_value = 0;
    return S_OK;
}

static const struct IVectorViewVtbl vector_view_vtbl =
{
    vector_view_QueryInterface,
    vector_view_AddRef,
    vector_view_Release,
    /* IInspectable methods */
    vector_view_GetIids,
    vector_view_GetRuntimeClassName,
    vector_view_GetTrustLevel,
    /*** IVectorView<T> methods ***/
    vector_view_GetAt,
    vector_view_get_Size,
    vector_view_IndexOf,
    vector_view_GetMany,
};

static HRESULT STDMETHODCALLTYPE network_information_statics_QueryInterface(
        INetworkInformationStatics *iface, REFIID iid, void **object)
{
    TRACE("iface %p, iid %s, object %p stub!\n", iface, debugstr_guid(iid), object);

    if (IsEqualGUID(iid, &IID_IAgileObject))
    {
        IUnknown_AddRef(iface);
        *object = iface;
        return S_OK;
    }

    WARN("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));
    *object = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE network_information_statics_AddRef(
        INetworkInformationStatics *iface)
{
    struct windows_networking_connectivity *impl = impl_from_INetworkInformationStatics(iface);
    ULONG rc = InterlockedIncrement(&impl->refcount);
    TRACE("%p increasing refcount to %u.\n", impl, rc);
    return rc;
}

static ULONG STDMETHODCALLTYPE network_information_statics_Release(
        INetworkInformationStatics *iface)
{
    struct windows_networking_connectivity *impl = impl_from_INetworkInformationStatics(iface);
    ULONG rc = InterlockedDecrement(&impl->refcount);
    TRACE("%p decreasing refcount to %u.\n", impl, rc);
    return rc;
}

static HRESULT STDMETHODCALLTYPE network_information_statics_GetIids(
        INetworkInformationStatics *iface, ULONG *iid_count, IID **iids)
{
    FIXME("iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE network_information_statics_GetRuntimeClassName(
        INetworkInformationStatics *iface, HSTRING *class_name)
{
    FIXME("iface %p, class_name %p stub!\n", iface, class_name);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE network_information_statics_GetTrustLevel(
        INetworkInformationStatics *iface, TrustLevel *trust_level)
{
    FIXME("iface %p, trust_level %p stub!\n", iface, trust_level);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE network_information_statics_eventadd_NetworkStatusChanged(
    INetworkInformationStatics *iface, void *value, EventRegistrationToken* token)
{
    FIXME("iface %p, value %p, token %p stub!\n", iface, value, token);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE network_information_statics_eventremove_NetworkStatusChanged(
    INetworkInformationStatics *iface, EventRegistrationToken token)
{
    FIXME("iface %p, token %#I64x stub!\n", iface, token.value);
    return S_OK;
}

static const struct INetworkInformationStaticsVtbl network_information_statics_vtbl =
{
    network_information_statics_QueryInterface,
    network_information_statics_AddRef,
    network_information_statics_Release,
    /* IInspectable methods */
    network_information_statics_GetIids,
    network_information_statics_GetRuntimeClassName,
    network_information_statics_GetTrustLevel,
    /* INetworkInformationStatics methods */
    network_information_statics_eventadd_NetworkStatusChanged,
    network_information_statics_eventremove_NetworkStatusChanged,
};

static HRESULT STDMETHODCALLTYPE windows_networking_connectivity_QueryInterface(
        IActivationFactory *iface, REFIID iid, void **object)
{
    struct windows_networking_connectivity *impl = impl_from_IActivationFactory(iface);
    TRACE("iface %p, iid %s, object %p stub!\n", iface, debugstr_guid(iid), object);

    if (IsEqualGUID(iid, &IID_INetworkInformationStatics))
    {
        IUnknown_AddRef(iface);
        *object = &impl->INetworkInformationStatics_iface;
        return S_OK;
    }

    WARN("%s not implemented, returning E_NOINTERFACE.\n", debugstr_guid(iid));
    *object = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE windows_networking_connectivity_AddRef(
        IActivationFactory *iface)
{
    struct windows_networking_connectivity *impl = impl_from_IActivationFactory(iface);
    ULONG rc = InterlockedIncrement(&impl->refcount);
    TRACE("%p increasing refcount to %u.\n", impl, rc);
    return rc;
}

static ULONG STDMETHODCALLTYPE windows_networking_connectivity_Release(
        IActivationFactory *iface)
{
    struct windows_networking_connectivity *impl = impl_from_IActivationFactory(iface);
    ULONG rc = InterlockedDecrement(&impl->refcount);
    TRACE("%p decreasing refcount to %u.\n", impl, rc);
    return rc;
}

static HRESULT STDMETHODCALLTYPE windows_networking_connectivity_GetIids(
        IActivationFactory *iface, ULONG *iid_count, IID **iids)
{
    FIXME("iface %p, iid_count %p, iids %p stub!\n", iface, iid_count, iids);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE windows_networking_connectivity_GetRuntimeClassName(
        IActivationFactory *iface, HSTRING *class_name)
{
    FIXME("iface %p, class_name %p stub!\n", iface, class_name);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE windows_networking_connectivity_GetTrustLevel(
        IActivationFactory *iface, TrustLevel *trust_level)
{
    FIXME("iface %p, trust_level %p stub!\n", iface, trust_level);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE windows_networking_connectivity_ActivateInstance(
        IActivationFactory *iface, IInspectable **instance)
{
    FIXME("iface %p, instance %p stub!\n", iface, instance);
    return E_NOTIMPL;
}

static const struct IActivationFactoryVtbl activation_factory_vtbl =
{
    windows_networking_connectivity_QueryInterface,
    windows_networking_connectivity_AddRef,
    windows_networking_connectivity_Release,
    /* IInspectable methods */
    windows_networking_connectivity_GetIids,
    windows_networking_connectivity_GetRuntimeClassName,
    windows_networking_connectivity_GetTrustLevel,
    /* IActivationFactory methods */
    windows_networking_connectivity_ActivateInstance,
};

static struct windows_networking_connectivity windows_networking_connectivity =
{
    {&activation_factory_vtbl},
    {&network_information_statics_vtbl},
    {&vector_view_vtbl},
    0
};

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void *reserved)
{
    TRACE("(%p, %u, %p)\n", instance, reason, reserved);

    switch (reason)
    {
    case DLL_WINE_PREATTACH:
        return FALSE;   /* prefer native version */
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(instance);
        break;
    }

    return TRUE;
}

HRESULT WINAPI DllCanUnloadNow(void)
{
    return S_FALSE;
}

HRESULT WINAPI DllGetClassObject(REFCLSID clsid, REFIID riid, LPVOID *object)
{
    FIXME("clsid %s, riid %s, object %p stub!\n", debugstr_guid(clsid), debugstr_guid(riid), object);
    return CLASS_E_CLASSNOTAVAILABLE;
}

HRESULT WINAPI DllGetActivationFactory(HSTRING classid, IActivationFactory **factory)
{
    TRACE("classid %s, factory %p.\n", debugstr_hstring(classid), factory);
    *factory = &windows_networking_connectivity.IActivationFactory_iface;
    IUnknown_AddRef(*factory);
    return S_OK;
}