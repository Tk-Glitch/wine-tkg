/**
 * This file has no copyright assigned and is placed in the Public Domain.
 * This file is part of the Wine project.
 * No warranty is given; refer to the file DISCLAIMER.PD within this package.
 */

#ifndef _WSTDIO_DEFINED
#define _WSTDIO_DEFINED

#include <corecrt.h>
#include <corecrt_stdio_config.h>

#ifndef RC_INVOKED
#include <stdarg.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <pshpack8.h>

#ifndef _FILE_DEFINED
#define _FILE_DEFINED
#include <pshpack8.h>
typedef struct _iobuf
{
  char* _ptr;
  int   _cnt;
  char* _base;
  int   _flag;
  int   _file;
  int   _charbuf;
  int   _bufsiz;
  char* _tmpfname;
} FILE;
#include <poppack.h>
#endif  /* _FILE_DEFINED */

#ifndef WEOF
#define WEOF        (wint_t)(0xFFFF)
#endif

FILE *__cdecl __acrt_iob_func(unsigned index);

#define stdin  (__acrt_iob_func(0))
#define stdout (__acrt_iob_func(1))
#define stderr (__acrt_iob_func(2))

wint_t   __cdecl _fgetwc_nolock(FILE*);
wint_t   __cdecl _fgetwchar(void);
wint_t   __cdecl _fputwc_nolock(wint_t,FILE*);
wint_t   __cdecl _fputwchar(wint_t);
wint_t   __cdecl _getwc_nolock(FILE*);
wchar_t* __cdecl _getws(wchar_t*);
wint_t   __cdecl _putwc_nolock(wint_t,FILE*);
int      __cdecl _putws(const wchar_t*);
wint_t   __cdecl _ungetwc_nolock(wint_t,FILE*);
FILE*    __cdecl _wfdopen(int,const wchar_t*);
FILE*    __cdecl _wfopen(const wchar_t*,const wchar_t*);
errno_t  __cdecl _wfopen_s(FILE**,const wchar_t*,const wchar_t*);
FILE*    __cdecl _wfreopen(const wchar_t*,const wchar_t*,FILE*);
FILE*    __cdecl _wfsopen(const wchar_t*,const wchar_t*,int);
void     __cdecl _wperror(const wchar_t*);
FILE*    __cdecl _wpopen(const wchar_t*,const wchar_t*);
int      __cdecl _wremove(const wchar_t*);
wchar_t* __cdecl _wtempnam(const wchar_t*,const wchar_t*);
wchar_t* __cdecl _wtmpnam(wchar_t*);

wint_t   __cdecl fgetwc(FILE*);
wchar_t* __cdecl fgetws(wchar_t*,int,FILE*);
wint_t   __cdecl fputwc(wint_t,FILE*);
int      __cdecl fputws(const wchar_t*,FILE*);
int      __cdecl fputws(const wchar_t*,FILE*);
wint_t   __cdecl getwc(FILE*);
wint_t   __cdecl getwchar(void);
wchar_t* __cdecl getws(wchar_t*);
wint_t   __cdecl putwc(wint_t,FILE*);
wint_t   __cdecl putwchar(wint_t);
int      __cdecl putws(const wchar_t*);
wint_t   __cdecl ungetwc(wint_t,FILE*);

#ifdef _UCRT

_ACRTIMP int __cdecl __stdio_common_vfwprintf(unsigned __int64,FILE*,const wchar_t*,_locale_t,__ms_va_list);
_ACRTIMP int __cdecl __stdio_common_vfwprintf_s(unsigned __int64,FILE*,const wchar_t*,_locale_t,__ms_va_list);
_ACRTIMP int __cdecl __stdio_common_vsnwprintf_s(unsigned __int64,wchar_t*,size_t,size_t,const wchar_t*,_locale_t,__ms_va_list);
_ACRTIMP int __cdecl __stdio_common_vswprintf(unsigned __int64,wchar_t*,size_t,const wchar_t*,_locale_t,__ms_va_list);
_ACRTIMP int __cdecl __stdio_common_vswprintf_p(unsigned __int64,wchar_t*,size_t,const wchar_t*,_locale_t,__ms_va_list);
_ACRTIMP int __cdecl __stdio_common_vswprintf_s(unsigned __int64,wchar_t*,size_t,const wchar_t*,_locale_t,__ms_va_list);

_ACRTIMP int __cdecl __stdio_common_vfwscanf(unsigned __int64,FILE*,const wchar_t*,_locale_t,__ms_va_list);
_ACRTIMP int __cdecl __stdio_common_vswscanf(unsigned __int64,const wchar_t*,size_t,const wchar_t*,_locale_t,__ms_va_list);

static inline int __cdecl _vsnwprintf(wchar_t *buffer, size_t size, const wchar_t *format, __ms_va_list args)
{
    int ret = __stdio_common_vswprintf(_CRT_INTERNAL_LOCAL_PRINTF_OPTIONS, buffer, size, format, NULL, args);
    return ret < 0 ? -1 : ret;
}

static inline int WINAPIV _snwprintf(wchar_t *buffer, size_t size, const wchar_t* format, ...)
{
    int ret;
    __ms_va_list args;

    __ms_va_start(args, format);
    ret = __stdio_common_vswprintf(_CRT_INTERNAL_LOCAL_PRINTF_OPTIONS, buffer, size, format, NULL, args);
    __ms_va_end(args);
    return ret;
}

static inline int __cdecl _vsnwprintf_s(wchar_t *buffer, size_t size, size_t count, const wchar_t *format, __ms_va_list args)
{
    int ret = __stdio_common_vsnwprintf_s(_CRT_INTERNAL_LOCAL_PRINTF_OPTIONS, buffer, size, count, format, NULL, args);
    return ret < 0 ? -1 : ret;
}

static inline int WINAPIV _snwprintf_s(wchar_t *buffer, size_t size, size_t count, const wchar_t *format, ...)
{
    int ret;
    __ms_va_list args;

    __ms_va_start(args, format);
    ret = __stdio_common_vsnwprintf_s(_CRT_INTERNAL_LOCAL_PRINTF_OPTIONS, buffer, size, count, format, NULL, args);
    __ms_va_end(args);
    return ret;
}

static inline int WINAPIV vswprintf(wchar_t *buffer, size_t size, const wchar_t *format, __ms_va_list args)
{
    int ret = __stdio_common_vswprintf(_CRT_INTERNAL_LOCAL_PRINTF_OPTIONS, buffer, size, format, NULL, args);
    return ret < 0 ? -1 : ret;
}

static inline int WINAPIV swprintf(wchar_t *buffer, size_t size, const wchar_t *format, ...)
{
    int ret;
    __ms_va_list args;

    __ms_va_start(args, format);
    ret = __stdio_common_vswprintf(_CRT_INTERNAL_LOCAL_PRINTF_OPTIONS, buffer, size, format, NULL, args);
    __ms_va_end(args);
    return ret;
}

static inline int WINAPIV _vswprintf(wchar_t *buffer, const wchar_t *format, __ms_va_list args)
{
    int ret = __stdio_common_vswprintf(_CRT_INTERNAL_LOCAL_PRINTF_OPTIONS, buffer, -1, format, NULL, args);
    return ret < 0 ? -1 : ret;
}

static inline int WINAPIV _swprintf(wchar_t *buffer, const wchar_t *format, ...)
{
    int ret;
    __ms_va_list args;

    __ms_va_start(args, format);
    ret = __stdio_common_vswprintf(_CRT_INTERNAL_LOCAL_PRINTF_OPTIONS, buffer, -1, format, NULL, args);
    __ms_va_end(args);
    return ret < 0 ? -1 : ret;
}

static inline int WINAPIV vswprintf_s(wchar_t *buffer, size_t size, const wchar_t *format, __ms_va_list args)
{
    int ret = __stdio_common_vswprintf_s(_CRT_INTERNAL_LOCAL_PRINTF_OPTIONS, buffer, size, format, NULL, args);
    return ret < 0 ? -1 : ret;
}

static inline int WINAPIV swprintf_s(wchar_t *buffer, size_t size, const wchar_t* format, ...)
{
    int ret;
    __ms_va_list args;

    __ms_va_start(args, format);
    ret = __stdio_common_vswprintf_s(_CRT_INTERNAL_LOCAL_PRINTF_OPTIONS, buffer, size, format, NULL, args);
    __ms_va_end(args);
    return ret;
}

static inline int __cdecl _vscwprintf(const wchar_t *format, __ms_va_list args)
{
    int ret = __stdio_common_vswprintf(_CRT_INTERNAL_LOCAL_PRINTF_OPTIONS | _CRT_INTERNAL_PRINTF_STANDARD_SNPRINTF_BEHAVIOR,
                                       NULL, 0, format, NULL, args);
    return ret < 0 ? -1 : ret;
}

static inline int __cdecl _vswprintf_p_l(wchar_t *buffer, size_t size, const wchar_t *format, _locale_t locale, __ms_va_list args)
{
    int ret = __stdio_common_vswprintf_p(_CRT_INTERNAL_LOCAL_PRINTF_OPTIONS, buffer, size, format, locale, args);
    return ret < 0 ? -1 : ret;
}

static inline int __cdecl _vscwprintf_p_l(const wchar_t *format, _locale_t locale, __ms_va_list args)
{
    int ret = __stdio_common_vswprintf_p(_CRT_INTERNAL_LOCAL_PRINTF_OPTIONS | _CRT_INTERNAL_PRINTF_STANDARD_SNPRINTF_BEHAVIOR,
                                         NULL, 0, format, locale, args);
    return ret < 0 ? -1 : ret;
}

static inline int WINAPIV _scwprintf(const wchar_t *format, ...)
{
    int ret;
    __ms_va_list args;

    __ms_va_start(args, format);
    ret = __stdio_common_vswprintf(_CRT_INTERNAL_LOCAL_PRINTF_OPTIONS | _CRT_INTERNAL_PRINTF_STANDARD_SNPRINTF_BEHAVIOR,
                                   NULL, 0, format, NULL, args);
    __ms_va_end(args);
    return ret;
}

static inline int __cdecl vfwprintf(FILE *file, const wchar_t *format, __ms_va_list args)
{
    return __stdio_common_vfwprintf(_CRT_INTERNAL_LOCAL_PRINTF_OPTIONS, file, format, NULL, args);
}

static inline int WINAPIV fwprintf(FILE *file, const wchar_t *format, ...)
{
    int ret;
    __ms_va_list args;

    __ms_va_start(args, format);
    ret = __stdio_common_vfwprintf(_CRT_INTERNAL_LOCAL_PRINTF_OPTIONS, file, format, NULL, args);
    __ms_va_end(args);
    return ret;
}

static inline int vfwprintf_s(FILE *file, const wchar_t *format, __ms_va_list args)
{
    return __stdio_common_vfwprintf_s(_CRT_INTERNAL_LOCAL_PRINTF_OPTIONS, file, format, NULL, args);
}

static inline int WINAPIV fwprintf_s(FILE *file, const wchar_t *format, ...)
{
    int ret;
    __ms_va_list args;

    __ms_va_start(args, format);
    ret = vfwprintf_s(file, format, args);
    __ms_va_end(args);
    return ret;
}

static inline int __cdecl vwprintf(const wchar_t *format, __ms_va_list args)
{
    return __stdio_common_vfwprintf(_CRT_INTERNAL_LOCAL_PRINTF_OPTIONS, stdout, format, NULL, args);
}

static inline int WINAPIV wprintf(const wchar_t *format, ...)
{
    int ret;
    __ms_va_list args;

    __ms_va_start(args, format);
    ret = __stdio_common_vfwprintf(_CRT_INTERNAL_LOCAL_PRINTF_OPTIONS, stdout, format, NULL, args);
    __ms_va_end(args);
    return ret;
}

static inline int vwprintf_s(const wchar_t *format, __ms_va_list args)
{
    return __stdio_common_vfwprintf_s(_CRT_INTERNAL_LOCAL_PRINTF_OPTIONS, stdout, format, NULL, args);
}

static inline int WINAPIV wprintf_s(const wchar_t *format, ...)
{
    int ret;
    __ms_va_list args;

    __ms_va_start(args, format);
    ret = vfwprintf_s(stdout, format, args);
    __ms_va_end(args);
    return ret;
}

static inline int WINAPIV swscanf(const wchar_t *buffer, const wchar_t *format, ...)
{
    int ret;
    __ms_va_list args;

    __ms_va_start(args, format);
    ret = __stdio_common_vswscanf(_CRT_INTERNAL_LOCAL_SCANF_OPTIONS, buffer, -1, format, NULL, args);
    __ms_va_end(args);
    return ret;
}

static inline int WINAPIV swscanf_s(const wchar_t *buffer, const wchar_t *format, ...)
{
    int ret;
    __ms_va_list args;

    __ms_va_start(args, format);
    ret = __stdio_common_vswscanf(_CRT_INTERNAL_LOCAL_SCANF_OPTIONS | _CRT_INTERNAL_SCANF_SECURECRT, buffer, -1, format, NULL, args);
    __ms_va_end(args);
    return ret;
}

static inline int WINAPIV fwscanf(FILE *file, const wchar_t *format, ...)
{
    int ret;
    __ms_va_list args;

    __ms_va_start(args, format);
    ret = __stdio_common_vfwscanf(_CRT_INTERNAL_LOCAL_SCANF_OPTIONS, file, format, NULL, args);
    __ms_va_end(args);
    return ret;
}

static inline int WINAPIV fwscanf_s(FILE *file, const wchar_t *format, ...)
{
    int ret;
    __ms_va_list args;

    __ms_va_start(args, format);
    ret = __stdio_common_vfwscanf(_CRT_INTERNAL_LOCAL_SCANF_OPTIONS | _CRT_INTERNAL_SCANF_SECURECRT, file, format, NULL, args);
    __ms_va_end(args);
    return ret;
}

static inline int WINAPIV wscanf(FILE *file, const wchar_t *format, ...)
{
    int ret;
    __ms_va_list args;

    __ms_va_start(args, format);
    ret = __stdio_common_vfwscanf(_CRT_INTERNAL_LOCAL_SCANF_OPTIONS, stdin, format, NULL, args);
    __ms_va_end(args);
    return ret;
}

static inline int WINAPIV wscanf_s(FILE *file, const wchar_t *format, ...)
{
    int ret;
    __ms_va_list args;

    __ms_va_start(args, format);
    ret = __stdio_common_vfwscanf(_CRT_INTERNAL_LOCAL_SCANF_OPTIONS | _CRT_INTERNAL_SCANF_SECURECRT, stdin, format, NULL, args);
    __ms_va_end(args);
    return ret;
}

#else /* _UCRT */

_ACRTIMP int WINAPIV _scwprintf(const wchar_t*,...);
_ACRTIMP int WINAPIV _snwprintf(wchar_t*,size_t,const wchar_t*,...);
_ACRTIMP int WINAPIV _snwprintf_s(wchar_t*,size_t,size_t,const wchar_t*,...);
_ACRTIMP int __cdecl _vscwprintf(const wchar_t*,__ms_va_list);
_ACRTIMP int __cdecl _vscwprintf_p_l(const wchar_t*,_locale_t,__ms_va_list);
_ACRTIMP int __cdecl _vsnwprintf(wchar_t*,size_t,const wchar_t*,__ms_va_list);
_ACRTIMP int __cdecl _vsnwprintf_s(wchar_t*,size_t,size_t,const wchar_t*,__ms_va_list);
_ACRTIMP int __cdecl _vswprintf_p_l(wchar_t*,size_t,const wchar_t*,_locale_t,__ms_va_list);
_ACRTIMP int WINAPIV fwprintf(FILE*,const wchar_t*,...);
_ACRTIMP int WINAPIV fwprintf_s(FILE*,const wchar_t*,...);
_ACRTIMP int WINAPIV swprintf_s(wchar_t*,size_t,const wchar_t*,...);
_ACRTIMP int __cdecl vfwprintf(FILE*,const wchar_t*,__ms_va_list);
_ACRTIMP int __cdecl vfwprintf_s(FILE*,const wchar_t*,__ms_va_list);
_ACRTIMP int __cdecl vswprintf_s(wchar_t*,size_t,const wchar_t*,__ms_va_list);
_ACRTIMP int __cdecl vwprintf(const wchar_t*,__ms_va_list);
_ACRTIMP int __cdecl vwprintf_s(const wchar_t*,__ms_va_list);
_ACRTIMP int WINAPIV wprintf(const wchar_t*,...);
_ACRTIMP int WINAPIV wprintf_s(const wchar_t*,...);

#ifdef _CRT_NON_CONFORMING_SWPRINTFS
int WINAPIV swprintf(wchar_t*,const wchar_t*,...);
int __cdecl vswprintf(wchar_t*,const wchar_t*,__ms_va_list);
#else  /*  _CRT_NON_CONFORMING_SWPRINTFS */
static inline int vswprintf(wchar_t *buffer, size_t size, const wchar_t *format, __ms_va_list args) { return _vsnwprintf(buffer,size,format,args); }
static inline int WINAPIV swprintf(wchar_t *buffer, size_t size, const wchar_t *format, ...)
{
    int ret;
    __ms_va_list args;

    __ms_va_start(args, format);
    ret = _vsnwprintf(buffer, size, format, args);
    __ms_va_end(args);
    return ret;
}
#endif  /*  _CRT_NON_CONFORMING_SWPRINTFS */

_ACRTIMP int WINAPIV fwscanf(FILE*,const wchar_t*,...);
_ACRTIMP int WINAPIV fwscanf_s(FILE*,const wchar_t*,...);
_ACRTIMP int WINAPIV swscanf(const wchar_t*,const wchar_t*,...);
_ACRTIMP int WINAPIV swscanf_s(const wchar_t*,const wchar_t*,...);
_ACRTIMP int WINAPIV wscanf(const wchar_t*,...);
_ACRTIMP int WINAPIV wscanf_s(const wchar_t*,...);

#endif /* _UCRT */

#ifdef __cplusplus
}
#endif

#include <poppack.h>

#endif /* _WSTDIO_DEFINED */
