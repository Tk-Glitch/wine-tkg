/*
 * Create dynamic new structures of various types
 * and some utils in that trend.
 *
 * Copyright 1998 Bertho A. Stultiens
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

#include "wrc.h"
#include "newstruc.h"
#include "utils.h"
#include "parser.h"

#include "wingdi.h"	/* for BITMAPINFOHEADER */

#define ICO_PNG_MAGIC 0x474e5089

#include <pshpack2.h>
typedef struct
{
    DWORD biSize;
    WORD  biWidth;
    WORD  biHeight;
    WORD  biPlanes;
    WORD  biBitCount;
} BITMAPOS2HEADER;
#include <poppack.h>

/* New instances for all types of structures */
/* Very inefficient (in size), but very functional :-]
 * Especially for type-checking.
 */

dialog_t *new_dialog(void)
{
    dialog_t *ret = xmalloc( sizeof(*ret) );
    memset( ret, 0, sizeof(*ret) );
    return ret;
}

name_id_t *new_name_id(void)
{
    name_id_t *ret = xmalloc( sizeof(*ret) );
    memset( ret, 0, sizeof(*ret) );
    return ret;
}

menu_t *new_menu(void)
{
    menu_t *ret = xmalloc( sizeof(*ret) );
    memset( ret, 0, sizeof(*ret) );
    return ret;
}

menu_item_t *new_menu_item(void)
{
    menu_item_t *ret = xmalloc( sizeof(*ret) );
    memset( ret, 0, sizeof(*ret) );
    return ret;
}

control_t *new_control(void)
{
    control_t *ret = xmalloc( sizeof(*ret) );
    memset( ret, 0, sizeof(*ret) );
    return ret;
}

icon_t *new_icon(void)
{
    icon_t *ret = xmalloc( sizeof(*ret) );
    memset( ret, 0, sizeof(*ret) );
    return ret;
}

cursor_t *new_cursor(void)
{
    cursor_t *ret = xmalloc( sizeof(*ret) );
    memset( ret, 0, sizeof(*ret) );
    return ret;
}

versioninfo_t *new_versioninfo(void)
{
    versioninfo_t *ret = xmalloc( sizeof(*ret) );
    memset( ret, 0, sizeof(*ret) );
    return ret;
}

ver_value_t *new_ver_value(void)
{
    ver_value_t *ret = xmalloc( sizeof(*ret) );
    memset( ret, 0, sizeof(*ret) );
    return ret;
}

ver_block_t *new_ver_block(void)
{
    ver_block_t *ret = xmalloc( sizeof(*ret) );
    memset( ret, 0, sizeof(*ret) );
    return ret;
}

stt_entry_t *new_stt_entry(void)
{
    stt_entry_t *ret = xmalloc( sizeof(*ret) );
    memset( ret, 0, sizeof(*ret) );
    return ret;
}

accelerator_t *new_accelerator(void)
{
    accelerator_t *ret = xmalloc( sizeof(*ret) );
    memset( ret, 0, sizeof(*ret) );
    return ret;
}

event_t *new_event(void)
{
    event_t *ret = xmalloc( sizeof(*ret) );
    memset( ret, 0, sizeof(*ret) );
    return ret;
}

raw_data_t *new_raw_data(void)
{
    raw_data_t *ret = xmalloc( sizeof(*ret) );
    memset( ret, 0, sizeof(*ret) );
    return ret;
}

lvc_t *new_lvc(void)
{
    lvc_t *ret = xmalloc( sizeof(*ret) );
    memset( ret, 0, sizeof(*ret) );
    return ret;
}

res_count_t *new_res_count(void)
{
    res_count_t *ret = xmalloc( sizeof(*ret) );
    memset( ret, 0, sizeof(*ret) );
    return ret;
}

string_t *new_string(void)
{
    string_t *ret = xmalloc( sizeof(*ret) );
    memset( ret, 0, sizeof(*ret) );
    set_location( &ret->loc );
    return ret;
}

toolbar_item_t *new_toolbar_item(void)
{
    toolbar_item_t *ret = xmalloc( sizeof(*ret) );
    memset( ret, 0, sizeof(*ret) );
    return ret;
}

ani_any_t *new_ani_any(void)
{
    ani_any_t *ret = xmalloc( sizeof(*ret) );
    memset( ret, 0, sizeof(*ret) );
    return ret;
}

resource_t *new_resource(enum res_e t, void *res, int memopt, language_t *lan)
{
	resource_t *r = xmalloc(sizeof(resource_t));
	memset( r, 0, sizeof(*r) );
	r->type = t;
	r->res.overlay = res;
	r->memopt = memopt;
	r->lan = lan;
	return r;
}

version_t *new_version(DWORD v)
{
	version_t *vp = xmalloc(sizeof(version_t));
	*vp = v;
	return vp;
}

characts_t *new_characts(DWORD c)
{
	characts_t *cp = xmalloc(sizeof(characts_t));
	*cp = c;
	return cp;
}

language_t *new_language(int id, int sub)
{
	language_t *lan = xmalloc(sizeof(language_t));
	lan->id = id;
	lan->sub = sub;
	return lan;
}

language_t *dup_language(language_t *l)
{
	if(!l) return NULL;
	return new_language(l->id, l->sub);
}

version_t *dup_version(version_t *v)
{
	if(!v) return NULL;
	return new_version(*v);
}

characts_t *dup_characts(characts_t *c)
{
	if(!c) return NULL;
	return new_characts(*c);
}

html_t *new_html(raw_data_t *rd, int *memopt)
{
	html_t *html = xmalloc(sizeof(html_t));
	html->data = rd;
	if(memopt)
	{
		html->memopt = *memopt;
		free(memopt);
	}
	else
		html->memopt = WRC_MO_MOVEABLE | WRC_MO_PURE;
	return html;
}

rcdata_t *new_rcdata(raw_data_t *rd, int *memopt)
{
	rcdata_t *rc = xmalloc(sizeof(rcdata_t));
	rc->data = rd;
	if(memopt)
	{
		rc->memopt = *memopt;
		free(memopt);
	}
	else
		rc->memopt = WRC_MO_MOVEABLE | WRC_MO_PURE;
	return rc;
}

font_id_t *new_font_id(int size, string_t *face, int weight, int italic)
{
	font_id_t *fid = xmalloc(sizeof(font_id_t));
	fid->name = face;
	fid->size = size;
	fid->weight = weight;
	fid->italic = italic;
	return fid;
}

user_t *new_user(name_id_t *type, raw_data_t *rd, int *memopt)
{
	user_t *usr = xmalloc(sizeof(user_t));
	usr->data = rd;
	if(memopt)
	{
		usr->memopt = *memopt;
		free(memopt);
	}
	else
		usr->memopt = WRC_MO_MOVEABLE | WRC_MO_PURE;
	usr->type = type;
	return usr;
}

font_t *new_font(raw_data_t *rd, int *memopt)
{
	font_t *fnt = xmalloc(sizeof(font_t));
	fnt->data = rd;
	if(memopt)
	{
		fnt->memopt = *memopt;
		free(memopt);
	}
	else
		fnt->memopt = WRC_MO_MOVEABLE | WRC_MO_DISCARDABLE;
	return fnt;
}

fontdir_t *new_fontdir(raw_data_t *rd, int *memopt)
{
	fontdir_t *fnd = xmalloc(sizeof(fontdir_t));
	fnd->data = rd;
	if(memopt)
	{
		fnd->memopt = *memopt;
		free(memopt);
	}
	else
		fnd->memopt = WRC_MO_MOVEABLE | WRC_MO_DISCARDABLE;
	return fnd;
}


/*
 * Convert bitmaps to proper endian
 */
static void convert_bitmap_swap(BITMAPV5HEADER *bh, DWORD size)
{
	bh->bV5Size		= BYTESWAP_DWORD(bh->bV5Size);
	bh->bV5Width		= BYTESWAP_DWORD(bh->bV5Width);
	bh->bV5Height		= BYTESWAP_DWORD(bh->bV5Height);
	bh->bV5Planes		= BYTESWAP_WORD(bh->bV5Planes);
	bh->bV5BitCount		= BYTESWAP_WORD(bh->bV5BitCount);
	bh->bV5Compression	= BYTESWAP_DWORD(bh->bV5Compression);
	bh->bV5SizeImage	= BYTESWAP_DWORD(bh->bV5SizeImage);
	bh->bV5XPelsPerMeter	= BYTESWAP_DWORD(bh->bV5XPelsPerMeter);
	bh->bV5YPelsPerMeter	= BYTESWAP_DWORD(bh->bV5YPelsPerMeter);
	bh->bV5ClrUsed		= BYTESWAP_DWORD(bh->bV5ClrUsed);
	bh->bV5ClrImportant	= BYTESWAP_DWORD(bh->bV5ClrImportant);
        if (size == sizeof(BITMAPINFOHEADER)) return;
	bh->bV5RedMask		= BYTESWAP_DWORD(bh->bV5RedMask);
	bh->bV5GreenMask	= BYTESWAP_DWORD(bh->bV5GreenMask);
	bh->bV5BlueMask		= BYTESWAP_DWORD(bh->bV5BlueMask);
	bh->bV5AlphaMask	= BYTESWAP_DWORD(bh->bV5AlphaMask);
	bh->bV5CSType		= BYTESWAP_DWORD(bh->bV5CSType);
	bh->bV5Endpoints.ciexyzRed.ciexyzX	= BYTESWAP_DWORD(bh->bV5Endpoints.ciexyzRed.ciexyzX);
	bh->bV5Endpoints.ciexyzRed.ciexyzY	= BYTESWAP_DWORD(bh->bV5Endpoints.ciexyzRed.ciexyzY);
	bh->bV5Endpoints.ciexyzRed.ciexyzZ	= BYTESWAP_DWORD(bh->bV5Endpoints.ciexyzRed.ciexyzZ);
	bh->bV5Endpoints.ciexyzGreen.ciexyzX	= BYTESWAP_DWORD(bh->bV5Endpoints.ciexyzGreen.ciexyzX);
	bh->bV5Endpoints.ciexyzGreen.ciexyzY	= BYTESWAP_DWORD(bh->bV5Endpoints.ciexyzGreen.ciexyzY);
	bh->bV5Endpoints.ciexyzGreen.ciexyzZ	= BYTESWAP_DWORD(bh->bV5Endpoints.ciexyzGreen.ciexyzZ);
	bh->bV5Endpoints.ciexyzBlue.ciexyzX	= BYTESWAP_DWORD(bh->bV5Endpoints.ciexyzBlue.ciexyzX);
	bh->bV5Endpoints.ciexyzBlue.ciexyzY	= BYTESWAP_DWORD(bh->bV5Endpoints.ciexyzBlue.ciexyzY);
	bh->bV5Endpoints.ciexyzBlue.ciexyzZ	= BYTESWAP_DWORD(bh->bV5Endpoints.ciexyzBlue.ciexyzZ);
	bh->bV5GammaRed		= BYTESWAP_DWORD(bh->bV5GammaRed);
	bh->bV5GammaGreen	= BYTESWAP_DWORD(bh->bV5GammaGreen);
	bh->bV5GammaBlue	= BYTESWAP_DWORD(bh->bV5GammaBlue);
        if (size == sizeof(BITMAPV4HEADER)) return;
	bh->bV5Intent		= BYTESWAP_DWORD(bh->bV5Intent);
	bh->bV5ProfileData	= BYTESWAP_DWORD(bh->bV5ProfileData);
	bh->bV5ProfileSize	= BYTESWAP_DWORD(bh->bV5ProfileSize);
	bh->bV5Reserved		= BYTESWAP_DWORD(bh->bV5Reserved);
}

static void convert_bitmap_swap_os2(BITMAPOS2HEADER *boh)
{
	boh->biSize		= BYTESWAP_DWORD(boh->biSize);
	boh->biWidth		= BYTESWAP_WORD(boh->biWidth);
	boh->biHeight		= BYTESWAP_WORD(boh->biHeight);
	boh->biPlanes		= BYTESWAP_WORD(boh->biPlanes);
	boh->biBitCount		= BYTESWAP_WORD(boh->biBitCount);
}

#define FL_SIGBE	0x01
#define FL_SIZEBE	0x02
static int convert_bitmap(char *data, int size)
{
	BITMAPV5HEADER *bih = (BITMAPV5HEADER *)data;
	BITMAPOS2HEADER *boh = (BITMAPOS2HEADER *)data;
	DWORD bmsize;
	int type = 0;
	int returnSize = 0;           /* size to be returned */

    /*
     * Originally the bih and b4h pointers were simply incremented here,
     * and memmoved at the end of the function.  This causes alignment
     * issues on solaris, so we do the memmove here rather than at the end.
     */
	if(data[0] == 'B' && data[1] == 'M')
	{
		/* Little endian signature */
         memmove(data, data+sizeof(BITMAPFILEHEADER), size - sizeof(BITMAPFILEHEADER));
         returnSize = sizeof(BITMAPFILEHEADER);
	}
	else if(data[0] == 'M' && data[1] == 'B')
	{
		type |= FL_SIGBE;	/* Big endian signature */
         memmove(data, data+sizeof(BITMAPFILEHEADER), size - sizeof(BITMAPFILEHEADER));
         returnSize = sizeof(BITMAPFILEHEADER);

	}

        bmsize = bih->bV5Size;
        switch (bmsize)
        {
        case sizeof(BITMAPOS2HEADER):
        case sizeof(BITMAPINFOHEADER):
        case sizeof(BITMAPV4HEADER):
        case sizeof(BITMAPV5HEADER):
#ifdef WORDS_BIGENDIAN
            type |= FL_SIZEBE;
#endif
            break;
        case BYTESWAP_DWORD( sizeof(BITMAPOS2HEADER) ):
        case BYTESWAP_DWORD( sizeof(BITMAPINFOHEADER) ):
        case BYTESWAP_DWORD( sizeof(BITMAPV4HEADER) ):
        case BYTESWAP_DWORD( sizeof(BITMAPV5HEADER) ):
#ifndef WORDS_BIGENDIAN
            type |= FL_SIZEBE;
#endif
            bmsize = BYTESWAP_DWORD( bmsize );
            break;
        case ICO_PNG_MAGIC:
        case BYTESWAP_DWORD( ICO_PNG_MAGIC ):
            return 0;  /* nothing to convert */
        default:
		parser_error("Invalid bitmap format, bih->biSize = %d", bih->bV5Size);
        }

	switch(type)
	{
	case FL_SIZEBE:
            parser_warning("Bitmap signature little-endian, but size big-endian\n");
            break;
	case FL_SIGBE:
            parser_warning("Bitmap signature big-endian, but size little-endian\n");
            break;
	}

	switch(byteorder)
	{
#ifdef WORDS_BIGENDIAN
	default:
#endif
	case WRC_BO_BIG:
		if(!(type & FL_SIZEBE))
		{
                    if (bmsize == sizeof(BITMAPOS2HEADER))
                        convert_bitmap_swap_os2(boh);
                    else
                        convert_bitmap_swap(bih, bmsize);
		}
		break;
#ifndef WORDS_BIGENDIAN
	default:
#endif
	case WRC_BO_LITTLE:
		if(type & FL_SIZEBE)
		{
                    if (bmsize == sizeof(BITMAPOS2HEADER))
                        convert_bitmap_swap_os2(boh);
                    else
                        convert_bitmap_swap(bih, bmsize);
		}
		break;
	}

	if(size && (void *)data != (void *)bih)
	{
		/* We have the fileheader still attached, remove it */
		memmove(data, data+sizeof(BITMAPFILEHEADER), size - sizeof(BITMAPFILEHEADER));
		return sizeof(BITMAPFILEHEADER);
	}
    return returnSize;
}
#undef FL_SIGBE
#undef FL_SIZEBE

/*
 * Cursor and icon splitter functions used when allocating
 * cursor- and icon-groups.
 */
typedef struct {
	language_t	lan;
	int		id;
} id_alloc_t;

static int get_new_id(id_alloc_t **list, int *n, language_t *lan)
{
	int i;
	assert(lan != NULL);
	assert(list != NULL);
	assert(n != NULL);

	if(!*list)
	{
		*list = xmalloc(sizeof(id_alloc_t));
		*n = 1;
		(*list)[0].lan = *lan;
		(*list)[0].id = 1;
		return 1;
	}

	for(i = 0; i < *n; i++)
	{
		if((*list)[i].lan.id == lan->id && (*list)[i].lan.sub == lan->sub)
			return ++((*list)[i].id);
	}

	*list = xrealloc(*list, sizeof(id_alloc_t) * (*n+1));
	(*list)[*n].lan = *lan;
	(*list)[*n].id = 1;
	*n += 1;
	return 1;
}

static int alloc_icon_id(language_t *lan)
{
	static id_alloc_t *idlist = NULL;
	static int nid = 0;

	return get_new_id(&idlist, &nid, lan);
}

static int alloc_cursor_id(language_t *lan)
{
	static id_alloc_t *idlist = NULL;
	static int nid = 0;

	return get_new_id(&idlist, &nid, lan);
}

static void split_icons(raw_data_t *rd, icon_group_t *icog, int *nico)
{
	int cnt;
	int i;
	icon_t *ico;
	icon_t *list = NULL;
	icon_header_t *ih = (icon_header_t *)rd->data;
	int swap = 0;

	if(ih->type == 1)
		swap = 0;
	else if(BYTESWAP_WORD(ih->type) == 1)
		swap = 1;
	else
		parser_error("Icon resource data has invalid type id %d", ih->type);

	cnt = swap ? BYTESWAP_WORD(ih->count) : ih->count;
	for(i = 0; i < cnt; i++)
	{
		icon_dir_entry_t ide;
		BITMAPINFOHEADER info;
		memcpy(&ide, rd->data + sizeof(icon_header_t)
                                      + i*sizeof(icon_dir_entry_t), sizeof(ide));

		ico = new_icon();
		ico->id = alloc_icon_id(icog->lvc.language);
		ico->lvc = icog->lvc;
		if(swap)
		{
			ide.offset = BYTESWAP_DWORD(ide.offset);
			ide.ressize= BYTESWAP_DWORD(ide.ressize);
		}
		if(ide.offset > rd->size
		|| ide.offset + ide.ressize > rd->size)
			parser_error("Icon resource data corrupt");
		ico->width = ide.width;
		ico->height = ide.height;
		ico->nclr = ide.nclr;
		ico->planes = swap ? BYTESWAP_WORD(ide.planes) : ide.planes;
		ico->bits = swap ? BYTESWAP_WORD(ide.bits) : ide.bits;
		memcpy(&info, rd->data + ide.offset, sizeof(info));
         convert_bitmap((char *) &info, 0);
         memcpy(rd->data + ide.offset, &info, sizeof(info));

		if(!ico->planes)
		{
			/* Argh! They did not fill out the resdir structure */
			/* The bitmap is in destination byteorder. We want native for our structures */
			switch(byteorder)
			{
#ifdef WORDS_BIGENDIAN
			case WRC_BO_LITTLE:
#else
			case WRC_BO_BIG:
#endif
				ico->planes = BYTESWAP_WORD(info.biPlanes);
				break;
			default:
				ico->planes = info.biPlanes;
			}
		}
		if(!ico->bits)
		{
			/* Argh! They did not fill out the resdir structure */
			/* The bitmap is in destination byteorder. We want native for our structures */
			switch(byteorder)
			{
#ifdef WORDS_BIGENDIAN
			case WRC_BO_LITTLE:
#else
			case WRC_BO_BIG:
#endif
				ico->bits = BYTESWAP_WORD(info.biBitCount);
				break;
			default:
				ico->bits = info.biBitCount;
			}
		}
		ico->data = new_raw_data();
		copy_raw_data(ico->data, rd, ide.offset, ide.ressize);
		if(!list)
		{
			list = ico;
		}
		else
		{
			ico->next = list;
			list->prev = ico;
			list = ico;
		}
	}
	icog->iconlist = list;
	*nico = cnt;
}

static void split_cursors(raw_data_t *rd, cursor_group_t *curg, int *ncur)
{
	int cnt;
	int i;
	cursor_t *cur;
	cursor_t *list = NULL;
	cursor_header_t *ch = (cursor_header_t *)rd->data;
	int swap = 0;

	if(ch->type == 2)
		swap = 0;
	else if(BYTESWAP_WORD(ch->type) == 2)
		swap = 1;
	else
		parser_error("Cursor resource data has invalid type id %d", ch->type);
	cnt = swap ? BYTESWAP_WORD(ch->count) : ch->count;
	for(i = 0; i < cnt; i++)
	{
		cursor_dir_entry_t cde;
		BITMAPINFOHEADER info;
		memcpy(&cde, rd->data + sizeof(cursor_header_t)
                                      + i*sizeof(cursor_dir_entry_t), sizeof(cde));

		cur = new_cursor();
		cur->id = alloc_cursor_id(curg->lvc.language);
		cur->lvc = curg->lvc;
		if(swap)
		{
			cde.offset = BYTESWAP_DWORD(cde.offset);
			cde.ressize= BYTESWAP_DWORD(cde.ressize);
		}
		if(cde.offset > rd->size
		|| cde.offset + cde.ressize > rd->size)
			parser_error("Cursor resource data corrupt");
		cur->width = cde.width;
		cur->height = cde.height;
		cur->nclr = cde.nclr;
		memcpy(&info, rd->data + cde.offset, sizeof(info));
         convert_bitmap((char *)&info, 0);
         memcpy(rd->data + cde.offset, &info, sizeof(info));
		/* The bitmap is in destination byteorder. We want native for our structures */
		switch(byteorder)
		{
#ifdef WORDS_BIGENDIAN
		case WRC_BO_LITTLE:
#else
		case WRC_BO_BIG:
#endif
			cur->planes = BYTESWAP_WORD(info.biPlanes);
			cur->bits = BYTESWAP_WORD(info.biBitCount);
			break;
		default:
			cur->planes = info.biPlanes;
			cur->bits = info.biBitCount;
		}
		if(!win32 && (cur->planes != 1 || cur->bits != 1))
			parser_warning("Win16 cursor contains colors\n");
		cur->xhot = swap ? BYTESWAP_WORD(cde.xhot) : cde.xhot;
		cur->yhot = swap ? BYTESWAP_WORD(cde.yhot) : cde.yhot;
		cur->data = new_raw_data();
		copy_raw_data(cur->data, rd, cde.offset, cde.ressize);
		if(!list)
		{
			list = cur;
		}
		else
		{
			cur->next = list;
			list->prev = cur;
			list = cur;
		}
	}
	curg->cursorlist = list;
	*ncur = cnt;
}


icon_group_t *new_icon_group(raw_data_t *rd, int *memopt)
{
	icon_group_t *icog = xmalloc(sizeof(icon_group_t));
	if(memopt)
	{
		icog->memopt = *memopt;
		free(memopt);
	}
	else
		icog->memopt = WRC_MO_MOVEABLE | WRC_MO_PURE | WRC_MO_DISCARDABLE;
	icog->lvc = rd->lvc;
	split_icons(rd, icog, &(icog->nicon));
	free(rd->data);
	free(rd);
	return icog;
}

cursor_group_t *new_cursor_group(raw_data_t *rd, int *memopt)
{
	cursor_group_t *curg = xmalloc(sizeof(cursor_group_t));
	if(memopt)
	{
		curg->memopt = *memopt;
		free(memopt);
	}
	else
		curg->memopt = WRC_MO_MOVEABLE | WRC_MO_PURE | WRC_MO_DISCARDABLE;
	curg->lvc = rd->lvc;
	split_cursors(rd, curg, &(curg->ncursor));
	free(rd->data);
	free(rd);
	return curg;
}

/*
 * Animated cursors and icons
 *
 * The format of animated cursors and icons is yet another example
 * of bad design by "The Company". The entire RIFF structure is
 * flawed by design because it is inconsistent and single minded:
 * - some tags have lengths attached, others don't. The use of these
 *   non-length tags is absolutely unclear;
 * - the content of "icon" tags can be both icons and cursors;
 * - tags lack proper alignment constraints. It seems that everything
 *   is 16bit aligned, but I could not find that in any docu. Just be
 *   prepared to eat anything;
 * - there are no strict constraints on tag-nesting and the organization
 *   is highly illogical;
 *
 * Anyhow, here is the basic structure:
 * "RIFF" { dword taglength }
 * 	"ACON"					// What does it do?
 *	"LIST" { dword taglength }
 *		"INFO"				// And what does this do?
 *		"INAM" { dword taglength }	// Icon/cursor name
 *			{inam data}
 *		"IART" { dword taglength }	// The artist
 *			{iart data}
 *		"fram"				// Is followed by "icon"s
 *		"icon" { dword taglength }	// First frame
 *			{ icon/cursor data }
 *		"icon" { dword taglength }	// Second frame
 *			{ icon/cursor data }
 *			...			// ...
 *	"anih" { dword taglength }		// Header structure
 *		{ aniheader_t structure }
 *	"rate" { dword taglength }		// The rate for each frame
 *		{ `steps' dwords }
 *	"seq " { dword taglength }		// The frame blit-order
 *		{ `steps' dwords }
 *
 * Tag length are bytelength without the header and length field (i.e. -8).
 * The "LIST" tag may occur several times and may encapsulate different
 * tags. The `steps' is the number of "icon" tags found (actually the
 * number of steps specified in the aniheader_t structure). The "seq "uence
 * tag can be omitted, in which case the sequence is equal to the sequence
 * of "icon"s found in the file. Also "rate" may be omitted, in which case
 * the default from the aniheader_t structure is used.
 *
 * An animated cursor puts `.cur' formatted files into each "icon" tag,
 * whereas animated icons contain `.ico' formatted files.
 *
 * Note about the code: Yes, it can be shorter/compressed. Some tags can be
 * dealt with in the same code. However, this version shows what is going on
 * and is better debug-able.
 */
static const char riff[4] = "RIFF";
static const char acon[4] = "ACON";
static const char list[4] = "LIST";
static const char info[4] = "INFO";
static const char inam[4] = "INAM";
static const char iart[4] = "IART";
static const char fram[4] = "fram";
static const char icon[4] = "icon";
static const char anih[4] = "anih";
static const char rate[4] = "rate";
static const char seq[4]  = "seq ";

#define SKIP_TAG(p,size) 	((riff_tag_t *)(((char *)p) + (size)))

#define NEXT_TAG(p)	SKIP_TAG(p,(isswapped ? BYTESWAP_DWORD(p->size) : p->size) + sizeof(*p))

static void handle_ani_icon(riff_tag_t *rtp, enum res_e type, int isswapped)
{
	cursor_dir_entry_t *cdp;
	cursor_header_t *chp;
	int count;
	int ctype;
	int i;
	static int once = 0;	/* This will trigger only once per file! */
	const char *anistr = type == res_aniico ? "icon" : "cursor";
	/* Notes:
	 * Both cursor and icon directories are similar
	 * Both cursor and icon headers are similar
	 */

	chp = (cursor_header_t *)(rtp+1);
	cdp = (cursor_dir_entry_t *)(chp+1);
	count = isswapped ? BYTESWAP_WORD(chp->count) : chp->count;
	ctype = isswapped ? BYTESWAP_WORD(chp->type) : chp->type;
	chp->reserved	= BYTESWAP_WORD(chp->reserved);
	chp->type	= BYTESWAP_WORD(chp->type);
	chp->count	= BYTESWAP_WORD(chp->count);

	if(type == res_anicur && ctype != 2 && !once)
	{
		parser_warning("Animated cursor contains invalid \"icon\" tag cursor-file (%d->%s)\n",
				ctype,
				ctype == 1 ? "icontype" : "?");
		once++;
	}
	else if(type == res_aniico && ctype != 1 && !once)
	{
		parser_warning("Animated icon contains invalid \"icon\" tag icon-file (%d->%s)\n",
				ctype,
				ctype == 2 ? "cursortype" : "?");
		once++;
	}
	else if(ctype != 1 && ctype != 2 && !once)
	{
		parser_warning("Animated %s contains invalid \"icon\" tag file-type (%d; neither icon nor cursor)\n", anistr, ctype);
		once++;
	}

	for(i = 0; i < count; i++)
	{
		DWORD ofs = isswapped ? BYTESWAP_DWORD(cdp[i].offset) : cdp[i].offset;
		DWORD sze = isswapped ? BYTESWAP_DWORD(cdp[i].ressize) : cdp[i].ressize;
		if(ofs > rtp->size || ofs+sze > rtp->size)
			parser_error("Animated %s's data corrupt", anistr);
		convert_bitmap((char *)chp + ofs, 0);
		cdp[i].xhot	= BYTESWAP_WORD(cdp->xhot);
		cdp[i].yhot	= BYTESWAP_WORD(cdp->yhot);
		cdp[i].ressize	= BYTESWAP_DWORD(cdp->ressize);
		cdp[i].offset	= BYTESWAP_DWORD(cdp->offset);
	}
}

static void handle_ani_list(riff_tag_t *lst, enum res_e type, int isswapped)
{
	riff_tag_t *rtp = lst+1;	/* Skip the "LIST" tag */

	while((char *)rtp < (char *)lst + lst->size + sizeof(*lst))
	{
		if(!memcmp(rtp->tag, info, sizeof(info)))
		{
			rtp = SKIP_TAG(rtp,4);
		}
		else if(!memcmp(rtp->tag, inam, sizeof(inam)))
		{
                        /* Ignore the icon/cursor name; it's a string */
			rtp = NEXT_TAG(rtp);
		}
		else if(!memcmp(rtp->tag, iart, sizeof(iart)))
		{
			/* Ignore the author's name; it's a string */
			rtp = NEXT_TAG(rtp);
		}
		else if(!memcmp(rtp->tag, fram, sizeof(fram)))
		{
			/* This should be followed by "icon"s, but we
			 * simply ignore this because it is pure
			 * non-information.
			 */
			rtp = SKIP_TAG(rtp,4);
		}
		else if(!memcmp(rtp->tag, icon, sizeof(icon)))
		{
			handle_ani_icon(rtp, type, isswapped);
			rtp = NEXT_TAG(rtp);
		}
		else
			internal_error(__FILE__, __LINE__, "Unknown tag \"%c%c%c%c\" in RIFF file\n",
				       isprint(rtp->tag[0]) ? rtp->tag[0] : '.',
				       isprint(rtp->tag[1]) ? rtp->tag[1] : '.',
				       isprint(rtp->tag[2]) ? rtp->tag[2] : '.',
				       isprint(rtp->tag[3]) ? rtp->tag[3] : '.');

		if((UINT_PTR)rtp & 1)
			rtp = SKIP_TAG(rtp,1);
	}
}

ani_curico_t *new_ani_curico(enum res_e type, raw_data_t *rd, int *memopt)
{
	ani_curico_t *ani = xmalloc(sizeof(ani_curico_t));
	riff_tag_t *rtp;
	int isswapped = 0;
	int doswap;
	const char *anistr = type == res_aniico ? "icon" : "cursor";

	assert(!memcmp(rd->data, riff, sizeof(riff)));
	assert(type == res_anicur || type == res_aniico);

	rtp = (riff_tag_t *)rd->data;

	if(BYTESWAP_DWORD(rtp->size) + 2*sizeof(DWORD) == rd->size)
		isswapped = 1;
	else if(rtp->size + 2*sizeof(DWORD) == rd->size)
		isswapped = 0;
	else
		parser_error("Animated %s has an invalid RIFF length", anistr);

	switch(byteorder)
	{
#ifdef WORDS_BIGENDIAN
	case WRC_BO_LITTLE:
#else
	case WRC_BO_BIG:
#endif
		doswap = !isswapped;
		break;
	default:
		doswap = isswapped;
	}

	/*
	 * When to swap what:
	 * isswapped | doswap |
	 * ----------+--------+---------------------------------
	 *     0     |    0   | read native; don't convert
	 *     1     |    0   | read swapped size; don't convert
	 *     0     |    1   | read native; convert
	 *     1     |    1   | read swapped size; convert
	 * Reading swapped size if necessary to calculate in native
	 * format. E.g. a little-endian source on a big-endian
	 * processor.
	 */
	if(doswap)
	{
		/* We only go through the RIFF file if we need to swap
		 * bytes in words/dwords. Else we couldn't care less
		 * what the file contains. This is consistent with
		 * MS' rc.exe, which doesn't complain at all, even though
		 * the file format might not be entirely correct.
		 */
		rtp++;	/* Skip the "RIFF" tag */

		while((char *)rtp < (char *)rd->data + rd->size)
		{
			if(!memcmp(rtp->tag, acon, sizeof(acon)))
			{
				rtp = SKIP_TAG(rtp,4);
			}
			else if(!memcmp(rtp->tag, list, sizeof(list)))
			{
				handle_ani_list(rtp, type, isswapped);
				rtp = NEXT_TAG(rtp);
			}
			else if(!memcmp(rtp->tag, anih, sizeof(anih)))
			{
				aniheader_t *ahp = (aniheader_t *)((char *)(rtp+1));
				ahp->structsize	= BYTESWAP_DWORD(ahp->structsize);
				ahp->frames	= BYTESWAP_DWORD(ahp->frames);
				ahp->steps	= BYTESWAP_DWORD(ahp->steps);
				ahp->cx		= BYTESWAP_DWORD(ahp->cx);
				ahp->cy		= BYTESWAP_DWORD(ahp->cy);
				ahp->bitcount	= BYTESWAP_DWORD(ahp->bitcount);
				ahp->planes	= BYTESWAP_DWORD(ahp->planes);
				ahp->rate	= BYTESWAP_DWORD(ahp->rate);
				ahp->flags	= BYTESWAP_DWORD(ahp->flags);
				rtp = NEXT_TAG(rtp);
			}
			else if(!memcmp(rtp->tag, rate, sizeof(rate)))
			{
				int cnt = rtp->size / sizeof(DWORD);
				DWORD *dwp = (DWORD *)(rtp+1);
				int i;
				for(i = 0; i < cnt; i++)
					dwp[i] = BYTESWAP_DWORD(dwp[i]);
				rtp = NEXT_TAG(rtp);
			}
			else if(!memcmp(rtp->tag, seq, sizeof(seq)))
			{
				int cnt = rtp->size / sizeof(DWORD);
				DWORD *dwp = (DWORD *)(rtp+1);
				int i;
				for(i = 0; i < cnt; i++)
					dwp[i] = BYTESWAP_DWORD(dwp[i]);
				rtp = NEXT_TAG(rtp);
			}
			else
				internal_error(__FILE__, __LINE__, "Unknown tag \"%c%c%c%c\" in RIFF file\n",
				       isprint(rtp->tag[0]) ? rtp->tag[0] : '.',
				       isprint(rtp->tag[1]) ? rtp->tag[1] : '.',
				       isprint(rtp->tag[2]) ? rtp->tag[2] : '.',
				       isprint(rtp->tag[3]) ? rtp->tag[3] : '.');

			if((UINT_PTR)rtp & 1)
				rtp = SKIP_TAG(rtp,1);
		}

		/* We must end correctly here */
		if((char *)rtp != (char *)rd->data + rd->size)
			parser_error("Animated %s contains invalid field size(s)", anistr);
	}

	ani->data = rd;
	if(memopt)
	{
		ani->memopt = *memopt;
		free(memopt);
	}
	else
		ani->memopt = WRC_MO_MOVEABLE | WRC_MO_DISCARDABLE;
	return ani;
}
#undef NEXT_TAG

/* Bitmaps */
bitmap_t *new_bitmap(raw_data_t *rd, int *memopt)
{
	bitmap_t *bmp = xmalloc(sizeof(bitmap_t));

	bmp->data = rd;
	if(memopt)
	{
		bmp->memopt = *memopt;
		free(memopt);
	}
	else
		bmp->memopt = WRC_MO_MOVEABLE | WRC_MO_PURE;
	rd->size -= convert_bitmap(rd->data, rd->size);
	return bmp;
}

ver_words_t *new_ver_words(int i)
{
	ver_words_t *w = xmalloc(sizeof(ver_words_t));
	w->words = xmalloc(sizeof(WORD));
	w->words[0] = (WORD)i;
	w->nwords = 1;
	return w;
}

ver_words_t *add_ver_words(ver_words_t *w, int i)
{
	w->words = xrealloc(w->words, (w->nwords+1) * sizeof(WORD));
	w->words[w->nwords] = (WORD)i;
	w->nwords++;
	return w;
}

#define MSGTAB_BAD_PTR(p, b, l, r)	(((l) - ((char *)(p) - (char *)(b))) > (r))
messagetable_t *new_messagetable(raw_data_t *rd, int *memopt)
{
	messagetable_t *msg = xmalloc(sizeof(messagetable_t));
 	msgtab_block_t *mbp;
	DWORD nblk;
	DWORD i;
	WORD lo;
	WORD hi;

	msg->data = rd;
	if(memopt)
	{
		msg->memopt = *memopt;
		free(memopt);
	}
	else
		msg->memopt = WRC_MO_MOVEABLE | WRC_MO_PURE;

	if(rd->size < sizeof(DWORD))
		parser_error("Invalid messagetable, size too small");

	nblk = *(DWORD *)rd->data;
	lo = WRC_LOWORD(nblk);
	hi = WRC_HIWORD(nblk);

	/* FIXME:
	 * This test will fail for all n*2^16 blocks in the messagetable.
	 * However, no sane person would want to have so many blocks
	 * and have a table of megabytes attached.
	 * So, I will assume that we have less than 2^16 blocks in the table
	 * and all will just work out fine. Otherwise, we would need to test
	 * the ID, offset and length (and flag) fields to be very sure.
	 */
	if(hi && lo)
		internal_error(__FILE__, __LINE__, "Messagetable contains more than 65535 blocks; cannot determine endian\n");
	if(!hi && !lo)
		parser_error("Invalid messagetable block count 0");

	if(!hi && lo)  /* Messagetable byteorder == native byteorder */
	{
#ifdef WORDS_BIGENDIAN
		if(byteorder != WRC_BO_LITTLE) goto out;
#else
		if(byteorder != WRC_BO_BIG) goto out;
#endif
		/* Resource byteorder != native byteorder */

		mbp = (msgtab_block_t *)&(((DWORD *)rd->data)[1]);
		if(MSGTAB_BAD_PTR(mbp, rd->data, rd->size, nblk * sizeof(*mbp)))
			parser_error("Messagetable's blocks are outside of defined data");
		for(i = 0; i < nblk; i++)
		{
			msgtab_entry_t *mep, *next_mep;
			DWORD id;

			mep = (msgtab_entry_t *)(((char *)rd->data) + mbp[i].offset);

			for(id = mbp[i].idlo; id <= mbp[i].idhi; id++)
			{
				if(MSGTAB_BAD_PTR(mep, rd->data, rd->size, mep->length))
					parser_error("Messagetable's data for block %d, ID 0x%08x is outside of defined data", i, id);
				if(mep->flags == 1)	/* Docu says 'flags == 0x0001' for unicode */
				{
					WORD *wp = (WORD *)&mep[1];
					int l = mep->length/2 - 2; /* Length included header */
					int n;

					if(mep->length & 1)
						parser_error("Message 0x%08x is unicode (block %d), but has odd length (%d)", id, i, mep->length);
					for(n = 0; n < l; n++)
						wp[n] = BYTESWAP_WORD(wp[n]);

				}
				next_mep = (msgtab_entry_t *)(((char *)mep) + mep->length);
				mep->length = BYTESWAP_WORD(mep->length);
				mep->flags  = BYTESWAP_WORD(mep->flags);
				mep = next_mep;
			}

			mbp[i].idlo   = BYTESWAP_DWORD(mbp[i].idlo);
			mbp[i].idhi   = BYTESWAP_DWORD(mbp[i].idhi);
			mbp[i].offset = BYTESWAP_DWORD(mbp[i].offset);
		}
	}
	if(hi && !lo)  /* Messagetable byteorder != native byteorder */
	{
#ifdef WORDS_BIGENDIAN
		if(byteorder == WRC_BO_LITTLE) goto out;
#else
		if(byteorder == WRC_BO_BIG) goto out;
#endif
		/* Resource byteorder == native byteorder */

		mbp = (msgtab_block_t *)&(((DWORD *)rd->data)[1]);
		nblk = BYTESWAP_DWORD(nblk);
		if(MSGTAB_BAD_PTR(mbp, rd->data, rd->size, nblk * sizeof(*mbp)))
			parser_error("Messagetable's blocks are outside of defined data");
		for(i = 0; i < nblk; i++)
		{
			msgtab_entry_t *mep;
			DWORD id;

			mbp[i].idlo   = BYTESWAP_DWORD(mbp[i].idlo);
			mbp[i].idhi   = BYTESWAP_DWORD(mbp[i].idhi);
			mbp[i].offset = BYTESWAP_DWORD(mbp[i].offset);
			mep = (msgtab_entry_t *)(((char *)rd->data) + mbp[i].offset);

			for(id = mbp[i].idlo; id <= mbp[i].idhi; id++)
			{
				mep->length = BYTESWAP_WORD(mep->length);
				mep->flags  = BYTESWAP_WORD(mep->flags);

				if(MSGTAB_BAD_PTR(mep, rd->data, rd->size, mep->length))
					parser_error("Messagetable's data for block %d, ID 0x%08x is outside of defined data", i, id);
				if(mep->flags == 1)	/* Docu says 'flags == 0x0001' for unicode */
				{
					WORD *wp = (WORD *)&mep[1];
					int l = mep->length/2 - 2; /* Length included header */
					int n;

					if(mep->length & 1)
						parser_error("Message 0x%08x is unicode (block %d), but has odd length (%d)", id, i, mep->length);
					for(n = 0; n < l; n++)
						wp[n] = BYTESWAP_WORD(wp[n]);

				}
				mep = (msgtab_entry_t *)(((char *)mep) + mep->length);
			}
		}
	}

 out:
	return msg;
}
#undef MSGTAB_BAD_PTR

void copy_raw_data(raw_data_t *dst, raw_data_t *src, unsigned int offs, int len)
{
	assert(offs <= src->size);
	assert(offs + len <= src->size);
	if(!dst->data)
	{
		dst->data = xmalloc(len);
		dst->size = 0;
	}
	else
		dst->data = xrealloc(dst->data, dst->size + len);
	/* dst->size holds the offset to copy to */
	memcpy(dst->data + dst->size, src->data + offs, len);
	dst->size += len;
}

int *new_int(int i)
{
	int *ip = xmalloc(sizeof(int));
	*ip = i;
	return ip;
}

stringtable_t *new_stringtable(lvc_t *lvc)
{
	stringtable_t *stt = xmalloc(sizeof(stringtable_t));

	memset( stt, 0, sizeof(*stt) );
	if(lvc)
		stt->lvc = *lvc;

	return stt;
}

toolbar_t *new_toolbar(int button_width, int button_height, toolbar_item_t *items, int nitems)
{
	toolbar_t *tb = xmalloc(sizeof(toolbar_t));
	memset( tb, 0, sizeof(*tb) );
	tb->button_width = button_width;
	tb->button_height = button_height;
	tb->nitems = nitems;
	tb->items = items;
	return tb;
}

dlginit_t *new_dlginit(raw_data_t *rd, int *memopt)
{
	dlginit_t *di = xmalloc(sizeof(dlginit_t));
	di->data = rd;
	if(memopt)
	{
		di->memopt = *memopt;
		free(memopt);
	}
	else
		di->memopt = WRC_MO_MOVEABLE | WRC_MO_PURE | WRC_MO_DISCARDABLE;

	return di;
}

style_pair_t *new_style_pair(style_t *style, style_t *exstyle)
{
	style_pair_t *sp = xmalloc(sizeof(style_pair_t));
	sp->style = style;
	sp->exstyle = exstyle;
	return sp;
}

style_t *new_style(DWORD or_mask, DWORD and_mask)
{
	style_t *st = xmalloc(sizeof(style_t));
	st->or_mask = or_mask;
	st->and_mask = and_mask;
	return st;
}
