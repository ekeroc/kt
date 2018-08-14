/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_IOSegment_Request.c
 */
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/types.h>
#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/dms_kernel_version.h"
#include "../common/thread_manager.h"
#include "../common/discoC_mem_manager.h"
#include "../metadata_manager/metadata_manager.h"
#include "discoC_IOSegment_Request.h"
#include "discoC_IO_Manager.h"
#include "../vdisk_manager/volume_manager.h"
#include "io_worker_manager.h"
#include "../config/dmsc_config.h"
#include "../discoNN_client/discoC_NNC_Manager_api.h"
#include "../discoDN_client/discoC_DNC_Manager_export.h"
#include "../payload_manager/payload_manager_export.h"
#include "discoC_ioSegReq_fsm.h"
#include "discoC_ioSegReq_mdata_cbfn.h"
#include "discoC_IORequest.h"
#ifdef DISCO_PREALLOC_SUPPORT
#include "discoC_IOReqPool.h"
#include "../metadata_manager/space_manager_export_api.h"
#endif

static uint32_t _ioSReq_alloc_ID (void)
{
    return (uint32_t)atomic_add_return(1, &discoCIO_Mgr.ioSegID_gentor);
}

ioSeg_req_t *ioSReq_alloc_request (void)
{
    return discoC_malloc_pool(discoCIO_Mgr.ioSeg_mpool_ID);
}

void ioSReq_free_request (ioSeg_req_t *ioSegReq)
{
    free_metadata(&ioSegReq->MetaData);
    discoC_free_pool(discoCIO_Mgr.ioSeg_mpool_ID, ioSegReq);
    atomic_dec(&discoCIO_Mgr.cnt_split_io_req);
}

void ioSReq_add_payloadReq (void *myReq, struct list_head *list)
{
    ioSeg_req_t *ioSReq;

    ioSReq = (ioSeg_req_t *)myReq;
    spin_lock(&ioSReq->pReq_plock);

    list_add_tail(list, &ioSReq->list_pReqPool);
    atomic_inc(&ioSReq->num_pReq);

    spin_unlock(&ioSReq->pReq_plock);
}

int32_t ioSReq_rm_payloadReq (void *myReq, struct list_head *list)
{
    ioSeg_req_t *ioSReq;
    int32_t num_rPReq;

    ioSReq = (ioSeg_req_t *)myReq;
    spin_lock(&ioSReq->pReq_plock);

    list_del(list);

    num_rPReq = atomic_sub_return(1, &ioSReq->num_pReq);

    spin_unlock(&ioSReq->pReq_plock);

    return num_rPReq;
}

/*
 * NOTE: caller guarantee argument ioSeqReq won't be NULL
 */
void ioSReq_init_request (ioSeg_req_t *ioSegReq, UserIO_addr_t *ioaddr,
        iorw_dir_t ioDir, priority_type reqPrior)
{
    ReqCommon_t *ioSR_comm;
    uint32_t lb_len;

    INIT_LIST_HEAD(&ioSegReq->list2ioReq);
#ifdef DISCO_PREALLOC_SUPPORT
    INIT_LIST_HEAD(&ioSegReq->entry_fwwaitpool);
#endif
    IOSegReq_init_fsm(&ioSegReq->ioSegReq_stat);

    ioSegReq->ref_ioReq = NULL;

    INIT_LIST_HEAD(&ioSegReq->list_pReqPool);
    atomic_set(&ioSegReq->num_pReq, 0);
    spin_lock_init(&ioSegReq->pReq_plock);

    memcpy(&(ioSegReq->ioSReq_ioaddr), ioaddr, sizeof(UserIO_addr_t));

    ioSR_comm = &ioSegReq->ioSR_comm;
    ioSR_comm->ReqID = _ioSReq_alloc_ID();
    ioSR_comm->ReqFeature = 0;
    ioSR_comm->ReqPrior = reqPrior;
    ioSR_comm->ReqOPCode = ioDir;
    atomic_set(&ioSR_comm->ReqRefCnt, 0);
    ioSegReq->io_dir_true = ioDir;

    ioSegReq->lb_sect_mask = NULL;

    ioSegReq->MetaData.num_MD_items = 0;

    ioSegReq->payload_offset = 0;

    ioSegReq->ciUser_result = true;
    ioSegReq->ciMDS_status = CI_MDS_OP_REPORT_SUCCESS;
    lb_len = ioaddr->lbid_len;
    atomic_set(&ioSegReq->ciUser_remainLB, lb_len);
    atomic_set(&ioSegReq->ciMDS_remainLB, lb_len);

    ioSegReq->t_send_DNReq = 0;
    ioSegReq->t_rcv_DNResp = 0;
    ioSegReq->t_txMDReq = 0;
    ioSegReq->t_rxMDReqAck = 0;

    init_metadata(&ioSegReq->MetaData, ioaddr);

    //for queryTrue process
    memset(&ioSegReq->userLB_state, 0, NUM_BYTES_MAX_LB);
    spin_lock_init(&ioSegReq->ciUser_slock);
    INIT_LIST_HEAD(&ioSegReq->leaf_ioSRPool);
    spin_lock_init(&ioSegReq->leaf_ioSR_plock);
    atomic_set(&ioSegReq->num_leaf_ioSR, 0);

#if (DN_PROTOCOL_VERSION == 2) && (CN_CRASH_RECOVERY == 1)
    ioSegReq->ts_dn_logging = 0;
#endif
}

void ioSReq_add_ioSLeafReq (ioSeg_req_t *rootR, struct list_head *leafR)
{
    spin_lock(&rootR->leaf_ioSR_plock);

    list_add_tail(leafR, &rootR->leaf_ioSRPool);
    atomic_inc(&rootR->num_leaf_ioSR);

    spin_unlock(&rootR->leaf_ioSR_plock);
}

void ioSReq_rm_ioSLeafReq (ioSeg_req_t *rootR, struct list_head *leafR)
{
    spin_lock(&rootR->leaf_ioSR_plock);

    list_del(leafR);
    atomic_inc(&rootR->num_leaf_ioSR);

    spin_unlock(&rootR->leaf_ioSR_plock);
}

/*
 * partial read done of first write reset feature
 */
void ioSReq_resetFea_PartialRDone (ioSeg_req_t *ioSReq)
{
    volatile void *my_fea;

    my_fea = &ioSReq->ioSR_comm.ReqFeature;
    clear_bit(IOSegR_FEA_NON_4KALIGN, (volatile void *)my_fea);
    clear_bit(IOSegR_FEA_OVW, my_fea);
    clear_bit(IOSegR_FEA_MDCacheH, my_fea);
    clear_bit(IOSegR_FEA_HBID_MISMATCH, my_fea);
    clear_bit(IOSegR_FEA_CIUSER, my_fea);
}

bool ioSReq_get_ovw (ioSeg_req_t *ioSReq)
{
    return test_bit(IOSegR_FEA_OVW,
            (const volatile void *)&ioSReq->ioSR_comm.ReqFeature);
}

void ioSReq_set_ovw (ioSeg_req_t *ioSReq)
{
    set_bit(IOSegR_FEA_OVW, (volatile void *)&ioSReq->ioSR_comm.ReqFeature);
}

bool ioSReq_chk_MDCacheHit (ioSeg_req_t *ioSReq)
{
    return test_bit(IOSegR_FEA_MDCacheH,
            (const volatile void *)&ioSReq->ioSR_comm.ReqFeature);
}

void ioSReq_set_MDCacheHit (ioSeg_req_t *ioSReq)
{
    set_bit(IOSegR_FEA_MDCacheH,
            (volatile void *)&ioSReq->ioSR_comm.ReqFeature);
}

bool ioSReq_chk_4KAlign (ioSeg_req_t *ioSReq)
{
    return !test_bit(IOSegR_FEA_NON_4KALIGN,
            (const volatile void *)&ioSReq->ioSR_comm.ReqFeature);
}

void ioSReq_set_HBIDErr (ioSeg_req_t *ioSReq)
{
    set_bit(IOSegR_FEA_HBID_MISMATCH,
            (volatile void *)&ioSReq->ioSR_comm.ReqFeature);
}

bool ioSReq_chk_HBIDErr (ioSeg_req_t *ioSReq)
{
    return test_bit(IOSegR_FEA_HBID_MISMATCH,
            (const volatile void *)&ioSReq->ioSR_comm.ReqFeature);
}

bool ioSReq_chkset_ciUser (ioSeg_req_t *ioSReq)
{
    return test_and_set_bit(IOSegR_FEA_CIUSER,
            (volatile void *)&ioSReq->ioSR_comm.ReqFeature);
}

bool ioSReq_chk_TrueMDProc (ioSeg_req_t *ioSReq)
{
    return test_bit(IOSegR_FEA_TrueProc,
            (const volatile void *)&ioSReq->ioSR_comm.ReqFeature);
}

bool ioSReq_chkset_TrueMDProc (ioSeg_req_t *ioSReq)
{
    return test_and_set_bit(IOSegR_FEA_TrueProc,
            (volatile void *)&ioSReq->ioSR_comm.ReqFeature);
}

#ifdef DISCO_PREALLOC_SUPPORT
bool ioSReq_chk_InFWWaitQ (ioSeg_req_t *ioSReq)
{
    return test_bit(IOSegR_FEA_IN_FWWAITQ,
            (volatile void *)&ioSReq->ioSR_comm.ReqFeature);
}

bool ioSReq_chkset_InFWWaitQ (ioSeg_req_t *ioSReq)
{
    return test_and_set_bit(IOSegR_FEA_IN_FWWAITQ,
            (volatile void *)&ioSReq->ioSR_comm.ReqFeature);
}

bool ioSReq_chkclear_InFWWaitQ (ioSeg_req_t *ioSReq)
{
    return test_and_clear_bit(IOSegR_FEA_IN_FWWAITQ,
            (volatile void *)&ioSReq->ioSR_comm.ReqFeature);
}
#endif


static uint16_t _ioSReq_get_MDReq_opcode (iorw_dir_t iorw, bool is_ovw, bool is4Kalign)
{
    uint16_t mdOpcode;

    do {
        if (iorw == KERNEL_IO_READ) { //read case
            mdOpcode = REQ_NN_QUERY_MDATA_READIO;
            break;
        }

        if (is_ovw && is4Kalign) { //ovw and 4K align
            mdOpcode = REQ_NN_QUERY_MDATA_WRITEIO;
            break;
        }

        if (!is_ovw && is4Kalign) { //FW and 4K align
            mdOpcode = REQ_NN_ALLOC_MDATA_NO_OLD;
            break;
        }

        if (is_ovw && !is4Kalign) { //ovw and non 4K align
            mdOpcode = REQ_NN_QUERY_MDATA_READIO;
            break;
        }

        //FW and non 4K align
        mdOpcode = REQ_NN_QUERY_MDATA_READIO;
    } while (0);

    return mdOpcode;
}

/*
 * NOTE: caller guarantee argument ioSeqReq won't be NULL
 *                                 refIOR won't be NULL
 */
int32_t ioSReq_set_request (ioSeg_req_t *ioSegReq, struct io_request *refIOR,
        uint8_t *lb_sect_mask, uint64_t payload_off,
        bool is_ovw, bool is_chit, bool is4Kalign)
{
    ReqCommon_t *ioSR_comm;
    int32_t ret;
    IOSegREQ_FSM_TREvent_t trevent;
    IOSegREQ_FSM_action_t action;

    ioSR_comm = &ioSegReq->ioSR_comm;
    if (is_ovw) {
        set_bit(IOSegR_FEA_OVW, (volatile void *)&ioSR_comm->ReqFeature);
    } //NOTE: io_feature be set as 0 at init_ioSeg_request

    if (is_chit) {
        set_bit(IOSegR_FEA_MDCacheH,
                (volatile void *)&ioSR_comm->ReqFeature);
    }

    if (false == is4Kalign) {
        set_bit(IOSegR_FEA_NON_4KALIGN,
                (volatile void *)&ioSR_comm->ReqFeature);
    }

    ioSegReq->lb_sect_mask = lb_sect_mask;
    ioSegReq->payload_offset = payload_off;
    ioSegReq->ref_ioReq = refIOR;
    ioSegReq->ioSR_comm.CallerReqID = refIOR->ioReqID;

    ret = 0;

    if (IOSegREQ_S_INIT == IOSegReq_get_state(&ioSegReq->ioSegReq_stat)) {
        ioSegReq->mdata_opcode = _ioSReq_get_MDReq_opcode(ioSegReq->io_dir_true,
                is_ovw, is4Kalign);

        if (ioSegReq->io_dir_true == KERNEL_IO_READ) {
            trevent = IOSegREQ_E_INIT_R;
        } else {
            if (is4Kalign) {
                trevent = IOSegREQ_E_INIT_4KW;
            } else {
                trevent = IOSegREQ_E_INIT_R;
                ioSegReq->ioSR_comm.ReqOPCode = KERNEL_IO_READ;
            }
        }

        action = IOSegReq_update_state(&ioSegReq->ioSegReq_stat, trevent);
        if (IOSegREQ_A_W_AcqMD == action && trevent == IOSegREQ_E_INIT_4KW) {
            ret = 0;
        } else if (IOSegREQ_A_R_AcqMD == action && trevent == IOSegREQ_E_INIT_R) {
            ret = 0;
        } else {
            ret = -EPERM;
        }
    } else {
        dms_printk(LOG_LVL_WARN, "DMSC WARN fail set ioSegReq %u state fail %s\n",
                ioSR_comm->ReqID,
                IOSegReq_get_state_fsm_str(&ioSegReq->ioSegReq_stat));
        ret = -EPERM;
    }

    return ret;
}

/*
 * TODO: move to metadata.c
 */
static bool _ioSReq_chk_MData_doReport (metadata_t *my_MData)
{
    metaD_item_t *my_MD_item;
    metaD_DNLoc_t *dnLoc;
    int32_t i, j;
    bool doReport;

    doReport = false;

    for (i = 0; i < my_MData->num_MD_items; i++) {
        my_MD_item = my_MData->array_metadata[i];

        //TODO: it should be == MData_VALID_LESS_REPLICA
        if (my_MD_item->mdata_stat != MData_VALID) {
            doReport = true;
            break;
        }

        for (j = 0; j < my_MD_item->num_dnLoc; j++) {
            dnLoc = &(my_MD_item->udata_dnLoc[j]);
            if (MData_chk_UDStatus_writefail(dnLoc)) {
                doReport = true;
                break;
            }
        }

        if (doReport) {
            break;
        }
    }

    return doReport;
}

static bool _ioSReq_chk_noReport (ioSeg_req_t *ioSReq)
{
    uint16_t mdOpcode;
    bool ret;

    ret = true;
    mdOpcode = ioSReq->mdata_opcode;
    do {
        if (CI_MDS_OP_REPORT_FAILURE == ioSReq->ciMDS_status) {
            ret = false;
            break;
        }

        if (REQ_NN_QUERY_MDATA_WRITEIO != mdOpcode &&
                REQ_NN_ALLOC_MDATA_NO_OLD != mdOpcode) {
            break;
        }

        if (_ioSReq_chk_MData_doReport(&ioSReq->MetaData)) {
            ret = false;
            break;
        }
    } while (0);

    return ret;
}

int32_t ioSReq_submit_reportMD (ioSeg_req_t *ioSReq, bool mdch_on,
        uint16_t vol_replica)
{
    int32_t ret;

    ret = 0;

    if (_ioSReq_chk_noReport(ioSReq)) {
        return ret;
    }

    discoC_report_metadata(&ioSReq->ioSReq_ioaddr, mdch_on, &ioSReq->ioSR_comm,
            vol_replica,
            ioSReq_chk_MDCacheHit(ioSReq),
            ioSReq_get_ovw(ioSReq), ioSReq_chk_HBIDErr(ioSReq),
            &ioSReq->MetaData, ioSReq, ioSReq_reportMD_endfn);

    return ret;
}

int32_t ioSReq_submit_acquireMD (ioSeg_req_t *ioSReq, bool mdch_on,
        uint16_t vol_replica)
{
    ReqCommon_t *ioSR_comm;
    UserIO_addr_t *ioSR_uioaddr;
    int32_t ret;
    bool ovwStat;
#ifdef DISCO_PREALLOC_SUPPORT
    bool do_wait;
#endif

    ret = 0;

    ioSR_comm = &ioSReq->ioSR_comm;
    ioSR_uioaddr = &ioSReq->ioSReq_ioaddr;
    ovwStat = ioSReq_get_ovw(ioSReq);

#ifdef DISCO_PREALLOC_SUPPORT
    if (KERNEL_IO_WRITE == ioSR_comm->ReqOPCode && false == ovwStat) {
        do_wait = ioSReq_alloc_freeSpace(ioSReq, true);
        if (do_wait) {
            return ret;
        }
    }
#endif

    discoC_acquire_metadata(ioSR_uioaddr, mdch_on,
            ioSR_comm, vol_replica,
            ioSReq_chk_MDCacheHit(ioSReq), ovwStat,
            ioSReq_chk_HBIDErr(ioSReq), &ioSReq->MetaData, ioSReq,
            ioSReq_acquireMD_endfn, ioSReq_setovw_fn);

    return ret;
}

void ioSReq_update_udata_ts (ioSeg_req_t *ioSReq, uint64_t ts_send, uint64_t ts_rcv)
{
    if (ioSReq->t_send_DNReq == 0) {
        ioSReq->t_send_DNReq = ts_send;
    }
    ioSReq->t_rcv_DNResp = ts_rcv;
    ioSReq->ioSR_comm.t_rcvReqAck = ts_rcv;
}

void ioSReq_update_mdata_ts (ioSeg_req_t *ioSReq, uint64_t ts_send, uint64_t ts_rcv)
{
    ReqCommon_t *ioSR_comm;

    ioSR_comm = &ioSReq->ioSR_comm;
    if (ioSR_comm->t_sendReq == 0) {
        ioSR_comm->t_sendReq = ts_send;
    }
    ioSR_comm->t_rcvReqAck = ts_rcv;

    if (ioSReq->t_txMDReq == 0) {
        ioSReq->t_txMDReq = ts_send;
    }
    ioSReq->t_rxMDReqAck = ts_rcv;
}

void ioSReq_commit_user (ioSeg_req_t *ioSReq, bool result)
{
    io_request_t *io_req;
    int32_t ciUser_rLB;

    if (unlikely(IS_ERR_OR_NULL(ioSReq))) {
        dms_printk(LOG_LVL_ERR, "DMSC ERROR commit null nn req\n");
        return;
    }

    if (ioSReq_chkset_ciUser(ioSReq)) {
        dms_printk(LOG_LVL_DEBUG, "DMSC DEBUG Commit nn req %u to io req\n",
                ioSReq->ioSR_comm.ReqID);
        return;
    }

    io_req = ioSReq->ref_ioReq;

    io_req->ioResult &= result;
    ciUser_rLB = atomic_sub_return(ioSReq->ioSReq_ioaddr.lbid_len,
            &io_req->ciUser_remainLB);
    if (ciUser_rLB == 0) {
        ioReq_commit_user(io_req, io_req->ioResult);
    }
}

#ifdef DISCO_PREALLOC_SUPPORT
bool ioSReq_alloc_freeSpace (ioSeg_req_t *ioSegReq, bool do_lock)
{
    volume_IOReqPool_t *refQ;
    ReqCommon_t *ioSR_comm;
    UserIO_addr_t *ioSR_uioaddr;
    sm_allocSpace_resp_t alloc_ret;
    bool do_wait;

    do_wait = false;
    refQ = ioSegReq->ref_ioReq->ref_ioReqPool;
    ioSR_comm = &ioSegReq->ioSR_comm;
    ioSR_uioaddr = &ioSegReq->ioSReq_ioaddr;

    if (do_lock) {
        volIOReqQMgr_gain_fwwaitQ_lock(refQ);
    }

    alloc_ret = sm_alloc_volume_space(ioSR_uioaddr->volumeID,
            ioSR_uioaddr->lbid_start, ioSR_uioaddr->lbid_len,
            &ioSegReq->MetaData, ioSR_comm);

    switch (alloc_ret) {
    case SM_ALLOC_SPACE_OK:
        ioSReq_setovw_fn(ioSegReq, ioSR_uioaddr->lbid_start, ioSR_uioaddr->lbid_len);
        ioSReq_set_MDCacheHit(ioSegReq);
        ioSReq_set_ovw(ioSegReq);
        break;
    case SM_ALLOC_SPACE_WAIT:
        volIOReqQMgr_add_fwwaitQ_nolock(refQ, &ioSegReq->entry_fwwaitpool);
        do_wait = true;
        break;
    case SM_ALLOC_SPACE_NOSPACE:
        volIOReqQMgr_add_fwwaitQ_nolock(refQ, &ioSegReq->entry_fwwaitpool);
        sm_prealloc_volume_freespace(ioSR_uioaddr->volumeID,
                ioSR_uioaddr->lbid_len, ioSReq_preallocMD_endfn, (void *)refQ);

        do_wait = true;
        break;
    case SM_ALLOC_SPACE_Fail:
    case SM_ALLOC_SPACE_NoVolume:
    default:
        dms_printk(LOG_LVL_WARN, "DMSC WARN alloc free space err %d\n", alloc_ret);
        do_wait = true;
        break;
    }

    if (do_lock) {
        volIOReqQMgr_rel_fwwaitQ_lock(refQ);
    }

    return do_wait;
}
#endif

