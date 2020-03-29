/*
 * File macho_module.c - processing of Mach-O files
 *      Originally based on elf_module.c
 *
 * Copyright (C) 1996, Eric Youngdale.
 *               1999-2007 Eric Pouech
 *               2009 Ken Thomases, CodeWeavers Inc.
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

#ifdef HAVE_MACH_O_LOADER_H
#include <CoreFoundation/CFString.h>
#define LoadResource mac_LoadResource
#define GetCurrentThread mac_GetCurrentThread
#include <CoreServices/CoreServices.h>
#undef LoadResource
#undef GetCurrentThread
#endif

#include <stdio.h>
#include <assert.h>
#include <stdarg.h>
#include <errno.h>
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#ifdef HAVE_SYS_MMAN_H
# include <sys/mman.h>
#endif

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "dbghelp_private.h"
#include "winternl.h"
#include "wine/debug.h"
#include "wine/heap.h"
#include "image_private.h"

#ifdef HAVE_MACH_O_LOADER_H

#include <mach-o/fat.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach-o/dyld.h>

struct dyld_image_info32 {
    uint32_t /* const struct mach_header* */    imageLoadAddress;
    uint32_t /* const char* */                  imageFilePath;
    uint32_t /* uintptr_t */                    imageFileModDate;
};

struct dyld_all_image_infos32 {
    uint32_t                                        version;
    uint32_t                                        infoArrayCount;
    uint32_t /* const struct dyld_image_info* */    infoArray;
};

struct dyld_image_info64 {
    uint64_t /* const struct mach_header* */    imageLoadAddress;
    uint64_t /* const char* */                  imageFilePath;
    uint64_t /* uintptr_t */                    imageFileModDate;
};

struct dyld_all_image_infos64 {
    uint32_t                                        version;
    uint32_t                                        infoArrayCount;
    uint64_t /* const struct dyld_image_info* */    infoArray;
};

union wine_image_info {
    struct dyld_image_info32 info32;
    struct dyld_image_info64 info64;
};

union wine_all_image_infos {
    struct dyld_all_image_infos32 infos32;
    struct dyld_all_image_infos64 infos64;
};

#ifdef WORDS_BIGENDIAN
#define swap_ulong_be_to_host(n) (n)
#else
#define swap_ulong_be_to_host(n) (RtlUlongByteSwap(n))
#endif

WINE_DEFAULT_DEBUG_CHANNEL(dbghelp_macho);


/* Bitmask for Mach-O image header flags indicating that the image is in dyld's
   shared cached.  That implies that its segments are mapped non-contiguously.
   This value isn't defined anywhere in headers.  It's used in dyld and in
   debuggers which support OS X as a magic number.

   The flag also isn't set in the on-disk image file.  It's only set in
   memory by dyld. */
#define MACHO_DYLD_IN_SHARED_CACHE 0x80000000


#define UUID_STRING_LEN 37 /* 16 bytes at 2 hex digits apiece, 4 dashes, and the null terminator */


struct macho_module_info
{
    struct image_file_map       file_map;
    ULONG_PTR                   load_addr;
    unsigned short              in_use : 1,
                                is_loader : 1;
};

struct section_info
{
    BOOL            split_segs;
    unsigned int    section_index;
};

#define MACHO_INFO_MODULE         0x0001
#define MACHO_INFO_NAME           0x0002

struct macho_info
{
    unsigned                    flags;          /* IN  one (or several) of the MACHO_INFO constants */
    struct module*              module;         /* OUT loaded module (if MACHO_INFO_MODULE is set) */
    const WCHAR*                module_name;    /* OUT found module name (if MACHO_INFO_NAME is set) */
};

static void macho_unmap_file(struct image_file_map* fmap);

static char* format_uuid(const uint8_t uuid[16], char out[UUID_STRING_LEN])
{
    sprintf(out, "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
            uuid[0], uuid[1], uuid[2], uuid[3], uuid[4], uuid[5], uuid[6], uuid[7],
            uuid[8], uuid[9], uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);
    return out;
}

/******************************************************************
 *              macho_calc_range
 *
 * For a range (offset & length) of a single architecture within
 * a Mach-O file, calculate the page-aligned range of the whole file
 * that encompasses it.  For a fat binary, the architecture will
 * itself be offset within the file, so take that into account.
 */
static void macho_calc_range(const struct macho_file_map* fmap, ULONG_PTR offset,
                             ULONG_PTR len, ULONG_PTR* out_aligned_offset,
                             ULONG_PTR* out_aligned_end, ULONG_PTR* out_misalign)
{
    ULONG_PTR pagemask;
    ULONG_PTR file_offset, misalign;

    pagemask = sysinfo.dwAllocationGranularity - 1;
    file_offset = fmap->arch_offset + offset;
    misalign = file_offset & pagemask;
    *out_aligned_offset = file_offset - misalign;
    *out_aligned_end = file_offset + len;
    if (out_misalign)
        *out_misalign = misalign;
}

/******************************************************************
 *              macho_map_range
 *
 * Maps a range (offset, length in bytes) from a Mach-O file into memory
 */
static const char* macho_map_range(const struct macho_file_map* fmap, ULONG_PTR offset, ULONG_PTR len,
                                   const char** base)
{
    ULONG_PTR       misalign, aligned_offset, aligned_map_end;
    const void*     aligned_ptr;
    HANDLE          mapping;

    TRACE("(%p/%p, 0x%08lx, 0x%08lx)\n", fmap, fmap->handle, offset, len);

    macho_calc_range(fmap, offset, len, &aligned_offset, &aligned_map_end, &misalign);

    if (!(mapping = CreateFileMappingW(fmap->handle, NULL, PAGE_READONLY, 0, 0, NULL)))
    {
        ERR("map creation %p failed %u size %lu\n", fmap->handle, GetLastError(), aligned_map_end);
        return IMAGE_NO_MAP;
    }
    aligned_ptr = MapViewOfFile(mapping, FILE_MAP_READ, 0, aligned_offset, aligned_map_end - aligned_offset);
    CloseHandle(mapping);
    if (!aligned_ptr)
    {
        ERR("map failed %u\n", GetLastError());
        return IMAGE_NO_MAP;
    }

    TRACE("Mapped (0x%08lx - 0x%08lx) to %p\n", aligned_offset, aligned_map_end, aligned_ptr);

    if (base)
        *base = aligned_ptr;
    return (const char*)aligned_ptr + misalign;
}

/******************************************************************
 *              macho_unmap_range
 *
 * Unmaps a range (offset, length in bytes) of a Mach-O file from memory
 */
static void macho_unmap_range(const char** base, const void** mapped, const struct macho_file_map* fmap,
                              ULONG_PTR offset, ULONG_PTR len)
{
    TRACE("(%p, %p, %p/%p, 0x%08lx, 0x%08lx)\n", base, mapped, fmap, fmap->handle, offset, len);

    if ((mapped && *mapped != IMAGE_NO_MAP) || (base && *base != IMAGE_NO_MAP))
    {
        ULONG_PTR       misalign, aligned_offset, aligned_map_end;
        void*           aligned_ptr;

        macho_calc_range(fmap, offset, len, &aligned_offset, &aligned_map_end, &misalign);

        if (mapped)
            aligned_ptr = (char*)*mapped - misalign;
        else
            aligned_ptr = (void*)*base;
        if (!UnmapViewOfFile(aligned_ptr))
            WARN("Couldn't unmap the range\n");
        if (mapped)
            *mapped = IMAGE_NO_MAP;
        if (base)
            *base = IMAGE_NO_MAP;
    }
}

/******************************************************************
 *              macho_map_ranges
 *
 * Maps two ranges (offset, length in bytes) from a Mach-O file
 * into memory.  If the two ranges overlap, use one mmap so that
 * the munmap doesn't fragment the mapping.
 */
static BOOL macho_map_ranges(const struct macho_file_map* fmap,
                             ULONG_PTR offset1, ULONG_PTR len1,
                             ULONG_PTR offset2, ULONG_PTR len2,
                             const void** mapped1, const void** mapped2)
{
    ULONG_PTR aligned_offset1, aligned_map_end1;
    ULONG_PTR aligned_offset2, aligned_map_end2;

    TRACE("(%p/%p, 0x%08lx, 0x%08lx, 0x%08lx, 0x%08lx, %p, %p)\n", fmap, fmap->handle,
            offset1, len1, offset2, len2, mapped1, mapped2);

    macho_calc_range(fmap, offset1, len1, &aligned_offset1, &aligned_map_end1, NULL);
    macho_calc_range(fmap, offset2, len2, &aligned_offset2, &aligned_map_end2, NULL);

    if (aligned_map_end1 < aligned_offset2 || aligned_map_end2 < aligned_offset1)
    {
        *mapped1 = macho_map_range(fmap, offset1, len1, NULL);
        if (*mapped1 != IMAGE_NO_MAP)
        {
            *mapped2 = macho_map_range(fmap, offset2, len2, NULL);
            if (*mapped2 == IMAGE_NO_MAP)
                macho_unmap_range(NULL, mapped1, fmap, offset1, len1);
        }
    }
    else
    {
        if (offset1 < offset2)
        {
            *mapped1 = macho_map_range(fmap, offset1, offset2 + len2 - offset1, NULL);
            if (*mapped1 != IMAGE_NO_MAP)
                *mapped2 = (const char*)*mapped1 + offset2 - offset1;
        }
        else
        {
            *mapped2 = macho_map_range(fmap, offset2, offset1 + len1 - offset2, NULL);
            if (*mapped2 != IMAGE_NO_MAP)
                *mapped1 = (const char*)*mapped2 + offset1 - offset2;
        }
    }

    TRACE(" => %p, %p\n", *mapped1, *mapped2);

    return (*mapped1 != IMAGE_NO_MAP) && (*mapped2 != IMAGE_NO_MAP);
}

/******************************************************************
 *              macho_unmap_ranges
 *
 * Unmaps two ranges (offset, length in bytes) of a Mach-O file
 * from memory.  Use for ranges which were mapped by
 * macho_map_ranges.
 */
static void macho_unmap_ranges(const struct macho_file_map* fmap,
                               ULONG_PTR offset1, ULONG_PTR len1,
                               ULONG_PTR offset2, ULONG_PTR len2,
                               const void** mapped1, const void** mapped2)
{
    ULONG_PTR       aligned_offset1, aligned_map_end1;
    ULONG_PTR       aligned_offset2, aligned_map_end2;

    TRACE("(%p/%p, 0x%08lx, 0x%08lx, 0x%08lx, 0x%08lx, %p/%p, %p/%p)\n", fmap, fmap->handle,
            offset1, len1, offset2, len2, mapped1, *mapped1, mapped2, *mapped2);

    macho_calc_range(fmap, offset1, len1, &aligned_offset1, &aligned_map_end1, NULL);
    macho_calc_range(fmap, offset2, len2, &aligned_offset2, &aligned_map_end2, NULL);

    if (aligned_map_end1 < aligned_offset2 || aligned_map_end2 < aligned_offset1)
    {
        macho_unmap_range(NULL, mapped1, fmap, offset1, len1);
        macho_unmap_range(NULL, mapped2, fmap, offset2, len2);
    }
    else
    {
        if (offset1 < offset2)
        {
            macho_unmap_range(NULL, mapped1, fmap, offset1, offset2 + len2 - offset1);
            *mapped2 = IMAGE_NO_MAP;
        }
        else
        {
            macho_unmap_range(NULL, mapped2, fmap, offset2, offset1 + len1 - offset2);
            *mapped1 = IMAGE_NO_MAP;
        }
    }
}

/******************************************************************
 *              macho_find_section
 */
static BOOL macho_find_segment_section(struct image_file_map* ifm, const char* segname, const char* sectname, struct image_section_map* ism)
{
    struct macho_file_map* fmap;
    unsigned i;
    char tmp[sizeof(fmap->sect[0].section.sectname)];

    /* Other parts of dbghelp use section names like ".eh_frame".  Mach-O uses
       names like "__eh_frame".  Convert those. */
    if (sectname[0] == '.')
    {
        lstrcpynA(tmp, "__", sizeof(tmp));
        lstrcpynA(tmp + 2, sectname + 1, sizeof(tmp) - 2);
        sectname = tmp;
    }

    while (ifm)
    {
        fmap = &ifm->u.macho;
        for (i = 0; i < fmap->num_sections; i++)
        {
            if (!fmap->sect[i].ignored &&
                strcmp(fmap->sect[i].section.sectname, sectname) == 0 &&
                (!segname || strcmp(fmap->sect[i].section.segname, segname) == 0))
            {
                ism->fmap = ifm;
                ism->sidx = i;
                return TRUE;
            }
        }
        ifm = fmap->dsym;
    }

    ism->fmap = NULL;
    ism->sidx = -1;
    return FALSE;
}

static BOOL macho_find_section(struct image_file_map* ifm, const char* sectname, struct image_section_map* ism)
{
    return macho_find_segment_section(ifm, NULL, sectname, ism);
}

/******************************************************************
 *              macho_map_section
 */
const char* macho_map_section(struct image_section_map* ism)
{
    struct macho_file_map* fmap = &ism->fmap->u.macho;

    assert(ism->fmap->modtype == DMT_MACHO);
    if (ism->sidx < 0 || ism->sidx >= ism->fmap->u.macho.num_sections || fmap->sect[ism->sidx].ignored)
        return IMAGE_NO_MAP;

    return macho_map_range(fmap, fmap->sect[ism->sidx].section.offset, fmap->sect[ism->sidx].section.size,
                           &fmap->sect[ism->sidx].mapped);
}

/******************************************************************
 *              macho_unmap_section
 */
void macho_unmap_section(struct image_section_map* ism)
{
    struct macho_file_map* fmap = &ism->fmap->u.macho;

    if (ism->sidx >= 0 && ism->sidx < fmap->num_sections && fmap->sect[ism->sidx].mapped != IMAGE_NO_MAP)
    {
        macho_unmap_range(&fmap->sect[ism->sidx].mapped, NULL, fmap, fmap->sect[ism->sidx].section.offset,
                          fmap->sect[ism->sidx].section.size);
    }
}

/******************************************************************
 *              macho_get_map_rva
 */
DWORD_PTR macho_get_map_rva(const struct image_section_map* ism)
{
    if (ism->sidx < 0 || ism->sidx >= ism->fmap->u.macho.num_sections ||
        ism->fmap->u.macho.sect[ism->sidx].ignored)
        return 0;
    return ism->fmap->u.macho.sect[ism->sidx].section.addr - ism->fmap->u.macho.segs_start;
}

/******************************************************************
 *              macho_get_map_size
 */
unsigned macho_get_map_size(const struct image_section_map* ism)
{
    if (ism->sidx < 0 || ism->sidx >= ism->fmap->u.macho.num_sections ||
        ism->fmap->u.macho.sect[ism->sidx].ignored)
        return 0;
    return ism->fmap->u.macho.sect[ism->sidx].section.size;
}

static const struct image_file_map_ops macho_file_map_ops =
{
    macho_map_section,
    macho_unmap_section,
    macho_find_section,
    macho_get_map_rva,
    macho_get_map_size,
    macho_unmap_file,
};

/******************************************************************
 *              macho_map_load_commands
 *
 * Maps the load commands from a Mach-O file into memory
 */
static const struct load_command* macho_map_load_commands(struct macho_file_map* fmap)
{
    if (fmap->load_commands == IMAGE_NO_MAP)
    {
        fmap->load_commands = (const struct load_command*) macho_map_range(
                fmap, fmap->header_size, fmap->mach_header.sizeofcmds, NULL);
        TRACE("Mapped load commands: %p\n", fmap->load_commands);
    }

    return fmap->load_commands;
}

/******************************************************************
 *              macho_unmap_load_commands
 *
 * Unmaps the load commands of a Mach-O file from memory
 */
static void macho_unmap_load_commands(struct macho_file_map* fmap)
{
    if (fmap->load_commands != IMAGE_NO_MAP)
    {
        TRACE("Unmapping load commands: %p\n", fmap->load_commands);
        macho_unmap_range(NULL, (const void**)&fmap->load_commands, fmap,
                    fmap->header_size, fmap->mach_header.sizeofcmds);
    }
}

/******************************************************************
 *              macho_next_load_command
 *
 * Advance to the next load command
 */
static const struct load_command* macho_next_load_command(const struct load_command* lc)
{
    return (const struct load_command*)((const char*)lc + lc->cmdsize);
}

/******************************************************************
 *              macho_enum_load_commands
 *
 * Enumerates the load commands for a Mach-O file, selecting by
 * the command type, calling a callback for each.  If the callback
 * returns <0, that indicates an error.  If it returns >0, that means
 * it's not interested in getting any more load commands.
 * If this function returns <0, that's an error produced by the
 * callback.  If >=0, that's the count of load commands successfully
 * processed.
 */
static int macho_enum_load_commands(struct image_file_map *ifm, unsigned cmd,
                                    int (*cb)(struct image_file_map*, const struct load_command*, void*),
                                    void* user)
{
    struct macho_file_map* fmap = &ifm->u.macho;
    const struct load_command* lc;
    int i;
    int count = 0;

    TRACE("(%p/%p, %u, %p, %p)\n", fmap, fmap->handle, cmd, cb, user);

    if ((lc = macho_map_load_commands(fmap)) == IMAGE_NO_MAP) return -1;

    TRACE("%d total commands\n", fmap->mach_header.ncmds);

    for (i = 0; i < fmap->mach_header.ncmds; i++, lc = macho_next_load_command(lc))
    {
        int result;

        if (cmd && cmd != lc->cmd) continue;
        count++;

        result = cb(ifm, lc, user);
        TRACE("load_command[%d] (%p), cmd %u; callback => %d\n", i, lc, lc->cmd, result);
        if (result) return (result < 0) ? result : count;
    }

    return count;
}

/******************************************************************
 *              macho_count_sections
 *
 * Callback for macho_enum_load_commands.  Counts the number of
 * significant sections in a Mach-O file.  All commands are
 * expected to be of LC_SEGMENT[_64] type.
 */
static int macho_count_sections(struct image_file_map* ifm, const struct load_command* lc, void* user)
{
    char segname[16];
    uint32_t nsects;

    if (ifm->addr_size == 32)
    {
        const struct segment_command *sc = (const struct segment_command *)lc;
        memcpy(segname, sc->segname, sizeof(segname));
        nsects = sc->nsects;
    }
    else
    {
        const struct segment_command_64 *sc = (const struct segment_command_64 *)lc;
        memcpy(segname, sc->segname, sizeof(segname));
        nsects = sc->nsects;
    }

    TRACE("(%p/%p, %p, %p) segment %s\n", ifm, ifm->u.macho.handle, lc, user,
        debugstr_an(segname, sizeof(segname)));

    ifm->u.macho.num_sections += nsects;
    return 0;
}

/******************************************************************
 *              macho_load_section_info
 *
 * Callback for macho_enum_load_commands.  Accumulates the address
 * range covered by the segments of a Mach-O file and builds the
 * section map.  All commands are expected to be of LC_SEGMENT[_64] type.
 */
static int macho_load_section_info(struct image_file_map* ifm, const struct load_command* lc, void* user)
{
    struct macho_file_map*          fmap = &ifm->u.macho;
    struct section_info*            info = user;
    BOOL                            ignore;
    int                             i;
    ULONG_PTR                       tmp, page_mask = sysinfo.dwPageSize - 1;
    uint64_t vmaddr, vmsize;
    char segname[16];
    uint32_t nsects;
    const void *sections;

    if (ifm->addr_size == 32)
    {
        const struct segment_command *sc = (const struct segment_command *)lc;
        vmaddr = sc->vmaddr;
        vmsize = sc->vmsize;
        memcpy(segname, sc->segname, sizeof(segname));
        nsects = sc->nsects;
        sections = (const void *)(sc + 1);
    }
    else
    {
        const struct segment_command_64 *sc = (const struct segment_command_64 *)lc;
        vmaddr = sc->vmaddr;
        vmsize = sc->vmsize;
        memcpy(segname, sc->segname, sizeof(segname));
        nsects = sc->nsects;
        sections = (const void *)(sc + 1);
    }

    TRACE("(%p/%p, %p, %p) before: 0x%08lx - 0x%08lx\n", fmap, fmap->handle, lc, user,
            (ULONG_PTR)fmap->segs_start, (ULONG_PTR)fmap->segs_size);
    TRACE("Segment command vm: 0x%08lx - 0x%08lx\n", (ULONG_PTR)vmaddr,
            (ULONG_PTR)(vmaddr + vmsize));

    /* Images in the dyld shared cache have their segments mapped non-contiguously.
       We don't know how to properly locate any of the segments other than __TEXT,
       so ignore them. */
    ignore = (info->split_segs && strcmp(segname, SEG_TEXT));

    if (!strncmp(segname, "WINE_", 5))
        TRACE("Ignoring special Wine segment %s\n", debugstr_an(segname, sizeof(segname)));
    else if (!strncmp(segname, "__PAGEZERO", 10))
        TRACE("Ignoring __PAGEZERO segment\n");
    else if (ignore)
        TRACE("Ignoring %s segment because image has split segments\n", segname);
    else
    {
        /* If this segment starts before previously-known earliest, record new earliest. */
        if (vmaddr < fmap->segs_start)
            fmap->segs_start = vmaddr;

        /* If this segment extends beyond previously-known furthest, record new furthest. */
        tmp = (vmaddr + vmsize + page_mask) & ~page_mask;
        if (fmap->segs_size < tmp) fmap->segs_size = tmp;

        TRACE("after: 0x%08lx - 0x%08lx\n", (ULONG_PTR)fmap->segs_start, (ULONG_PTR)fmap->segs_size);
    }

    for (i = 0; i < nsects; i++)
    {
        if (ifm->addr_size == 32)
        {
            const struct section *section = &((const struct section *)sections)[i];
            memcpy(fmap->sect[info->section_index].section.sectname, section->sectname, sizeof(section->sectname));
            memcpy(fmap->sect[info->section_index].section.segname,  section->segname,  sizeof(section->segname));
            fmap->sect[info->section_index].section.addr      = section->addr;
            fmap->sect[info->section_index].section.size      = section->size;
            fmap->sect[info->section_index].section.offset    = section->offset;
            fmap->sect[info->section_index].section.align     = section->align;
            fmap->sect[info->section_index].section.reloff    = section->reloff;
            fmap->sect[info->section_index].section.nreloc    = section->nreloc;
            fmap->sect[info->section_index].section.flags     = section->flags;
        }
        else
            fmap->sect[info->section_index].section = ((const struct section_64 *)sections)[i];

        fmap->sect[info->section_index].mapped = IMAGE_NO_MAP;
        fmap->sect[info->section_index].ignored = ignore;
        info->section_index++;
    }

    return 0;
}

/******************************************************************
 *              find_uuid
 *
 * Callback for macho_enum_load_commands.  Records the UUID load
 * command of a Mach-O file.
 */
static int find_uuid(struct image_file_map* ifm, const struct load_command* lc, void* user)
{
    ifm->u.macho.uuid = (const struct uuid_command*)lc;
    return 1;
}

/******************************************************************
 *              reset_file_map
 */
static inline void reset_file_map(struct image_file_map* ifm)
{
    struct macho_file_map* fmap = &ifm->u.macho;

    fmap->handle = INVALID_HANDLE_VALUE;
    fmap->dsym = NULL;
    fmap->load_commands = IMAGE_NO_MAP;
    fmap->uuid = NULL;
    fmap->num_sections = 0;
    fmap->sect = NULL;
}

/******************************************************************
 *              macho_map_file
 *
 * Maps a Mach-O file into memory (and checks it's a real Mach-O file)
 */
static BOOL macho_map_file(struct process *pcs, const WCHAR *filenameW,
    BOOL split_segs, struct image_file_map* ifm)
{
    struct macho_file_map* fmap = &ifm->u.macho;
    struct fat_header   fat_header;
    int                 i;
    WCHAR*              filename;
    struct section_info info;
    BOOL                ret = FALSE;
    cpu_type_t target_cpu = (pcs->is_64bit) ? CPU_TYPE_X86_64 : CPU_TYPE_X86;
    uint32_t target_magic = (pcs->is_64bit) ? MH_MAGIC_64 : MH_MAGIC;
    uint32_t target_cmd   = (pcs->is_64bit) ? LC_SEGMENT_64 : LC_SEGMENT;
    DWORD bytes_read;

    TRACE("(%s, %p)\n", debugstr_w(filenameW), fmap);

    reset_file_map(ifm);

    ifm->modtype = DMT_MACHO;
    ifm->ops = &macho_file_map_ops;
    ifm->alternate = NULL;
    ifm->addr_size = (pcs->is_64bit) ? 64 : 32;
    fmap->header_size = (pcs->is_64bit) ? sizeof(struct mach_header_64) : sizeof(struct mach_header);

    if (!(filename = get_dos_file_name(filenameW))) return FALSE;

    /* Now open the file, so that we can map it. */
    fmap->handle = CreateFileW(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (fmap->handle == INVALID_HANDLE_VALUE)
    {
        TRACE("failed to open file %s: %d\n", debugstr_w(filename), errno);
        goto done;
    }

    if (!ReadFile(fmap->handle, &fat_header, sizeof(fat_header), &bytes_read, NULL) || bytes_read != sizeof(fat_header))
    {
        TRACE("failed to read fat header: %u\n", GetLastError());
        goto done;
    }
    TRACE("... got possible fat header\n");

    /* Fat header is always in big-endian order. */
    if (swap_ulong_be_to_host(fat_header.magic) == FAT_MAGIC)
    {
        int narch = swap_ulong_be_to_host(fat_header.nfat_arch);
        for (i = 0; i < narch; i++)
        {
            struct fat_arch fat_arch;
            if (!ReadFile(fmap->handle, &fat_arch, sizeof(fat_arch), &bytes_read, NULL) || bytes_read != sizeof(fat_arch))
                goto done;
            if (swap_ulong_be_to_host(fat_arch.cputype) == target_cpu)
            {
                fmap->arch_offset = swap_ulong_be_to_host(fat_arch.offset);
                break;
            }
        }
        if (i >= narch) goto done;
        TRACE("... found target arch (%d)\n", target_cpu);
    }
    else
    {
        fmap->arch_offset = 0;
        TRACE("... not a fat header\n");
    }

    /* Individual architecture (standalone or within a fat file) is in its native byte order. */
    SetFilePointer(fmap->handle, fmap->arch_offset, 0, FILE_BEGIN);
    if (!ReadFile(fmap->handle, &fmap->mach_header, sizeof(fmap->mach_header), &bytes_read, NULL)
        || bytes_read != sizeof(fmap->mach_header))
        goto done;
    TRACE("... got possible Mach header\n");
    /* and check for a Mach-O header */
    if (fmap->mach_header.magic != target_magic ||
        fmap->mach_header.cputype != target_cpu) goto done;
    /* Make sure the file type is one of the ones we expect. */
    switch (fmap->mach_header.filetype)
    {
        case MH_EXECUTE:
        case MH_DYLIB:
        case MH_DYLINKER:
        case MH_BUNDLE:
        case MH_DSYM:
            break;
        default:
            goto done;
    }
    TRACE("... verified Mach header\n");

    fmap->num_sections = 0;
    if (macho_enum_load_commands(ifm, target_cmd, macho_count_sections, NULL) < 0)
        goto done;
    TRACE("%d sections\n", fmap->num_sections);

    fmap->sect = HeapAlloc(GetProcessHeap(), 0, fmap->num_sections * sizeof(fmap->sect[0]));
    if (!fmap->sect)
        goto done;

    fmap->segs_size = 0;
    fmap->segs_start = ~0L;

    info.split_segs = split_segs;
    info.section_index = 0;
    if (macho_enum_load_commands(ifm, target_cmd, macho_load_section_info, &info) < 0)
    {
        fmap->num_sections = 0;
        goto done;
    }

    fmap->segs_size -= fmap->segs_start;
    TRACE("segs_start: 0x%08lx, segs_size: 0x%08lx\n", (ULONG_PTR)fmap->segs_start,
            (ULONG_PTR)fmap->segs_size);

    if (macho_enum_load_commands(ifm, LC_UUID, find_uuid, NULL) < 0)
        goto done;
    if (fmap->uuid)
    {
        char uuid_string[UUID_STRING_LEN];
        TRACE("UUID %s\n", format_uuid(fmap->uuid->uuid, uuid_string));
    }
    else
        TRACE("no UUID found\n");

    ret = TRUE;
done:
    if (!ret)
        macho_unmap_file(ifm);
    HeapFree(GetProcessHeap(), 0, filename);
    return ret;
}

/******************************************************************
 *              macho_unmap_file
 *
 * Unmaps a Mach-O file from memory (previously mapped with macho_map_file)
 */
static void macho_unmap_file(struct image_file_map* ifm)
{
    struct image_file_map* cursor;

    TRACE("(%p/%p)\n", ifm, ifm->u.macho.handle);

    cursor = ifm;
    while (cursor)
    {
        struct image_file_map* next;

        if (ifm->u.macho.handle != INVALID_HANDLE_VALUE)
        {
            struct image_section_map ism;

            ism.fmap = ifm;
            for (ism.sidx = 0; ism.sidx < ifm->u.macho.num_sections; ism.sidx++)
                macho_unmap_section(&ism);

            HeapFree(GetProcessHeap(), 0, ifm->u.macho.sect);
            macho_unmap_load_commands(&ifm->u.macho);
            CloseHandle(ifm->u.macho.handle);
            ifm->u.macho.handle = INVALID_HANDLE_VALUE;
        }

        next = cursor->u.macho.dsym;
        if (cursor != ifm)
            HeapFree(GetProcessHeap(), 0, cursor);
        cursor = next;
    }
}

/******************************************************************
 *              macho_sect_is_code
 *
 * Checks if a section, identified by sectidx which is a 1-based
 * index into the sections of all segments, in order of load
 * commands, contains code.
 */
static BOOL macho_sect_is_code(struct macho_file_map* fmap, unsigned char sectidx)
{
    BOOL ret;

    TRACE("(%p/%p, %u)\n", fmap, fmap->handle, sectidx);

    if (!sectidx) return FALSE;

    sectidx--; /* convert from 1-based to 0-based */
    if (sectidx >= fmap->num_sections || fmap->sect[sectidx].ignored) return FALSE;

    ret = (!(fmap->sect[sectidx].section.flags & SECTION_TYPE) &&
           (fmap->sect[sectidx].section.flags & (S_ATTR_PURE_INSTRUCTIONS|S_ATTR_SOME_INSTRUCTIONS)));
    TRACE("-> %d\n", ret);
    return ret;
}

struct symtab_elt
{
    struct hash_table_elt       ht_elt;
    struct symt_compiland*      compiland;
    ULONG_PTR                   addr;
    unsigned char               is_code:1,
                                is_public:1,
                                is_global:1,
                                used:1;
};

struct macho_debug_info
{
    struct macho_file_map*      fmap;
    struct module*              module;
    struct pool                 pool;
    struct hash_table           ht_symtab;
};

/******************************************************************
 *              macho_stabs_def_cb
 *
 * Callback for stabs_parse.  Collect symbol definitions.
 */
static void macho_stabs_def_cb(struct module* module, ULONG_PTR load_offset,
                               const char* name, ULONG_PTR offset,
                               BOOL is_public, BOOL is_global, unsigned char sectidx,
                               struct symt_compiland* compiland, void* user)
{
    struct macho_debug_info*    mdi = user;
    struct symtab_elt*          ste;

    TRACE("(%p, 0x%08lx, %s, 0x%08lx, %d, %d, %u, %p, %p/%p/%p)\n", module, load_offset,
            debugstr_a(name), offset, is_public, is_global, sectidx,
            compiland, mdi, mdi->fmap, mdi->fmap->handle);

    /* Defer the creation of new non-debugging symbols until after we've
     * finished parsing the stabs. */
    ste                 = pool_alloc(&mdi->pool, sizeof(*ste));
    ste->ht_elt.name    = pool_strdup(&mdi->pool, name);
    ste->compiland      = compiland;
    ste->addr           = load_offset + offset;
    ste->is_code        = !!macho_sect_is_code(mdi->fmap, sectidx);
    ste->is_public      = !!is_public;
    ste->is_global      = !!is_global;
    ste->used           = 0;
    hash_table_add(&mdi->ht_symtab, &ste->ht_elt);
}

/******************************************************************
 *              macho_parse_symtab
 *
 * Callback for macho_enum_load_commands.  Processes the LC_SYMTAB
 * load commands from the Mach-O file.
 */
static int macho_parse_symtab(struct image_file_map* ifm,
                              const struct load_command* lc, void* user)
{
    struct macho_file_map* fmap = &ifm->u.macho;
    const struct symtab_command*    sc = (const struct symtab_command*)lc;
    struct macho_debug_info*        mdi = user;
    const char*                     stabstr;
    int                             ret = 0;
    size_t stabsize = (ifm->addr_size == 32) ? sizeof(struct nlist) : sizeof(struct nlist_64);
    const char *stab;

    TRACE("(%p/%p, %p, %p) %u syms at 0x%08x, strings 0x%08x - 0x%08x\n", fmap, fmap->handle, lc,
            user, sc->nsyms, sc->symoff, sc->stroff, sc->stroff + sc->strsize);

    if (!macho_map_ranges(fmap, sc->symoff, sc->nsyms * stabsize,
            sc->stroff, sc->strsize, (const void**)&stab, (const void**)&stabstr))
        return 0;

    if (!stabs_parse(mdi->module,
                     mdi->module->format_info[DFI_MACHO]->u.macho_info->load_addr - fmap->segs_start,
                     stab, sc->nsyms * stabsize,
                     stabstr, sc->strsize, macho_stabs_def_cb, mdi))
        ret = -1;

    macho_unmap_ranges(fmap, sc->symoff, sc->nsyms * stabsize,
            sc->stroff, sc->strsize, (const void**)&stab, (const void**)&stabstr);

    return ret;
}

/******************************************************************
 *              macho_finish_stabs
 *
 * Integrate the non-debugging symbols we've gathered into the
 * symbols that were generated during stabs parsing.
 */
static void macho_finish_stabs(struct module* module, struct hash_table* ht_symtab)
{
    struct hash_table_iter      hti_ours;
    struct symtab_elt*          ste;
    BOOL                        adjusted = FALSE;

    TRACE("(%p, %p)\n", module, ht_symtab);

    /* For each of our non-debugging symbols, see if it can provide some
     * missing details to one of the module's known symbols. */
    hash_table_iter_init(ht_symtab, &hti_ours, NULL);
    while ((ste = hash_table_iter_up(&hti_ours)))
    {
        struct hash_table_iter  hti_modules;
        void*                   ptr;
        struct symt_ht*         sym;
        struct symt_function*   func;
        struct symt_data*       data;

        hash_table_iter_init(&module->ht_symbols, &hti_modules, ste->ht_elt.name);
        while ((ptr = hash_table_iter_up(&hti_modules)))
        {
            sym = CONTAINING_RECORD(ptr, struct symt_ht, hash_elt);

            if (strcmp(sym->hash_elt.name, ste->ht_elt.name))
                continue;

            switch (sym->symt.tag)
            {
            case SymTagFunction:
                func = (struct symt_function*)sym;
                if (func->address == module->format_info[DFI_MACHO]->u.macho_info->load_addr)
                {
                    TRACE("Adjusting function %p/%s!%s from 0x%08lx to 0x%08lx\n", func,
                          debugstr_w(module->module.ModuleName), sym->hash_elt.name,
                          func->address, ste->addr);
                    func->address = ste->addr;
                    adjusted = TRUE;
                }
                if (func->address == ste->addr)
                    ste->used = 1;
                break;
            case SymTagData:
                data = (struct symt_data*)sym;
                switch (data->kind)
                {
                case DataIsGlobal:
                case DataIsFileStatic:
                    if (data->u.var.offset == module->format_info[DFI_MACHO]->u.macho_info->load_addr)
                    {
                        TRACE("Adjusting data symbol %p/%s!%s from 0x%08lx to 0x%08lx\n",
                              data, debugstr_w(module->module.ModuleName), sym->hash_elt.name,
                              data->u.var.offset, ste->addr);
                        data->u.var.offset = ste->addr;
                        adjusted = TRUE;
                    }
                    if (data->u.var.offset == ste->addr)
                    {
                        enum DataKind new_kind;

                        new_kind = ste->is_global ? DataIsGlobal : DataIsFileStatic;
                        if (data->kind != new_kind)
                        {
                            WARN("Changing kind for %p/%s!%s from %d to %d\n", sym,
                                 debugstr_w(module->module.ModuleName), sym->hash_elt.name,
                                 (int)data->kind, (int)new_kind);
                            data->kind = new_kind;
                            adjusted = TRUE;
                        }
                        ste->used = 1;
                    }
                    break;
                default:;
                }
                break;
            default:
                TRACE("Ignoring tag %u\n", sym->symt.tag);
                break;
            }
        }
    }

    if (adjusted)
    {
        /* since we may have changed some addresses, mark the module to be resorted */
        module->sortlist_valid = FALSE;
    }

    /* Mark any of our non-debugging symbols which fall on an already-used
     * address as "used".  This allows us to skip them in the next loop,
     * below.  We do this in separate loops because symt_new_* marks the
     * list as needing sorting and symt_find_nearest sorts if needed,
     * causing thrashing. */
    if (!(dbghelp_options & SYMOPT_PUBLICS_ONLY))
    {
        hash_table_iter_init(ht_symtab, &hti_ours, NULL);
        while ((ste = hash_table_iter_up(&hti_ours)))
        {
            struct symt_ht* sym;
            ULONG64         addr;

            if (ste->used) continue;

            sym = symt_find_nearest(module, ste->addr);
            if (sym)
                symt_get_address(&sym->symt, &addr);
            if (sym && ste->addr == addr)
            {
                ULONG64 size = 0;
                DWORD   kind = -1;

                ste->used = 1;

                /* If neither symbol has a correct size (ours never does), we
                 * consider them both to be markers.  No warning is needed in
                 * that case.
                 * Also, we check that we don't have two symbols, one local, the other
                 * global, which is legal.
                 */
                symt_get_info(module, &sym->symt, TI_GET_LENGTH,   &size);
                symt_get_info(module, &sym->symt, TI_GET_DATAKIND, &kind);
                if (size && kind == (ste->is_global ? DataIsGlobal : DataIsFileStatic))
                    FIXME("Duplicate in %s: %s<%08lx> %s<%s-%s>\n",
                          debugstr_w(module->module.ModuleName),
                          ste->ht_elt.name, ste->addr,
                          sym->hash_elt.name,
                          wine_dbgstr_longlong(addr), wine_dbgstr_longlong(size));
            }
        }
    }

    /* For any of our remaining non-debugging symbols which have no match
     * among the module's known symbols, add them as new symbols. */
    hash_table_iter_init(ht_symtab, &hti_ours, NULL);
    while ((ste = hash_table_iter_up(&hti_ours)))
    {
        if (!(dbghelp_options & SYMOPT_PUBLICS_ONLY) && !ste->used)
        {
            if (ste->is_code)
            {
                symt_new_function(module, ste->compiland, ste->ht_elt.name,
                    ste->addr, 0, NULL);
            }
            else
            {
                struct location loc;

                loc.kind = loc_absolute;
                loc.reg = 0;
                loc.offset = ste->addr;
                symt_new_global_variable(module, ste->compiland, ste->ht_elt.name,
                                         !ste->is_global, loc, 0, NULL);
            }

            ste->used = 1;
        }

        if (ste->is_public && !(dbghelp_options & SYMOPT_NO_PUBLICS))
        {
            symt_new_public(module, ste->compiland, ste->ht_elt.name, ste->is_code, ste->addr, 0);
        }
    }
}

/******************************************************************
 *              try_dsym
 *
 * Try to load a debug symbol file from the given path and check
 * if its UUID matches the UUID of an already-mapped file.  If so,
 * stash the file map in the "dsym" field of the file and return
 * TRUE.  If it can't be mapped or its UUID doesn't match, return
 * FALSE.
 */
static BOOL try_dsym(struct process *pcs, const WCHAR* path, struct macho_file_map* fmap)
{
    struct image_file_map dsym_ifm;

    if (macho_map_file(pcs, path, FALSE, &dsym_ifm))
    {
        char uuid_string[UUID_STRING_LEN];

        if (dsym_ifm.u.macho.uuid && !memcmp(dsym_ifm.u.macho.uuid->uuid, fmap->uuid->uuid, sizeof(fmap->uuid->uuid)))
        {
            TRACE("found matching debug symbol file at %s\n", debugstr_w(path));
            fmap->dsym = HeapAlloc(GetProcessHeap(), 0, sizeof(dsym_ifm));
            *fmap->dsym = dsym_ifm;
            return TRUE;
        }

        TRACE("candidate debug symbol file at %s has wrong UUID %s; ignoring\n", debugstr_w(path),
              format_uuid(dsym_ifm.u.macho.uuid->uuid, uuid_string));

        macho_unmap_file(&dsym_ifm);
    }
    else
        TRACE("couldn't map file at %s\n", debugstr_w(path));

    return FALSE;
}

/******************************************************************
 *              find_and_map_dsym
 *
 * Search for a debugging symbols file associated with a module and
 * map it.  First look for a .dSYM bundle next to the module file
 * (e.g. <path>.dSYM/Contents/Resources/DWARF/<basename of path>)
 * as produced by dsymutil.  Next, look for a .dwarf file next to
 * the module file (e.g. <path>.dwarf) as produced by
 * "dsymutil --flat".  Finally, use Spotlight to search for a
 * .dSYM bundle with the same UUID as the module file.
 */
static void find_and_map_dsym(struct process *pcs, struct module* module)
{
    static const WCHAR dot_dsym[] = {'.','d','S','Y','M',0};
    static const WCHAR dsym_subpath[] = {'/','C','o','n','t','e','n','t','s','/','R','e','s','o','u','r','c','e','s','/','D','W','A','R','F','/',0};
    static const WCHAR dot_dwarf[] = {'.','d','w','a','r','f',0};
    struct macho_file_map* fmap = &module->format_info[DFI_MACHO]->u.macho_info->file_map.u.macho;
    const WCHAR* p;
    size_t len;
    WCHAR* path = NULL;
    char uuid_string[UUID_STRING_LEN];
    CFStringRef uuid_cfstring;
    CFStringRef query_string;
    MDQueryRef query = NULL;

    /* Without a UUID, we can't verify that any debug info file we find corresponds
       to this file.  Better to have no debug info than incorrect debug info. */
    if (!fmap->uuid)
        return;

    p = file_name(module->module.LoadedImageName);
    len = strlenW(module->module.LoadedImageName) + strlenW(dot_dsym) + strlenW(dsym_subpath) + strlenW(p) + 1;
    path = HeapAlloc(GetProcessHeap(), 0, len * sizeof(WCHAR));
    if (!path)
        return;
    strcpyW(path, module->module.LoadedImageName);
    strcatW(path, dot_dsym);
    strcatW(path, dsym_subpath);
    strcatW(path, p);

    if (try_dsym(pcs, path, fmap))
        goto found;

    strcpyW(path + strlenW(module->module.LoadedImageName), dot_dwarf);

    if (try_dsym(pcs, path, fmap))
        goto found;

    format_uuid(fmap->uuid->uuid, uuid_string);
    uuid_cfstring = CFStringCreateWithCString(NULL, uuid_string, kCFStringEncodingASCII);
    query_string = CFStringCreateWithFormat(NULL, NULL, CFSTR("com_apple_xcode_dsym_uuids == \"%@\""), uuid_cfstring);
    CFRelease(uuid_cfstring);
    query = MDQueryCreate(NULL, query_string, NULL, NULL);
    CFRelease(query_string);
    MDQuerySetMaxCount(query, 1);
    if (MDQueryExecute(query, kMDQuerySynchronous) && MDQueryGetResultCount(query) >= 1)
    {
        MDItemRef item = (MDItemRef)MDQueryGetResultAtIndex(query, 0);
        CFStringRef item_path = MDItemCopyAttribute(item, kMDItemPath);
        if (item_path)
        {
            CFIndex item_path_len = CFStringGetLength(item_path);
            if (item_path_len + strlenW(dsym_subpath) + strlenW(p) >= len)
            {
                HeapFree(GetProcessHeap(), 0, path);
                len = item_path_len + strlenW(dsym_subpath) + strlenW(p) + 1;
                path = HeapAlloc(GetProcessHeap(), 0, len * sizeof(WCHAR));
            }
            CFStringGetCharacters(item_path, CFRangeMake(0, item_path_len), (UniChar*)path);
            strcpyW(path + item_path_len, dsym_subpath);
            strcatW(path, p);
            CFRelease(item_path);

            if (try_dsym(pcs, path, fmap))
                goto found;
        }
    }

found:
    HeapFree(GetProcessHeap(), 0, path);
    if (query) CFRelease(query);
}

/******************************************************************
 *              image_uses_split_segs
 *
 * Determine if the Mach-O image loaded at a particular address in
 * the given process is in the dyld shared cache and therefore has
 * its segments mapped non-contiguously.
 *
 * The image header has to be loaded from the process's memory
 * because the relevant flag is only set in memory, not in the file.
 */
static BOOL image_uses_split_segs(struct process* process, ULONG_PTR load_addr)
{
    BOOL split_segs = FALSE;

    if (load_addr)
    {
        cpu_type_t target_cpu = (process->is_64bit) ? CPU_TYPE_X86_64 : CPU_TYPE_X86;
        uint32_t target_magic = (process->is_64bit) ? MH_MAGIC_64 : MH_MAGIC;
        struct mach_header header;

        if (ReadProcessMemory(process->handle, (void*)load_addr, &header, sizeof(header), NULL) &&
            header.magic == target_magic && header.cputype == target_cpu &&
            header.flags & MACHO_DYLD_IN_SHARED_CACHE)
        {
            split_segs = TRUE;
        }
    }

    return split_segs;
}

/******************************************************************
 *              macho_load_debug_info
 *
 * Loads Mach-O debugging information from the module image file.
 */
static BOOL macho_load_debug_info(struct process *pcs, struct module* module)
{
    BOOL                    ret = FALSE;
    struct macho_debug_info mdi;
    int                     result;
    struct image_file_map  *ifm;
    struct macho_file_map  *fmap;

    if (module->type != DMT_MACHO || !module->format_info[DFI_MACHO]->u.macho_info)
    {
        ERR("Bad Mach-O module '%s'\n", debugstr_w(module->module.LoadedImageName));
        return FALSE;
    }

    ifm = &module->format_info[DFI_MACHO]->u.macho_info->file_map;
    fmap = &ifm->u.macho;

    TRACE("(%p, %p/%p)\n", module, fmap, fmap->handle);

    module->module.SymType = SymExport;

    if (!(dbghelp_options & SYMOPT_PUBLICS_ONLY))
    {
        find_and_map_dsym(pcs, module);

        if (dwarf2_parse(module, module->reloc_delta, NULL /* FIXME: some thunks to deal with ? */,
                         &module->format_info[DFI_MACHO]->u.macho_info->file_map))
            ret = TRUE;
    }

    mdi.fmap = fmap;
    mdi.module = module;
    pool_init(&mdi.pool, 65536);
    hash_table_init(&mdi.pool, &mdi.ht_symtab, 256);
    result = macho_enum_load_commands(ifm, LC_SYMTAB, macho_parse_symtab, &mdi);
    if (result > 0)
        ret = TRUE;
    else if (result < 0)
        WARN("Couldn't correctly read stabs\n");

    if (!(dbghelp_options & SYMOPT_PUBLICS_ONLY) && fmap->dsym)
    {
        mdi.fmap = &fmap->dsym->u.macho;
        result = macho_enum_load_commands(fmap->dsym, LC_SYMTAB, macho_parse_symtab, &mdi);
        if (result > 0)
            ret = TRUE;
        else if (result < 0)
            WARN("Couldn't correctly read stabs\n");
    }

    macho_finish_stabs(module, &mdi.ht_symtab);

    pool_destroy(&mdi.pool);
    return ret;
}

/******************************************************************
 *              macho_fetch_file_info
 *
 * Gathers some more information for a Mach-O module from a given file
 */
static BOOL macho_fetch_file_info(struct process* process, const WCHAR* name, ULONG_PTR load_addr, DWORD_PTR* base,
                                  DWORD* size, DWORD* checksum)
{
    struct image_file_map fmap;
    BOOL split_segs;

    TRACE("(%s, %p, %p, %p)\n", debugstr_w(name), base, size, checksum);

    split_segs = image_uses_split_segs(process, load_addr);
    if (!macho_map_file(process, name, split_segs, &fmap)) return FALSE;
    if (base) *base = fmap.u.macho.segs_start;
    *size = fmap.u.macho.segs_size;
    *checksum = calc_crc32(fmap.u.macho.handle);
    macho_unmap_file(&fmap);
    return TRUE;
}

/******************************************************************
 *              macho_module_remove
 */
static void macho_module_remove(struct process* pcs, struct module_format* modfmt)
{
    macho_unmap_file(&modfmt->u.macho_info->file_map);
    HeapFree(GetProcessHeap(), 0, modfmt);
}

/******************************************************************
 *              get_dyld_image_info_address
 */
static ULONG_PTR get_dyld_image_info_address(struct process* pcs)
{
    ULONG_PTR dyld_image_info_address = 0;

#ifndef __LP64__ /* No reading the symtab with nlist(3) in LP64 */
    if (!dyld_image_info_address)
    {
        static void* dyld_all_image_infos_addr;

        /* Our next best guess is that dyld was loaded at its base address
           and we can find the dyld image infos address by looking up its symbol. */
        if (!dyld_all_image_infos_addr)
        {
            struct nlist nl[2];
            memset(nl, 0, sizeof(nl));
            nl[0].n_un.n_name = (char*)"_dyld_all_image_infos";
            if (!nlist("/usr/lib/dyld", nl))
                dyld_all_image_infos_addr = (void*)nl[0].n_value;
        }

        if (dyld_all_image_infos_addr)
        {
            TRACE("got dyld_image_info_address %p from /usr/lib/dyld symbol table\n",
                  dyld_all_image_infos_addr);
            dyld_image_info_address = (ULONG_PTR)dyld_all_image_infos_addr;
        }
    }
#endif

    return dyld_image_info_address;
}

/******************************************************************
 *              macho_load_file
 *
 * Loads the information for Mach-O module stored in 'filename'.
 * The module has been loaded at 'load_addr' address.
 * returns
 *      FALSE if the file cannot be found/opened or if the file doesn't
 *              contain symbolic info (or this info cannot be read or parsed)
 *      TRUE on success
 */
static BOOL macho_load_file(struct process* pcs, const WCHAR* filename,
                            ULONG_PTR load_addr, struct macho_info* macho_info)
{
    BOOL                    ret = TRUE;
    BOOL                    split_segs;
    struct image_file_map   fmap;

    TRACE("(%p/%p, %s, 0x%08lx, %p/0x%08x)\n", pcs, pcs->handle, debugstr_w(filename),
            load_addr, macho_info, macho_info->flags);

    split_segs = image_uses_split_segs(pcs, load_addr);
    if (!macho_map_file(pcs, filename, split_segs, &fmap)) return FALSE;

    if (macho_info->flags & MACHO_INFO_MODULE)
    {
        struct macho_module_info *macho_module_info;
        struct module_format*   modfmt =
            HeapAlloc(GetProcessHeap(), 0, sizeof(struct module_format) + sizeof(struct macho_module_info));
        if (!modfmt) goto leave;
        if (!load_addr)
            load_addr = fmap.u.macho.segs_start;
        macho_info->module = module_new(pcs, filename, DMT_MACHO, FALSE, load_addr,
                                        fmap.u.macho.segs_size, 0, calc_crc32(fmap.u.macho.handle));
        if (!macho_info->module)
        {
            HeapFree(GetProcessHeap(), 0, modfmt);
            goto leave;
        }
        macho_info->module->reloc_delta = macho_info->module->module.BaseOfImage - fmap.u.macho.segs_start;
        macho_module_info = (void*)(modfmt + 1);
        macho_info->module->format_info[DFI_MACHO] = modfmt;

        modfmt->module       = macho_info->module;
        modfmt->remove       = macho_module_remove;
        modfmt->loc_compute  = NULL;
        modfmt->u.macho_info = macho_module_info;

        macho_module_info->load_addr = load_addr;

        macho_module_info->file_map = fmap;
        reset_file_map(&fmap);
        if (dbghelp_options & SYMOPT_DEFERRED_LOADS)
            macho_info->module->module.SymType = SymDeferred;
        else if (!macho_load_debug_info(pcs, macho_info->module))
            ret = FALSE;

        macho_info->module->format_info[DFI_MACHO]->u.macho_info->in_use = 1;
        macho_info->module->format_info[DFI_MACHO]->u.macho_info->is_loader = 0;
        TRACE("module = %p\n", macho_info->module);
    }

    if (macho_info->flags & MACHO_INFO_NAME)
    {
        WCHAR*  ptr;
        ptr = HeapAlloc(GetProcessHeap(), 0, (lstrlenW(filename) + 1) * sizeof(WCHAR));
        if (ptr)
        {
            strcpyW(ptr, filename);
            macho_info->module_name = ptr;
        }
        else ret = FALSE;
        TRACE("module_name = %p %s\n", macho_info->module_name, debugstr_w(macho_info->module_name));
    }
leave:
    macho_unmap_file(&fmap);

    TRACE(" => %d\n", ret);
    return ret;
}

struct macho_load_params
{
    struct process *process;
    ULONG_PTR load_addr;
    struct macho_info *macho_info;
};

static BOOL macho_load_file_cb(void *param, HANDLE handle, const WCHAR *filename)
{
    struct macho_load_params *macho_load = param;
    return macho_load_file(macho_load->process, filename, macho_load->load_addr, macho_load->macho_info);
}

/******************************************************************
 *              macho_search_and_load_file
 *
 * Lookup a file in standard Mach-O locations, and if found, load it
 */
static BOOL macho_search_and_load_file(struct process* pcs, const WCHAR* filename,
                                       ULONG_PTR load_addr,
                                       struct macho_info* macho_info)
{
    BOOL                ret = FALSE;
    struct module*      module;
    static const WCHAR  S_libstdcPPW[] = {'l','i','b','s','t','d','c','+','+','\0'};
    const WCHAR*        p;
    struct macho_load_params load_params;

    TRACE("(%p/%p, %s, 0x%08lx, %p)\n", pcs, pcs->handle, debugstr_w(filename), load_addr,
            macho_info);

    if (filename == NULL || *filename == '\0') return FALSE;
    if ((module = module_is_already_loaded(pcs, filename)))
    {
        macho_info->module = module;
        module->format_info[DFI_MACHO]->u.macho_info->in_use = 1;
        return module->module.SymType;
    }

    if (strstrW(filename, S_libstdcPPW)) return FALSE; /* We know we can't do it */

    load_params.process    = pcs;
    load_params.load_addr  = load_addr;
    load_params.macho_info = macho_info;

    /* If has no directories, try PATH first. */
    p = file_name(filename);
    if (p == filename)
    {
        ret = search_unix_path(filename, getenv("PATH"), macho_load_file_cb, &load_params);
    }
    /* Try DYLD_LIBRARY_PATH, with just the filename (no directories). */
    if (!ret)
        ret = search_unix_path(p, getenv("DYLD_LIBRARY_PATH"), macho_load_file_cb, &load_params);

    /* Try the path as given. */
    if (!ret)
        ret = macho_load_file(pcs, filename, load_addr, macho_info);
    /* Try DYLD_FALLBACK_LIBRARY_PATH, with just the filename (no directories). */
    if (!ret)
    {
        const char* fallback = getenv("DYLD_FALLBACK_LIBRARY_PATH");
        if (!fallback)
            fallback = "/usr/local/lib:/lib:/usr/lib";
        ret = search_unix_path(p, fallback, macho_load_file_cb, &load_params);
    }
    if (!ret && p == filename)
        ret = search_dll_path(filename, macho_load_file_cb, &load_params);

    return ret;
}

/******************************************************************
 *              macho_enum_modules_internal
 *
 * Enumerate Mach-O modules from a running process
 */
static BOOL macho_enum_modules_internal(const struct process* pcs,
                                        const WCHAR* main_name,
                                        enum_modules_cb cb, void* user)
{
    union wine_all_image_infos  image_infos;
    union wine_image_info*      info_array = NULL;
    ULONG_PTR                   len;
    int                         i;
    char                        bufstr[256];
    WCHAR                       bufstrW[MAX_PATH];
    BOOL                        ret = FALSE;

    TRACE("(%p/%p, %s, %p, %p)\n", pcs, pcs->handle, debugstr_w(main_name), cb,
            user);

    if (pcs->is_64bit)
        len = sizeof(image_infos.infos64);
    else
        len = sizeof(image_infos.infos32);
    if (!pcs->dbg_hdr_addr ||
        !ReadProcessMemory(pcs->handle, (void*)pcs->dbg_hdr_addr,
                           &image_infos, len, NULL))
        goto done;
    if (!pcs->is_64bit)
    {
        struct dyld_all_image_infos32 temp = image_infos.infos32;
        image_infos.infos64.infoArrayCount = temp.infoArrayCount;
        image_infos.infos64.infoArray = temp.infoArray;
    }
    if (!image_infos.infos64.infoArray)
        goto done;
    TRACE("Process has %u image infos at %p\n", image_infos.infos64.infoArrayCount, (void*)image_infos.infos64.infoArray);

    if (pcs->is_64bit)
        len = sizeof(info_array->info64);
    else
        len = sizeof(info_array->info32);
    len *= image_infos.infos64.infoArrayCount;
    info_array = HeapAlloc(GetProcessHeap(), 0, len);
    if (!info_array ||
        !ReadProcessMemory(pcs->handle, (void*)image_infos.infos64.infoArray,
                           info_array, len, NULL))
        goto done;
    TRACE("... read image infos\n");

    for (i = 0; i < image_infos.infos64.infoArrayCount; i++)
    {
        struct dyld_image_info64 info;
        if (pcs->is_64bit)
            info = info_array[i].info64;
        else
        {
            struct dyld_image_info32 *info32 = &info_array->info32 + i;
            info.imageLoadAddress = info32->imageLoadAddress;
            info.imageFilePath = info32->imageFilePath;
        }
        if (info.imageFilePath &&
            ReadProcessMemory(pcs->handle, (void*)info.imageFilePath, bufstr, sizeof(bufstr), NULL))
        {
            bufstr[sizeof(bufstr) - 1] = '\0';
            TRACE("[%d] image file %s\n", i, debugstr_a(bufstr));
            MultiByteToWideChar(CP_UNIXCP, 0, bufstr, -1, bufstrW, ARRAY_SIZE(bufstrW));
            if (main_name && !bufstrW[0]) strcpyW(bufstrW, main_name);
            if (!cb(bufstrW, info.imageLoadAddress, user)) break;
        }
    }

    ret = TRUE;
done:
    HeapFree(GetProcessHeap(), 0, info_array);
    return ret;
}

struct macho_sync
{
    struct process*     pcs;
    struct macho_info   macho_info;
};

static BOOL macho_enum_sync_cb(const WCHAR* name, ULONG_PTR addr, void* user)
{
    struct macho_sync*  ms = user;

    TRACE("(%s, 0x%08lx, %p)\n", debugstr_w(name), addr, user);
    macho_search_and_load_file(ms->pcs, name, addr, &ms->macho_info);
    return TRUE;
}

/******************************************************************
 *              macho_synchronize_module_list
 *
 * Rescans the debuggee's modules list and synchronizes it with
 * the one from 'pcs', ie:
 * - if a module is in debuggee and not in pcs, it's loaded into pcs
 * - if a module is in pcs and not in debuggee, it's unloaded from pcs
 */
static BOOL macho_synchronize_module_list(struct process* pcs)
{
    struct module*      module;
    struct macho_sync     ms;

    TRACE("(%p/%p)\n", pcs, pcs->handle);

    for (module = pcs->lmodules; module; module = module->next)
    {
        if (module->type == DMT_MACHO && !module->is_virtual)
            module->format_info[DFI_MACHO]->u.macho_info->in_use = 0;
    }

    ms.pcs = pcs;
    ms.macho_info.flags = MACHO_INFO_MODULE;
    if (!macho_enum_modules_internal(pcs, NULL, macho_enum_sync_cb, &ms))
        return FALSE;

    module = pcs->lmodules;
    while (module)
    {
        if (module->type == DMT_MACHO && !module->is_virtual &&
            !module->format_info[DFI_MACHO]->u.macho_info->in_use &&
            !module->format_info[DFI_MACHO]->u.macho_info->is_loader)
        {
            module_remove(pcs, module);
            /* restart all over */
            module = pcs->lmodules;
        }
        else module = module->next;
    }
    return TRUE;
}

/******************************************************************
 *              macho_enum_modules
 *
 * Enumerates the Mach-O loaded modules from a running target (hProc)
 * This function doesn't require that someone has called SymInitialize
 * on this very process.
 */
static BOOL macho_enum_modules(struct process* process, enum_modules_cb cb, void* user)
{
    struct macho_info   macho_info;
    BOOL                ret;

    TRACE("(%p, %p, %p)\n", process->handle, cb, user);
    macho_info.flags = MACHO_INFO_NAME;
    ret = macho_enum_modules_internal(process, macho_info.module_name, cb, user);
    HeapFree(GetProcessHeap(), 0, (char*)macho_info.module_name);
    return ret;
}

struct macho_load
{
    struct process*     pcs;
    struct macho_info   macho_info;
    const WCHAR*        name;
    BOOL                ret;
};

/******************************************************************
 *              macho_load_cb
 *
 * Callback for macho_load_module, used to walk the list of loaded
 * modules.
 */
static BOOL macho_load_cb(const WCHAR* name, ULONG_PTR addr, void* user)
{
    struct macho_load*  ml = user;
    const WCHAR*        p;

    TRACE("(%s, 0x%08lx, %p)\n", debugstr_w(name), addr, user);

    /* memcmp is needed for matches when bufstr contains also version information
     * ml->name: libc.so, name: libc.so.6.0
     */
    p = file_name(name);
    if (!memcmp(p, ml->name, lstrlenW(ml->name) * sizeof(WCHAR)))
    {
        ml->ret = macho_search_and_load_file(ml->pcs, name, addr, &ml->macho_info);
        return FALSE;
    }
    return TRUE;
}

/******************************************************************
 *              macho_load_module
 *
 * Loads a Mach-O module and stores it in process' module list.
 * Also, find module real name and load address from
 * the real loaded modules list in pcs address space.
 */
static struct module* macho_load_module(struct process* pcs, const WCHAR* name, ULONG_PTR addr)
{
    struct macho_load   ml;

    TRACE("(%p/%p, %s, 0x%08lx)\n", pcs, pcs->handle, debugstr_w(name), addr);

    ml.macho_info.flags = MACHO_INFO_MODULE;
    ml.ret = FALSE;

    if (pcs->dbg_hdr_addr) /* we're debugging a live target */
    {
        ml.pcs = pcs;
        /* do only the lookup from the filename, not the path (as we lookup module
         * name in the process' loaded module list)
         */
        ml.name = file_name(name);
        ml.ret = FALSE;

        if (!macho_enum_modules_internal(pcs, NULL, macho_load_cb, &ml))
            return NULL;
    }
    else if (addr)
    {
        ml.name = name;
        ml.ret = macho_search_and_load_file(pcs, ml.name, addr, &ml.macho_info);
    }
    if (!ml.ret) return NULL;
    assert(ml.macho_info.module);
    return ml.macho_info.module;
}

/******************************************************************
 *              macho_search_loader
 *
 * Lookup in a running Mach-O process the loader, and sets its Mach-O link
 * address (for accessing the list of loaded images) in pcs.
 * If flags is MACHO_INFO_MODULE, the module for the loader is also
 * added as a module into pcs.
 */
static BOOL macho_search_loader(struct process* pcs, struct macho_info* macho_info)
{
    BOOL ret = FALSE;
    union wine_all_image_infos image_infos;
    union wine_image_info image_info;
    uint32_t len;
    char path[PATH_MAX];
    BOOL got_path = FALSE;

    if (pcs->is_64bit)
        len = sizeof(image_infos.infos64);
    else
        len = sizeof(image_infos.infos32);
    if (pcs->dbg_hdr_addr &&
        ReadProcessMemory(pcs->handle, (void*)pcs->dbg_hdr_addr, &image_infos, len, NULL))
    {
        if (pcs->is_64bit)
            len = sizeof(image_info.info64);
        else
        {
            struct dyld_all_image_infos32 temp = image_infos.infos32;
            image_infos.infos64.infoArrayCount = temp.infoArrayCount;
            image_infos.infos64.infoArray = temp.infoArray;
            len = sizeof(image_info.info32);
        }
        if (image_infos.infos64.infoArray && image_infos.infos64.infoArrayCount &&
            ReadProcessMemory(pcs->handle, (void*)image_infos.infos64.infoArray, &image_info, len, NULL))
        {
            if (!pcs->is_64bit)
            {
                struct dyld_image_info32 temp = image_info.info32;
                image_info.info64.imageLoadAddress = temp.imageLoadAddress;
                image_info.info64.imageFilePath = temp.imageFilePath;
            }
            for (len = sizeof(path); image_info.info64.imageFilePath && len > 0; len /= 2)
            {
                if (ReadProcessMemory(pcs->handle, (void*)image_info.info64.imageFilePath, path, len, NULL))
                {
                    path[len - 1] = 0;
                    got_path = TRUE;
                    TRACE("got executable path from target's dyld image info: %s\n", debugstr_a(path));
                    break;
                }
            }
        }
    }

    /* If we couldn't get the executable path from the target process, try our
       own.  It will almost always be the same. */
    if (!got_path)
    {
        len = sizeof(path);
        if (!_NSGetExecutablePath(path, &len))
        {
            got_path = TRUE;
            TRACE("using own executable path: %s\n", debugstr_a(path));
        }
    }

    if (got_path)
    {
        WCHAR* pathW;

        len = MultiByteToWideChar(CP_UNIXCP, 0, path, -1, NULL, 0);
        pathW = HeapAlloc(GetProcessHeap(), 0, len * sizeof(WCHAR));
        if (pathW)
        {
            MultiByteToWideChar(CP_UNIXCP, 0, path, -1, pathW, len);
            ret = macho_load_file(pcs, pathW, 0, macho_info);
            HeapFree(GetProcessHeap(), 0, pathW);
        }
    }

    if (!ret)
    {
        WCHAR *loader = get_wine_loader_name(pcs);
        ret = loader && macho_search_and_load_file(pcs, loader, 0, macho_info);
        heap_free(loader);
    }
    return ret;
}

static const struct loader_ops macho_loader_ops =
{
    macho_synchronize_module_list,
    macho_load_module,
    macho_load_debug_info,
    macho_enum_modules,
    macho_fetch_file_info,
};

/******************************************************************
 *              macho_read_wine_loader_dbg_info
 *
 * Try to find a decent wine executable which could have loaded the debuggee
 */
BOOL macho_read_wine_loader_dbg_info(struct process* pcs, ULONG_PTR addr)
{
    struct macho_info     macho_info;

    TRACE("(%p/%p)\n", pcs, pcs->handle);
    pcs->dbg_hdr_addr = addr ? addr : get_dyld_image_info_address(pcs);
    macho_info.flags = MACHO_INFO_MODULE;
    if (!macho_search_loader(pcs, &macho_info)) return FALSE;
    macho_info.module->format_info[DFI_MACHO]->u.macho_info->is_loader = 1;
    module_set_module(macho_info.module, S_WineLoaderW);
    pcs->loader = &macho_loader_ops;
    TRACE("Found macho debug header %#lx\n", pcs->dbg_hdr_addr);
    return TRUE;
}

#else  /* HAVE_MACH_O_LOADER_H */

BOOL macho_read_wine_loader_dbg_info(struct process* pcs, ULONG_PTR addr)
{
    return FALSE;
}

#endif  /* HAVE_MACH_O_LOADER_H */
