/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoNNSrv_CNRequest.c
 *
 * This component try to simulate NN
 *
 */

#ifndef DISCODN_SIMULATOR_DISCONNSRV_CNREQUEST_H_
#define DISCODN_SIMULATOR_DISCONNSRV_CNREQUEST_H_

typedef struct discoCNMD_request {
    struct list_head list_CNMDReq_pool;
    uint16_t opcode;
    uint64_t CNMDReqID;
    uint64_t volumeID;
    uint64_t lbid_s;
    uint32_t lbid_len;
} CNMDReq_t;

extern CNMDReq_t *CNMDReq_alloc_request(int32_t mpoolID);
extern void CNMDReq_init_request(CNMDReq_t *cnReq, uint16_t opcode, uint64_t cnMDReqID, uint64_t volumeID);
extern void CNMDReq_free_request(int32_t mpool, CNMDReq_t *cnReq);

#endif /* DISCODN_SIMULATOR_DISCONNSRV_CNREQUEST_H_ */
