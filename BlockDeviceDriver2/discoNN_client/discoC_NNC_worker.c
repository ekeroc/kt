/*
 * Copyright (C) 2010 - 2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_NNC_worker.c
 *
 */

#include <linux/version.h>
#include <linux/blkdev.h>
#include <net/sock.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/dms_kernel_version.h"
#include "../common/thread_manager.h"
#include "../connection_manager/conn_manager_export.h"
#include "../flowcontrol/FlowControl.h"
#include "discoC_NNC_worker.h"
#include "discoC_NNClient.h"

/*
 * NNCWker_get_numReq_workQ: get number of requests in worker queue
 * @myWker: target worker to get request number
 *
 * Return: number of requests
 * NOTE: Caller guarantee myWker not NULL
 */
int32_t NNCWker_get_numReq_workQ (discoNNC_worker_t *myWker)
{
    return atomic_read(&myWker->wker_ReqQ_size);
}

/*
 * NNCWker_wakeup_worker: wake up worker thread that sleep at waitqueue
 * @myWker: target worker to wake up
 * NOTE: Caller guarantee myWker not NULL
 */
void NNCWker_wakeup_worker (discoNNC_worker_t *myWker)
{
    if (waitqueue_active(&myWker->wker_waitq)) {
        dms_printk(LOG_LVL_DEBUG,
                "DMSC DEBUG %s wake up NNC wker\n", current->comm);
        wake_up_interruptible_all(&myWker->wker_waitq);
    }
}

/*
 * NNCWker_gain_qlock: gain specific bucket pool's lock
 * @ReqID: base on request to hash into a buck for add
 * @ptype: request's priority
 * @myWker: worker that hold request pool
 */
int32_t NNCWker_gain_qlock (uint32_t reqID, priority_type ptype,
        discoNNC_worker_t *myWker)
{
    spinlock_t *nn_lhead_lock;
    uint32_t hindex;

    if (PRIORITY_LOW == ptype) {
        hindex = (reqID & NNC_WORKQ_MOD_NUM);
    } else {
        hindex = NNC_WORKQ_HP_BK_IDX;
    }

    nn_lhead_lock = &myWker->wker_ReqPlock[hindex];
    spin_lock(nn_lhead_lock);

    return hindex;
}

/*
 * NNCWker_rel_qlock: release specific bucket pool's lock
 * @lockID: which bucket lock to be release
 * @myWker: worker that hold request pool
 */
void NNCWker_rel_qlock (uint32_t lockID, discoNNC_worker_t *myWker)
{
    spinlock_t *nn_lhead_lock;

    nn_lhead_lock = &myWker->wker_ReqPlock[lockID];

    spin_unlock(nn_lhead_lock);
}

/*
 * NNCWker_addReq_workerQ: add a request to worker thread's pool without lock protection
 * @reqID: which request ID of request to be added
 * @ptype: what is the request priority
 * @list_ptr: list_head pointer member of request
 * @myWker: which worker should handle this request
 *
 * When request is high priority, add request to high priority queue.
 * Pool with largest bucket number is high priority pool.
 * When request is normal priority, add request to pool (0 ~ max bucket - 1) with
 * hash way by: Request ID % (number of bucket)
 */
void NNCWker_addReq_workerQ_nolock (uint32_t lockID, struct list_head *list_ptr,
        discoNNC_worker_t *myWker)
{
    struct list_head *nn_lhead;

    nn_lhead = &myWker->wker_ReqPool[lockID];
    do {
        list_add_tail(list_ptr, nn_lhead);

        if (atomic_add_return(1, &myWker->wker_ReqQ_size) == 1) {
            myWker->deq_bucket_idx = lockID;
        }

        if (NNC_WORKQ_HP_BK_IDX == lockID) {
            atomic_inc(&myWker->wker_HPReqQ_size);
        }
    } while (0);
}

/*
 * NNCWker_peekReq_gain_lock: acquire lock for peek request purpose
 * @myWker: worker need to peek a request
 *
 * Peek a request from hashtable need to know which bucket is target.
 * Caller won't know this, so provide a specific function for peek request
 */
int32_t NNCWker_peekReq_gain_lock (discoNNC_worker_t *myWker)
{
    spinlock_t *lhead_lock;
    uint32_t deq_index;

    if (atomic_read(&myWker->wker_HPReqQ_size) > 0) {
        deq_index = NNC_WORKQ_HP_BK_IDX;
    } else {
        deq_index = myWker->deq_bucket_idx;
        myWker->deq_bucket_idx++;
        myWker->deq_bucket_idx = (myWker->deq_bucket_idx & NNC_WORKQ_MOD_NUM);
    }

    lhead_lock = &myWker->wker_ReqPlock[deq_index];

    spin_lock(lhead_lock);
    return deq_index;
}

/*
 * NNCWker_peekReq_workerQ: peek a request from worker thread's pool without lock
 * @myWker: worker need to peek a request
 *
 * Pool with largest bucket number is high priority pool. If has entry in it,
 * it need be peek at first priority.
 * Return: request in list_head * type, caller need translate it into correct data type
 */
struct list_head *NNCWker_peekReq_workerQ_nolock (int32_t lockID, discoNNC_worker_t *myWker)
{
    struct list_head *lst_ptr, *cur, *next, *lhead;

    lst_ptr = NULL;
    lhead = &myWker->wker_ReqPool[lockID];

    list_for_each_safe (cur, next, lhead) {
        lst_ptr = cur;
        break;
    }

    return lst_ptr;
}

/*
 * NNCWker_rmReq_workerQ_nolock: remove a request from worker queue without lock protection
 * @lockID: which bucket of pool this request stay in
 * @list_ptr: list_head pointer member of request
 * @myWker: which worker should handle this request
 */
void NNCWker_rmReq_workerQ_nolock (uint32_t lockID,
        struct list_head *list_ptr, discoNNC_worker_t *myWker)
{
    list_del(list_ptr);
    atomic_dec(&myWker->wker_ReqQ_size);

    if (NNC_WORKQ_HP_BK_IDX == lockID) {
        atomic_dec(&myWker->wker_HPReqQ_size);
    }
}

/*
 * NNCWker_addReq_workerQ: add a request to worker thread's pool
 * @reqID: which request ID of request to be added
 * @ptype: what is the request priority
 * @list_ptr: list_head pointer member of request
 * @myWker: which worker should handle this request
 *
 * When request is high priority, add request to high priority queue.
 * Pool with largest bucket number is high priority pool.
 * When request is normal priority, add request to pool (0 ~ max bucket - 1) with
 * hash way by: Request ID % (number of bucket)
 */
void NNCWker_addReq_workerQ (uint32_t reqID, priority_type ptype,
        struct list_head *list_ptr, discoNNC_worker_t *myWker)
{
    struct list_head *nn_lhead;
    spinlock_t *nn_lhead_lock;
    uint32_t hindex;

    if (PRIORITY_LOW == ptype) {
        hindex = (reqID & NNC_WORKQ_MOD_NUM);
    } else {
        hindex = NNC_WORKQ_HP_BK_IDX;
    }

    nn_lhead = &myWker->wker_ReqPool[hindex];
    nn_lhead_lock = &myWker->wker_ReqPlock[hindex];

    spin_lock(nn_lhead_lock);
    do {
        list_add_tail(list_ptr, nn_lhead);

        if (atomic_add_return(1, &myWker->wker_ReqQ_size) == 1) {
            myWker->deq_bucket_idx = hindex;
        }

        if (NNC_WORKQ_HP_BK_IDX == hindex) {
            atomic_inc(&myWker->wker_HPReqQ_size);
        }

    } while (0);

    spin_unlock(nn_lhead_lock);
}

/*
 * NNCWker_peekReq_workerQ: peek a request from worker thread's pool
 * @myWker: worker need to peek a request
 *
 * Pool with largest bucket number is high priority pool. If has entry in it,
 * it need be peek at first priority.
 * Return: request in list_head * type, caller need translate it into correct data type
 */
struct list_head *NNCWker_peekReq_workerQ (discoNNC_worker_t *myWker)
{
    struct list_head *lst_ptr, *cur, *next;
    struct list_head *lhead;
    spinlock_t *lhead_lock;
    uint32_t deq_index;

    lst_ptr = NULL;

    if (atomic_read(&myWker->wker_HPReqQ_size) > 0) {
        deq_index = NNC_WORKQ_HP_BK_IDX;
    } else {
        deq_index = myWker->deq_bucket_idx;
        myWker->deq_bucket_idx++;
        myWker->deq_bucket_idx = (myWker->deq_bucket_idx & NNC_WORKQ_MOD_NUM);
    }

    lhead = &myWker->wker_ReqPool[deq_index];
    lhead_lock = &myWker->wker_ReqPlock[deq_index];

    spin_lock(lhead_lock);
    list_for_each_safe (cur, next, lhead) {
        list_del(cur);
        lst_ptr = cur;
        break;
    }
    spin_unlock(lhead_lock);

    if (likely(IS_ERR_OR_NULL(lst_ptr) == false)) {
        if (NNC_WORKQ_HP_BK_IDX == deq_index) {
            atomic_dec(&myWker->wker_HPReqQ_size);
        }
        atomic_dec(&myWker->wker_ReqQ_size);
    }

    return lst_ptr;
}

/*
 * _wker_thd_fn: function for passing to linux kernel thread for running
 *               @_wker_thd_fn will execute function caller submit
 *               at function NNCWker_init_worker
 * @thread_data: private data of thread
 *
 * Return: thread exit code : 0: success
 *                           ~0: fail
 */
static int _wker_thd_fn (void *thread_data)
{
    dmsc_thread_t *t_data;
    discoNNC_worker_t *myWker;
    discoC_NNClient_t *nnc_mgr;

    t_data = (dmsc_thread_t *)thread_data;
    myWker = t_data->usr_data;
    nnc_mgr = (discoC_NNClient_t *)myWker->wker_private_data;

    atomic_inc(&nnc_mgr->num_worker);

    while (t_data->running) {
        wait_event_interruptible(*(t_data->ctl_wq),
                atomic_read(&myWker->wker_ReqQ_size) > 0 || !t_data->running);
        myWker->wker_fn(nnc_mgr);
    }

    atomic_dec(&nnc_mgr->num_worker);

    return 0;
}

/*
 * NNCWker_init_worker: initialize NNClient worker and run NNClient worker
 * @myWker: a NNClient worker instance to be initialized and run
 * @thd_nm: thread name for worker
 * @wkerfn: function for worker thread to execute
 * @wkerPData: private data for @wkerfn
 *
 * Return: run worker result: 0: success
 *                           ~0: fail
 */
int32_t NNCWker_init_worker (discoNNC_worker_t *myWker, int8_t *thd_nm,
        NNC_worker_fn *wkerfn, void *wkerPData)
{
    int32_t ret, i;

    ret = 0;

    for (i = 0;i < NNC_WORKQ_TOTAL_NUM; i++) {
        INIT_LIST_HEAD(&myWker->wker_ReqPool[i]);
        spin_lock_init(&myWker->wker_ReqPlock[i]);
    }

    myWker->deq_bucket_idx = 0;

    atomic_set(&myWker->wker_ReqQ_size, 0);
    atomic_set(&myWker->wker_HPReqQ_size, 0);
    myWker->wker_fn = wkerfn;
    myWker->wker_private_data = wkerPData;

    init_waitqueue_head(&myWker->wker_waitq);
    myWker->wker_thd = create_worker_thread(thd_nm,
            &myWker->wker_waitq, myWker, _wker_thd_fn);
    if (IS_ERR_OR_NULL(myWker->wker_thd)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN fail create NN client worker %s\n", thd_nm);
        DMS_WARN_ON(true);
        return -ENOMEM;
    }

    return ret;
}

/*
 * NNCWker_init_worker: initialize NNClient worker and run NNClient worker
 * @myWker: a NNClient worker instance to be initialized and run
 * @thd_nm: thread name for worker
 * @wkerfn: function for worker thread to execute
 * @wkerPData: private data for @wkerfn
 *
 * Return: run worker result: 0: success
 *                           ~0: fail
 * TODO: should try to check whether any requests stay in worker queue
 */
int32_t NNCWker_stop_worker (discoNNC_worker_t *myWker)
{
    int32_t ret;

    ret = stop_worker_thread(myWker->wker_thd);
    if (ret) {
        dms_printk(LOG_LVL_WARN, "fail to stop NN client\n");
        DMS_WARN_ON(true);
    }

    return ret;
}
