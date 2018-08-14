/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * thread_manager_private.h
 *
 */

#ifndef _THREAD_MANAGER_PRIVATE_H_
#define _THREAD_MANAGER_PRIVATE_H_

#define MAX_NUM_OF_SAMPLE_THREAD_PERFORMANCE    10000


typedef struct _thread_perf_item {
    int64_t Total_process_time;
    atomic64_t Total_process_cnt;

    int64_t Last_request_start_time;
    int64_t Total_time_period_between_2_requests;
    int64_t Process_Sample_Start_Time;
    int64_t Process_Sample_End_Time;
    int64_t Process_Sample_IOPS;
    int64_t Process_Sample_TP;
    int64_t Process_Sample_Bytes;
    atomic64_t Process_Sample_CNT;
} thd_perf_item_t;

typedef struct _dms_client_thread_performance {
    thd_perf_item_t main_thread;
    thd_perf_item_t main_thread_read;
    thd_perf_item_t main_thread_write;
    struct mutex main_thread_data_lock;

    thd_perf_item_t NN_worker;
    thd_perf_item_t NN_worker_cache_hit;
    thd_perf_item_t NN_worker_cache_miss;
    struct mutex nn_worker_data_lock;

    thd_perf_item_t NN_receivor;
    struct mutex nn_receivor_data_lock;

    thd_perf_item_t DN_worker;
    thd_perf_item_t DN_worker_read;
    thd_perf_item_t DN_worker_write;
    struct mutex dn_worker_data_lock;

    thd_perf_item_t DN_receivor;
    thd_perf_item_t DN_receivor_read;
    thd_perf_item_t DN_receivor_write;
    struct mutex dn_receivor_data_lock;

    thd_perf_item_t DN_Response_worker;
    thd_perf_item_t DN_Response_worker_read;
    thd_perf_item_t DN_Response_worker_write;
    struct mutex dn_response_worker_data_lock;

    thd_perf_item_t NN_commit_worker;
    struct mutex nn_commit_worker_data_lock;
} cn_thread_perf;

static cn_thread_perf dmsc_thd_perf;
struct rw_semaphore thd_pool_lock;  //thread pool lock to protect pool
struct list_head thd_pool;          //thread pool to hold all threads
atomic_t thread_id_cnt;             //counter for allocating ID for each thread

#endif /* _THREAD_MANAGER_PRIVATE_H_ */
