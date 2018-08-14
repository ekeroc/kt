/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * socket_reconn_worker.c
 *
 * We use flush_workqueue, cancal_delayed_work, queue_delayed_work to implement
 * connection retry mechanism in old version clientnode.
 * In this version, we create several threads to check connection pools and reconnect to
 * DN.
 * I think we have better control in new mechanism.
 */

#ifndef SOCKET_RECONN_WORKER_PRIVATE_H_
#define SOCKET_RECONN_WORKER_PRIVATE_H_

#define ONE_DAY_SECS        60*60*24
#define CONN_RETRY_INTVL       5   //sec

#define CANCAL_CR_TIME_WAIT_PERIOD  100
#define CANCEL_CR_CNT_WAIT_RETRY  300

typedef enum {
    CANCAL_CR_KEEP_RETRY,
    CANCAL_CR_DO_NOTHING,
    CANCAL_CR_FREE_TASK,
} cancel_cr_action_t;

#define NUM_CONNRETRY_WORKQ 8
typedef enum {
    BIT_Running,
} CR_stat_bit;

typedef enum {
    CR_STAT_INIT,
    CR_STAT_Waiting,
    CR_STAT_Connecting,
    CR_STAT_Cancel,
    CR_STAT_stop,
} CRTask_stat_t;

typedef struct conn_retry_task
{
    int8_t ipv4addr_str[IPV4ADDR_NM_LEN];
    uint32_t ipv4addr;
    uint32_t port;

    struct list_head list_CRPool;
    cs_wrapper_t *connRetry_sock;

    CRTask_stat_t crTask_state;
    spinlock_t crTask_state_lock;
    worker_t crTask_work;
    uint64_t crTask_start_t;  //jiffies to indicate when to start
    int32_t crTask_workq_idx; //which conn_retry_workq for retrying
    conn_retry_done_fn *cr_done_fn;
    void *user_data;
} connRetry_task_t;

typedef struct conn_retry_manager
{
    struct list_head connRetry_queue;
    struct rw_semaphore connRetry_qlock;
    atomic_t cnt_cr_tasks;
    uint64_t connRetry_INTVL_HZ;
    int32_t connRetry_mgr_state;
    struct workqueue_struct *conn_retry_workq[NUM_CONNRETRY_WORKQ];
} connRetry_mgr_t;

connRetry_mgr_t connR_Mgr;

static void conn_retry_handler_common(worker_t *retry_data);

#endif /* SOCKET_RECONN_WORKER_PRIVATE_H_ */
