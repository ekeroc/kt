/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_NNMDReqPool.c
 * Provide a hash base pool for holding all metadata requests.
 */

#include <linux/version.h>
#include <linux/blkdev.h>
#include <net/sock.h>
#include <linux/delay.h>
#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/dms_kernel_version.h"
#include "../common/discoC_mem_manager.h"
#include "../metadata_manager/metadata_manager.h"
#include "../common/thread_manager.h"
#include "../connection_manager/conn_manager_export.h"
#include "discoC_NNC_Manager_api.h"
#include "discoC_NNC_worker.h"
#include "../flowcontrol/FlowControl.h"
#include "discoC_NNClient.h"
#include "discoC_NNMData_Request.h"
#include "discoC_NNMDReqPool.h"

/*
 * NNMDReqPool_get_numReq: get number of namenode metadata request in request pool
 * @reqPool: target pool to get
 *
 * Return: number of requests
 */
int32_t NNMDReqPool_get_numReq (NNMDataReq_Pool_t *reqPool)
{
    return atomic_read(&reqPool->num_NNMDReq);
}

/*
 * NNMDReqPool_acquire_plock: gain and lock bucket lock base on request ID
 * @reqPool: target request pool
 * @nnReqID: request ID which try to request
 *
 * Return: bucket ID for other API - add to queue
 *                                 - unlock
 */
int32_t NNMDReqPool_acquire_plock (NNMDataReq_Pool_t *reqPool, uint32_t nnReqID)
{
    uint32_t buck_idx;
    spinlock_t *buck_lock;

    buck_idx = nnReqID % NNREQ_HBUCKET_SIZE;
    buck_lock = &(reqPool->NNMDReq_plock[buck_idx]);
    spin_lock(buck_lock);

    return buck_idx;
}

/*
 * NNMDReqPool_rel_plock: unlock bucket lock base on request ID
 * @reqPool: target request pool
 * @lockID: lock bucket ID for release lock
 */
void NNMDReqPool_rel_plock (NNMDataReq_Pool_t *reqPool, int32_t lockID)
{
    spinlock_t *buck_lock;

    buck_lock = &(reqPool->NNMDReq_plock[lockID]);
    spin_unlock(buck_lock);
}

/*
 * NNMDReqPool_addReq_nolock: add namenode metadata request to request pool
 * @nnMDReq: nn metadata request to be added to request pool
 * @lockID : lock ID gain from API: NNMDReqPool_acquire_plock
 * @reqPool: target request pool to add
 *
 * Return:  0: add success
 *         ~0: add failure
 */
int32_t NNMDReqPool_addReq_nolock (NNMData_Req_t *nnMDReq, int32_t lockID,
        NNMDataReq_Pool_t *reqPool)
{
    struct list_head *lhead;

    if (unlikely(IS_ERR_OR_NULL(nnMDReq))) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN add null NNMDReq to req pool\n");
        DMS_WARN_ON(true);
        return -EINVAL;
    }

    lhead = &(reqPool->NNMDReq_pool[lockID]);
    list_add_tail(&nnMDReq->entry_nnReqPool, lhead);
    atomic_inc(&reqPool->num_NNMDReq);

    return 0;
}

/*
 * NNMDReqPool_findReq_nolock: find namenode metadata request from request pool
 *                             by given a request ID
 * @nnReqID: target request ID for find in request pool
 * @lockID : lock ID gain from API: NNMDReqPool_acquire_plock
 * @reqPool: target request pool to search
 *
 * Return:  0: add success
 *         ~0: add failure
 */
NNMData_Req_t *NNMDReqPool_findReq_nolock (uint32_t nnReqID, int32_t lockID,
        NNMDataReq_Pool_t *reqPool)
{
    NNMData_Req_t *cur, *next, *retReq;
    struct list_head *lhead;

    retReq = NULL;
    lhead = &(reqPool->NNMDReq_pool[lockID]);
    list_for_each_entry_safe (cur, next, lhead, entry_nnReqPool) {
        if (unlikely(IS_ERR_OR_NULL(cur))) {
            break;
        }

        if (cur->nnMDReq_comm.ReqID != nnReqID) {
            continue;
        }

        retReq = cur;
        break;
    }

    return retReq;
}

/*
 * NNMDReqPool_rmReq_nolock: remove namenode metadata request from request pool without lock
 * @myReq: target namenode metadata request to be removed from request pool
 * @reqPool : target pool be removed
 *
 */
void NNMDReqPool_rmReq_nolock (NNMData_Req_t *myReq, NNMDataReq_Pool_t *reqPool)
{
    list_del(&myReq->entry_nnReqPool);
    atomic_dec(&reqPool->num_NNMDReq);
}

/*
 * _get_NNMDReq_rlist_bk: given a list_head, find metadata request in list_head that can be retry
 * @lhead: link list to be search
 * @aMD_rlist : link list to put retryable acquire metadata request (alloc/query)
 * @num_raMD  : number of retryable acquire metadata request
 * @ciMD_rlist: link list to put retryable report metadata request (alloc/query)
 * @num_ciMD  : number of retryable report metadata request
 */
static void _get_NNMDReq_rlist_bk (uint32_t nnIPaddr, uint32_t nnPort,
        struct list_head *lhead,
        struct list_head *aMD_rlist, int32_t *num_raMD,
        struct list_head *ciMD_rlist, int32_t *num_ciMD)
{
    NNMData_Req_t *cur, *next;
    discoC_NNClient_t *myClient;

    list_for_each_entry_safe (cur, next, lhead, entry_nnReqPool) {
        NNMDReq_ref_Req(cur);
        myClient = cur->nnSrv_client;
        if (nnIPaddr != myClient->nnSrv_IPAddr || nnPort != myClient->nnSrv_port) {
            NNMDReq_deref_Req(cur);
            continue;
        }

        if (NNMDReq_check_acquire(cur)) {
            if (NNMDReq_acquire_retryable(cur)) {
                list_add_tail(&cur->entry_retryPool, aMD_rlist);
                (*num_raMD)++;
            } else {
                NNMDReq_deref_Req(cur);
            }
        } else {
            if (NNMDReq_report_retryable(cur)) {
                list_add_tail(&cur->entry_retryPool, ciMD_rlist);
                (*num_ciMD)++;
            } else {
                NNMDReq_deref_Req(cur);
            }
        }
    }
}

/*
 * NNMDReqPool_get_retryReq: find retryable metadata request in request pool
 * @aMD_rlist : link list to put retryable acquire metadata request (alloc/query)
 * @num_raMD  : number of retryable acquire metadata request
 * @ciMD_rlist: link list to put retryable report metadata request (alloc/query)
 * @num_ciMD  : number of retryable report metadata request
 * @reqPool: target request pool to be search
 */
void NNMDReqPool_get_retryReq (uint32_t nnIPaddr, uint32_t nnPort,
        struct list_head *aMD_rlist, int32_t *num_raMD,
        struct list_head *ciMD_rlist, int32_t *num_ciMD, NNMDataReq_Pool_t *reqPool)
{
    struct list_head *lhead;
    spinlock_t *buck_lock;
    int32_t i;

    *num_raMD = 0;
    *num_ciMD = 0;

    for (i = 0; i < NNREQ_HBUCKET_SIZE; i++) {
        lhead = &(reqPool->NNMDReq_pool[i]);
        buck_lock = &(reqPool->NNMDReq_plock[i]);
        spin_lock(buck_lock);

        _get_NNMDReq_rlist_bk(nnIPaddr, nnPort, lhead,
                aMD_rlist, num_raMD, ciMD_rlist, num_ciMD);

        spin_unlock(buck_lock);
    }
}

/*
 * NNMDReqPool_show_pool: print all metadata request in request pool to buffer
 * @reqPool : target request pool to be search
 * @buffer  : member buffer that hold content
 * @buff_len: size of buffer
 *
 * Return: how many characters be filled in buffer
 *
 * TODO: support me
 */
int32_t NNMDReqPool_show_pool (NNMDataReq_Pool_t *reqPool, int8_t *buffer,
        int32_t buff_len) {
    int32_t len;

    len = 0;
    len += sprintf(buffer + len, "Not support now\n");

//    if (len >= buff_len) {
//        break;
//    }


    return len;
}

/*
 * NNMDReqPool_init_pool: initialize a metadata request pool
 *                        (hashtable implemened by link list array)
 * @reqPool : target request pool to be initialized
 */
void NNMDReqPool_init_pool (NNMDataReq_Pool_t *reqPool)
{
    int32_t i;

    for (i = 0; i < NNREQ_HBUCKET_SIZE; i++) {
        INIT_LIST_HEAD(&(reqPool->NNMDReq_pool[i]));
        spin_lock_init(&(reqPool->NNMDReq_plock[i]));
    }

    atomic_set(&reqPool->num_NNMDReq, 0);
}

/*
 * rel_MDReqPool: remove all metadata request in request pool
 * @reqPool : target request pool to be initialized
 *
 * NOTE: Try not free request when hold spinlock
 */
void NNMDReqPool_rel_pool (NNMDataReq_Pool_t *reqPool)
{
    NNMData_Req_t *nnReq;
    spinlock_t *buck_lock;
    struct list_head *lhead;
    int32_t i, rel_cnt;

    rel_cnt = 0;
    for (i = 0; i < NNREQ_HBUCKET_SIZE; i++) {
        lhead = &(reqPool->NNMDReq_pool[i]);
        buck_lock = &(reqPool->NNMDReq_plock[i]);

RELEASE_NNMDREQ:
        spin_lock(buck_lock);
        nnReq = list_first_entry_or_null(lhead, NNMData_Req_t, entry_nnReqPool);
        spin_unlock(buck_lock);

        if (!IS_ERR_OR_NULL(nnReq)) {
            NNMDReq_free_Req(nnReq);
            goto RELEASE_NNMDREQ;
        }
    }
}
