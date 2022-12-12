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

#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
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
#ifdef HAVE_LIBUNWIND
# define UNW_LOCAL_ONLY
# include <libunwind.h>
#endif
#ifdef HAVE_LINK_H
# include <link.h>
#endif

#define NONAMELESSUNION
#define NONAMELESSSTRUCT
#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winnt.h"
#include "winternl.h"
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
    UINT                  r0;             /* 000 */
    UINT                  r1;             /* 004 */
    UINT                  r2;             /* 008 */
    UINT                  r3;             /* 00c */
    UINT                  r4;             /* 010 */
    UINT                  r5;             /* 014 */
    UINT                  r6;             /* 018 */
    UINT                  r7;             /* 01c */
    UINT                  r8;             /* 020 */
    UINT                  r9;             /* 024 */
    UINT                  r10;            /* 028 */
    UINT                  r11;            /* 02c */
    UINT                  r12;            /* 030 */
    UINT                  pc;             /* 034 */
    UINT                  sp;             /* 038 */
    UINT                  lr;             /* 03c */
    UINT                  cpsr;           /* 040 */
    UINT                  restore_flags;  /* 044 */
    UINT                  fpscr;          /* 048 */
    struct syscall_frame *prev_frame;     /* 04c */
    SYSTEM_SERVICE_TABLE *syscall_table;  /* 050 */
    UINT                  align[3];       /* 054 */
    ULONGLONG             d[32];          /* 060 */
};

C_ASSERT( sizeof( struct syscall_frame ) == 0x160);

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

static BOOL is_inside_syscall( ucontext_t *sigcontext )
{
    return ((char *)SP_sig(sigcontext) >= (char *)ntdll_get_thread_data()->kernel_stack &&
            (char *)SP_sig(sigcontext) <= (char *)arm_thread_data()->syscall_frame);
}

extern void raise_func_trampoline( EXCEPTION_RECORD *rec, CONTEXT *context, void *dispatcher );

struct exidx_entry
{
    uint32_t addr;
    uint32_t data;
};

static uint32_t prel31_to_abs(const uint32_t *ptr)
{
    uint32_t prel31 = *ptr;
    uint32_t rel = prel31 | ((prel31 << 1) & 0x80000000);
    return (uintptr_t)ptr + rel;
}

static uint8_t get_byte(const uint32_t *ptr, int offset, int bytes)
{
    int word = offset >> 2;
    int byte = offset & 0x3;
    if (offset >= bytes)
        return 0xb0; /* finish opcode */
    return (ptr[word] >> (24 - 8*byte)) & 0xff;
}

static uint32_t get_uleb128(const uint32_t *ptr, int *offset, int bytes)
{
    int shift = 0;
    uint32_t val = 0;
    while (1)
    {
        uint8_t byte = get_byte(ptr, (*offset)++, bytes);
        val |= (byte & 0x7f) << shift;
        if ((byte & 0x80) == 0)
            break;
        shift += 7;
    }
    return val;
}

static void pop_regs(CONTEXT *context, uint32_t regs)
{
    int i;
    DWORD new_sp = 0;
    for (i = 0; i < 16; i++)
    {
        if (regs & (1U << i))
        {
            DWORD val = *(DWORD *)context->Sp;
            if (i != 13)
                (&context->R0)[i] = val;
            else
                new_sp = val;
            context->Sp += 4;
        }
    }
    if (regs & (1 << 13))
        context->Sp = new_sp;
}

static void pop_vfp(CONTEXT *context, int first, int last)
{
    int i;
    for (i = first; i <= last; i++)
    {
        context->u.D[i] = *(ULONGLONG *)context->Sp;
        context->Sp += 8;
    }
}

static uint32_t regmask(int first_bit, int n_bits)
{
    return ((1U << (n_bits + 1)) - 1) << first_bit;
}

/***********************************************************************
 *           ehabi_virtual_unwind
 */
static NTSTATUS ehabi_virtual_unwind( UINT ip, DWORD *frame, CONTEXT *context,
                                      const struct exidx_entry *entry,
                                      PEXCEPTION_ROUTINE *handler, void **handler_data )
{
    const uint32_t *ptr;
    const void *lsda = NULL;
    int compact_inline = 0;
    int offset = 0;
    int bytes = 0;
    int personality;
    int extra_words;
    int finish = 0;
    int set_pc = 0;
    UINT func_begin = prel31_to_abs(&entry->addr);

    *frame = context->Sp;

    TRACE( "ip %#x function %#x\n", ip, func_begin );

    if (entry->data == 1)
    {
        ERR("EXIDX_CANTUNWIND\n");
        return STATUS_UNSUCCESSFUL;
    }
    else if (entry->data & 0x80000000)
    {
        if ((entry->data & 0x7f000000) != 0)
        {
            ERR("compact inline EXIDX must have personality 0\n");
            return STATUS_UNSUCCESSFUL;
        }
        ptr = &entry->data;
        compact_inline = 1;
    }
    else
    {
        ptr = (uint32_t *)prel31_to_abs(&entry->data);
    }

    if ((*ptr & 0x80000000) == 0)
    {
        /* Generic */
        void *personality_func = (void *)prel31_to_abs(ptr);
        int words = (ptr[1] >> 24) & 0xff;
        lsda = ptr + 1 + words + 1;

        ERR("generic EHABI unwinding not supported\n");
        (void)personality_func;
        return STATUS_UNSUCCESSFUL;
    }

    /* Compact */

    personality = (*ptr >> 24) & 0x0f;
    switch (personality)
    {
    case 0:
        if (!compact_inline)
            lsda = ptr + 1;
        extra_words = 0;
        offset = 1;
        break;
    case 1:
        extra_words = (*ptr >> 16) & 0xff;
        lsda = ptr + extra_words + 1;
        offset = 2;
        break;
    case 2:
        extra_words = (*ptr >> 16) & 0xff;
        lsda = ptr + extra_words + 1;
        offset = 2;
        break;
    default:
        ERR("unsupported compact EXIDX personality %d\n", personality);
        return STATUS_UNSUCCESSFUL;
    }

    /* Not inspecting the descriptors */
    (void)lsda;

    bytes = 4 + 4*extra_words;
    while (offset < bytes && !finish)
    {
        uint8_t byte = get_byte(ptr, offset++, bytes);
        if ((byte & 0xc0) == 0x00)
        {
            /* Increment Sp */
            context->Sp += (byte & 0x3f) * 4 + 4;
        }
        else if ((byte & 0xc0) == 0x40)
        {
            /* Decrement Sp */
            context->Sp -= (byte & 0x3f) * 4 + 4;
        }
        else if ((byte & 0xf0) == 0x80)
        {
            /* Pop {r4-r15} based on register mask */
            int regs = ((byte & 0x0f) << 8) | get_byte(ptr, offset++, bytes);
            if (!regs)
            {
                ERR("refuse to unwind\n");
                return STATUS_UNSUCCESSFUL;
            }
            regs <<= 4;
            pop_regs(context, regs);
            if (regs & (1 << 15))
                set_pc = 1;
        }
        else if ((byte & 0xf0) == 0x90)
        {
            /* Restore Sp from other register */
            int reg = byte & 0x0f;
            if (reg == 13 || reg == 15)
            {
                ERR("reserved opcode\n");
                return STATUS_UNSUCCESSFUL;
            }
            context->Sp = (&context->R0)[reg];
        }
        else if ((byte & 0xf0) == 0xa0)
        {
            /* Pop r4-r(4+n) (+lr) */
            int n = byte & 0x07;
            int regs = regmask(4, n);
            if (byte & 0x08)
                regs |= 1 << 14;
            pop_regs(context, regs);
        }
        else if (byte == 0xb0)
        {
            finish = 1;
        }
        else if (byte == 0xb1)
        {
            /* Pop {r0-r3} based on register mask */
            int regs = get_byte(ptr, offset++, bytes);
            if (regs == 0 || (regs & 0xf0) != 0)
            {
                ERR("spare opcode\n");
                return STATUS_UNSUCCESSFUL;
            }
            pop_regs(context, regs);
        }
        else if (byte == 0xb2)
        {
            /* Increment Sp by a larger amount */
            int imm = get_uleb128(ptr, &offset, bytes);
            context->Sp += 0x204 + imm * 4;
        }
        else if (byte == 0xb3)
        {
            /* Pop VFP registers as if saved by FSTMFDX; this opcode
             * is deprecated. */
            ERR("FSTMFDX unsupported\n");
            return STATUS_UNSUCCESSFUL;
        }
        else if ((byte & 0xfc) == 0xb4)
        {
            ERR("spare opcode\n");
            return STATUS_UNSUCCESSFUL;
        }
        else if ((byte & 0xf8) == 0xb8)
        {
            /* Pop VFP registers as if saved by FSTMFDX; this opcode
             * is deprecated. */
            ERR("FSTMFDX unsupported\n");
            return STATUS_UNSUCCESSFUL;
        }
        else if ((byte & 0xf8) == 0xc0)
        {
            ERR("spare opcode / iWMMX\n");
            return STATUS_UNSUCCESSFUL;
        }
        else if ((byte & 0xfe) == 0xc8)
        {
            /* Pop VFP registers d(16+ssss)-d(16+ssss+cccc), or
             * d(0+ssss)-d(0+ssss+cccc) as if saved by VPUSH */
            int first, last;
            if ((byte & 0x01) == 0)
                first = 16;
            else
                first = 0;
            byte = get_byte(ptr, offset++, bytes);
            first += (byte & 0xf0) >> 4;
            last = first + (byte & 0x0f);
            if (last >= 32)
            {
                ERR("reserved opcode\n");
                return STATUS_UNSUCCESSFUL;
            }
            pop_vfp(context, first, last);
        }
        else if ((byte & 0xf8) == 0xc8)
        {
            ERR("spare opcode\n");
            return STATUS_UNSUCCESSFUL;
        }
        else if ((byte & 0xf8) == 0xd0)
        {
            /* Pop VFP registers d8-d(8+n) as if saved by VPUSH */
            int n = byte & 0x07;
            pop_vfp(context, 8, 8 + n);
        }
        else
        {
            ERR("spare opcode\n");
            return STATUS_UNSUCCESSFUL;
        }
    }
    if (offset > bytes)
    {
        ERR("truncated opcodes\n");
        return STATUS_UNSUCCESSFUL;
    }

    *handler      = NULL; /* personality */
    *handler_data = NULL; /* lsda */

    context->ContextFlags |= CONTEXT_UNWOUND_TO_CALL;
    if (!set_pc)
        context->Pc = context->Lr;

    /* There's no need to check for raise_func_trampoline and manually restore
     * Lr separately from Pc like with libunwind; the EHABI unwind info
     * describes how both of them are restored separately, and as long as
     * the unwind info restored Pc, it doesn't have to be set from Lr. */

    TRACE( "next function pc=%08lx\n", context->Pc );
    TRACE("  r0=%08lx  r1=%08lx  r2=%08lx  r3=%08lx\n",
          context->R0, context->R1, context->R2, context->R3 );
    TRACE("  r4=%08lx  r5=%08lx  r6=%08lx  r7=%08lx\n",
          context->R4, context->R5, context->R6, context->R7 );
    TRACE("  r8=%08lx  r9=%08lx r10=%08lx r11=%08lx\n",
          context->R8, context->R9, context->R10, context->R11 );
    TRACE(" r12=%08lx  sp=%08lx  lr=%08lx  pc=%08lx\n",
          context->R12, context->Sp, context->Lr, context->Pc );

    return STATUS_SUCCESS;
}

#ifdef linux
struct iterate_data
{
    ULONG_PTR ip;
    int failed;
    struct exidx_entry *entry;
};

static int contains_addr(struct dl_phdr_info *info, const ElfW(Phdr) *phdr, struct iterate_data *data)
{
    if (phdr->p_type != PT_LOAD)
        return 0;
    return data->ip >= info->dlpi_addr + phdr->p_vaddr && data->ip < info->dlpi_addr + phdr->p_vaddr + phdr->p_memsz;
}

static int check_exidx(struct dl_phdr_info *info, size_t info_size, void *arg)
{
    struct iterate_data *data = arg;
    int i;
    int found_addr;
    const ElfW(Phdr) *exidx = NULL;
    struct exidx_entry *begin, *end;

    if (info->dlpi_phnum == 0 || data->ip < info->dlpi_addr || data->failed)
        return 0;

    found_addr = 0;
    for (i = 0; i < info->dlpi_phnum; i++)
    {
        const ElfW(Phdr) *phdr = &info->dlpi_phdr[i];
        if (contains_addr(info, phdr, data))
            found_addr = 1;
        if (phdr->p_type == PT_ARM_EXIDX)
            exidx = phdr;
    }

    if (!found_addr || !exidx)
    {
        if (found_addr)
        {
            TRACE("found matching address in %s, but no EXIDX\n", info->dlpi_name);
            data->failed = 1;
        }
        return 0;
    }

    begin = (struct exidx_entry *)(info->dlpi_addr + exidx->p_vaddr);
    end = (struct exidx_entry *)(info->dlpi_addr + exidx->p_vaddr + exidx->p_memsz);
    if (data->ip < prel31_to_abs(&begin->addr))
    {
        TRACE("%lx before EXIDX start at %x\n", data->ip, prel31_to_abs(&begin->addr));
        data->failed = 1;
        return 0;
    }

    while (begin + 1 < end)
    {
        struct exidx_entry *mid = begin + (end - begin)/2;
        uint32_t abs_addr = prel31_to_abs(&mid->addr);
        if (abs_addr > data->ip)
        {
            end = mid;
        }
        else if (abs_addr < data->ip)
        {
            begin = mid;
        }
        else
        {
            begin = mid;
            end = mid + 1;
        }
    }

    data->entry = begin;
    TRACE("found %lx in %s, base %x, entry %p with addr %x (rel %x) data %x\n",
          data->ip, info->dlpi_name, info->dlpi_addr, begin,
          prel31_to_abs(&begin->addr),
          prel31_to_abs(&begin->addr) - info->dlpi_addr, begin->data);
    return 1;
}

static const struct exidx_entry *find_exidx_entry( void *ip )
{
    struct iterate_data data = {};

    data.ip = (ULONG_PTR)ip;
    data.failed = 0;
    data.entry = NULL;
    dl_iterate_phdr(check_exidx, &data);

    return data.entry;
}
#endif

#ifdef HAVE_LIBUNWIND
static NTSTATUS libunwind_virtual_unwind( DWORD ip, DWORD *frame, CONTEXT *context,
                                          PEXCEPTION_ROUTINE *handler, void **handler_data )
{
    unw_context_t unw_context;
    unw_cursor_t cursor;
    unw_proc_info_t info;
    int rc, i;

    for (i = 0; i <= 12; i++)
        unw_context.regs[i] = (&context->R0)[i];
    unw_context.regs[13] = context->Sp;
    unw_context.regs[14] = context->Lr;
    unw_context.regs[15] = context->Pc;
    rc = unw_init_local( &cursor, &unw_context );

    if (rc != UNW_ESUCCESS)
    {
        WARN( "setup failed: %d\n", rc );
        return STATUS_INVALID_DISPOSITION;
    }
    rc = unw_get_proc_info( &cursor, &info );
    if (UNW_ENOINFO < 0) rc = -rc;  /* LLVM libunwind has negative error codes */
    if (rc != UNW_ESUCCESS && rc != -UNW_ENOINFO)
    {
        WARN( "failed to get info: %d\n", rc );
        return STATUS_INVALID_DISPOSITION;
    }
    if (rc == -UNW_ENOINFO || ip < info.start_ip || ip > info.end_ip)
    {
        NTSTATUS status = context->Pc != context->Lr ?
                          STATUS_SUCCESS : STATUS_INVALID_DISPOSITION;
        TRACE( "no info found for %x ip %x-%x, %s\n",
               ip, info.start_ip, info.end_ip, status == STATUS_SUCCESS ?
               "assuming leaf function" : "error, stuck" );
        *handler = NULL;
        *frame = context->Sp;
        context->Pc = context->Lr;
        context->ContextFlags |= CONTEXT_UNWOUND_TO_CALL;
        return status;
    }

    TRACE( "ip %#x function %#lx-%#lx personality %#lx lsda %#lx fde %#lx\n",
           ip, (unsigned long)info.start_ip, (unsigned long)info.end_ip, (unsigned long)info.handler,
           (unsigned long)info.lsda, (unsigned long)info.unwind_info );

    rc = unw_step( &cursor );
    if (rc < 0)
    {
        WARN( "failed to unwind: %d %d\n", rc, UNW_ENOINFO );
        return STATUS_INVALID_DISPOSITION;
    }

    *handler      = (void *)info.handler;
    *handler_data = (void *)info.lsda;
    *frame        = context->Sp;

    for (i = 0; i <= 12; i++)
        unw_get_reg( &cursor, UNW_ARM_R0 + i, (unw_word_t *)&(&context->R0)[i] );
    unw_get_reg( &cursor, UNW_ARM_R13, (unw_word_t *)&context->Sp );
    unw_get_reg( &cursor, UNW_ARM_R14, (unw_word_t *)&context->Lr );
    unw_get_reg( &cursor, UNW_REG_IP,  (unw_word_t *)&context->Pc );
    context->ContextFlags |= CONTEXT_UNWOUND_TO_CALL;

    if ((info.start_ip & ~(unw_word_t)1) ==
        ((unw_word_t)raise_func_trampoline & ~(unw_word_t)1)) {
        /* raise_func_trampoline stores the original Lr at the bottom of the
         * stack. The unwinder normally can't restore both Pc and Lr to
         * individual values, thus do that manually here.
         * (The function we unwind to might be a leaf function that hasn't
         * backed up its own original Lr value on the stack.) */
        const DWORD *orig_lr = (const DWORD *) *frame;
        context->Lr = *orig_lr;
    }

    TRACE( "next function pc=%08lx%s\n", context->Pc, rc ? "" : " (last frame)" );
    TRACE("  r0=%08lx  r1=%08lx  r2=%08lx  r3=%08lx\n",
          context->R0, context->R1, context->R2, context->R3 );
    TRACE("  r4=%08lx  r5=%08lx  r6=%08lx  r7=%08lx\n",
          context->R4, context->R5, context->R6, context->R7 );
    TRACE("  r8=%08lx  r9=%08lx r10=%08lx r11=%08lx\n",
          context->R8, context->R9, context->R10, context->R11 );
    TRACE(" r12=%08lx  sp=%08lx  lr=%08lx  pc=%08lx\n",
          context->R12, context->Sp, context->Lr, context->Pc );
    return STATUS_SUCCESS;
}
#endif

/***********************************************************************
 *           unwind_builtin_dll
 */
NTSTATUS unwind_builtin_dll( void *args )
{
    struct unwind_builtin_dll_params *params = args;
    DISPATCHER_CONTEXT *dispatch = params->dispatch;
    CONTEXT *context = params->context;
    DWORD ip = context->Pc - (dispatch->ControlPcIsUnwound ? 2 : 0);
#ifdef linux
    const struct exidx_entry *entry = find_exidx_entry( (void *)ip );

    if (entry)
        return ehabi_virtual_unwind( ip, &dispatch->EstablisherFrame, context, entry,
                                     &dispatch->LanguageHandler, &dispatch->HandlerData );
#endif
#ifdef HAVE_LIBUNWIND
    return libunwind_virtual_unwind( ip, &dispatch->EstablisherFrame, context,
                                     &dispatch->LanguageHandler, &dispatch->HandlerData );
#else
    ERR("libunwind not available, unable to unwind\n");
    return STATUS_INVALID_DISPOSITION;
#endif
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
 *           get_udf_immediate
 *
 * Get the immediate operand if the PC is at a UDF instruction.
 */
static inline int get_udf_immediate( const ucontext_t *sigcontext )
{
    if (CPSR_sig(sigcontext) & 0x20)
    {
        WORD thumb_insn = *(WORD *)PC_sig(sigcontext);
        if ((thumb_insn >> 8) == 0xde) return thumb_insn & 0xff;
        if ((thumb_insn & 0xfff0) == 0xf7f0)  /* udf.w */
        {
            WORD ext = *(WORD *)(PC_sig(sigcontext) + 2);
            if ((ext & 0xf000) == 0xa000) return ((thumb_insn & 0xf) << 12) | (ext & 0x0fff);
        }
    }
    else
    {
        DWORD arm_insn = *(DWORD *)PC_sig(sigcontext);
        if ((arm_insn & 0xfff000f0) == 0xe7f000f0)
        {
            return ((arm_insn >> 4) & 0xfff0) | (arm_insn & 0xf);
        }
    }
    return -1;
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
 *           signal_set_full_context
 */
NTSTATUS signal_set_full_context( CONTEXT *context )
{
    NTSTATUS status = NtSetContextThread( GetCurrentThread(), context );

    if (!status && (context->ContextFlags & CONTEXT_INTEGER) == CONTEXT_INTEGER)
        arm_thread_data()->syscall_frame->restore_flags |= CONTEXT_INTEGER;
    return status;
}


/***********************************************************************
 *              get_native_context
 */
void *get_native_context( CONTEXT *context )
{
    return context;
}


/***********************************************************************
 *              get_wow_context
 */
void *get_wow_context( CONTEXT *context )
{
    return NULL;
}


/***********************************************************************
 *              NtSetContextThread  (NTDLL.@)
 *              ZwSetContextThread  (NTDLL.@)
 */
NTSTATUS WINAPI NtSetContextThread( HANDLE handle, const CONTEXT *context )
{
    NTSTATUS ret;
    struct syscall_frame *frame = arm_thread_data()->syscall_frame;
    DWORD flags = context->ContextFlags & ~CONTEXT_ARM;
    BOOL self = (handle == GetCurrentThread());

    if (!self)
    {
        ret = set_thread_context( handle, context, &self, IMAGE_FILE_MACHINE_ARMNT );
        if (ret || !self) return ret;
    }
    if (flags & CONTEXT_INTEGER)
    {
        frame->r0  = context->R0;
        frame->r1  = context->R1;
        frame->r2  = context->R2;
        frame->r3  = context->R3;
        frame->r4  = context->R4;
        frame->r5  = context->R5;
        frame->r6  = context->R6;
        frame->r7  = context->R7;
        frame->r8  = context->R8;
        frame->r9  = context->R9;
        frame->r10 = context->R10;
        frame->r11 = context->R11;
        frame->r12 = context->R12;
    }
    if (flags & CONTEXT_CONTROL)
    {
        frame->sp = context->Sp;
        frame->lr = context->Lr;
        frame->pc = context->Pc & ~1;
        frame->cpsr = context->Cpsr;
        if (context->Cpsr & 0x20) frame->pc |= 1; /* thumb */
    }
    if (flags & CONTEXT_FLOATING_POINT)
    {
        frame->fpscr = context->Fpscr;
        memcpy( frame->d, context->u.D, sizeof(context->u.D) );
    }
    frame->restore_flags |= flags & ~CONTEXT_INTEGER;
    return STATUS_SUCCESS;
}


/***********************************************************************
 *              NtGetContextThread  (NTDLL.@)
 *              ZwGetContextThread  (NTDLL.@)
 */
NTSTATUS WINAPI NtGetContextThread( HANDLE handle, CONTEXT *context )
{
    struct syscall_frame *frame = arm_thread_data()->syscall_frame;
    DWORD needed_flags = context->ContextFlags & ~CONTEXT_ARM;
    BOOL self = (handle == GetCurrentThread());

    if (!self)
    {
        NTSTATUS ret = get_thread_context( handle, &context, &self, IMAGE_FILE_MACHINE_ARMNT );
        if (ret || !self) return ret;
    }

    if (needed_flags & CONTEXT_INTEGER)
    {
        context->R0  = frame->r0;
        context->R1  = frame->r1;
        context->R2  = frame->r2;
        context->R3  = frame->r3;
        context->R4  = frame->r4;
        context->R5  = frame->r5;
        context->R6  = frame->r6;
        context->R7  = frame->r7;
        context->R8  = frame->r8;
        context->R9  = frame->r9;
        context->R10 = frame->r10;
        context->R11 = frame->r11;
        context->R12 = frame->r12;
        context->ContextFlags |= CONTEXT_INTEGER;
    }
    if (needed_flags & CONTEXT_CONTROL)
    {
        context->Sp   = frame->sp;
        context->Lr   = frame->lr;
        context->Pc   = frame->pc;
        context->Cpsr = frame->cpsr;
        context->ContextFlags |= CONTEXT_CONTROL;
    }
    if (needed_flags & CONTEXT_FLOATING_POINT)
    {
        context->Fpscr = frame->fpscr;
        memcpy( context->u.D, frame->d, sizeof(frame->d) );
        context->ContextFlags |= CONTEXT_FLOATING_POINT;
    }
    return STATUS_SUCCESS;
}


/***********************************************************************
 *              set_thread_wow64_context
 */
NTSTATUS set_thread_wow64_context( HANDLE handle, const void *ctx, ULONG size )
{
    return STATUS_INVALID_INFO_CLASS;
}


/***********************************************************************
 *              get_thread_wow64_context
 */
NTSTATUS get_thread_wow64_context( HANDLE handle, void *ctx, ULONG size )
{
    return STATUS_INVALID_INFO_CLASS;
}


__ASM_GLOBAL_FUNC( raise_func_trampoline,
                   "push {r12,lr}\n\t" /* (Padding +) Pc in the original frame */
                   "ldr r3, [r1, #0x38]\n\t" /* context->Sp */
                   "push {r3}\n\t" /* Original Sp */
                   __ASM_CFI(".cfi_escape 0x0f,0x03,0x7D,0x04,0x06\n\t") /* CFA, DW_OP_breg13 + 0x04, DW_OP_deref */
                   __ASM_CFI(".cfi_escape 0x10,0x0e,0x02,0x7D,0x0c\n\t") /* LR, DW_OP_breg13 + 0x0c */
                   __ASM_EHABI(".save {sp}\n\t")
                   __ASM_EHABI(".pad #-12\n\t")
                   __ASM_EHABI(".save {pc}\n\t")
                   __ASM_EHABI(".pad #8\n\t")
                   __ASM_EHABI(".save {lr}\n\t")
                   /* We can't express restoring both Pc and Lr with CFI
                    * directives, but we manually load Lr from the stack
                    * in unwind_builtin_dll above. */
                   "ldr r3, [r1, #0x3c]\n\t" /* context->Lr */
                   "push {r3}\n\t" /* Original Lr */
                   "blx r2\n\t"
                   "udf #0")

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
    LR_sig(sigcontext) = context.Pc;
    PC_sig(sigcontext) = (DWORD)raise_func_trampoline;
    if (PC_sig(sigcontext) & 1) CPSR_sig(sigcontext) |= 0x20;
    else CPSR_sig(sigcontext) &= ~0x20;
    REGn_sig(0, sigcontext) = (DWORD)&stack->rec;  /* first arg for KiUserExceptionDispatcher */
    REGn_sig(1, sigcontext) = (DWORD)&stack->context; /* second arg for KiUserExceptionDispatcher */
    REGn_sig(2, sigcontext) = (DWORD)pKiUserExceptionDispatcher;
}


/***********************************************************************
 *           call_user_apc_dispatcher
 */
NTSTATUS call_user_apc_dispatcher( CONTEXT *context, ULONG_PTR arg1, ULONG_PTR arg2, ULONG_PTR arg3,
                                   PNTAPCFUNC func, NTSTATUS status )
{
    struct syscall_frame *frame = arm_thread_data()->syscall_frame;
    ULONG sp = context ? context->Sp : frame->sp;
    struct apc_stack_layout
    {
        void   *func;
        void   *align;
        CONTEXT context;
    } *stack;

    sp &= ~15;
    stack = (struct apc_stack_layout *)sp - 1;
    if (context)
    {
        memmove( &stack->context, context, sizeof(stack->context) );
        NtSetContextThread( GetCurrentThread(), &stack->context );
    }
    else
    {
        stack->context.ContextFlags = CONTEXT_FULL;
        NtGetContextThread( GetCurrentThread(), &stack->context );
        stack->context.R0 = status;
    }
    frame->sp = (DWORD)stack;
    frame->pc = (DWORD)pKiUserApcDispatcher;
    frame->r0 = (DWORD)&stack->context;
    frame->r1 = arg1;
    frame->r2 = arg2;
    frame->r3 = arg3;
    stack->func = func;
    frame->restore_flags |= CONTEXT_CONTROL | CONTEXT_INTEGER;
    return status;
}


/***********************************************************************
 *           call_raise_user_exception_dispatcher
 */
void call_raise_user_exception_dispatcher(void)
{
    arm_thread_data()->syscall_frame->pc = (DWORD)pKiRaiseUserExceptionDispatcher;
}


/***********************************************************************
 *           call_user_exception_dispatcher
 */
NTSTATUS call_user_exception_dispatcher( EXCEPTION_RECORD *rec, CONTEXT *context )
{
    struct syscall_frame *frame = arm_thread_data()->syscall_frame;
    DWORD lr = frame->lr;
    DWORD sp = frame->sp;
    NTSTATUS status = NtSetContextThread( GetCurrentThread(), context );

    if (status) return status;
    frame->r0 = (DWORD)rec;
    frame->r1 = (DWORD)context;
    frame->pc = (DWORD)pKiUserExceptionDispatcher;
    frame->lr = lr;
    frame->sp = sp;
    frame->restore_flags |= CONTEXT_INTEGER | CONTEXT_CONTROL;
    return status;
}


/***********************************************************************
 *           call_user_mode_callback
 */
extern NTSTATUS CDECL call_user_mode_callback( void *func, void *stack, void **ret_ptr,
                                               ULONG *ret_len, TEB *teb );
__ASM_GLOBAL_FUNC( call_user_mode_callback,
                   "push {r2-r12,lr}\n\t"
                   "ldr r4, [sp, #0x30]\n\t"  /* teb */
                   "ldr r5, [r4]\n\t"         /* teb->Tib.ExceptionList */
                   "str r5, [sp, #0x28]\n\t"
#ifndef __SOFTFP__
                   "sub sp, sp, #0x90\n\t"
                   "mov r5, sp\n\t"
                   "vmrs r6, fpscr\n\t"
                   "vstm r5, {d8-d15}\n\t"
                   "str r6, [r5, #0x80]\n\t"
#endif
                   "sub sp, sp, #0x160\n\t"   /* sizeof(struct syscall_frame) + registers */
                   "ldr r5, [r4, #0x1d8]\n\t" /* arm_thread_data()->syscall_frame */
                   "str r5, [sp, #0x4c]\n\t"  /* frame->prev_frame */
                   "str sp, [r4, #0x1d8]\n\t" /* arm_thread_data()->syscall_frame */
                   "ldr r6, [r5, #0x50]\n\t"  /* prev_frame->syscall_table */
                   "str r6, [sp, #0x50]\n\t"  /* frame->syscall_table */
                   "mov ip, r0\n\t"
                   "mov sp, r1\n\t"
                   "pop {r0-r3}\n\t"
                   "bx ip" )


/***********************************************************************
 *           user_mode_callback_return
 */
extern void CDECL DECLSPEC_NORETURN user_mode_callback_return( void *ret_ptr, ULONG ret_len,
                                                               NTSTATUS status, TEB *teb );
__ASM_GLOBAL_FUNC( user_mode_callback_return,
                   "ldr r4, [r3, #0x1d8]\n\t" /* arm_thread_data()->syscall_frame */
                   "ldr r5, [r4, #0x4c]\n\t"  /* frame->prev_frame */
                   "str r5, [r3, #0x1d8]\n\t" /* arm_thread_data()->syscall_frame */
                   "add r5, r4, #0x160\n\t"
#ifndef __SOFTFP__
                   "vldm r5, {d8-d15}\n\t"
                   "ldr r6, [r5, #0x80]\n\t"
                   "vmsr fpscr, r6\n\t"
                   "add r5, r5, #0x90\n\t"
#endif
                   "mov sp, r5\n\t"
                   "ldr r5, [sp, #0x28]\n\t"
                   "str r5, [r3]\n\t"         /* teb->Tib.ExceptionList */
                   "pop {r5, r6}\n\t"         /* ret_ptr, ret_len */
                   "str r0, [r5]\n\t"         /* ret_ptr */
                   "str r1, [r6]\n\t"         /* ret_len */
                   "mov r0, r2\n\t"           /* status */
                   "pop {r4-r12,pc}" )


/***********************************************************************
 *           KeUserModeCallback
 */
NTSTATUS WINAPI KeUserModeCallback( ULONG id, const void *args, ULONG len, void **ret_ptr, ULONG *ret_len )
{
    struct syscall_frame *frame = arm_thread_data()->syscall_frame;
    void *args_data = (void *)((frame->sp - len) & ~15);
    ULONG_PTR *stack = args_data;

    /* if we have no syscall frame, call the callback directly */
    if ((char *)&frame < (char *)ntdll_get_thread_data()->kernel_stack ||
        (char *)&frame > (char *)arm_thread_data()->syscall_frame)
    {
        NTSTATUS (WINAPI *func)(const void *, ULONG) = ((void **)NtCurrentTeb()->Peb->KernelCallbackTable)[id];
        return func( args, len );
    }

    if ((char *)ntdll_get_thread_data()->kernel_stack + min_kernel_stack > (char *)&frame)
        return STATUS_STACK_OVERFLOW;

    memcpy( args_data, args, len );
    *(--stack) = 0;
    *(--stack) = len;
    *(--stack) = (ULONG_PTR)args_data;
    *(--stack) = id;

    return call_user_mode_callback( pKiUserCallbackDispatcher, stack, ret_ptr, ret_len, NtCurrentTeb() );
}


/***********************************************************************
 *           NtCallbackReturn  (NTDLL.@)
 */
NTSTATUS WINAPI NtCallbackReturn( void *ret_ptr, ULONG ret_len, NTSTATUS status )
{
    if (!arm_thread_data()->syscall_frame->prev_frame) return STATUS_NO_CALLBACK_ACTIVE;
    user_mode_callback_return( ret_ptr, ret_len, status, NtCurrentTeb() );
}


/***********************************************************************
 *           handle_syscall_fault
 *
 * Handle a page fault happening during a system call.
 */
static BOOL handle_syscall_fault( ucontext_t *context, EXCEPTION_RECORD *rec )
{
    struct syscall_frame *frame = arm_thread_data()->syscall_frame;
    UINT i;

    if (!is_inside_syscall( context ) && !ntdll_get_thread_data()->jmp_buf) return FALSE;

    TRACE( "code=%lx flags=%lx addr=%p pc=%08lx\n",
           rec->ExceptionCode, rec->ExceptionFlags, rec->ExceptionAddress, (DWORD)PC_sig(context) );
    for (i = 0; i < rec->NumberParameters; i++)
        TRACE( " info[%d]=%08lx\n", i, rec->ExceptionInformation[i] );

    TRACE( " r0=%08lx r1=%08lx r2=%08lx r3=%08lx r4=%08lx r5=%08lx\n",
           (DWORD)REGn_sig(0, context), (DWORD)REGn_sig(1, context), (DWORD)REGn_sig(2, context),
           (DWORD)REGn_sig(3, context), (DWORD)REGn_sig(4, context), (DWORD)REGn_sig(5, context) );
    TRACE( " r6=%08lx r7=%08lx r8=%08lx r9=%08lx r10=%08lx r11=%08lx\n",
           (DWORD)REGn_sig(6, context), (DWORD)REGn_sig(7, context), (DWORD)REGn_sig(8, context),
           (DWORD)REGn_sig(9, context), (DWORD)REGn_sig(10, context), (DWORD)FP_sig(context) );
    TRACE( " r12=%08lx sp=%08lx lr=%08lx pc=%08lx cpsr=%08lx\n",
           (DWORD)IP_sig(context), (DWORD)SP_sig(context), (DWORD)LR_sig(context),
           (DWORD)PC_sig(context), (DWORD)CPSR_sig(context) );

    if (rec->ExceptionCode == STATUS_ACCESS_VIOLATION
            && is_inside_syscall_stack_guard( (char *)rec->ExceptionInformation[1] ))
        ERR_(seh)( "Syscall stack overrun.\n ");

    if (ntdll_get_thread_data()->jmp_buf)
    {
        TRACE( "returning to handler\n" );
        REGn_sig(0, context) = (DWORD)ntdll_get_thread_data()->jmp_buf;
        REGn_sig(1, context) = 1;
        PC_sig(context)      = (DWORD)__wine_longjmp;
        ntdll_get_thread_data()->jmp_buf = NULL;
    }
    else
    {
        TRACE( "returning to user mode ip=%08x ret=%08lx\n", frame->pc, rec->ExceptionCode );
        REGn_sig(0, context) = (DWORD)frame;
        REGn_sig(1, context) = rec->ExceptionCode;
        PC_sig(context)      = (DWORD)__wine_syscall_dispatcher_return;
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
        switch (get_udf_immediate( context ))
        {
        case 0xfb:  /* __fastfail */
        {
            CONTEXT ctx;
            save_context( &ctx, sigcontext );
            rec.ExceptionCode = STATUS_STACK_BUFFER_OVERRUN;
            rec.ExceptionAddress = (void *)ctx.Pc;
            rec.ExceptionFlags = EH_NONCONTINUABLE;
            rec.NumberParameters = 1;
            rec.ExceptionInformation[0] = ctx.R0;
            NtRaiseException( &rec, &ctx, FALSE );
            return;
        }
        case 0xfe:  /* breakpoint */
            rec.ExceptionCode = EXCEPTION_BREAKPOINT;
            rec.NumberParameters = 1;
            break;
        default:
            rec.ExceptionCode = EXCEPTION_ILLEGAL_INSTRUCTION;
            break;
        }
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
    HANDLE handle;

    if (!p__wine_ctrl_routine) return;
    if (!NtCreateThreadEx( &handle, THREAD_ALL_ACCESS, NULL, NtCurrentProcess(),
                           p__wine_ctrl_routine, 0 /* CTRL_C_EVENT */, 0, 0, 0, 0, NULL ))
        NtClose( handle );
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

    if (is_inside_syscall( sigcontext ))
    {
        context.ContextFlags = CONTEXT_FULL;
        NtGetContextThread( GetCurrentThread(), &context );
        wait_suspend( &context );
        NtSetContextThread( GetCurrentThread(), &context );
    }
    else
    {
        save_context( &context, sigcontext );
        wait_suspend( &context );
        restore_context( &context, sigcontext );
    }
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
    teb->WOW32Reserved = __wine_syscall_dispatcher;
    return STATUS_SUCCESS;
}


/**********************************************************************
 *             signal_free_thread
 */
void signal_free_thread( TEB *teb )
{
}


/**********************************************************************
 *		signal_init_process
 */
void signal_init_process(void)
{
    struct sigaction sig_act;
    void *kernel_stack = (char *)ntdll_get_thread_data()->kernel_stack + kernel_stack_size;

    arm_thread_data()->syscall_frame = (struct syscall_frame *)kernel_stack - 1;

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
 *           call_init_thunk
 */
void DECLSPEC_HIDDEN call_init_thunk( LPTHREAD_START_ROUTINE entry, void *arg, BOOL suspend, TEB *teb )
{
    struct arm_thread_data *thread_data = (struct arm_thread_data *)&teb->GdiTebBatch;
    struct syscall_frame *frame = thread_data->syscall_frame;
    CONTEXT *ctx, context = { CONTEXT_ALL };

    __asm__ __volatile__( "mcr p15, 0, %0, c13, c0, 2" : : "r" (teb) );

    context.R0 = (DWORD)entry;
    context.R1 = (DWORD)arg;
    context.Sp = (DWORD)teb->Tib.StackBase;
    context.Pc = (DWORD)pRtlUserThreadStart;
    if (context.Pc & 1) context.Cpsr |= 0x20; /* thumb mode */
    if ((ctx = get_cpu_area( IMAGE_FILE_MACHINE_ARMNT ))) *ctx = context;

    if (suspend) wait_suspend( &context );

    ctx = (CONTEXT *)((ULONG_PTR)context.Sp & ~15) - 1;
    *ctx = context;
    ctx->ContextFlags = CONTEXT_FULL;
    memset( frame, 0, sizeof(*frame) );
    NtSetContextThread( GetCurrentThread(), ctx );

    frame->sp = (DWORD)ctx;
    frame->pc = (DWORD)pLdrInitializeThunk;
    frame->r0 = (DWORD)ctx;
    frame->prev_frame = NULL;
    frame->restore_flags |= CONTEXT_INTEGER;
    frame->syscall_table = KeServiceDescriptorTable;

    pthread_sigmask( SIG_UNBLOCK, &server_block_set, NULL );
    __wine_syscall_dispatcher_return( frame, 0 );
}


/***********************************************************************
 *           signal_start_thread
 */
__ASM_GLOBAL_FUNC( signal_start_thread,
                   __ASM_EHABI(".cantunwind\n\t")
                   "push {r4-r12,lr}\n\t"
                   /* store exit frame */
                   "str sp, [r3, #0x1d4]\n\t" /* arm_thread_data()->exit_frame */
                   /* set syscall frame */
                   "ldr r6, [r3, #0x1d8]\n\t" /* arm_thread_data()->syscall_frame */
                   "cbnz r6, 1f\n\t"
                   "sub r6, sp, #0x160\n\t"   /* sizeof(struct syscall_frame) */
                   "str r6, [r3, #0x1d8]\n\t" /* arm_thread_data()->syscall_frame */
                   "1:\tmov sp, r6\n\t"
                   "bl " __ASM_NAME("call_init_thunk") )


/***********************************************************************
 *           signal_exit_thread
 */
__ASM_GLOBAL_FUNC( signal_exit_thread,
                   __ASM_EHABI(".cantunwind\n\t")
                   "ldr r3, [r2, #0x1d4]\n\t"  /* arm_thread_data()->exit_frame */
                   "mov ip, #0\n\t"
                   "str ip, [r2, #0x1d4]\n\t"
                   "cmp r3, ip\n\t"
                   "it ne\n\t"
                   "movne sp, r3\n\t"
                   "blx r1" )


/***********************************************************************
 *           __wine_syscall_dispatcher
 */
__ASM_GLOBAL_FUNC( __wine_syscall_dispatcher,
                   __ASM_EHABI(".cantunwind\n\t")
                   "mrc p15, 0, r1, c13, c0, 2\n\t" /* NtCurrentTeb() */
                   "ldr r1, [r1, #0x1d8]\n\t"       /* arm_thread_data()->syscall_frame */
                   "add r0, r1, #0x10\n\t"
                   "stm r0, {r4-r12,lr}\n\t"
                   "add r2, sp, #0x10\n\t"
                   "str r2, [r1, #0x38]\n\t"
                   "str r3, [r1, #0x3c]\n\t"
                   "mrs r0, CPSR\n\t"
                   "bfi r0, lr, #5, #1\n\t"         /* set thumb bit */
                   "str r0, [r1, #0x40]\n\t"
                   "mov r0, #0\n\t"
                   "str r0, [r1, #0x44]\n\t"        /* frame->restore_flags */
#ifndef __SOFTFP__
                   "vmrs r0, fpscr\n\t"
                   "str r0, [r1, #0x48]\n\t"
                   "add r0, r1, #0x60\n\t"
                   "vstm r0, {d0-d15}\n\t"
#endif
                   "mov r6, sp\n\t"
                   "mov sp, r1\n\t"
                   "mov r8, r1\n\t"
                   "ldr r5, [r1, #0x50]\n\t"        /* frame->syscall_table */
                   "ubfx r4, ip, #12, #2\n\t"       /* syscall table number */
                   "bfc ip, #12, #20\n\t"           /* syscall number */
                   "add r4, r5, r4, lsl #4\n\t"
                   "ldr r5, [r4, #8]\n\t"           /* table->ServiceLimit */
                   "cmp ip, r5\n\t"
                   "bcs 5f\n\t"
                   "ldr r5, [r4, #12]\n\t"          /* table->ArgumentTable */
                   "ldrb r5, [r5, ip]\n\t"
                   "cmp r5, #16\n\t"
                   "it le\n\t"
                   "movle r5, #16\n\t"
                   "sub r0, sp, r5\n\t"
                   "and r0, #~7\n\t"
                   "mov sp, r0\n"
                   "2:\tsubs r5, r5, #4\n\t"
                   "ldr r0, [r6, r5]\n\t"
                   "str r0, [sp, r5]\n\t"
                   "bgt 2b\n\t"
                   "pop {r0-r3}\n\t"                /* first 4 args are in registers */
                   "ldr r5, [r4]\n\t"               /* table->ServiceTable */
                   "ldr ip, [r5, ip, lsl #2]\n\t"
                   "blx ip\n"
                   "4:\tldr ip, [r8, #0x44]\n\t"    /* frame->restore_flags */
#ifndef __SOFTFP__
                   "tst ip, #4\n\t"                 /* CONTEXT_FLOATING_POINT */
                   "beq 3f\n\t"
                   "ldr r4, [r8, #0x48]\n\t"
                   "vmsr fpscr, r4\n\t"
                   "add r4, r8, #0x60\n\t"
                   "vldm r4, {d0-d15}\n"
                   "3:\n\t"
#endif
                   "tst ip, #2\n\t"                 /* CONTEXT_INTEGER */
                   "it ne\n\t"
                   "ldmne r8, {r0-r3}\n\t"
                   "ldr lr, [r8, #0x3c]\n\t"
                   "ldr sp, [r8, #0x38]\n\t"
                   "add r8, r8, #0x10\n\t"
                   "ldm r8, {r4-r12,pc}\n"
                   "5:\tmovw r0, #0x000d\n\t" /* STATUS_INVALID_PARAMETER */
                   "movt r0, #0xc000\n\t"
                   "add sp, sp, #0x10\n\t"
                   "b 4b\n\t"
                   ".globl " __ASM_NAME("__wine_syscall_dispatcher_return") "\n"
                   __ASM_NAME("__wine_syscall_dispatcher_return") ":\n\t"
                   "mov r8, r0\n\t"
                   "mov r0, r1\n\t"
                   "b 4b" )


/***********************************************************************
 *           __wine_setjmpex
 */
__ASM_GLOBAL_FUNC( __wine_setjmpex,
                   __ASM_EHABI(".cantunwind\n\t")
                   "stm r0, {r1,r4-r11}\n"         /* jmp_buf->Frame,R4..R11 */
                   "str sp, [r0, #0x24]\n\t"       /* jmp_buf->Sp */
                   "str lr, [r0, #0x28]\n\t"       /* jmp_buf->Pc */
#ifndef __SOFTFP__
                   "vmrs r2, fpscr\n\t"
                   "str r2, [r0, #0x2c]\n\t"       /* jmp_buf->Fpscr */
                   "add r0, r0, #0x30\n\t"
                   "vstm r0, {d8-d15}\n\t"         /* jmp_buf->D[0..7] */
#endif
                   "mov r0, #0\n\t"
                   "bx lr" )


/***********************************************************************
 *           __wine_longjmp
 */
__ASM_GLOBAL_FUNC( __wine_longjmp,
                   __ASM_EHABI(".cantunwind\n\t")
                   "ldm r0, {r3-r11}\n\t"          /* jmp_buf->Frame,R4..R11 */
                   "ldr sp, [r0, #0x24]\n\t"       /* jmp_buf->Sp */
                   "ldr r2, [r0, #0x28]\n\t"       /* jmp_buf->Pc */
#ifndef __SOFTFP__
                   "ldr r3, [r0, #0x2c]\n\t"       /* jmp_buf->Fpscr */
                   "vmsr fpscr, r3\n\t"
                   "add r0, r0, #0x30\n\t"
                   "vldm r0, {d8-d15}\n\t"         /* jmp_buf->D[0..7] */
#endif
                   "mov r0, r1\n\t"                /* retval */
                   "bx r2" )

#endif  /* __arm__ */
