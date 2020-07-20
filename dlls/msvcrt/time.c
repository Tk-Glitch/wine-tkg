/*
 * msvcrt.dll date/time functions
 *
 * Copyright 1996,1998 Marcus Meissner
 * Copyright 1996 Jukka Iivonen
 * Copyright 1997,2000 Uwe Bonnes
 * Copyright 2000 Jon Griffiths
 * Copyright 2004 Hans Leidekker
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

#include "msvcrt.h"
#include "mtdll.h"
#include "winbase.h"
#include "winnls.h"
#include "winternl.h"
#include "wine/debug.h"
#include "wine/unicode.h"

WINE_DEFAULT_DEBUG_CHANNEL(msvcrt);

BOOL WINAPI GetDaylightFlag(void);

static LONGLONG init_time;

void msvcrt_init_clock(void)
{
    LARGE_INTEGER systime;

    NtQuerySystemTime(&systime);
    init_time = systime.QuadPart;
}

static const int MonthLengths[2][12] =
{
    { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
    { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
};

#if _MSVCR_VER>=140
static const int MAX_SECONDS = 60;
#else
static const int MAX_SECONDS = 59;
#endif

static inline BOOL IsLeapYear(int Year)
{
    return Year % 4 == 0 && (Year % 100 != 0 || Year % 400 == 0);
}

static inline void write_invalid_msvcrt_tm( struct MSVCRT_tm *tm )
{
    tm->tm_sec = -1;
    tm->tm_min = -1;
    tm->tm_hour = -1;
    tm->tm_mday = -1;
    tm->tm_mon = -1;
    tm->tm_year = -1;
    tm->tm_wday = -1;
    tm->tm_yday = -1;
    tm->tm_isdst = -1;
}

/*********************************************************************
 *		_daylight (MSVCRT.@)
 */
int MSVCRT___daylight = 1;

/*********************************************************************
 *		_timezone (MSVCRT.@)
 */
MSVCRT_long MSVCRT___timezone = 28800;

/*********************************************************************
 *		_dstbias (MSVCRT.@)
 */
int MSVCRT__dstbias = -3600;

/*********************************************************************
 *		_tzname (MSVCRT.@)
 * NOTES
 *  Some apps (notably Mozilla) insist on writing to these, so the buffer
 *  must be large enough.
 */
static char tzname_std[64] = "PST";
static char tzname_dst[64] = "PDT";
char *MSVCRT__tzname[2] = { tzname_std, tzname_dst };

static TIME_ZONE_INFORMATION tzi = {0};
/*********************************************************************
 *		_tzset (MSVCRT.@)
 */
void CDECL MSVCRT__tzset(void)
{
    char *tz = MSVCRT_getenv("TZ");
    BOOL error;

    _mlock(_TIME_LOCK);
    if(tz && tz[0]) {
        BOOL neg_zone = FALSE;

        memset(&tzi, 0, sizeof(tzi));

        /* Parse timezone information: tzn[+|-]hh[:mm[:ss]][dzn] */
        lstrcpynA(MSVCRT__tzname[0], tz, 3);
        tz += 3;

        if(*tz == '-') {
            neg_zone = TRUE;
            tz++;
        }else if(*tz == '+') {
            tz++;
        }
        MSVCRT___timezone = strtol(tz, &tz, 10)*3600;
        if(*tz == ':') {
            MSVCRT___timezone += strtol(tz+1, &tz, 10)*60;
            if(*tz == ':')
                MSVCRT___timezone += strtol(tz+1, &tz, 10);
        }
        if(neg_zone)
            MSVCRT___timezone = -MSVCRT___timezone;

        MSVCRT___daylight = *tz;
        lstrcpynA(MSVCRT__tzname[1], tz, 3);
    }else if(GetTimeZoneInformation(&tzi) != TIME_ZONE_ID_INVALID) {
        MSVCRT___timezone = tzi.Bias*60;
        if(tzi.StandardDate.wMonth)
            MSVCRT___timezone += tzi.StandardBias*60;

        if(tzi.DaylightDate.wMonth) {
            MSVCRT___daylight = 1;
            MSVCRT__dstbias = (tzi.DaylightBias-tzi.StandardBias)*60;
        }else {
            MSVCRT___daylight = 0;
            MSVCRT__dstbias = 0;
        }

        if(!WideCharToMultiByte(CP_ACP, 0, tzi.StandardName, -1, MSVCRT__tzname[0],
                    sizeof(tzname_std), NULL, &error) || error)
            *MSVCRT__tzname[0] = 0;
        if(!WideCharToMultiByte(CP_ACP, 0, tzi.DaylightName, -1, MSVCRT__tzname[1],
                    sizeof(tzname_dst), NULL, &error) || error)
            *MSVCRT__tzname[0] = 0;
    }
    _munlock(_TIME_LOCK);
}

static void _tzset_init(void)
{
    static BOOL init = FALSE;

    if(!init) {
        _mlock(_TIME_LOCK);
        if(!init) {
            MSVCRT__tzset();
            init = TRUE;
        }
        _munlock(_TIME_LOCK);
    }
}

static BOOL is_dst(const SYSTEMTIME *st)
{
    TIME_ZONE_INFORMATION tmp;
    SYSTEMTIME out;

    if(!MSVCRT___daylight)
        return FALSE;

    if(tzi.DaylightDate.wMonth) {
        tmp = tzi;
    }else if(st->wYear >= 2007) {
        memset(&tmp, 0, sizeof(tmp));
        tmp.StandardDate.wMonth = 11;
        tmp.StandardDate.wDay = 1;
        tmp.StandardDate.wHour = 2;
        tmp.DaylightDate.wMonth = 3;
        tmp.DaylightDate.wDay = 2;
        tmp.DaylightDate.wHour = 2;
    }else {
        memset(&tmp, 0, sizeof(tmp));
        tmp.StandardDate.wMonth = 10;
        tmp.StandardDate.wDay = 5;
        tmp.StandardDate.wHour = 2;
        tmp.DaylightDate.wMonth = 4;
        tmp.DaylightDate.wDay = 1;
        tmp.DaylightDate.wHour = 2;
    }

    tmp.Bias = 0;
    tmp.StandardBias = 0;
    tmp.DaylightBias = MSVCRT__dstbias/60;
    if(!SystemTimeToTzSpecificLocalTime(&tmp, st, &out))
        return FALSE;

    return memcmp(st, &out, sizeof(SYSTEMTIME));
}

#define SECSPERDAY        86400
/* 1601 to 1970 is 369 years plus 89 leap days */
#define SECS_1601_TO_1970  ((369 * 365 + 89) * (ULONGLONG)SECSPERDAY)
#define TICKSPERSEC       10000000
#define TICKSPERMSEC      10000
#define TICKS_1601_TO_1970 (SECS_1601_TO_1970 * TICKSPERSEC)

static MSVCRT___time64_t mktime_helper(struct MSVCRT_tm *mstm, BOOL local)
{
    SYSTEMTIME st;
    FILETIME ft;
    MSVCRT___time64_t ret = 0;
    int i;
    BOOL use_dst = FALSE;

    ret = mstm->tm_year + mstm->tm_mon/12;
    mstm->tm_mon %= 12;
    if(mstm->tm_mon < 0) {
        mstm->tm_mon += 12;
        ret--;
    }

    if(ret<70 || ret>1100) {
        *MSVCRT__errno() = MSVCRT_EINVAL;
        return -1;
    }

    memset(&st, 0, sizeof(SYSTEMTIME));
    st.wDay = 1;
    st.wMonth = mstm->tm_mon+1;
    st.wYear = ret+1900;

    if(!SystemTimeToFileTime(&st, &ft)) {
        *MSVCRT__errno() = MSVCRT_EINVAL;
        return -1;
    }

    ret = ((MSVCRT___time64_t)ft.dwHighDateTime<<32)+ft.dwLowDateTime;
    ret += (MSVCRT___time64_t)mstm->tm_sec*TICKSPERSEC;
    ret += (MSVCRT___time64_t)mstm->tm_min*60*TICKSPERSEC;
    ret += (MSVCRT___time64_t)mstm->tm_hour*60*60*TICKSPERSEC;
    ret += (MSVCRT___time64_t)(mstm->tm_mday-1)*SECSPERDAY*TICKSPERSEC;

    ft.dwLowDateTime = ret & 0xffffffff;
    ft.dwHighDateTime = ret >> 32;
    FileTimeToSystemTime(&ft, &st);

    if(local) {
        _tzset_init();
        use_dst = is_dst(&st);
        if((mstm->tm_isdst<=-1 && use_dst) || (mstm->tm_isdst>=1)) {
            SYSTEMTIME tmp;

            ret += (MSVCRT___time64_t)MSVCRT__dstbias*TICKSPERSEC;

            ft.dwLowDateTime = ret & 0xffffffff;
            ft.dwHighDateTime = ret >> 32;
            FileTimeToSystemTime(&ft, &tmp);

            if(!is_dst(&tmp)) {
                st = tmp;
                use_dst = FALSE;
            }else {
                use_dst = TRUE;
            }
        }else if(mstm->tm_isdst==0 && use_dst) {
            ret -= (MSVCRT___time64_t)MSVCRT__dstbias*TICKSPERSEC;
            ft.dwLowDateTime = ret & 0xffffffff;
            ft.dwHighDateTime = ret >> 32;
            FileTimeToSystemTime(&ft, &st);
            ret += (MSVCRT___time64_t)MSVCRT__dstbias*TICKSPERSEC;
        }
        ret += (MSVCRT___time64_t)MSVCRT___timezone*TICKSPERSEC;
    }

    mstm->tm_sec = st.wSecond;
    mstm->tm_min = st.wMinute;
    mstm->tm_hour = st.wHour;
    mstm->tm_mday = st.wDay;
    mstm->tm_mon = st.wMonth-1;
    mstm->tm_year = st.wYear-1900;
    mstm->tm_wday = st.wDayOfWeek;
    for(i=mstm->tm_yday=0; i<st.wMonth-1; i++)
        mstm->tm_yday += MonthLengths[IsLeapYear(st.wYear)][i];
    mstm->tm_yday += st.wDay-1;
    mstm->tm_isdst = use_dst ? 1 : 0;

    if(ret < TICKS_1601_TO_1970) {
        *MSVCRT__errno() = MSVCRT_EINVAL;
        return -1;
    }
    ret = (ret-TICKS_1601_TO_1970)/TICKSPERSEC;
    return ret;
}

/**********************************************************************
 *		_mktime64 (MSVCRT.@)
 */
MSVCRT___time64_t CDECL MSVCRT__mktime64(struct MSVCRT_tm *mstm)
{
    return mktime_helper(mstm, TRUE);
}

/**********************************************************************
 *		_mktime32 (MSVCRT.@)
 */
MSVCRT___time32_t CDECL MSVCRT__mktime32(struct MSVCRT_tm *mstm)
{
    MSVCRT___time64_t ret = MSVCRT__mktime64( mstm );
    return ret == (MSVCRT___time32_t)ret ? ret : -1;
}

/**********************************************************************
 *		mktime (MSVCRT.@)
 */
#ifdef _WIN64
MSVCRT___time64_t CDECL MSVCRT_mktime(struct MSVCRT_tm *mstm)
{
    return MSVCRT__mktime64( mstm );
}
#else
MSVCRT___time32_t CDECL MSVCRT_mktime(struct MSVCRT_tm *mstm)
{
    return MSVCRT__mktime32( mstm );
}
#endif

/**********************************************************************
 *		_mkgmtime64 (MSVCRT.@)
 *
 * time->tm_isdst value is ignored
 */
MSVCRT___time64_t CDECL MSVCRT__mkgmtime64(struct MSVCRT_tm *time)
{
    return mktime_helper(time, FALSE);
}

/**********************************************************************
 *		_mkgmtime32 (MSVCRT.@)
 */
MSVCRT___time32_t CDECL MSVCRT__mkgmtime32(struct MSVCRT_tm *time)
{
    MSVCRT___time64_t ret = MSVCRT__mkgmtime64(time);
    return ret == (MSVCRT___time32_t)ret ? ret : -1;
}

/**********************************************************************
 *		_mkgmtime (MSVCRT.@)
 */
#ifdef _WIN64
MSVCRT___time64_t CDECL MSVCRT__mkgmtime(struct MSVCRT_tm *time)
{
    return MSVCRT__mkgmtime64(time);
}
#else
MSVCRT___time32_t CDECL MSVCRT__mkgmtime(struct MSVCRT_tm *time)
{
    return MSVCRT__mkgmtime32(time);
}
#endif

/*********************************************************************
 *      _localtime64_s (MSVCRT.@)
 */
int CDECL _localtime64_s(struct MSVCRT_tm *res, const MSVCRT___time64_t *secs)
{
    int i;
    FILETIME ft;
    SYSTEMTIME st;
    ULONGLONG time;

    if (!res || !secs || *secs < 0 || *secs > _MAX__TIME64_T)
    {
        if (res)
            write_invalid_msvcrt_tm(res);

        *MSVCRT__errno() = MSVCRT_EINVAL;
        return MSVCRT_EINVAL;
    }

    _tzset_init();
    time = (*secs - MSVCRT___timezone) * (ULONGLONG)TICKSPERSEC + TICKS_1601_TO_1970;

    ft.dwHighDateTime = (UINT)(time >> 32);
    ft.dwLowDateTime  = (UINT)time;
    FileTimeToSystemTime(&ft, &st);

    res->tm_isdst = is_dst(&st) ? 1 : 0;
    if(res->tm_isdst) {
        time -= MSVCRT__dstbias * (ULONGLONG)TICKSPERSEC;
        ft.dwHighDateTime = (UINT)(time >> 32);
        ft.dwLowDateTime  = (UINT)time;
        FileTimeToSystemTime(&ft, &st);
    }

    res->tm_sec  = st.wSecond;
    res->tm_min  = st.wMinute;
    res->tm_hour = st.wHour;
    res->tm_mday = st.wDay;
    res->tm_year = st.wYear - 1900;
    res->tm_mon  = st.wMonth - 1;
    res->tm_wday = st.wDayOfWeek;
    for (i = res->tm_yday = 0; i < st.wMonth - 1; i++)
        res->tm_yday += MonthLengths[IsLeapYear(st.wYear)][i];
    res->tm_yday += st.wDay - 1;

    return 0;
}

/*********************************************************************
 *      _localtime64 (MSVCRT.@)
 */
struct MSVCRT_tm* CDECL MSVCRT__localtime64(const MSVCRT___time64_t* secs)
{
    thread_data_t *data = msvcrt_get_thread_data();

    if(!data->time_buffer)
        data->time_buffer = MSVCRT_malloc(sizeof(struct MSVCRT_tm));

    if(_localtime64_s(data->time_buffer, secs))
        return NULL;
    return data->time_buffer;
}

/*********************************************************************
 *      _localtime32 (MSVCRT.@)
 */
struct MSVCRT_tm* CDECL MSVCRT__localtime32(const MSVCRT___time32_t* secs)
{
    MSVCRT___time64_t secs64;

    if(!secs)
        return NULL;

    secs64 = *secs;
    return MSVCRT__localtime64( &secs64 );
}

/*********************************************************************
 *      _localtime32_s (MSVCRT.@)
 */
int CDECL _localtime32_s(struct MSVCRT_tm *time, const MSVCRT___time32_t *secs)
{
    MSVCRT___time64_t secs64;

    if (!time || !secs || *secs < 0)
    {
        if (time)
            write_invalid_msvcrt_tm(time);

        *MSVCRT__errno() = MSVCRT_EINVAL;
        return MSVCRT_EINVAL;
    }

    secs64 = *secs;
    return _localtime64_s(time, &secs64);
}

/*********************************************************************
 *      localtime (MSVCRT.@)
 */
#ifdef _WIN64
struct MSVCRT_tm* CDECL MSVCRT_localtime(const MSVCRT___time64_t* secs)
{
    return MSVCRT__localtime64( secs );
}
#else
struct MSVCRT_tm* CDECL MSVCRT_localtime(const MSVCRT___time32_t* secs)
{
    return MSVCRT__localtime32( secs );
}
#endif

/*********************************************************************
 *      _gmtime64 (MSVCRT.@)
 */
int CDECL MSVCRT__gmtime64_s(struct MSVCRT_tm *res, const MSVCRT___time64_t *secs)
{
    int i;
    FILETIME ft;
    SYSTEMTIME st;
    ULONGLONG time;

    if (!res || !secs || *secs < 0 || *secs > _MAX__TIME64_T) {
        if (res) {
            write_invalid_msvcrt_tm(res);
        }

        *MSVCRT__errno() = MSVCRT_EINVAL;
        return MSVCRT_EINVAL;
    }

    time = *secs * (ULONGLONG)TICKSPERSEC + TICKS_1601_TO_1970;

    ft.dwHighDateTime = (UINT)(time >> 32);
    ft.dwLowDateTime  = (UINT)time;

    FileTimeToSystemTime(&ft, &st);

    res->tm_sec  = st.wSecond;
    res->tm_min  = st.wMinute;
    res->tm_hour = st.wHour;
    res->tm_mday = st.wDay;
    res->tm_year = st.wYear - 1900;
    res->tm_mon  = st.wMonth - 1;
    res->tm_wday = st.wDayOfWeek;
    for (i = res->tm_yday = 0; i < st.wMonth - 1; i++) {
        res->tm_yday += MonthLengths[IsLeapYear(st.wYear)][i];
    }

    res->tm_yday += st.wDay - 1;
    res->tm_isdst = 0;

    return 0;
}

/*********************************************************************
 *      _gmtime64 (MSVCRT.@)
 */
struct MSVCRT_tm* CDECL MSVCRT__gmtime64(const MSVCRT___time64_t *secs)
{
    thread_data_t * const data = msvcrt_get_thread_data();

    if(!data->time_buffer)
        data->time_buffer = MSVCRT_malloc(sizeof(struct MSVCRT_tm));

    if(MSVCRT__gmtime64_s(data->time_buffer, secs))
        return NULL;
    return data->time_buffer;
}

/*********************************************************************
 *      _gmtime32_s (MSVCRT.@)
 */
int CDECL MSVCRT__gmtime32_s(struct MSVCRT_tm *res, const MSVCRT___time32_t *secs)
{
    MSVCRT___time64_t secs64;

    if(secs) {
        secs64 = *secs;
        return MSVCRT__gmtime64_s(res, &secs64);
    }
    return MSVCRT__gmtime64_s(res, NULL);
}

/*********************************************************************
 *      _gmtime32 (MSVCRT.@)
 */
struct MSVCRT_tm* CDECL MSVCRT__gmtime32(const MSVCRT___time32_t* secs)
{
    MSVCRT___time64_t secs64;

    if(!secs)
        return NULL;

    secs64 = *secs;
    return MSVCRT__gmtime64( &secs64 );
}

/*********************************************************************
 *      gmtime (MSVCRT.@)
 */
#ifdef _WIN64
struct MSVCRT_tm* CDECL MSVCRT_gmtime(const MSVCRT___time64_t* secs)
{
    return MSVCRT__gmtime64( secs );
}
#else
struct MSVCRT_tm* CDECL MSVCRT_gmtime(const MSVCRT___time32_t* secs)
{
    return MSVCRT__gmtime32( secs );
}
#endif

/**********************************************************************
 *		_strdate (MSVCRT.@)
 */
char* CDECL MSVCRT__strdate(char* date)
{
  static const char format[] = "MM'/'dd'/'yy";

  GetDateFormatA(LOCALE_NEUTRAL, 0, NULL, format, date, 9);

  return date;
}

/**********************************************************************
 *              _strdate_s (MSVCRT.@)
 */
int CDECL _strdate_s(char* date, MSVCRT_size_t size)
{
    if(date && size)
        date[0] = '\0';

    if(!date) {
        *MSVCRT__errno() = MSVCRT_EINVAL;
        return MSVCRT_EINVAL;
    }

    if(size < 9) {
        *MSVCRT__errno() = MSVCRT_ERANGE;
        return MSVCRT_ERANGE;
    }

    MSVCRT__strdate(date);
    return 0;
}

/**********************************************************************
 *		_wstrdate (MSVCRT.@)
 */
MSVCRT_wchar_t* CDECL MSVCRT__wstrdate(MSVCRT_wchar_t* date)
{
  static const WCHAR format[] = { 'M','M','\'','/','\'','d','d','\'','/','\'','y','y',0 };

  GetDateFormatW(LOCALE_NEUTRAL, 0, NULL, format, date, 9);

  return date;
}

/**********************************************************************
 *              _wstrdate_s (MSVCRT.@)
 */
int CDECL _wstrdate_s(MSVCRT_wchar_t* date, MSVCRT_size_t size)
{
    if(date && size)
        date[0] = '\0';

    if(!date) {
        *MSVCRT__errno() = MSVCRT_EINVAL;
        return MSVCRT_EINVAL;
    }

    if(size < 9) {
        *MSVCRT__errno() = MSVCRT_ERANGE;
        return MSVCRT_ERANGE;
    }

    MSVCRT__wstrdate(date);
    return 0;
}

/*********************************************************************
 *		_strtime (MSVCRT.@)
 */
char* CDECL MSVCRT__strtime(char* time)
{
  static const char format[] = "HH':'mm':'ss";

  GetTimeFormatA(LOCALE_NEUTRAL, 0, NULL, format, time, 9); 

  return time;
}

/*********************************************************************
 *              _strtime_s (MSVCRT.@)
 */
int CDECL _strtime_s(char* time, MSVCRT_size_t size)
{
    if(time && size)
        time[0] = '\0';

    if(!time) {
        *MSVCRT__errno() = MSVCRT_EINVAL;
        return MSVCRT_EINVAL;
    }

    if(size < 9) {
        *MSVCRT__errno() = MSVCRT_ERANGE;
        return MSVCRT_ERANGE;
    }

    MSVCRT__strtime(time);
    return 0;
}

/*********************************************************************
 *		_wstrtime (MSVCRT.@)
 */
MSVCRT_wchar_t* CDECL MSVCRT__wstrtime(MSVCRT_wchar_t* time)
{
  static const WCHAR format[] = { 'H','H','\'',':','\'','m','m','\'',':','\'','s','s',0 };

  GetTimeFormatW(LOCALE_NEUTRAL, 0, NULL, format, time, 9);

  return time;
}

/*********************************************************************
 *              _wstrtime_s (MSVCRT.@)
 */
int CDECL _wstrtime_s(MSVCRT_wchar_t* time, MSVCRT_size_t size)
{
    if(time && size)
        time[0] = '\0';

    if(!time) {
        *MSVCRT__errno() = MSVCRT_EINVAL;
        return MSVCRT_EINVAL;
    }

    if(size < 9) {
        *MSVCRT__errno() = MSVCRT_ERANGE;
        return MSVCRT_ERANGE;
    }

    MSVCRT__wstrtime(time);
    return 0;
}

/*********************************************************************
 *		clock (MSVCRT.@)
 */
MSVCRT_clock_t CDECL MSVCRT_clock(void)
{
    LARGE_INTEGER systime;

    NtQuerySystemTime(&systime);
    return (systime.QuadPart - init_time) / (TICKSPERSEC / MSVCRT_CLOCKS_PER_SEC);
}

/*********************************************************************
 *		_difftime64 (MSVCRT.@)
 */
double CDECL MSVCRT__difftime64(MSVCRT___time64_t time1, MSVCRT___time64_t time2)
{
  return (double)(time1 - time2);
}

/*********************************************************************
 *		_difftime32 (MSVCRT.@)
 */
double CDECL MSVCRT__difftime32(MSVCRT___time32_t time1, MSVCRT___time32_t time2)
{
  return (double)(time1 - time2);
}

/*********************************************************************
 *		difftime (MSVCRT.@)
 */
#ifdef _WIN64
double CDECL MSVCRT_difftime(MSVCRT___time64_t time1, MSVCRT___time64_t time2)
{
    return MSVCRT__difftime64( time1, time2 );
}
#else
double CDECL MSVCRT_difftime(MSVCRT___time32_t time1, MSVCRT___time32_t time2)
{
    return MSVCRT__difftime32( time1, time2 );
}
#endif

/*********************************************************************
 *		_ftime64 (MSVCRT.@)
 */
void CDECL MSVCRT__ftime64(struct MSVCRT___timeb64 *buf)
{
  FILETIME ft;
  ULONGLONG time;

  _tzset_init();

  GetSystemTimeAsFileTime(&ft);

  time = ((ULONGLONG)ft.dwHighDateTime << 32) | ft.dwLowDateTime;

  buf->time = time / TICKSPERSEC - SECS_1601_TO_1970;
  buf->millitm = (time % TICKSPERSEC) / TICKSPERMSEC;
  buf->timezone = MSVCRT___timezone / 60;
  buf->dstflag = GetDaylightFlag();
}

/*********************************************************************
 *		_ftime64_s (MSVCRT.@)
 */
int CDECL MSVCRT__ftime64_s(struct MSVCRT___timeb64 *buf)
{
    if (!MSVCRT_CHECK_PMT( buf != NULL )) return MSVCRT_EINVAL;
    MSVCRT__ftime64(buf);
    return 0;
}

/*********************************************************************
 *		_ftime32 (MSVCRT.@)
 */
void CDECL MSVCRT__ftime32(struct MSVCRT___timeb32 *buf)
{
    struct MSVCRT___timeb64 buf64;

    MSVCRT__ftime64( &buf64 );
    buf->time     = buf64.time;
    buf->millitm  = buf64.millitm;
    buf->timezone = buf64.timezone;
    buf->dstflag  = buf64.dstflag;
}

/*********************************************************************
 *		_ftime32_s (MSVCRT.@)
 */
int CDECL MSVCRT__ftime32_s(struct MSVCRT___timeb32 *buf)
{
    if (!MSVCRT_CHECK_PMT( buf != NULL )) return MSVCRT_EINVAL;
    MSVCRT__ftime32(buf);
    return 0;
}

/*********************************************************************
 *		_ftime (MSVCRT.@)
 */
#ifdef _WIN64
void CDECL MSVCRT__ftime(struct MSVCRT___timeb64 *buf)
{
    MSVCRT__ftime64( buf );
}
#else
void CDECL MSVCRT__ftime(struct MSVCRT___timeb32 *buf)
{
    MSVCRT__ftime32( buf );
}
#endif

/*********************************************************************
 *		_time64 (MSVCRT.@)
 */
MSVCRT___time64_t CDECL MSVCRT__time64(MSVCRT___time64_t *buf)
{
    MSVCRT___time64_t curtime;
    struct MSVCRT___timeb64 tb;

    MSVCRT__ftime64(&tb);

    curtime = tb.time;
    return buf ? *buf = curtime : curtime;
}

/*********************************************************************
 *		_time32 (MSVCRT.@)
 */
MSVCRT___time32_t CDECL MSVCRT__time32(MSVCRT___time32_t *buf)
{
    MSVCRT___time32_t curtime;
    struct MSVCRT___timeb64 tb;

    MSVCRT__ftime64(&tb);

    curtime = tb.time;
    return buf ? *buf = curtime : curtime;
}

/*********************************************************************
 *		time (MSVCRT.@)
 */
#ifdef _WIN64
MSVCRT___time64_t CDECL MSVCRT_time(MSVCRT___time64_t* buf)
{
    return MSVCRT__time64( buf );
}
#else
MSVCRT___time32_t CDECL MSVCRT_time(MSVCRT___time32_t* buf)
{
    return MSVCRT__time32( buf );
}
#endif

/*********************************************************************
 *		__p__daylight (MSVCRT.@)
 */
int * CDECL MSVCRT___p__daylight(void)
{
	return &MSVCRT___daylight;
}

/*********************************************************************
 *		__p__dstbias (MSVCRT.@)
 */
int * CDECL MSVCRT___p__dstbias(void)
{
    return &MSVCRT__dstbias;
}

#if _MSVCR_VER >= 80
/*********************************************************************
 *              _get_dstbias (MSVCR80.@)
 */
int CDECL  MSVCRT__get_dstbias(int *seconds)
{
    if (!MSVCRT_CHECK_PMT(seconds != NULL)) return MSVCRT_EINVAL;
    *seconds = MSVCRT__dstbias;
    return 0;
}
#endif

/*********************************************************************
 *		__p__timezone (MSVCRT.@)
 */
MSVCRT_long * CDECL MSVCRT___p__timezone(void)
{
	return &MSVCRT___timezone;
}

/*********************************************************************
 *		_get_tzname (MSVCRT.@)
 */
int CDECL MSVCRT__get_tzname(MSVCRT_size_t *ret, char *buf, MSVCRT_size_t bufsize, int index)
{
    char *timezone;

    switch(index)
    {
    case 0:
        timezone = tzname_std;
        break;
    case 1:
        timezone = tzname_dst;
        break;
    default:
        *MSVCRT__errno() = MSVCRT_EINVAL;
        return MSVCRT_EINVAL;
    }

    if(!ret || (!buf && bufsize > 0) || (buf && !bufsize))
    {
        *MSVCRT__errno() = MSVCRT_EINVAL;
        return MSVCRT_EINVAL;
    }

    *ret = strlen(timezone)+1;
    if(!buf && !bufsize)
        return 0;
    if(*ret > bufsize)
    {
        buf[0] = 0;
        return MSVCRT_ERANGE;
    }

    strcpy(buf, timezone);
    return 0;
}

/*********************************************************************
 *		__p_tzname (MSVCRT.@)
 */
char ** CDECL __p__tzname(void)
{
	return MSVCRT__tzname;
}

#if _MSVCR_VER <= 90
#define STRFTIME_CHAR char
#define STRFTIME_TD(td, name) td->str.names.name
#else
#define STRFTIME_CHAR MSVCRT_wchar_t
#define STRFTIME_TD(td, name) td->wstr.names.name
#endif

#define strftime_str(a,b,c,d) strftime_nstr(a,b,c,d,MSVCRT_SIZE_MAX)
static inline BOOL strftime_nstr(STRFTIME_CHAR *str, MSVCRT_size_t *pos,
        MSVCRT_size_t max, const STRFTIME_CHAR *src, MSVCRT_size_t len)
{
    while(*src && len)
    {
        if(*pos >= max) {
            *str = 0;
            *MSVCRT__errno() = MSVCRT_ERANGE;
            return FALSE;
        }

        str[*pos] = *src;
        src++;
        *pos += 1;
        len--;
    }
    return TRUE;
}

static inline BOOL strftime_int(STRFTIME_CHAR *str, MSVCRT_size_t *pos, MSVCRT_size_t max,
        int src, int prec, int l, int h)
{
#if _MSVCR_VER > 90
    static const WCHAR fmt[] = {'%','0','*','d',0};
#endif
    MSVCRT_size_t len;

    if(!MSVCRT_CHECK_PMT(src>=l && src<=h)) {
        *str = 0;
        return FALSE;
    }

#if _MSVCR_VER <= 90
    len = MSVCRT__snprintf(str+*pos, max-*pos, "%0*d", prec, src);
#else
    len = MSVCRT__snwprintf(str+*pos, max-*pos, fmt, prec, src);
#endif
    if(len == -1) {
        *str = 0;
        *MSVCRT__errno() = MSVCRT_ERANGE;
        return FALSE;
    }

    *pos += len;
    return TRUE;
}

static inline BOOL strftime_format(STRFTIME_CHAR *str, MSVCRT_size_t *pos, MSVCRT_size_t max,
        const struct MSVCRT_tm *mstm, MSVCRT___lc_time_data *time_data, const STRFTIME_CHAR *format)
{
    MSVCRT_size_t count;
    BOOL ret = TRUE;

    while(*format && ret)
    {
        count = 1;
        while(format[0] == format[count]) count++;

        switch(*format) {
        case '\'':
            if(count % 2 == 0) break;

            format += count;
            count = 0;
            while(format[count] && format[count] != '\'') count++;

            ret = strftime_nstr(str, pos, max, format, count);
            if(!ret) return FALSE;
            if(format[count] == '\'') count++;
            break;
        case 'd':
            if(count > 2)
            {
                if(!MSVCRT_CHECK_PMT(mstm->tm_wday>=0 && mstm->tm_wday<=6))
                {
                    *str = 0;
                    return FALSE;
                }
            }
            switch(count) {
            case 1:
            case 2:
                ret = strftime_int(str, pos, max, mstm->tm_mday, count==1 ? 0 : 2, 1, 31);
                break;
            case 3:
                ret = strftime_str(str, pos, max, STRFTIME_TD(time_data, short_wday)[mstm->tm_wday]);
                break;
            default:
                ret = strftime_nstr(str, pos, max, format, count-4);
                if(ret)
                    ret = strftime_str(str, pos, max, STRFTIME_TD(time_data, wday)[mstm->tm_wday]);
                break;
            }
            break;
        case 'M':
            if(count > 2)
            {
                if(!MSVCRT_CHECK_PMT(mstm->tm_mon>=0 && mstm->tm_mon<=11))
                {
                    *str = 0;
                    return FALSE;
                }
            }
            switch(count) {
            case 1:
            case 2:
                ret = strftime_int(str, pos, max, mstm->tm_mon+1, count==1 ? 0 : 2, 1, 12);
                break;
            case 3:
                ret = strftime_str(str, pos, max, STRFTIME_TD(time_data, short_mon)[mstm->tm_mon]);
                break;
            default:
                ret = strftime_nstr(str, pos, max, format, count-4);
                if(ret)
                    ret = strftime_str(str, pos, max, STRFTIME_TD(time_data, mon)[mstm->tm_mon]);
                break;
            }
            break;
        case 'y':
            if(count > 1)
            {
#if _MSVCR_VER>=140
                if(!MSVCRT_CHECK_PMT(mstm->tm_year >= -1900 && mstm->tm_year <= 8099))
#else
                if(!MSVCRT_CHECK_PMT(mstm->tm_year >= 0))
#endif
                {
                    *str = 0;
                    return FALSE;
                }
            }

            switch(count) {
            case 1:
                ret = strftime_nstr(str, pos, max, format, 1);
                break;
            case 2:
            case 3:
                ret = strftime_nstr(str, pos, max, format, count-2);
                if(ret)
                    ret = strftime_int(str, pos, max, (mstm->tm_year+1900)%100, 2, 0, 99);
                break;
            default:
                ret = strftime_nstr(str, pos, max, format, count-4);
                if(ret)
                    ret = strftime_int(str, pos, max, mstm->tm_year+1900, 4, 0, 9999);
                break;
            }
            break;
        case 'h':
            if(!MSVCRT_CHECK_PMT(mstm->tm_hour>=0 && mstm->tm_hour<=23))
            {
                *str = 0;
                return FALSE;
            }
            if(count > 2)
                ret = strftime_nstr(str, pos, max, format, count-2);
            if(ret)
                ret = strftime_int(str, pos, max, (mstm->tm_hour + 11) % 12 + 1,
                        count == 1 ? 0 : 2, 1, 12);
            break;
        case 'H':
            if(count > 2)
                ret = strftime_nstr(str, pos, max, format, count-2);
            if(ret)
                ret = strftime_int(str, pos, max, mstm->tm_hour, count == 1 ? 0 : 2, 0, 23);
            break;
        case 'm':
            if(count > 2)
                ret = strftime_nstr(str, pos, max, format, count-2);
            if(ret)
                ret = strftime_int(str, pos, max, mstm->tm_min, count == 1 ? 0 : 2, 0, 59);
            break;
        case 's':
            if(count > 2)
                ret = strftime_nstr(str, pos, max, format, count-2);
            if(ret)
                ret = strftime_int(str, pos, max, mstm->tm_sec, count == 1 ? 0 : 2, 0, MAX_SECONDS);
            break;
        case 'a':
        case 'A':
        case 't':
            if(!MSVCRT_CHECK_PMT(mstm->tm_hour>=0 && mstm->tm_hour<=23))
            {
                *str = 0;
                return FALSE;
            }
            ret = strftime_nstr(str, pos, max,
                    mstm->tm_hour < 12 ? STRFTIME_TD(time_data, am) : STRFTIME_TD(time_data, pm),
                    (*format == 't' && count == 1) ? 1 : MSVCRT_SIZE_MAX);
            break;
        default:
            ret = strftime_nstr(str, pos, max, format, count);
            break;
        }
        format += count;
    }

    return ret;
}

#if _MSVCR_VER>=140
static inline BOOL strftime_tzdiff(STRFTIME_CHAR *str, MSVCRT_size_t *pos, MSVCRT_size_t max, BOOL is_dst)
{
    MSVCRT_long tz = MSVCRT___timezone + (is_dst ? MSVCRT__dstbias : 0);
    char sign;

    if(tz < 0) {
        sign = '+';
        tz = -tz;
    }else {
        sign = '-';
    }

    if(*pos < max)
        str[(*pos)++] = sign;
    if(!strftime_int(str, pos, max, tz/60/60, 2, 0, 99))
        return FALSE;
    return strftime_int(str, pos, max, tz/60%60, 2, 0, 59);
}
#endif

static MSVCRT_size_t strftime_impl(STRFTIME_CHAR *str, MSVCRT_size_t max,
        const STRFTIME_CHAR *format, const struct MSVCRT_tm *mstm,
        MSVCRT___lc_time_data *time_data, MSVCRT__locale_t loc)
{
    MSVCRT_size_t ret, tmp;
    BOOL alternate;
    int year = mstm ? mstm->tm_year + 1900 : -1;

    if(!str || !format) {
        if(str && max)
            *str = 0;
        *MSVCRT__errno() = MSVCRT_EINVAL;
        return 0;
    }

    if(!time_data)
        time_data = loc ? loc->locinfo->lc_time_curr : get_locinfo()->lc_time_curr;

    for(ret=0; *format && ret<max; format++) {
        if(*format != '%') {
            if(MSVCRT__isleadbyte_l((unsigned char)*format, loc)) {
                str[ret++] = *(format++);
                if(ret == max) continue;
                if(!MSVCRT_CHECK_PMT(str[ret]))
                    goto einval_error;
            }
            str[ret++] = *format;
            continue;
        }

        format++;
        if(*format == '#') {
            alternate = TRUE;
            format++;
        }else {
            alternate = FALSE;
        }

        if(!MSVCRT_CHECK_PMT(mstm))
            goto einval_error;

        switch(*format) {
        case 'c':
#if _MSVCR_VER>=140
            if(time_data == &cloc_time_data && !alternate)
            {
                static const WCHAR datetime_format[] =
                        { '%','a',' ','%','b',' ','%','e',' ','%','T',' ','%','Y',0 };
                tmp = strftime_impl(str+ret, max-ret, datetime_format, mstm, time_data, loc);
                if(!tmp)
                    return 0;
                ret += tmp;
                break;
            }
#endif
            if(!strftime_format(str, &ret, max, mstm, time_data,
                    alternate ? STRFTIME_TD(time_data, date) : STRFTIME_TD(time_data, short_date)))
                return 0;
            if(ret < max)
                str[ret++] = ' ';
            if(!strftime_format(str, &ret, max, mstm, time_data, STRFTIME_TD(time_data, time)))
                return 0;
            break;
        case 'x':
            if(!strftime_format(str, &ret, max, mstm, time_data,
                    alternate ? STRFTIME_TD(time_data, date) : STRFTIME_TD(time_data, short_date)))
                return 0;
            break;
        case 'X':
            if(!strftime_format(str, &ret, max, mstm, time_data, STRFTIME_TD(time_data, time)))
                return 0;
            break;
        case 'a':
            if(!MSVCRT_CHECK_PMT(mstm->tm_wday>=0 && mstm->tm_wday<=6))
                goto einval_error;
            if(!strftime_str(str, &ret, max, STRFTIME_TD(time_data, short_wday)[mstm->tm_wday]))
                return 0;
            break;
        case 'A':
            if(!MSVCRT_CHECK_PMT(mstm->tm_wday>=0 && mstm->tm_wday<=6))
                goto einval_error;
            if(!strftime_str(str, &ret, max, STRFTIME_TD(time_data, wday)[mstm->tm_wday]))
                return 0;
            break;
        case 'b':
#if _MSVCR_VER>=140
        case 'h':
#endif
            if(!MSVCRT_CHECK_PMT(mstm->tm_mon>=0 && mstm->tm_mon<=11))
                goto einval_error;
            if(!strftime_str(str, &ret, max, STRFTIME_TD(time_data, short_mon)[mstm->tm_mon]))
                return 0;
            break;
        case 'B':
            if(!MSVCRT_CHECK_PMT(mstm->tm_mon>=0 && mstm->tm_mon<=11))
                goto einval_error;
            if(!strftime_str(str, &ret, max, STRFTIME_TD(time_data, mon)[mstm->tm_mon]))
                return 0;
            break;
#if _MSVCR_VER>=140
        case 'C':
            if(!MSVCRT_CHECK_PMT(year>=0 && year<=9999))
                goto einval_error;
            if(!strftime_int(str, &ret, max, year/100, alternate ? 0 : 2, 0, 99))
                return 0;
            break;
#endif
        case 'd':
            if(!strftime_int(str, &ret, max, mstm->tm_mday, alternate ? 0 : 2, 1, 31))
                return 0;
            break;
#if _MSVCR_VER>=140
        case 'D':
            if(!MSVCRT_CHECK_PMT(year>=0 && year<=9999))
                goto einval_error;
            if(!strftime_int(str, &ret, max, mstm->tm_mon+1, alternate ? 0 : 2, 1, 12))
                return 0;
            if(ret < max)
                str[ret++] = '/';
            if(!strftime_int(str, &ret, max, mstm->tm_mday, alternate ? 0 : 2, 1, 31))
                return 0;
            if(ret < max)
                str[ret++] = '/';
            if(!strftime_int(str, &ret, max, year%100, alternate ? 0 : 2, 0, 99))
                return 0;
            break;
        case 'e':
            if(!strftime_int(str, &ret, max, mstm->tm_mday, alternate ? 0 : 2, 1, 31))
                return 0;
            if(!alternate && str[ret-2] == '0')
                str[ret-2] = ' ';
            break;
        case 'F':
            if(!strftime_int(str, &ret, max, year, alternate ? 0 : 4, 0, 9999))
                return 0;
            if(ret < max)
                str[ret++] = '-';
            if(!strftime_int(str, &ret, max, mstm->tm_mon+1, alternate ? 0 : 2, 1, 12))
                return 0;
            if(ret < max)
                str[ret++] = '-';
            if(!strftime_int(str, &ret, max, mstm->tm_mday, alternate ? 0 : 2, 1, 31))
                return 0;
            break;
        case 'g':
        case 'G':
            if(!MSVCRT_CHECK_PMT(year>=0 && year<=9999))
                goto einval_error;
            /* fall through */
        case 'V':
        {
            int iso_year = year;
            int iso_days = mstm->tm_yday - (mstm->tm_wday ? mstm->tm_wday : 7) + 4;
            if (iso_days < 0)
                iso_days += 365 + IsLeapYear(--iso_year);
            else if(iso_days >= 365 + IsLeapYear(iso_year))
                iso_days -= 365 + IsLeapYear(iso_year++);

            if(*format == 'G') {
                if(!strftime_int(str, &ret, max, iso_year, 4, 0, 9999))
                    return 0;
            } else if(*format == 'g') {
                if(!strftime_int(str, &ret, max, iso_year%100, 2, 0, 99))
                    return 0;
            } else {
                if(!strftime_int(str, &ret, max, iso_days/7 + 1, alternate ? 0 : 2, 0, 53))
                    return 0;
            }
            break;
        }
#endif
        case 'H':
            if(!strftime_int(str, &ret, max, mstm->tm_hour, alternate ? 0 : 2, 0, 23))
                return 0;
            break;
        case 'I':
            if(!MSVCRT_CHECK_PMT(mstm->tm_hour>=0 && mstm->tm_hour<=23))
                goto einval_error;
            if(!strftime_int(str, &ret, max, (mstm->tm_hour + 11) % 12 + 1,
                        alternate ? 0 : 2, 1, 12))
                return 0;
            break;
        case 'j':
            if(!strftime_int(str, &ret, max, mstm->tm_yday+1, alternate ? 0 : 3, 1, 366))
                return 0;
            break;
        case 'm':
            if(!strftime_int(str, &ret, max, mstm->tm_mon+1, alternate ? 0 : 2, 1, 12))
                return 0;
            break;
        case 'M':
            if(!strftime_int(str, &ret, max, mstm->tm_min, alternate ? 0 : 2, 0, 59))
                return 0;
            break;
#if _MSVCR_VER>=140
        case 'n':
            str[ret++] = '\n';
            break;
#endif
        case 'p':
            if(!MSVCRT_CHECK_PMT(mstm->tm_hour>=0 && mstm->tm_hour<=23))
                goto einval_error;
            if(!strftime_str(str, &ret, max, mstm->tm_hour<12 ?
                        STRFTIME_TD(time_data, am) : STRFTIME_TD(time_data, pm)))
                return 0;
            break;
#if _MSVCR_VER>=140
        case 'r':
            if(time_data == &cloc_time_data)
            {
                if(!MSVCRT_CHECK_PMT(mstm->tm_hour>=0 && mstm->tm_hour<=23))
                    goto einval_error;
                if(!strftime_int(str, &ret, max, (mstm->tm_hour + 11) % 12 + 1,
                            alternate ? 0 : 2, 1, 12))
                    return 0;
                if(ret < max)
                    str[ret++] = ':';
                if(!strftime_int(str, &ret, max, mstm->tm_min, alternate ? 0 : 2, 0, 59))
                    return 0;
                if(ret < max)
                    str[ret++] = ':';
                if(!strftime_int(str, &ret, max, mstm->tm_sec, alternate ? 0 : 2, 0, MAX_SECONDS))
                    return 0;
                if(ret < max)
                    str[ret++] = ' ';
                if(!strftime_str(str, &ret, max, mstm->tm_hour<12 ?
                            STRFTIME_TD(time_data, am) : STRFTIME_TD(time_data, pm)))
                    return 0;
            }
            else
            {
                if(!strftime_format(str, &ret, max, mstm, time_data, STRFTIME_TD(time_data, time)))
                    return 0;
            }
            break;
        case 'R':
            if(!strftime_int(str, &ret, max, mstm->tm_hour, alternate ? 0 : 2, 0, 23))
                return 0;
            if(ret < max)
                str[ret++] = ':';
            if(!strftime_int(str, &ret, max, mstm->tm_min, alternate ? 0 : 2, 0, 59))
                return 0;
            break;
#endif
        case 'S':
            if(!strftime_int(str, &ret, max, mstm->tm_sec, alternate ? 0 : 2, 0, MAX_SECONDS))
                return 0;
            break;
#if _MSVCR_VER>=140
        case 't':
            str[ret++] = '\t';
            break;
        case 'T':
            if(!strftime_int(str, &ret, max, mstm->tm_hour, alternate ? 0 : 2, 0, 23))
                return 0;
            if(ret < max)
                str[ret++] = ':';
            if(!strftime_int(str, &ret, max, mstm->tm_min, alternate ? 0 : 2, 0, 59))
                return 0;
            if(ret < max)
                str[ret++] = ':';
            if(!strftime_int(str, &ret, max, mstm->tm_sec, alternate ? 0 : 2, 0, MAX_SECONDS))
                return 0;
            break;
        case 'u':
            if(!MSVCRT_CHECK_PMT(mstm->tm_wday>=0 && mstm->tm_wday<=6))
                goto einval_error;
            tmp = mstm->tm_wday ? mstm->tm_wday : 7;
            if(!strftime_int(str, &ret, max, tmp, 0, 1, 7))
                return 0;
            break;
#endif
        case 'w':
            if(!strftime_int(str, &ret, max, mstm->tm_wday, 0, 0, 6))
                return 0;
            break;
        case 'y':
#if _MSVCR_VER>=140
            if(!MSVCRT_CHECK_PMT(year>=0 && year<=9999))
#else
            if(!MSVCRT_CHECK_PMT(year>=1900))
#endif
                goto einval_error;
            if(!strftime_int(str, &ret, max, year%100, alternate ? 0 : 2, 0, 99))
                return 0;
            break;
        case 'Y':
            if(!strftime_int(str, &ret, max, year, alternate ? 0 : 4, 0, 9999))
                return 0;
            break;
        case 'z':
#if _MSVCR_VER>=140
            MSVCRT__tzset();
            if(!strftime_tzdiff(str, &ret, max, mstm->tm_isdst))
                return 0;
            break;
#endif
        case 'Z':
            MSVCRT__tzset();
#if _MSVCR_VER <= 90
            if(MSVCRT__get_tzname(&tmp, str+ret, max-ret, mstm->tm_isdst ? 1 : 0))
                return 0;
#else
                if(MSVCRT__mbstowcs_s_l(&tmp, str+ret, max-ret,
                            mstm->tm_isdst ? tzname_dst : tzname_std,
                            MSVCRT__TRUNCATE, loc) == MSVCRT_STRUNCATE)
                    ret = max;
#endif
            ret += tmp-1;
            break;
        case 'U':
        case 'W':
            if(!MSVCRT_CHECK_PMT(mstm->tm_wday>=0 && mstm->tm_wday<=6))
                goto einval_error;
            if(!MSVCRT_CHECK_PMT(mstm->tm_yday>=0 && mstm->tm_yday<=365))
                goto einval_error;
            if(*format == 'U')
                tmp = mstm->tm_wday;
            else if(!mstm->tm_wday)
                tmp = 6;
            else
                tmp = mstm->tm_wday-1;

            tmp = mstm->tm_yday/7 + (tmp<=mstm->tm_yday%7);
            if(!strftime_int(str, &ret, max, tmp, alternate ? 0 : 2, 0, 53))
                return 0;
            break;
        case '%':
            str[ret++] = '%';
            break;
        default:
            WARN("unknown format %c\n", *format);
            MSVCRT_INVALID_PMT("unknown format", MSVCRT_EINVAL);
            goto einval_error;
        }
    }

    if(ret == max) {
        if(max)
            *str = 0;
        *MSVCRT__errno() = MSVCRT_ERANGE;
        return 0;
    }

    str[ret] = 0;
    return ret;

einval_error:
    *str = 0;
    return 0;
}

static MSVCRT_size_t strftime_helper(char *str, MSVCRT_size_t max, const char *format,
        const struct MSVCRT_tm *mstm, MSVCRT___lc_time_data *time_data, MSVCRT__locale_t loc)
{
#if _MSVCR_VER <= 90
    TRACE("(%p %ld %s %p %p %p)\n", str, max, format, mstm, time_data, loc);
    return strftime_impl(str, max, format, mstm, time_data, loc);
#else
    MSVCRT_wchar_t *s, *fmt;
    MSVCRT_size_t len;

    TRACE("(%p %ld %s %p %p %p)\n", str, max, format, mstm, time_data, loc);

    if (!MSVCRT_CHECK_PMT(str != NULL)) return 0;
    if (!MSVCRT_CHECK_PMT(max != 0)) return 0;
    *str = 0;
    if (!MSVCRT_CHECK_PMT(format != NULL)) return 0;

    len = MSVCRT__mbstowcs_l( NULL, format, 0, loc ) + 1;
    if (!len || !(fmt = MSVCRT_malloc( len*sizeof(MSVCRT_wchar_t) ))) return 0;
    MSVCRT__mbstowcs_l(fmt, format, len, loc);

    if ((s = MSVCRT_malloc( max*sizeof(MSVCRT_wchar_t) )))
    {
        len = strftime_impl( s, max, fmt, mstm, time_data, loc );
        if (len)
            len = MSVCRT__wcstombs_l( str, s, max, loc );
        MSVCRT_free( s );
    }
    else len = 0;

    MSVCRT_free( fmt );
    return len;
#endif
}

#if _MSVCR_VER >= 80
/********************************************************************
 *     _strftime_l (MSVCR80.@)
 */
MSVCRT_size_t CDECL MSVCRT__strftime_l( char *str, MSVCRT_size_t max, const char *format,
        const struct MSVCRT_tm *mstm, MSVCRT__locale_t loc )
{
    return strftime_helper(str, max, format, mstm, NULL, loc);
}
#endif

/*********************************************************************
 *		_Strftime (MSVCRT.@)
 */
MSVCRT_size_t CDECL _Strftime(char *str, MSVCRT_size_t max, const char *format,
        const struct MSVCRT_tm *mstm, MSVCRT___lc_time_data *time_data)
{
    return strftime_helper(str, max, format, mstm, time_data, NULL);
}

/*********************************************************************
 *		strftime (MSVCRT.@)
 */
MSVCRT_size_t CDECL MSVCRT_strftime( char *str, MSVCRT_size_t max, const char *format,
                                     const struct MSVCRT_tm *mstm )
{
    return strftime_helper(str, max, format, mstm, NULL, NULL);
}

static MSVCRT_size_t wcsftime_helper( MSVCRT_wchar_t *str, MSVCRT_size_t max,
        const MSVCRT_wchar_t *format, const struct MSVCRT_tm *mstm,
        MSVCRT___lc_time_data *time_data, MSVCRT__locale_t loc )
{
#if _MSVCR_VER <= 90
    char *s, *fmt;
    MSVCRT_size_t len;

    TRACE("%p %ld %s %p %p %p\n", str, max, debugstr_w(format), mstm, time_data, loc);

    len = MSVCRT__wcstombs_l( NULL, format, 0, loc ) + 1;
    if (!(fmt = MSVCRT_malloc( len ))) return 0;
    MSVCRT__wcstombs_l(fmt, format, len, loc);

    if ((s = MSVCRT_malloc( max*4 )))
    {
        if (!strftime_impl( s, max*4, fmt, mstm, time_data, loc )) s[0] = 0;
        len = MSVCRT__mbstowcs_l( str, s, max, loc );
        MSVCRT_free( s );
    }
    else len = 0;

    MSVCRT_free( fmt );
    return len;
#else
    TRACE("%p %ld %s %p %p %p\n", str, max, debugstr_w(format), mstm, time_data, loc);
    return strftime_impl(str, max, format, mstm, time_data, loc);
#endif
}

/*********************************************************************
 *              _wcsftime_l (MSVCRT.@)
 */
MSVCRT_size_t CDECL MSVCRT__wcsftime_l( MSVCRT_wchar_t *str, MSVCRT_size_t max,
        const MSVCRT_wchar_t *format, const struct MSVCRT_tm *mstm, MSVCRT__locale_t loc )
{
    return wcsftime_helper(str, max, format, mstm, NULL, loc);
}

/*********************************************************************
 *     wcsftime (MSVCRT.@)
 */
MSVCRT_size_t CDECL MSVCRT_wcsftime( MSVCRT_wchar_t *str, MSVCRT_size_t max,
                                     const MSVCRT_wchar_t *format, const struct MSVCRT_tm *mstm )
{
    return wcsftime_helper(str, max, format, mstm, NULL, NULL);
}

#if _MSVCR_VER >= 110
/*********************************************************************
 *		_Wcsftime (MSVCR110.@)
 */
MSVCRT_size_t CDECL _Wcsftime(MSVCRT_wchar_t *str, MSVCRT_size_t max,
        const MSVCRT_wchar_t *format, const struct MSVCRT_tm *mstm,
        MSVCRT___lc_time_data *time_data)
{
    return wcsftime_helper(str, max, format, mstm, time_data, NULL);
}
#endif

static char* asctime_buf(char *buf, const struct MSVCRT_tm *mstm)
{
    static const char wday[7][4] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static const char month[12][4] = {"Jan", "Feb", "Mar", "Apr", "May",
        "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    if (!mstm || mstm->tm_sec<0 || mstm->tm_sec>59
            || mstm->tm_min<0 || mstm->tm_min>59
            || mstm->tm_hour<0 || mstm->tm_hour>23
            || mstm->tm_mon<0 || mstm->tm_mon>11
            || mstm->tm_wday<0 || mstm->tm_wday>6
            || mstm->tm_year<0 || mstm->tm_mday<0
            || mstm->tm_mday>MonthLengths[IsLeapYear(1900+mstm->tm_year)][mstm->tm_mon]) {
        *MSVCRT__errno() = MSVCRT_EINVAL;
        return NULL;
    }

#if _MSVCR_VER>=140
    /* C89 (4.12.3.1) uses space-padding for day of month. */
    MSVCRT__snprintf(buf, 26, "%s %s %2d %02d:%02d:%02d %c%03d\n", wday[mstm->tm_wday],
            month[mstm->tm_mon], mstm->tm_mday, mstm->tm_hour, mstm->tm_min,
            mstm->tm_sec, '1'+(mstm->tm_year+900)/1000, (900+mstm->tm_year)%1000);
#else
    MSVCRT__snprintf(buf, 26, "%s %s %02d %02d:%02d:%02d %c%03d\n", wday[mstm->tm_wday],
            month[mstm->tm_mon], mstm->tm_mday, mstm->tm_hour, mstm->tm_min,
            mstm->tm_sec, '1'+(mstm->tm_year+900)/1000, (900+mstm->tm_year)%1000);
#endif
    return buf;
}

/*********************************************************************
 *		asctime (MSVCRT.@)
 */
char * CDECL MSVCRT_asctime(const struct MSVCRT_tm *mstm)
{
    thread_data_t *data = msvcrt_get_thread_data();

    /* asctime returns date in format that always has exactly 26 characters */
    if (!data->asctime_buffer) {
        data->asctime_buffer = MSVCRT_malloc(26);
        if (!data->asctime_buffer) {
            *MSVCRT__errno() = MSVCRT_ENOMEM;
            return NULL;
        }
    }

    return asctime_buf(data->asctime_buffer, mstm);
}

/*********************************************************************
 *      asctime_s (MSVCRT.@)
 */
int CDECL MSVCRT_asctime_s(char* time, MSVCRT_size_t size, const struct MSVCRT_tm *mstm)
{
    if (!MSVCRT_CHECK_PMT(time != NULL)) return MSVCRT_EINVAL;
    if (size) time[0] = 0;
    if (!MSVCRT_CHECK_PMT(size >= 26)) return MSVCRT_EINVAL;
    if (!MSVCRT_CHECK_PMT(mstm != NULL)) return MSVCRT_EINVAL;
    if (!MSVCRT_CHECK_PMT(mstm->tm_sec >= 0 && mstm->tm_sec < 60)) return MSVCRT_EINVAL;
    if (!MSVCRT_CHECK_PMT(mstm->tm_min >= 0 && mstm->tm_min < 60)) return MSVCRT_EINVAL;
    if (!MSVCRT_CHECK_PMT(mstm->tm_hour >= 0 && mstm->tm_hour < 24)) return MSVCRT_EINVAL;
    if (!MSVCRT_CHECK_PMT(mstm->tm_mon >= 0 && mstm->tm_mon < 12)) return MSVCRT_EINVAL;
    if (!MSVCRT_CHECK_PMT(mstm->tm_wday >= 0 && mstm->tm_wday < 7)) return MSVCRT_EINVAL;
    if (!MSVCRT_CHECK_PMT(mstm->tm_year >= 0)) return MSVCRT_EINVAL;
    if (!MSVCRT_CHECK_PMT(mstm->tm_mday >= 0)) return MSVCRT_EINVAL;
    if (!MSVCRT_CHECK_PMT(mstm->tm_mday <= MonthLengths[IsLeapYear(1900+mstm->tm_year)][mstm->tm_mon])) return MSVCRT_EINVAL;

    asctime_buf(time, mstm);
    return 0;
}

/*********************************************************************
 *		_wasctime (MSVCRT.@)
 */
MSVCRT_wchar_t * CDECL MSVCRT__wasctime(const struct MSVCRT_tm *mstm)
{
    thread_data_t *data = msvcrt_get_thread_data();
    char buffer[26];

    if(!data->wasctime_buffer) {
        data->wasctime_buffer = MSVCRT_malloc(26*sizeof(MSVCRT_wchar_t));
        if(!data->wasctime_buffer) {
            *MSVCRT__errno() = MSVCRT_ENOMEM;
            return NULL;
        }
    }

    if(!asctime_buf(buffer, mstm))
        return NULL;

    MultiByteToWideChar(CP_ACP, 0, buffer, -1, data->wasctime_buffer, 26);
    return data->wasctime_buffer;
}

/*********************************************************************
 *      _wasctime_s (MSVCRT.@)
 */
int CDECL MSVCRT__wasctime_s(MSVCRT_wchar_t* time, MSVCRT_size_t size, const struct MSVCRT_tm *mstm)
{
    char buffer[26];
    int ret;

    if (!MSVCRT_CHECK_PMT(time != NULL)) return MSVCRT_EINVAL;
    if (size) time[0] = 0;
    if (!MSVCRT_CHECK_PMT(size >= 26)) return MSVCRT_EINVAL;
    if (!MSVCRT_CHECK_PMT(mstm != NULL)) return MSVCRT_EINVAL;

    ret = MSVCRT_asctime_s(buffer, sizeof(buffer), mstm);
    if(ret)
        return ret;
    MultiByteToWideChar(CP_ACP, 0, buffer, -1, time, size);
    return 0;
}

/*********************************************************************
 *		_ctime64 (MSVCRT.@)
 */
char * CDECL MSVCRT__ctime64(const MSVCRT___time64_t *time)
{
    struct MSVCRT_tm *t;
    t = MSVCRT__localtime64( time );
    if (!t) return NULL;
    return MSVCRT_asctime( t );
}

/*********************************************************************
 *		_ctime64_s (MSVCRT.@)
 */
int CDECL MSVCRT__ctime64_s(char *res, MSVCRT_size_t len, const MSVCRT___time64_t *time)
{
    struct MSVCRT_tm *t;

    if (!MSVCRT_CHECK_PMT( res != NULL )) return MSVCRT_EINVAL;
    if (!MSVCRT_CHECK_PMT( len >= 26 )) return MSVCRT_EINVAL;
    res[0] = '\0';
    if (!MSVCRT_CHECK_PMT( time != NULL )) return MSVCRT_EINVAL;
    if (!MSVCRT_CHECK_PMT( *time > 0 )) return MSVCRT_EINVAL;

    t = MSVCRT__localtime64( time );
    strcpy( res, MSVCRT_asctime( t ) );
    return 0;
}

/*********************************************************************
 *		_ctime32 (MSVCRT.@)
 */
char * CDECL MSVCRT__ctime32(const MSVCRT___time32_t *time)
{
    struct MSVCRT_tm *t;
    t = MSVCRT__localtime32( time );
    if (!t) return NULL;
    return MSVCRT_asctime( t );
}

/*********************************************************************
 *		_ctime32_s (MSVCRT.@)
 */
int CDECL MSVCRT__ctime32_s(char *res, MSVCRT_size_t len, const MSVCRT___time32_t *time)
{
    struct MSVCRT_tm *t;

    if (!MSVCRT_CHECK_PMT( res != NULL )) return MSVCRT_EINVAL;
    if (!MSVCRT_CHECK_PMT( len >= 26 )) return MSVCRT_EINVAL;
    res[0] = '\0';
    if (!MSVCRT_CHECK_PMT( time != NULL )) return MSVCRT_EINVAL;
    if (!MSVCRT_CHECK_PMT( *time > 0 )) return MSVCRT_EINVAL;

    t = MSVCRT__localtime32( time );
    strcpy( res, MSVCRT_asctime( t ) );
    return 0;
}

/*********************************************************************
 *		ctime (MSVCRT.@)
 */
#ifdef _WIN64
char * CDECL MSVCRT_ctime(const MSVCRT___time64_t *time)
{
    return MSVCRT__ctime64( time );
}
#else
char * CDECL MSVCRT_ctime(const MSVCRT___time32_t *time)
{
    return MSVCRT__ctime32( time );
}
#endif

/*********************************************************************
 *		_wctime64 (MSVCRT.@)
 */
MSVCRT_wchar_t * CDECL MSVCRT__wctime64(const MSVCRT___time64_t *time)
{
    return MSVCRT__wasctime( MSVCRT__localtime64(time) );
}

/*********************************************************************
 *		_wctime32 (MSVCRT.@)
 */
MSVCRT_wchar_t * CDECL MSVCRT__wctime32(const MSVCRT___time32_t *time)
{
    return MSVCRT__wasctime( MSVCRT__localtime32(time) );
}

/*********************************************************************
 *		_wctime (MSVCRT.@)
 */
#ifdef _WIN64
MSVCRT_wchar_t * CDECL MSVCRT__wctime(const MSVCRT___time64_t *time)
{
    return MSVCRT__wctime64( time );
}
#else
MSVCRT_wchar_t * CDECL MSVCRT__wctime(const MSVCRT___time32_t *time)
{
    return MSVCRT__wctime32( time );
}
#endif

/*********************************************************************
 *              _wctime64_s (MSVCRT.@)
 */
int CDECL MSVCRT__wctime64_s(MSVCRT_wchar_t *buf,
        MSVCRT_size_t size, const MSVCRT___time64_t *time)
{
    struct MSVCRT_tm tm;
    int ret;

    if(!MSVCRT_CHECK_PMT(buf != NULL)) return MSVCRT_EINVAL;
    if(!MSVCRT_CHECK_PMT(size != 0)) return MSVCRT_EINVAL;
    buf[0] = 0;
    if(!MSVCRT_CHECK_PMT(time != NULL)) return MSVCRT_EINVAL;
    if(!MSVCRT_CHECK_PMT(*time >= 0)) return MSVCRT_EINVAL;
    if(!MSVCRT_CHECK_PMT(*time <= _MAX__TIME64_T)) return MSVCRT_EINVAL;

    ret = _localtime64_s(&tm, time);
    if(ret != 0)
        return ret;

    return MSVCRT__wasctime_s(buf, size, &tm);
}

/*********************************************************************
 *              _wctime32_s (MSVCRT.@)
 */
int CDECL MSVCRT__wctime32_s(MSVCRT_wchar_t *buf, MSVCRT_size_t size,
        const MSVCRT___time32_t *time)
{
    struct MSVCRT_tm tm;
    int ret;

    if(!MSVCRT_CHECK_PMT(buf != NULL)) return MSVCRT_EINVAL;
    if(!MSVCRT_CHECK_PMT(size != 0)) return MSVCRT_EINVAL;
    buf[0] = 0;
    if(!MSVCRT_CHECK_PMT(time != NULL)) return MSVCRT_EINVAL;
    if(!MSVCRT_CHECK_PMT(*time >= 0)) return MSVCRT_EINVAL;

    ret = _localtime32_s(&tm, time);
    if(ret != 0)
        return ret;

    return MSVCRT__wasctime_s(buf, size, &tm);
}

#if _MSVCR_VER >= 80

/*********************************************************************
 * _get_timezone (MSVCR80.@)
 */
int CDECL _get_timezone(LONG *timezone)
{
    if(!MSVCRT_CHECK_PMT(timezone != NULL)) return MSVCRT_EINVAL;

    *timezone = MSVCRT___timezone;
    return 0;
}

/*********************************************************************
 * _get_daylight (MSVCR80.@)
 */
int CDECL _get_daylight(int *hours)
{
    if(!MSVCRT_CHECK_PMT(hours != NULL)) return MSVCRT_EINVAL;

    *hours = MSVCRT___daylight;
    return 0;
}

#endif /* _MSVCR_VER >= 80 */

#if _MSVCR_VER >= 140

#define TIME_UTC 1

struct _timespec32
{
    MSVCRT___time32_t tv_sec;
    LONG tv_nsec;
};

struct _timespec64
{
    MSVCRT___time64_t tv_sec;
    LONG tv_nsec;
};

/*********************************************************************
 * _timespec64_get (UCRTBASE.@)
 */
int CDECL _timespec64_get(struct _timespec64 *ts, int base)
{
    ULONGLONG time;
    FILETIME ft;

    if(!MSVCRT_CHECK_PMT(ts != NULL)) return 0;
    if(base != TIME_UTC) return 0;

    GetSystemTimePreciseAsFileTime(&ft);
    time = ((ULONGLONG)ft.dwHighDateTime << 32) | ft.dwLowDateTime;

    ts->tv_sec = time / TICKSPERSEC - SECS_1601_TO_1970;
    ts->tv_nsec = time % TICKSPERSEC * 100;
    return base;
}

/*********************************************************************
 * _timespec32_get (UCRTBASE.@)
 */
int CDECL _timespec32_get(struct _timespec32 *ts, int base)
{
    struct _timespec64 ts64;

    if(!MSVCRT_CHECK_PMT(ts != NULL)) return 0;
    if(base != TIME_UTC) return 0;

    if(_timespec64_get(&ts64, base) != base)
        return 0;
    if(ts64.tv_sec != (MSVCRT___time32_t)ts64.tv_sec)
        return 0;

    ts->tv_sec = ts64.tv_sec;
    ts->tv_nsec = ts64.tv_nsec;
    return base;
}
#endif /* _MSVCR_VER >= 140 */
