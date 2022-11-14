/*
 * WoW64 virtual memory functions
 *
 * Copyright 2021 Alexandre Julliard
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

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winnt.h"
#include "winternl.h"
#include "winioctl.h"
#include "wow64_private.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(wow);


static MEMORY_RANGE_ENTRY *memory_range_entry_array_32to64( const MEMORY_RANGE_ENTRY32 *addresses32,
                                                            ULONG count )
{
    MEMORY_RANGE_ENTRY *addresses = Wow64AllocateTemp( sizeof(MEMORY_RANGE_ENTRY) * count );
    ULONG i;

    for (i = 0; i < count; i++)
    {
        addresses[i].VirtualAddress = ULongToPtr( addresses32[i].VirtualAddress );
        addresses[i].NumberOfBytes = addresses32[i].NumberOfBytes;
    }

    return addresses;
}

/**********************************************************************
 *           wow64_NtAllocateVirtualMemory
 */
NTSTATUS WINAPI wow64_NtAllocateVirtualMemory( UINT *args )
{
    HANDLE process = get_handle( &args );
    ULONG *addr32 = get_ptr( &args );
    ULONG_PTR zero_bits = get_ulong( &args );
    ULONG *size32 = get_ptr( &args );
    ULONG type = get_ulong( &args );
    ULONG protect = get_ulong( &args );

    void *addr;
    SIZE_T size;
    NTSTATUS status;

    status = NtAllocateVirtualMemory( process, addr_32to64( &addr, addr32 ), get_zero_bits( zero_bits ),
                                      size_32to64( &size, size32 ), type, protect );
    if (!status)
    {
        put_addr( addr32, addr );
        put_size( size32, size );
    }
    return status;
}


/**********************************************************************
 *           wow64_NtAllocateVirtualMemoryEx
 */
NTSTATUS WINAPI wow64_NtAllocateVirtualMemoryEx( UINT *args )
{
    HANDLE process = get_handle( &args );
    ULONG *addr32 = get_ptr( &args );
    ULONG *size32 = get_ptr( &args );
    ULONG type = get_ulong( &args );
    ULONG protect = get_ulong( &args );
    MEM_EXTENDED_PARAMETER *params = get_ptr( &args );
    ULONG count = get_ulong( &args );

    void *addr;
    SIZE_T size;
    NTSTATUS status;
    SIZE_T alloc_size = count * sizeof(*params);
    MEM_EXTENDED_PARAMETER *params64;
    BOOL set_highest_address = (!*addr32 && process == GetCurrentProcess());
    BOOL add_address_requirements = set_highest_address;
    MEM_ADDRESS_REQUIREMENTS *buf;
    unsigned int i;

    if (count && !params) return STATUS_INVALID_PARAMETER;

    for (i = 0; i < count; ++i)
    {
        if (params[i].Type == MemExtendedParameterAddressRequirements)
        {
            alloc_size += sizeof(MEM_ADDRESS_REQUIREMENTS);
            add_address_requirements = FALSE;
        }
        else if (params[i].Type && params[i].Type < MemExtendedParameterMax)
        {
            FIXME( "Unsupported parameter type %d.\n", params[i].Type);
        }
    }

    if (add_address_requirements)
        alloc_size += sizeof(*params) + sizeof(MEM_ADDRESS_REQUIREMENTS);
    params64 = Wow64AllocateTemp( alloc_size );
    memcpy( params64, params, count * sizeof(*params64) );
    if (add_address_requirements)
    {
        buf = (MEM_ADDRESS_REQUIREMENTS *)((char *)params64 + (count + 1) * sizeof(*params64));
        params64[count].Type = MemExtendedParameterAddressRequirements;
        params64[count].Pointer = buf;
        memset(buf, 0, sizeof(*buf));
        buf->HighestEndingAddress = (void *)highest_user_address;
        ++buf;
    }
    else
    {
        buf = (MEM_ADDRESS_REQUIREMENTS *)((char *)params64 + count * sizeof(*params64));
    }
    for (i = 0; i < count; ++i)
    {
        if (params64[i].Type == MemExtendedParameterAddressRequirements)
        {
            MEM_ADDRESS_REQUIREMENTS32 *p = (MEM_ADDRESS_REQUIREMENTS32 *)params[i].Pointer;

            buf->LowestStartingAddress = ULongToPtr(p->LowestStartingAddress);
            if (p->HighestEndingAddress)
            {
                if (p->HighestEndingAddress > highest_user_address) return STATUS_INVALID_PARAMETER;
                buf->HighestEndingAddress = ULongToPtr(p->HighestEndingAddress);
            }
            else
            {
                buf->HighestEndingAddress = set_highest_address ? (void *)highest_user_address : NULL;
            }
            buf->Alignment = p->Alignment;
            params64[i].Pointer = buf;
            ++buf;
        }
    }

    status = NtAllocateVirtualMemoryEx( process, addr_32to64( &addr, addr32 ), size_32to64( &size, size32 ),
                                        type, protect, params64, count + add_address_requirements );
    if (!status)
    {
        put_addr( addr32, addr );
        put_size( size32, size );
    }
    return status;
}


/**********************************************************************
 *           wow64_NtAreMappedFilesTheSame
 */
NTSTATUS WINAPI wow64_NtAreMappedFilesTheSame( UINT *args )
{
    void *ptr1 = get_ptr( &args );
    void *ptr2 = get_ptr( &args );

    return NtAreMappedFilesTheSame( ptr1, ptr2 );
}


/**********************************************************************
 *           wow64_NtFlushVirtualMemory
 */
NTSTATUS WINAPI wow64_NtFlushVirtualMemory( UINT *args )
{
    HANDLE process = get_handle( &args );
    ULONG *addr32 = get_ptr( &args );
    ULONG *size32 = get_ptr( &args );
    ULONG unknown = get_ulong( &args );

    void *addr;
    SIZE_T size;
    NTSTATUS status;

    status = NtFlushVirtualMemory( process, (const void **)addr_32to64( &addr, addr32 ),
                                   size_32to64( &size, size32 ), unknown );
    if (!status)
    {
        put_addr( addr32, addr );
        put_size( size32, size );
    }
    return status;
}


/**********************************************************************
 *           wow64_NtFreeVirtualMemory
 */
NTSTATUS WINAPI wow64_NtFreeVirtualMemory( UINT *args )
{
    HANDLE process = get_handle( &args );
    ULONG *addr32 = get_ptr( &args );
    ULONG *size32 = get_ptr( &args );
    ULONG type = get_ulong( &args );

    void *addr;
    SIZE_T size;
    NTSTATUS status;

    status = NtFreeVirtualMemory( process, addr_32to64( &addr, addr32 ),
                                  size_32to64( &size, size32 ), type );
    if (!status)
    {
        put_addr( addr32, addr );
        put_size( size32, size );
    }
    return status;
}


/**********************************************************************
 *           wow64_NtGetNlsSectionPtr
 */
NTSTATUS WINAPI wow64_NtGetNlsSectionPtr( UINT *args )
{
    ULONG type = get_ulong( &args );
    ULONG id = get_ulong( &args );
    void *unknown = get_ptr( &args );
    ULONG *addr32 = get_ptr( &args );
    ULONG *size32 = get_ptr( &args );

    void *addr;
    SIZE_T size;
    NTSTATUS status;

    status = NtGetNlsSectionPtr( type, id, unknown, addr_32to64( &addr, addr32 ),
                                 size_32to64( &size, size32 ));
    if (!status)
    {
        put_addr( addr32, addr );
        put_size( size32, size );
    }
    return status;
}


/**********************************************************************
 *           wow64_NtGetWriteWatch
 */
NTSTATUS WINAPI wow64_NtGetWriteWatch( UINT *args )
{
    HANDLE handle = get_handle( &args );
    ULONG flags = get_ulong( &args );
    void *base = get_ptr( &args );
    SIZE_T size = get_ulong( &args );
    ULONG *addr_ptr = get_ptr( &args );
    ULONG *count_ptr = get_ptr( &args );
    ULONG *granularity = get_ptr( &args );

    ULONG_PTR i, count = *count_ptr;
    void **addresses;
    NTSTATUS status;

    if (!count || !size) return STATUS_INVALID_PARAMETER;
    if (flags & ~WRITE_WATCH_FLAG_RESET) return STATUS_INVALID_PARAMETER;
    if (!addr_ptr) return STATUS_ACCESS_VIOLATION;

    addresses = Wow64AllocateTemp( count * sizeof(*addresses) );
    if (!(status = NtGetWriteWatch( handle, flags, base, size, addresses, &count, granularity )))
    {
        for (i = 0; i < count; i++) addr_ptr[i] = PtrToUlong( addresses[i] );
        *count_ptr = count;
    }
    return status;
}


/**********************************************************************
 *           wow64_NtInitializeNlsFiles
 */
NTSTATUS WINAPI wow64_NtInitializeNlsFiles( UINT *args )
{
    ULONG *addr32 = get_ptr( &args );
    LCID *lcid = get_ptr( &args );
    LARGE_INTEGER *size = get_ptr( &args );

    void *addr;
    NTSTATUS status;

    status = NtInitializeNlsFiles( addr_32to64( &addr, addr32 ), lcid, size );
    if (!status) put_addr( addr32, addr );
    return status;
}


/**********************************************************************
 *           wow64_NtLockVirtualMemory
 */
NTSTATUS WINAPI wow64_NtLockVirtualMemory( UINT *args )
{
    HANDLE process = get_handle( &args );
    ULONG *addr32 = get_ptr( &args );
    ULONG *size32 = get_ptr( &args );
    ULONG unknown = get_ulong( &args );

    void *addr;
    SIZE_T size;
    NTSTATUS status;

    status = NtLockVirtualMemory( process, addr_32to64( &addr, addr32 ),
                                  size_32to64( &size, size32 ), unknown );
    if (!status)
    {
        put_addr( addr32, addr );
        put_size( size32, size );
    }
    return status;
}


/**********************************************************************
 *           wow64_NtMapViewOfSection
 */
NTSTATUS WINAPI wow64_NtMapViewOfSection( UINT *args )
{
    HANDLE handle = get_handle( &args );
    HANDLE process = get_handle( &args );
    ULONG *addr32 = get_ptr( &args );
    ULONG_PTR zero_bits = get_ulong( &args );
    SIZE_T commit = get_ulong( &args );
    const LARGE_INTEGER *offset = get_ptr( &args );
    ULONG *size32 = get_ptr( &args );
    SECTION_INHERIT inherit = get_ulong( &args );
    ULONG alloc = get_ulong( &args );
    ULONG protect = get_ulong( &args );

    void *addr;
    SIZE_T size;
    NTSTATUS status;

    status = NtMapViewOfSection( handle, process, addr_32to64( &addr, addr32 ), get_zero_bits( zero_bits ),
                                 commit, offset, size_32to64( &size, size32 ), inherit, alloc, protect );
    if (NT_SUCCESS(status))
    {
        SECTION_IMAGE_INFORMATION info;

        if (!NtQuerySection( handle, SectionImageInformation, &info, sizeof(info), NULL ))
        {
            if (info.Machine == current_machine) init_image_mapping( addr );
        }
        put_addr( addr32, addr );
        put_size( size32, size );
    }
    return status;
}

/**********************************************************************
 *           wow64_NtMapViewOfSectionEx
 */
NTSTATUS WINAPI wow64_NtMapViewOfSectionEx( UINT *args )
{
    HANDLE handle = get_handle( &args );
    HANDLE process = get_handle( &args );
    ULONG *addr32 = get_ptr( &args );
    const LARGE_INTEGER *offset = get_ptr( &args );
    ULONG *size32 = get_ptr( &args );
    ULONG alloc = get_ulong( &args );
    ULONG protect = get_ulong( &args );
    MEM_EXTENDED_PARAMETER *params = get_ptr( &args );
    ULONG params_count = get_ulong( &args );

    void *addr;
    SIZE_T size;
    NTSTATUS status;

    status = NtMapViewOfSectionEx( handle, process, addr_32to64( &addr, addr32 ), offset, size_32to64( &size, size32 ), alloc,
            protect, params, params_count );
    if (NT_SUCCESS(status))
    {
        SECTION_IMAGE_INFORMATION info;

        if (!NtQuerySection( handle, SectionImageInformation, &info, sizeof(info), NULL ))
        {
            if (info.Machine == current_machine) init_image_mapping( addr );
        }
        put_addr( addr32, addr );
        put_size( size32, size );
    }
    return status;
}

/**********************************************************************
 *           wow64_NtProtectVirtualMemory
 */
NTSTATUS WINAPI wow64_NtProtectVirtualMemory( UINT *args )
{
    HANDLE process = get_handle( &args );
    ULONG *addr32 = get_ptr( &args );
    ULONG *size32 = get_ptr( &args );
    ULONG new_prot = get_ulong( &args );
    ULONG *old_prot = get_ptr( &args );

    void *addr;
    SIZE_T size;
    NTSTATUS status;

    status = NtProtectVirtualMemory( process, addr_32to64( &addr, addr32 ),
                                     size_32to64( &size, size32 ), new_prot, old_prot );
    if (!status)
    {
        put_addr( addr32, addr );
        put_size( size32, size );
    }
    return status;
}


/**********************************************************************
 *           wow64_NtQueryVirtualMemory
 */
NTSTATUS WINAPI wow64_NtQueryVirtualMemory( UINT *args )
{
    HANDLE handle = get_handle( &args );
    void *addr = get_ptr( &args );
    MEMORY_INFORMATION_CLASS class = get_ulong( &args );
    void *ptr = get_ptr( &args );
    ULONG len = get_ulong( &args );
    ULONG *retlen = get_ptr( &args );

    SIZE_T res_len = 0;
    NTSTATUS status;

    switch (class)
    {
    case MemoryBasicInformation:  /* MEMORY_BASIC_INFORMATION */
        if (len < sizeof(MEMORY_BASIC_INFORMATION32))
            status = STATUS_INFO_LENGTH_MISMATCH;
        else if ((ULONG_PTR)addr > highest_user_address)
            status = STATUS_INVALID_PARAMETER;
        else
        {
            MEMORY_BASIC_INFORMATION info;
            MEMORY_BASIC_INFORMATION32 *info32 = ptr;

            if (!(status = NtQueryVirtualMemory( handle, addr, class, &info, sizeof(info), &res_len )))
            {
                info32->BaseAddress = PtrToUlong( info.BaseAddress );
                info32->AllocationBase = PtrToUlong( info.AllocationBase );
                info32->AllocationProtect = info.AllocationProtect;
                info32->RegionSize = info.RegionSize;
                info32->State = info.State;
                info32->Protect = info.Protect;
                info32->Type = info.Type;
                if ((ULONG_PTR)info.BaseAddress + info.RegionSize > highest_user_address)
                    info32->RegionSize = highest_user_address - (ULONG_PTR)info.BaseAddress + 1;
            }
        }
        res_len = sizeof(MEMORY_BASIC_INFORMATION32);
        break;

    case MemoryMappedFilenameInformation:  /* MEMORY_SECTION_NAME */
    {
        MEMORY_SECTION_NAME *info;
        MEMORY_SECTION_NAME32 *info32 = ptr;
        SIZE_T size = len + sizeof(*info) - sizeof(*info32);

        info = Wow64AllocateTemp( size );
        if (!(status = NtQueryVirtualMemory( handle, addr, class, info, size, &res_len )))
        {
            info32->SectionFileName.Length = info->SectionFileName.Length;
            info32->SectionFileName.MaximumLength = info->SectionFileName.MaximumLength;
            info32->SectionFileName.Buffer = PtrToUlong( info32 + 1 );
            memcpy( info32 + 1, info->SectionFileName.Buffer, info->SectionFileName.MaximumLength );
        }
        res_len += sizeof(*info32) - sizeof(*info);
        break;
    }

    case MemoryRegionInformation: /* MEMORY_REGION_INFORMATION */
    {
        if (len < sizeof(MEMORY_REGION_INFORMATION32))
            status = STATUS_INFO_LENGTH_MISMATCH;
        if ((ULONG_PTR)addr > highest_user_address)
            status = STATUS_INVALID_PARAMETER;
        else
        {
            MEMORY_REGION_INFORMATION info;
            MEMORY_REGION_INFORMATION32 *info32 = ptr;

            if (!(status = NtQueryVirtualMemory( handle, addr, class, &info, sizeof(info), &res_len )))
            {
                info32->AllocationBase = PtrToUlong( info.AllocationBase );
                info32->AllocationProtect = info.AllocationProtect;
                info32->RegionType = info.RegionType;
                info32->RegionSize = info.RegionSize;
                info32->CommitSize = info.CommitSize;
                info32->PartitionId = info.PartitionId;
                info32->NodePreference = info.NodePreference;
                if ((ULONG_PTR)info.AllocationBase + info.RegionSize > highest_user_address)
                    info32->RegionSize = highest_user_address - (ULONG_PTR)info.AllocationBase + 1;
            }
        }
        res_len = sizeof(MEMORY_REGION_INFORMATION32);
        break;
    }

    case MemoryWorkingSetExInformation:  /* MEMORY_WORKING_SET_EX_INFORMATION */
    {
        MEMORY_WORKING_SET_EX_INFORMATION32 *info32 = ptr;
        MEMORY_WORKING_SET_EX_INFORMATION *info;
        ULONG i, count = len / sizeof(*info32);

        info = Wow64AllocateTemp( count * sizeof(*info) );
        for (i = 0; i < count; i++) info[i].VirtualAddress = ULongToPtr( info32[i].VirtualAddress );
        if (!(status = NtQueryVirtualMemory( handle, addr, class, info, count * sizeof(*info), &res_len )))
        {
            count = res_len / sizeof(*info);
            for (i = 0; i < count; i++) info32[i].VirtualAttributes.Flags = info[i].VirtualAttributes.Flags;
            res_len = count * sizeof(*info32);
        }
        break;
    }

    case MemoryWineUnixWow64Funcs:
        return STATUS_INVALID_INFO_CLASS;

    case MemoryWineUnixFuncs:
        status = NtQueryVirtualMemory( handle, addr, MemoryWineUnixWow64Funcs, ptr, len, &res_len );
        break;

    default:
        FIXME( "unsupported class %u\n", class );
        return STATUS_INVALID_INFO_CLASS;
    }
    if (!status || status == STATUS_INFO_LENGTH_MISMATCH) put_size( retlen, res_len );
    return status;
}


/**********************************************************************
 *           wow64_NtReadVirtualMemory
 */
NTSTATUS WINAPI wow64_NtReadVirtualMemory( UINT *args )
{
    HANDLE process = get_handle( &args );
    const void *addr = get_ptr( &args );
    void *buffer = get_ptr( &args );
    SIZE_T size = get_ulong( &args );
    ULONG *retlen = get_ptr( &args );

    SIZE_T ret_size;
    NTSTATUS status;

    status = NtReadVirtualMemory( process, addr, buffer, size, &ret_size );
    put_size( retlen, ret_size );
    return status;
}


/**********************************************************************
 *           wow64_NtResetWriteWatch
 */
NTSTATUS WINAPI wow64_NtResetWriteWatch( UINT *args )
{
    HANDLE process = get_handle( &args );
    void *base = get_ptr( &args );
    SIZE_T size = get_ulong( &args );

    return NtResetWriteWatch( process, base, size );
}


/**********************************************************************
 *           wow64_NtSetInformationVirtualMemory
 */
NTSTATUS WINAPI wow64_NtSetInformationVirtualMemory( UINT *args )
{
    HANDLE process = get_handle( &args );
    VIRTUAL_MEMORY_INFORMATION_CLASS info_class = get_ulong( &args );
    ULONG count = get_ulong( &args );
    MEMORY_RANGE_ENTRY32 *addresses32 = get_ptr( &args );
    PVOID ptr = get_ptr( &args );
    ULONG len = get_ulong( &args );

    MEMORY_RANGE_ENTRY *addresses;

    if (!count) return STATUS_INVALID_PARAMETER_3;
    addresses = memory_range_entry_array_32to64( addresses32, count );

    switch (info_class)
    {
    case VmPrefetchInformation:
        break;
    default:
        FIXME( "(%p,info_class=%u,%lu,%p,%p,%lu): not implemented\n",
               process, info_class, count, addresses32, ptr, len );
        return STATUS_INVALID_PARAMETER_2;
    }

    return NtSetInformationVirtualMemory( process, info_class, count, addresses, ptr, len );
}


/**********************************************************************
 *           wow64_NtSetLdtEntries
 */
NTSTATUS WINAPI wow64_NtSetLdtEntries( UINT *args )
{
    ULONG sel1 = get_ulong( &args );
    ULONG entry1_low = get_ulong( &args );
    ULONG entry1_high = get_ulong( &args );
    ULONG sel2 = get_ulong( &args );
    ULONG entry2_low = get_ulong( &args );
    ULONG entry2_high = get_ulong( &args );

    FIXME( "%04lx %08lx %08lx %04lx %08lx %08lx: stub\n",
           sel1, entry1_low, entry1_high, sel2, entry2_low, entry2_high );
    return STATUS_NOT_IMPLEMENTED;
}


/**********************************************************************
 *           wow64_NtUnlockVirtualMemory
 */
NTSTATUS WINAPI wow64_NtUnlockVirtualMemory( UINT *args )
{
    HANDLE process = get_handle( &args );
    ULONG *addr32 = get_ptr( &args );
    ULONG *size32 = get_ptr( &args );
    ULONG unknown = get_ulong( &args );

    void *addr;
    SIZE_T size;
    NTSTATUS status;

    status = NtUnlockVirtualMemory( process, addr_32to64( &addr, addr32 ),
                                    size_32to64( &size, size32 ), unknown );
    if (!status)
    {
        put_addr( addr32, addr );
        put_size( size32, size );
    }
    return status;
}


/**********************************************************************
 *           wow64_NtUnmapViewOfSection
 */
NTSTATUS WINAPI wow64_NtUnmapViewOfSection( UINT *args )
{
    HANDLE process = get_handle( &args );
    void *addr = get_ptr( &args );

    return NtUnmapViewOfSection( process, addr );
}


/**********************************************************************
 *           wow64_NtUnmapViewOfSectionEx
 */
NTSTATUS WINAPI wow64_NtUnmapViewOfSectionEx( UINT *args )
{
    HANDLE process = get_handle( &args );
    void *addr = get_ptr( &args );
    ULONG flags = get_ulong( &args );

    return NtUnmapViewOfSectionEx( process, addr, flags );
}


/**********************************************************************
 *           wow64_NtWow64AllocateVirtualMemory64
 */
NTSTATUS WINAPI wow64_NtWow64AllocateVirtualMemory64( UINT *args )
{
    HANDLE process = get_handle( &args );
    void **addr = get_ptr( &args );
    ULONG_PTR zero_bits = get_ulong64( &args );
    SIZE_T *size = get_ptr( &args );
    ULONG type = get_ulong( &args );
    ULONG protect = get_ulong( &args );

    return NtAllocateVirtualMemory( process, addr, zero_bits, size, type, protect );
}


/**********************************************************************
 *           wow64_NtWow64ReadVirtualMemory64
 */
NTSTATUS WINAPI wow64_NtWow64ReadVirtualMemory64( UINT *args )
{
    HANDLE process = get_handle( &args );
    void *addr = (void *)(ULONG_PTR)get_ulong64( &args );
    void *buffer = get_ptr( &args );
    SIZE_T size = get_ulong64( &args );
    SIZE_T *ret_size = get_ptr( &args );

    return NtReadVirtualMemory( process, addr, buffer, size, ret_size );
}


/**********************************************************************
 *           wow64_NtWow64WriteVirtualMemory64
 */
NTSTATUS WINAPI wow64_NtWow64WriteVirtualMemory64( UINT *args )
{
    HANDLE process = get_handle( &args );
    void *addr = (void *)(ULONG_PTR)get_ulong64( &args );
    const void *buffer = get_ptr( &args );
    SIZE_T size = get_ulong64( &args );
    SIZE_T *ret_size = get_ptr( &args );

    return NtWriteVirtualMemory( process, addr, buffer, size, ret_size );
}


/**********************************************************************
 *           wow64_NtWriteVirtualMemory
 */
NTSTATUS WINAPI wow64_NtWriteVirtualMemory( UINT *args )
{
    HANDLE process = get_handle( &args );
    void *addr = get_ptr( &args );
    const void *buffer = get_ptr( &args );
    SIZE_T size = get_ulong( &args );
    ULONG *retlen = get_ptr( &args );

    SIZE_T ret_size;
    NTSTATUS status;

    status = NtWriteVirtualMemory( process, addr, buffer, size, &ret_size );
    put_size( retlen, ret_size );
    return status;
}
