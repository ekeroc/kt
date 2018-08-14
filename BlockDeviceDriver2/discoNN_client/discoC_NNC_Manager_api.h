/*
 * Copyright (C) 2010 - 2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_NNC_Manager_api.h
 *
 * This header file define data structure and provide API for non discoNN_client components
 */

#ifndef DISCONN_CLIENT_DISCOC_NNC_MANAGER_API_H_
#define DISCONN_CLIENT_DISCOC_NNC_MANAGER_API_H_

typedef enum {
    REQ_NN_QUERY_MDATA_READIO = 0,
    REQ_NN_QUERY_TRUE_MDATA_READIO,
    REQ_NN_ALLOC_MDATA_AND_GET_OLD,
    REQ_NN_ALLOC_MDATA_NO_OLD,
    REQ_NN_QUERY_MDATA_WRITEIO,
    REQ_NN_REPORT_READIO_RESULT,
    REQ_NN_REPORT_WRITEIO_RESULT_DEPRECATED, //useless now, when send ci report with opcode = 6, nn won't ack
    REQ_NN_REPORT_WRITEIO_RESULT,
    REQ_NN_UPDATE_HEARTBEAT,
#ifdef DISCO_PREALLOC_SUPPORT
    REQ_NN_PREALLOC_SPACE,
    REQ_NN_REPORT_SPACE_MDATA,
    REQ_NN_RETURN_SPACE,
#endif
} NNMDReq_opcode_t;

extern int8_t *get_NN_ipaddr(uint32_t *port);
extern int32_t get_cm_workq_size(void);
extern int32_t get_ch_workq_size(void);

extern uint32_t get_num_noResp_nnReq(void);
extern uint32_t get_last_rcv_nnReq_ID(void);

/*
 * nnMDReq_end_fn_t: metadata request end callback function
 * MData: metadata for IO Manager handle when acquire/report metadata done
 * private_data: private data from caller
 * calleeReq: caller request that trigger metadata request
 * result: metadata request process result
 */
typedef int32_t (nnMDReq_end_fn_t) (metadata_t *MData, void *private_data, ReqCommon_t *calleeReq, int32_t result);
/*
 * nnMDReq_setovw_fn_t: overwritten flag setting callback when acquire metadata for first write
 * private_data: private data from caller
 * lbid_s: start lbid
 * lbid_len: length of lbid
 */
typedef int32_t (nnMDReq_setovw_fn_t) (void *private_data, uint64_t lbid_s, uint32_t lbid_len);

extern uint32_t NNClientMgr_ref_NNClient(uint32_t nnIPAddr, uint32_t nnPort);
extern uint32_t NNClientMgr_deref_NNClient(uint32_t nnIPAddr, uint32_t nnPort);
//extern discoC_conn_t *get_NNClient_conn(uint32_t nnIPAddr, uint32_t nnPort);

extern void NNClientMgr_retry_NNMDReq(uint32_t nnIPaddr, uint32_t nnPort);
extern void NNClientMgr_wakeup_NNCWorker(void);
extern int32_t NNClientMgr_show_MDRequest(int8_t *buffer, int32_t buff_len);
extern int32_t NNClientMgr_get_num_MDReq(void);
extern uint32_t NNClientMgr_get_last_MDReqID(void);

extern int32_t discoC_acquire_metadata(UserIO_addr_t *ioaddr,
        bool mdCacheOn, ReqCommon_t *callerReq, uint16_t volReplica,
        bool mdCachehit, bool ovw, bool hbiderr,
        metadata_t *arr_MD, void *udata, nnMDReq_end_fn_t *MD_endfn, nnMDReq_setovw_fn_t *setOVW_fn);
extern int32_t discoC_report_metadata(UserIO_addr_t *ioaddr, bool mdCacheOn,
        ReqCommon_t *callerReq, uint16_t volReplica,
        bool mdCachehit, bool ovw, bool hbiderr,
        metadata_t *arr_MD, void *udata, nnMDReq_end_fn_t *MD_endfn);
#ifdef DISCO_PREALLOC_SUPPORT
extern int32_t discoC_prealloc_freeSpace(UserIO_addr_t *ioaddr, ReqCommon_t *callerReq,
        metadata_t *arr_MD, void *udata, nnMDReq_end_fn_t *MD_endfn);
extern int32_t discoC_flush_spaceMetadata(UserIO_addr_t *ioaddr, ReqCommon_t *callerReq,
        metadata_t *arr_MD, void *udata, nnMDReq_end_fn_t *MD_endfn);
extern int32_t discoC_report_freeSpace(UserIO_addr_t *ioaddr, ReqCommon_t *callerReq,
        metadata_t *arr_MD, void *udata, nnMDReq_end_fn_t *MD_endfn);
#endif

extern uint32_t NNClientMgr_add_NNClient(int8_t *nm_nnC, uint32_t ipaddr, uint32_t port);
extern int32_t NNClientMgr_rel_manager(void);
extern int32_t NNClientMgr_init_manager(void);

#endif /* DISCONN_CLIENT_DISCOC_NNC_MANAGER_API_H_ */
