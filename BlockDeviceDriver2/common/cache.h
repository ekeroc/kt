/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * cache.c
 *
 */

#ifndef _CACHE_H_
#define _CACHE_H_

#define DEBUG   0

/*
 * tree_leaf is a data structure that contains (key, user data).
 * so that it can be used for any type of user data.
 * tree leaf will be added to radix tree as a leaf node.
 */
struct tree_leaf {
    /* must put in the first */
    void *data;  //pointer to user data

    /* least recent used list */
    struct list_head list;  //entry to add cache pool's lru list

    /* for memory reclaim */
    uint64_t blkid;         //key of cache items (LBID when apply cache to metadata)

    uint32_t flag_refcnt;   //reference flags to know whether a item can be free
#define tree_leaf_set_delete(leaf)  (leaf)->flag_refcnt |= (1u<<31)
#define tree_leaf_is_delete(leaf)   ((leaf)->flag_refcnt & (1u<<31))
#define tree_leaf_ref_count(leaf)   ((leaf)->flag_refcnt & (~(1u<<31)))
} __attribute__((__packed__));

typedef enum {
    INVAL_CACHE_LB = 1,
    INVAL_CACHE_VOLID,
    INVAL_CACHE_COND_CHK,
    INVAL_CACHE_HB,
} cache_inval_op_t;

extern atomic_t dms_node_count;

typedef void (cache_item_free_fn) (void *metadata_item);
typedef int32_t (metadata_cmp_fn) (void *metadata_item, void *cond_criteria, int32_t *update_flag);

extern int32_t cacheMgr_rel_pool(uint64_t vid);
extern int32_t cacheMgr_create_pool(uint64_t volumeID, uint64_t capacity);

extern int32_t cache_invalidate(int8_t type, uint64_t volid, uint64_t start, uint32_t count,
        metadata_cmp_fn *md_cmp_fn, void *criteria);
extern int32_t cache_update(uint64_t volid, uint64_t start, uint32_t count, void **basket);
extern int32_t cache_find(uint64_t volid, uint64_t start, uint32_t count, void **basket);
extern int32_t cache_put(uint64_t volid, uint64_t start, uint32_t count, void **basket);

extern int32_t cacheMgr_get_num_treeleaf(void);
extern void cacheMgr_get_cachePool_size(uint64_t volid, uint32_t *cache_size, uint32_t *curr_cache_items);
extern int32_t cacheMgr_init_manager(cache_item_free_fn *citem_free_fn);
extern void cacheMgr_rel_manager(void);

#endif /* __CCMA_NBD_CACHE_H  */

#ifdef ENABLE_PAYLOAD_CACHE_FOR_DEBUG
/*
 *  1. we need to tranverse payload for some peticular volumee, so list. (kick out by lru)
 *  2. we may want to lookup a payload in other volume's payloads, so global hash.
 *  3. recent-used list needed to kick payload out.
 *  4. on kicking, we need to remove from valumn's tranversing list too (kick out by lru).
 */
struct payload_node {
    uint64_t pbID;
    struct hlist_node hlist;
    struct list_head list;  // lru list
    void *data;
};

#define PAYLOAD_HASH_SIZE   1024L //Power of 2
#define PAYLOAD_HASH_MOD_NUM    (PAYLOAD_HASH_SIZE - 1)

typedef struct payload_cache_item {
    struct list_head list;
    unsigned long long sector;
    char payloads[512];
} payload_cache_item_t;

typedef struct payload_cache_volume {
    struct hlist_node hlist;
    unsigned long long volid;   // 30 bits
    struct mutex cache_lock;
    struct list_head payload_list[PAYLOAD_HASH_SIZE];
} payload_cache_volume_t;

extern int payload_cache_init( void );
extern int payload_cache_release( void );
extern int payload_volume_create( unsigned long long volumeID );
extern int payload_volume_release( unsigned long long volumeID );
extern void payload_volume_remove_all( void );
extern int add_entry_to_cache( unsigned long long volid,
        unsigned long long sector, char* payload );
extern int query_payload_in_cache( unsigned long long volid,
        unsigned long long sector, char* payload );
#endif	/* _CACHE_H_ */
