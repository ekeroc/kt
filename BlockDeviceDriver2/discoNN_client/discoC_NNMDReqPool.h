/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_NNMDReqPool.h
 *
 * Provide a hash base pool for holding all metadata requests
 */
#ifndef _DISCOC_NNMDREQPOOL_H_
#define _DISCOC_NNMDREQPOOL_H_

#define NNREQ_HBUCKET_SIZE  4001

typedef struct discoC_NNMData_request_pool {
    struct list_head NNMDReq_pool[NNREQ_HBUCKET_SIZE];
    spinlock_t NNMDReq_plock[NNREQ_HBUCKET_SIZE];
    atomic_t num_NNMDReq;
} NNMDataReq_Pool_t;

extern int32_t NNMDReqPool_acquire_plock(NNMDataReq_Pool_t *reqPool, uint32_t nnReqID);
extern void NNMDReqPool_rel_plock(NNMDataReq_Pool_t *reqPool, int32_t lockID);

extern int32_t NNMDReqPool_addReq_nolock(NNMData_Req_t *nnMDReq, int32_t lockID, NNMDataReq_Pool_t *reqPool);
extern NNMData_Req_t *NNMDReqPool_findReq_nolock(uint32_t nnReqID, int32_t lockID, NNMDataReq_Pool_t *reqPool);
extern void NNMDReqPool_rmReq_nolock(NNMData_Req_t *myReq, NNMDataReq_Pool_t *reqPool);

extern void NNMDReqPool_get_retryReq(uint32_t nnIPaddr, uint32_t nnPort,
        struct list_head *aMD_rlist, int32_t *num_raMD,
        struct list_head *ciMD_rlist, int32_t *num_ciMD, NNMDataReq_Pool_t *reqPool);

extern int32_t NNMDReqPool_show_pool(NNMDataReq_Pool_t *reqPool, int8_t *buffer, int32_t buff_len);
extern int32_t NNMDReqPool_get_numReq(NNMDataReq_Pool_t *reqPool);

extern void NNMDReqPool_rel_pool(NNMDataReq_Pool_t *reqPool);
extern void NNMDReqPool_init_pool(NNMDataReq_Pool_t *reqPool);

#endif /* _DISCOC_NNMDREQPOOL_H_ */
