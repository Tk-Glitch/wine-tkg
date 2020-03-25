/*
 * Copyright 2019 Hans Leidekker for CodeWeavers
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

#ifndef _WINE_MSADO15_PRIVATE_H_
#define _WINE_MSADO15_PRIVATE_H_

#define MAKE_ADO_HRESULT( err ) MAKE_HRESULT( SEVERITY_ERROR, FACILITY_CONTROL, err )

HRESULT Command_create( void ** ) DECLSPEC_HIDDEN;
HRESULT Connection_create( void ** ) DECLSPEC_HIDDEN;
HRESULT Recordset_create( void ** ) DECLSPEC_HIDDEN;
HRESULT Stream_create( void ** ) DECLSPEC_HIDDEN;

static inline void *heap_realloc_zero( void *mem, SIZE_T len )
{
    if (!mem) return HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, len );
    return HeapReAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, mem, len );
}

static inline WCHAR *strdupW( const WCHAR *src )
{
    WCHAR *dst;
    if (!src) return NULL;
    if ((dst = heap_alloc( (lstrlenW( src ) + 1) * sizeof(*dst) ))) lstrcpyW( dst, src );
    return dst;
}

#endif /* _WINE_MSADO15_PRIVATE_H_ */
