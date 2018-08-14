/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_IODone_worker.h
 */

#ifndef IO_MANAGER_DISCOC_IODONE_WORKER_H_
#define IO_MANAGER_DISCOC_IODONE_WORKER_H_

#define MAX_IO_FREE_THDS 16
#define NUM_IO_FREE_THDS 1
#define IORFreeQ_HASH_NUM   32 //need power of 2
#define IORFreeQ_MOD_NUM   (IORFreeQ_HASH_NUM - 1)
#define IORFreeQ_TOTAL_NUM (IORFreeQ_HASH_NUM)

typedef struct discoC_IOR_free_worker_manager {
    spinlock_t ioReq_freeQ_plock[IORFreeQ_TOTAL_NUM];
    struct list_head ioReq_freeQ_pool[IORFreeQ_TOTAL_NUM];
    atomic_t ioReq_freeQ_deqIdx;

    atomic_t ioReq_freeQ_Size;
    wait_queue_head_t ioReq_fWker_wq;

    dmsc_thread_t *ioReq_fWker[MAX_IO_FREE_THDS];
    atomic_t num_fWker;
} IOR_freeWker_Mgr_t;

extern IOR_freeWker_Mgr_t IOR_fWker_mgr;

extern int32_t IORFWkerMgr_workq_size(void);

extern void IORFWkerMgr_submit_request(io_request_t *io_req);
extern void IORFWkerMgr_init_manager(void);
extern void IORFWkerMgr_rel_manager(void);

#endif /* IO_MANAGER_DISCOC_IODONE_WORKER_H_ */
