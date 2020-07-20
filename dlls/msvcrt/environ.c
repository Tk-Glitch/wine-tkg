/*
 * msvcrt.dll environment functions
 *
 * Copyright 1996,1998 Marcus Meissner
 * Copyright 1996 Jukka Iivonen
 * Copyright 1997,2000 Uwe Bonnes
 * Copyright 2000 Jon Griffiths
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
#include "wine/unicode.h"
#include "msvcrt.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(msvcrt);

/*********************************************************************
 *		getenv (MSVCRT.@)
 */
char * CDECL MSVCRT_getenv(const char *name)
{
    char **environ;
    unsigned int length=strlen(name);

    for (environ = MSVCRT__environ; *environ; environ++)
    {
        char *str = *environ;
        char *pos = strchr(str,'=');
        if (pos && ((pos - str) == length) && !MSVCRT__strnicmp(str,name,length))
        {
            TRACE("(%s): got %s\n", debugstr_a(name), debugstr_a(pos + 1));
            return pos + 1;
        }
    }
    return NULL;
}

/*********************************************************************
 *		_wgetenv (MSVCRT.@)
 */
MSVCRT_wchar_t * CDECL MSVCRT__wgetenv(const MSVCRT_wchar_t *name)
{
    MSVCRT_wchar_t **environ;
    unsigned int length=strlenW(name);

    /* Initialize the _wenviron array if it's not already created. */
    if (!MSVCRT__wenviron)
        MSVCRT__wenviron = msvcrt_SnapshotOfEnvironmentW(NULL);

    for (environ = MSVCRT__wenviron; *environ; environ++)
    {
        MSVCRT_wchar_t *str = *environ;
        MSVCRT_wchar_t *pos = strchrW(str,'=');
        if (pos && ((pos - str) == length) && !MSVCRT__wcsnicmp(str,name,length))
        {
            TRACE("(%s): got %s\n", debugstr_w(name), debugstr_w(pos + 1));
            return pos + 1;
        }
    }
    return NULL;
}

/*********************************************************************
 *		_putenv (MSVCRT.@)
 */
int CDECL _putenv(const char *str)
{
 char *name, *value;
 char *dst;
 int ret;

 TRACE("%s\n", debugstr_a(str));

 if (!str)
   return -1;
   
 name = HeapAlloc(GetProcessHeap(), 0, strlen(str) + 1);
 if (!name)
   return -1;
 dst = name;
 while (*str && *str != '=')
  *dst++ = *str++;
 if (!*str++)
 {
   ret = -1;
   goto finish;
 }
 *dst++ = '\0';
 value = dst;
 while (*str)
  *dst++ = *str++;
 *dst = '\0';

 ret = SetEnvironmentVariableA(name, value[0] ? value : NULL) ? 0 : -1;

 /* _putenv returns success on deletion of nonexistent variable, unlike [Rtl]SetEnvironmentVariable */
 if ((ret == -1) && (GetLastError() == ERROR_ENVVAR_NOT_FOUND)) ret = 0;

 MSVCRT__environ = msvcrt_SnapshotOfEnvironmentA(MSVCRT__environ);
 /* Update the __p__wenviron array only when already initialized */
 if (MSVCRT__wenviron)
   MSVCRT__wenviron = msvcrt_SnapshotOfEnvironmentW(MSVCRT__wenviron);
   
finish:
 HeapFree(GetProcessHeap(), 0, name);
 return ret;
}

/*********************************************************************
 *		_wputenv (MSVCRT.@)
 */
int CDECL _wputenv(const MSVCRT_wchar_t *str)
{
 MSVCRT_wchar_t *name, *value;
 MSVCRT_wchar_t *dst;
 int ret;

 TRACE("%s\n", debugstr_w(str));

 if (!str)
   return -1;
 name = HeapAlloc(GetProcessHeap(), 0, (strlenW(str) + 1) * sizeof(MSVCRT_wchar_t));
 if (!name)
   return -1;
 dst = name;
 while (*str && *str != '=')
  *dst++ = *str++;
 if (!*str++)
 {
   ret = -1;
   goto finish;
 }
 *dst++ = 0;
 value = dst;
 while (*str)
  *dst++ = *str++;
 *dst = 0;

 ret = SetEnvironmentVariableW(name, value[0] ? value : NULL) ? 0 : -1;

 /* _putenv returns success on deletion of nonexistent variable, unlike [Rtl]SetEnvironmentVariable */
 if ((ret == -1) && (GetLastError() == ERROR_ENVVAR_NOT_FOUND)) ret = 0;

 MSVCRT__environ = msvcrt_SnapshotOfEnvironmentA(MSVCRT__environ);
 MSVCRT__wenviron = msvcrt_SnapshotOfEnvironmentW(MSVCRT__wenviron);

finish:
 HeapFree(GetProcessHeap(), 0, name);
 return ret;
}

/*********************************************************************
 *		_putenv_s (MSVCRT.@)
 */
int CDECL _putenv_s(const char *name, const char *value)
{
    int ret;

    TRACE("%s %s\n", debugstr_a(name), debugstr_a(value));

    if (!MSVCRT_CHECK_PMT(name != NULL)) return -1;
    if (!MSVCRT_CHECK_PMT(value != NULL)) return -1;

    ret = SetEnvironmentVariableA(name, value[0] ? value : NULL) ? 0 : -1;

    /* _putenv returns success on deletion of nonexistent variable, unlike [Rtl]SetEnvironmentVariable */
    if ((ret == -1) && (GetLastError() == ERROR_ENVVAR_NOT_FOUND)) ret = 0;

    MSVCRT__environ = msvcrt_SnapshotOfEnvironmentA(MSVCRT__environ);
    MSVCRT__wenviron = msvcrt_SnapshotOfEnvironmentW(MSVCRT__wenviron);

    return ret;
}

/*********************************************************************
 *		_wputenv_s (MSVCRT.@)
 */
int CDECL _wputenv_s(const MSVCRT_wchar_t *name, const MSVCRT_wchar_t *value)
{
    int ret;

    TRACE("%s %s\n", debugstr_w(name), debugstr_w(value));

    if (!MSVCRT_CHECK_PMT(name != NULL)) return -1;
    if (!MSVCRT_CHECK_PMT(value != NULL)) return -1;

    ret = SetEnvironmentVariableW(name, value[0] ? value : NULL) ? 0 : -1;

    /* _putenv returns success on deletion of nonexistent variable, unlike [Rtl]SetEnvironmentVariable */
    if ((ret == -1) && (GetLastError() == ERROR_ENVVAR_NOT_FOUND)) ret = 0;

    MSVCRT__environ = msvcrt_SnapshotOfEnvironmentA(MSVCRT__environ);
    MSVCRT__wenviron = msvcrt_SnapshotOfEnvironmentW(MSVCRT__wenviron);

    return ret;
}

#if _MSVCR_VER>=80

/******************************************************************
 *		_dupenv_s (MSVCR80.@)
 */
int CDECL _dupenv_s(char **buffer, MSVCRT_size_t *numberOfElements, const char *varname)
{
    char*               e;
    MSVCRT_size_t       sz;

    if (!MSVCRT_CHECK_PMT(buffer != NULL)) return MSVCRT_EINVAL;
    if (!MSVCRT_CHECK_PMT(varname != NULL)) return MSVCRT_EINVAL;

    if (!(e = MSVCRT_getenv(varname))) return *MSVCRT__errno() = MSVCRT_EINVAL;

    sz = strlen(e) + 1;
    if (!(*buffer = MSVCRT_malloc(sz)))
    {
        if (numberOfElements) *numberOfElements = 0;
        return *MSVCRT__errno() = MSVCRT_ENOMEM;
    }
    strcpy(*buffer, e);
    if (numberOfElements) *numberOfElements = sz;
    return 0;
}

/******************************************************************
 *		_wdupenv_s (MSVCR80.@)
 */
int CDECL _wdupenv_s(MSVCRT_wchar_t **buffer, MSVCRT_size_t *numberOfElements,
                     const MSVCRT_wchar_t *varname)
{
    MSVCRT_wchar_t*     e;
    MSVCRT_size_t       sz;

    if (!MSVCRT_CHECK_PMT(buffer != NULL)) return MSVCRT_EINVAL;
    if (!MSVCRT_CHECK_PMT(varname != NULL)) return MSVCRT_EINVAL;

    if (!(e = MSVCRT__wgetenv(varname))) return *MSVCRT__errno() = MSVCRT_EINVAL;

    sz = strlenW(e) + 1;
    if (!(*buffer = MSVCRT_malloc(sz * sizeof(MSVCRT_wchar_t))))
    {
        if (numberOfElements) *numberOfElements = 0;
        return *MSVCRT__errno() = MSVCRT_ENOMEM;
    }
    strcpyW(*buffer, e);
    if (numberOfElements) *numberOfElements = sz;
    return 0;
}

#endif /* _MSVCR_VER>=80 */

/******************************************************************
 *		getenv_s (MSVCRT.@)
 */
int CDECL getenv_s(MSVCRT_size_t *pReturnValue, char* buffer, MSVCRT_size_t numberOfElements, const char *varname)
{
    char*       e;

    if (!MSVCRT_CHECK_PMT(pReturnValue != NULL)) return MSVCRT_EINVAL;
    if (!MSVCRT_CHECK_PMT(!(buffer == NULL && numberOfElements > 0))) return MSVCRT_EINVAL;
    if (!MSVCRT_CHECK_PMT(varname != NULL)) return MSVCRT_EINVAL;

    if (!(e = MSVCRT_getenv(varname)))
    {
        *pReturnValue = 0;
        return *MSVCRT__errno() = MSVCRT_EINVAL;
    }
    *pReturnValue = strlen(e) + 1;
    if (numberOfElements < *pReturnValue)
    {
        return *MSVCRT__errno() = MSVCRT_ERANGE;
    }
    strcpy(buffer, e);
    return 0;
}

/******************************************************************
 *		_wgetenv_s (MSVCRT.@)
 */
int CDECL _wgetenv_s(MSVCRT_size_t *pReturnValue, MSVCRT_wchar_t *buffer, MSVCRT_size_t numberOfElements,
                     const MSVCRT_wchar_t *varname)
{
    MSVCRT_wchar_t*     e;

    if (!MSVCRT_CHECK_PMT(pReturnValue != NULL)) return MSVCRT_EINVAL;
    if (!MSVCRT_CHECK_PMT(!(buffer == NULL && numberOfElements > 0))) return MSVCRT_EINVAL;
    if (!MSVCRT_CHECK_PMT(varname != NULL)) return MSVCRT_EINVAL;

    if (!(e = MSVCRT__wgetenv(varname)))
    {
        *pReturnValue = 0;
        return *MSVCRT__errno() = MSVCRT_EINVAL;
    }
    *pReturnValue = strlenW(e) + 1;
    if (numberOfElements < *pReturnValue)
    {
        return *MSVCRT__errno() = MSVCRT_ERANGE;
    }
    strcpyW(buffer, e);
    return 0;
}

/*********************************************************************
 *		_get_environ (MSVCRT.@)
 */
void CDECL MSVCRT__get_environ(char ***ptr)
{
    *ptr = MSVCRT__environ;
}

/*********************************************************************
 *		_get_wenviron (MSVCRT.@)
 */
void CDECL MSVCRT__get_wenviron(MSVCRT_wchar_t ***ptr)
{
    *ptr = MSVCRT__wenviron;
}
