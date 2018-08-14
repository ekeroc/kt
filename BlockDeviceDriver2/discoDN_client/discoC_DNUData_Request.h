/*
 * Copyright (C) 2010-2020 by Division F / ICL
 * Industrial Technology Research Institute
 *
 * discoC_DNUData_Request.h
 *
 * Define data structure and related API of datanode user data read write request
 */

#ifndef DISCODN_CLIENT_DISCOC_DNUDATA_REQUEST_H_
#define DISCODN_CLIENT_DISCOC_DNUDATA_REQUEST_H_

#include <linux/uio.h>

typedef enum {
    DNREQ_E_InitDone,         //Caller init request done, try to transit Request to next state
    DNREQ_E_PeekWorkQ,        //Worker thread peek a request, try to transit Request to next state
    DNREQ_E_SendDone,         //Worker thread finish the sending, try to transit Request to next state
    DNREQ_E_FindReqOK_MemAck, //Ack worker find corresponding request for memory ack from DN (write Req)
    DNREQ_E_FindReqOK,        //Ack worker find corresponding request for disk ack (write) and read ack
    DNREQ_E_ConnBack,         //Connection back, housekeeping try to move to retry state
    DNREQ_E_RetryChkDone,     //housekeeping already finish the checking for resned and add to sender worker queue
    DNREQ_E_Cancel,           //Caller want to perform cancel by send a event to request
} DNUDREQ_FSM_TREvent_t;

typedef enum {
    DNREQ_A_START,
    DNREQ_A_NoAction,                //Caller can not do anything on request
    DNREQ_A_DO_ADD_WORKQ,            //Caller is allowed to add request to worker queue
    DNREQ_A_DO_SEND,                 //Caller is allowed to send request
    DNREQ_A_DO_WAIT_RESP,            //Caller is allowed to wait response of request
    DNREQ_A_DO_PROCESS_RESP,         //Caller is allowed to process response of request
    DNREQ_A_DO_RETRY_REQ,            //Caller is allowed to resend request
    DNREQ_A_DO_CANCEL_REQ,           //Caller is allowed to cancel request
    DNREQ_A_DO_RETRY_CANCEL_REQ,     //Caller should keep send TREvent and try to cacnel request
    DNREQ_A_END,
    DNREQ_A_UNKNOWN = DNREQ_A_END,
} DNUDREQ_FSM_action_t;

typedef enum {
    DNREQ_S_START,
    DNREQ_S_INIT,                     //Request is under initialize or init done
    DNREQ_S_WAIT_SEND,                //Request stay at socket sender's worker queue
    DNREQ_S_SENDING,                  //Socket sender is processing Request
    DNREQ_S_WAIT_RESP,                //Socket sender always send request out and waiting DN's response
    DNREQ_S_PROCESS_RESP,             //Ack worker is processing Request
    DNREQ_S_RETRY_REQ,                //housekeeping thread is check whether request need retry
    DNREQ_S_CANCEL_REQ,               //Caller (payload manager) is canceling request
    DNREQ_S_END,
    DNREQ_S_UNKNOWN = DNREQ_S_END,
} DNUDREQ_FSM_state_t;

typedef struct discoC_DNUData_Request_fsm {
    rwlock_t st_lock;             //lock to protect state
    DNUDREQ_FSM_state_t st_dnReq; //request state
} DNUDReq_fsm_t;

typedef enum {
    DNREQ_FEA_IN_WORKQ,     //Request stay in worker queue
    DNREQ_FEA_IN_REQPOOL,   //Request stay in request pool
    DNREQ_FEA_DO_RW_IO,     //Request is allow to R/W from/to disk
    DNREQ_FEA_SEND_PAYLOAD, //Request is allowed to send payload to DN or DN is allow to send payload to CN
} DNMDReq_fea_t; //fea: feature

typedef struct discoC_DNUData_request {
    ReqCommon_t dnReq_comm;           //DNRequest also share same request common data members
    DNUDReq_fsm_t dnReq_stat;         //DNRequest's fsm

    struct list_head entry_dnReqPool; //Entry to add to DNReq pool
    struct list_head entry_dnWkerQ;   //Entry to add to DN Socket Sender Worker Queue

    metaD_item_t *mdata_item; //metadata information for DNClient access right location on right DN
    uint16_t dnLoc_idx;       //which DN in mdata_item DNClient should access
    uint32_t ipaddr;          //target DN's ipaddr that request want to access
    uint16_t port;            //target DN's port that request want to access
    uint16_t num_hbids;       //number of hbids in arr_hbids
    uint64_t arr_hbids[MAX_LBS_PER_IOR]; //hbids array to hold HBID for datanode
    uint16_t num_phyloc;      //number of rbid-len-offset in phyLoc_rlo
    uint64_t phyLoc_rlo[MAX_LBS_PER_IOR]; //rlo arry to hold rlo informatino for R/W physical location on datanode

    int32_t num_UDataSeg; //number of data segment in UDataSeg
    struct iovec UDataSeg[MAX_LBS_PER_IOR]; //Caller use non-continuous memory space to hold user data
                                            //use iovec array to hold the memory pointer and send datanode
    uint64_t dn_payload_offset;  //payload offset for data buffer in io_request (Unit: bytes)

    /*
     * ref_fp_map and ref_fp_cache will reference to payload request's fp_map and fp_cache
     * payload request will ref to io_request
     * NOTE: all these two pointer will ref to start address of io_request.
     *       To get right value of DNRequest should handle. Need reference dn_payload_offset
     */
    int8_t *ref_fp_map;  //fp map use bit to indicate whether fp cache contains fingerprint of specific 4K data
    int8_t *ref_fp_cache; //fp cache for storing fingerprint of particular 4K data

    DNUDReq_endfn_t *dnReq_endfn; //Callback function provided by caller. When DNClient finish DNRequest processing, call this to notify caller.
    void *usr_pdata;              //Argument passed by caller as a argument of dnReq_endfn

    int32_t caller_bkidx; //which bucket hold DN request ID, why we need this?
                          //we need a API: cancel DN Request and require pass DN request ID to this module
                          //But in normal case, no need cancel (since all DN request end properly),
                          //with this bucket idx, caller dereference when request done, prevent search in bucket
    uint64_t t_rcvReqAck_mem;
} DNUData_Req_t;

extern int32_t DNUDReq_test_set_fea(DNUData_Req_t *myReq, uint32_t fea_bit);
extern bool DNUDReq_check_feature(DNUData_Req_t *myReq, uint32_t fea_bit);
extern int32_t DNUDReq_test_clear_fea(DNUData_Req_t *myReq, uint32_t fea_bit);
extern void DNUDReq_ref_Req(DNUData_Req_t *myReq);
extern void DNUDReq_deref_Req(DNUData_Req_t *myReq);
extern DNUData_Req_t *DNUDReq_alloc_Req(void);

extern void DNUDReq_init_Req(DNUData_Req_t *myReq, uint16_t opcode,
        ReqCommon_t *callerReq, metaD_item_t *md_item, uint16_t dnLoc_idx);
extern int32_t init_DNUDReq_Read(DNUData_Req_t *myReq, uint64_t data_offset, DNUDReq_endfn_t *endfn, void *pdata,
        bool send_data, bool rw_data);
extern int32_t init_DNUDReq_Write(DNUData_Req_t *myReq, data_mem_chunks_t *data_buf,
        uint64_t payload_offset, DNUDReq_endfn_t *endfn, void *pdata,
        uint8_t *fp_map, uint8_t *fp_cache, bool send_data, bool rw_data);
extern void DNUDReq_free_Req(DNUData_Req_t *dn_req);

extern bool DNUDReq_check_retryable(DNUData_Req_t *myReq);
extern void DNUDReq_retry_Req(DNUData_Req_t *myReq);
extern void DNUDReq_cancel_Req(uint32_t dnReqID, uint32_t callerID);

extern int32_t DNUDReq_get_dnInfo(uint32_t dnReqID, uint32_t callerID, uint32_t *ipv4addr, uint32_t *port);
extern DNUData_Req_t *find_DNUDReq_ReqPool(uint32_t dnReqID, uint16_t dnAck);
#endif /* DISCODN_CLIENT_DISCOC_DNUDATA_REQUEST_H_ */
