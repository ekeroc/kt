/*
 * Copyright (C) 2010-2020 by Division F / ICL
 * Industrial Technology Research Institute
 *
 * discoC_DNUData_Request.c
 *
 * Define data structure and related API of datanode user data read write request
 */
#include <linux/version.h>
#include <linux/blkdev.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <net/sock.h>
#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/dms_kernel_version.h"
#include "../common/discoC_mem_manager.h"
#include "../common/thread_manager.h"
#include "../metadata_manager/metadata_manager.h"
#include "discoC_DNC_Manager_export.h"
#include "discoC_DNUData_Request.h"
#include "discoC_DNUDReq_fsm.h"
#include "discoC_DNUDReqPool.h"
#include "discoC_DNC_worker.h"
#include "discoC_DNC_Manager.h"
#include "discoC_DNC_workerfun.h"

/*
 * DNUDReq_set_feature: set feature bit of ReqFeature
 * @myReq  : target request to be set
 * @fea_bit: which feature be set as 1
 */
void DNUDReq_set_feature (DNUData_Req_t *myReq, uint32_t fea_bit)
{
    set_bit(fea_bit, (volatile void *)&myReq->dnReq_comm.ReqFeature);
}

/*
 * DNUDReq_clear_feature: clear feature bit of ReqFeature
 * @myReq  : target request to be set
 * @fea_bit: which feature be set as 0
 */
void DNUDReq_clear_feature (DNUData_Req_t *myReq, uint32_t fea_bit)
{
    clear_bit(fea_bit, (volatile void *)&myReq->dnReq_comm.ReqFeature);
}

/*
 * DNUDReq_check_feature: check whether feature on or off
 * @myReq  : target request to be set
 * @fea_bit: which feature be check
 *
 * Return: 0: feature off
 *         1: feature on
 */
bool DNUDReq_check_feature (DNUData_Req_t *myReq, uint32_t fea_bit)
{
    return test_bit(fea_bit,
            (const volatile void *)&myReq->dnReq_comm.ReqFeature);
}

/*
 * DNUDReq_test_set_fea: set feature and return old value in atomic operation
 * @myReq  : target request to be set
 * @fea_bit: which feature be check
 *
 * Return: 0: feature origin state is off
 *         1: feature origin state is on
 */
int32_t DNUDReq_test_set_fea (DNUData_Req_t *myReq, uint32_t fea_bit)
{
    return test_and_set_bit(fea_bit,
            (volatile void *)&myReq->dnReq_comm.ReqFeature);
}

/*
 * DNUDReq_test_clear_fea: clear feature and return old value in atomic operation
 * @myReq  : target request to be set
 * @fea_bit: which feature be check
 *
 * Return: 0: feature origin state is off
 *         1: feature origin state is on
 */
int32_t DNUDReq_test_clear_fea (DNUData_Req_t *myReq, uint32_t fea_bit)
{
    return test_and_clear_bit(fea_bit,
            (volatile void *)&myReq->dnReq_comm.ReqFeature);
}

/*
 * DNUDReq_ref_Req: gain reference count of request
 * @myReq  : target request to gain reference
 *
 */
void DNUDReq_ref_Req (DNUData_Req_t *myReq)
{
    atomic_inc(&myReq->dnReq_comm.ReqRefCnt);
}

/*
 * DNUDReq_ref_Req: release reference count of request
 * @myReq  : target request to release reference
 *
 */
void DNUDReq_deref_Req (DNUData_Req_t *myReq)
{
    atomic_dec(&myReq->dnReq_comm.ReqRefCnt);
}

/*
 * DNUDReq_alloc_Req: allocate a DNUData Request from memory pool
 *
 * Return: allocation result
 */
DNUData_Req_t *DNUDReq_alloc_Req (void)
{
    return discoC_malloc_pool(dnClient_mgr.DNUDReq_mpool_ID);
}

/*
 * _add_dnReq2pool: add a DNRequest to request pool
 * @myReq: request to be added to request pool
 */
static void _add_dnReq2pool (DNUData_Req_t *myReq)
{
    int32_t lockID;

    lockID = DNUDReqPool_acquire_plock(&dnClient_mgr.UData_ReqPool,
            myReq->dnReq_comm.ReqID);

    do {
        if (DNUDReq_test_set_fea(myReq, DNREQ_FEA_IN_REQPOOL)) {
            break;
        }

        DNUDReqPool_addReq_nolock(myReq, lockID, &dnClient_mgr.UData_ReqPool);
    } while (0);

    DNUDReqPool_rel_plock(&dnClient_mgr.UData_ReqPool, lockID);
}

/*
 * DNUDReq_init_Req: initialize the basic data member of a DNUData request
 * @myReq:     request to be initialized
 * @opcode:    which DN operation code that caller want to perform (DNUDReq_opcode_t)
 * @callerReq: which caller request want perform R/W DN
 * @md_item  : which metadata that DN Request get datanode information
 * @dnLoc_idx: which DN Location in md_item for DN Client to access
 */
void DNUDReq_init_Req (DNUData_Req_t *myReq, uint16_t opcode,
        ReqCommon_t *callerReq, metaD_item_t *md_item, uint16_t dnLoc_idx)
{
    ReqCommon_t *dnR_comm;
    metaD_DNLoc_t *dn_loc;

    memset(myReq, 0, sizeof(DNUData_Req_t));
    dnR_comm = &myReq->dnReq_comm;
    dnR_comm->ReqID = DNClientMgr_gen_DNUDReqID();
    dnR_comm->ReqPrior = callerReq->ReqPrior;
    dnR_comm->ReqOPCode = opcode;
    //dnR_comm->ReqFeature = 0;
    dnR_comm->CallerReqID = callerReq->ReqID;
    //dnR_comm->t_sendReq = 0;
    //dnR_comm->t_rcvReqAck = 0;
    atomic_set(&dnR_comm->ReqRefCnt, 0);
    //myReq->t_rcvReqAck_mem = 0;

    myReq->dnLoc_idx = dnLoc_idx;
    myReq->num_hbids = md_item->num_hbids;

    do {
#ifdef DISCO_ERC_SUPPORT
        if (MData_chk_ERCProt(md_item)) {
            dn_loc = &md_item->erc_dnLoc[dnLoc_idx];
            myReq->arr_hbids[0] = dn_loc->ERC_HBID;
            break;
        }
#endif

        memcpy(myReq->arr_hbids, md_item->arr_HBID,
                sizeof(uint64_t)*md_item->num_hbids);
        dn_loc = &md_item->udata_dnLoc[dnLoc_idx];
    } while (0);

    myReq->ipaddr = dn_loc->ipaddr;
    myReq->port = dn_loc->port;

    myReq->num_phyloc = dn_loc->num_phyLocs;
    memcpy(myReq->phyLoc_rlo, dn_loc->rbid_len_offset,
            sizeof(uint64_t)*dn_loc->num_phyLocs);

    memset(myReq->UDataSeg, 0, sizeof(struct iovec)*MAX_LBS_PER_RQ);
    myReq->num_UDataSeg = 0;

    myReq->mdata_item = NULL;

    INIT_LIST_HEAD(&myReq->entry_dnReqPool);
    INIT_LIST_HEAD(&myReq->entry_dnWkerQ);

    DNUDReq_init_fsm(&myReq->dnReq_stat);

    _add_dnReq2pool(myReq);
}

/*
 * init_DNUDReq_Read: initialize the data member of a DNUData request that related to read data
 * @myReq:       request to be initialized
 * @data_offset: data offset from that start of caller request
 * @endfn      : callback function when read DN done to call
 * @pdata      : private data for callback function
 * @send_data  : whether this request need send/receive data through socket
 *                (When send_data = 0, DN should not send payload back)
 *               for performance evaluation
 * @rw_data    : whether DN should R/W data from data when got this request
 *
 * Return: 0: initialize fail
 *        ~0: initialize failure
 */
int32_t init_DNUDReq_Read (DNUData_Req_t *myReq, uint64_t data_offset,
        DNUDReq_endfn_t *endfn, void *pdata, bool send_data, bool rw_data)
{
    int32_t ret;
    DNUDREQ_FSM_action_t action;

    ret = 0;

    myReq->dn_payload_offset = data_offset;
    myReq->dnReq_endfn = endfn;
    myReq->usr_pdata = pdata;
    if (send_data) {
        DNUDReq_set_feature(myReq, DNREQ_FEA_SEND_PAYLOAD);
    }

    if (rw_data) {
        DNUDReq_set_feature(myReq, DNREQ_FEA_DO_RW_IO);
    }
    myReq->ref_fp_map = NULL;
    myReq->ref_fp_cache = NULL;

    action = DNUDReq_update_state(&myReq->dnReq_stat, DNREQ_E_InitDone);
    if (action != DNREQ_A_DO_ADD_WORKQ) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN fail tr dnReq state DNREQ_E_InitDone action %s\n",
                DNUDReq_get_action_str(action));
        ret = -EPERM;
    }

    return ret;
}

/*
 * _get_data_buffer_loc: get memory address in original data buffer
 * @data_buf      : data buffer that hold user's data
 * @payload_offset: data offset from start address of data_buf
 * @len           : how many length caller function want to set
 * @ref_len       : how many length in data_buf specific chunk allow to be return
 *
 * Return: data pointer to return to caller
 *
 * NOTE: data_buf use array of non-continuous memory space to hold data
 *       caller: should pointer array to record data_buf
 */
static int8_t *_get_data_buffer_loc (data_mem_chunks_t *data_buf,
        uint64_t payload_offset, uint32_t len, uint32_t *ref_len)
{
    int8_t *ref_ptr;
    uint64_t chunk_start, off, next_chunk_start;
    uint32_t chunk_index;

    ref_ptr = NULL;
    *ref_len = 0;

    chunk_index = payload_offset >> BIT_LEN_MEM_CHUNK;
    chunk_start = chunk_index << BIT_LEN_MEM_CHUNK;

    off = payload_offset - chunk_start;
    ref_ptr = data_buf->chunks[chunk_index]->data_ptr + off;

    next_chunk_start = chunk_start + data_buf->chunk_item_size[chunk_index];

    *ref_len = next_chunk_start >= (payload_offset + len) ?
            len : (next_chunk_start - payload_offset);

    return ref_ptr;
}

/*
 * init_DNUDReq_Write: initialize the data member of a DNUData request that related to write data
 * @myReq      :       request to be initialized
 * @data_buf   : data memory chunk in caller request for holding user data
 * @payload_offset: data offset from that start of caller request
 * @endfn      : callback function when read DN done to call
 * @pdata      : private data for callback function
 * @fp_map     : caller's fingerprint map start memory address
 * @fp_cache   : caller's fingerprint cache start memory address
 * @send_data  : whether this request need send/receive data through socket
 *                (When send_data = 0, DN should not send payload back)
 *               for performance evaluation
 * @rw_data    : whether DN should R/W data from data when got this request
 *
 * Return: 0: initialize fail
 *        ~0: initialize failure
 */
int32_t init_DNUDReq_Write (DNUData_Req_t *myReq, data_mem_chunks_t *data_buf,
        uint64_t payload_offset, DNUDReq_endfn_t *endfn, void *pdata,
        uint8_t *fp_map, uint8_t *fp_cache, bool send_data, bool rw_data)
{
    int8_t *data_ptr;
    uint64_t data_off, dnlen;
    uint32_t len;
    int32_t cnt, ret;
    DNUDREQ_FSM_action_t action;

    ret = 0;
    myReq->dn_payload_offset = payload_offset;
    myReq->ref_fp_map = fp_map;
    myReq->ref_fp_cache = fp_cache;

    if (send_data) {
        DNUDReq_set_feature(myReq, DNREQ_FEA_SEND_PAYLOAD);
    }

    if (rw_data) {
        DNUDReq_set_feature(myReq, DNREQ_FEA_DO_RW_IO);
    }

    myReq->num_UDataSeg = 0;
    data_off = payload_offset;
    dnlen = myReq->num_hbids << BIT_LEN_DMS_LB_SIZE;
    cnt = 0;
    while (dnlen > 0) {
        data_ptr = _get_data_buffer_loc(data_buf, data_off, dnlen, &len);
        if (IS_ERR_OR_NULL(data_ptr)) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN fail get payload ptr\n");
            DMS_WARN_ON(true);
            break;
        }

        myReq->UDataSeg[cnt].iov_base = data_ptr;
        myReq->UDataSeg[cnt].iov_len = len;
        cnt++;
        data_off += len;
        dnlen -= len;
    }
    myReq->num_UDataSeg = cnt;

    myReq->dnReq_endfn = endfn;
    myReq->usr_pdata = pdata;

    action = DNUDReq_update_state(&myReq->dnReq_stat, DNREQ_E_InitDone);
    if (action != DNREQ_A_DO_ADD_WORKQ) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN fail tr dnReq state DNREQ_E_InitDone action %s\n",
                DNUDReq_get_action_str(action));
        ret = -EPERM;
    }

    return ret;
}

/*
 * DNUDReq_free_Req: return DNRequest memory space to memory pool
 * @myReq      : request to be free
 *
 * NOTE: will check reference count. only free when reference count == 0
 */
void DNUDReq_free_Req (DNUData_Req_t *dn_req)
{
#define MAX_BUSY_WAIT_CNT   1000
#define SLEEP_WAIT_PERIOD   100 //milli-second
    ReqCommon_t *dnR_comm;
    int32_t cnt;

    if (unlikely(IS_ERR_OR_NULL(dn_req))) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN free null dn req\n");
        DMS_WARN_ON(true);
        return;
    }

    dnR_comm = &dn_req->dnReq_comm;
    cnt = 0;
    while (true) {
        if (atomic_read(&dnR_comm->ReqRefCnt) == 0) {
            break;
        }

        cnt++;
        if (cnt <= MAX_BUSY_WAIT_CNT) {
            //sched_yield();
            yield();
        } else {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN free DNUDReq %u wait refcnt -> 0 (%u %u)\n",
                    dnR_comm->ReqID, atomic_read(&dnR_comm->ReqRefCnt), cnt);
            msleep(SLEEP_WAIT_PERIOD);
        }
    }

    discoC_free_pool(dnClient_mgr.DNUDReq_mpool_ID, dn_req);
}

/*
 * DNUDReq_check_retryable: check whether a request can be retry
 * @myReq      : request to be checked
 *
 * Return: false: cannot be retry
 *         true:  can be retry
 */
bool DNUDReq_check_retryable (DNUData_Req_t *myReq)
{
    DNUDREQ_FSM_action_t action;
    bool ret;

    ret = false;

    action = DNUDReq_update_state(&myReq->dnReq_stat, DNREQ_E_ConnBack);
    if (DNREQ_A_DO_RETRY_REQ == action) {
        ret = true;
    }

    return ret;
}

/*
 * _find_dnReq_update_state: update state when DNClient receive a ack and try to
 *                           update state after got request from request pool
 * @myReq      : request to be update
 * @dnAck      : which datanode ack return from datanode
 *
 * Return: action of update state
 *
 * TODO: change return type to  DNUDREQ_FSM_action_t
 */
int32_t _find_dnReq_update_state (DNUData_Req_t *myReq, uint16_t dnAck)
{
    int32_t ret;
    DNUDREQ_FSM_TREvent_t event;
    DNUDREQ_FSM_action_t action;

    ret = 0;

    if (dnAck == DNUDRESP_MEM_ACK || dnAck == DNUDRESP_WRITEANDGETOLD_MEM_ACK) {
        event = DNREQ_E_FindReqOK_MemAck;
    } else {
        event = DNREQ_E_FindReqOK;
    }

    action = DNUDReq_update_state(&myReq->dnReq_stat, event);
    if (action != DNREQ_A_DO_PROCESS_RESP) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN fail tr dnReq state find request action %s\n",
                DNUDReq_get_action_str(action));
        ret = -EPERM;
    }

    return ret;
}

/*
 * find_DNUDReq_ReqPool: get DNRequest from pool base on input
 * @dnReqID    : DN Request ID to be search
 * @dnAck      : which datanode ack return from datanode
 *
 * Return: A DNRequest instance. (NULL when not found or state transmit not allowed)
 *
 */
DNUData_Req_t *find_DNUDReq_ReqPool (uint32_t dnReqID, uint16_t dnAck)
{
    DNUData_Req_t *myReq;
    int32_t lockID;


    lockID = DNUDReqPool_acquire_plock(&dnClient_mgr.UData_ReqPool, dnReqID);

    do {
        myReq = DNUDReqPool_findReq_nolock(dnReqID, lockID,
                &dnClient_mgr.UData_ReqPool);
        if (IS_ERR_OR_NULL(myReq)) {
            break;
        }

        DNUDReq_ref_Req(myReq);
        if (_find_dnReq_update_state(myReq, dnAck)) {
            DNUDReq_deref_Req(myReq);
            myReq = NULL;
            break;
        }

        if (dnAck != DNUDRESP_MEM_ACK &&
                dnAck != DNUDRESP_WRITEANDGETOLD_MEM_ACK) {
            if (DNUDReq_test_clear_fea(myReq, DNREQ_FEA_IN_REQPOOL) == false) {
                //TODO show message
                DNUDReq_deref_Req(myReq);
                myReq = NULL;
                break;
            }

            DNUDReqPool_rmReq_nolock(myReq, &dnClient_mgr.UData_ReqPool);
        }
    } while (0);

    DNUDReqPool_rel_plock(&dnClient_mgr.UData_ReqPool, lockID);

    return myReq;
}

#if 0
static void rm_DNUDReq_ReqPool (DNUData_Req_t *myReq)
{
    int32_t lockID;
    lockID = DNUDReqPool_acquire_plock(&dnClient_mgr.UData_ReqPool, myReq->dnreq_id);

    if (DNUDReq_test_clear_fea(myReq, DNREQ_FEA_IN_REQPOOL)) {
        DNUDReqPool_rmReq_nolock(myReq, &dnClient_mgr.UData_ReqPool);
    }

    DNUDReqPool_rel_plock(&dnClient_mgr.UData_ReqPool, lockID);
}
#endif

/*
 * DNUDReq_cancel_Req: Cancel a DNRequest by given DNRequest ID and callerID
 * @dnReqID    : DN Request ID to be found and cancel request
 * @callerID   : Caller Request ID that trigger request to found and cancel request
 *
 */
void DNUDReq_cancel_Req (uint32_t dnReqID, uint32_t callerID)
{
    DNUData_Req_t *myReq;
    int32_t lockID;
    DNUDREQ_FSM_action_t action;

    action = DNREQ_A_NoAction;

CANCEL_DNREQ_AGAIN:
    lockID = DNUDReqPool_acquire_plock(&dnClient_mgr.UData_ReqPool, dnReqID);
    myReq = DNUDReqPool_findReq_nolock(dnReqID, lockID, &dnClient_mgr.UData_ReqPool);

    do {
        if (IS_ERR_OR_NULL(myReq)) {
            break;
        }

        if (unlikely(myReq->dnReq_comm.CallerReqID != callerID)) {
            //ERROR handling
            dms_printk(LOG_LVL_WARN, "DMSC WARN cancel DNReq %u caller mismatch %u %u\n",
                    myReq->dnReq_comm.ReqID, callerID, myReq->dnReq_comm.CallerReqID);
            break;
        }

        action = DNUDReq_update_state(&myReq->dnReq_stat, DNREQ_E_Cancel);
        if (DNREQ_A_DO_CANCEL_REQ == action) {
            DNUDReq_ref_Req(myReq);
            DNUDReqPool_rmReq_nolock(myReq, &dnClient_mgr.UData_ReqPool);
        }
    } while (0);

    DNUDReqPool_rel_plock(&dnClient_mgr.UData_ReqPool, lockID);

    if (action == DNREQ_A_DO_RETRY_CANCEL_REQ) {
        goto CANCEL_DNREQ_AGAIN;
    }

    if (DNREQ_A_DO_CANCEL_REQ == action) {
        DNUDReq_deref_Req(myReq);
        DNCWker_rm_Req_workQ(myReq, &dnClient_mgr.dnClient);
        DNUDReq_free_Req(myReq);
    }
}

/*
 * DNUDReq_retry_Req: Retry a DNRequest
 * @myReq    : DN Request to be retry
 *
 */
void DNUDReq_retry_Req (DNUData_Req_t *myReq)
{
    DNUDREQ_FSM_action_t action;

    action = DNUDReq_update_state(&myReq->dnReq_stat, DNREQ_E_RetryChkDone);
    if (DNREQ_A_DO_ADD_WORKQ != action) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN fail tr RetryChkDone when retry Req %u\n",
                myReq->dnReq_comm.ReqID);
        //ret = -EPERM;
        return;
    }

    //TODO: maybe the better way is ci to payload request, so that DNClient won't touch metadata
    MData_wRetry_reset_DNLoc(myReq->mdata_item, myReq->dnLoc_idx);
    DNCWker_add_Req_workQ(myReq, &dnClient_mgr.dnClient);
}

/*
 * DNUDReq_get_dnInfo: get ipaddr/port information of DN Request
 * @dnReqID    : which ID of DNRequest for search from Request Pool
 * @callerID   : which caller ID of DNRequest for search from Request Pool
 * @ipv4addr   : ip address of found DNReq to be returned to caller
 * @poer       : port of found DNReq to be returned to caller
 *
 * Return: 0: found DNReq
 *        ~0: DN Request not found
 *
 */
int32_t DNUDReq_get_dnInfo (uint32_t dnReqID, uint32_t callerID,
        uint32_t *ipv4addr, uint32_t *port)
{
    DNUData_Req_t *myReq;
    int32_t ret, lockID;

    ret = 0;

    lockID = DNUDReqPool_acquire_plock(&dnClient_mgr.UData_ReqPool, dnReqID);
    myReq = DNUDReqPool_findReq_nolock(dnReqID, lockID, &dnClient_mgr.UData_ReqPool);

    do {
        if (IS_ERR_OR_NULL(myReq)) {
            ret = -ENOENT;
            break;
        }

        if (unlikely(myReq->dnReq_comm.CallerReqID != callerID)) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN get dnInfo found req but callerID err\n");
            ret = -ENOENT;
            break;
        }

        *ipv4addr = myReq->ipaddr;
        *port = myReq->port;
    } while (0);

    DNUDReqPool_rel_plock(&dnClient_mgr.UData_ReqPool, lockID);

    return ret;
}

