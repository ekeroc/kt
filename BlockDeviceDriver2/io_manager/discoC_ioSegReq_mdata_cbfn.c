/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_ioSegReq_mdata_cbfn.c
 *
 * Provide callback function for metadata request when request finish
 */
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/delay.h>
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
#include "../vdisk_manager/volume_manager.h"
#include "discoC_IORequest.h"
#include "../discoNN_client/discoC_NNC_ovw.h"
#include "discoC_ioSegReq_udata_cbfn.h"
#include "discoC_IOReq_copyData_func.h"
#include "discoC_ioSegReq_mdata_cbfn.h"
#include "discoC_IOSegLeaf_Request.h"
#ifdef DISCO_PREALLOC_SUPPORT
#include "discoC_IOReqPool.h"
#include "../metadata_manager/space_manager_export_api.h"
#endif

int32_t ioSReq_setovw_fn (void *user_data, uint64_t lb_s, uint32_t lb_len)
{
    ioSeg_req_t *ioSReq;

    ioSReq = (ioSeg_req_t *)user_data;

    return set_vol_ovw_range(ioSReq->ref_ioReq->drive, lb_s, lb_len, 1);
}

int32_t ioSReq_reportMD_endfn (metadata_t *MData, void *private_data,
        ReqCommon_t *calleeReq, int32_t result)
{
    io_request_t *io_req;
    ioSeg_req_t *ioSReq;
    int32_t ret;

    ret = 0;

    ioSReq = (ioSeg_req_t *)private_data;
    io_req = ioSReq->ref_ioReq;

    ioSReq->ciMDS_status = CI_MDS_OP_REPORT_NONE;

#if (DN_PROTOCOL_VERSION == 2) && (CN_CRASH_RECOVERY == 1)
    atomic_dec(&nn_commit_wait_resp);
    if (nn_req->io_req->iodir == KERNEL_IO_WRITE) {
        update_cleanLog_ts();
    }
#endif

    ioReq_MDReport_endfn(io_req, ioSReq->ioSReq_ioaddr.lbid_len);

    return ret;
}

static int32_t check_metadata_status (metadata_t *MData, ioSeg_req_t *ioSReq,
        int32_t mdres)
{
    ReqCommon_t *ioSR_comm;
    if (mdres) {
        //alloc space for metadata fail
        return true;
    }

    if (MData->num_invalMD_items == 0) {
        return false;
    }

    ioSR_comm = &ioSReq->ioSR_comm;
    if (ioSR_comm->ReqOPCode == KERNEL_IO_READ) {
        return false;
    }

    //NOTE: has invalidate times when write
    if (ioSR_comm->ReqOPCode == KERNEL_IO_WRITE) {
        return true;
    }

    return false;
}

/*
 * Only for query metadata for Read
 *
 */
static void _post_handle_all_MD_inval (ioSeg_req_t *ioSegReq)
{
    io_request_t *io_req;
    UserIO_addr_t *ioaddr;

    io_req = ioSegReq->ref_ioReq;
    ioaddr = &ioSegReq->ioSReq_ioaddr;

    switch (ioSegReq->ioSR_comm.ReqOPCode) {
    case KERNEL_IO_READ:
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN fail alloc MD for Read"
                "volid %u iorid %u nnid %u opcode %u\n",
                ioaddr->volumeID, io_req->ioReqID, ioSegReq->ioSR_comm.ReqID,
                ioSegReq->mdata_opcode);
        break;
    case KERNEL_IO_WRITE:
        dms_printk(LOG_LVL_WARN,
                "DMSC INFO All Metadata for WRITE invalidate "
                "volid %u iorid %u nnid %u opcode %u\n",
                ioaddr->volumeID, io_req->ioReqID, ioSegReq->ioSR_comm.ReqID,
                ioSegReq->mdata_opcode);
        break;
    default:
        dms_printk(LOG_LVL_WARN,
                "DMSC INFO All Metadata for unknow opcode %d "
                "volid %u iorid %u ioSReq %u opcode %u\n",
                ioSegReq->ioSR_comm.ReqOPCode,
                ioaddr->volumeID, io_req->ioReqID, ioSegReq->ioSR_comm.ReqID,
                ioSegReq->mdata_opcode);
        break;
    }

    ioSReq_commit_user(ioSegReq, false);
    ioSegReq->ciMDS_status = CI_MDS_OP_REPORT_NONE;

    ioReq_IODone_endfn(io_req, ioaddr->lbid_len);
}

static void _invalMD_IODone_Proc (metadata_t *my_metaD, ioSeg_req_t *ioSReq)
{
    metaD_item_t *md_item;
    ReqCommon_t calleeReq;
    uint64_t LB_s;
    int32_t i, num_locs;

    memset(&calleeReq, 0, sizeof(ReqCommon_t));
    num_locs = my_metaD->num_MD_items;
    LB_s = my_metaD->usr_ioaddr.lbid_start;
    for (i = 0; i < num_locs; i++) {
        md_item = my_metaD->array_metadata[i];

        if (md_item->mdata_stat != MData_VALID &&
                md_item->mdata_stat != MData_VALID_LESS_REPLICA) {
            if (ioSReq->io_dir_true == KERNEL_IO_READ) {
                ioReq_RCopyData_zero2bio(ioSReq->ref_ioReq, LB_s, md_item->num_hbids);
                ioSReq_ReadPL_endfn(ioSReq, &calleeReq,
                        LB_s, md_item->num_hbids, true, 0);
            } else {
                ioSReq_PartialW_ReadPL_endfn(ioSReq, &calleeReq,
                        LB_s, md_item->num_hbids, true, 0);
            }
        }

        LB_s += md_item->num_hbids;
    }
}


int32_t ioSReq_acquireMD_endfn (metadata_t *MData, void *private_data,
        ReqCommon_t *calleeReq, int32_t result)
{
    io_request_t *io_req;
    ioSeg_req_t *ioSReq;
    ReqCommon_t *ioSR_comm;
    int32_t retCode, numLB_inval;
    IOSegREQ_FSM_action_t action;
    PLReq_func_t plReq_func;

    ioSReq = (ioSeg_req_t *)private_data;
    ioSR_comm = &ioSReq->ioSR_comm;

    ioSReq_update_mdata_ts(ioSReq, calleeReq->t_sendReq, calleeReq->t_rcvReqAck);

    io_req = ioSReq->ref_ioReq;
    retCode = 0;

    if (check_metadata_status(MData, ioSReq, result)) {
        _post_handle_all_MD_inval(ioSReq);
        return 0;
    }

    numLB_inval = 0;
    switch (ioSReq->ioSR_comm.ReqOPCode) {
    case KERNEL_IO_READ:
        action = IOSegReq_update_state(&ioSReq->ioSegReq_stat, IOSegREQ_E_R_AcqMD_DONE);
        if (IOSegREQ_A_DO_RUD == action) {
            update_metadata_UDataStat(MData, UData_S_INIT);

            plReq_func.add_PLReq_CallerReq = ioSReq_add_payloadReq;
            plReq_func.rm_PLReq_CallerReq = ioSReq_rm_payloadReq;
            plReq_func.ciUser_CallerReq = NULL;
            plReq_func.reportMDErr_CallerReq = ioSReq_queryTrueMD;
            if (ioSReq->io_dir_true == KERNEL_IO_READ) {
                plReq_func.PLReq_endReq = ioSReq_ReadPL_endfn;
                plReq_func.cpData_CallerReq = ioSReq_read_cpData;
            } else {
                plReq_func.PLReq_endReq = ioSReq_PartialW_ReadPL_endfn;
                plReq_func.cpData_CallerReq = ioSReq_partialW_read_cpData;
            }

            PLMgr_read_payload(MData, &ioSReq->ioSR_comm,
                ioSReq->payload_offset,
                is_vol_send_rw_payload(io_req->drive),
                is_vol_do_rw_IO(io_req->drive), ioSReq, &numLB_inval,
                &io_req->drive->dnSeltor, &plReq_func);
        } else {
            dms_printk(LOG_LVL_WARN, "DMSC WARN fail ioSegR tr R_AcqMD_DONE "
                    "state %s action %s\n",
                    IOSegReq_get_state_fsm_str(&ioSReq->ioSegReq_stat),
                    IOSegReq_get_action_str(action));
        }
        break;
    case KERNEL_IO_WRITE:
        plReq_func.add_PLReq_CallerReq = ioSReq_add_payloadReq;
        plReq_func.rm_PLReq_CallerReq = ioSReq_rm_payloadReq;
        plReq_func.ciUser_CallerReq = ioSReq_WritePL_ciUser;
        plReq_func.PLReq_endReq = ioSReq_WritePL_endfn;
        plReq_func.cpData_CallerReq = NULL;
        plReq_func.reportMDErr_CallerReq = NULL;
        action = IOSegReq_update_state(&ioSReq->ioSegReq_stat, IOSegREQ_E_W_AcqMD_DONE);
        if (IOSegREQ_A_DO_WUD == action) {
            update_metadata_UDataStat(MData, UData_S_INIT);
            PLMgr_write_payload(MData, &ioSReq->ioSR_comm,
                    &(ioSReq->ref_ioReq->payload),
                    ioSReq->payload_offset,
                    io_req->fp_map, io_req->fp_cache,
                    is_vol_send_rw_payload(io_req->drive),
                    is_vol_do_rw_IO(io_req->drive), ioSReq, &plReq_func);
        } else {
            dms_printk(LOG_LVL_WARN, "DMSC WARN fail ioSegR tr W_AcqMD_DONE "
                    "state %s action %s\n",
                    IOSegReq_get_state_fsm_str(&ioSReq->ioSegReq_stat),
                    IOSegReq_get_action_str(action));
        }
        break;
    default:
        dms_printk(LOG_LVL_WARN, "DMSC WARN gen dn req fail unkown rw_type %d"
                " mdopcode %d\n",
                ioSReq->ioSR_comm.ReqOPCode, ioSReq->mdata_opcode);
        retCode = -EINVAL;
        break;
    }

    if (numLB_inval > 0) {
        _invalMD_IODone_Proc(MData, ioSReq);
    }

    return retCode;
}

static bool _ioSReq_taretLB_gotData (ioSeg_req_t *ioSReq, uint64_t lbid)
{
    metaD_item_t *mdItem;
    uint32_t i;
    bool gotData;

    gotData = false;
    for (i = 0; i < ioSReq->MetaData.num_MD_items; i++) {
        mdItem = ioSReq->MetaData.array_metadata[i];
        if (lbid < mdItem->lbid_s) {
            continue;
        }

        if (lbid >= (mdItem->lbid_s + mdItem->num_hbids)) {
            continue;
        }

        if (MData_chk_ciUser(mdItem)) {
            gotData = true;
        }

        break;
    }

    return gotData;
}

static void _ioSReq_set_LBStat (ioSeg_req_t *ioSReq, uint64_t lbid_s, uint32_t lb_len)
{
    uint64_t target_LB;
    uint32_t i, bit_off;

    spin_lock(&ioSReq->ciUser_slock);

    do {
        if (ioSReq_chkset_TrueMDProc(ioSReq)) {
            //already got a payload request that trigger root ioSegReq enter True Process
            //no need to set LB state again
            break;
        }

        for (i = 0; i < lb_len; i++) {
            target_LB = lbid_s + (uint64_t)i;
            if (_ioSReq_taretLB_gotData(ioSReq, target_LB)) {
                continue;
            }

            bit_off = target_LB - ioSReq->ioSReq_ioaddr.lbid_start;
            if (test_and_set_bit(bit_off, (volatile void *)ioSReq->userLB_state)) {
                dms_printk(LOG_LVL_WARN, "DMSC WARN wrong to set LB state twice\n");
            }
        }
    } while (0);

    spin_unlock(&ioSReq->ciUser_slock);
}

static bool _ioSReq_gen_leafIOSReq (ioSeg_req_t *ioSReq,
        uint64_t lbid_s, uint32_t lb_len)
{
    ioSeg_leafReq_t *ioS_leafR;
    uint64_t target_LB;
    uint32_t i;
    bool do_submit;

    do_submit = false;
    for (i = 0; i < lb_len; i++) {
        target_LB = lbid_s + (uint64_t)i;
        if (_ioSReq_taretLB_gotData(ioSReq, target_LB)) {
            continue;
        }

        ioS_leafR = ioSegLeafReq_alloc_request();
        ioSegLeafReq_init_request(ioS_leafR, target_LB, ioSReq);
        ioSReq_add_ioSLeafReq(ioSReq, &ioS_leafR->entry_ioSegReq);

        do_submit = true;
    }

    return do_submit;
}

static void _ioSReq_submit_all_leafR (ioSeg_req_t *ioSReq)
{
    ioSeg_leafReq_t *cur, *next;

    list_for_each_entry_safe (cur, next, &ioSReq->leaf_ioSRPool, entry_ioSegReq) {
        ioSegLeafReq_submit_request(cur, ioSReq->ref_ioReq->is_mdcache_on,
                ioSReq->ref_ioReq->volume_replica);
    }
}

static void _ioSReq_queryTrue_gut (ioSeg_req_t *ioSReq, uint64_t lbid_s, uint32_t lb_len)
{
    _ioSReq_set_LBStat(ioSReq, lbid_s, lb_len);

    if (_ioSReq_gen_leafIOSReq(ioSReq, lbid_s, lb_len)) {
        //step 2: submit with query true
        _ioSReq_submit_all_leafR(ioSReq);
    }
}

int32_t ioSReq_queryTrueMD (void *myReq, uint64_t lbid_s, uint32_t lb_len)
{
#define MAX_BUSY_WAIT_CNT   1000
#define SLEEP_WAIT_PERIOD   100 //milli-second
    ioSeg_req_t *ioSReq;
    int32_t cnt;
    IOSegREQ_FSM_action_t action;
    bool genReq;

    ioSReq = (ioSeg_req_t *)myReq;
    genReq = false;
    cnt = 0;

    while (true) {
        action = IOSegReq_update_state(&ioSReq->ioSegReq_stat, IOSegREQ_E_MDUD_UNSYNC);
        if (action == IOSegREQ_A_RetryTR) {
            cnt++;

            if (cnt <= MAX_BUSY_WAIT_CNT) {
                yield();
            } else {
                msleep(SLEEP_WAIT_PERIOD);
                cnt = 0;
            }
            continue;
        }

        if (action == IOSegREQ_A_DO_ATrueMD_PROC) {
            genReq = true;
            break;
        }
    }

    if (genReq) {
        _ioSReq_queryTrue_gut(ioSReq, lbid_s, lb_len);
        action = IOSegReq_update_state(&ioSReq->ioSegReq_stat, IOSegREQ_E_ATrueMD_DONE);
        if (action != IOSegREQ_A_DO_RUD) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC INFO fail tr ATrueMD_DONE state %s action %s\n",
                    IOSegReq_get_state_fsm_str(&ioSReq->ioSegReq_stat),
                    IOSegReq_get_action_str(action));
        }
    }

    return 0;
}

#ifdef DISCO_PREALLOC_SUPPORT
int32_t ioSReq_preallocMD_endfn (void *pdata, int32_t result)
{
    volume_IOReqPool_t *volIOPool;
    struct list_head *entry;
    ioSeg_req_t *ioSR;

    volIOPool = (volume_IOReqPool_t *)pdata;

    volIOReqQMgr_gain_fwwaitQ_lock(volIOPool);

    while (1) {
        entry = volIOReqQMgr_peek_fwwaitQ_nolock(volIOPool);
        if (IS_ERR_OR_NULL(entry)) {
            break;
        }

        ioSR = list_entry(entry, ioSeg_req_t, entry_fwwaitpool);

        if (ioSReq_alloc_freeSpace(ioSR, false)) {
            break;
        }

        discoC_acquire_metadata(&ioSR->ioSReq_ioaddr,
                ioSR->ref_ioReq->is_mdcache_on,
                &ioSR->ioSR_comm, ioSR->ref_ioReq->volume_replica,
                ioSReq_chk_MDCacheHit(ioSR),
                ioSReq_get_ovw(ioSR),
                ioSReq_chk_HBIDErr(ioSR), &ioSR->MetaData,
                ioSR, ioSReq_acquireMD_endfn, ioSReq_setovw_fn);
    }
    volIOReqQMgr_rel_fwwaitQ_lock(volIOPool);

    return 0;
}
#endif
