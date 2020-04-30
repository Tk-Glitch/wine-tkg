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

#ifdef __arm__

#include "config.h"
#include "wine/port.h"

#include <assert.h>
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
#include "winternl.h"
#include "wine/library.h"
#include "wine/exception.h"
#include "ntdll_misc.h"
#include "wine/debug.h"
#include "winnt.h"

WINE_DEFAULT_DEBUG_CHANNEL(seh);
WINE_DECLARE_DEBUG_CHANNEL(relay);

static pthread_key_t teb_key;

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

#endif /* linux */

enum arm_trap_code
{
    TRAP_ARM_UNKNOWN    = -1,  /* Unknown fault (TRAP_sig not defined) */
    TRAP_ARM_PRIVINFLT  =  6,  /* Invalid opcode exception */
    TRAP_ARM_PAGEFLT    = 14,  /* Page fault */
    TRAP_ARM_ALIGNFLT   = 17,  /* Alignment check exception */
};

typedef void (WINAPI *raise_func)( EXCEPTION_RECORD *rec, CONTEXT *context );
typedef int (*wine_signal_handler)(unsigned int sig);

static wine_signal_handler handlers[256];


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
 *           dispatch_signal
 */
static inline int dispatch_signal(unsigned int sig)
{
    if (handlers[sig] == NULL) return 0;
    return handlers[sig](sig);
}

/*******************************************************************
 *         is_valid_frame
 */
static inline BOOL is_valid_frame( void *frame )
{
    if ((ULONG_PTR)frame & 3) return FALSE;
    return (frame >= NtCurrentTeb()->Tib.StackLimit &&
            (void **)frame < (void **)NtCurrentTeb()->Tib.StackBase - 1);
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
    LR_sig(sigcontext)   = context->Lr ;  /* Link register */
    PC_sig(sigcontext)   = context->Pc;   /* Program Counter */
    CPSR_sig(sigcontext) = context->Cpsr; /* Current State Register */
    FP_sig(sigcontext)   = context->R11;  /* Frame pointer */
    IP_sig(sigcontext)   = context->R12;  /* Intra-Procedure-call scratch register */
}


/***********************************************************************
 *           save_fpu
 *
 * Set the FPU context from a sigcontext.
 */
static inline void save_fpu( CONTEXT *context, const ucontext_t *sigcontext )
{
    FIXME("not implemented\n");
}


/***********************************************************************
 *           restore_fpu
 *
 * Restore the FPU context to a sigcontext.
 */
static inline void restore_fpu( CONTEXT *context, const ucontext_t *sigcontext )
{
    FIXME("not implemented\n");
}

/**************************************************************************
 *		__chkstk (NTDLL.@)
 *
 * Incoming r4 contains words to allocate, converting to bytes then return
 */
__ASM_GLOBAL_FUNC( __chkstk, "lsl r4, r4, #2\n\t"
                             "bx lr" )

/***********************************************************************
 *		RtlCaptureContext (NTDLL.@)
 */
/* FIXME: Use the Stack instead of the actual register values */
__ASM_STDCALL_FUNC( RtlCaptureContext, 4,
                    ".arm\n\t"
                    "stmib r0, {r0-r12}\n\t"   /* context->R0..R12 */
                    "mov r1, #0x0200000\n\t"   /* CONTEXT_ARM */
                    "add r1, r1, #0x3\n\t"     /* CONTEXT_FULL */
                    "str r1, [r0]\n\t"         /* context->ContextFlags */
                    "str SP, [r0, #0x38]\n\t"  /* context->Sp */
                    "str LR, [r0, #0x3c]\n\t"  /* context->Lr */
                    "str PC, [r0, #0x40]\n\t"  /* context->Pc */
                    "mrs r1, CPSR\n\t"
                    "str r1, [r0, #0x44]\n\t"  /* context->Cpsr */
                    "bx lr"
                    )


/***********************************************************************
 *           set_cpu_context
 *
 * Set the new CPU context.
 */
void DECLSPEC_HIDDEN set_cpu_context( const CONTEXT *context );
__ASM_GLOBAL_FUNC( set_cpu_context,
                   ".arm\n\t"
                   "ldr r2, [r0, #0x44]\n\t"  /* context->Cpsr */
                   "tst r2, #0x20\n\t"        /* thumb? */
                   "ldr r1, [r0, #0x40]\n\t"  /* context->Pc */
                   "orrne r1, r1, #1\n\t"     /* Adjust PC according to thumb */
                   "biceq r1, r1, #1\n\t"     /* Adjust PC according to arm */
                   "msr CPSR_f, r2\n\t"
                   "ldr lr, [r0, #0x3c]\n\t"  /* context->Lr */
                   "ldr sp, [r0, #0x38]\n\t"  /* context->Sp */
                   "push {r1}\n\t"
                   "ldmib r0, {r0-r12}\n\t"   /* context->R0..R12 */
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
 *           copy_context
 *
 * Copy a register context according to the flags.
 */
static void copy_context( CONTEXT *to, const CONTEXT *from, DWORD flags )
{
    flags &= ~CONTEXT_ARM;  /* get rid of CPU id */
    if (flags & CONTEXT_CONTROL)
    {
        to->Sp      = from->Sp;
        to->Lr      = from->Lr;
        to->Pc      = from->Pc;
        to->Cpsr    = from->Cpsr;
    }
    if (flags & CONTEXT_INTEGER)
    {
        to->R0  = from->R0;
        to->R1  = from->R1;
        to->R2  = from->R2;
        to->R3  = from->R3;
        to->R4  = from->R4;
        to->R5  = from->R5;
        to->R6  = from->R6;
        to->R7  = from->R7;
        to->R8  = from->R8;
        to->R9  = from->R9;
        to->R10 = from->R10;
        to->R11 = from->R11;
        to->R12 = from->R12;
    }
    if (flags & CONTEXT_FLOATING_POINT)
    {
        to->Fpscr = from->Fpscr;
        memcpy( to->u.D, from->u.D, sizeof(to->u.D) );
    }
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
    if (self && ret == STATUS_SUCCESS) set_cpu_context( context );
    return ret;
}


/***********************************************************************
 *              NtGetContextThread  (NTDLL.@)
 *              ZwGetContextThread  (NTDLL.@)
 */
NTSTATUS WINAPI NtGetContextThread( HANDLE handle, CONTEXT *context )
{
    NTSTATUS ret;
    DWORD needed_flags = context->ContextFlags;
    BOOL self = (handle == GetCurrentThread());

    if (!self)
    {
        context_t server_context;
        unsigned int server_flags = get_server_context_flags( context->ContextFlags );

        if ((ret = get_thread_context( handle, &server_context, server_flags, &self ))) return ret;
        if ((ret = context_from_server( context, &server_context ))) return ret;
        needed_flags &= ~context->ContextFlags;
    }

    if (self && needed_flags)
    {
        CONTEXT ctx;
        RtlCaptureContext( &ctx );
        copy_context( context, &ctx, ctx.ContextFlags & needed_flags );
        context->ContextFlags |= ctx.ContextFlags & needed_flags;
    }
    return STATUS_SUCCESS;
}


extern void raise_func_trampoline_thumb( EXCEPTION_RECORD *rec, CONTEXT *context, raise_func func );
__ASM_GLOBAL_FUNC( raise_func_trampoline_thumb,
                   ".thumb\n\t"
                   "blx r2\n\t"
                   "bkpt")

extern void raise_func_trampoline_arm( EXCEPTION_RECORD *rec, CONTEXT *context, raise_func func );
__ASM_GLOBAL_FUNC( raise_func_trampoline_arm,
                   ".arm\n\t"
                   "blx r2\n\t"
                   "bkpt")

/***********************************************************************
 *           setup_exception_record
 *
 * Setup the exception record and context on the thread stack.
 */
static EXCEPTION_RECORD *setup_exception( ucontext_t *sigcontext, raise_func func )
{
    struct stack_layout
    {
        CONTEXT           context;
        EXCEPTION_RECORD  rec;
    } *stack;
    DWORD exception_code = 0;

    stack = (struct stack_layout *)(SP_sig(sigcontext) & ~3);
    stack--;  /* push the stack_layout structure */

    stack->rec.ExceptionRecord  = NULL;
    stack->rec.ExceptionCode    = exception_code;
    stack->rec.ExceptionFlags   = EXCEPTION_CONTINUABLE;
    stack->rec.ExceptionAddress = (LPVOID)PC_sig(sigcontext);
    stack->rec.NumberParameters = 0;

    save_context( &stack->context, sigcontext );

    /* now modify the sigcontext to return to the raise function */
    SP_sig(sigcontext) = (DWORD)stack;
    if (CPSR_sig(sigcontext) & 0x20)
        PC_sig(sigcontext) = (DWORD)raise_func_trampoline_thumb;
    else
        PC_sig(sigcontext) = (DWORD)raise_func_trampoline_arm;
    REGn_sig(0, sigcontext) = (DWORD)&stack->rec;  /* first arg for raise_func */
    REGn_sig(1, sigcontext) = (DWORD)&stack->context; /* second arg for raise_func */
    REGn_sig(2, sigcontext) = (DWORD)func; /* the raise_func as third arg for the trampoline */


    return &stack->rec;
}

/**********************************************************************
 *		raise_segv_exception
 */
static void WINAPI raise_segv_exception( EXCEPTION_RECORD *rec, CONTEXT *context )
{
    NTSTATUS status;

    switch(rec->ExceptionCode)
    {
    case EXCEPTION_ACCESS_VIOLATION:
        if (rec->NumberParameters == 2)
        {
            if (!(rec->ExceptionCode = virtual_handle_fault( (void *)rec->ExceptionInformation[1],
                                                             rec->ExceptionInformation[0], FALSE )))
                goto done;
        }
        break;
    }
    status = NtRaiseException( rec, context, TRUE );
    if (status) raise_status( status, rec );
done:
    set_cpu_context( context );
}

/**********************************************************************
 *           call_stack_handlers
 *
 * Call the stack handlers chain.
 */
static NTSTATUS call_stack_handlers( EXCEPTION_RECORD *rec, CONTEXT *context )
{
    EXCEPTION_REGISTRATION_RECORD *frame, *dispatch, *nested_frame;
    DWORD res;

    frame = NtCurrentTeb()->Tib.ExceptionList;
    nested_frame = NULL;
    while (frame != (EXCEPTION_REGISTRATION_RECORD*)~0UL)
    {
        /* Check frame address */
        if (!is_valid_frame( frame ))
        {
            rec->ExceptionFlags |= EH_STACK_INVALID;
            break;
        }

        /* Call handler */
        TRACE( "calling handler at %p code=%x flags=%x\n",
               frame->Handler, rec->ExceptionCode, rec->ExceptionFlags );
        res = frame->Handler( rec, frame, context, &dispatch );
        TRACE( "handler at %p returned %x\n", frame->Handler, res );

        if (frame == nested_frame)
        {
            /* no longer nested */
            nested_frame = NULL;
            rec->ExceptionFlags &= ~EH_NESTED_CALL;
        }

        switch(res)
        {
        case ExceptionContinueExecution:
            if (!(rec->ExceptionFlags & EH_NONCONTINUABLE)) return STATUS_SUCCESS;
            return STATUS_NONCONTINUABLE_EXCEPTION;
        case ExceptionContinueSearch:
            break;
        case ExceptionNestedException:
            if (nested_frame < dispatch) nested_frame = dispatch;
            rec->ExceptionFlags |= EH_NESTED_CALL;
            break;
        default:
            return STATUS_INVALID_DISPOSITION;
        }
        frame = frame->Prev;
    }
    return STATUS_UNHANDLED_EXCEPTION;
}


/*******************************************************************
 *		raise_exception
 *
 * Implementation of NtRaiseException.
 */
static NTSTATUS raise_exception( EXCEPTION_RECORD *rec, CONTEXT *context, BOOL first_chance )
{
    NTSTATUS status;

    if (first_chance)
    {
        DWORD c;

        TRACE( "code=%x flags=%x addr=%p pc=%08x tid=%04x\n",
               rec->ExceptionCode, rec->ExceptionFlags, rec->ExceptionAddress,
               context->Pc, GetCurrentThreadId() );
        for (c = 0; c < rec->NumberParameters; c++)
            TRACE( " info[%d]=%08lx\n", c, rec->ExceptionInformation[c] );
        if (rec->ExceptionCode == EXCEPTION_WINE_STUB)
        {
            if (rec->ExceptionInformation[1] >> 16)
                MESSAGE( "wine: Call from %p to unimplemented function %s.%s, aborting\n",
                         rec->ExceptionAddress,
                         (char*)rec->ExceptionInformation[0], (char*)rec->ExceptionInformation[1] );
            else
                MESSAGE( "wine: Call from %p to unimplemented function %s.%ld, aborting\n",
                         rec->ExceptionAddress,
                         (char*)rec->ExceptionInformation[0], rec->ExceptionInformation[1] );
        }
        else
        {
            TRACE( " r0=%08x r1=%08x r2=%08x r3=%08x r4=%08x r5=%08x\n",
                   context->R0, context->R1, context->R2, context->R3, context->R4, context->R5 );
            TRACE( " r6=%08x r7=%08x r8=%08x r9=%08x r10=%08x r11=%08x\n",
                   context->R6, context->R7, context->R8, context->R9, context->R10, context->R11 );
            TRACE( " r12=%08x sp=%08x lr=%08x pc=%08x cpsr=%08x\n",
                   context->R12, context->Sp, context->Lr, context->Pc, context->Cpsr );
        }

        status = send_debug_event( rec, TRUE, context );
        if (status == DBG_CONTINUE || status == DBG_EXCEPTION_HANDLED)
            return STATUS_SUCCESS;

        if (call_vectored_handlers( rec, context ) == EXCEPTION_CONTINUE_EXECUTION)
            return STATUS_SUCCESS;

        if ((status = call_stack_handlers( rec, context )) != STATUS_UNHANDLED_EXCEPTION)
            return status;
    }

    /* last chance exception */

    status = send_debug_event( rec, FALSE, context );
    if (status != DBG_CONTINUE)
    {
        if (rec->ExceptionFlags & EH_STACK_INVALID)
            ERR("Exception frame is not in stack limits => unable to dispatch exception.\n");
        else if (rec->ExceptionCode == STATUS_NONCONTINUABLE_EXCEPTION)
            ERR("Process attempted to continue execution after noncontinuable exception.\n");
        else
            ERR("Unhandled exception code %x flags %x addr %p\n",
                rec->ExceptionCode, rec->ExceptionFlags, rec->ExceptionAddress );
        NtTerminateProcess( NtCurrentProcess(), rec->ExceptionCode );
    }
    return STATUS_SUCCESS;
}


/**********************************************************************
 *		segv_handler
 *
 * Handler for SIGSEGV and related errors.
 */
static void segv_handler( int signal, siginfo_t *info, void *ucontext )
{
    EXCEPTION_RECORD *rec;
    ucontext_t *context = ucontext;

    /* check for page fault inside the thread stack */
    if (get_trap_code(signal, context) == TRAP_ARM_PAGEFLT)
    {
        switch (virtual_handle_stack_fault( info->si_addr ))
        {
        case 1:  /* handled */
            return;
        case -1:  /* overflow */
            rec = setup_exception( context, raise_segv_exception );
            rec->ExceptionCode = EXCEPTION_STACK_OVERFLOW;
            return;
        }
    }

    rec = setup_exception( context, raise_segv_exception );
    if (rec->ExceptionCode == EXCEPTION_STACK_OVERFLOW) return;

    switch(get_trap_code(signal, context))
    {
    case TRAP_ARM_PRIVINFLT:   /* Invalid opcode exception */
        rec->ExceptionCode = EXCEPTION_ILLEGAL_INSTRUCTION;
        break;
    case TRAP_ARM_PAGEFLT:  /* Page fault */
        rec->ExceptionCode = EXCEPTION_ACCESS_VIOLATION;
        rec->NumberParameters = 2;
        rec->ExceptionInformation[0] = (get_error_code(context) & 0x800) != 0;
        rec->ExceptionInformation[1] = (ULONG_PTR)info->si_addr;
        break;
    case TRAP_ARM_ALIGNFLT:  /* Alignment check exception */
        rec->ExceptionCode = EXCEPTION_DATATYPE_MISALIGNMENT;
        break;
    case TRAP_ARM_UNKNOWN:   /* Unknown fault code */
        rec->ExceptionCode = EXCEPTION_ACCESS_VIOLATION;
        rec->NumberParameters = 2;
        rec->ExceptionInformation[0] = 0;
        rec->ExceptionInformation[1] = 0xffffffff;
        break;
    default:
        ERR("Got unexpected trap %d\n", get_trap_code(signal, context));
        rec->ExceptionCode = EXCEPTION_ILLEGAL_INSTRUCTION;
        break;
    }
}

/**********************************************************************
 *		trap_handler
 *
 * Handler for SIGTRAP.
 */
static void trap_handler( int signal, siginfo_t *info, void *ucontext )
{
    EXCEPTION_RECORD rec;
    CONTEXT context;
    NTSTATUS status;

    switch ( info->si_code )
    {
    case TRAP_TRACE:
        rec.ExceptionCode = EXCEPTION_SINGLE_STEP;
        break;
    case TRAP_BRKPT:
    default:
        rec.ExceptionCode = EXCEPTION_BREAKPOINT;
        break;
    }

    save_context( &context, ucontext );
    rec.ExceptionFlags   = EXCEPTION_CONTINUABLE;
    rec.ExceptionRecord  = NULL;
    rec.ExceptionAddress = (LPVOID)context.Pc;
    rec.NumberParameters = 0;
    status = raise_exception( &rec, &context, TRUE );
    if (status) raise_status( status, &rec );
    restore_context( &context, ucontext );
}

/**********************************************************************
 *		fpe_handler
 *
 * Handler for SIGFPE.
 */
static void fpe_handler( int signal, siginfo_t *siginfo, void *sigcontext )
{
    EXCEPTION_RECORD rec;
    CONTEXT context;
    NTSTATUS status;

    save_fpu( &context, sigcontext );
    save_context( &context, sigcontext );

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
    rec.ExceptionFlags   = EXCEPTION_CONTINUABLE;
    rec.ExceptionRecord  = NULL;
    rec.ExceptionAddress = (LPVOID)context.Pc;
    rec.NumberParameters = 0;
    status = raise_exception( &rec, &context, TRUE );
    if (status) raise_status( status, &rec );

    restore_context( &context, sigcontext );
    restore_fpu( &context, sigcontext );
}

/**********************************************************************
 *		int_handler
 *
 * Handler for SIGINT.
 */
static void int_handler( int signal, siginfo_t *siginfo, void *sigcontext )
{
    if (!dispatch_signal(SIGINT))
    {
        EXCEPTION_RECORD rec;
        CONTEXT context;
        NTSTATUS status;

        save_context( &context, sigcontext );
        rec.ExceptionCode    = CONTROL_C_EXIT;
        rec.ExceptionFlags   = EXCEPTION_CONTINUABLE;
        rec.ExceptionRecord  = NULL;
        rec.ExceptionAddress = (LPVOID)context.Pc;
        rec.NumberParameters = 0;
        status = raise_exception( &rec, &context, TRUE );
        if (status) raise_status( status, &rec );
        restore_context( &context, sigcontext );
    }
}


/**********************************************************************
 *		abrt_handler
 *
 * Handler for SIGABRT.
 */
static void abrt_handler( int signal, siginfo_t *siginfo, void *sigcontext )
{
    EXCEPTION_RECORD rec;
    CONTEXT context;
    NTSTATUS status;

    save_context( &context, sigcontext );
    rec.ExceptionCode    = EXCEPTION_WINE_ASSERTION;
    rec.ExceptionFlags   = EH_NONCONTINUABLE;
    rec.ExceptionRecord  = NULL;
    rec.ExceptionAddress = (LPVOID)context.Pc;
    rec.NumberParameters = 0;
    status = raise_exception( &rec, &context, TRUE );
    if (status) raise_status( status, &rec );
    restore_context( &context, sigcontext );
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


/***********************************************************************
 *           __wine_set_signal_handler   (NTDLL.@)
 */
int CDECL __wine_set_signal_handler(unsigned int sig, wine_signal_handler wsh)
{
    if (sig >= ARRAY_SIZE(handlers)) return -1;
    if (handlers[sig] != NULL) return -2;
    handlers[sig] = wsh;
    return 0;
}


/**********************************************************************
 *             signal_init_threading
 */
void signal_init_threading(void)
{
    pthread_key_create( &teb_key, NULL );
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
#if defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_8A__)
    /* Win32/ARM applications expect the TEB pointer to be in the TPIDRURW register. */
    __asm__ __volatile__( "mcr p15, 0, %0, c13, c0, 2" : : "r" (teb) );
#endif

    pthread_setspecific( teb_key, teb );
}


/**********************************************************************
 *		signal_init_process
 */
void signal_init_process(void)
{
    struct sigaction sig_act;

    sig_act.sa_mask = server_block_set;
    sig_act.sa_flags = SA_RESTART | SA_SIGINFO;

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

    sig_act.sa_sigaction = segv_handler;
    if (sigaction( SIGSEGV, &sig_act, NULL ) == -1) goto error;
    if (sigaction( SIGILL, &sig_act, NULL ) == -1) goto error;
#ifdef SIGBUS
    if (sigaction( SIGBUS, &sig_act, NULL ) == -1) goto error;
#endif

#ifdef SIGTRAP
    sig_act.sa_sigaction = trap_handler;
    if (sigaction( SIGTRAP, &sig_act, NULL ) == -1) goto error;
#endif
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
 *            RtlUnwind  (NTDLL.@)
 */
void WINAPI RtlUnwind( void *endframe, void *target_ip, EXCEPTION_RECORD *rec, void *retval )
{
    CONTEXT context;
    EXCEPTION_RECORD record;
    EXCEPTION_REGISTRATION_RECORD *frame, *dispatch;
    DWORD res;

    RtlCaptureContext( &context );
    context.R0 = (DWORD)retval;

    /* build an exception record, if we do not have one */
    if (!rec)
    {
        record.ExceptionCode    = STATUS_UNWIND;
        record.ExceptionFlags   = 0;
        record.ExceptionRecord  = NULL;
        record.ExceptionAddress = (void *)context.Pc;
        record.NumberParameters = 0;
        rec = &record;
    }

    rec->ExceptionFlags |= EH_UNWINDING | (endframe ? 0 : EH_EXIT_UNWIND);

    TRACE( "code=%x flags=%x\n", rec->ExceptionCode, rec->ExceptionFlags );

    /* get chain of exception frames */
    frame = NtCurrentTeb()->Tib.ExceptionList;
    while ((frame != (EXCEPTION_REGISTRATION_RECORD*)~0UL) && (frame != endframe))
    {
        /* Check frame address */
        if (endframe && ((void*)frame > endframe))
            raise_status( STATUS_INVALID_UNWIND_TARGET, rec );

        if (!is_valid_frame( frame )) raise_status( STATUS_BAD_STACK, rec );

        /* Call handler */
        TRACE( "calling handler at %p code=%x flags=%x\n",
               frame->Handler, rec->ExceptionCode, rec->ExceptionFlags );
        res = frame->Handler(rec, frame, &context, &dispatch);
        TRACE( "handler at %p returned %x\n", frame->Handler, res );

        switch(res)
        {
        case ExceptionContinueSearch:
            break;
        case ExceptionCollidedUnwind:
            frame = dispatch;
            break;
        default:
            raise_status( STATUS_INVALID_DISPOSITION, rec );
            break;
        }
        frame = __wine_pop_frame( frame );
    }
}

/*******************************************************************
 *		NtRaiseException (NTDLL.@)
 */
NTSTATUS WINAPI NtRaiseException( EXCEPTION_RECORD *rec, CONTEXT *context, BOOL first_chance )
{
    NTSTATUS status = raise_exception( rec, context, first_chance );
    if (status == STATUS_SUCCESS) NtSetContextThread( GetCurrentThread(), context );
    return status;
}

/***********************************************************************
 *		RtlRaiseException (NTDLL.@)
 */
void WINAPI RtlRaiseException( EXCEPTION_RECORD *rec )
{
    CONTEXT context;
    NTSTATUS status;

    RtlCaptureContext( &context );
    rec->ExceptionAddress = (LPVOID)context.Pc;
    status = raise_exception( rec, &context, TRUE );
    if (status) raise_status( status, rec );
}

/*************************************************************************
 *             RtlCaptureStackBackTrace (NTDLL.@)
 */
USHORT WINAPI RtlCaptureStackBackTrace( ULONG skip, ULONG count, PVOID *buffer, ULONG *hash )
{
    FIXME( "(%d, %d, %p, %p) stub!\n", skip, count, buffer, hash );
    return 0;
}

/***********************************************************************
 *           call_thread_entry_point
 */
static void call_thread_entry_point( LPTHREAD_START_ROUTINE entry, void *arg )
{
    __TRY
    {
        TRACE_(relay)( "\1Starting thread proc %p (arg=%p)\n", entry, arg );
        RtlExitUserThread( entry( arg ));
    }
    __EXCEPT(call_unhandled_exception_filter)
    {
        NtTerminateThread( GetCurrentThread(), GetExceptionCode() );
    }
    __ENDTRY
    abort();  /* should not be reached */
}

extern void DECLSPEC_NORETURN start_thread( LPTHREAD_START_ROUTINE entry, void *arg, BOOL suspend,
                                            void *relay, TEB *teb );
__ASM_GLOBAL_FUNC( start_thread,
                   ".arm\n\t"
                   "push {r4-r12,lr}\n\t"
                   /* store exit frame */
                   "ldr r4, [sp, #40]\n\t"    /* teb */
                   "str sp, [r4, #0x1d4]\n\t" /* teb->SystemReserved2 */
                   /* switch to thread stack */
                   "ldr r4, [r4, #4]\n\t"     /* teb->Tib.StackBase */
                   "sub sp, r4, #0x1000\n\t"
                   /* attach dlls */
                   "bl " __ASM_NAME("attach_thread") "\n\t"
                   "mov sp, r0\n\t"
                   /* clear the stack */
                   "and r0, #~0xff0\n\t"  /* round down to page size */
                   "bl " __ASM_NAME("virtual_clear_thread_stack") "\n\t"
                   /* switch to the initial context */
                   "mov r0, sp\n\t"
                   "b " __ASM_NAME("set_cpu_context") )

extern void DECLSPEC_NORETURN call_thread_exit_func( int status, void (*func)(int), TEB *teb );
__ASM_GLOBAL_FUNC( call_thread_exit_func,
                   ".arm\n\t"
                   "ldr r3, [r2, #0x1d4]\n\t"  /* teb->SystemReserved2 */
                   "mov ip, #0\n\t"
                   "str ip, [r2, #0x1d4]\n\t"
                   "cmp r3, ip\n\t"
                   "movne sp, r3\n\t"
                   "blx r1" )

/***********************************************************************
 *           init_thread_context
 */
static void init_thread_context( CONTEXT *context, LPTHREAD_START_ROUTINE entry, void *arg, void *relay )
{
    context->R0 = (DWORD)entry;
    context->R1 = (DWORD)arg;
    context->Sp = (DWORD)NtCurrentTeb()->Tib.StackBase;
    context->Pc = (DWORD)relay;
}


/***********************************************************************
 *           attach_thread
 */
PCONTEXT DECLSPEC_HIDDEN attach_thread( LPTHREAD_START_ROUTINE entry, void *arg,
                                        BOOL suspend, void *relay )
{
    CONTEXT *ctx;

    if (suspend)
    {
        CONTEXT context = { CONTEXT_ALL };

        init_thread_context( &context, entry, arg, relay );
        wait_suspend( &context );
        ctx = (CONTEXT *)((ULONG_PTR)context.Sp & ~15) - 1;
        *ctx = context;
    }
    else
    {
        ctx = (CONTEXT *)NtCurrentTeb()->Tib.StackBase - 1;
        init_thread_context( ctx, entry, arg, relay );
    }
    ctx->ContextFlags = CONTEXT_FULL;
    LdrInitializeThunk( ctx, (void **)&ctx->R0, 0, 0 );
    return ctx;
}


/***********************************************************************
 *           signal_start_thread
 *
 * Thread startup sequence:
 * signal_start_thread()
 *   -> thread_startup()
 *     -> call_thread_entry_point()
 */
void signal_start_thread( LPTHREAD_START_ROUTINE entry, void *arg, BOOL suspend )
{
    start_thread( entry, arg, suspend, call_thread_entry_point, NtCurrentTeb() );
}

/**********************************************************************
 *		signal_start_process
 *
 * Process startup sequence:
 * signal_start_process()
 *   -> thread_startup()
 *     -> kernel32_start_process()
 */
void signal_start_process( LPTHREAD_START_ROUTINE entry, BOOL suspend )
{
    start_thread( entry, NtCurrentTeb()->Peb, suspend, kernel32_start_process, NtCurrentTeb() );
}

/***********************************************************************
 *           signal_exit_thread
 */
void signal_exit_thread( int status )
{
    call_thread_exit_func( status, exit_thread, NtCurrentTeb() );
}

/***********************************************************************
 *           signal_exit_process
 */
void signal_exit_process( int status )
{
    call_thread_exit_func( status, exit, NtCurrentTeb() );
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
 *              DbgBreakPoint   (NTDLL.@)
 */
void WINAPI DbgBreakPoint(void)
{
     kill(getpid(), SIGTRAP);
}

/**********************************************************************
 *              DbgUserBreakPoint   (NTDLL.@)
 */
void WINAPI DbgUserBreakPoint(void)
{
     kill(getpid(), SIGTRAP);
}

/**********************************************************************
 *           NtCurrentTeb   (NTDLL.@)
 */
TEB * WINAPI NtCurrentTeb(void)
{
    return pthread_getspecific( teb_key );
}

#endif  /* __arm__ */
