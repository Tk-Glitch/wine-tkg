/*
 * Wine debugger - back-end for an active target
 *
 * Copyright 2000-2006 Eric Pouech
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
#include <stdarg.h>

#include "debugger.h"
#include "psapi.h"
#include "resource.h"
#include "winternl.h"
#include "wine/debug.h"
#include "wine/exception.h"
#include "wine/unicode.h"

WINE_DEFAULT_DEBUG_CHANNEL(winedbg);

static char*            dbg_last_cmd_line;
static struct be_process_io be_process_active_io;

static void dbg_init_current_process(void)
{
}

static void dbg_init_current_thread(void* start)
{
    if (start)
    {
	if (list_count(&dbg_curr_process->threads) == 1 /* first thread ? */ &&
	    DBG_IVAR(BreakAllThreadsStartup))
        {
	    ADDRESS64   addr;

            break_set_xpoints(FALSE);
	    addr.Mode   = AddrModeFlat;
	    addr.Offset = (DWORD_PTR)start;
	    break_add_break(&addr, TRUE, TRUE);
	    break_set_xpoints(TRUE);
	}
    } 
}

static unsigned dbg_handle_debug_event(DEBUG_EVENT* de);

/******************************************************************
 *		dbg_attach_debuggee
 *
 * Sets the debuggee to <pid>
 * cofe instructs winedbg what to do when first exception is received 
 * (break=FALSE, continue=TRUE)
 * wfe is set to TRUE if dbg_attach_debuggee should also proceed with all debug events
 * until the first exception is received (aka: attach to an already running process)
 */
BOOL dbg_attach_debuggee(DWORD pid)
{
    if (!(dbg_curr_process = dbg_add_process(&be_process_active_io, pid, 0))) return FALSE;

    if (!DebugActiveProcess(pid)) 
    {
        dbg_printf("Can't attach process %04x: error %u\n", pid, GetLastError());
        dbg_del_process(dbg_curr_process);
	return FALSE;
    }

    SetEnvironmentVariableA("DBGHELP_NOLIVE", NULL);

    dbg_curr_process->active_debuggee = TRUE;
    return TRUE;
}

static unsigned dbg_fetch_context(void)
{
    if (!dbg_curr_process->be_cpu->get_context(dbg_curr_thread->handle, &dbg_context))
    {
        WINE_WARN("Can't get thread's context\n");
        return FALSE;
    }
    return TRUE;
}

BOOL dbg_set_curr_thread(DWORD tid)
{
    struct dbg_thread* thread;

    if (!dbg_curr_process)
    {
        dbg_printf("No process loaded\n");
        return FALSE;
    }

    thread = dbg_get_thread(dbg_curr_process, tid);
    if (thread)
    {
        dbg_curr_thread = thread;
        dbg_fetch_context();
        stack_fetch_frames(&dbg_context);
        dbg_curr_tid = tid;
        return TRUE;
    }
    dbg_printf("No such thread\n");
    return thread != NULL;
}

/***********************************************************************
 *              dbg_exception_prolog
 *
 * Examine exception and decide if interactive mode is entered(return TRUE)
 * or exception is silently continued(return FALSE)
 * is_debug means the exception is a breakpoint or single step exception
 */
static BOOL dbg_exception_prolog(BOOL is_debug, const EXCEPTION_RECORD* rec)
{
    ADDRESS64   addr;
    BOOL        is_break;

    memory_get_current_pc(&addr);
    break_suspend_execution();

    /* this will resynchronize builtin dbghelp's internal ELF module list */
    SymLoadModule(dbg_curr_process->handle, 0, 0, 0, 0, 0);

    if (is_debug) break_adjust_pc(&addr, rec->ExceptionCode, dbg_curr_thread->first_chance, &is_break);
    /*
     * Do a quiet backtrace so that we have an idea of what the situation
     * is WRT the source files.
     */
    stack_fetch_frames(&dbg_context);

    if (is_debug && !is_break && break_should_continue(&addr, rec->ExceptionCode))
	return FALSE;

    if (addr.Mode != dbg_curr_thread->addr_mode)
    {
        const char* name = NULL;

        switch (addr.Mode)
        {
        case AddrMode1616: name = "16 bit";     break;
        case AddrMode1632: name = "segmented 32 bit"; break;
        case AddrModeReal: name = "vm86";       break;
        case AddrModeFlat: name = dbg_curr_process->be_cpu->pointer_size == 4
                                  ? "32 bit" : "64 bit"; break;
        }
        dbg_printf("In %s mode.\n", name);
        dbg_curr_thread->addr_mode = addr.Mode;
    }
    display_print();

    if (!is_debug)
    {
	/* This is a real crash, dump some info */
        dbg_curr_process->be_cpu->print_context(dbg_curr_thread->handle, &dbg_context, 0);
	stack_info(-1);
        dbg_curr_process->be_cpu->print_segment_info(dbg_curr_thread->handle, &dbg_context);
	stack_backtrace(dbg_curr_tid);
    }
    else
    {
        static char*        last_name;
        static char*        last_file;

        char                buffer[sizeof(SYMBOL_INFO) + 256];
        SYMBOL_INFO*        si = (SYMBOL_INFO*)buffer;
        void*               lin = memory_to_linear_addr(&addr);
        DWORD64             disp64;
        IMAGEHLP_LINE64     il;
        DWORD               disp;

        si->SizeOfStruct = sizeof(*si);
        si->MaxNameLen   = 256;
        il.SizeOfStruct = sizeof(il);
        if (SymFromAddr(dbg_curr_process->handle, (DWORD_PTR)lin, &disp64, si) &&
            SymGetLineFromAddr64(dbg_curr_process->handle, (DWORD_PTR)lin, &disp, &il))
        {
            if ((!last_name || strcmp(last_name, si->Name)) ||
                (!last_file || strcmp(last_file, il.FileName)))
            {
                HeapFree(GetProcessHeap(), 0, last_name);
                HeapFree(GetProcessHeap(), 0, last_file);
                last_name = strcpy(HeapAlloc(GetProcessHeap(), 0, strlen(si->Name) + 1), si->Name);
                last_file = strcpy(HeapAlloc(GetProcessHeap(), 0, strlen(il.FileName) + 1), il.FileName);
                dbg_printf("%s () at %s:%u\n", last_name, last_file, il.LineNumber);
            }
        }
    }
    if (!is_debug || is_break ||
        dbg_curr_thread->exec_mode == dbg_exec_step_over_insn ||
        dbg_curr_thread->exec_mode == dbg_exec_step_into_insn)
    {
        ADDRESS64 tmp = addr;
        /* Show where we crashed */
        memory_disasm_one_insn(&tmp);
    }
    source_list_from_addr(&addr, 0);

    return TRUE;
}

static void dbg_exception_epilog(void)
{
    break_restart_execution(dbg_curr_thread->exec_count);
    /*
     * This will have gotten absorbed into the breakpoint info
     * if it was used.  Otherwise it would have been ignored.
     * In any case, we don't mess with it any more.
     */
    if (dbg_curr_thread->exec_mode == dbg_exec_cont)
	dbg_curr_thread->exec_count = 0;
    dbg_curr_thread->in_exception = FALSE;
}

static DWORD dbg_handle_exception(const EXCEPTION_RECORD* rec, BOOL first_chance)
{
    BOOL                is_debug = FALSE;
    const THREADNAME_INFO*    pThreadName;
    struct dbg_thread*  pThread;

    assert(dbg_curr_thread);

    WINE_TRACE("exception=%x first_chance=%c\n",
               rec->ExceptionCode, first_chance ? 'Y' : 'N');

    switch (rec->ExceptionCode)
    {
    case EXCEPTION_BREAKPOINT:
    case EXCEPTION_SINGLE_STEP:
        is_debug = TRUE;
        break;
    case EXCEPTION_NAME_THREAD:
        pThreadName = (const THREADNAME_INFO*)(rec->ExceptionInformation);
        if (pThreadName->dwThreadID == -1)
            pThread = dbg_curr_thread;
        else
            pThread = dbg_get_thread(dbg_curr_process, pThreadName->dwThreadID);
        if(!pThread)
        {
            dbg_printf("Thread ID=%04x not in our list of threads -> can't rename\n", pThreadName->dwThreadID);
            return DBG_CONTINUE;
        }
        if (dbg_read_memory(pThreadName->szName, pThread->name, 9))
            dbg_printf("Thread ID=%04x renamed using MS VC6 extension (name==\"%.9s\")\n",
                       pThread->tid, pThread->name);
        return DBG_CONTINUE;
    case EXCEPTION_INVALID_HANDLE:
        return DBG_CONTINUE;
    }

    if (first_chance && !is_debug && !DBG_IVAR(BreakOnFirstChance) &&
	!(rec->ExceptionFlags & EH_STACK_INVALID))
    {
        /* pass exception to program except for debug exceptions */
        return DBG_EXCEPTION_NOT_HANDLED;
    }

    dbg_curr_thread->excpt_record = *rec;
    dbg_curr_thread->in_exception = TRUE;
    dbg_curr_thread->first_chance = first_chance;

    if (!is_debug) info_win32_exception();

    if (rec->ExceptionCode == STATUS_POSSIBLE_DEADLOCK && !DBG_IVAR(BreakOnCritSectTimeOut))
    {
        dbg_curr_thread->in_exception = FALSE;
        return DBG_EXCEPTION_NOT_HANDLED;
    }

    if (dbg_exception_prolog(is_debug, rec))
    {
	dbg_interactiveP = TRUE;
        return 0;
    }
    dbg_exception_epilog();

    return DBG_CONTINUE;
}

static BOOL tgt_process_active_close_process(struct dbg_process* pcs, BOOL kill);

static void fetch_module_name(void* name_addr, BOOL unicode, void* mod_addr,
                              WCHAR* buffer, size_t bufsz, BOOL is_pcs)
{
    static const WCHAR pcspid[] = {'P','r','o','c','e','s','s','_','%','0','8','x',0};
    static const WCHAR dlladdr[] = {'D','L','L','_','%','0','8','l','x',0};

    memory_get_string_indirect(dbg_curr_process, name_addr, unicode, buffer, bufsz);
    if (!buffer[0] &&
        !GetModuleFileNameExW(dbg_curr_process->handle, mod_addr, buffer, bufsz))
    {
        if (is_pcs)
        {
            HMODULE h;
            WORD (WINAPI *gpif)(HANDLE, LPWSTR, DWORD);

            /* On Windows, when we get the process creation debug event for a process
             * created by winedbg, the modules' list is not initialized yet. Hence,
             * GetModuleFileNameExA (on the main module) will generate an error.
             * Psapi (starting on XP) provides GetProcessImageFileName() which should
             * give us the expected result
             */
            if (!(h = GetModuleHandleA("psapi")) ||
                !(gpif = (void*)GetProcAddress(h, "GetProcessImageFileNameW")) ||
                !(gpif)(dbg_curr_process->handle, buffer, bufsz))
                snprintfW(buffer, bufsz, pcspid, dbg_curr_pid);
        }
        else
            snprintfW(buffer, bufsz, dlladdr, (ULONG_PTR)mod_addr);
    }
}

static unsigned dbg_handle_debug_event(DEBUG_EVENT* de)
{
    union {
        char	bufferA[256];
        WCHAR	buffer[256];
    } u;
    DWORD       cont = DBG_CONTINUE;

    dbg_curr_pid = de->dwProcessId;
    dbg_curr_tid = de->dwThreadId;

    if ((dbg_curr_process = dbg_get_process(de->dwProcessId)) != NULL)
        dbg_curr_thread = dbg_get_thread(dbg_curr_process, de->dwThreadId);
    else
        dbg_curr_thread = NULL;

    switch (de->dwDebugEventCode)
    {
    case EXCEPTION_DEBUG_EVENT:
        if (!dbg_curr_thread)
        {
            WINE_ERR("%04x:%04x: not a registered process or thread (perhaps a 16 bit one ?)\n",
                     de->dwProcessId, de->dwThreadId);
            break;
        }

        WINE_TRACE("%04x:%04x: exception code=%08x\n",
                   de->dwProcessId, de->dwThreadId,
                   de->u.Exception.ExceptionRecord.ExceptionCode);

        if (dbg_curr_process->event_on_first_exception)
        {
            SetEvent(dbg_curr_process->event_on_first_exception);
            CloseHandle(dbg_curr_process->event_on_first_exception);
            dbg_curr_process->event_on_first_exception = NULL;
            if (!DBG_IVAR(BreakOnAttach)) break;
        }
        if (dbg_fetch_context())
        {
            cont = dbg_handle_exception(&de->u.Exception.ExceptionRecord,
                                        de->u.Exception.dwFirstChance);
            if (cont && dbg_curr_thread)
            {
                dbg_curr_process->be_cpu->set_context(dbg_curr_thread->handle, &dbg_context);
            }
        }
        break;

    case CREATE_PROCESS_DEBUG_EVENT:
        dbg_curr_process = dbg_add_process(&be_process_active_io, de->dwProcessId,
                                           de->u.CreateProcessInfo.hProcess);
        if (dbg_curr_process == NULL)
        {
            WINE_ERR("Couldn't create process\n");
            break;
        }
        fetch_module_name(de->u.CreateProcessInfo.lpImageName,
                          de->u.CreateProcessInfo.fUnicode,
                          de->u.CreateProcessInfo.lpBaseOfImage,
                          u.buffer, ARRAY_SIZE(u.buffer), TRUE);

        WINE_TRACE("%04x:%04x: create process '%s'/%p @%p (%u<%u>)\n",
                   de->dwProcessId, de->dwThreadId,
                   wine_dbgstr_w(u.buffer),
                   de->u.CreateProcessInfo.lpImageName,
                   de->u.CreateProcessInfo.lpStartAddress,
                   de->u.CreateProcessInfo.dwDebugInfoFileOffset,
                   de->u.CreateProcessInfo.nDebugInfoSize);
        dbg_set_process_name(dbg_curr_process, u.buffer);

        if (!dbg_init(dbg_curr_process->handle, u.buffer, FALSE))
            dbg_printf("Couldn't initiate DbgHelp\n");
        if (!dbg_load_module(dbg_curr_process->handle, de->u.CreateProcessInfo.hFile, u.buffer,
                             (DWORD_PTR)de->u.CreateProcessInfo.lpBaseOfImage, 0))
            dbg_printf("couldn't load main module (%u)\n", GetLastError());

        WINE_TRACE("%04x:%04x: create thread I @%p\n",
                   de->dwProcessId, de->dwThreadId, de->u.CreateProcessInfo.lpStartAddress);

        dbg_curr_thread = dbg_add_thread(dbg_curr_process,
                                         de->dwThreadId,
                                         de->u.CreateProcessInfo.hThread,
                                         de->u.CreateProcessInfo.lpThreadLocalBase);
        if (!dbg_curr_thread)
        {
            WINE_ERR("Couldn't create thread\n");
            break;
        }
        dbg_init_current_process();
        dbg_init_current_thread(de->u.CreateProcessInfo.lpStartAddress);
        break;

    case EXIT_PROCESS_DEBUG_EVENT:
        WINE_TRACE("%04x:%04x: exit process (%d)\n",
                   de->dwProcessId, de->dwThreadId, de->u.ExitProcess.dwExitCode);

        if (dbg_curr_process == NULL)
        {
            WINE_ERR("Unknown process\n");
            break;
        }
        tgt_process_active_close_process(dbg_curr_process, FALSE);
        dbg_printf("Process of pid=%04x has terminated\n", de->dwProcessId);
        break;

    case CREATE_THREAD_DEBUG_EVENT:
        WINE_TRACE("%04x:%04x: create thread D @%p\n",
                   de->dwProcessId, de->dwThreadId, de->u.CreateThread.lpStartAddress);

        if (dbg_curr_process == NULL)
        {
            WINE_ERR("Unknown process\n");
            break;
        }
        if (dbg_get_thread(dbg_curr_process, de->dwThreadId) != NULL)
        {
            WINE_TRACE("Thread already listed, skipping\n");
            break;
        }

        dbg_curr_thread = dbg_add_thread(dbg_curr_process,
                                         de->dwThreadId,
                                         de->u.CreateThread.hThread,
                                         de->u.CreateThread.lpThreadLocalBase);
        if (!dbg_curr_thread)
        {
            WINE_ERR("Couldn't create thread\n");
            break;
        }
        dbg_init_current_thread(de->u.CreateThread.lpStartAddress);
        break;

    case EXIT_THREAD_DEBUG_EVENT:
        WINE_TRACE("%04x:%04x: exit thread (%d)\n",
                   de->dwProcessId, de->dwThreadId, de->u.ExitThread.dwExitCode);

        if (dbg_curr_thread == NULL)
        {
            WINE_ERR("Unknown thread\n");
            break;
        }
        /* FIXME: remove break point set on thread startup */
        dbg_del_thread(dbg_curr_thread);
        break;

    case LOAD_DLL_DEBUG_EVENT:
        if (dbg_curr_thread == NULL)
        {
            WINE_ERR("Unknown thread\n");
            break;
        }
        fetch_module_name(de->u.LoadDll.lpImageName,
                          de->u.LoadDll.fUnicode,
                          de->u.LoadDll.lpBaseOfDll,
                          u.buffer, ARRAY_SIZE(u.buffer), FALSE);

        WINE_TRACE("%04x:%04x: loads DLL %s @%p (%u<%u>)\n",
                   de->dwProcessId, de->dwThreadId,
                   wine_dbgstr_w(u.buffer), de->u.LoadDll.lpBaseOfDll,
                   de->u.LoadDll.dwDebugInfoFileOffset,
                   de->u.LoadDll.nDebugInfoSize);
        dbg_load_module(dbg_curr_process->handle, de->u.LoadDll.hFile, u.buffer,
                        (DWORD_PTR)de->u.LoadDll.lpBaseOfDll, 0);
        break_set_xpoints(FALSE);
        break_check_delayed_bp();
        break_set_xpoints(TRUE);
        if (DBG_IVAR(BreakOnDllLoad))
        {
            dbg_printf("Stopping on DLL %s loading at %p\n",
                       dbg_W2A(u.buffer, -1), de->u.LoadDll.lpBaseOfDll);
            if (dbg_fetch_context()) cont = 0;
        }
        break;

    case UNLOAD_DLL_DEBUG_EVENT:
        WINE_TRACE("%04x:%04x: unload DLL @%p\n",
                   de->dwProcessId, de->dwThreadId,
                   de->u.UnloadDll.lpBaseOfDll);
        break_delete_xpoints_from_module((DWORD_PTR)de->u.UnloadDll.lpBaseOfDll);
        SymUnloadModule64(dbg_curr_process->handle, (DWORD_PTR)de->u.UnloadDll.lpBaseOfDll);
        break;

    case OUTPUT_DEBUG_STRING_EVENT:
        if (dbg_curr_thread == NULL)
        {
            WINE_ERR("Unknown thread\n");
            break;
        }

        memory_get_string(dbg_curr_process,
                          de->u.DebugString.lpDebugStringData, TRUE,
                          de->u.DebugString.fUnicode, u.bufferA, sizeof(u.bufferA));
        WINE_TRACE("%04x:%04x: output debug string (%s)\n",
                   de->dwProcessId, de->dwThreadId, u.bufferA);
        break;

    case RIP_EVENT:
        WINE_TRACE("%04x:%04x: rip error=%u type=%u\n",
                   de->dwProcessId, de->dwThreadId, de->u.RipInfo.dwError,
                   de->u.RipInfo.dwType);
        break;

    default:
        WINE_TRACE("%04x:%04x: unknown event (%x)\n",
                   de->dwProcessId, de->dwThreadId, de->dwDebugEventCode);
    }
    if (!cont) return TRUE;  /* stop execution */
    ContinueDebugEvent(de->dwProcessId, de->dwThreadId, cont);
    return FALSE;  /* continue execution */
}

static void dbg_resume_debuggee(DWORD cont)
{
    if (dbg_curr_thread->in_exception)
    {
        ADDRESS64       addr;
        char            hexbuf[MAX_OFFSET_TO_STR_LEN];

        dbg_exception_epilog();
        memory_get_current_pc(&addr);
        WINE_TRACE("Exiting debugger      PC=%s mode=%d count=%d\n",
                   memory_offset_to_string(hexbuf, addr.Offset, 0),
                   dbg_curr_thread->exec_mode,
                   dbg_curr_thread->exec_count);
        if (dbg_curr_thread)
        {
            if (!dbg_curr_process->be_cpu->set_context(dbg_curr_thread->handle, &dbg_context))
                dbg_printf("Cannot set ctx on %04lx\n", dbg_curr_tid);
        }
    }
    dbg_interactiveP = FALSE;
    if (!ContinueDebugEvent(dbg_curr_pid, dbg_curr_tid, cont))
        dbg_printf("Cannot continue on %04lx (%08x)\n", dbg_curr_tid, cont);
}

static void wait_exception(void)
{
    DEBUG_EVENT		de;

    while (dbg_num_processes() && WaitForDebugEvent(&de, INFINITE))
    {
        if (dbg_handle_debug_event(&de)) break;
    }
    dbg_interactiveP = TRUE;
}

void dbg_wait_next_exception(DWORD cont, int count, int mode)
{
    ADDRESS64           addr;
    char                hexbuf[MAX_OFFSET_TO_STR_LEN];

    if (cont == DBG_CONTINUE)
    {
        dbg_curr_thread->exec_count = count;
        dbg_curr_thread->exec_mode = mode;
    }
    dbg_resume_debuggee(cont);

    wait_exception();
    if (!dbg_curr_process) return;

    memory_get_current_pc(&addr);
    WINE_TRACE("Entering debugger     PC=%s mode=%d count=%d\n",
               memory_offset_to_string(hexbuf, addr.Offset, 0),
               dbg_curr_thread->exec_mode,
               dbg_curr_thread->exec_count);
}

void     dbg_active_wait_for_first_exception(void)
{
    dbg_interactiveP = FALSE;
    /* wait for first exception */
    wait_exception();
}

static BOOL dbg_start_debuggee(LPSTR cmdLine)
{
    PROCESS_INFORMATION	info;
    STARTUPINFOA	startup, current;
    DWORD               flags;

    GetStartupInfoA(&current);

    memset(&startup, 0, sizeof(startup));
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESHOWWINDOW;

    startup.wShowWindow = (current.dwFlags & STARTF_USESHOWWINDOW) ?
        current.wShowWindow : SW_SHOWNORMAL;

    /* FIXME: shouldn't need the CREATE_NEW_CONSOLE, but as usual CUIs need it
     * while GUIs don't
     */
    flags = DEBUG_PROCESS | CREATE_NEW_CONSOLE;
    if (!DBG_IVAR(AlsoDebugProcChild)) flags |= DEBUG_ONLY_THIS_PROCESS;

    if (!CreateProcessA(NULL, cmdLine, NULL, NULL, FALSE, flags,
                        NULL, NULL, &startup, &info))
    {
	dbg_printf("Couldn't start process '%s'\n", cmdLine);
	return FALSE;
    }
    if (!info.dwProcessId)
    {
        /* this happens when the program being run is not a Wine binary
         * (for example, a shell wrapper around a WineLib app)
         */
        /* Current fix: list running processes and let the user attach
         * to one of them (sic)
         * FIXME: implement a real fix => grab the process (from the
         * running processes) from its name
         */
        dbg_printf("Debuggee has been started (%s)\n"
                   "But WineDbg isn't attached to it. Maybe you're trying to debug a winelib wrapper ??\n"
                   "Try to attach to one of those processes:\n", cmdLine);
        /* FIXME: (HACK) we need some time before the wrapper executes the winelib app */
        Sleep(100);
        info_win32_processes();
        return TRUE;
    }
    dbg_curr_pid = info.dwProcessId;
    if (!(dbg_curr_process = dbg_add_process(&be_process_active_io, dbg_curr_pid, 0))) return FALSE;
    dbg_curr_process->active_debuggee = TRUE;

    return TRUE;
}

void	dbg_run_debuggee(const char* args)
{
    if (args)
    {
        WINE_FIXME("Re-running current program with %s as args is broken\n", wine_dbgstr_a(args));
        return;
    }
    else 
    {
	if (!dbg_last_cmd_line)
        {
	    dbg_printf("Cannot find previously used command line.\n");
	    return;
	}
	dbg_start_debuggee(dbg_last_cmd_line);
        dbg_active_wait_for_first_exception();
        source_list_from_addr(NULL, 0);
    }
}

static BOOL str2int(const char* str, DWORD_PTR* val)
{
    char*   ptr;

    *val = strtol(str, &ptr, 10);
    return str < ptr && !*ptr;
}

static HANDLE create_temp_file(void)
{
    static const WCHAR prefixW[] = {'w','d','b',0};
    WCHAR path[MAX_PATH], name[MAX_PATH];

    if (!GetTempPathW( MAX_PATH, path ) || !GetTempFileNameW( path, prefixW, 0, name ))
        return INVALID_HANDLE_VALUE;
    return CreateFileW( name, GENERIC_READ|GENERIC_WRITE|DELETE, FILE_SHARE_DELETE,
                        NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, 0 );
}

static const struct
{
    int type;
    int platform;
    int major;
    int minor;
    const char *str;
}
version_table[] =
{
    { 0,                   VER_PLATFORM_WIN32s,        2,  0, "2.0" },
    { 0,                   VER_PLATFORM_WIN32s,        3,  0, "3.0" },
    { 0,                   VER_PLATFORM_WIN32s,        3, 10, "3.1" },
    { 0,                   VER_PLATFORM_WIN32_WINDOWS, 4,  0, "95" },
    { 0,                   VER_PLATFORM_WIN32_WINDOWS, 4, 10, "98" },
    { 0,                   VER_PLATFORM_WIN32_WINDOWS, 4, 90, "ME" },
    { VER_NT_WORKSTATION,  VER_PLATFORM_WIN32_NT,      3, 51, "NT 3.51" },
    { VER_NT_WORKSTATION,  VER_PLATFORM_WIN32_NT,      4,  0, "NT 4.0" },
    { VER_NT_WORKSTATION,  VER_PLATFORM_WIN32_NT,      5,  0, "2000" },
    { VER_NT_WORKSTATION,  VER_PLATFORM_WIN32_NT,      5,  1, "XP" },
    { VER_NT_WORKSTATION,  VER_PLATFORM_WIN32_NT,      5,  2, "XP" },
    { VER_NT_SERVER,       VER_PLATFORM_WIN32_NT,      5,  2, "Server 2003" },
    { VER_NT_WORKSTATION,  VER_PLATFORM_WIN32_NT,      6,  0, "Vista" },
    { VER_NT_SERVER,       VER_PLATFORM_WIN32_NT,      6,  0, "Server 2008" },
    { VER_NT_WORKSTATION,  VER_PLATFORM_WIN32_NT,      6,  1, "7" },
    { VER_NT_SERVER,       VER_PLATFORM_WIN32_NT,      6,  1, "Server 2008 R2" },
    { VER_NT_WORKSTATION,  VER_PLATFORM_WIN32_NT,      6,  2, "8" },
    { VER_NT_SERVER,       VER_PLATFORM_WIN32_NT,      6,  2, "Server 2012" },
    { VER_NT_WORKSTATION,  VER_PLATFORM_WIN32_NT,      6,  3, "8.1" },
    { VER_NT_SERVER,       VER_PLATFORM_WIN32_NT,      6,  3, "Server 2012 R2" },
    { VER_NT_WORKSTATION,  VER_PLATFORM_WIN32_NT,     10,  0, "10" },
};

static const char *get_windows_version(void)
{
    RTL_OSVERSIONINFOEXW info = { sizeof(RTL_OSVERSIONINFOEXW) };
    static char str[64];
    int i;

    RtlGetVersion( &info );

    for (i = 0; i < ARRAY_SIZE(version_table); i++)
    {
        if (version_table[i].type == info.wProductType &&
            version_table[i].platform == info.dwPlatformId &&
            version_table[i].major == info.dwMajorVersion &&
            version_table[i].minor == info.dwMinorVersion)
        {
            return version_table[i].str;
        }
    }

    snprintf( str, sizeof(str), "%d.%d (%d)", info.dwMajorVersion,
              info.dwMinorVersion, info.wProductType );
    return str;
}

static void output_system_info(void)
{
#ifdef __i386__
    static const char platform[] = "i386";
#elif defined(__x86_64__)
    static const char platform[] = "x86_64";
#elif defined(__arm__)
    static const char platform[] = "arm";
#elif defined(__aarch64__)
    static const char platform[] = "arm64";
#else
# error CPU unknown
#endif

    const char *(CDECL *wine_get_build_id)(void);
    void (CDECL *wine_get_host_version)( const char **sysname, const char **release );
    BOOL is_wow64;

    wine_get_build_id = (void *)GetProcAddress(GetModuleHandleA("ntdll.dll"), "wine_get_build_id");
    wine_get_host_version = (void *)GetProcAddress(GetModuleHandleA("ntdll.dll"), "wine_get_host_version");
    if (!IsWow64Process( dbg_curr_process->handle, &is_wow64 )) is_wow64 = FALSE;

    dbg_printf( "System information:\n" );
    if (wine_get_build_id) dbg_printf( "    Wine build: %s\n", wine_get_build_id() );
    dbg_printf( "    Platform: %s%s\n", platform, is_wow64 ? " (WOW64)" : "" );
    dbg_printf( "    Version: Windows %s\n", get_windows_version() );
    if (wine_get_host_version)
    {
        const char *sysname, *release;
        wine_get_host_version( &sysname, &release );
        dbg_printf( "    Host system: %s\n", sysname );
        dbg_printf( "    Host version: %s\n", release );
    }
}

/******************************************************************
 *		dbg_active_attach
 *
 * Tries to attach to a running process
 * Handles the <pid> or <pid> <evt> forms
 */
enum dbg_start  dbg_active_attach(int argc, char* argv[])
{
    DWORD_PTR pid, evt;

    /* try the form <myself> pid */
    if (argc == 1 && str2int(argv[0], &pid) && pid != 0)
    {
        if (!dbg_attach_debuggee(pid))
            return start_error_init;
    }
    /* try the form <myself> pid evt (Win32 JIT debugger) */
    else if (argc == 2 && str2int(argv[0], &pid) && pid != 0 &&
             str2int(argv[1], &evt) && evt != 0)
    {
        if (!dbg_attach_debuggee(pid))
        {
            /* don't care about result */
            SetEvent((HANDLE)evt);
            return start_error_init;
        }
        dbg_curr_process->event_on_first_exception = (HANDLE)evt;
    }
    else return start_error_parse;

    dbg_curr_pid = pid;
    return start_ok;
}

/******************************************************************
 *		dbg_active_launch
 *
 * Launches a debuggee (with its arguments) from argc/argv
 */
enum dbg_start    dbg_active_launch(int argc, char* argv[])
{
    int         i, len;
    LPSTR	cmd_line;

    if (argc == 0) return start_error_parse;

    if (!(cmd_line = HeapAlloc(GetProcessHeap(), 0, len = 1)))
    {
    oom_leave:
        dbg_printf("Out of memory\n");
        return start_error_init;
    }
    cmd_line[0] = '\0';

    for (i = 0; i < argc; i++)
    {
        len += strlen(argv[i]) + 1;
        if (!(cmd_line = HeapReAlloc(GetProcessHeap(), 0, cmd_line, len)))
            goto oom_leave;
        strcat(cmd_line, argv[i]);
        cmd_line[len - 2] = ' ';
        cmd_line[len - 1] = '\0';
    }

    if (!dbg_start_debuggee(cmd_line))
    {
        HeapFree(GetProcessHeap(), 0, cmd_line);
        return start_error_init;
    }
    HeapFree(GetProcessHeap(), 0, dbg_last_cmd_line);
    dbg_last_cmd_line = cmd_line;
    return start_ok;
}

/******************************************************************
 *		dbg_active_auto
 *
 * Starts (<pid> or <pid> <evt>) in automatic mode
 */
enum dbg_start dbg_active_auto(int argc, char* argv[])
{
    HANDLE thread = 0, event = 0, input, output = INVALID_HANDLE_VALUE;
    enum dbg_start      ds = start_error_parse;

    DBG_IVAR(BreakOnDllLoad) = 0;

    /* auto mode */
    argc--; argv++;
    ds = dbg_active_attach(argc, argv);
    if (ds != start_ok) {
        msgbox_res_id(NULL, IDS_INVALID_PARAMS, IDS_AUTO_CAPTION, MB_OK);
        return ds;
    }

    switch (display_crash_dialog())
    {
    case ID_DEBUG:
        AllocConsole();
        dbg_init_console();
        dbg_start_interactive(INVALID_HANDLE_VALUE);
        return start_ok;
    case ID_DETAILS:
        event = CreateEventW( NULL, TRUE, FALSE, NULL );
        if (event) thread = display_crash_details( event );
        if (thread) dbg_houtput = output = create_temp_file();
        break;
    }

    input = parser_generate_command_file("echo Modules:", "info share",
                                         "echo Threads:", "info threads", NULL);
    if (input == INVALID_HANDLE_VALUE) return start_error_parse;

    if (dbg_curr_process->active_debuggee)
        dbg_active_wait_for_first_exception();

    dbg_interactiveP = TRUE;
    parser_handle(input);
    output_system_info();

    if (output != INVALID_HANDLE_VALUE)
    {
        SetEvent( event );
        WaitForSingleObject( thread, INFINITE );
        CloseHandle( output );
        CloseHandle( thread );
        CloseHandle( event );
    }

    CloseHandle( input );
    dbg_curr_process->process_io->close_process(dbg_curr_process, TRUE);
    return start_ok;
}

/******************************************************************
 *		dbg_active_minidump
 *
 * Starts (<pid> or <pid> <evt>) in minidump mode
 */
enum dbg_start dbg_active_minidump(int argc, char* argv[])
{
    HANDLE              hFile;
    enum dbg_start      ds = start_error_parse;
    const char*     file = NULL;
    char            tmp[8 + 1 + 2 + MAX_PATH]; /* minidump "<file>" */

    dbg_houtput = GetStdHandle(STD_ERROR_HANDLE);
    DBG_IVAR(BreakOnDllLoad) = 0;

    argc--; argv++;
    /* hard stuff now ; we can get things like:
     * --minidump <pid>                     1 arg
     * --minidump <pid> <evt>               2 args
     * --minidump <file> <pid>              2 args
     * --minidump <file> <pid> <evt>        3 args
     */
    switch (argc)
    {
    case 1:
        ds = dbg_active_attach(argc, argv);
        break;
    case 2:
        if ((ds = dbg_active_attach(argc, argv)) != start_ok)
        {
            file = argv[0];
            ds = dbg_active_attach(argc - 1, argv + 1);
        }
        break;
    case 3:
        file = argv[0];
        ds = dbg_active_attach(argc - 1, argv + 1);
        break;
    default:
        return start_error_parse;
    }
    if (ds != start_ok) return ds;
    memcpy(tmp, "minidump \"", 10);
    if (!file)
    {
        char        path[MAX_PATH];

        GetTempPathA(sizeof(path), path);
        GetTempFileNameA(path, "WD", 0, tmp + 10);
    }
    else strcpy(tmp + 10, file);
    strcat(tmp, "\"");
    if (!file)
    {
        /* FIXME: should generate unix name as well */
        dbg_printf("Capturing program state in %s\n", tmp + 9);
    }
    hFile = parser_generate_command_file(tmp, "detach", NULL);
    if (hFile == INVALID_HANDLE_VALUE) return start_error_parse;

    if (dbg_curr_process->active_debuggee)
        dbg_active_wait_for_first_exception();

    dbg_interactiveP = TRUE;
    parser_handle(hFile);

    return start_ok;
}

static BOOL tgt_process_active_close_process(struct dbg_process* pcs, BOOL kill)
{
    if (kill)
    {
        DWORD exit_code = 0;

        if (pcs == dbg_curr_process && dbg_curr_thread->in_exception)
            exit_code = dbg_curr_thread->excpt_record.ExceptionCode;

        TerminateProcess(pcs->handle, exit_code);
    }
    else if (pcs == dbg_curr_process)
    {
        /* remove all set breakpoints in debuggee code */
        break_set_xpoints(FALSE);
        /* needed for single stepping (ugly).
         * should this be handled inside the server ??? 
         */
        dbg_curr_process->be_cpu->single_step(&dbg_context, FALSE);
        if (dbg_curr_thread->in_exception)
        {
            dbg_curr_process->be_cpu->set_context(dbg_curr_thread->handle, &dbg_context);
            ContinueDebugEvent(dbg_curr_pid, dbg_curr_tid, DBG_CONTINUE);
        }
    }

    if (!kill)
    {
        if (!DebugActiveProcessStop(pcs->pid)) return FALSE;
    }
    SymCleanup(pcs->handle);
    dbg_del_process(pcs);

    return TRUE;
}

static BOOL tgt_process_active_read(HANDLE hProcess, const void* addr,
                                    void* buffer, SIZE_T len, SIZE_T* rlen)
{
    return ReadProcessMemory( hProcess, addr, buffer, len, rlen );
}

static BOOL tgt_process_active_write(HANDLE hProcess, void* addr,
                                     const void* buffer, SIZE_T len, SIZE_T* wlen)
{
    return WriteProcessMemory( hProcess, addr, buffer, len, wlen );
}

static BOOL tgt_process_active_get_selector(HANDLE hThread, DWORD sel, LDT_ENTRY* le)
{
    return GetThreadSelectorEntry( hThread, sel, le );
}

static struct be_process_io be_process_active_io =
{
    tgt_process_active_close_process,
    tgt_process_active_read,
    tgt_process_active_write,
    tgt_process_active_get_selector
};
