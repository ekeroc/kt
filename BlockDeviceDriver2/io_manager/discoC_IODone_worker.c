/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_IODone_worker.c
 */
#include <linux/version.h>
#include <linux/delay.h>
#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/dms_kernel_version.h"
#include "../common/discoC_mem_manager.h"
#include "../common/thread_manager.h"
#include "../common/dms_client_mm.h"
#include "../metadata_manager/metadata_manager.h"
#include "discoC_IOSegment_Request.h"
#include "discoC_IO_Manager.h"
#include "../connection_manager/conn_manager_export.h"
#include "../vdisk_manager/volume_manager.h"
#include "../config/dmsc_config.h"
#include "../flowcontrol/FlowControl.h"
#include "../discoNN_client/discoC_NNC_Manager_api.h"
#include "discoC_IODone_worker.h"
#include "discoC_IODone_worker_private.h"
#include "discoC_IOReq_fsm.h"
#include "discoC_IORequest.h"

int32_t IORFWkerMgr_workq_size (void)
{
    return atomic_read(&IOR_fWker_mgr.ioReq_freeQ_Size);
}

void IORFWkerMgr_submit_request (io_request_t *io_req)
{
    struct list_head *lhead;
    spinlock_t *lhead_lock;
    int32_t hindex;

    if (unlikely(IS_ERR_OR_NULL(io_req))) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN add null IOReq to free process\n");
        DMS_WARN_ON(true);
        return;
    }

    hindex = io_req->ioReqID & IORFreeQ_MOD_NUM;
    lhead = &(IOR_fWker_mgr.ioReq_freeQ_pool[hindex]);
    lhead_lock = &(IOR_fWker_mgr.ioReq_freeQ_plock[hindex]);
    spin_lock(lhead_lock);

    if (false == ioReq_chkset_fea(io_req, IOREQ_FEA_IN_IODONEPOOL)) {
        list_add_tail(&io_req->list2ioDoneWPool, lhead);
        if (atomic_add_return(1, &IOR_fWker_mgr.ioReq_freeQ_Size) == 1) {
            atomic_set(&IOR_fWker_mgr.ioReq_freeQ_deqIdx, hindex);
        }
    }

    spin_unlock(lhead_lock);

    if (waitqueue_active(&IOR_fWker_mgr.ioReq_fWker_wq)) {
        wake_up_interruptible_all(&IOR_fWker_mgr.ioReq_fWker_wq);
    }
}

static io_request_t *_fWKer_peek_ioReq (IOR_freeWker_Mgr_t *mymgr)
{
    io_request_t *io_req, *cur, *next;
    struct list_head *lhead;
    spinlock_t *lhead_lock;
    uint32_t deq_index;

    io_req = NULL;
    deq_index = atomic_read(&mymgr->ioReq_freeQ_deqIdx);
    atomic_inc(&mymgr->ioReq_freeQ_deqIdx);
    deq_index = deq_index & IORFreeQ_MOD_NUM;
    lhead = &(mymgr->ioReq_freeQ_pool[deq_index]);
    lhead_lock = &(mymgr->ioReq_freeQ_plock[deq_index]);

    spin_lock(lhead_lock);
    list_for_each_entry_safe (cur, next, lhead, list2ioDoneWPool) {
        if (unlikely(IS_ERR_OR_NULL(cur))) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN ioReq free queue broken\n");
            DMS_WARN_ON(true);
            break;
        }

        if (ioReq_chkclear_fea(cur, IOREQ_FEA_IN_IODONEPOOL)) {
            list_del(&(cur->list2ioDoneWPool));
            atomic_dec(&mymgr->ioReq_freeQ_Size);
            io_req = cur;
        }

        break;
    }
    spin_unlock(lhead_lock);

    return io_req;
}

/*
 * _chk_write_fail: check whether report user data access status at write IO
 * Case 1: RR on, if number of replica in metadata < volume replica, report
 * Case 2: RR off, if number of replica in metadata = number of alive DN, don't report
 * Case 3: Some DN write fail (IO exception, connection broken
 * TODO: move to metadata.c
 */
static bool _chk_writeDN_fail (metadata_t *my_MData, struct volume_device *drv)
{
    metaD_item_t *my_MD_item;
    int32_t i, num_lrs;
    bool doReport;

    doReport = false;
    num_lrs = my_MData->num_MD_items;

    for (i = 0; i < num_lrs; i++) {
        my_MD_item = my_MData->array_metadata[i];
        if (my_MD_item->mdata_stat != MData_VALID) {
            doReport = true;
            break;
        }

        if (MData_get_num_diskAck(my_MD_item) < get_volume_replica(drv)) {
            doReport = true;
            break;
        }
    }

    return doReport;
}

/*
 * return value:
 *     true: caller should send report to namenode
 *     false: caller don't need send report to namenode
 */
static bool _chk_dnerr_commit_nn (io_request_t *io_req)
{
    ioSeg_req_t *ioSReq, *cur, *next;
    ciMDS_opCode_t ci_stat;
    bool ret;

    ret = false;

    if (IOREQ_S_ReportMD != IOReq_get_state(&io_req->ioReq_stat)) {
        return ret;
    }

#ifdef DISCO_ERC_SUPPORT //TODO: fixme report MD when support ERC
    return ret;
#endif

    list_for_each_entry_safe (cur, next, &(io_req->ioSegReq_lPool), list2ioReq) {
        if (unlikely(IS_ERR_OR_NULL(cur))) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN split list of io req broken when commit\n");
            DMS_WARN_ON(true);
            continue;
        }

        ioSReq = cur;
        ci_stat = ioSReq->ciMDS_status;
        if (CI_MDS_OP_REPORT_NONE == ci_stat) {
            continue;
        }

        if (CI_MDS_OP_REPORT_FAILURE == ci_stat) {
            ret = true;
            atomic_inc(&io_req->num_of_ci2nn);
#if (DN_PROTOCOL_VERSION == 2) && (CN_CRASH_RECOVERY == 1)
            atomic_inc(&nn_commit_wait_resp);
#endif
            continue;
        }

        switch (ioSReq->mdata_opcode) {
        case REQ_NN_QUERY_MDATA_WRITEIO:
        case REQ_NN_ALLOC_MDATA_NO_OLD:
            if (_chk_writeDN_fail(&ioSReq->MetaData, io_req->drive)) {
                ret = true;
                atomic_inc(&io_req->num_of_ci2nn);
#if (DN_PROTOCOL_VERSION == 2) && (CN_CRASH_RECOVERY == 1)
                atomic_inc(&nn_commit_wait_resp);
#endif
            }
            break;

        default:
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN wrong with nn req ci2nn stat: %d rw: %d id: %u\n",
                    ci_stat, ioSReq->mdata_opcode, ioSReq->ioSR_comm.ReqID);
            DMS_WARN_ON(true);
                break;
        }
    }

    return ret;
}

static void _fWker_free_ioReq (io_request_t *io_req)
{
    if (unlikely(IS_ERR_OR_NULL(io_req))) {
        dms_printk(LOG_LVL_WARN,
                   "DSMC WARN commit finish io_req ptr is null\n");
        DMS_WARN_ON(true);
        return;
    }

    vm_dec_iocnt(io_req->drive, io_req->iodir);
    io_req->t_ioReq_ciMDS = jiffies_64;

    if (dms_client_config->show_performance_data) {
        vm_update_perf_data(io_req);
    }

    ioReq_end_request(io_req);
}

static int _IOR_fWker_fn (void *thread_data)
{
    IOR_freeWker_Mgr_t *mymgr;
    io_request_t *io_req;
    dmsc_thread_t *t_data;
    IOR_FSM_action_t action;

    t_data = (dmsc_thread_t *)thread_data;
    mymgr = (IOR_freeWker_Mgr_t *)t_data->usr_data;
    atomic_inc(&mymgr->num_fWker);

    while (t_data->running) {
        wait_event_interruptible(*(t_data->ctl_wq),
                (atomic_read(&mymgr->ioReq_freeQ_Size) > 0) || (!t_data->running));

        io_req = _fWKer_peek_ioReq(mymgr);
        if (IS_ERR_OR_NULL(io_req)) {
            continue;
        }

        if (_chk_dnerr_commit_nn(io_req)) {
            ioReq_submit_reportMDReq(io_req);
            continue;
        }

#if (DN_PROTOCOL_VERSION == 2) && (CN_CRASH_RECOVERY == 1)
        if (atomic_read(&nn_commit_wait_resp) == 0
                && io_req->iodir == KERNEL_IO_WRITE) {
            update_cleanLog_ts();
        }
#endif

        action = IOReq_update_state(&io_req->ioReq_stat, IOREQ_E_ReportMDDone);

#if 0
        if (IOREQ_A_DO_Healing == action) {
            dms_printk(LOG_LVL_INFO,
                    "DMSC INFO not support RR in clientnode now\n");
        } else {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN fail ioReq stat tr OvlpOtherIO %s %s\n",
                    IOReq_get_state_fsm_str(&io_req->ioReq_stat),
                    IOReq_get_action_str(action));
        }
        continue;
#endif

        action = IOReq_update_state(&io_req->ioReq_stat, IOREQ_E_HealDone);
        if (IOREQ_A_FreeReq != action) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN fail ioReq stat tr HealDone %s %s\n",
                    IOReq_get_state_fsm_str(&io_req->ioReq_stat),
                    IOReq_get_action_str(action));
            continue;
        }

        _fWker_free_ioReq(io_req);
    }

    atomic_dec(&mymgr->num_fWker);

    return 0;
}

void IORFWkerMgr_init_manager (void)
{
    uint16_t i;

    for (i = 0; i < IORFreeQ_TOTAL_NUM; i++) {
        spin_lock_init(&IOR_fWker_mgr.ioReq_freeQ_plock[i]);
        INIT_LIST_HEAD(&IOR_fWker_mgr.ioReq_freeQ_pool[i]);
    }
    atomic_set(&IOR_fWker_mgr.ioReq_freeQ_deqIdx, 0);
    atomic_set(&IOR_fWker_mgr.ioReq_freeQ_Size, 0);
    init_waitqueue_head(&IOR_fWker_mgr.ioReq_fWker_wq);

    atomic_set(&IOR_fWker_mgr.num_fWker, 0);

    for (i = 0; i < NUM_IO_FREE_THDS; i++) {
        IOR_fWker_mgr.ioReq_fWker[i] =
                create_worker_thread("dms_io_fwker",
                &IOR_fWker_mgr.ioReq_fWker_wq, &IOR_fWker_mgr, _IOR_fWker_fn);
        if (IS_ERR_OR_NULL(IOR_fWker_mgr.ioReq_fWker[i])) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN fail gen io free wker %d\n", i);
            DMS_WARN_ON(true);
        }
    }
}

void IORFWkerMgr_rel_manager (void)
{
    int32_t count, i, num_wker;

    num_wker = atomic_read(&IOR_fWker_mgr.num_fWker);
    for (i = 0; i < num_wker; i++) {
        if (stop_worker_thread(IOR_fWker_mgr.ioReq_fWker[i])) {
            dms_printk(LOG_LVL_WARN, "fail to stop NN IOR free worker %d\n", i);
            DMS_WARN_ON(true);
        }
    }

    count = 0;
    while (atomic_read(&IOR_fWker_mgr.num_fWker) != 0) {
        msleep(100);
        count++;

        if (count == 0 || count % 10 != 0) {
            continue;
        }

        dms_printk(LOG_LVL_WARN,
                "DMSC WARN fail stop IOR free worker %d thread running "
                "(wait count %d)\n",
                atomic_read(&IOR_fWker_mgr.num_fWker), count);
    }
}
