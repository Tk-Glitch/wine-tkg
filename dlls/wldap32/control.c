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

#include "config.h"
#include "wine/port.h"

#include <stdarg.h>
#ifdef HAVE_LDAP_H
#include <ldap.h>
#endif

#include "windef.h"
#include "winbase.h"
#include "winnls.h"

#include "winldap_private.h"
#include "wldap32.h"
#include "wine/debug.h"

#ifdef HAVE_LDAP
WINE_DEFAULT_DEBUG_CHANNEL(wldap32);
#endif

/***********************************************************************
 *      ldap_control_freeA     (WLDAP32.@)
 *
 * See ldap_control_freeW.
 */
ULONG CDECL ldap_control_freeA( LDAPControlA *control )
{
    ULONG ret = WLDAP32_LDAP_SUCCESS;
#ifdef HAVE_LDAP

    TRACE( "(%p)\n", control );
    controlfreeA( control );

#endif
    return ret;
}

/***********************************************************************
 *      ldap_control_freeW     (WLDAP32.@)
 *
 * Free an LDAPControl structure.
 *
 * PARAMS
 *  control  [I] LDAPControl structure to free.
 *
 * RETURNS
 *  LDAP_SUCCESS
 */
ULONG CDECL ldap_control_freeW( LDAPControlW *control )
{
    ULONG ret = WLDAP32_LDAP_SUCCESS;
#ifdef HAVE_LDAP

    TRACE( "(%p)\n", control );
    controlfreeW( control );

#endif
    return ret;
}

/***********************************************************************
 *      ldap_controls_freeA     (WLDAP32.@)
 *
 * See ldap_controls_freeW.
 */
ULONG CDECL ldap_controls_freeA( LDAPControlA **controls )
{
    ULONG ret = WLDAP32_LDAP_SUCCESS;
#ifdef HAVE_LDAP

    TRACE( "(%p)\n", controls );
    controlarrayfreeA( controls );

#endif
    return ret;
}

/***********************************************************************
 *      ldap_controls_freeW     (WLDAP32.@)
 *
 * Free an array of LDAPControl structures.
 *
 * PARAMS
 *  controls  [I] Array of LDAPControl structures to free.
 *
 * RETURNS
 *  LDAP_SUCCESS
 */
ULONG CDECL ldap_controls_freeW( LDAPControlW **controls )
{
    ULONG ret = WLDAP32_LDAP_SUCCESS;
#ifdef HAVE_LDAP

    TRACE( "(%p)\n", controls );
    controlarrayfreeW( controls );

#endif
    return ret;
}

/***********************************************************************
 *      ldap_create_sort_controlA     (WLDAP32.@)
 *
 * See ldap_create_sort_controlW.
 */
ULONG CDECL ldap_create_sort_controlA( WLDAP32_LDAP *ld, PLDAPSortKeyA *sortkey,
    UCHAR critical, PLDAPControlA *control )
{
    ULONG ret = WLDAP32_LDAP_NOT_SUPPORTED;
#ifdef HAVE_LDAP
    LDAPSortKeyW **sortkeyW = NULL;
    LDAPControlW *controlW = NULL;

    TRACE( "(%p, %p, 0x%02x, %p)\n", ld, sortkey, critical, control );

    if (!ld || !sortkey || !control)
        return WLDAP32_LDAP_PARAM_ERROR;

    sortkeyW = sortkeyarrayAtoW( sortkey );
    if (!sortkeyW) return WLDAP32_LDAP_NO_MEMORY;

    ret = ldap_create_sort_controlW( ld, sortkeyW, critical, &controlW );

    *control = controlWtoA( controlW );
    if (!*control) ret = WLDAP32_LDAP_NO_MEMORY;

    ldap_control_freeW( controlW );
    sortkeyarrayfreeW( sortkeyW );

#endif
    return ret;
}

/***********************************************************************
 *      ldap_create_sort_controlW     (WLDAP32.@)
 *
 * Create a control for server sorted search results.
 *
 * PARAMS
 *  ld       [I] Pointer to an LDAP context.
 *  sortkey  [I] Array of LDAPSortKey structures, each specifying an
 *               attribute to use as a sort key, a matching rule and
 *               the sort order (ascending or descending).
 *  critical [I] Tells the server this control is critical to the
 *               search operation.
 *  control  [O] LDAPControl created.
 *
 * RETURNS
 *  Success: LDAP_SUCCESS
 *  Failure: An LDAP error code.
 *
 * NOTES
 *  Pass the created control as a server control in subsequent calls
 *  to ldap_search_ext(_s) to obtain sorted search results.
 */
ULONG CDECL ldap_create_sort_controlW( WLDAP32_LDAP *ld, PLDAPSortKeyW *sortkey,
    UCHAR critical, PLDAPControlW *control )
{
    ULONG ret = WLDAP32_LDAP_NOT_SUPPORTED;
#ifdef HAVE_LDAP
    LDAPSortKey **sortkeyU = NULL;
    LDAPControl *controlU = NULL;

    TRACE( "(%p, %p, 0x%02x, %p)\n", ld, sortkey, critical, control );

    if (!ld || !sortkey || !control)
        return WLDAP32_LDAP_PARAM_ERROR;

    sortkeyU = sortkeyarrayWtoU( sortkey );
    if (!sortkeyU) return WLDAP32_LDAP_NO_MEMORY;

    ret = map_error( ldap_create_sort_control( ld->ld, sortkeyU, critical, &controlU ));

    *control = controlUtoW( controlU );
    if (!*control) ret = WLDAP32_LDAP_NO_MEMORY;

    ldap_control_free( controlU );
    sortkeyarrayfreeU( sortkeyU );

#endif
    return ret;
}

/***********************************************************************
 *      ldap_create_vlv_controlA     (WLDAP32.@)
 *
 * See ldap_create_vlv_controlW.
 */
INT CDECL ldap_create_vlv_controlA( WLDAP32_LDAP *ld, WLDAP32_LDAPVLVInfo *info,
    UCHAR critical, LDAPControlA **control )
{
    INT ret = WLDAP32_LDAP_NOT_SUPPORTED;
#ifdef HAVE_LDAP
    LDAPControlW *controlW = NULL;

    TRACE( "(%p, %p, 0x%02x, %p)\n", ld, info, critical, control );

    if (!ld || !control) return ~0u;

    ret = ldap_create_vlv_controlW( ld, info, critical, &controlW );

    if (ret == WLDAP32_LDAP_SUCCESS)
    {
        *control = controlWtoA( controlW );
        if (!*control) ret = WLDAP32_LDAP_NO_MEMORY;
        ldap_control_freeW( controlW );
    }

#endif
    return ret;
}

/***********************************************************************
 *      ldap_create_vlv_controlW     (WLDAP32.@)
 *
 * Create a virtual list view control.
 *
 * PARAMS
 *  ld       [I] Pointer to an LDAP context.
 *  info     [I] LDAPVLVInfo structure specifying a list view window.
 *  critical [I] Tells the server this control is critical to the
 *               search operation.
 *  control  [O] LDAPControl created.
 *
 * RETURNS
 *  Success: LDAP_SUCCESS
 *  Failure: An LDAP error code.
 *
 * NOTES
 *  Pass the created control in conjunction with a sort control as
 *  server controls in subsequent calls to ldap_search_ext(_s). The
 *  server will then return a sorted, contiguous subset of results
 *  that meets the criteria specified in the LDAPVLVInfo structure.
 */
INT CDECL ldap_create_vlv_controlW( WLDAP32_LDAP *ld, WLDAP32_LDAPVLVInfo *info,
    UCHAR critical, LDAPControlW **control )
{
    INT ret = WLDAP32_LDAP_NOT_SUPPORTED;
#ifdef HAVE_LDAP
    LDAPControl *controlU = NULL;

    TRACE( "(%p, %p, 0x%02x, %p)\n", ld, info, critical, control );

    if (!ld || !control) return ~0u;

    ret = map_error( ldap_create_vlv_control( ld->ld, (LDAPVLVInfo *)info, &controlU ));

    if (ret == WLDAP32_LDAP_SUCCESS)
    {
        *control = controlUtoW( controlU );
        if (!*control) ret = WLDAP32_LDAP_NO_MEMORY;
        ldap_control_free( controlU );
    }

#endif
    return ret;
}

static inline void bv_val_dup( const struct WLDAP32_berval *src, struct WLDAP32_berval *dst )
{
    if ((dst->bv_val = heap_alloc( src->bv_len )))
    {
        memcpy( dst->bv_val, src->bv_val, src->bv_len );
        dst->bv_len = src->bv_len;
    }
    else
        dst->bv_len = 0;
}

/***********************************************************************
 *      ldap_encode_sort_controlA     (WLDAP32.@)
 *
 * See ldap_encode_sort_controlW.
 */
ULONG CDECL ldap_encode_sort_controlA( WLDAP32_LDAP *ld, PLDAPSortKeyA *sortkeys,
    PLDAPControlA ret, BOOLEAN critical )
{
    LDAPControlA *control;
    ULONG result;

    if ((result = ldap_create_sort_controlA( ld, sortkeys, critical, &control )) == WLDAP32_LDAP_SUCCESS)
    {
        ret->ldctl_oid = strdupU(control->ldctl_oid);
        bv_val_dup( &control->ldctl_value, &ret->ldctl_value );
        ret->ldctl_iscritical = control->ldctl_iscritical;
        ldap_control_freeA( control );
    }
    return result;
}

/***********************************************************************
 *      ldap_encode_sort_controlW     (WLDAP32.@)
 *
 * Create a control for server sorted search results.
 *
 * PARAMS
 *  ld       [I] Pointer to an LDAP context.
 *  sortkey  [I] Array of LDAPSortKey structures, each specifying an
 *               attribute to use as a sort key, a matching rule and
 *               the sort order (ascending or descending).
 *  critical [I] Tells the server this control is critical to the
 *               search operation.
 *  control  [O] LDAPControl created.
 *
 * RETURNS
 *  Success: LDAP_SUCCESS
 *  Failure: An LDAP error code.
 *
 * NOTES
 *  This function is obsolete. Use its equivalent
 *  ldap_create_sort_control instead.
 */
ULONG CDECL ldap_encode_sort_controlW( WLDAP32_LDAP *ld, PLDAPSortKeyW *sortkeys,
    PLDAPControlW ret, BOOLEAN critical )
{
    LDAPControlW *control;
    ULONG result;

    if ((result = ldap_create_sort_controlW( ld, sortkeys, critical, &control )) == WLDAP32_LDAP_SUCCESS)
    {
        ret->ldctl_oid = strdupW(control->ldctl_oid);
        bv_val_dup( &control->ldctl_value, &ret->ldctl_value );
        ret->ldctl_iscritical = control->ldctl_iscritical;
        ldap_control_freeW( control );
    }
    return result;
}

/***********************************************************************
 *      ldap_free_controlsA     (WLDAP32.@)
 *
 * See ldap_free_controlsW.
 */
ULONG CDECL ldap_free_controlsA( LDAPControlA **controls )
{
    return ldap_controls_freeA( controls );
}

/***********************************************************************
 *      ldap_free_controlsW     (WLDAP32.@)
 *
 * Free an array of LDAPControl structures.
 *
 * PARAMS
 *  controls  [I] Array of LDAPControl structures to free.
 *
 * RETURNS
 *  LDAP_SUCCESS
 *  
 * NOTES
 *  Obsolete, use ldap_controls_freeW.
 */
ULONG CDECL ldap_free_controlsW( LDAPControlW **controls )
{
    return ldap_controls_freeW( controls );
}
