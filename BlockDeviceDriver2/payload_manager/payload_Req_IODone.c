/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * payload_Req_IODone.c
 *
 * Provide function for registering to Datanode client as callback function of
 * datanode R/W requests.
 * When datanode request finish R/W request, it will call callback function.
 *
 * Including: Write DN request, Read DN request, Read DN request of non-4K align write
 */
#include <linux/version.h>
#include <linux/blkdev.h>
#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/dms_kernel_version.h"
#include "../common/discoC_mem_manager.h"
#include "../metadata_manager/metadata_manager.h"
#include "../connection_manager/conn_manager_export.h"
#include "../common/thread_manager.h"
#include "payload_manager_export.h"
#include "payload_request_fsm.h"
#include "payload_request.h"
#include "payload_ReqPool.h"
#include "payload_manager.h"
#include "payload_Req_IODone.h"

/*
 * _PLReq_read_ciCaller: commit read payload result to caller request
 *     A payload requests might stays in following data structure, need remove from it
 *     1. Caller's callee pool
 *     2. payload request pool
 * @mdItem: need metadata item to notify caller which lbs be done
 * @pReq: which payload request finish read process
 * @ioResult: io result of payload read data
 *
 * Return: result of commit to caller
 */
static int32_t _PLReq_read_ciCaller (metaD_item_t *mdItem, payload_req_t *pReq,
        bool ioResult)
{
    int32_t ret;

    PLReq_rm_ReqPool(pReq);  //NOTE: REQ DONE chk criteria 2: Remove from request pool
    //NOTE: REQ DONE chk criteria 3: Remove from from caller request
    pReq->plReq_func.rm_PLReq_CallerReq(pReq->user_data, &pReq->entry_callerReqPool);
    ret = pReq->plReq_func.PLReq_endReq(pReq->user_data, &pReq->plReq_comm,
            mdItem->lbid_s, mdItem->num_hbids, ioResult,
            atomic_read(&pReq->num_readFail_DN));

    //NOTE: REQ DONE chk criteria 4: Remove from worker's queue (payload req no worker)

    return ret;
}

/*
 * _PLReq_read_nextDN: try to read data from other DN when DN report disk failure
 *     or HBID error. If already sent request to all DNs, check whether
 *     any one of them is connection lost.
 *     Case 1: Yes, keep waiting until connection back or payload request timeout
 *     Case 2: No, means all DNs has disk error for that metadata or all DNs reply hbid error
 *
 *     When MData_check_ciUserStatus return not keep waiting, finish payload request
 * @mdItem: need metadata item for knowing keep waiting or not.
 * @pReq: which payload request got disk error or hbid error that need read from other DN
 * @dnLoc_idx: which DN with dn loc index in metadata report disk error or hbid error
 * @bk_idx: which bucket of payload request hold DN's ID
 *
 * Return: result of commit to caller
 *
 * NOTE: graceful way to stop a payload request should contains
 *       a. stop timer   b. cancel all DN Requests related to payload requests
 *       c. remove from payload request pool
 *       d. remove from caller's callee pool
 *       e. commit to caller
 *       f. free requests
 */
static bool _PLReq_read_nextDN (metaD_item_t *mdItem, payload_req_t *pReq,
        int32_t dnLoc_idx, int32_t bk_idx)
{
    int32_t ret;
    PLREQ_FSM_action_t action;
    bool keep_wait, ciUser_ret;

    ret = PLMgr_submit_DNUDReadReq(mdItem, pReq, dnLoc_idx);
    if (ret < 0) { //NOTE: generate another DN to read data
        return false;
    }

    //NOTE: already sent all replica, check whether some replica is conn lost
    keep_wait = MData_check_ciUserStatus(mdItem, &ciUser_ret);
    if (keep_wait) {
        return false;
    }

    action = PLReq_update_state(&pReq->plReq_stat, PLREQ_E_CICaller);
    if (PLREQ_A_DO_CICaller == action) {
        PLReq_cancel_timer(pReq);
        PLReq_cancel_DNUDReq(pReq, bk_idx);

        ret = _PLReq_read_ciCaller(mdItem, pReq, ciUser_ret);

        return true;
    }

    return false;
}

/*
 * _PLReq_1st_hbidErr: handle 1st time got DN report hbid errors
 *
 * @mdItem: need metadata item for committing to caller
 * @pReq: which payload request got disk error or hbid error that need read from other DN
 * @dnLoc_idx: which DN with dn loc index in metadata report disk error or hbid error
 * @bk_idx: which bucket of payload request hold DN's ID
 *
 * Return: whether commit to caller
 *
 * fixme: should remove from payload request pool
 *        remove from caller's callee pool (?)
 */
static bool _PLReq_1st_hbidErr (metaD_item_t *mdItem, payload_req_t *pReq,
        int32_t dnLoc_idx, int32_t bk_idx)
{
    PLREQ_FSM_action_t action;
    bool ret;

    ret = false;

    action = PLReq_update_state(&pReq->plReq_stat, PLREQ_E_CICaller);
    if (PLREQ_A_DO_CICaller == action) {
        //TODO: we can choose not to cancel timer if not all request got hbid err
        PLReq_cancel_timer(pReq);
        PLReq_cancel_DNUDReq(pReq, bk_idx);
        pReq->plReq_func.reportMDErr_CallerReq(pReq->user_data,
                mdItem->lbid_s, mdItem->num_hbids);

        ret = true;
    }

    return ret;
}

/*
 * _PLReq_readDN_NonTrueMD: process read ack for non-true metadata
 *
 * @mdItem: need metadata item recording DN status and decide whether keeping process or finish
 * @pReq: which payload request got disk error or hbid error that need read from other DN
 * @payload: payload from DN
 * @result: ack result from datanode
 * @dnLoc_idx: which DN with dn loc index in metadata report disk error or hbid error
 * @bk_idx: which bucket of payload request hold DN's ID
 *
 * Return: whether any errors when process DN ack
 *         0: OK
 *        ~0: failure
 */
static int32_t _PLReq_readDN_NonTrueMD (metaD_item_t *mdItem,
        payload_req_t *pReq, data_mem_chunks_t *payload,
        int32_t result, int32_t bk_idx, int32_t dnLoc_idx)
{
    int32_t ret;
    PLREQ_FSM_action_t action;
    bool do_free;

    ret = 0;
    do_free = false;

    switch (result) {
    case UData_S_READ_OK:
        PLReq_rm_DNUDReq(pReq, bk_idx); //NOTE: REQ DONE chk criteria 1: Remove all callee request

        action = PLReq_update_state(&pReq->plReq_stat, PLREQ_E_CICaller);
        if (PLREQ_A_DO_CICaller == action) {
            MData_set_ciUser(mdItem);
            PLReq_cancel_timer(pReq);
            pReq->plReq_func.cpData_CallerReq(pReq->user_data, mdItem,
                    pReq->payload_offset, payload);
            PLReq_cancel_DNUDReq(pReq, bk_idx);
#ifdef DISCO_ERC_SUPPORT
            PLReq_cancel_ERCDNUDReq(pReq, -1);
#endif
            ret = _PLReq_read_ciCaller(mdItem, pReq, true);

            do_free = true;
        }
        break;
    case UData_S_READ_NA:
        if (mdItem->num_replica > 1) {
            ret = PLMgr_submit_DNUDReadReq(mdItem, pReq, dnLoc_idx);
        } else {
            PLReq_rm_DNUDReq(pReq, bk_idx);
            //TODO: we can choose don't free this payload request and wait for response
            //NOTE: target DN N/A, policy get true metadata that in case NN finish RR or has ERC protection
            do_free = _PLReq_1st_hbidErr(mdItem, pReq, dnLoc_idx, bk_idx);
        }
        break;
    case UData_S_MD_ERR:
        PLReq_rm_DNUDReq(pReq, bk_idx);
        //inval_volume_MDCache_LBID(ioSegReq->volume_ID, ioSegReq->LB_start, ioSegReq->LB_len);
        do_free = _PLReq_1st_hbidErr(mdItem, pReq, dnLoc_idx, bk_idx);

        break;
    case UData_S_DISK_FAILURE:
        PLReq_rm_DNUDReq(pReq, bk_idx);
        atomic_inc(&pReq->num_readFail_DN);

        //inval_volume_MDCache_LBID(ioSegReq->volume_ID, ioSegReq->LB_start, ioSegReq->LB_len);
        if (mdItem->num_replica > 1) {
            do_free = _PLReq_read_nextDN(mdItem, pReq, dnLoc_idx, bk_idx);
        } else {
            do_free = _PLReq_1st_hbidErr(mdItem, pReq, dnLoc_idx, bk_idx);
        }

        break;
    default:
        break;
    }

    PLReq_deref_Req(pReq);
    if (do_free) {
        PLMgr_free_PLReq(pReq);  //NOTE: REQ DONE chk criteria 5: check reference count
    }

    return ret;
}

/*
 * _PLReq_readDN_TrueMD_REP: process read ack for true metadata and data protection is
 *    replica protection
 *
 * @mdItem: need metadata item recording DN status and decide whether keeping process or finish
 * @pReq: which payload request that own ack from DNClient
 * @payload: payload from DN
 * @result: ack result from datanode
 * @dnLoc_idx: which DN with dn loc index in metadata report disk error or hbid error
 * @bk_idx: which bucket of payload request hold DN's ID
 *
 * Return: whether any errors when process DN ack
 *         0: OK
 *        ~0: failure
 */
static int32_t _PLReq_readDN_TrueMD_REP (metaD_item_t *mdItem,
        payload_req_t *pReq, data_mem_chunks_t *payload,
        int32_t result, int32_t bk_idx, int32_t dnLoc_idx)
{
    int32_t ret;
    PLREQ_FSM_action_t action;
    bool do_free;

    ret = 0;
    do_free = false;

    switch (result) {
    case UData_S_READ_OK:
        PLReq_rm_DNUDReq(pReq, bk_idx); //NOTE: REQ DONE chk criteria 1: Remove all callee request

        action = PLReq_update_state(&pReq->plReq_stat, PLREQ_E_CICaller);
        if (PLREQ_A_DO_CICaller == action) {
            MData_set_ciUser(mdItem);
            PLReq_cancel_timer(pReq);
            pReq->plReq_func.cpData_CallerReq(pReq->user_data, mdItem,
                    pReq->payload_offset, payload);
            PLReq_cancel_DNUDReq(pReq, bk_idx);
            ret = _PLReq_read_ciCaller(mdItem, pReq, true);

            do_free = true;
        }
        break;
    case UData_S_READ_NA:
        ret = PLMgr_submit_DNUDReadReq(mdItem, pReq, dnLoc_idx);
        break;
    case UData_S_MD_ERR:
        PLReq_rm_DNUDReq(pReq, bk_idx);
        //inval_volume_MDCache_LBID(ioSegReq->volume_ID, ioSegReq->LB_start, ioSegReq->LB_len);
        do_free = _PLReq_read_nextDN(mdItem, pReq, dnLoc_idx, bk_idx);

        break;
    case UData_S_DISK_FAILURE:
        PLReq_rm_DNUDReq(pReq, bk_idx);
        atomic_inc(&pReq->num_readFail_DN);

        //inval_volume_MDCache_LBID(ioSegReq->volume_ID, ioSegReq->LB_start, ioSegReq->LB_len);
        do_free = _PLReq_read_nextDN(mdItem, pReq, dnLoc_idx, bk_idx);

        break;
    default:
        break;
    }

    PLReq_deref_Req(pReq);
    if (do_free) {
        PLMgr_free_PLReq(pReq);  //NOTE: REQ DONE chk criteria 5: check reference count
    }

    return ret;
}

#ifdef DISCO_ERC_SUPPORT
static int32_t _PLReq_ReadDN_TrueMD_ERC (metaD_item_t *mdItem,
        payload_req_t *pReq, data_mem_chunks_t *payload,
        int32_t result, int32_t bk_idx, int32_t dnLoc_idx)
{
    int32_t ret;
    PLREQ_FSM_action_t action;
    bool do_free;

    ret = 0;
    do_free = false;

    switch (result) {
    case UData_S_READ_OK:
        PLReq_rm_DNUDReq(pReq, bk_idx); //NOTE: REQ DONE chk criteria 1: Remove all callee request

        action = PLReq_update_state(&pReq->plReq_stat, PLREQ_E_CICaller);
        if (PLREQ_A_DO_CICaller == action) {
            MData_set_ciUser(mdItem);
            PLReq_cancel_timer(pReq);
            pReq->plReq_func.cpData_CallerReq(pReq->user_data, mdItem,
                    pReq->payload_offset, payload);
            PLReq_cancel_DNUDReq(pReq, bk_idx);
            PLReq_cancel_ERCDNUDReq(pReq, -1);
            ret = _PLReq_read_ciCaller(mdItem, pReq, true);

            do_free = true;
        }
        break;
    case UData_S_READ_NA:
        ret = PLMgr_submit_ERCRebuild_DNUDReq(mdItem, pReq);

        break;
    case UData_S_MD_ERR:
        PLReq_rm_DNUDReq(pReq, bk_idx);
        ret = PLMgr_submit_ERCRebuild_DNUDReq(mdItem, pReq);

        break;
    case UData_S_DISK_FAILURE:
        PLReq_rm_DNUDReq(pReq, bk_idx);
        atomic_inc(&pReq->num_readFail_DN);
        ret = PLMgr_submit_ERCRebuild_DNUDReq(mdItem, pReq);

        break;
    default:
        break;
    }

    PLReq_deref_Req(pReq);
    if (do_free) {
        PLMgr_free_PLReq(pReq);  //NOTE: REQ DONE chk criteria 5: check reference count
    }

    return ret;
}

static int32_t _xor_computing (metaD_item_t *mdItem, data_mem_chunks_t *payload)
{
    uint64_t *dest_ptr, *src_ptr;
    int32_t num_dn_wait;
    int32_t i, totalLen;

    mdItem->erc_cnt_dnresp++;
    num_dn_wait = mdItem->num_dnLoc - mdItem->erc_cnt_dnresp - 1;

    do {
        if (mdItem->erc_cnt_dnresp == 1) {
            memcpy(mdItem->erc_buffer, payload->chunks[0]->data_ptr, PAGE_SIZE);
            break;
        }

        if (num_dn_wait > 0) {
            dest_ptr = (uint64_t *)mdItem->erc_buffer;
            src_ptr = (uint64_t *)(payload->chunks[0]->data_ptr);
        } else if (num_dn_wait == 0) {
            dest_ptr = (uint64_t *)(payload->chunks[0]->data_ptr);
            src_ptr = (uint64_t *)mdItem->erc_buffer;
        } else {
            dms_printk(LOG_LVL_WARN, "DMSC WARN num dn wait ERC %d < 0\n",
                    num_dn_wait);
            break;
        }

        totalLen = PAGE_SIZE >> 3; //PAGE_SIZE / sizeof(uint64_t)
        for (i = 0; i < totalLen; i++) {
            *dest_ptr = *dest_ptr ^ *src_ptr;
            dest_ptr++;
            src_ptr++;
        }

    } while (0);

    return num_dn_wait;
}

static int32_t _PLReq_ReadDN_TrueMD_ERC_rebuild (metaD_item_t *mdItem,
        payload_req_t *pReq, data_mem_chunks_t *payload,
        int32_t result, int32_t bk_idx, int32_t dnLoc_idx)
{
    int32_t ret, num_dn_wait;
    PLREQ_FSM_action_t action;
    bool do_free;

    ret = 0;
    do_free = false;

    switch (result) {
    case UData_S_READ_OK:
        PLReq_rm_ERCDNUDReq(pReq, bk_idx);
        mutex_lock(&mdItem->erc_buff_lock);
        num_dn_wait = _xor_computing(mdItem, payload);
        mutex_unlock(&mdItem->erc_buff_lock);

        if (num_dn_wait != 0) {
            break;
        }

        action = PLReq_update_state(&pReq->plReq_stat, PLREQ_E_CICaller);
        if (PLREQ_A_DO_CICaller == action) {
            MData_set_ciUser(mdItem);
            PLReq_cancel_timer(pReq);
            pReq->plReq_func.cpData_CallerReq(pReq->user_data, mdItem,
                    pReq->payload_offset, payload);

            PLReq_cancel_ERCDNUDReq(pReq, bk_idx);
            PLReq_cancel_DNUDReq(pReq, -1);
            ret = _PLReq_read_ciCaller(mdItem, pReq, true);

            do_free = true;
        }
        break;
    case UData_S_READ_NA:
        //TODO: support READ NA of ERC rebuild
        //ret = PLMgr_submit_DNUDReadReq(mdItem, pReq, dnLoc_idx);
        break;
    case UData_S_MD_ERR:
        PLReq_rm_ERCDNUDReq(pReq, bk_idx);
        action = PLReq_update_state(&pReq->plReq_stat, PLREQ_E_CICaller);
        if (PLREQ_A_DO_CICaller == action) {
            PLReq_cancel_timer(pReq);
            PLReq_cancel_ERCDNUDReq(pReq, bk_idx);
            PLReq_cancel_DNUDReq(pReq, -1);

            //TODO: double confirm if previous read target DN is N/A and ack already
            ret = _PLReq_read_ciCaller(mdItem, pReq, false);

            do_free = true;
        }

        break;
    case UData_S_DISK_FAILURE:
        PLReq_rm_ERCDNUDReq(pReq, bk_idx);
        atomic_inc(&pReq->num_readFail_DN);

        action = PLReq_update_state(&pReq->plReq_stat, PLREQ_E_CICaller);
        if (PLREQ_A_DO_CICaller == action) {
            PLReq_cancel_timer(pReq);
            PLReq_cancel_ERCDNUDReq(pReq, bk_idx);
            PLReq_cancel_DNUDReq(pReq, -1);

            //TODO: double confirm if previous read target DN is N/A and ack already
            ret = _PLReq_read_ciCaller(mdItem, pReq, false);

            do_free = true;
        }
        break;
    default:
        break;
    }

    PLReq_deref_Req(pReq);
    if (do_free) {
        PLMgr_free_PLReq(pReq);  //NOTE: REQ DONE chk criteria 5: check reference count
    }

    return ret;
}
#endif

/*
 * PLReq_readDN_endfn: function to handle datanode ack from DNClient modules.
 *    Register to DNClient as callback function when submit request to DNClient
 *
 * @mdItem: need metadata item recording DN status and decide whether keeping process or finish
 * @private_data: callback argument that pass to DNClient when submit requests
 * @payload: payload from DN
 * @result: ack result from datanode
 * @calleeReq: DNUData Request
 * @dnLoc_idx: which DN with dn loc index in metadata report disk error or hbid error
 * @bk_idx: which bucket of payload request hold DN's ID
 *
 * Return: whether any errors when process DN ack
 *         0: OK
 *        ~0: failure
 */
int32_t PLReq_readDN_endfn (metaD_item_t *mdItem, void *private_data,
        data_mem_chunks_t *payload, int32_t result,
        ReqCommon_t *calleeReq, int32_t bk_idx, int32_t dnLoc_idx)
{
    payload_req_t *pReq;
    int32_t ret;

    pReq = (payload_req_t *)private_data;
    PLReq_ref_Req(pReq);

    //NOTE: it's ok to update without lock, we don't need accuracy
    if (pReq->plReq_comm.t_sendReq == 0) {
        pReq->plReq_comm.t_sendReq = calleeReq->t_sendReq;
    }
    pReq->plReq_comm.t_rcvReqAck = calleeReq->t_rcvReqAck;

    ret = 0;

    do {
#ifdef DISCO_ERC_SUPPORT
        if (MData_chk_ERCProt(mdItem)) {
            MData_update_DNUDataStat(&mdItem->erc_dnLoc[dnLoc_idx], result);
            break;
        }
#endif
        MData_update_DNUDataStat(&mdItem->udata_dnLoc[dnLoc_idx], result);
    } while (0);

    do {
        if (MData_chk_TrueMD(mdItem) == false) {
            ret = _PLReq_readDN_NonTrueMD(mdItem, pReq, payload, result,
                    bk_idx, dnLoc_idx);
            break;
        }

        /** not ERC but true metadata**/
        if (MData_chk_ERCProt(mdItem) == false) {
            ret = _PLReq_readDN_TrueMD_REP(mdItem, pReq, payload, result,
                    bk_idx, dnLoc_idx);
            break;
        }

#ifdef DISCO_ERC_SUPPORT
        if (mdItem->arr_HBID[0] == mdItem->erc_dnLoc[dnLoc_idx].ERC_HBID) {
            ret = _PLReq_ReadDN_TrueMD_ERC(mdItem, pReq, payload, result,
                    bk_idx, dnLoc_idx);
            break;
        }

        ret = _PLReq_ReadDN_TrueMD_ERC_rebuild(mdItem, pReq, payload, result,
                bk_idx, dnLoc_idx);
#else
        dms_printk(LOG_LVL_INFO, "DMSC WARN not support ERC\n");
#endif
    } while (0);

    return ret;
}

/*
 * PLReq_writeDN_endfn: function to handle datanode ack from DNClient modules.
 *    Register to DNClient as callback function when submit request to DNClient
 *
 * @mdItem: need metadata item recording DN status and decide whether keeping process or finish
 * @private_data: callback argument that pass to DNClient when submit requests
 * @payload: Dummy for unify callback function
 * @result: ack result from datanode
 * @calleeReq: DNUData Request
 * @dnLoc_idx: which DN with dn loc index in metadata report disk error or hbid error
 * @bk_idx: which bucket of payload request hold DN's ID
 *
 * Return: whether any errors when process DN ack
 *         0: OK
 *        ~0: failure
 */
int32_t PLReq_writeDN_endfn (metaD_item_t *mdItem, void *private_data,
        data_mem_chunks_t *payload, int32_t result,
        ReqCommon_t *calleeReq, int32_t bk_idx, int32_t dnLoc_idx)
{
    payload_req_t *pReq;
    ReqCommon_t *plReq_comm;
    int32_t ret;
    PLREQ_FSM_action_t action;
    bool ciUser, ciMDS, ciUser_ret, do_free;

    ret = 0;
    pReq = (payload_req_t *)private_data;
    PLReq_ref_Req(pReq);

    plReq_comm = &pReq->plReq_comm;
    do_free = false;

    //NOTE: it's ok to update without lock, we don't need accuracy
    if (plReq_comm->t_sendReq == 0) {
        plReq_comm->t_sendReq = calleeReq->t_sendReq;
    }
    plReq_comm->t_rcvReqAck = calleeReq->t_rcvReqAck;

    MData_update_DNUDataStat(&mdItem->udata_dnLoc[dnLoc_idx], result);
    if (UData_S_WRITE_OK == result || UData_S_DISK_FAILURE == result) {
        PLReq_rm_DNUDReq(pReq, bk_idx);
    }
    
    ciMDS = MData_writeAck_commitResult(mdItem, &ciUser_ret, result, &ciUser);
    if (ciUser) {
        pReq->t_ciCaller = jiffies_64;
        ret = pReq->plReq_func.ciUser_CallerReq(pReq->user_data,
                mdItem->num_hbids, ciUser_ret);
    }

    if (ciMDS) {
        action = PLReq_update_state(&pReq->plReq_stat, PLREQ_E_CICaller);
        if (PLREQ_A_DO_CICaller == action) {
            PLReq_cancel_timer(pReq);
            PLReq_rm_ReqPool(pReq);
            PLReq_cancel_DNUDReq(pReq, bk_idx);
            pReq->plReq_func.rm_PLReq_CallerReq(pReq->user_data,
                    &pReq->entry_callerReqPool);
            ret = pReq->plReq_func.PLReq_endReq(pReq->user_data, plReq_comm,
                    mdItem->lbid_s, mdItem->num_hbids, true, 0);
            do_free = true;
        }
    }

    PLReq_deref_Req(pReq);
    if (do_free) {
        PLMgr_free_PLReq(pReq);
    }

    return ret;
}
