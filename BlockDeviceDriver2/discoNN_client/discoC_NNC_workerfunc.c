/*
 * Copyright (C) 2010 - 2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_NNClient_ReqProcFunc.c
 *
 * Implement a set of request process functions.
 * The purpose is prevent discoC_NNClient.c become a big file
 */
#include <linux/version.h>
#include <linux/blkdev.h>
#include <net/sock.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/dms_kernel_version.h"
#include "../metadata_manager/metadata_manager.h"
#include "../metadata_manager/metadata_cache.h"
#include "../common/discoC_mem_manager.h"
#include "../config/dmsc_config.h"
#include "../common/thread_manager.h"
#include "../connection_manager/conn_manager_export.h"
#include "discoC_NNC_Manager_api.h"
#include "discoC_NNC_worker.h"
#include "../flowcontrol/FlowControl.h"
#include "discoC_NNClient.h"
#include "discoC_NNMData_Request.h"
#include "discoC_NNMDReq_fsm.h"
#include "discoC_NNC_workerfunc_reportMD.h"
#include "discoC_NN_protocol.h"
#include "discoC_NNAck_acquireMD.h"
#include "discoC_NNAck_reportMD.h"
#include "discoC_NNMDReqPool.h"
#include "../housekeeping.h"
#include "discoC_NNC_Manager.h"
#ifdef DISCO_PREALLOC_SUPPORT
#include "NN_FSReq_ProcFunc.h"
#endif

/*
 * NNClient_wakeup_worker: wake up worker within nnclient
 * @wake_type: what kind of wake up
 *
 * Return: 0: allow caller process request
 *        ~0: caller not allowed to process request
 */
void NNClient_wakeup_worker (wakeup_type_t wake_type)
{
    discoC_NNClient_t *myClient;

    //TODO: we should get client ID
    myClient = NNClientMgr_get_defNNClient();
    if (wake_type == WAKEUP_CM_WORKER || wake_type == WAKEUP_ALL) {
        NNCWker_wakeup_worker(&myClient->nnMDCM_worker);
    }

    if (wake_type == WAKEUP_CH_WORKER || wake_type == WAKEUP_ALL) {
        NNCWker_wakeup_worker(&myClient->nnMDCH_worker);
    }
}

/*
 * _nnc_peekReq_update_state: caller try to gain permission to process request by update state
 * @myMDReq: target request to update state
 *
 * Return: 0: allow caller process request
 *        ~0: caller not allowed to process request
 */
static int32_t _nnc_peekReq_update_state (NNMData_Req_t *myMDReq)
{
    int32_t ret;
    NNMDgetReq_FSM_action_t nnmd_action;
    NNMDciReq_FSM_action_t nncimd_action;

    ret = 0;

    if (NNMDReq_check_acquire(myMDReq)) {
        nnmd_action = NNMDgetReq_update_state(&myMDReq->nnReq_stat, NNREQ_E_PeekWorkQ);
        if (NNREQ_A_DO_SEND != nnmd_action) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN nnMDReq %u fail TR PeekWorkQ "
                    "action %s\n",
                    myMDReq->nnMDReq_comm.ReqID, NNMDgetReq_get_action_str(nnmd_action));
            ret = -EPERM;
        }
    } else {
        nncimd_action = NNMDciReq_update_state(&myMDReq->nnReq_stat,
                NNCIREQ_E_PeekWorkQ);
        if (NNCIREQ_A_DO_SEND != nncimd_action) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN nnMDCIReq %u fail TR PeekWorkQ "
                    "action %s\n",
                    myMDReq->nnMDReq_comm.ReqID, NNMDciReq_get_action_str(nncimd_action));
            ret = -EPERM;
        }
    }

    return ret;
}

/*
 * _nnc_peek_MDReq: peek a request from worker queue and check whether state is allow to
 *                  processed by socket worker
 * @myWker: worker that has queue to peek request
 *
 * Return: peeked request for handling
 */
static NNMData_Req_t *_nnc_peek_MDReq (discoNNC_worker_t *myWker)
{
    NNMData_Req_t *myMDReq;
    struct list_head *list_ptr;
    int32_t lockID;

    while (true) {
        myMDReq = NULL;

        lockID = NNCWker_peekReq_gain_lock(myWker);
        list_ptr = NNCWker_peekReq_workerQ_nolock(lockID, myWker);
        if (unlikely(IS_ERR_OR_NULL(list_ptr))) {
            NNCWker_rel_qlock(lockID, myWker);
            break;
        }

        myMDReq = list_entry(list_ptr, NNMData_Req_t, entry_nnWkerPool);
        NNMDReq_ref_Req(myMDReq);

        if (_nnc_peekReq_update_state(myMDReq)) {
            //NOTE: the state should be TO_handle remove from workq
            if (NNMDReq_test_clear_fea(myMDReq, NNREQ_FEA_IN_WORKQ)) {
                NNCWker_rmReq_workerQ_nolock(lockID, &myMDReq->entry_nnWkerPool, myWker);
            }

            NNCWker_rel_qlock(lockID, myWker);
            NNMDReq_deref_Req(myMDReq);
            myMDReq = NULL;
            break;
        }

        if (NNMDReq_test_clear_fea(myMDReq, NNREQ_FEA_IN_WORKQ) == false) {
            NNCWker_rel_qlock(lockID, myWker);
            dms_printk(LOG_LVL_WARN, "DMSC WARN peek MDReq %u not in worker Q\n",
                    myMDReq->nnMDReq_comm.ReqID);
            NNMDReq_deref_Req(myMDReq);
            myMDReq = NULL;
            break;
        }

        NNCWker_rmReq_workerQ_nolock(lockID, &myMDReq->entry_nnWkerPool, myWker);
        //NOTE: do we need gain caller's data structure? should not (exp: inc ior ref cnt
        NNCWker_rel_qlock(lockID, myWker);

        break;
    }

    return myMDReq;
}

/*
 * _nnc_sendMDReq_check_conn: check whether current connection is ok to send request
 * @conn: target connect to check
 *
 * Return: 0: connection ok
 *        ~0: connection fail
 */
static int32_t _nnc_sendMDReq_check_conn (discoC_conn_t *conn)
{
    if (unlikely(IS_ERR_OR_NULL(conn))) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN null conn NN client\n");
        return -EINVAL;
    }

    if (connMgr_get_connState(conn) != SOCK_CONN) {
        return -ENETUNREACH;
    }

    return 0;
}

/*
 * _nnc_sendMDReq_fccheck: check time passed from last submit request time until now
 *                         if time duration > fc time slot, means has new resource to send within this time slow
 *                         if time duration < fc time: case 1: number of req allow to sent > sent, sent
 *                                                     case 2: number of req allow to sent < sent, enable fc
 * @myClient: which NNClient need check flow control for sending request
 */
static void _nnc_sendMDReq_updatefctime (discoC_NNClient_t *myClient)
{
    uint64_t curr_time;
    uint64_t time_period;

    curr_time = jiffies_64;
    if (time_after64(myClient->nnMDCM_ReqFC.nn_fc_start_t, curr_time)) {
        return;
    }

    time_period = jiffies_to_msecs(curr_time - myClient->nnMDCM_ReqFC.nn_fc_start_t);

    if (time_period >= myClient->nnMDCM_ReqFC.fc_t_slot) {
        myClient->nnMDCM_ReqFC.nn_fc_start_t = curr_time;
        update_all_resource_rate(myClient->nnMDCM_ReqFC.nn_fc_start_t);
    }
}

/*
 * _nnc_sendMDReq_fccheck: consult flow control module and alloc a request bandwidth
 * @myClient: which NNClient need check flow control for sending request
 * @num_reqs: how many request to sent
 * @wait_time: how many time caller should wait when allocate bandwidth fail
 *
 *
 * Return: nn_fc_code_t to indicate different situation
 */
static nn_fc_code_t _nnc_sendMDReq_fccheck (discoC_NNClient_t *myClient,
        int32_t num_reqs, int32_t *wait_time)
{
    int32_t fc_type;

    /* Config'ed flow control */

    fc_type = should_trigger_fc(myClient->nnSrv_resource,
            num_reqs, myClient->nnMDCM_ReqFC.nn_fc_start_t);

    switch (fc_type) {
    case FC_RES_CONNECTION_LOST:
        *wait_time = BASIC_NN_FC_WAIT;
        fc_type = NN_FC_CONN_LOST;
        break;

    case FC_MAX_RES_REQUESTS:
        *wait_time = 100;
        fc_type = NN_FC_MAX_REQS;
        break;

    case FC_NO_BLOCK:
        *wait_time = 0;
        fc_type = NN_FC_TOGO;
        break;

    default:
        *wait_time = fc_type;
        fc_type = NN_FC_REQ_CTL;
        break;
    }

    return fc_type;
}

/*
 * _nnc_sendMDReq_fccongestion: consult flow control module to know when to send metadata request to namenode
 * @nnMDReq: target request to be checked whether allow to sent immediately
 *
 * Case 1: if flow control is disable or request is high priority, caller send request immediately
 * Case 2: if flow control return NN_FC_TOGO, caller send request immediately
 * Case 3: if flow control return value != NN_FC_TOGO, call do_flow_control and thread sleep for a while
 *         after wakeup, check again
 *
 * Return: true: no need process request
 *         false: process request
 */
static bool _nnc_sendMDReq_fccongestion (NNMData_Req_t *nnMDReq)
{
    discoC_NNClient_t *myClient;
    ReqCommon_t *nnReq_comm;
    uint64_t t_s, t_e;
    int64_t period;
    int32_t wait_time;
    nn_fc_code_t fc_action;
    NNMDgetReq_FSM_state_t cur_stat;
    bool ret;

    ret = false;

    myClient = nnMDReq->nnSrv_client;
    _nnc_sendMDReq_updatefctime(myClient);
    if (dms_client_config->enable_fc == false ||
                (PRIORITY_HIGH == nnMDReq->nnMDReq_comm.ReqPrior &&
                        NNMDReq_check_acquire(nnMDReq))) {
        alloc_res_no_fc(myClient->nnSrv_resource, 1);
        return ret;
    }

    t_s = jiffies_64;

#if 0
    if (time_after64(t_s, ior->t_ioReq_start_acquirMD)) {
        period = jiffies_to_msecs(t_s - ior->t_ioReq_start_acquirMD);

        /*
         * TODO: For avoid calculation each io, we should pre-compute warn value,
         *       and modify it when ior_perf_warn be changed.
         *       i.e register an callback function to configuration module
         */
        if (period > dms_client_config->req_qdelay_warn*1000) {
            dms_printk(LOG_LVL_INFO,
                    "DMSC INFO PERF ioreq %u md cm queuing delay %llu secs\n",
                    ior->ioReqID, period / 1000);
        }
    }
#endif

    nnReq_comm = &nnMDReq->nnMDReq_comm;
    while ((fc_action = _nnc_sendMDReq_fccheck(myClient, 1, &wait_time)) != NN_FC_TOGO) {
        switch (fc_action) {
        case NN_FC_CONN_LOST:
            do_flow_control(WAIT_TYPE_FIX_TIME, wait_time);
            break;
        case NN_FC_MAX_REQS:
            do_flow_control(WAIT_TYPE_WAIT_Q, wait_time);
            break;
        case NN_FC_REQ_CTL:
            do_flow_control(WAIT_TYPE_FIX_TIME, wait_time);
            break;
        default:
            dms_printk(LOG_LVL_WARN, "DMSC WARN unknow do fc_action %d\n",
                    fc_action);
            DMS_WARN_ON(true);
            break;
        }

        _nnc_sendMDReq_updatefctime(myClient);

        t_e = jiffies_64;

        if (time_after64(t_e, t_s)) {
            period = jiffies_to_msecs(t_e - t_s);
            if (period > dms_client_config->req_qdelay_warn*1000) {
                dms_printk(LOG_LVL_INFO,
                        "DMSC INFO PERF MDReq %u wait fc %llu secs reason %d\n",
                        nnReq_comm->ReqID, period / 1000, fc_action);
                t_s = t_e;
            }
        }

        cur_stat = NNMDgetReq_get_state(&nnMDReq->nnReq_stat);
        if (NNREQ_S_SENDING != cur_stat) {
            dms_printk(LOG_LVL_INFO, "DMSC WARN MDReq %u cur state %s reject send\n",
                    nnReq_comm->ReqID, NNMDgetReq_get_state_str(&nnMDReq->nnReq_stat));
            ret = true;
            break;
        }

        if (dms_client_config->enable_fc == false) {
            //TODO: will cause wrong value of fc and we should restore to old value
            //TODO: when enable_fc is disable, we should not change value when timeout or got resp
            ret = false;
            break;
        }
    }

    return ret;
}

/*
 * _nnc_send_reportMDReq: send report metadata request to namenode
 *     this function will function in discoC_NNC_workerfunc_reportMD.c
 * @myClient: nn client that request belong to. We need the send buffer in discoC_NNClient_t
 * @nnMDReq: target request to be pack information into packet and send to namenode
 */
static void _nnc_send_reportMDReq (discoC_NNClient_t *myClient, NNMData_Req_t *nnMDReq)
{
    ReqCommon_t *nnReq_comm;
    NNMDciReq_FSM_action_t action;

    nnReq_comm = &nnMDReq->nnMDReq_comm;
    /*
     * TODO: we can move this state machine change to request
     */
    action = NNMDciReq_update_state(&nnMDReq->nnReq_stat, NNCIREQ_E_SendDone);
    if (NNCIREQ_A_DO_WAIT_RESP != action) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN send nnMDReq %u fail TR SendDone "
                "action %s\n",
                nnReq_comm->ReqID, NNMDciReq_get_action_str(action));
        return;
    }

    if (REQ_NN_REPORT_WRITEIO_RESULT == nnReq_comm->ReqOPCode) {
        NNClient_reportMD_writeErr(nnMDReq, myClient->nn_ci_tx_buffer);
        return;
    }

    if (REQ_NN_REPORT_READIO_RESULT == nnReq_comm->ReqOPCode) {
        NNClient_reportMD_readErr(nnMDReq, myClient->nn_ci_tx_buffer);
        return;
    }
}

/*
 * _nnc_send_acquireMDReq: send acquire metadata request to namenode
 * @nnMDReq: target request to be pack information into packet and send to namenode
 */
static void _nnc_send_acquireMDReq (NNMData_Req_t *nnMDReq)
{
    CN2NN_getMDReq_hder_t nn_req_pkt;
    discoC_NNClient_t *myClient;
    ReqCommon_t *nnReq_comm;
    discoC_conn_t *nnConn;
    conn_intf_fn_t *connIntf;
    UserIO_addr_t *ioaddr;
    int32_t sent_bytes;
    NNMDgetReq_FSM_action_t action;
    bool do_simulate;

    nnReq_comm = &nnMDReq->nnMDReq_comm;
    ioaddr = &nnMDReq->nnMDReq_ioaddr;
    dms_printk(LOG_LVL_DEBUG,
            "DMSC DEBUG send MDReq %u vol %u rw %u LB (%llu %u)\n",
            nnReq_comm->ReqID, ioaddr->volumeID, nnReq_comm->ReqOPCode,
            ioaddr->lbid_start, ioaddr->lbid_len);

    action = NNMDgetReq_update_state(&nnMDReq->nnReq_stat, NNREQ_E_SendDone);
    if (NNREQ_A_DO_WAIT_RESP != action) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN send nnMDReq %u fail TR SendDone action %s\n",
                nnReq_comm->ReqID, NNMDgetReq_get_action_str(action));
        return;
    }

    myClient = nnMDReq->nnSrv_client;
    nnConn = myClient->nnSrv_conn;

    NNProto_gen_getMDReqheader(nnReq_comm->ReqOPCode, nnReq_comm->ReqID,
            ioaddr->volumeID, ioaddr->lbid_start, ioaddr->lbid_len, &nn_req_pkt);
    atomic_inc(&myClient->num_nnReq_noResp);

    if (_nnc_sendMDReq_check_conn(nnConn)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN fail send MDReq %u vol %u rw %u LB (%llu %u)\n",
                nnReq_comm->ReqID, ioaddr->volumeID, nnReq_comm->ReqOPCode,
                ioaddr->lbid_start, ioaddr->lbid_len);
        return;
    }

    connIntf = &(nnConn->connOPIntf);

#if 0 //TODO for internal self test, we might need this
    if (unlikely(dms_client_config->clientnode_running_mode ==
                    discoC_MODE_SelfTest ||
                    dms_client_config->clientnode_running_mode ==
                            discoC_MODE_SelfTest_NN_Only)) {
        do_simulate = true;
    }
#endif
    do_simulate = false;

    nnReq_comm->t_sendReq = jiffies_64;
    sent_bytes = connIntf->tx_send(nnConn, &nn_req_pkt, sizeof(CN2NN_getMDReq_hder_t),
            true, do_simulate);
    if (sent_bytes != sizeof(CN2NN_getMDReq_hder_t)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN conn send in-complete MDReq %u vol %u rw %u LB (%llu %u)\n",
                nnReq_comm->ReqID, ioaddr->volumeID, nnReq_comm->ReqOPCode,
                ioaddr->lbid_start, ioaddr->lbid_len);

        connIntf->resetConn(nnConn);
        return;
    }

#if 0 //TODO for internal self test, we might need this
    if (do_simulate) {
        do_nn_selftest(nn_req);
    }
#endif
}

#ifdef DISCO_PREALLOC_SUPPORT
static void send_fs_req (discoC_NNClient_t *myClient, NNMData_Req_t *nnMDReq)
{
    ReqCommon_t *nnReq_comm;
    discoC_conn_t *nnConn;
    conn_intf_fn_t *connIntf;
    uint32_t pkt_size;
    int32_t sent_bytes;
    NNMDciReq_FSM_action_t action;

    nnReq_comm = &nnMDReq->nnMDReq_comm;
    atomic_inc(&myClient->num_nnReq_noResp);

    pkt_size = 0;
    /*
     * TODO: we should move this state machine change
     */
    action = NNMDciReq_update_state(&nnMDReq->nnReq_stat, NNCIREQ_E_SendDone);
    if (NNCIREQ_A_DO_WAIT_RESP != action) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN send prealloc MDReq %u fail TR SendDone "
                "action %s\n",
                nnReq_comm->ReqID, NNMDciReq_get_action_str(action));
        return;
    }

    switch (nnMDReq->nnMDReq_comm.ReqOPCode) {
    case REQ_NN_PREALLOC_SPACE:
        pkt_size = fsReq_send_AllocMData(nnMDReq, myClient->nn_ci_tx_buffer);
        break;
    case REQ_NN_REPORT_SPACE_MDATA:
        pkt_size = fsReq_send_ReportMData(nnMDReq, myClient->nn_ci_tx_buffer);
        break;
    case REQ_NN_RETURN_SPACE:
        pkt_size = fsReq_send_ReportFreeSpace(nnMDReq, myClient->nn_ci_tx_buffer);
        break;
    default:
        dms_printk(LOG_LVL_WARN, "DMSC WARN unknown prealloc request %d\n",
                nnMDReq->nnMDReq_comm.ReqOPCode);
        break;
    }

    myClient = nnMDReq->nnSrv_client;
    nnConn = myClient->nnSrv_conn;
    if (_nnc_sendMDReq_check_conn(nnConn)) {
        dms_printk(LOG_LVL_INFO,
                "DMSC INFO conn broken fail send prealloc MDReq %u callerID %u\n",
                nnMDReq->nnMDReq_comm.ReqID, nnMDReq->nnMDReq_comm.CallerReqID);
        return;
    }

    connIntf = &(nnConn->connOPIntf);

    nnReq_comm->t_sendReq = jiffies_64;
    sent_bytes = connIntf->tx_send(nnConn, myClient->nn_ci_tx_buffer, pkt_size,
            true, false);
    if (sent_bytes != pkt_size) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN conn send in-complete prealloc MDReq\n");
        connIntf->resetConn(nnConn);
        return;
    }
}
#endif

/*
 * NNClient_send_MDReq: function to register to NNClient worker.
 *     this function will peek a metadata request (acquire metadata or report metadata).
 *     call proper handling function base on operation code in request
 *     acquire metadata or report metadata
 *
 *     for each metadata request, perform flow control checking
 */
void NNClient_send_MDReq (discoC_NNClient_t *myClient)
{
    discoNNC_worker_t *myWker;
    NNMData_Req_t *nnMDReq;

    myWker = &myClient->nnMDCM_worker;
    nnMDReq = _nnc_peek_MDReq(myWker);
    if (unlikely(IS_ERR_OR_NULL(nnMDReq))) {
        return;
    }

    //NOTE: not need check again
    //if (unlikely(NNMDReq_MData_cacheHit(nnMDReq))) {
    //    return;
    //}

    switch (nnMDReq->nnMDReq_comm.ReqOPCode) {
    case REQ_NN_QUERY_MDATA_READIO:
    case REQ_NN_QUERY_TRUE_MDATA_READIO:
    case REQ_NN_ALLOC_MDATA_AND_GET_OLD:
    case REQ_NN_ALLOC_MDATA_NO_OLD:
    case REQ_NN_QUERY_MDATA_WRITEIO:
        /*
         * NOTE: when _nnc_sendMDReq_fccongestion return true, means no need process requests
         */
        if (_nnc_sendMDReq_fccongestion(nnMDReq)) {
            return;
        }

        _nnc_send_acquireMDReq(nnMDReq);
        break;
    case REQ_NN_REPORT_READIO_RESULT:
    case REQ_NN_REPORT_WRITEIO_RESULT_DEPRECATED:
    case REQ_NN_REPORT_WRITEIO_RESULT:
        /*
         * NOTE: when _nnc_sendMDReq_fccongestion return true, means no need process requests
         */
        if (_nnc_sendMDReq_fccongestion(nnMDReq)) {
            return;
        }

        _nnc_send_reportMDReq(myClient, nnMDReq);
        break;
    case REQ_NN_UPDATE_HEARTBEAT:
        break;
#ifdef DISCO_PREALLOC_SUPPORT
    case REQ_NN_PREALLOC_SPACE:
    case REQ_NN_REPORT_SPACE_MDATA:
    case REQ_NN_RETURN_SPACE:
        send_fs_req(myClient, nnMDReq);
        break;
#endif
    default:
        dms_printk(LOG_LVL_WARN, "DMSC WARN unknow nn MDReq opcode %d\n",
                nnMDReq->nnMDReq_comm.ReqOPCode);
        break;
    }

    NNMDReq_deref_Req(nnMDReq);
}

/*
 * NNClient_send_getMDReq_cachehit: function to register to NNClient worker.
 *     this function will peek acquire metadata request with metadata from local cache.
 *     Since there are metadata within metadata structure, this function act as a end request function
 *     step 1: stop acquire metadata timer
 *     step 2: remove from request pool
 *     step 3: call callback function to notify caller
 *     step 4: free request
 */
void NNClient_send_getMDReq_cachehit (discoC_NNClient_t *myClient)
{
    discoNNC_worker_t *myWker;
    NNMData_Req_t *nnMDReq;
    ReqCommon_t *nnReq_comm;
    UserIO_addr_t *ioaddr;
    NNMDgetReq_FSM_action_t action;
    int32_t ret;

    myWker = &myClient->nnMDCH_worker;
    nnMDReq = _nnc_peek_MDReq(myWker);
    if (unlikely(IS_ERR_OR_NULL(nnMDReq))) {
        return;
    }

    nnReq_comm = &nnMDReq->nnMDReq_comm;
    action = NNMDgetReq_update_state(&nnMDReq->nnReq_stat, NNREQ_E_SendDone);
    if (NNREQ_A_DO_WAIT_RESP != action) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN nnMDReq %u fail TR SendDone action %s\n",
                nnReq_comm->ReqID, NNMDgetReq_get_action_str(action));
        NNMDReq_deref_Req(nnMDReq);
        return;
    }

    if (NNMDReq_rm_ReqPool(nnMDReq)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN CHit nnMDReq %u fail rm from ReqQ\n", nnReq_comm->ReqID);
        NNMDReq_deref_Req(nnMDReq);
        return;
    }
    NNMDReq_cancel_timer(nnMDReq);

    //NOTE: not need check again
    //if (unlikely(!NNMDReq_MData_cacheHit(nnMDReq))) {
    //    return;
    //}

    ioaddr = &nnMDReq->nnMDReq_ioaddr;
    dms_printk(LOG_LVL_DEBUG,
            "DMSC DEBUG peek NNMData cache hit Req %u vol %u rw %u LB (%llu %u)\n",
            nnReq_comm->ReqID, ioaddr->volumeID, nnReq_comm->ReqOPCode,
            ioaddr->lbid_start, ioaddr->lbid_len);

#if 0 //NOTE: should no need check again
    if (unlikely(IS_ERR_OR_NULL(nnMDReq->MetaData) ||
                    IS_ERR_OR_NULL(nnMDReq->MetaData->array_metadata))) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN null MData array NNMData ch req %u\n",
                nnMDReq->nn_ReqID);
        DMS_WARN_ON(true);

        nnMDReq->nnReq_endfn(nnMDReq->MetaData, nnMDReq->usr_pdata, -ENOMEM);
        return;
    }
#endif

    if ((ret = nnMDReq->nnReq_endfn(nnMDReq->MetaData, nnMDReq->usr_pdata,
            nnReq_comm, 0))) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN fail exec MD req end function ret %d\n", ret);
    }

    NNMDReq_deref_Req(nnMDReq);
    NNMDReq_free_Req(nnMDReq);
}

/*
 * NNClient_sockrcv_thdfn: thread function for socket receive thread.
 *                         listen at socket and receive response from namenode
 * @thread_data: thread private data
 *
 * Return: thread exit code
 */
int NNClient_sockrcv_thdfn (void *thread_data)
{
    cs_wrapper_t *sk_wapper;
    CN2NN_MDResp_hder_t resppkt;
    dmsc_thread_t *rx_thread_data;
    discoC_conn_t *goal;
    conn_intf_fn_t *connIntf;
    discoC_NNClient_t *myClient;
    int32_t conn_ret;
    int8_t *buf_ptr;
    bool is_alloc_buf;

    rx_thread_data = (dmsc_thread_t *)thread_data;
    goal = (discoC_conn_t *)rx_thread_data->usr_data;

    if (IS_ERR_OR_NULL(goal)) {
        dms_printk(LOG_LVL_ERR, "DMSC ERROR nn conn pool item is NULL\n");
        DMS_WARN_ON(true);
        return 0;
    }

    allow_signal(SIGKILL);

    myClient = NNClientMgr_get_NNClient(goal->ipaddr, goal->port);
    connIntf = &(goal->connOPIntf);
    sk_wapper = get_sock_wrapper(goal);

    if (IS_ERR_OR_NULL(sk_wapper)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN fail to get sk_wapper from %s:%d at sk recver",
                goal->ipv4addr_str, goal->port);
        return 0;
    }

    while (kthread_should_stop() == false) {
        if ((conn_ret =
                connIntf->rx_rcv(sk_wapper, &resppkt, sizeof(resppkt))) <= 0) {
            dms_printk(LOG_LVL_INFO,
                    "DMSC INFO receive magic_num from namenode failed\n");
            if (conn_ret == 0) {
                //TODO: why we need a msleep here?
                //      reason: when connection close, rx threads won't
                //              exist but retry to rcv and cause CPU high loading
                //
                msleep(1000);
            }

            continue;
        }

        if (NNProto_MDRespheader_ntoh(&resppkt)) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN wrong nn resp content %llu %d\n",
                    resppkt.magic_number1, resppkt.resp_data_size);
            add_hk_task(TASK_NN_CONN_RESET, (uint64_t *)goal);
            continue;
        }

#ifdef DISCO_PREALLOC_SUPPORT
        //NOTE: in pre-alloc related ack packet, has magic_num2 and request ID not yet be received
        //      in non-pre-alloc related ack packet, no magic_num2, and
        //      NNProto_MDRespheader_ntoh already dec the size by 8 bytes.
        if (resppkt.response_type > NN_RESP_METADATA_COMMIT) {
            resppkt.resp_data_size += 8;
        }
#endif

        is_alloc_buf = false;
        if (resppkt.resp_data_size <= MD_RX_BUFFER_SIZE) {
            buf_ptr = myClient->MDRX_buffer;

            /*
             * NTOE: We need a large buffer for use, worse case will be each
             * HBID has a lr :
             * 8 + 2 + 2 + 8 + 8 + 2 = 30 bytes (magic, type, len, cid, sid, #lr)
             * 257*(2 + 8 + 2 + (2 + 20 + 4 + 2 + 8)*3) = 30840
             */
        } else {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN Not enough space for receive metadata expected: %d\n",
                    resppkt.resp_data_size);
            buf_ptr = discoC_mem_alloc(sizeof(int8_t)*(resppkt.resp_data_size + 128),
                    GFP_NOWAIT | GFP_ATOMIC);
            is_alloc_buf = true;
        }

        if ((conn_ret =
                connIntf->rx_rcv(sk_wapper, buf_ptr, resppkt.resp_data_size)) <= 0) {
            dms_printk(LOG_LVL_INFO, "fail rcv NN ack body\n");

            if (is_alloc_buf) {
                discoC_mem_free(buf_ptr);
            }

            if (conn_ret == 0) {
                //TODO: why we need a msleep here?
                //      reason: when connection close, rx threads won't
                //              exist but retry to rcv and cause CPU high loading
                //
                msleep(1000);
            }

            continue;
        }

        switch (resppkt.response_type) {
        case NN_RESP_METADATA_REQUEST:
            NNAckProc_acquireMDReq(buf_ptr, &resppkt);
            break;
        case NN_RESP_METADATA_COMMIT:
            NNAckProc_reportMDReq(buf_ptr, &resppkt);
            break;
#ifdef DISCO_PREALLOC_SUPPORT
        case NN_RESP_FS_PREALLOCATION:
            fsReq_process_preAllocAck(buf_ptr);
            break;
        case NN_RESP_FS_REPORT_MDATA:
            fsReq_process_ReportMDAck(buf_ptr);
            break;
        case NN_RESP_FS_REPORT_FREE:
            fsReq_process_ReportFreeAck(buf_ptr);
            break;
#endif
        default:
            DMS_WARN_ON(true);
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN unknown nn response type %d\n", resppkt.response_type);
            break;
        }

        if (is_alloc_buf) {
            discoC_mem_free(buf_ptr);
        }
    }

    deRef_conn_sock(sk_wapper);
    return 0;
}
