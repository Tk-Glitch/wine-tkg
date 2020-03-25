/*
 * File handling functions
 *
 * Copyright 1993 Erik Bos
 * Copyright 1996, 2004 Alexandre Julliard
 * Copyright 2003 Eric Pouech
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
 */

#include "config.h"
#include "wine/port.h"

#include <errno.h>
#include <stdio.h>
#include <stdarg.h>

#include "winerror.h"
#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winternl.h"
#include "winioctl.h"
#include "ntifs.h"

#include "kernel_private.h"
#include "wine/unicode.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(file);

#define MAX_PATHNAME_LEN        1024

/* check if a file name is for an executable file (.exe or .com) */
static inline BOOL is_executable( const WCHAR *name )
{
    static const WCHAR exeW[] = {'.','e','x','e',0};
    static const WCHAR comW[] = {'.','c','o','m',0};
    int len = strlenW(name);

    if (len < 4) return FALSE;
    return (!strcmpiW( name + len - 4, exeW ) || !strcmpiW( name + len - 4, comW ));
}

/***********************************************************************
 *           copy_filename_WtoA
 *
 * copy a file name back to OEM/Ansi, but only if the buffer is large enough
 */
static DWORD copy_filename_WtoA( LPCWSTR nameW, LPSTR buffer, DWORD len )
{
    UNICODE_STRING strW;
    DWORD ret;
    BOOL is_ansi = AreFileApisANSI();

    RtlInitUnicodeString( &strW, nameW );

    ret = is_ansi ? RtlUnicodeStringToAnsiSize(&strW) : RtlUnicodeStringToOemSize(&strW);
    if (buffer && ret <= len)
    {
        ANSI_STRING str;

        str.Buffer = buffer;
        str.MaximumLength = min( len, UNICODE_STRING_MAX_CHARS );
        if (is_ansi)
            RtlUnicodeStringToAnsiString( &str, &strW, FALSE );
        else
            RtlUnicodeStringToOemString( &str, &strW, FALSE );
        ret = str.Length;  /* length without terminating 0 */
    }
    return ret;
}

/***********************************************************************
 *           add_boot_rename_entry
 *
 * Adds an entry to the registry that is loaded when windows boots and
 * checks if there are some files to be removed or renamed/moved.
 * <fn1> has to be valid and <fn2> may be NULL. If both pointers are
 * non-NULL then the file is moved, otherwise it is deleted.  The
 * entry of the registry key is always appended with two zero
 * terminated strings. If <fn2> is NULL then the second entry is
 * simply a single 0-byte. Otherwise the second filename goes
 * there. The entries are prepended with \??\ before the path and the
 * second filename gets also a '!' as the first character if
 * MOVEFILE_REPLACE_EXISTING is set. After the final string another
 * 0-byte follows to indicate the end of the strings.
 * i.e.:
 * \??\D:\test\file1[0]
 * !\??\D:\test\file1_renamed[0]
 * \??\D:\Test|delete[0]
 * [0]                        <- file is to be deleted, second string empty
 * \??\D:\test\file2[0]
 * !\??\D:\test\file2_renamed[0]
 * [0]                        <- indicates end of strings
 *
 * or:
 * \??\D:\test\file1[0]
 * !\??\D:\test\file1_renamed[0]
 * \??\D:\Test|delete[0]
 * [0]                        <- file is to be deleted, second string empty
 * [0]                        <- indicates end of strings
 *
 */
static BOOL add_boot_rename_entry( LPCWSTR source, LPCWSTR dest, DWORD flags )
{
    static const WCHAR ValueName[] = {'P','e','n','d','i','n','g',
                                      'F','i','l','e','R','e','n','a','m','e',
                                      'O','p','e','r','a','t','i','o','n','s',0};
    static const WCHAR SessionW[] = {'\\','R','e','g','i','s','t','r','y','\\',
                                     'M','a','c','h','i','n','e','\\',
                                     'S','y','s','t','e','m','\\',
                                     'C','u','r','r','e','n','t','C','o','n','t','r','o','l','S','e','t','\\',
                                     'C','o','n','t','r','o','l','\\',
                                     'S','e','s','s','i','o','n',' ','M','a','n','a','g','e','r',0};
    static const int info_size = FIELD_OFFSET( KEY_VALUE_PARTIAL_INFORMATION, Data );

    OBJECT_ATTRIBUTES attr;
    UNICODE_STRING nameW, source_name, dest_name;
    KEY_VALUE_PARTIAL_INFORMATION *info;
    BOOL rc = FALSE;
    HANDLE Reboot = 0;
    DWORD len1, len2;
    DWORD DataSize = 0;
    BYTE *Buffer = NULL;
    WCHAR *p;

    if (!RtlDosPathNameToNtPathName_U( source, &source_name, NULL, NULL ))
    {
        SetLastError( ERROR_PATH_NOT_FOUND );
        return FALSE;
    }
    dest_name.Buffer = NULL;
    if (dest && !RtlDosPathNameToNtPathName_U( dest, &dest_name, NULL, NULL ))
    {
        RtlFreeUnicodeString( &source_name );
        SetLastError( ERROR_PATH_NOT_FOUND );
        return FALSE;
    }

    attr.Length = sizeof(attr);
    attr.RootDirectory = 0;
    attr.ObjectName = &nameW;
    attr.Attributes = 0;
    attr.SecurityDescriptor = NULL;
    attr.SecurityQualityOfService = NULL;
    RtlInitUnicodeString( &nameW, SessionW );

    if (NtCreateKey( &Reboot, KEY_ALL_ACCESS, &attr, 0, NULL, 0, NULL ) != STATUS_SUCCESS)
    {
        WARN("Error creating key for reboot management [%s]\n",
             "SYSTEM\\CurrentControlSet\\Control\\Session Manager");
        RtlFreeUnicodeString( &source_name );
        RtlFreeUnicodeString( &dest_name );
        return FALSE;
    }

    len1 = source_name.Length + sizeof(WCHAR);
    if (dest)
    {
        len2 = dest_name.Length + sizeof(WCHAR);
        if (flags & MOVEFILE_REPLACE_EXISTING)
            len2 += sizeof(WCHAR); /* Plus 1 because of the leading '!' */
    }
    else len2 = sizeof(WCHAR); /* minimum is the 0 characters for the empty second string */

    RtlInitUnicodeString( &nameW, ValueName );

    /* First we check if the key exists and if so how many bytes it already contains. */
    if (NtQueryValueKey( Reboot, &nameW, KeyValuePartialInformation,
                         NULL, 0, &DataSize ) == STATUS_BUFFER_TOO_SMALL)
    {
        if (!(Buffer = HeapAlloc( GetProcessHeap(), 0, DataSize + len1 + len2 + sizeof(WCHAR) )))
            goto Quit;
        if (NtQueryValueKey( Reboot, &nameW, KeyValuePartialInformation,
                             Buffer, DataSize, &DataSize )) goto Quit;
        info = (KEY_VALUE_PARTIAL_INFORMATION *)Buffer;
        if (info->Type != REG_MULTI_SZ) goto Quit;
        if (DataSize > sizeof(info)) DataSize -= sizeof(WCHAR);  /* remove terminating null (will be added back later) */
    }
    else
    {
        DataSize = info_size;
        if (!(Buffer = HeapAlloc( GetProcessHeap(), 0, DataSize + len1 + len2 + sizeof(WCHAR) )))
            goto Quit;
    }

    memcpy( Buffer + DataSize, source_name.Buffer, len1 );
    DataSize += len1;
    p = (WCHAR *)(Buffer + DataSize);
    if (dest)
    {
        if (flags & MOVEFILE_REPLACE_EXISTING)
            *p++ = '!';
        memcpy( p, dest_name.Buffer, len2 );
        DataSize += len2;
    }
    else
    {
        *p = 0;
        DataSize += sizeof(WCHAR);
    }

    /* add final null */
    p = (WCHAR *)(Buffer + DataSize);
    *p = 0;
    DataSize += sizeof(WCHAR);

    rc = !NtSetValueKey(Reboot, &nameW, 0, REG_MULTI_SZ, Buffer + info_size, DataSize - info_size);

 Quit:
    RtlFreeUnicodeString( &source_name );
    RtlFreeUnicodeString( &dest_name );
    if (Reboot) NtClose(Reboot);
    HeapFree( GetProcessHeap(), 0, Buffer );
    return(rc);
}



/***********************************************************************
 *           GetShortPathNameA   (KERNEL32.@)
 */
DWORD WINAPI GetShortPathNameA( LPCSTR longpath, LPSTR shortpath, DWORD shortlen )
{
    WCHAR *longpathW;
    WCHAR shortpathW[MAX_PATH];
    DWORD ret;

    TRACE("%s\n", debugstr_a(longpath));

    if (!(longpathW = FILE_name_AtoW( longpath, FALSE ))) return 0;

    ret = GetShortPathNameW(longpathW, shortpathW, MAX_PATH);

    if (!ret) return 0;
    if (ret > MAX_PATH)
    {
        SetLastError(ERROR_FILENAME_EXCED_RANGE);
        return 0;
    }
    return copy_filename_WtoA( shortpathW, shortpath, shortlen );
}


static BOOL is_same_file(HANDLE h1, HANDLE h2)
{
    int fd1;
    BOOL ret = FALSE;
    if (wine_server_handle_to_fd(h1, 0, &fd1, NULL) == STATUS_SUCCESS)
    {
        int fd2;
        if (wine_server_handle_to_fd(h2, 0, &fd2, NULL) == STATUS_SUCCESS)
        {
            struct stat stat1, stat2;
            if (fstat(fd1, &stat1) == 0 && fstat(fd2, &stat2) == 0)
                ret = (stat1.st_dev == stat2.st_dev && stat1.st_ino == stat2.st_ino);
            wine_server_release_fd(h2, fd2);
        }
        wine_server_release_fd(h1, fd1);
    }
    return ret;
}

/**************************************************************************
 *           CopyFileW   (KERNEL32.@)
 */
BOOL WINAPI CopyFileW( LPCWSTR source, LPCWSTR dest, BOOL fail_if_exists )
{
    return CopyFileExW( source, dest, NULL, NULL, NULL,
                        fail_if_exists ? COPY_FILE_FAIL_IF_EXISTS : 0 );
}


/**************************************************************************
 *           CopyFileA   (KERNEL32.@)
 */
BOOL WINAPI CopyFileA( LPCSTR source, LPCSTR dest, BOOL fail_if_exists)
{
    WCHAR *sourceW, *destW;
    BOOL ret;

    if (!(sourceW = FILE_name_AtoW( source, FALSE ))) return FALSE;
    if (!(destW = FILE_name_AtoW( dest, TRUE ))) return FALSE;

    ret = CopyFileW( sourceW, destW, fail_if_exists );

    HeapFree( GetProcessHeap(), 0, destW );
    return ret;
}


/**************************************************************************
 *           CopyFileExW   (KERNEL32.@)
 */
BOOL WINAPI CopyFileExW(LPCWSTR source, LPCWSTR dest,
                        LPPROGRESS_ROUTINE progress, LPVOID param,
                        LPBOOL cancel_ptr, DWORD flags)
{
    static const int buffer_size = 65536;
    HANDLE h1, h2;
    BY_HANDLE_FILE_INFORMATION info;
    DWORD count;
    BOOL ret = FALSE;
    char *buffer;
    LARGE_INTEGER size;
    LARGE_INTEGER transferred;
    DWORD cbret;

    if (!source || !dest)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (!(buffer = HeapAlloc( GetProcessHeap(), 0, buffer_size )))
    {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }

    TRACE("%s -> %s, %x\n", debugstr_w(source), debugstr_w(dest), flags);

    if (flags & COPY_FILE_RESTARTABLE)
        FIXME("COPY_FILE_RESTARTABLE is not supported\n");
    if (flags & COPY_FILE_COPY_SYMLINK)
        FIXME("COPY_FILE_COPY_SYMLINK is not supported\n");

    if ((h1 = CreateFileW(source, (flags & COPY_FILE_OPEN_SOURCE_FOR_WRITE) ?
                     GENERIC_WRITE | GENERIC_READ : GENERIC_READ,
                     FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                     NULL, OPEN_EXISTING, 0, 0)) == INVALID_HANDLE_VALUE)
    {
        WARN("Unable to open source %s\n", debugstr_w(source));
        HeapFree( GetProcessHeap(), 0, buffer );
        return FALSE;
    }

    if (!GetFileInformationByHandle( h1, &info ))
    {
        WARN("GetFileInformationByHandle returned error for %s\n", debugstr_w(source));
        HeapFree( GetProcessHeap(), 0, buffer );
        CloseHandle( h1 );
        return FALSE;
    }

    if (!(flags & COPY_FILE_FAIL_IF_EXISTS))
    {
        BOOL same_file = FALSE;
        h2 = CreateFileW( dest, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                         OPEN_EXISTING, 0, 0);
        if (h2 != INVALID_HANDLE_VALUE)
        {
            same_file = is_same_file( h1, h2 );
            CloseHandle( h2 );
        }
        if (same_file)
        {
            HeapFree( GetProcessHeap(), 0, buffer );
            CloseHandle( h1 );
            SetLastError( ERROR_SHARING_VIOLATION );
            return FALSE;
        }
    }

    if ((h2 = CreateFileW( dest, GENERIC_WRITE | DELETE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                           (flags & COPY_FILE_FAIL_IF_EXISTS) ? CREATE_NEW : CREATE_ALWAYS,
                           info.dwFileAttributes, h1 )) == INVALID_HANDLE_VALUE &&
        /* retry without DELETE if we got a sharing violation */
        (h2 = CreateFileW( dest, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                           (flags & COPY_FILE_FAIL_IF_EXISTS) ? CREATE_NEW : CREATE_ALWAYS,
                           info.dwFileAttributes, h1 )) == INVALID_HANDLE_VALUE)
    {
        WARN("Unable to open dest %s\n", debugstr_w(dest));
        HeapFree( GetProcessHeap(), 0, buffer );
        CloseHandle( h1 );
        return FALSE;
    }

    size.u.LowPart = info.nFileSizeLow;
    size.u.HighPart = info.nFileSizeHigh;
    transferred.QuadPart = 0;

    if (progress)
    {
        cbret = progress( size, transferred, size, transferred, 1,
                          CALLBACK_STREAM_SWITCH, h1, h2, param );
        if (cbret == PROGRESS_QUIET)
            progress = NULL;
        else if (cbret == PROGRESS_STOP)
        {
            SetLastError( ERROR_REQUEST_ABORTED );
            goto done;
        }
        else if (cbret == PROGRESS_CANCEL)
        {
            BOOLEAN disp = TRUE;
            SetFileInformationByHandle( h2, FileDispositionInfo, &disp, sizeof(disp) );
            SetLastError( ERROR_REQUEST_ABORTED );
            goto done;
        }
    }

    while (ReadFile( h1, buffer, buffer_size, &count, NULL ) && count)
    {
        char *p = buffer;
        while (count != 0)
        {
            DWORD res;
            if (!WriteFile( h2, p, count, &res, NULL ) || !res) goto done;
            p += res;
            count -= res;

            if (progress)
            {
                transferred.QuadPart += res;
                cbret = progress( size, transferred, size, transferred, 1,
                                  CALLBACK_CHUNK_FINISHED, h1, h2, param );
                if (cbret == PROGRESS_QUIET)
                    progress = NULL;
                else if (cbret == PROGRESS_STOP)
                {
                    SetLastError( ERROR_REQUEST_ABORTED );
                    goto done;
                }
                else if (cbret == PROGRESS_CANCEL)
                {
                    BOOLEAN disp = TRUE;
                    SetFileInformationByHandle( h2, FileDispositionInfo, &disp, sizeof(disp) );
                    SetLastError( ERROR_REQUEST_ABORTED );
                    goto done;
                }
            }
        }
    }
    ret =  TRUE;
done:
    /* Maintain the timestamp of source file to destination file */
    SetFileTime(h2, NULL, NULL, &info.ftLastWriteTime);
    HeapFree( GetProcessHeap(), 0, buffer );
    CloseHandle( h1 );
    CloseHandle( h2 );
    return ret;
}


/**************************************************************************
 *           CopyFileExA   (KERNEL32.@)
 */
BOOL WINAPI CopyFileExA(LPCSTR sourceFilename, LPCSTR destFilename,
                        LPPROGRESS_ROUTINE progressRoutine, LPVOID appData,
                        LPBOOL cancelFlagPointer, DWORD copyFlags)
{
    WCHAR *sourceW, *destW;
    BOOL ret;

    /* can't use the TEB buffer since we may have a callback routine */
    if (!(sourceW = FILE_name_AtoW( sourceFilename, TRUE ))) return FALSE;
    if (!(destW = FILE_name_AtoW( destFilename, TRUE )))
    {
        HeapFree( GetProcessHeap(), 0, sourceW );
        return FALSE;
    }
    ret = CopyFileExW(sourceW, destW, progressRoutine, appData,
                      cancelFlagPointer, copyFlags);
    HeapFree( GetProcessHeap(), 0, sourceW );
    HeapFree( GetProcessHeap(), 0, destW );
    return ret;
}

/**************************************************************************
 *           MoveFileTransactedA   (KERNEL32.@)
 */
BOOL WINAPI MoveFileTransactedA(const char *source, const char *dest, LPPROGRESS_ROUTINE progress, void *data, DWORD flags, HANDLE handle)
{
    FIXME("(%s, %s, %p, %p, %d, %p)\n", debugstr_a(source), debugstr_a(dest), progress, data, flags, handle);
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

/**************************************************************************
 *           MoveFileTransactedW   (KERNEL32.@)
 */
BOOL WINAPI MoveFileTransactedW(const WCHAR *source, const WCHAR *dest, LPPROGRESS_ROUTINE progress, void *data, DWORD flags, HANDLE handle)
{
    FIXME("(%s, %s, %p, %p, %d, %p)\n", debugstr_w(source), debugstr_w(dest), progress, data, flags, handle);
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

/**************************************************************************
 *           MoveFileWithProgressW   (KERNEL32.@)
 */
BOOL WINAPI MoveFileWithProgressW( LPCWSTR source, LPCWSTR dest,
                                   LPPROGRESS_ROUTINE fnProgress,
                                   LPVOID param, DWORD flag )
{
    FILE_RENAME_INFORMATION *rename_info;
    FILE_BASIC_INFORMATION info;
    UNICODE_STRING nt_name;
    OBJECT_ATTRIBUTES attr;
    IO_STATUS_BLOCK io;
    NTSTATUS status;
    HANDLE source_handle = 0;
    ULONG size;

    TRACE("(%s,%s,%p,%p,%04x)\n",
          debugstr_w(source), debugstr_w(dest), fnProgress, param, flag );

    if (flag & MOVEFILE_DELAY_UNTIL_REBOOT)
        return add_boot_rename_entry( source, dest, flag );

    if (!dest)
        return DeleteFileW( source );

    /* check if we are allowed to rename the source */

    if (!RtlDosPathNameToNtPathName_U( source, &nt_name, NULL, NULL ))
    {
        SetLastError( ERROR_PATH_NOT_FOUND );
        return FALSE;
    }
    attr.Length = sizeof(attr);
    attr.RootDirectory = 0;
    attr.Attributes = OBJ_CASE_INSENSITIVE;
    attr.ObjectName = &nt_name;
    attr.SecurityDescriptor = NULL;
    attr.SecurityQualityOfService = NULL;

    status = NtOpenFile( &source_handle, DELETE | SYNCHRONIZE, &attr, &io,
                         FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, FILE_SYNCHRONOUS_IO_NONALERT );
    RtlFreeUnicodeString( &nt_name );
    if (status != STATUS_SUCCESS)
    {
        SetLastError( RtlNtStatusToDosError(status) );
        goto error;
    }
    status = NtQueryInformationFile( source_handle, &io, &info, sizeof(info), FileBasicInformation );
    if (status != STATUS_SUCCESS)
    {
        SetLastError( RtlNtStatusToDosError(status) );
        goto error;
    }

    if (!RtlDosPathNameToNtPathName_U( dest, &nt_name, NULL, NULL ))
    {
        SetLastError( ERROR_PATH_NOT_FOUND );
        goto error;
    }

    size = offsetof( FILE_RENAME_INFORMATION, FileName ) + nt_name.Length;
    if (!(rename_info = HeapAlloc( GetProcessHeap(), 0, size )))
        goto error;

    rename_info->ReplaceIfExists = !!(flag & MOVEFILE_REPLACE_EXISTING);
    rename_info->RootDirectory = NULL;
    rename_info->FileNameLength = nt_name.Length;
    memcpy( rename_info->FileName, nt_name.Buffer, nt_name.Length );
    RtlFreeUnicodeString( &nt_name );
    status = NtSetInformationFile( source_handle, &io, rename_info, size, FileRenameInformation );
    if (status == STATUS_NOT_SAME_DEVICE && (flag & MOVEFILE_COPY_ALLOWED))
    {
        NtClose( source_handle );
        if (!CopyFileExW( source, dest, fnProgress, param, NULL,
                          flag & MOVEFILE_REPLACE_EXISTING ?
                          0 : COPY_FILE_FAIL_IF_EXISTS ))
            return FALSE;
        return DeleteFileW( source );
    }

    NtClose( source_handle );
    return set_ntstatus( status );

error:
    if (source_handle) NtClose( source_handle );
    return FALSE;
}

/**************************************************************************
 *           MoveFileWithProgressA   (KERNEL32.@)
 */
BOOL WINAPI MoveFileWithProgressA( LPCSTR source, LPCSTR dest,
                                   LPPROGRESS_ROUTINE fnProgress,
                                   LPVOID param, DWORD flag )
{
    WCHAR *sourceW, *destW;
    BOOL ret;

    if (!(sourceW = FILE_name_AtoW( source, FALSE ))) return FALSE;
    if (dest)
    {
        if (!(destW = FILE_name_AtoW( dest, TRUE ))) return FALSE;
    }
    else
        destW = NULL;

    ret = MoveFileWithProgressW( sourceW, destW, fnProgress, param, flag );
    HeapFree( GetProcessHeap(), 0, destW );
    return ret;
}

/**************************************************************************
 *           MoveFileExW   (KERNEL32.@)
 */
BOOL WINAPI MoveFileExW( LPCWSTR source, LPCWSTR dest, DWORD flag )
{
    return MoveFileWithProgressW( source, dest, NULL, NULL, flag );
}

/**************************************************************************
 *           MoveFileExA   (KERNEL32.@)
 */
BOOL WINAPI MoveFileExA( LPCSTR source, LPCSTR dest, DWORD flag )
{
    return MoveFileWithProgressA( source, dest, NULL, NULL, flag );
}


/**************************************************************************
 *           MoveFileW   (KERNEL32.@)
 *
 *  Move file or directory
 */
BOOL WINAPI MoveFileW( LPCWSTR source, LPCWSTR dest )
{
    return MoveFileExW( source, dest, MOVEFILE_COPY_ALLOWED );
}


/**************************************************************************
 *           MoveFileA   (KERNEL32.@)
 */
BOOL WINAPI MoveFileA( LPCSTR source, LPCSTR dest )
{
    return MoveFileExA( source, dest, MOVEFILE_COPY_ALLOWED );
}


/*************************************************************************
 *           CreateHardLinkW   (KERNEL32.@)
 */
BOOL WINAPI CreateHardLinkW(LPCWSTR lpFileName, LPCWSTR lpExistingFileName,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
    UNICODE_STRING ntDest, ntSource;
    FILE_LINK_INFORMATION *info = NULL;
    OBJECT_ATTRIBUTES attr;
    IO_STATUS_BLOCK io;
    BOOL ret = FALSE;
    HANDLE file;
    ULONG size;

    TRACE("(%s, %s, %p)\n", debugstr_w(lpFileName),
        debugstr_w(lpExistingFileName), lpSecurityAttributes);

    ntDest.Buffer = ntSource.Buffer = NULL;
    if (!RtlDosPathNameToNtPathName_U( lpFileName, &ntDest, NULL, NULL ) ||
        !RtlDosPathNameToNtPathName_U( lpExistingFileName, &ntSource, NULL, NULL ))
    {
        SetLastError( ERROR_PATH_NOT_FOUND );
        goto err;
    }

    size = offsetof( FILE_LINK_INFORMATION, FileName ) + ntDest.Length;
    if (!(info = HeapAlloc( GetProcessHeap(), 0, size )))
    {
        SetLastError( ERROR_OUTOFMEMORY );
        goto err;
    }

    InitializeObjectAttributes( &attr, &ntSource, OBJ_CASE_INSENSITIVE, 0, NULL );
    if (!(ret = set_ntstatus( NtOpenFile( &file, SYNCHRONIZE, &attr, &io, FILE_SHARE_READ | FILE_SHARE_WRITE,
            FILE_SYNCHRONOUS_IO_NONALERT ) )))
        goto err;

    info->ReplaceIfExists = FALSE;
    info->RootDirectory = NULL;
    info->FileNameLength = ntDest.Length;
    memcpy( info->FileName, ntDest.Buffer, ntDest.Length );
    ret = set_ntstatus( NtSetInformationFile( file, &io, info, size, FileLinkInformation ) );

    NtClose( file );

err:
    RtlFreeUnicodeString( &ntSource );
    RtlFreeUnicodeString( &ntDest );
    HeapFree( GetProcessHeap(), 0, info );
    return ret;
}


/*************************************************************************
 *           CreateHardLinkA   (KERNEL32.@)
 */
BOOL WINAPI CreateHardLinkA(LPCSTR lpFileName, LPCSTR lpExistingFileName,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes)
{
    WCHAR *sourceW, *destW;
    BOOL res;

    if (!(sourceW = FILE_name_AtoW( lpExistingFileName, TRUE )))
    {
        return FALSE;
    }
    if (!(destW = FILE_name_AtoW( lpFileName, TRUE )))
    {
        HeapFree( GetProcessHeap(), 0, sourceW );
        return FALSE;
    }

    res = CreateHardLinkW( destW, sourceW, lpSecurityAttributes );

    HeapFree( GetProcessHeap(), 0, sourceW );
    HeapFree( GetProcessHeap(), 0, destW );

    return res;
}


/***********************************************************************
 *           CreateDirectoryExA   (KERNEL32.@)
 */
BOOL WINAPI CreateDirectoryExA( LPCSTR template, LPCSTR path, LPSECURITY_ATTRIBUTES sa )
{
    WCHAR *pathW, *templateW = NULL;
    BOOL ret;

    if (!(pathW = FILE_name_AtoW( path, FALSE ))) return FALSE;
    if (template && !(templateW = FILE_name_AtoW( template, TRUE ))) return FALSE;

    ret = CreateDirectoryExW( templateW, pathW, sa );
    HeapFree( GetProcessHeap(), 0, templateW );
    return ret;
}


/***********************************************************************
 *           RemoveDirectoryW   (KERNEL32.@)
 */
BOOL WINAPI RemoveDirectoryW( LPCWSTR path )
{
    FILE_BASIC_INFORMATION info;
    OBJECT_ATTRIBUTES attr;
    UNICODE_STRING nt_name;
    ANSI_STRING unix_name;
    IO_STATUS_BLOCK io;
    NTSTATUS status;
    HANDLE handle;
    BOOL ret = FALSE;

    TRACE( "%s\n", debugstr_w(path) );

    if (!RtlDosPathNameToNtPathName_U( path, &nt_name, NULL, NULL ))
    {
        SetLastError( ERROR_PATH_NOT_FOUND );
        return FALSE;
    }
    attr.Length = sizeof(attr);
    attr.RootDirectory = 0;
    attr.Attributes = OBJ_CASE_INSENSITIVE;
    attr.ObjectName = &nt_name;
    attr.SecurityDescriptor = NULL;
    attr.SecurityQualityOfService = NULL;

    if (!set_ntstatus( NtOpenFile( &handle, DELETE | SYNCHRONIZE, &attr, &io,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                   FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT )))
    {
        RtlFreeUnicodeString( &nt_name );
        return FALSE;
    }

    status = wine_nt_to_unix_file_name( &nt_name, &unix_name, FILE_OPEN, FALSE );
    if (status == STATUS_SUCCESS)
    {
        status = NtQueryAttributesFile( &attr, &info );
        if (status == STATUS_SUCCESS && (info.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) &&
                                        (info.FileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            ret = (unlink( unix_name.Buffer ) != -1);
        else
            ret = (rmdir( unix_name.Buffer ) != -1);
        if (status == STATUS_SUCCESS && (info.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) &&
                                        !(info.FileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            SetLastError( ERROR_DIRECTORY );
        else if (!ret) FILE_SetDosError();
        RtlFreeAnsiString( &unix_name );
    }
    else
        set_ntstatus( status );
    RtlFreeUnicodeString( &nt_name );

    NtClose( handle );
    return ret;
}


/***********************************************************************
 *           RemoveDirectoryA   (KERNEL32.@)
 */
BOOL WINAPI RemoveDirectoryA( LPCSTR path )
{
    WCHAR *pathW;

    if (!(pathW = FILE_name_AtoW( path, FALSE ))) return FALSE;
    return RemoveDirectoryW( pathW );
}


/***********************************************************************
 *           GetSystemDirectoryW   (KERNEL32.@)
 *
 * See comment for GetWindowsDirectoryA.
 */
UINT WINAPI GetSystemDirectoryW( LPWSTR path, UINT count )
{
    UINT len = strlenW( DIR_System ) + 1;
    if (path && count >= len)
    {
        strcpyW( path, DIR_System );
        len--;
    }
    return len;
}


/***********************************************************************
 *           GetSystemDirectoryA   (KERNEL32.@)
 *
 * See comment for GetWindowsDirectoryA.
 */
UINT WINAPI GetSystemDirectoryA( LPSTR path, UINT count )
{
    return copy_filename_WtoA( DIR_System, path, count );
}


/***********************************************************************
 *           Wow64EnableWow64FsRedirection   (KERNEL32.@)
 *
 * Microsoft C++ Redistributable installers are depending on all %eax bits being set.
 */
DWORD /*BOOLEAN*/ WINAPI KERNEL32_Wow64EnableWow64FsRedirection( BOOLEAN enable )
{
    return set_ntstatus( RtlWow64EnableFsRedirection( enable ));
}


/***********************************************************************
 *           wine_get_unix_file_name (KERNEL32.@) Not a Windows API
 *
 * Return the full Unix file name for a given path.
 * Returned buffer must be freed by caller.
 */
char * CDECL wine_get_unix_file_name( LPCWSTR dosW )
{
    UNICODE_STRING nt_name;
    ANSI_STRING unix_name;
    NTSTATUS status;

    if (!RtlDosPathNameToNtPathName_U( dosW, &nt_name, NULL, NULL )) return NULL;
    status = wine_nt_to_unix_file_name( &nt_name, &unix_name, FILE_OPEN_IF, FALSE );
    RtlFreeUnicodeString( &nt_name );
    if (status && status != STATUS_NO_SUCH_FILE)
    {
        SetLastError( RtlNtStatusToDosError( status ) );
        return NULL;
    }
    return unix_name.Buffer;
}


/***********************************************************************
 *           wine_get_dos_file_name (KERNEL32.@) Not a Windows API
 *
 * Return the full DOS file name for a given Unix path.
 * Returned buffer must be freed by caller.
 */
WCHAR * CDECL wine_get_dos_file_name( LPCSTR str )
{
    UNICODE_STRING nt_name;
    ANSI_STRING unix_name;
    DWORD len;

    RtlInitAnsiString( &unix_name, str );
    if (!set_ntstatus( wine_unix_to_nt_file_name( &unix_name, &nt_name ))) return NULL;
    if (nt_name.Buffer[5] == ':')
    {
        /* get rid of the \??\ prefix */
        /* FIXME: should implement RtlNtPathNameToDosPathName and use that instead */
        len = nt_name.Length - 4 * sizeof(WCHAR);
        memmove( nt_name.Buffer, nt_name.Buffer + 4, len );
        nt_name.Buffer[len / sizeof(WCHAR)] = 0;
    }
    else
        nt_name.Buffer[1] = '\\';
    return nt_name.Buffer;
}

/*************************************************************************
 *           CreateSymbolicLinkW   (KERNEL32.@)
 */
BOOLEAN WINAPI CreateSymbolicLinkW(LPCWSTR link, LPCWSTR target, DWORD flags)
{
    static INT struct_size = offsetof(REPARSE_DATA_BUFFER, SymbolicLinkReparseBuffer.PathBuffer[0]);
    static INT header_size = offsetof(REPARSE_DATA_BUFFER, GenericReparseBuffer);
    INT buffer_size, data_size, string_len, prefix_len;
    WCHAR *subst_dest, *print_dest, *string;
    REPARSE_DATA_BUFFER *buffer;
    LPWSTR target_path = NULL;
    BOOL is_relative, is_dir;
    int target_path_len = 0;
    UNICODE_STRING nt_name;
    BOOLEAN bret = FALSE;
    NTSTATUS status;
    HANDLE hlink;
    DWORD dwret;

    TRACE("(%s %s %d)\n", debugstr_w(link), debugstr_w(target), flags);

    is_relative = (RtlDetermineDosPathNameType_U( target ) == RELATIVE_PATH);
    is_dir = (flags & SYMBOLIC_LINK_FLAG_DIRECTORY);
    if (is_dir && !CreateDirectoryW( link, NULL ))
        return FALSE;
    hlink = CreateFileW( link, GENERIC_READ | GENERIC_WRITE, 0, 0,
                         is_dir ? OPEN_EXISTING : CREATE_NEW,
                         FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, 0 );
    if (hlink == INVALID_HANDLE_VALUE)
        goto cleanup;
    if (is_relative)
    {
        UNICODE_STRING nt_path;
        int len;

        status = RtlDosPathNameToNtPathName_U_WithStatus( link, &nt_path, NULL, NULL );
        if (status != STATUS_SUCCESS)
        {
            SetLastError( RtlNtStatusToDosError(status) );
            goto cleanup;
        }
        /* obtain the path of the link */
        for (; nt_path.Length > 0; nt_path.Length -= sizeof(WCHAR))
        {
            WCHAR c = nt_path.Buffer[nt_path.Length/sizeof(WCHAR)];
            if (c == '/' || c == '\\')
            {
                nt_path.Length += sizeof(WCHAR);
                break;
            }
        }
        /* append the target to the link path */
        target_path_len = nt_path.Length / sizeof(WCHAR);
        len = target_path_len + (strlenW( target ) + 1);
        target_path = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, len*sizeof(WCHAR) );
        lstrcpynW( target_path, nt_path.Buffer, target_path_len+1 );
        target_path[target_path_len+1] = 0;
        lstrcatW( target_path, target );
        RtlFreeUnicodeString( &nt_path );
    }
    else
        target_path = (LPWSTR)target;
    status = RtlDosPathNameToNtPathName_U_WithStatus( target_path, &nt_name, NULL, NULL );
    if (status != STATUS_SUCCESS)
    {
        SetLastError( RtlNtStatusToDosError(status) );
        goto cleanup;
    }
    if (is_relative && strncmpW( target_path, nt_name.Buffer, target_path_len ) != 0)
    {
        SetLastError( RtlNtStatusToDosError(status) );
        goto cleanup;
    }
    prefix_len = is_relative ? 0 : strlen("\\??\\");
    string = &nt_name.Buffer[target_path_len];
    string_len = lstrlenW( &string[prefix_len] );
    data_size = (prefix_len + 2 * string_len + 2) * sizeof(WCHAR);
    buffer_size = struct_size + data_size;
    buffer = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, buffer_size );
    buffer->ReparseTag = IO_REPARSE_TAG_SYMLINK;
    buffer->ReparseDataLength = struct_size - header_size + data_size;
    buffer->SymbolicLinkReparseBuffer.SubstituteNameLength = (prefix_len + string_len) * sizeof(WCHAR);
    buffer->SymbolicLinkReparseBuffer.PrintNameOffset = (prefix_len + string_len + 1) * sizeof(WCHAR);
    buffer->SymbolicLinkReparseBuffer.PrintNameLength = string_len * sizeof(WCHAR);
    buffer->SymbolicLinkReparseBuffer.Flags = is_relative ? SYMLINK_FLAG_RELATIVE : 0;
    subst_dest = &buffer->SymbolicLinkReparseBuffer.PathBuffer[0];
    print_dest = &buffer->SymbolicLinkReparseBuffer.PathBuffer[prefix_len + string_len + 1];
    lstrcpyW( subst_dest, string );
    lstrcpyW( print_dest, &string[prefix_len] );
    RtlFreeUnicodeString( &nt_name );
    bret = DeviceIoControl( hlink, FSCTL_SET_REPARSE_POINT, (LPVOID)buffer, buffer_size, NULL, 0,
                            &dwret, 0 );
    HeapFree( GetProcessHeap(), 0, buffer );

cleanup:
    CloseHandle( hlink );
    if (!bret)
    {
        if (is_dir)
            RemoveDirectoryW( link );
        else
            DeleteFileW( link );
    }
    if (is_relative) HeapFree( GetProcessHeap(), 0, target_path );
    return bret;
}

/*************************************************************************
 *           CreateSymbolicLinkA   (KERNEL32.@)
 */
BOOLEAN WINAPI CreateSymbolicLinkA(LPCSTR link, LPCSTR target, DWORD flags)
{
    WCHAR *targetW, *linkW;
    BOOL ret;

    TRACE("(%s %s %d)\n", debugstr_a(link), debugstr_a(target), flags);

    if (!(linkW = FILE_name_AtoW( link, TRUE )))
    {
        return FALSE;
    }
    if (!(targetW = FILE_name_AtoW( target, TRUE )))
    {
        HeapFree( GetProcessHeap(), 0, linkW );
        return FALSE;
    }
    ret = CreateSymbolicLinkW( linkW, targetW, flags );
    HeapFree( GetProcessHeap(), 0, linkW );
    HeapFree( GetProcessHeap(), 0, targetW );
    return ret;
}

/*************************************************************************
 *           CreateHardLinkTransactedA   (KERNEL32.@)
 */
BOOL WINAPI CreateHardLinkTransactedA(LPCSTR link, LPCSTR target, LPSECURITY_ATTRIBUTES sa, HANDLE transaction)
{
    FIXME("(%s %s %p %p): stub\n", debugstr_a(link), debugstr_a(target), sa, transaction);
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

/*************************************************************************
 *           CreateHardLinkTransactedW   (KERNEL32.@)
 */
BOOL WINAPI CreateHardLinkTransactedW(LPCWSTR link, LPCWSTR target, LPSECURITY_ATTRIBUTES sa, HANDLE transaction)
{
    FIXME("(%s %s %p %p): stub\n", debugstr_w(link), debugstr_w(target), sa, transaction);
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

/*************************************************************************
 *           CheckNameLegalDOS8Dot3A   (KERNEL32.@)
 */
BOOL WINAPI CheckNameLegalDOS8Dot3A(const char *name, char *oemname, DWORD oemname_len,
        BOOL *contains_spaces, BOOL *is_legal)
{
    WCHAR *nameW;

    TRACE("(%s %p %u %p %p)\n", name, oemname,
            oemname_len, contains_spaces, is_legal);

    if (!name || !is_legal)
        return FALSE;

    if (!(nameW = FILE_name_AtoW( name, FALSE ))) return FALSE;

    return CheckNameLegalDOS8Dot3W( nameW, oemname, oemname_len, contains_spaces, is_legal );
}

/*************************************************************************
 *           CheckNameLegalDOS8Dot3W   (KERNEL32.@)
 */
BOOL WINAPI CheckNameLegalDOS8Dot3W(const WCHAR *name, char *oemname, DWORD oemname_len,
        BOOL *contains_spaces_ret, BOOL *is_legal)
{
    OEM_STRING oem_str;
    UNICODE_STRING nameW;
    BOOLEAN contains_spaces;

    TRACE("(%s %p %u %p %p)\n", wine_dbgstr_w(name), oemname,
          oemname_len, contains_spaces_ret, is_legal);

    if (!name || !is_legal)
        return FALSE;

    RtlInitUnicodeString( &nameW, name );

    if (oemname) {
        oem_str.Length = oemname_len;
        oem_str.MaximumLength = oemname_len;
        oem_str.Buffer = oemname;
    }

    *is_legal = RtlIsNameLegalDOS8Dot3( &nameW, oemname ? &oem_str : NULL, &contains_spaces );
    if (contains_spaces_ret) *contains_spaces_ret = contains_spaces;

    return TRUE;
}

/*************************************************************************
 *           SetSearchPathMode   (KERNEL32.@)
 */
BOOL WINAPI SetSearchPathMode( DWORD flags )
{
    return set_ntstatus( RtlSetSearchPathMode( flags ));
}
