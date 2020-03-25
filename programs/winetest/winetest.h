/*
 * winetest definitions
 *
 * Copyright 2003 Dimitrie O. Paun
 * Copyright 2003 Ferenc Wagner
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

#ifndef __WINETESTS_H
#define __WINETESTS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

void fatal (const char* msg);
void warning (const char* msg);
void WINAPIV xprintf (const char *fmt, ...);
char *vstrmake (size_t *lenp, __ms_va_list ap);
char * WINAPIV strmake (size_t *lenp, ...);
int goodtagchar (char c);
const char *findbadtagchar (const char *tag);

int send_file (const char *url, const char *name);

extern HANDLE logfile;

/* GUI definitions */

#include <windows.h>

#ifndef __WINE_ALLOC_SIZE
#define __WINE_ALLOC_SIZE(x)
#endif
void *heap_alloc (size_t len) __WINE_ALLOC_SIZE(1);
void *heap_realloc (void *op, size_t len) __WINE_ALLOC_SIZE(2);
char *heap_strdup( const char *str );
void heap_free (void *op);

enum report_type {
    R_STATUS = 0,
    R_PROGRESS,
    R_STEP,
    R_DELTA,
    R_TAG,
    R_DIR,
    R_OUT,
    R_WARNING,
    R_ERROR,
    R_FATAL,
    R_ASK,
    R_TEXTMODE,
    R_QUIET
};

#define MAXTAGLEN 30
extern char *tag;
extern char *email;
extern BOOL aborting;
int guiAskTag (void);
int guiAskEmail (void);
int WINAPIV report (enum report_type t, ...);

#endif /* __WINETESTS_H */
