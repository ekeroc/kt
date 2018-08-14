/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * IOReqPool.c
 */
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/blkdev.h>
#include <linux/types.h>
#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/dms_kernel_version.h"
#include "../common/discoC_mem_manager.h"
#include "../metadata_manager/metadata_manager.h"
#include "discoC_IOSegment_Request.h"
#include "discoC_IO_Manager.h"
#include "../vdisk_manager/volume_manager.h"
#include "discoC_IORequest.h"
#include "discoC_IOReqPool.h"
#include "discoC_IOReqPool_private.h"

void IOReqPool_gain_lock (volume_IOReqPool_t *volIOq)
{
    mutex_lock(&volIOq->ioReq_plock);
}

void IOReqPool_rel_lock (volume_IOReqPool_t *volIOq)
{
    mutex_unlock(&volIOq->ioReq_plock);
}

void IOReqPool_addOvlpReq_nolock (io_request_t *ioReq, volume_IOReqPool_t *volIOq)
{
    if (ioReq_chkset_fea(ioReq, IOREQ_FEA_IN_WAITPOOL)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN ioReq %u in wait pool add fail\n",
                ioReq->ioReqID);
        return;
    }

    list_add_tail(&ioReq->list2ioReqPool, &volIOq->ioReq_ovlp_pool);
    atomic_inc(&discoCIO_Mgr.cnt_onfly_ovlpioReq);
}

void IOReqPool_rmOvlpReq_nolock (io_request_t *ioReq)
{
    if (ioReq_chkclear_fea(ioReq, IOREQ_FEA_IN_WAITPOOL)) {
        list_del(&ioReq->list2ioReqPool);
        atomic_dec(&discoCIO_Mgr.cnt_onfly_ovlpioReq);
    } else {
        dms_printk(LOG_LVL_WARN, "DMSC WARN ioReq %u not in wait pool rm fail\n",
                ioReq->ioReqID);
    }
}

void IOReqPool_addReq_nolock (io_request_t *ioReq, volume_IOReqPool_t *volIOq)
{
    if (ioReq_chkset_fea(ioReq, IOREQ_FEA_IN_REQPOOL)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN ioReq %u in req pool add fail\n",
                ioReq->ioReqID);
        return;
    }

    list_add_tail(&ioReq->list2ioReqPool, &volIOq->ioReq_pool);
    atomic_inc(&discoCIO_Mgr.cnt_onfly_ioReq);
}

void IOReqPool_rmReq_nolock (io_request_t *ioReq)
{
    if (ioReq_chkclear_fea(ioReq, IOREQ_FEA_IN_REQPOOL)) {
        list_del(&ioReq->list2ioReqPool);
        atomic_dec(&discoCIO_Mgr.cnt_onfly_ioReq);
    } else {
        dms_printk(LOG_LVL_WARN, "DMSC WARN ioReq %u not in pool rm fail\n",
                ioReq->ioReqID);
    }
}

#ifdef DISCO_PREALLOC_SUPPORT
void volIOReqQMgr_gain_fwwaitQ_lock (volume_IOReqPool_t *myPool)
{
    mutex_lock(&myPool->ioSReq_fw_waitplock);
}

void volIOReqQMgr_rel_fwwaitQ_lock (volume_IOReqPool_t *myPool)
{
    mutex_unlock(&myPool->ioSReq_fw_waitplock);
}

void volIOReqQMgr_add_fwwaitQ_nolock (volume_IOReqPool_t *myPool,
        struct list_head *entry)
{
    list_add_tail(entry, &myPool->ioSReq_fw_waitpool);
    myPool->numWaitReqs++;
}

struct list_head *volIOReqQMgr_peek_fwwaitQ_nolock (volume_IOReqPool_t *myPool)
{
    struct list_head *entry, *cur, *next;

    entry = NULL;

    list_for_each_safe (cur, next, &myPool->ioSReq_fw_waitpool) {
        entry = cur;
        myPool->numWaitReqs--;
        list_del(entry);
        break;
    }

    return entry;
}
#endif

static uint32_t _gain_volIOQ_pWlock (uint32_t volumeID, volIOReqPool_Mgr_t *mymgr)
{
    uint32_t lockID;

    lockID = volumeID % IOMGR_VOLIOQ_HKEY;

    down_write(&(mymgr->volIOQ_plock[lockID]));

    return lockID;
}

static void _rel_volIOQ_pWlock (uint32_t lockID, volIOReqPool_Mgr_t *mymgr)
{
    up_write(&(mymgr->volIOQ_plock[lockID]));
}

static uint32_t _gain_volIOQ_pRlock (uint32_t volumeID, volIOReqPool_Mgr_t *mymgr)
{
    uint32_t lockID;

    lockID = volumeID % IOMGR_VOLIOQ_HKEY;

    down_read(&(mymgr->volIOQ_plock[lockID]));

    return lockID;
}

static void _rel_volIOQ_pRlock (uint32_t lockID, volIOReqPool_Mgr_t *mymgr)
{
    up_read(&(mymgr->volIOQ_plock[lockID]));
}

static volume_IOReqPool_t *_find_volIOQ_nolock (uint32_t volumeID,
        uint32_t lockID, volIOReqPool_Mgr_t *mymgr)
{
    struct list_head *lhead;
    volume_IOReqPool_t *cur, *next, *myPool;

    lhead = &(mymgr->volIOQ_pool[lockID]);
    myPool = NULL;
    list_for_each_entry_safe (cur, next, lhead, list2volIORQ) {
        if (cur->volumeID == volumeID) {
            myPool = cur;
            break;
        }
    }

    return myPool;
}

static void _add_volIOQ_nolock (volume_IOReqPool_t *myPool, uint32_t lockID,
        volIOReqPool_Mgr_t *mymgr)
{
    struct list_head *lhead;

    lhead = &(mymgr->volIOQ_pool[lockID]);
    list_add_tail(&myPool->list2volIORQ, lhead);
    atomic_inc(&mymgr->num_volIOQ);
}

static void _rm_volIOQ_nolock (volume_IOReqPool_t *myPool,
        volIOReqPool_Mgr_t *mymgr)
{
    list_del(&myPool->list2volIORQ);
    atomic_dec(&mymgr->num_volIOQ);
}

static volume_IOReqPool_t *_alloc_volIOQ (void)
{
    return discoC_mem_alloc(sizeof(volume_IOReqPool_t), GFP_NOWAIT);
}

static void _init_volIOQ (volume_IOReqPool_t *myPool, uint32_t volID)
{
    mutex_init(&myPool->ioReq_plock);
    INIT_LIST_HEAD(&myPool->ioReq_pool);
    INIT_LIST_HEAD(&myPool->ioReq_ovlp_pool);
    INIT_LIST_HEAD(&myPool->list2volIORQ);
#ifdef DISCO_PREALLOC_SUPPORT
    INIT_LIST_HEAD(&myPool->ioSReq_fw_waitpool);
    mutex_init(&myPool->ioSReq_fw_waitplock);
    myPool->numWaitReqs = 0;
#endif
    myPool->volumeID = volID;
    atomic_set(&myPool->numIOReq_pool, 0);
    atomic_set(&myPool->numIOReq_waitpool, 0);
}

int32_t volIOReqQMgr_add_pool (uint32_t volumeID, volIOReqPool_Mgr_t *mymgr)
{
    volume_IOReqPool_t *myPool;
    uint32_t lockID;
    int32_t ret;

    ret = 0;

    lockID = _gain_volIOQ_pWlock(volumeID, mymgr);

    myPool = _find_volIOQ_nolock(volumeID, lockID, mymgr);
    do {
        if (!IS_ERR_OR_NULL(myPool)) {
            ret = -EEXIST;
            dms_printk(LOG_LVL_WARN, "DMSC WARN vol %u exist add fail\n",
                    volumeID);
            break;
        }

        myPool = _alloc_volIOQ();
        if (IS_ERR_OR_NULL(myPool)) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN fail alloc mem volume IO queue %u\n", volumeID);
            ret = -ENOMEM;
            break;
        }


        _init_volIOQ(myPool, volumeID);
        _add_volIOQ_nolock(myPool, lockID, mymgr);
    } while (0);

    _rel_volIOQ_pWlock(lockID, mymgr);

    return ret;
}

static int32_t _clear_IOReq_pool (volume_IOReqPool_t *myPool, struct list_head *buff_list)
{
    io_request_t *cur, *next;
    int32_t num_IOR;

    //gain lock

    num_IOR = 0;

    mutex_lock(&myPool->ioReq_plock);
    list_for_each_entry_safe (cur, next, &myPool->ioReq_ovlp_pool, list2ioReqPool) {
        list_move_tail(&cur->list2ioReqPool, buff_list);
        num_IOR++;
    }

    list_for_each_entry_safe (cur, next, &myPool->ioReq_pool, list2ioReqPool) {
        list_move_tail(&cur->list2ioReqPool, buff_list);
        num_IOR++;
    }
    mutex_unlock(&myPool->ioReq_plock);

    return num_IOR;
}

int32_t volIOReqQMgr_clear_pool (uint32_t volumeID, volIOReqPool_Mgr_t *mymgr,
        struct list_head *buff_list)
{
    volume_IOReqPool_t *myPool;
    uint32_t lockID;
    int32_t ret;

    ret = 0;
    lockID = _gain_volIOQ_pRlock(volumeID, mymgr);

    myPool = _find_volIOQ_nolock(volumeID, lockID, mymgr);
    do {
        if (IS_ERR_OR_NULL(myPool)) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN clear non-exist volIOQ %u\n", volumeID);
            ret = -ENOENT;
            break;
        }

        ret = _clear_IOReq_pool(myPool, buff_list);
    } while (0);

    _rel_volIOQ_pRlock(lockID, mymgr);

    return ret;
}

void volIOReqQMgr_rm_pool (uint32_t volumeID, volIOReqPool_Mgr_t *mymgr)
{
    volume_IOReqPool_t *myPool;
    uint32_t lockID;
    int32_t ret;

    ret = 0;
    lockID = _gain_volIOQ_pWlock(volumeID, mymgr);

    myPool = _find_volIOQ_nolock(volumeID, lockID, mymgr);
    do {
        if (IS_ERR_OR_NULL(myPool)) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN rm non-exist volIOQ %u\n", volumeID);
            ret = -ENOENT;
            break;
        }
        _rm_volIOQ_nolock(myPool, mymgr);
    } while (0);

    _rel_volIOQ_pWlock(lockID, mymgr);

    if (IS_ERR_OR_NULL(myPool) == false) {
        discoC_mem_free(myPool);
    }
}

volume_IOReqPool_t *volIOReqQMgr_find_pool (uint32_t volumeID, volIOReqPool_Mgr_t *mymgr)
{
    volume_IOReqPool_t *myPool;
    uint32_t lockID;

    lockID = _gain_volIOQ_pRlock(volumeID, mymgr);
    myPool = _find_volIOQ_nolock(volumeID, lockID, mymgr);
    _rel_volIOQ_pRlock(lockID, mymgr);

    return myPool;
}

volIOReqPool_Mgr_t *volIOReqQMgr_get_manager (void)
{
    return &volIOReqPool_mgr;
}

void volIOReqQMgr_init_manager (volIOReqPool_Mgr_t *mymgr)
{
    int32_t i;

    for (i = 0; i < IOMGR_VOLIOQ_HKEY; i++) {
        INIT_LIST_HEAD(&(mymgr->volIOQ_pool[i]));
        init_rwsem(&(mymgr->volIOQ_plock[i]));
    }

    atomic_set(&mymgr->num_volIOQ, 0);
}

//TODO: check whether linger any request and volume queue
int32_t volIOReqQMgr_rel_manager (volIOReqPool_Mgr_t *mymgr)
{
    int32_t ret;

    ret = 0;

    if (atomic_read(&mymgr->num_volIOQ) != 0) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN linger volume IOReq queue\n");
    }

    return ret;
}
