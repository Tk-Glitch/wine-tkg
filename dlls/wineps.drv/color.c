/*
 *	PostScript colour functions
 *
 *	Copyright 1998  Huw D M Davies
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

#include "psdrv.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(psdrv);


/**********************************************************************
 *	     PSDRV_CopyColor
 *
 * Copies col2 into col1. Return FALSE on error.
 */
BOOL PSDRV_CopyColor(PSCOLOR *col1, PSCOLOR *col2)
{

    switch(col2->type) {
    case PSCOLOR_GRAY:
        col1->value.gray.i = col2->value.gray.i;
	break;

    case PSCOLOR_RGB:
        col1->value.rgb.r = col2->value.rgb.r;
	col1->value.rgb.g = col2->value.rgb.g;
	col1->value.rgb.b = col2->value.rgb.b;
	break;

    default:
        ERR("Unknown colour type %d\n", col1->type);
	return FALSE;
    }

    col1->type = col2->type;
    return TRUE;
}

PSRGB rgb_to_grayscale_scale( void )
{
    static const PSRGB scale = {0.3, 0.59, 0.11};
    /* FIXME configurable */
    return scale;
}

/**********************************************************************
 *	     PSDRV_CreateColor
 *
 * Creates a PostScript colour from a COLORREF.
 * Result is grey scale if ColorDevice field of ppd is CD_False else an
 * rgb colour is produced.
 */
void PSDRV_CreateColor( PHYSDEV dev, PSCOLOR *pscolor, COLORREF wincolor )
{
    PSDRV_PDEVICE *physDev = get_psdrv_dev( dev );
    int ctype = wincolor >> 24;
    float r, g, b;

    if(ctype != 0 && ctype != 2)
        FIXME("Colour is %08x\n", wincolor);

    r = (wincolor & 0xff) / 256.0;
    g = ((wincolor >> 8) & 0xff) / 256.0;
    b = ((wincolor >> 16) & 0xff) / 256.0;

    if(physDev->pi->ppd->ColorDevice != CD_False) {
        pscolor->type = PSCOLOR_RGB;
	pscolor->value.rgb.r = r;
	pscolor->value.rgb.g = g;
	pscolor->value.rgb.b = b;
    } else {
        PSRGB scale = rgb_to_grayscale_scale();
        pscolor->type = PSCOLOR_GRAY;
        pscolor->value.gray.i = r * scale.r + g * scale.g + b * scale.b;
    }
    return;
}


/***********************************************************************
 *           PSDRV_SetBkColor
 */
COLORREF CDECL PSDRV_SetBkColor( PHYSDEV dev, COLORREF color )
{
    PSDRV_PDEVICE *physDev = get_psdrv_dev( dev );
    PSDRV_CreateColor(dev, &physDev->bkColor, color);
    return color;
}


/***********************************************************************
 *           PSDRV_SetTextColor
 */
COLORREF CDECL PSDRV_SetTextColor( PHYSDEV dev, COLORREF color )
{
    PSDRV_PDEVICE *physDev = get_psdrv_dev( dev );
    PSDRV_CreateColor(dev, &physDev->font.color, color);
    physDev->font.set = FALSE;
    return color;
}
