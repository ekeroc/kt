/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * payload_manager.c
 *
 * This component aim for managing payload
 *
 */
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/types.h>
#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/dms_kernel_version.h"
#include "../common/discoC_mem_manager.h"
#include "../connection_manager/conn_manager_export.h"
#include "../metadata_manager/metadata_manager.h"
#include "../discoDN_client/discoC_DNC_Manager_export.h"
#include "../common/thread_manager.h"
#include "payload_manager_export.h"
#include "payload_request_fsm.h"
#include "payload_request.h"
#include "payload_ReqPool.h"
#include "payload_manager.h"
#include "payload_manager_private.h"
#include "payload_Req_IODone.h"
#include "Read_Replica_Selector.h"
#include "../config/dmsc_config.h"

/*
 * PLMgr_gen_PLReqID: generate next payload request ID
 *
 * Return: a 4 bytes integer for payload request ID
 */
uint32_t PLMgr_gen_PLReqID (void)
{
    uint32_t pReqID;

    pReqID = (uint32_t)atomic_add_return(1, &g_payload_mgr.udata_reqID_generator);

    if (INVAL_REQID == pReqID) {
        pReqID = (uint32_t)atomic_add_return(1, &g_payload_mgr.udata_reqID_generator);
    }

    return pReqID;
}

#ifdef DISCO_ERC_SUPPORT
static void _PLReq_free_ERCDNReq (payload_req_t *pReq)
{
    spinlock_t *group_lock;
    uint32_t myReqID;
    int32_t i;

    group_lock = &pReq->DNUDReq_glock;

    spin_lock(group_lock);

    for (i = 0; i < pReq->num_ERCRebuild_DNUDReq; i++) {
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
 * _PLReq_free_allDNReq:
 *     for each DNUData Request in payload request, call DNClient module
 *     DNClientMgr_cancel_DNUDReq to cancel all DNUData Request
 * @pReq: which payload request need cancel all DNUData Request
 */
static void _PLReq_free_allDNReq (payload_req_t *pReq) {
    spinlock_t *group_lock;
    uint32_t myReqID;
    int32_t i;

    if (unlikely(IS_ERR_OR_NULL(pReq))) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN null param when rm all dn reqs\n");
        DMS_WARN_ON(true);
        return;
    }

#ifdef DISCO_ERC_SUPPORT
    _PLReq_free_ERCDNReq(pReq);
#endif

    group_lock = &pReq->DNUDReq_glock;

    spin_lock(group_lock);

    if (atomic_read(&pReq->num_DNUDReq) == 0) {
        spin_unlock(group_lock);
        return;
    }

    for (i = 0; i < MAX_NUM_REPLICA; i++) {
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

/*
 * PLMgr_free_PLReq:
 *     Free payload request by step 1: cancel all DNUData Request
 *                                  2: return payload request memory to memory pool
 * @pReq: which payload request need free
 */
void PLMgr_free_PLReq (payload_req_t *preq)
{
    _PLReq_free_allDNReq(preq);
    PLReq_free_Req(preq, g_payload_mgr.preq_mpool_ID);
}

/*
 * get_next_DN_index:
 *     get an available dn_location_index from start by checking whether
 *     the dn_result is default value (true) or not.
 *     If not init value, it means payload manager not yet send a DNUData Request
 *     for this replica. Payload manager can send a DNUData Request for this replica
 *     to read user data.
 * @dn_lr: a metadata item that contains all DN information
 * @start_index: which start index to be checked.
 *
 * Return: DN index in metadata item that indicate a DN that can be read data
 *       >= 0: validate DN index
 *       < 0 : already try to read data from all DN
 */
static int32_t get_next_DN_index (metaD_item_t *dn_lr, int32_t start_index)
{
    int32_t i;

    if (unlikely(dms_client_config->clientnode_running_mode ==
            discoC_MODE_SelfTest)) {
        return 0;
    }

    /*
     * find one dn_location.result == true from start_index to
     * MAX_NUM_REPLICA
     */
    for (i = start_index; i < dn_lr->num_dnLoc; i++) {
        if (UData_S_INIT == dn_lr->udata_dnLoc[i].udata_status) {
            return i;
        }
    }

    //find one dn_location.result == true from 0 to start_index
    for (i = 0; i < start_index; i++) {
        if (UData_S_INIT == dn_lr->udata_dnLoc[i].udata_status) {
            return i;
        }
    }

    return -ENXIO;
}

/*
 * _gen_submit_DNReadReq:
 *     Peek a dn index in metadata and ask DNClient to read data from specific DN
 * @md_item: a metadata item that contains all DN information
 * @dnLoc_idx: which datanode index in metadata for DNClient to read data
 * @payload_offset: offset from IO's payload start addressing. will need this when
 *                  DNClient try to copy data to caller
 * @pReq: which payload request as caller request for generated DNUData request
 * @bk_idx: which bucket to holding DNRequest's ID
 *
 * Return: result to indicate whether gen / submit DN Request
 *         0: success
 *        ~0: fail
 */
static int32_t _gen_submit_DNReadReq (metaD_item_t *md_item,
        int32_t dnLoc_idx, uint64_t payload_offset,
        payload_req_t *pReq, int32_t bk_idx)
{
    int32_t ret;

    do {
#ifdef DISCO_ERC_SUPPORT
        if (MData_chk_ERCProt(md_item)) {
            break;
        }
#endif

        dnLoc_idx = get_next_DN_index(md_item, dnLoc_idx);
        if (dnLoc_idx >= md_item->num_dnLoc || dnLoc_idx < 0) {
            dms_printk(LOG_LVL_INFO,
                    "DMSC INFO no more replica to read %d\n", dnLoc_idx);
            //there is no more available locations
            return -EINVAL;
        }
    } while (0);

    if (unlikely(bk_idx >= md_item->num_dnLoc || bk_idx < 0)) {
        dms_printk(LOG_LVL_INFO,
                "DMSC INFO payload req %u DNReq bucket %d >= num DN %d\n",
                pReq->plReq_comm.ReqID, dnLoc_idx, md_item->num_dnLoc);
        //there is no more available locations
        return -EINVAL;
    }

    pReq->last_readReplica_idx = dnLoc_idx;
    pReq->last_readDNbk_idx = bk_idx;

    ret = submit_DNUData_Read_Request(md_item, dnLoc_idx, payload_offset,
            &(pReq->plReq_comm), PLReq_readDN_endfn,
            pReq, bk_idx, &pReq->arr_DNUDReq[bk_idx],
            PLReqFea_send_payload(pReq), PLReqFea_rw_payload(pReq));

    return ret;
}

/*
 * PLMgr_submit_DNUDReadReq:
 *     allocate a bucket for holding read DNRequest ID
 *     and then call _gen_submit_DNReadReq to submit DNUData Request
 * @md_item: a metadata item that contains all DN information
 * @pReq: which payload request as caller request for generated DNUData request
 * @replica_idx: which DN index in metadata for
 *
 * Return: result to indicate whether gen / submit DN Request
 *         0: success
 *        ~0: fail
 */
int32_t PLMgr_submit_DNUDReadReq (metaD_item_t *mdItem, payload_req_t *pReq,
        int32_t replica_idx)
{
    int32_t ret, bk_idx;

    ret = 0;

    bk_idx = PLReq_allocBK_DNUDReq(pReq);
    ret = _gen_submit_DNReadReq(mdItem, replica_idx, pReq->payload_offset, pReq, bk_idx);

    return ret;
}

#ifdef DISCO_ERC_SUPPORT
int32_t PLMgr_submit_ERCRebuild_DNUDReq (metaD_item_t *mdItem, payload_req_t *pReq)
{
    int32_t ret, i;

    ret = 0;

    pReq->num_ERCRebuild_DNUDReq = mdItem->num_dnLoc - 1;
    pReq->arr_ERCDNUDReq = discoC_mem_alloc(sizeof(uint32_t)*(mdItem->num_dnLoc - 1), GFP_NOWAIT);
    if (IS_ERR_OR_NULL(pReq->arr_ERCDNUDReq)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN fail alloc mem ERC Rebuild\n");
        return -ENOMEM;
    }

    for (i = 0; i < mdItem->num_dnLoc; i++) {
        if (mdItem->arr_HBID[0] == mdItem->erc_dnLoc[i].ERC_HBID) {
            continue;
        }

        ret = submit_DNUData_Read_Request(mdItem, i, pReq->payload_offset,
                &(pReq->plReq_comm), PLReq_readDN_endfn,
                pReq, i, &pReq->arr_ERCDNUDReq[i],
                PLReqFea_send_payload(pReq), PLReqFea_rw_payload(pReq));
    }

    return ret;
}

static int32_t PLMgr_ReadERC_SelectDN (metaD_item_t *loc)
{
    int32_t i, retIdx;

    retIdx = -1;

    for (i = 0; i < loc->num_dnLoc; i++) {
        if (loc->arr_HBID[0] == loc->erc_dnLoc[i].ERC_HBID) {
            retIdx = i;
            break;
        }
    }

    return retIdx;
}
#endif

/*
 * _submit_read_PLReq:
 *     generate a read payload request base on given metadata item and
 *     call PLMgr_submit_DNUDReadReq to create DNUData Read request and
 *     submit DNClient module.
 * @md_item: a metadata item that contains all DN information for read
 * @payload_off: offset from IO's payload start addressing. will need this when
 *               DNClient try to copy data to caller
 * @callerReq: which caller request want to use payload request read payload
 * @send_data: whether this read request need datanode send payload back
 * @rw_data  : whether this read request need datanode read data from disk
 * @usr_data : callback data for PLReq_endReq function
 * @dnSeltor : A selector for choose a better datanode to read
 * @pReq_func: table of callback function for execution
 *
 * Return: Request ID of payload request
 *
 * TODO: should not use dms_client_config->ioreq_timeout in PLReq_init_Req for
 *       timeout value. Need change to use
 *       total time of a io request !V time spent in acquiring metadata.
 */
static uint32_t _submit_read_PLReq (metaD_item_t *mdItem,
        uint64_t payload_off, ReqCommon_t *callerReq,
        bool send_data, bool rw_data, void *usr_data,
        struct Read_DN_selector *dnSeltor, PLReq_func_t *pReq_func)
{
    payload_req_t *pReq;
    int32_t replica_idx;
    PLREQ_FSM_action_t action;

    pReq = PLReq_alloc_Req(g_payload_mgr.preq_mpool_ID);
    if (IS_ERR_OR_NULL(pReq)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN fail alloc payload req\n");
        return INVAL_REQID;
    }

    PLReq_init_Req(pReq, mdItem, KERNEL_IO_READ, payload_off, callerReq,
            NULL, NULL, send_data, rw_data,
            dms_client_config->ioreq_timeout, usr_data, pReq_func);
    PLReq_ref_Req(pReq);

    PLReq_add_ReqPool(pReq);
    if (IS_ERR_OR_NULL(pReq_func->add_PLReq_CallerReq) == false) {
        pReq_func->add_PLReq_CallerReq(usr_data, &pReq->entry_callerReqPool);
    }

    action = PLReq_update_state(&pReq->plReq_stat, PLREQ_E_InitDone);

    if (PLREQ_A_RWData == action) {
        PLReq_schedule_timer(pReq);

        do {
#ifdef DISCO_ERC_SUPPORT
            if (MData_chk_ERCProt(mdItem)) {
                replica_idx = PLMgr_ReadERC_SelectDN(mdItem);
                break;
            }
#endif
            replica_idx = PLMgr_Read_selectDN(mdItem, dnSeltor,
                    dms_client_config->read_hbs_next_dn,
                    dms_client_config->enable_read_balance);
        } while (0);

        if (PLMgr_submit_DNUDReadReq(mdItem, pReq, replica_idx)) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN fail create read DNReq\n");
        }
    } else {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN tr read payload InitDone wrong action %s\n",
                PLReq_get_action_str(action));
    }

    PLReq_deref_Req(pReq);
    return pReq->plReq_comm.ReqID;
}

/*
 * PLMgr_read_payload:
 *              export API for caller request read payload service
 *              For each metadata item in metadata_t, generate a payload request to
 *              read data from specific DNs
 * @my_metaD: A metadata array contains several metadata item which represents metadata
 *            of a continuous range of LBID (start, length)
 * @callerReq: which caller request want to use payload request read payload
 * @payload_offset: offset from IO's payload start addressing. will need this when
 *               DNClient try to copy data to caller
 * @send_data: whether this read request need datanode send payload back
 * @rw_data  : whether this read request need datanode read data from disk
 * @usr_data : callback data for PLReq_endReq function
 * @numLB_inval: for preventing caller form a for loop to get number of LBID doesn't
 *               has validate metadata, PLMgr_read_payload will calculate number LBID
 *               and return to caller with numLB_inval
 * @dnSeltor : A selector for choose a better datanode to read
 * @pReq_func: table of callback function for execution
 *
 * return: submit payload read request success or not
 *         0: success
 *        ~0: fail
 */
int32_t PLMgr_read_payload (metadata_t *my_metaD, ReqCommon_t *callerReq,
        uint64_t payload_offset, bool send_data, bool rw_data, void *usr_data,
        int32_t *numLB_inval, struct Read_DN_selector *dnSeltor,
        PLReq_func_t *pReq_func)
{
    metaD_item_t *md_item;
    uint64_t plOff, LB_s;
    int32_t i, ret, num_locs;

    ret = 0;

    num_locs = my_metaD->num_MD_items;
    plOff = payload_offset;

    LB_s = my_metaD->usr_ioaddr.lbid_start;
    *numLB_inval = 0;
    for (i = 0; i < num_locs; i++) {
        md_item = my_metaD->array_metadata[i];
        dms_printk(LOG_LVL_DEBUG,
                "DMSC DEBUG locs[%d]: lbid = %llu, payload_offset = %llu\n",
                i, md_item->lbid_s, plOff);

        if (md_item->mdata_stat == MData_VALID ||
                md_item->mdata_stat == MData_VALID_LESS_REPLICA) {
            _submit_read_PLReq(md_item, plOff, callerReq,
                    send_data, rw_data, usr_data, dnSeltor, pReq_func);
        } else {
            *numLB_inval += md_item->num_hbids;
        }

        plOff += (md_item->num_hbids << BIT_LEN_DMS_LB_SIZE);
        LB_s += md_item->num_hbids;
    }

    return 0;
}

/*
 * _gen_submit_DNWriteReq: for each DN replica in a metadata item, generate corresponding
 *                         DN for writing data
 * @mdItem: metadata item that contains all DN information for writing data
 * @wdata: source user data buffer
 * @pReq: payload request that want to call DNClient to write data to all replica in metadata item
 *
 * return: generate all DN success or fail
 *         0: success
 *        ~0: fail
 */
static int32_t _gen_submit_DNWriteReq (metaD_item_t *mdItem,
        data_mem_chunks_t *wdata, payload_req_t *pReq)
{
    int32_t ret, i, bk_idx;

    ret = 0;
    for (i = 0; i < mdItem->num_dnLoc; i++) {
        bk_idx = PLReq_allocBK_DNUDReq(pReq);

        if (unlikely(bk_idx > i)) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN arry DNReq %d > num dnloc %d\n",
                    bk_idx, i);
            ret = -EFAULT;
            break;
        }

        pReq->last_readReplica_idx = i;
        pReq->last_readDNbk_idx = bk_idx;
        ret = submit_DNUData_Write_Request(mdItem, i, wdata, pReq->payload_offset,
                &pReq->plReq_comm, PLReq_writeDN_endfn, pReq, bk_idx,
                &(pReq->arr_DNUDReq[bk_idx]), pReq->ref_fp_map, pReq->ref_fp_cache,
                PLReqFea_send_payload(pReq), PLReqFea_rw_payload(pReq));
    }

    return ret;
}

/*
 * _submit_write_PLReq: create and init a write payload request.
 *                      call _gen_submit_DNWriteReq to generate all related DNUData Request
 * @mdItem: metadata item that contains all DN information for writing data
 * @wdata: source user data buffer
 * @payload_off: offset from IO's payload start addressing. will need this when
 *               DNUData Request internal pointer array point to wdata
 * @callerReq: which caller request want to use payload request read payload
 * @fp_map   : a fingerprint map to indicate whether LB has fingerprint
 * @fp_cache : a fingerprint cache to store LB's fingerprint
 * @send_data: whether this read request need datanode send payload back
 * @rw_data  : whether this read request need datanode read data from disk
 * @usr_data : callback data for PLReq_endReq function
 * @pReq_func: table of callback function for execution
 *
 * Return: Request ID of payload request
 * TODO: should not use dms_client_config->ioreq_timeout in PLReq_init_Req for
 *       timeout value. Need change to use
 *       total time of a io request - time spent in acquiring metadata.
 */
static uint32_t _submit_write_PLReq (metaD_item_t *mdItem,
        data_mem_chunks_t *wdata, uint64_t payload_off,
        ReqCommon_t *callerReq, uint8_t *fp_map, uint8_t *fp_cache,
        bool send_data, bool rw_data,
        void *usr_data, PLReq_func_t *pReq_func)
{
    payload_req_t *pReq;
    PLREQ_FSM_action_t action;

    pReq = PLReq_alloc_Req(g_payload_mgr.preq_mpool_ID);
    if (IS_ERR_OR_NULL(pReq)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN fail alloc payload req\n");
        return INVAL_REQID;
    }
    PLReq_init_Req(pReq, mdItem, KERNEL_IO_WRITE, payload_off, callerReq,
            fp_map, fp_cache, send_data, rw_data,
            dms_client_config->ioreq_timeout, usr_data, pReq_func);
    PLReq_ref_Req(pReq);

    PLReq_add_ReqPool(pReq);
    if (IS_ERR_OR_NULL(pReq_func->add_PLReq_CallerReq) == false) {
        pReq_func->add_PLReq_CallerReq(usr_data, &pReq->entry_callerReqPool);
    }

    action = PLReq_update_state(&pReq->plReq_stat, PLREQ_E_InitDone);

    if (PLREQ_A_RWData == action) {
        PLReq_schedule_timer(pReq);
        if (_gen_submit_DNWriteReq(mdItem, wdata, pReq)) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN fail create read DNReq\n");
        }
    } else {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN tr write payload InitDone wrong action %s\n",
                PLReq_get_action_str(action));
    }

    PLReq_deref_Req(pReq);
    return pReq->plReq_comm.ReqID;
}

/*
 * PLMgr_write_payload: export API for caller to submit write payload request.
 *     for each metadata item in metadata array, call _submit_write_PLReq to
 *     generate corresponding payload write request to write data to each replica
 * @my_metaD: A metadata array contains several metadata item which represents metadata
 *            of a continuous range of LBID (start, length)
 * @callerReq: which caller request want to use payload request read payload
 * @wdata: source user data buffer
 * @payload_off: offset from IO's payload start addressing. will need this when
 *               DNUData Request internal pointer array point to wdata
 * @fp_map   : a fingerprint map to indicate whether LB has fingerprint
 * @fp_cache : a fingerprint cache to store LB's fingerprint
 * @send_data: whether this read request need datanode send payload back
 * @rw_data  : whether this read request need datanode read data from disk
 * @usr_data : callback data for PLReq_endReq function
 * @pReq_func: table of callback function for execution
 *
 * Return: generate success or fail
 *         0: sucess
 *        ~0: fail
 */
int32_t PLMgr_write_payload (metadata_t *my_metaD, ReqCommon_t *callerReq,
        data_mem_chunks_t *wdata, uint64_t payload_offset,
        uint8_t *fp_map, uint8_t *fp_cache, bool send_data, bool rw_data,
        void *usr_data, PLReq_func_t *pReq_func)
{
    metaD_item_t *md_item;
    uint64_t plOffset;
    int32_t ret, i;

    ret = 0;

    dms_printk(LOG_LVL_DEBUG, "DMSC DEBUG gen all write dn num of lr %d\n",
            my_metaD->num_MD_items);

    plOffset = payload_offset;

    for (i = 0; i < my_metaD->num_MD_items; i++) {
        md_item = my_metaD->array_metadata[i];

        _submit_write_PLReq(md_item, wdata, plOffset,
                callerReq, fp_map, fp_cache, send_data, rw_data,
                usr_data, pReq_func);
        plOffset += (md_item->num_hbids << BIT_LEN_DMS_LB_SIZE);
    }

    return ret;
}

/*
 * PLMgr_init_manager: initialize payload manager (global data: g_payload_mgr)
 *
 * Return: init success or fail
 *         0: sucess
 *        ~0: fail
 */
int32_t PLMgr_init_manager (void)
{
    int32_t ret, num_req_size;

    ret = 0;

    PLReqPool_init_pool(&g_payload_mgr.PLReqPool);
    atomic_set(&g_payload_mgr.udata_reqID_generator, 0);

    num_req_size = dms_client_config->max_nr_io_reqs*MAX_LBS_PER_RQ;
    g_payload_mgr.preq_mpool_ID =
            register_mem_pool("payload_request", num_req_size,
                    sizeof(payload_req_t), true);
    if (g_payload_mgr.preq_mpool_ID < 0) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN fail register payload request mem pool\n");
        return -ENOMEM;
    }

    g_payload_mgr.PLReq_retry_workq = create_workqueue(PLREQ_RETRY_WQ_NM);
    if (IS_ERR_OR_NULL(g_payload_mgr.PLReq_retry_workq)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN fail create Payload Request retry workq\n");
        DMS_WARN_ON(true);
    }

    ret = DNClientMgr_init_manager();
    return ret;
}

/*
 * PLMgr_rel_manager: release payload manager (global data: g_payload_mgr)
 *
 * Return: release success or fail
 *         0: sucess
 *        ~0: fail
 */
int32_t PLMgr_rel_manager (void)
{
    int32_t ret;

    ret = 0;

    ret = DNClientMgr_rel_manager();

    if (IS_ERR_OR_NULL(g_payload_mgr.PLReq_retry_workq) == false) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN rel PLMgr linger PLReq in workqueue\n");
        flush_workqueue(g_payload_mgr.PLReq_retry_workq);
        destroy_workqueue(g_payload_mgr.PLReq_retry_workq);
    }

    PLReqPool_rel_pool(&g_payload_mgr.PLReqPool);

    ret = deregister_mem_pool(g_payload_mgr.preq_mpool_ID);
    return ret;
}

