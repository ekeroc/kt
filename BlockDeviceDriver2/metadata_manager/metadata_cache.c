/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * metadata_cache.c
 *
 */
#include <linux/version.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/blkdev.h>
#include "../common/discoC_sys_def.h"
#include "../common/dms_kernel_version.h"
#include "../common/common_util.h"
#include "../common/discoC_mem_manager.h"
#include "../common/sys_mem_monitor.h"
#include "metadata_manager.h"
#include "../common/cache.h"
#include "metadata_cache.h"
#include "../config/dmsc_config.h"
#include "../connection_manager/conn_manager_export.h"
#include "../discoNN_client/discoC_NNC_Manager_api.h"

static void free_mdata_cache_item (void *data)
{
    mcache_item_t *item;

    if (!IS_ERR_OR_NULL(data)) {
        item = (mcache_item_t *)data;
        discoC_mem_free(item);
        data = NULL;
    } else {
        dms_printk(LOG_LVL_WARN, "DMSC WARN free null metadata\n");
    }
}

static int32_t _get_max_seg_len (mcache_item_t **arr_MD_ptr,
        int32_t max_len, bool *md_ch)
{
    int32_t i, cnt;

    cnt = 1;

    *md_ch = true;
    if (IS_ERR_OR_NULL(arr_MD_ptr[0])) {
        *md_ch = false;
    }

    for (i = 1; i < max_len; i++) {
        if (IS_ERR_OR_NULL(arr_MD_ptr[i]) && (*md_ch)) {
            break;
        }

        if (!IS_ERR_OR_NULL(arr_MD_ptr[i]) && !(*md_ch)) {
            break;
        }

        cnt++;
    }

    return cnt;
}

//retLen = 1: means there are no continuous triplet, only first one itself
//retLen = 2: means there are 1 continuous triplet, only first one and next one can merge....
static int32_t getContTripletLength (uint64_t *triplet, int32_t num_of_triplet,
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
static void merge_lr_triplet (metadata_t *my_MData)
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
                contLen = getContTripletLength(&myMD_item->udata_dnLoc[j].rbid_len_offset[start],
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

//TODO: we need to handle dn_location out of order issue. ex: LR1:1,2,3 LR2:2,1,3
static int32_t GetContLenForSameDN (int32_t addr, int32_t len,
        mcache_item_t **metaData)
{
    mcache_item_t *pSrc, *pDest;
    int32_t i, cn_cont;

    pSrc = metaData[addr];

    cn_cont = 1;
    while (cn_cont < len) {
        pDest = metaData[addr + cn_cont];

        if (pSrc->num_dnLoc != pDest->num_dnLoc) {
            return cn_cont;
        }

        for (i = 0; i < pDest->num_dnLoc; i++) {
            if (pDest->udata_dnLoc[i].ipaddr != pSrc->udata_dnLoc[i].ipaddr) {
                return cn_cont;
            }

            if (pDest->udata_dnLoc[i].port != pSrc->udata_dnLoc[i].port) {
                return cn_cont;
            }
        }

        cn_cont++;
    }

    return cn_cont;
}

static metaD_item_t *_create_MDItem (uint64_t startLB, int32_t addr, int32_t len,
        mcache_item_t **metaData)
{
    metaD_item_t *myMDItem;
    mcache_item_t *mci_tmp, *mci_head;
    mcache_dnloc_t *dnInfo_cache;
    int32_t i, j;

    myMDItem = MData_alloc_MDItem();
    if (unlikely(IS_ERR_OR_NULL(myMDItem))) {
        dms_printk(LOG_LVL_WARN,
                    "DMSC WARN fail alloc MDItem when cache operation\n");
        DMS_WARN_ON(true);
        return NULL;
    }
    memset(myMDItem, 0, sizeof(metaD_item_t));

    mci_head = metaData[addr];
    MData_init_MDItem(myMDItem, startLB, len, mci_head->num_dnLoc,
            MData_VALID, 0, 0, false);
    for (i = 0; i < len; i++) {
        mci_tmp = metaData[addr + i];
        myMDItem->arr_HBID[i] = mci_tmp->HBID;
    }

    for (j = 0; j < mci_head->num_dnLoc; j++) {
        dnInfo_cache = &(mci_head->udata_dnLoc[j]);
        MData_init_DNLoc(&myMDItem->udata_dnLoc[j],
                dnInfo_cache->ipaddr, dnInfo_cache->port, len);

        for (i = 0; i < len; i++) {
            mci_tmp = metaData[addr + i];
            myMDItem->udata_dnLoc[j].rbid_len_offset[i] = compose_triple(
                    mci_tmp->udata_dnLoc[j].rbid, 1, mci_tmp->udata_dnLoc[j].offset);
        }
    }

    return myMDItem;
}

static int32_t _create_metadata (metadata_t *my_MData, uint64_t len,
        mcache_item_t **metaData, uint64_t addr)
{
    uint64_t start_LB;
    int32_t cn_cont, cn_used, ret;

    ret = 0;

    my_MData->array_metadata = (metaD_item_t **)discoC_mem_alloc(sizeof(metaD_item_t *)*len,
            GFP_NOWAIT | GFP_ATOMIC);
    if (unlikely(IS_ERR_OR_NULL(my_MData->array_metadata))) {
        dms_printk( LOG_LVL_WARN, "DMSC WARN Cannot allocate memory for lr\n");
        DMS_WARN_ON(true);
        return -ENOMEM;
    }

    my_MData->num_MD_items = len;
    my_MData->num_invalMD_items = 0;
    start_LB = my_MData->usr_ioaddr.lbid_start;
    cn_cont = 0;
    cn_used = 0;

    while (len > 0) {
        cn_cont = GetContLenForSameDN(addr, len, metaData);
        my_MData->array_metadata[cn_used] =
                _create_MDItem(start_LB, addr, cn_cont, metaData);
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

int32_t query_volume_MDCache (metadata_t *arr_MD_bk, uint32_t volid,
        uint64_t lbid_s, uint32_t lbid_len, int16_t req_priority,
        bool query_cache)
{
    mcache_item_t *metaData[MAX_LBS_PER_IOR];
    struct tree_leaf *tree_leaf_bk[MAX_LBS_PER_IOR];
    metadata_t *mdItem;
    int32_t i, ret, num_mdItem, lstart_lb, remain_len, seg_len;
    int32_t idx_s;
    bool mdata_ch;

    memset(metaData, 0, sizeof(mcache_item_t *)*MAX_LBS_PER_IOR);
    memset(tree_leaf_bk, 0, sizeof(struct tree_leaf *)*MAX_LBS_PER_IOR);

    if (query_cache) {
        ret = cache_find(volid, lbid_s, lbid_len, (void **)(tree_leaf_bk));
    } else {
        arr_MD_bk[0].usr_ioaddr.volumeID = volid;
        arr_MD_bk[0].usr_ioaddr.lbid_start = lbid_s;
        arr_MD_bk[0].usr_ioaddr.lbid_len = lbid_len;
        arr_MD_bk[0].num_MD_items = 0;

        return 1;
    }

    for (i = 0; i < lbid_len; i++) {
        if (IS_ERR_OR_NULL(tree_leaf_bk[i]) == false) {
            metaData[i] = (mcache_item_t *)tree_leaf_bk[i]->data;
        }
    }

    num_mdItem = 0;
    lstart_lb = lbid_s;
    remain_len = lbid_len;
    idx_s = 0;

    while (remain_len > 0) {
        seg_len = _get_max_seg_len(&(metaData[idx_s]), remain_len, &mdata_ch);
        mdItem = &(arr_MD_bk[num_mdItem]);
        mdItem->usr_ioaddr.volumeID = volid;
        mdItem->usr_ioaddr.lbid_start = lstart_lb;
        mdItem->usr_ioaddr.lbid_len = seg_len;

        if (mdata_ch) {
            if (_create_metadata(mdItem, seg_len, metaData, idx_s)) {
                dms_printk(LOG_LVL_WARN,
                        "DMSC WARN fail gen create metadata vol %u\n", volid);
                DMS_WARN_ON(true);
                continue;
            }

            merge_lr_triplet(mdItem);
        } else {
            mdItem->num_MD_items = 0;
        }

        lstart_lb += seg_len;
        idx_s += seg_len;
        remain_len -= seg_len;
        num_mdItem++;
    }

    if (ret > 0) {
        cache_put(volid, lbid_s, lbid_len, (void **)(tree_leaf_bk));
    }

    return num_mdItem;
}

static int32_t _alloc_chItem_arr (mcache_item_t **arr_chItem, int32_t num_items)
{
    int32_t i, j, ret;

    ret = 0;

    for (i = 0; i < num_items; i++) {
        arr_chItem[i] = (mcache_item_t *)
                discoC_mem_alloc(sizeof(mcache_item_t), GFP_NOWAIT | GFP_ATOMIC);

        if (unlikely(IS_ERR_OR_NULL(arr_chItem[i]))) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN fail alloc mem for "
                    "m_cache_item_arr[%d]\n", i);

            for (j = 0; j < i; j++) {
                discoC_mem_free(arr_chItem[j]);
                arr_chItem[j] = NULL;
            }
            ret = -ENOMEM;
            break;
        }

        memset(arr_chItem[i], 0, sizeof(mcache_item_t));
    }

    return ret;
}

static void _split_metadata (metaD_item_t *myMD_item, mcache_item_t **m_cache_item_arr)
{
    mcache_item_t *citem;
    mcache_dnloc_t *dnLoc;
    metaD_DNLoc_t *mdata_dnLoc;
    int32_t i, arr_idx, m, n, rbid, len, offset, prev_len;

    arr_idx = 0;

    for (i = 0; i < myMD_item->num_hbids; i++) {
        citem = m_cache_item_arr[arr_idx];

        citem->HBID = myMD_item->arr_HBID[i];
        citem->num_dnLoc = myMD_item->num_dnLoc;

        for (m = 0; m < citem->num_dnLoc; m++) {
            dnLoc = &citem->udata_dnLoc[m];
            mdata_dnLoc = &myMD_item->udata_dnLoc[m];

            dnLoc->ipaddr = mdata_dnLoc->ipaddr;
            dnLoc->port = mdata_dnLoc->port;

            prev_len = 0;

            for (n = 0; n < mdata_dnLoc->num_phyLocs; n++) {
                decompose_triple(mdata_dnLoc->rbid_len_offset[n], &rbid, &len, &offset);

                if (i < len + prev_len) {
                    dnLoc->rbid = rbid;
                    dnLoc->offset = offset + i - prev_len;
                    break;
                } else {
                    prev_len += len;
                }
            }
        }

        arr_idx++;
    }
}

int32_t _update_mdcache (metadata_t *my_MData, uint32_t vol_id,
        uint64_t lb_s, uint32_t lb_len)
{
    mcache_item_t *m_cache_item_arr[MAX_LBS_PER_IOR];
    metaD_item_t *myMD_item;
    int32_t i, j, update_ret;
    uint64_t start_lb_index;

    start_lb_index = lb_s;

    for (i = 0; i < my_MData->num_MD_items; i++) {
        myMD_item = my_MData->array_metadata[i];

        if (myMD_item->mdata_stat != MData_VALID) {
            inval_volume_MDCache_LBID(vol_id, start_lb_index, myMD_item->num_hbids);
            start_lb_index = start_lb_index + myMD_item->num_hbids;
            continue;
        }

#ifdef DISCO_ERC_SUPPORT
        if (myMD_item->blk_md_type != MDPROT_REP) {
            inval_volume_MDCache_LBID(vol_id, start_lb_index,
                    myMD_item->num_hbids);
            start_lb_index = start_lb_index + myMD_item->num_hbids;
            continue;
        }
#endif

        if (_alloc_chItem_arr(m_cache_item_arr, myMD_item->num_hbids)) {
            inval_volume_MDCache_LBID(vol_id, start_lb_index, myMD_item->num_hbids);
            start_lb_index = start_lb_index + myMD_item->num_hbids;
            continue;
        }

        _split_metadata(myMD_item, m_cache_item_arr);

        update_ret = cache_update(vol_id, start_lb_index, myMD_item->num_hbids,
                (void **)m_cache_item_arr);
        if (update_ret < 0) {
            inval_volume_MDCache_LBID(vol_id, start_lb_index, myMD_item->num_hbids);
        }

        for (j = 0; j < myMD_item->num_hbids; j++) {
            //Lego: in cache_update, m_cache_iterm_arr doesn't be release

            if (update_ret < 0) {
                dms_printk(LOG_LVL_INFO,
                        "DMSC INFO fail add cache item ret %d (%u %llu)\n",
                        update_ret, vol_id, start_lb_index + j);
                discoC_mem_free(m_cache_item_arr[j]);
            }

            m_cache_item_arr[j] = NULL;
        }

        start_lb_index = start_lb_index + myMD_item->num_hbids;
    }

    return 0;
}

int32_t update_volume_MDCache (metadata_t *my_MData,
        uint32_t volid, uint64_t lbid_s, uint32_t lbid_len, bool enable_mdcache)
{
    if (unlikely(IS_ERR_OR_NULL(my_MData))) {
        return -EINVAL;
    }

    if (enable_mdcache == false) {
        inval_volume_MDCache_LBID(volid, lbid_s, lbid_len);
        return -EPERM;
    }

    if (is_sys_free_mem_low()) {
        inval_volume_MDCache_LBID(volid, lbid_s, lbid_len);
        return -ENOMEM;
    }

    return _update_mdcache(my_MData, volid, lbid_s, lbid_len);
}

int32_t inval_volume_MDCache_HBID (uint64_t hbid)
{
    return cache_invalidate(INVAL_CACHE_HB, 0, hbid, 1, NULL, NULL);
}

int32_t inval_volume_MDCache_LBID (uint64_t volid,
        uint64_t start_LB, uint32_t LB_cnt)
{
    return cache_invalidate(INVAL_CACHE_LB, volid, start_LB, LB_cnt, NULL, NULL);
}

int32_t inval_volume_MDCache_volID (uint64_t volid)
{
    return cache_invalidate(INVAL_CACHE_VOLID, volid, -1, -1, NULL, NULL);
}

static int32_t _is_mditem_has_DN (void *metadata_item,
                                    void *criteria, int32_t *update_flag)
{
    mcache_item_t *item;
    mcache_dnloc_t *cmp_criteria;
    int32_t i, retCode;

    item = (mcache_item_t *)metadata_item;
    retCode = 0;
    *update_flag = 0;

    if (IS_ERR_OR_NULL(item)) {
        return retCode;
    }

    cmp_criteria = (mcache_dnloc_t *)criteria;
    for (i = 0; i < item->num_dnLoc; i++) {
        if (item->udata_dnLoc[i].ipaddr == cmp_criteria->ipaddr &&
                item->udata_dnLoc[i].port == cmp_criteria->port) {
            retCode = 1;
            break;
        }
    }

    return retCode;
}

int32_t inval_volume_MDCache_DN (int8_t *dnName, int32_t dnName_len, int32_t port)
{
    int32_t retCode;
    mcache_dnloc_t cmp_criteria;

    retCode = 0;

    cmp_criteria.ipaddr = ccma_inet_ntoa(dnName);
    cmp_criteria.port = port;
    cmp_criteria.rbid = -1;
    cmp_criteria.offset = -1;
    retCode = cache_invalidate(INVAL_CACHE_COND_CHK, 1, -1, -1,
            _is_mditem_has_DN, (void *)(&cmp_criteria));

    return retCode;
}

int32_t free_volume_MDCache (uint64_t volid)
{
    return cacheMgr_rel_pool(volid);
}

int32_t create_volume_MDCache (uint64_t volid, uint64_t capacity)
{
    return cacheMgr_create_pool(volid, capacity);
}

int32_t init_metadata_cache (void)
{
    return cacheMgr_init_manager(free_mdata_cache_item);
}

void rel_metadata_cache (void)
{
    cacheMgr_rel_manager();
}

