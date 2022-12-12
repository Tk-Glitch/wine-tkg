/*
 * WLDAP32 - LDAP support for Wine
 *
 * Copyright 2005 Hans Leidekker
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

#include <stdarg.h>
#include <stdlib.h>
#include "windef.h"
#include "winbase.h"
#include "winnls.h"
#include "winternl.h"

#include "wine/debug.h"
#include "winldap_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(wldap32);

/* Split a space separated string of hostnames into a string array */
static char **split_hostnames( const char *hostnames )
{
    char **res, *str, *p, *q;
    unsigned int i = 0;

    str = strdup( hostnames );
    if (!str) return NULL;

    p = str;
    while (isspace( *p )) p++;
    if (*p) i++;

    while (*p)
    {
        if (isspace( *p ))
        {
            while (isspace( *p )) p++;
            if (*p) i++;
        }
        p++;
    }

    if (!(res = malloc( (i + 1) * sizeof(char *) )))
    {
        free( str );
        return NULL;
    }

    p = str;
    while (isspace( *p )) p++;

    q = p;
    i = 0;

    while (*p)
    {
        if (p[1] != '\0')
        {
            if (isspace( *p ))
            {
                *p = '\0'; p++;
                res[i] = strdup( q );
                if (!res[i]) goto oom;
                i++;

                while (isspace( *p )) p++;
                q = p;
            }
        }
        else
        {
            res[i] = strdup( q );
            if (!res[i]) goto oom;
            i++;
        }
        p++;
    }
    res[i] = NULL;

    free( str );
    return res;

oom:
    while (i > 0) free( res[--i] );
    free( res );
    free( str );
    return NULL;
}

/* Determine if a URL starts with a known LDAP scheme */
static BOOL has_ldap_scheme( char *url )
{
    return !_strnicmp( url, "ldap://", 7 )  ||
           !_strnicmp( url, "ldaps://", 8 ) ||
           !_strnicmp( url, "ldapi://", 8 ) ||
           !_strnicmp( url, "cldap://", 8 );
}

/* Flatten an array of hostnames into a space separated string of URLs.
 * Prepend a given scheme and append a given port number to each hostname
 * if necessary.
 */
static char *join_hostnames( const char *scheme, char **hostnames, ULONG portnumber )
{
    char *res, *p, *q, **v;
    unsigned int i = 0, size = 0;
    static const char sep[] = " ";
    char port[7];

    sprintf( port, ":%lu", portnumber );

    for (v = hostnames; *v; v++)
    {
        if (!has_ldap_scheme( *v ))
        {
            size += strlen( scheme );
            q = *v;
        }
        else
            /* skip past colon in scheme prefix */
            q = strchr( *v, '/' );

        size += strlen( *v );

        if (!strchr( q, ':' ))
            size += strlen( port );

        i++;
    }

    size += (i - 1) * strlen( sep );
    if (!(res = malloc( size + 1 ))) return NULL;

    p = res;
    for (v = hostnames; *v; v++)
    {
        if (v != hostnames)
        {
            strcpy( p, sep );
            p += strlen( sep );
        }

        if (!has_ldap_scheme( *v ))
        {
            strcpy( p, scheme );
            p += strlen( scheme );
            q = *v;
        }
        else
            /* skip past colon in scheme prefix */
            q = strchr( *v, '/' );

        strcpy( p, *v );
        p += strlen( *v );

        if (!strchr( q, ':' ))
        {
            strcpy( p, port );
            p += strlen( port );
        }
    }
    return res;
}

static char *urlify_hostnames( const char *scheme, char *hostnames, ULONG port )
{
    char *url = NULL, **strarray;

    strarray = split_hostnames( hostnames );
    if (strarray)
        url = join_hostnames( scheme, strarray, port );
    else
        return NULL;

    strarrayfreeU( strarray );
    return url;
}

static LDAP *create_context( const char *url )
{
    LDAP *ld;
    int version = WLDAP32_LDAP_VERSION3;

    if (!(ld = calloc( 1, sizeof( *ld )))) return NULL;
    if (map_error( ldap_initialize( &CTX(ld), url ) ) == WLDAP32_LDAP_SUCCESS)
    {
        ldap_set_option( CTX(ld), WLDAP32_LDAP_OPT_PROTOCOL_VERSION, &version );
        return ld;
    }
    free( ld );
    return NULL;
}

/***********************************************************************
 *      cldap_openA     (WLDAP32.@)
 */
LDAP * CDECL cldap_openA( char *hostname, ULONG portnumber )
{
    LDAP *ld;
    WCHAR *hostnameW = NULL;

    TRACE( "(%s, %lu)\n", debugstr_a(hostname), portnumber );

    if (hostname && !(hostnameW = strAtoW( hostname ))) return NULL;

    ld = cldap_openW( hostnameW, portnumber );

    free( hostnameW );
    return ld;
}

/***********************************************************************
 *      cldap_openW     (WLDAP32.@)
 */
LDAP * CDECL cldap_openW( WCHAR *hostname, ULONG portnumber )
{
    LDAP *ld = NULL;
    char *hostnameU, *url = NULL;

    TRACE( "(%s, %lu)\n", debugstr_w(hostname), portnumber );

    if (!(hostnameU = strWtoU( hostname ? hostname : L"localhost" ))) return NULL;
    if (!(url = urlify_hostnames( "cldap://", hostnameU, portnumber ))) goto exit;

    ld = create_context( url );

exit:
    free( hostnameU );
    free( url );
    return ld;
}

/***********************************************************************
 *      ldap_connect     (WLDAP32.@)
 */
ULONG CDECL WLDAP32_ldap_connect( LDAP *ld, struct l_timeval *timeout )
{
    TRACE( "(%p, %p)\n", ld, timeout );

    if (!ld) return WLDAP32_LDAP_PARAM_ERROR;
    return WLDAP32_LDAP_SUCCESS; /* FIXME: do something, e.g. ping the host */
}

/***********************************************************************
 *      ldap_initA     (WLDAP32.@)
 */
LDAP *  CDECL ldap_initA( const PCHAR hostname, ULONG portnumber )
{
    LDAP *ld;
    WCHAR *hostnameW = NULL;

    TRACE( "(%s, %lu)\n", debugstr_a(hostname), portnumber );

    if (hostname && !(hostnameW = strAtoW( hostname ))) return NULL;

    ld = ldap_initW( hostnameW, portnumber );

    free( hostnameW );
    return ld;
}

/***********************************************************************
 *      ldap_initW     (WLDAP32.@)
 */
LDAP * CDECL ldap_initW( const PWCHAR hostname, ULONG portnumber )
{
    LDAP *ld = NULL;
    char *hostnameU, *url = NULL;

    TRACE( "(%s, %lu)\n", debugstr_w(hostname), portnumber );

    if (!(hostnameU = strWtoU( hostname ? hostname : L"localhost" ))) return NULL;
    if (!(url = urlify_hostnames( "ldap://", hostnameU, portnumber ))) goto exit;

    ld = create_context( url );

exit:
    free( hostnameU );
    free( url );
    return ld;
}

/***********************************************************************
 *      ldap_openA     (WLDAP32.@)
 */
LDAP * CDECL ldap_openA( char *hostname, ULONG portnumber )
{
    LDAP *ld;
    WCHAR *hostnameW = NULL;

    TRACE( "(%s, %lu)\n", debugstr_a(hostname), portnumber );

    if (hostname && !(hostnameW = strAtoW( hostname ))) return NULL;

    ld = ldap_openW( hostnameW, portnumber );

    free( hostnameW );
    return ld;
}

/***********************************************************************
 *      ldap_openW     (WLDAP32.@)
 */
LDAP * CDECL ldap_openW( WCHAR *hostname, ULONG portnumber )
{
    LDAP *ld = NULL;
    char *hostnameU, *url = NULL;

    TRACE( "(%s, %lu)\n", debugstr_w(hostname), portnumber );

    if (!(hostnameU = strWtoU( hostname ? hostname : L"localhost" ))) return NULL;
    if (!(url = urlify_hostnames( "ldap://", hostnameU, portnumber ))) goto exit;

    ld = create_context( url );

exit:
    free( hostnameU );
    free( url );
    return ld;
}

/***********************************************************************
 *      ldap_sslinitA     (WLDAP32.@)
 */
LDAP * CDECL ldap_sslinitA( char *hostname, ULONG portnumber, int secure )
{
    LDAP *ld;
    WCHAR *hostnameW = NULL;

    TRACE( "(%s, %lu, %d)\n", debugstr_a(hostname), portnumber, secure );

    if (hostname && !(hostnameW = strAtoW( hostname ))) return NULL;

    ld  = ldap_sslinitW( hostnameW, portnumber, secure );

    free( hostnameW );
    return ld;
}

/***********************************************************************
 *      ldap_sslinitW     (WLDAP32.@)
 */
LDAP * CDECL ldap_sslinitW( WCHAR *hostname, ULONG portnumber, int secure )
{
    LDAP *ld = NULL;
    char *hostnameU, *url = NULL;

    TRACE( "(%s, %lu, %d)\n", debugstr_w(hostname), portnumber, secure );

    if (!(hostnameU = strWtoU( hostname ? hostname : L"localhost" ))) return NULL;

    if (secure)
        url = urlify_hostnames( "ldaps://", hostnameU, portnumber );
    else
        url = urlify_hostnames( "ldap://", hostnameU, portnumber );
    if (!url) goto exit;

    ld = create_context( url );

exit:
    free( hostnameU );
    free( url );
    return ld;
}

/***********************************************************************
 *      ldap_start_tls_sA     (WLDAP32.@)
 */
ULONG CDECL ldap_start_tls_sA( LDAP *ld, ULONG *retval, LDAPMessage **result, LDAPControlA **serverctrls,
                               LDAPControlA **clientctrls )
{
    ULONG ret = WLDAP32_LDAP_NO_MEMORY;
    LDAPControlW **serverctrlsW = NULL, **clientctrlsW = NULL;

    TRACE( "(%p, %p, %p, %p, %p)\n", ld, retval, result, serverctrls, clientctrls );

    if (!ld) return ~0u;

    if (serverctrls && !(serverctrlsW = controlarrayAtoW( serverctrls ))) goto exit;
    if (clientctrls && !(clientctrlsW = controlarrayAtoW( clientctrls ))) goto exit;

    ret = ldap_start_tls_sW( ld, retval, result, serverctrlsW, clientctrlsW );

exit:
    controlarrayfreeW( serverctrlsW );
    controlarrayfreeW( clientctrlsW );
    return ret;
}

/***********************************************************************
 *      ldap_start_tls_s     (WLDAP32.@)
 */
ULONG CDECL ldap_start_tls_sW( LDAP *ld, ULONG *retval, LDAPMessage **result, LDAPControlW **serverctrls,
                               LDAPControlW **clientctrls )
{
    ULONG ret = WLDAP32_LDAP_NO_MEMORY;
    LDAPControl **serverctrlsU = NULL, **clientctrlsU = NULL;

    TRACE( "(%p, %p, %p, %p, %p)\n", ld, retval, result, serverctrls, clientctrls );
    if (result)
    {
        FIXME( "result message not supported\n" );
        *result = NULL;
    }

    if (!ld) return ~0u;

    if (serverctrls && !(serverctrlsU = controlarrayWtoU( serverctrls ))) goto exit;
    if (clientctrls && !(clientctrlsU = controlarrayWtoU( clientctrls ))) goto exit;
    else
    {
        ret = map_error( ldap_start_tls_s( CTX(ld), serverctrlsU, clientctrlsU ) );
    }

exit:
    controlarrayfreeU( serverctrlsU );
    controlarrayfreeU( clientctrlsU );
    return ret;
}

/***********************************************************************
 *      ldap_startup     (WLDAP32.@)
 */
ULONG CDECL ldap_startup( LDAP_VERSION_INFO *version, HANDLE *instance )
{
    TRACE( "(%p, %p)\n", version, instance );
    return WLDAP32_LDAP_SUCCESS;
}

/***********************************************************************
 *      ldap_stop_tls_s     (WLDAP32.@)
 */
BOOLEAN CDECL ldap_stop_tls_s( LDAP *ld )
{
    TRACE( "(%p)\n", ld );
    return TRUE; /* FIXME: find a way to stop tls on a connection */
}
