/*
 * Copyright 2020 Brendan Shanks for CodeWeavers
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

#ifndef __WINE_D3DKMDT_H
#define __WINE_D3DKMDT_H

typedef enum _D3DKMDT_VIDEO_SIGNAL_STANDARD
{
    D3DKMDT_VSS_UNINITIALIZED =  0,
    D3DKMDT_VSS_VESA_DMT      =  1,
    D3DKMDT_VSS_VESA_GTF      =  2,
    D3DKMDT_VSS_VESA_CVT      =  3,
    D3DKMDT_VSS_IBM           =  4,
    D3DKMDT_VSS_APPLE         =  5,
    D3DKMDT_VSS_NTSC_M        =  6,
    D3DKMDT_VSS_NTSC_J        =  7,
    D3DKMDT_VSS_NTSC_443      =  8,
    D3DKMDT_VSS_PAL_B         =  9,
    D3DKMDT_VSS_PAL_B1        = 10,
    D3DKMDT_VSS_PAL_G         = 11,
    D3DKMDT_VSS_PAL_H         = 12,
    D3DKMDT_VSS_PAL_I         = 13,
    D3DKMDT_VSS_PAL_D         = 14,
    D3DKMDT_VSS_PAL_N         = 15,
    D3DKMDT_VSS_PAL_NC        = 16,
    D3DKMDT_VSS_SECAM_B       = 17,
    D3DKMDT_VSS_SECAM_D       = 18,
    D3DKMDT_VSS_SECAM_G       = 19,
    D3DKMDT_VSS_SECAM_H       = 20,
    D3DKMDT_VSS_SECAM_K       = 21,
    D3DKMDT_VSS_SECAM_K1      = 22,
    D3DKMDT_VSS_SECAM_L       = 23,
    D3DKMDT_VSS_SECAM_L1      = 24,
    D3DKMDT_VSS_EIA_861       = 25,
    D3DKMDT_VSS_EIA_861A      = 26,
    D3DKMDT_VSS_EIA_861B      = 27,
    D3DKMDT_VSS_PAL_K         = 28,
    D3DKMDT_VSS_PAL_K1        = 29,
    D3DKMDT_VSS_PAL_L         = 30,
    D3DKMDT_VSS_PAL_M         = 31,
    D3DKMDT_VSS_OTHER         = 255
} D3DKMDT_VIDEO_SIGNAL_STANDARD;

#endif /* __WINE_D3DKMDT_H */
