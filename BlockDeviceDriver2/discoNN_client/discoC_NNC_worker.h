/*
 * Copyright (C) 2010 - 2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_NNC_worker.h
 *
 */

#ifndef DISCONN_CLIENT_DISCOC_NNC_WORKER_H_
#define DISCONN_CLIENT_DISCOC_NNC_WORKER_H_

#define NNC_WORKQ_HASH_NUM   32 //need power of 2
#define NNC_WORKQ_MOD_NUM   (NNC_WORKQ_HASH_NUM - 1)
#define NNC_WORKQ_TOTAL_NUM (NNC_WORKQ_HASH_NUM + 1)
#define NNC_WORKQ_HP_BK_IDX    NNC_WORKQ_HASH_NUM

struct discoC_namenode_client;
typedef void (NNC_worker_fn) (struct discoC_namenode_client *myClient);
typedef struct disco_NNClient_worker {
    struct list_head wker_ReqPool[NNC_WORKQ_TOTAL_NUM]; //worker request pool
    spinlock_t wker_ReqPlock[NNC_WORKQ_TOTAL_NUM];      //worker request pool lock
    uint32_t deq_bucket_idx;   //which bucket index to be get request from worker q
    atomic_t wker_ReqQ_size;   //number of requests in worker queue
    atomic_t wker_HPReqQ_size; //number of HP requests in worker queue

    wait_queue_head_t wker_waitq;  //linux waitq to control thread
    dmsc_thread_t *wker_thd;       //thread return by thread maanager
    NNC_worker_fn *wker_fn;        //worker function that be executed at thread function to process metadata request
    void *wker_private_data;       //private data for wker_fn
} discoNNC_worker_t;

extern void NNCWker_wakeup_worker(discoNNC_worker_t *myWker);
extern int32_t NNCWker_get_numReq_workQ(discoNNC_worker_t *myWker);

extern int32_t NNCWker_gain_qlock(uint32_t reqID, priority_type ptype, discoNNC_worker_t *myWker);
extern void NNCWker_rel_qlock(uint32_t lockID, discoNNC_worker_t *myWker);
extern void NNCWker_addReq_workerQ_nolock(uint32_t lockID, struct list_head *list_ptr, discoNNC_worker_t *myWker);
extern int32_t NNCWker_peekReq_gain_lock(discoNNC_worker_t *myWker);
extern struct list_head *NNCWker_peekReq_workerQ_nolock(int32_t lockID, discoNNC_worker_t *myWker);
extern void NNCWker_rmReq_workerQ_nolock(uint32_t lockID, struct list_head *list_ptr, discoNNC_worker_t *myWker);

extern void NNCWker_addReq_workerQ(uint32_t reqID, priority_type ptype, struct list_head *list_ptr, discoNNC_worker_t *myWker);
extern struct list_head *NNCWker_peekReq_workerQ(discoNNC_worker_t *myWker);

extern int32_t NNCWker_init_worker(discoNNC_worker_t *myWker, int8_t *thd_nm,
        NNC_worker_fn *wkerfn, void *wkerPData);
extern int32_t NNCWker_stop_worker(discoNNC_worker_t *myWker);
#endif /* DISCONN_CLIENT_DISCOC_NNC_WORKER_H_ */
