/*
 * Copyright 2015 Jacek Caban for CodeWeavers
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

#define COBJMACROS

#include "windows.h"
#include "initguid.h"
#include "dispex.h"
#include "ole2.h"
#include "olectl.h"
#include "objsafe.h"
#include "activscp.h"
#include "rpcproxy.h"
#include "msscript.h"
DEFINE_GUID(GUID_NULL,0,0,0,0,0,0,0,0,0,0,0);

#include "wine/debug.h"
#include "wine/heap.h"
#include "wine/list.h"

WINE_DEFAULT_DEBUG_CHANNEL(msscript);

#ifdef _WIN64

#define IActiveScriptParse_Release IActiveScriptParse64_Release
#define IActiveScriptParse_InitNew IActiveScriptParse64_InitNew
#define IActiveScriptParse_ParseScriptText IActiveScriptParse64_ParseScriptText

#else

#define IActiveScriptParse_Release IActiveScriptParse32_Release
#define IActiveScriptParse_InitNew IActiveScriptParse32_InitNew
#define IActiveScriptParse_ParseScriptText IActiveScriptParse32_ParseScriptText

#endif

struct ScriptControl;
typedef struct ConnectionPoint ConnectionPoint;
typedef struct ScriptHost ScriptHost;

struct ConnectionPoint {
    IConnectionPoint IConnectionPoint_iface;
    ScriptControl *control;
    const IID *riid;
    ConnectionPoint *next;
};

struct named_item {
    struct list entry;
    BSTR name;
    IDispatch *disp;
};

typedef struct {
    IScriptModule IScriptModule_iface;
    LONG ref;

    BSTR name;
    ScriptHost *host;
    IDispatch *script_dispatch;
} ScriptModule;

struct ScriptHost {
    IActiveScriptSite IActiveScriptSite_iface;
    IActiveScriptSiteWindow IActiveScriptSiteWindow_iface;
    IServiceProvider IServiceProvider_iface;
    LONG ref;

    IActiveScript *script;
    IActiveScriptParse *parse;
    SCRIPTSTATE script_state;
    CLSID clsid;

    unsigned int module_count;
    struct list named_items;
};

struct ScriptControl {
    IScriptControl IScriptControl_iface;
    IPersistStreamInit IPersistStreamInit_iface;
    IOleObject IOleObject_iface;
    IOleControl IOleControl_iface;
    IQuickActivate IQuickActivate_iface;
    IViewObjectEx IViewObjectEx_iface;
    IPointerInactive IPointerInactive_iface;
    IConnectionPointContainer IConnectionPointContainer_iface;
    LONG ref;
    IOleClientSite *site;
    SIZEL extent;
    LONG timeout;
    VARIANT_BOOL allow_ui;
    VARIANT_BOOL use_safe_subset;

    /* connection points */
    ConnectionPoint *cp_list;
    ConnectionPoint cp_scsource;
    ConnectionPoint cp_propnotif;

    /* IViewObject sink */
    IAdviseSink *view_sink;
    DWORD view_sink_flags;

    /* modules */
    ScriptModule **modules;
    IScriptModuleCollection IScriptModuleCollection_iface;

    ScriptHost *host;
};

static HINSTANCE msscript_instance;

typedef enum tid_t {
    IScriptControl_tid,
    IScriptModuleCollection_tid,
    IScriptModule_tid,
    LAST_tid
} tid_t;

static ITypeLib *typelib;
static ITypeInfo *typeinfos[LAST_tid];

static REFIID tid_ids[] = {
    &IID_IScriptControl,
    &IID_IScriptModuleCollection,
    &IID_IScriptModule
};

static HRESULT load_typelib(void)
{
    HRESULT hres;
    ITypeLib *tl;

    hres = LoadRegTypeLib(&LIBID_MSScriptControl, 1, 0, LOCALE_SYSTEM_DEFAULT, &tl);
    if(FAILED(hres)) {
        ERR("LoadRegTypeLib failed: %08x\n", hres);
        return hres;
    }

    if(InterlockedCompareExchangePointer((void**)&typelib, tl, NULL))
        ITypeLib_Release(tl);
    return hres;
}

static HRESULT get_typeinfo(tid_t tid, ITypeInfo **typeinfo)
{
    HRESULT hres;

    if (!typelib)
        hres = load_typelib();
    if (!typelib)
        return hres;

    if(!typeinfos[tid]) {
        ITypeInfo *ti;

        hres = ITypeLib_GetTypeInfoOfGuid(typelib, tid_ids[tid], &ti);
        if(FAILED(hres)) {
            ERR("GetTypeInfoOfGuid(%s) failed: %08x\n", debugstr_guid(tid_ids[tid]), hres);
            return hres;
        }

        if(InterlockedCompareExchangePointer((void**)(typeinfos+tid), ti, NULL))
            ITypeInfo_Release(ti);
    }

    *typeinfo = typeinfos[tid];
    ITypeInfo_AddRef(typeinfos[tid]);
    return S_OK;
}

static void release_typelib(void)
{
    unsigned i;

    if(!typelib)
        return;

    for(i = 0; i < ARRAY_SIZE(typeinfos); i++)
        if(typeinfos[i])
            ITypeInfo_Release(typeinfos[i]);

    ITypeLib_Release(typelib);
}

static inline BOOL is_power_of_2(unsigned x)
{
    return !(x & (x - 1));
}

static void clear_named_items(ScriptHost *host)
{
    struct named_item *item, *item1;
    LIST_FOR_EACH_ENTRY_SAFE(item, item1, &host->named_items, struct named_item, entry) {
       list_remove(&item->entry);
       SysFreeString(item->name);
       IDispatch_Release(item->disp);
       heap_free(item);
    }
}

static struct named_item *host_get_named_item(ScriptHost *host, const WCHAR *nameW)
{
    struct named_item *item;

    LIST_FOR_EACH_ENTRY(item, &host->named_items, struct named_item, entry) {
        if (!wcscmp(item->name, nameW))
            return item;
    }

    return NULL;
}

static HRESULT get_script_dispatch(ScriptModule *module, IDispatch **disp)
{
    if (!module->script_dispatch)
    {
        HRESULT hr = IActiveScript_GetScriptDispatch(module->host->script,
                                                     module->name, &module->script_dispatch);
        if (FAILED(hr)) return hr;
    }
    *disp = module->script_dispatch;
    return S_OK;
}

static HRESULT set_script_state(ScriptHost *host, SCRIPTSTATE state)
{
    HRESULT hr;

    hr = IActiveScript_SetScriptState(host->script, state);
    if (SUCCEEDED(hr))
        host->script_state = state;
    return hr;
}

static HRESULT start_script(ScriptHost *host)
{
    HRESULT hr = S_OK;

    if (host->script_state != SCRIPTSTATE_STARTED)
        hr = set_script_state(host, SCRIPTSTATE_STARTED);

    return hr;
}

static HRESULT add_script_object(ScriptHost *host, BSTR name, IDispatch *object, DWORD flags)
{
    struct named_item *item;
    HRESULT hr;

    if (host_get_named_item(host, name))
        return E_INVALIDARG;

    item = heap_alloc(sizeof(*item));
    if (!item)
        return E_OUTOFMEMORY;

    item->name = SysAllocString(name);
    if (!item->name)
    {
        heap_free(item);
        return E_OUTOFMEMORY;
    }
    IDispatch_AddRef(item->disp = object);
    list_add_tail(&host->named_items, &item->entry);

    hr = IActiveScript_AddNamedItem(host->script, name, flags);
    if (FAILED(hr))
    {
        list_remove(&item->entry);
        IDispatch_Release(item->disp);
        SysFreeString(item->name);
        heap_free(item);
        return hr;
    }

    return hr;
}

static HRESULT parse_script_text(ScriptModule *module, BSTR script_text, DWORD flag, VARIANT *res)
{
    EXCEPINFO excepinfo;
    HRESULT hr;

    hr = start_script(module->host);
    if (FAILED(hr)) return hr;

    hr = IActiveScriptParse_ParseScriptText(module->host->parse, script_text, module->name,
                                            NULL, NULL, 0, 1, flag, res, &excepinfo);
    /* FIXME: more error handling */
    return hr;
}

static inline ScriptControl *impl_from_IScriptControl(IScriptControl *iface)
{
    return CONTAINING_RECORD(iface, ScriptControl, IScriptControl_iface);
}

static inline ScriptControl *impl_from_IOleObject(IOleObject *iface)
{
    return CONTAINING_RECORD(iface, ScriptControl, IOleObject_iface);
}

static inline ScriptControl *impl_from_IPersistStreamInit(IPersistStreamInit *iface)
{
    return CONTAINING_RECORD(iface, ScriptControl, IPersistStreamInit_iface);
}

static inline ScriptControl *impl_from_IOleControl(IOleControl *iface)
{
    return CONTAINING_RECORD(iface, ScriptControl, IOleControl_iface);
}

static inline ScriptControl *impl_from_IQuickActivate(IQuickActivate *iface)
{
    return CONTAINING_RECORD(iface, ScriptControl, IQuickActivate_iface);
}

static inline ScriptControl *impl_from_IViewObjectEx(IViewObjectEx *iface)
{
    return CONTAINING_RECORD(iface, ScriptControl, IViewObjectEx_iface);
}

static inline ScriptControl *impl_from_IPointerInactive(IPointerInactive *iface)
{
    return CONTAINING_RECORD(iface, ScriptControl, IPointerInactive_iface);
}

static inline ScriptControl *impl_from_IConnectionPointContainer(IConnectionPointContainer *iface)
{
    return CONTAINING_RECORD(iface, ScriptControl, IConnectionPointContainer_iface);
}

static inline ScriptControl *impl_from_IScriptModuleCollection(IScriptModuleCollection *iface)
{
    return CONTAINING_RECORD(iface, ScriptControl, IScriptModuleCollection_iface);
}

static inline ScriptModule *impl_from_IScriptModule(IScriptModule *iface)
{
    return CONTAINING_RECORD(iface, ScriptModule, IScriptModule_iface);
}

static inline ConnectionPoint *impl_from_IConnectionPoint(IConnectionPoint *iface)
{
    return CONTAINING_RECORD(iface, ConnectionPoint, IConnectionPoint_iface);
}

static inline ScriptHost *impl_from_IActiveScriptSite(IActiveScriptSite *iface)
{
    return CONTAINING_RECORD(iface, ScriptHost, IActiveScriptSite_iface);
}

static inline ScriptHost *impl_from_IActiveScriptSiteWindow(IActiveScriptSiteWindow *iface)
{
    return CONTAINING_RECORD(iface, ScriptHost, IActiveScriptSiteWindow_iface);
}

static inline ScriptHost *impl_from_IServiceProvider(IServiceProvider *iface)
{
    return CONTAINING_RECORD(iface, ScriptHost, IServiceProvider_iface);
}

/* IActiveScriptSite */
static HRESULT WINAPI ActiveScriptSite_QueryInterface(IActiveScriptSite *iface, REFIID riid, void **ppv)
{
    ScriptHost *This = impl_from_IActiveScriptSite(iface);

    if(IsEqualGUID(&IID_IUnknown, riid)) {
        TRACE("(%p)->(IID_IUnknown %p)\n", This, ppv);
        *ppv = &This->IActiveScriptSite_iface;
    }else if(IsEqualGUID(&IID_IActiveScriptSite, riid)) {
        TRACE("(%p)->(IID_IActiveScriptSite %p)\n", This, ppv);
        *ppv = &This->IActiveScriptSite_iface;
    }else if(IsEqualGUID(&IID_IActiveScriptSiteWindow, riid)) {
        TRACE("(%p)->(IID_IActiveScriptSiteWindow %p)\n", This, ppv);
        *ppv = &This->IActiveScriptSiteWindow_iface;
    }else if(IsEqualGUID(&IID_IServiceProvider, riid)) {
        TRACE("(%p)->(IID_IServiceProvider %p)\n", This, ppv);
        *ppv = &This->IServiceProvider_iface;
    }else {
        FIXME("(%p)->(%s %p)\n", This, debugstr_guid(riid), ppv);
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown*)*ppv);
    return S_OK;
}

static ULONG WINAPI ActiveScriptSite_AddRef(IActiveScriptSite *iface)
{
    ScriptHost *This = impl_from_IActiveScriptSite(iface);
    LONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref=%d\n", This, ref);

    return ref;
}

static ULONG WINAPI ActiveScriptSite_Release(IActiveScriptSite *iface)
{
    ScriptHost *This = impl_from_IActiveScriptSite(iface);
    LONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref=%d\n", This, ref);

    if(!ref) {
        clear_named_items(This);
        heap_free(This);
    }

    return ref;
}

static HRESULT WINAPI ActiveScriptSite_GetLCID(IActiveScriptSite *iface, LCID *lcid)
{
    ScriptHost *This = impl_from_IActiveScriptSite(iface);

    TRACE("(%p, %p)\n", This, lcid);

    *lcid = GetUserDefaultLCID();
    return S_OK;
}

static HRESULT WINAPI ActiveScriptSite_GetItemInfo(IActiveScriptSite *iface, LPCOLESTR name, DWORD mask,
    IUnknown **unk, ITypeInfo **ti)
{
    ScriptHost *This = impl_from_IActiveScriptSite(iface);
    struct named_item *item;

    TRACE("(%p, %s, %#x, %p, %p)\n", This, debugstr_w(name), mask, unk, ti);

    item = host_get_named_item(This, name);
    if (!item)
        return TYPE_E_ELEMENTNOTFOUND;

    if (mask != SCRIPTINFO_IUNKNOWN) {
        FIXME("mask %#x is not supported\n", mask);
        return E_NOTIMPL;
    }

    *unk = (IUnknown*)item->disp;
    IUnknown_AddRef(*unk);

    return S_OK;
}

static HRESULT WINAPI ActiveScriptSite_GetDocVersionString(IActiveScriptSite *iface, BSTR *version)
{
    ScriptHost *This = impl_from_IActiveScriptSite(iface);

    FIXME("(%p, %p)\n", This, version);

    return E_NOTIMPL;
}

static HRESULT WINAPI ActiveScriptSite_OnScriptTerminate(IActiveScriptSite *iface, const VARIANT *result,
    const EXCEPINFO *ei)
{
    ScriptHost *This = impl_from_IActiveScriptSite(iface);

    FIXME("(%p, %s, %p)\n", This, debugstr_variant(result), ei);

    return E_NOTIMPL;
}

static HRESULT WINAPI ActiveScriptSite_OnStateChange(IActiveScriptSite *iface, SCRIPTSTATE state)
{
    ScriptHost *This = impl_from_IActiveScriptSite(iface);

    FIXME("(%p, %d)\n", This, state);

    return E_NOTIMPL;
}

static HRESULT WINAPI ActiveScriptSite_OnScriptError(IActiveScriptSite *iface, IActiveScriptError *script_error)
{
    ScriptHost *This = impl_from_IActiveScriptSite(iface);

    FIXME("(%p, %p)\n", This, script_error);

    return E_NOTIMPL;
}

static HRESULT WINAPI ActiveScriptSite_OnEnterScript(IActiveScriptSite *iface)
{
    ScriptHost *This = impl_from_IActiveScriptSite(iface);

    FIXME("(%p)\n", This);

    return E_NOTIMPL;
}

static HRESULT WINAPI ActiveScriptSite_OnLeaveScript(IActiveScriptSite *iface)
{
    ScriptHost *This = impl_from_IActiveScriptSite(iface);

    FIXME("(%p)\n", This);

    return E_NOTIMPL;
}

static const IActiveScriptSiteVtbl ActiveScriptSiteVtbl = {
    ActiveScriptSite_QueryInterface,
    ActiveScriptSite_AddRef,
    ActiveScriptSite_Release,
    ActiveScriptSite_GetLCID,
    ActiveScriptSite_GetItemInfo,
    ActiveScriptSite_GetDocVersionString,
    ActiveScriptSite_OnScriptTerminate,
    ActiveScriptSite_OnStateChange,
    ActiveScriptSite_OnScriptError,
    ActiveScriptSite_OnEnterScript,
    ActiveScriptSite_OnLeaveScript
};

/* IActiveScriptSiteWindow */
static HRESULT WINAPI ActiveScriptSiteWindow_QueryInterface(IActiveScriptSiteWindow *iface, REFIID riid, void **obj)
{
    ScriptHost *This = impl_from_IActiveScriptSiteWindow(iface);
    return IActiveScriptSite_QueryInterface(&This->IActiveScriptSite_iface, riid, obj);
}

static ULONG WINAPI ActiveScriptSiteWindow_AddRef(IActiveScriptSiteWindow *iface)
{
    ScriptHost *This = impl_from_IActiveScriptSiteWindow(iface);
    return IActiveScriptSite_AddRef(&This->IActiveScriptSite_iface);
}

static ULONG WINAPI ActiveScriptSiteWindow_Release(IActiveScriptSiteWindow *iface)
{
    ScriptHost *This = impl_from_IActiveScriptSiteWindow(iface);
    return IActiveScriptSite_Release(&This->IActiveScriptSite_iface);
}

static HRESULT WINAPI ActiveScriptSiteWindow_GetWindow(IActiveScriptSiteWindow *iface, HWND *hwnd)
{
    ScriptHost *This = impl_from_IActiveScriptSiteWindow(iface);

    FIXME("(%p, %p)\n", This, hwnd);

    return E_NOTIMPL;
}

static HRESULT WINAPI ActiveScriptSiteWindow_EnableModeless(IActiveScriptSiteWindow *iface, BOOL enable)
{
    ScriptHost *This = impl_from_IActiveScriptSiteWindow(iface);

    FIXME("(%p, %d)\n", This, enable);

    return E_NOTIMPL;
}

static const IActiveScriptSiteWindowVtbl ActiveScriptSiteWindowVtbl = {
    ActiveScriptSiteWindow_QueryInterface,
    ActiveScriptSiteWindow_AddRef,
    ActiveScriptSiteWindow_Release,
    ActiveScriptSiteWindow_GetWindow,
    ActiveScriptSiteWindow_EnableModeless
};

/* IServiceProvider */
static HRESULT WINAPI ServiceProvider_QueryInterface(IServiceProvider *iface, REFIID riid, void **obj)
{
    ScriptHost *This = impl_from_IServiceProvider(iface);
    return IActiveScriptSite_QueryInterface(&This->IActiveScriptSite_iface, riid, obj);
}

static ULONG WINAPI ServiceProvider_AddRef(IServiceProvider *iface)
{
    ScriptHost *This = impl_from_IServiceProvider(iface);
    return IActiveScriptSite_AddRef(&This->IActiveScriptSite_iface);
}

static ULONG WINAPI ServiceProvider_Release(IServiceProvider *iface)
{
    ScriptHost *This = impl_from_IServiceProvider(iface);
    return IActiveScriptSite_Release(&This->IActiveScriptSite_iface);
}

static HRESULT WINAPI ServiceProvider_QueryService(IServiceProvider *iface, REFGUID service,
    REFIID riid, void **obj)
{
    ScriptHost *This = impl_from_IServiceProvider(iface);

    FIXME("(%p)->(%s %s %p)\n", This, debugstr_guid(service), debugstr_guid(riid), obj);

    return E_NOTIMPL;
}

static const IServiceProviderVtbl ServiceProviderVtbl = {
    ServiceProvider_QueryInterface,
    ServiceProvider_AddRef,
    ServiceProvider_Release,
    ServiceProvider_QueryService
};

static void detach_script_host(ScriptHost *host)
{
    if (--host->module_count)
        return;

    if (host->script) {
        IActiveScript_Close(host->script);
        IActiveScript_Release(host->script);
    }

    if (host->parse)
        IActiveScriptParse_Release(host->parse);

    host->parse = NULL;
    host->script = NULL;
}

static void detach_module(ScriptModule *module)
{
    ScriptHost *host = module->host;

    if (host) {
        module->host = NULL;
        detach_script_host(host);
        IActiveScriptSite_Release(&host->IActiveScriptSite_iface);
    }
}

static HRESULT WINAPI ScriptModule_QueryInterface(IScriptModule *iface, REFIID riid, void **ppv)
{
    ScriptModule *This = impl_from_IScriptModule(iface);

    if (IsEqualGUID(&IID_IDispatch, riid) || IsEqualGUID(&IID_IUnknown, riid) ||
        IsEqualGUID(&IID_IScriptModule, riid))
    {
        *ppv = &This->IScriptModule_iface;
    }
    else
    {
        WARN("unsupported interface: (%p)->(%s %p)\n", This, debugstr_guid(riid), ppv);
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown*)*ppv);
    return S_OK;
}

static ULONG WINAPI ScriptModule_AddRef(IScriptModule *iface)
{
    ScriptModule *This = impl_from_IScriptModule(iface);
    LONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref=%d\n", This, ref);

    return ref;
}

static ULONG WINAPI ScriptModule_Release(IScriptModule *iface)
{
    ScriptModule *This = impl_from_IScriptModule(iface);
    LONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref=%d\n", This, ref);

    if (!ref)
    {
        detach_module(This);
        SysFreeString(This->name);
        if (This->script_dispatch)
            IDispatch_Release(This->script_dispatch);
        heap_free(This);
    }

    return ref;
}

static HRESULT WINAPI ScriptModule_GetTypeInfoCount(IScriptModule *iface, UINT *pctinfo)
{
    ScriptModule *This = impl_from_IScriptModule(iface);

    TRACE("(%p)->(%p)\n", This, pctinfo);

    *pctinfo = 1;
    return S_OK;
}

static HRESULT WINAPI ScriptModule_GetTypeInfo(IScriptModule *iface, UINT iTInfo,
        LCID lcid, ITypeInfo **ppTInfo)
{
    ScriptModule *This = impl_from_IScriptModule(iface);

    TRACE("(%p)->(%u %u %p)\n", This, iTInfo, lcid, ppTInfo);

    return get_typeinfo(IScriptModule_tid, ppTInfo);
}

static HRESULT WINAPI ScriptModule_GetIDsOfNames(IScriptModule *iface, REFIID riid,
        LPOLESTR *rgszNames, UINT cNames, LCID lcid, DISPID *rgDispId)
{
    ScriptModule *This = impl_from_IScriptModule(iface);
    ITypeInfo *typeinfo;
    HRESULT hr;

    TRACE("(%p)->(%s %p %u %u %p)\n", This, debugstr_guid(riid), rgszNames, cNames, lcid, rgDispId);

    hr = get_typeinfo(IScriptModule_tid, &typeinfo);
    if (SUCCEEDED(hr))
    {
        hr = ITypeInfo_GetIDsOfNames(typeinfo, rgszNames, cNames, rgDispId);
        ITypeInfo_Release(typeinfo);
    }

    return hr;
}

static HRESULT WINAPI ScriptModule_Invoke(IScriptModule *iface, DISPID dispIdMember,
        REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams, VARIANT *pVarResult,
        EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    ScriptModule *This = impl_from_IScriptModule(iface);
    ITypeInfo *typeinfo;
    HRESULT hr;

    TRACE("(%p)->(%d %s %d %d %p %p %p %p)\n", This, dispIdMember, debugstr_guid(riid),
           lcid, wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);

    hr = get_typeinfo(IScriptModule_tid, &typeinfo);
    if(SUCCEEDED(hr))
    {
        hr = ITypeInfo_Invoke(typeinfo, iface, dispIdMember, wFlags,
                              pDispParams, pVarResult, pExcepInfo, puArgErr);
        ITypeInfo_Release(typeinfo);
    }

    return hr;
}

static HRESULT WINAPI ScriptModule_get_Name(IScriptModule *iface, BSTR *pbstrName)
{
    ScriptModule *This = impl_from_IScriptModule(iface);

    TRACE("(%p)->(%p)\n", This, pbstrName);

    if (!pbstrName) return E_POINTER;
    if (!This->host) return E_FAIL;

    *pbstrName = SysAllocString(This->name ? This->name : L"Global");
    return *pbstrName ? S_OK : E_OUTOFMEMORY;
}

static HRESULT WINAPI ScriptModule_get_CodeObject(IScriptModule *iface, IDispatch **ppdispObject)
{
    ScriptModule *This = impl_from_IScriptModule(iface);

    FIXME("(%p)->(%p)\n", This, ppdispObject);

    return E_NOTIMPL;
}

static HRESULT WINAPI ScriptModule_get_Procedures(IScriptModule *iface, IScriptProcedureCollection **ppdispProcedures)
{
    ScriptModule *This = impl_from_IScriptModule(iface);

    FIXME("(%p)->(%p)\n", This, ppdispProcedures);

    return E_NOTIMPL;
}

static HRESULT WINAPI ScriptModule_AddCode(IScriptModule *iface, BSTR code)
{
    ScriptModule *This = impl_from_IScriptModule(iface);

    TRACE("(%p)->(%s)\n", This, debugstr_w(code));

    if (!This->host)
        return E_FAIL;

    return parse_script_text(This, code, SCRIPTTEXT_ISVISIBLE, NULL);
}

static HRESULT WINAPI ScriptModule_Eval(IScriptModule *iface, BSTR expression, VARIANT *res)
{
    ScriptModule *This = impl_from_IScriptModule(iface);

    TRACE("(%p)->(%s, %p)\n", This, debugstr_w(expression), res);

    if (!res)
        return E_POINTER;
    V_VT(res) = VT_EMPTY;
    if (!This->host)
        return E_FAIL;

    return parse_script_text(This, expression, SCRIPTTEXT_ISEXPRESSION, res);
}

static HRESULT WINAPI ScriptModule_ExecuteStatement(IScriptModule *iface, BSTR statement)
{
    ScriptModule *This = impl_from_IScriptModule(iface);

    TRACE("(%p)->(%s)\n", This, debugstr_w(statement));

    if (!This->host)
        return E_FAIL;

    return parse_script_text(This, statement, 0, NULL);
}

static HRESULT WINAPI ScriptModule_Run(IScriptModule *iface, BSTR procedure_name, SAFEARRAY **parameters, VARIANT *res)
{
    ScriptModule *This = impl_from_IScriptModule(iface);

    FIXME("(%p)->(%s %p %p)\n", This, debugstr_w(procedure_name), parameters, res);

    return E_NOTIMPL;
}

static const IScriptModuleVtbl ScriptModuleVtbl = {
    ScriptModule_QueryInterface,
    ScriptModule_AddRef,
    ScriptModule_Release,
    ScriptModule_GetTypeInfoCount,
    ScriptModule_GetTypeInfo,
    ScriptModule_GetIDsOfNames,
    ScriptModule_Invoke,
    ScriptModule_get_Name,
    ScriptModule_get_CodeObject,
    ScriptModule_get_Procedures,
    ScriptModule_AddCode,
    ScriptModule_Eval,
    ScriptModule_ExecuteStatement,
    ScriptModule_Run
};

static ScriptModule *create_module(ScriptHost *host, BSTR name)
{
    ScriptModule *module;

    if (!(module = heap_alloc(sizeof(*module)))) return NULL;

    module->IScriptModule_iface.lpVtbl = &ScriptModuleVtbl;
    module->ref = 1;
    module->name = NULL;
    if (name && !(module->name = SysAllocString(name)))
    {
        heap_free(module);
        return NULL;
    }
    module->host = host;
    module->script_dispatch = NULL;
    IActiveScriptSite_AddRef(&host->IActiveScriptSite_iface);
    return module;
}

static void release_modules(ScriptControl *control, BOOL force_detach)
{
    unsigned int i, module_count = control->host->module_count;

    for (i = 0; i < module_count; i++) {
        if (force_detach) detach_module(control->modules[i]);
        IScriptModule_Release(&control->modules[i]->IScriptModule_iface);
    }

    heap_free(control->modules);
}

static ScriptModule *find_module(ScriptControl *control, BSTR name)
{
    unsigned int i;

    if (!wcsicmp(name, L"Global"))
        return control->modules[0];

    for (i = 1; i < control->host->module_count; i++)
    {
        if (!wcsicmp(name, control->modules[i]->name))
            return control->modules[i];
    }
    return NULL;
}

static HRESULT WINAPI ScriptModuleCollection_QueryInterface(IScriptModuleCollection *iface, REFIID riid, void **ppv)
{
    ScriptControl *This = impl_from_IScriptModuleCollection(iface);

    if (IsEqualGUID(&IID_IDispatch, riid) || IsEqualGUID(&IID_IUnknown, riid) ||
        IsEqualGUID(&IID_IScriptModuleCollection, riid))
    {
        *ppv = &This->IScriptModuleCollection_iface;
    }
    else
    {
        WARN("unsupported interface: (%p)->(%s %p)\n", This, debugstr_guid(riid), ppv);
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown*)*ppv);
    return S_OK;
}

static ULONG WINAPI ScriptModuleCollection_AddRef(IScriptModuleCollection *iface)
{
    ScriptControl *This = impl_from_IScriptModuleCollection(iface);
    return IScriptControl_AddRef(&This->IScriptControl_iface);
}

static ULONG WINAPI ScriptModuleCollection_Release(IScriptModuleCollection *iface)
{
    ScriptControl *This = impl_from_IScriptModuleCollection(iface);
    return IScriptControl_Release(&This->IScriptControl_iface);
}

static HRESULT WINAPI ScriptModuleCollection_GetTypeInfoCount(IScriptModuleCollection *iface, UINT *pctinfo)
{
    ScriptControl *This = impl_from_IScriptModuleCollection(iface);

    TRACE("(%p)->(%p)\n", This, pctinfo);

    *pctinfo = 1;
    return S_OK;
}

static HRESULT WINAPI ScriptModuleCollection_GetTypeInfo(IScriptModuleCollection *iface, UINT iTInfo,
        LCID lcid, ITypeInfo **ppTInfo)
{
    ScriptControl *This = impl_from_IScriptModuleCollection(iface);

    TRACE("(%p)->(%u %u %p)\n", This, iTInfo, lcid, ppTInfo);

    return get_typeinfo(IScriptModuleCollection_tid, ppTInfo);
}

static HRESULT WINAPI ScriptModuleCollection_GetIDsOfNames(IScriptModuleCollection *iface, REFIID riid,
        LPOLESTR *rgszNames, UINT cNames, LCID lcid, DISPID *rgDispId)
{
    ScriptControl *This = impl_from_IScriptModuleCollection(iface);
    ITypeInfo *typeinfo;
    HRESULT hr;

    TRACE("(%p)->(%s %p %u %u %p)\n", This, debugstr_guid(riid), rgszNames, cNames, lcid, rgDispId);

    hr = get_typeinfo(IScriptModuleCollection_tid, &typeinfo);
    if (SUCCEEDED(hr))
    {
        hr = ITypeInfo_GetIDsOfNames(typeinfo, rgszNames, cNames, rgDispId);
        ITypeInfo_Release(typeinfo);
    }

    return hr;
}

static HRESULT WINAPI ScriptModuleCollection_Invoke(IScriptModuleCollection *iface, DISPID dispIdMember,
        REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams, VARIANT *pVarResult,
        EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    ScriptControl *This = impl_from_IScriptModuleCollection(iface);
    ITypeInfo *typeinfo;
    HRESULT hr;

    TRACE("(%p)->(%d %s %d %d %p %p %p %p)\n", This, dispIdMember, debugstr_guid(riid),
           lcid, wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);

    hr = get_typeinfo(IScriptModuleCollection_tid, &typeinfo);
    if(SUCCEEDED(hr))
    {
        hr = ITypeInfo_Invoke(typeinfo, iface, dispIdMember, wFlags,
                              pDispParams, pVarResult, pExcepInfo, puArgErr);
        ITypeInfo_Release(typeinfo);
    }

    return hr;
}

static HRESULT WINAPI ScriptModuleCollection_get__NewEnum(IScriptModuleCollection *iface, IUnknown **ppenumContexts)
{
    ScriptControl *This = impl_from_IScriptModuleCollection(iface);

    FIXME("(%p)->(%p)\n", This, ppenumContexts);

    return E_NOTIMPL;
}

static HRESULT WINAPI ScriptModuleCollection_get_Item(IScriptModuleCollection *iface, VARIANT index,
        IScriptModule **ppmod)
{
    ScriptControl *This = impl_from_IScriptModuleCollection(iface);
    ScriptModule *module;
    unsigned int i;
    HRESULT hr;

    TRACE("(%p)->(%s %p)\n", This, wine_dbgstr_variant(&index), ppmod);

    if (!ppmod) return E_POINTER;
    if (!This->host) return E_FAIL;

    if (V_VT(&index) == VT_BSTR)
    {
        module = find_module(This, V_BSTR(&index));
        if (!module) return CTL_E_ILLEGALFUNCTIONCALL;
    }
    else
    {
        hr = VariantChangeType(&index, &index, 0, VT_INT);
        if (FAILED(hr)) return hr;

        i = V_INT(&index) - 1;
        if (i >= This->host->module_count) return 0x800a0009;

        module = This->modules[i];
    }

    *ppmod = &module->IScriptModule_iface;
    IScriptModule_AddRef(*ppmod);
    return S_OK;
}

static HRESULT WINAPI ScriptModuleCollection_get_Count(IScriptModuleCollection *iface, LONG *plCount)
{
    ScriptControl *This = impl_from_IScriptModuleCollection(iface);

    TRACE("(%p)->(%p)\n", This, plCount);

    if (!plCount) return E_POINTER;
    if (!This->host) return E_FAIL;

    *plCount = This->host->module_count;
    return S_OK;
}

static HRESULT WINAPI ScriptModuleCollection_Add(IScriptModuleCollection *iface, BSTR name,
        VARIANT *object, IScriptModule **ppmod)
{
    ScriptControl *This = impl_from_IScriptModuleCollection(iface);
    ScriptModule *module, **modules;
    ScriptHost *host = This->host;
    HRESULT hr;

    TRACE("(%p)->(%s %s %p)\n", This, wine_dbgstr_w(name), wine_dbgstr_variant(object), ppmod);

    if (!ppmod) return E_POINTER;
    if (!name || V_VT(object) != VT_DISPATCH) return E_INVALIDARG;
    if (!host) return E_FAIL;
    if (find_module(This, name)) return E_INVALIDARG;

    /* See if we need to grow the array */
    if (is_power_of_2(host->module_count))
    {
        modules = heap_realloc(This->modules, host->module_count * 2 * sizeof(*This->modules));
        if (!modules) return E_OUTOFMEMORY;
        This->modules = modules;
    }

    if (!(module = create_module(host, name)))
        return E_OUTOFMEMORY;

    /* If no object, Windows only calls AddNamedItem without adding a NULL object */
    if (V_DISPATCH(object))
        hr = add_script_object(host, name, V_DISPATCH(object), 0);
    else
        hr = IActiveScript_AddNamedItem(host->script, name, SCRIPTITEM_CODEONLY);

    if (FAILED(hr))
    {
        IScriptModule_Release(&module->IScriptModule_iface);
        return hr;
    }
    This->modules[host->module_count++] = module;

    *ppmod = &module->IScriptModule_iface;
    IScriptModule_AddRef(*ppmod);
    return S_OK;
}

static const IScriptModuleCollectionVtbl ScriptModuleCollectionVtbl = {
    ScriptModuleCollection_QueryInterface,
    ScriptModuleCollection_AddRef,
    ScriptModuleCollection_Release,
    ScriptModuleCollection_GetTypeInfoCount,
    ScriptModuleCollection_GetTypeInfo,
    ScriptModuleCollection_GetIDsOfNames,
    ScriptModuleCollection_Invoke,
    ScriptModuleCollection_get__NewEnum,
    ScriptModuleCollection_get_Item,
    ScriptModuleCollection_get_Count,
    ScriptModuleCollection_Add
};

static HRESULT init_script_host(const CLSID *clsid, ScriptHost **ret)
{
    IObjectSafety *objsafety;
    ScriptHost *host;
    HRESULT hr;

    *ret = NULL;

    host = heap_alloc(sizeof(*host));
    if (!host)
        return E_OUTOFMEMORY;

    host->IActiveScriptSite_iface.lpVtbl = &ActiveScriptSiteVtbl;
    host->IActiveScriptSiteWindow_iface.lpVtbl = &ActiveScriptSiteWindowVtbl;
    host->IServiceProvider_iface.lpVtbl = &ServiceProviderVtbl;
    host->ref = 1;
    host->script = NULL;
    host->parse = NULL;
    host->clsid = *clsid;
    host->module_count = 1;
    list_init(&host->named_items);

    hr = CoCreateInstance(&host->clsid, NULL, CLSCTX_INPROC_SERVER|CLSCTX_INPROC_HANDLER,
            &IID_IActiveScript, (void**)&host->script);
    if (FAILED(hr)) {
        WARN("Failed to create an instance for %s, %#x\n", debugstr_guid(clsid), hr);
        goto failed;
    }

    hr = IActiveScript_QueryInterface(host->script, &IID_IObjectSafety, (void**)&objsafety);
    if (FAILED(hr)) {
        FIXME("Could not get IObjectSafety, %#x\n", hr);
        goto failed;
    }

    hr = IObjectSafety_SetInterfaceSafetyOptions(objsafety, &IID_IActiveScriptParse, INTERFACESAFE_FOR_UNTRUSTED_DATA, 0);
    IObjectSafety_Release(objsafety);
    if (FAILED(hr)) {
        FIXME("SetInterfaceSafetyOptions failed, %#x\n", hr);
        goto failed;
    }

    hr = IActiveScript_SetScriptSite(host->script, &host->IActiveScriptSite_iface);
    if (FAILED(hr)) {
        WARN("SetScriptSite failed, %#x\n", hr);
        goto failed;
    }

    hr = IActiveScript_QueryInterface(host->script, &IID_IActiveScriptParse, (void**)&host->parse);
    if (FAILED(hr)) {
        WARN("Failed to get IActiveScriptParse, %#x\n", hr);
        goto failed;
    }

    hr = IActiveScriptParse_InitNew(host->parse);
    if (FAILED(hr)) {
        WARN("InitNew failed, %#x\n", hr);
        goto failed;
    }
    host->script_state = SCRIPTSTATE_INITIALIZED;

    *ret = host;
    return S_OK;

failed:
    detach_script_host(host);
    return hr;
}

static HRESULT WINAPI ScriptControl_QueryInterface(IScriptControl *iface, REFIID riid, void **ppv)
{
    ScriptControl *This = impl_from_IScriptControl(iface);

    if(IsEqualGUID(&IID_IUnknown, riid)) {
        TRACE("(%p)->(IID_IUnknown %p)\n", This, ppv);
        *ppv = &This->IScriptControl_iface;
    }else if(IsEqualGUID(&IID_IDispatch, riid)) {
        TRACE("(%p)->(IID_IDispatch %p)\n", This, ppv);
        *ppv = &This->IScriptControl_iface;
    }else if(IsEqualGUID(&IID_IScriptControl, riid)) {
        TRACE("(%p)->(IID_IScriptControl %p)\n", This, ppv);
        *ppv = &This->IScriptControl_iface;
    }else if(IsEqualGUID(&IID_IOleObject, riid)) {
        TRACE("(%p)->(IID_IOleObject %p)\n", This, ppv);
        *ppv = &This->IOleObject_iface;
    }else if(IsEqualGUID(&IID_IPersistStreamInit, riid)) {
        TRACE("(%p)->(IID_IPersistStreamInit %p)\n", This, ppv);
        *ppv = &This->IPersistStreamInit_iface;
    }else if(IsEqualGUID(&IID_IPersist, riid)) {
        TRACE("(%p)->(IID_IPersist %p)\n", This, ppv);
        *ppv = &This->IPersistStreamInit_iface;
    }else if(IsEqualGUID(&IID_IOleControl, riid)) {
        TRACE("(%p)->(IID_IOleControl %p)\n", This, ppv);
        *ppv = &This->IOleControl_iface;
    }else if(IsEqualGUID(&IID_IQuickActivate, riid)) {
        TRACE("(%p)->(IID_IQuickActivate %p)\n", This, ppv);
        *ppv = &This->IQuickActivate_iface;
    }else if(IsEqualGUID(&IID_IViewObject, riid)) {
        TRACE("(%p)->(IID_IViewObject %p)\n", This, ppv);
        *ppv = &This->IViewObjectEx_iface;
    }else if(IsEqualGUID(&IID_IViewObject2, riid)) {
        TRACE("(%p)->(IID_IViewObject2 %p)\n", This, ppv);
        *ppv = &This->IViewObjectEx_iface;
    }else if(IsEqualGUID(&IID_IViewObjectEx, riid)) {
        TRACE("(%p)->(IID_IViewObjectEx %p)\n", This, ppv);
        *ppv = &This->IViewObjectEx_iface;
    }else if(IsEqualGUID(&IID_IPointerInactive, riid)) {
        TRACE("(%p)->(IID_IPointerInactive %p)\n", This, ppv);
        *ppv = &This->IPointerInactive_iface;
    }else if(IsEqualGUID(&IID_IConnectionPointContainer, riid)) {
        TRACE("(%p)->(IID_IConnectionPointContainer %p)\n", This, ppv);
        *ppv = &This->IConnectionPointContainer_iface;
    }else {
        FIXME("(%p)->(%s %p)\n", This, debugstr_guid(riid), ppv);
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown*)*ppv);
    return S_OK;
}

static ULONG WINAPI ScriptControl_AddRef(IScriptControl *iface)
{
    ScriptControl *This = impl_from_IScriptControl(iface);
    LONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref=%d\n", This, ref);

    return ref;
}

static ULONG WINAPI ScriptControl_Release(IScriptControl *iface)
{
    ScriptControl *This = impl_from_IScriptControl(iface);
    LONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref=%d\n", This, ref);

    if(!ref) {
        if (This->site)
            IOleClientSite_Release(This->site);
        if (This->host)
        {
            release_modules(This, FALSE);
            IActiveScriptSite_Release(&This->host->IActiveScriptSite_iface);
        }
        heap_free(This);
    }

    return ref;
}

static HRESULT WINAPI ScriptControl_GetTypeInfoCount(IScriptControl *iface, UINT *pctinfo)
{
    ScriptControl *This = impl_from_IScriptControl(iface);
    TRACE("(%p)->(%p)\n", This, pctinfo);
    *pctinfo = 1;
    return S_OK;
}

static HRESULT WINAPI ScriptControl_GetTypeInfo(IScriptControl *iface, UINT iTInfo,
        LCID lcid, ITypeInfo **ppTInfo)
{
    ScriptControl *This = impl_from_IScriptControl(iface);
    TRACE("(%p)->(%u %u %p)\n", This, iTInfo, lcid, ppTInfo);
    return get_typeinfo(IScriptControl_tid, ppTInfo);
}

static HRESULT WINAPI ScriptControl_GetIDsOfNames(IScriptControl *iface, REFIID riid,
        LPOLESTR *rgszNames, UINT cNames, LCID lcid, DISPID *rgDispId)
{
    ScriptControl *This = impl_from_IScriptControl(iface);
    ITypeInfo *typeinfo;
    HRESULT hres;

    TRACE("(%p)->(%s %p %u %u %p)\n", This, debugstr_guid(riid), rgszNames, cNames, lcid, rgDispId);

    hres = get_typeinfo(IScriptControl_tid, &typeinfo);
    if(SUCCEEDED(hres)) {
        hres = ITypeInfo_GetIDsOfNames(typeinfo, rgszNames, cNames, rgDispId);
        ITypeInfo_Release(typeinfo);
    }

    return hres;
}

static HRESULT WINAPI ScriptControl_Invoke(IScriptControl *iface, DISPID dispIdMember,
        REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams, VARIANT *pVarResult,
        EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    ScriptControl *This = impl_from_IScriptControl(iface);
    ITypeInfo *typeinfo;
    HRESULT hres;

    TRACE("(%p)->(%d %s %d %d %p %p %p %p)\n", This, dispIdMember, debugstr_guid(riid),
           lcid, wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);

    hres = get_typeinfo(IScriptControl_tid, &typeinfo);
    if(SUCCEEDED(hres)) {
        hres = ITypeInfo_Invoke(typeinfo, iface, dispIdMember, wFlags,
                pDispParams, pVarResult, pExcepInfo, puArgErr);
        ITypeInfo_Release(typeinfo);
    }

    return hres;
}

static HRESULT WINAPI ScriptControl_get_Language(IScriptControl *iface, BSTR *p)
{
    ScriptControl *This = impl_from_IScriptControl(iface);
    LPOLESTR progidW;
    HRESULT hr;

    TRACE("(%p)->(%p)\n", This, p);

    if (!p)
        return E_POINTER;

    *p = NULL;

    if (!This->host)
        return S_OK;

    hr = ProgIDFromCLSID(&This->host->clsid, &progidW);
    if (FAILED(hr))
        return hr;

    *p = SysAllocString(progidW);
    CoTaskMemFree(progidW);
    return *p ? S_OK : E_OUTOFMEMORY;
}

static HRESULT WINAPI ScriptControl_put_Language(IScriptControl *iface, BSTR language)
{
    ScriptControl *This = impl_from_IScriptControl(iface);
    CLSID clsid;
    HRESULT hres;

    TRACE("(%p)->(%s)\n", This, debugstr_w(language));

    if (language && FAILED(CLSIDFromProgID(language, &clsid)))
        return CTL_E_INVALIDPROPERTYVALUE;

    if (This->host) {
        release_modules(This, TRUE);
        IActiveScriptSite_Release(&This->host->IActiveScriptSite_iface);
        This->host = NULL;
    }

    if (!language)
        return S_OK;

    hres = init_script_host(&clsid, &This->host);
    if (FAILED(hres))
        return hres;

    /* Alloc global module */
    This->modules = heap_alloc_zero(sizeof(*This->modules));
    if (This->modules) {
        This->modules[0] = create_module(This->host, NULL);
        if (!This->modules[0]) {
            heap_free(This->modules);
            This->modules = NULL;
            hres = E_OUTOFMEMORY;
        }
    }
    else
        hres = E_OUTOFMEMORY;

    if (FAILED(hres)) {
        detach_script_host(This->host);
        This->host = NULL;
    }
    return hres;
}

static HRESULT WINAPI ScriptControl_get_State(IScriptControl *iface, ScriptControlStates *p)
{
    ScriptControl *This = impl_from_IScriptControl(iface);
    SCRIPTSTATE state;
    HRESULT hres;

    TRACE("(%p)->(%p)\n", This, p);

    if(!p)
        return E_POINTER;

    if(!This->host)
        return E_FAIL;

    hres = IActiveScript_GetScriptState(This->host->script, &state);
    if (FAILED(hres)) return hres;

    switch (state)
    {
    case SCRIPTSTATE_INITIALIZED:
    case SCRIPTSTATE_STARTED:
        *p = Initialized;
        break;
    case SCRIPTSTATE_CONNECTED:
        *p = Connected;
        break;
    default:
        WARN("unexpected state %d\n", state);
        return E_FAIL;
    }
    return S_OK;
}

static HRESULT WINAPI ScriptControl_put_State(IScriptControl *iface, ScriptControlStates state)
{
    ScriptControl *This = impl_from_IScriptControl(iface);
    TRACE("(%p)->(%x)\n", This, state);

    if(!This->host)
        return E_FAIL;

    if(state != Initialized && state != Connected)
        return CTL_E_INVALIDPROPERTYVALUE;

    return IActiveScript_SetScriptState(This->host->script, state == Connected ? SCRIPTSTATE_CONNECTED : SCRIPTSTATE_STARTED);
}

static HRESULT WINAPI ScriptControl_put_SitehWnd(IScriptControl *iface, LONG hwnd)
{
    ScriptControl *This = impl_from_IScriptControl(iface);

    FIXME("(%p)->(%x)\n", This, hwnd);

    return S_OK;
}

static HRESULT WINAPI ScriptControl_get_SitehWnd(IScriptControl *iface, LONG *p)
{
    ScriptControl *This = impl_from_IScriptControl(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI ScriptControl_get_Timeout(IScriptControl *iface, LONG *p)
{
    ScriptControl *This = impl_from_IScriptControl(iface);

    TRACE("(%p)->(%p)\n", This, p);

    if (!p)
        return E_POINTER;

    *p = This->timeout;
    return S_OK;
}

static HRESULT WINAPI ScriptControl_put_Timeout(IScriptControl *iface, LONG timeout)
{
    ScriptControl *This = impl_from_IScriptControl(iface);

    TRACE("(%p)->(%d)\n", This, timeout);

    if (timeout < -1)
        return CTL_E_INVALIDPROPERTYVALUE;

    if (timeout != -1)
        FIXME("execution timeout ignored\n");

    This->timeout = timeout;
    return S_OK;
}

static HRESULT WINAPI ScriptControl_get_AllowUI(IScriptControl *iface, VARIANT_BOOL *p)
{
    ScriptControl *This = impl_from_IScriptControl(iface);
    TRACE("(%p)->(%p)\n", This, p);

    if(!p)
        return E_POINTER;

    *p = This->allow_ui;
    return S_OK;
}

static HRESULT WINAPI ScriptControl_put_AllowUI(IScriptControl *iface, VARIANT_BOOL allow_ui)
{
    ScriptControl *This = impl_from_IScriptControl(iface);
    TRACE("(%p)->(%x)\n", This, allow_ui);

    This->allow_ui = allow_ui;
    return S_OK;
}

static HRESULT WINAPI ScriptControl_get_UseSafeSubset(IScriptControl *iface, VARIANT_BOOL *p)
{
    ScriptControl *This = impl_from_IScriptControl(iface);
    TRACE("(%p)->(%p)\n", This, p);

    if(!p)
        return E_POINTER;

    *p = This->use_safe_subset;
    return S_OK;
}

static HRESULT WINAPI ScriptControl_put_UseSafeSubset(IScriptControl *iface, VARIANT_BOOL use_safe_subset)
{
    ScriptControl *This = impl_from_IScriptControl(iface);
    TRACE("(%p)->(%x)\n", This, use_safe_subset);

    This->use_safe_subset = use_safe_subset;
    return S_OK;
}

static HRESULT WINAPI ScriptControl_get_Modules(IScriptControl *iface, IScriptModuleCollection **p)
{
    ScriptControl *This = impl_from_IScriptControl(iface);

    TRACE("(%p)->(%p)\n", This, p);

    if (!This->host) return E_FAIL;

    *p = &This->IScriptModuleCollection_iface;
    IScriptControl_AddRef(iface);
    return S_OK;
}

static HRESULT WINAPI ScriptControl_get_Error(IScriptControl *iface, IScriptError **p)
{
    ScriptControl *This = impl_from_IScriptControl(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI ScriptControl_get_CodeObject(IScriptControl *iface, IDispatch **p)
{
    ScriptControl *This = impl_from_IScriptControl(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI ScriptControl_get_Procedures(IScriptControl *iface, IScriptProcedureCollection **p)
{
    ScriptControl *This = impl_from_IScriptControl(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI ScriptControl__AboutBox(IScriptControl *iface)
{
    ScriptControl *This = impl_from_IScriptControl(iface);
    FIXME("(%p)\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI ScriptControl_AddObject(IScriptControl *iface, BSTR name, IDispatch *object, VARIANT_BOOL add_members)
{
    ScriptControl *This = impl_from_IScriptControl(iface);
    DWORD flags = SCRIPTITEM_ISVISIBLE | SCRIPTITEM_ISSOURCE;

    TRACE("(%p)->(%s %p %x)\n", This, debugstr_w(name), object, add_members);

    if (!object)
        return E_INVALIDARG;

    if (!This->host)
        return E_FAIL;

    if (add_members)
        flags |= SCRIPTITEM_GLOBALMEMBERS;
    return add_script_object(This->host, name, object, flags);
}

static HRESULT WINAPI ScriptControl_Reset(IScriptControl *iface)
{
    ScriptControl *This = impl_from_IScriptControl(iface);

    TRACE("(%p)\n", This);

    if (!This->host)
        return E_FAIL;

    clear_named_items(This->host);
    return set_script_state(This->host, SCRIPTSTATE_INITIALIZED);
}

static HRESULT WINAPI ScriptControl_AddCode(IScriptControl *iface, BSTR code)
{
    ScriptControl *This = impl_from_IScriptControl(iface);

    TRACE("(%p)->(%s).\n", This, debugstr_w(code));

    if (!This->host)
        return E_FAIL;

    return parse_script_text(This->modules[0], code, SCRIPTTEXT_ISVISIBLE, NULL);
}

static HRESULT WINAPI ScriptControl_Eval(IScriptControl *iface, BSTR expression, VARIANT *res)
{
    ScriptControl *This = impl_from_IScriptControl(iface);

    TRACE("(%p)->(%s, %p).\n", This, debugstr_w(expression), res);

    if (!res)
        return E_POINTER;
    V_VT(res) = VT_EMPTY;
    if (!This->host)
        return E_FAIL;

    return parse_script_text(This->modules[0], expression, SCRIPTTEXT_ISEXPRESSION, res);
}

static HRESULT WINAPI ScriptControl_ExecuteStatement(IScriptControl *iface, BSTR statement)
{
    ScriptControl *This = impl_from_IScriptControl(iface);

    TRACE("(%p)->(%s)\n", This, debugstr_w(statement));

    if (!This->host)
        return E_FAIL;

    return parse_script_text(This->modules[0], statement, 0, NULL);
}

static HRESULT WINAPI ScriptControl_Run(IScriptControl *iface, BSTR procedure_name, SAFEARRAY **parameters, VARIANT *res)
{
    ScriptControl *This = impl_from_IScriptControl(iface);
    IDispatchEx *dispex;
    IDispatch *disp;
    SAFEARRAY *sa;
    DISPPARAMS dp;
    DISPID dispid;
    HRESULT hr;
    UINT i;

    TRACE("(%p)->(%s %p %p)\n", This, debugstr_w(procedure_name), parameters, res);

    if (!parameters || !res) return E_POINTER;
    if (!(sa = *parameters)) return E_POINTER;

    V_VT(res) = VT_EMPTY;
    if (sa->cDims == 0) return DISP_E_BADINDEX;
    if (!(sa->fFeatures & FADF_VARIANT)) return DISP_E_BADVARTYPE;


    if (!This->host) return E_FAIL;
    hr = start_script(This->host);
    if (FAILED(hr)) return hr;

    hr = get_script_dispatch(This->modules[0], &disp);
    if (FAILED(hr)) return hr;

    hr = IDispatch_GetIDsOfNames(disp, &IID_NULL, &procedure_name, 1, LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr)) return hr;

    dp.cArgs = sa->rgsabound[0].cElements;
    dp.rgdispidNamedArgs = NULL;
    dp.cNamedArgs = 0;
    dp.rgvarg = heap_alloc(dp.cArgs * sizeof(*dp.rgvarg));
    if (!dp.rgvarg) return E_OUTOFMEMORY;

    hr = SafeArrayLock(sa);
    if (SUCCEEDED(hr))
    {
        /* The DISPPARAMS are stored in reverse order */
        for (i = 0; i < dp.cArgs; i++)
            dp.rgvarg[i] = *(VARIANT*)((char*)(sa->pvData) + (dp.cArgs - i - 1) * sa->cbElements);
        SafeArrayUnlock(sa);

        hr = IDispatch_QueryInterface(disp, &IID_IDispatchEx, (void**)&dispex);
        if (FAILED(hr))
        {
            hr = IDispatch_Invoke(disp, dispid, &IID_NULL, LOCALE_USER_DEFAULT,
                                  DISPATCH_METHOD, &dp, res, NULL, NULL);
        }
        else
        {
            hr = IDispatchEx_InvokeEx(dispex, dispid, LOCALE_USER_DEFAULT,
                                      DISPATCH_METHOD, &dp, res, NULL, NULL);
            IDispatchEx_Release(dispex);
        }
    }
    heap_free(dp.rgvarg);

    return hr;
}

static const IScriptControlVtbl ScriptControlVtbl = {
    ScriptControl_QueryInterface,
    ScriptControl_AddRef,
    ScriptControl_Release,
    ScriptControl_GetTypeInfoCount,
    ScriptControl_GetTypeInfo,
    ScriptControl_GetIDsOfNames,
    ScriptControl_Invoke,
    ScriptControl_get_Language,
    ScriptControl_put_Language,
    ScriptControl_get_State,
    ScriptControl_put_State,
    ScriptControl_put_SitehWnd,
    ScriptControl_get_SitehWnd,
    ScriptControl_get_Timeout,
    ScriptControl_put_Timeout,
    ScriptControl_get_AllowUI,
    ScriptControl_put_AllowUI,
    ScriptControl_get_UseSafeSubset,
    ScriptControl_put_UseSafeSubset,
    ScriptControl_get_Modules,
    ScriptControl_get_Error,
    ScriptControl_get_CodeObject,
    ScriptControl_get_Procedures,
    ScriptControl__AboutBox,
    ScriptControl_AddObject,
    ScriptControl_Reset,
    ScriptControl_AddCode,
    ScriptControl_Eval,
    ScriptControl_ExecuteStatement,
    ScriptControl_Run
};

static HRESULT WINAPI OleObject_QueryInterface(IOleObject *iface, REFIID riid, void **obj)
{
    ScriptControl *This = impl_from_IOleObject(iface);
    return IScriptControl_QueryInterface(&This->IScriptControl_iface, riid, obj);
}

static ULONG WINAPI OleObject_AddRef(IOleObject *iface)
{
    ScriptControl *This = impl_from_IOleObject(iface);
    return IScriptControl_AddRef(&This->IScriptControl_iface);
}

static ULONG WINAPI OleObject_Release(IOleObject *iface)
{
    ScriptControl *This = impl_from_IOleObject(iface);
    return IScriptControl_Release(&This->IScriptControl_iface);
}

static HRESULT WINAPI OleObject_SetClientSite(IOleObject *iface, IOleClientSite *site)
{
    ScriptControl *This = impl_from_IOleObject(iface);

    TRACE("(%p)->(%p)\n", This, site);

    if (This->site)
        IOleClientSite_Release(This->site);

    if ((This->site = site))
        IOleClientSite_AddRef(site);

    return S_OK;
}

static HRESULT WINAPI OleObject_GetClientSite(IOleObject *iface, IOleClientSite **site)
{
    ScriptControl *This = impl_from_IOleObject(iface);

    TRACE("(%p)->(%p)\n", This, site);

    if (!site)
        return E_POINTER;

    if ((*site = This->site))
        IOleClientSite_AddRef(*site);

    return S_OK;
}

static HRESULT WINAPI OleObject_SetHostNames(IOleObject *iface, LPCOLESTR containerapp, LPCOLESTR containerobj)
{
    ScriptControl *This = impl_from_IOleObject(iface);

    FIXME("(%p)->(%s %s)\n", This, debugstr_w(containerapp), debugstr_w(containerobj));

    return E_NOTIMPL;
}

static HRESULT WINAPI OleObject_Close(IOleObject *iface, DWORD save)
{
    ScriptControl *This = impl_from_IOleObject(iface);

    FIXME("(%p)->(%d)\n", This, save);

    return E_NOTIMPL;
}

static HRESULT WINAPI OleObject_SetMoniker(IOleObject *iface, DWORD which, IMoniker *moniker)
{
    ScriptControl *This = impl_from_IOleObject(iface);

    FIXME("(%p)->(%d %p)\n", This, which, moniker);

    return E_NOTIMPL;
}

static HRESULT WINAPI OleObject_GetMoniker(IOleObject *iface, DWORD assign, DWORD which, IMoniker **moniker)
{
    ScriptControl *This = impl_from_IOleObject(iface);

    FIXME("(%p)->(%d %d %p)\n", This, assign, which, moniker);

    return E_NOTIMPL;
}

static HRESULT WINAPI OleObject_InitFromData(IOleObject *iface, IDataObject *dataobj, BOOL creation,
    DWORD reserved)
{
    ScriptControl *This = impl_from_IOleObject(iface);

    FIXME("(%p)->(%p %d %d)\n", This, dataobj, creation, reserved);

    return E_NOTIMPL;
}

static HRESULT WINAPI OleObject_GetClipboardData(IOleObject *iface, DWORD reserved, IDataObject **dataobj)
{
    ScriptControl *This = impl_from_IOleObject(iface);

    FIXME("(%p)->(%d %p)\n", This, reserved, dataobj);

    return E_NOTIMPL;
}

static HRESULT WINAPI OleObject_DoVerb(IOleObject *iface, LONG verb, LPMSG msg, IOleClientSite *active_site,
    LONG index, HWND hwndParent, LPCRECT rect)
{
    ScriptControl *This = impl_from_IOleObject(iface);

    FIXME("(%p)->(%d %p %p %d %p %p)\n", This, verb, msg, active_site, index, hwndParent, rect);

    return E_NOTIMPL;
}

static HRESULT WINAPI OleObject_EnumVerbs(IOleObject *iface, IEnumOLEVERB **enumoleverb)
{
    ScriptControl *This = impl_from_IOleObject(iface);

    FIXME("(%p)->(%p)\n", This, enumoleverb);

    return E_NOTIMPL;
}

static HRESULT WINAPI OleObject_Update(IOleObject *iface)
{
    ScriptControl *This = impl_from_IOleObject(iface);

    FIXME("(%p)\n", This);

    return E_NOTIMPL;
}

static HRESULT WINAPI OleObject_IsUpToDate(IOleObject *iface)
{
    ScriptControl *This = impl_from_IOleObject(iface);

    FIXME("(%p)\n", This);

    return E_NOTIMPL;
}

static HRESULT WINAPI OleObject_GetUserClassID(IOleObject *iface, CLSID *clsid)
{
    ScriptControl *This = impl_from_IOleObject(iface);

    FIXME("(%p)->(%p)\n", This, clsid);

    return E_NOTIMPL;
}

static HRESULT WINAPI OleObject_GetUserType(IOleObject *iface, DWORD form_of_type, LPOLESTR *usertype)
{
    ScriptControl *This = impl_from_IOleObject(iface);

    FIXME("(%p)->(%d %p)\n", This, form_of_type, usertype);

    return E_NOTIMPL;
}

static HRESULT WINAPI OleObject_SetExtent(IOleObject *iface, DWORD aspect, SIZEL *size)
{
    ScriptControl *This = impl_from_IOleObject(iface);

    FIXME("(%p)->(%d %p)\n", This, aspect, size);

    return E_NOTIMPL;
}

static HRESULT WINAPI OleObject_GetExtent(IOleObject *iface, DWORD aspect, SIZEL *size)
{
    ScriptControl *This = impl_from_IOleObject(iface);

    TRACE("(%p)->(%d %p)\n", This, aspect, size);

    if (aspect != DVASPECT_CONTENT)
        return DV_E_DVASPECT;

    *size = This->extent;
    return S_OK;
}

static HRESULT WINAPI OleObject_Advise(IOleObject *iface, IAdviseSink *sink, DWORD *connection)
{
    ScriptControl *This = impl_from_IOleObject(iface);

    FIXME("(%p)->(%p %p)\n", This, sink, connection);

    return E_NOTIMPL;
}

static HRESULT WINAPI OleObject_Unadvise(IOleObject *iface, DWORD connection)
{
    ScriptControl *This = impl_from_IOleObject(iface);

    FIXME("(%p)->(%d)\n", This, connection);

    return E_NOTIMPL;
}

static HRESULT WINAPI OleObject_EnumAdvise(IOleObject *iface, IEnumSTATDATA **enumadvise)
{
    ScriptControl *This = impl_from_IOleObject(iface);

    FIXME("(%p)->(%p)\n", This, enumadvise);

    return E_NOTIMPL;
}

static HRESULT WINAPI OleObject_GetMiscStatus(IOleObject *iface, DWORD aspect, DWORD *status)
{
    ScriptControl *This = impl_from_IOleObject(iface);

    TRACE("(%p)->(%d %p)\n", This, aspect, status);

    return OleRegGetMiscStatus(&CLSID_ScriptControl, aspect, status);
}

static HRESULT WINAPI OleObject_SetColorScheme(IOleObject *iface, LOGPALETTE *logpal)
{
    ScriptControl *This = impl_from_IOleObject(iface);

    FIXME("(%p)->(%p)\n", This, logpal);

    return E_NOTIMPL;
}

static const IOleObjectVtbl OleObjectVtbl = {
    OleObject_QueryInterface,
    OleObject_AddRef,
    OleObject_Release,
    OleObject_SetClientSite,
    OleObject_GetClientSite,
    OleObject_SetHostNames,
    OleObject_Close,
    OleObject_SetMoniker,
    OleObject_GetMoniker,
    OleObject_InitFromData,
    OleObject_GetClipboardData,
    OleObject_DoVerb,
    OleObject_EnumVerbs,
    OleObject_Update,
    OleObject_IsUpToDate,
    OleObject_GetUserClassID,
    OleObject_GetUserType,
    OleObject_SetExtent,
    OleObject_GetExtent,
    OleObject_Advise,
    OleObject_Unadvise,
    OleObject_EnumAdvise,
    OleObject_GetMiscStatus,
    OleObject_SetColorScheme
};

static HRESULT WINAPI PersistStreamInit_QueryInterface(IPersistStreamInit *iface, REFIID riid, void **obj)
{
    ScriptControl *This = impl_from_IPersistStreamInit(iface);
    return IScriptControl_QueryInterface(&This->IScriptControl_iface, riid, obj);
}

static ULONG WINAPI PersistStreamInit_AddRef(IPersistStreamInit *iface)
{
    ScriptControl *This = impl_from_IPersistStreamInit(iface);
    return IScriptControl_AddRef(&This->IScriptControl_iface);
}

static ULONG WINAPI PersistStreamInit_Release(IPersistStreamInit *iface)
{
    ScriptControl *This = impl_from_IPersistStreamInit(iface);
    return IScriptControl_Release(&This->IScriptControl_iface);
}

static HRESULT WINAPI PersistStreamInit_GetClassID(IPersistStreamInit *iface, CLSID *clsid)
{
    ScriptControl *This = impl_from_IPersistStreamInit(iface);

    FIXME("(%p)->(%p)\n", This, clsid);

    return E_NOTIMPL;
}

static HRESULT WINAPI PersistStreamInit_IsDirty(IPersistStreamInit *iface)
{
    ScriptControl *This = impl_from_IPersistStreamInit(iface);

    FIXME("(%p)\n", This);

    return E_NOTIMPL;
}

static HRESULT WINAPI PersistStreamInit_Load(IPersistStreamInit *iface, IStream *stream)
{
    ScriptControl *This = impl_from_IPersistStreamInit(iface);

    FIXME("(%p)->(%p)\n", This, stream);

    return E_NOTIMPL;
}

static HRESULT WINAPI PersistStreamInit_Save(IPersistStreamInit *iface, IStream *stream, BOOL clear_dirty)
{
    ScriptControl *This = impl_from_IPersistStreamInit(iface);

    FIXME("(%p)->(%p %d)\n", This, stream, clear_dirty);

    return E_NOTIMPL;
}

static HRESULT WINAPI PersistStreamInit_GetSizeMax(IPersistStreamInit *iface, ULARGE_INTEGER *size)
{
    ScriptControl *This = impl_from_IPersistStreamInit(iface);

    FIXME("(%p)->(%p)\n", This, size);

    return E_NOTIMPL;
}

static HRESULT WINAPI PersistStreamInit_InitNew(IPersistStreamInit *iface)
{
    ScriptControl *This = impl_from_IPersistStreamInit(iface);

    FIXME("(%p)\n", This);

    return S_OK;
}

static const IPersistStreamInitVtbl PersistStreamInitVtbl = {
    PersistStreamInit_QueryInterface,
    PersistStreamInit_AddRef,
    PersistStreamInit_Release,
    PersistStreamInit_GetClassID,
    PersistStreamInit_IsDirty,
    PersistStreamInit_Load,
    PersistStreamInit_Save,
    PersistStreamInit_GetSizeMax,
    PersistStreamInit_InitNew
};

static HRESULT WINAPI OleControl_QueryInterface(IOleControl *iface, REFIID riid, void **obj)
{
    ScriptControl *This = impl_from_IOleControl(iface);
    return IScriptControl_QueryInterface(&This->IScriptControl_iface, riid, obj);
}

static ULONG WINAPI OleControl_AddRef(IOleControl *iface)
{
    ScriptControl *This = impl_from_IOleControl(iface);
    return IScriptControl_AddRef(&This->IScriptControl_iface);
}

static ULONG WINAPI OleControl_Release(IOleControl *iface)
{
    ScriptControl *This = impl_from_IOleControl(iface);
    return IScriptControl_Release(&This->IScriptControl_iface);
}

static HRESULT WINAPI OleControl_GetControlInfo(IOleControl *iface, CONTROLINFO *info)
{
    ScriptControl *This = impl_from_IOleControl(iface);

    TRACE("(%p)->(%p)\n", This, info);

    if (!info)
        return E_POINTER;

    info->hAccel = NULL;
    info->cAccel = 0;

    return S_OK;
}

static HRESULT WINAPI OleControl_OnMnemonic(IOleControl *iface, MSG *msg)
{
    ScriptControl *This = impl_from_IOleControl(iface);

    FIXME("(%p)->(%p)\n", This, msg);

    return E_NOTIMPL;
}

static HRESULT WINAPI OleControl_OnAmbientPropertyChange(IOleControl *iface, DISPID dispid)
{
    ScriptControl *This = impl_from_IOleControl(iface);

    FIXME("(%p)->(%#x)\n", This, dispid);

    return E_NOTIMPL;
}

static HRESULT WINAPI OleControl_FreezeEvents(IOleControl *iface, BOOL freeze)
{
    ScriptControl *This = impl_from_IOleControl(iface);

    FIXME("(%p)->(%d)\n", This, freeze);

    return E_NOTIMPL;
}

static const IOleControlVtbl OleControlVtbl = {
    OleControl_QueryInterface,
    OleControl_AddRef,
    OleControl_Release,
    OleControl_GetControlInfo,
    OleControl_OnMnemonic,
    OleControl_OnAmbientPropertyChange,
    OleControl_FreezeEvents
};

static HRESULT WINAPI QuickActivate_QueryInterface(IQuickActivate *iface, REFIID riid, void **obj)
{
    ScriptControl *This = impl_from_IQuickActivate(iface);
    return IScriptControl_QueryInterface(&This->IScriptControl_iface, riid, obj);
}

static ULONG WINAPI QuickActivate_AddRef(IQuickActivate *iface)
{
    ScriptControl *This = impl_from_IQuickActivate(iface);
    return IScriptControl_AddRef(&This->IScriptControl_iface);
}

static ULONG WINAPI QuickActivate_Release(IQuickActivate *iface)
{
    ScriptControl *This = impl_from_IQuickActivate(iface);
    return IScriptControl_Release(&This->IScriptControl_iface);
}

static HRESULT WINAPI QuickActivate_QuickActivate(IQuickActivate *iface, QACONTAINER *container, QACONTROL *control)
{
    ScriptControl *This = impl_from_IQuickActivate(iface);

    FIXME("(%p)->(%p %p)\n", This, container, control);

    return E_NOTIMPL;
}

static HRESULT WINAPI QuickActivate_SetContentExtent(IQuickActivate *iface, SIZEL *size)
{
    ScriptControl *This = impl_from_IQuickActivate(iface);

    FIXME("(%p)->(%p)\n", This, size);

    return E_NOTIMPL;
}

static HRESULT WINAPI QuickActivate_GetContentExtent(IQuickActivate *iface, SIZEL *size)
{
    ScriptControl *This = impl_from_IQuickActivate(iface);

    FIXME("(%p)->(%p)\n", This, size);

    return E_NOTIMPL;
}

static const IQuickActivateVtbl QuickActivateVtbl = {
    QuickActivate_QueryInterface,
    QuickActivate_AddRef,
    QuickActivate_Release,
    QuickActivate_QuickActivate,
    QuickActivate_SetContentExtent,
    QuickActivate_GetContentExtent
};

static HRESULT WINAPI ViewObject_QueryInterface(IViewObjectEx *iface, REFIID riid, void **obj)
{
    ScriptControl *This = impl_from_IViewObjectEx(iface);
    return IScriptControl_QueryInterface(&This->IScriptControl_iface, riid, obj);
}

static ULONG WINAPI ViewObject_AddRef(IViewObjectEx *iface)
{
    ScriptControl *This = impl_from_IViewObjectEx(iface);
    return IScriptControl_AddRef(&This->IScriptControl_iface);
}

static ULONG WINAPI ViewObject_Release(IViewObjectEx *iface)
{
    ScriptControl *This = impl_from_IViewObjectEx(iface);
    return IScriptControl_Release(&This->IScriptControl_iface);
}

static HRESULT WINAPI ViewObject_Draw(IViewObjectEx *iface, DWORD drawaspect, LONG index, void *aspect,
    DVTARGETDEVICE *device, HDC target_dev, HDC hdc_draw, const RECTL *bounds, const RECTL *win_bounds,
    BOOL (STDMETHODCALLTYPE *fn_continue)(ULONG_PTR cont), ULONG_PTR cont)
{
    ScriptControl *This = impl_from_IViewObjectEx(iface);

    FIXME("(%p)->(%d %d %p %p %p %p %p %p %p %lu)\n", This, drawaspect, index, aspect, device, target_dev,
        hdc_draw, bounds, win_bounds, fn_continue, cont);

    return E_NOTIMPL;
}

static HRESULT WINAPI ViewObject_GetColorSet(IViewObjectEx *iface, DWORD drawaspect, LONG index, void *aspect,
    DVTARGETDEVICE *device, HDC hic_target, LOGPALETTE **colorset)
{
    ScriptControl *This = impl_from_IViewObjectEx(iface);

    FIXME("(%p)->(%d %d %p %p %p %p)\n", This, drawaspect, index, aspect, device, hic_target,
        colorset);

    return E_NOTIMPL;
}

static HRESULT WINAPI ViewObject_Freeze(IViewObjectEx *iface, DWORD drawaspect, LONG index, void *aspect,
    DWORD *freeze)
{
    ScriptControl *This = impl_from_IViewObjectEx(iface);

    FIXME("(%p)->(%d %d %p %p)\n", This, drawaspect, index, aspect, freeze);

    return E_NOTIMPL;
}

static HRESULT WINAPI ViewObject_Unfreeze(IViewObjectEx *iface, DWORD freeze)
{
    ScriptControl *This = impl_from_IViewObjectEx(iface);

    FIXME("(%p)->(%d)\n", This, freeze);

    return E_NOTIMPL;
}

static HRESULT WINAPI ViewObject_SetAdvise(IViewObjectEx *iface, DWORD aspects, DWORD flags, IAdviseSink *sink)
{
    ScriptControl *This = impl_from_IViewObjectEx(iface);

    TRACE("(%p)->(%d %#x %p)\n", This, aspects, flags, sink);

    if (aspects != DVASPECT_CONTENT)
        return DV_E_DVASPECT;

    This->view_sink_flags = flags;
    if (This->view_sink)
        IAdviseSink_Release(This->view_sink);
    This->view_sink = sink;
    if (This->view_sink)
        IAdviseSink_AddRef(This->view_sink);

    return S_OK;
}

static HRESULT WINAPI ViewObject_GetAdvise(IViewObjectEx *iface, DWORD *aspects, DWORD *flags, IAdviseSink **sink)
{
    ScriptControl *This = impl_from_IViewObjectEx(iface);

    TRACE("(%p)->(%p %p %p)\n", This, aspects, flags, sink);

    if (aspects)
        *aspects = DVASPECT_CONTENT;
    if (flags)
        *flags = This->view_sink_flags;
    if (sink) {
        *sink = This->view_sink;
        if (*sink)
            IAdviseSink_AddRef(*sink);
    }

    return S_OK;
}

static HRESULT WINAPI ViewObject_GetExtent(IViewObjectEx *iface, DWORD draw_aspect, LONG index,
    DVTARGETDEVICE *device, SIZEL *size)
{
    ScriptControl *This = impl_from_IViewObjectEx(iface);

    FIXME("(%p)->(%d %d %p %p)\n", This, draw_aspect, index, device, size);

    return E_NOTIMPL;
}

static HRESULT WINAPI ViewObject_GetRect(IViewObjectEx *iface, DWORD aspect, RECTL *rect)
{
    ScriptControl *This = impl_from_IViewObjectEx(iface);

    FIXME("(%p)->(%d %p)\n", This, aspect, rect);

    return E_NOTIMPL;
}

static HRESULT WINAPI ViewObject_GetViewStatus(IViewObjectEx *iface, DWORD *status)
{
    ScriptControl *This = impl_from_IViewObjectEx(iface);

    TRACE("(%p)->(%p)\n", This, status);

    *status = VIEWSTATUS_OPAQUE;
    return S_OK;
}

static HRESULT WINAPI ViewObject_QueryHitPoint(IViewObjectEx *iface, DWORD aspect, const RECT *bounds,
    POINT pt, LONG close_hint, DWORD *hit_result)
{
    ScriptControl *This = impl_from_IViewObjectEx(iface);

    FIXME("(%p)->(%d %s %s %d %p)\n", This, aspect, wine_dbgstr_rect(bounds), wine_dbgstr_point(&pt), close_hint, hit_result);

    return E_NOTIMPL;
}

static HRESULT WINAPI ViewObject_QueryHitRect(IViewObjectEx *iface, DWORD aspect, const RECT *bounds,
    const RECT *loc, LONG close_hint, DWORD *hit_result)
{
    ScriptControl *This = impl_from_IViewObjectEx(iface);

    FIXME("(%p)->(%d %s %s %d %p)\n", This, aspect, wine_dbgstr_rect(bounds), wine_dbgstr_rect(loc), close_hint, hit_result);

    return E_NOTIMPL;
}

static HRESULT WINAPI ViewObject_GetNaturalExtent(IViewObjectEx *iface, DWORD aspect, LONG index,
    DVTARGETDEVICE *device, HDC target_hdc, DVEXTENTINFO *extent_info, SIZEL *size)
{
    ScriptControl *This = impl_from_IViewObjectEx(iface);

    FIXME("(%p)->(%d %d %p %p %p %p)\n", This, aspect, index, device, target_hdc, extent_info, size);

    return E_NOTIMPL;
}

static const IViewObjectExVtbl ViewObjectExVtbl = {
    ViewObject_QueryInterface,
    ViewObject_AddRef,
    ViewObject_Release,
    ViewObject_Draw,
    ViewObject_GetColorSet,
    ViewObject_Freeze,
    ViewObject_Unfreeze,
    ViewObject_SetAdvise,
    ViewObject_GetAdvise,
    ViewObject_GetExtent,
    ViewObject_GetRect,
    ViewObject_GetViewStatus,
    ViewObject_QueryHitPoint,
    ViewObject_QueryHitRect,
    ViewObject_GetNaturalExtent
};

static HRESULT WINAPI PointerInactive_QueryInterface(IPointerInactive *iface, REFIID riid, void **obj)
{
    ScriptControl *This = impl_from_IPointerInactive(iface);
    return IScriptControl_QueryInterface(&This->IScriptControl_iface, riid, obj);
}

static ULONG WINAPI PointerInactive_AddRef(IPointerInactive *iface)
{
    ScriptControl *This = impl_from_IPointerInactive(iface);
    return IScriptControl_AddRef(&This->IScriptControl_iface);
}

static ULONG WINAPI PointerInactive_Release(IPointerInactive *iface)
{
    ScriptControl *This = impl_from_IPointerInactive(iface);
    return IScriptControl_Release(&This->IScriptControl_iface);
}

static HRESULT WINAPI PointerInactive_GetActivationPolicy(IPointerInactive *iface, DWORD *policy)
{
    ScriptControl *This = impl_from_IPointerInactive(iface);

    TRACE("(%p)->(%p)\n", This, policy);

    if (!policy)
        return E_POINTER;

    *policy = 0;
    return S_OK;
}

static HRESULT WINAPI PointerInactive_OnInactiveMouseMove(IPointerInactive *iface, const RECT *bounds,
    LONG x, LONG y, DWORD key_state)
{
    ScriptControl *This = impl_from_IPointerInactive(iface);

    FIXME("(%p)->(%s %d %d %#x)\n", This, wine_dbgstr_rect(bounds), x, y, key_state);

    return E_NOTIMPL;
}

static HRESULT WINAPI PointerInactive_OnInactiveSetCursor(IPointerInactive *iface, const RECT *bounds,
    LONG x, LONG y, DWORD msg, BOOL set_always)
{
    ScriptControl *This = impl_from_IPointerInactive(iface);

    FIXME("(%p)->(%s %d %d %#x %d)\n", This, wine_dbgstr_rect(bounds), x, y, msg, set_always);

    return E_NOTIMPL;
}

static const IPointerInactiveVtbl PointerInactiveVtbl = {
    PointerInactive_QueryInterface,
    PointerInactive_AddRef,
    PointerInactive_Release,
    PointerInactive_GetActivationPolicy,
    PointerInactive_OnInactiveMouseMove,
    PointerInactive_OnInactiveSetCursor
};

static HRESULT WINAPI ConnectionPointContainer_QueryInterface(IConnectionPointContainer *iface, REFIID riid, void **obj)
{
    ScriptControl *This = impl_from_IConnectionPointContainer(iface);
    return IScriptControl_QueryInterface(&This->IScriptControl_iface, riid, obj);
}

static ULONG WINAPI ConnectionPointContainer_AddRef(IConnectionPointContainer *iface)
{
    ScriptControl *This = impl_from_IConnectionPointContainer(iface);
    return IScriptControl_AddRef(&This->IScriptControl_iface);
}

static ULONG WINAPI ConnectionPointContainer_Release(IConnectionPointContainer *iface)
{
    ScriptControl *This = impl_from_IConnectionPointContainer(iface);
    return IScriptControl_Release(&This->IScriptControl_iface);
}

static HRESULT WINAPI ConnectionPointContainer_EnumConnectionPoints(IConnectionPointContainer *iface, IEnumConnectionPoints **enum_points)
{
    ScriptControl *This = impl_from_IConnectionPointContainer(iface);

    FIXME("(%p)->(%p)\n", This, enum_points);

    return E_NOTIMPL;
}

static HRESULT WINAPI ConnectionPointContainer_FindConnectionPoint(IConnectionPointContainer *iface, REFIID riid, IConnectionPoint **cp)
{
    ScriptControl *This = impl_from_IConnectionPointContainer(iface);
    ConnectionPoint *iter;

    TRACE("(%p)->(%s %p)\n", This, debugstr_guid(riid), cp);

    *cp = NULL;

    for (iter = This->cp_list; iter; iter = iter->next) {
        if (IsEqualIID(iter->riid, riid))
            *cp = &iter->IConnectionPoint_iface;
    }

    if (*cp) {
        IConnectionPoint_AddRef(*cp);
        return S_OK;
    }

    FIXME("unsupported connection point %s\n", debugstr_guid(riid));
    return CONNECT_E_NOCONNECTION;
}

static const IConnectionPointContainerVtbl ConnectionPointContainerVtbl = {
    ConnectionPointContainer_QueryInterface,
    ConnectionPointContainer_AddRef,
    ConnectionPointContainer_Release,
    ConnectionPointContainer_EnumConnectionPoints,
    ConnectionPointContainer_FindConnectionPoint
};

static HRESULT WINAPI ConnectionPoint_QueryInterface(IConnectionPoint *iface,
        REFIID riid, void **ppv)
{
    ConnectionPoint *This = impl_from_IConnectionPoint(iface);

    if(IsEqualGUID(&IID_IUnknown, riid)) {
        TRACE("(%p)->(IID_IUnknown %p)\n", This, ppv);
        *ppv = &This->IConnectionPoint_iface;
    }else if(IsEqualGUID(&IID_IConnectionPoint, riid)) {
        TRACE("(%p)->(IID_IDispatch %p)\n", This, ppv);
        *ppv = &This->IConnectionPoint_iface;
    }else {
        FIXME("(%p)->(%s %p)\n", This, debugstr_guid(riid), ppv);
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown*)*ppv);
    return S_OK;
}

static ULONG WINAPI ConnectionPoint_AddRef(IConnectionPoint *iface)
{
    ConnectionPoint *This = impl_from_IConnectionPoint(iface);
    return IConnectionPointContainer_AddRef(&This->control->IConnectionPointContainer_iface);
}

static ULONG WINAPI ConnectionPoint_Release(IConnectionPoint *iface)
{
    ConnectionPoint *This = impl_from_IConnectionPoint(iface);
    return IConnectionPointContainer_Release(&This->control->IConnectionPointContainer_iface);
}

static HRESULT WINAPI ConnectionPoint_GetConnectionInterface(IConnectionPoint *iface, IID *iid)
{
    ConnectionPoint *This = impl_from_IConnectionPoint(iface);

    TRACE("(%p)->(%p)\n", This, iid);

    *iid = *This->riid;
    return S_OK;
}

static HRESULT WINAPI ConnectionPoint_GetConnectionPointContainer(IConnectionPoint *iface,
        IConnectionPointContainer **container)
{
    ConnectionPoint *This = impl_from_IConnectionPoint(iface);

    TRACE("(%p)->(%p)\n", This, container);

    if (!container)
        return E_POINTER;

    *container = &This->control->IConnectionPointContainer_iface;
    IConnectionPointContainer_AddRef(*container);

    return S_OK;
}

static HRESULT WINAPI ConnectionPoint_Advise(IConnectionPoint *iface, IUnknown *unk_sink,
        DWORD *cookie)
{
    ConnectionPoint *This = impl_from_IConnectionPoint(iface);

    FIXME("(%p)->(%p %p)\n", This, unk_sink, cookie);

    return E_NOTIMPL;
}

static HRESULT WINAPI ConnectionPoint_Unadvise(IConnectionPoint *iface, DWORD cookie)
{
    ConnectionPoint *This = impl_from_IConnectionPoint(iface);

    FIXME("(%p)->(%d)\n", This, cookie);

    return E_NOTIMPL;
}

static HRESULT WINAPI ConnectionPoint_EnumConnections(IConnectionPoint *iface,
        IEnumConnections **ppEnum)
{
    ConnectionPoint *This = impl_from_IConnectionPoint(iface);

    FIXME("(%p)->(%p): stub\n", This, ppEnum);

    return E_NOTIMPL;
}

static const IConnectionPointVtbl ConnectionPointVtbl =
{
    ConnectionPoint_QueryInterface,
    ConnectionPoint_AddRef,
    ConnectionPoint_Release,
    ConnectionPoint_GetConnectionInterface,
    ConnectionPoint_GetConnectionPointContainer,
    ConnectionPoint_Advise,
    ConnectionPoint_Unadvise,
    ConnectionPoint_EnumConnections
};

static void ConnectionPoint_Init(ConnectionPoint *cp, ScriptControl *sc, REFIID riid)
{
    cp->IConnectionPoint_iface.lpVtbl = &ConnectionPointVtbl;
    cp->control = sc;
    cp->riid = riid;

    cp->next = sc->cp_list;
    sc->cp_list = cp;
}

static HRESULT WINAPI ScriptControl_CreateInstance(IClassFactory *iface, IUnknown *outer, REFIID riid, void **ppv)
{
    ScriptControl *script_control;
    DWORD dpi_x, dpi_y;
    HRESULT hres;
    HDC hdc;

    TRACE("(%p %s %p)\n", outer, debugstr_guid(riid), ppv);

    script_control = heap_alloc_zero(sizeof(*script_control));
    if(!script_control)
        return E_OUTOFMEMORY;

    script_control->IScriptControl_iface.lpVtbl = &ScriptControlVtbl;
    script_control->IPersistStreamInit_iface.lpVtbl = &PersistStreamInitVtbl;
    script_control->IOleObject_iface.lpVtbl = &OleObjectVtbl;
    script_control->IOleControl_iface.lpVtbl = &OleControlVtbl;
    script_control->IQuickActivate_iface.lpVtbl = &QuickActivateVtbl;
    script_control->IViewObjectEx_iface.lpVtbl = &ViewObjectExVtbl;
    script_control->IPointerInactive_iface.lpVtbl = &PointerInactiveVtbl;
    script_control->IConnectionPointContainer_iface.lpVtbl = &ConnectionPointContainerVtbl;
    script_control->IScriptModuleCollection_iface.lpVtbl = &ScriptModuleCollectionVtbl;
    script_control->ref = 1;
    script_control->timeout = 10000;
    script_control->allow_ui = VARIANT_TRUE;
    script_control->use_safe_subset = VARIANT_FALSE;

    ConnectionPoint_Init(&script_control->cp_scsource, script_control, &DIID_DScriptControlSource);
    ConnectionPoint_Init(&script_control->cp_propnotif, script_control, &IID_IPropertyNotifySink);

    hdc = GetDC(0);
    dpi_x = GetDeviceCaps(hdc, LOGPIXELSX);
    dpi_y = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(0, hdc);

    script_control->extent.cx = MulDiv(38, 2540, dpi_x);
    script_control->extent.cy = MulDiv(38, 2540, dpi_y);

    hres = IScriptControl_QueryInterface(&script_control->IScriptControl_iface, riid, ppv);
    IScriptControl_Release(&script_control->IScriptControl_iface);
    return hres;
}

/******************************************************************
 *              DllMain (msscript.ocx.@)
 */
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void *reserved)
{
    TRACE("(%p %d %p)\n", instance, reason, reserved);

    switch(reason) {
    case DLL_WINE_PREATTACH:
        return FALSE;  /* prefer native version */
    case DLL_PROCESS_ATTACH:
        msscript_instance = instance;
        DisableThreadLibraryCalls(instance);
        break;
    case DLL_PROCESS_DETACH:
        if(!reserved)
            release_typelib();
        break;
    }

    return TRUE;
}

static HRESULT WINAPI ClassFactory_QueryInterface(IClassFactory *iface, REFIID riid, void **ppv)
{
    *ppv = NULL;

    if(IsEqualGUID(&IID_IUnknown, riid)) {
        TRACE("(%p)->(IID_IUnknown %p)\n", iface, ppv);
        *ppv = iface;
    }else if(IsEqualGUID(&IID_IClassFactory, riid)) {
        TRACE("(%p)->(IID_IClassFactory %p)\n", iface, ppv);
        *ppv = iface;
    }

    if(*ppv) {
        IUnknown_AddRef((IUnknown*)*ppv);
        return S_OK;
    }

    WARN("(%p)->(%s %p)\n", iface, debugstr_guid(riid), ppv);
    return E_NOINTERFACE;
}

static ULONG WINAPI ClassFactory_AddRef(IClassFactory *iface)
{
    TRACE("(%p)\n", iface);
    return 2;
}

static ULONG WINAPI ClassFactory_Release(IClassFactory *iface)
{
    TRACE("(%p)\n", iface);
    return 1;
}

static HRESULT WINAPI ClassFactory_LockServer(IClassFactory *iface, BOOL fLock)
{
    TRACE("(%p)->(%x)\n", iface, fLock);
    return S_OK;
}

static const IClassFactoryVtbl ScriptControlFactoryVtbl = {
    ClassFactory_QueryInterface,
    ClassFactory_AddRef,
    ClassFactory_Release,
    ScriptControl_CreateInstance,
    ClassFactory_LockServer
};

static IClassFactory ScriptControlFactory = { &ScriptControlFactoryVtbl };

/***********************************************************************
 *		DllGetClassObject	(msscript.ocx.@)
 */
HRESULT WINAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppv)
{
    if(IsEqualGUID(&CLSID_ScriptControl, rclsid)) {
        TRACE("(CLSID_ScriptControl %s %p)\n", debugstr_guid(riid), ppv);
        return IClassFactory_QueryInterface(&ScriptControlFactory, riid, ppv);
    }

    FIXME("%s %s %p\n", debugstr_guid(rclsid), debugstr_guid(riid), ppv);
    return CLASS_E_CLASSNOTAVAILABLE;
}

/***********************************************************************
 *          DllCanUnloadNow (msscript.ocx.@)
 */
HRESULT WINAPI DllCanUnloadNow(void)
{
    TRACE("\n");
    return S_FALSE;
}

/***********************************************************************
 *          DllRegisterServer (msscript.ocx.@)
 */
HRESULT WINAPI DllRegisterServer(void)
{
    TRACE("()\n");
    return __wine_register_resources(msscript_instance);
}

/***********************************************************************
 *          DllUnregisterServer (msscript.ocx.@)
 */
HRESULT WINAPI DllUnregisterServer(void)
{
    TRACE("()\n");
    return __wine_unregister_resources(msscript_instance);
}
