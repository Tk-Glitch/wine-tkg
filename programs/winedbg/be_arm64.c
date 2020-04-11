/*
 * Debugger ARM64 specific functions
 *
 * Copyright 2010-2013 André Hentschel
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

#include "debugger.h"

#if defined(__aarch64__) && !defined(__AARCH64EB__)

static BOOL be_arm64_get_addr(HANDLE hThread, const dbg_ctx_t *ctx,
                              enum be_cpu_addr bca, ADDRESS64* addr)
{
    switch (bca)
    {
    case be_cpu_addr_pc:
        return be_cpu_build_addr(hThread, ctx, addr, 0, ctx->ctx.Pc);
    case be_cpu_addr_stack:
        return be_cpu_build_addr(hThread, ctx, addr, 0, ctx->ctx.Sp);
    case be_cpu_addr_frame:
        return be_cpu_build_addr(hThread, ctx, addr, 0, ctx->ctx.u.s.Fp);
        break;
    }
    return FALSE;
}

static BOOL be_arm64_get_register_info(int regno, enum be_cpu_addr* kind)
{
    switch (regno)
    {
    case CV_ARM64_PC:      *kind = be_cpu_addr_pc;    return TRUE;
    case CV_ARM64_SP:      *kind = be_cpu_addr_stack; return TRUE;
    case CV_ARM64_FP:      *kind = be_cpu_addr_frame; return TRUE;
    }
    return FALSE;
}

static void be_arm64_single_step(dbg_ctx_t *ctx, BOOL enable)
{
    dbg_printf("be_arm64_single_step: not done\n");
}

static void be_arm64_print_context(HANDLE hThread, const dbg_ctx_t *ctx, int all_regs)
{
    static const char condflags[] = "NZCV";
    int i;
    char        buf[8];

    switch (ctx->ctx.Cpsr & 0x0f)
    {
    case 0:  strcpy(buf, "EL0t"); break;
    case 4:  strcpy(buf, "EL1t"); break;
    case 5:  strcpy(buf, "EL1t"); break;
    case 8:  strcpy(buf, "EL2t"); break;
    case 9:  strcpy(buf, "EL2t"); break;
    case 12: strcpy(buf, "EL3t"); break;
    case 13: strcpy(buf, "EL3t"); break;
    default: strcpy(buf, "UNKNWN"); break;
    }

    dbg_printf("Register dump:\n");
    dbg_printf("%s %s Mode\n", (ctx->ctx.Cpsr & 0x10) ? "ARM" : "ARM64", buf);

    strcpy(buf, condflags);
    for (i = 0; buf[i]; i++)
        if (!((ctx->ctx.Cpsr >> 26) & (1 << (sizeof(condflags) - i))))
            buf[i] = '-';

    dbg_printf(" Pc:%016lx Sp:%016lx Lr:%016lx Cpsr:%08x(%s)\n",
               ctx->ctx.Pc, ctx->ctx.Sp, ctx->ctx.u.s.Lr, ctx->ctx.Cpsr, buf);
    dbg_printf(" x0: %016lx x1: %016lx x2: %016lx x3: %016lx x4: %016lx\n",
               ctx->ctx.u.s.X0, ctx->ctx.u.s.X1, ctx->ctx.u.s.X2, ctx->ctx.u.s.X3, ctx->ctx.u.s.X4);
    dbg_printf(" x5: %016lx x6: %016lx x7: %016lx x8: %016lx x9: %016lx\n",
               ctx->ctx.u.s.X5, ctx->ctx.u.s.X6, ctx->ctx.u.s.X7, ctx->ctx.u.s.X8, ctx->ctx.u.s.X9);
    dbg_printf(" x10:%016lx x11:%016lx x12:%016lx x13:%016lx x14:%016lx\n",
               ctx->ctx.u.s.X10, ctx->ctx.u.s.X11, ctx->ctx.u.s.X12, ctx->ctx.u.s.X13, ctx->ctx.u.s.X14);
    dbg_printf(" x15:%016lx ip0:%016lx ip1:%016lx x18:%016lx x19:%016lx\n",
               ctx->ctx.u.s.X15, ctx->ctx.u.s.X16, ctx->ctx.u.s.X17, ctx->ctx.u.s.X18, ctx->ctx.u.s.X19);
    dbg_printf(" x20:%016lx x21:%016lx x22:%016lx x23:%016lx x24:%016lx\n",
               ctx->ctx.u.s.X20, ctx->ctx.u.s.X21, ctx->ctx.u.s.X22, ctx->ctx.u.s.X23, ctx->ctx.u.s.X24);
    dbg_printf(" x25:%016lx x26:%016lx x27:%016lx x28:%016lx Fp:%016lx\n",
               ctx->ctx.u.s.X25, ctx->ctx.u.s.X26, ctx->ctx.u.s.X27, ctx->ctx.u.s.X28, ctx->ctx.u.s.Fp);

    if (all_regs) dbg_printf( "Floating point ARM64 dump not implemented\n" );
}

static void be_arm64_print_segment_info(HANDLE hThread, const dbg_ctx_t *ctx)
{
}

static struct dbg_internal_var be_arm64_ctx[] =
{
    {CV_ARM64_PSTATE,     "cpsr",   (DWORD_PTR*)FIELD_OFFSET(CONTEXT, Cpsr),    dbg_itype_unsigned_int},
    {CV_ARM64_X0 +  0,    "x0",     (DWORD_PTR*)FIELD_OFFSET(CONTEXT, u.s.X0),  dbg_itype_unsigned_long_int},
    {CV_ARM64_X0 +  1,    "x1",     (DWORD_PTR*)FIELD_OFFSET(CONTEXT, u.s.X1),  dbg_itype_unsigned_long_int},
    {CV_ARM64_X0 +  2,    "x2",     (DWORD_PTR*)FIELD_OFFSET(CONTEXT, u.s.X2),  dbg_itype_unsigned_long_int},
    {CV_ARM64_X0 +  3,    "x3",     (DWORD_PTR*)FIELD_OFFSET(CONTEXT, u.s.X3),  dbg_itype_unsigned_long_int},
    {CV_ARM64_X0 +  4,    "x4",     (DWORD_PTR*)FIELD_OFFSET(CONTEXT, u.s.X4),  dbg_itype_unsigned_long_int},
    {CV_ARM64_X0 +  5,    "x5",     (DWORD_PTR*)FIELD_OFFSET(CONTEXT, u.s.X5),  dbg_itype_unsigned_long_int},
    {CV_ARM64_X0 +  6,    "x6",     (DWORD_PTR*)FIELD_OFFSET(CONTEXT, u.s.X6),  dbg_itype_unsigned_long_int},
    {CV_ARM64_X0 +  7,    "x7",     (DWORD_PTR*)FIELD_OFFSET(CONTEXT, u.s.X7),  dbg_itype_unsigned_long_int},
    {CV_ARM64_X0 +  8,    "x8",     (DWORD_PTR*)FIELD_OFFSET(CONTEXT, u.s.X8),  dbg_itype_unsigned_long_int},
    {CV_ARM64_X0 +  9,    "x9",     (DWORD_PTR*)FIELD_OFFSET(CONTEXT, u.s.X9),  dbg_itype_unsigned_long_int},
    {CV_ARM64_X0 +  10,   "x10",    (DWORD_PTR*)FIELD_OFFSET(CONTEXT, u.s.X10), dbg_itype_unsigned_long_int},
    {CV_ARM64_X0 +  11,   "x11",    (DWORD_PTR*)FIELD_OFFSET(CONTEXT, u.s.X11), dbg_itype_unsigned_long_int},
    {CV_ARM64_X0 +  12,   "x12",    (DWORD_PTR*)FIELD_OFFSET(CONTEXT, u.s.X12), dbg_itype_unsigned_long_int},
    {CV_ARM64_X0 +  13,   "x13",    (DWORD_PTR*)FIELD_OFFSET(CONTEXT, u.s.X13), dbg_itype_unsigned_long_int},
    {CV_ARM64_X0 +  14,   "x14",    (DWORD_PTR*)FIELD_OFFSET(CONTEXT, u.s.X14), dbg_itype_unsigned_long_int},
    {CV_ARM64_X0 +  15,   "x15",    (DWORD_PTR*)FIELD_OFFSET(CONTEXT, u.s.X15), dbg_itype_unsigned_long_int},
    {CV_ARM64_X0 +  16,   "x16",    (DWORD_PTR*)FIELD_OFFSET(CONTEXT, u.s.X16), dbg_itype_unsigned_long_int},
    {CV_ARM64_X0 +  17,   "x17",    (DWORD_PTR*)FIELD_OFFSET(CONTEXT, u.s.X17), dbg_itype_unsigned_long_int},
    {CV_ARM64_X0 +  18,   "x18",    (DWORD_PTR*)FIELD_OFFSET(CONTEXT, u.s.X18), dbg_itype_unsigned_long_int},
    {CV_ARM64_X0 +  19,   "x19",    (DWORD_PTR*)FIELD_OFFSET(CONTEXT, u.s.X19), dbg_itype_unsigned_long_int},
    {CV_ARM64_X0 +  20,   "x20",    (DWORD_PTR*)FIELD_OFFSET(CONTEXT, u.s.X20), dbg_itype_unsigned_long_int},
    {CV_ARM64_X0 +  21,   "x21",    (DWORD_PTR*)FIELD_OFFSET(CONTEXT, u.s.X21), dbg_itype_unsigned_long_int},
    {CV_ARM64_X0 +  22,   "x22",    (DWORD_PTR*)FIELD_OFFSET(CONTEXT, u.s.X22), dbg_itype_unsigned_long_int},
    {CV_ARM64_X0 +  23,   "x23",    (DWORD_PTR*)FIELD_OFFSET(CONTEXT, u.s.X23), dbg_itype_unsigned_long_int},
    {CV_ARM64_X0 +  24,   "x24",    (DWORD_PTR*)FIELD_OFFSET(CONTEXT, u.s.X24), dbg_itype_unsigned_long_int},
    {CV_ARM64_X0 +  25,   "x25",    (DWORD_PTR*)FIELD_OFFSET(CONTEXT, u.s.X25), dbg_itype_unsigned_long_int},
    {CV_ARM64_X0 +  26,   "x26",    (DWORD_PTR*)FIELD_OFFSET(CONTEXT, u.s.X26), dbg_itype_unsigned_long_int},
    {CV_ARM64_X0 +  27,   "x27",    (DWORD_PTR*)FIELD_OFFSET(CONTEXT, u.s.X27), dbg_itype_unsigned_long_int},
    {CV_ARM64_X0 +  28,   "x28",    (DWORD_PTR*)FIELD_OFFSET(CONTEXT, u.s.X28), dbg_itype_unsigned_long_int},
    {CV_ARM64_FP,         "fp",     (DWORD_PTR*)FIELD_OFFSET(CONTEXT, u.s.Fp),  dbg_itype_unsigned_long_int},
    {CV_ARM64_LR,         "lr",     (DWORD_PTR*)FIELD_OFFSET(CONTEXT, u.s.Lr),  dbg_itype_unsigned_long_int},
    {CV_ARM64_SP,         "sp",     (DWORD_PTR*)FIELD_OFFSET(CONTEXT, Sp),      dbg_itype_unsigned_long_int},
    {CV_ARM64_PC,         "pc",     (DWORD_PTR*)FIELD_OFFSET(CONTEXT, Pc),      dbg_itype_unsigned_long_int},
    {0,                   NULL,     0,                                          dbg_itype_none}
};

static BOOL be_arm64_is_step_over_insn(const void* insn)
{
    dbg_printf("be_arm64_is_step_over_insn: not done\n");
    return FALSE;
}

static BOOL be_arm64_is_function_return(const void* insn)
{
    dbg_printf("be_arm64_is_function_return: not done\n");
    return FALSE;
}

static BOOL be_arm64_is_break_insn(const void* insn)
{
    dbg_printf("be_arm64_is_break_insn: not done\n");
    return FALSE;
}

static BOOL be_arm64_is_func_call(const void* insn, ADDRESS64* callee)
{
    return FALSE;
}

static BOOL be_arm64_is_jump(const void* insn, ADDRESS64* jumpee)
{
    return FALSE;
}

static BOOL be_arm64_insert_Xpoint(HANDLE hProcess, const struct be_process_io* pio,
                                   dbg_ctx_t *ctx, enum be_xpoint_type type,
                                   void* addr, unsigned long* val, unsigned size)
{
    SIZE_T              sz;

    switch (type)
    {
    case be_xpoint_break:
        if (!size) return FALSE;
        if (!pio->read(hProcess, addr, val, 4, &sz) || sz != 4) return FALSE;
    default:
        dbg_printf("Unknown/unsupported bp type %c\n", type);
        return FALSE;
    }
    return TRUE;
}

static BOOL be_arm64_remove_Xpoint(HANDLE hProcess, const struct be_process_io* pio,
                                   dbg_ctx_t *ctx, enum be_xpoint_type type,
                                   void* addr, unsigned long val, unsigned size)
{
    SIZE_T              sz;

    switch (type)
    {
    case be_xpoint_break:
        if (!size) return FALSE;
        if (!pio->write(hProcess, addr, &val, 4, &sz) || sz == 4) return FALSE;
        break;
    default:
        dbg_printf("Unknown/unsupported bp type %c\n", type);
        return FALSE;
    }
    return TRUE;
}

static BOOL be_arm64_is_watchpoint_set(const dbg_ctx_t *ctx, unsigned idx)
{
    dbg_printf("be_arm64_is_watchpoint_set: not done\n");
    return FALSE;
}

static void be_arm64_clear_watchpoint(dbg_ctx_t *ctx, unsigned idx)
{
    dbg_printf("be_arm64_clear_watchpoint: not done\n");
}

static int be_arm64_adjust_pc_for_break(dbg_ctx_t *ctx, BOOL way)
{
    if (way)
    {
        ctx->ctx.Pc -= 4;
        return -4;
    }
    ctx->ctx.Pc += 4;
    return 4;
}

static BOOL be_arm64_fetch_integer(const struct dbg_lvalue* lvalue, unsigned size,
                                   BOOL is_signed, LONGLONG* ret)
{
    if (size != 1 && size != 2 && size != 4 && size != 8) return FALSE;

    memset(ret, 0, sizeof(*ret)); /* clear unread bytes */
    /* FIXME: this assumes that debuggee and debugger use the same
     * integral representation
     */
    if (!memory_read_value(lvalue, size, ret)) return FALSE;

    /* propagate sign information */
    if (is_signed && size < 8 && (*ret >> (size * 8 - 1)) != 0)
    {
        ULONGLONG neg = -1;
        *ret |= neg << (size * 8);
    }
    return TRUE;
}

static BOOL be_arm64_fetch_float(const struct dbg_lvalue* lvalue, unsigned size,
                                 long double* ret)
{
    char tmp[sizeof(long double)];

    /* FIXME: this assumes that debuggee and debugger use the same
     * representation for reals
     */
    if (!memory_read_value(lvalue, size, tmp)) return FALSE;

    if (size == sizeof(float)) *ret = *(float*)tmp;
    else if (size == sizeof(double)) *ret = *(double*)tmp;
    else if (size == sizeof(long double)) *ret = *(long double*)tmp;
    else return FALSE;

    return TRUE;
}

static BOOL be_arm64_store_integer(const struct dbg_lvalue* lvalue, unsigned size,
                                   BOOL is_signed, LONGLONG val)
{
    /* this is simple if we're on a little endian CPU */
    return memory_write_value(lvalue, size, &val);
}

void be_arm64_disasm_one_insn(ADDRESS64 *addr, int display)
{
    dbg_printf("be_arm64_disasm_one_insn: not done\n");
}

static BOOL be_arm64_get_context(HANDLE thread, dbg_ctx_t *ctx)
{
    ctx->ctx.ContextFlags = CONTEXT_ALL;
    return GetThreadContext(thread, &ctx->ctx);
}

static BOOL be_arm64_set_context(HANDLE thread, const dbg_ctx_t *ctx)
{
    return SetThreadContext(thread, &ctx->ctx);
}

#define REG(f,n,t,r)  {f, n, t, FIELD_OFFSET(CONTEXT, r), sizeof(((CONTEXT*)NULL)->r)}

static struct gdb_register be_arm64_gdb_register_map[] = {
    REG("core", "x0",   NULL,         u.s.X0),
    REG(NULL,   "x1",   NULL,         u.s.X1),
    REG(NULL,   "x2",   NULL,         u.s.X2),
    REG(NULL,   "x3",   NULL,         u.s.X3),
    REG(NULL,   "x4",   NULL,         u.s.X4),
    REG(NULL,   "x5",   NULL,         u.s.X5),
    REG(NULL,   "x6",   NULL,         u.s.X6),
    REG(NULL,   "x7",   NULL,         u.s.X7),
    REG(NULL,   "x8",   NULL,         u.s.X8),
    REG(NULL,   "x9",   NULL,         u.s.X9),
    REG(NULL,   "x10",  NULL,         u.s.X10),
    REG(NULL,   "x11",  NULL,         u.s.X11),
    REG(NULL,   "x12",  NULL,         u.s.X12),
    REG(NULL,   "x13",  NULL,         u.s.X13),
    REG(NULL,   "x14",  NULL,         u.s.X14),
    REG(NULL,   "x15",  NULL,         u.s.X15),
    REG(NULL,   "x16",  NULL,         u.s.X16),
    REG(NULL,   "x17",  NULL,         u.s.X17),
    REG(NULL,   "x18",  NULL,         u.s.X18),
    REG(NULL,   "x19",  NULL,         u.s.X19),
    REG(NULL,   "x20",  NULL,         u.s.X20),
    REG(NULL,   "x21",  NULL,         u.s.X21),
    REG(NULL,   "x22",  NULL,         u.s.X22),
    REG(NULL,   "x23",  NULL,         u.s.X23),
    REG(NULL,   "x24",  NULL,         u.s.X24),
    REG(NULL,   "x25",  NULL,         u.s.X25),
    REG(NULL,   "x26",  NULL,         u.s.X26),
    REG(NULL,   "x27",  NULL,         u.s.X27),
    REG(NULL,   "x28",  NULL,         u.s.X28),
    REG(NULL,   "x29",  NULL,         u.s.Fp),
    REG(NULL,   "x30",  NULL,         u.s.Lr),
    REG(NULL,   "sp",   "data_ptr",   Sp),
    REG(NULL,   "pc",   "code_ptr",   Pc),
    REG(NULL,   "cpsr", "cpsr_flags", Cpsr),
};

struct backend_cpu be_arm64 =
{
    IMAGE_FILE_MACHINE_ARM64,
    8,
    be_cpu_linearize,
    be_cpu_build_addr,
    be_arm64_get_addr,
    be_arm64_get_register_info,
    be_arm64_single_step,
    be_arm64_print_context,
    be_arm64_print_segment_info,
    be_arm64_ctx,
    be_arm64_is_step_over_insn,
    be_arm64_is_function_return,
    be_arm64_is_break_insn,
    be_arm64_is_func_call,
    be_arm64_is_jump,
    be_arm64_disasm_one_insn,
    be_arm64_insert_Xpoint,
    be_arm64_remove_Xpoint,
    be_arm64_is_watchpoint_set,
    be_arm64_clear_watchpoint,
    be_arm64_adjust_pc_for_break,
    be_arm64_fetch_integer,
    be_arm64_fetch_float,
    be_arm64_store_integer,
    be_arm64_get_context,
    be_arm64_set_context,
    be_arm64_gdb_register_map,
    ARRAY_SIZE(be_arm64_gdb_register_map),
};
#endif
