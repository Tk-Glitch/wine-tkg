/*
 * Debugger definitions
 *
 * Copyright 1995 Alexandre Julliard
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

#ifndef __WINE_DEBUGGER_H
#define __WINE_DEBUGGER_H

#include <assert.h>
#include <stdarg.h>

#define NONAMELESSUNION
#define NONAMELESSSTRUCT
#include "ntstatus.h"
#define WIN32_NO_STATUS
#define WIN32_LEAN_AND_MEAN
#include "windef.h"
#include "winbase.h"
#include "winver.h"
#include "winternl.h"
#include "dbghelp.h"
#include "cvconst.h"
#include "objbase.h"
#include "oaidl.h"
#include <wine/list.h>

#define ADDRSIZE        (dbg_curr_process->be_cpu->pointer_size)
#define ADDRWIDTH       (ADDRSIZE * 2)

/* the debugger uses these exceptions for its internal use */
#define	DEBUG_STATUS_OFFSET		0x80003000
#define	DEBUG_STATUS_INTERNAL_ERROR	(DEBUG_STATUS_OFFSET+0) /* something went wrong */
#define	DEBUG_STATUS_NO_SYMBOL		(DEBUG_STATUS_OFFSET+1) /* no symbol found in lookup */
#define	DEBUG_STATUS_DIV_BY_ZERO	(DEBUG_STATUS_OFFSET+2)
#define	DEBUG_STATUS_BAD_TYPE		(DEBUG_STATUS_OFFSET+3) /* no type found, when type was expected */
#define DEBUG_STATUS_NO_FIELD		(DEBUG_STATUS_OFFSET+4) /* when dereferencing a struct, the field was not found */
#define DEBUG_STATUS_ABORT              (DEBUG_STATUS_OFFSET+5) /* user aborted on going action */
#define DEBUG_STATUS_CANT_DEREF         (DEBUG_STATUS_OFFSET+6) /* either not deref:able, or index out of bounds */
#define DEBUG_STATUS_NOT_AN_INTEGER     (DEBUG_STATUS_OFFSET+7) /* requiring an integral value */

/*
 * Return values for symbol_get_function_line_status.  Used to determine
 * what to do when the 'step' command is given.
 */
enum dbg_line_status
{
    dbg_no_line_info,
    dbg_not_on_a_line_number,
    dbg_on_a_line_number,
    dbg_in_a_thunk,
};

enum dbg_internal_types
{
    dbg_itype_first             = 0xffffff00,
    dbg_itype_unsigned_int,
    dbg_itype_signed_int,
    dbg_itype_signed_char_int,
    dbg_itype_unsigned_char_int,
    dbg_itype_unsigned_short_int,
    dbg_itype_signed_short_int,
    dbg_itype_unsigned_long_int,
    dbg_itype_signed_long_int,
    dbg_itype_unsigned_longlong_int,
    dbg_itype_signed_longlong_int,
    dbg_itype_char,
    dbg_itype_wchar,
    dbg_itype_short_real, /* aka float */
    dbg_itype_real,       /* aka double */
    dbg_itype_long_real,  /* aka long double */
    dbg_itype_astring,
    dbg_itype_ustring,
    dbg_itype_segptr,     /* hack for segmented pointers */
    dbg_itype_m128a,      /* 128-bit (XMM) registers */
    dbg_itype_none              = 0xffffffff
};

/* type description (in the following order):
 * - if 'id' is dbg_itype_none (whatever 'module' value), the type isn't known
 * - if 'module' is 0, it's an internal type (id is one of dbg_itype...)
 * - if 'module' is non 0, then 'id' is a type ID referring to module (loaded in
 *   dbghelp) which (linear) contains address 'module'.
 */
struct dbg_type
{
    unsigned long       id;
    DWORD_PTR           module;
};

struct dbg_lvalue       /* structure to hold left-values... */
{
    int			cookie;	/* DLV_??? */
/* DLV_TARGET references an address in debuggee's address space, whereas DLV_HOST
 * references the winedbg's address space
 */
#	define	DLV_TARGET	0xF00D
#	define	DLV_HOST	0x50DA
    ADDRESS64           addr;
    struct dbg_type     type;
};

enum dbg_exec_mode
{
    dbg_exec_cont,       		/* Continue execution */
    dbg_exec_step_over_line,  		/* Stepping over a call to next source line */
    dbg_exec_step_into_line,  		/* Step to next source line, stepping in if needed */
    dbg_exec_step_over_insn,  		/* Stepping over a call */
    dbg_exec_step_into_insn,  		/* Single-stepping an instruction */
    dbg_exec_finish,		        /* Single-step until we exit current frame */
#if 0
    EXEC_STEP_OVER_TRAMPOLINE, 	/* Step over trampoline.  Requires that we dig the real
                                 * return value off the stack and set breakpoint there - 
                                 * not at the instr just after the call.
				 */
#endif
};

struct dbg_breakpoint
{
    ADDRESS64           addr;
    unsigned long       enabled : 1,
                        xpoint_type : 2,
                        refcount : 13,
                        skipcount : 16;
    unsigned long       info;
    struct              /* only used for watchpoints */
    {
        BYTE		len : 2;
        DWORD64		oldval;
    } w;
    struct expr*        condition;
};

/* used for C++ exceptions in msvcrt
 * parameters:
 * [0] CXX_FRAME_MAGIC
 * [1] pointer to exception object
 * [2] pointer to type
 */
#define CXX_EXCEPTION                       0xe06d7363
#define CXX_FRAME_MAGIC                     0x19930520

/* Wine extension; Windows doesn't have a name for this code.  This is an
   undocumented exception understood by MS VC debugger, allowing the program
   to name a particular thread.  Search google.com or deja.com for "0x406d1388"
   for more info. */
#define EXCEPTION_NAME_THREAD               0x406D1388

/* Helper structure */
typedef struct tagTHREADNAME_INFO
{
   DWORD   dwType;     /* Must be 0x1000 */
   LPCSTR  szName;     /* Pointer to name - limited to 9 bytes (8 characters + terminator) */
   DWORD   dwThreadID; /* Thread ID (-1 = caller thread) */
   DWORD   dwFlags;    /* Reserved for future use.  Must be zero. */
} THREADNAME_INFO;

typedef union dbg_ctx
{
    CONTEXT ctx;
    WOW64_CONTEXT x86;
} dbg_ctx_t;

struct dbg_thread
{
    struct list                 entry;
    struct dbg_process* 	process;
    HANDLE			handle;
    DWORD			tid;
    void*			teb;
    enum dbg_exec_mode          exec_mode;      /* mode the thread is run (step/run...) */
    int			        exec_count;     /* count of mode operations */
    ADDRESS_MODE	        addr_mode;      /* mode */
    int                         stopped_xpoint; /* xpoint on which the thread has stopped (-1 if none) */
    struct dbg_breakpoint	step_over_bp;
    char                        name[9];
    BOOL                        in_exception;   /* TRUE if thread stopped with an exception */
    BOOL                        first_chance;   /* TRUE if thread stopped with a first chance exception
                                                 *      - only valid when in_exception is TRUE
                                                 */
    EXCEPTION_RECORD            excpt_record;   /* only valid when in_exception is TRUE */
    struct
    {
        ADDRESS64               addr_pc;
        ADDRESS64               addr_frame;
        ADDRESS64               addr_stack;
        DWORD_PTR               linear_pc;
        DWORD_PTR               linear_frame;
        DWORD_PTR               linear_stack;
        dbg_ctx_t               context;        /* context we got out of stackwalk for this frame */
        BOOL                    is_ctx_valid;   /* is the context above valid */
    }*                          frames;
    int                         num_frames;
    int                         curr_frame;
    BOOL                        suspended;
};

struct dbg_delayed_bp
{
    BOOL                        is_symbol;
    BOOL                        software_bp;
    union
    {
        struct
        {
            int				lineno;
            char*			name;
        } symbol;
        ADDRESS64               addr;
    } u;
};

#define MAX_BREAKPOINTS 100
struct dbg_process
{
    struct list                 entry;
    HANDLE			handle;
    DWORD			pid;
    const struct be_process_io* process_io;
    void*                       pio_data;
    const WCHAR*		imageName;
    struct list           	threads;
    struct backend_cpu*         be_cpu;
    HANDLE                      event_on_first_exception;
    BOOL                        active_debuggee;
    struct dbg_breakpoint       bp[MAX_BREAKPOINTS];
    unsigned                    next_bp;
    struct dbg_delayed_bp*      delayed_bp;
    int				num_delayed_bp;
    struct open_file_list*      source_ofiles;
    char*                       search_path;
    char                        source_current_file[MAX_PATH];
    int                         source_start_line;
    int                         source_end_line;
};

/* describes the way the debugger interacts with a given process */
struct be_process_io
{
    BOOL        (*close_process)(struct dbg_process*, BOOL);
    BOOL        (*read)(HANDLE, const void*, void*, SIZE_T, SIZE_T*);
    BOOL        (*write)(HANDLE, void*, const void*, SIZE_T, SIZE_T*);
    BOOL        (*get_selector)(HANDLE, DWORD, LDT_ENTRY*);
};

extern	struct dbg_process*	dbg_curr_process;
extern	DWORD_PTR	        dbg_curr_pid;
extern	struct dbg_thread*	dbg_curr_thread;
extern	DWORD_PTR	        dbg_curr_tid;
extern  dbg_ctx_t               dbg_context;
extern  BOOL                    dbg_interactiveP;
extern  HANDLE                  dbg_houtput;

struct dbg_internal_var
{
    DWORD_PTR		        val;
    const char*		        name;
    DWORD_PTR		        *pval;
    unsigned long               typeid; /* always internal type */
};

enum sym_get_lval {sglv_found, sglv_unknown, sglv_aborted};

enum type_expr_e
{
    type_expr_type_id,
    type_expr_udt_class,
    type_expr_udt_struct,
    type_expr_udt_union,
    type_expr_enumeration
};

struct type_expr_t
{ 
    enum type_expr_e    type;
    unsigned            deref_count;
    union
    {
        struct dbg_type type;
        const char*     name;
    } u;
};

enum dbg_start {start_ok, start_error_parse, start_error_init};

  /* break.c */
extern void             break_set_xpoints(BOOL set);
extern BOOL             break_add_break(const ADDRESS64* addr, BOOL verbose, BOOL swbp);
extern BOOL             break_add_break_from_lvalue(const struct dbg_lvalue* value, BOOL swbp);
extern void             break_add_break_from_id(const char* name, int lineno, BOOL swbp);
extern void             break_add_break_from_lineno(const char *filename, int lineno, BOOL swbp);
extern void             break_add_watch_from_lvalue(const struct dbg_lvalue* lvalue, BOOL is_write);
extern void             break_add_watch_from_id(const char* name, BOOL is_write);
extern void             break_check_delayed_bp(void);
extern void             break_delete_xpoint(int num);
extern void             break_delete_xpoints_from_module(DWORD64 base);
extern void             break_enable_xpoint(int num, BOOL enable);
extern void             break_info(void);
extern void             break_adjust_pc(ADDRESS64* addr, DWORD code, BOOL first_chance, BOOL* is_break);
extern BOOL             break_should_continue(ADDRESS64* addr, DWORD code);
extern void             break_suspend_execution(void);
extern void             break_restart_execution(int count);
extern int              break_add_condition(int bpnum, struct expr* exp);

  /* crashdlg.c */
extern int              display_crash_dialog(void);
extern HANDLE           display_crash_details(HANDLE event);
extern int              msgbox_res_id(HWND hwnd, UINT textId, UINT captionId, UINT uType);

  /* dbg.y */
extern void             parser_handle(HANDLE);
extern int              input_read_line(const char* pfx, char* buffer, int size);
extern int              input_fetch_entire_line(const char* pfx, char** line);
extern HANDLE           parser_generate_command_file(const char*, ...);

  /* debug.l */
extern void             lexeme_flush(void);
extern char*            lexeme_alloc_size(int);

  /* display.c */
extern BOOL             display_print(void);
extern BOOL             display_add(struct expr* exp, int count, char format);
extern BOOL             display_delete(int displaynum);
extern BOOL             display_info(void);
extern BOOL             display_enable(int displaynum, int enable);

  /* expr.c */
extern void             expr_free_all(void);
extern struct expr*     expr_alloc_internal_var(const char* name);
extern struct expr*     expr_alloc_symbol(const char* name);
extern struct expr*     expr_alloc_sconstant(long int val);
extern struct expr*     expr_alloc_uconstant(long unsigned val);
extern struct expr*     expr_alloc_string(const char* str);
extern struct expr*     expr_alloc_binary_op(int oper, struct expr*, struct expr*);
extern struct expr*     expr_alloc_unary_op(int oper, struct expr*);
extern struct expr*     expr_alloc_pstruct(struct expr*, const char* element);
extern struct expr*     expr_alloc_struct(struct expr*, const char* element);
extern struct expr*     expr_alloc_func_call(const char*, int nargs, ...);
extern struct expr*     expr_alloc_typecast(struct type_expr_t*, struct expr*);
extern struct dbg_lvalue expr_eval(struct expr*);
extern struct expr*     expr_clone(const struct expr* exp, BOOL *local_binding);
extern BOOL             expr_free(struct expr* exp);
extern BOOL             expr_print(const struct expr* exp);

  /* info.c */
extern void             print_help(void);
extern void             info_help(void);
extern void             info_win32_module(DWORD64 mod);
extern void             info_win32_class(HWND hWnd, const char* clsName);
extern void             info_win32_window(HWND hWnd, BOOL detailed);
extern void             info_win32_processes(void);
extern void             info_win32_threads(void);
extern void             info_win32_frame_exceptions(DWORD tid);
extern void             info_win32_virtual(DWORD pid);
extern void             info_win32_segments(DWORD start, int length);
extern void             info_win32_exception(void);
extern void             info_wine_dbg_channel(BOOL add, const char* chnl, const char* name);

  /* memory.c */
extern BOOL             memory_read_value(const struct dbg_lvalue* lvalue, DWORD size, void* result);
extern BOOL             memory_write_value(const struct dbg_lvalue* val, DWORD size, void* value);
extern void             memory_examine(const struct dbg_lvalue *lvalue, int count, char format);
extern void*            memory_to_linear_addr(const ADDRESS64* address);
extern BOOL             memory_get_current_pc(ADDRESS64* address);
extern BOOL             memory_get_current_stack(ADDRESS64* address);
extern BOOL             memory_get_string(struct dbg_process* pcs, void* addr, BOOL in_debuggee, BOOL unicode, char* buffer, int size);
extern BOOL             memory_get_string_indirect(struct dbg_process* pcs, void* addr, BOOL unicode, WCHAR* buffer, int size);
extern BOOL             memory_get_register(DWORD regno, DWORD_PTR** value, char* buffer, int len);
extern void             memory_disassemble(const struct dbg_lvalue*, const struct dbg_lvalue*, int instruction_count);
extern BOOL             memory_disasm_one_insn(ADDRESS64* addr);
#define MAX_OFFSET_TO_STR_LEN 19
extern char*            memory_offset_to_string(char *str, DWORD64 offset, unsigned mode);
extern void             print_bare_address(const ADDRESS64* addr);
extern void             print_address(const ADDRESS64* addr, BOOLEAN with_line);
extern void             print_basic(const struct dbg_lvalue* value, char format);

  /* source.c */
extern void             source_list(IMAGEHLP_LINE64* src1, IMAGEHLP_LINE64* src2, int delta);
extern void             source_list_from_addr(const ADDRESS64* addr, int nlines);
extern void             source_show_path(void);
extern void             source_add_path(const char* path);
extern void             source_nuke_path(struct dbg_process* p);
extern void             source_free_files(struct dbg_process* p);

  /* stack.c */
extern void             stack_info(int len);
extern void             stack_backtrace(DWORD threadID);
extern BOOL             stack_set_frame(int newframe);
extern BOOL             stack_get_current_frame(IMAGEHLP_STACK_FRAME* ihsf);
extern BOOL             stack_get_register_frame(const struct dbg_internal_var* div, DWORD_PTR** pval);
extern unsigned         stack_fetch_frames(const dbg_ctx_t *ctx);
extern BOOL             stack_get_current_symbol(SYMBOL_INFO* sym);

  /* symbol.c */
extern enum sym_get_lval symbol_get_lvalue(const char* name, const int lineno, struct dbg_lvalue* addr, BOOL bp_disp);
extern void             symbol_read_symtable(const char* filename, unsigned long offset);
extern enum dbg_line_status symbol_get_function_line_status(const ADDRESS64* addr);
extern BOOL             symbol_get_line(const char* filename, const char* func, IMAGEHLP_LINE64* ret);
extern void             symbol_info(const char* str);
extern void             symbol_print_local(const SYMBOL_INFO* sym, DWORD_PTR base, BOOL detailed);
extern BOOL             symbol_info_locals(void);
extern BOOL             symbol_is_local(const char* name);
struct sgv_data;
typedef enum sym_get_lval (*symbol_picker_t)(const char* name, const struct sgv_data* sgv,
                                             struct dbg_lvalue* rtn);
extern symbol_picker_t symbol_current_picker;
extern enum sym_get_lval symbol_picker_interactive(const char* name, const struct sgv_data* sgv,
                                                   struct dbg_lvalue* rtn);
extern enum sym_get_lval symbol_picker_scoped(const char* name, const struct sgv_data* sgv,
                                              struct dbg_lvalue* rtn);

  /* tgt_active.c */
extern void             dbg_run_debuggee(const char* args);
extern void             dbg_wait_next_exception(DWORD cont, int count, int mode);
extern enum dbg_start   dbg_active_attach(int argc, char* argv[]);
extern BOOL             dbg_set_curr_thread(DWORD tid);
extern enum dbg_start   dbg_active_launch(int argc, char* argv[]);
extern enum dbg_start   dbg_active_auto(int argc, char* argv[]);
extern enum dbg_start   dbg_active_minidump(int argc, char* argv[]);
extern void             dbg_active_wait_for_first_exception(void);
extern BOOL             dbg_attach_debuggee(DWORD pid);

  /* tgt_minidump.c */
extern void             minidump_write(const char*, const EXCEPTION_RECORD*);
extern enum dbg_start   minidump_reload(int argc, char* argv[]);

  /* tgt_module.c */
extern enum dbg_start   tgt_module_load(const char* name, BOOL keep);

  /* types.c */
extern void             print_value(const struct dbg_lvalue* addr, char format, int level);
extern BOOL             types_print_type(const struct dbg_type*, BOOL details);
extern BOOL             print_types(void);
extern long int         types_extract_as_integer(const struct dbg_lvalue*);
extern LONGLONG         types_extract_as_longlong(const struct dbg_lvalue*, unsigned* psize, BOOL *pissigned);
extern void             types_extract_as_address(const struct dbg_lvalue*, ADDRESS64*);
extern BOOL             types_store_value(struct dbg_lvalue* lvalue_to, const struct dbg_lvalue* lvalue_from);
extern BOOL             types_udt_find_element(struct dbg_lvalue* value, const char* name, long int* tmpbuf);
extern BOOL             types_array_index(const struct dbg_lvalue* value, int index, struct dbg_lvalue* result);
extern BOOL             types_get_info(const struct dbg_type*, IMAGEHLP_SYMBOL_TYPE_INFO, void*);
extern BOOL             types_get_real_type(struct dbg_type* type, DWORD* tag);
extern struct dbg_type  types_find_pointer(const struct dbg_type* type);
extern struct dbg_type  types_find_type(unsigned long linear, const char* name, enum SymTagEnum tag);

  /* winedbg.c */
extern void	        dbg_outputW(const WCHAR* buffer, int len);
extern const char*      dbg_W2A(const WCHAR* buffer, unsigned len);
#ifdef __GNUC__
extern int	        dbg_printf(const char* format, ...) __attribute__((format (printf,1,2)));
#else
extern int	        dbg_printf(const char* format, ...);
#endif
extern const struct dbg_internal_var* dbg_get_internal_var(const char*);
extern BOOL             dbg_interrupt_debuggee(void);
extern unsigned         dbg_num_processes(void);
extern struct dbg_process* dbg_add_process(const struct be_process_io* pio, DWORD pid, HANDLE h);
extern void             dbg_set_process_name(struct dbg_process* p, const WCHAR* name);
extern struct dbg_process* dbg_get_process(DWORD pid);
extern struct dbg_process* dbg_get_process_h(HANDLE handle);
extern void             dbg_del_process(struct dbg_process* p);
struct dbg_thread*	dbg_add_thread(struct dbg_process* p, DWORD tid, HANDLE h, void* teb);
extern struct dbg_thread* dbg_get_thread(struct dbg_process* p, DWORD tid);
extern void             dbg_del_thread(struct dbg_thread* t);
extern BOOL             dbg_init(HANDLE hProc, const WCHAR* in, BOOL invade);
extern BOOL             dbg_load_module(HANDLE hProc, HANDLE hFile, const WCHAR* name, DWORD_PTR base, DWORD size);
extern BOOL             dbg_get_debuggee_info(HANDLE hProcess, IMAGEHLP_MODULE64* imh_mod);
extern void             dbg_set_option(const char*, const char*);
extern void             dbg_start_interactive(HANDLE hFile);
extern void             dbg_init_console(void);

  /* gdbproxy.c */
extern int              gdb_main(int argc, char* argv[]);

static inline BOOL dbg_read_memory(const void* addr, void* buffer, size_t len)
{
    SIZE_T rlen;
    return dbg_curr_process->process_io->read(dbg_curr_process->handle, addr, buffer, len, &rlen) && len == rlen;
}

static inline BOOL dbg_write_memory(void* addr, const void* buffer, size_t len)
{
    SIZE_T wlen;
    return dbg_curr_process->process_io->write(dbg_curr_process->handle, addr, buffer, len, &wlen) && len == wlen;
}

static inline void* dbg_heap_realloc(void* buffer, size_t size)
{
    return (buffer) ? HeapReAlloc(GetProcessHeap(), 0, buffer, size) :
        HeapAlloc(GetProcessHeap(), 0, size);
}

extern struct dbg_internal_var          dbg_internal_vars[];

#define  DBG_IVARNAME(_var)	dbg_internal_var_##_var
#define  DBG_IVARSTRUCT(_var)	dbg_internal_vars[DBG_IVARNAME(_var)]
#define  DBG_IVAR(_var)		(*(DBG_IVARSTRUCT(_var).pval))
#define  INTERNAL_VAR(_var,_val,_ref,itype) DBG_IVARNAME(_var),
enum debug_int_var
{
#include "intvar.h"
   DBG_IV_LAST
};
#undef   INTERNAL_VAR

/* include CPU dependent bits */
#include "be_cpu.h"

#endif  /* __WINE_DEBUGGER_H */
