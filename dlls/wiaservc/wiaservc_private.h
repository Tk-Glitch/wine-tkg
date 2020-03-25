/*
 * WiaServc definitions
 *
 * Copyright 2009 Damjan Jovanovic
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

#ifndef __WIASERVC_PRIVATE__
#define __WIASERVC_PRIVATE__

typedef struct
{
    IClassFactory IClassFactory_iface;
} ClassFactoryImpl;

extern ClassFactoryImpl WIASERVC_ClassFactory DECLSPEC_HIDDEN;

typedef struct
{
    IWiaDevMgr IWiaDevMgr_iface;
    LONG ref;
} wiadevmgr;

HRESULT wiadevmgr_Constructor(IWiaDevMgr **ppObj) DECLSPEC_HIDDEN;

/* Little helper functions */
static inline char *
wiaservc_strdup(const char *s)
{
    size_t n = strlen(s) + 1;
    char *d = HeapAlloc(GetProcessHeap(), 0, n);
    return d ? memcpy(d, s, n) : NULL;
}

#endif /* __WIASERVC_PRIVATE__ */
