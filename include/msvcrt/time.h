/*
 * Time definitions
 *
 * Copyright 2000 Francois Gouget.
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
#ifndef __WINE_TIME_H
#define __WINE_TIME_H

#include <corecrt_wtime.h>

#include <pshpack8.h>

#ifndef _CLOCK_T_DEFINED
typedef __msvcrt_long clock_t;
#define _CLOCK_T_DEFINED
#endif

#ifndef CLOCKS_PER_SEC
#define CLOCKS_PER_SEC 1000
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __i386__
#define _daylight (*__p__daylight())
#define _dstbias (*__p__dstbias())
#define _timezone (*__p__timezone())
#define _tzname (__p__tzname())

int *   __cdecl __p__daylight(void);
__msvcrt_long *  __cdecl __p__dstbias(void);
__msvcrt_long *  __cdecl __p__timezone(void);
char ** __cdecl __p__tzname(void);
#else
extern int _daylight;
extern __msvcrt_long _dstbias;
extern __msvcrt_long _timezone;
extern char *_tzname;
#endif

#if !defined(_UCRT) && defined(_USE_32BIT_TIME_T)
#define _ctime32     ctime
#define _difftime32  difftime
#define _gmtime32    gmtime
#define _localtime32 localtime
#define _mktime32    mktime
#define _time32      time
#endif

unsigned    __cdecl _getsystime(struct tm*);
unsigned    __cdecl _setsystime(struct tm*,unsigned);
char*       __cdecl _strdate(char*);
errno_t     __cdecl _strdate_s(char*,size_t);
char*       __cdecl _strtime(char*);
errno_t     __cdecl _strtime_s(char*,size_t);
void        __cdecl _tzset(void);

char*       __cdecl asctime(const struct tm*);
clock_t     __cdecl clock(void);
char*       __cdecl _ctime32(const __time32_t*);
char*       __cdecl _ctime64(const __time64_t*);
double      __cdecl _difftime32(__time32_t,__time32_t);
double      __cdecl _difftime64(__time64_t,__time64_t);
struct tm*  __cdecl _gmtime32(const __time32_t*);
struct tm*  __cdecl _gmtime64(const __time64_t*);
struct tm*  __cdecl _localtime32(const __time32_t*);
errno_t     __cdecl _localtime32_s(struct tm*, const __time64_t*);
struct tm*  __cdecl _localtime64(const __time64_t*);
errno_t     __cdecl _localtime64_s(struct tm*, const __time64_t*);
__time32_t  __cdecl _mktime32(struct tm*);
__time64_t  __cdecl _mktime64(struct tm*);
size_t      __cdecl strftime(char*,size_t,const char*,const struct tm*);
__time32_t  __cdecl _time32(__time32_t*);
__time64_t  __cdecl _time64(__time64_t*);

#ifndef _USE_32BIT_TIME_T
static inline char* ctime(const time_t *t) { return _ctime64(t); }
static inline double difftime(time_t t1, time_t t2) { return _difftime64(t1, t2); }
static inline struct tm* gmtime(const time_t *t) { return _gmtime64(t); }
static inline struct tm* localtime(const time_t *t) { return _localtime64(t); }
static inline time_t mktime(struct tm *tm) { return _mktime64(tm); }
static inline time_t time(time_t *t) { return _time64(t); }
#elif defined(_UCRT)
static inline char* ctime(const time_t *t) { return _ctime32(t); }
static inline double difftime(time_t t1, time_t t2) { return _difftime32(t1, t2); }
static inline struct tm* gmtime(const time_t *t) { return _gmtime32(t); }
static inline struct tm* localtime(const time_t *t) { return _localtime32(t); }
static inline time_t mktime(struct tm *tm) { return _mktime32(tm); }
static inline time_t time(time_t *t) { return _time32(t); }
#endif

#ifdef __cplusplus
}
#endif

#include <poppack.h>

#endif /* __WINE_TIME_H */
