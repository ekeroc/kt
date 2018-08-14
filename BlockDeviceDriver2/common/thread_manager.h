/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * thread_manager.h
 *
 */

#ifndef _THREAD_MANAGER_H_
#define _THREAD_MANAGER_H_

#include <linux/wait.h>

typedef enum {
    THD_T_Main,
    THD_T_NNReq_Worker,
    THD_T_NNAck_Receivor,
    THD_T_DNReq_Worker,
    THD_T_DNAck_Receivor,
    THD_T_DNAck_Handler,
    THD_T_NNCIReport_Worker,
} discoC_thd_role_t;

typedef enum {
    THD_SubT_Main_Read,
    THD_SubT_Main_Write,
    THD_SubT_NNReq_CH,  //metadata cache hit
    THD_SubT_NNReq_CM,  //metadata cache miss
    THD_SubT_DNReq_Read,  //metadata cache miss
    THD_SubT_DNReq_Write,  //metadata cache miss
} discoC_thd_subrolt_t;

/*
 * NOTE: Normally, worker sleep on wait queue, sk receiver sleep on socket.
 *
 */
typedef enum {
    THREAD_WORKER,
    THREAD_SK_RECEIVER,
} thread_type_t;

#define MAX_THREAD_NAME_LEN 64

typedef struct dmsc_thread {
    struct list_head list;  //entry for adding thread to pool
    thread_type_t t_type;   //type: worker or socket rx thread
    int8_t t_name[MAX_THREAD_NAME_LEN];  //thread name

    wait_queue_head_t *ctl_wq;  //waitqueue pointer for worker threads to sleep when no requests
    volatile bool running;      //NOTE: force read from memory, flag to know whether user stop thread
    int32_t thread_id;          //thread id assigned when create

    void *usr_data;                  //hold user's private data
    int (*thread_fn)(void *data);    //thread function for thread to execute
    struct task_struct *kthread_ts;  //Linux thread data structure for user to stop

    uint32_t t_state;                //deprecated
    uint64_t last_handle_req;        //deprecated

    bool in_pool;                    //check whether thread in thread pool
} dmsc_thread_t;

extern int32_t stop_worker_thread(dmsc_thread_t *dmsc_t);
extern dmsc_thread_t *create_worker_thread(int8_t *name, wait_queue_head_t *wq, void *data,
        int32_t (*t_fn)(void *data));
extern dmsc_thread_t *create_sk_rx_thread(int8_t *name, void *usr_data,
        int32_t (*t_fn)(void *data));
extern void stop_sk_rx_thread(dmsc_thread_t *dmsc_t);

extern void init_thread_manager(void);
extern void release_thread_manager(void);

extern void add_thread_performance_data(int32_t thread_type, int32_t thread_sub_type,
                                 int64_t start_time, int64_t end_time,
                                 uint64_t num_of_bytes );
extern int Get_Thread_Performance_Data(int32_t thread_type, uint64_t *total_time,
                                       uint64_t *total_cnt,
                                       uint64_t *total_period_times,
                                       uint64_t *sample_start_time,
                                       uint64_t *sample_end_time,
                                       uint64_t *total_sample_bytes,
                                       uint64_t *total_sample_cnt,
                                       uint64_t *prev_iops, uint64_t *prev_tp,
                                       uint64_t  *sub_type_0_total_time,
                                       uint64_t *sub_type_0_total_cnt,
                                       uint64_t *sub_type_1_total_time,
                                       uint64_t *sub_type_1_total_cnt);
extern void clean_thread_performance_data(void);
extern uint32_t show_thread_info(int8_t *buffer, int32_t buff_len);
#endif /* _THREAD_MANAGER_H_ */
