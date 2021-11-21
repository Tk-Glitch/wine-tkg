/*
 * WoW64 CPU support
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
#include "winnt.h"
#include "winternl.h"
#include "wine/asm.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(wow);

#include "pshpack1.h"
struct thunk_32to64
{
    BYTE  ljmp;   /* ljmp %cs:1f */
    DWORD addr;
    WORD  cs;
};
#include "poppack.h"

static BYTE DECLSPEC_ALIGN(4096) code_buffer[0x1000];


BOOL WINAPI DllMain( HINSTANCE inst, DWORD reason, void *reserved )
{
    if (reason == DLL_PROCESS_ATTACH) LdrDisableThreadCalloutsForDll( inst );
    return TRUE;
}


/**********************************************************************
 *           syscall_32to64
 *
 * Execute a 64-bit syscall from 32-bit code, then return to 32-bit.
 */
extern void WINAPI syscall_32to64(void) DECLSPEC_HIDDEN;
__ASM_GLOBAL_FUNC( syscall_32to64,
                   /* cf. BTCpuSimulate prolog */
                   __ASM_SEH(".seh_stackalloc 0x28\n\t")
                   __ASM_SEH(".seh_endprologue\n\t")
                   __ASM_CFI(".cfi_adjust_cfa_offset 0x28\n\t")
                   "xchgq %r14,%rsp\n\t"
                   "movl %edi,0x9c(%r13)\n\t"   /* context->Edi */
                   "movl %esi,0xa0(%r13)\n\t"   /* context->Esi */
                   "movl %ebx,0xa4(%r13)\n\t"   /* context->Ebx */
                   "movl %ebp,0xb4(%r13)\n\t"   /* context->Ebp */
                   "movl (%r14),%edx\n\t"
                   "movl %edx,0xb8(%r13)\n\t"   /* context->Eip */
                   "pushfq\n\t"
                   "popq %rdx\n\t"
                   "movl %edx,0xc0(%r13)\n\t"   /* context->EFlags */
                   "leaq 4(%r14),%rdx\n\t"
                   "movl %edx,0xc4(%r13)\n\t"   /* context->Esp */
                   "movq %rax,%rcx\n\t"         /* syscall number */
                   "leaq 8(%r14),%rdx\n\t"      /* parameters */
                   "call " __ASM_NAME("Wow64SystemServiceEx") "\n\t"

                   "syscall_32to64_return:\n\t"
                   "movl 0x9c(%r13),%edi\n\t"   /* context->Edi */
                   "movl 0xa0(%r13),%esi\n\t"   /* context->Esi */
                   "movl 0xa4(%r13),%ebx\n\t"   /* context->Ebx */
                   "movl 0xb4(%r13),%ebp\n\t"   /* context->Ebp */
                   "btrl $0,-4(%r13)\n\t"       /* cpu->Flags & WOW64_CPURESERVED_FLAG_RESET_STATE */
                   "jc 1f\n\t"
                   "movl 0xb8(%r13),%edx\n\t"   /* context->Eip */
                   "movl %edx,(%rsp)\n\t"
                   "movl 0xbc(%r13),%edx\n\t"   /* context->SegCs */
                   "movl %edx,4(%rsp)\n\t"
                   "movl 0xc4(%r13),%r14d\n\t"  /* context->Esp */
                   "xchgq %r14,%rsp\n\t"
                   "ljmp *(%r14)\n"
                   "1:\tmovq %rsp,%r14\n\t"
                   "movl 0xa8(%r13),%edx\n\t"   /* context->Edx */
                   "movl 0xac(%r13),%ecx\n\t"   /* context->Ecx */
                   "movl 0xc8(%r13),%eax\n\t"   /* context->SegSs */
                   "movq %rax,0x20(%rsp)\n\t"
                   "mov %ax,%ds\n\t"
                   "mov %ax,%es\n\t"
                   "mov 0x90(%r13),%fs\n\t"     /* context->SegFs */
                   "movl 0xc4(%r13),%eax\n\t"   /* context->Esp */
                   "movq %rax,0x18(%rsp)\n\t"
                   "movl 0xc0(%r13),%eax\n\t"   /* context->EFlags */
                   "movq %rax,0x10(%rsp)\n\t"
                   "movl 0xbc(%r13),%eax\n\t"   /* context->SegCs */
                   "movq %rax,0x8(%rsp)\n\t"
                   "movl 0xb8(%r13),%eax\n\t"   /* context->Eip */
                   "movq %rax,(%rsp)\n\t"
                   "movl 0xb0(%r13),%eax\n\t"   /* context->Eax */
                   "iretq" )


/**********************************************************************
 *           BTCpuSimulate  (wow64cpu.@)
 */
__ASM_STDCALL_FUNC( BTCpuSimulate, 0,
                    "subq $0x28,%rsp\n"
                   __ASM_SEH(".seh_stackalloc 0x28\n\t")
                   __ASM_SEH(".seh_endprologue\n\t")
                   __ASM_CFI(".cfi_adjust_cfa_offset 0x28\n\t")
                    "movq %gs:0x30,%r12\n\t"
                    "movq 0x1488(%r12),%rcx\n\t" /* NtCurrentTeb()->TlsSlots[WOW64_TLS_CPURESERVED] */
                    "leaq 4(%rcx),%r13\n"        /* cpu->Context */
                    "jmp syscall_32to64_return\n" )


/**********************************************************************
 *           BTCpuProcessInit  (wow64cpu.@)
 */
NTSTATUS WINAPI BTCpuProcessInit(void)
{
    struct thunk_32to64 *thunk = (struct thunk_32to64 *)code_buffer;
    SIZE_T size = sizeof(*thunk);
    ULONG old_prot;
    CONTEXT context;

    if ((ULONG_PTR)syscall_32to64 >> 32)
    {
        ERR( "wow64cpu loaded above 4G, disabling\n" );
        return STATUS_INVALID_ADDRESS;
    }

    RtlCaptureContext( &context );
    thunk->ljmp = 0xea;
    thunk->addr = PtrToUlong( syscall_32to64 );
    thunk->cs   = context.SegCs;
    NtProtectVirtualMemory( GetCurrentProcess(), (void **)&thunk, &size, PAGE_EXECUTE_READ, &old_prot );
    return STATUS_SUCCESS;
}


/**********************************************************************
 *           BTCpuGetBopCode  (wow64cpu.@)
 */
void * WINAPI BTCpuGetBopCode(void)
{
    return code_buffer;
}


/**********************************************************************
 *           BTCpuGetContext  (wow64cpu.@)
 */
NTSTATUS WINAPI BTCpuGetContext( HANDLE thread, HANDLE process, void *unknown, I386_CONTEXT *ctx )
{
    return NtQueryInformationThread( thread, ThreadWow64Context, ctx, sizeof(*ctx), NULL );
}


/**********************************************************************
 *           BTCpuSetContext  (wow64cpu.@)
 */
NTSTATUS WINAPI BTCpuSetContext( HANDLE thread, HANDLE process, void *unknown, I386_CONTEXT *ctx )
{
    return NtSetInformationThread( thread, ThreadWow64Context, ctx, sizeof(*ctx) );
}
