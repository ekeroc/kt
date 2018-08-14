/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * payload_request.c
 *
 */
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/delay.h>
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
#include "../discoDN_client/discoC_DNC_Manager_export.h"
#include "payload_Req_IODone.h"
#include "../config/dmsc_config.h"

/*
 * set_PLReq_fea: set feature bit of feature as 1
 * @pReq: target payload request to set feature
 * @fea_bit: which feature bit to set
 */
static void set_PLReq_fea (payload_req_t *pReq, uint32_t fea_bit)
{
    set_bit(fea_bit, (volatile void *)&pReq->plReq_comm.ReqFeature);
}

/*
 * clear_PLReq_fea: clear feature bit of feature as 0
 * @pReq: target payload request to set feature
 * @fea_bit: which feature bit to clear
 */
static void clear_PLReq_fea (payload_req_t *pReq, uint32_t fea_bit)
{
    clear_bit(fea_bit, (volatile void *)&pReq->plReq_comm.ReqFeature);
}

/*
 * check_PLReq_fea: check feature bit of feature is 0 or 1
 * @pReq: target payload request to set feature
 * @fea_bit: which feature bit to clear
 *
 * Return: bit is 0 or 1
 */
static bool check_PLReq_fea (payload_req_t *pReq, uint32_t fea_bit)
{
    return test_bit(fea_bit,
            (const volatile void *)&pReq->plReq_comm.ReqFeature);
}

/*
 * test_and_set_PLReq_fea: set feature and return old value of feature bit
 * @pReq: target payload request to set feature
 * @fea_bit: which feature bit to test_and_set
 *
 * Return: old feature bit is 0 or 1
 */
static int32_t test_and_set_PLReq_fea (payload_req_t *pReq, uint32_t fea_bit)
{
    return test_and_set_bit(fea_bit,
            (volatile void *)&pReq->plReq_comm.ReqFeature);
}

/*
 * test_and_clear_PLReq_fea: clear feature and return old value of feature bit
 * @pReq: target payload request to set feature
 * @fea_bit: which feature bit to test_and_clear
 *
 * Return: old feature bit is 0 or 1
 */
static int32_t test_and_clear_PLReq_fea (payload_req_t *pReq, uint32_t fea_bit)
{
    return test_and_clear_bit(fea_bit,
            (volatile void *)&pReq->plReq_comm.ReqFeature);
}

/*
 * _PLReqTO_DNNoRes_updateFC:
 *     This function will try to notify DNClient which DNRequest still no response
 *     after timeout. DNClient can find out which DN has slow response and
 *     choose other DN for other DN read request
 * @pReq: target payload request that timer fire and ran out of time
 */
static void _PLReqTO_DNNoRes_updateFC (payload_req_t *pReq)
{
    spinlock_t *group_lock;
    uint32_t myReqID;
    int32_t i;

    if (atomic_read(&pReq->num_DNUDReq) == 0) {
        return;
    }

    group_lock = &pReq->DNUDReq_glock;
    spin_lock(group_lock);

    for (i = 0; i < MAX_NUM_REPLICA; i++) {
        myReqID = pReq->arr_DNUDReq[i];
        if (unlikely(myReqID == INVAL_REQID)) {
            continue;
        }
        spin_unlock(group_lock);

        DNClientMgr_update_DNUDReq_TOInfo(myReqID, pReq->plReq_comm.ReqID);

        spin_lock(group_lock);
    }

    spin_unlock(group_lock);
}

/*
 * _rPLReq_TO_try_replica:
 *     A read payload request not yet receive payload from sent datanode request.
 *     After timer fired, try to read data from other datanode.
 * @pReq: target payload request not yet got data and timer fire
 */
static void _rPLReq_TO_try_replica (payload_req_t *pReq)
{
    PLReq_readDN_endfn(pReq->mdata_item, pReq,
            NULL, UData_S_READ_NA, &pReq->plReq_comm,
            pReq->last_readDNbk_idx, pReq->last_readReplica_idx);
}

/*
 * _rPLReq_TO_timeup:
 *     A read payload request not yet receive payload from sent datanode request
 *     and ran out of time
 * @pReq: target payload request not yet got data and timer fire
 *
 * TODO: check again, should use function in payload_Req_IODone.c
 */
static bool _rPLReq_TO_timeup (payload_req_t *pReq)
{
    PLREQ_FSM_action_t action;
    bool do_free;

    do_free = false;
    action = PLReq_update_state(&pReq->plReq_stat, PLREQ_A_DO_CICaller);
    if (action == PLREQ_A_DO_CICaller) {
        _PLReqTO_DNNoRes_updateFC(pReq);
        do_free = true;

        clear_PLReq_fea(pReq, PLREQ_FEA_TIMER_ON);
        PLReq_cancel_DNUDReq(pReq, -1);  //Remove all Callee
#ifdef DISCO_ERC_SUPPORT
        PLReq_cancel_ERCDNUDReq(pReq, -1);
#endif

        PLReq_rm_ReqPool(pReq);  //Remove from request pool
        pReq->plReq_func.rm_PLReq_CallerReq(pReq->user_data,
                &pReq->entry_callerReqPool);
        pReq->plReq_func.PLReq_endReq(pReq->user_data, &pReq->plReq_comm,
                pReq->mdata_item->lbid_s, pReq->mdata_item->num_hbids, false,
                atomic_read(&pReq->num_readFail_DN)); //Remove from caller
    }

    return do_free;
}

/*
 * _readPLReq_TO_handle:
 *     A function to handle read payload request when timer fire
 * @pReq: target payload request not yet got data and timer fire
 *
 */
static bool _readPLReq_TO_handle (payload_req_t *pReq)
{
    bool dofree;

    dofree = false;
    if (pReq->rTO_chk_cnt > 0) {
        _rPLReq_TO_try_replica(pReq);
        //TODO rTO_chk_cnt--
        PLReq_update_state(&pReq->plReq_stat, PLREQ_E_TOFireChkDone);
    } else {
        dofree = _rPLReq_TO_timeup(pReq);
    }

    return dofree;
}

/*
 * _wPLReq_chk_wait_otherReplica:
 *     When a write payload request timer fired and already write data to some replica
 *     this function help to make decision about whether keep waiting
 * @pReq: target payload request not yet got data and timer fire
 *
 * Return: true: keep waiting
 *         false: give up waiting
 */
static bool _wPLReq_chk_wait_otherReplica (payload_req_t *pReq)
{
    uint64_t curr_time, time_wait;
    bool ret;

    do {
        ret = true;

//        if (pReq->t_ciCaller == 0) { //Not yet commit
//            break;
//        }

        curr_time = jiffies_64;
        if (time_after64(pReq->t_ciCaller, curr_time)) { //jiffies overflow
            break;
        }

        time_wait = jiffies_to_msecs(curr_time - pReq->t_ciCaller);
        if (time_wait >= dms_client_config->time_wait_other_replica) {
            ret = false;
            break;
        }
    } while (0);

    return ret;
}

/*
 * _chk_wPLReq_wait:
 *     When a write payload request timer fired, this function make a decision
 *     to decide whether waiting for DN respons
 * @pReq: target payload request not yet got data and timer fire
 *
 * Return: true: keep waiting
 *         false: give up waiting
 *
 * Case 1: ran out of time: no wait
 * Case 2: not yet commit to user, keep waiting
 * Case 3: already commit to user, call _wPLReq_chk_wait_otherReplica to check whether
 *         wait other DN
 */
static bool _chk_wPLReq_wait (payload_req_t *pReq)
{
    bool keepwait;

    keepwait = true;

    do {
        if (pReq->rTO_chk_cnt <= 0) { //time up
            keepwait = false;
            break;
        }

        if (MData_chk_ciUser(pReq->mdata_item) == false) { //not yet ci user, keep wait
            break;
        }

        if (dms_client_config->high_resp_write_io) {
            keepwait = _wPLReq_chk_wait_otherReplica(pReq);
            break;
        }

        //not high resp, keep wait all disk ack
    } while (0);

    return keepwait;
}

/*
 * _writePLReq_TO_handle:
 *        A function to handle write payload request when timer fire
 * @pReq: target payload request not yet got data and timer fire
 *
 * Return: This payload request should finish or not
 *         true: payload request should finish, and caller should free payload request
 *         false: payload request keeping processing request, caller should not free
 */
static bool _writePLReq_TO_handle (payload_req_t *pReq)
{
    PLREQ_FSM_action_t action;
    bool dofree, ciMDS, ciUser, ciUser_ret;

    dofree = false;

    do {
        if (_chk_wPLReq_wait(pReq)) {
            //TODO: keep wait rTO_chk_cnt--
            PLReq_update_state(&pReq->plReq_stat, PLREQ_E_TOFireChkDone);
            break;
        }

        //write timeout handling
        ciMDS = MData_timeout_commitResult(pReq->mdata_item, &ciUser_ret, &ciUser);
        if (ciUser) {
            pReq->t_ciCaller = jiffies_64;
            pReq->plReq_func.ciUser_CallerReq(pReq->user_data,
                    pReq->mdata_item->num_hbids, ciUser_ret);
        }

        action = PLReq_update_state(&pReq->plReq_stat, PLREQ_A_DO_CICaller);
        if (action != PLREQ_A_DO_CICaller) {
            break;
        }

        if (ciMDS) {
            _PLReqTO_DNNoRes_updateFC(pReq);
            clear_PLReq_fea(pReq, PLREQ_FEA_TIMER_ON);
            PLReq_rm_ReqPool(pReq);
            PLReq_cancel_DNUDReq(pReq, -1);  //Remove all Callee
#ifdef DISCO_ERC_SUPPORT
            PLReq_cancel_ERCDNUDReq(pReq, -1);
#endif

            pReq->plReq_func.rm_PLReq_CallerReq(pReq->user_data,
                    &pReq->entry_callerReqPool);
            pReq->plReq_func.PLReq_endReq(pReq->user_data,  //Remove from caller
                    &pReq->plReq_comm, pReq->mdata_item->lbid_s, pReq->mdata_item->num_hbids,
                    false, 0);

            dofree = true;
        }
    } while (0);

    return dofree;
}

/*
 * PLReq_TO_handler_gut:
 *        Timeout handler function for register to work queue and will be executed
 *        when timer fire
 * @w_data: a work queue data structure for timeout thread handling
 */
static void PLReq_TO_handler_gut (worker_t *w_data) {
    payload_req_t *pReq;
    PLREQ_FSM_action_t action;
    bool do_free;

    pReq = (payload_req_t *)w_data->data;
    if (unlikely(IS_ERR_OR_NULL(pReq))) {
        dms_printk(LOG_LVL_WARN, "DMS WARN PLReq timer fired with NULL ptr\n");
        DMS_WARN_ON(true);
        return;
    }

    PLReq_ref_Req(pReq);

    do_free = false;
    pReq->rTO_chk_cnt--;

    do {
        action = PLReq_update_state(&pReq->plReq_stat, PLREQ_E_TOFireChk);
        if (action != PLREQ_A_DO_TOFireChk) {
            //NOTE: it means timeout thread should give up handle this request
            break;
        }

        if (pReq->plReq_comm.ReqOPCode == KERNEL_IO_READ) {
            do_free = _readPLReq_TO_handle(pReq);
            break;
        }

        do_free = _writePLReq_TO_handle(pReq);
    } while (0);

    PLReq_deref_Req(pReq);
    if (do_free) {
        PLMgr_free_PLReq(pReq);
    } else {
        //TODO: PLReq_schedule_timer again
    }
}

/*
 * PLReq_timeout_handler:
 *        Wrap function for register to workqueue
 * @w_data: a work queue data structure for timeout thread handling
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 26)
void PLReq_timeout_handler (struct work_struct *work)
{
    PLReq_TO_handler_gut((worker_t *)work);
}
#else
void PLReq_timeout_handler (void *work)
{
    PLReq_TO_handler_gut((worker_t *)work);
}
#endif

/*
 * PLReq_ref_Req:
 *        increase reference count of payload request so that request won't be free
 *        when someone reference it
 * @pReq: payload request for referencing
 */
void PLReq_ref_Req (payload_req_t *pReq)
{
    atomic_inc(&pReq->plReq_comm.ReqRefCnt);
}

/*
 * PLReq_deref_Req:
 *        decrease reference count of payload request which means a thread no long
 *        touch payload request
 * @pReq: payload request for de-referencing
 */
void PLReq_deref_Req (payload_req_t *pReq)
{
    atomic_dec(&pReq->plReq_comm.ReqRefCnt);
}

/*
 * PLReqFea_send_payload:
 *        check whether this payload request need Tx/Rx data over socket
 * @pReq: payload request for checking
 *
 * Return: true: should tx/rx data over socket
 *        false: no tx/rx data over socket
 */
bool PLReqFea_send_payload (payload_req_t *pReq)
{
    return check_PLReq_fea(pReq, PLREQ_FEA_TXRX_DATA);
}

/*
 * PLReqFea_rw_payload:
 *        check whether this payload request need datanode R/W data from disk
 * @pReq: payload request for checking
 *
 * Return: true: should R/W data from disk
 *        false: shouldn't R/W data from disk
 */
bool PLReqFea_rw_payload (payload_req_t *pReq)
{
    return check_PLReq_fea(pReq, PLREA_FEA_RW_DATA);
}

/*
 * PLReq_allocBK_DNUDReq:
 *        allocate a bucket for holding DNUData Request ID
 * @pReq: payload request for allocating bucket
 *
 * Return: bucket index of arr_DNUDReq
 */
int32_t PLReq_allocBK_DNUDReq (payload_req_t *pReq)
{
    int32_t bkIdx;

    bkIdx = (atomic_add_return(1, &pReq->num_DNUDReq) - 1);
    if (unlikely(bkIdx >= MAX_NUM_REPLICA)) {
        bkIdx = -1;
    }

    return bkIdx;
}

#if 0
void PLReq_add_DNUDReq (payload_req_t *pReq, uint32_t dnReqID, int32_t bk_idx)
{
    spinlock_t *group_lock;

    group_lock = &pReq->DNUDReq_glock;
    pReq->arr_DNUDReq[bk_idx] = dnReqID;
    spin_unlock(group_lock);
}
#endif

/*
 * PLReq_rm_DNUDReq:
 *        remove DN request ID from arr_DNUDReq by marking content in bucket as INVAL_REQID
 * @pReq: payload request for allocating bucket
 * @bk_idx: which bucket index to remove DN Request ID
 *
 */
void PLReq_rm_DNUDReq (payload_req_t *pReq, int32_t bk_idx)
{
    spinlock_t *group_lock;

    group_lock = &pReq->DNUDReq_glock;
    spin_lock(group_lock);
    pReq->arr_DNUDReq[bk_idx] = INVAL_REQID;
    atomic_dec(&pReq->num_DNUDReq);
    spin_unlock(group_lock);
}

#ifdef DISCO_ERC_SUPPORT
void PLReq_rm_ERCDNUDReq (payload_req_t *pReq, int32_t bk_idx)
{
    spinlock_t *group_lock;

    group_lock = &pReq->DNUDReq_glock;
    spin_lock(group_lock);
    pReq->arr_ERCDNUDReq[bk_idx] = INVAL_REQID;
    spin_unlock(group_lock);
}
#endif

/*
 * PLReq_alloc_Req:
 *        allocate a payload request from memory pool
 * @plMPoll_ID: memory pool
 *
 * Return: allocaing result
 */
payload_req_t *PLReq_alloc_Req (int32_t plMPoll_ID)
{
    return (payload_req_t *)discoC_malloc_pool(plMPoll_ID);
}

/*
 * _set_PLReq_TOValue:
 *        calculate timeout value for payload request
 * @myReq: which payload request to calculate timeout value
 * @t_callerWait: time duration in seconds
 *
 */
static void _set_PLReq_TOValue (payload_req_t *myReq, int32_t t_callerWait)
{
#if 0
    //TODO: when a request spent most of time on get metadata,
    //      we give additional time to RW DN
    if (tTotalWait < MIN_PLREQ_IO_TIME) {
        tTotalWait = MIN_PLREQ_IO_TIME;
    }
#endif

    myReq->t_callerWait_PLReq = t_callerWait;
    myReq->t_submit_PLReq = jiffies_64;
    myReq->rTO_chk_cnt = PLREQ_READ_TO_CNT;
    myReq->TO_wait_period = myReq->t_callerWait_PLReq / PLREQ_READ_TO_CNT;
    if (myReq->TO_wait_period == 0) {
        myReq->TO_wait_period = MIN_PLREQ_SCHED_TIME;
    }
}

/*
 * PLReq_init_Req:
 *        initialize payload request
 * @myReq: which payload request to be initialized
 * @mdItem: metadata item that contains information needed by payload request to read/write payload
 * @rw_opcode: read/write operation code
 * @payload_off: payload offset from start of io request
 * @callerReq: caller request for requesting payload manager
 * @fp_map   : a bit map for creating DNRequest, DNRequest will know whether fp_cache has cached fingerprint
 * @fp_cache : a cache for caching fingerprint in DNRequest
 * @send_data: whether this payload request has to tx/rx data through socket
 * @rw_data  : whether this payload request need datanode read/write data from disk
 * @t_callerWait: how many time caller can wait
 * @user_data: user data for payload request submit to callback function
 * @pReq_func: caller define callback function for payload request to call when paylaod request DONE
 *
 */
void PLReq_init_Req (payload_req_t *pReq, metaD_item_t *mdItem,
        uint16_t rw_opcode, uint64_t payload_off,
        ReqCommon_t *callerReq, uint8_t *fp_map, uint8_t *fp_cache,
        bool send_data, bool rw_data, int32_t t_callerWait,
        void *user_data, PLReq_func_t *pReq_func)
{
    ReqCommon_t *plR_comm;

    memset(pReq, 0, sizeof(payload_req_t));

    INIT_LIST_HEAD(&pReq->entry_plReqPool);
    INIT_LIST_HEAD(&pReq->entry_callerReqPool);

    plR_comm = &pReq->plReq_comm;
    plR_comm->ReqID = PLMgr_gen_PLReqID();
    plR_comm->ReqPrior = callerReq->ReqPrior;
    plR_comm->ReqFeature = 0;
    plR_comm->ReqOPCode = rw_opcode;
    plR_comm->CallerReqID = callerReq->ReqID;
    plR_comm->t_sendReq = 0;
    plR_comm->t_rcvReqAck = 0;
    atomic_set(&plR_comm->ReqRefCnt, 0);

    pReq->mdata_item = mdItem;
    pReq->payload_offset = payload_off;

    atomic_set(&pReq->num_readFail_DN, 0);
    spin_lock_init(&pReq->DNUDReq_glock);
    //NOTE: if INVAL_REQID not 0, should not reset by memset
    memset(pReq->arr_DNUDReq, INVAL_REQID, sizeof(uint32_t)*MAX_NUM_REPLICA);
    atomic_set(&pReq->num_DNUDReq, 0);

    pReq->t_ciCaller = 0;
    PLReq_init_fsm(&pReq->plReq_stat);
    _set_PLReq_TOValue(pReq, t_callerWait);

    pReq->ref_fp_map = fp_map;
    pReq->ref_fp_cache = fp_cache;

    if (send_data) {
        set_PLReq_fea(pReq, PLREQ_FEA_TXRX_DATA);
    }

    if (rw_data) {
        set_PLReq_fea(pReq, PLREA_FEA_RW_DATA);
    }

    pReq->user_data = user_data;
    memcpy(&(pReq->plReq_func), pReq_func, sizeof(PLReq_func_t));
}

/*
 * PLReq_free_Req:
 *        free a payload request and return memory to memory pool
 * @preq: target payload to be free
 * @plMPoll_ID: memory pool for return memory
 */
void PLReq_free_Req (payload_req_t *preq, int32_t plMPoll_ID)
{
#define MAX_BUSY_WAIT_CNT   1000
#define SLEEP_WAIT_PERIOD   100 //milli-second
    ReqCommon_t *plR_comm;
    int32_t cnt;

    if (unlikely(IS_ERR_OR_NULL(preq))) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN free null payload req\n");
        DMS_WARN_ON(true);
        return;
    }

    plR_comm = &preq->plReq_comm;
    cnt = 0;
    while (true) {
        if (atomic_read(&plR_comm->ReqRefCnt) == 0) {
            break;
        }

        cnt++;
        if (cnt <= MAX_BUSY_WAIT_CNT) {
            //sched_yield();
            yield();
        } else {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN free DNUDReq %u wait refcnt -> 0 (%u %u)\n",
                    preq->plReq_comm.ReqID,
                    atomic_read(&plR_comm->ReqRefCnt), cnt);
            msleep(SLEEP_WAIT_PERIOD);
        }
    }

#ifdef DISCO_ERC_SUPPORT
    if (IS_ERR_OR_NULL(preq->arr_ERCDNUDReq) == false) {
        discoC_mem_free(preq->arr_ERCDNUDReq);
    }
#endif

    discoC_free_pool(plMPoll_ID, preq);
}

/*
 * _gain_PLReq_timer_ctrl:
 *        try to gain a control to set payload request timer
 * @preq: target payload gain the control to set timer
 *
 * NOTE: this function will keeping retry until gaining control
 *       We cannot let 2 threads to has right to set timer
 */
static void _gain_PLReq_timer_ctrl (payload_req_t *myReq)
{
    while (true) {
        //TODO: Maybe we should show message when stuck for a period, debugging purpose
        if (test_and_set_PLReq_fea(myReq, PLREA_FEA_TIMER_SetCtrl)) {
            yield();
            continue;
        }

        break;
    }
}

/*
 * _rel_PLReq_timer_ctrl:
 *        release control of setting timer
 * @preq: target payload gain the control to set timer
 *
 */
static void _rel_PLReq_timer_ctrl (payload_req_t *myReq)
{
    clear_PLReq_fea(myReq, PLREA_FEA_TIMER_SetCtrl);
}

/*
 * PLReq_schedule_timer:
 *        Schedule timer of payload request
 * @preq: target payload to setup timer
 *
 */
int32_t PLReq_schedule_timer (payload_req_t *myReq)
{
    uint64_t t_dur;
    int32_t ret;

    ret = 0;

    t_dur = myReq->TO_wait_period;
    if (t_dur < MIN_PLREQ_SCHED_TIME) {
        return -ETIME;
    }

    DMS_INIT_DELAYED_WORK(&myReq->PLReq_retry_work,
            (void *)PLReq_timeout_handler, (void *)myReq);

    _gain_PLReq_timer_ctrl(myReq);

    if (test_and_set_PLReq_fea(myReq, PLREQ_FEA_TIMER_ON) == false) {
        queue_delayed_work(g_payload_mgr.PLReq_retry_workq,
                (delayed_worker_t *)&(myReq->PLReq_retry_work), t_dur*HZ);
    } else {
        ret = -EBUSY;
    }

    _rel_PLReq_timer_ctrl(myReq);

    return ret;
}

/*
 * PLReq_cancel_timer:
 *        cancel timer when reqeust is finish
 * @preq: target payload to stop timer
 *
 */
void PLReq_cancel_timer (payload_req_t *myReq)
{
    worker_t *mywork;

    _gain_PLReq_timer_ctrl(myReq);

    if (test_and_clear_PLReq_fea(myReq, PLREQ_FEA_TIMER_ON)) {
        mywork = &myReq->PLReq_retry_work;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
        if (cancel_delayed_work_sync((delayed_worker_t *)mywork) == false) {
            flush_delayed_work((delayed_worker_t *)mywork);
        }
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 23)
        if (cancel_delayed_work_sync((delayed_worker_t *)mywork) == false) {
            flush_workqueue(g_payload_mgr.PLReq_retry_workq);
        }
#else
    // this API will flush the work if cancel failed.
    // cancel_rearming_delayed_work(work);
        if (cancel_delayed_work((delayed_worker_t*)mywork) == 0) {
            flush_workqueue(g_payload_mgr.PLReq_retry_workq);
        }
#endif

        mywork->data = NULL;
    }

    _rel_PLReq_timer_ctrl(myReq);
}

/*
 * PLReq_add_ReqPool:
 *        add payload request to request pool
 * @preq: target payload to add to pool
 *
 * Return: add ok or fail (0: ok, ~0: fail)
 */
int32_t PLReq_add_ReqPool (payload_req_t *pReq)
{
    int32_t bkID, ret;

    ret = 0;

    bkID = PLReqPool_acquire_plock(&g_payload_mgr.PLReqPool, pReq->plReq_comm.ReqID);

    if (test_and_set_PLReq_fea(pReq, PLREQ_FEA_IN_REQPOOL) == false) {
        PLReqPool_addReq_nolock(pReq, bkID, &g_payload_mgr.PLReqPool);
    } else {
        dms_printk(LOG_LVL_INFO,
                "DMSC INFO payload req %u already in req pool\n",
                pReq->plReq_comm.ReqID);
        ret = -EEXIST;
    }

    PLReqPool_rel_plock(&g_payload_mgr.PLReqPool, bkID);

    return ret;
}

/*
 * PLReq_rm_ReqPool:
 *        rm payload request from request pool
 * @preq: target payload to remove from pool
 *
 */
void PLReq_rm_ReqPool (payload_req_t *pReq)
{
    int32_t bkID;

    if (unlikely(IS_ERR_OR_NULL(pReq))) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN rm null payload req from pool\n");
        return;
    }

    bkID = PLReqPool_acquire_plock(&g_payload_mgr.PLReqPool, pReq->plReq_comm.ReqID);

    if (test_and_clear_PLReq_fea(pReq, PLREQ_FEA_IN_REQPOOL)) {
        PLReqPool_rmReq_nolock(pReq, &g_payload_mgr.PLReqPool);
    } else {
        dms_printk(LOG_LVL_INFO,
                "DMSC INFO payload req %u not in req pool\n",
                pReq->plReq_comm.ReqID);
    }

    PLReqPool_rel_plock(&g_payload_mgr.PLReqPool, bkID);
}

#ifdef DISCO_ERC_SUPPORT
/*
 * PLReq_cancel_ERCDNUDReq: use discoDN_client cancel_DNUDReq to cancel all ERC
 *                          related DNUDReq of payload request
 * @pReq:    DNUDReq of pReq to be cancel
 * @bk_skip: which DN Request in bucket should be skip
 *           if value = -1, means cancel all DNUDRequest of payload request
 */
void PLReq_cancel_ERCDNUDReq (payload_req_t *pReq, int32_t bk_skip)
{
    spinlock_t *group_lock;
    uint32_t myReqID, i;

    group_lock = &pReq->DNUDReq_glock;
    spin_lock(group_lock);

    for (i = 0; i < pReq->num_ERCRebuild_DNUDReq; i++) {
        if (i == bk_skip) {
            continue;
        }

        myReqID = pReq->arr_ERCDNUDReq[i];
        if (unlikely(myReqID == INVAL_REQID)) {
            continue;
        }

        pReq->arr_ERCDNUDReq[i] = INVAL_REQID;
        spin_unlock(group_lock);

        DNClientMgr_cancel_DNUDReq(myReqID, pReq->plReq_comm.ReqID);

        spin_lock(group_lock);
    }
    spin_unlock(group_lock);
}
#endif

/*
 * PLReq_cancel_DNUDReq: use discoDN_client cancel_DNUDReq to cancel all
 *                       DNUDReq of payload request
 * @pReq:    DNUDReq of pReq to be cancel
 * @bk_skip: which DN Request in bucket should be skip
 *           if value = -1, means cancel all DNUDRequest of payload request
 */
void PLReq_cancel_DNUDReq (payload_req_t *pReq, int32_t bk_skip)
{
    spinlock_t *group_lock;
    uint32_t myReqID, i;

    group_lock = &pReq->DNUDReq_glock;

    if (bk_skip == -1) {
        if (atomic_read(&pReq->num_DNUDReq) == 0) {
            return;
        }
    } else {
        if (atomic_read(&pReq->num_DNUDReq) == 1) {
            return;
        }
    }

    spin_lock(group_lock);

    for (i = 0; i < MAX_NUM_REPLICA; i++) {
        if (i == bk_skip) {
            continue;
        }

        myReqID = pReq->arr_DNUDReq[i];
        if (unlikely(myReqID == INVAL_REQID)) {
            continue;
        }

        pReq->arr_DNUDReq[i] = INVAL_REQID;
        atomic_dec(&pReq->num_DNUDReq);

        spin_unlock(group_lock);

        DNClientMgr_cancel_DNUDReq(myReqID, pReq->plReq_comm.ReqID);

        spin_lock(group_lock);
    }

    spin_unlock(group_lock);
}
