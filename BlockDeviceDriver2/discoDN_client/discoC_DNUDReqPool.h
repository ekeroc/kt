/*
 * Copyright (C) 2010-2020 by Division F / ICL
 * Industrial Technology Research Institute
 *
 * discoC_DNUDReqPool.h
 *
 * Provide a hash base pool for holding all user data request (send to datanode)
 */

#ifndef DISCODN_CLIENT_DNCLIENT_UDREQPOOL_H_
#define DISCODN_CLIENT_DNCLIENT_UDREQPOOL_H_

#define DNREQ_HBUCKET_SIZE  4001

typedef struct discoC_DNUData_request_pool {
    struct list_head DNUDReq_pool[DNREQ_HBUCKET_SIZE]; //hashtable implemented by array of list_head
    spinlock_t DNUDReq_plock[DNREQ_HBUCKET_SIZE];      //per-bucket spin lock protection
    atomic_t num_DNUDReq;                              //number of DNUData Request in hashtable
} DNUDataReq_Pool_t;

extern int32_t DNUDReqPool_acquire_plock(DNUDataReq_Pool_t *reqPool, uint32_t dnReqID);
extern void DNUDReqPool_rel_plock(DNUDataReq_Pool_t *reqPool, int32_t lockID);

extern int32_t DNUDReqPool_addReq_nolock(DNUData_Req_t *dnUDReq, int32_t lockID, DNUDataReq_Pool_t *reqPool);
extern DNUData_Req_t *DNUDReqPool_findReq_nolock(uint32_t dnReqID, int32_t lockID, DNUDataReq_Pool_t *reqPool);
extern void DNUDReqPool_rmReq_nolock(DNUData_Req_t *myReq, DNUDataReq_Pool_t *reqPool);

extern void DNUDReqPool_get_retryReq(uint32_t dnIPaddr, uint32_t dnPort,
        struct list_head *UDReq_rlist, int32_t *num_UDReq, DNUDataReq_Pool_t *reqPool);

extern int32_t DNUDReqPool_show_pool(DNUDataReq_Pool_t *reqPool, int8_t *buffer, int32_t buff_len);
extern int32_t DNUDReqPool_get_numReq(DNUDataReq_Pool_t *reqPool);
extern void DNUDReqPool_rel_pool(DNUDataReq_Pool_t *reqPool);
extern void DNUDReqPool_init_pool(DNUDataReq_Pool_t *reqPool);

#endif /* DISCODN_CLIENT_DNCLIENT_UDREQPOOL_H_ */
