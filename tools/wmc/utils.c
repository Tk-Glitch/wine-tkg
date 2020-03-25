/*
 * Utility routines
 *
 * Copyright 1998,2000 Bertho A. Stultiens
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
#include "wine/port.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

#include "wmctypes.h"
#include "winnls.h"
#include "utils.h"
#include "wmc.h"

#define SUPPRESS_YACC_ERROR_MESSAGE

static void generic_msg(const char *s, const char *t, va_list ap)
{
	fprintf(stderr, "%s:%d:%d: %s: ", input_name ? input_name : "stdin", line_number, char_number, t);
	vfprintf(stderr, s, ap);
}

/*
 * The yyerror routine should not exit because we use the error-token
 * to determine the syntactic error in the source. However, YACC
 * uses the same routine to print an error just before the error
 * token is reduced.
 * The extra routine 'xyyerror' is used to exit after giving a real
 * message.
 */
int mcy_error(const char *s, ...)
{
#ifndef SUPPRESS_YACC_ERROR_MESSAGE
	va_list ap;
	va_start(ap, s);
	generic_msg(s, "Yacc error", ap);
	va_end(ap);
#endif
	return 1;
}

int xyyerror(const char *s, ...)
{
	va_list ap;
	va_start(ap, s);
	generic_msg(s, "Error", ap);
	va_end(ap);
	exit(1);
	return 1;
}

int mcy_warning(const char *s, ...)
{
	va_list ap;
	va_start(ap, s);
	generic_msg(s, "Warning", ap);
	va_end(ap);
	return 0;
}

void internal_error(const char *file, int line, const char *s, ...)
{
	va_list ap;
	va_start(ap, s);
	fprintf(stderr, "Internal error (please report) %s %d: ", file, line);
	vfprintf(stderr, s, ap);
	va_end(ap);
	exit(3);
}

void fatal_perror( const char *msg, ... )
{
        va_list valist;
        va_start( valist, msg );
	fprintf(stderr, "Error: ");
        vfprintf( stderr, msg, valist );
        perror( " " );
        va_end( valist );
        exit(2);
}

void error(const char *s, ...)
{
	va_list ap;
	va_start(ap, s);
	fprintf(stderr, "Error: ");
	vfprintf(stderr, s, ap);
	va_end(ap);
	exit(2);
}

void warning(const char *s, ...)
{
	va_list ap;
	va_start(ap, s);
	fprintf(stderr, "Warning: ");
	vfprintf(stderr, s, ap);
	va_end(ap);
}

char *dup_basename(const char *name, const char *ext)
{
	int namelen;
	int extlen = strlen(ext);
	char *base;
	char *slash;

	if(!name)
		name = "wmc.tab";

	slash = strrchr(name, '/');
	if (slash)
		name = slash + 1;

	namelen = strlen(name);

	/* +4 for later extension and +1 for '\0' */
	base = xmalloc(namelen +4 +1);
	strcpy(base, name);
	if(!strcasecmp(name + namelen-extlen, ext))
	{
		base[namelen - extlen] = '\0';
	}
	return base;
}

void *xmalloc(size_t size)
{
    void *res;

    assert(size > 0);
    res = malloc(size);
    if(res == NULL)
    {
	error("Virtual memory exhausted.\n");
    }
    memset(res, 0x55, size);
    return res;
}


void *xrealloc(void *p, size_t size)
{
    void *res;

    assert(size > 0);
    res = realloc(p, size);
    if(res == NULL)
    {
	error("Virtual memory exhausted.\n");
    }
    return res;
}

char *xstrdup(const char *str)
{
	char *s;

	assert(str != NULL);
	s = xmalloc(strlen(str)+1);
	return strcpy(s, str);
}

char *strmake( const char* fmt, ... )
{
    int n;
    size_t size = 100;
    va_list ap;

    for (;;)
    {
        char *p = xmalloc( size );
        va_start( ap, fmt );
        n = vsnprintf( p, size, fmt, ap );
        va_end( ap );
        if (n == -1) size *= 2;
        else if ((size_t)n >= size) size = n + 1;
        else return p;
        free( p );
    }
}

int strendswith( const char *str, const char *end )
{
    int l = strlen(str);
    int m = strlen(end);
    return l >= m && !strcmp( str + l - m, end );
}

int unistrlen(const WCHAR *s)
{
	int n;
	for(n = 0; *s; n++, s++)
		;
	return n;
}

WCHAR *unistrcpy(WCHAR *dst, const WCHAR *src)
{
	WCHAR *t = dst;
	while(*src)
		*t++ = *src++;
	*t = 0;
	return dst;
}

WCHAR *xunistrdup(const WCHAR * str)
{
	WCHAR *s;

	assert(str != NULL);
	s = xmalloc((unistrlen(str)+1) * sizeof(WCHAR));
	return unistrcpy(s, str);
}

int unistricmp(const WCHAR *s1, const WCHAR *s2)
{
	int i;
	int once = 0;
	static const char warn[] = "Don't know the uppercase equivalent of non ascii characters;"
	       		     "comparison might yield wrong results";
	while(*s1 && *s2)
	{
		if((*s1 & 0xffff) > 0x7f || (*s2 & 0xffff) > 0x7f)
		{
			if(!once)
			{
				once++;
				mcy_warning(warn);
			}
			i = *s1++ - *s2++;
		}
		else
			i = toupper(*s1++) - toupper(*s2++);
		if(i)
			return i;
	}

	if((*s1 & 0xffff) > 0x7f || (*s2 & 0xffff) > 0x7f)
	{
		if(!once)
			mcy_warning(warn);
		return *s1 - *s2;
	}
	else
		return 	toupper(*s1) - toupper(*s2);
}

int unistrcmp(const WCHAR *s1, const WCHAR *s2)
{
	int i;
	while(*s1 && *s2)
	{
		i = *s1++ - *s2++;
		if(i)
			return i;
	}

	return *s1 - *s2;
}

WCHAR *utf8_to_unicode( const char *src, int srclen, int *dstlen )
{
    static const char utf8_length[128] =
    {
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0x80-0x8f */
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0x90-0x9f */
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0xa0-0xaf */
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 0xb0-0xbf */
        0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 0xc0-0xcf */
        1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, /* 0xd0-0xdf */
        2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, /* 0xe0-0xef */
        3,3,3,3,3,0,0,0,0,0,0,0,0,0,0,0  /* 0xf0-0xff */
    };
    static const unsigned char utf8_mask[4] = { 0x7f, 0x1f, 0x0f, 0x07 };

    const char *srcend = src + srclen;
    int len, res;
    WCHAR *ret, *dst;

    dst = ret = xmalloc( (srclen + 1) * sizeof(WCHAR) );
    while (src < srcend)
    {
        unsigned char ch = *src++;
        if (ch < 0x80)  /* special fast case for 7-bit ASCII */
        {
            *dst++ = ch;
            continue;
        }
        len = utf8_length[ch - 0x80];
        if (len && src + len <= srcend)
        {
            res = ch & utf8_mask[len];
            switch (len)
            {
            case 3:
                if ((ch = *src ^ 0x80) >= 0x40) break;
                res = (res << 6) | ch;
                src++;
                if (res < 0x10) break;
            case 2:
                if ((ch = *src ^ 0x80) >= 0x40) break;
                res = (res << 6) | ch;
                if (res >= 0x110000 >> 6) break;
                src++;
                if (res < 0x20) break;
                if (res >= 0xd800 >> 6 && res <= 0xdfff >> 6) break;
            case 1:
                if ((ch = *src ^ 0x80) >= 0x40) break;
                res = (res << 6) | ch;
                src++;
                if (res < 0x80) break;
                if (res <= 0xffff) *dst++ = res;
                else
                {
                    res -= 0x10000;
                    *dst++ = 0xd800 | (res >> 10);
                    *dst++ = 0xdc00 | (res & 0x3ff);
                }
                continue;
            }
        }
        *dst++ = 0xfffd;
    }
    *dst = 0;
    *dstlen = dst - ret;
    return ret;
}

char *unicode_to_utf8( const WCHAR *src, int srclen, int *dstlen )
{
    char *ret, *dst;

    dst = ret = xmalloc( srclen * 3 + 1 );
    for ( ; srclen; srclen--, src++)
    {
        unsigned int ch = *src;

        if (ch < 0x80)  /* 0x00-0x7f: 1 byte */
        {
            *dst++ = ch;
            continue;
        }
        if (ch < 0x800)  /* 0x80-0x7ff: 2 bytes */
        {
            dst[1] = 0x80 | (ch & 0x3f);
            ch >>= 6;
            dst[0] = 0xc0 | ch;
            dst += 2;
            continue;
        }
        if (ch >= 0xd800 && ch <= 0xdbff && srclen > 1 && src[1] >= 0xdc00 && src[1] <= 0xdfff)
        {
            /* 0x10000-0x10ffff: 4 bytes */
            ch = 0x10000 + ((ch & 0x3ff) << 10) + (src[1] & 0x3ff);
            dst[3] = 0x80 | (ch & 0x3f);
            ch >>= 6;
            dst[2] = 0x80 | (ch & 0x3f);
            ch >>= 6;
            dst[1] = 0x80 | (ch & 0x3f);
            ch >>= 6;
            dst[0] = 0xf0 | ch;
            dst += 4;
            src++;
            srclen--;
            continue;
        }
        if (ch >= 0xd800 && ch <= 0xdfff) ch = 0xfffd;  /* invalid surrogate pair */

        /* 0x800-0xffff: 3 bytes */
        dst[2] = 0x80 | (ch & 0x3f);
        ch >>= 6;
        dst[1] = 0x80 | (ch & 0x3f);
        ch >>= 6;
        dst[0] = 0xe0 | ch;
        dst += 3;
    }
    *dst = 0;
    *dstlen = dst - ret;
    return ret;
}

#ifdef _WIN32

int is_valid_codepage(int id)
{
    return IsValidCodePage( id );
}

WCHAR *codepage_to_unicode( int codepage, const char *src, int srclen, int *dstlen )
{
    WCHAR *dst = xmalloc( (srclen + 1) * sizeof(WCHAR) );
    DWORD ret = MultiByteToWideChar( codepage, MB_ERR_INVALID_CHARS, src, srclen, dst, srclen );
    if (!ret) return NULL;
    dst[ret] = 0;
    *dstlen = ret;
    return dst;
}

#else  /* _WIN32 */

struct nls_info
{
    unsigned short  codepage;
    unsigned short  unidef;
    unsigned short  trans_unidef;
    unsigned short *cp2uni;
    unsigned short *dbcs_offsets;
};

static struct nls_info nlsinfo[128];

static void init_nls_info( struct nls_info *info, unsigned short *ptr )
{
    unsigned short hdr_size = ptr[0];

    info->codepage      = ptr[1];
    info->unidef        = ptr[4];
    info->trans_unidef  = ptr[6];
    ptr += hdr_size;
    info->cp2uni = ++ptr;
    ptr += 256;
    if (*ptr++) ptr += 256;  /* glyph table */
    info->dbcs_offsets  = *ptr ? ptr + 1 : NULL;
}

static const struct nls_info *get_nls_info( unsigned int codepage )
{
    struct stat st;
    unsigned short *data;
    char *path;
    unsigned int i;
    int fd;

    for (i = 0; i < ARRAY_SIZE(nlsinfo) && nlsinfo[i].codepage; i++)
        if (nlsinfo[i].codepage == codepage) return &nlsinfo[i];

    assert( i < ARRAY_SIZE(nlsinfo) );

    for (i = 0; nlsdirs[i]; i++)
    {
        path = strmake( "%s/c_%03u.nls", nlsdirs[i], codepage );
        if ((fd = open( path, O_RDONLY )) != -1) break;
        free( path );
    }
    if (!nlsdirs[i]) return NULL;

    fstat( fd, &st );
    data = xmalloc( st.st_size );
    if (read( fd, data, st.st_size ) != st.st_size) error( "failed to load %s\n", path );
    close( fd );
    free( path );
    init_nls_info( &nlsinfo[i], data );
    return &nlsinfo[i];
}

int is_valid_codepage(int cp)
{
    return cp == CP_UTF8 || get_nls_info( cp );
}

WCHAR *codepage_to_unicode( int codepage, const char *src, int srclen, int *dstlen )
{
    const struct nls_info *info = get_nls_info( codepage );
    unsigned int i;
    WCHAR dbch, *dst = xmalloc( (srclen + 1) * sizeof(WCHAR) );

    if (!info) error( "codepage %u not supported\n", codepage );

    if (info->dbcs_offsets)
    {
        for (i = 0; srclen; i++, srclen--, src++)
        {
            unsigned short off = info->dbcs_offsets[(unsigned char)*src];
            if (off)
            {
                if (srclen == 1) return NULL;
                dbch = (src[0] << 8) | (unsigned char)src[1];
                src++;
                srclen--;
                dst[i] = info->dbcs_offsets[off + (unsigned char)*src];
                if (dst[i] == info->unidef && dbch != info->trans_unidef) return NULL;
            }
            else
            {
                dst[i] = info->cp2uni[(unsigned char)*src];
                if (dst[i] == info->unidef && *src != info->trans_unidef) return NULL;
            }
        }
    }
    else
    {
        for (i = 0; i < srclen; i++)
        {
            dst[i] = info->cp2uni[(unsigned char)src[i]];
            if (dst[i] == info->unidef && src[i] != info->trans_unidef) return NULL;
        }
    }
    dst[i] = 0;
    *dstlen = i;
    return dst;
}

#endif  /* _WIN32 */

/*******************************************************************
 *         buffer management
 *
 * Function for writing to a memory buffer.
 */

int byte_swapped = 0;
unsigned char *output_buffer;
size_t output_buffer_pos;
size_t output_buffer_size;

static void check_output_buffer_space( size_t size )
{
    if (output_buffer_pos + size >= output_buffer_size)
    {
        output_buffer_size = max( output_buffer_size * 2, output_buffer_pos + size );
        output_buffer = xrealloc( output_buffer, output_buffer_size );
    }
}

void init_output_buffer(void)
{
    output_buffer_size = 1024;
    output_buffer_pos = 0;
    output_buffer = xmalloc( output_buffer_size );
}

void flush_output_buffer( const char *name )
{
    int fd = open( name, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0666 );
    if (fd == -1) error( "Error creating %s\n", name );
    if (write( fd, output_buffer, output_buffer_pos ) != output_buffer_pos)
        error( "Error writing to %s\n", name );
    close( fd );
    free( output_buffer );
}

void put_data( const void *data, size_t size )
{
    check_output_buffer_space( size );
    memcpy( output_buffer + output_buffer_pos, data, size );
    output_buffer_pos += size;
}

void put_byte( unsigned char val )
{
    check_output_buffer_space( 1 );
    output_buffer[output_buffer_pos++] = val;
}

void put_word( unsigned short val )
{
    if (byte_swapped) val = (val << 8) | (val >> 8);
    put_data( &val, sizeof(val) );
}

void put_dword( unsigned int val )
{
    if (byte_swapped)
        val = ((val << 24) | ((val << 8) & 0x00ff0000) | ((val >> 8) & 0x0000ff00) | (val >> 24));
    put_data( &val, sizeof(val) );
}

void align_output( unsigned int align )
{
    size_t size = align - (output_buffer_pos % align);

    if (size == align) return;
    check_output_buffer_space( size );
    memset( output_buffer + output_buffer_pos, 0, size );
    output_buffer_pos += size;
}
