/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoDNSrv_CNRequest.c
 *
 * This component try to simulate DN
 *
 */

#ifndef DISCODN_SIMULATOR_DISCODNSRV_CNREQUEST_H_
#define DISCODN_SIMULATOR_DISCODNSRV_CNREQUEST_H_

typedef struct discoCN_request {
    struct list_head list_CNReq_pool;
    uint16_t opcode;
    uint16_t num_hbids;
    uint32_t pReqID;
    uint32_t DNReqID;
    CN2DN_UDRWReq_hder_t pkt_hder;
    uint64_t arr_hbids[MAX_LBS_PER_IOR];
    uint64_t phyLoc_rlo[MAX_LBS_PER_IOR];
} CNReq_t;

extern CNReq_t *CNReq_alloc_request(int32_t mpoolID);
extern void CNReq_init_request (CNReq_t *cnReq, uint16_t opcode, uint16_t numhb, uint32_t pReqID, uint32_t DNReqID);
extern void CNReq_free_request(int32_t mpool, CNReq_t *cnReq);

#endif /* DISCODN_SIMULATOR_DISCODNSRV_CNREQUEST_H_ */
