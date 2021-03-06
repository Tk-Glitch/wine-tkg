/*
 * ntoskrnl.exe testing framework
 *
 * Copyright 2015 Sebastian Lackner
 * Copyright 2015 Michael Müller
 * Copyright 2015 Christian Costa
 * Copyright 2020 Paul Gofman for CodeWeavers
 * Copyright 2020-2021 Zebediah Figura for CodeWeavers
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

static HANDLE okfile;
static LONG successes;
static LONG failures;
static LONG skipped;
static LONG todo_successes;
static LONG todo_failures;
static int todo_level, todo_do_loop;
static int running_under_wine;
static int winetest_debug;
static int winetest_report_success;

static inline void kvprintf(const char *format, __ms_va_list ap)
{
    static char buffer[512];
    IO_STATUS_BLOCK io;
    int len = vsnprintf(buffer, sizeof(buffer), format, ap);
    ZwWriteFile(okfile, NULL, NULL, NULL, &io, buffer, len, NULL, NULL);
}

static inline void WINAPIV kprintf(const char *format, ...)
{
    __ms_va_list valist;

    __ms_va_start(valist, format);
    kvprintf(format, valist);
    __ms_va_end(valist);
}

static inline NTSTATUS winetest_init(void)
{
    const struct test_data *data;
    SIZE_T size = sizeof(*data);
    OBJECT_ATTRIBUTES attr;
    UNICODE_STRING string;
    IO_STATUS_BLOCK io;
    void *addr = NULL;
    HANDLE section;
    NTSTATUS ret;

    RtlInitUnicodeString(&string, L"\\BaseNamedObjects\\winetest_ntoskrnl_section");
    /* OBJ_KERNEL_HANDLE is necessary for the file to be accessible from system threads */
    InitializeObjectAttributes(&attr, &string, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, 0, NULL);
    if ((ret = ZwOpenSection(&section, SECTION_MAP_READ, &attr)))
        return ret;

    if ((ret = ZwMapViewOfSection(section, NtCurrentProcess(), &addr,
            0, 0, NULL, &size, ViewUnmap, 0, PAGE_READONLY)))
    {
        ZwClose(section);
        return ret;
    }
    data = addr;
    running_under_wine = data->running_under_wine;
    winetest_debug = data->winetest_debug;
    winetest_report_success = data->winetest_report_success;

    ZwUnmapViewOfSection(NtCurrentProcess(), addr);
    ZwClose(section);

    RtlInitUnicodeString(&string, L"\\??\\C:\\windows\\winetest_ntoskrnl_okfile");
    return ZwOpenFile(&okfile, FILE_APPEND_DATA | SYNCHRONIZE, &attr, &io,
            FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_SYNCHRONOUS_IO_NONALERT);
}

static inline void winetest_cleanup(void)
{
    struct test_data *data;
    SIZE_T size = sizeof(*data);
    OBJECT_ATTRIBUTES attr;
    UNICODE_STRING string;
    void *addr = NULL;
    HANDLE section;

    if (winetest_debug)
    {
        kprintf("%04x:ntoskrnl: %d tests executed (%d marked as todo, %d %s), %d skipped.\n",
                PsGetCurrentProcessId(), successes + failures + todo_successes + todo_failures,
                todo_successes, failures + todo_failures,
                (failures + todo_failures != 1) ? "failures" : "failure", skipped );
    }

    RtlInitUnicodeString(&string, L"\\BaseNamedObjects\\winetest_ntoskrnl_section");
    /* OBJ_KERNEL_HANDLE is necessary for the file to be accessible from system threads */
    InitializeObjectAttributes(&attr, &string, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, 0, NULL);

    if (!ZwOpenSection(&section, SECTION_MAP_READ | SECTION_MAP_WRITE, &attr))
    {
        if (!ZwMapViewOfSection(section, NtCurrentProcess(), &addr,
                0, 0, NULL, &size, ViewUnmap, 0, PAGE_READWRITE))
        {
            data = addr;

            InterlockedExchangeAdd(&data->successes, successes);
            InterlockedExchangeAdd(&data->failures, failures);
            InterlockedExchangeAdd(&data->skipped, skipped);
            InterlockedExchangeAdd(&data->todo_successes, todo_successes);
            InterlockedExchangeAdd(&data->todo_failures, todo_failures);

            ZwUnmapViewOfSection(NtCurrentProcess(), addr);
        }
        ZwClose(section);
    }

    ZwClose(okfile);
}

static inline void WINAPIV vok_(const char *file, int line, int condition, const char *msg,  __ms_va_list args)
{
    const char *current_file;

    if (!(current_file = drv_strrchr(file, '/')) &&
        !(current_file = drv_strrchr(file, '\\')))
        current_file = file;
    else
        current_file++;

    if (todo_level)
    {
        if (condition)
        {
            kprintf("%s:%d: Test succeeded inside todo block: ", current_file, line);
            kvprintf(msg, args);
            InterlockedIncrement(&todo_failures);
        }
        else
        {
            if (winetest_debug > 0)
            {
                kprintf("%s:%d: Test marked todo: ", current_file, line);
                kvprintf(msg, args);
            }
            InterlockedIncrement(&todo_successes);
        }
    }
    else
    {
        if (!condition)
        {
            kprintf("%s:%d: Test failed: ", current_file, line);
            kvprintf(msg, args);
            InterlockedIncrement(&failures);
        }
        else
        {
            if (winetest_report_success)
                kprintf("%s:%d: Test succeeded\n", current_file, line);
            InterlockedIncrement(&successes);
        }
    }
}

static inline void WINAPIV ok_(const char *file, int line, int condition, const char *msg, ...)
{
    __ms_va_list args;
    __ms_va_start(args, msg);
    vok_(file, line, condition, msg, args);
    __ms_va_end(args);
}

static inline void vskip_(const char *file, int line, const char *msg, __ms_va_list args)
{
    const char *current_file;

    if (!(current_file = drv_strrchr(file, '/')) &&
        !(current_file = drv_strrchr(file, '\\')))
        current_file = file;
    else
        current_file++;

    kprintf("%s:%d: Tests skipped: ", current_file, line);
    kvprintf(msg, args);
    skipped++;
}

static inline void WINAPIV win_skip_(const char *file, int line, const char *msg, ...)
{
    __ms_va_list args;
    __ms_va_start(args, msg);
    if (running_under_wine)
        vok_(file, line, 0, msg, args);
    else
        vskip_(file, line, msg, args);
    __ms_va_end(args);
}

static inline void WINAPIV trace_(const char *file, int line, const char *msg, ...)
{
    const char *current_file;
    __ms_va_list args;

    if (!(current_file = drv_strrchr(file, '/')) &&
        !(current_file = drv_strrchr(file, '\\')))
        current_file = file;
    else
        current_file++;

    __ms_va_start(args, msg);
    kprintf("%s:%d: ", current_file, line);
    kvprintf(msg, args);
    __ms_va_end(args);
}

static inline void winetest_start_todo( int is_todo )
{
    todo_level = (todo_level << 1) | (is_todo != 0);
    todo_do_loop=1;
}

static inline int winetest_loop_todo(void)
{
    int do_loop=todo_do_loop;
    todo_do_loop=0;
    return do_loop;
}

static inline void winetest_end_todo(void)
{
    todo_level >>= 1;
}

static inline int broken(int condition)
{
    return !running_under_wine && condition;
}

#define ok(condition, ...)  ok_(__FILE__, __LINE__, condition, __VA_ARGS__)
#define todo_if(is_todo) for (winetest_start_todo(is_todo); \
                              winetest_loop_todo(); \
                              winetest_end_todo())
#define todo_wine               todo_if(running_under_wine)
#define todo_wine_if(is_todo)   todo_if((is_todo) && running_under_wine)
#define win_skip(...)           win_skip_(__FILE__, __LINE__, __VA_ARGS__)
#define trace(...)              trace_(__FILE__, __LINE__, __VA_ARGS__)
