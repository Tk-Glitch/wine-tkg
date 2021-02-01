/*
 * NTDLL string functions
 *
 * Copyright 2000 Alexandre Julliard
 * Copyright 2000 Jon Griffiths
 * Copyright 2003 Thomas Mertes
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

#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "windef.h"
#include "winbase.h"
#include "winnls.h"
#include "winternl.h"
#include "ntdll_misc.h"


/* same as wctypes except for TAB, which doesn't have C1_BLANK for some reason... */
static const unsigned short ctypes[257] =
{
    /* -1 */
    0x0000,
    /* 00 */
    0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020,
    0x0020, 0x0028, 0x0028, 0x0028, 0x0028, 0x0028, 0x0020, 0x0020,
    /* 10 */
    0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020,
    0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020,
    /* 20 */
    0x0048, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    /* 30 */
    0x0084, 0x0084, 0x0084, 0x0084, 0x0084, 0x0084, 0x0084, 0x0084,
    0x0084, 0x0084, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    /* 40 */
    0x0010, 0x0181, 0x0181, 0x0181, 0x0181, 0x0181, 0x0181, 0x0101,
    0x0101, 0x0101, 0x0101, 0x0101, 0x0101, 0x0101, 0x0101, 0x0101,
    /* 50 */
    0x0101, 0x0101, 0x0101, 0x0101, 0x0101, 0x0101, 0x0101, 0x0101,
    0x0101, 0x0101, 0x0101, 0x0010, 0x0010, 0x0010, 0x0010, 0x0010,
    /* 60 */
    0x0010, 0x0182, 0x0182, 0x0182, 0x0182, 0x0182, 0x0182, 0x0102,
    0x0102, 0x0102, 0x0102, 0x0102, 0x0102, 0x0102, 0x0102, 0x0102,
    /* 70 */
    0x0102, 0x0102, 0x0102, 0x0102, 0x0102, 0x0102, 0x0102, 0x0102,
    0x0102, 0x0102, 0x0102, 0x0010, 0x0010, 0x0010, 0x0010, 0x0020
};


/*********************************************************************
 *                  memchr   (NTDLL.@)
 */
void * __cdecl memchr( const void *ptr, int c, size_t n )
{
    const unsigned char *p = ptr;

    for (p = ptr; n; n--, p++) if (*p == (unsigned char)c) return (void *)(ULONG_PTR)p;
    return NULL;
}


/*********************************************************************
 *                  memcmp   (NTDLL.@)
 */
int __cdecl memcmp( const void *ptr1, const void *ptr2, size_t n )
{
    const unsigned char *p1, *p2;

    for (p1 = ptr1, p2 = ptr2; n; n--, p1++, p2++)
    {
        if (*p1 < *p2) return -1;
        if (*p1 > *p2) return 1;
    }
    return 0;
}


/*********************************************************************
 *                  memcpy   (NTDLL.@)
 *
 * NOTES
 *  Behaves like memmove.
 */
void * __cdecl memcpy( void *dst, const void *src, size_t n )
{
    volatile unsigned char *d = dst;  /* avoid gcc optimizations */
    const unsigned char *s = src;

    if ((size_t)dst - (size_t)src >= n)
    {
        while (n--) *d++ = *s++;
    }
    else
    {
        d += n - 1;
        s += n - 1;
        while (n--) *d-- = *s--;
    }
    return dst;
}


/*********************************************************************
 *                  memmove   (NTDLL.@)
 */
void * __cdecl memmove( void *dst, const void *src, size_t n )
{
    volatile unsigned char *d = dst;  /* avoid gcc optimizations */
    const unsigned char *s = src;

    if ((size_t)dst - (size_t)src >= n)
    {
        while (n--) *d++ = *s++;
    }
    else
    {
        d += n - 1;
        s += n - 1;
        while (n--) *d-- = *s--;
    }
    return dst;
}


/*********************************************************************
 *                  memset   (NTDLL.@)
 */
void * __cdecl memset( void *dst, int c, size_t n )
{
    volatile unsigned char *d = dst;  /* avoid gcc optimizations */
    while (n--) *d++ = c;
    return dst;
}


/*********************************************************************
 *                  strcat   (NTDLL.@)
 */
char * __cdecl strcat( char *dst, const char *src )
{
    char *d = dst;
    while (*d) d++;
    while ((*d++ = *src++));
    return dst;
}


/*********************************************************************
 *                  strchr   (NTDLL.@)
 */
char * __cdecl strchr( const char *str, int c )
{
    do { if (*str == (char)c) return (char *)(ULONG_PTR)str; } while (*str++);
    return NULL;
}


/*********************************************************************
 *                  strcmp   (NTDLL.@)
 */
int __cdecl strcmp( const char *str1, const char *str2 )
{
    while (*str1 && *str1 == *str2) { str1++; str2++; }
    if ((unsigned char)*str1 > (unsigned char)*str2) return 1;
    if ((unsigned char)*str1 < (unsigned char)*str2) return -1;
    return 0;
}


/*********************************************************************
 *                  strcpy   (NTDLL.@)
 */
char * __cdecl strcpy( char *dst, const char *src )
{
    char *d = dst;
    while ((*d++ = *src++));
    return dst;
}


/*********************************************************************
 *                  strcspn   (NTDLL.@)
 */
size_t __cdecl strcspn( const char *str, const char *reject )
{
    const char *ptr;
    for (ptr = str; *ptr; ptr++) if (strchr( reject, *ptr )) break;
    return ptr - str;
}


/*********************************************************************
 *                  strlen   (NTDLL.@)
 */
size_t __cdecl strlen( const char *str )
{
    const char *s = str;
    while (*s) s++;
    return s - str;
}


/*********************************************************************
 *                  strncat   (NTDLL.@)
 */
char * __cdecl strncat( char *dst, const char *src, size_t len )
{
    char *d = dst;
    while (*d) d++;
    for ( ; len && *src; d++, src++, len--) *d = *src;
    *d = 0;
    return dst;
}


/*********************************************************************
 *                  strncmp   (NTDLL.@)
 */
int __cdecl strncmp( const char *str1, const char *str2, size_t len )
{
    if (!len) return 0;
    while (--len && *str1 && *str1 == *str2) { str1++; str2++; }
    return (unsigned char)*str1 - (unsigned char)*str2;
}


/*********************************************************************
 *                  strncpy   (NTDLL.@)
 */
#undef strncpy
char * __cdecl strncpy( char *dst, const char *src, size_t len )
{
    char *d;
    for (d = dst; len && *src; d++, src++, len--) *d = *src;
    while (len--) *d++ = 0;
    return dst;
}


/*********************************************************************
 *                  strnlen   (NTDLL.@)
 */
size_t __cdecl strnlen( const char *str, size_t len )
{
    const char *s = str;
    for (s = str; len && *s; s++, len--) ;
    return s - str;
}


/*********************************************************************
 *                  strpbrk   (NTDLL.@)
 */
char * __cdecl strpbrk( const char *str, const char *accept )
{
    for ( ; *str; str++) if (strchr( accept, *str )) return (char *)(ULONG_PTR)str;
    return NULL;
}


/*********************************************************************
 *                  strrchr   (NTDLL.@)
 */
char * __cdecl strrchr( const char *str, int c )
{
    char *ret = NULL;
    do { if (*str == (char)c) ret = (char *)(ULONG_PTR)str; } while (*str++);
    return ret;
}


/*********************************************************************
 *                  strspn   (NTDLL.@)
 */
size_t __cdecl strspn( const char *str, const char *accept )
{
    const char *ptr;
    for (ptr = str; *ptr; ptr++) if (!strchr( accept, *ptr )) break;
    return ptr - str;
}


/*********************************************************************
 *                  strstr   (NTDLL.@)
 */
char * __cdecl strstr( const char *str, const char *sub )
{
    while (*str)
    {
        const char *p1 = str, *p2 = sub;
        while (*p1 && *p2 && *p1 == *p2) { p1++; p2++; }
        if (!*p2) return (char *)str;
        str++;
    }
    return NULL;
}


/*********************************************************************
 *                  _memccpy   (NTDLL.@)
 */
void * __cdecl _memccpy( void *dst, const void *src, int c, size_t n )
{
    unsigned char *d = dst;
    const unsigned char *s = src;
    while (n--) if ((*d++ = *s++) == (unsigned char)c) return d;
    return NULL;
}


/*********************************************************************
 *                  tolower   (NTDLL.@)
 */
int __cdecl tolower( int c )
{
    return (char)c >= 'A' && (char)c <= 'Z' ? c - 'A' + 'a' : c;
}


/*********************************************************************
 *                  _memicmp   (NTDLL.@)
 *
 * Compare two blocks of memory as strings, ignoring case.
 *
 * PARAMS
 *  s1  [I] First string to compare to s2
 *  s2  [I] Second string to compare to s1
 *  len [I] Number of bytes to compare
 *
 * RETURNS
 *  An integer less than, equal to, or greater than zero indicating that
 *  s1 is less than, equal to or greater than s2 respectively.
 *
 * NOTES
 *  Any Nul characters in s1 or s2 are ignored. This function always
 *  compares up to len bytes or the first place where s1 and s2 differ.
 */
int __cdecl _memicmp( const void *str1, const void *str2, size_t len )
{
    const unsigned char *s1 = str1, *s2 = str2;
    int ret = 0;
    while (len--)
    {
        if ((ret = tolower(*s1) - tolower(*s2))) break;
        s1++;
        s2++;
    }
    return ret;
}


/*********************************************************************
 *                  _strnicmp   (NTDLL.@)
 */
int __cdecl _strnicmp( LPCSTR str1, LPCSTR str2, size_t n )
{
    int l1, l2;

    while (n--)
    {
        l1 = (unsigned char)tolower(*str1);
        l2 = (unsigned char)tolower(*str2);
        if (l1 != l2)
        {
            if (sizeof(void *) > sizeof(int)) return l1 - l2;
            return l1 - l2 > 0 ? 1 : -1;
        }
        if (!l1) return 0;
        str1++;
        str2++;
    }
    return 0;
}


/*********************************************************************
 *                  _stricmp   (NTDLL.@)
 *                  _strcmpi   (NTDLL.@)
 */
int __cdecl _stricmp( LPCSTR str1, LPCSTR str2 )
{
    return _strnicmp( str1, str2, -1 );
}


/*********************************************************************
 *                  _strupr   (NTDLL.@)
 *
 * Convert a string to upper case.
 *
 * PARAMS
 *  str [I/O] String to convert
 *
 * RETURNS
 *  str. There is no error return, if str is NULL or invalid, this
 *  function will crash.
 */
LPSTR __cdecl _strupr( LPSTR str )
{
    LPSTR ret = str;
    for ( ; *str; str++) *str = RtlUpperChar(*str);
    return ret;
}


/*********************************************************************
 *                  _strlwr   (NTDLL.@)
 *
 * Convert a string to lowercase
 *
 * PARAMS
 *  str [I/O] String to convert
 *
 * RETURNS
 *  str. There is no error return, if str is NULL or invalid, this
 *  function will crash.
 */
LPSTR __cdecl _strlwr( LPSTR str )
{
    LPSTR ret = str;
    for ( ; *str; str++) *str = tolower(*str);
    return ret;
}


/*********************************************************************
 *                  toupper   (NTDLL.@)
 */
int __cdecl toupper( int c )
{
    char str[2], *p = str;
    WCHAR wc;
    DWORD len;

    str[0] = c;
    str[1] = c >> 8;
    wc = RtlAnsiCharToUnicodeChar( &p );
    wc = RtlUpcaseUnicodeChar( wc );
    RtlUnicodeToMultiByteN( str, sizeof(str), &len, &wc, sizeof(wc) );
    if (len == 2) return ((unsigned char)str[0] << 8) + (unsigned char)str[1];
    return (unsigned char)str[0];
}


/*********************************************************************
 *                  isalnum   (NTDLL.@)
 */
int __cdecl isalnum( int c )
{
    return ctypes[c + 1] & (C1_LOWER | C1_UPPER | C1_DIGIT);
}


/*********************************************************************
 *                  isalpha   (NTDLL.@)
 */
int __cdecl isalpha( int c )
{
    return ctypes[c + 1] & (C1_LOWER | C1_UPPER);
}


/*********************************************************************
 *                  iscntrl   (NTDLL.@)
 */
int __cdecl iscntrl( int c )
{
    return ctypes[c + 1] & C1_CNTRL;
}


/*********************************************************************
 *                  isdigit   (NTDLL.@)
 */
int __cdecl isdigit( int c )
{
    return ctypes[c + 1] & C1_DIGIT;
}


/*********************************************************************
 *                  isgraph   (NTDLL.@)
 */
int __cdecl isgraph( int c )
{
    return ctypes[c + 1] & (C1_LOWER | C1_UPPER | C1_DIGIT | C1_PUNCT);
}


/*********************************************************************
 *                  islower   (NTDLL.@)
 */
int __cdecl islower( int c )
{
    return ctypes[c + 1] & C1_LOWER;
}


/*********************************************************************
 *                  isprint   (NTDLL.@)
 */
int __cdecl isprint( int c )
{
    return ctypes[c + 1] & (C1_LOWER | C1_UPPER | C1_DIGIT | C1_PUNCT | C1_BLANK);
}


/*********************************************************************
 *                  ispunct   (NTDLL.@)
 */
int __cdecl ispunct( int c )
{
    return ctypes[c + 1] & C1_PUNCT;
}


/*********************************************************************
 *                  isspace   (NTDLL.@)
 */
int __cdecl isspace( int c )
{
    return ctypes[c + 1] & C1_SPACE;
}


/*********************************************************************
 *                  isupper   (NTDLL.@)
 */
int __cdecl isupper( int c )
{
    return ctypes[c + 1] & C1_UPPER;
}


/*********************************************************************
 *                  isxdigit   (NTDLL.@)
 */
int __cdecl isxdigit( int c )
{
    return ctypes[c + 1] & C1_XDIGIT;
}


/*********************************************************************
 *		__isascii (NTDLL.@)
 */
int CDECL __isascii(int c)
{
    return (unsigned)c < 0x80;
}


/*********************************************************************
 *		__toascii (NTDLL.@)
 */
int CDECL __toascii(int c)
{
    return (unsigned)c & 0x7f;
}


/*********************************************************************
 *		__iscsym (NTDLL.@)
 */
int CDECL __iscsym(int c)
{
    return (c < 127 && (isalnum(c) || c == '_'));
}


/*********************************************************************
 *		__iscsymf (NTDLL.@)
 */
int CDECL __iscsymf(int c)
{
    return (c < 127 && (isalpha(c) || c == '_'));
}


/*********************************************************************
 *		_toupper (NTDLL.@)
 */
int CDECL _toupper(int c)
{
    return c - 0x20;  /* sic */
}


/*********************************************************************
 *		_tolower (NTDLL.@)
 */
int CDECL _tolower(int c)
{
    return c + 0x20;  /* sic */
}


static int char_to_int( char c )
{
    if ('0' <= c && c <= '9') return c - '0';
    if ('A' <= c && c <= 'Z') return c - 'A' + 10;
    if ('a' <= c && c <= 'z') return c - 'a' + 10;
    return -1;
}

/*********************************************************************
 *                  strtol   (NTDLL.@)
 */
__msvcrt_long __cdecl strtol( const char *s, char **end, int base )
{
    BOOL negative = FALSE, empty = TRUE;
    LONG ret = 0;

    if (base < 0 || base == 1 || base > 36) return 0;
    if (end) *end = (char *)s;
    while (isspace(*s)) s++;

    if (*s == '-')
    {
        negative = TRUE;
        s++;
    }
    else if (*s == '+') s++;

    if ((base == 0 || base == 16) && !char_to_int( *s ) && (s[1] == 'x' || s[1] == 'X'))
    {
        base = 16;
        s += 2;
    }
    if (base == 0) base = char_to_int( *s ) ? 10 : 8;

    while (*s)
    {
        int v = char_to_int( *s );
        if (v < 0 || v >= base) break;
        if (negative) v = -v;
        s++;
        empty = FALSE;

        if (!negative && (ret > MAXLONG / base || ret * base > MAXLONG - v))
            ret = MAXLONG;
        else if (negative && (ret < (LONG)MINLONG / base || ret * base < (LONG)(MINLONG - v)))
            ret = MINLONG;
        else
            ret = ret * base + v;
    }

    if (end && !empty) *end = (char *)s;
    return ret;
}


/*********************************************************************
 *                  strtoul   (NTDLL.@)
 */
__msvcrt_ulong __cdecl strtoul( const char *s, char **end, int base )
{
    BOOL negative = FALSE, empty = TRUE;
    ULONG ret = 0;

    if (base < 0 || base == 1 || base > 36) return 0;
    if (end) *end = (char *)s;
    while (isspace(*s)) s++;

    if (*s == '-')
    {
        negative = TRUE;
        s++;
    }
    else if (*s == '+') s++;

    if ((base == 0 || base == 16) && !char_to_int( *s ) && (s[1] == 'x' || s[1] == 'X'))
    {
        base = 16;
        s += 2;
    }
    if (base == 0) base = char_to_int( *s ) ? 10 : 8;

    while (*s)
    {
        int v = char_to_int( *s );
        if (v < 0 || v >= base) break;
        s++;
        empty = FALSE;

        if (ret > MAXDWORD / base || ret * base > MAXDWORD - v)
            ret = MAXDWORD;
        else
            ret = ret * base + v;
    }

    if (end && !empty) *end = (char *)s;
    return negative ? -ret : ret;
}


/*********************************************************************
 *      _ultoa   (NTDLL.@)
 *
 * Convert an unsigned long integer to a string.
 *
 * RETURNS
 *  str.
 *
 * NOTES
 *  - Converts value to a Nul terminated string which is copied to str.
 *  - The maximum length of the copied str is 33 bytes.
 *  - Does not check if radix is in the range of 2 to 36.
 *  - If str is NULL it crashes, as the native function does.
 */
char * __cdecl _ultoa( __msvcrt_ulong value, char *str, int radix )
{
    char buffer[33];
    char *pos;
    int digit;

    pos = &buffer[32];
    *pos = '\0';

    do {
	digit = value % radix;
	value = value / radix;
	if (digit < 10) {
	    *--pos = '0' + digit;
	} else {
	    *--pos = 'a' + digit - 10;
	} /* if */
    } while (value != 0L);

    memcpy(str, pos, &buffer[32] - pos + 1);
    return str;
}


/*********************************************************************
 *      _ltoa   (NTDLL.@)
 *
 * Convert a long integer to a string.
 *
 * RETURNS
 *  str.
 *
 * NOTES
 *  - Converts value to a Nul terminated string which is copied to str.
 *  - The maximum length of the copied str is 33 bytes. If radix
 *  is 10 and value is negative, the value is converted with sign.
 *  - Does not check if radix is in the range of 2 to 36.
 *  - If str is NULL it crashes, as the native function does.
 */
char * __cdecl _ltoa( __msvcrt_long value, char *str, int radix )
{
    ULONG val;
    int negative;
    char buffer[33];
    char *pos;
    int digit;

    if (value < 0 && radix == 10) {
	negative = 1;
        val = -value;
    } else {
	negative = 0;
        val = value;
    } /* if */

    pos = &buffer[32];
    *pos = '\0';

    do {
	digit = val % radix;
	val = val / radix;
	if (digit < 10) {
	    *--pos = '0' + digit;
	} else {
	    *--pos = 'a' + digit - 10;
	} /* if */
    } while (val != 0L);

    if (negative) {
	*--pos = '-';
    } /* if */

    memcpy(str, pos, &buffer[32] - pos + 1);
    return str;
}


/*********************************************************************
 *      _itoa    (NTDLL.@)
 *
 * Converts an integer to a string.
 *
 * RETURNS
 *  str.
 *
 * NOTES
 *  - Converts value to a '\0' terminated string which is copied to str.
 *  - The maximum length of the copied str is 33 bytes. If radix
 *  is 10 and value is negative, the value is converted with sign.
 *  - Does not check if radix is in the range of 2 to 36.
 *  - If str is NULL it crashes, as the native function does.
 */
char * __cdecl _itoa(
    int value, /* [I] Value to be converted */
    char *str, /* [O] Destination for the converted value */
    int radix) /* [I] Number base for conversion */
{
    return _ltoa(value, str, radix);
}


/*********************************************************************
 *      _ui64toa   (NTDLL.@)
 *
 * Converts a large unsigned integer to a string.
 *
 * RETURNS
 *  str.
 *
 * NOTES
 *  - Converts value to a '\0' terminated string which is copied to str.
 *  - The maximum length of the copied str is 65 bytes.
 *  - Does not check if radix is in the range of 2 to 36.
 *  - If str is NULL it crashes, as the native function does.
 */
char * __cdecl _ui64toa(
    ULONGLONG value, /* [I] Value to be converted */
    char *str,       /* [O] Destination for the converted value */
    int radix)       /* [I] Number base for conversion */
{
    char buffer[65];
    char *pos;
    int digit;

    pos = &buffer[64];
    *pos = '\0';

    do {
	digit = value % radix;
	value = value / radix;
	if (digit < 10) {
	    *--pos = '0' + digit;
	} else {
	    *--pos = 'a' + digit - 10;
	} /* if */
    } while (value != 0L);

    memcpy(str, pos, &buffer[64] - pos + 1);
    return str;
}


/*********************************************************************
 *      _i64toa   (NTDLL.@)
 *
 * Converts a large integer to a string.
 *
 * RETURNS
 *  str.
 *
 * NOTES
 *  - Converts value to a Nul terminated string which is copied to str.
 *  - The maximum length of the copied str is 65 bytes. If radix
 *  is 10 and value is negative, the value is converted with sign.
 *  - Does not check if radix is in the range of 2 to 36.
 *  - If str is NULL it crashes, as the native function does.
 *
 * DIFFERENCES
 * - The native DLL converts negative values (for base 10) wrong:
 *|                     -1 is converted to -18446744073709551615
 *|                     -2 is converted to -18446744073709551614
 *|   -9223372036854775807 is converted to  -9223372036854775809
 *|   -9223372036854775808 is converted to  -9223372036854775808
 *   The native msvcrt _i64toa function and our ntdll _i64toa function
 *   do not have this bug.
 */
char * __cdecl _i64toa(
    LONGLONG value, /* [I] Value to be converted */
    char *str,      /* [O] Destination for the converted value */
    int radix)      /* [I] Number base for conversion */
{
    ULONGLONG val;
    int negative;
    char buffer[65];
    char *pos;
    int digit;

    if (value < 0 && radix == 10) {
	negative = 1;
        val = -value;
    } else {
	negative = 0;
        val = value;
    } /* if */

    pos = &buffer[64];
    *pos = '\0';

    do {
	digit = val % radix;
	val = val / radix;
	if (digit < 10) {
	    *--pos = '0' + digit;
	} else {
	    *--pos = 'a' + digit - 10;
	} /* if */
    } while (val != 0L);

    if (negative) {
	*--pos = '-';
    } /* if */

    memcpy(str, pos, &buffer[64] - pos + 1);
    return str;
}


/*********************************************************************
 *      _atoi64   (NTDLL.@)
 *
 * Convert a string to a large integer.
 *
 * PARAMS
 *  str [I] String to be converted
 *
 * RETURNS
 *  Success: The integer value represented by str.
 *  Failure: 0. Note that this cannot be distinguished from a successful
 *           return, if the string contains "0".
 *
 * NOTES
 *  - Accepts: {whitespace} [+|-] {digits}
 *  - No check is made for value overflow, only the lower 64 bits are assigned.
 *  - If str is NULL it crashes, as the native function does.
 */
LONGLONG __cdecl _atoi64( const char *str )
{
    ULONGLONG RunningTotal = 0;
    BOOL bMinus = FALSE;

    while (*str == ' ' || (*str >= '\011' && *str <= '\015')) {
	str++;
    } /* while */

    if (*str == '+') {
	str++;
    } else if (*str == '-') {
        bMinus = TRUE;
	str++;
    } /* if */

    while (*str >= '0' && *str <= '9') {
	RunningTotal = RunningTotal * 10 + *str - '0';
	str++;
    } /* while */

    return bMinus ? -RunningTotal : RunningTotal;
}


/*********************************************************************
 *                  atoi   (NTDLL.@)
 */
int __cdecl atoi( const char *nptr )
{
    return _atoi64( nptr );
}


/*********************************************************************
 *                  atol   (NTDLL.@)
 */
__msvcrt_long __cdecl atol( const char *nptr )
{
    return _atoi64( nptr );
}


/* helper function for *scanf.  Returns the value of character c in the
 * given base, or -1 if the given character is not a digit of the base.
 */
static int char2digit( char c, int base )
{
    if ((c >= '0' && c <= '9') && (c <= '0'+base-1)) return (c-'0');
    if (base <= 10) return -1;
    if ((c >= 'A') && (c <= 'Z') && (c <= 'A'+base-11)) return (c-'A'+10);
    if ((c >= 'a') && (c <= 'z') && (c <= 'a'+base-11)) return (c-'a'+10);
    return -1;
}


static int vsscanf( const char *str, const char *format, __ms_va_list ap)
{
    int rd = 0, consumed = 0;
    int nch;
    if (!*format) return 0;

    nch = (consumed++, *str++);
    if (nch == '\0')
        return EOF;

    while (*format)
    {
        if (isspace( *format ))
        {
            /* skip whitespace */
            while ((nch != '\0') && isspace( nch ))
                nch = (consumed++, *str++);
        }
        else if (*format == '%')
        {
            int st = 0;
            BOOLEAN suppress = 0;
            int width = 0;
            int base;
            int h_prefix = 0;
            BOOLEAN l_prefix = FALSE;
            BOOLEAN w_prefix = FALSE;
            BOOLEAN I64_prefix = FALSE;
            BOOLEAN prefix_finished = FALSE;
            format++;
            /* a leading asterisk means 'suppress assignment of this field' */
            if (*format == '*')
            {
                format++;
                suppress = TRUE;
            }
            /* look for width specification */
            while (isdigit( *format ))
            {
                width *= 10;
                width += *format++ - '0';
            }
            if (width == 0) width = -1; /* no width spec seen */
            /* read prefix (if any) */
            while (!prefix_finished)
            {
                switch (*format)
                {
                case 'h': h_prefix++; break;
                case 'l':
                    if (*(format+1) == 'l')
                    {
                        I64_prefix = TRUE;
                        format++;
                    }
                    l_prefix = TRUE;
                    break;
                case 'w': w_prefix = TRUE; break;
                case 'I':
                    if (*(format + 1) == '6' &&
                        *(format + 2) == '4')
                    {
                        I64_prefix = TRUE;
                        format += 2;
                    }
                    break;
                default:
                    prefix_finished = TRUE;
                }
                if (!prefix_finished) format++;
            }
            /* read type */
            switch (*format)
            {
            case 'p':
            case 'P': /* pointer. */
                if (sizeof(void *) == sizeof(LONGLONG)) I64_prefix = TRUE;
                /* fall through */
            case 'x':
            case 'X': /* hexadecimal integer. */
                base = 16;
                goto number;
            case 'o': /* octal integer */
                base = 8;
                goto number;
            case 'u': /* unsigned decimal integer */
                base = 10;
                goto number;
            case 'd': /* signed decimal integer */
                base = 10;
                goto number;
            case 'i': /* generic integer */
                base = 0;
            number:
                {
                    /* read an integer */
                    ULONGLONG cur = 0;
                    BOOLEAN negative = FALSE;
                    BOOLEAN seendigit = FALSE;
                    /* skip initial whitespace */
                    while ((nch != '\0') && isspace( nch ))
                        nch = (consumed++, *str++);
                    /* get sign */
                    if (nch == '-' || nch == '+')
                    {
                        negative = (nch == '-');
                        nch = (consumed++, *str++);
                        if (width > 0) width--;
                    }
                    /* look for leading indication of base */
                    if (width != 0 && nch == '0' && *format != 'p' && *format != 'P')
                    {
                        nch = (consumed++, *str++);
                        if (width > 0) width--;
                        seendigit = TRUE;
                        if (width != 0 && (nch == 'x' || nch == 'X'))
                        {
                            if (base == 0)
                                base = 16;
                            if (base == 16)
                            {
                                nch = (consumed++, *str++);
                                if (width > 0) width--;
                                seendigit = FALSE;
                            }
                        } else if (base == 0)
                            base = 8;
                    }
                    /* format %i without indication of base */
                    if (base == 0)
                        base = 10;
                    /* throw away leading zeros */
                    while (width != 0 && nch == '0')
                    {
                        nch = (consumed++, *str++);
                        if (width > 0) width--;
                        seendigit = TRUE;
                    }
                    if (width != 0 && char2digit( nch, base ) != -1)
                    {
                        cur = char2digit( nch, base );
                        nch = (consumed++, *str++);
                        if (width > 0) width--;
                        seendigit = TRUE;
                    }
                    /* read until no more digits */
                    while (width != 0 && nch != '\0' && char2digit( nch, base ) != -1)
                    {
                        cur = cur*base + char2digit( nch, base );
                        nch = (consumed++, *str++);
                        if (width > 0) width--;
                        seendigit = TRUE;
                    }
                    /* okay, done! */
                    if (!seendigit) break; /* not a valid number */
                    st = 1;
                    if (!suppress)
                    {
#define _SET_NUMBER_( type ) *va_arg( ap, type* ) = negative ? -cur : cur
                        if (I64_prefix) _SET_NUMBER_( LONGLONG );
                        else if (l_prefix) _SET_NUMBER_( LONG );
                        else if (h_prefix == 1) _SET_NUMBER_( short int );
                        else _SET_NUMBER_( int );
                    }
                }
                break;
                /* According to msdn,
                 * 's' reads a character string in a call to fscanf
                 * and 'S' a wide character string and vice versa in a
                 * call to fwscanf. The 'h', 'w' and 'l' prefixes override
                 * this behaviour. 'h' forces reading char * but 'l' and 'w'
                 * force reading WCHAR. */
            case 's':
                if (w_prefix || l_prefix) goto widecharstring;
                else if (h_prefix) goto charstring;
                else goto charstring;
            case 'S':
                if (w_prefix || l_prefix) goto widecharstring;
                else if (h_prefix) goto charstring;
                else goto widecharstring;
            charstring:
                { /* read a word into a char */
                    char *sptr = suppress ? NULL : va_arg( ap, char * );
                    char *sptr_beg = sptr;
                    unsigned size = UINT_MAX;
                    /* skip initial whitespace */
                    while (nch != '\0' && isspace( nch ))
                        nch = (consumed++, *str++);
                    /* read until whitespace */
                    while (width != 0 && nch != '\0' && !isspace( nch ))
                    {
                        if (!suppress)
                        {
                            *sptr++ = nch;
                            if(size > 1) size--;
                            else
                            {
                                *sptr_beg = 0;
                                return rd;
                            }
                        }
                        st++;
                        nch = (consumed++, *str++);
                        if (width > 0) width--;
                    }
                    /* terminate */
                    if (st && !suppress) *sptr = 0;
                }
                break;
            widecharstring:
                { /* read a word into a WCHAR * */
                    WCHAR *sptr = suppress ? NULL : va_arg( ap, WCHAR * );
                    WCHAR *sptr_beg = sptr;
                    unsigned size = UINT_MAX;
                    /* skip initial whitespace */
                    while (nch != '\0' && isspace( nch ))
                        nch = (consumed++, *str++);
                    /* read until whitespace */
                    while (width != 0 && nch != '\0' && !isspace( nch ))
                    {
                        if (!suppress)
                        {
                            *sptr++ = nch;
                            if (size > 1) size--;
                            else
                            {
                                *sptr_beg = 0;
                                return rd;
                            }
                        }
                        st++;
                        nch = (consumed++, *str++);
                        if (width > 0) width--;
                    }
                    /* terminate */
                    if (st && !suppress) *sptr = 0;
                }
                break;
            /* 'c' and 'C work analogously to 's' and 'S' as described
             * above */
            case 'c':
                if (w_prefix || l_prefix) goto widecharacter;
                else if (h_prefix) goto character;
                else goto character;
            case 'C':
                if (w_prefix || l_prefix) goto widecharacter;
                else if (h_prefix) goto character;
                else goto widecharacter;
            character:
                { /* read single character into char */
                    char *sptr = suppress ? NULL : va_arg( ap, char * );
                    char *sptr_beg = sptr;
                    unsigned size = UINT_MAX;
                    if (width == -1) width = 1;
                    while (width && nch != '\0')
                    {
                        if (!suppress)
                        {
                            *sptr++ = nch;
                            if(size) size--;
                            else
                            {
                                *sptr_beg = 0;
                                return rd;
                            }
                        }
                        st++;
                        width--;
                        nch = (consumed++, *str++);
                    }
                }
                break;
            widecharacter:
                { /* read single character into a WCHAR */
                    WCHAR *sptr = suppress ? NULL : va_arg( ap, WCHAR * );
                    WCHAR *sptr_beg = sptr;
                    unsigned size = UINT_MAX;
                    if (width == -1) width = 1;
                    while (width && nch != '\0')
                    {
                        if (!suppress)
                        {
                            *sptr++ = nch;
                            if (size) size--;
                            else
                            {
                                *sptr_beg = 0;
                                return rd;
                            }
                        }
                        st++;
                        width--;
                        nch = (consumed++, *str++);
                    }
                }
                break;
            case 'n':
                {
                    if (!suppress)
                    {
                        int *n = va_arg( ap, int * );
                        *n = consumed - 1;
                    }
                    /* This is an odd one: according to the standard,
                     * "Execution of a %n directive does not increment the
                     * assignment count returned at the completion of
                     * execution" even if it wasn't suppressed with the
                     * '*' flag.  The Corrigendum to the standard seems
                     * to contradict this (comment out the assignment to
                     * suppress below if you want to implement these
                     * alternate semantics) but the windows program I'm
                     * looking at expects the behavior I've coded here
                     * (which happens to be what glibc does as well).
                     */
                    suppress = TRUE;
                    st = 1;
                }
                break;
            case '[':
                {
                    char *sptr = suppress ? NULL : va_arg( ap, char * );
                    char *sptr_beg = sptr;
                    RTL_BITMAP bitMask;
                    ULONG Mask[8] = { 0 };
                    BOOLEAN invert = FALSE; /* Set if we are NOT to find the chars */
                    unsigned size = UINT_MAX;

                    RtlInitializeBitMap( &bitMask, Mask, sizeof(Mask) * 8 );

                    /* Read the format */
                    format++;
                    if (*format == '^')
                    {
                        invert = TRUE;
                        format++;
                    }
                    if (*format == ']')
                    {
                        RtlSetBits( &bitMask, ']', 1 );
                        format++;
                    }
                    while (*format && (*format != ']'))
                    {
                        /* According to msdn:
                         * "Note that %[a-z] and %[z-a] are interpreted as equivalent to %[abcde...z]." */
                        if ((*format == '-') && (*(format + 1) != ']'))
                        {
                            if ((*(format - 1)) < *(format + 1))
                                RtlSetBits( &bitMask, *(format - 1) +1 , *(format + 1) - *(format - 1) );
                            else
                                RtlSetBits( &bitMask, *(format + 1)    , *(format - 1) - *(format + 1) );
                            format++;
                        }
                        else
                            RtlSetBits( &bitMask, *format, 1 );
                        format++;
                    }
                    /* read until char is not suitable */
                    while (width != 0 && nch != '\0')
                    {
                        if (!invert)
                        {
                            if(RtlAreBitsSet( &bitMask, nch, 1 ))
                            {
                                if (!suppress) *sptr++ = nch;
                            }
                            else
                                break;
                        }
                        else
                        {
                            if (RtlAreBitsClear( &bitMask, nch, 1 ))
                            {
                                if (!suppress) *sptr++ = nch;
                            }
                            else
                                break;
                        }
                        st++;
                        nch = (consumed++, *str++);
                        if (width > 0) width--;
                        if(size > 1) size--;
                        else
                        {
                            if (!suppress) *sptr_beg = 0;
                            return rd;
                        }
                    }
                    /* terminate */
                    if (!suppress) *sptr = 0;
                }
                break;
            default:
                /* From spec: "if a percent sign is followed by a character
                 * that has no meaning as a format-control character, that
                 * character and the following characters are treated as
                 * an ordinary sequence of characters, that is, a sequence
                 * of characters that must match the input.  For example,
                 * to specify that a percent-sign character is to be input,
                 * use %%." */
                while (nch != '\0' && isspace( nch ))
                    nch = (consumed++, *str++);
                if (nch == *format)
                {
                    suppress = TRUE; /* whoops no field to be read */
                    st = 1; /* but we got what we expected */
                    nch = (consumed++, *str++);
                }
                break;
            }
            if (st && !suppress) rd++;
            else if (!st) break;
        }
        /* A non-white-space character causes scanf to read, but not store,
         * a matching non-white-space character. */
        else
        {
            if (nch == *format)
                nch = (consumed++, *str++);
            else break;
        }
        format++;
    }
    if (nch != '\0')
    {
        consumed--, str--;
    }

    return rd;
}


/*********************************************************************
 *                  sscanf   (NTDLL.@)
 */
int WINAPIV sscanf( const char *str, const char *format, ... )
{
    int ret;
    __ms_va_list valist;
    __ms_va_start( valist, format );
    ret = vsscanf( str, format, valist );
    __ms_va_end( valist );
    return ret;
}


/*********************************************************************
 *		_splitpath (NTDLL.@)
 *
 * Split a path into its component pieces.
 *
 * PARAMS
 *  inpath [I] Path to split
 *  drv    [O] Destination for drive component (e.g. "A:"). Must be at least 3 characters.
 *  dir    [O] Destination for directory component. Should be at least MAX_PATH characters.
 *  fname  [O] Destination for File name component. Should be at least MAX_PATH characters.
 *  ext    [O] Destination for file extension component. Should be at least MAX_PATH characters.
 *
 * RETURNS
 *  Nothing.
 */
void __cdecl _splitpath(const char* inpath, char * drv, char * dir,
                        char* fname, char * ext )
{
    const char *p, *end;

    if (inpath[0] && inpath[1] == ':')
    {
        if (drv)
        {
            drv[0] = inpath[0];
            drv[1] = inpath[1];
            drv[2] = 0;
        }
        inpath += 2;
    }
    else if (drv) drv[0] = 0;

    /* look for end of directory part */
    end = NULL;
    for (p = inpath; *p; p++) if (*p == '/' || *p == '\\') end = p + 1;

    if (end)  /* got a directory */
    {
        if (dir)
        {
            memcpy( dir, inpath, end - inpath );
            dir[end - inpath] = 0;
        }
        inpath = end;
    }
    else if (dir) dir[0] = 0;

    /* look for extension: what's after the last dot */
    end = NULL;
    for (p = inpath; *p; p++) if (*p == '.') end = p;

    if (!end) end = p; /* there's no extension */

    if (fname)
    {
        memcpy( fname, inpath, end - inpath );
        fname[end - inpath] = 0;
    }
    if (ext) strcpy( ext, end );
}
