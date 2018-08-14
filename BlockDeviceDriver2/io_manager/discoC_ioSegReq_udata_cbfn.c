/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_ioSegReq_udata_cbfn.c
 *
 * Provide callback function for payload request when request finish
 */
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/blkdev.h>
#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/dms_kernel_version.h"
#include "../common/discoC_mem_manager.h"
#include "../metadata_manager/metadata_manager.h"
#include "../discoNN_client/discoC_NNC_Manager_api.h"
#include "../payload_manager/payload_manager_export.h"
#include "discoC_IOSegment_Request.h"
#include "discoC_ioSegReq_fsm.h"
#include "discoC_IO_Manager.h"
#include "discoC_IOReq_copyData_func.h"
#include "../vdisk_manager/volume_manager.h"
#include "discoC_IORequest.h"
#include "discoC_ioSegReq_mdata_cbfn.h"
#include "discoC_ioSegReq_udata_cbfn.h"

void ioSReq_read_cpData (void *user_data, metaD_item_t *mdItem,
        uint64_t payload_off, data_mem_chunks_t *payload)
{
    ioSeg_req_t *ioSReq;

    ioSReq = (ioSeg_req_t *)user_data;

    ioReq_RCopyData_mem2bio(ioSReq->ref_ioReq, mdItem->lbid_s, mdItem->num_hbids,
            payload_off, payload);

}

void ioSReq_partialW_read_cpData (void *user_data, metaD_item_t *mdItem,
        uint64_t payload_off, data_mem_chunks_t *payload)
{
    ioSeg_req_t *ioSReq;

    ioSReq = (ioSeg_req_t *)user_data;

    ioReq_RCopyData_mem2wbuff(ioSReq->ref_ioReq, mdItem->lbid_s, mdItem->num_hbids,
            payload_off, payload);
}

/*
 * TODO: clarify me. How to decide the IO result to commit to user when a io seg request
 *       enter query true process
 */
static bool _ioSReq_chk_ciUser_trueMD (ioSeg_req_t *ioSReq, uint64_t lb_s, uint32_t lb_len)
{
    uint32_t bit_off;
    int32_t ciUsr_rLB, i, num_bit_clear;
    bool ciUser;

    ciUser = true;
    num_bit_clear = 0;

    for (i = 0; i < lb_len; i++) {
        bit_off = lb_s - ioSReq->ioSReq_ioaddr.lbid_start;
        if (test_and_clear_bit(bit_off, (volatile void *)ioSReq->userLB_state)) {
            num_bit_clear++;
            continue;
        }

        ciUser = false;
        break;
    }

    if (num_bit_clear) {
        ciUsr_rLB = atomic_sub_return(num_bit_clear, &(ioSReq->ciUser_remainLB));
    } else {
        ciUsr_rLB = atomic_read(&(ioSReq->ciUser_remainLB));
    }

    if (ciUser) {
        for (i = 0; i < NUM_BYTES_MAX_LB; i++) {
            if (ioSReq->userLB_state[i] != 0x0) {
                ciUser = false;
                break;
            }
        }
    }

    if (ciUser && ciUsr_rLB != 0) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN ciUser LB state remain LB miss match %d %d\n",
                ciUser, ciUsr_rLB);
    }

    return ciUser;
}

static bool _ioSReq_chk_ciUser (ioSeg_req_t *ioSReq, uint64_t lb_s, uint32_t lb_len)
{
    int32_t ciUsr_rLB;
    bool ciUser;

    ciUsr_rLB = 0;
    spin_lock(&ioSReq->ciUser_slock);

    if (false == ioSReq_chk_TrueMDProc(ioSReq)) {
        ciUsr_rLB = atomic_sub_return(lb_len, &(ioSReq->ciUser_remainLB));
        ciUser = ((ciUsr_rLB == 0) ? true : false);
    } else {
        ciUser = _ioSReq_chk_ciUser_trueMD(ioSReq, lb_s, lb_len);
    }

    spin_unlock(&ioSReq->ciUser_slock);

    return ciUser;
}

/*
 * TODO: make sure when all HBID err, copy zero data to bio
 *
 * TODO: refactor me, long function
 */
int32_t ioSReq_ReadPL_endfn (void *user_data, ReqCommon_t *calleeReq,
        uint64_t lb_s, uint32_t lb_len, bool ioRes, int32_t numFailDN)
{
    ioSeg_req_t *ioSReq;
    int32_t numPReq, ret, ciMDS_rLB;
    IOSegREQ_FSM_action_t action;
    bool ciUser;

    ioSReq = (ioSeg_req_t *)user_data;
    ret = 0;

    ioSReq_update_udata_ts(ioSReq, calleeReq->t_sendReq, calleeReq->t_rcvReqAck);

    if (numFailDN) {
        ioSReq->ciMDS_status = CI_MDS_OP_REPORT_FAILURE;
    } else {
        if (ioSReq->ciMDS_status != CI_MDS_OP_REPORT_FAILURE) {
            ioSReq->ciMDS_status = CI_MDS_OP_REPORT_NONE;
        }
    }

    ioSReq->ciUser_result &= ioRes;
    numPReq = atomic_read(&ioSReq->num_pReq);

    ciUser = _ioSReq_chk_ciUser(ioSReq, lb_s, lb_len);
    ciMDS_rLB = atomic_sub_return(lb_len, &(ioSReq->ciMDS_remainLB));

    if (ciUser) {
        do {
            action = IOSegReq_update_state(&ioSReq->ioSegReq_stat,
                    IOSegREQ_E_R_ALLPL_DONE);
            if (action != IOSegREQ_A_DO_CI2IOSegREQ) {
                ret = -EPERM;
                dms_printk(LOG_LVL_INFO, "DMSC INFO Read ioSeg fail tr "
                        "R_ALLPL_DONE action %s state %s\n",
                        IOSegReq_get_action_str(action),
                        IOSegReq_get_state_fsm_str(&ioSReq->ioSegReq_stat));
                break;
            }

            action = IOSegReq_update_state(&ioSReq->ioSegReq_stat,
                    IOSegREQ_E_R_RPhase_DONE);
            if (numPReq != 0) {
                dms_printk(LOG_LVL_WARN,
                        "DMSC WARN num payload req in ioSegR should 0 "
                        "when all read DONE %d\n", numPReq);
            }

            if (action != IOSegREQ_A_DO_CI2IOREQ) {
                dms_printk(LOG_LVL_INFO, "DMSC INFO Read ioSeg fail tr "
                        "R_RPhase_DONE action %s state %s\n",
                        IOSegReq_get_action_str(action),
                        IOSegReq_get_state_fsm_str(&ioSReq->ioSegReq_stat));
                ret = -EPERM;
                break;
            }

            ioSReq_commit_user(ioSReq, ioSReq->ciUser_result);
            ioReq_IODone_endfn(ioSReq->ref_ioReq, ioSReq->ioSReq_ioaddr.lbid_len);
        } while (0);
    }

    return ret;
}

static void _ioSReq_PW_resetReq (ioSeg_req_t *ioSReq)
{
    atomic_set(&(ioSReq->ciUser_remainLB), ioSReq->ioSReq_ioaddr.lbid_len);
    ioSReq->ioSR_comm.ReqOPCode = ioSReq->io_dir_true;
    ioSReq->ciUser_result = true;
    ioSReq->ciMDS_status = CI_MDS_OP_REPORT_SUCCESS;
    ioSReq->t_send_DNReq = 0;
    ioSReq->t_rcv_DNResp = 0;
    ioSReq->t_txMDReq = 0;
    ioSReq->t_rxMDReqAck = 0;
}

static void _ioSReq_PW_WPhase_OVW (ioSeg_req_t *ioSReq)
{
    PLReq_func_t plReq_func;
    io_request_t *io_req;
    IOSegREQ_FSM_action_t action;

    io_req = ioSReq->ref_ioReq;
    ioSReq->mdata_opcode = REQ_NN_QUERY_MDATA_WRITEIO;
    action = IOSegReq_update_state(&ioSReq->ioSegReq_stat, IOSegREQ_E_W_AcqMD_DONE);
    if (IOSegREQ_A_DO_WUD == action) {

        plReq_func.add_PLReq_CallerReq = ioSReq_add_payloadReq;
        plReq_func.rm_PLReq_CallerReq = ioSReq_rm_payloadReq;
        plReq_func.ciUser_CallerReq = ioSReq_WritePL_ciUser;
        plReq_func.PLReq_endReq = ioSReq_WritePL_endfn;
        plReq_func.cpData_CallerReq = NULL;

        update_metadata_UDataStat(&ioSReq->MetaData, UData_S_INIT);
        PLMgr_write_payload(&ioSReq->MetaData, &ioSReq->ioSR_comm,
                &(ioSReq->ref_ioReq->payload),
                ioSReq->payload_offset,
                io_req->fp_map, io_req->fp_cache,
                is_vol_send_rw_payload(io_req->drive),
                is_vol_do_rw_IO(io_req->drive), ioSReq, &plReq_func);
    } else {
        dms_printk(LOG_LVL_INFO,
                "DMSC INFO ioSReq %u fail tr W_AcqMD_DONE state %s action %s\n",
                ioSReq->ioSR_comm.ReqID,
                IOSegReq_get_state_fsm_str(&ioSReq->ioSegReq_stat),
                IOSegReq_get_action_str(action));
    }
}

static void _ioSReq_PW_WPhase_FW (ioSeg_req_t *ioSReq)
{
    io_request_t *io_req;
#ifdef DISCO_PREALLOC_SUPPORT
    bool do_wait;
#endif

    io_req = ioSReq->ref_ioReq;

    ioSReq_resetFea_PartialRDone(ioSReq);
    free_metadata(&ioSReq->MetaData);
    init_metadata(&ioSReq->MetaData, &ioSReq->ioSReq_ioaddr);

    ioSReq->mdata_opcode = REQ_NN_ALLOC_MDATA_NO_OLD;

#ifdef DISCO_PREALLOC_SUPPORT
    do_wait = ioSReq_alloc_freeSpace(ioSReq, true);
    if (do_wait) {
        return;
    }
#endif

    discoC_acquire_metadata(&ioSReq->ioSReq_ioaddr,
            ioSReq->ref_ioReq->is_mdcache_on,
            &ioSReq->ioSR_comm, io_req->volume_replica,
            ioSReq_chk_MDCacheHit(ioSReq),
            ioSReq_get_ovw(ioSReq),
            ioSReq_chk_HBIDErr(ioSReq), &ioSReq->MetaData,
            ioSReq, ioSReq_acquireMD_endfn, ioSReq_setovw_fn);
}

static void _ioSReq_PW_WPhase (ioSeg_req_t *ioSReq)
{
    _ioSReq_PW_resetReq(ioSReq);
    if (ioSReq_get_ovw(ioSReq)) {
        _ioSReq_PW_WPhase_OVW(ioSReq);
    } else {
        _ioSReq_PW_WPhase_FW(ioSReq);
    }
}

/*
 * TODO: how to commit to namenode when data IO error during read phase of partial write
 */
int32_t ioSReq_PartialW_ReadPL_endfn (void *user_data, ReqCommon_t *calleeReq,
        uint64_t lb_s, uint32_t lb_len, bool ioRes, int32_t numFailDN)
{
    ioSeg_req_t *ioSReq;
    int32_t numPReq, ret;
    IOSegREQ_FSM_action_t action;
    bool ciUser;

    ioSReq = (ioSeg_req_t *)user_data;
    ret = 0;

    ioSReq_update_udata_ts(ioSReq, calleeReq->t_sendReq, calleeReq->t_rcvReqAck);

//    if (numFailDN) {
//        ioSReq->ciMDS_status = CI_MDS_OP_REPORT_FAILURE;
//    } else {
//        if (ioSReq->ciMDS_status != CI_MDS_OP_REPORT_FAILURE) {
//            ioSReq->ciMDS_status = CI_MDS_OP_REPORT_NONE;
//        }
//    }

    ioSReq->ciUser_result &= ioRes;

    numPReq = atomic_read(&ioSReq->num_pReq);
    ciUser = _ioSReq_chk_ciUser(ioSReq, lb_s, lb_len);
    if (ciUser == false) { //io seg req not yet got all payload
        return ret;
    }

    do {
        if (ioSReq->ciUser_result == false) {
            //fixme: if already commit to user, what operation to take when read fail??
            ioSReq_commit_user(ioSReq, ioSReq->ciUser_result);
            //TODO: error here ioSReq_WritePL_endfn will decrease ciUser_remainLB again
            ioSReq_WritePL_endfn(user_data, calleeReq,
                    ioSReq->ioSReq_ioaddr.lbid_start,
                    ioSReq->ioSReq_ioaddr.lbid_len,
                    false, numFailDN);
            break;
        }

        if (numPReq != 0) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN num payload req in ioSegR should 0 "
                    "when read phase of PWrite DONE %d\n", numPReq);
        }

        action = IOSegReq_update_state(&ioSReq->ioSegReq_stat, IOSegREQ_E_R_ALLPL_DONE);
        if (action != IOSegREQ_A_DO_CI2IOSegREQ) {
            dms_printk(LOG_LVL_INFO, "DMSC INFO Read of Write ioSeg fail tr "
                    "R_ALLPL_DONE action %s state %s\n",
                    IOSegReq_get_action_str(action),
                    IOSegReq_get_state_fsm_str(&ioSReq->ioSegReq_stat));
            break;
        }

        action = IOSegReq_update_state(&ioSReq->ioSegReq_stat, IOSegREQ_E_W_RPhase_DONE);
        if (action != IOSegREQ_A_W_AcqMD) {
            dms_printk(LOG_LVL_INFO, "DMSC INFO Read of Write ioSeg fail tr "
                    "W_RPhase_DONE action %s state %s\n",
                    IOSegReq_get_action_str(action),
                    IOSegReq_get_state_fsm_str(&ioSReq->ioSegReq_stat));
            break;
        }

        _ioSReq_PW_WPhase(ioSReq);

    } while (0);

    return ret;
}

int32_t ioSReq_WritePL_ciUser (void *user_data, int32_t numLB, bool ioRes)
{
    ioSeg_req_t *ioSReq;
    int32_t ret;

    ioSReq = (ioSeg_req_t *)user_data;

    ret = 0;

    ioSReq->ciUser_result &= ioRes;
    //TODO: we should move the decrease logic to ioSReq_commit_user, check whether
    //      we can unify the logic
    if (atomic_sub_return(numLB, &(ioSReq->ciUser_remainLB)) == 0) {
        ioSReq_commit_user(ioSReq, ioSReq->ciUser_result);
    }

    return ret;
}

int32_t ioSReq_WritePL_endfn (void *user_data, ReqCommon_t *calleeReq,
        uint64_t lb_s, uint32_t lb_len, bool ioRes, int32_t numFailDN)
{
    ioSeg_req_t *ioSReq;
    IOSegREQ_FSM_action_t action;
    int32_t ret;

    ret = 0;

    ioSReq = (ioSeg_req_t *)user_data;
    ioSReq_update_udata_ts(ioSReq, calleeReq->t_sendReq, calleeReq->t_rcvReqAck);

    do {
        if (atomic_sub_return(lb_len, &(ioSReq->ciMDS_remainLB)) != 0) {
            break;
        }

        action = IOSegReq_update_state(&ioSReq->ioSegReq_stat, IOSegREQ_E_W_ALLPL_DONE);
        if (action == IOSegREQ_A_DO_CI2IOREQ) {
            ioReq_IODone_endfn(ioSReq->ref_ioReq, ioSReq->ioSReq_ioaddr.lbid_len);
        } else {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN ioSeg %u state %s fail W_ALLPL_DONE action %s\n",
                    ioSReq->ioSR_comm.ReqID,
                    IOSegReq_get_state_fsm_str(&ioSReq->ioSegReq_stat),
                    IOSegReq_get_action_str(action));
        }
    } while (0);

    return ret;
}
