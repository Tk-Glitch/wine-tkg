/*
 * Copyright 2020 Dmitry Timoshkov
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

#ifndef _ADSLDP_PRIVATE_H
#define _ADSLDP_PRIVATE_H

#include "wine/heap.h"

static inline WCHAR *strdupW(const WCHAR *src)
{
    WCHAR *dst;
    if (!src) return NULL;
    if ((dst = heap_alloc((wcslen(src) + 1) * sizeof(WCHAR)))) wcscpy(dst, src);
    return dst;
}

static inline LPWSTR strnUtoW( LPCSTR str, DWORD inlen, DWORD *outlen )
{
    LPWSTR ret = NULL;
    *outlen = 0;
    if (str)
    {
        DWORD len = MultiByteToWideChar( CP_UTF8, 0, str, inlen, NULL, 0 );
        if ((ret = heap_alloc( (len + 1) * sizeof(WCHAR) )))
        {
            MultiByteToWideChar( CP_UTF8, 0, str, inlen, ret, len );
            ret[len] = 0;
            *outlen = len;
        }
    }
    return ret;
}

struct attribute_type
{
    WCHAR *oid;
    WCHAR *name;
    ULONG name_count;
    WCHAR *syntax;
    int single_value;
};

DWORD map_ldap_error(DWORD) DECLSPEC_HIDDEN;
struct attribute_type *load_schema(LDAP *ld, ULONG *, ULONG *) DECLSPEC_HIDDEN;
ADSTYPEENUM get_schema_type(const WCHAR *, const struct attribute_type *, ULONG, ULONG) DECLSPEC_HIDDEN;
void free_attribute_types(struct attribute_type *, ULONG) DECLSPEC_HIDDEN;

#endif
