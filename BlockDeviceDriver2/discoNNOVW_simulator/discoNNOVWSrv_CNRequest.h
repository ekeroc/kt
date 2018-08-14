/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoNNOVWSrv_CNRequest.c
 *
 * This component try to simulate NN OVW
 *
 */

#ifndef DISCODN_SIMULATOR_DISCONNOVWSRV_CNREQUEST_H_
#define DISCODN_SIMULATOR_DISCONNOVWSRV_CNREQUEST_H_

typedef struct discoCNOVW_request {
    struct list_head list_CNOVWReq_pool;
    uint16_t opcode;
    uint16_t subopcode;
    uint64_t volumeID;
} CNOVWReq_t;

extern CNOVWReq_t *CNOVWReq_alloc_request(int32_t mpoolID);
extern void CNOVWReq_init_request(CNOVWReq_t *cnReq, uint16_t opcode, uint16_t subopcode, uint64_t volumeID);
extern void CNOVWReq_free_request(int32_t mpool, CNOVWReq_t *cnReq);

#endif /* DISCODN_SIMULATOR_DISCONNOVWSRV_CNREQUEST_H_ */
