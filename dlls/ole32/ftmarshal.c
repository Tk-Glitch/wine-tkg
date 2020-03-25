/*
 *	free threaded marshaller
 *
 *  Copyright 2002  Juergen Schmied
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

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define COBJMACROS

#include "windef.h"
#include "winbase.h"
#include "objbase.h"

#include "wine/debug.h"

#include "compobj_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(ole);

typedef struct _FTMarshalImpl {
        IUnknown IUnknown_inner;
        IMarshal IMarshal_iface;
        IUnknown *outer_unk;
        LONG ref;
} FTMarshalImpl;

static inline FTMarshalImpl *impl_from_IUnknown(IUnknown *iface)
{
    return CONTAINING_RECORD(iface, FTMarshalImpl, IUnknown_inner);
}

static inline FTMarshalImpl *impl_from_IMarshal( IMarshal *iface )
{
    return CONTAINING_RECORD(iface, FTMarshalImpl, IMarshal_iface);
}

/* inner IUnknown to handle aggregation */
static HRESULT WINAPI
IiFTMUnknown_fnQueryInterface (IUnknown * iface, REFIID riid, LPVOID * ppv)
{

    FTMarshalImpl *This = impl_from_IUnknown(iface);

    TRACE ("\n");
    *ppv = NULL;

    if (IsEqualIID (&IID_IUnknown, riid))
        *ppv = &This->IUnknown_inner;
    else if (IsEqualIID (&IID_IMarshal, riid))
        *ppv = &This->IMarshal_iface;
    else {
	FIXME ("No interface for %s.\n", debugstr_guid (riid));
	return E_NOINTERFACE;
    }
    IUnknown_AddRef ((IUnknown *) * ppv);
    return S_OK;
}

static ULONG WINAPI IiFTMUnknown_fnAddRef (IUnknown * iface)
{

    FTMarshalImpl *This = impl_from_IUnknown(iface);

    TRACE ("\n");
    return InterlockedIncrement (&This->ref);
}

static ULONG WINAPI IiFTMUnknown_fnRelease (IUnknown * iface)
{

    FTMarshalImpl *This = impl_from_IUnknown(iface);

    TRACE ("\n");
    if (InterlockedDecrement (&This->ref))
	return This->ref;
    HeapFree (GetProcessHeap (), 0, This);
    return 0;
}

static const IUnknownVtbl iunkvt =
{
	IiFTMUnknown_fnQueryInterface,
	IiFTMUnknown_fnAddRef,
	IiFTMUnknown_fnRelease
};

static HRESULT WINAPI
FTMarshalImpl_QueryInterface (LPMARSHAL iface, REFIID riid, LPVOID * ppv)
{

    FTMarshalImpl *This = impl_from_IMarshal(iface);

    TRACE ("(%p)->(%s,%p)\n", This, debugstr_guid (riid), ppv);
    return IUnknown_QueryInterface(This->outer_unk, riid, ppv);
}

static ULONG WINAPI
FTMarshalImpl_AddRef (LPMARSHAL iface)
{

    FTMarshalImpl *This = impl_from_IMarshal(iface);

    TRACE ("\n");
    return IUnknown_AddRef(This->outer_unk);
}

static ULONG WINAPI
FTMarshalImpl_Release (LPMARSHAL iface)
{

    FTMarshalImpl *This = impl_from_IMarshal(iface);

    TRACE ("\n");
    return IUnknown_Release(This->outer_unk);
}

static HRESULT WINAPI
FTMarshalImpl_GetUnmarshalClass (LPMARSHAL iface, REFIID riid, void *pv, DWORD dwDestContext,
						void *pvDestContext, DWORD mshlflags, CLSID * pCid)
{
    TRACE("(%s, %p, 0x%x, %p, 0x%x, %p)\n", debugstr_guid(riid), pv,
        dwDestContext, pvDestContext, mshlflags, pCid);
    if (dwDestContext == MSHCTX_INPROC || dwDestContext == MSHCTX_CROSSCTX)
        *pCid = CLSID_InProcFreeMarshaler;
    else
        *pCid = CLSID_StdMarshal;
    return S_OK;
}

static HRESULT WINAPI
FTMarshalImpl_GetMarshalSizeMax (LPMARSHAL iface, REFIID riid, void *pv, DWORD dwDestContext,
						void *pvDestContext, DWORD mshlflags, DWORD * pSize)
{

    IMarshal *pMarshal = NULL;
    HRESULT hres;

    TRACE("(%s, %p, 0x%x, %p, 0x%x, %p)\n", debugstr_guid(riid), pv,
        dwDestContext, pvDestContext, mshlflags, pSize);

    /* if the marshalling happens inside the same process the interface pointer is
       copied between the apartments */
    if (dwDestContext == MSHCTX_INPROC || dwDestContext == MSHCTX_CROSSCTX) {
        *pSize = sizeof (mshlflags) + sizeof (pv) + sizeof (DWORD) + sizeof (GUID);
        return S_OK;
    }

    /* use the standard marshaller to handle all other cases */
    CoGetStandardMarshal (riid, pv, dwDestContext, pvDestContext, mshlflags, &pMarshal);
    hres = IMarshal_GetMarshalSizeMax (pMarshal, riid, pv, dwDestContext, pvDestContext, mshlflags, pSize);
    IMarshal_Release (pMarshal);
    return hres;
}

static HRESULT WINAPI
FTMarshalImpl_MarshalInterface (LPMARSHAL iface, IStream * pStm, REFIID riid, void *pv,
			       DWORD dwDestContext, void *pvDestContext, DWORD mshlflags)
{

    IMarshal *pMarshal = NULL;
    HRESULT hres;

    TRACE("(%p, %s, %p, 0x%x, %p, 0x%x)\n", pStm, debugstr_guid(riid), pv,
        dwDestContext, pvDestContext, mshlflags);

    /* if the marshalling happens inside the same process the interface pointer is
       copied between the apartments */
    if (dwDestContext == MSHCTX_INPROC || dwDestContext == MSHCTX_CROSSCTX) {
        void *object;
        DWORD constant = 0;
        GUID unknown_guid = { 0 };

        hres = IUnknown_QueryInterface((IUnknown *)pv, riid, &object);
        if (FAILED(hres))
            return hres;

        /* don't hold a reference to table-weak marshaled interfaces */
        if (mshlflags & MSHLFLAGS_TABLEWEAK)
            IUnknown_Release((IUnknown *)object);

        hres = IStream_Write (pStm, &mshlflags, sizeof (mshlflags), NULL);
        if (hres != S_OK) return STG_E_MEDIUMFULL;

        hres = IStream_Write (pStm, &object, sizeof (object), NULL);
        if (hres != S_OK) return STG_E_MEDIUMFULL;

        if (sizeof(object) == sizeof(DWORD))
        {
            hres = IStream_Write (pStm, &constant, sizeof (constant), NULL);
            if (hres != S_OK) return STG_E_MEDIUMFULL;
        }

        hres = IStream_Write (pStm, &unknown_guid, sizeof (unknown_guid), NULL);
        if (hres != S_OK) return STG_E_MEDIUMFULL;

        return S_OK;
    }

    /* use the standard marshaler to handle all other cases */
    CoGetStandardMarshal (riid, pv, dwDestContext, pvDestContext, mshlflags, &pMarshal);
    hres = IMarshal_MarshalInterface (pMarshal, pStm, riid, pv, dwDestContext, pvDestContext, mshlflags);
    IMarshal_Release (pMarshal);
    return hres;
}

static HRESULT WINAPI
FTMarshalImpl_UnmarshalInterface (LPMARSHAL iface, IStream * pStm, REFIID riid, void **ppv)
{
    DWORD mshlflags;
    IUnknown *object;
    DWORD constant;
    GUID unknown_guid;
    HRESULT hres;

    TRACE ("(%p, %s, %p)\n", pStm, debugstr_guid(riid), ppv);

    hres = IStream_Read (pStm, &mshlflags, sizeof (mshlflags), NULL);
    if (hres != S_OK) return STG_E_READFAULT;

    hres = IStream_Read (pStm, &object, sizeof (object), NULL);
    if (hres != S_OK) return STG_E_READFAULT;

    if (sizeof(object) == sizeof(DWORD))
    {
        hres = IStream_Read (pStm, &constant, sizeof (constant), NULL);
        if (hres != S_OK) return STG_E_READFAULT;
        if (constant != 0)
            FIXME("constant is 0x%x instead of 0\n", constant);
    }

    hres = IStream_Read (pStm, &unknown_guid, sizeof (unknown_guid), NULL);
    if (hres != S_OK) return STG_E_READFAULT;

    hres = IUnknown_QueryInterface(object, riid, ppv);
    if (!(mshlflags & (MSHLFLAGS_TABLEWEAK|MSHLFLAGS_TABLESTRONG)))
        IUnknown_Release(object);
    return hres;
}

static HRESULT WINAPI FTMarshalImpl_ReleaseMarshalData (LPMARSHAL iface, IStream * pStm)
{
    DWORD mshlflags;
    IUnknown *object;
    DWORD constant;
    GUID unknown_guid;
    HRESULT hres;

    TRACE ("(%p)\n", pStm);

    hres = IStream_Read (pStm, &mshlflags, sizeof (mshlflags), NULL);
    if (hres != S_OK) return STG_E_READFAULT;

    hres = IStream_Read (pStm, &object, sizeof (object), NULL);
    if (hres != S_OK) return STG_E_READFAULT;

    if (sizeof(object) == sizeof(DWORD))
    {
        hres = IStream_Read (pStm, &constant, sizeof (constant), NULL);
        if (hres != S_OK) return STG_E_READFAULT;
        if (constant != 0)
            FIXME("constant is 0x%x instead of 0\n", constant);
    }

    hres = IStream_Read (pStm, &unknown_guid, sizeof (unknown_guid), NULL);
    if (hres != S_OK) return STG_E_READFAULT;

    IUnknown_Release(object);
    return S_OK;
}

static HRESULT WINAPI FTMarshalImpl_DisconnectObject (LPMARSHAL iface, DWORD dwReserved)
{
    TRACE ("()\n");
    /* nothing to do */
    return S_OK;
}

static const IMarshalVtbl ftmvtbl =
{
	FTMarshalImpl_QueryInterface,
	FTMarshalImpl_AddRef,
	FTMarshalImpl_Release,
	FTMarshalImpl_GetUnmarshalClass,
	FTMarshalImpl_GetMarshalSizeMax,
	FTMarshalImpl_MarshalInterface,
	FTMarshalImpl_UnmarshalInterface,
	FTMarshalImpl_ReleaseMarshalData,
	FTMarshalImpl_DisconnectObject
};

/***********************************************************************
 *          CoCreateFreeThreadedMarshaler [OLE32.@]
 *
 * Creates a free-threaded marshaler.
 *
 * PARAMS
 *  punkOuter    [I] Optional. Outer unknown.
 *  ppunkMarshal [O] On return, the inner unknown of the created free-threaded marshaler.
 *
 * RETURNS
 *  Success: S_OK
 *  Failure: E_OUTOFMEMORY if no memory available to create object.
 *
 * NOTES
 *  Objects that ensure their state is maintained consistent when used by
 *  multiple threads and reference no single-threaded objects are known as
 *  free-threaded. The free-threaded marshaler enables these objects to be
 *  efficiently marshaled within the same process, by not creating proxies
 *  (as they aren't needed for the object to be safely used), whilst still
 *  allowing the object to be used in inter-process and inter-machine contexts.
 */
HRESULT WINAPI CoCreateFreeThreadedMarshaler (LPUNKNOWN punkOuter, LPUNKNOWN * ppunkMarshal)
{

    FTMarshalImpl *ftm;

    TRACE ("(%p %p)\n", punkOuter, ppunkMarshal);

    ftm = HeapAlloc (GetProcessHeap (), 0, sizeof (FTMarshalImpl));
    if (!ftm)
	return E_OUTOFMEMORY;

    ftm->IUnknown_inner.lpVtbl = &iunkvt;
    ftm->IMarshal_iface.lpVtbl = &ftmvtbl;
    ftm->ref = 1;
    ftm->outer_unk = punkOuter ? punkOuter : &ftm->IUnknown_inner;

    *ppunkMarshal = &ftm->IUnknown_inner;
    return S_OK;
}

static HRESULT WINAPI FTMarshalCF_QueryInterface(LPCLASSFACTORY iface,
                                                  REFIID riid, LPVOID *ppv)
{
    *ppv = NULL;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IClassFactory))
    {
        *ppv = iface;
        IClassFactory_AddRef(iface);
        return S_OK;
    }
    return E_NOINTERFACE;
}

static ULONG WINAPI FTMarshalCF_AddRef(LPCLASSFACTORY iface)
{
    return 2; /* non-heap based object */
}

static ULONG WINAPI FTMarshalCF_Release(LPCLASSFACTORY iface)
{
    return 1; /* non-heap based object */
}

static HRESULT WINAPI FTMarshalCF_CreateInstance(LPCLASSFACTORY iface,
    LPUNKNOWN pUnk, REFIID riid, LPVOID *ppv)
{
    IUnknown *pUnknown;
    HRESULT  hr;

    TRACE("(%p, %s, %p)\n", pUnk, debugstr_guid(riid), ppv);

    *ppv = NULL;

    hr = CoCreateFreeThreadedMarshaler(pUnk, &pUnknown);

    if (SUCCEEDED(hr))
    {
        hr = IUnknown_QueryInterface(pUnknown, riid, ppv);
        IUnknown_Release(pUnknown);
    }

    return hr;
}

static HRESULT WINAPI FTMarshalCF_LockServer(LPCLASSFACTORY iface, BOOL fLock)
{
    FIXME("(%d), stub!\n",fLock);
    return S_OK;
}

static const IClassFactoryVtbl FTMarshalCFVtbl =
{
    FTMarshalCF_QueryInterface,
    FTMarshalCF_AddRef,
    FTMarshalCF_Release,
    FTMarshalCF_CreateInstance,
    FTMarshalCF_LockServer
};
static const IClassFactoryVtbl *FTMarshalCF = &FTMarshalCFVtbl;

HRESULT FTMarshalCF_Create(REFIID riid, LPVOID *ppv)
{
    return IClassFactory_QueryInterface((IClassFactory *)&FTMarshalCF, riid, ppv);
}
