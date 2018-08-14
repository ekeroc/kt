/*
 * Copyright (C) 2001 Momchil Velikov
 * Portions Copyright (C) 2001 Christoph Hellwig
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _DMS_RADIX_TREE_H_
#define _DMS_RADIX_TREE_H_

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0)
#define dms_get_cpu_ptr(x)  (this_cpu_ptr(&x))
#else
#define gfpflags_allow_blocking(x) (x & __GFP_WAIT)
#define dms_get_cpu_ptr(x)  (&__get_cpu_var(x))
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
struct kmem_cache *dms_kmem_cache_create_common(const char *name, size_t size,
        size_t align, unsigned long flags,
        void (*ctor)(void *), void (*dtor )(void *));
#else
struct kmem_cache *dms_kmem_cache_create_common(const char *name, size_t size,
        size_t align, unsigned long flags,
        void (*ctor)(void *, struct kmem_cache *, unsigned long),
        void (*dtor)(void *, struct kmem_cache *, unsigned long));
#endif

#ifdef __KERNEL__
#define _RADIX_TREE_MAP_SHIFT    6
#else
#define _RADIX_TREE_MAP_SHIFT    3   /* For more stressful testing */
#endif

#define _RADIX_TREE_MAP_SIZE (1ULL << _RADIX_TREE_MAP_SHIFT)
#define _RADIX_TREE_MAP_MASK (_RADIX_TREE_MAP_SIZE-1)

struct dms_radix_tree_node {
    uint32_t    count;
    void        *slots[_RADIX_TREE_MAP_SIZE];
} __attribute__((__packed__));
typedef struct dms_radix_tree_node dms_radix_tree_node_t;

/* root tags are stored in gfp_mask, shifted by __GFP_BITS_SHIFT */
struct dms_radix_tree_root {
    uint32_t        height;
    gfp_t           gfp_mask;
    struct dms_radix_tree_node  *rnode;
};
typedef struct dms_radix_tree_root dms_radix_tree_root_t;

#define DMS_RADIX_TREE_INIT(mask)   {                   \
    .height = 0,                            \
    .gfp_mask = (mask),                     \
    .rnode = NULL,                          \
}

#define DMS_RADIX_TREE(name, mask) \
    struct dms_radix_tree_root name = DMS_RADIX_TREE_INIT(mask)

#define DMS_INIT_RADIX_TREE(root, mask)                 \
do {                                    \
    (root)->height = 0;                     \
    (root)->gfp_mask = (mask);                  \
    (root)->rnode = NULL;                       \
} while (0)



extern int dms_radix_tree_insert(dms_radix_tree_root_t *, uint64_t,
                                 void *, void **);
//void *dms_radix_tree_lookup(dms_radix_tree_root_t *, unsigned long long );
//void **dms_radix_tree_lookup_slot(dms_radix_tree_root_t *, unsigned long long );
extern void *dms_radix_tree_delete(dms_radix_tree_root_t *, uint64_t);
extern uint32_t dms_radix_tree_gang_lookup(dms_radix_tree_root_t *root,
                     void **results, uint64_t first_index, uint32_t max_items);
////int dms_radix_tree_preload( gfp_t gfp_mask );
extern void dms_radix_tree_init(void);
extern void dms_radix_tree_exit(void);
//void dms_radix_tree_dump(dms_radix_tree_node_t *node, int height );

/*static inline void radix_tree_preload_end( void )
{
    preempt_enable();
}
*/

#endif /* _DMS_RADIX_TREE_H_ */









