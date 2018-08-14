/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_DNC_Manager_export.h
 *
 * Export data structure and API for non-discoDN_client components
 */

#ifndef DISCODN_CLIENT_DISCOC_DNC_MANAGER_EXPORT_H_
#define DISCODN_CLIENT_DISCOC_DNC_MANAGER_EXPORT_H_


#if (DN_PROTOCOL_VERSION == 2) && (CN_CRASH_RECOVERY == 1)
extern void update_cleanLog_ts(void);
#endif

/*
 * @mdItem: metadata item that DNClient handling for R/W data at submit
 * @private_data: callback argument that required when DNClient call callback function.
 *                caller delivery this at submit
 * @payload: when handling a read data request, DNClient return payload to caller
 * @result:  result the DNClient process R/W request from caller
 * @calleeReq: DNReq to for caller
 * @bk_idx   : which bucket of caller's DNReq pool has this DNRequest
 * @dnLoc_idx: which DNLoc index of mdItem DNClent processing
 */
typedef int32_t (DNUDReq_endfn_t) (metaD_item_t *mdItem, void *private_data,
        data_mem_chunks_t *payload, int32_t result,
        ReqCommon_t *calleeReq, int32_t bk_idx, int32_t dnLoc_idx);

extern int32_t get_num_DNUDataReq_waitSend(void);
extern int32_t get_num_DNUDataReq_waitResp(void);
extern uint32_t get_last_DNUDataReqID(void);
extern uint32_t get_last_DNUDataRespID(void);

extern void DNClientMgr_resend_DNUDReq(uint32_t ipaddr, uint32_t port);
extern void DNClientMgr_cancel_DNUDReq(uint32_t dnReqID, uint32_t callerID);
extern void DNClientMgr_update_DNUDReq_TOInfo(uint32_t dnReqID, uint32_t callerID);

extern int32_t submit_DNUData_Write_Request(metaD_item_t *md_item,
        int32_t replic_idx, data_mem_chunks_t *data_buf, uint64_t payload_offset,
        ReqCommon_t *callerReq, DNUDReq_endfn_t *endfn, void *pdata,
        int32_t bk_idx_caller, uint32_t *dnReqID_bk,
        uint8_t *fp_map, uint8_t *fp_cache, bool send_data, bool rw_data);
extern int32_t submit_DNUData_Read_Request(metaD_item_t *md_item,
        int32_t replic_idx, uint64_t payload_offset,
        ReqCommon_t *callerReq, DNUDReq_endfn_t *endfn, void *pdata,
        int32_t bk_idx_caller, uint32_t *dnReqID_bk,
        bool send_data, bool rw_data);

extern int32_t DNClientMgr_rel_manager(void);
extern int32_t DNClientMgr_init_manager(void);

#endif /* DISCODN_CLIENT_DISCOC_DNC_MANAGER_EXPORT_H_ */
