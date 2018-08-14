/*
 * Copyright (C) 2010 - 2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_DNC_worker.h
 *
 */

#ifndef DISCODN_CLIENT_DISCOC_DNC_WORKER_H_
#define DISCODN_CLIENT_DISCOC_DNC_WORKER_H_

#define DN_PACKET_BUFFER_SIZE 8192

#define DNC_WORKQ_HASH_NUM   32 //need power of 2
#define DNC_WORKQ_MOD_NUM   (DNC_WORKQ_HASH_NUM - 1)
#define DNC_WORKQ_TOTAL_NUM (DNC_WORKQ_HASH_NUM + 1)
#define DNC_WORKQ_HP_BK_IDX    DNC_WORKQ_HASH_NUM

//typedef void (DNC_worker_fn) (struct discoC_namenode_client *myClient);
typedef struct disco_DNClient_worker {
    struct list_head wker_ReqPool[DNC_WORKQ_TOTAL_NUM]; //worker request pool
    spinlock_t wker_ReqPlock[DNC_WORKQ_TOTAL_NUM];      //request pool lock
    uint32_t deq_bucket_idx;      //which bucket index to be get request from worker q
    atomic_t wker_ReqQ_size;      //number of requests in worker queue
    atomic_t wker_HPReqQ_size;    //number of HP requests in worker queue
    wait_queue_head_t wker_waitq; //wait queue for thread to sleep
    dmsc_thread_t *wker_thd;      //worker thread data structure
} discoDNC_worker_t;

extern int32_t DNCWker_get_numReq_workQ(discoDNC_worker_t *myWker);
extern void DNCWker_add_Req_workQ(DNUData_Req_t *myReq, discoDNC_worker_t *wker);
extern void DNCWker_rm_Req_workQ(DNUData_Req_t *myReq, discoDNC_worker_t *wker);
extern int32_t DNCWker_init_worker(discoDNC_worker_t *myWker, int8_t *thd_nm, void *pData);
extern int32_t DNCWker_stop_worker(discoDNC_worker_t *myWker);

#endif /* DISCODN_CLIENT_DISCOC_DNC_WORKER_H_ */
