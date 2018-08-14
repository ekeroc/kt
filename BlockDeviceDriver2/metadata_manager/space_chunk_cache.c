/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * space_chunk_cache.c
 *
 * A caching system for querying metadata that not yet be flush to namenode
 */

#include <linux/version.h>
#include <linux/kthread.h>
#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/dms_kernel_version.h"
#include "../common/discoC_mem_manager.h"
#include "../lib/dms-radix-tree.h"
#include "metadata_manager.h"

#ifdef DISCO_PREALLOC_SUPPORT

#include "space_chunk_fsm.h"
#include "space_chunk.h"
#include "space_manager_export_api.h"
#include "space_chunk_cache.h"
#include "space_chunk_cache_private.h"

static void _smcache_free_bucket (sm_cache_item_t *item)
{
    discoC_mem_free(item->user_data);
    discoC_mem_free(item);
}

static sm_cache_item_t *_smcache_alloc_bucket (uint64_t user_key)
{
    sm_cache_item_t *citem;

    do {
        citem = discoC_mem_alloc(sizeof(sm_cache_item_t), GFP_NOWAIT);
        if (IS_ERR_OR_NULL(citem)) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN sm cache fail alloc bucket header\n");
            citem = NULL;
            break;
        }

        citem->user_data = discoC_mem_alloc(MEM_BUCKET_SIZE, GFP_NOWAIT);
        if (IS_ERR_OR_NULL(citem->user_data)) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN sm cache fail alloc bucket\n");
            _smcache_free_bucket(citem);
            citem = NULL;
            break;
        }

        memset(citem->user_data, 0, MEM_BUCKET_SIZE);
        INIT_LIST_HEAD(&citem->entry_lru);
        citem->user_key = user_key;
        citem->num_items = 0;
    } while (0);

    return citem;
}

static sm_cache_item_t *_smcache_query_bucket_nolock (sm_cache_pool_t *cpool,
        uint64_t bk_idx)
{
    void *tree_leaf_bk[1];
    sm_cache_item_t *citem;
    uint32_t ret;

    memset(tree_leaf_bk, 0, sizeof(void *)*1);

    ret = dms_radix_tree_gang_lookup(&cpool->cache_pool, tree_leaf_bk, bk_idx, 1);
    if (ret == 0) {
        citem = NULL;
    } else {
        citem = (sm_cache_item_t *)tree_leaf_bk[0];
        if (citem->user_key != bk_idx) {
            /* radix-tree's bug */
            citem = NULL;
        }
    }

    return citem;
}

//TODO: we should reutrn number of items added. It will make our unit test more complete
int32_t smcache_add_aschunk (sm_cache_pool_t *cpool, as_chunk_t *chunk)
{
    sm_cache_item_t *citem, *old_item;
    uint64_t lb_query, bk_idx, offset, *offset_loc;
    int32_t ret, remain_len, len, rlen_bk, i;

    remain_len = chunk->lb_len;
    lb_query = chunk->lbid_s;
    ret = 0;

    down_write(&cpool->cache_plock);

    while (remain_len > 0) {
        bk_idx = SM_GET_CACHEKEY_LBID(lb_query);

        citem = _smcache_query_bucket_nolock(cpool, bk_idx);
        if (IS_ERR_OR_NULL(citem)) {
            citem = _smcache_alloc_bucket(bk_idx);
            if (unlikely(IS_ERR_OR_NULL(citem))) {
                ret = -ENOMEM;
                break;
            }
            dms_radix_tree_insert(&cpool->cache_pool, bk_idx,
                    (void *)citem, (void *)&old_item);
            //if (unlikely(IS_ERR_OR_NULL(old_item) == false)) {
            //    dms_printk(LOG_LVL_WARN,
            //            "DMSC WARN smcache add new item with old exist\n");
            //}
            cpool->num_cache_buckets++;
            list_add_tail(&citem->entry_lru, &cpool->cache_lru);
        }

        offset = (lb_query & BUCKET_MOD_VAL);
        rlen_bk = (uint64_t)NUM_ENTRY_BUCKET - offset;
        len = rlen_bk > remain_len ? remain_len : rlen_bk;

        offset_loc = (uint64_t *)citem->user_data;
        offset_loc += offset;
        for (i = 0; i < len; i++) {
            if (likely(*offset_loc == 0L)) {
                citem->num_items++;
                cpool->num_cache_items++;
            }

            *offset_loc = (uint64_t)chunk;
            offset_loc++;
        }

        lb_query += len;
        remain_len -= len;
    }

    up_write(&cpool->cache_plock);

    return ret;
}

//TODO: we should return number of items rm. It will make our unit test more complete
int32_t smcache_rm_aschunk (sm_cache_pool_t *cpool, uint64_t lb_s, uint32_t lb_len)
{
    sm_cache_item_t *citem;
    uint64_t lb_query, bk_idx, offset, *offset_loc;
    int32_t ret, remain_len, len, rlen_bk, i;

    remain_len = lb_len;
    lb_query = lb_s;
    ret = 0;

    down_write(&cpool->cache_plock);

    while (remain_len > 0) {
        bk_idx = SM_GET_CACHEKEY_LBID(lb_query);

        offset = (lb_query & BUCKET_MOD_VAL);
        rlen_bk = (uint64_t)NUM_ENTRY_BUCKET - offset;
        len = rlen_bk > remain_len ? remain_len : rlen_bk;

        remain_len -= len;
        lb_query += len;

        citem = _smcache_query_bucket_nolock(cpool, bk_idx);
        if (IS_ERR_OR_NULL(citem)) {
            continue;
        }

        offset_loc = (uint64_t *)citem->user_data;
        offset_loc += offset;
        for (i = 0; i < len; i++) {
            if (likely(*offset_loc)) {
                citem->num_items--;
                cpool->num_cache_items--;
            }

            *offset_loc = 0L;
            offset_loc++;
        }

        if (citem->num_items == 0) {
            dms_radix_tree_delete(&cpool->cache_pool, bk_idx);
            cpool->num_cache_buckets--;
            list_del(&citem->entry_lru);
            _smcache_free_bucket(citem);
        }
    }

    up_write(&cpool->cache_plock);

    return ret;
}

static int32_t _smcache_find_aschunk_nolock (sm_cache_pool_t *cpool,
        uint64_t lb_s, uint32_t lb_len, as_chunk_t **chunk_bk)
{
    sm_cache_item_t *citem;
    uint64_t lb_query, bk_idx, offset, *offset_loc;
    int32_t ret, remain_len, len, rlen_bk, i, idx;

    lb_query = lb_s;
    remain_len = lb_len;
    idx = 0;
    ret = 0;

    while (remain_len > 0) {
        bk_idx = SM_GET_CACHEKEY_LBID(lb_query);

        offset = (lb_query & BUCKET_MOD_VAL);
        rlen_bk = (uint64_t)NUM_ENTRY_BUCKET - offset;
        len = rlen_bk > remain_len ? remain_len : rlen_bk;

        remain_len -= len;
        lb_query += len;

        citem = _smcache_query_bucket_nolock(cpool, bk_idx);
        if (IS_ERR_OR_NULL(citem)) {
            idx += len;
            continue;
        }

        offset_loc = (uint64_t *)citem->user_data;
        offset_loc += offset;
        for (i = 0; i < len; i++) {
            if (*offset_loc != 0L) {
                ret++;
            }
            chunk_bk[idx] = (as_chunk_t *)(*offset_loc);
            idx++;
            offset_loc++;
        }
    }

    return ret;
}

int32_t smcache_find_aschunk (sm_cache_pool_t *cpool,
        uint64_t lb_s, uint32_t lb_len, as_chunk_t **chunk_bk)
{
    int32_t ret;

    memset(chunk_bk, 0, sizeof(as_chunk_t *)*lb_len);

    down_read(&cpool->cache_plock);
    ret = _smcache_find_aschunk_nolock(cpool, lb_s, lb_len, chunk_bk);
    up_read(&cpool->cache_plock);

    return ret;
}

static int32_t _get_chitstat_max_seglen (as_chunk_t **chunk_bk,
        int32_t max_len, bool *md_ch)
{
    int32_t i, cnt;

    cnt = 1;

    *md_ch = true;
    if (IS_ERR_OR_NULL(chunk_bk[0])) {
        *md_ch = false;
    }

    for (i = 1; i < max_len; i++) {
        if (IS_ERR_OR_NULL(chunk_bk[i]) && (*md_ch)) {
            break;
        }

        if (!IS_ERR_OR_NULL(chunk_bk[i]) && !(*md_ch)) {
            break;
        }

        cnt++;
    }

    return cnt;
}

/*
 * TODO: should check whether DN same state - normal / io err ... etc
 * find max cont LB len which has same DN ip addr
 */
static int32_t _get_contlen_aschunk (uint64_t start_LB, as_chunk_t **arr_chunk,
        uint32_t len)
{
    as_chunk_t *pSrc, *pDest;
    int32_t i, cn_cont;

    pSrc = arr_chunk[0];

    cn_cont = 1;
    while (cn_cont < len) {
        pDest = arr_chunk[cn_cont];

        if (pSrc->num_replica != pDest->num_replica) {
            return cn_cont;
        }

        for (i = 0; i < pDest->num_replica; i++) {
            if (pDest->chunk_DNLoc[i].ipaddr != pSrc->chunk_DNLoc[i].ipaddr) {
                return cn_cont;
            }

            if (pDest->chunk_DNLoc[i].port != pSrc->chunk_DNLoc[i].port) {
                return cn_cont;
            }
        }

        cn_cont++;
    }

    return cn_cont;
}

/*
 * create metadata item
 */
static metaD_item_t *_smcache_create_MDItem (uint64_t startLB, int32_t len,
        as_chunk_t **arr_chunk)
{
    metaD_item_t *myMDItem;
    as_chunk_t *mci_tmp, *mci_head;
    mcache_dnloc_t *dnInfo_cache;
    uint64_t lb_s;
    int32_t i, j, addr, rbid, offset;

    myMDItem = MData_alloc_MDItem();
    if (unlikely(IS_ERR_OR_NULL(myMDItem))) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN sm cache fail alloc MDItem\n");
        DMS_WARN_ON(true);
        return NULL;
    }
    memset(myMDItem, 0, sizeof(metaD_item_t));

    addr = 0;
    lb_s = startLB;
    mci_head = arr_chunk[addr];
    MData_init_MDItem(myMDItem, lb_s, len, mci_head->num_replica,
            MData_VALID, 0, 0, true);
    for (i = 0; i < len; i++) {
        //set HBID
        mci_tmp = arr_chunk[addr + i];
        myMDItem->arr_HBID[i] = mci_tmp->hbid_s + (lb_s - mci_tmp->lbid_s);
        lb_s++;
    }

    for (j = 0; j < mci_head->num_replica; j++) {
        dnInfo_cache = &(mci_head->chunk_DNLoc[j]);
        MData_init_DNLoc(&myMDItem->udata_dnLoc[j],
                dnInfo_cache->ipaddr, dnInfo_cache->port, len);

        lb_s = startLB;
        for (i = 0; i < len; i++) {
            mci_tmp = arr_chunk[addr + i];
            rbid = mci_tmp->chunk_DNLoc[j].rbid;
            offset = mci_tmp->chunk_DNLoc[j].offset + (lb_s - mci_tmp->lbid_s);
            myMDItem->udata_dnLoc[j].rbid_len_offset[i] =
                    compose_triple(rbid, 1, offset);
            lb_s++;
        }
    }

    return myMDItem;
}

static int32_t _smcache_create_metadata (metadata_t *my_MData, uint64_t cont_len,
        as_chunk_t **arr_chunk)
{
    uint64_t start_LB;
    int32_t cn_cont, cn_used, ret, len, addr;

    ret = 0;

    //create metadata array
    my_MData->array_metadata = (metaD_item_t **)
            discoC_mem_alloc(sizeof(metaD_item_t *)*cont_len,
                    GFP_NOWAIT | GFP_ATOMIC);
    if (unlikely(IS_ERR_OR_NULL(my_MData->array_metadata))) {
        dms_printk( LOG_LVL_WARN, "DMSC WARN smcache fail alloc MD array\n");
        DMS_WARN_ON(true);
        return -ENOMEM;
    }

    my_MData->num_MD_items = cont_len; //default create a metadata item for each LB
    my_MData->num_invalMD_items = 0;
    len = cont_len;
    start_LB = my_MData->usr_ioaddr.lbid_start;
    cn_cont = 0;
    cn_used = 0;
    addr = 0;

    while (len > 0) {
        //find max len of aschunk that all as chunk has same DN
        cn_cont = _get_contlen_aschunk(start_LB, &(arr_chunk[addr]), len);

        //create metadata item for continuous LB which has same DN
        my_MData->array_metadata[cn_used] =
                _smcache_create_MDItem(start_LB, cn_cont, &(arr_chunk[addr]));
        if (unlikely(IS_ERR_OR_NULL(my_MData->array_metadata[cn_used]))) {
            dms_printk(LOG_LVL_WARN,
                        "DMSC WANR fail create MDItem for MData[%d]\n", cn_used);
            DMS_WARN_ON(true);

            my_MData->num_MD_items = cn_used;
            free_metadata(my_MData);
            return -ENOMEM;
        }

        cn_used++;
        addr += cn_cont;
        len -= cn_cont;
        start_LB += cn_cont;
    }

    my_MData->num_MD_items = cn_used;

    return ret;
}

//retLen = 1: means there are no continuous triplet, only first one itself
//retLen = 2: means there are 1 continuous triplet, only first one and next one can merge....
static int32_t _smcache_get_contTripletLen (uint64_t *triplet, int32_t num_of_triplet,
                          int32_t *accum_len)
{
    int32_t i, retLen;
    int32_t rbid, len, offset;
    int32_t rbid_n, len_n, offset_n;

    *accum_len = 0;
    retLen = 1;

    if (num_of_triplet == 1) {
        return retLen;
    }

    decompose_triple(triplet[0], &rbid, &len, &offset);
    *accum_len = len;

    for (i = 1; i < num_of_triplet; i++) {
        decompose_triple(triplet[i], &rbid_n, &len_n, &offset_n);

        if (rbid_n != rbid) {
            break;
        }

        if (offset + *accum_len != offset_n) {
            break;
        }

        *accum_len = *accum_len + len_n;
        retLen++;
    }

    return retLen;
}

//TODO: refactor this function : for -> if -> for -> while
//      Move to metadata manager
static void _smcache_merge_triplet (metadata_t *my_MData)
{
    metaD_item_t *myMD_item;
    int32_t i, j, start;
    int32_t rbid_s, len_s, offset_s;
    int32_t store_index, contLen, accum_len;

    store_index = 0;
    contLen = 0;
    accum_len = 0;

    for (i = 0; i < my_MData->num_MD_items; i++) {
        myMD_item = my_MData->array_metadata[i];
        if (myMD_item->mdata_stat != MData_VALID) {
            continue;
        }

        for (j = 0; j < myMD_item->num_dnLoc; j++) {
            start = 0;
            store_index = 0;

            while (start < myMD_item->udata_dnLoc[j].num_phyLocs) {
                decompose_triple(myMD_item->udata_dnLoc[j].rbid_len_offset[start],
                        &rbid_s, &len_s, &offset_s);

                accum_len = 0;
                contLen = _smcache_get_contTripletLen(&myMD_item->udata_dnLoc[j].rbid_len_offset[start],
                        myMD_item->udata_dnLoc[j].num_phyLocs - start,
                        &accum_len);
                if (contLen > 1) {
                    len_s = accum_len;

                    myMD_item->udata_dnLoc[j].rbid_len_offset[store_index] =
                            compose_triple( rbid_s, len_s, offset_s);
                } else {
                    myMD_item->udata_dnLoc[j].rbid_len_offset[store_index] =
                            myMD_item->udata_dnLoc[j].rbid_len_offset[start];
                }

                start = start + contLen;
                store_index++;
            }

            myMD_item->udata_dnLoc[j].num_phyLocs = store_index;
        }
    }
}

/*
 * caller should guarantee as_chunk in radix tree won't be free
 * Return: number of bucket has metadata segment
 */
int32_t smcache_query_MData (sm_cache_pool_t *cpool, uint32_t vol_id,
        uint64_t lb_s, uint32_t lb_len, metadata_t *arr_MD_bk)
{
    as_chunk_t *chunk_bk[MAX_LBS_PER_IOR];
    metadata_t *mdata;
    uint64_t lb_start;
    int32_t ret_bk, remain_len, seg_len, idx_s, num_mdItem, num_hit;
    bool mdata_ch;

    ret_bk = 0;
    //for each LBID, find corresponding aschunk
    num_hit = smcache_find_aschunk(cpool, lb_s, lb_len, chunk_bk);
    if (num_hit == 0) {
        ret_bk = 1;
        mdata = &(arr_MD_bk[0]);
        mdata->num_MD_items = 0;
        mdata->usr_ioaddr.volumeID = vol_id;
        mdata->usr_ioaddr.lbid_start = lb_s;
        mdata->usr_ioaddr.lbid_len = lb_len;
        return ret_bk;
    }

    remain_len = lb_len;
    idx_s = 0;
    num_mdItem = 0;
    lb_start = lb_s;

    while (remain_len > 0) {
        //find a segment of LBID which all LB has aschunk or all no aschunk
        seg_len = _get_chitstat_max_seglen(&(chunk_bk[idx_s]), remain_len,
                &mdata_ch);
        mdata = &(arr_MD_bk[num_mdItem]);
        mdata->usr_ioaddr.volumeID = vol_id;
        mdata->usr_ioaddr.lbid_start = lb_start;
        mdata->usr_ioaddr.lbid_len = seg_len;

        ret_bk++;

        if (mdata_ch) {
            if (_smcache_create_metadata(mdata, seg_len, &(chunk_bk[idx_s]))) {
                dms_printk(LOG_LVL_WARN,
                        "DMSC WARN fail gen create metadata vol %u\n", vol_id);
                DMS_WARN_ON(true);
                continue;
            }

            _smcache_merge_triplet(mdata);
        } else {
            mdata->num_MD_items = 0;
        }

        num_mdItem++;
        idx_s += seg_len;
        lb_start += seg_len;
        remain_len -= seg_len;
    }

    return ret_bk;
}

int32_t smcache_init_cache (sm_cache_pool_t *cpool)
{
    int32_t ret;

    ret = 0;

    init_rwsem(&cpool->cache_plock);
    cpool->num_cache_buckets = 0;
    cpool->num_cache_items = 0;
    DMS_INIT_RADIX_TREE(&cpool->cache_pool, GFP_NOWAIT | GFP_ATOMIC);
    INIT_LIST_HEAD(&cpool->cache_lru);

    return ret;
}

int32_t smcache_rel_cache (sm_cache_pool_t *cpool)
{
    sm_cache_item_t *cur, *next;
    int32_t ret;

    ret = 0;

    down_write(&cpool->cache_plock);

    list_for_each_entry_safe (cur, next, &cpool->cache_lru, entry_lru) {
        list_del(&cur->entry_lru);
        dms_radix_tree_delete(&cpool->cache_pool, cur->user_key);
        cpool->num_cache_buckets--;
        cpool->num_cache_items -= cur->num_items;
        _smcache_free_bucket(cur);
        ret++;
    }
    up_write(&cpool->cache_plock);

    return ret;
}
#endif
