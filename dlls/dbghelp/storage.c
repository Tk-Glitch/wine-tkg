/*
 * Various storage structures (pool allocation, vector, hash table)
 *
 * Copyright (C) 1993, Eric Youngdale.
 *               2004, Eric Pouech
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
#include <assert.h>
#include <stdlib.h>
#include "wine/debug.h"

#include "dbghelp_private.h"
#ifdef USE_STATS
#include <math.h>
#endif

WINE_DEFAULT_DEBUG_CHANNEL(dbghelp);

struct pool_arena
{
    struct list entry;
    char       *current;
    char       *end;
};

void pool_init(struct pool* a, size_t arena_size)
{
    list_init( &a->arena_list );
    list_init( &a->arena_full );
    a->arena_size = arena_size;
}

void pool_destroy(struct pool* pool)
{
    struct pool_arena*  arena;
    struct pool_arena*  next;

#ifdef USE_STATS
    size_t alloc, used, num;

    alloc = used = num = 0;
    LIST_FOR_EACH_ENTRY( arena, &pool->arena_list, struct pool_arena, entry )
    {
        alloc += arena->end - (char *)arena;
        used += arena->current - (char*)arena;
        num++;
    }
    LIST_FOR_EACH_ENTRY( arena, &pool->arena_full, struct pool_arena, entry )
    {
        alloc += arena->end - (char *)arena;
        used += arena->current - (char*)arena;
        num++;
    }
    if (alloc == 0) alloc = 1;      /* avoid division by zero */
    FIXME("STATS: pool %p has allocated %u kbytes, used %u kbytes in %u arenas, non-allocation ratio: %.2f%%\n",
          pool, (unsigned)(alloc >> 10), (unsigned)(used >> 10), (unsigned)num,
          100.0 - (float)used / (float)alloc * 100.0);
#endif

    LIST_FOR_EACH_ENTRY_SAFE( arena, next, &pool->arena_list, struct pool_arena, entry )
    {
        list_remove( &arena->entry );
        HeapFree(GetProcessHeap(), 0, arena);
    }
    LIST_FOR_EACH_ENTRY_SAFE( arena, next, &pool->arena_full, struct pool_arena, entry )
    {
        list_remove( &arena->entry );
        HeapFree(GetProcessHeap(), 0, arena);
    }
}

void* pool_alloc(struct pool* pool, size_t len)
{
    struct pool_arena*  arena;
    void*               ret;
    size_t size;

    len = (len + 3) & ~3; /* round up size on DWORD boundary */

    LIST_FOR_EACH_ENTRY( arena, &pool->arena_list, struct pool_arena, entry )
    {
        if (arena->end - arena->current >= len)
        {
            ret = arena->current;
            arena->current += len;
            if (arena->current + 16 >= arena->end)
            {
                list_remove( &arena->entry );
                list_add_tail( &pool->arena_full, &arena->entry );
            }
            return ret;
        }
    }

    size = max( pool->arena_size, len );
    arena = HeapAlloc(GetProcessHeap(), 0, size + sizeof(struct pool_arena));
    if (!arena) return NULL;

    ret = arena + 1;
    arena->current = (char*)ret + len;
    arena->end = (char*)ret + size;
    if (arena->current + 16 >= arena->end)
        list_add_tail( &pool->arena_full, &arena->entry );
    else
        list_add_head( &pool->arena_list, &arena->entry );
    return ret;
}

char* pool_strdup(struct pool* pool, const char* str)
{
    char* ret;
    if ((ret = pool_alloc(pool, strlen(str) + 1))) strcpy(ret, str);
    return ret;
}

void vector_init(struct vector* v, unsigned esz, unsigned bucket_sz)
{
    v->buckets = NULL;
    /* align size on DWORD boundaries */
    v->elt_size = (esz + 3) & ~3;
    switch (bucket_sz)
    {
    case    2: v->shift =  1; break;
    case    4: v->shift =  2; break;
    case    8: v->shift =  3; break;
    case   16: v->shift =  4; break;
    case   32: v->shift =  5; break;
    case   64: v->shift =  6; break;
    case  128: v->shift =  7; break;
    case  256: v->shift =  8; break;
    case  512: v->shift =  9; break;
    case 1024: v->shift = 10; break;
    default: assert(0);
    }
    v->num_buckets = 0;
    v->buckets_allocated = 0;
    v->num_elts = 0;
}

unsigned vector_length(const struct vector* v)
{
    return v->num_elts;
}

void* vector_at(const struct vector* v, unsigned pos)
{
    unsigned o;

    if (pos >= v->num_elts) return NULL;
    o = pos & ((1 << v->shift) - 1);
    return (char*)v->buckets[pos >> v->shift] + o * v->elt_size;
}

void* vector_add(struct vector* v, struct pool* pool)
{
    unsigned    ncurr = v->num_elts++;

    /* check that we don't wrap around */
    assert(v->num_elts > ncurr);
    if (ncurr == (v->num_buckets << v->shift))
    {
        if(v->num_buckets == v->buckets_allocated)
        {
            /* Double the bucket cache, so it scales well with big vectors.*/
            unsigned    new_reserved;
            void*       new;

            new_reserved = 2*v->buckets_allocated;
            if(new_reserved == 0) new_reserved = 1;

            /* Don't even try to resize memory.
               Pool datastructure is very inefficient with reallocs. */
            new = pool_alloc(pool, new_reserved * sizeof(void*));
            memcpy(new, v->buckets, v->buckets_allocated * sizeof(void*));
            v->buckets = new;
            v->buckets_allocated = new_reserved;
        }
        v->buckets[v->num_buckets] = pool_alloc(pool, v->elt_size << v->shift);
        return v->buckets[v->num_buckets++];
    }
    return vector_at(v, ncurr);
}

/* We construct the sparse array as two vectors (of equal size)
 * The first vector (key2index) is the lookup table between the key and
 * an index in the second vector (elements)
 * When inserting an element, it's always appended in second vector (and
 * never moved in memory later on), only the first vector is reordered
 */
struct key2index
{
    unsigned long       key;
    unsigned            index;
};

void sparse_array_init(struct sparse_array* sa, unsigned elt_sz, unsigned bucket_sz)
{
    vector_init(&sa->key2index, sizeof(struct key2index), bucket_sz);
    vector_init(&sa->elements, elt_sz, bucket_sz);
}

/******************************************************************
 *		sparse_array_lookup
 *
 * Returns the first index which key is >= at passed key
 */
static struct key2index* sparse_array_lookup(const struct sparse_array* sa,
                                             unsigned long key, unsigned* idx)
{
    struct key2index*   pk2i;
    unsigned            low, high;

    if (!sa->elements.num_elts)
    {
        *idx = 0;
        return NULL;
    }
    high = sa->elements.num_elts;
    pk2i = vector_at(&sa->key2index, high - 1);
    if (pk2i->key < key)
    {
        *idx = high;
        return NULL;
    }
    if (pk2i->key == key)
    {
        *idx = high - 1;
        return pk2i;
    }
    low = 0;
    pk2i = vector_at(&sa->key2index, low);
    if (pk2i->key >= key)
    {
        *idx = 0;
        return pk2i;
    }
    /* now we have: sa(lowest key) < key < sa(highest key) */
    while (low < high)
    {
        *idx = (low + high) / 2;
        pk2i = vector_at(&sa->key2index, *idx);
        if (pk2i->key > key)            high = *idx;
        else if (pk2i->key < key)       low = *idx + 1;
        else                            return pk2i;
    }
    /* binary search could return exact item, we search for highest one
     * below the key
     */
    if (pk2i->key < key)
        pk2i = vector_at(&sa->key2index, ++(*idx));
    return pk2i;
}

void*   sparse_array_find(const struct sparse_array* sa, unsigned long key)
{
    unsigned            idx;
    struct key2index*   pk2i;

    if ((pk2i = sparse_array_lookup(sa, key, &idx)) && pk2i->key == key)
        return vector_at(&sa->elements, pk2i->index);
    return NULL;
}

void*   sparse_array_add(struct sparse_array* sa, unsigned long key, 
                         struct pool* pool)
{
    unsigned            idx, i;
    struct key2index*   pk2i;
    struct key2index*   to;

    pk2i = sparse_array_lookup(sa, key, &idx);
    if (pk2i && pk2i->key == key)
    {
        FIXME("re-adding an existing key\n");
        return NULL;
    }
    to = vector_add(&sa->key2index, pool);
    if (pk2i)
    {
        /* we need to shift vector's content... */
        /* let's do it brute force... (FIXME) */
        assert(sa->key2index.num_elts >= 2);
        for (i = sa->key2index.num_elts - 1; i > idx; i--)
        {
            pk2i = vector_at(&sa->key2index, i - 1);
            *to = *pk2i;
            to = pk2i;
        }
    }

    to->key = key;
    to->index = sa->elements.num_elts;

    return vector_add(&sa->elements, pool);
}

unsigned sparse_array_length(const struct sparse_array* sa)
{
    return sa->elements.num_elts;
}

static unsigned hash_table_hash(const char* name, unsigned num_buckets)
{
    unsigned    hash = 0;
    while (*name)
    {
        hash += *name++;
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash % num_buckets;
}

void hash_table_init(struct pool* pool, struct hash_table* ht, unsigned num_buckets)
{
    ht->num_elts = 0;
    ht->num_buckets = num_buckets;
    ht->pool = pool;
    ht->buckets = NULL;
}

void hash_table_destroy(struct hash_table* ht)
{
#if defined(USE_STATS)
    int                         i;
    unsigned                    len;
    unsigned                    min = 0xffffffff, max = 0, sq = 0;
    struct hash_table_elt*      elt;
    double                      mean, variance;

    for (i = 0; i < ht->num_buckets; i++)
    {
        for (len = 0, elt = ht->buckets[i]; elt; elt = elt->next) len++;
        if (len < min) min = len;
        if (len > max) max = len;
        sq += len * len;
    }
    mean = (double)ht->num_elts / ht->num_buckets;
    variance = (double)sq / ht->num_buckets - mean * mean;
    FIXME("STATS: elts[num:%-4u size:%u mean:%f] buckets[min:%-4u variance:%+f max:%-4u]\n",
          ht->num_elts, ht->num_buckets, mean, min, variance, max);

    for (i = 0; i < ht->num_buckets; i++)
    {
        for (len = 0, elt = ht->buckets[i]; elt; elt = elt->next) len++;
        if (len == max)
        {
            FIXME("Longest bucket:\n");
            for (elt = ht->buckets[i]; elt; elt = elt->next)
                FIXME("\t%s\n", elt->name);
            break;
        }

    }
#endif
}

void hash_table_add(struct hash_table* ht, struct hash_table_elt* elt)
{
    unsigned                    hash = hash_table_hash(elt->name, ht->num_buckets);

    if (!ht->buckets)
    {
        ht->buckets = pool_alloc(ht->pool, ht->num_buckets * sizeof(struct hash_table_bucket));
        assert(ht->buckets);
        memset(ht->buckets, 0, ht->num_buckets * sizeof(struct hash_table_bucket));
    }

    /* in some cases, we need to get back the symbols of same name in the order
     * in which they've been inserted. So insert new elements at the end of the list.
     */
    if (!ht->buckets[hash].first)
    {
        ht->buckets[hash].first = elt;
    }
    else
    {
        ht->buckets[hash].last->next = elt;
    }
    ht->buckets[hash].last = elt;
    elt->next = NULL;
    ht->num_elts++;
}

void hash_table_iter_init(const struct hash_table* ht, 
                          struct hash_table_iter* hti, const char* name)
{
    hti->ht = ht;
    if (name)
    {
        hti->last = hash_table_hash(name, ht->num_buckets);
        hti->index = hti->last - 1;
    }
    else
    {
        hti->last = ht->num_buckets - 1;
        hti->index = -1;
    }
    hti->element = NULL;
}

void* hash_table_iter_up(struct hash_table_iter* hti)
{
    if (!hti->ht->buckets) return NULL;

    if (hti->element) hti->element = hti->element->next;
    while (!hti->element && hti->index < hti->last) 
        hti->element = hti->ht->buckets[++hti->index].first;
    return hti->element;
}
