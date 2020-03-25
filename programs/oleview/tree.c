/*
 * OleView (tree.c)
 *
 * Copyright 2006 Piotr Caban
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

#include "main.h"

TREE tree;
static const WCHAR wszCLSID[] = { 'C','L','S','I','D','\\','\0' };
static const WCHAR wszInProcServer32[] = 
    { 'I','n','P','r','o','c','S','e','r','v','e','r','3','2','\0' };
static const WCHAR wszOle32dll[] = { 'o','l','e','3','2','.','d','l','l','\0' };
static const WCHAR wszOleAut32dll[] =
    { 'o','l','e','a','u','t','3','2','.','d','l','l','\0' };
static const WCHAR wszImplementedCategories[] = 
    { 'I','m','p','l','e','m','e','n','t','e','d',' ',
        'C','a','t','e','g','o','r','i','e','s','\0' };
static const WCHAR wszAppID[] = { 'A','p','p','I','D','\\','\0' };
static const WCHAR wszTypeLib[] = { 'T','y','p','e','L','i','b','\\','\0' };
static const WCHAR wszInterface[] = { 'I','n','t','e','r','f','a','c','e','\\','\0' };
static const WCHAR wszComponentCategories[] = { 'C','o','m','p','o','n','e','n','t',
    ' ','C','a','t','e','g','o','r','i','e','s','\\','\0' };
static const WCHAR wszGetPath[] = { '0','\\','w','i','n','3','2','\0' };

static LPARAM CreateITEM_INFO(INT flag, const WCHAR *info, const WCHAR *clsid, const WCHAR *path)
{
    ITEM_INFO *reg;

    reg = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(ITEM_INFO));

    reg->cFlag = flag;
    lstrcpyW(reg->info, info);
    if(clsid) lstrcpyW(reg->clsid, clsid);
    if(path) lstrcpyW(reg->path, path);

    return (LPARAM)reg;
}

void CreateInst(HTREEITEM item, WCHAR *wszMachineName)
{
    TVITEMW tvi;
    HTREEITEM hCur;
    TVINSERTSTRUCTW tvis;
    WCHAR wszTitle[MAX_LOAD_STRING];
    WCHAR wszMessage[MAX_LOAD_STRING];
    WCHAR wszFlagName[MAX_LOAD_STRING];
    WCHAR wszTreeName[MAX_LOAD_STRING];
    WCHAR wszRegPath[MAX_LOAD_STRING];
    const WCHAR wszFormat[] = { '\n','%','s',' ','(','$','%','x',')','\n','\0' };
    CLSID clsid;
    COSERVERINFO remoteInfo;
    MULTI_QI qi;
    IUnknown *obj, *unk;
    HRESULT hRes;

    memset(&tvi, 0, sizeof(TVITEMW));
    tvi.mask = TVIF_TEXT;
    tvi.hItem = item;
    tvi.cchTextMax = MAX_LOAD_STRING;
    tvi.pszText = wszTreeName;

    memset(&tvis, 0, sizeof(TVINSERTSTRUCTW));
    U(tvis).item.mask = TVIF_TEXT|TVIF_PARAM;
    U(tvis).item.cchTextMax = MAX_LOAD_STRING;
    tvis.hInsertAfter = TVI_FIRST;
    U(tvis).item.pszText = tvi.pszText;
    tvis.hParent = item;
    tvis.hInsertAfter = TVI_LAST;

    if (!SendMessageW(globals.hTree, TVM_GETITEMW, 0, (LPARAM)&tvi)) return;

    if(!tvi.lParam || ((ITEM_INFO *)tvi.lParam)->loaded
                || !(((ITEM_INFO *)tvi.lParam)->cFlag&SHOWALL)) return;

    if(FAILED(CLSIDFromString(((ITEM_INFO *)tvi.lParam)->clsid, &clsid))) return;

    if(wszMachineName)
    {
        remoteInfo.dwReserved1 = 0;
        remoteInfo.dwReserved2 = 0;
        remoteInfo.pAuthInfo = NULL;
        remoteInfo.pwszName = wszMachineName;

        qi.pIID = &IID_IUnknown;

        CoCreateInstanceEx(&clsid, NULL, globals.dwClsCtx|CLSCTX_REMOTE_SERVER,
                &remoteInfo, 1, &qi);
        hRes = qi.hr;
        obj = qi.pItf;
    }
    else hRes = CoCreateInstance(&clsid, NULL, globals.dwClsCtx,
            &IID_IUnknown, (void **)&obj);

    if(FAILED(hRes))
    {
        LoadStringW(globals.hMainInst, IDS_CGCOFAIL, wszMessage, ARRAY_SIZE(wszMessage));
        LoadStringW(globals.hMainInst, IDS_ABOUT, wszTitle, ARRAY_SIZE(wszTitle));

#define CASE_ERR(i) case i: \
    MultiByteToWideChar(CP_ACP, 0, #i, -1, wszFlagName, MAX_LOAD_STRING); \
    break

        switch(hRes)
        {
            CASE_ERR(REGDB_E_CLASSNOTREG);
            CASE_ERR(E_NOINTERFACE);
            CASE_ERR(REGDB_E_READREGDB);
            CASE_ERR(REGDB_E_KEYMISSING);
            CASE_ERR(CO_E_DLLNOTFOUND);
            CASE_ERR(CO_E_APPNOTFOUND);
            CASE_ERR(E_ACCESSDENIED);
            CASE_ERR(CO_E_ERRORINDLL);
            CASE_ERR(CO_E_APPDIDNTREG);
            CASE_ERR(CLASS_E_CLASSNOTAVAILABLE);
            default:
                LoadStringW(globals.hMainInst, IDS_ERROR_UNKN, wszFlagName, ARRAY_SIZE(wszFlagName));
        }

        wsprintfW(&wszMessage[lstrlenW(wszMessage)], wszFormat,
                wszFlagName, (unsigned)hRes);
        MessageBoxW(globals.hMainWnd, wszMessage, wszTitle, MB_OK|MB_ICONEXCLAMATION);
        return;
    }

    ((ITEM_INFO *)tvi.lParam)->loaded = 1;
    ((ITEM_INFO *)tvi.lParam)->pU = obj;

    tvi.mask = TVIF_STATE;
    tvi.state = TVIS_BOLD;
    tvi.stateMask = TVIS_BOLD;
    SendMessageW(globals.hTree, TVM_SETITEMW, 0, (LPARAM)&tvi);

    tvi.mask = TVIF_TEXT;
    hCur = (HTREEITEM)SendMessageW(globals.hTree, TVM_GETNEXTITEM,
            TVGN_CHILD, (LPARAM)tree.hI);

    while(hCur)
    {
        tvi.hItem = hCur;
        if(!SendMessageW(globals.hTree, TVM_GETITEMW, 0, (LPARAM)&tvi) || !tvi.lParam)
        {
            hCur = (HTREEITEM)SendMessageW(globals.hTree, TVM_GETNEXTITEM,
                    TVGN_NEXT, (LPARAM)hCur);
            continue;
        }

        CLSIDFromString(((ITEM_INFO *)tvi.lParam)->clsid, &clsid);
        hRes = IUnknown_QueryInterface(obj, &clsid, (void *)&unk);

        if(SUCCEEDED(hRes))
        {
            IUnknown_Release(unk);

            lstrcpyW(wszRegPath, wszInterface);
            lstrcpyW(&wszRegPath[lstrlenW(wszRegPath)], ((ITEM_INFO *)tvi.lParam)->clsid);
            U(tvis).item.lParam = CreateITEM_INFO(REGTOP|INTERFACE|REGPATH,
                    wszRegPath, ((ITEM_INFO *)tvi.lParam)->clsid, NULL);
            SendMessageW(globals.hTree, TVM_INSERTITEMW, 0, (LPARAM)&tvis);
        }
        hCur = (HTREEITEM)SendMessageW(globals.hTree, TVM_GETNEXTITEM,
                TVGN_NEXT, (LPARAM)hCur);
    }

    RefreshMenu(item);
    RefreshDetails(item);
}

void ReleaseInst(HTREEITEM item)
{
    TVITEMW tvi;
    HTREEITEM cur;
    IUnknown *pU;

    memset(&tvi, 0, sizeof(TVITEMW));
    tvi.hItem = item;
    if(!SendMessageW(globals.hTree, TVM_GETITEMW, 0, (LPARAM)&tvi) || !tvi.lParam) return;

    pU = ((ITEM_INFO *)tvi.lParam)->pU;

    if(pU) IUnknown_Release(pU);
    ((ITEM_INFO *)tvi.lParam)->loaded = 0;

    SendMessageW(globals.hTree, TVM_EXPAND, TVE_COLLAPSE, (LPARAM)item);

    cur = (HTREEITEM)SendMessageW(globals.hTree, TVM_GETNEXTITEM,
            TVGN_CHILD, (LPARAM)item);
    while(cur)
    {
        SendMessageW(globals.hTree, TVM_DELETEITEM, 0, (LPARAM)cur);
        cur = (HTREEITEM)SendMessageW(globals.hTree, TVM_GETNEXTITEM,
                TVGN_CHILD, (LPARAM)item);
    }

    tvi.mask = TVIF_CHILDREN|TVIF_STATE;
    tvi.state = 0;
    tvi.stateMask = TVIS_BOLD;
    tvi.cChildren = 1;
    SendMessageW(globals.hTree, TVM_SETITEMW, 0, (LPARAM)&tvi);
}

BOOL CreateRegPath(HTREEITEM item, WCHAR *buffer, int bufSize)
{
    TVITEMW tvi;
    int bufLen;
    BOOL ret = FALSE;

    memset(buffer, 0, bufSize * sizeof(WCHAR));
    memset(&tvi, 0, sizeof(TVITEMW));
    tvi.hItem = item;

    if (SendMessageW(globals.hTree, TVM_GETITEMW, 0, (LPARAM)&tvi))
        ret = (tvi.lParam && ((ITEM_INFO *)tvi.lParam)->cFlag & REGPATH);

    while(TRUE)
    {
        if(!SendMessageW(globals.hTree, TVM_GETITEMW, 0, (LPARAM)&tvi)) break;

        if(tvi.lParam && (((ITEM_INFO *)tvi.lParam)->cFlag & (REGPATH|REGTOP)))
        {
            bufLen = lstrlenW(((ITEM_INFO *)tvi.lParam)->info);
            memmove(&buffer[bufLen], buffer, (bufSize - bufLen) * sizeof(WCHAR));
            memcpy(buffer, ((ITEM_INFO *)tvi.lParam)->info, bufLen * sizeof(WCHAR));
        }

        if(tvi.lParam && ((ITEM_INFO *)tvi.lParam)->cFlag & REGTOP) break;

        if(!tvi.lParam) return FALSE;

        tvi.hItem = (HTREEITEM)SendMessageW(globals.hTree, TVM_GETNEXTITEM,
                TVGN_PARENT, (LPARAM)tvi.hItem);
    }
    return ret;
}

static void AddCOMandAll(void)
{
    TVINSERTSTRUCTW tvis;
    TVITEMW tvi;
    HTREEITEM curSearch;
    HKEY hKey, hCurKey, hInfo;
    WCHAR valName[MAX_LOAD_STRING];
    WCHAR buffer[MAX_LOAD_STRING];
    WCHAR wszComp[MAX_LOAD_STRING];
    LONG lenBuffer;
    int i=-1;

    memset(&tvi, 0, sizeof(TVITEMW));
    U(tvis).item.mask = TVIF_TEXT|TVIF_PARAM|TVIF_CHILDREN;
    U(tvis).item.cchTextMax = MAX_LOAD_STRING;
    U(tvis).item.cChildren = 1;
    tvis.hInsertAfter = TVI_FIRST;

    if(RegOpenKeyW(HKEY_CLASSES_ROOT, wszCLSID, &hKey) != ERROR_SUCCESS) return;

    while(TRUE)
    {
        i++;

        if(RegEnumKeyW(hKey, i, valName, ARRAY_SIZE(valName)) != ERROR_SUCCESS) break;

        if(RegOpenKeyW(hKey, valName, &hCurKey) != ERROR_SUCCESS) continue;

        lenBuffer = sizeof(WCHAR[MAX_LOAD_STRING]);
        tvis.hParent = tree.hAO;

        if(RegOpenKeyW(hCurKey, wszInProcServer32, &hInfo) == ERROR_SUCCESS)
        {
            if(RegQueryValueW(hInfo, NULL, buffer, &lenBuffer) == ERROR_SUCCESS
                    && *buffer)
                if(!memcmp(buffer, wszOle32dll, sizeof(WCHAR[9]))
                        ||!memcmp(buffer, wszOleAut32dll, sizeof(WCHAR[12])))
                    tvis.hParent = tree.hCLO;

            RegCloseKey(hInfo);
        }

        lenBuffer = sizeof(WCHAR[MAX_LOAD_STRING]);

        if(RegQueryValueW(hCurKey, NULL, buffer, &lenBuffer) == ERROR_SUCCESS && *buffer)
            U(tvis).item.pszText = buffer;
        else U(tvis).item.pszText = valName;

        U(tvis).item.lParam = CreateITEM_INFO(REGPATH|SHOWALL, valName, valName, NULL);
        if(tvis.hParent) SendMessageW(globals.hTree, TVM_INSERTITEMW, 0, (LPARAM)&tvis);

        if(RegOpenKeyW(hCurKey, wszImplementedCategories, &hInfo) == ERROR_SUCCESS)
        {
            if(RegEnumKeyW(hInfo, 0, wszComp, ARRAY_SIZE(wszComp)) != ERROR_SUCCESS) break;

            RegCloseKey(hInfo);

            if(tree.hGBCC) curSearch = (HTREEITEM)SendMessageW(globals.hTree,
                    TVM_GETNEXTITEM, TVGN_CHILD, (LPARAM)tree.hGBCC);
            else curSearch = (HTREEITEM)SendMessageW(globals.hTree,
                    TVM_GETNEXTITEM, TVGN_CHILD, (LPARAM)TVI_ROOT);

            while(curSearch)
            {
                tvi.hItem = curSearch;
                if(!SendMessageW(globals.hTree, TVM_GETITEMW, 0, (LPARAM)&tvi)) break;

                if(tvi.lParam && !lstrcmpW(((ITEM_INFO *)tvi.lParam)->info, wszComp))
                {
                    tvis.hParent = curSearch;

                    memmove(&valName[6], valName, sizeof(WCHAR[MAX_LOAD_STRING-6]));
                    memmove(valName, wszCLSID, sizeof(WCHAR[6]));
                    U(tvis).item.lParam = CreateITEM_INFO(REGTOP|REGPATH|SHOWALL,
                            valName, &valName[6], NULL);

                    SendMessageW(globals.hTree, TVM_INSERTITEMW, 0, (LPARAM)&tvis);
                    break;
                }
                curSearch = (HTREEITEM)SendMessageW(globals.hTree,
                        TVM_GETNEXTITEM, TVGN_NEXT, (LPARAM)curSearch);
            }
        }
        RegCloseKey(hCurKey);
    }
    RegCloseKey(hKey);

    SendMessageW(globals.hTree, TVM_SORTCHILDREN, FALSE, (LPARAM)tree.hCLO);
    SendMessageW(globals.hTree, TVM_SORTCHILDREN, FALSE, (LPARAM)tree.hAO);
}

static void AddApplicationID(void)
{
    TVINSERTSTRUCTW tvis;
    HKEY hKey, hCurKey;
    WCHAR valName[MAX_LOAD_STRING];
    WCHAR buffer[MAX_LOAD_STRING];
    LONG lenBuffer;
    int i=-1;

    U(tvis).item.mask = TVIF_TEXT|TVIF_PARAM;
    U(tvis).item.cchTextMax = MAX_LOAD_STRING;
    tvis.hInsertAfter = TVI_FIRST;
    tvis.hParent = tree.hAID;

    if(RegOpenKeyW(HKEY_CLASSES_ROOT, wszAppID, &hKey) != ERROR_SUCCESS) return;

    while(TRUE)
    {
        i++;

        if(RegEnumKeyW(hKey, i, valName, ARRAY_SIZE(valName)) != ERROR_SUCCESS) break;

        if(RegOpenKeyW(hKey, valName, &hCurKey) != ERROR_SUCCESS) continue;

        lenBuffer = sizeof(WCHAR[MAX_LOAD_STRING]);

        if(RegQueryValueW(hCurKey, NULL, buffer, &lenBuffer) == ERROR_SUCCESS && *buffer)
            U(tvis).item.pszText = buffer;
        else U(tvis).item.pszText = valName;

        RegCloseKey(hCurKey);

        U(tvis).item.lParam = CreateITEM_INFO(REGPATH, valName, valName, NULL);
        SendMessageW(globals.hTree, TVM_INSERTITEMW, 0, (LPARAM)&tvis);
    }
    RegCloseKey(hKey);

    SendMessageW(globals.hTree, TVM_SORTCHILDREN, FALSE, (LPARAM)tree.hAID);
}

static void AddTypeLib(void)
{
    TVINSERTSTRUCTW tvis;
    HKEY hKey, hCurKey, hInfoKey, hPath;
    WCHAR valName[MAX_LOAD_STRING];
    WCHAR valParent[MAX_LOAD_STRING];
    WCHAR buffer[MAX_LOAD_STRING];
    WCHAR wszVer[MAX_LOAD_STRING];
    WCHAR wszPath[MAX_LOAD_STRING];
    const WCHAR wszFormat[] = { ' ','(','%','s',' ','%','s',')','\0' };
    const WCHAR wszFormat2[] = { '%','s','\\','%','s','\0' };
    LONG lenBuffer;
    int i=-1, j;

    U(tvis).item.mask = TVIF_TEXT|TVIF_PARAM;
    U(tvis).item.cchTextMax = MAX_LOAD_STRING;
    tvis.hInsertAfter = TVI_FIRST;
    tvis.hParent = tree.hTL;

    if(RegOpenKeyW(HKEY_CLASSES_ROOT, wszTypeLib, &hKey) != ERROR_SUCCESS) return;

    while(TRUE)
    {
        i++;

        if(RegEnumKeyW(hKey, i, valParent, ARRAY_SIZE(valParent)) != ERROR_SUCCESS) break;

        if(RegOpenKeyW(hKey, valParent, &hCurKey) != ERROR_SUCCESS) continue;

        j = -1;
        while(TRUE)
        {
            j++;

            if(RegEnumKeyW(hCurKey, j, valName, ARRAY_SIZE(valName)) != ERROR_SUCCESS) break;

            if(RegOpenKeyW(hCurKey, valName, &hInfoKey) != ERROR_SUCCESS) continue;

            lenBuffer = sizeof(WCHAR[MAX_LOAD_STRING]);

            if(RegQueryValueW(hInfoKey, NULL, buffer, &lenBuffer) == ERROR_SUCCESS
                    && *buffer)
            {
                LoadStringW(globals.hMainInst, IDS_TL_VER, wszVer, ARRAY_SIZE(wszVer));

                wsprintfW(&buffer[lstrlenW(buffer)], wszFormat, wszVer, valName);
                U(tvis).item.pszText = buffer;

                lenBuffer = MAX_LOAD_STRING;
                RegOpenKeyW(hInfoKey, wszGetPath, &hPath);
                RegQueryValueW(hPath, NULL, wszPath, &lenBuffer);
                RegCloseKey(hPath);
            }
            else U(tvis).item.pszText = valName;

            RegCloseKey(hInfoKey);

            wsprintfW(wszVer, wszFormat2, valParent, valName);
            U(tvis).item.lParam = CreateITEM_INFO(REGPATH, wszVer, valParent, wszPath);

            SendMessageW(globals.hTree, TVM_INSERTITEMW, 0, (LPARAM)&tvis);
        }
        RegCloseKey(hCurKey);
    }

    RegCloseKey(hKey);

    SendMessageW(globals.hTree, TVM_SORTCHILDREN, FALSE, (LPARAM)tree.hTL);
}

static void AddInterfaces(void)
{
    TVINSERTSTRUCTW tvis;
    HKEY hKey, hCurKey;
    WCHAR valName[MAX_LOAD_STRING];
    WCHAR buffer[MAX_LOAD_STRING];
    LONG lenBuffer;
    int i=-1;

    U(tvis).item.mask = TVIF_TEXT|TVIF_PARAM;
    U(tvis).item.cchTextMax = MAX_LOAD_STRING;
    tvis.hInsertAfter = TVI_FIRST;
    tvis.hParent = tree.hI;

    if(RegOpenKeyW(HKEY_CLASSES_ROOT, wszInterface, &hKey) != ERROR_SUCCESS) return;

    while(TRUE)
    {
        i++;

        if(RegEnumKeyW(hKey, i, valName, ARRAY_SIZE(valName)) != ERROR_SUCCESS) break;

        if(RegOpenKeyW(hKey, valName, &hCurKey) != ERROR_SUCCESS) continue;

        lenBuffer = sizeof(WCHAR[MAX_LOAD_STRING]);

        if(RegQueryValueW(hCurKey, NULL, buffer, &lenBuffer) == ERROR_SUCCESS && *buffer)
            U(tvis).item.pszText = buffer;
        else U(tvis).item.pszText = valName;

        RegCloseKey(hCurKey);

        U(tvis).item.lParam = CreateITEM_INFO(REGPATH|INTERFACE, valName, valName, NULL);
        SendMessageW(globals.hTree, TVM_INSERTITEMW, 0, (LPARAM)&tvis);
    }

    RegCloseKey(hKey);

    SendMessageW(globals.hTree, TVM_SORTCHILDREN, FALSE, (LPARAM)tree.hI);
}

static void AddComponentCategories(void)
{
    TVINSERTSTRUCTW tvis;
    HKEY hKey, hCurKey;
    WCHAR keyName[MAX_LOAD_STRING];
    WCHAR valName[MAX_LOAD_STRING];
    WCHAR buffer[MAX_LOAD_STRING];
    LONG lenBuffer;
    DWORD lenBufferHlp;
    DWORD lenValName;
    int i=-1;

    U(tvis).item.mask = TVIF_TEXT|TVIF_PARAM|TVIF_CHILDREN;
    U(tvis).item.cchTextMax = MAX_LOAD_STRING;
    tvis.hInsertAfter = TVI_FIRST;
    if(tree.hGBCC) tvis.hParent = tree.hGBCC;
    else tvis.hParent = TVI_ROOT;
    U(tvis).item.cChildren = 1;

    if(RegOpenKeyW(HKEY_CLASSES_ROOT, wszComponentCategories, &hKey) != ERROR_SUCCESS)
        return;

    while(TRUE)
    {
        i++;

        if(RegEnumKeyW(hKey, i, keyName, ARRAY_SIZE(keyName)) != ERROR_SUCCESS) break;

        if(RegOpenKeyW(hKey, keyName, &hCurKey) != ERROR_SUCCESS) continue;

        lenBuffer = sizeof(WCHAR[MAX_LOAD_STRING]);
        lenBufferHlp = sizeof(WCHAR[MAX_LOAD_STRING]);
        lenValName = sizeof(WCHAR[MAX_LOAD_STRING]);

        if(RegQueryValueW(hCurKey, NULL, buffer, &lenBuffer) == ERROR_SUCCESS && *buffer)
            U(tvis).item.pszText = buffer;
        else if(RegEnumValueW(hCurKey, 0, valName, &lenValName, NULL, NULL,
                    (LPBYTE)buffer, &lenBufferHlp) == ERROR_SUCCESS && *buffer)
            U(tvis).item.pszText = buffer;
        else continue;

        RegCloseKey(hCurKey);

        U(tvis).item.lParam = CreateITEM_INFO(REGTOP, keyName, keyName, NULL);
        SendMessageW(globals.hTree, TVM_INSERTITEMW, 0, (LPARAM)&tvis);
    }

    RegCloseKey(hKey);

    SendMessageW(globals.hTree, TVM_SORTCHILDREN, FALSE, (LPARAM)tree.hGBCC);
}

static void AddBaseEntries(void)
{
    TVINSERTSTRUCTW tvis;
    WCHAR name[MAX_LOAD_STRING];

    U(tvis).item.mask = TVIF_TEXT|TVIF_CHILDREN|TVIF_PARAM;
    /* FIXME add TVIF_IMAGE */
    U(tvis).item.pszText = name;
    U(tvis).item.cchTextMax = MAX_LOAD_STRING;
    U(tvis).item.cChildren = 1;
    tvis.hInsertAfter = TVI_FIRST;
    tvis.hParent = TVI_ROOT;

    LoadStringW(globals.hMainInst, IDS_TREE_I, U(tvis).item.pszText,
            MAX_LOAD_STRING);
    U(tvis).item.lParam = CreateITEM_INFO(REGTOP, wszInterface, NULL, NULL);
    tree.hI = TreeView_InsertItemW(globals.hTree, &tvis);

    LoadStringW(globals.hMainInst, IDS_TREE_TL, U(tvis).item.pszText,
            MAX_LOAD_STRING);
    U(tvis).item.lParam = CreateITEM_INFO(REGTOP, wszTypeLib, NULL, NULL);
    tree.hTL = TreeView_InsertItemW(globals.hTree, &tvis);

    LoadStringW(globals.hMainInst, IDS_TREE_AID, U(tvis).item.pszText,
            MAX_LOAD_STRING);
    U(tvis).item.lParam = CreateITEM_INFO(REGTOP|REGPATH, wszAppID, NULL, NULL);
    tree.hAID = TreeView_InsertItemW(globals.hTree, &tvis);

    LoadStringW(globals.hMainInst, IDS_TREE_OC, U(tvis).item.pszText,
            MAX_LOAD_STRING);
    U(tvis).item.lParam = 0;
    tree.hOC = TreeView_InsertItemW(globals.hTree, &tvis);


    tvis.hParent = tree.hOC;
    LoadStringW(globals.hMainInst, IDS_TREE_AO, U(tvis).item.pszText,
            MAX_LOAD_STRING);
    U(tvis).item.lParam = CreateITEM_INFO(REGTOP, wszCLSID, NULL, NULL);
    tree.hAO = TreeView_InsertItemW(globals.hTree, &tvis);

    LoadStringW(globals.hMainInst, IDS_TREE_CLO, U(tvis).item.pszText,
            MAX_LOAD_STRING);
    tree.hCLO = TreeView_InsertItemW(globals.hTree, &tvis);

    LoadStringW(globals.hMainInst, IDS_TREE_O1O, U(tvis).item.pszText,
            MAX_LOAD_STRING);
    U(tvis).item.lParam = 0;
    tree.hO1O = TreeView_InsertItemW(globals.hTree, &tvis);

    LoadStringW(globals.hMainInst, IDS_TREE_GBCC, U(tvis).item.pszText,
            MAX_LOAD_STRING);
    U(tvis).item.lParam = CreateITEM_INFO(REGTOP|REGPATH,
            wszComponentCategories, NULL, NULL);
    tree.hGBCC = TreeView_InsertItemW(globals.hTree, &tvis);

    SendMessageW(globals.hTree, TVM_EXPAND, TVE_EXPAND, (LPARAM)tree.hOC);
}

void EmptyTree(void)
{
    SendMessageW(globals.hTree, TVM_DELETEITEM, 0, (LPARAM)TVI_ROOT);
}

void AddTreeEx(void)
{
    AddBaseEntries();
    AddComponentCategories();
    AddCOMandAll();
    AddApplicationID();
    AddTypeLib();
    AddInterfaces();
}

void AddTree(void)
{
    memset(&tree, 0, sizeof(TREE));
    AddComponentCategories();
    AddCOMandAll();
}

static LRESULT CALLBACK TreeProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uMsg)
    {
        case WM_CREATE:
            globals.hTree = CreateWindowW(WC_TREEVIEWW, NULL,
                    WS_CHILD|WS_VISIBLE|TVS_HASLINES|TVS_HASBUTTONS|TVS_LINESATROOT,
                    0, 0, 0, 0, hWnd, (HMENU)TREE_WINDOW, globals.hMainInst, NULL);
            AddTreeEx();
            break;
        case WM_NOTIFY:
            if((int)wParam != TREE_WINDOW) break;
            switch(((LPNMHDR)lParam)->code)
            {
                case TVN_ITEMEXPANDINGW:
                    CreateInst(((NMTREEVIEWW *)lParam)->itemNew.hItem, NULL);
                    break;
                case TVN_SELCHANGEDW:
                    RefreshMenu(((NMTREEVIEWW *)lParam)->itemNew.hItem);
                    RefreshDetails(((NMTREEVIEWW *)lParam)->itemNew.hItem);
                    break;
                case TVN_DELETEITEMW:
                {
                    NMTREEVIEWW *nm = (NMTREEVIEWW*)lParam;
                    ITEM_INFO *info = (ITEM_INFO*)nm->itemOld.lParam;

                    if (info)
                    {
                        if (info->loaded)
                            ReleaseInst(nm->itemOld.hItem);
                        HeapFree(GetProcessHeap(), 0, info);
                    }
                    break;
                }
            }
            break;
        case WM_SIZE:
            MoveWindow(globals.hTree, 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE);
            break;
        default:
            return DefWindowProcW(hWnd, uMsg, wParam, lParam);
    }
    return 0;
}

HWND CreateTreeWindow(HINSTANCE hInst)
{
    WNDCLASSW wct;
    const WCHAR wszTreeClass[] = { 'T','R','E','E','\0' };

    memset(&wct, 0, sizeof(WNDCLASSW));
    wct.lpfnWndProc = TreeProc;
    wct.lpszClassName = wszTreeClass;
    wct.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wct.hCursor       = LoadCursorW(0, (LPCWSTR)IDC_ARROW);

    if(!RegisterClassW(&wct)) return NULL;

    return CreateWindowExW(WS_EX_CLIENTEDGE, wszTreeClass, NULL, WS_CHILD|WS_VISIBLE,
            0, 0, 0, 0, globals.hPaneWnd, NULL, hInst, NULL);
}
