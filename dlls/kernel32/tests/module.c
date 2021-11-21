/*
 * Unit tests for module/DLL/library API
 *
 * Copyright (c) 2004 Eric Pouech
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
#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winnls.h"
#include "winternl.h"
#include <psapi.h>
#include "wine/test.h"

static DWORD (WINAPI *pGetDllDirectoryA)(DWORD,LPSTR);
static DWORD (WINAPI *pGetDllDirectoryW)(DWORD,LPWSTR);
static BOOL (WINAPI *pSetDllDirectoryA)(LPCSTR);
static DLL_DIRECTORY_COOKIE (WINAPI *pAddDllDirectory)(const WCHAR*);
static BOOL (WINAPI *pRemoveDllDirectory)(DLL_DIRECTORY_COOKIE);
static BOOL (WINAPI *pSetDefaultDllDirectories)(DWORD);
static BOOL (WINAPI *pK32GetModuleInformation)(HANDLE process, HMODULE module,
                                               MODULEINFO *modinfo, DWORD cb);

static NTSTATUS (WINAPI *pLdrGetDllDirectory)(UNICODE_STRING*);
static NTSTATUS (WINAPI *pLdrSetDllDirectory)(UNICODE_STRING*);
static NTSTATUS (WINAPI *pLdrGetDllHandle)( LPCWSTR load_path, ULONG flags, const UNICODE_STRING *name, HMODULE *base );
static NTSTATUS (WINAPI *pLdrGetDllHandleEx)( ULONG flags, LPCWSTR load_path, ULONG *dll_characteristics,
                                              const UNICODE_STRING *name, HMODULE *base );
static NTSTATUS (WINAPI *pLdrGetDllFullName)( HMODULE module, UNICODE_STRING *name );

static BOOL is_unicode_enabled = TRUE;

static BOOL cmpStrAW(const char* a, const WCHAR* b, DWORD lenA, DWORD lenB)
{
    WCHAR       aw[1024];

    DWORD len = MultiByteToWideChar( AreFileApisANSI() ? CP_ACP : CP_OEMCP, 0,
                                     a, lenA, aw, ARRAY_SIZE(aw));
    if (len != lenB) return FALSE;
    return memcmp(aw, b, len * sizeof(WCHAR)) == 0;
}

static const struct
{
    IMAGE_DOS_HEADER dos;
    IMAGE_NT_HEADERS nt;
    IMAGE_SECTION_HEADER section;
} dll_image =
{
    { IMAGE_DOS_SIGNATURE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, { 0 }, 0, 0, { 0 },
      sizeof(IMAGE_DOS_HEADER) },
    {
        IMAGE_NT_SIGNATURE, /* Signature */
        {
#if defined __i386__
            IMAGE_FILE_MACHINE_I386, /* Machine */
#elif defined __x86_64__
            IMAGE_FILE_MACHINE_AMD64, /* Machine */
#elif defined __arm__
            IMAGE_FILE_MACHINE_ARMNT, /* Machine */
#elif defined __aarch64__
            IMAGE_FILE_MACHINE_ARM64, /* Machine */
#else
# error You must specify the machine type
#endif
            1, /* NumberOfSections */
            0, /* TimeDateStamp */
            0, /* PointerToSymbolTable */
            0, /* NumberOfSymbols */
            sizeof(IMAGE_OPTIONAL_HEADER), /* SizeOfOptionalHeader */
            IMAGE_FILE_EXECUTABLE_IMAGE | IMAGE_FILE_DLL /* Characteristics */
        },
        { IMAGE_NT_OPTIONAL_HDR_MAGIC, /* Magic */
          1, /* MajorLinkerVersion */
          0, /* MinorLinkerVersion */
          0, /* SizeOfCode */
          0, /* SizeOfInitializedData */
          0, /* SizeOfUninitializedData */
          0, /* AddressOfEntryPoint */
          0x1000, /* BaseOfCode */
#ifndef _WIN64
          0, /* BaseOfData */
#endif
          0x10000000, /* ImageBase */
          0x1000, /* SectionAlignment */
          0x1000, /* FileAlignment */
          4, /* MajorOperatingSystemVersion */
          0, /* MinorOperatingSystemVersion */
          1, /* MajorImageVersion */
          0, /* MinorImageVersion */
          4, /* MajorSubsystemVersion */
          0, /* MinorSubsystemVersion */
          0, /* Win32VersionValue */
          0x2000, /* SizeOfImage */
          sizeof(IMAGE_DOS_HEADER) + sizeof(IMAGE_NT_HEADERS), /* SizeOfHeaders */
          0, /* CheckSum */
          IMAGE_SUBSYSTEM_WINDOWS_CUI, /* Subsystem */
          0, /* DllCharacteristics */
          0, /* SizeOfStackReserve */
          0, /* SizeOfStackCommit */
          0, /* SizeOfHeapReserve */
          0, /* SizeOfHeapCommit */
          0, /* LoaderFlags */
          0, /* NumberOfRvaAndSizes */
          { { 0 } } /* DataDirectory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES] */
        }
    },
    { ".rodata", { 0 }, 0x1000, 0x1000, 0, 0, 0, 0, 0,
      IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ }
};

static void create_test_dll( const char *name )
{
    DWORD dummy;
    HANDLE handle = CreateFileA( name, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, 0 );

    ok( handle != INVALID_HANDLE_VALUE, "failed to create file err %u\n", GetLastError() );
    WriteFile( handle, &dll_image, sizeof(dll_image), &dummy, NULL );
    SetFilePointer( handle, dll_image.nt.OptionalHeader.SizeOfImage, NULL, FILE_BEGIN );
    SetEndOfFile( handle );
    CloseHandle( handle );
}

static void testGetModuleFileName(const char* name)
{
    HMODULE     hMod;
    char        bufA[MAX_PATH];
    WCHAR       bufW[MAX_PATH];
    DWORD       len1A, len1W = 0, len2A, len2W = 0;

    hMod = (name) ? GetModuleHandleA(name) : NULL;

    /* first test, with enough space in buffer */
    memset(bufA, '-', sizeof(bufA));
    SetLastError(0xdeadbeef);
    len1A = GetModuleFileNameA(hMod, bufA, sizeof(bufA));
    ok(GetLastError() == ERROR_SUCCESS ||
       broken(GetLastError() == 0xdeadbeef), /* <= XP SP3 */
       "LastError was not reset: %u\n", GetLastError());
    ok(len1A > 0, "Getting module filename for handle %p\n", hMod);

    if (is_unicode_enabled)
    {
        memset(bufW, '-', sizeof(bufW));
        SetLastError(0xdeadbeef);
        len1W = GetModuleFileNameW(hMod, bufW, ARRAY_SIZE(bufW));
        ok(GetLastError() == ERROR_SUCCESS ||
           broken(GetLastError() == 0xdeadbeef), /* <= XP SP3 */
           "LastError was not reset: %u\n", GetLastError());
        ok(len1W > 0, "Getting module filename for handle %p\n", hMod);
    }

    ok(len1A == strlen(bufA), "Unexpected length of GetModuleFilenameA (%d/%d)\n", len1A, lstrlenA(bufA));

    if (is_unicode_enabled)
    {
        ok(len1W == lstrlenW(bufW), "Unexpected length of GetModuleFilenameW (%d/%d)\n", len1W, lstrlenW(bufW));
        ok(cmpStrAW(bufA, bufW, len1A, len1W), "Comparing GetModuleFilenameAW results\n");
    }

    /* second test with a buffer too small */
    memset(bufA, '-', sizeof(bufA));
    len2A = GetModuleFileNameA(hMod, bufA, len1A / 2);
    ok(len2A > 0, "Getting module filename for handle %p\n", hMod);

    if (is_unicode_enabled)
    {
        memset(bufW, '-', sizeof(bufW));
        len2W = GetModuleFileNameW(hMod, bufW, len1W / 2);
        ok(len2W > 0, "Getting module filename for handle %p\n", hMod);
        ok(cmpStrAW(bufA, bufW, len2A, len2W), "Comparing GetModuleFilenameAW results with buffer too small\n" );
        ok(len1W / 2 == len2W, "Correct length in GetModuleFilenameW with buffer too small (%d/%d)\n", len1W / 2, len2W);
    }

    ok(len1A / 2 == len2A,
       "Correct length in GetModuleFilenameA with buffer too small (%d/%d)\n", len1A / 2, len2A);

    len1A = GetModuleFileNameA(hMod, bufA, 0x10000);
    ok(len1A > 0, "Getting module filename for handle %p\n", hMod);
    len1W = GetModuleFileNameW(hMod, bufW, 0x10000);
    ok(len1W > 0, "Getting module filename for handle %p\n", hMod);
}

static void testGetModuleFileName_Wrong(void)
{
    char        bufA[MAX_PATH];
    WCHAR       bufW[MAX_PATH];

    /* test wrong handle */
    if (is_unicode_enabled)
    {
        bufW[0] = '*';
        ok(GetModuleFileNameW((void*)0xffffffff, bufW, ARRAY_SIZE(bufW)) == 0,
           "Unexpected success in module handle\n");
        ok(bufW[0] == '*', "When failing, buffer shouldn't be written to\n");
    }

    bufA[0] = '*';
    ok(GetModuleFileNameA((void*)0xffffffff, bufA, sizeof(bufA)) == 0, "Unexpected success in module handle\n");
    ok(bufA[0] == '*', "When failing, buffer shouldn't be written to\n");
}

static void testLoadLibraryA(void)
{
    HMODULE hModule, hModule1;
    FARPROC fp;

    SetLastError(0xdeadbeef);
    hModule = LoadLibraryA("kernel32.dll");
    ok( hModule != NULL, "kernel32.dll should be loadable\n");
    ok( GetLastError() == 0xdeadbeef, "GetLastError should be 0xdeadbeef but is %d\n", GetLastError());

    fp = GetProcAddress(hModule, "CreateFileA");
    ok( fp != NULL, "CreateFileA should be there\n");
    ok( GetLastError() == 0xdeadbeef, "GetLastError should be 0xdeadbeef but is %d\n", GetLastError());

    SetLastError(0xdeadbeef);
    hModule1 = LoadLibraryA("kernel32   ");
    ok( hModule1 != NULL, "\"kernel32   \" should be loadable\n" );
    ok( GetLastError() == 0xdeadbeef, "GetLastError should be 0xdeadbeef but is %d\n", GetLastError() );
    ok( hModule == hModule1, "Loaded wrong module\n" );
    FreeLibrary(hModule1);
    FreeLibrary(hModule);
}

static void testNestedLoadLibraryA(void)
{
    static const char dllname[] = "shell32.dll";
    char path1[MAX_PATH], path2[MAX_PATH];
    HMODULE hModule1, hModule2, hModule3;

    /* This is not really a Windows conformance test, but more a Wine
     * regression test. Wine's builtin dlls can be loaded from multiple paths,
     * and this test tries to make sure that Wine does not get confused and
     * really unloads the Unix .so file at the right time. Failure to do so
     * will result in the dll being unloadable.
     * This test must be done with a dll that can be unloaded, which means:
     * - it must not already be loaded
     * - it must not have a 16-bit counterpart
     */
    GetWindowsDirectoryA(path1, sizeof(path1));
    strcat(path1, "\\system\\");
    strcat(path1, dllname);
    hModule1 = LoadLibraryA(path1);
    if (!hModule1)
    {
        /* We must be on Windows, so we cannot test */
        return;
    }

    GetWindowsDirectoryA(path2, sizeof(path2));
    strcat(path2, "\\system32\\");
    strcat(path2, dllname);
    hModule2 = LoadLibraryA(path2);
    ok(hModule2 != NULL, "LoadLibrary(%s) failed\n", path2);

    /* The first LoadLibrary() call may have registered the dll under the
     * system32 path. So load it, again, under the '...\system\...' path so
     * Wine does not immediately notice that it is already loaded.
     */
    hModule3 = LoadLibraryA(path1);
    ok(hModule3 != NULL, "LoadLibrary(%s) failed\n", path1);

    /* Now fully unload the dll */
    ok(FreeLibrary(hModule3), "FreeLibrary() failed\n");
    ok(FreeLibrary(hModule2), "FreeLibrary() failed\n");
    ok(FreeLibrary(hModule1), "FreeLibrary() failed\n");
    ok(GetModuleHandleA(dllname) == NULL, "%s was not fully unloaded\n", dllname);

    /* Try to load the dll again, if refcounting is ok, this should work */
    hModule1 = LoadLibraryA(path1);
    ok(hModule1 != NULL, "LoadLibrary(%s) failed\n", path1);
    if (hModule1 != NULL)
        ok(FreeLibrary(hModule1), "FreeLibrary() failed\n");
}

static void testLoadLibraryA_Wrong(void)
{
    HMODULE hModule;

    /* Try to load a nonexistent dll */
    SetLastError(0xdeadbeef);
    hModule = LoadLibraryA("non_ex_pv.dll");
    ok( !hModule, "non_ex_pv.dll should be not loadable\n");
    ok( GetLastError() == ERROR_MOD_NOT_FOUND, "Expected ERROR_MOD_NOT_FOUND, got %d\n", GetLastError() );

    /* Just in case */
    FreeLibrary(hModule);
}

static void testGetProcAddress_Wrong(void)
{
    FARPROC fp;

    SetLastError(0xdeadbeef);
    fp = GetProcAddress(NULL, "non_ex_call");
    ok( !fp, "non_ex_call should not be found\n");
    ok( GetLastError() == ERROR_PROC_NOT_FOUND, "Expected ERROR_PROC_NOT_FOUND, got %d\n", GetLastError() );

    SetLastError(0xdeadbeef);
    fp = GetProcAddress((HMODULE)0xdeadbeef, "non_ex_call");
    ok( !fp, "non_ex_call should not be found\n");
    ok( GetLastError() == ERROR_MOD_NOT_FOUND, "Expected ERROR_MOD_NOT_FOUND, got %d\n", GetLastError() );
}

static void testLoadLibraryEx(void)
{
    CHAR path[MAX_PATH];
    HMODULE hmodule;
    HANDLE hfile;
    BOOL ret;

    hfile = CreateFileA("testfile.dll", GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE,
                        NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    ok(hfile != INVALID_HANDLE_VALUE, "Expected a valid file handle\n");

    /* NULL lpFileName */
    SetLastError(0xdeadbeef);
    hmodule = LoadLibraryExA(NULL, NULL, 0);
    ok(hmodule == 0, "Expected 0, got %p\n", hmodule);
    ok(GetLastError() == ERROR_MOD_NOT_FOUND ||
       GetLastError() == ERROR_INVALID_PARAMETER,
       "Expected ERROR_MOD_NOT_FOUND or ERROR_INVALID_PARAMETER, got %d\n", GetLastError());

    /* empty lpFileName */
    SetLastError(0xdeadbeef);
    hmodule = LoadLibraryExA("", NULL, 0);
    ok(hmodule == 0, "Expected 0, got %p\n", hmodule);
    ok(GetLastError() == ERROR_MOD_NOT_FOUND ||
       GetLastError() == ERROR_INVALID_PARAMETER /* win8 */,
       "Expected ERROR_MOD_NOT_FOUND or ERROR_DLL_NOT_FOUND, got %d\n", GetLastError());

    /* hFile is non-NULL */
    SetLastError(0xdeadbeef);
    hmodule = LoadLibraryExA("testfile.dll", hfile, 0);
    ok(hmodule == 0, "Expected 0, got %p\n", hmodule);
    ok(GetLastError() == ERROR_SHARING_VIOLATION ||
       GetLastError() == ERROR_INVALID_PARAMETER, /* win2k3 */
       "Unexpected last error, got %d\n", GetLastError());

    SetLastError(0xdeadbeef);
    hmodule = LoadLibraryExA("testfile.dll", (HANDLE)0xdeadbeef, 0);
    ok(hmodule == 0, "Expected 0, got %p\n", hmodule);
    ok(GetLastError() == ERROR_SHARING_VIOLATION ||
       GetLastError() == ERROR_INVALID_PARAMETER, /* win2k3 */
       "Unexpected last error, got %d\n", GetLastError());

    /* try to open a file that is locked */
    SetLastError(0xdeadbeef);
    hmodule = LoadLibraryExA("testfile.dll", NULL, 0);
    ok(hmodule == 0, "Expected 0, got %p\n", hmodule);
    ok(GetLastError() == ERROR_SHARING_VIOLATION,
       "Expected ERROR_SHARING_VIOLATION, got %d\n", GetLastError());

    /* lpFileName does not matter */
    if (is_unicode_enabled)
    {
        SetLastError(0xdeadbeef);
        hmodule = LoadLibraryExA(NULL, hfile, 0);
        ok(hmodule == 0, "Expected 0, got %p\n", hmodule);
        ok(GetLastError() == ERROR_MOD_NOT_FOUND ||
           GetLastError() == ERROR_INVALID_PARAMETER, /* win2k3 */
           "Expected ERROR_MOD_NOT_FOUND or ERROR_INVALID_PARAMETER, got %d\n", GetLastError());
    }

    CloseHandle(hfile);

    /* load empty file */
    SetLastError(0xdeadbeef);
    hmodule = LoadLibraryExA("testfile.dll", NULL, LOAD_LIBRARY_AS_DATAFILE);
    ok(hmodule == 0, "Expected 0, got %p\n", hmodule);
    ok(GetLastError() == ERROR_FILE_INVALID, "Expected ERROR_FILE_INVALID, got %d\n", GetLastError());

    DeleteFileA("testfile.dll");

    GetSystemDirectoryA(path, MAX_PATH);
    if (path[lstrlenA(path) - 1] != '\\')
        lstrcatA(path, "\\");
    lstrcatA(path, "kernel32.dll");

    /* load kernel32.dll with an absolute path */
    SetLastError(0xdeadbeef);
    hmodule = LoadLibraryExA(path, NULL, LOAD_LIBRARY_AS_DATAFILE);
    ok(hmodule != 0, "Expected valid module handle\n");
    ok(GetLastError() == 0xdeadbeef ||
       GetLastError() == ERROR_SUCCESS,
       "Expected 0xdeadbeef or ERROR_SUCCESS, got %d\n", GetLastError());

    /* try invalid file handle */
    SetLastError(0xdeadbeef);
    hmodule = LoadLibraryExA(path, (HANDLE)0xdeadbeef, 0);
    if (!hmodule)  /* succeeds on xp and older */
        ok(GetLastError() == ERROR_INVALID_PARAMETER, "wrong error %u\n", GetLastError());

    FreeLibrary(hmodule);

    /* load kernel32.dll with no path */
    SetLastError(0xdeadbeef);
    hmodule = LoadLibraryExA("kernel32.dll", NULL, LOAD_LIBRARY_AS_DATAFILE);
    ok(hmodule != 0, "Expected valid module handle\n");
    ok(GetLastError() == 0xdeadbeef, "Expected 0xdeadbeef, got %d\n", GetLastError());

    FreeLibrary(hmodule);

    GetCurrentDirectoryA(MAX_PATH, path);
    if (path[lstrlenA(path) - 1] != '\\')
        lstrcatA(path, "\\");
    lstrcatA(path, "kernel32.dll");

    /* load kernel32.dll with an absolute path that does not exist */
    SetLastError(0xdeadbeef);
    hmodule = LoadLibraryExA(path, NULL, LOAD_LIBRARY_AS_DATAFILE);
    ok(hmodule == 0, "Expected 0, got %p\n", hmodule);
    ok(GetLastError() == ERROR_FILE_NOT_FOUND,
       "Expected ERROR_FILE_NOT_FOUND, got %d\n", GetLastError());

    /* Free the loaded dll when it's the first time this dll is loaded
       in process - First time should pass, second fail */
    SetLastError(0xdeadbeef);
    hmodule = LoadLibraryExA("comctl32.dll", NULL, LOAD_LIBRARY_AS_DATAFILE);
    ok(hmodule != 0, "Expected valid module handle\n");

    SetLastError(0xdeadbeef);
    ret = FreeLibrary( (HMODULE)((ULONG_PTR)hmodule + 0x1230));
    ok(!ret, "Free succeeded on wrong handle\n");
    ok(GetLastError() == ERROR_BAD_EXE_FORMAT, "wrong error %u\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = FreeLibrary(hmodule);
    ok(ret, "Expected to be able to free the module, failed with %d\n", GetLastError());
    SetLastError(0xdeadbeef);
    ret = FreeLibrary(hmodule);
    ok(!ret, "Unexpected ability to free the module, failed with %d\n", GetLastError());

    /* load with full path, name without extension */
    GetSystemDirectoryA(path, MAX_PATH);
    if (path[lstrlenA(path) - 1] != '\\')
        lstrcatA(path, "\\");
    lstrcatA(path, "kernel32");
    hmodule = LoadLibraryExA(path, NULL, 0);
    ok(hmodule != NULL, "got %p\n", hmodule);
    FreeLibrary(hmodule);

    /* same with alterate search path */
    hmodule = LoadLibraryExA(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    ok(hmodule != NULL, "got %p\n", hmodule);
    FreeLibrary(hmodule);
}

static void test_LoadLibraryEx_search_flags(void)
{
    static const struct
    {
        int add_dirs[4];
        int dll_dir;
        int expect;
    } tests[] =
    {
        { { 1, 2, 3 }, 4, 3 }, /* 0 */
        { { 1, 3, 2 }, 4, 2 },
        { { 3, 1 },    4, 1 },
        { { 5, 6 },    4, 4 },
        { { 5, 2 },    4, 2 },
        { { 0 },       4, 4 }, /* 5 */
        { { 0 },       0, 0 },
        { { 6, 5 },    5, 0 },
        { { 1, 1, 2 }, 0, 2 },
    };
    char *p, path[MAX_PATH], buf[MAX_PATH], curdir[MAX_PATH];
    WCHAR bufW[MAX_PATH];
    DLL_DIRECTORY_COOKIE cookies[4];
    unsigned int i, j, k;
    BOOL ret;
    HMODULE mod;

    GetTempPathA( sizeof(path), path );
    GetTempFileNameA( path, "tmp", 0, buf );
    DeleteFileA( buf );
    ret = CreateDirectoryA( buf, NULL );
    ok( ret, "CreateDirectory failed err %u\n", GetLastError() );
    p = buf + strlen( buf );
    for (i = 1; i <= 6; i++)
    {
        sprintf( p, "\\%u", i );
        ret = CreateDirectoryA( buf, NULL );
        ok( ret, "CreateDirectory failed err %u\n", GetLastError() );
        if (i >= 5) continue;  /* dirs 5 and 6 are left empty */
        sprintf( p, "\\%u\\winetestdll.dll", i );
        create_test_dll( buf );
    }

    GetCurrentDirectoryA( MAX_PATH, curdir );
    *p = 0;
    SetCurrentDirectoryA( buf );

    SetLastError( 0xdeadbeef );
    mod = LoadLibraryA( "1\\winetestdll.dll" );
    ok( mod != NULL, "LoadLibrary failed err %u\n", GetLastError() );
    FreeLibrary( mod );

    SetLastError( 0xdeadbeef );
    sprintf( path, "%c:1\\winetestdll.dll", buf[0] );
    mod = LoadLibraryA( path );
    ok( mod != NULL, "LoadLibrary failed err %u\n", GetLastError() );
    FreeLibrary( mod );

    if (pAddDllDirectory)
    {
        SetLastError( 0xdeadbeef );
        mod = LoadLibraryExA( "1\\winetestdll.dll", 0, LOAD_LIBRARY_SEARCH_SYSTEM32 );
        ok( !mod, "LoadLibrary succeeded\n" );
        ok( GetLastError() == ERROR_MOD_NOT_FOUND, "wrong error %u\n", GetLastError() );

        SetLastError( 0xdeadbeef );
        mod = LoadLibraryExA( path, 0, LOAD_LIBRARY_SEARCH_SYSTEM32 );
        ok( mod != NULL, "LoadLibrary failed err %u\n", GetLastError() );
        FreeLibrary( mod );
    }

    strcpy( p, "\\1" );
    SetCurrentDirectoryA( buf );

    SetLastError( 0xdeadbeef );
    mod = LoadLibraryA( "winetestdll.dll" );
    ok( mod != NULL, "LoadLibrary failed err %u\n", GetLastError() );
    FreeLibrary( mod );

    SetLastError( 0xdeadbeef );
    sprintf( path, "%c:winetestdll.dll", buf[0] );
    mod = LoadLibraryA( path );
    ok( mod != NULL || broken(!mod), /* win10 disallows this but allows c:1\\winetestdll.dll */
        "LoadLibrary failed err %u\n", GetLastError() );
    if (!mod) ok( GetLastError() == ERROR_MOD_NOT_FOUND, "wrong error %u\n", GetLastError() );
    else FreeLibrary( mod );

    SetLastError( 0xdeadbeef );
    sprintf( path, "%s\\winetestdll.dll", buf + 2 );
    mod = LoadLibraryA( path );
    ok( mod != NULL, "LoadLibrary failed err %u\n", GetLastError() );
    FreeLibrary( mod );

    if (pAddDllDirectory)
    {
        SetLastError( 0xdeadbeef );
        mod = LoadLibraryExA( "winetestdll.dll", 0, LOAD_LIBRARY_SEARCH_SYSTEM32 );
        ok( !mod, "LoadLibrary succeeded\n" );
        ok( GetLastError() == ERROR_MOD_NOT_FOUND, "wrong error %u\n", GetLastError() );

        SetLastError( 0xdeadbeef );
        mod = LoadLibraryExA( path, 0, LOAD_LIBRARY_SEARCH_SYSTEM32 );
        ok( mod != NULL, "LoadLibrary failed err %u\n", GetLastError() );
        FreeLibrary( mod );

        SetLastError( 0xdeadbeef );
        sprintf( path, "%s\\winetestdll.dll", buf + 2 );
        mod = LoadLibraryExA( path, 0, LOAD_LIBRARY_SEARCH_SYSTEM32 );
        ok( mod != NULL, "LoadLibrary failed err %u\n", GetLastError() );
        FreeLibrary( mod );

        SetLastError( 0xdeadbeef );
        mod = LoadLibraryExA( "winetestdll.dll", 0, LOAD_LIBRARY_SEARCH_APPLICATION_DIR | LOAD_WITH_ALTERED_SEARCH_PATH );
        ok( !mod, "LoadLibrary succeeded\n" );
        ok( GetLastError() == ERROR_INVALID_PARAMETER, "wrong error %u\n", GetLastError() );

        SetLastError( 0xdeadbeef );
        mod = LoadLibraryExA( "winetestdll.dll", 0, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_WITH_ALTERED_SEARCH_PATH );
        ok( !mod, "LoadLibrary succeeded\n" );
        ok( GetLastError() == ERROR_INVALID_PARAMETER, "wrong error %u\n", GetLastError() );

        SetLastError( 0xdeadbeef );
        mod = LoadLibraryExA( "winetestdll.dll", 0, LOAD_LIBRARY_SEARCH_SYSTEM32 | LOAD_WITH_ALTERED_SEARCH_PATH );
        ok( !mod, "LoadLibrary succeeded\n" );
        ok( GetLastError() == ERROR_INVALID_PARAMETER, "wrong error %u\n", GetLastError() );

        SetLastError( 0xdeadbeef );
        mod = LoadLibraryExA( "winetestdll.dll", 0, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_WITH_ALTERED_SEARCH_PATH );
        ok( !mod, "LoadLibrary succeeded\n" );
        ok( GetLastError() == ERROR_INVALID_PARAMETER, "wrong error %u\n", GetLastError() );

        SetLastError( 0xdeadbeef );
        mod = LoadLibraryExA( "winetestdll.dll", 0, LOAD_LIBRARY_SEARCH_USER_DIRS | LOAD_WITH_ALTERED_SEARCH_PATH );
        ok( !mod, "LoadLibrary succeeded\n" );
        ok( GetLastError() == ERROR_INVALID_PARAMETER, "wrong error %u\n", GetLastError() );
    }

    SetCurrentDirectoryA( curdir );

    if (!pAddDllDirectory || !pSetDllDirectoryA) goto done;

    SetLastError( 0xdeadbeef );
    mod = LoadLibraryExA( "winetestdll.dll", 0, LOAD_LIBRARY_SEARCH_APPLICATION_DIR );
    ok( !mod, "LoadLibrary succeeded\n" );
    ok( GetLastError() == ERROR_MOD_NOT_FOUND, "wrong error %u\n", GetLastError() );

    if (0)  /* crashes on win10 */
    {
    SetLastError( 0xdeadbeef );
    mod = LoadLibraryExA( "winetestdll.dll", 0, LOAD_LIBRARY_SEARCH_USER_DIRS );
    ok( !mod, "LoadLibrary succeeded\n" );
    ok( GetLastError() == ERROR_MOD_NOT_FOUND || broken(GetLastError() == ERROR_NOT_ENOUGH_MEMORY),
        "wrong error %u\n", GetLastError() );
    }

    SetLastError( 0xdeadbeef );
    mod = LoadLibraryExA( "winetestdll.dll", 0, LOAD_LIBRARY_SEARCH_SYSTEM32 );
    ok( !mod, "LoadLibrary succeeded\n" );
    ok( GetLastError() == ERROR_MOD_NOT_FOUND, "wrong error %u\n", GetLastError() );

    SetLastError( 0xdeadbeef );
    mod = LoadLibraryExA( "winetestdll.dll", 0, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR );
    ok( !mod, "LoadLibrary succeeded\n" );
    ok( GetLastError() == ERROR_INVALID_PARAMETER, "wrong error %u\n", GetLastError() );

    SetLastError( 0xdeadbeef );
    mod = LoadLibraryExA( "winetestdll.dll", 0, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32 );
    ok( !mod, "LoadLibrary succeeded\n" );
    ok( GetLastError() == ERROR_INVALID_PARAMETER, "wrong error %u\n", GetLastError() );

    SetLastError( 0xdeadbeef );
    mod = LoadLibraryExA( "foo\\winetestdll.dll", 0, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR );
    ok( !mod, "LoadLibrary succeeded\n" );
    ok( GetLastError() == ERROR_INVALID_PARAMETER, "wrong error %u\n", GetLastError() );

    SetLastError( 0xdeadbeef );
    mod = LoadLibraryExA( "\\windows\\winetestdll.dll", 0, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR );
    ok( !mod, "LoadLibrary succeeded\n" );
    ok( GetLastError() == ERROR_MOD_NOT_FOUND, "wrong error %u\n", GetLastError() );

    SetLastError( 0xdeadbeef );
    mod = LoadLibraryA( "1\\winetestdll.dll" );
    ok( !mod, "LoadLibrary succeeded\n" );
    ok( GetLastError() == ERROR_MOD_NOT_FOUND, "wrong error %u\n", GetLastError() );

    for (j = 0; j < ARRAY_SIZE(tests); j++)
    {
        for (k = 0; tests[j].add_dirs[k]; k++)
        {
            sprintf( p, "\\%u", tests[j].add_dirs[k] );
            MultiByteToWideChar( CP_ACP, 0, buf, -1, bufW, MAX_PATH );
            cookies[k] = pAddDllDirectory( bufW );
            ok( cookies[k] != NULL, "failed to add %s\n", buf );
        }
        if (tests[j].dll_dir)
        {
            sprintf( p, "\\%u", tests[j].dll_dir );
            pSetDllDirectoryA( buf );
        }
        else pSetDllDirectoryA( NULL );

        SetLastError( 0xdeadbeef );
        mod = LoadLibraryExA( "winetestdll.dll", 0, LOAD_LIBRARY_SEARCH_USER_DIRS );
        if (tests[j].expect)
        {
            ok( mod != NULL, "%u: LoadLibrary failed err %u\n", j, GetLastError() );
            GetModuleFileNameA( mod, path, MAX_PATH );
            sprintf( p, "\\%u\\winetestdll.dll", tests[j].expect );
            ok( !lstrcmpiA( path, buf ), "%u: wrong module %s expected %s\n", j, path, buf );
        }
        else
        {
            ok( !mod, "%u: LoadLibrary succeeded\n", j );
            ok( GetLastError() == ERROR_MOD_NOT_FOUND || broken(GetLastError() == ERROR_NOT_ENOUGH_MEMORY),
                "%u: wrong error %u\n", j, GetLastError() );
        }
        FreeLibrary( mod );

        for (k = 0; tests[j].add_dirs[k]; k++) pRemoveDllDirectory( cookies[k] );
    }

done:
    for (i = 1; i <= 6; i++)
    {
        sprintf( p, "\\%u\\winetestdll.dll", i );
        DeleteFileA( buf );
        sprintf( p, "\\%u", i );
        RemoveDirectoryA( buf );
    }
    *p = 0;
    RemoveDirectoryA( buf );
}

static void testGetDllDirectory(void)
{
    CHAR bufferA[MAX_PATH];
    WCHAR bufferW[MAX_PATH];
    DWORD length, ret;
    int i;
    static const char *dll_directories[] =
    {
        "",
        "C:\\Some\\Path",
        "C:\\Some\\Path\\",
        "Q:\\A\\Long\\Path with spaces that\\probably\\doesn't exist!",
    };
    const int test_count = ARRAY_SIZE(dll_directories);

    if (!pGetDllDirectoryA || !pGetDllDirectoryW)
    {
        win_skip("GetDllDirectory not available\n");
        return;
    }
    if (!pSetDllDirectoryA)
    {
        win_skip("SetDllDirectoryA not available\n");
        return;
    }

    for (i = 0; i < test_count; i++)
    {
        length = strlen(dll_directories[i]);
        if (!pSetDllDirectoryA(dll_directories[i]))
        {
            skip("i=%d, SetDllDirectoryA failed\n", i);
            continue;
        }

        /* no buffer, determine length */
        ret = pGetDllDirectoryA(0, NULL);
        ok(ret == length + 1, "Expected %u, got %u\n", length + 1, ret);

        ret = pGetDllDirectoryW(0, NULL);
        ok(ret == length + 1, "Expected %u, got %u\n", length + 1, ret);

        /* buffer of exactly the right size */
        bufferA[length] = 'A';
        bufferA[length + 1] = 'A';
        ret = pGetDllDirectoryA(length + 1, bufferA);
        ok(ret == length || broken(ret + 1 == length) /* win8 */,
           "i=%d, Expected %u(+1), got %u\n", i, length, ret);
        ok(bufferA[length + 1] == 'A', "i=%d, Buffer overflow\n", i);
        ok(strcmp(bufferA, dll_directories[i]) == 0, "i=%d, Wrong path returned: '%s'\n", i, bufferA);

        bufferW[length] = 'A';
        bufferW[length + 1] = 'A';
        ret = pGetDllDirectoryW(length + 1, bufferW);
        ok(ret == length, "i=%d, Expected %u, got %u\n", i, length, ret);
        ok(bufferW[length + 1] == 'A', "i=%d, Buffer overflow\n", i);
        ok(cmpStrAW(dll_directories[i], bufferW, length, length),
           "i=%d, Wrong path returned: %s\n", i, wine_dbgstr_w(bufferW));

        /* Zero size buffer. The buffer may or may not be terminated depending
         * on the Windows version and whether the A or W API is called. */
        bufferA[0] = 'A';
        ret = pGetDllDirectoryA(0, bufferA);
        ok(ret == length + 1, "i=%d, Expected %u, got %u\n", i, length + 1, ret);

        bufferW[0] = 'A';
        ret = pGetDllDirectoryW(0, bufferW);
        ok(ret == length + 1, "i=%d, Expected %u, got %u\n", i, length + 1, ret);
        ok(bufferW[0] == 'A' || broken(bufferW[0] == 0), /* XP, 2003 */
           "i=%d, Buffer overflow\n", i);

        /* buffer just one too short */
        bufferA[0] = 'A';
        ret = pGetDllDirectoryA(length, bufferA);
        ok(ret == length + 1, "i=%d, Expected %u, got %u\n", i, length + 1, ret);
        if (length != 0)
            ok(bufferA[0] == 0, "i=%d, Buffer not null terminated\n", i);

        bufferW[0] = 'A';
        ret = pGetDllDirectoryW(length, bufferW);
        ok(ret == length + 1, "i=%d, Expected %u, got %u\n", i, length + 1, ret);
        if (length != 0)
            ok(bufferW[0] == 0, "i=%d, Buffer overflow\n", i);

        if (0)
        {
            /* crashes on win8 */
            /* no buffer, but too short length */
            ret = pGetDllDirectoryA(length, NULL);
            ok(ret == length + 1, "i=%d, Expected %u, got %u\n", i, length + 1, ret);

            ret = pGetDllDirectoryW(length, NULL);
            ok(ret == length + 1, "i=%d, Expected %u, got %u\n", i, length + 1, ret);
        }

        if (pLdrGetDllDirectory)
        {
            UNICODE_STRING str;
            NTSTATUS status;
            str.Buffer = bufferW;
            str.MaximumLength = sizeof(bufferW);
            status = pLdrGetDllDirectory( &str );
            ok( !status, "LdrGetDllDirectory failed %x\n", status );
            ok( cmpStrAW( dll_directories[i], bufferW, strlen(dll_directories[i]),
                          str.Length / sizeof(WCHAR) ), "%u: got %s instead of %s\n",
                i, wine_dbgstr_w(bufferW), dll_directories[i] );
            if (dll_directories[i][0])
            {
                memset( bufferW, 0xcc, sizeof(bufferW) );
                str.MaximumLength = (strlen( dll_directories[i] ) - 1) * sizeof(WCHAR);
                status = pLdrGetDllDirectory( &str );
                ok( status == STATUS_BUFFER_TOO_SMALL, "%u: LdrGetDllDirectory failed %x\n", i, status );
                ok( bufferW[0] == 0 && bufferW[1] == 0xcccc,
                    "%u: buffer %x %x\n", i, bufferW[0], bufferW[1] );
                length = (strlen( dll_directories[i] ) + 1) * sizeof(WCHAR);
                ok( str.Length == length, "%u: wrong len %u / %u\n", i, str.Length, length );
            }
        }
    }

    /* unset whatever we did so following tests won't be affected */
    pSetDllDirectoryA(NULL);
}

static void init_pointers(void)
{
    HMODULE mod = GetModuleHandleA("kernel32.dll");

#define MAKEFUNC(f) (p##f = (void*)GetProcAddress(mod, #f))
    MAKEFUNC(GetDllDirectoryA);
    MAKEFUNC(GetDllDirectoryW);
    MAKEFUNC(SetDllDirectoryA);
    MAKEFUNC(AddDllDirectory);
    MAKEFUNC(RemoveDllDirectory);
    MAKEFUNC(SetDefaultDllDirectories);
    MAKEFUNC(K32GetModuleInformation);
    mod = GetModuleHandleA( "ntdll.dll" );
    MAKEFUNC(LdrGetDllDirectory);
    MAKEFUNC(LdrSetDllDirectory);
    MAKEFUNC(LdrGetDllHandle);
    MAKEFUNC(LdrGetDllHandleEx);
    MAKEFUNC(LdrGetDllFullName);
#undef MAKEFUNC

    /* before Windows 7 this was not exported in kernel32 */
    if (!pK32GetModuleInformation)
    {
        HMODULE hPsapi = LoadLibraryA("psapi.dll");
        pK32GetModuleInformation = (void *)GetProcAddress(hPsapi, "GetModuleInformation");
    }
}

static void testGetModuleHandleEx(void)
{
    static const WCHAR kernel32W[] = {'k','e','r','n','e','l','3','2',0};
    static const WCHAR nosuchmodW[] = {'n','o','s','u','c','h','m','o','d',0};
    BOOL ret;
    DWORD error;
    HMODULE mod, mod_kernel32;

    SetLastError( 0xdeadbeef );
    ret = GetModuleHandleExA( 0, NULL, NULL );
    error = GetLastError();
    ok( !ret, "unexpected success\n" );
    ok( error == ERROR_INVALID_PARAMETER, "got %u\n", error );

    SetLastError( 0xdeadbeef );
    ret = GetModuleHandleExA( 0, "kernel32", NULL );
    error = GetLastError();
    ok( !ret, "unexpected success\n" );
    ok( error == ERROR_INVALID_PARAMETER, "got %u\n", error );

    SetLastError( 0xdeadbeef );
    mod = (HMODULE)0xdeadbeef;
    ret = GetModuleHandleExA( 0, "kernel32", &mod );
    ok( ret, "unexpected failure %u\n", GetLastError() );
    ok( mod != (HMODULE)0xdeadbeef, "got %p\n", mod );
    FreeLibrary( mod );

    SetLastError( 0xdeadbeef );
    mod = (HMODULE)0xdeadbeef;
    ret = GetModuleHandleExA( 0, "nosuchmod", &mod );
    error = GetLastError();
    ok( !ret, "unexpected success\n" );
    ok( error == ERROR_MOD_NOT_FOUND, "got %u\n", error );
    ok( mod == NULL, "got %p\n", mod );

    SetLastError( 0xdeadbeef );
    ret = GetModuleHandleExW( 0, NULL, NULL );
    error = GetLastError();
    ok( !ret, "unexpected success\n" );
    ok( error == ERROR_INVALID_PARAMETER, "got %u\n", error );

    SetLastError( 0xdeadbeef );
    ret = GetModuleHandleExW( 0, kernel32W, NULL );
    error = GetLastError();
    ok( !ret, "unexpected success\n" );
    ok( error == ERROR_INVALID_PARAMETER, "got %u\n", error );

    SetLastError( 0xdeadbeef );
    mod = (HMODULE)0xdeadbeef;
    ret = GetModuleHandleExW( 0, kernel32W, &mod );
    ok( ret, "unexpected failure %u\n", GetLastError() );
    ok( mod != (HMODULE)0xdeadbeef, "got %p\n", mod );
    FreeLibrary( mod );

    SetLastError( 0xdeadbeef );
    mod = (HMODULE)0xdeadbeef;
    ret = GetModuleHandleExW( 0, nosuchmodW, &mod );
    error = GetLastError();
    ok( !ret, "unexpected success\n" );
    ok( error == ERROR_MOD_NOT_FOUND, "got %u\n", error );
    ok( mod == NULL, "got %p\n", mod );

    SetLastError( 0xdeadbeef );
    ret = GetModuleHandleExA( GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, NULL, NULL );
    error = GetLastError();
    ok( !ret, "unexpected success\n" );
    ok( error == ERROR_INVALID_PARAMETER, "got %u\n", error );

    SetLastError( 0xdeadbeef );
    ret = GetModuleHandleExA( GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, "kernel32", NULL );
    error = GetLastError();
    ok( !ret, "unexpected success\n" );
    ok( error == ERROR_INVALID_PARAMETER, "got %u\n", error );

    SetLastError( 0xdeadbeef );
    mod = (HMODULE)0xdeadbeef;
    ret = GetModuleHandleExA( GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, "kernel32", &mod );
    ok( ret, "unexpected failure %u\n", GetLastError() );
    ok( mod != (HMODULE)0xdeadbeef, "got %p\n", mod );

    SetLastError( 0xdeadbeef );
    mod = (HMODULE)0xdeadbeef;
    ret = GetModuleHandleExA( GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, "nosuchmod", &mod );
    error = GetLastError();
    ok( !ret, "unexpected success\n" );
    ok( error == ERROR_MOD_NOT_FOUND, "got %u\n", error );
    ok( mod == NULL, "got %p\n", mod );

    SetLastError( 0xdeadbeef );
    ret = GetModuleHandleExW( GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, NULL, NULL );
    error = GetLastError();
    ok( !ret, "unexpected success\n" );
    ok( error == ERROR_INVALID_PARAMETER, "got %u\n", error );

    SetLastError( 0xdeadbeef );
    ret = GetModuleHandleExW( GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, kernel32W, NULL );
    error = GetLastError();
    ok( !ret, "unexpected success\n" );
    ok( error == ERROR_INVALID_PARAMETER, "got %u\n", error );

    SetLastError( 0xdeadbeef );
    mod = (HMODULE)0xdeadbeef;
    ret = GetModuleHandleExW( GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, kernel32W, &mod );
    ok( ret, "unexpected failure %u\n", GetLastError() );
    ok( mod != (HMODULE)0xdeadbeef, "got %p\n", mod );

    SetLastError( 0xdeadbeef );
    mod = (HMODULE)0xdeadbeef;
    ret = GetModuleHandleExW( GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, nosuchmodW, &mod );
    error = GetLastError();
    ok( !ret, "unexpected success\n" );
    ok( error == ERROR_MOD_NOT_FOUND, "got %u\n", error );
    ok( mod == NULL, "got %p\n", mod );

    mod_kernel32 = LoadLibraryA( "kernel32" );

    SetLastError( 0xdeadbeef );
    ret = GetModuleHandleExA( GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, NULL, NULL );
    error = GetLastError();
    ok( !ret, "unexpected success\n" );
    ok( error == ERROR_INVALID_PARAMETER, "got %u\n", error );

    SetLastError( 0xdeadbeef );
    ret = GetModuleHandleExA( GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCSTR)mod_kernel32, NULL );
    error = GetLastError();
    ok( !ret, "unexpected success\n" );
    ok( error == ERROR_INVALID_PARAMETER, "got %u\n", error );

    SetLastError( 0xdeadbeef );
    mod = (HMODULE)0xdeadbeef;
    ret = GetModuleHandleExA( GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCSTR)mod_kernel32, &mod );
    ok( ret, "unexpected failure %u\n", GetLastError() );
    ok( mod == mod_kernel32, "got %p\n", mod );
    FreeLibrary( mod );

    SetLastError( 0xdeadbeef );
    mod = (HMODULE)0xdeadbeef;
    ret = GetModuleHandleExA( GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCSTR)0xbeefdead, &mod );
    error = GetLastError();
    ok( !ret, "unexpected success\n" );
    ok( error == ERROR_MOD_NOT_FOUND, "got %u\n", error );
    ok( mod == NULL, "got %p\n", mod );

    SetLastError( 0xdeadbeef );
    ret = GetModuleHandleExW( GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, NULL, NULL );
    error = GetLastError();
    ok( !ret, "unexpected success\n" );
    ok( error == ERROR_INVALID_PARAMETER, "got %u\n", error );

    SetLastError( 0xdeadbeef );
    ret = GetModuleHandleExW( GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR)mod_kernel32, NULL );
    error = GetLastError();
    ok( !ret, "unexpected success\n" );
    ok( error == ERROR_INVALID_PARAMETER, "got %u\n", error );

    SetLastError( 0xdeadbeef );
    mod = (HMODULE)0xdeadbeef;
    ret = GetModuleHandleExW( GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR)mod_kernel32, &mod );
    ok( ret, "unexpected failure %u\n", GetLastError() );
    ok( mod == mod_kernel32, "got %p\n", mod );
    FreeLibrary( mod );

    SetLastError( 0xdeadbeef );
    mod = (HMODULE)0xdeadbeef;
    ret = GetModuleHandleExW( GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR)0xbeefdead, &mod );
    error = GetLastError();
    ok( !ret, "unexpected success\n" );
    ok( error == ERROR_MOD_NOT_FOUND, "got %u\n", error );
    ok( mod == NULL, "got %p\n", mod );

    SetLastError( 0xdeadbeef );
    mod = (HMODULE)0xdeadbeef;
    ret = GetModuleHandleExW( GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT
                              | GET_MODULE_HANDLE_EX_FLAG_PIN, (LPCWSTR)mod_kernel32, &mod );
    error = GetLastError();
    ok( !ret, "unexpected success\n" );
    ok( error == ERROR_INVALID_PARAMETER, "got %u\n", error );
    ok( mod == NULL, "got %p\n", mod );

    SetLastError( 0xdeadbeef );
    mod = (HMODULE)0xdeadbeef;
    ret = GetModuleHandleExW( 8, kernel32W, &mod );
    error = GetLastError();
    ok( !ret, "unexpected success\n" );
    ok( error == ERROR_INVALID_PARAMETER, "got %u\n", error );
    ok( mod == NULL, "got %p\n", mod );

    FreeLibrary( mod_kernel32 );
}

static void testK32GetModuleInformation(void)
{
    MODULEINFO info;
    HMODULE mod;
    BOOL ret;

    mod = GetModuleHandleA(NULL);
    memset(&info, 0xAA, sizeof(info));
    ret = pK32GetModuleInformation(GetCurrentProcess(), mod, &info, sizeof(info));
    ok(ret, "K32GetModuleInformation failed for main module\n");
    ok(info.lpBaseOfDll == mod, "Wrong info.lpBaseOfDll = %p, expected %p\n", info.lpBaseOfDll, mod);
    ok(info.EntryPoint != NULL, "Expected nonzero entrypoint\n");

    mod = GetModuleHandleA("kernel32.dll");
    memset(&info, 0xAA, sizeof(info));
    ret = pK32GetModuleInformation(GetCurrentProcess(), mod, &info, sizeof(info));
    ok(ret, "K32GetModuleInformation failed for kernel32 module\n");
    ok(info.lpBaseOfDll == mod, "Wrong info.lpBaseOfDll = %p, expected %p\n", info.lpBaseOfDll, mod);
    ok(info.EntryPoint != NULL, "Expected nonzero entrypoint\n");
}

static void test_AddDllDirectory(void)
{
    static const WCHAR tmpW[] = {'t','m','p',0};
    static const WCHAR dotW[] = {'.','\\','.',0};
    static const WCHAR rootW[] = {'\\',0};
    WCHAR path[MAX_PATH], buf[MAX_PATH];
    DLL_DIRECTORY_COOKIE cookie;
    BOOL ret;

    if (!pAddDllDirectory || !pRemoveDllDirectory)
    {
        win_skip( "AddDllDirectory not available\n" );
        return;
    }

    buf[0] = '\0';
    GetTempPathW(ARRAY_SIZE(path), path );
    ret = GetTempFileNameW( path, tmpW, 0, buf );
    ok( ret, "GetTempFileName failed err %u\n", GetLastError() );
    SetLastError( 0xdeadbeef );
    cookie = pAddDllDirectory( buf );
    ok( cookie != NULL, "AddDllDirectory failed err %u\n", GetLastError() );
    SetLastError( 0xdeadbeef );
    ret = pRemoveDllDirectory( cookie );
    ok( ret, "RemoveDllDirectory failed err %u\n", GetLastError() );

    DeleteFileW( buf );
    SetLastError( 0xdeadbeef );
    cookie = pAddDllDirectory( buf );
    ok( !cookie, "AddDllDirectory succeeded\n" );
    ok( GetLastError() == ERROR_FILE_NOT_FOUND, "wrong error %u\n", GetLastError() );
    cookie = pAddDllDirectory( dotW );
    ok( !cookie, "AddDllDirectory succeeded\n" );
    ok( GetLastError() == ERROR_INVALID_PARAMETER, "wrong error %u\n", GetLastError() );
    cookie = pAddDllDirectory( rootW );
    ok( cookie != NULL, "AddDllDirectory failed err %u\n", GetLastError() );
    SetLastError( 0xdeadbeef );
    ret = pRemoveDllDirectory( cookie );
    ok( ret, "RemoveDllDirectory failed err %u\n", GetLastError() );
    GetWindowsDirectoryW( buf, MAX_PATH );
    lstrcpyW( buf + 2, tmpW );
    cookie = pAddDllDirectory( buf );
    ok( !cookie, "AddDllDirectory succeeded\n" );
    ok( GetLastError() == ERROR_INVALID_PARAMETER, "wrong error %u\n", GetLastError() );
}

static void test_SetDefaultDllDirectories(void)
{
    HMODULE mod;
    BOOL ret;

    if (!pSetDefaultDllDirectories)
    {
        win_skip( "SetDefaultDllDirectories not available\n" );
        return;
    }

    mod = LoadLibraryA( "authz.dll" );
    ok( mod != NULL, "loading authz failed\n" );
    FreeLibrary( mod );
    ret = pSetDefaultDllDirectories( LOAD_LIBRARY_SEARCH_USER_DIRS );
    ok( ret, "SetDefaultDllDirectories failed err %u\n", GetLastError() );
    mod = LoadLibraryA( "authz.dll" );
    ok( !mod, "loading authz succeeded\n" );
    FreeLibrary( mod );
    ret = pSetDefaultDllDirectories( LOAD_LIBRARY_SEARCH_SYSTEM32 );
    ok( ret, "SetDefaultDllDirectories failed err %u\n", GetLastError() );
    mod = LoadLibraryA( "authz.dll" );
    ok( mod != NULL, "loading authz failed\n" );
    FreeLibrary( mod );
    mod = LoadLibraryExA( "authz.dll", 0, LOAD_LIBRARY_SEARCH_APPLICATION_DIR );
    ok( !mod, "loading authz succeeded\n" );
    FreeLibrary( mod );
    ret = pSetDefaultDllDirectories( LOAD_LIBRARY_SEARCH_APPLICATION_DIR );
    ok( ret, "SetDefaultDllDirectories failed err %u\n", GetLastError() );
    mod = LoadLibraryA( "authz.dll" );
    ok( !mod, "loading authz succeeded\n" );
    FreeLibrary( mod );
    ret = pSetDefaultDllDirectories( LOAD_LIBRARY_SEARCH_DEFAULT_DIRS );
    ok( ret, "SetDefaultDllDirectories failed err %u\n", GetLastError() );
    mod = LoadLibraryA( "authz.dll" );
    ok( mod != NULL, "loading authz failed\n" );
    FreeLibrary( mod );

    SetLastError( 0xdeadbeef );
    ret = pSetDefaultDllDirectories( 0 );
    ok( !ret, "SetDefaultDllDirectories succeeded\n" );
    ok( GetLastError() == ERROR_INVALID_PARAMETER, "wrong error %u\n", GetLastError() );

    SetLastError( 0xdeadbeef );
    ret = pSetDefaultDllDirectories( 3 );
    ok( !ret, "SetDefaultDllDirectories succeeded\n" );
    ok( GetLastError() == ERROR_INVALID_PARAMETER, "wrong error %u\n", GetLastError() );

    SetLastError( 0xdeadbeef );
    ret = pSetDefaultDllDirectories( LOAD_LIBRARY_SEARCH_APPLICATION_DIR | 0x8000 );
    ok( !ret, "SetDefaultDllDirectories succeeded\n" );
    ok( GetLastError() == ERROR_INVALID_PARAMETER, "wrong error %u\n", GetLastError() );

    SetLastError( 0xdeadbeef );
    ret = pSetDefaultDllDirectories( LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR );
    ok( !ret || broken(ret) /* win7 */, "SetDefaultDllDirectories succeeded\n" );
    if (!ret) ok( GetLastError() == ERROR_INVALID_PARAMETER, "wrong error %u\n", GetLastError() );

    SetLastError( 0xdeadbeef );
    ret = pSetDefaultDllDirectories( LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_USER_DIRS );
    ok( !ret || broken(ret) /* win7 */, "SetDefaultDllDirectories succeeded\n" );
    if (!ret) ok( GetLastError() == ERROR_INVALID_PARAMETER, "wrong error %u\n", GetLastError() );

    /* restore some sane defaults */
    pSetDefaultDllDirectories( LOAD_LIBRARY_SEARCH_DEFAULT_DIRS );
}

static void check_refcount( HMODULE mod, unsigned int refcount )
{
    unsigned int i;
    BOOL ret;

    for (i = 0; i < min( refcount, 10 ); ++i)
    {
        ret = FreeLibrary( mod );
        ok( ret || broken( refcount == ~0u && GetLastError() == ERROR_MOD_NOT_FOUND && i == 2 ) /* Win8 */,
            "Refcount test failed, i %u, error %u.\n", i, GetLastError() );
        if (!ret) return;
    }
    if (refcount != ~0u)
    {
        ret = FreeLibrary( mod );
        ok( !ret && GetLastError() == ERROR_MOD_NOT_FOUND, "Refcount test failed, ret %d, error %u.\n",
                ret, GetLastError() );
    }
}

static void test_LdrGetDllHandleEx(void)
{
    HMODULE mod, loaded_mod;
    UNICODE_STRING name;
    NTSTATUS status;
    unsigned int i;

    if (!pLdrGetDllHandleEx)
    {
        win_skip( "LdrGetDllHandleEx is not available.\n" );
        return;
    }

    RtlInitUnicodeString( &name, L"unknown.dll" );
    status = pLdrGetDllHandleEx( 0, NULL, NULL, &name, &mod );
    ok( status == STATUS_DLL_NOT_FOUND, "Got unexpected status %#x.\n", status );

    RtlInitUnicodeString( &name, L"authz.dll" );
    loaded_mod = LoadLibraryW( name.Buffer );
    ok( !!loaded_mod, "Failed to load module.\n" );
    status = pLdrGetDllHandleEx( 0, NULL, NULL, &name, &mod );
    ok( !status, "Got unexpected status %#x.\n", status );
    ok( mod == loaded_mod, "got %p\n", mod );
    winetest_push_context( "Flags 0" );
    check_refcount( loaded_mod, 2 );
    winetest_pop_context();

    loaded_mod = LoadLibraryW( name.Buffer );
    ok( !!loaded_mod, "Failed to load module.\n" );
    status = pLdrGetDllHandleEx( LDR_GET_DLL_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, NULL,
                                 NULL, &name, &mod );
    ok( !status, "Got unexpected status %#x.\n", status );
    ok( mod == loaded_mod, "got %p\n", mod );
    winetest_push_context( "LDR_GET_DLL_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT" );
    check_refcount( loaded_mod, 1 );
    winetest_pop_context();

    loaded_mod = LoadLibraryW( name.Buffer );
    ok( !!loaded_mod, "Failed to load module.\n" );
    status = pLdrGetDllHandle( NULL, ~0u, &name, &mod );
    ok( !status, "Got unexpected status %#x.\n", status );
    ok( mod == loaded_mod, "got %p\n", mod );
    winetest_push_context( "LdrGetDllHandle" );
    check_refcount( loaded_mod, 1 );
    winetest_pop_context();

    loaded_mod = LoadLibraryW( name.Buffer );
    ok( !!loaded_mod, "Failed to load module.\n" );
    status = pLdrGetDllHandleEx( 4, NULL, NULL, (void *)&name, &mod );
    ok( !status, "Got unexpected status %#x.\n", status );
    ok( mod == loaded_mod, "got %p\n", mod );
    winetest_push_context( "Flag 4" );
    check_refcount( loaded_mod, 2 );
    winetest_pop_context();

    for (i = 3; i < 32; ++i)
    {
        loaded_mod = LoadLibraryW( name.Buffer );
        ok( !!loaded_mod, "Failed to load module.\n" );
        status = pLdrGetDllHandleEx( 1 << i, NULL, NULL, &name, &mod );
        ok( status == STATUS_INVALID_PARAMETER, "Got unexpected status %#x.\n", status );
        winetest_push_context( "Invalid flags, i %u", i );
        check_refcount( loaded_mod, 1 );
        winetest_pop_context();
    }

    status = pLdrGetDllHandleEx( LDR_GET_DLL_HANDLE_EX_FLAG_PIN | LDR_GET_DLL_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                 NULL, NULL, &name, &mod );
    ok( status == STATUS_INVALID_PARAMETER, "Got unexpected status %#x.\n", status );

    loaded_mod = LoadLibraryW( name.Buffer );
    ok( !!loaded_mod, "Failed to load module.\n" );
    status = pLdrGetDllHandleEx( LDR_GET_DLL_HANDLE_EX_FLAG_PIN, NULL,
                                 NULL, &name, &mod );
    ok( !status, "Got unexpected status %#x.\n", status );
    ok( mod == loaded_mod, "got %p\n", mod );
    winetest_push_context( "LDR_GET_DLL_HANDLE_EX_FLAG_PIN" );
    check_refcount( loaded_mod, ~0u );
    winetest_pop_context();
}

static void test_LdrGetDllFullName(void)
{
    WCHAR expected_path[MAX_PATH], path_buffer[MAX_PATH];
    UNICODE_STRING path = {0, 0, path_buffer};
    WCHAR expected_terminator;
    NTSTATUS status;
    HMODULE ntdll;

    if (!pLdrGetDllFullName)
    {
        win_skip( "LdrGetDllFullName not available.\n" );
        return;
    }

    if (0) /* crashes on Windows */
        pLdrGetDllFullName( ntdll, NULL );

    ntdll = GetModuleHandleW( L"ntdll.dll" );

    memset( path_buffer, 0x23, sizeof(path_buffer) );

    status = pLdrGetDllFullName( ntdll, &path );
    ok( status == STATUS_BUFFER_TOO_SMALL, "Got unexpected status %#x.\n", status );
    ok( path.Length == 0, "Expected length 0, got %d.\n", path.Length );
    ok( path_buffer[0] == 0x2323, "Expected 0x2323, got 0x%x.\n", path_buffer[0] );

    GetSystemDirectoryW( expected_path, ARRAY_SIZE(expected_path) );
    path.MaximumLength = 5; /* odd numbers produce partially copied characters */

    status = pLdrGetDllFullName( ntdll, &path );
    ok( status == STATUS_BUFFER_TOO_SMALL, "Got unexpected status %#x.\n", status );
    ok( path.Length == path.MaximumLength, "Expected length %u, got %u.\n", path.MaximumLength, path.Length );
    expected_terminator = 0x2300 | (expected_path[path.MaximumLength / sizeof(WCHAR)] & 0xFF);
    ok( path_buffer[path.MaximumLength / sizeof(WCHAR)] == expected_terminator,
            "Expected 0x%x, got 0x%x.\n", expected_terminator, path_buffer[path.MaximumLength / 2] );
    path_buffer[path.MaximumLength / sizeof(WCHAR)] = 0;
    expected_path[path.MaximumLength / sizeof(WCHAR)] = 0;
    ok( lstrcmpW(path_buffer, expected_path) == 0, "Expected %s, got %s.\n",
            wine_dbgstr_w(expected_path), wine_dbgstr_w(path_buffer) );

    GetSystemDirectoryW( expected_path, ARRAY_SIZE(expected_path) );
    lstrcatW( expected_path, L"\\ntdll.dll" );
    path.MaximumLength = sizeof(path_buffer);

    status = pLdrGetDllFullName( ntdll, &path );
    ok( status == STATUS_SUCCESS, "Got unexpected status %#x.\n", status );
    ok( !lstrcmpiW(path_buffer, expected_path), "Expected %s, got %s\n",
            wine_dbgstr_w(expected_path), wine_dbgstr_w(path_buffer) );

    status = pLdrGetDllFullName( NULL, &path );
    ok( status == STATUS_SUCCESS, "Got unexpected status %#x.\n", status );
    GetModuleFileNameW( NULL, expected_path, ARRAY_SIZE(expected_path) );
    ok( !lstrcmpiW(path_buffer, expected_path), "Expected %s, got %s.\n",
            wine_dbgstr_w(expected_path), wine_dbgstr_w(path_buffer) );
}

static void test_ddag_node(void)
{
    static const struct
    {
        const WCHAR *dllname;
        BOOL optional;
    }
    expected_exe_dependencies[] =
    {
        { L"advapi32.dll" },
        { L"msvcrt.dll", TRUE },
        { L"user32.dll", TRUE },
    };
    LDR_DDAG_NODE *node, *dep_node, *prev_node;
    LDR_DATA_TABLE_ENTRY *mod, *mod2;
    SINGLE_LIST_ENTRY *se, *se2;
    LDR_DEPENDENCY *dep, *dep2;
    NTSTATUS status;
    unsigned int i;
    HMODULE hexe;

    hexe = GetModuleHandleW( NULL );
    ok( !!hexe, "Got NULL exe handle.\n" );

    status = LdrFindEntryForAddress( hexe, &mod );
    ok( !status, "Got unexpected status %#x.\n", status );

    if (!(node = mod->DdagNode))
    {
        win_skip( "DdagNode is NULL, skipping tests.\n" );
        return;
    }

    ok( !!node->Modules.Flink, "Got NULL module link.\n" );
    mod2 = CONTAINING_RECORD(node->Modules.Flink, LDR_DATA_TABLE_ENTRY, NodeModuleLink);
    ok( mod2 == mod || broken( (void **)mod2 == (void **)mod - 1 ), "Got unexpected mod2 %p, expected %p.\n",
            mod2, mod );
    if (mod2 != mod)
    {
        win_skip( "Old LDR_DATA_TABLE_ENTRY structure, skipping tests.\n" );
        return;
    }
    ok( node->Modules.Flink->Flink == &node->Modules, "Expected one element in list.\n" );

    ok( !node->IncomingDependencies.Tail, "Expected empty incoming dependencies list.\n" );

    /* node->Dependencies.Tail is NULL on Windows 10 1507-1607 32 bit test, maybe due to broken structure layout. */
    ok( !!node->Dependencies.Tail || broken( sizeof(void *) == 4 && !node->Dependencies.Tail ),
            "Expected nonempty dependencies list.\n" );
    if (!node->Dependencies.Tail)
    {
        win_skip( "Empty dependencies list.\n" );
        return;
    }
    todo_wine ok( node->LoadCount == -1, "Got unexpected LoadCount %d.\n", node->LoadCount );

    prev_node = NULL;
    se = node->Dependencies.Tail;
    for (i = 0; i < ARRAY_SIZE(expected_exe_dependencies); ++i)
    {
        winetest_push_context( "Dep %u (%s)", i, debugstr_w(expected_exe_dependencies[i].dllname) );

        se = se->Next;

        ok( !!se, "Got NULL list element.\n" );
        dep = CONTAINING_RECORD(se, LDR_DEPENDENCY, dependency_to_entry);
        ok( dep->dependency_from == node, "Got unexpected dependency_from %p.\n", dep->dependency_from );
        ok( !!dep->dependency_to, "Got null dependency_to.\n" );
        dep_node = dep->dependency_to;
        ok( !!dep_node, "Got NULL dep_node.\n" );

        if (dep_node == prev_node && expected_exe_dependencies[i].optional)
        {
            win_skip( "Module is not directly referenced.\n" );
            winetest_pop_context();
            prev_node = dep_node;
            continue;
        }

        mod2 = CONTAINING_RECORD(dep_node->Modules.Flink, LDR_DATA_TABLE_ENTRY, NodeModuleLink);
        ok( !lstrcmpW( mod2->BaseDllName.Buffer, expected_exe_dependencies[i].dllname ),
                "Got unexpected module %s.\n", debugstr_w(mod2->BaseDllName.Buffer));

        se2 = dep_node->IncomingDependencies.Tail;
        ok( !!se2, "Got empty incoming dependencies list.\n" );
        do
        {
            se2 = se2->Next;
            dep2 = CONTAINING_RECORD(se2, LDR_DEPENDENCY, dependency_from_entry);
        }
        while (dep2 != dep && se2 != dep_node->IncomingDependencies.Tail);
        ok( dep2 == dep, "Dependency not found in incoming deps list.\n" );

        todo_wine ok( dep_node->LoadCount > 0 || broken(!dep_node->LoadCount) /* Win8 */,
                "Got unexpected LoadCount %d.\n", dep_node->LoadCount );

        winetest_pop_context();
        prev_node = dep_node;
    }
    ok( se == node->Dependencies.Tail, "Expected end of the list.\n" );
}

START_TEST(module)
{
    WCHAR filenameW[MAX_PATH];

    /* Test if we can use GetModuleFileNameW */

    SetLastError(0xdeadbeef);
    GetModuleFileNameW(NULL, filenameW, MAX_PATH);
    if (GetLastError() == ERROR_CALL_NOT_IMPLEMENTED)
    {
        win_skip("GetModuleFileNameW not existing on this platform, skipping W-calls\n");
        is_unicode_enabled = FALSE;
    }

    init_pointers();

    testGetModuleFileName(NULL);
    testGetModuleFileName("kernel32.dll");
    testGetModuleFileName_Wrong();

    testGetDllDirectory();

    testLoadLibraryA();
    testNestedLoadLibraryA();
    testLoadLibraryA_Wrong();
    testGetProcAddress_Wrong();
    testLoadLibraryEx();
    test_LoadLibraryEx_search_flags();
    testGetModuleHandleEx();
    testK32GetModuleInformation();
    test_AddDllDirectory();
    test_SetDefaultDllDirectories();
    test_LdrGetDllHandleEx();
    test_LdrGetDllFullName();
    test_ddag_node();
}
