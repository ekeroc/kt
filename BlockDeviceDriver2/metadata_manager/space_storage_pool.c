/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * space_storage_pool.c
 *
 */

#include <linux/version.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/dms_kernel_version.h"
#include "../common/thread_manager.h"
#include "../lib/dms-radix-tree.h"
#include "../common/discoC_mem_manager.h"
#include "metadata_manager.h"

#ifdef DISCO_PREALLOC_SUPPORT
#include "space_chunk_fsm.h"
#include "space_chunk.h"
#include "space_manager_export_api.h"
#include "space_chunk_cache.h"
#include "space_storage_pool.h"
#include "space_manager.h"
#include "space_commit_worker.h"

/*
 * Description: find free space pool in a list without lock
 */
static freespace_pool_t *_fstbl_find_fspool_nolock (struct list_head *lhead,
        uint32_t volumeID)
{
    freespace_pool_t *cur, *next, *fspool;

    fspool = NULL;
    list_for_each_entry_safe (cur, next, lhead, entry_fspooltbl) {
        if (cur->volumeID == volumeID) {
            fspool = cur;
            break;
        }
    }

    return fspool;
}

static void _fstbl_rm_fspool_nolock (fspool_table_t *fstbl,
        freespace_pool_t *fspool)
{
    list_del(&fspool->entry_fspooltbl);
    atomic_dec(&fstbl->num_fsPool);
}

/*
 * Description: remove free space pool from table
 */
static freespace_pool_t *_fstbl_rm_fspool (fspool_table_t *fstbl, uint32_t volumeID)
{
    freespace_pool_t *fspool;
    uint32_t bk_idx;

    bk_idx = volumeID % FREESPACE_POOL_HSIZE;

    down_write(&(fstbl->fspool_tbl_lock[bk_idx]));

    fspool = _fstbl_find_fspool_nolock(&(fstbl->fspool_tbl[bk_idx]), volumeID);
    if (IS_ERR_OR_NULL(fspool) == false) {
        _fstbl_rm_fspool_nolock(fstbl, fspool);
    }

    up_write(&(fstbl->fspool_tbl_lock[bk_idx]));

    return fspool;
}

/*
 * Description: add free space pool to table
 */
static void _fstbl_add_fspool (fspool_table_t *fstbl, freespace_pool_t *fspool)
{
    uint32_t bk_idx;

    bk_idx = fspool->volumeID % FREESPACE_POOL_HSIZE;

    down_write(&(fstbl->fspool_tbl_lock[bk_idx]));

    list_add_tail(&fspool->entry_fspooltbl, &fstbl->fspool_tbl[bk_idx]);
    atomic_inc(&fstbl->num_fsPool);

    up_write(&(fstbl->fspool_tbl_lock[bk_idx]));
}

freespace_pool_t *fspool_create_pool (fspool_table_t *fstbl)
{
    freespace_pool_t *fsPool;

    fsPool = discoC_mem_alloc(sizeof(fspool_table_t), GFP_NOWAIT);

    return fsPool;
}

void fspool_init_pool (fspool_table_t *fstbl, freespace_pool_t *fspool,
        uint32_t volumeID)
{
    fspool->volumeID = volumeID;
    spin_lock_init(&fspool->fsSizeInfo_lock);
    fspool->fsSize_remaining = 0;
    fspool->fsSize_requesting = 0;
    fspool->fsSize_userWant = 0;

    init_rwsem(&fspool->space_plock);
    INIT_LIST_HEAD(&fspool->space_chunk_pool);
    INIT_LIST_HEAD(&fspool->alloc_chunk_pool);
    INIT_LIST_HEAD(&fspool->mdataReq_wait_pool);

    fspool->num_space_chunk = 0;
    fspool->num_alloc_unit = 0;
    fspool->num_waitReq = 0;

    INIT_LIST_HEAD(&fspool->entry_fspooltbl);

    smcache_init_cache(&fspool->cpool);

    _fstbl_add_fspool(fstbl, fspool);
}

static void _fspool_free_pool (freespace_pool_t *fsPool)
{
    fs_chunk_t *cur, *next;
    as_chunk_t *as_cur, *as_next;
    int32_t memPID;
    bool showWarn;

    showWarn = false;

    if (fsPool->num_space_chunk != 0 || fsPool->num_alloc_unit != 0 ||
            fsPool->num_waitReq != 0) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN try to free fspool with "
                "numChunk %u numAU %u numWaitReq %u\n",
                fsPool->num_space_chunk, fsPool->num_alloc_unit,
                fsPool->num_waitReq);
        showWarn = true;
    }

    if (fsPool->fsSize_remaining != 0 || fsPool->fsSize_requesting != 0 ||
            fsPool->fsSize_userWant != 0) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN try to free fspool with "
                "remaining %u requesting %u userWant %u\n",
                fsPool->fsSize_remaining, fsPool->fsSize_requesting,
                fsPool->fsSize_userWant);
        showWarn = true;
    }

    memPID = get_fsChunkMemPoolID();

    down_write(&fsPool->space_plock);
    list_for_each_entry_safe (cur, next, &fsPool->space_chunk_pool,
            entry_spacePool) {
        list_del(&cur->entry_spacePool);
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN linger fsChunk when free fspool volume %llu\n",
                fsPool->volumeID);
        fschunk_show_chunk(cur);

        fschunk_free_chunk(memPID, cur);

        showWarn = true;
    }

    list_for_each_entry_safe (as_cur, as_next, &fsPool->alloc_chunk_pool,
            entry_allocPool) {
        list_del(&as_cur->entry_allocPool);
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN linger asChunk when free fspool volume %llu\n",
                fsPool->volumeID);
        aschunk_show_chunk(as_cur);
        aschunk_free_chunk(get_asChunkMemPoolID(), as_cur);
        showWarn = true;
    }
    up_write(&fsPool->space_plock);
}

int32_t fspool_free_pool (fspool_table_t *fstbl, uint32_t volumeID)
{
    freespace_pool_t *fsPool;
    int32_t ret;

    ret = 0;

    fsPool = _fstbl_rm_fspool(fstbl, volumeID);

    smcache_rel_cache(&fsPool->cpool);

    _fspool_free_pool(fsPool);

    discoC_mem_free(fsPool);

    return ret;
}

freespace_pool_t *fspool_find_pool (fspool_table_t *fstbl, uint32_t volumeID)
{
    freespace_pool_t *fspool;
    uint32_t bk_idx;

    bk_idx = volumeID % FREESPACE_POOL_HSIZE;

    down_read(&(fstbl->fspool_tbl_lock[bk_idx]));

    fspool = _fstbl_find_fspool_nolock(&(fstbl->fspool_tbl[bk_idx]),
            volumeID);

    up_read(&(fstbl->fspool_tbl_lock[bk_idx]));

    return fspool;
}

static void _fspool_add_fschunk (freespace_pool_t *fspool, fs_chunk_t *fsChunk)
{
    down_write(&fspool->space_plock);

    spin_lock(&fspool->fsSizeInfo_lock);

    list_add_tail(&fsChunk->entry_spacePool, &fspool->space_chunk_pool);

    fspool->fsSize_remaining += fsChunk->chunkLen;

    spin_unlock(&fspool->fsSizeInfo_lock);

    fspool->num_space_chunk++;

    up_write(&fspool->space_plock);
}

void fspool_add_fschunk (fspool_table_t *fstbl, uint32_t volumeID,
        fs_chunk_t *fsChunk)
{
    freespace_pool_t *fspool;

    //TODO: fix me, we should gain pool's reference count to prevent from free
    fspool = fspool_find_pool(fstbl, volumeID);
    _fspool_add_fschunk(fspool, fsChunk);
}

static fs_chunk_t *_find_fschunk_fspool_lockless (freespace_pool_t *fspool,
        uint64_t chunkID)
{
    fs_chunk_t *cur, *next, *chunk;

    chunk = NULL;

    list_for_each_entry_safe (cur, next, &fspool->space_chunk_pool,
            entry_spacePool) {
        if (cur->chunkID == chunkID) {
            chunk = cur;
            break;
        }
    }

    return chunk;
}

spool_invalChunk_resp_t fspool_inval_fschunk (fspool_table_t *fstbl,
        uint32_t volumeID, uint64_t chunkID, uint64_t *maxUnuseHBID)
{
    freespace_pool_t *fspool;
    fs_chunk_t *chunk;
    spool_invalChunk_resp_t ret;
    int32_t bk_idx, inval_ret, report_ret;

    ret = SPOOL_INVAL_CHUNK_OK;

    bk_idx = volumeID % FREESPACE_POOL_HSIZE;

    down_read(&(fstbl->fspool_tbl_lock[bk_idx]));

    do {
        fspool = _fstbl_find_fspool_nolock(&(fstbl->fspool_tbl[bk_idx]), volumeID);
        if (IS_ERR_OR_NULL(fspool)) {
            dms_printk(LOG_LVL_INFO, "DMSC INFO inval fschunk null volume %u\n",
                    volumeID);
            ret = SPOOL_INVAL_CHUNK_NoVolume;
            break;
        }

        down_write(&fspool->space_plock);
        chunk = _find_fschunk_fspool_lockless(fspool, chunkID);
        if (IS_ERR_OR_NULL(chunk)) {
            up_write(&fspool->space_plock);
            ret = SPOOL_INVAL_CHUNK_NoChunk;
            dms_printk(LOG_LVL_INFO, "DMSC INFO inval null fschunk %llu volume %u\n",
                    chunkID, volumeID);

            break;
        }

        inval_ret = fschunk_invalidate_chunk(chunk);
        if (inval_ret) {
            ret = SPOOL_INVAL_CHUNK_Fail;
            up_write(&fspool->space_plock);
            break;
        }

        fspool->fsSize_remaining -= chunk->chunkLen;

        report_ret = fschunk_report_unuse(chunk, maxUnuseHBID);
        if (report_ret) {
            dms_printk(LOG_LVL_INFO, "DMSC INFO fail get max unuse hbid report\n");
            ret = SPOOL_INVAL_CHUNK_Fail;
        }

        up_write(&fspool->space_plock);
    } while (0);

    up_read(&(fstbl->fspool_tbl_lock[bk_idx]));

    return ret;
}

static int32_t _fspool_alloc_space (freespace_pool_t *fspool,
        uint64_t lb_s, uint32_t lb_len, metadata_t *arr_MD, ReqCommon_t *callerReq)
{
    fs_chunk_t *chunk;
    as_chunk_t *aschunk;
    metaD_item_t *mdPtr;
    uint64_t lb_s_tmp;
    uint32_t remain_lb;
    bool free_chunk;

    lb_s_tmp = lb_s;
    remain_lb = lb_len;

    arr_MD->array_metadata = (metaD_item_t **)discoC_mem_alloc(sizeof(metaD_item_t *)*lb_len,
            GFP_NOWAIT | GFP_ATOMIC);
    if (unlikely(IS_ERR_OR_NULL(arr_MD->array_metadata))) {
        dms_printk( LOG_LVL_WARN, "DMSC WARN Cannot allocate memory for lr\n");
        DMS_WARN_ON(true);
        return -ENOMEM;
    }

PEEK_NEXT_FSCHUNK:
    free_chunk = false;
    chunk = list_first_entry_or_null(&fspool->space_chunk_pool, fs_chunk_t,
            entry_spacePool);
    if (IS_ERR_OR_NULL(chunk)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN alloc space with null chunk\n");
        return -ENOENT;
    }

    aschunk = fschunk_alloc_space(chunk, lb_s_tmp, remain_lb);
    if (IS_ERR_OR_NULL(aschunk)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN fail alloc space from fschunk\n");
        return -ENOMEM;
    }

    spin_lock(&fspool->fsSizeInfo_lock);
    list_add_tail(&aschunk->entry_allocPool, &fspool->alloc_chunk_pool);
    fspool->num_alloc_unit++;
    fspool->fsSize_remaining -= aschunk->lb_len;

    if (chunk->chunkLen == 0) {
        list_del(&chunk->entry_spacePool);
        fspool->num_space_chunk--;
        free_chunk = true;
    }
    spin_unlock(&fspool->fsSizeInfo_lock);

    lb_s_tmp += aschunk->lb_len;
    remain_lb -= aschunk->lb_len;

    if (free_chunk) {
        fschunk_free_chunk(get_fsChunkMemPoolID(), chunk);
    }

    mdPtr = MData_alloc_MDItem();
    if (unlikely(IS_ERR_OR_NULL(mdPtr))) {
        dms_printk( LOG_LVL_WARN, "DMSC WARN fail alloc mem MDItem alloc fs\n");
        DMS_WARN_ON(true);
        return -ENOMEM;
    }
    arr_MD->array_metadata[arr_MD->num_MD_items] = mdPtr;
    arr_MD->num_MD_items++;
    aschunk_fill_metadata(aschunk, mdPtr);
    aschunk_commit_chunk(aschunk);

    smcache_add_aschunk(&fspool->cpool, aschunk);

    if (remain_lb > 0) {
        goto PEEK_NEXT_FSCHUNK;
    }

    return 0;
}

int32_t fspool_alloc_space (fspool_table_t *fstbl, uint32_t volumeID,
        uint64_t lb_s, uint32_t lb_len, metadata_t *arr_MD,
        ReqCommon_t *callerReq)
{
    freespace_pool_t *fspool;
    int32_t ret, bk_idx, alloc_ret;
    bool flush_all;

    ret = SPOOL_ALLOC_SPACE_OK;
    alloc_ret = 0;

    flush_all = false;

ALLOC_SPACE_AGAIN:
    bk_idx = volumeID % FREESPACE_POOL_HSIZE;
    down_read(&(fstbl->fspool_tbl_lock[bk_idx]));

    do {
        fspool = _fstbl_find_fspool_nolock(&(fstbl->fspool_tbl[bk_idx]), volumeID);
        if (IS_ERR_OR_NULL(fspool)) {
            dms_printk(LOG_LVL_INFO, "DMSC INFO inval fschunk null volume %u\n",
                    volumeID);
            ret = SPOOL_ALLOC_SPACE_NoVolume;
            break;
        }

        down_write(&fspool->space_plock);

        if (fspool->fsSize_remaining >= lb_len) {
            alloc_ret = _fspool_alloc_space(fspool, lb_s, lb_len, arr_MD, callerReq);
        } else if (fspool->fsSize_requesting) {
            ret = SPOOL_ALLOC_SPACE_WAIT;
        } else {
            ret = SPOOL_ALLOC_SPACE_NOSPACE;
        }

        up_write(&fspool->space_plock);
    } while (0);

    up_read(&(fstbl->fspool_tbl_lock[bk_idx]));

    if (-ENOMEM == alloc_ret ||
            fspool->num_alloc_unit > sm_get_flush_criteria()) {
        if (flush_all == false) {
            sm_report_allvolume_spaceMData();
            flush_all = true;
        }
        msleep(100);
        goto ALLOC_SPACE_AGAIN;
    }

    return ret;
}

int32_t fspool_update_fsSizeRequesting (fspool_table_t *fstbl, uint32_t volumeID,
        bool do_inc)
{
    freespace_pool_t *fspool;
    int32_t ret, bk_idx;

    ret = 0;

    bk_idx = volumeID % FREESPACE_POOL_HSIZE;
    down_read(&(fstbl->fspool_tbl_lock[bk_idx]));

    do {
        fspool = _fstbl_find_fspool_nolock(&(fstbl->fspool_tbl[bk_idx]), volumeID);
        if (IS_ERR_OR_NULL(fspool)) {
            dms_printk(LOG_LVL_INFO, "DMSC INFO inval fschunk null volume %u\n",
                    volumeID);
            ret = -ENOENT;
            break;
        }

        down_write(&fspool->space_plock);
        if (do_inc) {
            fspool->fsSize_requesting++;
        } else {
            fspool->fsSize_requesting--;
        }
        up_write(&fspool->space_plock);
    } while (0);

    up_read(&(fstbl->fspool_tbl_lock[bk_idx]));

    return ret;
}

static void _fspool_cleanup_pool_fschunk (fspool_table_t *fstbl,
        freespace_pool_t *fspool)
{
    fs_chunk_t *cur, *next;
    int32_t memPID;

    memPID = get_fsChunkMemPoolID();

    list_for_each_entry_safe (cur, next, &fspool->space_chunk_pool,
            entry_spacePool) {
        if (fschunk_cleanup_chunk(cur)) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN fail cleanup chunk %llu volume %llu\n",
                    cur->chunkID, fspool->volumeID);
            continue;
        }

        list_del(&cur->entry_spacePool);
        fschunk_free_chunk(memPID, cur);
        fspool->num_space_chunk--;
    }
}

void fspool_cleanup_pool (fspool_table_t *fstbl, uint32_t volumeID)
{
    freespace_pool_t *fspool;
    int32_t bk_idx;

    bk_idx = volumeID % FREESPACE_POOL_HSIZE;

    down_read(&(fstbl->fspool_tbl_lock[bk_idx]));

    do {
        fspool = _fstbl_find_fspool_nolock(&(fstbl->fspool_tbl[bk_idx]), volumeID);
        if (IS_ERR_OR_NULL(fspool)) {
            dms_printk(LOG_LVL_INFO, "DMSC INFO inval fschunk null volume %u\n",
                    volumeID);
            break;
        }

        down_write(&fspool->space_plock);

        _fspool_cleanup_pool_fschunk(fstbl, fspool);

        up_write(&fspool->space_plock);
    } while (0);

    up_read(&(fstbl->fspool_tbl_lock[bk_idx]));
}

static void _fspool_inval_pool_fschunk (freespace_pool_t *fspool)
{
    fs_chunk_t *cur, *next;

    list_for_each_entry_safe (cur, next, &fspool->space_chunk_pool,
            entry_spacePool) {
        if (fschunk_invalidate_chunk(cur)) {
            continue;
        }

        fspool->fsSize_remaining -= cur->chunkLen;
    }
}

/*
 * Description: find fs pool in table and invalidate all free space chunk of a volume
 */
void fspool_inval_pool (fspool_table_t *fstbl, uint32_t volumeID)
{
    freespace_pool_t *fspool;
    uint32_t bk_idx;

    bk_idx = volumeID % FREESPACE_POOL_HSIZE;

    down_read(&(fstbl->fspool_tbl_lock[bk_idx]));

    do {
        fspool = _fstbl_find_fspool_nolock(&(fstbl->fspool_tbl[bk_idx]), volumeID);
        if (IS_ERR_OR_NULL(fspool)) {
            dms_printk(LOG_LVL_INFO, "DMSC INFO inval fschunk null volume %u\n",
                    volumeID);
            break;
        }

        down_write(&fspool->space_plock);

        _fspool_inval_pool_fschunk(fspool);

        up_write(&fspool->space_plock);
    } while (0);

    up_read(&(fstbl->fspool_tbl_lock[bk_idx]));
}

static void _gen_reportFree_task_data (fs_task_t *fstsk, fs_chunk_t *chunk,
        uint32_t volumeID)
{
    metaD_item_t *mdItem_ptr;

    fstsk->MetaData.num_MD_items = 1;
    fstsk->MetaData.num_invalMD_items = 0;

    fstsk->MetaData.usr_ioaddr.volumeID = volumeID;
    //fstsk->MetaData.usr_ioaddr.lbid_start = 0;
    //fstsk->MetaData.usr_ioaddr.lbid_len = 0;
    //fstsk->MetaData.usr_ioaddr.metaSrv_ipaddr = 1234567;
    //fstsk->MetaData.usr_ioaddr.metaSrv_port = 1234;

    fstsk->MetaData.array_metadata = discoC_mem_alloc(sizeof(metaD_item_t *),
            GFP_NOWAIT | GFP_ATOMIC);
    fstsk->MetaData.array_metadata[0] = MData_alloc_MDItem();

    mdItem_ptr = fstsk->MetaData.array_metadata[0];
    memset(mdItem_ptr, 0, sizeof(metaD_item_t));
    MData_init_MDItem(mdItem_ptr, fstsk->MetaData.usr_ioaddr.lbid_start,
            fstsk->MetaData.usr_ioaddr.lbid_len, 2, MData_INVALID, false, false, false);

    mdItem_ptr->space_chunkID = chunk->chunkID;
    mdItem_ptr->arr_HBID[0] = chunk->HBID_s;
}

static int32_t _fspool_report_chunk_unuse (struct list_head *phead,
        struct list_head *tsk_lhead, uint32_t volumeID)
{
    fs_chunk_t *cur, *next;
    fs_task_t *fstsk;
    uint64_t max_hbid;
    int32_t num_chunk, ret, tskMPollID;

    num_chunk = 0;
    tskMPollID = get_tskMemPoolID();

    list_for_each_entry_safe (cur, next, phead, entry_spacePool) {
        ret = fschunk_report_unuse(cur, &max_hbid);
        if (ret) {
            continue;
        }

        fstsk = alloc_fs_task(tskMPollID);

        init_fs_task(fstsk, fstask_report_unuse, generate_fsReq_ID(), NULL, NULL);

        _gen_reportFree_task_data(fstsk, cur, volumeID);
        list_add_tail(&fstsk->entry_taskWQ, tsk_lhead);

        num_chunk++;
    }

    return num_chunk;
}

int32_t fspool_report_pool_unusespace (fspool_table_t *fstbl, uint32_t volumeID,
        struct list_head *tsk_lhead)
{
    freespace_pool_t *fspool;
    int32_t bk_idx, numtsk;

    bk_idx = volumeID % FREESPACE_POOL_HSIZE;

    numtsk = 0;

    down_read(&(fstbl->fspool_tbl_lock[bk_idx]));

    do {
        fspool = _fstbl_find_fspool_nolock(&(fstbl->fspool_tbl[bk_idx]), volumeID);
        if (IS_ERR_OR_NULL(fspool)) {
            break;
        }

        down_read(&fspool->space_plock);

        numtsk = _fspool_report_chunk_unuse(&fspool->space_chunk_pool,
                tsk_lhead, volumeID);
        up_read(&fspool->space_plock);
    } while (0);

    up_read(&(fstbl->fspool_tbl_lock[bk_idx]));

    return numtsk;
}

static void _gen_flush_task_data (fs_task_t *fstsk, as_chunk_t *aschunk,
        uint32_t volumeID)
{
    fstsk->MetaData.num_MD_items = 1;
    fstsk->MetaData.num_invalMD_items = 0;

    fstsk->MetaData.usr_ioaddr.volumeID = volumeID;
    fstsk->MetaData.usr_ioaddr.lbid_start = aschunk->lbid_s;
    fstsk->MetaData.usr_ioaddr.lbid_len = aschunk->lb_len;
    //fstsk->MetaData.usr_ioaddr.metaSrv_ipaddr = 999;
    //fstsk->MetaData.usr_ioaddr.metaSrv_port = 1234;

    fstsk->MetaData.array_metadata = discoC_mem_alloc(sizeof(metaD_item_t *),
            GFP_NOWAIT | GFP_ATOMIC);
    fstsk->MetaData.array_metadata[0] = MData_alloc_MDItem();

    aschunk_fill_metadata(aschunk, fstsk->MetaData.array_metadata[0]);
}

static int32_t _fspool_report_chunk_alloc (struct list_head *phead,
        struct list_head *tsk_lhead, uint32_t volumeID)
{
    as_chunk_t *cur, *next;
    fs_task_t *fstsk;
    int32_t num_tsk, ret, tskMPollID;

    num_tsk = 0;
    tskMPollID = get_tskMemPoolID();

    list_for_each_entry_safe (cur, next, phead, entry_allocPool) {
        ret = aschunk_flush_chunk(cur);
        if (ret) {
            continue;
        }

        fstsk = alloc_fs_task(tskMPollID);

        init_fs_task(fstsk, fstask_flush_LHO, generate_fsReq_ID(), NULL, NULL);

        _gen_flush_task_data(fstsk, cur, volumeID);
        list_add_tail(&fstsk->entry_taskWQ, tsk_lhead);

        num_tsk++;
    }

    return num_tsk;
}

static int32_t _fspool_report_pool_allocspace (freespace_pool_t *pool,
        struct list_head *tsk_lhead)
{
    int32_t numtsk;

    numtsk = 0;

    down_read(&pool->space_plock);

    numtsk = _fspool_report_chunk_alloc(&pool->alloc_chunk_pool,
                    tsk_lhead, pool->volumeID);

    up_read(&pool->space_plock);

    return numtsk;
}

int32_t fspool_report_pool_allocspace (fspool_table_t *fstbl, uint32_t volumeID,
        struct list_head *tsk_lhead)
{
    freespace_pool_t *fspool;
    int32_t bk_idx, numtsk;

    bk_idx = volumeID % FREESPACE_POOL_HSIZE;

    numtsk = 0;

    down_read(&(fstbl->fspool_tbl_lock[bk_idx]));

    do {
        fspool = _fstbl_find_fspool_nolock(&(fstbl->fspool_tbl[bk_idx]), volumeID);
        if (IS_ERR_OR_NULL(fspool)) {
            break;
        }

        numtsk = _fspool_report_pool_allocspace(fspool, tsk_lhead);

    } while (0);

    up_read(&(fstbl->fspool_tbl_lock[bk_idx]));

    return numtsk;
}

static as_chunk_t *_find_aschunk_lbid (freespace_pool_t *fspool, uint64_t lbid_s)
{
    as_chunk_t *cur, *next;
    as_chunk_t *aschunk;

    aschunk = NULL;
    list_for_each_entry_safe (cur, next, &fspool->alloc_chunk_pool, entry_allocPool) {
        if (cur->lbid_s == lbid_s) {
            aschunk = cur;
            break;
        }
    }

    return aschunk;
}

/*
 * Description: called when worker send flush LHO and got response
 * TODO: if alloc chunk is flushing and has some DN R/W errors, how to handle?
 *
 */
int32_t fspool_remove_allocspace (fspool_table_t *fstbl, uint32_t volumeID,
        uint64_t lbid_s, uint32_t lb_len)
{
    freespace_pool_t *fspool;
    as_chunk_t *aschunk;
    int32_t ret, bk_idx;
    bool dofree;

    ret = 0;

    bk_idx = volumeID % FREESPACE_POOL_HSIZE;
    down_read(&(fstbl->fspool_tbl_lock[bk_idx]));
    do {
        fspool = _fstbl_find_fspool_nolock(&(fstbl->fspool_tbl[bk_idx]), volumeID);
        if (IS_ERR_OR_NULL(fspool)) {
            break;
        }

        dofree = false;
        down_write(&fspool->space_plock);

        aschunk = _find_aschunk_lbid(fspool, lbid_s);
        if (IS_ERR_OR_NULL(aschunk) == false) {
            //TODO: should check whether aschunk state is removable
            list_del(&aschunk->entry_allocPool);
            fspool->num_alloc_unit--;
            //TODO: test me 20180306
            smcache_rm_aschunk(&fspool->cpool, aschunk->lbid_s, aschunk->lb_len);
            dofree = true;
        }

        up_write(&fspool->space_plock);

        if (dofree) {
            aschunk_free_chunk(get_asChunkMemPoolID(), aschunk);
        }
    } while(0);
    up_read(&(fstbl->fspool_tbl_lock[bk_idx]));

    return ret;
}

static int32_t _fspool_report_bucket_allocspace (fspool_table_t *fstbl,
        struct list_head *tsk_lhead, struct list_head *lhead)
{
    freespace_pool_t *cur, *next;
    int32_t numtsk;

    numtsk = 0;

    list_for_each_entry_safe (cur, next, lhead, entry_fspooltbl) {
        numtsk += _fspool_report_pool_allocspace(cur, tsk_lhead);
    }

    return numtsk;
}

/*
 * TODO: we may need lots of task, we should do some flow control when too many
 */
int32_t fspool_report_allpool_allocspace (fspool_table_t *fstbl,
        struct list_head *tsk_lhead)
{
    int32_t numtsk, i;

    numtsk = 0;
    for (i = 0; i < FREESPACE_POOL_HSIZE; i++) {
        down_read(&(fstbl->fspool_tbl_lock[i]));
        numtsk += _fspool_report_bucket_allocspace(fstbl, tsk_lhead,
                &(fstbl->fspool_tbl[i]));
        up_read(&(fstbl->fspool_tbl_lock[i]));
    }

    return numtsk;
}

/*
 * return number of MD bucket for lbid_s lbid_len
 */
int32_t fspool_query_volume_MDCache (fspool_table_t *fstbl, metadata_t *arr_MD_bk,
        uint32_t volumeID, uint64_t lbid_s, uint32_t lbid_len)
{
    freespace_pool_t *fspool;
    int32_t ret, bk_idx;

    ret = 0;

    bk_idx = volumeID % FREESPACE_POOL_HSIZE;
    down_read(&(fstbl->fspool_tbl_lock[bk_idx]));
    do {
        fspool = _fstbl_find_fspool_nolock(&(fstbl->fspool_tbl[bk_idx]), volumeID);
        if (IS_ERR_OR_NULL(fspool)) {
            break;
        }

        down_write(&fspool->space_plock);

        ret = smcache_query_MData(&fspool->cpool, fspool->volumeID,
                lbid_s, lbid_len, arr_MD_bk);

        up_write(&fspool->space_plock);

    } while(0);
    up_read(&(fstbl->fspool_tbl_lock[bk_idx]));

    return ret;
}

void fspool_init_table (fspool_table_t *fstbl)
{
    int32_t i;

    for (i = 0; i < FREESPACE_POOL_HSIZE; i++) {
        INIT_LIST_HEAD(&(fstbl->fspool_tbl[i]));
        init_rwsem(&(fstbl->fspool_tbl_lock[i]));
    }

    atomic_set(&fstbl->num_fsPool, 0);
}

static void _rel_fspool_table_allBucket (fspool_table_t *fstbl)
{
    freespace_pool_t *cur, *next;
    struct list_head *lhead;
    int32_t i;

    for (i = 0; i < FREESPACE_POOL_HSIZE; i++) {
        lhead = &(fstbl->fspool_tbl[i]);
        down_write(&(fstbl->fspool_tbl_lock[i]));

        list_for_each_entry_safe (cur, next, lhead, entry_fspooltbl) {
            list_del(&cur->entry_fspooltbl);
            atomic_dec(&fstbl->num_fsPool);
            _fspool_free_pool(cur);
            discoC_mem_free(cur);
        }

        up_write(&(fstbl->fspool_tbl_lock[i]));
    }
}

int32_t fspool_rel_table (fspool_table_t *fstbl)
{
    int32_t ret;

    ret = 0;

    _rel_fspool_table_allBucket(fstbl);

    return ret;
}

static void _show_fspool_listpool (freespace_pool_t *fspool)
{
    fs_chunk_t *cur, *next;
    as_chunk_t *as_cur, *as_next;

    dms_printk(LOG_LVL_INFO, "DMSC INFO volume %llu:\n", fspool->volumeID);
    dms_printk(LOG_LVL_INFO, "DMSC INFO size info: %u %u %u\n",
            fspool->fsSize_remaining, fspool->fsSize_requesting,
            fspool->fsSize_userWant);
    dms_printk(LOG_LVL_INFO, "DMSC INFO numChunk %u numALU %u numReq %u\n",
            fspool->num_space_chunk, fspool->num_alloc_unit,
            fspool->num_waitReq);

    down_read(&fspool->space_plock);

    dms_printk(LOG_LVL_INFO, "DMSC INFO fs chunk list:\n");
    list_for_each_entry_safe (cur, next, &fspool->space_chunk_pool,
            entry_spacePool) {
        fschunk_show_chunk(cur);
    }

    dms_printk(LOG_LVL_INFO, "DMSC INFO alloc chunk list:\n");
    list_for_each_entry_safe (as_cur, as_next, &fspool->alloc_chunk_pool,
            entry_allocPool) {
        aschunk_show_chunk(as_cur);
    }

    up_read(&fspool->space_plock);
}

static void _show_fspool_bucket (struct list_head *lhead)
{
    freespace_pool_t *cur, *next;

    list_for_each_entry_safe (cur, next, lhead, entry_fspooltbl) {
        _show_fspool_listpool(cur);
    }
}

void fspool_show_table (fspool_table_t *fstbl)
{
    int32_t i;

    for (i = 0; i < FREESPACE_POOL_HSIZE; i++) {
        down_read(&fstbl->fspool_tbl_lock[i]);

        _show_fspool_bucket(&(fstbl->fspool_tbl[i]));

        up_read(&fstbl->fspool_tbl_lock[i]);
    }
}

static int32_t _dump_fspool_listpool (freespace_pool_t *fspool,
        int8_t *buff, int32_t buff_size)
{
    fs_chunk_t *cur, *next;
    as_chunk_t *as_cur, *as_next;
    int32_t len;

    len = 0;

    len += sprintf(buff + len,
            "\nfspool ID %llu size (remaining, requesting, userWant) (%u, %u, %u)\n",
            fspool->volumeID, fspool->fsSize_remaining, fspool->fsSize_requesting,
            fspool->fsSize_userWant);
    if (len >= buff_size) {
        return len;
    }

    len += sprintf(buff + len, "  chunk info (fs, alloc, wait) (%u, %u, %u)\n",
            fspool->num_space_chunk, fspool->num_alloc_unit, fspool->num_waitReq);
    if (len >= buff_size) {
        return len;
    }

    down_read(&fspool->space_plock);

    list_for_each_entry_safe (cur, next, &fspool->space_chunk_pool,
            entry_spacePool) {
        len += fschunk_dump_chunk(cur, buff + len, buff_size - len);
        if (len >= buff_size) {
            break;
        }
    }

    if (len < buff_size) {
        list_for_each_entry_safe_reverse (as_cur, as_next,
                &fspool->alloc_chunk_pool, entry_allocPool) {
            len += aschunk_dump_chunk(as_cur, buff + len, buff_size - len);
            break;
        }
    }

    up_read(&fspool->space_plock);

    return len;
}

static int32_t _dump_fspool_bucket (struct list_head *lhead,
        int8_t *buff, int32_t buff_size)
{
    freespace_pool_t *cur, *next;
    int32_t len;

    len = 0;
    list_for_each_entry_safe (cur, next, lhead, entry_fspooltbl) {

        len += _dump_fspool_listpool(cur, buff + len, buff_size - len);
    }

    return len;
}

int32_t fspool_dump_table (fspool_table_t *fstbl, int8_t *buff, int32_t buff_size)
{
    int32_t i, len;

    len = 0;
    for (i = 0; i < FREESPACE_POOL_HSIZE; i++) {
        down_read(&fstbl->fspool_tbl_lock[i]);

        len += _dump_fspool_bucket(&(fstbl->fspool_tbl[i]), buff + len, buff_size - len);

        up_read(&fstbl->fspool_tbl_lock[i]);

        if (len >= buff_size) {
            break;
        }
    }

    return len;
}

#endif
