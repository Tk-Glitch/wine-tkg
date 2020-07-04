/*
 * Win32 virtual memory functions
 *
 * Copyright 1997, 2002 Alexandre Julliard
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

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#ifdef HAVE_SYS_MMAN_H
# include <sys/mman.h>
#endif
#ifdef HAVE_SYS_SYSINFO_H
# include <sys/sysinfo.h>
#endif
#ifdef HAVE_VALGRIND_VALGRIND_H
# include <valgrind/valgrind.h>
#endif

#include "ntstatus.h"
#define WIN32_NO_STATUS
#define NONAMELESSUNION
#include "windef.h"
#include "winternl.h"
#include "wine/library.h"
#include "wine/server.h"
#include "wine/exception.h"
#include "wine/rbtree.h"
#include "wine/debug.h"
#include "ntdll_misc.h"

WINE_DEFAULT_DEBUG_CHANNEL(virtual);
WINE_DECLARE_DEBUG_CHANNEL(module);

#ifndef MAP_NORESERVE
#define MAP_NORESERVE 0
#endif

/* File view */
struct file_view
{
    struct wine_rb_entry entry;  /* entry in global view tree */
    void         *base;          /* base address */
    size_t        size;          /* size in bytes */
    unsigned int  protect;       /* protection for all pages at allocation time and SEC_* flags */
};

/* per-page protection flags */
#define VPROT_READ       0x01
#define VPROT_WRITE      0x02
#define VPROT_EXEC       0x04
#define VPROT_WRITECOPY  0x08
#define VPROT_GUARD      0x10
#define VPROT_COMMITTED  0x20
#define VPROT_WRITEWATCH 0x40
#define VPROT_WRITTEN    0x80
/* per-mapping protection flags */
#define VPROT_SYSTEM     0x0200  /* system view (underlying mmap not under our control) */

/* Conversion from VPROT_* to Win32 flags */
static const BYTE VIRTUAL_Win32Flags[16] =
{
    PAGE_NOACCESS,              /* 0 */
    PAGE_READONLY,              /* READ */
    PAGE_READWRITE,             /* WRITE */
    PAGE_READWRITE,             /* READ | WRITE */
    PAGE_EXECUTE,               /* EXEC */
    PAGE_EXECUTE_READ,          /* READ | EXEC */
    PAGE_EXECUTE_READWRITE,     /* WRITE | EXEC */
    PAGE_EXECUTE_READWRITE,     /* READ | WRITE | EXEC */
    PAGE_WRITECOPY,             /* WRITECOPY */
    PAGE_WRITECOPY,             /* READ | WRITECOPY */
    PAGE_WRITECOPY,             /* WRITE | WRITECOPY */
    PAGE_WRITECOPY,             /* READ | WRITE | WRITECOPY */
    PAGE_EXECUTE_WRITECOPY,     /* EXEC | WRITECOPY */
    PAGE_EXECUTE_WRITECOPY,     /* READ | EXEC | WRITECOPY */
    PAGE_EXECUTE_WRITECOPY,     /* WRITE | EXEC | WRITECOPY */
    PAGE_EXECUTE_WRITECOPY      /* READ | WRITE | EXEC | WRITECOPY */
};

static struct wine_rb_tree views_tree;

static void *last_already_mapped;
static size_t last_already_mapped_size;

static RTL_CRITICAL_SECTION csVirtual;
static RTL_CRITICAL_SECTION_DEBUG critsect_debug =
{
    0, 0, &csVirtual,
    { &critsect_debug.ProcessLocksList, &critsect_debug.ProcessLocksList },
      0, 0, { (DWORD_PTR)(__FILE__ ": csVirtual") }
};
static RTL_CRITICAL_SECTION csVirtual = { &critsect_debug, -1, 0, 0, 0, 0 };

#ifdef __i386__
static const UINT page_shift = 12;
static const UINT_PTR page_mask = 0xfff;
/* Note: these are Windows limits, you cannot change them. */
static void *address_space_limit = (void *)0xc0000000;  /* top of the total available address space */
static void *user_space_limit    = (void *)0x7fff0000;  /* top of the user address space */
static void *working_set_limit   = (void *)0x7fff0000;  /* top of the current working set */
static void *address_space_start = (void *)0x110000;    /* keep DOS area clear */
#elif defined(__x86_64__)
static const UINT page_shift = 12;
static const UINT_PTR page_mask = 0xfff;
static void *address_space_limit = (void *)0x7fffffff0000;
static void *user_space_limit    = (void *)0x7fffffff0000;
static void *working_set_limit   = (void *)0x7fffffff0000;
static void *address_space_start = (void *)0x10000;
#elif defined(__arm__)
static const UINT page_shift = 12;
static const UINT_PTR page_mask = 0xfff;
static void *address_space_limit = (void *)0xc0000000;
static void *user_space_limit    = (void *)0x7fff0000;
static void *working_set_limit   = (void *)0x7fff0000;
static void *address_space_start = (void *)0x10000;
#elif defined(__aarch64__)
static const UINT page_shift = 12;
static const UINT_PTR page_mask = 0xfff;
static void *address_space_limit = (void *)0xffffffff0000;
static void *user_space_limit    = (void *)0x7fffffff0000;
static void *working_set_limit   = (void *)0x7fffffff0000;
static void *address_space_start = (void *)0x10000;
#else
UINT_PTR page_size = 0;
static UINT page_shift;
static UINT_PTR page_mask;
static void *address_space_limit;
static void *user_space_limit;
static void *working_set_limit;
static void *address_space_start = (void *)0x10000;
#endif  /* __i386__ */
static const BOOL is_win64 = (sizeof(void *) > sizeof(int));
static const UINT_PTR granularity_mask = 0xffff;

SIZE_T signal_stack_size = 0;
SIZE_T signal_stack_mask = 0;
static SIZE_T signal_stack_align;

/* TEB allocation blocks */
static TEB *teb_block;
static TEB *next_free_teb;
static int teb_block_pos;

#define ROUND_ADDR(addr,mask) \
   ((void *)((UINT_PTR)(addr) & ~(UINT_PTR)(mask)))

#define ROUND_SIZE(addr,size) \
   (((SIZE_T)(size) + ((UINT_PTR)(addr) & page_mask) + page_mask) & ~page_mask)

#define VIRTUAL_DEBUG_DUMP_VIEW(view) \
    do { if (TRACE_ON(virtual)) VIRTUAL_DumpView(view); } while (0)

#ifdef _WIN64  /* on 64-bit the page protection bytes use a 2-level table */
static const size_t pages_vprot_shift = 20;
static const size_t pages_vprot_mask = (1 << 20) - 1;
static size_t pages_vprot_size;
static BYTE **pages_vprot;
#else  /* on 32-bit we use a simple array with one byte per page */
static BYTE *pages_vprot;
#endif

#define MAX_DIR_ENTRY_LEN 255  /* max length of a directory entry in chars */

static struct file_view *view_block_start, *view_block_end, *next_free_view;
#ifdef _WIN64
static const size_t view_block_size = 0x200000;
#else
static const size_t view_block_size = 0x100000;
#endif
static void *preload_reserve_start;
static void *preload_reserve_end;
static BOOL use_locks;
static BOOL force_exec_prot;  /* whether to force PROT_EXEC on all PROT_READ mmaps */

struct range_entry
{
    void *base;
    void *end;
};

static struct range_entry *free_ranges;
static struct range_entry *free_ranges_end;


/***********************************************************************
 *           free_ranges_lower_bound
 *
 * Returns the first range whose end is not less than addr, or end if there's none.
 */
static struct range_entry *free_ranges_lower_bound( void *addr )
{
    struct range_entry *begin = free_ranges;
    struct range_entry *end = free_ranges_end;
    struct range_entry *mid;

    while (begin < end)
    {
        mid = begin + (end - begin) / 2;
        if (mid->end < addr)
            begin = mid + 1;
        else
            end = mid;
    }

    return begin;
}


/***********************************************************************
 *           free_ranges_insert_view
 *
 * Updates the free_ranges after a new view has been created.
 */
static void free_ranges_remove_range( void *view_base, void *view_end, void *view_base_unaligned )
{
    struct range_entry *range = free_ranges_lower_bound( view_base );
    struct range_entry *next = range + 1;

    TRACE("view %p-%p (%p).\n", view_base, view_end, view_base_unaligned);

    /* free_ranges initial value is such that the view is either inside range or before another one. */
    assert( range != free_ranges_end );
    assert( range->end > view_base || next != free_ranges_end );

    /* this happens because virtual_alloc_thread_stack shrinks a view, then creates another one on top,
     * or because AT_ROUND_TO_PAGE was used with NtMapViewOfSection to force 4kB aligned mapping. */
    if ((range->end > view_base && range->base >= view_end) ||
        (range->end == view_base && next->base >= view_end))
    {
        /* on Win64, assert that it's correctly aligned so we're not going to be in trouble later */
        assert( (!is_win64 && !is_wow64) || view_base_unaligned == view_base );
        WARN( "range %p - %p is already mapped\n", view_base, view_end );
        return;
    }

    /* this should never happen */
    if (range->base > view_base || range->end < view_end)
        ERR( "range %p - %p is already partially mapped\n", view_base, view_end );
    assert( range->base <= view_base && range->end >= view_end );

    /* need to split the range in two */
    if (range->base < view_base && range->end > view_end)
    {
        memmove( next + 1, next, (free_ranges_end - next) * sizeof(struct range_entry) );
        free_ranges_end += 1;
        if ((char *)free_ranges_end - (char *)free_ranges > view_block_size)
            MESSAGE( "Free range sequence is full, trouble ahead!\n" );
        assert( (char *)free_ranges_end - (char *)free_ranges <= view_block_size );

        next->base = view_end;
        next->end = range->end;
        range->end = view_base;
    }
    else
    {
        /* otherwise we just have to shrink it */
        if (range->base < view_base)
            range->end = view_base;
        else
            range->base = view_end;

        if (range->base < range->end) return;

        /* and possibly remove it if it's now empty */
        memmove( range, next, (free_ranges_end - next) * sizeof(struct range_entry) );
        free_ranges_end -= 1;
        assert( free_ranges_end - free_ranges > 0 );
    }
}

static void free_ranges_insert_view( struct file_view *view )
{
    free_ranges_remove_range(ROUND_ADDR(view->base, granularity_mask),
            ROUND_ADDR((char *)view->base + view->size + granularity_mask, granularity_mask),
            view->base);
}

/***********************************************************************
 *           free_ranges_remove_view
 *
 * Updates the free_ranges after a view has been destroyed.
 */
static void free_ranges_remove_view( struct file_view *view )
{
    void *view_base = ROUND_ADDR( view->base, granularity_mask );
    void *view_end = ROUND_ADDR( (char *)view->base + view->size + granularity_mask, granularity_mask );
    struct range_entry *range = free_ranges_lower_bound( view_base );
    struct range_entry *next = range + 1;

    /* It's possible to use AT_ROUND_TO_PAGE on 32bit with NtMapViewOfSection to force 4kB alignment,
     * and this breaks our assumptions. Look at the views around to check if the range is still in use. */
#ifndef _WIN64
    struct file_view *prev_view = WINE_RB_ENTRY_VALUE( wine_rb_prev( &view->entry ), struct file_view, entry );
    struct file_view *next_view = WINE_RB_ENTRY_VALUE( wine_rb_next( &view->entry ), struct file_view, entry );
    void *prev_view_base = prev_view ? ROUND_ADDR( prev_view->base, granularity_mask ) : NULL;
    void *prev_view_end = prev_view ? ROUND_ADDR( (char *)prev_view->base + prev_view->size + granularity_mask, granularity_mask ) : NULL;
    void *next_view_base = next_view ? ROUND_ADDR( next_view->base, granularity_mask ) : NULL;
    void *next_view_end = next_view ? ROUND_ADDR( (char *)next_view->base + next_view->size + granularity_mask, granularity_mask ) : NULL;

    if ((prev_view_base < view_end && prev_view_end > view_base) ||
        (next_view_base < view_end && next_view_end > view_base))
    {
        WARN( "range %p - %p is still mapped\n", view_base, view_end );
        return;
    }
#endif
    TRACE("view %p-%p.\n", view_base, view_end);

    /* free_ranges initial value is such that the view is either inside range or before another one. */
    assert( range != free_ranges_end );
    assert( range->end > view_base || next != free_ranges_end );

    /* this should never happen, but we can safely ignore it */
    if (range->base <= view_base && range->end >= view_end)
    {
        WARN( "range %p - %p is already unmapped\n", view_base, view_end );
        return;
    }

    /* this should never happen */
    if (range->base < view_end && range->end > view_base)
        ERR( "range %p - %p is already partially unmapped\n", view_base, view_end );
    assert( range->end <= view_base || range->base >= view_end );

    /* merge with next if possible */
    if (range->end == view_base && next->base == view_end)
    {
        range->end = next->end;
        memmove( next, next + 1, (free_ranges_end - next - 1) * sizeof(struct range_entry) );
        free_ranges_end -= 1;
        assert( free_ranges_end - free_ranges > 0 );
    }
    /* or try growing the range */
    else if (range->end == view_base)
        range->end = view_end;
    else if (range->base == view_end)
        range->base = view_base;
    /* otherwise create a new one */
    else
    {
        memmove( range + 1, range, (free_ranges_end - range) * sizeof(struct range_entry) );
        free_ranges_end += 1;
        if ((char *)free_ranges_end - (char *)free_ranges > view_block_size)
            MESSAGE( "Free range sequence is full, trouble ahead!\n" );
        assert( (char *)free_ranges_end - (char *)free_ranges <= view_block_size );

        range->base = view_base;
        range->end = view_end;
    }
}


#if defined(__i386__)
NTSTATUS WINAPI NtProtectVirtualMemory( HANDLE process, PVOID *addr_ptr, SIZE_T *size_ptr,
                                        ULONG new_prot, ULONG *old_prot ) DECLSPEC_ALIGN(4096);
NTSTATUS WINAPI NtCreateSection( HANDLE *handle, ACCESS_MASK access, const OBJECT_ATTRIBUTES *attr,
                                 const LARGE_INTEGER *size, ULONG protect,
                                 ULONG sec_flags, HANDLE file ) DECLSPEC_ALIGN(4096);
#endif

static inline int is_view_valloc( const struct file_view *view )
{
    return !(view->protect & (SEC_FILE | SEC_RESERVE | SEC_COMMIT));
}

/***********************************************************************
 *           get_page_vprot
 *
 * Return the page protection byte.
 */
static BYTE get_page_vprot( const void *addr )
{
    size_t idx = (size_t)addr >> page_shift;

#ifdef _WIN64
    if ((idx >> pages_vprot_shift) >= pages_vprot_size) return 0;
    if (!pages_vprot[idx >> pages_vprot_shift]) return 0;
    return pages_vprot[idx >> pages_vprot_shift][idx & pages_vprot_mask];
#else
    return pages_vprot[idx];
#endif
}


/***********************************************************************
 *           set_page_vprot
 *
 * Set a range of page protection bytes.
 */
static void set_page_vprot( const void *addr, size_t size, BYTE vprot )
{
    size_t idx = (size_t)addr >> page_shift;
    size_t end = ((size_t)addr + size + page_mask) >> page_shift;

#ifdef _WIN64
    while (idx >> pages_vprot_shift != end >> pages_vprot_shift)
    {
        size_t dir_size = pages_vprot_mask + 1 - (idx & pages_vprot_mask);
        memset( pages_vprot[idx >> pages_vprot_shift] + (idx & pages_vprot_mask), vprot, dir_size );
        idx += dir_size;
    }
    memset( pages_vprot[idx >> pages_vprot_shift] + (idx & pages_vprot_mask), vprot, end - idx );
#else
    memset( pages_vprot + idx, vprot, end - idx );
#endif
}


/***********************************************************************
 *           set_page_vprot_bits
 *
 * Set or clear bits in a range of page protection bytes.
 */
static void set_page_vprot_bits( const void *addr, size_t size, BYTE set, BYTE clear )
{
    size_t idx = (size_t)addr >> page_shift;
    size_t end = ((size_t)addr + size + page_mask) >> page_shift;

#ifdef _WIN64
    for ( ; idx < end; idx++)
    {
        BYTE *ptr = pages_vprot[idx >> pages_vprot_shift] + (idx & pages_vprot_mask);
        *ptr = (*ptr & ~clear) | set;
    }
#else
    for ( ; idx < end; idx++) pages_vprot[idx] = (pages_vprot[idx] & ~clear) | set;
#endif
}


/***********************************************************************
 *           alloc_pages_vprot
 *
 * Allocate the page protection bytes for a given range.
 */
static BOOL alloc_pages_vprot( const void *addr, size_t size )
{
#ifdef _WIN64
    size_t idx = (size_t)addr >> page_shift;
    size_t end = ((size_t)addr + size + page_mask) >> page_shift;
    size_t i;
    void *ptr;

    assert( end <= pages_vprot_size << pages_vprot_shift );
    for (i = idx >> pages_vprot_shift; i < (end + pages_vprot_mask) >> pages_vprot_shift; i++)
    {
        if (pages_vprot[i]) continue;
        if ((ptr = wine_anon_mmap( NULL, pages_vprot_mask + 1, PROT_READ | PROT_WRITE, 0 )) == (void *)-1)
            return FALSE;
        pages_vprot[i] = ptr;
    }
#endif
    return TRUE;
}


/***********************************************************************
 *           compare_view
 *
 * View comparison function used for the rb tree.
 */
static int compare_view( const void *addr, const struct wine_rb_entry *entry )
{
    struct file_view *view = WINE_RB_ENTRY_VALUE( entry, struct file_view, entry );

    if (addr < view->base) return -1;
    if (addr > view->base) return 1;
    return 0;
}


/***********************************************************************
 *           VIRTUAL_GetProtStr
 */
static const char *VIRTUAL_GetProtStr( BYTE prot )
{
    static char buffer[6];
    buffer[0] = (prot & VPROT_COMMITTED) ? 'c' : '-';
    buffer[1] = (prot & VPROT_GUARD) ? 'g' : ((prot & VPROT_WRITEWATCH) ? 'H' : '-');
    buffer[2] = (prot & VPROT_READ) ? 'r' : '-';
    buffer[3] = (prot & VPROT_WRITECOPY) ? 'W' : ((prot & VPROT_WRITE) ? 'w' : '-');
    buffer[4] = (prot & VPROT_EXEC) ? 'x' : '-';
    buffer[5] = 0;
    return buffer;
}

/* This might look like a hack, but it actually isn't - the 'experimental' version
 * is correct, but it already has revealed a couple of additional Wine bugs, which
 * were not triggered before, and there are probably some more.
 * To avoid breaking Wine for everyone, the new correct implementation has to be
 * manually enabled, until it is tested a bit more. */
static inline BOOL experimental_WRITECOPY( void )
{
    static int enabled = -1;
    if (enabled == -1)
    {
        const char *str = getenv("STAGING_WRITECOPY");
        enabled = str && (atoi(str) != 0);
    }
    return enabled;
}

/***********************************************************************
 *           VIRTUAL_GetUnixProt
 *
 * Convert page protections to protection for mmap/mprotect.
 */
static int VIRTUAL_GetUnixProt( BYTE vprot )
{
    int prot = 0;
    if ((vprot & VPROT_COMMITTED) && !(vprot & VPROT_GUARD))
    {
        if (vprot & VPROT_READ) prot |= PROT_READ;
        if (vprot & VPROT_WRITE) prot |= PROT_WRITE | PROT_READ;
        if (vprot & VPROT_EXEC) prot |= PROT_EXEC | PROT_READ;
#if defined(__i386__) || defined(__x86_64__)
        if (vprot & VPROT_WRITECOPY)
        {
            if (experimental_WRITECOPY() && !(vprot & VPROT_WRITTEN))
                prot = (prot & ~PROT_WRITE) | PROT_READ;
            else
                prot |= PROT_WRITE | PROT_READ;
        }
#else
        /* FIXME: Architecture needs implementation of signal_init_early. */
        if (vprot & VPROT_WRITECOPY) prot |= PROT_WRITE | PROT_READ;
#endif
        if (vprot & VPROT_WRITEWATCH) prot &= ~PROT_WRITE;
    }
    if (!prot) prot = PROT_NONE;
    return prot;
}


/***********************************************************************
 *           VIRTUAL_DumpView
 */
static void VIRTUAL_DumpView( struct file_view *view )
{
    UINT i, count;
    char *addr = view->base;
    BYTE prot = get_page_vprot( addr );

    TRACE( "View: %p - %p", addr, addr + view->size - 1 );
    if (view->protect & VPROT_SYSTEM)
        TRACE( " (builtin image)\n" );
    else if (view->protect & SEC_IMAGE)
        TRACE( " (image)\n" );
    else if (view->protect & SEC_FILE)
        TRACE( " (file)\n" );
    else if (view->protect & (SEC_RESERVE | SEC_COMMIT))
        TRACE( " (anonymous)\n" );
    else
        TRACE( " (valloc)\n");

    for (count = i = 1; i < view->size >> page_shift; i++, count++)
    {
        BYTE next = get_page_vprot( addr + (count << page_shift) );
        if (next == prot) continue;
        TRACE( "      %p - %p %s\n",
                 addr, addr + (count << page_shift) - 1, VIRTUAL_GetProtStr(prot) );
        addr += (count << page_shift);
        prot = next;
        count = 0;
    }
    if (count)
        TRACE( "      %p - %p %s\n",
                 addr, addr + (count << page_shift) - 1, VIRTUAL_GetProtStr(prot) );
}


/***********************************************************************
 *           VIRTUAL_Dump
 */
#ifdef WINE_VM_DEBUG
static void VIRTUAL_Dump(void)
{
    sigset_t sigset;
    struct file_view *view;

    TRACE( "Dump of all virtual memory views:\n" );
    server_enter_uninterrupted_section( &csVirtual, &sigset );
    WINE_RB_FOR_EACH_ENTRY( view, &views_tree, struct file_view, entry )
    {
        VIRTUAL_DumpView( view );
    }
    server_leave_uninterrupted_section( &csVirtual, &sigset );
}
#endif


/***********************************************************************
 *           VIRTUAL_FindView
 *
 * Find the view containing a given address. The csVirtual section must be held by caller.
 *
 * PARAMS
 *      addr  [I] Address
 *
 * RETURNS
 *	View: Success
 *	NULL: Failure
 */
static struct file_view *VIRTUAL_FindView( const void *addr, size_t size )
{
    struct wine_rb_entry *ptr = views_tree.root;

    if ((const char *)addr + size < (const char *)addr) return NULL; /* overflow */

    while (ptr)
    {
        struct file_view *view = WINE_RB_ENTRY_VALUE( ptr, struct file_view, entry );

        if (view->base > addr) ptr = ptr->left;
        else if ((const char *)view->base + view->size <= (const char *)addr) ptr = ptr->right;
        else if ((const char *)view->base + view->size < (const char *)addr + size) break;  /* size too large */
        else return view;
    }
    return NULL;
}


/***********************************************************************
 *           zero_bits_win_to_64
 *
 * Convert from Windows hybrid 32bit-based / bitmask to 64bit-based format
 */
static inline unsigned short zero_bits_win_to_64( ULONG_PTR zero_bits )
{
    unsigned short zero_bits_64;

    if (zero_bits == 0) return 0;
    if (zero_bits < 32) return 32 + zero_bits;
    zero_bits_64 = 63;
#ifdef _WIN64
    if (zero_bits >> 32) { zero_bits_64 -= 32; zero_bits >>= 32; }
#endif
    if (zero_bits >> 16) { zero_bits_64 -= 16; zero_bits >>= 16; }
    if (zero_bits >> 8) { zero_bits_64 -= 8; zero_bits >>= 8; }
    if (zero_bits >> 4) { zero_bits_64 -= 4; zero_bits >>= 4; }
    if (zero_bits >> 2) { zero_bits_64 -= 2; zero_bits >>= 2; }
    if (zero_bits >> 1) { zero_bits_64 -= 1; }
    return zero_bits_64;
}


/***********************************************************************
 *           get_zero_bits_64_mask
 */
static inline UINT_PTR get_zero_bits_64_mask( USHORT zero_bits_64 )
{
    return (UINT_PTR)((~(UINT64)0) >> zero_bits_64);
}


/***********************************************************************
 *           is_write_watch_range
 */
static inline BOOL is_write_watch_range( const void *addr, size_t size )
{
    struct file_view *view = VIRTUAL_FindView( addr, size );
    return view && (view->protect & VPROT_WRITEWATCH);
}


/***********************************************************************
 *           is_system_range
 */
static inline BOOL is_system_range( const void *addr, size_t size )
{
    struct file_view *view = VIRTUAL_FindView( addr, size );
    return view && (view->protect & VPROT_SYSTEM);
}


/***********************************************************************
 *           find_view_range
 *
 * Find the first view overlapping at least part of the specified range.
 * The csVirtual section must be held by caller.
 */
static struct file_view *find_view_range( const void *addr, size_t size )
{
    struct wine_rb_entry *ptr = views_tree.root;

    while (ptr)
    {
        struct file_view *view = WINE_RB_ENTRY_VALUE( ptr, struct file_view, entry );

        if ((const char *)view->base >= (const char *)addr + size) ptr = ptr->left;
        else if ((const char *)view->base + view->size <= (const char *)addr) ptr = ptr->right;
        else return view;
    }
    return NULL;
}


/***********************************************************************
 *           try_map_free_area
 *
 * Try mmaping some expected free memory region, eventually stepping and
 * retrying inside it, and return where it actually succeeded, or NULL.
 */
static void* try_map_free_area( void *base, void *end, ptrdiff_t step,
                                void *start, size_t size, int unix_prot )
{
#ifdef MAP_FIXED_NOREPLACE
    static int flags = MAP_FIXED_NOREPLACE;
#else
    static int flags = 0;
#endif
    void *ptr;

    while (start && base <= start && (char*)start + size <= (char*)end)
    {
        if ((ptr = wine_anon_mmap( start, size, unix_prot, flags )) == start)
            return start;
        TRACE( "Found free area is already mapped, start %p.\n", start );

        if (ptr == (void *)-1 && errno != EEXIST)
        {
            ERR("wine_anon_mmap() error %s, start %p, size %p, unix_prot %#x.\n",
                    strerror(errno), start, (void *)size, unix_prot);
            return NULL;
        }

        if (ptr != (void *)-1)
            munmap( ptr, size );

        if (!last_already_mapped && step)
        {
            last_already_mapped = start;
            last_already_mapped_size = step > 0 ? step : -step;
            last_already_mapped_size = min(last_already_mapped_size, (char *)end - (char *)start);
        }

        if ((step > 0 && (char *)end - (char *)start < step) ||
            (step < 0 && (char *)start - (char *)base < -step) ||
            step == 0)
            break;
        start = (char *)start + step;
        step *= 2;
    }

    return NULL;
}

/***********************************************************************
 *           find_reserved_free_area
 *
 * Find a free area between views inside the specified range.
 * The csVirtual section must be held by caller.
 */
static void *find_reserved_free_area( void *base, void *end, size_t size, int top_down )
{
    struct range_entry *range;
    void *start;

    base = ROUND_ADDR( (char *)base + granularity_mask, granularity_mask );
    end = (char *)ROUND_ADDR( (char *)end - size, granularity_mask ) + size;

    if (top_down)
    {
        start = (char *)end - size;
        range = free_ranges_lower_bound( start );
        assert(range != free_ranges_end && range->end >= start);

        if ((char *)range->end - (char *)start < size) start = ROUND_ADDR( (char *)range->end - size, granularity_mask );
        do
        {
            if (start >= end || start < base || (char *)end - (char *)start < size) return NULL;
            if (start < range->end && start >= range->base && (char *)range->end - (char *)start >= size) break;
            if (--range < free_ranges) return NULL;
            start = ROUND_ADDR( (char *)range->end - size, granularity_mask );
        }
        while (1);
    }
    else
    {
        start = base;
        range = free_ranges_lower_bound( start );
        assert(range != free_ranges_end && range->end >= start);

        if (start < range->base) start = ROUND_ADDR( (char *)range->base + granularity_mask, granularity_mask );
        do
        {
            if (start >= end || start < base || (char *)end - (char *)start < size) return NULL;
            if (start < range->end && start >= range->base && (char *)range->end - (char *)start >= size) break;
            if (++range == free_ranges_end) return NULL;
            start = ROUND_ADDR( (char *)range->base + granularity_mask, granularity_mask );
        }
        while (1);
    }
    return start;
}


/***********************************************************************
 *           add_reserved_area
 *
 * Add a reserved area to the list maintained by libwine.
 * The csVirtual section must be held by caller.
 */
static void add_reserved_area( void *addr, size_t size )
{
    TRACE( "adding %p-%p\n", addr, (char *)addr + size );

    if (addr < user_space_limit)
    {
        /* unmap the part of the area that is below the limit */
        assert( (char *)addr + size > (char *)user_space_limit );
        munmap( addr, (char *)user_space_limit - (char *)addr );
        size -= (char *)user_space_limit - (char *)addr;
        addr = user_space_limit;
    }
    /* blow away existing mappings */
    wine_anon_mmap( addr, size, PROT_NONE, MAP_NORESERVE | MAP_FIXED );
    unix_funcs->mmap_add_reserved_area( addr, size );
}


/***********************************************************************
 *           remove_reserved_area
 *
 * Remove a reserved area from the list maintained by libwine.
 * The csVirtual section must be held by caller.
 */
static void remove_reserved_area( void *addr, size_t size )
{
    struct file_view *view;

    TRACE( "removing %p-%p\n", addr, (char *)addr + size );
    unix_funcs->mmap_remove_reserved_area( addr, size );

    /* unmap areas not covered by an existing view */
    WINE_RB_FOR_EACH_ENTRY( view, &views_tree, struct file_view, entry )
    {
        if ((char *)view->base >= (char *)addr + size) break;
        if ((char *)view->base + view->size <= (char *)addr) continue;
        if (view->base > addr) munmap( addr, (char *)view->base - (char *)addr );
        if ((char *)view->base + view->size > (char *)addr + size) return;
        size = (char *)addr + size - ((char *)view->base + view->size);
        addr = (char *)view->base + view->size;
    }
    munmap( addr, size );
}


struct area_boundary
{
    void  *base;
    size_t size;
    void  *boundary;
};

/***********************************************************************
 *           get_area_boundary_callback
 *
 * Get lowest boundary address between reserved area and non-reserved area
 * in the specified region. If no boundaries are found, result is NULL.
 * The csVirtual section must be held by caller.
 */
static int CDECL get_area_boundary_callback( void *start, SIZE_T size, void *arg )
{
    struct area_boundary *area = arg;
    void *end = (char *)start + size;

    area->boundary = NULL;
    if (area->base >= end) return 0;
    if ((char *)start >= (char *)area->base + area->size) return 1;
    if (area->base >= start)
    {
        if ((char *)area->base + area->size > (char *)end)
        {
            area->boundary = end;
            return 1;
        }
        return 0;
    }
    area->boundary = start;
    return 1;
}


/***********************************************************************
 *           is_beyond_limit
 *
 * Check if an address range goes beyond a given limit.
 */
static inline BOOL is_beyond_limit( const void *addr, size_t size, const void *limit )
{
    return (addr >= limit || (const char *)addr + size > (const char *)limit);
}


/***********************************************************************
 *           unmap_area
 *
 * Unmap an area, or simply replace it by an empty mapping if it is
 * in a reserved area. The csVirtual section must be held by caller.
 */
static inline void unmap_area( void *addr, size_t size )
{
    switch (unix_funcs->mmap_is_in_reserved_area( addr, size ))
    {
    case -1: /* partially in a reserved area */
    {
        struct area_boundary area;
        size_t lower_size;
        area.base = addr;
        area.size = size;
        unix_funcs->mmap_enum_reserved_areas( get_area_boundary_callback, &area, 0 );
        assert( area.boundary );
        lower_size = (char *)area.boundary - (char *)addr;
        unmap_area( addr, lower_size );
        unmap_area( area.boundary, size - lower_size );
        break;
    }
    case 1:  /* in a reserved area */
        wine_anon_mmap( addr, size, PROT_NONE, MAP_NORESERVE | MAP_FIXED );
        break;
    default:
    case 0:  /* not in a reserved area */
        if (is_beyond_limit( addr, size, user_space_limit ))
            add_reserved_area( addr, size );
        else
            munmap( addr, size );
        break;
    }
}


/***********************************************************************
 *           alloc_view
 *
 * Allocate a new view. The csVirtual section must be held by caller.
 */
static struct file_view *alloc_view(void)
{
    if (next_free_view)
    {
        struct file_view *ret = next_free_view;
        next_free_view = *(struct file_view **)ret;
        return ret;
    }
    if (view_block_start == view_block_end)
    {
        void *ptr = wine_anon_mmap( NULL, view_block_size, PROT_READ | PROT_WRITE, 0 );
        if (ptr == (void *)-1) return NULL;
        view_block_start = ptr;
        view_block_end = view_block_start + view_block_size / sizeof(*view_block_start);
    }
    return view_block_start++;
}


/***********************************************************************
 *           delete_view
 *
 * Deletes a view. The csVirtual section must be held by caller.
 */
static void delete_view( struct file_view *view ) /* [in] View */
{
    if (!(view->protect & VPROT_SYSTEM)) unmap_area( view->base, view->size );
    set_page_vprot( view->base, view->size, 0 );
    free_ranges_remove_view( view );
    wine_rb_remove( &views_tree, &view->entry );
    *(struct file_view **)view = next_free_view;
    next_free_view = view;
}


/***********************************************************************
 *           create_view
 *
 * Create a view. The csVirtual section must be held by caller.
 */
static NTSTATUS create_view( struct file_view **view_ret, void *base, size_t size, unsigned int vprot )
{
    struct file_view *view;
    int unix_prot = VIRTUAL_GetUnixProt( vprot );

    assert( !((UINT_PTR)base & page_mask) );
    assert( !(size & page_mask) );

    /* Check for overlapping views. This can happen if the previous view
     * was a system view that got unmapped behind our back. In that case
     * we recover by simply deleting it. */

    while ((view = find_view_range( base, size )))
    {
        TRACE( "overlapping view %p-%p for %p-%p\n",
               view->base, (char *)view->base + view->size, base, (char *)base + size );
        assert( view->protect & VPROT_SYSTEM );
        delete_view( view );
    }

    if (!alloc_pages_vprot( base, size )) return STATUS_NO_MEMORY;

    /* Create the view structure */

    if (!(view = alloc_view()))
    {
        FIXME( "out of memory for %p-%p\n", base, (char *)base + size );
        return STATUS_NO_MEMORY;
    }

    view->base    = base;
    view->size    = size;
    view->protect = vprot;
    set_page_vprot( base, size, vprot );

    wine_rb_put( &views_tree, view->base, &view->entry );
    free_ranges_insert_view( view );

    *view_ret = view;

    if (force_exec_prot && (unix_prot & PROT_READ) && !(unix_prot & PROT_EXEC))
    {
        TRACE( "forcing exec permission on %p-%p\n", base, (char *)base + size - 1 );
        mprotect( base, size, unix_prot | PROT_EXEC );
    }
    return STATUS_SUCCESS;
}


/***********************************************************************
 *           VIRTUAL_GetWin32Prot
 *
 * Convert page protections to Win32 flags.
 */
static DWORD VIRTUAL_GetWin32Prot( BYTE vprot, unsigned int map_prot )
{
    DWORD ret;

    if ((vprot & VPROT_WRITECOPY) && (vprot & VPROT_WRITTEN))
        vprot = (vprot & ~VPROT_WRITECOPY) | VPROT_WRITE;
    ret = VIRTUAL_Win32Flags[vprot & 0x0f];
    if (vprot & VPROT_GUARD) ret |= PAGE_GUARD;
    if (map_prot & SEC_NOCACHE) ret |= PAGE_NOCACHE;
    return ret;
}


/***********************************************************************
 *           get_vprot_flags
 *
 * Build page protections from Win32 flags.
 *
 * PARAMS
 *      protect [I] Win32 protection flags
 *
 * RETURNS
 *	Value of page protection flags
 */
static NTSTATUS get_vprot_flags( DWORD protect, unsigned int *vprot, BOOL image )
{
    switch(protect & 0xff)
    {
    case PAGE_READONLY:
        *vprot = VPROT_READ;
        break;
    case PAGE_READWRITE:
        if (image)
            *vprot = VPROT_READ | VPROT_WRITECOPY;
        else
            *vprot = VPROT_READ | VPROT_WRITE;
        break;
    case PAGE_WRITECOPY:
        *vprot = VPROT_READ | VPROT_WRITECOPY;
        break;
    case PAGE_EXECUTE:
        *vprot = VPROT_EXEC;
        break;
    case PAGE_EXECUTE_READ:
        *vprot = VPROT_EXEC | VPROT_READ;
        break;
    case PAGE_EXECUTE_READWRITE:
        if (image)
            *vprot = VPROT_EXEC | VPROT_READ | VPROT_WRITECOPY;
        else
            *vprot = VPROT_EXEC | VPROT_READ | VPROT_WRITE;
        break;
    case PAGE_EXECUTE_WRITECOPY:
        *vprot = VPROT_EXEC | VPROT_READ | VPROT_WRITECOPY;
        break;
    case PAGE_NOACCESS:
        *vprot = 0;
        break;
    default:
        return STATUS_INVALID_PAGE_PROTECTION;
    }
    if (protect & PAGE_GUARD) *vprot |= VPROT_GUARD;
    return STATUS_SUCCESS;
}


/***********************************************************************
 *           mprotect_exec
 *
 * Wrapper for mprotect, adds PROT_EXEC if forced by force_exec_prot
 */
static inline int mprotect_exec( void *base, size_t size, int unix_prot )
{
    if (force_exec_prot && (unix_prot & PROT_READ) && !(unix_prot & PROT_EXEC))
    {
        TRACE( "forcing exec permission on %p-%p\n", base, (char *)base + size - 1 );
        if (!mprotect( base, size, unix_prot | PROT_EXEC )) return 0;
        /* exec + write may legitimately fail, in that case fall back to write only */
        if (!(unix_prot & PROT_WRITE)) return -1;
    }

    return mprotect( base, size, unix_prot );
}


/***********************************************************************
 *           mprotect_range
 *
 * Call mprotect on a page range, applying the protections from the per-page byte.
 */
static void mprotect_range( void *base, size_t size, BYTE set, BYTE clear )
{
    size_t i, count;
    char *addr = ROUND_ADDR( base, page_mask );
    int prot, next;

    size = ROUND_SIZE( base, size );
    prot = VIRTUAL_GetUnixProt( (get_page_vprot( addr ) & ~clear ) | set );
    for (count = i = 1; i < size >> page_shift; i++, count++)
    {
        next = VIRTUAL_GetUnixProt( (get_page_vprot( addr + (count << page_shift) ) & ~clear) | set );
        if (next == prot) continue;
        mprotect_exec( addr, count << page_shift, prot );
        addr += count << page_shift;
        prot = next;
        count = 0;
    }
    if (count) mprotect_exec( addr, count << page_shift, prot );
}


/***********************************************************************
 *           VIRTUAL_SetProt
 *
 * Change the protection of a range of pages.
 *
 * RETURNS
 *	TRUE: Success
 *	FALSE: Failure
 */
static BOOL VIRTUAL_SetProt( struct file_view *view, /* [in] Pointer to view */
                             void *base,      /* [in] Starting address */
                             size_t size,     /* [in] Size in bytes */
                             BYTE vprot )     /* [in] Protections to use */
{
    int unix_prot = VIRTUAL_GetUnixProt(vprot);

    if (view->protect & VPROT_WRITEWATCH)
    {
        /* each page may need different protections depending on write watch flag */
        set_page_vprot_bits( base, size, vprot & ~VPROT_WRITEWATCH, ~vprot & ~(VPROT_WRITEWATCH|VPROT_WRITTEN) );
        mprotect_range( base, size, 0, 0 );
        return TRUE;
    }

    /* if setting stack guard pages, store the permissions first, as the guard may be
     * triggered at any point after mprotect and change the permissions again */
    if ((vprot & VPROT_GUARD) &&
        (base >= NtCurrentTeb()->DeallocationStack) &&
        (base < NtCurrentTeb()->Tib.StackBase))
    {
        set_page_vprot( base, size, vprot );
        mprotect( base, size, unix_prot );
        return TRUE;
    }

    /* check that we can map this memory with PROT_WRITE since we cannot fail later,
     * but we fallback to copying pages for read-only mappings in virtual_handle_fault */
    if ((vprot & VPROT_WRITECOPY) && (view->protect & VPROT_WRITECOPY))
        unix_prot |= PROT_WRITE;

    if (mprotect_exec( base, size, unix_prot )) /* FIXME: last error */
        return FALSE;

    /* each page may need different protections depending on writecopy */
    set_page_vprot_bits( base, size, vprot, ~vprot & ~VPROT_WRITTEN );
    if (vprot & VPROT_WRITECOPY)
        mprotect_range( base, size, 0, 0 );

    return TRUE;
}


/***********************************************************************
 *           set_protection
 *
 * Set page protections on a range of pages
 */
static NTSTATUS set_protection( struct file_view *view, void *base, SIZE_T size, ULONG protect )
{
    unsigned int vprot;
    NTSTATUS status;

    if ((status = get_vprot_flags( protect, &vprot, view->protect & SEC_IMAGE ))) return status;
    if (is_view_valloc( view ))
    {
        if (vprot & VPROT_WRITECOPY) return STATUS_INVALID_PAGE_PROTECTION;
    }
    else
    {
        BYTE access = vprot & (VPROT_READ | VPROT_WRITE | VPROT_EXEC);
        if ((view->protect & access) != access) return STATUS_INVALID_PAGE_PROTECTION;
    }

    if (!VIRTUAL_SetProt( view, base, size, vprot | VPROT_COMMITTED )) return STATUS_ACCESS_DENIED;
    return STATUS_SUCCESS;
}


/***********************************************************************
 *           update_write_watches
 */
static void update_write_watches( void *base, size_t size, size_t accessed_size )
{
    TRACE( "updating watch %p-%p-%p\n", base, (char *)base + accessed_size, (char *)base + size );
    /* clear write watch flag on accessed pages */
    set_page_vprot_bits( base, accessed_size, VPROT_WRITE, VPROT_WRITEWATCH | VPROT_WRITECOPY );
    /* restore page protections on the entire range */
    mprotect_range( base, size, 0, 0 );
}


/***********************************************************************
 *           reset_write_watches
 *
 * Reset write watches in a memory range.
 */
static void reset_write_watches( void *base, SIZE_T size )
{
    set_page_vprot_bits( base, size, VPROT_WRITEWATCH, 0 );
    mprotect_range( base, size, 0, 0 );
}


/***********************************************************************
 *           unmap_extra_space
 *
 * Release the extra memory while keeping the range starting on the granularity boundary.
 */
static inline void *unmap_extra_space( void *ptr, size_t total_size, size_t wanted_size )
{
    if ((ULONG_PTR)ptr & granularity_mask)
    {
        size_t extra = granularity_mask + 1 - ((ULONG_PTR)ptr & granularity_mask);
        munmap( ptr, extra );
        ptr = (char *)ptr + extra;
        total_size -= extra;
    }
    if (total_size > wanted_size)
        munmap( (char *)ptr + wanted_size, total_size - wanted_size );
    return ptr;
}


struct alloc_area
{
    size_t size;
    int    top_down;
    void  *limit;
    void  *result;
    int    unix_prot;
};

/***********************************************************************
 *           alloc_reserved_area_callback
 *
 * Try to map some space inside a reserved area. Callback for mmap_enum_reserved_areas.
 */
static int CDECL alloc_reserved_area_callback( void *start, SIZE_T size, void *arg )
{
    struct alloc_area *alloc = arg;
    void *end = (char *)start + size;

    if (start < address_space_start) start = address_space_start;
    if (is_beyond_limit( start, size, alloc->limit )) end = alloc->limit;
    if (start >= end) return 0;

    /* make sure we don't touch the preloader reserved range */
    if (preload_reserve_end >= start)
    {
        if (preload_reserve_end >= end)
        {
            if (preload_reserve_start <= start) return 0;  /* no space in that area */
            if (preload_reserve_start < end) end = preload_reserve_start;
        }
        else if (preload_reserve_start <= start) start = preload_reserve_end;
        else
        {
            /* range is split in two by the preloader reservation, try first part */
            if ((alloc->result = find_reserved_free_area( start, preload_reserve_start, alloc->size,
                                                          alloc->top_down )))
                return 1;
            /* then fall through to try second part */
            start = preload_reserve_end;
        }
    }
    if ((alloc->result = find_reserved_free_area( start, end, alloc->size, alloc->top_down )))
        return 1;

    return 0;
}

struct area_alloc_reserved
{
    char *map_area_start, *map_area_end, *result;
    size_t size;
    ptrdiff_t step;
    int unix_prot;
    BOOL top_down;
};

static int CDECL alloc_area_in_reserved_or_between_callback( void *start, SIZE_T size, void *arg )
{
    struct area_alloc_reserved *area = arg;
    char *end = (char *)start + size;
    char *intersect_start, *intersect_end;
    char *alloc_start;

    if (area->top_down)
    {
        if (area->map_area_start >= end)
            return 1;

        if (area->map_area_end <= (char *)start)
            return 0;

        intersect_start = max((char *)start, area->map_area_start);
        intersect_end = min((char *)end, area->map_area_end);

        assert(ROUND_ADDR(intersect_start, granularity_mask) == intersect_start);
        assert(ROUND_ADDR(intersect_end + granularity_mask - 1, granularity_mask) == intersect_end);

        alloc_start = ROUND_ADDR( (char *)area->map_area_end - size, granularity_mask );

        if (alloc_start >= intersect_end)
        {
            if ((area->result = try_map_free_area( area->map_area_start, alloc_start + size, area->step,
                    alloc_start, area->size, area->unix_prot )))
                return 1;
        }

        alloc_start = ROUND_ADDR( intersect_end - area->size, granularity_mask );
        if (alloc_start >= intersect_start)
        {
            if ((area->result = wine_anon_mmap( alloc_start, area->size, area->unix_prot, MAP_FIXED )) != alloc_start)
                ERR("Could not map in reserved area, alloc_start %p, size %p.\n",
                        alloc_start, (void *)area->size);
            return 1;
        }

        area->map_area_end = intersect_start;
        if (area->map_area_end - area->map_area_start < area->size)
            return 1;
    }
    else
    {
        if (area->map_area_end <= (char *)start)
            return 1;

        if (area->map_area_start >= (char *)end)
            return 0;

        intersect_start = max((char *)start, area->map_area_start);
        intersect_end = min((char *)end, area->map_area_end);

        assert(ROUND_ADDR(intersect_start, granularity_mask) == intersect_start);
        assert(ROUND_ADDR(intersect_end + granularity_mask - 1, granularity_mask) == intersect_end);

        if (intersect_start - area->map_area_start >= area->size)
        {
            if ((area->result = try_map_free_area( area->map_area_start, intersect_start, area->step,
                    area->map_area_start, area->size, area->unix_prot )))
                return 1;
        }

        if (intersect_end - intersect_start >= area->size)
        {
            if ((area->result = wine_anon_mmap( intersect_start, area->size, area->unix_prot, MAP_FIXED ))
                    != intersect_start)
                ERR("Could not map in reserved area.\n");
            return 1;
        }
        area->map_area_start = intersect_end;
        if (area->map_area_end - area->map_area_start < area->size)
            return 1;
    }

    return 0;
}

static void *alloc_free_area_in_range(struct area_alloc_reserved *area, char *base, char *end,
        size_t size, int top_down, int unix_prot)
{
    char *start;

    TRACE("range %p-%p.\n", base, end);

    if (base >= end) return NULL;

    area->map_area_start = base;
    area->map_area_end = end;

    if (top_down)
    {
        start = ROUND_ADDR( end - size, granularity_mask );
        if (start >= end || start < base)
            return NULL;
    }
    else
    {
        start = ROUND_ADDR( base + granularity_mask, granularity_mask );
        if (!start || start >= end || (char *)end - (char *)start < size)
            return NULL;
    }

    unix_funcs->mmap_enum_reserved_areas( alloc_area_in_reserved_or_between_callback, area, top_down );
    if (area->result)
        return area->result;

    if (top_down)
    {
        start = ROUND_ADDR( area->map_area_end - size, granularity_mask );
        if (start >= area->map_area_end || start < area->map_area_start)
            return NULL;

        return try_map_free_area( area->map_area_start, start + size, area->step,
                start, size, unix_prot );
    }
    else
    {
        start = ROUND_ADDR( area->map_area_start + granularity_mask, granularity_mask );
        if (!start || start >= area->map_area_end
                || area->map_area_end - start < size)
            return NULL;

        return try_map_free_area( start, area->map_area_end, area->step,
                start, size, unix_prot );
    }
}

static void *alloc_free_area(void *limit, size_t size, BOOL top_down, int unix_prot)
{
    struct range_entry *range, *ranges_start, *ranges_end;
    char *reserve_start, *reserve_end;
    struct area_alloc_reserved area;
    char *base, *end;
    int ranges_inc;

    TRACE("limit %p, size %p, top_down %#x.\n", limit, (void *)size, top_down);

    if (top_down)
    {
        ranges_start = free_ranges_end - 1;
        ranges_end = free_ranges - 1;
        ranges_inc = -1;
    }
    else
    {
        ranges_start = free_ranges;
        ranges_end = free_ranges_end;
        ranges_inc = 1;
    }

    memset(&area, 0, sizeof(area));
    area.step = top_down ? -(granularity_mask + 1) : (granularity_mask + 1);
    area.size = size;
    area.top_down = top_down;
    area.unix_prot = unix_prot;

    reserve_start = ROUND_ADDR((char *)preload_reserve_start, granularity_mask);
    reserve_end = ROUND_ADDR((char *)preload_reserve_end + granularity_mask, granularity_mask);

    for (range = ranges_start; range != ranges_end; range += ranges_inc)
    {
        base = range->base;
        end = range->end;

        TRACE("range %p-%p.\n", base, end);

        if (base < (char *)address_space_start) base = (char *)address_space_start;
        if (end > (char *)ROUND_ADDR(limit, granularity_mask)) end = ROUND_ADDR(limit, granularity_mask);

        if (reserve_end >= base)
        {
            if (reserve_end >= end)
            {
                if (reserve_start <= base) continue;  /* no space in that area */
                if (reserve_start < end) end = reserve_start;
            }
            else if (reserve_start <= base) base = reserve_end;
            else
            {
                /* range is split in two by the preloader reservation, try first part */
                if ((area.result = alloc_free_area_in_range(&area, base, reserve_start, size, top_down, unix_prot)))
                    return area.result;
                /* then fall through to try second part */
                base = reserve_end;
            }
        }

        if ((area.result = alloc_free_area_in_range(&area, base, end, size, top_down, unix_prot)))
            return area.result;
    }
    return NULL;
}

/***********************************************************************
 *           map_fixed_area
 *
 * mmap the fixed memory area.
 * The csVirtual section must be held by caller.
 */
static NTSTATUS map_fixed_area( void *base, size_t size, unsigned int vprot )
{
    void *ptr;

    switch (unix_funcs->mmap_is_in_reserved_area( base, size ))
    {
    case -1: /* partially in a reserved area */
    {
        NTSTATUS status;
        struct area_boundary area;
        size_t lower_size;
        area.base = base;
        area.size = size;
        unix_funcs->mmap_enum_reserved_areas( get_area_boundary_callback, &area, 0 );
        assert( area.boundary );
        lower_size = (char *)area.boundary - (char *)base;
        status = map_fixed_area( base, lower_size, vprot );
        if (status == STATUS_SUCCESS)
        {
            status = map_fixed_area( area.boundary, size - lower_size, vprot);
            if (status != STATUS_SUCCESS) unmap_area( base, lower_size );
        }
        return status;
    }
    case 0:  /* not in a reserved area, do a normal allocation */
        if ((ptr = wine_anon_mmap( base, size, VIRTUAL_GetUnixProt(vprot), 0 )) == (void *)-1)
        {
            if (errno == ENOMEM) return STATUS_NO_MEMORY;
            return STATUS_INVALID_PARAMETER;
        }
        if (ptr != base)
        {
            /* We couldn't get the address we wanted */
            if (is_beyond_limit( ptr, size, user_space_limit )) add_reserved_area( ptr, size );
            else munmap( ptr, size );
            return STATUS_CONFLICTING_ADDRESSES;
        }
        break;

    default:
    case 1:  /* in a reserved area, make sure the address is available */
        if (find_view_range( base, size )) return STATUS_CONFLICTING_ADDRESSES;
        /* replace the reserved area by our mapping */
        if ((ptr = wine_anon_mmap( base, size, VIRTUAL_GetUnixProt(vprot), MAP_FIXED )) != base)
            return STATUS_INVALID_PARAMETER;
        break;
    }
    if (is_beyond_limit( ptr, size, working_set_limit )) working_set_limit = address_space_limit;
    return STATUS_SUCCESS;
}

/***********************************************************************
 *           map_view
 *
 * Create a view and mmap the corresponding memory area.
 * The csVirtual section must be held by caller.
 */
static NTSTATUS map_view( struct file_view **view_ret, void *base, size_t size,
                          int top_down, unsigned int vprot, unsigned short zero_bits_64 )
{
    void *ptr;
    NTSTATUS status;

    if (base)
    {
        if (is_beyond_limit( base, size, address_space_limit ))
            return STATUS_WORKING_SET_LIMIT_RANGE;
        status = map_fixed_area( base, size, vprot );
        if (status != STATUS_SUCCESS) return status;
        ptr = base;
    }
    else
    {
        struct alloc_area alloc;
        size_t view_size;

        alloc.size = size;
        alloc.top_down = top_down;
        alloc.limit = (void*)(get_zero_bits_64_mask( zero_bits_64 ) & (UINT_PTR)user_space_limit);
        alloc.unix_prot = VIRTUAL_GetUnixProt(vprot);

        if (is_win64 || zero_bits_64)
        {
            last_already_mapped = NULL;

            if (!(ptr = alloc_free_area( alloc.limit, alloc.size, top_down, alloc.unix_prot)))
                return STATUS_NO_MEMORY;

            if (last_already_mapped)
            {
                void *last_mapped_start, *last_mapped_end;

                TRACE("Permanently excluding %p - %p from free list.\n",
                        last_already_mapped, (char *)last_already_mapped + last_already_mapped_size - 1);
                last_mapped_start = ROUND_ADDR(last_already_mapped, granularity_mask);
                last_mapped_end = ROUND_ADDR((char *)last_already_mapped + last_already_mapped_size + granularity_mask,
                        granularity_mask);
                if (ptr > last_mapped_end || (char *)ptr + size < (char *)last_mapped_start)
                    free_ranges_remove_range(last_mapped_start, last_mapped_end, last_already_mapped);
            }

            TRACE( "got mem in free area %p-%p\n", ptr, (char *)ptr + size );
            goto done;
        }

        if (unix_funcs->mmap_enum_reserved_areas( alloc_reserved_area_callback, &alloc, top_down ))
        {
            ptr = alloc.result;
            TRACE( "got mem in reserved area %p-%p\n", ptr, (char *)ptr + size );
            if (wine_anon_mmap( ptr, size, VIRTUAL_GetUnixProt(vprot), MAP_FIXED ) != ptr)
                return STATUS_INVALID_PARAMETER;
            goto done;
        }

        view_size = size + granularity_mask + 1;

        for (;;)
        {
            if ((ptr = wine_anon_mmap( NULL, view_size, VIRTUAL_GetUnixProt(vprot), 0 )) == (void *)-1)
            {
                if (errno == ENOMEM) return STATUS_NO_MEMORY;
                return STATUS_INVALID_PARAMETER;
            }
            TRACE( "got mem with anon mmap %p-%p\n", ptr, (char *)ptr + size );
            /* if we got something beyond the user limit, unmap it and retry */
            if (is_beyond_limit( ptr, view_size, user_space_limit )) add_reserved_area( ptr, view_size );
            else break;
        }
        ptr = unmap_extra_space( ptr, view_size, size );
    }
done:
    status = create_view( view_ret, ptr, size, vprot );
    if (status != STATUS_SUCCESS) unmap_area( ptr, size );
    return status;
}


/***********************************************************************
 *           map_file_into_view
 *
 * Wrapper for mmap() to map a file into a view, falling back to read if mmap fails.
 * The csVirtual section must be held by caller.
 */
static NTSTATUS map_file_into_view( struct file_view *view, int fd, size_t start, size_t size,
                                    off_t offset, unsigned int vprot, BOOL removable )
{
    void *ptr;
    int prot = VIRTUAL_GetUnixProt( vprot | VPROT_COMMITTED /* make sure it is accessible */ );
    unsigned int flags = MAP_FIXED | ((vprot & VPROT_WRITECOPY) ? MAP_PRIVATE : MAP_SHARED);

    assert( start < view->size );
    assert( start + size <= view->size );

    if (force_exec_prot && (vprot & VPROT_READ))
    {
        TRACE( "forcing exec permission on mapping %p-%p\n",
               (char *)view->base + start, (char *)view->base + start + size - 1 );
        prot |= PROT_EXEC;
    }

    /* only try mmap if media is not removable (or if we require write access) */
    if (!removable || (flags & MAP_SHARED))
    {
        if (mmap( (char *)view->base + start, size, prot, flags, fd, offset ) != (void *)-1)
            goto done;

        switch (errno)
        {
        case EINVAL:  /* file offset is not page-aligned, fall back to read() */
            if (flags & MAP_SHARED) return STATUS_INVALID_PARAMETER;
            break;
        case ENOEXEC:
        case ENODEV:  /* filesystem doesn't support mmap(), fall back to read() */
            if (vprot & VPROT_WRITE)
            {
                ERR( "shared writable mmap not supported, broken filesystem?\n" );
                return STATUS_NOT_SUPPORTED;
            }
            break;
        case EACCES:
        case EPERM:  /* noexec filesystem, fall back to read() */
            if (flags & MAP_SHARED)
            {
                if (prot & PROT_EXEC) ERR( "failed to set PROT_EXEC on file map, noexec filesystem?\n" );
                return STATUS_ACCESS_DENIED;
            }
            if (prot & PROT_EXEC) WARN( "failed to set PROT_EXEC on file map, noexec filesystem?\n" );
            break;
        default:
            return FILE_GetNtStatus();
        }
    }

    /* Reserve the memory with an anonymous mmap */
    ptr = wine_anon_mmap( (char *)view->base + start, size, PROT_READ | PROT_WRITE, MAP_FIXED );
    if (ptr == (void *)-1) return FILE_GetNtStatus();
    /* Now read in the file */
    pread( fd, ptr, size, offset );
    if (prot != (PROT_READ|PROT_WRITE)) mprotect( ptr, size, prot );  /* Set the right protection */
done:
    set_page_vprot( (char *)view->base + start, size, vprot );
    return STATUS_SUCCESS;
}


/***********************************************************************
 *           get_committed_size
 *
 * Get the size of the committed range starting at base.
 * Also return the protections for the first page.
 */
static SIZE_T get_committed_size( struct file_view *view, void *base, BYTE *vprot )
{
    SIZE_T i, start;

    start = ((char *)base - (char *)view->base) >> page_shift;
    *vprot = get_page_vprot( base );

    if (view->protect & SEC_RESERVE)
    {
        SIZE_T ret = 0;
        SERVER_START_REQ( get_mapping_committed_range )
        {
            req->base   = wine_server_client_ptr( view->base );
            req->offset = start << page_shift;
            if (!wine_server_call( req ))
            {
                ret = reply->size;
                if (reply->committed)
                {
                    *vprot |= VPROT_COMMITTED;
                    set_page_vprot_bits( base, ret, VPROT_COMMITTED, 0 );
                }
            }
        }
        SERVER_END_REQ;
        return ret;
    }
    for (i = start + 1; i < view->size >> page_shift; i++)
        if ((*vprot ^ get_page_vprot( (char *)view->base + (i << page_shift) )) & VPROT_COMMITTED) break;
    return (i - start) << page_shift;
}


/***********************************************************************
 *           decommit_view
 *
 * Decommit some pages of a given view.
 * The csVirtual section must be held by caller.
 */
static NTSTATUS decommit_pages( struct file_view *view, size_t start, size_t size )
{
    if (wine_anon_mmap( (char *)view->base + start, size, PROT_NONE, MAP_FIXED ) != (void *)-1)
    {
        set_page_vprot_bits( (char *)view->base + start, size, 0, VPROT_COMMITTED );
        return STATUS_SUCCESS;
    }
    return FILE_GetNtStatus();
}


/***********************************************************************
 *           allocate_dos_memory
 *
 * Allocate the DOS memory range.
 */
static NTSTATUS allocate_dos_memory( struct file_view **view, unsigned int vprot )
{
    size_t size;
    void *addr = NULL;
    void * const low_64k = (void *)0x10000;
    const size_t dosmem_size = 0x110000;
    int unix_prot = VIRTUAL_GetUnixProt( vprot );

    /* check for existing view */

    if (find_view_range( 0, dosmem_size )) return STATUS_CONFLICTING_ADDRESSES;

    /* check without the first 64K */

    if (unix_funcs->mmap_is_in_reserved_area( low_64k, dosmem_size - 0x10000 ) != 1)
    {
        addr = wine_anon_mmap( low_64k, dosmem_size - 0x10000, unix_prot, 0 );
        if (addr != low_64k)
        {
            if (addr != (void *)-1) munmap( addr, dosmem_size - 0x10000 );
            return map_view( view, NULL, dosmem_size, FALSE, vprot, 0 );
        }
    }

    /* now try to allocate the low 64K too */

    if (unix_funcs->mmap_is_in_reserved_area( NULL, 0x10000 ) != 1)
    {
        addr = wine_anon_mmap( (void *)page_size, 0x10000 - page_size, unix_prot, 0 );
        if (addr == (void *)page_size)
        {
            if (!wine_anon_mmap( NULL, page_size, unix_prot, MAP_FIXED ))
            {
                addr = NULL;
                TRACE( "successfully mapped low 64K range\n" );
            }
            else TRACE( "failed to map page 0\n" );
        }
        else
        {
            if (addr != (void *)-1) munmap( addr, 0x10000 - page_size );
            addr = low_64k;
            TRACE( "failed to map low 64K range\n" );
        }
    }

    /* now reserve the whole range */

    size = (char *)dosmem_size - (char *)addr;
    wine_anon_mmap( addr, size, unix_prot, MAP_FIXED );
    return create_view( view, addr, size, vprot );
}


/***********************************************************************
 *           map_pe_header
 *
 * Map the header of a PE file into memory.
 */
static NTSTATUS map_pe_header( void *ptr, size_t size, int fd, BOOL *removable )
{
    if (!size) return STATUS_INVALID_IMAGE_FORMAT;

    if (!*removable)
    {
        if (mmap( ptr, size, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_FIXED|MAP_PRIVATE, fd, 0 ) != (void *)-1)
            return STATUS_SUCCESS;

        switch (errno)
        {
        case EPERM:
        case EACCES:
            WARN( "noexec file system, falling back to read\n" );
            break;
        case ENOEXEC:
        case ENODEV:
            WARN( "file system doesn't support mmap, falling back to read\n" );
            break;
        default:
            return FILE_GetNtStatus();
        }
        *removable = TRUE;
    }
    pread( fd, ptr, size, 0 );
    return STATUS_SUCCESS;  /* page protections will be updated later */
}


/***********************************************************************
 *           map_image
 *
 * Map an executable (PE format) image into memory.
 */
static NTSTATUS map_image( HANDLE hmapping, ACCESS_MASK access, int fd, int top_down, unsigned short zero_bits_64,
                           pe_image_info_t *image_info, int shared_fd, BOOL removable, PVOID *addr_ptr )
{
    IMAGE_DOS_HEADER *dos;
    IMAGE_NT_HEADERS *nt;
    IMAGE_SECTION_HEADER sections[96];
    IMAGE_SECTION_HEADER *sec;
    IMAGE_DATA_DIRECTORY *imports;
    NTSTATUS status = STATUS_CONFLICTING_ADDRESSES;
    SIZE_T header_size, total_size = image_info->map_size;
    int i;
    off_t pos;
    sigset_t sigset;
    struct stat st;
    struct file_view *view = NULL;
    char *ptr, *header_end, *header_start;
    char *base = wine_server_get_ptr( image_info->base );

    if (total_size != image_info->map_size)  /* truncated */
    {
        WARN( "Modules larger than 4Gb (%s) not supported\n", wine_dbgstr_longlong(image_info->map_size) );
        return STATUS_INVALID_PARAMETER;
    }
    if ((ULONG_PTR)base != image_info->base) base = NULL;

    /* zero-map the whole range */

    server_enter_uninterrupted_section( &csVirtual, &sigset );

    if (base >= (char *)address_space_start)  /* make sure the DOS area remains free */
        status = map_view( &view, base, total_size, top_down, SEC_IMAGE | SEC_FILE |
                           VPROT_COMMITTED | VPROT_READ | VPROT_EXEC | VPROT_WRITECOPY, zero_bits_64 );

    if (status != STATUS_SUCCESS)
        status = map_view( &view, NULL, total_size, top_down, SEC_IMAGE | SEC_FILE |
                           VPROT_COMMITTED | VPROT_READ | VPROT_EXEC | VPROT_WRITECOPY, zero_bits_64 );

    if (status != STATUS_SUCCESS) goto error;

    ptr = view->base;
    TRACE_(module)( "mapped PE file at %p-%p\n", ptr, ptr + total_size );

    /* map the header */

    if (fstat( fd, &st ) == -1)
    {
        status = FILE_GetNtStatus();
        goto error;
    }
    header_size = min( image_info->header_size, st.st_size );
    if ((status = map_pe_header( view->base, header_size, fd, &removable )) != STATUS_SUCCESS) goto error;

    status = STATUS_INVALID_IMAGE_FORMAT;  /* generic error */
    dos = (IMAGE_DOS_HEADER *)ptr;
    nt = (IMAGE_NT_HEADERS *)(ptr + dos->e_lfanew);
    header_end = ptr + ROUND_SIZE( 0, header_size );
    memset( ptr + header_size, 0, header_end - (ptr + header_size) );
    if ((char *)(nt + 1) > header_end) goto error;
    header_start = (char*)&nt->OptionalHeader+nt->FileHeader.SizeOfOptionalHeader;
    if (nt->FileHeader.NumberOfSections > ARRAY_SIZE( sections )) goto error;
    if (header_start + sizeof(*sections) * nt->FileHeader.NumberOfSections > header_end) goto error;
    /* Some applications (e.g. the Steam version of Borderlands) map over the top of the section headers,
     * copying the headers into local memory is necessary to properly load such applications. */
    memcpy(sections, header_start, sizeof(*sections) * nt->FileHeader.NumberOfSections);
    sec = sections;

    imports = nt->OptionalHeader.DataDirectory + IMAGE_DIRECTORY_ENTRY_IMPORT;
    if (!imports->Size || !imports->VirtualAddress) imports = NULL;

    /* check for non page-aligned binary */

    if (image_info->image_flags & IMAGE_FLAGS_ImageMappedFlat)
    {
        /* unaligned sections, this happens for native subsystem binaries */
        /* in that case Windows simply maps in the whole file */

        total_size = min( total_size, ROUND_SIZE( 0, st.st_size ));
        if (map_file_into_view( view, fd, 0, total_size, 0, VPROT_COMMITTED | VPROT_READ | VPROT_WRITECOPY,
                                removable ) != STATUS_SUCCESS) goto error;

        /* check that all sections are loaded at the right offset */
        if (nt->OptionalHeader.FileAlignment != nt->OptionalHeader.SectionAlignment) goto error;
        for (i = 0; i < nt->FileHeader.NumberOfSections; i++)
        {
            if (sec[i].VirtualAddress != sec[i].PointerToRawData)
                goto error;  /* Windows refuses to load in that case too */
        }

        /* set the image protections */
        VIRTUAL_SetProt( view, ptr, total_size,
                         VPROT_COMMITTED | VPROT_READ | VPROT_WRITECOPY | VPROT_EXEC );

        /* no relocations are performed on non page-aligned binaries */
        goto done;
    }


    /* map all the sections */

    for (i = pos = 0; i < nt->FileHeader.NumberOfSections; i++, sec++)
    {
        static const SIZE_T sector_align = 0x1ff;
        SIZE_T map_size, file_start, file_size, end;

        if (!sec->Misc.VirtualSize)
            map_size = ROUND_SIZE( 0, sec->SizeOfRawData );
        else
            map_size = ROUND_SIZE( 0, sec->Misc.VirtualSize );

        /* file positions are rounded to sector boundaries regardless of OptionalHeader.FileAlignment */
        file_start = sec->PointerToRawData & ~sector_align;
        file_size = (sec->SizeOfRawData + (sec->PointerToRawData & sector_align) + sector_align) & ~sector_align;
        if (file_size > map_size) file_size = map_size;

        /* a few sanity checks */
        end = sec->VirtualAddress + ROUND_SIZE( sec->VirtualAddress, map_size );
        if (sec->VirtualAddress > total_size || end > total_size || end < sec->VirtualAddress)
        {
            WARN_(module)( "Section %.8s too large (%x+%lx/%lx)\n",
                           sec->Name, sec->VirtualAddress, map_size, total_size );
            goto error;
        }

        if ((sec->Characteristics & IMAGE_SCN_MEM_SHARED) &&
            (sec->Characteristics & IMAGE_SCN_MEM_WRITE))
        {
            TRACE_(module)( "mapping shared section %.8s at %p off %x (%x) size %lx (%lx) flags %x\n",
                            sec->Name, ptr + sec->VirtualAddress,
                            sec->PointerToRawData, (int)pos, file_size, map_size,
                            sec->Characteristics );
            if (map_file_into_view( view, shared_fd, sec->VirtualAddress, map_size, pos,
                                    VPROT_COMMITTED | VPROT_READ | VPROT_WRITE, FALSE ) != STATUS_SUCCESS)
            {
                ERR_(module)( "Could not map shared section %.8s\n", sec->Name );
                goto error;
            }

            /* check if the import directory falls inside this section */
            if (imports && imports->VirtualAddress >= sec->VirtualAddress &&
                imports->VirtualAddress < sec->VirtualAddress + map_size)
            {
                UINT_PTR base = imports->VirtualAddress & ~page_mask;
                UINT_PTR end = base + ROUND_SIZE( imports->VirtualAddress, imports->Size );
                if (end > sec->VirtualAddress + map_size) end = sec->VirtualAddress + map_size;
                if (end > base)
                    map_file_into_view( view, shared_fd, base, end - base,
                                        pos + (base - sec->VirtualAddress),
                                        VPROT_COMMITTED | VPROT_READ | VPROT_WRITECOPY, FALSE );
            }
            pos += map_size;
            continue;
        }

        TRACE_(module)( "mapping section %.8s at %p off %x size %x virt %x flags %x\n",
                        sec->Name, ptr + sec->VirtualAddress,
                        sec->PointerToRawData, sec->SizeOfRawData,
                        sec->Misc.VirtualSize, sec->Characteristics );

        if (!sec->PointerToRawData || !file_size) continue;

        /* Note: if the section is not aligned properly map_file_into_view will magically
         *       fall back to read(), so we don't need to check anything here.
         */
        end = file_start + file_size;
        if (sec->PointerToRawData >= st.st_size ||
            end > ((st.st_size + sector_align) & ~sector_align) ||
            end < file_start ||
            map_file_into_view( view, fd, sec->VirtualAddress, file_size, file_start,
                                VPROT_COMMITTED | VPROT_READ | VPROT_WRITECOPY,
                                removable ) != STATUS_SUCCESS)
        {
            ERR_(module)( "Could not map section %.8s, file probably truncated\n", sec->Name );
            goto error;
        }

        if (file_size & page_mask)
        {
            end = ROUND_SIZE( 0, file_size );
            if (end > map_size) end = map_size;
            TRACE_(module)("clearing %p - %p\n",
                           ptr + sec->VirtualAddress + file_size,
                           ptr + sec->VirtualAddress + end );
            memset( ptr + sec->VirtualAddress + file_size, 0, end - file_size );
            /* clear WRITTEN mark so QueryVirtualMemory returns correct values */
            set_page_vprot_bits( ptr + sec->VirtualAddress + file_size, 1, 0, VPROT_WRITTEN );
        }
    }

    /* set the image protections */

    VIRTUAL_SetProt( view, ptr, ROUND_SIZE( 0, header_size ), VPROT_COMMITTED | VPROT_READ );

    sec = sections;
    for (i = 0; i < nt->FileHeader.NumberOfSections; i++, sec++)
    {
        SIZE_T size;
        BYTE vprot = VPROT_COMMITTED;

        if (sec->Misc.VirtualSize)
            size = ROUND_SIZE( sec->VirtualAddress, sec->Misc.VirtualSize );
        else
            size = ROUND_SIZE( sec->VirtualAddress, sec->SizeOfRawData );

        if (sec->Characteristics & IMAGE_SCN_MEM_READ)    vprot |= VPROT_READ;
        if (sec->Characteristics & IMAGE_SCN_MEM_WRITE)   vprot |= VPROT_WRITECOPY;
        if (sec->Characteristics & IMAGE_SCN_MEM_EXECUTE) vprot |= VPROT_EXEC;

        /* Dumb game crack lets the AOEP point into a data section. Adjust. */
        if ((nt->OptionalHeader.AddressOfEntryPoint >= sec->VirtualAddress) &&
            (nt->OptionalHeader.AddressOfEntryPoint < sec->VirtualAddress + size))
            vprot |= VPROT_EXEC;

        if (!VIRTUAL_SetProt( view, ptr + sec->VirtualAddress, size, vprot ) && (vprot & VPROT_EXEC))
            ERR( "failed to set %08x protection on section %.8s, noexec filesystem?\n",
                 sec->Characteristics, sec->Name );
    }

 done:

    SERVER_START_REQ( map_view )
    {
        req->mapping = wine_server_obj_handle( hmapping );
        req->access  = access;
        req->base    = wine_server_client_ptr( view->base );
        req->size    = view->size;
        req->start   = 0;
        status = wine_server_call( req );
    }
    SERVER_END_REQ;
    if (status) goto error;

    VIRTUAL_DEBUG_DUMP_VIEW( view );
    server_leave_uninterrupted_section( &csVirtual, &sigset );

    *addr_ptr = ptr;
#ifdef VALGRIND_LOAD_PDB_DEBUGINFO
    VALGRIND_LOAD_PDB_DEBUGINFO(fd, ptr, total_size, ptr - base);
#endif
    if (ptr != base) return STATUS_IMAGE_NOT_AT_BASE;
    return STATUS_SUCCESS;

 error:
    if (view) delete_view( view );
    server_leave_uninterrupted_section( &csVirtual, &sigset );
    return status;
}


/***********************************************************************
 *             virtual_map_section
 *
 * Map a file section into memory.
 */
NTSTATUS virtual_map_section( HANDLE handle, PVOID *addr_ptr, unsigned short zero_bits_64, SIZE_T commit_size,
                              const LARGE_INTEGER *offset_ptr, SIZE_T *size_ptr, ULONG alloc_type,
                              ULONG protect, pe_image_info_t *image_info )
{
    NTSTATUS res;
    mem_size_t full_size;
    ACCESS_MASK access;
    SIZE_T size;
    int unix_handle = -1, needs_close;
    unsigned int vprot, sec_flags;
    struct file_view *view;
    HANDLE shared_file;
    LARGE_INTEGER offset;
    sigset_t sigset;

    offset.QuadPart = offset_ptr ? offset_ptr->QuadPart : 0;

    switch(protect)
    {
    case PAGE_NOACCESS:
    case PAGE_READONLY:
    case PAGE_WRITECOPY:
        access = SECTION_MAP_READ;
        break;
    case PAGE_READWRITE:
        access = SECTION_MAP_WRITE;
        break;
    case PAGE_EXECUTE:
    case PAGE_EXECUTE_READ:
    case PAGE_EXECUTE_WRITECOPY:
        access = SECTION_MAP_READ | SECTION_MAP_EXECUTE;
        break;
    case PAGE_EXECUTE_READWRITE:
        access = SECTION_MAP_WRITE | SECTION_MAP_EXECUTE;
        break;
    default:
        return STATUS_INVALID_PAGE_PROTECTION;
    }

    SERVER_START_REQ( get_mapping_info )
    {
        req->handle = wine_server_obj_handle( handle );
        req->access = access;
        wine_server_set_reply( req, image_info, sizeof(*image_info) );
        res = wine_server_call( req );
        sec_flags   = reply->flags;
        full_size   = reply->size;
        shared_file = wine_server_ptr_handle( reply->shared_file );
    }
    SERVER_END_REQ;
    if (res) return res;

    if ((res = server_get_unix_fd( handle, 0, &unix_handle, &needs_close, NULL, NULL ))) goto done;

    if (sec_flags & SEC_IMAGE)
    {
        if (shared_file)
        {
            int shared_fd, shared_needs_close;

            if ((res = server_get_unix_fd( shared_file, FILE_READ_DATA|FILE_WRITE_DATA,
                                           &shared_fd, &shared_needs_close, NULL, NULL ))) goto done;
            res = map_image( handle, access, unix_handle, alloc_type & MEM_TOP_DOWN, zero_bits_64, image_info,
                             shared_fd, needs_close, addr_ptr );
            if (shared_needs_close) close( shared_fd );
            close_handle( shared_file );
        }
        else
        {
            res = map_image( handle, access, unix_handle, alloc_type & MEM_TOP_DOWN, zero_bits_64, image_info,
                             -1, needs_close, addr_ptr );
        }
        if (needs_close) close( unix_handle );
        if (res >= 0) *size_ptr = image_info->map_size;
        return res;
    }

    res = STATUS_INVALID_PARAMETER;
    if (offset.QuadPart >= full_size) goto done;
    if (*size_ptr)
    {
        size = *size_ptr;
        if (size > full_size - offset.QuadPart)
        {
            res = STATUS_INVALID_VIEW_SIZE;
            goto done;
        }
    }
    else
    {
        size = full_size - offset.QuadPart;
        if (size != full_size - offset.QuadPart)  /* truncated */
        {
            WARN( "Files larger than 4Gb (%s) not supported on this platform\n",
                  wine_dbgstr_longlong(full_size) );
            goto done;
        }
    }
    if (!(size = ROUND_SIZE( 0, size ))) goto done;  /* wrap-around */

    /* Reserve a properly aligned area */

    server_enter_uninterrupted_section( &csVirtual, &sigset );

    get_vprot_flags( protect, &vprot, sec_flags & SEC_IMAGE );
    vprot |= sec_flags;
    if (!(sec_flags & SEC_RESERVE)) vprot |= VPROT_COMMITTED;
    res = map_view( &view, *addr_ptr, size, alloc_type & MEM_TOP_DOWN, vprot, zero_bits_64 );
    if (res)
    {
        server_leave_uninterrupted_section( &csVirtual, &sigset );
        goto done;
    }

    /* Map the file */

    TRACE( "handle=%p size=%lx offset=%x%08x\n", handle, size, offset.u.HighPart, offset.u.LowPart );

    res = map_file_into_view( view, unix_handle, 0, size, offset.QuadPart, vprot, needs_close );
    if (res == STATUS_SUCCESS)
    {
        SERVER_START_REQ( map_view )
        {
            req->mapping = wine_server_obj_handle( handle );
            req->access  = access;
            req->base    = wine_server_client_ptr( view->base );
            req->size    = size;
            req->start   = offset.QuadPart;
            res = wine_server_call( req );
        }
        SERVER_END_REQ;
    }

    if (res == STATUS_SUCCESS)
    {
        *addr_ptr = view->base;
        *size_ptr = size;
        VIRTUAL_DEBUG_DUMP_VIEW( view );
    }
    else
    {
        ERR( "mapping %p %lx %x%08x failed\n", view->base, size, offset.u.HighPart, offset.u.LowPart );
        delete_view( view );
    }

    server_leave_uninterrupted_section( &csVirtual, &sigset );

done:
    if (needs_close) close( unix_handle );
    return res;
}


struct alloc_virtual_heap
{
    void  *base;
    size_t size;
};

/* callback for mmap_enum_reserved_areas to allocate space for the virtual heap */
static int CDECL alloc_virtual_heap( void *base, SIZE_T size, void *arg )
{
    struct alloc_virtual_heap *alloc = arg;

    if (is_beyond_limit( base, size, address_space_limit )) address_space_limit = (char *)base + size;
    if (size < alloc->size) return 0;
    if (is_win64 && base < (void *)0x80000000) return 0;
    alloc->base = wine_anon_mmap( (char *)base + size - alloc->size, alloc->size,
                                  PROT_READ|PROT_WRITE, MAP_FIXED );
    return (alloc->base != (void *)-1);
}

/***********************************************************************
 *           virtual_init
 */
void virtual_init(void)
{
    const char *preload;
    struct alloc_virtual_heap alloc_views;
    size_t size;

#if !defined(__i386__) && !defined(__x86_64__) && !defined(__arm__) && !defined(__aarch64__)
    page_size = sysconf( _SC_PAGESIZE );
    page_mask = page_size - 1;
    /* Make sure we have a power of 2 */
    assert( !(page_size & page_mask) );
    page_shift = 0;
    while ((1 << page_shift) != page_size) page_shift++;
#ifdef _WIN64
    address_space_limit = (void *)(((1UL << 47) - 1) & ~page_mask);
#else
    address_space_limit = (void *)~page_mask;
#endif
    user_space_limit = working_set_limit = address_space_limit;
#endif
    if ((preload = getenv("WINEPRELOADRESERVE")))
    {
        unsigned long start, end;
        if (sscanf( preload, "%lx-%lx", &start, &end ) == 2)
        {
            preload_reserve_start = (void *)start;
            preload_reserve_end = (void *)end;
            /* some apps start inside the DOS area */
            if (preload_reserve_start)
                address_space_start = min( address_space_start, preload_reserve_start );
        }
        TRACE("preload reserve %p-%p.\n", preload_reserve_start, preload_reserve_end);
    }

    size = ROUND_SIZE( 0, sizeof(TEB) ) + max( MINSIGSTKSZ, 8192 );
    /* find the first power of two not smaller than size */
    signal_stack_align = page_shift;
    while ((1u << signal_stack_align) < size) signal_stack_align++;
    signal_stack_mask = (1 << signal_stack_align) - 1;
    signal_stack_size = (1 << signal_stack_align) - ROUND_SIZE( 0, sizeof(TEB) );

    /* try to find space in a reserved area for the views and pages protection table */
#ifdef _WIN64
    pages_vprot_size = ((size_t)address_space_limit >> page_shift >> pages_vprot_shift) + 1;
    alloc_views.size = 2 * view_block_size + pages_vprot_size * sizeof(*pages_vprot);
#else
    alloc_views.size = 2 * view_block_size + (1U << (32 - page_shift));
#endif
    if (unix_funcs->mmap_enum_reserved_areas( alloc_virtual_heap, &alloc_views, 1 ))
        unix_funcs->mmap_remove_reserved_area( alloc_views.base, alloc_views.size );
    else
        alloc_views.base = wine_anon_mmap( NULL, alloc_views.size, PROT_READ | PROT_WRITE, 0 );

    assert( alloc_views.base != (void *)-1 );
    view_block_start = alloc_views.base;
    view_block_end = view_block_start + view_block_size / sizeof(*view_block_start);
    free_ranges = (void *)((char *)alloc_views.base + view_block_size);
    pages_vprot = (void *)((char *)alloc_views.base + 2 * view_block_size);
    wine_rb_init( &views_tree, compare_view );

    free_ranges[0].base = (void *)0;
    free_ranges[0].end = (void *)~0;
    free_ranges_end = free_ranges + 1;

    /* make the DOS area accessible (except the low 64K) to hide bugs in broken apps like Excel 2003 */
    size = (char *)address_space_start - (char *)0x10000;
    if (size && unix_funcs->mmap_is_in_reserved_area( (void*)0x10000, size ) == 1)
        wine_anon_mmap( (void *)0x10000, size, PROT_READ | PROT_WRITE, MAP_FIXED );
}


/***********************************************************************
 *           virtual_get_system_info
 */
void virtual_get_system_info( SYSTEM_BASIC_INFORMATION *info )
{
#ifdef HAVE_SYSINFO
    struct sysinfo sinfo;
#endif

    info->unknown                 = 0;
    info->KeMaximumIncrement      = 0;  /* FIXME */
    info->PageSize                = page_size;
    info->MmLowestPhysicalPage    = 1;
    info->MmHighestPhysicalPage   = 0x7fffffff / page_size;
#ifdef HAVE_SYSINFO
    if (!sysinfo(&sinfo))
    {
        ULONG64 total = (ULONG64)sinfo.totalram * sinfo.mem_unit;
        info->MmHighestPhysicalPage = max(1, total / page_size);
    }
#endif
    info->MmNumberOfPhysicalPages = info->MmHighestPhysicalPage - info->MmLowestPhysicalPage;
    info->AllocationGranularity   = granularity_mask + 1;
    info->LowestUserAddress       = (void *)0x10000;
    info->HighestUserAddress      = (char *)user_space_limit - 1;
    info->ActiveProcessorsAffinityMask = get_system_affinity_mask();
    info->NumberOfProcessors      = NtCurrentTeb()->Peb->NumberOfProcessors;
}


/***********************************************************************
 *           virtual_create_builtin_view
 */
NTSTATUS virtual_create_builtin_view( void *module )
{
    NTSTATUS status;
    sigset_t sigset;
    IMAGE_NT_HEADERS *nt = RtlImageNtHeader( module );
    SIZE_T size = nt->OptionalHeader.SizeOfImage;
    IMAGE_SECTION_HEADER *sec;
    struct file_view *view;
    void *base;
    int i;

    size = ROUND_SIZE( module, size );
    base = ROUND_ADDR( module, page_mask );
    server_enter_uninterrupted_section( &csVirtual, &sigset );
    status = create_view( &view, base, size, SEC_IMAGE | SEC_FILE | VPROT_SYSTEM |
                          VPROT_COMMITTED | VPROT_READ | VPROT_WRITECOPY | VPROT_EXEC );
    if (!status)
    {
        TRACE( "created %p-%p\n", base, (char *)base + size );

        /* The PE header is always read-only, no write, no execute. */
        set_page_vprot( base, page_size, VPROT_COMMITTED | VPROT_READ );

        sec = (IMAGE_SECTION_HEADER *)((char *)&nt->OptionalHeader + nt->FileHeader.SizeOfOptionalHeader);
        for (i = 0; i < nt->FileHeader.NumberOfSections; i++)
        {
            BYTE flags = VPROT_COMMITTED;

            if (sec[i].Characteristics & IMAGE_SCN_MEM_EXECUTE) flags |= VPROT_EXEC;
            if (sec[i].Characteristics & IMAGE_SCN_MEM_READ) flags |= VPROT_READ;
            if (sec[i].Characteristics & IMAGE_SCN_MEM_WRITE) flags |= VPROT_WRITE;
            set_page_vprot( (char *)base + sec[i].VirtualAddress, sec[i].Misc.VirtualSize, flags );
        }
        VIRTUAL_DEBUG_DUMP_VIEW( view );
    }
    server_leave_uninterrupted_section( &csVirtual, &sigset );
    return status;
}


/***********************************************************************
 *           virtual_alloc_first_teb
 */
TEB *virtual_alloc_first_teb(void)
{
    TEB *teb;
    PEB *peb;
    SIZE_T peb_size = page_size;
    SIZE_T teb_size = signal_stack_mask + 1;
    SIZE_T total = 32 * teb_size;

    NtAllocateVirtualMemory( NtCurrentProcess(), (void **)&teb_block, 0, &total,
                             MEM_RESERVE | MEM_TOP_DOWN, PAGE_READWRITE );
    teb_block_pos = 30;
    teb = (TEB *)((char *)teb_block + 30 * teb_size);
    peb = (PEB *)((char *)teb_block + 32 * teb_size - peb_size);
    NtAllocateVirtualMemory( NtCurrentProcess(), (void **)&teb, 0, &teb_size, MEM_COMMIT, PAGE_READWRITE );
    NtAllocateVirtualMemory( NtCurrentProcess(), (void **)&peb, 0, &peb_size, MEM_COMMIT, PAGE_READWRITE );

    teb->Peb = peb;
    teb->Tib.Self = &teb->Tib;
    teb->Tib.ExceptionList = (void *)~0ul;
    teb->Tib.StackBase = (void *)~0ul;
    teb->StaticUnicodeString.Buffer = teb->StaticUnicodeBuffer;
    teb->StaticUnicodeString.MaximumLength = sizeof(teb->StaticUnicodeBuffer);
    signal_init_threading();
    signal_alloc_thread( teb );
    signal_init_thread( teb );
    use_locks = TRUE;
    return teb;
}


/***********************************************************************
 *           virtual_alloc_teb
 */
NTSTATUS virtual_alloc_teb( TEB **ret_teb )
{
    sigset_t sigset;
    TEB *teb = NULL;
    NTSTATUS status = STATUS_SUCCESS;
    SIZE_T teb_size = signal_stack_mask + 1;

    server_enter_uninterrupted_section( &csVirtual, &sigset );
    if (next_free_teb)
    {
        teb = next_free_teb;
        next_free_teb = *(TEB **)teb;
        memset( teb, 0, sizeof(*teb) );
    }
    else
    {
        if (!teb_block_pos)
        {
            void *addr = NULL;
            SIZE_T total = 32 * teb_size;

            if ((status = NtAllocateVirtualMemory( NtCurrentProcess(), &addr, 0, &total,
                                                   MEM_RESERVE, PAGE_READWRITE )))
            {
                server_leave_uninterrupted_section( &csVirtual, &sigset );
                return status;
            }
            teb_block = addr;
            teb_block_pos = 32;
        }
        teb = (TEB *)((char *)teb_block + --teb_block_pos * teb_size);
        NtAllocateVirtualMemory( NtCurrentProcess(), (void **)&teb, 0, &teb_size,
                                 MEM_COMMIT, PAGE_READWRITE );
    }
    server_leave_uninterrupted_section( &csVirtual, &sigset );

    *ret_teb = teb;
    teb->Peb = NtCurrentTeb()->Peb;
    teb->Tib.Self = &teb->Tib;
    teb->Tib.ExceptionList = (void *)~0UL;
    teb->StaticUnicodeString.Buffer = teb->StaticUnicodeBuffer;
    teb->StaticUnicodeString.MaximumLength = sizeof(teb->StaticUnicodeBuffer);
    if ((status = signal_alloc_thread( teb )))
    {
        server_enter_uninterrupted_section( &csVirtual, &sigset );
        *(TEB **)teb = next_free_teb;
        next_free_teb = teb;
        server_leave_uninterrupted_section( &csVirtual, &sigset );
    }
    return status;
}


/***********************************************************************
 *           virtual_free_teb
 */
void virtual_free_teb( TEB *teb )
{
    struct ntdll_thread_data *thread_data = (struct ntdll_thread_data *)&teb->GdiTebBatch;
    SIZE_T size;
    sigset_t sigset;

    signal_free_thread( teb );
    if (teb->DeallocationStack)
    {
        size = 0;
        NtFreeVirtualMemory( GetCurrentProcess(), &teb->DeallocationStack, &size, MEM_RELEASE );
    }
    if (thread_data->start_stack)
    {
        size = 0;
        NtFreeVirtualMemory( GetCurrentProcess(), &thread_data->start_stack, &size, MEM_RELEASE );
    }

    server_enter_uninterrupted_section( &csVirtual, &sigset );
    *(TEB **)teb = next_free_teb;
    next_free_teb = teb;
    server_leave_uninterrupted_section( &csVirtual, &sigset );
}


/***********************************************************************
 *           virtual_alloc_thread_stack
 */
NTSTATUS virtual_alloc_thread_stack( INITIAL_TEB *stack, SIZE_T reserve_size, SIZE_T commit_size, SIZE_T *pthread_size )
{
    struct file_view *view;
    NTSTATUS status;
    sigset_t sigset;
    SIZE_T size, extra_size = 0;

    if (!reserve_size || !commit_size)
    {
        IMAGE_NT_HEADERS *nt = RtlImageNtHeader( NtCurrentTeb()->Peb->ImageBaseAddress );
        if (!reserve_size) reserve_size = nt->OptionalHeader.SizeOfStackReserve;
        if (!commit_size) commit_size = nt->OptionalHeader.SizeOfStackCommit;
    }

    size = max( reserve_size, commit_size );
    if (size < 1024 * 1024) size = 1024 * 1024;  /* Xlib needs a large stack */
    size = (size + 0xffff) & ~0xffff;  /* round to 64K boundary */
    if (pthread_size) *pthread_size = extra_size = max( page_size, ROUND_SIZE( 0, *pthread_size ));

    server_enter_uninterrupted_section( &csVirtual, &sigset );

    if ((status = map_view( &view, NULL, size + extra_size, FALSE,
                            VPROT_READ | VPROT_WRITE | VPROT_COMMITTED, 0 )) != STATUS_SUCCESS)
        goto done;

#ifdef VALGRIND_STACK_REGISTER
    VALGRIND_STACK_REGISTER( view->base, (char *)view->base + view->size );
#endif

    /* setup no access guard page */
    set_page_vprot( view->base, page_size, VPROT_COMMITTED );
    set_page_vprot( (char *)view->base + page_size, page_size,
                    VPROT_READ | VPROT_WRITE | VPROT_COMMITTED | VPROT_GUARD );
    mprotect_range( view->base, 2 * page_size, 0, 0 );
    VIRTUAL_DEBUG_DUMP_VIEW( view );

    if (extra_size)
    {
        struct file_view *extra_view;

        /* shrink the first view and create a second one for the extra size */
        /* this allows the app to free the stack without freeing the thread start portion */
        view->size -= extra_size;
        status = create_view( &extra_view, (char *)view->base + view->size, extra_size,
                              VPROT_READ | VPROT_WRITE | VPROT_COMMITTED );
        if (status != STATUS_SUCCESS)
        {
            unmap_area( (char *)view->base + view->size, extra_size );
            delete_view( view );
            goto done;
        }
    }

    /* note: limit is lower than base since the stack grows down */
    stack->OldStackBase = 0;
    stack->OldStackLimit = 0;
    stack->DeallocationStack = view->base;
    stack->StackBase = (char *)view->base + view->size;
    stack->StackLimit = (char *)view->base + 2 * page_size;
    ((struct ntdll_thread_data *)&NtCurrentTeb()->GdiTebBatch)->pthread_stack = view->base;

done:
    server_leave_uninterrupted_section( &csVirtual, &sigset );
    return status;
}


/***********************************************************************
 *           virtual_clear_thread_stack
 *
 * Clear the stack contents before calling the main entry point, some broken apps need that.
 */
void virtual_clear_thread_stack( void *stack_end )
{
    void *stack = NtCurrentTeb()->Tib.StackLimit;
    size_t size = (char *)stack_end - (char *)stack;

    wine_anon_mmap( stack, size, PROT_READ | PROT_WRITE, MAP_FIXED );
    if (force_exec_prot) mprotect( stack, size, PROT_READ | PROT_WRITE | PROT_EXEC );
}

/**********************************************************************
 *           RtlCreateUserStack (NTDLL.@)
 */
NTSTATUS WINAPI RtlCreateUserStack( SIZE_T commit, SIZE_T reserve, ULONG zero_bits,
                                    SIZE_T commit_align, SIZE_T reserve_align, INITIAL_TEB *stack )
{
    TRACE("commit %#lx, reserve %#lx, zero_bits %u, commit_align %#lx, reserve_align %#lx, stack %p\n",
            commit, reserve, zero_bits, commit_align, reserve_align, stack);

    if (!commit_align || !reserve_align)
        return STATUS_INVALID_PARAMETER;

    if (!commit || !reserve)
    {
        IMAGE_NT_HEADERS *nt = RtlImageNtHeader( NtCurrentTeb()->Peb->ImageBaseAddress );
        if (!reserve) reserve = nt->OptionalHeader.SizeOfStackReserve;
        if (!commit) commit = nt->OptionalHeader.SizeOfStackCommit;
    }

    reserve = (reserve + reserve_align - 1) & ~(reserve_align - 1);
    commit = (commit + commit_align - 1) & ~(commit_align - 1);

    return virtual_alloc_thread_stack( stack, reserve, commit, NULL );
}

/**********************************************************************
 *           RtlFreeUserStack (NTDLL.@)
 */
void WINAPI RtlFreeUserStack( void *stack )
{
    SIZE_T size = 0;

    TRACE("stack %p\n", stack);

    NtFreeVirtualMemory( NtCurrentProcess(), &stack, &size, MEM_RELEASE );
}

/***********************************************************************
 *           virtual_handle_fault
 */
NTSTATUS virtual_handle_fault( LPCVOID addr, DWORD err, BOOL on_signal_stack )
{
    NTSTATUS ret = STATUS_ACCESS_VIOLATION;
    void *page = ROUND_ADDR( addr, page_mask );
    sigset_t sigset;
    BYTE vprot;

    server_enter_uninterrupted_section( &csVirtual, &sigset );
    vprot = get_page_vprot( page );
    if (!on_signal_stack && (vprot & VPROT_GUARD))
    {
        set_page_vprot_bits( page, page_size, 0, VPROT_GUARD );
        mprotect_range( page, page_size, 0, 0 );
        ret = STATUS_GUARD_PAGE_VIOLATION;
    }
    else if (err & EXCEPTION_WRITE_FAULT)
    {
        if (vprot & VPROT_WRITEWATCH)
        {
            set_page_vprot_bits( page, page_size, 0, VPROT_WRITEWATCH );
            mprotect_range( page, page_size, 0, 0 );
        }
        if ((vprot & VPROT_WRITECOPY) && (vprot & VPROT_COMMITTED))
        {
            struct file_view *view = VIRTUAL_FindView( page, 0 );

            set_page_vprot_bits( page, page_size, VPROT_WRITE | VPROT_WRITTEN, VPROT_WRITECOPY );
            if (view->protect & VPROT_WRITECOPY)
            {
                mprotect_range( page, page_size, 0, 0 );
            }
            else
            {
                static BYTE *temp_page = NULL;
                if (!temp_page)
                    temp_page = wine_anon_mmap( NULL, page_size, PROT_READ | PROT_WRITE, 0 );

                /* original mapping is shared, replace with a private page */
                memcpy( temp_page, page, page_size );
                wine_anon_mmap( page, page_size, VIRTUAL_GetUnixProt(vprot | VPROT_WRITE | VPROT_WRITTEN), MAP_FIXED );
                memcpy( page, temp_page, page_size );
            }
        }
        /* ignore fault if page is writable now */
        if (VIRTUAL_GetUnixProt( get_page_vprot( page )) & PROT_WRITE) ret = STATUS_SUCCESS;
    }
    else if (!err && (VIRTUAL_GetUnixProt( vprot ) & PROT_READ) && is_system_range( page, page_size ))
    {
        int unix_prot = VIRTUAL_GetUnixProt( vprot );
        unsigned char vec;

        mprotect_range( page, page_size, 0, 0 );
        if (!mincore( page, page_size, &vec ) && (vec & 1))
            ret = STATUS_SUCCESS;
        else if (wine_anon_mmap( page, page_size, unix_prot, MAP_FIXED ) == page)
            ret = STATUS_SUCCESS;
        else
            set_page_vprot_bits( page, page_size, 0, VPROT_READ | VPROT_EXEC );
    }
    server_leave_uninterrupted_section( &csVirtual, &sigset );
    return ret;
}


/***********************************************************************
 *           check_write_access
 *
 * Check if the memory range is writable, temporarily disabling write watches if necessary.
 */
static NTSTATUS check_write_access( void *base, size_t size, BOOL *has_write_watch )
{
    size_t i;
    char *addr = ROUND_ADDR( base, page_mask );

    size = ROUND_SIZE( base, size );
    for (i = 0; i < size; i += page_size)
    {
        BYTE vprot = get_page_vprot( addr + i );
        if (vprot & VPROT_WRITEWATCH) *has_write_watch = TRUE;
        if (vprot & VPROT_WRITECOPY)
        {
            vprot = (vprot & ~VPROT_WRITECOPY) | VPROT_WRITE;
            *has_write_watch = TRUE;
        }
        if (!(VIRTUAL_GetUnixProt( vprot & ~VPROT_WRITEWATCH ) & PROT_WRITE))
            return STATUS_INVALID_USER_BUFFER;
    }
    if (*has_write_watch)
        mprotect_range( addr, size, VPROT_WRITE, VPROT_WRITEWATCH | VPROT_WRITECOPY );  /* temporarily enable write access */
    return STATUS_SUCCESS;
}


/***********************************************************************
 *           virtual_locked_server_call
 */
unsigned int virtual_locked_server_call( void *req_ptr )
{
    struct __server_request_info * const req = req_ptr;
    sigset_t sigset;
    void *addr = req->reply_data;
    data_size_t size = req->u.req.request_header.reply_size;
    BOOL has_write_watch = FALSE;
    unsigned int ret = STATUS_ACCESS_VIOLATION;

    if (!size) return wine_server_call( req_ptr );

    server_enter_uninterrupted_section( &csVirtual, &sigset );
    if (!(ret = check_write_access( addr, size, &has_write_watch )))
    {
        ret = server_call_unlocked( req );
        if (has_write_watch) update_write_watches( addr, size, wine_server_reply_size( req ));
    }
    server_leave_uninterrupted_section( &csVirtual, &sigset );
    return ret;
}


/***********************************************************************
 *           virtual_locked_read
 */
ssize_t virtual_locked_read( int fd, void *addr, size_t size )
{
    sigset_t sigset;
    BOOL has_write_watch = FALSE;
    int err = EFAULT;

    ssize_t ret = read( fd, addr, size );
    if (ret != -1 || errno != EFAULT) return ret;

    server_enter_uninterrupted_section( &csVirtual, &sigset );
    if (!check_write_access( addr, size, &has_write_watch ))
    {
        ret = read( fd, addr, size );
        err = errno;
        if (has_write_watch) update_write_watches( addr, size, max( 0, ret ));
    }
    server_leave_uninterrupted_section( &csVirtual, &sigset );
    errno = err;
    return ret;
}


/***********************************************************************
 *           virtual_locked_pread
 */
ssize_t virtual_locked_pread( int fd, void *addr, size_t size, off_t offset )
{
    sigset_t sigset;
    BOOL has_write_watch = FALSE;
    int err = EFAULT;

    ssize_t ret = pread( fd, addr, size, offset );
    if (ret != -1 || errno != EFAULT) return ret;

    server_enter_uninterrupted_section( &csVirtual, &sigset );
    if (!check_write_access( addr, size, &has_write_watch ))
    {
        ret = pread( fd, addr, size, offset );
        err = errno;
        if (has_write_watch) update_write_watches( addr, size, max( 0, ret ));
    }
    server_leave_uninterrupted_section( &csVirtual, &sigset );
    errno = err;
    return ret;
}


/***********************************************************************
 *           __wine_locked_recvmsg
 */
ssize_t CDECL __wine_locked_recvmsg( int fd, struct msghdr *hdr, int flags )
{
    sigset_t sigset;
    size_t i;
    BOOL has_write_watch = FALSE;
    int err = EFAULT;

    ssize_t ret = recvmsg( fd, hdr, flags );
    if (ret != -1 || errno != EFAULT) return ret;

    server_enter_uninterrupted_section( &csVirtual, &sigset );
    for (i = 0; i < hdr->msg_iovlen; i++)
        if (check_write_access( hdr->msg_iov[i].iov_base, hdr->msg_iov[i].iov_len, &has_write_watch ))
            break;
    if (i == hdr->msg_iovlen)
    {
        ret = recvmsg( fd, hdr, flags );
        err = errno;
    }
    if (has_write_watch)
        while (i--) update_write_watches( hdr->msg_iov[i].iov_base, hdr->msg_iov[i].iov_len, 0 );

    server_leave_uninterrupted_section( &csVirtual, &sigset );
    errno = err;
    return ret;
}


/***********************************************************************
 *           virtual_is_valid_code_address
 */
BOOL virtual_is_valid_code_address( const void *addr, SIZE_T size )
{
    struct file_view *view;
    BOOL ret = FALSE;
    sigset_t sigset;

    server_enter_uninterrupted_section( &csVirtual, &sigset );
    if ((view = VIRTUAL_FindView( addr, size )))
        ret = !(view->protect & VPROT_SYSTEM);  /* system views are not visible to the app */
    server_leave_uninterrupted_section( &csVirtual, &sigset );
    return ret;
}


/***********************************************************************
 *           virtual_handle_stack_fault
 *
 * Handle an access fault inside the current thread stack.
 * Return 1 if safely handled, -1 if handled into the overflow space.
 * Called from inside a signal handler.
 */
int virtual_handle_stack_fault( void *addr )
{
    int ret = 0;

    if ((char *)addr < (char *)NtCurrentTeb()->DeallocationStack) return 0;
    if ((char *)addr >= (char *)NtCurrentTeb()->Tib.StackBase) return 0;

    RtlEnterCriticalSection( &csVirtual );  /* no need for signal masking inside signal handler */
    if (get_page_vprot( addr ) & VPROT_GUARD)
    {
        size_t guaranteed = max( NtCurrentTeb()->GuaranteedStackBytes, page_size * (is_win64 ? 2 : 1) );
        char *page = ROUND_ADDR( addr, page_mask );
        set_page_vprot_bits( page, page_size, 0, VPROT_GUARD );
        mprotect_range( page, page_size, 0, 0 );
        if (page >= (char *)NtCurrentTeb()->DeallocationStack + page_size + guaranteed)
        {
            set_page_vprot_bits( page - page_size, page_size, VPROT_COMMITTED | VPROT_GUARD, 0 );
            mprotect_range( page - page_size, page_size, 0, 0 );
            ret = 1;
        }
        else  /* inside guaranteed space -> overflow exception */
        {
            page = (char *)NtCurrentTeb()->DeallocationStack + page_size;
            set_page_vprot_bits( page, guaranteed, VPROT_COMMITTED, VPROT_GUARD );
            mprotect_range( page, guaranteed, 0, 0 );
            ret = -1;
        }
        NtCurrentTeb()->Tib.StackLimit = page;
    }
    RtlLeaveCriticalSection( &csVirtual );
    return ret;
}


/***********************************************************************
 *           virtual_check_buffer_for_read
 *
 * Check if a memory buffer can be read, triggering page faults if needed for DIB section access.
 */
BOOL virtual_check_buffer_for_read( const void *ptr, SIZE_T size )
{
    if (!size) return TRUE;
    if (!ptr) return FALSE;

    __TRY
    {
        volatile const char *p = ptr;
        char dummy __attribute__((unused));
        SIZE_T count = size;

        while (count > page_size)
        {
            dummy = *p;
            p += page_size;
            count -= page_size;
        }
        dummy = p[0];
        dummy = p[count - 1];
    }
    __EXCEPT_PAGE_FAULT
    {
        return FALSE;
    }
    __ENDTRY
    return TRUE;
}


/***********************************************************************
 *           virtual_check_buffer_for_write
 *
 * Check if a memory buffer can be written to, triggering page faults if needed for write watches.
 */
BOOL virtual_check_buffer_for_write( void *ptr, SIZE_T size )
{
    if (!size) return TRUE;
    if (!ptr) return FALSE;

    __TRY
    {
        volatile char *p = ptr;
        SIZE_T count = size;

        while (count > page_size)
        {
            *p |= 0;
            p += page_size;
            count -= page_size;
        }
        p[0] |= 0;
        p[count - 1] |= 0;
    }
    __EXCEPT_PAGE_FAULT
    {
        return FALSE;
    }
    __ENDTRY
    return TRUE;
}


/***********************************************************************
 *           virtual_uninterrupted_read_memory
 *
 * Similar to NtReadVirtualMemory, but without wineserver calls. Moreover
 * permissions are checked before accessing each page, to ensure that no
 * exceptions can happen.
 */
SIZE_T virtual_uninterrupted_read_memory( const void *addr, void *buffer, SIZE_T size )
{
    struct file_view *view;
    sigset_t sigset;
    SIZE_T bytes_read = 0;

    if (!size) return 0;

    server_enter_uninterrupted_section( &csVirtual, &sigset );
    if ((view = VIRTUAL_FindView( addr, size )))
    {
        if (!(view->protect & VPROT_SYSTEM))
        {
            while (bytes_read < size && (VIRTUAL_GetUnixProt( get_page_vprot( addr )) & PROT_READ))
            {
                SIZE_T block_size = min( size - bytes_read, page_size - ((UINT_PTR)addr & page_mask) );
                memcpy( buffer, addr, block_size );

                addr   = (const void *)((const char *)addr + block_size);
                buffer = (void *)((char *)buffer + block_size);
                bytes_read += block_size;
            }
        }
    }
    server_leave_uninterrupted_section( &csVirtual, &sigset );
    return bytes_read;
}


/***********************************************************************
 *           virtual_uninterrupted_write_memory
 *
 * Similar to NtWriteVirtualMemory, but without wineserver calls. Moreover
 * permissions are checked before accessing each page, to ensure that no
 * exceptions can happen.
 */
NTSTATUS virtual_uninterrupted_write_memory( void *addr, const void *buffer, SIZE_T size )
{
    BOOL has_write_watch = FALSE;
    sigset_t sigset;
    NTSTATUS ret;

    if (!size) return STATUS_SUCCESS;

    server_enter_uninterrupted_section( &csVirtual, &sigset );
    if (!(ret = check_write_access( addr, size, &has_write_watch )))
    {
        memcpy( addr, buffer, size );
        if (has_write_watch) update_write_watches( addr, size, size );
    }
    server_leave_uninterrupted_section( &csVirtual, &sigset );
    return ret;
}


/***********************************************************************
 *           VIRTUAL_SetForceExec
 *
 * Whether to force exec prot on all views.
 */
void VIRTUAL_SetForceExec( BOOL enable )
{
    struct file_view *view;
    sigset_t sigset;

    server_enter_uninterrupted_section( &csVirtual, &sigset );
    if (!force_exec_prot != !enable)  /* change all existing views */
    {
        force_exec_prot = enable;

        WINE_RB_FOR_EACH_ENTRY( view, &views_tree, struct file_view, entry )
        {
            /* file mappings are always accessible */
            BYTE commit = is_view_valloc( view ) ? 0 : VPROT_COMMITTED;

            mprotect_range( view->base, view->size, commit, 0 );
        }
    }
    server_leave_uninterrupted_section( &csVirtual, &sigset );
}

struct free_range
{
    char *base;
    char *limit;
};

/* free reserved areas above the limit; callback for mmap_enum_reserved_areas */
static int CDECL free_reserved_memory( void *base, SIZE_T size, void *arg )
{
    struct free_range *range = arg;

    if ((char *)base >= range->limit) return 0;
    if ((char *)base + size <= range->base) return 0;
    if ((char *)base < range->base)
    {
        size -= range->base - (char *)base;
        base = range->base;
    }
    if ((char *)base + size > range->limit) size = range->limit - (char *)base;
    remove_reserved_area( base, size );
    return 1;  /* stop enumeration since the list has changed */
}

/***********************************************************************
 *           virtual_release_address_space
 *
 * Release some address space once we have loaded and initialized the app.
 */
void virtual_release_address_space(void)
{
    struct free_range range;
    sigset_t sigset;

    if (is_win64) return;

    server_enter_uninterrupted_section( &csVirtual, &sigset );

    range.base  = (char *)0x82000000;
    range.limit = user_space_limit;

    if (range.limit > range.base)
    {
        while (unix_funcs->mmap_enum_reserved_areas( free_reserved_memory, &range, 1 )) /* nothing */;
#ifdef __APPLE__
        /* On macOS, we still want to free some of low memory, for OpenGL resources */
        range.base  = (char *)0x40000000;
#else
        range.base  = NULL;
#endif
    }
    else
        range.base = (char *)0x20000000;

    if (range.base)
    {
        range.limit = (char *)0x7f000000;
        while (unix_funcs->mmap_enum_reserved_areas( free_reserved_memory, &range, 0 )) /* nothing */;
    }

    server_leave_uninterrupted_section( &csVirtual, &sigset );
}


/***********************************************************************
 *           virtual_set_large_address_space
 *
 * Enable use of a large address space when allowed by the application.
 */
void virtual_set_large_address_space(BOOL force_large_address)
{
    IMAGE_NT_HEADERS *nt = RtlImageNtHeader( NtCurrentTeb()->Peb->ImageBaseAddress );

    if (!(nt->FileHeader.Characteristics & IMAGE_FILE_LARGE_ADDRESS_AWARE) && !force_large_address) return;

    /* no large address space on win9x */
    if (NtCurrentTeb()->Peb->OSPlatformId != VER_PLATFORM_WIN32_NT) return;

    user_space_limit = working_set_limit = address_space_limit;
}


/***********************************************************************
 *             NtAllocateVirtualMemory   (NTDLL.@)
 *             ZwAllocateVirtualMemory   (NTDLL.@)
 */
NTSTATUS WINAPI NtAllocateVirtualMemory( HANDLE process, PVOID *ret, ULONG_PTR zero_bits,
                                         SIZE_T *size_ptr, ULONG type, ULONG protect )
{
    SIZE_T size = *size_ptr;
    NTSTATUS status = STATUS_SUCCESS;
    unsigned short zero_bits_64 = zero_bits_win_to_64( zero_bits );

    TRACE("%p %p %08lx %x %08x\n", process, *ret, size, type, protect );

    if (type & MEM_WRITE_WATCH)
    {
        static int disable = -1;

        if (disable == -1)
        {
            const char *env_var;

            if ((disable = (env_var = getenv("WINE_DISABLE_WRITE_WATCH")) && atoi(env_var)))
                FIXME("Disabling write watch support.\n");
        }

        if (disable)
            return STATUS_NOT_SUPPORTED;
    }

    if (!size) return STATUS_INVALID_PARAMETER;
    if (zero_bits > 21 && zero_bits < 32) return STATUS_INVALID_PARAMETER_3;
    if (!is_win64 && !is_wow64 && zero_bits >= 32) return STATUS_INVALID_PARAMETER_3;

    if (process != NtCurrentProcess())
    {
        apc_call_t call;
        apc_result_t result;

        memset( &call, 0, sizeof(call) );

        call.virtual_alloc.type         = APC_VIRTUAL_ALLOC;
        call.virtual_alloc.addr         = wine_server_client_ptr( *ret );
        call.virtual_alloc.size         = *size_ptr;
        call.virtual_alloc.zero_bits_64 = zero_bits_64;
        call.virtual_alloc.op_type      = type;
        call.virtual_alloc.prot         = protect;
        status = server_queue_process_apc( process, &call, &result );
        if (status != STATUS_SUCCESS) return status;

        if (result.virtual_alloc.status == STATUS_SUCCESS)
        {
            *ret      = wine_server_get_ptr( result.virtual_alloc.addr );
            *size_ptr = result.virtual_alloc.size;
        }
        return result.virtual_alloc.status;
    }

    return virtual_alloc( ret, zero_bits_64, size_ptr, type, protect );
}


/***********************************************************************
 *             virtual_alloc   (NTDLL.@)
 *
 * Same as NtAllocateVirtualMemory for the current process.
 */
NTSTATUS virtual_alloc( PVOID *ret, unsigned short zero_bits_64, SIZE_T *size_ptr,
                        ULONG type, ULONG protect )
{
    void *base;
    unsigned int vprot;
    SIZE_T size = *size_ptr;
    NTSTATUS status = STATUS_SUCCESS;
    BOOL is_dos_memory = FALSE;
    struct file_view *view;
    sigset_t sigset;

    /* Round parameters to a page boundary */

    if (is_beyond_limit( 0, size, working_set_limit )) return STATUS_WORKING_SET_LIMIT_RANGE;

    if (*ret)
    {
        if (type & MEM_RESERVE) /* Round down to 64k boundary */
            base = ROUND_ADDR( *ret, granularity_mask );
        else
            base = ROUND_ADDR( *ret, page_mask );
        size = (((UINT_PTR)*ret + size + page_mask) & ~page_mask) - (UINT_PTR)base;

        /* disallow low 64k, wrap-around and kernel space */
        if (((char *)base < (char *)0x10000) ||
            ((char *)base + size < (char *)base) ||
            is_beyond_limit( base, size, address_space_limit ))
        {
            /* address 1 is magic to mean DOS area */
            if (!base && *ret == (void *)1 && size == 0x110000) is_dos_memory = TRUE;
            else return STATUS_INVALID_PARAMETER;
        }
    }
    else
    {
        base = NULL;
        size = (size + page_mask) & ~page_mask;
    }

    /* Compute the alloc type flags */

    if (!(type & (MEM_COMMIT | MEM_RESERVE | MEM_RESET)) ||
        (type & ~(MEM_COMMIT | MEM_RESERVE | MEM_TOP_DOWN | MEM_WRITE_WATCH | MEM_RESET)))
    {
        WARN("called with wrong alloc type flags (%08x) !\n", type);
        return STATUS_INVALID_PARAMETER;
    }

    /* Reserve the memory */

    if (use_locks) server_enter_uninterrupted_section( &csVirtual, &sigset );

    if ((type & MEM_RESERVE) || !base)
    {
        if (!(status = get_vprot_flags( protect, &vprot, FALSE )))
        {
            if (type & MEM_COMMIT) vprot |= VPROT_COMMITTED;
            if (type & MEM_WRITE_WATCH) vprot |= VPROT_WRITEWATCH;
            if (protect & PAGE_NOCACHE) vprot |= SEC_NOCACHE;

            if (vprot & VPROT_WRITECOPY) status = STATUS_INVALID_PAGE_PROTECTION;
            else if (is_dos_memory) status = allocate_dos_memory( &view, vprot );
            else status = map_view( &view, base, size, type & MEM_TOP_DOWN, vprot, zero_bits_64 );

            if (status == STATUS_SUCCESS) base = view->base;
        }
    }
    else if (type & MEM_RESET)
    {
        if (!(view = VIRTUAL_FindView( base, size ))) status = STATUS_NOT_MAPPED_VIEW;
        else madvise( base, size, MADV_DONTNEED );
    }
    else  /* commit the pages */
    {
        if (!(view = VIRTUAL_FindView( base, size ))) status = STATUS_NOT_MAPPED_VIEW;
        else if (view->protect & SEC_FILE) status = STATUS_ALREADY_COMMITTED;
        else if (!(status = set_protection( view, base, size, protect )) && (view->protect & SEC_RESERVE))
        {
            SERVER_START_REQ( add_mapping_committed_range )
            {
                req->base   = wine_server_client_ptr( view->base );
                req->offset = (char *)base - (char *)view->base;
                req->size   = size;
                wine_server_call( req );
            }
            SERVER_END_REQ;
        }
    }

    if (!status) VIRTUAL_DEBUG_DUMP_VIEW( view );

    if (use_locks) server_leave_uninterrupted_section( &csVirtual, &sigset );

    if (status == STATUS_SUCCESS)
    {
        *ret = base;
        *size_ptr = size;
    }
    return status;
}


/***********************************************************************
 *             NtFreeVirtualMemory   (NTDLL.@)
 *             ZwFreeVirtualMemory   (NTDLL.@)
 */
NTSTATUS WINAPI NtFreeVirtualMemory( HANDLE process, PVOID *addr_ptr, SIZE_T *size_ptr, ULONG type )
{
    struct file_view *view;
    char *base;
    sigset_t sigset;
    NTSTATUS status = STATUS_SUCCESS;
    LPVOID addr = *addr_ptr;
    SIZE_T size = *size_ptr;

    TRACE("%p %p %08lx %x\n", process, addr, size, type );

    if (process != NtCurrentProcess())
    {
        apc_call_t call;
        apc_result_t result;

        memset( &call, 0, sizeof(call) );

        call.virtual_free.type      = APC_VIRTUAL_FREE;
        call.virtual_free.addr      = wine_server_client_ptr( addr );
        call.virtual_free.size      = size;
        call.virtual_free.op_type   = type;
        status = server_queue_process_apc( process, &call, &result );
        if (status != STATUS_SUCCESS) return status;

        if (result.virtual_free.status == STATUS_SUCCESS)
        {
            *addr_ptr = wine_server_get_ptr( result.virtual_free.addr );
            *size_ptr = result.virtual_free.size;
        }
        return result.virtual_free.status;
    }

    /* Fix the parameters */

    size = ROUND_SIZE( addr, size );
    base = ROUND_ADDR( addr, page_mask );

    /* avoid freeing the DOS area when a broken app passes a NULL pointer */
    if (!base) return STATUS_INVALID_PARAMETER;

    server_enter_uninterrupted_section( &csVirtual, &sigset );

    if (!(view = VIRTUAL_FindView( base, size )) || !is_view_valloc( view ))
    {
        status = STATUS_INVALID_PARAMETER;
    }
    else if (type == MEM_RELEASE)
    {
        /* Free the pages */

        if (size || (base != view->base)) status = STATUS_INVALID_PARAMETER;
        else if (view->base == (void *)((ULONG_PTR)ntdll_get_thread_data()->pthread_stack & ~1))
        {
            ULONG_PTR stack = (ULONG_PTR)ntdll_get_thread_data()->pthread_stack;
            if (stack & 1) status = STATUS_INVALID_PARAMETER;
            else
            {
                WARN( "Application tried to deallocate current pthread stack %p, deferring\n", view->base);
                ntdll_get_thread_data()->pthread_stack = (void *)(stack | 1);
            }
        }
        else
        {
            delete_view( view );
            *addr_ptr = base;
            *size_ptr = size;
        }
    }
    else if (type == MEM_DECOMMIT)
    {
        status = decommit_pages( view, base - (char *)view->base, size );
        if (status == STATUS_SUCCESS)
        {
            *addr_ptr = base;
            *size_ptr = size;
        }
    }
    else
    {
        WARN("called with wrong free type flags (%08x) !\n", type);
        status = STATUS_INVALID_PARAMETER;
    }

    server_leave_uninterrupted_section( &csVirtual, &sigset );
    return status;
}


/***********************************************************************
 *             NtProtectVirtualMemory   (NTDLL.@)
 *             ZwProtectVirtualMemory   (NTDLL.@)
 */
NTSTATUS WINAPI DECLSPEC_HOTPATCH NtProtectVirtualMemory( HANDLE process, PVOID *addr_ptr, SIZE_T *size_ptr,
                                                          ULONG new_prot, ULONG *old_prot )
{
    struct file_view *view;
    sigset_t sigset;
    NTSTATUS status = STATUS_SUCCESS;
    char *base;
    BYTE vprot;
    SIZE_T size = *size_ptr;
    LPVOID addr = *addr_ptr;
    DWORD old;

    TRACE("%p %p %08lx %08x\n", process, addr, size, new_prot );

    if (!old_prot)
        return STATUS_ACCESS_VIOLATION;

    if (process != NtCurrentProcess())
    {
        apc_call_t call;
        apc_result_t result;

        memset( &call, 0, sizeof(call) );

        call.virtual_protect.type = APC_VIRTUAL_PROTECT;
        call.virtual_protect.addr = wine_server_client_ptr( addr );
        call.virtual_protect.size = size;
        call.virtual_protect.prot = new_prot;
        status = server_queue_process_apc( process, &call, &result );
        if (status != STATUS_SUCCESS) return status;

        if (result.virtual_protect.status == STATUS_SUCCESS)
        {
            *addr_ptr = wine_server_get_ptr( result.virtual_protect.addr );
            *size_ptr = result.virtual_protect.size;
            *old_prot = result.virtual_protect.prot;
        }
        return result.virtual_protect.status;
    }

    /* Fix the parameters */

    size = ROUND_SIZE( addr, size );
    base = ROUND_ADDR( addr, page_mask );

    server_enter_uninterrupted_section( &csVirtual, &sigset );

    if ((view = VIRTUAL_FindView( base, size )))
    {
        /* Make sure all the pages are committed */
        if (get_committed_size( view, base, &vprot ) >= size && (vprot & VPROT_COMMITTED))
        {
            old = VIRTUAL_GetWin32Prot( vprot, view->protect );
            status = set_protection( view, base, size, new_prot );
        }
        else status = STATUS_NOT_COMMITTED;
    }
    else status = STATUS_INVALID_PARAMETER;

    if (!status) VIRTUAL_DEBUG_DUMP_VIEW( view );

    server_leave_uninterrupted_section( &csVirtual, &sigset );

    if (status == STATUS_SUCCESS)
    {
        *addr_ptr = base;
        *size_ptr = size;
        *old_prot = old;
    }
    return status;
}


/* retrieve state for a free memory area; callback for mmap_enum_reserved_areas */
static int CDECL get_free_mem_state_callback( void *start, SIZE_T size, void *arg )
{
    MEMORY_BASIC_INFORMATION *info = arg;
    void *end = (char *)start + size;

    if ((char *)info->BaseAddress + info->RegionSize <= (char *)start) return 0;

    if (info->BaseAddress >= end)
    {
        if (info->AllocationBase < end) info->AllocationBase = end;
        return 0;
    }

    if (info->BaseAddress >= start || start <= address_space_start)
    {
        /* it's a real free area */
        info->State             = MEM_FREE;
        info->Protect           = PAGE_NOACCESS;
        info->AllocationBase    = 0;
        info->AllocationProtect = 0;
        info->Type              = 0;
        if ((char *)info->BaseAddress + info->RegionSize > (char *)end)
            info->RegionSize = (char *)end - (char *)info->BaseAddress;
    }
    else /* outside of the reserved area, pretend it's allocated */
    {
        info->RegionSize        = (char *)start - (char *)info->BaseAddress;
        info->State             = MEM_RESERVE;
        info->Protect           = PAGE_NOACCESS;
        info->AllocationProtect = PAGE_NOACCESS;
        info->Type              = MEM_PRIVATE;
    }
    return 1;
}

/* get basic information about a memory block */
static NTSTATUS get_basic_memory_info( HANDLE process, LPCVOID addr,
                                       MEMORY_BASIC_INFORMATION *info,
                                       SIZE_T len, SIZE_T *res_len )
{
    struct file_view *view;
    char *base, *alloc_base = 0, *alloc_end = working_set_limit;
    struct wine_rb_entry *ptr;
    sigset_t sigset;

    if (len < sizeof(MEMORY_BASIC_INFORMATION))
        return STATUS_INFO_LENGTH_MISMATCH;

    if (process != NtCurrentProcess())
    {
        NTSTATUS status;
        apc_call_t call;
        apc_result_t result;

        memset( &call, 0, sizeof(call) );

        call.virtual_query.type = APC_VIRTUAL_QUERY;
        call.virtual_query.addr = wine_server_client_ptr( addr );
        status = server_queue_process_apc( process, &call, &result );
        if (status != STATUS_SUCCESS) return status;

        if (result.virtual_query.status == STATUS_SUCCESS)
        {
            info->BaseAddress       = wine_server_get_ptr( result.virtual_query.base );
            info->AllocationBase    = wine_server_get_ptr( result.virtual_query.alloc_base );
            info->RegionSize        = result.virtual_query.size;
            info->Protect           = result.virtual_query.prot;
            info->AllocationProtect = result.virtual_query.alloc_prot;
            info->State             = (DWORD)result.virtual_query.state << 12;
            info->Type              = (DWORD)result.virtual_query.alloc_type << 16;
            if (info->RegionSize != result.virtual_query.size)  /* truncated */
                return STATUS_INVALID_PARAMETER;  /* FIXME */
            if (res_len) *res_len = sizeof(*info);
        }
        return result.virtual_query.status;
    }

    base = ROUND_ADDR( addr, page_mask );

    if (is_beyond_limit( base, 1, working_set_limit )) return STATUS_INVALID_PARAMETER;

    /* Find the view containing the address */

    server_enter_uninterrupted_section( &csVirtual, &sigset );
    ptr = views_tree.root;
    while (ptr)
    {
        view = WINE_RB_ENTRY_VALUE( ptr, struct file_view, entry );
        if ((char *)view->base > base)
        {
            alloc_end = view->base;
            ptr = ptr->left;
        }
        else if ((char *)view->base + view->size <= base)
        {
            alloc_base = (char *)view->base + view->size;
            ptr = ptr->right;
        }
        else
        {
            alloc_base = view->base;
            alloc_end = (char *)view->base + view->size;
            break;
        }
    }

    /* Fill the info structure */

    info->AllocationBase = alloc_base;
    info->BaseAddress    = base;
    info->RegionSize     = alloc_end - base;

    if (!ptr)
    {
        if (!unix_funcs->mmap_enum_reserved_areas( get_free_mem_state_callback, info, 0 ))
        {
            /* not in a reserved area at all, pretend it's allocated */
#ifdef __i386__
            if (base >= (char *)address_space_start)
            {
                info->State             = MEM_RESERVE;
                info->Protect           = PAGE_NOACCESS;
                info->AllocationProtect = PAGE_NOACCESS;
                info->Type              = MEM_PRIVATE;
            }
            else
#endif
            {
                info->State             = MEM_FREE;
                info->Protect           = PAGE_NOACCESS;
                info->AllocationBase    = 0;
                info->AllocationProtect = 0;
                info->Type              = 0;
            }
        }
    }
    else
    {
        BYTE vprot;
        char *ptr;
        SIZE_T range_size = get_committed_size( view, base, &vprot );

        info->State = (vprot & VPROT_COMMITTED) ? MEM_COMMIT : MEM_RESERVE;
        info->Protect = (vprot & VPROT_COMMITTED) ? VIRTUAL_GetWin32Prot( vprot, view->protect ) : 0;
        info->AllocationProtect = VIRTUAL_GetWin32Prot( view->protect, view->protect );
        if (view->protect & SEC_IMAGE) info->Type = MEM_IMAGE;
        else if (view->protect & (SEC_FILE | SEC_RESERVE | SEC_COMMIT)) info->Type = MEM_MAPPED;
        else info->Type = MEM_PRIVATE;
        for (ptr = base; ptr < base + range_size; ptr += page_size)
            if ((get_page_vprot( ptr ) ^ vprot) & ~(VPROT_WRITEWATCH|VPROT_WRITTEN)) break;
        info->RegionSize = ptr - base;
    }
    server_leave_uninterrupted_section( &csVirtual, &sigset );

    if (res_len) *res_len = sizeof(*info);
    return STATUS_SUCCESS;
}

static NTSTATUS get_working_set_ex( HANDLE process, LPCVOID addr,
                                    MEMORY_WORKING_SET_EX_INFORMATION *info,
                                    SIZE_T len, SIZE_T *res_len )
{
    FILE *f;
    MEMORY_WORKING_SET_EX_INFORMATION *p;
    sigset_t sigset;

    if (process != NtCurrentProcess())
    {
        FIXME( "(process=%p,addr=%p) Unimplemented information class: MemoryWorkingSetExInformation\n", process, addr );
        return STATUS_INVALID_INFO_CLASS;
    }

    f = fopen( "/proc/self/pagemap", "rb" );
    if (!f)
    {
        static int once;
        if (!once++) WARN( "unable to open /proc/self/pagemap\n" );
    }

    server_enter_uninterrupted_section( &csVirtual, &sigset );
    for (p = info; (UINT_PTR)(p + 1) <= (UINT_PTR)info + len; p++)
    {
        BYTE vprot;
        UINT64 pagemap;
        struct file_view *view;

        memset( &p->VirtualAttributes, 0, sizeof(p->VirtualAttributes) );

        /* If we don't have pagemap information, default to invalid. */
        if (!f || fseek( f, ((UINT_PTR)p->VirtualAddress >> 12) * sizeof(pagemap), SEEK_SET ) == -1 ||
                fread( &pagemap, sizeof(pagemap), 1, f ) != 1)
        {
            pagemap = 0;
        }

        if ((view = VIRTUAL_FindView( p->VirtualAddress, 0 )) &&
                get_committed_size( view, p->VirtualAddress, &vprot ) &&
                (vprot & VPROT_COMMITTED))
        {
            p->VirtualAttributes.Valid = !(vprot & VPROT_GUARD) && (vprot & 0x0f) && (pagemap >> 63);
            p->VirtualAttributes.Shared = (!is_view_valloc( view ) && ((pagemap >> 61) & 1)) || ((view->protect & VPROT_WRITECOPY) && !(vprot & VPROT_WRITTEN));
            if (p->VirtualAttributes.Shared && p->VirtualAttributes.Valid)
                p->VirtualAttributes.ShareCount = 1; /* FIXME */
            if (p->VirtualAttributes.Valid)
                p->VirtualAttributes.Win32Protection = VIRTUAL_GetWin32Prot( vprot, view->protect );
        }
    }
    server_leave_uninterrupted_section( &csVirtual, &sigset );

    if (f)
        fclose( f );
    if (res_len)
        *res_len = (UINT_PTR)p - (UINT_PTR)info;
    return STATUS_SUCCESS;
}

/* get file name for mapped section */
static NTSTATUS get_section_name( HANDLE process, LPCVOID addr,
                                  MEMORY_SECTION_NAME *info,
                                  SIZE_T len, SIZE_T *res_len )
{
    static const WCHAR dosprefixW[] = {'\\','?','?','\\'};
    WCHAR symlinkW[MAX_DIR_ENTRY_LEN] = {0};
    UNICODE_STRING nt_name;
    ANSI_STRING unix_name;
    data_size_t size = 1024;
    WCHAR *ptr, *name = NULL;
    NTSTATUS status;
    HANDLE mapping;
    size_t offset = 0;

    if (!addr || !info || !res_len) return STATUS_INVALID_PARAMETER;

    SERVER_START_REQ( get_mapping_file )
    {
        req->process = wine_server_obj_handle( process );
        req->addr = wine_server_client_ptr( addr );
        status = wine_server_call( req );
        mapping = wine_server_ptr_handle( reply->handle );
    }
    SERVER_END_REQ;

    if (!status && mapping)
    {
        status = server_get_unix_name( mapping, &unix_name );
        close_handle( mapping );
        if (!status)
        {
            status = wine_unix_to_nt_file_name( &unix_name, &nt_name );
            RtlFreeAnsiString( &unix_name );
        }
        if (!status) goto found;
        if (status == STATUS_OBJECT_TYPE_MISMATCH) status = STATUS_FILE_INVALID;
        return status;
    }

    for (;;)
    {
        if (!(name = RtlAllocateHeap( GetProcessHeap(), 0, (size + 1) * sizeof(WCHAR) )))
            return STATUS_NO_MEMORY;

        SERVER_START_REQ( get_dll_info )
        {
            req->handle = wine_server_obj_handle( process );
            req->base_address = (ULONG_PTR)addr;
            wine_server_set_reply( req, name, size * sizeof(WCHAR) );
            status = wine_server_call( req );
            size = reply->filename_len / sizeof(WCHAR);
        }
        SERVER_END_REQ;

        if (!status)
        {
            name[size] = 0;
            break;
        }
        RtlFreeHeap( GetProcessHeap(), 0, name );
        if (status == STATUS_DLL_NOT_FOUND) return STATUS_INVALID_ADDRESS;
        if (status != STATUS_BUFFER_TOO_SMALL) return status;
    }

    if (!RtlDosPathNameToNtPathName_U( name, &nt_name, NULL, NULL ))
    {
        RtlFreeHeap( GetProcessHeap(), 0, name );
        return STATUS_INVALID_PARAMETER;
    }

found:
    if (nt_name.Length >= sizeof(dosprefixW) &&
        !memcmp( nt_name.Buffer, dosprefixW, sizeof(dosprefixW) ))
    {
        UNICODE_STRING device_name = nt_name;
        offset = sizeof(dosprefixW) / sizeof(WCHAR);
        while (offset * sizeof(WCHAR) < nt_name.Length && nt_name.Buffer[ offset ] != '\\') offset++;
        device_name.Length = offset * sizeof(WCHAR);
        if (read_nt_symlink( NULL, &device_name, symlinkW, MAX_DIR_ENTRY_LEN ))
        {
            symlinkW[0] = 0;
            offset = 0;
        }
    }

    *res_len = sizeof(MEMORY_SECTION_NAME) + wcslen(symlinkW) * sizeof(WCHAR) +
               nt_name.Length - offset * sizeof(WCHAR) + sizeof(WCHAR);
    if (len >= *res_len)
    {
        info->SectionFileName.Length = wcslen(symlinkW) * sizeof(WCHAR) +
                                       nt_name.Length - offset * sizeof(WCHAR);
        info->SectionFileName.MaximumLength = info->SectionFileName.Length + sizeof(WCHAR);
        info->SectionFileName.Buffer = (WCHAR *)(info + 1);

        ptr = (WCHAR *)(info + 1);
        wcscpy( ptr, symlinkW );
        ptr += wcslen(symlinkW);
        memcpy( ptr, nt_name.Buffer + offset, nt_name.Length - offset * sizeof(WCHAR) );
        ptr[ nt_name.Length / sizeof(WCHAR) - offset ] = 0;
    }
    else
        status = (len < sizeof(MEMORY_SECTION_NAME)) ? STATUS_INFO_LENGTH_MISMATCH : STATUS_BUFFER_OVERFLOW;

    RtlFreeHeap( GetProcessHeap(), 0, name );
    RtlFreeUnicodeString( &nt_name );
    return status;
}


#define UNIMPLEMENTED_INFO_CLASS(c) \
    case c: \
        FIXME("(process=%p,addr=%p) Unimplemented information class: " #c "\n", process, addr); \
        return STATUS_INVALID_INFO_CLASS

/***********************************************************************
 *             NtQueryVirtualMemory   (NTDLL.@)
 *             ZwQueryVirtualMemory   (NTDLL.@)
 */
NTSTATUS WINAPI NtQueryVirtualMemory( HANDLE process, LPCVOID addr,
                                      MEMORY_INFORMATION_CLASS info_class,
                                      PVOID buffer, SIZE_T len, SIZE_T *res_len )
{
    TRACE("(%p, %p, info_class=%d, %p, %ld, %p)\n",
          process, addr, info_class, buffer, len, res_len);

    switch(info_class)
    {
        case MemoryBasicInformation:
            return get_basic_memory_info( process, addr, buffer, len, res_len );

        case MemoryWorkingSetExInformation:
            return get_working_set_ex( process, addr, buffer, len, res_len );

        case MemorySectionName:
            return get_section_name( process, addr, buffer, len, res_len );

        UNIMPLEMENTED_INFO_CLASS(MemoryWorkingSetList);
        UNIMPLEMENTED_INFO_CLASS(MemoryBasicVlmInformation);

        default:
            FIXME("(%p,%p,info_class=%d,%p,%ld,%p) Unknown information class\n",
                  process, addr, info_class, buffer, len, res_len);
            return STATUS_INVALID_INFO_CLASS;
    }
}


/***********************************************************************
 *             NtLockVirtualMemory   (NTDLL.@)
 *             ZwLockVirtualMemory   (NTDLL.@)
 */
NTSTATUS WINAPI NtLockVirtualMemory( HANDLE process, PVOID *addr, SIZE_T *size, ULONG unknown )
{
    NTSTATUS status = STATUS_SUCCESS;

    if (process != NtCurrentProcess())
    {
        apc_call_t call;
        apc_result_t result;

        memset( &call, 0, sizeof(call) );

        call.virtual_lock.type = APC_VIRTUAL_LOCK;
        call.virtual_lock.addr = wine_server_client_ptr( *addr );
        call.virtual_lock.size = *size;
        status = server_queue_process_apc( process, &call, &result );
        if (status != STATUS_SUCCESS) return status;

        if (result.virtual_lock.status == STATUS_SUCCESS)
        {
            *addr = wine_server_get_ptr( result.virtual_lock.addr );
            *size = result.virtual_lock.size;
        }
        return result.virtual_lock.status;
    }

    *size = ROUND_SIZE( *addr, *size );
    *addr = ROUND_ADDR( *addr, page_mask );

    if (mlock( *addr, *size )) status = STATUS_ACCESS_DENIED;
    return status;
}


/***********************************************************************
 *             NtUnlockVirtualMemory   (NTDLL.@)
 *             ZwUnlockVirtualMemory   (NTDLL.@)
 */
NTSTATUS WINAPI NtUnlockVirtualMemory( HANDLE process, PVOID *addr, SIZE_T *size, ULONG unknown )
{
    NTSTATUS status = STATUS_SUCCESS;

    if (process != NtCurrentProcess())
    {
        apc_call_t call;
        apc_result_t result;

        memset( &call, 0, sizeof(call) );

        call.virtual_unlock.type = APC_VIRTUAL_UNLOCK;
        call.virtual_unlock.addr = wine_server_client_ptr( *addr );
        call.virtual_unlock.size = *size;
        status = server_queue_process_apc( process, &call, &result );
        if (status != STATUS_SUCCESS) return status;

        if (result.virtual_unlock.status == STATUS_SUCCESS)
        {
            *addr = wine_server_get_ptr( result.virtual_unlock.addr );
            *size = result.virtual_unlock.size;
        }
        return result.virtual_unlock.status;
    }

    *size = ROUND_SIZE( *addr, *size );
    *addr = ROUND_ADDR( *addr, page_mask );

    if (munlock( *addr, *size )) status = STATUS_ACCESS_DENIED;
    return status;
}


/***********************************************************************
 *             NtCreateSection   (NTDLL.@)
 *             ZwCreateSection   (NTDLL.@)
 */
NTSTATUS WINAPI NtCreateSection( HANDLE *handle, ACCESS_MASK access, const OBJECT_ATTRIBUTES *attr,
                                 const LARGE_INTEGER *size, ULONG protect,
                                 ULONG sec_flags, HANDLE file )
{
    NTSTATUS ret;
    unsigned int vprot, file_access = 0;
    data_size_t len;
    struct object_attributes *objattr;

    if ((ret = get_vprot_flags( protect, &vprot, sec_flags & SEC_IMAGE ))) return ret;
    if ((ret = alloc_object_attributes( attr, &objattr, &len ))) return ret;

    if (vprot & VPROT_READ)  file_access |= FILE_READ_DATA;
    if (vprot & VPROT_WRITE) file_access |= FILE_WRITE_DATA;

    SERVER_START_REQ( create_mapping )
    {
        req->access      = access;
        req->flags       = sec_flags;
        req->file_handle = wine_server_obj_handle( file );
        req->file_access = file_access;
        req->size        = size ? size->QuadPart : 0;
        wine_server_add_data( req, objattr, len );
        ret = wine_server_call( req );
        *handle = wine_server_ptr_handle( reply->handle );
    }
    SERVER_END_REQ;

    RtlFreeHeap( GetProcessHeap(), 0, objattr );
    return ret;
}


/***********************************************************************
 *             NtOpenSection   (NTDLL.@)
 *             ZwOpenSection   (NTDLL.@)
 */
NTSTATUS WINAPI NtOpenSection( HANDLE *handle, ACCESS_MASK access, const OBJECT_ATTRIBUTES *attr )
{
    NTSTATUS ret;

    if ((ret = validate_open_object_attributes( attr ))) return ret;

    SERVER_START_REQ( open_mapping )
    {
        req->access     = access;
        req->attributes = attr->Attributes;
        req->rootdir    = wine_server_obj_handle( attr->RootDirectory );
        if (attr->ObjectName)
            wine_server_add_data( req, attr->ObjectName->Buffer, attr->ObjectName->Length );
        ret = wine_server_call( req );
        *handle = wine_server_ptr_handle( reply->handle );
    }
    SERVER_END_REQ;
    return ret;
}


/***********************************************************************
 *             NtMapViewOfSection   (NTDLL.@)
 *             ZwMapViewOfSection   (NTDLL.@)
 */
NTSTATUS WINAPI NtMapViewOfSection( HANDLE handle, HANDLE process, PVOID *addr_ptr, ULONG_PTR zero_bits,
                                    SIZE_T commit_size, const LARGE_INTEGER *offset_ptr, SIZE_T *size_ptr,
                                    SECTION_INHERIT inherit, ULONG alloc_type, ULONG protect )
{
    NTSTATUS res;
    SIZE_T mask = granularity_mask;
    pe_image_info_t image_info;
    LARGE_INTEGER offset;
    unsigned short zero_bits_64 = zero_bits_win_to_64( zero_bits );

    offset.QuadPart = offset_ptr ? offset_ptr->QuadPart : 0;

    TRACE("handle=%p process=%p addr=%p off=%x%08x size=%lx access=%x\n",
          handle, process, *addr_ptr, offset.u.HighPart, offset.u.LowPart, *size_ptr, protect );

    /* Check parameters */
    if (zero_bits > 21 && zero_bits < 32)
        return STATUS_INVALID_PARAMETER_4;
    if (!is_win64 && !is_wow64 && zero_bits >= 32)
        return STATUS_INVALID_PARAMETER_4;

    /* If both addr_ptr and zero_bits are passed, they have match */
    if (*addr_ptr && zero_bits && zero_bits < 32 &&
        (((UINT_PTR)*addr_ptr) >> (32 - zero_bits)))
        return STATUS_INVALID_PARAMETER_4;
    if (*addr_ptr && zero_bits >= 32 &&
        (((UINT_PTR)*addr_ptr) & ~zero_bits))
        return STATUS_INVALID_PARAMETER_4;

#ifndef _WIN64
    if (!is_wow64 && (alloc_type & AT_ROUND_TO_PAGE))
    {
        *addr_ptr = ROUND_ADDR( *addr_ptr, page_mask );
        mask = page_mask;
    }
#endif

    if ((offset.u.LowPart & mask) || (*addr_ptr && ((UINT_PTR)*addr_ptr & mask)))
        return STATUS_MAPPED_ALIGNMENT;

    if (process != NtCurrentProcess())
    {
        apc_call_t call;
        apc_result_t result;

        memset( &call, 0, sizeof(call) );

        call.map_view.type         = APC_MAP_VIEW;
        call.map_view.handle       = wine_server_obj_handle( handle );
        call.map_view.addr         = wine_server_client_ptr( *addr_ptr );
        call.map_view.size         = *size_ptr;
        call.map_view.offset       = offset.QuadPart;
        call.map_view.zero_bits_64 = zero_bits_64;
        call.map_view.alloc_type   = alloc_type;
        call.map_view.prot         = protect;
        res = server_queue_process_apc( process, &call, &result );
        if (res != STATUS_SUCCESS) return res;

        if ((NTSTATUS)result.map_view.status >= 0)
        {
            *addr_ptr = wine_server_get_ptr( result.map_view.addr );
            *size_ptr = result.map_view.size;
        }
        return result.map_view.status;
    }

    return virtual_map_section( handle, addr_ptr, zero_bits_64, commit_size,
                                offset_ptr, size_ptr, alloc_type, protect,
                                &image_info );
}


/***********************************************************************
 *           virtual_map_shared_memory
 */
NTSTATUS virtual_map_shared_memory( int fd, PVOID *addr_ptr, ULONG zero_bits,
                                    SIZE_T *size_ptr, ULONG protect )
{
    SIZE_T size;
    struct file_view *view;
    unsigned int vprot;
    sigset_t sigset;
    NTSTATUS res;
    int prot;

    size = ROUND_SIZE( 0, *size_ptr );
    if (size < *size_ptr)
        return STATUS_INVALID_PARAMETER;

    server_enter_uninterrupted_section( &csVirtual, &sigset );

    get_vprot_flags( protect, &vprot, FALSE );
    vprot |= VPROT_COMMITTED;
    res = map_view( &view, *addr_ptr, size, FALSE, vprot, 0 );
    if (!res)
    {
        /* Map the shared memory */

        prot = VIRTUAL_GetUnixProt( vprot );
        if (force_exec_prot && (vprot & VPROT_READ))
        {
            TRACE( "forcing exec permission on mapping %p-%p\n",
                   (char *)view->base, (char *)view->base + size - 1 );
            prot |= PROT_EXEC;
        }

        if (mmap( view->base, size, prot, MAP_FIXED | MAP_SHARED, fd, 0 ) != (void *)-1)
        {
            *addr_ptr = view->base;
            *size_ptr = size;
        }
        else
        {
            ERR( "virtual_map_shared_memory %p %lx failed\n", view->base, size );
            delete_view( view );
        }
    }

    server_leave_uninterrupted_section( &csVirtual, &sigset );
    return res;
}


/***********************************************************************
 *             NtUnmapViewOfSection   (NTDLL.@)
 *             ZwUnmapViewOfSection   (NTDLL.@)
 */
NTSTATUS WINAPI NtUnmapViewOfSection( HANDLE process, PVOID addr )
{
    struct file_view *view;
    NTSTATUS status = STATUS_NOT_MAPPED_VIEW;
    sigset_t sigset;

    if (process != NtCurrentProcess())
    {
        apc_call_t call;
        apc_result_t result;

        memset( &call, 0, sizeof(call) );

        call.unmap_view.type = APC_UNMAP_VIEW;
        call.unmap_view.addr = wine_server_client_ptr( addr );
        status = server_queue_process_apc( process, &call, &result );
        if (status == STATUS_SUCCESS) status = result.unmap_view.status;
        return status;
    }

    server_enter_uninterrupted_section( &csVirtual, &sigset );
    if ((view = VIRTUAL_FindView( addr, 0 )) && !is_view_valloc( view ))
    {
        if (!(view->protect & VPROT_SYSTEM))
        {
            SERVER_START_REQ( unmap_view )
            {
                req->base = wine_server_client_ptr( view->base );
                status = wine_server_call( req );
            }
            SERVER_END_REQ;
            if (!status) delete_view( view );
            else FIXME( "failed to unmap %p %x\n", view->base, status );
        }
        else
        {
            delete_view( view );
            status = STATUS_SUCCESS;
        }
    }
    server_leave_uninterrupted_section( &csVirtual, &sigset );
    return status;
}


/******************************************************************************
 *             virtual_fill_image_information
 *
 * Helper for NtQuerySection.
 */
void virtual_fill_image_information( const pe_image_info_t *pe_info, SECTION_IMAGE_INFORMATION *info )
{
    info->TransferAddress      = wine_server_get_ptr( pe_info->entry_point );
    info->ZeroBits             = pe_info->zerobits;
    info->MaximumStackSize     = pe_info->stack_size;
    info->CommittedStackSize   = pe_info->stack_commit;
    info->SubSystemType        = pe_info->subsystem;
    info->SubsystemVersionLow  = pe_info->subsystem_low;
    info->SubsystemVersionHigh = pe_info->subsystem_high;
    info->GpValue              = pe_info->gp;
    info->ImageCharacteristics = pe_info->image_charact;
    info->DllCharacteristics   = pe_info->dll_charact;
    info->Machine              = pe_info->machine;
    info->ImageContainsCode    = pe_info->contains_code;
    info->u.ImageFlags         = pe_info->image_flags & ~(IMAGE_FLAGS_WineBuiltin|IMAGE_FLAGS_WineFakeDll);
    info->LoaderFlags          = pe_info->loader_flags;
    info->ImageFileSize        = pe_info->file_size;
    info->CheckSum             = pe_info->checksum;
#ifndef _WIN64 /* don't return 64-bit values to 32-bit processes */
    if (pe_info->machine == IMAGE_FILE_MACHINE_AMD64 || pe_info->machine == IMAGE_FILE_MACHINE_ARM64)
    {
        info->TransferAddress = (void *)0x81231234;  /* sic */
        info->MaximumStackSize = 0x100000;
        info->CommittedStackSize = 0x10000;
    }
#endif
}

/******************************************************************************
 *             NtQuerySection   (NTDLL.@)
 *             ZwQuerySection   (NTDLL.@)
 */
NTSTATUS WINAPI NtQuerySection( HANDLE handle, SECTION_INFORMATION_CLASS class, void *ptr,
                                SIZE_T size, SIZE_T *ret_size )
{
    NTSTATUS status;
    pe_image_info_t image_info;

    switch (class)
    {
    case SectionBasicInformation:
        if (size < sizeof(SECTION_BASIC_INFORMATION)) return STATUS_INFO_LENGTH_MISMATCH;
        break;
    case SectionImageInformation:
        if (size < sizeof(SECTION_IMAGE_INFORMATION)) return STATUS_INFO_LENGTH_MISMATCH;
        break;
    default:
	FIXME( "class %u not implemented\n", class );
	return STATUS_NOT_IMPLEMENTED;
    }
    if (!ptr) return STATUS_ACCESS_VIOLATION;

    SERVER_START_REQ( get_mapping_info )
    {
        req->handle = wine_server_obj_handle( handle );
        req->access = SECTION_QUERY;
        wine_server_set_reply( req, &image_info, sizeof(image_info) );
        if (!(status = wine_server_call( req )))
        {
            if (class == SectionBasicInformation)
            {
                SECTION_BASIC_INFORMATION *info = ptr;
                info->Attributes    = reply->flags;
                info->BaseAddress   = NULL;
                info->Size.QuadPart = reply->size;
                if (ret_size) *ret_size = sizeof(*info);
            }
            else if (reply->flags & SEC_IMAGE)
            {
                SECTION_IMAGE_INFORMATION *info = ptr;
                virtual_fill_image_information( &image_info, info );
                if (ret_size) *ret_size = sizeof(*info);
            }
            else status = STATUS_SECTION_NOT_IMAGE;
        }
    }
    SERVER_END_REQ;

    return status;
}


/***********************************************************************
 *             NtFlushVirtualMemory   (NTDLL.@)
 *             ZwFlushVirtualMemory   (NTDLL.@)
 */
NTSTATUS WINAPI NtFlushVirtualMemory( HANDLE process, LPCVOID *addr_ptr,
                                      SIZE_T *size_ptr, ULONG unknown )
{
    struct file_view *view;
    NTSTATUS status = STATUS_SUCCESS;
    sigset_t sigset;
    void *addr = ROUND_ADDR( *addr_ptr, page_mask );

    if (process != NtCurrentProcess())
    {
        apc_call_t call;
        apc_result_t result;

        memset( &call, 0, sizeof(call) );

        call.virtual_flush.type = APC_VIRTUAL_FLUSH;
        call.virtual_flush.addr = wine_server_client_ptr( addr );
        call.virtual_flush.size = *size_ptr;
        status = server_queue_process_apc( process, &call, &result );
        if (status != STATUS_SUCCESS) return status;

        if (result.virtual_flush.status == STATUS_SUCCESS)
        {
            *addr_ptr = wine_server_get_ptr( result.virtual_flush.addr );
            *size_ptr = result.virtual_flush.size;
        }
        return result.virtual_flush.status;
    }

    server_enter_uninterrupted_section( &csVirtual, &sigset );
    if (!(view = VIRTUAL_FindView( addr, *size_ptr ))) status = STATUS_INVALID_PARAMETER;
    else
    {
        if (!*size_ptr) *size_ptr = view->size;
        *addr_ptr = addr;
#ifdef MS_ASYNC
        if (msync( addr, *size_ptr, MS_ASYNC )) status = STATUS_NOT_MAPPED_DATA;
#endif
    }
    server_leave_uninterrupted_section( &csVirtual, &sigset );
    return status;
}


/***********************************************************************
 *             NtGetWriteWatch   (NTDLL.@)
 *             ZwGetWriteWatch   (NTDLL.@)
 */
NTSTATUS WINAPI NtGetWriteWatch( HANDLE process, ULONG flags, PVOID base, SIZE_T size, PVOID *addresses,
                                 ULONG_PTR *count, ULONG *granularity )
{
    NTSTATUS status = STATUS_SUCCESS;
    sigset_t sigset;

    size = ROUND_SIZE( base, size );
    base = ROUND_ADDR( base, page_mask );

    if (!count || !granularity) return STATUS_ACCESS_VIOLATION;
    if (!*count || !size) return STATUS_INVALID_PARAMETER;
    if (flags & ~WRITE_WATCH_FLAG_RESET) return STATUS_INVALID_PARAMETER;

    if (!addresses) return STATUS_ACCESS_VIOLATION;

    TRACE( "%p %x %p-%p %p %lu\n", process, flags, base, (char *)base + size,
           addresses, *count );

    server_enter_uninterrupted_section( &csVirtual, &sigset );

    if (is_write_watch_range( base, size ))
    {
        ULONG_PTR pos = 0;
        char *addr = base;
        char *end = addr + size;

        while (pos < *count && addr < end)
        {
            if (!(get_page_vprot( addr ) & VPROT_WRITEWATCH)) addresses[pos++] = addr;
            addr += page_size;
        }
        if (flags & WRITE_WATCH_FLAG_RESET) reset_write_watches( base, addr - (char *)base );
        *count = pos;
        *granularity = page_size;
    }
    else status = STATUS_INVALID_PARAMETER;

    server_leave_uninterrupted_section( &csVirtual, &sigset );
    return status;
}


/***********************************************************************
 *             NtResetWriteWatch   (NTDLL.@)
 *             ZwResetWriteWatch   (NTDLL.@)
 */
NTSTATUS WINAPI NtResetWriteWatch( HANDLE process, PVOID base, SIZE_T size )
{
    NTSTATUS status = STATUS_SUCCESS;
    sigset_t sigset;

    size = ROUND_SIZE( base, size );
    base = ROUND_ADDR( base, page_mask );

    TRACE( "%p %p-%p\n", process, base, (char *)base + size );

    if (!size) return STATUS_INVALID_PARAMETER;

    server_enter_uninterrupted_section( &csVirtual, &sigset );

    if (is_write_watch_range( base, size ))
        reset_write_watches( base, size );
    else
        status = STATUS_INVALID_PARAMETER;

    server_leave_uninterrupted_section( &csVirtual, &sigset );
    return status;
}


/***********************************************************************
 *             NtReadVirtualMemory   (NTDLL.@)
 *             ZwReadVirtualMemory   (NTDLL.@)
 */
NTSTATUS WINAPI NtReadVirtualMemory( HANDLE process, const void *addr, void *buffer,
                                     SIZE_T size, SIZE_T *bytes_read )
{
    NTSTATUS status;

    if (virtual_check_buffer_for_write( buffer, size ))
    {
        SERVER_START_REQ( read_process_memory )
        {
            req->handle = wine_server_obj_handle( process );
            req->addr   = wine_server_client_ptr( addr );
            wine_server_set_reply( req, buffer, size );
            if ((status = wine_server_call( req ))) size = 0;
        }
        SERVER_END_REQ;
    }
    else
    {
        status = STATUS_ACCESS_VIOLATION;
        size = 0;
    }
    if (bytes_read) *bytes_read = size;
    return status;
}


/***********************************************************************
 *             NtWriteVirtualMemory   (NTDLL.@)
 *             ZwWriteVirtualMemory   (NTDLL.@)
 */
NTSTATUS WINAPI NtWriteVirtualMemory( HANDLE process, void *addr, const void *buffer,
                                      SIZE_T size, SIZE_T *bytes_written )
{
    NTSTATUS status;

    if (virtual_check_buffer_for_read( buffer, size ))
    {
        SERVER_START_REQ( write_process_memory )
        {
            req->handle     = wine_server_obj_handle( process );
            req->addr       = wine_server_client_ptr( addr );
            wine_server_add_data( req, buffer, size );
            if ((status = wine_server_call( req ))) size = 0;
        }
        SERVER_END_REQ;
    }
    else
    {
        status = STATUS_PARTIAL_COPY;
        size = 0;
    }
    if (bytes_written) *bytes_written = size;
    return status;
}


/***********************************************************************
 *             NtAreMappedFilesTheSame   (NTDLL.@)
 *             ZwAreMappedFilesTheSame   (NTDLL.@)
 */
NTSTATUS WINAPI NtAreMappedFilesTheSame(PVOID addr1, PVOID addr2)
{
    struct file_view *view1, *view2;
    NTSTATUS status;
    sigset_t sigset;

    TRACE("%p %p\n", addr1, addr2);

    server_enter_uninterrupted_section( &csVirtual, &sigset );

    view1 = VIRTUAL_FindView( addr1, 0 );
    view2 = VIRTUAL_FindView( addr2, 0 );

    if (!view1 || !view2)
        status = STATUS_INVALID_ADDRESS;
    else if (is_view_valloc( view1 ) || is_view_valloc( view2 ))
        status = STATUS_CONFLICTING_ADDRESSES;
    else if (view1 == view2)
        status = STATUS_SUCCESS;
    else if ((view1->protect & VPROT_SYSTEM) || (view2->protect & VPROT_SYSTEM))
        status = STATUS_NOT_SAME_DEVICE;
    else
    {
        SERVER_START_REQ( is_same_mapping )
        {
            req->base1 = wine_server_client_ptr( view1->base );
            req->base2 = wine_server_client_ptr( view2->base );
            status = wine_server_call( req );
        }
        SERVER_END_REQ;
    }

    server_leave_uninterrupted_section( &csVirtual, &sigset );
    return status;
}
