/*
 * MSCMS - Color Management System for Wine
 *
 * Copyright 2005, 2006, 2008 Hans Leidekker
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

#include "windef.h"
#include "winbase.h"
#include "winnls.h"
#include "wingdi.h"
#include "winuser.h"
#include "icm.h"
#include "wine/debug.h"

#include "mscms_priv.h"

WINE_DEFAULT_DEBUG_CHANNEL(mscms);

/******************************************************************************
 * CreateColorTransformA            [MSCMS.@]
 *
 * See CreateColorTransformW.
 */
HTRANSFORM WINAPI CreateColorTransformA( LPLOGCOLORSPACEA space, HPROFILE dest,
    HPROFILE target, DWORD flags )
{
    LOGCOLORSPACEW spaceW;
    DWORD len;

    TRACE( "( %p, %p, %p, 0x%08x )\n", space, dest, target, flags );

    if (!space || !dest) return FALSE;

    memcpy( &spaceW, space, FIELD_OFFSET(LOGCOLORSPACEA, lcsFilename) );
    spaceW.lcsSize = sizeof(LOGCOLORSPACEW);

    len = MultiByteToWideChar( CP_ACP, 0, space->lcsFilename, -1, NULL, 0 );
    MultiByteToWideChar( CP_ACP, 0, space->lcsFilename, -1, spaceW.lcsFilename, len );

    return CreateColorTransformW( &spaceW, dest, target, flags );
}

/******************************************************************************
 * CreateColorTransformW            [MSCMS.@]
 *
 * Create a color transform.
 *
 * PARAMS
 *  space  [I] Input color space.
 *  dest   [I] Color profile of destination device.
 *  target [I] Color profile of target device.
 *  flags  [I] Flags.
 *
 * RETURNS
 *  Success: Handle to a transform.
 *  Failure: NULL
 */
HTRANSFORM WINAPI CreateColorTransformW( LPLOGCOLORSPACEW space, HPROFILE dest,
    HPROFILE target, DWORD flags )
{
    HTRANSFORM ret = NULL;
    struct transform transform;
    struct profile *dst, *tgt = NULL;
    int intent;

    TRACE( "( %p, %p, %p, 0x%08x )\n", space, dest, target, flags );

    if (!lcms_funcs) return FALSE;
    if (!space || !(dst = grab_profile( dest ))) return FALSE;

    if (target && !(tgt = grab_profile( target )))
    {
        release_profile( dst );
        return FALSE;
    }
    intent = space->lcsIntent > 3 ? INTENT_PERCEPTUAL : space->lcsIntent;

    TRACE( "lcsIntent:   %x\n", space->lcsIntent );
    TRACE( "lcsCSType:   %s\n", dbgstr_tag( space->lcsCSType ) );
    TRACE( "lcsFilename: %s\n", debugstr_w( space->lcsFilename ) );

    transform.cmstransform = lcms_funcs->create_transform( dst->cmsprofile,
                                                           tgt ? tgt->cmsprofile : NULL, intent );
    if (!transform.cmstransform)
    {
        if (tgt) release_profile( tgt );
        release_profile( dst );
        return FALSE;
    }

    ret = create_transform( &transform );

    if (tgt) release_profile( tgt );
    release_profile( dst );
    return ret;
}

/******************************************************************************
 * CreateMultiProfileTransform      [MSCMS.@]
 *
 * Create a color transform from an array of color profiles.
 *
 * PARAMS
 *  profiles  [I] Array of color profiles.
 *  nprofiles [I] Number of color profiles.
 *  intents   [I] Array of rendering intents.
 *  flags     [I] Flags.
 *  cmm       [I] Profile to take the CMM from.
 *
 * RETURNS
 *  Success: Handle to a transform.
 *  Failure: NULL
 */ 
HTRANSFORM WINAPI CreateMultiProfileTransform( PHPROFILE profiles, DWORD nprofiles,
    PDWORD intents, DWORD nintents, DWORD flags, DWORD cmm )
{
    HTRANSFORM ret = NULL;
    void *cmsprofiles[2];
    struct transform transform;
    struct profile *profile0, *profile1;

    TRACE( "( %p, 0x%08x, %p, 0x%08x, 0x%08x, 0x%08x )\n",
           profiles, nprofiles, intents, nintents, flags, cmm );

    if (!lcms_funcs) return NULL;
    if (!profiles || !nprofiles || !intents) return NULL;

    if (nprofiles > 2)
    {
        FIXME("more than 2 profiles not supported\n");
        return NULL;
    }

    profile0 = grab_profile( profiles[0] );
    if (!profile0) return NULL;
    profile1 = grab_profile( profiles[1] );
    if (!profile1)
    {
        release_profile( profile0 );
        return NULL;
    }

    cmsprofiles[0] = profile0->cmsprofile;
    cmsprofiles[1] = profile1->cmsprofile;

    transform.cmstransform = lcms_funcs->create_multi_transform( cmsprofiles, nprofiles, *intents );
    if (transform.cmstransform) ret = create_transform( &transform );

    release_profile( profile0 );
    release_profile( profile1 );
    return ret;
}

/******************************************************************************
 * DeleteColorTransform             [MSCMS.@]
 *
 * Delete a color transform.
 *
 * PARAMS
 *  transform [I] Handle to a color transform.
 *
 * RETURNS
 *  Success: TRUE
 *  Failure: FALSE
 */ 
BOOL WINAPI DeleteColorTransform( HTRANSFORM handle )
{
    TRACE( "( %p )\n", handle );

    return close_transform( handle );
}

/******************************************************************************
 * TranslateBitmapBits              [MSCMS.@]
 *
 * Perform color translation.
 *
 * PARAMS
 *  transform    [I] Handle to a color transform.
 *  srcbits      [I] Source bitmap.
 *  input        [I] Format of the source bitmap.
 *  width        [I] Width of the source bitmap.
 *  height       [I] Height of the source bitmap.
 *  inputstride  [I] Number of bytes in one scanline.
 *  destbits     [I] Destination bitmap.
 *  output       [I] Format of the destination bitmap.
 *  outputstride [I] Number of bytes in one scanline. 
 *  callback     [I] Callback function.
 *  data         [I] Callback data. 
 *
 * RETURNS
 *  Success: TRUE
 *  Failure: FALSE
 */
BOOL WINAPI TranslateBitmapBits( HTRANSFORM handle, PVOID srcbits, BMFORMAT input,
    DWORD width, DWORD height, DWORD inputstride, PVOID destbits, BMFORMAT output,
    DWORD outputstride, PBMCALLBACKFN callback, ULONG data )
{
    BOOL ret;
    struct transform *transform = grab_transform( handle );

    TRACE( "( %p, %p, 0x%08x, 0x%08x, 0x%08x, 0x%08x, %p, 0x%08x, 0x%08x, %p, 0x%08x )\n",
           handle, srcbits, input, width, height, inputstride, destbits, output,
           outputstride, callback, data );

    if (!transform) return FALSE;
    ret = lcms_funcs->translate_bits( transform->cmstransform, srcbits, input,
                                      destbits, output, width * height );
    release_transform( transform );
    return ret;
}

/******************************************************************************
 * TranslateColors              [MSCMS.@]
 *
 * Perform color translation.
 *
 * PARAMS
 *  transform    [I] Handle to a color transform.
 *  input        [I] Array of input colors.
 *  number       [I] Number of colors to translate.
 *  input_type   [I] Input color format.
 *  output       [O] Array of output colors.
 *  output_type  [I] Output color format.
 *
 * RETURNS
 *  Success: TRUE
 *  Failure: FALSE
 */
BOOL WINAPI TranslateColors( HTRANSFORM handle, PCOLOR in, DWORD count,
                             COLORTYPE input_type, PCOLOR out, COLORTYPE output_type )
{
    BOOL ret;
    struct transform *transform = grab_transform( handle );

    TRACE( "( %p, %p, %d, %d, %p, %d )\n", handle, in, count, input_type, out, output_type );

    if (!transform) return FALSE;
    ret = lcms_funcs->translate_colors( transform->cmstransform, in, count, input_type, out, output_type );
    release_transform( transform );
    return ret;
}
