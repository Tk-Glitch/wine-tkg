/**
 * This file has no copyright assigned and is placed in the Public Domain.
 * This file is part of the Wine project.
 */

#ifndef _WPROCESS_DEFINED
#define _WPROCESS_DEFINED

#include <corecrt.h>

#ifdef __cplusplus
extern "C" {
#endif

intptr_t WINAPIV _wexecl(const wchar_t*,const wchar_t*,...);
intptr_t WINAPIV _wexecle(const wchar_t*,const wchar_t*,...);
intptr_t WINAPIV _wexeclp(const wchar_t*,const wchar_t*,...);
intptr_t WINAPIV _wexeclpe(const wchar_t*,const wchar_t*,...);
intptr_t __cdecl _wexecv(const wchar_t*,const wchar_t* const *);
intptr_t __cdecl _wexecve(const wchar_t*,const wchar_t* const *,const wchar_t* const *);
intptr_t __cdecl _wexecvp(const wchar_t*,const wchar_t* const *);
intptr_t __cdecl _wexecvpe(const wchar_t*,const wchar_t* const *,const wchar_t* const *);
intptr_t WINAPIV _wspawnl(int,const wchar_t*,const wchar_t*,...);
intptr_t WINAPIV _wspawnle(int,const wchar_t*,const wchar_t*,...);
intptr_t WINAPIV _wspawnlp(int,const wchar_t*,const wchar_t*,...);
intptr_t WINAPIV _wspawnlpe(int,const wchar_t*,const wchar_t*,...);
intptr_t __cdecl _wspawnv(int,const wchar_t*,const wchar_t* const *);
intptr_t __cdecl _wspawnve(int,const wchar_t*,const wchar_t* const *,const wchar_t* const *);
intptr_t __cdecl _wspawnvp(int,const wchar_t*,const wchar_t* const *);
intptr_t __cdecl _wspawnvpe(int,const wchar_t*,const wchar_t* const *,const wchar_t* const *);
int      __cdecl _wsystem(const wchar_t*);

#ifdef __cplusplus
}
#endif

#endif /* _WPROCESS_DEFINED */
