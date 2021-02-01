/*
 * Copyright 2014 Yifu Wang for ESRI
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
#include <stdarg.h>
#include <stdlib.h>
#include <wchar.h>
#include <stdio.h>
#include <float.h>
#include <math.h>
#include <fenv.h>
#include <limits.h>
#include <wctype.h>

#include <windef.h>
#include <winbase.h>
#include <winnls.h>
#include "wine/test.h"
#include <process.h>

#include <locale.h>

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

static ULONG_PTR (WINAPI *call_thiscall_func1)( void *func, void *this );
static ULONG_PTR (WINAPI *call_thiscall_func2)( void *func, void *this, const void *a );
static ULONG_PTR (WINAPI *call_thiscall_func3)( void *func,
        void *this, const void *a, const void *b );

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
    call_thiscall_func3 = (void *)thunk;
}

#define call_func1(func,_this) call_thiscall_func1(func,_this)
#define call_func2(func,_this,a) call_thiscall_func2(func,_this,(const void*)(a))
#define call_func3(func,_this,a,b) call_thiscall_func3(func,_this,(const void*)(a),(const void*)(b))

#else

#define init_thiscall_thunk()
#define call_func1(func,_this) func(_this)
#define call_func2(func,_this,a) func(_this,a)
#define call_func3(func,_this,a,b) func(_this,a,b)

#endif /* __i386__ */

#undef __thiscall
#ifdef __i386__
#define __thiscall __stdcall
#else
#define __thiscall __cdecl
#endif

typedef unsigned char MSVCRT_bool;

typedef struct cs_queue
{
    struct cs_queue *next;
    BOOL free;
    int unknown;
} cs_queue;

typedef struct
{
    ULONG_PTR unk_thread_id;
    cs_queue unk_active;
    void *unknown[2];
    cs_queue *head;
    void *tail;
} critical_section;

typedef struct
{
    critical_section *cs;
    void *unknown[4];
    int unknown2[2];
} critical_section_scoped_lock;

typedef struct {
    void *chain;
    critical_section lock;
} _Condition_variable;

struct MSVCRT_lconv
{
    char* decimal_point;
    char* thousands_sep;
    char* grouping;
    char* int_curr_symbol;
    char* currency_symbol;
    char* mon_decimal_point;
    char* mon_thousands_sep;
    char* mon_grouping;
    char* positive_sign;
    char* negative_sign;
    char int_frac_digits;
    char frac_digits;
    char p_cs_precedes;
    char p_sep_by_space;
    char n_cs_precedes;
    char n_sep_by_space;
    char p_sign_posn;
    char n_sign_posn;
    wchar_t* _W_decimal_point;
    wchar_t* _W_thousands_sep;
    wchar_t* _W_int_curr_symbol;
    wchar_t* _W_currency_symbol;
    wchar_t* _W_mon_decimal_point;
    wchar_t* _W_mon_thousands_sep;
    wchar_t* _W_positive_sign;
    wchar_t* _W_negative_sign;
};

typedef struct
{
    double r;
    double i;
} _Dcomplex;

typedef void (*vtable_ptr)(void);

typedef struct {
    const vtable_ptr *vtable;
} Context;

typedef struct {
    Context *ctx;
} _Context;

static char* (CDECL *p_setlocale)(int category, const char* locale);
static struct MSVCRT_lconv* (CDECL *p_localeconv)(void);
static size_t (CDECL *p_wcstombs_s)(size_t *ret, char* dest, size_t sz, const wchar_t* src, size_t max);
static int (CDECL *p__dsign)(double);
static int (CDECL *p__fdsign)(float);
static int (__cdecl *p__dpcomp)(double x, double y);
static wchar_t** (CDECL *p____lc_locale_name_func)(void);
static unsigned int (CDECL *p__GetConcurrency)(void);
static void* (CDECL *p__W_Gettnames)(void);
static void (CDECL *p_free)(void*);
static float (CDECL *p_strtof)(const char *, char **);
static int (CDECL *p__finite)(double);
static float (CDECL *p_wcstof)(const wchar_t*, wchar_t**);
static double (CDECL *p_remainder)(double, double);
static int* (CDECL *p_errno)(void);
static int (CDECL *p_fegetenv)(fenv_t*);
static int (CDECL *p_fesetenv)(const fenv_t*);
static int (CDECL *p_fegetround)(void);
static int (CDECL *p_fesetround)(int);
static int (CDECL *p__clearfp)(void);
static _locale_t (__cdecl *p_wcreate_locale)(int, const wchar_t *);
static void (__cdecl *p_free_locale)(_locale_t);
static unsigned short (__cdecl *p_wctype)(const char*);
static int (__cdecl *p_vsscanf)(const char*, const char *, __ms_va_list valist);
static _Dcomplex* (__cdecl *p__Cbuild)(_Dcomplex*, double, double);
static double (__cdecl *p_creal)(_Dcomplex);
static double (__cdecl *p_nexttoward)(double, double);
static float (__cdecl *p_nexttowardf)(float, double);
static double (__cdecl *p_nexttowardl)(double, double);
static wctrans_t (__cdecl *p_wctrans)(const char*);
static wint_t (__cdecl *p_towctrans)(wint_t, wctrans_t);

/* make sure we use the correct errno */
#undef errno
#define errno (*p_errno())

static critical_section* (__thiscall *p_critical_section_ctor)(critical_section*);
static void (__thiscall *p_critical_section_dtor)(critical_section*);
static void (__thiscall *p_critical_section_lock)(critical_section*);
static void (__thiscall *p_critical_section_unlock)(critical_section*);
static critical_section* (__thiscall *p_critical_section_native_handle)(critical_section*);
static MSVCRT_bool (__thiscall *p_critical_section_try_lock)(critical_section*);
static MSVCRT_bool (__thiscall *p_critical_section_try_lock_for)(critical_section*, unsigned int);
static critical_section_scoped_lock* (__thiscall *p_critical_section_scoped_lock_ctor)
    (critical_section_scoped_lock*, critical_section *);
static void (__thiscall *p_critical_section_scoped_lock_dtor)(critical_section_scoped_lock*);

static _Condition_variable* (__thiscall *p__Condition_variable_ctor)(_Condition_variable*);
static void (__thiscall *p__Condition_variable_dtor)(_Condition_variable*);
static void (__thiscall *p__Condition_variable_wait)(_Condition_variable*, critical_section*);
static MSVCRT_bool (__thiscall *p__Condition_variable_wait_for)(_Condition_variable*, critical_section*, unsigned int);
static void (__thiscall *p__Condition_variable_notify_one)(_Condition_variable*);
static void (__thiscall *p__Condition_variable_notify_all)(_Condition_variable*);

static Context* (__cdecl *p_Context_CurrentContext)(void);
static _Context* (__cdecl *p__Context__CurrentContext)(_Context*);

#define SETNOFAIL(x,y) x = (void*)GetProcAddress(module,y)
#define SET(x,y) do { SETNOFAIL(x,y); ok(x != NULL, "Export '%s' not found\n", y); } while(0)

static BOOL init(void)
{
    HMODULE module;

    module = LoadLibraryA("msvcr120.dll");
    if (!module)
    {
        win_skip("msvcr120.dll not installed\n");
        return FALSE;
    }

    p_setlocale = (void*)GetProcAddress(module, "setlocale");
    p_localeconv = (void*)GetProcAddress(module, "localeconv");
    p_wcstombs_s = (void*)GetProcAddress(module, "wcstombs_s");
    p__dsign = (void*)GetProcAddress(module, "_dsign");
    p__fdsign = (void*)GetProcAddress(module, "_fdsign");
    p__dpcomp = (void*)GetProcAddress(module, "_dpcomp");
    p____lc_locale_name_func = (void*)GetProcAddress(module, "___lc_locale_name_func");
    p__GetConcurrency = (void*)GetProcAddress(module,"?_GetConcurrency@details@Concurrency@@YAIXZ");
    p__W_Gettnames = (void*)GetProcAddress(module, "_W_Gettnames");
    p_free = (void*)GetProcAddress(module, "free");
    p_strtof = (void*)GetProcAddress(module, "strtof");
    p__finite = (void*)GetProcAddress(module, "_finite");
    p_wcstof = (void*)GetProcAddress(module, "wcstof");
    p_remainder = (void*)GetProcAddress(module, "remainder");
    p_errno = (void*)GetProcAddress(module, "_errno");
    p_wcreate_locale = (void*)GetProcAddress(module, "_wcreate_locale");
    p_free_locale = (void*)GetProcAddress(module, "_free_locale");
    SET(p_wctype, "wctype");
    SET(p_fegetenv, "fegetenv");
    SET(p_fesetenv, "fesetenv");
    SET(p_fegetround, "fegetround");
    SET(p_fesetround, "fesetround");

    SET(p__clearfp, "_clearfp");
    SET(p_vsscanf, "vsscanf");
    SET(p__Cbuild, "_Cbuild");
    SET(p_creal, "creal");
    SET(p_nexttoward, "nexttoward");
    SET(p_nexttowardf, "nexttowardf");
    SET(p_nexttowardl, "nexttowardl");
    SET(p_wctrans, "wctrans");
    SET(p_towctrans, "towctrans");
    SET(p__Context__CurrentContext, "?_CurrentContext@_Context@details@Concurrency@@SA?AV123@XZ");
    if(sizeof(void*) == 8) { /* 64-bit initialization */
        SET(p_critical_section_ctor,
                "??0critical_section@Concurrency@@QEAA@XZ");
        SET(p_critical_section_dtor,
                "??1critical_section@Concurrency@@QEAA@XZ");
        SET(p_critical_section_lock,
                "?lock@critical_section@Concurrency@@QEAAXXZ");
        SET(p_critical_section_unlock,
                "?unlock@critical_section@Concurrency@@QEAAXXZ");
        SET(p_critical_section_native_handle,
                "?native_handle@critical_section@Concurrency@@QEAAAEAV12@XZ");
        SET(p_critical_section_try_lock,
                "?try_lock@critical_section@Concurrency@@QEAA_NXZ");
        SET(p_critical_section_try_lock_for,
                "?try_lock_for@critical_section@Concurrency@@QEAA_NI@Z");
        SET(p_critical_section_scoped_lock_ctor,
                "??0scoped_lock@critical_section@Concurrency@@QEAA@AEAV12@@Z");
        SET(p_critical_section_scoped_lock_dtor,
                "??1scoped_lock@critical_section@Concurrency@@QEAA@XZ");
        SET(p__Condition_variable_ctor,
                "??0_Condition_variable@details@Concurrency@@QEAA@XZ");
        SET(p__Condition_variable_dtor,
                "??1_Condition_variable@details@Concurrency@@QEAA@XZ");
        SET(p__Condition_variable_wait,
                "?wait@_Condition_variable@details@Concurrency@@QEAAXAEAVcritical_section@3@@Z");
        SET(p__Condition_variable_wait_for,
                "?wait_for@_Condition_variable@details@Concurrency@@QEAA_NAEAVcritical_section@3@I@Z");
        SET(p__Condition_variable_notify_one,
                "?notify_one@_Condition_variable@details@Concurrency@@QEAAXXZ");
        SET(p__Condition_variable_notify_all,
                "?notify_all@_Condition_variable@details@Concurrency@@QEAAXXZ");
        SET(p_Context_CurrentContext,
                "?CurrentContext@Context@Concurrency@@SAPEAV12@XZ");
    } else {
#ifdef __arm__
        SET(p_critical_section_ctor,
                "??0critical_section@Concurrency@@QAA@XZ");
        SET(p_critical_section_dtor,
                "??1critical_section@Concurrency@@QAA@XZ");
        SET(p_critical_section_lock,
                "?lock@critical_section@Concurrency@@QAAXXZ");
        SET(p_critical_section_unlock,
                "?unlock@critical_section@Concurrency@@QAAXXZ");
        SET(p_critical_section_native_handle,
                "?native_handle@critical_section@Concurrency@@QAAAAV12@XZ");
        SET(p_critical_section_try_lock,
                "?try_lock@critical_section@Concurrency@@QAA_NXZ");
        SET(p_critical_section_try_lock_for,
                "?try_lock_for@critical_section@Concurrency@@QAA_NI@Z");
        SET(p_critical_section_scoped_lock_ctor,
                "??0scoped_lock@critical_section@Concurrency@@QAA@AAV12@@Z");
        SET(p_critical_section_scoped_lock_dtor,
                "??1scoped_lock@critical_section@Concurrency@@QAA@XZ");
        SET(p__Condition_variable_ctor,
                "??0_Condition_variable@details@Concurrency@@QAA@XZ");
        SET(p__Condition_variable_dtor,
                "??1_Condition_variable@details@Concurrency@@QAA@XZ");
        SET(p__Condition_variable_wait,
                "?wait@_Condition_variable@details@Concurrency@@QAAXAAVcritical_section@3@@Z");
        SET(p__Condition_variable_wait_for,
                "?wait_for@_Condition_variable@details@Concurrency@@QAA_NAAVcritical_section@3@I@Z");
        SET(p__Condition_variable_notify_one,
                "?notify_one@_Condition_variable@details@Concurrency@@QAAXXZ");
        SET(p__Condition_variable_notify_all,
                "?notify_all@_Condition_variable@details@Concurrency@@QAAXXZ");
#else
        SET(p_critical_section_ctor,
                "??0critical_section@Concurrency@@QAE@XZ");
        SET(p_critical_section_dtor,
                "??1critical_section@Concurrency@@QAE@XZ");
        SET(p_critical_section_lock,
                "?lock@critical_section@Concurrency@@QAEXXZ");
        SET(p_critical_section_unlock,
                "?unlock@critical_section@Concurrency@@QAEXXZ");
        SET(p_critical_section_native_handle,
                "?native_handle@critical_section@Concurrency@@QAEAAV12@XZ");
        SET(p_critical_section_try_lock,
                "?try_lock@critical_section@Concurrency@@QAE_NXZ");
        SET(p_critical_section_try_lock_for,
                "?try_lock_for@critical_section@Concurrency@@QAE_NI@Z");
        SET(p_critical_section_scoped_lock_ctor,
                "??0scoped_lock@critical_section@Concurrency@@QAE@AAV12@@Z");
        SET(p_critical_section_scoped_lock_dtor,
                "??1scoped_lock@critical_section@Concurrency@@QAE@XZ");
        SET(p__Condition_variable_ctor,
                "??0_Condition_variable@details@Concurrency@@QAE@XZ");
        SET(p__Condition_variable_dtor,
                "??1_Condition_variable@details@Concurrency@@QAE@XZ");
        SET(p__Condition_variable_wait,
                "?wait@_Condition_variable@details@Concurrency@@QAEXAAVcritical_section@3@@Z");
        SET(p__Condition_variable_wait_for,
                "?wait_for@_Condition_variable@details@Concurrency@@QAE_NAAVcritical_section@3@I@Z");
        SET(p__Condition_variable_notify_one,
                "?notify_one@_Condition_variable@details@Concurrency@@QAEXXZ");
        SET(p__Condition_variable_notify_all,
                "?notify_all@_Condition_variable@details@Concurrency@@QAEXXZ");
#endif
        SET(p_Context_CurrentContext,
                "?CurrentContext@Context@Concurrency@@SAPAV12@XZ");
    }

    init_thiscall_thunk();
    return TRUE;
}

static void test_lconv_helper(const char *locstr)
{
    struct MSVCRT_lconv *lconv;
    char mbs[256];
    size_t i;

    if(!p_setlocale(LC_ALL, locstr))
    {
        win_skip("locale %s not available\n", locstr);
        return;
    }

    lconv = p_localeconv();

    /* If multi-byte version available, asserts that wide char version also available.
     * If wide char version can be converted to a multi-byte string , asserts that the
     * conversion result is the same as the multi-byte version.
     */
    if(strlen(lconv->decimal_point) > 0)
        ok(wcslen(lconv->_W_decimal_point) > 0, "%s: decimal_point\n", locstr);
    if(p_wcstombs_s(&i, mbs, 256, lconv->_W_decimal_point, 256) == 0)
        ok(strcmp(mbs, lconv->decimal_point) == 0, "%s: decimal_point\n", locstr);

    if(strlen(lconv->thousands_sep) > 0)
        ok(wcslen(lconv->_W_thousands_sep) > 0, "%s: thousands_sep\n", locstr);
    if(p_wcstombs_s(&i, mbs, 256, lconv->_W_thousands_sep, 256) == 0)
        ok(strcmp(mbs, lconv->thousands_sep) == 0, "%s: thousands_sep\n", locstr);

    if(strlen(lconv->int_curr_symbol) > 0)
        ok(wcslen(lconv->_W_int_curr_symbol) > 0, "%s: int_curr_symbol\n", locstr);
    if(p_wcstombs_s(&i, mbs, 256, lconv->_W_int_curr_symbol, 256) == 0)
        ok(strcmp(mbs, lconv->int_curr_symbol) == 0, "%s: int_curr_symbol\n", locstr);

    if(strlen(lconv->currency_symbol) > 0)
        ok(wcslen(lconv->_W_currency_symbol) > 0, "%s: currency_symbol\n", locstr);
    if(p_wcstombs_s(&i, mbs, 256, lconv->_W_currency_symbol, 256) == 0)
        ok(strcmp(mbs, lconv->currency_symbol) == 0, "%s: currency_symbol\n", locstr);

    if(strlen(lconv->mon_decimal_point) > 0)
        ok(wcslen(lconv->_W_mon_decimal_point) > 0, "%s: decimal_point\n", locstr);
    if(p_wcstombs_s(&i, mbs, 256, lconv->_W_mon_decimal_point, 256) == 0)
        ok(strcmp(mbs, lconv->mon_decimal_point) == 0, "%s: decimal_point\n", locstr);

    if(strlen(lconv->positive_sign) > 0)
        ok(wcslen(lconv->_W_positive_sign) > 0, "%s: positive_sign\n", locstr);
    if(p_wcstombs_s(&i, mbs, 256, lconv->_W_positive_sign, 256) == 0)
        ok(strcmp(mbs, lconv->positive_sign) == 0, "%s: positive_sign\n", locstr);

    if(strlen(lconv->negative_sign) > 0)
        ok(wcslen(lconv->_W_negative_sign) > 0, "%s: negative_sign\n", locstr);
    if(p_wcstombs_s(&i, mbs, 256, lconv->_W_negative_sign, 256) == 0)
        ok(strcmp(mbs, lconv->negative_sign) == 0, "%s: negative_sign\n", locstr);
}

static void test_lconv(void)
{
    int i;
    const char *locstrs[] =
    {
        "American", "Belgian", "Chinese",
        "Dutch", "English", "French",
        "German", "Hungarian", "Icelandic",
        "Japanese", "Korean", "Spanish"
    };

    for(i = 0; i < ARRAY_SIZE(locstrs); i ++)
        test_lconv_helper(locstrs[i]);
}

static void test__dsign(void)
{
    int ret;

    ret = p__dsign(1);
    ok(ret == 0, "p_dsign(1) = %x\n", ret);
    ret = p__dsign(0);
    ok(ret == 0, "p_dsign(0) = %x\n", ret);
    ret = p__dsign(-1);
    ok(ret == 0x8000, "p_dsign(-1) = %x\n", ret);

    ret = p__fdsign(1);
    ok(ret == 0, "p_fdsign(1) = %x\n", ret);
    ret = p__fdsign(0);
    ok(ret == 0, "p_fdsign(0) = %x\n", ret);
    ret = p__fdsign(-1);
    ok(ret == 0x8000, "p_fdsign(-1) = %x\n", ret);
}

static void test__dpcomp(void)
{
    struct {
        double x, y;
        int ret;
    } tests[] = {
        {0, 0, 2}, {1, 1, 2}, {-1, -1, 2},
        {-2, -1, 1}, {-1, 1, 1}, {1, 2, 1},
        {1, -1, 4}, {2, 1, 4}, {-1, -2, 4},
        {NAN, NAN, 0}, {NAN, 1, 0}, {1, NAN, 0},
        {INFINITY, INFINITY, 2}, {-1, INFINITY, 1}, {1, INFINITY, 1},
    };
    int i, ret;

    for(i=0; i<ARRAY_SIZE(tests); i++) {
        ret = p__dpcomp(tests[i].x, tests[i].y);
        ok(ret == tests[i].ret, "%d) dpcomp(%f, %f) = %x\n", i, tests[i].x, tests[i].y, ret);
    }
}

static void test____lc_locale_name_func(void)
{
    struct {
        const char *locale;
        const WCHAR name[10];
        const WCHAR broken_name[10];
    } tests[] = {
        { "American",  {'e','n',0}, {'e','n','-','U','S',0} },
        { "Belgian",   {'n','l','-','B','E',0} },
        { "Chinese",   {'z','h',0}, {'z','h','-','C','N',0} },
        { "Dutch",     {'n','l',0}, {'n','l','-','N','L',0} },
        { "English",   {'e','n',0}, {'e','n','-','U','S',0} },
        { "French",    {'f','r',0}, {'f','r','-','F','R',0} },
        { "German",    {'d','e',0}, {'d','e','-','D','E',0} },
        { "Hungarian", {'h','u',0}, {'h','u','-','H','U',0} },
        { "Icelandic", {'i','s',0}, {'i','s','-','I','S',0} },
        { "Japanese",  {'j','a',0}, {'j','a','-','J','P',0} },
        { "Korean",    {'k','o',0}, {'k','o','-','K','R',0} }
    };
    int i, j;
    wchar_t **lc_names;

    for(i=0; i<ARRAY_SIZE(tests); i++) {
        if(!p_setlocale(LC_ALL, tests[i].locale))
            continue;

        lc_names = p____lc_locale_name_func();
        ok(lc_names[0] == NULL, "%d - lc_names[0] = %s\n", i, wine_dbgstr_w(lc_names[0]));
        ok(!lstrcmpW(lc_names[1], tests[i].name) || broken(!lstrcmpW(lc_names[1], tests[i].broken_name)),
           "%d - lc_names[1] = %s\n", i, wine_dbgstr_w(lc_names[1]));

        for(j=LC_MIN+2; j<=LC_MAX; j++) {
            ok(!lstrcmpW(lc_names[1], lc_names[j]), "%d - lc_names[%d] = %s, expected %s\n",
                    i, j, wine_dbgstr_w(lc_names[j]), wine_dbgstr_w(lc_names[1]));
        }
    }

    p_setlocale(LC_ALL, "C");
    lc_names = p____lc_locale_name_func();
    ok(!lc_names[1], "___lc_locale_name_func()[1] = %s\n", wine_dbgstr_w(lc_names[1]));
}

static void test__GetConcurrency(void)
{
    SYSTEM_INFO si;
    unsigned int c;

    GetSystemInfo(&si);
    c = (*p__GetConcurrency)();
    ok(c == si.dwNumberOfProcessors, "expected %u, got %u\n", si.dwNumberOfProcessors, c);
}

static void test__W_Gettnames(void)
{
    static const char *str[] = {
        "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat",
        "Sunday", "Monday", "Tuesday", "Wednesday",
        "Thursday", "Friday", "Saturday",
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
        "January", "February", "March", "April", "May", "June", "July",
        "August", "September", "October", "November", "December",
        "AM", "PM", "M/d/yyyy"
    };

    struct {
        char *str[43];
        int  unk[2];
        wchar_t *wstr[43];
        wchar_t *locname;
        char data[1];
    } *ret;
    int i, size;
    WCHAR buf[64];

    if(!p_setlocale(LC_ALL, "english"))
        return;

    ret = p__W_Gettnames();
    size = ret->str[0]-(char*)ret;
    if(sizeof(void*) == 8)
        ok(size==0x2c0, "structure size: %x\n", size);
    else
        ok(size==0x164, "structure size: %x\n", size);

    for(i=0; i<ARRAY_SIZE(str); i++) {
        ok(!strcmp(ret->str[i], str[i]), "ret->str[%d] = %s, expected %s\n",
                i, ret->str[i], str[i]);

        MultiByteToWideChar(CP_ACP, 0, str[i], strlen(str[i])+1, buf, ARRAY_SIZE(buf));
        ok(!lstrcmpW(ret->wstr[i], buf), "ret->wstr[%d] = %s, expected %s\n",
                i, wine_dbgstr_w(ret->wstr[i]), wine_dbgstr_w(buf));
    }
    p_free(ret);

    p_setlocale(LC_ALL, "C");
}

static void test__strtof(void)
{
    const char float1[] = "12.0";
    const char float2[] = "3.402823466e+38";          /* FLT_MAX */
    const char float3[] = "-3.402823466e+38";
    const char float4[] = "1.7976931348623158e+308";  /* DBL_MAX */

    char *end;
    float f;

    f = p_strtof(float1, &end);
    ok(f == 12.0, "f = %lf\n", f);
    ok(end == float1+4, "incorrect end (%d)\n", (int)(end-float1));

    f = p_strtof(float2, &end);
    ok(f == FLT_MAX, "f = %lf\n", f);
    ok(end == float2+15, "incorrect end (%d)\n", (int)(end-float2));

    f = p_strtof(float3, &end);
    ok(f == -FLT_MAX, "f = %lf\n", f);
    ok(end == float3+16, "incorrect end (%d)\n", (int)(end-float3));

    f = p_strtof(float4, &end);
    ok(!p__finite(f), "f = %lf\n", f);
    ok(end == float4+23, "incorrect end (%d)\n", (int)(end-float4));

    f = p_strtof("inf", NULL);
    ok(f == 0, "f = %lf\n", f);

    f = p_strtof("INF", NULL);
    ok(f == 0, "f = %lf\n", f);

    f = p_strtof("1.#inf", NULL);
    ok(f == 1, "f = %lf\n", f);

    f = p_strtof("INFINITY", NULL);
    ok(f == 0, "f = %lf\n", f);

    f = p_strtof("0x12", NULL);
    ok(f == 0, "f = %lf\n", f);

    f = p_wcstof(L"12.0", NULL);
    ok(f == 12.0, "f = %lf\n", f);

    f = p_wcstof(L"\x0662\x0663", NULL);
    ok(f == 0, "f = %lf\n", f);

    if(!p_setlocale(LC_ALL, "Arabic")) {
        win_skip("Arabic locale not available\n");
        return;
    }

    f = p_wcstof(L"12.0", NULL);
    ok(f == 12.0, "f = %lf\n", f);

    f = p_wcstof(L"\x0662\x0663", NULL);
    ok(f == 0, "f = %lf\n", f);

    p_setlocale(LC_ALL, "C");
}

static void test_remainder(void)
{
    struct {
        double  x, y, r;
        errno_t e;
    } tests[] = {
        { 3.0,      2.0,       -1.0,    -1   },
        { 1.0,      1.0,        0.0,    -1   },
        { INFINITY, 0.0,        NAN,    EDOM },
        { INFINITY, 42.0,       NAN,    EDOM },
        { NAN,      0.0,        NAN,    EDOM },
        { NAN,      42.0,       NAN,    EDOM },
        { 0.0,      INFINITY,   0.0,    -1   },
        { 42.0,     INFINITY,   42.0,   -1   },
        { 0.0,      NAN,        NAN,    EDOM },
        { 42.0,     NAN,        NAN,    EDOM },
        { 1.0,      0.0,        NAN,    EDOM },
        { INFINITY, INFINITY,   NAN,    EDOM },
    };
    errno_t e;
    double r;
    int i;

    if(sizeof(void*) != 8) /* errno handling slightly different on 32-bit */
        return;

    for(i=0; i<ARRAY_SIZE(tests); i++) {
        errno = -1;
        r = p_remainder(tests[i].x, tests[i].y);
        e = errno;

        ok(tests[i].e == e, "expected errno %i, but got %i\n", tests[i].e, e);
        if(_isnan(tests[i].r))
            ok(_isnan(r), "expected NAN, but got %f\n", r);
        else
            ok(tests[i].r == r, "expected result %f, but got %f\n", tests[i].r, r);
    }
}

static int enter_flag;
static critical_section cs;
static unsigned __stdcall test_critical_section_lock(void *arg)
{
    critical_section *native_handle;
    native_handle = (critical_section*)call_func1(p_critical_section_native_handle, &cs);
    ok(native_handle == &cs, "native_handle = %p\n", native_handle);
    call_func1(p_critical_section_lock, &cs);
    ok(enter_flag == 1, "enter_flag = %d\n", enter_flag);
    call_func1(p_critical_section_unlock, &cs);
    return 0;
}

static unsigned __stdcall test_critical_section_try_lock(void *arg)
{
    ok(!(MSVCRT_bool)call_func1(p_critical_section_try_lock, &cs),
            "critical_section_try_lock succeeded\n");
    return 0;
}

static unsigned __stdcall test_critical_section_try_lock_for(void *arg)
{
    ok((MSVCRT_bool)call_func2(p_critical_section_try_lock_for, &cs, 5000),
            "critical_section_try_lock_for failed\n");
    ok(enter_flag == 1, "enter_flag = %d\n", enter_flag);
    call_func1(p_critical_section_unlock, &cs);
    return 0;
}

static unsigned __stdcall test_critical_section_scoped_lock(void *arg)
{
    critical_section_scoped_lock counter_scope_lock;

    call_func2(p_critical_section_scoped_lock_ctor, &counter_scope_lock, &cs);
    ok(enter_flag == 1, "enter_flag = %d\n", enter_flag);
    call_func1(p_critical_section_scoped_lock_dtor, &counter_scope_lock);
    return 0;
}

static void test_critical_section(void)
{
    HANDLE thread;
    DWORD ret;

    enter_flag = 0;
    call_func1(p_critical_section_ctor, &cs);
    call_func1(p_critical_section_lock, &cs);
    thread = (HANDLE)_beginthreadex(NULL, 0, test_critical_section_lock, NULL, 0, NULL);
    ok(thread != INVALID_HANDLE_VALUE, "_beginthread failed (%d)\n", errno);
    ret = WaitForSingleObject(thread, 100);
    ok(ret == WAIT_TIMEOUT, "WaitForSingleObject returned  %d\n", ret);
    enter_flag = 1;
    call_func1(p_critical_section_unlock, &cs);
    ret = WaitForSingleObject(thread, INFINITE);
    ok(ret == WAIT_OBJECT_0, "WaitForSingleObject returned %d\n", ret);
    ret = CloseHandle(thread);
    ok(ret, "CloseHandle failed\n");

    ok((MSVCRT_bool)call_func1(p_critical_section_try_lock, &cs),
            "critical_section_try_lock failed\n");
    thread = (HANDLE)_beginthreadex(NULL, 0, test_critical_section_try_lock, NULL, 0, NULL);
    ok(thread != INVALID_HANDLE_VALUE, "_beginthread failed (%d)\n", errno);
    ret = WaitForSingleObject(thread, INFINITE);
    ok(ret == WAIT_OBJECT_0, "WaitForSingleObject returned %d\n", ret);
    ret = CloseHandle(thread);
    ok(ret, "CloseHandle failed\n");
    call_func1(p_critical_section_unlock, &cs);

    enter_flag = 0;
    ok((MSVCRT_bool)call_func2(p_critical_section_try_lock_for, &cs, 50),
            "critical_section_try_lock_for failed\n");
    thread = (HANDLE)_beginthreadex(NULL, 0, test_critical_section_try_lock, NULL, 0, NULL);
    ok(thread != INVALID_HANDLE_VALUE, "_beginthread failed (%d)\n", errno);
    ret = WaitForSingleObject(thread, INFINITE);
    ok(ret == WAIT_OBJECT_0, "WaitForSingleObject returned %d\n", ret);
    ret = CloseHandle(thread);
    ok(ret, "CloseHandle failed\n");
    thread = (HANDLE)_beginthreadex(NULL, 0, test_critical_section_try_lock_for, NULL, 0, NULL);
    ok(thread != INVALID_HANDLE_VALUE, "_beginthread failed (%d)\n", errno);
    enter_flag = 1;
    Sleep(10);
    call_func1(p_critical_section_unlock, &cs);
    ret = WaitForSingleObject(thread, INFINITE);
    ok(ret == WAIT_OBJECT_0, "WaitForSingleObject returned %d\n", ret);
    ret = CloseHandle(thread);
    ok(ret, "CloseHandle failed\n");

    enter_flag = 0;
    call_func1(p_critical_section_lock, &cs);
    thread = (HANDLE)_beginthreadex(NULL, 0, test_critical_section_scoped_lock, NULL, 0, NULL);
    ok(thread != INVALID_HANDLE_VALUE, "_beginthread failed (%d)\n", errno);
    ret = WaitForSingleObject(thread, 100);
    ok(ret == WAIT_TIMEOUT, "WaitForSingleObject returned  %d\n", ret);
    enter_flag = 1;
    call_func1(p_critical_section_unlock, &cs);
    ret = WaitForSingleObject(thread, INFINITE);
    ok(ret == WAIT_OBJECT_0, "WaitForSingleObject returned %d\n", ret);
    ret = CloseHandle(thread);
    ok(ret, "CloseHandle failed\n");
    call_func1(p_critical_section_dtor, &cs);
}

static void test_feenv(void)
{
    fenv_t env, env2;
    int ret;

    p__clearfp();

    ret = p_fegetenv(&env);
    ok(!ret, "fegetenv returned %x\n", ret);
    p_fesetround(FE_UPWARD);
    ok(env._Fe_ctl == (_EM_INEXACT|_EM_UNDERFLOW|_EM_OVERFLOW|_EM_ZERODIVIDE|_EM_INVALID),
            "env._Fe_ctl = %lx\n", env._Fe_ctl);
    ok(!env._Fe_stat, "env._Fe_stat = %lx\n", env._Fe_stat);
    ret = p_fegetenv(&env2);
    ok(!ret, "fegetenv returned %x\n", ret);
    ok(env2._Fe_ctl == (_EM_INEXACT|_EM_UNDERFLOW|_EM_OVERFLOW|_EM_ZERODIVIDE|_EM_INVALID | FE_UPWARD),
            "env2._Fe_ctl = %lx\n", env2._Fe_ctl);
    ret = p_fesetenv(&env);
    ok(!ret, "fesetenv returned %x\n", ret);
    ret = p_fegetround();
    ok(ret == FE_TONEAREST, "Got unexpected round mode %#x.\n", ret);
}

static void test__wcreate_locale(void)
{
    _locale_t lcl;
    errno_t e;

    /* simple success */
    errno = -1;
    lcl = p_wcreate_locale(LC_ALL, L"C");
    e = errno;
    ok(!!lcl, "expected success, but got NULL\n");
    ok(errno == -1, "expected errno -1, but got %i\n", e);
    p_free_locale(lcl);

    errno = -1;
    lcl = p_wcreate_locale(LC_ALL, L"");
    e = errno;
    ok(!!lcl, "expected success, but got NULL\n");
    ok(errno == -1, "expected errno -1, but got %i\n", e);
    p_free_locale(lcl);

    /* bogus category */
    errno = -1;
    lcl = p_wcreate_locale(-1, L"C");
    e = errno;
    ok(!lcl, "expected failure, but got %p\n", lcl);
    ok(errno == -1, "expected errno -1, but got %i\n", e);

    /* bogus names */
    errno = -1;
    lcl = p_wcreate_locale(LC_ALL, L"bogus");
    e = errno;
    ok(!lcl, "expected failure, but got %p\n", lcl);
    ok(errno == -1, "expected errno -1, but got %i\n", e);

    errno = -1;
    lcl = p_wcreate_locale(LC_ALL, NULL);
    e = errno;
    ok(!lcl, "expected failure, but got %p\n", lcl);
    ok(errno == -1, "expected errno -1, but got %i\n", e);
}

struct wait_thread_arg
{
    critical_section *cs;
    _Condition_variable *cv;
    HANDLE thread_initialized;
};

static DWORD WINAPI condition_variable_wait_thread(void *varg)
{
    struct wait_thread_arg *arg = varg;

    call_func1(p_critical_section_lock, arg->cs);
    SetEvent(arg->thread_initialized);
    call_func2(p__Condition_variable_wait, arg->cv, arg->cs);
    call_func1(p_critical_section_unlock, arg->cs);
    return 0;
}

static void test__Condition_variable(void)
{
    critical_section cs;
    _Condition_variable cv;
    HANDLE thread_initialized = CreateEventW(NULL, FALSE, FALSE, NULL);
    struct wait_thread_arg wait_thread_arg = { &cs, &cv, thread_initialized };
    HANDLE threads[2];
    DWORD ret;
    MSVCRT_bool b;

    ok(thread_initialized != NULL, "CreateEvent failed\n");

    call_func1(p_critical_section_ctor, &cs);
    call_func1(p__Condition_variable_ctor, &cv);

    call_func1(p__Condition_variable_notify_one, &cv);
    call_func1(p__Condition_variable_notify_all, &cv);

    threads[0] = CreateThread(0, 0, condition_variable_wait_thread,
            &wait_thread_arg, 0, 0);
    ok(threads[0] != NULL, "CreateThread failed\n");
    WaitForSingleObject(thread_initialized, INFINITE);
    call_func1(p_critical_section_lock, &cs);
    call_func1(p_critical_section_unlock, &cs);

    threads[1] = CreateThread(0, 0, condition_variable_wait_thread,
            &wait_thread_arg, 0, 0);
    ok(threads[1] != NULL, "CreateThread failed\n");
    WaitForSingleObject(thread_initialized, INFINITE);
    call_func1(p_critical_section_lock, &cs);
    call_func1(p_critical_section_unlock, &cs);

    call_func1(p__Condition_variable_notify_one, &cv);
    ret = WaitForSingleObject(threads[1], 500);
    ok(ret == WAIT_OBJECT_0, "WaitForSingleObject returned %d\n", ret);
    call_func1(p__Condition_variable_notify_one, &cv);
    ret = WaitForSingleObject(threads[0], 500);
    ok(ret == WAIT_OBJECT_0, "WaitForSingleObject returned %d\n", ret);

    CloseHandle(threads[0]);
    CloseHandle(threads[1]);

    call_func1(p_critical_section_lock, &cs);
    b = call_func3(p__Condition_variable_wait_for, &cv, &cs, 1);
    ok(!b, "_Condition_variable_wait_for returned TRUE\n");
    call_func1(p_critical_section_unlock, &cs);

    call_func1(p_critical_section_dtor, &cs);
    call_func1(p__Condition_variable_dtor, &cv);

    CloseHandle(thread_initialized);
}

static void test_wctype(void)
{
    static const struct {
        const char *name;
        unsigned short mask;
    } properties[] = {
        { "alnum",  0x107 },
        { "alpha",  0x103 },
        { "cntrl",  0x020 },
        { "digit",  0x004 },
        { "graph",  0x117 },
        { "lower",  0x002 },
        { "print",  0x157 },
        { "punct",  0x010 },
        { "space",  0x008 },
        { "upper",  0x001 },
        { "xdigit", 0x080 },
        { "ALNUM",  0x000 },
        { "Alnum",  0x000 },
        { "",  0x000 }
    };
    int i, ret;

    for(i=0; i<ARRAY_SIZE(properties); i++) {
        ret = p_wctype(properties[i].name);
        ok(properties[i].mask == ret, "%d - Expected %x, got %x\n", i, properties[i].mask, ret);
    }
}

static int WINAPIV _vsscanf_wrapper(const char *buffer, const char *format, ...)
{
    int ret;
    __ms_va_list valist;
    __ms_va_start(valist, format);
    ret = p_vsscanf(buffer, format, valist);
    __ms_va_end(valist);
    return ret;
}

static void test_vsscanf(void)
{
    static const char *fmt = "%d";
    char buff[16];
    int ret, v;

    v = 0;
    strcpy(buff, "10");
    ret = _vsscanf_wrapper(buff, fmt, &v);
    ok(ret == 1, "Unexpected ret %d.\n", ret);
    ok(v == 10, "got %d.\n", v);
}

static void test__Cbuild(void)
{
    _Dcomplex c;
    double d;

    memset(&c, 0, sizeof(c));
    p__Cbuild(&c, 1.0, 2.0);
    ok(c.r == 1.0, "c.r = %lf\n", c.r);
    ok(c.i == 2.0, "c.i = %lf\n", c.i);
    d = p_creal(c);
    ok(d == 1.0, "creal returned %lf\n", d);

    p__Cbuild(&c, 3.0, NAN);
    ok(c.r == 3.0, "c.r = %lf\n", c.r);
    ok(_isnan(c.i), "c.i = %lf\n", c.i);
    d = p_creal(c);
    ok(d == 3.0, "creal returned %lf\n", d);
}

static void test_nexttoward(void)
{
    errno_t e;
    double d;
    float f;
    int i;

    struct
    {
        double source;
        double dir;
        float f;
        double d;
    }
    tests[] =
    {
        {0.0,                      0.0,                      0.0f,        0.0},
        {0.0,                      1.0,                      1.0e-45f,    5.0e-324},
        {0.0,                     -1.0,                     -1.0e-45f,   -5.0e-324},
        {2.2250738585072009e-308,  0.0,                      0.0f,        2.2250738585072004e-308},
        {2.2250738585072009e-308,  2.2250738585072010e-308,  1.0e-45f,    2.2250738585072009e-308},
        {2.2250738585072009e-308,  1.0,                      1.0e-45f,    2.2250738585072014e-308},
        {2.2250738585072014e-308,  0.0,                      0.0f,        2.2250738585072009e-308},
        {2.2250738585072014e-308,  2.2250738585072014e-308,  1.0e-45f,    2.2250738585072014e-308},
        {2.2250738585072014e-308,  1.0,                      1.0e-45f,    2.2250738585072019e-308},
        {1.0,                      2.0,                      1.00000012f, 1.0000000000000002},
        {1.0,                      0.0,                      0.99999994f, 0.9999999999999999},
        {1.0,                      1.0,                      1.0f,        1.0},
        {0.0,                      INFINITY,                 1.0e-45f,    5.0e-324},
        {FLT_MAX,                  INFINITY,                 INFINITY,    3.402823466385289e+038},
        {DBL_MAX,                  INFINITY,                 INFINITY,    INFINITY},
        {INFINITY,                 INFINITY,                 INFINITY,    INFINITY},
        {INFINITY,                 0,                        FLT_MAX,     DBL_MAX},
    };

    for (i = 0; i < ARRAY_SIZE(tests); ++i)
    {
        f = p_nexttowardf(tests[i].source, tests[i].dir);
        ok(f == tests[i].f, "Test %d: expected %0.8ef, got %0.8ef.\n", i, tests[i].f, f);

        errno = -1;
        d = p_nexttoward(tests[i].source, tests[i].dir);
        e = errno;
        ok(d == tests[i].d, "Test %d: expected %0.16e, got %0.16e.\n", i, tests[i].d, d);
        if (!isnormal(d) && !isinf(tests[i].source))
            ok(e == ERANGE, "Test %d: expected ERANGE, got %d.\n", i, e);
        else
            ok(e == -1, "Test %d: expected no error, got %d.\n", i, e);

        d = p_nexttowardl(tests[i].source, tests[i].dir);
        ok(d == tests[i].d, "Test %d: expected %0.16e, got %0.16e.\n", i, tests[i].d, d);
    }

    errno = -1;
    d = p_nexttoward(NAN, 0);
    e = errno;
    ok(_isnan(d), "Expected NAN, got %0.16e.\n", d);
    ok(e == -1, "Expected no error, got %d.\n", e);

    errno = -1;
    d = p_nexttoward(NAN, NAN);
    e = errno;
    ok(_isnan(d), "Expected NAN, got %0.16e.\n", d);
    ok(e == -1, "Expected no error, got %d.\n", e);

    errno = -1;
    d = p_nexttoward(0, NAN);
    e = errno;
    ok(_isnan(d), "Expected NAN, got %0.16e.\n", d);
    ok(e == -1, "Expected no error, got %d.\n", e);
}

static void test_towctrans(void)
{
    wchar_t ret;

    ret = p_wctrans("tolower");
    ok(ret == 2, "wctrans returned %d, expected 2\n", ret);
    ret = p_wctrans("toupper");
    ok(ret == 1, "wctrans returned %d, expected 1\n", ret);
    ret = p_wctrans("toLower");
    ok(ret == 0, "wctrans returned %d, expected 0\n", ret);
    ret = p_wctrans("");
    ok(ret == 0, "wctrans returned %d, expected 0\n", ret);
    if(0) { /* crashes on windows */
        ret = p_wctrans(NULL);
        ok(ret == 0, "wctrans returned %d, expected 0\n", ret);
    }

    ret = p_towctrans('t', 2);
    ok(ret == 't', "towctrans('t', 2) returned %c, expected t\n", ret);
    ret = p_towctrans('T', 2);
    ok(ret == 't', "towctrans('T', 2) returned %c, expected t\n", ret);
    ret = p_towctrans('T', 0);
    ok(ret == 't', "towctrans('T', 0) returned %c, expected t\n", ret);
    ret = p_towctrans('T', 3);
    ok(ret == 't', "towctrans('T', 3) returned %c, expected t\n", ret);
    ret = p_towctrans('t', 1);
    ok(ret == 'T', "towctrans('t', 1) returned %c, expected T\n", ret);
    ret = p_towctrans('T', 1);
    ok(ret == 'T', "towctrans('T', 1) returned %c, expected T\n", ret);
}

static void test_CurrentContext(void)
{
    _Context _ctx, *ret;
    Context *ctx;

    ctx = p_Context_CurrentContext();
    ok(!!ctx, "got NULL\n");

    memset(&_ctx, 0xcc, sizeof(_ctx));
    ret = p__Context__CurrentContext(&_ctx);
    ok(_ctx.ctx == ctx, "expected %p, got %p\n", ctx, _ctx.ctx);
    ok(ret == &_ctx, "expected %p, got %p\n", &_ctx, ret);
}

START_TEST(msvcr120)
{
    if (!init()) return;
    test__strtof();
    test_lconv();
    test__dsign();
    test__dpcomp();
    test____lc_locale_name_func();
    test__GetConcurrency();
    test__W_Gettnames();
    test__strtof();
    test_remainder();
    test_critical_section();
    test_feenv();
    test__wcreate_locale();
    test__Condition_variable();
    test_wctype();
    test_vsscanf();
    test__Cbuild();
    test_nexttoward();
    test_towctrans();
    test_CurrentContext();
}
