/*
 *  Dump an Enhanced Meta File
 *
 *  Copyright 2005 Mike McCormack
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
#include "winedump.h"

#include <stdio.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#include <fcntl.h>
#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "gdiplusenums.h"

typedef struct
{
    WORD Type;
    WORD Flags;
    DWORD Size;
    DWORD DataSize;
} EmfPlusRecordHeader;

static const char *debugstr_wn(const WCHAR *wstr, unsigned int n)
{
    static char buf[80];
    char *p;
    unsigned int i;

    if (!wstr) return "(null)";

    i = 0;
    p = buf;
    *p++ = '\"';
    while (i < n && i < sizeof(buf) - 2 && wstr[i])
    {
        if (wstr[i] < 127) *p++ = wstr[i];
        else *p++ = '.';
        i++;
    }
    *p++ = '\"';
    *p = 0;
    return buf;
}

static unsigned int read_int(const unsigned char *buffer)
{
    return buffer[0]
     + (buffer[1]<<8)
     + (buffer[2]<<16)
     + (buffer[3]<<24);
}

#define EMRCASE(x) case x: printf("%-20s %08x\n", #x, length); break
#define EMRPLUSCASE(x) case x: printf("    %-20s %04x %08x %08x\n", #x, header->Flags, header->Size, header->DataSize); break

static unsigned offset = 0;

static int dump_emfrecord(void)
{
    const unsigned char*        ptr;
    unsigned int type, length, i;

    ptr = PRD(offset, 8);
    if (!ptr) return -1;

    type = read_int(ptr);
    length = read_int(ptr + 4);

    switch(type)
    {
    EMRCASE(EMR_HEADER);
    EMRCASE(EMR_POLYBEZIER);
    EMRCASE(EMR_POLYGON);
    EMRCASE(EMR_POLYLINE);
    EMRCASE(EMR_POLYBEZIERTO);
    EMRCASE(EMR_POLYLINETO);
    EMRCASE(EMR_POLYPOLYLINE);
    EMRCASE(EMR_POLYPOLYGON);
    EMRCASE(EMR_SETWINDOWEXTEX);
    EMRCASE(EMR_SETWINDOWORGEX);
    EMRCASE(EMR_SETVIEWPORTEXTEX);
    EMRCASE(EMR_SETVIEWPORTORGEX);
    EMRCASE(EMR_SETBRUSHORGEX);
    EMRCASE(EMR_EOF);
    EMRCASE(EMR_SETPIXELV);
    EMRCASE(EMR_SETMAPPERFLAGS);
    EMRCASE(EMR_SETMAPMODE);
    EMRCASE(EMR_SETBKMODE);
    EMRCASE(EMR_SETPOLYFILLMODE);
    EMRCASE(EMR_SETROP2);
    EMRCASE(EMR_SETSTRETCHBLTMODE);
    EMRCASE(EMR_SETTEXTALIGN);
    EMRCASE(EMR_SETCOLORADJUSTMENT);
    EMRCASE(EMR_SETTEXTCOLOR);
    EMRCASE(EMR_SETBKCOLOR);
    EMRCASE(EMR_OFFSETCLIPRGN);
    EMRCASE(EMR_MOVETOEX);
    EMRCASE(EMR_SETMETARGN);
    EMRCASE(EMR_EXCLUDECLIPRECT);

    case EMR_INTERSECTCLIPRECT:
    {
        const EMRINTERSECTCLIPRECT *clip = PRD(offset, sizeof(*clip));

        printf("%-20s %08x\n", "EMR_INTERSECTCLIPRECT", length);
        printf("rect %d,%d - %d, %d\n",
               clip->rclClip.left, clip->rclClip.top,
               clip->rclClip.right, clip->rclClip.bottom);
        break;
    }

    EMRCASE(EMR_SCALEVIEWPORTEXTEX);
    EMRCASE(EMR_SCALEWINDOWEXTEX);
    EMRCASE(EMR_SAVEDC);
    EMRCASE(EMR_RESTOREDC);
    EMRCASE(EMR_SETWORLDTRANSFORM);
    EMRCASE(EMR_MODIFYWORLDTRANSFORM);
    EMRCASE(EMR_SELECTOBJECT);
    EMRCASE(EMR_CREATEPEN);
    EMRCASE(EMR_CREATEBRUSHINDIRECT);
    EMRCASE(EMR_DELETEOBJECT);
    EMRCASE(EMR_ANGLEARC);
    EMRCASE(EMR_ELLIPSE);
    EMRCASE(EMR_RECTANGLE);
    EMRCASE(EMR_ROUNDRECT);
    EMRCASE(EMR_ARC);
    EMRCASE(EMR_CHORD);
    EMRCASE(EMR_PIE);
    EMRCASE(EMR_SELECTPALETTE);
    EMRCASE(EMR_CREATEPALETTE);
    EMRCASE(EMR_SETPALETTEENTRIES);
    EMRCASE(EMR_RESIZEPALETTE);
    EMRCASE(EMR_REALIZEPALETTE);
    EMRCASE(EMR_EXTFLOODFILL);
    EMRCASE(EMR_LINETO);
    EMRCASE(EMR_ARCTO);
    EMRCASE(EMR_POLYDRAW);
    EMRCASE(EMR_SETARCDIRECTION);
    EMRCASE(EMR_SETMITERLIMIT);
    EMRCASE(EMR_BEGINPATH);
    EMRCASE(EMR_ENDPATH);
    EMRCASE(EMR_CLOSEFIGURE);
    EMRCASE(EMR_FILLPATH);
    EMRCASE(EMR_STROKEANDFILLPATH);
    EMRCASE(EMR_STROKEPATH);
    EMRCASE(EMR_FLATTENPATH);
    EMRCASE(EMR_WIDENPATH);
    EMRCASE(EMR_SELECTCLIPPATH);
    EMRCASE(EMR_ABORTPATH);

    case EMR_GDICOMMENT:
    {
        printf("%-20s %08x\n", "EMR_GDICOMMENT", length);

        /* Handle EMF+ records */
        if (length >= 16 && !memcmp((char*)PRD(offset + 12, sizeof(unsigned int)), "EMF+", 4))
        {
            const EmfPlusRecordHeader *header;
            const unsigned int *data_size;

            offset += 8;
            length -= 8;
            data_size = PRD(offset, sizeof(*data_size));
            printf("data size = %x\n", *data_size);
            offset += 8;
            length -= 8;

            while (length >= sizeof(*header))
            {
                header = PRD(offset, sizeof(*header));
                switch(header->Type)
                {
                EMRPLUSCASE(EmfPlusRecordTypeInvalid);
                EMRPLUSCASE(EmfPlusRecordTypeHeader);
                EMRPLUSCASE(EmfPlusRecordTypeEndOfFile);
                EMRPLUSCASE(EmfPlusRecordTypeComment);
                EMRPLUSCASE(EmfPlusRecordTypeGetDC);
                EMRPLUSCASE(EmfPlusRecordTypeMultiFormatStart);
                EMRPLUSCASE(EmfPlusRecordTypeMultiFormatSection);
                EMRPLUSCASE(EmfPlusRecordTypeMultiFormatEnd);
                EMRPLUSCASE(EmfPlusRecordTypeObject);
                EMRPLUSCASE(EmfPlusRecordTypeClear);
                EMRPLUSCASE(EmfPlusRecordTypeFillRects);
                EMRPLUSCASE(EmfPlusRecordTypeDrawRects);
                EMRPLUSCASE(EmfPlusRecordTypeFillPolygon);
                EMRPLUSCASE(EmfPlusRecordTypeDrawLines);
                EMRPLUSCASE(EmfPlusRecordTypeFillEllipse);
                EMRPLUSCASE(EmfPlusRecordTypeDrawEllipse);
                EMRPLUSCASE(EmfPlusRecordTypeFillPie);
                EMRPLUSCASE(EmfPlusRecordTypeDrawPie);
                EMRPLUSCASE(EmfPlusRecordTypeDrawArc);
                EMRPLUSCASE(EmfPlusRecordTypeFillRegion);
                EMRPLUSCASE(EmfPlusRecordTypeFillPath);
                EMRPLUSCASE(EmfPlusRecordTypeDrawPath);
                EMRPLUSCASE(EmfPlusRecordTypeFillClosedCurve);
                EMRPLUSCASE(EmfPlusRecordTypeDrawClosedCurve);
                EMRPLUSCASE(EmfPlusRecordTypeDrawCurve);
                EMRPLUSCASE(EmfPlusRecordTypeDrawBeziers);
                EMRPLUSCASE(EmfPlusRecordTypeDrawImage);
                EMRPLUSCASE(EmfPlusRecordTypeDrawImagePoints);
                EMRPLUSCASE(EmfPlusRecordTypeDrawString);
                EMRPLUSCASE(EmfPlusRecordTypeSetRenderingOrigin);
                EMRPLUSCASE(EmfPlusRecordTypeSetAntiAliasMode);
                EMRPLUSCASE(EmfPlusRecordTypeSetTextRenderingHint);
                EMRPLUSCASE(EmfPlusRecordTypeSetTextContrast);
                EMRPLUSCASE(EmfPlusRecordTypeSetInterpolationMode);
                EMRPLUSCASE(EmfPlusRecordTypeSetPixelOffsetMode);
                EMRPLUSCASE(EmfPlusRecordTypeSetCompositingMode);
                EMRPLUSCASE(EmfPlusRecordTypeSetCompositingQuality);
                EMRPLUSCASE(EmfPlusRecordTypeSave);
                EMRPLUSCASE(EmfPlusRecordTypeRestore);
                EMRPLUSCASE(EmfPlusRecordTypeBeginContainer);
                EMRPLUSCASE(EmfPlusRecordTypeBeginContainerNoParams);
                EMRPLUSCASE(EmfPlusRecordTypeEndContainer);
                EMRPLUSCASE(EmfPlusRecordTypeSetWorldTransform);
                EMRPLUSCASE(EmfPlusRecordTypeResetWorldTransform);
                EMRPLUSCASE(EmfPlusRecordTypeMultiplyWorldTransform);
                EMRPLUSCASE(EmfPlusRecordTypeTranslateWorldTransform);
                EMRPLUSCASE(EmfPlusRecordTypeScaleWorldTransform);
                EMRPLUSCASE(EmfPlusRecordTypeRotateWorldTransform);
                EMRPLUSCASE(EmfPlusRecordTypeSetPageTransform);
                EMRPLUSCASE(EmfPlusRecordTypeResetClip);
                EMRPLUSCASE(EmfPlusRecordTypeSetClipRect);
                EMRPLUSCASE(EmfPlusRecordTypeSetClipPath);
                EMRPLUSCASE(EmfPlusRecordTypeSetClipRegion);
                EMRPLUSCASE(EmfPlusRecordTypeOffsetClip);
                EMRPLUSCASE(EmfPlusRecordTypeDrawDriverString);
                EMRPLUSCASE(EmfPlusRecordTypeStrokeFillPath);
                EMRPLUSCASE(EmfPlusRecordTypeSerializableObject);
                EMRPLUSCASE(EmfPlusRecordTypeSetTSGraphics);
                EMRPLUSCASE(EmfPlusRecordTypeSetTSClip);
                EMRPLUSCASE(EmfPlusRecordTotal);

                default:
                    printf("    unknown EMF+ record %x %04x %08x\n", header->Type, header->Flags, header->Size);
                    break;
                }

                if (length<sizeof(*header) || header->Size%4)
                    return -1;

                length -= sizeof(*header);
                offset += sizeof(*header);

                for (i=0; i<header->Size-sizeof(*header); i+=4)
                {
                    if (i%16 == 0)
                        printf("       ");
                    if (!(ptr = PRD(offset, 4))) return -1;
                    length -= 4;
                    offset += 4;
                    printf("%08x ", read_int(ptr));
                    if ((i % 16 == 12) || (i + 4 == header->Size - sizeof(*header)))
                        printf("\n");
                }
            }

            return 0;
        }

        break;
    }

    EMRCASE(EMR_FILLRGN);
    EMRCASE(EMR_FRAMERGN);
    EMRCASE(EMR_INVERTRGN);
    EMRCASE(EMR_PAINTRGN);

    case EMR_EXTSELECTCLIPRGN:
    {
        const EMREXTSELECTCLIPRGN *clip = PRD(offset, sizeof(*clip));
        const RGNDATA *data = (const RGNDATA *)clip->RgnData;
        DWORD i, rc_count = 0;
        const RECT *rc;

        if (length >= sizeof(*clip) + sizeof(*data))
            rc_count = data->rdh.nCount;

        printf("%-20s %08x\n", "EMR_EXTSELECTCLIPRGN", length);
        printf("mode %d, rects %d\n", clip->iMode, rc_count);
        for (i = 0, rc = (const RECT *)data->Buffer; i < rc_count; i++, rc++)
            printf(" (%d,%d)-(%d,%d)", rc->left, rc->top, rc->right, rc->bottom);
        if (rc_count != 0) printf("\n");
        break;
    }

    EMRCASE(EMR_BITBLT);
    EMRCASE(EMR_STRETCHBLT);
    EMRCASE(EMR_MASKBLT);
    EMRCASE(EMR_PLGBLT);
    EMRCASE(EMR_SETDIBITSTODEVICE);
    EMRCASE(EMR_STRETCHDIBITS);

    case EMR_EXTCREATEFONTINDIRECTW:
    {
        const EMREXTCREATEFONTINDIRECTW *pf = PRD(offset, sizeof(*pf));
        const LOGFONTW *plf = &pf->elfw.elfLogFont;

        printf("%-20s %08x\n", "EMR_EXTCREATEFONTINDIRECTW", length);
        printf("(%d %d %d %d %x out %d clip %x quality %d charset %d) %s %s %s %s\n",
               plf->lfHeight, plf->lfWidth,
               plf->lfEscapement, plf->lfOrientation,
               plf->lfPitchAndFamily,
               plf->lfOutPrecision, plf->lfClipPrecision,
               plf->lfQuality, plf->lfCharSet,
               debugstr_wn(plf->lfFaceName, LF_FACESIZE),
               plf->lfWeight > 400 ? "Bold" : "",
               plf->lfItalic ? "Italic" : "",
               plf->lfUnderline ? "Underline" : "");
	break;
    }

    EMRCASE(EMR_EXTTEXTOUTA);

    case EMR_EXTTEXTOUTW:
    {
        const EMREXTTEXTOUTW *etoW = PRD(offset, sizeof(*etoW));

        printf("%-20s %08x\n", "EMR_EXTTEXTOUTW", length);
        printf("pt (%d,%d) rect (%d,%d - %d,%d) flags %#x, %s\n",
               etoW->emrtext.ptlReference.x, etoW->emrtext.ptlReference.y,
               etoW->emrtext.rcl.left, etoW->emrtext.rcl.top,
               etoW->emrtext.rcl.right, etoW->emrtext.rcl.bottom,
               etoW->emrtext.fOptions,
               debugstr_wn((LPCWSTR)((const BYTE *)etoW + etoW->emrtext.offString), etoW->emrtext.nChars));
	break;
    }

    EMRCASE(EMR_POLYBEZIER16);
    EMRCASE(EMR_POLYGON16);
    EMRCASE(EMR_POLYLINE16);
    EMRCASE(EMR_POLYBEZIERTO16);
    EMRCASE(EMR_POLYLINETO16);
    EMRCASE(EMR_POLYPOLYLINE16);
    EMRCASE(EMR_POLYPOLYGON16);
    EMRCASE(EMR_POLYDRAW16);
    EMRCASE(EMR_CREATEMONOBRUSH);
    EMRCASE(EMR_CREATEDIBPATTERNBRUSHPT);
    EMRCASE(EMR_EXTCREATEPEN);
    EMRCASE(EMR_POLYTEXTOUTA);
    EMRCASE(EMR_POLYTEXTOUTW);
    EMRCASE(EMR_SETICMMODE);
    EMRCASE(EMR_CREATECOLORSPACE);
    EMRCASE(EMR_SETCOLORSPACE);
    EMRCASE(EMR_DELETECOLORSPACE);
    EMRCASE(EMR_GLSRECORD);
    EMRCASE(EMR_GLSBOUNDEDRECORD);
    EMRCASE(EMR_PIXELFORMAT);
    EMRCASE(EMR_DRAWESCAPE);
    EMRCASE(EMR_EXTESCAPE);
    EMRCASE(EMR_STARTDOC);
    EMRCASE(EMR_SMALLTEXTOUT);
    EMRCASE(EMR_FORCEUFIMAPPING);
    EMRCASE(EMR_NAMEDESCAPE);
    EMRCASE(EMR_COLORCORRECTPALETTE);
    EMRCASE(EMR_SETICMPROFILEA);
    EMRCASE(EMR_SETICMPROFILEW);
    EMRCASE(EMR_ALPHABLEND);
    EMRCASE(EMR_SETLAYOUT);
    EMRCASE(EMR_TRANSPARENTBLT);
    EMRCASE(EMR_RESERVED_117);
    EMRCASE(EMR_GRADIENTFILL);
    EMRCASE(EMR_SETLINKEDUFI);
    EMRCASE(EMR_SETTEXTJUSTIFICATION);
    EMRCASE(EMR_COLORMATCHTOTARGETW);
    EMRCASE(EMR_CREATECOLORSPACEW);

    default:
        printf("%u %08x\n", type, length);
        break;
    }

    if ( (length < 8) || (length % 4) )
        return -1;

    length -= 8;

    offset += 8;

    for(i=0; i<length; i+=4)
    {
         if (i%16 == 0)
             printf("   ");
         if (!(ptr = PRD(offset, 4))) return -1;
         offset += 4;
         printf("%08x ", read_int(ptr));
         if ( (i % 16 == 12) || (i + 4 == length))
             printf("\n");
    }

    return 0;
}

enum FileSig get_kind_emf(void)
{
    const ENHMETAHEADER*        hdr;

    hdr = PRD(0, sizeof(*hdr));
    if (hdr && hdr->iType == EMR_HEADER && hdr->dSignature == ENHMETA_SIGNATURE)
        return SIG_EMF;
    return SIG_UNKNOWN;
}

void emf_dump(void)
{
    offset = 0;
    while (!dump_emfrecord());
}
