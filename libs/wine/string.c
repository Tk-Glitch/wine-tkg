/*
 * Unicode string manipulation functions
 *
 * Copyright 2000 Alexandre Julliard
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

#include "wine/asm.h"

#ifdef __ASM_OBSOLETE

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdarg.h>

#include <windef.h>
#include <winbase.h>
#include <winnls.h>

WCHAR tolowerW( WCHAR ch )
{
    extern const WCHAR wine_casemap_lower[];
    return ch + wine_casemap_lower[wine_casemap_lower[ch >> 8] + (ch & 0xff)];
}

WCHAR toupperW( WCHAR ch )
{
    extern const WCHAR wine_casemap_upper[];
    return ch + wine_casemap_upper[wine_casemap_upper[ch >> 8] + (ch & 0xff)];
}

/* the character type contains the C1_* flags in the low 12 bits */
/* and the C2_* type in the high 4 bits */
unsigned short get_char_typeW( WCHAR ch )
{
    extern const unsigned short wine_wctype_table[];
    return wine_wctype_table[wine_wctype_table[ch >> 8] + (ch & 0xff)];
}

int iscntrlW( WCHAR wc )
{
    return get_char_typeW(wc) & C1_CNTRL;
}

int ispunctW( WCHAR wc )
{
    return get_char_typeW(wc) & C1_PUNCT;
}

int isspaceW( WCHAR wc )
{
    return get_char_typeW(wc) & C1_SPACE;
}

int isdigitW( WCHAR wc )
{
    return get_char_typeW(wc) & C1_DIGIT;
}

int isxdigitW( WCHAR wc )
{
    return get_char_typeW(wc) & C1_XDIGIT;
}

int islowerW( WCHAR wc )
{
    return get_char_typeW(wc) & C1_LOWER;
}

int isupperW( WCHAR wc )
{
    return get_char_typeW(wc) & C1_UPPER;
}

int isalnumW( WCHAR wc )
{
    return get_char_typeW(wc) & (C1_ALPHA|C1_DIGIT|C1_LOWER|C1_UPPER);
}

int isalphaW( WCHAR wc )
{
    return get_char_typeW(wc) & (C1_ALPHA|C1_LOWER|C1_UPPER);
}

int isgraphW( WCHAR wc )
{
    return get_char_typeW(wc) & (C1_ALPHA|C1_PUNCT|C1_DIGIT|C1_LOWER|C1_UPPER);
}

int isprintW( WCHAR wc )
{
    return get_char_typeW(wc) & (C1_ALPHA|C1_BLANK|C1_PUNCT|C1_DIGIT|C1_LOWER|C1_UPPER);
}

unsigned int strlenW( const WCHAR *str )
{
    const WCHAR *s = str;
    while (*s) s++;
    return s - str;
}

WCHAR *strcpyW( WCHAR *dst, const WCHAR *src )
{
    WCHAR *p = dst;
    while ((*p++ = *src++));
    return dst;
}

int strcmpW( const WCHAR *str1, const WCHAR *str2 )
{
    while (*str1 && (*str1 == *str2)) { str1++; str2++; }
    return *str1 - *str2;
}

int strncmpW( const WCHAR *str1, const WCHAR *str2, int n )
{
    if (n <= 0) return 0;
    while ((--n > 0) && *str1 && (*str1 == *str2)) { str1++; str2++; }
    return *str1 - *str2;
}

WCHAR *strcatW( WCHAR *dst, const WCHAR *src )
{
    strcpyW( dst + strlenW(dst), src );
    return dst;
}

WCHAR *strchrW( const WCHAR *str, WCHAR ch )
{
    do { if (*str == ch) return (WCHAR *)(ULONG_PTR)str; } while (*str++);
    return NULL;
}

WCHAR *strrchrW( const WCHAR *str, WCHAR ch )
{
    WCHAR *ret = NULL;
    do { if (*str == ch) ret = (WCHAR *)(ULONG_PTR)str; } while (*str++);
    return ret;
}

WCHAR *strpbrkW( const WCHAR *str, const WCHAR *accept )
{
    for ( ; *str; str++) if (strchrW( accept, *str )) return (WCHAR *)(ULONG_PTR)str;
    return NULL;
}

size_t strspnW( const WCHAR *str, const WCHAR *accept )
{
    const WCHAR *ptr;
    for (ptr = str; *ptr; ptr++) if (!strchrW( accept, *ptr )) break;
    return ptr - str;
}

size_t strcspnW( const WCHAR *str, const WCHAR *reject )
{
    const WCHAR *ptr;
    for (ptr = str; *ptr; ptr++) if (strchrW( reject, *ptr )) break;
    return ptr - str;
}

WCHAR *strlwrW( WCHAR *str )
{
    WCHAR *ret;
    for (ret = str; *str; str++) *str = tolowerW(*str);
    return ret;
}

WCHAR *struprW( WCHAR *str )
{
    WCHAR *ret;
    for (ret = str; *str; str++) *str = toupperW(*str);
    return ret;
}

WCHAR *memchrW( const WCHAR *ptr, WCHAR ch, size_t n )
{
    const WCHAR *end;
    for (end = ptr + n; ptr < end; ptr++) if (*ptr == ch) return (WCHAR *)(ULONG_PTR)ptr;
    return NULL;
}

WCHAR *memrchrW( const WCHAR *ptr, WCHAR ch, size_t n )
{
    const WCHAR *end;
    WCHAR *ret = NULL;
    for (end = ptr + n; ptr < end; ptr++) if (*ptr == ch) ret = (WCHAR *)(ULONG_PTR)ptr;
    return ret;
}

int strcmpiW( const WCHAR *str1, const WCHAR *str2 )
{
    for (;;)
    {
        int ret = tolowerW(*str1) - tolowerW(*str2);
        if (ret || !*str1) return ret;
        str1++;
        str2++;
    }
}

int strncmpiW( const WCHAR *str1, const WCHAR *str2, int n )
{
    int ret = 0;
    for ( ; n > 0; n--, str1++, str2++)
        if ((ret = tolowerW(*str1) - tolowerW(*str2)) || !*str1) break;
    return ret;
}

int memicmpW( const WCHAR *str1, const WCHAR *str2, int n )
{
    int ret = 0;
    for ( ; n > 0; n--, str1++, str2++)
        if ((ret = tolowerW(*str1) - tolowerW(*str2))) break;
    return ret;
}

WCHAR *strstrW( const WCHAR *str, const WCHAR *sub )
{
    while (*str)
    {
        const WCHAR *p1 = str, *p2 = sub;
        while (*p1 && *p2 && *p1 == *p2) { p1++; p2++; }
        if (!*p2) return (WCHAR *)str;
        str++;
    }
    return NULL;
}

/* strtolW and strtoulW implementation based on the GNU C library code */
/* Copyright (C) 1991,92,94,95,96,97,98,99,2000,2001 Free Software Foundation, Inc. */

long int strtolW( const WCHAR *nptr, WCHAR **endptr, int base )
{
  int negative;
  register unsigned long int cutoff;
  register unsigned int cutlim;
  register unsigned long int i;
  register const WCHAR *s;
  register WCHAR c;
  const WCHAR *save, *end;
  int overflow;

  if (base < 0 || base == 1 || base > 36) return 0;

  save = s = nptr;

  /* Skip white space.  */
  while (isspaceW (*s))
    ++s;
  if (!*s) goto noconv;

  /* Check for a sign.  */
  negative = 0;
  if (*s == '-')
    {
      negative = 1;
      ++s;
    }
  else if (*s == '+')
    ++s;

  /* Recognize number prefix and if BASE is zero, figure it out ourselves.  */
  if (*s == '0')
    {
      if ((base == 0 || base == 16) && toupperW(s[1]) == 'X')
	{
	  s += 2;
	  base = 16;
	}
      else if (base == 0)
	base = 8;
    }
  else if (base == 0)
    base = 10;

  /* Save the pointer so we can check later if anything happened.  */
  save = s;
  end = NULL;

  cutoff = ULONG_MAX / (unsigned long int) base;
  cutlim = ULONG_MAX % (unsigned long int) base;

  overflow = 0;
  i = 0;
  c = *s;
  for (;c != '\0'; c = *++s)
  {
      if (s == end)
          break;
      if (c >= '0' && c <= '9')
          c -= '0';
      else if (isalphaW (c))
          c = toupperW (c) - 'A' + 10;
      else
          break;
      if ((int) c >= base)
          break;
      /* Check for overflow.  */
      if (i > cutoff || (i == cutoff && c > cutlim))
          overflow = 1;
      else
      {
          i *= (unsigned long int) base;
          i += c;
      }
  }

  /* Check if anything actually happened.  */
  if (s == save)
    goto noconv;

  /* Store in ENDPTR the address of one character
     past the last character we converted.  */
  if (endptr != NULL)
    *endptr = (WCHAR *)s;

  /* Check for a value that is within the range of
     `unsigned LONG int', but outside the range of `LONG int'.  */
  if (overflow == 0
      && i > (negative
	      ? -((unsigned long int) (LONG_MIN + 1)) + 1
	      : (unsigned long int) LONG_MAX))
    overflow = 1;

  if (overflow)
    {
      errno = ERANGE;
      return negative ? LONG_MIN : LONG_MAX;
    }

  /* Return the result of the appropriate sign.  */
  return negative ? -i : i;

noconv:
  /* We must handle a special case here: the base is 0 or 16 and the
     first two characters are '0' and 'x', but the rest are not
     hexadecimal digits.  This is no error case.  We return 0 and
     ENDPTR points to the `x`.  */
  if (endptr != NULL)
    {
      if (save - nptr >= 2 && toupperW (save[-1]) == 'X'
	  && save[-2] == '0')
	*endptr = (WCHAR *)&save[-1];
      else
	/*  There was no number to convert.  */
	*endptr = (WCHAR *)nptr;
    }

  return 0L;
}


unsigned long int strtoulW( const WCHAR *nptr, WCHAR **endptr, int base )
{
  int negative;
  register unsigned long int cutoff;
  register unsigned int cutlim;
  register unsigned long int i;
  register const WCHAR *s;
  register WCHAR c;
  const WCHAR *save, *end;
  int overflow;

  if (base < 0 || base == 1 || base > 36) return 0;

  save = s = nptr;

  /* Skip white space.  */
  while (isspaceW (*s))
    ++s;
  if (!*s) goto noconv;

  /* Check for a sign.  */
  negative = 0;
  if (*s == '-')
    {
      negative = 1;
      ++s;
    }
  else if (*s == '+')
    ++s;

  /* Recognize number prefix and if BASE is zero, figure it out ourselves.  */
  if (*s == '0')
    {
      if ((base == 0 || base == 16) && toupperW(s[1]) == 'X')
	{
	  s += 2;
	  base = 16;
	}
      else if (base == 0)
	base = 8;
    }
  else if (base == 0)
    base = 10;

  /* Save the pointer so we can check later if anything happened.  */
  save = s;
  end = NULL;

  cutoff = ULONG_MAX / (unsigned long int) base;
  cutlim = ULONG_MAX % (unsigned long int) base;

  overflow = 0;
  i = 0;
  c = *s;
  for (;c != '\0'; c = *++s)
  {
      if (s == end)
          break;
      if (c >= '0' && c <= '9')
          c -= '0';
      else if (isalphaW (c))
          c = toupperW (c) - 'A' + 10;
      else
          break;
      if ((int) c >= base)
          break;
      /* Check for overflow.  */
      if (i > cutoff || (i == cutoff && c > cutlim))
          overflow = 1;
      else
      {
          i *= (unsigned long int) base;
          i += c;
      }
  }

  /* Check if anything actually happened.  */
  if (s == save)
    goto noconv;

  /* Store in ENDPTR the address of one character
     past the last character we converted.  */
  if (endptr != NULL)
    *endptr = (WCHAR *)s;

  if (overflow)
    {
      errno = ERANGE;
      return ULONG_MAX;
    }

  /* Return the result of the appropriate sign.  */
  return negative ? -i : i;

noconv:
  /* We must handle a special case here: the base is 0 or 16 and the
     first two characters are '0' and 'x', but the rest are not
     hexadecimal digits.  This is no error case.  We return 0 and
     ENDPTR points to the `x`.  */
  if (endptr != NULL)
    {
      if (save - nptr >= 2 && toupperW (save[-1]) == 'X'
	  && save[-2] == '0')
	*endptr = (WCHAR *)&save[-1];
      else
	/*  There was no number to convert.  */
	*endptr = (WCHAR *)nptr;
    }

  return 0L;
}

long int atolW( const WCHAR *str )
{
    return strtolW( str, (WCHAR **)0, 10 );
}

int atoiW( const WCHAR *str )
{
    return (int)atolW( str );
}


/* format a WCHAR string according to a printf format; helper for vsnprintfW */
static size_t format_string( WCHAR *buffer, size_t len, const char *format, const WCHAR *str, int str_len )
{
    size_t count = 0;
    int i, left_align = 0, width = 0, max = 0;

    assert( *format == '%' );
    format++; /* skip '%' */

    while (*format == '0' || *format == '+' || *format == '-' || *format == ' ' || *format == '#')
    {
        if (*format == '-') left_align = 1;
        format++;
    }

    while (isdigit(*format)) width = width * 10 + *format++ - '0';

    if (str_len == -1) str_len = strlenW( str );
    if (*format == '.')
    {
        format++;
        while (isdigit(*format)) max = max * 10 + *format++ - '0';
        if (max > str_len) max = str_len;
    }
    else max = str_len;

    if (*format == 'h' || *format == 'l') format++;

    assert( *format == 's' );

    if (!left_align && width > max)
    {
        for (i = 0; i < width - max; i++)
        {
            if (count++ < len)
                *buffer++ = ' ';
        }
    }

    if (count < len)
        memcpy( buffer, str, min( max, len - count ) * sizeof(WCHAR) );
    count += max;
    buffer += max;

    if (left_align && width > max)
    {
        for (i = 0; i < width - max; i++)
        {
            if (count++ < len)
                *buffer++ = ' ';
        }
    }
    return count;
}

int vsnprintfW(WCHAR *str, size_t len, const WCHAR *format, va_list valist)
{
    unsigned int written = 0;
    const WCHAR *iter = format;
    char bufa[512], fmtbufa[64], *fmta;

    while (*iter)
    {
        while (*iter && *iter != '%')
        {
            if (written++ < len)
                *str++ = *iter;
            iter++;
        }
        if (*iter == '%')
        {
            if (iter[1] == '%')
            {
                if (written++ < len)
                    *str++ = '%'; /* "%%"->'%' */
                iter += 2;
                continue;
            }

            fmta = fmtbufa;
            *fmta++ = *iter++;
            while (*iter == '0' ||
                   *iter == '+' ||
                   *iter == '-' ||
                   *iter == ' ' ||
                   *iter == '*' ||
                   *iter == '#')
            {
                if (*iter == '*')
                {
                    char *buffiter = bufa;
                    int fieldlen = va_arg(valist, int);
                    sprintf(buffiter, "%d", fieldlen);
                    while (*buffiter)
                        *fmta++ = *buffiter++;
                }
                else
                    *fmta++ = *iter;
                iter++;
            }

            while (isdigit(*iter))
                *fmta++ = *iter++;

            if (*iter == '.')
            {
                *fmta++ = *iter++;
                if (*iter == '*')
                {
                    char *buffiter = bufa;
                    int fieldlen = va_arg(valist, int);
                    sprintf(buffiter, "%d", fieldlen);
                    while (*buffiter)
                        *fmta++ = *buffiter++;
                    iter++;
                }
                else
                    while (isdigit(*iter))
                        *fmta++ = *iter++;
            }
            if (*iter == 'h' || *iter == 'l')
                *fmta++ = *iter++;

            switch (*iter)
            {
            case 's':
            {
                static const WCHAR none[] = { '(','n','u','l','l',')',0 };
                const WCHAR *wstr = va_arg(valist, const WCHAR *);
                size_t remaining = written < len ? len - written : 0;
                size_t count;

                *fmta++ = 's';
                *fmta = 0;
                count = format_string( str, remaining, fmtbufa, wstr ? wstr : none, -1 );
                str += min( count, remaining );
                written += count;
                iter++;
                break;
            }

            case 'c':
            {
                WCHAR wstr;
                size_t remaining = written < len ? len - written : 0;
                size_t count;

                wstr = va_arg(valist, int);
                *fmta++ = 's';
                *fmta = 0;
                count = format_string( str, remaining, fmtbufa, &wstr, 1 );
                str += min( count, remaining );
                written += count;
                iter++;
                break;
            }

            default:
            {
                /* For non wc types, use system sprintf and append to wide char output */
                /* FIXME: for unrecognised types, should ignore % when printing */
                char *bufaiter = bufa;
                if (*iter == 'p')
                    sprintf(bufaiter, "%0*lX", 2 * (int)sizeof(void*),
                            (unsigned long)va_arg(valist, void *));
                else
                {
                    *fmta++ = *iter;
                    *fmta = '\0';
                    if (*iter == 'a' || *iter == 'A' ||
                        *iter == 'e' || *iter == 'E' ||
                        *iter == 'f' || *iter == 'F' ||
                        *iter == 'g' || *iter == 'G')
                        sprintf(bufaiter, fmtbufa, va_arg(valist, double));
                    else
                    {
                        /* FIXME: On 32 bit systems this doesn't handle int 64's. */
                        sprintf(bufaiter, fmtbufa, va_arg(valist, void *));
                    }
                }
                while (*bufaiter)
                {
                    if (written++ < len)
                        *str++ = *bufaiter;
                    bufaiter++;
                }
                iter++;
                break;
            }
            }
        }
    }
    if (len)
    {
        if (written >= len)
            str--;
        *str++ = 0;
    }

    /* FIXME: POSIX [v]snprintf() returns the equivalent of written, not -1, on short buffer. */
    return written < len ? (int)written : -1;
}

int vsprintfW( WCHAR *str, const WCHAR *format, va_list valist )
{
    return vsnprintfW( str, INT_MAX, format, valist );
}

int snprintfW( WCHAR *str, size_t len, const WCHAR *format, ...)
{
    int retval;
    va_list valist;
    va_start(valist, format);
    retval = vsnprintfW(str, len, format, valist);
    va_end(valist);
    return retval;
}

int sprintfW( WCHAR *str, const WCHAR *format, ...)
{
    int retval;
    va_list valist;
    va_start(valist, format);
    retval = vsnprintfW(str, INT_MAX, format, valist);
    va_end(valist);
    return retval;
}

#endif /* __ASM_OBSOLETE */
