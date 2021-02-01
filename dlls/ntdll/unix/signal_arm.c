/*
 * ARM signal handling routines
 *
 * Copyright 2002 Marcus Meissner, SuSE Linux AG
 * Copyright 2010-2013, 2015 André Hentschel
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

#if 0
#pragma makedep unix
#endif

#ifdef __arm__

#include "config.h"
#include "wine/port.h"

#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
#ifdef HAVE_SYSCALL_H
# include <syscall.h>
#else
# ifdef HAVE_SYS_SYSCALL_H
#  include <sys/syscall.h>
# endif
#endif
#ifdef HAVE_SYS_SIGNAL_H
# include <sys/signal.h>
#endif
#ifdef HAVE_SYS_UCONTEXT_H
# include <sys/ucontext.h>
#endif

#define NONAMELESSUNION
#define NONAMELESSSTRUCT
#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winnt.h"
#include "winternl.h"
#include "wine/exception.h"
#include "wine/asm.h"
#include "unix_private.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(seh);


/***********************************************************************
 * signal context platform-specific definitions
 */
#ifdef linux

#if defined(__ANDROID__) && !defined(HAVE_SYS_UCONTEXT_H)
typedef struct ucontext
{
    unsigned long uc_flags;
    struct ucontext *uc_link;
    stack_t uc_stack;
    struct sigcontext uc_mcontext;
    sigset_t uc_sigmask;
    unsigned long uc_regspace[128] __attribute__((__aligned__(8)));
} ucontext_t;
#endif

/* All Registers access - only for local access */
# define REG_sig(reg_name, context) ((context)->uc_mcontext.reg_name)
# define REGn_sig(reg_num, context) ((context)->uc_mcontext.arm_r##reg_num)

/* Special Registers access  */
# define SP_sig(context)            REG_sig(arm_sp, context)    /* Stack pointer */
# define LR_sig(context)            REG_sig(arm_lr, context)    /* Link register */
# define PC_sig(context)            REG_sig(arm_pc, context)    /* Program counter */
# define CPSR_sig(context)          REG_sig(arm_cpsr, context)  /* Current State Register */
# define IP_sig(context)            REG_sig(arm_ip, context)    /* Intra-Procedure-call scratch register */
# define FP_sig(context)            REG_sig(arm_fp, context)    /* Frame pointer */

/* Exceptions */
# define ERROR_sig(context)         REG_sig(error_code, context)
# define TRAP_sig(context)          REG_sig(trap_no, context)

struct extended_ctx
{
    unsigned long magic;
    unsigned long size;
};

struct vfp_sigframe
{
    struct extended_ctx ctx;
    unsigned long long fpregs[32];
    unsigned long fpscr;
};

static void *get_extended_sigcontext( const ucontext_t *sigcontext, unsigned int magic )
{
    struct extended_ctx *ctx = (struct extended_ctx *)sigcontext->uc_regspace;
    while ((char *)ctx < (char *)(sigcontext + 1) && ctx->magic && ctx->size)
    {
        if (ctx->magic == magic) return ctx;
        ctx = (struct extended_ctx *)((char *)ctx + ctx->size);
    }
    return NULL;
}

static void save_fpu( CONTEXT *context, const ucontext_t *sigcontext )
{
    struct vfp_sigframe *frame = get_extended_sigcontext( sigcontext, 0x56465001 );

    if (!frame) return;
    memcpy( context->u.D, frame->fpregs, sizeof(context->u.D) );
    context->Fpscr = frame->fpscr;
}

static void restore_fpu( const CONTEXT *context, ucontext_t *sigcontext )
{
    struct vfp_sigframe *frame = get_extended_sigcontext( sigcontext, 0x56465001 );

    if (!frame) return;
    memcpy( frame->fpregs, context->u.D, sizeof(context->u.D) );
    frame->fpscr = context->Fpscr;
}

#elif defined(__FreeBSD__)

/* All Registers access - only for local access */
# define REGn_sig(reg_num, context) ((context)->uc_mcontext.__gregs[reg_num])

/* Special Registers access  */
# define SP_sig(context)            REGn_sig(_REG_SP, context)    /* Stack pointer */
# define LR_sig(context)            REGn_sig(_REG_LR, context)    /* Link register */
# define PC_sig(context)            REGn_sig(_REG_PC, context)    /* Program counter */
# define CPSR_sig(context)          REGn_sig(_REG_CPSR, context)  /* Current State Register */
# define IP_sig(context)            REGn_sig(_REG_R12, context)   /* Intra-Procedure-call scratch register */
# define FP_sig(context)            REGn_sig(_REG_FP, context)    /* Frame pointer */

static void save_fpu( CONTEXT *context, const ucontext_t *sigcontext ) { }
static void restore_fpu( const CONTEXT *context, ucontext_t *sigcontext ) { }

#endif /* linux */

enum arm_trap_code
{
    TRAP_ARM_UNKNOWN    = -1,  /* Unknown fault (TRAP_sig not defined) */
    TRAP_ARM_PRIVINFLT  =  6,  /* Invalid opcode exception */
    TRAP_ARM_PAGEFLT    = 14,  /* Page fault */
    TRAP_ARM_ALIGNFLT   = 17,  /* Alignment check exception */
};

struct syscall_frame
{
    DWORD                 pad;
    DWORD                 cpsr;
    DWORD                 r5;
    DWORD                 r6;
    DWORD                 r7;
    DWORD                 r8;
    DWORD                 r9;
    DWORD                 r10;
    DWORD                 r11;
    DWORD                 thunk_addr;
    DWORD                 r4;
    DWORD                 ret_addr;
};

struct arm_thread_data
{
    void                 *exit_frame;    /* 1d4 exit frame pointer */
    struct syscall_frame *syscall_frame; /* 1d8 frame pointer on syscall entry */
};

C_ASSERT( sizeof(struct arm_thread_data) <= sizeof(((struct ntdll_thread_data *)0)->cpu_data) );
C_ASSERT( offsetof( TEB, GdiTebBatch ) + offsetof( struct arm_thread_data, exit_frame ) == 0x1d4 );
C_ASSERT( offsetof( TEB, GdiTebBatch ) + offsetof( struct arm_thread_data, syscall_frame ) == 0x1d8 );

static inline struct arm_thread_data *arm_thread_data(void)
{
    return (struct arm_thread_data *)ntdll_get_thread_data()->cpu_data;
}

void *get_syscall_frame(void)
{
    return arm_thread_data()->syscall_frame;
}

void set_syscall_frame(void *frame)
{
    arm_thread_data()->syscall_frame = frame;
}

/***********************************************************************
 *           unwind_builtin_dll
 */
NTSTATUS CDECL unwind_builtin_dll( ULONG type, struct _DISPATCHER_CONTEXT *dispatch, CONTEXT *context )
{
    return STATUS_UNSUCCESSFUL;
}


/***********************************************************************
 *           get_trap_code
 *
 * Get the trap code for a signal.
 */
static inline enum arm_trap_code get_trap_code( int signal, const ucontext_t *sigcontext )
{
#ifdef TRAP_sig
    enum arm_trap_code trap = TRAP_sig(sigcontext);
    if (trap)
        return trap;
#endif

    switch (signal)
    {
    case SIGILL:
        return TRAP_ARM_PRIVINFLT;
    case SIGSEGV:
        return TRAP_ARM_PAGEFLT;
    case SIGBUS:
        return TRAP_ARM_ALIGNFLT;
    default:
        return TRAP_ARM_UNKNOWN;
    }
}


/***********************************************************************
 *           get_error_code
 *
 * Get the error code for a signal.
 */
static inline WORD get_error_code( const ucontext_t *sigcontext )
{
#ifdef ERROR_sig
    return ERROR_sig(sigcontext);
#else
    return 0;
#endif
}


/***********************************************************************
 *           save_context
 *
 * Set the register values from a sigcontext.
 */
static void save_context( CONTEXT *context, const ucontext_t *sigcontext )
{
#define C(x) context->R##x = REGn_sig(x,sigcontext)
    /* Save normal registers */
    C(0); C(1); C(2); C(3); C(4); C(5); C(6); C(7); C(8); C(9); C(10);
#undef C

    context->ContextFlags = CONTEXT_FULL;
    context->Sp   = SP_sig(sigcontext);   /* Stack pointer */
    context->Lr   = LR_sig(sigcontext);   /* Link register */
    context->Pc   = PC_sig(sigcontext);   /* Program Counter */
    context->Cpsr = CPSR_sig(sigcontext); /* Current State Register */
    context->R11  = FP_sig(sigcontext);   /* Frame pointer */
    context->R12  = IP_sig(sigcontext);   /* Intra-Procedure-call scratch register */
    if (CPSR_sig(sigcontext) & 0x20) context->Pc |= 1;  /* Thumb mode */
    save_fpu( context, sigcontext );
}


/***********************************************************************
 *           restore_context
 *
 * Build a sigcontext from the register values.
 */
static void restore_context( const CONTEXT *context, ucontext_t *sigcontext )
{
#define C(x)  REGn_sig(x,sigcontext) = context->R##x
    /* Restore normal registers */
    C(0); C(1); C(2); C(3); C(4); C(5); C(6); C(7); C(8); C(9); C(10);
#undef C

    SP_sig(sigcontext)   = context->Sp;   /* Stack pointer */
    LR_sig(sigcontext)   = context->Lr;   /* Link register */
    PC_sig(sigcontext)   = context->Pc;   /* Program Counter */
    CPSR_sig(sigcontext) = context->Cpsr; /* Current State Register */
    FP_sig(sigcontext)   = context->R11;  /* Frame pointer */
    IP_sig(sigcontext)   = context->R12;  /* Intra-Procedure-call scratch register */
    if (PC_sig(sigcontext) & 1) CPSR_sig(sigcontext) |= 0x20;
    else CPSR_sig(sigcontext) &= ~0x20;
    restore_fpu( context, sigcontext );
}


/***********************************************************************
 *           set_cpu_context
 *
 * Set the new CPU context.
 */
void DECLSPEC_HIDDEN set_cpu_context( const CONTEXT *context );
__ASM_GLOBAL_FUNC( set_cpu_context,
                   "ldr r2, [r0, #0x44]\n\t"  /* context->Cpsr */
                   "tst r2, #0x20\n\t"        /* thumb? */
                   "ldr r1, [r0, #0x40]\n\t"  /* context->Pc */
                   "ite ne\n\t"
                   "orrne r1, r1, #1\n\t"     /* Adjust PC according to thumb */
                   "biceq r1, r1, #1\n\t"     /* Adjust PC according to arm */
                   "msr CPSR_f, r2\n\t"
                   "ldr lr, [r0, #0x3c]\n\t"  /* context->Lr */
                   "ldr sp, [r0, #0x38]\n\t"  /* context->Sp */
                   "push {r1}\n\t"
                   "add r0, #0x4\n\t"
                   "ldm r0, {r0-r12}\n\t"     /* context->R0..R12 */
                   "pop {pc}" )


/***********************************************************************
 *           get_server_context_flags
 *
 * Convert CPU-specific flags to generic server flags
 */
static unsigned int get_server_context_flags( DWORD flags )
{
    unsigned int ret = 0;

    flags &= ~CONTEXT_ARM;  /* get rid of CPU id */
    if (flags & CONTEXT_CONTROL) ret |= SERVER_CTX_CONTROL;
    if (flags & CONTEXT_INTEGER) ret |= SERVER_CTX_INTEGER;
    if (flags & CONTEXT_FLOATING_POINT) ret |= SERVER_CTX_FLOATING_POINT;
    if (flags & CONTEXT_DEBUG_REGISTERS) ret |= SERVER_CTX_DEBUG_REGISTERS;
    return ret;
}


/***********************************************************************
 *           context_to_server
 *
 * Convert a register context to the server format.
 */
NTSTATUS context_to_server( context_t *to, const CONTEXT *from )
{
    DWORD i, flags = from->ContextFlags & ~CONTEXT_ARM;  /* get rid of CPU id */

    memset( to, 0, sizeof(*to) );
    to->cpu = CPU_ARM;

    if (flags & CONTEXT_CONTROL)
    {
        to->flags |= SERVER_CTX_CONTROL;
        to->ctl.arm_regs.sp   = from->Sp;
        to->ctl.arm_regs.lr   = from->Lr;
        to->ctl.arm_regs.pc   = from->Pc;
        to->ctl.arm_regs.cpsr = from->Cpsr;
    }
    if (flags & CONTEXT_INTEGER)
    {
        to->flags |= SERVER_CTX_INTEGER;
        to->integer.arm_regs.r[0]  = from->R0;
        to->integer.arm_regs.r[1]  = from->R1;
        to->integer.arm_regs.r[2]  = from->R2;
        to->integer.arm_regs.r[3]  = from->R3;
        to->integer.arm_regs.r[4]  = from->R4;
        to->integer.arm_regs.r[5]  = from->R5;
        to->integer.arm_regs.r[6]  = from->R6;
        to->integer.arm_regs.r[7]  = from->R7;
        to->integer.arm_regs.r[8]  = from->R8;
        to->integer.arm_regs.r[9]  = from->R9;
        to->integer.arm_regs.r[10] = from->R10;
        to->integer.arm_regs.r[11] = from->R11;
        to->integer.arm_regs.r[12] = from->R12;
    }
    if (flags & CONTEXT_FLOATING_POINT)
    {
        to->flags |= SERVER_CTX_FLOATING_POINT;
        for (i = 0; i < 32; i++) to->fp.arm_regs.d[i] = from->u.D[i];
        to->fp.arm_regs.fpscr = from->Fpscr;
    }
    if (flags & CONTEXT_DEBUG_REGISTERS)
    {
        to->flags |= SERVER_CTX_DEBUG_REGISTERS;
        for (i = 0; i < ARM_MAX_BREAKPOINTS; i++) to->debug.arm_regs.bvr[i] = from->Bvr[i];
        for (i = 0; i < ARM_MAX_BREAKPOINTS; i++) to->debug.arm_regs.bcr[i] = from->Bcr[i];
        for (i = 0; i < ARM_MAX_WATCHPOINTS; i++) to->debug.arm_regs.wvr[i] = from->Wvr[i];
        for (i = 0; i < ARM_MAX_WATCHPOINTS; i++) to->debug.arm_regs.wcr[i] = from->Wcr[i];
    }
    return STATUS_SUCCESS;
}


/***********************************************************************
 *           context_from_server
 *
 * Convert a register context from the server format.
 */
NTSTATUS context_from_server( CONTEXT *to, const context_t *from )
{
    DWORD i;

    if (from->cpu != CPU_ARM) return STATUS_INVALID_PARAMETER;

    to->ContextFlags = CONTEXT_ARM;
    if (from->flags & SERVER_CTX_CONTROL)
    {
        to->ContextFlags |= CONTEXT_CONTROL;
        to->Sp   = from->ctl.arm_regs.sp;
        to->Lr   = from->ctl.arm_regs.lr;
        to->Pc   = from->ctl.arm_regs.pc;
        to->Cpsr = from->ctl.arm_regs.cpsr;
    }
    if (from->flags & SERVER_CTX_INTEGER)
    {
        to->ContextFlags |= CONTEXT_INTEGER;
        to->R0  = from->integer.arm_regs.r[0];
        to->R1  = from->integer.arm_regs.r[1];
        to->R2  = from->integer.arm_regs.r[2];
        to->R3  = from->integer.arm_regs.r[3];
        to->R4  = from->integer.arm_regs.r[4];
        to->R5  = from->integer.arm_regs.r[5];
        to->R6  = from->integer.arm_regs.r[6];
        to->R7  = from->integer.arm_regs.r[7];
        to->R8  = from->integer.arm_regs.r[8];
        to->R9  = from->integer.arm_regs.r[9];
        to->R10 = from->integer.arm_regs.r[10];
        to->R11 = from->integer.arm_regs.r[11];
        to->R12 = from->integer.arm_regs.r[12];
    }
    if (from->flags & SERVER_CTX_FLOATING_POINT)
    {
        to->ContextFlags |= CONTEXT_FLOATING_POINT;
        for (i = 0; i < 32; i++) to->u.D[i] = from->fp.arm_regs.d[i];
        to->Fpscr = from->fp.arm_regs.fpscr;
    }
    if (from->flags & SERVER_CTX_DEBUG_REGISTERS)
    {
        to->ContextFlags |= CONTEXT_DEBUG_REGISTERS;
        for (i = 0; i < ARM_MAX_BREAKPOINTS; i++) to->Bvr[i] = from->debug.arm_regs.bvr[i];
        for (i = 0; i < ARM_MAX_BREAKPOINTS; i++) to->Bcr[i] = from->debug.arm_regs.bcr[i];
        for (i = 0; i < ARM_MAX_WATCHPOINTS; i++) to->Wvr[i] = from->debug.arm_regs.wvr[i];
        for (i = 0; i < ARM_MAX_WATCHPOINTS; i++) to->Wcr[i] = from->debug.arm_regs.wcr[i];
    }
    return STATUS_SUCCESS;
}


/***********************************************************************
 *              NtSetContextThread  (NTDLL.@)
 *              ZwSetContextThread  (NTDLL.@)
 */
NTSTATUS WINAPI NtSetContextThread( HANDLE handle, const CONTEXT *context )
{
    NTSTATUS ret;
    BOOL self;
    context_t server_context;

    context_to_server( &server_context, context );
    ret = set_thread_context( handle, &server_context, &self );
    if (self && ret == STATUS_SUCCESS)
    {
        arm_thread_data()->syscall_frame = NULL;
        set_cpu_context( context );
    }
    return ret;
}


/***********************************************************************
 *              NtGetContextThread  (NTDLL.@)
 *              ZwGetContextThread  (NTDLL.@)
 */
NTSTATUS WINAPI NtGetContextThread( HANDLE handle, CONTEXT *context )
{
    NTSTATUS ret;
    struct syscall_frame *frame = arm_thread_data()->syscall_frame;
    DWORD needed_flags = context->ContextFlags & ~CONTEXT_ARM;
    BOOL self = (handle == GetCurrentThread());

    if (!self)
    {
        context_t server_context;
        unsigned int server_flags = get_server_context_flags( context->ContextFlags );

        if ((ret = get_thread_context( handle, &server_context, server_flags, &self ))) return ret;
        if ((ret = context_from_server( context, &server_context ))) return ret;
        needed_flags &= ~context->ContextFlags;
    }

    if (self)
    {
        if (needed_flags & CONTEXT_INTEGER)
        {
            context->R0  = 0;
            context->R1  = 0;
            context->R2  = 0;
            context->R3  = 0;
            context->R4  = frame->r4;
            context->R5  = frame->r5;
            context->R6  = frame->r6;
            context->R7  = frame->r7;
            context->R8  = frame->r8;
            context->R9  = frame->r9;
            context->R10 = frame->r10;
            context->R11 = frame->r11;
            context->R12 = 0;
            context->ContextFlags |= CONTEXT_INTEGER;
        }
        if (needed_flags & CONTEXT_CONTROL)
        {
            context->Sp   = (DWORD)&frame->r4;
            context->Lr   = frame->thunk_addr;
            context->Pc   = frame->thunk_addr;
            context->Cpsr = frame->cpsr;
            context->ContextFlags |= CONTEXT_CONTROL;
        }
        if (needed_flags & CONTEXT_FLOATING_POINT) FIXME( "floating point not implemented\n" );
    }
    return STATUS_SUCCESS;
}


/***********************************************************************
 *           setup_exception
 *
 * Modify the signal context to call the exception raise function.
 */
static void setup_exception( ucontext_t *sigcontext, EXCEPTION_RECORD *rec )
{
    struct
    {
        CONTEXT          context;
        EXCEPTION_RECORD rec;
    } *stack;

    void *stack_ptr = (void *)(SP_sig(sigcontext) & ~3);
    CONTEXT context;
    NTSTATUS status;

    rec->ExceptionAddress = (void *)PC_sig(sigcontext);
    save_context( &context, sigcontext );
    if (rec->ExceptionCode == EXCEPTION_BREAKPOINT)
        context.Pc += CPSR_sig(sigcontext) & 0x20 ? 2 : 4;

    status = send_debug_event( rec, &context, TRUE );
    if (status == DBG_CONTINUE || status == DBG_EXCEPTION_HANDLED)
    {
        restore_context( &context, sigcontext );
        return;
    }

    stack = virtual_setup_exception( stack_ptr, sizeof(*stack), rec );
    stack->rec = *rec;
    stack->context = context;

    /* now modify the sigcontext to return to the raise function */
    SP_sig(sigcontext) = (DWORD)stack;
    PC_sig(sigcontext) = (DWORD)pKiUserExceptionDispatcher;
    if (PC_sig(sigcontext) & 1) CPSR_sig(sigcontext) |= 0x20;
    else CPSR_sig(sigcontext) &= ~0x20;
    REGn_sig(0, sigcontext) = (DWORD)&stack->rec;  /* first arg for KiUserExceptionDispatcher */
    REGn_sig(1, sigcontext) = (DWORD)&stack->context; /* second arg for KiUserExceptionDispatcher */
}


/***********************************************************************
 *           call_user_apc_dispatcher
 */
__ASM_GLOBAL_FUNC( call_user_apc_dispatcher,
                   "mov r5, r1\n\t"           /* ctx */
                   "mov r6, r2\n\t"           /* arg1 */
                   "mov r7, r3\n\t"           /* arg2 */
                   "ldr r8, [sp]\n\t"         /* func */
                   "ldr r9, [sp, #4]\n\t"     /* dispatcher */
                   "mrc p15, 0, r10, c13, c0, 2\n\t"  /* NtCurrentTeb() */
                   "cmp r0, #0\n\t"           /* context_ptr */
                   "beq 1f\n\t"
                   "ldr r0, [r0, #0x38]\n\t"  /* context_ptr->Sp */
                   "sub r0, r0, #0x1c8\n\t"   /* sizeof(CONTEXT) + offsetof(frame,r4) */
                   "mov ip, #0\n\t"
                   "str ip, [r10, #0x1d8]\n\t"  /* arm_thread_data()->syscall_frame */
                   "mov sp, r0\n\t"
                   "b 2f\n"
                   "1:\tldr r0, [r10, #0x1d8]\n\t"
                   "sub r0, #0x1a0\n\t"
                   "mov sp, r0\n\t"
                   "mov r0, #3\n\t"
                   "movt r0, #32\n\t"
                   "str r0, [sp]\n\t"         /* context.ContextFlags = CONTEXT_FULL */
                   "mov r1, sp\n\t"
                   "mov r0, #~1\n\t"
                   "bl " __ASM_NAME("NtGetContextThread") "\n\t"
                   "mov r0, #0xc0\n\t"
                   "str r0, [sp, #4]\n\t"     /* context.R0 = STATUS_USER_APC */
                   "mov r0, sp\n\t"
                   "mov ip, #0\n\t"
                   "str ip, [r10, #0x1d8]\n\t"
                   "2:\tmov r1, r5\n\t"       /* ctx */
                   "mov r2, r6\n\t"           /* arg1 */
                   "mov r3, r7\n\t"           /* arg2 */
                   "push {r8, r9}\n\t"        /* func */
                   "ldr lr, [r0, #0x3c]\n\t"  /* context.Lr */
                   "bx r9" )


/***********************************************************************
 *           call_raise_user_exception_dispatcher
 */
__ASM_GLOBAL_FUNC( call_raise_user_exception_dispatcher,
                   "mov r2, r0\n\t"  /* dispatcher */
                   "b " __ASM_NAME("call_user_exception_dispatcher") )


/***********************************************************************
 *           call_user_exception_dispatcher
 */
__ASM_GLOBAL_FUNC( call_user_exception_dispatcher,
                   "mrc p15, 0, r7, c13, c0, 2\n\t"  /* NtCurrentTeb() */
                   "ldr r3, [r7, #0x1d8]\n\t"  /* arm_thread_data()->syscall_frame */
                   "mov ip, #0\n\t"
                   "str ip, [r7, #0x1d8]\n\t"
                   "add r3, r3, #8\n\t"
                   "ldm r3, {r5-r11}\n\t"
                   "ldr r4, [r3, #32]\n\t"
                   "ldr lr, [r3, #36]\n\t"
                   "add r3, #40\n\t"
                   "mov sp, r3\n\t"
                   "bx r2" )


/***********************************************************************
 *           handle_syscall_fault
 *
 * Handle a page fault happening during a system call.
 */
static BOOL handle_syscall_fault( ucontext_t *context, EXCEPTION_RECORD *rec )
{
    struct syscall_frame *frame = arm_thread_data()->syscall_frame;
    __WINE_FRAME *wine_frame = (__WINE_FRAME *)NtCurrentTeb()->Tib.ExceptionList;
    DWORD i;

    if (!frame) return FALSE;

    TRACE( "code=%x flags=%x addr=%p pc=%08x tid=%04x\n",
           rec->ExceptionCode, rec->ExceptionFlags, rec->ExceptionAddress,
           (DWORD)PC_sig(context), GetCurrentThreadId() );
    for (i = 0; i < rec->NumberParameters; i++)
        TRACE( " info[%d]=%08lx\n", i, rec->ExceptionInformation[i] );

    TRACE( " r0=%08x r1=%08x r2=%08x r3=%08x r4=%08x r5=%08x\n",
           (DWORD)REGn_sig(0, context), (DWORD)REGn_sig(1, context), (DWORD)REGn_sig(2, context),
           (DWORD)REGn_sig(3, context), (DWORD)REGn_sig(4, context), (DWORD)REGn_sig(5, context) );
    TRACE( " r6=%08x r7=%08x r8=%08x r9=%08x r10=%08x r11=%08x\n",
           (DWORD)REGn_sig(6, context), (DWORD)REGn_sig(7, context), (DWORD)REGn_sig(8, context),
           (DWORD)REGn_sig(9, context), (DWORD)REGn_sig(10, context), (DWORD)FP_sig(context) );
    TRACE( " r12=%08x sp=%08x lr=%08x pc=%08x cpsr=%08x\n",
           (DWORD)IP_sig(context), (DWORD)SP_sig(context), (DWORD)LR_sig(context),
           (DWORD)PC_sig(context), (DWORD)CPSR_sig(context) );

    if ((char *)wine_frame < (char *)frame)
    {
        TRACE( "returning to handler\n" );
        REGn_sig(0, context) = (DWORD)&wine_frame->jmp;
        REGn_sig(1, context) = 1;
        PC_sig(context)      = (DWORD)__wine_longjmp;
    }
    else
    {
        TRACE( "returning to user mode ip=%08x ret=%08x\n", frame->ret_addr, rec->ExceptionCode );
        REGn_sig(0, context)  = rec->ExceptionCode;
        REGn_sig(4, context)  = frame->r4;
        REGn_sig(5, context)  = frame->r5;
        REGn_sig(6, context)  = frame->r6;
        REGn_sig(7, context)  = frame->r7;
        REGn_sig(8, context)  = frame->r8;
        REGn_sig(9, context)  = frame->r9;
        REGn_sig(10, context) = frame->r10;
        FP_sig(context)       = frame->r11;
        LR_sig(context)       = frame->thunk_addr;
        SP_sig(context)       = (DWORD)&frame->r4;
        PC_sig(context)       = frame->thunk_addr;
        CPSR_sig(context)     = frame->cpsr;
        arm_thread_data()->syscall_frame = NULL;
    }
    return TRUE;
}


/**********************************************************************
 *		segv_handler
 *
 * Handler for SIGSEGV and related errors.
 */
static void segv_handler( int signal, siginfo_t *siginfo, void *sigcontext )
{
    EXCEPTION_RECORD rec = { 0 };
    ucontext_t *context = sigcontext;

    switch (get_trap_code(signal, context))
    {
    case TRAP_ARM_PRIVINFLT:   /* Invalid opcode exception */
        rec.ExceptionCode = EXCEPTION_ILLEGAL_INSTRUCTION;
        break;
    case TRAP_ARM_PAGEFLT:  /* Page fault */
        rec.NumberParameters = 2;
        rec.ExceptionInformation[0] = (get_error_code(context) & 0x800) != 0;
        rec.ExceptionInformation[1] = (ULONG_PTR)siginfo->si_addr;
        rec.ExceptionCode = virtual_handle_fault( siginfo->si_addr, rec.ExceptionInformation[0],
                                                  (void *)SP_sig(context) );
        if (!rec.ExceptionCode) return;
        break;
    case TRAP_ARM_ALIGNFLT:  /* Alignment check exception */
        rec.ExceptionCode = EXCEPTION_DATATYPE_MISALIGNMENT;
        break;
    case TRAP_ARM_UNKNOWN:   /* Unknown fault code */
        rec.ExceptionCode = EXCEPTION_ACCESS_VIOLATION;
        rec.NumberParameters = 2;
        rec.ExceptionInformation[0] = 0;
        rec.ExceptionInformation[1] = 0xffffffff;
        break;
    default:
        ERR("Got unexpected trap %d\n", get_trap_code(signal, context));
        rec.ExceptionCode = EXCEPTION_ILLEGAL_INSTRUCTION;
        break;
    }
    if (handle_syscall_fault( context, &rec )) return;
    setup_exception( context, &rec );
}


/**********************************************************************
 *		trap_handler
 *
 * Handler for SIGTRAP.
 */
static void trap_handler( int signal, siginfo_t *siginfo, void *sigcontext )
{
    EXCEPTION_RECORD rec = { 0 };

    switch (siginfo->si_code)
    {
    case TRAP_TRACE:
        rec.ExceptionCode = EXCEPTION_SINGLE_STEP;
        break;
    case TRAP_BRKPT:
    default:
        rec.ExceptionCode = EXCEPTION_BREAKPOINT;
        rec.NumberParameters = 1;
        break;
    }
    setup_exception( sigcontext, &rec );
}


/**********************************************************************
 *		fpe_handler
 *
 * Handler for SIGFPE.
 */
static void fpe_handler( int signal, siginfo_t *siginfo, void *sigcontext )
{
    EXCEPTION_RECORD rec = { 0 };

    switch (siginfo->si_code & 0xffff )
    {
#ifdef FPE_FLTSUB
    case FPE_FLTSUB:
        rec.ExceptionCode = EXCEPTION_ARRAY_BOUNDS_EXCEEDED;
        break;
#endif
#ifdef FPE_INTDIV
    case FPE_INTDIV:
        rec.ExceptionCode = EXCEPTION_INT_DIVIDE_BY_ZERO;
        break;
#endif
#ifdef FPE_INTOVF
    case FPE_INTOVF:
        rec.ExceptionCode = EXCEPTION_INT_OVERFLOW;
        break;
#endif
#ifdef FPE_FLTDIV
    case FPE_FLTDIV:
        rec.ExceptionCode = EXCEPTION_FLT_DIVIDE_BY_ZERO;
        break;
#endif
#ifdef FPE_FLTOVF
    case FPE_FLTOVF:
        rec.ExceptionCode = EXCEPTION_FLT_OVERFLOW;
        break;
#endif
#ifdef FPE_FLTUND
    case FPE_FLTUND:
        rec.ExceptionCode = EXCEPTION_FLT_UNDERFLOW;
        break;
#endif
#ifdef FPE_FLTRES
    case FPE_FLTRES:
        rec.ExceptionCode = EXCEPTION_FLT_INEXACT_RESULT;
        break;
#endif
#ifdef FPE_FLTINV
    case FPE_FLTINV:
#endif
    default:
        rec.ExceptionCode = EXCEPTION_FLT_INVALID_OPERATION;
        break;
    }
    setup_exception( sigcontext, &rec );
}


/**********************************************************************
 *		int_handler
 *
 * Handler for SIGINT.
 */
static void int_handler( int signal, siginfo_t *siginfo, void *sigcontext )
{
    EXCEPTION_RECORD rec = { CONTROL_C_EXIT };

    setup_exception( sigcontext, &rec );
}


/**********************************************************************
 *		abrt_handler
 *
 * Handler for SIGABRT.
 */
static void abrt_handler( int signal, siginfo_t *siginfo, void *sigcontext )
{
    EXCEPTION_RECORD rec = { EXCEPTION_WINE_ASSERTION, EH_NONCONTINUABLE };

    setup_exception( sigcontext, &rec );
}


/**********************************************************************
 *		quit_handler
 *
 * Handler for SIGQUIT.
 */
static void quit_handler( int signal, siginfo_t *siginfo, void *sigcontext )
{
    abort_thread(0);
}


/**********************************************************************
 *		usr1_handler
 *
 * Handler for SIGUSR1, used to signal a thread that it got suspended.
 */
static void usr1_handler( int signal, siginfo_t *siginfo, void *sigcontext )
{
    CONTEXT context;

    save_context( &context, sigcontext );
    wait_suspend( &context );
    restore_context( &context, sigcontext );
}


/**********************************************************************
 *           get_thread_ldt_entry
 */
NTSTATUS get_thread_ldt_entry( HANDLE handle, void *data, ULONG len, ULONG *ret_len )
{
    return STATUS_NOT_IMPLEMENTED;
}


/******************************************************************************
 *           NtSetLdtEntries   (NTDLL.@)
 *           ZwSetLdtEntries   (NTDLL.@)
 */
NTSTATUS WINAPI NtSetLdtEntries( ULONG sel1, LDT_ENTRY entry1, ULONG sel2, LDT_ENTRY entry2 )
{
    return STATUS_NOT_IMPLEMENTED;
}


/**********************************************************************
 *             signal_init_threading
 */
void signal_init_threading(void)
{
}


/**********************************************************************
 *             signal_alloc_thread
 */
NTSTATUS signal_alloc_thread( TEB *teb )
{
    return STATUS_SUCCESS;
}


/**********************************************************************
 *             signal_free_thread
 */
void signal_free_thread( TEB *teb )
{
}


/**********************************************************************
 *		signal_init_thread
 */
void signal_init_thread( TEB *teb )
{
    __asm__ __volatile__( "mcr p15, 0, %0, c13, c0, 2" : : "r" (teb) );
}


/**********************************************************************
 *		signal_init_process
 */
void signal_init_process(void)
{
    struct sigaction sig_act;

    sig_act.sa_mask = server_block_set;
    sig_act.sa_flags = SA_RESTART | SA_SIGINFO | SA_ONSTACK;

    sig_act.sa_sigaction = int_handler;
    if (sigaction( SIGINT, &sig_act, NULL ) == -1) goto error;
    sig_act.sa_sigaction = fpe_handler;
    if (sigaction( SIGFPE, &sig_act, NULL ) == -1) goto error;
    sig_act.sa_sigaction = abrt_handler;
    if (sigaction( SIGABRT, &sig_act, NULL ) == -1) goto error;
    sig_act.sa_sigaction = quit_handler;
    if (sigaction( SIGQUIT, &sig_act, NULL ) == -1) goto error;
    sig_act.sa_sigaction = usr1_handler;
    if (sigaction( SIGUSR1, &sig_act, NULL ) == -1) goto error;
    sig_act.sa_sigaction = trap_handler;
    if (sigaction( SIGTRAP, &sig_act, NULL ) == -1) goto error;
    sig_act.sa_sigaction = segv_handler;
    if (sigaction( SIGSEGV, &sig_act, NULL ) == -1) goto error;
    if (sigaction( SIGILL, &sig_act, NULL ) == -1) goto error;
    if (sigaction( SIGBUS, &sig_act, NULL ) == -1) goto error;
    return;

 error:
    perror("sigaction");
    exit(1);
}

/**********************************************************************
 *    signal_init_early
 */
void signal_init_early(void)
{
}

/***********************************************************************
 *           init_thread_context
 */
static void init_thread_context( CONTEXT *context, LPTHREAD_START_ROUTINE entry, void *arg, TEB *teb )
{
    context->R0 = (DWORD)entry;
    context->R1 = (DWORD)arg;
    context->Sp = (DWORD)teb->Tib.StackBase;
    context->Pc = (DWORD)pRtlUserThreadStart;
    if (context->Pc & 1) context->Cpsr |= 0x20; /* thumb mode */
}


/***********************************************************************
 *           get_initial_context
 */
PCONTEXT DECLSPEC_HIDDEN get_initial_context( LPTHREAD_START_ROUTINE entry, void *arg,
                                              BOOL suspend, TEB *teb )
{
    CONTEXT *ctx;

    if (suspend)
    {
        CONTEXT context = { CONTEXT_ALL };

        init_thread_context( &context, entry, arg, teb );
        wait_suspend( &context );
        ctx = (CONTEXT *)((ULONG_PTR)context.Sp & ~15) - 1;
        *ctx = context;
    }
    else
    {
        ctx = (CONTEXT *)teb->Tib.StackBase - 1;
        init_thread_context( ctx, entry, arg, teb );
    }
    pthread_sigmask( SIG_UNBLOCK, &server_block_set, NULL );
    ctx->ContextFlags = CONTEXT_FULL;
    return ctx;
}


/***********************************************************************
 *           signal_start_thread
 */
__ASM_GLOBAL_FUNC( signal_start_thread,
                   "push {r4-r12,lr}\n\t"
                   "mov r5, r3\n\t"           /* thunk */
                   /* store exit frame */
                   "ldr r3, [sp, #40]\n\t"    /* teb */
                   "str sp, [r3, #0x1d4]\n\t" /* arm_thread_data()->exit_frame */
                   /* switch to thread stack */
                   "ldr r4, [r3, #4]\n\t"     /* teb->Tib.StackBase */
                   "sub r4, #0x1000\n\t"
                   "mov sp, r4\n\t"
                   /* attach dlls */
                   "bl " __ASM_NAME("get_initial_context") "\n\t"
                   "mov lr, #0\n\t"
                   "bx r5" )


extern void DECLSPEC_NORETURN call_thread_exit_func( int status, void (*func)(int), TEB *teb );
__ASM_GLOBAL_FUNC( call_thread_exit_func,
                   "ldr r3, [r2, #0x1d4]\n\t"  /* arm_thread_data()->exit_frame */
                   "mov ip, #0\n\t"
                   "str ip, [r2, #0x1d4]\n\t"
                   "cmp r3, ip\n\t"
                   "it ne\n\t"
                   "movne sp, r3\n\t"
                   "blx r1" )

/***********************************************************************
 *           signal_exit_thread
 */
void signal_exit_thread( int status, void (*func)(int) )
{
    call_thread_exit_func( status, func, NtCurrentTeb() );
}

#endif  /* __arm__ */
