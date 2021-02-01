/*
 * Copyright 2014 Dmitry Timoshkov
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

#include <stdio.h>
#include <windows.h>
#include <ole2.h>
#include <rpcdce.h>
#include <taskschd.h>
#include "schrpc.h"

#include "wine/test.h"

extern handle_t schrpc_handle;

static LONG CALLBACK rpc_exception_filter(EXCEPTION_POINTERS *ptrs)
{
    skip("Can't connect to Scheduler service: %#x\n", ptrs->ExceptionRecord->ExceptionCode);

    if (winetest_debug)
    {
        fprintf(stdout, "%04x:rpcapi: 0 tests executed (0 marked as todo, 0 failures), 1 skipped.\n", GetCurrentProcessId());
        fflush(stdout);
    }
    ExitProcess(0);
}

START_TEST(rpcapi)
{
    static unsigned char ncalrpc[] = "ncalrpc";
    static const char xml1[] =
        "<?xml version=\"1.0\"?>\n"
        "<Task xmlns=\"http://schemas.microsoft.com/windows/2004/02/mit/task\">\n"
        "  <RegistrationInfo>\n"
        "    <Description>\"Task1\"</Description>\n"
        "  </RegistrationInfo>\n"
        "  <Settings>\n"
        "    <Enabled>false</Enabled>\n"
        "    <Hidden>false</Hidden>\n"
        "  </Settings>\n"
        "  <Actions>\n"
        "    <Exec>\n"
        "      <Command>\"task1.exe\"</Command>\n"
        "    </Exec>\n"
        "  </Actions>\n"
        "</Task>\n";
    static const struct
    {
        DWORD flags, hr;
    } create_new_task[] =
    {
        { 0, S_OK },
        { TASK_CREATE, S_OK },
        { TASK_UPDATE, 0x80070002 /* HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) */ },
        { TASK_CREATE | TASK_UPDATE, S_OK }
    };
    static const struct
    {
        DWORD flags, hr;
    } open_existing_task[] =
    {
        { 0, 0x800700b7 /* HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS) */ },
        { TASK_CREATE, 0x800700b7 /* HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS) */ },
        { TASK_UPDATE, S_OK },
        { TASK_CREATE | TASK_UPDATE, S_OK }
    };
    WCHAR xmlW[sizeof(xml1)], *xml;
    HRESULT hr;
    DWORD version, start_index, count, i, enumed, enabled, state;
    WCHAR *path;
    TASK_XML_ERROR_INFO *info;
    TASK_NAMES names;
    unsigned char *binding_str;
    PTOP_LEVEL_EXCEPTION_FILTER old_exception_filter;
    IID iid;

    hr = RpcStringBindingComposeA(NULL, ncalrpc, NULL, NULL, NULL, &binding_str);
    ok(hr == RPC_S_OK, "RpcStringBindingCompose error %#x\n", hr);
    hr = RpcBindingFromStringBindingA(binding_str, &schrpc_handle);
    ok(hr == RPC_S_OK, "RpcBindingFromStringBinding error %#x\n", hr);
    hr = RpcStringFreeA(&binding_str);
    ok(hr == RPC_S_OK, "RpcStringFree error %#x\n", hr);

    /* widl generated RpcTryExcept/RpcExcept can't catch raised exceptions */
    old_exception_filter = SetUnhandledExceptionFilter(rpc_exception_filter);

    version = 0;
    hr = SchRpcHighestVersion(&version);
    ok(hr == S_OK, "expected S_OK, got %#x\n", hr);
    ok(version == 0x10002 /* vista */ || version == 0x10003 /* win7 */ ||
       version == 0x10004 /* win8 */ || version == 0x10005 /* win10 */ ||
       version == 0x10006 /* win10 1709 */,
       "wrong version %#x\n", version);

    SetUnhandledExceptionFilter(old_exception_filter);

    hr = SchRpcCreateFolder(L"\\Wine\\Folder1", NULL, 1);
    ok(hr == E_INVALIDARG, "expected E_INVALIDARG, got %#x\n", hr);

    hr = SchRpcCreateFolder(L"\\Wine\\Folder1", NULL, 0);
    ok(hr == S_OK, "expected S_OK, got %#x\n", hr);

    hr = SchRpcCreateFolder(L"\\Wine\\Folder1", NULL, 0);
    ok(hr == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS), "expected ERROR_ALREADY_EXISTS, got %#x\n", hr);

    hr = SchRpcCreateFolder(L"Wine\\Folder2", L"", 0);
    ok(hr == S_OK, "expected S_OK, got %#x\n", hr);

    hr = SchRpcDelete(L"Wine", 0);
    ok(hr == HRESULT_FROM_WIN32(ERROR_DIR_NOT_EMPTY), "expected ERROR_DIR_NOT_EMPTY, got %#x\n", hr);

    hr = SchRpcDelete(L"Wine\\Folder1", 1);
    ok(hr == E_INVALIDARG, "expected E_INVALIDARG, got %#x\n", hr);

    hr = SchRpcDelete(L"Wine\\Folder1", 0);
    ok(hr == S_OK, "expected S_OK, got %#x\n", hr);

    hr = SchRpcDelete(L"\\Wine\\Folder2", 0);
    ok(hr == S_OK, "expected S_OK, got %#x\n", hr);

    hr = SchRpcDelete(L"\\Wine", 0);
    ok(hr == S_OK, "expected S_OK, got %#x\n", hr);

    hr = SchRpcDelete(L"\\Wine", 0);
    ok(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) ||
       broken(hr == S_OK), /* Early versions of Win 10 */
       "expected ERROR_FILE_NOT_FOUND, got %#x\n", hr);

    hr = SchRpcDelete(L"", 0);
    ok(hr == E_ACCESSDENIED /* win7 */ || hr == E_INVALIDARG /* vista */, "expected E_ACCESSDENIED, got %#x\n", hr);

    hr = SchRpcCreateFolder(L"\\Wine", NULL, 0);
    ok(hr == S_OK, "expected S_OK, got %#x\n", hr);

    MultiByteToWideChar(CP_ACP, 0, xml1, -1, xmlW, ARRAY_SIZE(xmlW));

    path = NULL;
    info = NULL;
    hr = SchRpcRegisterTask(L"Wine", xmlW, TASK_VALIDATE_ONLY, NULL, TASK_LOGON_NONE, 0, NULL, &path, &info);
    ok(hr == S_OK, "expected S_OK, got %#x\n", hr);
    ok(!path, "expected NULL, path %p\n", path);
    ok(!info, "expected NULL, info %p\n", info);

    for (i = 0; i < ARRAY_SIZE(create_new_task); i++)
    {
        path = NULL;
        info = NULL;
        hr = SchRpcRegisterTask(L"\\Wine\\Task1", xmlW, create_new_task[i].flags, NULL,
                                TASK_LOGON_NONE, 0, NULL, &path, &info);
        ok(hr == create_new_task[i].hr, "%u: expected %#x, got %#x\n", i, create_new_task[i].hr, hr);

        if (hr == S_OK)
        {
            hr = SchRpcDelete(L"\\Wine\\Task1", 0);
            ok(hr == S_OK, "expected S_OK, got %#x\n", hr);
            ok(!info, "expected NULL, info %p\n", info);
            MIDL_user_free(path);
        }
    }

    path = NULL;
    info = NULL;
    hr = SchRpcRegisterTask(L"\\Wine\\Task1", xmlW, TASK_VALIDATE_ONLY, NULL, TASK_LOGON_NONE, 0, NULL, &path, &info);
    ok(hr == S_OK, "expected S_OK, got %#x\n", hr);
    ok(!path, "expected NULL, path %p\n", path);
    ok(!info, "expected NULL, info %p\n", info);

    path = NULL;
    info = NULL;
    hr = SchRpcRegisterTask(NULL, xmlW, TASK_VALIDATE_ONLY, NULL, TASK_LOGON_NONE, 0, NULL, &path, &info);
    ok(hr == S_OK, "expected S_OK, got %#x\n", hr);
    ok(!path, "expected NULL, path %p\n", path);
    ok(!info, "expected NULL, info %p\n", info);

    path = NULL;
    info = NULL;
    hr = SchRpcRegisterTask(L"Wine\\Folder1\\Task1", xmlW, TASK_CREATE, NULL, TASK_LOGON_NONE, 0, NULL, &path, &info);
    ok(hr == S_OK, "expected S_OK, got %#x\n", hr);
    ok(!lstrcmpW(path, L"\\Wine\\Folder1\\Task1") /* win7 */ || !lstrcmpW(path, L"Wine\\Folder1\\Task1") /* vista */,
       "expected \\Wine\\Folder1\\Task1, task actual path %s\n", wine_dbgstr_w(path));
    ok(!info, "expected NULL, info %p\n", info);
    MIDL_user_free(path);

    for (i = 0; i < ARRAY_SIZE(open_existing_task); i++)
    {
        path = NULL;
        info = NULL;
        hr = SchRpcRegisterTask(L"Wine\\Folder1\\Task1", xmlW, open_existing_task[i].flags, NULL,
                                TASK_LOGON_NONE, 0, NULL, &path, &info);
        ok(hr == open_existing_task[i].hr, "%u: expected %#x, got %#x\n", i, open_existing_task[i].hr, hr);
        if (hr == S_OK)
        {
            ok(!lstrcmpW(path, L"\\Wine\\Folder1\\Task1") /* win7 */ ||
               !lstrcmpW(path, L"Wine\\Folder1\\Task1") /* vista */,
               "expected \\Wine\\Folder1\\Task1, task actual path %s\n", wine_dbgstr_w(path));
            MIDL_user_free(path);
        }
        else
            ok(!path, "%u: expected NULL, path %p\n", i, path);
        ok(!info, "%u: expected NULL, info %p\n", i, info);
    }

    path = NULL;
    info = NULL;
    hr = SchRpcRegisterTask(L"Wine\\Folder1\\Task1", xmlW, TASK_CREATE, NULL, TASK_LOGON_NONE, 0, NULL, &path, &info);
    ok(hr == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS), "expected ERROR_ALREADY_EXISTS, got %#x\n", hr);
    ok(!path, "expected NULL, path %p\n", path);
    ok(!info, "expected NULL, info %p\n", info);

    count = 0;
    xml = NULL;
    hr = SchRpcRetrieveTask(L"\\Wine\\Folder1\\Task1", L"", &count, &xml);
    ok(hr == S_OK, "expected S_OK, got %#x\n", hr);
    if (hr == S_OK) trace("%s\n", wine_dbgstr_w(xml));
    MIDL_user_free(xml);

    hr = SchRpcDelete(L"Wine\\Folder1\\Task1", 0);
    ok(hr == S_OK, "expected S_OK, got %#x\n", hr);
    hr = SchRpcDelete(L"Wine\\Folder1", 0);
    ok(hr == S_OK, "expected S_OK, got %#x\n", hr);
    hr = SchRpcDelete(L"Wine", 0);
    ok(hr == S_OK, "expected S_OK, got %#x\n", hr);

    start_index = 0;
    count = 0xdeadbeef;
    names = NULL;
    hr = SchRpcEnumFolders(L"\\Wine", TASK_ENUM_HIDDEN, &start_index, 0, &count, &names);
    ok(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), "expected ERROR_FILE_NOT_FOUND, got %#x\n", hr);
    ok(!count, "expected 0, got %u\n", count);
    ok(!start_index, "expected 0, got %u\n", start_index);
    ok(!names, "expected NULL, got %p\n", names);

    hr = SchRpcCreateFolder(L"\\Wine", NULL, 0);
    ok(hr == S_OK, "expected S_OK, got %#x\n", hr);

    start_index = 0;
    count = 0xdeadbeef;
    names = NULL;
    hr = SchRpcEnumFolders(L"\\Wine", 0, &start_index, 0, &count, &names);
    ok(hr == S_OK, "expected S_OK, got %#x\n", hr);
    ok(!count, "expected 0, got %u\n", count);
    ok(!start_index, "expected 0, got %u\n", start_index);
    ok(!names, "expected NULL, got %p\n", names);

    hr = SchRpcCreateFolder(L"\\Wine\\Folder1", NULL, 0);
    ok(hr == S_OK, "expected S_OK, got %#x\n", hr);
    hr = SchRpcCreateFolder(L"\\Wine\\Folder2", NULL, 0);
    ok(hr == S_OK, "expected S_OK, got %#x\n", hr);

    start_index = 0;
    count = 0xdeadbeef;
    names = NULL;
    hr = SchRpcEnumFolders(L"\\Wine", 0, &start_index, 0, &count, &names);
    ok(hr == S_OK, "expected S_OK, got %#x\n", hr);
    ok(count == 2, "expected 2, got %u\n", count);
    ok(start_index == 2, "expected 2, got %u\n", start_index);
    ok(names != NULL, "names should not be NULL\n");
    enumed = 0;
    for (i = 0; i < count; i++)
    {
        if (!lstrcmpW(names[i], L"Folder1"))
            enumed += 1;
        else if (!lstrcmpW(names[i], L"Folder2"))
            enumed += 2;
        MIDL_user_free(names[i]);
    }
    MIDL_user_free(names);
    ok(enumed == 3, "expected 3, got %u\n", enumed);

    start_index = 0;
    count = 0xdeadbeef;
    names = NULL;
    hr = SchRpcEnumFolders(L"\\Wine", 0, &start_index, 1, &count, &names);
    ok(hr == S_FALSE, "expected S_FALSE, got %#x\n", hr);
    ok(count == 1, "expected 1, got %u\n", count);
    ok(start_index == 1, "expected 1, got %u\n", start_index);
    ok(names != NULL, "names should not be NULL\n");
    ok(!lstrcmpW(names[0], L"Folder1") || !lstrcmpW(names[0], L"Folder2"), "got %s\n", wine_dbgstr_w(names[0]));
    MIDL_user_free(names[0]);
    MIDL_user_free(names);

    start_index = 0;
    count = 0xdeadbeef;
    names = NULL;
    hr = SchRpcEnumFolders(L"\\Wine", 0, &start_index, 2, &count, &names);
    ok(hr == S_FALSE, "expected S_FALSE, got %#x\n", hr);
    ok(count == 2, "expected 2, got %u\n", count);
    ok(start_index == 2, "expected 2, got %u\n", start_index);
    ok(names != NULL, "names should not be NULL\n");
    enumed = 0;
    for (i = 0; i < count; i++)
    {
        if (!lstrcmpW(names[i], L"Folder1"))
            enumed += 1;
        else if (!lstrcmpW(names[i], L"Folder2"))
            enumed += 2;
        MIDL_user_free(names[i]);
    }
    MIDL_user_free(names);
    ok(enumed == 3, "expected 3, got %u\n", enumed);

    start_index = 0;
    count = 0xdeadbeef;
    names = NULL;
    hr = SchRpcEnumFolders(L"\\Wine", 0, &start_index, 10, &count, &names);
    ok(hr == S_OK, "expected S_OK, got %#x\n", hr);
    ok(count == 2, "expected 2, got %u\n", count);
    ok(start_index == 2, "expected 2, got %u\n", start_index);
    ok(names != NULL, "names should not be NULL\n");
    enumed = 0;
    for (i = 0; i < count; i++)
    {
        if (!lstrcmpW(names[i], L"Folder1"))
            enumed += 1;
        else if (!lstrcmpW(names[i], L"Folder2"))
            enumed += 2;
        MIDL_user_free(names[i]);
    }
    MIDL_user_free(names);
    ok(enumed == 3, "expected 3, got %u\n", enumed);

    start_index = 10;
    count = 0xdeadbeef;
    names = NULL;
    hr = SchRpcEnumFolders(L"\\Wine", 0, &start_index, 0, &count, &names);
    ok(hr == S_FALSE, "expected S_FALSE, got %#x\n", hr);
    ok(!count, "expected 0, got %u\n", count);
    ok(start_index == 10, "expected 10, got %u\n", start_index);
    ok(!names, "expected NULL, got %p\n", names);

    hr = SchRpcDelete(L"Wine\\Folder1", 0);
    ok(hr == S_OK, "expected S_OK, got %#x\n", hr);
    hr = SchRpcDelete(L"\\Wine\\Folder2", 0);
    ok(hr == S_OK, "expected S_OK, got %#x\n", hr);

    start_index = 0;
    count = 0xdeadbeef;
    names = NULL;
    hr = SchRpcEnumTasks(L"\\Wine\\Folder1", TASK_ENUM_HIDDEN, &start_index, 0, &count, &names);
    ok(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), "expected ERROR_FILE_NOT_FOUND, got %#x\n", hr);
    ok(!count, "expected 0, got %u\n", count);
    ok(!start_index, "expected 0, got %u\n", start_index);
    ok(!names, "expected NULL, got %p\n", names);

    path = NULL;
    info = NULL;
    hr = SchRpcRegisterTask(L"Wine\\Task1", xmlW, TASK_CREATE, NULL, TASK_LOGON_NONE, 0, NULL, &path, &info);
    ok(hr == S_OK, "expected S_OK, got %#x\n", hr);
    ok(!lstrcmpW(path, L"\\Wine\\Task1") /* win7 */ || !lstrcmpW(path, L"Wine\\Task1") /* vista */,
       "expected \\Wine\\Task1, task actual path %s\n", wine_dbgstr_w(path));
    ok(!info, "expected NULL, info %p\n", info);
    MIDL_user_free(path);

    path = NULL;
    info = NULL;
    hr = SchRpcRegisterTask(L"\\Wine\\Task2", xmlW, TASK_CREATE, NULL, TASK_LOGON_NONE, 0, NULL, &path, &info);
    ok(hr == S_OK, "expected S_OK, got %#x\n", hr);
    ok(!lstrcmpW(path, L"\\Wine\\Task2"), "expected \\Wine\\Task2, task actual path %s\n", wine_dbgstr_w(path));
    ok(!info, "expected NULL, info %p\n", info);
    MIDL_user_free(path);

    start_index = 0;
    count = 0xdeadbeef;
    names = NULL;
    hr = SchRpcEnumTasks(L"\\Wine", 0, &start_index, 0, &count, &names);
    ok(hr == S_OK, "expected S_OK, got %#x\n", hr);
    ok(count == 2, "expected 2, got %u\n", count);
    ok(start_index == 2, "expected 2, got %u\n", start_index);
    ok(names != NULL, "names should not be NULL\n");
    enumed = 0;
    for (i = 0; i < count; i++)
    {
        if (!lstrcmpW(names[i], L"Task1"))
            enumed += 1;
        else if (!lstrcmpW(names[i], L"Task2"))
            enumed += 2;
        MIDL_user_free(names[i]);
    }
    MIDL_user_free(names);
    ok(enumed == 3, "expected 3, got %u\n", enumed);

    start_index = 0;
    count = 0xdeadbeef;
    names = NULL;
    hr = SchRpcEnumTasks(L"\\Wine", 0, &start_index, 1, &count, &names);
    ok(hr == S_FALSE, "expected S_FALSE, got %#x\n", hr);
    ok(count == 1, "expected 1, got %u\n", count);
    ok(start_index == 1, "expected 1, got %u\n", start_index);
    ok(names != NULL, "names should not be NULL\n");
    /* returned name depends whether directory randomization is on */
    ok(!lstrcmpW(names[0], L"Task1") || !lstrcmpW(names[0], L"Task2"),
       "expected Task1, got %s\n", wine_dbgstr_w(names[0]));
    MIDL_user_free(names[0]);
    MIDL_user_free(names);

    start_index = 0;
    count = 0xdeadbeef;
    names = NULL;
    hr = SchRpcEnumTasks(L"\\Wine", 0, &start_index, 2, &count, &names);
    ok(hr == S_FALSE, "expected S_FALSE, got %#x\n", hr);
    ok(count == 2, "expected 2, got %u\n", count);
    ok(start_index == 2, "expected 2, got %u\n", start_index);
    ok(names != NULL, "names should not be NULL\n");
    enumed = 0;
    for (i = 0; i < count; i++)
    {
        if (!lstrcmpW(names[i], L"Task1"))
            enumed += 1;
        else if (!lstrcmpW(names[i], L"Task2"))
            enumed += 2;
        MIDL_user_free(names[i]);
    }
    MIDL_user_free(names);
    ok(enumed == 3, "expected 3, got %u\n", enumed);

    start_index = 0;
    count = 0xdeadbeef;
    names = NULL;
    hr = SchRpcEnumTasks(L"\\Wine", 0, &start_index, 10, &count, &names);
    ok(hr == S_OK, "expected S_OK, got %#x\n", hr);
    ok(count == 2, "expected 2, got %u\n", count);
    ok(start_index == 2, "expected 2, got %u\n", start_index);
    ok(names != NULL, "names should not be NULL\n");
    enumed = 0;
    for (i = 0; i < count; i++)
    {
        if (!lstrcmpW(names[i], L"Task1"))
            enumed += 1;
        else if (!lstrcmpW(names[i], L"Task2"))
            enumed += 2;
        MIDL_user_free(names[i]);
    }
    MIDL_user_free(names);
    ok(enumed == 3, "expected 3, got %u\n", enumed);

    path = NULL;
    info = NULL;
    hr = SchRpcRegisterTask(L"Wine\\Task3", xmlW, TASK_CREATE, NULL, TASK_LOGON_NONE, 0, NULL, &path, &info);
    ok(hr == S_OK, "expected S_OK, got %#x\n", hr);
    ok(!lstrcmpW(path, L"\\Wine\\Task3") /* win7 */ || !lstrcmpW(path, L"Wine\\Task3") /* vista */,
       "expected \\Wine\\Task3, task actual path %s\n", wine_dbgstr_w(path));
    ok(!info, "expected NULL, info %p\n", info);
    MIDL_user_free(path);

    count = 0xdeadbeef;
    names = NULL;
    hr = SchRpcEnumTasks(L"Wine", 0, &start_index, 10, &count, &names);
    ok(hr == S_OK, "expected S_OK, got %#x\n", hr);
    ok(count == 1, "expected 1, got %u\n", count);
    ok(start_index == 3, "expected 3, got %u\n", start_index);
    ok(names != NULL, "names should not be NULL\n");
    /* returned name depends whether directory randomization is on */
    ok(!lstrcmpW(names[0], L"Task1") || !lstrcmpW(names[0], L"Task2") || !lstrcmpW(names[0], L"Task3"),
       "expected Task3, got %s\n", wine_dbgstr_w(names[0]));
    MIDL_user_free(names[0]);
    MIDL_user_free(names);

    if (0) /* crashes under win7 */
    {
    hr = SchRpcGetTaskInfo(NULL, 0, NULL, NULL);
    hr = SchRpcGetTaskInfo(L"Task1", 0, NULL, NULL);
    hr = SchRpcGetTaskInfo(L"Task1", 0, &enabled, NULL);
    hr = SchRpcGetTaskInfo(L"Task1", 0, NULL, &state);
    }

    hr = SchRpcGetTaskInfo(L"Task1", 0, &enabled, &state);
    ok(hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), "expected ERROR_FILE_NOT_FOUND, got %#x\n", hr);

    enabled = state = 0xdeadbeef;
    hr = SchRpcGetTaskInfo(L"\\Wine\\Task1", 0, &enabled, &state);
    ok(hr == S_OK, "expected S_OK, got %#x\n", hr);
    ok(enabled == 0, "expected 0, got %u\n", enabled);
    ok(state == TASK_STATE_UNKNOWN, "expected TASK_STATE_UNKNOWN, got %u\n", state);

    enabled = state = 0xdeadbeef;
    hr = SchRpcGetTaskInfo(L"\\Wine\\Task1", SCH_FLAG_STATE, &enabled, &state);
    ok(hr == S_OK, "expected S_OK, got %#x\n", hr);
    ok(enabled == 0, "expected 0, got %u\n", enabled);
    ok(state == TASK_STATE_DISABLED, "expected TASK_STATE_DISABLED, got %u\n", state);

    hr = SchRpcEnableTask(L"\\Wine\\Task1", 0xdeadbeef);
todo_wine
    ok(hr == S_OK, "expected S_OK, got %#x\n", hr);

    enabled = state = 0xdeadbeef;
    hr = SchRpcGetTaskInfo(L"\\Wine\\Task1", SCH_FLAG_STATE, &enabled, &state);
    ok(hr == S_OK, "expected S_OK, got %#x\n", hr);
todo_wine
    ok(enabled == 1, "expected 1, got %u\n", enabled);
todo_wine
    ok(state == TASK_STATE_READY, "expected TASK_STATE_READY, got %u\n", state);

    hr = SchRpcDelete(L"Wine\\Task1", 0);
    ok(hr == S_OK, "expected S_OK, got %#x\n", hr);
    hr = SchRpcDelete(L"Wine\\Task2", 0);
    ok(hr == S_OK, "expected S_OK, got %#x\n", hr);
    hr = SchRpcDelete(L"Wine\\Task3", 0);
    ok(hr == S_OK, "expected S_OK, got %#x\n", hr);
    hr = SchRpcDelete(L"\\Wine", 0);
    ok(hr == S_OK, "expected S_OK, got %#x\n", hr);

    path = NULL;
    info = NULL;
    hr = SchRpcRegisterTask(NULL, xmlW, TASK_CREATE, NULL, TASK_LOGON_NONE, 0, NULL, &path, &info);
    ok(hr == S_OK || hr == E_ACCESSDENIED, "expected S_OK, got %#x\n", hr);
    if (hr != E_ACCESSDENIED)
    {
        ok(!info, "expected NULL, info %p\n", info);
        hr = IIDFromString(path, &iid);
        ok(hr == S_OK, "IIDFromString(%s) error %#x\n", wine_dbgstr_w(path), hr);
        hr = SchRpcDelete(path, 0);
        ok(hr == S_OK, "expected S_OK, got %#x\n", hr);
        MIDL_user_free(path);
    }

    hr = RpcBindingFree(&schrpc_handle);
    ok(hr == RPC_S_OK, "RpcBindingFree error %#x\n", hr);
}

void __RPC_FAR *__RPC_USER MIDL_user_allocate(SIZE_T n)
{
    return HeapAlloc(GetProcessHeap(), 0, n);
}

void __RPC_USER MIDL_user_free(void __RPC_FAR *p)
{
    HeapFree(GetProcessHeap(), 0, p);
}
