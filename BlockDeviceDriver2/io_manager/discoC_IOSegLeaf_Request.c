/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_IOSegLeaf_Request.c
 */
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/types.h>
#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/dms_kernel_version.h"
#include "../common/discoC_mem_manager.h"
#include "../metadata_manager/metadata_manager.h"
#include "../payload_manager/payload_manager_export.h"
#include "discoC_IO_Manager.h"
#include "discoC_IOSegment_Request.h"
#include "discoC_IOReq_copyData_func.h"
#include "discoC_IO_Manager.h"
#include "discoC_IOSegLeaf_Request.h"
#include "../vdisk_manager/volume_manager.h"
#include "../discoNN_client/discoC_NNC_Manager_api.h"
#include "discoC_ioSegReq_udata_cbfn.h"

/**
 * _ioSleafR_alloc_reqID - allocate an io seg leaf req ID
 *
 * Return: 4 bytes request ID
 */
static uint32_t _ioSleafR_alloc_reqID (void)
{
    return (uint32_t)atomic_add_return(1, &discoCIO_Mgr.ioSleafR_ID_gentor);
}

/**
 * ioSegLeafReq_alloc_request - allocate io seg leaf req from memory pool
 *
 * Return: an io seg leaf req or NULL
 */
ioSeg_leafReq_t *ioSegLeafReq_alloc_request (void)
{
    return discoC_malloc_pool(discoCIO_Mgr.ioSeg_leafR_mpool_ID);
}

/**
 * ioSegLeafReq_free_request - free io seg leaf req to memory pool
 * @ioSeg_leafR: target io seg leaf request for free
 */
void ioSegLeafReq_free_request (ioSeg_leafReq_t *ioSeg_leafR)
{
    int32_t i;

    for (i = 0; i < MAX_MDATA_VERSION; i++) {
        free_metadata(&(ioSeg_leafR->ioSleafR_MData[i]));
    }

    discoC_free_pool(discoCIO_Mgr.ioSeg_leafR_mpool_ID, ioSeg_leafR);
}

/**
 * ioSegLeafReq_init_request - initialize a io seg leaf Req
 * @ioSeg_leafR: target req to initialize
 * @lb_s: start lbid that io seg leaf req should handle
 * @root_ioSR: caller io seg req of ioSeg_leafR
 */
void ioSegLeafReq_init_request (ioSeg_leafReq_t *ioSeg_leafR, uint64_t lb_s,
        ioSeg_req_t *root_ioSR)
{
    ReqCommon_t *ioSR_comm;
    uint32_t lb_off, i;

    INIT_LIST_HEAD(&ioSeg_leafR->entry_ioSegReq);
    //TODO: init req state

    ioSeg_leafR->ref_ioSegReq = root_ioSR;
    memcpy(&(ioSeg_leafR->ioSleafR_uioaddr),
            &(root_ioSR->ioSReq_ioaddr), sizeof(UserIO_addr_t));
    ioSeg_leafR->ioSleafR_uioaddr.lbid_start = lb_s;
    ioSeg_leafR->ioSleafR_uioaddr.lbid_len = 1;

    lb_off = lb_s - root_ioSR->ioSReq_ioaddr.lbid_start;
    ioSeg_leafR->lb_sect_mask = &(root_ioSR->lb_sect_mask[lb_off]);
    ioSeg_leafR->payload_offset = root_ioSR->payload_offset +
            (lb_off * DMS_LB_SIZE);
    atomic_set(&ioSeg_leafR->ioSleafR_MData_ver, 0);
    ioSeg_leafR->ciUser_result = true;

    INIT_LIST_HEAD(&ioSeg_leafR->list_plReqPool);
    atomic_set(&ioSeg_leafR->num_plReq, 0);
    spin_lock_init(&ioSeg_leafR->plReq_plock);

    ioSR_comm = &ioSeg_leafR->ioSleafR_comm;
    ioSR_comm->ReqID = _ioSleafR_alloc_reqID();
    ioSR_comm->ReqFeature = 0;
    ioSR_comm->ReqPrior = PRIORITY_LOW;
    ioSR_comm->ReqOPCode = KERNEL_IO_READ;
    atomic_set(&ioSR_comm->ReqRefCnt, 0);

    for (i = 0; i < MAX_MDATA_VERSION; i++) {
        init_metadata(&(ioSeg_leafR->ioSleafR_MData[i]),
                &ioSeg_leafR->ioSleafR_uioaddr);
    }
}

/**
 * _cb_ioSleafR_add_plReq - callback function of add payload request into ioSeg leaf
 *     for registering to payload request
 * @myReq: callback argument (is a io seg leaf req)
 * @entry_plReq: payload request entry
 */
static void _cb_ioSleafR_add_plReq (void *myReq, struct list_head *entry_plReq)
{
    ioSeg_leafReq_t *ioS_leafR;

    ioS_leafR = (ioSeg_leafReq_t *)myReq;
    spin_lock(&ioS_leafR->plReq_plock);

    list_add_tail(entry_plReq, &ioS_leafR->list_plReqPool);
    atomic_inc(&ioS_leafR->num_plReq);

    spin_unlock(&ioS_leafR->plReq_plock);
}

/**
 * _cb_ioSleafR_rm_plReq - callback function of rm payload request from ioSeg leaf
 * @myReq: callback argument (is a io seg leaf req)
 * @entry_plReq: payload request entry
 *
 * Return: number of remain payload request in pool
 */
static int32_t _cb_ioSleafR_rm_plReq (void *myReq, struct list_head *entry_plReq)
{
    ioSeg_leafReq_t *ioS_leafR;
    int32_t num_rPReq;

    ioS_leafR = (ioSeg_leafReq_t *)myReq;
    spin_lock(&ioS_leafR->plReq_plock);

    list_del(entry_plReq);

    num_rPReq = atomic_sub_return(1, &ioS_leafR->num_plReq);

    spin_unlock(&ioS_leafR->plReq_plock);

    return num_rPReq;
}

/**
 * _ioSleafR_chk_MDStat - check metadata status whether commit to usert
 * @MData: metadata of io seg leaf req to check whether commit to user
 * @mdres: get metadata result from nn client module
 *
 * Return: whether a validate metadata
 *     0 : validate
 *    ~0 : invalidate
 */
static int32_t _ioSleafR_chk_MDStat (metadata_t *MData, int32_t mdres)
{
    if (mdres) {
        //result of get metadata request : fail
        return -EFAULT;
    }

    if (MData->num_invalMD_items > 0) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN get inval MD when query true\n");
        return -EFAULT;
    }

    return 0;
}

/**
 * _cb_ioSleafR_read_cpData - callback function of copy data for payload request
 *     copy data to ioSeg leaf Req
 * @user_data: callback argument (is a io seg leaf req)
 * @mdItem: metadata that contains information payload request can get data from DN
 * @payload_off: where the offset of payload should copy to buffer
 * @payload: payload provide by payload request
 */
static void _cb_ioSleafR_read_cpData (void *user_data, metaD_item_t *mdItem,
        uint64_t payload_off, data_mem_chunks_t *payload)
{
    ioSeg_leafReq_t *ioS_leafR;
    io_request_t *ioReq;

    ioS_leafR = (ioSeg_leafReq_t *)user_data;

    ioReq = ioS_leafR->ref_ioSegReq->ref_ioReq;

    ioReq_RCopyData_mem2bio(ioReq, mdItem->lbid_s, mdItem->num_hbids,
            payload_off, payload);
}

/**
 * _cb_ioSleafR_queryTrueMD - when DN return hbid not match, io seg leaf req should re-query NN again
 * @myReq: callback argument (is a io seg leaf req)
 * @lbid_s: start lbid for re-query metadata
 * @lb_len: lbid length for re-query metadata
 *
 * Return: process result: 0 : sucess
 *                        ~0 : process fail
 * TODO: implement me
 */
static int32_t _cb_ioSleafR_queryTrueMD (void *myReq, uint64_t lbid_s, uint32_t lb_len)
{
    ioSeg_leafReq_t *ioS_leafR;

    ioS_leafR = (ioSeg_leafReq_t *)myReq;

    return 0;
}

/**
 * _cb_ioSleafR_ReadPL_endfn - callback function for payload request notify io seg leaf Req
 *     when request done
 * @myReq: callback argument (is a io seg leaf req)
 * @calleeReq: callee payload request
 * @lb_s: start lbid of callee req
 * @lb_len: lbid length of callee req
 * @ioRes: payload request read payload result
 * @numFailDN: number of failed DN
 *
 * Return: callback function hanlding result
 *     0: no error
 *    ~0: has error
 * TODO: need commit DN error to namenode
 */
static int32_t _cb_ioSleafR_ReadPL_endfn (void *user_data, ReqCommon_t *calleeReq,
        uint64_t lb_s, uint32_t lb_len, bool ioRes, int32_t numFailDN)
{
    ioSeg_leafReq_t *ioS_leafR;
    int32_t numPReq, ret;

    ioS_leafR = (ioSeg_leafReq_t *)user_data;
    ret = 0;

    ioS_leafR->ciUser_result &= ioRes;
    numPReq = atomic_read(&ioS_leafR->num_plReq);

    ioSReq_rm_ioSLeafReq(ioS_leafR->ref_ioSegReq, &ioS_leafR->entry_ioSegReq);
    ioSReq_ReadPL_endfn(ioS_leafR->ref_ioSegReq, &ioS_leafR->ioSleafR_comm,
            ioS_leafR->ioSleafR_uioaddr.lbid_start,
            ioS_leafR->ioSleafR_uioaddr.lbid_len,
            ioS_leafR->ciUser_result, 0);

    ioSegLeafReq_free_request(ioS_leafR);

    return ret;
}

/**
 * _ioSleafR_acquireMD_endfn - callback function for NN MD request notify io seg leaf Req
 *     when request done
 * @MData: metadata got from NN module
 * @calleeReq: metadata request that get metadata from nn
 * @result: NN metadata request process result
 *
 * Return: callback function hanlding result
 *     0: no error
 *    ~0: has error
 */
static int32_t _cb_ioSleafR_acquireMD_endfn (metadata_t *MData, void *private_data,
        ReqCommon_t *calleeReq, int32_t result)
{
    PLReq_func_t plReq_func;
    ioSeg_leafReq_t *ioS_leafR;
    ReqCommon_t *ioS_leafR_comm;
    io_request_t *ioReq;
    int32_t numLB_inval;

    ioS_leafR = (ioSeg_leafReq_t *)private_data;
    ioReq = ioS_leafR->ref_ioSegReq->ref_ioReq;
    ioS_leafR_comm = &ioS_leafR->ioSleafR_comm;

    if (_ioSleafR_chk_MDStat(MData, result)) {
        //TODO: we should commit error to root ioSegReq
        return 0;
    }

    if (ioS_leafR_comm->ReqOPCode == KERNEL_IO_WRITE) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN error get trueMD with write IO\n");
        return 0;
    }

    //TODO update leaf IOSReq
    update_metadata_UDataStat(MData, UData_S_INIT);

    plReq_func.add_PLReq_CallerReq = _cb_ioSleafR_add_plReq;
    plReq_func.rm_PLReq_CallerReq = _cb_ioSleafR_rm_plReq;
    plReq_func.ciUser_CallerReq = NULL;
    plReq_func.reportMDErr_CallerReq = _cb_ioSleafR_queryTrueMD;
    plReq_func.PLReq_endReq = _cb_ioSleafR_ReadPL_endfn;
    plReq_func.cpData_CallerReq = _cb_ioSleafR_read_cpData;

    PLMgr_read_payload(MData, &ioS_leafR->ioSleafR_comm,
            ioS_leafR->payload_offset,
            is_vol_send_rw_payload(ioReq->drive), is_vol_do_rw_IO(ioReq->drive),
            ioS_leafR, &numLB_inval,
            &ioReq->drive->dnSeltor, &plReq_func);

    if (numLB_inval > 0) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN error get trueMD with inval MD %d\n",
                numLB_inval);
    }

    return 0;
}

/**
 * ioSegLeafReq_submit_request - API for other sub-component submit an io seg leaf request
 *     for reading payload - get metadata and read payload
 * @is_mdcache_on: whether metadata cache capability on
 * @volReplica: number of volume default replica
 *
 */
void ioSegLeafReq_submit_request (ioSeg_leafReq_t *ioS_leafR, bool is_mdcache_on,
        int32_t volReplica)
{
    metadata_t *mdarr;

    mdarr = &ioS_leafR->ioSleafR_MData[atomic_read(&ioS_leafR->ioSleafR_MData_ver)];
    discoC_acquire_metadata(&ioS_leafR->ioSleafR_uioaddr,
            is_mdcache_on, &ioS_leafR->ioSleafR_comm, volReplica,
            false, true, true, mdarr, ioS_leafR, _cb_ioSleafR_acquireMD_endfn, NULL);
}
