/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_DNC_Manager.h
 *
 */

#ifndef DISCOC_DNC_MANAGER_H_
#define DISCOC_DNC_MANAGER_H_

/*
 * DNUDResp_opcode_t is the ack operation code from datanode.
 * Range is from 100 to 299,
 * 100 to 199 is positive response
 * 200 to 299 is error response
 */
typedef enum {
    DNUDRESP_READ_PAYLOAD = 101,             //normal response of DNUDREQ_READ
    DNUDRESP_MEM_ACK = 102,                  //normal mem ack  of DNUDREQ_WRITE DNUDREQ_WRITE_NO_PAYLOAD ...
    DNUDRESP_DISK_ACK = 103,                 //normal disk ack  of DNUDREQ_WRITE DNUDREQ_WRITE_NO_PAYLOAD ...
    DNUDRESP_WRITEANDGETOLD_MEM_ACK = 104,   //useless now
    DNUDRESP_WRITEANDGETOLD_DISK_ACK = 105,  //useless now
    DNUDRESP_READ_NO_PAYLOAD = 110,          //normal response of DNUDREQ_READ_NO_PAYLOAD
#if (DN_PROTOCOL_VERSION == 2) && (CN_CRASH_RECOVERY == 1)
    DNUDRESP_CLEAN_LOG = 111,
#endif
} DNUDResp_opcode_t;

/*
 * DNUDResp_err_code_t is the ack error code from datanode.
 * Range is from 100 to 299,
 * 100 to 199 is positive response
 * 200 to 299 is error response
 */
typedef enum {
    DNUDRESP_ERR_BASE = 200,
    DNUDRESP_ERR_IOException,   //DN indicate that R/W with IO exception
    DNUDRESP_ERR_IOFailLUNOK,   //useless now
    DNUDRESP_ERR_LUNFail,       //useless now
    DNUDRESP_HBIDERR,           //DN indicate that Read but HBID error
    DN_ERR_DISCONNECTED         //CN try to send but connection lost
} DNUDResp_err_code_t;

/*
 * DNUDReq_opcode_t is the request operation code for CN sending request to DN
 */
typedef enum {
    DNUDREQ_WRITE = 3,                 //write payload request
    DNUDREQ_READ  = 4,                 //read payload request
    DNUDREQ_HEART_BEAT = 10,           //heartbeat request
    DNUDREQ_WRITE_NO_PAYLOAD     = 12, //CN will send write request without payload to DN, for performance evaluation
    DNUDREQ_READ_NO_PAYLOAD      = 13, //CN will send read request and DN don't need send payload back, for performance evaluation
    DNUDREQ_WRITE_NO_IO          = 14, //CN will send write request with payload to DN, DN don't need write to disk, for performance evaluation
    DNUDREQ_READ_NO_IO           = 15, //CN will send read request to DN, DN don't need read from disk but need send dummy payload back, for performance evaluation
    DNUDREQ_WRITE_NO_PAYLOAD_IO  = 16, //CN will send write request header only, DN don't need write dummy data to disk also, for performance evaluation
    DNUDREQ_READ_NO_PAYLOAD_IO   = 17, //CN will send read request, DN don't need read data from disk neither send dummy data to CN, for performance evaluation
#if (DN_PROTOCOL_VERSION == 2) && (CN_CRASH_RECOVERY == 1)
    DNUDREQ_CLEAN_LOG            = 19,
#endif
} DNUDReq_opcode_t;


#if (DN_PROTOCOL_VERSION == 2)
#if (CN_CRASH_RECOVERY == 1)
#define CLEAN_LOG_INTVL 30000 //30 seconds

extern uint64_t ts_cleanLog;
extern uint32_t ts_chk_bit;
#endif
#endif

#if (DN_PROTOCOL_VERSION == 2) && (CN_CRASH_RECOVERY == 1)
extern void update_cleanLog_ts(void);
#endif

typedef struct discoC_DNClient_manager {
    DNUDataReq_Pool_t UData_ReqPool; //Request pool that hold un-finished DN request
    discoDNC_worker_t dnClient;      //DN socket client worker

    int32_t DNUDReq_mpool_ID;        //memory pool ID for allocating memory for DN Request
    int32_t DNUDReadBuff_mpool_ID;   //memory pool ID for allocating read buffer for holding payload from datanode
    atomic_t DNUDReqID_counter;      //A request counter for assigning request ID for each DN Request
    atomic_t num_DNReq_waitResp;     //num of DNReq that still waiting resp
    uint32_t lastMax_DNUDRespID;     //last max caller id of DN Request we got from DN.
                                     //(DN will reply DN Request ID and caller ID of DN Request), this variable record caller ID
    atomic_t num_dnClient;           //number of DN socket sender (currently, only support 1 thread)
} DNClient_MGR_t;

extern DNClient_MGR_t dnClient_mgr;

extern int32_t DNClientMgr_get_ReadMemPoolID(void);
extern void DNClientMgr_set_last_DNUDataRespID(uint32_t dnReqID);
extern uint32_t DNClientMgr_gen_DNUDReqID(void);
#endif /* DISCOC_DNC_MANAGER_H_ */
