/*
 * Generate hash tables for Wine debugger symbols
 *
 * Copyright (C) 1993, Eric Youngdale.
 *               2004-2005, Eric Pouech.
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "debugger.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(winedbg);

static BOOL symbol_get_debug_start(const struct dbg_type* func, ULONG64* start)
{
    DWORD                       count, tag;
    char                        buffer[sizeof(TI_FINDCHILDREN_PARAMS) + 256 * sizeof(DWORD)];
    TI_FINDCHILDREN_PARAMS*     fcp = (TI_FINDCHILDREN_PARAMS*)buffer;
    int                         i;
    struct dbg_type             child;

    if (!func->id) return FALSE; /* native dbghelp not always fills the info field */

    if (!types_get_info(func, TI_GET_CHILDRENCOUNT, &count)) return FALSE;
    fcp->Start = 0;
    while (count)
    {
        fcp->Count = min(count, 256);
        if (types_get_info(func, TI_FINDCHILDREN, fcp))
        {
            for (i = 0; i < min(fcp->Count, count); i++)
            {
                child.module = func->module;
                child.id = fcp->ChildId[i];
                types_get_info(&child, TI_GET_SYMTAG, &tag);
                if (tag != SymTagFuncDebugStart) continue;
                return types_get_info(&child, TI_GET_ADDRESS, start);
            }
            count -= min(count, 256);
            fcp->Start += 256;
            fcp->Start += 256;
        }
    }
    return FALSE;
}

static BOOL fill_sym_lvalue(const SYMBOL_INFO* sym, ULONG_PTR base,
                            struct dbg_lvalue* lvalue, char* buffer, size_t sz)
{
    if (buffer) buffer[0] = '\0';
    if (sym->Flags & SYMFLAG_REGISTER)
    {
        DWORD_PTR* pval;

        if (!memory_get_register(sym->Register, &pval, buffer, sz))
            return FALSE;
        lvalue->cookie = DLV_HOST;
        lvalue->addr.Offset = (DWORD_PTR)pval;
    }
    else if (sym->Flags & SYMFLAG_REGREL)
    {
        DWORD_PTR* pval;
        size_t  l;

        *buffer++ = '['; sz--;
        if (!memory_get_register(sym->Register, &pval, buffer, sz))
            return FALSE;
        l = strlen(buffer);
        sz -= l;
        buffer += l;
        lvalue->cookie = DLV_TARGET;
        lvalue->addr.Offset = (ULONG64)*pval + sym->Address;
        if ((LONG_PTR)sym->Address >= 0)
            snprintf(buffer, sz, "+%ld]", (ULONG_PTR)sym->Address);
        else
            snprintf(buffer, sz, "-%ld]", -(LONG_PTR)sym->Address);
    }
    else if (sym->Flags & SYMFLAG_VALUEPRESENT)
    {
        struct dbg_type type;
        VARIANT         v;

        type.module = sym->ModBase;
        type.id = sym->Index;

        if (!types_get_info(&type, TI_GET_VALUE, &v))
        {
            if (buffer) snprintf(buffer, sz, "Couldn't get full value information for %s", sym->Name);
            return FALSE;
        }
        else if (v.n1.n2.vt & VT_BYREF)
        {
            /* FIXME: this won't work for pointers or arrays, as we don't always
             * know, if the value to be dereferenced lies in debuggee or
             * debugger address space.
             */
            if (sym->Tag == SymTagPointerType || sym->Tag == SymTagArrayType)
            {
                if (buffer) snprintf(buffer, sz, "Couldn't dereference pointer for const value for %s", sym->Name);
                return FALSE;
            }
            /* this is likely Wine's dbghelp which passes const values by reference
             * (object is managed by dbghelp, hence in debugger address space)
             */
            lvalue->cookie = DLV_HOST;
            lvalue->addr.Offset = (DWORD_PTR)sym->Value;
        }
        else
        {
            DWORD* pdw = (DWORD*)lexeme_alloc_size(sizeof(*pdw));
            lvalue->cookie = DLV_HOST;
            lvalue->addr.Offset = (DWORD_PTR)pdw;
            *pdw = sym->Value;
        }
    }
    else if (sym->Flags & SYMFLAG_LOCAL)
    {
        lvalue->cookie = DLV_TARGET;
        lvalue->addr.Offset = base + sym->Address;
    }
    else if (sym->Flags & SYMFLAG_TLSREL)
    {
        PROCESS_BASIC_INFORMATION pbi;
        THREAD_BASIC_INFORMATION  tbi;
        DWORD_PTR                 addr;
        PEB                       peb;
        PEB_LDR_DATA              ldr_data;
        PLIST_ENTRY               head, current;
        LDR_DATA_TABLE_ENTRY      ldr_module;
        unsigned                  tlsindex = -1;

        if (NtQueryInformationProcess(dbg_curr_process->handle, ProcessBasicInformation,
                                      &pbi, sizeof(pbi), NULL) ||
            NtQueryInformationThread(dbg_curr_thread->handle, ThreadBasicInformation,
                                     &tbi, sizeof(tbi), NULL))
        {
        tls_error:
            if (buffer) snprintf(buffer, sz, "Cannot read TLS address\n");
            return FALSE;
        }
        addr = (DWORD_PTR)&(((TEB*)tbi.TebBaseAddress)->ThreadLocalStoragePointer);
        if (!dbg_read_memory((void*)addr, &addr, sizeof(addr)) ||
            !dbg_read_memory(pbi.PebBaseAddress, &peb, sizeof(peb)) ||
            !dbg_read_memory(peb.LdrData, &ldr_data, sizeof(ldr_data)))
            goto tls_error;
        current = ldr_data.InLoadOrderModuleList.Flink;
        head = &((PEB_LDR_DATA*)peb.LdrData)->InLoadOrderModuleList;
        do
        {
            if (!dbg_read_memory(CONTAINING_RECORD(current, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks),
                                 &ldr_module, sizeof(ldr_module))) goto tls_error;
            if ((DWORD_PTR)ldr_module.DllBase == sym->ModBase)
            {
                tlsindex = ldr_module.TlsIndex;
                break;
            }
            current = ldr_module.InLoadOrderLinks.Flink;
        } while (current != head);

        addr += tlsindex * sizeof(DWORD_PTR);
        if (!dbg_read_memory((void*)addr, &addr, sizeof(addr))) goto tls_error;
        lvalue->cookie = DLV_TARGET;
        lvalue->addr.Offset = addr + sym->Address;
    }
    else
    {
        lvalue->cookie = DLV_TARGET;
        lvalue->addr.Offset = sym->Address;
    }
    lvalue->addr.Mode = AddrModeFlat;
    lvalue->type.module = sym->ModBase;
    lvalue->type.id = sym->TypeIndex;

    return TRUE;
}

struct sgv_data
{
#define NUMDBGV                 100
    struct
    {
        /* FIXME: NUMDBGV should be made variable */
        struct dbg_lvalue               lvalue;
        DWORD                           flags;
        DWORD                           sym_info;
    }                           syms[NUMDBGV];  /* out     : will be filled in with various found symbols */
    int		                num;            /* out     : number of found symbols */
    int                         num_thunks;     /* out     : number of thunks found */
    const char*                 name;           /* in      : name of symbol to look up */
    unsigned                    do_thunks : 1;  /* in      : whether we return thunks tags */
    ULONG64                     frame_offset;   /* in      : frame for local & parameter variables look up */
};

static BOOL CALLBACK sgv_cb(PSYMBOL_INFO sym, ULONG size, PVOID ctx)
{
    struct sgv_data*    sgv = ctx;
    unsigned            insp;
    char                tmp[64];

    if (sym->Flags & SYMFLAG_THUNK)
    {
        if (!sgv->do_thunks) return TRUE;
        sgv->num_thunks++;
    }

    if (sgv->num >= NUMDBGV)
    {
        dbg_printf("Too many addresses for symbol '%s', limiting the first %d\n",
                   sgv->name, NUMDBGV);
        return FALSE;
    }
    WINE_TRACE("==> %s %s%s%s%s%s%s%s%s\n",
               sym->Name,
               (sym->Flags & SYMFLAG_FUNCTION) ? "func " : "",
               (sym->Flags & SYMFLAG_FRAMEREL) ? "framerel " : "",
               (sym->Flags & SYMFLAG_TLSREL) ? "tlsrel " : "",
               (sym->Flags & SYMFLAG_REGISTER) ? "register " : "",
               (sym->Flags & SYMFLAG_REGREL) ? "regrel " : "",
               (sym->Flags & SYMFLAG_PARAMETER) ? "param " : "",
               (sym->Flags & SYMFLAG_LOCAL) ? "local " : "",
               (sym->Flags & SYMFLAG_THUNK) ? "thunk " : "");

    /* always keep the thunks at end of the array */
    insp = sgv->num;
    if (sgv->num_thunks && !(sym->Flags & SYMFLAG_THUNK))
    {
        insp -= sgv->num_thunks;
        memmove(&sgv->syms[insp + 1], &sgv->syms[insp],
                sizeof(sgv->syms[0]) * sgv->num_thunks);
    }
    if (!fill_sym_lvalue(sym, sgv->frame_offset, &sgv->syms[insp].lvalue, tmp, sizeof(tmp)))
    {
        dbg_printf("%s: %s\n", sym->Name, tmp);
        return TRUE;
    }
    sgv->syms[insp].flags              = sym->Flags;
    sgv->syms[insp].sym_info           = sym->Index;
    sgv->num++;

    return TRUE;
}

enum sym_get_lval symbol_picker_interactive(const char* name, const struct sgv_data* sgv,
                                            struct dbg_lvalue* rtn)
{
    char        buffer[512];
    unsigned    i;

    if (!dbg_interactiveP)
    {
        dbg_printf("More than one symbol named %s, picking the first one\n", name);
        *rtn = sgv->syms[0].lvalue;
        return sglv_found;
    }

    dbg_printf("Many symbols with name '%s', "
               "choose the one you want (<cr> to abort):\n", name);
    for (i = 0; i < sgv->num; i++)
    {
        if (sgv->num - sgv->num_thunks > 1 && (sgv->syms[i].flags & SYMFLAG_THUNK) && !DBG_IVAR(AlwaysShowThunks))
            continue;
        dbg_printf("[%d]: ", i + 1);
        if (sgv->syms[i].flags & (SYMFLAG_LOCAL | SYMFLAG_PARAMETER))
        {
            dbg_printf("%s %sof %s\n",
                       sgv->syms[i].flags & SYMFLAG_PARAMETER ? "Parameter" : "Local variable",
                       sgv->syms[i].flags & (SYMFLAG_REGISTER|SYMFLAG_REGREL) ? "(in a register) " : "",
                       name);
        }
        else if (sgv->syms[i].flags & SYMFLAG_THUNK)
        {
            print_address(&sgv->syms[i].lvalue.addr, TRUE);
            /* FIXME: should display where the thunks points to */
            dbg_printf(" thunk %s\n", name);
        }
        else
        {
            print_address(&sgv->syms[i].lvalue.addr, TRUE);
            dbg_printf("\n");
        }
    }
    do
    {
        if (input_read_line("=> ", buffer, sizeof(buffer)))
        {
            if (buffer[0] == '\0') return sglv_aborted;
            i = atoi(buffer);
            if (i < 1 || i > sgv->num)
                dbg_printf("Invalid choice %d\n", i);
        }
        else return sglv_aborted;
    } while (i < 1 || i > sgv->num);

    /* The array is 0-based, but the choices are 1..n,
     * so we have to subtract one before returning.
     */
    *rtn = sgv->syms[i - 1].lvalue;
    return sglv_found;
}

enum sym_get_lval symbol_picker_scoped(const char* name, const struct sgv_data* sgv,
                                       struct dbg_lvalue* rtn)
{
    unsigned i;
    int local = -1;

    for (i = 0; i < sgv->num; i++)
    {
        if (sgv->num - sgv->num_thunks > 1 && (sgv->syms[i].flags & SYMFLAG_THUNK) && !DBG_IVAR(AlwaysShowThunks))
            continue;
        if (sgv->syms[i].flags & (SYMFLAG_LOCAL | SYMFLAG_PARAMETER))
        {
            if (local == -1)
                local = i;
            else
            {
                /* FIXME: several locals with same name... which one to pick ?? */
                dbg_printf("Several local variables/parameters for %s, aborting\n", name);
                return sglv_aborted;
            }
        }
    }
    if (local != -1)
    {
        *rtn = sgv->syms[local].lvalue;
        return sglv_found;
    }
    /* no locals found, multiple globals... abort for now */
    dbg_printf("Several global variables for %s, aborting\n", name);
    return sglv_aborted;
}

symbol_picker_t symbol_current_picker = symbol_picker_interactive;

/***********************************************************************
 *           symbol_get_lvalue
 *
 * Get the address of a named symbol.
 * Return values:
 *      sglv_found:   if the symbol is found
 *      sglv_unknown: if the symbol isn't found
 *      sglv_aborted: some error occurred (likely, many symbols of same name exist,
 *          and user didn't pick one of them)
 */
enum sym_get_lval symbol_get_lvalue(const char* name, const int lineno,
                                    struct dbg_lvalue* rtn, BOOL bp_disp)
{
    struct sgv_data             sgv;
    int		                i;
    char                        buffer[512];
    BOOL                        opt;
    IMAGEHLP_STACK_FRAME        ihsf;

    if (strlen(name) + 4 > sizeof(buffer))
    {
        WINE_WARN("Too long symbol (%s)\n", name);
        return sglv_unknown;
    }

    sgv.num        = 0;
    sgv.num_thunks = 0;
    sgv.name       = &buffer[2];
    sgv.do_thunks  = DBG_IVAR(AlwaysShowThunks);

    if (strchr(name, '!'))
    {
        strcpy(buffer, name);
    }
    else
    {
        buffer[0] = '*';
        buffer[1] = '!';
        strcpy(&buffer[2], name);
    }

    /* this is a wine specific options to return also ELF modules in the
     * enumeration
     */
    opt = SymSetExtendedOption(SYMOPT_EX_WINE_NATIVE_MODULES, TRUE);
    SymEnumSymbols(dbg_curr_process->handle, 0, buffer, sgv_cb, (void*)&sgv);

    if (!sgv.num)
    {
        const char*   ptr = strchr(name, '!');
        if ((ptr && ptr[1] != '_') || (!ptr && *name != '_'))
        {
            if (ptr)
            {
                int offset = ptr - name;
                memcpy(buffer, name, offset + 1);
                buffer[offset + 1] = '_';
                strcpy(&buffer[offset + 2], ptr + 1);
            }
            else
            {
                buffer[0] = '*';
                buffer[1] = '!';
                buffer[2] = '_';
                strcpy(&buffer[3], name);
            }
            SymEnumSymbols(dbg_curr_process->handle, 0, buffer, sgv_cb, (void*)&sgv);
        }
    }
    SymSetExtendedOption(SYMOPT_EX_WINE_NATIVE_MODULES, opt);

    /* now grab local symbols */
    if (stack_get_current_frame(&ihsf) && sgv.num < NUMDBGV)
    {
        sgv.frame_offset = ihsf.FrameOffset;
        SymEnumSymbols(dbg_curr_process->handle, 0, name, sgv_cb, (void*)&sgv);
    }

    if (!sgv.num)
    {
        dbg_printf("No symbols found for %s\n", name);
        return sglv_unknown;
    }

    /* recompute potential offsets for functions (linenumber, skip prolog) */
    for (i = 0; i < sgv.num; i++)
    {
        if (sgv.syms[i].flags & (SYMFLAG_REGISTER|SYMFLAG_REGREL|SYMFLAG_LOCAL|SYMFLAG_THUNK))
            continue;

        if (lineno == -1)
        {
            struct dbg_type     type;
            ULONG64             addr;

            type.module = sgv.syms[i].lvalue.type.module;
            type.id     = sgv.syms[i].sym_info;
            if (bp_disp && symbol_get_debug_start(&type, &addr))
                sgv.syms[i].lvalue.addr.Offset = addr;
        }
        else
        {
            DWORD               disp;
            IMAGEHLP_LINE64     il;
            BOOL                found = FALSE;

            il.SizeOfStruct = sizeof(il);
            SymGetLineFromAddr64(dbg_curr_process->handle,
                                 (DWORD_PTR)memory_to_linear_addr(&sgv.syms[i].lvalue.addr),
				 &disp, &il);
            do
            {
                if (lineno == il.LineNumber)
                {
                    sgv.syms[i].lvalue.addr.Offset = il.Address;
                    found = TRUE;
                    break;
                }
            } while (SymGetLineNext64(dbg_curr_process->handle, &il));
            if (!found)
                WINE_FIXME("No line (%d) found for %s (setting to symbol start)\n",
                           lineno, name);
        }
    }

    if (sgv.num - sgv.num_thunks > 1 || /* many symbols non thunks (and showing only non thunks) */
        (sgv.num > 1 && DBG_IVAR(AlwaysShowThunks)) || /* many symbols (showing symbols & thunks) */
        (sgv.num == sgv.num_thunks && sgv.num_thunks > 1))
    {
        return symbol_current_picker(name, &sgv, rtn);
    }
    /* first symbol is the one we want:
     * - only one symbol found,
     * - or many symbols but only one non thunk when AlwaysShowThunks is FALSE
     */
    *rtn = sgv.syms[0].lvalue;
    return sglv_found;
}

BOOL symbol_is_local(const char* name)
{
    struct sgv_data             sgv;
    IMAGEHLP_STACK_FRAME        ihsf;

    sgv.num        = 0;
    sgv.num_thunks = 0;
    sgv.name       = name;
    sgv.do_thunks  = FALSE;

    if (stack_get_current_frame(&ihsf))
    {
        sgv.frame_offset = ihsf.FrameOffset;
        SymEnumSymbols(dbg_curr_process->handle, 0, name, sgv_cb, (void*)&sgv);
    }
    return sgv.num > 0;
}

/***********************************************************************
 *           symbol_read_symtable
 *
 * Read a symbol file into the hash table.
 */
void symbol_read_symtable(const char* filename, unsigned long offset)
{
    dbg_printf("No longer supported\n");

#if 0
/* FIXME: have to implement SymAddSymbol in dbghelp, but likely we'll need to link
 * this with an already loaded module !! 
 */
    FILE*       symbolfile;
    unsigned    addr;
    char        type;
    char*       cpnt;
    char        buffer[256];
    char        name[256];

    if (!(symbolfile = fopen(filename, "r")))
    {
        WINE_WARN("Unable to open symbol table %s\n", filename);
        return;
    }

    dbg_printf("Reading symbols from file %s\n", filename);

    while (1)
    {
        fgets(buffer, sizeof(buffer), symbolfile);
        if (feof(symbolfile)) break;

        /* Strip any text after a # sign (i.e. comments) */
        cpnt = strchr(buffer, '#');
        if (cpnt) *cpnt = '\0';

        /* Quietly ignore any lines that have just whitespace */
        for (cpnt = buffer; *cpnt; cpnt++)
        {
            if (*cpnt != ' ' && *cpnt != '\t') break;
        }
        if (!*cpnt || *cpnt == '\n') continue;

        if (sscanf(buffer, "%lx %c %s", &addr, &type, name) == 3)
        {
            if (value.addr.off + offset < value.addr.off)
                WINE_WARN("Address wrap around\n");
            value.addr.off += offset;
            SymAddSymbol(current_process->handle, BaseOfDll,
                         name, addr, 0, 0);
        }
    }
    fclose(symbolfile);
#endif
}

/***********************************************************************
 *           symbol_get_function_line_status
 *
 * Find the symbol nearest to a given address.
 */
enum dbg_line_status symbol_get_function_line_status(const ADDRESS64* addr)
{
    IMAGEHLP_LINE64     il;
    DWORD               disp;
    ULONG64             disp64, start;
    DWORD_PTR           lin = (DWORD_PTR)memory_to_linear_addr(addr);
    char                buffer[sizeof(SYMBOL_INFO) + 256];
    SYMBOL_INFO*        sym = (SYMBOL_INFO*)buffer;
    struct dbg_type     func;

    il.SizeOfStruct = sizeof(il);
    sym->SizeOfStruct = sizeof(SYMBOL_INFO);
    sym->MaxNameLen = sizeof(buffer) - sizeof(SYMBOL_INFO);

    /* do we have some info for lin address ? */
    if (!SymFromAddr(dbg_curr_process->handle, lin, &disp64, sym))
    {
        ADDRESS64   jumpee;
        /* some compilers insert thunks in their code without debug info associated
         * take care of this situation
         */
        if (dbg_curr_process->be_cpu->is_jump((void*)lin, &jumpee))
            return symbol_get_function_line_status(&jumpee);
        return dbg_no_line_info;
    }

    switch (sym->Tag)
    {
    case SymTagThunk:
        /* FIXME: so far dbghelp doesn't return the 16 <=> 32 thunks
         * and furthermore, we no longer take care of them !!!
         */
        return dbg_in_a_thunk;
    case SymTagFunction:
    case SymTagPublicSymbol: break;
    default:
        WINE_FIXME("Unexpected sym-tag 0x%08x\n", sym->Tag);
    case SymTagData:
        return dbg_no_line_info;
    }
    /* we should have a function now */
    if (!SymGetLineFromAddr64(dbg_curr_process->handle, lin, &disp, &il))
        return dbg_no_line_info;

    func.module = sym->ModBase;
    func.id     = sym->Index;

    if (symbol_get_debug_start(&func, &start) && lin < start)
        return dbg_not_on_a_line_number;

    if (!sym->Size) sym->Size = 0x100000;
    if (il.FileName && il.FileName[0] && disp < sym->Size)
        return (disp == 0) ? dbg_on_a_line_number : dbg_not_on_a_line_number;

    return dbg_no_line_info;
}

/***********************************************************************
 *           symbol_get_line
 *
 * Find the symbol nearest to a given address.
 * Returns sourcefile name and line number in a format that the listing
 * handler can deal with.
 */
BOOL symbol_get_line(const char* filename, const char* name,
		     IMAGEHLP_LINE64* line)
{
    struct sgv_data     sgv;
    char                buffer[512];
    DWORD               opt, disp;
    unsigned            i;
    BOOL                found = FALSE;
    IMAGEHLP_LINE64     il;

    sgv.num        = 0;
    sgv.num_thunks = 0;
    sgv.name       = &buffer[2];
    sgv.do_thunks  = FALSE;

    buffer[0] = '*';
    buffer[1] = '!';
    strcpy(&buffer[2], name);

    /* this is a wine specific options to return also ELF modules in the
     * enumeration
     */
    opt = SymSetExtendedOption(SYMOPT_EX_WINE_NATIVE_MODULES, TRUE);
    if (!SymEnumSymbols(dbg_curr_process->handle, 0, buffer, sgv_cb, (void*)&sgv))
    {
        SymSetExtendedOption(SYMOPT_EX_WINE_NATIVE_MODULES, opt);
        return FALSE;
    }

    if (!sgv.num && (name[0] != '_'))
    {
        buffer[2] = '_';
        strcpy(&buffer[3], name);
        if (!SymEnumSymbols(dbg_curr_process->handle, 0, buffer, sgv_cb, (void*)&sgv))
        {
            SymSetExtendedOption(SYMOPT_EX_WINE_NATIVE_MODULES, opt);
            return FALSE;
        }
    }
    SymSetExtendedOption(SYMOPT_EX_WINE_NATIVE_MODULES, opt);

    for (i = 0; i < sgv.num; i++)
    {
        DWORD_PTR linear = (DWORD_PTR)memory_to_linear_addr(&sgv.syms[i].lvalue.addr);

        il.SizeOfStruct = sizeof(il);
        if (!SymGetLineFromAddr64(dbg_curr_process->handle, linear, &disp, &il))
            continue;
        if (filename && strcmp(il.FileName, filename)) continue;
        if (found)
        {
            WINE_FIXME("Several found, returning first (may not be what you want)...\n");
            break;
        }
        found = TRUE;
        *line = il;
    }
    if (!found)
    {
        if (filename)   dbg_printf("No such function %s in %s\n", name, filename);
	else            dbg_printf("No such function %s\n", name);
        return FALSE;
    }
    return TRUE;
}

/******************************************************************
 *		symbol_print_local
 *
 * Overall format is:
 * <name>=<value>                       in non detailed form
 * <name>=<value> (local|pmt <where>)   in detailed form
 * Note <value> can be an error message in case of error
 */
void symbol_print_local(const SYMBOL_INFO* sym, DWORD_PTR base, BOOL detailed)
{
    struct dbg_lvalue   lvalue;
    char                buffer[64];

    dbg_printf("%s=", sym->Name);

    if (fill_sym_lvalue(sym, base, &lvalue, buffer, sizeof(buffer)))
    {
        print_value(&lvalue, 0, 1);
        if (detailed)
            dbg_printf(" (%s %s)",
                       (sym->Flags & SYMFLAG_PARAMETER) ? "parameter" : "local",
                       buffer);
    }
    else
    {
        dbg_printf("%s", buffer);
        if (detailed)
            dbg_printf(" (%s)",
                       (sym->Flags & SYMFLAG_PARAMETER) ? "parameter" : "local");
    }
}

static BOOL CALLBACK info_locals_cb(PSYMBOL_INFO sym, ULONG size, PVOID ctx)
{
    struct dbg_type     type;

    dbg_printf("\t");
    type.module = sym->ModBase;
    type.id = sym->TypeIndex;
    types_print_type(&type, FALSE);

    dbg_printf(" ");
    symbol_print_local(sym, (DWORD_PTR)ctx, TRUE);
    dbg_printf("\n");

    return TRUE;
}

BOOL symbol_info_locals(void)
{
    IMAGEHLP_STACK_FRAME        ihsf;
    ADDRESS64                   addr;

    stack_get_current_frame(&ihsf);
    addr.Mode = AddrModeFlat;
    addr.Offset = ihsf.InstructionOffset;
    print_address(&addr, FALSE);
    dbg_printf(": (%08lx)\n", (DWORD_PTR)ihsf.FrameOffset);
    SymEnumSymbols(dbg_curr_process->handle, 0, NULL, info_locals_cb, (void*)(DWORD_PTR)ihsf.FrameOffset);

    return TRUE;

}

static BOOL CALLBACK symbols_info_cb(PSYMBOL_INFO sym, ULONG size, PVOID ctx)
{
    struct dbg_type     type;
    IMAGEHLP_MODULE     mi;

    mi.SizeOfStruct = sizeof(mi);

    if (!SymGetModuleInfo(dbg_curr_process->handle, sym->ModBase, &mi))
        mi.ModuleName[0] = '\0';
    else
    {
        size_t  len = strlen(mi.ModuleName);
        if (len > 5 && !strcmp(mi.ModuleName + len - 5, "<elf>"))
            mi.ModuleName[len - 5] = '\0';
    }

    dbg_printf("%08lx: %s!%s", (ULONG_PTR)sym->Address, mi.ModuleName, sym->Name);
    type.id = sym->TypeIndex;
    type.module = sym->ModBase;

    if (sym->TypeIndex != dbg_itype_none && sym->TypeIndex != 0)
    {
        dbg_printf(" ");
        types_print_type(&type, FALSE);
    }
    dbg_printf("\n");
    return TRUE;
}

void symbol_info(const char* str)
{
    char        buffer[512];
    BOOL        opt;

    if (strlen(str) + 3 >= sizeof(buffer))
    {
        dbg_printf("Symbol too long (%s)\n", str);
        return;
    }
    buffer[0] = '*';
    buffer[1] = '!';
    strcpy(&buffer[2], str);
    /* this is a wine specific options to return also ELF modules in the
     * enumeration
     */
    opt = SymSetExtendedOption(SYMOPT_EX_WINE_NATIVE_MODULES, TRUE);
    SymEnumSymbols(dbg_curr_process->handle, 0, buffer, symbols_info_cb, NULL);
    SymSetExtendedOption(SYMOPT_EX_WINE_NATIVE_MODULES, opt);
}
