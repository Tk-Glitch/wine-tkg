/*
 * Copyright 2011 Jacek Caban for CodeWeavers
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
#define CONST_VTABLE

#include <initguid.h>
#include <ole2.h>
#include <dispex.h>

#include "wshom.h"
#include "wine/test.h"

DEFINE_GUID(GUID_NULL,0,0,0,0,0,0,0,0,0,0,0);

#define EXPECT_HR(hr,hr_exp) \
    ok(hr == hr_exp, "got 0x%08x, expected 0x%08x\n", hr, hr_exp)

#define test_provideclassinfo(a, b) _test_provideclassinfo((IDispatch*)a, b, __LINE__)
static void _test_provideclassinfo(IDispatch *disp, const GUID *guid, int line)
{
    IProvideClassInfo *classinfo;
    TYPEATTR *attr;
    ITypeInfo *ti;
    HRESULT hr;

    hr = IDispatch_QueryInterface(disp, &IID_IProvideClassInfo, (void **)&classinfo);
    ok_(__FILE__,line) (hr == S_OK, "Failed to get IProvideClassInfo, %#x.\n", hr);

    hr = IProvideClassInfo_GetClassInfo(classinfo, &ti);
    ok_(__FILE__,line) (hr == S_OK, "GetClassInfo() failed, %#x.\n", hr);

    hr = ITypeInfo_GetTypeAttr(ti, &attr);
    ok_(__FILE__,line) (hr == S_OK, "GetTypeAttr() failed, %#x.\n", hr);

    ok_(__FILE__,line) (IsEqualGUID(&attr->guid, guid), "Unexpected typeinfo %s, expected %s\n", wine_dbgstr_guid(&attr->guid),
        wine_dbgstr_guid(guid));

    IProvideClassInfo_Release(classinfo);
    ITypeInfo_ReleaseTypeAttr(ti, attr);
    ITypeInfo_Release(ti);
}

#define CHECK_BSTR_LENGTH(str) check_bstr_length(str, __LINE__)
static void check_bstr_length(BSTR str, int line)
{
    ok_(__FILE__, line)(SysStringLen(str) == lstrlenW(str), "Unexpected string length %u vs %u.\n",
            SysStringLen(str), lstrlenW(str));
}

static void test_wshshell(void)
{
    static const WCHAR emptyW[] = L"empty";
    static const WCHAR cmdexeW[] = L"\\cmd.exe";
    WCHAR path[MAX_PATH], path2[MAX_PATH], buf[MAX_PATH];
    IWshEnvironment *env;
    IWshExec *shexec;
    IWshShell3 *sh3;
    IDispatchEx *dispex;
    IWshCollection *coll;
    IDispatch *disp, *shortcut;
    IUnknown *shell, *unk;
    IFolderCollection *folders;
    IWshShortcut *shcut;
    ITypeInfo *ti;
    HRESULT hr;
    TYPEATTR *tattr;
    DISPPARAMS dp;
    EXCEPINFO ei;
    VARIANT arg, res, arg2;
    BSTR str, ret;
    DWORD retval, attrs;
    UINT err;

    hr = CoCreateInstance(&CLSID_WshShell, NULL, CLSCTX_INPROC_SERVER|CLSCTX_INPROC_HANDLER,
            &IID_IDispatch, (void**)&disp);
    ok(hr == S_OK, "got 0x%08x\n", hr);

    hr = IDispatch_QueryInterface(disp, &IID_IWshShell3, (void**)&shell);
    EXPECT_HR(hr, S_OK);
    test_provideclassinfo(disp, &IID_IWshShell3);

    hr = IDispatch_QueryInterface(disp, &IID_IDispatchEx, (void**)&dispex);
    EXPECT_HR(hr, E_NOINTERFACE);
    IDispatch_Release(disp);

    hr = IUnknown_QueryInterface(shell, &IID_IWshShell3, (void**)&sh3);
    EXPECT_HR(hr, S_OK);

    hr = IWshShell3_QueryInterface(sh3, &IID_IObjectWithSite, (void**)&unk);
    ok(hr == E_NOINTERFACE, "got 0x%08x\n", hr);

    hr = IWshShell3_QueryInterface(sh3, &IID_IWshShell, (void**)&unk);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    IUnknown_Release(unk);

    hr = IWshShell3_QueryInterface(sh3, &IID_IWshShell2, (void**)&unk);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    IUnknown_Release(unk);

    hr = IWshShell3_get_SpecialFolders(sh3, &coll);
    EXPECT_HR(hr, S_OK);
    test_provideclassinfo(coll, &IID_IWshCollection);

    hr = IWshCollection_QueryInterface(coll, &IID_IFolderCollection, (void**)&folders);
    EXPECT_HR(hr, E_NOINTERFACE);

    hr = IWshCollection_QueryInterface(coll, &IID_IDispatch, (void**)&disp);
    EXPECT_HR(hr, S_OK);

    hr = IDispatch_GetTypeInfo(disp, 0, 0, &ti);
    EXPECT_HR(hr, S_OK);

    hr = ITypeInfo_GetTypeAttr(ti, &tattr);
    EXPECT_HR(hr, S_OK);
    ok(IsEqualIID(&tattr->guid, &IID_IWshCollection), "got wrong type guid\n");
    ITypeInfo_ReleaseTypeAttr(ti, tattr);

    /* try to call Item() with normal IDispatch procedure */
    str = SysAllocString(L"Desktop");
    V_VT(&arg) = VT_BSTR;
    V_BSTR(&arg) = str;
    dp.rgvarg = &arg;
    dp.rgdispidNamedArgs = NULL;
    dp.cArgs = 1;
    dp.cNamedArgs = 0;
    hr = IDispatch_Invoke(disp, DISPID_VALUE, &IID_NULL, 1033, DISPATCH_PROPERTYGET, &dp, &res, &ei, &err);
    EXPECT_HR(hr, DISP_E_MEMBERNOTFOUND);

    /* try Item() directly, it returns directory path apparently */
    V_VT(&res) = VT_EMPTY;
    hr = IWshCollection_Item(coll, &arg, &res);
    EXPECT_HR(hr, S_OK);
    ok(V_VT(&res) == VT_BSTR, "got res type %d\n", V_VT(&res));
    CHECK_BSTR_LENGTH(V_BSTR(&res));
    SysFreeString(str);
    VariantClear(&res);

    /* CreateShortcut() */
    str = SysAllocString(L"file.lnk");
    hr = IWshShell3_CreateShortcut(sh3, str, &shortcut);
    EXPECT_HR(hr, S_OK);
    SysFreeString(str);
    hr = IDispatch_QueryInterface(shortcut, &IID_IWshShortcut, (void**)&shcut);
    EXPECT_HR(hr, S_OK);
    test_provideclassinfo(shortcut, &IID_IWshShortcut);

    hr = IWshShortcut_get_Arguments(shcut, NULL);
    ok(hr == E_POINTER, "got 0x%08x\n", hr);

    hr = IWshShortcut_get_IconLocation(shcut, NULL);
    ok(hr == E_POINTER, "got 0x%08x\n", hr);

    IWshShortcut_Release(shcut);
    IDispatch_Release(shortcut);

    /* ExpandEnvironmentStrings */
    hr = IWshShell3_ExpandEnvironmentStrings(sh3, NULL, NULL);
    ok(hr == E_POINTER, "got 0x%08x\n", hr);

    str = SysAllocString(L"%PATH%");
    hr = IWshShell3_ExpandEnvironmentStrings(sh3, str, NULL);
    ok(hr == E_POINTER, "got 0x%08x\n", hr);
    SysFreeString(str);

    V_VT(&arg) = VT_BSTR;
    V_BSTR(&arg) = SysAllocString(L"SYSTEM");
    hr = IWshShell3_get_Environment(sh3, &arg, &env);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    VariantClear(&arg);

    hr = IWshEnvironment_get_Item(env, NULL, NULL);
    ok(hr == E_POINTER, "got 0x%08x\n", hr);
    test_provideclassinfo(env, &IID_IWshEnvironment);

    ret = (BSTR)0x1;
    hr = IWshEnvironment_get_Item(env, NULL, &ret);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    ok(ret && !*ret, "got %p\n", ret);
    SysFreeString(ret);

    /* invalid var name */
    str = SysAllocString(L"file.lnk");
    hr = IWshEnvironment_get_Item(env, str, NULL);
    ok(hr == E_POINTER, "got 0x%08x\n", hr);

    ret = NULL;
    hr = IWshEnvironment_get_Item(env, str, &ret);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    ok(ret && *ret == 0, "got %s\n", wine_dbgstr_w(ret));
    CHECK_BSTR_LENGTH(ret);
    SysFreeString(ret);
    SysFreeString(str);

    /* valid name */
    str = SysAllocString(L"PATH");
    hr = IWshEnvironment_get_Item(env, str, &ret);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    ok(ret && *ret != 0, "got %s\n", wine_dbgstr_w(ret));
    CHECK_BSTR_LENGTH(ret);
    SysFreeString(ret);
    SysFreeString(str);

    IWshEnvironment_Release(env);

    V_VT(&arg) = VT_I2;
    V_I2(&arg) = 0;
    V_VT(&arg2) = VT_ERROR;
    V_ERROR(&arg2) = DISP_E_PARAMNOTFOUND;

    str = SysAllocString(L"notepad.exe");
    hr = IWshShell3_Run(sh3, str, &arg, &arg2, NULL);
    ok(hr == E_POINTER, "got 0x%08x\n", hr);

    retval = 10;
    hr = IWshShell3_Run(sh3, str, NULL, &arg2, &retval);
    ok(hr == E_POINTER, "got 0x%08x\n", hr);
    ok(retval == 10, "got %u\n", retval);

    retval = 10;
    hr = IWshShell3_Run(sh3, str, &arg, NULL, &retval);
    ok(hr == E_POINTER, "got 0x%08x\n", hr);
    ok(retval == 10, "got %u\n", retval);

    retval = 10;
    V_VT(&arg2) = VT_ERROR;
    V_ERROR(&arg2) = 0;
    hr = IWshShell3_Run(sh3, str, &arg, &arg2, &retval);
    ok(hr == DISP_E_TYPEMISMATCH, "got 0x%08x\n", hr);
    ok(retval == 10, "got %u\n", retval);
    SysFreeString(str);

    V_VT(&arg2) = VT_BOOL;
    V_BOOL(&arg2) = VARIANT_TRUE;

    retval = 0xdeadbeef;
    str = SysAllocString(L"cmd.exe /c rd /s /q c:\\nosuchdir");
    hr = IWshShell3_Run(sh3, str, &arg, &arg2, &retval);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    todo_wine ok(retval == ERROR_FILE_NOT_FOUND, "got %u\n", retval);
    SysFreeString(str);

    retval = 0xdeadbeef;
    str = SysAllocString(L"\"cmd.exe \" /c rd /s /q c:\\nosuchdir");
    hr = IWshShell3_Run(sh3, str, &arg, &arg2, &retval);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    todo_wine ok(retval == ERROR_FILE_NOT_FOUND, "got %u\n", retval);
    SysFreeString(str);

    GetSystemDirectoryW(path, ARRAY_SIZE(path));
    lstrcatW(path, cmdexeW);
    attrs = GetFileAttributesW(path);
    ok(attrs != INVALID_FILE_ATTRIBUTES, "cmd.exe not found\n");

    /* copy cmd.exe to a path with spaces */
    GetTempPathW(ARRAY_SIZE(path2), path2);
    lstrcatW(path2, L"wshom test dir");
    CreateDirectoryW(path2, NULL);
    lstrcatW(path2, cmdexeW);
    CopyFileW(path, path2, FALSE);

    buf[0] = '"';
    lstrcpyW(buf + 1, path2);
    buf[lstrlenW(buf)] = '"';
    lstrcpyW(buf + lstrlenW(path2) + 2, L" /c rd /s /q c:\\nosuchdir");

    retval = 0xdeadbeef;
    str = SysAllocString(buf);
    hr = IWshShell3_Run(sh3, str, &arg, &arg2, &retval);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    todo_wine ok(retval == ERROR_FILE_NOT_FOUND, "got %u\n", retval);
    SysFreeString(str);

    DeleteFileW(path2);
    path2[lstrlenW(path2) - lstrlenW(cmdexeW)] = 0;
    RemoveDirectoryW(path2);

    /* current directory */
    if (0) /* crashes on native */
        hr = IWshShell3_get_CurrentDirectory(sh3, NULL);

    str = NULL;
    hr = IWshShell3_get_CurrentDirectory(sh3, &str);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    ok(str && str[0] != 0, "got empty string\n");
    CHECK_BSTR_LENGTH(str);
    SysFreeString(str);

    hr = IWshShell3_put_CurrentDirectory(sh3, NULL);
    ok(hr == E_INVALIDARG ||
       broken(hr == HRESULT_FROM_WIN32(ERROR_NOACCESS)), "got 0x%08x\n", hr);

    str = SysAllocString(emptyW);
    hr = IWshShell3_put_CurrentDirectory(sh3, str);
    ok(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), "got 0x%08x\n", hr);
    SysFreeString(str);

    str = SysAllocString(L"deadparrot");
    hr = IWshShell3_put_CurrentDirectory(sh3, str);
    ok(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), "got 0x%08x\n", hr);
    SysFreeString(str);

    /* Exec */
    hr = IWshShell3_Exec(sh3, NULL, NULL);
    ok(hr == E_POINTER, "got 0x%08x\n", hr);

    hr = IWshShell3_Exec(sh3, NULL, &shexec);
    ok(hr == DISP_E_EXCEPTION, "got 0x%08x\n", hr);

    str = SysAllocString(emptyW);
    hr = IWshShell3_Exec(sh3, str, &shexec);
    ok(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), "got 0x%08x\n", hr);
    SysFreeString(str);

    str = SysAllocString(L"%deadbeaf% /c echo test");
    hr = IWshShell3_Exec(sh3, str, &shexec);
    ok(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), "got 0x%08x\n", hr);
    SysFreeString(str);

    str = SysAllocString(L"%ComSpec% /c echo test");
    hr = IWshShell3_Exec(sh3, str, &shexec);
    ok(hr == HRESULT_FROM_WIN32(ERROR_SUCCESS), "got 0x%08x\n", hr);
    SysFreeString(str);

    IWshCollection_Release(coll);
    IDispatch_Release(disp);
    IWshShell3_Release(sh3);
    IUnknown_Release(shell);
}

/* delete key and all its subkeys */
static DWORD delete_key(HKEY hkey)
{
    char name[MAX_PATH];
    DWORD ret;

    while (!(ret = RegEnumKeyA(hkey, 0, name, sizeof(name)))) {
        HKEY tmp;
        if (!(ret = RegOpenKeyExA(hkey, name, 0, KEY_ENUMERATE_SUB_KEYS, &tmp))) {
            ret = delete_key(tmp);
            RegCloseKey(tmp);
        }
        if (ret) break;
    }
    if (ret != ERROR_NO_MORE_ITEMS) return ret;
    RegDeleteKeyA(hkey, "");
    return 0;
}

static void test_registry(void)
{
    static const WCHAR keypathW[] = L"HKEY_CURRENT_USER\\Software\\Wine\\Test\\";
    static const WCHAR regszW[] = L"regsz";
    WCHAR pathW[MAX_PATH];
    DWORD dwvalue, type;
    VARIANT value, v;
    IWshShell3 *sh3;
    VARTYPE vartype;
    LONG bound;
    HRESULT hr;
    BSTR name;
    HKEY root;
    LONG ret;
    UINT dim;

    hr = CoCreateInstance(&CLSID_WshShell, NULL, CLSCTX_INPROC_SERVER|CLSCTX_INPROC_HANDLER,
            &IID_IWshShell3, (void**)&sh3);
    ok(hr == S_OK, "got 0x%08x\n", hr);

    /* RegRead() */
    V_VT(&value) = VT_I2;
    hr = IWshShell3_RegRead(sh3, NULL, &value);
    ok(hr == E_POINTER, "got 0x%08x\n", hr);
    ok(V_VT(&value) == VT_I2, "got %d\n", V_VT(&value));

    name = SysAllocString(L"HKEY_broken_key");
    hr = IWshShell3_RegRead(sh3, name, NULL);
    ok(hr == E_POINTER, "got 0x%08x\n", hr);
    V_VT(&value) = VT_I2;
    hr = IWshShell3_RegRead(sh3, name, &value);
    ok(hr == HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND), "got 0x%08x\n", hr);
    ok(V_VT(&value) == VT_I2, "got %d\n", V_VT(&value));
    SysFreeString(name);

    name = SysAllocString(L"HKEY_CURRENT_USERa");
    V_VT(&value) = VT_I2;
    hr = IWshShell3_RegRead(sh3, name, &value);
    ok(hr == HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND), "got 0x%08x\n", hr);
    ok(V_VT(&value) == VT_I2, "got %d\n", V_VT(&value));
    SysFreeString(name);

    ret = RegCreateKeyA(HKEY_CURRENT_USER, "Software\\Wine\\Test", &root);
    ok(ret == 0, "got %d\n", ret);

    ret = RegSetValueExA(root, "regsz", 0, REG_SZ, (const BYTE*)"foobar", 7);
    ok(ret == 0, "got %d\n", ret);

    ret = RegSetValueExA(root, "regsz2", 0, REG_SZ, (const BYTE*)"foobar\0f", 9);
    ok(ret == 0, "got %d\n", ret);

    ret = RegSetValueExA(root, "regmultisz", 0, REG_MULTI_SZ, (const BYTE*)"foo\0bar\0", 9);
    ok(ret == 0, "got %d\n", ret);

    dwvalue = 10;
    ret = RegSetValueExA(root, "regdword", 0, REG_DWORD, (const BYTE*)&dwvalue, sizeof(dwvalue));
    ok(ret == 0, "got %d\n", ret);

    dwvalue = 11;
    ret = RegSetValueExA(root, "regbinary", 0, REG_BINARY, (const BYTE*)&dwvalue, sizeof(dwvalue));
    ok(ret == 0, "got %d\n", ret);

    /* REG_SZ */
    lstrcpyW(pathW, keypathW);
    lstrcatW(pathW, regszW);
    name = SysAllocString(pathW);
    VariantInit(&value);
    hr = IWshShell3_RegRead(sh3, name, &value);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    ok(V_VT(&value) == VT_BSTR, "got %d\n", V_VT(&value));
    ok(!lstrcmpW(V_BSTR(&value), L"foobar"), "got %s\n", wine_dbgstr_w(V_BSTR(&value)));
    CHECK_BSTR_LENGTH(V_BSTR(&value));
    VariantClear(&value);
    SysFreeString(name);

    /* REG_SZ with embedded NULL */
    lstrcpyW(pathW, keypathW);
    lstrcatW(pathW, L"regsz2");
    name = SysAllocString(pathW);
    VariantInit(&value);
    hr = IWshShell3_RegRead(sh3, name, &value);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    ok(V_VT(&value) == VT_BSTR, "got %d\n", V_VT(&value));
    ok(SysStringLen(V_BSTR(&value)) == 6, "len %d\n", SysStringLen(V_BSTR(&value)));
    CHECK_BSTR_LENGTH(V_BSTR(&value));
    VariantClear(&value);
    SysFreeString(name);

    /* REG_DWORD */
    lstrcpyW(pathW, keypathW);
    lstrcatW(pathW, L"regdword");
    name = SysAllocString(pathW);
    VariantInit(&value);
    hr = IWshShell3_RegRead(sh3, name, &value);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    ok(V_VT(&value) == VT_I4, "got %d\n", V_VT(&value));
    ok(V_I4(&value) == 10, "got %d\n", V_I4(&value));
    SysFreeString(name);

    /* REG_BINARY */
    lstrcpyW(pathW, keypathW);
    lstrcatW(pathW, L"regbinary");
    name = SysAllocString(pathW);
    VariantInit(&value);
    hr = IWshShell3_RegRead(sh3, name, &value);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    ok(V_VT(&value) == (VT_ARRAY|VT_VARIANT), "got 0x%x\n", V_VT(&value));
    dim = SafeArrayGetDim(V_ARRAY(&value));
    ok(dim == 1, "got %u\n", dim);

    hr = SafeArrayGetLBound(V_ARRAY(&value), 1, &bound);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    ok(bound == 0, "got %u\n", bound);

    hr = SafeArrayGetUBound(V_ARRAY(&value), 1, &bound);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    ok(bound == 3, "got %u\n", bound);

    hr = SafeArrayGetVartype(V_ARRAY(&value), &vartype);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    ok(vartype == VT_VARIANT, "got %d\n", vartype);

    bound = 0;
    hr = SafeArrayGetElement(V_ARRAY(&value), &bound, &v);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    ok(V_VT(&v) == VT_UI1, "got %d\n", V_VT(&v));
    ok(V_UI1(&v) == 11, "got %u\n", V_UI1(&v));
    VariantClear(&v);
    VariantClear(&value);
    SysFreeString(name);

    /* REG_MULTI_SZ */
    lstrcpyW(pathW, keypathW);
    lstrcatW(pathW, L"regmultisz");
    name = SysAllocString(pathW);
    VariantInit(&value);
    hr = IWshShell3_RegRead(sh3, name, &value);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    ok(V_VT(&value) == (VT_ARRAY|VT_VARIANT), "got 0x%x\n", V_VT(&value));
    SysFreeString(name);

    dim = SafeArrayGetDim(V_ARRAY(&value));
    ok(dim == 1, "got %u\n", dim);

    hr = SafeArrayGetLBound(V_ARRAY(&value), 1, &bound);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    ok(bound == 0, "got %u\n", bound);

    hr = SafeArrayGetUBound(V_ARRAY(&value), 1, &bound);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    ok(bound == 1, "got %u\n", bound);

    hr = SafeArrayGetVartype(V_ARRAY(&value), &vartype);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    ok(vartype == VT_VARIANT, "got %d\n", vartype);

    bound = 0;
    hr = SafeArrayGetElement(V_ARRAY(&value), &bound, &v);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    ok(V_VT(&v) == VT_BSTR, "got %d\n", V_VT(&v));
    ok(!lstrcmpW(V_BSTR(&v), L"foo"), "got %s\n", wine_dbgstr_w(V_BSTR(&v)));
    CHECK_BSTR_LENGTH(V_BSTR(&v));
    VariantClear(&v);
    VariantClear(&value);

    name = SysAllocString(L"HKEY_CURRENT_USER\\Software\\Wine\\Test\\regsz1");
    V_VT(&value) = VT_I2;
    hr = IWshShell3_RegRead(sh3, name, &value);
    ok(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), "got 0x%08x\n", hr);
    ok(V_VT(&value) == VT_I2, "got %d\n", V_VT(&value));
    VariantClear(&value);
    SysFreeString(name);

    delete_key(root);

    /* RegWrite() */
    ret = RegCreateKeyA(HKEY_CURRENT_USER, "Software\\Wine\\Test", &root);
    ok(ret == 0, "got %d\n", ret);

    hr = IWshShell3_RegWrite(sh3, NULL, NULL, NULL);
    ok(hr == E_POINTER, "got 0x%08x\n", hr);

    lstrcpyW(pathW, keypathW);
    lstrcatW(pathW, regszW);
    name = SysAllocString(pathW);

    hr = IWshShell3_RegWrite(sh3, name, NULL, NULL);
    ok(hr == E_POINTER, "got 0x%08x\n", hr);

    VariantInit(&value);
    hr = IWshShell3_RegWrite(sh3, name, &value, NULL);
    ok(hr == E_POINTER, "got 0x%08x\n", hr);

    hr = IWshShell3_RegWrite(sh3, name, &value, &value);
    ok(hr == E_INVALIDARG, "got 0x%08x\n", hr);

    /* type is optional */
    V_VT(&v) = VT_ERROR;
    V_ERROR(&v) = DISP_E_PARAMNOTFOUND;
    hr = IWshShell3_RegWrite(sh3, name, &value, &v);
    ok(hr == S_OK, "got 0x%08x\n", hr);

    /* default type is REG_SZ */
    V_VT(&value) = VT_I4;
    V_I4(&value) = 12;
    hr = IWshShell3_RegWrite(sh3, name, &value, &v);
    ok(hr == S_OK, "got 0x%08x\n", hr);

    type = REG_NONE;
    ret = RegQueryValueExA(root, "regsz", 0, &type, NULL, NULL);
    ok(ret == ERROR_SUCCESS, "got %d\n", ret);
    ok(type == REG_SZ, "got %d\n", type);

    ret = RegDeleteValueA(root, "regsz");
    ok(ret == ERROR_SUCCESS, "got %d\n", ret);
    V_VT(&value) = VT_BSTR;
    V_BSTR(&value) = SysAllocString(regszW);
    hr = IWshShell3_RegWrite(sh3, name, &value, &v);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    VariantClear(&value);

    type = REG_NONE;
    ret = RegQueryValueExA(root, "regsz", 0, &type, NULL, NULL);
    ok(ret == ERROR_SUCCESS, "got %d\n", ret);
    ok(type == REG_SZ, "got %d\n", type);

    ret = RegDeleteValueA(root, "regsz");
    ok(ret == ERROR_SUCCESS, "got %d\n", ret);
    V_VT(&value) = VT_R4;
    V_R4(&value) = 1.2;
    hr = IWshShell3_RegWrite(sh3, name, &value, &v);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    VariantClear(&value);

    type = REG_NONE;
    ret = RegQueryValueExA(root, "regsz", 0, &type, NULL, NULL);
    ok(ret == ERROR_SUCCESS, "got %d\n", ret);
    ok(type == REG_SZ, "got %d\n", type);

    ret = RegDeleteValueA(root, "regsz");
    ok(ret == ERROR_SUCCESS, "got %d\n", ret);
    V_VT(&value) = VT_R4;
    V_R4(&value) = 1.2;
    V_VT(&v) = VT_I2;
    V_I2(&v) = 1;
    hr = IWshShell3_RegWrite(sh3, name, &value, &v);
    ok(hr == E_INVALIDARG, "got 0x%08x\n", hr);
    VariantClear(&value);

    SysFreeString(name);

    delete_key(root);
    IWshShell3_Release(sh3);
}

static void test_popup(void)
{
    VARIANT timeout, type, title, optional;
    IWshShell *sh;
    int button;
    HRESULT hr;
    BSTR text;

    hr = CoCreateInstance(&CLSID_WshShell, NULL, CLSCTX_INPROC_SERVER | CLSCTX_INPROC_HANDLER,
            &IID_IWshShell, (void **)&sh);
    ok(hr == S_OK, "Failed to create WshShell object, hr %#x.\n", hr);

    button = 123;
    text = SysAllocString(L"Text");

    hr = IWshShell_Popup(sh, NULL, NULL, NULL, NULL, &button);
    ok(hr == E_POINTER, "Unexpected retval %#x.\n", hr);
    ok(button == 123, "Unexpected button id %d.\n", button);

    hr = IWshShell_Popup(sh, text, NULL, NULL, NULL, &button);
    ok(hr == E_POINTER, "Unexpected retval %#x.\n", hr);
    ok(button == 123, "Unexpected button id %d.\n", button);

    V_VT(&optional) = VT_ERROR;
    V_ERROR(&optional) = DISP_E_PARAMNOTFOUND;

    V_VT(&timeout) = VT_I2;
    V_I2(&timeout) = 1;

    V_VT(&type) = VT_I2;
    V_I2(&type) = 1;

    V_VT(&title) = VT_BSTR;
    V_BSTR(&title) = NULL;

    hr = IWshShell_Popup(sh, text, &timeout, &optional, &type, &button);
    ok(hr == S_OK, "Unexpected retval %#x.\n", hr);
    ok(button == -1, "Unexpected button id %d.\n", button);

    hr = IWshShell_Popup(sh, text, &timeout, &title, &optional, &button);
    ok(hr == S_OK, "Unexpected retval %#x.\n", hr);
    ok(button == -1, "Unexpected button id %d.\n", button);

    SysFreeString(text);
    IWshShell_Release(sh);
}

START_TEST(wshom)
{
    IUnknown *unk;
    HRESULT hr;

    CoInitialize(NULL);

    hr = CoCreateInstance(&CLSID_WshShell, NULL, CLSCTX_INPROC_SERVER|CLSCTX_INPROC_HANDLER,
            &IID_IUnknown, (void**)&unk);
    if (FAILED(hr)) {
        win_skip("Could not create WshShell object: %08x\n", hr);
        return;
    }
    IUnknown_Release(unk);

    test_wshshell();
    test_registry();
    test_popup();

    CoUninitialize();
}
