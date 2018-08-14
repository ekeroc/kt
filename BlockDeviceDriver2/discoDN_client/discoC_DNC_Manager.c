/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_DNC_Manager.c
 *
 */
#include <linux/version.h>
#include <linux/delay.h>
#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/dms_kernel_version.h"
#include "../common/thread_manager.h"
#include "../common/discoC_mem_manager.h"
#include "../connection_manager/conn_manager_export.h"
#include "../metadata_manager/metadata_manager.h"
#include "discoC_DNC_Manager_export.h"
#include "discoC_DNUData_Request.h"
#include "discoC_DNUDReqPool.h"
#include "discoC_DNC_worker.h"

#include "discoC_DNC_Manager.h"
#include "discoC_DNC_Manager_private.h"
#include "discoC_DN_protocol.h"
#include "discoC_DNC_workerfun.h"
#include "discoC_DNAck_worker.h"
#include "DNAck_ErrorInjection.h"
#include "../SelfTest.h"
#include "../config/dmsc_config.h"
#include "../flowcontrol/FlowControl.h"

#if (DN_PROTOCOL_VERSION == 2) && (CN_CRASH_RECOVERY == 1)
void update_cleanLog_ts (void)
{
    set_bit(0, (volatile void *)&ts_chk_bit);
    ts_cleanLog = jiffies_64;
}
#endif

/*
 * DNClientMgr_get_ReadMemPoolID: get read payload buffer memory pool ID
 *                                for allocating memory to hold read payload from socket buffer
 * Return: memory pool ID
 */
int32_t DNClientMgr_get_ReadMemPoolID (void)
{
    return dnClient_mgr.DNUDReadBuff_mpool_ID;
}

/*
 * get_num_DNUDataReq_waitSend: get number of DN User Data R/W Request that
 *                              stay in worker queue wait for sending
 * Return: number of requests
 */
int32_t get_num_DNUDataReq_waitSend (void)
{
    return DNCWker_get_numReq_workQ(&dnClient_mgr.dnClient);
}

/*
 * get_num_DNUDataReq_waitResp: get number of DN User Data R/W Request that
 *                              wait DN send ack back
 * Return: number of requests
 */
int32_t get_num_DNUDataReq_waitResp (void)
{
    return atomic_read(&dnClient_mgr.num_DNReq_waitResp);
}

/*
 * get_last_DNUDataRespID: get max request ID that return from DN
 * Return: max rcv DN request from DN
 */
uint32_t get_last_DNUDataRespID (void)
{
    return dnClient_mgr.lastMax_DNUDRespID;
}

/*
 * DNClientMgr_set_last_DNUDataRespID: set max rcv DN request ID from DN to dn client manager
 */
void DNClientMgr_set_last_DNUDataRespID (uint32_t dnReqID)
{
    if (dnClient_mgr.lastMax_DNUDRespID < dnReqID) {
        dnClient_mgr.lastMax_DNUDRespID = dnReqID;
    } else if (dnClient_mgr.lastMax_DNUDRespID == 0xffff && dnReqID == 1) {
        dnClient_mgr.lastMax_DNUDRespID = dnReqID;
    }
}

/*
 * DNClientMgr_gen_DNUDReqID: generate Request ID for DN Request
 *
 * Return: request ID
 */
uint32_t DNClientMgr_gen_DNUDReqID (void)
{
    uint32_t myReqID;

    myReqID = (uint32_t)atomic_add_return(1, &dnClient_mgr.DNUDReqID_counter);
    if (INVAL_REQID == myReqID) {
        myReqID = (uint32_t)atomic_add_return(1, &dnClient_mgr.DNUDReqID_counter);
    }

    return myReqID;
}

/*
 * get_last_DNUDataReqID: get last generated DN request ID
 *
 * Return: last DN request ID
 */
uint32_t get_last_DNUDataReqID (void)
{
    return (uint32_t)atomic_read(&dnClient_mgr.DNUDReqID_counter);
}

/*
 * submit_DNUData_Read_Request:
 *         Given a list of arguments for DNClient for read data from datanode.
 *         DNClient will create corresponding DNRequest base on input parameters
 *         and send request to DN
 * @md_item:        metadata item that has target DN information for DNClient to form a request
 * @replic_idx:     which replica (DN) inside metadata item DNClient should access
 * @payload_offset: which start offset in payload buffer of caller.
 *                  DNClient need this to pass back to caller when write data to
 *                  caller's buffer
 * @callerReq     : which caller request need DNClient fullfill it's mission
 * @endfn         : callback function that DNClient should call when got ack from DN
 * @pdata         : argument for callback function
 * @bk_idx_caller : caller provide a array for holding all related DNRequest.
 *                  caller pass the array index to DNClient. DNClient has to return
 *                  this value to caller when execute callback function endfn.
 *                  With this bk_idx_caller, caller can skip call cacnel_request
 *                  to DNRequest that calling endfn
 * @dnReqID_bk    : caller provide a space for storing DN Request ID.
 *                  DNClient has to store DNRequest ID in bucket.
 *                  Caller can cancel DNRequest base on Request ID
 * @send_data     : caller ask DNClient to generate a DNRequest with send request head + payload
 *                  or send request head only
 * @rw_data       : caller ask DNClient to generate a DNRequest that will ask DN
 *                  to write data to disk or not, or read data from disk or not
 *
 * Return: submit success or fail
 *         0: success
 *         ~0: fail
 */
int32_t submit_DNUData_Read_Request (metaD_item_t *md_item,
        int32_t replic_idx, uint64_t payload_offset, ReqCommon_t *callerReq,
        DNUDReq_endfn_t *endfn, void *pdata, int32_t bk_idx_caller, uint32_t *dnReqID_bk,
        bool send_data, bool rw_data)
{
    DNUData_Req_t *myReq;
    int32_t ret;

    if (IS_ERR_OR_NULL(md_item)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN null metadata gen read DN\n");
        DMS_WARN_ON(true);
        return -EINVAL;
    }

    myReq = DNUDReq_alloc_Req();
    if (IS_ERR_OR_NULL(myReq)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN fail alloc datanode request read\n");
        DMS_WARN_ON(true);
        return -ENOMEM;
    }

    ret = 0;
    DNUDReq_init_Req(myReq, DNUDREQ_READ, callerReq, md_item, replic_idx);
    myReq->caller_bkidx = bk_idx_caller;
    if ((ret = init_DNUDReq_Read(myReq, payload_offset, endfn, pdata,
            send_data, rw_data))) {
        //TODO: should free dn req
        dms_printk(LOG_LVL_WARN, "DMSC WARN fail init read dnReq\n");
        return ret;
    }

    DNUDReq_ref_Req(myReq);
    myReq->mdata_item = md_item;
    *dnReqID_bk = myReq->dnReq_comm.ReqID;
    DNCWker_add_Req_workQ(myReq, &dnClient_mgr.dnClient);
    DNUDReq_deref_Req(myReq);

    return ret;
}

/*
 * submit_DNUData_Write_Request:
 *         Given a list of arguments for DNClient for write data to datanode.
 *         DNClient will create corresponding DNRequest base on input parameters
 *         and send request to DN
 * @md_item:        metadata item that has target DN information for DNClient to form a request
 * @replic_idx:     which replica (DN) inside metadata item DNClient should access
 * @data_buf:       memory buffer that hold user data for DNClient sending to DN
 * @payload_offset: which start offset of data_buf that DNClient should send
 * @callerReq     : which caller request need DNClient fullfill it's mission
 * @endfn         : callback function that DNClient should call when got ack from DN
 * @pdata         : argument for callback function
 * @bk_idx_caller : caller provide a array for holding all related DNRequest.
 *                  caller pass the array index to DNClient. DNClient has to return
 *                  this value to caller when execute callback function endfn.
 *                  With this bk_idx_caller, caller can skip call cacnel_request
 *                  to DNRequest that calling endfn
 * @dnReqID_bk    : caller provide a space for storing DN Request ID.
 *                  DNClient has to store DNRequest ID in bucket.
 *                  Caller can cancel DNRequest base on Request ID
 * @fp_map:       : fingerprint map to indicate which 4K data has fingerprint in
 *                  fp_cache, if no, sender thread can calculate fingerprint,
 *                            if yes, sender read fingerprint from fp_cache
 *                  0: not finger print in fp_cache
 *                  1: has finger print in fp_cache
 * @fp_cache      : for storing fingerprint generated by sender thread
 * @send_data     : caller ask DNClient to generate a DNRequest with send request head + payload
 *                  or send request head only
 * @rw_data       : caller ask DNClient to generate a DNRequest that will ask DN
 *                  to write data to disk or not, or read data from disk or not
 *
 * Return: submit success or fail
 *         0: success
 *         ~0: fail
 */
int32_t submit_DNUData_Write_Request (metaD_item_t *md_item,
        int32_t replic_idx, data_mem_chunks_t *data_buf, uint64_t payload_offset,
        ReqCommon_t *callerReq,
        DNUDReq_endfn_t *endfn, void *pdata,
        int32_t bk_idx_caller, uint32_t *dnReqID_bk,
        uint8_t *fp_map, uint8_t *fp_cache, bool send_data, bool rw_data)
{
    DNUData_Req_t *myReq;
    int32_t ret;

    myReq = DNUDReq_alloc_Req();
    if (unlikely(NULL == myReq)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN fail alloc datanode request write\n");
        DMS_WARN_ON(true);
        return -ENOMEM;
    }

    ret = 0;
    DNUDReq_init_Req(myReq, DNUDREQ_WRITE, callerReq, md_item, replic_idx);
    myReq->caller_bkidx = bk_idx_caller;

    if ((ret = init_DNUDReq_Write(myReq, data_buf, payload_offset,
            endfn, pdata, fp_map, fp_cache, send_data, rw_data))) {
        //TODO: should free dn req
        dms_printk(LOG_LVL_WARN, "DMSC WARN fail init write dnReq\n");
        return ret;
    }

    DNUDReq_ref_Req(myReq);
    myReq->mdata_item = md_item;
    *dnReqID_bk = myReq->dnReq_comm.ReqID;
    DNCWker_add_Req_workQ(myReq, &dnClient_mgr.dnClient);
    DNUDReq_deref_Req(myReq);

    return ret;
}

/*
 * _retry_DNUDReq_rlist:
 *         Given a list of DN Request, call function in DNRequest to resend request
 *
 * @lhead: pool for requests that waiting for resend
 *
 */
static void _retry_DNUDReq_rlist (struct list_head *lhead)
{
    int8_t ipv4addr_str[IPV4ADDR_NM_LEN];
    DNUData_Req_t *cur, *next;

    list_for_each_entry_safe (cur, next, lhead, entry_dnWkerQ) {
        list_del(&cur->entry_dnWkerQ);
        ccma_inet_aton(cur->ipaddr, ipv4addr_str, IPV4ADDR_NM_LEN);
        DNUDReq_retry_Req(cur);
        DNUDReq_deref_Req(cur);
    }
}

/*
 * DNClientMgr_resend_DNUDReq:
 *         When a connection back to connected, caller can call this function.
 *         DNClient will scan all request that belong to particular DN in request pool
 *         and resend these requests to that DN again
 *
 * @ipaddr: IP address of DN that backed to connect
 * @port:   port of DN that backed to connect
 *
 */
void DNClientMgr_resend_DNUDReq (uint32_t ipaddr, uint32_t port)
{
    int8_t str_ipaddr[IPV4ADDR_NM_LEN];
    struct list_head retry_list;
    int32_t num_retry;

    ccma_inet_aton(ipaddr, str_ipaddr, IPV4ADDR_NM_LEN);
    //Retry requests stay at dn pending q
    INIT_LIST_HEAD(&retry_list);

    num_retry = 0;
    DNUDReqPool_get_retryReq(ipaddr, port, &retry_list, &num_retry,
            &dnClient_mgr.UData_ReqPool);
    if (num_retry > 0) {
        _retry_DNUDReq_rlist(&retry_list);
    }
}

#if 0
//TODO: commit DNReq to payload request when conn lost to read from other replica
void ci_DNUDReq_payloadReq_ConnLost (uint32_t ipaddr, uint32_t port)
{
    //TODO: commit DN request to payload request for reading data from other replica
    //step 1: prepare a callback function to conn - generate a task
    //step 2: _retry_discoC_conn call callback function
    //note if a request be handled by DN worker will trigger some issue?
}
#endif

/*
 * DNClientMgr_update_DNUDReq_TOInfo:
 *         In this version, the timeout be set at payload request.
 *         DNClient provide a API for payload manager to tell DNClient that
 *         payload manager didn't receive particular DNRequest's response.
 *         DNClient has to find out corresponding request from request pool,
 *         and add a record to flowcontrol module that a DN has slow response.
 *         An next stage, payload manager can select a fast DN to read base on
 *         the information in flowcontrol module
 * @dnReqID: DNRequest ID of which DN Request not yet response
 * @callerID: Caller ID of which DN Request not yet response
 *
 * NOTE: A DN Request has a Request ID and a caller ID
 */
void DNClientMgr_cancel_DNUDReq (uint32_t dnReqID, uint32_t callerID)
{
    DNUDReq_cancel_Req(dnReqID, callerID);
}

/*
 * DNClientMgr_update_DNUDReq_TOInfo:
 *         In this version, the timeout be set at payload manager.
 *         So DNClient provide a API for payload manager to tell DNClient that
 *         payload manager didn't receive particular DNRequest's response.
 *         DNClient has to find out corresponding request from request pool,
 *         and add a record to flowcontrol module that a DN has slow response.
 *         An next stage, payload manager can select a fast DN to read data base on
 *         the information in flowcontrol module
 * @dnReqID: DNRequest ID of which DN Request not yet response
 * @callerID: Caller ID of which DN Request not yet response
 *
 * NOTE: A DN Request has a Request ID and a caller ID
 */
void DNClientMgr_update_DNUDReq_TOInfo (uint32_t dnReqID, uint32_t callerID)
{
    int32_t ipv4addr, port, ret;

    ret = DNUDReq_get_dnInfo(dnReqID, callerID, &ipv4addr, &port);

    if (ret == 0) {
        add_request_timeout_record(ipv4addr, port);
    }
}

/*
 * _init_DNC_Mgr: initialize each data member of manager instance
 * @myMgr: target instance to be initialized
 *
 * Return: initialize success or fail
 *         0: success
 *         ~0: fail
 */
static int32_t _init_DNC_Mgr (DNClient_MGR_t *myMgr)
{
    int32_t ret, num_req_size;

    ret = 0;

    atomic_set(&myMgr->DNUDReqID_counter, 0);
    atomic_set(&myMgr->num_DNReq_waitResp, 0);
    myMgr->lastMax_DNUDRespID = 0;
    atomic_set(&myMgr->num_dnClient, 0);

    num_req_size = dms_client_config->max_nr_io_reqs*MAX_LBS_PER_RQ*MAX_NUM_REPLICA;
    myMgr->DNUDReq_mpool_ID = register_mem_pool("DNRequest", num_req_size,
            sizeof(DNUData_Req_t), true);
    if (myMgr->DNUDReq_mpool_ID < 0) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN fail register dn req mem pool %d\n",
                myMgr->DNUDReq_mpool_ID);
        return -ENOMEM;
    }
    dms_printk(LOG_LVL_INFO, "DMSC INFO DN Req manager get mem pool id %d\n",
            myMgr->DNUDReq_mpool_ID);

    num_req_size = dms_client_config->max_nr_io_reqs + READBUFF_EXTRA_SIZE;
    myMgr->DNUDReadBuff_mpool_ID = register_mem_pool("DNReqReadBuffer", num_req_size,
            MAX_SIZE_KERNEL_MEMCHUNK, true);
    if (myMgr->DNUDReadBuff_mpool_ID < 0) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN fail reg dn req read buffer mem pool %d\n",
                myMgr->DNUDReadBuff_mpool_ID);
        return -ENOMEM;
    }
    dms_printk(LOG_LVL_INFO,
            "DMSC INFO DN Req manager get read buffer mem pool id %d\n",
            myMgr->DNUDReadBuff_mpool_ID);

    return ret;
}

/*
 * DNClientMgr_init_manager: initialize whole DNClient module which includes a manager instance and
 *                           initialize ack manager
 * manager instance: dnClient_mgr
 *
 * Return: initialize success or fail
 *         0: success
 *         ~0: fail
 */
int32_t DNClientMgr_init_manager (void)
{
    uint8_t *ptr;
    int32_t ret, i;

    ret = 0;
    do {
        DNProto_ReadMask_buffer =
                discoC_mem_alloc(sizeof(uint8_t)*MAX_LBS_PER_RQ*8, GFP_KERNEL);
        if (IS_ERR_OR_NULL(DNProto_ReadMask_buffer)) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN fail alloc DN protcol read mask buffer\n");
            ret = -ENOMEM;
            break;
        }

        ptr = DNProto_ReadMask_buffer;
        for (i = 0; i < MAX_LBS_PER_RQ; i++) {
            *ptr = 0x1;
            ptr += sizeof(uint8_t) * 8;
        }

#if (DN_PROTOCOL_VERSION == 2) && (CN_CRASH_RECOVERY == 1)
        ts_chk_bit = 0;
        ts_cleanLog = 0;
#endif

        if ((ret = _init_DNC_Mgr(&dnClient_mgr))) {
            return ret;
        }

        DNUDReqPool_init_pool(&dnClient_mgr.UData_ReqPool);
        ret = DNCWker_init_worker(&dnClient_mgr.dnClient, "dms_dn_worker",
                &dnClient_mgr);
        if (ret) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN fail init DN Client worker manager\n");
            break;
        }

        ret = DNAckMgr_init_manager();
        if (ret) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN fail init DN Ack worker manager\n");
            break;
        }

        DNAckEIJ_init_manager();
    } while (0);

    return ret;
}

/*
 * DNClientMgr_rel_manager: release whole DNClient module which includes a manager instance and
 *                          release ack manager
 * manager instance: dnClient_mgr
 *
 * Return: release success or fail
 *         0: success
 *         ~0: fail
 */
int32_t DNClientMgr_rel_manager (void)
{
    int32_t ret, count;

    dms_printk(LOG_LVL_INFO, "DMSC INFO rel DN Request Client manager\n");

    ret = DNAckMgr_rel_manager();

    ret = DNCWker_stop_worker(&dnClient_mgr.dnClient);

    count = 0;
    while (atomic_read(&dnClient_mgr.num_dnClient) != 0) {
        msleep(100);
        count++;

        if (count == 0 || count % 10 != 0) {
            continue;
        }

        dms_printk(LOG_LVL_WARN,
                "DMSC WARN fail stop DNReq worker %d thread running "
                "(wait count %d)\n",
                atomic_read(&dnClient_mgr.num_dnClient), count);
    }

    DNUDReqPool_rel_pool(&dnClient_mgr.UData_ReqPool);

    if (!IS_ERR_OR_NULL(DNProto_ReadMask_buffer)) {
        discoC_mem_free(DNProto_ReadMask_buffer);
    }

    ret = deregister_mem_pool(dnClient_mgr.DNUDReq_mpool_ID);
    ret = deregister_mem_pool(dnClient_mgr.DNUDReadBuff_mpool_ID);

    DNAckEIJ_rel_manager();
    dms_printk(LOG_LVL_INFO, "DMSC INFO rel DN Request Client manager DONE\n");
    return ret;
}
