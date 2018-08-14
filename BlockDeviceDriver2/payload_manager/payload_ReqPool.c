/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * payload_ReqPool.c
 *
 */
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/dms_kernel_version.h"
#include "../common/discoC_mem_manager.h"
#include "../metadata_manager/metadata_manager.h"
#include "payload_manager_export.h"
#include "payload_request_fsm.h"
#include "payload_request.h"
#include "payload_ReqPool.h"
#include "payload_manager.h"

/*
 * PLReqPool_get_numReq: get number of payload request in request pool
 * @pReqPool: target pool to get
 *
 * Return: number of requests
 */
int32_t PLReqPool_get_numReq (payloadReq_Pool_t *plReqPool)
{
    return atomic_read(&plReqPool->num_plReqs);
}

/*
 * PLReqPool_acquire_plock: gain and lock bucket lock base on request ID
 * @reqPool: target request pool
 * @dnReqID: request ID which try to handling
 *
 * Return: bucket ID for other API - add to queue
 *                                 - unlock
 */
int32_t PLReqPool_acquire_plock (payloadReq_Pool_t *pReqPool, uint32_t plReqID)
{
    uint32_t buck_idx;
    spinlock_t *buck_lock;

    buck_idx = plReqID % PLREQ_HBUCKET_SIZE;
    buck_lock = &(pReqPool->plReq_plock[buck_idx]);
    spin_lock(buck_lock);

    return buck_idx;
}

/*
 * PLReqPool_rel_plock: unlock bucket lock base on request ID
 * @reqPool: target request pool
 * @lockID: lock bucket ID for release lock
 */
void PLReqPool_rel_plock (payloadReq_Pool_t *pReqPool, uint32_t locID)
{
    spinlock_t *buck_lock;

    buck_lock = &(pReqPool->plReq_plock[locID]);
    spin_unlock(buck_lock);
}

/*
 * PLReqPool_addReq_nolock: add payload request to request pool
 * @plReq: payload request to be added to pool
 * @lockID : lock ID gain from API: PLReqPool_acquire_plock
 * @plReqPool: target request pool to add
 *
 * Return:  0: add success
 *         ~0: add failure
 */
int32_t PLReqPool_addReq_nolock (payload_req_t *plReq, int32_t lockID,
        payloadReq_Pool_t *plReqPool)
{
    struct list_head *lhead;

#if 0  //NOTE: caller guarantee plReq won't be NULL
    if (unlikely(IS_ERR_OR_NULL(plReq))) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN add null payload req to req pool\n");
        DMS_WARN_ON(true);
        return -EINVAL;
    }
#endif

    lhead = &(plReqPool->plReq_pool[lockID]);
    list_add_tail(&plReq->entry_plReqPool, lhead);
    atomic_inc(&plReqPool->num_plReqs);

    return 0;
}

/*
 * PLReqPool_findReq_nolock: find payload request from request pool by given a request ID
 * @plReqID: target request ID for find in request pool
 * @lockID : lock ID gain from API: DNUDReqPool_acquire_lock
 * @reqPool: target request pool to search
 *
 * Return:  payload request
 */
payload_req_t *PLReqPool_findReq_nolock (uint32_t plReqID, int32_t lockID,
        payloadReq_Pool_t *plReqPool)
{
    payload_req_t *cur, *next, *retReq;
    struct list_head *lhead;

    retReq = NULL;
    lhead = &(plReqPool->plReq_pool[lockID]);
    list_for_each_entry_safe (cur, next, lhead, entry_plReqPool) {
        if (unlikely(IS_ERR_OR_NULL(cur))) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN PLReq pool list broken\n");
            break;
        }

        if (cur->plReq_comm.ReqID != plReqID) {
            continue;
        }

        retReq = cur;
        break;
    }

    return retReq;
}

/*
 * PLReqPool_rmReq_nolock: remove payload request from request pool
 * @myReq: target payloada request to be removed from request pool
 * @reqPool : target pool be removed
 *
 */
void PLReqPool_rmReq_nolock (payload_req_t *myReq, payloadReq_Pool_t *reqPool)
{
    list_del(&myReq->entry_plReqPool);
    atomic_dec(&reqPool->num_plReqs);
}

/*
 * PLReqPool_init_pool: initial request pool
 * @pReqPool: target request pool to be initialize
 */
void PLReqPool_init_pool (payloadReq_Pool_t *pReqPool)
{
    int32_t ret, i;

    ret = 0;

    for (i = 0; i < PLREQ_HBUCKET_SIZE; i++) {
        INIT_LIST_HEAD(&pReqPool->plReq_pool[i]);
        spin_lock_init(&pReqPool->plReq_plock[i]);
    }

    atomic_set(&pReqPool->num_plReqs, 0);
}

/*
 * PLReqPool_rel_pool: release all payload request in request pool
 * @pReqPool: target request pool to be handled
 */
void PLReqPool_rel_pool (payloadReq_Pool_t *pReqPool)
{
    struct list_head *lhead;
    spinlock_t *buck_lock;
    payload_req_t *plReq;
    int32_t rel_cnt, i;

    if (atomic_read(&pReqPool->num_plReqs) == 0) {
        return;
    }

    dms_printk(LOG_LVL_WARN,
            "DMSC WARN free PLReq pool linger payload request %u\n",
            atomic_read(&pReqPool->num_plReqs));

    rel_cnt = 0;
    for (i = 0; i < PLREQ_HBUCKET_SIZE; i++) {
        lhead = &(pReqPool->plReq_pool[i]);
        buck_lock = &(pReqPool->plReq_plock[i]);

RELEASE_NNMDREQ:
        spin_lock(buck_lock);
        plReq = list_first_entry_or_null(lhead, payload_req_t, entry_plReqPool);
        spin_unlock(buck_lock);

        if (IS_ERR_OR_NULL(plReq) == false) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN free PLReq %u\n", plReq->plReq_comm.ReqID);
            PLReq_free_Req(plReq, g_payload_mgr.preq_mpool_ID);
            rel_cnt++;
            goto RELEASE_NNMDREQ;
        }
    }

    dms_printk(LOG_LVL_WARN, "DMSC WARN free PLReq num linger %u\n", rel_cnt);
}
