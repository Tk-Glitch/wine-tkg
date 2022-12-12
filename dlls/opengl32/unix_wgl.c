/* Window-specific OpenGL functions implementation.
 *
 * Copyright (c) 1999 Lionel Ulmer
 * Copyright (c) 2005 Raphael Junqueira
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

#if 0
#pragma makedep unix
#endif

#include "config.h"

#include <stdarg.h>
#include <stdlib.h>

#include <pthread.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winreg.h"
#include "ntuser.h"

#include "unixlib.h"
#include "unix_private.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(opengl);

static pthread_mutex_t wgl_lock = PTHREAD_MUTEX_INITIALIZER;

/* handle management */

enum wgl_handle_type
{
    HANDLE_PBUFFER = 0 << 12,
    HANDLE_CONTEXT = 1 << 12,
    HANDLE_CONTEXT_V3 = 3 << 12,
    HANDLE_GLSYNC = 4 << 12,
    HANDLE_TYPE_MASK = 15 << 12,
};

struct opengl_context
{
    DWORD tid;                   /* thread that the context is current in */
    void (CALLBACK *debug_callback)( GLenum, GLenum, GLuint, GLenum, GLsizei, const GLchar *, const void * ); /* debug callback */
    const void *debug_user;      /* debug user parameter */
    GLubyte *extensions;         /* extension string */
    GLuint *disabled_exts;       /* indices of disabled extensions */
    struct wgl_context *drv_ctx; /* driver context */
};

struct wgl_handle
{
    UINT handle;
    struct opengl_funcs *funcs;
    union
    {
        struct opengl_context *context; /* for HANDLE_CONTEXT */
        struct wgl_pbuffer *pbuffer;    /* for HANDLE_PBUFFER */
        struct wgl_handle *next;        /* for free handles */
    } u;
};

#define MAX_WGL_HANDLES 1024
static struct wgl_handle wgl_handles[MAX_WGL_HANDLES];
static struct wgl_handle *next_free;
static unsigned int handle_count;

/* the current context is assumed valid and doesn't need locking */
static inline struct wgl_handle *get_current_context_ptr( TEB *teb )
{
    if (!teb->glCurrentRC) return NULL;
    return &wgl_handles[LOWORD(teb->glCurrentRC) & ~HANDLE_TYPE_MASK];
}

static inline HANDLE next_handle( struct wgl_handle *ptr, enum wgl_handle_type type )
{
    WORD generation = HIWORD( ptr->handle ) + 1;
    if (!generation) generation++;
    ptr->handle = MAKELONG( ptr - wgl_handles, generation ) | type;
    return ULongToHandle( ptr->handle );
}

static struct wgl_handle *get_handle_ptr( HANDLE handle, enum wgl_handle_type type )
{
    unsigned int index = LOWORD( handle ) & ~HANDLE_TYPE_MASK;

    if (index < handle_count && ULongToHandle(wgl_handles[index].handle) == handle)
        return &wgl_handles[index];

    RtlSetLastWin32Error( ERROR_INVALID_HANDLE );
    return NULL;
}

static HANDLE alloc_handle( enum wgl_handle_type type, struct opengl_funcs *funcs, void *user_ptr )
{
    HANDLE handle = 0;
    struct wgl_handle *ptr = NULL;

    if ((ptr = next_free))
        next_free = next_free->u.next;
    else if (handle_count < MAX_WGL_HANDLES)
        ptr = &wgl_handles[handle_count++];

    if (ptr)
    {
        ptr->funcs = funcs;
        ptr->u.context = user_ptr;
        handle = next_handle( ptr, type );
    }
    else RtlSetLastWin32Error( ERROR_NOT_ENOUGH_MEMORY );
    return handle;
}

static void free_handle_ptr( struct wgl_handle *ptr )
{
    ptr->handle |= 0xffff;
    ptr->u.next = next_free;
    ptr->funcs = NULL;
    next_free = ptr;
}

/* check if the extension is present in the list */
static BOOL has_extension( const char *list, const char *ext, size_t len )
{
    while (list)
    {
        while (*list == ' ') list++;
        if (!strncmp( list, ext, len ) && (!list[len] || list[len] == ' ')) return TRUE;
        list = strchr( list, ' ' );
    }
    return FALSE;
}

static GLubyte *filter_extensions_list( const char *extensions, const char *disabled )
{
    const char *end;
    char *p, *str;

    p = str = malloc( strlen( extensions ) + 2 );
    if (!str) return NULL;

    TRACE( "GL_EXTENSIONS:\n" );

    for (;;)
    {
        while (*extensions == ' ') extensions++;
        if (!*extensions) break;

        if (!(end = strchr( extensions, ' ' ))) end = extensions + strlen( extensions );
        memcpy( p, extensions, end - extensions );
        p[end - extensions] = 0;

        if (!has_extension( disabled, p, strlen( p ) ))
        {
            TRACE( "++ %s\n", p );
            p += end - extensions;
            *p++ = ' ';
        }
        else
        {
            TRACE( "-- %s (disabled by config)\n", p );
        }
        extensions = end;
    }
    *p = 0;
    return (GLubyte *)str;
}

static GLuint *filter_extensions_index( TEB *teb, const char *disabled )
{
    const struct opengl_funcs *funcs = teb->glTable;
    GLuint *disabled_index;
    GLint extensions_count;
    unsigned int i = 0, j;
    const char *ext;

    if (!funcs->ext.p_glGetStringi)
    {
        void **func_ptr = (void **)&funcs->ext.p_glGetStringi;
        *func_ptr = funcs->wgl.p_wglGetProcAddress( "glGetStringi" );
        if (!funcs->ext.p_glGetStringi) return NULL;
    }

    funcs->gl.p_glGetIntegerv( GL_NUM_EXTENSIONS, &extensions_count );
    disabled_index = malloc( extensions_count * sizeof(*disabled_index) );
    if (!disabled_index) return NULL;

    TRACE( "GL_EXTENSIONS:\n" );

    for (j = 0; j < extensions_count; ++j)
    {
        ext = (const char *)funcs->ext.p_glGetStringi( GL_EXTENSIONS, j );
        if (!has_extension( disabled, ext, strlen( ext ) ))
        {
            TRACE( "++ %s\n", ext );
        }
        else
        {
            TRACE( "-- %s (disabled by config)\n", ext );
            disabled_index[i++] = j;
        }
    }

    disabled_index[i] = ~0u;
    return disabled_index;
}

static inline void ascii_to_unicode( WCHAR *dst, const char *src, size_t len )
{
    while (len--) *dst++ = (unsigned char)*src++;
}

static inline UINT asciiz_to_unicode( WCHAR *dst, const char *src )
{
    WCHAR *p = dst;
    while ((*p++ = *src++));
    return (p - dst) * sizeof(WCHAR);
}

static inline void unicode_to_ascii( char *dst, const WCHAR *src, size_t len )
{
    while (len--) *dst++ = *src++;
}

static HKEY reg_open_key( HKEY root, const WCHAR *name, ULONG name_len )
{
    UNICODE_STRING nameW = { name_len, name_len, (WCHAR *)name };
    OBJECT_ATTRIBUTES attr;
    HANDLE ret;

    attr.Length = sizeof(attr);
    attr.RootDirectory = root;
    attr.ObjectName = &nameW;
    attr.Attributes = 0;
    attr.SecurityDescriptor = NULL;
    attr.SecurityQualityOfService = NULL;

    return NtOpenKeyEx( &ret, MAXIMUM_ALLOWED, &attr, 0 ) ? 0 : ret;
}

static HKEY open_hkcu_key( const char *name )
{
    WCHAR bufferW[256];
    static HKEY hkcu;

    if (!hkcu)
    {
        char buffer[256];
        DWORD_PTR sid_data[(sizeof(TOKEN_USER) + SECURITY_MAX_SID_SIZE) / sizeof(DWORD_PTR)];
        DWORD i, len = sizeof(sid_data);
        SID *sid;

        if (NtQueryInformationToken( GetCurrentThreadEffectiveToken(), TokenUser, sid_data, len, &len ))
            return 0;

        sid = ((TOKEN_USER *)sid_data)->User.Sid;
        len = sprintf( buffer, "\\Registry\\User\\S-%u-%u", sid->Revision,
                       (int)MAKELONG( MAKEWORD( sid->IdentifierAuthority.Value[5],
                                                sid->IdentifierAuthority.Value[4] ),
                                      MAKEWORD( sid->IdentifierAuthority.Value[3],
                                                sid->IdentifierAuthority.Value[2] )));
        for (i = 0; i < sid->SubAuthorityCount; i++)
            len += sprintf( buffer + len, "-%u", (int)sid->SubAuthority[i] );

        ascii_to_unicode( bufferW, buffer, len );
        hkcu = reg_open_key( NULL, bufferW, len * sizeof(WCHAR) );
    }

    return reg_open_key( hkcu, bufferW, asciiz_to_unicode( bufferW, name ) - sizeof(WCHAR) );
}

static NTSTATUS query_reg_value( HKEY hkey, const WCHAR *name, KEY_VALUE_PARTIAL_INFORMATION *info, ULONG size )
{
    unsigned int name_size = name ? lstrlenW( name ) * sizeof(WCHAR) : 0;
    UNICODE_STRING nameW = { name_size, name_size, (WCHAR *)name };

    return NtQueryValueKey( hkey, &nameW, KeyValuePartialInformation, info, size, &size );
}

/* build the extension string by filtering out the disabled extensions */
static BOOL filter_extensions( TEB * teb, const char *extensions, GLubyte **exts_list, GLuint **disabled_exts )
{
    static const char *disabled;

    if (!disabled)
    {
        char *str = NULL;
        HKEY hkey;

        /* @@ Wine registry key: HKCU\Software\Wine\OpenGL */
        if ((hkey = open_hkcu_key( "Software\\Wine\\OpenGL" )))
        {
            char buffer[4096];
            KEY_VALUE_PARTIAL_INFORMATION *value = (void *)buffer;
            static WCHAR disabled_extensionsW[] = {'D','i','s','a','b','l','e','d','E','x','t','e','n','s','i','o','n','s',0};

            if (!query_reg_value( hkey, disabled_extensionsW, value, sizeof(buffer) ))
            {
                ULONG len = value->DataLength / sizeof(WCHAR);

                unicode_to_ascii( buffer, (WCHAR *)value->Data, len );
                buffer[len] = 0;
                str = strdup( buffer );
            }
            NtClose( hkey );
        }
        if (str)
        {
            if (InterlockedCompareExchangePointer( (void **)&disabled, str, NULL )) free( str );
        }
        else disabled = "";
    }

    if (!disabled[0]) return FALSE;
    if (extensions && !*exts_list) *exts_list = filter_extensions_list( extensions, disabled );
    if (!*disabled_exts) *disabled_exts = filter_extensions_index( teb, disabled );
    return (exts_list && *exts_list) || *disabled_exts;
}

static const GLuint *disabled_extensions_index( TEB *teb )
{
    struct wgl_handle *ptr = get_current_context_ptr( teb );
    GLuint **disabled = &ptr->u.context->disabled_exts;
    if (*disabled || filter_extensions( teb, NULL, NULL, disabled )) return *disabled;
    return NULL;
}

/* Check if a GL extension is supported */
static BOOL check_extension_support( TEB *teb, const char *extension, const char *available_extensions )
{
    const struct opengl_funcs *funcs = teb->glTable;
    size_t len;

    TRACE( "Checking for extension '%s'\n", extension );

    /* We use the GetProcAddress function from the display driver to retrieve function pointers
     * for OpenGL and WGL extensions. In case of winex11.drv the OpenGL extension lookup is done
     * using glXGetProcAddress. This function is quite unreliable in the sense that its specs don't
     * require the function to return NULL when an extension isn't found. For this reason we check
     * if the OpenGL extension required for the function we are looking up is supported. */

    while ((len = strcspn( extension, " " )))
    {
        /* Check if the extension is part of the GL extension string to see if it is supported. */
        if (has_extension( available_extensions, extension, len )) return TRUE;

        /* In general an OpenGL function starts as an ARB/EXT extension and at some stage
         * it becomes part of the core OpenGL library and can be reached without the ARB/EXT
         * suffix as well. In the extension table, these functions contain GL_VERSION_major_minor.
         * Check if we are searching for a core GL function */
        if (!strncmp( extension, "GL_VERSION_", 11 ))
        {
            const GLubyte *gl_version = funcs->gl.p_glGetString( GL_VERSION );
            const char *version = extension + 11; /* Move past 'GL_VERSION_' */

            if (!gl_version)
            {
                ERR( "No OpenGL version found!\n" );
                return FALSE;
            }

            /* Compare the major/minor version numbers of the native OpenGL library and what is required by the function.
             * The gl_version string is guaranteed to have at least a major/minor and sometimes it has a release number as well. */
            if ((gl_version[0] > version[0]) || ((gl_version[0] == version[0]) && (gl_version[2] >= version[2]))) return TRUE;

            WARN( "The function requires OpenGL version '%c.%c' while your drivers only provide '%c.%c'\n",
                  version[0], version[2], gl_version[0], gl_version[2] );
        }

        if (extension[len] == ' ') len++;
        extension += len;
    }

    return FALSE;
}

static void wrap_glGetIntegerv( TEB *teb, GLenum pname, GLint *data )
{
    const struct opengl_funcs *funcs = teb->glTable;
    const GLuint *disabled;

    funcs->gl.p_glGetIntegerv( pname, data );

    if (pname == GL_NUM_EXTENSIONS && (disabled = disabled_extensions_index( teb )))
        while (*disabled++ != ~0u) (*data)--;
}

static const GLubyte *wrap_glGetString( TEB *teb, GLenum name )
{
    const struct opengl_funcs *funcs = teb->glTable;
    const GLubyte *ret;

    if ((ret = funcs->gl.p_glGetString( name )) && name == GL_EXTENSIONS)
    {
        struct wgl_handle *ptr = get_current_context_ptr( teb );
        GLubyte **extensions = &ptr->u.context->extensions;
        GLuint **disabled = &ptr->u.context->disabled_exts;
        if (*extensions || filter_extensions( teb, (const char *)ret, extensions, disabled )) return *extensions;
    }

    return ret;
}

static const GLubyte *wrap_glGetStringi( TEB *teb, GLenum name, GLuint index )
{
    const struct opengl_funcs *funcs = teb->glTable;
    const GLuint *disabled;

    if (!funcs->ext.p_glGetStringi)
    {
        void **func_ptr = (void **)&funcs->ext.p_glGetStringi;
        *func_ptr = funcs->wgl.p_wglGetProcAddress( "glGetStringi" );
    }

    if (name == GL_EXTENSIONS && (disabled = disabled_extensions_index( teb )))
        while (index >= *disabled++) index++;

    return funcs->ext.p_glGetStringi( name, index );
}

static char *build_extension_list( TEB *teb )
{
    GLint len = 0, capacity, i, extensions_count;
    char *extension, *tmp, *available_extensions;

    wrap_glGetIntegerv( teb, GL_NUM_EXTENSIONS, &extensions_count );
    capacity = 128 * extensions_count;

    if (!(available_extensions = malloc( capacity ))) return NULL;
    for (i = 0; i < extensions_count; ++i)
    {
        extension = (char *)wrap_glGetStringi( teb, GL_EXTENSIONS, i );
        capacity = max( capacity, len + strlen( extension ) + 2 );
        if (!(tmp = realloc( available_extensions, capacity ))) break;
        available_extensions = tmp;
        len += sprintf( available_extensions + len, "%s ", extension );
    }
    if (len) available_extensions[len - 1] = 0;

    return available_extensions;
}

static inline enum wgl_handle_type get_current_context_type( TEB *teb )
{
    if (!teb->glCurrentRC) return HANDLE_CONTEXT;
    return LOWORD(teb->glCurrentRC) & HANDLE_TYPE_MASK;
}

/* Check if a GL extension is supported */
static BOOL is_extension_supported( TEB *teb, const char *extension )
{
    enum wgl_handle_type type = get_current_context_type( teb );
    char *available_extensions = NULL;
    BOOL ret = FALSE;

    if (type == HANDLE_CONTEXT) available_extensions = strdup( (const char *)wrap_glGetString( teb, GL_EXTENSIONS ) );
    if (!available_extensions) available_extensions = build_extension_list( teb );

    if (!available_extensions) ERR( "No OpenGL extensions found, check if your OpenGL setup is correct!\n" );
    else ret = check_extension_support( teb, extension, available_extensions );

    free( available_extensions );
    return ret;
}

static int registry_entry_cmp( const void *a, const void *b )
{
    const struct registry_entry *entry_a = a, *entry_b = b;
    return strcmp( entry_a->name, entry_b->name );
}

static PROC wrap_wglGetProcAddress( TEB *teb, LPCSTR name )
{
    const struct registry_entry entry = {.name = name}, *found;
    struct opengl_funcs *funcs = teb->glTable;
    const void **func_ptr;

    /* Without an active context opengl32 doesn't know to what
     * driver it has to dispatch wglGetProcAddress.
     */
    if (!get_current_context_ptr( teb ))
    {
        WARN( "No active WGL context found\n" );
        return (void *)-1;
    }

    if (!(found = bsearch( &entry, extension_registry, extension_registry_size, sizeof(entry), registry_entry_cmp )))
    {
        WARN( "Function %s unknown\n", name );
        return (void *)-1;
    }

    func_ptr = (const void **)&funcs->ext + (found - extension_registry);
    if (!*func_ptr)
    {
        void *driver_func = funcs->wgl.p_wglGetProcAddress( name );

        if (!is_extension_supported( teb, found->extension ))
        {
            unsigned int i;
            static const struct { const char *name, *alt; } alternatives[] =
            {
                { "glCopyTexSubImage3DEXT", "glCopyTexSubImage3D" },     /* needed by RuneScape */
                { "glVertexAttribDivisor", "glVertexAttribDivisorARB"},  /* needed by Caffeine */
            };

            for (i = 0; i < ARRAY_SIZE(alternatives); i++)
            {
                if (strcmp( name, alternatives[i].name )) continue;
                WARN( "Extension %s required for %s not supported, trying %s\n", found->extension,
                      name, alternatives[i].alt );
                return wrap_wglGetProcAddress( teb, alternatives[i].alt );
            }

            WARN( "Extension %s required for %s not supported\n", found->extension, name );
            return (void *)-1;
        }

        if (driver_func == NULL)
        {
            WARN( "Function %s not supported by driver\n", name );
            return (void *)-1;
        }

        *func_ptr = driver_func;
    }

    /* Return the index into the extension registry instead of a useless
     * function pointer, PE side will returns its own function pointers.
     */
    return (void *)(UINT_PTR)(found - extension_registry);
}

static BOOL wrap_wglCopyContext( HGLRC hglrcSrc, HGLRC hglrcDst, UINT mask )
{
    struct wgl_handle *src, *dst;
    BOOL ret = FALSE;

    if (!(src = get_handle_ptr( hglrcSrc, HANDLE_CONTEXT ))) return FALSE;
    if ((dst = get_handle_ptr( hglrcDst, HANDLE_CONTEXT )))
    {
        if (src->funcs != dst->funcs) RtlSetLastWin32Error( ERROR_INVALID_HANDLE );
        else ret = src->funcs->wgl.p_wglCopyContext( src->u.context->drv_ctx, dst->u.context->drv_ctx, mask );
    }
    return ret;
}

static HGLRC wrap_wglCreateContext( HDC hdc )
{
    HGLRC ret = 0;
    struct wgl_context *drv_ctx;
    struct opengl_context *context;
    struct opengl_funcs *funcs = get_dc_funcs( hdc );

    if (!funcs) return 0;
    if (!(drv_ctx = funcs->wgl.p_wglCreateContext( hdc ))) return 0;
    if ((context = calloc( 1, sizeof(*context) )))
    {
        context->drv_ctx = drv_ctx;
        if (!(ret = alloc_handle( HANDLE_CONTEXT, funcs, context ))) free( context );
    }
    if (!ret) funcs->wgl.p_wglDeleteContext( drv_ctx );
    return ret;
}

static BOOL wrap_wglMakeCurrent( TEB *teb, HDC hdc, HGLRC hglrc )
{
    BOOL ret = TRUE;
    DWORD tid = HandleToULong(teb->ClientId.UniqueThread);
    struct wgl_handle *ptr, *prev = get_current_context_ptr( teb );

    if (hglrc)
    {
        if (!(ptr = get_handle_ptr( hglrc, HANDLE_CONTEXT ))) return FALSE;
        if (!ptr->u.context->tid || ptr->u.context->tid == tid)
        {
            ret = ptr->funcs->wgl.p_wglMakeCurrent( hdc, ptr->u.context->drv_ctx );
            if (ret)
            {
                if (prev) prev->u.context->tid = 0;
                ptr->u.context->tid = tid;
                teb->glReserved1[0] = hdc;
                teb->glReserved1[1] = hdc;
                teb->glCurrentRC = hglrc;
                teb->glTable = ptr->funcs;
            }
        }
        else
        {
            RtlSetLastWin32Error( ERROR_BUSY );
            ret = FALSE;
        }
    }
    else if (prev)
    {
        if (!prev->funcs->wgl.p_wglMakeCurrent( 0, NULL )) return FALSE;
        prev->u.context->tid = 0;
        teb->glCurrentRC = 0;
        teb->glTable = &null_opengl_funcs;
    }
    else if (!hdc)
    {
        RtlSetLastWin32Error( ERROR_INVALID_HANDLE );
        ret = FALSE;
    }
    return ret;
}

static BOOL wrap_wglDeleteContext( TEB *teb, HGLRC hglrc )
{
    struct wgl_handle *ptr;
    DWORD tid = HandleToULong(teb->ClientId.UniqueThread);

    if (!(ptr = get_handle_ptr( hglrc, HANDLE_CONTEXT ))) return FALSE;
    if (ptr->u.context->tid && ptr->u.context->tid != tid)
    {
        RtlSetLastWin32Error( ERROR_BUSY );
        return FALSE;
    }
    if (hglrc == teb->glCurrentRC) wrap_wglMakeCurrent( teb, 0, 0 );
    ptr->funcs->wgl.p_wglDeleteContext( ptr->u.context->drv_ctx );
    free( ptr->u.context->disabled_exts );
    free( ptr->u.context->extensions );
    free( ptr->u.context );
    free_handle_ptr( ptr );
    return TRUE;
}

static BOOL wrap_wglShareLists( HGLRC hglrcSrc, HGLRC hglrcDst )
{
    BOOL ret = FALSE;
    struct wgl_handle *src, *dst;

    if (!(src = get_handle_ptr( hglrcSrc, HANDLE_CONTEXT ))) return FALSE;
    if ((dst = get_handle_ptr( hglrcDst, HANDLE_CONTEXT )))
    {
        if (src->funcs != dst->funcs) RtlSetLastWin32Error( ERROR_INVALID_HANDLE );
        else ret = src->funcs->wgl.p_wglShareLists( src->u.context->drv_ctx, dst->u.context->drv_ctx );
    }
    return ret;
}

static BOOL wrap_wglBindTexImageARB( HPBUFFERARB handle, int buffer )
{
    struct wgl_handle *ptr;
    if (!(ptr = get_handle_ptr( handle, HANDLE_PBUFFER ))) return FALSE;
    return ptr->funcs->ext.p_wglBindTexImageARB( ptr->u.pbuffer, buffer );
}

static HGLRC wrap_wglCreateContextAttribsARB( HDC hdc, HGLRC share, const int *attribs )
{
    HGLRC ret = 0;
    struct wgl_context *drv_ctx;
    struct wgl_handle *share_ptr = NULL;
    struct opengl_context *context;
    struct opengl_funcs *funcs = get_dc_funcs( hdc );

    if (!funcs)
    {
        RtlSetLastWin32Error( ERROR_DC_NOT_FOUND );
        return 0;
    }
    if (!funcs->ext.p_wglCreateContextAttribsARB) return 0;
    if (share && !(share_ptr = get_handle_ptr( share, HANDLE_CONTEXT )))
    {
        RtlSetLastWin32Error( ERROR_INVALID_OPERATION );
        return 0;
    }
    if ((drv_ctx = funcs->ext.p_wglCreateContextAttribsARB( hdc, share_ptr ? share_ptr->u.context->drv_ctx : NULL, attribs )))
    {
        if ((context = calloc( 1, sizeof(*context) )))
        {
            enum wgl_handle_type type = HANDLE_CONTEXT;

            if (attribs)
            {
                while (*attribs)
                {
                    if (attribs[0] == WGL_CONTEXT_MAJOR_VERSION_ARB)
                    {
                        if (attribs[1] >= 3) type = HANDLE_CONTEXT_V3;
                        break;
                    }
                    attribs += 2;
                }
            }

            context->drv_ctx = drv_ctx;
            if (!(ret = alloc_handle( type, funcs, context ))) free( context );
        }
        if (!ret) funcs->wgl.p_wglDeleteContext( drv_ctx );
    }

    return ret;
}

static HPBUFFERARB wrap_wglCreatePbufferARB( HDC hdc, int format, int width, int height, const int *attribs )
{
    HPBUFFERARB ret;
    struct wgl_pbuffer *pbuffer;
    struct opengl_funcs *funcs = get_dc_funcs( hdc );

    if (!funcs || !funcs->ext.p_wglCreatePbufferARB) return 0;
    if (!(pbuffer = funcs->ext.p_wglCreatePbufferARB( hdc, format, width, height, attribs ))) return 0;
    ret = alloc_handle( HANDLE_PBUFFER, funcs, pbuffer );
    if (!ret) funcs->ext.p_wglDestroyPbufferARB( pbuffer );
    return ret;
}

static BOOL wrap_wglDestroyPbufferARB( HPBUFFERARB handle )
{
    struct wgl_handle *ptr;

    if (!(ptr = get_handle_ptr( handle, HANDLE_PBUFFER ))) return FALSE;
    ptr->funcs->ext.p_wglDestroyPbufferARB( ptr->u.pbuffer );
    free_handle_ptr( ptr );
    return TRUE;
}

static HDC wrap_wglGetPbufferDCARB( HPBUFFERARB handle )
{
    struct wgl_handle *ptr;
    if (!(ptr = get_handle_ptr( handle, HANDLE_PBUFFER ))) return 0;
    return ptr->funcs->ext.p_wglGetPbufferDCARB( ptr->u.pbuffer );
}

static BOOL wrap_wglMakeContextCurrentARB( TEB *teb, HDC draw_hdc, HDC read_hdc, HGLRC hglrc )
{
    BOOL ret = TRUE;
    DWORD tid = HandleToULong(teb->ClientId.UniqueThread);
    struct wgl_handle *ptr, *prev = get_current_context_ptr( teb );

    if (hglrc)
    {
        if (!(ptr = get_handle_ptr( hglrc, HANDLE_CONTEXT ))) return FALSE;
        if (!ptr->u.context->tid || ptr->u.context->tid == tid)
        {
            ret = (ptr->funcs->ext.p_wglMakeContextCurrentARB &&
                   ptr->funcs->ext.p_wglMakeContextCurrentARB( draw_hdc, read_hdc, ptr->u.context->drv_ctx ));
            if (ret)
            {
                if (prev) prev->u.context->tid = 0;
                ptr->u.context->tid = tid;
                teb->glReserved1[0] = draw_hdc;
                teb->glReserved1[1] = read_hdc;
                teb->glCurrentRC = hglrc;
                teb->glTable = ptr->funcs;
            }
        }
        else
        {
            RtlSetLastWin32Error( ERROR_BUSY );
            ret = FALSE;
        }
    }
    else if (prev)
    {
        if (!prev->funcs->wgl.p_wglMakeCurrent( 0, NULL )) return FALSE;
        prev->u.context->tid = 0;
        teb->glCurrentRC = 0;
        teb->glTable = &null_opengl_funcs;
    }
    return ret;
}

static BOOL wrap_wglQueryPbufferARB( HPBUFFERARB handle, int attrib, int *value )
{
    struct wgl_handle *ptr;
    if (!(ptr = get_handle_ptr( handle, HANDLE_PBUFFER ))) return FALSE;
    return ptr->funcs->ext.p_wglQueryPbufferARB( ptr->u.pbuffer, attrib, value );
}

static int wrap_wglReleasePbufferDCARB( HPBUFFERARB handle, HDC hdc )
{
    struct wgl_handle *ptr;
    if (!(ptr = get_handle_ptr( handle, HANDLE_PBUFFER ))) return FALSE;
    return ptr->funcs->ext.p_wglReleasePbufferDCARB( ptr->u.pbuffer, hdc );
}

static BOOL wrap_wglReleaseTexImageARB( HPBUFFERARB handle, int buffer )
{
    struct wgl_handle *ptr;
    if (!(ptr = get_handle_ptr( handle, HANDLE_PBUFFER ))) return FALSE;
    return ptr->funcs->ext.p_wglReleaseTexImageARB( ptr->u.pbuffer, buffer );
}

static BOOL wrap_wglSetPbufferAttribARB( HPBUFFERARB handle, const int *attribs )
{
    struct wgl_handle *ptr;
    if (!(ptr = get_handle_ptr( handle, HANDLE_PBUFFER ))) return FALSE;
    return ptr->funcs->ext.p_wglSetPbufferAttribARB( ptr->u.pbuffer, attribs );
}

static void gl_debug_message_callback( GLenum source, GLenum type, GLuint id, GLenum severity,
                                       GLsizei length, const GLchar *message, const void *userParam )
{
    struct wine_gl_debug_message_params params =
    {
        .source = source,
        .type = type,
        .id = id,
        .severity = severity,
        .length = length,
        .message = message,
    };
    void *ret_ptr;
    ULONG ret_len;

    struct wgl_handle *ptr = (struct wgl_handle *)userParam;
    if (!(params.user_callback = ptr->u.context->debug_callback)) return;
    params.user_data = ptr->u.context->debug_user;

    KeUserModeCallback( NtUserCallOpenGLDebugMessageCallback, &params, sizeof(params),
                        &ret_ptr, &ret_len );
}

static void wrap_glDebugMessageCallback( TEB *teb, GLDEBUGPROC callback, const void *userParam )
{
    struct wgl_handle *ptr = get_current_context_ptr( teb );
    const struct opengl_funcs *funcs = teb->glTable;

    if (!funcs->ext.p_glDebugMessageCallback) return;

    ptr->u.context->debug_callback = callback;
    ptr->u.context->debug_user     = userParam;
    funcs->ext.p_glDebugMessageCallback( gl_debug_message_callback, ptr );
}

static void wrap_glDebugMessageCallbackAMD( TEB *teb, GLDEBUGPROCAMD callback, void *userParam )
{
    struct wgl_handle *ptr = get_current_context_ptr( teb );
    const struct opengl_funcs *funcs = teb->glTable;

    if (!funcs->ext.p_glDebugMessageCallbackAMD) return;

    ptr->u.context->debug_callback = callback;
    ptr->u.context->debug_user     = userParam;
    funcs->ext.p_glDebugMessageCallbackAMD( gl_debug_message_callback, ptr );
}

static void wrap_glDebugMessageCallbackARB( TEB *teb, GLDEBUGPROCARB callback, const void *userParam )
{
    struct wgl_handle *ptr = get_current_context_ptr( teb );
    const struct opengl_funcs *funcs = teb->glTable;

    if (!funcs->ext.p_glDebugMessageCallbackARB) return;

    ptr->u.context->debug_callback = callback;
    ptr->u.context->debug_user     = userParam;
    funcs->ext.p_glDebugMessageCallbackARB( gl_debug_message_callback, ptr );
}

NTSTATUS wgl_wglCopyContext( void *args )
{
    struct wglCopyContext_params *params = args;
    pthread_mutex_lock( &wgl_lock );
    params->ret = wrap_wglCopyContext( params->hglrcSrc, params->hglrcDst, params->mask );
    pthread_mutex_unlock( &wgl_lock );
    return STATUS_SUCCESS;
}

NTSTATUS wgl_wglCreateContext( void *args )
{
    struct wglCreateContext_params *params = args;
    pthread_mutex_lock( &wgl_lock );
    params->ret = wrap_wglCreateContext( params->hDc );
    pthread_mutex_unlock( &wgl_lock );
    return STATUS_SUCCESS;
}

NTSTATUS wgl_wglDeleteContext( void *args )
{
    struct wglDeleteContext_params *params = args;
    pthread_mutex_lock( &wgl_lock );
    params->ret = wrap_wglDeleteContext( params->teb, params->oldContext );
    pthread_mutex_unlock( &wgl_lock );
    return STATUS_SUCCESS;
}

NTSTATUS wgl_wglGetProcAddress( void *args )
{
    struct wglGetProcAddress_params *params = args;
    params->ret = wrap_wglGetProcAddress( params->teb, params->lpszProc );
    return STATUS_SUCCESS;
}

NTSTATUS wgl_wglMakeCurrent( void *args )
{
    struct wglMakeCurrent_params *params = args;
    if (params->newContext) pthread_mutex_lock( &wgl_lock );
    params->ret = wrap_wglMakeCurrent( params->teb, params->hDc, params->newContext );
    if (params->newContext) pthread_mutex_unlock( &wgl_lock );
    return STATUS_SUCCESS;
}

NTSTATUS wgl_wglShareLists( void *args )
{
    struct wglShareLists_params *params = args;
    pthread_mutex_lock( &wgl_lock );
    params->ret = wrap_wglShareLists( params->hrcSrvShare, params->hrcSrvSource );
    pthread_mutex_unlock( &wgl_lock );
    return STATUS_SUCCESS;
}

NTSTATUS gl_glGetIntegerv( void *args )
{
    struct glGetIntegerv_params *params = args;
    wrap_glGetIntegerv( params->teb, params->pname, params->data );
    return STATUS_SUCCESS;
}

NTSTATUS gl_glGetString( void *args )
{
    struct glGetString_params *params = args;
    params->ret = wrap_glGetString( params->teb, params->name );
    return STATUS_SUCCESS;
}

NTSTATUS ext_glDebugMessageCallback( void *args )
{
    struct glDebugMessageCallback_params *params = args;
    wrap_glDebugMessageCallback( params->teb, params->callback, params->userParam );
    return STATUS_SUCCESS;
}

NTSTATUS ext_glDebugMessageCallbackAMD( void *args )
{
    struct glDebugMessageCallbackAMD_params *params = args;
    wrap_glDebugMessageCallbackAMD( params->teb, params->callback, params->userParam );
    return STATUS_SUCCESS;
}

NTSTATUS ext_glDebugMessageCallbackARB( void *args )
{
    struct glDebugMessageCallbackARB_params *params = args;
    wrap_glDebugMessageCallbackARB( params->teb, params->callback, params->userParam );
    return STATUS_SUCCESS;
}

NTSTATUS ext_glGetStringi( void *args )
{
    struct glGetStringi_params *params = args;
    params->ret = wrap_glGetStringi( params->teb, params->name, params->index );
    return STATUS_SUCCESS;
}

NTSTATUS ext_wglBindTexImageARB( void *args )
{
    struct wglBindTexImageARB_params *params = args;
    pthread_mutex_lock( &wgl_lock );
    params->ret = wrap_wglBindTexImageARB( params->hPbuffer, params->iBuffer );
    pthread_mutex_unlock( &wgl_lock );
    return STATUS_SUCCESS;
}

NTSTATUS ext_wglCreateContextAttribsARB( void *args )
{
    struct wglCreateContextAttribsARB_params *params = args;
    pthread_mutex_lock( &wgl_lock );
    params->ret = wrap_wglCreateContextAttribsARB( params->hDC, params->hShareContext, params->attribList );
    pthread_mutex_unlock( &wgl_lock );
    return STATUS_SUCCESS;
}

NTSTATUS ext_wglCreatePbufferARB( void *args )
{
    struct wglCreatePbufferARB_params *params = args;
    pthread_mutex_lock( &wgl_lock );
    params->ret = wrap_wglCreatePbufferARB( params->hDC, params->iPixelFormat, params->iWidth, params->iHeight, params->piAttribList );
    pthread_mutex_unlock( &wgl_lock );
    return STATUS_SUCCESS;
}

NTSTATUS ext_wglDestroyPbufferARB( void *args )
{
    struct wglDestroyPbufferARB_params *params = args;
    pthread_mutex_lock( &wgl_lock );
    params->ret = wrap_wglDestroyPbufferARB( params->hPbuffer );
    pthread_mutex_unlock( &wgl_lock );
    return STATUS_SUCCESS;
}

NTSTATUS ext_wglGetPbufferDCARB( void *args )
{
    struct wglGetPbufferDCARB_params *params = args;
    pthread_mutex_lock( &wgl_lock );
    params->ret = wrap_wglGetPbufferDCARB( params->hPbuffer );
    pthread_mutex_unlock( &wgl_lock );
    return STATUS_SUCCESS;
}

NTSTATUS ext_wglMakeContextCurrentARB( void *args )
{
    struct wglMakeContextCurrentARB_params *params = args;
    if (params->hglrc) pthread_mutex_lock( &wgl_lock );
    params->ret = wrap_wglMakeContextCurrentARB( params->teb, params->hDrawDC, params->hReadDC, params->hglrc );
    if (params->hglrc) pthread_mutex_unlock( &wgl_lock );
    return STATUS_SUCCESS;
}

NTSTATUS ext_wglQueryPbufferARB( void *args )
{
    struct wglQueryPbufferARB_params *params = args;
    pthread_mutex_lock( &wgl_lock );
    params->ret = wrap_wglQueryPbufferARB( params->hPbuffer, params->iAttribute, params->piValue );
    pthread_mutex_unlock( &wgl_lock );
    return STATUS_SUCCESS;
}

NTSTATUS ext_wglReleasePbufferDCARB( void *args )
{
    struct wglReleasePbufferDCARB_params *params = args;
    pthread_mutex_lock( &wgl_lock );
    params->ret = wrap_wglReleasePbufferDCARB( params->hPbuffer, params->hDC );
    pthread_mutex_unlock( &wgl_lock );
    return STATUS_SUCCESS;
}

NTSTATUS ext_wglReleaseTexImageARB( void *args )
{
    struct wglReleaseTexImageARB_params *params = args;
    pthread_mutex_lock( &wgl_lock );
    params->ret = wrap_wglReleaseTexImageARB( params->hPbuffer, params->iBuffer );
    pthread_mutex_unlock( &wgl_lock );
    return STATUS_SUCCESS;
}

NTSTATUS ext_wglSetPbufferAttribARB( void *args )
{
    struct wglSetPbufferAttribARB_params *params = args;
    pthread_mutex_lock( &wgl_lock );
    params->ret = wrap_wglSetPbufferAttribARB( params->hPbuffer, params->piAttribList );
    pthread_mutex_unlock( &wgl_lock );
    return STATUS_SUCCESS;
}

NTSTATUS thread_attach( void *args )
{
    TEB *teb = args;
    teb->glTable = &null_opengl_funcs;
    return STATUS_SUCCESS;
}

NTSTATUS process_detach( void *args )
{
    return STATUS_SUCCESS;
}

#ifdef _WIN64

typedef ULONG PTR32;

extern NTSTATUS ext_glClientWaitSync( void *args ) DECLSPEC_HIDDEN;
extern NTSTATUS ext_glDeleteSync( void *args ) DECLSPEC_HIDDEN;
extern NTSTATUS ext_glFenceSync( void *args ) DECLSPEC_HIDDEN;
extern NTSTATUS ext_glGetBufferPointerv( void *args ) DECLSPEC_HIDDEN;
extern NTSTATUS ext_glGetBufferPointervARB( void *args ) DECLSPEC_HIDDEN;
extern NTSTATUS ext_glGetNamedBufferPointerv( void *args ) DECLSPEC_HIDDEN;
extern NTSTATUS ext_glGetNamedBufferPointervEXT( void *args ) DECLSPEC_HIDDEN;
extern NTSTATUS ext_glGetSynciv( void *args ) DECLSPEC_HIDDEN;
extern NTSTATUS ext_glIsSync( void *args ) DECLSPEC_HIDDEN;
extern NTSTATUS ext_glMapBuffer( void *args ) DECLSPEC_HIDDEN;

extern NTSTATUS ext_glUnmapBuffer( void *args ) DECLSPEC_HIDDEN;
extern NTSTATUS ext_glUnmapBufferARB( void *args ) DECLSPEC_HIDDEN;
extern NTSTATUS ext_glUnmapNamedBuffer( void *args ) DECLSPEC_HIDDEN;
extern NTSTATUS ext_glUnmapNamedBufferEXT( void *args ) DECLSPEC_HIDDEN;

extern NTSTATUS ext_glMapBufferARB( void *args ) DECLSPEC_HIDDEN;
extern NTSTATUS ext_glMapBufferRange( void *args ) DECLSPEC_HIDDEN;
extern NTSTATUS ext_glMapNamedBuffer( void *args ) DECLSPEC_HIDDEN;
extern NTSTATUS ext_glMapNamedBufferEXT( void *args ) DECLSPEC_HIDDEN;
extern NTSTATUS ext_glMapNamedBufferRange( void *args ) DECLSPEC_HIDDEN;
extern NTSTATUS ext_glMapNamedBufferRangeEXT( void *args ) DECLSPEC_HIDDEN;
extern NTSTATUS ext_glPathGlyphIndexRangeNV( void *args ) DECLSPEC_HIDDEN;
extern NTSTATUS ext_glWaitSync( void *args ) DECLSPEC_HIDDEN;
extern NTSTATUS ext_wglGetExtensionsStringARB( void *args ) DECLSPEC_HIDDEN;
extern NTSTATUS ext_wglGetExtensionsStringEXT( void *args ) DECLSPEC_HIDDEN;
extern NTSTATUS ext_wglQueryCurrentRendererStringWINE( void *args ) DECLSPEC_HIDDEN;
extern NTSTATUS ext_wglQueryRendererStringWINE( void *args ) DECLSPEC_HIDDEN;

struct wow64_string_entry
{
    const char *str;
    PTR32 wow64_str;
};
static struct wow64_string_entry *wow64_strings;
static SIZE_T wow64_strings_count;

static PTR32 find_wow64_string( const char *str, PTR32 wow64_str )
{
    void *tmp;
    SIZE_T i;

    pthread_mutex_lock( &wgl_lock );

    for (i = 0; i < wow64_strings_count; i++) if (wow64_strings[i].str == str) break;
    if (i == wow64_strings_count && (tmp = realloc( wow64_strings, (i + 1) * sizeof(*wow64_strings) )))
    {
        wow64_strings = tmp;
        wow64_strings[i].str = str;
        wow64_strings[i].wow64_str = 0;
        wow64_strings_count += 1;
    }

    if (i == wow64_strings_count) ERR( "Failed to allocate memory for wow64 strings\n" );
    else if (wow64_strings[i].wow64_str) wow64_str = wow64_strings[i].wow64_str;
    else if (wow64_str)
    {
        strcpy( UlongToPtr(wow64_str), (char *)str );
        wow64_strings[i].wow64_str = wow64_str;
    }

    pthread_mutex_unlock( &wgl_lock );

    return wow64_str;
}

static inline void update_teb32_context( TEB *teb )
{
    void *teb32;

    if (!teb->WowTebOffset) return;
    teb32 = (char *)teb + teb->WowTebOffset;

    ((TEB32 *)teb32)->glCurrentRC = (UINT_PTR)teb->glCurrentRC;
    ((TEB32 *)teb32)->glReserved1[0] = (UINT_PTR)teb->glReserved1[0];
    ((TEB32 *)teb32)->glReserved1[1] = (UINT_PTR)teb->glReserved1[1];
}

NTSTATUS wow64_wgl_wglCreateContext( void *args )
{
    struct
    {
        PTR32 teb;
        PTR32 hDc;
        PTR32 ret;
    } *params32 = args;
    struct wglCreateContext_params params =
    {
        .teb = get_teb64(params32->teb),
        .hDc = ULongToPtr(params32->hDc),
    };
    NTSTATUS status;
    if ((status = wgl_wglCreateContext( &params ))) return status;
    params32->ret = (UINT_PTR)params.ret;
    return STATUS_SUCCESS;
}

NTSTATUS wow64_ext_wglCreateContextAttribsARB( void *args )
{
    struct
    {
        PTR32 teb;
        PTR32 hDC;
        PTR32 hShareContext;
        PTR32 attribList;
        PTR32 ret;
    } *params32 = args;
    struct wglCreateContextAttribsARB_params params =
    {
        .teb = get_teb64(params32->teb),
        .hDC = ULongToPtr(params32->hDC),
        .hShareContext = ULongToPtr(params32->hShareContext),
        .attribList = ULongToPtr(params32->attribList),
    };
    NTSTATUS status;
    if ((status = ext_wglCreateContextAttribsARB( &params ))) return status;
    params32->ret = (UINT_PTR)params.ret;
    return STATUS_SUCCESS;
}

NTSTATUS wow64_ext_wglCreatePbufferARB( void *args )
{
    struct
    {
        PTR32 teb;
        PTR32 hDC;
        GLint iPixelFormat;
        GLint iWidth;
        GLint iHeight;
        PTR32 piAttribList;
        PTR32 ret;
    } *params32 = args;
    struct wglCreatePbufferARB_params params =
    {
        .teb = get_teb64(params32->teb),
        .hDC = ULongToPtr(params32->hDC),
        .iPixelFormat = params32->iPixelFormat,
        .iWidth = params32->iWidth,
        .iHeight = params32->iHeight,
        .piAttribList = ULongToPtr(params32->piAttribList),
    };
    NTSTATUS status;
    if ((status = ext_wglCreatePbufferARB( &params ))) return status;
    params32->ret = (UINT_PTR)params.ret;
    return STATUS_SUCCESS;
}

NTSTATUS wow64_wgl_wglDeleteContext( void *args )
{
    struct
    {
        PTR32 teb;
        PTR32 oldContext;
        BOOL ret;
    } *params32 = args;
    struct wglDeleteContext_params params =
    {
        .teb = get_teb64(params32->teb),
        .oldContext = ULongToPtr(params32->oldContext),
    };
    NTSTATUS status;
    if (!(status = wgl_wglDeleteContext( &params ))) update_teb32_context( params.teb );
    params32->ret = params.ret;
    return status;
}

NTSTATUS wow64_wgl_wglMakeCurrent( void *args )
{
    struct
    {
        PTR32 teb;
        PTR32 hDc;
        PTR32 newContext;
        BOOL ret;
    } *params32 = args;
    struct wglMakeCurrent_params params =
    {
        .teb = get_teb64(params32->teb),
        .hDc = ULongToPtr(params32->hDc),
        .newContext = ULongToPtr(params32->newContext),
    };
    NTSTATUS status;
    if (!(status = wgl_wglMakeCurrent( &params ))) update_teb32_context( params.teb );
    params32->ret = params.ret;
    return status;
}

NTSTATUS wow64_ext_wglMakeContextCurrentARB( void *args )
{
    struct
    {
        PTR32 teb;
        PTR32 hDrawDC;
        PTR32 hReadDC;
        PTR32 hglrc;
        BOOL ret;
    } *params32 = args;
    struct wglMakeContextCurrentARB_params params =
    {
        .teb = get_teb64(params32->teb),
        .hDrawDC = ULongToPtr(params32->hDrawDC),
        .hReadDC = ULongToPtr(params32->hReadDC),
        .hglrc = ULongToPtr(params32->hglrc),
    };
    NTSTATUS status;
    if (!(status = ext_wglMakeContextCurrentARB( &params ))) update_teb32_context( params.teb );
    params32->ret = params.ret;
    return status;
}

NTSTATUS wow64_ext_wglGetPbufferDCARB( void *args )
{
    struct
    {
        PTR32 teb;
        PTR32 hPbuffer;
        PTR32 ret;
    } *params32 = args;
    struct wglGetPbufferDCARB_params params =
    {
        .teb = get_teb64(params32->teb),
        .hPbuffer = (HPBUFFERARB)ULongToPtr(params32->hPbuffer),
    };
    NTSTATUS status;
    if ((status = ext_wglGetPbufferDCARB( &params ))) return status;
    params32->ret = (UINT_PTR)params.ret;
    return STATUS_SUCCESS;
}

NTSTATUS wow64_wgl_wglGetProcAddress( void *args )
{
    struct
    {
        PTR32 teb;
        PTR32 lpszProc;
        PTR32 ret;
    } *params32 = args;
    struct wglGetProcAddress_params params =
    {
        .teb = get_teb64(params32->teb),
        .lpszProc = ULongToPtr(params32->lpszProc),
    };
    NTSTATUS status;
    if ((status = wgl_wglGetProcAddress( &params ))) return status;
    params32->ret = (UINT_PTR)params.ret;
    return STATUS_SUCCESS;
}

NTSTATUS wow64_gl_glGetString( void *args )
{
    struct
    {
        PTR32 teb;
        GLenum name;
        PTR32 ret;
    } *params32 = args;
    struct glGetString_params params =
    {
        .teb = get_teb64(params32->teb),
        .name = params32->name,
    };
    NTSTATUS status;

    if ((status = gl_glGetString( &params ))) return status;

    if (!(params32->ret = find_wow64_string( (char *)params.ret, params32->ret )))
    {
        params32->ret = strlen( (char *)params.ret ) + 1;
        return STATUS_BUFFER_TOO_SMALL;
    }

    return STATUS_SUCCESS;
}

NTSTATUS wow64_ext_glGetStringi( void *args )
{
    struct
    {
        PTR32 teb;
        GLenum name;
        GLuint index;
        PTR32 ret;
    } *params32 = args;
    struct glGetStringi_params params =
    {
        .teb = get_teb64(params32->teb),
        .name = params32->name,
        .index = params32->index,
    };
    NTSTATUS status;

    if ((status = ext_glGetStringi( &params ))) return status;

    if (!(params32->ret = find_wow64_string( (char *)params.ret, params32->ret )))
    {
        params32->ret = strlen( (char *)params.ret ) + 1;
        return STATUS_BUFFER_TOO_SMALL;
    }

    return STATUS_SUCCESS;
}

NTSTATUS wow64_ext_glPathGlyphIndexRangeNV( void *args )
{
    struct
    {
        PTR32 teb;
        GLenum fontTarget;
        PTR32 fontName;
        GLbitfield fontStyle;
        GLuint pathParameterTemplate;
        GLfloat emScale;
        GLuint baseAndCount[2];
        GLenum ret;
    } *params32 = args;
    struct glPathGlyphIndexRangeNV_params params =
    {
        .teb = get_teb64(params32->teb),
        .fontTarget = params32->fontTarget,
        .fontName = ULongToPtr(params32->fontName),
        .fontStyle = params32->fontStyle,
        .pathParameterTemplate = params32->pathParameterTemplate,
        .emScale = params32->emScale,
        .baseAndCount = {params32->baseAndCount[0], params32->baseAndCount[1]},
    };
    NTSTATUS status;
    if ((status = ext_glPathGlyphIndexRangeNV( &params ))) return status;
    params32->ret = params.ret;
    return status;
}

NTSTATUS wow64_ext_wglGetExtensionsStringARB( void *args )
{
    struct
    {
        PTR32 teb;
        PTR32 hdc;
        PTR32 ret;
    } *params32 = args;
    struct wglGetExtensionsStringARB_params params =
    {
        .teb = get_teb64(params32->teb),
        .hdc = ULongToPtr(params32->hdc),
    };
    NTSTATUS status;

    if ((status = ext_wglGetExtensionsStringARB( &params ))) return status;

    if (!(params32->ret = find_wow64_string( params.ret, params32->ret )))
    {
        params32->ret = strlen( params.ret ) + 1;
        return STATUS_BUFFER_TOO_SMALL;
    }

    return STATUS_SUCCESS;
}

NTSTATUS wow64_ext_wglGetExtensionsStringEXT( void *args )
{
    struct
    {
        PTR32 teb;
        PTR32 ret;
    } *params32 = args;
    struct wglGetExtensionsStringEXT_params params =
    {
        .teb = get_teb64(params32->teb),
    };
    NTSTATUS status;

    if ((status = ext_wglGetExtensionsStringEXT( &params ))) return status;

    if (!(params32->ret = find_wow64_string( params.ret, params32->ret )))
    {
        params32->ret = strlen( params.ret ) + 1;
        return STATUS_BUFFER_TOO_SMALL;
    }

    return STATUS_SUCCESS;
}

NTSTATUS wow64_ext_wglQueryCurrentRendererStringWINE( void *args )
{
    struct
    {
        PTR32 teb;
        GLenum attribute;
        PTR32 ret;
    } *params32 = args;
    struct wglQueryCurrentRendererStringWINE_params params =
    {
        .teb = get_teb64(params32->teb),
        .attribute = params32->attribute,
    };
    NTSTATUS status;

    if ((status = ext_wglQueryCurrentRendererStringWINE( &params ))) return status;

    if (!(params32->ret = find_wow64_string( params.ret, params32->ret )))
    {
        params32->ret = strlen( params.ret ) + 1;
        return STATUS_BUFFER_TOO_SMALL;
    }

    return STATUS_SUCCESS;
}

NTSTATUS wow64_ext_wglQueryRendererStringWINE( void *args )
{
    struct
    {
        PTR32 teb;
        PTR32 dc;
        GLint renderer;
        GLenum attribute;
        PTR32 ret;
    } *params32 = args;
    struct wglQueryRendererStringWINE_params params =
    {
        .teb = get_teb64(params32->teb),
        .dc = ULongToPtr(params32->dc),
        .renderer = params32->renderer,
        .attribute = params32->attribute,
    };
    NTSTATUS status;

    if ((status = ext_wglQueryRendererStringWINE( &params ))) return status;

    if (!(params32->ret = find_wow64_string( params.ret, params32->ret )))
    {
        params32->ret = strlen( params.ret ) + 1;
        return STATUS_BUFFER_TOO_SMALL;
    }

    return STATUS_SUCCESS;
}

NTSTATUS wow64_ext_glClientWaitSync( void *args )
{
    struct wgl_handle *handle;
    struct
    {
        PTR32 teb;
        PTR32 sync;
        GLbitfield flags;
        GLuint64 timeout;
        GLenum ret;
    } *params32 = args;
    NTSTATUS status;

    pthread_mutex_lock( &wgl_lock );

    if (!(handle = get_handle_ptr( ULongToPtr(params32->sync), HANDLE_GLSYNC )))
        status = STATUS_INVALID_HANDLE;
    else
    {
        struct glClientWaitSync_params params =
        {
            .teb = get_teb64(params32->teb),
            .sync = (GLsync)handle->u.context,
            .flags = params32->flags,
            .timeout = params32->timeout,
        };
        status = ext_glClientWaitSync( &params );
        params32->ret = params.ret;
    }

    pthread_mutex_unlock( &wgl_lock );
    return status;
}

NTSTATUS wow64_ext_glDeleteSync( void *args )
{
    struct wgl_handle *handle;
    struct
    {
        PTR32 teb;
        PTR32 sync;
    } *params32 = args;
    NTSTATUS status;

    pthread_mutex_lock( &wgl_lock );

    if (!(handle = get_handle_ptr( ULongToPtr(params32->sync), HANDLE_GLSYNC )))
        status = STATUS_INVALID_HANDLE;
    else
    {
        struct glDeleteSync_params params =
        {
            .teb = get_teb64(params32->teb),
            .sync = (GLsync)handle->u.context,
        };
        status = ext_glDeleteSync( &params );
        free_handle_ptr( handle );
    }

    pthread_mutex_unlock( &wgl_lock );
    return status;
}

NTSTATUS wow64_ext_glFenceSync( void *args )
{
    struct
    {
        PTR32 teb;
        GLenum condition;
        GLbitfield flags;
        PTR32 ret;
    } *params32 = args;
    struct glFenceSync_params params =
    {
        .teb = get_teb64(params32->teb),
        .condition = params32->condition,
        .flags = params32->flags,
    };
    NTSTATUS status;

    if ((status = ext_glFenceSync( &params ))) return status;

    pthread_mutex_lock( &wgl_lock );

    if (!(params32->ret = (UINT_PTR)alloc_handle( HANDLE_GLSYNC, NULL, params.ret )))
    {
        struct glDeleteSync_params delete_params =
        {
            .teb = params.teb,
            .sync = params.ret,
        };

        ext_glDeleteSync( &delete_params );
        status = STATUS_NO_MEMORY;
    }

    pthread_mutex_unlock( &wgl_lock );
    return status;
}

NTSTATUS wow64_ext_glGetSynciv( void *args )
{
    struct wgl_handle *handle;
    struct
    {
        PTR32 teb;
        PTR32 sync;
        GLenum pname;
        GLsizei count;
        PTR32 length;
        PTR32 values;
    } *params32 = args;
    NTSTATUS status;

    pthread_mutex_lock( &wgl_lock );

    if (!(handle = get_handle_ptr( ULongToPtr(params32->sync), HANDLE_GLSYNC )))
        status = STATUS_INVALID_HANDLE;
    else
    {
        struct glGetSynciv_params params =
        {
            .teb = get_teb64(params32->teb),
            .sync = (GLsync)handle->u.context,
            .pname = params32->pname,
            .count = params32->count,
            .length = ULongToPtr(params32->length),
            .values = ULongToPtr(params32->values),
        };
        status = ext_glGetSynciv( &params );
    }

    pthread_mutex_unlock( &wgl_lock );
    return status;
}

NTSTATUS wow64_ext_glIsSync( void *args )
{
    struct wgl_handle *handle;
    struct
    {
        PTR32 teb;
        PTR32 sync;
        GLboolean ret;
    } *params32 = args;
    NTSTATUS status;

    pthread_mutex_lock( &wgl_lock );

    if (!(handle = get_handle_ptr( ULongToPtr(params32->sync), HANDLE_GLSYNC )))
        status = STATUS_INVALID_HANDLE;
    else
    {
        struct glIsSync_params params =
        {
            .teb = get_teb64(params32->teb),
            .sync = (GLsync)handle->u.context,
        };
        status = ext_glIsSync( &params );
        params32->ret = params.ret;
    }

    pthread_mutex_unlock( &wgl_lock );
    return status;
}

NTSTATUS wow64_ext_glWaitSync( void *args )
{
    struct wgl_handle *handle;
    struct
    {
        PTR32 teb;
        PTR32 sync;
        GLbitfield flags;
        GLuint64 timeout;
    } *params32 = args;
    NTSTATUS status;

    pthread_mutex_lock( &wgl_lock );

    if (!(handle = get_handle_ptr( ULongToPtr(params32->sync), HANDLE_GLSYNC )))
        status = STATUS_INVALID_HANDLE;
    else
    {
        struct glWaitSync_params params =
        {
            .teb = get_teb64(params32->teb),
            .sync = (GLsync)handle->u.context,
            .flags = params32->flags,
            .timeout = params32->timeout,
        };
        status = ext_glWaitSync( &params );
    }

    pthread_mutex_unlock( &wgl_lock );
    return status;
}

static GLint get_buffer_param( TEB *teb, GLenum target, GLenum param )
{
    const struct opengl_funcs *funcs = teb->glTable;
    typeof(*funcs->ext.p_glGetBufferParameteriv) *func;
    GLint size = 0;
    if (!(func = funcs->ext.p_glGetBufferParameteriv)) func = (void *)funcs->wgl.p_wglGetProcAddress( "glGetBufferParameteriv" );
    if (func) func( target, param, &size );
    return size;
}

static void *get_buffer_pointer( TEB *teb, GLenum target )
{
    const struct opengl_funcs *funcs = teb->glTable;
    typeof(*funcs->ext.p_glGetBufferPointerv) *func;
    void *ptr = NULL;
    if (!(func = funcs->ext.p_glGetBufferPointerv)) func = (void *)funcs->wgl.p_wglGetProcAddress( "glGetBufferPointerv" );
    if (func) func( target, GL_BUFFER_MAP_POINTER, &ptr );
    return ptr;
}

static GLint get_named_buffer_param( TEB *teb, GLint buffer, GLenum param )
{
    const struct opengl_funcs *funcs = teb->glTable;
    typeof(*funcs->ext.p_glGetNamedBufferParameteriv) *func;
    GLint size = 0;
    if (!(func = funcs->ext.p_glGetNamedBufferParameteriv)) func = (void *)funcs->wgl.p_wglGetProcAddress( "glGetNamedBufferParameteriv" );
    if (func) func( buffer, param, &size );
    return size;
}

static void *get_named_buffer_pointer( TEB *teb, GLint buffer )
{
    const struct opengl_funcs *funcs = teb->glTable;
    typeof(*funcs->ext.p_glGetNamedBufferPointerv) *func;
    void *ptr = NULL;
    if (!(func = funcs->ext.p_glGetNamedBufferPointerv)) func = (void *)funcs->wgl.p_wglGetProcAddress( "glGetNamedBufferPointerv" );
    if (func) func( buffer, GL_BUFFER_MAP_POINTER, &ptr );
    return ptr;
}

static void unmap_buffer( TEB *teb, GLenum target )
{
    const struct opengl_funcs *funcs = teb->glTable;
    typeof(*funcs->ext.p_glUnmapBuffer) *func;
    if (!(func = funcs->ext.p_glUnmapBuffer)) func = (void *)funcs->wgl.p_wglGetProcAddress( "glUnmapBuffer" );
    if (func) func( target );
}

static void unmap_named_buffer( TEB *teb, GLint buffer )
{
    const struct opengl_funcs *funcs = teb->glTable;
    typeof(*funcs->ext.p_glUnmapNamedBuffer) *func;
    if (!(func = funcs->ext.p_glUnmapNamedBuffer)) func = (void *)funcs->wgl.p_wglGetProcAddress( "glUnmapNamedBuffer" );
    if (func) func( buffer );
}

static NTSTATUS wow64_map_buffer( TEB *teb, GLint buffer, GLenum target, void *ptr, SIZE_T size,
                                  GLbitfield access, PTR32 *ret )
{
    if (*ret)  /* wow64 pointer provided, map buffer to it */
    {
        if (access & GL_MAP_READ_BIT)
        {
            TRACE( "Copying %#zx from buffer at %p to wow64 buffer %p\n", size, ptr, UlongToPtr(*ret) );
            memcpy( UlongToPtr(*ret), ptr, size );
        }

        /* save the wow64 pointer in the buffer data, we'll overwrite it on unmap */
        *(PTR32 *)ptr = (UINT_PTR)*ret;
        return STATUS_SUCCESS;
    }

    if (ULongToPtr(*ret = PtrToUlong(ptr)) == ptr) return STATUS_SUCCESS;  /* we're lucky */
    if (access & GL_MAP_PERSISTENT_BIT)
    {
        FIXME( "GL_MAP_PERSISTENT_BIT not supported!\n" );
        return STATUS_NOT_SUPPORTED;
    }

    if (!size) size = buffer ? get_named_buffer_param( teb, buffer, GL_BUFFER_SIZE ) : get_buffer_param( teb, target, GL_BUFFER_SIZE );
    if ((PTR32)size != size) return STATUS_NO_MEMORY;  /* overflow */
    if (size < sizeof(PTR32))
    {
        FIXME( "Buffer too small for metadata!\n" );
        return STATUS_BUFFER_TOO_SMALL;
    }

    *ret = size;
    return STATUS_INVALID_ADDRESS;
}

static NTSTATUS wow64_unmap_buffer( void *ptr, SIZE_T size, GLbitfield access )
{
    void *wow_ptr;

    if (ULongToPtr(PtrToUlong(ptr)) == ptr) return STATUS_SUCCESS;  /* we're lucky */

    wow_ptr = UlongToPtr(*(PTR32 *)ptr);
    if (access & GL_MAP_WRITE_BIT)
    {
        TRACE( "Copying %#zx from wow64 buffer %p to buffer %p\n", size, wow_ptr, ptr );
        memcpy( ptr, wow_ptr, size );
    }

    return STATUS_INVALID_ADDRESS;
}

static NTSTATUS wow64_gl_get_buffer_pointer_v( void *args, NTSTATUS (*get_buffer_pointer_v64)(void *) )
{
    PTR32 *ptr; /* pointer to the buffer data, where we saved the wow64 pointer */
    struct
    {
        PTR32 teb;
        GLenum target;
        GLenum pname;
        PTR32 params;
    } *params32 = args;
    struct glGetBufferPointerv_params params =
    {
        .teb = get_teb64(params32->teb),
        .target = params32->target,
        .pname = params32->pname,
        .params = (void **)&ptr,
    };
    PTR32 *wow_ptr = UlongToPtr(params32->params);
    NTSTATUS status;

    if ((status = get_buffer_pointer_v64( &params ))) return status;
    if (params.pname != GL_BUFFER_MAP_POINTER) return STATUS_NOT_IMPLEMENTED;
    if (ULongToPtr(*wow_ptr = PtrToUlong(ptr)) == ptr) return STATUS_SUCCESS;  /* we're lucky */
    *wow_ptr = ptr[0];
    return STATUS_SUCCESS;
}

NTSTATUS wow64_ext_glGetBufferPointerv( void *args )
{
    return wow64_gl_get_buffer_pointer_v( args, ext_glGetBufferPointerv );
}

NTSTATUS wow64_ext_glGetBufferPointervARB( void *args )
{
    return wow64_gl_get_buffer_pointer_v( args, ext_glGetBufferPointervARB );
}

static NTSTATUS wow64_gl_get_named_buffer_pointer_v( void *args, NTSTATUS (*gl_get_named_buffer_pointer_v64)(void *) )
{
    PTR32 *ptr; /* pointer to the buffer data, where we saved the wow64 pointer */
    struct
    {
        PTR32 teb;
        GLuint buffer;
        GLenum pname;
        PTR32 params;
    } *params32 = args;
    struct glGetNamedBufferPointerv_params params =
    {
        .teb = get_teb64(params32->teb),
        .buffer = params32->buffer,
        .pname = params32->pname,
        .params = (void **)&ptr,
    };
    PTR32 *wow_ptr = UlongToPtr(params32->params);
    NTSTATUS status;

    if ((status = gl_get_named_buffer_pointer_v64( &params ))) return status;
    if (params.pname != GL_BUFFER_MAP_POINTER) return STATUS_NOT_IMPLEMENTED;
    if (ULongToPtr(*wow_ptr = PtrToUlong(ptr)) == ptr) return STATUS_SUCCESS;  /* we're lucky */
    *wow_ptr = ptr[0];
    return STATUS_SUCCESS;
}

NTSTATUS wow64_ext_glGetNamedBufferPointerv( void *args )
{
    return wow64_gl_get_named_buffer_pointer_v( args, ext_glGetNamedBufferPointerv );
}

NTSTATUS wow64_ext_glGetNamedBufferPointervEXT( void *args )
{
    return wow64_gl_get_named_buffer_pointer_v( args, ext_glGetNamedBufferPointervEXT );
}

static NTSTATUS wow64_gl_map_buffer( void *args, NTSTATUS (*gl_map_buffer64)(void *) )
{
    struct
    {
        PTR32 teb;
        GLenum target;
        GLenum access;
        PTR32 ret;
    } *params32 = args;
    struct glMapBuffer_params params =
    {
        .teb = get_teb64(params32->teb),
        .target = params32->target,
        .access = params32->access,
    };
    NTSTATUS status;

    /* already mapped, we're being called again with a wow64 pointer */
    if (params32->ret) params.ret = get_buffer_pointer( params.teb, params.target );
    else if ((status = gl_map_buffer64( &params ))) return status;

    status = wow64_map_buffer( params.teb, 0, params.target, params.ret, 0, params.access, &params32->ret );
    if (!status || status == STATUS_INVALID_ADDRESS) return status;

    unmap_buffer( params.teb, params.target );
    return status;
}

NTSTATUS wow64_ext_glMapBuffer( void *args )
{
    return wow64_gl_map_buffer( args, ext_glMapBuffer );
}

NTSTATUS wow64_ext_glMapBufferARB( void *args )
{
    return wow64_gl_map_buffer( args, ext_glMapBufferARB );
}

NTSTATUS wow64_ext_glMapBufferRange( void *args )
{
    struct
    {
        PTR32 teb;
        GLenum target;
        PTR32 offset;
        PTR32 length;
        GLbitfield access;
        PTR32 ret;
    } *params32 = args;
    struct glMapBufferRange_params params =
    {
        .teb = get_teb64(params32->teb),
        .target = params32->target,
        .offset = (GLintptr)ULongToPtr(params32->offset),
        .length = (GLsizeiptr)ULongToPtr(params32->length),
        .access = params32->access,
    };
    NTSTATUS status;

    /* already mapped, we're being called again with a wow64 pointer */
    if (params32->ret) params.ret = (char *)get_buffer_pointer( params.teb, params.target );
    else if ((status = ext_glMapBufferRange( &params ))) return status;

    status = wow64_map_buffer( params.teb, 0, params.target, params.ret, params.length, params.access, &params32->ret );
    if (!status || status == STATUS_INVALID_ADDRESS) return status;

    unmap_buffer( params.teb, params.target );
    return status;
}

static NTSTATUS wow64_gl_map_named_buffer( void *args, NTSTATUS (*gl_map_named_buffer64)(void *) )
{
    struct
    {
        PTR32 teb;
        GLuint buffer;
        GLenum access;
        PTR32 ret;
    } *params32 = args;
    struct glMapNamedBuffer_params params =
    {
        .teb = get_teb64(params32->teb),
        .buffer = params32->buffer,
        .access = params32->access,
    };
    NTSTATUS status;

    /* already mapped, we're being called again with a wow64 pointer */
    if (params32->ret) params.ret = get_named_buffer_pointer( params.teb, params.buffer );
    else if ((status = gl_map_named_buffer64( &params ))) return status;

    status = wow64_map_buffer( params.teb, params.buffer, 0, params.ret, 0, params.access, &params32->ret );
    if (!status || status == STATUS_INVALID_ADDRESS) return status;

    unmap_named_buffer( params.teb, params.buffer );
    return status;
}

NTSTATUS wow64_ext_glMapNamedBuffer( void *args )
{
    return wow64_gl_map_named_buffer( args, ext_glMapNamedBuffer );
}

NTSTATUS wow64_ext_glMapNamedBufferEXT( void *args )
{
    return wow64_gl_map_named_buffer( args, ext_glMapNamedBufferEXT );
}

static NTSTATUS wow64_gl_map_named_buffer_range( void *args, NTSTATUS (*gl_map_named_buffer_range64)(void *) )
{
    struct
    {
        PTR32 teb;
        GLuint buffer;
        PTR32 offset;
        PTR32 length;
        GLbitfield access;
        PTR32 ret;
    } *params32 = args;
    struct glMapNamedBufferRange_params params =
    {
        .teb = get_teb64(params32->teb),
        .buffer = params32->buffer,
        .offset = (GLintptr)ULongToPtr(params32->offset),
        .length = (GLsizeiptr)ULongToPtr(params32->length),
        .access = params32->access,
    };
    NTSTATUS status;

    /* already mapped, we're being called again with a wow64 pointer */
    if (params32->ret) params.ret = get_named_buffer_pointer( params.teb, params.buffer );
    else if ((status = gl_map_named_buffer_range64( &params ))) return status;

    status = wow64_map_buffer( params.teb, params.buffer, 0, params.ret, params.length, params.access, &params32->ret );
    if (!status || status == STATUS_INVALID_ADDRESS) return status;

    unmap_named_buffer( params.teb, params.buffer );
    return status;
}

NTSTATUS wow64_ext_glMapNamedBufferRange( void *args )
{
    return wow64_gl_map_named_buffer_range( args, ext_glMapNamedBufferRange );
}

NTSTATUS wow64_ext_glMapNamedBufferRangeEXT( void *args )
{
    return wow64_gl_map_named_buffer_range( args, ext_glMapNamedBufferRangeEXT );
}

static NTSTATUS wow64_gl_unmap_buffer( void *args, NTSTATUS (*gl_unmap_buffer64)(void *) )
{
    PTR32 *ptr;
    struct
    {
        PTR32 teb;
        GLenum target;
        GLboolean ret;
    } *params32 = args;
    NTSTATUS status;
    TEB *teb = get_teb64( params32->teb );

    if (!(ptr = get_buffer_pointer( teb, params32->target ))) return STATUS_SUCCESS;

    status = wow64_unmap_buffer( ptr, get_buffer_param( teb, params32->target, GL_BUFFER_MAP_LENGTH ),
                                 get_buffer_param( teb, params32->target, GL_BUFFER_ACCESS_FLAGS ) );
    gl_unmap_buffer64( args );

    return status;
}

NTSTATUS wow64_ext_glUnmapBuffer( void *args )
{
    return wow64_gl_unmap_buffer( args, ext_glUnmapBuffer );
}

NTSTATUS wow64_ext_glUnmapBufferARB( void *args )
{
    return wow64_gl_unmap_buffer( args, ext_glUnmapBufferARB );
}

static NTSTATUS wow64_gl_unmap_named_buffer( void *args, NTSTATUS (*gl_unmap_named_buffer64)(void *) )
{
    PTR32 *ptr;
    struct
    {
        PTR32 teb;
        GLint buffer;
        GLboolean ret;
    } *params32 = args;
    NTSTATUS status;
    TEB *teb = get_teb64( params32->teb );

    if (!(ptr = get_named_buffer_pointer( teb, params32->buffer ))) return STATUS_SUCCESS;

    status = wow64_unmap_buffer( ptr, get_named_buffer_param( teb, params32->buffer, GL_BUFFER_MAP_LENGTH ),
                                 get_named_buffer_param( teb, params32->buffer, GL_BUFFER_ACCESS_FLAGS ) );
    gl_unmap_named_buffer64( args );

    return status;
}

NTSTATUS wow64_ext_glUnmapNamedBuffer( void *args )
{
    return wow64_gl_unmap_named_buffer( args, ext_glUnmapNamedBuffer );
}

NTSTATUS wow64_ext_glUnmapNamedBufferEXT( void *args )
{
    return wow64_gl_unmap_named_buffer( args, ext_glUnmapNamedBufferEXT );
}

NTSTATUS wow64_thread_attach( void *args )
{
    return thread_attach( get_teb64( (ULONG_PTR)args ));
}

NTSTATUS wow64_process_detach( void *args )
{
    NTSTATUS status;

    if ((status = process_detach( NULL ))) return status;

    free( wow64_strings );
    wow64_strings = NULL;
    wow64_strings_count = 0;

    return STATUS_SUCCESS;
}

#endif
