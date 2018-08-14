/*
 * Copyright (C) 2010 - 2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_NNMData_Request.h
 *
 * Define the data structure of metadata allocate/query requests
 *                              metadata commit requests
 *
 */

#ifndef DISCONN_CLIENT_DISCOC_NNMDATA_REQUEST_H_
#define DISCONN_CLIENT_DISCOC_NNMDATA_REQUEST_H_

#define MIN_NNMDREQ_SCHED_TIME  5 //5 seconds

typedef enum {
    NNREQ_E_InitDone,      //Caller init request done, try to transit Request to next state
    NNREQ_E_PeekWorkQ,     //socket sender thread peek a request, try to transit Request to next state
    NNREQ_E_SendDone,      //socket sender finish the sending, try to transit Request to next state
    NNREQ_E_FindReqOK,     //socket receiver find corresponding request for response and try to process response
    NNREQ_E_TimeoutFire,   //timeout fire thread try to transit Request to next state
    NNREQ_E_ConnBack,      //Connection back, housekeeping thread try to move to retry state
    NNREQ_E_RetryChkDone,  //housekeeping thread already finish the checking for resned and add to sender worker queue
} NNMDgetReq_FSM_TREvent_t;

typedef enum {
    NNREQ_A_START,
    NNREQ_A_NoAction,        //Caller can not do anything on request
    NNREQ_A_DO_ADD_WORKQ,    //Caller is allowed to add request to worker queue
    NNREQ_A_DO_SEND,         //Caller is allowed to send request
    NNREQ_A_DO_WAIT_RESP,    //Caller is allowed to wait response of request
    NNREQ_A_DO_PROCESS_RESP, //Caller is allowed to process response of request
    NNREQ_A_DO_TO_PROCESS,   //Caller is allowed to process response of request during timeout
    NNREQ_A_DO_RETRY_REQ,    //Caller is allowed to resend request
    NNREQ_A_END,
    NNREQ_A_UNKNOWN = NNREQ_A_END,
} NNMDgetReq_FSM_action_t;

typedef enum {
    NNREQ_S_START,
    NNREQ_S_INIT,       //Request is under initialize or init done
    NNREQ_S_WAIT_SEND,  //Request stay at socket sender's worker queue
    NNREQ_S_SENDING,    //Socket sender is processing Request
    NNREQ_S_WAIT_RESP,  //Socket sender always send request out and waiting NN's response
    NNREQ_S_PROCESS_RESP,  //Socket receiver is processing Request
    NNREQ_S_RETRY_REQ,      //housekeeping thread is check whether request need retry
    NNREQ_S_TIMEOUT_FIRE,   //timeout fire and timeout thread is checking request
    NNREQ_S_END,
    NNREQ_S_UNKNOWN = NNREQ_S_END,
} NNMDgetReq_FSM_state_t;

typedef enum {
    NNCIREQ_E_InitDone,    //Caller init request done, try to transit Request to next state
    NNCIREQ_E_PeekWorkQ,   //socket sender thread peek a request, try to transit Request to next state
    NNCIREQ_E_SendDone,    //socket sender finish the sending, try to transit Request to next state
    NNCIREQ_E_FindReqOK,   //socket receiver find corresponding request for response and try to process response
    NNCIREQ_E_ConnBack,    //Connection back, housekeeping thread try to move to retry state
    NNCIREQ_E_RetryChkDone, //housekeeping thread already finish the checking for resned and add to sender worker queue
} NNMDciReq_FSM_TREvent_t;

typedef enum {
    NNCIREQ_A_START,
    NNCIREQ_A_NoAction,     //Caller can not do anything on request
    NNCIREQ_A_DO_ADD_WORKQ, //Caller is allowed to add request to worker queue
    NNCIREQ_A_DO_SEND,      //Caller is allowed to send request
    NNCIREQ_A_DO_WAIT_RESP, //Caller is allowed to wait response of request
    NNCIREQ_A_DO_PROCESS_RESP, //Caller is allowed to process response of request
    NNCIREQ_A_DO_RETRY_REQ,    //Caller is allowed to resend request
    NNCIREQ_A_END,
    NNCIREQ_A_UNKNOWN = NNCIREQ_A_END,
} NNMDciReq_FSM_action_t;

typedef enum {
    NNCIREQ_S_START,
    NNCIREQ_S_INIT,      //Request is under initialize or init done
    NNCIREQ_S_WAIT_SEND, //Request stay at socket sender's worker queue
    NNCIREQ_S_SENDING,   //Socket sender is processing Request
    NNCIREQ_S_WAIT_RESP, //Socket sender always send request out and waiting NN's response
    NNCIREQ_S_PROCESS_RESP, //Socket receiver is processing Request
    NNCIREQ_S_RETRY_REQ,    //housekeeping thread is check whether request need retry
    NNCIREQ_S_END,
    NNCIREQ_S_UNKNOWN = NNCIREQ_S_END,
} NNMDciReq_FSM_state_t;

typedef struct discoC_NNMD_request_fsm {
    rwlock_t st_lock;                 //state lock to protect state change in atomic
    NNMDgetReq_FSM_state_t st_nnReq;  //state relate to acquire metadata request
    NNMDciReq_FSM_state_t st_nnCIReq; //state relate to commit metadata request
} NNMDReq_fsm_t;

typedef enum {
    NNREQ_FEA_CACHE_HIT,  //MDReq has metadata from local cache
    NNREQ_FEA_HBID_ERR,   //With this feature, MDReq need get metadata by operation: REQ_NN_QUERY_TRUE_MDATA_READIO
    NNREQ_FEA_IN_WORKQ,   //Request stay in socket sender worker queue
    NNREQ_FEA_IN_REQPOOL, //Request stay in metadata request pool
    NNREQ_FEA_OVW_STAT,   //MDReq ovw state: 0: send opcode with allocate metadata, 1: send opcode with query metadata for write
    NNREQ_FEA_SCHED_TIMER,
    NNREQ_FEA_MDCACHE_ON,
} NNMDReq_fea_t;

typedef struct discoC_NNMData_request {
    ReqCommon_t nnMDReq_comm;          //NNMDRequest also share same request common data members
    struct list_head entry_nnReqPool;  //Entry to add to NNReq pool
    struct list_head entry_nnWkerPool; //Entry to add to NNReq worker pool
    struct list_head entry_retryPool;  //Entry to add to retry list

    UserIO_addr_t nnMDReq_ioaddr;      //user namespace information: which namenode/volume ID/start LBA/LBA length

    metadata_t *MetaData;  //metadata pointer to hold metadata structure for holding metadat from local cache or from namenode

    uint64_t t_MDReq_last_submitT; //time in jiffies, when caller submit requests, calculate curr - last submit time
    int32_t t_MDReq_TO_msec;       //how many time in milli-seconds that user will wait
    int32_t t_MDReq_rTO_msec;      //retry nn request will stop timer and re-schedule again, we need know remaining time to schedule
    int32_t t_MDCIReq_rTO_msec;    //a fix time interval in milli-secs to show info that ci request not yet finished
    worker_t MDReq_retry_work;     //A work structure for submitting to linux workqueue

    NNMDReq_fsm_t nnReq_stat;      //NN Metadata request state machine
    nnMDReq_end_fn_t *nnReq_endfn; //end function to hold callback function passed from caller
    nnMDReq_setovw_fn_t *nnReq_setOVW_fn;  //overwritten function to hold set ovw function passed from caller
    void *usr_pdata;               //private data passed from caller for nnReq_endfn

    uint64_t t_txMDReport;     //time in jiffies to send nn report to namenode
    uint64_t t_rxMDReportAck;  //time in jiffies to rcv nn response from namenode

    uint64_t ts_process_MDReport;  //start time in jiffies start to process MDReport, jiffies
    uint64_t te_procDone_MDReport; //end time in jiffies start to process MDReport, jiffies

    discoC_NNClient_t *nnSrv_client;  //which NN this metadata should be sent to

    uint16_t volReplica;              //number of volume replica for setting metadata status
} NNMData_Req_t;

extern void NNMDReq_set_fea(NNMData_Req_t *myReq, uint32_t fea_bit);
extern int32_t NNMDReq_test_clear_fea(NNMData_Req_t *myReq, uint32_t fea_bit);
extern void NNMDReq_clear_fea(NNMData_Req_t *myReq, uint32_t fea_bit);

extern bool NNMDReq_MData_cacheHit(NNMData_Req_t *myReq);
extern bool NNMDReq_MData_hbidErr(NNMData_Req_t *myReq);
extern bool NNMDReq_MData_cacheEnable(NNMData_Req_t *myReq);
extern bool NNMDReq_check_acquire(NNMData_Req_t *myReq);
extern bool NNMDReq_check_alloc(NNMData_Req_t *myReq);

extern void NNMDReq_cancel_timer(NNMData_Req_t *myReq);

extern void NNMDReq_ref_Req(NNMData_Req_t *myReq);
extern void NNMDReq_deref_Req(NNMData_Req_t *myReq);

extern int32_t NNMDReq_submit_workerQ(NNMData_Req_t *myReq);
extern NNMData_Req_t *NNMDReq_find_ReqPool(uint32_t nnReqID);
extern int32_t NNMDReq_rm_ReqPool(NNMData_Req_t *nnMDReq);

extern bool NNMDReq_report_retryable(NNMData_Req_t *myReq);
extern bool NNMDReq_acquire_retryable(NNMData_Req_t *myReq);
extern void NNMDReq_retry_Req(NNMData_Req_t *myReq);

extern int32_t NNMDReq_init_Req(NNMData_Req_t *myReq, UserIO_addr_t *ioaddr,
        ReqCommon_t *callerReq, uint16_t volReplica,
        bool mdch_stat, bool ovw_stat,
        bool hbid_err, bool is_mdcache_on, bool do_MDReport);
extern int32_t NNMDReq_init_endfn(NNMData_Req_t *myReq, metadata_t *MData,
        nnMDReq_end_fn_t *endfn, nnMDReq_setovw_fn_t *setOVW_fn,
        void *pdata, int32_t t_dur);
extern NNMData_Req_t *NNMDReq_alloc_Req(void);
extern int32_t NNMDReq_free_Req(NNMData_Req_t *myReq);

#endif /* DISCONN_CLIENT_DISCOC_NNMDATA_REQUEST_H_ */
