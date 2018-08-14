/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * DN_Ack_Worker_private.h
 *
 */

#ifndef _DN_ACK_WORKER_PRIVATE_H_
#define _DN_ACK_WORKER_PRIVATE_H_

typedef struct disco_DNAck_Worker {
    dmsc_thread_t *DNAck_wker_thd;  //worker thread data structure
    int32_t DNAck_handle_qidx;      //which queue is ack worker handling
    void *wker_private_data;        //private data for thread function
} DNAck_wker_t;

#define MAXNUM_DNACK_WORKER    16
#define NUM_DNACK_WORKER        2
#define DNACK_ENQ_MOD_NUMBER     (NUM_DNACK_WORKER - 1)

typedef struct disco_DNAck_Worker_Manager {
    struct list_head dnAck_queue[MAXNUM_DNACK_WORKER]; //ack worker queue for holding ack
    spinlock_t dnAck_qlock[MAXNUM_DNACK_WORKER];       //q lock to protect ack queue
    atomic_t dnAck_qsize;                              //number of ack in queues
    wait_queue_head_t dnAck_waitq;                     //linux wait queue for worker sleep on
    DNAck_wker_t DNCAck_wker[MAXNUM_DNACK_WORKER];     //Ack worker

    int32_t DNAck_mpool_ID;                            //memory pool id of ack memory pool
    atomic_t num_DNAckWorker;                          //number of running ack worker
} DNAckWker_Mgr_t;

DNAckWker_Mgr_t DNAck_mgr;

#endif /* _DN_ACK_WORKER_PRIVATE_H_ */
