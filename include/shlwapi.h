/*
 * SHLWAPI.DLL functions
 *
 * Copyright (C) 2000 Juergen Schmied
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

#ifndef __WINE_SHLWAPI_H
#define __WINE_SHLWAPI_H

/* FIXME: #include <specstrings.h> */
#include <objbase.h>
#include <shtypes.h>

#ifdef __cplusplus
extern "C" {
#endif /* defined(__cplusplus) */

#include <pshpack8.h>

#ifndef NO_SHLWAPI_HTTP

HRESULT WINAPI GetAcceptLanguagesA(char *buffer, DWORD *buff_len);
HRESULT WINAPI GetAcceptLanguagesW(WCHAR *buffer, DWORD *buff_len);
#define GetAcceptLanguages WINELIB_NAME_AW(GetAcceptLanguages)

#endif /* NO_SHLWAPI_HTTP */

#ifndef NO_SHLWAPI_REG

/* Registry functions */

DWORD WINAPI SHDeleteEmptyKeyA(HKEY,LPCSTR);
DWORD WINAPI SHDeleteEmptyKeyW(HKEY,LPCWSTR);
#define SHDeleteEmptyKey WINELIB_NAME_AW(SHDeleteEmptyKey)

DWORD WINAPI SHDeleteKeyA(HKEY,LPCSTR);
DWORD WINAPI SHDeleteKeyW(HKEY,LPCWSTR);
#define SHDeleteKey WINELIB_NAME_AW(SHDeleteKey)

DWORD WINAPI SHDeleteValueA(HKEY,LPCSTR,LPCSTR);
DWORD WINAPI SHDeleteValueW(HKEY,LPCWSTR,LPCWSTR);
#define SHDeleteValue WINELIB_NAME_AW(SHDeleteValue)

DWORD WINAPI SHGetValueA(HKEY,LPCSTR,LPCSTR,LPDWORD,LPVOID,LPDWORD);
DWORD WINAPI SHGetValueW(HKEY,LPCWSTR,LPCWSTR,LPDWORD,LPVOID,LPDWORD);
#define SHGetValue WINELIB_NAME_AW(SHGetValue)

DWORD WINAPI SHSetValueA(HKEY,LPCSTR,LPCSTR,DWORD,LPCVOID,DWORD);
DWORD WINAPI SHSetValueW(HKEY,LPCWSTR,LPCWSTR,DWORD,LPCVOID,DWORD);
#define SHSetValue WINELIB_NAME_AW(SHSetValue)

DWORD WINAPI SHQueryValueExA(HKEY,LPCSTR,LPDWORD,LPDWORD,LPVOID,LPDWORD);
DWORD WINAPI SHQueryValueExW(HKEY,LPCWSTR,LPDWORD,LPDWORD,LPVOID,LPDWORD);
#define SHQueryValueEx WINELIB_NAME_AW(SHQueryValueEx)

LONG WINAPI SHEnumKeyExA(HKEY,DWORD,LPSTR,LPDWORD);
LONG WINAPI SHEnumKeyExW(HKEY,DWORD,LPWSTR,LPDWORD);
#define SHEnumKeyEx WINELIB_NAME_AW(SHEnumKeyEx)

LONG WINAPI SHEnumValueA(HKEY,DWORD,LPSTR,LPDWORD,LPDWORD,LPVOID,LPDWORD);
LONG WINAPI SHEnumValueW(HKEY,DWORD,LPWSTR,LPDWORD,LPDWORD,LPVOID,LPDWORD);
#define SHEnumValue WINELIB_NAME_AW(SHEnumValue)

LONG WINAPI SHQueryInfoKeyA(HKEY,LPDWORD,LPDWORD,LPDWORD,LPDWORD);
LONG WINAPI SHQueryInfoKeyW(HKEY,LPDWORD,LPDWORD,LPDWORD,LPDWORD);
#define SHQueryInfoKey WINELIB_NAME_AW(SHQueryInfoKey)

DWORD WINAPI SHRegGetPathA(HKEY,LPCSTR,LPCSTR,LPSTR,DWORD);
DWORD WINAPI SHRegGetPathW(HKEY,LPCWSTR,LPCWSTR,LPWSTR,DWORD);
#define SHRegGetPath WINELIB_NAME_AW(SHRegGetPath)

DWORD WINAPI SHRegSetPathA(HKEY,LPCSTR,LPCSTR,LPCSTR,DWORD);
DWORD WINAPI SHRegSetPathW(HKEY,LPCWSTR,LPCWSTR,LPCWSTR,DWORD);
#define SHRegSetPath WINELIB_NAME_AW(SHRegSetPath)

DWORD WINAPI SHCopyKeyA(HKEY,LPCSTR,HKEY,DWORD);
DWORD WINAPI SHCopyKeyW(HKEY,LPCWSTR,HKEY,DWORD);
#define SHCopyKey WINELIB_NAME_AW(SHCopyKey)

HKEY WINAPI  SHRegDuplicateHKey(HKEY);

/* SHRegGetValue flags */
typedef INT SRRF;

#define SRRF_RT_REG_NONE 0x1
#define SRRF_RT_REG_SZ 0x2
#define SRRF_RT_REG_EXPAND_SZ 0x4
#define SRRF_RT_REG_BINARY 0x8
#define SRRF_RT_REG_DWORD 0x10
#define SRRF_RT_REG_MULTI_SZ 0x20
#define SRRF_RT_REG_QWORD 0x40

#define SRRF_RT_DWORD (SRRF_RT_REG_BINARY|SRRF_RT_REG_DWORD)
#define SRRF_RT_QWORD (SRRF_RT_REG_BINARY|SRRF_RT_REG_QWORD)
#define SRRF_RT_ANY 0xffff

#define SRRF_RM_ANY 0
#define SRRF_RM_NORMAL 0x10000
#define SRRF_RM_SAFE 0x20000
#define SRRF_RM_SAFENETWORK 0x40000

#define SRRF_NOEXPAND 0x10000000
#define SRRF_ZEROONFAILURE 0x20000000
#define SRRF_NOVIRT 0x40000000

LSTATUS WINAPI SHRegGetValueA(HKEY,LPCSTR,LPCSTR,SRRF,LPDWORD,LPVOID,LPDWORD);
LSTATUS WINAPI SHRegGetValueW(HKEY,LPCWSTR,LPCWSTR,SRRF,LPDWORD,LPVOID,LPDWORD);
#define SHRegGetValue WINELIB_NAME_AW(SHRegGetValue)

/* Undocumented registry functions */

DWORD WINAPI SHDeleteOrphanKeyA(HKEY,LPCSTR);
DWORD WINAPI SHDeleteOrphanKeyW(HKEY,LPCWSTR);
#define SHDeleteOrphanKey WINELIB_NAME_AW(SHDeleteOrphanKey)


/* User registry functions */

typedef enum
{
  SHREGDEL_DEFAULT = 0,
  SHREGDEL_HKCU    = 0x1,
  SHREGDEL_HKLM    = 0x10,
  SHREGDEL_BOTH    = SHREGDEL_HKLM | SHREGDEL_HKCU
} SHREGDEL_FLAGS;

typedef enum
{
  SHREGENUM_DEFAULT = 0,
  SHREGENUM_HKCU    = 0x1,
  SHREGENUM_HKLM    = 0x10,
  SHREGENUM_BOTH    = SHREGENUM_HKLM | SHREGENUM_HKCU
} SHREGENUM_FLAGS;

#define SHREGSET_HKCU       0x1 /* Apply to HKCU if empty */
#define SHREGSET_FORCE_HKCU 0x2 /* Always apply to HKCU */
#define SHREGSET_HKLM       0x4 /* Apply to HKLM if empty */
#define SHREGSET_FORCE_HKLM 0x8 /* Always apply to HKLM */
#define SHREGSET_DEFAULT    (SHREGSET_FORCE_HKCU | SHREGSET_HKLM)

typedef HANDLE HUSKEY;
typedef HUSKEY *PHUSKEY;

LONG WINAPI SHRegCreateUSKeyA(LPCSTR,REGSAM,HUSKEY,PHUSKEY,DWORD);
LONG WINAPI SHRegCreateUSKeyW(LPCWSTR,REGSAM,HUSKEY,PHUSKEY,DWORD);
#define SHRegCreateUSKey WINELIB_NAME_AW(SHRegCreateUSKey)

LONG WINAPI SHRegOpenUSKeyA(LPCSTR,REGSAM,HUSKEY,PHUSKEY,BOOL);
LONG WINAPI SHRegOpenUSKeyW(LPCWSTR,REGSAM,HUSKEY,PHUSKEY,BOOL);
#define SHRegOpenUSKey WINELIB_NAME_AW(SHRegOpenUSKey)

LONG WINAPI SHRegQueryUSValueA(HUSKEY,LPCSTR,LPDWORD,LPVOID,LPDWORD,
                               BOOL,LPVOID,DWORD);
LONG WINAPI SHRegQueryUSValueW(HUSKEY,LPCWSTR,LPDWORD,LPVOID,LPDWORD,
                               BOOL,LPVOID,DWORD);
#define SHRegQueryUSValue WINELIB_NAME_AW(SHRegQueryUSValue)

LONG WINAPI SHRegWriteUSValueA(HUSKEY,LPCSTR,DWORD,LPVOID,DWORD,DWORD);
LONG WINAPI SHRegWriteUSValueW(HUSKEY,LPCWSTR,DWORD,LPVOID,DWORD,DWORD);
#define SHRegWriteUSValue WINELIB_NAME_AW(SHRegWriteUSValue)

LONG WINAPI SHRegDeleteUSValueA(HUSKEY,LPCSTR,SHREGDEL_FLAGS);
LONG WINAPI SHRegDeleteUSValueW(HUSKEY,LPCWSTR,SHREGDEL_FLAGS);
#define SHRegDeleteUSValue WINELIB_NAME_AW(SHRegDeleteUSValue)

LONG WINAPI SHRegDeleteEmptyUSKeyA(HUSKEY,LPCSTR,SHREGDEL_FLAGS);
LONG WINAPI SHRegDeleteEmptyUSKeyW(HUSKEY,LPCWSTR,SHREGDEL_FLAGS);
#define SHRegDeleteEmptyUSKey WINELIB_NAME_AW(SHRegDeleteEmptyUSKey)

LONG WINAPI SHRegEnumUSKeyA(HUSKEY,DWORD,LPSTR,LPDWORD,SHREGENUM_FLAGS);
LONG WINAPI SHRegEnumUSKeyW(HUSKEY,DWORD,LPWSTR,LPDWORD,SHREGENUM_FLAGS);
#define SHRegEnumUSKey WINELIB_NAME_AW(SHRegEnumUSKey)

LONG WINAPI SHRegEnumUSValueA(HUSKEY,DWORD,LPSTR,LPDWORD,LPDWORD,
                              LPVOID,LPDWORD,SHREGENUM_FLAGS);
LONG WINAPI SHRegEnumUSValueW(HUSKEY,DWORD,LPWSTR,LPDWORD,LPDWORD,
                              LPVOID,LPDWORD,SHREGENUM_FLAGS);
#define SHRegEnumUSValue WINELIB_NAME_AW(SHRegEnumUSValue)

LONG WINAPI SHRegQueryInfoUSKeyA(HUSKEY,LPDWORD,LPDWORD,LPDWORD,
                                 LPDWORD,SHREGENUM_FLAGS);
LONG WINAPI SHRegQueryInfoUSKeyW(HUSKEY,LPDWORD,LPDWORD,LPDWORD,
                                 LPDWORD,SHREGENUM_FLAGS);
#define SHRegQueryInfoUSKey WINELIB_NAME_AW(SHRegQueryInfoUSKey)

LONG WINAPI SHRegCloseUSKey(HUSKEY);

LONG WINAPI SHRegGetUSValueA(LPCSTR,LPCSTR,LPDWORD,LPVOID,LPDWORD,
                             BOOL,LPVOID,DWORD);
LONG WINAPI SHRegGetUSValueW(LPCWSTR,LPCWSTR,LPDWORD,LPVOID,LPDWORD,
                             BOOL,LPVOID,DWORD);
#define SHRegGetUSValue WINELIB_NAME_AW(SHRegGetUSValue)

LONG WINAPI SHRegSetUSValueA(LPCSTR,LPCSTR,DWORD,LPVOID,DWORD,DWORD);
LONG WINAPI SHRegSetUSValueW(LPCWSTR,LPCWSTR,DWORD,LPVOID,DWORD,DWORD);
#define SHRegSetUSValue WINELIB_NAME_AW(SHRegSetUSValue)

BOOL WINAPI SHRegGetBoolUSValueA(LPCSTR,LPCSTR,BOOL,BOOL);
BOOL WINAPI SHRegGetBoolUSValueW(LPCWSTR,LPCWSTR,BOOL,BOOL);
#define SHRegGetBoolUSValue WINELIB_NAME_AW(SHRegGetBoolUSValue)

int WINAPI SHRegGetIntW(HKEY,LPCWSTR,int);

/* IQueryAssociation and helpers */
enum
{
    ASSOCF_NONE                 = 0x0000,
    ASSOCF_INIT_NOREMAPCLSID    = 0x0001, /* Don't map clsid->progid */
    ASSOCF_INIT_BYEXENAME       = 0x0002, /* .exe name given */
    ASSOCF_OPEN_BYEXENAME       = 0x0002, /* Synonym */
    ASSOCF_INIT_DEFAULTTOSTAR   = 0x0004, /* Use * as base */
    ASSOCF_INIT_DEFAULTTOFOLDER = 0x0008, /* Use folder as base */
    ASSOCF_NOUSERSETTINGS       = 0x0010, /* No HKCU reads */
    ASSOCF_NOTRUNCATE           = 0x0020, /* Don't truncate return */
    ASSOCF_VERIFY               = 0x0040, /* Verify data */
    ASSOCF_REMAPRUNDLL          = 0x0080, /* Get rundll args */
    ASSOCF_NOFIXUPS             = 0x0100, /* Don't fixup errors */
    ASSOCF_IGNOREBASECLASS      = 0x0200, /* Don't read baseclass */
    ASSOCF_INIT_IGNOREUNKNOWN   = 0x0400, /* Fail for unknown progid */
    ASSOCF_INIT_FIXED_PROGID    = 0x0800, /* Used passed string as progid, don't try to map it */
    ASSOCF_IS_PROTOCOL          = 0x1000, /* Treat as protocol, that should be mapped */
    ASSOCF_INIT_FOR_FILE        = 0x2000, /* progid is for file extension association */
};

typedef DWORD ASSOCF;

typedef enum
{
    ASSOCSTR_COMMAND = 1,     /* Verb command */
    ASSOCSTR_EXECUTABLE,      /* .exe from command string */
    ASSOCSTR_FRIENDLYDOCNAME, /* Friendly doc type name */
    ASSOCSTR_FRIENDLYAPPNAME, /* Friendly .exe name */
    ASSOCSTR_NOOPEN,          /* noopen value */
    ASSOCSTR_SHELLNEWVALUE,   /* Use shellnew key */
    ASSOCSTR_DDECOMMAND,      /* DDE command template */
    ASSOCSTR_DDEIFEXEC,       /* DDE command for process create */
    ASSOCSTR_DDEAPPLICATION,  /* DDE app name */
    ASSOCSTR_DDETOPIC,        /* DDE topic */
    ASSOCSTR_INFOTIP,         /* Infotip */
    ASSOCSTR_QUICKTIP,        /* Quick infotip */
    ASSOCSTR_TILEINFO,        /* Properties for tileview */
    ASSOCSTR_CONTENTTYPE,     /* Mimetype */
    ASSOCSTR_DEFAULTICON,     /* Icon */
    ASSOCSTR_SHELLEXTENSION,  /* GUID for shell extension handler */
    ASSOCSTR_MAX
} ASSOCSTR;

typedef enum
{
    ASSOCKEY_SHELLEXECCLASS = 1, /* Key for ShellExec */
    ASSOCKEY_APP,                /* Application */
    ASSOCKEY_CLASS,              /* Progid or class */
    ASSOCKEY_BASECLASS,          /* Base class */
    ASSOCKEY_MAX
} ASSOCKEY;

typedef enum
{
    ASSOCDATA_MSIDESCRIPTOR = 1, /* Component descriptor */
    ASSOCDATA_NOACTIVATEHANDLER, /* Don't activate */
    ASSOCDATA_QUERYCLASSSTORE,   /* Look in Class Store */
    ASSOCDATA_HASPERUSERASSOC,   /* Use user association */
    ASSOCDATA_EDITFLAGS,         /* Edit flags */
    ASSOCDATA_VALUE,             /* pszExtra is value */
    ASSOCDATA_MAX
} ASSOCDATA;

typedef enum
{
    ASSOCENUM_NONE
} ASSOCENUM;

typedef struct IQueryAssociations *LPQUERYASSOCIATIONS;

#define INTERFACE IQueryAssociations
DECLARE_INTERFACE_(IQueryAssociations,IUnknown)
{
    /*** IUnknown methods ***/
    STDMETHOD_(HRESULT,QueryInterface)(THIS_ REFIID riid, void** ppvObject) PURE;
    STDMETHOD_(ULONG,AddRef)(THIS) PURE;
    STDMETHOD_(ULONG,Release)(THIS) PURE;
    /*** IQueryAssociations methods ***/
    STDMETHOD(Init)(THIS_ ASSOCF  flags, LPCWSTR  pszAssoc, HKEY  hkProgid, HWND  hwnd) PURE;
    STDMETHOD(GetString)(THIS_ ASSOCF  flags, ASSOCSTR  str, LPCWSTR  pszExtra, LPWSTR  pszOut, DWORD * pcchOut) PURE;
    STDMETHOD(GetKey)(THIS_ ASSOCF  flags, ASSOCKEY  key, LPCWSTR  pszExtra, HKEY * phkeyOut) PURE;
    STDMETHOD(GetData)(THIS_ ASSOCF  flags, ASSOCDATA  data, LPCWSTR  pszExtra, LPVOID  pvOut, DWORD * pcbOut) PURE;
    STDMETHOD(GetEnum)(THIS_ ASSOCF  flags, ASSOCENUM  assocenum, LPCWSTR  pszExtra, REFIID  riid, LPVOID * ppvOut) PURE;
};
#undef INTERFACE

#if !defined(__cplusplus) || defined(CINTERFACE)
#define IQueryAssociations_QueryInterface(p,a,b)   (p)->lpVtbl->QueryInterface(p,a,b)
#define IQueryAssociations_AddRef(p)               (p)->lpVtbl->AddRef(p)
#define IQueryAssociations_Release(p)              (p)->lpVtbl->Release(p)
#define IQueryAssociations_Init(p,a,b,c,d)         (p)->lpVtbl->Init(p,a,b,c,d)
#define IQueryAssociations_GetString(p,a,b,c,d,e)  (p)->lpVtbl->GetString(p,a,b,c,d,e)
#define IQueryAssociations_GetKey(p,a,b,c,d)       (p)->lpVtbl->GetKey(p,a,b,c,d)
#define IQueryAssociations_GetData(p,a,b,c,d,e)    (p)->lpVtbl->GetData(p,a,b,c,d,e)
#define IQueryAssociations_GetEnum(p,a,b,c,d,e)    (p)->lpVtbl->GetEnum(p,a,b,c,d,e)
#endif

HRESULT WINAPI AssocCreate(CLSID,REFIID,LPVOID*);

HRESULT WINAPI AssocQueryStringA(ASSOCF,ASSOCSTR,LPCSTR,LPCSTR,LPSTR,LPDWORD);
HRESULT WINAPI AssocQueryStringW(ASSOCF,ASSOCSTR,LPCWSTR,LPCWSTR,LPWSTR,LPDWORD);
#define AssocQueryString WINELIB_NAME_AW(AssocQueryString)

HRESULT WINAPI AssocQueryStringByKeyA(ASSOCF,ASSOCSTR,HKEY,LPCSTR,LPSTR,LPDWORD);
HRESULT WINAPI AssocQueryStringByKeyW(ASSOCF,ASSOCSTR,HKEY,LPCWSTR,LPWSTR,LPDWORD);
#define AssocQueryStringByKey WINELIB_NAME_AW(AssocQueryStringByKey)

HRESULT WINAPI AssocQueryKeyA(ASSOCF,ASSOCKEY,LPCSTR,LPCSTR,PHKEY);
HRESULT WINAPI AssocQueryKeyW(ASSOCF,ASSOCKEY,LPCWSTR,LPCWSTR,PHKEY);
#define AssocQueryKey WINELIB_NAME_AW(AssocQueryKey)

BOOL WINAPI AssocIsDangerous(LPCWSTR);

#endif /* NO_SHLWAPI_REG */

void WINAPI IUnknown_Set(IUnknown **ppunk, IUnknown *punk);
void WINAPI IUnknown_AtomicRelease(IUnknown **punk);
HRESULT WINAPI IUnknown_GetWindow(IUnknown *punk, HWND *phwnd);
HRESULT WINAPI IUnknown_SetSite(IUnknown *punk, IUnknown *punkSite);
HRESULT WINAPI IUnknown_GetSite(IUnknown *punk, REFIID riid, void **ppv);
HRESULT WINAPI IUnknown_QueryService(IUnknown *punk, REFGUID guidService, REFIID riid, void **ppvOut);

/* Path functions */
#ifndef NO_SHLWAPI_PATH

/* GetPathCharType return flags */
#define GCT_INVALID     0x0
#define GCT_LFNCHAR     0x1
#define GCT_SHORTCHAR   0x2
#define GCT_WILD        0x4
#define GCT_SEPARATOR   0x8

LPSTR  WINAPI PathAddBackslashA(LPSTR);
LPWSTR WINAPI PathAddBackslashW(LPWSTR);
#define PathAddBackslash WINELIB_NAME_AW(PathAddBackslash)

BOOL WINAPI PathAddExtensionA(LPSTR,LPCSTR);
BOOL WINAPI PathAddExtensionW(LPWSTR,LPCWSTR);
#define PathAddExtension WINELIB_NAME_AW(PathAddExtension)

BOOL WINAPI PathAppendA(LPSTR,LPCSTR);
BOOL WINAPI PathAppendW(LPWSTR,LPCWSTR);
#define PathAppend WINELIB_NAME_AW(PathAppend)

LPSTR  WINAPI PathBuildRootA(LPSTR,int);
LPWSTR WINAPI PathBuildRootW(LPWSTR,int);
#define PathBuildRoot WINELIB_NAME_AW(PathBuiltRoot)

BOOL WINAPI PathCanonicalizeA(LPSTR,LPCSTR);
BOOL WINAPI PathCanonicalizeW(LPWSTR,LPCWSTR);
#define PathCanonicalize WINELIB_NAME_AW(PathCanonicalize)

LPSTR  WINAPI PathCombineA(LPSTR,LPCSTR,LPCSTR);
LPWSTR WINAPI PathCombineW(LPWSTR,LPCWSTR,LPCWSTR);
#define PathCombine WINELIB_NAME_AW(PathCombine)

BOOL WINAPI PathCompactPathA(HDC,LPSTR,UINT);
BOOL WINAPI PathCompactPathW(HDC,LPWSTR,UINT);
#define PathCompactPath WINELIB_NAME_AW(PathCompactPath)

BOOL WINAPI PathCompactPathExA(LPSTR,LPCSTR,UINT,DWORD);
BOOL WINAPI PathCompactPathExW(LPWSTR,LPCWSTR,UINT,DWORD);
#define PathCompactPathEx WINELIB_NAME_AW(PathCompactPathEx)

int WINAPI PathCommonPrefixA(LPCSTR,LPCSTR,LPSTR);
int WINAPI PathCommonPrefixW(LPCWSTR,LPCWSTR,LPWSTR);
#define PathCommonPrefix WINELIB_NAME_AW(PathCommonPrefix)

HRESULT WINAPI PathCreateFromUrlA(LPCSTR,LPSTR,LPDWORD,DWORD);
HRESULT WINAPI PathCreateFromUrlW(LPCWSTR,LPWSTR,LPDWORD,DWORD);
#define PathCreateFromUrl WINELIB_NAME_AW(PathCreateFromUrl)

HRESULT WINAPI PathCreateFromUrlAlloc(LPCWSTR,LPWSTR*,DWORD);

BOOL WINAPI PathFileExistsA(LPCSTR);
BOOL WINAPI PathFileExistsW(LPCWSTR);
#define PathFileExists WINELIB_NAME_AW(PathFileExists)

BOOL WINAPI PathFileExistsAndAttributesA(LPCSTR,DWORD*);
BOOL WINAPI PathFileExistsAndAttributesW(LPCWSTR,DWORD*);
#define PathFileExistsAndAttributes WINELIB_NAME_AW(PathFileExistsAndAttributes)

LPSTR  WINAPI PathFindExtensionA(LPCSTR);
LPWSTR WINAPI PathFindExtensionW(LPCWSTR);
#define PathFindExtension WINELIB_NAME_AW(PathFindExtension)

LPSTR  WINAPI PathFindFileNameA(LPCSTR);
LPWSTR WINAPI PathFindFileNameW(LPCWSTR);
#define PathFindFileName WINELIB_NAME_AW(PathFindFileName)

LPSTR  WINAPI PathFindNextComponentA(LPCSTR);
LPWSTR WINAPI PathFindNextComponentW(LPCWSTR);
#define PathFindNextComponent WINELIB_NAME_AW(PathFindNextComponent)

BOOL WINAPI PathFindOnPathA(LPSTR,LPCSTR*);
BOOL WINAPI PathFindOnPathW(LPWSTR,LPCWSTR*);
#define PathFindOnPath WINELIB_NAME_AW(PathFindOnPath)

LPSTR  WINAPI PathGetArgsA(LPCSTR);
LPWSTR WINAPI PathGetArgsW(LPCWSTR);
#define PathGetArgs WINELIB_NAME_AW(PathGetArgs)

UINT WINAPI PathGetCharTypeA(UCHAR);
UINT WINAPI PathGetCharTypeW(WCHAR);
#define PathGetCharType WINELIB_NAME_AW(PathGetCharType)

int WINAPI PathGetDriveNumberA(LPCSTR);
int WINAPI PathGetDriveNumberW(LPCWSTR);
#define PathGetDriveNumber WINELIB_NAME_AW(PathGetDriveNumber)

BOOL WINAPI PathIsDirectoryA(LPCSTR);
BOOL WINAPI PathIsDirectoryW(LPCWSTR);
#define PathIsDirectory WINELIB_NAME_AW(PathIsDirectory)

BOOL WINAPI PathIsDirectoryEmptyA(LPCSTR);
BOOL WINAPI PathIsDirectoryEmptyW(LPCWSTR);
#define PathIsDirectoryEmpty WINELIB_NAME_AW(PathIsDirectoryEmpty)

BOOL WINAPI PathIsFileSpecA(LPCSTR);
BOOL WINAPI PathIsFileSpecW(LPCWSTR);
#define PathIsFileSpec WINELIB_NAME_AW(PathIsFileSpec)

BOOL WINAPI PathIsPrefixA(LPCSTR,LPCSTR);
BOOL WINAPI PathIsPrefixW(LPCWSTR,LPCWSTR);
#define PathIsPrefix WINELIB_NAME_AW(PathIsPrefix)

BOOL WINAPI PathIsRelativeA(LPCSTR);
BOOL WINAPI PathIsRelativeW(LPCWSTR);
#define PathIsRelative WINELIB_NAME_AW(PathIsRelative)

BOOL WINAPI PathIsRootA(LPCSTR);
BOOL WINAPI PathIsRootW(LPCWSTR);
#define PathIsRoot WINELIB_NAME_AW(PathIsRoot)

BOOL WINAPI PathIsSameRootA(LPCSTR,LPCSTR);
BOOL WINAPI PathIsSameRootW(LPCWSTR,LPCWSTR);
#define PathIsSameRoot WINELIB_NAME_AW(PathIsSameRoot)

BOOL WINAPI PathIsUNCA(LPCSTR);
BOOL WINAPI PathIsUNCW(LPCWSTR);
#define PathIsUNC WINELIB_NAME_AW(PathIsUNC)

BOOL WINAPI PathIsUNCServerA(LPCSTR);
BOOL WINAPI PathIsUNCServerW(LPCWSTR);
#define PathIsUNCServer WINELIB_NAME_AW(PathIsUNCServer)

BOOL WINAPI PathIsUNCServerShareA(LPCSTR);
BOOL WINAPI PathIsUNCServerShareW(LPCWSTR);
#define PathIsUNCServerShare WINELIB_NAME_AW(PathIsUNCServerShare)

BOOL WINAPI PathIsContentTypeA(LPCSTR,LPCSTR);
BOOL WINAPI PathIsContentTypeW(LPCWSTR,LPCWSTR);
#define PathIsContentType WINELIB_NAME_AW(PathIsContentType)

BOOL WINAPI PathIsURLA(LPCSTR);
BOOL WINAPI PathIsURLW(LPCWSTR);
#define PathIsURL WINELIB_NAME_AW(PathIsURL)

BOOL WINAPI PathMakePrettyA(LPSTR);
BOOL WINAPI PathMakePrettyW(LPWSTR);
#define PathMakePretty WINELIB_NAME_AW(PathMakePretty)

BOOL WINAPI PathMatchSpecA(LPCSTR,LPCSTR);
BOOL WINAPI PathMatchSpecW(LPCWSTR,LPCWSTR);
#define PathMatchSpec WINELIB_NAME_AW(PathMatchSpec)

int WINAPI PathParseIconLocationA(LPSTR);
int WINAPI PathParseIconLocationW(LPWSTR);
#define PathParseIconLocation WINELIB_NAME_AW(PathParseIconLocation)

VOID WINAPI PathQuoteSpacesA(LPSTR);
VOID WINAPI PathQuoteSpacesW(LPWSTR);
#define PathQuoteSpaces WINELIB_NAME_AW(PathQuoteSpaces)

BOOL WINAPI PathRelativePathToA(LPSTR,LPCSTR,DWORD,LPCSTR,DWORD);
BOOL WINAPI PathRelativePathToW(LPWSTR,LPCWSTR,DWORD,LPCWSTR,DWORD);
#define PathRelativePathTo WINELIB_NAME_AW(PathRelativePathTo)

VOID WINAPI PathRemoveArgsA(LPSTR);
VOID WINAPI PathRemoveArgsW(LPWSTR);
#define PathRemoveArgs WINELIB_NAME_AW(PathRemoveArgs)

LPSTR  WINAPI PathRemoveBackslashA(LPSTR);
LPWSTR WINAPI PathRemoveBackslashW(LPWSTR);
#define PathRemoveBackslash WINELIB_NAME_AW(PathRemoveBackslash)

VOID WINAPI PathRemoveBlanksA(LPSTR);
VOID WINAPI PathRemoveBlanksW(LPWSTR);
#define PathRemoveBlanks WINELIB_NAME_AW(PathRemoveBlanks)

VOID WINAPI PathRemoveExtensionA(LPSTR);
VOID WINAPI PathRemoveExtensionW(LPWSTR);
#define PathRemoveExtension WINELIB_NAME_AW(PathRemoveExtension)

BOOL WINAPI PathRemoveFileSpecA(LPSTR);
BOOL WINAPI PathRemoveFileSpecW(LPWSTR);
#define PathRemoveFileSpec WINELIB_NAME_AW(PathRemoveFileSpec)

BOOL WINAPI PathRenameExtensionA(LPSTR,LPCSTR);
BOOL WINAPI PathRenameExtensionW(LPWSTR,LPCWSTR);
#define PathRenameExtension WINELIB_NAME_AW(PathRenameExtension)

BOOL WINAPI PathSearchAndQualifyA(LPCSTR,LPSTR,UINT);
BOOL WINAPI PathSearchAndQualifyW(LPCWSTR,LPWSTR,UINT);
#define PathSearchAndQualify WINELIB_NAME_AW(PathSearchAndQualify)

VOID WINAPI PathSetDlgItemPathA(HWND,int,LPCSTR);
VOID WINAPI PathSetDlgItemPathW(HWND,int,LPCWSTR);
#define PathSetDlgItemPath WINELIB_NAME_AW(PathSetDlgItemPath)

LPSTR  WINAPI PathSkipRootA(LPCSTR);
LPWSTR WINAPI PathSkipRootW(LPCWSTR);
#define PathSkipRoot WINELIB_NAME_AW(PathSkipRoot)

VOID WINAPI PathStripPathA(LPSTR);
VOID WINAPI PathStripPathW(LPWSTR);
#define PathStripPath WINELIB_NAME_AW(PathStripPath)

BOOL WINAPI PathStripToRootA(LPSTR);
BOOL WINAPI PathStripToRootW(LPWSTR);
#define PathStripToRoot WINELIB_NAME_AW(PathStripToRoot)

VOID WINAPI PathUnquoteSpacesA(LPSTR);
VOID WINAPI PathUnquoteSpacesW(LPWSTR);
#define PathUnquoteSpaces WINELIB_NAME_AW(PathUnquoteSpaces)

BOOL WINAPI PathMakeSystemFolderA(LPCSTR);
BOOL WINAPI PathMakeSystemFolderW(LPCWSTR);
#define PathMakeSystemFolder WINELIB_NAME_AW(PathMakeSystemFolder)

BOOL WINAPI PathUnmakeSystemFolderA(LPCSTR);
BOOL WINAPI PathUnmakeSystemFolderW(LPCWSTR);
#define PathUnmakeSystemFolder WINELIB_NAME_AW(PathUnmakeSystemFolder)

BOOL WINAPI PathIsSystemFolderA(LPCSTR,DWORD);
BOOL WINAPI PathIsSystemFolderW(LPCWSTR,DWORD);
#define PathIsSystemFolder WINELIB_NAME_AW(PathIsSystemFolder)

BOOL WINAPI PathIsNetworkPathA(LPCSTR);
BOOL WINAPI PathIsNetworkPathW(LPCWSTR);
#define PathIsNetworkPath WINELIB_NAME_AW(PathIsNetworkPath)

BOOL WINAPI PathIsLFNFileSpecA(LPCSTR);
BOOL WINAPI PathIsLFNFileSpecW(LPCWSTR);
#define PathIsLFNFileSpec WINELIB_NAME_AW(PathIsLFNFileSpec)

LPCSTR WINAPI PathFindSuffixArrayA(LPCSTR,LPCSTR *,int);
LPCWSTR WINAPI PathFindSuffixArrayW(LPCWSTR,LPCWSTR *,int);
#define PathFindSuffixArray WINELIB_NAME_AW(PathFindSuffixArray)

VOID WINAPI PathUndecorateA(LPSTR);
VOID WINAPI PathUndecorateW(LPWSTR);
#define PathUndecorate WINELIB_NAME_AW(PathUndecorate)

BOOL WINAPI PathUnExpandEnvStringsA(LPCSTR,LPSTR,UINT);
BOOL WINAPI PathUnExpandEnvStringsW(LPCWSTR,LPWSTR,UINT);
#define PathUnExpandEnvStrings WINELIB_NAME_AW(PathUnExpandEnvStrings)

/* Url functions */
typedef enum {
    URL_SCHEME_INVALID     = -1,
    URL_SCHEME_UNKNOWN     =  0,
    URL_SCHEME_FTP,
    URL_SCHEME_HTTP,
    URL_SCHEME_GOPHER,
    URL_SCHEME_MAILTO,
    URL_SCHEME_NEWS,
    URL_SCHEME_NNTP,
    URL_SCHEME_TELNET,
    URL_SCHEME_WAIS,
    URL_SCHEME_FILE,
    URL_SCHEME_MK,
    URL_SCHEME_HTTPS,
    URL_SCHEME_SHELL,
    URL_SCHEME_SNEWS,
    URL_SCHEME_LOCAL,
    URL_SCHEME_JAVASCRIPT,
    URL_SCHEME_VBSCRIPT,
    URL_SCHEME_ABOUT,
    URL_SCHEME_RES,
    URL_SCHEME_MSSHELLROOTED,
    URL_SCHEME_MSSHELLIDLIST,
    URL_SCHEME_MSHELP,
    URL_SCHEME_MSSHELLDEVICE,
    URL_SCHEME_WILDCARD,
    URL_SCHEME_SEARCH_MS,
    URL_SCHEME_SEARCH,
    URL_SCHEME_KNOWNFOLDER,
    URL_SCHEME_MAXVALUE
} URL_SCHEME;

/* These are used by UrlGetPart routine */
typedef enum {
    URL_PART_NONE    = 0,
    URL_PART_SCHEME  = 1,
    URL_PART_HOSTNAME,
    URL_PART_USERNAME,
    URL_PART_PASSWORD,
    URL_PART_PORT,
    URL_PART_QUERY
} URL_PART;

#define URL_PARTFLAG_KEEPSCHEME  0x00000001

/* These are used by the UrlIs... routines */
typedef enum {
    URLIS_URL,
    URLIS_OPAQUE,
    URLIS_NOHISTORY,
    URLIS_FILEURL,
    URLIS_APPLIABLE,
    URLIS_DIRECTORY,
    URLIS_HASQUERY
} URLIS;

/* This is used by the UrlApplyScheme... routines */
#define URL_APPLY_FORCEAPPLY         0x00000008
#define URL_APPLY_GUESSFILE          0x00000004
#define URL_APPLY_GUESSSCHEME        0x00000002
#define URL_APPLY_DEFAULT            0x00000001

/* The following are used by UrlEscape..., UrlUnEscape...,
 * UrlCanonicalize..., and UrlCombine... routines
 */
#define URL_WININET_COMPATIBILITY    0x80000000
#define URL_PLUGGABLE_PROTOCOL       0x40000000
#define URL_ESCAPE_UNSAFE            0x20000000
#define URL_UNESCAPE                 0x10000000

#define URL_DONT_SIMPLIFY            0x08000000
#define URL_NO_META                  URL_DONT_SIMPLIFY
#define URL_ESCAPE_SPACES_ONLY       0x04000000
#define URL_DONT_ESCAPE_EXTRA_INFO   0x02000000
#define URL_DONT_UNESCAPE_EXTRA_INFO URL_DONT_ESCAPE_EXTRA_INFO
#define URL_BROWSER_MODE             URL_DONT_ESCAPE_EXTRA_INFO

#define URL_INTERNAL_PATH            0x00800000  /* Will escape #'s in paths */
#define URL_UNESCAPE_HIGH_ANSI_ONLY  0x00400000
#define URL_CONVERT_IF_DOSPATH       0x00200000
#define URL_UNESCAPE_INPLACE         0x00100000

#define URL_FILE_USE_PATHURL         0x00010000
#define URL_ESCAPE_AS_UTF8           0x00040000

#define URL_ESCAPE_SEGMENT_ONLY      0x00002000
#define URL_ESCAPE_PERCENT           0x00001000

HRESULT WINAPI UrlApplySchemeA(LPCSTR,LPSTR,LPDWORD,DWORD);
HRESULT WINAPI UrlApplySchemeW(LPCWSTR,LPWSTR,LPDWORD,DWORD);
#define UrlApplyScheme WINELIB_NAME_AW(UrlApplyScheme)

HRESULT WINAPI UrlCanonicalizeA(LPCSTR,LPSTR,LPDWORD,DWORD);
HRESULT WINAPI UrlCanonicalizeW(LPCWSTR,LPWSTR,LPDWORD,DWORD);
#define UrlCanonicalize WINELIB_NAME_AW(UrlCanonicalize)

HRESULT WINAPI UrlCombineA(LPCSTR,LPCSTR,LPSTR,LPDWORD,DWORD);
HRESULT WINAPI UrlCombineW(LPCWSTR,LPCWSTR,LPWSTR,LPDWORD,DWORD);
#define UrlCombine WINELIB_NAME_AW(UrlCombine)

INT WINAPI UrlCompareA(LPCSTR,LPCSTR,BOOL);
INT WINAPI UrlCompareW(LPCWSTR,LPCWSTR,BOOL);
#define UrlCompare WINELIB_NAME_AW(UrlCompare)

HRESULT WINAPI UrlEscapeA(LPCSTR,LPSTR,LPDWORD,DWORD);
HRESULT WINAPI UrlEscapeW(LPCWSTR,LPWSTR,LPDWORD,DWORD);
#define UrlEscape WINELIB_NAME_AW(UrlEscape)

#define UrlEscapeSpacesA(x,y,z) UrlCanonicalizeA(x, y, z, \
                         URL_DONT_ESCAPE_EXTRA_INFO|URL_ESCAPE_SPACES_ONLY)
#define UrlEscapeSpacesW(x,y,z) UrlCanonicalizeW(x, y, z, \
                         URL_DONT_ESCAPE_EXTRA_INFO|URL_ESCAPE_SPACES_ONLY)
#define UrlEscapeSpaces WINELIB_NAME_AW(UrlEscapeSpaces)

LPCSTR  WINAPI UrlGetLocationA(LPCSTR);
LPCWSTR WINAPI UrlGetLocationW(LPCWSTR);
#define UrlGetLocation WINELIB_NAME_AW(UrlGetLocation)

HRESULT WINAPI UrlGetPartA(LPCSTR,LPSTR,LPDWORD,DWORD,DWORD);
HRESULT WINAPI UrlGetPartW(LPCWSTR,LPWSTR,LPDWORD,DWORD,DWORD);
#define UrlGetPart WINELIB_NAME_AW(UrlGetPart)

HRESULT WINAPI HashData(const unsigned char *,DWORD,unsigned char *lpDest,DWORD);

HRESULT WINAPI UrlHashA(LPCSTR,unsigned char *,DWORD);
HRESULT WINAPI UrlHashW(LPCWSTR,unsigned char *,DWORD);
#define UrlHash WINELIB_NAME_AW(UrlHash)

BOOL    WINAPI UrlIsA(LPCSTR,URLIS);
BOOL    WINAPI UrlIsW(LPCWSTR,URLIS);
#define UrlIs WINELIB_NAME_AW(UrlIs)

BOOL    WINAPI UrlIsNoHistoryA(LPCSTR);
BOOL    WINAPI UrlIsNoHistoryW(LPCWSTR);
#define UrlIsNoHistory WINELIB_NAME_AW(UrlIsNoHistory)

BOOL    WINAPI UrlIsOpaqueA(LPCSTR);
BOOL    WINAPI UrlIsOpaqueW(LPCWSTR);
#define UrlIsOpaque WINELIB_NAME_AW(UrlIsOpaque)

#define UrlIsFileUrlA(x) UrlIsA(x, URLIS_FILEURL)
#define UrlIsFileUrlW(x) UrlIsW(x, URLIS_FILEURL)
#define UrlIsFileUrl WINELIB_NAME_AW(UrlIsFileUrl)

HRESULT WINAPI UrlUnescapeA(LPSTR,LPSTR,LPDWORD,DWORD);
HRESULT WINAPI UrlUnescapeW(LPWSTR,LPWSTR,LPDWORD,DWORD);
#define UrlUnescape WINELIB_NAME_AW(UrlUnescape)

#define UrlUnescapeInPlaceA(x,y) UrlUnescapeA(x, NULL, NULL, \
                                              y | URL_UNESCAPE_INPLACE)
#define UrlUnescapeInPlaceW(x,y) UrlUnescapeW(x, NULL, NULL, \
                                              y | URL_UNESCAPE_INPLACE)
#define UrlUnescapeInPlace WINELIB_NAME_AW(UrlUnescapeInPlace)

HRESULT WINAPI UrlCreateFromPathA(LPCSTR,LPSTR,LPDWORD,DWORD);
HRESULT WINAPI UrlCreateFromPathW(LPCWSTR,LPWSTR,LPDWORD,DWORD);
#define UrlCreateFromPath WINELIB_NAME_AW(UrlCreateFromPath)

typedef struct tagPARSEDURLA {
    DWORD cbSize;
    LPCSTR pszProtocol;
    UINT cchProtocol;
    LPCSTR pszSuffix;
    UINT cchSuffix;
    UINT nScheme;
} PARSEDURLA, *PPARSEDURLA;

typedef struct tagPARSEDURLW {
    DWORD cbSize;
    LPCWSTR pszProtocol;
    UINT cchProtocol;
    LPCWSTR pszSuffix;
    UINT cchSuffix;
    UINT nScheme;
} PARSEDURLW, *PPARSEDURLW;

HRESULT WINAPI ParseURLA(LPCSTR pszUrl, PARSEDURLA *ppu);
HRESULT WINAPI ParseURLW(LPCWSTR pszUrl, PARSEDURLW *ppu);
#define ParseURL WINELIB_NAME_AW(ParseUrl)

#endif /* NO_SHLWAPI_PATH */


/* String functions */
#ifndef NO_SHLWAPI_STRFCNS

/* StrToIntEx flags */
#define STIF_DEFAULT     __MSABI_LONG(0x0)
#define STIF_SUPPORT_HEX __MSABI_LONG(0x1)

BOOL WINAPI ChrCmpIA (WORD,WORD);
BOOL WINAPI ChrCmpIW (WCHAR,WCHAR);
#define ChrCmpI WINELIB_NAME_AW(ChrCmpI)

INT WINAPI StrCSpnA(LPCSTR,LPCSTR);
INT WINAPI StrCSpnW(LPCWSTR,LPCWSTR);
#define StrCSpn WINELIB_NAME_AW(StrCSpn)

INT WINAPI StrCSpnIA(LPCSTR,LPCSTR);
INT WINAPI StrCSpnIW(LPCWSTR,LPCWSTR);
#define StrCSpnI WINELIB_NAME_AW(StrCSpnI)

#define StrCatA lstrcatA
LPWSTR WINAPI StrCatW(LPWSTR,LPCWSTR);
#define StrCat WINELIB_NAME_AW(StrCat)

LPSTR WINAPI StrCatBuffA(LPSTR,LPCSTR,INT);
LPWSTR WINAPI StrCatBuffW(LPWSTR,LPCWSTR,INT);
#define StrCatBuff WINELIB_NAME_AW(StrCatBuff)

DWORD WINAPI StrCatChainW(LPWSTR,DWORD,DWORD,LPCWSTR);

LPSTR WINAPI StrChrA(LPCSTR,WORD);
LPWSTR WINAPI StrChrW(LPCWSTR,WCHAR);
#define StrChr WINELIB_NAME_AW(StrChr)

LPSTR WINAPI StrChrIA(LPCSTR,WORD);
LPWSTR WINAPI StrChrIW(LPCWSTR,WCHAR);
#define StrChrI WINELIB_NAME_AW(StrChrI)

#define StrCmpA lstrcmpA
int WINAPI StrCmpW(LPCWSTR,LPCWSTR);
#define StrCmp WINELIB_NAME_AW(StrCmp)

#define StrCmpIA lstrcmpiA
int WINAPI StrCmpIW(LPCWSTR,LPCWSTR);
#define StrCmpI WINELIB_NAME_AW(StrCmpI)

#define StrCpyA lstrcpyA
LPWSTR WINAPI StrCpyW(LPWSTR,LPCWSTR);
#define StrCpy WINELIB_NAME_AW(StrCpy)

#define StrCpyNA lstrcpynA
LPWSTR WINAPI StrCpyNW(LPWSTR,LPCWSTR,int);
#define StrCpyN WINELIB_NAME_AW(StrCpyN)
#define StrNCpy WINELIB_NAME_AW(StrCpyN)

INT WINAPI StrCmpLogicalW(LPCWSTR,LPCWSTR);

INT WINAPI StrCmpNA(LPCSTR,LPCSTR,INT);
INT WINAPI StrCmpNW(LPCWSTR,LPCWSTR,INT);
#define StrCmpN WINELIB_NAME_AW(StrCmpN)
#define StrNCmp WINELIB_NAME_AW(StrCmpN)

INT WINAPI StrCmpNIA(LPCSTR,LPCSTR,INT);
INT WINAPI StrCmpNIW(LPCWSTR,LPCWSTR,INT);
#define StrCmpNI WINELIB_NAME_AW(StrCmpNI)
#define StrNCmpI WINELIB_NAME_AW(StrCmpNI)

LPSTR WINAPI StrDupA(LPCSTR);
LPWSTR WINAPI StrDupW(LPCWSTR);
#define StrDup WINELIB_NAME_AW(StrDup)

HRESULT WINAPI SHStrDupA(LPCSTR,WCHAR**);
HRESULT WINAPI SHStrDupW(LPCWSTR,WCHAR**);
#define SHStrDup WINELIB_NAME_AW(SHStrDup)

LPSTR WINAPI StrFormatByteSizeA (DWORD,LPSTR,UINT);

/* A/W Pairing is broken for this function */
LPSTR WINAPI StrFormatByteSize64A (LONGLONG,LPSTR,UINT);
LPWSTR WINAPI StrFormatByteSizeW (LONGLONG,LPWSTR,UINT);
#ifndef WINE_NO_UNICODE_MACROS
#ifdef UNICODE
#define StrFormatByteSize StrFormatByteSizeW
#else
#define StrFormatByteSize StrFormatByteSize64A
#endif
#endif

LPSTR WINAPI StrFormatKBSizeA(LONGLONG,LPSTR,UINT);
LPWSTR WINAPI StrFormatKBSizeW(LONGLONG,LPWSTR,UINT);
#define StrFormatKBSize WINELIB_NAME_AW(StrFormatKBSize)

int WINAPI StrFromTimeIntervalA(LPSTR,UINT,DWORD,int);
int WINAPI StrFromTimeIntervalW(LPWSTR,UINT,DWORD,int);
#define StrFromTimeInterval WINELIB_NAME_AW(StrFromTimeInterval)

BOOL WINAPI StrIsIntlEqualA(BOOL,LPCSTR,LPCSTR,int);
BOOL WINAPI StrIsIntlEqualW(BOOL,LPCWSTR,LPCWSTR,int);
#define StrIsIntlEqual WINELIB_NAME_AW(StrIsIntlEqual)

#define StrIntlEqNA(a,b,c) StrIsIntlEqualA(TRUE,a,b,c)
#define StrIntlEqNW(a,b,c) StrIsIntlEqualW(TRUE,a,b,c)

#define StrIntlEqNIA(a,b,c) StrIsIntlEqualA(FALSE,a,b,c)
#define StrIntlEqNIW(a,b,c) StrIsIntlEqualW(FALSE,a,b,c)

LPSTR  WINAPI StrNCatA(LPSTR,LPCSTR,int);
LPWSTR WINAPI StrNCatW(LPWSTR,LPCWSTR,int);
#define StrNCat WINELIB_NAME_AW(StrNCat)
#define StrCatN WINELIB_NAME_AW(StrNCat)

LPSTR  WINAPI StrPBrkA(LPCSTR,LPCSTR);
LPWSTR WINAPI StrPBrkW(LPCWSTR,LPCWSTR);
#define StrPBrk WINELIB_NAME_AW(StrPBrk)

LPSTR  WINAPI StrRChrA(LPCSTR,LPCSTR,WORD);
LPWSTR WINAPI StrRChrW(LPCWSTR,LPCWSTR,WORD);
#define StrRChr WINELIB_NAME_AW(StrRChr)

LPSTR  WINAPI StrRChrIA(LPCSTR,LPCSTR,WORD);
LPWSTR WINAPI StrRChrIW(LPCWSTR,LPCWSTR,WORD);
#define StrRChrI WINELIB_NAME_AW(StrRChrI)

LPSTR  WINAPI StrRStrIA(LPCSTR,LPCSTR,LPCSTR);
LPWSTR WINAPI StrRStrIW(LPCWSTR,LPCWSTR,LPCWSTR);
#define StrRStrI WINELIB_NAME_AW(StrRStrI)

int WINAPI StrSpnA(LPCSTR,LPCSTR);
int WINAPI StrSpnW(LPCWSTR,LPCWSTR);
#define StrSpn WINELIB_NAME_AW(StrSpn)

LPSTR  WINAPI StrStrA(LPCSTR,LPCSTR);
LPWSTR WINAPI StrStrW(LPCWSTR,LPCWSTR);
#define StrStr WINELIB_NAME_AW(StrStr)

LPSTR  WINAPI StrStrIA(LPCSTR,LPCSTR);
LPWSTR WINAPI StrStrIW(LPCWSTR,LPCWSTR);
#define StrStrI WINELIB_NAME_AW(StrStrI)

LPWSTR WINAPI StrStrNW(LPCWSTR,LPCWSTR,UINT);
LPWSTR WINAPI StrStrNIW(LPCWSTR,LPCWSTR,UINT);

int WINAPI StrToIntA(LPCSTR);
int WINAPI StrToIntW(LPCWSTR);
#define StrToInt WINELIB_NAME_AW(StrToInt)
#define StrToLong WINELIB_NAME_AW(StrToInt)

BOOL WINAPI StrToIntExA(LPCSTR,DWORD,int*);
BOOL WINAPI StrToIntExW(LPCWSTR,DWORD,int*);
#define StrToIntEx WINELIB_NAME_AW(StrToIntEx)

BOOL WINAPI StrToInt64ExA(LPCSTR,DWORD,LONGLONG*);
BOOL WINAPI StrToInt64ExW(LPCWSTR,DWORD,LONGLONG*);
#define StrToIntEx64 WINELIB_NAME_AW(StrToIntEx64)

BOOL WINAPI StrTrimA(LPSTR,LPCSTR);
BOOL WINAPI StrTrimW(LPWSTR,LPCWSTR);
#define StrTrim WINELIB_NAME_AW(StrTrim)

INT WINAPI wvnsprintfA(LPSTR,INT,LPCSTR,__ms_va_list);
INT WINAPI wvnsprintfW(LPWSTR,INT,LPCWSTR,__ms_va_list);
#define wvnsprintf WINELIB_NAME_AW(wvnsprintf)

INT WINAPIV wnsprintfA(LPSTR,INT,LPCSTR, ...);
INT WINAPIV wnsprintfW(LPWSTR,INT,LPCWSTR, ...);
#define wnsprintf WINELIB_NAME_AW(wnsprintf)

HRESULT WINAPI SHLoadIndirectString(LPCWSTR,LPWSTR,UINT,PVOID*);

BOOL WINAPI IntlStrEqWorkerA(BOOL,LPCSTR,LPCSTR,int);
BOOL WINAPI IntlStrEqWorkerW(BOOL,LPCWSTR,LPCWSTR,int);
#define IntlStrEqWorker WINELIB_NAME_AW(IntlStrEqWorker)

#define IntlStrEqNA(s1,s2,n) IntlStrEqWorkerA(TRUE,s1,s2,n)
#define IntlStrEqNW(s1,s2,n) IntlStrEqWorkerW(TRUE,s1,s2,n)
#define IntlStrEqN WINELIB_NAME_AW(IntlStrEqN)

#define IntlStrEqNIA(s1,s2,n) IntlStrEqWorkerA(FALSE,s1,s2,n)
#define IntlStrEqNIW(s1,s2,n) IntlStrEqWorkerW(FALSE,s1,s2,n)
#define IntlStrEqNI WINELIB_NAME_AW(IntlStrEqNI)

HRESULT WINAPI StrRetToStrA(STRRET*,LPCITEMIDLIST,LPSTR*);
HRESULT WINAPI StrRetToStrW(STRRET*,LPCITEMIDLIST,LPWSTR*);
#define StrRetToStr WINELIB_NAME_AW(StrRetToStr)

HRESULT WINAPI StrRetToBufA(STRRET*,LPCITEMIDLIST,LPSTR,UINT);
HRESULT WINAPI StrRetToBufW(STRRET*,LPCITEMIDLIST,LPWSTR,UINT);
#define StrRetToBuf WINELIB_NAME_AW(StrRetToBuf)

HRESULT WINAPI StrRetToBSTR(STRRET*,LPCITEMIDLIST,BSTR*);

BOOL WINAPI IsCharSpaceA(CHAR);
BOOL WINAPI IsCharSpaceW(WCHAR);
#define IsCharSpace WINELIB_NAME_AW(IsCharSpace)

#endif /* NO_SHLWAPI_STRFCNS */


/* GDI functions */
#ifndef NO_SHLWAPI_GDI

HPALETTE WINAPI SHCreateShellPalette(HDC);

COLORREF WINAPI ColorHLSToRGB(WORD,WORD,WORD);

COLORREF WINAPI ColorAdjustLuma(COLORREF,int,BOOL);

VOID WINAPI ColorRGBToHLS(COLORREF,LPWORD,LPWORD,LPWORD);

#endif /* NO_SHLWAPI_GDI */

/* Security functions */
BOOL WINAPI IsInternetESCEnabled(void);

/* Stream functions */
#ifndef NO_SHLWAPI_STREAM

struct IStream * WINAPI SHOpenRegStreamA(HKEY,LPCSTR,LPCSTR,DWORD);
struct IStream * WINAPI SHOpenRegStreamW(HKEY,LPCWSTR,LPCWSTR,DWORD);
#define SHOpenRegStream WINELIB_NAME_AW(SHOpenRegStream2) /* Uses version 2 */

struct IStream * WINAPI SHOpenRegStream2A(HKEY,LPCSTR,LPCSTR,DWORD);
struct IStream * WINAPI SHOpenRegStream2W(HKEY,LPCWSTR,LPCWSTR,DWORD);
#define SHOpenRegStream2 WINELIB_NAME_AW(SHOpenRegStream2)

HRESULT WINAPI SHCreateStreamOnFileA(LPCSTR,DWORD,struct IStream**);
HRESULT WINAPI SHCreateStreamOnFileW(LPCWSTR,DWORD,struct IStream**);
#define SHCreateStreamOnFile WINELIB_NAME_AW(SHCreateStreamOnFile)

struct IStream * WINAPI SHCreateMemStream(const BYTE*,UINT);
HRESULT WINAPI SHCreateStreamOnFileEx(LPCWSTR,DWORD,DWORD,BOOL,struct IStream*,struct IStream**);
HRESULT WINAPI SHCreateStreamWrapper(LPBYTE,DWORD,DWORD,struct IStream**);

#endif /* NO_SHLWAPI_STREAM */

HRESULT WINAPI IStream_Reset(IStream*);
HRESULT WINAPI IStream_Size(IStream*,ULARGE_INTEGER*);

/* SHAutoComplete flags */
#define SHACF_DEFAULT               0x00000000
#define SHACF_FILESYSTEM            0x00000001
#define SHACF_URLHISTORY            0x00000002
#define SHACF_URLMRU                0x00000004
#define SHACF_URLALL                (SHACF_URLHISTORY|SHACF_URLMRU)
#define SHACF_USETAB                0x00000008
#define SHACF_FILESYS_ONLY          0x00000010
#define SHACF_FILESYS_DIRS          0x00000020
#define SHACF_AUTOSUGGEST_FORCE_ON  0x10000000
#define SHACF_AUTOSUGGEST_FORCE_OFF 0x20000000
#define SHACF_AUTOAPPEND_FORCE_ON   0x40000000
#define SHACF_AUTOAPPEND_FORCE_OFF  0x80000000

HRESULT WINAPI SHAutoComplete(HWND,DWORD);

/* Threads */
HRESULT WINAPI SHCreateThreadRef(LONG*, IUnknown**);
HRESULT WINAPI SHGetThreadRef(IUnknown**);
HRESULT WINAPI SHSetThreadRef(IUnknown*);
HRESULT WINAPI SHReleaseThreadRef(void);

/* SHCreateThread flags */
enum
{
    CTF_INSIST            = 0x00000001, /* Always call */
    CTF_THREAD_REF        = 0x00000002, /* Hold thread ref */
    CTF_PROCESS_REF       = 0x00000004, /* Hold process ref */
    CTF_COINIT_STA        = 0x00000008,
    CTF_COINIT            = 0x00000008, /* Startup COM first */
    CTF_FREELIBANDEXIT    = 0x00000010, /* Hold DLL ref */
    CTF_REF_COUNTED       = 0x00000020, /* Thread is ref counted */
    CTF_WAIT_ALLOWCOM     = 0x00000040, /* Allow marshalling */
    CTF_UNUSED            = 0x00000080,
    CTF_INHERITWOW64      = 0x00000100,
    CTF_WAIT_NO_REENTRACY = 0x00000200,
    CTF_KEYBOARD_LOCALE   = 0x00000400,
    CTF_OLEINITIALIZE     = 0x00000800,
    CTF_COINIT_MTA        = 0x00001000,
    CTF_NOADDREFLIB       = 0x00002000,
};

BOOL WINAPI SHCreateThread(LPTHREAD_START_ROUTINE,void*,DWORD,LPTHREAD_START_ROUTINE);

BOOL WINAPI SHSkipJunction(struct IBindCtx*,const CLSID*);

/* Version Information */

typedef struct _DllVersionInfo {
    DWORD cbSize;
    DWORD dwMajorVersion;
    DWORD dwMinorVersion;
    DWORD dwBuildNumber;
    DWORD dwPlatformID;
} DLLVERSIONINFO;

#define DLLVER_PLATFORM_WINDOWS 0x01 /* Win9x */
#define DLLVER_PLATFORM_NT      0x02 /* WinNT */

typedef HRESULT (CALLBACK *DLLGETVERSIONPROC)(DLLVERSIONINFO *);

#ifdef __WINESRC__
/* shouldn't be here, but is nice for type checking */
HRESULT WINAPI DllGetVersion(DLLVERSIONINFO *) DECLSPEC_HIDDEN;
#endif

typedef struct _DLLVERSIONINFO2 {
    DLLVERSIONINFO info1;
    DWORD          dwFlags;    /* Reserved */
    ULONGLONG DECLSPEC_ALIGN(8) ullVersion; /* 16 bits each for Major, Minor, Build, QFE */
} DLLVERSIONINFO2;

#define DLLVER_MAJOR_MASK 0xFFFF000000000000
#define DLLVER_MINOR_MASK 0x0000FFFF00000000
#define DLLVER_BUILD_MASK 0x00000000FFFF0000
#define DLLVER_QFE_MASK   0x000000000000FFFF

#define MAKEDLLVERULL(mjr, mnr, bld, qfe) (((ULONGLONG)(mjr)<< 48)| \
  ((ULONGLONG)(mnr)<< 32) | ((ULONGLONG)(bld)<< 16) | (ULONGLONG)(qfe))

HRESULT WINAPI DllInstall(BOOL,LPCWSTR) DECLSPEC_HIDDEN;


/* IsOS definitions */

#define OS_WIN32SORGREATER        0x00
#define OS_NT                     0x01
#define OS_WIN95ORGREATER         0x02
#define OS_NT4ORGREATER           0x03
#define OS_WIN2000ORGREATER_ALT   0x04
#define OS_WIN98ORGREATER         0x05
#define OS_WIN98_GOLD             0x06
#define OS_WIN2000ORGREATER       0x07
#define OS_WIN2000PRO             0x08
#define OS_WIN2000SERVER          0x09
#define OS_WIN2000ADVSERVER       0x0A
#define OS_WIN2000DATACENTER      0x0B
#define OS_WIN2000TERMINAL        0x0C
#define OS_EMBEDDED               0x0D
#define OS_TERMINALCLIENT         0x0E
#define OS_TERMINALREMOTEADMIN    0x0F
#define OS_WIN95_GOLD             0x10
#define OS_MEORGREATER            0x11
#define OS_XPORGREATER            0x12
#define OS_HOME                   0x13
#define OS_PROFESSIONAL           0x14
#define OS_DATACENTER             0x15
#define OS_ADVSERVER              0x16
#define OS_SERVER                 0x17
#define OS_TERMINALSERVER         0x18
#define OS_PERSONALTERMINALSERVER 0x19
#define OS_FASTUSERSWITCHING      0x1A
#define OS_WELCOMELOGONUI         0x1B
#define OS_DOMAINMEMBER           0x1C
#define OS_ANYSERVER              0x1D
#define OS_WOW6432                0x1E
#define OS_WEBSERVER              0x1F
#define OS_SMALLBUSINESSSERVER    0x20
#define OS_TABLETPC               0x21
#define OS_SERVERADMINUI          0x22
#define OS_MEDIACENTER            0x23
#define OS_APPLIANCE              0x24

BOOL WINAPI IsOS(DWORD);

/* SHSetTimerQueueTimer definitions */
#define TPS_EXECUTEIO    0x00000001
#define TPS_LONGEXECTIME 0x00000008

/* SHFormatDateTimeA/SHFormatDateTimeW flags */
#define FDTF_SHORTTIME          0x00000001
#define FDTF_SHORTDATE          0x00000002
#define FDTF_DEFAULT            (FDTF_SHORTDATE | FDTF_SHORTTIME)
#define FDTF_LONGDATE           0x00000004
#define FDTF_LONGTIME           0x00000008
#define FDTF_RELATIVE           0x00000010
#define FDTF_LTRDATE            0x00000100
#define FDTF_RTLDATE            0x00000200
#define FDTF_NOAUTOREADINGORDER 0x00000400

int WINAPI SHFormatDateTimeA(const FILETIME UNALIGNED *filetime, DWORD *flags, LPSTR buffer, UINT size);
int WINAPI SHFormatDateTimeW(const FILETIME UNALIGNED *filetime, DWORD *flags, LPWSTR buffer, UINT size);

typedef struct
{
    const IID *piid;
    DWORD dwOffset;
} QITAB, *LPQITAB;

HRESULT WINAPI QISearch(void* base, const QITAB *pqit, REFIID riid, void **ppv);

#define PLATFORM_UNKNOWN     0
#define PLATFORM_IE3         1
#define PLATFORM_BROWSERONLY 1
#define PLATFORM_INTEGRATED  2

UINT WINAPI WhichPlatform(void);

#define SHGVSPB_PERUSER          0x00000001
#define SHGVSPB_ALLUSERS         0x00000002
#define SHGVSPB_PERFOLDER        0x00000004
#define SHGVSPB_ALLFOLDERS       0x00000008
#define SHGVSPB_INHERIT          0x00000010
#define SHGVSPB_ROAM             0x00000020
#define SHGVSPB_NOAUTODEFAULTS   0x80000000
#define SHGVSPB_FOLDER           (SHGVSPB_PERUSER | SHGVSPB_PERFOLDER)
#define SHGVSPB_FOLDERNODEFAULTS (SHGVSPB_PERUSER | SHGVSPB_PERFOLDER | SHGVSPB_NOAUTODEFAULTS)
#define SHGVSPB_USERDEFAULTS     (SHGVSPB_PERUSER | SHGVSPB_ALLFOLDERS)
#define SHGVSPB_GLOBALDEFAULTS   (SHGVSPB_ALLUSERS | SHGVSPB_ALLFOLDERS)

HRESULT WINAPI SHGetViewStatePropertyBag(PCIDLIST_ABSOLUTE pidl, PCWSTR bagname, DWORD flags, REFIID riid, void **ppv);

#define ILMM_IE4 0

BOOL WINAPI SHIsLowMemoryMachine(DWORD type);

#include <poppack.h> 

#ifdef __cplusplus
} /* extern "C" */
#endif /* defined(__cplusplus) */

#endif /* __WINE_SHLWAPI_H */
