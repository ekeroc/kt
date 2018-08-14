/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * cache_private.h
 *
 */

#ifndef _CACHE_PRIVATE_H_
#define _CACHE_PRIVATE_H_

#define CACHE_STAT  1

#ifdef CACHE_STAT
struct cache_stat {
    uint64_t m_hit;
    uint64_t m_miss;
    uint64_t p_hit;
    uint64_t p_miss;
};
#endif

#ifdef DISCO_ERC_SUPPORT
#define HBID_TBL_SIZE   4096
#define HBID_TBL_MOD_VAL    (HBID_TBL_SIZE - 1)

typedef struct lbid_item {
    struct list_head list_for_h; //list for hbid item
    uint64_t LBID;
} lbid_item_t;

typedef struct hbid_ht_item {
    struct hlist_node hlist;
    struct list_head LBID_list;
    uint64_t HBID;
} hbid_ht_item_t;
#endif

typedef struct volume_cache_pool {
    struct hlist_node hlist;   // entry to cache pool hashtabl
    uint64_t volid;            // 30 bits volume ID as pool id
    uint64_t capacity;         // volume's capacity, for calculating max pool size (cache items)

    atomic_t cache_num;        // number of cache items in pool
    uint32_t cache_size;       // max cache items (pool size) a pool can hold

    struct mutex tree_lock;    // tree lock to protect pool
    dms_radix_tree_root_t tree; //radix tree to hold data and lookup
    struct list_head lru_list;  //lru_list link user data base on lru policy.

    /* one spinlock for protecting tree_leaf's reference count */
    spinlock_t tree_leaf_lock;
    atomic_t ref_cnt;
    struct cache_stat stat;

#ifdef DISCO_ERC_SUPPORT
    struct hlist_head hbid_table[HBID_TBL_SIZE];
#endif
} cache_pool_t;

#define CACHE_POOL_HSIZE    257
#define MAX_FREE_VOL_RETRY  30
#define FREE_VOL_RETRY_PERIOD  1000 //unit msec

typedef struct cache_pool_manager {
    struct hlist_head cpool_tbl[CACHE_POOL_HSIZE];  //hashtable to hold all pool
    struct rw_semaphore cpool_tbl_lock;             //lock to protect hashtable
    struct kmem_cache *tree_leaf_pool;              //kernel memory cache for allocating tree leaf
    atomic_t tree_leaf_cnt;                         //number of tree leaf be allocated
    cache_item_free_fn *citem_free_fn;              //free function to free user data
} cpool_manager_t;

cpool_manager_t cpool_Mgr;

static cache_pool_t *_find_cache_pool(uint64_t volid);

#endif /* _CACHE_PRIVATE_H_ */
