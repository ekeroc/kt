/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * payload_request.h
 *
 */

#ifndef PAYLOAD_MANAGER_PAYLOAD_REQUEST_H_
#define PAYLOAD_MANAGER_PAYLOAD_REQUEST_H_

typedef enum {
    PLREQ_FEA_IN_REQPOOL,   //whether payload request in request pool
    PLREQ_FEA_TXRX_DATA,    //whether payload request need tx/rx data for creating DNRequest
    PLREA_FEA_RW_DATA,      //whether payload request need r/w data from data disk for creating DNRequest
    PLREQ_FEA_TIMER_ON,     //whether payload request schedule timer
    PLREA_FEA_TIMER_SetCtrl,//whether a thread gain the right to schedule timer
} PLReq_fea_t; //fea: feature

typedef struct discoC_payload_request {
    ReqCommon_t plReq_comm;             //payload request also share same request common data members
    struct list_head entry_plReqPool;   //entry to add to payload request pool
    struct list_head entry_callerReqPool; //entry to add to caller's callee request pool

    metaD_item_t *mdata_item;  //metadata item that contain necessary information for payload manager create DN

    uint64_t payload_offset; //payload_offset describe the offset with io req start unit: bytes
    PLReq_fsm_t plReq_stat;  //payload request state machine

    spinlock_t DNUDReq_glock;              //a locking for protect arr_DNUDReq
    uint32_t arr_DNUDReq[MAX_NUM_REPLICA]; //Record DNReq id for cancel request
    atomic_t num_DNUDReq;                  //number of DNUData Request in arr_DNUDReq
#ifdef DISCO_ERC_SUPPORT
    uint32_t *arr_ERCDNUDReq;
    uint32_t num_ERCRebuild_DNUDReq;
#endif

    int32_t last_readReplica_idx; //last replica idx to generate DN
    int32_t last_readDNbk_idx;    //last bucket index to hold request DNReq request ID

    atomic_t num_readFail_DN;     /* number of read IO error */

    int8_t *ref_fp_map;    //point to fp_map of io requests (first element of fp_map)
    int8_t *ref_fp_cache;  //point to fp_cache of io requests (first element of fp_cache)

    uint64_t t_ciCaller;     //timestamp in jiffies that ci to user
    uint64_t t_submit_PLReq; //timestamp in jiffies that submit payload request
    int32_t t_callerWait_PLReq; //time in seconds that caller can wait
    int32_t rTO_chk_cnt;    //remain timeout fire count
    int32_t TO_wait_period; //time in seconds for setting timer

    worker_t PLReq_retry_work; //A work structure for adding to linux work queue for timeout mechanism

    PLReq_func_t plReq_func;
    void *user_data;
} payload_req_t;

extern int32_t PLReq_allocBK_DNUDReq(payload_req_t *pReq);
//extern void PLReq_add_DNUDReq(payload_req_t *pReq, uint32_t dnReqID, int32_t bk_idx);
extern void PLReq_rm_DNUDReq(payload_req_t *pReq, int32_t bk_idx);
#ifdef DISCO_ERC_SUPPORT
extern void PLReq_rm_ERCDNUDReq(payload_req_t *pReq, int32_t bk_idx);
#endif

extern bool PLReqFea_send_payload(payload_req_t *pReq);
extern bool PLReqFea_rw_payload(payload_req_t *pReq);

extern void PLReq_ref_Req(payload_req_t *pReq);
extern void PLReq_deref_Req(payload_req_t *pReq);
extern payload_req_t *PLReq_alloc_Req(int32_t plMPoll_ID);
extern void PLReq_free_Req(payload_req_t *preq, int32_t plMPoll_ID);
extern void PLReq_init_Req(payload_req_t *pReq, metaD_item_t *mdItem,
        uint16_t rw_opcode, uint64_t payload_off,
        ReqCommon_t *callerReq,
        uint8_t *fp_map, uint8_t *fp_cache, bool send_data, bool rw_data,
        int32_t t_callerWait, void *user_data, PLReq_func_t *pReq_func);

extern int32_t PLReq_schedule_timer(payload_req_t *myReq);
extern void PLReq_cancel_timer(payload_req_t *myReq);
extern int32_t PLReq_add_ReqPool(payload_req_t *pReq);
extern void PLReq_rm_ReqPool(payload_req_t *pReq);
extern void PLReq_cancel_DNUDReq(payload_req_t *pReq, int32_t bk_skip);

#ifdef DISCO_ERC_SUPPORT
extern void PLReq_cancel_ERCDNUDReq(payload_req_t *pReq, int32_t bk_skip);
#endif
#endif /* PAYLOAD_MANAGER_PAYLOAD_REQUEST_H_ */
