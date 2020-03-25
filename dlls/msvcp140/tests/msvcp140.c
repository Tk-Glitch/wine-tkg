/*
 * Copyright 2016 Daniel Lehman
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

#include <errno.h>
#include <stdio.h>

#include "windef.h"
#include "winbase.h"
#include "winnls.h"

#include "wine/test.h"
#include "winbase.h"

#define SECSPERDAY        86400
/* 1601 to 1970 is 369 years plus 89 leap days */
#define SECS_1601_TO_1970  ((369 * 365 + 89) * (ULONGLONG)SECSPERDAY)
#define TICKSPERSEC       10000000
#define TICKS_1601_TO_1970 (SECS_1601_TO_1970 * TICKSPERSEC)

#define DEFINE_EXPECT(func) \
    static BOOL expect_ ## func = FALSE, called_ ## func = FALSE

#define SET_EXPECT(func) \
    do { \
        expect_ ## func = TRUE; \
        errno = 0xdeadbeef; \
    }while(0)

#define CHECK_EXPECT2(func) \
    do { \
        ok(expect_ ##func, "unexpected call " #func "\n"); \
        called_ ## func = TRUE; \
    }while(0)

#define CHECK_EXPECT(func) \
    do { \
        CHECK_EXPECT2(func); \
        expect_ ## func = FALSE; \
    }while(0)

#define CHECK_CALLED(func) \
    do { \
        ok(called_ ## func, "expected " #func "\n"); \
        expect_ ## func = called_ ## func = FALSE; \
    }while(0)

#ifdef _WIN64
DEFINE_EXPECT(function_do_call);
DEFINE_EXPECT(function_do_clean);
#endif

#undef __thiscall
#ifdef __i386__
#define __thiscall __stdcall
#else
#define __thiscall __cdecl
#endif

/* Emulate a __thiscall */
#ifdef __i386__

#include "pshpack1.h"
struct thiscall_thunk
{
    BYTE pop_eax;    /* popl  %eax (ret addr) */
    BYTE pop_edx;    /* popl  %edx (func) */
    BYTE pop_ecx;    /* popl  %ecx (this) */
    BYTE push_eax;   /* pushl %eax */
    WORD jmp_edx;    /* jmp  *%edx */
};
#include "poppack.h"

static void * (WINAPI *call_thiscall_func1)( void *func, void *this );
static void * (WINAPI *call_thiscall_func2)( void *func, void *this, const void *a );

static void init_thiscall_thunk(void)
{
    struct thiscall_thunk *thunk = VirtualAlloc( NULL, sizeof(*thunk),
            MEM_COMMIT, PAGE_EXECUTE_READWRITE );
    thunk->pop_eax  = 0x58;   /* popl  %eax */
    thunk->pop_edx  = 0x5a;   /* popl  %edx */
    thunk->pop_ecx  = 0x59;   /* popl  %ecx */
    thunk->push_eax = 0x50;   /* pushl %eax */
    thunk->jmp_edx  = 0xe2ff; /* jmp  *%edx */
    call_thiscall_func1 = (void *)thunk;
    call_thiscall_func2 = (void *)thunk;
}

#define call_func1(func,_this) call_thiscall_func1(func,_this)
#define call_func2(func,_this,a) call_thiscall_func2(func,_this,a)

#else

#define init_thiscall_thunk()
#define call_func1(func,_this) func(_this)
#define call_func2(func,_this,a) func(_this,a)

#endif /* __i386__ */
typedef unsigned char MSVCP_bool;

typedef struct {
    void *unk0;
    BYTE unk1;
} task_continuation_context;

typedef struct {
    void *unused;
} _ContextCallback;

typedef struct {
    const void *vtable;
    void (__cdecl *func)(void);
    int unk[4];
    void *unk2[3];
    void *this;
} function_void_cdecl_void;

typedef struct {
    void *task;
    MSVCP_bool scheduled;
    MSVCP_bool started;
} _TaskEventLogger;

typedef struct {
    PTP_WORK work;
    void (__cdecl *callback)(void*);
    void *arg;
} _Threadpool_chore;

enum file_type {
    file_not_found = -1,
    none_file,
    regular_file,
    directory_file,
    symlink_file,
    block_file,
    character_file,
    fifo_file,
    socket_file,
    status_unknown
};

static unsigned int (__cdecl *p__Thrd_id)(void);
static MSVCP_bool (__cdecl *p__Task_impl_base__IsNonBlockingThread)(void);
static task_continuation_context* (__thiscall *p_task_continuation_context_ctor)(task_continuation_context*);
static void (__thiscall *p__ContextCallback__Assign)(_ContextCallback*, void*);
static void (__thiscall *p__ContextCallback__CallInContext)(const _ContextCallback*, function_void_cdecl_void, MSVCP_bool);
static void (__thiscall *p__ContextCallback__Capture)(_ContextCallback*);
static void (__thiscall *p__ContextCallback__Reset)(_ContextCallback*);
static MSVCP_bool (__cdecl *p__ContextCallback__IsCurrentOriginSTA)(_ContextCallback*);
static void (__thiscall *p__TaskEventLogger__LogCancelTask)(_TaskEventLogger*);
static void (__thiscall *p__TaskEventLogger__LogScheduleTask)(_TaskEventLogger*, MSVCP_bool);
static void (__thiscall *p__TaskEventLogger__LogTaskCompleted)(_TaskEventLogger*);
static void (__thiscall *p__TaskEventLogger__LogTaskExecutionCompleted)(_TaskEventLogger*);
static void (__thiscall *p__TaskEventLogger__LogWorkItemCompleted)(_TaskEventLogger*);
static void (__thiscall *p__TaskEventLogger__LogWorkItemStarted)(_TaskEventLogger*);
static int (__cdecl *p__Schedule_chore)(_Threadpool_chore*);
static int (__cdecl *p__Reschedule_chore)(const _Threadpool_chore*);
static void (__cdecl *p__Release_chore)(_Threadpool_chore*);

static void (__cdecl *p_Close_dir)(void*);
static MSVCP_bool (__cdecl *p_Current_get)(WCHAR *);
static MSVCP_bool (__cdecl *p_Current_set)(WCHAR const *);
static int (__cdecl *p_Equivalent)(WCHAR const*, WCHAR const*);
static ULONGLONG (__cdecl *p_File_size)(WCHAR const *);
static int (__cdecl *p_Resize)(const WCHAR *, UINT64);
static __int64 (__cdecl *p_Last_write_time)(WCHAR const*);
static void (__cdecl *p_Set_last_write_time)(WCHAR const*, __int64);
static int (__cdecl *p_Link)(WCHAR const*, WCHAR const*);
static enum file_type (__cdecl *p_Lstat)(WCHAR const *, int *);
static int (__cdecl *p_Make_dir)(WCHAR const*);
static void* (__cdecl *p_Open_dir)(WCHAR*, WCHAR const*, int *, enum file_type*);
static WCHAR* (__cdecl *p_Read_dir)(WCHAR*, void*, enum file_type*);
static MSVCP_bool (__cdecl *p_Remove_dir)(WCHAR const*);
static int (__cdecl *p_Rename)(WCHAR const*, WCHAR const*);
static enum file_type (__cdecl *p_Stat)(WCHAR const *, int *);
static int (__cdecl *p_Symlink)(WCHAR const*, WCHAR const*);
static WCHAR* (__cdecl *p_Temp_get)(WCHAR *);
static int (__cdecl *p_To_byte)(const WCHAR *src, char *dst);
static int (__cdecl *p_To_wide)(const char *src, WCHAR *dst);
static int (__cdecl *p_Unlink)(WCHAR const*);
static ULONG (__cdecl *p__Winerror_message)(ULONG, char*, ULONG);
static int (__cdecl *p__Winerror_map)(int);

static BOOLEAN (WINAPI *pCreateSymbolicLinkW)(const WCHAR *, const WCHAR *, DWORD);

static HMODULE msvcp;
#define SETNOFAIL(x,y) x = (void*)GetProcAddress(msvcp,y)
#define SET(x,y) do { SETNOFAIL(x,y); ok(x != NULL, "Export '%s' not found\n", y); } while(0)
static BOOL init(void)
{
    HANDLE hdll;

    msvcp = LoadLibraryA("msvcp140.dll");
    if(!msvcp)
    {
        win_skip("msvcp140.dll not installed\n");
        return FALSE;
    }

    SET(p__Thrd_id, "_Thrd_id");
    SET(p__Task_impl_base__IsNonBlockingThread, "?_IsNonBlockingThread@_Task_impl_base@details@Concurrency@@SA_NXZ");
    SET(p__ContextCallback__IsCurrentOriginSTA, "?_IsCurrentOriginSTA@_ContextCallback@details@Concurrency@@CA_NXZ");
    SET(p__Winerror_map, "?_Winerror_map@std@@YAHH@Z");

    if(sizeof(void*) == 8) { /* 64-bit initialization */
        SET(p_task_continuation_context_ctor, "??0task_continuation_context@Concurrency@@AEAA@XZ");
        SET(p__ContextCallback__Assign, "?_Assign@_ContextCallback@details@Concurrency@@AEAAXPEAX@Z");
        SET(p__ContextCallback__CallInContext, "?_CallInContext@_ContextCallback@details@Concurrency@@QEBAXV?$function@$$A6AXXZ@std@@_N@Z");
        SET(p__ContextCallback__Capture, "?_Capture@_ContextCallback@details@Concurrency@@AEAAXXZ");
        SET(p__ContextCallback__Reset, "?_Reset@_ContextCallback@details@Concurrency@@AEAAXXZ");
        SET(p__TaskEventLogger__LogCancelTask, "?_LogCancelTask@_TaskEventLogger@details@Concurrency@@QEAAXXZ");
        SET(p__TaskEventLogger__LogScheduleTask, "?_LogScheduleTask@_TaskEventLogger@details@Concurrency@@QEAAX_N@Z");
        SET(p__TaskEventLogger__LogTaskCompleted, "?_LogTaskCompleted@_TaskEventLogger@details@Concurrency@@QEAAXXZ");
        SET(p__TaskEventLogger__LogTaskExecutionCompleted, "?_LogTaskExecutionCompleted@_TaskEventLogger@details@Concurrency@@QEAAXXZ");
        SET(p__TaskEventLogger__LogWorkItemCompleted, "?_LogWorkItemCompleted@_TaskEventLogger@details@Concurrency@@QEAAXXZ");
        SET(p__TaskEventLogger__LogWorkItemStarted, "?_LogWorkItemStarted@_TaskEventLogger@details@Concurrency@@QEAAXXZ");
        SET(p__Schedule_chore, "?_Schedule_chore@details@Concurrency@@YAHPEAU_Threadpool_chore@12@@Z");
        SET(p__Reschedule_chore, "?_Reschedule_chore@details@Concurrency@@YAHPEBU_Threadpool_chore@12@@Z");
        SET(p__Release_chore, "?_Release_chore@details@Concurrency@@YAXPEAU_Threadpool_chore@12@@Z");
        SET(p__Winerror_message, "?_Winerror_message@std@@YAKKPEADK@Z");
    } else {
#ifdef __arm__
        SET(p_task_continuation_context_ctor, "??0task_continuation_context@Concurrency@@AAA@XZ");
        SET(p__ContextCallback__Assign, "?_Assign@_ContextCallback@details@Concurrency@@AAAXPAX@Z");
        SET(p__ContextCallback__CallInContext, "?_CallInContext@_ContextCallback@details@Concurrency@@QBAXV?$function@$$A6AXXZ@std@@_N@Z");
        SET(p__ContextCallback__Capture, "?_Capture@_ContextCallback@details@Concurrency@@AAAXXZ");
        SET(p__ContextCallback__Reset, "?_Reset@_ContextCallback@details@Concurrency@@AAAXXZ");
        SET(p__TaskEventLogger__LogCancelTask, "?_LogCancelTask@_TaskEventLogger@details@Concurrency@@QAAXXZ");
        SET(p__TaskEventLogger__LogScheduleTask, "?_LogScheduleTask@_TaskEventLogger@details@Concurrency@@QAEX_N@Z");
        SET(p__TaskEventLogger__LogTaskCompleted, "?_LogTaskCompleted@_TaskEventLogger@details@Concurrency@@QAAXXZ");
        SET(p__TaskEventLogger__LogTaskExecutionCompleted, "?_LogTaskExecutionCompleted@_TaskEventLogger@details@Concurrency@@QAAXXZ");
        SET(p__TaskEventLogger__LogWorkItemCompleted, "?_LogWorkItemCompleted@_TaskEventLogger@details@Concurrency@@QAAXXZ");
        SET(p__TaskEventLogger__LogWorkItemStarted, "?_LogWorkItemStarted@_TaskEventLogger@details@Concurrency@@QAAXXZ");
#else
        SET(p_task_continuation_context_ctor, "??0task_continuation_context@Concurrency@@AAE@XZ");
        SET(p__ContextCallback__Assign, "?_Assign@_ContextCallback@details@Concurrency@@AAEXPAX@Z");
        SET(p__ContextCallback__CallInContext, "?_CallInContext@_ContextCallback@details@Concurrency@@QBEXV?$function@$$A6AXXZ@std@@_N@Z");
        SET(p__ContextCallback__Capture, "?_Capture@_ContextCallback@details@Concurrency@@AAEXXZ");
        SET(p__ContextCallback__Reset, "?_Reset@_ContextCallback@details@Concurrency@@AAEXXZ");
        SET(p__TaskEventLogger__LogCancelTask, "?_LogCancelTask@_TaskEventLogger@details@Concurrency@@QAEXXZ");
        SET(p__TaskEventLogger__LogScheduleTask, "?_LogScheduleTask@_TaskEventLogger@details@Concurrency@@QAEX_N@Z");
        SET(p__TaskEventLogger__LogTaskCompleted, "?_LogTaskCompleted@_TaskEventLogger@details@Concurrency@@QAEXXZ");
        SET(p__TaskEventLogger__LogTaskExecutionCompleted, "?_LogTaskExecutionCompleted@_TaskEventLogger@details@Concurrency@@QAEXXZ");
        SET(p__TaskEventLogger__LogWorkItemCompleted, "?_LogWorkItemCompleted@_TaskEventLogger@details@Concurrency@@QAEXXZ");
        SET(p__TaskEventLogger__LogWorkItemStarted, "?_LogWorkItemStarted@_TaskEventLogger@details@Concurrency@@QAEXXZ");
#endif
        SET(p__Schedule_chore, "?_Schedule_chore@details@Concurrency@@YAHPAU_Threadpool_chore@12@@Z");
        SET(p__Reschedule_chore, "?_Reschedule_chore@details@Concurrency@@YAHPBU_Threadpool_chore@12@@Z");
        SET(p__Release_chore, "?_Release_chore@details@Concurrency@@YAXPAU_Threadpool_chore@12@@Z");
        SET(p__Winerror_message, "?_Winerror_message@std@@YAKKPADK@Z");
    }

    SET(p_Close_dir, "_Close_dir");
    SET(p_Current_get, "_Current_get");
    SET(p_Current_set, "_Current_set");
    SET(p_Equivalent, "_Equivalent");
    SET(p_File_size, "_File_size");
    SET(p_Resize, "_Resize");
    SET(p_Last_write_time, "_Last_write_time");
    SET(p_Set_last_write_time, "_Set_last_write_time");
    SET(p_Link, "_Link");
    SET(p_Lstat, "_Lstat");
    SET(p_Make_dir, "_Make_dir");
    SET(p_Open_dir, "_Open_dir");
    SET(p_Read_dir, "_Read_dir");
    SET(p_Remove_dir, "_Remove_dir");
    SET(p_Rename, "_Rename");
    SET(p_Stat, "_Stat");
    SET(p_Symlink, "_Symlink");
    SET(p_Temp_get, "_Temp_get");
    SET(p_To_byte, "_To_byte");
    SET(p_To_wide, "_To_wide");
    SET(p_Unlink, "_Unlink");

    hdll = GetModuleHandleA("kernel32.dll");
    pCreateSymbolicLinkW = (void*)GetProcAddress(hdll, "CreateSymbolicLinkW");

    init_thiscall_thunk();
    return TRUE;
}

static void test_thrd(void)
{
    ok(p__Thrd_id() == GetCurrentThreadId(),
        "expected same id, got _Thrd_id %u GetCurrentThreadId %u\n",
        p__Thrd_id(), GetCurrentThreadId());
}

static void test__Task_impl_base__IsNonBlockingThread(void)
{
    ok(!p__Task_impl_base__IsNonBlockingThread(), "_IsNonBlockingThread() returned true\n");
}

static struct {
    int value[2];
    const char* export_name;
} vbtable_size_exports_list[] = {
    {{0x20, 0x20}, "??_8?$basic_iostream@DU?$char_traits@D@std@@@std@@7B?$basic_istream@DU?$char_traits@D@std@@@1@@"},
    {{0x10, 0x10}, "??_8?$basic_iostream@DU?$char_traits@D@std@@@std@@7B?$basic_ostream@DU?$char_traits@D@std@@@1@@"},
    {{0x20, 0x20}, "??_8?$basic_iostream@GU?$char_traits@G@std@@@std@@7B?$basic_istream@GU?$char_traits@G@std@@@1@@"},
    {{0x10, 0x10}, "??_8?$basic_iostream@GU?$char_traits@G@std@@@std@@7B?$basic_ostream@GU?$char_traits@G@std@@@1@@"},
    {{0x20, 0x20}, "??_8?$basic_iostream@_WU?$char_traits@_W@std@@@std@@7B?$basic_istream@_WU?$char_traits@_W@std@@@1@@"},
    {{0x10, 0x10}, "??_8?$basic_iostream@_WU?$char_traits@_W@std@@@std@@7B?$basic_ostream@_WU?$char_traits@_W@std@@@1@@"},
    {{0x18, 0x18}, "??_8?$basic_istream@DU?$char_traits@D@std@@@std@@7B@"},
    {{0x18, 0x18}, "??_8?$basic_istream@GU?$char_traits@G@std@@@std@@7B@"},
    {{0x18, 0x18}, "??_8?$basic_istream@_WU?$char_traits@_W@std@@@std@@7B@"},
    {{ 0x8, 0x10}, "??_8?$basic_ostream@DU?$char_traits@D@std@@@std@@7B@"},
    {{ 0x8, 0x10}, "??_8?$basic_ostream@GU?$char_traits@G@std@@@std@@7B@"},
    {{ 0x8, 0x10}, "??_8?$basic_ostream@_WU?$char_traits@_W@std@@@std@@7B@"},
    {{ 0x0,  0x0}, 0}
};

static void test_vbtable_size_exports(void)
{
    int i;
    const int *p_vbtable;
    int arch_idx = (sizeof(void*) == 8);

    for (i = 0; vbtable_size_exports_list[i].export_name; i++)
    {
        SET(p_vbtable, vbtable_size_exports_list[i].export_name);

        ok(p_vbtable[0] == 0, "vbtable[0] wrong, got 0x%x\n", p_vbtable[0]);
        ok(p_vbtable[1] == vbtable_size_exports_list[i].value[arch_idx],
                "%d: %s[1] wrong, got 0x%x\n", i, vbtable_size_exports_list[i].export_name, p_vbtable[1]);
    }
}

static void test_task_continuation_context(void)
{
    task_continuation_context tcc;

    memset(&tcc, 0xff, sizeof(tcc));
    call_func1(p_task_continuation_context_ctor, &tcc);
    ok(!tcc.unk0, "tcc.unk0 != NULL (%p)\n", tcc.unk0);
    ok(!tcc.unk1, "tcc.unk1 != 0 (%x)\n", tcc.unk1);
}

#ifdef _WIN64
static void __cdecl function_do_call(void *this)
{
    CHECK_EXPECT(function_do_call);
}

static void __cdecl function_do_clean(void *this, MSVCP_bool b)
{
    CHECK_EXPECT(function_do_clean);
    ok(b, "b == FALSE\n");
}
#endif

static void test__ContextCallback(void)
{
    _ContextCallback cc = {0};
    void *v = (void*)0xdeadbeef;
#ifdef _WIN64
    void* function_vtbl[] = {
        NULL,
        NULL,
        (void*)function_do_call,
        NULL,
        (void*)function_do_clean,
        NULL
    };
    function_void_cdecl_void function = { function_vtbl, NULL, {0}, {NULL}, &function };
    function_void_cdecl_void function2 = { NULL, NULL, {0}, {NULL}, &function };
#endif

    call_func2(p__ContextCallback__Assign, &cc, v);
    ok(!cc.unused, "cc.unused = %p\n", cc.unused);
    call_func1(p__ContextCallback__Reset, &cc);
    ok(!cc.unused, "cc.unused = %p\n", cc.unused);
    call_func1(p__ContextCallback__Capture, &cc);
    ok(!cc.unused, "cc.unused = %p\n", cc.unused);
    ok(!p__ContextCallback__IsCurrentOriginSTA(&cc), "IsCurrentOriginSTA returned TRUE\n");

    cc.unused = v;
    call_func2(p__ContextCallback__Assign, &cc, NULL);
    ok(cc.unused == v, "cc.unused = %p\n", cc.unused);
    call_func1(p__ContextCallback__Reset, &cc);
    ok(cc.unused == v, "cc.unused = %p\n", cc.unused);
    call_func1(p__ContextCallback__Capture, &cc);
    ok(cc.unused == v, "cc.unused = %p\n", cc.unused);
    ok(!p__ContextCallback__IsCurrentOriginSTA(&cc), "IsCurrentOriginSTA returned TRUE\n");
    ok(cc.unused == v, "cc.unused = %p\n", cc.unused);

#ifdef _WIN64
    SET_EXPECT(function_do_call);
    SET_EXPECT(function_do_clean);
    p__ContextCallback__CallInContext(&cc, function, FALSE);
    CHECK_CALLED(function_do_call);
    CHECK_CALLED(function_do_clean);

    SET_EXPECT(function_do_call);
    SET_EXPECT(function_do_clean);
    p__ContextCallback__CallInContext(&cc, function, TRUE);
    CHECK_CALLED(function_do_call);
    CHECK_CALLED(function_do_clean);

    SET_EXPECT(function_do_call);
    SET_EXPECT(function_do_clean);
    p__ContextCallback__CallInContext(&cc, function2, FALSE);
    CHECK_CALLED(function_do_call);
    CHECK_CALLED(function_do_clean);

    SET_EXPECT(function_do_call);
    SET_EXPECT(function_do_clean);
    p__ContextCallback__CallInContext(&cc, function2, TRUE);
    CHECK_CALLED(function_do_call);
    CHECK_CALLED(function_do_clean);
#endif
}

static void test__TaskEventLogger(void)
{
    _TaskEventLogger logger;
    memset(&logger, 0, sizeof(logger));

    call_func1(p__TaskEventLogger__LogCancelTask, &logger);
    ok(!logger.task, "logger.task = %p\n", logger.task);
    ok(!logger.scheduled, "logger.scheduled = %x\n", logger.scheduled);
    ok(!logger.started, "logger.started = %x\n", logger.started);

    call_func2(p__TaskEventLogger__LogScheduleTask, &logger, FALSE);
    ok(!logger.task, "logger.task = %p\n", logger.task);
    ok(!logger.scheduled, "logger.scheduled = %x\n", logger.scheduled);
    ok(!logger.started, "logger.started = %x\n", logger.started);

    call_func1(p__TaskEventLogger__LogTaskCompleted, &logger);
    ok(!logger.task, "logger.task = %p\n", logger.task);
    ok(!logger.scheduled, "logger.scheduled = %x\n", logger.scheduled);
    ok(!logger.started, "logger.started = %x\n", logger.started);

    call_func1(p__TaskEventLogger__LogTaskExecutionCompleted, &logger);
    ok(!logger.task, "logger.task = %p\n", logger.task);
    ok(!logger.scheduled, "logger.scheduled = %x\n", logger.scheduled);
    ok(!logger.started, "logger.started = %x\n", logger.started);

    call_func1(p__TaskEventLogger__LogWorkItemCompleted, &logger);
    ok(!logger.task, "logger.task = %p\n", logger.task);
    ok(!logger.scheduled, "logger.scheduled = %x\n", logger.scheduled);
    ok(!logger.started, "logger.started = %x\n", logger.started);

    call_func1(p__TaskEventLogger__LogWorkItemStarted, &logger);
    ok(!logger.task, "logger.task = %p\n", logger.task);
    ok(!logger.scheduled, "logger.scheduled = %x\n", logger.scheduled);
    ok(!logger.started, "logger.started = %x\n", logger.started);

    logger.task = (void*)0xdeadbeef;
    logger.scheduled = TRUE;
    logger.started = TRUE;

    call_func1(p__TaskEventLogger__LogCancelTask, &logger);
    ok(logger.task == (void*)0xdeadbeef, "logger.task = %p\n", logger.task);
    ok(logger.scheduled, "logger.scheduled = FALSE\n");
    ok(logger.started, "logger.started = FALSE\n");

    call_func2(p__TaskEventLogger__LogScheduleTask, &logger, FALSE);
    ok(logger.task == (void*)0xdeadbeef, "logger.task = %p\n", logger.task);
    ok(logger.scheduled, "logger.scheduled = FALSE\n");
    ok(logger.started, "logger.started = FALSE\n");

    call_func1(p__TaskEventLogger__LogTaskCompleted, &logger);
    ok(logger.task == (void*)0xdeadbeef, "logger.task = %p\n", logger.task);
    ok(logger.scheduled, "logger.scheduled = FALSE\n");
    ok(logger.started, "logger.started = FALSE\n");

    call_func1(p__TaskEventLogger__LogTaskExecutionCompleted, &logger);
    ok(logger.task == (void*)0xdeadbeef, "logger.task = %p\n", logger.task);
    ok(logger.scheduled, "logger.scheduled = FALSE\n");
    ok(logger.started, "logger.started = FALSE\n");

    call_func1(p__TaskEventLogger__LogWorkItemCompleted, &logger);
    ok(logger.task == (void*)0xdeadbeef, "logger.task = %p\n", logger.task);
    ok(logger.scheduled, "logger.scheduled = FALSE\n");
    ok(logger.started, "logger.started = FALSE\n");

    call_func1(p__TaskEventLogger__LogWorkItemStarted, &logger);
    ok(logger.task == (void*)0xdeadbeef, "logger.task = %p\n", logger.task);
    ok(logger.scheduled, "logger.scheduled = FALSE\n");
    ok(logger.started, "logger.started = FALSE\n");
}

static void __cdecl chore_callback(void *arg)
{
    HANDLE event = arg;
    SetEvent(event);
}

static void test_chore(void)
{
    HANDLE event = CreateEventW(NULL, FALSE, FALSE, NULL);
    _Threadpool_chore chore, old_chore;
    DWORD wait;
    int ret;

    memset(&chore, 0, sizeof(chore));
    ret = p__Schedule_chore(&chore);
    ok(!ret, "_Schedule_chore returned %d\n", ret);
    ok(chore.work != NULL, "chore.work == NULL\n");
    ok(!chore.callback, "chore.callback != NULL\n");
    p__Release_chore(&chore);

    chore.work = NULL;
    chore.callback = chore_callback;
    chore.arg = event;
    ret = p__Schedule_chore(&chore);
    ok(!ret, "_Schedule_chore returned %d\n", ret);
    ok(chore.work != NULL, "chore.work == NULL\n");
    ok(chore.callback == chore_callback, "chore.callback = %p, expected %p\n", chore.callback, chore_callback);
    ok(chore.arg == event, "chore.arg = %p, expected %p\n", chore.arg, event);
    wait = WaitForSingleObject(event, 500);
    ok(wait == WAIT_OBJECT_0, "WaitForSingleObject returned %d\n", wait);

    if(!GetProcAddress(GetModuleHandleA("kernel32"), "CreateThreadpoolWork"))
    {
        win_skip("_Reschedule_chore not supported\n");
        p__Release_chore(&chore);
        CloseHandle(event);
        return;
    }

    old_chore = chore;
    ret = p__Schedule_chore(&chore);
    ok(!ret, "_Schedule_chore returned %d\n", ret);
    ok(old_chore.work != chore.work, "new threadpool work was not created\n");
    p__Release_chore(&old_chore);
    wait = WaitForSingleObject(event, 500);
    ok(wait == WAIT_OBJECT_0, "WaitForSingleObject returned %d\n", wait);

    ret = p__Reschedule_chore(&chore);
    ok(!ret, "_Reschedule_chore returned %d\n", ret);
    wait = WaitForSingleObject(event, 500);
    ok(wait == WAIT_OBJECT_0, "WaitForSingleObject returned %d\n", wait);

    p__Release_chore(&chore);
    ok(!chore.work, "chore.work != NULL\n");
    ok(chore.callback == chore_callback, "chore.callback = %p, expected %p\n", chore.callback, chore_callback);
    ok(chore.arg == event, "chore.arg = %p, expected %p\n", chore.arg, event);
    p__Release_chore(&chore);

    CloseHandle(event);
}

static void test_to_byte(void)
{
    static const WCHAR test_1[] = {'T', 'E', 'S', 'T', 0};
    static const WCHAR test_2[] = {0x9580, 0x9581, 0x9582, 0x9583, 0}; /* some CJK characters */
    static const WCHAR *tests[] = {test_1, test_2};

    char dst[MAX_PATH + 4] = "ABC\0XXXXXXX";
    char compare[MAX_PATH + 4] = "ABC\0XXXXXXX";
    int ret,expected;
    unsigned int i, j;
    WCHAR longstr[MAX_PATH + 3];

    ret = p_To_byte(NULL, NULL);
    ok(!ret, "Got unexpected result %d\n", ret);
    ret = p_To_byte(tests[0], NULL);
    ok(!ret, "Got unexpected result %d\n", ret);
    ret = p_To_byte(NULL, dst);
    ok(!ret, "Got unexpected result %d\n", ret);

    ok(!memcmp(dst, compare, sizeof(compare)), "Destination was modified: %s\n", dst);

    for (i = 0; i < ARRAY_SIZE(tests); ++i)
    {
        ret = p_To_byte(tests[i], dst);
        expected = WideCharToMultiByte(CP_ACP, 0, tests[i], -1, compare, ARRAY_SIZE(compare), NULL, NULL);
        ok(ret == expected,  "Got unexpected result %d, expected %d, test case %u\n", ret, expected, i);
        ok(!memcmp(dst, compare, sizeof(compare)), "Got unexpected output %s, test case %u\n", dst, i);
    }

    /* Output length is limited to MAX_PATH.*/
    for (i = MAX_PATH - 2; i < MAX_PATH + 2; ++i)
    {
        for (j = 0; j < i; j++)
            longstr[j] = 'A';
        longstr[i] = 0;
        memset(dst, 0xff, sizeof(dst));
        memset(compare, 0xff, sizeof(compare));

        ret = p_To_byte(longstr, dst);
        expected = WideCharToMultiByte(CP_ACP, 0, longstr, -1, compare, MAX_PATH, NULL, NULL);
        ok(ret == expected,  "Got unexpected result %d, expected %d, length %u\n", ret, expected, i);
        ok(!memcmp(dst, compare, sizeof(compare)), "Got unexpected output %s, length %u\n", dst, i);
    }
}

static void test_to_wide(void)
{
     /* öäüß€Ÿ.A.B in cp1252, the two . are an undefined value and delete.
      * With a different system codepage it will produce different results, so do not hardcode the
      * expected output but convert it with MultiByteToWideChar. */
    static const char special_input[] = {0xf6, 0xe4, 0xfc, 0xdf, 0x80, 0x9f, 0x81, 0x41, 0x7f, 0x42, 0};
    static const char *tests[] = {"Testtest", special_input};
    WCHAR dst[MAX_PATH + 4] = {'A', 'B', 'C', 0, 'X', 'X', 'X', 'X', 'X', 'X', 'X'};
    WCHAR compare[MAX_PATH + 4] = {'A', 'B', 'C', 0, 'X', 'X', 'X', 'X', 'X', 'X', 'X'};
    int ret, expected;
    unsigned int i;
    char longstr[MAX_PATH + 3];

    ret = p_To_wide(NULL, NULL);
    ok(!ret, "Got unexpected result %d\n", ret);
    ret = p_To_wide(tests[0], NULL);
    ok(!ret, "Got unexpected result %d\n", ret);
    ret = p_To_wide(NULL, dst);
    ok(!ret, "Got unexpected result %d\n", ret);
    ok(!memcmp(dst, compare, sizeof(compare)), "Destination was modified: %s\n", wine_dbgstr_w(dst));

    for (i = 0; i < ARRAY_SIZE(tests); ++i)
    {
        ret = p_To_wide(tests[i], dst);
        expected = MultiByteToWideChar(CP_ACP, 0, tests[i], -1, compare, ARRAY_SIZE(compare));
        ok(ret == expected,  "Got unexpected result %d, expected %d, test case %u\n", ret, expected, i);
        ok(!memcmp(dst, compare, sizeof(compare)), "Got unexpected output %s, test case %u\n",
                wine_dbgstr_w(dst), i);
    }

    /* Output length is limited to MAX_PATH.*/
    for (i = MAX_PATH - 2; i < MAX_PATH + 2; ++i)
    {
        memset(longstr, 'A', sizeof(longstr));
        longstr[i] = 0;
        memset(dst, 0xff, sizeof(dst));
        memset(compare, 0xff, sizeof(compare));

        ret = p_To_wide(longstr, dst);
        expected = MultiByteToWideChar(CP_ACP, 0, longstr, -1, compare, MAX_PATH);
        ok(ret == expected,  "Got unexpected result %d, expected %d, length %u\n", ret, expected, i);
        ok(!memcmp(dst, compare, sizeof(compare)), "Got unexpected output %s, length %u\n",
                wine_dbgstr_w(dst), i);
    }
}

static void test_File_size(void)
{
    ULONGLONG val;
    HANDLE file;
    LARGE_INTEGER file_size;
    WCHAR test_f1_W[] = {'w','i','n','e','_','t','e','s','t','_','d','i','r','/','f','1',0};
    WCHAR test_f2_W[] = {'w','i','n','e','_','t','e','s','t','_','d','i','r','/','f','2',0};
    WCHAR test_dir_W[] = {'w','i','n','e','_','t','e','s','t','_','d','i','r',0};
    WCHAR test_ne_W[] = {'w','i','n','e','_','t','e','s','t','_','d','i','r','/','n','e',0};
    WCHAR temp_path[MAX_PATH], origin_path[MAX_PATH];
    int r;

    GetCurrentDirectoryW(MAX_PATH, origin_path);
    GetTempPathW(MAX_PATH, temp_path);
    ok(SetCurrentDirectoryW(temp_path), "SetCurrentDirectoryW to temp_path failed\n");

    CreateDirectoryW(test_dir_W, NULL);

    file = CreateFileW(test_f1_W, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    ok(file != INVALID_HANDLE_VALUE, "create file failed: INVALID_HANDLE_VALUE\n");
    file_size.QuadPart = 7;
    ok(SetFilePointerEx(file, file_size, NULL, FILE_BEGIN), "SetFilePointerEx failed\n");
    ok(SetEndOfFile(file), "SetEndOfFile failed\n");
    CloseHandle(file);
    val = p_File_size(test_f1_W);
    ok(val == 7, "file_size is %s\n", wine_dbgstr_longlong(val));

    file = CreateFileW(test_f2_W, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    ok(file != INVALID_HANDLE_VALUE, "create file failed: INVALID_HANDLE_VALUE\n");
    CloseHandle(file);
    val = p_File_size(test_f2_W);
    ok(val == 0, "file_size is %s\n", wine_dbgstr_longlong(val));

    val = p_File_size(test_dir_W);
    ok(val == 0, "file_size is %s\n", wine_dbgstr_longlong(val));

    errno = 0xdeadbeef;
    val = p_File_size(test_ne_W);
    ok(val == ~(ULONGLONG)0, "file_size is %s\n", wine_dbgstr_longlong(val));
    ok(errno == 0xdeadbeef, "errno = %d\n", errno);

    errno = 0xdeadbeef;
    val = p_File_size(NULL);
    ok(val == ~(ULONGLONG)0, "file_size is %s\n", wine_dbgstr_longlong(val));
    ok(errno == 0xdeadbeef, "errno = %d\n", errno);

    r = p_Resize(test_f1_W, 1000);
    ok(!r, "p_Resize returned %d\n", r);
    val = p_File_size(test_f1_W);
    ok(val == 1000, "file_size is %s\n", wine_dbgstr_longlong(val));

    r = p_Resize(test_f1_W, 100);
    ok(!r, "p_Resize returned %d\n", r);
    val = p_File_size(test_f1_W);
    ok(val == 100, "file_size is %s\n", wine_dbgstr_longlong(val));

    r = p_Resize(test_f1_W, 0);
    ok(!r, "p_Resize returned %d\n", r);
    val = p_File_size(test_f1_W);
    ok(val == 0, "file_size is %s\n", wine_dbgstr_longlong(val));

    ok(DeleteFileW(test_f1_W), "expect wine_test_dir/f1 to exist\n");

    r = p_Resize(test_f1_W, 0);
    ok(r == ERROR_FILE_NOT_FOUND, "p_Resize returned %d\n", r);

    ok(DeleteFileW(test_f2_W), "expect wine_test_dir/f2 to exist\n");
    ok(RemoveDirectoryW(test_dir_W), "expect wine_test_dir to exist\n");
    ok(SetCurrentDirectoryW(origin_path), "SetCurrentDirectoryW to origin_path failed\n");
}

static void test_Current_get(void)
{
    static const WCHAR backslashW[] = {'\\',0};
    WCHAR temp_path[MAX_PATH], current_path[MAX_PATH], origin_path[MAX_PATH];
    BOOL ret;

    GetCurrentDirectoryW(MAX_PATH, origin_path);
    GetTempPathW(MAX_PATH, temp_path);

    ok(SetCurrentDirectoryW(temp_path), "SetCurrentDirectoryW to temp_path failed\n");
    ret = p_Current_get(current_path);
    ok(ret == TRUE, "p_Current_get returned %u\n", ret);
    wcscat(current_path, backslashW);
    ok(!wcscmp(temp_path, current_path), "p_Current_get(): expect: %s, got %s\n",
            wine_dbgstr_w(temp_path), wine_dbgstr_w(current_path));

    ok(SetCurrentDirectoryW(origin_path), "SetCurrentDirectoryW to origin_path failed\n");
    ret = p_Current_get(current_path);
    ok(ret == TRUE, "p_Current_get returned %u\n", ret);
    ok(!wcscmp(origin_path, current_path), "p_Current_get(): expect: %s, got %s\n",
            wine_dbgstr_w(origin_path), wine_dbgstr_w(current_path));
}

static void test_Current_set(void)
{
    WCHAR temp_path[MAX_PATH], current_path[MAX_PATH], origin_path[MAX_PATH];
    MSVCP_bool ret;
    static const WCHAR backslashW[] = {'\\',0};
    static const WCHAR testW[] = {'.','/',0};
    static const WCHAR not_exit_dirW[] = {'n', 'o', 't', '_', 'e', 'x', 'i', 's', 't', '_', 'd', 'i', 'r', 0};
    static const WCHAR invalid_nameW[] = {'?', '?', 'i', 'n', 'v', 'a', 'l', 'i', 'd', '_', 'n', 'a', 'm', 'e', '>', '>', 0};

    GetTempPathW(MAX_PATH, temp_path);
    GetCurrentDirectoryW(MAX_PATH, origin_path);

    ok(p_Current_set(temp_path), "p_Current_set to temp_path failed\n");
    ret = p_Current_get(current_path);
    ok(ret == TRUE, "p_Current_get returned %u\n", ret);
    wcscat(current_path, backslashW);
    ok(!wcscmp(temp_path, current_path), "p_Current_get(): expect: %s, got %s\n",
            wine_dbgstr_w(temp_path), wine_dbgstr_w(current_path));

    ok(p_Current_set(testW), "p_Current_set to temp_path failed\n");
    ret = p_Current_get(current_path);
    ok(ret == TRUE, "p_Current_get returned %u\n", ret);
    wcscat(current_path, backslashW);
    ok(!wcscmp(temp_path, current_path), "p_Current_get(): expect: %s, got %s\n",
            wine_dbgstr_w(temp_path), wine_dbgstr_w(current_path));

    errno = 0xdeadbeef;
    ok(!p_Current_set(not_exit_dirW), "p_Current_set to not_exist_dir succeed\n");
    ok(errno == 0xdeadbeef, "errno = %d\n", errno);

    errno = 0xdeadbeef;
    ok(!p_Current_set(invalid_nameW), "p_Current_set to ??invalid_name>> succeed\n");
    ok(errno == 0xdeadbeef, "errno = %d\n", errno);

    ok(p_Current_set(origin_path), "p_Current_set to origin_path failed\n");
    ret = p_Current_get(current_path);
    ok(ret == TRUE, "p_Current_get returned %u\n", ret);
    ok(!wcscmp(origin_path, current_path), "p_Current_get(): expect: %s, got %s\n",
            wine_dbgstr_w(origin_path), wine_dbgstr_w(current_path));
}

static void test_Stat(void)
{
    int i, perms, expected_perms, ret;
    HANDLE file;
    DWORD attr;
    enum file_type val;
    WCHAR test_dirW[] = {'w','i','n','e','_','t','e','s','t','_','d','i','r',0};
    WCHAR test_f1W[] = {'w','i','n','e','_','t','e','s','t','_','d','i','r','/','f','1',0};
    WCHAR test_f2W[] = {'w','i','n','e','_','t','e','s','t','_','d','i','r','/','f','2',0};
    WCHAR pipeW[] = {'\\','\\','.','\\','P','i','P','e','\\','t','e','s','t','s','_','p','i','p','e','.','c', 0};
    WCHAR test_neW[] = {'w','i','n','e','_','t','e','s','t','_','d','i','r','/','n','e',0};
    WCHAR test_invW[] = {'w','i','n','e','_','t','e','s','t','_','d','i','r','\\','?','?','i','n','v','a','l','i','d','_','n','a','m','e','>','>',0};
    WCHAR test_f1_linkW[] = {'w','i','n','e','_','t','e','s','t','_','d','i','r','\\','f','1','_','l','i','n','k',0};
    WCHAR test_dir_linkW[] = {'w','i','n','e','_','t','e','s','t','_','d','i','r','\\','d','i','r','_','l','i','n','k',0};
    WCHAR sys_path[MAX_PATH], origin_path[MAX_PATH], temp_path[MAX_PATH];
    struct {
        WCHAR const *path;
        enum file_type ret;
        int perms;
    } tests[] = {
        { NULL, file_not_found, 0xdeadbeef },
        { test_dirW, directory_file, 0777 },
        { test_f1W, regular_file, 0777 },
        { test_f2W, regular_file, 0555 },
        { test_neW, file_not_found, 0xdeadbeef },
        { test_invW, file_not_found, 0xdeadbeef },
        { test_f1_linkW, regular_file, 0777 },
        { test_dir_linkW, directory_file, 0777 },
    };

    GetCurrentDirectoryW(MAX_PATH, origin_path);
    GetTempPathW(MAX_PATH, temp_path);
    ok(SetCurrentDirectoryW(temp_path), "SetCurrentDirectoryW to temp_path failed\n");

    CreateDirectoryW(test_dirW, NULL);

    file = CreateFileW(test_f1W, 0, 0, NULL, CREATE_ALWAYS, 0, NULL);
    ok(file != INVALID_HANDLE_VALUE, "create file failed: INVALID_HANDLE_VALUE\n");
    ok(CloseHandle(file), "CloseHandle\n");

    file = CreateFileW(test_f2W, 0, 0, NULL, CREATE_ALWAYS, 0, NULL);
    ok(file != INVALID_HANDLE_VALUE, "create file failed: INVALID_HANDLE_VALUE\n");
    ok(CloseHandle(file), "CloseHandle\n");
    SetFileAttributesW(test_f2W, FILE_ATTRIBUTE_READONLY);

    SetLastError(0xdeadbeef);
    ret = pCreateSymbolicLinkW && pCreateSymbolicLinkW(test_f1_linkW, test_f1W, 0);
    if(!ret && (!pCreateSymbolicLinkW || GetLastError()==ERROR_PRIVILEGE_NOT_HELD||GetLastError()==ERROR_INVALID_FUNCTION)) {
        tests[6].ret = tests[7].ret = file_not_found;
        tests[6].perms = tests[7].perms = 0xdeadbeef;
        win_skip("Privilege not held or symbolic link not supported, skipping symbolic link tests.\n");
    }else {
        ok(ret, "CreateSymbolicLinkW failed\n");
        ok(pCreateSymbolicLinkW(test_dir_linkW, test_dirW, 1), "CreateSymbolicLinkW failed\n");
    }

    file = CreateNamedPipeW(pipeW,
            PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 2, 1024, 1024,
            NMPWAIT_USE_DEFAULT_WAIT, NULL);
    ok(file != INVALID_HANDLE_VALUE, "CreateNamedPipe failed\n");
    perms = 0xdeadbeef;
    val = p_Stat(pipeW, &perms);
    todo_wine ok(regular_file == val, "_Stat(): expect: regular, got %d\n", val);
    todo_wine ok(0777 == perms, "_Stat(): perms expect: 0777, got 0%o\n", perms);
    perms = 0xdeadbeef;
    val = p_Lstat(pipeW, &perms);
    ok(status_unknown == val, "_Lstat(): expect: unknown, got %d\n", val);
    ok(0xdeadbeef == perms, "_Lstat(): perms expect: 0xdeadbeef, got %x\n", perms);
    ok(CloseHandle(file), "CloseHandle\n");
    file = CreateNamedPipeW(pipeW,
            PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 2, 1024, 1024,
            NMPWAIT_USE_DEFAULT_WAIT, NULL);
    ok(file != INVALID_HANDLE_VALUE, "CreateNamedPipe failed\n");
    perms = 0xdeadbeef;
    val = p_Lstat(pipeW, &perms);
    todo_wine ok(regular_file == val, "_Lstat(): expect: regular, got %d\n", val);
    todo_wine ok(0777 == perms, "_Lstat(): perms expect: 0777, got 0%o\n", perms);
    ok(CloseHandle(file), "CloseHandle\n");

    for(i=0; i<ARRAY_SIZE(tests); i++) {
        perms = 0xdeadbeef;
        val = p_Stat(tests[i].path, &perms);
        ok(tests[i].ret == val, "_Stat(): test %d expect: %d, got %d\n", i+1, tests[i].ret, val);
        ok(tests[i].perms == perms, "_Stat(): test %d perms expect: 0%o, got 0%o\n",
                i+1, tests[i].perms, perms);
        val = p_Stat(tests[i].path, NULL);
        ok(tests[i].ret == val, "_Stat(): test %d expect: %d, got %d\n", i+1, tests[i].ret, val);

        /* test _Lstat */
        perms = 0xdeadbeef;
        val = p_Lstat(tests[i].path, &perms);
        ok(tests[i].ret == val, "_Lstat(): test %d expect: %d, got %d\n", i+1, tests[i].ret, val);
        ok(tests[i].perms == perms, "_Lstat(): test %d perms expect: 0%o, got 0%o\n",
                i+1, tests[i].perms, perms);
        val = p_Lstat(tests[i].path, NULL);
        ok(tests[i].ret == val, "_Lstat(): test %d expect: %d, got %d\n", i+1, tests[i].ret, val);
    }

    GetSystemDirectoryW(sys_path, MAX_PATH);
    attr = GetFileAttributesW(sys_path);
    expected_perms = (attr & FILE_ATTRIBUTE_READONLY) ? 0555 : 0777;
    perms = 0xdeadbeef;
    val = p_Stat(sys_path, &perms);
    ok(directory_file == val, "_Stat(): expect: regular, got %d\n", val);
    ok(perms == expected_perms, "_Stat(): perms expect: 0%o, got 0%o\n", expected_perms, perms);

    if(ret) {
        ok(DeleteFileW(test_f1_linkW), "expect wine_test_dir/f1_link to exist\n");
        ok(RemoveDirectoryW(test_dir_linkW), "expect wine_test_dir/dir_link to exist\n");
    }
    ok(DeleteFileW(test_f1W), "expect wine_test_dir/f1 to exist\n");
    SetFileAttributesW(test_f2W, FILE_ATTRIBUTE_NORMAL);
    ok(DeleteFileW(test_f2W), "expect wine_test_dir/f2 to exist\n");
    ok(RemoveDirectoryW(test_dirW), "expect wine_test_dir to exist\n");

    ok(SetCurrentDirectoryW(origin_path), "SetCurrentDirectoryW to origin_path failed\n");
}

static void test_dir_operation(void)
{
    WCHAR *file_name, first_file_name[MAX_PATH], dest[MAX_PATH], longer_path[MAX_PATH];
    WCHAR origin_path[MAX_PATH], temp_path[MAX_PATH];
    HANDLE file, result_handle;
    enum file_type type;
    int err, num_of_f1 = 0, num_of_f2 = 0, num_of_sub_dir = 0, num_of_other_files = 0;
    WCHAR test_dirW[] = {'w','i','n','e','_','t','e','s','t','_','d','i','r',0};
    WCHAR test_f1W[] = {'w','i','n','e','_','t','e','s','t','_','d','i','r','/','f','1',0};
    WCHAR test_f2W[] = {'w','i','n','e','_','t','e','s','t','_','d','i','r','/','f','2',0};
    WCHAR test_sub_dirW[] = {'w','i','n','e','_','t','e','s','t','_','d','i','r','/','s','u','b','_','d','i','r',0};
    WCHAR test_sub_dir_f1W[] = {'w','i','n','e','_','t','e','s','t','_','d','i','r',
            '/','s','u','b','_','d','i','r','/','f','1',0};
    WCHAR backslashW[] = {'\\',0};
    WCHAR sW[] = {'s',0};
    WCHAR f1W[] = {'f','1',0};
    WCHAR f2W[] = {'f','2',0};
    WCHAR sub_dirW[] = {'s','u','b','_','d','i','r',0};
    WCHAR not_existW[] = {'n','o','t','_','e','x','i','s','t',0};
    WCHAR empty_dirW[] = {'e','m','p','t','y','_','d','i','r',0};

    GetCurrentDirectoryW(MAX_PATH, origin_path);
    GetTempPathW(MAX_PATH, temp_path);
    ok(SetCurrentDirectoryW(temp_path), "SetCurrentDirectoryW to temp_path failed\n");

    CreateDirectoryW(test_dirW, NULL);
    file = CreateFileW(test_f1W, 0, 0, NULL, CREATE_ALWAYS, 0, NULL);
    ok(file != INVALID_HANDLE_VALUE, "create file failed: INVALID_HANDLE_VALUE\n");
    CloseHandle(file);
    file = CreateFileW(test_f2W, 0, 0, NULL, CREATE_ALWAYS, 0, NULL);
    ok(file != INVALID_HANDLE_VALUE, "create file failed: INVALID_HANDLE_VALUE\n");
    CloseHandle(file);
    CreateDirectoryW(test_sub_dirW, NULL);
    file = CreateFileW(test_sub_dir_f1W, 0, 0, NULL, CREATE_ALWAYS, 0, NULL);
    ok(file != INVALID_HANDLE_VALUE, "create file failed: INVALID_HANDLE_VALUE\n");
    CloseHandle(file);

    memcpy(longer_path, temp_path, sizeof(longer_path));
    wcscat(longer_path, backslashW);
    wcscat(longer_path, test_dirW);
    wcscat(longer_path, backslashW);
    while(lstrlenW(longer_path) < MAX_PATH-1)
        wcscat(longer_path, sW);
    memset(first_file_name, 0xff, sizeof(first_file_name));
    type = err =  0xdeadbeef;
    result_handle = NULL;
    result_handle = p_Open_dir(first_file_name, longer_path, &err, &type);
    ok(result_handle == NULL, "_Open_dir(): expect NULL, got %p\n", result_handle);
    ok(!*first_file_name, "_Open_dir(): expect: 0, got %s\n", wine_dbgstr_w(first_file_name));
    ok(err == ERROR_BAD_PATHNAME, "_Open_dir(): expect: ERROR_BAD_PATHNAME, got %d\n", err);
    ok((int)type == 0xdeadbeef, "_Open_dir(): expect 0xdeadbeef, got %d\n", type);

    memset(first_file_name, 0xff, sizeof(first_file_name));
    memset(dest, 0, sizeof(dest));
    err = type = 0xdeadbeef;
    result_handle = NULL;
    result_handle = p_Open_dir(first_file_name, test_dirW, &err, &type);
    ok(result_handle != NULL, "_Open_dir(): expect: not NULL, got %p\n", result_handle);
    ok(err == ERROR_SUCCESS, "_Open_dir(): expect: ERROR_SUCCESS, got %d\n", err);
    file_name = first_file_name;
    while(*file_name) {
        if (!wcscmp(file_name, f1W)) {
            ++num_of_f1;
            ok(type == regular_file, "expect regular_file, got %d\n", type);
        }else if(!wcscmp(file_name, f2W)) {
            ++num_of_f2;
            ok(type == regular_file, "expect regular_file, got %d\n", type);
        }else if(!wcscmp(file_name, sub_dirW)) {
            ++num_of_sub_dir;
            ok(type == directory_file, "expect directory_file, got %d\n", type);
        }else {
            ++num_of_other_files;
        }
        file_name = p_Read_dir(dest, result_handle, &type);
    }
    ok(type == status_unknown, "_Read_dir(): expect: status_unknown, got %d\n", type);
    p_Close_dir(result_handle);
    ok(result_handle != NULL, "_Open_dir(): expect: not NULL, got %p\n", result_handle);
    ok(num_of_f1 == 1, "found f1 %d times\n", num_of_f1);
    ok(num_of_f2 == 1, "found f2 %d times\n", num_of_f2);
    ok(num_of_sub_dir == 1, "found sub_dir %d times\n", num_of_sub_dir);
    ok(num_of_other_files == 0, "found %d other files\n", num_of_other_files);

    memset(first_file_name, 0xff, sizeof(first_file_name));
    err = type = 0xdeadbeef;
    result_handle = file;
    result_handle = p_Open_dir(first_file_name, not_existW, &err, &type);
    ok(result_handle == NULL, "_Open_dir(): expect: NULL, got %p\n", result_handle);
    ok(err == ERROR_BAD_PATHNAME, "_Open_dir(): expect: ERROR_BAD_PATHNAME, got %d\n", err);
    ok((int)type == 0xdeadbeef, "_Open_dir(): expect: 0xdeadbeef, got %d\n", type);
    ok(!*first_file_name, "_Open_dir(): expect: 0, got %s\n", wine_dbgstr_w(first_file_name));

    CreateDirectoryW(empty_dirW, NULL);
    memset(first_file_name, 0xff, sizeof(first_file_name));
    err = type = 0xdeadbeef;
    result_handle = file;
    result_handle = p_Open_dir(first_file_name, empty_dirW, &err, &type);
    ok(result_handle == NULL, "_Open_dir(): expect: NULL, got %p\n", result_handle);
    ok(err == ERROR_SUCCESS, "_Open_dir(): expect: ERROR_SUCCESS, got %d\n", err);
    ok(type == status_unknown, "_Open_dir(): expect: status_unknown, got %d\n", type);
    ok(!*first_file_name, "_Open_dir(): expect: 0, got %s\n", wine_dbgstr_w(first_file_name));
    p_Close_dir(result_handle);
    ok(result_handle == NULL, "_Open_dir(): expect: NULL, got %p\n", result_handle);

    ok(RemoveDirectoryW(empty_dirW), "expect empty_dir to exist\n");
    ok(DeleteFileW(test_sub_dir_f1W), "expect wine_test_dir/sub_dir/sub_f1 to exist\n");
    ok(RemoveDirectoryW(test_sub_dirW), "expect wine_test_dir/sub_dir to exist\n");
    ok(DeleteFileW(test_f1W), "expect wine_test_dir/f1 to exist\n");
    ok(DeleteFileW(test_f2W), "expect wine_test_dir/f2 to exist\n");
    ok(RemoveDirectoryW(test_dirW), "expect wine_test_dir to exist\n");

    ok(SetCurrentDirectoryW(origin_path), "SetCurrentDirectoryW to origin_path failed\n");
}

static void test_Unlink(void)
{
    WCHAR temp_path[MAX_PATH], current_path[MAX_PATH];
    int ret, i;
    HANDLE file;
    LARGE_INTEGER file_size;
    static const WCHAR f1_symlinkW[] =
            {'w','i','n','e','_','t','e','s','t','_','d','i','r','\\','f','1','_','s','y','m','l','i','n','k',0};
    static const WCHAR f1_linkW[] =
            {'w','i','n','e','_','t','e','s','t','_','d','i','r','\\','f','1','_','l','i','n','k',0};
    static const WCHAR f1W[] =
            {'w','i','n','e','_','t','e','s','t','_','d','i','r','\\','f','1',0};
    static const WCHAR wine_test_dirW[] =
            {'w','i','n','e','_','t','e','s','t','_','d','i','r',0};
    static const WCHAR not_existW[] =
            {'n','o','t','_','e','x','i','s','t',0};
    static const WCHAR not_exist_fileW[] =
            {'n','o','t','_','e','x','i','s','t','_','d','i','r','\\','n','o','t','_','e','x','i','s','t','_','f','i','l','e',0};
    struct {
        WCHAR const *path;
        int last_error;
    } tests[] = {
        { f1_symlinkW, ERROR_SUCCESS },
        { f1_linkW, ERROR_SUCCESS },
        { f1W, ERROR_SUCCESS },
        { wine_test_dirW, ERROR_ACCESS_DENIED },
        { not_existW, ERROR_FILE_NOT_FOUND },
        { not_exist_fileW, ERROR_PATH_NOT_FOUND },
        { NULL, ERROR_PATH_NOT_FOUND }
    };

    GetCurrentDirectoryW(MAX_PATH, current_path);
    GetTempPathW(MAX_PATH, temp_path);
    ok(SetCurrentDirectoryW(temp_path), "SetCurrentDirectoryW to temp_path failed\n");

    ret = p_Make_dir(wine_test_dirW);
    ok(ret == 1, "_Make_dir(): expect 1 got %d\n", ret);
    file = CreateFileW(f1W, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    ok(file != INVALID_HANDLE_VALUE, "create file failed: INVALID_HANDLE_VALUE\n");
    file_size.QuadPart = 7;
    ok(SetFilePointerEx(file, file_size, NULL, FILE_BEGIN), "SetFilePointerEx failed\n");
    ok(SetEndOfFile(file), "SetEndOfFile failed\n");
    CloseHandle(file);

    ret = p_Symlink(f1W, f1_symlinkW);
    if(ret==ERROR_PRIVILEGE_NOT_HELD || ret==ERROR_INVALID_FUNCTION || ret==ERROR_CALL_NOT_IMPLEMENTED) {
        tests[0].last_error = ERROR_FILE_NOT_FOUND;
        win_skip("Privilege not held or symbolic link not supported, skipping symbolic link tests.\n");
    }else {
        ok(ret == ERROR_SUCCESS, "_Symlink(): expect: ERROR_SUCCESS, got %d\n", ret);
    }
    ret = p_Link(f1W, f1_linkW);
    ok(ret == ERROR_SUCCESS, "_Link(): expect: ERROR_SUCCESS, got %d\n", ret);

    for(i=0; i<ARRAY_SIZE(tests); i++) {
        errno = 0xdeadbeef;
        ret = p_Unlink(tests[i].path);
        ok(ret == tests[i].last_error, "_Unlink(): test %d expect: %d, got %d\n",
           i+1, tests[i].last_error, ret);
        ok(errno == 0xdeadbeef, "_Unlink(): test %d errno expect: 0xdeadbeef, got %d\n", i+1, ret);
    }

    ok(!DeleteFileW(f1W), "expect wine_test_dir/f1 not to exist\n");
    ok(!DeleteFileW(f1_linkW), "expect wine_test_dir/f1_link not to exist\n");
    ok(!DeleteFileW(f1_symlinkW), "expect wine_test_dir/f1_symlink not to exist\n");
    ret = p_Remove_dir(wine_test_dirW);
    ok(ret == 1, "_Remove_dir(): expect 1 got %d\n", ret);

    ok(SetCurrentDirectoryW(current_path), "SetCurrentDirectoryW failed\n");
}

static void test_Temp_get(void)
{
    WCHAR path[MAX_PATH + 1], temp_path[MAX_PATH];
    WCHAR *retval;
    DWORD len;

    GetTempPathW(ARRAY_SIZE(temp_path), temp_path);

    /* This crashes on Windows, the input pointer is not validated. */
    if (0)
    {
        retval = p_Temp_get(NULL);
        ok(!retval, "_Temp_get(): Got %p\n", retval);
    }

    memset(path, 0xaa, sizeof(path));
    retval = p_Temp_get(path);
    ok(retval == path, "_Temp_get(): Got %p, expected %p\n", retval, path);
    ok(!wcscmp(path, temp_path), "Expected path %s, got %s\n", wine_dbgstr_w(temp_path), wine_dbgstr_w(path));
    len = wcslen(path);
    todo_wine ok(path[len + 1] == 0xaaaa, "Too many bytes were zeroed - %x\n", path[len + 1]);
}

static void test_Rename(void)
{
    int ret, i;
    HANDLE file, h1, h2;
    BY_HANDLE_FILE_INFORMATION info1, info2;
    WCHAR temp_path[MAX_PATH], current_path[MAX_PATH];
    LARGE_INTEGER file_size;
    static const WCHAR wine_test_dirW[] =
            {'w','i','n','e','_','t','e','s','t','_','d','i','r',0};
    static const WCHAR f1W[] =
            {'w','i','n','e','_','t','e','s','t','_','d','i','r','\\','f','1',0};
    static const WCHAR f1_renameW[] =
            {'w','i','n','e','_','t','e','s','t','_','d','i','r','\\','f','1','_','r','e','n','a','m','e',0};
    static const WCHAR f1_rename2W[] =
            {'w','i','n','e','_','t','e','s','t','_','d','i','r','\\','f','1','_','r','e','n','a','m','e','2',0};
    static const WCHAR not_existW[] =
            {'w','i','n','e','_','t','e','s','t','_','d','i','r','\\','n','o','t','_','e','x','i','s','t',0};
    static const WCHAR not_exist2W[] =
            {'w','i','n','e','_','t','e','s','t','_','d','i','r','\\','n','o','t','_','e','x','i','s','t','2',0};
    static const WCHAR invalidW[] =
            {'w','i','n','e','_','t','e','s','t','_','d','i','r','\\','?','?','i','n','v','a','l','i','d','>',0};
    static const struct {
        const WCHAR *old_path;
        const WCHAR *new_path;
        int val;
    } tests[] = {
        { f1W, f1_renameW, ERROR_SUCCESS },
        { f1W, NULL, ERROR_FILE_NOT_FOUND },
        { f1W, f1_renameW, ERROR_FILE_NOT_FOUND },
        { NULL, f1_rename2W, ERROR_PATH_NOT_FOUND },
        { f1_renameW, invalidW, ERROR_INVALID_NAME },
        { not_existW, not_exist2W, ERROR_FILE_NOT_FOUND },
        { not_existW, invalidW, ERROR_FILE_NOT_FOUND }
    };

    GetCurrentDirectoryW(MAX_PATH, current_path);
    GetTempPathW(MAX_PATH, temp_path);
    ok(SetCurrentDirectoryW(temp_path), "SetCurrentDirectoryW to temp_path failed\n");
    ret = p_Make_dir(wine_test_dirW);

    ok(ret == 1, "_Make_dir(): expect 1 got %d\n", ret);
    file = CreateFileW(f1W, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    ok(file != INVALID_HANDLE_VALUE, "create file failed: INVALID_HANDLE_VALUE\n");
    CloseHandle(file);

    ret = p_Rename(f1W, f1W);
    ok(ERROR_SUCCESS == ret, "_Rename(): expect: ERROR_SUCCESS, got %d\n", ret);
    for(i=0; i<ARRAY_SIZE(tests); i++) {
        errno = 0xdeadbeef;
        if(tests[i].val == ERROR_SUCCESS) {
            h1 = CreateFileW(tests[i].old_path, 0, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
                    NULL, OPEN_EXISTING, 0, 0);
            ok(h1 != INVALID_HANDLE_VALUE, "create file failed: INVALID_HANDLE_VALUE\n");
            ok(GetFileInformationByHandle(h1, &info1), "GetFileInformationByHandle failed\n");
            CloseHandle(h1);
        }
        SetLastError(0xdeadbeef);
        ret = p_Rename(tests[i].old_path, tests[i].new_path);
        ok(ret == tests[i].val, "_Rename(): test %d expect: %d, got %d\n", i+1, tests[i].val, ret);
        ok(errno == 0xdeadbeef, "_Rename(): test %d errno expect 0xdeadbeef, got %d\n", i+1, errno);
        if(ret == ERROR_SUCCESS) {
            h2 = CreateFileW(tests[i].new_path, 0, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
                    NULL, OPEN_EXISTING, 0, 0);
            ok(h2 != INVALID_HANDLE_VALUE, "create file failed: INVALID_HANDLE_VALUE\n");
            ok(GetFileInformationByHandle(h2, &info2), "GetFileInformationByHandle failed\n");
            CloseHandle(h2);
            ok(info1.nFileIndexHigh == info2.nFileIndexHigh
                    && info1.nFileIndexLow == info2.nFileIndexLow,
                    "_Rename(): test %d expect two files equivalent\n", i+1);
        }
    }

    file = CreateFileW(f1W, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    ok(file != INVALID_HANDLE_VALUE, "create file failed: INVALID_HANDLE_VALUE\n");
    file_size.QuadPart = 7;
    ok(SetFilePointerEx(file, file_size, NULL, FILE_BEGIN), "SetFilePointerEx failed\n");
    ok(SetEndOfFile(file), "SetEndOfFile failed\n");
    CloseHandle(file);
    ret = p_Rename(f1W, f1_renameW);
    ok(ret == ERROR_ALREADY_EXISTS, "_Rename(): expect: ERROR_ALREADY_EXISTS, got %d\n", ret);
    ok(p_File_size(f1W) == 7, "_Rename(): expect: 7, got %s\n",
            wine_dbgstr_longlong(p_File_size(f1W)));
    ok(p_File_size(f1_renameW) == 0, "_Rename(): expect: 0, got %s\n",
            wine_dbgstr_longlong(p_File_size(f1_renameW)));

    ok(DeleteFileW(f1_renameW), "expect f1_rename to exist\n");
    ok(DeleteFileW(f1W), "expect f1 to exist\n");
    ret = p_Remove_dir(wine_test_dirW);
    ok(ret == 1, "_Remove_dir(): expect %d got %d\n", 1, ret);
    ok(SetCurrentDirectoryW(current_path), "SetCurrentDirectoryW failed\n");
}

static void test_Last_write_time(void)
{
    HANDLE file;
    int ret;
    FILETIME lwt;
    __int64 last_write_time, newtime, margin_of_error = 10 * TICKSPERSEC;
    static const WCHAR test_dirW[] = {'w','i','n','e','_','t','e','s','t','_','d','i','r',0};
    static const WCHAR f1W[] =
            {'w','i','n','e','_','t','e','s','t','_','d','i','r','\\','f','1',0};
    static const WCHAR not_existW[] =
            {'w','i','n','e','_','t','e','s','t','_','d','i','r','\\','n','o','t','_','e','x','i','s','t',0};
    WCHAR temp_path[MAX_PATH], origin_path[MAX_PATH];

    GetCurrentDirectoryW(ARRAY_SIZE(origin_path), origin_path);
    GetTempPathW(ARRAY_SIZE(temp_path), temp_path);
    ok(SetCurrentDirectoryW(temp_path), "SetCurrentDirectoryW to temp_path failed\n");

    ret = p_Make_dir(test_dirW);
    ok(ret == 1, "_Make_dir() expect 1 got %d\n", ret);

    file = CreateFileW(f1W, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    ok(file != INVALID_HANDLE_VALUE, "create file failed: INVALID_HANDLE_VALUE\n");
    CloseHandle(file);

    last_write_time = p_Last_write_time(f1W);
    newtime = last_write_time + 222222;
    p_Set_last_write_time(f1W, newtime);
    ok(last_write_time != p_Last_write_time(f1W),
            "last_write_time should have changed: %s\n",
            wine_dbgstr_longlong(last_write_time));

    /* test the formula */
    file = CreateFileW(f1W, 0, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
    ok(file != INVALID_HANDLE_VALUE, "create file failed: INVALID_HANDLE_VALUE\n");
    ok(GetFileTime(file, 0, 0, &lwt), "GetFileTime failed\n");
    CloseHandle(file);
    last_write_time = (((__int64)lwt.dwHighDateTime)<< 32) + lwt.dwLowDateTime;
    last_write_time -= TICKS_1601_TO_1970;
    ok(newtime-margin_of_error<=last_write_time && last_write_time<=newtime+margin_of_error,
            "don't fit the formula, last_write_time is %s expected %s\n",
            wine_dbgstr_longlong(newtime), wine_dbgstr_longlong(last_write_time));

    newtime = 0;
    p_Set_last_write_time(f1W, newtime);
    newtime = p_Last_write_time(f1W);
    file = CreateFileW(f1W, 0, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
    ok(file != INVALID_HANDLE_VALUE, "create file failed: INVALID_HANDLE_VALUE\n");
    ok(GetFileTime(file, 0, 0, &lwt), "GetFileTime failed\n");
    CloseHandle(file);
    last_write_time = (((__int64)lwt.dwHighDateTime)<< 32) + lwt.dwLowDateTime;
    last_write_time -= TICKS_1601_TO_1970;
    ok(newtime-margin_of_error<=last_write_time && last_write_time<=newtime+margin_of_error,
            "don't fit the formula, last_write_time is %s expected %s\n",
            wine_dbgstr_longlong(newtime), wine_dbgstr_longlong(last_write_time));

    newtime = 123456789;
    p_Set_last_write_time(f1W, newtime);
    newtime = p_Last_write_time(f1W);
    file = CreateFileW(f1W, 0, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
    ok(file != INVALID_HANDLE_VALUE, "create file failed: INVALID_HANDLE_VALUE\n");
    ok(GetFileTime(file, 0, 0, &lwt), "GetFileTime failed\n");
    CloseHandle(file);
    last_write_time = (((__int64)lwt.dwHighDateTime)<< 32) + lwt.dwLowDateTime;
    last_write_time -= TICKS_1601_TO_1970;
    ok(newtime-margin_of_error<=last_write_time && last_write_time<=newtime+margin_of_error,
            "don't fit the formula, last_write_time is %s expected %s\n",
            wine_dbgstr_longlong(newtime), wine_dbgstr_longlong(last_write_time));

    errno = 0xdeadbeef;
    last_write_time = p_Last_write_time(not_existW);
    ok(errno == 0xdeadbeef, "_Set_last_write_time(): errno expect 0xdeadbeef, got %d\n", errno);
    ok(last_write_time == -1, "expect -1 got %s\n", wine_dbgstr_longlong(last_write_time));
    last_write_time = p_Last_write_time(NULL);
    ok(last_write_time == -1, "expect -1 got %s\n", wine_dbgstr_longlong(last_write_time));

    errno = 0xdeadbeef;
    p_Set_last_write_time(not_existW, newtime);
    ok(errno == 0xdeadbeef, "_Set_last_write_time(): errno expect 0xdeadbeef, got %d\n", errno);
    p_Set_last_write_time(NULL, newtime);
    ok(errno == 0xdeadbeef, "_Set_last_write_time(): errno expect 0xdeadbeef, got %d\n", errno);

    ok(DeleteFileW(f1W), "expect wine_test_dir/f1 to exist\n");
    ret = p_Remove_dir(test_dirW);
    ok(ret == 1, "p_Remove_dir(): expect 1 got %d\n", ret);
    ok(SetCurrentDirectoryW(origin_path), "SetCurrentDirectoryW to origin_path failed\n");
}

static void test__Winerror_message(void)
{
    char buf[256], buf_fm[256];
    ULONG ret, ret_fm;

    ret_fm = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, 0, 0, buf_fm, sizeof(buf_fm), NULL);

    memset(buf, 'a', sizeof(buf));
    ret = p__Winerror_message(0, buf, sizeof(buf));
    ok(ret == ret_fm, "ret = %u, expected %u\n", ret, ret_fm);
    ok(!strcmp(buf, buf_fm), "buf = %s, expected %s\n", buf, buf_fm);

    memset(buf, 'a', sizeof(buf));
    ret = p__Winerror_message(0, buf, 2);
    ok(!ret, "ret = %u\n", ret);
    ok(buf[0] == 'a', "buf = %s\n", buf);
}

static void test__Winerror_map(void)
{
    static struct {
        int winerr, doserr;
        BOOL broken;
    } tests[] = {
        {ERROR_INVALID_FUNCTION, ENOSYS}, {ERROR_FILE_NOT_FOUND, ENOENT},
        {ERROR_PATH_NOT_FOUND, ENOENT}, {ERROR_TOO_MANY_OPEN_FILES, EMFILE},
        {ERROR_ACCESS_DENIED, EACCES}, {ERROR_INVALID_HANDLE, EINVAL},
        {ERROR_NOT_ENOUGH_MEMORY, ENOMEM}, {ERROR_INVALID_ACCESS, EACCES},
        {ERROR_OUTOFMEMORY, ENOMEM}, {ERROR_INVALID_DRIVE, ENODEV},
        {ERROR_CURRENT_DIRECTORY, EACCES}, {ERROR_NOT_SAME_DEVICE, EXDEV},
        {ERROR_WRITE_PROTECT, EACCES}, {ERROR_BAD_UNIT, ENODEV},
        {ERROR_NOT_READY, EAGAIN}, {ERROR_SEEK, EIO}, {ERROR_WRITE_FAULT, EIO},
        {ERROR_READ_FAULT, EIO}, {ERROR_SHARING_VIOLATION, EACCES},
        {ERROR_LOCK_VIOLATION, ENOLCK}, {ERROR_HANDLE_DISK_FULL, ENOSPC},
        {ERROR_NOT_SUPPORTED, ENOTSUP, TRUE}, {ERROR_DEV_NOT_EXIST, ENODEV},
        {ERROR_FILE_EXISTS, EEXIST}, {ERROR_CANNOT_MAKE, EACCES},
        {ERROR_INVALID_PARAMETER, EINVAL, TRUE}, {ERROR_OPEN_FAILED, EIO},
        {ERROR_BUFFER_OVERFLOW, ENAMETOOLONG}, {ERROR_DISK_FULL, ENOSPC},
        {ERROR_INVALID_NAME, EINVAL}, {ERROR_NEGATIVE_SEEK, EINVAL},
        {ERROR_BUSY_DRIVE, EBUSY}, {ERROR_DIR_NOT_EMPTY, ENOTEMPTY},
        {ERROR_BUSY, EBUSY}, {ERROR_ALREADY_EXISTS, EEXIST},
        {ERROR_LOCKED, ENOLCK}, {ERROR_DIRECTORY, EINVAL},
        {ERROR_OPERATION_ABORTED, ECANCELED}, {ERROR_NOACCESS, EACCES},
        {ERROR_CANTOPEN, EIO}, {ERROR_CANTREAD, EIO}, {ERROR_CANTWRITE, EIO},
        {ERROR_RETRY, EAGAIN}, {ERROR_OPEN_FILES, EBUSY},
        {ERROR_DEVICE_IN_USE, EBUSY}, {ERROR_REPARSE_TAG_INVALID, EINVAL, TRUE},
        {WSAEINTR, EINTR}, {WSAEBADF, EBADF}, {WSAEACCES, EACCES},
        {WSAEFAULT, EFAULT}, {WSAEINVAL, EINVAL}, {WSAEMFILE, EMFILE},
        {WSAEWOULDBLOCK, EWOULDBLOCK}, {WSAEINPROGRESS, EINPROGRESS},
        {WSAEALREADY, EALREADY}, {WSAENOTSOCK, ENOTSOCK},
        {WSAEDESTADDRREQ, EDESTADDRREQ}, {WSAEMSGSIZE, EMSGSIZE},
        {WSAEPROTOTYPE, EPROTOTYPE}, {WSAENOPROTOOPT, ENOPROTOOPT},
        {WSAEPROTONOSUPPORT, EPROTONOSUPPORT}, {WSAEOPNOTSUPP, EOPNOTSUPP},
        {WSAEAFNOSUPPORT, EAFNOSUPPORT}, {WSAEADDRINUSE, EADDRINUSE},
        {WSAEADDRNOTAVAIL, EADDRNOTAVAIL}, {WSAENETDOWN, ENETDOWN},
        {WSAENETUNREACH, ENETUNREACH}, {WSAENETRESET, ENETRESET},
        {WSAECONNABORTED, ECONNABORTED}, {WSAECONNRESET, ECONNRESET},
        {WSAENOBUFS, ENOBUFS}, {WSAEISCONN, EISCONN}, {WSAENOTCONN, ENOTCONN},
        {WSAETIMEDOUT, ETIMEDOUT}, {WSAECONNREFUSED, ECONNREFUSED},
        {WSAENAMETOOLONG, ENAMETOOLONG}, {WSAEHOSTUNREACH, EHOSTUNREACH}
    };
    int i, ret;

    for(i=0; i<ARRAY_SIZE(tests); i++)
    {
        ret = p__Winerror_map(tests[i].winerr);
        ok(ret == tests[i].doserr || broken(tests[i].broken && !ret),
                "_Winerror_map(%d) returned %d, expected %d\n",
                tests[i].winerr, ret, tests[i].doserr);
    }
}

static void test_Equivalent(void)
{
    int val, i;
    HANDLE file;
    WCHAR temp_path[MAX_PATH], current_path[MAX_PATH];
    static const WCHAR wine_test_dirW[] =
            {'w','i','n','e','_','t','e','s','t','_','d','i','r',0};
    static const WCHAR f1W[] =
            {'w','i','n','e','_','t','e','s','t','_','d','i','r','/','f','1',0};
    static const WCHAR f1W_backslash[] =
            {'w','i','n','e','_','t','e','s','t','_','d','i','r','\\','f','1',0};
    static const WCHAR f1W_subdir[] =
            {'w','i','n','e','_','t','e','s','t','_','d','i','r','/','.','/','f','1',0};
    static const WCHAR f1W_long[] =
            {'w','i','n','e','_','t','e','s','t','_','d','i','r','/','.','.','/','w','i','n','e','_','t','e','s','t','_','d','i','r','/','f','1',0};
    static const WCHAR f2W[] =
            {'w','i','n','e','_','t','e','s','t','_','d','i','r','/','f','2',0};
    static const WCHAR not_existW[] =
            {'n','o','t','_','e','x','i','s','t','s','_','f','i','l','e',0};
    static const struct {
        const WCHAR *path1;
        const WCHAR *path2;
        int equivalent;
    } tests[] = {
        { NULL, NULL, -1 },
        { NULL, f1W, 0 },
        { f1W, NULL, 0 },
        { f1W, wine_test_dirW, 0 },
        { wine_test_dirW, f1W, 0 },
        { f1W_subdir, f2W, 0 },
        { f1W, f1W, 1 },
        { not_existW, f1W, 0 },
        { f1W_backslash, f1W_subdir, 1 },
        { not_existW, not_existW, -1 },
        { f1W, not_existW, 0 },
        { f1W_long, f1W, 1 }
    };

    GetCurrentDirectoryW(MAX_PATH, current_path);
    GetTempPathW(MAX_PATH, temp_path);
    ok(SetCurrentDirectoryW(temp_path), "SetCurrentDirectoryW to temp_path failed\n");
    CreateDirectoryW(wine_test_dirW, NULL);

    file = CreateFileW(f1W, 0, 0, NULL, CREATE_ALWAYS, 0, NULL);
    ok(file != INVALID_HANDLE_VALUE, "create file failed: INVALID_HANDLE_VALUE\n");
    CloseHandle(file);
    file = CreateFileW(f2W, 0, 0, NULL, CREATE_ALWAYS, 0, NULL);
    ok(file != INVALID_HANDLE_VALUE, "create file failed: INVALID_HANDLE_VALUE\n");
    CloseHandle(file);

    for(i=0; i<ARRAY_SIZE(tests); i++) {
        errno = 0xdeadbeef;
        val = p_Equivalent(tests[i].path1, tests[i].path2);
        ok(tests[i].equivalent == val, "_Equivalent(): test %d expect: %d, got %d\n", i+1, tests[i].equivalent, val);
        ok(errno == 0xdeadbeef, "errno = %d\n", errno);
    }

    errno = 0xdeadbeef;
    val = p_Equivalent(wine_test_dirW, wine_test_dirW);
    ok(val == 1 || broken(val == -1), "_Equivalent() returned %d, expected %d\n", val, 1);
    ok(errno == 0xdeadbeef, "errno = %d\n", errno);

    ok(DeleteFileW(f1W), "expect wine_test_dir/f1 to exist\n");
    ok(DeleteFileW(f2W), "expect wine_test_dir/f2 to exist\n");
    ok(p_Remove_dir(wine_test_dirW), "expect wine_test_dir to exist\n");
    ok(SetCurrentDirectoryW(current_path), "SetCurrentDirectoryW failed\n");
}

START_TEST(msvcp140)
{
    if(!init()) return;
    test_thrd();
    test__Task_impl_base__IsNonBlockingThread();
    test_vbtable_size_exports();
    test_task_continuation_context();
    test__ContextCallback();
    test__TaskEventLogger();
    test_chore();
    test_to_byte();
    test_to_wide();
    test_File_size();
    test_Current_get();
    test_Current_set();
    test_Stat();
    test_dir_operation();
    test_Unlink();
    test_Temp_get();
    test_Rename();
    test_Last_write_time();
    test__Winerror_message();
    test__Winerror_map();
    test_Equivalent();
    FreeLibrary(msvcp);
}
