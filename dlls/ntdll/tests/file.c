/* Unit test suite for Ntdll file functions
 *
 * Copyright 2007 Jeff Latimer
 * Copyright 2007 Andrey Turkin
 * Copyright 2008 Jeff Zaroyko
 * Copyright 2011 Dmitry Timoshkov
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
 *
 * NOTES
 * We use function pointers here as there is no import library for NTDLL on
 * windows.
 */

#include <stdio.h>
#include <stdarg.h>

#include "ntstatus.h"
/* Define WIN32_NO_STATUS so MSVC does not give us duplicate macro
 * definition errors when we get to winnt.h
 */
#define WIN32_NO_STATUS

#include "wine/test.h"
#include "winternl.h"
#include "winuser.h"
#include "winioctl.h"
#include "winnls.h"
#include "ntifs.h"

#ifndef IO_COMPLETION_ALL_ACCESS
#define IO_COMPLETION_ALL_ACCESS 0x001F0003
#endif

static BOOL     (WINAPI * pGetVolumePathNameW)(LPCWSTR, LPWSTR, DWORD);
static UINT     (WINAPI *pGetSystemWow64DirectoryW)( LPWSTR, UINT );

static VOID     (WINAPI *pRtlFreeUnicodeString)( PUNICODE_STRING );
static VOID     (WINAPI *pRtlInitUnicodeString)( PUNICODE_STRING, LPCWSTR );
static BOOL     (WINAPI *pRtlDosPathNameToNtPathName_U)( LPCWSTR, PUNICODE_STRING, PWSTR*, CURDIR* );
static NTSTATUS (WINAPI *pRtlWow64EnableFsRedirectionEx)( ULONG, ULONG * );

static NTSTATUS (WINAPI *pNtCreateMailslotFile)( PHANDLE, ULONG, POBJECT_ATTRIBUTES, PIO_STATUS_BLOCK,
                                       ULONG, ULONG, ULONG, PLARGE_INTEGER );
static NTSTATUS (WINAPI *pNtCreateFile)(PHANDLE,ACCESS_MASK,POBJECT_ATTRIBUTES,PIO_STATUS_BLOCK,PLARGE_INTEGER,ULONG,ULONG,ULONG,ULONG,PVOID,ULONG);
static NTSTATUS (WINAPI *pNtOpenFile)(PHANDLE,ACCESS_MASK,POBJECT_ATTRIBUTES,PIO_STATUS_BLOCK,ULONG,ULONG);
static NTSTATUS (WINAPI *pNtDeleteFile)(POBJECT_ATTRIBUTES ObjectAttributes);
static NTSTATUS (WINAPI *pNtReadFile)(HANDLE hFile, HANDLE hEvent,
                                      PIO_APC_ROUTINE apc, void* apc_user,
                                      PIO_STATUS_BLOCK io_status, void* buffer, ULONG length,
                                      PLARGE_INTEGER offset, PULONG key);
static NTSTATUS (WINAPI *pNtWriteFile)(HANDLE hFile, HANDLE hEvent,
                                       PIO_APC_ROUTINE apc, void* apc_user,
                                       PIO_STATUS_BLOCK io_status,
                                       const void* buffer, ULONG length,
                                       PLARGE_INTEGER offset, PULONG key);
static NTSTATUS (WINAPI *pNtCancelIoFile)(HANDLE hFile, PIO_STATUS_BLOCK io_status);
static NTSTATUS (WINAPI *pNtCancelIoFileEx)(HANDLE hFile, PIO_STATUS_BLOCK iosb, PIO_STATUS_BLOCK io_status);
static NTSTATUS (WINAPI *pNtClose)( PHANDLE );
static NTSTATUS (WINAPI *pNtFsControlFile) (HANDLE handle, HANDLE event, PIO_APC_ROUTINE apc, PVOID apc_context, PIO_STATUS_BLOCK io, ULONG code, PVOID in_buffer, ULONG in_size, PVOID out_buffer, ULONG out_size);

static NTSTATUS (WINAPI *pNtCreateIoCompletion)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, ULONG);
static NTSTATUS (WINAPI *pNtOpenIoCompletion)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES);
static NTSTATUS (WINAPI *pNtQueryIoCompletion)(HANDLE, IO_COMPLETION_INFORMATION_CLASS, PVOID, ULONG, PULONG);
static NTSTATUS (WINAPI *pNtRemoveIoCompletion)(HANDLE, PULONG_PTR, PULONG_PTR, PIO_STATUS_BLOCK, PLARGE_INTEGER);
static NTSTATUS (WINAPI *pNtRemoveIoCompletionEx)(HANDLE,FILE_IO_COMPLETION_INFORMATION*,ULONG,ULONG*,LARGE_INTEGER*,BOOLEAN);
static NTSTATUS (WINAPI *pNtSetIoCompletion)(HANDLE, ULONG_PTR, ULONG_PTR, NTSTATUS, SIZE_T);
static NTSTATUS (WINAPI *pNtSetInformationFile)(HANDLE, PIO_STATUS_BLOCK, PVOID, ULONG, FILE_INFORMATION_CLASS);
static NTSTATUS (WINAPI *pNtQueryAttributesFile)(const OBJECT_ATTRIBUTES*,FILE_BASIC_INFORMATION*);
static NTSTATUS (WINAPI *pNtQueryInformationFile)(HANDLE, PIO_STATUS_BLOCK, PVOID, ULONG, FILE_INFORMATION_CLASS);
static NTSTATUS (WINAPI *pNtQueryDirectoryFile)(HANDLE,HANDLE,PIO_APC_ROUTINE,PVOID,PIO_STATUS_BLOCK,
                                                PVOID,ULONG,FILE_INFORMATION_CLASS,BOOLEAN,PUNICODE_STRING,BOOLEAN);
static NTSTATUS (WINAPI *pNtQueryVolumeInformationFile)(HANDLE,PIO_STATUS_BLOCK,PVOID,ULONG,FS_INFORMATION_CLASS);
static NTSTATUS (WINAPI *pNtQueryFullAttributesFile)(const OBJECT_ATTRIBUTES*, FILE_NETWORK_OPEN_INFORMATION*);
static NTSTATUS (WINAPI *pNtFlushBuffersFile)(HANDLE, IO_STATUS_BLOCK*);
static NTSTATUS (WINAPI *pNtQueryEaFile)(HANDLE,PIO_STATUS_BLOCK,PVOID,ULONG,BOOLEAN,PVOID,ULONG,PULONG,BOOLEAN);

static WCHAR fooW[] = {'f','o','o',0};

static inline BOOL is_signaled( HANDLE obj )
{
    return WaitForSingleObject( obj, 0 ) == WAIT_OBJECT_0;
}

#define TEST_BUF_LEN 3

static HANDLE create_temp_file( ULONG flags )
{
    char path[MAX_PATH], buffer[MAX_PATH];
    HANDLE handle;

    GetTempPathA( MAX_PATH, path );
    GetTempFileNameA( path, "foo", 0, buffer );
    handle = CreateFileA(buffer, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                         flags | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ok( handle != INVALID_HANDLE_VALUE, "failed to create temp file\n" );
    return (handle == INVALID_HANDLE_VALUE) ? 0 : handle;
}

#define CVALUE_FIRST 0xfffabbcc
#define CKEY_FIRST 0x1030341
#define CKEY_SECOND 0x132E46

static ULONG get_pending_msgs(HANDLE h)
{
    NTSTATUS res;
    ULONG a, req;

    res = pNtQueryIoCompletion( h, IoCompletionBasicInformation, &a, sizeof(a), &req );
    ok( res == STATUS_SUCCESS, "NtQueryIoCompletion failed: %x\n", res );
    if (res != STATUS_SUCCESS) return -1;
    ok( req == sizeof(a), "Unexpected response size: %x\n", req );
    return a;
}

static void WINAPI apc( void *arg, IO_STATUS_BLOCK *iosb, ULONG reserved )
{
    int *count = arg;

    trace( "apc called block %p iosb.status %x iosb.info %lu\n",
           iosb, U(*iosb).Status, iosb->Information );
    (*count)++;
    ok( !reserved, "reserved is not 0: %x\n", reserved );
}

static void create_file_test(void)
{
    static const WCHAR notepadW[] = {'n','o','t','e','p','a','d','.','e','x','e',0};
    static const WCHAR systemrootW[] = {'\\','S','y','s','t','e','m','R','o','o','t',
                                        '\\','f','a','i','l','i','n','g',0};
    static const WCHAR systemrootExplorerW[] = {'\\','S','y','s','t','e','m','R','o','o','t',
                                               '\\','e','x','p','l','o','r','e','r','.','e','x','e',0};
    static const WCHAR questionmarkInvalidNameW[] = {'a','f','i','l','e','?',0};
    static const WCHAR pipeInvalidNameW[]  = {'a','|','b',0};
    static const WCHAR pathInvalidNtW[] = {'\\','\\','?','\\',0};
    static const WCHAR pathInvalidNt2W[] = {'\\','?','?','\\',0};
    static const WCHAR pathInvalidDosW[] = {'\\','D','o','s','D','e','v','i','c','e','s','\\',0};
    static const char testdata[] = "Hello World";
    static const WCHAR sepW[] = {'\\',0};
    FILE_NETWORK_OPEN_INFORMATION info;
    NTSTATUS status;
    HANDLE dir, file;
    WCHAR path[MAX_PATH], temp[MAX_PATH];
    OBJECT_ATTRIBUTES attr;
    IO_STATUS_BLOCK io;
    UNICODE_STRING nameW;
    LARGE_INTEGER offset;
    char buf[32];
    DWORD ret;

    GetCurrentDirectoryW( MAX_PATH, path );
    pRtlDosPathNameToNtPathName_U( path, &nameW, NULL, NULL );
    attr.Length = sizeof(attr);
    attr.RootDirectory = 0;
    attr.ObjectName = &nameW;
    attr.Attributes = OBJ_CASE_INSENSITIVE;
    attr.SecurityDescriptor = NULL;
    attr.SecurityQualityOfService = NULL;

    /* try various open modes and options on directories */
    status = pNtCreateFile( &dir, GENERIC_READ|GENERIC_WRITE, &attr, &io, NULL, 0,
                            FILE_SHARE_READ|FILE_SHARE_WRITE, FILE_OPEN, FILE_DIRECTORY_FILE, NULL, 0 );
    ok( !status, "open %s failed %x\n", wine_dbgstr_w(nameW.Buffer), status );

    U(io).Status = 0xdeadbeef;
    offset.QuadPart = 0;
    status = pNtReadFile( dir, NULL, NULL, NULL, &io, buf, sizeof(buf), &offset, NULL );
    ok( status == STATUS_INVALID_DEVICE_REQUEST || status == STATUS_PENDING, "NtReadFile error %08x\n", status );
    if (status == STATUS_PENDING)
    {
        ret = WaitForSingleObject( dir, 1000 );
        ok( ret == WAIT_OBJECT_0, "WaitForSingleObject error %u\n", ret );
        ok( U(io).Status == STATUS_INVALID_DEVICE_REQUEST,
            "expected STATUS_INVALID_DEVICE_REQUEST, got %08x\n", U(io).Status );
    }

    U(io).Status = 0xdeadbeef;
    offset.QuadPart = 0;
    status = pNtWriteFile( dir, NULL, NULL, NULL, &io, testdata, sizeof(testdata), &offset, NULL);
    todo_wine
    ok( status == STATUS_INVALID_DEVICE_REQUEST || status == STATUS_PENDING, "NtWriteFile error %08x\n", status );
    if (status == STATUS_PENDING)
    {
        ret = WaitForSingleObject( dir, 1000 );
        ok( ret == WAIT_OBJECT_0, "WaitForSingleObject error %u\n", ret );
        ok( U(io).Status == STATUS_INVALID_DEVICE_REQUEST,
            "expected STATUS_INVALID_DEVICE_REQUEST, got %08x\n", U(io).Status );
    }

    CloseHandle( dir );

    status = pNtCreateFile( &dir, GENERIC_READ, &attr, &io, NULL, 0, FILE_SHARE_READ|FILE_SHARE_WRITE,
                            FILE_CREATE, FILE_DIRECTORY_FILE, NULL, 0 );
    ok( status == STATUS_OBJECT_NAME_COLLISION || status == STATUS_ACCESS_DENIED,
        "open %s failed %x\n", wine_dbgstr_w(nameW.Buffer), status );

    status = pNtCreateFile( &dir, GENERIC_READ, &attr, &io, NULL, 0, FILE_SHARE_READ|FILE_SHARE_WRITE,
                            FILE_OPEN_IF, FILE_DIRECTORY_FILE, NULL, 0 );
    ok( !status, "open %s failed %x\n", wine_dbgstr_w(nameW.Buffer), status );
    CloseHandle( dir );

    status = pNtCreateFile( &dir, GENERIC_READ, &attr, &io, NULL, 0, FILE_SHARE_READ|FILE_SHARE_WRITE,
                            FILE_SUPERSEDE, FILE_DIRECTORY_FILE, NULL, 0 );
    ok( status == STATUS_INVALID_PARAMETER, "open %s failed %x\n", wine_dbgstr_w(nameW.Buffer), status );

    status = pNtCreateFile( &dir, GENERIC_READ, &attr, &io, NULL, 0, FILE_SHARE_READ|FILE_SHARE_WRITE,
                            FILE_OVERWRITE, FILE_DIRECTORY_FILE, NULL, 0 );
    ok( status == STATUS_INVALID_PARAMETER, "open %s failed %x\n", wine_dbgstr_w(nameW.Buffer), status );

    status = pNtCreateFile( &dir, GENERIC_READ, &attr, &io, NULL, 0, FILE_SHARE_READ|FILE_SHARE_WRITE,
                            FILE_OVERWRITE_IF, FILE_DIRECTORY_FILE, NULL, 0 );
    ok( status == STATUS_INVALID_PARAMETER, "open %s failed %x\n", wine_dbgstr_w(nameW.Buffer), status );

    status = pNtCreateFile( &dir, GENERIC_READ, &attr, &io, NULL, 0, FILE_SHARE_READ|FILE_SHARE_WRITE,
                            FILE_OPEN, 0, NULL, 0 );
    ok( !status, "open %s failed %x\n", wine_dbgstr_w(nameW.Buffer), status );
    CloseHandle( dir );

    status = pNtCreateFile( &dir, GENERIC_READ, &attr, &io, NULL, 0, FILE_SHARE_READ|FILE_SHARE_WRITE,
                            FILE_CREATE, 0, NULL, 0 );
    ok( status == STATUS_OBJECT_NAME_COLLISION || status == STATUS_ACCESS_DENIED,
        "open %s failed %x\n", wine_dbgstr_w(nameW.Buffer), status );

    status = pNtCreateFile( &dir, GENERIC_READ, &attr, &io, NULL, 0, FILE_SHARE_READ|FILE_SHARE_WRITE,
                            FILE_OPEN_IF, 0, NULL, 0 );
    ok( !status, "open %s failed %x\n", wine_dbgstr_w(nameW.Buffer), status );
    CloseHandle( dir );

    status = pNtCreateFile( &dir, GENERIC_READ, &attr, &io, NULL, 0, FILE_SHARE_READ|FILE_SHARE_WRITE,
                            FILE_SUPERSEDE, 0, NULL, 0 );
    ok( status == STATUS_OBJECT_NAME_COLLISION || status == STATUS_ACCESS_DENIED,
        "open %s failed %x\n", wine_dbgstr_w(nameW.Buffer), status );

    status = pNtCreateFile( &dir, GENERIC_READ, &attr, &io, NULL, 0, FILE_SHARE_READ|FILE_SHARE_WRITE,
                            FILE_OVERWRITE, 0, NULL, 0 );
    ok( status == STATUS_OBJECT_NAME_COLLISION || status == STATUS_ACCESS_DENIED,
        "open %s failed %x\n", wine_dbgstr_w(nameW.Buffer), status );

    status = pNtCreateFile( &dir, GENERIC_READ, &attr, &io, NULL, 0, FILE_SHARE_READ|FILE_SHARE_WRITE,
                            FILE_OVERWRITE_IF, 0, NULL, 0 );
    ok( status == STATUS_OBJECT_NAME_COLLISION || status == STATUS_ACCESS_DENIED,
        "open %s failed %x\n", wine_dbgstr_w(nameW.Buffer), status );

    pRtlFreeUnicodeString( &nameW );

    pRtlInitUnicodeString( &nameW, systemrootW );
    attr.Length = sizeof(attr);
    attr.RootDirectory = NULL;
    attr.ObjectName = &nameW;
    attr.Attributes = OBJ_CASE_INSENSITIVE;
    attr.SecurityDescriptor = NULL;
    attr.SecurityQualityOfService = NULL;
    dir = NULL;
    status = pNtCreateFile( &dir, FILE_APPEND_DATA, &attr, &io, NULL, FILE_ATTRIBUTE_NORMAL, 0,
                            FILE_OPEN_IF, FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0 );
    todo_wine
    ok( status == STATUS_INVALID_PARAMETER,
        "open %s failed %x\n", wine_dbgstr_w(nameW.Buffer), status );

    /* Invalid chars in file/dirnames */
    pRtlDosPathNameToNtPathName_U(questionmarkInvalidNameW, &nameW, NULL, NULL);
    attr.ObjectName = &nameW;
    status = pNtCreateFile(&dir, GENERIC_READ|SYNCHRONIZE, &attr, &io, NULL, 0,
                           FILE_SHARE_READ, FILE_CREATE,
                           FILE_DIRECTORY_FILE|FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);
    ok(status == STATUS_OBJECT_NAME_INVALID,
       "open %s failed %x\n", wine_dbgstr_w(nameW.Buffer), status);

    status = pNtCreateFile(&file, GENERIC_WRITE|SYNCHRONIZE, &attr, &io, NULL, 0,
                           0, FILE_CREATE,
                           FILE_NON_DIRECTORY_FILE|FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);
    ok(status == STATUS_OBJECT_NAME_INVALID,
       "open %s failed %x\n", wine_dbgstr_w(nameW.Buffer), status);
    pRtlFreeUnicodeString(&nameW);

    pRtlDosPathNameToNtPathName_U(pipeInvalidNameW, &nameW, NULL, NULL);
    attr.ObjectName = &nameW;
    status = pNtCreateFile(&dir, GENERIC_READ|SYNCHRONIZE, &attr, &io, NULL, 0,
                           FILE_SHARE_READ, FILE_CREATE,
                           FILE_DIRECTORY_FILE|FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);
    ok(status == STATUS_OBJECT_NAME_INVALID,
       "open %s failed %x\n", wine_dbgstr_w(nameW.Buffer), status);

    status = pNtCreateFile(&file, GENERIC_WRITE|SYNCHRONIZE, &attr, &io, NULL, 0,
                           0, FILE_CREATE,
                           FILE_NON_DIRECTORY_FILE|FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);
    ok(status == STATUS_OBJECT_NAME_INVALID,
       "open %s failed %x\n", wine_dbgstr_w(nameW.Buffer), status);
    pRtlFreeUnicodeString(&nameW);

    pRtlInitUnicodeString( &nameW, pathInvalidNtW );
    status = pNtCreateFile( &dir, GENERIC_READ|SYNCHRONIZE, &attr, &io, NULL, 0,
                            FILE_SHARE_READ, FILE_CREATE,
                            FILE_DIRECTORY_FILE|FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0 );
    ok( status == STATUS_OBJECT_NAME_INVALID,
        "open %s failed %x\n", wine_dbgstr_w(nameW.Buffer), status );

    status = pNtQueryFullAttributesFile( &attr, &info );
    todo_wine ok( status == STATUS_OBJECT_NAME_INVALID,
                  "query %s failed %x\n", wine_dbgstr_w(nameW.Buffer), status );

    pRtlInitUnicodeString( &nameW, pathInvalidNt2W );
    status = pNtCreateFile( &dir, GENERIC_READ|SYNCHRONIZE, &attr, &io, NULL, 0,
                            FILE_SHARE_READ, FILE_CREATE,
                            FILE_DIRECTORY_FILE|FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0 );
    ok( status == STATUS_OBJECT_NAME_INVALID,
        "open %s failed %x\n", wine_dbgstr_w(nameW.Buffer), status );

    status = pNtQueryFullAttributesFile( &attr, &info );
    ok( status == STATUS_OBJECT_NAME_INVALID,
        "query %s failed %x\n", wine_dbgstr_w(nameW.Buffer), status );

    pRtlInitUnicodeString( &nameW, pathInvalidDosW );
    status = pNtCreateFile( &dir, GENERIC_READ|SYNCHRONIZE, &attr, &io, NULL, 0,
                            FILE_SHARE_READ, FILE_CREATE,
                            FILE_DIRECTORY_FILE|FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0 );
    ok( status == STATUS_OBJECT_NAME_INVALID,
        "open %s failed %x\n", wine_dbgstr_w(nameW.Buffer), status );

    status = pNtQueryFullAttributesFile( &attr, &info );
    ok( status == STATUS_OBJECT_NAME_INVALID,
        "query %s failed %x\n", wine_dbgstr_w(nameW.Buffer), status );

    GetWindowsDirectoryW( path, MAX_PATH );
    path[2] = 0;
    ok( QueryDosDeviceW( path, temp, MAX_PATH ),
        "QueryDosDeviceW failed with error %u\n", GetLastError() );
    lstrcatW( temp, sepW );
    lstrcatW( temp, path+3 );
    lstrcatW( temp, sepW );
    lstrcatW( temp, notepadW );

    pRtlInitUnicodeString( &nameW, temp );
    status = pNtQueryFullAttributesFile( &attr, &info );
    ok( status == STATUS_SUCCESS,
        "query %s failed %x\n", wine_dbgstr_w(nameW.Buffer), status );

    pRtlInitUnicodeString( &nameW, systemrootExplorerW );
    status = pNtQueryFullAttributesFile( &attr, &info );
    ok( status == STATUS_SUCCESS,
        "query %s failed %x\n", wine_dbgstr_w(nameW.Buffer), status );
}

static void open_file_test(void)
{
    static const WCHAR testdirW[] = {'o','p','e','n','f','i','l','e','t','e','s','t',0};
    static const char testdata[] = "Hello World";
    NTSTATUS status;
    HANDLE dir, root, handle, file;
    WCHAR path[MAX_PATH], tmpfile[MAX_PATH];
    BYTE data[1024];
    OBJECT_ATTRIBUTES attr;
    IO_STATUS_BLOCK io;
    UNICODE_STRING nameW;
    UINT i, len;
    BOOL ret, restart = TRUE;
    DWORD numbytes;

    len = GetWindowsDirectoryW( path, MAX_PATH );
    pRtlDosPathNameToNtPathName_U( path, &nameW, NULL, NULL );
    attr.Length = sizeof(attr);
    attr.RootDirectory = 0;
    attr.ObjectName = &nameW;
    attr.Attributes = 0;
    attr.SecurityDescriptor = NULL;
    attr.SecurityQualityOfService = NULL;
    status = pNtOpenFile( &dir, SYNCHRONIZE|FILE_LIST_DIRECTORY, &attr, &io,
                          FILE_SHARE_READ|FILE_SHARE_WRITE, FILE_DIRECTORY_FILE|FILE_SYNCHRONOUS_IO_NONALERT );
    ok( !status, "open %s failed %x\n", wine_dbgstr_w(nameW.Buffer), status );
    pRtlFreeUnicodeString( &nameW );

    path[3] = 0;  /* root of the drive */
    pRtlDosPathNameToNtPathName_U( path, &nameW, NULL, NULL );
    status = pNtOpenFile( &root, GENERIC_READ, &attr, &io,
                          FILE_SHARE_READ|FILE_SHARE_WRITE, FILE_DIRECTORY_FILE );
    ok( !status, "open %s failed %x\n", wine_dbgstr_w(nameW.Buffer), status );
    pRtlFreeUnicodeString( &nameW );

    /* test opening system dir with RootDirectory set to windows dir */
    GetSystemDirectoryW( path, MAX_PATH );
    while (path[len] == '\\') len++;
    nameW.Buffer = path + len;
    nameW.Length = lstrlenW(path + len) * sizeof(WCHAR);
    attr.RootDirectory = dir;
    status = pNtOpenFile( &handle, GENERIC_READ, &attr, &io,
                          FILE_SHARE_READ|FILE_SHARE_WRITE, FILE_DIRECTORY_FILE );
    ok( !status, "open %s failed %x\n", wine_dbgstr_w(nameW.Buffer), status );
    CloseHandle( handle );

    /* try uppercase name */
    for (i = len; path[i]; i++) if (path[i] >= 'a' && path[i] <= 'z') path[i] -= 'a' - 'A';
    status = pNtOpenFile( &handle, GENERIC_READ, &attr, &io,
                          FILE_SHARE_READ|FILE_SHARE_WRITE, FILE_DIRECTORY_FILE );
    ok( !status, "open %s failed %x\n", wine_dbgstr_w(nameW.Buffer), status );
    CloseHandle( handle );

    /* try with leading backslash */
    nameW.Buffer--;
    nameW.Length += sizeof(WCHAR);
    status = pNtOpenFile( &handle, GENERIC_READ, &attr, &io,
                          FILE_SHARE_READ|FILE_SHARE_WRITE, FILE_DIRECTORY_FILE );
    ok( status == STATUS_INVALID_PARAMETER ||
        status == STATUS_OBJECT_NAME_INVALID ||
        status == STATUS_OBJECT_PATH_SYNTAX_BAD,
        "open %s failed %x\n", wine_dbgstr_w(nameW.Buffer), status );
    if (!status) CloseHandle( handle );

    /* try with empty name */
    nameW.Length = 0;
    status = pNtOpenFile( &handle, GENERIC_READ, &attr, &io,
                          FILE_SHARE_READ|FILE_SHARE_WRITE, FILE_DIRECTORY_FILE );
    ok( !status, "open %s failed %x\n", wine_dbgstr_w(nameW.Buffer), status );
    CloseHandle( handle );
    CloseHandle( dir );

    GetTempPathW( MAX_PATH, path );
    lstrcatW( path, testdirW );
    CreateDirectoryW( path, NULL );

    pRtlDosPathNameToNtPathName_U( path, &nameW, NULL, NULL );
    attr.RootDirectory = NULL;
    status = pNtOpenFile( &dir, SYNCHRONIZE|FILE_LIST_DIRECTORY, &attr, &io,
                          FILE_SHARE_READ|FILE_SHARE_WRITE, FILE_DIRECTORY_FILE|FILE_SYNCHRONOUS_IO_NONALERT );
    ok( !status, "open %s failed %x\n", wine_dbgstr_w(nameW.Buffer), status );
    pRtlFreeUnicodeString( &nameW );

    GetTempFileNameW( path, fooW, 0, tmpfile );
    file = CreateFileW( tmpfile, FILE_WRITE_DATA, 0, NULL, CREATE_ALWAYS, 0, 0 );
    ok( file != INVALID_HANDLE_VALUE, "CreateFile error %d\n", GetLastError() );
    numbytes = 0xdeadbeef;
    ret = WriteFile( file, testdata, sizeof(testdata) - 1, &numbytes, NULL );
    ok( ret, "WriteFile failed with error %u\n", GetLastError() );
    ok( numbytes == sizeof(testdata) - 1, "failed to write all data\n" );
    CloseHandle( file );

    /* try open by file id */

    while (!pNtQueryDirectoryFile( dir, NULL, NULL, NULL, &io, data, sizeof(data),
                                   FileIdBothDirectoryInformation, TRUE, NULL, restart ))
    {
        FILE_ID_BOTH_DIRECTORY_INFORMATION *info = (FILE_ID_BOTH_DIRECTORY_INFORMATION *)data;

        restart = FALSE;

        if (!info->FileId.QuadPart) continue;

        nameW.Buffer = (WCHAR *)&info->FileId;
        nameW.Length = sizeof(info->FileId);
        info->FileName[info->FileNameLength/sizeof(WCHAR)] = 0;
        attr.RootDirectory = dir;
        /* We skip 'open' files by not specifying FILE_SHARE_WRITE */
        status = pNtOpenFile( &handle, GENERIC_READ, &attr, &io,
                              FILE_SHARE_READ,
                              FILE_OPEN_BY_FILE_ID |
                              ((info->FileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? FILE_DIRECTORY_FILE : 0) );
        ok( status == STATUS_SUCCESS, "open %s failed %x\n", wine_dbgstr_w(info->FileName), status );
        if (!status)
        {
            BYTE buf[sizeof(FILE_ALL_INFORMATION) + MAX_PATH * sizeof(WCHAR)];

            if (!pNtQueryInformationFile( handle, &io, buf, sizeof(buf), FileAllInformation ))
            {
                FILE_ALL_INFORMATION *fai = (FILE_ALL_INFORMATION *)buf;

                /* check that it's the same file/directory */

                /* don't check the size for directories */
                if (!(info->FileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                    ok( info->EndOfFile.QuadPart == fai->StandardInformation.EndOfFile.QuadPart,
                        "mismatched file size for %s\n", wine_dbgstr_w(info->FileName));

                ok( info->CreationTime.QuadPart == fai->BasicInformation.CreationTime.QuadPart,
                    "mismatched creation time for %s\n", wine_dbgstr_w(info->FileName));
            }
            CloseHandle( handle );

            /* try same thing from drive root */
            attr.RootDirectory = root;
            status = pNtOpenFile( &handle, GENERIC_READ, &attr, &io,
                                  FILE_SHARE_READ|FILE_SHARE_WRITE,
                                  FILE_OPEN_BY_FILE_ID |
                                  ((info->FileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? FILE_DIRECTORY_FILE : 0) );
            ok( status == STATUS_SUCCESS || status == STATUS_NOT_IMPLEMENTED,
                "open %s failed %x\n", wine_dbgstr_w(info->FileName), status );
            if (!status) CloseHandle( handle );
        }
    }

    CloseHandle( dir );
    CloseHandle( root );

    pRtlDosPathNameToNtPathName_U( tmpfile, &nameW, NULL, NULL );
    attr.Length = sizeof(attr);
    attr.RootDirectory = 0;
    attr.ObjectName = &nameW;
    attr.Attributes = OBJ_CASE_INSENSITIVE;
    attr.SecurityDescriptor = NULL;
    attr.SecurityQualityOfService = NULL;
    status = pNtOpenFile( &file, SYNCHRONIZE|FILE_LIST_DIRECTORY, &attr, &io,
                         FILE_SHARE_READ, FILE_SYNCHRONOUS_IO_NONALERT );
    ok( !status, "open %s failed %x\n", wine_dbgstr_w(nameW.Buffer), status );
    pRtlFreeUnicodeString( &nameW );

    numbytes = 0xdeadbeef;
    memset( data, 0, sizeof(data) );
    ret = ReadFile( file, data, sizeof(data), &numbytes, NULL );
    ok( ret, "ReadFile failed with error %u\n", GetLastError() );
    ok( numbytes == sizeof(testdata) - 1, "failed to read all data\n" );
    ok( !memcmp( data, testdata, sizeof(testdata) - 1 ), "testdata doesn't match\n" );

    nameW.Length = sizeof(fooW) - sizeof(WCHAR);
    nameW.Buffer = fooW;
    attr.RootDirectory = file;
    attr.ObjectName = &nameW;
    status = pNtOpenFile( &root, SYNCHRONIZE|FILE_LIST_DIRECTORY, &attr, &io,
                         FILE_SHARE_READ, FILE_SYNCHRONOUS_IO_NONALERT );
    ok( status == STATUS_OBJECT_PATH_NOT_FOUND,
        "expected STATUS_OBJECT_PATH_NOT_FOUND, got %08x\n", status );

    nameW.Length = 0;
    nameW.Buffer = NULL;
    attr.RootDirectory = file;
    attr.ObjectName = &nameW;
    status = pNtOpenFile( &root, SYNCHRONIZE|FILE_LIST_DIRECTORY, &attr, &io,
                          FILE_SHARE_READ, FILE_SYNCHRONOUS_IO_NONALERT );
    ok( !status, "open %s failed %x\n", wine_dbgstr_w(tmpfile), status );

    numbytes = SetFilePointer( file, 0, 0, FILE_CURRENT );
    ok( numbytes == sizeof(testdata) - 1, "SetFilePointer returned %u\n", numbytes );
    numbytes = SetFilePointer( root, 0, 0, FILE_CURRENT );
    ok( numbytes == 0, "SetFilePointer returned %u\n", numbytes );

    numbytes = 0xdeadbeef;
    memset( data, 0, sizeof(data) );
    ret = ReadFile( root, data, sizeof(data), &numbytes, NULL );
    ok( ret, "ReadFile failed with error %u\n", GetLastError() );
    ok( numbytes == sizeof(testdata) - 1, "failed to read all data\n" );
    ok( !memcmp( data, testdata, sizeof(testdata) - 1 ), "testdata doesn't match\n" );

    numbytes = SetFilePointer( file, 0, 0, FILE_CURRENT );
    ok( numbytes == sizeof(testdata) - 1, "SetFilePointer returned %u\n", numbytes );
    numbytes = SetFilePointer( root, 0, 0, FILE_CURRENT );
    ok( numbytes == sizeof(testdata) - 1, "SetFilePointer returned %u\n", numbytes );

    CloseHandle( file );
    CloseHandle( root );
    DeleteFileW( tmpfile );
    RemoveDirectoryW( path );
}

static void delete_file_test(void)
{
    NTSTATUS ret;
    OBJECT_ATTRIBUTES attr;
    UNICODE_STRING nameW;
    WCHAR pathW[MAX_PATH];
    WCHAR pathsubW[MAX_PATH];
    static const WCHAR testdirW[] = {'n','t','d','e','l','e','t','e','f','i','l','e',0};
    static const WCHAR subdirW[]  = {'\\','s','u','b',0};

    ret = GetTempPathW(MAX_PATH, pathW);
    if (!ret)
    {
	ok(0, "couldn't get temp dir\n");
	return;
    }
    if (ret + ARRAY_SIZE(testdirW)-1 + ARRAY_SIZE(subdirW)-1 >= MAX_PATH)
    {
	ok(0, "MAX_PATH exceeded in constructing paths\n");
	return;
    }

    lstrcatW(pathW, testdirW);
    lstrcpyW(pathsubW, pathW);
    lstrcatW(pathsubW, subdirW);

    ret = CreateDirectoryW(pathW, NULL);
    ok(ret == TRUE, "couldn't create directory ntdeletefile\n");
    if (!pRtlDosPathNameToNtPathName_U(pathW, &nameW, NULL, NULL))
    {
	ok(0,"RtlDosPathNametoNtPathName_U failed\n");
	return;
    }

    attr.Length = sizeof(attr);
    attr.RootDirectory = 0;
    attr.Attributes = OBJ_CASE_INSENSITIVE;
    attr.ObjectName = &nameW;
    attr.SecurityDescriptor = NULL;
    attr.SecurityQualityOfService = NULL;

    /* test NtDeleteFile on an empty directory */
    ret = pNtDeleteFile(&attr);
    ok(ret == STATUS_SUCCESS, "NtDeleteFile should succeed in removing an empty directory\n");
    ret = RemoveDirectoryW(pathW);
    ok(ret == FALSE, "expected to fail removing directory, NtDeleteFile should have removed it\n");

    /* test NtDeleteFile on a non-empty directory */
    ret = CreateDirectoryW(pathW, NULL);
    ok(ret == TRUE, "couldn't create directory ntdeletefile ?!\n");
    ret = CreateDirectoryW(pathsubW, NULL);
    ok(ret == TRUE, "couldn't create directory subdir\n");
    ret = pNtDeleteFile(&attr);
    ok(ret == STATUS_SUCCESS, "expected NtDeleteFile to ret STATUS_SUCCESS\n");
    ret = RemoveDirectoryW(pathsubW);
    ok(ret == TRUE, "expected to remove directory ntdeletefile\\sub\n");
    ret = RemoveDirectoryW(pathW);
    ok(ret == TRUE, "expected to remove directory ntdeletefile, NtDeleteFile failed.\n");

    pRtlFreeUnicodeString( &nameW );
}

#define TEST_OVERLAPPED_READ_SIZE 4096

static void read_file_test(void)
{
    DECLSPEC_ALIGN(TEST_OVERLAPPED_READ_SIZE) static unsigned char aligned_buffer[TEST_OVERLAPPED_READ_SIZE];
    const char text[] = "foobar";
    HANDLE handle;
    IO_STATUS_BLOCK iosb;
    NTSTATUS status;
    int apc_count = 0;
    char buffer[128];
    LARGE_INTEGER offset;
    HANDLE event = CreateEventA( NULL, TRUE, FALSE, NULL );

    if (!(handle = create_temp_file( FILE_FLAG_OVERLAPPED ))) return;
    apc_count = 0;
    U(iosb).Status = 0xdeadbabe;
    iosb.Information = 0xdeadbeef;
    offset.QuadPart = 0;
    ResetEvent( event );
    status = pNtWriteFile( handle, event, apc, &apc_count, &iosb, text, strlen(text), &offset, NULL );
    ok( status == STATUS_PENDING || broken(status == STATUS_SUCCESS) /* before Vista */,
            "wrong status %x.\n", status );
    if (status == STATUS_PENDING) WaitForSingleObject( event, 1000 );
    ok( U(iosb).Status == STATUS_SUCCESS, "wrong status %x\n", U(iosb).Status );
    ok( iosb.Information == strlen(text), "wrong info %lu\n", iosb.Information );
    ok( is_signaled( event ), "event is not signaled\n" );
    ok( !apc_count, "apc was called\n" );
    SleepEx( 1, TRUE ); /* alertable sleep */
    ok( apc_count == 1, "apc was not called\n" );

    apc_count = 0;
    U(iosb).Status = 0xdeadbabe;
    iosb.Information = 0xdeadbeef;
    offset.QuadPart = 0;
    ResetEvent( event );
    status = pNtReadFile( handle, event, apc, &apc_count, &iosb, buffer, strlen(text) + 10, &offset, NULL );
    ok(status == STATUS_PENDING
            || broken(status == STATUS_SUCCESS) /* before Vista */,
            "wrong status %x.\n", status);
    if (status == STATUS_PENDING) WaitForSingleObject( event, 1000 );
    ok( U(iosb).Status == STATUS_SUCCESS, "wrong status %x\n", U(iosb).Status );
    ok( iosb.Information == strlen(text), "wrong info %lu\n", iosb.Information );
    ok( is_signaled( event ), "event is not signaled\n" );
    ok( !apc_count, "apc was called\n" );
    SleepEx( 1, TRUE ); /* alertable sleep */
    ok( apc_count == 1, "apc was not called\n" );

    /* read beyond eof */
    apc_count = 0;
    U(iosb).Status = 0xdeadbabe;
    iosb.Information = 0xdeadbeef;
    offset.QuadPart = strlen(text) + 2;
    status = pNtReadFile( handle, event, apc, &apc_count, &iosb, buffer, 2, &offset, NULL );
    ok(status == STATUS_PENDING || broken(status == STATUS_END_OF_FILE) /* before Vista */,
            "expected STATUS_PENDING, got %#x\n", status);
    if (status == STATUS_PENDING)  /* vista */
    {
        WaitForSingleObject( event, 1000 );
        ok( U(iosb).Status == STATUS_END_OF_FILE, "wrong status %x\n", U(iosb).Status );
        ok( iosb.Information == 0, "wrong info %lu\n", iosb.Information );
        ok( is_signaled( event ), "event is not signaled\n" );
        ok( !apc_count, "apc was called\n" );
        SleepEx( 1, TRUE ); /* alertable sleep */
        ok( apc_count == 1, "apc was not called\n" );
    }
    CloseHandle( handle );

    /* now a non-overlapped file */
    if (!(handle = create_temp_file(0))) return;
    apc_count = 0;
    U(iosb).Status = 0xdeadbabe;
    iosb.Information = 0xdeadbeef;
    offset.QuadPart = 0;
    status = pNtWriteFile( handle, event, apc, &apc_count, &iosb, text, strlen(text), &offset, NULL );
    ok( status == STATUS_END_OF_FILE ||
        status == STATUS_SUCCESS ||
        status == STATUS_PENDING,  /* vista */
        "wrong status %x\n", status );
    if (status == STATUS_PENDING) WaitForSingleObject( event, 1000 );
    ok( U(iosb).Status == STATUS_SUCCESS, "wrong status %x\n", U(iosb).Status );
    ok( iosb.Information == strlen(text), "wrong info %lu\n", iosb.Information );
    ok( is_signaled( event ), "event is not signaled\n" );
    ok( !apc_count, "apc was called\n" );
    SleepEx( 1, TRUE ); /* alertable sleep */
    ok( apc_count == 1, "apc was not called\n" );

    apc_count = 0;
    U(iosb).Status = 0xdeadbabe;
    iosb.Information = 0xdeadbeef;
    offset.QuadPart = 0;
    ResetEvent( event );
    status = pNtReadFile( handle, event, apc, &apc_count, &iosb, buffer, strlen(text) + 10, &offset, NULL );
    ok( status == STATUS_SUCCESS, "wrong status %x\n", status );
    ok( U(iosb).Status == STATUS_SUCCESS, "wrong status %x\n", U(iosb).Status );
    ok( iosb.Information == strlen(text), "wrong info %lu\n", iosb.Information );
    ok( is_signaled( event ), "event is not signaled\n" );
    ok( !apc_count, "apc was called\n" );
    SleepEx( 1, TRUE ); /* alertable sleep */
    todo_wine ok( !apc_count, "apc was called\n" );

    /* read beyond eof */
    apc_count = 0;
    U(iosb).Status = 0xdeadbabe;
    iosb.Information = 0xdeadbeef;
    offset.QuadPart = strlen(text) + 2;
    ResetEvent( event );
    status = pNtReadFile( handle, event, apc, &apc_count, &iosb, buffer, 2, &offset, NULL );
    ok( status == STATUS_END_OF_FILE, "wrong status %x\n", status );
    ok( U(iosb).Status == STATUS_END_OF_FILE, "wrong status %x\n", U(iosb).Status );
    ok( iosb.Information == 0, "wrong info %lu\n", iosb.Information );
    ok( is_signaled( event ), "event is not signaled\n" );
    ok( !apc_count, "apc was called\n" );
    SleepEx( 1, TRUE ); /* alertable sleep */
    ok( !apc_count, "apc was called\n" );

    CloseHandle( handle );

    if (!(handle = create_temp_file(FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING)))
        return;

    apc_count = 0;
    offset.QuadPart = 0;
    U(iosb).Status = 0xdeadbabe;
    iosb.Information = 0xdeadbeef;
    offset.QuadPart = 0;
    ResetEvent(event);
    status = pNtWriteFile(handle, event, apc, &apc_count, &iosb,
            aligned_buffer, sizeof(aligned_buffer), &offset, NULL);
    ok(status == STATUS_END_OF_FILE || status == STATUS_PENDING
            || broken(status == STATUS_SUCCESS) /* before Vista */,
            "Wrong status %x.\n", status);
    ok(U(iosb).Status == STATUS_SUCCESS, "Wrong status %x.\n", U(iosb).Status);
    ok(iosb.Information == sizeof(aligned_buffer), "Wrong info %lu.\n", iosb.Information);
    ok(is_signaled(event), "event is not signaled.\n");
    ok(!apc_count, "apc was called.\n");
    SleepEx(1, TRUE); /* alertable sleep */
    ok(apc_count == 1, "apc was not called.\n");

    apc_count = 0;
    offset.QuadPart = 0;
    U(iosb).Status = 0xdeadbabe;
    iosb.Information = 0xdeadbeef;
    offset.QuadPart = 0;
    ResetEvent(event);
    status = pNtReadFile(handle, event, apc, &apc_count, &iosb,
            aligned_buffer, sizeof(aligned_buffer), &offset, NULL);
    ok(status == STATUS_PENDING, "Wrong status %x.\n", status);
    WaitForSingleObject(event, 1000);
    ok(U(iosb).Status == STATUS_SUCCESS, "Wrong status %x.\n", U(iosb).Status);
    ok(iosb.Information == sizeof(aligned_buffer), "Wrong info %lu.\n", iosb.Information);
    ok(is_signaled(event), "event is not signaled.\n");
    ok(!apc_count, "apc was called.\n");
    SleepEx(1, TRUE); /* alertable sleep */
    ok(apc_count == 1, "apc was not called.\n");

    CloseHandle(handle);
    CloseHandle(event);
}

static void append_file_test(void)
{
    static const char text[6] = "foobar";
    HANDLE handle;
    NTSTATUS status;
    IO_STATUS_BLOCK iosb;
    LARGE_INTEGER offset;
    char path[MAX_PATH], buffer[MAX_PATH], buf[16];
    DWORD ret;

    GetTempPathA( MAX_PATH, path );
    GetTempFileNameA( path, "foo", 0, buffer );

    handle = CreateFileA(buffer, FILE_WRITE_DATA, 0, NULL, CREATE_ALWAYS, 0, 0);
    ok(handle != INVALID_HANDLE_VALUE, "CreateFile error %d\n", GetLastError());

    U(iosb).Status = -1;
    iosb.Information = -1;
    status = pNtWriteFile(handle, NULL, NULL, NULL, &iosb, text, 2, NULL, NULL);
    ok(status == STATUS_SUCCESS, "NtWriteFile error %#x\n", status);
    ok(U(iosb).Status == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %#x\n", U(iosb).Status);
    ok(iosb.Information == 2, "expected 2, got %lu\n", iosb.Information);

    CloseHandle(handle);

    /* It is possible to open a file with only FILE_APPEND_DATA access flags.
       It matches the O_WRONLY|O_APPEND open() posix behavior */
    handle = CreateFileA(buffer, FILE_APPEND_DATA, 0, NULL, OPEN_EXISTING, 0, 0);
    ok(handle != INVALID_HANDLE_VALUE, "CreateFile error %d\n", GetLastError());

    U(iosb).Status = -1;
    iosb.Information = -1;
    offset.QuadPart = 1;
    status = pNtWriteFile(handle, NULL, NULL, NULL, &iosb, text + 2, 2, &offset, NULL);
    ok(status == STATUS_SUCCESS, "NtWriteFile error %#x\n", status);
    ok(U(iosb).Status == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %#x\n", U(iosb).Status);
    ok(iosb.Information == 2, "expected 2, got %lu\n", iosb.Information);

    ret = SetFilePointer(handle, 0, NULL, FILE_CURRENT);
    ok(ret == 4, "expected 4, got %u\n", ret);

    U(iosb).Status = -1;
    iosb.Information = -1;
    offset.QuadPart = 3;
    status = pNtWriteFile(handle, NULL, NULL, NULL, &iosb, text + 4, 2, &offset, NULL);
    ok(status == STATUS_SUCCESS, "NtWriteFile error %#x\n", status);
    ok(U(iosb).Status == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %#x\n", U(iosb).Status);
    ok(iosb.Information == 2, "expected 2, got %lu\n", iosb.Information);

    ret = SetFilePointer(handle, 0, NULL, FILE_CURRENT);
    ok(ret == 6, "expected 6, got %u\n", ret);

    CloseHandle(handle);

    handle = CreateFileA(buffer, FILE_READ_DATA | FILE_WRITE_DATA | FILE_APPEND_DATA, 0, NULL, OPEN_EXISTING, 0, 0);
    ok(handle != INVALID_HANDLE_VALUE, "CreateFile error %d\n", GetLastError());

    memset(buf, 0, sizeof(buf));
    U(iosb).Status = -1;
    iosb.Information = -1;
    offset.QuadPart = 0;
    status = pNtReadFile(handle, 0, NULL, NULL, &iosb, buf, sizeof(buf), &offset, NULL);
    ok(status == STATUS_SUCCESS, "NtReadFile error %#x\n", status);
    ok(U(iosb).Status == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %#x\n", U(iosb).Status);
    ok(iosb.Information == 6, "expected 6, got %lu\n", iosb.Information);
    buf[6] = 0;
    ok(memcmp(buf, text, 6) == 0, "wrong file contents: %s\n", buf);

    U(iosb).Status = -1;
    iosb.Information = -1;
    offset.QuadPart = 0;
    status = pNtWriteFile(handle, NULL, NULL, NULL, &iosb, text + 3, 3, &offset, NULL);
    ok(status == STATUS_SUCCESS, "NtWriteFile error %#x\n", status);
    ok(U(iosb).Status == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %#x\n", U(iosb).Status);
    ok(iosb.Information == 3, "expected 3, got %lu\n", iosb.Information);

    memset(buf, 0, sizeof(buf));
    U(iosb).Status = -1;
    iosb.Information = -1;
    offset.QuadPart = 0;
    status = pNtReadFile(handle, 0, NULL, NULL, &iosb, buf, sizeof(buf), &offset, NULL);
    ok(status == STATUS_SUCCESS, "NtReadFile error %#x\n", status);
    ok(U(iosb).Status == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %#x\n", U(iosb).Status);
    ok(iosb.Information == 6, "expected 6, got %lu\n", iosb.Information);
    buf[6] = 0;
    ok(memcmp(buf, "barbar", 6) == 0, "wrong file contents: %s\n", buf);

    CloseHandle(handle);
    DeleteFileA(buffer);
}

static void nt_mailslot_test(void)
{
    HANDLE hslot;
    ACCESS_MASK DesiredAccess;
    OBJECT_ATTRIBUTES attr;

    ULONG CreateOptions;
    ULONG MailslotQuota;
    ULONG MaxMessageSize;
    LARGE_INTEGER TimeOut;
    IO_STATUS_BLOCK IoStatusBlock;
    NTSTATUS rc;
    UNICODE_STRING str;
    WCHAR buffer1[] = { '\\','?','?','\\','M','A','I','L','S','L','O','T','\\',
                        'R',':','\\','F','R','E','D','\0' };

    TimeOut.QuadPart = -1;

    pRtlInitUnicodeString(&str, buffer1);
    InitializeObjectAttributes(&attr, &str, OBJ_CASE_INSENSITIVE, 0, NULL);
    CreateOptions = MailslotQuota = MaxMessageSize = 0;
    DesiredAccess = GENERIC_READ;

    /*
     * Check for NULL pointer handling
     */
    rc = pNtCreateMailslotFile(NULL, DesiredAccess,
         &attr, &IoStatusBlock, CreateOptions, MailslotQuota, MaxMessageSize,
         &TimeOut);
    ok( rc == STATUS_ACCESS_VIOLATION ||
        rc == STATUS_INVALID_PARAMETER, /* win2k3 */
        "rc = %x not STATUS_ACCESS_VIOLATION or STATUS_INVALID_PARAMETER\n", rc);

    /*
     * Test to see if the Timeout can be NULL
     */
    hslot = (HANDLE)0xdeadbeef;
    rc = pNtCreateMailslotFile(&hslot, DesiredAccess,
         &attr, &IoStatusBlock, CreateOptions, MailslotQuota, MaxMessageSize,
         NULL);
    ok( rc == STATUS_SUCCESS ||
        rc == STATUS_INVALID_PARAMETER, /* win2k3 */
        "rc = %x not STATUS_SUCCESS or STATUS_INVALID_PARAMETER\n", rc);
    ok( hslot != 0, "Handle is invalid\n");

    if  ( rc == STATUS_SUCCESS ) pNtClose(hslot);

    /*
     * Test a valid call
     */
    InitializeObjectAttributes(&attr, &str, OBJ_CASE_INSENSITIVE, 0, NULL);
    rc = pNtCreateMailslotFile(&hslot, DesiredAccess,
         &attr, &IoStatusBlock, CreateOptions, MailslotQuota, MaxMessageSize,
         &TimeOut);
    ok( rc == STATUS_SUCCESS, "Create MailslotFile failed rc = %x\n", rc);
    ok( hslot != 0, "Handle is invalid\n");

    rc = pNtClose(hslot);
    ok( rc == STATUS_SUCCESS, "NtClose failed\n");
}

static void WINAPI user_apc_proc(ULONG_PTR arg)
{
    unsigned int *apc_count = (unsigned int *)arg;
    ++*apc_count;
}

static void test_set_io_completion(void)
{
    FILE_IO_COMPLETION_INFORMATION info[2] = {{0}};
    LARGE_INTEGER timeout = {{0}};
    unsigned int apc_count;
    IO_STATUS_BLOCK iosb;
    ULONG_PTR key, value;
    NTSTATUS res;
    ULONG count;
    SIZE_T size = 3;
    HANDLE h;

    if (sizeof(size) > 4) size |= (ULONGLONG)0x12345678 << 32;

    res = pNtCreateIoCompletion( &h, IO_COMPLETION_ALL_ACCESS, NULL, 0 );
    ok( res == STATUS_SUCCESS, "NtCreateIoCompletion failed: %#x\n", res );
    ok( h && h != INVALID_HANDLE_VALUE, "got invalid handle %p\n", h );

    res = pNtRemoveIoCompletion( h, &key, &value, &iosb, &timeout );
    ok( res == STATUS_TIMEOUT, "NtRemoveIoCompletion failed: %#x\n", res );

    res = pNtSetIoCompletion( h, CKEY_FIRST, CVALUE_FIRST, STATUS_INVALID_DEVICE_REQUEST, size );
    ok( res == STATUS_SUCCESS, "NtSetIoCompletion failed: %x\n", res );

    count = get_pending_msgs(h);
    ok( count == 1, "Unexpected msg count: %d\n", count );

    res = pNtRemoveIoCompletion( h, &key, &value, &iosb, &timeout );
    ok( res == STATUS_SUCCESS, "NtRemoveIoCompletion failed: %#x\n", res );
    ok( key == CKEY_FIRST, "Invalid completion key: %#lx\n", key );
    ok( iosb.Information == size, "Invalid iosb.Information: %lu\n", iosb.Information );
    ok( U(iosb).Status == STATUS_INVALID_DEVICE_REQUEST, "Invalid iosb.Status: %#x\n", U(iosb).Status );
    ok( value == CVALUE_FIRST, "Invalid completion value: %#lx\n", value );

    count = get_pending_msgs(h);
    ok( !count, "Unexpected msg count: %d\n", count );

    if (!pNtRemoveIoCompletionEx)
    {
        skip("NtRemoveIoCompletionEx() not present\n");
        pNtClose( h );
        return;
    }

    count = 0xdeadbeef;
    res = pNtRemoveIoCompletionEx( h, info, 2, &count, &timeout, FALSE );
    ok( res == STATUS_TIMEOUT, "NtRemoveIoCompletionEx failed: %#x\n", res );
    ok( count == 1, "wrong count %u\n", count );

    res = pNtSetIoCompletion( h, 123, 456, 789, size );
    ok( res == STATUS_SUCCESS, "NtSetIoCompletion failed: %#x\n", res );

    count = 0xdeadbeef;
    res = pNtRemoveIoCompletionEx( h, info, 2, &count, &timeout, FALSE );
    ok( res == STATUS_SUCCESS, "NtRemoveIoCompletionEx failed: %#x\n", res );
    ok( count == 1, "wrong count %u\n", count );
    ok( info[0].CompletionKey == 123, "wrong key %#lx\n", info[0].CompletionKey );
    ok( info[0].CompletionValue == 456, "wrong value %#lx\n", info[0].CompletionValue );
    ok( info[0].IoStatusBlock.Information == size, "wrong information %#lx\n",
        info[0].IoStatusBlock.Information );
    ok( U(info[0].IoStatusBlock).Status == 789, "wrong status %#x\n", U(info[0].IoStatusBlock).Status);

    res = pNtSetIoCompletion( h, 123, 456, 789, size );
    ok( res == STATUS_SUCCESS, "NtSetIoCompletion failed: %#x\n", res );

    res = pNtSetIoCompletion( h, 12, 34, 56, size );
    ok( res == STATUS_SUCCESS, "NtSetIoCompletion failed: %#x\n", res );

    count = 0xdeadbeef;
    res = pNtRemoveIoCompletionEx( h, info, 2, &count, &timeout, FALSE );
    ok( res == STATUS_SUCCESS, "NtRemoveIoCompletionEx failed: %#x\n", res );
    ok( count == 2, "wrong count %u\n", count );
    ok( info[0].CompletionKey == 123, "wrong key %#lx\n", info[0].CompletionKey );
    ok( info[0].CompletionValue == 456, "wrong value %#lx\n", info[0].CompletionValue );
    ok( info[0].IoStatusBlock.Information == size, "wrong information %#lx\n",
        info[0].IoStatusBlock.Information );
    ok( U(info[0].IoStatusBlock).Status == 789, "wrong status %#x\n", U(info[0].IoStatusBlock).Status);
    ok( info[1].CompletionKey == 12, "wrong key %#lx\n", info[1].CompletionKey );
    ok( info[1].CompletionValue == 34, "wrong value %#lx\n", info[1].CompletionValue );
    ok( info[1].IoStatusBlock.Information == size, "wrong information %#lx\n",
        info[1].IoStatusBlock.Information );
    ok( U(info[1].IoStatusBlock).Status == 56, "wrong status %#x\n", U(info[1].IoStatusBlock).Status);

    res = pNtSetIoCompletion( h, 123, 456, 789, size );
    ok( res == STATUS_SUCCESS, "NtSetIoCompletion failed: %#x\n", res );

    res = pNtSetIoCompletion( h, 12, 34, 56, size );
    ok( res == STATUS_SUCCESS, "NtSetIoCompletion failed: %#x\n", res );

    count = 0xdeadbeef;
    res = pNtRemoveIoCompletionEx( h, info, 1, &count, NULL, FALSE );
    ok( res == STATUS_SUCCESS, "NtRemoveIoCompletionEx failed: %#x\n", res );
    ok( count == 1, "wrong count %u\n", count );
    ok( info[0].CompletionKey == 123, "wrong key %#lx\n", info[0].CompletionKey );
    ok( info[0].CompletionValue == 456, "wrong value %#lx\n", info[0].CompletionValue );
    ok( info[0].IoStatusBlock.Information == size, "wrong information %#lx\n",
        info[0].IoStatusBlock.Information );
    ok( U(info[0].IoStatusBlock).Status == 789, "wrong status %#x\n", U(info[0].IoStatusBlock).Status);

    count = 0xdeadbeef;
    res = pNtRemoveIoCompletionEx( h, info, 1, &count, NULL, FALSE );
    ok( res == STATUS_SUCCESS, "NtRemoveIoCompletionEx failed: %#x\n", res );
    ok( count == 1, "wrong count %u\n", count );
    ok( info[0].CompletionKey == 12, "wrong key %#lx\n", info[0].CompletionKey );
    ok( info[0].CompletionValue == 34, "wrong value %#lx\n", info[0].CompletionValue );
    ok( info[0].IoStatusBlock.Information == size, "wrong information %#lx\n",
        info[0].IoStatusBlock.Information );
    ok( U(info[0].IoStatusBlock).Status == 56, "wrong status %#x\n", U(info[0].IoStatusBlock).Status);

    apc_count = 0;
    QueueUserAPC( user_apc_proc, GetCurrentThread(), (ULONG_PTR)&apc_count );

    count = 0xdeadbeef;
    res = pNtRemoveIoCompletionEx( h, info, 2, &count, &timeout, FALSE );
    ok( res == STATUS_TIMEOUT, "NtRemoveIoCompletionEx failed: %#x\n", res );
    ok( count == 1, "wrong count %u\n", count );
    ok( !apc_count, "wrong apc count %d\n", apc_count );

    res = pNtRemoveIoCompletionEx( h, info, 2, &count, &timeout, TRUE );
    ok( res == STATUS_USER_APC, "NtRemoveIoCompletionEx failed: %#x\n", res );
    ok( count == 1, "wrong count %u\n", count );
    ok( apc_count == 1, "wrong apc count %u\n", apc_count );

    apc_count = 0;
    QueueUserAPC( user_apc_proc, GetCurrentThread(), (ULONG_PTR)&apc_count );

    res = pNtSetIoCompletion( h, 123, 456, 789, size );
    ok( res == STATUS_SUCCESS, "NtSetIoCompletion failed: %#x\n", res );

    res = pNtRemoveIoCompletionEx( h, info, 2, &count, &timeout, TRUE );
    ok( res == STATUS_SUCCESS, "NtRemoveIoCompletionEx failed: %#x\n", res );
    ok( count == 1, "wrong count %u\n", count );
    ok( !apc_count, "wrong apc count %u\n", apc_count );

    SleepEx( 1, TRUE );

    pNtClose( h );
}

static void test_file_io_completion(void)
{
    static const char pipe_name[] = "\\\\.\\pipe\\iocompletiontestnamedpipe";

    IO_STATUS_BLOCK iosb;
    BYTE send_buf[TEST_BUF_LEN], recv_buf[TEST_BUF_LEN];
    FILE_COMPLETION_INFORMATION fci;
    LARGE_INTEGER timeout = {{0}};
    HANDLE server, client;
    ULONG_PTR key, value;
    OVERLAPPED o = {0};
    int apc_count = 0;
    NTSTATUS res;
    DWORD read;
    long count;
    HANDLE h;

    res = pNtCreateIoCompletion( &h, IO_COMPLETION_ALL_ACCESS, NULL, 0 );
    ok( res == STATUS_SUCCESS, "NtCreateIoCompletion failed: %#x\n", res );
    ok( h && h != INVALID_HANDLE_VALUE, "got invalid handle %p\n", h );
    fci.CompletionPort = h;
    fci.CompletionKey = CKEY_SECOND;

    server = CreateNamedPipeA( pipe_name, PIPE_ACCESS_INBOUND,
                               PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                               4, 1024, 1024, 1000, NULL );
    ok( server != INVALID_HANDLE_VALUE, "CreateNamedPipe failed: %u\n", GetLastError() );
    client = CreateFileA( pipe_name, GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
                          FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, NULL );
    ok( client != INVALID_HANDLE_VALUE, "CreateFile failed: %u\n", GetLastError() );

    U(iosb).Status = 0xdeadbeef;
    res = pNtSetInformationFile( server, &iosb, &fci, sizeof(fci), FileCompletionInformation );
    ok( res == STATUS_INVALID_PARAMETER, "NtSetInformationFile failed: %#x\n", res );
todo_wine
    ok( U(iosb).Status == 0xdeadbeef, "wrong status %#x\n", U(iosb).Status );
    CloseHandle( client );
    CloseHandle( server );

    server = CreateNamedPipeA( pipe_name, PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
                               PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                               4, 1024, 1024, 1000, NULL );
    ok( server != INVALID_HANDLE_VALUE, "CreateNamedPipe failed: %u\n", GetLastError() );
    client = CreateFileA( pipe_name, GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
                          FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, NULL );
    ok( client != INVALID_HANDLE_VALUE, "CreateFile failed: %u\n", GetLastError() );

    U(iosb).Status = 0xdeadbeef;
    res = pNtSetInformationFile( server, &iosb, &fci, sizeof(fci), FileCompletionInformation );
    ok( res == STATUS_SUCCESS, "NtSetInformationFile failed: %#x\n", res );
    ok( U(iosb).Status == STATUS_SUCCESS, "wrong status %#x\n", U(iosb).Status );

    memset( send_buf, 0, TEST_BUF_LEN );
    memset( recv_buf, 0xde, TEST_BUF_LEN );
    count = get_pending_msgs(h);
    ok( !count, "Unexpected msg count: %ld\n", count );
    ReadFile( server, recv_buf, TEST_BUF_LEN, &read, &o);
    count = get_pending_msgs(h);
    ok( !count, "Unexpected msg count: %ld\n", count );
    WriteFile( client, send_buf, TEST_BUF_LEN, &read, NULL );

    res = pNtRemoveIoCompletion( h, &key, &value, &iosb, &timeout );
    ok( res == STATUS_SUCCESS, "NtRemoveIoCompletion failed: %#x\n", res );
    ok( key == CKEY_SECOND, "Invalid completion key: %#lx\n", key );
    ok( iosb.Information == 3, "Invalid iosb.Information: %ld\n", iosb.Information );
    ok( U(iosb).Status == STATUS_SUCCESS, "Invalid iosb.Status: %#x\n", U(iosb).Status );
    ok( value == (ULONG_PTR)&o, "Invalid completion value: %#lx\n", value );
    ok( !memcmp( send_buf, recv_buf, TEST_BUF_LEN ),
            "Receive buffer (%02x %02x %02x) did not match send buffer (%02x %02x %02x)\n",
            recv_buf[0], recv_buf[1], recv_buf[2], send_buf[0], send_buf[1], send_buf[2] );
    count = get_pending_msgs(h);
    ok( !count, "Unexpected msg count: %ld\n", count );

    memset( send_buf, 0, TEST_BUF_LEN );
    memset( recv_buf, 0xde, TEST_BUF_LEN );
    WriteFile( client, send_buf, 2, &read, NULL );
    count = get_pending_msgs(h);
    ok( !count, "Unexpected msg count: %ld\n", count );
    ReadFile( server, recv_buf, 2, &read, &o);
    count = get_pending_msgs(h);
    ok( count == 1, "Unexpected msg count: %ld\n", count );

    res = pNtRemoveIoCompletion( h, &key, &value, &iosb, &timeout );
    ok( res == STATUS_SUCCESS, "NtRemoveIoCompletion failed: %#x\n", res );
    ok( key == CKEY_SECOND, "Invalid completion key: %#lx\n", key );
    ok( iosb.Information == 2, "Invalid iosb.Information: %ld\n", iosb.Information );
    ok( U(iosb).Status == STATUS_SUCCESS, "Invalid iosb.Status: %#x\n", U(iosb).Status );
    ok( value == (ULONG_PTR)&o, "Invalid completion value: %#lx\n", value );
    ok( !memcmp( send_buf, recv_buf, 2 ),
            "Receive buffer (%02x %02x) did not match send buffer (%02x %02x)\n",
            recv_buf[0], recv_buf[1], send_buf[0], send_buf[1] );

    ReadFile( server, recv_buf, TEST_BUF_LEN, &read, &o);
    CloseHandle( server );
    count = get_pending_msgs(h);
    ok( count == 1, "Unexpected msg count: %ld\n", count );

    res = pNtRemoveIoCompletion( h, &key, &value, &iosb, &timeout );
    ok( res == STATUS_SUCCESS, "NtRemoveIoCompletion failed: %#x\n", res );
    ok( key == CKEY_SECOND, "Invalid completion key: %lx\n", key );
    ok( iosb.Information == 0, "Invalid iosb.Information: %ld\n", iosb.Information );
    ok( U(iosb).Status == STATUS_PIPE_BROKEN, "Invalid iosb.Status: %x\n", U(iosb).Status );
    ok( value == (ULONG_PTR)&o, "Invalid completion value: %lx\n", value );

    CloseHandle( client );

    /* test associating a completion port with a handle after an async is queued */
    server = CreateNamedPipeA( pipe_name, PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
                               PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                               4, 1024, 1024, 1000, NULL );
    ok( server != INVALID_HANDLE_VALUE, "CreateNamedPipe failed: %u\n", GetLastError() );
    client = CreateFileA( pipe_name, GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
                          FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, NULL );
    ok( client != INVALID_HANDLE_VALUE, "CreateFile failed: %u\n", GetLastError() );

    memset( send_buf, 0, TEST_BUF_LEN );
    memset( recv_buf, 0xde, TEST_BUF_LEN );
    count = get_pending_msgs(h);
    ok( !count, "Unexpected msg count: %ld\n", count );
    ReadFile( server, recv_buf, TEST_BUF_LEN, &read, &o);

    U(iosb).Status = 0xdeadbeef;
    res = pNtSetInformationFile( server, &iosb, &fci, sizeof(fci), FileCompletionInformation );
    ok( res == STATUS_SUCCESS, "NtSetInformationFile failed: %x\n", res );
    ok( U(iosb).Status == STATUS_SUCCESS, "iosb.Status invalid: %x\n", U(iosb).Status );
    count = get_pending_msgs(h);
    ok( !count, "Unexpected msg count: %ld\n", count );

    WriteFile( client, send_buf, TEST_BUF_LEN, &read, NULL );

    res = pNtRemoveIoCompletion( h, &key, &value, &iosb, &timeout );
    ok( res == STATUS_SUCCESS, "NtRemoveIoCompletion failed: %#x\n", res );
    ok( key == CKEY_SECOND, "Invalid completion key: %#lx\n", key );
    ok( iosb.Information == 3, "Invalid iosb.Information: %ld\n", iosb.Information );
    ok( U(iosb).Status == STATUS_SUCCESS, "Invalid iosb.Status: %#x\n", U(iosb).Status );
    ok( value == (ULONG_PTR)&o, "Invalid completion value: %#lx\n", value );
    ok( !memcmp( send_buf, recv_buf, TEST_BUF_LEN ),
            "Receive buffer (%02x %02x %02x) did not match send buffer (%02x %02x %02x)\n",
            recv_buf[0], recv_buf[1], recv_buf[2], send_buf[0], send_buf[1], send_buf[2] );

    count = get_pending_msgs(h);
    ok( !count, "Unexpected msg count: %ld\n", count );

    /* using APCs on handle with associated completion port is not allowed */
    res = pNtReadFile( server, NULL, apc, &apc_count, &iosb, recv_buf, sizeof(recv_buf), NULL, NULL );
    ok(res == STATUS_INVALID_PARAMETER, "NtReadFile returned %x\n", res);

    CloseHandle( server );
    CloseHandle( client );

    /* test associating a completion port with a handle after an async using APC is queued */
    server = CreateNamedPipeA( pipe_name, PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
                               PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                               4, 1024, 1024, 1000, NULL );
    ok( server != INVALID_HANDLE_VALUE, "CreateNamedPipe failed: %u\n", GetLastError() );
    client = CreateFileA( pipe_name, GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
                          FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, NULL );
    ok( client != INVALID_HANDLE_VALUE, "CreateFile failed: %u\n", GetLastError() );

    apc_count = 0;
    memset( send_buf, 0, TEST_BUF_LEN );
    memset( recv_buf, 0xde, TEST_BUF_LEN );
    count = get_pending_msgs(h);
    ok( !count, "Unexpected msg count: %ld\n", count );

    res = pNtReadFile( server, NULL, apc, &apc_count, &iosb, recv_buf, sizeof(recv_buf), NULL, NULL );
    ok(res == STATUS_PENDING, "NtReadFile returned %x\n", res);

    U(iosb).Status = 0xdeadbeef;
    res = pNtSetInformationFile( server, &iosb, &fci, sizeof(fci), FileCompletionInformation );
    ok( res == STATUS_SUCCESS, "NtSetInformationFile failed: %x\n", res );
    ok( U(iosb).Status == STATUS_SUCCESS, "iosb.Status invalid: %x\n", U(iosb).Status );
    count = get_pending_msgs(h);
    ok( !count, "Unexpected msg count: %ld\n", count );

    WriteFile( client, send_buf, TEST_BUF_LEN, &read, NULL );

    ok(!apc_count, "apc_count = %u\n", apc_count);
    count = get_pending_msgs(h);
    ok( !count, "Unexpected msg count: %ld\n", count );

    SleepEx(1, TRUE); /* alertable sleep */
    ok(apc_count == 1, "apc was not called\n");
    count = get_pending_msgs(h);
    ok( !count, "Unexpected msg count: %ld\n", count );

    /* using APCs on handle with associated completion port is not allowed */
    res = pNtReadFile( server, NULL, apc, &apc_count, &iosb, recv_buf, sizeof(recv_buf), NULL, NULL );
    ok(res == STATUS_INVALID_PARAMETER, "NtReadFile returned %x\n", res);

    CloseHandle( server );
    CloseHandle( client );
    pNtClose( h );
}

static void test_file_full_size_information(void)
{
    IO_STATUS_BLOCK io;
    FILE_FS_FULL_SIZE_INFORMATION ffsi;
    FILE_FS_SIZE_INFORMATION fsi;
    HANDLE h;
    NTSTATUS res;

    if(!(h = create_temp_file(0))) return ;

    memset(&ffsi,0,sizeof(ffsi));
    memset(&fsi,0,sizeof(fsi));

    /* Assume No Quota Settings configured on Wine Testbot */
    res = pNtQueryVolumeInformationFile(h, &io, &ffsi, sizeof ffsi, FileFsFullSizeInformation);
    ok(res == STATUS_SUCCESS, "cannot get attributes, res %x\n", res);
    res = pNtQueryVolumeInformationFile(h, &io, &fsi, sizeof fsi, FileFsSizeInformation);
    ok(res == STATUS_SUCCESS, "cannot get attributes, res %x\n", res);

    /* Test for FileFsSizeInformation */
    ok(fsi.TotalAllocationUnits.QuadPart > 0,
        "[fsi] TotalAllocationUnits expected positive, got 0x%s\n",
        wine_dbgstr_longlong(fsi.TotalAllocationUnits.QuadPart));
    ok(fsi.AvailableAllocationUnits.QuadPart > 0,
        "[fsi] AvailableAllocationUnits expected positive, got 0x%s\n",
        wine_dbgstr_longlong(fsi.AvailableAllocationUnits.QuadPart));

    /* Assume file system is NTFS */
    ok(fsi.BytesPerSector == 512, "[fsi] BytesPerSector expected 512, got %d\n",fsi.BytesPerSector);
    ok(fsi.SectorsPerAllocationUnit == 8, "[fsi] SectorsPerAllocationUnit expected 8, got %d\n",fsi.SectorsPerAllocationUnit);

    ok(ffsi.TotalAllocationUnits.QuadPart > 0,
        "[ffsi] TotalAllocationUnits expected positive, got negative value 0x%s\n",
        wine_dbgstr_longlong(ffsi.TotalAllocationUnits.QuadPart));
    ok(ffsi.CallerAvailableAllocationUnits.QuadPart > 0,
        "[ffsi] CallerAvailableAllocationUnits expected positive, got negative value 0x%s\n",
        wine_dbgstr_longlong(ffsi.CallerAvailableAllocationUnits.QuadPart));
    ok(ffsi.ActualAvailableAllocationUnits.QuadPart > 0,
        "[ffsi] ActualAvailableAllocationUnits expected positive, got negative value 0x%s\n",
        wine_dbgstr_longlong(ffsi.ActualAvailableAllocationUnits.QuadPart));
    ok(ffsi.TotalAllocationUnits.QuadPart == fsi.TotalAllocationUnits.QuadPart,
        "[ffsi] TotalAllocationUnits error fsi:0x%s, ffsi:0x%s\n",
        wine_dbgstr_longlong(fsi.TotalAllocationUnits.QuadPart),
        wine_dbgstr_longlong(ffsi.TotalAllocationUnits.QuadPart));
    ok(ffsi.CallerAvailableAllocationUnits.QuadPart == fsi.AvailableAllocationUnits.QuadPart,
        "[ffsi] CallerAvailableAllocationUnits error fsi:0x%s, ffsi: 0x%s\n",
        wine_dbgstr_longlong(fsi.AvailableAllocationUnits.QuadPart),
        wine_dbgstr_longlong(ffsi.CallerAvailableAllocationUnits.QuadPart));

    /* Assume file system is NTFS */
    ok(ffsi.BytesPerSector == 512, "[ffsi] BytesPerSector expected 512, got %d\n",ffsi.BytesPerSector);
    ok(ffsi.SectorsPerAllocationUnit == 8, "[ffsi] SectorsPerAllocationUnit expected 8, got %d\n",ffsi.SectorsPerAllocationUnit);

    CloseHandle( h );
}

static void test_file_basic_information(void)
{
    FILE_BASIC_INFORMATION fbi, fbi2;
    IO_STATUS_BLOCK io;
    HANDLE h;
    int res;
    int attrib_mask = FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_NORMAL;

    if (!(h = create_temp_file(0))) return;

    /* Check default first */
    memset(&fbi, 0, sizeof(fbi));
    res = pNtQueryInformationFile(h, &io, &fbi, sizeof fbi, FileBasicInformation);
    ok ( res == STATUS_SUCCESS, "can't get attributes, res %x\n", res);
    ok ( (fbi.FileAttributes & FILE_ATTRIBUTE_ARCHIVE) == FILE_ATTRIBUTE_ARCHIVE,
         "attribute %x not expected\n", fbi.FileAttributes );

    memset(&fbi2, 0, sizeof(fbi2));
    fbi2.LastWriteTime.QuadPart = -1;
    U(io).Status = 0xdeadbeef;
    res = pNtSetInformationFile(h, &io, &fbi2, sizeof fbi2, FileBasicInformation);
    ok ( res == STATUS_SUCCESS, "can't set system attribute, NtSetInformationFile returned %x\n", res );
    ok ( U(io).Status == STATUS_SUCCESS, "can't set system attribute, io.Status is %x\n", U(io).Status );

    memset(&fbi2, 0, sizeof(fbi2));
    res = pNtQueryInformationFile(h, &io, &fbi2, sizeof fbi2, FileBasicInformation);
    ok ( res == STATUS_SUCCESS, "can't get attributes, res %x\n", res);
    ok ( fbi2.LastWriteTime.QuadPart == fbi.LastWriteTime.QuadPart, "unexpected write time.\n");

    memset(&fbi2, 0, sizeof(fbi2));
    U(io).Status = 0xdeadbeef;
    res = pNtSetInformationFile(h, &io, &fbi2, sizeof fbi2, FileBasicInformation);
    ok ( res == STATUS_SUCCESS, "can't set system attribute, NtSetInformationFile returned %x\n", res );
    ok ( U(io).Status == STATUS_SUCCESS, "can't set system attribute, io.Status is %x\n", U(io).Status );

    memset(&fbi2, 0, sizeof(fbi2));
    res = pNtQueryInformationFile(h, &io, &fbi2, sizeof fbi2, FileBasicInformation);
    ok ( res == STATUS_SUCCESS, "can't get attributes, res %x\n", res);
    ok ( fbi2.LastWriteTime.QuadPart == fbi.LastWriteTime.QuadPart, "unexpected write time.\n");

    /* Then SYSTEM */
    /* Clear fbi to avoid setting times */
    memset(&fbi, 0, sizeof(fbi));
    fbi.FileAttributes = FILE_ATTRIBUTE_SYSTEM;
    U(io).Status = 0xdeadbeef;
    res = pNtSetInformationFile(h, &io, &fbi, sizeof fbi, FileBasicInformation);
    ok ( res == STATUS_SUCCESS, "can't set system attribute, NtSetInformationFile returned %x\n", res );
    ok ( U(io).Status == STATUS_SUCCESS, "can't set system attribute, io.Status is %x\n", U(io).Status );

    memset(&fbi, 0, sizeof(fbi));
    res = pNtQueryInformationFile(h, &io, &fbi, sizeof fbi, FileBasicInformation);
    ok ( res == STATUS_SUCCESS, "can't get attributes\n");
    ok ( (fbi.FileAttributes & attrib_mask) == FILE_ATTRIBUTE_SYSTEM, "attribute %x not FILE_ATTRIBUTE_SYSTEM (ok in old linux without xattr)\n", fbi.FileAttributes );

    /* Then HIDDEN */
    memset(&fbi, 0, sizeof(fbi));
    fbi.FileAttributes = FILE_ATTRIBUTE_HIDDEN;
    U(io).Status = 0xdeadbeef;
    res = pNtSetInformationFile(h, &io, &fbi, sizeof fbi, FileBasicInformation);
    ok ( res == STATUS_SUCCESS, "can't set system attribute, NtSetInformationFile returned %x\n", res );
    ok ( U(io).Status == STATUS_SUCCESS, "can't set system attribute, io.Status is %x\n", U(io).Status );

    memset(&fbi, 0, sizeof(fbi));
    res = pNtQueryInformationFile(h, &io, &fbi, sizeof fbi, FileBasicInformation);
    ok ( res == STATUS_SUCCESS, "can't get attributes\n");
    ok ( (fbi.FileAttributes & attrib_mask) == FILE_ATTRIBUTE_HIDDEN, "attribute %x not FILE_ATTRIBUTE_HIDDEN (ok in old linux without xattr)\n", fbi.FileAttributes );

    /* Check NORMAL last of all (to make sure we can clear attributes) */
    memset(&fbi, 0, sizeof(fbi));
    fbi.FileAttributes = FILE_ATTRIBUTE_NORMAL;
    U(io).Status = 0xdeadbeef;
    res = pNtSetInformationFile(h, &io, &fbi, sizeof fbi, FileBasicInformation);
    ok ( res == STATUS_SUCCESS, "can't set normal attribute, NtSetInformationFile returned %x\n", res );
    ok ( U(io).Status == STATUS_SUCCESS, "can't set normal attribute, io.Status is %x\n", U(io).Status );

    memset(&fbi, 0, sizeof(fbi));
    res = pNtQueryInformationFile(h, &io, &fbi, sizeof fbi, FileBasicInformation);
    ok ( res == STATUS_SUCCESS, "can't get attributes\n");
    todo_wine ok ( (fbi.FileAttributes & attrib_mask) == FILE_ATTRIBUTE_NORMAL, "attribute %x not 0\n", fbi.FileAttributes );

    CloseHandle( h );
}

static void test_file_all_information(void)
{
    IO_STATUS_BLOCK io;
    /* FileAllInformation, like FileNameInformation, has a variable-length pathname
     * buffer at the end.  Vista objects with STATUS_BUFFER_OVERFLOW if you
     * don't leave enough room there.
     */
    struct {
      FILE_ALL_INFORMATION fai;
      WCHAR buf[256];
    } fai_buf;
    HANDLE h;
    int res;
    int attrib_mask = FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_NORMAL;

    if (!(h = create_temp_file(0))) return;

    /* Check default first */
    res = pNtQueryInformationFile(h, &io, &fai_buf.fai, sizeof fai_buf, FileAllInformation);
    ok ( res == STATUS_SUCCESS, "can't get attributes, res %x\n", res);
    ok ( (fai_buf.fai.BasicInformation.FileAttributes & FILE_ATTRIBUTE_ARCHIVE) == FILE_ATTRIBUTE_ARCHIVE,
         "attribute %x not expected\n", fai_buf.fai.BasicInformation.FileAttributes );

    /* Then SYSTEM */
    /* Clear fbi to avoid setting times */
    memset(&fai_buf.fai.BasicInformation, 0, sizeof(fai_buf.fai.BasicInformation));
    fai_buf.fai.BasicInformation.FileAttributes = FILE_ATTRIBUTE_SYSTEM;
    U(io).Status = 0xdeadbeef;
    res = pNtSetInformationFile(h, &io, &fai_buf.fai, sizeof fai_buf, FileAllInformation);
    ok ( res == STATUS_INVALID_INFO_CLASS || broken(res == STATUS_NOT_IMPLEMENTED), "shouldn't be able to set FileAllInformation, res %x\n", res);
    todo_wine ok ( U(io).Status == 0xdeadbeef, "shouldn't be able to set FileAllInformation, io.Status is %x\n", U(io).Status);
    U(io).Status = 0xdeadbeef;
    res = pNtSetInformationFile(h, &io, &fai_buf.fai.BasicInformation, sizeof fai_buf.fai.BasicInformation, FileBasicInformation);
    ok ( res == STATUS_SUCCESS, "can't set system attribute, res: %x\n", res );
    ok ( U(io).Status == STATUS_SUCCESS, "can't set system attribute, io.Status: %x\n", U(io).Status );

    memset(&fai_buf.fai, 0, sizeof(fai_buf.fai));
    res = pNtQueryInformationFile(h, &io, &fai_buf.fai, sizeof fai_buf, FileAllInformation);
    ok ( res == STATUS_SUCCESS, "can't get attributes, res %x\n", res);
    ok ( (fai_buf.fai.BasicInformation.FileAttributes & attrib_mask) == FILE_ATTRIBUTE_SYSTEM, "attribute %x not FILE_ATTRIBUTE_SYSTEM (ok in old linux without xattr)\n", fai_buf.fai.BasicInformation.FileAttributes );

    /* Then HIDDEN */
    memset(&fai_buf.fai.BasicInformation, 0, sizeof(fai_buf.fai.BasicInformation));
    fai_buf.fai.BasicInformation.FileAttributes = FILE_ATTRIBUTE_HIDDEN;
    U(io).Status = 0xdeadbeef;
    res = pNtSetInformationFile(h, &io, &fai_buf.fai.BasicInformation, sizeof fai_buf.fai.BasicInformation, FileBasicInformation);
    ok ( res == STATUS_SUCCESS, "can't set system attribute, res: %x\n", res );
    ok ( U(io).Status == STATUS_SUCCESS, "can't set system attribute, io.Status: %x\n", U(io).Status );

    memset(&fai_buf.fai, 0, sizeof(fai_buf.fai));
    res = pNtQueryInformationFile(h, &io, &fai_buf.fai, sizeof fai_buf, FileAllInformation);
    ok ( res == STATUS_SUCCESS, "can't get attributes\n");
    ok ( (fai_buf.fai.BasicInformation.FileAttributes & attrib_mask) == FILE_ATTRIBUTE_HIDDEN, "attribute %x not FILE_ATTRIBUTE_HIDDEN (ok in old linux without xattr)\n", fai_buf.fai.BasicInformation.FileAttributes );

    /* Check NORMAL last of all (to make sure we can clear attributes) */
    memset(&fai_buf.fai.BasicInformation, 0, sizeof(fai_buf.fai.BasicInformation));
    fai_buf.fai.BasicInformation.FileAttributes = FILE_ATTRIBUTE_NORMAL;
    U(io).Status = 0xdeadbeef;
    res = pNtSetInformationFile(h, &io, &fai_buf.fai.BasicInformation, sizeof fai_buf.fai.BasicInformation, FileBasicInformation);
    ok ( res == STATUS_SUCCESS, "can't set system attribute, res: %x\n", res );
    ok ( U(io).Status == STATUS_SUCCESS, "can't set system attribute, io.Status: %x\n", U(io).Status );

    memset(&fai_buf.fai, 0, sizeof(fai_buf.fai));
    res = pNtQueryInformationFile(h, &io, &fai_buf.fai, sizeof fai_buf, FileAllInformation);
    ok ( res == STATUS_SUCCESS, "can't get attributes\n");
    todo_wine ok ( (fai_buf.fai.BasicInformation.FileAttributes & attrib_mask) == FILE_ATTRIBUTE_NORMAL, "attribute %x not FILE_ATTRIBUTE_NORMAL\n", fai_buf.fai.BasicInformation.FileAttributes );

    CloseHandle( h );
}

static void delete_object( WCHAR *path )
{
    BOOL ret = DeleteFileW( path );
    ok( ret || GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_ACCESS_DENIED,
        "DeleteFileW failed with %u\n", GetLastError() );
    if (!ret && GetLastError() == ERROR_ACCESS_DENIED)
    {
        ret = RemoveDirectoryW( path );
        ok( ret, "RemoveDirectoryW failed with %u\n", GetLastError() );
    }
}

static void test_file_rename_information(void)
{
    static const WCHAR foo_txtW[] = {'\\','f','o','o','.','t','x','t',0};
    static const WCHAR fooW[] = {'f','o','o',0};
    WCHAR tmp_path[MAX_PATH], oldpath[MAX_PATH + 16], newpath[MAX_PATH + 16], *filename, *p;
    FILE_RENAME_INFORMATION *fri;
    FILE_NAME_INFORMATION *fni;
    BOOL success, fileDeleted;
    UNICODE_STRING name_str;
    HANDLE handle, handle2;
    IO_STATUS_BLOCK io;
    NTSTATUS res;

    GetTempPathW( MAX_PATH, tmp_path );

    /* oldpath is a file, newpath doesn't exist */
    res = GetTempFileNameW( tmp_path, fooW, 0, oldpath );
    ok( res != 0, "failed to create temp file\n" );
    handle = CreateFileW( oldpath, GENERIC_WRITE | DELETE, 0, NULL, CREATE_ALWAYS, 0, 0 );
    ok( handle != INVALID_HANDLE_VALUE, "CreateFileW failed\n" );

    res = GetTempFileNameW( tmp_path, fooW, 0, newpath );
    ok( res != 0, "failed to create temp file\n" );
    pRtlDosPathNameToNtPathName_U( newpath, &name_str, NULL, NULL );
    DeleteFileW( newpath );
    fri = HeapAlloc( GetProcessHeap(), 0, sizeof(FILE_RENAME_INFORMATION) + name_str.Length );
    fri->ReplaceIfExists = FALSE;
    fri->RootDirectory = NULL;
    fri->FileNameLength = name_str.Length;
    memcpy( fri->FileName, name_str.Buffer, name_str.Length );
    pRtlFreeUnicodeString( &name_str );

    U(io).Status = 0xdeadbeef;
    res = pNtSetInformationFile( handle, &io, fri, sizeof(FILE_RENAME_INFORMATION) + fri->FileNameLength, FileRenameInformation );
    ok( U(io).Status == STATUS_SUCCESS, "io.Status expected STATUS_SUCCESS, got %x\n", U(io).Status );
    ok( res == STATUS_SUCCESS, "res expected STATUS_SUCCESS, got %x\n", res );
    fileDeleted = GetFileAttributesW( oldpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( fileDeleted, "file should not exist\n" );
    fileDeleted = GetFileAttributesW( newpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );

    fni = HeapAlloc( GetProcessHeap(), 0, sizeof(FILE_NAME_INFORMATION) + MAX_PATH * sizeof(WCHAR) );
    res = pNtQueryInformationFile( handle, &io, fni, sizeof(FILE_NAME_INFORMATION) + MAX_PATH * sizeof(WCHAR), FileNameInformation );
    ok( res == STATUS_SUCCESS, "res expected STATUS_SUCCESS, got %x\n", res );
    fni->FileName[ fni->FileNameLength / sizeof(WCHAR) ] = 0;
    ok( !lstrcmpiW(fni->FileName, newpath + 2), "FileName expected %s, got %s\n",
        wine_dbgstr_w(newpath + 2), wine_dbgstr_w(fni->FileName) );
    HeapFree( GetProcessHeap(), 0, fni );

    CloseHandle( handle );
    HeapFree( GetProcessHeap(), 0, fri );
    delete_object( oldpath );
    delete_object( newpath );

    /* oldpath is a file, newpath is a file, Replace = FALSE */
    res = GetTempFileNameW( tmp_path, fooW, 0, oldpath );
    ok( res != 0, "failed to create temp file\n" );
    handle = CreateFileW( oldpath, GENERIC_WRITE | DELETE, 0, NULL, CREATE_ALWAYS, 0, 0 );
    ok( handle != INVALID_HANDLE_VALUE, "CreateFileW failed\n" );

    res = GetTempFileNameW( tmp_path, fooW, 0, newpath );
    ok( res != 0, "failed to create temp file\n" );
    pRtlDosPathNameToNtPathName_U( newpath, &name_str, NULL, NULL );
    fri = HeapAlloc( GetProcessHeap(), 0, sizeof(FILE_RENAME_INFORMATION) + name_str.Length );
    fri->ReplaceIfExists = FALSE;
    fri->RootDirectory = NULL;
    fri->FileNameLength = name_str.Length;
    memcpy( fri->FileName, name_str.Buffer, name_str.Length );
    pRtlFreeUnicodeString( &name_str );

    U(io).Status = 0xdeadbeef;
    res = pNtSetInformationFile( handle, &io, fri, sizeof(FILE_RENAME_INFORMATION) + fri->FileNameLength, FileRenameInformation );
    todo_wine ok( U(io).Status == 0xdeadbeef, "io.Status expected 0xdeadbeef, got %x\n", U(io).Status );
    ok( res == STATUS_OBJECT_NAME_COLLISION, "res expected STATUS_OBJECT_NAME_COLLISION, got %x\n", res );
    fileDeleted = GetFileAttributesW( oldpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );
    fileDeleted = GetFileAttributesW( newpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );

    CloseHandle( handle );
    HeapFree( GetProcessHeap(), 0, fri );
    delete_object( oldpath );
    delete_object( newpath );

    /* oldpath is a file, newpath is a file, Replace = TRUE */
    res = GetTempFileNameW( tmp_path, fooW, 0, oldpath );
    ok( res != 0, "failed to create temp file\n" );
    handle = CreateFileW( oldpath, GENERIC_WRITE | DELETE, 0, NULL, CREATE_ALWAYS, 0, 0 );
    ok( handle != INVALID_HANDLE_VALUE, "CreateFileW failed\n" );

    res = GetTempFileNameW( tmp_path, fooW, 0, newpath );
    ok( res != 0, "failed to create temp file\n" );
    pRtlDosPathNameToNtPathName_U( newpath, &name_str, NULL, NULL );
    fri = HeapAlloc( GetProcessHeap(), 0, sizeof(FILE_RENAME_INFORMATION) + name_str.Length );
    fri->ReplaceIfExists = TRUE;
    fri->RootDirectory = NULL;
    fri->FileNameLength = name_str.Length;
    memcpy( fri->FileName, name_str.Buffer, name_str.Length );
    pRtlFreeUnicodeString( &name_str );

    U(io).Status = 0xdeadbeef;
    res = pNtSetInformationFile( handle, &io, fri, sizeof(FILE_RENAME_INFORMATION) + fri->FileNameLength, FileRenameInformation );
    ok( U(io).Status == STATUS_SUCCESS, "io.Status expected STATUS_SUCCESS, got %x\n", U(io).Status );
    ok( res == STATUS_SUCCESS, "res expected STATUS_SUCCESS, got %x\n", res );
    fileDeleted = GetFileAttributesW( oldpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( fileDeleted, "file should not exist\n" );
    fileDeleted = GetFileAttributesW( newpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );

    CloseHandle( handle );
    HeapFree( GetProcessHeap(), 0, fri );
    delete_object( oldpath );
    delete_object( newpath );

    /* oldpath is a file, newpath is a file, Replace = FALSE, target file opened */
    res = GetTempFileNameW( tmp_path, fooW, 0, oldpath );
    ok( res != 0, "failed to create temp file\n" );
    handle = CreateFileW( oldpath, GENERIC_WRITE | DELETE, 0, NULL, CREATE_ALWAYS, 0, 0 );
    ok( handle != INVALID_HANDLE_VALUE, "CreateFileW failed\n" );

    res = GetTempFileNameW( tmp_path, fooW, 0, newpath );
    ok( res != 0, "failed to create temp file\n" );
    handle2 = CreateFileW( newpath, GENERIC_WRITE | DELETE, 0, NULL, CREATE_ALWAYS, 0, 0 );
    ok( handle2 != INVALID_HANDLE_VALUE, "CreateFileW failed\n" );

    pRtlDosPathNameToNtPathName_U( newpath, &name_str, NULL, NULL );
    fri = HeapAlloc( GetProcessHeap(), 0, sizeof(FILE_RENAME_INFORMATION) + name_str.Length );
    fri->ReplaceIfExists = FALSE;
    fri->RootDirectory = NULL;
    fri->FileNameLength = name_str.Length;
    memcpy( fri->FileName, name_str.Buffer, name_str.Length );
    pRtlFreeUnicodeString( &name_str );

    U(io).Status = 0xdeadbeef;
    res = pNtSetInformationFile( handle, &io, fri, sizeof(FILE_RENAME_INFORMATION) + fri->FileNameLength, FileRenameInformation );
    todo_wine ok( U(io).Status == 0xdeadbeef, "io.Status expected 0xdeadbeef, got %x\n", U(io).Status );
    ok( res == STATUS_OBJECT_NAME_COLLISION, "res expected STATUS_OBJECT_NAME_COLLISION, got %x\n", res );
    fileDeleted = GetFileAttributesW( oldpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );
    fileDeleted = GetFileAttributesW( newpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );

    CloseHandle( handle );
    CloseHandle( handle2 );
    HeapFree( GetProcessHeap(), 0, fri );
    delete_object( oldpath );
    delete_object( newpath );

    /* oldpath is a file, newpath is a file, Replace = TRUE, target file opened */
    res = GetTempFileNameW( tmp_path, fooW, 0, oldpath );
    ok( res != 0, "failed to create temp file\n" );
    handle = CreateFileW( oldpath, GENERIC_WRITE | DELETE, 0, NULL, CREATE_ALWAYS, 0, 0 );
    ok( handle != INVALID_HANDLE_VALUE, "CreateFileW failed\n" );

    res = GetTempFileNameW( tmp_path, fooW, 0, newpath );
    ok( res != 0, "failed to create temp file\n" );
    handle2 = CreateFileW( newpath, GENERIC_WRITE | DELETE, 0, NULL, CREATE_ALWAYS, 0, 0 );
    ok( handle2 != INVALID_HANDLE_VALUE, "CreateFileW failed\n" );

    pRtlDosPathNameToNtPathName_U( newpath, &name_str, NULL, NULL );
    fri = HeapAlloc( GetProcessHeap(), 0, sizeof(FILE_RENAME_INFORMATION) + name_str.Length );
    fri->ReplaceIfExists = TRUE;
    fri->RootDirectory = NULL;
    fri->FileNameLength = name_str.Length;
    memcpy( fri->FileName, name_str.Buffer, name_str.Length );
    pRtlFreeUnicodeString( &name_str );

    U(io).Status = 0xdeadbeef;
    res = pNtSetInformationFile( handle, &io, fri, sizeof(FILE_RENAME_INFORMATION) + fri->FileNameLength, FileRenameInformation );
    todo_wine ok( U(io).Status == 0xdeadbeef, "io.Status expected 0xdeadbeef, got %x\n", U(io).Status );
    ok( res == STATUS_ACCESS_DENIED, "res expected STATUS_ACCESS_DENIED, got %x\n", res );
    fileDeleted = GetFileAttributesW( oldpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );
    fileDeleted = GetFileAttributesW( newpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );

    CloseHandle( handle );
    CloseHandle( handle2 );
    HeapFree( GetProcessHeap(), 0, fri );
    delete_object( oldpath );
    delete_object( newpath );

    /* oldpath is a directory, newpath doesn't exist, Replace = FALSE */
    res = GetTempFileNameW( tmp_path, fooW, 0, oldpath );
    ok( res != 0, "failed to create temp file\n" );
    DeleteFileW( oldpath );
    success = CreateDirectoryW( oldpath, NULL );
    ok( success != 0, "failed to create temp directory\n" );
    handle = CreateFileW( oldpath, GENERIC_WRITE | DELETE, 0, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0 );
    ok( handle != INVALID_HANDLE_VALUE, "CreateFileW failed\n" );

    res = GetTempFileNameW( tmp_path, fooW, 0, newpath );
    ok( res != 0, "failed to create temp file\n" );
    pRtlDosPathNameToNtPathName_U( newpath, &name_str, NULL, NULL );
    DeleteFileW( newpath );
    fri = HeapAlloc( GetProcessHeap(), 0, sizeof(FILE_RENAME_INFORMATION) + name_str.Length );
    fri->ReplaceIfExists = FALSE;
    fri->RootDirectory = NULL;
    fri->FileNameLength = name_str.Length;
    memcpy( fri->FileName, name_str.Buffer, name_str.Length );
    pRtlFreeUnicodeString( &name_str );

    U(io).Status = 0xdeadbeef;
    res = pNtSetInformationFile( handle, &io, fri, sizeof(FILE_RENAME_INFORMATION) + fri->FileNameLength, FileRenameInformation );
    ok( U(io).Status == STATUS_SUCCESS, "io.Status expected STATUS_SUCCESS, got %x\n", U(io).Status );
    ok( res == STATUS_SUCCESS, "res expected STATUS_SUCCESS, got %x\n", res );
    fileDeleted = GetFileAttributesW( oldpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( fileDeleted, "file should not exist\n" );
    fileDeleted = GetFileAttributesW( newpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );

    fni = HeapAlloc( GetProcessHeap(), 0, sizeof(FILE_NAME_INFORMATION) + MAX_PATH * sizeof(WCHAR) );
    res = pNtQueryInformationFile( handle, &io, fni, sizeof(FILE_NAME_INFORMATION) + MAX_PATH * sizeof(WCHAR), FileNameInformation );
    ok( res == STATUS_SUCCESS, "res expected STATUS_SUCCESS, got %x\n", res );
    fni->FileName[ fni->FileNameLength / sizeof(WCHAR) ] = 0;
    ok( !lstrcmpiW(fni->FileName, newpath + 2), "FileName expected %s, got %s\n",
        wine_dbgstr_w(newpath + 2), wine_dbgstr_w(fni->FileName) );
    HeapFree( GetProcessHeap(), 0, fni );

    CloseHandle( handle );
    HeapFree( GetProcessHeap(), 0, fri );
    delete_object( oldpath );
    delete_object( newpath );

    /* oldpath is a directory (but child object opened), newpath doesn't exist, Replace = FALSE */
    res = GetTempFileNameW( tmp_path, fooW, 0, oldpath );
    ok( res != 0, "failed to create temp file\n" );
    DeleteFileW( oldpath );
    success = CreateDirectoryW( oldpath, NULL );
    ok( success != 0, "failed to create temp directory\n" );
    handle = CreateFileW( oldpath, GENERIC_WRITE | DELETE, 0, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0 );
    ok( handle != INVALID_HANDLE_VALUE, "CreateFileW failed\n" );

    lstrcpyW( newpath, oldpath );
    lstrcatW( newpath, foo_txtW );
    handle2 = CreateFileW( newpath, GENERIC_WRITE | DELETE, 0, NULL, CREATE_ALWAYS, FILE_FLAG_DELETE_ON_CLOSE, 0 );
    ok( handle2 != INVALID_HANDLE_VALUE, "CreateFileW failed\n" );

    res = GetTempFileNameW( tmp_path, fooW, 0, newpath );
    ok( res != 0, "failed to create temp file\n" );
    pRtlDosPathNameToNtPathName_U( newpath, &name_str, NULL, NULL );
    DeleteFileW( newpath );
    fri = HeapAlloc( GetProcessHeap(), 0, sizeof(FILE_RENAME_INFORMATION) + name_str.Length );
    fri->ReplaceIfExists = FALSE;
    fri->RootDirectory = NULL;
    fri->FileNameLength = name_str.Length;
    memcpy( fri->FileName, name_str.Buffer, name_str.Length );
    pRtlFreeUnicodeString( &name_str );

    U(io).Status = 0xdeadbeef;
    res = pNtSetInformationFile( handle, &io, fri, sizeof(FILE_RENAME_INFORMATION) + fri->FileNameLength, FileRenameInformation );
    todo_wine ok( U(io).Status == 0xdeadbeef, "io.Status expected 0xdeadbeef, got %x\n", U(io).Status );
    todo_wine ok( res == STATUS_ACCESS_DENIED, "res expected STATUS_ACCESS_DENIED, got %x\n", res );
    fileDeleted = GetFileAttributesW( oldpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    todo_wine ok( !fileDeleted, "file should exist\n" );
    fileDeleted = GetFileAttributesW( newpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    todo_wine ok( fileDeleted, "file should not exist\n" );

    CloseHandle( handle );
    CloseHandle( handle2 );
    HeapFree( GetProcessHeap(), 0, fri );
    delete_object( oldpath );
    if (res == STATUS_SUCCESS) /* remove when Wine is fixed */
    {
        lstrcpyW( oldpath, newpath );
        lstrcatW( oldpath, foo_txtW );
        delete_object( oldpath );
    }
    delete_object( newpath );

    /* oldpath is a directory, newpath is a file, Replace = FALSE */
    res = GetTempFileNameW( tmp_path, fooW, 0, oldpath );
    ok( res != 0, "failed to create temp file\n" );
    DeleteFileW( oldpath );
    success = CreateDirectoryW( oldpath, NULL );
    ok( success != 0, "failed to create temp directory\n" );
    handle = CreateFileW( oldpath, GENERIC_WRITE | DELETE, 0, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0 );
    ok( handle != INVALID_HANDLE_VALUE, "CreateFileW failed\n" );

    res = GetTempFileNameW( tmp_path, fooW, 0, newpath );
    ok( res != 0, "failed to create temp file\n" );
    pRtlDosPathNameToNtPathName_U( newpath, &name_str, NULL, NULL );
    fri = HeapAlloc( GetProcessHeap(), 0, sizeof(FILE_RENAME_INFORMATION) + name_str.Length );
    fri->ReplaceIfExists = FALSE;
    fri->RootDirectory = NULL;
    fri->FileNameLength = name_str.Length;
    memcpy( fri->FileName, name_str.Buffer, name_str.Length );
    pRtlFreeUnicodeString( &name_str );

    U(io).Status = 0xdeadbeef;
    res = pNtSetInformationFile( handle, &io, fri, sizeof(FILE_RENAME_INFORMATION) + fri->FileNameLength, FileRenameInformation );
    todo_wine ok( U(io).Status == 0xdeadbeef, "io.Status expected 0xdeadbeef, got %x\n", U(io).Status );
    ok( res == STATUS_OBJECT_NAME_COLLISION, "res expected STATUS_OBJECT_NAME_COLLISION, got %x\n", res );
    fileDeleted = GetFileAttributesW( oldpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );
    fileDeleted = GetFileAttributesW( newpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );

    CloseHandle( handle );
    HeapFree( GetProcessHeap(), 0, fri );
    delete_object( oldpath );
    delete_object( newpath );

    /* oldpath is a directory, newpath is a file, Replace = FALSE, target file opened */
    res = GetTempFileNameW( tmp_path, fooW, 0, oldpath );
    ok( res != 0, "failed to create temp file\n" );
    DeleteFileW( oldpath );
    success = CreateDirectoryW( oldpath, NULL );
    ok( success != 0, "failed to create temp directory\n" );
    handle = CreateFileW( oldpath, GENERIC_WRITE | DELETE, 0, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0 );
    ok( handle != INVALID_HANDLE_VALUE, "CreateFileW failed\n" );

    res = GetTempFileNameW( tmp_path, fooW, 0, newpath );
    ok( res != 0, "failed to create temp file\n" );
    handle2 = CreateFileW( newpath, GENERIC_WRITE | DELETE, 0, NULL, CREATE_ALWAYS, 0, 0 );
    ok( handle2 != INVALID_HANDLE_VALUE, "CreateFileW failed\n" );

    pRtlDosPathNameToNtPathName_U( newpath, &name_str, NULL, NULL );
    fri = HeapAlloc( GetProcessHeap(), 0, sizeof(FILE_RENAME_INFORMATION) + name_str.Length );
    fri->ReplaceIfExists = FALSE;
    fri->RootDirectory = NULL;
    fri->FileNameLength = name_str.Length;
    memcpy( fri->FileName, name_str.Buffer, name_str.Length );
    pRtlFreeUnicodeString( &name_str );

    U(io).Status = 0xdeadbeef;
    res = pNtSetInformationFile( handle, &io, fri, sizeof(FILE_RENAME_INFORMATION) + fri->FileNameLength, FileRenameInformation );
    todo_wine ok( U(io).Status == 0xdeadbeef, "io.Status expected 0xdeadbeef, got %x\n", U(io).Status );
    ok( res == STATUS_OBJECT_NAME_COLLISION, "res expected STATUS_OBJECT_NAME_COLLISION, got %x\n", res );
    fileDeleted = GetFileAttributesW( oldpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );
    fileDeleted = GetFileAttributesW( newpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );

    CloseHandle( handle );
    CloseHandle( handle2 );
    HeapFree( GetProcessHeap(), 0, fri );
    delete_object( oldpath );
    delete_object( newpath );

    /* oldpath is a directory, newpath is a file, Replace = TRUE */
    res = GetTempFileNameW( tmp_path, fooW, 0, oldpath );
    ok( res != 0, "failed to create temp file\n" );
    DeleteFileW( oldpath );
    success = CreateDirectoryW( oldpath, NULL );
    ok( success != 0, "failed to create temp directory\n" );
    handle = CreateFileW( oldpath, GENERIC_WRITE | DELETE, 0, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0 );
    ok( handle != INVALID_HANDLE_VALUE, "CreateFileW failed\n" );

    res = GetTempFileNameW( tmp_path, fooW, 0, newpath );
    ok( res != 0, "failed to create temp file\n" );
    pRtlDosPathNameToNtPathName_U( newpath, &name_str, NULL, NULL );
    fri = HeapAlloc( GetProcessHeap(), 0, sizeof(FILE_RENAME_INFORMATION) + name_str.Length );
    fri->ReplaceIfExists = TRUE;
    fri->RootDirectory = NULL;
    fri->FileNameLength = name_str.Length;
    memcpy( fri->FileName, name_str.Buffer, name_str.Length );
    pRtlFreeUnicodeString( &name_str );

    U(io).Status = 0xdeadbeef;
    res = pNtSetInformationFile( handle, &io, fri, sizeof(FILE_RENAME_INFORMATION) + fri->FileNameLength, FileRenameInformation );
    ok( U(io).Status == STATUS_SUCCESS, "io.Status expected STATUS_SUCCESS, got %x\n", U(io).Status );
    ok( res == STATUS_SUCCESS, "res expected STATUS_SUCCESS, got %x\n", res );
    fileDeleted = GetFileAttributesW( oldpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( fileDeleted, "file should not exist\n" );
    fileDeleted = GetFileAttributesW( newpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );

    CloseHandle( handle );
    HeapFree( GetProcessHeap(), 0, fri );
    delete_object( oldpath );
    delete_object( newpath );

    /* oldpath is a directory, newpath is a file, Replace = TRUE, target file opened */
    res = GetTempFileNameW( tmp_path, fooW, 0, oldpath );
    ok( res != 0, "failed to create temp file\n" );
    DeleteFileW( oldpath );
    success = CreateDirectoryW( oldpath, NULL );
    ok( success != 0, "failed to create temp directory\n" );
    handle = CreateFileW( oldpath, GENERIC_WRITE | DELETE, 0, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0 );
    ok( handle != INVALID_HANDLE_VALUE, "CreateFileW failed\n" );

    res = GetTempFileNameW( tmp_path, fooW, 0, newpath );
    ok( res != 0, "failed to create temp file\n" );
    handle2 = CreateFileW( newpath, GENERIC_WRITE | DELETE, 0, NULL, CREATE_ALWAYS, 0, 0 );
    ok( handle2 != INVALID_HANDLE_VALUE, "CreateFileW failed\n" );

    pRtlDosPathNameToNtPathName_U( newpath, &name_str, NULL, NULL );
    fri = HeapAlloc( GetProcessHeap(), 0, sizeof(FILE_RENAME_INFORMATION) + name_str.Length );
    fri->ReplaceIfExists = TRUE;
    fri->RootDirectory = NULL;
    fri->FileNameLength = name_str.Length;
    memcpy( fri->FileName, name_str.Buffer, name_str.Length );
    pRtlFreeUnicodeString( &name_str );

    U(io).Status = 0xdeadbeef;
    res = pNtSetInformationFile( handle, &io, fri, sizeof(FILE_RENAME_INFORMATION) + fri->FileNameLength, FileRenameInformation );
    todo_wine ok( U(io).Status == 0xdeadbeef, "io.Status expected 0xdeadbeef, got %x\n", U(io).Status );
    ok( res == STATUS_ACCESS_DENIED, "res expected STATUS_ACCESS_DENIED, got %x\n", res );
    fileDeleted = GetFileAttributesW( oldpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );
    fileDeleted = GetFileAttributesW( newpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );

    CloseHandle( handle );
    CloseHandle( handle2 );
    HeapFree( GetProcessHeap(), 0, fri );
    delete_object( oldpath );
    delete_object( newpath );

    /* oldpath is a directory, newpath is a directory, Replace = FALSE */
    res = GetTempFileNameW( tmp_path, fooW, 0, oldpath );
    ok( res != 0, "failed to create temp file\n" );
    DeleteFileW( oldpath );
    success = CreateDirectoryW( oldpath, NULL );
    ok( success != 0, "failed to create temp directory\n" );
    handle = CreateFileW( oldpath, GENERIC_WRITE | DELETE, 0, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0 );
    ok( handle != INVALID_HANDLE_VALUE, "CreateFileW failed\n" );

    res = GetTempFileNameW( tmp_path, fooW, 0, newpath );
    ok( res != 0, "failed to create temp file\n" );
    DeleteFileW( newpath );
    success = CreateDirectoryW( newpath, NULL );
    ok( success != 0, "failed to create temp directory\n" );
    pRtlDosPathNameToNtPathName_U( newpath, &name_str, NULL, NULL );
    fri = HeapAlloc( GetProcessHeap(), 0, sizeof(FILE_RENAME_INFORMATION) + name_str.Length );
    fri->ReplaceIfExists = FALSE;
    fri->RootDirectory = NULL;
    fri->FileNameLength = name_str.Length;
    memcpy( fri->FileName, name_str.Buffer, name_str.Length );
    pRtlFreeUnicodeString( &name_str );

    U(io).Status = 0xdeadbeef;
    res = pNtSetInformationFile( handle, &io, fri, sizeof(FILE_RENAME_INFORMATION) + fri->FileNameLength, FileRenameInformation );
    todo_wine ok( U(io).Status == 0xdeadbeef, "io.Status expected 0xdeadbeef, got %x\n", U(io).Status );
    ok( res == STATUS_OBJECT_NAME_COLLISION, "res expected STATUS_OBJECT_NAME_COLLISION, got %x\n", res );
    fileDeleted = GetFileAttributesW( oldpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );
    fileDeleted = GetFileAttributesW( newpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );

    CloseHandle( handle );
    HeapFree( GetProcessHeap(), 0, fri );
    delete_object( oldpath );
    delete_object( newpath );

    /* oldpath is a directory, newpath is a directory, Replace = TRUE */
    res = GetTempFileNameW( tmp_path, fooW, 0, oldpath );
    ok( res != 0, "failed to create temp file\n" );
    DeleteFileW( oldpath );
    success = CreateDirectoryW( oldpath, NULL );
    ok( success != 0, "failed to create temp directory\n" );
    handle = CreateFileW( oldpath, GENERIC_WRITE | DELETE, 0, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0 );
    ok( handle != INVALID_HANDLE_VALUE, "CreateFileW failed\n" );

    res = GetTempFileNameW( tmp_path, fooW, 0, newpath );
    ok( res != 0, "failed to create temp file\n" );
    DeleteFileW( newpath );
    success = CreateDirectoryW( newpath, NULL );
    ok( success != 0, "failed to create temp directory\n" );
    pRtlDosPathNameToNtPathName_U( newpath, &name_str, NULL, NULL );
    fri = HeapAlloc( GetProcessHeap(), 0, sizeof(FILE_RENAME_INFORMATION) + name_str.Length );
    fri->ReplaceIfExists = TRUE;
    fri->RootDirectory = NULL;
    fri->FileNameLength = name_str.Length;
    memcpy( fri->FileName, name_str.Buffer, name_str.Length );
    pRtlFreeUnicodeString( &name_str );

    U(io).Status = 0xdeadbeef;
    res = pNtSetInformationFile( handle, &io, fri, sizeof(FILE_RENAME_INFORMATION) + fri->FileNameLength, FileRenameInformation );
    todo_wine ok( U(io).Status == 0xdeadbeef, "io.Status expected 0xdeadbeef, got %x\n", U(io).Status );
    ok( res == STATUS_ACCESS_DENIED, "res expected STATUS_ACCESS_DENIED, got %x\n", res );
    fileDeleted = GetFileAttributesW( oldpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );
    fileDeleted = GetFileAttributesW( newpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );

    CloseHandle( handle );
    HeapFree( GetProcessHeap(), 0, fri );
    delete_object( oldpath );
    delete_object( newpath );

    /* oldpath is a directory, newpath is a directory, Replace = TRUE, target file opened */
    res = GetTempFileNameW( tmp_path, fooW, 0, oldpath );
    ok( res != 0, "failed to create temp file\n" );
    DeleteFileW( oldpath );
    success = CreateDirectoryW( oldpath, NULL );
    ok( success != 0, "failed to create temp directory\n" );
    handle = CreateFileW( oldpath, GENERIC_WRITE | DELETE, 0, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0 );
    ok( handle != INVALID_HANDLE_VALUE, "CreateFileW failed\n" );

    res = GetTempFileNameW( tmp_path, fooW, 0, newpath );
    ok( res != 0, "failed to create temp file\n" );
    DeleteFileW( newpath );
    success = CreateDirectoryW( newpath, NULL );
    ok( success != 0, "failed to create temp directory\n" );
    handle2 = CreateFileW( newpath, GENERIC_WRITE | DELETE, 0, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0 );
    ok( handle2 != INVALID_HANDLE_VALUE, "CreateFileW failed\n" );

    pRtlDosPathNameToNtPathName_U( newpath, &name_str, NULL, NULL );
    fri = HeapAlloc( GetProcessHeap(), 0, sizeof(FILE_RENAME_INFORMATION) + name_str.Length );
    fri->ReplaceIfExists = TRUE;
    fri->RootDirectory = NULL;
    fri->FileNameLength = name_str.Length;
    memcpy( fri->FileName, name_str.Buffer, name_str.Length );
    pRtlFreeUnicodeString( &name_str );

    U(io).Status = 0xdeadbeef;
    res = pNtSetInformationFile( handle, &io, fri, sizeof(FILE_RENAME_INFORMATION) + fri->FileNameLength, FileRenameInformation );
    todo_wine ok( U(io).Status == 0xdeadbeef, "io.Status expected 0xdeadbeef, got %x\n", U(io).Status );
    ok( res == STATUS_ACCESS_DENIED, "res expected STATUS_ACCESS_DENIED, got %x\n", res );
    fileDeleted = GetFileAttributesW( oldpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );
    fileDeleted = GetFileAttributesW( newpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );

    CloseHandle( handle );
    CloseHandle( handle2 );
    HeapFree( GetProcessHeap(), 0, fri );
    delete_object( oldpath );
    delete_object( newpath );

    /* oldpath is a file, newpath is a directory, Replace = FALSE */
    res = GetTempFileNameW( tmp_path, fooW, 0, oldpath );
    ok( res != 0, "failed to create temp file\n" );
    handle = CreateFileW( oldpath, GENERIC_WRITE | DELETE, 0, NULL, CREATE_ALWAYS, 0, 0 );
    ok( handle != INVALID_HANDLE_VALUE, "CreateFileW failed\n" );

    res = GetTempFileNameW( tmp_path, fooW, 0, newpath );
    ok( res != 0, "failed to create temp file\n" );
    DeleteFileW( newpath );
    success = CreateDirectoryW( newpath, NULL );
    ok( success != 0, "failed to create temp directory\n" );
    pRtlDosPathNameToNtPathName_U( newpath, &name_str, NULL, NULL );
    fri = HeapAlloc( GetProcessHeap(), 0, sizeof(FILE_RENAME_INFORMATION) + name_str.Length );
    fri->ReplaceIfExists = FALSE;
    fri->RootDirectory = NULL;
    fri->FileNameLength = name_str.Length;
    memcpy( fri->FileName, name_str.Buffer, name_str.Length );
    pRtlFreeUnicodeString( &name_str );

    U(io).Status = 0xdeadbeef;
    res = pNtSetInformationFile( handle, &io, fri, sizeof(FILE_RENAME_INFORMATION) + fri->FileNameLength, FileRenameInformation );
    todo_wine ok( U(io).Status == 0xdeadbeef, "io.Status expected 0xdeadbeef, got %x\n", U(io).Status );
    ok( res == STATUS_OBJECT_NAME_COLLISION, "res expected STATUS_OBJECT_NAME_COLLISION, got %x\n", res );
    fileDeleted = GetFileAttributesW( oldpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );
    fileDeleted = GetFileAttributesW( newpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );

    CloseHandle( handle );
    HeapFree( GetProcessHeap(), 0, fri );
    delete_object( oldpath );
    delete_object( newpath );

    /* oldpath is a file, newpath is a directory, Replace = TRUE */
    res = GetTempFileNameW( tmp_path, fooW, 0, oldpath );
    ok( res != 0, "failed to create temp file\n" );
    handle = CreateFileW( oldpath, GENERIC_WRITE | DELETE, 0, NULL, CREATE_ALWAYS, 0, 0 );
    ok( handle != INVALID_HANDLE_VALUE, "CreateFileW failed\n" );

    res = GetTempFileNameW( tmp_path, fooW, 0, newpath );
    ok( res != 0, "failed to create temp file\n" );
    DeleteFileW( newpath );
    success = CreateDirectoryW( newpath, NULL );
    ok( success != 0, "failed to create temp directory\n" );
    pRtlDosPathNameToNtPathName_U( newpath, &name_str, NULL, NULL );
    fri = HeapAlloc( GetProcessHeap(), 0, sizeof(FILE_RENAME_INFORMATION) + name_str.Length );
    fri->ReplaceIfExists = TRUE;
    fri->RootDirectory = NULL;
    fri->FileNameLength = name_str.Length;
    memcpy( fri->FileName, name_str.Buffer, name_str.Length );
    pRtlFreeUnicodeString( &name_str );

    U(io).Status = 0xdeadbeef;
    res = pNtSetInformationFile( handle, &io, fri, sizeof(FILE_RENAME_INFORMATION) + fri->FileNameLength, FileRenameInformation );
    todo_wine ok( U(io).Status == 0xdeadbeef, "io.Status expected 0xdeadbeef, got %x\n", U(io).Status );
    ok( res == STATUS_ACCESS_DENIED, "res expected STATUS_ACCESS_DENIED, got %x\n", res );
    fileDeleted = GetFileAttributesW( oldpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );
    fileDeleted = GetFileAttributesW( newpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );

    CloseHandle( handle );
    HeapFree( GetProcessHeap(), 0, fri );
    delete_object( oldpath );
    delete_object( newpath );

    /* oldpath is a file, newpath doesn't exist, test with RootDir != NULL */
    res = GetTempFileNameW( tmp_path, fooW, 0, oldpath );
    ok( res != 0, "failed to create temp file\n" );
    handle = CreateFileW( oldpath, GENERIC_WRITE | DELETE, 0, NULL, CREATE_ALWAYS, 0, 0 );
    ok( handle != INVALID_HANDLE_VALUE, "CreateFileW failed\n" );

    res = GetTempFileNameW( tmp_path, fooW, 0, newpath );
    ok( res != 0, "failed to create temp file\n" );
    DeleteFileW( newpath );
    for (filename = newpath, p = newpath; *p; p++)
        if (*p == '\\') filename = p + 1;
    handle2 = CreateFileW( tmp_path, 0, 0, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0 );
    ok( handle2 != INVALID_HANDLE_VALUE, "CreateFileW failed\n" );

    fri = HeapAlloc( GetProcessHeap(), 0, sizeof(FILE_RENAME_INFORMATION) + lstrlenW(filename) * sizeof(WCHAR) );
    fri->ReplaceIfExists = FALSE;
    fri->RootDirectory = handle2;
    fri->FileNameLength = lstrlenW(filename) * sizeof(WCHAR);
    memcpy( fri->FileName, filename, fri->FileNameLength );

    U(io).Status = 0xdeadbeef;
    res = pNtSetInformationFile( handle, &io, fri, sizeof(FILE_RENAME_INFORMATION) + fri->FileNameLength, FileRenameInformation );
    ok( U(io).Status == STATUS_SUCCESS, "io.Status expected STATUS_SUCCESS, got %x\n", U(io).Status );
    ok( res == STATUS_SUCCESS, "res expected STATUS_SUCCESS, got %x\n", res );
    fileDeleted = GetFileAttributesW( oldpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( fileDeleted, "file should not exist\n" );
    fileDeleted = GetFileAttributesW( newpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );

    fni = HeapAlloc( GetProcessHeap(), 0, sizeof(FILE_NAME_INFORMATION) + MAX_PATH * sizeof(WCHAR) );
    res = pNtQueryInformationFile( handle, &io, fni, sizeof(FILE_NAME_INFORMATION) + MAX_PATH * sizeof(WCHAR), FileNameInformation );
    ok( res == STATUS_SUCCESS, "res expected STATUS_SUCCESS, got %x\n", res );
    fni->FileName[ fni->FileNameLength / sizeof(WCHAR) ] = 0;
    ok( !lstrcmpiW(fni->FileName, newpath + 2), "FileName expected %s, got %s\n",
                  wine_dbgstr_w(newpath + 2), wine_dbgstr_w(fni->FileName) );
    HeapFree( GetProcessHeap(), 0, fni );

    CloseHandle( handle );
    CloseHandle( handle2 );
    HeapFree( GetProcessHeap(), 0, fri );
    delete_object( oldpath );
    delete_object( newpath );

    /* oldpath == newpath */
    res = GetTempFileNameW( tmp_path, fooW, 0, oldpath );
    ok( res != 0, "failed to create temp file\n" );
    handle = CreateFileW( oldpath, GENERIC_WRITE | DELETE, 0, NULL, CREATE_ALWAYS, 0, 0 );
    ok( handle != INVALID_HANDLE_VALUE, "CreateFileW failed\n" );

    pRtlDosPathNameToNtPathName_U( oldpath, &name_str, NULL, NULL );
    fri = HeapAlloc( GetProcessHeap(), 0, sizeof(FILE_RENAME_INFORMATION) + name_str.Length );
    fri->ReplaceIfExists = FALSE;
    fri->RootDirectory = NULL;
    fri->FileNameLength = name_str.Length;
    memcpy( fri->FileName, name_str.Buffer, name_str.Length );
    pRtlFreeUnicodeString( &name_str );

    U(io).Status = 0xdeadbeef;
    res = pNtSetInformationFile( handle, &io, fri, sizeof(FILE_RENAME_INFORMATION) + fri->FileNameLength, FileRenameInformation );
    ok( U(io).Status == STATUS_SUCCESS, "got io status %#x\n", U(io).Status );
    ok( res == STATUS_SUCCESS, "got status %x\n", res );
    ok( GetFileAttributesW( oldpath ) != INVALID_FILE_ATTRIBUTES, "file should exist\n" );

    CloseHandle( handle );
    HeapFree( GetProcessHeap(), 0, fri );
    delete_object( oldpath );
}

static void test_file_link_information(void)
{
    static const WCHAR foo_txtW[] = {'\\','f','o','o','.','t','x','t',0};
    static const WCHAR fooW[] = {'f','o','o',0};
    WCHAR tmp_path[MAX_PATH], oldpath[MAX_PATH + 16], newpath[MAX_PATH + 16], *filename, *p;
    FILE_LINK_INFORMATION *fli;
    FILE_NAME_INFORMATION *fni;
    BOOL success, fileDeleted;
    UNICODE_STRING name_str;
    HANDLE handle, handle2;
    IO_STATUS_BLOCK io;
    NTSTATUS res;

    GetTempPathW( MAX_PATH, tmp_path );

    /* oldpath is a file, newpath doesn't exist */
    res = GetTempFileNameW( tmp_path, fooW, 0, oldpath );
    ok( res != 0, "failed to create temp file\n" );
    handle = CreateFileW( oldpath, GENERIC_WRITE | DELETE, 0, NULL, CREATE_ALWAYS, 0, 0 );
    ok( handle != INVALID_HANDLE_VALUE, "CreateFileW failed\n" );

    res = GetTempFileNameW( tmp_path, fooW, 0, newpath );
    ok( res != 0, "failed to create temp file\n" );
    pRtlDosPathNameToNtPathName_U( newpath, &name_str, NULL, NULL );
    DeleteFileW( newpath );
    fli = HeapAlloc( GetProcessHeap(), 0, sizeof(FILE_LINK_INFORMATION) + name_str.Length );
    fli->ReplaceIfExists = FALSE;
    fli->RootDirectory = NULL;
    fli->FileNameLength = name_str.Length;
    memcpy( fli->FileName, name_str.Buffer, name_str.Length );
    pRtlFreeUnicodeString( &name_str );

    U(io).Status = 0xdeadbeef;
    res = pNtSetInformationFile( handle, &io, fli, sizeof(FILE_LINK_INFORMATION) + fli->FileNameLength, FileLinkInformation );
    ok( U(io).Status == STATUS_SUCCESS, "io.Status expected STATUS_SUCCESS, got %x\n", U(io).Status );
    ok( res == STATUS_SUCCESS, "res expected STATUS_SUCCESS, got %x\n", res );
    fileDeleted = GetFileAttributesW( oldpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );
    fileDeleted = GetFileAttributesW( newpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );

    fni = HeapAlloc( GetProcessHeap(), 0, sizeof(FILE_NAME_INFORMATION) + MAX_PATH * sizeof(WCHAR) );
    res = pNtQueryInformationFile( handle, &io, fni, sizeof(FILE_NAME_INFORMATION) + MAX_PATH * sizeof(WCHAR), FileNameInformation );
    ok( res == STATUS_SUCCESS, "res expected STATUS_SUCCESS, got %x\n", res );
    fni->FileName[ fni->FileNameLength / sizeof(WCHAR) ] = 0;
    ok( !lstrcmpiW(fni->FileName, oldpath + 2), "FileName expected %s, got %s\n",
        wine_dbgstr_w(oldpath + 2), wine_dbgstr_w(fni->FileName) );
    HeapFree( GetProcessHeap(), 0, fni );

    CloseHandle( handle );
    HeapFree( GetProcessHeap(), 0, fli );
    delete_object( oldpath );
    delete_object( newpath );

    /* oldpath is a file, newpath is a file, ReplaceIfExists = FALSE */
    res = GetTempFileNameW( tmp_path, fooW, 0, oldpath );
    ok( res != 0, "failed to create temp file\n" );
    handle = CreateFileW( oldpath, GENERIC_WRITE | DELETE, 0, NULL, CREATE_ALWAYS, 0, 0 );
    ok( handle != INVALID_HANDLE_VALUE, "CreateFileW failed\n" );

    res = GetTempFileNameW( tmp_path, fooW, 0, newpath );
    ok( res != 0, "failed to create temp file\n" );
    pRtlDosPathNameToNtPathName_U( newpath, &name_str, NULL, NULL );
    fli = HeapAlloc( GetProcessHeap(), 0, sizeof(FILE_LINK_INFORMATION) + name_str.Length );
    fli->ReplaceIfExists = FALSE;
    fli->RootDirectory = NULL;
    fli->FileNameLength = name_str.Length;
    memcpy( fli->FileName, name_str.Buffer, name_str.Length );
    pRtlFreeUnicodeString( &name_str );

    U(io).Status = 0xdeadbeef;
    res = pNtSetInformationFile( handle, &io, fli, sizeof(FILE_LINK_INFORMATION) + fli->FileNameLength, FileLinkInformation );
    todo_wine ok( U(io).Status == 0xdeadbeef, "io.Status expected 0xdeadbeef, got %x\n", U(io).Status );
    ok( res == STATUS_OBJECT_NAME_COLLISION, "res expected STATUS_OBJECT_NAME_COLLISION, got %x\n", res );
    fileDeleted = GetFileAttributesW( oldpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );
    fileDeleted = GetFileAttributesW( newpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );

    CloseHandle( handle );
    HeapFree( GetProcessHeap(), 0, fli );
    delete_object( oldpath );
    delete_object( newpath );

    /* oldpath is a file, newpath is a file, ReplaceIfExists = TRUE */
    res = GetTempFileNameW( tmp_path, fooW, 0, oldpath );
    ok( res != 0, "failed to create temp file\n" );
    handle = CreateFileW( oldpath, GENERIC_WRITE | DELETE, 0, NULL, CREATE_ALWAYS, 0, 0 );
    ok( handle != INVALID_HANDLE_VALUE, "CreateFileW failed\n" );

    res = GetTempFileNameW( tmp_path, fooW, 0, newpath );
    ok( res != 0, "failed to create temp file\n" );
    pRtlDosPathNameToNtPathName_U( newpath, &name_str, NULL, NULL );
    fli = HeapAlloc( GetProcessHeap(), 0, sizeof(FILE_LINK_INFORMATION) + name_str.Length );
    fli->ReplaceIfExists = TRUE;
    fli->RootDirectory = NULL;
    fli->FileNameLength = name_str.Length;
    memcpy( fli->FileName, name_str.Buffer, name_str.Length );
    pRtlFreeUnicodeString( &name_str );

    U(io).Status = 0xdeadbeef;
    res = pNtSetInformationFile( handle, &io, fli, sizeof(FILE_LINK_INFORMATION) + fli->FileNameLength, FileLinkInformation );
    ok( U(io).Status == STATUS_SUCCESS, "io.Status expected STATUS_SUCCESS, got %x\n", U(io).Status );
    ok( res == STATUS_SUCCESS, "res expected STATUS_SUCCESS, got %x\n", res );
    fileDeleted = GetFileAttributesW( oldpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );
    fileDeleted = GetFileAttributesW( newpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );

    CloseHandle( handle );
    HeapFree( GetProcessHeap(), 0, fli );
    delete_object( oldpath );
    delete_object( newpath );

    /* oldpath is a file, newpath is a file, ReplaceIfExists = FALSE, target file opened */
    res = GetTempFileNameW( tmp_path, fooW, 0, oldpath );
    ok( res != 0, "failed to create temp file\n" );
    handle = CreateFileW( oldpath, GENERIC_WRITE | DELETE, 0, NULL, CREATE_ALWAYS, 0, 0 );
    ok( handle != INVALID_HANDLE_VALUE, "CreateFileW failed\n" );

    res = GetTempFileNameW( tmp_path, fooW, 0, newpath );
    ok( res != 0, "failed to create temp file\n" );
    handle2 = CreateFileW( newpath, GENERIC_WRITE | DELETE, 0, NULL, CREATE_ALWAYS, 0, 0 );
    ok( handle2 != INVALID_HANDLE_VALUE, "CreateFileW failed\n" );

    pRtlDosPathNameToNtPathName_U( newpath, &name_str, NULL, NULL );
    fli = HeapAlloc( GetProcessHeap(), 0, sizeof(FILE_LINK_INFORMATION) + name_str.Length );
    fli->ReplaceIfExists = FALSE;
    fli->RootDirectory = NULL;
    fli->FileNameLength = name_str.Length;
    memcpy( fli->FileName, name_str.Buffer, name_str.Length );
    pRtlFreeUnicodeString( &name_str );

    U(io).Status = 0xdeadbeef;
    res = pNtSetInformationFile( handle, &io, fli, sizeof(FILE_LINK_INFORMATION) + fli->FileNameLength, FileLinkInformation );
    todo_wine ok( U(io).Status == 0xdeadbeef, "io.Status expected 0xdeadbeef, got %x\n", U(io).Status );
    ok( res == STATUS_OBJECT_NAME_COLLISION, "res expected STATUS_OBJECT_NAME_COLLISION, got %x\n", res );
    fileDeleted = GetFileAttributesW( oldpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );
    fileDeleted = GetFileAttributesW( newpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );

    CloseHandle( handle );
    CloseHandle( handle2 );
    HeapFree( GetProcessHeap(), 0, fli );
    delete_object( oldpath );
    delete_object( newpath );

    /* oldpath is a file, newpath is a file, ReplaceIfExists = TRUE, target file opened */
    res = GetTempFileNameW( tmp_path, fooW, 0, oldpath );
    ok( res != 0, "failed to create temp file\n" );
    handle = CreateFileW( oldpath, GENERIC_WRITE | DELETE, 0, NULL, CREATE_ALWAYS, 0, 0 );
    ok( handle != INVALID_HANDLE_VALUE, "CreateFileW failed\n" );

    res = GetTempFileNameW( tmp_path, fooW, 0, newpath );
    ok( res != 0, "failed to create temp file\n" );
    handle2 = CreateFileW( newpath, GENERIC_WRITE | DELETE, 0, NULL, CREATE_ALWAYS, 0, 0 );
    ok( handle2 != INVALID_HANDLE_VALUE, "CreateFileW failed\n" );

    pRtlDosPathNameToNtPathName_U( newpath, &name_str, NULL, NULL );
    fli = HeapAlloc( GetProcessHeap(), 0, sizeof(FILE_LINK_INFORMATION) + name_str.Length );
    fli->ReplaceIfExists = TRUE;
    fli->RootDirectory = NULL;
    fli->FileNameLength = name_str.Length;
    memcpy( fli->FileName, name_str.Buffer, name_str.Length );
    pRtlFreeUnicodeString( &name_str );

    U(io).Status = 0xdeadbeef;
    res = pNtSetInformationFile( handle, &io, fli, sizeof(FILE_LINK_INFORMATION) + fli->FileNameLength, FileLinkInformation );
    todo_wine ok( U(io).Status == 0xdeadbeef, "io.Status expected 0xdeadbeef, got %x\n", U(io).Status );
    ok( res == STATUS_ACCESS_DENIED, "res expected STATUS_ACCESS_DENIED, got %x\n", res );
    fileDeleted = GetFileAttributesW( oldpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );
    fileDeleted = GetFileAttributesW( newpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );

    CloseHandle( handle );
    CloseHandle( handle2 );
    HeapFree( GetProcessHeap(), 0, fli );
    delete_object( oldpath );
    delete_object( newpath );

    /* oldpath is a directory, newpath doesn't exist, ReplaceIfExists = FALSE */
    res = GetTempFileNameW( tmp_path, fooW, 0, oldpath );
    ok( res != 0, "failed to create temp file\n" );
    DeleteFileW( oldpath );
    success = CreateDirectoryW( oldpath, NULL );
    ok( success != 0, "failed to create temp directory\n" );
    handle = CreateFileW( oldpath, GENERIC_WRITE | DELETE, 0, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0 );
    ok( handle != INVALID_HANDLE_VALUE, "CreateFileW failed\n" );

    res = GetTempFileNameW( tmp_path, fooW, 0, newpath );
    ok( res != 0, "failed to create temp file\n" );
    pRtlDosPathNameToNtPathName_U( newpath, &name_str, NULL, NULL );
    DeleteFileW( newpath );
    fli = HeapAlloc( GetProcessHeap(), 0, sizeof(FILE_LINK_INFORMATION) + name_str.Length );
    fli->ReplaceIfExists = FALSE;
    fli->RootDirectory = NULL;
    fli->FileNameLength = name_str.Length;
    memcpy( fli->FileName, name_str.Buffer, name_str.Length );
    pRtlFreeUnicodeString( &name_str );

    U(io).Status = 0xdeadbeef;
    res = pNtSetInformationFile( handle, &io, fli, sizeof(FILE_LINK_INFORMATION) + fli->FileNameLength, FileLinkInformation );
    todo_wine ok( U(io).Status == 0xdeadbeef , "io.Status expected 0xdeadbeef, got %x\n", U(io).Status );
    ok( res == STATUS_FILE_IS_A_DIRECTORY, "res expected STATUS_FILE_IS_A_DIRECTORY, got %x\n", res );
    fileDeleted = GetFileAttributesW( oldpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );
    fileDeleted = GetFileAttributesW( newpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( fileDeleted, "file should not exist\n" );

    fni = HeapAlloc( GetProcessHeap(), 0, sizeof(FILE_NAME_INFORMATION) + MAX_PATH * sizeof(WCHAR) );
    res = pNtQueryInformationFile( handle, &io, fni, sizeof(FILE_NAME_INFORMATION) + MAX_PATH * sizeof(WCHAR), FileNameInformation );
    ok( res == STATUS_SUCCESS, "res expected STATUS_SUCCESS, got %x\n", res );
    fni->FileName[ fni->FileNameLength / sizeof(WCHAR) ] = 0;
    ok( !lstrcmpiW(fni->FileName, oldpath + 2), "FileName expected %s, got %s\n",
        wine_dbgstr_w(oldpath + 2), wine_dbgstr_w(fni->FileName) );
    HeapFree( GetProcessHeap(), 0, fni );

    CloseHandle( handle );
    HeapFree( GetProcessHeap(), 0, fli );
    delete_object( oldpath );
    delete_object( newpath );

    /* oldpath is a directory (but child object opened), newpath doesn't exist, ReplaceIfExists = FALSE */
    res = GetTempFileNameW( tmp_path, fooW, 0, oldpath );
    ok( res != 0, "failed to create temp file\n" );
    DeleteFileW( oldpath );
    success = CreateDirectoryW( oldpath, NULL );
    ok( success != 0, "failed to create temp directory\n" );
    handle = CreateFileW( oldpath, GENERIC_WRITE | DELETE, 0, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0 );
    ok( handle != INVALID_HANDLE_VALUE, "CreateFileW failed\n" );

    lstrcpyW( newpath, oldpath );
    lstrcatW( newpath, foo_txtW );
    handle2 = CreateFileW( newpath, GENERIC_WRITE | DELETE, 0, NULL, CREATE_ALWAYS, FILE_FLAG_DELETE_ON_CLOSE, 0 );
    ok( handle2 != INVALID_HANDLE_VALUE, "CreateFileW failed\n" );

    res = GetTempFileNameW( tmp_path, fooW, 0, newpath );
    ok( res != 0, "failed to create temp file\n" );
    pRtlDosPathNameToNtPathName_U( newpath, &name_str, NULL, NULL );
    DeleteFileW( newpath );
    fli = HeapAlloc( GetProcessHeap(), 0, sizeof(FILE_LINK_INFORMATION) + name_str.Length );
    fli->ReplaceIfExists = FALSE;
    fli->RootDirectory = NULL;
    fli->FileNameLength = name_str.Length;
    memcpy( fli->FileName, name_str.Buffer, name_str.Length );
    pRtlFreeUnicodeString( &name_str );

    U(io).Status = 0xdeadbeef;
    res = pNtSetInformationFile( handle, &io, fli, sizeof(FILE_LINK_INFORMATION) + fli->FileNameLength, FileLinkInformation );
    todo_wine ok( U(io).Status == 0xdeadbeef, "io.Status expected 0xdeadbeef, got %x\n", U(io).Status );
    ok( res == STATUS_FILE_IS_A_DIRECTORY, "res expected STATUS_FILE_IS_A_DIRECTORY, got %x\n", res );
    fileDeleted = GetFileAttributesW( oldpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );
    fileDeleted = GetFileAttributesW( newpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( fileDeleted, "file should not exist\n" );

    CloseHandle( handle );
    CloseHandle( handle2 );
    HeapFree( GetProcessHeap(), 0, fli );
    delete_object( oldpath );
    delete_object( newpath );

    /* oldpath is a directory, newpath is a file, ReplaceIfExists = FALSE */
    res = GetTempFileNameW( tmp_path, fooW, 0, oldpath );
    ok( res != 0, "failed to create temp file\n" );
    DeleteFileW( oldpath );
    success = CreateDirectoryW( oldpath, NULL );
    ok( success != 0, "failed to create temp directory\n" );
    handle = CreateFileW( oldpath, GENERIC_WRITE | DELETE, 0, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0 );
    ok( handle != INVALID_HANDLE_VALUE, "CreateFileW failed\n" );

    res = GetTempFileNameW( tmp_path, fooW, 0, newpath );
    ok( res != 0, "failed to create temp file\n" );
    pRtlDosPathNameToNtPathName_U( newpath, &name_str, NULL, NULL );
    fli = HeapAlloc( GetProcessHeap(), 0, sizeof(FILE_LINK_INFORMATION) + name_str.Length );
    fli->ReplaceIfExists = FALSE;
    fli->RootDirectory = NULL;
    fli->FileNameLength = name_str.Length;
    memcpy( fli->FileName, name_str.Buffer, name_str.Length );
    pRtlFreeUnicodeString( &name_str );

    U(io).Status = 0xdeadbeef;
    res = pNtSetInformationFile( handle, &io, fli, sizeof(FILE_LINK_INFORMATION) + fli->FileNameLength, FileLinkInformation );
    todo_wine ok( U(io).Status == 0xdeadbeef, "io.Status expected 0xdeadbeef, got %x\n", U(io).Status );
    ok( res == STATUS_OBJECT_NAME_COLLISION || res == STATUS_FILE_IS_A_DIRECTORY /* > Win XP */,
        "res expected STATUS_OBJECT_NAME_COLLISION or STATUS_FILE_IS_A_DIRECTORY, got %x\n", res );
    fileDeleted = GetFileAttributesW( oldpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );
    fileDeleted = GetFileAttributesW( newpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );

    CloseHandle( handle );
    HeapFree( GetProcessHeap(), 0, fli );
    delete_object( oldpath );
    delete_object( newpath );

    /* oldpath is a directory, newpath is a file, ReplaceIfExists = FALSE, target file opened */
    res = GetTempFileNameW( tmp_path, fooW, 0, oldpath );
    ok( res != 0, "failed to create temp file\n" );
    DeleteFileW( oldpath );
    success = CreateDirectoryW( oldpath, NULL );
    ok( success != 0, "failed to create temp directory\n" );
    handle = CreateFileW( oldpath, GENERIC_WRITE | DELETE, 0, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0 );
    ok( handle != INVALID_HANDLE_VALUE, "CreateFileW failed\n" );

    res = GetTempFileNameW( tmp_path, fooW, 0, newpath );
    ok( res != 0, "failed to create temp file\n" );
    handle2 = CreateFileW( newpath, GENERIC_WRITE | DELETE, 0, NULL, CREATE_ALWAYS, 0, 0 );
    ok( handle2 != INVALID_HANDLE_VALUE, "CreateFileW failed\n" );

    pRtlDosPathNameToNtPathName_U( newpath, &name_str, NULL, NULL );
    fli = HeapAlloc( GetProcessHeap(), 0, sizeof(FILE_LINK_INFORMATION) + name_str.Length );
    fli->ReplaceIfExists = FALSE;
    fli->RootDirectory = NULL;
    fli->FileNameLength = name_str.Length;
    memcpy( fli->FileName, name_str.Buffer, name_str.Length );
    pRtlFreeUnicodeString( &name_str );

    U(io).Status = 0xdeadbeef;
    res = pNtSetInformationFile( handle, &io, fli, sizeof(FILE_LINK_INFORMATION) + fli->FileNameLength, FileLinkInformation );
    todo_wine ok( U(io).Status == 0xdeadbeef, "io.Status expected 0xdeadbeef, got %x\n", U(io).Status );
    ok( res == STATUS_OBJECT_NAME_COLLISION || res == STATUS_FILE_IS_A_DIRECTORY /* > Win XP */,
        "res expected STATUS_OBJECT_NAME_COLLISION or STATUS_FILE_IS_A_DIRECTORY, got %x\n", res );
    fileDeleted = GetFileAttributesW( oldpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );
    fileDeleted = GetFileAttributesW( newpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );

    CloseHandle( handle );
    CloseHandle( handle2 );
    HeapFree( GetProcessHeap(), 0, fli );
    delete_object( oldpath );
    delete_object( newpath );

    /* oldpath is a directory, newpath is a file, ReplaceIfExists = TRUE */
    res = GetTempFileNameW( tmp_path, fooW, 0, oldpath );
    ok( res != 0, "failed to create temp file\n" );
    DeleteFileW( oldpath );
    success = CreateDirectoryW( oldpath, NULL );
    ok( success != 0, "failed to create temp directory\n" );
    handle = CreateFileW( oldpath, GENERIC_WRITE | DELETE, 0, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0 );
    ok( handle != INVALID_HANDLE_VALUE, "CreateFileW failed\n" );

    res = GetTempFileNameW( tmp_path, fooW, 0, newpath );
    ok( res != 0, "failed to create temp file\n" );
    pRtlDosPathNameToNtPathName_U( newpath, &name_str, NULL, NULL );
    fli = HeapAlloc( GetProcessHeap(), 0, sizeof(FILE_LINK_INFORMATION) + name_str.Length );
    fli->ReplaceIfExists = TRUE;
    fli->RootDirectory = NULL;
    fli->FileNameLength = name_str.Length;
    memcpy( fli->FileName, name_str.Buffer, name_str.Length );
    pRtlFreeUnicodeString( &name_str );

    U(io).Status = 0xdeadbeef;
    res = pNtSetInformationFile( handle, &io, fli, sizeof(FILE_LINK_INFORMATION) + fli->FileNameLength, FileLinkInformation );
    todo_wine ok( U(io).Status == 0xdeadbeef, "io.Status expected 0xdeadbeef, got %x\n", U(io).Status );
    ok( res == STATUS_FILE_IS_A_DIRECTORY, "res expected STATUS_FILE_IS_A_DIRECTORY, got %x\n", res );
    fileDeleted = GetFileAttributesW( oldpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );
    fileDeleted = GetFileAttributesW( newpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );

    CloseHandle( handle );
    HeapFree( GetProcessHeap(), 0, fli );
    delete_object( oldpath );
    delete_object( newpath );

    /* oldpath is a directory, newpath is a file, ReplaceIfExists = TRUE, target file opened */
    res = GetTempFileNameW( tmp_path, fooW, 0, oldpath );
    ok( res != 0, "failed to create temp file\n" );
    DeleteFileW( oldpath );
    success = CreateDirectoryW( oldpath, NULL );
    ok( success != 0, "failed to create temp directory\n" );
    handle = CreateFileW( oldpath, GENERIC_WRITE | DELETE, 0, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0 );
    ok( handle != INVALID_HANDLE_VALUE, "CreateFileW failed\n" );

    res = GetTempFileNameW( tmp_path, fooW, 0, newpath );
    ok( res != 0, "failed to create temp file\n" );
    handle2 = CreateFileW( newpath, GENERIC_WRITE | DELETE, 0, NULL, CREATE_ALWAYS, 0, 0 );
    ok( handle2 != INVALID_HANDLE_VALUE, "CreateFileW failed\n" );

    pRtlDosPathNameToNtPathName_U( newpath, &name_str, NULL, NULL );
    fli = HeapAlloc( GetProcessHeap(), 0, sizeof(FILE_LINK_INFORMATION) + name_str.Length );
    fli->ReplaceIfExists = TRUE;
    fli->RootDirectory = NULL;
    fli->FileNameLength = name_str.Length;
    memcpy( fli->FileName, name_str.Buffer, name_str.Length );
    pRtlFreeUnicodeString( &name_str );

    U(io).Status = 0xdeadbeef;
    res = pNtSetInformationFile( handle, &io, fli, sizeof(FILE_LINK_INFORMATION) + fli->FileNameLength, FileLinkInformation );
    todo_wine ok( U(io).Status == 0xdeadbeef, "io.Status expected 0xdeadbeef, got %x\n", U(io).Status );
    ok( res == STATUS_FILE_IS_A_DIRECTORY, "res expected STATUS_FILE_IS_A_DIRECTORY, got %x\n", res );
    fileDeleted = GetFileAttributesW( oldpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );
    fileDeleted = GetFileAttributesW( newpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );

    CloseHandle( handle );
    CloseHandle( handle2 );
    HeapFree( GetProcessHeap(), 0, fli );
    delete_object( oldpath );
    delete_object( newpath );

    /* oldpath is a directory, newpath is a directory, ReplaceIfExists = FALSE */
    res = GetTempFileNameW( tmp_path, fooW, 0, oldpath );
    ok( res != 0, "failed to create temp file\n" );
    DeleteFileW( oldpath );
    success = CreateDirectoryW( oldpath, NULL );
    ok( success != 0, "failed to create temp directory\n" );
    handle = CreateFileW( oldpath, GENERIC_WRITE | DELETE, 0, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0 );
    ok( handle != INVALID_HANDLE_VALUE, "CreateFileW failed\n" );

    res = GetTempFileNameW( tmp_path, fooW, 0, newpath );
    ok( res != 0, "failed to create temp file\n" );
    DeleteFileW( newpath );
    success = CreateDirectoryW( newpath, NULL );
    ok( success != 0, "failed to create temp directory\n" );
    pRtlDosPathNameToNtPathName_U( newpath, &name_str, NULL, NULL );
    fli = HeapAlloc( GetProcessHeap(), 0, sizeof(FILE_LINK_INFORMATION) + name_str.Length );
    fli->ReplaceIfExists = FALSE;
    fli->RootDirectory = NULL;
    fli->FileNameLength = name_str.Length;
    memcpy( fli->FileName, name_str.Buffer, name_str.Length );
    pRtlFreeUnicodeString( &name_str );

    U(io).Status = 0xdeadbeef;
    res = pNtSetInformationFile( handle, &io, fli, sizeof(FILE_LINK_INFORMATION) + fli->FileNameLength, FileLinkInformation );
    todo_wine ok( U(io).Status == 0xdeadbeef, "io.Status expected 0xdeadbeef, got %x\n", U(io).Status );
    ok( res == STATUS_OBJECT_NAME_COLLISION || res == STATUS_FILE_IS_A_DIRECTORY /* > Win XP */,
        "res expected STATUS_OBJECT_NAME_COLLISION or STATUS_FILE_IS_A_DIRECTORY, got %x\n", res );
    fileDeleted = GetFileAttributesW( oldpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );
    fileDeleted = GetFileAttributesW( newpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );

    CloseHandle( handle );
    HeapFree( GetProcessHeap(), 0, fli );
    delete_object( oldpath );
    delete_object( newpath );

    /* oldpath is a directory, newpath is a directory, ReplaceIfExists = TRUE */
    res = GetTempFileNameW( tmp_path, fooW, 0, oldpath );
    ok( res != 0, "failed to create temp file\n" );
    DeleteFileW( oldpath );
    success = CreateDirectoryW( oldpath, NULL );
    ok( success != 0, "failed to create temp directory\n" );
    handle = CreateFileW( oldpath, GENERIC_WRITE | DELETE, 0, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0 );
    ok( handle != INVALID_HANDLE_VALUE, "CreateFileW failed\n" );

    res = GetTempFileNameW( tmp_path, fooW, 0, newpath );
    ok( res != 0, "failed to create temp file\n" );
    DeleteFileW( newpath );
    success = CreateDirectoryW( newpath, NULL );
    ok( success != 0, "failed to create temp directory\n" );
    pRtlDosPathNameToNtPathName_U( newpath, &name_str, NULL, NULL );
    fli = HeapAlloc( GetProcessHeap(), 0, sizeof(FILE_LINK_INFORMATION) + name_str.Length );
    fli->ReplaceIfExists = TRUE;
    fli->RootDirectory = NULL;
    fli->FileNameLength = name_str.Length;
    memcpy( fli->FileName, name_str.Buffer, name_str.Length );
    pRtlFreeUnicodeString( &name_str );

    U(io).Status = 0xdeadbeef;
    res = pNtSetInformationFile( handle, &io, fli, sizeof(FILE_LINK_INFORMATION) + fli->FileNameLength, FileLinkInformation );
    todo_wine ok( U(io).Status == 0xdeadbeef, "io.Status expected 0xdeadbeef, got %x\n", U(io).Status );
    ok( res == STATUS_FILE_IS_A_DIRECTORY, "res expected STATUS_FILE_IS_A_DIRECTORY, got %x\n", res );
    fileDeleted = GetFileAttributesW( oldpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );
    fileDeleted = GetFileAttributesW( newpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );

    CloseHandle( handle );
    HeapFree( GetProcessHeap(), 0, fli );
    delete_object( oldpath );
    delete_object( newpath );

    /* oldpath is a directory, newpath is a directory, ReplaceIfExists = TRUE, target file opened */
    res = GetTempFileNameW( tmp_path, fooW, 0, oldpath );
    ok( res != 0, "failed to create temp file\n" );
    DeleteFileW( oldpath );
    success = CreateDirectoryW( oldpath, NULL );
    ok( success != 0, "failed to create temp directory\n" );
    handle = CreateFileW( oldpath, GENERIC_WRITE | DELETE, 0, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0 );
    ok( handle != INVALID_HANDLE_VALUE, "CreateFileW failed\n" );

    res = GetTempFileNameW( tmp_path, fooW, 0, newpath );
    ok( res != 0, "failed to create temp file\n" );
    DeleteFileW( newpath );
    success = CreateDirectoryW( newpath, NULL );
    ok( success != 0, "failed to create temp directory\n" );
    handle2 = CreateFileW( newpath, GENERIC_WRITE | DELETE, 0, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0 );
    ok( handle2 != INVALID_HANDLE_VALUE, "CreateFileW failed\n" );

    pRtlDosPathNameToNtPathName_U( newpath, &name_str, NULL, NULL );
    fli = HeapAlloc( GetProcessHeap(), 0, sizeof(FILE_LINK_INFORMATION) + name_str.Length );
    fli->ReplaceIfExists = TRUE;
    fli->RootDirectory = NULL;
    fli->FileNameLength = name_str.Length;
    memcpy( fli->FileName, name_str.Buffer, name_str.Length );
    pRtlFreeUnicodeString( &name_str );

    U(io).Status = 0xdeadbeef;
    res = pNtSetInformationFile( handle, &io, fli, sizeof(FILE_LINK_INFORMATION) + fli->FileNameLength, FileLinkInformation );
    todo_wine ok( U(io).Status == 0xdeadbeef, "io.Status expected 0xdeadbeef, got %x\n", U(io).Status );
    ok( res == STATUS_FILE_IS_A_DIRECTORY, "res expected STATUS_FILE_IS_A_DIRECTORY, got %x\n", res );
    fileDeleted = GetFileAttributesW( oldpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );
    fileDeleted = GetFileAttributesW( newpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );

    CloseHandle( handle );
    CloseHandle( handle2 );
    HeapFree( GetProcessHeap(), 0, fli );
    delete_object( oldpath );
    delete_object( newpath );

    /* oldpath is a file, newpath is a directory, ReplaceIfExists = FALSE */
    res = GetTempFileNameW( tmp_path, fooW, 0, oldpath );
    ok( res != 0, "failed to create temp file\n" );
    handle = CreateFileW( oldpath, GENERIC_WRITE | DELETE, 0, NULL, CREATE_ALWAYS, 0, 0 );
    ok( handle != INVALID_HANDLE_VALUE, "CreateFileW failed\n" );

    res = GetTempFileNameW( tmp_path, fooW, 0, newpath );
    ok( res != 0, "failed to create temp file\n" );
    DeleteFileW( newpath );
    success = CreateDirectoryW( newpath, NULL );
    ok( success != 0, "failed to create temp directory\n" );
    pRtlDosPathNameToNtPathName_U( newpath, &name_str, NULL, NULL );
    fli = HeapAlloc( GetProcessHeap(), 0, sizeof(FILE_LINK_INFORMATION) + name_str.Length );
    fli->ReplaceIfExists = FALSE;
    fli->RootDirectory = NULL;
    fli->FileNameLength = name_str.Length;
    memcpy( fli->FileName, name_str.Buffer, name_str.Length );
    pRtlFreeUnicodeString( &name_str );

    U(io).Status = 0xdeadbeef;
    res = pNtSetInformationFile( handle, &io, fli, sizeof(FILE_LINK_INFORMATION) + fli->FileNameLength, FileLinkInformation );
    todo_wine ok( U(io).Status == 0xdeadbeef, "io.Status expected 0xdeadbeef, got %x\n", U(io).Status );
    ok( res == STATUS_OBJECT_NAME_COLLISION, "res expected STATUS_OBJECT_NAME_COLLISION, got %x\n", res );
    fileDeleted = GetFileAttributesW( oldpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );
    fileDeleted = GetFileAttributesW( newpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );

    CloseHandle( handle );
    HeapFree( GetProcessHeap(), 0, fli );
    delete_object( oldpath );
    delete_object( newpath );

    /* oldpath is a file, newpath is a directory, ReplaceIfExists = TRUE */
    res = GetTempFileNameW( tmp_path, fooW, 0, oldpath );
    ok( res != 0, "failed to create temp file\n" );
    handle = CreateFileW( oldpath, GENERIC_WRITE | DELETE, 0, NULL, CREATE_ALWAYS, 0, 0 );
    ok( handle != INVALID_HANDLE_VALUE, "CreateFileW failed\n" );

    res = GetTempFileNameW( tmp_path, fooW, 0, newpath );
    ok( res != 0, "failed to create temp file\n" );
    DeleteFileW( newpath );
    success = CreateDirectoryW( newpath, NULL );
    ok( success != 0, "failed to create temp directory\n" );
    pRtlDosPathNameToNtPathName_U( newpath, &name_str, NULL, NULL );
    fli = HeapAlloc( GetProcessHeap(), 0, sizeof(FILE_LINK_INFORMATION) + name_str.Length );
    fli->ReplaceIfExists = TRUE;
    fli->RootDirectory = NULL;
    fli->FileNameLength = name_str.Length;
    memcpy( fli->FileName, name_str.Buffer, name_str.Length );
    pRtlFreeUnicodeString( &name_str );

    U(io).Status = 0xdeadbeef;
    res = pNtSetInformationFile( handle, &io, fli, sizeof(FILE_LINK_INFORMATION) + fli->FileNameLength, FileLinkInformation );
    todo_wine ok( U(io).Status == 0xdeadbeef, "io.Status expected 0xdeadbeef, got %x\n", U(io).Status );
    ok( res == STATUS_ACCESS_DENIED, "res expected STATUS_ACCESS_DENIED, got %x\n", res );
    fileDeleted = GetFileAttributesW( oldpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );
    fileDeleted = GetFileAttributesW( newpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );

    CloseHandle( handle );
    HeapFree( GetProcessHeap(), 0, fli );
    delete_object( oldpath );
    delete_object( newpath );

    /* oldpath is a file, newpath doesn't exist, test with RootDirectory != NULL */
    res = GetTempFileNameW( tmp_path, fooW, 0, oldpath );
    ok( res != 0, "failed to create temp file\n" );
    handle = CreateFileW( oldpath, GENERIC_WRITE | DELETE, 0, NULL, CREATE_ALWAYS, 0, 0 );
    ok( handle != INVALID_HANDLE_VALUE, "CreateFileW failed\n" );

    res = GetTempFileNameW( tmp_path, fooW, 0, newpath );
    ok( res != 0, "failed to create temp file\n" );
    DeleteFileW( newpath );
    for (filename = newpath, p = newpath; *p; p++)
        if (*p == '\\') filename = p + 1;
    handle2 = CreateFileW( tmp_path, 0, 0, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0 );
    ok( handle2 != INVALID_HANDLE_VALUE, "CreateFileW failed\n" );

    fli = HeapAlloc( GetProcessHeap(), 0, sizeof(FILE_LINK_INFORMATION) + lstrlenW(filename) * sizeof(WCHAR) );
    fli->ReplaceIfExists = FALSE;
    fli->RootDirectory = handle2;
    fli->FileNameLength = lstrlenW(filename) * sizeof(WCHAR);
    memcpy( fli->FileName, filename, fli->FileNameLength );

    U(io).Status = 0xdeadbeef;
    res = pNtSetInformationFile( handle, &io, fli, sizeof(FILE_LINK_INFORMATION) + fli->FileNameLength, FileLinkInformation );
    ok( U(io).Status == STATUS_SUCCESS, "io.Status expected STATUS_SUCCESS, got %x\n", U(io).Status );
    ok( res == STATUS_SUCCESS, "res expected STATUS_SUCCESS, got %x\n", res );
    fileDeleted = GetFileAttributesW( oldpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );
    fileDeleted = GetFileAttributesW( newpath ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "file should exist\n" );

    fni = HeapAlloc( GetProcessHeap(), 0, sizeof(FILE_NAME_INFORMATION) + MAX_PATH * sizeof(WCHAR) );
    res = pNtQueryInformationFile( handle, &io, fni, sizeof(FILE_NAME_INFORMATION) + MAX_PATH * sizeof(WCHAR), FileNameInformation );
    ok( res == STATUS_SUCCESS, "res expected STATUS_SUCCESS, got %x\n", res );
    fni->FileName[ fni->FileNameLength / sizeof(WCHAR) ] = 0;
    ok( !lstrcmpiW(fni->FileName, oldpath + 2), "FileName expected %s, got %s\n",
        wine_dbgstr_w(oldpath + 2), wine_dbgstr_w(fni->FileName) );
    HeapFree( GetProcessHeap(), 0, fni );

    CloseHandle( handle );
    CloseHandle( handle2 );
    HeapFree( GetProcessHeap(), 0, fli );
    delete_object( oldpath );
    delete_object( newpath );

    /* oldpath == newpath */
    res = GetTempFileNameW( tmp_path, fooW, 0, oldpath );
    ok( res != 0, "failed to create temp file\n" );
    handle = CreateFileW( oldpath, GENERIC_WRITE | DELETE, 0, NULL, CREATE_ALWAYS, 0, 0 );
    ok( handle != INVALID_HANDLE_VALUE, "CreateFileW failed\n" );

    pRtlDosPathNameToNtPathName_U( oldpath, &name_str, NULL, NULL );
    fli = HeapAlloc( GetProcessHeap(), 0, sizeof(FILE_RENAME_INFORMATION) + name_str.Length );
    fli->ReplaceIfExists = FALSE;
    fli->RootDirectory = NULL;
    fli->FileNameLength = name_str.Length;
    memcpy( fli->FileName, name_str.Buffer, name_str.Length );
    pRtlFreeUnicodeString( &name_str );

    U(io).Status = 0xdeadbeef;
    res = pNtSetInformationFile( handle, &io, fli, sizeof(FILE_LINK_INFORMATION) + fli->FileNameLength, FileLinkInformation );
    todo_wine ok( U(io).Status == 0xdeadbeef, "got io status %#x\n", U(io).Status );
    ok( res == STATUS_OBJECT_NAME_COLLISION, "got status %x\n", res );

    fli->ReplaceIfExists = TRUE;
    U(io).Status = 0xdeadbeef;
    res = pNtSetInformationFile( handle, &io, fli, sizeof(FILE_LINK_INFORMATION) + fli->FileNameLength, FileLinkInformation );
    ok( U(io).Status == STATUS_SUCCESS, "got io status %#x\n", U(io).Status );
    ok( res == STATUS_SUCCESS, "got status %x\n", res );
    ok( GetFileAttributesW( oldpath ) != INVALID_FILE_ATTRIBUTES, "file should exist\n" );

    CloseHandle( handle );
    HeapFree( GetProcessHeap(), 0, fli );
    delete_object( oldpath );
}

static void test_file_both_information(void)
{
    IO_STATUS_BLOCK io;
    FILE_BOTH_DIR_INFORMATION fbi;
    HANDLE h;
    int res;

    if (!(h = create_temp_file(0))) return;

    memset(&fbi, 0, sizeof(fbi));
    res = pNtQueryInformationFile(h, &io, &fbi, sizeof fbi, FileBothDirectoryInformation);
    ok ( res == STATUS_INVALID_INFO_CLASS || res == STATUS_NOT_IMPLEMENTED, "shouldn't be able to query FileBothDirectoryInformation, res %x\n", res);

    CloseHandle( h );
}

static NTSTATUS nt_get_file_attrs(const char *name, DWORD *attrs)
{
    WCHAR nameW[MAX_PATH];
    FILE_BASIC_INFORMATION info;
    UNICODE_STRING nt_name;
    OBJECT_ATTRIBUTES attr;
    NTSTATUS status;

    MultiByteToWideChar( CP_ACP, 0, name, -1, nameW, MAX_PATH );

    *attrs = INVALID_FILE_ATTRIBUTES;

    if (!pRtlDosPathNameToNtPathName_U( nameW, &nt_name, NULL, NULL ))
        return STATUS_UNSUCCESSFUL;

    attr.Length = sizeof(attr);
    attr.RootDirectory = 0;
    attr.Attributes = OBJ_CASE_INSENSITIVE;
    attr.ObjectName = &nt_name;
    attr.SecurityDescriptor = NULL;
    attr.SecurityQualityOfService = NULL;

    status = pNtQueryAttributesFile( &attr, &info );
    pRtlFreeUnicodeString( &nt_name );

    if (status == STATUS_SUCCESS)
        *attrs = info.FileAttributes;

    return status;
}

static void test_file_disposition_information(void)
{
    char tmp_path[MAX_PATH], buffer[MAX_PATH + 16];
    DWORD dirpos;
    HANDLE handle, handle2, handle3, mapping;
    NTSTATUS res;
    IO_STATUS_BLOCK io;
    FILE_DISPOSITION_INFORMATION fdi;
    BOOL fileDeleted;
    DWORD fdi2;
    void *ptr;

    GetTempPathA( MAX_PATH, tmp_path );

    /* tests for info struct size */
    GetTempFileNameA( tmp_path, "dis", 0, buffer );
    handle = CreateFileA( buffer, GENERIC_WRITE | DELETE, 0, NULL, CREATE_ALWAYS, 0, 0 );
    ok( handle != INVALID_HANDLE_VALUE, "failed to create temp file\n" );
    res = pNtSetInformationFile( handle, &io, &fdi, 0, FileDispositionInformation );
    todo_wine
    ok( res == STATUS_INFO_LENGTH_MISMATCH, "expected STATUS_INFO_LENGTH_MISMATCH, got %x\n", res );
    fdi2 = 0x100;
    res = pNtSetInformationFile( handle, &io, &fdi2, sizeof(fdi2), FileDispositionInformation );
    ok( res == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %x\n", res );
    CloseHandle( handle );
    fileDeleted = GetFileAttributesA( buffer ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "File shouldn't have been deleted\n" );
    DeleteFileA( buffer );

    /* cannot set disposition on file not opened with delete access */
    GetTempFileNameA( tmp_path, "dis", 0, buffer );
    handle = CreateFileA(buffer, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, 0);
    ok( handle != INVALID_HANDLE_VALUE, "failed to create temp file\n" );
    res = pNtQueryInformationFile( handle, &io, &fdi, sizeof fdi, FileDispositionInformation );
    ok( res == STATUS_INVALID_INFO_CLASS || res == STATUS_NOT_IMPLEMENTED, "Unexpected NtQueryInformationFile result (expected STATUS_INVALID_INFO_CLASS, got %x)\n", res );
    fdi.DoDeleteFile = TRUE;
    res = pNtSetInformationFile( handle, &io, &fdi, sizeof fdi, FileDispositionInformation );
    ok( res == STATUS_ACCESS_DENIED, "unexpected FileDispositionInformation result (expected STATUS_ACCESS_DENIED, got %x)\n", res );
    CloseHandle( handle );
    fileDeleted = GetFileAttributesA( buffer ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "File shouldn't have been deleted\n" );
    DeleteFileA( buffer );

    /* can set disposition on file opened with proper access */
    GetTempFileNameA( tmp_path, "dis", 0, buffer );
    handle = CreateFileA(buffer, GENERIC_WRITE | DELETE, 0, NULL, CREATE_ALWAYS, 0, 0);
    ok( handle != INVALID_HANDLE_VALUE, "failed to create temp file\n" );
    fdi.DoDeleteFile = TRUE;
    res = pNtSetInformationFile( handle, &io, &fdi, sizeof fdi, FileDispositionInformation );
    ok( res == STATUS_SUCCESS, "unexpected FileDispositionInformation result (expected STATUS_SUCCESS, got %x)\n", res );
    CloseHandle( handle );
    fileDeleted = GetFileAttributesA( buffer ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( fileDeleted, "File should have been deleted\n" );

    /* file exists until all handles to it get closed */
    GetTempFileNameA( tmp_path, "dis", 0, buffer );
    handle = CreateFileA(buffer, GENERIC_WRITE | DELETE, FILE_SHARE_DELETE, NULL, CREATE_ALWAYS, 0, 0);
    ok( handle != INVALID_HANDLE_VALUE, "failed to create temp file\n" );
    handle2 = CreateFileA(buffer, DELETE, FILE_SHARE_DELETE | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);
    ok( handle2 != INVALID_HANDLE_VALUE, "failed to open temp file\n" );
    fdi.DoDeleteFile = TRUE;
    res = pNtSetInformationFile( handle, &io, &fdi, sizeof fdi, FileDispositionInformation );
    ok( res == STATUS_SUCCESS, "unexpected FileDispositionInformation result (expected STATUS_SUCCESS, got %x)\n", res );
    res = nt_get_file_attrs( buffer, &fdi2 );
todo_wine
    ok( res == STATUS_DELETE_PENDING, "got %#x\n", res );
    /* can't open the deleted file */
    handle3 = CreateFileA(buffer, DELETE, FILE_SHARE_DELETE | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);
todo_wine
    ok( handle3 == INVALID_HANDLE_VALUE, "CreateFile should fail\n" );
if (handle3 != INVALID_HANDLE_VALUE)
    CloseHandle( handle3 );
todo_wine
    ok(GetLastError() == ERROR_ACCESS_DENIED, "got %u\n", GetLastError());
    /* can't open the deleted file (wrong sharing mode) */
    handle3 = CreateFileA(buffer, DELETE, 0, NULL, OPEN_EXISTING, 0, 0);
    ok( handle3 == INVALID_HANDLE_VALUE, "CreateFile should fail\n" );
todo_wine
    ok(GetLastError() == ERROR_ACCESS_DENIED, "got %u\n", GetLastError());
    CloseHandle( handle );
    fileDeleted = GetFileAttributesA( buffer ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "File shouldn't have been deleted\n" );
    CloseHandle( handle2 );
    fileDeleted = GetFileAttributesA( buffer ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( fileDeleted, "File should have been deleted\n" );

    /* file exists until all handles to it get closed */
    GetTempFileNameA( tmp_path, "dis", 0, buffer );
    handle = CreateFileA(buffer, GENERIC_WRITE | DELETE, FILE_SHARE_DELETE, NULL, CREATE_ALWAYS, FILE_FLAG_DELETE_ON_CLOSE, 0);
    ok( handle != INVALID_HANDLE_VALUE, "failed to create temp file\n" );
    /* can open the marked for delete file (proper sharing mode) */
    handle2 = CreateFileA(buffer, DELETE, FILE_SHARE_DELETE | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);
    ok( handle2 != INVALID_HANDLE_VALUE, "failed to open temp file\n" );
    res = nt_get_file_attrs( buffer, &fdi2 );
    ok( res == STATUS_SUCCESS, "got %#x\n", res );
    /* can't open the marked for delete file (wrong sharing mode) */
    handle3 = CreateFileA(buffer, DELETE, 0, NULL, OPEN_EXISTING, 0, 0);
    ok( handle3 == INVALID_HANDLE_VALUE, "CreateFile should fail\n" );
    ok(GetLastError() == ERROR_SHARING_VIOLATION, "got %u\n", GetLastError());
    CloseHandle( handle );
    fileDeleted = GetFileAttributesA( buffer ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "File shouldn't have been deleted\n" );
    CloseHandle( handle2 );
    fileDeleted = GetFileAttributesA( buffer ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( fileDeleted, "File should have been deleted\n" );

    /* cannot set disposition on readonly file */
    GetTempFileNameA( tmp_path, "dis", 0, buffer );
    DeleteFileA( buffer );
    handle = CreateFileA(buffer, GENERIC_WRITE | DELETE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_READONLY, 0);
    ok( handle != INVALID_HANDLE_VALUE, "failed to create temp file\n" );
    fdi.DoDeleteFile = TRUE;
    res = pNtSetInformationFile( handle, &io, &fdi, sizeof fdi, FileDispositionInformation );
    ok( res == STATUS_CANNOT_DELETE, "unexpected FileDispositionInformation result (expected STATUS_CANNOT_DELETE, got %x)\n", res );
    CloseHandle( handle );
    fileDeleted = GetFileAttributesA( buffer ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "File shouldn't have been deleted\n" );
    SetFileAttributesA( buffer, FILE_ATTRIBUTE_NORMAL );
    DeleteFileA( buffer );

    /* cannot set disposition on readonly file */
    GetTempFileNameA( tmp_path, "dis", 0, buffer );
    handle = CreateFileA(buffer, GENERIC_WRITE | DELETE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_READONLY, 0);
    ok( handle != INVALID_HANDLE_VALUE, "failed to create temp file\n" );
    fdi.DoDeleteFile = TRUE;
    res = pNtSetInformationFile( handle, &io, &fdi, sizeof fdi, FileDispositionInformation );
    todo_wine
    ok( res == STATUS_CANNOT_DELETE, "unexpected FileDispositionInformation result (expected STATUS_CANNOT_DELETE, got %x)\n", res );
    CloseHandle( handle );
    fileDeleted = GetFileAttributesA( buffer ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    todo_wine
    ok( !fileDeleted, "File shouldn't have been deleted\n" );
    SetFileAttributesA( buffer, FILE_ATTRIBUTE_NORMAL );
    DeleteFileA( buffer );

    /* can set disposition on file and then reset it */
    GetTempFileNameA( tmp_path, "dis", 0, buffer );
    handle = CreateFileA(buffer, GENERIC_WRITE | DELETE, 0, NULL, CREATE_ALWAYS, 0, 0);
    ok( handle != INVALID_HANDLE_VALUE, "failed to create temp file\n" );
    fdi.DoDeleteFile = TRUE;
    res = pNtSetInformationFile( handle, &io, &fdi, sizeof fdi, FileDispositionInformation );
    ok( res == STATUS_SUCCESS, "unexpected FileDispositionInformation result (expected STATUS_SUCCESS, got %x)\n", res );
    fdi.DoDeleteFile = FALSE;
    res = pNtSetInformationFile( handle, &io, &fdi, sizeof fdi, FileDispositionInformation );
    ok( res == STATUS_SUCCESS, "unexpected FileDispositionInformation result (expected STATUS_SUCCESS, got %x)\n", res );
    CloseHandle( handle );
    fileDeleted = GetFileAttributesA( buffer ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "File shouldn't have been deleted\n" );
    DeleteFileA( buffer );

    /* can't reset disposition if delete-on-close flag is specified */
    GetTempFileNameA( tmp_path, "dis", 0, buffer );
    handle = CreateFileA(buffer, GENERIC_WRITE | DELETE, 0, NULL, CREATE_ALWAYS, FILE_FLAG_DELETE_ON_CLOSE, 0);
    ok( handle != INVALID_HANDLE_VALUE, "failed to create temp file\n" );
    fdi.DoDeleteFile = FALSE;
    res = pNtSetInformationFile( handle, &io, &fdi, sizeof fdi, FileDispositionInformation );
    ok( res == STATUS_SUCCESS, "unexpected FileDispositionInformation result (expected STATUS_SUCCESS, got %x)\n", res );
    CloseHandle( handle );
    fileDeleted = GetFileAttributesA( buffer ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( fileDeleted, "File should have been deleted\n" );

    /* can't reset disposition on duplicated handle if delete-on-close flag is specified */
    GetTempFileNameA( tmp_path, "dis", 0, buffer );
    handle = CreateFileA(buffer, GENERIC_WRITE | DELETE, 0, NULL, CREATE_ALWAYS, FILE_FLAG_DELETE_ON_CLOSE, 0);
    ok( handle != INVALID_HANDLE_VALUE, "failed to create temp file\n" );
    ok( DuplicateHandle( GetCurrentProcess(), handle, GetCurrentProcess(), &handle2, 0, FALSE, DUPLICATE_SAME_ACCESS ), "DuplicateHandle failed\n" );
    CloseHandle( handle );
    fileDeleted = GetFileAttributesA( buffer ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "File shouldn't have been deleted\n" );
    fdi.DoDeleteFile = FALSE;
    res = pNtSetInformationFile( handle2, &io, &fdi, sizeof fdi, FileDispositionInformation );
    ok( res == STATUS_SUCCESS, "unexpected FileDispositionInformation result (expected STATUS_SUCCESS, got %x)\n", res );
    CloseHandle( handle2 );
    fileDeleted = GetFileAttributesA( buffer ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( fileDeleted, "File should have been deleted\n" );

    /* DeleteFile fails for wrong sharing mode */
    GetTempFileNameA( tmp_path, "dis", 0, buffer );
    handle = CreateFileA(buffer, GENERIC_WRITE | DELETE, 0, NULL, CREATE_ALWAYS, 0, 0);
    ok( handle != INVALID_HANDLE_VALUE, "failed to create temp file\n" );
    fileDeleted = DeleteFileA( buffer );
    ok( !fileDeleted, "File shouldn't have been deleted\n" );
    ok(GetLastError() == ERROR_SHARING_VIOLATION, "got %u\n", GetLastError());
    CloseHandle( handle );
    fileDeleted = GetFileAttributesA( buffer ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "File shouldn't have been deleted\n" );
    DeleteFileA( buffer );

    /* DeleteFile succeeds for proper sharing mode */
    GetTempFileNameA( tmp_path, "dis", 0, buffer );
    handle = CreateFileA(buffer, GENERIC_WRITE | DELETE, FILE_SHARE_DELETE, NULL, CREATE_ALWAYS, 0, 0);
    ok( handle != INVALID_HANDLE_VALUE, "failed to create temp file\n" );
    fileDeleted = DeleteFileA( buffer );
    ok( fileDeleted, "File should have been deleted\n" );
    fileDeleted = GetFileAttributesA( buffer ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "File shouldn't have been deleted\n" );
    res = nt_get_file_attrs( buffer, &fdi2 );
todo_wine
    ok( res == STATUS_DELETE_PENDING, "got %#x\n", res );
    /* can't open the deleted file */
    handle2 = CreateFileA(buffer, DELETE, FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, 0);
    ok( handle2 == INVALID_HANDLE_VALUE, "CreateFile should fail\n" );
todo_wine
    ok(GetLastError() == ERROR_ACCESS_DENIED, "got %u\n", GetLastError());
    CloseHandle( handle );
    fileDeleted = GetFileAttributesA( buffer ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( fileDeleted, "File should have been deleted\n" );

    /* can set disposition on a directory opened with proper access */
    GetTempFileNameA( tmp_path, "dis", 0, buffer );
    DeleteFileA( buffer );
    ok( CreateDirectoryA( buffer, NULL ), "CreateDirectory failed\n" );
    handle = CreateFileA(buffer, DELETE, 0, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
    ok( handle != INVALID_HANDLE_VALUE, "failed to open a directory\n" );
    fdi.DoDeleteFile = TRUE;
    res = pNtSetInformationFile( handle, &io, &fdi, sizeof fdi, FileDispositionInformation );
    ok( res == STATUS_SUCCESS, "unexpected FileDispositionInformation result (expected STATUS_SUCCESS, got %x)\n", res );
    CloseHandle( handle );
    fileDeleted = GetFileAttributesA( buffer ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( fileDeleted, "Directory should have been deleted\n" );

    /* RemoveDirectory fails for wrong sharing mode */
    GetTempFileNameA( tmp_path, "dis", 0, buffer );
    DeleteFileA( buffer );
    ok( CreateDirectoryA( buffer, NULL ), "CreateDirectory failed\n" );
    handle = CreateFileA(buffer, DELETE, 0, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
    ok( handle != INVALID_HANDLE_VALUE, "failed to open a directory\n" );
    fileDeleted = RemoveDirectoryA( buffer );
    ok( !fileDeleted, "Directory shouldn't have been deleted\n" );
    ok(GetLastError() == ERROR_SHARING_VIOLATION, "got %u\n", GetLastError());
    CloseHandle( handle );
    fileDeleted = GetFileAttributesA( buffer ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "Directory shouldn't have been deleted\n" );
    RemoveDirectoryA( buffer );

    /* RemoveDirectory succeeds for proper sharing mode */
    GetTempFileNameA( tmp_path, "dis", 0, buffer );
    DeleteFileA( buffer );
    ok( CreateDirectoryA( buffer, NULL ), "CreateDirectory failed\n" );
    handle = CreateFileA(buffer, DELETE, FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
    ok( handle != INVALID_HANDLE_VALUE, "failed to open a directory\n" );
    fileDeleted = RemoveDirectoryA( buffer );
    ok( fileDeleted, "Directory should have been deleted\n" );
    fileDeleted = GetFileAttributesA( buffer ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
todo_wine
    ok( !fileDeleted, "Directory shouldn't have been deleted\n" );
    res = nt_get_file_attrs( buffer, &fdi2 );
todo_wine
    ok( res == STATUS_DELETE_PENDING, "got %#x\n", res );
    /* can't open the deleted directory */
    handle2 = CreateFileA(buffer, DELETE, FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
    ok( handle2 == INVALID_HANDLE_VALUE, "CreateFile should fail\n" );
todo_wine
    ok(GetLastError() == ERROR_ACCESS_DENIED, "got %u\n", GetLastError());
    CloseHandle( handle );
    fileDeleted = GetFileAttributesA( buffer ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( fileDeleted, "Directory should have been deleted\n" );

    /* directory exists until all handles to it get closed */
    GetTempFileNameA( tmp_path, "dis", 0, buffer );
    DeleteFileA( buffer );
    ok( CreateDirectoryA( buffer, NULL ), "CreateDirectory failed\n" );
    handle = CreateFileA(buffer, DELETE, FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
    ok( handle != INVALID_HANDLE_VALUE, "failed to open a directory\n" );
    handle2 = CreateFileA(buffer, DELETE, FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
    ok( handle2 != INVALID_HANDLE_VALUE, "failed to open a directory\n" );
    fdi.DoDeleteFile = TRUE;
    res = pNtSetInformationFile( handle2, &io, &fdi, sizeof fdi, FileDispositionInformation );
    ok( res == STATUS_SUCCESS, "unexpected FileDispositionInformation result (expected STATUS_SUCCESS, got %x)\n", res );
    res = nt_get_file_attrs( buffer, &fdi2 );
todo_wine
    ok( res == STATUS_DELETE_PENDING, "got %#x\n", res );
    /* can't open the deleted directory */
    handle3 = CreateFileA(buffer, DELETE, FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
todo_wine
    ok( handle3 == INVALID_HANDLE_VALUE, "CreateFile should fail\n" );
if (handle3 != INVALID_HANDLE_VALUE)
    CloseHandle( handle3 );
todo_wine
    ok(GetLastError() == ERROR_ACCESS_DENIED, "got %u\n", GetLastError());
    /* can't open the deleted directory (wrong sharing mode) */
    handle3 = CreateFileA(buffer, DELETE, 0, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
    ok( handle3 == INVALID_HANDLE_VALUE, "CreateFile should fail\n" );
todo_wine
    ok(GetLastError() == ERROR_ACCESS_DENIED, "got %u\n", GetLastError());
    CloseHandle( handle2 );
    fileDeleted = GetFileAttributesA( buffer ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "Directory shouldn't have been deleted\n" );
    CloseHandle( handle );
    fileDeleted = GetFileAttributesA( buffer ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( fileDeleted, "Directory should have been deleted\n" );

    /* directory exists until all handles to it get closed */
    GetTempFileNameA( tmp_path, "dis", 0, buffer );
    DeleteFileA( buffer );
    ok( CreateDirectoryA( buffer, NULL ), "CreateDirectory failed\n" );
    handle = CreateFileA(buffer, DELETE, FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ok( handle != INVALID_HANDLE_VALUE, "failed to open a directory\n" );
    /* can open the marked for delete directory (proper sharing mode) */
    handle2 = CreateFileA(buffer, DELETE, FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
    ok( handle2 != INVALID_HANDLE_VALUE, "failed to open a directory\n" );
    /* can't open the marked for delete file (wrong sharing mode) */
    handle3 = CreateFileA(buffer, DELETE, 0, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
    ok( handle3 == INVALID_HANDLE_VALUE, "CreateFile should fail\n" );
    ok(GetLastError() == ERROR_SHARING_VIOLATION, "got %u\n", GetLastError());
    CloseHandle( handle );
    fileDeleted = GetFileAttributesA( buffer ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "Directory shouldn't have been deleted\n" );
    CloseHandle( handle2 );
    fileDeleted = GetFileAttributesA( buffer ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( fileDeleted, "Directory should have been deleted\n" );

    /* can open a non-empty directory with FILE_FLAG_DELETE_ON_CLOSE */
    GetTempFileNameA( tmp_path, "dis", 0, buffer );
    DeleteFileA( buffer );
    ok( CreateDirectoryA( buffer, NULL ), "CreateDirectory failed\n" );
    dirpos = lstrlenA( buffer );
    lstrcpyA( buffer + dirpos, "\\tst" );
    handle2 = CreateFileA(buffer, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, 0);
    CloseHandle( handle2 );
    buffer[dirpos] = '\0';
    handle = CreateFileA(buffer, DELETE, 0, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_DELETE_ON_CLOSE, 0);
    ok( handle != INVALID_HANDLE_VALUE, "failed to open a directory\n" );
    SetLastError(0xdeadbeef);
    CloseHandle( handle );
    ok(GetLastError() == 0xdeadbeef, "got %u\n", GetLastError());
    fileDeleted = GetFileAttributesA( buffer ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "Directory shouldn't have been deleted\n" );
    buffer[dirpos] = '\\';
    fileDeleted = DeleteFileA( buffer );
    ok( fileDeleted, "File should have been deleted\n" );
    buffer[dirpos] = '\0';
    fileDeleted = RemoveDirectoryA( buffer );
    ok( fileDeleted, "Directory should have been deleted\n" );

    /* cannot set disposition on a non-empty directory */
    GetTempFileNameA( tmp_path, "dis", 0, buffer );
    DeleteFileA( buffer );
    ok( CreateDirectoryA( buffer, NULL ), "CreateDirectory failed\n" );
    handle = CreateFileA(buffer, DELETE, 0, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
    ok( handle != INVALID_HANDLE_VALUE, "failed to open a directory\n" );
    dirpos = lstrlenA( buffer );
    lstrcpyA( buffer + dirpos, "\\tst" );
    handle2 = CreateFileA(buffer, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, 0);
    CloseHandle( handle2 );
    fdi.DoDeleteFile = TRUE;
    res = pNtSetInformationFile( handle, &io, &fdi, sizeof fdi, FileDispositionInformation );
    ok( res == STATUS_DIRECTORY_NOT_EMPTY, "unexpected FileDispositionInformation result (expected STATUS_DIRECTORY_NOT_EMPTY, got %x)\n", res );
    fileDeleted = DeleteFileA( buffer );
    ok( fileDeleted, "File should have been deleted\n" );
    buffer[dirpos] = '\0';
    CloseHandle( handle );
    fileDeleted = GetFileAttributesA( buffer ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "Directory shouldn't have been deleted\n" );
    fileDeleted = RemoveDirectoryA( buffer );
    ok( fileDeleted, "Directory should have been deleted\n" );

    /* cannot set disposition on file with file mapping opened */
    GetTempFileNameA( tmp_path, "dis", 0, buffer );
    handle = CreateFileA(buffer, GENERIC_READ | GENERIC_WRITE | DELETE, 0, NULL, CREATE_ALWAYS, 0, 0);
    ok( handle != INVALID_HANDLE_VALUE, "failed to create temp file\n" );
    mapping = CreateFileMappingA( handle, NULL, PAGE_READWRITE, 0, 64 * 1024, "DelFileTest" );
    ok( mapping != NULL, "failed to create file mapping\n");
    fdi.DoDeleteFile = TRUE;
    res = pNtSetInformationFile( handle, &io, &fdi, sizeof fdi, FileDispositionInformation );
    ok( res == STATUS_CANNOT_DELETE, "unexpected FileDispositionInformation result (expected STATUS_CANNOT_DELETE, got %x)\n", res );
    CloseHandle( handle );
    fileDeleted = GetFileAttributesA( buffer ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "File shouldn't have been deleted\n" );
    CloseHandle( mapping );
    DeleteFileA( buffer );

    /* can set disposition on file with file mapping closed */
    GetTempFileNameA( tmp_path, "dis", 0, buffer );
    handle = CreateFileA(buffer, GENERIC_READ | GENERIC_WRITE | DELETE, 0, NULL, CREATE_ALWAYS, 0, 0);
    ok( handle != INVALID_HANDLE_VALUE, "failed to create temp file\n" );
    mapping = CreateFileMappingA( handle, NULL, PAGE_READWRITE, 0, 64 * 1024, "DelFileTest" );
    ok( mapping != NULL, "failed to create file mapping\n");
    CloseHandle( mapping );
    fdi.DoDeleteFile = TRUE;
    res = pNtSetInformationFile( handle, &io, &fdi, sizeof fdi, FileDispositionInformation );
    ok( res == STATUS_SUCCESS, "unexpected FileDispositionInformation result (expected STATUS_SUCCESS, got %x)\n", res );
    CloseHandle( handle );
    fileDeleted = GetFileAttributesA( buffer ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( fileDeleted, "File should have been deleted\n" );
    DeleteFileA( buffer );

    /* cannot set disposition on file which is mapped to memory */
    GetTempFileNameA( tmp_path, "dis", 0, buffer );
    handle = CreateFileA(buffer, GENERIC_READ | GENERIC_WRITE | DELETE, 0, NULL, CREATE_ALWAYS, 0, 0);
    ok( handle != INVALID_HANDLE_VALUE, "failed to create temp file\n" );
    mapping = CreateFileMappingA( handle, NULL, PAGE_READWRITE, 0, 64 * 1024, "DelFileTest" );
    ok( mapping != NULL, "failed to create file mapping\n");
    ptr = MapViewOfFile( mapping, FILE_MAP_READ, 0, 0, 4096 );
    ok( ptr != NULL, "MapViewOfFile failed\n");
    CloseHandle( mapping );
    fdi.DoDeleteFile = TRUE;
    res = pNtSetInformationFile( handle, &io, &fdi, sizeof fdi, FileDispositionInformation );
    ok( res == STATUS_CANNOT_DELETE, "unexpected FileDispositionInformation result (expected STATUS_CANNOT_DELETE, got %x)\n", res );
    CloseHandle( handle );
    fileDeleted = GetFileAttributesA( buffer ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( !fileDeleted, "File shouldn't have been deleted\n" );
    UnmapViewOfFile( ptr );
    DeleteFileA( buffer );

    /* can set disposition on file which is mapped to memory and unmapped again */
    GetTempFileNameA( tmp_path, "dis", 0, buffer );
    handle = CreateFileA(buffer, GENERIC_READ | GENERIC_WRITE | DELETE, 0, NULL, CREATE_ALWAYS, 0, 0);
    ok( handle != INVALID_HANDLE_VALUE, "failed to create temp file\n" );
    mapping = CreateFileMappingA( handle, NULL, PAGE_READWRITE, 0, 64 * 1024, "DelFileTest" );
    ok( mapping != NULL, "failed to create file mapping\n");
    ptr = MapViewOfFile( mapping, FILE_MAP_READ, 0, 0, 4096 );
    ok( ptr != NULL, "MapViewOfFile failed\n");
    CloseHandle( mapping );
    UnmapViewOfFile( ptr );
    fdi.DoDeleteFile = TRUE;
    res = pNtSetInformationFile( handle, &io, &fdi, sizeof fdi, FileDispositionInformation );
    ok( res == STATUS_SUCCESS, "unexpected FileDispositionInformation result (expected STATUS_SUCCESS, got %x)\n", res );
    CloseHandle( handle );
    fileDeleted = GetFileAttributesA( buffer ) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND;
    ok( fileDeleted, "File should have been deleted\n" );
    DeleteFileA( buffer );
}

static void test_file_name_information(void)
{
    WCHAR *file_name, *volume_prefix, *expected;
    FILE_NAME_INFORMATION *info;
    ULONG old_redir = 1, tmp;
    UINT file_name_size;
    IO_STATUS_BLOCK io;
    UINT info_size;
    HRESULT hr;
    HANDLE h;
    UINT len;

    /* GetVolumePathName is not present before w2k */
    if (!pGetVolumePathNameW) {
        win_skip("GetVolumePathNameW not found\n");
        return;
    }

    file_name_size = GetSystemDirectoryW( NULL, 0 );
    file_name = HeapAlloc( GetProcessHeap(), 0, file_name_size * sizeof(*file_name) );
    volume_prefix = HeapAlloc( GetProcessHeap(), 0, file_name_size * sizeof(*volume_prefix) );
    expected = HeapAlloc( GetProcessHeap(), 0, file_name_size * sizeof(*volume_prefix) );

    len = GetSystemDirectoryW( file_name, file_name_size );
    ok(len == file_name_size - 1,
            "GetSystemDirectoryW returned %u, expected %u.\n",
            len, file_name_size - 1);

    len = pGetVolumePathNameW( file_name, volume_prefix, file_name_size );
    ok(len, "GetVolumePathNameW failed.\n");

    len = lstrlenW( volume_prefix );
    if (len && volume_prefix[len - 1] == '\\') --len;
    memcpy( expected, file_name + len, (file_name_size - len - 1) * sizeof(WCHAR) );
    expected[file_name_size - len - 1] = '\0';

    /* A bit more than we actually need, but it keeps the calculation simple. */
    info_size = sizeof(*info) + (file_name_size * sizeof(WCHAR));
    info = HeapAlloc( GetProcessHeap(), 0, info_size );

    if (pRtlWow64EnableFsRedirectionEx) pRtlWow64EnableFsRedirectionEx( TRUE, &old_redir );
    h = CreateFileW( file_name, GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0 );
    if (pRtlWow64EnableFsRedirectionEx) pRtlWow64EnableFsRedirectionEx( old_redir, &tmp );
    ok(h != INVALID_HANDLE_VALUE, "Failed to open file.\n");

    hr = pNtQueryInformationFile( h, &io, info, sizeof(*info) - 1, FileNameInformation );
    ok(hr == STATUS_INFO_LENGTH_MISMATCH, "NtQueryInformationFile returned %#x.\n", hr);

    memset( info, 0xcc, info_size );
    hr = pNtQueryInformationFile( h, &io, info, sizeof(*info), FileNameInformation );
    ok(hr == STATUS_BUFFER_OVERFLOW, "NtQueryInformationFile returned %#x, expected %#x.\n",
            hr, STATUS_BUFFER_OVERFLOW);
    ok(U(io).Status == STATUS_BUFFER_OVERFLOW, "io.Status is %#x, expected %#x.\n",
            U(io).Status, STATUS_BUFFER_OVERFLOW);
    ok(info->FileNameLength == lstrlenW( expected ) * sizeof(WCHAR), "info->FileNameLength is %u\n", info->FileNameLength);
    ok(info->FileName[2] == 0xcccc, "info->FileName[2] is %#x, expected 0xcccc.\n", info->FileName[2]);
    ok(CharLowerW((LPWSTR)(UINT_PTR)info->FileName[1]) == CharLowerW((LPWSTR)(UINT_PTR)expected[1]),
            "info->FileName[1] is %p, expected %p.\n",
            CharLowerW((LPWSTR)(UINT_PTR)info->FileName[1]), CharLowerW((LPWSTR)(UINT_PTR)expected[1]));
    ok(io.Information == sizeof(*info), "io.Information is %lu\n", io.Information);

    memset( info, 0xcc, info_size );
    hr = pNtQueryInformationFile( h, &io, info, info_size, FileNameInformation );
    ok(hr == STATUS_SUCCESS, "NtQueryInformationFile returned %#x, expected %#x.\n", hr, STATUS_SUCCESS);
    ok(U(io).Status == STATUS_SUCCESS, "io.Status is %#x, expected %#x.\n", U(io).Status, STATUS_SUCCESS);
    ok(info->FileNameLength == lstrlenW( expected ) * sizeof(WCHAR), "info->FileNameLength is %u\n", info->FileNameLength);
    ok(info->FileName[info->FileNameLength / sizeof(WCHAR)] == 0xcccc, "info->FileName[len] is %#x, expected 0xcccc.\n",
       info->FileName[info->FileNameLength / sizeof(WCHAR)]);
    info->FileName[info->FileNameLength / sizeof(WCHAR)] = '\0';
    ok(!lstrcmpiW( info->FileName, expected ), "info->FileName is %s, expected %s.\n",
            wine_dbgstr_w( info->FileName ), wine_dbgstr_w( expected ));
    ok(io.Information == FIELD_OFFSET(FILE_NAME_INFORMATION, FileName) + info->FileNameLength,
            "io.Information is %lu, expected %u.\n",
            io.Information, FIELD_OFFSET(FILE_NAME_INFORMATION, FileName) + info->FileNameLength);

    CloseHandle( h );
    HeapFree( GetProcessHeap(), 0, info );
    HeapFree( GetProcessHeap(), 0, expected );
    HeapFree( GetProcessHeap(), 0, volume_prefix );

    if (old_redir || !pGetSystemWow64DirectoryW || !(file_name_size = pGetSystemWow64DirectoryW( NULL, 0 )))
    {
        skip("Not running on WoW64, skipping test.\n");
        HeapFree( GetProcessHeap(), 0, file_name );
        return;
    }

    h = CreateFileW( file_name, GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0 );
    ok(h != INVALID_HANDLE_VALUE, "Failed to open file.\n");
    HeapFree( GetProcessHeap(), 0, file_name );

    file_name = HeapAlloc( GetProcessHeap(), 0, file_name_size * sizeof(*file_name) );
    volume_prefix = HeapAlloc( GetProcessHeap(), 0, file_name_size * sizeof(*volume_prefix) );
    expected = HeapAlloc( GetProcessHeap(), 0, file_name_size * sizeof(*expected) );

    len = pGetSystemWow64DirectoryW( file_name, file_name_size );
    ok(len == file_name_size - 1,
            "GetSystemWow64DirectoryW returned %u, expected %u.\n",
            len, file_name_size - 1);

    len = pGetVolumePathNameW( file_name, volume_prefix, file_name_size );
    ok(len, "GetVolumePathNameW failed.\n");

    len = lstrlenW( volume_prefix );
    if (len && volume_prefix[len - 1] == '\\') --len;
    memcpy( expected, file_name + len, (file_name_size - len - 1) * sizeof(WCHAR) );
    expected[file_name_size - len - 1] = '\0';

    info_size = sizeof(*info) + (file_name_size * sizeof(WCHAR));
    info = HeapAlloc( GetProcessHeap(), 0, info_size );

    memset( info, 0xcc, info_size );
    hr = pNtQueryInformationFile( h, &io, info, info_size, FileNameInformation );
    ok(hr == STATUS_SUCCESS, "NtQueryInformationFile returned %#x, expected %#x.\n", hr, STATUS_SUCCESS);
    info->FileName[info->FileNameLength / sizeof(WCHAR)] = '\0';
    ok(!lstrcmpiW( info->FileName, expected ), "info->FileName is %s, expected %s.\n",
            wine_dbgstr_w( info->FileName ), wine_dbgstr_w( expected ));

    CloseHandle( h );
    HeapFree( GetProcessHeap(), 0, info );
    HeapFree( GetProcessHeap(), 0, expected );
    HeapFree( GetProcessHeap(), 0, volume_prefix );
    HeapFree( GetProcessHeap(), 0, file_name );
}

static void test_file_all_name_information(void)
{
    WCHAR *file_name, *volume_prefix, *expected;
    FILE_ALL_INFORMATION *info;
    ULONG old_redir = 1, tmp;
    UINT file_name_size;
    IO_STATUS_BLOCK io;
    UINT info_size;
    HRESULT hr;
    HANDLE h;
    UINT len;

    /* GetVolumePathName is not present before w2k */
    if (!pGetVolumePathNameW) {
        win_skip("GetVolumePathNameW not found\n");
        return;
    }

    file_name_size = GetSystemDirectoryW( NULL, 0 );
    file_name = HeapAlloc( GetProcessHeap(), 0, file_name_size * sizeof(*file_name) );
    volume_prefix = HeapAlloc( GetProcessHeap(), 0, file_name_size * sizeof(*volume_prefix) );
    expected = HeapAlloc( GetProcessHeap(), 0, file_name_size * sizeof(*volume_prefix) );

    len = GetSystemDirectoryW( file_name, file_name_size );
    ok(len == file_name_size - 1,
            "GetSystemDirectoryW returned %u, expected %u.\n",
            len, file_name_size - 1);

    len = pGetVolumePathNameW( file_name, volume_prefix, file_name_size );
    ok(len, "GetVolumePathNameW failed.\n");

    len = lstrlenW( volume_prefix );
    if (len && volume_prefix[len - 1] == '\\') --len;
    memcpy( expected, file_name + len, (file_name_size - len - 1) * sizeof(WCHAR) );
    expected[file_name_size - len - 1] = '\0';

    /* A bit more than we actually need, but it keeps the calculation simple. */
    info_size = sizeof(*info) + (file_name_size * sizeof(WCHAR));
    info = HeapAlloc( GetProcessHeap(), 0, info_size );

    if (pRtlWow64EnableFsRedirectionEx) pRtlWow64EnableFsRedirectionEx( TRUE, &old_redir );
    h = CreateFileW( file_name, GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0 );
    if (pRtlWow64EnableFsRedirectionEx) pRtlWow64EnableFsRedirectionEx( old_redir, &tmp );
    ok(h != INVALID_HANDLE_VALUE, "Failed to open file.\n");

    hr = pNtQueryInformationFile( h, &io, info, sizeof(*info) - 1, FileAllInformation );
    ok(hr == STATUS_INFO_LENGTH_MISMATCH, "NtQueryInformationFile returned %#x, expected %#x.\n",
            hr, STATUS_INFO_LENGTH_MISMATCH);

    memset( info, 0xcc, info_size );
    hr = pNtQueryInformationFile( h, &io, info, sizeof(*info), FileAllInformation );
    ok(hr == STATUS_BUFFER_OVERFLOW, "NtQueryInformationFile returned %#x, expected %#x.\n",
            hr, STATUS_BUFFER_OVERFLOW);
    ok(U(io).Status == STATUS_BUFFER_OVERFLOW, "io.Status is %#x, expected %#x.\n",
            U(io).Status, STATUS_BUFFER_OVERFLOW);
    ok(info->NameInformation.FileNameLength == lstrlenW( expected ) * sizeof(WCHAR),
       "info->NameInformation.FileNameLength is %u\n", info->NameInformation.FileNameLength );
    ok(info->NameInformation.FileName[2] == 0xcccc,
            "info->NameInformation.FileName[2] is %#x, expected 0xcccc.\n", info->NameInformation.FileName[2]);
    ok(CharLowerW((LPWSTR)(UINT_PTR)info->NameInformation.FileName[1]) == CharLowerW((LPWSTR)(UINT_PTR)expected[1]),
            "info->NameInformation.FileName[1] is %p, expected %p.\n",
            CharLowerW((LPWSTR)(UINT_PTR)info->NameInformation.FileName[1]), CharLowerW((LPWSTR)(UINT_PTR)expected[1]));
    ok(io.Information == sizeof(*info), "io.Information is %lu\n", io.Information);

    memset( info, 0xcc, info_size );
    hr = pNtQueryInformationFile( h, &io, info, info_size, FileAllInformation );
    ok(hr == STATUS_SUCCESS, "NtQueryInformationFile returned %#x, expected %#x.\n", hr, STATUS_SUCCESS);
    ok(U(io).Status == STATUS_SUCCESS, "io.Status is %#x, expected %#x.\n", U(io).Status, STATUS_SUCCESS);
    ok(info->NameInformation.FileNameLength == lstrlenW( expected ) * sizeof(WCHAR),
       "info->NameInformation.FileNameLength is %u\n", info->NameInformation.FileNameLength );
    ok(info->NameInformation.FileName[info->NameInformation.FileNameLength / sizeof(WCHAR)] == 0xcccc,
       "info->NameInformation.FileName[len] is %#x, expected 0xcccc.\n",
       info->NameInformation.FileName[info->NameInformation.FileNameLength / sizeof(WCHAR)]);
    info->NameInformation.FileName[info->NameInformation.FileNameLength / sizeof(WCHAR)] = '\0';
    ok(!lstrcmpiW( info->NameInformation.FileName, expected ),
            "info->NameInformation.FileName is %s, expected %s.\n",
            wine_dbgstr_w( info->NameInformation.FileName ), wine_dbgstr_w( expected ));
    ok(io.Information == FIELD_OFFSET(FILE_ALL_INFORMATION, NameInformation.FileName)
            + info->NameInformation.FileNameLength,
            "io.Information is %lu\n", io.Information );

    CloseHandle( h );
    HeapFree( GetProcessHeap(), 0, info );
    HeapFree( GetProcessHeap(), 0, expected );
    HeapFree( GetProcessHeap(), 0, volume_prefix );

    if (old_redir || !pGetSystemWow64DirectoryW || !(file_name_size = pGetSystemWow64DirectoryW( NULL, 0 )))
    {
        skip("Not running on WoW64, skipping test.\n");
        HeapFree( GetProcessHeap(), 0, file_name );
        return;
    }

    h = CreateFileW( file_name, GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0 );
    ok(h != INVALID_HANDLE_VALUE, "Failed to open file.\n");
    HeapFree( GetProcessHeap(), 0, file_name );

    file_name = HeapAlloc( GetProcessHeap(), 0, file_name_size * sizeof(*file_name) );
    volume_prefix = HeapAlloc( GetProcessHeap(), 0, file_name_size * sizeof(*volume_prefix) );
    expected = HeapAlloc( GetProcessHeap(), 0, file_name_size * sizeof(*expected) );

    len = pGetSystemWow64DirectoryW( file_name, file_name_size );
    ok(len == file_name_size - 1,
            "GetSystemWow64DirectoryW returned %u, expected %u.\n",
            len, file_name_size - 1);

    len = pGetVolumePathNameW( file_name, volume_prefix, file_name_size );
    ok(len, "GetVolumePathNameW failed.\n");

    len = lstrlenW( volume_prefix );
    if (len && volume_prefix[len - 1] == '\\') --len;
    memcpy( expected, file_name + len, (file_name_size - len - 1) * sizeof(WCHAR) );
    expected[file_name_size - len - 1] = '\0';

    info_size = sizeof(*info) + (file_name_size * sizeof(WCHAR));
    info = HeapAlloc( GetProcessHeap(), 0, info_size );

    memset( info, 0xcc, info_size );
    hr = pNtQueryInformationFile( h, &io, info, info_size, FileAllInformation );
    ok(hr == STATUS_SUCCESS, "NtQueryInformationFile returned %#x, expected %#x.\n", hr, STATUS_SUCCESS);
    info->NameInformation.FileName[info->NameInformation.FileNameLength / sizeof(WCHAR)] = '\0';
    ok(!lstrcmpiW( info->NameInformation.FileName, expected ), "info->NameInformation.FileName is %s, expected %s.\n",
            wine_dbgstr_w( info->NameInformation.FileName ), wine_dbgstr_w( expected ));

    CloseHandle( h );
    HeapFree( GetProcessHeap(), 0, info );
    HeapFree( GetProcessHeap(), 0, expected );
    HeapFree( GetProcessHeap(), 0, volume_prefix );
    HeapFree( GetProcessHeap(), 0, file_name );
}

#define test_completion_flags(a,b) _test_completion_flags(__LINE__,a,b)
static void _test_completion_flags(unsigned line, HANDLE handle, DWORD expected_flags)
{
    FILE_IO_COMPLETION_NOTIFICATION_INFORMATION info;
    IO_STATUS_BLOCK io;
    NTSTATUS status;

    info.Flags = 0xdeadbeef;
    status = pNtQueryInformationFile(handle, &io, &info, sizeof(info),
                                     FileIoCompletionNotificationInformation);
    ok_(__FILE__,line)(status == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %08x\n", status);
    ok_(__FILE__,line)(io.Status == STATUS_SUCCESS, "Status = %x\n", io.Status);
    ok_(__FILE__,line)(io.Information == sizeof(info), "Information = %lu\n", io.Information);
    /* FILE_SKIP_SET_USER_EVENT_ON_FAST_IO is not supported on win2k3 */
    ok_(__FILE__,line)((info.Flags & ~FILE_SKIP_SET_USER_EVENT_ON_FAST_IO) == expected_flags,
                       "got %08x\n", info.Flags);
}

static void test_file_completion_information(void)
{
    DECLSPEC_ALIGN(TEST_OVERLAPPED_READ_SIZE) static unsigned char aligned_buf[TEST_OVERLAPPED_READ_SIZE];
    static const char buf[] = "testdata";
    FILE_IO_COMPLETION_NOTIFICATION_INFORMATION info;
    OVERLAPPED ov, *pov;
    IO_STATUS_BLOCK io;
    NTSTATUS status;
    DWORD num_bytes;
    HANDLE port, h;
    ULONG_PTR key;
    BOOL ret;
    int i;

    if (!(h = create_temp_file(0))) return;

    status = pNtSetInformationFile(h, &io, &info, sizeof(info) - 1, FileIoCompletionNotificationInformation);
    ok(status == STATUS_INFO_LENGTH_MISMATCH || broken(status == STATUS_INVALID_INFO_CLASS /* XP */),
       "expected STATUS_INFO_LENGTH_MISMATCH, got %08x\n", status);
    if (status != STATUS_INFO_LENGTH_MISMATCH)
    {
        win_skip("FileIoCompletionNotificationInformation class not supported\n");
        CloseHandle(h);
        return;
    }

    info.Flags = FILE_SKIP_COMPLETION_PORT_ON_SUCCESS;
    status = pNtSetInformationFile(h, &io, &info, sizeof(info), FileIoCompletionNotificationInformation);
    ok(status == STATUS_INVALID_PARAMETER, "expected STATUS_INVALID_PARAMETER, got %08x\n", status);

    CloseHandle(h);
    if (!(h = create_temp_file(FILE_FLAG_OVERLAPPED))) return;

    info.Flags = FILE_SKIP_SET_EVENT_ON_HANDLE;
    status = pNtSetInformationFile(h, &io, &info, sizeof(info), FileIoCompletionNotificationInformation);
    ok(status == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %08x\n", status);
    test_completion_flags(h, FILE_SKIP_SET_EVENT_ON_HANDLE);

    info.Flags = FILE_SKIP_SET_USER_EVENT_ON_FAST_IO;
    status = pNtSetInformationFile(h, &io, &info, sizeof(info), FileIoCompletionNotificationInformation);
    ok(status == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %08x\n", status);
    test_completion_flags(h, FILE_SKIP_SET_EVENT_ON_HANDLE);

    info.Flags = 0;
    status = pNtSetInformationFile(h, &io, &info, sizeof(info), FileIoCompletionNotificationInformation);
    ok(status == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %08x\n", status);
    test_completion_flags(h, FILE_SKIP_SET_EVENT_ON_HANDLE);

    info.Flags = FILE_SKIP_COMPLETION_PORT_ON_SUCCESS;
    status = pNtSetInformationFile(h, &io, &info, sizeof(info), FileIoCompletionNotificationInformation);
    ok(status == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %08x\n", status);
    test_completion_flags(h, FILE_SKIP_SET_EVENT_ON_HANDLE | FILE_SKIP_COMPLETION_PORT_ON_SUCCESS);

    info.Flags = 0xdeadbeef;
    status = pNtSetInformationFile(h, &io, &info, sizeof(info), FileIoCompletionNotificationInformation);
    ok(status == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %08x\n", status);
    test_completion_flags(h, FILE_SKIP_SET_EVENT_ON_HANDLE | FILE_SKIP_COMPLETION_PORT_ON_SUCCESS);

    CloseHandle(h);
    if (!(h = create_temp_file(FILE_FLAG_OVERLAPPED))) return;
    test_completion_flags(h, 0);

    memset(&ov, 0, sizeof(ov));
    ov.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    port = CreateIoCompletionPort(h, NULL, 0xdeadbeef, 0);
    ok(port != NULL, "CreateIoCompletionPort failed, error %u\n", GetLastError());

    for (i = 0; i < 10; i++)
    {
        SetLastError(0xdeadbeef);
        ret = WriteFile(h, buf, sizeof(buf), &num_bytes, &ov);
        ok((!ret && GetLastError() == ERROR_IO_PENDING) || broken(ret) /* Before Vista */,
                "Unexpected result %#x, GetLastError() %u.\n", ret, GetLastError());
        if (ret || GetLastError() != ERROR_IO_PENDING) break;
        ret = GetOverlappedResult(h, &ov, &num_bytes, TRUE);
        ok(ret, "GetOverlappedResult failed, error %u\n", GetLastError());
        ret = GetQueuedCompletionStatus(port, &num_bytes, &key, &pov, 1000);
        ok(ret, "GetQueuedCompletionStatus failed, error %u\n", GetLastError());
        ret = FALSE;
    }
    if (ret)
    {
        ok(num_bytes == sizeof(buf), "expected sizeof(buf), got %u\n", num_bytes);

        key = 0;
        pov = NULL;
        ret = GetQueuedCompletionStatus(port, &num_bytes, &key, &pov, 1000);
        ok(ret, "GetQueuedCompletionStatus failed, error %u\n", GetLastError());
        ok(key == 0xdeadbeef, "expected 0xdeadbeef, got %lx\n", key);
        ok(pov == &ov, "expected %p, got %p\n", &ov, pov);
    }

    info.Flags = FILE_SKIP_COMPLETION_PORT_ON_SUCCESS;
    status = pNtSetInformationFile(h, &io, &info, sizeof(info), FileIoCompletionNotificationInformation);
    ok(status == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %08x\n", status);
    test_completion_flags(h, FILE_SKIP_COMPLETION_PORT_ON_SUCCESS);

    for (i = 0; i < 10; i++)
    {
        SetLastError(0xdeadbeef);
        ret = WriteFile(h, buf, sizeof(buf), &num_bytes, &ov);
        ok((!ret && GetLastError() == ERROR_IO_PENDING) || broken(ret) /* Before Vista */,
                "Unexpected result %#x, GetLastError() %u.\n", ret, GetLastError());
        if (ret || GetLastError() != ERROR_IO_PENDING) break;
        ret = GetOverlappedResult(h, &ov, &num_bytes, TRUE);
        ok(ret, "GetOverlappedResult failed, error %u\n", GetLastError());
        ret = FALSE;
    }
    if (ret)
    {
        ok(num_bytes == sizeof(buf), "expected sizeof(buf), got %u\n", num_bytes);

        pov = (void *)0xdeadbeef;
        ret = GetQueuedCompletionStatus(port, &num_bytes, &key, &pov, 500);
        ok(!ret, "GetQueuedCompletionStatus succeeded\n");
        ok(pov == NULL, "expected NULL, got %p\n", pov);
    }

    info.Flags = 0;
    status = pNtSetInformationFile(h, &io, &info, sizeof(info), FileIoCompletionNotificationInformation);
    ok(status == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %08x\n", status);
    test_completion_flags(h, FILE_SKIP_COMPLETION_PORT_ON_SUCCESS);

    for (i = 0; i < 10; i++)
    {
        SetLastError(0xdeadbeef);
        ret = WriteFile(h, buf, sizeof(buf), &num_bytes, &ov);
        ok((!ret && GetLastError() == ERROR_IO_PENDING) || broken(ret) /* Before Vista */,
                "Unexpected result %#x, GetLastError() %u.\n", ret, GetLastError());
        if (ret || GetLastError() != ERROR_IO_PENDING) break;
        ret = GetOverlappedResult(h, &ov, &num_bytes, TRUE);
        ok(ret, "GetOverlappedResult failed, error %u\n", GetLastError());
        ret = GetQueuedCompletionStatus(port, &num_bytes, &key, &pov, 1000);
        ok(ret, "GetQueuedCompletionStatus failed, error %u\n", GetLastError());
        ret = FALSE;
    }
    if (ret)
    {
        ok(num_bytes == sizeof(buf), "expected sizeof(buf), got %u\n", num_bytes);

        pov = (void *)0xdeadbeef;
        ret = GetQueuedCompletionStatus(port, &num_bytes, &key, &pov, 1000);
        ok(!ret, "GetQueuedCompletionStatus succeeded\n");
        ok(pov == NULL, "expected NULL, got %p\n", pov);
    }

    CloseHandle(port);
    CloseHandle(h);

    if (!(h = create_temp_file(FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING)))
        return;

    port = CreateIoCompletionPort(h, NULL, 0xdeadbeef, 0);
    ok(port != NULL, "CreateIoCompletionPort failed, error %u.\n", GetLastError());

    info.Flags = FILE_SKIP_COMPLETION_PORT_ON_SUCCESS;
    status = pNtSetInformationFile(h, &io, &info, sizeof(info), FileIoCompletionNotificationInformation);
    ok(status == STATUS_SUCCESS, "Expected STATUS_SUCCESS, got %#x.\n", status);
    test_completion_flags(h, FILE_SKIP_COMPLETION_PORT_ON_SUCCESS);

    ret = WriteFile(h, aligned_buf, sizeof(aligned_buf), &num_bytes, &ov);
    if (!ret && GetLastError() == ERROR_IO_PENDING)
    {
        ret = GetOverlappedResult(h, &ov, &num_bytes, TRUE);
        ok(ret, "GetOverlappedResult failed, error %u.\n", GetLastError());
        ok(num_bytes == sizeof(aligned_buf), "expected sizeof(aligned_buf), got %u.\n", num_bytes);
        ret = GetQueuedCompletionStatus(port, &num_bytes, &key, &pov, 1000);
        ok(ret, "GetQueuedCompletionStatus failed, error %u.\n", GetLastError());
    }
    ok(num_bytes == sizeof(aligned_buf), "expected sizeof(buf), got %u.\n", num_bytes);

    SetLastError(0xdeadbeef);
    ret = ReadFile(h, aligned_buf, sizeof(aligned_buf), &num_bytes, &ov);
    ok(!ret && GetLastError() == ERROR_IO_PENDING, "Unexpected result, ret %#x, error %u.\n",
            ret, GetLastError());
    ret = GetOverlappedResult(h, &ov, &num_bytes, TRUE);
    ok(ret, "GetOverlappedResult failed, error %u.\n", GetLastError());
    ret = GetQueuedCompletionStatus(port, &num_bytes, &key, &pov, 1000);
    ok(ret, "GetQueuedCompletionStatus failed, error %u.\n", GetLastError());

    CloseHandle(ov.hEvent);
    CloseHandle(port);
    CloseHandle(h);
}

static void test_file_id_information(void)
{
    BY_HANDLE_FILE_INFORMATION info;
    FILE_ID_INFORMATION fid;
    IO_STATUS_BLOCK io;
    NTSTATUS status;
    DWORD *dwords;
    HANDLE h;
    BOOL ret;

    if (!(h = create_temp_file(0))) return;

    memset( &fid, 0x11, sizeof(fid) );
    status = pNtQueryInformationFile( h, &io, &fid, sizeof(fid), FileIdInformation );
    if (status == STATUS_NOT_IMPLEMENTED || status == STATUS_INVALID_INFO_CLASS)
    {
        win_skip( "FileIdInformation not supported\n" );
        CloseHandle( h );
        return;
    }

    memset( &info, 0x22, sizeof(info) );
    ret = GetFileInformationByHandle( h, &info );
    ok( ret, "GetFileInformationByHandle failed\n" );

    dwords = (DWORD *)&fid.VolumeSerialNumber;
    ok( dwords[0] == info.dwVolumeSerialNumber, "expected %08x, got %08x\n",
        info.dwVolumeSerialNumber, dwords[0] );
    ok( dwords[1] != 0x11111111, "expected != 0x11111111\n" );

    dwords = (DWORD *)&fid.FileId;
    ok( dwords[0] == info.nFileIndexLow, "expected %08x, got %08x\n", info.nFileIndexLow, dwords[0] );
    ok( dwords[1] == info.nFileIndexHigh, "expected %08x, got %08x\n", info.nFileIndexHigh, dwords[1] );
    ok( dwords[2] == 0, "expected 0, got %08x\n", dwords[2] );
    ok( dwords[3] == 0, "expected 0, got %08x\n", dwords[3] );

    CloseHandle( h );
}

static void test_file_access_information(void)
{
    FILE_ACCESS_INFORMATION info;
    IO_STATUS_BLOCK io;
    NTSTATUS status;
    HANDLE h;

    if (!(h = create_temp_file(0))) return;

    status = pNtQueryInformationFile( h, &io, &info, sizeof(info) - 1, FileAccessInformation );
    ok( status == STATUS_INFO_LENGTH_MISMATCH, "expected STATUS_INFO_LENGTH_MISMATCH, got %08x\n", status );

    status = pNtQueryInformationFile( (HANDLE)0xdeadbeef, &io, &info, sizeof(info), FileAccessInformation );
    ok( status == STATUS_INVALID_HANDLE, "expected STATUS_INVALID_HANDLE, got %08x\n", status );

    memset(&info, 0x11, sizeof(info));
    status = pNtQueryInformationFile( h, &io, &info, sizeof(info), FileAccessInformation );
    ok( status == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %08x\n", status );
    ok( info.AccessFlags == 0x13019f, "got %08x\n", info.AccessFlags );

    CloseHandle( h );
}

static void test_file_attribute_tag_information(void)
{
    FILE_ATTRIBUTE_TAG_INFORMATION info;
    FILE_BASIC_INFORMATION fbi = {};
    IO_STATUS_BLOCK io;
    NTSTATUS status;
    HANDLE h;

    if (!(h = create_temp_file(0))) return;

    status = pNtQueryInformationFile( h, &io, &info, sizeof(info) - 1, FileAttributeTagInformation );
    ok( status == STATUS_INFO_LENGTH_MISMATCH, "got %#x\n", status );

    status = pNtQueryInformationFile( (HANDLE)0xdeadbeef, &io, &info, sizeof(info), FileAttributeTagInformation );
    ok( status == STATUS_INVALID_HANDLE, "got %#x\n", status );

    memset(&info, 0x11, sizeof(info));
    status = pNtQueryInformationFile( h, &io, &info, sizeof(info), FileAttributeTagInformation );
    ok( status == STATUS_SUCCESS, "got %#x\n", status );
    info.FileAttributes &= ~FILE_ATTRIBUTE_NOT_CONTENT_INDEXED;
    ok( info.FileAttributes == FILE_ATTRIBUTE_ARCHIVE, "got attributes %#x\n", info.FileAttributes );
    ok( !info.ReparseTag, "got reparse tag %#x\n", info.ReparseTag );

    fbi.FileAttributes = FILE_ATTRIBUTE_SYSTEM;
    status = pNtSetInformationFile(h, &io, &fbi, sizeof(fbi), FileBasicInformation);
    ok( status == STATUS_SUCCESS, "got %#x\n", status );

    memset(&info, 0x11, sizeof(info));
    status = pNtQueryInformationFile( h, &io, &info, sizeof(info), FileAttributeTagInformation );
    ok( status == STATUS_SUCCESS, "got %#x\n", status );
    todo_wine ok( info.FileAttributes == FILE_ATTRIBUTE_SYSTEM, "got attributes %#x\n", info.FileAttributes );
    ok( !info.ReparseTag, "got reparse tag %#x\n", info.ReparseTag );

    fbi.FileAttributes = FILE_ATTRIBUTE_HIDDEN;
    status = pNtSetInformationFile(h, &io, &fbi, sizeof fbi, FileBasicInformation);
    ok( status == STATUS_SUCCESS, "got %#x\n", status );

    memset(&info, 0x11, sizeof(info));
    status = pNtQueryInformationFile( h, &io, &info, sizeof(info), FileAttributeTagInformation );
    ok( status == STATUS_SUCCESS, "got %#x\n", status );
    todo_wine ok( info.FileAttributes == FILE_ATTRIBUTE_HIDDEN, "got attributes %#x\n", info.FileAttributes );
    ok( !info.ReparseTag, "got reparse tag %#x\n", info.ReparseTag );

    CloseHandle( h );
}

static void test_file_mode(void)
{
    UNICODE_STRING file_name, pipe_dev_name, mountmgr_dev_name, mailslot_dev_name;
    WCHAR tmp_path[MAX_PATH], dos_file_name[MAX_PATH];
    FILE_MODE_INFORMATION mode;
    OBJECT_ATTRIBUTES attr;
    IO_STATUS_BLOCK io;
    HANDLE file;
    unsigned i;
    DWORD res, access;
    NTSTATUS status;

    const struct {
        UNICODE_STRING *file_name;
        ULONG options;
        ULONG mode;
        BOOL todo;
    } option_tests[] = {
        { &file_name, 0, 0 },
        { &file_name, FILE_NON_DIRECTORY_FILE, 0 },
        { &file_name, FILE_NON_DIRECTORY_FILE | FILE_SEQUENTIAL_ONLY, FILE_SEQUENTIAL_ONLY },
        { &file_name, FILE_WRITE_THROUGH, FILE_WRITE_THROUGH },
        { &file_name, FILE_SYNCHRONOUS_IO_ALERT, FILE_SYNCHRONOUS_IO_ALERT },
        { &file_name, FILE_NO_INTERMEDIATE_BUFFERING, FILE_NO_INTERMEDIATE_BUFFERING },
        { &file_name, FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE, FILE_SYNCHRONOUS_IO_NONALERT },
        { &file_name, FILE_DELETE_ON_CLOSE, 0 },
        { &file_name, FILE_RANDOM_ACCESS | FILE_NO_COMPRESSION, 0 },
        { &pipe_dev_name, 0, 0 },
        { &pipe_dev_name, FILE_SYNCHRONOUS_IO_ALERT, FILE_SYNCHRONOUS_IO_ALERT },
        { &mailslot_dev_name, 0, 0 },
        { &mailslot_dev_name, FILE_SYNCHRONOUS_IO_ALERT, FILE_SYNCHRONOUS_IO_ALERT, TRUE },
        { &mountmgr_dev_name, 0, 0 },
        { &mountmgr_dev_name, FILE_SYNCHRONOUS_IO_ALERT, FILE_SYNCHRONOUS_IO_ALERT }
    };

    static WCHAR pipe_devW[] = {'\\','?','?','\\','P','I','P','E','\\'};
    static WCHAR mailslot_devW[] = {'\\','?','?','\\','M','A','I','L','S','L','O','T','\\'};
    static WCHAR mountmgr_devW[] =
        {'\\','?','?','\\','M','o','u','n','t','P','o','i','n','t','M','a','n','a','g','e','r'};

    GetTempPathW(MAX_PATH, tmp_path);
    res = GetTempFileNameW(tmp_path, fooW, 0, dos_file_name);
    ok(res, "GetTempFileNameW failed: %u\n", GetLastError());
    pRtlDosPathNameToNtPathName_U( dos_file_name, &file_name, NULL, NULL );

    pipe_dev_name.Buffer = pipe_devW;
    pipe_dev_name.Length = sizeof(pipe_devW);
    pipe_dev_name.MaximumLength = sizeof(pipe_devW);

    mailslot_dev_name.Buffer = mailslot_devW;
    mailslot_dev_name.Length = sizeof(mailslot_devW);
    mailslot_dev_name.MaximumLength = sizeof(mailslot_devW);

    mountmgr_dev_name.Buffer = mountmgr_devW;
    mountmgr_dev_name.Length = sizeof(mountmgr_devW);
    mountmgr_dev_name.MaximumLength = sizeof(mountmgr_devW);

    attr.Length = sizeof(attr);
    attr.RootDirectory = 0;
    attr.Attributes = OBJ_CASE_INSENSITIVE;
    attr.SecurityDescriptor = NULL;
    attr.SecurityQualityOfService = NULL;

    for (i = 0; i < ARRAY_SIZE(option_tests); i++)
    {
        attr.ObjectName = option_tests[i].file_name;
        access = SYNCHRONIZE;

        if (option_tests[i].file_name == &file_name)
        {
            file = CreateFileW(dos_file_name, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, 0);
            ok(file != INVALID_HANDLE_VALUE, "CreateFile failed: %u\n", GetLastError());
            CloseHandle(file);
            access |= GENERIC_WRITE | DELETE;
        }

        status = pNtOpenFile(&file, access, &attr, &io, 0, option_tests[i].options);
        ok(status == STATUS_SUCCESS, "[%u] NtOpenFile failed: %x\n", i, status);

        memset(&mode, 0xcc, sizeof(mode));
        status = pNtQueryInformationFile(file, &io, &mode, sizeof(mode), FileModeInformation);
        ok(status == STATUS_SUCCESS, "[%u] can't get FileModeInformation: %x\n", i, status);
        todo_wine_if(option_tests[i].todo)
        ok(mode.Mode == option_tests[i].mode, "[%u] Mode = %x, expected %x\n",
           i, mode.Mode, option_tests[i].mode);

        pNtClose(file);
        if (option_tests[i].file_name == &file_name)
            DeleteFileW(dos_file_name);
    }

    pRtlFreeUnicodeString(&file_name);
}

static void test_query_volume_information_file(void)
{
    NTSTATUS status;
    HANDLE dir;
    WCHAR path[MAX_PATH];
    OBJECT_ATTRIBUTES attr;
    IO_STATUS_BLOCK io;
    UNICODE_STRING nameW;
    FILE_FS_VOLUME_INFORMATION *ffvi;
    BYTE buf[sizeof(FILE_FS_VOLUME_INFORMATION) + MAX_PATH * sizeof(WCHAR)];

    GetWindowsDirectoryW( path, MAX_PATH );
    pRtlDosPathNameToNtPathName_U( path, &nameW, NULL, NULL );
    attr.Length = sizeof(attr);
    attr.RootDirectory = 0;
    attr.ObjectName = &nameW;
    attr.Attributes = OBJ_CASE_INSENSITIVE;
    attr.SecurityDescriptor = NULL;
    attr.SecurityQualityOfService = NULL;

    status = pNtOpenFile( &dir, SYNCHRONIZE|FILE_LIST_DIRECTORY, &attr, &io,
                          FILE_SHARE_READ|FILE_SHARE_WRITE, FILE_DIRECTORY_FILE|FILE_SYNCHRONOUS_IO_NONALERT );
    ok( !status, "open %s failed %x\n", wine_dbgstr_w(nameW.Buffer), status );
    pRtlFreeUnicodeString( &nameW );

    ZeroMemory( buf, sizeof(buf) );
    U(io).Status = 0xdadadada;
    io.Information = 0xcacacaca;

    status = pNtQueryVolumeInformationFile( dir, &io, buf, sizeof(buf), FileFsVolumeInformation );

    ffvi = (FILE_FS_VOLUME_INFORMATION *)buf;

    ok(status == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %d\n", status);
    ok(U(io).Status == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %d\n", U(io).Status);

    ok(io.Information == (FIELD_OFFSET(FILE_FS_VOLUME_INFORMATION, VolumeLabel) + ffvi->VolumeLabelLength),
    "expected %d, got %lu\n", (FIELD_OFFSET(FILE_FS_VOLUME_INFORMATION, VolumeLabel) + ffvi->VolumeLabelLength),
     io.Information);

    todo_wine ok(ffvi->VolumeCreationTime.QuadPart != 0, "Missing VolumeCreationTime\n");
    ok(ffvi->VolumeSerialNumber != 0, "Missing VolumeSerialNumber\n");
    ok(ffvi->SupportsObjects == 1,"expected 1, got %d\n", ffvi->SupportsObjects);
    ok(ffvi->VolumeLabelLength == lstrlenW(ffvi->VolumeLabel) * sizeof(WCHAR), "got %d\n", ffvi->VolumeLabelLength);

    trace("VolumeSerialNumber: %x VolumeLabelName: %s\n", ffvi->VolumeSerialNumber, wine_dbgstr_w(ffvi->VolumeLabel));

    CloseHandle( dir );
}

static void test_query_attribute_information_file(void)
{
    NTSTATUS status;
    HANDLE dir;
    WCHAR path[MAX_PATH];
    OBJECT_ATTRIBUTES attr;
    IO_STATUS_BLOCK io;
    UNICODE_STRING nameW;
    FILE_FS_ATTRIBUTE_INFORMATION *ffai;
    BYTE buf[sizeof(FILE_FS_ATTRIBUTE_INFORMATION) + MAX_PATH * sizeof(WCHAR)];

    GetWindowsDirectoryW( path, MAX_PATH );
    pRtlDosPathNameToNtPathName_U( path, &nameW, NULL, NULL );
    attr.Length = sizeof(attr);
    attr.RootDirectory = 0;
    attr.ObjectName = &nameW;
    attr.Attributes = OBJ_CASE_INSENSITIVE;
    attr.SecurityDescriptor = NULL;
    attr.SecurityQualityOfService = NULL;

    status = pNtOpenFile( &dir, SYNCHRONIZE|FILE_LIST_DIRECTORY, &attr, &io,
                          FILE_SHARE_READ|FILE_SHARE_WRITE, FILE_DIRECTORY_FILE|FILE_SYNCHRONOUS_IO_NONALERT );
    ok( !status, "open %s failed %x\n", wine_dbgstr_w(nameW.Buffer), status );
    pRtlFreeUnicodeString( &nameW );

    ZeroMemory( buf, sizeof(buf) );
    U(io).Status = 0xdadadada;
    io.Information = 0xcacacaca;

    status = pNtQueryVolumeInformationFile( dir, &io, buf, sizeof(buf), FileFsAttributeInformation );

    ffai = (FILE_FS_ATTRIBUTE_INFORMATION *)buf;

    ok(status == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %d\n", status);
    ok(U(io).Status == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %d\n", U(io).Status);
    ok(ffai->FileSystemAttributes != 0, "Missing FileSystemAttributes\n");
    ok(ffai->MaximumComponentNameLength != 0, "Missing MaximumComponentNameLength\n");
    ok(ffai->FileSystemNameLength != 0, "Missing FileSystemNameLength\n");

    trace("FileSystemAttributes: %x MaximumComponentNameLength: %x FileSystemName: %s\n",
          ffai->FileSystemAttributes, ffai->MaximumComponentNameLength,
          wine_dbgstr_wn(ffai->FileSystemName, ffai->FileSystemNameLength / sizeof(WCHAR)));

    CloseHandle( dir );
}

static void test_NtCreateFile(void)
{
    static const struct test_data
    {
        DWORD disposition, attrib_in, status, result, attrib_out, needs_cleanup;
    } td[] =
    {
    /* 0*/{ FILE_CREATE, FILE_ATTRIBUTE_READONLY, 0, FILE_CREATED, FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_READONLY, FALSE },
    /* 1*/{ FILE_CREATE, 0, STATUS_OBJECT_NAME_COLLISION, 0, 0, TRUE },
    /* 2*/{ FILE_CREATE, 0, 0, FILE_CREATED, FILE_ATTRIBUTE_ARCHIVE, FALSE },
    /* 3*/{ FILE_OPEN, FILE_ATTRIBUTE_READONLY, 0, FILE_OPENED, FILE_ATTRIBUTE_ARCHIVE, TRUE },
    /* 4*/{ FILE_OPEN, FILE_ATTRIBUTE_READONLY, STATUS_OBJECT_NAME_NOT_FOUND, 0, 0, FALSE },
    /* 5*/{ FILE_OPEN_IF, 0, 0, FILE_CREATED, FILE_ATTRIBUTE_ARCHIVE, FALSE },
    /* 6*/{ FILE_OPEN_IF, FILE_ATTRIBUTE_READONLY, 0, FILE_OPENED, FILE_ATTRIBUTE_ARCHIVE, TRUE },
    /* 7*/{ FILE_OPEN_IF, FILE_ATTRIBUTE_READONLY, 0, FILE_CREATED, FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_READONLY, FALSE },
    /* 8*/{ FILE_OPEN_IF, 0, 0, FILE_OPENED, FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_READONLY, FALSE },
    /* 9*/{ FILE_OVERWRITE, 0, STATUS_ACCESS_DENIED, 0, 0, TRUE },
    /*10*/{ FILE_OVERWRITE, 0, STATUS_OBJECT_NAME_NOT_FOUND, 0, 0, FALSE },
    /*11*/{ FILE_CREATE, 0, 0, FILE_CREATED, FILE_ATTRIBUTE_ARCHIVE, FALSE },
    /*12*/{ FILE_OVERWRITE, FILE_ATTRIBUTE_READONLY, 0, FILE_OVERWRITTEN, FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_READONLY, FALSE },
    /*13*/{ FILE_OVERWRITE_IF, 0, STATUS_ACCESS_DENIED, 0, 0, TRUE },
    /*14*/{ FILE_OVERWRITE_IF, 0, 0, FILE_CREATED, FILE_ATTRIBUTE_ARCHIVE, FALSE },
    /*15*/{ FILE_OVERWRITE_IF, FILE_ATTRIBUTE_READONLY, 0, FILE_OVERWRITTEN, FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_READONLY, FALSE },
    /*16*/{ FILE_SUPERSEDE, 0, 0, FILE_SUPERSEDED, FILE_ATTRIBUTE_ARCHIVE, FALSE },
    /*17*/{ FILE_SUPERSEDE, FILE_ATTRIBUTE_READONLY, 0, FILE_SUPERSEDED, FILE_ATTRIBUTE_ARCHIVE|FILE_ATTRIBUTE_READONLY, TRUE },
    /*18*/{ FILE_SUPERSEDE, 0, 0, FILE_CREATED, FILE_ATTRIBUTE_ARCHIVE, TRUE }
    };
    static const WCHAR fooW[] = {'f','o','o',0};
    NTSTATUS status;
    HANDLE handle;
    WCHAR path[MAX_PATH];
    OBJECT_ATTRIBUTES attr;
    IO_STATUS_BLOCK io;
    UNICODE_STRING nameW;
    DWORD ret, i;

    GetTempPathW(MAX_PATH, path);
    GetTempFileNameW(path, fooW, 0, path);
    DeleteFileW(path);
    pRtlDosPathNameToNtPathName_U(path, &nameW, NULL, NULL);

    attr.Length = sizeof(attr);
    attr.RootDirectory = NULL;
    attr.ObjectName = &nameW;
    attr.Attributes = OBJ_CASE_INSENSITIVE;
    attr.SecurityDescriptor = NULL;
    attr.SecurityQualityOfService = NULL;

    for (i = 0; i < ARRAY_SIZE(td); i++)
    {
        status = pNtCreateFile(&handle, GENERIC_READ, &attr, &io, NULL,
                               td[i].attrib_in, FILE_SHARE_READ|FILE_SHARE_WRITE,
                               td[i].disposition, 0, NULL, 0);

        ok(status == td[i].status, "%d: expected %#x got %#x\n", i, td[i].status, status);

        if (!status)
        {
            ok(io.Information == td[i].result,"%d: expected %#x got %#lx\n", i, td[i].result, io.Information);

            ret = GetFileAttributesW(path);
            ret &= ~FILE_ATTRIBUTE_NOT_CONTENT_INDEXED;
            /* FIXME: leave only 'else' case below once Wine is fixed */
            if (ret != td[i].attrib_out)
            {
            todo_wine
                ok(ret == td[i].attrib_out, "%d: expected %#x got %#x\n", i, td[i].attrib_out, ret);
                SetFileAttributesW(path, td[i].attrib_out);
            }
            else
                ok(ret == td[i].attrib_out, "%d: expected %#x got %#x\n", i, td[i].attrib_out, ret);

            CloseHandle(handle);
        }

        if (td[i].needs_cleanup)
        {
            SetFileAttributesW(path, FILE_ATTRIBUTE_ARCHIVE);
            DeleteFileW(path);
        }
    }

    pRtlFreeUnicodeString( &nameW );
    SetFileAttributesW(path, FILE_ATTRIBUTE_ARCHIVE);
    DeleteFileW( path );
}

static void test_readonly(void)
{
    static const WCHAR fooW[] = {'f','o','o',0};
    NTSTATUS status;
    HANDLE handle;
    WCHAR path[MAX_PATH];
    OBJECT_ATTRIBUTES attr;
    IO_STATUS_BLOCK io;
    UNICODE_STRING nameW;

    GetTempPathW(MAX_PATH, path);
    GetTempFileNameW(path, fooW, 0, path);
    DeleteFileW(path);
    pRtlDosPathNameToNtPathName_U(path, &nameW, NULL, NULL);

    attr.Length = sizeof(attr);
    attr.RootDirectory = NULL;
    attr.ObjectName = &nameW;
    attr.Attributes = OBJ_CASE_INSENSITIVE;
    attr.SecurityDescriptor = NULL;
    attr.SecurityQualityOfService = NULL;

    status = pNtCreateFile(&handle, GENERIC_READ, &attr, &io, NULL, FILE_ATTRIBUTE_READONLY,
                           FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, FILE_CREATE, 0, NULL, 0);
    ok(status == STATUS_SUCCESS, "got %#x\n", status);
    CloseHandle(handle);

    status = pNtOpenFile(&handle, GENERIC_WRITE,  &attr, &io,
                         FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, FILE_OPEN_FOR_BACKUP_INTENT);
    ok(status == STATUS_ACCESS_DENIED, "got %#x\n", status);
    CloseHandle(handle);

    status = pNtOpenFile(&handle, GENERIC_READ,  &attr, &io,
                         FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, FILE_OPEN_FOR_BACKUP_INTENT);
    ok(status == STATUS_SUCCESS, "got %#x\n", status);
    CloseHandle(handle);

    status = pNtOpenFile(&handle, FILE_READ_ATTRIBUTES,  &attr, &io,
                         FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, FILE_OPEN_FOR_BACKUP_INTENT);
    ok(status == STATUS_SUCCESS, "got %#x\n", status);
    CloseHandle(handle);

    status = pNtOpenFile(&handle, FILE_WRITE_ATTRIBUTES,  &attr, &io,
                         FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, FILE_OPEN_FOR_BACKUP_INTENT);
    ok(status == STATUS_SUCCESS, "got %#x\n", status);
    CloseHandle(handle);

    status = pNtOpenFile(&handle, DELETE,  &attr, &io,
                         FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, FILE_OPEN_FOR_BACKUP_INTENT);
    ok(status == STATUS_SUCCESS, "got %#x\n", status);
    CloseHandle(handle);

    status = pNtOpenFile(&handle, READ_CONTROL,  &attr, &io,
                         FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, FILE_OPEN_FOR_BACKUP_INTENT);
    ok(status == STATUS_SUCCESS, "got %#x\n", status);
    CloseHandle(handle);

    status = pNtOpenFile(&handle, WRITE_DAC,  &attr, &io,
                         FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, FILE_OPEN_FOR_BACKUP_INTENT);
    ok(status == STATUS_SUCCESS, "got %#x\n", status);
    CloseHandle(handle);

    status = pNtOpenFile(&handle, WRITE_OWNER,  &attr, &io,
                         FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, FILE_OPEN_FOR_BACKUP_INTENT);
    ok(status == STATUS_SUCCESS, "got %#x\n", status);
    CloseHandle(handle);

    status = pNtOpenFile(&handle, SYNCHRONIZE,  &attr, &io,
                         FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE, FILE_OPEN_FOR_BACKUP_INTENT);
    ok(status == STATUS_SUCCESS, "got %#x\n", status);
    CloseHandle( handle );

    pRtlFreeUnicodeString(&nameW);
    SetFileAttributesW(path, FILE_ATTRIBUTE_ARCHIVE);
    DeleteFileW(path);
}

static void test_read_write(void)
{
    static const char contents[14] = "1234567890abcd";
    char buf[256];
    HANDLE hfile, event;
    OVERLAPPED ovl;
    IO_STATUS_BLOCK iob;
    DWORD ret, bytes, status, off;
    LARGE_INTEGER offset;
    LONG i;

    event = CreateEventA( NULL, TRUE, FALSE, NULL );

    U(iob).Status = -1;
    iob.Information = -1;
    offset.QuadPart = 0;
    status = pNtReadFile(INVALID_HANDLE_VALUE, 0, NULL, NULL, &iob, buf, sizeof(buf), &offset, NULL);
    ok(status == STATUS_OBJECT_TYPE_MISMATCH || status == STATUS_INVALID_HANDLE, "expected STATUS_OBJECT_TYPE_MISMATCH, got %#x\n", status);
    ok(U(iob).Status == -1, "expected -1, got %#x\n", U(iob).Status);
    ok(iob.Information == -1, "expected -1, got %lu\n", iob.Information);

    U(iob).Status = -1;
    iob.Information = -1;
    offset.QuadPart = 0;
    status = pNtReadFile(INVALID_HANDLE_VALUE, 0, NULL, NULL, &iob, NULL, sizeof(buf), &offset, NULL);
    ok(status == STATUS_OBJECT_TYPE_MISMATCH || status == STATUS_INVALID_HANDLE, "expected STATUS_OBJECT_TYPE_MISMATCH, got %#x\n", status);
    ok(U(iob).Status == -1, "expected -1, got %#x\n", U(iob).Status);
    ok(iob.Information == -1, "expected -1, got %lu\n", iob.Information);

    U(iob).Status = -1;
    iob.Information = -1;
    offset.QuadPart = 0;
    status = pNtWriteFile(INVALID_HANDLE_VALUE, 0, NULL, NULL, &iob, buf, sizeof(buf), &offset, NULL);
    ok(status == STATUS_OBJECT_TYPE_MISMATCH || status == STATUS_INVALID_HANDLE, "expected STATUS_OBJECT_TYPE_MISMATCH, got %#x\n", status);
    ok(U(iob).Status == -1, "expected -1, got %#x\n", U(iob).Status);
    ok(iob.Information == -1, "expected -1, got %lu\n", iob.Information);

    U(iob).Status = -1;
    iob.Information = -1;
    offset.QuadPart = 0;
    status = pNtWriteFile(INVALID_HANDLE_VALUE, 0, NULL, NULL, &iob, buf, sizeof(buf), &offset, NULL);
    ok(status == STATUS_OBJECT_TYPE_MISMATCH || status == STATUS_INVALID_HANDLE, "expected STATUS_OBJECT_TYPE_MISMATCH, got %#x\n", status);
    ok(U(iob).Status == -1, "expected -1, got %#x\n", U(iob).Status);
    ok(iob.Information == -1, "expected -1, got %lu\n", iob.Information);

    hfile = create_temp_file(0);
    if (!hfile) return;

    U(iob).Status = -1;
    iob.Information = -1;
    status = pNtWriteFile(hfile, 0, NULL, NULL, &iob, NULL, sizeof(contents), NULL, NULL);
    ok(status == STATUS_INVALID_USER_BUFFER, "expected STATUS_INVALID_USER_BUFFER, got %#x\n", status);
    ok(U(iob).Status == -1, "expected -1, got %#x\n", U(iob).Status);
    ok(iob.Information == -1, "expected -1, got %lu\n", iob.Information);

    U(iob).Status = -1;
    iob.Information = -1;
    SetEvent(event);
    status = pNtWriteFile(hfile, event, NULL, NULL, &iob, NULL, sizeof(contents), NULL, NULL);
    ok(status == STATUS_INVALID_USER_BUFFER, "expected STATUS_INVALID_USER_BUFFER, got %#x\n", status);
    ok(U(iob).Status == -1, "expected -1, got %#x\n", U(iob).Status);
    ok(iob.Information == -1, "expected -1, got %lu\n", iob.Information);
    ok(!is_signaled(event), "event is not signaled\n");

    U(iob).Status = -1;
    iob.Information = -1;
    status = pNtReadFile(hfile, 0, NULL, NULL, &iob, NULL, sizeof(contents), NULL, NULL);
    ok(status == STATUS_ACCESS_VIOLATION, "expected STATUS_ACCESS_VIOLATION, got %#x\n", status);
    ok(U(iob).Status == -1, "expected -1, got %#x\n", U(iob).Status);
    ok(iob.Information == -1, "expected -1, got %lu\n", iob.Information);

    U(iob).Status = -1;
    iob.Information = -1;
    SetEvent(event);
    status = pNtReadFile(hfile, event, NULL, NULL, &iob, NULL, sizeof(contents), NULL, NULL);
    ok(status == STATUS_ACCESS_VIOLATION, "expected STATUS_ACCESS_VIOLATION, got %#x\n", status);
    ok(U(iob).Status == -1, "expected -1, got %#x\n", U(iob).Status);
    ok(iob.Information == -1, "expected -1, got %lu\n", iob.Information);
    ok(is_signaled(event), "event is not signaled\n");

    U(iob).Status = -1;
    iob.Information = -1;
    SetEvent(event);
    status = pNtReadFile(hfile, event, NULL, NULL, &iob, (void*)0xdeadbeef, sizeof(contents), NULL, NULL);
    ok(status == STATUS_ACCESS_VIOLATION, "expected STATUS_ACCESS_VIOLATION, got %#x\n", status);
    ok(U(iob).Status == -1, "expected -1, got %#x\n", U(iob).Status);
    ok(iob.Information == -1, "expected -1, got %lu\n", iob.Information);
    ok(is_signaled(event), "event is not signaled\n");

    U(iob).Status = -1;
    iob.Information = -1;
    status = pNtWriteFile(hfile, 0, NULL, NULL, &iob, contents, 7, NULL, NULL);
    ok(status == STATUS_SUCCESS, "NtWriteFile error %#x\n", status);
    ok(U(iob).Status == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %#x\n", U(iob).Status);
    ok(iob.Information == 7, "expected 7, got %lu\n", iob.Information);

    SetFilePointer(hfile, 0, NULL, FILE_BEGIN);

    U(iob).Status = -1;
    iob.Information = -1;
    offset.QuadPart = (LONGLONG)-1 /* FILE_WRITE_TO_END_OF_FILE */;
    status = pNtWriteFile(hfile, 0, NULL, NULL, &iob, contents + 7, sizeof(contents) - 7, &offset, NULL);
    ok(status == STATUS_SUCCESS, "NtWriteFile error %#x\n", status);
    ok(U(iob).Status == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %#x\n", U(iob).Status);
    ok(iob.Information == sizeof(contents) - 7, "expected sizeof(contents)-7, got %lu\n", iob.Information);

    off = SetFilePointer(hfile, 0, NULL, FILE_CURRENT);
    ok(off == sizeof(contents), "expected sizeof(contents), got %u\n", off);

    bytes = 0xdeadbeef;
    SetLastError(0xdeadbeef);
    ret = ReadFile(INVALID_HANDLE_VALUE, buf, 0, &bytes, NULL);
    ok(!ret, "ReadFile should fail\n");
    ok(GetLastError() == ERROR_INVALID_HANDLE, "expected ERROR_INVALID_HANDLE, got %d\n", GetLastError());
    ok(bytes == 0, "bytes %u\n", bytes);

    bytes = 0xdeadbeef;
    SetLastError(0xdeadbeef);
    ret = ReadFile(hfile, buf, 0, &bytes, NULL);
    ok(ret, "ReadFile error %d\n", GetLastError());
    ok(GetLastError() == 0xdeadbeef, "expected 0xdeadbeef, got %d\n", GetLastError());
    ok(bytes == 0, "bytes %u\n", bytes);

    bytes = 0xdeadbeef;
    SetLastError(0xdeadbeef);
    ret = ReadFile(hfile, buf, sizeof(buf), &bytes, NULL);
    ok(ret, "ReadFile error %d\n", GetLastError());
    ok(GetLastError() == 0xdeadbeef, "expected 0xdeadbeef, got %d\n", GetLastError());
    ok(bytes == 0, "bytes %u\n", bytes);

    SetFilePointer(hfile, 0, NULL, FILE_BEGIN);

    bytes = 0;
    SetLastError(0xdeadbeef);
    ret = ReadFile(hfile, buf, sizeof(buf), &bytes, NULL);
    ok(ret, "ReadFile error %d\n", GetLastError());
    ok(bytes == sizeof(contents), "bytes %u\n", bytes);
    ok(!memcmp(contents, buf, sizeof(contents)), "file contents mismatch\n");

    for (i = -20; i < -1; i++)
    {
        if (i == -2) continue;

        U(iob).Status = -1;
        iob.Information = -1;
        offset.QuadPart = (LONGLONG)i;
        status = pNtWriteFile(hfile, 0, NULL, NULL, &iob, contents, sizeof(contents), &offset, NULL);
        ok(status == STATUS_INVALID_PARAMETER, "%d: expected STATUS_INVALID_PARAMETER, got %#x\n", i, status);
        ok(U(iob).Status == -1, "expected -1, got %#x\n", U(iob).Status);
        ok(iob.Information == -1, "expected -1, got %ld\n", iob.Information);
    }

    SetFilePointer(hfile, sizeof(contents) - 4, NULL, FILE_BEGIN);

    U(iob).Status = -1;
    iob.Information = -1;
    offset.QuadPart = (LONGLONG)-2 /* FILE_USE_FILE_POINTER_POSITION */;
    status = pNtWriteFile(hfile, 0, NULL, NULL, &iob, "DCBA", 4, &offset, NULL);
    ok(status == STATUS_SUCCESS, "NtWriteFile error %#x\n", status);
    ok(U(iob).Status == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %#x\n", U(iob).Status);
    ok(iob.Information == 4, "expected 4, got %lu\n", iob.Information);

    off = SetFilePointer(hfile, 0, NULL, FILE_CURRENT);
    ok(off == sizeof(contents), "expected sizeof(contents), got %u\n", off);

    U(iob).Status = -1;
    iob.Information = -1;
    status = pNtReadFile(hfile, 0, NULL, NULL, &iob, buf, sizeof(buf), NULL, NULL);
    ok(status == STATUS_END_OF_FILE, "expected STATUS_END_OF_FILE, got %#x\n", status);
    ok(U(iob).Status == STATUS_END_OF_FILE, "expected STATUS_END_OF_FILE, got %#x\n", U(iob).Status);
    ok(iob.Information == 0, "expected 0, got %lu\n", iob.Information);

    SetFilePointer(hfile, 0, NULL, FILE_BEGIN);

    bytes = 0;
    SetLastError(0xdeadbeef);
    ret = ReadFile(hfile, buf, sizeof(buf), &bytes, NULL);
    ok(ret, "ReadFile error %d\n", GetLastError());
    ok(bytes == sizeof(contents), "bytes %u\n", bytes);
    ok(!memcmp(contents, buf, sizeof(contents) - 4), "file contents mismatch\n");
    ok(!memcmp(buf + sizeof(contents) - 4, "DCBA", 4), "file contents mismatch\n");

    off = SetFilePointer(hfile, 0, NULL, FILE_CURRENT);
    ok(off == sizeof(contents), "expected sizeof(contents), got %u\n", off);

    SetFilePointer(hfile, 0, NULL, FILE_BEGIN);

    bytes = 0;
    SetLastError(0xdeadbeef);
    ret = WriteFile(hfile, contents, sizeof(contents), &bytes, NULL);
    ok(ret, "WriteFile error %d\n", GetLastError());
    ok(bytes == sizeof(contents), "bytes %u\n", bytes);

    off = SetFilePointer(hfile, 0, NULL, FILE_CURRENT);
    ok(off == sizeof(contents), "expected sizeof(contents), got %u\n", off);

    /* test reading beyond EOF */
    bytes = -1;
    SetLastError(0xdeadbeef);
    ret = ReadFile(hfile, buf, sizeof(buf), &bytes, NULL);
    ok(ret, "ReadFile error %d\n", GetLastError());
    ok(GetLastError() == 0xdeadbeef, "expected 0xdeadbeef, got %d\n", GetLastError());
    ok(bytes == 0, "bytes %u\n", bytes);

    bytes = -1;
    SetLastError(0xdeadbeef);
    ret = ReadFile(hfile, buf, 0, &bytes, NULL);
    ok(ret, "ReadFile error %d\n", GetLastError());
    ok(GetLastError() == 0xdeadbeef, "expected 0xdeadbeef, got %d\n", GetLastError());
    ok(bytes == 0, "bytes %u\n", bytes);

    bytes = -1;
    SetLastError(0xdeadbeef);
    ret = ReadFile(hfile, NULL, 0, &bytes, NULL);
    ok(ret, "ReadFile error %d\n", GetLastError());
    ok(GetLastError() == 0xdeadbeef, "expected 0xdeadbeef, got %d\n", GetLastError());
    ok(bytes == 0, "bytes %u\n", bytes);

    S(U(ovl)).Offset = sizeof(contents);
    S(U(ovl)).OffsetHigh = 0;
    ovl.Internal = -1;
    ovl.InternalHigh = -1;
    ovl.hEvent = 0;
    bytes = -1;
    SetLastError(0xdeadbeef);
    ret = ReadFile(hfile, buf, sizeof(buf), &bytes, &ovl);
    ok(!ret, "ReadFile should fail\n");
    ok(GetLastError() == ERROR_HANDLE_EOF, "expected ERROR_HANDLE_EOF, got %d\n", GetLastError());
    ok(bytes == 0, "bytes %u\n", bytes);
    ok((NTSTATUS)ovl.Internal == STATUS_END_OF_FILE, "expected STATUS_END_OF_FILE, got %#lx\n", ovl.Internal);
    ok(ovl.InternalHigh == 0, "expected 0, got %lu\n", ovl.InternalHigh);

    S(U(ovl)).Offset = sizeof(contents);
    S(U(ovl)).OffsetHigh = 0;
    ovl.Internal = -1;
    ovl.InternalHigh = -1;
    ovl.hEvent = 0;
    bytes = -1;
    SetLastError(0xdeadbeef);
    ret = ReadFile(hfile, buf, 0, &bytes, &ovl);
    ok(ret, "ReadFile error %d\n", GetLastError());
    ok(GetLastError() == 0xdeadbeef, "expected 0xdeadbeef, got %d\n", GetLastError());
    ok(bytes == 0, "bytes %u\n", bytes);
    ok((NTSTATUS)ovl.Internal == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %#lx\n", ovl.Internal);
    ok(ovl.InternalHigh == 0, "expected 0, got %lu\n", ovl.InternalHigh);

    U(iob).Status = -1;
    iob.Information = -1;
    status = pNtReadFile(hfile, 0, NULL, NULL, &iob, buf, sizeof(buf), NULL, NULL);
    ok(status == STATUS_END_OF_FILE, "expected STATUS_END_OF_FILE, got %#x\n", status);
    ok(U(iob).Status == STATUS_END_OF_FILE, "expected STATUS_END_OF_FILE, got %#x\n", U(iob).Status);
    ok(iob.Information == 0, "expected 0, got %lu\n", iob.Information);

    U(iob).Status = -1;
    iob.Information = -1;
    status = pNtReadFile(hfile, 0, NULL, NULL, &iob, buf, 0, NULL, NULL);
    ok(status == STATUS_SUCCESS, "NtReadFile error %#x\n", status);
    ok(U(iob).Status == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %#x\n", U(iob).Status);
    ok(iob.Information == 0, "expected 0, got %lu\n", iob.Information);

    U(iob).Status = -1;
    iob.Information = -1;
    offset.QuadPart = sizeof(contents);
    status = pNtReadFile(hfile, 0, NULL, NULL, &iob, buf, sizeof(buf), &offset, NULL);
    ok(status == STATUS_END_OF_FILE, "expected STATUS_END_OF_FILE, got %#x\n", status);
    ok(U(iob).Status == STATUS_END_OF_FILE, "expected STATUS_END_OF_FILE, got %#x\n", U(iob).Status);
    ok(iob.Information == 0, "expected 0, got %lu\n", iob.Information);

    U(iob).Status = -1;
    iob.Information = -1;
    offset.QuadPart = sizeof(contents);
    status = pNtReadFile(hfile, 0, NULL, NULL, &iob, buf, 0, &offset, NULL);
    ok(status == STATUS_SUCCESS, "NtReadFile error %#x\n", status);
    ok(U(iob).Status == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %#x\n", U(iob).Status);
    ok(iob.Information == 0, "expected 0, got %lu\n", iob.Information);

    U(iob).Status = -1;
    iob.Information = -1;
    offset.QuadPart = (LONGLONG)-2 /* FILE_USE_FILE_POINTER_POSITION */;
    status = pNtReadFile(hfile, 0, NULL, NULL, &iob, buf, sizeof(buf), &offset, NULL);
    ok(status == STATUS_END_OF_FILE, "expected STATUS_END_OF_FILE, got %#x\n", status);
    ok(U(iob).Status == STATUS_END_OF_FILE, "expected STATUS_END_OF_FILE, got %#x\n", U(iob).Status);
    ok(iob.Information == 0, "expected 0, got %lu\n", iob.Information);

    U(iob).Status = -1;
    iob.Information = -1;
    offset.QuadPart = (LONGLONG)-2 /* FILE_USE_FILE_POINTER_POSITION */;
    status = pNtReadFile(hfile, 0, NULL, NULL, &iob, buf, 0, &offset, NULL);
    ok(status == STATUS_SUCCESS, "NtReadFile error %#x\n", status);
    ok(U(iob).Status == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %#x\n", U(iob).Status);
    ok(iob.Information == 0, "expected 0, got %lu\n", iob.Information);

    for (i = -20; i < 0; i++)
    {
        if (i == -2) continue;

        U(iob).Status = -1;
        iob.Information = -1;
        offset.QuadPart = (LONGLONG)i;
        status = pNtReadFile(hfile, 0, NULL, NULL, &iob, buf, sizeof(buf), &offset, NULL);
        ok(status == STATUS_INVALID_PARAMETER, "%d: expected STATUS_INVALID_PARAMETER, got %#x\n", i, status);
        ok(U(iob).Status == -1, "expected -1, got %#x\n", U(iob).Status);
        ok(iob.Information == -1, "expected -1, got %ld\n", iob.Information);
    }

    SetFilePointer(hfile, 0, NULL, FILE_BEGIN);

    bytes = 0;
    SetLastError(0xdeadbeef);
    ret = ReadFile(hfile, buf, sizeof(buf), &bytes, NULL);
    ok(ret, "ReadFile error %d\n", GetLastError());
    ok(bytes == sizeof(contents), "bytes %u\n", bytes);
    ok(!memcmp(contents, buf, sizeof(contents)), "file contents mismatch\n");

    off = SetFilePointer(hfile, 0, NULL, FILE_CURRENT);
    ok(off == sizeof(contents), "expected sizeof(contents), got %u\n", off);

    U(iob).Status = -1;
    iob.Information = -1;
    offset.QuadPart = 0;
    status = pNtReadFile(hfile, 0, NULL, NULL, &iob, buf, sizeof(buf), &offset, NULL);
    ok(status == STATUS_SUCCESS, "NtReadFile error %#x\n", status);
    ok(U(iob).Status == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %#x\n", U(iob).Status);
    ok(iob.Information == sizeof(contents), "expected sizeof(contents), got %lu\n", iob.Information);
    ok(!memcmp(contents, buf, sizeof(contents)), "file contents mismatch\n");

    off = SetFilePointer(hfile, 0, NULL, FILE_CURRENT);
    ok(off == sizeof(contents), "expected sizeof(contents), got %u\n", off);

    U(iob).Status = -1;
    iob.Information = -1;
    offset.QuadPart = sizeof(contents) - 4;
    status = pNtWriteFile(hfile, 0, NULL, NULL, &iob, "DCBA", 4, &offset, NULL);
    ok(status == STATUS_SUCCESS, "NtWriteFile error %#x\n", status);
    ok(U(iob).Status == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %#x\n", U(iob).Status);
    ok(iob.Information == 4, "expected 4, got %lu\n", iob.Information);

    off = SetFilePointer(hfile, 0, NULL, FILE_CURRENT);
    ok(off == sizeof(contents), "expected sizeof(contents), got %u\n", off);

    U(iob).Status = -1;
    iob.Information = -1;
    offset.QuadPart = 0;
    status = pNtReadFile(hfile, 0, NULL, NULL, &iob, buf, sizeof(buf), &offset, NULL);
    ok(status == STATUS_SUCCESS, "NtReadFile error %#x\n", status);
    ok(U(iob).Status == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %#x\n", U(iob).Status);
    ok(iob.Information == sizeof(contents), "expected sizeof(contents), got %lu\n", iob.Information);
    ok(!memcmp(contents, buf, sizeof(contents) - 4), "file contents mismatch\n");
    ok(!memcmp(buf + sizeof(contents) - 4, "DCBA", 4), "file contents mismatch\n");

    off = SetFilePointer(hfile, 0, NULL, FILE_CURRENT);
    ok(off == sizeof(contents), "expected sizeof(contents), got %u\n", off);

    S(U(ovl)).Offset = sizeof(contents) - 4;
    S(U(ovl)).OffsetHigh = 0;
    ovl.hEvent = 0;
    bytes = 0;
    SetLastError(0xdeadbeef);
    ret = WriteFile(hfile, "ABCD", 4, &bytes, &ovl);
    ok(ret, "WriteFile error %d\n", GetLastError());
    ok(bytes == 4, "bytes %u\n", bytes);

    off = SetFilePointer(hfile, 0, NULL, FILE_CURRENT);
    ok(off == sizeof(contents), "expected sizeof(contents), got %u\n", off);

    S(U(ovl)).Offset = 0;
    S(U(ovl)).OffsetHigh = 0;
    ovl.Internal = -1;
    ovl.InternalHigh = -1;
    ovl.hEvent = 0;
    bytes = 0;
    SetLastError(0xdeadbeef);
    ret = ReadFile(hfile, buf, sizeof(buf), &bytes, &ovl);
    ok(ret, "ReadFile error %d\n", GetLastError());
    ok(bytes == sizeof(contents), "bytes %u\n", bytes);
    ok((NTSTATUS)ovl.Internal == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %#lx\n", ovl.Internal);
    ok(ovl.InternalHigh == sizeof(contents), "expected sizeof(contents), got %lu\n", ovl.InternalHigh);
    ok(!memcmp(contents, buf, sizeof(contents) - 4), "file contents mismatch\n");
    ok(!memcmp(buf + sizeof(contents) - 4, "ABCD", 4), "file contents mismatch\n");

    off = SetFilePointer(hfile, 0, NULL, FILE_CURRENT);
    ok(off == sizeof(contents), "expected sizeof(contents), got %u\n", off);

    CloseHandle(hfile);

    hfile = create_temp_file(FILE_FLAG_OVERLAPPED);
    if (!hfile) return;

    bytes = 0xdeadbeef;
    SetLastError(0xdeadbeef);
    ret = ReadFile(INVALID_HANDLE_VALUE, buf, 0, &bytes, NULL);
    ok(!ret, "ReadFile should fail\n");
    ok(GetLastError() == ERROR_INVALID_HANDLE, "expected ERROR_INVALID_HANDLE, got %d\n", GetLastError());
    ok(bytes == 0, "bytes %u\n", bytes);

    S(U(ovl)).Offset = 0;
    S(U(ovl)).OffsetHigh = 0;
    ovl.Internal = -1;
    ovl.InternalHigh = -1;
    ovl.hEvent = 0;
    bytes = 0xdeadbeef;
    SetLastError(0xdeadbeef);
    /* ReadFile return value depends on Windows version and testing it is not practical */
    ReadFile(hfile, buf, 0, &bytes, &ovl);
    ok(bytes == 0, "bytes %u\n", bytes);
    ok((NTSTATUS)ovl.Internal == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %#lx\n", ovl.Internal);
    ok(ovl.InternalHigh == 0, "expected 0, got %lu\n", ovl.InternalHigh);

    bytes = 0xdeadbeef;
    SetLastError(0xdeadbeef);
    ret = WriteFile(hfile, contents, sizeof(contents), &bytes, NULL);
    ok(!ret, "WriteFile should fail\n");
    ok(GetLastError() == ERROR_INVALID_PARAMETER, "expected ERROR_INVALID_PARAMETER, got %d\n", GetLastError());
    ok(bytes == 0, "bytes %u\n", bytes);

    U(iob).Status = -1;
    iob.Information = -1;
    status = pNtWriteFile(hfile, 0, NULL, NULL, &iob, contents, sizeof(contents), NULL, NULL);
    ok(status == STATUS_INVALID_PARAMETER, "expected STATUS_INVALID_PARAMETER, got %#x\n", status);
    ok(U(iob).Status == -1, "expected -1, got %#x\n", U(iob).Status);
    ok(iob.Information == -1, "expected -1, got %ld\n", iob.Information);

    for (i = -20; i < -1; i++)
    {
        U(iob).Status = -1;
        iob.Information = -1;
        offset.QuadPart = (LONGLONG)i;
        status = pNtWriteFile(hfile, 0, NULL, NULL, &iob, contents, sizeof(contents), &offset, NULL);
        ok(status == STATUS_INVALID_PARAMETER, "%d: expected STATUS_INVALID_PARAMETER, got %#x\n", i, status);
        ok(U(iob).Status == -1, "expected -1, got %#x\n", U(iob).Status);
        ok(iob.Information == -1, "expected -1, got %ld\n", iob.Information);
    }

    U(iob).Status = -1;
    iob.Information = -1;
    offset.QuadPart = 0;
    status = pNtWriteFile(hfile, 0, NULL, NULL, &iob, contents, sizeof(contents), &offset, NULL);
    ok(status == STATUS_PENDING || broken(status == STATUS_SUCCESS) /* before Vista */,
            "expected STATUS_PENDING, got %#x.\n", status);
    if (status == STATUS_PENDING)
    {
        ret = WaitForSingleObject(hfile, 3000);
        ok(ret == WAIT_OBJECT_0, "WaitForSingleObject error %d\n", ret);
    }
    ok(U(iob).Status == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %#x\n", U(iob).Status);
    ok(iob.Information == sizeof(contents), "expected sizeof(contents), got %lu\n", iob.Information);

    off = SetFilePointer(hfile, 0, NULL, FILE_CURRENT);
    ok(off == 0, "expected 0, got %u\n", off);

    bytes = 0xdeadbeef;
    SetLastError(0xdeadbeef);
    ret = ReadFile(hfile, buf, sizeof(buf), &bytes, NULL);
    ok(!ret, "ReadFile should fail\n");
    ok(GetLastError() == ERROR_INVALID_PARAMETER, "expected ERROR_INVALID_PARAMETER, got %d\n", GetLastError());
    ok(bytes == 0, "bytes %u\n", bytes);

    U(iob).Status = -1;
    iob.Information = -1;
    status = pNtReadFile(hfile, 0, NULL, NULL, &iob, buf, sizeof(buf), NULL, NULL);
    ok(status == STATUS_INVALID_PARAMETER, "expected STATUS_INVALID_PARAMETER, got %#x\n", status);
    ok(U(iob).Status == -1, "expected -1, got %#x\n", U(iob).Status);
    ok(iob.Information == -1, "expected -1, got %ld\n", iob.Information);

    for (i = -20; i < 0; i++)
    {
        U(iob).Status = -1;
        iob.Information = -1;
        offset.QuadPart = (LONGLONG)i;
        status = pNtReadFile(hfile, 0, NULL, NULL, &iob, buf, sizeof(buf), &offset, NULL);
        ok(status == STATUS_INVALID_PARAMETER, "%d: expected STATUS_INVALID_PARAMETER, got %#x\n", i, status);
        ok(U(iob).Status == -1, "expected -1, got %#x\n", U(iob).Status);
        ok(iob.Information == -1, "expected -1, got %ld\n", iob.Information);
    }

    off = SetFilePointer(hfile, 0, NULL, FILE_CURRENT);
    ok(off == 0, "expected 0, got %u\n", off);

    /* test reading beyond EOF */
    offset.QuadPart = sizeof(contents);
    S(U(ovl)).Offset = offset.u.LowPart;
    S(U(ovl)).OffsetHigh = offset.u.HighPart;
    ovl.Internal = -1;
    ovl.InternalHigh = -1;
    ovl.hEvent = 0;
    bytes = 0xdeadbeef;
    SetLastError(0xdeadbeef);
    ret = ReadFile(hfile, buf, sizeof(buf), &bytes, &ovl);
    ok(!ret, "ReadFile should fail\n");
    ret = GetLastError();
    ok(ret == ERROR_IO_PENDING || broken(ret == ERROR_HANDLE_EOF) /* before Vista */,
            "expected ERROR_IO_PENDING, got %d\n", ret);
    ok(bytes == 0, "bytes %u\n", bytes);

    off = SetFilePointer(hfile, 0, NULL, FILE_CURRENT);
    ok(off == 0, "expected 0, got %u\n", off);

    if (ret == ERROR_IO_PENDING)
    {
        bytes = 0xdeadbeef;
        SetLastError(0xdeadbeef);
        ret = GetOverlappedResult(hfile, &ovl, &bytes, TRUE);
        ok(!ret, "GetOverlappedResult should report FALSE\n");
        ok(GetLastError() == ERROR_HANDLE_EOF, "expected ERROR_HANDLE_EOF, got %d\n", GetLastError());
        ok(bytes == 0, "expected 0, read %u\n", bytes);
        ok((NTSTATUS)ovl.Internal == STATUS_END_OF_FILE, "expected STATUS_END_OF_FILE, got %#lx\n", ovl.Internal);
        ok(ovl.InternalHigh == 0, "expected 0, got %lu\n", ovl.InternalHigh);
    }

    off = SetFilePointer(hfile, 0, NULL, FILE_CURRENT);
    ok(off == 0, "expected 0, got %u\n", off);

    offset.QuadPart = sizeof(contents);
    S(U(ovl)).Offset = offset.u.LowPart;
    S(U(ovl)).OffsetHigh = offset.u.HighPart;
    ovl.Internal = -1;
    ovl.InternalHigh = -1;
    ovl.hEvent = 0;
    bytes = 0xdeadbeef;
    SetLastError(0xdeadbeef);
    ret = ReadFile(hfile, buf, 0, &bytes, &ovl);
    ok((!ret && GetLastError() == ERROR_IO_PENDING) || broken(ret) /* before Vista */,
            "Unexpected result, ret %#x, GetLastError() %u.\n", ret, GetLastError());
    ret = GetLastError();
    ok(bytes == 0, "bytes %u\n", bytes);

    off = SetFilePointer(hfile, 0, NULL, FILE_CURRENT);
    ok(off == 0, "expected 0, got %u\n", off);

    if (ret == ERROR_IO_PENDING)
    {
        bytes = 0xdeadbeef;
        SetLastError(0xdeadbeef);
        ret = GetOverlappedResult(hfile, &ovl, &bytes, TRUE);
        ok(ret, "GetOverlappedResult returned FALSE with %u (expected TRUE)\n", GetLastError());
        ok(bytes == 0, "expected 0, read %u\n", bytes);
        ok((NTSTATUS)ovl.Internal == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %#lx\n", ovl.Internal);
        ok(ovl.InternalHigh == 0, "expected 0, got %lu\n", ovl.InternalHigh);
    }

    offset.QuadPart = sizeof(contents);
    S(U(ovl)).Offset = offset.u.LowPart;
    S(U(ovl)).OffsetHigh = offset.u.HighPart;
    ovl.Internal = -1;
    ovl.InternalHigh = -1;
    ovl.hEvent = 0;
    bytes = 0xdeadbeef;
    SetLastError(0xdeadbeef);
    ret = ReadFile(hfile, NULL, 0, &bytes, &ovl);
    ok((!ret && GetLastError() == ERROR_IO_PENDING) || broken(ret) /* before Vista */,
            "Unexpected result, ret %#x, GetLastError() %u.\n", ret, GetLastError());
    ret = GetLastError();
    ok(bytes == 0, "bytes %u\n", bytes);

    off = SetFilePointer(hfile, 0, NULL, FILE_CURRENT);
    ok(off == 0, "expected 0, got %u\n", off);

    if (ret == ERROR_IO_PENDING)
    {
        bytes = 0xdeadbeef;
        SetLastError(0xdeadbeef);
        ret = GetOverlappedResult(hfile, &ovl, &bytes, TRUE);
        ok(ret, "GetOverlappedResult returned FALSE with %u (expected TRUE)\n", GetLastError());
        ok(bytes == 0, "expected 0, read %u\n", bytes);
        ok((NTSTATUS)ovl.Internal == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %#lx\n", ovl.Internal);
        ok(ovl.InternalHigh == 0, "expected 0, got %lu\n", ovl.InternalHigh);
    }

    U(iob).Status = -1;
    iob.Information = -1;
    offset.QuadPart = sizeof(contents);
    status = pNtReadFile(hfile, 0, NULL, NULL, &iob, buf, sizeof(buf), &offset, NULL);
    if (status == STATUS_PENDING)
    {
        ret = WaitForSingleObject(hfile, 3000);
        ok(ret == WAIT_OBJECT_0, "WaitForSingleObject error %d\n", ret);
        ok(U(iob).Status == STATUS_END_OF_FILE, "expected STATUS_END_OF_FILE, got %#x\n", U(iob).Status);
        ok(iob.Information == 0, "expected 0, got %lu\n", iob.Information);
    }
    else
    {
        ok(status == STATUS_END_OF_FILE, "expected STATUS_END_OF_FILE, got %#x\n", status);
        ok(U(iob).Status == -1, "expected -1, got %#x\n", U(iob).Status);
        ok(iob.Information == -1, "expected -1, got %lu\n", iob.Information);
    }

    off = SetFilePointer(hfile, 0, NULL, FILE_CURRENT);
    ok(off == 0, "expected 0, got %u\n", off);

    U(iob).Status = -1;
    iob.Information = -1;
    offset.QuadPart = sizeof(contents);
    status = pNtReadFile(hfile, 0, NULL, NULL, &iob, buf, 0, &offset, NULL);
    ok(status == STATUS_PENDING || broken(status == STATUS_SUCCESS) /* before Vista */,
            "expected STATUS_PENDING, got %#x.\n", status);
    if (status == STATUS_PENDING)
    {
        ret = WaitForSingleObject(hfile, 3000);
        ok(ret == WAIT_OBJECT_0, "WaitForSingleObject error %d\n", ret);
        ok(U(iob).Status == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %#x\n", U(iob).Status);
        ok(iob.Information == 0, "expected 0, got %lu\n", iob.Information);
    }
    else
    {
        ok(U(iob).Status == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %#x\n", U(iob).Status);
        ok(iob.Information == 0, "expected 0, got %lu\n", iob.Information);
    }

    off = SetFilePointer(hfile, 0, NULL, FILE_CURRENT);
    ok(off == 0, "expected 0, got %u\n", off);

    S(U(ovl)).Offset = 0;
    S(U(ovl)).OffsetHigh = 0;
    ovl.Internal = -1;
    ovl.InternalHigh = -1;
    ovl.hEvent = 0;
    bytes = 0;
    SetLastError(0xdeadbeef);
    ret = ReadFile(hfile, buf, sizeof(buf), &bytes, &ovl);
    ok((!ret && GetLastError() == ERROR_IO_PENDING) || broken(ret) /* before Vista */,
            "Unexpected result, ret %#x, GetLastError() %u.\n", ret, GetLastError());
    if (!ret)
        ok(bytes == 0, "bytes %u\n", bytes);
    else
        ok(bytes == 14, "bytes %u\n", bytes);
    ok((NTSTATUS)ovl.Internal == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %#lx\n", ovl.Internal);
    ok(ovl.InternalHigh == sizeof(contents), "expected sizeof(contents), got %lu\n", ovl.InternalHigh);

    off = SetFilePointer(hfile, 0, NULL, FILE_CURRENT);
    ok(off == 0, "expected 0, got %u\n", off);

    bytes = 0xdeadbeef;
    ret = GetOverlappedResult(hfile, &ovl, &bytes, TRUE);
    ok(ret, "GetOverlappedResult error %d\n", GetLastError());
    ok(bytes == sizeof(contents), "bytes %u\n", bytes);
    ok((NTSTATUS)ovl.Internal == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %#lx\n", ovl.Internal);
    ok(ovl.InternalHigh == sizeof(contents), "expected sizeof(contents), got %lu\n", ovl.InternalHigh);
    ok(!memcmp(contents, buf, sizeof(contents)), "file contents mismatch\n");

    off = SetFilePointer(hfile, 0, NULL, FILE_CURRENT);
    ok(off == 0, "expected 0, got %u\n", off);

    SetFilePointer(hfile, sizeof(contents) - 4, NULL, FILE_BEGIN);
    SetEndOfFile(hfile);
    SetFilePointer(hfile, 0, NULL, FILE_BEGIN);

    U(iob).Status = -1;
    iob.Information = -1;
    offset.QuadPart = (LONGLONG)-1 /* FILE_WRITE_TO_END_OF_FILE */;
    status = pNtWriteFile(hfile, 0, NULL, NULL, &iob, "DCBA", 4, &offset, NULL);
    ok(status == STATUS_PENDING || broken(status == STATUS_SUCCESS) /* before Vista */,
            "expected STATUS_PENDING, got %#x.\n", status);
    if (status == STATUS_PENDING)
    {
        ret = WaitForSingleObject(hfile, 3000);
        ok(ret == WAIT_OBJECT_0, "WaitForSingleObject error %d\n", ret);
    }
    ok(U(iob).Status == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %#x\n", U(iob).Status);
    ok(iob.Information == 4, "expected 4, got %lu\n", iob.Information);

    off = SetFilePointer(hfile, 0, NULL, FILE_CURRENT);
    ok(off == 0, "expected 0, got %u\n", off);

    U(iob).Status = -1;
    iob.Information = -1;
    offset.QuadPart = 0;
    status = pNtReadFile(hfile, 0, NULL, NULL, &iob, buf, sizeof(buf), &offset, NULL);
    ok(status == STATUS_PENDING || broken(status == STATUS_SUCCESS) /* before Vista */,
            "expected STATUS_PENDING, got %#x.\n", status);
    if (status == STATUS_PENDING)
    {
        ret = WaitForSingleObject(hfile, 3000);
        ok(ret == WAIT_OBJECT_0, "WaitForSingleObject error %d\n", ret);
    }
    ok(U(iob).Status == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %#x\n", U(iob).Status);
    ok(iob.Information == sizeof(contents), "expected sizeof(contents), got %lu\n", iob.Information);

    off = SetFilePointer(hfile, 0, NULL, FILE_CURRENT);
    ok(off == 0, "expected 0, got %u\n", off);

    ok(!memcmp(contents, buf, sizeof(contents) - 4), "file contents mismatch\n");
    ok(!memcmp(buf + sizeof(contents) - 4, "DCBA", 4), "file contents mismatch\n");

    off = SetFilePointer(hfile, 0, NULL, FILE_CURRENT);
    ok(off == 0, "expected 0, got %u\n", off);

    S(U(ovl)).Offset = sizeof(contents) - 4;
    S(U(ovl)).OffsetHigh = 0;
    ovl.Internal = -1;
    ovl.InternalHigh = -1;
    ovl.hEvent = 0;
    bytes = 0;
    SetLastError(0xdeadbeef);
    ret = WriteFile(hfile, "ABCD", 4, &bytes, &ovl);
    ok((!ret && GetLastError() == ERROR_IO_PENDING) || broken(ret) /* before Vista */,
            "Unexpected result %#x, GetLastError() %u.\n", ret, GetLastError());
    if (!ret)
    {
        ok(bytes == 0, "bytes %u\n", bytes);
        ret = WaitForSingleObject(hfile, 3000);
        ok(ret == WAIT_OBJECT_0, "WaitForSingleObject error %d\n", ret);
    }
    else ok(bytes == 4, "bytes %u\n", bytes);
    ok((NTSTATUS)ovl.Internal == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %#lx\n", ovl.Internal);
    ok(ovl.InternalHigh == 4, "expected 4, got %lu\n", ovl.InternalHigh);

    off = SetFilePointer(hfile, 0, NULL, FILE_CURRENT);
    ok(off == 0, "expected 0, got %u\n", off);

    bytes = 0xdeadbeef;
    ret = GetOverlappedResult(hfile, &ovl, &bytes, TRUE);
    ok(ret, "GetOverlappedResult error %d\n", GetLastError());
    ok(bytes == 4, "bytes %u\n", bytes);
    ok((NTSTATUS)ovl.Internal == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %#lx\n", ovl.Internal);
    ok(ovl.InternalHigh == 4, "expected 4, got %lu\n", ovl.InternalHigh);

    off = SetFilePointer(hfile, 0, NULL, FILE_CURRENT);
    ok(off == 0, "expected 0, got %u\n", off);

    S(U(ovl)).Offset = 0;
    S(U(ovl)).OffsetHigh = 0;
    ovl.Internal = -1;
    ovl.InternalHigh = -1;
    ovl.hEvent = 0;
    bytes = 0;
    SetLastError(0xdeadbeef);
    ret = ReadFile(hfile, buf, sizeof(buf), &bytes, &ovl);
    ok((!ret && GetLastError() == ERROR_IO_PENDING) || broken(ret) /* before Vista */,
            "Unexpected result %#x, GetLastError() %u.\n", ret, GetLastError());
    if (!ret)
    {
        ok(bytes == 0, "bytes %u\n", bytes);
        ret = WaitForSingleObject(hfile, 3000);
        ok(ret == WAIT_OBJECT_0, "WaitForSingleObject error %d\n", ret);
    }
    else ok(bytes == 14, "bytes %u\n", bytes);
    ok((NTSTATUS)ovl.Internal == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %#lx\n", ovl.Internal);
    ok(ovl.InternalHigh == sizeof(contents), "expected sizeof(contents), got %lu\n", ovl.InternalHigh);

    off = SetFilePointer(hfile, 0, NULL, FILE_CURRENT);
    ok(off == 0, "expected 0, got %u\n", off);

    bytes = 0xdeadbeef;
    ret = GetOverlappedResult(hfile, &ovl, &bytes, TRUE);
    ok(ret, "GetOverlappedResult error %d\n", GetLastError());
    ok(bytes == sizeof(contents), "bytes %u\n", bytes);
    ok((NTSTATUS)ovl.Internal == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %#lx\n", ovl.Internal);
    ok(ovl.InternalHigh == sizeof(contents), "expected sizeof(contents), got %lu\n", ovl.InternalHigh);
    ok(!memcmp(contents, buf, sizeof(contents) - 4), "file contents mismatch\n");
    ok(!memcmp(buf + sizeof(contents) - 4, "ABCD", 4), "file contents mismatch\n");

    off = SetFilePointer(hfile, 0, NULL, FILE_CURRENT);
    ok(off == 0, "expected 0, got %u\n", off);

    CloseHandle(event);
    CloseHandle(hfile);
}

static void test_ioctl(void)
{
    HANDLE event = CreateEventA(NULL, TRUE, FALSE, NULL);
    FILE_PIPE_PEEK_BUFFER peek_buf;
    IO_STATUS_BLOCK iosb;
    HANDLE file;
    NTSTATUS status;

    file = create_temp_file(FILE_FLAG_OVERLAPPED);
    ok(file != INVALID_HANDLE_VALUE, "could not create temp file\n");

    SetEvent(event);
    status = pNtFsControlFile(file, event, NULL, NULL, &iosb, 0xdeadbeef, 0, 0, 0, 0);
    todo_wine
    ok(status == STATUS_INVALID_DEVICE_REQUEST, "NtFsControlFile returned %x\n", status);
    ok(!is_signaled(event), "event is signaled\n");

    status = pNtFsControlFile(file, (HANDLE)0xdeadbeef, NULL, NULL, &iosb, 0xdeadbeef, 0, 0, 0, 0);
    ok(status == STATUS_INVALID_HANDLE, "NtFsControlFile returned %x\n", status);

    memset(&iosb, 0x55, sizeof(iosb));
    status = pNtFsControlFile(file, NULL, NULL, NULL, &iosb, FSCTL_PIPE_PEEK, NULL, 0,
                             &peek_buf, sizeof(peek_buf));
    todo_wine
    ok(status == STATUS_INVALID_DEVICE_REQUEST, "NtFsControlFile failed: %x\n", status);
    ok(iosb.Status == 0x55555555, "iosb.Status = %x\n", iosb.Status);

    CloseHandle(event);
    CloseHandle(file);
}

static void test_flush_buffers_file(void)
{
    char path[MAX_PATH], buffer[MAX_PATH];
    HANDLE hfile, hfileread;
    NTSTATUS status;
    IO_STATUS_BLOCK io_status_block;

    GetTempPathA(MAX_PATH, path);
    GetTempFileNameA(path, "foo", 0, buffer);
    hfile = CreateFileA(buffer, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL, 0);
    ok(hfile != INVALID_HANDLE_VALUE, "failed to create temp file.\n" );

    hfileread = CreateFileA(buffer, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
            OPEN_EXISTING, 0, NULL);
    ok(hfileread != INVALID_HANDLE_VALUE, "could not open temp file, error %d.\n", GetLastError());

    status = pNtFlushBuffersFile(hfile, NULL);
    ok(status == STATUS_ACCESS_VIOLATION, "expected STATUS_ACCESS_VIOLATION, got %#x.\n", status);

    status = pNtFlushBuffersFile(hfile, (IO_STATUS_BLOCK *)0xdeadbeaf);
    ok(status == STATUS_ACCESS_VIOLATION, "expected STATUS_ACCESS_VIOLATION, got %#x.\n", status);

    io_status_block.Information = 0xdeadbeef;
    io_status_block.Status = 0xdeadbeef;
    status = pNtFlushBuffersFile(hfile, &io_status_block);
    ok(status == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %#x.\n", status);
    ok(io_status_block.Status == STATUS_SUCCESS, "Got unexpected io_status_block.Status %#x.\n",
            io_status_block.Status);
    ok(!io_status_block.Information, "Got unexpected io_status_block.Information %#lx.\n",
            io_status_block.Information);

    status = pNtFlushBuffersFile(hfileread, &io_status_block);
    ok(status == STATUS_ACCESS_DENIED, "expected STATUS_ACCESS_DENIED, got %#x.\n", status);

    io_status_block.Information = 0xdeadbeef;
    io_status_block.Status = 0xdeadbeef;
    status = pNtFlushBuffersFile(NULL, &io_status_block);
    ok(status == STATUS_INVALID_HANDLE, "expected STATUS_INVALID_HANDLE, got %#x.\n", status);
    ok(io_status_block.Status == 0xdeadbeef, "Got unexpected io_status_block.Status %#x.\n",
            io_status_block.Status);
    ok(io_status_block.Information == 0xdeadbeef, "Got unexpected io_status_block.Information %#lx.\n",
            io_status_block.Information);

    CloseHandle(hfileread);
    CloseHandle(hfile);
    hfile = CreateFileA(buffer, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
            OPEN_EXISTING, 0, NULL);
    ok(hfile != INVALID_HANDLE_VALUE, "could not open temp file, error %d.\n", GetLastError());

    status = pNtFlushBuffersFile(hfile, &io_status_block);
    ok(status == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %#x.\n", status);

    io_status_block.Information = 0xdeadbeef;
    io_status_block.Status = 0xdeadbeef;
    status = pNtFlushBuffersFile((HANDLE)0xdeadbeef, &io_status_block);
    ok(status == STATUS_INVALID_HANDLE, "expected STATUS_INVALID_HANDLE, got %#x.\n", status);
    ok(io_status_block.Status == 0xdeadbeef, "Got unexpected io_status_block.Status %#x.\n",
            io_status_block.Status);
    ok(io_status_block.Information == 0xdeadbeef, "Got unexpected io_status_block.Information %#lx.\n",
            io_status_block.Information);

    CloseHandle(hfile);
    DeleteFileA(buffer);
}

static void test_query_ea(void)
{
    #define EA_BUFFER_SIZE 4097
    unsigned char data[EA_BUFFER_SIZE + 8];
    unsigned char *buffer = (void *)(((DWORD_PTR)data + 7) & ~7);
    DWORD buffer_len, i;
    IO_STATUS_BLOCK io;
    NTSTATUS status;
    HANDLE handle;

    if (!(handle = create_temp_file(0))) return;

    /* test with INVALID_HANDLE_VALUE */
    U(io).Status = 0xdeadbeef;
    io.Information = 0xdeadbeef;
    memset(buffer, 0xcc, EA_BUFFER_SIZE);
    buffer_len = EA_BUFFER_SIZE - 1;
    status = pNtQueryEaFile(INVALID_HANDLE_VALUE, &io, buffer, buffer_len, TRUE, NULL, 0, NULL, FALSE);
    ok(status == STATUS_OBJECT_TYPE_MISMATCH, "expected STATUS_OBJECT_TYPE_MISMATCH, got %x\n", status);
    ok(U(io).Status == 0xdeadbeef, "expected 0xdeadbeef, got %x\n", U(io).Status);
    ok(io.Information == 0xdeadbeef, "expected 0xdeadbeef, got %lu\n", io.Information);
    ok(buffer[0] == 0xcc, "data at position 0 overwritten\n");

    /* test with 0xdeadbeef */
    U(io).Status = 0xdeadbeef;
    io.Information = 0xdeadbeef;
    memset(buffer, 0xcc, EA_BUFFER_SIZE);
    buffer_len = EA_BUFFER_SIZE - 1;
    status = pNtQueryEaFile((void *)0xdeadbeef, &io, buffer, buffer_len, TRUE, NULL, 0, NULL, FALSE);
    ok(status == STATUS_INVALID_HANDLE, "expected STATUS_INVALID_HANDLE, got %x\n", status);
    ok(U(io).Status == 0xdeadbeef, "expected 0xdeadbeef, got %x\n", U(io).Status);
    ok(io.Information == 0xdeadbeef, "expected 0xdeadbeef, got %lu\n", io.Information);
    ok(buffer[0] == 0xcc, "data at position 0 overwritten\n");

    /* test without buffer */
    U(io).Status = 0xdeadbeef;
    io.Information = 0xdeadbeef;
    status = pNtQueryEaFile(handle, &io, NULL, 0, TRUE, NULL, 0, NULL, FALSE);
    ok(status == STATUS_NO_EAS_ON_FILE, "expected STATUS_NO_EAS_ON_FILE, got %x\n", status);
    ok(U(io).Status == 0xdeadbeef, "expected 0xdeadbeef, got %x\n", U(io).Status);
    ok(io.Information == 0xdeadbeef, "expected 0xdeadbeef, got %lu\n", io.Information);

    /* test with zero buffer */
    U(io).Status = 0xdeadbeef;
    io.Information = 0xdeadbeef;
    status = pNtQueryEaFile(handle, &io, buffer, 0, TRUE, NULL, 0, NULL, FALSE);
    ok(status == STATUS_NO_EAS_ON_FILE, "expected STATUS_NO_EAS_ON_FILE, got %x\n", status);
    ok(U(io).Status == 0xdeadbeef, "expected 0xdeadbeef, got %x\n", U(io).Status);
    ok(io.Information == 0xdeadbeef, "expected 0xdeadbeef, got %lu\n", io.Information);

    /* test with very small buffer */
    U(io).Status = 0xdeadbeef;
    io.Information = 0xdeadbeef;
    memset(buffer, 0xcc, EA_BUFFER_SIZE);
    buffer_len = 4;
    status = pNtQueryEaFile(handle, &io, buffer, buffer_len, TRUE, NULL, 0, NULL, FALSE);
    ok(status == STATUS_NO_EAS_ON_FILE, "expected STATUS_NO_EAS_ON_FILE, got %x\n", status);
    ok(U(io).Status == 0xdeadbeef, "expected 0xdeadbeef, got %x\n", U(io).Status);
    ok(io.Information == 0xdeadbeef, "expected 0xdeadbeef, got %lu\n", io.Information);
    for (i = 0; i < buffer_len && !buffer[i]; i++);
    ok(i == buffer_len,  "expected %u bytes filled with 0x00, got %u bytes\n", buffer_len, i);
    ok(buffer[i] == 0xcc, "data at position %u overwritten\n", buffer[i]);

    /* test with very big buffer */
    U(io).Status = 0xdeadbeef;
    io.Information = 0xdeadbeef;
    memset(buffer, 0xcc, EA_BUFFER_SIZE);
    buffer_len = EA_BUFFER_SIZE - 1;
    status = pNtQueryEaFile(handle, &io, buffer, buffer_len, TRUE, NULL, 0, NULL, FALSE);
    ok(status == STATUS_NO_EAS_ON_FILE, "expected STATUS_NO_EAS_ON_FILE, got %x\n", status);
    ok(U(io).Status == 0xdeadbeef, "expected 0xdeadbeef, got %x\n", U(io).Status);
    ok(io.Information == 0xdeadbeef, "expected 0xdeadbeef, got %lu\n", io.Information);
    for (i = 0; i < buffer_len && !buffer[i]; i++);
    ok(i == buffer_len,  "expected %u bytes filled with 0x00, got %u bytes\n", buffer_len, i);
    ok(buffer[i] == 0xcc, "data at position %u overwritten\n", buffer[i]);

    CloseHandle(handle);
    #undef EA_BUFFER_SIZE
}

static void test_file_readonly_access(void)
{
    static const DWORD default_sharing = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
    static const WCHAR fooW[] = {'f', 'o', 'o', 0};
    WCHAR path[MAX_PATH];
    OBJECT_ATTRIBUTES attr;
    UNICODE_STRING nameW;
    IO_STATUS_BLOCK io;
    HANDLE handle;
    NTSTATUS status;
    DWORD ret;

    /* Set up */
    GetTempPathW(MAX_PATH, path);
    GetTempFileNameW(path, fooW, 0, path);
    DeleteFileW(path);
    pRtlDosPathNameToNtPathName_U(path, &nameW, NULL, NULL);

    attr.Length = sizeof(attr);
    attr.RootDirectory = NULL;
    attr.ObjectName = &nameW;
    attr.Attributes = OBJ_CASE_INSENSITIVE;
    attr.SecurityDescriptor = NULL;
    attr.SecurityQualityOfService = NULL;

    status = pNtCreateFile(&handle, FILE_GENERIC_WRITE, &attr, &io, NULL, FILE_ATTRIBUTE_READONLY, default_sharing,
                           FILE_CREATE, 0, NULL, 0);
    ok(status == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %#x.\n", status);
    CloseHandle(handle);

    /* NtCreateFile FILE_GENERIC_WRITE */
    status = pNtCreateFile(&handle, FILE_GENERIC_WRITE, &attr, &io, NULL, FILE_ATTRIBUTE_NORMAL, default_sharing,
                           FILE_OPEN, FILE_NON_DIRECTORY_FILE, NULL, 0);
    ok(status == STATUS_ACCESS_DENIED, "expected STATUS_ACCESS_DENIED, got %#x.\n", status);

    /* NtCreateFile DELETE without FILE_DELETE_ON_CLOSE */
    status = pNtCreateFile(&handle, DELETE, &attr, &io, NULL, FILE_ATTRIBUTE_NORMAL, default_sharing, FILE_OPEN,
                           FILE_NON_DIRECTORY_FILE, NULL, 0);
    ok(status == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %#x.\n", status);
    CloseHandle(handle);

    /* NtCreateFile DELETE with FILE_DELETE_ON_CLOSE */
    status = pNtCreateFile(&handle, SYNCHRONIZE | DELETE, &attr, &io, NULL, FILE_ATTRIBUTE_NORMAL, default_sharing,
                           FILE_OPEN, FILE_DELETE_ON_CLOSE | FILE_NON_DIRECTORY_FILE, NULL, 0);
    ok(status == STATUS_CANNOT_DELETE, "expected STATUS_CANNOT_DELETE, got %#x.\n", status);

    /* NtOpenFile GENERIC_WRITE */
    status = pNtOpenFile(&handle, GENERIC_WRITE, &attr, &io, default_sharing, FILE_NON_DIRECTORY_FILE);
    ok(status == STATUS_ACCESS_DENIED, "expected STATUS_ACCESS_DENIED, got %#x.\n", status);

    /* NtOpenFile DELETE without FILE_DELETE_ON_CLOSE */
    status = pNtOpenFile(&handle, DELETE, &attr, &io, default_sharing, FILE_NON_DIRECTORY_FILE);
    ok(status == STATUS_SUCCESS, "expected STATUS_SUCCESS, got %#x.\n", status);
    CloseHandle(handle);

    /* NtOpenFile DELETE with FILE_DELETE_ON_CLOSE */
    status = pNtOpenFile(&handle, DELETE, &attr, &io, default_sharing, FILE_DELETE_ON_CLOSE | FILE_NON_DIRECTORY_FILE);
    ok(status == STATUS_CANNOT_DELETE, "expected STATUS_CANNOT_DELETE, got %#x.\n", status);

    ret = GetFileAttributesW(path);
    ok(ret & FILE_ATTRIBUTE_READONLY, "got wrong attribute: %#x.\n", ret);

    /* Clean up */
    pRtlFreeUnicodeString(&nameW);
    SetFileAttributesW(path, FILE_ATTRIBUTE_NORMAL);
    DeleteFileW(path);
}

static INT build_reparse_buffer(const WCHAR *filename, ULONG tag, ULONG flags,
                                REPARSE_DATA_BUFFER **pbuffer)
{
    static INT header_size = offsetof(REPARSE_DATA_BUFFER, GenericReparseBuffer);
    INT buffer_size, struct_size, data_size, string_len, prefix_len;
    WCHAR *subst_dest, *print_dest;
    REPARSE_DATA_BUFFER *buffer;

    switch(tag)
    {
    case IO_REPARSE_TAG_MOUNT_POINT:
        struct_size = offsetof(REPARSE_DATA_BUFFER, MountPointReparseBuffer.PathBuffer[0]);
        break;
    case IO_REPARSE_TAG_SYMLINK:
        struct_size = offsetof(REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer.PathBuffer[0]);
        break;
    default:
        return 0;
    }
    prefix_len = (flags == SYMLINK_FLAG_RELATIVE) ? 0 : strlen("\\??\\");
    string_len = lstrlenW(&filename[prefix_len]);
    data_size = (prefix_len + 2 * string_len + 2) * sizeof(WCHAR);
    buffer_size = struct_size + data_size;
    buffer = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, buffer_size);
    buffer->ReparseTag = tag;
    buffer->ReparseDataLength = struct_size - header_size + data_size;
    switch(tag)
    {
    case IO_REPARSE_TAG_MOUNT_POINT:
        buffer->MountPointReparseBuffer.SubstituteNameLength = (prefix_len + string_len) * sizeof(WCHAR);
        buffer->MountPointReparseBuffer.PrintNameOffset = (prefix_len + string_len + 1) * sizeof(WCHAR);
        buffer->MountPointReparseBuffer.PrintNameLength = string_len * sizeof(WCHAR);
        subst_dest = &buffer->MountPointReparseBuffer.PathBuffer[0];
        print_dest = &buffer->MountPointReparseBuffer.PathBuffer[prefix_len + string_len + 1];
        break;
    case IO_REPARSE_TAG_SYMLINK:
        buffer->SymbolicLinkReparseBuffer.SubstituteNameLength = (prefix_len + string_len) * sizeof(WCHAR);
        buffer->SymbolicLinkReparseBuffer.PrintNameOffset = (prefix_len + string_len + 1) * sizeof(WCHAR);
        buffer->SymbolicLinkReparseBuffer.PrintNameLength = string_len * sizeof(WCHAR);
        buffer->SymbolicLinkReparseBuffer.Flags = flags;
        subst_dest = &buffer->SymbolicLinkReparseBuffer.PathBuffer[0];
        print_dest = &buffer->SymbolicLinkReparseBuffer.PathBuffer[prefix_len + string_len + 1];
        break;
    default:
        return 0;
    }
    lstrcpyW(subst_dest, filename);
    lstrcpyW(print_dest, &filename[prefix_len]);
    *pbuffer = buffer;
    return buffer_size;
}

static void test_reparse_points(void)
{
    static const WCHAR reparseW[] = {'\\','r','e','p','a','r','s','e',0};
    WCHAR path[MAX_PATH], reparse_path[MAX_PATH], target_path[MAX_PATH];
    static const WCHAR targetW[] = {'\\','t','a','r','g','e','t',0};
    FILE_BASIC_INFORMATION old_attrib, new_attrib;
    static const WCHAR fooW[] = {'f','o','o',0};
    static WCHAR volW[] = {'c',':','\\',0};
    REPARSE_GUID_DATA_BUFFER guid_buffer;
    static const WCHAR dotW[] = {'.',0};
    REPARSE_DATA_BUFFER *buffer = NULL;
    DWORD dwret, dwLen, dwFlags, err;
    WIN32_FILE_ATTRIBUTE_DATA fad;
    INT buffer_len, string_len;
    WCHAR buf[] = {0,0,0,0};
    HANDLE handle, token;
    IO_STATUS_BLOCK iosb;
    UNICODE_STRING nameW;
    TOKEN_PRIVILEGES tp;
    WCHAR *dest;
    LUID luid;
    BOOL bret;

    /* Create a temporary folder for the junction point tests */
    GetTempFileNameW(dotW, fooW, 0, path);
    DeleteFileW(path);
    if (!CreateDirectoryW(path, NULL))
    {
        win_skip("Unable to create a temporary junction point directory.\n");
        return;
    }

    /* Check that the volume this folder is located on supports junction points */
    pRtlDosPathNameToNtPathName_U(path, &nameW, NULL, NULL);
    volW[0] = nameW.Buffer[4];
    pRtlFreeUnicodeString( &nameW );
    GetVolumeInformationW(volW, 0, 0, 0, &dwLen, &dwFlags, 0, 0);
    if (!(dwFlags & FILE_SUPPORTS_REPARSE_POINTS))
    {
        skip("File system does not support reparse points.\n");
        RemoveDirectoryW(path);
        return;
    }

    /* Create the folder to be replaced by a junction point */
    lstrcpyW(reparse_path, path);
    lstrcatW(reparse_path, reparseW);
    bret = CreateDirectoryW(reparse_path, NULL);
    ok(bret, "Failed to create junction point directory.\n");

    /* Create a destination folder for the junction point to target */
    lstrcpyW(target_path, path);
    lstrcatW(target_path, targetW);
    bret = CreateDirectoryW(target_path, NULL);
    ok(bret, "Failed to create junction point target directory.\n");
    pRtlDosPathNameToNtPathName_U(target_path, &nameW, NULL, NULL);

    /* Create the junction point */
    handle = CreateFileW(reparse_path, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING,
                         FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, 0);
    if (handle == INVALID_HANDLE_VALUE)
    {
        win_skip("Failed to open junction point directory handle (0x%x).\n", GetLastError());
        goto cleanup;
    }
    dwret = NtQueryInformationFile(handle, &iosb, &old_attrib, sizeof(old_attrib), FileBasicInformation);
    ok(dwret == STATUS_SUCCESS, "Failed to get junction point folder's attributes (0x%x).\n", dwret);
    buffer_len = build_reparse_buffer(nameW.Buffer, IO_REPARSE_TAG_MOUNT_POINT, 0, &buffer);
    bret = DeviceIoControl(handle, FSCTL_SET_REPARSE_POINT, (LPVOID)buffer, buffer_len, NULL, 0, &dwret, 0);
    ok(bret, "Failed to create junction point! (0x%x)\n", GetLastError());

    /* Check the file attributes of the junction point */
    dwret = GetFileAttributesW(reparse_path);
    ok(dwret != (DWORD)~0, "Junction point doesn't exist (attributes: 0x%x)!\n", dwret);
    ok(dwret & FILE_ATTRIBUTE_REPARSE_POINT, "File is not a junction point! (attributes: %d)\n", dwret);

    /* Read back the junction point */
    HeapFree(GetProcessHeap(), 0, buffer);
    buffer_len = sizeof(*buffer) + MAX_PATH*sizeof(WCHAR);
    buffer = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, buffer_len);
    bret = DeviceIoControl(handle, FSCTL_GET_REPARSE_POINT, NULL, 0, (LPVOID)buffer, buffer_len, &dwret, 0);
    ok(bret, "Failed to read junction point!\n");
    string_len = buffer->MountPointReparseBuffer.SubstituteNameLength;
    dest = &buffer->MountPointReparseBuffer.PathBuffer[buffer->MountPointReparseBuffer.SubstituteNameOffset/sizeof(WCHAR)];
    ok((memcmp(dest, nameW.Buffer, string_len) == 0), "Junction point destination does not match ('%s' != '%s')!\n",
                                                      wine_dbgstr_w(dest), wine_dbgstr_w(nameW.Buffer));

    /* Delete the junction point */
    memset(&old_attrib, 0x00, sizeof(old_attrib));
    old_attrib.LastAccessTime.QuadPart = 0x200deadcafebeef;
    dwret = NtSetInformationFile(handle, &iosb, &old_attrib, sizeof(old_attrib), FileBasicInformation);
    ok(dwret == STATUS_SUCCESS, "Failed to set junction point folder's attributes (0x%x).\n", dwret);
    memset(&guid_buffer, 0x00, sizeof(guid_buffer));
    guid_buffer.ReparseTag = IO_REPARSE_TAG_MOUNT_POINT;
    bret = DeviceIoControl(handle, FSCTL_DELETE_REPARSE_POINT, (LPVOID)&guid_buffer,
                           REPARSE_GUID_DATA_BUFFER_HEADER_SIZE, NULL, 0, &dwret, 0);
    ok(bret, "Failed to delete junction point! (0x%x)\n", GetLastError());
    memset(&new_attrib, 0x00, sizeof(new_attrib));
    dwret = NtQueryInformationFile(handle, &iosb, &new_attrib, sizeof(new_attrib), FileBasicInformation);
    ok(dwret == STATUS_SUCCESS, "Failed to get junction point folder's attributes (0x%x).\n", dwret);
    ok(old_attrib.LastAccessTime.QuadPart == new_attrib.LastAccessTime.QuadPart,
       "Junction point folder's access time does not match.\n");
    CloseHandle(handle);

    /* Check deleting a junction point as if it were a directory */
    HeapFree(GetProcessHeap(), 0, buffer);
    handle = CreateFileW(reparse_path, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING,
                            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, 0);
    buffer_len = build_reparse_buffer(nameW.Buffer, IO_REPARSE_TAG_MOUNT_POINT, 0, &buffer);
    bret = DeviceIoControl(handle, FSCTL_SET_REPARSE_POINT, (LPVOID)buffer, buffer_len, NULL, 0, &dwret, 0);
    ok(bret, "Failed to create junction point! (0x%x)\n", GetLastError());
    CloseHandle(handle);
    bret = RemoveDirectoryW(reparse_path);
    ok(bret, "Failed to delete junction point as directory!\n");
    dwret = GetFileAttributesW(reparse_path);
    ok(dwret == (DWORD)~0, "Junction point still exists (attributes: 0x%x)!\n", dwret);

    /* Check deleting a junction point as if it were a file */
    HeapFree(GetProcessHeap(), 0, buffer);
    bret = CreateDirectoryW(reparse_path, NULL);
    ok(bret, "Failed to create junction point target directory.\n");
    handle = CreateFileW(reparse_path, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING,
                         FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, 0);
    buffer_len = build_reparse_buffer(nameW.Buffer, IO_REPARSE_TAG_MOUNT_POINT, 0, &buffer);
    bret = DeviceIoControl(handle, FSCTL_SET_REPARSE_POINT, (LPVOID)buffer, buffer_len, NULL, 0, &dwret, 0);
    ok(bret, "Failed to create junction point! (0x%x)\n", GetLastError());
    CloseHandle(handle);
    bret = DeleteFileW(reparse_path);
    ok(!bret, "Succeeded in deleting junction point as file!\n");
    err = GetLastError();
    ok(err == ERROR_ACCESS_DENIED, "Expected last error 0x%x for DeleteFile on junction point (actually 0x%x)!\n",
                                   ERROR_ACCESS_DENIED, err);
    dwret = GetFileAttributesW(reparse_path);
    ok(dwret != (DWORD)~0, "Junction point doesn't exist (attributes: 0x%x)!\n", dwret);
    ok(dwret & FILE_ATTRIBUTE_REPARSE_POINT, "File is not a junction point! (attributes: 0x%x)\n", dwret);

    /* Test deleting a junction point's target */
    dwret = GetFileAttributesW(reparse_path);
    ok(dwret == 0x410 || broken(dwret == 0x430) /* win2k */ || broken(dwret == 0xc10) /* vista */,
       "Unexpected junction point attributes (0x%x != 0x410)!\n", dwret);
    bret = RemoveDirectoryW(target_path);
    ok(bret, "Failed to delete junction point target!\n");

    /* Establish permissions for symlink creation */
    bret = OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &token);
    ok(bret, "OpenProcessToken failed: %u\n", GetLastError());
    bret = LookupPrivilegeValueA(NULL, "SeCreateSymbolicLinkPrivilege", &luid);
    todo_wine ok(bret || broken(!bret && GetLastError() == ERROR_NO_SUCH_PRIVILEGE) /* winxp */,
                 "LookupPrivilegeValue failed: %u\n", GetLastError());
    if (bret)
    {
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        bret = AdjustTokenPrivileges(token, FALSE, &tp, 0, NULL, NULL);
        ok(bret, "AdjustTokenPrivileges failed: %u\n", GetLastError());
        if (GetLastError() == ERROR_NOT_ALL_ASSIGNED)
        {
            win_skip("Insufficient permissions to perform symlink tests.\n");
            goto cleanup;
        }
    }

    /* Delete the junction point directory and create a blank slate for symlink tests */
    bret = RemoveDirectoryW(reparse_path);
    ok(bret, "Failed to delete junction point!\n");
    handle = CreateFileW(target_path, GENERIC_READ | GENERIC_WRITE, 0, 0, CREATE_NEW, 0, 0);
    ok(handle != INVALID_HANDLE_VALUE, "Failed to create symlink target file.\n");
    bret = WriteFile(handle, fooW, sizeof(fooW), &dwLen, NULL);
    ok(bret, "Failed to write data to the symlink target file.\n");
    ok(GetFileSize(handle, NULL) == sizeof(fooW), "target size is incorrect (%d vs %d)\n",
       GetFileSize(handle, NULL), sizeof(fooW));
    CloseHandle(handle);

    /* Create the file symlink */
    HeapFree(GetProcessHeap(), 0, buffer);
    handle = CreateFileW(reparse_path, GENERIC_READ | GENERIC_WRITE, 0, 0, CREATE_NEW,
                         FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, 0);
    ok(handle != INVALID_HANDLE_VALUE, "Failed to create symlink file.\n");
    dwret = NtQueryInformationFile(handle, &iosb, &old_attrib, sizeof(old_attrib), FileBasicInformation);
    ok(dwret == STATUS_SUCCESS, "Failed to get symlink file's attributes (0x%x).\n", dwret);
    buffer_len = build_reparse_buffer(nameW.Buffer, IO_REPARSE_TAG_SYMLINK, 0, &buffer);
    bret = DeviceIoControl(handle, FSCTL_SET_REPARSE_POINT, (LPVOID)buffer, buffer_len, NULL, 0, &dwret, 0);
    ok(bret, "Failed to create symlink! (0x%x)\n", GetLastError());
    CloseHandle(handle);

    /* Check the size of the symlink */
    bret = GetFileAttributesExW(reparse_path, GetFileExInfoStandard, &fad);
    ok(bret, "Failed to read file attributes from the symlink target.\n");
    ok(fad.nFileSizeLow == 0 && fad.nFileSizeHigh == 0, "Size of symlink is not zero.\n");
    handle = CreateFileW(reparse_path, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING,
                         FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, 0);
    ok(handle != INVALID_HANDLE_VALUE, "Failed to open symlink file.\n");
    todo_wine ok(GetFileSize(handle, NULL) == 0, "symlink size is not zero\n");
    bret = ReadFile(handle, &buf, sizeof(buf), &dwLen, NULL);
    ok(bret, "Failed to read data from the symlink.\n");
    todo_wine ok(dwLen == 0, "Length of symlink data is not zero.\n");
    CloseHandle(handle);

    /* Check the size/data of the symlink target */
    handle = CreateFileW(reparse_path, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING,
                         FILE_FLAG_BACKUP_SEMANTICS, 0);
    if (handle == INVALID_HANDLE_VALUE)
    {
        win_skip("Failed to open symlink file handle (0x%x).\n", GetLastError());
        goto cleanup;
    }
    ok(GetFileSize(handle, NULL) == sizeof(fooW), "symlink target size does not match (%d != %d)\n",
       GetFileSize(handle, NULL), sizeof(fooW));
    bret = ReadFile(handle, &buf, sizeof(buf), &dwLen, NULL);
    ok(bret, "Failed to read data from the symlink.\n");
    ok(dwLen == sizeof(fooW), "Length of symlink target data does not match (%d != %d).\n",
       dwLen, sizeof(fooW));
    ok(!memcmp(fooW, &buf, sizeof(fooW)), "Symlink target data does not match (%s != %s).\n",
       wine_dbgstr_wn(buf, dwLen), wine_dbgstr_w(fooW));
    CloseHandle(handle);

    /* Check deleting a file symlink as if it were a directory */
    bret = RemoveDirectoryW(reparse_path);
    ok(!bret, "Succeeded in deleting file symlink as a directory!\n");
    err = GetLastError();
    ok(err == ERROR_DIRECTORY,
       "Expected last error 0x%x for RemoveDirectory on file symlink (actually 0x%x)!\n",
       ERROR_DIRECTORY, err);
    dwret = GetFileAttributesW(reparse_path);
    ok(dwret != (DWORD)~0, "Symlink doesn't exist (attributes: 0x%x)!\n", dwret);
    ok(dwret & FILE_ATTRIBUTE_REPARSE_POINT, "File is not a symlink! (attributes: 0x%x)\n", dwret);

    /* Delete the symlink as a file */
    bret = DeleteFileW(reparse_path);
    ok(bret, "Failed to delete symlink as a file!\n");

    /* Create a blank slate for directory symlink tests */
    bret = CreateDirectoryW(reparse_path, NULL);
    ok(bret, "Failed to create junction point directory.\n");
    dwret = GetFileAttributesW(reparse_path);
    ok(dwret != (DWORD)~0, "Path doesn't exist (attributes: 0x%x)!\n", dwret);
    ok(!(dwret & FILE_ATTRIBUTE_REPARSE_POINT), "File is already a reparse point! (attributes: %d)\n", dwret);
    bret = DeleteFileW(target_path);
    ok(bret, "Failed to delete symlink target!\n");
    bret = CreateDirectoryW(target_path, NULL);
    ok(bret, "Failed to create symlink target directory.\n");

    /* Create the directory symlink */
    HeapFree(GetProcessHeap(), 0, buffer);
    handle = CreateFileW(reparse_path, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING,
                         FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, 0);
    if (handle == INVALID_HANDLE_VALUE)
    {
        win_skip("Failed to open symlink directory handle (0x%x).\n", GetLastError());
        goto cleanup;
    }
    dwret = NtQueryInformationFile(handle, &iosb, &old_attrib, sizeof(old_attrib), FileBasicInformation);
    ok(dwret == STATUS_SUCCESS, "Failed to get symlink folder's attributes (0x%x).\n", dwret);
    buffer_len = build_reparse_buffer(nameW.Buffer, IO_REPARSE_TAG_SYMLINK, 0, &buffer);
    bret = DeviceIoControl(handle, FSCTL_SET_REPARSE_POINT, (LPVOID)buffer, buffer_len, NULL, 0, &dwret, 0);
    ok(bret, "Failed to create symlink! (0x%x)\n", GetLastError());

    /* Check the file attributes of the symlink */
    dwret = GetFileAttributesW(reparse_path);
    ok(dwret != (DWORD)~0, "Symlink doesn't exist (attributes: 0x%x)!\n", dwret);
    ok(dwret & FILE_ATTRIBUTE_REPARSE_POINT, "File is not a symlink! (attributes: %d)\n", dwret);

    /* Read back the symlink */
    HeapFree(GetProcessHeap(), 0, buffer);
    buffer_len = sizeof(*buffer) + MAX_PATH*sizeof(WCHAR);
    buffer = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, buffer_len);
    bret = DeviceIoControl(handle, FSCTL_GET_REPARSE_POINT, NULL, 0, (LPVOID)buffer, buffer_len, &dwret, 0);
    string_len = buffer->SymbolicLinkReparseBuffer.SubstituteNameLength;
    dest = &buffer->SymbolicLinkReparseBuffer.PathBuffer[buffer->SymbolicLinkReparseBuffer.SubstituteNameOffset/sizeof(WCHAR)];
    ok(bret, "Failed to read symlink!\n");
    ok((memcmp(dest, nameW.Buffer, string_len) == 0), "Symlink destination does not match ('%s' != '%s')!\n",
                                                      wine_dbgstr_w(dest), wine_dbgstr_w(nameW.Buffer));

    /* Delete the symlink */
    memset(&old_attrib, 0x00, sizeof(old_attrib));
    old_attrib.LastAccessTime.QuadPart = 0x200deadcafebeef;
    dwret = NtSetInformationFile(handle, &iosb, &old_attrib, sizeof(old_attrib), FileBasicInformation);
    ok(dwret == STATUS_SUCCESS, "Failed to set symlink folder's attributes (0x%x).\n", dwret);
    memset(&guid_buffer, 0x00, sizeof(guid_buffer));
    guid_buffer.ReparseTag = IO_REPARSE_TAG_SYMLINK;
    bret = DeviceIoControl(handle, FSCTL_DELETE_REPARSE_POINT, (LPVOID)&guid_buffer,
                           REPARSE_GUID_DATA_BUFFER_HEADER_SIZE, NULL, 0, &dwret, 0);
    ok(bret, "Failed to delete symlink! (0x%x)\n", GetLastError());
    memset(&new_attrib, 0x00, sizeof(new_attrib));
    dwret = NtQueryInformationFile(handle, &iosb, &new_attrib, sizeof(new_attrib), FileBasicInformation);
    ok(dwret == STATUS_SUCCESS, "Failed to get symlink folder's attributes (0x%x).\n", dwret);
    ok(old_attrib.LastAccessTime.QuadPart == new_attrib.LastAccessTime.QuadPart,
       "Symlink folder's access time does not match.\n");
    CloseHandle(handle);

    /* Create a relative directory symlink */
    HeapFree(GetProcessHeap(), 0, buffer);
    handle = CreateFileW(reparse_path, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING,
                         FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, 0);
    if (handle == INVALID_HANDLE_VALUE)
    {
        win_skip("Failed to open symlink directory handle (0x%x).\n", GetLastError());
        goto cleanup;
    }
    dwret = NtQueryInformationFile(handle, &iosb, &old_attrib, sizeof(old_attrib), FileBasicInformation);
    ok(dwret == STATUS_SUCCESS, "Failed to get symlink folder's attributes (0x%x).\n", dwret);
    buffer_len = build_reparse_buffer(targetW, IO_REPARSE_TAG_SYMLINK, SYMLINK_FLAG_RELATIVE, &buffer);
    bret = DeviceIoControl(handle, FSCTL_SET_REPARSE_POINT, (LPVOID)buffer, buffer_len, NULL, 0, &dwret, 0);
    ok(bret, "Failed to create symlink! (0x%x)\n", GetLastError());

    /* Read back the relative symlink */
    HeapFree(GetProcessHeap(), 0, buffer);
    buffer_len = sizeof(*buffer) + MAX_PATH*sizeof(WCHAR);
    buffer = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, buffer_len);
    bret = DeviceIoControl(handle, FSCTL_GET_REPARSE_POINT, NULL, 0, (LPVOID)buffer, buffer_len, &dwret, 0);
    ok(bret, "Failed to read relative symlink!\n");
    string_len = buffer->SymbolicLinkReparseBuffer.SubstituteNameLength;
    dest = &buffer->SymbolicLinkReparseBuffer.PathBuffer[buffer->SymbolicLinkReparseBuffer.SubstituteNameOffset/sizeof(WCHAR)];
    ok((memcmp(dest, targetW, string_len) == 0), "Symlink destination does not match ('%s' != '%s')!\n",
                                                 wine_dbgstr_w(dest), wine_dbgstr_w(targetW));
    CloseHandle(handle);

cleanup:
    /* Cleanup */
    pRtlFreeUnicodeString(&nameW);
    HeapFree(GetProcessHeap(), 0, buffer);
    RemoveDirectoryW(reparse_path);
    DeleteFileW(reparse_path);
    RemoveDirectoryW(target_path);
    DeleteFileW(target_path);
    RemoveDirectoryW(path);
}

START_TEST(file)
{
    HMODULE hkernel32 = GetModuleHandleA("kernel32.dll");
    HMODULE hntdll = GetModuleHandleA("ntdll.dll");
    if (!hntdll)
    {
        skip("not running on NT, skipping test\n");
        return;
    }

    pGetVolumePathNameW = (void *)GetProcAddress(hkernel32, "GetVolumePathNameW");
    pGetSystemWow64DirectoryW = (void *)GetProcAddress(hkernel32, "GetSystemWow64DirectoryW");

    pRtlFreeUnicodeString   = (void *)GetProcAddress(hntdll, "RtlFreeUnicodeString");
    pRtlInitUnicodeString   = (void *)GetProcAddress(hntdll, "RtlInitUnicodeString");
    pRtlDosPathNameToNtPathName_U = (void *)GetProcAddress(hntdll, "RtlDosPathNameToNtPathName_U");
    pRtlWow64EnableFsRedirectionEx = (void *)GetProcAddress(hntdll, "RtlWow64EnableFsRedirectionEx");
    pNtCreateMailslotFile   = (void *)GetProcAddress(hntdll, "NtCreateMailslotFile");
    pNtCreateFile           = (void *)GetProcAddress(hntdll, "NtCreateFile");
    pNtOpenFile             = (void *)GetProcAddress(hntdll, "NtOpenFile");
    pNtDeleteFile           = (void *)GetProcAddress(hntdll, "NtDeleteFile");
    pNtReadFile             = (void *)GetProcAddress(hntdll, "NtReadFile");
    pNtWriteFile            = (void *)GetProcAddress(hntdll, "NtWriteFile");
    pNtCancelIoFile         = (void *)GetProcAddress(hntdll, "NtCancelIoFile");
    pNtCancelIoFileEx       = (void *)GetProcAddress(hntdll, "NtCancelIoFileEx");
    pNtClose                = (void *)GetProcAddress(hntdll, "NtClose");
    pNtFsControlFile        = (void *)GetProcAddress(hntdll, "NtFsControlFile");
    pNtCreateIoCompletion   = (void *)GetProcAddress(hntdll, "NtCreateIoCompletion");
    pNtOpenIoCompletion     = (void *)GetProcAddress(hntdll, "NtOpenIoCompletion");
    pNtQueryIoCompletion    = (void *)GetProcAddress(hntdll, "NtQueryIoCompletion");
    pNtRemoveIoCompletion   = (void *)GetProcAddress(hntdll, "NtRemoveIoCompletion");
    pNtRemoveIoCompletionEx = (void *)GetProcAddress(hntdll, "NtRemoveIoCompletionEx");
    pNtSetIoCompletion      = (void *)GetProcAddress(hntdll, "NtSetIoCompletion");
    pNtSetInformationFile   = (void *)GetProcAddress(hntdll, "NtSetInformationFile");
    pNtQueryAttributesFile  = (void *)GetProcAddress(hntdll, "NtQueryAttributesFile");
    pNtQueryInformationFile = (void *)GetProcAddress(hntdll, "NtQueryInformationFile");
    pNtQueryDirectoryFile   = (void *)GetProcAddress(hntdll, "NtQueryDirectoryFile");
    pNtQueryVolumeInformationFile = (void *)GetProcAddress(hntdll, "NtQueryVolumeInformationFile");
    pNtQueryFullAttributesFile = (void *)GetProcAddress(hntdll, "NtQueryFullAttributesFile");
    pNtFlushBuffersFile = (void *)GetProcAddress(hntdll, "NtFlushBuffersFile");
    pNtQueryEaFile          = (void *)GetProcAddress(hntdll, "NtQueryEaFile");

    test_read_write();
    test_NtCreateFile();
    test_readonly();
    create_file_test();
    open_file_test();
    delete_file_test();
    read_file_test();
    append_file_test();
    nt_mailslot_test();
    test_set_io_completion();
    test_file_io_completion();
    test_file_basic_information();
    test_file_all_information();
    test_file_both_information();
    test_file_name_information();
    test_file_full_size_information();
    test_file_all_name_information();
    test_file_rename_information();
    test_file_link_information();
    test_file_disposition_information();
    test_file_completion_information();
    test_file_id_information();
    test_file_access_information();
    test_file_attribute_tag_information();
    test_file_mode();
    test_file_readonly_access();
    test_query_volume_information_file();
    test_query_attribute_information_file();
    test_ioctl();
    test_query_ea();
    test_flush_buffers_file();
    test_reparse_points();
}
