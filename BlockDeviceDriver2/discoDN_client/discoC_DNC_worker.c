/*
 * Copyright (C) 2010 - 2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_DNC_worker.c
 *
 */
#include <linux/version.h>
//#include <linux/blkdev.h>
//#include <net/sock.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/dms_kernel_version.h"
#include "../common/thread_manager.h"
#include "../common/discoC_mem_manager.h"
#include "../metadata_manager/metadata_manager.h"
#include "discoC_DNC_Manager_export.h"
#include "discoC_DNUData_Request.h"
#include "discoC_DNUDReq_fsm.h"
#include "discoC_DNC_worker.h"
#include "discoC_DNC_workerfun.h"
#include "discoC_DNUDReqPool.h"
#include "discoC_DNC_Manager.h"

/*
 * DNCWker_get_numReq_workQ: get number of requests in worker queue
 * @myWker: target worker to get request number
 *
 * Return: number of requests
 * NOTE: Caller guarantee myWker not NULL
 */
int32_t DNCWker_get_numReq_workQ (discoDNC_worker_t *myWker)
{
    return atomic_read(&myWker->wker_ReqQ_size);
}

/*
 * _acquire_DNCWorkQ_lock: get worker queue bucket lock
 * @reqID : request ID to know which bucket might be submit
 * @ptype : request priority to know which bucket might be submit
 * @myWker: socket sender worker that need handle request
 *
 * Return: bucket ID that required when unlock and other operations
 */
static int32_t _acquire_DNCWorkQ_lock (uint32_t reqID, priority_type ptype,
        discoDNC_worker_t *myWker)
{
    spinlock_t *nn_lhead_lock;
    uint32_t hindex;

    if (PRIORITY_LOW == ptype) {
        hindex = (reqID & DNC_WORKQ_MOD_NUM);
    } else {
        hindex = DNC_WORKQ_HP_BK_IDX;
    }

    nn_lhead_lock = &myWker->wker_ReqPlock[hindex];
    spin_lock(nn_lhead_lock);

    return hindex;
}

/*
 * _rel_DNCWorkQ_lock: release worker queue bucket lock
 * @lockID : bucket ID that get from _acquire_DNCWorkQ_lock
 * @myWker: socket sender worker that need handle request
 *
 */
static void _rel_DNCWorkQ_lock (uint32_t lockID, discoDNC_worker_t *myWker)
{
    spinlock_t *nn_lhead_lock;

    nn_lhead_lock = &myWker->wker_ReqPlock[lockID];

    spin_unlock(nn_lhead_lock);
}

/*
 * _addReq_DNCWorkQ_nolock: add request to request queue without lock protection
 * @lockID   : bucket ID that get from _acquire_DNCWorkQ_lock
 * @list_ptr : list_head member of DNRequest
 * @myWker: socket sender worker that need handle request
 *
 */
static void _addReq_DNCWorkQ_nolock (uint32_t lockID, struct list_head *list_ptr,
        discoDNC_worker_t *myWker)
{
    struct list_head *nn_lhead;

    nn_lhead = &myWker->wker_ReqPool[lockID];
    do {
        list_add_tail(list_ptr, nn_lhead);

        if (atomic_add_return(1, &myWker->wker_ReqQ_size) == 1) {
            myWker->deq_bucket_idx = lockID;
        }

        if (DNC_WORKQ_HP_BK_IDX == lockID) {
            atomic_inc(&myWker->wker_HPReqQ_size);
        }
    } while (0);
}

/*
 * _wakeup_DNCWorker: wakeup thread that sleep at wker_waitq
 * @myWker: socket sender worker that should be woke up to handle request
 */
void _wakeup_DNCWorker (discoDNC_worker_t *myWker)
{
    if (waitqueue_active(&myWker->wker_waitq)) {
        wake_up_interruptible_all(&myWker->wker_waitq);
    }
}

/*
 * DNCWker_add_Req_workQ: add DN Request to worker queue
 * @myReq: DN Request be added to queue
 * @wker: which worker to add
 */
void DNCWker_add_Req_workQ (DNUData_Req_t *myReq, discoDNC_worker_t *wker)
{
    int32_t lockID;

    lockID = _acquire_DNCWorkQ_lock(myReq->dnReq_comm.ReqID,
            myReq->dnReq_comm.ReqPrior, wker);

    if (DNUDReq_test_set_fea(myReq, DNREQ_FEA_IN_WORKQ) == false) {
        _addReq_DNCWorkQ_nolock(lockID, &myReq->entry_dnWkerQ, wker);
    } else {
        //TODO: don't print msg when hold spinlock
        dms_printk(LOG_LVL_WARN, "DMSC WARN DNReq already in worker Q\n");
    }

    _rel_DNCWorkQ_lock(lockID, wker);
    _wakeup_DNCWorker(wker);
}

/*
 * _peekReq_acquire_DNCWorkQ_lock: hold a lock for peek a DNRequest
 * @myWker: socket sender worker that should be woke up to handle request
 *
 * Return: bucket ID for later handling
 */
static int32_t _peekReq_acquire_DNCWorkQ_lock (discoDNC_worker_t *myWker)
{
    spinlock_t *lhead_lock;
    uint32_t deq_index;

    if (atomic_read(&myWker->wker_HPReqQ_size) > 0) {
        deq_index = DNC_WORKQ_HP_BK_IDX;
    } else {
        deq_index = myWker->deq_bucket_idx;
        myWker->deq_bucket_idx++;
        myWker->deq_bucket_idx = (myWker->deq_bucket_idx & DNC_WORKQ_MOD_NUM);
    }

    lhead_lock = &myWker->wker_ReqPlock[deq_index];

    spin_lock(lhead_lock);
    return deq_index;
}

/*
 * _peekReq_DNCWorkQ_nolock: peek a request from target bucket
 * @lockID: which bucket that match bucket ID
 * @myWker: target worker that has queue
 *
 * Return: DNRequest that return by list_head
 */
static struct list_head *_peekReq_DNCWorkQ_nolock (int32_t lockID, discoDNC_worker_t *myWker)
{
    struct list_head *lst_ptr, *cur, *next, *lhead;

    lst_ptr = NULL;
    lhead = &myWker->wker_ReqPool[lockID];

    list_for_each_safe (cur, next, lhead) {
        if (unlikely(IS_ERR_OR_NULL(cur))) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN DNClient workerQ list broken\n");
            DMS_WARN_ON(true);
            break;
        }

        lst_ptr = cur;
        break;
    }

    return lst_ptr;
}

/*
 * _rmReq_DNCWorkQ_nolock: remove request from specific bucket without lock protection
 * @lockID: which bucket that match bucket ID
 * @list_ptr: request to be removed from pool
 * @myWker: target worker that has queue
 *
 */
static void _rmReq_DNCWorkQ_nolock (uint32_t lockID,
        struct list_head *list_ptr, discoDNC_worker_t *myWker)
{
    list_del(list_ptr);
    atomic_dec(&myWker->wker_ReqQ_size);

    if (DNC_WORKQ_HP_BK_IDX == lockID) {
        atomic_dec(&myWker->wker_HPReqQ_size);
    }
}

/*
 * _peekReq_update_state: update state to become process by DN worker
 * @myReq: target request to change state
 *
 * Return: whether caller should handle request
 *         0: yes, caller should process request
 *        ~0: no, caller should not process request
 */
static int32_t _peekReq_update_state (DNUData_Req_t *myReq)
{
    int32_t ret;
    DNUDREQ_FSM_action_t action;

    ret = 0;

    action = DNUDReq_update_state(&myReq->dnReq_stat, DNREQ_E_PeekWorkQ);
    if (action != DNREQ_A_DO_SEND) {
        ret = -EPERM;
        dms_printk(LOG_LVL_WARN, "DMSC WARN fail tr dnReq PeekWorkQ %s\n",
                DNUDReq_get_action_str(action));
    }

    return ret;
}

/*
 * _DNCWker_peek_Req_workQ: DNRequest worker peek a request from queue
 * @myWker: which worker queue to handle
 *
 * Return: peeked request from queue
 */
static DNUData_Req_t *_DNCWker_peek_Req_workQ (discoDNC_worker_t *myWker)
{
    DNUData_Req_t *myReq;
    struct list_head *list_ptr;
    int32_t lockID;

    do {
        myReq = NULL;

        lockID = _peekReq_acquire_DNCWorkQ_lock(myWker);
        list_ptr = _peekReq_DNCWorkQ_nolock(lockID, myWker);
        if (unlikely(IS_ERR_OR_NULL(list_ptr))) {
            _rel_DNCWorkQ_lock(lockID, myWker);
            break;
        }

        myReq = list_entry(list_ptr, DNUData_Req_t, entry_dnWkerQ);

        DNUDReq_ref_Req(myReq);
        if (DNUDReq_test_clear_fea(myReq, DNREQ_FEA_IN_WORKQ)) {
            _rmReq_DNCWorkQ_nolock(lockID, &myReq->entry_dnWkerQ, myWker);
        }

        if (_peekReq_update_state(myReq)) {
            DNUDReq_deref_Req(myReq);
            _rel_DNCWorkQ_lock(lockID, myWker);
            myReq = NULL;
            break;
        }

        _rel_DNCWorkQ_lock(lockID, myWker);
    } while (0);

    return myReq;
}

/*
 * _DNCWker_thread_fn: thread function executed by DN Request worker
 * @thread_data: thread private data
 *
 * Return: whether thread end normally
 *         0: thread end normally
 *        ~0: thread end with error
 */
static int32_t _DNCWker_thread_fn (void *thread_data)
{
    dmsc_thread_t *t_data;
    DNClient_MGR_t *myMgr;
    discoDNC_worker_t *myClient;
    DNUData_Req_t *dnReq;
    int8_t *packet;

    t_data = (dmsc_thread_t *)thread_data;
    myMgr = (DNClient_MGR_t *)(t_data->usr_data);
    myClient = &myMgr->dnClient;
    atomic_inc(&myMgr->num_dnClient);

    packet = discoC_mem_alloc(sizeof(int8_t) * DN_PACKET_BUFFER_SIZE, GFP_KERNEL);
    if (IS_ERR_OR_NULL(packet)) {
        dms_printk(LOG_LVL_ERR,
                "DMSC WARN fail to alloc mem for q worker\n");
        DMS_WARN_ON(true);
        goto EXIT_DN_Q_WORKER;
    }

    while (t_data->running) {
        wait_event_interruptible(*(t_data->ctl_wq),
                atomic_read(&myClient->wker_ReqQ_size) > 0 || !(t_data->running));

        dnReq = _DNCWker_peek_Req_workQ(myClient);
        if (IS_ERR_OR_NULL(dnReq)) {
            continue;
        }

        DNCWker_send_DNUDReq(dnReq, packet);
    } // while

EXIT_DN_Q_WORKER:
    if (IS_ERR_OR_NULL(packet) == false) {
        discoC_mem_free(packet);
    }

    atomic_dec(&myMgr->num_dnClient);

    return 0;
}

/*
 * DNCWker_rm_Req_workQ: remove request from worker queue
 * @myReq: request need removed from queue
 * @wker : worker pool
 *
 */
void DNCWker_rm_Req_workQ (DNUData_Req_t *myReq, discoDNC_worker_t *wker)
{
    int32_t lockID;

    lockID = _acquire_DNCWorkQ_lock(myReq->dnReq_comm.ReqID,
                myReq->dnReq_comm.ReqPrior, wker);
    if (DNUDReq_test_clear_fea(myReq, DNREQ_FEA_IN_WORKQ)) {
        _rmReq_DNCWorkQ_nolock(lockID, &myReq->entry_dnWkerQ, wker);
    }

    _rel_DNCWorkQ_lock(lockID, wker);
}

/*
 * DNCWker_init_worker: initialize a worker data structure
 * @wker  : target worker data structure to be initialized
 * @thd_nm: thread name that assigned to worker thread
 * @pData : private data that required by thread function
 *
 * Return: initialize result: 0: OK
 *                           ~0: fail
 *
 */
int32_t DNCWker_init_worker (discoDNC_worker_t *myWker, int8_t *thd_nm, void *pData)
{
    int32_t i, ret;

    ret = 0;

    for (i = 0; i < DNC_WORKQ_TOTAL_NUM; i++) {
        INIT_LIST_HEAD(&myWker->wker_ReqPool[i]);
        spin_lock_init(&myWker->wker_ReqPlock[i]);
    }

    myWker->deq_bucket_idx = 0;

    atomic_set(&myWker->wker_ReqQ_size, 0);
    atomic_set(&myWker->wker_HPReqQ_size, 0);
    init_waitqueue_head(&myWker->wker_waitq);

    myWker->wker_thd = create_worker_thread(thd_nm,
            &(myWker->wker_waitq), pData, _DNCWker_thread_fn);
    if (IS_ERR_OR_NULL(myWker->wker_thd)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN fail create DNClient worker\n");
        DMS_WARN_ON(true);
        ret = -ESRCH;
    }
    return ret;
}

/*
 * DNCWker_stop_worker: stop DNWorker
 * @wker  : target worker data structure to be stop
 *
 * Return: initialize result: 0: OK
 *                           ~0: fail
 */
int32_t DNCWker_stop_worker (discoDNC_worker_t *myWker)
{
    int32_t ret;

    ret = 0;

    dms_printk(LOG_LVL_INFO, "DMSC INFO rel stop DNClient worker\n");
    if (stop_worker_thread(myWker->wker_thd)) {
        dms_printk(LOG_LVL_WARN, "fail to stop DN Request worker\n");
        DMS_WARN_ON(true);
    }
    dms_printk(LOG_LVL_INFO, "DMSC INFO rel stop DNClient worker DONE\n");

    return ret;
}

