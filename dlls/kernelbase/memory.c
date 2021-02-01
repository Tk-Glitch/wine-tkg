/*
 * Win32 memory management functions
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

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#define NONAMELESSUNION
#define NONAMELESSSTRUCT
#include "windef.h"
#include "winbase.h"
#include "winnls.h"
#include "winternl.h"
#include "winerror.h"
#include "ddk/wdm.h"

#include "kernelbase.h"
#include "wine/exception.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(heap);
WINE_DECLARE_DEBUG_CHANNEL(virtual);


/***********************************************************************
 * Virtual memory functions
 ***********************************************************************/


/***********************************************************************
 *             FlushViewOfFile   (kernelbase.@)
 */
BOOL WINAPI DECLSPEC_HOTPATCH FlushViewOfFile( const void *base, SIZE_T size )
{
    NTSTATUS status = NtFlushVirtualMemory( GetCurrentProcess(), &base, &size, 0 );

    if (status == STATUS_NOT_MAPPED_DATA) status = STATUS_SUCCESS;
    return set_ntstatus( status );
}


/***********************************************************************
 *          GetLargePageMinimum   (kernelbase.@)
 */
SIZE_T WINAPI GetLargePageMinimum(void)
{
    return 2 * 1024 * 1024;
}


/***********************************************************************
 *          GetNativeSystemInfo   (kernelbase.@)
 */
void WINAPI DECLSPEC_HOTPATCH GetNativeSystemInfo( SYSTEM_INFO *si )
{
    GetSystemInfo( si );
    if (!is_wow64) return;
    switch (si->u.s.wProcessorArchitecture)
    {
    case PROCESSOR_ARCHITECTURE_INTEL:
        si->u.s.wProcessorArchitecture = PROCESSOR_ARCHITECTURE_AMD64;
        si->dwProcessorType = PROCESSOR_AMD_X8664;
        break;
    default:
        FIXME( "Add the proper information for %d in wow64 mode\n", si->u.s.wProcessorArchitecture );
    }
}


/***********************************************************************
 *          GetSystemInfo   (kernelbase.@)
 */
void WINAPI DECLSPEC_HOTPATCH GetSystemInfo( SYSTEM_INFO *si )
{
    SYSTEM_BASIC_INFORMATION basic_info;
    SYSTEM_CPU_INFORMATION cpu_info;

    if (!set_ntstatus( NtQuerySystemInformation( SystemBasicInformation,
                                                 &basic_info, sizeof(basic_info), NULL )) ||
        !set_ntstatus( NtQuerySystemInformation( SystemCpuInformation,
                                                 &cpu_info, sizeof(cpu_info), NULL )))
        return;

    si->u.s.wProcessorArchitecture  = cpu_info.Architecture;
    si->u.s.wReserved               = 0;
    si->dwPageSize                  = basic_info.PageSize;
    si->lpMinimumApplicationAddress = basic_info.LowestUserAddress;
    si->lpMaximumApplicationAddress = basic_info.HighestUserAddress;
    si->dwActiveProcessorMask       = basic_info.ActiveProcessorsAffinityMask;
    si->dwNumberOfProcessors        = basic_info.NumberOfProcessors;
    si->dwAllocationGranularity     = basic_info.AllocationGranularity;
    si->wProcessorLevel             = cpu_info.Level;
    si->wProcessorRevision          = cpu_info.Revision;

    switch (cpu_info.Architecture)
    {
    case PROCESSOR_ARCHITECTURE_INTEL:
        switch (cpu_info.Level)
        {
        case 3:  si->dwProcessorType = PROCESSOR_INTEL_386;     break;
        case 4:  si->dwProcessorType = PROCESSOR_INTEL_486;     break;
        case 5:
        case 6:  si->dwProcessorType = PROCESSOR_INTEL_PENTIUM; break;
        default: si->dwProcessorType = PROCESSOR_INTEL_PENTIUM; break;
        }
        break;
    case PROCESSOR_ARCHITECTURE_PPC:
        switch (cpu_info.Level)
        {
        case 1:  si->dwProcessorType = PROCESSOR_PPC_601;       break;
        case 3:
        case 6:  si->dwProcessorType = PROCESSOR_PPC_603;       break;
        case 4:  si->dwProcessorType = PROCESSOR_PPC_604;       break;
        case 9:  si->dwProcessorType = PROCESSOR_PPC_604;       break;
        case 20: si->dwProcessorType = PROCESSOR_PPC_620;       break;
        default: si->dwProcessorType = 0;
        }
        break;
    case PROCESSOR_ARCHITECTURE_AMD64:
        si->dwProcessorType = PROCESSOR_AMD_X8664;
        break;
    case PROCESSOR_ARCHITECTURE_ARM:
        switch (cpu_info.Level)
        {
        case 4:  si->dwProcessorType = PROCESSOR_ARM_7TDMI;     break;
        default: si->dwProcessorType = PROCESSOR_ARM920;
        }
        break;
    case PROCESSOR_ARCHITECTURE_ARM64:
        si->dwProcessorType = 0;
        break;
    default:
        FIXME( "Unknown processor architecture %x\n", cpu_info.Architecture );
        si->dwProcessorType = 0;
        break;
    }
}


/***********************************************************************
 *          GetSystemFileCacheSize   (kernelbase.@)
 */
BOOL WINAPI DECLSPEC_HOTPATCH GetSystemFileCacheSize( SIZE_T *mincache, SIZE_T *maxcache, DWORD *flags )
{
    FIXME( "stub: %p %p %p\n", mincache, maxcache, flags );
    SetLastError( ERROR_CALL_NOT_IMPLEMENTED );
    return FALSE;
}


/***********************************************************************
 *             GetWriteWatch   (kernelbase.@)
 */
UINT WINAPI DECLSPEC_HOTPATCH GetWriteWatch( DWORD flags, void *base, SIZE_T size, void **addresses,
                                             ULONG_PTR *count, ULONG *granularity )
{
    if (!set_ntstatus( NtGetWriteWatch( GetCurrentProcess(), flags, base, size,
                                        addresses, count, granularity )))
        return ~0u;
    return 0;
}


/***********************************************************************
 *             MapViewOfFile   (kernelbase.@)
 */
LPVOID WINAPI DECLSPEC_HOTPATCH MapViewOfFile( HANDLE mapping, DWORD access, DWORD offset_high,
                                               DWORD offset_low, SIZE_T count )
{
    return MapViewOfFileEx( mapping, access, offset_high, offset_low, count, NULL );
}


/***********************************************************************
 *             MapViewOfFileEx   (kernelbase.@)
 */
LPVOID WINAPI DECLSPEC_HOTPATCH MapViewOfFileEx( HANDLE handle, DWORD access, DWORD offset_high,
                                                 DWORD offset_low, SIZE_T count, LPVOID addr )
{
    NTSTATUS status;
    LARGE_INTEGER offset;
    ULONG protect;
    BOOL exec;

    offset.u.LowPart  = offset_low;
    offset.u.HighPart = offset_high;

    exec = access & FILE_MAP_EXECUTE;
    access &= ~FILE_MAP_EXECUTE;

    if (access == FILE_MAP_COPY)
        protect = exec ? PAGE_EXECUTE_WRITECOPY : PAGE_WRITECOPY;
    else if (access & FILE_MAP_WRITE)
        protect = exec ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE;
    else if (access & FILE_MAP_READ)
        protect = exec ? PAGE_EXECUTE_READ : PAGE_READONLY;
    else protect = PAGE_NOACCESS;

    if ((status = NtMapViewOfSection( handle, GetCurrentProcess(), &addr, 0, 0, &offset,
                                      &count, ViewShare, 0, protect )) < 0)
    {
        SetLastError( RtlNtStatusToDosError(status) );
        addr = NULL;
    }
    return addr;
}


/***********************************************************************
 *	       ReadProcessMemory   (kernelbase.@)
 */
BOOL WINAPI DECLSPEC_HOTPATCH ReadProcessMemory( HANDLE process, const void *addr, void *buffer,
                                                 SIZE_T size, SIZE_T *bytes_read )
{
    return set_ntstatus( NtReadVirtualMemory( process, addr, buffer, size, bytes_read ));
}


/***********************************************************************
 *             ResetWriteWatch   (kernelbase.@)
 */
UINT WINAPI DECLSPEC_HOTPATCH ResetWriteWatch( void *base, SIZE_T size )
{
    if (!set_ntstatus( NtResetWriteWatch( GetCurrentProcess(), base, size )))
        return ~0u;
    return 0;
}


/***********************************************************************
 *          SetSystemFileCacheSize   (kernelbase.@)
 */
BOOL WINAPI DECLSPEC_HOTPATCH SetSystemFileCacheSize( SIZE_T mincache, SIZE_T maxcache, DWORD flags )
{
    FIXME( "stub: %ld %ld %d\n", mincache, maxcache, flags );
    SetLastError( ERROR_CALL_NOT_IMPLEMENTED );
    return FALSE;
}


/***********************************************************************
 *             UnmapViewOfFile   (kernelbase.@)
 */
BOOL WINAPI DECLSPEC_HOTPATCH UnmapViewOfFile( const void *addr )
{
    if (GetVersion() & 0x80000000)
    {
        MEMORY_BASIC_INFORMATION info;
        if (!VirtualQuery( addr, &info, sizeof(info) ) || info.AllocationBase != addr)
        {
            SetLastError( ERROR_INVALID_ADDRESS );
            return FALSE;
        }
    }
    return set_ntstatus( NtUnmapViewOfSection( GetCurrentProcess(), (void *)addr ));
}


/***********************************************************************
 *             VirtualAlloc   (kernelbase.@)
 */
LPVOID WINAPI DECLSPEC_HOTPATCH VirtualAlloc( void *addr, SIZE_T size, DWORD type, DWORD protect )
{
    return VirtualAllocEx( GetCurrentProcess(), addr, size, type, protect );
}


/***********************************************************************
 *             VirtualAllocEx   (kernelbase.@)
 */
LPVOID WINAPI DECLSPEC_HOTPATCH VirtualAllocEx( HANDLE process, void *addr, SIZE_T size,
                                                DWORD type, DWORD protect )
{
    LPVOID ret = addr;

    if (!set_ntstatus( NtAllocateVirtualMemory( process, &ret, 0, &size, type, protect ))) return NULL;
    return ret;
}


/***********************************************************************
 *             VirtualAlloc2   (kernelbase.@)
 */
LPVOID WINAPI DECLSPEC_HOTPATCH VirtualAlloc2( HANDLE process, void *addr, SIZE_T size,
                                               DWORD type, DWORD protect,
                                               MEM_EXTENDED_PARAMETER *parameters, ULONG count )
{
    LPVOID ret = addr;

    if (!set_ntstatus( NtAllocateVirtualMemoryEx( process, &ret, &size, type, protect, parameters, count )))
        return NULL;
    return ret;
}


/***********************************************************************
 *             VirtualFree   (kernelbase.@)
 */
BOOL WINAPI DECLSPEC_HOTPATCH VirtualFree( void *addr, SIZE_T size, DWORD type )
{
    return VirtualFreeEx( GetCurrentProcess(), addr, size, type );
}


/***********************************************************************
 *             VirtualFreeEx   (kernelbase.@)
 */
BOOL WINAPI DECLSPEC_HOTPATCH VirtualFreeEx( HANDLE process, void *addr, SIZE_T size, DWORD type )
{
    return set_ntstatus( NtFreeVirtualMemory( process, &addr, &size, type ));
}


/***********************************************************************
 *             VirtualLock   (kernelbase.@)
 */
BOOL WINAPI DECLSPEC_HOTPATCH  VirtualLock( void *addr, SIZE_T size )
{
    return set_ntstatus( NtLockVirtualMemory( GetCurrentProcess(), &addr, &size, 1 ));
}


/***********************************************************************
 *             VirtualProtect   (kernelbase.@)
 */
BOOL WINAPI DECLSPEC_HOTPATCH VirtualProtect( void *addr, SIZE_T size, DWORD new_prot, DWORD *old_prot )
{
    return VirtualProtectEx( GetCurrentProcess(), addr, size, new_prot, old_prot );
}


/***********************************************************************
 *             VirtualProtectEx   (kernelbase.@)
 */
BOOL WINAPI DECLSPEC_HOTPATCH VirtualProtectEx( HANDLE process, void *addr, SIZE_T size,
                                                DWORD new_prot, DWORD *old_prot )
{
    DWORD prot;

    /* Win9x allows passing NULL as old_prot while this fails on NT */
    if (!old_prot && (GetVersion() & 0x80000000)) old_prot = &prot;
    return set_ntstatus( NtProtectVirtualMemory( process, &addr, &size, new_prot, old_prot ));
}


/***********************************************************************
 *             VirtualQuery   (kernelbase.@)
 */
SIZE_T WINAPI DECLSPEC_HOTPATCH VirtualQuery( LPCVOID addr, PMEMORY_BASIC_INFORMATION info, SIZE_T len )
{
    return VirtualQueryEx( GetCurrentProcess(), addr, info, len );
}


/***********************************************************************
 *             VirtualQueryEx   (kernelbase.@)
 */
SIZE_T WINAPI DECLSPEC_HOTPATCH VirtualQueryEx( HANDLE process, LPCVOID addr,
                                                PMEMORY_BASIC_INFORMATION info, SIZE_T len )
{
    SIZE_T ret;

    if (!set_ntstatus( NtQueryVirtualMemory( process, addr, MemoryBasicInformation, info, len, &ret )))
        return 0;
    return ret;
}


/***********************************************************************
 *             VirtualUnlock   (kernelbase.@)
 */
BOOL WINAPI DECLSPEC_HOTPATCH VirtualUnlock( void *addr, SIZE_T size )
{
    return set_ntstatus( NtUnlockVirtualMemory( GetCurrentProcess(), &addr, &size, 1 ));
}


/***********************************************************************
 *             WriteProcessMemory    (kernelbase.@)
 */
BOOL WINAPI DECLSPEC_HOTPATCH WriteProcessMemory( HANDLE process, void *addr, const void *buffer,
                                                  SIZE_T size, SIZE_T *bytes_written )
{
    return set_ntstatus( NtWriteVirtualMemory( process, addr, buffer, size, bytes_written ));
}


/* IsBadStringPtrA replacement for kernelbase, to catch exception in debug traces. */
BOOL WINAPI IsBadStringPtrA( LPCSTR str, UINT_PTR max )
{
    if (!str) return TRUE;
    __TRY
    {
        volatile const char *p = str;
        while (p != str + max) if (!*p++) break;
    }
    __EXCEPT_PAGE_FAULT
    {
        return TRUE;
    }
    __ENDTRY
    return FALSE;
}


/* IsBadStringPtrW replacement for kernelbase, to catch exception in debug traces. */
BOOL WINAPI IsBadStringPtrW( LPCWSTR str, UINT_PTR max )
{
    if (!str) return TRUE;
    __TRY
    {
        volatile const WCHAR *p = str;
        while (p != str + max) if (!*p++) break;
    }
    __EXCEPT_PAGE_FAULT
    {
        return TRUE;
    }
    __ENDTRY
    return FALSE;
}


/***********************************************************************
 * Heap functions
 ***********************************************************************/


/***********************************************************************
 *           HeapCompact   (kernelbase.@)
 */
SIZE_T WINAPI DECLSPEC_HOTPATCH HeapCompact( HANDLE heap, DWORD flags )
{
    return RtlCompactHeap( heap, flags );
}


/***********************************************************************
 *           HeapCreate   (kernelbase.@)
 */
HANDLE WINAPI DECLSPEC_HOTPATCH HeapCreate( DWORD flags, SIZE_T init_size, SIZE_T max_size )
{
    HANDLE ret = RtlCreateHeap( flags, NULL, max_size, init_size, NULL, NULL );
    if (!ret) SetLastError( ERROR_NOT_ENOUGH_MEMORY );
    return ret;
}


/***********************************************************************
 *           HeapDestroy   (kernelbase.@)
 */
BOOL WINAPI DECLSPEC_HOTPATCH HeapDestroy( HANDLE heap )
{
    if (!RtlDestroyHeap( heap )) return TRUE;
    SetLastError( ERROR_INVALID_HANDLE );
    return FALSE;
}


/***********************************************************************
 *           HeapLock   (kernelbase.@)
 */
BOOL WINAPI DECLSPEC_HOTPATCH HeapLock( HANDLE heap )
{
    return RtlLockHeap( heap );
}


/***********************************************************************
 *           HeapQueryInformation   (kernelbase.@)
 */
BOOL WINAPI HeapQueryInformation( HANDLE heap, HEAP_INFORMATION_CLASS info_class,
                                  PVOID info, SIZE_T size, PSIZE_T size_out )
{
    return set_ntstatus( RtlQueryHeapInformation( heap, info_class, info, size, size_out ));
}


/***********************************************************************
 *           HeapSetInformation   (kernelbase.@)
 */
BOOL WINAPI HeapSetInformation( HANDLE heap, HEAP_INFORMATION_CLASS infoclass, PVOID info, SIZE_T size )
{
    return set_ntstatus( RtlSetHeapInformation( heap, infoclass, info, size ));
}


/***********************************************************************
 *           HeapUnlock   (kernelbase.@)
 */
BOOL WINAPI HeapUnlock( HANDLE heap )
{
    return RtlUnlockHeap( heap );
}


/***********************************************************************
 *           HeapValidate   (kernelbase.@)
 */
BOOL WINAPI DECLSPEC_HOTPATCH HeapValidate( HANDLE heap, DWORD flags, LPCVOID ptr )
{
    return RtlValidateHeap( heap, flags, ptr );
}


/***********************************************************************
 *           HeapWalk   (kernelbase.@)
 */
BOOL WINAPI DECLSPEC_HOTPATCH HeapWalk( HANDLE heap, PROCESS_HEAP_ENTRY *entry )
{
    return set_ntstatus( RtlWalkHeap( heap, entry ));
}


/***********************************************************************
 * Global/local heap functions
 ***********************************************************************/

#include "pshpack1.h"

struct local_header
{
   WORD  magic;
   void *ptr;
   BYTE flags;
   BYTE lock;
};

#include "poppack.h"

#define MAGIC_LOCAL_USED    0x5342
/* align the storage needed for the HLOCAL on an 8-byte boundary thus
 * LocalAlloc/LocalReAlloc'ing with LMEM_MOVEABLE of memory with
 * size = 8*k, where k=1,2,3,... allocs exactly the given size.
 * The Minolta DiMAGE Image Viewer heavily relies on this, corrupting
 * the output jpeg's > 1 MB if not */
#define HLOCAL_STORAGE      (sizeof(HLOCAL) * 2)

static inline struct local_header *get_header( HLOCAL hmem )
{
    return (struct local_header *)((char *)hmem - 2);
}

static inline HLOCAL get_handle( struct local_header *header )
{
    return &header->ptr;
}

static inline BOOL is_pointer( HLOCAL hmem )
{
    return !((ULONG_PTR)hmem & 2);
}

/***********************************************************************
 *           GlobalAlloc   (kernelbase.@)
 */
HGLOBAL WINAPI DECLSPEC_HOTPATCH GlobalAlloc( UINT flags, SIZE_T size )
{
    /* mask out obsolete flags */
    flags &= ~(GMEM_NOCOMPACT | GMEM_NOT_BANKED | GMEM_NOTIFY);

    /* LocalAlloc allows a 0-size fixed block, but GlobalAlloc doesn't */
    if (!(flags & GMEM_MOVEABLE) && !size) size = 1;

    return LocalAlloc( flags, size );
}


/***********************************************************************
 *           GlobalFree   (kernelbase.@)
 */
HGLOBAL WINAPI DECLSPEC_HOTPATCH GlobalFree( HLOCAL hmem )
{
    return LocalFree( hmem );
}


/***********************************************************************
 *           LocalAlloc   (kernelbase.@)
 */
HLOCAL WINAPI DECLSPEC_HOTPATCH LocalAlloc( UINT flags, SIZE_T size )
{
    struct local_header *header;
    DWORD heap_flags = 0;
    void *ptr;

    if (flags & LMEM_ZEROINIT) heap_flags = HEAP_ZERO_MEMORY;

    if (!(flags & LMEM_MOVEABLE)) /* pointer */
    {
        ptr = HeapAlloc( GetProcessHeap(), heap_flags, size );
        TRACE( "(flags=%04x) returning %p\n",  flags, ptr );
        return ptr;
    }

    if (size > INT_MAX - HLOCAL_STORAGE)
    {
        SetLastError( ERROR_OUTOFMEMORY );
        return 0;
    }
    if (!(header = HeapAlloc( GetProcessHeap(), 0, sizeof(*header) ))) return 0;

    header->magic = MAGIC_LOCAL_USED;
    header->flags = flags >> 8;
    header->lock  = 0;

    if (size)
    {
        if (!(ptr = HeapAlloc(GetProcessHeap(), heap_flags, size + HLOCAL_STORAGE )))
        {
            HeapFree( GetProcessHeap(), 0, header );
            return 0;
        }
        *(HLOCAL *)ptr = get_handle( header );
        header->ptr = (char *)ptr + HLOCAL_STORAGE;
    }
    else header->ptr = NULL;

    TRACE( "(flags=%04x) returning handle %p pointer %p\n",
           flags, get_handle( header ), header->ptr );
    return get_handle( header );
}


/***********************************************************************
 *           LocalFree   (kernelbase.@)
 */
HLOCAL WINAPI DECLSPEC_HOTPATCH LocalFree( HLOCAL hmem )
{
    struct local_header *header;
    HLOCAL ret;

    RtlLockHeap( GetProcessHeap() );
    __TRY
    {
        ret = 0;
        if (is_pointer(hmem)) /* POINTER */
        {
            if (!HeapFree( GetProcessHeap(), HEAP_NO_SERIALIZE, hmem ))
            {
                SetLastError( ERROR_INVALID_HANDLE );
                ret = hmem;
            }
        }
        else  /* HANDLE */
        {
            header = get_header( hmem );
            if (header->magic == MAGIC_LOCAL_USED)
            {
                header->magic = 0xdead;
                if (header->ptr)
                {
                    if (!HeapFree( GetProcessHeap(), HEAP_NO_SERIALIZE,
                                   (char *)header->ptr - HLOCAL_STORAGE ))
                        ret = hmem;
                }
                if (!HeapFree( GetProcessHeap(), HEAP_NO_SERIALIZE, header )) ret = hmem;
            }
            else
            {
                WARN( "invalid handle %p (magic: 0x%04x)\n", hmem, header->magic );
                SetLastError( ERROR_INVALID_HANDLE );
                ret = hmem;
            }
        }
    }
    __EXCEPT_PAGE_FAULT
    {
        WARN( "invalid handle %p\n", hmem );
        SetLastError( ERROR_INVALID_HANDLE );
        ret = hmem;
    }
    __ENDTRY
    RtlUnlockHeap( GetProcessHeap() );
    return ret;
}


/***********************************************************************
 *           LocalLock   (kernelbase.@)
 */
LPVOID WINAPI DECLSPEC_HOTPATCH LocalLock( HLOCAL hmem )
{
    void *ret = NULL;

    if (is_pointer( hmem ))
    {
        __TRY
        {
            volatile char *p = hmem;
            *p |= 0;
        }
        __EXCEPT_PAGE_FAULT
        {
            return NULL;
        }
        __ENDTRY
        return hmem;
    }

    RtlLockHeap( GetProcessHeap() );
    __TRY
    {
        struct local_header *header = get_header( hmem );
        if (header->magic == MAGIC_LOCAL_USED)
        {
            ret = header->ptr;
            if (!header->ptr) SetLastError( ERROR_DISCARDED );
            else if (header->lock < LMEM_LOCKCOUNT) header->lock++;
        }
        else
        {
            WARN( "invalid handle %p (magic: 0x%04x)\n", hmem, header->magic );
            SetLastError( ERROR_INVALID_HANDLE );
        }
    }
    __EXCEPT_PAGE_FAULT
    {
        WARN("(%p): Page fault occurred ! Caused by bug ?\n", hmem);
        SetLastError( ERROR_INVALID_HANDLE );
    }
    __ENDTRY
    RtlUnlockHeap( GetProcessHeap() );
    return ret;
}


/***********************************************************************
 *           LocalReAlloc   (kernelbase.@)
 */
HLOCAL WINAPI DECLSPEC_HOTPATCH LocalReAlloc( HLOCAL hmem, SIZE_T size, UINT flags )
{
   struct local_header *header;
   void *ptr;
   HLOCAL ret = 0;
   DWORD heap_flags = (flags & LMEM_ZEROINIT) ? HEAP_ZERO_MEMORY : 0;

   RtlLockHeap( GetProcessHeap() );
   if (flags & LMEM_MODIFY) /* modify flags */
   {
      if (is_pointer( hmem ) && (flags & LMEM_MOVEABLE))
      {
         /* make a fixed block moveable
          * actually only NT is able to do this. But it's soo simple
          */
         if (hmem == 0)
         {
             WARN( "null handle\n");
             SetLastError( ERROR_NOACCESS );
         }
         else
         {
             size = RtlSizeHeap( GetProcessHeap(), 0, hmem );
             ret = LocalAlloc( flags, size );
             ptr = LocalLock( ret );
             memcpy( ptr, hmem, size );
             LocalUnlock( ret );
             LocalFree( hmem );
         }
      }
      else if (!is_pointer( hmem ) && (flags & LMEM_DISCARDABLE))
      {
         /* change the flags to make our block "discardable" */
         header = get_header( hmem );
         header->flags |= LMEM_DISCARDABLE >> 8;
         ret = hmem;
      }
      else SetLastError( ERROR_INVALID_PARAMETER );
   }
   else
   {
      if (is_pointer( hmem ))
      {
         /* reallocate fixed memory */
         if (!(flags & LMEM_MOVEABLE)) heap_flags |= HEAP_REALLOC_IN_PLACE_ONLY;
         ret = HeapReAlloc( GetProcessHeap(), heap_flags, hmem, size );
      }
      else
      {
          /* reallocate a moveable block */
          header = get_header( hmem );
          if (size != 0)
          {
              if (size <= INT_MAX - HLOCAL_STORAGE)
              {
                  if (header->ptr)
                  {
                      if ((ptr = HeapReAlloc( GetProcessHeap(), heap_flags,
                                              (char *)header->ptr - HLOCAL_STORAGE,
                                              size + HLOCAL_STORAGE )))
                      {
                          header->ptr = (char *)ptr + HLOCAL_STORAGE;
                          ret = hmem;
                      }
                  }
                  else
                  {
                      if ((ptr = HeapAlloc( GetProcessHeap(), heap_flags, size + HLOCAL_STORAGE )))
                      {
                          *(HLOCAL *)ptr = hmem;
                          header->ptr = (char *)ptr + HLOCAL_STORAGE;
                          ret = hmem;
                      }
                  }
              }
              else SetLastError( ERROR_OUTOFMEMORY );
          }
          else
          {
              if (header->lock == 0)
              {
                  if (header->ptr)
                  {
                      HeapFree( GetProcessHeap(), 0, (char *)header->ptr - HLOCAL_STORAGE );
                      header->ptr = NULL;
                  }
                  ret = hmem;
              }
              else WARN( "not freeing memory associated with locked handle\n" );
          }
      }
   }
   RtlUnlockHeap( GetProcessHeap() );
   return ret;
}


/***********************************************************************
 *           LocalUnlock   (kernelbase.@)
 */
BOOL WINAPI DECLSPEC_HOTPATCH LocalUnlock( HLOCAL hmem )
{
    BOOL ret = FALSE;

    if (is_pointer( hmem ))
    {
        SetLastError( ERROR_NOT_LOCKED );
        return FALSE;
    }

    RtlLockHeap( GetProcessHeap() );
    __TRY
    {
        struct local_header *header = get_header( hmem );
        if (header->magic == MAGIC_LOCAL_USED)
        {
            if (header->lock)
            {
                header->lock--;
                ret = (header->lock != 0);
                if (!ret) SetLastError( NO_ERROR );
            }
            else
            {
                WARN( "%p not locked\n", hmem );
                SetLastError( ERROR_NOT_LOCKED );
            }
        }
        else
        {
            WARN( "invalid handle %p (Magic: 0x%04x)\n", hmem, header->magic );
            SetLastError( ERROR_INVALID_HANDLE );
        }
    }
    __EXCEPT_PAGE_FAULT
    {
        WARN("(%p): Page fault occurred ! Caused by bug ?\n", hmem);
        SetLastError( ERROR_INVALID_PARAMETER );
    }
    __ENDTRY
    RtlUnlockHeap( GetProcessHeap() );
    return ret;
}


/***********************************************************************
 * Memory resource functions
 ***********************************************************************/


/***********************************************************************
 *           CreateMemoryResourceNotification   (kernelbase.@)
 */
HANDLE WINAPI DECLSPEC_HOTPATCH CreateMemoryResourceNotification( MEMORY_RESOURCE_NOTIFICATION_TYPE type )
{
    HANDLE ret;
    UNICODE_STRING nameW;
    OBJECT_ATTRIBUTES attr;

    switch (type)
    {
    case LowMemoryResourceNotification:
        RtlInitUnicodeString( &nameW, L"\\KernelObjects\\LowMemoryCondition" );
        break;
    case HighMemoryResourceNotification:
        RtlInitUnicodeString( &nameW, L"\\KernelObjects\\HighMemoryCondition" );
        break;
    default:
        SetLastError( ERROR_INVALID_PARAMETER );
        return 0;
    }

    InitializeObjectAttributes( &attr, &nameW, 0, 0, NULL );
    if (!set_ntstatus( NtOpenEvent( &ret, EVENT_ALL_ACCESS, &attr ))) return 0;
    return ret;
}

/***********************************************************************
 *          QueryMemoryResourceNotification   (kernelbase.@)
 */
BOOL WINAPI DECLSPEC_HOTPATCH QueryMemoryResourceNotification( HANDLE handle, BOOL *state )
{
    switch (WaitForSingleObject( handle, 0 ))
    {
    case WAIT_OBJECT_0:
        *state = TRUE;
        return TRUE;
    case WAIT_TIMEOUT:
        *state = FALSE;
        return TRUE;
    }
    SetLastError( ERROR_INVALID_PARAMETER );
    return FALSE;
}


/***********************************************************************
 * Physical memory functions
 ***********************************************************************/


/***********************************************************************
 *             AllocateUserPhysicalPages   (kernelbase.@)
 */
BOOL WINAPI DECLSPEC_HOTPATCH AllocateUserPhysicalPages( HANDLE process, ULONG_PTR *pages,
                                                         ULONG_PTR *userarray )
{
    FIXME( "stub: %p %p %p\n", process, pages, userarray );
    SetLastError( ERROR_CALL_NOT_IMPLEMENTED );
    return FALSE;
}


/***********************************************************************
 *             FreeUserPhysicalPages   (kernelbase.@)
 */
BOOL WINAPI DECLSPEC_HOTPATCH FreeUserPhysicalPages( HANDLE process, ULONG_PTR *pages,
                                                     ULONG_PTR *userarray )
{
    FIXME( "stub: %p %p %p\n", process, pages, userarray );
    *pages = 0;
    SetLastError( ERROR_CALL_NOT_IMPLEMENTED );
    return FALSE;
}


/***********************************************************************
 *             GetPhysicallyInstalledSystemMemory   (kernelbase.@)
 */
BOOL WINAPI DECLSPEC_HOTPATCH GetPhysicallyInstalledSystemMemory( ULONGLONG *memory )
{
    MEMORYSTATUSEX status;

    if (!memory)
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return FALSE;
    }
    status.dwLength = sizeof(status);
    GlobalMemoryStatusEx( &status );
    *memory = status.ullTotalPhys / 1024;
    return TRUE;
}


/***********************************************************************
 *             GlobalMemoryStatusEx   (kernelbase.@)
 */
BOOL WINAPI DECLSPEC_HOTPATCH GlobalMemoryStatusEx( MEMORYSTATUSEX *status )
{
    static MEMORYSTATUSEX cached_status;
    static DWORD last_check;
    SYSTEM_BASIC_INFORMATION basic_info;
    SYSTEM_PERFORMANCE_INFORMATION perf_info;

    if (status->dwLength != sizeof(*status))
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return FALSE;
    }
    if ((NtGetTickCount() - last_check) < 1000)
    {
	*status = cached_status;
	return TRUE;
    }
    last_check = NtGetTickCount();

    if (!set_ntstatus( NtQuerySystemInformation( SystemBasicInformation,
                                                 &basic_info, sizeof(basic_info), NULL )) ||
        !set_ntstatus( NtQuerySystemInformation( SystemPerformanceInformation,
                                                 &perf_info, sizeof(perf_info), NULL)))
        return FALSE;

    status->dwMemoryLoad     = 0;
    status->ullTotalPhys     = perf_info.TotalCommitLimit;
    status->ullAvailPhys     = perf_info.AvailablePages;
    status->ullTotalPageFile = perf_info.TotalCommitLimit + 1; /* Titan Quest refuses to run if TotalPageFile <= TotalPhys */
    status->ullAvailPageFile = status->ullTotalPageFile - perf_info.TotalCommittedPages;
    status->ullTotalVirtual  = (ULONG_PTR)basic_info.HighestUserAddress - (ULONG_PTR)basic_info.LowestUserAddress;
    status->ullAvailVirtual  = status->ullTotalVirtual - 64 * 1024;  /* FIXME */
    status->ullAvailExtendedVirtual = 0;

    status->ullTotalPhys     *= basic_info.PageSize;
    status->ullAvailPhys     *= basic_info.PageSize;
    status->ullTotalPageFile *= basic_info.PageSize;
    status->ullAvailPageFile *= basic_info.PageSize;

    if (status->ullTotalPhys)
        status->dwMemoryLoad = (status->ullTotalPhys - status->ullAvailPhys) / (status->ullTotalPhys / 100);

    TRACE_(virtual)( "MemoryLoad %d, TotalPhys %s, AvailPhys %s, TotalPageFile %s,"
                     "AvailPageFile %s, TotalVirtual %s, AvailVirtual %s\n",
                    status->dwMemoryLoad, wine_dbgstr_longlong(status->ullTotalPhys),
                    wine_dbgstr_longlong(status->ullAvailPhys), wine_dbgstr_longlong(status->ullTotalPageFile),
                    wine_dbgstr_longlong(status->ullAvailPageFile), wine_dbgstr_longlong(status->ullTotalVirtual),
                    wine_dbgstr_longlong(status->ullAvailVirtual) );

    cached_status = *status;
    return TRUE;
}


/***********************************************************************
 *             MapUserPhysicalPages   (kernelbase.@)
 */
BOOL WINAPI DECLSPEC_HOTPATCH MapUserPhysicalPages( void *addr, ULONG_PTR page_count, ULONG_PTR *pages )
{
    FIXME( "stub: %p %lu %p\n", addr, page_count, pages );
    *pages = 0;
    SetLastError( ERROR_CALL_NOT_IMPLEMENTED );
    return FALSE;
}


/***********************************************************************
 * NUMA functions
 ***********************************************************************/


/***********************************************************************
 *             AllocateUserPhysicalPagesNuma   (kernelbase.@)
 */
BOOL WINAPI DECLSPEC_HOTPATCH AllocateUserPhysicalPagesNuma( HANDLE process, ULONG_PTR *pages,
                                                             ULONG_PTR *userarray, DWORD node )
{
    if (node) FIXME( "Ignoring preferred node %u\n", node );
    return AllocateUserPhysicalPages( process, pages, userarray );
}


/***********************************************************************
 *             CreateFileMappingNumaW   (kernelbase.@)
 */
HANDLE WINAPI DECLSPEC_HOTPATCH CreateFileMappingNumaW( HANDLE file, LPSECURITY_ATTRIBUTES sa,
                                                        DWORD protect, DWORD size_high, DWORD size_low,
                                                        LPCWSTR name, DWORD node )
{
    if (node) FIXME( "Ignoring preferred node %u\n", node );
    return CreateFileMappingW( file, sa, protect, size_high, size_low, name );
}


/***********************************************************************
 *           GetLogicalProcessorInformation   (kernelbase.@)
 */
BOOL WINAPI DECLSPEC_HOTPATCH GetLogicalProcessorInformation( SYSTEM_LOGICAL_PROCESSOR_INFORMATION *buffer,
                                                              DWORD *len )
{
    NTSTATUS status;

    if (!len)
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return FALSE;
    }
    status = NtQuerySystemInformation( SystemLogicalProcessorInformation, buffer, *len, len );
    if (status == STATUS_INFO_LENGTH_MISMATCH) status = STATUS_BUFFER_TOO_SMALL;
    return set_ntstatus( status );
}


/***********************************************************************
 *           GetLogicalProcessorInformationEx   (kernelbase.@)
 */
BOOL WINAPI DECLSPEC_HOTPATCH GetLogicalProcessorInformationEx( LOGICAL_PROCESSOR_RELATIONSHIP relationship,
                                            SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *buffer, DWORD *len )
{
    NTSTATUS status;

    if (!len)
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return FALSE;
    }
    status = NtQuerySystemInformationEx( SystemLogicalProcessorInformationEx, &relationship,
                                         sizeof(relationship), buffer, *len, len );
    if (status == STATUS_INFO_LENGTH_MISMATCH) status = STATUS_BUFFER_TOO_SMALL;
    return set_ntstatus( status );
}


/**********************************************************************
 *             GetNumaHighestNodeNumber   (kernelbase.@)
 */
BOOL WINAPI DECLSPEC_HOTPATCH GetNumaHighestNodeNumber( ULONG *node )
{
    FIXME( "semi-stub: %p\n", node );
    *node = 0;
    return TRUE;
}


/**********************************************************************
 *             GetNumaNodeProcessorMaskEx   (kernelbase.@)
 */
BOOL WINAPI DECLSPEC_HOTPATCH GetNumaNodeProcessorMaskEx( USHORT node, GROUP_AFFINITY *mask )
{
    FIXME( "stub: %hu %p\n", node, mask );
    SetLastError( ERROR_CALL_NOT_IMPLEMENTED );
    return FALSE;
}


/***********************************************************************
 *             GetNumaProximityNodeEx   (kernelbase.@)
 */
BOOL WINAPI DECLSPEC_HOTPATCH GetNumaProximityNodeEx( ULONG proximity_id, USHORT *node )
{
    SetLastError( ERROR_CALL_NOT_IMPLEMENTED );
    return FALSE;
}


/***********************************************************************
 *             MapViewOfFileExNuma   (kernelbase.@)
 */
LPVOID WINAPI DECLSPEC_HOTPATCH MapViewOfFileExNuma( HANDLE handle, DWORD access, DWORD offset_high,
                                                     DWORD offset_low, SIZE_T count, LPVOID addr,
                                                     DWORD node )
{
    if (node) FIXME( "Ignoring preferred node %u\n", node );
    return MapViewOfFileEx( handle, access, offset_high, offset_low, count, addr );
}


/***********************************************************************
 *             VirtualAllocExNuma   (kernelbase.@)
 */
LPVOID WINAPI DECLSPEC_HOTPATCH VirtualAllocExNuma( HANDLE process, void *addr, SIZE_T size,
                                                    DWORD type, DWORD protect, DWORD node )
{
    if (node) FIXME( "Ignoring preferred node %u\n", node );
    return VirtualAllocEx( process, addr, size, type, protect );
}


/***********************************************************************
 * CPU functions
 ***********************************************************************/


#if defined(__i386__) || defined(__x86_64__)
/***********************************************************************
 *             GetEnabledXStateFeatures   (kernelbase.@)
 */
DWORD64 WINAPI GetEnabledXStateFeatures(void)
{
    TRACE( "\n" );
    return RtlGetEnabledExtendedFeatures( ~(ULONG64)0 );
}


/***********************************************************************
 *             InitializeContext2         (kernelbase.@)
 */
BOOL WINAPI InitializeContext2( void *buffer, DWORD context_flags, CONTEXT **context, DWORD *length,
        ULONG64 compaction_mask )
{
    ULONG orig_length;
    NTSTATUS status;

    TRACE( "buffer %p, context_flags %#x, context %p, ret_length %p, compaction_mask %s.\n",
            buffer, context_flags, context, length, wine_dbgstr_longlong(compaction_mask) );

    orig_length = *length;

    if ((status = RtlGetExtendedContextLength2( context_flags, length, compaction_mask )))
    {
        if (status == STATUS_NOT_SUPPORTED && context_flags & 0x40)
        {
            context_flags &= ~0x40;
            status = RtlGetExtendedContextLength2( context_flags, length, compaction_mask );
        }

        if (status)
            return set_ntstatus( status );
    }

    if (!buffer || orig_length < *length)
    {
        SetLastError( ERROR_INSUFFICIENT_BUFFER );
        return FALSE;
    }

    if ((status = RtlInitializeExtendedContext2( buffer, context_flags, (CONTEXT_EX **)context, compaction_mask )))
        return set_ntstatus( status );

    *context = (CONTEXT *)((BYTE *)*context + (*(CONTEXT_EX **)context)->Legacy.Offset);

    return TRUE;
}

/***********************************************************************
 *             InitializeContext               (kernelbase.@)
 */
BOOL WINAPI InitializeContext( void *buffer, DWORD context_flags, CONTEXT **context, DWORD *length )
{
    return InitializeContext2( buffer, context_flags, context, length, ~(ULONG64)0 );
}

/***********************************************************************
 *           CopyContext                       (kernelbase.@)
 */
BOOL WINAPI CopyContext( CONTEXT *dst, DWORD context_flags, CONTEXT *src )
{
    DWORD context_size, arch_flag, flags_offset, dst_flags, src_flags;
    static const DWORD arch_mask = 0x110000;
    NTSTATUS status;
    BYTE *d, *s;

    TRACE("dst %p, context_flags %#x, src %p.\n", dst, context_flags, src);

    if (context_flags & 0x40 && !RtlGetEnabledExtendedFeatures( ~(ULONG64)0 ))
    {
        SetLastError(ERROR_NOT_SUPPORTED);
        return FALSE;
    }

    arch_flag = context_flags & arch_mask;

    switch (arch_flag)
    {
        case  0x10000: context_size = 0x2cc; flags_offset =    0; break;
        case 0x100000: context_size = 0x4d0; flags_offset = 0x30; break;
        default:
            SetLastError( ERROR_INVALID_PARAMETER );
            return FALSE;
    }

    d = (BYTE *)dst;
    s = (BYTE *)src;
    dst_flags = *(DWORD *)(d + flags_offset);
    src_flags = *(DWORD *)(s + flags_offset);

    if ((dst_flags & arch_mask) != arch_flag
            || (src_flags & arch_mask) != arch_flag)
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return FALSE;
    }

    context_flags &= src_flags;

    if (context_flags & ~dst_flags & 0x40)
    {
        SetLastError(ERROR_MORE_DATA);
        return FALSE;
    }

    if ((status = RtlCopyExtendedContext( (CONTEXT_EX *)(d + context_size), context_flags,
            (CONTEXT_EX *)(s + context_size) )))
        return set_ntstatus( status );

    return TRUE;
}
#endif


#if defined(__x86_64__)
/***********************************************************************
 *           LocateXStateFeature   (kernelbase.@)
 */
void * WINAPI LocateXStateFeature( CONTEXT *context, DWORD feature_id, DWORD *length )
{
    if (!(context->ContextFlags & CONTEXT_AMD64))
        return NULL;

    if (feature_id >= 2)
        return ((context->ContextFlags & CONTEXT_XSTATE) == CONTEXT_XSTATE)
                ? RtlLocateExtendedFeature( (CONTEXT_EX *)(context + 1), feature_id, length ) : NULL;

    if (feature_id == 1)
    {
        if (length)
            *length = sizeof(M128A) * 16;

        return &context->u.FltSave.XmmRegisters;
    }

    if (length)
        *length = offsetof(XSAVE_FORMAT, XmmRegisters);

    return &context->u.FltSave;
}

/***********************************************************************
 *           SetXStateFeaturesMask (kernelbase.@)
 */
BOOL WINAPI SetXStateFeaturesMask( CONTEXT *context, DWORD64 feature_mask )
{
    if (!(context->ContextFlags & CONTEXT_AMD64))
        return FALSE;

    if (feature_mask & 0x3)
        context->ContextFlags |= CONTEXT_FLOATING_POINT;

    if ((context->ContextFlags & CONTEXT_XSTATE) != CONTEXT_XSTATE)
        return !(feature_mask & ~(DWORD64)3);

    RtlSetExtendedFeaturesMask( (CONTEXT_EX *)(context + 1), feature_mask );
    return TRUE;
}

/***********************************************************************
 *           GetXStateFeaturesMask (kernelbase.@)
 */
BOOL WINAPI GetXStateFeaturesMask( CONTEXT *context, DWORD64 *feature_mask )
{
    if (!(context->ContextFlags & CONTEXT_AMD64))
        return FALSE;

    *feature_mask = (context->ContextFlags & CONTEXT_FLOATING_POINT) == CONTEXT_FLOATING_POINT
            ? 3 : 0;

    if ((context->ContextFlags & CONTEXT_XSTATE) == CONTEXT_XSTATE)
        *feature_mask |= RtlGetExtendedFeaturesMask( (CONTEXT_EX *)(context + 1) );

    return TRUE;
}
#elif defined(__i386__)
/***********************************************************************
 *           LocateXStateFeature   (kernelbase.@)
 */
void * WINAPI LocateXStateFeature( CONTEXT *context, DWORD feature_id, DWORD *length )
{
    if (!(context->ContextFlags & CONTEXT_X86))
        return NULL;

    if (feature_id >= 2)
        return ((context->ContextFlags & CONTEXT_XSTATE) == CONTEXT_XSTATE)
                ? RtlLocateExtendedFeature( (CONTEXT_EX *)(context + 1), feature_id, length ) : NULL;

    if (feature_id == 1)
    {
        if (length)
            *length = sizeof(M128A) * 8;

        return (BYTE *)&context->ExtendedRegisters + offsetof(XSAVE_FORMAT, XmmRegisters);
    }

    if (length)
        *length = offsetof(XSAVE_FORMAT, XmmRegisters);

    return &context->ExtendedRegisters;
}

/***********************************************************************
 *           SetXStateFeaturesMask (kernelbase.@)
 */
BOOL WINAPI SetXStateFeaturesMask( CONTEXT *context, DWORD64 feature_mask )
{
    if (!(context->ContextFlags & CONTEXT_X86))
        return FALSE;

    if (feature_mask & 0x3)
        context->ContextFlags |= CONTEXT_EXTENDED_REGISTERS;

    if ((context->ContextFlags & CONTEXT_XSTATE) != CONTEXT_XSTATE)
        return !(feature_mask & ~(DWORD64)3);

    RtlSetExtendedFeaturesMask( (CONTEXT_EX *)(context + 1), feature_mask );
    return TRUE;
}

/***********************************************************************
 *           GetXStateFeaturesMask (kernelbase.@)
 */
BOOL WINAPI GetXStateFeaturesMask( CONTEXT *context, DWORD64 *feature_mask )
{
    if (!(context->ContextFlags & CONTEXT_X86))
        return FALSE;

    *feature_mask = (context->ContextFlags & CONTEXT_EXTENDED_REGISTERS) == CONTEXT_EXTENDED_REGISTERS
            ? 3 : 0;

    if ((context->ContextFlags & CONTEXT_XSTATE) == CONTEXT_XSTATE)
        *feature_mask |= RtlGetExtendedFeaturesMask( (CONTEXT_EX *)(context + 1) );

    return TRUE;
}
#endif

/***********************************************************************
 * Firmware functions
 ***********************************************************************/


/***********************************************************************
 *             EnumSystemFirmwareTable   (kernelbase.@)
 */
UINT WINAPI EnumSystemFirmwareTables( DWORD provider, void *buffer, DWORD size )
{
    FIXME( "(0x%08x, %p, %d)\n", provider, buffer, size );
    return 0;
}


/***********************************************************************
 *             GetSystemFirmwareTable   (kernelbase.@)
 */
UINT WINAPI GetSystemFirmwareTable( DWORD provider, DWORD id, void *buffer, DWORD size )
{
    SYSTEM_FIRMWARE_TABLE_INFORMATION *info;
    ULONG buffer_size = offsetof( SYSTEM_FIRMWARE_TABLE_INFORMATION, TableBuffer ) + size;

    TRACE( "(0x%08x, 0x%08x, %p, %d)\n", provider, id, buffer, size );

    if (!(info = RtlAllocateHeap( GetProcessHeap(), 0, buffer_size )))
    {
        SetLastError( ERROR_OUTOFMEMORY );
        return 0;
    }

    info->ProviderSignature = provider;
    info->Action = SystemFirmwareTable_Get;
    info->TableID = id;

    set_ntstatus( NtQuerySystemInformation( SystemFirmwareTableInformation,
                                            info, buffer_size, &buffer_size ));
    buffer_size -= offsetof( SYSTEM_FIRMWARE_TABLE_INFORMATION, TableBuffer );
    if (buffer_size <= size) memcpy( buffer, info->TableBuffer, buffer_size );

    HeapFree( GetProcessHeap(), 0, info );
    return buffer_size;
}
