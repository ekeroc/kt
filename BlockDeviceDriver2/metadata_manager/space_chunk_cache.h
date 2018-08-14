/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * space_chunk_cache.h
 *
 * A caching system for querying metadata that not yet be flush to namenode
 */


#ifndef METADATA_MANAGER_SPACE_CHUNK_CACHE_H_
#define METADATA_MANAGER_SPACE_CHUNK_CACHE_H_

#ifdef DISCO_PREALLOC_SUPPORT

#define MEM_BUCKET_SIZE 1024
#define MEM_ENTRY_SIZE  8
#define NUM_ENTRY_BUCKET    (MEM_BUCKET_SIZE / MEM_ENTRY_SIZE)
#define BUCKET_MOD_VAL  (NUM_ENTRY_BUCKET - 1)
#define BIT_SHIFT_BKSIZE   7
#define DUMMY_ENTRY_VAL 0L
#define RADIX_TREE_KEY_SHIFT    1 //NOTE: radix tree has bug, we need key value 0 as error key

#define SM_GET_CACHEKEY_LBID(x) ((x >> BIT_SHIFT_BKSIZE) + 1)

typedef struct sm_cache_pool {
    struct rw_semaphore cache_plock;
    dms_radix_tree_root_t cache_pool;
    struct list_head cache_lru;
    uint32_t num_cache_items;
    uint32_t num_cache_buckets;
} sm_cache_pool_t;

extern int32_t smcache_add_aschunk(sm_cache_pool_t *cpool, as_chunk_t *chunk);
extern int32_t smcache_rm_aschunk(sm_cache_pool_t *cpool, uint64_t lb_s, uint32_t lb_len);
extern int32_t smcache_find_aschunk(sm_cache_pool_t *cpool, uint64_t lb_s, uint32_t lb_len, as_chunk_t **chunk_bk);
extern int32_t smcache_query_MData(sm_cache_pool_t *cpool, uint32_t vol_id, uint64_t lb_s, uint32_t lb_len, metadata_t *arr_MD_bk);

extern int32_t smcache_init_cache(sm_cache_pool_t *cpool);
extern int32_t smcache_rel_cache(sm_cache_pool_t *cpool);

#endif

#endif /* METADATA_MANAGER_SPACE_CHUNK_CACHE_H_ */
