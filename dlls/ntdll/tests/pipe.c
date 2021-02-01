/* Unit test suite for Ntdll NamedPipe API functions
 *
 * Copyright 2011 Bernhard Loos
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
#include <stdarg.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "winreg.h"
#include "winnls.h"
#include "wine/test.h"
#include "winternl.h"
#include "winioctl.h"

#ifndef __WINE_WINTERNL_H

typedef struct {
  ULONG ReadMode;
  ULONG CompletionMode;
} FILE_PIPE_INFORMATION;

typedef struct {
  ULONG NamedPipeType;
  ULONG NamedPipeConfiguration;
  ULONG MaximumInstances;
  ULONG CurrentInstances;
  ULONG InboundQuota;
  ULONG ReadDataAvailable;
  ULONG OutboundQuota;
  ULONG WriteQuotaAvailable;
  ULONG NamedPipeState;
  ULONG NamedPipeEnd;
} FILE_PIPE_LOCAL_INFORMATION;

#ifndef FILE_SYNCHRONOUS_IO_ALERT
#define FILE_SYNCHRONOUS_IO_ALERT 0x10
#endif

#ifndef FILE_SYNCHRONOUS_IO_NONALERT
#define FILE_SYNCHRONOUS_IO_NONALERT 0x20
#endif

#ifndef FSCTL_PIPE_LISTEN
#define FSCTL_PIPE_LISTEN CTL_CODE(FILE_DEVICE_NAMED_PIPE, 2, METHOD_BUFFERED, FILE_ANY_ACCESS)
#endif
#endif

static NTSTATUS (WINAPI *pNtFsControlFile) (HANDLE handle, HANDLE event, PIO_APC_ROUTINE apc, PVOID apc_context, PIO_STATUS_BLOCK io, ULONG code, PVOID in_buffer, ULONG in_size, PVOID out_buffer, ULONG out_size);
static NTSTATUS (WINAPI *pNtCreateDirectoryObject)(HANDLE *, ACCESS_MASK, OBJECT_ATTRIBUTES *);
static NTSTATUS (WINAPI *pNtCreateNamedPipeFile) (PHANDLE handle, ULONG access,
                                        POBJECT_ATTRIBUTES attr, PIO_STATUS_BLOCK iosb,
                                        ULONG sharing, ULONG dispo, ULONG options,
                                        ULONG pipe_type, ULONG read_mode,
                                        ULONG completion_mode, ULONG max_inst,
                                        ULONG inbound_quota, ULONG outbound_quota,
                                        PLARGE_INTEGER timeout);
static NTSTATUS (WINAPI *pNtQueryInformationFile) (IN HANDLE FileHandle, OUT PIO_STATUS_BLOCK IoStatusBlock, OUT PVOID FileInformation, IN ULONG Length, IN FILE_INFORMATION_CLASS FileInformationClass);
static NTSTATUS (WINAPI *pNtQueryObject)(HANDLE, OBJECT_INFORMATION_CLASS, void *, ULONG, ULONG *);
static NTSTATUS (WINAPI *pNtQueryVolumeInformationFile)(HANDLE handle, PIO_STATUS_BLOCK io, void *buffer, ULONG length, FS_INFORMATION_CLASS info_class);
static NTSTATUS (WINAPI *pNtSetInformationFile) (HANDLE handle, PIO_STATUS_BLOCK io, PVOID ptr, ULONG len, FILE_INFORMATION_CLASS class);
static NTSTATUS (WINAPI *pNtCancelIoFile) (HANDLE hFile, PIO_STATUS_BLOCK io_status);
static NTSTATUS (WINAPI *pNtCancelIoFileEx) (HANDLE hFile, IO_STATUS_BLOCK *iosb, IO_STATUS_BLOCK *io_status);
static NTSTATUS (WINAPI *pNtRemoveIoCompletion)(HANDLE, PULONG_PTR, PULONG_PTR, PIO_STATUS_BLOCK, PLARGE_INTEGER);
static void (WINAPI *pRtlInitUnicodeString) (PUNICODE_STRING target, PCWSTR source);

static HANDLE (WINAPI *pOpenThread)(DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwThreadId);
static DWORD (WINAPI *pQueueUserAPC)(PAPCFUNC pfnAPC, HANDLE hThread, ULONG_PTR dwData);


static BOOL init_func_ptrs(void)
{
    HMODULE module = GetModuleHandleA("ntdll.dll");

#define loadfunc(name)  if (!(p##name = (void *)GetProcAddress(module, #name))) { \
                            trace("GetProcAddress(%s) failed\n", #name); \
                            return FALSE; \
                        }

    loadfunc(NtFsControlFile)
    loadfunc(NtCreateDirectoryObject)
    loadfunc(NtCreateNamedPipeFile)
    loadfunc(NtQueryInformationFile)
    loadfunc(NtQueryObject)
    loadfunc(NtQueryVolumeInformationFile)
    loadfunc(NtSetInformationFile)
    loadfunc(NtCancelIoFile)
    loadfunc(RtlInitUnicodeString)
    loadfunc(NtRemoveIoCompletion)

    /* not fatal */
    pNtCancelIoFileEx = (void *)GetProcAddress(module, "NtCancelIoFileEx");
    module = GetModuleHandleA("kernel32.dll");
    pOpenThread = (void *)GetProcAddress(module, "OpenThread");
    pQueueUserAPC = (void *)GetProcAddress(module, "QueueUserAPC");
    return TRUE;
}

static inline BOOL is_signaled( HANDLE obj )
{
    return WaitForSingleObject( obj, 0 ) == WAIT_OBJECT_0;
}

#define test_file_access(a,b) _test_file_access(__LINE__,a,b)
static void _test_file_access(unsigned line, HANDLE handle, DWORD expected_access)
{
    FILE_ACCESS_INFORMATION info;
    IO_STATUS_BLOCK io;
    NTSTATUS status;

    memset(&info, 0x11, sizeof(info));
    status = NtQueryInformationFile(handle, &io, &info, sizeof(info), FileAccessInformation);
    ok_(__FILE__,line)(status == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %08x\n", status);
    ok_(__FILE__,line)(info.AccessFlags == expected_access, "got access %08x expected %08x\n",
                       info.AccessFlags, expected_access);
}

static const WCHAR testpipe[] = { '\\', '\\', '.', '\\', 'p', 'i', 'p', 'e', '\\',
                                  't', 'e', 's', 't', 'p', 'i', 'p', 'e', 0 };
static const WCHAR testpipe_nt[] = { '\\', '?', '?', '\\', 'p', 'i', 'p', 'e', '\\',
                                     't', 'e', 's', 't', 'p', 'i', 'p', 'e', 0 };

static NTSTATUS create_pipe(PHANDLE handle, ULONG access, ULONG sharing, ULONG options)
{
    IO_STATUS_BLOCK iosb;
    OBJECT_ATTRIBUTES attr;
    UNICODE_STRING name;
    LARGE_INTEGER timeout;
    NTSTATUS res;

    pRtlInitUnicodeString(&name, testpipe_nt);

    attr.Length                   = sizeof(attr);
    attr.RootDirectory            = 0;
    attr.ObjectName               = &name;
    attr.Attributes               = OBJ_CASE_INSENSITIVE;
    attr.SecurityDescriptor       = NULL;
    attr.SecurityQualityOfService = NULL;

    timeout.QuadPart = -100000000;

    res = pNtCreateNamedPipeFile(handle, FILE_READ_ATTRIBUTES | SYNCHRONIZE | access, &attr, &iosb, sharing,
                                 FILE_CREATE, options, 1, 0, 0, 0xFFFFFFFF, 500, 500, &timeout);
    return res;
}

static BOOL ioapc_called;
static void CALLBACK ioapc(void *arg, PIO_STATUS_BLOCK io, ULONG reserved)
{
    ioapc_called = TRUE;
}

static NTSTATUS listen_pipe(HANDLE hPipe, HANDLE hEvent, PIO_STATUS_BLOCK iosb, BOOL use_apc)
{
    int dummy;

    ioapc_called = FALSE;

    return pNtFsControlFile(hPipe, hEvent, use_apc ? &ioapc: NULL, use_apc ? &dummy: NULL, iosb, FSCTL_PIPE_LISTEN, 0, 0, 0, 0);
}

static void test_create_invalid(void)
{
    IO_STATUS_BLOCK iosb;
    OBJECT_ATTRIBUTES attr;
    UNICODE_STRING name;
    LARGE_INTEGER timeout;
    NTSTATUS res;
    HANDLE handle, handle2;
    FILE_PIPE_LOCAL_INFORMATION info;

    pRtlInitUnicodeString(&name, testpipe_nt);

    attr.Length                   = sizeof(attr);
    attr.RootDirectory            = 0;
    attr.ObjectName               = &name;
    attr.Attributes               = OBJ_CASE_INSENSITIVE;
    attr.SecurityDescriptor       = NULL;
    attr.SecurityQualityOfService = NULL;

    timeout.QuadPart = -100000000;

/* create a pipe with FILE_OVERWRITE */
    res = pNtCreateNamedPipeFile(&handle, FILE_READ_ATTRIBUTES | SYNCHRONIZE, &attr, &iosb, FILE_SHARE_READ, 4 /*FILE_OVERWRITE*/,
                                 0, 1, 0, 0, 0xFFFFFFFF, 500, 500, &timeout);
    todo_wine ok(res == STATUS_INVALID_PARAMETER, "NtCreateNamedPipeFile returned %x\n", res);
    if (!res)
        CloseHandle(handle);

/* create a pipe with FILE_OVERWRITE_IF */
    res = pNtCreateNamedPipeFile(&handle, FILE_READ_ATTRIBUTES | SYNCHRONIZE, &attr, &iosb, FILE_SHARE_READ, 5 /*FILE_OVERWRITE_IF*/,
                                 0, 1, 0, 0, 0xFFFFFFFF, 500, 500, &timeout);
    todo_wine ok(res == STATUS_INVALID_PARAMETER, "NtCreateNamedPipeFile returned %x\n", res);
    if (!res)
        CloseHandle(handle);

/* create a pipe with sharing = 0 */
    res = pNtCreateNamedPipeFile(&handle, FILE_READ_ATTRIBUTES | SYNCHRONIZE, &attr, &iosb, 0, 2 /*FILE_CREATE*/,
                                 0, 1, 0, 0, 0xFFFFFFFF, 500, 500, &timeout);
    ok(res == STATUS_INVALID_PARAMETER, "NtCreateNamedPipeFile returned %x\n", res);
    if (!res)
        CloseHandle(handle);

/* create a pipe without r/w access */
    res = pNtCreateNamedPipeFile(&handle, SYNCHRONIZE, &attr, &iosb, FILE_SHARE_READ | FILE_SHARE_WRITE, 2 /*FILE_CREATE*/,
                                 0, 1, 0, 0, 0xFFFFFFFF, 500, 500, &timeout);
    ok(!res, "NtCreateNamedPipeFile returned %x\n", res);

    res = pNtQueryInformationFile(handle, &iosb, &info, sizeof(info), FilePipeLocalInformation);
    ok(res == STATUS_ACCESS_DENIED, "NtQueryInformationFile returned %x\n", res);

/* test FILE_CREATE creation disposition */
    res = pNtCreateNamedPipeFile(&handle2, SYNCHRONIZE, &attr, &iosb, FILE_SHARE_READ | FILE_SHARE_WRITE, 2 /*FILE_CREATE*/,
                                 0, 1, 0, 0, 0xFFFFFFFF, 500, 500, &timeout);
    todo_wine ok(res == STATUS_ACCESS_DENIED, "NtCreateNamedPipeFile returned %x\n", res);
    if (!res)
        CloseHandle(handle2);

    CloseHandle(handle);
}

static void test_create(void)
{
    HANDLE hserver;
    NTSTATUS res;
    int j, k;
    FILE_PIPE_LOCAL_INFORMATION info;
    IO_STATUS_BLOCK iosb;
    HANDLE hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);

    static const DWORD access[] = { 0, GENERIC_READ, GENERIC_WRITE, GENERIC_READ | GENERIC_WRITE};
    static const DWORD sharing[] =    { FILE_SHARE_READ, FILE_SHARE_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE };
    static const DWORD pipe_config[]= {               1,                0,                                  2 };

    for (j = 0; j < ARRAY_SIZE(sharing); j++) {
        for (k = 0; k < ARRAY_SIZE(access); k++) {
            HANDLE hclient;
            BOOL should_succeed = TRUE;

            res  = create_pipe(&hserver, 0, sharing[j], 0);
            if (res) {
                ok(0, "NtCreateNamedPipeFile returned %x, sharing: %x\n", res, sharing[j]);
                continue;
            }

            res = listen_pipe(hserver, hEvent, &iosb, FALSE);
            ok(res == STATUS_PENDING, "NtFsControlFile returned %x\n", res);

            res = pNtQueryInformationFile(hserver, &iosb, &info, sizeof(info), FilePipeLocalInformation);
            ok(!res, "NtQueryInformationFile for server returned %x, sharing: %x\n", res, sharing[j]);
            ok(info.NamedPipeConfiguration == pipe_config[j], "wrong duplex status for pipe: %d, expected %d\n",
               info.NamedPipeConfiguration, pipe_config[j]);

            hclient = CreateFileW(testpipe, access[k], 0, 0, OPEN_EXISTING, 0, 0);
            if (hclient != INVALID_HANDLE_VALUE) {
                res = pNtQueryInformationFile(hclient, &iosb, &info, sizeof(info), FilePipeLocalInformation);
                ok(!res, "NtQueryInformationFile for client returned %x, access: %x, sharing: %x\n",
                   res, access[k], sharing[j]);
                ok(info.NamedPipeConfiguration == pipe_config[j], "wrong duplex status for pipe: %d, expected %d\n",
                   info.NamedPipeConfiguration, pipe_config[j]);

                res = listen_pipe(hclient, hEvent, &iosb, FALSE);
                ok(res == STATUS_ILLEGAL_FUNCTION, "expected STATUS_ILLEGAL_FUNCTION, got %x\n", res);
                CloseHandle(hclient);
            }

            if (access[k] & GENERIC_WRITE)
                should_succeed &= !!(sharing[j] & FILE_SHARE_WRITE);
            if (access[k] & GENERIC_READ)
                should_succeed &= !!(sharing[j] & FILE_SHARE_READ);

            if (should_succeed)
                ok(hclient != INVALID_HANDLE_VALUE, "CreateFile failed for sharing %x, access: %x, GetLastError: %d\n",
                   sharing[j], access[k], GetLastError());
            else
                ok(hclient == INVALID_HANDLE_VALUE, "CreateFile succeeded for sharing %x, access: %x\n", sharing[j], access[k]);

            CloseHandle(hserver);
        }
    }
    CloseHandle(hEvent);
}

static void test_overlapped(void)
{
    IO_STATUS_BLOCK iosb;
    HANDLE hEvent;
    HANDLE hPipe;
    HANDLE hClient;
    NTSTATUS res;

    hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    ok(hEvent != INVALID_HANDLE_VALUE, "can't create event, GetLastError: %x\n", GetLastError());

    res = create_pipe(&hPipe, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, 0 /* OVERLAPPED */);
    ok(!res, "NtCreateNamedPipeFile returned %x\n", res);

    memset(&iosb, 0x55, sizeof(iosb));
    res = listen_pipe(hPipe, hEvent, &iosb, TRUE);
    ok(res == STATUS_PENDING, "NtFsControlFile returned %x\n", res);
    ok(U(iosb).Status == 0x55555555, "iosb.Status got changed to %x\n", U(iosb).Status);

    hClient = CreateFileW(testpipe, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
    ok(hClient != INVALID_HANDLE_VALUE, "can't open pipe, GetLastError: %x\n", GetLastError());

    ok(U(iosb).Status == 0, "Wrong iostatus %x\n", U(iosb).Status);
    ok(WaitForSingleObject(hEvent, 0) == 0, "hEvent not signaled\n");

    ok(!ioapc_called, "IOAPC ran too early\n");

    SleepEx(0, TRUE); /* alertable wait state */

    ok(ioapc_called, "IOAPC didn't run\n");

    CloseHandle(hPipe);
    CloseHandle(hClient);

    res = create_pipe(&hPipe, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, 0 /* OVERLAPPED */);
    ok(!res, "NtCreateNamedPipeFile returned %x\n", res);

    hClient = CreateFileW(testpipe, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
    ok(hClient != INVALID_HANDLE_VALUE || broken(GetLastError() == ERROR_PIPE_BUSY) /* > Win 8 */,
       "can't open pipe, GetLastError: %x\n", GetLastError());

    if (hClient != INVALID_HANDLE_VALUE)
    {
        SetEvent(hEvent);
        memset(&iosb, 0x55, sizeof(iosb));
        res = listen_pipe(hPipe, hEvent, &iosb, TRUE);
        ok(res == STATUS_PIPE_CONNECTED, "NtFsControlFile returned %x\n", res);
        ok(U(iosb).Status == 0x55555555, "iosb.Status got changed to %x\n", U(iosb).Status);
        ok(!is_signaled(hEvent), "hEvent not signaled\n");

        CloseHandle(hClient);
    }

    CloseHandle(hPipe);
    CloseHandle(hEvent);
}

static BOOL userapc_called;
static void CALLBACK userapc(ULONG_PTR dwParam)
{
    userapc_called = TRUE;
}

static BOOL open_succeeded;
static DWORD WINAPI thread(PVOID main_thread)
{
    HANDLE h;

    Sleep(400);

    if (main_thread) {
        DWORD ret;
        userapc_called = FALSE;
        ret = pQueueUserAPC(&userapc, main_thread, 0);
        ok(ret, "can't queue user apc, GetLastError: %x\n", GetLastError());
        CloseHandle(main_thread);
    }

    Sleep(400);

    h = CreateFileW(testpipe, GENERIC_WRITE | GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);

    if (h != INVALID_HANDLE_VALUE) {
        open_succeeded = TRUE;
        Sleep(100);
        CloseHandle(h);
    } else
        open_succeeded = FALSE;

    return 0;
}

static void test_alertable(void)
{
    IO_STATUS_BLOCK iosb;
    HANDLE hEvent;
    HANDLE hPipe;
    NTSTATUS res;
    HANDLE hThread;
    DWORD ret;

    memset(&iosb, 0x55, sizeof(iosb));

    hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    ok(hEvent != INVALID_HANDLE_VALUE, "can't create event, GetLastError: %x\n", GetLastError());

    res = create_pipe(&hPipe, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_SYNCHRONOUS_IO_ALERT);
    ok(!res, "NtCreateNamedPipeFile returned %x\n", res);

/* queue an user apc before calling listen */
    userapc_called = FALSE;
    ret = pQueueUserAPC(&userapc, GetCurrentThread(), 0);
    ok(ret, "can't queue user apc, GetLastError: %x\n", GetLastError());

    res = listen_pipe(hPipe, hEvent, &iosb, TRUE);
    todo_wine ok(res == STATUS_CANCELLED, "NtFsControlFile returned %x\n", res);

    ok(userapc_called, "user apc didn't run\n");
    ok(U(iosb).Status == 0x55555555, "iosb.Status got changed to %x\n", U(iosb).Status);
    ok(WaitForSingleObjectEx(hEvent, 0, TRUE) == WAIT_TIMEOUT, "hEvent signaled\n");
    ok(!ioapc_called, "IOAPC ran\n");

/* queue an user apc from a different thread */
    hThread = CreateThread(NULL, 0, &thread, pOpenThread(MAXIMUM_ALLOWED, FALSE, GetCurrentThreadId()), 0, 0);
    ok(hThread != INVALID_HANDLE_VALUE, "can't create thread, GetLastError: %x\n", GetLastError());

    /* wine_todo: the earlier NtFsControlFile call gets cancelled after the pipe gets set into listen state
                  instead of before, so this NtFsControlFile will fail STATUS_INVALID_HANDLE */
    res = listen_pipe(hPipe, hEvent, &iosb, TRUE);
    todo_wine ok(res == STATUS_CANCELLED, "NtFsControlFile returned %x\n", res);

    ok(userapc_called, "user apc didn't run\n");
    ok(U(iosb).Status == 0x55555555, "iosb.Status got changed to %x\n", U(iosb).Status);
    ok(WaitForSingleObjectEx(hEvent, 0, TRUE) == WAIT_TIMEOUT, "hEvent signaled\n");
    ok(!ioapc_called, "IOAPC ran\n");

    WaitForSingleObject(hThread, INFINITE);

    SleepEx(0, TRUE); /* get rid of the userapc, if NtFsControlFile failed */

    ok(open_succeeded, "couldn't open client side pipe\n");

    CloseHandle(hThread);
    DisconnectNamedPipe(hPipe);

/* finally try without an apc */
    hThread = CreateThread(NULL, 0, &thread, 0, 0, 0);
    ok(hThread != INVALID_HANDLE_VALUE, "can't create thread, GetLastError: %x\n", GetLastError());

    res = listen_pipe(hPipe, hEvent, &iosb, TRUE);
    ok(!res, "NtFsControlFile returned %x\n", res);

    ok(open_succeeded, "couldn't open client side pipe\n");
    ok(!U(iosb).Status, "Wrong iostatus %x\n", U(iosb).Status);
    ok(WaitForSingleObject(hEvent, 0) == 0, "hEvent not signaled\n");

    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);
    CloseHandle(hEvent);
    CloseHandle(hPipe);
}

static void test_nonalertable(void)
{
    IO_STATUS_BLOCK iosb;
    HANDLE hEvent;
    HANDLE hPipe;
    NTSTATUS res;
    HANDLE hThread;
    DWORD ret;

    memset(&iosb, 0x55, sizeof(iosb));

    hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    ok(hEvent != INVALID_HANDLE_VALUE, "can't create event, GetLastError: %x\n", GetLastError());

    res = create_pipe(&hPipe, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_SYNCHRONOUS_IO_NONALERT);
    ok(!res, "NtCreateNamedPipeFile returned %x\n", res);

    hThread = CreateThread(NULL, 0, &thread, 0, 0, 0);
    ok(hThread != INVALID_HANDLE_VALUE, "can't create thread, GetLastError: %x\n", GetLastError());

    userapc_called = FALSE;
    ret = pQueueUserAPC(&userapc, GetCurrentThread(), 0);
    ok(ret, "can't queue user apc, GetLastError: %x\n", GetLastError());

    res = listen_pipe(hPipe, hEvent, &iosb, TRUE);
    ok(!res, "NtFsControlFile returned %x\n", res);

    ok(open_succeeded, "couldn't open client side pipe\n");
    ok(!U(iosb).Status, "Wrong iostatus %x\n", U(iosb).Status);
    ok(WaitForSingleObject(hEvent, 0) == 0, "hEvent not signaled\n");

    ok(!ioapc_called, "IOAPC ran too early\n");
    ok(!userapc_called, "user apc ran too early\n");

    SleepEx(0, TRUE); /* alertable wait state */

    ok(ioapc_called, "IOAPC didn't run\n");
    ok(userapc_called, "user apc didn't run\n");

    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);
    CloseHandle(hEvent);
    CloseHandle(hPipe);
}

static void test_cancelio(void)
{
    IO_STATUS_BLOCK iosb;
    IO_STATUS_BLOCK cancel_sb;
    HANDLE hEvent;
    HANDLE hPipe;
    NTSTATUS res;

    hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    ok(hEvent != INVALID_HANDLE_VALUE, "can't create event, GetLastError: %x\n", GetLastError());

    res = create_pipe(&hPipe, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, 0 /* OVERLAPPED */);
    ok(!res, "NtCreateNamedPipeFile returned %x\n", res);

    memset(&iosb, 0x55, sizeof(iosb));

    res = listen_pipe(hPipe, hEvent, &iosb, TRUE);
    ok(res == STATUS_PENDING, "NtFsControlFile returned %x\n", res);

    res = pNtCancelIoFile(hPipe, &cancel_sb);
    ok(!res, "NtCancelIoFile returned %x\n", res);

    ok(U(iosb).Status == STATUS_CANCELLED, "Wrong iostatus %x\n", U(iosb).Status);
    ok(WaitForSingleObject(hEvent, 0) == 0, "hEvent not signaled\n");

    ok(!ioapc_called, "IOAPC ran too early\n");

    SleepEx(0, TRUE); /* alertable wait state */

    ok(ioapc_called, "IOAPC didn't run\n");

    CloseHandle(hPipe);

    if (pNtCancelIoFileEx)
    {
        res = create_pipe(&hPipe, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, 0 /* OVERLAPPED */);
        ok(!res, "NtCreateNamedPipeFile returned %x\n", res);

        memset(&iosb, 0x55, sizeof(iosb));
        res = listen_pipe(hPipe, hEvent, &iosb, FALSE);
        ok(res == STATUS_PENDING, "NtFsControlFile returned %x\n", res);

        res = pNtCancelIoFileEx(hPipe, &iosb, &cancel_sb);
        ok(!res, "NtCancelIoFileEx returned %x\n", res);

        ok(U(iosb).Status == STATUS_CANCELLED, "Wrong iostatus %x\n", U(iosb).Status);
        ok(WaitForSingleObject(hEvent, 0) == 0, "hEvent not signaled\n");

        CloseHandle(hPipe);
    }
    else
        win_skip("NtCancelIoFileEx not available\n");

    CloseHandle(hEvent);
}

static void _check_pipe_handle_state(int line, HANDLE handle, ULONG read, ULONG completion)
{
    IO_STATUS_BLOCK iosb;
    FILE_PIPE_INFORMATION fpi;
    NTSTATUS res;
    if (handle != INVALID_HANDLE_VALUE)
    {
        memset(&fpi, 0x55, sizeof(fpi));
        res = pNtQueryInformationFile(handle, &iosb, &fpi, sizeof(fpi), FilePipeInformation);
        ok_(__FILE__, line)(!res, "NtQueryInformationFile returned %x\n", res);
        ok_(__FILE__, line)(fpi.ReadMode == read, "Unexpected ReadMode, expected %x, got %x\n",
                            read, fpi.ReadMode);
        ok_(__FILE__, line)(fpi.CompletionMode == completion, "Unexpected CompletionMode, expected %x, got %x\n",
                            completion, fpi.CompletionMode);
    }
}
#define check_pipe_handle_state(handle, r, c) _check_pipe_handle_state(__LINE__, handle, r, c)

static void test_filepipeinfo(void)
{
    FILE_PIPE_LOCAL_INFORMATION local_info;
    IO_STATUS_BLOCK iosb;
    OBJECT_ATTRIBUTES attr;
    UNICODE_STRING name;
    LARGE_INTEGER timeout;
    HANDLE hServer, hClient;
    FILE_PIPE_INFORMATION fpi;
    NTSTATUS res;

    pRtlInitUnicodeString(&name, testpipe_nt);

    attr.Length                   = sizeof(attr);
    attr.RootDirectory            = 0;
    attr.ObjectName               = &name;
    attr.Attributes               = OBJ_CASE_INSENSITIVE;
    attr.SecurityDescriptor       = NULL;
    attr.SecurityQualityOfService = NULL;

    timeout.QuadPart = -100000000;

    /* test with INVALID_HANDLE_VALUE */
    res = pNtQueryInformationFile(INVALID_HANDLE_VALUE, &iosb, &fpi, sizeof(fpi), FilePipeInformation);
    ok(res == STATUS_OBJECT_TYPE_MISMATCH, "NtQueryInformationFile returned %x\n", res);

    fpi.ReadMode = 0;
    fpi.CompletionMode = 0;
    res = pNtSetInformationFile(INVALID_HANDLE_VALUE, &iosb, &fpi, sizeof(fpi), FilePipeInformation);
    ok(res == STATUS_OBJECT_TYPE_MISMATCH, "NtSetInformationFile returned %x\n", res);

    /* server end with read-only attributes */
    res = pNtCreateNamedPipeFile(&hServer, FILE_READ_ATTRIBUTES | SYNCHRONIZE, &attr, &iosb,
                                 FILE_SHARE_READ | FILE_SHARE_WRITE,  2 /* FILE_CREATE */,
                                 0, 0, 0, 1, 0xFFFFFFFF, 500, 500, &timeout);
    ok(!res, "NtCreateNamedPipeFile returned %x\n", res);

    check_pipe_handle_state(hServer, 0, 1);

    hClient = CreateFileW(testpipe, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
    ok(hClient != INVALID_HANDLE_VALUE || broken(GetLastError() == ERROR_PIPE_BUSY) /* > Win 8 */,
       "can't open pipe, GetLastError: %x\n", GetLastError());

    check_pipe_handle_state(hServer, 0, 1);
    check_pipe_handle_state(hClient, 0, 0);

    fpi.ReadMode = 0;
    fpi.CompletionMode = 0;
    res = pNtSetInformationFile(hServer, &iosb, &fpi, sizeof(fpi), FilePipeInformation);
    ok(res == STATUS_ACCESS_DENIED, "NtSetInformationFile returned %x\n", res);

    check_pipe_handle_state(hServer, 0, 1);
    check_pipe_handle_state(hClient, 0, 0);

    fpi.ReadMode = 1; /* invalid on a byte stream pipe */
    fpi.CompletionMode = 1;
    res = pNtSetInformationFile(hServer, &iosb, &fpi, sizeof(fpi), FilePipeInformation);
    ok(res == STATUS_ACCESS_DENIED, "NtSetInformationFile returned %x\n", res);

    check_pipe_handle_state(hServer, 0, 1);
    check_pipe_handle_state(hClient, 0, 0);

    if (hClient != INVALID_HANDLE_VALUE)
    {
        fpi.ReadMode = 1; /* invalid on a byte stream pipe */
        fpi.CompletionMode = 1;
        res = pNtSetInformationFile(hClient, &iosb, &fpi, sizeof(fpi), FilePipeInformation);
        ok(res == STATUS_INVALID_PARAMETER, "NtSetInformationFile returned %x\n", res);
    }

    check_pipe_handle_state(hServer, 0, 1);
    check_pipe_handle_state(hClient, 0, 0);

    if (hClient != INVALID_HANDLE_VALUE)
    {
        fpi.ReadMode = 0;
        fpi.CompletionMode = 1;
        res = pNtSetInformationFile(hClient, &iosb, &fpi, sizeof(fpi), FilePipeInformation);
        ok(!res, "NtSetInformationFile returned %x\n", res);
    }

    check_pipe_handle_state(hServer, 0, 1);
    check_pipe_handle_state(hClient, 0, 1);

    if (hClient != INVALID_HANDLE_VALUE)
    {
        fpi.ReadMode = 0;
        fpi.CompletionMode = 2; /* not in range 0-1 */
        res = pNtSetInformationFile(hClient, &iosb, &fpi, sizeof(fpi), FilePipeInformation);
        ok(res == STATUS_INVALID_PARAMETER || broken(!res) /* < Vista */, "NtSetInformationFile returned %x\n", res);

        fpi.ReadMode = 2; /* not in range 0-1 */
        fpi.CompletionMode = 0;
        res = pNtSetInformationFile(hClient, &iosb, &fpi, sizeof(fpi), FilePipeInformation);
        ok(res == STATUS_INVALID_PARAMETER || broken(!res) /* < Vista */, "NtSetInformationFile returned %x\n", res);
    }

    CloseHandle(hClient);

    check_pipe_handle_state(hServer, 0, 1);

    fpi.ReadMode = 0;
    fpi.CompletionMode = 0;
    res = pNtSetInformationFile(hServer, &iosb, &fpi, sizeof(fpi), FilePipeInformation);
    ok(res == STATUS_ACCESS_DENIED, "NtSetInformationFile returned %x\n", res);

    CloseHandle(hServer);

    /* message mode server with read/write attributes */
    res = pNtCreateNamedPipeFile(&hServer, FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES | SYNCHRONIZE, &attr, &iosb,
                                 FILE_SHARE_READ | FILE_SHARE_WRITE,  2 /* FILE_CREATE */,
                                 0, 1, 1, 0, 0xFFFFFFFF, 500, 500, &timeout);
    ok(!res, "NtCreateNamedPipeFile returned %x\n", res);

    check_pipe_handle_state(hServer, 1, 0);

    hClient = CreateFileW(testpipe, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
    ok(hClient != INVALID_HANDLE_VALUE || broken(GetLastError() == ERROR_PIPE_BUSY) /* > Win 8 */,
       "can't open pipe, GetLastError: %x\n", GetLastError());

    check_pipe_handle_state(hServer, 1, 0);
    check_pipe_handle_state(hClient, 0, 0);

    if (hClient != INVALID_HANDLE_VALUE)
    {
        fpi.ReadMode = 1;
        fpi.CompletionMode = 1;
        res = pNtSetInformationFile(hClient, &iosb, &fpi, sizeof(fpi), FilePipeInformation);
        ok(!res, "NtSetInformationFile returned %x\n", res);
    }

    check_pipe_handle_state(hServer, 1, 0);
    check_pipe_handle_state(hClient, 1, 1);

    fpi.ReadMode = 0;
    fpi.CompletionMode = 1;
    res = pNtSetInformationFile(hServer, &iosb, &fpi, sizeof(fpi), FilePipeInformation);
    ok(!res, "NtSetInformationFile returned %x\n", res);

    check_pipe_handle_state(hServer, 0, 1);
    check_pipe_handle_state(hClient, 1, 1);

    if (hClient != INVALID_HANDLE_VALUE)
    {
        fpi.ReadMode = 0;
        fpi.CompletionMode = 2; /* not in range 0-1 */
        res = pNtSetInformationFile(hClient, &iosb, &fpi, sizeof(fpi), FilePipeInformation);
        ok(res == STATUS_INVALID_PARAMETER || broken(!res) /* < Vista */, "NtSetInformationFile returned %x\n", res);

        fpi.ReadMode = 2; /* not in range 0-1 */
        fpi.CompletionMode = 0;
        res = pNtSetInformationFile(hClient, &iosb, &fpi, sizeof(fpi), FilePipeInformation);
        ok(res == STATUS_INVALID_PARAMETER || broken(!res) /* < Vista */, "NtSetInformationFile returned %x\n", res);
    }

    CloseHandle(hClient);

    check_pipe_handle_state(hServer, 0, 1);

    fpi.ReadMode = 1;
    fpi.CompletionMode = 0;
    res = pNtSetInformationFile(hServer, &iosb, &fpi, sizeof(fpi), FilePipeInformation);
    ok(!res, "NtSetInformationFile returned %x\n", res);

    check_pipe_handle_state(hServer, 1, 0);

    CloseHandle(hServer);

    res = pNtCreateNamedPipeFile(&hServer,
                                 FILE_READ_DATA | FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES | SYNCHRONIZE,
                                 &attr, &iosb, FILE_SHARE_READ | FILE_SHARE_WRITE,  FILE_CREATE,
                                 0, 1, 1, 0, 0xFFFFFFFF, 500, 500, &timeout);
    ok(!res, "NtCreateNamedPipeFile returned %x\n", res);

    res = NtCreateFile(&hClient, SYNCHRONIZE, &attr, &iosb, NULL, 0,
                       FILE_SHARE_READ|FILE_SHARE_WRITE, FILE_OPEN, 0, NULL, 0 );
    ok(!res, "NtCreateFile returned %x\n", res);

    test_file_access(hClient, SYNCHRONIZE);

    res = pNtQueryInformationFile(hClient, &iosb, &local_info, sizeof(local_info),
                                  FilePipeLocalInformation);
    ok(res == STATUS_ACCESS_DENIED,
       "NtQueryInformationFile(FilePipeLocalInformation) returned: %x\n", res);

    res = pNtQueryInformationFile(hClient, &iosb, &local_info, sizeof(local_info),
                                  FilePipeInformation);
    ok(res == STATUS_ACCESS_DENIED,
       "NtQueryInformationFile(FilePipeInformation) returned: %x\n", res);

    res = pNtQueryInformationFile(hClient, &iosb, &local_info, sizeof(local_info),
                                  FileNameInformation);
    ok(res == STATUS_SUCCESS, "NtQueryInformationFile(FileNameInformation) returned: %x\n", res);

    CloseHandle(hClient);
    CloseHandle(hServer);
}

static void WINAPI apc( void *arg, IO_STATUS_BLOCK *iosb, ULONG reserved )
{
    int *count = arg;
    (*count)++;
    ok( !reserved, "reserved is not 0: %x\n", reserved );
}

static void test_peek(HANDLE pipe)
{
    FILE_PIPE_PEEK_BUFFER buf;
    IO_STATUS_BLOCK iosb;
    HANDLE event = CreateEventA( NULL, TRUE, FALSE, NULL );
    NTSTATUS status;

    memset(&iosb, 0x55, sizeof(iosb));
    status = NtFsControlFile(pipe, NULL, NULL, NULL, &iosb, FSCTL_PIPE_PEEK, NULL, 0, &buf, sizeof(buf));
    ok(!status || status == STATUS_PENDING, "NtFsControlFile failed: %x\n", status);
    ok(!iosb.Status, "iosb.Status = %x\n", iosb.Status);
    ok(buf.ReadDataAvailable == 1, "ReadDataAvailable = %u\n", buf.ReadDataAvailable);

    ResetEvent(event);
    memset(&iosb, 0x55, sizeof(iosb));
    status = NtFsControlFile(pipe, event, NULL, NULL, &iosb, FSCTL_PIPE_PEEK, NULL, 0, &buf, sizeof(buf));
    ok(!status || status == STATUS_PENDING, "NtFsControlFile failed: %x\n", status);
    ok(buf.ReadDataAvailable == 1, "ReadDataAvailable = %u\n", buf.ReadDataAvailable);
    ok(!iosb.Status, "iosb.Status = %x\n", iosb.Status);
    ok(is_signaled(event), "event is not signaled\n");

    CloseHandle(event);
}

#define PIPENAME "\\\\.\\pipe\\ntdll_tests_pipe.c"

static BOOL create_pipe_pair( HANDLE *read, HANDLE *write, ULONG flags, ULONG type, ULONG size )
{
    HANDLE client, server;

    server = CreateNamedPipeA(PIPENAME, flags, PIPE_WAIT | type,
                              1, size, size, NMPWAIT_USE_DEFAULT_WAIT, NULL);
    ok(server != INVALID_HANDLE_VALUE, "CreateNamedPipe failed\n");

    client = CreateFileA(PIPENAME, (flags & PIPE_ACCESS_INBOUND ? GENERIC_WRITE : 0)
                         | (flags & PIPE_ACCESS_OUTBOUND ? GENERIC_READ : 0)
                         | FILE_WRITE_ATTRIBUTES, 0,
                         NULL, OPEN_EXISTING, flags & FILE_FLAG_OVERLAPPED, 0);
    ok(client != INVALID_HANDLE_VALUE, "CreateFile failed (%d)\n", GetLastError());

    if ((type & PIPE_READMODE_MESSAGE) && (flags & PIPE_ACCESS_OUTBOUND))
    {
        DWORD read_mode = PIPE_READMODE_MESSAGE;
        ok(SetNamedPipeHandleState(client, &read_mode, NULL, NULL), "Change mode\n");
    }

    if (flags & PIPE_ACCESS_INBOUND)
    {
        *read = server;
        *write = client;
    }
    else
    {
        *read = client;
        *write = server;
    }
    return TRUE;
}

static void read_pipe_test(ULONG pipe_flags, ULONG pipe_type)
{
    IO_STATUS_BLOCK iosb, iosb2;
    HANDLE handle, read, write;
    HANDLE event = CreateEventA( NULL, TRUE, FALSE, NULL );
    int apc_count = 0;
    char buffer[128];
    DWORD written;
    BOOL ret;
    NTSTATUS status;

    if (!create_pipe_pair( &read, &write, FILE_FLAG_OVERLAPPED | pipe_flags, pipe_type, 4096 )) return;

    /* try read with no data */
    U(iosb).Status = 0xdeadbabe;
    iosb.Information = 0xdeadbeef;
    ok( is_signaled( read ), "read handle is not signaled\n" );
    status = NtReadFile( read, event, apc, &apc_count, &iosb, buffer, 1, NULL, NULL );
    ok( status == STATUS_PENDING, "wrong status %x\n", status );
    ok( !is_signaled( read ), "read handle is signaled\n" );
    ok( !is_signaled( event ), "event is signaled\n" );
    ok( U(iosb).Status == 0xdeadbabe, "wrong status %x\n", U(iosb).Status );
    ok( iosb.Information == 0xdeadbeef, "wrong info %lu\n", iosb.Information );
    ok( !apc_count, "apc was called\n" );
    ret = WriteFile( write, buffer, 1, &written, NULL );
    ok(ret && written == 1, "WriteFile error %d\n", GetLastError());
    /* iosb updated here by async i/o */
    ok( U(iosb).Status == 0, "wrong status %x\n", U(iosb).Status );
    ok( iosb.Information == 1, "wrong info %lu\n", iosb.Information );
    ok( !is_signaled( read ), "read handle is signaled\n" );
    ok( is_signaled( event ), "event is not signaled\n" );
    ok( !apc_count, "apc was called\n" );
    apc_count = 0;
    SleepEx( 1, FALSE ); /* non-alertable sleep */
    ok( !apc_count, "apc was called\n" );
    SleepEx( 1, TRUE ); /* alertable sleep */
    ok( apc_count == 1, "apc not called\n" );

    /* with no event, the pipe handle itself gets signaled */
    apc_count = 0;
    U(iosb).Status = 0xdeadbabe;
    iosb.Information = 0xdeadbeef;
    ok( !is_signaled( read ), "read handle is signaled\n" );
    status = NtReadFile( read, 0, apc, &apc_count, &iosb, buffer, 1, NULL, NULL );
    ok( status == STATUS_PENDING, "wrong status %x\n", status );
    ok( !is_signaled( read ), "read handle is signaled\n" );
    ok( U(iosb).Status == 0xdeadbabe, "wrong status %x\n", U(iosb).Status );
    ok( iosb.Information == 0xdeadbeef, "wrong info %lu\n", iosb.Information );
    ok( !apc_count, "apc was called\n" );
    ret = WriteFile( write, buffer, 1, &written, NULL );
    ok(ret && written == 1, "WriteFile error %d\n", GetLastError());
    /* iosb updated here by async i/o */
    ok( U(iosb).Status == 0, "wrong status %x\n", U(iosb).Status );
    ok( iosb.Information == 1, "wrong info %lu\n", iosb.Information );
    ok( is_signaled( read ), "read handle is not signaled\n" );
    ok( !apc_count, "apc was called\n" );
    apc_count = 0;
    SleepEx( 1, FALSE ); /* non-alertable sleep */
    ok( !apc_count, "apc was called\n" );
    SleepEx( 1, TRUE ); /* alertable sleep */
    ok( apc_count == 1, "apc not called\n" );

    /* now read with data ready */
    apc_count = 0;
    U(iosb).Status = 0xdeadbabe;
    iosb.Information = 0xdeadbeef;
    ResetEvent( event );
    ret = WriteFile( write, buffer, 1, &written, NULL );
    ok(ret && written == 1, "WriteFile error %d\n", GetLastError());

    test_peek(read);

    status = NtReadFile( read, event, apc, &apc_count, &iosb, buffer, 1, NULL, NULL );
    ok( status == STATUS_SUCCESS, "wrong status %x\n", status );
    ok( U(iosb).Status == 0, "wrong status %x\n", U(iosb).Status );
    ok( iosb.Information == 1, "wrong info %lu\n", iosb.Information );
    ok( is_signaled( event ), "event is not signaled\n" );
    ok( !apc_count, "apc was called\n" );
    SleepEx( 1, FALSE ); /* non-alertable sleep */
    ok( !apc_count, "apc was called\n" );
    SleepEx( 1, TRUE ); /* alertable sleep */
    ok( apc_count == 1, "apc not called\n" );

    /* now partial read with data ready */
    apc_count = 0;
    U(iosb).Status = 0xdeadbabe;
    iosb.Information = 0xdeadbeef;
    ResetEvent( event );
    ret = WriteFile( write, buffer, 2, &written, NULL );
    ok(ret && written == 2, "WriteFile error %d\n", GetLastError());

    memset( &iosb, 0xcc, sizeof(iosb) );
    status = NtFsControlFile( read, NULL, NULL, NULL, &iosb, FSCTL_PIPE_PEEK, NULL, 0, buffer,
                              FIELD_OFFSET(FILE_PIPE_PEEK_BUFFER, Data[1]) );
    if (pipe_type & PIPE_TYPE_MESSAGE)
    {
        ok( status == STATUS_BUFFER_OVERFLOW || status == STATUS_PENDING,
            "FSCTL_PIPE_PEEK returned %x\n", status );
        ok( U(iosb).Status == STATUS_BUFFER_OVERFLOW, "wrong status %x\n", U(iosb).Status );
    }
    else
    {
        ok( !status || status == STATUS_PENDING, "FSCTL_PIPE_PEEK returned %x\n", status );
        ok( U(iosb).Status == 0, "wrong status %x\n", U(iosb).Status );
    }
    ok( iosb.Information == FIELD_OFFSET(FILE_PIPE_PEEK_BUFFER, Data[1]),
        "wrong info %lu\n", iosb.Information );

    status = NtReadFile( read, event, apc, &apc_count, &iosb, buffer, 1, NULL, NULL );
    if (pipe_type & PIPE_READMODE_MESSAGE)
    {
        ok( status == STATUS_BUFFER_OVERFLOW, "wrong status %x\n", status );
        ok( U(iosb).Status == STATUS_BUFFER_OVERFLOW, "wrong status %x\n", U(iosb).Status );
    }
    else
    {
        ok( status == STATUS_SUCCESS, "wrong status %x\n", status );
        ok( U(iosb).Status == 0, "wrong status %x\n", U(iosb).Status );
    }
    ok( iosb.Information == 1, "wrong info %lu\n", iosb.Information );
    ok( is_signaled( event ), "event is not signaled\n" );
    ok( !apc_count, "apc was called\n" );
    SleepEx( 1, FALSE ); /* non-alertable sleep */
    ok( !apc_count, "apc was called\n" );
    SleepEx( 1, TRUE ); /* alertable sleep */
    ok( apc_count == 1, "apc not called\n" );
    apc_count = 0;
    status = NtReadFile( read, event, apc, &apc_count, &iosb, buffer, 1, NULL, NULL );
    ok( status == STATUS_SUCCESS, "wrong status %x\n", status );
    ok( U(iosb).Status == 0, "wrong status %x\n", U(iosb).Status );
    ok( iosb.Information == 1, "wrong info %lu\n", iosb.Information );
    ok( is_signaled( event ), "event is not signaled\n" );
    ok( !apc_count, "apc was called\n" );
    SleepEx( 1, FALSE ); /* non-alertable sleep */
    ok( !apc_count, "apc was called\n" );
    SleepEx( 1, TRUE ); /* alertable sleep */
    ok( apc_count == 1, "apc not called\n" );

    /* try read with no data */
    apc_count = 0;
    U(iosb).Status = 0xdeadbabe;
    iosb.Information = 0xdeadbeef;
    ok( is_signaled( event ), "event is not signaled\n" ); /* check that read resets the event */
    status = NtReadFile( read, event, apc, &apc_count, &iosb, buffer, 2, NULL, NULL );
    ok( status == STATUS_PENDING, "wrong status %x\n", status );
    ok( !is_signaled( event ), "event is signaled\n" );
    ok( U(iosb).Status == 0xdeadbabe, "wrong status %x\n", U(iosb).Status );
    ok( iosb.Information == 0xdeadbeef, "wrong info %lu\n", iosb.Information );
    ok( !apc_count, "apc was called\n" );
    ret = WriteFile( write, buffer, 1, &written, NULL );
    ok(ret && written == 1, "WriteFile error %d\n", GetLastError());
    /* partial read is good enough */
    Sleep(1);  /* FIXME: needed for wine to run the i/o apc  */
    ok( is_signaled( event ), "event is not signaled\n" );
    ok( U(iosb).Status == 0, "wrong status %x\n", U(iosb).Status );
    ok( iosb.Information == 1, "wrong info %lu\n", iosb.Information );
    ok( !apc_count, "apc was called\n" );
    SleepEx( 1, TRUE ); /* alertable sleep */
    ok( apc_count == 1, "apc was not called\n" );

    /* read from disconnected pipe */
    apc_count = 0;
    U(iosb).Status = 0xdeadbabe;
    iosb.Information = 0xdeadbeef;
    CloseHandle( write );
    status = NtReadFile( read, event, apc, &apc_count, &iosb, buffer, 1, NULL, NULL );
    ok( status == STATUS_PIPE_BROKEN, "wrong status %x\n", status );
    ok( U(iosb).Status == 0xdeadbabe, "wrong status %x\n", U(iosb).Status );
    ok( iosb.Information == 0xdeadbeef, "wrong info %lu\n", iosb.Information );
    ok( !is_signaled( event ), "event is signaled\n" );
    ok( !apc_count, "apc was called\n" );
    SleepEx( 1, TRUE ); /* alertable sleep */
    ok( !apc_count, "apc was called\n" );
    CloseHandle( read );

    /* read from disconnected pipe, with invalid event handle */
    apc_count = 0;
    U(iosb).Status = 0xdeadbabe;
    iosb.Information = 0xdeadbeef;
    status = NtReadFile( read, (HANDLE)0xdeadbeef, apc, &apc_count, &iosb, buffer, 1, NULL, NULL );
    ok( status == STATUS_INVALID_HANDLE, "wrong status %x\n", status );
    ok( U(iosb).Status == 0xdeadbabe, "wrong status %x\n", U(iosb).Status );
    ok( iosb.Information == 0xdeadbeef, "wrong info %lu\n", iosb.Information );
    ok( !apc_count, "apc was called\n" );
    SleepEx( 1, TRUE ); /* alertable sleep */
    ok( !apc_count, "apc was called\n" );
    CloseHandle( read );

    /* read from closed handle */
    apc_count = 0;
    U(iosb).Status = 0xdeadbabe;
    iosb.Information = 0xdeadbeef;
    SetEvent( event );
    status = NtReadFile( read, event, apc, &apc_count, &iosb, buffer, 1, NULL, NULL );
    ok( status == STATUS_INVALID_HANDLE, "wrong status %x\n", status );
    ok( U(iosb).Status == 0xdeadbabe, "wrong status %x\n", U(iosb).Status );
    ok( iosb.Information == 0xdeadbeef, "wrong info %lu\n", iosb.Information );
    ok( is_signaled( event ), "event is not signaled\n" );  /* not reset on invalid handle */
    ok( !apc_count, "apc was called\n" );
    SleepEx( 1, TRUE ); /* alertable sleep */
    ok( !apc_count, "apc was called\n" );

    /* disconnect while async read is in progress */
    if (!create_pipe_pair( &read, &write, FILE_FLAG_OVERLAPPED | pipe_flags, pipe_type, 4096 )) return;
    apc_count = 0;
    U(iosb).Status = 0xdeadbabe;
    iosb.Information = 0xdeadbeef;
    status = NtReadFile( read, event, apc, &apc_count, &iosb, buffer, 2, NULL, NULL );
    ok( status == STATUS_PENDING, "wrong status %x\n", status );
    ok( !is_signaled( event ), "event is signaled\n" );
    ok( U(iosb).Status == 0xdeadbabe, "wrong status %x\n", U(iosb).Status );
    ok( iosb.Information == 0xdeadbeef, "wrong info %lu\n", iosb.Information );
    ok( !apc_count, "apc was called\n" );
    CloseHandle( write );
    Sleep(1);  /* FIXME: needed for wine to run the i/o apc  */
    ok( U(iosb).Status == STATUS_PIPE_BROKEN, "wrong status %x\n", U(iosb).Status );
    ok( iosb.Information == 0, "wrong info %lu\n", iosb.Information );
    ok( is_signaled( event ), "event is not signaled\n" );
    ok( !apc_count, "apc was called\n" );
    SleepEx( 1, TRUE ); /* alertable sleep */
    ok( apc_count == 1, "apc was not called\n" );
    CloseHandle( read );

    if (!create_pipe_pair( &read, &write, FILE_FLAG_OVERLAPPED | pipe_flags, pipe_type, 4096 )) return;
    ret = DuplicateHandle(GetCurrentProcess(), read, GetCurrentProcess(), &handle, 0, TRUE, DUPLICATE_SAME_ACCESS);
    ok(ret, "Failed to duplicate handle: %d\n", GetLastError());

    apc_count = 0;
    U(iosb).Status = 0xdeadbabe;
    iosb.Information = 0xdeadbeef;
    status = NtReadFile( handle, event, apc, &apc_count, &iosb, buffer, 2, NULL, NULL );
    ok( status == STATUS_PENDING, "wrong status %x\n", status );
    ok( !is_signaled( event ), "event is signaled\n" );
    ok( U(iosb).Status == 0xdeadbabe, "wrong status %x\n", U(iosb).Status );
    ok( iosb.Information == 0xdeadbeef, "wrong info %lu\n", iosb.Information );
    ok( !apc_count, "apc was called\n" );
    /* Cancel by other handle */
    status = pNtCancelIoFile( read, &iosb2 );
    ok(status == STATUS_SUCCESS, "failed to cancel by different handle: %x\n", status);
    Sleep(1);  /* FIXME: needed for wine to run the i/o apc  */
    ok( U(iosb).Status == STATUS_CANCELLED, "wrong status %x\n", U(iosb).Status );
    ok( iosb.Information == 0, "wrong info %lu\n", iosb.Information );
    ok( is_signaled( event ), "event is not signaled\n" );
    ok( !apc_count, "apc was called\n" );
    SleepEx( 1, TRUE ); /* alertable sleep */
    ok( apc_count == 1, "apc was not called\n" );

    apc_count = 0;
    U(iosb).Status = 0xdeadbabe;
    iosb.Information = 0xdeadbeef;
    status = NtReadFile( read, event, apc, &apc_count, &iosb, buffer, 2, NULL, NULL );
    ok( status == STATUS_PENDING, "wrong status %x\n", status );
    ok( !is_signaled( event ), "event is signaled\n" );
    ok( U(iosb).Status == 0xdeadbabe, "wrong status %x\n", U(iosb).Status );
    ok( iosb.Information == 0xdeadbeef, "wrong info %lu\n", iosb.Information );
    ok( !apc_count, "apc was called\n" );
    /* Close queued handle */
    CloseHandle( read );
    SleepEx( 1, TRUE ); /* alertable sleep */
    ok( U(iosb).Status == 0xdeadbabe, "wrong status %x\n", U(iosb).Status );
    ok( iosb.Information == 0xdeadbeef, "wrong info %lu\n", iosb.Information );
    status = pNtCancelIoFile( read, &iosb2 );
    ok(status == STATUS_INVALID_HANDLE, "cancelled by closed handle?\n");
    status = pNtCancelIoFile( handle, &iosb2 );
    ok(status == STATUS_SUCCESS, "failed to cancel: %x\n", status);
    Sleep(1);  /* FIXME: needed for wine to run the i/o apc  */
    ok( U(iosb).Status == STATUS_CANCELLED, "wrong status %x\n", U(iosb).Status );
    ok( iosb.Information == 0, "wrong info %lu\n", iosb.Information );
    ok( is_signaled( event ), "event is not signaled\n" );
    ok( !apc_count, "apc was called\n" );
    SleepEx( 1, TRUE ); /* alertable sleep */
    ok( apc_count == 1, "apc was not called\n" );
    CloseHandle( handle );
    CloseHandle( write );

    if (pNtCancelIoFileEx)
    {
        /* Basic Cancel Ex */
        if (!create_pipe_pair( &read, &write, FILE_FLAG_OVERLAPPED | pipe_flags, pipe_type, 4096 )) return;

        apc_count = 0;
        U(iosb).Status = 0xdeadbabe;
        iosb.Information = 0xdeadbeef;
        status = NtReadFile( read, event, apc, &apc_count, &iosb, buffer, 2, NULL, NULL );
        ok( status == STATUS_PENDING, "wrong status %x\n", status );
        ok( !is_signaled( event ), "event is signaled\n" );
        ok( U(iosb).Status == 0xdeadbabe, "wrong status %x\n", U(iosb).Status );
        ok( iosb.Information == 0xdeadbeef, "wrong info %lu\n", iosb.Information );
        ok( !apc_count, "apc was called\n" );
        status = pNtCancelIoFileEx( read, &iosb, &iosb2 );
        ok(status == STATUS_SUCCESS, "Failed to cancel I/O\n");
        Sleep(1);  /* FIXME: needed for wine to run the i/o apc  */
        ok( U(iosb).Status == STATUS_CANCELLED, "wrong status %x\n", U(iosb).Status );
        ok( iosb.Information == 0, "wrong info %lu\n", iosb.Information );
        ok( is_signaled( event ), "event is not signaled\n" );
        ok( !apc_count, "apc was called\n" );
        SleepEx( 1, TRUE ); /* alertable sleep */
        ok( apc_count == 1, "apc was not called\n" );

        /* Duplicate iosb */
        apc_count = 0;
        U(iosb).Status = 0xdeadbabe;
        iosb.Information = 0xdeadbeef;
        status = NtReadFile( read, event, apc, &apc_count, &iosb, buffer, 2, NULL, NULL );
        ok( status == STATUS_PENDING, "wrong status %x\n", status );
        ok( !is_signaled( event ), "event is signaled\n" );
        ok( U(iosb).Status == 0xdeadbabe, "wrong status %x\n", U(iosb).Status );
        ok( iosb.Information == 0xdeadbeef, "wrong info %lu\n", iosb.Information );
        ok( !apc_count, "apc was called\n" );
        status = NtReadFile( read, event, apc, &apc_count, &iosb, buffer, 2, NULL, NULL );
        ok( status == STATUS_PENDING, "wrong status %x\n", status );
        ok( !is_signaled( event ), "event is signaled\n" );
        ok( U(iosb).Status == 0xdeadbabe, "wrong status %x\n", U(iosb).Status );
        ok( iosb.Information == 0xdeadbeef, "wrong info %lu\n", iosb.Information );
        ok( !apc_count, "apc was called\n" );
        status = pNtCancelIoFileEx( read, &iosb, &iosb2 );
        ok(status == STATUS_SUCCESS, "Failed to cancel I/O\n");
        Sleep(1);  /* FIXME: needed for wine to run the i/o apc  */
        ok( U(iosb).Status == STATUS_CANCELLED, "wrong status %x\n", U(iosb).Status );
        ok( iosb.Information == 0, "wrong info %lu\n", iosb.Information );
        ok( is_signaled( event ), "event is not signaled\n" );
        ok( !apc_count, "apc was called\n" );
        SleepEx( 1, TRUE ); /* alertable sleep */
        ok( apc_count == 2, "apc was not called\n" );

        CloseHandle( read );
        CloseHandle( write );
    }
    else
        win_skip("NtCancelIoFileEx not available\n");

    CloseHandle(event);
}

static void test_transceive(void)
{
    IO_STATUS_BLOCK iosb;
    HANDLE caller, callee;
    HANDLE event = CreateEventA( NULL, TRUE, FALSE, NULL );
    char buffer[128];
    DWORD written;
    BOOL ret;
    NTSTATUS status;

    if (!create_pipe_pair( &caller, &callee, FILE_FLAG_OVERLAPPED | PIPE_ACCESS_DUPLEX,
                           PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE, 4096 )) return;

    status = NtFsControlFile( caller, event, NULL, NULL, &iosb, FSCTL_PIPE_TRANSCEIVE,
                              (BYTE*)"test", 4, buffer, sizeof(buffer) );
    ok( status == STATUS_PENDING, "NtFsControlFile(FSCTL_PIPE_TRANSCEIVE) returned %x\n", status);
    ok( !is_signaled( event ), "event is signaled\n" );

    ret = WriteFile( callee, buffer, 2, &written, NULL );
    ok(ret && written == 2, "WriteFile error %d\n", GetLastError());

    ok( U(iosb).Status == 0, "wrong status %x\n", U(iosb).Status );
    ok( iosb.Information == 2, "wrong info %lu\n", iosb.Information );
    ok( is_signaled( event ), "event is not signaled\n" );

    CloseHandle( caller );
    CloseHandle( callee );
}

#define test_no_queued_completion(a) _test_no_queued_completion(__LINE__,a)
static void _test_no_queued_completion(unsigned line, HANDLE port)
{
    OVERLAPPED *pov;
    DWORD num_bytes;
    ULONG_PTR key;
    BOOL ret;

    pov = (void *)0xdeadbeef;
    ret = GetQueuedCompletionStatus(port, &num_bytes, &key, &pov, 10);
    ok_(__FILE__,line)(!ret && GetLastError() == WAIT_TIMEOUT,
                       "GetQueuedCompletionStatus returned %x(%u)\n", ret, GetLastError());
}

#define test_queued_completion(a,b,c,d) _test_queued_completion(__LINE__,a,b,c,d)
static void _test_queued_completion(unsigned line, HANDLE port, IO_STATUS_BLOCK *io,
                                    NTSTATUS expected_status, ULONG expected_information)
{
    LARGE_INTEGER timeout = {{0}};
    ULONG_PTR value = 0xdeadbeef;
    IO_STATUS_BLOCK iosb;
    ULONG_PTR key;
    NTSTATUS status;

    status = pNtRemoveIoCompletion(port, &key, &value, &iosb, &timeout);
    ok_(__FILE__,line)(status == STATUS_SUCCESS, "NtRemoveIoCompletion returned %x\n", status);
    ok_(__FILE__,line)(value == (ULONG_PTR)io, "value = %lx\n", value);
    ok_(__FILE__,line)(io->Status == expected_status, "Status = %x\n", io->Status);
    ok_(__FILE__,line)(io->Information == expected_information,
                       "Information = %lu\n", io->Information);
}

static void test_completion(void)
{
    static const char buf[] = "testdata";
    FILE_IO_COMPLETION_NOTIFICATION_INFORMATION info;
    FILE_PIPE_PEEK_BUFFER peek_buf;
    char read_buf[16];
    HANDLE port, pipe, client, event;
    OVERLAPPED ov;
    IO_STATUS_BLOCK io;
    NTSTATUS status;
    DWORD num_bytes;
    BOOL ret;

    create_pipe_pair( &pipe, &client, FILE_FLAG_OVERLAPPED | PIPE_ACCESS_DUPLEX,
                      PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE, 4096 );

    status = pNtQueryInformationFile(pipe, &io, &info, sizeof(info),
                                     FileIoCompletionNotificationInformation);
    ok(status == STATUS_SUCCESS || broken(status == STATUS_INVALID_INFO_CLASS),
       "status = %x\n", status);
    if (status)
    {
        win_skip("FileIoCompletionNotificationInformation not supported\n");
        CloseHandle(pipe);
        CloseHandle(client);
        return;
    }

    memset(&ov, 0, sizeof(ov));
    ov.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    ok(ov.hEvent != INVALID_HANDLE_VALUE, "CreateEvent failed, error %u\n", GetLastError());

    port = CreateIoCompletionPort(client, NULL, 0xdeadbeef, 0);
    ok(port != NULL, "CreateIoCompletionPort failed, error %u\n", GetLastError());

    ret = WriteFile(client, buf, sizeof(buf), &num_bytes, &ov);
    ok(ret, "WriteFile failed, error %u\n", GetLastError());
    ok(num_bytes == sizeof(buf), "expected sizeof(buf), got %u\n", num_bytes);
    test_queued_completion(port, (IO_STATUS_BLOCK*)&ov, STATUS_SUCCESS, num_bytes);

    status = NtFsControlFile(client, NULL, NULL, &io, &io, FSCTL_PIPE_PEEK,
                             NULL, 0, &peek_buf, sizeof(peek_buf));
    ok(status == STATUS_PENDING || status == STATUS_SUCCESS, "FSCTL_PIPE_PEEK returned %x\n", status);
    test_queued_completion(port, &io, STATUS_SUCCESS, FIELD_OFFSET(FILE_PIPE_PEEK_BUFFER, Data));

    info.Flags = FILE_SKIP_COMPLETION_PORT_ON_SUCCESS;
    status = pNtSetInformationFile(client, &io, &info, sizeof(info), FileIoCompletionNotificationInformation);
    ok(status == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %08x\n", status);

    ret = WriteFile(client, buf, sizeof(buf), &num_bytes, &ov);
    ok(ret, "WriteFile failed, error %u\n", GetLastError());
    ok(num_bytes == sizeof(buf), "expected sizeof(buf), got %u\n", num_bytes);
    test_no_queued_completion(port);

    ret = WriteFile(pipe, buf, sizeof(buf), &num_bytes, &ov);
    ok(ret, "WriteFile failed, error %u\n", GetLastError());
    ok(num_bytes == sizeof(buf), "expected sizeof(buf), got %u\n", num_bytes);

    status = NtReadFile(client, NULL, NULL, &io, &io, read_buf, 1, NULL, NULL);
    ok(status == STATUS_BUFFER_OVERFLOW || status == STATUS_PENDING, "status = %x\n", status);
    ok(io.Status == STATUS_BUFFER_OVERFLOW, "Status = %x\n", io.Status);
    ok(io.Information == 1, "Information = %lu\n", io.Information);
    if(status == STATUS_PENDING) /* win8+ */
        test_queued_completion(port, &io, STATUS_BUFFER_OVERFLOW, 1);
    else
        test_no_queued_completion(port);

    status = NtReadFile(client, NULL, NULL, &io, &io, read_buf, sizeof(read_buf), NULL, NULL);
    ok(status == STATUS_SUCCESS, "status = %x\n", status);
    ok(io.Status == STATUS_SUCCESS, "Status = %x\n", io.Status);
    ok(io.Information == sizeof(buf)-1, "Information = %lu\n", io.Information);
    test_no_queued_completion(port);

    status = NtFsControlFile(client, NULL, NULL, &io, &io, FSCTL_PIPE_PEEK,
                             NULL, 0, &peek_buf, sizeof(peek_buf));
    ok(status == STATUS_PENDING || status == STATUS_SUCCESS, "FSCTL_PIPE_PEEK returned %x\n", status);
    if(status == STATUS_PENDING) /* win8+ */
        test_queued_completion(port, &io, STATUS_SUCCESS, FIELD_OFFSET(FILE_PIPE_PEEK_BUFFER, Data));
    else
        test_no_queued_completion(port);

    memset(&io, 0xcc, sizeof(io));
    status = NtReadFile(client, ov.hEvent, NULL, &io, &io, read_buf, sizeof(read_buf), NULL, NULL);
    ok(status == STATUS_PENDING, "status = %x\n", status);
    ok(!is_signaled(ov.hEvent), "event is signtaled\n");
    test_no_queued_completion(port);

    ret = WriteFile(pipe, buf, sizeof(buf), &num_bytes, NULL);
    ok(ret, "WriteFile failed, error %u\n", GetLastError());
    test_queued_completion(port, &io, STATUS_SUCCESS, sizeof(buf));

    ret = WriteFile(pipe, buf, sizeof(buf), &num_bytes, NULL);
    ok(ret, "WriteFile failed, error %u\n", GetLastError());
    status = NtFsControlFile(client, NULL, NULL, &io, &io, FSCTL_PIPE_PEEK,
                             NULL, 0, &peek_buf, sizeof(peek_buf));
    ok(status == STATUS_PENDING || status == STATUS_BUFFER_OVERFLOW,
       "FSCTL_PIPE_PEEK returned %x\n", status);
    if(status == STATUS_PENDING) /* win8+ */
        test_queued_completion(port, &io, STATUS_BUFFER_OVERFLOW, sizeof(peek_buf));
    else
        test_no_queued_completion(port);

    CloseHandle(ov.hEvent);
    CloseHandle(client);
    CloseHandle(pipe);
    CloseHandle(port);

    event = CreateEventW(NULL, TRUE, TRUE, NULL);
    create_pipe_pair( &pipe, &client, FILE_FLAG_OVERLAPPED | PIPE_ACCESS_DUPLEX,
                      PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE, 4096 );

    ok(is_signaled(client), "client is not signaled\n");

    /* no event, APC nor completion: only signals on handle */
    memset(&io, 0xcc, sizeof(io));
    status = NtReadFile(client, NULL, NULL, NULL, &io, read_buf, sizeof(read_buf), NULL, NULL);
    ok(status == STATUS_PENDING, "status = %x\n", status);
    ok(!is_signaled(client), "client is signaled\n");

    ret = WriteFile(pipe, buf, sizeof(buf), &num_bytes, NULL);
    ok(ret, "WriteFile failed, error %u\n", GetLastError());
    ok(is_signaled(client), "client is signaled\n");
    ok(io.Status == STATUS_SUCCESS, "Status = %x\n", io.Status);
    ok(io.Information == sizeof(buf), "Information = %lu\n", io.Information);

    /* event with no APC nor completion: signals only event */
    memset(&io, 0xcc, sizeof(io));
    status = NtReadFile(client, event, NULL, NULL, &io, read_buf, sizeof(read_buf), NULL, NULL);
    ok(status == STATUS_PENDING, "status = %x\n", status);
    ok(!is_signaled(client), "client is signaled\n");
    ok(!is_signaled(event), "event is signaled\n");

    ret = WriteFile(pipe, buf, sizeof(buf), &num_bytes, NULL);
    ok(ret, "WriteFile failed, error %u\n", GetLastError());
    ok(!is_signaled(client), "client is signaled\n");
    ok(is_signaled(event), "event is not signaled\n");
    ok(io.Status == STATUS_SUCCESS, "Status = %x\n", io.Status);
    ok(io.Information == sizeof(buf), "Information = %lu\n", io.Information);

    /* APC with no event: handle is signaled */
    ioapc_called = FALSE;
    memset(&io, 0xcc, sizeof(io));
    status = NtReadFile(client, NULL, ioapc, &io, &io, read_buf, sizeof(read_buf), NULL, NULL);
    ok(status == STATUS_PENDING, "status = %x\n", status);
    ok(!is_signaled(client), "client is signaled\n");

    ret = WriteFile(pipe, buf, sizeof(buf), &num_bytes, NULL);
    ok(ret, "WriteFile failed, error %u\n", GetLastError());
    ok(is_signaled(client), "client is signaled\n");
    ok(io.Status == STATUS_SUCCESS, "Status = %x\n", io.Status);
    ok(io.Information == sizeof(buf), "Information = %lu\n", io.Information);

    ok(!ioapc_called, "ioapc called\n");
    SleepEx(0, TRUE);
    ok(ioapc_called, "ioapc not called\n");

    /* completion with no completion port: handle signaled */
    memset(&io, 0xcc, sizeof(io));
    status = NtReadFile(client, NULL, NULL, &io, &io, read_buf, sizeof(read_buf), NULL, NULL);
    ok(status == STATUS_PENDING, "status = %x\n", status);
    ok(!is_signaled(client), "client is signaled\n");

    ret = WriteFile(pipe, buf, sizeof(buf), &num_bytes, NULL);
    ok(ret, "WriteFile failed, error %u\n", GetLastError());
    ok(is_signaled(client), "client is not signaled\n");

    port = CreateIoCompletionPort(client, NULL, 0xdeadbeef, 0);
    ok(port != NULL, "CreateIoCompletionPort failed, error %u\n", GetLastError());

    /* skipping completion on success: handle is signaled */
    info.Flags = FILE_SKIP_COMPLETION_PORT_ON_SUCCESS;
    status = pNtSetInformationFile(client, &io, &info, sizeof(info), FileIoCompletionNotificationInformation);
    ok(status == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %08x\n", status);
    ok(is_signaled(client), "client is not signaled\n");

    memset(&io, 0xcc, sizeof(io));
    status = NtReadFile(client, NULL, NULL, &io, &io, read_buf, sizeof(read_buf), NULL, NULL);
    ok(status == STATUS_PENDING, "status = %x\n", status);
    ok(!is_signaled(client), "client is signaled\n");

    ret = WriteFile(client, buf, 1, &num_bytes, NULL);
    ok(ret, "WriteFile failed, error %u\n", GetLastError());
    ok(is_signaled(client), "client is not signaled\n");

    /* skipping set event on handle: handle is never signaled */
    info.Flags = FILE_SKIP_SET_EVENT_ON_HANDLE;
    status = pNtSetInformationFile(client, &io, &info, sizeof(info), FileIoCompletionNotificationInformation);
    ok(status == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %08x\n", status);
    ok(!is_signaled(client), "client is not signaled\n");

    ret = WriteFile(pipe, buf, sizeof(buf), &num_bytes, NULL);
    ok(ret, "WriteFile failed, error %u\n", GetLastError());
    ok(!is_signaled(client), "client is signaled\n");
    test_queued_completion(port, &io, STATUS_SUCCESS, sizeof(buf));

    memset(&io, 0xcc, sizeof(io));
    status = NtReadFile(client, NULL, NULL, NULL, &io, read_buf, sizeof(read_buf), NULL, NULL);
    ok(status == STATUS_PENDING, "status = %x\n", status);
    ok(!is_signaled(client), "client is signaled\n");

    ret = WriteFile(client, buf, 1, &num_bytes, NULL);
    ok(ret, "WriteFile failed, error %u\n", GetLastError());
    ok(!is_signaled(client), "client is signaled\n");

    ret = WriteFile(pipe, buf, sizeof(buf), &num_bytes, NULL);
    ok(ret, "WriteFile failed, error %u\n", GetLastError());
    ok(!is_signaled(client), "client is signaled\n");

    CloseHandle(port);
    CloseHandle(client);
    CloseHandle(pipe);
}

struct blocking_thread_args
{
    HANDLE wait;
    HANDLE done;
    enum {
        BLOCKING_THREAD_WRITE,
        BLOCKING_THREAD_READ,
        BLOCKING_THREAD_QUIT
    } cmd;
    HANDLE client;
    HANDLE pipe;
    HANDLE event;
};

static DWORD WINAPI blocking_thread(void *arg)
{
    struct blocking_thread_args *ctx = arg;
    static const char buf[] = "testdata";
    char read_buf[32];
    DWORD res, num_bytes;
    BOOL ret;

    for (;;)
    {
        res = WaitForSingleObject(ctx->wait, 10000);
        ok(res == WAIT_OBJECT_0, "wait returned %x\n", res);
        if (res != WAIT_OBJECT_0) break;
        switch(ctx->cmd) {
        case BLOCKING_THREAD_WRITE:
            Sleep(100);
            if(ctx->event)
                ok(!is_signaled(ctx->event), "event is signaled\n");
            ok(!ioapc_called, "ioapc called\n");
            ok(!is_signaled(ctx->client), "client is signaled\n");
            ok(is_signaled(ctx->pipe), "pipe is not signaled\n");
            ret = WriteFile(ctx->pipe, buf, 1, &num_bytes, NULL);
            ok(ret, "WriteFile failed, error %u\n", GetLastError());
            break;
        case BLOCKING_THREAD_READ:
            Sleep(100);
            if(ctx->event)
                ok(!is_signaled(ctx->event), "event is signaled\n");
            ok(!ioapc_called, "ioapc called\n");
            ok(!is_signaled(ctx->client), "client is signaled\n");
            ok(is_signaled(ctx->pipe), "pipe is not signaled\n");
            ret = ReadFile(ctx->pipe, read_buf, 1, &num_bytes, NULL);
            ok(ret, "WriteFile failed, error %u\n", GetLastError());
            break;
        case BLOCKING_THREAD_QUIT:
            return 0;
        default:
            ok(0, "unvalid command\n");
        }
        SetEvent(ctx->done);
    }

    return 1;
}

static void test_blocking(ULONG options)
{
    struct blocking_thread_args ctx;
    OBJECT_ATTRIBUTES attr;
    UNICODE_STRING name;
    char read_buf[16];
    HANDLE thread;
    IO_STATUS_BLOCK io;
    NTSTATUS status;
    DWORD res, num_bytes;
    BOOL ret;

    ctx.wait = CreateEventW(NULL, FALSE, FALSE, NULL);
    ctx.done = CreateEventW(NULL, FALSE, FALSE, NULL);
    thread = CreateThread(NULL, 0, blocking_thread, &ctx, 0, 0);
    ok(thread != INVALID_HANDLE_VALUE, "can't create thread, GetLastError: %x\n", GetLastError());

    status = create_pipe(&ctx.pipe, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                         options);
    ok(status == STATUS_SUCCESS, "NtCreateNamedPipeFile returned %x\n", status);

    pRtlInitUnicodeString(&name, testpipe_nt);
    attr.Length                   = sizeof(attr);
    attr.RootDirectory            = 0;
    attr.ObjectName               = &name;
    attr.Attributes               = OBJ_CASE_INSENSITIVE;
    attr.SecurityDescriptor       = NULL;
    attr.SecurityQualityOfService = NULL;
    status = NtCreateFile(&ctx.client, SYNCHRONIZE | GENERIC_READ | GENERIC_WRITE, &attr, &io,
                          NULL, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_OPEN,
                          options, NULL, 0 );
    ok(status == STATUS_SUCCESS, "NtCreateFile returned %x\n", status);

    ok(is_signaled(ctx.client), "client is not signaled\n");
    ok(is_signaled(ctx.pipe), "pipe is not signaled\n");

    /* blocking read with no event nor APC */
    ioapc_called = FALSE;
    memset(&io, 0xff, sizeof(io));
    ctx.cmd = BLOCKING_THREAD_WRITE;
    ctx.event = NULL;
    SetEvent(ctx.wait);
    status = NtReadFile(ctx.client, NULL, NULL, NULL, &io, read_buf, sizeof(read_buf), NULL, NULL);
    ok(status == STATUS_SUCCESS, "status = %x\n", status);
    ok(io.Status == STATUS_SUCCESS, "Status = %x\n", io.Status);
    ok(io.Information == 1, "Information = %lu\n", io.Information);
    ok(is_signaled(ctx.client), "client is not signaled\n");
    ok(is_signaled(ctx.pipe), "pipe is not signaled\n");

    res = WaitForSingleObject(ctx.done, 10000);
    ok(res == WAIT_OBJECT_0, "wait returned %x\n", res);

    /* blocking read with event and APC */
    ioapc_called = FALSE;
    memset(&io, 0xff, sizeof(io));
    ctx.cmd = BLOCKING_THREAD_WRITE;
    ctx.event = CreateEventW(NULL, TRUE, TRUE, NULL);
    SetEvent(ctx.wait);
    status = NtReadFile(ctx.client, ctx.event, ioapc, &io, &io, read_buf,
                        sizeof(read_buf), NULL, NULL);
    ok(status == STATUS_SUCCESS, "status = %x\n", status);
    ok(io.Status == STATUS_SUCCESS, "Status = %x\n", io.Status);
    ok(io.Information == 1, "Information = %lu\n", io.Information);
    ok(is_signaled(ctx.event), "event is not signaled\n");
    todo_wine
    ok(is_signaled(ctx.client), "client is not signaled\n");
    ok(is_signaled(ctx.pipe), "pipe is not signaled\n");

    if (!(options & FILE_SYNCHRONOUS_IO_ALERT))
        ok(!ioapc_called, "ioapc called\n");
    SleepEx(0, TRUE); /* alertable wait state */
    ok(ioapc_called, "ioapc not called\n");

    res = WaitForSingleObject(ctx.done, 10000);
    ok(res == WAIT_OBJECT_0, "wait returned %x\n", res);
    ioapc_called = FALSE;
    CloseHandle(ctx.event);
    ctx.event = NULL;

    /* blocking flush */
    ret = WriteFile(ctx.client, read_buf, 1, &num_bytes, NULL);
    ok(ret, "WriteFile failed, error %u\n", GetLastError());

    ioapc_called = FALSE;
    memset(&io, 0xff, sizeof(io));
    ctx.cmd = BLOCKING_THREAD_READ;
    SetEvent(ctx.wait);
    status = NtFlushBuffersFile(ctx.client, &io);
    ok(status == STATUS_SUCCESS, "status = %x\n", status);
    ok(io.Status == STATUS_SUCCESS, "Status = %x\n", io.Status);
    ok(io.Information == 0, "Information = %lu\n", io.Information);
    ok(is_signaled(ctx.client), "client is not signaled\n");

    res = WaitForSingleObject(ctx.done, 10000);
    ok(res == WAIT_OBJECT_0, "wait returned %x\n", res);

    ok(is_signaled(ctx.pipe), "pipe is not signaled\n");
    CloseHandle(ctx.pipe);
    CloseHandle(ctx.client);

    /* flush is blocking even in overlapped mode */
    create_pipe_pair(&ctx.pipe, &ctx.client, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                     PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE, 4096);

    ok(is_signaled(ctx.client), "client is not signaled\n");
    ok(is_signaled(ctx.pipe), "pipe is not signaled\n");

    ret = WriteFile(ctx.client, read_buf, 1, &num_bytes, NULL);
    ok(ret, "WriteFile failed, error %u\n", GetLastError());

    ok(is_signaled(ctx.client), "client is not signaled\n");
    ok(is_signaled(ctx.pipe), "pipe is not signaled\n");

    ioapc_called = FALSE;
    memset(&io, 0xff, sizeof(io));
    ctx.cmd = BLOCKING_THREAD_READ;
    SetEvent(ctx.wait);
    status = NtFlushBuffersFile(ctx.client, &io);
    ok(status == STATUS_SUCCESS, "status = %x\n", status);
    ok(io.Status == STATUS_SUCCESS, "Status = %x\n", io.Status);
    ok(io.Information == 0, "Information = %lu\n", io.Information);
    /* client signaling is inconsistent in this case */

    res = WaitForSingleObject(ctx.done, 10000);
    ok(res == WAIT_OBJECT_0, "wait returned %x\n", res);

    CloseHandle(ctx.pipe);
    CloseHandle(ctx.client);

    ctx.cmd = BLOCKING_THREAD_QUIT;
    SetEvent(ctx.wait);
    res = WaitForSingleObject(thread, 10000);
    ok(res == WAIT_OBJECT_0, "wait returned %x\n", res);

    CloseHandle(ctx.wait);
    CloseHandle(ctx.done);
    CloseHandle(thread);
}

static void test_volume_info(void)
{
    FILE_FS_DEVICE_INFORMATION *device_info;
    IO_STATUS_BLOCK iosb;
    HANDLE read, write;
    char buffer[128];
    NTSTATUS status;

    if (!create_pipe_pair( &read, &write, FILE_FLAG_OVERLAPPED | PIPE_ACCESS_INBOUND,
                           PIPE_TYPE_MESSAGE, 4096 )) return;

    memset( buffer, 0xaa, sizeof(buffer) );
    status = pNtQueryVolumeInformationFile( read, &iosb, buffer, sizeof(buffer), FileFsDeviceInformation );
    ok( status == STATUS_SUCCESS, "NtQueryVolumeInformationFile failed: %x\n", status );
    ok( iosb.Information == sizeof(*device_info), "Information = %lu\n", iosb.Information );
    device_info = (FILE_FS_DEVICE_INFORMATION*)buffer;
    ok( device_info->DeviceType == FILE_DEVICE_NAMED_PIPE, "DeviceType = %u\n", device_info->DeviceType );
    ok( !(device_info->Characteristics & ~FILE_DEVICE_ALLOW_APPCONTAINER_TRAVERSAL),
        "Characteristics = %x\n", device_info->Characteristics );

    memset( buffer, 0xaa, sizeof(buffer) );
    status = pNtQueryVolumeInformationFile( write, &iosb, buffer, sizeof(buffer), FileFsDeviceInformation );
    ok( status == STATUS_SUCCESS, "NtQueryVolumeInformationFile failed: %x\n", status );
    ok( iosb.Information == sizeof(*device_info), "Information = %lu\n", iosb.Information );
    device_info = (FILE_FS_DEVICE_INFORMATION*)buffer;
    ok( device_info->DeviceType == FILE_DEVICE_NAMED_PIPE, "DeviceType = %u\n", device_info->DeviceType );
    ok( !(device_info->Characteristics & ~FILE_DEVICE_ALLOW_APPCONTAINER_TRAVERSAL),
        "Characteristics = %x\n", device_info->Characteristics );

    CloseHandle( read );
    CloseHandle( write );
}

#define test_file_name_fail(a,b,c) _test_file_name_fail(__LINE__,a,b,c)
static void _test_file_name_fail(unsigned line, HANDLE pipe, NTSTATUS expected_status, BOOL todo)
{
    char buffer[512];
    IO_STATUS_BLOCK iosb;
    NTSTATUS status;

    status = NtQueryInformationFile( pipe, &iosb, buffer, 0, FileNameInformation );
    ok_(__FILE__,line)( status == STATUS_INFO_LENGTH_MISMATCH,
            "expected STATUS_INFO_LENGTH_MISMATCH, got %#x\n", status );

    status = NtQueryInformationFile( pipe, &iosb, buffer, sizeof(buffer), FileNameInformation );
    todo_wine_if (todo)
        ok_(__FILE__,line)( status == expected_status, "expected %#x, got %#x\n", expected_status, status );
}

#define test_file_name(a) _test_file_name(__LINE__,a)
static void _test_file_name(unsigned line, HANDLE pipe)
{
    char buffer[512];
    FILE_NAME_INFORMATION *name_info = (FILE_NAME_INFORMATION*)buffer;
    IO_STATUS_BLOCK iosb;
    NTSTATUS status;

    static const WCHAR nameW[] =
        {'\\','n','t','d','l','l','_','t','e','s','t','s','_','p','i','p','e','.','c'};

    memset( buffer, 0xaa, sizeof(buffer) );
    memset( &iosb, 0xaa, sizeof(iosb) );
    status = NtQueryInformationFile( pipe, &iosb, buffer, sizeof(buffer), FileNameInformation );
    ok_(__FILE__,line)( status == STATUS_SUCCESS, "NtQueryInformationFile failed: %x\n", status );
    ok_(__FILE__,line)( iosb.Status == STATUS_SUCCESS, "Status = %x\n", iosb.Status );
    ok_(__FILE__,line)( iosb.Information == sizeof(name_info->FileNameLength) + sizeof(nameW),
        "Information = %lu\n", iosb.Information );
    ok( name_info->FileNameLength == sizeof(nameW), "FileNameLength = %u\n", name_info->FileNameLength );
    ok( !memcmp(name_info->FileName, nameW, sizeof(nameW)), "FileName = %s\n", wine_dbgstr_w(name_info->FileName) );

    /* too small buffer */
    memset( buffer, 0xaa, sizeof(buffer) );
    memset( &iosb, 0xaa, sizeof(iosb) );
    status = NtQueryInformationFile( pipe, &iosb, buffer, 20, FileNameInformation );
    ok( status == STATUS_BUFFER_OVERFLOW, "NtQueryInformationFile failed: %x\n", status );
    ok( iosb.Status == STATUS_BUFFER_OVERFLOW, "Status = %x\n", iosb.Status );
    ok( iosb.Information == 20, "Information = %lu\n", iosb.Information );
    ok( name_info->FileNameLength == sizeof(nameW), "FileNameLength = %u\n", name_info->FileNameLength );
    ok( !memcmp(name_info->FileName, nameW, 16), "FileName = %s\n", wine_dbgstr_w(name_info->FileName) );

    /* too small buffer */
    memset( buffer, 0xaa, sizeof(buffer) );
    memset( &iosb, 0xaa, sizeof(iosb) );
    status = NtQueryInformationFile( pipe, &iosb, buffer, 4, FileNameInformation );
    ok( status == STATUS_INFO_LENGTH_MISMATCH, "NtQueryInformationFile failed: %x\n", status );
}

static HANDLE create_pipe_server(void)
{
    HANDLE handle;
    NTSTATUS status;

    status = create_pipe(&handle, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0);
    ok(status == STATUS_SUCCESS, "create_pipe failed: %x\n", status);
    return handle;
}

static HANDLE connect_pipe(HANDLE server)
{
    HANDLE client;

    client = CreateFileW(testpipe, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING,
                         FILE_FLAG_OVERLAPPED, 0);
    ok(client != INVALID_HANDLE_VALUE, "can't open pipe: %u\n", GetLastError());

    return client;
}

static HANDLE connect_and_write_pipe(HANDLE server)
{
    BYTE buf[10] = {0};
    HANDLE client;
    DWORD written;
    BOOL res;

    client = connect_pipe(server);

    res = WriteFile(client, buf, sizeof(buf), &written, NULL);
    ok(res, "WriteFile failed: %u\n", GetLastError());
    res = WriteFile(server, buf, sizeof(buf), &written, NULL);
    ok(res, "WriteFile failed: %u\n", GetLastError());

    return client;
}

static void test_pipe_state(HANDLE pipe, BOOL is_server, DWORD state)
{
    FILE_PIPE_PEEK_BUFFER peek_buf;
    IO_STATUS_BLOCK io;
    static char buf[] = "test";
    NTSTATUS status, expected_status;

    memset(&peek_buf, 0xcc, sizeof(peek_buf));
    memset(&io, 0xcc, sizeof(io));
    status = NtFsControlFile(pipe, NULL, NULL, NULL, &io, FSCTL_PIPE_PEEK, NULL, 0, &peek_buf, sizeof(peek_buf));
    if (!status || status == STATUS_PENDING)
        status = io.Status;
    switch (state)
    {
    case FILE_PIPE_DISCONNECTED_STATE:
        expected_status = is_server ? STATUS_INVALID_PIPE_STATE : STATUS_PIPE_DISCONNECTED;
        break;
    case FILE_PIPE_LISTENING_STATE:
        expected_status = STATUS_INVALID_PIPE_STATE;
        break;
    case FILE_PIPE_CONNECTED_STATE:
        expected_status = STATUS_SUCCESS;
        break;
    default:
        expected_status = STATUS_PIPE_BROKEN;
        break;
    }
    ok(status == expected_status, "status = %x, expected %x in %s state %u\n",
       status, expected_status, is_server ? "server" : "client", state);
    if (!status)
        ok(peek_buf.NamedPipeState == state, "NamedPipeState = %u, expected %u\n",
           peek_buf.NamedPipeState, state);

    if (state != FILE_PIPE_CONNECTED_STATE)
    {
        if (state == FILE_PIPE_CLOSING_STATE)
            expected_status = STATUS_INVALID_PIPE_STATE;
        status = NtFsControlFile(pipe, NULL, NULL, NULL, &io, FSCTL_PIPE_TRANSCEIVE,
                                 buf, 1, buf+1, 1);
        if (!status || status == STATUS_PENDING)
            status = io.Status;
        ok(status == expected_status,
            "NtFsControlFile(FSCTL_PIPE_TRANSCEIVE) failed in %s state %u: %x\n",
            is_server ? "server" : "client", state, status);
    }

    memset(&io, 0xcc, sizeof(io));
    status = NtFlushBuffersFile(pipe, &io);
    if (!is_server && state == FILE_PIPE_DISCONNECTED_STATE)
    {
        ok(status == STATUS_PIPE_DISCONNECTED, "status = %x in %s state %u\n",
           status, is_server ? "server" : "client", state);
    }
    else
    {
        ok(status == STATUS_SUCCESS, "status = %x in %s state %u\n",
           status, is_server ? "server" : "client", state);
        ok(io.Status == status, "io.Status = %x\n", io.Status);
        ok(!io.Information, "io.Information = %lx\n", io.Information);
    }

    if (state != FILE_PIPE_CONNECTED_STATE)
    {
        switch (state)
        {
        case FILE_PIPE_DISCONNECTED_STATE:
            expected_status = STATUS_PIPE_DISCONNECTED;
            break;
        case FILE_PIPE_LISTENING_STATE:
            expected_status = STATUS_PIPE_LISTENING;
            break;
        default:
            expected_status = STATUS_PIPE_BROKEN;
            break;
        }
        status = NtReadFile(pipe, NULL, NULL, NULL, &io, buf, 1, NULL, NULL);
        ok(status == expected_status, "NtReadFile failed in %s state %u: %x\n",
            is_server ? "server" : "client", state, status);
    }

    if (is_server && (state == FILE_PIPE_CLOSING_STATE || state == FILE_PIPE_CONNECTED_STATE))
    {
        memset(&io, 0xcc, sizeof(io));
        status = listen_pipe(pipe, NULL, &io, FALSE);
        ok(status == (state == FILE_PIPE_CLOSING_STATE ? STATUS_PIPE_CLOSING : STATUS_PIPE_CONNECTED),
           "status = %x in %u state\n", status, state);
    }
}

static void test_pipe_with_data_state(HANDLE pipe, BOOL is_server, DWORD state)
{
    FILE_PIPE_LOCAL_INFORMATION local_info;
    FILE_PIPE_INFORMATION pipe_info;
    FILE_PIPE_PEEK_BUFFER peek_buf;
    IO_STATUS_BLOCK io;
    char buf[256] = "test";
    NTSTATUS status, expected_status;

    memset(&io, 0xcc, sizeof(io));
    status = pNtQueryInformationFile(pipe, &io, &local_info, sizeof(local_info), FilePipeLocalInformation);
    if (!is_server && state == FILE_PIPE_DISCONNECTED_STATE)
        ok(status == STATUS_PIPE_DISCONNECTED,
            "NtQueryInformationFile(FilePipeLocalInformation) failed in %s state %u: %x\n",
            is_server ? "server" : "client", state, status);
    else
        ok(status == STATUS_SUCCESS,
            "NtQueryInformationFile(FilePipeLocalInformation) failed in %s state %u: %x\n",
            is_server ? "server" : "client", state, status);
    if (!status)
        ok(local_info.NamedPipeState == state, "%s NamedPipeState = %u, expected %u\n",
            is_server ? "server" : "client", local_info.NamedPipeState, state);

    status = pNtQueryInformationFile(pipe, &io, &pipe_info, sizeof(pipe_info), FilePipeInformation);
    if (!is_server && state == FILE_PIPE_DISCONNECTED_STATE)
        ok(status == STATUS_PIPE_DISCONNECTED,
            "NtQueryInformationFile(FilePipeInformation) failed in %s state %u: %x\n",
            is_server ? "server" : "client", state, status);
    else
        ok(status == STATUS_SUCCESS,
            "NtQueryInformationFile(FilePipeInformation) failed in %s state %u: %x\n",
            is_server ? "server" : "client", state, status);

    status = NtQueryInformationFile(pipe, &io, buf, sizeof(buf), FileNameInformation);
    if (!is_server && state == FILE_PIPE_DISCONNECTED_STATE)
        ok(status == STATUS_PIPE_DISCONNECTED,
           "NtQueryInformationFile(FileNameInformation) failed: %x\n", status);
    else
        todo_wine_if(!is_server && state == FILE_PIPE_CLOSING_STATE)
        ok(status == STATUS_SUCCESS,
           "NtQueryInformationFile(FileNameInformation) failed: %x\n", status);

    memset(&peek_buf, 0xcc, sizeof(peek_buf));
    memset(&io, 0xcc, sizeof(io));
    status = NtFsControlFile(pipe, NULL, NULL, NULL, &io, FSCTL_PIPE_PEEK, NULL, 0, &peek_buf, sizeof(peek_buf));
    if (!status || status == STATUS_PENDING)
        status = io.Status;
    switch (state)
    {
    case FILE_PIPE_DISCONNECTED_STATE:
        expected_status = is_server ? STATUS_INVALID_PIPE_STATE : STATUS_PIPE_DISCONNECTED;
        break;
    case FILE_PIPE_LISTENING_STATE:
        expected_status = STATUS_INVALID_PIPE_STATE;
        break;
    default:
        expected_status = STATUS_BUFFER_OVERFLOW;
        break;
    }
    ok(status == expected_status, "status = %x, expected %x in %s state %u\n",
       status, expected_status, is_server ? "server" : "client", state);
    if (status == STATUS_BUFFER_OVERFLOW)
        ok(peek_buf.NamedPipeState == state, "NamedPipeState = %u, expected %u\n",
           peek_buf.NamedPipeState, state);

    switch (state)
    {
    case FILE_PIPE_DISCONNECTED_STATE:
        expected_status = STATUS_PIPE_DISCONNECTED;
        break;
    case FILE_PIPE_LISTENING_STATE:
        expected_status = STATUS_PIPE_LISTENING;
        break;
    case FILE_PIPE_CONNECTED_STATE:
        expected_status = STATUS_SUCCESS;
        break;
    default:
        expected_status = STATUS_PIPE_CLOSING;
        break;
    }
    status = NtWriteFile(pipe, NULL, NULL, NULL, &io, buf, 1, NULL, NULL);
    ok(status == expected_status, "NtWriteFile failed in %s state %u: %x\n",
        is_server ? "server" : "client", state, status);

    if (state == FILE_PIPE_CLOSING_STATE)
        expected_status = STATUS_SUCCESS;
    status = NtReadFile(pipe, NULL, NULL, NULL, &io, buf, 1, NULL, NULL);
    ok(status == expected_status, "NtReadFile failed in %s state %u: %x\n",
        is_server ? "server" : "client", state, status);
}

static void pipe_for_each_state(HANDLE (*create_server)(void),
                                HANDLE (*connect_client)(HANDLE),
                                void (*test)(HANDLE pipe, BOOL is_server, DWORD pipe_state))
{
    HANDLE client, server;
    IO_STATUS_BLOCK iosb;
    NTSTATUS status;
    HANDLE event;
    BOOL ret;

    event = CreateEventW(NULL, TRUE, FALSE, NULL);

    server = create_server();
    test(server, TRUE, FILE_PIPE_LISTENING_STATE);

    status = listen_pipe(server, event, &iosb, FALSE);
    ok(status == STATUS_PENDING, "listen_pipe returned %x\n", status);
    test(server, TRUE, FILE_PIPE_LISTENING_STATE);

    client = connect_client(server);
    test(server, TRUE, FILE_PIPE_CONNECTED_STATE);
    test(client, FALSE, FILE_PIPE_CONNECTED_STATE);

    /* server closed, but not disconnected */
    CloseHandle(server);
    test(client, FALSE, FILE_PIPE_CLOSING_STATE);
    CloseHandle(client);

    server = create_server();
    status = listen_pipe(server, event, &iosb, FALSE);
    ok(status == STATUS_PENDING, "listen_pipe returned %x\n", status);

    client = connect_client(server);
    ret = DisconnectNamedPipe(server);
    ok(ret, "DisconnectNamedPipe failed: %u\n", GetLastError());
    test(server, TRUE, FILE_PIPE_DISCONNECTED_STATE);
    test(client, FALSE, FILE_PIPE_DISCONNECTED_STATE);
    CloseHandle(server);
    test(client, FALSE, FILE_PIPE_DISCONNECTED_STATE);
    CloseHandle(client);

    server = create_server();
    status = listen_pipe(server, event, &iosb, FALSE);
    ok(status == STATUS_PENDING, "listen_pipe returned %x\n", status);

    client = connect_client(server);
    CloseHandle(client);
    test(server, TRUE, FILE_PIPE_CLOSING_STATE);
    ret = DisconnectNamedPipe(server);
    ok(ret, "DisconnectNamedPipe failed: %u\n", GetLastError());
    test(server, TRUE, FILE_PIPE_DISCONNECTED_STATE);

    status = listen_pipe(server, event, &iosb, FALSE);
    ok(status == STATUS_PENDING, "listen_pipe returned %x\n", status);
    client = connect_client(server);
    test(server, TRUE, FILE_PIPE_CONNECTED_STATE);
    test(client, FALSE, FILE_PIPE_CONNECTED_STATE);
    CloseHandle(client);
    CloseHandle(server);

    CloseHandle(event);
}

static HANDLE create_local_info_test_pipe(void)
{
    IO_STATUS_BLOCK iosb;
    OBJECT_ATTRIBUTES attr;
    UNICODE_STRING name;
    LARGE_INTEGER timeout;
    HANDLE pipe;
    NTSTATUS status;

    pRtlInitUnicodeString(&name, testpipe_nt);

    attr.Length                   = sizeof(attr);
    attr.RootDirectory            = 0;
    attr.ObjectName               = &name;
    attr.Attributes               = OBJ_CASE_INSENSITIVE;
    attr.SecurityDescriptor       = NULL;
    attr.SecurityQualityOfService = NULL;

    timeout.QuadPart = -100000000;

    status = pNtCreateNamedPipeFile(&pipe, FILE_READ_ATTRIBUTES | SYNCHRONIZE | GENERIC_WRITE,
                                    &attr, &iosb, FILE_SHARE_READ, FILE_CREATE, 0, 1, 0, 0, 1,
                                    100, 200, &timeout);
    ok(status == STATUS_SUCCESS, "NtCreateNamedPipeFile failed: %x\n", status);

    return pipe;
}

static HANDLE connect_pipe_reader(HANDLE server)
{
    HANDLE client;

    client = CreateFileW(testpipe, GENERIC_READ | FILE_WRITE_ATTRIBUTES, 0, 0, OPEN_EXISTING,
                         FILE_FLAG_OVERLAPPED, 0);
    ok(client != INVALID_HANDLE_VALUE, "can't open pipe: %u\n", GetLastError());

    return client;
}

static void test_pipe_local_info(HANDLE pipe, BOOL is_server, DWORD state)
{
    FILE_PIPE_LOCAL_INFORMATION local_info;
    FILE_PIPE_INFORMATION pipe_info;
    OBJECT_ATTRIBUTES attr;
    UNICODE_STRING name;
    LARGE_INTEGER timeout;
    HANDLE new_pipe;
    IO_STATUS_BLOCK iosb;
    NTSTATUS status;

    memset(&iosb, 0xcc, sizeof(iosb));
    memset(&local_info, 0xcc, sizeof(local_info));
    status = pNtQueryInformationFile(pipe, &iosb, &local_info, sizeof(local_info), FilePipeLocalInformation);
    if (!is_server && state == FILE_PIPE_DISCONNECTED_STATE)
        ok(status == STATUS_PIPE_DISCONNECTED,
            "NtQueryInformationFile(FilePipeLocalInformation) failed in %s state %u: %x\n",
            is_server ? "server" : "client", state, status);
    else
        ok(status == STATUS_SUCCESS,
            "NtQueryInformationFile(FilePipeLocalInformation) failed in %s state %u: %x\n",
            is_server ? "server" : "client", state, status);
    if (!status)
    {
        ok(local_info.NamedPipeType == 1, "NamedPipeType = %u\n", local_info.NamedPipeType);
        ok(local_info.NamedPipeConfiguration == 1, "NamedPipeConfiguration = %u\n",
           local_info.NamedPipeConfiguration);
        ok(local_info.MaximumInstances == 1, "MaximumInstances = %u\n", local_info.MaximumInstances);
        if (!is_server && state == FILE_PIPE_CLOSING_STATE)
            ok(local_info.CurrentInstances == 0 || broken(local_info.CurrentInstances == 1 /* winxp */),
               "CurrentInstances = %u\n", local_info.CurrentInstances);
        else
            ok(local_info.CurrentInstances == 1,
               "CurrentInstances = %u\n", local_info.CurrentInstances);
        ok(local_info.InboundQuota == 100, "InboundQuota = %u\n", local_info.InboundQuota);
        ok(local_info.ReadDataAvailable == 0, "ReadDataAvailable = %u\n",
           local_info.ReadDataAvailable);
        ok(local_info.OutboundQuota == 200, "OutboundQuota = %u\n", local_info.OutboundQuota);
        todo_wine
        ok(local_info.WriteQuotaAvailable == (is_server ? 200 : 100), "WriteQuotaAvailable = %u\n",
           local_info.WriteQuotaAvailable);
        ok(local_info.NamedPipeState == state, "%s NamedPipeState = %u, expected %u\n",
           is_server ? "server" : "client", local_info.NamedPipeState, state);
        ok(local_info.NamedPipeEnd == is_server, "NamedPipeEnd = %u\n", local_info.NamedPipeEnd);

        /* try to create another, incompatible, instance of pipe */
        pRtlInitUnicodeString(&name, testpipe_nt);

        attr.Length                   = sizeof(attr);
        attr.RootDirectory            = 0;
        attr.ObjectName               = &name;
        attr.Attributes               = OBJ_CASE_INSENSITIVE;
        attr.SecurityDescriptor       = NULL;
        attr.SecurityQualityOfService = NULL;

        timeout.QuadPart = -100000000;

        status = pNtCreateNamedPipeFile(&new_pipe, FILE_READ_ATTRIBUTES | SYNCHRONIZE | GENERIC_READ,
                                        &attr, &iosb, FILE_SHARE_WRITE, FILE_CREATE, 0, 0, 0, 0, 1,
                                        100, 200, &timeout);
        if (!local_info.CurrentInstances)
            ok(status == STATUS_SUCCESS, "NtCreateNamedPipeFile failed: %x\n", status);
        else
            ok(status == STATUS_INSTANCE_NOT_AVAILABLE, "NtCreateNamedPipeFile failed: %x\n", status);
        if (!status) CloseHandle(new_pipe);

        memset(&iosb, 0xcc, sizeof(iosb));
        status = pNtQueryInformationFile(pipe, &iosb, &local_info, sizeof(local_info),
                                         FilePipeLocalInformation);
        ok(status == STATUS_SUCCESS,
           "NtQueryInformationFile(FilePipeLocalInformation) failed in %s state %u: %x\n",
           is_server ? "server" : "client", state, status);

        if (!is_server && state == FILE_PIPE_CLOSING_STATE)
            ok(local_info.CurrentInstances == 0 || broken(local_info.CurrentInstances == 1 /* winxp */),
               "CurrentInstances = %u\n", local_info.CurrentInstances);
        else
            ok(local_info.CurrentInstances == 1,
               "CurrentInstances = %u\n", local_info.CurrentInstances);
    }

    memset(&iosb, 0xcc, sizeof(iosb));
    status = pNtQueryInformationFile(pipe, &iosb, &pipe_info, sizeof(pipe_info), FilePipeInformation);
    if (!is_server && state == FILE_PIPE_DISCONNECTED_STATE)
        ok(status == STATUS_PIPE_DISCONNECTED,
            "NtQueryInformationFile(FilePipeLocalInformation) failed in %s state %u: %x\n",
            is_server ? "server" : "client", state, status);
    else
        ok(status == STATUS_SUCCESS,
            "NtQueryInformationFile(FilePipeLocalInformation) failed in %s state %u: %x\n",
            is_server ? "server" : "client", state, status);

    if (!status)
    {
        ok(pipe_info.ReadMode == 0, "ReadMode = %u\n", pipe_info.ReadMode);
        ok(pipe_info.CompletionMode == 0, "CompletionMode = %u\n", pipe_info.CompletionMode);
    }

    pipe_info.ReadMode = 0;
    pipe_info.CompletionMode = 0;
    memset(&iosb, 0xcc, sizeof(iosb));
    status = pNtSetInformationFile(pipe, &iosb, &pipe_info, sizeof(pipe_info), FilePipeInformation);
    if (!is_server && state == FILE_PIPE_DISCONNECTED_STATE)
        ok(status == STATUS_PIPE_DISCONNECTED,
            "NtQueryInformationFile(FilePipeLocalInformation) failed in %s state %u: %x\n",
            is_server ? "server" : "client", state, status);
    else
        ok(status == STATUS_SUCCESS,
            "NtQueryInformationFile(FilePipeLocalInformation) failed in %s state %u: %x\n",
            is_server ? "server" : "client", state, status);
}

static void test_file_info(void)
{
    HANDLE server, client, device;

    if (!create_pipe_pair( &server, &client, FILE_FLAG_OVERLAPPED | PIPE_ACCESS_INBOUND,
                           PIPE_TYPE_MESSAGE, 4096 )) return;

    test_file_name( client );
    test_file_name( server );

    DisconnectNamedPipe( server );
    test_file_name_fail( client, STATUS_PIPE_DISCONNECTED, FALSE );

    CloseHandle( server );
    CloseHandle( client );

    device = CreateFileA("\\\\.\\pipe", 0, 0, NULL, OPEN_EXISTING, 0, NULL);
    ok(device != INVALID_HANDLE_VALUE, "got error %u\n", GetLastError());

    test_file_name_fail( device, STATUS_INVALID_PARAMETER, TRUE );

    CloseHandle( device );
}

static PSECURITY_DESCRIPTOR get_security_descriptor(HANDLE handle, BOOL todo)
{
    SECURITY_DESCRIPTOR *sec_desc;
    ULONG length = 0;
    NTSTATUS status;

    status = NtQuerySecurityObject(handle, GROUP_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION,
                                   NULL, 0, &length);
    todo_wine_if(todo && status == STATUS_PIPE_DISCONNECTED)
    ok(status == STATUS_BUFFER_TOO_SMALL,
       "Failed to query object security descriptor length: %08x\n", status);
    if(status != STATUS_BUFFER_TOO_SMALL) return NULL;
    ok(length != 0, "length = 0\n");

    sec_desc = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, length);
    status = NtQuerySecurityObject(handle, GROUP_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION,
                                   sec_desc, length, &length);
    ok(status == STATUS_SUCCESS, "Failed to query object security descriptor: %08x\n", status);

    return sec_desc;
}

static TOKEN_OWNER *get_current_owner(void)
{
    TOKEN_OWNER *owner;
    ULONG length = 0;
    HANDLE token;
    BOOL ret;

    ret = OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &token);
    ok(ret, "Failed to get process token: %u\n", GetLastError());

    ret = GetTokenInformation(token, TokenOwner, NULL, 0, &length);
    ok(!ret && GetLastError() == ERROR_INSUFFICIENT_BUFFER,
       "GetTokenInformation failed: %u\n", GetLastError());
    ok(length != 0, "Failed to get token owner information length: %u\n", GetLastError());

    owner = HeapAlloc(GetProcessHeap(), 0, length);
    ret = GetTokenInformation(token, TokenOwner, owner, length, &length);
    ok(ret, "Failed to get token owner information: %u)\n", GetLastError());

    CloseHandle(token);
    return owner;
}

static TOKEN_PRIMARY_GROUP *get_current_group(void)
{
    TOKEN_PRIMARY_GROUP *group;
    ULONG length = 0;
    HANDLE token;
    BOOL ret;

    ret = OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &token);
    ok(ret, "Failed to get process token: %u\n", GetLastError());

    ret = GetTokenInformation(token, TokenPrimaryGroup, NULL, 0, &length);
    ok(!ret && GetLastError() == ERROR_INSUFFICIENT_BUFFER,
       "GetTokenInformation failed: %u\n", GetLastError());
    ok(length != 0, "Failed to get primary group token information length: %u\n", GetLastError());

    group = HeapAlloc(GetProcessHeap(), 0, length);
    ret = GetTokenInformation(token, TokenPrimaryGroup, group, length, &length);
    ok(ret, "Failed to get primary group token information: %u\n", GetLastError());

    CloseHandle(token);
    return group;
}

static SID *well_known_sid(WELL_KNOWN_SID_TYPE sid_type)
{
    DWORD size = SECURITY_MAX_SID_SIZE;
    SID *sid;
    BOOL ret;

    sid = HeapAlloc(GetProcessHeap(), 0, size);
    ret = CreateWellKnownSid(sid_type, NULL, sid, &size);
    ok(ret, "CreateWellKnownSid failed: %u\n", GetLastError());
    return sid;
}

#define test_group(a,b,c) _test_group(__LINE__,a,b,c)
static void _test_group(unsigned line, HANDLE handle, SID *expected_sid, BOOL todo)
{
    SECURITY_DESCRIPTOR *sec_desc;
    BOOLEAN defaulted;
    PSID group_sid;
    NTSTATUS status;

    sec_desc = get_security_descriptor(handle, todo);
    if (!sec_desc) return;

    status = RtlGetGroupSecurityDescriptor(sec_desc, &group_sid, &defaulted);
    ok_(__FILE__,line)(status == STATUS_SUCCESS,
                       "Failed to query group from security descriptor: %08x\n", status);
    todo_wine_if(todo)
    ok_(__FILE__,line)(EqualSid(group_sid, expected_sid), "SIDs are not equal\n");

    HeapFree(GetProcessHeap(), 0, sec_desc);
}

static void test_security_info(void)
{
    char sec_desc[SECURITY_DESCRIPTOR_MIN_LENGTH];
    TOKEN_PRIMARY_GROUP *process_group;
    SECURITY_ATTRIBUTES sec_attr;
    TOKEN_OWNER *process_owner;
    HANDLE server, client, server2;
    SID *world_sid, *local_sid;
    ULONG length;
    NTSTATUS status;
    BOOL ret;

    trace("security tests...\n");

    process_owner = get_current_owner();
    process_group = get_current_group();
    world_sid = well_known_sid(WinWorldSid);
    local_sid = well_known_sid(WinLocalSid);

    ret = InitializeSecurityDescriptor(sec_desc, SECURITY_DESCRIPTOR_REVISION);
    ok(ret, "InitializeSecurityDescriptor failed\n");

    ret = SetSecurityDescriptorOwner(sec_desc, process_owner->Owner, FALSE);
    ok(ret, "SetSecurityDescriptorOwner failed\n");

    ret = SetSecurityDescriptorGroup(sec_desc, process_group->PrimaryGroup, FALSE);
    ok(ret, "SetSecurityDescriptorGroup failed\n");

    server = CreateNamedPipeA(PIPENAME, PIPE_ACCESS_DUPLEX | WRITE_OWNER, PIPE_TYPE_BYTE, 10,
                              0x20000, 0x20000, 0, NULL);
    ok(server != INVALID_HANDLE_VALUE, "CreateNamedPipe failed: %u\n", GetLastError());

    client = CreateFileA(PIPENAME, GENERIC_ALL, 0, NULL, OPEN_EXISTING, 0, NULL);
    ok(client != INVALID_HANDLE_VALUE, "CreateFile failed: %u\n", GetLastError());

    test_group(server, process_group->PrimaryGroup, TRUE);
    test_group(client, process_group->PrimaryGroup, TRUE);

    /* set server group, client changes as well */
    ret = SetSecurityDescriptorGroup(sec_desc, world_sid, FALSE);
    ok(ret, "SetSecurityDescriptorGroup failed\n");
    status = NtSetSecurityObject(server, GROUP_SECURITY_INFORMATION, sec_desc);
    ok(status == STATUS_SUCCESS, "NtSetSecurityObject failed: %08x\n", status);

    test_group(server, world_sid, FALSE);
    test_group(client, world_sid, FALSE);

    /* new instance of pipe server has the same security descriptor */
    server2 = CreateNamedPipeA(PIPENAME, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE, 10,
                               0x20000, 0x20000, 0, NULL);
    ok(server2 != INVALID_HANDLE_VALUE, "CreateNamedPipe failed: %u\n", GetLastError());
    test_group(server2, world_sid, FALSE);

    /* set client group, server changes as well */
    ret = SetSecurityDescriptorGroup(sec_desc, local_sid, FALSE);
    ok(ret, "SetSecurityDescriptorGroup failed\n");
    status = NtSetSecurityObject(server, GROUP_SECURITY_INFORMATION, sec_desc);
    ok(status == STATUS_SUCCESS, "NtSetSecurityObject failed: %08x\n", status);

    test_group(server, local_sid, FALSE);
    test_group(client, local_sid, FALSE);
    test_group(server2, local_sid, FALSE);

    CloseHandle(server);
    /* SD is preserved after closing server object */
    test_group(client, local_sid, FALSE);
    CloseHandle(client);

    server = server2;
    client = CreateFileA(PIPENAME, GENERIC_ALL, 0, NULL, OPEN_EXISTING, 0, NULL);
    ok(client != INVALID_HANDLE_VALUE, "CreateFile failed: %u\n", GetLastError());

    test_group(client, local_sid, FALSE);

    ret = DisconnectNamedPipe(server);
    ok(ret, "DisconnectNamedPipe failed: %u\n", GetLastError());

    /* disconnected server may be queried for security info, but client does not */
    test_group(server, local_sid, FALSE);
    status = NtQuerySecurityObject(client, GROUP_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION,
                                   NULL, 0, &length);
    ok(status == STATUS_PIPE_DISCONNECTED, "NtQuerySecurityObject returned %08x\n", status);
    status = NtSetSecurityObject(client, GROUP_SECURITY_INFORMATION, sec_desc);
    ok(status == STATUS_PIPE_DISCONNECTED, "NtQuerySecurityObject returned %08x\n", status);

    /* attempting to create another pipe instance with specified sd fails */
    sec_attr.nLength = sizeof(sec_attr);
    sec_attr.lpSecurityDescriptor = sec_desc;
    sec_attr.bInheritHandle = FALSE;
    ret = SetSecurityDescriptorGroup(sec_desc, local_sid, FALSE);
    ok(ret, "SetSecurityDescriptorGroup failed\n");
    server2 = CreateNamedPipeA(PIPENAME, PIPE_ACCESS_DUPLEX | WRITE_OWNER, PIPE_TYPE_BYTE, 10,
                               0x20000, 0x20000, 0, &sec_attr);
    todo_wine
    ok(server2 == INVALID_HANDLE_VALUE && GetLastError() == ERROR_ACCESS_DENIED,
       "CreateNamedPipe failed: %u\n", GetLastError());
    if (server2 != INVALID_HANDLE_VALUE) CloseHandle(server2);

    CloseHandle(server);
    CloseHandle(client);

    server = CreateNamedPipeA(PIPENAME, PIPE_ACCESS_DUPLEX | WRITE_OWNER, PIPE_TYPE_BYTE, 10,
                              0x20000, 0x20000, 0, &sec_attr);
    ok(server != INVALID_HANDLE_VALUE, "CreateNamedPipe failed: %u\n", GetLastError());
    test_group(server, local_sid, FALSE);
    CloseHandle(server);

    HeapFree(GetProcessHeap(), 0, process_owner);
    HeapFree(GetProcessHeap(), 0, process_group);
    HeapFree(GetProcessHeap(), 0, world_sid);
    HeapFree(GetProcessHeap(), 0, local_sid);
}

static void test_empty_name(void)
{
    HANDLE hdirectory, hpipe, hpipe2, hwrite, hwrite2, handle;
    OBJECT_TYPE_INFORMATION *type_info;
    OBJECT_NAME_INFORMATION *name_info;
    OBJECT_ATTRIBUTES attr;
    LARGE_INTEGER timeout;
    UNICODE_STRING name;
    IO_STATUS_BLOCK io;
    DWORD data, length;
    char buffer[1024];
    NTSTATUS status;
    BOOL ret;

    type_info = (OBJECT_TYPE_INFORMATION *)buffer;
    name_info = (OBJECT_NAME_INFORMATION *)buffer;

    hpipe = hwrite = NULL;

    attr.Length                   = sizeof(attr);
    attr.Attributes               = OBJ_CASE_INSENSITIVE;
    attr.SecurityDescriptor       = NULL;
    attr.SecurityQualityOfService = NULL;

    pRtlInitUnicodeString(&name, L"\\Device\\NamedPipe");
    attr.RootDirectory            = 0;
    attr.ObjectName               = &name;

    status = NtCreateFile(&hdirectory, GENERIC_READ | SYNCHRONIZE, &attr, &io, NULL, 0,
            FILE_SHARE_READ|FILE_SHARE_WRITE, FILE_OPEN, 0, NULL, 0 );
    ok(!status, "Got unexpected status %#x.\n", status);

    name.Buffer = NULL;
    name.Length = 0;
    name.MaximumLength = 0;
    attr.RootDirectory = hdirectory;

    timeout.QuadPart = -(LONG64)10000000;
    status = pNtCreateNamedPipeFile(&hpipe, GENERIC_READ | FILE_WRITE_ATTRIBUTES | SYNCHRONIZE, &attr,
            &io, FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_CREATE, FILE_SYNCHRONOUS_IO_NONALERT,
            0, 0, 0, 3, 4096, 4096, &timeout);
    todo_wine ok(status == STATUS_OBJECT_NAME_INVALID, "Got unexpected status %#x.\n", status);
    if (!status)
        CloseHandle(hpipe);
    CloseHandle(hdirectory);

    pRtlInitUnicodeString(&name, L"\\Device\\NamedPipe\\");
    attr.RootDirectory            = 0;
    attr.ObjectName               = &name;

    status = pNtCreateDirectoryObject(&hdirectory, GENERIC_READ | SYNCHRONIZE, &attr);
    todo_wine ok(status == STATUS_OBJECT_TYPE_MISMATCH, "Got unexpected status %#x.\n", status);

    status = NtCreateFile(&hdirectory, GENERIC_READ | SYNCHRONIZE, &attr, &io, NULL, 0,
            FILE_SHARE_READ|FILE_SHARE_WRITE, FILE_OPEN, 0, NULL, 0 );
    ok(!status, "Got unexpected status %#x.\n", status);

    name.Buffer = NULL;
    name.Length = 0;
    name.MaximumLength = 0;
    attr.RootDirectory = hdirectory;

    hpipe = NULL;
    status = pNtCreateNamedPipeFile(&hpipe, GENERIC_READ | SYNCHRONIZE, &attr,
            &io, FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_CREATE, FILE_SYNCHRONOUS_IO_NONALERT,
            0, 0, 0, 3, 4096, 4096, &timeout);
    ok(!status, "Got unexpected status %#x.\n", status);
    type_info->TypeName.Buffer = NULL;
    status = pNtQueryObject(hpipe, ObjectTypeInformation, type_info, sizeof(buffer), NULL);
    ok(!status, "Got unexpected status %#x.\n", status);
    ok(type_info->TypeName.Buffer && !wcscmp(type_info->TypeName.Buffer, L"File"),
            "Got unexpected type %s.\n", debugstr_w(type_info->TypeName.Buffer));
    status = pNtQueryObject(hpipe, ObjectNameInformation, name_info, sizeof(buffer), NULL);
    ok(status == STATUS_OBJECT_PATH_INVALID, "Got unexpected status %#x.\n", status);

    status = pNtCreateNamedPipeFile(&handle, GENERIC_READ | SYNCHRONIZE, &attr,
            &io, FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_OPEN, FILE_SYNCHRONOUS_IO_NONALERT,
            0, 0, 0, 1, 4096, 4096, &timeout);
    todo_wine ok(status == STATUS_OBJECT_NAME_NOT_FOUND, "Got unexpected status %#x.\n", status);

    status = pNtCreateNamedPipeFile(&hpipe2, GENERIC_READ | SYNCHRONIZE, &attr,
            &io, FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_CREATE, FILE_SYNCHRONOUS_IO_NONALERT,
            0, 0, 0, 3, 4096, 4096, &timeout);
    ok(!status, "Got unexpected status %#x.\n", status);

    attr.RootDirectory = hpipe;
    pRtlInitUnicodeString(&name, L"a");
    status = NtCreateFile(&hwrite, GENERIC_WRITE | FILE_WRITE_ATTRIBUTES | SYNCHRONIZE, &attr, &io, NULL, 0,
            FILE_SHARE_READ|FILE_SHARE_WRITE, FILE_OPEN, FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0 );
    ok(status == STATUS_OBJECT_NAME_INVALID, "Got unexpected status %#x.\n", status);

    name.Buffer = NULL;
    name.Length = 0;
    name.MaximumLength = 0;
    attr.RootDirectory = hpipe;
    status = NtCreateFile(&hwrite, GENERIC_WRITE | FILE_WRITE_ATTRIBUTES | SYNCHRONIZE, &attr, &io, NULL, 0,
            FILE_SHARE_READ|FILE_SHARE_WRITE, FILE_OPEN, FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0 );
    ok(!status, "Got unexpected status %#x.\n", status);

    type_info->TypeName.Buffer = NULL;
    status = pNtQueryObject(hwrite, ObjectTypeInformation, type_info, sizeof(buffer), NULL);
    ok(!status, "Got unexpected status %#x.\n", status);
    ok(type_info->TypeName.Buffer && !wcscmp(type_info->TypeName.Buffer, L"File"),
            "Got unexpected type %s.\n", debugstr_w(type_info->TypeName.Buffer));
    status = pNtQueryObject(hwrite, ObjectNameInformation, name_info, sizeof(buffer), NULL);
    ok(status == STATUS_OBJECT_PATH_INVALID, "Got unexpected status %#x.\n", status);

    attr.RootDirectory = hpipe;
    status = NtCreateFile(&handle, GENERIC_READ | SYNCHRONIZE, &attr, &io, NULL, 0,
            FILE_SHARE_READ|FILE_SHARE_WRITE, FILE_OPEN,
            FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0 );
    ok(status == STATUS_PIPE_NOT_AVAILABLE, "Got unexpected status %#x.\n", status);

    attr.RootDirectory = hpipe;
    status = NtCreateFile(&handle, GENERIC_WRITE | SYNCHRONIZE, &attr, &io, NULL, 0,
            FILE_SHARE_READ|FILE_SHARE_WRITE, FILE_OPEN, FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0 );
    ok(status == STATUS_PIPE_NOT_AVAILABLE, "Got unexpected status %#x.\n", status);

    attr.RootDirectory = hpipe2;
    status = NtCreateFile(&hwrite2, GENERIC_WRITE | FILE_WRITE_ATTRIBUTES | SYNCHRONIZE, &attr, &io, NULL, 0,
            FILE_SHARE_READ|FILE_SHARE_WRITE, FILE_OPEN, FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0 );
    ok(!status, "Got unexpected status %#x.\n", status);

    data = 0xdeadbeef;
    ret = WriteFile(hwrite, &data, sizeof(data), &length, NULL);
    ok(ret, "Got unexpected ret %#x, GetLastError() %u.\n", ret, GetLastError());
    ok(length == sizeof(data), "Got unexpected length %#x.\n", length);

    data = 0xfeedcafe;
    ret = WriteFile(hwrite2, &data, sizeof(data), &length, NULL);
    ok(ret, "Got unexpected ret %#x, GetLastError() %u.\n", ret, GetLastError());
    ok(length == sizeof(data), "Got unexpected length %#x.\n", length);

    data = 0;
    ret = ReadFile(hpipe, &data, sizeof(data), &length, NULL);
    ok(ret, "Got unexpected ret %#x, GetLastError() %u.\n", ret, GetLastError());
    ok(length == sizeof(data), "Got unexpected length %#x.\n", length);
    ok(data == 0xdeadbeef, "Got unexpected data %#x.\n", data);

    data = 0;
    ret = ReadFile(hpipe2, &data, sizeof(data), &length, NULL);
    ok(ret, "Got unexpected ret %#x, GetLastError() %u.\n", ret, GetLastError());
    ok(length == sizeof(data), "Got unexpected length %#x.\n", length);
    ok(data == 0xfeedcafe, "Got unexpected data %#x.\n", data);

    CloseHandle(hwrite);
    CloseHandle(hpipe);
    CloseHandle(hdirectory);
    CloseHandle(hpipe2);
    CloseHandle(hwrite2);
}

START_TEST(pipe)
{
    if (!init_func_ptrs())
        return;

    trace("starting invalid create tests\n");
    test_create_invalid();

    trace("starting create tests\n");
    test_create();

    trace("starting overlapped tests\n");
    test_overlapped();

    trace("starting completion tests\n");
    test_completion();

    trace("starting blocking tests\n");
    test_blocking(FILE_SYNCHRONOUS_IO_NONALERT);
    test_blocking(FILE_SYNCHRONOUS_IO_ALERT);

    trace("starting FILE_PIPE_INFORMATION tests\n");
    test_filepipeinfo();

    if (!pOpenThread || !pQueueUserAPC)
        return;

    trace("starting alertable tests\n");
    test_alertable();

    trace("starting nonalertable tests\n");
    test_nonalertable();

    trace("starting cancelio tests\n");
    test_cancelio();

    trace("starting byte read in byte mode client -> server\n");
    read_pipe_test(PIPE_ACCESS_INBOUND, PIPE_TYPE_BYTE);
    trace("starting byte read in message mode client -> server\n");
    read_pipe_test(PIPE_ACCESS_INBOUND, PIPE_TYPE_MESSAGE);
    trace("starting message read in message mode client -> server\n");
    read_pipe_test(PIPE_ACCESS_INBOUND, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE);
    trace("starting byte read in byte mode server -> client\n");
    read_pipe_test(PIPE_ACCESS_OUTBOUND, PIPE_TYPE_BYTE);
    trace("starting byte read in message mode server -> client\n");
    read_pipe_test(PIPE_ACCESS_OUTBOUND, PIPE_TYPE_MESSAGE);
    trace("starting message read in message mode server -> client\n");
    read_pipe_test(PIPE_ACCESS_OUTBOUND, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE);

    test_transceive();
    test_volume_info();
    test_file_info();
    test_security_info();
    test_empty_name();

    pipe_for_each_state(create_pipe_server, connect_pipe, test_pipe_state);
    pipe_for_each_state(create_pipe_server, connect_and_write_pipe, test_pipe_with_data_state);
    pipe_for_each_state(create_local_info_test_pipe, connect_pipe_reader, test_pipe_local_info);
}
