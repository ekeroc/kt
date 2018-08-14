/*
 * Copyright (C) 2010 - 2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_NNMData_Request.c
 *
 * Provide a set of operations on NN Metadata Request
 *
 */
#include <linux/version.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/dms_kernel_version.h"
#include "../metadata_manager/metadata_manager.h"
#include "../common/discoC_mem_manager.h"
#include "../common/thread_manager.h"
#include "../connection_manager/conn_manager_export.h"
#include "discoC_NNC_worker.h"
#include "../flowcontrol/FlowControl.h"
#include "discoC_NNClient.h"
#include "discoC_NNC_Manager_api.h"
#include "discoC_NNMData_Request.h"
#include "discoC_NNMDReq_fsm.h"
#include "discoC_NNMDReqPool.h"
#include "discoC_NNC_Manager.h"

static void NNMDReq_acquire_TOfunc(worker_t *w_data);
static void NNMDReq_report_TOfunc(worker_t *w_data);

/*
 * NNMDReq_set_fea: set feature bit of ReqFeature
 * @myReq  : target request to be set
 * @fea_bit: which feature be set as 1
 */
void NNMDReq_set_fea (NNMData_Req_t *myReq, uint32_t fea_bit)
{
    set_bit(fea_bit, (volatile void *)&myReq->nnMDReq_comm.ReqFeature);
}

/*
 * NNMDReq_clear_fea: clear feature bit of ReqFeature
 * @myReq  : target request to be set
 * @fea_bit: which feature be set as 0
 */
void NNMDReq_clear_fea (NNMData_Req_t *myReq, uint32_t fea_bit)
{
    clear_bit(fea_bit, (volatile void *)&myReq->nnMDReq_comm.ReqFeature);
}

/*
 * NNMDReq_check_feature: check whether feature on or off
 * @myReq  : target request to be set
 * @fea_bit: which feature be check
 *
 * Return: 0: feature off
 *         1: feature on
 */
bool NNMDReq_check_feature (NNMData_Req_t *myReq, uint32_t fea_bit)
{
    return test_bit(fea_bit,
            (const volatile void *)&myReq->nnMDReq_comm.ReqFeature);
}

/*
 * NNMDReq_test_set_fea: set feature and return old value in atomic operation
 * @myReq  : target request to be set
 * @fea_bit: which feature be check
 *
 * Return: 0: feature origin state is off
 *         1: feature origin state is on
 */
int32_t NNMDReq_test_set_fea (NNMData_Req_t *myReq, uint32_t fea_bit)
{
    return test_and_set_bit(fea_bit,
            (volatile void *)&myReq->nnMDReq_comm.ReqFeature);
}

/*
 * NNMDReq_test_clear_fea: clear feature and return old value in atomic operation
 * @myReq  : target request to be set
 * @fea_bit: which feature be check
 *
 * Return: 0: feature origin state is off
 *         1: feature origin state is on
 */
int32_t NNMDReq_test_clear_fea (NNMData_Req_t *myReq, uint32_t fea_bit)
{
    return test_and_clear_bit(fea_bit,
            (volatile void *)&myReq->nnMDReq_comm.ReqFeature);
}

/*
 * NNMDReq_MData_cacheHit: check whether metadata of request come from local metadata cache
 *                         or remote NN (or no DN information and need acquire from namenode)
 * @myReq  : target request to be check
 *
 * Return: 0: metadata in request doesn't has DN information or metadata from namenode
 *         1: metadata in request has DN information and comes from local cache
 */
bool NNMDReq_MData_cacheHit (NNMData_Req_t *myReq)
{
    return NNMDReq_check_feature(myReq, NNREQ_FEA_CACHE_HIT);
}

/*
 * NNMDReq_MData_hbidErr:
 *     1. caller set this feature so that NNClient will send request with query true metadata operation code
 *     2. metadata in request is a true metadata
 * @myReq  : target request to be check
 *
 * Return: 0: caller need the request to perform query metadata: REQ_NN_QUERY_MDATA_READIO
 *         1: caller need the request to perform query true metadata: REQ_NN_QUERY_TRUE_MDATA_READIO
 */
bool NNMDReq_MData_hbidErr (NNMData_Req_t *myReq)
{
    return NNMDReq_check_feature(myReq, NNREQ_FEA_HBID_ERR);
}

/*
 * NNMDReq_MData_cacheEnable:
 *     1. caller set this feature to indicate NNClient whether need put metadata to local cache
 * @myReq  : target request to be check
 *
 * Return: 0: NNClient don't need put metadata from namenode to local cache
 *         1: NNClient need put metadata from namenode to local cache
 *
 * NOTE: whether caching metadata enable/disable is per-volume. For preventing NNClient module
 *       aware volume data structure, we set this feature.
 */
bool NNMDReq_MData_cacheEnable (NNMData_Req_t *myReq)
{
    return NNMDReq_check_feature(myReq, NNREQ_FEA_MDCACHE_ON);
}

/*
 * NNMDReq_check_acquire: check metadata request is acquire metadata or report metadata
 * @myReq  : target request to be check
 *
 * Return: true: is acquire metadata
 *         false: is report metadata
 */
bool NNMDReq_check_acquire (NNMData_Req_t *myReq)
{
    if (myReq->nnMDReq_comm.ReqOPCode < REQ_NN_REPORT_READIO_RESULT) {
        return true;
    }

    return false;
}

/*
 * NNMDReq_check_alloc: check metadata request is query metadata or allocate metadata
 * @myReq  : target request to be check
 *
 * Return: true: is allocate metadata
 *         false: is query metadata
 */
bool NNMDReq_check_alloc (NNMData_Req_t *myReq)
{
    int16_t nnop;
    bool ret;

    ret = true;
    nnop = myReq->nnMDReq_comm.ReqOPCode;

    do {
        if (REQ_NN_ALLOC_MDATA_NO_OLD == nnop) {
            break;
        }

        if (REQ_NN_ALLOC_MDATA_AND_GET_OLD == nnop) {
            break;
        }

        ret = false;
    } while (0);

    return ret;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 26)
static void NNMDReq_acquire_TO_handler (struct work_struct *work)
{
    NNMDReq_acquire_TOfunc((worker_t *)work);
}

static void NNMDReq_report_TO_handler (struct work_struct *work)
{
    NNMDReq_report_TOfunc((worker_t *)work);
}
#else
static void NNMDReq_acquire_TO_handler (void *work)
{
    NNMDReq_acquire_TOfunc((worker_t *)work);
}

static void NNMDReq_report_TO_handler (void *work)
{
    NNMDReq_report_TOfunc((worker_t *)work);
}
#endif

/*
 * _NNMDReq_TO_chkState: check whether a request can be processed by timeout fire thread
 * @myReq  : target request to be check
 *
 * Return: 0: can be process
 *         ~0: other thread is process request
 * NOTE: unlike payload request, NNClient only schedule timer 1 time. If timeout fire,
 *       if means all time ran out.
 */
static int32_t _NNMDReq_TO_chkState (NNMData_Req_t *myReq)
{
    int32_t lockID, reqID, ret;
    NNMDgetReq_FSM_action_t action;

    ret = 0;
    reqID = myReq->nnMDReq_comm.ReqID;
    lockID = NNMDReqPool_acquire_plock(&nnClient_mgr.MData_ReqPool, reqID);

    do {
        action = NNMDgetReq_update_state(&myReq->nnReq_stat, NNREQ_E_TimeoutFire);
        if (NNREQ_A_DO_TO_PROCESS != action) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN TO NNMDReq %u fail tr NNREQ_E_TimeoutFire\n",
                    reqID);
            ret = -EACCES;
            break;
        }

        if (NNMDReq_test_clear_fea(myReq, NNREQ_FEA_IN_REQPOOL) == false) {
            ret = -EPERM;
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN TO MDReq %u ReqQ not in ReqQ\n", reqID);
            break;
        }

        NNMDReqPool_rmReq_nolock(myReq, &nnClient_mgr.MData_ReqPool);
    } while (0);

    NNMDReqPool_rel_plock(&nnClient_mgr.MData_ReqPool, reqID);

    return ret;
}

/*
 * _NNMDReq_TO_rm_workQ: remove request from worker queue by timeout thread
 * @myReq  : target request to be check
 *
 * Return: 0: can be process
 *         ~0: other thread is process request
 * NOTE: unlike payload request, NNClient only schedule timer 1 time. If timeout fire,
 *       if means all time ran out.
 */
static void _NNMDReq_TO_rm_workQ (NNMData_Req_t *myReq)
{
    discoNNC_worker_t *myWker;
    discoC_NNClient_t *myClient;
    int32_t lockID;

    myClient = myReq->nnSrv_client;
    if (NNMDReq_MData_cacheHit(myReq)) {
        myWker = &myClient->nnMDCH_worker;
    } else {
        myWker = &myClient->nnMDCM_worker;
    }

    lockID = NNCWker_gain_qlock(myReq->nnMDReq_comm.ReqID,
            myReq->nnMDReq_comm.ReqPrior, myWker);
    if (NNMDReq_test_clear_fea(myReq, NNREQ_FEA_IN_WORKQ)) {
        NNCWker_rmReq_workerQ_nolock(lockID, &myReq->entry_nnWkerPool, myWker);
    }
    NNCWker_rel_qlock(lockID, myWker);
}

/*
 * NNMDReq_acquire_TOfunc: timeout handle function for acquiring metadata request
 * @w_data  : worker structure passed by workqueue
 *
 * NOTE: There is a CN-NN flow control mechanism.
 *       When request timeout, we need decrease number of sent request with response value.
 *       So that clientnode can send metadata request prevent bounding by flow control
 *       dec_res_rate_max_reqs
 *       update_res_rate_timeout
 */
static void NNMDReq_acquire_TOfunc (worker_t *w_data) {
    NNMData_Req_t *myReq;
    int32_t ret;

    myReq = (NNMData_Req_t *)w_data->data;
    if (unlikely(IS_ERR_OR_NULL(myReq))) {
        dms_printk(LOG_LVL_WARN, "DMS WARN NNMDReq timer fired with NULL ptr\n");
        DMS_WARN_ON(true);
        return;
    }

    NNMDReq_ref_Req(myReq);
    dms_printk(LOG_LVL_WARN, "DMSC WARN NNMDReq %u timeout fired\n",
            myReq->nnMDReq_comm.ReqID);
    if (_NNMDReq_TO_chkState(myReq)) {
        NNMDReq_deref_Req(myReq);
        return;
    }

    _NNMDReq_TO_rm_workQ(myReq);
    if ((ret = myReq->nnReq_endfn(myReq->MetaData, myReq->usr_pdata,
            &myReq->nnMDReq_comm, -ETIME))) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN fail exec MD req end function ret %d\n", ret);
    }

    dec_res_rate_max_reqs(myReq->nnSrv_client->nnSrv_resource);
    update_res_rate_timeout(myReq->nnSrv_client->nnSrv_resource);

    NNMDReq_deref_Req(myReq);

    //NOTE: we should make sure timerout handler can be sleepable
    NNMDReq_free_Req(myReq);
}

/*
 * NNMDReq_report_TOfunc: timeout handle function for reporting metadata request
 * @w_data  : worker structure passed by workqueue
 *
 * NOTE: For Metadata report request, clientnode will keep
 *       a. waiting response from namenode or
 *       b. resent when CN/NN connection back
 *       the flow control value will be updated when resent.
 *       No need update in timeout function
 */
static void NNMDReq_report_TOfunc (worker_t *w_data) {
    NNMData_Req_t *myReq;

    myReq = (NNMData_Req_t *)w_data->data;
    if (unlikely(IS_ERR_OR_NULL(myReq))) {
        dms_printk(LOG_LVL_WARN, "DMS WARN NNMDCIReq timer fired with NULL ptr\n");
        DMS_WARN_ON(true);
        return;
    }

    NNMDReq_ref_Req(myReq);
    dms_printk(LOG_LVL_WARN,
            "DMSC WARN NNMDCIReq %u timeout fired not yet report namenode\n",
            myReq->nnMDReq_comm.ReqID);
    NNMDReq_deref_Req(myReq);
    //TODO we can show more info here (metadata content)
}

/*
 * _get_MDReq_schedule_time: get process duration for acquire metadata request
 * @myReq  : target request for get processing time
 *
 * Return: process time in seconds
 */
static uint64_t _get_MDReq_schedule_time (NNMData_Req_t *myReq)
{
    uint64_t cur_time, ret_time, t_dur;

    cur_time = jiffies_64;
    if (myReq->t_MDReq_last_submitT == 0) {
        myReq->t_MDReq_last_submitT = cur_time;
        ret_time = (myReq->t_MDReq_rTO_msec + 999)/1000;
    } else {
        if (time_after64(cur_time, myReq->t_MDReq_last_submitT)) {
            t_dur = cur_time - myReq->t_MDReq_last_submitT;
        } else {
            t_dur = 0;
        }

        myReq->t_MDReq_last_submitT = cur_time;
        myReq->t_MDReq_rTO_msec -= jiffies_to_msecs(t_dur);
        ret_time = (myReq->t_MDReq_rTO_msec + 999)/1000;
    }

    if (ret_time <= MIN_NNMDREQ_SCHED_TIME) {
        ret_time = 0L;
    }

    return ret_time*HZ;
}

/*
 * _get_MDCIReq_schedule_time: get process duration for report metadata request
 * @myReq  : target request for get processing time
 *
 * Return: process time in seconds
 */
static uint64_t _get_MDCIReq_schedule_time (NNMData_Req_t *myReq)
{
    return (myReq->t_MDCIReq_rTO_msec/1000)*HZ;
}

/*
 * NNMDReq_sched_timer: schedule timer for metadata request
 * @myReq  : target request to be set timer
 *
 * Return: whether schedule success
 *         0: schedule success
 *        ~0: schedule fail
 */
static int32_t NNMDReq_sched_timer (NNMData_Req_t *myReq)
{
    uint64_t t_dur;
    int32_t ret;
    bool is_mdReq;

    ret = 0;

    is_mdReq = NNMDReq_check_acquire(myReq);
    if (is_mdReq) {
        t_dur = _get_MDReq_schedule_time(myReq);
        DMS_INIT_DELAYED_WORK(&myReq->MDReq_retry_work,
                (void *)NNMDReq_acquire_TO_handler, (void*)myReq);
    } else {
        t_dur = _get_MDCIReq_schedule_time(myReq);
        DMS_INIT_DELAYED_WORK(&myReq->MDReq_retry_work,
                (void *)NNMDReq_report_TO_handler, (void*)myReq);
    }

#ifdef DISCO_PREALLOC_SUPPORT
    //TODO: handle space related timeout
#endif

    if (t_dur == 0) {
        return -ETIME;
    }

    //TODO: please reference payload request mechanism and check again
    if (NNMDReq_test_set_fea(myReq, NNREQ_FEA_SCHED_TIMER)) {
        return -EBUSY;
    }

    queue_delayed_work(myReq->nnSrv_client->nnMDReq_retry_workq,
            (delayed_worker_t *)&(myReq->MDReq_retry_work), t_dur);

    return ret;
}

/*
 * NNMDReq_cancel_timer: stop timer for metadata request
 * @myReq  : target request to be stop timer
 */
void NNMDReq_cancel_timer (NNMData_Req_t *myReq)
{
    worker_t *mywork;

    if (NNMDReq_test_clear_fea(myReq, NNREQ_FEA_SCHED_TIMER)) {
        mywork = &myReq->MDReq_retry_work;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
        if (cancel_delayed_work_sync((delayed_worker_t *)mywork) == false) {
            flush_delayed_work((delayed_worker_t *)mywork);
        }
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 23)
        if (cancel_delayed_work_sync((delayed_worker_t *)mywork) == false) {
            flush_workqueue(myReq->nnSrv_client->nnMDReq_retry_workq);
        }
#else
    // this API will flush the work if cancel failed.
    // cancel_rearming_delayed_work(work);
        if (cancel_delayed_work((delayed_worker_t*)mywork) == 0) {
            flush_workqueue(myReq->nnSrv_client->nnMDReq_retry_workq);
        }
#endif

        mywork->data = NULL;
    }
}

/*
 * NNMDReq_ref_Req: gain reference count of request
 * @myReq  : target request to gain reference
 */
void NNMDReq_ref_Req (NNMData_Req_t *myReq)
{
    atomic_inc(&myReq->nnMDReq_comm.ReqRefCnt);
}

/*
 * NNMDReq_deref_Req: release reference count of request
 * @myReq  : target request to release reference
 */
void NNMDReq_deref_Req (NNMData_Req_t *myReq)
{
    atomic_dec(&myReq->nnMDReq_comm.ReqRefCnt);
}

/*
 * NNMDReq_acquire_retryable: check whether acquire metadata request can be retry
 * @myReq  : target request to check
 *
 * Return: true  can be retry
 *         false cann't be retry
 */
bool NNMDReq_acquire_retryable (NNMData_Req_t *myReq)
{
    NNMDgetReq_FSM_action_t action;
    bool ret;

    ret = false;

    action = NNMDgetReq_update_state(&myReq->nnReq_stat, NNREQ_E_ConnBack);
    if (NNREQ_A_DO_RETRY_REQ == action) {
        ret = true;
    }

    return ret;
}

/*
 * NNMDReq_report_retryable: check whether report metadata request can be retry
 * @myReq  : target request to check
 *
 * Return: true  can be retry
 *         false cann't be retry
 */
bool NNMDReq_report_retryable (NNMData_Req_t *myReq)
{
    NNMDciReq_FSM_action_t action;
    bool ret;

    ret = false;

    action = NNMDciReq_update_state(&myReq->nnReq_stat, NNCIREQ_E_ConnBack);
    if (NNCIREQ_A_DO_RETRY_REQ == action) {
        ret = true;
    }

    return ret;
}

/*
 * _retryReq_update_state: try to update request state to gain permission of retry request
 * @myReq  : target request to check
 *
 * Return: whether update state success or fail
 *         0: success
 *        ~0: fail
 */
static int32_t _retryReq_update_state (NNMData_Req_t *myReq)
{
    int32_t ret;
    NNMDgetReq_FSM_action_t mdReq_action;
    NNMDciReq_FSM_action_t mdCIReq_action;

    ret = 0;

    if (NNMDReq_check_acquire(myReq)) {
        mdReq_action = NNMDgetReq_update_state(&myReq->nnReq_stat, NNREQ_E_RetryChkDone);
        if (NNREQ_A_DO_ADD_WORKQ != mdReq_action) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN fail tr RetryChkDone when retry Req %u\n",
                    myReq->nnMDReq_comm.ReqID);
            ret = -EPERM;
        }
    } else {
        mdCIReq_action = NNMDciReq_update_state(&myReq->nnReq_stat,
                NNCIREQ_E_RetryChkDone);
        if (NNCIREQ_A_DO_ADD_WORKQ != mdCIReq_action) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN fail tr RetryChkDone when retry CIReq %u\n",
                    myReq->nnMDReq_comm.ReqID);
            ret = -EPERM;
        }
    }

    return ret;
}

/*
 * NNMDReq_retry_Req: try to resend a metadata request
 * @myReq  : target request to resend
 *
 */
void NNMDReq_retry_Req (NNMData_Req_t *myReq)
{
    int32_t ret;

    NNMDReq_cancel_timer(myReq);

    if (_retryReq_update_state(myReq)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN fail retry MDReq %u\n",
                myReq->nnMDReq_comm.ReqID);
        //TODO: check again, if fsm reject resend request due to ran out time. what to do?
        return;
    }

    ret = NNMDReq_submit_workerQ(myReq);
    if (-ETIME == ret) {
        if ((ret = myReq->nnReq_endfn(myReq->MetaData, myReq->usr_pdata,
                &myReq->nnMDReq_comm, ret))) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN fail exec MD req end function ret %d\n", ret);
        }

        NNMDReq_deref_Req(myReq);
        NNMDReq_free_Req(myReq);
    } else {
        NNMDReq_deref_Req(myReq);
    }
}

/*
 * NNMDReq_alloc_Req: allocate a metadata request from memory pool
 *
 * Return: allocated metadata request
 */
NNMData_Req_t *NNMDReq_alloc_Req (void)
{
    return (NNMData_Req_t *)discoC_malloc_pool(nnClient_mgr.NNMDReq_mpool_ID);
}

/*
 * _get_NNMDReq_opcode: get metadata operation code for sending request to namenode.
 * @io_rw:    caller is read request or write request
 * @ovw_stat: is a first write or overwrite (read IO ovw_stat is 1)
 *            ovw_state = 0: first write
 *            ovw_state = 1: overwrite
 * @hbid_err: is this request try to query metadata or query true metadata
 * @do_MDReport: is this request a report request
 *
 * Return: operation code (reference: NNMDReq_opcode_t)
 */
static int16_t _get_NNMDReq_opcode (uint16_t io_rw, bool ovw_stat, bool hbid_err,
        bool do_MDReport)
{
    NNMDReq_opcode_t nnreq_op;

    do {
        if (do_MDReport) {
            nnreq_op = (KERNEL_IO_READ == io_rw ?
                    REQ_NN_REPORT_READIO_RESULT : REQ_NN_REPORT_WRITEIO_RESULT);
            break;
        }

        if (KERNEL_IO_READ == io_rw) {
            if (hbid_err) {
                nnreq_op = REQ_NN_QUERY_TRUE_MDATA_READIO;
            } else {
                nnreq_op = REQ_NN_QUERY_MDATA_READIO;
            }
            break;
        }

        if (ovw_stat) {
            nnreq_op = REQ_NN_QUERY_MDATA_WRITEIO;
            break;
        }

        nnreq_op = REQ_NN_ALLOC_MDATA_NO_OLD;
    } while (0);

    return nnreq_op;
}

/*
 * _add_req2pool: add metadata request to pool
 * @myReq:    target request to be added
 *
 */
static void _add_req2pool (NNMData_Req_t *myReq)
{
    int32_t lockID;

    lockID = NNMDReqPool_acquire_plock(&nnClient_mgr.MData_ReqPool,
            myReq->nnMDReq_comm.ReqID);

    do {
        if (NNMDReq_test_set_fea(myReq, NNREQ_FEA_IN_REQPOOL)) {
            break;
        }

        NNMDReqPool_addReq_nolock(myReq, lockID, &nnClient_mgr.MData_ReqPool);
    } while (0);

    NNMDReqPool_rel_plock(&nnClient_mgr.MData_ReqPool, lockID);
}

/*
 * NNMDReq_init_Req: initialize MData Request
 * @myReq:    target request to be initialized
 * @ioaddr: user namespace information (volume ID, start LBA, LBA length) for acquire/report metadata
 * @callerReq:  caller request that want NNClient's service
 * @volReplica: replica number of a volume. Need this for setting metadata state
 * @mdch_stat:  whether this request need acquire metadata from namenode
 *              mdch_stat: 0: means request should processed by cache miss thread
 *              mdch_stat: 1: means request should processed by cache hit thread
 * @ovw_stat: first write or overwrite
 * @hbid_err: caller submit this value to indicate whether request has to query metadata or query true metadata
 * @is_mdcache_on: whether corresponding volume of @myReq is metadata cache enable/disable
 *            0: disable, 1: enable
 * @do_MDReport: whether caller want to perform an acquire or a report
 *
 * Return: 0: initialize success
 *        ~0: initialize fail
 */
int32_t NNMDReq_init_Req (NNMData_Req_t *myReq, UserIO_addr_t *ioaddr,
        ReqCommon_t *callerReq, uint16_t volReplica,
        bool mdch_stat, bool ovw_stat, bool hbid_err, bool is_mdcache_on,
        bool do_MDReport)
{
    ReqCommon_t *nnReq_comm;
    NNMDReq_init_fsm(&myReq->nnReq_stat);

    INIT_LIST_HEAD(&myReq->entry_nnReqPool);
    INIT_LIST_HEAD(&myReq->entry_nnWkerPool);
    INIT_LIST_HEAD(&myReq->entry_retryPool);

    memcpy(&myReq->nnMDReq_ioaddr, ioaddr, sizeof(UserIO_addr_t));

    nnReq_comm = &myReq->nnMDReq_comm;
    nnReq_comm->ReqID = NNClientMgr_alloc_MDReqID();
    nnReq_comm->ReqOPCode = _get_NNMDReq_opcode(callerReq->ReqOPCode, ovw_stat,
            hbid_err, do_MDReport);
    nnReq_comm->ReqPrior = callerReq->ReqPrior;
    nnReq_comm->CallerReqID = callerReq->ReqID;
    nnReq_comm->ReqFeature = 0;
    nnReq_comm->t_sendReq = 0;
    nnReq_comm->t_rcvReqAck = 0;
    atomic_set(&nnReq_comm->ReqRefCnt, 0);

    myReq->MetaData = NULL;

    myReq->t_txMDReport = 0;
    myReq->t_rxMDReportAck = 0;
    myReq->ts_process_MDReport = 0;
    myReq->te_procDone_MDReport = 0;

    if (mdch_stat) {
        NNMDReq_set_fea(myReq, NNREQ_FEA_CACHE_HIT);
    }

    if (hbid_err) {
        NNMDReq_set_fea(myReq, NNREQ_FEA_HBID_ERR);
    }

    if (ovw_stat) {
        NNMDReq_set_fea(myReq, NNREQ_FEA_OVW_STAT);
    }

    if (is_mdcache_on) {
        NNMDReq_set_fea(myReq, NNREQ_FEA_MDCACHE_ON);
    }

    if (REQ_NN_QUERY_TRUE_MDATA_READIO == nnReq_comm->ReqOPCode) {
        NNMDReq_set_fea(myReq, NNREQ_FEA_HBID_ERR);
    }

    //TODO: Currently, we get default NN for performance consideration
    //      For supporting multiple NN, we need get right NNClient (It might impact performance a little)
    //myReq->nnSrv_client = NNClientMgr_get_NNClient(nnSrvIP, nnSrvPort);
    myReq->nnSrv_client = NNClientMgr_get_defNNClient();

    myReq->volReplica = volReplica;
    _add_req2pool(myReq);

    return 0;
}

/*
 * NNMDReq_init_endfn: initialize MData Request's end function, metadata structure and  timeout value
 * @myReq: target request to be initialized
 * @MData: a metadata structure shared with caller. There might has DN information in MData or not.
 *         has DN location in MData: it means MData has metadata from local cache
 *         no DN location in MData : it means caller need NNClient acquire metadata from namenode
 * @endfn: callback function for NNClient to call when finish MDRequest processing
 * @setOVW_fn: set ovw map function for NNClient to call when finish allocate MD Request processing
 * @pdata: private data for endfn
 * @t_dur: time in seconds for schedule request timer
 *
 * Return: 0: initialize success
 *        ~0: initialize fail
 */
int32_t NNMDReq_init_endfn (NNMData_Req_t *myReq, metadata_t *MData,
        nnMDReq_end_fn_t *endfn, nnMDReq_setovw_fn_t *setOVW_fn,
        void *pdata, int32_t t_dur)
{
    int32_t ret;
    NNMDgetReq_FSM_action_t mdreq_action;
    NNMDciReq_FSM_action_t mdcireq_action;

    ret = 0;
    myReq->nnReq_endfn = endfn;
    myReq->nnReq_setOVW_fn = setOVW_fn;
    myReq->usr_pdata = pdata;
    myReq->MetaData = MData;

    if (NNMDReq_check_acquire(myReq)) {
        myReq->t_MDReq_last_submitT = 0;
        myReq->t_MDReq_TO_msec = t_dur*1000;
        myReq->t_MDReq_rTO_msec = t_dur*1000;

        mdreq_action = NNMDgetReq_update_state(&myReq->nnReq_stat, NNREQ_E_InitDone);
        if (NNREQ_A_DO_ADD_WORKQ != mdreq_action) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN nnMDReq %u fail TR with init action %s\n",
                    myReq->nnMDReq_comm.ReqID, NNMDgetReq_get_action_str(mdreq_action));
            ret = -EPERM;
        }
    } else {
        myReq->t_MDCIReq_rTO_msec = t_dur*1000;
        mdcireq_action = NNMDciReq_update_state(&myReq->nnReq_stat,
                NNCIREQ_E_InitDone);
        if (NNCIREQ_A_DO_ADD_WORKQ != mdcireq_action) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN nnMDCIReq %u fail TR with init action %s\n",
                    myReq->nnMDReq_comm.ReqID, NNMDciReq_get_action_str(mdcireq_action));
            ret = -EPERM;
        }
    }

    return ret;
}

/*
 * NNMDReq_submit_workerQ: submit metadata request to worker queue of NNClient
 * @myReq: target request to be added
 *
 * Return: 0: submit success
 *        ~0: submit fail
 */
int32_t NNMDReq_submit_workerQ (NNMData_Req_t *myReq)
{
    discoC_NNClient_t *myClient;
    discoNNC_worker_t *myWker;
    wakeup_type_t who_to_wake;
    int32_t ret, lockID;

    if ((ret = NNMDReq_sched_timer(myReq))) {
        return ret;
    }

    ret = 0;

    myClient = myReq->nnSrv_client;
    if (NNMDReq_check_acquire(myReq)) {
        if (NNMDReq_MData_cacheHit(myReq)) {
            myWker = &myClient->nnMDCH_worker;
            who_to_wake = WAKEUP_CH_WORKER;
        } else {
            myWker = &myClient->nnMDCM_worker;
            who_to_wake = WAKEUP_CM_WORKER;
        }
    } else {
        myWker = &myClient->nnMDCM_worker;
        who_to_wake = WAKEUP_CM_WORKER;
    }

    lockID = NNCWker_gain_qlock(myReq->nnMDReq_comm.ReqID,
            myReq->nnMDReq_comm.ReqPrior, myWker);

    do {
        if (NNMDReq_test_set_fea(myReq, NNREQ_FEA_IN_WORKQ)) {
            ret = -EPERM;
            break;
        }

        NNCWker_addReq_workerQ_nolock(lockID, &myReq->entry_nnWkerPool, myWker);
    } while (0);

    NNCWker_rel_qlock(lockID, myWker);

    NNClient_wakeup_worker(who_to_wake);
    return ret;
}

/*
 * _findReq_update_state: update state for process metadata response from namenode
 * @myReq: target request to be updated state
 *
 * Return: whether caller has permission to process response
 *         0: yes, has permission
 *        ~0: no permission
 */
static int32_t _findReq_update_state (NNMData_Req_t *myReq)
{
    int32_t ret;
    NNMDgetReq_FSM_action_t nnmd_action;
    NNMDciReq_FSM_action_t nnmd_ciaction;

    ret = 0;

    if (NNMDReq_check_acquire(myReq)) {
        nnmd_action = NNMDgetReq_update_state(&myReq->nnReq_stat, NNREQ_E_FindReqOK);
        if (unlikely(NNREQ_A_DO_PROCESS_RESP != nnmd_action)) {
            dms_printk(LOG_LVL_WARN,
                "DMSC WARN process MDReq %u fail tr NNREQ_E_FindReqOK\n",
                myReq->nnMDReq_comm.ReqID);
            ret = -EPERM;
        }
    } else {
        nnmd_ciaction = NNMDciReq_update_state(&myReq->nnReq_stat,
                NNCIREQ_E_FindReqOK);
        if (unlikely(NNCIREQ_A_DO_PROCESS_RESP != nnmd_ciaction)) {
            dms_printk(LOG_LVL_WARN,
                "DMSC WARN process MDReq %u fail tr NNCIREQ_E_FindReqOK\n",
                myReq->nnMDReq_comm.ReqID);
            ret = -EPERM;
        }
    }

    return ret;
}

/*
 * NNMDReq_find_ReqPool: find metadata request from request pool and remove from pool
 * @nnReqID: target request ID to find corresponding request instance
 *
 * NNMData_Req_t: metadata request that match @nnReqID and can be process by caller
 */
NNMData_Req_t *NNMDReq_find_ReqPool (uint32_t nnReqID)
{
    NNMData_Req_t *myReq;
    int32_t lockID;

    lockID = NNMDReqPool_acquire_plock(&nnClient_mgr.MData_ReqPool, nnReqID);

    do {
        myReq = NNMDReqPool_findReq_nolock(nnReqID, lockID, &nnClient_mgr.MData_ReqPool);
        if (IS_ERR_OR_NULL(myReq)) {
            break;
        }

        if (_findReq_update_state(myReq)) {
            myReq = NULL;
            break;
        }

        if (NNMDReq_test_clear_fea(myReq, NNREQ_FEA_IN_REQPOOL) == false) {
            //TODO show message
            myReq = NULL;
            break;
        }

        NNMDReqPool_rmReq_nolock(myReq, &nnClient_mgr.MData_ReqPool);
        NNMDReq_ref_Req(myReq);
    } while (0);

    NNMDReqPool_rel_plock(&nnClient_mgr.MData_ReqPool, lockID);

    return myReq;
}

/*
 * NNMDReq_rm_ReqPool: remove request from pool
 * @nnMDReq: target request to be removed from pool
 *
 * Return: remove success or fail
 *         0: remove success
 *        ~0: remove fail
 */
int32_t NNMDReq_rm_ReqPool (NNMData_Req_t *nnMDReq)
{
    int32_t lockID, ret;
    NNMDgetReq_FSM_action_t action;

    ret = 0;
    lockID = NNMDReqPool_acquire_plock(&nnClient_mgr.MData_ReqPool,
            nnMDReq->nnMDReq_comm.ReqID);

    do {
        action = NNMDgetReq_update_state(&nnMDReq->nnReq_stat, NNREQ_E_FindReqOK);
        if (NNREQ_A_DO_PROCESS_RESP != action) {
            ret = -EPERM;
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN rm MDReq ReqQ %u fail tr NNREQ_E_FindReqOK\n",
                    nnMDReq->nnMDReq_comm.ReqID);
            break;
        }

        if (NNMDReq_test_clear_fea(nnMDReq, NNREQ_FEA_IN_REQPOOL) == false) {
            ret = -EPERM;
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN rm MDReq %u ReqQ not in ReqQ\n",
                    nnMDReq->nnMDReq_comm.ReqID);
            break;
        }

        NNMDReqPool_rmReq_nolock(nnMDReq, &nnClient_mgr.MData_ReqPool);
    } while (0);

    NNMDReqPool_rel_plock(&nnClient_mgr.MData_ReqPool, lockID);

    return ret;
}

/*
 * NNMDReq_free_Req: free request by return request to memory pool
 * @nnMDReq: target request to be freed
 *
 * NOTE: will check reference count to prevent from free request with someone accessing
 * Return: free success or fail
 *         0: free success
 *        ~0: free fail
 */
int32_t NNMDReq_free_Req (NNMData_Req_t *myReq)
{
#define MAX_BUSY_WAIT_CNT   1000
#define SLEEP_WAIT_PERIOD   100 //milli-second
    ReqCommon_t *nnReq_comm;
    int32_t cnt;

    if (unlikely(IS_ERR_OR_NULL(myReq))) {
        return -EINVAL;
    }

    nnReq_comm = &myReq->nnMDReq_comm;
    cnt = 0;
    while (true) {
        if (atomic_read(&nnReq_comm->ReqRefCnt) == 0) {
            break;
        }

        cnt++;
        if (cnt <= MAX_BUSY_WAIT_CNT) {
            //sched_yield();
            yield();
        } else {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN free NNMDReq %u wait refcnt -> 0 (%u %u)\n",
                    nnReq_comm->ReqID,
                    atomic_read(&nnReq_comm->ReqRefCnt), cnt);
            msleep(SLEEP_WAIT_PERIOD);
        }
    }

    discoC_free_pool(nnClient_mgr.NNMDReq_mpool_ID, myReq);

    return 0;
}
