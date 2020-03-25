/*
 * Utility routines' prototypes etc.
 *
 * Copyright 1998 Bertho A. Stultiens (BS)
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

#ifndef __WRC_UTILS_H
#define __WRC_UTILS_H

#include <stddef.h>	/* size_t */

#include "wrctypes.h"

void *xmalloc(size_t);
void *xrealloc(void *, size_t);
char *xstrdup(const char *str);

#ifndef __GNUC__
#define __attribute__(X)
#endif

int compare_striA( const char *str1, const char *str2 );
int compare_striW( const WCHAR *str1, const WCHAR *str2 );
char *strmake(const char* fmt, ...) __attribute__((__format__ (__printf__, 1, 2 )));
int strendswith( const char *str, const char *end );
int parser_error(const char *s, ...) __attribute__((format (printf, 1, 2)));
int parser_warning(const char *s, ...) __attribute__((format (printf, 1, 2)));
void internal_error(const char *file, int line, const char *s, ...) __attribute__((format (printf, 3, 4), noreturn));
void fatal_perror( const char *msg, ... ) __attribute__((format (printf, 1, 2), noreturn));
void error(const char *s, ...) __attribute__((format (printf, 1, 2), noreturn));
void warning(const char *s, ...) __attribute__((format (printf, 1, 2)));
void chat(const char *s, ...) __attribute__((format (printf, 1, 2)));

char *dup_basename(const char *name, const char *ext);
int compare_name_id(const name_id_t *n1, const name_id_t *n2);
string_t *convert_string_unicode( const string_t *str, int codepage );
char *convert_string_utf8( const string_t *str, int codepage );
void free_string( string_t *str );
int check_valid_utf8( const string_t *str, int codepage );
int get_language_codepage( unsigned short lang, unsigned short sublang );
int is_valid_codepage(int cp);

#endif
