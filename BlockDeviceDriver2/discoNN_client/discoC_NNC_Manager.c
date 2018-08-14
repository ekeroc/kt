/*
 * Copyright (C) 2010 - 2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_NNC_Manager.c
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
#include "../common/thread_manager.h"
#include "../flowcontrol/FlowControl.h"
#include "../config/dmsc_config.h"
#include "discoC_NNC_Manager_api.h"
#include "discoC_NNC_worker.h"
#include "discoC_NNClient.h"
#include "discoC_NNMData_Request.h"
#include "discoC_NNMDReqPool.h"
#include "discoC_NNC_workerfunc_reportMD.h"
#include "discoC_NNC_Manager.h"
#include "discoC_NNC_Manager_private.h"
#include "discoC_NNC_worker.h"
#ifdef DISCO_PREALLOC_SUPPORT
#include "../metadata_manager/space_manager_export_api.h"
#endif

/*
 * _get_NNClientIP_bkID: get bucket of IP hashtable by given NNClient namenode's IP
 *     bucket number = ipaddr % number of bucket
 * @ipaddr: target IP for find
 *
 * Return: which bucket for holding NNClient
 */
static uint32_t _get_NNClientIP_bkID (uint32_t ipaddr)
{
    return (ipaddr % NNCLIENT_HBUCKET_SIZE);
}

/*
 * _find_NNClientIP_lockless: find NNClient within a bucket pool by IP
 * @bkID:  target bucket pool for searching
 * @ipaddr: target NNClient's namenode ip
 * @port: target NNClient's namenode port
 *
 * Return: NNClient
 */
static discoC_NNClient_t *_find_NNClientIP_lockless (uint32_t bkID,
        uint32_t ipaddr, uint32_t port)
{
    discoC_NNClient_t *cur, *next, *myClient;
    struct list_head *lhead;

    lhead = &(nnClient_mgr.nnClientIP_hpool[bkID]);

    myClient = NULL;
    list_for_each_entry_safe (cur, next, lhead, entry_nnc_IPAddrPool) {
        if (cur->nnSrv_IPAddr == ipaddr && cur->nnSrv_port == port) {
            myClient = cur;
            break;
        }
    }

    return myClient;
}

/*
 * _get_NNClientID_bkID: get bucket of ID hashtable by given NNClient ID
 *     bucket number = nncID % number of bucket
 * @nncID: target ID for find
 *
 * Return: which bucket for holding NNClient
 */
static uint32_t _get_NNClientID_bkID (uint32_t nncID)
{
    return (nncID % NNCLIENT_HBUCKET_SIZE);
}

/*
 * _find_NNClientID_lockless: find NNClient within a bucket pool by ID
 * @bkID:  target bucket pool for searching
 * @nncID: target ID for find
 *
 * Return: search result NNClient
 */
static discoC_NNClient_t *_find_NNClientID_lockless (uint32_t bkID, uint32_t nncID)
{
    discoC_NNClient_t *cur, *next, *myClient;
    struct list_head *lhead;

    lhead = &(nnClient_mgr.nnClientID_hpool[bkID]);

    myClient = NULL;
    list_for_each_entry_safe (cur, next, lhead, entry_nnc_IDPool) {
        if (cur->nnClient_ID == nncID) {
            myClient = cur;
            break;
        }
    }

    return myClient;
}

/*
 * NNClientMgr_alloc_NNClientID: allocate a NNClient ID
 *
 * Return: NNClient ID
 */
static uint32_t NNClientMgr_alloc_NNClientID (void)
{
    uint32_t nnCID;

    nnCID = 0;;
    while (nnCID == 0) {
        nnCID = (uint32_t)atomic_add_return(1, &nnClient_mgr.nnClient_ID_gen);
    }

    return nnCID;
}

/*
 * get_NN_ipaddr: get namenode ip of default NNClient
 *
 * Return: ip address of namenode
 */
int8_t *get_NN_ipaddr (uint32_t *port)
{
    discoC_NNClient_t *myClient;

    //TODO: we need a correct ID
    myClient = NNClientMgr_get_defNNClient();

    *port = myClient->nnSrv_port;
    return myClient->nnSrv_IPAddr_str;
}

/*
 * get_num_noResp_nnReq: get number request without response of
 *                    default NNClient
 *
 * Return: number on-fly requests
 */
uint32_t get_num_noResp_nnReq (void)
{
    discoC_NNClient_t *myClient;

    //TODO: we need a correct ID
    myClient = NNClientMgr_get_defNNClient();

    return (uint32_t)atomic_read(&myClient->num_nnReq_noResp);
}

/*
 * get_last_rcv_nnReq_ID: get last receive MDReq ID of
 *                    default NNClient
 *
 * Return: last receive request ID
 */
uint32_t get_last_rcv_nnReq_ID (void)
{
    discoC_NNClient_t *myClient;

    //TODO: we need a correct ID
    myClient = NNClientMgr_get_defNNClient();
    return myClient->nnReq_last_rcvID;
}

/*
 * get_ch_workq_size: get number of requests in
 *                    default NNClient's cache miss worker queue
 *
 * Return: number of requests
 */
int32_t get_cm_workq_size (void)
{
    discoC_NNClient_t *myClient;

    //TODO: we need a correct ID
    myClient = NNClientMgr_get_defNNClient();
    return NNCWker_get_numReq_workQ(&myClient->nnMDCM_worker);
}

/*
 * get_ch_workq_size: get number of requests in
 *                    default NNClient's cache hit worker queue
 *
 * Return: number of requests
 */
int32_t get_ch_workq_size (void)
{
    discoC_NNClient_t *myClient;

    //TODO: we need a correct ID
    myClient = NNClientMgr_get_defNNClient();
    return NNCWker_get_numReq_workQ(&myClient->nnMDCH_worker);
}

/*
 * NNClientMgr_alloc_MDReqID: allocate a metadata request
 *
 * Return: metadata request ID for new metadata request
 */
uint32_t NNClientMgr_alloc_MDReqID (void)
{
    return (uint32_t)atomic_add_return(1, &nnClient_mgr.MDReq_ID_gen);
}

/*
 * NNClientMgr_get_last_MDReqID: get last allocated metadata request ID
 *
 * Return: max request ID
 */
uint32_t NNClientMgr_get_last_MDReqID (void)
{
    return (uint32_t)atomic_read(&nnClient_mgr.MDReq_ID_gen);
}

/*
 * NNClientMgr_get_num_MDReq: get number of request in pool
 *
 * Return: number requests
 */
int32_t NNClientMgr_get_num_MDReq (void)
{
    return NNMDReqPool_get_numReq(&nnClient_mgr.MData_ReqPool);
}

/*
 * NNClientMgr_show_MDRequest: dump all MDreq in Req pool to buffer
 *
 * Return: size of bytes in buffer
 */
int32_t NNClientMgr_show_MDRequest (int8_t *buffer, int32_t buff_len)
{
    return NNMDReqPool_show_pool(&nnClient_mgr.MData_ReqPool, buffer, buff_len);
}

/*
 * NNClientMgr_get_NNClient: get NNClient from IP pool by namenode IP
 *
 * Return: default NNClient instance
 */
discoC_NNClient_t *NNClientMgr_get_NNClient (uint32_t nnSrvIP, uint32_t nnSrvPort)
{
    discoC_NNClient_t *myClient;
    uint32_t bkID;

    bkID = _get_NNClientIP_bkID(nnSrvIP);

    read_lock(&nnClient_mgr.nnClient_plock);
    myClient = _find_NNClientIP_lockless(bkID, nnSrvIP, nnSrvPort);
    read_unlock(&nnClient_mgr.nnClient_plock);

    return myClient;
}

/*
 * NNClientMgr_get_NNClient_byID: get NNClient from ID pool by ID
 *
 * Return: default NNClient instance
 */
discoC_NNClient_t *NNClientMgr_get_NNClient_byID (uint32_t nnClientID)
{
    uint32_t bkID;
    discoC_NNClient_t *myClient;

    bkID = _get_NNClientID_bkID(nnClientID);

    read_lock(&nnClient_mgr.nnClient_plock);
    myClient = _find_NNClientID_lockless(bkID, nnClientID);
    read_unlock(&nnClient_mgr.nnClient_plock);

    return myClient;
}

/*
 * NNClientMgr_get_defNNClient: get default NNClient
 *
 * Return: default NNClient instance
 */
discoC_NNClient_t *NNClientMgr_get_defNNClient (void)
{
    return nnClient_mgr.def_nnClient;
}

#if 0
discoC_conn_t *get_NNClient_conn (uint32_t nnIPAddr, uint32_t nnPort)
{
    discoC_NNClient_t *myClient;
    discoC_conn_t *myConn;
    uint32_t bkID;

    myConn = NULL;
    bkID = _get_NNClientIP_bkID(nnIPAddr);
    read_lock(&nnClient_mgr.nnClient_plock);
    myClient = _find_NNClientIP_lockless(bkID, nnIPAddr, nnPort);
    if (!IS_ERR_OR_NULL(myClient)) {
        myConn = myClient->nnSrv_conn;
    }
    read_unlock(&nnClient_mgr.nnClient_plock);

    return myConn;
}
#endif

/*
 * NNClientMgr_ref_NNClient:
 *     reference NNClient when attach volume
 * @nnIPAddr: ip addree of namenode's NNClient to dereference
 * @nnPort  : port of namenode's NNClient to dereference
 *
 * Return: NNClient ID
 */
uint32_t NNClientMgr_ref_NNClient (uint32_t nnIPAddr, uint32_t nnPort)
{
    discoC_NNClient_t *myClient;
    int8_t ipv4addr_str[IPV4ADDR_NM_LEN];
    uint32_t bkID, nnCID;

    nnCID = 0;

    ccma_inet_aton(nnIPAddr, ipv4addr_str, IPV4ADDR_NM_LEN);
    dms_printk(LOG_LVL_INFO, "DMSC INFO attach volume gain NNClient %s:%d\n",
            ipv4addr_str, nnPort);

    bkID = _get_NNClientIP_bkID(nnIPAddr);

    read_lock(&nnClient_mgr.nnClient_plock);
    myClient = _find_NNClientIP_lockless(bkID, nnIPAddr, nnPort);
    if (!IS_ERR_OR_NULL(myClient)) {
        nnCID = myClient->nnClient_ID;
        atomic_inc(&myClient->volume_refCnt);
    }
    read_unlock(&nnClient_mgr.nnClient_plock);

    dms_printk(LOG_LVL_INFO, "DMSC INFO attach volume gain NNClient %s:%d %u\n",
            ipv4addr_str, nnPort, nnCID);

    return nnCID;
}

/*
 * NNClientMgr_deref_NNClient:
 *     deference NNClient when detach volume
 * @nnIPAddr: ip addree of namenode's NNClient to dereference
 * @nnPort  : port of namenode's NNClient to dereference
 *
 * Return: NNClient ID
 */
uint32_t NNClientMgr_deref_NNClient (uint32_t nnIPAddr, uint32_t nnPort)
{
    discoC_NNClient_t *myClient;
    int8_t ipv4addr_str[IPV4ADDR_NM_LEN];
    uint32_t bkID, nnCID;

    nnCID = 0;

    ccma_inet_aton(nnIPAddr, ipv4addr_str, IPV4ADDR_NM_LEN);
    dms_printk(LOG_LVL_INFO, "DMSC INFO detach volume rel NNClient %s:%d\n",
            ipv4addr_str, nnPort);

    bkID = _get_NNClientIP_bkID(nnIPAddr);

    read_lock(&nnClient_mgr.nnClient_plock);
    myClient = _find_NNClientIP_lockless(bkID, nnIPAddr, nnPort);
    if (!IS_ERR_OR_NULL(myClient)) {
        nnCID = myClient->nnClient_ID;
        atomic_dec(&myClient->volume_refCnt);
    }
    read_unlock(&nnClient_mgr.nnClient_plock);

    dms_printk(LOG_LVL_INFO, "DMSC INFO detach volume rel NNClient %s:%d %u\n",
            ipv4addr_str, nnPort, nnCID);
    return nnCID;
}

/*
 * NNClientMgr_wakeup_NNCWorker:
 *     Wakeup all worker of NNClient
 * TODO: for supporting multiple NN, this function should find specific NNClient
 */
void NNClientMgr_wakeup_NNCWorker (void)
{
    NNClient_wakeup_worker(WAKEUP_ALL);
}

/*
 * retry_NNMData_rlist:
 *     Retry all requests in list by submitting them into worker queue
 * @lhead:link list pool that has requests for retrying
 */
static void retry_NNMData_rlist (struct list_head *lhead)
{
    NNMData_Req_t *cur, *next;

    list_for_each_entry_safe (cur, next, lhead, entry_retryPool) {
        list_del(&cur->entry_retryPool);
        NNMDReq_retry_Req(cur);
    }
}

/*
 * NNClientMgr_retry_NNMDReq:
 *     1. scan all requests in request pool of manager
 *     2. check whether a request should be retry (acquire / report request)
 *     3. submit all retryable request to worker queue
 * @nnIPaddr: IP of namenode that NNClient re-connect for retrying request
 * @nnPort: port of namenode that NNClient re-connect for retrying request
 *
 * TODO: test me again
 */
void NNClientMgr_retry_NNMDReq (uint32_t nnIPaddr, uint32_t nnPort)
{
    struct list_head allocMDReq_rlist, ciMDReq_rlist;
    int32_t num_allocMDReq, num_ciMDReq;

    INIT_LIST_HEAD(&allocMDReq_rlist);
    INIT_LIST_HEAD(&ciMDReq_rlist);
    num_allocMDReq = 0;
    num_ciMDReq = 0;
    NNMDReqPool_get_retryReq(nnIPaddr, nnPort, &allocMDReq_rlist, &num_allocMDReq,
            &ciMDReq_rlist, &num_ciMDReq, &nnClient_mgr.MData_ReqPool);

    if (num_allocMDReq > 0) {
        retry_NNMData_rlist(&allocMDReq_rlist);
    }

    if (num_ciMDReq > 0) {
        retry_NNMData_rlist(&ciMDReq_rlist);
    }
}

#if 0
#ifdef DISCO_PREALLOC_SUPPORT
static bool _alloc_metadata_prealloc_pool (UserIO_addr_t *ioaddr,
        NNMData_Req_t *nnMDReq)
{
    int32_t ret;
    bool do_wait;

    do_wait = false;

    ret = sm_alloc_volume_space(ioaddr->volumeID,
            ioaddr->lbid_start, ioaddr->lbid_len, nnMDReq->MetaData,
            &nnMDReq->nnMDReq_comm);
    if (ret == 0) {
        nnMDReq->nnReq_setOVW_fn(nnMDReq->usr_pdata, ioaddr->lbid_start, ioaddr->lbid_len);
        NNMDReq_set_fea(nnMDReq, NNREQ_FEA_CACHE_HIT);
    } else {
        do_wait = true;
    }

    return do_wait;
}
#endif
#endif

/*
 * discoC_acquire_metadata: API for other modules request an acquire metadata
 *     services
 * @ioaddr: user namespace information (which namenode, which volume, LBA range)
 * @mdCacheOn: whether volume metadata cache on or off
 * @callerReq: which caller request trigger this service
 * @volReplica: volume's default replica
 * @mdCachehit: whether metadata from local cache
 * @ovw: whether first write or overwrite
 * @hbiderr: whether caller want NNClient get true metadata
 * @arr_MD: metadata array for NNClient to check and report DN access to namenode
 * @udata: private data for callback function
 * @MD_endfn: callback function for NNClient notify caller request
 *
 * Return: submit report request result
 *         0: submit OK
 *        ~0: submit fail
 */
int32_t discoC_acquire_metadata (UserIO_addr_t *ioaddr, bool mdCacheOn,
        ReqCommon_t *callerReq, uint16_t volReplica,
        bool mdCachehit, bool ovw,
        bool hbiderr, metadata_t *arr_MD, void *udata,
        nnMDReq_end_fn_t *MD_endfn, nnMDReq_setovw_fn_t *setOVW_fn)
{
    NNMData_Req_t *nnMDReq;
    int32_t ret;

    ret = 0;

    do {
        nnMDReq = NNMDReq_alloc_Req();
        if (IS_ERR_OR_NULL(nnMDReq)) {
            ret = -ENOMEM;
            dms_printk(LOG_LVL_WARN, "DMSC WARN fail alloc MDReq callReqID %u\n",
                    callerReq->ReqID);
            break;
        }

        ret = NNMDReq_init_Req(nnMDReq, ioaddr, callerReq, volReplica, mdCachehit,
                ovw, hbiderr, mdCacheOn, false);

        //TODO: using ioreq_timeout is wrong, we should re-caclulate
        ret = NNMDReq_init_endfn(nnMDReq, arr_MD, MD_endfn, setOVW_fn, udata,
                dms_client_config->ioreq_timeout);
        if (ret) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN fail init MDReq %u callerID %u\n",
                    nnMDReq->nnMDReq_comm.ReqID, callerReq->ReqID);
            break;
        }

#if 0
#ifdef DISCO_PREALLOC_SUPPORT
        if (ovw == false && callerReq->ReqOPCode == KERNEL_IO_WRITE) {
            if (_alloc_metadata_prealloc_pool(ioaddr, nnMDReq)) {
                break;
            }
        }
#endif
#endif

        NNMDReq_ref_Req(nnMDReq);
        if (NNMDReq_submit_workerQ(nnMDReq)) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN fail add nnMDReq to workerQ "
                    "%u (%llu %u)\n", nnMDReq->nnMDReq_comm.ReqID,
                    nnMDReq->nnMDReq_ioaddr.lbid_start,
                    nnMDReq->nnMDReq_ioaddr.lbid_len);
        }
        NNMDReq_deref_Req(nnMDReq);

    } while (0);

    return ret;
}

/*
 * discoC_report_metadata: API for other modules request an report metadata
 *     services
 * @ioaddr: user namespace information (which namenode, which volume, LBA range)
 * @mdCacheOn: whether volume metadata cache on or off (useless when report)
 * @callerReq: which caller request trigger this service
 * @volReplica: volume's default replica (useless when report)
 * @mdCachehit: whether metadata from local cache (useless when report)
 * @ovw: whether first write or overwrite (useless when report)
 * @hbiderr: whether caller want NNClient get true metadata (useless when report)
 * @arr_MD: metadata array for NNClient to check and report DN access to namenode
 * @udata: private data for callback function
 * @MD_endfn: callback function for NNClient notify caller request
 *
 * Return: submit report request result
 *         0: submit OK
 *        ~0: submit fail
 */
int32_t discoC_report_metadata (UserIO_addr_t *ioaddr, bool mdCacheOn,
        ReqCommon_t *callerReq, uint16_t volReplica,
        bool mdCachehit, bool ovw, bool hbiderr,
        metadata_t *arr_MD, void *udata, nnMDReq_end_fn_t *MD_endfn)
{
    NNMData_Req_t *nnMDReq;
    int32_t ret;

    ret = 0;
    do {
        nnMDReq = NNMDReq_alloc_Req();
        if (IS_ERR_OR_NULL(nnMDReq)) {
            ret = -ENOMEM;
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN fail alloc Report MDReq callerID %u\n",
                    callerReq->ReqID);
            break;
        }

        NNMDReq_init_Req(nnMDReq, ioaddr, callerReq, volReplica, mdCachehit,
                ovw, hbiderr, mdCacheOn, true);
        //TODO: timeout interval can be a long fix value
        ret = NNMDReq_init_endfn(nnMDReq, arr_MD, MD_endfn, NULL, udata,
                dms_client_config->ioreq_timeout);
        if (ret) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN fail init ReportMDReq %u callerID %u\n",
                    nnMDReq->nnMDReq_comm.ReqID, callerReq->ReqID);
            break;
        }

        NNMDReq_ref_Req(nnMDReq);
        if (NNMDReq_submit_workerQ(nnMDReq)) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN fail add nnMDReq to workerQ "
                    "%u (%llu %u)\n", nnMDReq->nnMDReq_comm.ReqID,
                    nnMDReq->nnMDReq_ioaddr.lbid_start,
                    nnMDReq->nnMDReq_ioaddr.lbid_len);
        }
        NNMDReq_deref_Req(nnMDReq);
    } while (0);

    return ret;
}

#ifdef DISCO_PREALLOC_SUPPORT
int32_t discoC_prealloc_freeSpace (UserIO_addr_t *ioaddr, ReqCommon_t *callerReq,
     metadata_t *arr_MD, void *udata, nnMDReq_end_fn_t *MD_endfn)
{
    NNMData_Req_t *nnMDReq;
    int32_t ret;

    ret = 0;

    do {
        nnMDReq = NNMDReq_alloc_Req();
        if (IS_ERR_OR_NULL(nnMDReq)) {
            ret = -ENOMEM;
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN fail alloc MDReq for alloc free space callerID %u\n",
                    callerReq->ReqID);
            break;
        }

        NNMDReq_init_Req(nnMDReq, ioaddr, callerReq, MAX_NUM_REPLICA, true, true,
                false, true, false);
        nnMDReq->nnMDReq_comm.ReqOPCode = REQ_NN_PREALLOC_SPACE;
        //TODO: timeout interval can be a long fix value
        ret = NNMDReq_init_endfn(nnMDReq, arr_MD, MD_endfn, NULL, udata,
                dms_client_config->ioreq_timeout);
        if (ret) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN fail init MDReq for alloc free space %u callerID %u\n",
                    nnMDReq->nnMDReq_comm.ReqID, callerReq->ReqID);
            break;
        }

        NNMDReq_ref_Req(nnMDReq);
        if (NNMDReq_submit_workerQ(nnMDReq)) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN fail add nnMDReq to workerQ for alloc free space"
                    "%u (%llu %u)\n", nnMDReq->nnMDReq_comm.ReqID,
                    nnMDReq->nnMDReq_ioaddr.lbid_start,
                    nnMDReq->nnMDReq_ioaddr.lbid_len);
        }
        NNMDReq_deref_Req(nnMDReq);
    } while (0);

    return ret;
}

int32_t discoC_flush_spaceMetadata (UserIO_addr_t *ioaddr, ReqCommon_t *callerReq,
     metadata_t *arr_MD, void *udata, nnMDReq_end_fn_t *MD_endfn)
{
    NNMData_Req_t *nnMDReq;
    int32_t ret;

    ret = 0;

    do {
        nnMDReq = NNMDReq_alloc_Req();
        if (IS_ERR_OR_NULL(nnMDReq)) {
            ret = -ENOMEM;
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN fail alloc MDReq for flush space metadata callerID %u\n",
                    callerReq->ReqID);
            break;
        }

        NNMDReq_init_Req(nnMDReq, ioaddr, callerReq, MAX_NUM_REPLICA, true, true,
                false, true, false);
        nnMDReq->nnMDReq_comm.ReqOPCode = REQ_NN_REPORT_SPACE_MDATA;
        //TODO: timeout interval can be a long fix value
        ret = NNMDReq_init_endfn(nnMDReq, arr_MD, MD_endfn, NULL, udata,
                dms_client_config->ioreq_timeout);
        if (ret) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN fail init MDReq for flush space metadata %u callerID %u\n",
                    nnMDReq->nnMDReq_comm.ReqID, callerReq->ReqID);
            break;
        }

        NNMDReq_ref_Req(nnMDReq);
        if (NNMDReq_submit_workerQ(nnMDReq)) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN fail add MDReq to workerQ for flush space metadata "
                    "%u (%llu %u)\n", nnMDReq->nnMDReq_comm.ReqID,
                    nnMDReq->nnMDReq_ioaddr.lbid_start,
                    nnMDReq->nnMDReq_ioaddr.lbid_len);
        }
        NNMDReq_deref_Req(nnMDReq);
    } while (0);

    return ret;
}

int32_t discoC_report_freeSpace (UserIO_addr_t *ioaddr, ReqCommon_t *callerReq,
     metadata_t *arr_MD, void *udata, nnMDReq_end_fn_t *MD_endfn)
{
    NNMData_Req_t *nnMDReq;
    int32_t ret;

    ret = 0;

    do {
        nnMDReq = NNMDReq_alloc_Req();
        if (IS_ERR_OR_NULL(nnMDReq)) {
            ret = -ENOMEM;
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN fail alloc MDReq for report free callerID %u\n",
                    callerReq->ReqID);
            break;
        }

        NNMDReq_init_Req(nnMDReq, ioaddr, callerReq, MAX_NUM_REPLICA, true, true,
                false, true, false);
        nnMDReq->nnMDReq_comm.ReqOPCode = REQ_NN_RETURN_SPACE;
        //TODO: timeout interval can be a long fix value
        ret = NNMDReq_init_endfn(nnMDReq, arr_MD, MD_endfn, NULL, udata,
                dms_client_config->ioreq_timeout);
        if (ret) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN fail init MDReq for report free %u callerID %u\n",
                    nnMDReq->nnMDReq_comm.ReqID, callerReq->ReqID);
            break;
        }

        NNMDReq_ref_Req(nnMDReq);
        if (NNMDReq_submit_workerQ(nnMDReq)) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN fail add MDReq to workerQ for report free "
                    "%u (%llu %u)\n", nnMDReq->nnMDReq_comm.ReqID,
                    nnMDReq->nnMDReq_ioaddr.lbid_start,
                    nnMDReq->nnMDReq_ioaddr.lbid_len);
        }
        NNMDReq_deref_Req(nnMDReq);
    } while (0);

    return ret;
}
#endif

/*
 * _add_NNClientIP_lockless: add NNClient to hashtable with IP as key
 * @myClient: NNClient instance to be added
 * @bkID: which bucket for add
 */
static void _add_NNClientIP_lockless (discoC_NNClient_t *myClient, uint32_t bkID)
{
    struct list_head *lhead;

    lhead = &(nnClient_mgr.nnClientIP_hpool[bkID]);
    list_add_tail(&myClient->entry_nnc_IPAddrPool, lhead);
    atomic_inc(&nnClient_mgr.num_nnClient);
}

/*
 * _add_NNClient_NNCMgr: Implementation of add a NNClient to NNClient manager
 * @nm_nnC: name of NNClient
 * @ipaddr: namenode's ip address for NNClient to connect
 * @port  : namenode's port number for NNClient to connect
 *
 * Return: NNClient ID
 */
static discoC_NNClient_t *_add_NNClient_NNCMgr (int8_t *nm_nnC, uint32_t ipaddr, uint32_t port)
{
    int8_t ipaddr_str[IPV4ADDR_NM_LEN];
    discoC_NNClient_t *newClient, *myClient;
    uint32_t bkID;
    bool dofree;

    newClient = NNClient_create_client(nm_nnC, NNClientMgr_alloc_NNClientID(),
    		ipaddr, port);
    if (IS_ERR_OR_NULL(newClient)) {
        ccma_inet_aton(ipaddr, ipaddr_str, IPV4ADDR_NM_LEN);
        dms_printk(LOG_LVL_WARN, "DMSC WARN fail create NNClient %s:%u\n",
                ipaddr_str, port);
        return NULL;
    }

    dofree = false;
    bkID = _get_NNClientIP_bkID(ipaddr);

    write_lock(&nnClient_mgr.nnClient_plock);
    myClient = _find_NNClientIP_lockless(bkID, ipaddr, port);

    if (IS_ERR_OR_NULL(myClient)) {
        myClient = newClient;
        _add_NNClientIP_lockless(myClient, bkID);
    } else {
        dofree = true;
    }
    write_unlock(&nnClient_mgr.nnClient_plock);

    if (dofree) {
        NNClient_free_client(newClient);
    } else {
        NNClient_launch_client(myClient);
    }

    return myClient;
}

/*
 * NNClientMgr_add_NNClient: API of add a NNClient to NNClient manager
 * @nm_nnC: name of NNClient
 * @ipaddr: namenode's ip address for NNClient to connect
 * @port  : namenode's port number for NNClient to connect
 *
 * Return: NNClient ID
 */
uint32_t NNClientMgr_add_NNClient (int8_t *nm_nnC, uint32_t ipaddr, uint32_t port)
{
    discoC_NNClient_t *myClient;

    myClient = _add_NNClient_NNCMgr(nm_nnC, ipaddr, port);

    return myClient->nnClient_ID;
}

/*
 * _stop_all_nnClient: stop all running client in client pool
 */
static void _stop_all_nnClient (void)
{
    struct list_head buff_list;
    discoC_NNClient_t *cur, *next;
    struct list_head *lhead;
    int32_t i;

    INIT_LIST_HEAD(&buff_list);

    write_lock(&nnClient_mgr.nnClient_plock);

    for (i = 0; i < NNCLIENT_HBUCKET_SIZE; i++) {
        lhead = &(nnClient_mgr.nnClientIP_hpool[i]);
        list_for_each_entry_safe (cur, next, lhead, entry_nnc_IPAddrPool) {
            list_move_tail(&cur->entry_nnc_IPAddrPool, &buff_list);
            list_del(&cur->entry_nnc_IDPool);
            atomic_dec(&nnClient_mgr.num_nnClient);
        }
    }
    write_unlock(&nnClient_mgr.nnClient_plock);

    list_for_each_entry_safe (cur, next, &buff_list, entry_nnc_IPAddrPool) {
        if (atomic_read(&cur->volume_refCnt) != 0) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN NNClient be ref by volume\n");
        }
        list_del(&cur->entry_nnc_IPAddrPool);
        NNClient_stop_client(cur);
        NNClient_free_client(cur);
    }

    nnClient_mgr.def_nnClient = NULL;
}

/*
 * NNClientMgr_rel_manager: release NNClient manager related resource
 *
 * Return: manager release result
 *         0:init OK
 *        ~0:init fail
 */
int32_t NNClientMgr_rel_manager (void)
{
    int32_t ret;

    ret = 0;

    _stop_all_nnClient();
    NNMDReqPool_rel_pool(&nnClient_mgr.MData_ReqPool);

    rel_nnovw_manager();

    deregister_mem_pool(nnClient_mgr.NNMDReq_mpool_ID);
    return ret;
}

/*
 * _init_NNCMgr_mpool: initialize MData request memory pool
 * @myMgr: manager instance that has pool to initialize
 * @maxReqSize: max request can be hold in memory pool
 *
 * Return: memory pool initialization result
 *         0:init OK
 *        ~0:init fail
 */
static int32_t _init_NNCMgr_mpool (NNClient_MGR_t *myMgr, int32_t maxReqSize)
{
    int32_t ret;

    ret = 0;

    myMgr->NNMDReq_mpool_ID = register_mem_pool("NNMDRequest", maxReqSize,
            sizeof(NNMData_Req_t), true);
    if (myMgr->NNMDReq_mpool_ID < 0) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN fail register NNMD req mem pool %d\n",
                myMgr->NNMDReq_mpool_ID);
        return -ENOMEM;
    }
    dms_printk(LOG_LVL_INFO, "DMSC INFO NNMD Req manager get mem pool id %d\n",
            myMgr->NNMDReq_mpool_ID);

    return ret;
}

/*
 * _init_NNCMgr_clientPool: initialize 2 NNClient pool
 * @myMgr: manager instance that has pool to initialize
 */
static void _init_NNCMgr_clientPool (NNClient_MGR_t *myMgr)
{
    int32_t i;

    for (i = 0; i < NNCLIENT_HBUCKET_SIZE; i++) {
        INIT_LIST_HEAD(&(myMgr->nnClientID_hpool[i]));
        INIT_LIST_HEAD(&(myMgr->nnClientIP_hpool[i]));
    }

    rwlock_init(&myMgr->nnClient_plock);
    atomic_set(&myMgr->num_nnClient, 0);
}

/*
 * NNClientMgr_init_manager: initialize global manager nnClient_mgr
 *
 * Return: initialization result
 *         0: success
 *        ~0: init fail
 */
int32_t NNClientMgr_init_manager (void)
{
    int32_t ret, max_ReqSize;

    ret = 0;

    mnum_net_order = htonll(MAGIC_NUM);

    max_ReqSize = dms_client_config->max_nr_io_reqs*MAX_LBS_PER_RQ;
    ret = _init_NNCMgr_mpool(&nnClient_mgr, max_ReqSize);
    if (ret) {
        return ret;
    }

    _init_NNCMgr_clientPool(&nnClient_mgr);
    atomic_set(&nnClient_mgr.MDReq_ID_gen, 0);
    atomic_set(&nnClient_mgr.nnClient_ID_gen, 0);
    NNMDReqPool_init_pool(&nnClient_mgr.MData_ReqPool);

    nnClient_mgr.def_nnClient = _add_NNClient_NNCMgr("dms", dms_client_config->mds_ipaddr,
            dms_client_config->mds_ioport);
    if (IS_ERR_OR_NULL(nnClient_mgr.def_nnClient)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN fail create NNClient %d:%d\n",
                dms_client_config->mds_ipaddr, dms_client_config->mds_ioport);
        return -ENOMEM;
    }

//    ret = init_NNCWorker_manager();
//    if (ret) {
//        return ret;
//    }

    init_nnovw_manager();

    return ret;
}
