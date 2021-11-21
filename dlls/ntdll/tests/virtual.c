/*
 * Unit test suite for the virtual memory APIs.
 *
 * Copyright 2019 Remi Bernon for CodeWeavers
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
#include "winternl.h"
#include "wine/test.h"
#include "ddk/wdm.h"

static unsigned int page_size;

static DWORD64 (WINAPI *pGetEnabledXStateFeatures)(void);
static NTSTATUS (WINAPI *pRtlCreateUserStack)(SIZE_T, SIZE_T, ULONG, SIZE_T, SIZE_T, INITIAL_TEB *);
static NTSTATUS (WINAPI *pRtlCreateUserThread)(HANDLE, SECURITY_DESCRIPTOR*, BOOLEAN, ULONG, SIZE_T,
                                               SIZE_T, PRTL_THREAD_START_ROUTINE, void*, HANDLE*, CLIENT_ID* );
static ULONG64 (WINAPI *pRtlGetEnabledExtendedFeatures)(ULONG64);
static NTSTATUS (WINAPI *pRtlFreeUserStack)(void *);
static void * (WINAPI *pRtlFindExportedRoutineByName)(HMODULE,const char*);
static BOOL (WINAPI *pIsWow64Process)(HANDLE, PBOOL);
static NTSTATUS (WINAPI *pNtAllocateVirtualMemoryEx)(HANDLE, PVOID *, SIZE_T *, ULONG, ULONG,
                                                     MEM_EXTENDED_PARAMETER *, ULONG);
static const BOOL is_win64 = sizeof(void*) != sizeof(int);
static BOOL is_wow64;

static SYSTEM_BASIC_INFORMATION sbi;

static HANDLE create_target_process(const char *arg)
{
    char **argv;
    char cmdline[MAX_PATH];
    PROCESS_INFORMATION pi;
    BOOL ret;
    STARTUPINFOA si = { 0 };
    si.cb = sizeof(si);

    winetest_get_mainargs(&argv);
    sprintf(cmdline, "%s %s %s", argv[0], argv[1], arg);
    ret = CreateProcessA(NULL, cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    ok(ret, "error: %u\n", GetLastError());
    ret = CloseHandle(pi.hThread);
    ok(ret, "error %u\n", GetLastError());
    return pi.hProcess;
}

static UINT_PTR get_zero_bits(UINT_PTR p)
{
    UINT_PTR z = 0;

#ifdef _WIN64
    if (p >= 0xffffffff)
        return (~(UINT_PTR)0) >> get_zero_bits(p >> 32);
#endif

    if (p == 0) return 0;
    while ((p >> (31 - z)) != 1) z++;
    return z;
}

static UINT_PTR get_zero_bits_mask(ULONG_PTR z)
{
    if (z >= 32)
    {
        z = get_zero_bits(z);
#ifdef _WIN64
        if (z >= 32) return z;
#endif
    }
    return (~(UINT32)0) >> z;
}

static void test_NtAllocateVirtualMemory(void)
{
    void *addr1, *addr2;
    NTSTATUS status;
    SIZE_T size;
    ULONG_PTR zero_bits;

    /* simple allocation should success */
    size = 0x1000;
    addr1 = NULL;
    status = NtAllocateVirtualMemory(NtCurrentProcess(), &addr1, 0, &size,
                                     MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    ok(status == STATUS_SUCCESS, "NtAllocateVirtualMemory returned %08x\n", status);

    /* allocation conflicts because of 64k align */
    size = 0x1000;
    addr2 = (char *)addr1 + 0x1000;
    status = NtAllocateVirtualMemory(NtCurrentProcess(), &addr2, 0, &size,
                                     MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    ok(status == STATUS_CONFLICTING_ADDRESSES, "NtAllocateVirtualMemory returned %08x\n", status);

    /* it should conflict, even when zero_bits is explicitly set */
    size = 0x1000;
    addr2 = (char *)addr1 + 0x1000;
    status = NtAllocateVirtualMemory(NtCurrentProcess(), &addr2, 12, &size,
                                     MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    ok(status == STATUS_CONFLICTING_ADDRESSES, "NtAllocateVirtualMemory returned %08x\n", status);

    /* 1 zero bits should zero 63-31 upper bits */
    size = 0x1000;
    addr2 = NULL;
    zero_bits = 1;
    status = NtAllocateVirtualMemory(NtCurrentProcess(), &addr2, zero_bits, &size,
                                     MEM_RESERVE | MEM_COMMIT | MEM_TOP_DOWN,
                                     PAGE_READWRITE);
    ok(status == STATUS_SUCCESS || status == STATUS_NO_MEMORY ||
       broken(status == STATUS_INVALID_PARAMETER_3) /* winxp */,
       "NtAllocateVirtualMemory returned %08x\n", status);
    if (status == STATUS_SUCCESS)
    {
        ok(((UINT_PTR)addr2 >> (32 - zero_bits)) == 0,
           "NtAllocateVirtualMemory returned address: %p\n", addr2);

        size = 0;
        status = NtFreeVirtualMemory(NtCurrentProcess(), &addr2, &size, MEM_RELEASE);
        ok(status == STATUS_SUCCESS, "NtFreeVirtualMemory return %08x, addr2: %p\n", status, addr2);
    }

    for (zero_bits = 2; zero_bits <= 20; zero_bits++)
    {
        size = 0x1000;
        addr2 = NULL;
        status = NtAllocateVirtualMemory(NtCurrentProcess(), &addr2, zero_bits, &size,
                                         MEM_RESERVE | MEM_COMMIT | MEM_TOP_DOWN,
                                         PAGE_READWRITE);
        ok(status == STATUS_SUCCESS || status == STATUS_NO_MEMORY ||
           broken(zero_bits == 20 && status == STATUS_CONFLICTING_ADDRESSES) /* w1064v1809 */,
           "NtAllocateVirtualMemory with %d zero_bits returned %08x\n", (int)zero_bits, status);
        if (status == STATUS_SUCCESS)
        {
            ok(((UINT_PTR)addr2 >> (32 - zero_bits)) == 0,
               "NtAllocateVirtualMemory with %d zero_bits returned address %p\n", (int)zero_bits, addr2);

            size = 0;
            status = NtFreeVirtualMemory(NtCurrentProcess(), &addr2, &size, MEM_RELEASE);
            ok(status == STATUS_SUCCESS, "NtFreeVirtualMemory return %08x, addr2: %p\n", status, addr2);
        }
    }

    /* 21 zero bits never succeeds */
    size = 0x1000;
    addr2 = NULL;
    status = NtAllocateVirtualMemory(NtCurrentProcess(), &addr2, 21, &size,
                                     MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    ok(status == STATUS_NO_MEMORY || status == STATUS_INVALID_PARAMETER,
       "NtAllocateVirtualMemory returned %08x\n", status);
    if (status == STATUS_SUCCESS)
    {
        size = 0;
        status = NtFreeVirtualMemory(NtCurrentProcess(), &addr2, &size, MEM_RELEASE);
        ok(status == STATUS_SUCCESS, "NtFreeVirtualMemory return %08x, addr2: %p\n", status, addr2);
    }

    /* 22 zero bits is invalid */
    size = 0x1000;
    addr2 = NULL;
    status = NtAllocateVirtualMemory(NtCurrentProcess(), &addr2, 22, &size,
                                     MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    ok(status == STATUS_INVALID_PARAMETER_3 || status == STATUS_INVALID_PARAMETER,
       "NtAllocateVirtualMemory returned %08x\n", status);

    /* zero bits > 31 should be considered as a leading zeroes bitmask on 64bit and WoW64 */
    size = 0x1000;
    addr2 = NULL;
    zero_bits = 0x1aaaaaaa;
    status = NtAllocateVirtualMemory(NtCurrentProcess(), &addr2, zero_bits, &size,
                                      MEM_RESERVE | MEM_COMMIT | MEM_TOP_DOWN,
                                      PAGE_READWRITE);

    if (!is_win64 && !is_wow64)
    {
        ok(status == STATUS_INVALID_PARAMETER_3, "NtAllocateVirtualMemory returned %08x\n", status);
    }
    else
    {
        ok(status == STATUS_SUCCESS || status == STATUS_NO_MEMORY,
           "NtAllocateVirtualMemory returned %08x\n", status);
        if (status == STATUS_SUCCESS)
        {
            ok(((UINT_PTR)addr2 & ~get_zero_bits_mask(zero_bits)) == 0 &&
               ((UINT_PTR)addr2 & ~zero_bits) != 0, /* only the leading zeroes matter */
               "NtAllocateVirtualMemory returned address %p\n", addr2);

            size = 0;
            status = NtFreeVirtualMemory(NtCurrentProcess(), &addr2, &size, MEM_RELEASE);
            ok(status == STATUS_SUCCESS, "NtFreeVirtualMemory return %08x, addr2: %p\n", status, addr2);
        }
    }

    /* AT_ROUND_TO_PAGE flag is not supported for NtAllocateVirtualMemory */
    size = 0x1000;
    addr2 = (char *)addr1 + 0x1000;
    status = NtAllocateVirtualMemory(NtCurrentProcess(), &addr2, 0, &size,
                                     MEM_RESERVE | MEM_COMMIT | AT_ROUND_TO_PAGE, PAGE_EXECUTE_READWRITE);
    ok(status == STATUS_INVALID_PARAMETER_5 || status == STATUS_INVALID_PARAMETER,
       "NtAllocateVirtualMemory returned %08x\n", status);

    size = 0;
    status = NtFreeVirtualMemory(NtCurrentProcess(), &addr1, &size, MEM_RELEASE);
    ok(status == STATUS_SUCCESS, "NtFreeVirtualMemory failed\n");

    if (!pNtAllocateVirtualMemoryEx)
    {
        win_skip("NtAllocateVirtualMemoryEx() is missing\n");
        return;
    }

    /* simple allocation should succeed */
    size = 0x1000;
    addr1 = NULL;
    status = pNtAllocateVirtualMemoryEx(NtCurrentProcess(), &addr1, &size, MEM_RESERVE | MEM_COMMIT,
                                        PAGE_EXECUTE_READWRITE, NULL, 0);
    ok(status == STATUS_SUCCESS, "NtAllocateVirtualMemoryEx returned %08x\n", status);

    /* specifying a count of >0 with NULL parameters should fail */
    status = pNtAllocateVirtualMemoryEx(NtCurrentProcess(), &addr1, &size, MEM_RESERVE | MEM_COMMIT,
                                        PAGE_EXECUTE_READWRITE, NULL, 1);
    ok(status == STATUS_INVALID_PARAMETER, "NtAllocateVirtualMemoryEx returned %08x\n", status);
}

struct test_stack_size_thread_args
{
    DWORD expect_committed;
    DWORD expect_reserved;
};

static void force_stack_grow(void)
{
    volatile int buffer[0x2000];
    buffer[0] = 0xdeadbeef;
    (void)buffer[0];
}

#ifdef _WIN64
static void force_stack_grow_small(void)
{
    volatile int buffer[0x400];
    buffer[0] = 0xdeadbeef;
    (void)buffer[0];
}
#endif

static DWORD WINAPI test_stack_size_thread(void *ptr)
{
    struct test_stack_size_thread_args *args = ptr;
    MEMORY_BASIC_INFORMATION mbi;
    NTSTATUS status;
    SIZE_T size, guard_size;
    DWORD committed, reserved;
    void *addr;
#ifdef _WIN64
    DWORD prot;
    void *tmp;
#endif

    committed = (char *)NtCurrentTeb()->Tib.StackBase - (char *)NtCurrentTeb()->Tib.StackLimit;
    reserved = (char *)NtCurrentTeb()->Tib.StackBase - (char *)NtCurrentTeb()->DeallocationStack;
    todo_wine ok( committed == args->expect_committed || broken(committed == 0x1000), "unexpected stack committed size %x, expected %x\n", committed, args->expect_committed );
    ok( reserved == args->expect_reserved, "unexpected stack reserved size %x, expected %x\n", reserved, args->expect_reserved );

    addr = (char *)NtCurrentTeb()->DeallocationStack;
    status = NtQueryVirtualMemory( NtCurrentProcess(), addr, MemoryBasicInformation, &mbi, sizeof(mbi), &size );
    ok( !status, "NtQueryVirtualMemory returned %08x\n", status );
    ok( mbi.AllocationBase == NtCurrentTeb()->DeallocationStack, "unexpected AllocationBase %p, expected %p\n", mbi.AllocationBase, NtCurrentTeb()->DeallocationStack );
    ok( mbi.AllocationProtect == PAGE_READWRITE, "unexpected AllocationProtect %#x, expected %#x\n", mbi.AllocationProtect, PAGE_READWRITE );
    ok( mbi.BaseAddress == addr, "unexpected BaseAddress %p, expected %p\n", mbi.BaseAddress, addr );
    todo_wine ok( mbi.State == MEM_RESERVE, "unexpected State %#x, expected %#x\n", mbi.State, MEM_RESERVE );
    todo_wine ok( mbi.Protect == 0, "unexpected Protect %#x, expected %#x\n", mbi.Protect, 0 );
    ok( mbi.Type == MEM_PRIVATE, "unexpected Type %#x, expected %#x\n", mbi.Type, MEM_PRIVATE );


    force_stack_grow();

    committed = (char *)NtCurrentTeb()->Tib.StackBase - (char *)NtCurrentTeb()->Tib.StackLimit;
    reserved = (char *)NtCurrentTeb()->Tib.StackBase - (char *)NtCurrentTeb()->DeallocationStack;
    todo_wine ok( committed == 0x9000, "unexpected stack committed size %x, expected 9000\n", committed );
    ok( reserved == args->expect_reserved, "unexpected stack reserved size %x, expected %x\n", reserved, args->expect_reserved );


    /* reserved area shrinks whenever stack grows */

    addr = (char *)NtCurrentTeb()->DeallocationStack;
    status = NtQueryVirtualMemory( NtCurrentProcess(), addr, MemoryBasicInformation, &mbi, sizeof(mbi), &size );
    ok( !status, "NtQueryVirtualMemory returned %08x\n", status );
    ok( mbi.AllocationBase == NtCurrentTeb()->DeallocationStack, "unexpected AllocationBase %p, expected %p\n", mbi.AllocationBase, NtCurrentTeb()->DeallocationStack );
    ok( mbi.AllocationProtect == PAGE_READWRITE, "unexpected AllocationProtect %#x, expected %#x\n", mbi.AllocationProtect, PAGE_READWRITE );
    ok( mbi.BaseAddress == addr, "unexpected BaseAddress %p, expected %p\n", mbi.BaseAddress, addr );
    todo_wine ok( mbi.State == MEM_RESERVE, "unexpected State %#x, expected %#x\n", mbi.State, MEM_RESERVE );
    todo_wine ok( mbi.Protect == 0, "unexpected Protect %#x, expected %#x\n", mbi.Protect, 0 );
    ok( mbi.Type == MEM_PRIVATE, "unexpected Type %#x, expected %#x\n", mbi.Type, MEM_PRIVATE );

    guard_size = reserved - committed - mbi.RegionSize;
    ok( guard_size == 0x1000 || guard_size == 0x2000 || guard_size == 0x3000, "unexpected guard_size %I64x, expected 1000, 2000 or 3000\n", (UINT64)guard_size );

    /* the commit area is initially preceded by guard pages */

    addr = (char *)NtCurrentTeb()->DeallocationStack + mbi.RegionSize;
    status = NtQueryVirtualMemory( NtCurrentProcess(), addr, MemoryBasicInformation, &mbi, sizeof(mbi), &size );
    ok( !status, "NtQueryVirtualMemory returned %08x\n", status );
    ok( mbi.AllocationBase == NtCurrentTeb()->DeallocationStack, "unexpected AllocationBase %p, expected %p\n", mbi.AllocationBase, NtCurrentTeb()->DeallocationStack );
    ok( mbi.AllocationProtect == PAGE_READWRITE, "unexpected AllocationProtect %#x, expected %#x\n", mbi.AllocationProtect, PAGE_READWRITE );
    ok( mbi.BaseAddress == addr, "unexpected BaseAddress %p, expected %p\n", mbi.BaseAddress, addr );
    ok( mbi.RegionSize == guard_size, "unexpected RegionSize %I64x, expected 3000\n", (UINT64)mbi.RegionSize );
    ok( mbi.State == MEM_COMMIT, "unexpected State %#x, expected %#x\n", mbi.State, MEM_COMMIT );
    ok( mbi.Protect == (PAGE_READWRITE|PAGE_GUARD), "unexpected Protect %#x, expected %#x\n", mbi.Protect, PAGE_READWRITE|PAGE_GUARD );
    ok( mbi.Type == MEM_PRIVATE, "unexpected Type %#x, expected %#x\n", mbi.Type, MEM_PRIVATE );

    addr = (char *)NtCurrentTeb()->Tib.StackLimit;
    status = NtQueryVirtualMemory( NtCurrentProcess(), addr, MemoryBasicInformation, &mbi, sizeof(mbi), &size );
    ok( !status, "NtQueryVirtualMemory returned %08x\n", status );
    ok( mbi.AllocationBase == NtCurrentTeb()->DeallocationStack, "unexpected AllocationBase %p, expected %p\n", mbi.AllocationBase, NtCurrentTeb()->DeallocationStack );
    ok( mbi.AllocationProtect == PAGE_READWRITE, "unexpected AllocationProtect %#x, expected %#x\n", mbi.AllocationProtect, PAGE_READWRITE );
    ok( mbi.BaseAddress == addr, "unexpected BaseAddress %p, expected %p\n", mbi.BaseAddress, addr );
    ok( mbi.RegionSize == committed, "unexpected RegionSize %I64x, expected %I64x\n", (UINT64)mbi.RegionSize, (UINT64)committed );
    ok( mbi.State == MEM_COMMIT, "unexpected State %#x, expected %#x\n", mbi.State, MEM_COMMIT );
    ok( mbi.Protect == PAGE_READWRITE, "unexpected Protect %#x, expected %#x\n", mbi.Protect, PAGE_READWRITE );
    ok( mbi.Type == MEM_PRIVATE, "unexpected Type %#x, expected %#x\n", mbi.Type, MEM_PRIVATE );


#ifdef _WIN64
    /* setting a guard page shrinks stack automatically */

    addr = (char *)NtCurrentTeb()->Tib.StackLimit + 0x2000;
    size = 0x1000;
    status = NtAllocateVirtualMemory( NtCurrentProcess(), &addr, 0, &size, MEM_COMMIT, PAGE_READWRITE | PAGE_GUARD );
    ok( !status, "NtAllocateVirtualMemory returned %08x\n", status );

    committed = (char *)NtCurrentTeb()->Tib.StackBase - (char *)NtCurrentTeb()->Tib.StackLimit;
    todo_wine ok( committed == 0x6000, "unexpected stack committed size %x, expected 6000\n", committed );

    status = NtQueryVirtualMemory( NtCurrentProcess(), (char *)addr - 0x2000, MemoryBasicInformation, &mbi, sizeof(mbi), &size );
    ok( !status, "NtQueryVirtualMemory returned %08x\n", status );
    ok( mbi.RegionSize == 0x2000, "unexpected RegionSize %I64x, expected 2000\n", (UINT64)mbi.RegionSize );
    ok( mbi.State == MEM_COMMIT, "unexpected State %#x, expected %#x\n", mbi.State, MEM_COMMIT );
    ok( mbi.Protect == PAGE_READWRITE, "unexpected Protect %#x, expected %#x\n", mbi.Protect, PAGE_READWRITE );

    status = NtQueryVirtualMemory( NtCurrentProcess(), addr, MemoryBasicInformation, &mbi, sizeof(mbi), &size );
    ok( !status, "NtQueryVirtualMemory returned %08x\n", status );
    ok( mbi.RegionSize == 0x1000, "unexpected RegionSize %I64x, expected 1000\n", (UINT64)mbi.RegionSize );
    ok( mbi.State == MEM_COMMIT, "unexpected State %#x, expected %#x\n", mbi.State, MEM_COMMIT );
    ok( mbi.Protect == (PAGE_READWRITE|PAGE_GUARD), "unexpected Protect %#x, expected %#x\n", mbi.Protect, (PAGE_READWRITE|PAGE_GUARD) );

    addr = (char *)NtCurrentTeb()->Tib.StackLimit;
    status = NtQueryVirtualMemory( NtCurrentProcess(), addr, MemoryBasicInformation, &mbi, sizeof(mbi), &size );
    ok( !status, "NtQueryVirtualMemory returned %08x\n", status );
    todo_wine ok( mbi.RegionSize == 0x6000, "unexpected RegionSize %I64x, expected 6000\n", (UINT64)mbi.RegionSize );
    ok( mbi.State == MEM_COMMIT, "unexpected State %#x, expected %#x\n", mbi.State, MEM_COMMIT );
    ok( mbi.Protect == PAGE_READWRITE, "unexpected Protect %#x, expected %#x\n", mbi.Protect, PAGE_READWRITE );


    /* guard pages are restored as the stack grows back */

    addr = (char *)NtCurrentTeb()->Tib.StackLimit + 0x4000;
    tmp = (char *)addr - guard_size - 0x1000;
    size = 0x1000;
    status = NtAllocateVirtualMemory( NtCurrentProcess(), &addr, 0, &size, MEM_COMMIT, PAGE_READWRITE | PAGE_GUARD );
    ok( !status, "NtAllocateVirtualMemory returned %08x\n", status );

    committed = (char *)NtCurrentTeb()->Tib.StackBase - (char *)NtCurrentTeb()->Tib.StackLimit;
    todo_wine ok( committed == 0x1000, "unexpected stack committed size %x, expected 1000\n", committed );

    status = NtQueryVirtualMemory( NtCurrentProcess(), tmp, MemoryBasicInformation, &mbi, sizeof(mbi), &size );
    ok( !status, "NtQueryVirtualMemory returned %08x\n", status );
    todo_wine ok( mbi.RegionSize == guard_size + 0x1000, "unexpected RegionSize %I64x, expected %I64x\n", (UINT64)mbi.RegionSize, (UINT64)(guard_size + 0x1000) );
    ok( mbi.State == MEM_COMMIT, "unexpected State %#x, expected %#x\n", mbi.State, MEM_COMMIT );
    todo_wine ok( mbi.Protect == PAGE_READWRITE, "unexpected Protect %#x, expected %#x\n", mbi.Protect, PAGE_READWRITE );

    force_stack_grow_small();

    committed = (char *)NtCurrentTeb()->Tib.StackBase - (char *)NtCurrentTeb()->Tib.StackLimit;
    todo_wine ok( committed == 0x2000, "unexpected stack committed size %x, expected 2000\n", committed );

    status = NtQueryVirtualMemory( NtCurrentProcess(), tmp, MemoryBasicInformation, &mbi, sizeof(mbi), &size );
    ok( !status, "NtQueryVirtualMemory returned %08x\n", status );
    ok( mbi.RegionSize == 0x1000, "unexpected RegionSize %I64x, expected 1000\n", (UINT64)mbi.RegionSize );
    ok( mbi.State == MEM_COMMIT, "unexpected State %#x, expected %#x\n", mbi.State, MEM_COMMIT );
    todo_wine ok( mbi.Protect == PAGE_READWRITE, "unexpected Protect %#x, expected %#x\n", mbi.Protect, PAGE_READWRITE );

    status = NtQueryVirtualMemory( NtCurrentProcess(), (char *)tmp + 0x1000, MemoryBasicInformation, &mbi, sizeof(mbi), &size );
    ok( !status, "NtQueryVirtualMemory returned %08x\n", status );
    ok( mbi.RegionSize == guard_size, "unexpected RegionSize %I64x, expected %I64x\n", (UINT64)mbi.RegionSize, (UINT64)guard_size );
    ok( mbi.State == MEM_COMMIT, "unexpected State %#x, expected %#x\n", mbi.State, MEM_COMMIT );
    todo_wine ok( mbi.Protect == (PAGE_READWRITE|PAGE_GUARD), "unexpected Protect %#x, expected %#x\n", mbi.Protect, (PAGE_READWRITE|PAGE_GUARD) );


    /* forcing stack limit over guard pages still shrinks the stack on page fault */

    addr = (char *)tmp + guard_size + 0x1000;
    size = 0x1000;
    status = NtAllocateVirtualMemory( NtCurrentProcess(), &addr, 0, &size, MEM_COMMIT, PAGE_READWRITE | PAGE_GUARD );
    ok( !status, "NtAllocateVirtualMemory returned %08x\n", status );

    NtCurrentTeb()->Tib.StackLimit = (char *)tmp;

    status = NtQueryVirtualMemory( NtCurrentProcess(), (char *)tmp + 0x1000, MemoryBasicInformation, &mbi, sizeof(mbi), &size );
    ok( !status, "NtQueryVirtualMemory returned %08x\n", status );
    todo_wine ok( mbi.RegionSize == guard_size + 0x1000, "unexpected RegionSize %I64x, expected %I64x\n", (UINT64)mbi.RegionSize, (UINT64)(guard_size + 0x1000) );
    ok( mbi.State == MEM_COMMIT, "unexpected State %#x, expected %#x\n", mbi.State, MEM_COMMIT );
    todo_wine ok( mbi.Protect == (PAGE_READWRITE|PAGE_GUARD), "unexpected Protect %#x, expected %#x\n", mbi.Protect, (PAGE_READWRITE|PAGE_GUARD) );

    force_stack_grow_small();

    committed = (char *)NtCurrentTeb()->Tib.StackBase - (char *)NtCurrentTeb()->Tib.StackLimit;
    todo_wine ok( committed == 0x2000, "unexpected stack committed size %x, expected 2000\n", committed );


    /* it works with NtProtectVirtualMemory as well */

    force_stack_grow();

    addr = (char *)NtCurrentTeb()->Tib.StackLimit + 0x2000;
    size = 0x1000;
    status = NtProtectVirtualMemory( NtCurrentProcess(), &addr, &size, PAGE_READWRITE | PAGE_GUARD, &prot );
    ok( !status, "NtProtectVirtualMemory returned %08x\n", status );
    todo_wine ok( prot == PAGE_READWRITE, "unexpected prot %#x, expected %#x\n", prot, PAGE_READWRITE );

    committed = (char *)NtCurrentTeb()->Tib.StackBase - (char *)NtCurrentTeb()->Tib.StackLimit;
    todo_wine ok( committed == 0x6000, "unexpected stack committed size %x, expected 6000\n", committed );

    status = NtQueryVirtualMemory( NtCurrentProcess(), (char *)addr - 0x2000, MemoryBasicInformation, &mbi, sizeof(mbi), &size );
    ok( !status, "NtQueryVirtualMemory returned %08x\n", status );
    todo_wine ok( mbi.RegionSize == 0x2000, "unexpected RegionSize %I64x, expected 2000\n", (UINT64)mbi.RegionSize );
    ok( mbi.State == MEM_COMMIT, "unexpected State %#x, expected %#x\n", mbi.State, MEM_COMMIT );
    todo_wine ok( mbi.Protect == PAGE_READWRITE, "unexpected Protect %#x, expected %#x\n", mbi.Protect, PAGE_READWRITE );

    status = NtQueryVirtualMemory( NtCurrentProcess(), addr, MemoryBasicInformation, &mbi, sizeof(mbi), &size );
    ok( !status, "NtQueryVirtualMemory returned %08x\n", status );
    ok( mbi.RegionSize == 0x1000, "unexpected RegionSize %I64x, expected 1000\n", (UINT64)mbi.RegionSize );
    ok( mbi.State == MEM_COMMIT, "unexpected State %#x, expected %#x\n", mbi.State, MEM_COMMIT );
    ok( mbi.Protect == (PAGE_READWRITE|PAGE_GUARD), "unexpected Protect %#x, expected %#x\n", mbi.Protect, (PAGE_READWRITE|PAGE_GUARD) );

    addr = (char *)NtCurrentTeb()->Tib.StackLimit;
    status = NtQueryVirtualMemory( NtCurrentProcess(), addr, MemoryBasicInformation, &mbi, sizeof(mbi), &size );
    ok( !status, "NtQueryVirtualMemory returned %08x\n", status );
    todo_wine ok( mbi.RegionSize == 0x6000, "unexpected RegionSize %I64x, expected 6000\n", (UINT64)mbi.RegionSize );
    ok( mbi.State == MEM_COMMIT, "unexpected State %#x, expected %#x\n", mbi.State, MEM_COMMIT );
    todo_wine ok( mbi.Protect == PAGE_READWRITE, "unexpected Protect %#x, expected %#x\n", mbi.Protect, PAGE_READWRITE );


    /* clearing the guard pages doesn't change StackLimit back */

    force_stack_grow();

    addr = (char *)NtCurrentTeb()->Tib.StackLimit + 0x2000;
    size = 0x1000;
    status = NtProtectVirtualMemory( NtCurrentProcess(), &addr, &size, PAGE_READWRITE | PAGE_GUARD, &prot );
    ok( !status, "NtProtectVirtualMemory returned %08x\n", status );
    todo_wine ok( prot == PAGE_READWRITE, "unexpected prot %#x, expected %#x\n", prot, PAGE_READWRITE );

    committed = (char *)NtCurrentTeb()->Tib.StackBase - (char *)NtCurrentTeb()->Tib.StackLimit;
    todo_wine ok( committed == 0x6000, "unexpected stack committed size %x, expected 6000\n", committed );

    status = NtProtectVirtualMemory( NtCurrentProcess(), &addr, &size, PAGE_READWRITE, &prot );
    ok( !status, "NtProtectVirtualMemory returned %08x\n", status );
    ok( prot == (PAGE_READWRITE | PAGE_GUARD), "unexpected prot %#x, expected %#x\n", prot, (PAGE_READWRITE | PAGE_GUARD) );

    committed = (char *)NtCurrentTeb()->Tib.StackBase - (char *)NtCurrentTeb()->Tib.StackLimit;
    todo_wine ok( committed == 0x6000, "unexpected stack committed size %x, expected 6000\n", committed );

    /* and as we messed with it and it now doesn't fault, it doesn't grow back either */

    force_stack_grow();

    committed = (char *)NtCurrentTeb()->Tib.StackBase - (char *)NtCurrentTeb()->Tib.StackLimit;
    todo_wine ok( committed == 0x6000, "unexpected stack committed size %x, expected 6000\n", committed );
#endif

    ExitThread(0);
}

static DWORD WINAPI test_stack_size_dummy_thread(void *ptr)
{
    return 0;
}

static void test_RtlCreateUserStack(void)
{
    IMAGE_NT_HEADERS *nt = RtlImageNtHeader( NtCurrentTeb()->Peb->ImageBaseAddress );
    struct test_stack_size_thread_args args;
    SIZE_T default_commit = nt->OptionalHeader.SizeOfStackCommit;
    SIZE_T default_reserve = nt->OptionalHeader.SizeOfStackReserve;
    INITIAL_TEB stack = {0};
    unsigned int i;
    NTSTATUS ret;
    HANDLE thread;
    CLIENT_ID id;

    struct
    {
        SIZE_T commit, reserve, commit_align, reserve_align, expect_commit, expect_reserve;
    }
    tests[] =
    {
        {       0,        0,      1,        1, default_commit, default_reserve},
        {  0x2000,        0,      1,        1,         0x2000, default_reserve},
        {  0x4000,        0,      1,        1,         0x4000, default_reserve},
        {       0, 0x200000,      1,        1, default_commit, 0x200000},
        {  0x4000, 0x200000,      1,        1,         0x4000, 0x200000},
        {0x100000, 0x100000,      1,        1,       0x100000, 0x100000},
        { 0x20000,  0x20000,      1,        1,        0x20000, 0x100000},

        {       0, 0x110000,      1,        1, default_commit, 0x110000},
        {       0, 0x110000,      1,  0x40000, default_commit, 0x140000},
        {       0, 0x140000,      1,  0x40000, default_commit, 0x140000},
        { 0x11000, 0x140000,      1,  0x40000,        0x11000, 0x140000},
        { 0x11000, 0x140000, 0x4000,  0x40000,        0x14000, 0x140000},
        {       0,        0, 0x4000, 0x400000,
                (default_commit + 0x3fff) & ~0x3fff,
                (default_reserve + 0x3fffff) & ~0x3fffff},
    };

    if (!pRtlCreateUserStack)
    {
        win_skip("RtlCreateUserStack() is missing\n");
        return;
    }

    for (i = 0; i < ARRAY_SIZE(tests); ++i)
    {
        memset(&stack, 0xcc, sizeof(stack));
        ret = pRtlCreateUserStack(tests[i].commit, tests[i].reserve, 0,
                tests[i].commit_align, tests[i].reserve_align, &stack);
        ok(!ret, "%u: got status %#x\n", i, ret);
        ok(!stack.OldStackBase, "%u: got OldStackBase %p\n", i, stack.OldStackBase);
        ok(!stack.OldStackLimit, "%u: got OldStackLimit %p\n", i, stack.OldStackLimit);
        ok(!((ULONG_PTR)stack.DeallocationStack & (page_size - 1)),
                "%u: got unaligned memory %p\n", i, stack.DeallocationStack);
        ok((ULONG_PTR)stack.StackBase - (ULONG_PTR)stack.DeallocationStack == tests[i].expect_reserve,
                "%u: got reserve %#lx\n", i, (ULONG_PTR)stack.StackBase - (ULONG_PTR)stack.DeallocationStack);
        todo_wine ok((ULONG_PTR)stack.StackBase - (ULONG_PTR)stack.StackLimit == tests[i].expect_commit,
                "%u: got commit %#lx\n", i, (ULONG_PTR)stack.StackBase - (ULONG_PTR)stack.StackLimit);
        pRtlFreeUserStack(stack.DeallocationStack);
    }

    ret = pRtlCreateUserStack(0x11000, 0x110000, 0, 1, 0, &stack);
    ok(ret == STATUS_INVALID_PARAMETER, "got %#x\n", ret);

    ret = pRtlCreateUserStack(0x11000, 0x110000, 0, 0, 1, &stack);
    ok(ret == STATUS_INVALID_PARAMETER, "got %#x\n", ret);

    args.expect_committed = 0x4000;
    args.expect_reserved = default_reserve;
    thread = CreateThread(NULL, 0x3f00, test_stack_size_thread, &args, 0, NULL);
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);

    args.expect_committed = default_commit < 0x2000 ? 0x2000 : default_commit;
    args.expect_reserved = 0x400000;
    thread = CreateThread(NULL, 0x3ff000, test_stack_size_thread, &args, STACK_SIZE_PARAM_IS_A_RESERVATION, NULL);
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);

    if (is_win64)
    {
        thread = CreateThread(NULL, 0x80000000, test_stack_size_dummy_thread, NULL, STACK_SIZE_PARAM_IS_A_RESERVATION, NULL);
        ok(thread != NULL, "CreateThread with huge stack failed\n");
        WaitForSingleObject(thread, INFINITE);
        CloseHandle(thread);
    }

    args.expect_committed = default_commit < 0x2000 ? 0x2000 : default_commit;
    args.expect_reserved = 0x100000;
    for (i = 0; i < 32; i++)
    {
        ULONG mask = ~0u >> i;
        NTSTATUS expect_ret = STATUS_SUCCESS;

        if (i == 12) expect_ret = STATUS_CONFLICTING_ADDRESSES;
        else if (i >= 13) expect_ret = STATUS_INVALID_PARAMETER;
        ret = pRtlCreateUserStack( args.expect_committed, args.expect_reserved, i, 0x1000, 0x1000, &stack );
        ok( ret == expect_ret || ret == STATUS_NO_MEMORY ||
            (ret == STATUS_INVALID_PARAMETER_3 && expect_ret == STATUS_INVALID_PARAMETER) ||
            broken( i == 1 && ret == STATUS_INVALID_PARAMETER_3 ), /* win7 */
            "%u: got %x / %x\n", i, ret, expect_ret );
        if (!ret) pRtlFreeUserStack( stack.DeallocationStack );
        ret = pRtlCreateUserThread( GetCurrentProcess(), NULL, FALSE, i,
                                    args.expect_reserved, args.expect_committed,
                                    (void *)test_stack_size_thread, &args, &thread, &id );
        ok( ret == expect_ret || ret == STATUS_NO_MEMORY ||
            (ret == STATUS_INVALID_PARAMETER_3 && expect_ret == STATUS_INVALID_PARAMETER) ||
            broken( i == 1 && ret == STATUS_INVALID_PARAMETER_3 ), /* win7 */
            "%u: got %x / %x\n", i, ret, expect_ret );
        if (!ret)
        {
            WaitForSingleObject( thread, INFINITE );
            CloseHandle( thread );
        }

        if (mask <= 31) continue;
        if (!is_win64 && !is_wow64) expect_ret = STATUS_INVALID_PARAMETER_3;
        ret = pRtlCreateUserStack( args.expect_committed, args.expect_reserved, mask, 0x1000, 0x1000, &stack );
        ok( ret == expect_ret || ret == STATUS_NO_MEMORY ||
            (ret == STATUS_INVALID_PARAMETER_3 && expect_ret == STATUS_INVALID_PARAMETER),
            "%08x: got %x / %x\n", mask, ret, expect_ret );
        if (!ret) pRtlFreeUserStack( stack.DeallocationStack );
        ret = pRtlCreateUserThread( GetCurrentProcess(), NULL, FALSE, mask,
                                    args.expect_reserved, args.expect_committed,
                                    (void *)test_stack_size_thread, &args, &thread, &id );
        ok( ret == expect_ret || ret == STATUS_NO_MEMORY ||
            (ret == STATUS_INVALID_PARAMETER_3 && expect_ret == STATUS_INVALID_PARAMETER),
            "%08x: got %x / %x\n", mask, ret, expect_ret );
        if (!ret)
        {
            WaitForSingleObject( thread, INFINITE );
            CloseHandle( thread );
        }
    }
}

static void test_NtMapViewOfSection(void)
{
    static const char testfile[] = "testfile.xxx";
    static const char data[] = "test data for NtMapViewOfSection";
    char buffer[sizeof(data)];
    HANDLE file, mapping, process;
    void *ptr, *ptr2;
    BOOL ret;
    DWORD status, written;
    SIZE_T size, result;
    LARGE_INTEGER offset;
    ULONG_PTR zero_bits;

    if (!pIsWow64Process || !pIsWow64Process(NtCurrentProcess(), &is_wow64)) is_wow64 = FALSE;

    file = CreateFileA(testfile, GENERIC_READ|GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, 0);
    ok(file != INVALID_HANDLE_VALUE, "Failed to create test file\n");
    WriteFile(file, data, sizeof(data), &written, NULL);
    SetFilePointer(file, 4096, NULL, FILE_BEGIN);
    SetEndOfFile(file);

    /* read/write mapping */

    mapping = CreateFileMappingA(file, NULL, PAGE_READWRITE, 0, 4096, NULL);
    ok(mapping != 0, "CreateFileMapping failed\n");

    process = create_target_process("sleep");
    ok(process != NULL, "Can't start process\n");

    ptr = NULL;
    size = 0;
    offset.QuadPart = 0;
    status = NtMapViewOfSection(mapping, process, &ptr, 0, 0, &offset, &size, 1, 0, PAGE_READWRITE);
    ok(status == STATUS_SUCCESS, "NtMapViewOfSection returned %08x\n", status);
    ok(!((ULONG_PTR)ptr & 0xffff), "returned memory %p is not aligned to 64k\n", ptr);

    ret = ReadProcessMemory(process, ptr, buffer, sizeof(buffer), &result);
    ok(ret, "ReadProcessMemory failed\n");
    ok(result == sizeof(buffer), "ReadProcessMemory didn't read all data (%lx)\n", result);
    ok(!memcmp(buffer, data, sizeof(buffer)), "Wrong data read\n");

    /* 1 zero bits should zero 63-31 upper bits */
    ptr2 = NULL;
    size = 0;
    zero_bits = 1;
    offset.QuadPart = 0;
    status = NtMapViewOfSection(mapping, process, &ptr2, zero_bits, 0, &offset, &size, 1, MEM_TOP_DOWN, PAGE_READWRITE);
    ok(status == STATUS_SUCCESS || status == STATUS_NO_MEMORY,
       "NtMapViewOfSection returned %08x\n", status);
    if (status == STATUS_SUCCESS)
    {
        ok(((UINT_PTR)ptr2 >> (32 - zero_bits)) == 0,
           "NtMapViewOfSection returned address: %p\n", ptr2);

        status = NtUnmapViewOfSection(process, ptr2);
        ok(status == STATUS_SUCCESS, "NtUnmapViewOfSection returned %08x\n", status);
    }

    for (zero_bits = 2; zero_bits <= 20; zero_bits++)
    {
        ptr2 = NULL;
        size = 0;
        offset.QuadPart = 0;
        status = NtMapViewOfSection(mapping, process, &ptr2, zero_bits, 0, &offset, &size, 1, MEM_TOP_DOWN, PAGE_READWRITE);
        ok(status == STATUS_SUCCESS || status == STATUS_NO_MEMORY,
           "NtMapViewOfSection with %d zero_bits returned %08x\n", (int)zero_bits, status);
        if (status == STATUS_SUCCESS)
        {
            ok(((UINT_PTR)ptr2 >> (32 - zero_bits)) == 0,
               "NtMapViewOfSection with %d zero_bits returned address %p\n", (int)zero_bits, ptr2);

            status = NtUnmapViewOfSection(process, ptr2);
            ok(status == STATUS_SUCCESS, "NtUnmapViewOfSection returned %08x\n", status);
        }
    }

    /* 21 zero bits never succeeds */
    ptr2 = NULL;
    size = 0;
    offset.QuadPart = 0;
    status = NtMapViewOfSection(mapping, process, &ptr2, 21, 0, &offset, &size, 1, 0, PAGE_READWRITE);
    ok(status == STATUS_NO_MEMORY || status == STATUS_INVALID_PARAMETER,
       "NtMapViewOfSection returned %08x\n", status);

    /* 22 zero bits is invalid */
    ptr2 = NULL;
    size = 0;
    offset.QuadPart = 0;
    status = NtMapViewOfSection(mapping, process, &ptr2, 22, 0, &offset, &size, 1, 0, PAGE_READWRITE);
    ok(status == STATUS_INVALID_PARAMETER_4 || status == STATUS_INVALID_PARAMETER,
       "NtMapViewOfSection returned %08x\n", status);

    /* zero bits > 31 should be considered as a leading zeroes bitmask on 64bit and WoW64 */
    ptr2 = NULL;
    size = 0;
    zero_bits = 0x1aaaaaaa;
    offset.QuadPart = 0;
    status = NtMapViewOfSection(mapping, process, &ptr2, zero_bits, 0, &offset, &size, 1, MEM_TOP_DOWN, PAGE_READWRITE);

    if (!is_win64 && !is_wow64)
    {
        ok(status == STATUS_INVALID_PARAMETER_4, "NtMapViewOfSection returned %08x\n", status);
    }
    else
    {
        ok(status == STATUS_SUCCESS || status == STATUS_NO_MEMORY,
           "NtMapViewOfSection returned %08x\n", status);
        if (status == STATUS_SUCCESS)
        {
            ok(((UINT_PTR)ptr2 & ~get_zero_bits_mask(zero_bits)) == 0 &&
               ((UINT_PTR)ptr2 & ~zero_bits) != 0, /* only the leading zeroes matter */
               "NtMapViewOfSection returned address %p\n", ptr2);

            status = NtUnmapViewOfSection(process, ptr2);
            ok(status == STATUS_SUCCESS, "NtUnmapViewOfSection returned %08x\n", status);
        }
    }

    /* mapping at the same page conflicts */
    ptr2 = ptr;
    size = 0;
    offset.QuadPart = 0;
    status = NtMapViewOfSection(mapping, process, &ptr2, 0, 0, &offset, &size, 1, 0, PAGE_READWRITE);
    ok(status == STATUS_CONFLICTING_ADDRESSES, "NtMapViewOfSection returned %08x\n", status);

    /* offset has to be aligned */
    ptr2 = ptr;
    size = 0;
    offset.QuadPart = 1;
    status = NtMapViewOfSection(mapping, process, &ptr2, 0, 0, &offset, &size, 1, 0, PAGE_READWRITE);
    ok(status == STATUS_MAPPED_ALIGNMENT, "NtMapViewOfSection returned %08x\n", status);

    /* ptr has to be aligned */
    ptr2 = (char *)ptr + 42;
    size = 0;
    offset.QuadPart = 0;
    status = NtMapViewOfSection(mapping, process, &ptr2, 0, 0, &offset, &size, 1, 0, PAGE_READWRITE);
    ok(status == STATUS_MAPPED_ALIGNMENT, "NtMapViewOfSection returned %08x\n", status);

    /* still not 64k aligned */
    ptr2 = (char *)ptr + 0x1000;
    size = 0;
    offset.QuadPart = 0;
    status = NtMapViewOfSection(mapping, process, &ptr2, 0, 0, &offset, &size, 1, 0, PAGE_READWRITE);
    ok(status == STATUS_MAPPED_ALIGNMENT, "NtMapViewOfSection returned %08x\n", status);

    /* when an address is passed, it has to satisfy the provided number of zero bits */
    ptr2 = (char *)ptr + 0x1000;
    size = 0;
    offset.QuadPart = 0;
    zero_bits = get_zero_bits(((UINT_PTR)ptr2) >> 1);
    status = NtMapViewOfSection(mapping, process, &ptr2, zero_bits, 0, &offset, &size, 1, 0, PAGE_READWRITE);
    ok(status == STATUS_INVALID_PARAMETER_4 || status == STATUS_INVALID_PARAMETER,
       "NtMapViewOfSection returned %08x\n", status);

    ptr2 = (char *)ptr + 0x1000;
    size = 0;
    offset.QuadPart = 0;
    zero_bits = get_zero_bits((UINT_PTR)ptr2);
    status = NtMapViewOfSection(mapping, process, &ptr2, zero_bits, 0, &offset, &size, 1, 0, PAGE_READWRITE);
    ok(status == STATUS_MAPPED_ALIGNMENT, "NtMapViewOfSection returned %08x\n", status);

    if (!is_win64 && !is_wow64)
    {
        /* new memory region conflicts with previous mapping */
        ptr2 = ptr;
        size = 0;
        offset.QuadPart = 0;
        status = NtMapViewOfSection(mapping, process, &ptr2, 0, 0, &offset,
                                    &size, 1, AT_ROUND_TO_PAGE, PAGE_READWRITE);
        ok(status == STATUS_CONFLICTING_ADDRESSES, "NtMapViewOfSection returned %08x\n", status);

        ptr2 = (char *)ptr + 42;
        size = 0;
        offset.QuadPart = 0;
        status = NtMapViewOfSection(mapping, process, &ptr2, 0, 0, &offset,
                                    &size, 1, AT_ROUND_TO_PAGE, PAGE_READWRITE);
        ok(status == STATUS_CONFLICTING_ADDRESSES, "NtMapViewOfSection returned %08x\n", status);

        /* in contrary to regular NtMapViewOfSection, only 4kb align is enforced */
        ptr2 = (char *)ptr + 0x1000;
        size = 0;
        offset.QuadPart = 0;
        status = NtMapViewOfSection(mapping, process, &ptr2, 0, 0, &offset,
                                    &size, 1, AT_ROUND_TO_PAGE, PAGE_READWRITE);
        ok(status == STATUS_SUCCESS, "NtMapViewOfSection returned %08x\n", status);
        ok((char *)ptr2 == (char *)ptr + 0x1000,
           "expected address %p, got %p\n", (char *)ptr + 0x1000, ptr2);
        status = NtUnmapViewOfSection(process, ptr2);
        ok(status == STATUS_SUCCESS, "NtUnmapViewOfSection returned %08x\n", status);

        /* the address is rounded down if not on a page boundary */
        ptr2 = (char *)ptr + 0x1001;
        size = 0;
        offset.QuadPart = 0;
        status = NtMapViewOfSection(mapping, process, &ptr2, 0, 0, &offset,
                                    &size, 1, AT_ROUND_TO_PAGE, PAGE_READWRITE);
        ok(status == STATUS_SUCCESS, "NtMapViewOfSection returned %08x\n", status);
        ok((char *)ptr2 == (char *)ptr + 0x1000,
           "expected address %p, got %p\n", (char *)ptr + 0x1000, ptr2);
        status = NtUnmapViewOfSection(process, ptr2);
        ok(status == STATUS_SUCCESS, "NtUnmapViewOfSection returned %08x\n", status);

        ptr2 = (char *)ptr + 0x2000;
        size = 0;
        offset.QuadPart = 0;
        status = NtMapViewOfSection(mapping, process, &ptr2, 0, 0, &offset,
                                    &size, 1, AT_ROUND_TO_PAGE, PAGE_READWRITE);
        ok(status == STATUS_SUCCESS, "NtMapViewOfSection returned %08x\n", status);
        ok((char *)ptr2 == (char *)ptr + 0x2000,
           "expected address %p, got %p\n", (char *)ptr + 0x2000, ptr2);
        status = NtUnmapViewOfSection(process, ptr2);
        ok(status == STATUS_SUCCESS, "NtUnmapViewOfSection returned %08x\n", status);
    }
    else
    {
        ptr2 = (char *)ptr + 0x1000;
        size = 0;
        offset.QuadPart = 0;
        status = NtMapViewOfSection(mapping, process, &ptr2, 0, 0, &offset,
                                    &size, 1, AT_ROUND_TO_PAGE, PAGE_READWRITE);
        todo_wine
        ok(status == STATUS_INVALID_PARAMETER_9 || status == STATUS_INVALID_PARAMETER,
           "NtMapViewOfSection returned %08x\n", status);
    }

    status = NtUnmapViewOfSection(process, ptr);
    ok(status == STATUS_SUCCESS, "NtUnmapViewOfSection returned %08x\n", status);

    NtClose(mapping);

    CloseHandle(file);
    DeleteFileA(testfile);

    TerminateProcess(process, 0);
    CloseHandle(process);
}

#define SUPPORTED_XSTATE_FEATURES ((1 << XSTATE_LEGACY_FLOATING_POINT) | (1 << XSTATE_LEGACY_SSE) | (1 << XSTATE_AVX))

static void test_user_shared_data(void)
{
    struct old_xstate_configuration
    {
        ULONG64 EnabledFeatures;
        ULONG Size;
        ULONG OptimizedSave:1;
        ULONG CompactionEnabled:1;
        XSTATE_FEATURE Features[MAXIMUM_XSTATE_FEATURES];
    };

    static const ULONG feature_offsets[] =
    {
            0,
            160, /*offsetof(XMM_SAVE_AREA32, XmmRegisters)*/
            512  /* sizeof(XMM_SAVE_AREA32) */ + offsetof(XSTATE, YmmContext),
    };
    static const ULONG feature_sizes[] =
    {
            160,
            256, /*sizeof(M128A) * 16 */
            sizeof(YMMCONTEXT),
    };
    const KSHARED_USER_DATA *user_shared_data = (void *)0x7ffe0000;
    XSTATE_CONFIGURATION xstate = user_shared_data->XState;
    ULONG64 feature_mask;
    unsigned int i;

    ok(user_shared_data->NumberOfPhysicalPages == sbi.MmNumberOfPhysicalPages,
            "Got number of physical pages %#x, expected %#x.\n",
            user_shared_data->NumberOfPhysicalPages, sbi.MmNumberOfPhysicalPages);

#if defined(__i386__) || defined(__x86_64__)
    ok(user_shared_data->ProcessorFeatures[PF_RDTSC_INSTRUCTION_AVAILABLE] /* Supported since Pentium CPUs. */,
            "_RDTSC not available.\n");
#endif
    ok(user_shared_data->ActiveProcessorCount == NtCurrentTeb()->Peb->NumberOfProcessors
            || broken(!user_shared_data->ActiveProcessorCount) /* before Win7 */,
            "Got unexpected ActiveProcessorCount %u.\n", user_shared_data->ActiveProcessorCount);
    ok(user_shared_data->ActiveGroupCount == 1
            || broken(!user_shared_data->ActiveGroupCount) /* before Win7 */,
            "Got unexpected ActiveGroupCount %u.\n", user_shared_data->ActiveGroupCount);

    if (!pRtlGetEnabledExtendedFeatures)
    {
        skip("RtlGetEnabledExtendedFeatures is not available.\n");
        return;
    }

    feature_mask = pRtlGetEnabledExtendedFeatures(~(ULONG64)0);
    if (!feature_mask)
    {
        skip("XState features are not available.\n");
        return;
    }

    if (!xstate.EnabledFeatures)
    {
        struct old_xstate_configuration *xs_old
                = (struct old_xstate_configuration *)((char *)user_shared_data + 0x3e0);

        ok(feature_mask == xs_old->EnabledFeatures, "Got unexpected xs_old->EnabledFeatures %s.\n",
                wine_dbgstr_longlong(xs_old->EnabledFeatures));
        win_skip("Old structure layout.\n");
        return;
    }

    trace("XState EnabledFeatures %s.\n", wine_dbgstr_longlong(xstate.EnabledFeatures));
    feature_mask = pRtlGetEnabledExtendedFeatures(0);
    ok(!feature_mask, "Got unexpected feature_mask %s.\n", wine_dbgstr_longlong(feature_mask));
    feature_mask = pRtlGetEnabledExtendedFeatures(~(ULONG64)0);
    ok(feature_mask == xstate.EnabledFeatures, "Got unexpected feature_mask %s.\n",
            wine_dbgstr_longlong(feature_mask));
    feature_mask = pGetEnabledXStateFeatures();
    ok(feature_mask == xstate.EnabledFeatures, "Got unexpected feature_mask %s.\n",
            wine_dbgstr_longlong(feature_mask));
    ok((xstate.EnabledFeatures & SUPPORTED_XSTATE_FEATURES) == SUPPORTED_XSTATE_FEATURES,
            "Got unexpected EnabledFeatures %s.\n", wine_dbgstr_longlong(xstate.EnabledFeatures));
    ok((xstate.EnabledVolatileFeatures & SUPPORTED_XSTATE_FEATURES) == xstate.EnabledFeatures,
            "Got unexpected EnabledVolatileFeatures %s.\n", wine_dbgstr_longlong(xstate.EnabledVolatileFeatures));
    ok(xstate.Size >= 512 + sizeof(XSTATE), "Got unexpected Size %u.\n", xstate.Size);
    if (xstate.CompactionEnabled)
        ok(xstate.OptimizedSave, "Got zero OptimizedSave with compaction enabled.\n");
    ok(!xstate.AlignedFeatures, "Got unexpected AlignedFeatures %s.\n",
            wine_dbgstr_longlong(xstate.AlignedFeatures));
    ok(xstate.AllFeatureSize >= 512 + sizeof(XSTATE)
            || !xstate.AllFeatureSize /* win8 on CPUs without XSAVEC */,
            "Got unexpected AllFeatureSize %u.\n", xstate.AllFeatureSize);

    for (i = 0; i < ARRAY_SIZE(feature_sizes); ++i)
    {
        ok(xstate.AllFeatures[i] == feature_sizes[i]
                || !xstate.AllFeatures[i] /* win8+ on CPUs without XSAVEC */,
                "Got unexpected AllFeatures[%u] %u, expected %u.\n", i,
                xstate.AllFeatures[i], feature_sizes[i]);
        ok(xstate.Features[i].Size == feature_sizes[i], "Got unexpected Features[%u].Size %u, expected %u.\n", i,
                xstate.Features[i].Size, feature_sizes[i]);
        ok(xstate.Features[i].Offset == feature_offsets[i], "Got unexpected Features[%u].Offset %u, expected %u.\n",
                i, xstate.Features[i].Offset, feature_offsets[i]);
    }
}

static void perform_relocations( void *module, INT_PTR delta )
{
    IMAGE_NT_HEADERS *nt;
    IMAGE_BASE_RELOCATION *rel, *end;
    const IMAGE_DATA_DIRECTORY *relocs;
    const IMAGE_SECTION_HEADER *sec;
    ULONG protect_old[96], i;

    nt = RtlImageNtHeader( module );
    relocs = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    if (!relocs->VirtualAddress || !relocs->Size) return;
    sec = (const IMAGE_SECTION_HEADER *)((const char *)&nt->OptionalHeader +
                                         nt->FileHeader.SizeOfOptionalHeader);
    for (i = 0; i < nt->FileHeader.NumberOfSections; i++)
    {
        void *addr = (char *)module + sec[i].VirtualAddress;
        SIZE_T size = sec[i].SizeOfRawData;
        NtProtectVirtualMemory( NtCurrentProcess(), &addr,
                                &size, PAGE_READWRITE, &protect_old[i] );
    }
    rel = (IMAGE_BASE_RELOCATION *)((char *)module + relocs->VirtualAddress);
    end = (IMAGE_BASE_RELOCATION *)((char *)rel + relocs->Size);
    while (rel && rel < end - 1 && rel->SizeOfBlock)
        rel = LdrProcessRelocationBlock( (char *)module + rel->VirtualAddress,
                                         (rel->SizeOfBlock - sizeof(*rel)) / sizeof(USHORT),
                                         (USHORT *)(rel + 1), delta );
    for (i = 0; i < nt->FileHeader.NumberOfSections; i++)
    {
        void *addr = (char *)module + sec[i].VirtualAddress;
        SIZE_T size = sec[i].SizeOfRawData;
        NtProtectVirtualMemory( NtCurrentProcess(), &addr,
                                &size, protect_old[i], &protect_old[i] );
    }
}


static void test_syscalls(void)
{
    HMODULE module = GetModuleHandleW( L"ntdll.dll" );
    HANDLE handle;
    NTSTATUS status;
    NTSTATUS (WINAPI *pNtClose)(HANDLE);
    WCHAR path[MAX_PATH];
    HANDLE file, mapping;
    INT_PTR delta;
    void *ptr;

    /* initial image */
    pNtClose = (void *)GetProcAddress( module, "NtClose" );
    handle = CreateEventW( NULL, FALSE, FALSE, NULL );
    ok( handle != 0, "CreateEventWfailed %u\n", GetLastError() );
    status = pNtClose( handle );
    ok( !status, "NtClose failed %x\n", status );
    status = pNtClose( handle );
    ok( status == STATUS_INVALID_HANDLE, "NtClose failed %x\n", status );

    /* syscall thunk copy */
    ptr = VirtualAlloc( NULL, 0x1000, MEM_COMMIT, PAGE_EXECUTE_READWRITE );
    ok( ptr != NULL, "VirtualAlloc failed\n" );
    memcpy( ptr, pNtClose, 32 );
    pNtClose = ptr;
    handle = CreateEventW( NULL, FALSE, FALSE, NULL );
    ok( handle != 0, "CreateEventWfailed %u\n", GetLastError() );
    status = pNtClose( handle );
    ok( !status, "NtClose failed %x\n", status );
    status = pNtClose( handle );
    ok( status == STATUS_INVALID_HANDLE, "NtClose failed %x\n", status );
    VirtualFree( ptr, 0, MEM_FREE );

    /* new mapping */
    GetModuleFileNameW( module, path, MAX_PATH );
    file = CreateFileW( path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, 0 );
    ok( file != INVALID_HANDLE_VALUE, "can't open %s: %u\n", wine_dbgstr_w(path), GetLastError() );
    mapping = CreateFileMappingW( file, NULL, SEC_IMAGE | PAGE_READONLY, 0, 0, NULL );
    ok( mapping != NULL, "CreateFileMappingW failed err %u\n", GetLastError() );
    ptr = MapViewOfFile( mapping, FILE_MAP_READ, 0, 0, 0 );
    ok( ptr != NULL, "MapViewOfFile failed err %u\n", GetLastError() );
    CloseHandle( mapping );
    CloseHandle( file );
    delta = (char *)ptr - (char *)module;

    if (memcmp( ptr, module, 0x1000 ))
    {
        skip( "modules are not identical (non-PE build?)\n" );
        UnmapViewOfFile( ptr );
        return;
    }
    perform_relocations( ptr, delta );
    pNtClose = (void *)GetProcAddress( module, "NtClose" );

    if (pRtlFindExportedRoutineByName)
    {
        void *func = pRtlFindExportedRoutineByName( module, "NtClose" );
        ok( func == (void *)pNtClose, "wrong ptr %p / %p\n", func, pNtClose );
        func = pRtlFindExportedRoutineByName( ptr, "NtClose" );
        ok( (char *)func - (char *)pNtClose == delta, "wrong ptr %p / %p\n", func, pNtClose );
    }
    else win_skip( "RtlFindExportedRoutineByName not supported\n" );

    if (!memcmp( pNtClose, (char *)pNtClose + delta, 32 ))
    {
        pNtClose = (void *)((char *)pNtClose + delta);
        handle = CreateEventW( NULL, FALSE, FALSE, NULL );
        ok( handle != 0, "CreateEventWfailed %u\n", GetLastError() );
        status = pNtClose( handle );
        ok( !status, "NtClose failed %x\n", status );
        status = pNtClose( handle );
        ok( status == STATUS_INVALID_HANDLE, "NtClose failed %x\n", status );
    }
    else
    {
#ifdef __x86_64__
        ok( 0, "syscall thunk relocated\n" );
#else
        skip( "syscall thunk relocated\n" );
#endif
    }
    UnmapViewOfFile( ptr );
}

START_TEST(virtual)
{
    HMODULE mod;

    int argc;
    char **argv;
    argc = winetest_get_mainargs(&argv);

    if (argc >= 3)
    {
        if (!strcmp(argv[2], "sleep"))
        {
            Sleep(5000); /* spawned process runs for at most 5 seconds */
            return;
        }
        return;
    }

    mod = GetModuleHandleA("kernel32.dll");
    pIsWow64Process = (void *)GetProcAddress(mod, "IsWow64Process");
    pGetEnabledXStateFeatures = (void *)GetProcAddress(mod, "GetEnabledXStateFeatures");
    mod = GetModuleHandleA("ntdll.dll");
    pRtlCreateUserStack = (void *)GetProcAddress(mod, "RtlCreateUserStack");
    pRtlCreateUserThread = (void *)GetProcAddress(mod, "RtlCreateUserThread");
    pRtlFreeUserStack = (void *)GetProcAddress(mod, "RtlFreeUserStack");
    pRtlFindExportedRoutineByName = (void *)GetProcAddress(mod, "RtlFindExportedRoutineByName");
    pRtlGetEnabledExtendedFeatures = (void *)GetProcAddress(mod, "RtlGetEnabledExtendedFeatures");
    pNtAllocateVirtualMemoryEx = (void *)GetProcAddress(mod, "NtAllocateVirtualMemoryEx");

    NtQuerySystemInformation(SystemBasicInformation, &sbi, sizeof(sbi), NULL);
    trace("system page size %#x\n", sbi.PageSize);
    page_size = sbi.PageSize;
    if (!pIsWow64Process || !pIsWow64Process(NtCurrentProcess(), &is_wow64)) is_wow64 = FALSE;

    test_NtAllocateVirtualMemory();
    test_RtlCreateUserStack();
    test_NtMapViewOfSection();
    test_user_shared_data();
    test_syscalls();
}
