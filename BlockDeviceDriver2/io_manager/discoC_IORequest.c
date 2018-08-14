/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_IORequest.c
 */
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/blkdev.h>
#include <linux/types.h>
#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/dms_kernel_version.h"
#include "../common/discoC_mem_manager.h"
#include "../common/dms_client_mm.h"
#include "../common/thread_manager.h"
#include "../metadata_manager/metadata_manager.h"
#include "../metadata_manager/metadata_cache.h"
#include "discoC_IO_Manager.h"
#include "../vdisk_manager/volume_manager.h"
#include "discoC_IOReq_copyData_func.h"
#include "ioreq_ovlp.h"
#include "discoC_IOReqPool.h"
#include "discoC_IOReq_fsm.h"
#include "discoC_IODone_worker.h"
#include "discoC_IOSegment_Request.h"
#include "../discoNN_client/discoC_NNC_Manager_api.h"
#include "../config/dmsc_config.h"
#include "discoC_IORequest.h"
#include "../discoNN_client/discoC_NNC_ovw.h"
#include "discoC_IO_Manager_export.h"
#ifdef DISCO_PREALLOC_SUPPORT
#include "../metadata_manager/space_manager_export_api.h"
#endif

void ioReq_add_waitReq (io_request_t *ioreq, struct list_head *waitR)
{
    spin_lock(&ioreq->ovlp_group_lock);
    list_add_tail(waitR, &ioreq->wait_list);
    atomic_inc(&ioreq->cnt_ovlp_wait);
    spin_unlock(&ioreq->ovlp_group_lock);
}

void ioReq_add_wakeReq (io_request_t *ioreq, struct list_head *wakeR)
{
    spin_lock(&ioreq->ovlp_group_lock);
    list_add_tail(wakeR, &ioreq->wake_list);
    atomic_inc(&ioreq->cnt_ovlp_wake);
    spin_unlock(&ioreq->ovlp_group_lock);
}

bool ioReq_chkset_fea (io_request_t *ioreq, uint32_t fea_bit)
{
    return test_and_set_bit(fea_bit, (volatile void *)&(ioreq->ioReq_fea));
}

bool ioReq_chkclear_fea (io_request_t *ioreq, uint32_t fea_bit)
{
    return test_and_clear_bit(fea_bit, (volatile void *)&(ioreq->ioReq_fea));
}

static void _ioReq_free_wiobuff (io_request_t *ioreq)
{
    uint32_t i;
    data_mem_chunks_t *data;

    data = &ioreq->payload;
    for (i = 0; i < data->num_chunks; i++) {
        discoC_free_pool(IOMgr_get_IOWbuffer_mpoolID(), data->chunks[i]);
    }
    data->num_chunks = 0;
}

//TODO: Using calculation avoid compare size
static bool _ioReq_alloc_wiobuff (io_request_t *ioreq)
{
    int32_t lblen;
    uint32_t chunk_lb;
    uint16_t *cnt;
    bool ret, alloc_fail;
    data_mem_chunks_t *data;

    data = &ioreq->payload;
    memset(data->chunks, 0, sizeof(struct data_mem_item *)*MAX_NUM_MEM_CHUNK_BUFF);
    lblen = ioreq->usrIO_addr.lbid_len;

    chunk_lb = 0;
    alloc_fail = false;
    ret = false;
    cnt = &data->num_chunks;
    *cnt = 0;
    while (lblen > 0) {
        chunk_lb = (lblen >= MAX_LB_PER_MEM_CHUNK ? MAX_LB_PER_MEM_CHUNK : lblen);

        ioreq->payload.chunks[*cnt] =
                discoC_malloc_pool(IOMgr_get_IOWbuffer_mpoolID());
        if (IS_ERR_OR_NULL(ioreq->payload.chunks[*cnt])) {
            alloc_fail = true;
            break;
        }

        ioreq->payload.chunk_item_size[*cnt] = (chunk_lb << BIT_LEN_DMS_LB_SIZE);
        (*cnt)++;
        lblen -= chunk_lb;
    }

    if (alloc_fail) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN fail alloc mem for wbuff\n");
        DMS_WARN_ON(true);
        _ioReq_free_wiobuff(ioreq);
        ret = true;
    }

    return ret;
}

/*
 * return false : dms lb not align
 *        true : dms lb align
 */
static bool _ioReq_set_lbalign_mask (uint8_t *mask, uint64_t sect_begin, uint32_t sect_len)
{
    uint64_t sect_start, sect_end;
    uint64_t startbit, rangebit, bitlen, bitend, i, j;

    memset(mask, 0, sizeof(uint8_t)*MAX_LBS_PER_IOR);

    i = 0;
    sect_start = sect_begin;
    sect_end = sect_begin + sect_len;
    while (sect_start < sect_end) {
        startbit = sect_start % BIT_LEN_PER_1_MASK;
        rangebit = BIT_LEN_PER_1_MASK - startbit;
        if (sect_start + rangebit >= sect_end) {
            bitlen = sect_end - sect_start;
        } else {
            bitlen = rangebit;
        }

        bitend = bitlen + startbit;
        for (j = startbit; j < bitend; j++) {
            set_bit(j, (volatile void *)&(mask[i]));
        }
        i++;
        sect_start += bitlen;
    }

    if ((mask[0] == 0xff) && (mask[i-1] == 0xff)) {
        return true;
    }

    return false;
}

static void _ioReq_init_ovlpItem (io_request_t *ioreq)
{
    ioreq->is_ovlp_ior = false;
    INIT_LIST_HEAD(&ioreq->wait_list);
    INIT_LIST_HEAD(&ioreq->wake_list);
    atomic_set(&ioreq->cnt_ovlp_wait, 0);
    atomic_set(&ioreq->cnt_ovlp_wake, 0);
    spin_lock_init(&ioreq->ovlp_group_lock);
}

static void _ioReq_init_list (io_request_t *ioreq)
{
    spin_lock_init(&ioreq->ioSegReq_plock);
    INIT_LIST_HEAD(&ioreq->ioSegReq_lPool);
    INIT_LIST_HEAD(&ioreq->list2ioReqPool);
    INIT_LIST_HEAD(&ioreq->list2ioDoneWPool);
}

static void _ioReq_init_timeinfo (io_request_t *ioreq)
{
    ioreq->t_ioReq_create = jiffies_64;
    ioreq->t_ioReq_start = 0;
    ioreq->t_ioReq_ciUser = 0;
    ioreq->t_ioReq_ciMDS = 0;
}

int32_t ioReq_init_request (io_request_t *ioreq, volume_device_t *drv,
        priority_type req_prior, dms_rq_t **rqs,
        uint32_t index_s, uint32_t num_rqs, uint64_t sect_s, uint32_t sect_len)
{
    struct request *rq;
    data_mem_chunks_t *data;
    UserIO_addr_t *ioaddr;
    int32_t ret;

    ret = 0;

    atomic_set(&ioreq->ref_cnt, 1);

    ioaddr = &(ioreq->usrIO_addr);
    ioreq->ioReqID = IOMgr_alloc_ioReqID();
    ioreq->drive = drv;
    ioreq->reqPrior = req_prior;
    ioreq->kern_sect = sect_s + ioreq->drive->sects_shift;
    ioreq->nr_ksects = sect_len;
    ioreq->ioResult = true;
    ioreq->ioReq_fea = 0;

    ioaddr->metaSrv_ipaddr = drv->nnIPAddr;
    //ioaddr->metaSrv_port = dms_client_config->mds_ioport;
    ioaddr->volumeID = drv->vol_id;
    ioaddr->lbid_start = ((uint64_t)ioreq->kern_sect) >> BIT_LEN_DMS_LB_SECTS;
    ioaddr->lbid_len = ((ioreq->kern_sect + (uint64_t)ioreq->nr_ksects - 1L)
            >> BIT_LEN_DMS_LB_SECTS) - ioaddr->lbid_start + 1L;

    ioreq->ref_ioReqPool = volIOReqQMgr_find_pool(ioaddr->volumeID,
            volIOReqQMgr_get_manager());
    if (IS_ERR_OR_NULL(ioreq->ref_ioReqPool)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN fail find IOReq pool volume %u\n",
                ioaddr->volumeID);
        ret = -ENOENT;
        goto INIT_IOR_FAIL;
    }

    memcpy(ioreq->rqs, &(rqs[index_s]), sizeof(dms_rq_t *)*num_rqs);
    ioreq->num_rqs = num_rqs;
    rq = rqs[index_s]->rq;
    ioreq->iodir = rq_data_dir(rq);
    init_rwsem(&ioreq->rq_lock);

    IOReq_init_fsm(&ioreq->ioReq_stat);

    _ioReq_init_list(ioreq);
    _ioReq_init_timeinfo(ioreq);

    atomic_set(&ioreq->ciUser_remainLB, ioaddr->lbid_len);
    atomic_set(&ioreq->ciMDS_remainLB, ioaddr->lbid_len);
    atomic_set(&ioreq->num_of_ci2nn, 0);

    _ioReq_init_ovlpItem(ioreq);

    if (ioreq->iodir != KERNEL_IO_READ) {
        if (_ioReq_alloc_wiobuff(ioreq)) {
            ret = -ENOMEM;
            goto INIT_IOR_FAIL;
        }
    } else {
       data = &ioreq->payload;
       memset(data->chunks, 0,
               sizeof(data_mem_item_t *)*MAX_NUM_MEM_CHUNK_BUFF);
       data->num_chunks = 0;
    }

    memset(ioreq->mask_lb_align, 0, sizeof(uint8_t)*MAX_LBS_PER_IOR);
    ioreq->is_mdcache_on = is_vol_mcache_enable(ioreq->drive);
    ioreq->is_dms_lb_align = _ioReq_set_lbalign_mask(ioreq->mask_lb_align,
                             ioreq->kern_sect, ioreq->nr_ksects);

    if (ioreq->iodir == KERNEL_IO_WRITE) {
        ioReq_WCopyData_bio2mem(ioreq);
    }

    memset(ioreq->fp_map, 0, sizeof(int8_t)*MAX_SIZE_FPMAP);
    ioreq->volume_replica = get_volume_replica(ioreq->drive);

    vm_inc_iocnt(ioreq->drive, ioreq->iodir);

    return ret;

INIT_IOR_FAIL:
    ccma_free_pool(MEM_Buf_IOReq, (int8_t *)ioreq);

    return ret;
}

static bool _chk_ovlp_ReqPool (io_request_t *io_req, struct list_head *lhead,
        int32_t *errno)
{
    IOR_ovlp_res_t ovlp_ret;
    bool ovlp_wait;

    ovlp_wait = false;
    *errno = 0;

    ovlp_ret = ioReq_chk_ovlp(lhead, io_req);

    switch (ovlp_ret) {
    case IOR_ovlp_NoOvlp:
        break;
    case IOR_ovlp_Ovlp:
        ovlp_wait = true;
        break;
    case IOR_ovlp_InQ:
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN ior should not in ovlp q %d\n", io_req->ioReqID);
        DMS_WARN_ON(true);
        *errno = -EEXIST;
        ovlp_wait = true;
        break;
    case IOR_ovlp_ListBroken:
        *errno = -EFAULT;
        ovlp_wait = true;
        break;
    default:
        dms_printk(LOG_LVL_WARN,  "DMSC WARN wrong ovlp type %d\n", ovlp_ret);
        DMS_WARN_ON(true);
        *errno = -EFAULT;
        ovlp_wait = true;
        break;
    }

    return ovlp_wait;
}

bool ioReq_add_ReqPool (io_request_t *io_req)
{
    volume_IOReqPool_t *volIOR_pool;
    int32_t errno;
    IOR_FSM_action_t action;
    bool add_ovlp_q, submit_req;

    add_ovlp_q = false;
    submit_req = true;

    volIOR_pool = io_req->ref_ioReqPool;
    IOReqPool_gain_lock(volIOR_pool);

    add_ovlp_q = _chk_ovlp_ReqPool(io_req, &volIOR_pool->ioReq_ovlp_pool, &errno);
    if (errno) {
        //TODO: if we free io req, we should release all ovlp_item
        goto ADD_IOR_REQPOOL_ERR;
    }

    add_ovlp_q |= _chk_ovlp_ReqPool(io_req, &volIOR_pool->ioReq_pool, &errno);
    if (errno) {
        //TODO: if we free io req, we should release all ovlp_item
        goto ADD_IOR_REQPOOL_ERR;
    }

    if (add_ovlp_q) {
        action = IOReq_update_state(&io_req->ioReq_stat, IOREQ_E_OvlpOtherIO);
        if (IOREQ_A_AddOvlpWaitQ == action) {
            IOReqPool_addOvlpReq_nolock(io_req, volIOR_pool);
        } else {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN fail ioReq stat tr OvlpOtherIO %s %s\n",
                    IOReq_get_state_fsm_str(&io_req->ioReq_stat),
                    IOReq_get_action_str(action));
        }

        submit_req = false;
    } else {
        action = IOReq_update_state(&io_req->ioReq_stat, IOREQ_E_INIT_DONE);
        if (IOREQ_A_SubmitIO == action) {
            IOReqPool_addReq_nolock(io_req, volIOR_pool);
        } else {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN fail ioReq stat tr INIT_DONE %s %s\n",
                    IOReq_get_state_fsm_str(&io_req->ioReq_stat),
                    IOReq_get_action_str(action));
        }
    }

    IOReqPool_rel_lock(volIOR_pool);

    return submit_req;

ADD_IOR_REQPOOL_ERR:
    IOReqPool_rel_lock(volIOR_pool);

    if (errno == -EFAULT) {
        ioReq_commit_user(io_req, false);
        vm_dec_iocnt(io_req->drive, io_req->iodir);
        ccma_free_pool(MEM_Buf_IOReq, (int8_t *)io_req);
    }

    return false;
}

void ioReq_IODone_endfn (io_request_t *ioReq, uint32_t num_LBs)
{
    int32_t ret;
    IOR_FSM_action_t action;

    do {
        //NOTE: Lego-20170920: this maybe a redundant protection due to protected by
        //                     io seg request commit state and atomic of io request
        //                     but I still implement it
        action = IOReq_update_state(&ioReq->ioReq_stat, IOREQ_E_IOSeg_IODone);
        if (action != IOREQ_A_IOSegIODoneCIIOR) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN fail ioReq stat tr IOSeg_IODone %s %s\n",
                    IOReq_get_state_fsm_str(&ioReq->ioReq_stat),
                    IOReq_get_action_str(action));
            break;
        }

        ret = atomic_sub_return(num_LBs, &ioReq->ciMDS_remainLB);
        if (ret) {
            break;
        }

        action = IOReq_update_state(&ioReq->ioReq_stat, IOREQ_E_RWUDataDone);
        if (IOREQ_A_DO_ReportMD != action) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN fail ioReq stat tr RWUdataDone %s %s\n",
                    IOReq_get_state_fsm_str(&ioReq->ioReq_stat),
                    IOReq_get_action_str(action));
            break;
        }

        IORFWkerMgr_submit_request(ioReq);
    } while (0);
}

void ioReq_MDReport_endfn (io_request_t *ioReq, uint32_t num_LBs)
{
    int32_t ret;
    IOR_FSM_action_t action;

    do {
        //NOTE: Lego-20170920: this maybe a redundant protection due to protected by
        //                     io seg request report md state and atomic of io request
        //                     but I still implement it
        action = IOReq_update_state(&ioReq->ioReq_stat, IOREQ_E_IOSeg_ReportMDDone);
        if (action != IOREQ_A_IOSegMDReportDoneCIIOR) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN fail ioReq stat tr IOSeg_ReportMDDone %s %s\n",
                    IOReq_get_state_fsm_str(&ioReq->ioReq_stat),
                    IOReq_get_action_str(action));
            break;
        }

        ret = atomic_sub_return(1, &ioReq->num_of_ci2nn);
        if (ret) {
            break;
        }

        action = IOReq_update_state(&ioReq->ioReq_stat, IOREQ_E_ReportMDDone);
        if (IOREQ_A_DO_Healing != action) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN fail ioReq stat tr ReportMDDone %s %s\n",
                    IOReq_get_state_fsm_str(&ioReq->ioReq_stat),
                    IOReq_get_action_str(action));
            break;
        }

        IORFWkerMgr_submit_request(ioReq);
    } while (0);
}

/*
 * NOTE: This function will decrease the reference cnt
 */
static bool mv_ovlp_ior_toIO (io_request_t *io_req)
{
    volume_IOReqPool_t *volIOR_pool;
    struct list_head *ovlp_q_head;
    struct list_head buffer_queue;
    io_request_t *cur, *next;
    int32_t num_ovlp_ior;
    uint16_t nn_req;

    dms_printk(LOG_LVL_DEBUG, "DMSC DEBUG Ready to move io req from overlap "
               "to pending with %d (start %llu len %d)\n",
               io_req->ioReqID, io_req->kern_sect, io_req->nr_ksects);
    INIT_LIST_HEAD(&buffer_queue);

    volIOR_pool = io_req->ref_ioReqPool;
    ovlp_q_head = &volIOR_pool->ioReq_ovlp_pool;

    num_ovlp_ior = ioReq_get_ovlpReq(ovlp_q_head, &buffer_queue, io_req);

    if (num_ovlp_ior < 0) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN Overlap "
                   "pending q list broken of io req %d (vol: %d)\n",
                   io_req->ioReqID, io_req->drive->vol_id);
        DMS_WARN_ON(true);
    }

    if (num_ovlp_ior <= 0) {
        return false;
    }

    nn_req = 0;
    list_for_each_entry_safe (cur, next, &buffer_queue, list2ioReqPool) {
        IOReqPool_rmOvlpReq_nolock(cur);

        atomic_inc(&cur->ref_cnt);

        IOReqPool_addReq_nolock(cur, volIOR_pool);
        ioReq_gen_ioSegReq(cur);
        ioReq_submit_request(cur);

        nn_req++;
    }

    return (nn_req > 0);
}

/*
 * remove an element from the io request pending list
 */
static void _ioReq_rm_ioReqQueue (io_request_t *io_req)
{
    volume_IOReqPool_t *volIOR_pool;
    bool wakeup_nn;

    wakeup_nn = false;

    volIOR_pool = io_req->ref_ioReqPool;
    IOReqPool_gain_lock(volIOR_pool);
    IOReqPool_rmReq_nolock(io_req);
    wakeup_nn = mv_ovlp_ior_toIO(io_req);
    IOReqPool_rel_lock(volIOR_pool);

    if (wakeup_nn) {        // wake up nn_req worker to handle nn_req
    	NNClientMgr_wakeup_NNCWorker();
    }
}

static void _ioReq_free_request (io_request_t *io_req)
{
    _ioReq_free_wiobuff(io_req);
    ccma_free_pool(MEM_Buf_IOReq, (int8_t *)io_req);
}

void ioReq_end_request (io_request_t *io_req)
{
    ioSeg_req_t *cur, *next;

    if (unlikely(IS_ERR_OR_NULL(io_req))) {
        dms_printk(LOG_LVL_ERR,
                "DMSC ERROR try to remove an null io req\n");
        DMS_WARN_ON(true);
        return;
    }

    list_for_each_entry_safe (cur, next, &io_req->ioSegReq_lPool, list2ioReq) {
        list_del(&cur->list2ioReq);
        ioSReq_free_request(cur);
    }

    _ioReq_rm_ioReqQueue(io_req);
    ioReq_rm_all_ovlpReq(io_req);

    _ioReq_free_request(io_req);
    io_req = NULL;
}

void ioReq_submit_request (io_request_t *ior)
{
    ioSeg_req_t *cur, *next;
    uint32_t nnIPAddr, nnPort;

    if (IOREQ_S_RWUserData != IOReq_get_state(&ior->ioReq_stat)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN submit IOReq acquire MD state err %s\n",
                IOReq_get_state_fsm_str(&ior->ioReq_stat));
        return;
    }

    nnIPAddr = ior->drive->nnIPAddr;
    nnPort = dms_client_config->mds_ioport;
    list_for_each_entry_safe (cur, next, &ior->ioSegReq_lPool, list2ioReq) {
        ioSReq_submit_acquireMD(cur, ior->is_mdcache_on, ior->volume_replica);
    }

    atomic_dec(&ior->ref_cnt);
}

void ioReq_submit_reportMDReq (io_request_t *ior)
{
    ioSeg_req_t *cur, *next;
    uint32_t nnIPAddr, nnPort;

    if (IOREQ_S_ReportMD != IOReq_get_state(&ior->ioReq_stat)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN submit IOReq report MD state err %s\n",
                IOReq_get_state_fsm_str(&ior->ioReq_stat));
        return;
    }

    nnIPAddr = ior->drive->nnIPAddr;
    nnPort = dms_client_config->mds_ioport;

    list_for_each_entry_safe (cur, next, &ior->ioSegReq_lPool, list2ioReq) {
        ioSReq_submit_reportMD(cur, ior->is_mdcache_on, ior->volume_replica);
    }
}

static void ioReq_add_ioSegReq (io_request_t *io_req, ioSeg_req_t *ioSegReq)
{
    spin_lock(&io_req->ioSegReq_plock);
    list_add_tail(&ioSegReq->list2ioReq, &io_req->ioSegReq_lPool);
    spin_unlock(&io_req->ioSegReq_plock);

    atomic_inc(&discoCIO_Mgr.cnt_split_io_req);
}

static void _copy_metadata_array (metadata_t *src, metadata_t *dst)
{
    dst->array_metadata = src->array_metadata;
    memcpy(&(dst->usr_ioaddr), &(src->usr_ioaddr), sizeof(UserIO_addr_t));
    dst->num_MD_items = src->num_MD_items;
    dst->num_invalMD_items = src->num_invalMD_items;
}

#ifdef DISCO_PREALLOC_SUPPORT
static int32_t _query_prealloc_MDcache (io_request_t *ioreq,
        uint64_t lb_s, uint32_t lb_len, uint64_t payload_off, uint8_t *msk_ptr,
        bool is4kalign)
{
    metadata_t arr_MD_bk[MAX_LBS_PER_IOR];
    metadata_t *mdItem;
    ioSeg_req_t *ioSegReq;
    int32_t ret, num_bk, i;
    uint64_t new_payload_offset;
    uint8_t *lmsk_ptr;
    bool chstat;

    ret = 0;

    new_payload_offset = payload_off;
    lmsk_ptr = msk_ptr;

    num_bk = sm_query_volume_MDCache(arr_MD_bk, ioreq->drive->vol_id,
            lb_s, lb_len);


    for (i = 0; i < num_bk; i++) {
        mdItem = &(arr_MD_bk[i]);

        mdItem->usr_ioaddr.metaSrv_ipaddr = ioreq->usrIO_addr.metaSrv_ipaddr;
        mdItem->usr_ioaddr.metaSrv_port = ioreq->usrIO_addr.metaSrv_port;
        ioSegReq = ioSReq_alloc_request();
        ioSReq_init_request(ioSegReq, &mdItem->usr_ioaddr, ioreq->iodir, ioreq->reqPrior);

        if (mdItem->num_MD_items) { //NOTE: won't < 0, so we avoid int compare
            chstat = true;
            _copy_metadata_array(mdItem, &ioSegReq->MetaData);
        } else {
            chstat = false;
        }

        ioSReq_set_request(ioSegReq, ioreq, lmsk_ptr, new_payload_offset,
                true, chstat, is4kalign);

        ioReq_add_ioSegReq(ioreq, ioSegReq);

        new_payload_offset += (mdItem->usr_ioaddr.lbid_len << BIT_LEN_DMS_LB_SIZE);
        lmsk_ptr += mdItem->usr_ioaddr.lbid_len;
    }

    return ret;
}
#endif

//return value: how many bytes shift for payload in io request
static int32_t _ioReq_gen_ioSReq_mdcache_state (io_request_t *ioreq,
        uint64_t beginning, uint32_t len,
        int32_t type, uint64_t payload_offset, uint8_t *mask_ptr)
{
    metadata_t arr_MD_bk[MAX_LBS_PER_IOR];
    metadata_t *mdItem;
    ioSeg_req_t *ioSegReq;
    uint64_t ioSReq_pl_offset;
    int32_t i, num_arr_MD;
    uint8_t *lmsk_ptr;
    bool query_mdcache, ovw_stat, chstat, is4Kalign;

    query_mdcache = true;
    do {
        if (false == ioreq->is_mdcache_on) {
            query_mdcache = false;
            break;
        }

        if (LBA_FEA_FW_4KALIGN == type || LBA_FEA_FW_non4KALIGN == type) {
            query_mdcache = false;
            break;
        }
    } while (0);

    num_arr_MD = query_volume_MDCache(arr_MD_bk, ioreq->drive->vol_id,
            beginning, len, ioreq->reqPrior, query_mdcache);
    if (num_arr_MD <= 0) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN get metadata cache error\n");
        DMS_WARN_ON(true);
        return (len << BIT_LEN_DMS_LB_SIZE);
    }

    ioSReq_pl_offset = payload_offset;
    lmsk_ptr = mask_ptr;

    if (LBA_FEA_OVW_4KALIGN  == type || LBA_FEA_OVW_non4KALIGN == type) {
        ovw_stat = true;
    } else {
        ovw_stat = false;
    }

    if (LBA_FEA_OVW_4KALIGN  == type || LBA_FEA_FW_4KALIGN == type) {
        is4Kalign = true;
    } else {
        is4Kalign = false;
    }

    for (i = 0; i < num_arr_MD; i++) {
        mdItem = &(arr_MD_bk[i]);

#ifdef DISCO_PREALLOC_SUPPORT
        if (mdItem->num_MD_items == 0 && ovw_stat) {
            if (_query_prealloc_MDcache(ioreq,
                    mdItem->usr_ioaddr.lbid_start, mdItem->usr_ioaddr.lbid_len,
                    ioSReq_pl_offset, lmsk_ptr, is4Kalign)) {
                dms_printk(LOG_LVL_WARN, "DMSC WARN fail query prealloc MDcache\n");
                //TODO: should commit errors
            }

            ioSReq_pl_offset += (mdItem->usr_ioaddr.lbid_len << BIT_LEN_DMS_LB_SIZE);
            lmsk_ptr += mdItem->usr_ioaddr.lbid_len;
            continue;
        }
#endif

        mdItem->usr_ioaddr.metaSrv_ipaddr = ioreq->usrIO_addr.metaSrv_ipaddr;
        mdItem->usr_ioaddr.metaSrv_port = ioreq->usrIO_addr.metaSrv_port;
        ioSegReq = ioSReq_alloc_request();
        ioSReq_init_request(ioSegReq, &mdItem->usr_ioaddr, ioreq->iodir,
                ioreq->reqPrior);

        if (mdItem->num_MD_items) { //NOTE: won't < 0, so we avoid int compare
            chstat = true;
            _copy_metadata_array(mdItem, &ioSegReq->MetaData);
        } else {
            chstat = false;
        }

        ioSReq_set_request(ioSegReq, ioreq, lmsk_ptr, ioSReq_pl_offset,
                ovw_stat, chstat, is4Kalign);
        ioReq_add_ioSegReq(ioreq, ioSegReq);

        ioSReq_pl_offset += (mdItem->usr_ioaddr.lbid_len << BIT_LEN_DMS_LB_SIZE);
        lmsk_ptr += mdItem->usr_ioaddr.lbid_len;
    }

    return (len << BIT_LEN_DMS_LB_SIZE);
}

static bool _chk_max_len_allocMD (uint16_t io_rw, int8_t split_type, uint32_t len)
{
    bool ret = false;

    do {
         if (io_rw == KERNEL_IO_READ) {
            break;
         }

         //NOTE: non-4K align only appear at left-most LB and right-most LB
         //      we generate 2 requests to read and write
         if (split_type == LBA_FEA_OVW_non4KALIGN ||
                 split_type == LBA_FEA_FW_non4KALIGN) {
             ret = true;
             break;
         }

        //write and ovw=true
        if (split_type == LBA_FEA_OVW_4KALIGN ||
            split_type == LBA_FEA_OVW_non4KALIGN) {
            break;
        }

        if (len < MAX_LBS_PER_ALLOCMD_REQ) {
            break;
        }

        ret = true;
    } while (0);

    return ret;
}

static int32_t find_cont_len (int8_t *group, uint32_t max_len, uint16_t io_rw)
{
    int32_t len, i;

    len = 1;
    for (i = 1; i < max_len; i++) {
        if (LBA_FEA_FW_non4KALIGN == group[0] ||
                LBA_FEA_OVW_non4KALIGN == group[0]) {
            break;
        }

        if (group[0] != group[i]) {
            break;
        }

        len++;
        if (_chk_max_len_allocMD(io_rw, group[i], len)) {
            break;
        }
    }

    return len;
}

static void _ioReq_get_LBOVW_state (io_request_t * io_req, int8_t *arr_lbovw,
        uint32_t arr_max_len)
{
    uint64_t lbid;
    uint32_t i, total_len;
    bool is_ovw;

    memset(arr_lbovw, LBA_FEA_INIT, sizeof(int8_t)*arr_max_len);

    total_len = io_req->usrIO_addr.lbid_len;
    lbid = io_req->usrIO_addr.lbid_start;

    for (i = 0; i < total_len; i++) {
        if (KERNEL_IO_WRITE == io_req->iodir) {
            is_ovw = ovw_get(io_req->drive, lbid+i);
        } else {
            is_ovw = true;
        }

        if (is_ovw && io_req->mask_lb_align[i] == 0xff) {
            arr_lbovw[i] = LBA_FEA_OVW_4KALIGN;
        } else if(is_ovw && io_req->mask_lb_align[i] != 0xff) {
            arr_lbovw[i]= LBA_FEA_OVW_non4KALIGN;
        } else if(!is_ovw && io_req->mask_lb_align[i] == 0xff) {
            arr_lbovw[i] = LBA_FEA_FW_4KALIGN;
        } else if (false == is_ovw) {
            arr_lbovw[i] = LBA_FEA_FW_non4KALIGN;
        }
    }

    arr_lbovw[total_len] = LBA_FEA_END;
}

/*
 * Return: 0 split OK
 *         -EFAULT: fail
 */
static int32_t _ioReq_gen_ioSReq_ovw (io_request_t * io_req)
{
    int8_t grouping[max_group_size];
    UserIO_addr_t *ioaddr;
    uint64_t payload_offset;
    uint8_t *mask_ptr;
    int32_t i, ret, len, total_len;

    _ioReq_get_LBOVW_state(io_req, grouping, max_group_size);

    ioaddr = &(io_req->usrIO_addr);
    total_len = ioaddr->lbid_len;

    payload_offset = 0;
    mask_ptr = io_req->mask_lb_align;

    i = 0;
    do {
        len = find_cont_len(&(grouping[i]), total_len - i, io_req->iodir);

        dms_printk(LOG_LVL_DEBUG, "DMSC DEBUG group: begin=%d, len=%d, "
                "total_len=%d group_value=%d rid=%d "
                "payload offset %llu\n",
                i, len, ioaddr->lbid_len, grouping[i],
                io_req->ioReqID, payload_offset);
        ret = _ioReq_gen_ioSReq_mdcache_state(io_req,
                ioaddr->lbid_start + i, len, grouping[i],
                payload_offset, mask_ptr);
        if (ret == 0) {
            dms_printk(LOG_LVL_ERR,
                    "DMSC ERROR: fail to gen ioSegReq when split\n");
            return -EFAULT;
        }

        payload_offset += ret;
        mask_ptr = mask_ptr + len;
        i += len;
    }  while (i < total_len);

    return 0;
}

/*
 * Return: 0: ok
 *         -EFAULT: fail
 */
int32_t ioReq_gen_ioSegReq (io_request_t *ior)
{
    int32_t retCode;

    dms_printk(LOG_LVL_DEBUG, "DMSC DEBUG vol:%d rid:%d rw=%d sector:%llu "
            "nr_sectors:%d startLB:%llu LB_cnt:%d\n",
            ior->drive->vol_id, ior->ioReqID, ior->iodir,
            ior->kern_sect, ior->nr_ksects,
            ior->usrIO_addr.lbid_start, ior->usrIO_addr.lbid_len);

    ior->t_ioReq_start = jiffies_64;

    retCode = _ioReq_gen_ioSReq_ovw(ior);

    if (retCode) {
        dms_printk(LOG_LVL_ERR,
                "DMSC ERROR fail split io (%u) into ioSegReq\n", ior->ioReqID);
        DMS_WARN_ON(true);
        goto GEN_FAIL_EXIST;
    }

    return retCode;

GEN_FAIL_EXIST:
    ioReq_commit_user(ior, false);
    atomic_dec(&ior->ref_cnt);
    vm_dec_iocnt(ior->drive, ior->iodir);
    ioReq_end_request(ior);
    return retCode;
}

/*
 * end the I/O request and commit to user
 */
void ioReq_commit_user (io_request_t *io_req, bool result)
{
    struct rw_semaphore *rq_lock;
    uint32_t i;
    dms_rq_t *dmsrq;

    if (ioReq_chkset_fea(io_req, IOREQ_FEA_CIUSER)) {
        return;
    }

    if (result == false) {
        io_req->drive->io_cnt.error_commit_cnt++;
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN something wrong, commit user error io req id %d\n",
                io_req->ioReqID);
    }

    rq_lock = &io_req->rq_lock;

    down_write(rq_lock);
    for (i = 0; i < io_req->num_rqs; i++) {
        dmsrq = io_req->rqs[i];
        dmsrq->result &= result;

        if (atomic_sub_return(1, &dmsrq->num_ref_ior) == 0) {
            commit_user_request(dmsrq->rq, dmsrq->result);
            dmsrq->rq = NULL;
            memset((int8_t *)dmsrq, 0, sizeof(dms_rq_t));

            up_write(rq_lock);

            IOMgr_free_dmsrq(dmsrq);

            down_write(rq_lock);
        }

        io_req->rqs[i] = NULL;
    }

    up_write(rq_lock);

    io_req->t_ioReq_ciUser = jiffies_64;
}
