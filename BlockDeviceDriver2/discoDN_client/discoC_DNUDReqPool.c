/*
 * Copyright (C) 2010-2020 by Division F / ICL
 * Industrial Technology Research Institute
 *
 * discoC_DNUDReqPool.c
 *
 * Provide a hash base pool for holding all user data request (send to datanode)
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
#include "discoC_DNC_Manager_export.h"
#include "discoC_DNUData_Request.h"
#include "discoC_DNUDReqPool.h"

/*
 * DNUDReqPool_get_numReq: get number of datanode user data request in request pool
 * @reqPool: target pool to get
 *
 * Return: number of requests
 */
int32_t DNUDReqPool_get_numReq (DNUDataReq_Pool_t *reqPool)
{
    return atomic_read(&reqPool->num_DNUDReq);
}

/*
 * DNUDReqPool_acquire_plock: gain and lock bucket lock base on request ID
 * @reqPool: target request pool
 * @dnReqID: request ID which try to handling
 *
 * Return: bucket ID for other API - add to queue
 *                                 - unlock
 */
int32_t DNUDReqPool_acquire_plock (DNUDataReq_Pool_t *reqPool, uint32_t dnReqID)
{
    uint32_t bk_idx;
    spinlock_t *bk_lock;

    bk_idx = dnReqID % DNREQ_HBUCKET_SIZE;
    bk_lock = &(reqPool->DNUDReq_plock[bk_idx]);
    spin_lock(bk_lock);

    return bk_idx;
}

/*
 * DNUDReqPool_rel_plock: unlock bucket lock base on request ID
 * @reqPool: target request pool
 * @lockID: lock bucket ID for release lock
 */
void DNUDReqPool_rel_plock (DNUDataReq_Pool_t *reqPool, int32_t lockID)
{
    spinlock_t *bk_lock;

    bk_lock = &(reqPool->DNUDReq_plock[lockID]);
    spin_unlock(bk_lock);
}

/*
 * DNUDReqPool_addReq_nolock: add datanode user data request to request pool
 * @dnUDReq: dn user data request to be added to request pool
 * @lockID : lock ID gain from API: DNUDReqPool_acquire_plock
 * @reqPool: target request pool to add
 *
 * Return:  0: add success
 *         ~0: add failure
 */
int32_t DNUDReqPool_addReq_nolock (DNUData_Req_t *dnUDReq, int32_t lockID,
        DNUDataReq_Pool_t *reqPool)
{
    struct list_head *lhead;

    if (unlikely(IS_ERR_OR_NULL(dnUDReq))) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN add null dn_req to req pool\n");
        DMS_WARN_ON(true);
        return -EINVAL;
    }

    lhead = &(reqPool->DNUDReq_pool[lockID]);
    list_add_tail(&dnUDReq->entry_dnReqPool, lhead);
    atomic_inc(&reqPool->num_DNUDReq);

    return 0;
}

/*
 * DNUDReqPool_findReq_nolock: find datanode user data request from request pool by given a request ID
 * @dnReqID: target request ID for find in request pool
 * @lockID : lock ID gain from API: DNUDReqPool_acquire_plock
 * @reqPool: target request pool to search
 *
 * Return:  Datanode User data request
 */
DNUData_Req_t *DNUDReqPool_findReq_nolock (uint32_t dnReqID, int32_t lockID,
        DNUDataReq_Pool_t *reqPool)
{
    DNUData_Req_t *cur, *next, *retReq;
    struct list_head *lhead;

    retReq = NULL;
    lhead = &(reqPool->DNUDReq_pool[lockID]);
    list_for_each_entry_safe (cur, next, lhead, entry_dnReqPool) {
        if (unlikely(IS_ERR_OR_NULL(cur))) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN find DNReq list broken\n");
            break;
        }

        if (cur->dnReq_comm.ReqID != dnReqID) {
            continue;
        }

        retReq = cur;
        break;
    }

    return retReq;
}

/*
 * DNUDReqPool_rmReq_nolock: remove datanode user data request from request pool
 * @myReq: target datanode user data request to be removed from request pool
 * @reqPool : target pool be removed
 *
 */
void DNUDReqPool_rmReq_nolock (DNUData_Req_t *myReq, DNUDataReq_Pool_t *reqPool)
{
    list_del(&myReq->entry_dnReqPool);
    atomic_dec(&reqPool->num_DNUDReq);
}

/*
 * _get_DNUDReq_rlist_bk: given a list_head, find datanode request in list_head that can be retry
 * @dnIPaddr  : IP of datanode that connected again
 * @dnPort    : port of datanode that connected again
 * @lhead: link list to be search
 * @UDReq_rlist : link list to put retryable datanode request
 * @num_DNUDReq  : number of retryable datanode request
 */
static void _get_DNUDReq_rlist_bk (uint32_t dnIPaddr, uint32_t dnPort,
        struct list_head *lhead,
        struct list_head *UDReq_rlist, int32_t *num_DNUDReq)
{
    DNUData_Req_t *cur, *next;

    list_for_each_entry_safe (cur, next, lhead, entry_dnReqPool) {
        //DNUDReq_ref_Req(cur);
        if (dnIPaddr != cur->ipaddr || dnPort != cur->port) {
            //DNUDReq_deref_Req(cur);
            continue;
        }

        if (DNUDReq_check_retryable(cur)) {
            DNUDReq_ref_Req(cur);
            list_add_tail(&cur->entry_dnWkerQ, UDReq_rlist);
            (*num_DNUDReq)++;
        }
    }
}

/*
 * DNUDReqPool_get_retryReq: find retryable datanode request in request pool
 * @dnIPaddr  : IP of datanode that connected again
 * @dnPort    : port of datanode that connected again
 * @UDReq_rlist : link list to put retryable datanode request
 * @num_DNUDReq  : number of retryable datanode request
 * @reqPool: target request pool to be search
 */
void DNUDReqPool_get_retryReq (uint32_t dnIPaddr, uint32_t dnPort,
        struct list_head *UDReq_rlist, int32_t *num_DNUDReq,
        DNUDataReq_Pool_t *reqPool)
{
    struct list_head *lhead;
    spinlock_t *buck_lock;
    int32_t i;

    *num_DNUDReq = 0;

    for (i = 0; i < DNREQ_HBUCKET_SIZE; i++) {
        lhead = &(reqPool->DNUDReq_pool[i]);
        buck_lock = &(reqPool->DNUDReq_plock[i]);
        spin_lock(buck_lock);

        _get_DNUDReq_rlist_bk(dnIPaddr, dnPort, lhead, UDReq_rlist, num_DNUDReq);

        spin_unlock(buck_lock);
    }
}

/*
 * DNUDReqPool_show_pool: print datanode request content to buffer
 * @reqPool : target request pool to be search
 * @buffer  : member buffer that hold content
 * @buff_len: size of buffer
 *
 * Return: how many characters be filled in buffer
 *
 * TODO: support me
 */
int32_t DNUDReqPool_show_pool (DNUDataReq_Pool_t *reqPool, int8_t *buffer,
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
 * DNUDReqPool_init_pool: initialize a user data request pool
 * @reqPool : target request pool to be initialized
 */
void DNUDReqPool_init_pool (DNUDataReq_Pool_t *reqPool)
{
    int32_t i;

    for (i = 0; i < DNREQ_HBUCKET_SIZE; i++) {
        INIT_LIST_HEAD(&(reqPool->DNUDReq_pool[i]));
        spin_lock_init(&(reqPool->DNUDReq_plock[i]));
    }

    atomic_set(&reqPool->num_DNUDReq, 0);
}

/*
 * DNUDReqPool_rel_pool: remove all datanode request in request pool
 * @reqPool : target request pool to be initialized
 *
 * NOTE: Try not free request when hold spinlock
 */
void DNUDReqPool_rel_pool (DNUDataReq_Pool_t *reqPool)
{
    DNUData_Req_t *dnReq;
    spinlock_t *buck_lock;
    struct list_head *lhead;
    int32_t i, rel_cnt;

    rel_cnt = 0;
    for (i = 0; i < DNREQ_HBUCKET_SIZE; i++) {
        lhead = &(reqPool->DNUDReq_pool[i]);
        buck_lock = &(reqPool->DNUDReq_plock[i]);

RELEASE_NNMDREQ:
        spin_lock(buck_lock);
        dnReq = list_first_entry_or_null(lhead, DNUData_Req_t, entry_dnReqPool);
        spin_unlock(buck_lock);

        if (IS_ERR_OR_NULL(dnReq) == false) {
            //TODO: chechk whether need free request
            dms_printk(LOG_LVL_WARN, "DMSC WARN rel pool linger DNUDReq\n");
            goto RELEASE_NNMDREQ;
        }
    }
}
