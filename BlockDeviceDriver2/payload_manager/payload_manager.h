/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * payload_manager.h
 *
 * This component aim for managing payload
 *
 */

#ifndef PAYLOAD_MANAGER_PAYLOAD_MANAGER_H_
#define PAYLOAD_MANAGER_PAYLOAD_MANAGER_H_

//Manager data structure for payload manager
typedef struct discoC_payload_manager {
    payloadReq_Pool_t PLReqPool;     //Payload Request pool for holding all ongoing payload request
    atomic_t udata_reqID_generator;  //An atomic variable for generating an ID for each payload request
    int32_t preq_mpool_ID;           //payload Request mem pool ID for allocating memory to create payload request
    struct workqueue_struct *PLReq_retry_workq;  //A linux workqueue for payload request timeout setting
} payload_manager_t;

extern payload_manager_t g_payload_mgr;

extern void PLMgr_free_PLReq(payload_req_t *preq);
extern uint32_t PLMgr_gen_PLReqID(void);
extern int32_t PLMgr_submit_DNUDReadReq(metaD_item_t *mdItem, payload_req_t *pReq, int32_t replica_idx);

#ifdef DISCO_ERC_SUPPORT
extern int32_t PLMgr_submit_ERCRebuild_DNUDReq(metaD_item_t *mdItem, payload_req_t *pReq);
#endif
#endif /* PAYLOAD_MANAGER_PAYLOAD_MANAGER_H_ */
