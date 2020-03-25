/*
 * Win32 virtual memory functions
 *
 * Copyright 1997 Alexandre Julliard
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

#include "config.h"
#include "wine/port.h"

#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include "ntstatus.h"
#define WIN32_NO_STATUS
#define NONAMELESSUNION
#include "windef.h"
#include "winbase.h"
#include "winnls.h"
#include "winternl.h"
#include "winerror.h"
#include "psapi.h"
#include "wine/exception.h"
#include "wine/debug.h"

#include "kernel_private.h"

WINE_DECLARE_DEBUG_CHANNEL(seh);
WINE_DECLARE_DEBUG_CHANNEL(file);


static LONG WINAPI badptr_handler( EXCEPTION_POINTERS *eptr )
{
    EXCEPTION_RECORD *rec = eptr->ExceptionRecord;

    if (rec->ExceptionCode == STATUS_ACCESS_VIOLATION) return EXCEPTION_EXECUTE_HANDLER;
    if (rec->ExceptionCode == STATUS_STACK_OVERFLOW)
    {
        /* restore stack guard page */
        void *addr = (char *)NtCurrentTeb()->DeallocationStack + system_info.PageSize;
        SIZE_T size = (char *)rec - (char *)addr;
        ULONG old_prot;
        NtProtectVirtualMemory( GetCurrentProcess(), &addr, &size, PAGE_GUARD|PAGE_READWRITE, &old_prot );
        return EXCEPTION_EXECUTE_HANDLER;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

/***********************************************************************
 *             IsBadReadPtr   (KERNEL32.@)
 *
 * Check for read access on a memory block.
 *
 * ptr  [I] Address of memory block.
 * size [I] Size of block.
 *
 * RETURNS
 *  Success: TRUE.
 *	Failure: FALSE. Process has read access to entire block.
 */
BOOL WINAPI IsBadReadPtr( LPCVOID ptr, UINT_PTR size )
{
    if (!size) return FALSE;  /* handle 0 size case w/o reference */
    if (!ptr) return TRUE;
    __TRY
    {
        volatile const char *p = ptr;
        char dummy __attribute__((unused));
        UINT_PTR count = size;

        while (count > system_info.PageSize)
        {
            dummy = *p;
            p += system_info.PageSize;
            count -= system_info.PageSize;
        }
        dummy = p[0];
        dummy = p[count - 1];
    }
    __EXCEPT( badptr_handler )
    {
        TRACE_(seh)("%p caused page fault during read\n", ptr);
        return TRUE;
    }
    __ENDTRY
    return FALSE;
}


/***********************************************************************
 *             IsBadWritePtr   (KERNEL32.@)
 *
 * Check for write access on a memory block.
 *
 * PARAMS
 *  ptr  [I] Address of memory block.
 *  size [I] Size of block in bytes.
 *
 * RETURNS
 *  Success: TRUE.
 *	Failure: FALSE. Process has write access to entire block.
 */
BOOL WINAPI IsBadWritePtr( LPVOID ptr, UINT_PTR size )
{
    if (!size) return FALSE;  /* handle 0 size case w/o reference */
    if (!ptr) return TRUE;
    __TRY
    {
        volatile char *p = ptr;
        UINT_PTR count = size;

        while (count > system_info.PageSize)
        {
            *p |= 0;
            p += system_info.PageSize;
            count -= system_info.PageSize;
        }
        p[0] |= 0;
        p[count - 1] |= 0;
    }
    __EXCEPT( badptr_handler )
    {
        TRACE_(seh)("%p caused page fault during write\n", ptr);
        return TRUE;
    }
    __ENDTRY
    return FALSE;
}


/***********************************************************************
 *             IsBadHugeReadPtr   (KERNEL32.@)
 *
 * Check for read access on a memory block.
 *
 * PARAMS
 *  ptr  [I] Address of memory block.
 *  size [I] Size of block.
 *
 * RETURNS
 *  Success: TRUE.
 *	Failure: FALSE. Process has read access to entire block.
 */
BOOL WINAPI IsBadHugeReadPtr( LPCVOID ptr, UINT_PTR size )
{
    return IsBadReadPtr( ptr, size );
}


/***********************************************************************
 *             IsBadHugeWritePtr   (KERNEL32.@)
 *
 * Check for write access on a memory block.
 *
 * PARAMS
 *  ptr  [I] Address of memory block.
 *  size [I] Size of block.
 *
 * RETURNS
 *  Success: TRUE.
 *	Failure: FALSE. Process has write access to entire block.
 */
BOOL WINAPI IsBadHugeWritePtr( LPVOID ptr, UINT_PTR size )
{
    return IsBadWritePtr( ptr, size );
}


/***********************************************************************
 *             IsBadCodePtr   (KERNEL32.@)
 *
 * Check for read access on a memory address.
 *
 * PARAMS
 *  ptr [I] Address of function.
 *
 * RETURNS
 *	Success: TRUE.
 *	Failure: FALSE. Process has read access to specified memory.
 */
BOOL WINAPI IsBadCodePtr( FARPROC ptr )
{
    return IsBadReadPtr( ptr, 1 );
}


/***********************************************************************
 *             IsBadStringPtrA   (KERNEL32.@)
 *
 * Check for read access on a range of memory pointed to by a string pointer.
 *
 * PARAMS
 *  str [I] Address of string.
 *  max [I] Maximum size of string.
 *
 * RETURNS
 *	Success: TRUE.
 *	Failure: FALSE. Read access to all bytes in string.
 */
BOOL WINAPI IsBadStringPtrA( LPCSTR str, UINT_PTR max )
{
    if (!str) return TRUE;

    __TRY
    {
        volatile const char *p = str;
        while (p != str + max) if (!*p++) break;
    }
    __EXCEPT( badptr_handler )
    {
        TRACE_(seh)("%p caused page fault during read\n", str);
        return TRUE;
    }
    __ENDTRY
    return FALSE;
}


/***********************************************************************
 *             IsBadStringPtrW   (KERNEL32.@)
 *
 * See IsBadStringPtrA.
 */
BOOL WINAPI IsBadStringPtrW( LPCWSTR str, UINT_PTR max )
{
    if (!str) return TRUE;

    __TRY
    {
        volatile const WCHAR *p = str;
        while (p != str + max) if (!*p++) break;
    }
    __EXCEPT( badptr_handler )
    {
        TRACE_(seh)("%p caused page fault during read\n", str);
        return TRUE;
    }
    __ENDTRY
    return FALSE;
}

/***********************************************************************
 *           K32GetMappedFileNameW (KERNEL32.@)
 */
DWORD WINAPI K32GetMappedFileNameW(HANDLE process, LPVOID addr, LPWSTR file_name, DWORD size)
{
    MEMORY_SECTION_NAME *name;
    SIZE_T buf_len;
    NTSTATUS status;

    TRACE_(file)("(%p, %p, %p, %d)\n", process, addr, file_name, size);

    if (!file_name || !size)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    buf_len = sizeof(*name) + size * sizeof(WCHAR);
    name = HeapAlloc(GetProcessHeap(), 0, buf_len);
    if (!name)
    {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return 0;
    }

    status = NtQueryVirtualMemory(process, addr, MemorySectionName, name, buf_len, &buf_len);
    if (status)
    {
        HeapFree(GetProcessHeap(), 0, name);
        SetLastError(RtlNtStatusToDosError(status));
        return 0;
    }

    memcpy(file_name, name->SectionFileName.Buffer, name->SectionFileName.MaximumLength);
    buf_len = name->SectionFileName.Length;

    HeapFree(GetProcessHeap(), 0, name);

    return buf_len;
}

/***********************************************************************
 *           K32GetMappedFileNameA (KERNEL32.@)
 */
DWORD WINAPI K32GetMappedFileNameA(HANDLE process, LPVOID addr, LPSTR file_name, DWORD size)
{
    WCHAR file_nameW[MAX_PATH];
    DWORD ret;

    TRACE_(file)("(%p, %p, %p, %d)\n", process, addr, file_name, size);

    if (!file_name || !size)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    ret = K32GetMappedFileNameW(process, addr, file_nameW, MAX_PATH);
    if (ret)
    {
        ret = FILE_name_WtoA(file_nameW, -1, file_name, size);
        if (ret > 1)
            ret--; /* don't account for terminating NUL */
        else
            file_name[0] = 0;
    }
    return ret;
}

/***********************************************************************
 *           K32EnumPageFilesA (KERNEL32.@)
 */
BOOL WINAPI K32EnumPageFilesA( PENUM_PAGE_FILE_CALLBACKA callback, LPVOID context )
{
    FIXME_(file)("(%p, %p) stub\n", callback, context );
    return FALSE;
}

/***********************************************************************
 *           K32EnumPageFilesW (KERNEL32.@)
 */
BOOL WINAPI K32EnumPageFilesW( PENUM_PAGE_FILE_CALLBACKW callback, LPVOID context )
{
    FIXME_(file)("(%p, %p) stub\n", callback, context );
    return FALSE;
}

/***********************************************************************
 *           K32GetWsChanges (KERNEL32.@)
 */
BOOL WINAPI K32GetWsChanges(HANDLE process, PPSAPI_WS_WATCH_INFORMATION watchinfo, DWORD size)
{
    NTSTATUS status;

    TRACE_(seh)("(%p, %p, %d)\n", process, watchinfo, size);

    status = NtQueryInformationProcess( process, ProcessWorkingSetWatch, watchinfo, size, NULL );

    if (status)
    {
        SetLastError( RtlNtStatusToDosError( status ) );
        return FALSE;
    }
    return TRUE;
}

/***********************************************************************
 *           K32GetWsChangesEx (KERNEL32.@)
 */
BOOL WINAPI K32GetWsChangesEx(HANDLE process, PSAPI_WS_WATCH_INFORMATION_EX *watchinfoex, DWORD *size)
{
    FIXME_(seh)("(%p, %p, %p)\n", process, watchinfoex, size);

    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}

/***********************************************************************
 *           K32InitializeProcessForWsWatch (KERNEL32.@)
 */
BOOL WINAPI K32InitializeProcessForWsWatch(HANDLE process)
{
    FIXME_(seh)("(process=%p): stub\n", process);

    return TRUE;
}
