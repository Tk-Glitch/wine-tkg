/* Unit test suite for Path functions
 *
 * Copyright 2002 Matthew Mastracci
 * Copyright 2007-2010 Detlef Riekenberg
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

#include <stdarg.h>
#include <stdio.h>

#include "wine/test.h"
#include "windef.h"
#include "winbase.h"
#include "winreg.h"
#include "shlwapi.h"
#include "wininet.h"
#include "intshcut.h"

static const char* TEST_URL_1 = "http://www.winehq.org/tests?date=10/10/1923";
static const char* TEST_URL_2 = "http://localhost:8080/tests%2e.html?date=Mon%2010/10/1923";
static const char* TEST_URL_3 = "http://foo:bar@localhost:21/internal.php?query=x&return=y";

static const WCHAR winehqW[] = L"http://www.winehq.org/";
static const char winehqA[] = "http://www.winehq.org/";
static const CHAR untouchedA[] = "untouched";

typedef struct _TEST_URL_APPLY {
    const char * url;
    DWORD flags;
    HRESULT res;
    const char * newurl;
} TEST_URL_APPLY;

static const TEST_URL_APPLY TEST_APPLY[] = {
    {"www.winehq.org", URL_APPLY_GUESSSCHEME | URL_APPLY_DEFAULT, S_OK, "http://www.winehq.org"},
    {"www.winehq.org", URL_APPLY_GUESSSCHEME, S_OK, "http://www.winehq.org"},
    {"www.winehq.org", URL_APPLY_DEFAULT, S_OK, "http://www.winehq.org"},
    {"ftp.winehq.org", URL_APPLY_GUESSSCHEME | URL_APPLY_DEFAULT, S_OK, "ftp://ftp.winehq.org"},
    {"ftp.winehq.org", URL_APPLY_GUESSSCHEME, S_OK, "ftp://ftp.winehq.org"},
    {"ftp.winehq.org", URL_APPLY_DEFAULT, S_OK, "http://ftp.winehq.org"},
    {"winehq.org", URL_APPLY_GUESSSCHEME | URL_APPLY_DEFAULT, S_OK, "http://winehq.org"},
    {"winehq.org", URL_APPLY_GUESSSCHEME, S_FALSE},
    {"winehq.org", URL_APPLY_DEFAULT, S_OK, "http://winehq.org"},
    {"http://www.winehq.org", URL_APPLY_GUESSSCHEME, S_FALSE},
    {"http://www.winehq.org", URL_APPLY_GUESSSCHEME | URL_APPLY_FORCEAPPLY, S_FALSE},
    {"http://www.winehq.org", URL_APPLY_GUESSSCHEME | URL_APPLY_FORCEAPPLY | URL_APPLY_DEFAULT, S_OK, "http://http://www.winehq.org"},
    {"http://www.winehq.org", URL_APPLY_GUESSSCHEME | URL_APPLY_DEFAULT, S_FALSE},
    {"", URL_APPLY_GUESSSCHEME | URL_APPLY_DEFAULT, S_OK, "http://"},
    {"", URL_APPLY_GUESSSCHEME, S_FALSE},
    {"", URL_APPLY_DEFAULT, S_OK, "http://"},
    {"u:\\windows", URL_APPLY_GUESSFILE | URL_APPLY_DEFAULT, S_OK, "file:///u:/windows"},
    {"u:\\windows", URL_APPLY_GUESSFILE, S_OK, "file:///u:/windows"},
    {"u:\\windows", URL_APPLY_DEFAULT, S_OK, "http://u:\\windows"},
    {"file:///c:/windows", URL_APPLY_GUESSFILE, S_FALSE},
    {"aa:\\windows", URL_APPLY_GUESSFILE, S_FALSE},
    {"\\\\server\\share", URL_APPLY_DEFAULT, S_OK, "http://\\\\server\\share"},
    {"\\\\server\\share", URL_APPLY_GUESSFILE, S_OK, "file://server/share"},
    {"\\\\server\\share", URL_APPLY_GUESSSCHEME, S_FALSE},
    {"file://server/share", URL_APPLY_GUESSFILE, S_FALSE},
    {"file://server/share", URL_APPLY_GUESSSCHEME, S_FALSE},
};

/* ################ */

typedef struct _TEST_URL_CANONICALIZE {
    const char *url;
    DWORD flags;
    const char *expecturl;
    BOOL todo;
} TEST_URL_CANONICALIZE;

static const TEST_URL_CANONICALIZE TEST_CANONICALIZE[] = {
    {"", 0, ""},
    {"http://www.winehq.org/tests/../tests/../..", 0, "http://www.winehq.org/", TRUE},
    {"http://www.winehq.org/..", 0, "http://www.winehq.org/..", FALSE},
    {"http://www.winehq.org/tests/tests2/../../tests", 0, "http://www.winehq.org/tests", FALSE},
    {"http://www.winehq.org/tests/../tests", 0, "http://www.winehq.org/tests", FALSE},
    {"http://www.winehq.org/tests\n", URL_WININET_COMPATIBILITY|URL_ESCAPE_SPACES_ONLY|URL_ESCAPE_UNSAFE, "http://www.winehq.org/tests", FALSE},
    {"http://www.winehq.org/tests\r", URL_WININET_COMPATIBILITY|URL_ESCAPE_SPACES_ONLY|URL_ESCAPE_UNSAFE, "http://www.winehq.org/tests", FALSE},
    {"http://www.winehq.org/tests\r", 0, "http://www.winehq.org/tests", FALSE},
    {"http://www.winehq.org/tests\r", URL_DONT_SIMPLIFY, "http://www.winehq.org/tests", FALSE},
    {"http://www.winehq.org/tests/../tests/", 0, "http://www.winehq.org/tests/", FALSE},
    {"http://www.winehq.org/tests/../tests/..", 0, "http://www.winehq.org/", FALSE},
    {"http://www.winehq.org/tests/../tests/../", 0, "http://www.winehq.org/", FALSE},
    {"http://www.winehq.org/tests/..", 0, "http://www.winehq.org/", FALSE},
    {"http://www.winehq.org/tests/../", 0, "http://www.winehq.org/", FALSE},
    {"http://www.winehq.org/tests/..?query=x&return=y", 0, "http://www.winehq.org/?query=x&return=y", FALSE},
    {"http://www.winehq.org/tests/../?query=x&return=y", 0, "http://www.winehq.org/?query=x&return=y", FALSE},
    {"\tht\ttp\t://www\t.w\tineh\t\tq.or\tg\t/\ttests/..\t?\tquer\ty=x\t\t&re\tturn=y\t\t", 0, "http://www.winehq.org/?query=x&return=y", FALSE},
    {"http://www.winehq.org/tests/..#example", 0, "http://www.winehq.org/#example", FALSE},
    {"http://www.winehq.org/tests/../#example", 0, "http://www.winehq.org/#example", FALSE},
    {"http://www.winehq.org/tests\\../#example", 0, "http://www.winehq.org/#example", FALSE},
    {"http://www.winehq.org/tests/..\\#example", 0, "http://www.winehq.org/#example", FALSE},
    {"http://www.winehq.org\\tests/../#example", 0, "http://www.winehq.org/#example", FALSE},
    {"http://www.winehq.org/tests/../#example", URL_DONT_SIMPLIFY, "http://www.winehq.org/tests/../#example", FALSE},
    {"http://www.winehq.org/tests/foo bar", URL_ESCAPE_SPACES_ONLY | URL_DONT_ESCAPE_EXTRA_INFO, "http://www.winehq.org/tests/foo%20bar", FALSE},
    {"http://www.winehq.org/tests/foo%20bar", URL_UNESCAPE, "http://www.winehq.org/tests/foo bar", FALSE},
    {"http://www.winehq.org", 0, "http://www.winehq.org/", FALSE},
    {"http:///www.winehq.org", 0, "http:///www.winehq.org", FALSE},
    {"http:////www.winehq.org", 0, "http:////www.winehq.org", FALSE},
    {"file:///c:/tests/foo%20bar", URL_UNESCAPE, "file:///c:/tests/foo bar", FALSE},
    {"file:///c:/tests\\foo%20bar", URL_UNESCAPE, "file:///c:/tests/foo bar", FALSE},
    {"file:///c:/tests/foo%20bar", 0, "file:///c:/tests/foo%20bar", FALSE},
    {"file:///c:/tests/foo%20bar", URL_FILE_USE_PATHURL, "file://c:\\tests\\foo bar", FALSE},
    {"file://localhost/c:/tests/../tests/foo%20bar", URL_FILE_USE_PATHURL, "file://c:\\tests\\foo bar", FALSE},
    {"file://localhost\\c:/tests/../tests/foo%20bar", URL_FILE_USE_PATHURL, "file://c:\\tests\\foo bar", FALSE},
    {"file://localhost\\\\c:/tests/../tests/foo%20bar", URL_FILE_USE_PATHURL, "file://c:\\tests\\foo bar", FALSE},
    {"file://localhost\\c:\\tests/../tests/foo%20bar", URL_FILE_USE_PATHURL, "file://c:\\tests\\foo bar", FALSE},
    {"file://c:/tests/../tests/foo%20bar", URL_FILE_USE_PATHURL, "file://c:\\tests\\foo bar", FALSE},
    {"file://c:/tests\\../tests/foo%20bar", URL_FILE_USE_PATHURL, "file://c:\\tests\\foo bar", FALSE},
    {"file://c:/tests/foo%20bar", URL_FILE_USE_PATHURL, "file://c:\\tests\\foo bar", FALSE},
    {"file:///c://tests/foo%20bar", URL_FILE_USE_PATHURL, "file://c:\\\\tests\\foo bar", FALSE},
    {"file:///c:\\tests\\foo bar", 0, "file:///c:/tests/foo bar", FALSE},
    {"file:///c:\\tests\\foo bar", URL_DONT_SIMPLIFY, "file:///c:/tests/foo bar", FALSE},
    {"file:///c:\\tests\\foobar", 0, "file:///c:/tests/foobar", FALSE},
    {"file:///c:\\tests\\foobar", URL_WININET_COMPATIBILITY, "file://c:\\tests\\foobar", FALSE},
    {"file://home/user/file", 0, "file://home/user/file", FALSE},
    {"file:///home/user/file", 0, "file:///home/user/file", FALSE},
    {"file:////home/user/file", 0, "file://home/user/file", FALSE},
    {"file://home/user/file", URL_WININET_COMPATIBILITY, "file://\\\\home\\user\\file", FALSE},
    {"file:///home/user/file", URL_WININET_COMPATIBILITY, "file://\\home\\user\\file", FALSE},
    {"file:////home/user/file", URL_WININET_COMPATIBILITY, "file://\\\\home\\user\\file", FALSE},
    {"file://///home/user/file", URL_WININET_COMPATIBILITY, "file://\\\\home\\user\\file", FALSE},
    {"file://C:/user/file", 0, "file:///C:/user/file", FALSE},
    {"file://C:/user/file/../asdf", 0, "file:///C:/user/asdf", FALSE},
    {"file:///C:/user/file", 0, "file:///C:/user/file", FALSE},
    {"file:////C:/user/file", 0, "file:///C:/user/file", FALSE},
    {"file://C:/user/file", URL_WININET_COMPATIBILITY, "file://C:\\user\\file", FALSE},
    {"file:///C:/user/file", URL_WININET_COMPATIBILITY, "file://C:\\user\\file", FALSE},
    {"file:////C:/user/file", URL_WININET_COMPATIBILITY, "file://C:\\user\\file", FALSE},
    {"http:///www.winehq.org", 0, "http:///www.winehq.org", FALSE},
    {"http:///www.winehq.org", URL_WININET_COMPATIBILITY, "http:///www.winehq.org", FALSE},
    {"http://www.winehq.org/site/about", URL_FILE_USE_PATHURL, "http://www.winehq.org/site/about", FALSE},
    {"file_://www.winehq.org/site/about", URL_FILE_USE_PATHURL, "file_://www.winehq.org/site/about", FALSE},
    {"c:\\dir\\file", 0, "file:///c:/dir/file", FALSE},
    {"file:///c:\\dir\\file", 0, "file:///c:/dir/file", FALSE},
    {"c:dir\\file", 0, "file:///c:dir/file", FALSE},
    {"c:\\tests\\foo bar", URL_FILE_USE_PATHURL, "file://c:\\tests\\foo bar", FALSE},
    {"c:\\tests\\foo bar", 0, "file:///c:/tests/foo%20bar", FALSE},
    {"c\t:\t\\te\tsts\\fo\to \tbar\t", 0, "file:///c:/tests/foo%20bar", FALSE},
    {"res://file", 0, "res://file/", FALSE},
    {"res://file", URL_FILE_USE_PATHURL, "res://file/", FALSE},
    {"res:///c:/tests/foo%20bar", URL_UNESCAPE, "res:///c:/tests/foo bar", FALSE},
    {"res:///c:/tests\\foo%20bar", URL_UNESCAPE, "res:///c:/tests\\foo bar", FALSE},
    {"res:///c:/tests/foo%20bar", 0, "res:///c:/tests/foo%20bar", FALSE},
    {"res:///c:/tests/foo%20bar", URL_FILE_USE_PATHURL, "res:///c:/tests/foo%20bar", FALSE},
    {"res://c:/tests/../tests/foo%20bar", URL_FILE_USE_PATHURL, "res://c:/tests/foo%20bar", FALSE},
    {"res://c:/tests\\../tests/foo%20bar", URL_FILE_USE_PATHURL, "res://c:/tests/foo%20bar", FALSE},
    {"res://c:/tests/foo%20bar", URL_FILE_USE_PATHURL, "res://c:/tests/foo%20bar", FALSE},
    {"res:///c://tests/foo%20bar", URL_FILE_USE_PATHURL, "res:///c://tests/foo%20bar", FALSE},
    {"res:///c:\\tests\\foo bar", 0, "res:///c:\\tests\\foo bar", FALSE},
    {"res:///c:\\tests\\foo bar", URL_DONT_SIMPLIFY, "res:///c:\\tests\\foo bar", FALSE},
    {"res://c:\\tests\\foo bar/res", URL_FILE_USE_PATHURL, "res://c:\\tests\\foo bar/res", FALSE},
    {"res://c:\\tests/res\\foo%20bar/strange\\sth", 0, "res://c:\\tests/res\\foo%20bar/strange\\sth", FALSE},
    {"res://c:\\tests/res\\foo%20bar/strange\\sth", URL_FILE_USE_PATHURL, "res://c:\\tests/res\\foo%20bar/strange\\sth", FALSE},
    {"res://c:\\tests/res\\foo%20bar/strange\\sth", URL_UNESCAPE, "res://c:\\tests/res\\foo bar/strange\\sth", FALSE},
    {"/A/../B/./C/../../test_remove_dot_segments", 0, "/test_remove_dot_segments", FALSE},
    {"/A/../B/./C/../../test_remove_dot_segments", URL_FILE_USE_PATHURL, "/test_remove_dot_segments", FALSE},
    {"/A/../B/./C/../../test_remove_dot_segments", URL_WININET_COMPATIBILITY, "/test_remove_dot_segments", FALSE},
    {"/A/B\\C/D\\E", 0, "/A/B\\C/D\\E", FALSE},
    {"/A/B\\C/D\\E", URL_FILE_USE_PATHURL, "/A/B\\C/D\\E", FALSE},
    {"/A/B\\C/D\\E", URL_WININET_COMPATIBILITY, "/A/B\\C/D\\E", FALSE},
    {"///A/../B", 0, "///B", FALSE},
    {"///A/../B", URL_FILE_USE_PATHURL, "///B", FALSE},
    {"///A/../B", URL_WININET_COMPATIBILITY, "///B", FALSE},
    {"A", 0, "A", FALSE},
    {"../A", 0, "../A", FALSE},
    {"./A", 0, "A", FALSE},
    {"./A/./B", 0, "A/B", FALSE},
    {"./A", URL_DONT_SIMPLIFY, "./A", FALSE},
    {"A/./B", 0, "A/B", TRUE},
    {"A/../B", 0, "B", TRUE},
    {"A/../B/./../C", 0, "C", TRUE},
    {"A/../B/./../C", URL_DONT_SIMPLIFY, "A/../B/./../C", FALSE},
    {".\\A", 0, ".\\A", FALSE},
    {"A\\.\\B", 0, "A\\.\\B", FALSE},
    {".", 0, "/", TRUE},
    {"./A", 0, "A", TRUE},
    {"A/./B", 0, "A/B", TRUE},
    {"/:test\\", 0, "/:test\\", TRUE},
    {"/uri-res/N2R?urn:sha1:B3K", URL_DONT_ESCAPE_EXTRA_INFO | URL_WININET_COMPATIBILITY /*0x82000000*/, "/uri-res/N2R?urn:sha1:B3K", FALSE} /*LimeWire online installer calls this*/,
    {"http:www.winehq.org/dir/../index.html", 0, "http:www.winehq.org/index.html"},
    {"http://localhost/test.html", URL_FILE_USE_PATHURL, "http://localhost/test.html"},
    {"http://localhost/te%20st.html", URL_FILE_USE_PATHURL, "http://localhost/te%20st.html"},
    {"http://www.winehq.org/%E6%A1%9C.html", URL_FILE_USE_PATHURL, "http://www.winehq.org/%E6%A1%9C.html"},
    {"mk:@MSITStore:C:\\Program Files/AutoCAD 2008\\Help/acad_acg.chm::/WSfacf1429558a55de1a7524c1004e616f8b-322b.htm", 0, "mk:@MSITStore:C:\\Program Files/AutoCAD 2008\\Help/acad_acg.chm::/WSfacf1429558a55de1a7524c1004e616f8b-322b.htm"},
    {"ftp:@MSITStore:C:\\Program Files/AutoCAD 2008\\Help/acad_acg.chm::/WSfacf1429558a55de1a7524c1004e616f8b-322b.htm", 0, "ftp:@MSITStore:C:\\Program Files/AutoCAD 2008\\Help/acad_acg.chm::/WSfacf1429558a55de1a7524c1004e616f8b-322b.htm"},
    {"file:@MSITStore:C:\\Program Files/AutoCAD 2008\\Help/acad_acg.chm::/WSfacf1429558a55de1a7524c1004e616f8b-322b.htm", 0, "file:@MSITStore:C:/Program Files/AutoCAD 2008/Help/acad_acg.chm::/WSfacf1429558a55de1a7524c1004e616f8b-322b.htm"},
    {"http:@MSITStore:C:\\Program Files/AutoCAD 2008\\Help/acad_acg.chm::/WSfacf1429558a55de1a7524c1004e616f8b-322b.htm", 0, "http:@MSITStore:C:/Program Files/AutoCAD 2008/Help/acad_acg.chm::/WSfacf1429558a55de1a7524c1004e616f8b-322b.htm"},
    {"http:@MSITStore:C:\\Program Files/AutoCAD 2008\\Help/acad_acg.chm::/WSfacf1429558a55de1a7524c1004e616f8b-322b.htm", URL_FILE_USE_PATHURL, "http:@MSITStore:C:/Program Files/AutoCAD 2008/Help/acad_acg.chm::/WSfacf1429558a55de1a7524c1004e616f8b-322b.htm"},
    {"mk:@MSITStore:C:\\Program Files/AutoCAD 2008\\Help/acad_acg.chm::/WSfacf1429558a55de1a7524c1004e616f8b-322b.htm", URL_FILE_USE_PATHURL, "mk:@MSITStore:C:\\Program Files/AutoCAD 2008\\Help/acad_acg.chm::/WSfacf1429558a55de1a7524c1004e616f8b-322b.htm"},
};

/* ################ */

typedef struct _TEST_URL_ESCAPE {
    const char *url;
    DWORD flags;
    const char *expecturl;
} TEST_URL_ESCAPE;

static const TEST_URL_ESCAPE TEST_ESCAPE[] = {
    {"http://www.winehq.org/tests0", 0, "http://www.winehq.org/tests0"},
    {"http://www.winehq.org/tests1\n", 0, "http://www.winehq.org/tests1%0A"},
    {"http://www.winehq.org/tests2\r", 0, "http://www.winehq.org/tests2%0D"},
    {"http://www.winehq.org/tests3\r", URL_ESCAPE_SPACES_ONLY|URL_ESCAPE_UNSAFE, "http://www.winehq.org/tests3\r"},
    {"http://www.winehq.org/tests4\r", URL_ESCAPE_SPACES_ONLY, "http://www.winehq.org/tests4\r"},
    {"http://www.winehq.org/tests5\r", URL_WININET_COMPATIBILITY|URL_ESCAPE_SPACES_ONLY, "http://www.winehq.org/tests5\r"},
    {"/direct/swhelp/series6/6.2i_latestservicepack.dat\r", URL_ESCAPE_SPACES_ONLY, "/direct/swhelp/series6/6.2i_latestservicepack.dat\r"},

    {"file://////foo/bar\\baz", 0, "file://foo/bar/baz"},
    {"file://///foo/bar\\baz", 0, "file://foo/bar/baz"},
    {"file:////foo/bar\\baz", 0, "file://foo/bar/baz"},
    {"file:///localhost/foo/bar\\baz", 0, "file:///localhost/foo/bar/baz"},
    {"file:///foo/bar\\baz", 0, "file:///foo/bar/baz"},
    {"file://loCalHost/foo/bar\\baz", 0, "file:///foo/bar/baz"},
    {"file://foo/bar\\baz", 0, "file://foo/bar/baz"},
    {"file:/localhost/foo/bar\\baz", 0, "file:///localhost/foo/bar/baz"},
    {"file:/foo/bar\\baz", 0, "file:///foo/bar/baz"},
    {"file:foo/bar\\baz", 0, "file:foo/bar/baz"},
    {"file:\\foo/bar\\baz", 0, "file:///foo/bar/baz"},
    {"file:\\\\foo/bar\\baz", 0, "file://foo/bar/baz"},
    {"file:\\\\\\foo/bar\\baz", 0, "file:///foo/bar/baz"},
    {"file:\\\\localhost\\foo/bar\\baz", 0, "file:///foo/bar/baz"},
    {"file:///f oo/b?a r\\baz", 0, "file:///f%20oo/b?a r\\baz"},
    {"file:///foo/b#a r\\baz", 0, "file:///foo/b%23a%20r/baz"},
    {"file:///f o^&`{}|][\"<>\\%o/b#a r\\baz", 0, "file:///f%20o%5E%26%60%7B%7D%7C%5D%5B%22%3C%3E/%o/b%23a%20r/baz"},
    {"file:///f o%o/b?a r\\b%az", URL_ESCAPE_PERCENT, "file:///f%20o%25o/b?a r\\b%az"},
    {"file:/foo/bar\\baz", URL_ESCAPE_SEGMENT_ONLY, "file:%2Ffoo%2Fbar%5Cbaz"},

    {"foo/b%ar\\ba?z\\", URL_ESCAPE_SEGMENT_ONLY, "foo%2Fb%ar%5Cba%3Fz%5C"},
    {"foo/b%ar\\ba?z\\", URL_ESCAPE_PERCENT | URL_ESCAPE_SEGMENT_ONLY, "foo%2Fb%25ar%5Cba%3Fz%5C"},
    {"foo/bar\\ba?z\\", 0, "foo/bar%5Cba?z\\"},
    {"/foo/bar\\ba?z\\", 0, "/foo/bar%5Cba?z\\"},
    {"/foo/bar\\ba#z\\", 0, "/foo/bar%5Cba#z\\"},
    {"/foo/%5C", 0, "/foo/%5C"},
    {"/foo/%5C", URL_ESCAPE_PERCENT, "/foo/%255C"},

    {"http://////foo/bar\\baz", 0, "http://////foo/bar/baz"},
    {"http://///foo/bar\\baz", 0, "http://///foo/bar/baz"},
    {"http:////foo/bar\\baz", 0, "http:////foo/bar/baz"},
    {"http:///foo/bar\\baz", 0, "http:///foo/bar/baz"},
    {"http://localhost/foo/bar\\baz", 0, "http://localhost/foo/bar/baz"},
    {"http://foo/bar\\baz", 0, "http://foo/bar/baz"},
    {"http:/foo/bar\\baz", 0, "http:/foo/bar/baz"},
    {"http:foo/bar\\ba?z\\", 0, "http:foo%2Fbar%2Fba?z\\"},
    {"http:foo/bar\\ba#z\\", 0, "http:foo%2Fbar%2Fba#z\\"},
    {"http:\\foo/bar\\baz", 0, "http:/foo/bar/baz"},
    {"http:\\\\foo/bar\\baz", 0, "http://foo/bar/baz"},
    {"http:\\\\\\foo/bar\\baz", 0, "http:///foo/bar/baz"},
    {"http:\\\\\\\\foo/bar\\baz", 0, "http:////foo/bar/baz"},
    {"http:/fo ?o/b ar\\baz", 0, "http:/fo%20?o/b ar\\baz"},
    {"http:fo ?o/b ar\\baz", 0, "http:fo%20?o/b ar\\baz"},
    {"http:/foo/bar\\baz", URL_ESCAPE_SEGMENT_ONLY, "http:%2Ffoo%2Fbar%5Cbaz"},

    {"https://foo/bar\\baz", 0, "https://foo/bar/baz"},
    {"https:/foo/bar\\baz", 0, "https:/foo/bar/baz"},
    {"https:\\foo/bar\\baz", 0, "https:/foo/bar/baz"},

    {"foo:////foo/bar\\baz", 0, "foo:////foo/bar%5Cbaz"},
    {"foo:///foo/bar\\baz", 0, "foo:///foo/bar%5Cbaz"},
    {"foo://localhost/foo/bar\\baz", 0, "foo://localhost/foo/bar%5Cbaz"},
    {"foo://foo/bar\\baz", 0, "foo://foo/bar%5Cbaz"},
    {"foo:/foo/bar\\baz", 0, "foo:/foo/bar%5Cbaz"},
    {"foo:foo/bar\\baz", 0, "foo:foo%2Fbar%5Cbaz"},
    {"foo:\\foo/bar\\baz", 0, "foo:%5Cfoo%2Fbar%5Cbaz"},
    {"foo:/foo/bar\\ba?\\z", 0, "foo:/foo/bar%5Cba?\\z"},
    {"foo:/foo/bar\\ba#\\z", 0, "foo:/foo/bar%5Cba#\\z"},

    {"mailto:/fo/o@b\\%a?\\r.b#\\az", 0, "mailto:%2Ffo%2Fo@b%5C%a%3F%5Cr.b%23%5Caz"},
    {"mailto:fo/o@b\\%a?\\r.b#\\az", 0, "mailto:fo%2Fo@b%5C%a%3F%5Cr.b%23%5Caz"},
    {"mailto:fo/o@b\\%a?\\r.b#\\az", URL_ESCAPE_PERCENT, "mailto:fo%2Fo@b%5C%25a%3F%5Cr.b%23%5Caz"},

    {"ftp:fo/o@bar.baz/foo/bar", 0, "ftp:fo%2Fo@bar.baz%2Ffoo%2Fbar"},
    {"ftp:/fo/o@bar.baz/foo/bar", 0, "ftp:/fo/o@bar.baz/foo/bar"},
    {"ftp://fo/o@bar.baz/fo?o\\bar", 0, "ftp://fo/o@bar.baz/fo?o\\bar"},
    {"ftp://fo/o@bar.baz/fo#o\\bar", 0, "ftp://fo/o@bar.baz/fo#o\\bar"},
    {"ftp://localhost/o@bar.baz/fo#o\\bar", 0, "ftp://localhost/o@bar.baz/fo#o\\bar"},
    {"ftp:///fo/o@bar.baz/foo/bar", 0, "ftp:///fo/o@bar.baz/foo/bar"},
    {"ftp:////fo/o@bar.baz/foo/bar", 0, "ftp:////fo/o@bar.baz/foo/bar"},

    {"ftp\x1f\1end/", 0, "ftp%1F%01end/"}
};

typedef struct _TEST_URL_ESCAPEW {
    const WCHAR url[INTERNET_MAX_URL_LENGTH];
    DWORD flags;
    const WCHAR expecturl[INTERNET_MAX_URL_LENGTH];
    const WCHAR win7url[INTERNET_MAX_URL_LENGTH];  /* <= Win7 */
} TEST_URL_ESCAPEW;

static const TEST_URL_ESCAPEW TEST_ESCAPEW[] = {
    {L" <>\"", URL_ESCAPE_AS_UTF8, L"%20%3C%3E%22"},
    {L"{}|\\", URL_ESCAPE_AS_UTF8, L"%7B%7D%7C%5C"},
    {L"^][`", URL_ESCAPE_AS_UTF8, L"%5E%5D%5B%60"},
    {L"&/?#", URL_ESCAPE_AS_UTF8, L"%26/?#"},
    {L"Mass", URL_ESCAPE_AS_UTF8, L"Mass"},

    /* broken < Win8/10 */
    {L"Ma\xdf", URL_ESCAPE_AS_UTF8, L"Ma%C3%9F", L"Ma%DF"},
    {L"\xd841\xdf0e", URL_ESCAPE_AS_UTF8, L"%F0%A0%9C%8E", L"%EF%BF%BD%EF%BF%BD"}, /* 0x2070E */
    {L"\xd85e\xde3e", URL_ESCAPE_AS_UTF8, L"%F0%A7%A8%BE", L"%EF%BF%BD%EF%BF%BD"}, /* 0x27A3E */
    {L"\xd85e", URL_ESCAPE_AS_UTF8, L"%EF%BF%BD", L"\xd85e"},
    {L"\xd85eQ", URL_ESCAPE_AS_UTF8, L"%EF%BF%BDQ", L"\xd85eQ"},
    {L"\xdc00", URL_ESCAPE_AS_UTF8, L"%EF%BF%BD", L"\xdc00"},
    {L"\xffff", URL_ESCAPE_AS_UTF8, L"%EF%BF%BF", L"\xffff"},
};

/* ################ */

typedef struct _TEST_URL_COMBINE {
    const char *url1;
    const char *url2;
    DWORD flags;
    const char *expecturl;
    BOOL todo;
} TEST_URL_COMBINE;

static const TEST_URL_COMBINE TEST_COMBINE[] = {
    {"http://www.winehq.org/tests", "tests1", 0, "http://www.winehq.org/tests1"},
    {"http://www.%77inehq.org/tests", "tests1", 0, "http://www.%77inehq.org/tests1"},
    /*FIXME {"http://www.winehq.org/tests", "../tests2", 0, "http://www.winehq.org/tests2"},*/
    {"http://www.winehq.org/tests/", "../tests3", 0, "http://www.winehq.org/tests3"},
    {"http://www.winehq.org/tests/test1", "test2", 0, "http://www.winehq.org/tests/test2"},
    {"http://www.winehq.org/tests/../tests", "tests4", 0, "http://www.winehq.org/tests4"},
    {"http://www.winehq.org/tests/../tests/", "tests5", 0, "http://www.winehq.org/tests/tests5"},
    {"http://www.winehq.org/tests/../tests/", "/tests6/..", 0, "http://www.winehq.org/"},
    {"http://www.winehq.org/tests/../tests/..", "tests7/..", 0, "http://www.winehq.org/"},
    {"http://www.winehq.org/tests/?query=x&return=y", "tests8", 0, "http://www.winehq.org/tests/tests8"},
    {"http://www.winehq.org/tests/#example", "tests9", 0, "http://www.winehq.org/tests/tests9"},
    {"http://www.winehq.org/tests/../tests/", "/tests10/..", URL_DONT_SIMPLIFY, "http://www.winehq.org/tests10/.."},
    {"http://www.winehq.org/tests/../", "tests11", URL_DONT_SIMPLIFY, "http://www.winehq.org/tests/../tests11"},
    {"http://www.winehq.org/test12", "#", 0, "http://www.winehq.org/test12#"},
    {"http://www.winehq.org/test13#aaa", "#bbb", 0, "http://www.winehq.org/test13#bbb"},
    {"http://www.winehq.org/test14#aaa/bbb#ccc", "#", 0, "http://www.winehq.org/test14#"},
    {"http://www.winehq.org/tests/?query=x/y/z", "tests15", 0, "http://www.winehq.org/tests/tests15"},
    {"http://www.winehq.org/tests/?query=x/y/z#example", "tests16", 0, "http://www.winehq.org/tests/tests16"},
    {"http://www.winehq.org/tests17", ".", 0, "http://www.winehq.org/"},
    {"http://www.winehq.org/tests18/test", ".", 0, "http://www.winehq.org/tests18/"},
    {"file:///C:\\dir\\file.txt", "test.txt", 0, "file:///C:/dir/test.txt"},
    {"file:///C:\\dir\\file.txt#hash\\hash", "test.txt", 0, "file:///C:/dir/file.txt#hash/test.txt"},
    {"file:///C:\\dir\\file.html#hash\\hash", "test.html", 0, "file:///C:/dir/test.html"},
    {"file:///C:\\dir\\file.htm#hash\\hash", "test.htm", 0, "file:///C:/dir/test.htm"},
    {"file:///C:\\dir\\file.hTmL#hash\\hash", "test.hTmL", 0, "file:///C:/dir/test.hTmL"},
    {"file:///C:\\dir.html\\file.txt#hash\\hash", "test.txt", 0, "file:///C:/dir.html/file.txt#hash/test.txt"},
    {"C:\\winehq\\winehq.txt", "C:\\Test\\test.txt", 0, "file:///C:/Test/test.txt"},
    {"http://www.winehq.org/test/", "test%20file.txt", 0, "http://www.winehq.org/test/test%20file.txt"},
    {"http://www.winehq.org/test/", "test%20file.txt", URL_FILE_USE_PATHURL, "http://www.winehq.org/test/test%20file.txt"},
    {"http://www.winehq.org%2ftest/", "test%20file.txt", URL_FILE_USE_PATHURL, "http://www.winehq.org%2ftest/test%20file.txt"},
    {"xxx:@MSITStore:file.chm/file.html", "dir/file", 0, "xxx:dir/file"},
    {"mk:@MSITStore:file.chm::/file.html", "/dir/file", 0, "mk:@MSITStore:file.chm::/dir/file"},
    {"mk:@MSITStore:file.chm::/file.html", "mk:@MSITStore:file.chm::/dir/file", 0, "mk:@MSITStore:file.chm::/dir/file"},
    {"foo:today", "foo:calendar", 0, "foo:calendar"},
    {"foo:today", "bar:calendar", 0, "bar:calendar"},
    {"foo:/today", "foo:calendar", 0, "foo:/calendar"},
    {"Foo:/today/", "fOo:calendar", 0, "foo:/today/calendar"},
    {"mk:@MSITStore:dir/test.chm::dir/index.html", "image.jpg", 0, "mk:@MSITStore:dir/test.chm::dir/image.jpg"},
    {"mk:@MSITStore:dir/test.chm::dir/dir2/index.html", "../image.jpg", 0, "mk:@MSITStore:dir/test.chm::dir/image.jpg"},
    /* UrlCombine case 2 tests.  Schemes do not match */
    {"outbind://xxxxxxxxx", "http://wine1/dir", 0, "http://wine1/dir"},
    {"xxxx://xxxxxxxxx", "http://wine2/dir", 0, "http://wine2/dir"},
    {"ftp://xxxxxxxxx/", "http://wine3/dir", 0, "http://wine3/dir"},
    {"outbind://xxxxxxxxx", "http://wine4/dir", URL_PLUGGABLE_PROTOCOL, "http://wine4/dir"},
    {"xxx://xxxxxxxxx", "http://wine5/dir", URL_PLUGGABLE_PROTOCOL, "http://wine5/dir"},
    {"ftp://xxxxxxxxx/", "http://wine6/dir", URL_PLUGGABLE_PROTOCOL, "http://wine6/dir"},
    {"http://xxxxxxxxx", "outbind://wine7/dir", 0, "outbind://wine7/dir"},
    {"xxx://xxxxxxxxx", "ftp://wine8/dir", 0, "ftp://wine8/dir"},
    {"ftp://xxxxxxxxx/", "xxx://wine9/dir", 0, "xxx://wine9/dir"},
    {"http://xxxxxxxxx", "outbind://wine10/dir", URL_PLUGGABLE_PROTOCOL, "outbind://wine10/dir"},
    {"xxx://xxxxxxxxx", "ftp://wine11/dir", URL_PLUGGABLE_PROTOCOL, "ftp://wine11/dir"},
    {"ftp://xxxxxxxxx/", "xxx://wine12/dir", URL_PLUGGABLE_PROTOCOL, "xxx://wine12/dir"},
    {"http://xxxxxxxxx", "outbind:wine13/dir", 0, "outbind:wine13/dir"},
    {"xxx://xxxxxxxxx", "ftp:wine14/dir", 0, "ftp:wine14/dir"},
    {"ftp://xxxxxxxxx/", "xxx:wine15/dir", 0, "xxx:wine15/dir"},
    {"outbind://xxxxxxxxx/", "http:wine16/dir", 0, "http:wine16/dir"},
    {"http://xxxxxxxxx", "outbind:wine17/dir", URL_PLUGGABLE_PROTOCOL, "outbind:wine17/dir"},
    {"xxx://xxxxxxxxx", "ftp:wine18/dir", URL_PLUGGABLE_PROTOCOL, "ftp:wine18/dir"},
    {"ftp://xxxxxxxxx/", "xXx:wine19/dir", URL_PLUGGABLE_PROTOCOL, "xxx:wine19/dir"},
    {"outbind://xxxxxxxxx/", "http:wine20/dir", URL_PLUGGABLE_PROTOCOL, "http:wine20/dir"},
    {"file:///c:/dir/file.txt", "index.html?test=c:/abc", URL_ESCAPE_SPACES_ONLY|URL_DONT_ESCAPE_EXTRA_INFO, "file:///c:/dir/index.html?test=c:/abc"}
};

/* ################ */

static const struct {
    const char *path;
    const char *url;
    DWORD ret;
} TEST_URLFROMPATH [] = {
    {"foo", "file:foo", S_OK},
    {"foo\\bar", "file:foo/bar", S_OK},
    {"\\foo\\bar", "file:///foo/bar", S_OK},
    {"c:\\foo\\bar", "file:///c:/foo/bar", S_OK},
    {"c:foo\\bar", "file:///c:foo/bar", S_OK},
    {"c:\\foo/b a%r", "file:///c:/foo/b%20a%25r", S_OK},
    {"c:\\foo\\foo bar", "file:///c:/foo/foo%20bar", S_OK},
    {"file:///c:/foo/bar", "file:///c:/foo/bar", S_FALSE},
    {"xx:c:\\foo\\bar", "xx:c:\\foo\\bar", S_FALSE}
};

/* ################ */

static struct {
    char url[30];
    const char *expect;
} TEST_URL_UNESCAPE[] = {
    {"file://foo/bar", "file://foo/bar"},
    {"file://fo%20o%5Ca/bar", "file://fo o\\a/bar"},
    {"file://%24%25foobar", "file://$%foobar"}
};

/* ################ */

static const  struct {
    const char *path;
    BOOL expect;
} TEST_PATH_IS_URL[] = {
    {"http://foo/bar", TRUE},
    {"c:\\foo\\bar", FALSE},
    {"foo://foo/bar", TRUE},
    {"foo\\bar", FALSE},
    {"foo.bar", FALSE},
    {"bogusscheme:", TRUE},
    {"http:partial", TRUE}
};

/* ################ */

static const struct {
    const char *url;
    BOOL expectOpaque;
    BOOL expectFile;
} TEST_URLIS_ATTRIBS[] = {
    {	"ftp:",						FALSE,	FALSE	},
    {	"http:",					FALSE,	FALSE	},
    {	"gopher:",					FALSE,	FALSE	},
    {	"mailto:",					TRUE,	FALSE	},
    {	"news:",					FALSE,	FALSE	},
    {	"nntp:",					FALSE,	FALSE	},
    {	"telnet:",					FALSE,	FALSE	},
    {	"wais:",					FALSE,	FALSE	},
    {	"file:",					FALSE,	TRUE	},
    {	"mk:",						FALSE,	FALSE	},
    {	"https:",					FALSE,	FALSE	},
    {	"shell:",					TRUE,	FALSE	},
    {	"https:",					FALSE,	FALSE	},
    {   "snews:",					FALSE,	FALSE	},
    {   "local:",					FALSE,	FALSE	},
    {	"javascript:",					TRUE,	FALSE	},
    {	"vbscript:",					TRUE,	FALSE	},
    {	"about:",					TRUE,	FALSE	},
    {   "res:",						FALSE,	FALSE	},
    {	"bogusscheme:",					FALSE,	FALSE	},
    {	"file:\\\\e:\\b\\c",				FALSE,	TRUE	},
    {	"file://e:/b/c",				FALSE,	TRUE	},
    {	"http:partial",					FALSE,	FALSE	},
    {	"mailto://www.winehq.org/test.html",		TRUE,	FALSE	},
    {	"file:partial",					FALSE,	TRUE	},
    {	"File:partial",					FALSE,	TRUE	},
};

/* ########################### */

static LPWSTR GetWideString(const char* szString)
{
  LPWSTR wszString = HeapAlloc(GetProcessHeap(), 0, (2*INTERNET_MAX_URL_LENGTH) * sizeof(WCHAR));

  MultiByteToWideChar(CP_ACP, 0, szString, -1, wszString, INTERNET_MAX_URL_LENGTH);

  return wszString;
}


static void FreeWideString(LPWSTR wszString)
{
   HeapFree(GetProcessHeap(), 0, wszString);
}

/* ########################### */

static void test_UrlApplyScheme(void)
{
    WCHAR urlW[INTERNET_MAX_URL_LENGTH], newurlW[INTERNET_MAX_URL_LENGTH], expectW[INTERNET_MAX_URL_LENGTH];
    char newurl[INTERNET_MAX_URL_LENGTH];
    HRESULT res;
    DWORD len;
    DWORD i;

    for (i = 0; i < ARRAY_SIZE(TEST_APPLY); i++) {
        len = ARRAY_SIZE(newurl);
        strcpy(newurl, "untouched");
        res = UrlApplySchemeA(TEST_APPLY[i].url, newurl, &len, TEST_APPLY[i].flags);
        ok( res == TEST_APPLY[i].res,
            "#%ldA: got HRESULT 0x%lx (expected 0x%lx)\n", i, res, TEST_APPLY[i].res);
        if (res == S_OK)
        {
            ok(len == strlen(newurl), "Test %lu: Expected length %Iu, got %lu.\n", i, strlen(newurl), len);
            ok(!strcmp(newurl, TEST_APPLY[i].newurl), "Test %lu: Expected %s, got %s.\n",
                    i, debugstr_a(TEST_APPLY[i].newurl), debugstr_a(newurl));
        }
        else
        {
            ok(len == ARRAY_SIZE(newurl), "Test %lu: Got length %lu.\n", i, len);
            ok(!strcmp(newurl, "untouched"), "Test %lu: Got %s.\n", i, debugstr_a(newurl));
        }

        /* returned length is in character */
        MultiByteToWideChar(CP_ACP, 0, TEST_APPLY[i].url, -1, urlW, ARRAY_SIZE(urlW));
        MultiByteToWideChar(CP_ACP, 0, TEST_APPLY[i].newurl, -1, expectW, ARRAY_SIZE(expectW));

        len = ARRAY_SIZE(newurlW);
        wcscpy(newurlW, L"untouched");
        res = UrlApplySchemeW(urlW, newurlW, &len, TEST_APPLY[i].flags);
        ok( res == TEST_APPLY[i].res,
            "#%ldW: got HRESULT 0x%lx (expected 0x%lx)\n", i, res, TEST_APPLY[i].res);
        if (res == S_OK)
        {
            ok(len == wcslen(newurlW), "Test %lu: Expected length %Iu, got %lu.\n", i, wcslen(newurlW), len);
            ok(!wcscmp(newurlW, expectW), "Test %lu: Expected %s, got %s.\n",
                    i, debugstr_w(expectW), debugstr_w(newurlW));
        }
        else
        {
            ok(len == ARRAY_SIZE(newurlW), "Test %lu: Got length %lu.\n", i, len);
            ok(!wcscmp(newurlW, L"untouched"), "Test %lu: Got %s.\n", i, debugstr_w(newurlW));
        }
    }

    /* buffer too small */
    lstrcpyA(newurl, untouchedA);
    len = lstrlenA(TEST_APPLY[0].newurl);
    res = UrlApplySchemeA(TEST_APPLY[0].url, newurl, &len, TEST_APPLY[0].flags);
    ok(res == E_POINTER, "got HRESULT 0x%lx (expected E_POINTER)\n", res);
    /* The returned length include the space for the terminating 0 */
    i = lstrlenA(TEST_APPLY[0].newurl)+1;
    ok(len == i, "got len %ld (expected %ld)\n", len, i);
    ok(!lstrcmpA(newurl, untouchedA), "got '%s' (expected '%s')\n", newurl, untouchedA);

    /* NULL as parameter. The length and the buffer are not modified */
    lstrcpyA(newurl, untouchedA);
    len = ARRAY_SIZE(newurl);
    res = UrlApplySchemeA(NULL, newurl, &len, TEST_APPLY[0].flags);
    ok(res == E_INVALIDARG, "got HRESULT 0x%lx (expected E_INVALIDARG)\n", res);
    ok(len == ARRAY_SIZE(newurl), "got len %ld\n", len);
    ok(!lstrcmpA(newurl, untouchedA), "got '%s' (expected '%s')\n", newurl, untouchedA);

    len = ARRAY_SIZE(newurl);
    res = UrlApplySchemeA(TEST_APPLY[0].url, NULL, &len, TEST_APPLY[0].flags);
    ok(res == E_INVALIDARG, "got HRESULT 0x%lx (expected E_INVALIDARG)\n", res);
    ok(len == ARRAY_SIZE(newurl), "got len %ld\n", len);

    lstrcpyA(newurl, untouchedA);
    res = UrlApplySchemeA(TEST_APPLY[0].url, newurl, NULL, TEST_APPLY[0].flags);
    ok(res == E_INVALIDARG, "got HRESULT 0x%lx (expected E_INVALIDARG)\n", res);
    ok(!lstrcmpA(newurl, untouchedA), "got '%s' (expected '%s')\n", newurl, untouchedA);

}

/* ########################### */

static void hash_url(const char* szUrl)
{
  LPCSTR szTestUrl = szUrl;
  LPWSTR wszTestUrl = GetWideString(szTestUrl);
  HRESULT res;

  DWORD cbSize = sizeof(DWORD);
  DWORD dwHash1, dwHash2;
  res = UrlHashA(szTestUrl, (LPBYTE)&dwHash1, cbSize);
  ok(res == S_OK, "UrlHashA returned 0x%lx (expected S_OK) for %s\n", res, szUrl);

  res = UrlHashW(wszTestUrl, (LPBYTE)&dwHash2, cbSize);
  ok(res == S_OK, "UrlHashW returned 0x%lx (expected S_OK) for %s\n", res, szUrl);
  ok(dwHash1 == dwHash2,
      "Hashes didn't match (A: 0x%lx, W: 0x%lx) for %s\n", dwHash1, dwHash2, szUrl);
  FreeWideString(wszTestUrl);
}

static void test_UrlHash(void)
{
  hash_url(TEST_URL_1);
  hash_url(TEST_URL_2);
  hash_url(TEST_URL_3);
}

static void test_UrlGetPart(void)
{
    WCHAR bufferW[200];
    char buffer[200];
    unsigned int i;
    HRESULT hr;
    DWORD size;

    static const struct
    {
        const char *url;
        DWORD part;
        DWORD flags;
        HRESULT hr;
        const char *expect;
    }
    tests[] =
    {
        {"hi", URL_PART_SCHEME, 0, S_FALSE, ""},
        {"hi", URL_PART_USERNAME, 0, E_FAIL},
        {"hi", URL_PART_PASSWORD, 0, E_FAIL},
        {"hi", URL_PART_HOSTNAME, 0, E_FAIL},
        {"hi", URL_PART_PORT, 0, E_FAIL},
        {"hi", URL_PART_QUERY, 0, S_FALSE, ""},

        {"http://foo:bar@localhost:21/internal.php?query=x&return=y", URL_PART_SCHEME, 0, S_OK, "http"},
        {"http://foo:bar@localhost:21/internal.php?query=x&return=y", URL_PART_USERNAME, 0, S_OK, "foo"},
        {"http://foo:bar@localhost:21/internal.php?query=x&return=y", URL_PART_PASSWORD, 0, S_OK, "bar"},
        {"http://foo:bar@localhost:21/internal.php?query=x&return=y", URL_PART_HOSTNAME, 0, S_OK, "localhost"},
        {"http://foo:bar@localhost:21/internal.php?query=x&return=y", URL_PART_PORT, 0, S_OK, "21"},
        {"http://foo:bar@localhost:21/internal.php?query=x&return=y", URL_PART_QUERY, 0, S_OK, "query=x&return=y"},
        {"http://foo:bar@localhost:21/internal.php#anchor", URL_PART_QUERY, 0, S_FALSE, ""},
        {"http://foo:bar@localhost:21/internal.php?query=x&return=y", URL_PART_SCHEME, URL_PARTFLAG_KEEPSCHEME, S_OK, "http"},
        {"http://foo:bar@localhost:21/internal.php?query=x&return=y", URL_PART_USERNAME, URL_PARTFLAG_KEEPSCHEME, S_OK, "http:foo"},
        {"http://foo:bar@localhost:21/internal.php?query=x&return=y", URL_PART_PASSWORD, URL_PARTFLAG_KEEPSCHEME, S_OK, "http:bar"},
        {"http://foo:bar@localhost:21/internal.php?query=x&return=y", URL_PART_HOSTNAME, URL_PARTFLAG_KEEPSCHEME, S_OK, "http:localhost"},
        {"http://foo:bar@localhost:21/internal.php?query=x&return=y", URL_PART_PORT, URL_PARTFLAG_KEEPSCHEME, S_OK, "http:21"},
        {"http://foo:bar@localhost:21/internal.php?query=x&return=y", URL_PART_QUERY, URL_PARTFLAG_KEEPSCHEME, S_OK, "query=x&return=y"},

        {"http://localhost/", URL_PART_USERNAME, 0, E_INVALIDARG},
        {"http://localhost/", URL_PART_PASSWORD, 0, E_INVALIDARG},
        {"http://localhost/", URL_PART_HOSTNAME, 0, S_OK, "localhost"},
        {"http://localhost/", URL_PART_PORT, 0, E_INVALIDARG},
        {"http://localhost/", URL_PART_QUERY, 0, S_FALSE, ""},

        {"http://localhost:port/", URL_PART_USERNAME, 0, E_INVALIDARG},
        {"http://localhost:port/", URL_PART_PASSWORD, 0, E_INVALIDARG},
        {"http://localhost:port/", URL_PART_HOSTNAME, 0, S_OK, "localhost"},
        {"http://localhost:port/", URL_PART_PORT, 0, S_OK, "port"},
        {"http://:", URL_PART_HOSTNAME, 0, S_FALSE, ""},
        {"http://:", URL_PART_PORT, 0, S_FALSE, ""},

        {"http://user@localhost", URL_PART_USERNAME, 0, S_OK, "user"},
        {"http://user@localhost", URL_PART_PASSWORD, 0, E_INVALIDARG},
        {"http://user@localhost", URL_PART_HOSTNAME, 0, S_OK, "localhost"},
        {"http://user@localhost", URL_PART_PORT, 0, E_INVALIDARG},
        {"http://@", URL_PART_USERNAME, 0, S_FALSE, ""},
        {"http://@", URL_PART_PASSWORD, 0, E_INVALIDARG},
        {"http://@", URL_PART_HOSTNAME, 0, S_FALSE, ""},

        {"http://user:pass@localhost", URL_PART_USERNAME, 0, S_OK, "user"},
        {"http://user:pass@localhost", URL_PART_PASSWORD, 0, S_OK, "pass"},
        {"http://user:pass@localhost", URL_PART_HOSTNAME, 0, S_OK, "localhost"},
        {"http://user:pass@localhost", URL_PART_PORT, 0, E_INVALIDARG},
        {"http://:@", URL_PART_USERNAME, 0, S_FALSE, ""},
        {"http://:@", URL_PART_PASSWORD, 0, S_FALSE, ""},
        {"http://:@", URL_PART_HOSTNAME, 0, S_FALSE, ""},

        {"http://host:port:q", URL_PART_HOSTNAME, 0, S_OK, "host"},
        {"http://host:port:q", URL_PART_PORT, 0, S_OK, "port:q"},
        {"http://user:pass:q@host", URL_PART_USERNAME, 0, S_OK, "user"},
        {"http://user:pass:q@host", URL_PART_PASSWORD, 0, S_OK, "pass:q"},
        {"http://user@host@q", URL_PART_USERNAME, 0, S_OK, "user"},
        {"http://user@host@q", URL_PART_HOSTNAME, 0, S_OK, "host@q"},

        {"http:localhost/index.html", URL_PART_HOSTNAME, 0, E_FAIL},
        {"http:/localhost/index.html", URL_PART_HOSTNAME, 0, E_FAIL},

        {"http://localhost\\index.html", URL_PART_HOSTNAME, 0, S_OK, "localhost"},
        {"http:/\\localhost/index.html", URL_PART_HOSTNAME, 0, S_OK, "localhost"},
        {"http:\\/localhost/index.html", URL_PART_HOSTNAME, 0, S_OK, "localhost"},

        {"ftp://localhost\\index.html", URL_PART_HOSTNAME, 0, S_OK, "localhost"},
        {"ftp:/\\localhost/index.html", URL_PART_HOSTNAME, 0, S_OK, "localhost"},
        {"ftp:\\/localhost/index.html", URL_PART_HOSTNAME, 0, S_OK, "localhost"},

        {"http://host?a:b@c:d", URL_PART_HOSTNAME, 0, S_OK, "host"},
        {"http://host?a:b@c:d", URL_PART_QUERY, 0, S_OK, "a:b@c:d"},
        {"http://host#a:b@c:d", URL_PART_HOSTNAME, 0, S_OK, "host"},
        {"http://host#a:b@c:d", URL_PART_QUERY, 0, S_FALSE, ""},

        /* All characters, other than those with special meaning, are allowed. */
        {"http://foo:bar@google.*.com:21/internal.php?query=x&return=y", URL_PART_HOSTNAME, 0, S_OK, "google.*.com"},
        {"http:// !\"$%&'()*+,-.;<=>[]^_`{|~}:pass@host", URL_PART_USERNAME, 0, S_OK, " !\"$%&'()*+,-.;<=>[]^_`{|~}"},
        {"http://user: !\"$%&'()*+,-.;<=>[]^_`{|~}@host", URL_PART_PASSWORD, 0, S_OK, " !\"$%&'()*+,-.;<=>[]^_`{|~}"},
        {"http:// !\"$%&'()*+,-.;<=>[]^_`{|~}", URL_PART_HOSTNAME, 0, S_OK, " !\"$%&'()*+,-.;<=>[]^_`{|~}"},
        {"http://host: !\"$%&'()*+,-.;<=>[]^_`{|~}", URL_PART_PORT, 0, S_OK, " !\"$%&'()*+,-.;<=>[]^_`{|~}"},

        {"http:///index.html", URL_PART_HOSTNAME, 0, S_FALSE, ""},
        {"http:///index.html", URL_PART_HOSTNAME, URL_PARTFLAG_KEEPSCHEME, S_OK, "http:"},
        {"file://h o s t/c:/windows/file", URL_PART_HOSTNAME, 0, S_OK, "h o s t"},
        {"file://h o s t/c:/windows/file", URL_PART_HOSTNAME, URL_PARTFLAG_KEEPSCHEME, S_OK, "h o s t"},
        {"file://foo:bar@localhost:21/file?query=x", URL_PART_USERNAME, 0, E_FAIL},
        {"file://foo:bar@localhost:21/file?query=x", URL_PART_PASSWORD, 0, E_FAIL},
        {"file://foo:bar@localhost:21/file?query=x", URL_PART_HOSTNAME, 0, S_OK, "foo:bar@localhost:21"},
        {"file://foo:bar@localhost:21/file?query=x", URL_PART_PORT, 0, E_FAIL},
        {"file://foo:bar@localhost:21/file?query=x", URL_PART_QUERY, 0, S_OK, "query=x"},
        {"http://user:pass 123@www.wine hq.org", URL_PART_HOSTNAME, 0, S_OK, "www.wine hq.org"},
        {"http://user:pass 123@www.wine hq.org", URL_PART_PASSWORD, 0, S_OK, "pass 123"},
        {"about:blank", URL_PART_SCHEME, 0, S_OK, "about"},
        {"about:blank", URL_PART_HOSTNAME, 0, E_FAIL},
        {"x-excid://36C00000/guid:{048B4E89-2E92-496F-A837-33BA02FF6D32}/Message.htm", URL_PART_SCHEME, 0, S_OK, "x-excid"},
        {"x-excid://36C00000/guid:{048B4E89-2E92-496F-A837-33BA02FF6D32}/Message.htm", URL_PART_HOSTNAME, 0, E_FAIL},
        {"x-excid://36C00000/guid:{048B4E89-2E92-496F-A837-33BA02FF6D32}/Message.htm", URL_PART_QUERY, 0, S_FALSE, ""},
        {"foo://bar-url/test", URL_PART_SCHEME, 0, S_OK, "foo"},
        {"foo://bar-url/test", URL_PART_HOSTNAME, 0, E_FAIL},
        {"foo://bar-url/test", URL_PART_QUERY, 0, S_FALSE, ""},
        {"ascheme:", URL_PART_SCHEME, 0, S_OK, "ascheme"},
        {"res://some.dll/find.dlg", URL_PART_SCHEME, 0, S_OK, "res"},
        {"res://some.dll/find.dlg", URL_PART_QUERY, 0, S_FALSE, ""},
        {"http://www.winehq.org", URL_PART_HOSTNAME, URL_PARTFLAG_KEEPSCHEME, S_OK, "http:www.winehq.org"},
        {"file:///index.html", URL_PART_HOSTNAME, 0, S_FALSE, ""},
        {"file:///index.html", URL_PART_HOSTNAME, URL_PARTFLAG_KEEPSCHEME, S_FALSE, ""},
        {"file://c:\\index.htm", URL_PART_HOSTNAME, 0, S_FALSE, ""},
        {"file://c:\\index.htm", URL_PART_HOSTNAME, URL_PARTFLAG_KEEPSCHEME, S_FALSE, ""},
        {"file:some text", URL_PART_HOSTNAME, 0, S_FALSE, ""},
        {"index.htm", URL_PART_HOSTNAME, 0, E_FAIL},
        {"sChEmE-.+:", URL_PART_SCHEME, 0, S_OK, "scheme-.+"},
        {"scheme_:", URL_PART_SCHEME, 0, S_FALSE, ""},
        {"scheme :", URL_PART_SCHEME, 0, S_FALSE, ""},
        {"sch eme:", URL_PART_SCHEME, 0, S_FALSE, ""},
        {":", URL_PART_SCHEME, 0, S_FALSE, ""},
        {"a:", URL_PART_SCHEME, 0, S_FALSE, ""},
        {"0:", URL_PART_SCHEME, 0, S_FALSE, ""},
        {"ab:", URL_PART_SCHEME, 0, S_OK, "ab"},

        {"about://hostname/", URL_PART_HOSTNAME, 0, E_FAIL},
        {"file://hostname/", URL_PART_HOSTNAME, 0, S_OK, "hostname"},
        {"ftp://hostname/", URL_PART_HOSTNAME, 0, S_OK, "hostname"},
        {"gopher://hostname/", URL_PART_HOSTNAME, 0, S_OK, "hostname"},
        {"http://hostname/", URL_PART_HOSTNAME, 0, S_OK, "hostname"},
        {"https://hostname/", URL_PART_HOSTNAME, 0, S_OK, "hostname"},
        {"javascript://hostname/", URL_PART_HOSTNAME, 0, E_FAIL},
        {"local://hostname/", URL_PART_HOSTNAME, 0, E_FAIL},
        {"mailto://hostname/", URL_PART_HOSTNAME, 0, E_FAIL},
        {"mk://hostname/", URL_PART_HOSTNAME, 0, E_FAIL},
        {"news://hostname/", URL_PART_HOSTNAME, 0, S_OK, "hostname"},
        {"nntp://hostname/", URL_PART_HOSTNAME, 0, S_OK, "hostname"},
        {"res://hostname/", URL_PART_HOSTNAME, 0, E_FAIL},
        {"shell://hostname/", URL_PART_HOSTNAME, 0, E_FAIL},
        {"snews://hostname/", URL_PART_HOSTNAME, 0, S_OK, "hostname"},
        {"telnet://hostname/", URL_PART_HOSTNAME, 0, S_OK, "hostname"},
        {"vbscript://hostname/", URL_PART_HOSTNAME, 0, E_FAIL},
        {"wais://hostname/", URL_PART_HOSTNAME, 0, E_FAIL},

        {"file://hostname/", URL_PART_HOSTNAME, URL_PARTFLAG_KEEPSCHEME, S_OK, "hostname"},
        {"ftp://hostname/", URL_PART_HOSTNAME, URL_PARTFLAG_KEEPSCHEME, S_OK, "ftp:hostname"},
        {"gopher://hostname/", URL_PART_HOSTNAME, URL_PARTFLAG_KEEPSCHEME, S_OK, "gopher:hostname"},
        {"http://hostname/", URL_PART_HOSTNAME, URL_PARTFLAG_KEEPSCHEME, S_OK, "http:hostname"},
        {"https://hostname/", URL_PART_HOSTNAME, URL_PARTFLAG_KEEPSCHEME, S_OK, "https:hostname"},
        {"news://hostname/", URL_PART_HOSTNAME, URL_PARTFLAG_KEEPSCHEME, S_OK, "news:hostname"},
        {"nntp://hostname/", URL_PART_HOSTNAME, URL_PARTFLAG_KEEPSCHEME, S_OK, "nntp:hostname"},
        {"snews://hostname/", URL_PART_HOSTNAME, URL_PARTFLAG_KEEPSCHEME, S_OK, "snews:hostname"},
        {"telnet://hostname/", URL_PART_HOSTNAME, URL_PARTFLAG_KEEPSCHEME, S_OK, "telnet:hostname"},
    };

    hr = UrlGetPartA(NULL, NULL, NULL, URL_PART_SCHEME, 0);
    ok(hr == E_INVALIDARG, "Got hr %#lx.\n", hr);

    hr = UrlGetPartA(NULL, buffer, &size, URL_PART_SCHEME, 0);
    ok(hr == E_INVALIDARG, "Got hr %#lx.\n", hr);

    hr = UrlGetPartA("res://some.dll/find.dlg", NULL, &size, URL_PART_SCHEME, 0);
    ok(hr == E_INVALIDARG, "Got hr %#lx.\n", hr);

    hr = UrlGetPartA("res://some.dll/find.dlg", buffer, NULL, URL_PART_SCHEME, 0);
    ok(hr == E_INVALIDARG, "Got hr %#lx.\n", hr);

    size = 0;
    strcpy(buffer, "x");
    hr = UrlGetPartA("hi", buffer, &size, URL_PART_SCHEME, 0);
    ok(hr == E_INVALIDARG, "Got hr %#lx.\n", hr);
    ok(!strcmp(buffer, "x"), "Got result %s.\n", debugstr_a(buffer));
    ok(!size, "Got size %lu.\n", size);

    for (i = 0; i < ARRAY_SIZE(tests); ++i)
    {
        WCHAR urlW[200], expectW[200];
        const char *expect = tests[i].expect;
        const char *url = tests[i].url;
        DWORD flags = tests[i].flags;
        DWORD part = tests[i].part;

        winetest_push_context("URL %s, part %#lx, flags %#lx", debugstr_a(url), part, flags);

        size = 1;
        strcpy(buffer, "x");
        hr = UrlGetPartA(url, buffer, &size, part, flags);
        if (tests[i].hr == S_OK)
            ok(hr == E_POINTER, "Got hr %#lx.\n", hr);
        else
            ok(hr == tests[i].hr, "Got hr %#lx.\n", hr);

        if (hr == S_FALSE)
        {
            ok(!size, "Got size %lu.\n", size);
            ok(!buffer[0], "Got result %s.\n", debugstr_a(buffer));
        }
        else
        {
            if (hr == E_POINTER)
                ok(size == strlen(expect) + 1, "Got size %lu.\n", size);
            else
                ok(size == 1, "Got size %lu.\n", size);
            ok(!strcmp(buffer, "x"), "Got result %s.\n", debugstr_a(buffer));
        }

        size = sizeof(buffer);
        strcpy(buffer, "x");
        hr = UrlGetPartA(url, buffer, &size, part, flags);
        ok(hr == tests[i].hr, "Got hr %#lx.\n", hr);
        if (SUCCEEDED(hr))
        {
            ok(size == strlen(buffer), "Got size %lu.\n", size);
            ok(!strcmp(buffer, expect), "Got result %s.\n", debugstr_a(buffer));
        }
        else
        {
            ok(size == sizeof(buffer), "Got size %lu.\n", size);
            ok(!strcmp(buffer, "x"), "Got result %s.\n", debugstr_a(buffer));
        }

        MultiByteToWideChar(CP_ACP, 0, url, -1, urlW, ARRAY_SIZE(urlW));

        size = 1;
        wcscpy(bufferW, L"x");
        hr = UrlGetPartW(urlW, bufferW, &size, part, flags);
        if (tests[i].hr == S_OK)
            ok(hr == E_POINTER, "Got hr %#lx.\n", hr);
        else
            ok(hr == (tests[i].hr == S_FALSE ? S_OK : tests[i].hr), "Got hr %#lx.\n", hr);

        if (SUCCEEDED(hr))
        {
            ok(!size, "Got size %lu.\n", size);
            ok(!buffer[0], "Got result %s.\n", debugstr_a(buffer));
        }
        else
        {
            if (hr == E_POINTER)
                ok(size == strlen(expect) + 1, "Got size %lu.\n", size);
            else
                ok(size == 1, "Got size %lu.\n", size);
            ok(!wcscmp(bufferW, L"x"), "Got result %s.\n", debugstr_w(bufferW));
        }

        size = ARRAY_SIZE(bufferW);
        wcscpy(bufferW, L"x");
        hr = UrlGetPartW(urlW, bufferW, &size, part, flags);
        ok(hr == (tests[i].hr == S_FALSE ? S_OK : tests[i].hr), "Got hr %#lx.\n", hr);
        if (SUCCEEDED(hr))
        {
            ok(size == wcslen(bufferW), "Got size %lu.\n", size);
            MultiByteToWideChar(CP_ACP, 0, buffer, -1, expectW, ARRAY_SIZE(expectW));
            ok(!wcscmp(bufferW, expectW), "Got result %s.\n", debugstr_w(bufferW));
        }
        else
        {
            ok(size == ARRAY_SIZE(bufferW), "Got size %lu.\n", size);
            ok(!wcscmp(bufferW, L"x"), "Got result %s.\n", debugstr_w(bufferW));
        }

        winetest_pop_context();
    }

    /* Test non-ASCII characters. */

    size = ARRAY_SIZE(bufferW);
    wcscpy(bufferW, L"x");
    hr = UrlGetPartW(L"http://\x01\x7f\x80\xff:pass@host", bufferW, &size, URL_PART_USERNAME, 0);
    ok(hr == S_OK, "Got hr %#lx.\n", hr);
    ok(size == wcslen(bufferW), "Got size %lu.\n", size);
    ok(!wcscmp(bufferW, L"\x01\x7f\x80\xff"), "Got result %s.\n", debugstr_w(bufferW));
}

/* ########################### */
static void check_url_canonicalize(int index, const char *szUrl, DWORD dwFlags, const char *szExpectUrl, BOOL todo)
{
    CHAR szReturnUrl[INTERNET_MAX_URL_LENGTH];
    WCHAR wszReturnUrl[INTERNET_MAX_URL_LENGTH];
    LPWSTR wszUrl = GetWideString(szUrl);
    LPWSTR wszExpectUrl = GetWideString(szExpectUrl);
    LPWSTR wszConvertedUrl;
    HRESULT ret;

    DWORD dwSize;

    dwSize = INTERNET_MAX_URL_LENGTH;
    ret = UrlCanonicalizeA(szUrl, NULL, &dwSize, dwFlags);
    ok(ret == E_INVALIDARG, "Got unexpected hr %#lx for index %d.\n", ret, index);
    ret = UrlCanonicalizeA(szUrl, szReturnUrl, &dwSize, dwFlags);
    ok(ret == S_OK || (!szUrl[0] && ret == S_FALSE) /* Vista+ */,
            "Got unexpected hr %#lx for index %d.\n", ret, index);
    todo_wine_if (todo)
        ok(strcmp(szReturnUrl,szExpectUrl)==0, "UrlCanonicalizeA dwFlags 0x%08lx url '%s' Expected \"%s\", but got \"%s\", index %d\n", dwFlags, szUrl, szExpectUrl, szReturnUrl, index);

    dwSize = INTERNET_MAX_URL_LENGTH;
    ret = UrlCanonicalizeW(wszUrl, NULL, &dwSize, dwFlags);
    ok(ret == E_INVALIDARG, "Got unexpected hr %#lx for index %d.\n", ret, index);
    ret = UrlCanonicalizeW(wszUrl, wszReturnUrl, &dwSize, dwFlags);
    ok(ret == S_OK, "Got unexpected hr %#lx for index %d.\n", ret, index);

    wszConvertedUrl = GetWideString(szReturnUrl);
    ok(lstrcmpW(wszReturnUrl, wszConvertedUrl)==0,
        "Strings didn't match between ansi and unicode UrlCanonicalize, index %d!\n", index);
    FreeWideString(wszConvertedUrl);

    FreeWideString(wszUrl);
    FreeWideString(wszExpectUrl);
}


static void test_UrlEscapeA(void)
{
    DWORD size = 0;
    HRESULT ret;
    unsigned int i;
    char empty_string[] = "";

    ret = UrlEscapeA("/woningplan/woonkamer basis.swf", NULL, &size, URL_ESCAPE_SPACES_ONLY);
    ok(ret == E_INVALIDARG, "got %lx, expected %lx\n", ret, E_INVALIDARG);
    ok(size == 0, "got %ld, expected %d\n", size, 0);

    size = 0;
    ret = UrlEscapeA("/woningplan/woonkamer basis.swf", empty_string, &size, URL_ESCAPE_SPACES_ONLY);
    ok(ret == E_INVALIDARG, "got %lx, expected %lx\n", ret, E_INVALIDARG);
    ok(size == 0, "got %ld, expected %d\n", size, 0);

    size = 1;
    ret = UrlEscapeA("/woningplan/woonkamer basis.swf", NULL, &size, URL_ESCAPE_SPACES_ONLY);
    ok(ret == E_INVALIDARG, "got %lx, expected %lx\n", ret, E_INVALIDARG);
    ok(size == 1, "got %ld, expected %d\n", size, 1);

    size = 1;
    ret = UrlEscapeA("/woningplan/woonkamer basis.swf", empty_string, NULL, URL_ESCAPE_SPACES_ONLY);
    ok(ret == E_INVALIDARG, "got %lx, expected %lx\n", ret, E_INVALIDARG);
    ok(size == 1, "got %ld, expected %d\n", size, 1);

    size = 1;
    empty_string[0] = 127;
    ret = UrlEscapeA("/woningplan/woonkamer basis.swf", empty_string, &size, URL_ESCAPE_SPACES_ONLY);
    ok(ret == E_POINTER, "got %lx, expected %lx\n", ret, E_POINTER);
    ok(size == 34, "got %ld, expected %d\n", size, 34);
    ok(empty_string[0] == 127, "String has changed, empty_string[0] = %d\n", empty_string[0]);

    size = 1;
    empty_string[0] = 127;
    ret = UrlEscapeA("/woningplan/woonkamer basis.swf", empty_string, &size, URL_ESCAPE_AS_UTF8);
    ok(ret == E_NOTIMPL, "Got unexpected hr %#lx.\n", ret);
    ok(size == 1, "Got unexpected size %lu.\n", size);
    ok(empty_string[0] == 127, "String has changed, empty_string[0] = %d\n", empty_string[0]);

    for (i = 0; i < ARRAY_SIZE(TEST_ESCAPE); i++) {
        CHAR ret_url[INTERNET_MAX_URL_LENGTH];

        size = INTERNET_MAX_URL_LENGTH;
        ret = UrlEscapeA(TEST_ESCAPE[i].url, ret_url, &size, TEST_ESCAPE[i].flags);
        ok(ret == S_OK, "Got unexpected hr %#lx for %s.\n", ret, debugstr_a(TEST_ESCAPE[i].url));
        ok(!strcmp(ret_url, TEST_ESCAPE[i].expecturl), "Expected \"%s\", but got \"%s\" for \"%s\"\n",
            TEST_ESCAPE[i].expecturl, ret_url, TEST_ESCAPE[i].url);
    }
}

static void test_UrlEscapeW(void)
{
    WCHAR ret_urlW[INTERNET_MAX_URL_LENGTH];
    WCHAR overwrite[10] = L"foo bar";
    WCHAR empty_string[] = {0};
    DWORD size;
    HRESULT ret;
    WCHAR wc;
    int i;

    /* Check error paths */

    ret = UrlEscapeW(L"/test", NULL, NULL, URL_ESCAPE_SPACES_ONLY);
    ok(ret == E_INVALIDARG, "got %lx, expected %lx\n", ret, E_INVALIDARG);

    size = 0;
    ret = UrlEscapeW(L"/test", NULL, &size, URL_ESCAPE_SPACES_ONLY);
    ok(ret == E_INVALIDARG, "got %lx, expected %lx\n", ret, E_INVALIDARG);
    ok(size == 0, "got %ld, expected %d\n", size, 0);

    ret = UrlEscapeW(L"/test", empty_string, NULL, URL_ESCAPE_SPACES_ONLY);
    ok(ret == E_INVALIDARG, "got %lx, expected %lx\n", ret, E_INVALIDARG);

    size = 0;
    ret = UrlEscapeW(L"/test", empty_string, &size, URL_ESCAPE_SPACES_ONLY);
    ok(ret == E_INVALIDARG, "got %lx, expected %lx\n", ret, E_INVALIDARG);
    ok(size == 0, "got %ld, expected %d\n", size, 0);

    ret = UrlEscapeW(L"/test", NULL, NULL, URL_ESCAPE_SPACES_ONLY);
    ok(ret == E_INVALIDARG, "got %lx, expected %lx\n", ret, E_INVALIDARG);

    size = 1;
    ret = UrlEscapeW(L"/test", NULL, &size, URL_ESCAPE_SPACES_ONLY);
    ok(ret == E_INVALIDARG, "got %lx, expected %lx\n", ret, E_INVALIDARG);
    ok(size == 1, "got %ld, expected %d\n", size, 1);

    ret = UrlEscapeW(L"/test", empty_string, NULL, URL_ESCAPE_SPACES_ONLY);
    ok(ret == E_INVALIDARG, "got %lx, expected %lx\n", ret, E_INVALIDARG);

    size = 1;
    ret = UrlEscapeW(L"/test", empty_string, &size, URL_ESCAPE_SPACES_ONLY);
    ok(ret == E_POINTER, "got %lx, expected %lx\n", ret, E_POINTER);
    ok(size == 6, "got %ld, expected %d\n", size, 6);

    /* Check actual escaping */

    size = ARRAY_SIZE(overwrite);
    ret = UrlEscapeW(overwrite, overwrite, &size, URL_ESCAPE_SPACES_ONLY);
    ok(ret == S_OK, "got %lx, expected S_OK\n", ret);
    ok(size == 9, "got %ld, expected 9\n", size);
    ok(!wcscmp(overwrite, L"foo%20bar"), "Got unexpected string %s.\n", debugstr_w(overwrite));

    size = 1;
    wc = 127;
    ret = UrlEscapeW(overwrite, &wc, &size, URL_ESCAPE_SPACES_ONLY);
    ok(ret == E_POINTER, "got %lx, expected %lx\n", ret, E_POINTER);
    ok(size == 10, "got %ld, expected 10\n", size);
    ok(wc == 127, "String has changed, wc = %d\n", wc);

    /* non-ASCII range */
    size = ARRAY_SIZE(ret_urlW);
    ret = UrlEscapeW(L"ftp\x1f\xff\xfa\x2122q/", ret_urlW, &size, 0);
    ok(ret == S_OK, "got %lx, expected S_OK\n", ret);
    ok(!wcscmp(ret_urlW, L"ftp%1F%FF%FA\x2122q/"), "Got unexpected string %s.\n", debugstr_w(ret_urlW));

    for (i = 0; i < ARRAY_SIZE(TEST_ESCAPE); i++) {

        WCHAR *urlW, *expected_urlW;

        size = INTERNET_MAX_URL_LENGTH;
        urlW = GetWideString(TEST_ESCAPE[i].url);
        expected_urlW = GetWideString(TEST_ESCAPE[i].expecturl);
        ret = UrlEscapeW(urlW, ret_urlW, &size, TEST_ESCAPE[i].flags);
        ok(ret == S_OK, "Got unexpected hr %#lx for %s.\n", ret, debugstr_w(urlW));
        ok(!lstrcmpW(ret_urlW, expected_urlW), "Expected %s, but got %s for %s flags %08lx\n",
            wine_dbgstr_w(expected_urlW), wine_dbgstr_w(ret_urlW), wine_dbgstr_w(urlW), TEST_ESCAPE[i].flags);
        FreeWideString(urlW);
        FreeWideString(expected_urlW);
    }

    for (i = 0; i < ARRAY_SIZE(TEST_ESCAPEW); i++) {
        WCHAR ret_url[INTERNET_MAX_URL_LENGTH];

        size = INTERNET_MAX_URL_LENGTH;
        ret = UrlEscapeW(TEST_ESCAPEW[i].url, ret_url, &size, TEST_ESCAPEW[i].flags);
        ok(ret == S_OK, "Got unexpected hr %#lx for %s.\n", ret, debugstr_w(TEST_ESCAPEW[i].url));
        ok(!wcscmp(ret_url, TEST_ESCAPEW[i].expecturl)
                || broken(!wcscmp(ret_url, TEST_ESCAPEW[i].win7url)),
                "Expected %s, but got %s for %s.\n", debugstr_w(TEST_ESCAPEW[i].expecturl),
                debugstr_w(ret_url), debugstr_w(TEST_ESCAPEW[i].url));
    }
}

/* ########################### */

static void test_UrlCanonicalizeA(void)
{
    unsigned int i;
    CHAR szReturnUrl[4*INTERNET_MAX_URL_LENGTH];
    CHAR longurl[4*INTERNET_MAX_URL_LENGTH];
    DWORD dwSize;
    DWORD urllen;
    HRESULT hr;

    urllen = lstrlenA(winehqA);

    /* Parameter checks */
    dwSize = ARRAY_SIZE(szReturnUrl);
    hr = UrlCanonicalizeA(NULL, szReturnUrl, &dwSize, URL_UNESCAPE);
    ok(hr == E_INVALIDARG, "Got unexpected hr %#lx.\n", hr);

    dwSize = ARRAY_SIZE(szReturnUrl);
    hr = UrlCanonicalizeA(winehqA, NULL, &dwSize, URL_UNESCAPE);
    ok(hr == E_INVALIDARG, "Got unexpected hr %#lx.\n", hr);

    hr = UrlCanonicalizeA(winehqA, szReturnUrl, NULL, URL_UNESCAPE);
    ok(hr == E_INVALIDARG, "Got unexpected hr %#lx.\n", hr);

    dwSize = 0;
    hr = UrlCanonicalizeA(winehqA, szReturnUrl, &dwSize, URL_UNESCAPE);
    ok(hr == E_INVALIDARG, "Got unexpected hr %#lx.\n", hr);

    /* buffer has no space for the result */
    dwSize=urllen-1;
    memset(szReturnUrl, '#', urllen+4);
    szReturnUrl[urllen+4] = '\0';
    SetLastError(0xdeadbeef);
    hr = UrlCanonicalizeA(winehqA, szReturnUrl, &dwSize, URL_WININET_COMPATIBILITY  | URL_ESCAPE_UNSAFE);
    ok( (hr == E_POINTER) && (dwSize == (urllen + 1)),
        "got 0x%lx with %lu and size %lu for '%s' and %u (expected 'E_POINTER' and size %lu)\n",
        hr, GetLastError(), dwSize, szReturnUrl, lstrlenA(szReturnUrl), urllen+1);

    /* buffer has no space for the terminating '\0' */
    dwSize=urllen;
    memset(szReturnUrl, '#', urllen+4);
    szReturnUrl[urllen+4] = '\0';
    SetLastError(0xdeadbeef);
    hr = UrlCanonicalizeA(winehqA, szReturnUrl, &dwSize, URL_WININET_COMPATIBILITY | URL_ESCAPE_UNSAFE);
    ok( (hr == E_POINTER) && (dwSize == (urllen + 1)),
        "got 0x%lx with %lu and size %lu for '%s' and %u (expected 'E_POINTER' and size %lu)\n",
        hr, GetLastError(), dwSize, szReturnUrl, lstrlenA(szReturnUrl), urllen+1);

    /* buffer has the required size */
    dwSize=urllen+1;
    memset(szReturnUrl, '#', urllen+4);
    szReturnUrl[urllen+4] = '\0';
    SetLastError(0xdeadbeef);
    hr = UrlCanonicalizeA(winehqA, szReturnUrl, &dwSize, URL_WININET_COMPATIBILITY | URL_ESCAPE_UNSAFE);
    ok( (hr == S_OK) && (dwSize == urllen),
        "got 0x%lx with %lu and size %lu for '%s' and %u (expected 'S_OK' and size %lu)\n",
        hr, GetLastError(), dwSize, szReturnUrl, lstrlenA(szReturnUrl), urllen);

    /* buffer is larger as the required size */
    dwSize=urllen+2;
    memset(szReturnUrl, '#', urllen+4);
    szReturnUrl[urllen+4] = '\0';
    SetLastError(0xdeadbeef);
    hr = UrlCanonicalizeA(winehqA, szReturnUrl, &dwSize, URL_WININET_COMPATIBILITY | URL_ESCAPE_UNSAFE);
    ok( (hr == S_OK) && (dwSize == urllen),
        "got 0x%lx with %lu and size %lu for '%s' and %u (expected 'S_OK' and size %lu)\n",
        hr, GetLastError(), dwSize, szReturnUrl, lstrlenA(szReturnUrl), urllen);

    /* length is set to 0 */
    dwSize=0;
    memset(szReturnUrl, '#', urllen+4);
    szReturnUrl[urllen+4] = '\0';
    SetLastError(0xdeadbeef);
    hr = UrlCanonicalizeA(winehqA, szReturnUrl, &dwSize, URL_WININET_COMPATIBILITY | URL_ESCAPE_UNSAFE);
    ok( (hr == E_INVALIDARG) && (dwSize == 0),
            "got 0x%lx with %lu and size %lu for '%s' and %u (expected 'E_INVALIDARG' and size %u)\n",
            hr, GetLastError(), dwSize, szReturnUrl, lstrlenA(szReturnUrl), 0);

    /* url length > INTERNET_MAX_URL_SIZE */
    dwSize=sizeof(szReturnUrl);
    memset(longurl, 'a', sizeof(longurl));
    memcpy(longurl, winehqA, sizeof(winehqA)-1);
    longurl[sizeof(longurl)-1] = '\0';
    hr = UrlCanonicalizeA(longurl, szReturnUrl, &dwSize, URL_WININET_COMPATIBILITY | URL_ESCAPE_UNSAFE);
    ok(hr == S_OK, "hr = %lx\n", hr);

    /* test url-modification */
    for (i = 0; i < ARRAY_SIZE(TEST_CANONICALIZE); i++) {
        check_url_canonicalize(i, TEST_CANONICALIZE[i].url, TEST_CANONICALIZE[i].flags,
                TEST_CANONICALIZE[i].expecturl, TEST_CANONICALIZE[i].todo);
    }
}

/* ########################### */

static void test_UrlCanonicalizeW(void)
{
    WCHAR szReturnUrl[INTERNET_MAX_URL_LENGTH];
    DWORD dwSize;
    DWORD urllen;
    HRESULT hr;
    int i;

    urllen = lstrlenW(winehqW);

    /* Parameter checks */
    dwSize = ARRAY_SIZE(szReturnUrl);
    hr = UrlCanonicalizeW(NULL, szReturnUrl, &dwSize, URL_UNESCAPE);
    ok(hr == E_INVALIDARG, "Got unexpected hr %#lx.\n", hr);

    dwSize = ARRAY_SIZE(szReturnUrl);
    hr = UrlCanonicalizeW(winehqW, NULL, &dwSize, URL_UNESCAPE);
    ok(hr == E_INVALIDARG, "Got unexpected hr %#lx.\n", hr);

    hr = UrlCanonicalizeW(winehqW, szReturnUrl, NULL, URL_UNESCAPE);
    ok(hr == E_INVALIDARG, "Got unexpected hr %#lx.\n", hr);

    dwSize = 0;
    hr = UrlCanonicalizeW(winehqW, szReturnUrl, &dwSize, URL_UNESCAPE);
    ok(hr == E_INVALIDARG, "Got unexpected hr %#lx.\n", hr);

    /* buffer has no space for the result */
    dwSize = (urllen-1);
    memset(szReturnUrl, '#', (urllen+4) * sizeof(WCHAR));
    szReturnUrl[urllen+4] = '\0';
    SetLastError(0xdeadbeef);
    hr = UrlCanonicalizeW(winehqW, szReturnUrl, &dwSize, URL_WININET_COMPATIBILITY | URL_ESCAPE_UNSAFE);
    ok( (hr == E_POINTER) && (dwSize == (urllen + 1)),
        "got 0x%lx with %lu and size %lu for %u (expected 'E_POINTER' and size %lu)\n",
        hr, GetLastError(), dwSize, lstrlenW(szReturnUrl), urllen+1);


    /* buffer has no space for the terminating '\0' */
    dwSize = urllen;
    memset(szReturnUrl, '#', (urllen+4) * sizeof(WCHAR));
    szReturnUrl[urllen+4] = '\0';
    SetLastError(0xdeadbeef);
    hr = UrlCanonicalizeW(winehqW, szReturnUrl, &dwSize, URL_WININET_COMPATIBILITY | URL_ESCAPE_UNSAFE);
    ok( (hr == E_POINTER) && (dwSize == (urllen + 1)),
        "got 0x%lx with %lu and size %lu for %u (expected 'E_POINTER' and size %lu)\n",
        hr, GetLastError(), dwSize, lstrlenW(szReturnUrl), urllen+1);

    /* buffer has the required size */
    dwSize = urllen +1;
    memset(szReturnUrl, '#', (urllen+4) * sizeof(WCHAR));
    szReturnUrl[urllen+4] = '\0';
    SetLastError(0xdeadbeef);
    hr = UrlCanonicalizeW(winehqW, szReturnUrl, &dwSize, URL_WININET_COMPATIBILITY | URL_ESCAPE_UNSAFE);
    ok( (hr == S_OK) && (dwSize == urllen),
        "got 0x%lx with %lu and size %lu for %u (expected 'S_OK' and size %lu)\n",
        hr, GetLastError(), dwSize, lstrlenW(szReturnUrl), urllen);

    /* buffer is larger as the required size */
    dwSize = (urllen+2);
    memset(szReturnUrl, '#', (urllen+4) * sizeof(WCHAR));
    szReturnUrl[urllen+4] = '\0';
    SetLastError(0xdeadbeef);
    hr = UrlCanonicalizeW(winehqW, szReturnUrl, &dwSize, URL_WININET_COMPATIBILITY | URL_ESCAPE_UNSAFE);
    ok( (hr == S_OK) && (dwSize == urllen),
        "got 0x%lx with %lu and size %lu for %u (expected 'S_OK' and size %lu)\n",
        hr, GetLastError(), dwSize, lstrlenW(szReturnUrl), urllen);

    /* check that the characters 1..32 are chopped from the end of the string */
    for (i = 1; i < 65536; i++)
    {
        WCHAR szUrl[128];
        BOOL choped;
        int pos;

        wcscpy(szUrl, L"http://www.winehq.org/X");
        pos = lstrlenW(szUrl) - 1;
        szUrl[pos] = i;
        urllen = INTERNET_MAX_URL_LENGTH;
        UrlCanonicalizeW(szUrl, szReturnUrl, &urllen, 0);
        choped = lstrlenW(szReturnUrl) < lstrlenW(szUrl);
        ok(choped == (i <= 32), "Incorrect char chopping for char %d\n", i);
    }
}

/* ########################### */

static void check_url_combine(const char *szUrl1, const char *szUrl2, DWORD dwFlags, const char *szExpectUrl, BOOL todo)
{
    HRESULT hr;
    CHAR szReturnUrl[INTERNET_MAX_URL_LENGTH];
    WCHAR wszReturnUrl[INTERNET_MAX_URL_LENGTH];
    LPWSTR wszUrl1, wszUrl2, wszExpectUrl, wszConvertedUrl;

    DWORD dwSize;
    DWORD dwExpectLen = lstrlenA(szExpectUrl);

    wszUrl1 = GetWideString(szUrl1);
    wszUrl2 = GetWideString(szUrl2);
    wszExpectUrl = GetWideString(szExpectUrl);

    hr = UrlCombineA(szUrl1, szUrl2, NULL, NULL, dwFlags);
    ok(hr == E_INVALIDARG, "UrlCombineA returned 0x%08lx, expected 0x%08lx\n", hr, E_INVALIDARG);

    dwSize = 0;
    hr = UrlCombineA(szUrl1, szUrl2, NULL, &dwSize, dwFlags);
    ok(hr == E_POINTER, "Checking length of string, return was 0x%08lx, expected 0x%08lx\n", hr, E_POINTER);
    ok(todo || dwSize == dwExpectLen+1, "Got length %ld, expected %ld\n", dwSize, dwExpectLen+1);

    dwSize--;
    hr = UrlCombineA(szUrl1, szUrl2, szReturnUrl, &dwSize, dwFlags);
    ok(hr == E_POINTER, "UrlCombineA returned 0x%08lx, expected 0x%08lx\n", hr, E_POINTER);
    ok(todo || dwSize == dwExpectLen+1, "Got length %ld, expected %ld\n", dwSize, dwExpectLen+1);

    hr = UrlCombineA(szUrl1, szUrl2, szReturnUrl, &dwSize, dwFlags);
    ok(hr == S_OK, "Got unexpected hr %#lx.\n", hr);
    ok(dwSize == dwExpectLen, "Got length %ld, expected %ld\n", dwSize, dwExpectLen);

    if (todo)
    {
        todo_wine ok(dwSize == dwExpectLen && (FAILED(hr) || strcmp(szReturnUrl, szExpectUrl)==0),
                "Expected %s (len=%ld), but got %s (len=%ld)\n", szExpectUrl, dwExpectLen, SUCCEEDED(hr) ? szReturnUrl : "(null)", dwSize);
    }
    else
    {
        ok(dwSize == dwExpectLen, "Got length %ld, expected %ld\n", dwSize, dwExpectLen);
        if (SUCCEEDED(hr))
            ok(strcmp(szReturnUrl, szExpectUrl)==0, "Expected %s, but got %s\n", szExpectUrl, szReturnUrl);
     }

    dwSize = 0;
    hr = UrlCombineW(wszUrl1, wszUrl2, NULL, &dwSize, dwFlags);
    ok(hr == E_POINTER, "Checking length of string, return was 0x%08lx, expected 0x%08lx\n", hr, E_POINTER);
    ok(dwSize == dwExpectLen+1, "Got length %ld, expected %ld\n", dwSize, dwExpectLen+1);

    dwSize--;
    hr = UrlCombineW(wszUrl1, wszUrl2, wszReturnUrl, &dwSize, dwFlags);
    ok(hr == E_POINTER, "UrlCombineW returned 0x%08lx, expected 0x%08lx\n", hr, E_POINTER);
    ok(dwSize == dwExpectLen+1, "Got length %ld, expected %ld\n", dwSize, dwExpectLen+1);

    hr = UrlCombineW(wszUrl1, wszUrl2, wszReturnUrl, &dwSize, dwFlags);
    ok(hr == S_OK, "Got unexpected hr %#lx.\n", hr);
    ok(dwSize == dwExpectLen, "Got length %ld, expected %ld\n", dwSize, dwExpectLen);
    wszConvertedUrl = GetWideString(szReturnUrl);
    ok(!wcscmp(wszReturnUrl, wszConvertedUrl), "Expected %s, got %s.\n",
            debugstr_w(wszConvertedUrl), debugstr_w(wszReturnUrl));
    FreeWideString(wszConvertedUrl);

    FreeWideString(wszUrl1);
    FreeWideString(wszUrl2);
    FreeWideString(wszExpectUrl);
}

/* ########################### */

static void test_UrlCombine(void)
{
    unsigned int i;
    for (i = 0; i < ARRAY_SIZE(TEST_COMBINE); i++) {
        check_url_combine(TEST_COMBINE[i].url1, TEST_COMBINE[i].url2, TEST_COMBINE[i].flags, TEST_COMBINE[i].expecturl, TEST_COMBINE[i].todo);
    }
}

/* ########################### */

static void test_UrlCreateFromPath(void)
{
    size_t i;
    char ret_url[INTERNET_MAX_URL_LENGTH];
    DWORD len, ret;
    WCHAR ret_urlW[INTERNET_MAX_URL_LENGTH];
    WCHAR *pathW, *urlW;

    for (i = 0; i < ARRAY_SIZE(TEST_URLFROMPATH); i++) {
        len = INTERNET_MAX_URL_LENGTH;
        ret = UrlCreateFromPathA(TEST_URLFROMPATH[i].path, ret_url, &len, 0);
        ok(ret == TEST_URLFROMPATH[i].ret, "ret %08lx from path %s\n", ret, TEST_URLFROMPATH[i].path);
        ok(!lstrcmpiA(ret_url, TEST_URLFROMPATH[i].url), "url %s from path %s\n", ret_url, TEST_URLFROMPATH[i].path);
        ok(len == strlen(ret_url), "ret len %ld from path %s\n", len, TEST_URLFROMPATH[i].path);

        len = INTERNET_MAX_URL_LENGTH;
        pathW = GetWideString(TEST_URLFROMPATH[i].path);
        urlW = GetWideString(TEST_URLFROMPATH[i].url);
        ret = UrlCreateFromPathW(pathW, ret_urlW, &len, 0);
        WideCharToMultiByte(CP_ACP, 0, ret_urlW, -1, ret_url, sizeof(ret_url),0,0);
        ok(ret == TEST_URLFROMPATH[i].ret, "ret %08lx from path L\"%s\", expected %08lx\n",
            ret, TEST_URLFROMPATH[i].path, TEST_URLFROMPATH[i].ret);
        ok(!lstrcmpiW(ret_urlW, urlW), "got %s expected %s from path L\"%s\"\n",
            ret_url, TEST_URLFROMPATH[i].url, TEST_URLFROMPATH[i].path);
        ok(len == lstrlenW(ret_urlW), "ret len %ld from path L\"%s\"\n", len, TEST_URLFROMPATH[i].path);
        FreeWideString(urlW);
        FreeWideString(pathW);
    }
}

/* ########################### */

static void test_UrlIs_null(DWORD flag)
{
    BOOL ret;
    ret = UrlIsA(NULL, flag);
    ok(ret == FALSE, "pUrlIsA(NULL, %ld) failed\n", flag);
    ret = UrlIsW(NULL, flag);
    ok(ret == FALSE, "pUrlIsW(NULL, %ld) failed\n", flag);
}

static void test_UrlIs(void)
{
    BOOL ret;
    size_t i;
    WCHAR wurl[80];

    test_UrlIs_null(URLIS_APPLIABLE);
    test_UrlIs_null(URLIS_DIRECTORY);
    test_UrlIs_null(URLIS_FILEURL);
    test_UrlIs_null(URLIS_HASQUERY);
    test_UrlIs_null(URLIS_NOHISTORY);
    test_UrlIs_null(URLIS_OPAQUE);
    test_UrlIs_null(URLIS_URL);

    for (i = 0; i < ARRAY_SIZE(TEST_PATH_IS_URL); i++) {
        MultiByteToWideChar(CP_ACP, 0, TEST_PATH_IS_URL[i].path, -1, wurl, ARRAY_SIZE(wurl));

        ret = UrlIsA( TEST_PATH_IS_URL[i].path, URLIS_URL );
        ok( ret == TEST_PATH_IS_URL[i].expect,
            "returned %d from path %s, expected %d\n", ret, TEST_PATH_IS_URL[i].path,
            TEST_PATH_IS_URL[i].expect );

        ret = UrlIsW( wurl, URLIS_URL );
        ok( ret == TEST_PATH_IS_URL[i].expect,
            "returned %d from path (UrlIsW) %s, expected %d\n", ret,
            TEST_PATH_IS_URL[i].path, TEST_PATH_IS_URL[i].expect );
    }
    for (i = 0; i < ARRAY_SIZE(TEST_URLIS_ATTRIBS); i++) {
        MultiByteToWideChar(CP_ACP, 0, TEST_URLIS_ATTRIBS[i].url, -1, wurl, ARRAY_SIZE(wurl));

        ret = UrlIsA( TEST_URLIS_ATTRIBS[i].url, URLIS_OPAQUE);
	ok( ret == TEST_URLIS_ATTRIBS[i].expectOpaque,
	    "returned %d for URLIS_OPAQUE, url \"%s\", expected %d\n", ret, TEST_URLIS_ATTRIBS[i].url,
	    TEST_URLIS_ATTRIBS[i].expectOpaque );
        ret = UrlIsA( TEST_URLIS_ATTRIBS[i].url, URLIS_FILEURL);
	ok( ret == TEST_URLIS_ATTRIBS[i].expectFile,
	    "returned %d for URLIS_FILEURL, url \"%s\", expected %d\n", ret, TEST_URLIS_ATTRIBS[i].url,
	    TEST_URLIS_ATTRIBS[i].expectFile );

        ret = UrlIsW( wurl, URLIS_OPAQUE);
        ok( ret == TEST_URLIS_ATTRIBS[i].expectOpaque,
            "returned %d for URLIS_OPAQUE (UrlIsW), url \"%s\", expected %d\n",
            ret, TEST_URLIS_ATTRIBS[i].url, TEST_URLIS_ATTRIBS[i].expectOpaque );
        ret = UrlIsW( wurl, URLIS_FILEURL);
        ok( ret == TEST_URLIS_ATTRIBS[i].expectFile,
            "returned %d for URLIS_FILEURL (UrlIsW), url \"%s\", expected %d\n",
            ret, TEST_URLIS_ATTRIBS[i].url, TEST_URLIS_ATTRIBS[i].expectFile );
    }
}

/* ########################### */

static void test_UrlUnescape(void)
{
    CHAR szReturnUrl[INTERNET_MAX_URL_LENGTH];
    WCHAR ret_urlW[INTERNET_MAX_URL_LENGTH];
    WCHAR *urlW, *expected_urlW;
    DWORD dwEscaped;
    size_t i;
    static char inplace[] = "file:///C:/Program%20Files";
    static char another_inplace[] = "file:///C:/Program%20Files";
    static const char expected[] = "file:///C:/Program Files";
    static WCHAR inplaceW[] = L"file:///C:/Program Files";
    static WCHAR another_inplaceW[] = L"file:///C:/Program%20Files";
    HRESULT res;

    for (i = 0; i < ARRAY_SIZE(TEST_URL_UNESCAPE); i++) {
        dwEscaped=INTERNET_MAX_URL_LENGTH;
        res = UrlUnescapeA(TEST_URL_UNESCAPE[i].url, szReturnUrl, &dwEscaped, 0);
        ok(res == S_OK,
            "UrlUnescapeA returned 0x%lx (expected S_OK) for \"%s\"\n",
            res, TEST_URL_UNESCAPE[i].url);
        ok(strcmp(szReturnUrl,TEST_URL_UNESCAPE[i].expect)==0, "Expected \"%s\", but got \"%s\" from \"%s\"\n", TEST_URL_UNESCAPE[i].expect, szReturnUrl, TEST_URL_UNESCAPE[i].url);

        ZeroMemory(szReturnUrl, sizeof(szReturnUrl));
        /* if we set the buffer pointer to NULL here, UrlUnescape fails and the string is not converted */
        res = UrlUnescapeA(TEST_URL_UNESCAPE[i].url, szReturnUrl, NULL, 0);
        ok(res == E_INVALIDARG,
            "UrlUnescapeA returned 0x%lx (expected E_INVALIDARG) for \"%s\"\n",
            res, TEST_URL_UNESCAPE[i].url);
        ok(strcmp(szReturnUrl,"")==0, "Expected empty string\n");

        dwEscaped = INTERNET_MAX_URL_LENGTH;
        urlW = GetWideString(TEST_URL_UNESCAPE[i].url);
        expected_urlW = GetWideString(TEST_URL_UNESCAPE[i].expect);
        res = UrlUnescapeW(urlW, ret_urlW, &dwEscaped, 0);
        ok(res == S_OK,
            "UrlUnescapeW returned 0x%lx (expected S_OK) for \"%s\"\n",
            res, TEST_URL_UNESCAPE[i].url);

        WideCharToMultiByte(CP_ACP,0,ret_urlW,-1,szReturnUrl,INTERNET_MAX_URL_LENGTH,0,0);
        ok(lstrcmpW(ret_urlW, expected_urlW)==0,
            "Expected \"%s\", but got \"%s\" from \"%s\" flags %08lx\n",
            TEST_URL_UNESCAPE[i].expect, szReturnUrl, TEST_URL_UNESCAPE[i].url, 0L);
        FreeWideString(urlW);
        FreeWideString(expected_urlW);
    }

    dwEscaped = sizeof(inplace);
    res = UrlUnescapeA(inplace, NULL, &dwEscaped, URL_UNESCAPE_INPLACE);
    ok(res == S_OK, "UrlUnescapeA returned 0x%lx (expected S_OK)\n", res);
    ok(!strcmp(inplace, expected), "got %s expected %s\n", inplace, expected);
    ok(dwEscaped == 27, "got %ld expected 27\n", dwEscaped);

    /* if we set the buffer pointer to NULL, the string apparently still gets converted (Google Lively does this) */
    res = UrlUnescapeA(another_inplace, NULL, NULL, URL_UNESCAPE_INPLACE);
    ok(res == S_OK, "UrlUnescapeA returned 0x%lx (expected S_OK)\n", res);
    ok(!strcmp(another_inplace, expected), "got %s expected %s\n", another_inplace, expected);

    dwEscaped = sizeof(inplaceW);
    res = UrlUnescapeW(inplaceW, NULL, &dwEscaped, URL_UNESCAPE_INPLACE);
    ok(res == S_OK, "UrlUnescapeW returned 0x%lx (expected S_OK)\n", res);
    ok(dwEscaped == 50, "got %ld expected 50\n", dwEscaped);

    /* if we set the buffer pointer to NULL, the string apparently still gets converted (Google Lively does this) */
    res = UrlUnescapeW(another_inplaceW, NULL, NULL, URL_UNESCAPE_INPLACE);
    ok(res == S_OK, "UrlUnescapeW returned 0x%lx (expected S_OK)\n", res);

    ok(lstrlenW(another_inplaceW) == 24, "got %d expected 24\n", lstrlenW(another_inplaceW));
}

static const struct parse_url_test_t {
    const char *url;
    HRESULT hres;
    UINT protocol_len;
    UINT scheme;
} parse_url_tests[] = {
    {"http://www.winehq.org/",S_OK,4,URL_SCHEME_HTTP},
    {"https://www.winehq.org/",S_OK,5,URL_SCHEME_HTTPS},
    {"ftp://www.winehq.org/",S_OK,3,URL_SCHEME_FTP},
    {"test.txt?test=c:/dir",URL_E_INVALID_SYNTAX},
    {"test.txt",URL_E_INVALID_SYNTAX},
    {"xxx://www.winehq.org/",S_OK,3,URL_SCHEME_UNKNOWN},
    {"1xx://www.winehq.org/",S_OK,3,URL_SCHEME_UNKNOWN},
    {"-xx://www.winehq.org/",S_OK,3,URL_SCHEME_UNKNOWN},
    {"xx0://www.winehq.org/",S_OK,3,URL_SCHEME_UNKNOWN},
    {"x://www.winehq.org/",URL_E_INVALID_SYNTAX},
    {"xx$://www.winehq.org/",URL_E_INVALID_SYNTAX},
    {"htt?p://www.winehq.org/",URL_E_INVALID_SYNTAX},
    {"ab-://www.winehq.org/",S_OK,3,URL_SCHEME_UNKNOWN},
    {" http://www.winehq.org/",URL_E_INVALID_SYNTAX},
    {"HTTP://www.winehq.org/",S_OK,4,URL_SCHEME_HTTP},
    {"a+-.://www.winehq.org/",S_OK,4,URL_SCHEME_UNKNOWN},
};

static void test_ParseURL(void)
{
    const struct parse_url_test_t *test;
    WCHAR url[INTERNET_MAX_URL_LENGTH];
    PARSEDURLA parseda;
    PARSEDURLW parsedw;
    HRESULT hres;

    for (test = parse_url_tests; test < parse_url_tests + ARRAY_SIZE(parse_url_tests); test++) {
        memset(&parseda, 0xd0, sizeof(parseda));
        parseda.cbSize = sizeof(parseda);
        hres = ParseURLA(test->url, &parseda);
        ok(hres == test->hres, "ParseURL failed: %08lx, expected %08lx\n", hres, test->hres);
        if(hres == S_OK) {
            ok(parseda.pszProtocol == test->url, "parseda.pszProtocol = %s, expected %s\n",
               parseda.pszProtocol, test->url);
            ok(parseda.cchProtocol == test->protocol_len, "parseda.cchProtocol = %d, expected %d\n",
               parseda.cchProtocol, test->protocol_len);
            ok(parseda.pszSuffix == test->url+test->protocol_len+1, "parseda.pszSuffix = %s, expected %s\n",
               parseda.pszSuffix, test->url+test->protocol_len+1);
            ok(parseda.cchSuffix == strlen(test->url+test->protocol_len+1),
               "parseda.pszSuffix = %d, expected %d\n",
               parseda.cchSuffix, lstrlenA(test->url+test->protocol_len+1));
            ok(parseda.nScheme == test->scheme, "parseda.nScheme = %d, expected %d\n",
               parseda.nScheme, test->scheme);
        }else {
            ok(!parseda.pszProtocol, "parseda.pszProtocol = %p\n", parseda.pszProtocol);
            ok(parseda.nScheme == 0xd0d0d0d0, "nScheme = %d\n", parseda.nScheme);
        }

        MultiByteToWideChar(CP_ACP, 0, test->url, -1, url, ARRAY_SIZE(url));

        memset(&parsedw, 0xd0, sizeof(parsedw));
        parsedw.cbSize = sizeof(parsedw);
        hres = ParseURLW(url, &parsedw);
        ok(hres == test->hres, "ParseURL failed: %08lx, expected %08lx\n", hres, test->hres);
        if(hres == S_OK) {
            ok(parsedw.pszProtocol == url, "parsedw.pszProtocol = %s, expected %s\n",
               wine_dbgstr_w(parsedw.pszProtocol), wine_dbgstr_w(url));
            ok(parsedw.cchProtocol == test->protocol_len, "parsedw.cchProtocol = %d, expected %d\n",
               parsedw.cchProtocol, test->protocol_len);
            ok(parsedw.pszSuffix == url+test->protocol_len+1, "parsedw.pszSuffix = %s, expected %s\n",
               wine_dbgstr_w(parsedw.pszSuffix), wine_dbgstr_w(url+test->protocol_len+1));
            ok(parsedw.cchSuffix == strlen(test->url+test->protocol_len+1),
               "parsedw.pszSuffix = %d, expected %d\n",
               parsedw.cchSuffix, lstrlenA(test->url+test->protocol_len+1));
            ok(parsedw.nScheme == test->scheme, "parsedw.nScheme = %d, expected %d\n",
               parsedw.nScheme, test->scheme);
        }else {
            ok(!parsedw.pszProtocol, "parsedw.pszProtocol = %p\n", parseda.pszProtocol);
            ok(parsedw.nScheme == 0xd0d0d0d0, "nScheme = %d\n", parsedw.nScheme);
        }
    }
}

static void test_HashData(void)
{
    HRESULT res;
    BYTE input[16] = {0x51, 0x33, 0x4F, 0xA7, 0x45, 0x15, 0xF0, 0x52, 0x90,
                      0x2B, 0xE7, 0xF5, 0xFD, 0xE1, 0xA6, 0xA7};
    BYTE output[32];
    static const BYTE expected[] = {0x54, 0x9C, 0x92, 0x55, 0xCD, 0x82, 0xFF,
                                    0xA1, 0x8E, 0x0F, 0xCF, 0x93, 0x14, 0xAA,
                                    0xE3, 0x2D};
    static const BYTE expected2[] = {0x54, 0x9C, 0x92, 0x55, 0xCD, 0x82, 0xFF,
                                     0xA1, 0x8E, 0x0F, 0xCF, 0x93, 0x14, 0xAA,
                                     0xE3, 0x2D, 0x47, 0xFC, 0x80, 0xB8, 0xD0,
                                     0x49, 0xE6, 0x13, 0x2A, 0x30, 0x51, 0x8D,
                                     0xF9, 0x4B, 0x07, 0xA6};
    static const BYTE expected3[] = {0x2B, 0xDC, 0x9A, 0x1B, 0xF0, 0x5A, 0xF9,
                                     0xC6, 0xBE, 0x94, 0x6D, 0xF3, 0x33, 0xC1,
                                     0x36, 0x07};
    int i;

    /* Test hashing with identically sized input/output buffers. */
    res = HashData(input, 16, output, 16);
    ok(res == S_OK, "Expected HashData to return S_OK, got 0x%08lx\n", res);
    ok(!memcmp(output, expected, sizeof(expected)), "data didn't match\n");

    /* Test hashing with larger output buffer. */
    res = HashData(input, 16, output, 32);
    ok(res == S_OK, "Expected HashData to return S_OK, got 0x%08lx\n", res);
    ok(!memcmp(output, expected2, sizeof(expected2)), "data didn't match\n");

    /* Test hashing with smaller input buffer. */
    res = HashData(input, 8, output, 16);
    ok(res == S_OK, "Expected HashData to return S_OK, got 0x%08lx\n", res);
    ok(!memcmp(output, expected3, sizeof(expected3)), "data didn't match\n");

    /* Test passing NULL pointers for input/output parameters. */
    res = HashData(NULL, 0, NULL, 0);
    ok(res == E_INVALIDARG, "Got unexpected hr %#lx.\n", res);

    res = HashData(input, 0, NULL, 0);
    ok(res == E_INVALIDARG, "Got unexpected hr %#lx.\n", res);

    res = HashData(NULL, 0, output, 0);
    ok(res == E_INVALIDARG, "Got unexpected hr %#lx.\n", res);

    /* Test passing valid pointers with sizes of zero. */
    for (i = 0; i < ARRAY_SIZE(input); i++)
        input[i] = 0x00;

    for (i = 0; i < ARRAY_SIZE(output); i++)
        output[i] = 0xFF;

    res = HashData(input, 0, output, 0);
    ok(res == S_OK, "Expected HashData to return S_OK, got 0x%08lx\n", res);

    /* The buffers should be unchanged. */
    for (i = 0; i < ARRAY_SIZE(input); i++)
        ok(input[i] == 0x00, "Expected the input buffer to be unchanged\n");

    for (i = 0; i < ARRAY_SIZE(output); i++)
        ok(output[i] == 0xFF, "Expected the output buffer to be unchanged\n");

    /* Input/output parameters are not validated. */
    res = HashData((BYTE *)0xdeadbeef, 0, (BYTE *)0xdeadbeef, 0);
    ok(res == S_OK, "Expected HashData to return S_OK, got 0x%08lx\n", res);

    if (0)
    {
        res = HashData((BYTE *)0xdeadbeef, 1, (BYTE *)0xdeadbeef, 1);
        trace("HashData returned 0x%08lx\n", res);
    }
}

/* ########################### */

START_TEST(url)
{
  test_UrlApplyScheme();
  test_UrlHash();
  test_UrlGetPart();
  test_UrlCanonicalizeA();
  test_UrlCanonicalizeW();
  test_UrlEscapeA();
  test_UrlEscapeW();
  test_UrlCombine();
  test_UrlCreateFromPath();
  test_UrlIs();
  test_UrlUnescape();
  test_ParseURL();
  test_HashData();
}
