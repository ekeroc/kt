/*
 * Copyright (C) 2010 - 2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_NNClient.c
 *
 * Provide a set of API to create a namenode client instance
 */
#include <linux/version.h>
#include <linux/blkdev.h>
#include <net/sock.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/dms_kernel_version.h"
#include "../common/discoC_mem_manager.h"
#include "../metadata_manager/metadata_manager.h"
#include "../metadata_manager/metadata_cache.h"
#include "../config/dmsc_config.h"
#include "../common/thread_manager.h"
#include "../connection_manager/conn_manager_export.h"
#include "discoC_NNC_Manager_api.h"
#include "discoC_NNC_worker.h"
#include "../flowcontrol/FlowControl.h"
#include "discoC_NNClient.h"
#include "discoC_NNMData_Request.h"
#include "discoC_NNC_workerfunc_reportMD.h"
#include "discoC_NN_protocol.h"
#include "discoC_NNAck_acquireMD.h"
#include "discoC_NNAck_reportMD.h"
#include "discoC_NNMDReqPool.h"
#include "../housekeeping.h"
#include "discoC_NNC_Manager.h"

/*
 * NNClient_set_last_rcvReqID: set max request ID that received by NNClient
 * @myClient: which NNClient got this ID
 * @lastID: lastest max NNRequest ID
 */
void NNClient_set_last_rcvReqID (discoC_NNClient_t *myClient, uint32_t lastID)
{
    myClient->nnReq_last_rcvID = lastID;
}

/*
 * NNClient_dec_numReq_NoResp: decrease number of requests that not receive response from namenode
 * @myClient: which NNClient didn't got response from namenode
 */
void NNClient_dec_numReq_NoResp (discoC_NNClient_t *myClient)
{
    atomic_dec(&myClient->num_nnReq_noResp);
}

/*
 * NNClient_create_client: allocate and initialize a NNClient instance
 * @nnSrvNM: name of namenode
 * @nnCID: NNClient instance ID
 * @nnSrvIP: namenode IP for NNClient connect to
 * @nnSrvPort: namenode port for NNClient connect to
 *
 * Return: created NNClient
 */
discoC_NNClient_t *NNClient_create_client (int8_t *nnSrvNM, uint32_t nnCID,
        uint32_t nnSrvIP, uint32_t nnSrvPort)
{
    discoC_NNClient_t *myClient;
    int32_t nm_len;

    myClient = discoC_mem_alloc(sizeof(discoC_NNClient_t), GFP_NOWAIT | GFP_ATOMIC);
    if (IS_ERR_OR_NULL(myClient)) {
        return NULL;
    }

    atomic_set(&myClient->nnClient_stat, NNC_STAT_INIT);
    myClient->nnClient_ID = nnCID;
    myClient->nnSrv_IPAddr = nnSrvIP;
    myClient->nnSrv_port = nnSrvPort;
    INIT_LIST_HEAD(&myClient->entry_nnc_IDPool);
    INIT_LIST_HEAD(&myClient->entry_nnc_IPAddrPool);

    nm_len = strlen(nnSrvNM) > MAX_LEN_NNSRV_NAME ?
            MAX_LEN_NNSRV_NAME : strlen(nnSrvNM);
    strncpy(myClient->nnSrv_name, nnSrvNM, nm_len);

    ccma_inet_aton(nnSrvIP, myClient->nnSrv_IPAddr_str, IPV4ADDR_NM_LEN);

    atomic_set(&myClient->num_nnReq_noResp, 0);

    myClient->nnMDCM_ReqFC.nn_fc_start_t = 0;
    myClient->nnMDCM_ReqFC.fc_t_slot = DEFAULT_FC_TIME_SLOTS;

    myClient->MDRX_buffer = discoC_mem_alloc(sizeof(int8_t)*MD_RX_BUFFER_SIZE,
            GFP_NOWAIT | GFP_ATOMIC);
    if (IS_ERR_OR_NULL(myClient->MDRX_buffer)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN fail alloc mem buffer MD RX\n");
        return NULL;
    }

    myClient->nn_ci_tx_buffer = discoC_mem_alloc(sizeof(int8_t)*MD_CI_BUFFER_SIZE,
            GFP_NOWAIT | GFP_ATOMIC);
    if (IS_ERR_OR_NULL(myClient->nn_ci_tx_buffer)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN fail alloc mem buffer MDReport TX Buffer\n");
        return NULL;
    }

    sprintf(myClient->nm_MDReq_retryWQ, "%s_%u", NN_MDR_RETRY_WQ_NM, nnCID);
    myClient->nnMDReq_retry_workq = create_workqueue(myClient->nm_MDReq_retryWQ);
    if (IS_ERR_OR_NULL(myClient->nnMDReq_retry_workq)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN fail create NNMData Request retry workq\n");
        DMS_WARN_ON(true);
    }

#if (DN_PROTOCOL_VERSION == 2) && (CN_CRASH_RECOVERY == 1)
    atomic_set(&IOR_fWker_mgr.nn_commit_wait_resp, 0);
#endif

    return myClient;
}

/*
 * NNClient_free_client: free a NNClient instance
 * @myClient: target client to be free
 *
 */
void NNClient_free_client (discoC_NNClient_t *myClient)
{
    if (IS_ERR_OR_NULL(myClient->MDRX_buffer) == false) {
        discoC_mem_free(myClient->MDRX_buffer);
    }

    if (IS_ERR_OR_NULL(myClient->nn_ci_tx_buffer) == false) {
        discoC_mem_free(myClient->nn_ci_tx_buffer);
    }
    discoC_mem_free(myClient);
}

/*
 * _NN_connected_post_fn: post process function for registering to connection manager.
 *     when connection broken and re-connect, connection manager will call this function
 *     to create a socket receiver thread and generate a task let housekeeping
 *     call function of NNClient manager to re-send all requests
 * @conn: connection that back to connect state
 * @do_req_retry: whether retry request
 *
 * Return: run success or fail
 *     0: run success
 *    ~0: run fail
 */
static int32_t _NN_connected_post_fn (discoC_conn_t *conn, bool do_req_retry)
{
    int32_t ret;

    ret = 0;

    if (unlikely(discoC_MODE_SelfTest ==
            dms_client_config->clientnode_running_mode)) {
        return ret;
    }

    if (unlikely(discoC_MODE_SelfTest_NN_Only ==
            dms_client_config->clientnode_running_mode)) {
        return ret;
    }

    conn->skrx_thread_ds = create_sk_rx_thread("dms_nn_md_rcv", conn,
            NNClient_sockrcv_thdfn);
    if (IS_ERR_OR_NULL(conn->skrx_thread_ds)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN kthread dms_nn_md_rcv creation fail\n");
        DMS_WARN_ON(true);
    }

    if (do_req_retry) {
        add_hk_task(TASK_NN_CONN_BACK_REQ_RETRY, (uint64_t *)conn);
    }

    return ret;
}

/*
 * NNClient_launch_client: call thread manager to create all worker in NNClient
 *     and start to service
 * @myClient: target client to be run
 *
 * Return: run success or fail
 *     0: stop success
 *    ~0: stop fail
 */
int32_t NNClient_launch_client (discoC_NNClient_t *myClient)
{
    int32_t ret;

    ret = 0;

    atomic_set(&myClient->num_worker, 0);

    NNCWker_init_worker(&myClient->nnMDCH_worker, "dms_nn_ch_wker",
            NNClient_send_getMDReq_cachehit, (void *)myClient);
    NNCWker_init_worker(&myClient->nnMDCM_worker, "dms_nn_cm_wker",
            NNClient_send_MDReq, (void *)myClient);

    if (dms_client_config->NNSimulator_on == false) {
        myClient->nnSrv_conn = connMgr_create_conn(myClient->nnSrv_IPAddr,
                myClient->nnSrv_port, _NN_connected_post_fn, NNProto_gen_HBPacket,
                CONN_IMPL_SOCK);
    } else {
        myClient->nnSrv_conn = connMgr_create_conn(myClient->nnSrv_IPAddr,
                myClient->nnSrv_port, _NN_connected_post_fn, NNProto_gen_HBPacket,
                CONN_IMPL_NN_SIMULATOR);
    }

    if (unlikely(IS_ERR_OR_NULL(myClient->nnSrv_conn))) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN Fail to get connection for %s:%d\n",
                myClient->nnSrv_IPAddr_str, myClient->nnSrv_port);
        DMS_WARN_ON(true);
        return -ENETUNREACH;
    }

    myClient->nnSrv_resource =
            add_NN_Resource(myClient->nnSrv_IPAddr, myClient->nnSrv_port);
    atomic_set(&myClient->nnClient_stat, NNC_STAT_RUNNING);

    return ret;
}

/*
 * NNClient_stop_client: stop a namenode client instance and free related data structure
 * @myClient: target client to be stop and free
 *
 * Return: stop success or fail
 *     0: stop success
 *    ~0: stop fail
 */
int32_t NNClient_stop_client (discoC_NNClient_t *myClient)
{
    int32_t ret, count;

    ret = 0;

    atomic_set(&myClient->nnClient_stat, NNC_STAT_STOPPING);
    if (NNCWker_stop_worker(&myClient->nnMDCH_worker)) {
        dms_printk(LOG_LVL_WARN, "fail to stop NN CH Request worker\n");
        DMS_WARN_ON(true);
    }

    if (NNCWker_stop_worker(&myClient->nnMDCM_worker)) {
        dms_printk(LOG_LVL_WARN, "fail to stop NN CM Request worker\n");
        DMS_WARN_ON(true);
    }

    count = 0;
    while (atomic_read(&myClient->num_worker) != 0) {
        msleep(100);
        count++;

        if (count == 0 || count % 10 != 0) {
            continue;
        }

        dms_printk(LOG_LVL_WARN,
                "DMSC WARN fail stop DNReq worker %d thread running "
                "(wait count %d)\n",
                atomic_read(&myClient->num_worker), count);
    }

    if (!IS_ERR_OR_NULL(myClient->nnMDReq_retry_workq)) {
        flush_workqueue(myClient->nnMDReq_retry_workq);
        destroy_workqueue(myClient->nnMDReq_retry_workq);
    }

    atomic_set(&myClient->nnClient_stat, NNC_STAT_STOP);
    return ret;
}
