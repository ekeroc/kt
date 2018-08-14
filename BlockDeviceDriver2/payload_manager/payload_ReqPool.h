/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * payload_ReqPool.h
 *
 */

#ifndef PAYLOAD_MANAGER_PAYLOAD_REQPOOL_H_
#define PAYLOAD_MANAGER_PAYLOAD_REQPOOL_H_

//UDATA: user data, HBUCKET: hash bucket
#define PLREQ_HBUCKET_SIZE    4001

typedef struct discoC_payload_request_pool {
    struct list_head plReq_pool[PLREQ_HBUCKET_SIZE]; //An array of link list to act as hashtable
    spinlock_t plReq_plock[PLREQ_HBUCKET_SIZE];      //llock: list lock to protect each link list
    atomic_t num_plReqs;                             //number of payload request in pool
} payloadReq_Pool_t;

extern int32_t PLReqPool_get_numReq(payloadReq_Pool_t *pReqPool);
extern int32_t PLReqPool_acquire_plock(payloadReq_Pool_t *pReqPool, uint32_t plReqID);
extern void PLReqPool_rel_plock(payloadReq_Pool_t *pReqPool, uint32_t locID);
extern int32_t PLReqPool_addReq_nolock(payload_req_t *plReq, int32_t lockID,
        payloadReq_Pool_t *plReqPool);
extern payload_req_t *PLReqPool_findReq_nolock(uint32_t plReqID, int32_t lockID,
        payloadReq_Pool_t *plReqPool);
extern void PLReqPool_rmReq_nolock(payload_req_t *myReq, payloadReq_Pool_t *reqPool);

extern void PLReqPool_init_pool(payloadReq_Pool_t *pReqPool);
extern void PLReqPool_rel_pool(payloadReq_Pool_t *pReqPool);
#endif /* PAYLOAD_MANAGER_PAYLOAD_REQPOOL_H_ */
