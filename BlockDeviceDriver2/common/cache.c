/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * cache.c
 *
 */
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/blkdev.h>
#include <net/sock.h>
#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/dms_kernel_version.h"
#include "../common/discoC_mem_manager.h"
#include "../lib/dms-radix-tree.h"
#include "cache.h"
#include "cache_private.h"
#include "../config/dmsc_config.h"

#ifdef DISCO_ERC_SUPPORT
#include "../metadata_manager/metadata_manager.h"
static lbid_item_t *_alloc_lbid_item (uint64_t lbid)
{
    lbid_item_t *lbid_item;

    lbid_item = (lbid_item_t *)discoC_mem_alloc(sizeof(lbid_item_t), GFP_NOWAIT);
    if (unlikely(IS_ERR_OR_NULL(lbid_item))) {
        return NULL;
    }

    INIT_LIST_HEAD(&lbid_item->list_for_h);
    lbid_item->LBID = lbid;

    return lbid_item;
}

static hbid_ht_item_t *_alloc_hbid_item (uint64_t lbid, uint64_t hbid)
{
    hbid_ht_item_t *hbid_item;
    lbid_item_t *lbid_item;

    lbid_item = _alloc_lbid_item(lbid);
    if (unlikely(IS_ERR_OR_NULL(lbid_item))) {
        return NULL;
    }

    hbid_item = (hbid_ht_item_t *)discoC_mem_alloc(sizeof(hbid_ht_item_t), GFP_NOWAIT);
    if (unlikely(IS_ERR_OR_NULL(hbid_item))) {
        discoC_mem_free(lbid_item);
        return NULL;
    }

    INIT_HLIST_NODE(&hbid_item->hlist);
    INIT_LIST_HEAD(&hbid_item->LBID_list);
    hbid_item->HBID = hbid;
    list_add_tail(&lbid_item->list_for_h, &hbid_item->LBID_list);

    return hbid_item;
}

static void _add_lbid2hbid (uint64_t lbid, hbid_ht_item_t *hbid_item)
{
    lbid_item_t *cur, *next, *new;
    bool addlb;

    addlb = true;
    list_for_each_entry_safe (cur, next, &hbid_item->LBID_list, list_for_h) {
        if (unlikely(IS_ERR_OR_NULL(cur))) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN LBID_list broken of hbid item (%llu %llu)\n",
                    lbid, hbid_item->HBID);
            return;
        }

        if (lbid == cur->LBID) {
            addlb = false;
            break;
        }
    }

    if (addlb && !IS_ERR_OR_NULL(new = _alloc_lbid_item(lbid))) {
        list_add_tail(&new->list_for_h, &hbid_item->LBID_list);
    }
}

static void _insert_hbid_htbl (uint64_t lbid, uint64_t hbid, cache_pool_t *vol)
{
    struct hlist_head *hhead;
    hbid_ht_item_t *hbid_item, *hitem;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
    struct hlist_node *node;
#endif
    struct hlist_node *tmp;

    hhead = &(vol->hbid_table[hbid & HBID_TBL_MOD_VAL]);

    hbid_item = NULL;
    hlist_for_each_entry_safe_ccma (hitem, node, tmp, hhead, hlist) {
        if (unlikely(IS_ERR_OR_NULL(hitem))) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN hlist broken of hbid item (%llu %llu)\n", lbid, hbid);
            return;
        }

        if (hitem->HBID == hbid) {
            hbid_item = hitem;
            break;
        }
    }

    if (IS_ERR_OR_NULL(hbid_item)) {
        hbid_item = _alloc_hbid_item(lbid, hbid);
        if (!IS_ERR_OR_NULL(hbid_item)) {
            hlist_add_head(&hbid_item->hlist, hhead);
            _add_lbid2hbid(lbid, hbid_item);
        }
    } else {
        _add_lbid2hbid(lbid, hbid_item);
    }
}

static void _remove_lbid_from_hbid (uint64_t lbid, hbid_ht_item_t *hbid_item)
{
    lbid_item_t *cur, *next, *item;

    item = NULL;
    list_for_each_entry_safe (cur, next, &hbid_item->LBID_list, list_for_h) {
        if (unlikely(IS_ERR_OR_NULL(cur))) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN LBID_list broken of hbid item (%llu %llu)\n",
                    lbid, hbid_item->HBID);
            return;
        }

        if (lbid == cur->LBID) {
            item = cur;
            break;
        }
    }

    if (!IS_ERR_OR_NULL(item)) {
        list_del(&item->list_for_h);
        discoC_mem_free(item);
    }
}

static void _remove_hbid_tbl (uint64_t lbid, uint64_t hbid, cache_pool_t *vol)
{
    struct hlist_head *hhead;
    hbid_ht_item_t *hbid_item, *hitem;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
    struct hlist_node *node;
#endif
    struct hlist_node *tmp;

    hhead = &(vol->hbid_table[hbid & HBID_TBL_MOD_VAL]);

    hbid_item = NULL;
    hlist_for_each_entry_safe_ccma (hitem, node, tmp, hhead, hlist) {
        if (unlikely(IS_ERR_OR_NULL(hitem))) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN hlist broken of hbid item (%llu %llu)\n", lbid, hbid);
            return;
        }

        if (hitem->HBID == hbid) {
            hbid_item = hitem;
            break;
        }
    }

    if (!IS_ERR_OR_NULL(hbid_item)) {
        _remove_lbid_from_hbid(lbid, hbid_item);

        if (list_empty(&hbid_item->LBID_list)) {
            hlist_del(&hbid_item->hlist);
            discoC_mem_free(hbid_item);
        }
    }
}

static void _remove_hbid (uint64_t hbid, cache_pool_t *vol,
        struct list_head *buff_list)
{
    struct hlist_head *hhead;
    hbid_ht_item_t *hbid_item, *hitem;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
    struct hlist_node *node;
#endif
    struct hlist_node *tmp;
    lbid_item_t *cur, *next;

    hhead = &(vol->hbid_table[hbid & HBID_TBL_MOD_VAL]);

    hbid_item = NULL;
    hlist_for_each_entry_safe_ccma (hitem, node, tmp, hhead, hlist) {
        if (unlikely(IS_ERR_OR_NULL(hitem))) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN hlist broken of hbid item (%llu)\n", hbid);
            return;
        }

        if (hitem->HBID == hbid) {
            hbid_item = hitem;
            break;
        }
    }

    if (!IS_ERR_OR_NULL(hbid_item)) {
        list_for_each_entry_safe (cur, next, &hbid_item->LBID_list, list_for_h) {
            list_move_tail(&cur->list_for_h, buff_list);
        }
        hlist_del(&hbid_item->hlist);
        discoC_mem_free(hbid_item);
    }
}

static void _remove_all_lbids_hbid (hbid_ht_item_t *hbid_item)
{
    lbid_item_t *cur, *next;

    list_for_each_entry_safe (cur, next, &hbid_item->LBID_list, list_for_h) {
        list_del(&cur->list_for_h);
        discoC_mem_free(cur);
    }
}

static void _remove_all_hbids (cache_pool_t *vol)
{
    struct hlist_head *hhead;
    hbid_ht_item_t *hitem;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
    struct hlist_node *node;
#endif
    struct hlist_node *tmp;
    int32_t i;

    for (i = 0; i < HBID_TBL_SIZE; i++) {
        hhead = &(vol->hbid_table[i]);

        hlist_for_each_entry_safe_ccma (hitem, node, tmp, hhead, hlist) {
            if (unlikely(IS_ERR_OR_NULL(hitem))) {
                dms_printk(LOG_LVL_WARN,
                        "DMSC WARN hlist broken rm all\n");
                break;
            }

            hlist_del(&hitem->hlist);
            _remove_all_lbids_hbid(hitem);
            discoC_mem_free(hitem);
        }
    }
}
#endif

/*
 * release_tree_leaf: free tree leaf
 * @leaf: leaf node to be freed
 */
static void release_tree_leaf (struct tree_leaf *leaf)
{
    if (unlikely(tree_leaf_ref_count(leaf) > 0)) {
        dms_printk(LOG_LVL_WARN,
                    "DMSC WARN fail to release radix tree leaf due to "
                    "reference cnt > 0 (blkid is %llu ref_cnt %d)\n",
                    leaf->blkid, leaf->flag_refcnt);
        DMS_WARN_ON(true);
    } else {
        cpool_Mgr.citem_free_fn(leaf->data);
        kmem_cache_free(cpool_Mgr.tree_leaf_pool, leaf);
        atomic_dec(&cpool_Mgr.tree_leaf_cnt);
    }
}

/*
 * __kick_meta_cache: remove cache items from cache pool
 * @cache: cache pool to be check
 * @count: number of cache items need to be removes
 * @md_cmp_fn: check function
 * @criteria: argument for md_cmp_fn
 *
 * Return: whether all items that meet criteria be removed
 *         0: all item removed
 *         1: not all item removed
 * Case 1: count = -1, cache # = 0 => list empty, do nothing
 * Case 2: count = -1, cache # > 0 => remove all (total = cache_num)
 * Case 3: count = 0,  cache # = 0 => remove nothing
 * Case 4: count = 0,  cache # > 0 => remove nothing
 * Case 5: count = x ,  cache # = 0 (x > 0) => list empty, do nothing
 * Case 6: count = x ,  cache # = y (x > 0, y > 0, x >= y) => remove all
 * Case 7: count = x ,  cache # = y (x > 0, y > 0, x < y) => remove count
 *
 */
static int32_t __kick_meta_cache (cache_pool_t *cache, int32_t count,
        metadata_cmp_fn *md_cmp_fn, void *criteria)
{
    struct tree_leaf *leaf, *node;
#ifdef DISCO_ERC_SUPPORT
    uint64_t LBID, HBID;
#endif
    uint32_t nr, total, undate_flag, not_all_clear_flag;

    if (count == -1 || count > atomic_read(&cache->cache_num)) {
        total = atomic_read(&cache->cache_num);
    } else if (count == 0 || count < -1) {
        return 0;
    } else {
        total = count;
    }

    nr = 0;
    not_all_clear_flag = 0;
    list_for_each_entry_safe_reverse (leaf, node, &cache->lru_list, list) {
        if (NULL != md_cmp_fn &&
                md_cmp_fn(leaf->data, criteria, &undate_flag) == false) {
            continue;
        }

        dms_radix_tree_delete(&cache->tree, leaf->blkid);
        list_del(&leaf->list);

#ifdef DISCO_ERC_SUPPORT
        LBID = leaf->blkid;
        HBID = ((struct meta_cache_item *)(leaf->data))->HBID;
        _remove_hbid_tbl(LBID, HBID, cache);
#endif

        atomic_dec(&cache->cache_num);
        spin_lock(&cache->tree_leaf_lock);

        if (tree_leaf_ref_count(leaf) > 0) {
            tree_leaf_set_delete(leaf);
            spin_unlock(&cache->tree_leaf_lock);

            if (-1 == count) {
                not_all_clear_flag = 1;
            }
        } else {
            spin_unlock(&cache->tree_leaf_lock);
            release_tree_leaf(leaf);
        }

        if (-1 != count && ++nr >= total) {
            break;
        }
    }

    return not_all_clear_flag;
}

/*
 * _cache_kick_allpool: traversal all cache pool and remove cache items that meet criteria
 * @md_cmp_fn: check function
 * @criteria: argument for md_cmp_fn
 */
static void _cache_kick_allpool (metadata_cmp_fn *md_cmp_fn, void *criteria)
{
    struct hlist_head *head;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
    struct hlist_node *node;
#endif
    struct hlist_node *tmp;
    cache_pool_t *v;
    int32_t i, j, ret;

    down_write(&cpool_Mgr.cpool_tbl_lock);

    for (i = 0; i < CACHE_POOL_HSIZE; i++) {
        head = &cpool_Mgr.cpool_tbl[i];
        hlist_for_each_entry_safe_ccma (v, node, tmp, head, hlist) {
            if (unlikely(IS_ERR_OR_NULL(v))) {
                dms_printk(LOG_LVL_WARN, "DMSC WARN hlist brokn when invalidate\n");
                break;
            }

            atomic_inc(&v->ref_cnt);
            mutex_lock(&v->tree_lock);
            ret = __kick_meta_cache(v, -1, md_cmp_fn, criteria);
            mutex_unlock(&v->tree_lock);

            for (j = 0; j < MAX_FREE_VOL_RETRY; j++) {
                if (0 == ret) {
                    break;
                } else {
                    dms_printk(LOG_LVL_WARN,
                            "DMSC WARN not all item be freeed try again\n");
                    mutex_lock(&v->tree_lock);
                    ret = __kick_meta_cache(v, -1, NULL, NULL);
                    mutex_unlock(&v->tree_lock);
                }
            }

            atomic_dec(&v->ref_cnt);

        }
    }

    up_write(&cpool_Mgr.cpool_tbl_lock);
}

/*
 * kick_meta_cache: remove some cache items from cache pool
 *     1. vid = -1, check all cache pool in table and remove item that meet criteria
 *     2. vid != -1, remove some cache items from cache pool
 * @vid: target pool want to invalidate
 * @count: how many cache item to be removed
 * @md_cmp_fn: a compare function to know which item should be removed
 * @criteria: argument for md_cmp_fn
 */
static void kick_meta_cache (uint64_t vid, int32_t count,
        metadata_cmp_fn *md_cmp_fn, void *criteria)
{
    cache_pool_t *cache;
    int32_t j, ret;

    if (-1 == vid) {
        _cache_kick_allpool(md_cmp_fn, criteria);
    } else {
        cache = _find_cache_pool(vid);
        if (unlikely(IS_ERR_OR_NULL(cache))) {
            return;
        }

        mutex_lock(&cache->tree_lock);
        ret = __kick_meta_cache(cache, count, md_cmp_fn, criteria);
        mutex_unlock(&cache->tree_lock);

        for (j = 0; j < MAX_FREE_VOL_RETRY; j++) {
            if (0 == ret) {
                break;
            } else {
                dms_printk(LOG_LVL_WARN,
                        "DMSC WARN not all item be freeed try again\n");
                mutex_lock(&cache->tree_lock);
                ret = __kick_meta_cache(cache, -1, NULL, NULL);
                mutex_unlock(&cache->tree_lock);
            }
        }

        atomic_dec(&cache->ref_cnt);
    }
}

/*
 * cache_find: find cache items and reference it to prevent item be invalidated
 * @volid: target pool want to invalidate
 * @start: start key to add cache item
 * @count: number of items
 * @basket: Return to user. Array to hold find result and return to user
 *
 * Return: invalidate success or fail
 *           0: add update ok
 *         < 0: add fail: update fail, caller should handle memory in basket
 */
int32_t cache_find (uint64_t volid, uint64_t start, uint32_t count, void **basket)
{
    cache_pool_t *cache;
    int32_t ret, i, rret;
    uint64_t j, ret_off;
    struct tree_leaf *leaf;

    cache = _find_cache_pool(volid);
    if (unlikely(IS_ERR_OR_NULL(cache))) {
        dms_printk(LOG_LVL_INFO, "DMSC INFO fail find cache pool %llu\n", volid);
        return -ENOENT;
    }

    memset(basket, 0, sizeof(void *)*count);

    ret = 0;
    rret = 0;

    mutex_lock(&cache->tree_lock);

    // borrow basket to store tree_leaf pointers temporarily */
    ret = dms_radix_tree_gang_lookup(&cache->tree, (void **)basket,
            start, count);

    if (ret > 0) {
        //memset(&basket[ret], 0, sizeof(void *)*(count - ret));
        for (i = ret; i < count; ++i) {
            basket[i] = NULL;
        }

        ret_off = start + count;

        for (i = ret - 1; i >= 0; --i) {
            leaf = (void *)basket[i];
            if (unlikely(IS_ERR_OR_NULL(leaf))) {
                basket[i] = NULL;
                continue;
            }

            j = leaf->blkid;
            spin_lock(&cache->tree_leaf_lock);

            if (j >= start && j < ret_off &&
                    likely(tree_leaf_is_delete(leaf) == false)) {
                basket[i] = NULL;
                //basket[j-start] = (void *)&leaf->data;
                basket[j - start] = (void *)leaf;
                ++leaf->flag_refcnt;

                ret_off = j;
                ++rret;

                dms_list_move_head(&leaf->list, &cache->lru_list);
            } else {
                /* radix-tree's bug */
                basket[i] = NULL;
            }

            spin_unlock(&cache->tree_leaf_lock);
        }
    } else {
        memset(basket, 0, sizeof(void *)*count);

        goto CACHE_FIND_OUT;
    }

#ifdef CACHE_STAT
    cache->stat.m_hit += ret;
    cache->stat.m_miss += count - ret;
#endif

CACHE_FIND_OUT:
    mutex_unlock(&cache->tree_lock);
    atomic_dec(&cache->ref_cnt);
    return rret;
}

/*
 * cache_put: dereference specific cache item
 * @volid: target pool want to invalidate
 * @start: start key to add cache item
 * @count: number of items
 * @basket: cache item to be added to cache pool
 *
 * Return: invalidate success or fail
 *           0: add update ok
 *         < 0: add fail: update fail, caller should handle memory in basket
 */
int32_t cache_put (uint64_t volid, uint64_t start, uint32_t count, void **basket)
{
    cache_pool_t *cache;
    int32_t i;
    struct tree_leaf *leaf;

    cache = _find_cache_pool(volid);
    if (unlikely(IS_ERR_OR_NULL(cache))) {
        dms_printk(LOG_LVL_INFO, "DMSC INFO fail find cache pool %llu\n", volid);
        return -ENOENT;
    }

    mutex_lock(&cache->tree_lock);
    spin_lock(&cache->tree_leaf_lock);

    for (i = 0; i < count; ++i) {
        leaf = (void *)basket[i];
        if (IS_ERR_OR_NULL(leaf)) {
            continue;
        }

        --leaf->flag_refcnt;

        if (likely(tree_leaf_is_delete(leaf) == false)) {
            continue;
        }

        if (tree_leaf_ref_count(leaf) > 0) {
            continue;
        }

        spin_unlock(&cache->tree_leaf_lock);
        release_tree_leaf(leaf);
        spin_lock(&cache->tree_leaf_lock);
    }

    spin_unlock(&cache->tree_leaf_lock);

    mutex_unlock(&cache->tree_lock);
    atomic_dec(&cache->ref_cnt);

    return 0;
}

/*
 * cache_invalidate_volLB: invalidate cache with volume ID and LB range
 * @volid: target pool want to invalidate
 * @start: start key to add cache item
 * @count: number of items
 *
 * Return: invalidate success or fail
 *           0: add update ok
 *         < 0: add fail: update fail, caller should handle memory in basket
 */
static int32_t cache_invalidate_volLB (uint64_t volid, uint64_t start, uint32_t count)
{
    cache_pool_t *cache;
    struct tree_leaf *leaf;
#ifdef DISCO_ERC_SUPPORT
    uint64_t LBID, HBID;
#endif
    int32_t i;

    cache = _find_cache_pool(volid);
    if (unlikely(IS_ERR_OR_NULL(cache))) {
        dms_printk(LOG_LVL_INFO, "DMSC INFO fail find cache pool %llu\n", volid);
        return 0;
    }

    mutex_lock(&cache->tree_lock);

    for (i = 0; i < count; ++i) {
        leaf = (struct tree_leaf *)
                dms_radix_tree_delete(&cache->tree, start + i);

        if (IS_ERR_OR_NULL(leaf)) {
            continue;
        }

#ifdef DISCO_ERC_SUPPORT
        LBID = leaf->blkid;
        HBID = ((struct meta_cache_item *)(leaf->data))->HBID;
        _remove_hbid_tbl(LBID, HBID, cache);
#endif

        list_del(&leaf->list); //Remove from lru
        atomic_dec(&cache->cache_num);
        spin_lock(&cache->tree_leaf_lock);

        if (tree_leaf_ref_count(leaf) > 0) {
            tree_leaf_set_delete(leaf);
        } else {
            spin_unlock(&cache->tree_leaf_lock);
            release_tree_leaf(leaf);
            spin_lock(&cache->tree_leaf_lock);
        }

        spin_unlock(&cache->tree_leaf_lock);
    }

    mutex_unlock(&cache->tree_lock);
    atomic_dec(&cache->ref_cnt);
    return 0;
}

#ifdef DISCO_ERC_SUPPORT
static int32_t _cache_invalidate_lockless (cache_pool_t *cache, uint64_t start,
        uint32_t count)
{
    struct tree_leaf *leaf;
    int32_t i;

    for (i = 0; i < count; ++i) {
        leaf = (struct tree_leaf *)
                dms_radix_tree_delete(&cache->tree, start + i);

        if (IS_ERR_OR_NULL(leaf)) {
            continue;
        }

        list_del(&leaf->list); //Remove from lru
        atomic_dec(&cache->cache_num);
        spin_lock(&cache->tree_leaf_lock);

        if (tree_leaf_ref_count(leaf) > 0) {
            tree_leaf_set_delete(leaf);
        } else {
            spin_unlock(&cache->tree_leaf_lock);
            release_tree_leaf(leaf);
            spin_lock(&cache->tree_leaf_lock);
        }

        spin_unlock(&cache->tree_leaf_lock);
    }

    return 0;
}

static void _invalidate_cache_by_LBID_list (struct list_head *lbid_list,
        cache_pool_t *v)
{
    lbid_item_t *cur, *next;

    list_for_each_entry_safe (cur, next, lbid_list, list_for_h) {
        list_del(&cur->list_for_h);
        _cache_invalidate_lockless(v, cur->LBID, 1);
        discoC_mem_free(cur);
    }
}

static int32_t cache_invalidate_by_HBID (uint64_t hbid)
{
    struct list_head lbid_list;
    struct hlist_head *head;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
    struct hlist_node *node;
#endif
    struct hlist_node *tmp;
    cache_pool_t *v;
    int32_t i, ret;

    ret = 0;

    down_write(&cpool_Mgr.cpool_tbl_lock);

    for (i = 0; i < CACHE_POOL_HSIZE; i++) {
        head = &cpool_Mgr.cpool_tbl[i];

        hlist_for_each_entry_safe_ccma (v, node, tmp, head, hlist) {
            atomic_inc(&v->ref_cnt);
            mutex_lock(&v->tree_lock);

            INIT_LIST_HEAD(&lbid_list);
            _remove_hbid(hbid, v, &lbid_list);
            _invalidate_cache_by_LBID_list(&lbid_list, v);

            mutex_unlock(&v->tree_lock);
            atomic_dec(&v->ref_cnt);
        }
    }
    up_write(&cpool_Mgr.cpool_tbl_lock);

    return ret;
}
#endif

/*
 * cache_invalidate: invalidate cache with different condition
 * @type: what kind of invalidate want to perform
 * @volid: target pool want to invalidate
 * @start: start key to add cache item
 * @count: number of items
 * @md_cmp_fn: Callback function to check whether cache items is target to be invalidated
 * @criteria: input parameter ofr md_cmp_fn
 *
 * Return: invalidate success or fail
 *           0: add update ok
 *         < 0: add fail: update fail, caller should handle memory in basket
 *
 * Invalidate cache items by type:
 *   INVAL_CACHE_LB              1
 *     invalidate items by a volume start lb + length,
 *     ex: volumeID = 1, start_LBID = 123, nr_LBIDs = 16, check_condition = don't_care, criteria = don't_care
 *     Invalidate_Metadata_Cache_Items(INVAL_CACHE_LB, 1, 123, 16, NULL, NULL);
 *
 *   INVAL_CACHE_VOLID           2
 *     invalidate items by volumeID,
 *     ex: volumeID = 1, start_LBID = don't_care, nr_LBIDs = don't_care, check_condition = don't_care, criteria = don't_care
 *     Invalidate_Metadata_Cache_Items(INVAL_CACHE_VOLID, 1, -1, -1, NULL, NULL);
 *
 *   INVAL_CACHE_COND_CHK  3
 *     invalidate items by call back function,
 *     ex: volumeID = 1, start_LBID = don't_care, nr_LBIDs = don't_care, check_condition = my_check_condition(), , criteria = get from caller
 *     Invalidate_Metadata_Cache_Items(INVAL_CACHE_COND_CHK, 1, -1, -1, my_check_condition, criteria);
 *
 *     The check_condition callback function implements by caller which depends on what condition caller
 *     wants to check when invalidate a cache item. The cache manager would traverse whole cache and call
 *     check_condition for each item.
 *     ex: invalide_by_datanode, we can check datanode_id in check_condition, if the function return true,
 *     means this item should be invalidate, otherwise needn't.
 *
 */
int32_t cache_invalidate (int8_t type,
        uint64_t volid, uint64_t start, uint32_t count,
        metadata_cmp_fn *md_cmp_fn, void *criteria)
{
    int32_t ret;

    dms_printk(LOG_LVL_DEBUG, "DMSC DEBUG invalidation md type = %d\n", type);
    ret = 0;
    switch (type) {
        case INVAL_CACHE_LB:
            dms_printk(LOG_LVL_DEBUG,
                    "DMSC DEBUG inval LBID: volid=%llu start=%llu count=%d\n",
                    volid, start, count);
            cache_invalidate_volLB(volid, start, count);
            break;

        case INVAL_CACHE_VOLID:
            dms_printk(LOG_LVL_DEBUG, "DMSC DEBUG inval volume: volid=%llu\n",
                    volid);
            kick_meta_cache(volid, -1, NULL, NULL);
            break;

        case INVAL_CACHE_COND_CHK:
            dms_printk(LOG_LVL_DEBUG,
                    "DMSC DEBUG inval call back: volid=%llu\n", volid);
            if (IS_ERR_OR_NULL(md_cmp_fn) == false) {
                kick_meta_cache(-1, -1, md_cmp_fn, criteria);
            }

            break;

        case INVAL_CACHE_HB:
#ifndef DISCO_ERC_SUPPORT
            dms_printk(LOG_LVL_INFO, "DMSC INFO not support DISCO ERC\n");
#else
            ret = cache_invalidate_by_HBID(start);
#endif
            break;
        default:
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN undefined invalidation type = %d\n",  type);
            DMS_WARN_ON(true);
            break;
    }

    return ret;
}

/*
 * cache_update: add cache item to cache pool
 * @volid: target cache pool
 * @start: start key to add cache item
 * @count: number of items
 * @basket: cache items to be added
 *
 * Return: add success of fail
 *           0: add update ok
 *         < 0: add fail: update fail, caller should handle memory in basket
 *
 * Case 1: Count = 0 Cache size = *  cache_num * : return 0
 * Case 2: Count = x Cache size = y  cache_num z  x > y      : return -1
 * Case 3: Count = x Cache size = y  cache_num z  x + z  > y : kick and add
 * Case 4: Count = x Cache size = y  cache_num z  x + z <= y : add
 */
int32_t cache_update (uint64_t volid, uint64_t start, uint32_t count,
        void **basket)
{
    cache_pool_t *cache;
    int32_t ret, i;
    struct tree_leaf *leaf;
    struct tree_leaf *old_leaf;

    ret = 0;
    cache = _find_cache_pool(volid);
    if (unlikely(IS_ERR_OR_NULL(cache))) {
        dms_printk(LOG_LVL_INFO, "DMSC INFO fail find cache pool %llu\n", volid);
        return -ENOENT;
    }

    mutex_lock(&cache->tree_lock);

    if (count > cache->cache_size || cache->cache_size == 0) {
        mutex_unlock(&cache->tree_lock);
        atomic_dec(&cache->ref_cnt);
        return -E2BIG;
    }

    /* reclaim some memory */
    i = atomic_read(&cache->cache_num) + count - cache->cache_size;

    if (i > (int32_t)cache->cache_size) {
        i = cache->cache_size;
    }

    dms_printk(LOG_LVL_DEBUG, "DMSC DEBUG cache_num/cache_size = %d/%d\n",
            atomic_read(&cache->cache_num), cache->cache_size);

    if (i > 0) {
        __kick_meta_cache(cache, i, NULL, NULL);
    }

    for (i = 0; i < count; ++i) {
        leaf = kmem_cache_alloc(cpool_Mgr.tree_leaf_pool, GFP_NOWAIT | GFP_ATOMIC);
        if (unlikely(IS_ERR_OR_NULL(leaf))) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN fail alloc mem for tree leaf\n");
            discoC_mem_free(basket[i]); //TODO: should use caller's free api
            basket[i] = NULL;
            ret = -ENOMEM;
            continue;
        }

        atomic_inc(&cpool_Mgr.tree_leaf_cnt);
        leaf->data = basket[i];
        leaf->blkid = start + i;
        leaf->flag_refcnt = 0;
        INIT_LIST_HEAD(&leaf->list);

        //*old_leaf = NULL;
        old_leaf = NULL;

        ret = dms_radix_tree_insert(&cache->tree,
                                 start + i, leaf, (void **)(&old_leaf));
        if (unlikely(ret < 0)) {
            release_tree_leaf(leaf);
            continue;
        }

#ifdef DISCO_ERC_SUPPORT
        _insert_hbid_htbl(start + i,
                ((struct meta_cache_item *)basket[i])->HBID, cache);
#endif

        list_add(&leaf->list, &cache->lru_list);
        atomic_inc(&cache->cache_num);

        leaf = old_leaf;
        if (IS_ERR_OR_NULL(leaf)) {
            continue;
        }

        list_del(&leaf->list);
        atomic_dec(&cache->cache_num);
        spin_lock(&cache->tree_leaf_lock);

        if (tree_leaf_ref_count(leaf) > 0) {
            tree_leaf_set_delete(leaf);
            spin_unlock(&cache->tree_leaf_lock);
        } else {
            spin_unlock(&cache->tree_leaf_lock);
            release_tree_leaf(leaf);
        }
    }

    mutex_unlock(&cache->tree_lock);
    atomic_dec(&cache->ref_cnt);

    return ret;
}

/*
 * _alloc_cache_pool: alloc a pool instance
 *
 * Return: memory pointer of cache_pool_t
 */
static cache_pool_t *_alloc_cache_pool (void)
{
    return discoC_mem_alloc(sizeof(cache_pool_t), GFP_NOWAIT | GFP_ATOMIC);
}

/*
 * _free_cache_pool: free a cache pool
 * @cpool: target cpool to be free
 */
static void _free_cache_pool (cache_pool_t *cpool)
{
    int32_t i;

    if (IS_ERR_OR_NULL(cpool)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN try to free null cache pool\n");
        return;
    }

    if (atomic_read(&cpool->ref_cnt) == 0) {
        discoC_mem_free(cpool);
        cpool = NULL;
    } else {
        for (i = 0; i < MAX_FREE_VOL_RETRY; i++) {
            msleep(FREE_VOL_RETRY_PERIOD);

            if (atomic_read(&cpool->ref_cnt) == 0) {
                discoC_mem_free(cpool);
                cpool = NULL;
                break;
            }
        }

        if (IS_ERR_OR_NULL(cpool) == false) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN fail to free volume with "
                    "volume id %llu since the reference cnt %d after waiting "
                    "30 seconds\n",
                    cpool->volid, atomic_read(&cpool->ref_cnt));
            DMS_WARN_ON(true);
        }
    }
}

/*
 * _find_cache_pool_nolock: find a cache pool from cache pool table without lock
 * @volid: target cpool to be found
 * @head: which link list to traversal
 *
 * Return: cache pool instance
 */
static cache_pool_t *_find_cache_pool_nolock (uint64_t volid, struct hlist_head *head)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
    struct hlist_node *node;
#endif
    struct hlist_node *tmp;
    cache_pool_t *v, *found;

    found = NULL;
    hlist_for_each_entry_safe_ccma (v, node, tmp, head, hlist) {
        if (v->volid == volid) {
            atomic_inc(&v->ref_cnt);
            found = v;
            break;
        }
    }

    return found;
}

/*
 * _find_cache_pool: find a cache pool from cache pool table
 * @volid: target cpool to be found
 *
 * Return: cache pool instance
 */
static cache_pool_t *_find_cache_pool (uint64_t volid)
{
    struct hlist_head *head;
    cache_pool_t *found;

    // TODO % doesn't work on 64-bit integer, check again!
    head = &cpool_Mgr.cpool_tbl[((unsigned long)volid) % CACHE_POOL_HSIZE];

    down_read(&cpool_Mgr.cpool_tbl_lock);

    found = _find_cache_pool_nolock(volid, head);

    up_read(&cpool_Mgr.cpool_tbl_lock);

    return found;
}

/*
 * _alloc_cachePoolSize_smallVol:
 *     1. allocate cache items to volume with small capacity
 *     2. summary cache items required by volumes with large capacity
 * @max_citems: current available max cache items in whole system
 * @small_volSize: upper bound of volume capacity that treated as small volume
 * @total_cache_need: return value. Summary result of cache items required by volumes with large capcity
 *
 * Return: remaining available cache items after allocation
 */
static int64_t _alloc_cachePoolSize_smallVol (int64_t max_citems,
        int64_t small_volSize, int64_t *total_cache_need)
{
    struct hlist_head *head;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
    struct hlist_node *node;
#endif
    struct hlist_node *tmp;
    cache_pool_t *v;
    int64_t cache_left;
    int32_t i, cache_kick;

    cache_left = max_citems;
    *total_cache_need = 0;

    for (i = 0; i < CACHE_POOL_HSIZE; ++i) {
        head = &cpool_Mgr.cpool_tbl[i];
        hlist_for_each_entry_safe_ccma (v, node, tmp, head, hlist) {
            atomic_inc(&v->ref_cnt);
            dms_printk(LOG_LVL_DEBUG,
                    "DMSC DEBUG cache need for %llu is %llu : capacity: %llu\n",
                    v->volid, (v->capacity >> BIT_LEN_DMS_LB_SIZE),
                    v->capacity);

            if (v->capacity > small_volSize) {
                *total_cache_need += (v->capacity >> BIT_LEN_DMS_LB_SIZE);
                atomic_dec(&v->ref_cnt);
                continue;
            }

            if (cache_left >=  v->cache_size) {
                v->cache_size = ((uint64_t)v->capacity >> BIT_LEN_DMS_LB_SIZE);
                cache_left -= (uint64_t)(v->cache_size);
            } else {
                v->cache_size = cache_left;
                cache_left = 0;
            }

            dms_printk(LOG_LVL_DEBUG,
                    "DMSC DEBUG assign cache size %u to vol %llu cache left %llu\n",
                    v->cache_size, v->volid, cache_left);

            if (atomic_read(&v->cache_num) > v->cache_size) {
                cache_kick = atomic_read(&v->cache_num) - v->cache_size;
                mutex_lock(&v->tree_lock);
                __kick_meta_cache(v, cache_kick, NULL, NULL);
                mutex_unlock(&v->tree_lock);
            }

            atomic_dec(&v->ref_cnt);
        }
    }

    return cache_left;
}

/*
 * _alloc_cachePoolSize_largeVol:
 *     1. allocate cache items to volume with large capacity according the ratio of capacity
 * @max_citems: current available max cache items in whole system
 * @small_volSize: upper bound of volume capacity that treated as small volume
 * @total_cache_need: summary result in _alloc_cachePoolSize_smallVol
 *
 */
static void _alloc_cachePoolSize_largeVol (int64_t max_citems,
        int64_t small_volSize, int64_t total_cache_need)
{
    struct hlist_head *head;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
    struct hlist_node *node;
#endif
    struct hlist_node *tmp;
    cache_pool_t *v;
    int32_t i, origin_size, cached_need, cache_left, cache_kick;

    cache_left = max_citems;
    for (i = 0; i < CACHE_POOL_HSIZE; ++i) {
        head = &cpool_Mgr.cpool_tbl[i];
        hlist_for_each_entry_safe_ccma (v, node, tmp, head, hlist) {
            atomic_inc(&v->ref_cnt);
            if (v->capacity <= small_volSize) {
                atomic_dec(&v->ref_cnt);
                continue;
            }

            origin_size = v->cache_size;
            if (max_citems >= total_cache_need) {
                cached_need = (v->capacity >> BIT_LEN_DMS_LB_SIZE);
            } else {
                cached_need = (max_citems*(v->capacity >> BIT_LEN_DMS_LB_SIZE))
                        / total_cache_need;
            }

            if (cache_left >= cached_need) {
                cache_left -= cached_need;
            } else {
                //Should not happens since we already allocate based on % of volume size
                cached_need = cache_left;
                cache_left = 0;
            }

            v->cache_size = (uint32_t)cached_need;

            dms_printk(LOG_LVL_DEBUG,
                    "DMSC DEBUG update volume %llu cache from %d to %d cache_num %d\n",
                    v->volid, origin_size, v->cache_size, atomic_read(&v->cache_num));

            if (atomic_read(&v->cache_num) > cached_need) {
                cache_kick = atomic_read(&v->cache_num) - cached_need;
                mutex_lock(&v->tree_lock);
                __kick_meta_cache(v, cache_kick, NULL, NULL);
                mutex_unlock(&v->tree_lock);
            }
            atomic_dec(&v->ref_cnt);
        }
    }
}

/*
 * update_cache_pool_size: update each pool's max number of items
 *
 * Algorithm design:
 *     1. total cache items in disco clientnode = dms_client_config->max_meta_cache_sizes
 *     2. default value = 33554432 (means clientnode can hold metadata of cap =
 *        33554432*4K = 128T
 *     3. for volume with capacity <= max_cap_cache_all_hit, we will try to alloc
 *        enough metadata cache item for these volumes
 *     4. for remaining volumes, we allocate remaining items size according the volume size ratio
 */
static void update_cache_pool_size (void)
{
    int64_t total_cache_need, cache_left;

    total_cache_need = 0;

    down_write(&cpool_Mgr.cpool_tbl_lock);

    cache_left = _alloc_cachePoolSize_smallVol(
            dms_client_config->max_meta_cache_sizes,
            dms_client_config->max_cap_cache_all_hit, &total_cache_need);

    dms_printk(LOG_LVL_DEBUG,
            "DMSC DEBUG total_cach_need: %llu and left: %llu\n",
            total_cache_need, cache_left);

    _alloc_cachePoolSize_largeVol(cache_left,
            dms_client_config->max_cap_cache_all_hit,
            total_cache_need);

    up_write(&cpool_Mgr.cpool_tbl_lock);
}

/*
 * _add_cache_pool_tbl: add cache pool to manager table
 * @v: cache pool to be added
 */
static int32_t _add_cache_pool_tbl (cache_pool_t *v)
{
    struct hlist_head *head;
    cache_pool_t *found;
    int32_t ret;

    INIT_HLIST_NODE(&v->hlist);

    // TODO % doesn't work on 64-bit integer, change again!
    head = &cpool_Mgr.cpool_tbl[(v->volid) % CACHE_POOL_HSIZE];

    ret = 0;

    down_write(&cpool_Mgr.cpool_tbl_lock);
    found = _find_cache_pool_nolock(v->volid, head);

    if (IS_ERR_OR_NULL(found)) {
        hlist_add_head(&v->hlist, head);
    } else {
        ret = -EEXIST;
    }

    up_write(&cpool_Mgr.cpool_tbl_lock);

    update_cache_pool_size();

    return ret;
}

/*
 * _rm_cache_pool_tbl: find and remove cache pool from manager table
 * @volid: cache pool ID
 *
 * Return: found cache pool
 */
static cache_pool_t *_rm_cache_pool_tbl (uint64_t volid)
{
    struct hlist_head *head;
    cache_pool_t *found;

    // TODO % doesn't work on 64-bit integer, change back!
    head = &cpool_Mgr.cpool_tbl[((uint64_t)volid) % CACHE_POOL_HSIZE];

    down_write(&cpool_Mgr.cpool_tbl_lock);

    found = _find_cache_pool_nolock(volid, head);
    if (IS_ERR_OR_NULL(found) == false) {
        hlist_del(&found->hlist);
        atomic_dec(&found->ref_cnt); //dec ref_cnt since inc ref_cnt in _find_cache_pool_nolock
    }

    up_write(&cpool_Mgr.cpool_tbl_lock);

    return found;
}

/*
 * _rm_all_cache_pool_tbl: remove and release all cache pool in table
 * TODO: long function, refactor me
 */
static void _rm_all_cache_pool_tbl (void)
{
    int32_t i, j, ret;
    struct hlist_head *head;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
    struct hlist_node *node;
#endif
    struct hlist_node *tmp;
    cache_pool_t *v, *volume_tmp;

    ret = 0;

vol_remove_all:
    down_write(&cpool_Mgr.cpool_tbl_lock);
    volume_tmp = NULL;
    for (i = 0; i < CACHE_POOL_HSIZE; ++i) {
        head = &cpool_Mgr.cpool_tbl[i];
        hlist_for_each_entry_safe_ccma (v, node, tmp, head, hlist) {
            dms_printk(LOG_LVL_DEBUG, "DMSC DEBUG handle table %d\n",
                    (int32_t)v->volid);
            hlist_del(&v->hlist);
            volume_tmp = v;
            atomic_inc(&volume_tmp->ref_cnt);
            break;
        }
		
		if (!IS_ERR_OR_NULL(volume_tmp)) {
		    break;
		}
    }

    up_write(&cpool_Mgr.cpool_tbl_lock);

    if (IS_ERR_OR_NULL(volume_tmp) == false) {
        mutex_lock(&volume_tmp->tree_lock);
        ret = __kick_meta_cache(volume_tmp, -1, NULL, NULL);
        mutex_unlock(&volume_tmp->tree_lock);

        for (j = 0; j < MAX_FREE_VOL_RETRY; j++) {
            if (0 == ret) {
                break;
            } else {
                dms_printk(LOG_LVL_WARN,
                        "DMSC WARN not all item be freeed try again\n");
                mutex_lock(&volume_tmp->tree_lock);
                ret = __kick_meta_cache(volume_tmp, -1, NULL, NULL);
                mutex_unlock(&volume_tmp->tree_lock);
            }
        }

        dms_printk(LOG_LVL_DEBUG, "DMSC DEBUG vol %d left cache_num %ld\n",
                    (int32_t)volume_tmp->volid,
                    (unsigned long)atomic_read(&volume_tmp->cache_num));
        dms_printk(LOG_LVL_DEBUG, "DMSC DEBUG tree->height %d\n",
                volume_tmp->tree.height);
        atomic_dec(&volume_tmp->ref_cnt);
        _free_cache_pool(volume_tmp);
		volume_tmp = NULL;
        goto vol_remove_all;
    }
}

/*
 * cacheMgr_create_pool: create a cache pool
 * @volumeID: cache pool ID
 * @capacity: size of capacity for setting max cache items a pool can own
 *
 * a 4K space own a cache item
 *
 * Return: init success or fail
 *         0: init success
 */
int32_t cacheMgr_create_pool (uint64_t volumeID, uint64_t capacity)
{
    cache_pool_t *v;
#ifdef DISCO_ERC_SUPPORT
    int32_t i;
#endif

    v = _alloc_cache_pool();
    if (unlikely(IS_ERR_OR_NULL(v))) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN fail alloc cache pool volume %llu\n",
                volumeID);
        return -ENOMEM;
    }

    v->volid = volumeID;
    v->capacity = capacity;
    dms_printk(LOG_LVL_DEBUG,
            "DMSC DEBUG volume %llu size is %llu (para %llu)\n",
            v->volid, v->capacity, capacity);
    DMS_INIT_RADIX_TREE(&v->tree, GFP_NOWAIT | GFP_ATOMIC);
    INIT_LIST_HEAD(&v->lru_list);
    mutex_init(&v->tree_lock);
    atomic_set(&v->ref_cnt, 0);
    atomic_set(&v->cache_num, 0);
    spin_lock_init(&v->tree_leaf_lock);
    v->cache_size = (capacity >> BIT_LEN_DMS_LB_SIZE);

#ifdef DISCO_ERC_SUPPORT
    for (i = 0; i < HBID_TBL_SIZE; i++) {
        INIT_HLIST_HEAD(&v->hbid_table[i]);
    }
#endif

    if (_add_cache_pool_tbl(v)) {
        _free_cache_pool(v);
    }

    return 0;
}

/*
 * cacheMgr_rel_pool: release a cache pool
 * @vid: pool id to be release
 *
 * Return: release success or fail
 *         0: init success
 */
int32_t cacheMgr_rel_pool (uint64_t vid)
{
    cache_pool_t *cpool;

    kick_meta_cache(vid, -1, NULL, NULL);

    cpool = _rm_cache_pool_tbl(vid);
    if (IS_ERR_OR_NULL(cpool) == false) {
#ifdef DISCO_ERC_SUPPORT
        _remove_all_hbids(cpool);
#endif
        _free_cache_pool(cpool);

        update_cache_pool_size();
    }

    return 0;
}

/*
 * _init_cacheMgr: initialize a instance of cache manager
 * @cpMgr: target cache manager to be initialized
 * @citem_free_fn: cache item free memory function
 *
 * Return: init success or fail
 *         0: init success
 */
static int32_t _init_cacheMgr (cpool_manager_t *cpMgr, cache_item_free_fn *citem_free_fn)
{
    int32_t i;

    init_rwsem(&cpMgr->cpool_tbl_lock);

    for (i = 0; i < CACHE_POOL_HSIZE; ++i) {
        INIT_HLIST_HEAD(&cpMgr->cpool_tbl[i]);
    }

    cpMgr->tree_leaf_pool = dms_kmem_cache_create_common("DMS-client/tree_leaf",
                     sizeof(struct tree_leaf),
                     0, 0, NULL, NULL);

    if (IS_ERR_OR_NULL(cpMgr->tree_leaf_pool)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN fail create kmem pool for tree leaf\n");
        DMS_WARN_ON(true);
        return -ENOMEM;
    }

    atomic_set(&cpMgr->tree_leaf_cnt, 0);

    cpMgr->citem_free_fn = citem_free_fn;
    return 0;
}

/*
 * cacheMgr_init_manager: init cache manager.
 *      Only caller know the exact content of cache item,
 *      caller should provide free function for free memory properly
 * @citem_free_fn: cache item free memory function
 *
 * Return: init success or fail
 *         0: init success
 */
int32_t cacheMgr_init_manager (cache_item_free_fn *citem_free_fn)
{
    int32_t ret;

    dms_radix_tree_init();

    ret = _init_cacheMgr(&cpool_Mgr, citem_free_fn);
    if (ret) {
        dms_radix_tree_exit();
    }

    return ret;
}

/*
 * cacheMgr_rel_manager: release cache manager
 */
void cacheMgr_rel_manager (void)
{
    synchronize_rcu();
    _rm_all_cache_pool_tbl();
    synchronize_rcu();
    kmem_cache_destroy(cpool_Mgr.tree_leaf_pool);
    dms_radix_tree_exit();
}

/*
 * cacheMgr_get_num_treeleaf: get number of tree leaf be allocated to hold user's
 *                        items. Number of tree leaf = number of current cache items
 *
 * Return: number of tree leaf item
 * NOTE: we export this value to let developer know there are other memory requirement
 *       in cache module.
 */
int32_t cacheMgr_get_num_treeleaf (void)
{
    return atomic_read(&cpool_Mgr.tree_leaf_cnt);
}

/*
 * cacheMgr_get_cachePool_size: get max items a cache pool can hold.
 *                        get current items a cache pool has.
 * @volid: which volume to get
 * @cache_size: return value. max items a cache pool can hold
 * @num_citem: return value. current number items a cache pool has
 */
void cacheMgr_get_cachePool_size (uint64_t volid, uint32_t *cache_size,
        uint32_t *num_citem)
{
    cache_pool_t *cache;

    cache = _find_cache_pool(volid);
    if (unlikely(IS_ERR_OR_NULL(cache))) {
        *cache_size = 0;
        *num_citem = 0;
        return;
    }

    *cache_size = cache->cache_size;
    *num_citem = atomic_read(&cache->cache_num);
    atomic_dec(&cache->ref_cnt);
}

#ifdef ENABLE_PAYLOAD_CACHE_FOR_DEBUG
struct hlist_head payload_vol_table[CACHE_POOL_HSIZE];
rwlock_t payload_vol_table_lock;    /* protect volid_table */

int payload_cache_init(void)
{
    int i;

    rwlock_init(&payload_vol_table_lock);

    for (i = 0; i < CACHE_POOL_HSIZE; ++i) {
        INIT_HLIST_HEAD(&payload_vol_table[i]);
    }

    return 0;
}

payload_cache_volume_t *alloc_payload_volume(void)
{
    return kzalloc(sizeof(payload_cache_volume_t), GFP_KERNEL);
}

void free_payload_volume(struct payload_cache_volume *v)
{
    kfree(v);
    v = NULL;
}

int payload_cache_release(void)
{
    payload_volume_remove_all();
    return 0;
}

payload_cache_volume_t *payload_volume_find(unsigned long long volid)
{
    struct hlist_head *head;
    struct hlist_node *node;
    payload_cache_volume_t *v = NULL;

    // TODO better hash function needed here.
    // TODO % doesn't work on 64-bit integer, change back!
    head = &payload_vol_table[((unsigned long)volid) % CACHE_POOL_HSIZE];

    read_lock(&payload_vol_table_lock);
    hlist_for_each_entry(v, node, head, hlist) {
        if (v->volid == volid) {
            goto unlock;
        }
    }
unlock:
    read_unlock(&payload_vol_table_lock);

    if (likely(v != NULL && v->volid == volid)) {
        return v;
    }

    return NULL;
}

void payload_volume_register(payload_cache_volume_t *v)
{
    INIT_HLIST_NODE(&v->hlist);

    write_lock(&payload_vol_table_lock);
    // TODO better hash function
    // TODO % doesn't work on 64-bit integer, change back!
    hlist_add_head(&v->hlist,
                    &payload_vol_table[((unsigned long)v->volid) %
                                       CACHE_POOL_HSIZE]);
    write_unlock(&payload_vol_table_lock);
}

int payload_volume_create(unsigned long long volumeID)
{
    int i;

    payload_cache_volume_t *v = alloc_payload_volume();
    if (unlikely(v == NULL)) {
        return -ENOMEM;
    }

    v->volid = volumeID;

    mutex_init(&v->cache_lock);
    for (i = 0; i < PAYLOAD_HASH_SIZE; i++) {
        INIT_LIST_HEAD(&v->payload_list[i]);
    }

    payload_volume_register(v);
    dms_printk(LOG_LVL_DEBUG, "%s: create done for "
            "id=%llu\n", __func__, volumeID);

    return 0;
}

void payload_item_release_by_volume(payload_cache_volume_t *v)
{
    payload_cache_item_t *item;
    int i;
    struct list_head *lhead, *cur, *next;

    if (v != NULL) {
        mutex_lock(&v->cache_lock);
        for (i = 0; i < PAYLOAD_HASH_SIZE; i++) {
            lhead = &v->payload_list[i];
            list_for_each_safe(cur, next, lhead) {
                if(cur != NULL) {
                    list_del(cur);
                    item = (struct payload_cache_item *)cur;
                    kfree(item);
                } else {
                    dms_printk(LOG_LVL_WARN, "DMS-CLIENTNODE WARN: %s "
                            "payload list broken for volume id=%llu\n",
                            __func__, v->volid);
                    break;
                }
            }
        }
        mutex_unlock(&v->cache_lock);
    }
}

void payload_item_release_by_id(unsigned long long volid)
{
    struct payload_cache_volume *v = payload_volume_find(volid);

    payload_item_release_by_volume(v);
}


void payload_volume_remove_by_id(unsigned long long volid)
{
    struct hlist_head *head;
    struct hlist_node *node;
    struct payload_cache_volume *v = NULL, *match = NULL;

    // TODO better hash function needed here.
    // TODO % doesn't work on 64-bit integer, change back!
    head = &payload_vol_table[((unsigned long)volid) % CACHE_POOL_HSIZE];

    write_lock(&payload_vol_table_lock);
    //TODO : Change to hlist_for_each_entry_safe
    hlist_for_each_entry(v, node, head, hlist) {
        if (v->volid == volid) {
            match = v;
        }

        break;
    }

    if (likely(match != NULL && match->volid == volid)) {
        hlist_del(&(match->hlist));
        write_unlock(&payload_vol_table_lock);
        kfree(match);
        match = NULL;
    } else {
        write_unlock(&payload_vol_table_lock);
    }
}

void payload_volume_remove_all(void)
{
    int i;
    struct hlist_head *head;
    struct hlist_node *node, *tmp;
    struct payload_cache_volume *v = NULL;

    dms_printk(LOG_LVL_DEBUG, "%s, getting table lock\n", __func__);
    write_lock(&payload_vol_table_lock);
    dms_printk(LOG_LVL_DEBUG, "%s, got table lock\n", __func__);

    for (i = 0; i < CACHE_POOL_HSIZE; ++i) {
        head = &payload_vol_table[i];
        hlist_for_each_entry_safe(v, node, tmp, head, hlist) {
            hlist_del(&v->hlist);
            payload_item_release_by_volume(v);
            free_payload_volume(v);
        }
    }

    write_unlock(&payload_vol_table_lock);
    dms_printk(LOG_LVL_DEBUG, "%s, release lock & leaving\n", __func__);
}


int payload_volume_release(unsigned long long volumeID)
{
    payload_item_release_by_id(volumeID);
    payload_volume_remove_by_id(volumeID);
    return 0;
}

int add_entry_to_cache(unsigned long long volid, unsigned long long sector,
                        char *payload)
{
    payload_cache_volume_t *v;
    payload_cache_item_t *match, *item;
    struct list_head *lhead, *cur, *next;

    v = payload_volume_find(volid);
    if (v == NULL) {
        dms_printk(LOG_LVL_DEBUG, "%s: cannot find volume "
                "for id=%llu, create 1\n", __func__, volid);
        if (payload_volume_create(volid) != 0) {
            return -ENOMEM;
        } else {
            v = payload_volume_find(volid);
            if (v == NULL) {
                return -ENOMEM;
            }
        }
    }

    match = NULL;
    lhead = &v->payload_list[sector & PAYLOAD_HASH_MOD_NUM];
    mutex_lock(&v->cache_lock);

    list_for_each_safe (cur, next, lhead) {
        if (cur != NULL) {
            item = (struct payload_cache_item *)cur;
            if (item->sector == sector) {
                match = item;
                break;
            }
        } else {
            dms_printk(LOG_LVL_WARN, "DMS-CLIENTNODE WARN: %s payload "
                    "list broken for volume id=%llu\n", __func__, volid);
            break;
        }
    }

    if (match == NULL) {
        match = kmalloc(sizeof(struct payload_cache_item), GFP_KERNEL);
        match->sector = sector;
        INIT_LIST_HEAD(&match->list);
        list_add_tail(&match->list, lhead);
    }

    memcpy(match->payloads, payload, 512);
    mutex_unlock(&v->cache_lock);

    return 0;
}

int query_payload_in_cache(unsigned long long volid, unsigned long long sector,
                            char *payload)
{
    payload_cache_volume_t * v;
    payload_cache_item_t *match, *item;
    struct list_head *lhead, *cur, *next;

    v = payload_volume_find(volid);
    dms_printk(LOG_LVL_DEBUG, "%s: query volid=%llu "
            "sector=%llu to payload cache\n", __func__, volid, sector);
    if (v == NULL) {
        dms_printk(LOG_LVL_DEBUG, "%s: cannot found volume "
                "volid=%llu\n", __func__, volid);
        return -ENOMEM;
    }

    match = NULL;
    lhead = &v->payload_list[sector & PAYLOAD_HASH_MOD_NUM];
    mutex_lock(&v->cache_lock);
    list_for_each_safe (cur, next, lhead) {
        if (cur != NULL) {
            item = (struct payload_cache_item *)cur;
            if (item->sector == sector) {
                match = item;
                break;
            }
        } else {
            dms_printk(LOG_LVL_WARN, "DMS-CLIENTNODE WARN: %s payload "
                    "list broken for volume id=%llu\n", __func__, volid);
            break;
        }
    }

    if(match == NULL) {
        mutex_unlock(&v->cache_lock);
        return -ENOMEM;
    }

    memcpy(payload, match->payloads, sizeof(char)*512);
    mutex_unlock(&v->cache_lock);
    return 0;
}

#endif
