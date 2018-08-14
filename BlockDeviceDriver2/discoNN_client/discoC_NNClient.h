/*
 * Copyright (C) 2010 - 2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_NNClient.h
 *
 */
#ifndef DISCONN_CLIENT_DISCOC_NNCLIENT_H_
#define DISCONN_CLIENT_DISCOC_NNCLIENT_H_

#define MAX_LEN_NNSRV_NAME  64
#define NN_MDR_RETRY_WQ_NM  "NNMDReq_retry_workq"

#define BASIC_NN_FC_WAIT        500
#define DEFAULT_FC_TIME_SLOTS   1000L

typedef enum {
    NN_RESP_METADATA_REQUEST,
    NN_RESP_METADATA_COMMIT,
#ifdef DISCO_PREALLOC_SUPPORT
    NN_RESP_FS_PREALLOCATION,
    NN_RESP_FS_REPORT_MDATA,
    NN_RESP_FS_REPORT_FREE,
#endif
} NNMDResp_opcode_t;

typedef enum
{
    NN_FC_TOGO,        /* no flow control is underway */
    NN_FC_CONN_LOST,     /* f.c. due to lost conn with nn */
    NN_FC_MAX_REQS,      /* f.c. due to reach max nn requests */
    NN_FC_REQ_CTL,        /* f.c. due to reach nn ctl */
} nn_fc_code_t;

typedef enum {
    WAKEUP_CM_WORKER,
    WAKEUP_CH_WORKER,
    WAKEUP_ALL
} wakeup_type_t;

typedef struct discoC_NN_Request_flowCtrl {
    uint64_t nn_fc_start_t;
    uint64_t fc_t_slot; //milli-second unit
} NNReq_flowCtrl_t;

typedef enum {
    NNC_STAT_INIT,
    NNC_STAT_RUNNING,
    NNC_STAT_STOPPING,
    NNC_STAT_STOP,
} NNClient_state_t;

#define MD_RX_BUFFER_SIZE 8192
#define MD_CI_BUFFER_SIZE 8192

typedef struct discoC_namenode_client {
    atomic_t nnClient_stat;    //NNClient state machine - NNClient_state_t
    uint32_t nnClient_ID;                       //ID assigned by NNC manager
    int8_t nnSrv_name[MAX_LEN_NNSRV_NAME];      //name of namenode server
    int8_t nnSrv_IPAddr_str[IPV4ADDR_NM_LEN];   //ip address of namenode server in string format
    uint32_t nnSrv_IPAddr;                      //ip address of namenode server in integer format
    uint32_t nnSrv_port;                        //port of namenode server
    discoC_conn_t *nnSrv_conn;                  //CN-NN connection instance

    struct list_head entry_nnc_IDPool;          //entry to be added to manager's pool, use ID as key
    struct list_head entry_nnc_IPAddrPool;      //entry to be added to manager's pool, use NNIP as key

    discoNNC_worker_t nnMDCH_worker;  //cache hit worker for handle metadata request has metadata from local cache
    discoNNC_worker_t nnMDCM_worker;  //cache miss worker for handle acquire metadata and report metadata
    atomic_t num_worker;  //number of running worker

    NNReq_flowCtrl_t nnMDCM_ReqFC;  //flow control related data structure

    atomic_t num_nnReq_noResp; //number of on-fly metadata request (sent and not yet got response)
    uint32_t nnReq_last_rcvID; //last receiver metadata request from namenode

    int8_t *MDRX_buffer;     //a prealloc rx buffer for receiving namenode response
    int8_t *nn_ci_tx_buffer; //a prealloc tx buffer for holding report metadata packet

#if (DN_PROTOCOL_VERSION == 2) && (CN_CRASH_RECOVERY == 1)
    atomic_t nn_commit_wait_resp;
#endif

    int8_t nm_MDReq_retryWQ[MAX_LEN_NNSRV_NAME]; //name of linux workqueue for timeout handling
    struct workqueue_struct *nnMDReq_retry_workq; //linux workqueue for timeout handling
    dms_res_item_t *nnSrv_resource;      //For monitor resources - Flow control and performance monitor
    atomic_t volume_refCnt;              //How many volume hoop on this client
} discoC_NNClient_t;

extern void NNClient_set_last_rcvReqID(discoC_NNClient_t *myClient, uint32_t lastID);
extern void NNClient_dec_numReq_NoResp(discoC_NNClient_t *myClient);

extern discoC_NNClient_t *NNClient_create_client(int8_t *nnSrvNM, uint32_t nnCID,
        uint32_t nnSrvIP, uint32_t nnSrvPort);
extern void NNClient_free_client(discoC_NNClient_t *myClient);
extern int32_t NNClient_launch_client(discoC_NNClient_t *myClient);
extern int32_t NNClient_stop_client(discoC_NNClient_t *myClient);

/*
 * Implemented at discoC_NNC_workerfunc.c
 */
extern void NNClient_wakeup_worker(wakeup_type_t wake_type);
extern void NNClient_send_MDReq(discoC_NNClient_t *myClient);
extern void NNClient_send_getMDReq_cachehit(discoC_NNClient_t *myClient);
extern int NNClient_sockrcv_thdfn(void *thread_data);
//extern int32_t init_NNCWorker_manager(void);

#endif /* DISCONN_CLIENT_DISCOC_NNCLIENT_H_ */
