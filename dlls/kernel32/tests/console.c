/*
 * Unit tests for console API
 *
 * Copyright (c) 2003,2004 Eric Pouech
 * Copyright (c) 2007 Kirill K. Smirnov
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

#include "wine/test.h"
#include <windows.h>
#include <winternl.h>
#include <stdio.h>

static BOOL (WINAPI *pGetConsoleInputExeNameA)(DWORD, LPSTR);
static DWORD (WINAPI *pGetConsoleProcessList)(LPDWORD, DWORD);
static HANDLE (WINAPI *pOpenConsoleW)(LPCWSTR,DWORD,BOOL,DWORD);
static BOOL (WINAPI *pSetConsoleInputExeNameA)(LPCSTR);
static BOOL (WINAPI *pVerifyConsoleIoHandle)(HANDLE handle);

static BOOL skip_nt;

/* DEFAULT_ATTRIB is used for all initial filling of the console.
 * all modifications are made with TEST_ATTRIB so that we could check
 * what has to be modified or not
 */
#define TEST_ATTRIB    (BACKGROUND_BLUE | FOREGROUND_GREEN)
#define DEFAULT_ATTRIB (FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_RED)
/* when filling the screen with non-blank chars, this macro defines
 * what character should be at position 'c'
 */
#define CONTENT(c)    ('A' + (((c).Y * 17 + (c).X) % 23))

#define	okCURSOR(hCon, c) do { \
  CONSOLE_SCREEN_BUFFER_INFO __sbi; \
  BOOL expect = GetConsoleScreenBufferInfo((hCon), &__sbi) && \
                __sbi.dwCursorPosition.X == (c).X && __sbi.dwCursorPosition.Y == (c).Y; \
  ok(expect, "Expected cursor at (%d,%d), got (%d,%d)\n", \
     (c).X, (c).Y, __sbi.dwCursorPosition.X, __sbi.dwCursorPosition.Y); \
} while (0)

#define okCHAR(hCon, c, ch, attr) do { \
  char __ch; WORD __attr; DWORD __len; BOOL expect; \
  expect = ReadConsoleOutputCharacterA((hCon), &__ch, 1, (c), &__len) == 1 && __len == 1 && __ch == (ch); \
  ok(expect, "At (%d,%d): expecting char '%c'/%02x got '%c'/%02x\n", (c).X, (c).Y, (ch), (ch), __ch, __ch); \
  expect = ReadConsoleOutputAttribute((hCon), &__attr, 1, (c), &__len) == 1 && __len == 1 && __attr == (attr); \
  ok(expect, "At (%d,%d): expecting attr %04x got %04x\n", (c).X, (c).Y, (attr), __attr); \
} while (0)

static void init_function_pointers(void)
{
    HMODULE hKernel32;

#define KERNEL32_GET_PROC(func)                                     \
    p##func = (void *)GetProcAddress(hKernel32, #func);             \
    if(!p##func) trace("GetProcAddress(hKernel32, '%s') failed\n", #func);

    hKernel32 = GetModuleHandleA("kernel32.dll");
    KERNEL32_GET_PROC(GetConsoleInputExeNameA);
    KERNEL32_GET_PROC(GetConsoleProcessList);
    KERNEL32_GET_PROC(OpenConsoleW);
    KERNEL32_GET_PROC(SetConsoleInputExeNameA);
    KERNEL32_GET_PROC(VerifyConsoleIoHandle);

#undef KERNEL32_GET_PROC
}

/* FIXME: this could be optimized on a speed point of view */
static void resetContent(HANDLE hCon, COORD sbSize, BOOL content)
{
    COORD       c;
    WORD        attr = DEFAULT_ATTRIB;
    char        ch;
    DWORD       len;

    for (c.X = 0; c.X < sbSize.X; c.X++)
    {
        for (c.Y = 0; c.Y < sbSize.Y; c.Y++)
        {
            ch = (content) ? CONTENT(c) : ' ';
            WriteConsoleOutputAttribute(hCon, &attr, 1, c, &len);
            WriteConsoleOutputCharacterA(hCon, &ch, 1, c, &len);
        }
    }
}

static void testCursor(HANDLE hCon, COORD sbSize)
{
    COORD		c;

    c.X = c.Y = 0;
    ok(SetConsoleCursorPosition(0, c) == 0, "No handle\n");
    ok(GetLastError() == ERROR_INVALID_HANDLE, "GetLastError: expecting %u got %u\n",
       ERROR_INVALID_HANDLE, GetLastError());

    c.X = c.Y = 0;
    ok(SetConsoleCursorPosition(hCon, c) != 0, "Cursor in upper-left\n");
    okCURSOR(hCon, c);

    c.X = sbSize.X - 1;
    c.Y = sbSize.Y - 1;
    ok(SetConsoleCursorPosition(hCon, c) != 0, "Cursor in lower-right\n");
    okCURSOR(hCon, c);

    c.X = sbSize.X;
    c.Y = sbSize.Y - 1;
    ok(SetConsoleCursorPosition(hCon, c) == 0, "Cursor is outside\n");
    ok(GetLastError() == ERROR_INVALID_PARAMETER, "GetLastError: expecting %u got %u\n",
       ERROR_INVALID_PARAMETER, GetLastError());

    c.X = sbSize.X - 1;
    c.Y = sbSize.Y;
    ok(SetConsoleCursorPosition(hCon, c) == 0, "Cursor is outside\n");
    ok(GetLastError() == ERROR_INVALID_PARAMETER, "GetLastError: expecting %u got %u\n",
       ERROR_INVALID_PARAMETER, GetLastError());

    c.X = -1;
    c.Y = 0;
    ok(SetConsoleCursorPosition(hCon, c) == 0, "Cursor is outside\n");
    ok(GetLastError() == ERROR_INVALID_PARAMETER, "GetLastError: expecting %u got %u\n",
       ERROR_INVALID_PARAMETER, GetLastError());

    c.X = 0;
    c.Y = -1;
    ok(SetConsoleCursorPosition(hCon, c) == 0, "Cursor is outside\n");
    ok(GetLastError() == ERROR_INVALID_PARAMETER, "GetLastError: expecting %u got %u\n",
       ERROR_INVALID_PARAMETER, GetLastError());
}

static void testCursorInfo(HANDLE hCon)
{
    BOOL ret;
    CONSOLE_CURSOR_INFO info;
    HANDLE pipe1, pipe2;

    SetLastError(0xdeadbeef);
    ret = GetConsoleCursorInfo(NULL, NULL);
    ok(!ret, "Expected failure\n");
    ok(GetLastError() == ERROR_INVALID_HANDLE, "GetLastError: expecting %u got %u\n",
       ERROR_INVALID_HANDLE, GetLastError());

    SetLastError(0xdeadbeef);
    info.dwSize = -1;
    ret = GetConsoleCursorInfo(NULL, &info);
    ok(!ret, "Expected failure\n");
    ok(info.dwSize == -1, "Expected no change for dwSize\n");
    ok(GetLastError() == ERROR_INVALID_HANDLE, "GetLastError: expecting %u got %u\n",
       ERROR_INVALID_HANDLE, GetLastError());

    /* Test the correct call first to distinguish between win9x and the rest */
    SetLastError(0xdeadbeef);
    ret = GetConsoleCursorInfo(hCon, &info);
    ok(ret, "Expected success\n");
    ok(info.dwSize == 25 ||
       info.dwSize == 12 /* win9x */,
       "Expected 12 or 25, got %d\n", info.dwSize);
    ok(info.bVisible, "Expected the cursor to be visible\n");
    ok(GetLastError() == 0xdeadbeef, "GetLastError: expecting %u got %u\n",
       0xdeadbeef, GetLastError());

    CreatePipe(&pipe1, &pipe2, NULL, 0);
    info.dwSize = -1;
    ret = GetConsoleCursorInfo(pipe1, &info);
    ok(!ret, "Expected failure\n");
    ok(info.dwSize == -1, "Expected no change for dwSize\n");
    ok(GetLastError() == ERROR_INVALID_HANDLE, "GetLastError: %u\n",  GetLastError());
    CloseHandle(pipe1);
    CloseHandle(pipe2);

    /* Don't test NULL CONSOLE_CURSOR_INFO, it crashes on win9x and win7 */
}

static void testEmptyWrite(HANDLE hCon)
{
    static const char	emptybuf[16];
    COORD		c;
    DWORD		len;

    c.X = c.Y = 0;
    ok(SetConsoleCursorPosition(hCon, c) != 0, "Cursor in upper-left\n");

    len = -1;
    ok(WriteConsoleA(hCon, NULL, 0, &len, NULL) != 0 && len == 0, "WriteConsole\n");
    okCURSOR(hCon, c);

    /* Passing a NULL lpBuffer with sufficiently large non-zero length succeeds
     * on native Windows and result in memory-like contents being written to
     * the console. Calling WriteConsoleW like this will crash on Wine. */
    if (0)
    {
        len = -1;
        ok(!WriteConsoleA(hCon, NULL, 16, &len, NULL) && len == -1, "WriteConsole\n");
        okCURSOR(hCon, c);

        /* Cursor advances for this call. */
        len = -1;
        ok(WriteConsoleA(hCon, NULL, 128, &len, NULL) != 0 && len == 128, "WriteConsole\n");
    }

    len = -1;
    ok(WriteConsoleA(hCon, emptybuf, 0, &len, NULL) != 0 && len == 0, "WriteConsole\n");
    okCURSOR(hCon, c);

    /* WriteConsole does not halt on a null terminator and is happy to write
     * memory contents beyond the actual size of the buffer. */
    len = -1;
    ok(WriteConsoleA(hCon, emptybuf, 16, &len, NULL) != 0 && len == 16, "WriteConsole\n");
    c.X += 16;
    okCURSOR(hCon, c);
}

static void simple_write_console(HANDLE console, const char *text)
{
    DWORD len;
    COORD c = {0, 0};
    BOOL ret;

    /* single line write */
    c.X = c.Y = 0;
    ok(SetConsoleCursorPosition(console, c) != 0, "Cursor in upper-left\n");

    ret = WriteConsoleA(console, text, strlen(text), &len, NULL);
    ok(ret, "WriteConsoleA failed: %u\n", GetLastError());
    ok(len == strlen(text), "unexpected len %u\n", len);
}

static void testWriteSimple(HANDLE hCon)
{
    const char*	mytest = "abcdefg";
    int mylen = strlen(mytest);
    COORD c = {0, 0};

    simple_write_console(hCon, mytest);

    for (c.X = 0; c.X < mylen; c.X++)
    {
        okCHAR(hCon, c, mytest[c.X], TEST_ATTRIB);
    }

    okCURSOR(hCon, c);
    okCHAR(hCon, c, ' ', DEFAULT_ATTRIB);
}

static void testWriteNotWrappedNotProcessed(HANDLE hCon, COORD sbSize)
{
    COORD		c;
    DWORD		len, mode;
    const char*         mytest = "123";
    const int           mylen = strlen(mytest);
    int                 ret;
    int			p;

    ok(GetConsoleMode(hCon, &mode) && SetConsoleMode(hCon, mode & ~(ENABLE_PROCESSED_OUTPUT|ENABLE_WRAP_AT_EOL_OUTPUT)),
       "clearing wrap at EOL & processed output\n");

    /* write line, wrapping disabled, buffer exceeds sb width */
    c.X = sbSize.X - 3; c.Y = 0;
    ok(SetConsoleCursorPosition(hCon, c) != 0, "Cursor in upper-left-3\n");

    ret = WriteConsoleA(hCon, mytest, mylen, &len, NULL);
    ok(ret != 0 && len == mylen, "Couldn't write, ret = %d, len = %d\n", ret, len);
    c.Y = 0;
    for (p = mylen - 3; p < mylen; p++)
    {
        c.X = sbSize.X - 3 + p % 3;
        okCHAR(hCon, c, mytest[p], TEST_ATTRIB);
    }

    c.X = 0; c.Y = 1;
    okCHAR(hCon, c, ' ', DEFAULT_ATTRIB);

    p = sbSize.X - 3 + mylen % 3;
    c.X = p; c.Y = 0;

    /* write line, wrapping disabled, strings end on end of line */
    c.X = sbSize.X - mylen; c.Y = 0;
    ok(SetConsoleCursorPosition(hCon, c) != 0, "Cursor in upper-left-3\n");

    ok(WriteConsoleA(hCon, mytest, mylen, &len, NULL) != 0 && len == mylen, "WriteConsole\n");
}

static void testWriteNotWrappedProcessed(HANDLE hCon, COORD sbSize)
{
    COORD		c;
    DWORD		len, mode;
    const char*		mytest = "abcd\nf\tg";
    const int	mylen = strlen(mytest);
    const int	mylen2 = strchr(mytest, '\n') - mytest;
    int			p;
    WORD                attr;

    ok(GetConsoleMode(hCon, &mode) && SetConsoleMode(hCon, (mode | ENABLE_PROCESSED_OUTPUT) & ~ENABLE_WRAP_AT_EOL_OUTPUT),
       "clearing wrap at EOL & setting processed output\n");

    /* write line, wrapping disabled, buffer exceeds sb width */
    c.X = sbSize.X - 5; c.Y = 0;
    ok(SetConsoleCursorPosition(hCon, c) != 0, "Cursor in upper-left-5\n");

    ok(WriteConsoleA(hCon, mytest, mylen, &len, NULL) != 0 && len == mylen, "WriteConsole\n");
    c.Y = 0;
    for (c.X = sbSize.X - 5; c.X < sbSize.X - 1; c.X++)
    {
        okCHAR(hCon, c, mytest[c.X - sbSize.X + 5], TEST_ATTRIB);
    }

    ReadConsoleOutputAttribute(hCon, &attr, 1, c, &len);
    /* Win9x and WinMe change the attribs for '\n' up to 'f' */
    if (attr == TEST_ATTRIB)
    {
        win_skip("Win9x/WinMe don't respect ~ENABLE_WRAP_AT_EOL_OUTPUT\n");
        return;
    }

    okCHAR(hCon, c, ' ', DEFAULT_ATTRIB);

    c.X = 0; c.Y++;
    okCHAR(hCon, c, mytest[5], TEST_ATTRIB);
    for (c.X = 1; c.X < 8; c.X++)
        okCHAR(hCon, c, ' ', TEST_ATTRIB);
    okCHAR(hCon, c, mytest[7], TEST_ATTRIB);
    c.X++;
    okCHAR(hCon, c, ' ', DEFAULT_ATTRIB);

    okCURSOR(hCon, c);

    /* write line, wrapping disabled, strings end on end of line */
    c.X = sbSize.X - 4; c.Y = 0;
    ok(SetConsoleCursorPosition(hCon, c) != 0, "Cursor in upper-left-4\n");

    ok(WriteConsoleA(hCon, mytest, mylen, &len, NULL) != 0 && len == mylen, "WriteConsole\n");
    c.Y = 0;
    for (c.X = sbSize.X - 4; c.X < sbSize.X; c.X++)
    {
        okCHAR(hCon, c, mytest[c.X - sbSize.X + 4], TEST_ATTRIB);
    }
    c.X = 0; c.Y++;
    okCHAR(hCon, c, mytest[5], TEST_ATTRIB);
    for (c.X = 1; c.X < 8; c.X++)
        okCHAR(hCon, c, ' ', TEST_ATTRIB);
    okCHAR(hCon, c, mytest[7], TEST_ATTRIB);
    c.X++;
    okCHAR(hCon, c, ' ', DEFAULT_ATTRIB);

    okCURSOR(hCon, c);

    /* write line, wrapping disabled, strings end after end of line */
    c.X = sbSize.X - 3; c.Y = 0;
    ok(SetConsoleCursorPosition(hCon, c) != 0, "Cursor in upper-left-4\n");

    ok(WriteConsoleA(hCon, mytest, mylen, &len, NULL) != 0 && len == mylen, "WriteConsole\n");
    c.Y = 0;
    for (p = mylen2 - 3; p < mylen2; p++)
    {
        c.X = sbSize.X - 3 + p % 3;
        okCHAR(hCon, c, mytest[p], TEST_ATTRIB);
    }
    c.X = 0; c.Y = 1;
    okCHAR(hCon, c, mytest[5], TEST_ATTRIB);
    for (c.X = 1; c.X < 8; c.X++)
        okCHAR(hCon, c, ' ', TEST_ATTRIB);
    okCHAR(hCon, c, mytest[7], TEST_ATTRIB);
    c.X++;
    okCHAR(hCon, c, ' ', DEFAULT_ATTRIB);

    okCURSOR(hCon, c);
}

static void testWriteWrappedNotProcessed(HANDLE hCon, COORD sbSize)
{
    COORD		c;
    DWORD		len, mode;
    const char*		mytest = "abcd\nf\tg";
    const int	mylen = strlen(mytest);
    int			p;

    ok(GetConsoleMode(hCon, &mode) && SetConsoleMode(hCon,(mode | ENABLE_WRAP_AT_EOL_OUTPUT) & ~(ENABLE_PROCESSED_OUTPUT)),
       "setting wrap at EOL & clearing processed output\n");

    /* write line, wrapping enabled, buffer doesn't exceed sb width */
    c.X = sbSize.X - 9; c.Y = 0;
    ok(SetConsoleCursorPosition(hCon, c) != 0, "Cursor in upper-left-9\n");

    ok(WriteConsoleA(hCon, mytest, mylen, &len, NULL) != 0 && len == mylen, "WriteConsole\n");
    c.Y = 0;
    for (p = 0; p < mylen; p++)
    {
        c.X = sbSize.X - 9 + p;
        okCHAR(hCon, c, mytest[p], TEST_ATTRIB);
    }
    c.X = sbSize.X - 9 + mylen;
    okCHAR(hCon, c, ' ', DEFAULT_ATTRIB);
    c.X = 0; c.Y = 1;
    okCHAR(hCon, c, ' ', DEFAULT_ATTRIB);

    /* write line, wrapping enabled, buffer does exceed sb width */
    c.X = sbSize.X - 3; c.Y = 0;
    ok(SetConsoleCursorPosition(hCon, c) != 0, "Cursor in upper-left-3\n");

    c.Y = 1;
    c.X = mylen - 3;
    okCHAR(hCon, c, ' ', DEFAULT_ATTRIB);
}

static void testWriteWrappedProcessed(HANDLE hCon, COORD sbSize)
{
    COORD		c;
    DWORD		len, mode;
    const char*		mytest = "abcd\nf\tg";
    const int	mylen = strlen(mytest);
    int			p;
    WORD                attr;

    ok(GetConsoleMode(hCon, &mode) && SetConsoleMode(hCon, mode | (ENABLE_WRAP_AT_EOL_OUTPUT|ENABLE_PROCESSED_OUTPUT)),
       "setting wrap at EOL & processed output\n");

    /* write line, wrapping enabled, buffer doesn't exceed sb width */
    c.X = sbSize.X - 9; c.Y = 0;
    ok(SetConsoleCursorPosition(hCon, c) != 0, "Cursor in upper-left-9\n");

    ok(WriteConsoleA(hCon, mytest, mylen, &len, NULL) != 0 && len == mylen, "WriteConsole\n");
    for (p = 0; p < 4; p++)
    {
        c.X = sbSize.X - 9 + p;
        okCHAR(hCon, c, mytest[p], TEST_ATTRIB);
    }
    c.X = sbSize.X - 9 + p;
    ReadConsoleOutputAttribute(hCon, &attr, 1, c, &len);
    if (attr == TEST_ATTRIB)
        win_skip("Win9x/WinMe changes attribs for '\\n' up to 'f'\n");
    else
        okCHAR(hCon, c, ' ', DEFAULT_ATTRIB);
    c.X = 0; c.Y++;
    okCHAR(hCon, c, mytest[5], TEST_ATTRIB);
    for (c.X = 1; c.X < 8; c.X++)
        okCHAR(hCon, c, ' ', TEST_ATTRIB);
    okCHAR(hCon, c, mytest[7], TEST_ATTRIB);
    c.X++;
    okCHAR(hCon, c, ' ', DEFAULT_ATTRIB);
    okCURSOR(hCon, c);

    /* write line, wrapping enabled, buffer does exceed sb width */
    c.X = sbSize.X - 3; c.Y = 2;
    ok(SetConsoleCursorPosition(hCon, c) != 0, "Cursor in upper-left-3\n");

    ok(WriteConsoleA(hCon, mytest, mylen, &len, NULL) != 0 && len == mylen, "WriteConsole\n");
    for (p = 0; p < 3; p++)
    {
        c.X = sbSize.X - 3 + p;
        okCHAR(hCon, c, mytest[p], TEST_ATTRIB);
    }
    c.X = 0; c.Y++;
    okCHAR(hCon, c, mytest[3], TEST_ATTRIB);
    c.X++;
    ReadConsoleOutputAttribute(hCon, &attr, 1, c, &len);
    if (attr == TEST_ATTRIB)
        win_skip("Win9x/WinMe changes attribs for '\\n' up to 'f'\n");
    else
        okCHAR(hCon, c, ' ', DEFAULT_ATTRIB);

    c.X = 0; c.Y++;
    okCHAR(hCon, c, mytest[5], TEST_ATTRIB);
    for (c.X = 1; c.X < 8; c.X++)
        okCHAR(hCon, c, ' ', TEST_ATTRIB);
    okCHAR(hCon, c, mytest[7], TEST_ATTRIB);
    c.X++;
    okCHAR(hCon, c, ' ', DEFAULT_ATTRIB);
    okCURSOR(hCon, c);
}

static void testWrite(HANDLE hCon, COORD sbSize)
{
    /* FIXME: should in fact ensure that the sb is at least 10 characters wide */
    ok(SetConsoleTextAttribute(hCon, TEST_ATTRIB), "Setting default text color\n");
    resetContent(hCon, sbSize, FALSE);
    testEmptyWrite(hCon);
    resetContent(hCon, sbSize, FALSE);
    testWriteSimple(hCon);
    resetContent(hCon, sbSize, FALSE);
    testWriteNotWrappedNotProcessed(hCon, sbSize);
    resetContent(hCon, sbSize, FALSE);
    testWriteNotWrappedProcessed(hCon, sbSize);
    resetContent(hCon, sbSize, FALSE);
    testWriteWrappedNotProcessed(hCon, sbSize);
    resetContent(hCon, sbSize, FALSE);
    testWriteWrappedProcessed(hCon, sbSize);
}

static void testScroll(HANDLE hCon, COORD sbSize)
{
    SMALL_RECT  scroll, clip;
    COORD       dst, c, tc;
    CHAR_INFO   ci;
    BOOL ret;

#define W 11
#define H 7

#define IN_SRECT(r,c) ((r).Left <= (c).X && (c).X <= (r).Right && (r).Top <= (c).Y && (c).Y <= (r).Bottom)
#define IN_SRECT2(r,d,c) ((d).X <= (c).X && (c).X <= (d).X + (r).Right - (r).Left && (d).Y <= (c).Y && (c).Y <= (d).Y + (r).Bottom - (r).Top)

    /* no clipping, src & dst rect don't overlap */
    resetContent(hCon, sbSize, TRUE);

    scroll.Left = 0;
    scroll.Right = W - 1;
    scroll.Top = 0;
    scroll.Bottom = H - 1;
    dst.X = W + 3;
    dst.Y = H + 3;
    ci.Char.UnicodeChar = '#';
    ci.Attributes = TEST_ATTRIB;

    clip.Left = 0;
    clip.Right = sbSize.X - 1;
    clip.Top = 0;
    clip.Bottom = sbSize.Y - 1;

    ok(ScrollConsoleScreenBufferA(hCon, &scroll, NULL, dst, &ci), "Scrolling SB\n");

    for (c.Y = 0; c.Y < sbSize.Y; c.Y++)
    {
        for (c.X = 0; c.X < sbSize.X; c.X++)
        {
            if (IN_SRECT2(scroll, dst, c) && IN_SRECT(clip, c))
            {
                tc.X = c.X - dst.X;
                tc.Y = c.Y - dst.Y;
                okCHAR(hCon, c, CONTENT(tc), DEFAULT_ATTRIB);
            }
            else if (IN_SRECT(scroll, c) && IN_SRECT(clip, c))
                okCHAR(hCon, c, '#', TEST_ATTRIB);
            else okCHAR(hCon, c, CONTENT(c), DEFAULT_ATTRIB);
        }
    }

    /* no clipping, src & dst rect do overlap */
    resetContent(hCon, sbSize, TRUE);

    scroll.Left = 0;
    scroll.Right = W - 1;
    scroll.Top = 0;
    scroll.Bottom = H - 1;
    dst.X = W /2;
    dst.Y = H / 2;
    ci.Char.UnicodeChar = '#';
    ci.Attributes = TEST_ATTRIB;

    clip.Left = 0;
    clip.Right = sbSize.X - 1;
    clip.Top = 0;
    clip.Bottom = sbSize.Y - 1;

    ok(ScrollConsoleScreenBufferA(hCon, &scroll, NULL, dst, &ci), "Scrolling SB\n");

    for (c.Y = 0; c.Y < sbSize.Y; c.Y++)
    {
        for (c.X = 0; c.X < sbSize.X; c.X++)
        {
            if (dst.X <= c.X && c.X < dst.X + W && dst.Y <= c.Y && c.Y < dst.Y + H)
            {
                tc.X = c.X - dst.X;
                tc.Y = c.Y - dst.Y;
                okCHAR(hCon, c, CONTENT(tc), DEFAULT_ATTRIB);
            }
            else if (c.X < W && c.Y < H) okCHAR(hCon, c, '#', TEST_ATTRIB);
            else okCHAR(hCon, c, CONTENT(c), DEFAULT_ATTRIB);
        }
    }

    /* clipping, src & dst rect don't overlap */
    resetContent(hCon, sbSize, TRUE);

    scroll.Left = 0;
    scroll.Right = W - 1;
    scroll.Top = 0;
    scroll.Bottom = H - 1;
    dst.X = W + 3;
    dst.Y = H + 3;
    ci.Char.UnicodeChar = '#';
    ci.Attributes = TEST_ATTRIB;

    clip.Left = W / 2;
    clip.Right = min(W + W / 2, sbSize.X - 1);
    clip.Top = H / 2;
    clip.Bottom = min(H + H / 2, sbSize.Y - 1);

    SetLastError(0xdeadbeef);
    ret = ScrollConsoleScreenBufferA(hCon, &scroll, &clip, dst, &ci);
    if (ret)
    {
        for (c.Y = 0; c.Y < sbSize.Y; c.Y++)
        {
            for (c.X = 0; c.X < sbSize.X; c.X++)
            {
                if (IN_SRECT2(scroll, dst, c) && IN_SRECT(clip, c))
                {
                    tc.X = c.X - dst.X;
                    tc.Y = c.Y - dst.Y;
                    okCHAR(hCon, c, CONTENT(tc), DEFAULT_ATTRIB);
                }
                else if (IN_SRECT(scroll, c) && IN_SRECT(clip, c))
                    okCHAR(hCon, c, '#', TEST_ATTRIB);
                else okCHAR(hCon, c, CONTENT(c), DEFAULT_ATTRIB);
            }
        }
    }
    else
    {
        /* Win9x will fail, Only accept ERROR_NOT_ENOUGH_MEMORY */
        ok(GetLastError() == ERROR_NOT_ENOUGH_MEMORY,
            "Expected ERROR_NOT_ENOUGH_MEMORY, got %u\n", GetLastError());
    }

    /* clipping, src & dst rect do overlap */
    resetContent(hCon, sbSize, TRUE);

    scroll.Left = 0;
    scroll.Right = W - 1;
    scroll.Top = 0;
    scroll.Bottom = H - 1;
    dst.X = W / 2 - 3;
    dst.Y = H / 2 - 3;
    ci.Char.UnicodeChar = '#';
    ci.Attributes = TEST_ATTRIB;

    clip.Left = W / 2;
    clip.Right = min(W + W / 2, sbSize.X - 1);
    clip.Top = H / 2;
    clip.Bottom = min(H + H / 2, sbSize.Y - 1);

    ret = ScrollConsoleScreenBufferA(hCon, &scroll, &clip, dst, &ci);
    ok(ret, "ScrollConsoleScreenBufferA failed: %u\n", GetLastError());

    for (c.Y = 0; c.Y < sbSize.Y; c.Y++)
    {
        for (c.X = 0; c.X < sbSize.X; c.X++)
        {
            if (IN_SRECT2(scroll, dst, c) && IN_SRECT(clip, c))
            {
                tc.X = c.X - dst.X;
                tc.Y = c.Y - dst.Y;
                okCHAR(hCon, c, CONTENT(tc), DEFAULT_ATTRIB);
            }
            else if (IN_SRECT(scroll, c) && IN_SRECT(clip, c))
                okCHAR(hCon, c, '#', TEST_ATTRIB);
            else okCHAR(hCon, c, CONTENT(c), DEFAULT_ATTRIB);
        }
    }
}

static int mch_count;
/* we need the event as Wine console event generation isn't synchronous
 * (ie GenerateConsoleCtrlEvent returns before all ctrl-handlers in all
 * processes have been called).
 */
static HANDLE mch_event;
static BOOL WINAPI mch(DWORD event)
{
    mch_count++;
    SetEvent(mch_event);
    return TRUE;
}

static void testCtrlHandler(void)
{
    ok(!SetConsoleCtrlHandler(mch, FALSE), "Shouldn't succeed\n");
    ok(GetLastError() == ERROR_INVALID_PARAMETER, "Bad error %u\n", GetLastError());
    ok(SetConsoleCtrlHandler(mch, TRUE), "Couldn't set handler\n");
    /* wine requires the event for the test, as we cannot ensure, so far, that
     * events are processed synchronously in GenerateConsoleCtrlEvent()
     */
    mch_event = CreateEventA(NULL, TRUE, FALSE, NULL);
    mch_count = 0;
    ok(GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0), "Couldn't send ctrl-c event\n");
    /* FIXME: it isn't synchronous on wine but it can still happen before we test */
    if (0) ok(mch_count == 1, "Event isn't synchronous\n");
    ok(WaitForSingleObject(mch_event, 3000) == WAIT_OBJECT_0, "event sending didn't work\n");
    CloseHandle(mch_event);

    /* Turning off ctrl-c handling doesn't work on win9x such way ... */
    ok(SetConsoleCtrlHandler(NULL, TRUE), "Couldn't turn off ctrl-c handling\n");
    mch_event = CreateEventA(NULL, TRUE, FALSE, NULL);
    mch_count = 0;
    if(!(GetVersion() & 0x80000000))
        /* ... and next line leads to an unhandled exception on 9x.  Avoid it on 9x. */
        ok(GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0), "Couldn't send ctrl-c event\n");
    ok(WaitForSingleObject(mch_event, 3000) == WAIT_TIMEOUT && mch_count == 0, "Event shouldn't have been sent\n");
    CloseHandle(mch_event);
    ok(SetConsoleCtrlHandler(mch, FALSE), "Couldn't remove handler\n");
    ok(!SetConsoleCtrlHandler(mch, FALSE), "Shouldn't succeed\n");
    ok(GetLastError() == ERROR_INVALID_PARAMETER, "Bad error %u\n", GetLastError());
}

/*
 * Test console screen buffer:
 * 1) Try to set invalid handle.
 * 2) Try to set non-console handles.
 * 3) Use CONOUT$ file as active SB.
 * 4) Test cursor.
 * 5) Test output codepage to show it is not a property of SB.
 * 6) Test switching to old SB if we close all handles to current SB - works
 * in Windows, TODO in wine.
 *
 * What is not tested but should be:
 * 1) ScreenBufferInfo
 */
static void testScreenBuffer(HANDLE hConOut)
{
    HANDLE hConOutRW, hConOutRO, hConOutWT;
    HANDLE hFileOutRW, hFileOutRO, hFileOutWT;
    HANDLE hConOutNew;
    char test_str1[] = "Test for SB1";
    char test_str2[] = "Test for SB2";
    char test_cp866[] = {0xe2, 0xa5, 0xe1, 0xe2, 0};
    char test_cp1251[] = {0xf2, 0xe5, 0xf1, 0xf2, 0};
    WCHAR test_unicode[] = {0x0442, 0x0435, 0x0441, 0x0442, 0};
    WCHAR str_wbuf[20];
    char str_buf[20];
    DWORD len, error;
    COORD c;
    BOOL ret;
    DWORD oldcp;

    if (!IsValidCodePage(866))
    {
        skip("Codepage 866 not available\n");
        return;
    }

    /* In the beginning set output codepage to 866 */
    oldcp = GetConsoleOutputCP();
    SetLastError(0xdeadbeef);
    ret = SetConsoleOutputCP(866);
    if (!ret && GetLastError() == ERROR_CALL_NOT_IMPLEMENTED)
    {
        win_skip("SetConsoleOutputCP is not implemented\n");
        return;
    }
    ok(ret, "Cannot set output codepage to 866\n");

    hConOutRW = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE,
                         FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                         CONSOLE_TEXTMODE_BUFFER, NULL);
    ok(hConOutRW != INVALID_HANDLE_VALUE,
       "Cannot create a new screen buffer for ReadWrite\n");
    hConOutRO = CreateConsoleScreenBuffer(GENERIC_READ,
                         FILE_SHARE_READ, NULL,
                         CONSOLE_TEXTMODE_BUFFER, NULL);
    ok(hConOutRO != INVALID_HANDLE_VALUE,
       "Cannot create a new screen buffer for ReadOnly\n");
    hConOutWT = CreateConsoleScreenBuffer(GENERIC_WRITE,
                         FILE_SHARE_WRITE, NULL,
                         CONSOLE_TEXTMODE_BUFFER, NULL);
    ok(hConOutWT != INVALID_HANDLE_VALUE,
       "Cannot create a new screen buffer for WriteOnly\n");

    hFileOutRW = CreateFileA("NUL", GENERIC_READ | GENERIC_WRITE,
                             FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                             OPEN_EXISTING, 0, NULL);
    ok(hFileOutRW != INVALID_HANDLE_VALUE, "Cannot open NUL for ReadWrite\n");
    hFileOutRO = CreateFileA("NUL", GENERIC_READ, FILE_SHARE_READ,
                             NULL, OPEN_EXISTING, 0, NULL);
    ok(hFileOutRO != INVALID_HANDLE_VALUE, "Cannot open NUL for ReadOnly\n");
    hFileOutWT = CreateFileA("NUL", GENERIC_WRITE, FILE_SHARE_WRITE,
                             NULL, OPEN_EXISTING, 0, NULL);
    ok(hFileOutWT != INVALID_HANDLE_VALUE, "Cannot open NUL for WriteOnly\n");

    /* Trying to set invalid handle */
    SetLastError(0);
    ok(!SetConsoleActiveScreenBuffer(INVALID_HANDLE_VALUE),
       "Shouldn't succeed\n");
    ok(GetLastError() == ERROR_INVALID_HANDLE,
       "GetLastError: expecting %u got %u\n",
       ERROR_INVALID_HANDLE, GetLastError());

    /* Trying to set non-console handles */
    SetLastError(0);
    ok(!SetConsoleActiveScreenBuffer(hFileOutRW), "Shouldn't succeed\n");
    ok(GetLastError() == ERROR_INVALID_HANDLE,
       "GetLastError: expecting %u got %u\n",
       ERROR_INVALID_HANDLE, GetLastError());

    SetLastError(0);
    ok(!SetConsoleActiveScreenBuffer(hFileOutRO), "Shouldn't succeed\n");
    ok(GetLastError() == ERROR_INVALID_HANDLE,
       "GetLastError: expecting %u got %u\n",
       ERROR_INVALID_HANDLE, GetLastError());

    SetLastError(0);
    ok(!SetConsoleActiveScreenBuffer(hFileOutWT), "Shouldn't succeed\n");
    ok(GetLastError() == ERROR_INVALID_HANDLE,
       "GetLastError: expecting %u got %u\n",
       ERROR_INVALID_HANDLE, GetLastError());

    /* trying to write non-console handle */
    SetLastError(0xdeadbeef);
    ret = WriteConsoleA(hFileOutRW, test_str1, lstrlenA(test_str1), &len, NULL);
    error = GetLastError();
    ok(!ret, "Shouldn't succeed\n");
    ok(error == ERROR_INVALID_HANDLE || error == ERROR_INVALID_FUNCTION,
       "GetLastError: got %u\n", error);

    SetLastError(0xdeadbeef);
    ret = WriteConsoleA(hFileOutRO, test_str1, lstrlenA(test_str1), &len, NULL);
    error = GetLastError();
    ok(!ret, "Shouldn't succeed\n");
    ok(error == ERROR_INVALID_HANDLE || error == ERROR_INVALID_FUNCTION,
       "GetLastError: got %u\n", error);

    SetLastError(0xdeadbeef);
    ret = WriteConsoleA(hFileOutWT, test_str1, lstrlenA(test_str1), &len, NULL);
    error = GetLastError();
    ok(!ret, "Shouldn't succeed\n");
    todo_wine ok(error == ERROR_INVALID_HANDLE || error == ERROR_INVALID_FUNCTION,
       "GetLastError: got %u\n", error);

    CloseHandle(hFileOutRW);
    CloseHandle(hFileOutRO);
    CloseHandle(hFileOutWT);

    /* Trying to set SB handles with various access modes */
    SetLastError(0);
    ok(!SetConsoleActiveScreenBuffer(hConOutRO), "Shouldn't succeed\n");
    ok(GetLastError() == ERROR_INVALID_HANDLE || broken(GetLastError() == ERROR_ACCESS_DENIED) /* win10 1809 */,
       "unexpected last error %u\n", GetLastError());

    ok(SetConsoleActiveScreenBuffer(hConOutWT), "Couldn't set new WriteOnly SB\n");

    ok(SetConsoleActiveScreenBuffer(hConOutRW), "Couldn't set new ReadWrite SB\n");

    CloseHandle(hConOutWT);
    CloseHandle(hConOutRO);

    /* Now we have two ReadWrite SB, active must be hConOutRW */
    /* Open current SB via CONOUT$ */
    hConOutNew = CreateFileA("CONOUT$", GENERIC_READ|GENERIC_WRITE, 0,
                             NULL, OPEN_EXISTING, 0, 0);
    ok(hConOutNew != INVALID_HANDLE_VALUE, "CONOUT$ is not opened\n");


    /* test cursor */
    c.X = c.Y = 10;
    SetConsoleCursorPosition(hConOut, c);
    c.X = c.Y = 5;
    SetConsoleCursorPosition(hConOutRW, c);
    okCURSOR(hConOutNew, c);
    c.X = c.Y = 10;
    okCURSOR(hConOut, c);


    c.X = c.Y = 0;

    /* Write using hConOutNew... */
    SetConsoleCursorPosition(hConOutNew, c);
    ret = WriteConsoleA(hConOutNew, test_str2, lstrlenA(test_str2), &len, NULL);
    ok (ret && len == lstrlenA(test_str2), "WriteConsoleA failed\n");
    /* ... and read it back via hConOutRW */
    ret = ReadConsoleOutputCharacterA(hConOutRW, str_buf, lstrlenA(test_str2), c, &len);
    ok(ret && len == lstrlenA(test_str2), "ReadConsoleOutputCharacterA failed\n");
    str_buf[lstrlenA(test_str2)] = 0;
    ok(!lstrcmpA(str_buf, test_str2), "got '%s' expected '%s'\n", str_buf, test_str2);


    /* Now test output codepage handling. Current is 866 as we set earlier. */
    SetConsoleCursorPosition(hConOutRW, c);
    ret = WriteConsoleA(hConOutRW, test_cp866, lstrlenA(test_cp866), &len, NULL);
    ok(ret && len == lstrlenA(test_cp866), "WriteConsoleA failed\n");
    ret = ReadConsoleOutputCharacterW(hConOutRW, str_wbuf, lstrlenA(test_cp866), c, &len);
    ok(ret && len == lstrlenA(test_cp866), "ReadConsoleOutputCharacterW failed\n");
    str_wbuf[lstrlenA(test_cp866)] = 0;
    ok(!lstrcmpW(str_wbuf, test_unicode), "string does not match the pattern\n");

    /*
     * cp866 is OK, let's switch to cp1251.
     * We expect that this codepage will be used in every SB - active and not.
     */
    ok(SetConsoleOutputCP(1251), "Cannot set output cp to 1251\n");
    SetConsoleCursorPosition(hConOutRW, c);
    ret = WriteConsoleA(hConOutRW, test_cp1251, lstrlenA(test_cp1251), &len, NULL);
    ok(ret && len == lstrlenA(test_cp1251), "WriteConsoleA failed\n");
    ret = ReadConsoleOutputCharacterW(hConOutRW, str_wbuf, lstrlenA(test_cp1251), c, &len);
    ok(ret && len == lstrlenA(test_cp1251), "ReadConsoleOutputCharacterW failed\n");
    str_wbuf[lstrlenA(test_cp1251)] = 0;
    ok(!lstrcmpW(str_wbuf, test_unicode), "string does not match the pattern\n");

    /* Check what has happened to hConOut. */
    SetConsoleCursorPosition(hConOut, c);
    ret = WriteConsoleA(hConOut, test_cp1251, lstrlenA(test_cp1251), &len, NULL);
    ok(ret && len == lstrlenA(test_cp1251), "WriteConsoleA failed\n");
    ret = ReadConsoleOutputCharacterW(hConOut, str_wbuf, lstrlenA(test_cp1251), c, &len);
    ok(ret && len == lstrlenA(test_cp1251), "ReadConsoleOutputCharacterW failed\n");
    str_wbuf[lstrlenA(test_cp1251)] = 0;
    ok(!lstrcmpW(str_wbuf, test_unicode), "string does not match the pattern\n");

    /* Close all handles of current console SB */
    CloseHandle(hConOutNew);
    CloseHandle(hConOutRW);

    /* Now active SB should be hConOut */
    hConOutNew = CreateFileA("CONOUT$", GENERIC_READ|GENERIC_WRITE, 0,
                             NULL, OPEN_EXISTING, 0, 0);
    ok(hConOutNew != INVALID_HANDLE_VALUE, "CONOUT$ is not opened\n");

    /* Write using hConOutNew... */
    SetConsoleCursorPosition(hConOutNew, c);
    ret = WriteConsoleA(hConOutNew, test_str1, lstrlenA(test_str1), &len, NULL);
    ok (ret && len == lstrlenA(test_str1), "WriteConsoleA failed\n");
    /* ... and read it back via hConOut */
    ret = ReadConsoleOutputCharacterA(hConOut, str_buf, lstrlenA(test_str1), c, &len);
    ok(ret && len == lstrlenA(test_str1), "ReadConsoleOutputCharacterA failed\n");
    str_buf[lstrlenA(test_str1)] = 0;
    todo_wine ok(!lstrcmpA(str_buf, test_str1), "got '%s' expected '%s'\n", str_buf, test_str1);
    CloseHandle(hConOutNew);

    /* This is not really needed under Windows */
    SetConsoleActiveScreenBuffer(hConOut);

    /* restore codepage */
    SetConsoleOutputCP(oldcp);
}

static void CALLBACK signaled_function(void *p, BOOLEAN timeout)
{
    HANDLE event = p;
    SetEvent(event);
    ok(!timeout, "wait shouldn't have timed out\n");
}

static void testWaitForConsoleInput(HANDLE input_handle)
{
    HANDLE wait_handle;
    HANDLE complete_event;
    INPUT_RECORD record;
    DWORD events_written;
    DWORD wait_ret;
    BOOL ret;

    complete_event = CreateEventW(NULL, FALSE, FALSE, NULL);

    /* Test success case */
    ret = RegisterWaitForSingleObject(&wait_handle, input_handle, signaled_function, complete_event, INFINITE, WT_EXECUTEONLYONCE);
    ok(ret == TRUE, "Expected RegisterWaitForSingleObject to return TRUE, got %d\n", ret);
    /* give worker thread a chance to start up */
    Sleep(100);
    record.EventType = KEY_EVENT;
    record.Event.KeyEvent.bKeyDown = 1;
    record.Event.KeyEvent.wRepeatCount = 1;
    record.Event.KeyEvent.wVirtualKeyCode = VK_RETURN;
    record.Event.KeyEvent.wVirtualScanCode = VK_RETURN;
    record.Event.KeyEvent.uChar.UnicodeChar = '\r';
    record.Event.KeyEvent.dwControlKeyState = 0;
    ret = WriteConsoleInputW(input_handle, &record, 1, &events_written);
    ok(ret == TRUE, "Expected WriteConsoleInputW to return TRUE, got %d\n", ret);
    wait_ret = WaitForSingleObject(complete_event, INFINITE);
    ok(wait_ret == WAIT_OBJECT_0, "Expected the handle to be signaled\n");
    ret = UnregisterWait(wait_handle);
    /* If the callback is still running, this fails with ERROR_IO_PENDING, but
       that's ok and expected. */
    ok(ret != 0 || GetLastError() == ERROR_IO_PENDING,
        "UnregisterWait failed with error %d\n", GetLastError());

    /* Test timeout case */
    FlushConsoleInputBuffer(input_handle);
    ret = RegisterWaitForSingleObject(&wait_handle, input_handle, signaled_function, complete_event, INFINITE, WT_EXECUTEONLYONCE);
    wait_ret = WaitForSingleObject(complete_event, 100);
    ok(wait_ret == WAIT_TIMEOUT, "Expected the wait to time out\n");
    ret = UnregisterWait(wait_handle);
    ok(ret, "UnregisterWait failed with error %d\n", GetLastError());

    /* Clean up */
    CloseHandle(complete_event);
}

static void test_GetSetConsoleInputExeName(void)
{
    BOOL ret;
    DWORD error;
    char buffer[MAX_PATH], module[MAX_PATH], *p;
    static char input_exe[MAX_PATH] = "winetest.exe";

    SetLastError(0xdeadbeef);
    ret = pGetConsoleInputExeNameA(0, NULL);
    error = GetLastError();
    ok(ret, "GetConsoleInputExeNameA failed\n");
    ok(error == ERROR_BUFFER_OVERFLOW, "got %u expected ERROR_BUFFER_OVERFLOW\n", error);

    SetLastError(0xdeadbeef);
    ret = pGetConsoleInputExeNameA(0, buffer);
    error = GetLastError();
    ok(ret, "GetConsoleInputExeNameA failed\n");
    ok(error == ERROR_BUFFER_OVERFLOW, "got %u expected ERROR_BUFFER_OVERFLOW\n", error);

    GetModuleFileNameA(GetModuleHandleA(NULL), module, sizeof(module));
    p = strrchr(module, '\\') + 1;

    ret = pGetConsoleInputExeNameA(ARRAY_SIZE(buffer), buffer);
    ok(ret, "GetConsoleInputExeNameA failed\n");
    todo_wine ok(!lstrcmpA(buffer, p), "got %s expected %s\n", buffer, p);

    SetLastError(0xdeadbeef);
    ret = pSetConsoleInputExeNameA(NULL);
    error = GetLastError();
    ok(!ret, "SetConsoleInputExeNameA failed\n");
    ok(error == ERROR_INVALID_PARAMETER, "got %u expected ERROR_INVALID_PARAMETER\n", error);

    SetLastError(0xdeadbeef);
    ret = pSetConsoleInputExeNameA("");
    error = GetLastError();
    ok(!ret, "SetConsoleInputExeNameA failed\n");
    ok(error == ERROR_INVALID_PARAMETER, "got %u expected ERROR_INVALID_PARAMETER\n", error);

    ret = pSetConsoleInputExeNameA(input_exe);
    ok(ret, "SetConsoleInputExeNameA failed\n");

    ret = pGetConsoleInputExeNameA(ARRAY_SIZE(buffer), buffer);
    ok(ret, "GetConsoleInputExeNameA failed\n");
    ok(!lstrcmpA(buffer, input_exe), "got %s expected %s\n", buffer, input_exe);
}

static void test_GetConsoleProcessList(void)
{
    DWORD ret, *list = NULL;

    if (!pGetConsoleProcessList)
    {
        win_skip("GetConsoleProcessList is not available\n");
        return;
    }

    SetLastError(0xdeadbeef);
    ret = pGetConsoleProcessList(NULL, 0);
    ok(ret == 0, "Expected failure\n");
    ok(GetLastError() == ERROR_INVALID_PARAMETER,
       "Expected ERROR_INVALID_PARAMETER, got %d\n",
       GetLastError());

    SetLastError(0xdeadbeef);
    ret = pGetConsoleProcessList(NULL, 1);
    ok(ret == 0, "Expected failure\n");
    ok(GetLastError() == ERROR_INVALID_PARAMETER,
       "Expected ERROR_INVALID_PARAMETER, got %d\n",
       GetLastError());

    /* We should only have 1 process but only for these specific unit tests as
     * we created our own console. An AttachConsole(ATTACH_PARENT_PROCESS) would
     * give us two processes for example.
     */
    list = HeapAlloc(GetProcessHeap(), 0, sizeof(DWORD));

    SetLastError(0xdeadbeef);
    ret = pGetConsoleProcessList(list, 0);
    ok(ret == 0, "Expected failure\n");
    ok(GetLastError() == ERROR_INVALID_PARAMETER,
       "Expected ERROR_INVALID_PARAMETER, got %d\n",
       GetLastError());

    SetLastError(0xdeadbeef);
    ret = pGetConsoleProcessList(list, 1);
    todo_wine
    ok(ret == 1, "Expected 1, got %d\n", ret);

    HeapFree(GetProcessHeap(), 0, list);

    list = HeapAlloc(GetProcessHeap(), 0, ret * sizeof(DWORD));

    SetLastError(0xdeadbeef);
    ret = pGetConsoleProcessList(list, ret);
    todo_wine
    ok(ret == 1, "Expected 1, got %d\n", ret);

    if (ret == 1)
    {
        DWORD pid = GetCurrentProcessId();
        ok(list[0] == pid, "Expected %d, got %d\n", pid, list[0]);
    }

    HeapFree(GetProcessHeap(), 0, list);
}

static void test_OpenCON(void)
{
    static const WCHAR conW[] = {'C','O','N',0};
    static const DWORD accesses[] = {CREATE_NEW, CREATE_ALWAYS, OPEN_EXISTING,
                                     OPEN_ALWAYS, TRUNCATE_EXISTING};
    unsigned            i;
    HANDLE              h;

    for (i = 0; i < ARRAY_SIZE(accesses); i++)
    {
        h = CreateFileW(conW, GENERIC_WRITE, 0, NULL, accesses[i], 0, NULL);
        ok(h != INVALID_HANDLE_VALUE || broken(accesses[i] == TRUNCATE_EXISTING /* Win8 */),
           "Expected to open the CON device on write (%x)\n", accesses[i]);
        CloseHandle(h);

        h = CreateFileW(conW, GENERIC_READ, 0, NULL, accesses[i], 0, NULL);
        /* Windows versions differ here:
         * MSDN states in CreateFile that TRUNCATE_EXISTING requires GENERIC_WRITE
         * NT, XP, Vista comply, but Win7 doesn't and allows opening CON with TRUNCATE_EXISTING
         * So don't test when disposition is TRUNCATE_EXISTING
         */
        ok(h != INVALID_HANDLE_VALUE || broken(accesses[i] == TRUNCATE_EXISTING /* Win7+ */),
           "Expected to open the CON device on read (%x)\n", accesses[i]);
        CloseHandle(h);
        h = CreateFileW(conW, GENERIC_READ|GENERIC_WRITE, 0, NULL, accesses[i], 0, NULL);
        ok(h == INVALID_HANDLE_VALUE, "Expected not to open the CON device on read-write (%x)\n", accesses[i]);
        ok(GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_INVALID_PARAMETER,
           "Unexpected error %x\n", GetLastError());
    }
}

static void test_OpenConsoleW(void)
{
    static const WCHAR coninW[] = {'C','O','N','I','N','$',0};
    static const WCHAR conoutW[] = {'C','O','N','O','U','T','$',0};
    static const WCHAR emptyW[] = {0};
    static const WCHAR invalidW[] = {'I','N','V','A','L','I','D',0};
    DWORD gle;

    static const struct
    {
        LPCWSTR name;
        DWORD access;
        BOOL inherit;
        DWORD creation;
        DWORD gle, gle2;
    } invalid_table[] = {
        {NULL,     0,                            FALSE,      0,                 ERROR_INVALID_PARAMETER, ERROR_PATH_NOT_FOUND},
        {NULL,     0,                            FALSE,      0xdeadbeef,        ERROR_INVALID_PARAMETER, ERROR_PATH_NOT_FOUND},
        {NULL,     0xdeadbeef,                   FALSE,      0,                 ERROR_INVALID_PARAMETER, ERROR_PATH_NOT_FOUND},
        {NULL,     0xdeadbeef,                   TRUE,       0xdeadbeef,        ERROR_INVALID_PARAMETER, ERROR_PATH_NOT_FOUND},
        {NULL,     0,                            FALSE,      OPEN_ALWAYS,       ERROR_INVALID_PARAMETER, ERROR_PATH_NOT_FOUND},
        {NULL,     GENERIC_READ | GENERIC_WRITE, FALSE,      0,                 ERROR_INVALID_PARAMETER, ERROR_PATH_NOT_FOUND},
        {NULL,     GENERIC_READ | GENERIC_WRITE, FALSE,      OPEN_ALWAYS,       ERROR_INVALID_PARAMETER, ERROR_PATH_NOT_FOUND},
        {NULL,     GENERIC_READ | GENERIC_WRITE, FALSE,      OPEN_EXISTING,     ERROR_INVALID_PARAMETER, ERROR_PATH_NOT_FOUND},
        {emptyW,   0,                            FALSE,      0,                 ERROR_INVALID_PARAMETER, ERROR_PATH_NOT_FOUND},
        {emptyW,   0,                            FALSE,      0xdeadbeef,        ERROR_INVALID_PARAMETER, ERROR_PATH_NOT_FOUND},
        {emptyW,   0xdeadbeef,                   FALSE,      0,                 ERROR_INVALID_PARAMETER, ERROR_PATH_NOT_FOUND},
        {emptyW,   0xdeadbeef,                   TRUE,       0xdeadbeef,        ERROR_INVALID_PARAMETER, ERROR_PATH_NOT_FOUND},
        {emptyW,   0,                            FALSE,      OPEN_ALWAYS,       ERROR_INVALID_PARAMETER, ERROR_PATH_NOT_FOUND},
        {emptyW,   GENERIC_READ | GENERIC_WRITE, FALSE,      0,                 ERROR_INVALID_PARAMETER, ERROR_PATH_NOT_FOUND},
        {emptyW,   GENERIC_READ | GENERIC_WRITE, FALSE,      OPEN_ALWAYS,       ERROR_INVALID_PARAMETER, ERROR_PATH_NOT_FOUND},
        {emptyW,   GENERIC_READ | GENERIC_WRITE, FALSE,      OPEN_EXISTING,     ERROR_INVALID_PARAMETER, ERROR_PATH_NOT_FOUND},
        {invalidW, 0,                            FALSE,      0,                 ERROR_INVALID_PARAMETER, ERROR_FILE_NOT_FOUND},
        {invalidW, 0,                            FALSE,      0xdeadbeef,        ERROR_INVALID_PARAMETER, 0},
        {invalidW, 0xdeadbeef,                   FALSE,      0,                 ERROR_INVALID_PARAMETER, ERROR_FILE_NOT_FOUND},
        {invalidW, 0xdeadbeef,                   TRUE,       0xdeadbeef,        ERROR_INVALID_PARAMETER, 0},
        {invalidW, 0,                            FALSE,      OPEN_ALWAYS,       ERROR_INVALID_PARAMETER, ERROR_FILE_NOT_FOUND},
        {invalidW, GENERIC_READ | GENERIC_WRITE, FALSE,      0,                 ERROR_INVALID_PARAMETER, ERROR_FILE_NOT_FOUND},
        {invalidW, GENERIC_READ | GENERIC_WRITE, FALSE,      OPEN_ALWAYS,       ERROR_INVALID_PARAMETER, ERROR_FILE_NOT_FOUND},
        {invalidW, GENERIC_READ | GENERIC_WRITE, FALSE,      OPEN_EXISTING,     ERROR_INVALID_PARAMETER, ERROR_FILE_NOT_FOUND},
        {coninW,   0,                            FALSE,      0xdeadbeef,        ERROR_INVALID_PARAMETER, 0},
        {coninW,   0xdeadbeef,                   FALSE,      0,                 ERROR_INVALID_PARAMETER, ERROR_ACCESS_DENIED},
        {coninW,   0xdeadbeef,                   TRUE,       0xdeadbeef,        ERROR_INVALID_PARAMETER, 0},
        {conoutW,  0,                            FALSE,      0xdeadbeef,        ERROR_INVALID_PARAMETER, 0},
        {conoutW,  0xceadbeef,                   FALSE,      0,                 ERROR_INVALID_PARAMETER, ERROR_ACCESS_DENIED},
        {conoutW,  0xdeadbeef,                   TRUE,       0xdeadbeef,        ERROR_INVALID_PARAMETER, 0},
    };
    static const struct
    {
        LPCWSTR name;
        DWORD access;
        BOOL inherit;
        DWORD creation;
    } valid_table[] = {
        {coninW,   0,                            FALSE,      0                },
        {coninW,   0,                            TRUE,       0                },
        {coninW,   GENERIC_EXECUTE,              TRUE,       0                },
        {coninW,   GENERIC_ALL,                  TRUE,       0                },
        {coninW,   0,                            FALSE,      OPEN_ALWAYS      },
        {coninW,   GENERIC_READ | GENERIC_WRITE, FALSE,      0                },
        {coninW,   GENERIC_READ | GENERIC_WRITE, FALSE,      CREATE_NEW       },
        {coninW,   GENERIC_READ | GENERIC_WRITE, FALSE,      CREATE_ALWAYS    },
        {coninW,   GENERIC_READ | GENERIC_WRITE, FALSE,      OPEN_ALWAYS      },
        {coninW,   GENERIC_READ | GENERIC_WRITE, FALSE,      TRUNCATE_EXISTING},
        {conoutW,  0,                            FALSE,      0                },
        {conoutW,  0,                            FALSE,      OPEN_ALWAYS      },
        {conoutW,  GENERIC_READ | GENERIC_WRITE, FALSE,      0                },
        {conoutW,  GENERIC_READ | GENERIC_WRITE, FALSE,      CREATE_NEW,      },
        {conoutW,  GENERIC_READ | GENERIC_WRITE, FALSE,      CREATE_ALWAYS    },
        {conoutW,  GENERIC_READ | GENERIC_WRITE, FALSE,      OPEN_ALWAYS      },
        {conoutW,  GENERIC_READ | GENERIC_WRITE, FALSE,      TRUNCATE_EXISTING},
    };

    int index;
    HANDLE ret;

    if (!pOpenConsoleW)
    {
        win_skip("OpenConsoleW is not available\n");
        return;
    }

    for (index = 0; index < ARRAY_SIZE(invalid_table); index++)
    {
        SetLastError(0xdeadbeef);
        ret = pOpenConsoleW(invalid_table[index].name, invalid_table[index].access,
                            invalid_table[index].inherit, invalid_table[index].creation);
        gle = GetLastError();
        ok(ret == INVALID_HANDLE_VALUE,
           "Expected OpenConsoleW to return INVALID_HANDLE_VALUE for index %d, got %p\n",
           index, ret);
        ok(gle == invalid_table[index].gle || (gle != 0 && gle == invalid_table[index].gle2),
           "Expected GetLastError() to return %u/%u for index %d, got %u\n",
           invalid_table[index].gle, invalid_table[index].gle2, index, gle);
    }

    for (index = 0; index < ARRAY_SIZE(valid_table); index++)
    {
        ret = pOpenConsoleW(valid_table[index].name, valid_table[index].access,
                            valid_table[index].inherit, valid_table[index].creation);
        todo_wine
        ok(ret != INVALID_HANDLE_VALUE || broken(ret == INVALID_HANDLE_VALUE /* until Win7 */),
           "Expected OpenConsoleW to succeed for index %d, got %p\n", index, ret);
        if (ret != INVALID_HANDLE_VALUE)
            CloseHandle(ret);
    }

    ret = pOpenConsoleW(coninW, GENERIC_READ | GENERIC_WRITE, FALSE, OPEN_EXISTING);
    ok(ret != INVALID_HANDLE_VALUE, "Expected OpenConsoleW to return a valid handle\n");
    if (ret != INVALID_HANDLE_VALUE)
        CloseHandle(ret);

    ret = pOpenConsoleW(conoutW, GENERIC_READ | GENERIC_WRITE, FALSE, OPEN_EXISTING);
    ok(ret != INVALID_HANDLE_VALUE, "Expected OpenConsoleW to return a valid handle\n");
    if (ret != INVALID_HANDLE_VALUE)
        CloseHandle(ret);
}

static void test_CreateFileW(void)
{
    static const struct
    {
        BOOL input;
        DWORD access;
        BOOL inherit;
        DWORD creation;
        DWORD gle;
        BOOL is_broken;
    } cf_table[] = {
        {TRUE,   0,                            FALSE,      OPEN_ALWAYS,       0,                              FALSE},
        {TRUE,   GENERIC_READ | GENERIC_WRITE, FALSE,      0,                 ERROR_INVALID_PARAMETER,        TRUE},
        {TRUE,   0,                            FALSE,      0,                 ERROR_INVALID_PARAMETER,        TRUE},
        {TRUE,   GENERIC_READ | GENERIC_WRITE, FALSE,      CREATE_NEW,        0,                              FALSE},
        {TRUE,   GENERIC_READ | GENERIC_WRITE, FALSE,      CREATE_ALWAYS,     0,                              FALSE},
        {TRUE,   GENERIC_READ | GENERIC_WRITE, FALSE,      OPEN_ALWAYS,       0,                              FALSE},
        {FALSE,  0,                            FALSE,      0,                 ERROR_INVALID_PARAMETER,        TRUE},
        {FALSE,  0,                            FALSE,      OPEN_ALWAYS,       0,                              FALSE},
        {FALSE,  GENERIC_READ | GENERIC_WRITE, FALSE,      0,                 ERROR_INVALID_PARAMETER,        TRUE},
        {FALSE,  GENERIC_READ | GENERIC_WRITE, FALSE,      CREATE_NEW,        0,                              FALSE},
        {FALSE,  GENERIC_READ | GENERIC_WRITE, FALSE,      CREATE_ALWAYS,     0,                              FALSE},
        {FALSE,  GENERIC_READ | GENERIC_WRITE, FALSE,      OPEN_ALWAYS,       0,                              FALSE},
        /* TRUNCATE_EXISTING is forbidden starting with Windows 8 */
    };

    static const UINT nt_disposition[5] =
    {
        FILE_CREATE,        /* CREATE_NEW */
        FILE_OVERWRITE_IF,  /* CREATE_ALWAYS */
        FILE_OPEN,          /* OPEN_EXISTING */
        FILE_OPEN_IF,       /* OPEN_ALWAYS */
        FILE_OVERWRITE      /* TRUNCATE_EXISTING */
    };

    int index;
    HANDLE ret;
    SECURITY_ATTRIBUTES sa;
    OBJECT_ATTRIBUTES attr = {sizeof(attr)};
    UNICODE_STRING string;
    IO_STATUS_BLOCK iosb;
    NTSTATUS status;

    for (index = 0; index < ARRAY_SIZE(cf_table); index++)
    {
        SetLastError(0xdeadbeef);

        sa.nLength = sizeof(sa);
        sa.lpSecurityDescriptor = NULL;
        sa.bInheritHandle = cf_table[index].inherit;

        ret = CreateFileW(cf_table[index].input ? L"CONIN$" : L"CONOUT$", cf_table[index].access,
                          FILE_SHARE_READ|FILE_SHARE_WRITE, &sa,
                          cf_table[index].creation, FILE_ATTRIBUTE_NORMAL, NULL);
        if (ret == INVALID_HANDLE_VALUE)
        {
            ok(cf_table[index].gle,
               "Expected CreateFileW not to return INVALID_HANDLE_VALUE for index %d\n", index);
            ok(GetLastError() == cf_table[index].gle,
                "Expected GetLastError() to return %u for index %d, got %u\n",
                cf_table[index].gle, index, GetLastError());
        }
        else
        {
            ok(!cf_table[index].gle || broken(cf_table[index].is_broken) /* Win7 */,
               "Expected CreateFileW to succeed for index %d\n", index);
            CloseHandle(ret);
        }

        if (skip_nt) continue;

        SetLastError(0xdeadbeef);

        sa.nLength = sizeof(sa);
        sa.lpSecurityDescriptor = NULL;
        sa.bInheritHandle = cf_table[index].inherit;

        ret = CreateFileW(cf_table[index].input ? L"\\??\\CONIN$" : L"\\??\\CONOUT$", cf_table[index].access,
                          FILE_SHARE_READ|FILE_SHARE_WRITE, &sa,
                          cf_table[index].creation, FILE_ATTRIBUTE_NORMAL, NULL);
        if (!index && ret == INVALID_HANDLE_VALUE)
        {
            todo_wine
            win_skip("Skipping NT path tests, not supported on this Windows version\n");
            skip_nt = TRUE;
            continue;
        }
        if (cf_table[index].gle)
            ok(ret == INVALID_HANDLE_VALUE && GetLastError() == cf_table[index].gle,
               "CreateFileW to returned %p %u for index %d\n", ret, GetLastError(), index);
        else
            ok(ret != INVALID_HANDLE_VALUE && (!cf_table[index].gle || broken(cf_table[index].is_broken) /* Win7 */),
               "CreateFileW to returned %p %u for index %d\n", ret, GetLastError(), index);
        if (ret != INVALID_HANDLE_VALUE) CloseHandle(ret);

        if (cf_table[index].gle) continue;

        RtlInitUnicodeString(&string, cf_table[index].input
                             ? L"\\Device\\ConDrv\\CurrentIn" : L"\\Device\\ConDrv\\CurrentOut");
        attr.ObjectName = &string;
        status = NtCreateFile(&ret, cf_table[index].access | SYNCHRONIZE | FILE_READ_ATTRIBUTES, &attr, &iosb, NULL,
                              FILE_ATTRIBUTE_NORMAL, 0, nt_disposition[cf_table[index].creation - CREATE_NEW],
                              FILE_NON_DIRECTORY_FILE, NULL, 0);
        ok(!status, "NtCreateFile failed %x for %u\n", status, index);
        CloseHandle(ret);

        RtlInitUnicodeString(&string, cf_table[index].input ? L"\\??\\CONIN$" : L"\\??\\CONOUT$");
        attr.ObjectName = &string;
        status = NtCreateFile(&ret, cf_table[index].access | SYNCHRONIZE | FILE_READ_ATTRIBUTES, &attr, &iosb, NULL,
                              FILE_ATTRIBUTE_NORMAL, 0, nt_disposition[cf_table[index].creation - CREATE_NEW],
                              FILE_NON_DIRECTORY_FILE, NULL, 0);
        ok(!status, "NtCreateFile failed %x for %u\n", status, index);
        CloseHandle(ret);
    }
}

static void test_VerifyConsoleIoHandle( HANDLE handle )
{
    BOOL ret;
    DWORD error;

    if (!pVerifyConsoleIoHandle)
    {
        win_skip("VerifyConsoleIoHandle is not available\n");
        return;
    }

    /* invalid handle */
    SetLastError(0xdeadbeef);
    ret = pVerifyConsoleIoHandle((HANDLE)0xdeadbee0);
    error = GetLastError();
    ok(!ret, "expected VerifyConsoleIoHandle to fail\n");
    ok(error == 0xdeadbeef, "wrong GetLastError() %d\n", error);

    /* invalid handle + 1 */
    SetLastError(0xdeadbeef);
    ret = pVerifyConsoleIoHandle((HANDLE)0xdeadbee1);
    error = GetLastError();
    ok(!ret, "expected VerifyConsoleIoHandle to fail\n");
    ok(error == 0xdeadbeef, "wrong GetLastError() %d\n", error);

    /* invalid handle + 2 */
    SetLastError(0xdeadbeef);
    ret = pVerifyConsoleIoHandle((HANDLE)0xdeadbee2);
    error = GetLastError();
    ok(!ret, "expected VerifyConsoleIoHandle to fail\n");
    ok(error == 0xdeadbeef, "wrong GetLastError() %d\n", error);

    /* invalid handle + 3 */
    SetLastError(0xdeadbeef);
    ret = pVerifyConsoleIoHandle((HANDLE)0xdeadbee3);
    error = GetLastError();
    ok(!ret, "expected VerifyConsoleIoHandle to fail\n");
    ok(error == 0xdeadbeef, "wrong GetLastError() %d\n", error);

    /* valid handle */
    SetLastError(0xdeadbeef);
    ret = pVerifyConsoleIoHandle(handle);
    error = GetLastError();
    ok(ret ||
       broken(!ret), /* Windows 8 and 10 */
       "expected VerifyConsoleIoHandle to succeed\n");
    ok(error == 0xdeadbeef, "wrong GetLastError() %d\n", error);
}

static void test_GetSetStdHandle(void)
{
    HANDLE handle;
    DWORD error;
    BOOL ret;

    /* get invalid std handle */
    SetLastError(0xdeadbeef);
    handle = GetStdHandle(42);
    error = GetLastError();
    ok(error == ERROR_INVALID_HANDLE || broken(error == ERROR_INVALID_FUNCTION)/* Win9x */,
       "wrong GetLastError() %d\n", error);
    ok(handle == INVALID_HANDLE_VALUE, "expected INVALID_HANDLE_VALUE\n");

    /* get valid */
    SetLastError(0xdeadbeef);
    handle = GetStdHandle(STD_INPUT_HANDLE);
    error = GetLastError();
    ok(error == 0xdeadbeef, "wrong GetLastError() %d\n", error);

    /* set invalid std handle */
    SetLastError(0xdeadbeef);
    ret = SetStdHandle(42, handle);
    error = GetLastError();
    ok(!ret, "expected SetStdHandle to fail\n");
    ok(error == ERROR_INVALID_HANDLE || broken(error == ERROR_INVALID_FUNCTION)/* Win9x */,
       "wrong GetLastError() %d\n", error);

    /* set valid (restore old value) */
    SetLastError(0xdeadbeef);
    ret = SetStdHandle(STD_INPUT_HANDLE, handle);
    error = GetLastError();
    ok(ret, "expected SetStdHandle to succeed\n");
    ok(error == 0xdeadbeef, "wrong GetLastError() %d\n", error);
}

static void test_GetNumberOfConsoleInputEvents(HANDLE input_handle)
{
    DWORD count;
    BOOL ret;
    int i;

    const struct
    {
        HANDLE handle;
        LPDWORD nrofevents;
        DWORD last_error;
    } invalid_table[] =
    {
        {NULL,                 NULL,   ERROR_INVALID_HANDLE},
        {NULL,                 &count, ERROR_INVALID_HANDLE},
        {INVALID_HANDLE_VALUE, NULL,   ERROR_INVALID_HANDLE},
        {INVALID_HANDLE_VALUE, &count, ERROR_INVALID_HANDLE},
    };

    for (i = 0; i < ARRAY_SIZE(invalid_table); i++)
    {
        SetLastError(0xdeadbeef);
        if (invalid_table[i].nrofevents) count = 0xdeadbeef;
        ret = GetNumberOfConsoleInputEvents(invalid_table[i].handle,
                                            invalid_table[i].nrofevents);
        ok(!ret, "[%d] Expected GetNumberOfConsoleInputEvents to return FALSE, got %d\n", i, ret);
        if (invalid_table[i].nrofevents)
        {
            ok(count == 0xdeadbeef,
               "[%d] Expected output count to be unmodified, got %u\n", i, count);
        }
        ok(GetLastError() == invalid_table[i].last_error,
           "[%d] Expected last error to be %u, got %u\n",
           i, invalid_table[i].last_error, GetLastError());
    }

    /* Test crashes on Windows 7. */
    if (0)
    {
        SetLastError(0xdeadbeef);
        ret = GetNumberOfConsoleInputEvents(input_handle, NULL);
        ok(!ret, "Expected GetNumberOfConsoleInputEvents to return FALSE, got %d\n", ret);
        ok(GetLastError() == ERROR_INVALID_ACCESS,
           "Expected last error to be ERROR_INVALID_ACCESS, got %u\n",
           GetLastError());
    }

    count = 0xdeadbeef;
    ret = GetNumberOfConsoleInputEvents(input_handle, &count);
    ok(ret == TRUE, "Expected GetNumberOfConsoleInputEvents to return TRUE, got %d\n", ret);
    ok(count != 0xdeadbeef, "Expected output count to initialized\n");
}

static void test_WriteConsoleInputA(HANDLE input_handle)
{
    INPUT_RECORD event;
    INPUT_RECORD event_list[5];
    MOUSE_EVENT_RECORD mouse_event = { {0, 0}, 0, 0, MOUSE_MOVED };
    KEY_EVENT_RECORD key_event;
    DWORD count, console_mode, gle;
    BOOL ret;
    int i;

    const struct
    {
        HANDLE handle;
        const INPUT_RECORD *buffer;
        DWORD count;
        LPDWORD written;
        DWORD gle, gle2;
        int win_crash;
    } invalid_table[] =
    {
        {NULL, NULL, 0, NULL, ERROR_INVALID_ACCESS, 0, 1},
        {NULL, NULL, 0, &count,ERROR_INVALID_HANDLE},
        {NULL, NULL, 1, NULL, ERROR_INVALID_ACCESS, 0, 1},
        {NULL, NULL, 1, &count, ERROR_NOACCESS, ERROR_INVALID_ACCESS},
        {NULL, &event, 0, NULL, ERROR_INVALID_ACCESS, 0, 1},
        {NULL, &event, 0, &count, ERROR_INVALID_HANDLE},
        {NULL, &event, 1, NULL, ERROR_INVALID_ACCESS, 0, 1},
        {NULL, &event, 1, &count, ERROR_INVALID_HANDLE},
        {INVALID_HANDLE_VALUE, NULL, 0, NULL, ERROR_INVALID_ACCESS, 0, 1},
        {INVALID_HANDLE_VALUE, NULL, 0, &count, ERROR_INVALID_HANDLE},
        {INVALID_HANDLE_VALUE, NULL, 1, NULL, ERROR_INVALID_ACCESS, 0, 1},
        {INVALID_HANDLE_VALUE, NULL, 1, &count, ERROR_INVALID_HANDLE, ERROR_INVALID_ACCESS},
        {INVALID_HANDLE_VALUE, &event, 0, NULL, ERROR_INVALID_ACCESS, 0, 1},
        {INVALID_HANDLE_VALUE, &event, 0, &count, ERROR_INVALID_HANDLE},
        {INVALID_HANDLE_VALUE, &event, 1, NULL, ERROR_INVALID_ACCESS, 0, 1},
        {INVALID_HANDLE_VALUE, &event, 1, &count, ERROR_INVALID_HANDLE},
        {input_handle, NULL, 0, NULL, ERROR_INVALID_ACCESS, 0, 1},
        {input_handle, NULL, 1, NULL, ERROR_INVALID_ACCESS, 0, 1},
        {input_handle, NULL, 1, &count, ERROR_NOACCESS, ERROR_INVALID_ACCESS},
        {input_handle, &event, 0, NULL, ERROR_INVALID_ACCESS, 0, 1},
        {input_handle, &event, 1, NULL, ERROR_INVALID_ACCESS, 0, 1},
    };

    /* Suppress external sources of input events for the duration of the test. */
    ret = GetConsoleMode(input_handle, &console_mode);
    ok(ret == TRUE, "Expected GetConsoleMode to return TRUE, got %d\n", ret);
    if (!ret)
    {
        skip("GetConsoleMode failed with last error %u\n", GetLastError());
        return;
    }

    ret = SetConsoleMode(input_handle, console_mode & ~(ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT));
    ok(ret == TRUE, "Expected SetConsoleMode to return TRUE, got %d\n", ret);
    if (!ret)
    {
        skip("SetConsoleMode failed with last error %u\n", GetLastError());
        return;
    }

    /* Discard any events queued before the tests. */
    ret = FlushConsoleInputBuffer(input_handle);
    ok(ret == TRUE, "Expected FlushConsoleInputBuffer to return TRUE, got %d\n", ret);

    event.EventType = MOUSE_EVENT;
    event.Event.MouseEvent = mouse_event;

    for (i = 0; i < ARRAY_SIZE(invalid_table); i++)
    {
        if (invalid_table[i].win_crash)
            continue;

        SetLastError(0xdeadbeef);
        if (invalid_table[i].written) count = 0xdeadbeef;
        ret = WriteConsoleInputA(invalid_table[i].handle,
                                 invalid_table[i].buffer,
                                 invalid_table[i].count,
                                 invalid_table[i].written);
        ok(!ret, "[%d] Expected WriteConsoleInputA to return FALSE, got %d\n", i, ret);
        gle = GetLastError();
        ok(gle == invalid_table[i].gle || (gle != 0 && gle == invalid_table[i].gle2),
           "[%d] Expected last error to be %u or %u, got %u\n",
           i, invalid_table[i].gle, invalid_table[i].gle2, gle);
    }

    count = 0xdeadbeef;
    ret = WriteConsoleInputA(input_handle, NULL, 0, &count);
    ok(ret == TRUE, "Expected WriteConsoleInputA to return TRUE, got %d\n", ret);
    ok(count == 0, "Expected count to be 0, got %u\n", count);

    count = 0xdeadbeef;
    ret = WriteConsoleInputA(input_handle, &event, 0, &count);
    ok(ret == TRUE, "Expected WriteConsoleInputA to return TRUE, got %d\n", ret);
    ok(count == 0, "Expected count to be 0, got %u\n", count);

    count = 0xdeadbeef;
    ret = WriteConsoleInputA(input_handle, &event, 1, &count);
    ok(ret == TRUE, "Expected WriteConsoleInputA to return TRUE, got %d\n", ret);
    ok(count == 1, "Expected count to be 1, got %u\n", count);

    ret = FlushConsoleInputBuffer(input_handle);
    ok(ret == TRUE, "Expected FlushConsoleInputBuffer to return TRUE, got %d\n", ret);

    /* Writing a single mouse event doesn't seem to affect the count if an adjacent mouse event is already queued. */
    event.EventType = MOUSE_EVENT;
    event.Event.MouseEvent = mouse_event;

    ret = WriteConsoleInputA(input_handle, &event, 1, &count);
    ok(ret == TRUE, "Expected WriteConsoleInputA to return TRUE, got %d\n", ret);
    ok(count == 1, "Expected count to be 1, got %u\n", count);

    ret = GetNumberOfConsoleInputEvents(input_handle, &count);
    ok(ret == TRUE, "Expected GetNumberOfConsoleInputEvents to return TRUE, got %d\n", ret);
    ok(count == 1, "Expected count to be 1, got %u\n", count);

    ret = WriteConsoleInputA(input_handle, &event, 1, &count);
    ok(ret == TRUE, "Expected WriteConsoleInputA to return TRUE, got %d\n", ret);
    ok(count == 1, "Expected count to be 1, got %u\n", count);

    ret = GetNumberOfConsoleInputEvents(input_handle, &count);
    ok(ret == TRUE, "Expected GetNumberOfConsoleInputEvents to return TRUE, got %d\n", ret);
    todo_wine
    ok(count == 1, "Expected count to be 1, got %u\n", count);

    ret = FlushConsoleInputBuffer(input_handle);
    ok(ret == TRUE, "Expected FlushConsoleInputBuffer to return TRUE, got %d\n", ret);

    for (i = 0; i < ARRAY_SIZE(event_list); i++)
    {
        event_list[i].EventType = MOUSE_EVENT;
        event_list[i].Event.MouseEvent = mouse_event;
    }

    /* Writing consecutive chunks of mouse events appears to work. */
    ret = WriteConsoleInputA(input_handle, event_list, ARRAY_SIZE(event_list), &count);
    ok(ret == TRUE, "Expected WriteConsoleInputA to return TRUE, got %d\n", ret);
    ok(count == ARRAY_SIZE(event_list),
       "Expected count to be event list length, got %u\n", count);

    ret = GetNumberOfConsoleInputEvents(input_handle, &count);
    ok(ret == TRUE, "Expected GetNumberOfConsoleInputEvents to return TRUE, got %d\n", ret);
    ok(count == ARRAY_SIZE(event_list),
       "Expected count to be event list length, got %u\n", count);

    ret = WriteConsoleInputA(input_handle, event_list, ARRAY_SIZE(event_list), &count);
    ok(ret == TRUE, "Expected WriteConsoleInputA to return TRUE, got %d\n", ret);
    ok(count == ARRAY_SIZE(event_list),
       "Expected count to be event list length, got %u\n", count);

    ret = GetNumberOfConsoleInputEvents(input_handle, &count);
    ok(ret == TRUE, "Expected GetNumberOfConsoleInputEvents to return TRUE, got %d\n", ret);
    ok(count == 2*ARRAY_SIZE(event_list),
       "Expected count to be twice event list length, got %u\n", count);

    /* Again, writing a single mouse event with adjacent mouse events queued doesn't appear to affect the count. */
    ret = WriteConsoleInputA(input_handle, &event, 1, &count);
    ok(ret == TRUE, "Expected WriteConsoleInputA to return TRUE, got %d\n", ret);
    ok(count == 1, "Expected count to be 1, got %u\n", count);

    ret = GetNumberOfConsoleInputEvents(input_handle, &count);
    ok(ret == TRUE, "Expected GetNumberOfConsoleInputEvents to return TRUE, got %d\n", ret);
    todo_wine
    ok(count == 2*ARRAY_SIZE(event_list),
       "Expected count to be twice event list length, got %u\n", count);

    ret = FlushConsoleInputBuffer(input_handle);
    ok(ret == TRUE, "Expected FlushConsoleInputBuffer to return TRUE, got %d\n", ret);

    key_event.bKeyDown = FALSE;
    key_event.wRepeatCount = 0;
    key_event.wVirtualKeyCode = VK_SPACE;
    key_event.wVirtualScanCode = VK_SPACE;
    key_event.uChar.AsciiChar = ' ';
    key_event.dwControlKeyState = 0;

    event.EventType = KEY_EVENT;
    event.Event.KeyEvent = key_event;

    /* Key events don't exhibit the same behavior as mouse events. */
    ret = WriteConsoleInputA(input_handle, &event, 1, &count);
    ok(ret == TRUE, "Expected WriteConsoleInputA to return TRUE, got %d\n", ret);
    ok(count == 1, "Expected count to be 1, got %u\n", count);

    ret = GetNumberOfConsoleInputEvents(input_handle, &count);
    ok(ret == TRUE, "Expected GetNumberOfConsoleInputEvents to return TRUE, got %d\n", ret);
    ok(count == 1, "Expected count to be 1, got %u\n", count);

    ret = WriteConsoleInputA(input_handle, &event, 1, &count);
    ok(ret == TRUE, "Expected WriteConsoleInputA to return TRUE, got %d\n", ret);
    ok(count == 1, "Expected count to be 1, got %u\n", count);

    ret = GetNumberOfConsoleInputEvents(input_handle, &count);
    ok(ret == TRUE, "Expected GetNumberOfConsoleInputEvents to return TRUE, got %d\n", ret);
    ok(count == 2, "Expected count to be 2, got %u\n", count);

    ret = FlushConsoleInputBuffer(input_handle);
    ok(ret == TRUE, "Expected FlushConsoleInputBuffer to return TRUE, got %d\n", ret);

    /* Try interleaving mouse and key events. */
    event.EventType = MOUSE_EVENT;
    event.Event.MouseEvent = mouse_event;

    ret = WriteConsoleInputA(input_handle, &event, 1, &count);
    ok(ret == TRUE, "Expected WriteConsoleInputA to return TRUE, got %d\n", ret);
    ok(count == 1, "Expected count to be 1, got %u\n", count);

    ret = GetNumberOfConsoleInputEvents(input_handle, &count);
    ok(ret == TRUE, "Expected GetNumberOfConsoleInputEvents to return TRUE, got %d\n", ret);
    ok(count == 1, "Expected count to be 1, got %u\n", count);

    event.EventType = KEY_EVENT;
    event.Event.KeyEvent = key_event;

    ret = WriteConsoleInputA(input_handle, &event, 1, &count);
    ok(ret == TRUE, "Expected WriteConsoleInputA to return TRUE, got %d\n", ret);
    ok(count == 1, "Expected count to be 1, got %u\n", count);

    ret = GetNumberOfConsoleInputEvents(input_handle, &count);
    ok(ret == TRUE, "Expected GetNumberOfConsoleInputEvents to return TRUE, got %d\n", ret);
    ok(count == 2, "Expected count to be 2, got %u\n", count);

    event.EventType = MOUSE_EVENT;
    event.Event.MouseEvent = mouse_event;

    ret = WriteConsoleInputA(input_handle, &event, 1, &count);
    ok(ret == TRUE, "Expected WriteConsoleInputA to return TRUE, got %d\n", ret);
    ok(count == 1, "Expected count to be 1, got %u\n", count);

    ret = GetNumberOfConsoleInputEvents(input_handle, &count);
    ok(ret == TRUE, "Expected GetNumberOfConsoleInputEvents to return TRUE, got %d\n", ret);
    ok(count == 3, "Expected count to be 3, got %u\n", count);

    /* Restore the old console mode. */
    ret = SetConsoleMode(input_handle, console_mode);
    ok(ret == TRUE, "Expected SetConsoleMode to return TRUE, got %d\n", ret);
}

static void test_WriteConsoleInputW(HANDLE input_handle)
{
    INPUT_RECORD event;
    INPUT_RECORD event_list[5];
    MOUSE_EVENT_RECORD mouse_event = { {0, 0}, 0, 0, MOUSE_MOVED };
    KEY_EVENT_RECORD key_event;
    DWORD count, console_mode, gle;
    BOOL ret;
    int i;

    const struct
    {
        HANDLE handle;
        const INPUT_RECORD *buffer;
        DWORD count;
        LPDWORD written;
        DWORD gle, gle2;
        int win_crash;
    } invalid_table[] =
    {
        {NULL, NULL, 0, NULL, ERROR_INVALID_ACCESS, 0, 1},
        {NULL, NULL, 0, &count, ERROR_INVALID_HANDLE},
        {NULL, NULL, 1, NULL, ERROR_INVALID_ACCESS, 0, 1},
        {NULL, NULL, 1, &count, ERROR_NOACCESS, ERROR_INVALID_ACCESS},
        {NULL, &event, 0, NULL, ERROR_INVALID_ACCESS, 0, 1},
        {NULL, &event, 0, &count, ERROR_INVALID_HANDLE},
        {NULL, &event, 1, NULL, ERROR_INVALID_ACCESS, 0, 1},
        {NULL, &event, 1, &count, ERROR_INVALID_HANDLE},
        {INVALID_HANDLE_VALUE, NULL, 0, NULL, ERROR_INVALID_ACCESS, 0, 1},
        {INVALID_HANDLE_VALUE, NULL, 0, &count, ERROR_INVALID_HANDLE},
        {INVALID_HANDLE_VALUE, NULL, 1, NULL, ERROR_INVALID_ACCESS, 0, 1},
        {INVALID_HANDLE_VALUE, NULL, 1, &count, ERROR_INVALID_HANDLE, ERROR_INVALID_ACCESS},
        {INVALID_HANDLE_VALUE, &event, 0, NULL, ERROR_INVALID_ACCESS, 0, 1},
        {INVALID_HANDLE_VALUE, &event, 0, &count, ERROR_INVALID_HANDLE},
        {INVALID_HANDLE_VALUE, &event, 1, NULL, ERROR_INVALID_ACCESS, 0, 1},
        {INVALID_HANDLE_VALUE, &event, 1, &count, ERROR_INVALID_HANDLE},
        {input_handle, NULL, 0, NULL, ERROR_INVALID_ACCESS, 0, 1},
        {input_handle, NULL, 1, NULL, ERROR_INVALID_ACCESS, 0, 1},
        {input_handle, NULL, 1, &count, ERROR_NOACCESS, ERROR_INVALID_ACCESS},
        {input_handle, &event, 0, NULL, ERROR_INVALID_ACCESS, 0, 1},
        {input_handle, &event, 1, NULL, ERROR_INVALID_ACCESS, 0, 1},
    };

    /* Suppress external sources of input events for the duration of the test. */
    ret = GetConsoleMode(input_handle, &console_mode);
    ok(ret == TRUE, "Expected GetConsoleMode to return TRUE, got %d\n", ret);
    if (!ret)
    {
        skip("GetConsoleMode failed with last error %u\n", GetLastError());
        return;
    }

    ret = SetConsoleMode(input_handle, console_mode & ~(ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT));
    ok(ret == TRUE, "Expected SetConsoleMode to return TRUE, got %d\n", ret);
    if (!ret)
    {
        skip("SetConsoleMode failed with last error %u\n", GetLastError());
        return;
    }

    /* Discard any events queued before the tests. */
    ret = FlushConsoleInputBuffer(input_handle);
    ok(ret == TRUE, "Expected FlushConsoleInputBuffer to return TRUE, got %d\n", ret);

    event.EventType = MOUSE_EVENT;
    event.Event.MouseEvent = mouse_event;

    for (i = 0; i < ARRAY_SIZE(invalid_table); i++)
    {
        if (invalid_table[i].win_crash)
            continue;

        SetLastError(0xdeadbeef);
        if (invalid_table[i].written) count = 0xdeadbeef;
        ret = WriteConsoleInputW(invalid_table[i].handle,
                                 invalid_table[i].buffer,
                                 invalid_table[i].count,
                                 invalid_table[i].written);
        ok(!ret, "[%d] Expected WriteConsoleInputW to return FALSE, got %d\n", i, ret);
        gle = GetLastError();
        ok(gle == invalid_table[i].gle || (gle != 0 && gle == invalid_table[i].gle2),
           "[%d] Expected last error to be %u or %u, got %u\n",
           i, invalid_table[i].gle, invalid_table[i].gle2, gle);
    }

    count = 0xdeadbeef;
    ret = WriteConsoleInputW(input_handle, NULL, 0, &count);
    ok(ret == TRUE, "Expected WriteConsoleInputW to return TRUE, got %d\n", ret);
    ok(count == 0, "Expected count to be 0, got %u\n", count);

    count = 0xdeadbeef;
    ret = WriteConsoleInputW(input_handle, &event, 0, &count);
    ok(ret == TRUE, "Expected WriteConsoleInputW to return TRUE, got %d\n", ret);
    ok(count == 0, "Expected count to be 0, got %u\n", count);

    count = 0xdeadbeef;
    ret = WriteConsoleInputW(input_handle, &event, 1, &count);
    ok(ret == TRUE, "Expected WriteConsoleInputW to return TRUE, got %d\n", ret);
    ok(count == 1, "Expected count to be 1, got %u\n", count);

    ret = FlushConsoleInputBuffer(input_handle);
    ok(ret == TRUE, "Expected FlushConsoleInputBuffer to return TRUE, got %d\n", ret);

    /* Writing a single mouse event doesn't seem to affect the count if an adjacent mouse event is already queued. */
    event.EventType = MOUSE_EVENT;
    event.Event.MouseEvent = mouse_event;

    ret = WriteConsoleInputW(input_handle, &event, 1, &count);
    ok(ret == TRUE, "Expected WriteConsoleInputW to return TRUE, got %d\n", ret);
    ok(count == 1, "Expected count to be 1, got %u\n", count);

    ret = GetNumberOfConsoleInputEvents(input_handle, &count);
    ok(ret == TRUE, "Expected GetNumberOfConsoleInputEvents to return TRUE, got %d\n", ret);
    ok(count == 1, "Expected count to be 1, got %u\n", count);

    ret = WriteConsoleInputW(input_handle, &event, 1, &count);
    ok(ret == TRUE, "Expected WriteConsoleInputW to return TRUE, got %d\n", ret);
    ok(count == 1, "Expected count to be 1, got %u\n", count);

    ret = GetNumberOfConsoleInputEvents(input_handle, &count);
    ok(ret == TRUE, "Expected GetNumberOfConsoleInputEvents to return TRUE, got %d\n", ret);
    todo_wine
    ok(count == 1, "Expected count to be 1, got %u\n", count);

    ret = FlushConsoleInputBuffer(input_handle);
    ok(ret == TRUE, "Expected FlushConsoleInputBuffer to return TRUE, got %d\n", ret);

    for (i = 0; i < ARRAY_SIZE(event_list); i++)
    {
        event_list[i].EventType = MOUSE_EVENT;
        event_list[i].Event.MouseEvent = mouse_event;
    }

    /* Writing consecutive chunks of mouse events appears to work. */
    ret = WriteConsoleInputW(input_handle, event_list, ARRAY_SIZE(event_list), &count);
    ok(ret == TRUE, "Expected WriteConsoleInputW to return TRUE, got %d\n", ret);
    ok(count == ARRAY_SIZE(event_list),
       "Expected count to be event list length, got %u\n", count);

    ret = GetNumberOfConsoleInputEvents(input_handle, &count);
    ok(ret == TRUE, "Expected GetNumberOfConsoleInputEvents to return TRUE, got %d\n", ret);
    ok(count == ARRAY_SIZE(event_list),
       "Expected count to be event list length, got %u\n", count);

    ret = WriteConsoleInputW(input_handle, event_list, ARRAY_SIZE(event_list), &count);
    ok(ret == TRUE, "Expected WriteConsoleInputW to return TRUE, got %d\n", ret);
    ok(count == ARRAY_SIZE(event_list),
       "Expected count to be event list length, got %u\n", count);

    ret = GetNumberOfConsoleInputEvents(input_handle, &count);
    ok(ret == TRUE, "Expected GetNumberOfConsoleInputEvents to return TRUE, got %d\n", ret);
    ok(count == 2*ARRAY_SIZE(event_list),
       "Expected count to be twice event list length, got %u\n", count);

    /* Again, writing a single mouse event with adjacent mouse events queued doesn't appear to affect the count. */
    ret = WriteConsoleInputW(input_handle, &event, 1, &count);
    ok(ret == TRUE, "Expected WriteConsoleInputW to return TRUE, got %d\n", ret);
    ok(count == 1, "Expected count to be 1, got %u\n", count);

    ret = GetNumberOfConsoleInputEvents(input_handle, &count);
    ok(ret == TRUE, "Expected GetNumberOfConsoleInputEvents to return TRUE, got %d\n", ret);
    todo_wine
    ok(count == 2*ARRAY_SIZE(event_list),
       "Expected count to be twice event list length, got %u\n", count);

    ret = FlushConsoleInputBuffer(input_handle);
    ok(ret == TRUE, "Expected FlushConsoleInputBuffer to return TRUE, got %d\n", ret);

    key_event.bKeyDown = FALSE;
    key_event.wRepeatCount = 0;
    key_event.wVirtualKeyCode = VK_SPACE;
    key_event.wVirtualScanCode = VK_SPACE;
    key_event.uChar.UnicodeChar = ' ';
    key_event.dwControlKeyState = 0;

    event.EventType = KEY_EVENT;
    event.Event.KeyEvent = key_event;

    /* Key events don't exhibit the same behavior as mouse events. */
    ret = WriteConsoleInputW(input_handle, &event, 1, &count);
    ok(ret == TRUE, "Expected WriteConsoleInputW to return TRUE, got %d\n", ret);
    ok(count == 1, "Expected count to be 1, got %u\n", count);

    ret = GetNumberOfConsoleInputEvents(input_handle, &count);
    ok(ret == TRUE, "Expected GetNumberOfConsoleInputEvents to return TRUE, got %d\n", ret);
    ok(count == 1, "Expected count to be 1, got %u\n", count);

    ret = WriteConsoleInputW(input_handle, &event, 1, &count);
    ok(ret == TRUE, "Expected WriteConsoleInputW to return TRUE, got %d\n", ret);
    ok(count == 1, "Expected count to be 1, got %u\n", count);

    ret = GetNumberOfConsoleInputEvents(input_handle, &count);
    ok(ret == TRUE, "Expected GetNumberOfConsoleInputEvents to return TRUE, got %d\n", ret);
    ok(count == 2, "Expected count to be 2, got %u\n", count);

    ret = FlushConsoleInputBuffer(input_handle);
    ok(ret == TRUE, "Expected FlushConsoleInputBuffer to return TRUE, got %d\n", ret);

    /* Try interleaving mouse and key events. */
    event.EventType = MOUSE_EVENT;
    event.Event.MouseEvent = mouse_event;

    ret = WriteConsoleInputW(input_handle, &event, 1, &count);
    ok(ret == TRUE, "Expected WriteConsoleInputW to return TRUE, got %d\n", ret);
    ok(count == 1, "Expected count to be 1, got %u\n", count);

    ret = GetNumberOfConsoleInputEvents(input_handle, &count);
    ok(ret == TRUE, "Expected GetNumberOfConsoleInputEvents to return TRUE, got %d\n", ret);
    ok(count == 1, "Expected count to be 1, got %u\n", count);

    event.EventType = KEY_EVENT;
    event.Event.KeyEvent = key_event;

    ret = WriteConsoleInputW(input_handle, &event, 1, &count);
    ok(ret == TRUE, "Expected WriteConsoleInputW to return TRUE, got %d\n", ret);
    ok(count == 1, "Expected count to be 1, got %u\n", count);

    ret = GetNumberOfConsoleInputEvents(input_handle, &count);
    ok(ret == TRUE, "Expected GetNumberOfConsoleInputEvents to return TRUE, got %d\n", ret);
    ok(count == 2, "Expected count to be 2, got %u\n", count);

    event.EventType = MOUSE_EVENT;
    event.Event.MouseEvent = mouse_event;

    ret = WriteConsoleInputW(input_handle, &event, 1, &count);
    ok(ret == TRUE, "Expected WriteConsoleInputW to return TRUE, got %d\n", ret);
    ok(count == 1, "Expected count to be 1, got %u\n", count);

    ret = GetNumberOfConsoleInputEvents(input_handle, &count);
    ok(ret == TRUE, "Expected GetNumberOfConsoleInputEvents to return TRUE, got %d\n", ret);
    ok(count == 3, "Expected count to be 3, got %u\n", count);

    /* Restore the old console mode. */
    ret = SetConsoleMode(input_handle, console_mode);
    ok(ret == TRUE, "Expected SetConsoleMode to return TRUE, got %d\n", ret);
}

static void test_WriteConsoleOutputCharacterA(HANDLE output_handle)
{
    static const char output[] = {'a', 0};

    COORD origin = {0, 0};
    DWORD count;
    BOOL ret;
    int i;

    const struct
    {
        HANDLE hConsoleOutput;
        LPCSTR str;
        DWORD length;
        COORD coord;
        LPDWORD lpNumCharsWritten;
        DWORD expected_count;
        DWORD last_error;
        int win7_crash;
    } invalid_table[] =
    {
        {NULL, NULL, 0, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {NULL, NULL, 0, {0, 0}, &count, 0, ERROR_INVALID_HANDLE},
        {NULL, NULL, 1, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {NULL, NULL, 1, {0, 0}, &count, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {NULL, output, 0, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {NULL, output, 0, {0, 0}, &count, 0, ERROR_INVALID_HANDLE},
        {NULL, output, 1, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {NULL, output, 1, {0, 0}, &count, 0, ERROR_INVALID_HANDLE},
        {INVALID_HANDLE_VALUE, NULL, 0, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {INVALID_HANDLE_VALUE, NULL, 0, {0, 0}, &count, 0, ERROR_INVALID_HANDLE},
        {INVALID_HANDLE_VALUE, NULL, 1, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {INVALID_HANDLE_VALUE, NULL, 1, {0, 0}, &count, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {INVALID_HANDLE_VALUE, output, 0, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {INVALID_HANDLE_VALUE, output, 0, {0, 0}, &count, 0, ERROR_INVALID_HANDLE},
        {INVALID_HANDLE_VALUE, output, 1, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {INVALID_HANDLE_VALUE, output, 1, {0, 0}, &count, 0, ERROR_INVALID_HANDLE},
        {output_handle, NULL, 0, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {output_handle, NULL, 1, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {output_handle, NULL, 1, {0, 0}, &count, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {output_handle, output, 0, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {output_handle, output, 1, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
    };

    for (i = 0; i < ARRAY_SIZE(invalid_table); i++)
    {
        if (invalid_table[i].win7_crash)
            continue;

        SetLastError(0xdeadbeef);
        if (invalid_table[i].lpNumCharsWritten) count = 0xdeadbeef;
        ret = WriteConsoleOutputCharacterA(invalid_table[i].hConsoleOutput,
                                           invalid_table[i].str,
                                           invalid_table[i].length,
                                           invalid_table[i].coord,
                                           invalid_table[i].lpNumCharsWritten);
        ok(!ret, "[%d] Expected WriteConsoleOutputCharacterA to return FALSE, got %d\n", i, ret);
        if (invalid_table[i].lpNumCharsWritten)
        {
            ok(count == invalid_table[i].expected_count,
               "[%d] Expected count to be %u, got %u\n",
               i, invalid_table[i].expected_count, count);
        }
        ok(GetLastError() == invalid_table[i].last_error,
           "[%d] Expected last error to be %u, got %u\n",
           i, invalid_table[i].last_error, GetLastError());
    }

    count = 0xdeadbeef;
    ret = WriteConsoleOutputCharacterA(output_handle, NULL, 0, origin, &count);
    ok(ret == TRUE, "Expected WriteConsoleOutputCharacterA to return TRUE, got %d\n", ret);
    ok(count == 0, "Expected count to be 0, got %u\n", count);

    count = 0xdeadbeef;
    ret = WriteConsoleOutputCharacterA(output_handle, output, 0, origin, &count);
    ok(ret == TRUE, "Expected WriteConsoleOutputCharacterA to return TRUE, got %d\n", ret);
    ok(count == 0, "Expected count to be 0, got %u\n", count);

    count = 0xdeadbeef;
    ret = WriteConsoleOutputCharacterA(output_handle, output, 1, origin, &count);
    ok(ret == TRUE, "Expected WriteConsoleOutputCharacterA to return TRUE, got %d\n", ret);
    ok(count == 1, "Expected count to be 1, got %u\n", count);

    count = 0xdeadbeef;
    origin.X = 200;
    ret = WriteConsoleOutputCharacterA(output_handle, output, 0, origin, &count);
    ok(ret == TRUE, "Expected WriteConsoleOutputCharacterA to return TRUE, got %d\n", ret);
    ok(count == 0, "Expected count to be 0, got %u\n", count);
}

static void test_WriteConsoleOutputCharacterW(HANDLE output_handle)
{
    static const WCHAR outputW[] = {'a',0};

    COORD origin = {0, 0};
    DWORD count;
    BOOL ret;
    int i;

    const struct
    {
        HANDLE hConsoleOutput;
        LPCWSTR str;
        DWORD length;
        COORD coord;
        LPDWORD lpNumCharsWritten;
        DWORD expected_count;
        DWORD last_error;
        int win7_crash;
    } invalid_table[] =
    {
        {NULL, NULL, 0, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {NULL, NULL, 0, {0, 0}, &count, 0, ERROR_INVALID_HANDLE},
        {NULL, NULL, 1, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {NULL, NULL, 1, {0, 0}, &count, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {NULL, outputW, 0, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {NULL, outputW, 0, {0, 0}, &count, 0, ERROR_INVALID_HANDLE},
        {NULL, outputW, 1, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {NULL, outputW, 1, {0, 0}, &count, 0, ERROR_INVALID_HANDLE},
        {INVALID_HANDLE_VALUE, NULL, 0, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {INVALID_HANDLE_VALUE, NULL, 0, {0, 0}, &count, 0, ERROR_INVALID_HANDLE},
        {INVALID_HANDLE_VALUE, NULL, 1, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {INVALID_HANDLE_VALUE, NULL, 1, {0, 0}, &count, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {INVALID_HANDLE_VALUE, outputW, 0, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {INVALID_HANDLE_VALUE, outputW, 0, {0, 0}, &count, 0, ERROR_INVALID_HANDLE},
        {INVALID_HANDLE_VALUE, outputW, 1, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {INVALID_HANDLE_VALUE, outputW, 1, {0, 0}, &count, 0, ERROR_INVALID_HANDLE},
        {output_handle, NULL, 0, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {output_handle, NULL, 1, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {output_handle, NULL, 1, {0, 0}, &count, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {output_handle, outputW, 0, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {output_handle, outputW, 1, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
    };

    for (i = 0; i < ARRAY_SIZE(invalid_table); i++)
    {
        if (invalid_table[i].win7_crash)
            continue;

        SetLastError(0xdeadbeef);
        if (invalid_table[i].lpNumCharsWritten) count = 0xdeadbeef;
        ret = WriteConsoleOutputCharacterW(invalid_table[i].hConsoleOutput,
                                           invalid_table[i].str,
                                           invalid_table[i].length,
                                           invalid_table[i].coord,
                                           invalid_table[i].lpNumCharsWritten);
        ok(!ret, "[%d] Expected WriteConsoleOutputCharacterW to return FALSE, got %d\n", i, ret);
        if (invalid_table[i].lpNumCharsWritten)
        {
            ok(count == invalid_table[i].expected_count,
               "[%d] Expected count to be %u, got %u\n",
               i, invalid_table[i].expected_count, count);
        }
        ok(GetLastError() == invalid_table[i].last_error,
           "[%d] Expected last error to be %u, got %u\n",
           i, invalid_table[i].last_error, GetLastError());
    }

    count = 0xdeadbeef;
    ret = WriteConsoleOutputCharacterW(output_handle, NULL, 0, origin, &count);
    ok(ret == TRUE, "Expected WriteConsoleOutputCharacterW to return TRUE, got %d\n", ret);
    ok(count == 0, "Expected count to be 0, got %u\n", count);

    count = 0xdeadbeef;
    ret = WriteConsoleOutputCharacterW(output_handle, outputW, 0, origin, &count);
    ok(ret == TRUE, "Expected WriteConsoleOutputCharacterW to return TRUE, got %d\n", ret);
    ok(count == 0, "Expected count to be 0, got %u\n", count);

    count = 0xdeadbeef;
    ret = WriteConsoleOutputCharacterW(output_handle, outputW, 1, origin, &count);
    ok(ret == TRUE, "Expected WriteConsoleOutputCharacterW to return TRUE, got %d\n", ret);
    ok(count == 1, "Expected count to be 1, got %u\n", count);

    count = 0xdeadbeef;
    origin.X = 200;
    ret = WriteConsoleOutputCharacterW(output_handle, outputW, 0, origin, &count);
    ok(ret == TRUE, "Expected WriteConsoleOutputCharacterW to return TRUE, got %d\n", ret);
    ok(count == 0, "Expected count to be 0, got %u\n", count);

}

static void test_WriteConsoleOutputAttribute(HANDLE output_handle)
{
    WORD attr = FOREGROUND_BLUE;
    COORD origin = {0, 0};
    DWORD count;
    BOOL ret;
    int i;

    const struct
    {
        HANDLE hConsoleOutput;
        const WORD *attr;
        DWORD length;
        COORD coord;
        LPDWORD lpNumAttrsWritten;
        DWORD expected_count;
        DWORD last_error;
        int win7_crash;
    } invalid_table[] =
    {
        {NULL, NULL, 0, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {NULL, NULL, 0, {0, 0}, &count, 0, ERROR_INVALID_HANDLE},
        {NULL, NULL, 1, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {NULL, NULL, 1, {0, 0}, &count, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {NULL, &attr, 0, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {NULL, &attr, 0, {0, 0}, &count, 0, ERROR_INVALID_HANDLE},
        {NULL, &attr, 1, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {NULL, &attr, 1, {0, 0}, &count, 0, ERROR_INVALID_HANDLE},
        {INVALID_HANDLE_VALUE, NULL, 0, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {INVALID_HANDLE_VALUE, NULL, 0, {0, 0}, &count, 0, ERROR_INVALID_HANDLE},
        {INVALID_HANDLE_VALUE, NULL, 1, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {INVALID_HANDLE_VALUE, NULL, 1, {0, 0}, &count, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {INVALID_HANDLE_VALUE, &attr, 0, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {INVALID_HANDLE_VALUE, &attr, 0, {0, 0}, &count, 0, ERROR_INVALID_HANDLE},
        {INVALID_HANDLE_VALUE, &attr, 1, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {INVALID_HANDLE_VALUE, &attr, 1, {0, 0}, &count, 0, ERROR_INVALID_HANDLE},
        {output_handle, NULL, 0, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {output_handle, NULL, 1, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {output_handle, NULL, 1, {0, 0}, &count, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {output_handle, &attr, 0, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {output_handle, &attr, 1, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
    };

    for (i = 0; i < ARRAY_SIZE(invalid_table); i++)
    {
        if (invalid_table[i].win7_crash)
            continue;

        SetLastError(0xdeadbeef);
        if (invalid_table[i].lpNumAttrsWritten) count = 0xdeadbeef;
        ret = WriteConsoleOutputAttribute(invalid_table[i].hConsoleOutput,
                                          invalid_table[i].attr,
                                          invalid_table[i].length,
                                          invalid_table[i].coord,
                                          invalid_table[i].lpNumAttrsWritten);
        ok(!ret, "[%d] Expected WriteConsoleOutputAttribute to return FALSE, got %d\n", i, ret);
        if (invalid_table[i].lpNumAttrsWritten)
        {
            ok(count == invalid_table[i].expected_count,
               "[%d] Expected count to be %u, got %u\n",
               i, invalid_table[i].expected_count, count);
        }
        ok(GetLastError() == invalid_table[i].last_error,
           "[%d] Expected last error to be %u, got %u\n",
           i, invalid_table[i].last_error, GetLastError());
    }

    count = 0xdeadbeef;
    ret = WriteConsoleOutputAttribute(output_handle, NULL, 0, origin, &count);
    ok(ret == TRUE, "Expected WriteConsoleOutputAttribute to return TRUE, got %d\n", ret);
    ok(count == 0, "Expected count to be 0, got %u\n", count);

    count = 0xdeadbeef;
    ret = WriteConsoleOutputAttribute(output_handle, &attr, 0, origin, &count);
    ok(ret == TRUE, "Expected WriteConsoleOutputAttribute to return TRUE, got %d\n", ret);
    ok(count == 0, "Expected count to be 0, got %u\n", count);

    count = 0xdeadbeef;
    ret = WriteConsoleOutputAttribute(output_handle, &attr, 1, origin, &count);
    ok(ret == TRUE, "Expected WriteConsoleOutputAttribute to return TRUE, got %d\n", ret);
    ok(count == 1, "Expected count to be 1, got %u\n", count);

    count = 0xdeadbeef;
    origin.X = 200;
    ret = WriteConsoleOutputAttribute(output_handle, &attr, 0, origin, &count);
    ok(ret == TRUE, "Expected WriteConsoleOutputAttribute to return TRUE, got %d\n", ret);
    ok(count == 0, "Expected count to be 0, got %u\n", count);
}

static void set_region(SMALL_RECT *region, unsigned int left, unsigned int top, unsigned int right, unsigned int bottom)
{
    region->Left   = left;
    region->Top    = top;
    region->Right  = right;
    region->Bottom = bottom;
}

#define check_region(a,b,c,d,e) check_region_(__LINE__,a,b,c,d,e)
static void check_region_(unsigned int line, const SMALL_RECT *region, unsigned int left, unsigned int top, int right, int bottom)
{
    ok_(__FILE__,line)(region->Left == left, "Left = %u, expected %u\n", region->Left, left);
    ok_(__FILE__,line)(region->Top == top, "Top = %u, expected %u\n", region->Top, top);
    /* In multiple places returned region depends on Windows versions: some return right < left, others leave it untouched */
    if (right >= 0)
        ok_(__FILE__,line)(region->Right == right, "Right = %u, expected %u\n", region->Right, right);
    else
        ok_(__FILE__,line)(region->Right == -right || region->Right == region->Left - 1,
                           "Right = %u, expected %d\n", region->Right, right);
    if (bottom >= 0)
        ok_(__FILE__,line)(region->Bottom == bottom, "Bottom = %u, expected %u\n", region->Bottom, bottom);
    else
        ok_(__FILE__,line)(region->Bottom == -bottom || region->Bottom == region->Top - 1,
                           "Bottom = %u, expected %d\n", region->Bottom, bottom);
}

static void test_WriteConsoleOutput(HANDLE console)
{
    CONSOLE_SCREEN_BUFFER_INFO info;
    CHAR_INFO char_info_buf[2048];
    SMALL_RECT region;
    COORD size, coord;
    unsigned int i;
    BOOL ret;

    for (i = 0; i < ARRAY_SIZE(char_info_buf); i++)
    {
        char_info_buf[i].Char.UnicodeChar = '0' + i % 10;
        char_info_buf[i].Attributes = 0;
    }

    ret = GetConsoleScreenBufferInfo(console, &info);
    ok(ret, "GetConsoleScreenBufferInfo failed: %u\n", GetLastError());

    size.X = 23;
    size.Y = 17;
    coord.X = 2;
    coord.Y = 3;
    set_region(&region, 10, 7, 15, 11);
    ret = WriteConsoleOutputW(console, char_info_buf, size, coord, &region);
    ok(ret, "WriteConsoleOutputW failed: %u\n", GetLastError());
    check_region(&region, 10, 7, 15, 11);

    size.X = 23;
    size.Y = 17;
    coord.X = 2;
    coord.Y = 3;
    set_region(&region, 200, 7, 15, 211);
    ret = WriteConsoleOutputW(console, char_info_buf, size, coord, &region);
    ok(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "WriteConsoleOutputW returned: %x(%u)\n", ret, GetLastError());
    check_region(&region, 200, 7, 15, 211);

    size.X = 23;
    size.Y = 17;
    coord.X = 2;
    coord.Y = 3;
    set_region(&region, 200, 7, 211, 8);
    ret = WriteConsoleOutputW(console, char_info_buf, size, coord, &region);
    ok(ret, "WriteConsoleOutputW failed: %u\n", GetLastError());
    check_region(&region, 200, 7, 211, 8);

    size.X = 23;
    size.Y = 17;
    coord.X = 2;
    coord.Y = 3;
    set_region(&region, 10, 7, 9, 11);
    ret = WriteConsoleOutputW(console, char_info_buf, size, coord, &region);
    ok(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "WriteConsoleOutputW returned: %x(%u)\n", ret, GetLastError());
    check_region(&region, 10, 7, 9, 11);

    size.X = 23;
    size.Y = 17;
    coord.X = 2;
    coord.Y = 3;
    set_region(&region, 10, 7, 11, 6);
    ret = WriteConsoleOutputW(console, char_info_buf, size, coord, &region);
    ok(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "WriteConsoleOutputW returned: %x(%u)\n", ret, GetLastError());
    check_region(&region, 10, 7, 11, 6);

    size.X = 2;
    size.Y = 17;
    coord.X = 2;
    coord.Y = 3;
    set_region(&region, 10, 7, 15, 11);
    ret = WriteConsoleOutputW(console, char_info_buf, size, coord, &region);
    ok(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "WriteConsoleOutputW returned: %x(%u)\n", ret, GetLastError());
    check_region(&region, 10, 7, 15, 11);

    size.X = 23;
    size.Y = 3;
    coord.X = 2;
    coord.Y = 3;
    set_region(&region, 10, 7, 15, 11);
    ret = WriteConsoleOutputW(console, char_info_buf, size, coord, &region);
    ok(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "WriteConsoleOutputW returned: %x(%u)\n", ret, GetLastError());
    check_region(&region, 10, 7, 15, 11);

    size.X = 6;
    size.Y = 17;
    coord.X = 2;
    coord.Y = 3;
    set_region(&region, 10, 7, 15, 11);
    ret = WriteConsoleOutputW(console, char_info_buf, size, coord, &region);
    ok(ret, "WriteConsoleOutputW failed: %u\n", GetLastError());
    check_region(&region, 10, 7, 13, 11);

    size.X = 6;
    size.Y = 17;
    coord.X = 2;
    coord.Y = 3;
    set_region(&region, 10, 7, 15, 11);
    ret = WriteConsoleOutputW((HANDLE)0xdeadbeef, char_info_buf, size, coord, &region);
    ok(!ret && GetLastError() == ERROR_INVALID_HANDLE, "WriteConsoleOutputW returned: %x(%u)\n", ret, GetLastError());
    if (!skip_nt) check_region(&region, 10, 7, 13, 11);

    size.X = 16;
    size.Y = 7;
    coord.X = 2;
    coord.Y = 3;
    set_region(&region, 10, 7, 15, 11);
    ret = WriteConsoleOutputW(console, char_info_buf, size, coord, &region);
    ok(ret, "WriteConsoleOutputW failed: %u\n", GetLastError());
    check_region(&region, 10, 7, 15, 10);

    size.X = 16;
    size.Y = 7;
    coord.X = 2;
    coord.Y = 3;
    set_region(&region, info.dwSize.X - 2, 7, info.dwSize.X + 2, 7);
    ret = WriteConsoleOutputW(console, char_info_buf, size, coord, &region);
    ok(ret, "WriteConsoleOutputW failed: %u\n", GetLastError());
    check_region(&region, info.dwSize.X - 2, 7, info.dwSize.X - 1, 7);
}

static void test_FillConsoleOutputCharacterA(HANDLE output_handle)
{
    COORD origin = {0, 0};
    DWORD count;
    BOOL ret;
    int i;

    const struct
    {
        HANDLE hConsoleOutput;
        CHAR ch;
        DWORD length;
        COORD coord;
        LPDWORD lpNumCharsWritten;
        DWORD last_error;
        int win7_crash;
    } invalid_table[] =
    {
        {NULL, 'a', 0, {0, 0}, NULL, ERROR_INVALID_ACCESS, 1},
        {NULL, 'a', 0, {0, 0}, &count, ERROR_INVALID_HANDLE},
        {NULL, 'a', 1, {0, 0}, NULL, ERROR_INVALID_ACCESS, 1},
        {NULL, 'a', 1, {0, 0}, &count, ERROR_INVALID_HANDLE},
        {INVALID_HANDLE_VALUE, 'a', 0, {0, 0}, NULL, ERROR_INVALID_ACCESS, 1},
        {INVALID_HANDLE_VALUE, 'a', 0, {0, 0}, &count, ERROR_INVALID_HANDLE},
        {INVALID_HANDLE_VALUE, 'a', 1, {0, 0}, NULL, ERROR_INVALID_ACCESS, 1},
        {INVALID_HANDLE_VALUE, 'a', 1, {0, 0}, &count, ERROR_INVALID_HANDLE},
        {output_handle, 'a', 0, {0, 0}, NULL, ERROR_INVALID_ACCESS, 1},
        {output_handle, 'a', 1, {0, 0}, NULL, ERROR_INVALID_ACCESS, 1},
    };

    for (i = 0; i < ARRAY_SIZE(invalid_table); i++)
    {
        if (invalid_table[i].win7_crash)
            continue;

        SetLastError(0xdeadbeef);
        if (invalid_table[i].lpNumCharsWritten) count = 0xdeadbeef;
        ret = FillConsoleOutputCharacterA(invalid_table[i].hConsoleOutput,
                                          invalid_table[i].ch,
                                          invalid_table[i].length,
                                          invalid_table[i].coord,
                                          invalid_table[i].lpNumCharsWritten);
        ok(!ret, "[%d] Expected FillConsoleOutputCharacterA to return FALSE, got %d\n", i, ret);
        ok(GetLastError() == invalid_table[i].last_error,
           "[%d] Expected last error to be %u, got %u\n",
           i, invalid_table[i].last_error, GetLastError());
    }

    count = 0xdeadbeef;
    ret = FillConsoleOutputCharacterA(output_handle, 'a', 0, origin, &count);
    ok(ret == TRUE, "Expected FillConsoleOutputCharacterA to return TRUE, got %d\n", ret);
    ok(count == 0, "Expected count to be 0, got %u\n", count);

    count = 0xdeadbeef;
    ret = FillConsoleOutputCharacterA(output_handle, 'a', 1, origin, &count);
    ok(ret == TRUE, "Expected FillConsoleOutputCharacterA to return TRUE, got %d\n", ret);
    ok(count == 1, "Expected count to be 1, got %u\n", count);
}

static void test_FillConsoleOutputCharacterW(HANDLE output_handle)
{
    COORD origin = {0, 0};
    DWORD count;
    BOOL ret;
    int i;

    const struct
    {
        HANDLE hConsoleOutput;
        WCHAR ch;
        DWORD length;
        COORD coord;
        LPDWORD lpNumCharsWritten;
        DWORD last_error;
        int win7_crash;
    } invalid_table[] =
    {
        {NULL, 'a', 0, {0, 0}, NULL, ERROR_INVALID_ACCESS, 1},
        {NULL, 'a', 0, {0, 0}, &count, ERROR_INVALID_HANDLE},
        {NULL, 'a', 1, {0, 0}, NULL, ERROR_INVALID_ACCESS, 1},
        {NULL, 'a', 1, {0, 0}, &count, ERROR_INVALID_HANDLE},
        {INVALID_HANDLE_VALUE, 'a', 0, {0, 0}, NULL, ERROR_INVALID_ACCESS, 1},
        {INVALID_HANDLE_VALUE, 'a', 0, {0, 0}, &count, ERROR_INVALID_HANDLE},
        {INVALID_HANDLE_VALUE, 'a', 1, {0, 0}, NULL, ERROR_INVALID_ACCESS, 1},
        {INVALID_HANDLE_VALUE, 'a', 1, {0, 0}, &count, ERROR_INVALID_HANDLE},
        {output_handle, 'a', 0, {0, 0}, NULL, ERROR_INVALID_ACCESS, 1},
        {output_handle, 'a', 1, {0, 0}, NULL, ERROR_INVALID_ACCESS, 1},
    };

    for (i = 0; i < ARRAY_SIZE(invalid_table); i++)
    {
        if (invalid_table[i].win7_crash)
            continue;

        SetLastError(0xdeadbeef);
        if (invalid_table[i].lpNumCharsWritten) count = 0xdeadbeef;
        ret = FillConsoleOutputCharacterW(invalid_table[i].hConsoleOutput,
                                          invalid_table[i].ch,
                                          invalid_table[i].length,
                                          invalid_table[i].coord,
                                          invalid_table[i].lpNumCharsWritten);
        ok(!ret, "[%d] Expected FillConsoleOutputCharacterW to return FALSE, got %d\n", i, ret);
        ok(GetLastError() == invalid_table[i].last_error,
           "[%d] Expected last error to be %u, got %u\n",
           i, invalid_table[i].last_error, GetLastError());
    }

    count = 0xdeadbeef;
    ret = FillConsoleOutputCharacterW(output_handle, 'a', 0, origin, &count);
    ok(ret == TRUE, "Expected FillConsoleOutputCharacterW to return TRUE, got %d\n", ret);
    ok(count == 0, "Expected count to be 0, got %u\n", count);

    count = 0xdeadbeef;
    ret = FillConsoleOutputCharacterW(output_handle, 'a', 1, origin, &count);
    ok(ret == TRUE, "Expected FillConsoleOutputCharacterW to return TRUE, got %d\n", ret);
    ok(count == 1, "Expected count to be 1, got %u\n", count);
}

static void test_FillConsoleOutputAttribute(HANDLE output_handle)
{
    COORD origin = {0, 0};
    DWORD count;
    BOOL ret;
    int i;

    const struct
    {
        HANDLE hConsoleOutput;
        WORD attr;
        DWORD length;
        COORD coord;
        LPDWORD lpNumAttrsWritten;
        DWORD last_error;
        int win7_crash;
    } invalid_table[] =
    {
        {NULL, FOREGROUND_BLUE, 0, {0, 0}, NULL, ERROR_INVALID_ACCESS, 1},
        {NULL, FOREGROUND_BLUE, 0, {0, 0}, &count, ERROR_INVALID_HANDLE},
        {NULL, FOREGROUND_BLUE, 1, {0, 0}, NULL, ERROR_INVALID_ACCESS, 1},
        {NULL, FOREGROUND_BLUE, 1, {0, 0}, &count, ERROR_INVALID_HANDLE},
        {INVALID_HANDLE_VALUE, FOREGROUND_BLUE, 0, {0, 0}, NULL, ERROR_INVALID_ACCESS, 1},
        {INVALID_HANDLE_VALUE, FOREGROUND_BLUE, 0, {0, 0}, &count, ERROR_INVALID_HANDLE},
        {INVALID_HANDLE_VALUE, FOREGROUND_BLUE, 1, {0, 0}, NULL, ERROR_INVALID_ACCESS, 1},
        {INVALID_HANDLE_VALUE, FOREGROUND_BLUE, 1, {0, 0}, &count, ERROR_INVALID_HANDLE},
        {output_handle, FOREGROUND_BLUE, 0, {0, 0}, NULL, ERROR_INVALID_ACCESS, 1},
        {output_handle, FOREGROUND_BLUE, 1, {0, 0}, NULL, ERROR_INVALID_ACCESS, 1},
    };

    for (i = 0; i < ARRAY_SIZE(invalid_table); i++)
    {
        if (invalid_table[i].win7_crash)
            continue;

        SetLastError(0xdeadbeef);
        if (invalid_table[i].lpNumAttrsWritten) count = 0xdeadbeef;
        ret = FillConsoleOutputAttribute(invalid_table[i].hConsoleOutput,
                                         invalid_table[i].attr,
                                         invalid_table[i].length,
                                         invalid_table[i].coord,
                                         invalid_table[i].lpNumAttrsWritten);
        ok(!ret, "[%d] Expected FillConsoleOutputAttribute to return FALSE, got %d\n", i, ret);
        ok(GetLastError() == invalid_table[i].last_error,
           "[%d] Expected last error to be %u, got %u\n",
           i, invalid_table[i].last_error, GetLastError());
    }

    count = 0xdeadbeef;
    ret = FillConsoleOutputAttribute(output_handle, FOREGROUND_BLUE, 0, origin, &count);
    ok(ret == TRUE, "Expected FillConsoleOutputAttribute to return TRUE, got %d\n", ret);
    ok(count == 0, "Expected count to be 0, got %u\n", count);

    count = 0xdeadbeef;
    ret = FillConsoleOutputAttribute(output_handle, FOREGROUND_BLUE, 1, origin, &count);
    ok(ret == TRUE, "Expected FillConsoleOutputAttribute to return TRUE, got %d\n", ret);
    ok(count == 1, "Expected count to be 1, got %u\n", count);

    count = 0xdeadbeef;
    ret = FillConsoleOutputAttribute(output_handle, ~0, 1, origin, &count);
    ok(ret == TRUE, "Expected FillConsoleOutputAttribute to return TRUE, got %d\n", ret);
    ok(count == 1, "Expected count to be 1, got %u\n", count);
}

static void test_ReadConsoleOutputCharacterA(HANDLE output_handle)
{
    CHAR read;
    COORD origin = {0, 0};
    DWORD count;
    BOOL ret;
    int i;

    const struct
    {
        HANDLE hConsoleOutput;
        LPSTR lpstr;
        DWORD length;
        COORD coord;
        LPDWORD read_count;
        DWORD expected_count;
        DWORD last_error;
        int win7_crash;
    } invalid_table[] =
    {
        {NULL, NULL, 0, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {NULL, NULL, 0, {0, 0}, &count, 0, ERROR_INVALID_HANDLE},
        {NULL, NULL, 1, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {NULL, NULL, 1, {0, 0}, &count, 0, ERROR_INVALID_HANDLE, 1},
        {NULL, &read, 0, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {NULL, &read, 0, {0, 0}, &count, 0, ERROR_INVALID_HANDLE},
        {NULL, &read, 1, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {NULL, &read, 1, {0, 0}, &count, 0, ERROR_INVALID_HANDLE},
        {INVALID_HANDLE_VALUE, NULL, 0, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {INVALID_HANDLE_VALUE, NULL, 0, {0, 0}, &count, 0, ERROR_INVALID_HANDLE},
        {INVALID_HANDLE_VALUE, NULL, 1, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {INVALID_HANDLE_VALUE, NULL, 1, {0, 0}, &count, 0, ERROR_INVALID_HANDLE, 1},
        {INVALID_HANDLE_VALUE, &read, 0, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {INVALID_HANDLE_VALUE, &read, 0, {0, 0}, &count, 0, ERROR_INVALID_HANDLE},
        {INVALID_HANDLE_VALUE, &read, 1, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {INVALID_HANDLE_VALUE, &read, 1, {0, 0}, &count, 0, ERROR_INVALID_HANDLE},
        {output_handle, NULL, 0, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {output_handle, NULL, 1, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {output_handle, NULL, 1, {0, 0}, &count, 1, ERROR_INVALID_ACCESS, 1},
        {output_handle, NULL, 10, {0, 0}, &count, 10, ERROR_INVALID_ACCESS, 1},
        {output_handle, &read, 0, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {output_handle, &read, 1, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
    };

    for (i = 0; i < ARRAY_SIZE(invalid_table); i++)
    {
        if (invalid_table[i].win7_crash)
            continue;

        SetLastError(0xdeadbeef);
        if (invalid_table[i].read_count) count = 0xdeadbeef;
        ret = ReadConsoleOutputCharacterA(invalid_table[i].hConsoleOutput,
                                          invalid_table[i].lpstr,
                                          invalid_table[i].length,
                                          invalid_table[i].coord,
                                          invalid_table[i].read_count);
        ok(!ret, "[%d] Expected ReadConsoleOutputCharacterA to return FALSE, got %d\n", i, ret);
        if (invalid_table[i].read_count)
        {
            ok(count == invalid_table[i].expected_count,
               "[%d] Expected count to be %u, got %u\n",
               i, invalid_table[i].expected_count, count);
        }
        ok(GetLastError() == invalid_table[i].last_error,
           "[%d] Expected last error to be %u, got %u\n",
           i, invalid_table[i].last_error, GetLastError());
    }

    count = 0xdeadbeef;
    ret = ReadConsoleOutputCharacterA(output_handle, NULL, 0, origin, &count);
    ok(ret == TRUE, "Expected ReadConsoleOutputCharacterA to return TRUE, got %d\n", ret);
    ok(count == 0, "Expected count to be 0, got %u\n", count);

    count = 0xdeadbeef;
    ret = ReadConsoleOutputCharacterA(output_handle, &read, 0, origin, &count);
    ok(ret == TRUE, "Expected ReadConsoleOutputCharacterA to return TRUE, got %d\n", ret);
    ok(count == 0, "Expected count to be 0, got %u\n", count);

    count = 0xdeadbeef;
    ret = ReadConsoleOutputCharacterA(output_handle, &read, 1, origin, &count);
    ok(ret == TRUE, "Expected ReadConsoleOutputCharacterA to return TRUE, got %d\n", ret);
    ok(count == 1, "Expected count to be 1, got %u\n", count);

    count = 0xdeadbeef;
    origin.X = 200;
    ret = ReadConsoleOutputCharacterA(output_handle, &read, 1, origin, &count);
    ok(ret == TRUE, "Expected ReadConsoleOutputCharacterA to return TRUE, got %d\n", ret);
    ok(count == 0, "Expected count to be 0, got %u\n", count);
}

static void test_ReadConsoleOutputCharacterW(HANDLE output_handle)
{
    WCHAR read;
    COORD origin = {0, 0};
    DWORD count;
    BOOL ret;
    int i;

    const struct
    {
        HANDLE hConsoleOutput;
        LPWSTR buffer;
        DWORD length;
        COORD coord;
        LPDWORD read_count;
        DWORD expected_count;
        DWORD last_error;
        int win7_crash;
    } invalid_table[] =
    {
        {NULL, NULL, 0, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {NULL, NULL, 0, {0, 0}, &count, 0, ERROR_INVALID_HANDLE},
        {NULL, NULL, 1, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {NULL, NULL, 1, {0, 0}, &count, 0, ERROR_INVALID_HANDLE, 1},
        {NULL, &read, 0, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {NULL, &read, 0, {0, 0}, &count, 0, ERROR_INVALID_HANDLE},
        {NULL, &read, 1, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {NULL, &read, 1, {0, 0}, &count, 0, ERROR_INVALID_HANDLE},
        {INVALID_HANDLE_VALUE, NULL, 0, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {INVALID_HANDLE_VALUE, NULL, 0, {0, 0}, &count, 0, ERROR_INVALID_HANDLE},
        {INVALID_HANDLE_VALUE, NULL, 1, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {INVALID_HANDLE_VALUE, NULL, 1, {0, 0}, &count, 0, ERROR_INVALID_HANDLE, 1},
        {INVALID_HANDLE_VALUE, &read, 0, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {INVALID_HANDLE_VALUE, &read, 0, {0, 0}, &count, 0, ERROR_INVALID_HANDLE},
        {INVALID_HANDLE_VALUE, &read, 1, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {INVALID_HANDLE_VALUE, &read, 1, {0, 0}, &count, 0, ERROR_INVALID_HANDLE},
        {output_handle, NULL, 0, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {output_handle, NULL, 1, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {output_handle, NULL, 1, {0, 0}, &count, 1, ERROR_INVALID_ACCESS, 1},
        {output_handle, NULL, 10, {0, 0}, &count, 10, ERROR_INVALID_ACCESS, 1},
        {output_handle, &read, 0, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {output_handle, &read, 1, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
    };

    for (i = 0; i < ARRAY_SIZE(invalid_table); i++)
    {
        if (invalid_table[i].win7_crash)
            continue;

        SetLastError(0xdeadbeef);
        if (invalid_table[i].read_count) count = 0xdeadbeef;
        ret = ReadConsoleOutputCharacterW(invalid_table[i].hConsoleOutput,
                                          invalid_table[i].buffer,
                                          invalid_table[i].length,
                                          invalid_table[i].coord,
                                          invalid_table[i].read_count);
        ok(!ret, "[%d] Expected ReadConsoleOutputCharacterW to return FALSE, got %d\n", i, ret);
        if (invalid_table[i].read_count)
        {
            ok(count == invalid_table[i].expected_count,
               "[%d] Expected count to be %u, got %u\n",
               i, invalid_table[i].expected_count, count);
        }
        ok(GetLastError() == invalid_table[i].last_error,
           "[%d] Expected last error to be %u, got %u\n",
           i, invalid_table[i].last_error, GetLastError());
    }

    count = 0xdeadbeef;
    ret = ReadConsoleOutputCharacterW(output_handle, NULL, 0, origin, &count);
    ok(ret == TRUE, "Expected ReadConsoleOutputCharacterW to return TRUE, got %d\n", ret);
    ok(count == 0, "Expected count to be 0, got %u\n", count);

    count = 0xdeadbeef;
    ret = ReadConsoleOutputCharacterW(output_handle, &read, 0, origin, &count);
    ok(ret == TRUE, "Expected ReadConsoleOutputCharacterW to return TRUE, got %d\n", ret);
    ok(count == 0, "Expected count to be 0, got %u\n", count);

    count = 0xdeadbeef;
    ret = ReadConsoleOutputCharacterW(output_handle, &read, 1, origin, &count);
    ok(ret == TRUE, "Expected ReadConsoleOutputCharacterW to return TRUE, got %d\n", ret);
    ok(count == 1, "Expected count to be 1, got %u\n", count);

    count = 0xdeadbeef;
    origin.X = 200;
    ret = ReadConsoleOutputCharacterW(output_handle, &read, 1, origin, &count);
    ok(ret == TRUE, "Expected ReadConsoleOutputCharacterW to return TRUE, got %d\n", ret);
    ok(count == 0, "Expected count to be 0, got %u\n", count);
}

static void test_ReadConsoleOutputAttribute(HANDLE output_handle)
{
    WORD attr;
    COORD origin = {0, 0};
    DWORD count;
    BOOL ret;
    int i;

    const struct
    {
        HANDLE hConsoleOutput;
        LPWORD lpAttribute;
        DWORD length;
        COORD coord;
        LPDWORD read_count;
        DWORD expected_count;
        DWORD last_error;
        int win7_crash;
    } invalid_table[] =
    {
        {NULL, NULL, 0, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {NULL, NULL, 0, {0, 0}, &count, 0, ERROR_INVALID_HANDLE},
        {NULL, NULL, 1, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {NULL, NULL, 1, {0, 0}, &count, 0, ERROR_INVALID_HANDLE, 1},
        {NULL, &attr, 0, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {NULL, &attr, 0, {0, 0}, &count, 0, ERROR_INVALID_HANDLE},
        {NULL, &attr, 1, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {NULL, &attr, 1, {0, 0}, &count, 0, ERROR_INVALID_HANDLE},
        {INVALID_HANDLE_VALUE, NULL, 0, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {INVALID_HANDLE_VALUE, NULL, 0, {0, 0}, &count, 0, ERROR_INVALID_HANDLE},
        {INVALID_HANDLE_VALUE, NULL, 1, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {INVALID_HANDLE_VALUE, NULL, 1, {0, 0}, &count, 0, ERROR_INVALID_HANDLE, 1},
        {INVALID_HANDLE_VALUE, &attr, 0, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {INVALID_HANDLE_VALUE, &attr, 0, {0, 0}, &count, 0, ERROR_INVALID_HANDLE},
        {INVALID_HANDLE_VALUE, &attr, 1, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {INVALID_HANDLE_VALUE, &attr, 1, {0, 0}, &count, 0, ERROR_INVALID_HANDLE},
        {output_handle, NULL, 0, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {output_handle, NULL, 1, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {output_handle, NULL, 1, {0, 0}, &count, 1, ERROR_INVALID_ACCESS, 1},
        {output_handle, &attr, 0, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
        {output_handle, &attr, 1, {0, 0}, NULL, 0xdeadbeef, ERROR_INVALID_ACCESS, 1},
    };

    for (i = 0; i < ARRAY_SIZE(invalid_table); i++)
    {
        if (invalid_table[i].win7_crash)
            continue;

        SetLastError(0xdeadbeef);
        if (invalid_table[i].read_count) count = 0xdeadbeef;
        ret = ReadConsoleOutputAttribute(invalid_table[i].hConsoleOutput,
                                         invalid_table[i].lpAttribute,
                                         invalid_table[i].length,
                                         invalid_table[i].coord,
                                         invalid_table[i].read_count);
        ok(!ret, "[%d] Expected ReadConsoleOutputAttribute to return FALSE, got %d\n", i, ret);
        if (invalid_table[i].read_count)
        {
            ok(count == invalid_table[i].expected_count,
               "[%d] Expected count to be %u, got %u\n",
               i, invalid_table[i].expected_count, count);
        }
        ok(GetLastError() == invalid_table[i].last_error,
           "[%d] Expected last error to be %u, got %u\n",
           i, invalid_table[i].last_error, GetLastError());
    }

    count = 0xdeadbeef;
    ret = ReadConsoleOutputAttribute(output_handle, NULL, 0, origin, &count);
    ok(ret == TRUE, "Expected ReadConsoleOutputAttribute to return TRUE, got %d\n", ret);
    ok(count == 0, "Expected count to be 0, got %u\n", count);

    count = 0xdeadbeef;
    ret = ReadConsoleOutputAttribute(output_handle, &attr, 0, origin, &count);
    ok(ret == TRUE, "Expected ReadConsoleOutputAttribute to return TRUE, got %d\n", ret);
    ok(count == 0, "Expected count to be 0, got %u\n", count);

    count = 0xdeadbeef;
    ret = ReadConsoleOutputAttribute(output_handle, &attr, 1, origin, &count);
    ok(ret == TRUE, "Expected ReadConsoleOutputAttribute to return TRUE, got %d\n", ret);
    ok(count == 1, "Expected count to be 1, got %u\n", count);

    count = 0xdeadbeef;
    origin.X = 200;
    ret = ReadConsoleOutputAttribute(output_handle, &attr, 1, origin, &count);
    ok(ret == TRUE, "Expected ReadConsoleOutputAttribute to return TRUE, got %d\n", ret);
    ok(count == 0, "Expected count to be 1, got %u\n", count);
}

static void test_ReadConsoleOutput(HANDLE console)
{
    CONSOLE_SCREEN_BUFFER_INFO info;
    CHAR_INFO char_info_buf[2048];
    SMALL_RECT region;
    COORD size, coord;
    DWORD count;
    WCHAR ch;
    BOOL ret;

    if (skip_nt) return;

    ret = GetConsoleScreenBufferInfo(console, &info);
    ok(ret, "GetConsoleScreenBufferInfo failed: %u\n", GetLastError());

    size.X = 23;
    size.Y = 17;
    coord.X = 2;
    coord.Y = 3;
    set_region(&region, 10, 7, 15, 11);
    ret = ReadConsoleOutputW(console, char_info_buf, size, coord, &region);
    ok(ret, "ReadConsoleOutputW failed: %u\n", GetLastError());
    check_region(&region, 10, 7, 15, 11);

    size.X = 23;
    size.Y = 17;
    coord.X = 2;
    coord.Y = 3;
    set_region(&region, 200, 7, 15, 211);
    ret = ReadConsoleOutputW(console, char_info_buf, size, coord, &region);
    ok(!ret && GetLastError() == ERROR_NOT_ENOUGH_MEMORY, "ReadConsoleOutputW returned: %x(%u)\n", ret, GetLastError());
    check_region(&region, 200, 7, 15, 211);

    size.X = 23;
    size.Y = 17;
    coord.X = 2;
    coord.Y = 3;
    set_region(&region, 200, 7, 211, 8);
    ret = ReadConsoleOutputW(console, char_info_buf, size, coord, &region);
    ok((!ret && (GetLastError() == ERROR_INVALID_PARAMETER || GetLastError() == ERROR_INVALID_FUNCTION)) || broken(ret /* win8 */),
       "ReadConsoleOutputW returned: %x %u\n", ret, GetLastError());
    if (!ret && GetLastError() == ERROR_INVALID_PARAMETER) check_region(&region, 200, 7, -211, -8);

    size.X = 23;
    size.Y = 17;
    coord.X = 2;
    coord.Y = 3;
    set_region(&region, 10, 7, 9, 11);
    ret = ReadConsoleOutputW(console, char_info_buf, size, coord, &region);
    ok((!ret && (GetLastError() == ERROR_INVALID_FUNCTION || GetLastError() == ERROR_NOT_ENOUGH_MEMORY)) || broken(ret /* win8 */),
       "ReadConsoleOutputW returned: %x(%u)\n", ret, GetLastError());
    check_region(&region, 10, 7, 9, -11);

    size.X = 23;
    size.Y = 17;
    coord.X = 2;
    coord.Y = 3;
    set_region(&region, 10, 7, 11, 6);
    ret = ReadConsoleOutputW(console, char_info_buf, size, coord, &region);
    ok((!ret && (GetLastError() == ERROR_INVALID_FUNCTION || GetLastError() == ERROR_NOT_ENOUGH_MEMORY)) || broken(ret /* win8 */),
       "ReadConsoleOutputW returned: %x(%u)\n", ret, GetLastError());
    check_region(&region, 10, 7, -11, 6);

    size.X = 2;
    size.Y = 17;
    coord.X = 2;
    coord.Y = 3;
    set_region(&region, 10, 7, 15, 11);
    ret = ReadConsoleOutputW(console, char_info_buf, size, coord, &region);
    ok((!ret && (GetLastError() == ERROR_INVALID_FUNCTION || GetLastError() == ERROR_NOT_ENOUGH_MEMORY)) || broken(ret /* win8 */),
       "ReadConsoleOutputW returned: %x(%u)\n", ret, GetLastError());
    check_region(&region, 10, 7, -15, -11);

    size.X = 23;
    size.Y = 3;
    coord.X = 2;
    coord.Y = 3;
    set_region(&region, 10, 7, 15, 11);
    ret = ReadConsoleOutputW(console, char_info_buf, size, coord, &region);
    ok((!ret && (GetLastError() == ERROR_INVALID_FUNCTION || GetLastError() == ERROR_NOT_ENOUGH_MEMORY)) || broken(ret /* win8 */),
       "ReadConsoleOutputW returned: %x(%u)\n", ret, GetLastError());
    check_region(&region, 10, 7, -15, 6);

    size.X = 6;
    size.Y = 17;
    coord.X = 2;
    coord.Y = 3;
    set_region(&region, 10, 7, 15, 11);
    ret = ReadConsoleOutputW(console, char_info_buf, size, coord, &region);
    ok(ret, "ReadConsoleOutputW failed: %u\n", GetLastError());
    check_region(&region, 10, 7, 13, 11);

    size.X = 6;
    size.Y = 17;
    coord.X = 2;
    coord.Y = 3;
    set_region(&region, 10, 7, 15, 11);
    ret = ReadConsoleOutputW((HANDLE)0xdeadbeef, char_info_buf, size, coord, &region);
    ok(!ret && GetLastError() == ERROR_INVALID_HANDLE, "ReadConsoleOutputW returned: %x(%u)\n", ret, GetLastError());
    if (!skip_nt) check_region(&region, 10, 7, 13, 11);

    size.X = 16;
    size.Y = 7;
    coord.X = 2;
    coord.Y = 3;
    set_region(&region, 10, 7, 15, 11);
    ret = ReadConsoleOutputW(console, char_info_buf, size, coord, &region);
    ok(ret, "ReadConsoleOutputW failed: %u\n", GetLastError());
    check_region(&region, 10, 7, 15, 10);

    size.X = 16;
    size.Y = 7;
    coord.X = 2;
    coord.Y = 3;
    set_region(&region, info.dwSize.X - 2, 7, info.dwSize.X + 2, 7);
    ret = ReadConsoleOutputW(console, char_info_buf, size, coord, &region);
    ok(ret || GetLastError() == ERROR_INVALID_PARAMETER, "ReadConsoleOutputW failed: %u\n", GetLastError());
    if (ret) check_region(&region, info.dwSize.X - 2, 7, info.dwSize.X - 1, 7);

    coord.X = 2;
    coord.Y = 3;
    ret = WriteConsoleOutputCharacterW(console, L"xyz", 3, coord, &count);
    ok(ret, "WriteConsoleOutputCharacterW failed: %u\n", GetLastError());
    ok(count == 3, "count = %u\n", count);

    memset(char_info_buf, 0xc0, sizeof(char_info_buf));
    size.X = 16;
    size.Y = 7;
    coord.X = 5;
    coord.Y = 6;
    set_region(&region, 2, 3, 5, 3);
    ret = ReadConsoleOutputW(console, char_info_buf, size, coord, &region);
    ok(ret, "ReadConsoleOutputW failed: %u\n", GetLastError());
    check_region(&region, 2, 3, 5, 3);
    ch = char_info_buf[coord.Y * size.X + coord.X].Char.UnicodeChar;
    ok(ch == 'x', "unexpected char %c/%x\n", ch, ch);
    ch = char_info_buf[coord.Y * size.X + coord.X + 1].Char.UnicodeChar;
    ok(ch == 'y', "unexpected char %c/%x\n", ch, ch);
    ch = char_info_buf[coord.Y * size.X + coord.X + 2].Char.UnicodeChar;
    ok(ch == 'z', "unexpected char %c/%x\n", ch, ch);
}

static void test_ReadConsole(void)
{
    HANDLE std_input;
    DWORD ret, bytes;
    char buf[1024];

    std_input = GetStdHandle(STD_INPUT_HANDLE);

    SetLastError(0xdeadbeef);
    ret = GetFileSize(std_input, NULL);
    if (GetLastError() == 0xdeadbeef)
    {
        skip("stdin is redirected\n");
        return;
    }
    ok(ret == INVALID_FILE_SIZE, "expected INVALID_FILE_SIZE, got %#x\n", ret);
    ok(GetLastError() == ERROR_INVALID_HANDLE ||
       GetLastError() == ERROR_INVALID_FUNCTION, /* Win 8, 10 */
       "expected ERROR_INVALID_HANDLE, got %d\n", GetLastError());

    bytes = 0xdeadbeef;
    SetLastError(0xdeadbeef);
    ret = ReadFile(std_input, buf, -128, &bytes, NULL);
    ok(!ret, "expected 0, got %u\n", ret);
    ok(GetLastError() == ERROR_NOT_ENOUGH_MEMORY ||
       GetLastError() == ERROR_NOACCESS, /* Win 8, 10 */
       "expected ERROR_NOT_ENOUGH_MEMORY, got %d\n", GetLastError());
    ok(!bytes, "expected 0, got %u\n", bytes);

    bytes = 0xdeadbeef;
    SetLastError(0xdeadbeef);
    ret = ReadConsoleA(std_input, buf, -128, &bytes, NULL);
    ok(!ret, "expected 0, got %u\n", ret);
    ok(GetLastError() == ERROR_NOT_ENOUGH_MEMORY ||
       GetLastError() == ERROR_NOACCESS, /* Win 8, 10 */
       "expected ERROR_NOT_ENOUGH_MEMORY, got %d\n", GetLastError());
    ok(bytes == 0xdeadbeef, "expected 0xdeadbeef, got %#x\n", bytes);

    bytes = 0xdeadbeef;
    SetLastError(0xdeadbeef);
    ret = ReadConsoleW(std_input, buf, -128, &bytes, NULL);
    ok(!ret, "expected 0, got %u\n", ret);
    ok(GetLastError() == ERROR_NOT_ENOUGH_MEMORY ||
       GetLastError() == ERROR_NOACCESS, /* Win 8, 10 */
       "expected ERROR_NOT_ENOUGH_MEMORY, got %d\n", GetLastError());
    ok(bytes == 0xdeadbeef, "expected 0xdeadbeef, got %#x\n", bytes);
}

static void test_GetCurrentConsoleFont(HANDLE std_output)
{
    BOOL ret;
    CONSOLE_FONT_INFO cfi;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    short int width, height;
    HANDLE pipe1, pipe2;
    COORD c;

    memset(&cfi, 0, sizeof(CONSOLE_FONT_INFO));
    SetLastError(0xdeadbeef);
    ret = GetCurrentConsoleFont(NULL, FALSE, &cfi);
    ok(!ret, "got %d, expected 0\n", ret);
    ok(GetLastError() == ERROR_INVALID_HANDLE, "got %u, expected 6\n", GetLastError());
    ok(!cfi.dwFontSize.X, "got %d, expected 0\n", cfi.dwFontSize.X);
    ok(!cfi.dwFontSize.Y, "got %d, expected 0\n", cfi.dwFontSize.Y);

    memset(&cfi, 0, sizeof(CONSOLE_FONT_INFO));
    SetLastError(0xdeadbeef);
    ret = GetCurrentConsoleFont(NULL, TRUE, &cfi);
    ok(!ret, "got %d, expected 0\n", ret);
    ok(GetLastError() == ERROR_INVALID_HANDLE, "got %u, expected 6\n", GetLastError());
    ok(!cfi.dwFontSize.X, "got %d, expected 0\n", cfi.dwFontSize.X);
    ok(!cfi.dwFontSize.Y, "got %d, expected 0\n", cfi.dwFontSize.Y);

    memset(&cfi, 0, sizeof(CONSOLE_FONT_INFO));
    SetLastError(0xdeadbeef);
    ret = GetCurrentConsoleFont(GetStdHandle(STD_INPUT_HANDLE), FALSE, &cfi);
    ok(!ret, "got %d, expected 0\n", ret);
    ok(GetLastError() == ERROR_INVALID_HANDLE, "got %u, expected 6\n", GetLastError());
    ok(!cfi.dwFontSize.X, "got %d, expected 0\n", cfi.dwFontSize.X);
    ok(!cfi.dwFontSize.Y, "got %d, expected 0\n", cfi.dwFontSize.Y);

    memset(&cfi, 0, sizeof(CONSOLE_FONT_INFO));
    SetLastError(0xdeadbeef);
    ret = GetCurrentConsoleFont(GetStdHandle(STD_INPUT_HANDLE), TRUE, &cfi);
    ok(!ret, "got %d, expected 0\n", ret);
    ok(GetLastError() == ERROR_INVALID_HANDLE, "got %u, expected 6\n", GetLastError());
    ok(!cfi.dwFontSize.X, "got %d, expected 0\n", cfi.dwFontSize.X);
    ok(!cfi.dwFontSize.Y, "got %d, expected 0\n", cfi.dwFontSize.Y);

    CreatePipe(&pipe1, &pipe2, NULL, 0);
    memset(&cfi, 0, sizeof(CONSOLE_FONT_INFO));
    SetLastError(0xdeadbeef);
    ret = GetCurrentConsoleFont(pipe1, TRUE, &cfi);
    ok(!ret, "got %d, expected 0\n", ret);
    ok(GetLastError() == ERROR_INVALID_HANDLE, "got %u, expected 6\n", GetLastError());
    ok(!cfi.dwFontSize.X, "got %d, expected 0\n", cfi.dwFontSize.X);
    ok(!cfi.dwFontSize.Y, "got %d, expected 0\n", cfi.dwFontSize.Y);
    CloseHandle(pipe1);
    CloseHandle(pipe2);

    memset(&cfi, 0, sizeof(CONSOLE_FONT_INFO));
    SetLastError(0xdeadbeef);
    ret = GetCurrentConsoleFont(std_output, FALSE, &cfi);
    ok(ret, "got %d, expected non-zero\n", ret);
    ok(GetLastError() == 0xdeadbeef, "got %u, expected 0xdeadbeef\n", GetLastError());
    GetConsoleScreenBufferInfo(std_output, &csbi);
    width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    c = GetConsoleFontSize(std_output, cfi.nFont);
    ok(cfi.dwFontSize.X == width || cfi.dwFontSize.X == c.X /* Vista and higher */,
        "got %d, expected %d\n", cfi.dwFontSize.X, width);
    ok(cfi.dwFontSize.Y == height || cfi.dwFontSize.Y == c.Y /* Vista and higher */,
        "got %d, expected %d\n", cfi.dwFontSize.Y, height);

    memset(&cfi, 0, sizeof(CONSOLE_FONT_INFO));
    SetLastError(0xdeadbeef);
    ret = GetCurrentConsoleFont(std_output, TRUE, &cfi);
    ok(ret, "got %d, expected non-zero\n", ret);
    ok(GetLastError() == 0xdeadbeef, "got %u, expected 0xdeadbeef\n", GetLastError());
    ok(cfi.dwFontSize.X == csbi.dwMaximumWindowSize.X,
       "got %d, expected %d\n", cfi.dwFontSize.X, csbi.dwMaximumWindowSize.X);
    ok(cfi.dwFontSize.Y == csbi.dwMaximumWindowSize.Y,
       "got %d, expected %d\n", cfi.dwFontSize.Y, csbi.dwMaximumWindowSize.Y);
}

static void test_GetCurrentConsoleFontEx(HANDLE std_output)
{
    HANDLE hmod;
    BOOL (WINAPI *pGetCurrentConsoleFontEx)(HANDLE, BOOL, CONSOLE_FONT_INFOEX *);
    CONSOLE_FONT_INFO cfi;
    CONSOLE_FONT_INFOEX cfix;
    BOOL ret;
    HANDLE std_input = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE pipe1, pipe2;

    hmod = GetModuleHandleA("kernel32.dll");
    pGetCurrentConsoleFontEx = (void *)GetProcAddress(hmod, "GetCurrentConsoleFontEx");
    if (!pGetCurrentConsoleFontEx)
    {
        win_skip("GetCurrentConsoleFontEx is not available\n");
        return;
    }

    SetLastError(0xdeadbeef);
    ret = pGetCurrentConsoleFontEx(NULL, FALSE, &cfix);
    ok(!ret, "got %d, expected 0\n", ret);
    ok(GetLastError() == ERROR_INVALID_PARAMETER, "got %u, expected 87\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = pGetCurrentConsoleFontEx(NULL, TRUE, &cfix);
    ok(!ret, "got %d, expected 0\n", ret);
    ok(GetLastError() == ERROR_INVALID_PARAMETER, "got %u, expected 87\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = pGetCurrentConsoleFontEx(std_input, FALSE, &cfix);
    ok(!ret, "got %d, expected 0\n", ret);
    ok(GetLastError() == ERROR_INVALID_PARAMETER, "got %u, expected 87\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = pGetCurrentConsoleFontEx(std_input, TRUE, &cfix);
    ok(!ret, "got %d, expected 0\n", ret);
    ok(GetLastError() == ERROR_INVALID_PARAMETER, "got %u, expected 87\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = pGetCurrentConsoleFontEx(std_output, FALSE, &cfix);
    ok(!ret, "got %d, expected 0\n", ret);
    ok(GetLastError() == ERROR_INVALID_PARAMETER, "got %u, expected 87\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = pGetCurrentConsoleFontEx(std_output, TRUE, &cfix);
    ok(!ret, "got %d, expected 0\n", ret);
    ok(GetLastError() == ERROR_INVALID_PARAMETER, "got %u, expected 87\n", GetLastError());

    cfix.cbSize = sizeof(CONSOLE_FONT_INFOEX);

    SetLastError(0xdeadbeef);
    ret = pGetCurrentConsoleFontEx(NULL, FALSE, &cfix);
    ok(!ret, "got %d, expected 0\n", ret);
    ok(GetLastError() == ERROR_INVALID_HANDLE, "got %u, expected 6\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = pGetCurrentConsoleFontEx(NULL, TRUE, &cfix);
    ok(!ret, "got %d, expected 0\n", ret);
    ok(GetLastError() == ERROR_INVALID_HANDLE, "got %u, expected 6\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = pGetCurrentConsoleFontEx(std_input, FALSE, &cfix);
    ok(!ret, "got %d, expected 0\n", ret);
    ok(GetLastError() == ERROR_INVALID_HANDLE, "got %u, expected 6\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = pGetCurrentConsoleFontEx(std_input, TRUE, &cfix);
    ok(!ret, "got %d, expected 0\n", ret);
    ok(GetLastError() == ERROR_INVALID_HANDLE, "got %u, expected 6\n", GetLastError());

    CreatePipe(&pipe1, &pipe2, NULL, 0);
    memset(&cfi, 0, sizeof(CONSOLE_FONT_INFO));
    SetLastError(0xdeadbeef);
    ret = pGetCurrentConsoleFontEx(pipe1, TRUE, &cfix);
    ok(!ret, "got %d, expected 0\n", ret);
    ok(GetLastError() == ERROR_INVALID_HANDLE, "got %u, expected 6\n", GetLastError());
    CloseHandle(pipe1);
    CloseHandle(pipe2);

    SetLastError(0xdeadbeef);
    ret = pGetCurrentConsoleFontEx(std_output, FALSE, &cfix);
    ok(ret, "got %d, expected non-zero\n", ret);
    ok(GetLastError() == 0xdeadbeef, "got %u, expected 0xdeadbeef\n", GetLastError());

    memset(&cfi, 0, sizeof(CONSOLE_FONT_INFO));
    SetLastError(0xdeadbeef);
    ret = GetCurrentConsoleFont(std_output, FALSE, &cfi);
    ok(ret, "got %d, expected non-zero\n", ret);
    ok(GetLastError() == 0xdeadbeef, "got %u, expected 0xdeadbeef\n", GetLastError());

    ok(cfix.dwFontSize.X == cfi.dwFontSize.X, "expected values to match\n");
    ok(cfix.dwFontSize.Y == cfi.dwFontSize.Y, "expected values to match\n");

    SetLastError(0xdeadbeef);
    ret = pGetCurrentConsoleFontEx(std_output, TRUE, &cfix);
    ok(ret, "got %d, expected non-zero\n", ret);
    ok(GetLastError() == 0xdeadbeef, "got %u, expected 0xdeadbeef\n", GetLastError());

    memset(&cfi, 0, sizeof(CONSOLE_FONT_INFO));
    SetLastError(0xdeadbeef);
    ret = GetCurrentConsoleFont(std_output, TRUE, &cfi);
    ok(ret, "got %d, expected non-zero\n", ret);
    ok(GetLastError() == 0xdeadbeef, "got %u, expected 0xdeadbeef\n", GetLastError());

    ok(cfix.dwFontSize.X == cfi.dwFontSize.X, "expected values to match\n");
    ok(cfix.dwFontSize.Y == cfi.dwFontSize.Y, "expected values to match\n");
}

static void test_GetConsoleFontSize(HANDLE std_output)
{
    COORD c;
    DWORD index = 0;
    CONSOLE_FONT_INFO cfi;
    RECT r;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    LONG font_width, font_height;
    HMODULE hmod;
    DWORD (WINAPI *pGetNumberOfConsoleFonts)(void);
    HANDLE pipe1, pipe2;

    memset(&c, 10, sizeof(COORD));
    SetLastError(0xdeadbeef);
    c = GetConsoleFontSize(NULL, index);
    ok(GetLastError() == ERROR_INVALID_HANDLE, "got %u, expected 6\n", GetLastError());
    ok(!c.X, "got %d, expected 0\n", c.X);
    ok(!c.Y, "got %d, expected 0\n", c.Y);

    memset(&c, 10, sizeof(COORD));
    SetLastError(0xdeadbeef);
    c = GetConsoleFontSize(GetStdHandle(STD_INPUT_HANDLE), index);
    ok(GetLastError() == ERROR_INVALID_HANDLE, "got %u, expected 6\n", GetLastError());
    ok(!c.X, "got %d, expected 0\n", c.X);
    ok(!c.Y, "got %d, expected 0\n", c.Y);

    CreatePipe(&pipe1, &pipe2, NULL, 0);
    memset(&c, 10, sizeof(COORD));
    SetLastError(0xdeadbeef);
    c = GetConsoleFontSize(pipe1, index);
    ok(GetLastError() == ERROR_INVALID_HANDLE, "got %u, expected 6\n", GetLastError());
    ok(!c.X, "got %d, expected 0\n", c.X);
    ok(!c.Y, "got %d, expected 0\n", c.Y);
    CloseHandle(pipe1);
    CloseHandle(pipe2);

    GetCurrentConsoleFont(std_output, FALSE, &cfi);
    memset(&c, 10, sizeof(COORD));
    SetLastError(0xdeadbeef);
    c = GetConsoleFontSize(std_output, cfi.nFont);
    ok(GetLastError() == 0xdeadbeef, "got %u, expected 0xdeadbeef\n", GetLastError());
    GetClientRect(GetConsoleWindow(), &r);
    GetConsoleScreenBufferInfo(std_output, &csbi);
    font_width = (r.right - r.left + 1) / csbi.srWindow.Right;
    font_height = (r.bottom - r.top + 1) / csbi.srWindow.Bottom;
    ok(c.X == font_width, "got %d, expected %d\n", c.X, font_width);
    ok(c.Y == font_height, "got %d, expected %d\n", c.Y, font_height);

    hmod = GetModuleHandleA("kernel32.dll");
    pGetNumberOfConsoleFonts = (void *)GetProcAddress(hmod, "GetNumberOfConsoleFonts");
    if (!pGetNumberOfConsoleFonts)
    {
        win_skip("GetNumberOfConsoleFonts is not available\n");
        return;
    }
    index = pGetNumberOfConsoleFonts();

    memset(&c, 10, sizeof(COORD));
    SetLastError(0xdeadbeef);
    c = GetConsoleFontSize(std_output, index);
    ok(GetLastError() == ERROR_INVALID_PARAMETER || broken(GetLastError() == 0xdeadbeef) /* win10 1809 */,
        "unexpected last error %u\n", GetLastError());
    if (GetLastError() == ERROR_INVALID_PARAMETER)
    {
        ok(!c.X, "got %d, expected 0\n", c.X);
        ok(!c.Y, "got %d, expected 0\n", c.Y);
    }
}

static void test_GetLargestConsoleWindowSize(HANDLE std_output)
{
    COORD c, font;
    RECT r;
    LONG workarea_w, workarea_h, maxcon_w, maxcon_h;
    CONSOLE_SCREEN_BUFFER_INFO sbi;
    CONSOLE_FONT_INFO cfi;
    HANDLE pipe1, pipe2;
    DWORD index, i;
    HMODULE hmod;
    BOOL ret;
    DWORD (WINAPI *pGetNumberOfConsoleFonts)(void);
    BOOL (WINAPI *pSetConsoleFont)(HANDLE, DWORD);

    memset(&c, 10, sizeof(COORD));
    SetLastError(0xdeadbeef);
    c = GetLargestConsoleWindowSize(NULL);
    ok(GetLastError() == ERROR_INVALID_HANDLE, "got %u, expected 6\n", GetLastError());
    ok(!c.X, "got %d, expected 0\n", c.X);
    ok(!c.Y, "got %d, expected 0\n", c.Y);

    memset(&c, 10, sizeof(COORD));
    SetLastError(0xdeadbeef);
    c = GetLargestConsoleWindowSize(GetStdHandle(STD_INPUT_HANDLE));
    ok(GetLastError() == ERROR_INVALID_HANDLE, "got %u, expected 6\n", GetLastError());
    ok(!c.X, "got %d, expected 0\n", c.X);
    ok(!c.Y, "got %d, expected 0\n", c.Y);

    CreatePipe(&pipe1, &pipe2, NULL, 0);
    memset(&c, 10, sizeof(COORD));
    SetLastError(0xdeadbeef);
    c = GetLargestConsoleWindowSize(pipe1);
    ok(GetLastError() == ERROR_INVALID_HANDLE, "got %u, expected 6\n", GetLastError());
    ok(!c.X, "got %d, expected 0\n", c.X);
    ok(!c.Y, "got %d, expected 0\n", c.Y);
    CloseHandle(pipe1);
    CloseHandle(pipe2);

    SystemParametersInfoW(SPI_GETWORKAREA, 0, &r, 0);
    workarea_w = r.right - r.left;
    workarea_h = r.bottom - r.top - GetSystemMetrics(SM_CYCAPTION);

    GetCurrentConsoleFont(std_output, FALSE, &cfi);
    index = cfi.nFont; /* save current font index */

    hmod = GetModuleHandleA("kernel32.dll");
    pGetNumberOfConsoleFonts = (void *)GetProcAddress(hmod, "GetNumberOfConsoleFonts");
    if (!pGetNumberOfConsoleFonts)
    {
        win_skip("GetNumberOfConsoleFonts is not available\n");
        return;
    }
    pSetConsoleFont = (void *)GetProcAddress(hmod, "SetConsoleFont");
    if (!pSetConsoleFont)
    {
        win_skip("SetConsoleFont is not available\n");
        return;
    }

    for (i = 0; i < pGetNumberOfConsoleFonts(); i++)
    {
        pSetConsoleFont(std_output, i);
        memset(&c, 10, sizeof(COORD));
        SetLastError(0xdeadbeef);
        c = GetLargestConsoleWindowSize(std_output);
        ok(GetLastError() == 0xdeadbeef, "got %u, expected 0xdeadbeef\n", GetLastError());
        GetCurrentConsoleFont(std_output, FALSE, &cfi);
        font = GetConsoleFontSize(std_output, cfi.nFont);
        maxcon_w = workarea_w / font.X;
        maxcon_h = workarea_h / font.Y;
        ok(c.X == maxcon_w || c.X == maxcon_w - 1 /* Win10 */, "got %d, expected %d\n", c.X, maxcon_w);
        ok(c.Y == maxcon_h || c.Y == maxcon_h - 1 /* Win10 */, "got %d, expected %d\n", c.Y, maxcon_h);

        ret = GetConsoleScreenBufferInfo(std_output, &sbi);
        ok(ret, "GetConsoleScreenBufferInfo failed %u\n", GetLastError());
        ok(sbi.dwMaximumWindowSize.X == min(c.X, sbi.dwSize.X), "got %d, expected %d\n",
           sbi.dwMaximumWindowSize.X, min(c.X, sbi.dwSize.X));
        ok(sbi.dwMaximumWindowSize.Y == min(c.Y, sbi.dwSize.Y), "got %d, expected %d\n",
           sbi.dwMaximumWindowSize.Y, min(c.Y, sbi.dwSize.Y));
    }
    pSetConsoleFont(std_output, index); /* restore original font size */
}

static void test_GetConsoleFontInfo(HANDLE std_output)
{
    HANDLE hmod;
    BOOL (WINAPI *pGetConsoleFontInfo)(HANDLE, BOOL, DWORD, CONSOLE_FONT_INFO *);
    DWORD (WINAPI *pGetNumberOfConsoleFonts)(void);
    DWORD num_fonts, index, i;
    int memsize, win_width, win_height, tmp_w, tmp_h;
    CONSOLE_FONT_INFO *cfi;
    BOOL ret;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    COORD orig_sb_size, tmp_sb_size, orig_font, tmp_font;

    hmod = GetModuleHandleA("kernel32.dll");
    pGetConsoleFontInfo = (void *)GetProcAddress(hmod, "GetConsoleFontInfo");
    if (!pGetConsoleFontInfo)
    {
        win_skip("GetConsoleFontInfo is not available\n");
        return;
    }

    pGetNumberOfConsoleFonts = (void *)GetProcAddress(hmod, "GetNumberOfConsoleFonts");
    if (!pGetNumberOfConsoleFonts)
    {
        skip("GetNumberOfConsoleFonts is not available\n");
        return;
    }

    num_fonts = pGetNumberOfConsoleFonts();
    memsize = num_fonts * sizeof(CONSOLE_FONT_INFO);
    cfi = HeapAlloc(GetProcessHeap(), 0, memsize);
    memset(cfi, 0, memsize);

    GetConsoleScreenBufferInfo(std_output, &csbi);
    orig_sb_size = csbi.dwSize;
    tmp_sb_size.X = csbi.dwSize.X + 3;
    tmp_sb_size.Y = csbi.dwSize.Y + 5;
    SetConsoleScreenBufferSize(std_output, tmp_sb_size);

    SetLastError(0xdeadbeef);
    ret = pGetConsoleFontInfo(NULL, FALSE, 0, cfi);
    ok(!ret, "got %d, expected zero\n", ret);
    if (GetLastError() == LOWORD(E_NOTIMPL) /* win10 1709+ */ ||
        broken(GetLastError() == ERROR_GEN_FAILURE) /* win10 1607 */)
    {
        skip("GetConsoleFontInfo is not implemented\n");
        SetConsoleScreenBufferSize(std_output, orig_sb_size);
        HeapFree(GetProcessHeap(), 0, cfi);
        return;
    }
    ok(GetLastError() == ERROR_INVALID_HANDLE, "got %u, expected 6\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = pGetConsoleFontInfo(GetStdHandle(STD_INPUT_HANDLE), FALSE, 0, cfi);
    ok(!ret, "got %d, expected zero\n", ret);
    ok(GetLastError() == ERROR_INVALID_HANDLE, "got %u, expected 6\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = pGetConsoleFontInfo(std_output, FALSE, 0, cfi);
    ok(!ret, "got %d, expected zero\n", ret);
    ok(GetLastError() == 0xdeadbeef, "got %u, expected 0xdeadbeef\n", GetLastError());

    GetConsoleScreenBufferInfo(std_output, &csbi);
    win_width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    win_height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;

    GetCurrentConsoleFont(std_output, FALSE, &cfi[0]);
    index = cfi[0].nFont;
    orig_font = GetConsoleFontSize(std_output, index);

    memset(cfi, 0, memsize);
    ret = pGetConsoleFontInfo(std_output, FALSE, num_fonts, cfi);
    ok(ret, "got %d, expected non-zero\n", ret);
    ok(cfi[index].dwFontSize.X == win_width, "got %d, expected %d\n",
       cfi[index].dwFontSize.X, win_width);
    ok(cfi[index].dwFontSize.Y == win_height, "got %d, expected %d\n",
       cfi[index].dwFontSize.Y, win_height);

    for (i = 0; i < num_fonts; i++)
    {
        ok(cfi[i].nFont == i, "element out of order, got nFont %d, expected %d\n", cfi[i].nFont, i);
        tmp_font = GetConsoleFontSize(std_output, cfi[i].nFont);
        tmp_w = (double)orig_font.X / tmp_font.X * win_width;
        tmp_h = (double)orig_font.Y / tmp_font.Y * win_height;
        ok(cfi[i].dwFontSize.X == tmp_w, "got %d, expected %d\n", cfi[i].dwFontSize.X, tmp_w);
        ok(cfi[i].dwFontSize.Y == tmp_h, "got %d, expected %d\n", cfi[i].dwFontSize.Y, tmp_h);
    }

    SetLastError(0xdeadbeef);
    ret = pGetConsoleFontInfo(NULL, TRUE, 0, cfi);
    ok(!ret, "got %d, expected zero\n", ret);
    ok(GetLastError() == ERROR_INVALID_HANDLE, "got %u, expected 6\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = pGetConsoleFontInfo(GetStdHandle(STD_INPUT_HANDLE), TRUE, 0, cfi);
    ok(!ret, "got %d, expected zero\n", ret);
    ok(GetLastError() == ERROR_INVALID_HANDLE, "got %u, expected 6\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = pGetConsoleFontInfo(std_output, TRUE, 0, cfi);
    ok(!ret, "got %d, expected zero\n", ret);
    ok(GetLastError() == 0xdeadbeef, "got %u, expected 0xdeadbeef\n", GetLastError());

    memset(cfi, 0, memsize);
    ret = pGetConsoleFontInfo(std_output, TRUE, num_fonts, cfi);
    ok(ret, "got %d, expected non-zero\n", ret);
    ok(cfi[index].dwFontSize.X == csbi.dwMaximumWindowSize.X, "got %d, expected %d\n",
       cfi[index].dwFontSize.X, csbi.dwMaximumWindowSize.X);
    ok(cfi[index].dwFontSize.Y == csbi.dwMaximumWindowSize.Y, "got %d, expected %d\n",
       cfi[index].dwFontSize.Y, csbi.dwMaximumWindowSize.Y);

    for (i = 0; i < num_fonts; i++)
    {
        ok(cfi[i].nFont == i, "element out of order, got nFont %d, expected %d\n", cfi[i].nFont, i);
        tmp_font = GetConsoleFontSize(std_output, cfi[i].nFont);
        tmp_w = (double)orig_font.X / tmp_font.X * csbi.dwMaximumWindowSize.X;
        tmp_h = (double)orig_font.Y / tmp_font.Y * csbi.dwMaximumWindowSize.Y;
        ok(cfi[i].dwFontSize.X == tmp_w, "got %d, expected %d\n", cfi[i].dwFontSize.X, tmp_w);
        ok(cfi[i].dwFontSize.Y == tmp_h, "got %d, expected %d\n", cfi[i].dwFontSize.Y, tmp_h);
     }

    HeapFree(GetProcessHeap(), 0, cfi);
    SetConsoleScreenBufferSize(std_output, orig_sb_size);
}

static void test_SetConsoleFont(HANDLE std_output)
{
    HANDLE hmod;
    BOOL (WINAPI *pSetConsoleFont)(HANDLE, DWORD);
    BOOL ret;
    DWORD (WINAPI *pGetNumberOfConsoleFonts)(void);
    DWORD num_fonts;

    hmod = GetModuleHandleA("kernel32.dll");
    pSetConsoleFont = (void *)GetProcAddress(hmod, "SetConsoleFont");
    if (!pSetConsoleFont)
    {
        win_skip("SetConsoleFont is not available\n");
        return;
    }

    SetLastError(0xdeadbeef);
    ret = pSetConsoleFont(NULL, 0);
    ok(!ret, "got %d, expected zero\n", ret);
    if (GetLastError() == LOWORD(E_NOTIMPL) /* win10 1709+ */ ||
        broken(GetLastError() == ERROR_GEN_FAILURE) /* win10 1607 */)
    {
        skip("SetConsoleFont is not implemented\n");
        return;
    }
    ok(GetLastError() == ERROR_INVALID_HANDLE, "got %u, expected 6\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = pSetConsoleFont(GetStdHandle(STD_INPUT_HANDLE), 0);
    ok(!ret, "got %d, expected zero\n", ret);
    ok(GetLastError() == ERROR_INVALID_HANDLE, "got %u, expected 6\n", GetLastError());

    pGetNumberOfConsoleFonts = (void *)GetProcAddress(hmod, "GetNumberOfConsoleFonts");
    if (!pGetNumberOfConsoleFonts)
    {
        win_skip("GetNumberOfConsoleFonts is not available\n");
        return;
    }

    num_fonts = pGetNumberOfConsoleFonts();

    SetLastError(0xdeadbeef);
    ret = pSetConsoleFont(std_output, num_fonts);
    ok(!ret, "got %d, expected zero\n", ret);
    todo_wine ok(GetLastError() == ERROR_INVALID_PARAMETER, "got %u, expected 87\n", GetLastError());
}

static void test_GetConsoleScreenBufferInfoEx(HANDLE std_output)
{
    HANDLE hmod;
    BOOL (WINAPI *pGetConsoleScreenBufferInfoEx)(HANDLE, CONSOLE_SCREEN_BUFFER_INFOEX *);
    CONSOLE_SCREEN_BUFFER_INFOEX csbix;
    HANDLE pipe1, pipe2;
    BOOL ret;
    HANDLE std_input = GetStdHandle(STD_INPUT_HANDLE);

    hmod = GetModuleHandleA("kernel32.dll");
    pGetConsoleScreenBufferInfoEx = (void *)GetProcAddress(hmod, "GetConsoleScreenBufferInfoEx");
    if (!pGetConsoleScreenBufferInfoEx)
    {
        win_skip("GetConsoleScreenBufferInfoEx is not available\n");
        return;
    }

    SetLastError(0xdeadbeef);
    ret = pGetConsoleScreenBufferInfoEx(NULL, &csbix);
    ok(!ret, "got %d, expected zero\n", ret);
    ok(GetLastError() == ERROR_INVALID_PARAMETER, "got %u, expected 87\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = pGetConsoleScreenBufferInfoEx(std_input, &csbix);
    ok(!ret, "got %d, expected zero\n", ret);
    ok(GetLastError() == ERROR_INVALID_PARAMETER, "got %u, expected 87\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = pGetConsoleScreenBufferInfoEx(std_output, &csbix);
    ok(!ret, "got %d, expected zero\n", ret);
    ok(GetLastError() == ERROR_INVALID_PARAMETER, "got %u, expected 87\n", GetLastError());

    csbix.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);

    SetLastError(0xdeadbeef);
    ret = pGetConsoleScreenBufferInfoEx(NULL, &csbix);
    ok(!ret, "got %d, expected zero\n", ret);
    ok(GetLastError() == ERROR_INVALID_HANDLE, "got %u, expected 6\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = pGetConsoleScreenBufferInfoEx(std_input, &csbix);
    ok(!ret, "got %d, expected zero\n", ret);
    ok(GetLastError() == ERROR_INVALID_HANDLE, "got %u, expected 6\n", GetLastError());

    CreatePipe(&pipe1, &pipe2, NULL, 0);
    SetLastError(0xdeadbeef);
    ret = pGetConsoleScreenBufferInfoEx(std_input, &csbix);
    ok(!ret, "got %d, expected zero\n", ret);
    ok(GetLastError() == ERROR_INVALID_HANDLE, "got %u, expected 6\n", GetLastError());
    CloseHandle(pipe1);
    CloseHandle(pipe2);

    SetLastError(0xdeadbeef);
    ret = pGetConsoleScreenBufferInfoEx(std_output, &csbix);
    ok(ret, "got %d, expected non-zero\n", ret);
    ok(GetLastError() == 0xdeadbeef, "got %u, expected 0xdeadbeef\n", GetLastError());
}

static void test_FreeConsole(void)
{
    HANDLE handle;
    BOOL ret;

    ret = FreeConsole();
    ok(ret, "FreeConsole failed: %u\n", GetLastError());

    handle = CreateFileA("CONOUT$", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, 0);
    todo_wine
    ok(handle == INVALID_HANDLE_VALUE &&
       (GetLastError() == ERROR_INVALID_HANDLE || broken(GetLastError() == ERROR_ACCESS_DENIED /* winxp */)),
       "CreateFileA failed: %u\n", GetLastError());

    handle = CreateFileA("CONIN$", GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, 0);
    todo_wine
    ok(handle == INVALID_HANDLE_VALUE &&
       (GetLastError() == ERROR_INVALID_HANDLE || broken(GetLastError() == ERROR_ACCESS_DENIED /* winxp */)),
       "CreateFileA failed: %u\n", GetLastError());

    handle = CreateFileA("CON", GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, 0);
    todo_wine
    ok(handle == INVALID_HANDLE_VALUE &&
       (GetLastError() == ERROR_INVALID_HANDLE || broken(GetLastError() == ERROR_ACCESS_DENIED /* winxp */)),
       "CreateFileA failed: %u\n", GetLastError());

    handle = CreateFileA("CON", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, 0);
    todo_wine
    ok(handle == INVALID_HANDLE_VALUE &&
       (GetLastError() == ERROR_INVALID_HANDLE || broken(GetLastError() == ERROR_FILE_NOT_FOUND /* winxp */)),
       "CreateFileA failed: %u\n", GetLastError());

    if (!skip_nt)
    {
        SetStdHandle( STD_INPUT_HANDLE, (HANDLE)0xdeadbeef );
        handle = GetConsoleInputWaitHandle();
        ok(handle == (HANDLE)0xdeadbeef, "GetConsoleInputWaitHandle returned %p\n", handle);
        SetStdHandle( STD_INPUT_HANDLE, NULL );
        handle = GetConsoleInputWaitHandle();
        ok(!handle, "GetConsoleInputWaitHandle returned %p\n", handle);
    }
}

static void test_SetConsoleScreenBufferInfoEx(HANDLE std_output)
{
    BOOL ret;
    HANDLE hmod;
    HANDLE std_input = CreateFileA("CONIN$", GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, 0);
    BOOL (WINAPI *pSetConsoleScreenBufferInfoEx)(HANDLE, CONSOLE_SCREEN_BUFFER_INFOEX *);
    BOOL (WINAPI *pGetConsoleScreenBufferInfoEx)(HANDLE, CONSOLE_SCREEN_BUFFER_INFOEX *);
    CONSOLE_SCREEN_BUFFER_INFOEX info;

    hmod = GetModuleHandleA("kernel32.dll");
    pSetConsoleScreenBufferInfoEx = (void *)GetProcAddress(hmod, "SetConsoleScreenBufferInfoEx");
    pGetConsoleScreenBufferInfoEx = (void *)GetProcAddress(hmod, "GetConsoleScreenBufferInfoEx");
    if (!pSetConsoleScreenBufferInfoEx || !pGetConsoleScreenBufferInfoEx)
    {
        win_skip("SetConsoleScreenBufferInfoEx is not available\n");
        return;
    }

    memset(&info, 0, sizeof(CONSOLE_SCREEN_BUFFER_INFOEX));
    info.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
    pGetConsoleScreenBufferInfoEx(std_output, &info);

    SetLastError(0xdeadbeef);
    ret = pSetConsoleScreenBufferInfoEx(NULL, &info);
    ok(!ret, "got %d, expected zero\n", ret);
    ok(GetLastError() == ERROR_INVALID_HANDLE, "got %u, expected 6\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = pSetConsoleScreenBufferInfoEx(std_output, &info);
    ok(ret, "got %d, expected one\n", ret);
    ok(GetLastError() == 0xdeadbeef, "got %u, expected 0xdeadbeef\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = pSetConsoleScreenBufferInfoEx(std_input, &info);
    ok(!ret, "got %d, expected zero\n", ret);
    ok(GetLastError() == ERROR_INVALID_HANDLE || GetLastError() == ERROR_ACCESS_DENIED,
            "got %u, expected 5 or 6\n", GetLastError());

    info.cbSize = 0;
    SetLastError(0xdeadbeef);
    ret = pSetConsoleScreenBufferInfoEx(std_output, &info);
    ok(!ret, "got %d, expected zero\n", ret);
    ok(GetLastError() == ERROR_INVALID_PARAMETER, "got %u, expected 87\n", GetLastError());

    CloseHandle(std_input);
}

static void test_AttachConsole_child(DWORD console_pid)
{
    HANDLE pipe_in, pipe_out;
    COORD c = {0,0};
    HANDLE console;
    char buf[32];
    DWORD len;
    BOOL res;

    res = CreatePipe(&pipe_in, &pipe_out, NULL, 0);
    ok(res, "CreatePipe failed: %u\n", GetLastError());

    res = AttachConsole(console_pid);
    ok(!res && GetLastError() == ERROR_ACCESS_DENIED,
       "AttachConsole returned: %x(%u)\n", res, GetLastError());

    res = FreeConsole();
    ok(res, "FreeConsole failed: %u\n", GetLastError());

    SetStdHandle(STD_ERROR_HANDLE, pipe_out);

    res = AttachConsole(console_pid);
    ok(res, "AttachConsole failed: %u\n", GetLastError());

    ok(pipe_out != GetStdHandle(STD_ERROR_HANDLE), "std handle not set to console\n");

    console = CreateFileA("CONOUT$", GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, 0);
    ok(console != INVALID_HANDLE_VALUE, "Could not open console\n");

    res = ReadConsoleOutputCharacterA(console, buf, 6, c, &len);
    ok(res, "ReadConsoleOutputCharacterA failed: %u\n", GetLastError());
    ok(len == 6, "len = %u\n", len);
    ok(!memcmp(buf, "Parent", 6), "Unexpected console output\n");

    res = FreeConsole();
    ok(res, "FreeConsole failed: %u\n", GetLastError());

    SetStdHandle(STD_INPUT_HANDLE, pipe_in);
    SetStdHandle(STD_OUTPUT_HANDLE, pipe_out);

    res = AttachConsole(ATTACH_PARENT_PROCESS);
    ok(res, "AttachConsole failed: %u\n", GetLastError());

    ok(pipe_in != GetStdHandle(STD_INPUT_HANDLE), "std handle not set to console\n");
    ok(pipe_out != GetStdHandle(STD_OUTPUT_HANDLE), "std handle not set to console\n");

    console = CreateFileA("CONOUT$", GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, 0);
    ok(console != INVALID_HANDLE_VALUE, "Could not open console\n");

    res = ReadConsoleOutputCharacterA(console, buf, 6, c, &len);
    ok(res, "ReadConsoleOutputCharacterA failed: %u\n", GetLastError());
    ok(len == 6, "len = %u\n", len);
    ok(!memcmp(buf, "Parent", 6), "Unexpected console output\n");

    simple_write_console(console, "Child");
    CloseHandle(console);

    res = FreeConsole();
    ok(res, "FreeConsole failed: %u\n", GetLastError());

    res = CloseHandle(pipe_in);
    ok(res, "pipe_in is no longer valid\n");
    res = CloseHandle(pipe_out);
    ok(res, "pipe_out is no longer valid\n");
}

static void test_AttachConsole(HANDLE console)
{
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION info;
    char **argv, buf[MAX_PATH];
    COORD c = {0,0};
    DWORD len;
    BOOL res;

    simple_write_console(console, "Parent console");

    winetest_get_mainargs(&argv);
    sprintf(buf, "\"%s\" console attach_console %x", argv[0], GetCurrentProcessId());
    res = CreateProcessA(NULL, buf, NULL, NULL, TRUE, 0, NULL, NULL, &si, &info);
    ok(res, "CreateProcess failed: %u\n", GetLastError());
    CloseHandle(info.hThread);

    wait_child_process(info.hProcess);
    CloseHandle(info.hProcess);

    res = ReadConsoleOutputCharacterA(console, buf, 5, c, &len);
    ok(res, "ReadConsoleOutputCharacterA failed: %u\n", GetLastError());
    ok(len == 5, "len = %u\n", len);
    ok(!memcmp(buf, "Child", 5), "Unexpected console output\n");
}

START_TEST(console)
{
    static const char font_name[] = "Lucida Console";
    HANDLE hConIn, hConOut;
    BOOL ret;
    CONSOLE_SCREEN_BUFFER_INFO	sbi;
    LONG err;
    HKEY console_key;
    char old_font[LF_FACESIZE];
    BOOL delete = FALSE;
    DWORD size;
    char **argv;
    int argc;

    init_function_pointers();

    argc = winetest_get_mainargs(&argv);

    if (argc > 3 && !strcmp(argv[2], "attach_console"))
    {
        DWORD parent_pid;
        sscanf(argv[3], "%x", &parent_pid);
        test_AttachConsole_child(parent_pid);
        return;
    }

    /* be sure we have a clean console (and that's our own)
     * FIXME: this will make the test fail (currently) if we don't run
     * under X11
     * Another solution would be to rerun the test under wineconsole with
     * the curses backend
     */

    /* ReadConsoleOutputW doesn't retrieve characters from the output buffer
     * correctly for characters that don't have a glyph in the console font. So,
     * we first set the console font to Lucida Console (which has a wider
     * selection of glyphs available than the default raster fonts). We want
     * to be able to restore the original font afterwards, so don't change
     * if we can't read the original font.
     */
    err = RegOpenKeyExA(HKEY_CURRENT_USER, "Console", 0,
                        KEY_QUERY_VALUE | KEY_SET_VALUE, &console_key);
    if (err == ERROR_SUCCESS)
    {
        size = sizeof(old_font);
        err = RegQueryValueExA(console_key, "FaceName", NULL, NULL,
                               (LPBYTE) old_font, &size);
        if (err == ERROR_SUCCESS || err == ERROR_FILE_NOT_FOUND)
        {
            delete = (err == ERROR_FILE_NOT_FOUND);
            err = RegSetValueExA(console_key, "FaceName", 0, REG_SZ,
                                 (const BYTE *) font_name, sizeof(font_name));
            if (err != ERROR_SUCCESS)
                trace("Unable to change default console font, error %d\n", err);
        }
        else
        {
            trace("Unable to query default console font, error %d\n", err);
            RegCloseKey(console_key);
            console_key = NULL;
        }
    }
    else
    {
        trace("Unable to open HKCU\\Console, error %d\n", err);
        console_key = NULL;
    }

    /* Now detach and open a fresh console to play with */
    FreeConsole();
    ok(AllocConsole(), "Couldn't alloc console\n");

    /* Restore default console font if needed */
    if (console_key != NULL)
    {
        if (delete)
            err = RegDeleteValueA(console_key, "FaceName");
        else
            err = RegSetValueExA(console_key, "FaceName", 0, REG_SZ,
                                 (const BYTE *) old_font, strlen(old_font) + 1);
        ok(err == ERROR_SUCCESS, "Unable to restore default console font, error %d\n", err);
    }
    hConIn = CreateFileA("CONIN$", GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, 0);
    hConOut = CreateFileA("CONOUT$", GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, 0);

    /* now verify everything's ok */
    ok(hConIn != INVALID_HANDLE_VALUE, "Opening ConIn\n");
    ok(hConOut != INVALID_HANDLE_VALUE, "Opening ConOut\n");

    ret = GetConsoleScreenBufferInfo(hConOut, &sbi);
    ok(ret, "Getting sb info\n");
    if (!ret) return;

    /* Reduce the size of the buffer to the visible area plus 3 lines to speed
     * up the tests.
     */
    trace("Visible area: %dx%d - %dx%d Buffer size: %dx%d\n", sbi.srWindow.Left, sbi.srWindow.Top, sbi.srWindow.Right, sbi.srWindow.Bottom, sbi.dwSize.X, sbi.dwSize.Y);
    sbi.dwSize.Y = size = (sbi.srWindow.Bottom + 1) + 3;
    ret = SetConsoleScreenBufferSize(hConOut, sbi.dwSize);
    ok(ret, "Setting sb info\n");
    ret = GetConsoleScreenBufferInfo(hConOut, &sbi);
    ok(ret, "Getting sb info\n");
    ok(sbi.dwSize.Y == size, "Unexpected buffer size: %d instead of %d\n", sbi.dwSize.Y, size);
    if (!ret) return;

    test_ReadConsole();
    /* Non interactive tests */
    testCursor(hConOut, sbi.dwSize);
    /* test parameters (FIXME: test functionality) */
    testCursorInfo(hConOut);
    /* will test wrapped (on/off) & processed (on/off) strings output */
    testWrite(hConOut, sbi.dwSize);
    /* will test line scrolling at the bottom of the screen */
    /* testBottomScroll(); */
    /* will test all the scrolling operations */
    testScroll(hConOut, sbi.dwSize);
    /* will test sb creation / modification / codepage handling */
    testScreenBuffer(hConOut);
    /* Test waiting for a console handle */
    testWaitForConsoleInput(hConIn);

    /* clear duplicated console font table */
    CloseHandle(hConIn);
    CloseHandle(hConOut);
    FreeConsole();
    ok(AllocConsole(), "Couldn't alloc console\n");
    hConIn = CreateFileA("CONIN$", GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, 0);
    hConOut = CreateFileA("CONOUT$", GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, 0);
    ok(hConIn != INVALID_HANDLE_VALUE, "Opening ConIn\n");
    ok(hConOut != INVALID_HANDLE_VALUE, "Opening ConOut\n");

    testCtrlHandler();
    /* still to be done: access rights & access on objects */

    if (!pGetConsoleInputExeNameA || !pSetConsoleInputExeNameA)
        win_skip("GetConsoleInputExeNameA and/or SetConsoleInputExeNameA is not available\n");
    else
        test_GetSetConsoleInputExeName();

    test_GetConsoleProcessList();
    test_OpenConsoleW();
    test_CreateFileW();
    test_OpenCON();
    test_VerifyConsoleIoHandle(hConOut);
    test_GetSetStdHandle();
    test_GetNumberOfConsoleInputEvents(hConIn);
    test_WriteConsoleInputA(hConIn);
    test_WriteConsoleInputW(hConIn);
    test_WriteConsoleOutputCharacterA(hConOut);
    test_WriteConsoleOutputCharacterW(hConOut);
    test_WriteConsoleOutputAttribute(hConOut);
    test_WriteConsoleOutput(hConOut);
    test_FillConsoleOutputCharacterA(hConOut);
    test_FillConsoleOutputCharacterW(hConOut);
    test_FillConsoleOutputAttribute(hConOut);
    test_ReadConsoleOutputCharacterA(hConOut);
    test_ReadConsoleOutputCharacterW(hConOut);
    test_ReadConsoleOutputAttribute(hConOut);
    test_ReadConsoleOutput(hConOut);
    test_GetCurrentConsoleFont(hConOut);
    test_GetCurrentConsoleFontEx(hConOut);
    test_GetConsoleFontSize(hConOut);
    test_GetLargestConsoleWindowSize(hConOut);
    test_GetConsoleFontInfo(hConOut);
    test_SetConsoleFont(hConOut);
    test_GetConsoleScreenBufferInfoEx(hConOut);
    test_SetConsoleScreenBufferInfoEx(hConOut);
    test_AttachConsole(hConOut);
    test_FreeConsole();
}
