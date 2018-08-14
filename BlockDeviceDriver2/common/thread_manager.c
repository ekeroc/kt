/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * thread_manager.c
 *
 */
#include <linux/blkdev.h>
#include <linux/kthread.h>
#include <net/sock.h>
#include <linux/delay.h>
#include "discoC_sys_def.h"
#include "common_util.h"
#include "dms_kernel_version.h"
#include "discoC_mem_manager.h"
#include "thread_manager_private.h"
#include "thread_manager.h"
#include "../config/dmsc_config.h"

/*
 * add_to_thread_pool: add thread to pool
 * @dmsc_t: thread to be added to pool
 */
static void add_to_thread_pool (dmsc_thread_t *dmsc_t)
{
    if (IS_ERR_OR_NULL(dmsc_t)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN wrong para list for add thread pool\n");
        DMS_WARN_ON(true);
        return;
    }

    down_write(&thd_pool_lock);

    do {
        if (dmsc_t->in_pool) {
            dms_printk(LOG_LVL_INFO,
                    "DMSC INFO worker thread %s already in pool\n",
                    dmsc_t->t_name);
            break;
        }

        list_add_tail(&(dmsc_t->list), &thd_pool);
        dmsc_t->in_pool = true;
    } while (0);

    up_write(&thd_pool_lock);
}

/*
 * rm_from_thread_pool: remove thread from pool
 * @dmsc_t: thread to be remove from pool
 */
static void rm_from_thread_pool (dmsc_thread_t *dmsc_t)
{
    if (IS_ERR_OR_NULL(dmsc_t)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN wrong para list for rm thread pool\n");
        DMS_WARN_ON(true);
        return;
    }

    down_write(&thd_pool_lock);
    do {
        if (!dmsc_t->in_pool) {
            dms_printk(LOG_LVL_INFO,
                    "DMSC INFO worker thread %s already in pool\n",
                    dmsc_t->t_name);
            break;
        }

        list_del(&(dmsc_t->list));
        dmsc_t->in_pool = false;
    } while (0);

    up_write(&thd_pool_lock);
}

static void init_thread_state (dmsc_thread_t *dmsc_t)
{
    dmsc_t->t_state = 0;
}

/*
 * stop_sk_rx_thread: actual implementation to stop socket rx thread
 * @dmsc_t: dmsc thread data structure get from create_sk_rx_thread
 *
 */
static void stop_sk_rx_thread_no_free (dmsc_thread_t *dmsc_t)
{
    struct task_struct *tsk;
    do {
        tsk = dmsc_t->kthread_ts;
        if (IS_ERR_OR_NULL(tsk)) {
            break;
        }

        if (pid_alive(tsk)) {
            kthread_stop(tsk);
        }
    } while (0);
}

/*
 * stop_sk_rx_thread: remove thread from pool and stop a socket rx thread
 * @dmsc_t: dmsc thread data structure get from create_sk_rx_thread
 *
 */
void stop_sk_rx_thread (dmsc_thread_t *dmsc_t)
{
    rm_from_thread_pool(dmsc_t);

    stop_sk_rx_thread_no_free(dmsc_t);

    discoC_mem_free(dmsc_t);
    dmsc_t = NULL;
}

/*
 * create_sk_rx_thread: create a worker thread and add to thread pool
 * @name: thread name
 * @data: user's private data
 * @t_fn: thread function the thread want to execute
 *
 * Return: A dmsc_thread_t that user can stop thread with this return data structure
 *
 * NOTE: thread function's input is a dmsc_thread_t, user need translate thread function
 *       into dmsc_thread_t and get their private data from dmsc_thread_t->usr_data
 */

dmsc_thread_t *create_sk_rx_thread (int8_t *name, void *usr_data,
        int32_t (*t_fn)(void *data))
{
    dmsc_thread_t *dmsc_t;

    if (IS_ERR_OR_NULL(name) || IS_ERR_OR_NULL(t_fn) || IS_ERR_OR_NULL(usr_data)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN wrong para for create sk rx thread\n");
        DMS_WARN_ON(true);
        return NULL;
    }

    dmsc_t = discoC_mem_alloc(sizeof(dmsc_thread_t), GFP_NOWAIT | GFP_ATOMIC);
    if (IS_ERR_OR_NULL(dmsc_t)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN alloc mem for worker thread %s fail \n", name);
        DMS_WARN_ON(true);
        return NULL;
    }

    INIT_LIST_HEAD(&(dmsc_t->list));
    dmsc_t->in_pool = false;
    dmsc_t->t_type = THREAD_SK_RECEIVER;
    memcpy(dmsc_t->t_name, name, strlen(name));
    dmsc_t->ctl_wq = NULL;
    dmsc_t->running = true;
    dmsc_t->thread_id = atomic_add_return(1, &thread_id_cnt);
    dmsc_t->usr_data = usr_data;
    dmsc_t->thread_fn = t_fn;
    init_thread_state(dmsc_t);
    dmsc_t->last_handle_req = 0;

    dmsc_t->kthread_ts = kthread_run(dmsc_t->thread_fn, dmsc_t, dmsc_t->t_name);
    if (IS_ERR_OR_NULL(dmsc_t->kthread_ts)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN create kthread for worker thread %s fail\n", name);
        discoC_mem_free(dmsc_t);
        return NULL;
    }

    add_to_thread_pool(dmsc_t);

    return dmsc_t;
}

/*
 * _stop_wker_thread: actual implementation to stop worker thread
 *      mark thread not running and wakeup all thread to exit from thread function
 * @dmsc_t: dmsc thread data structure get from create_worker_thread
 */
static void _stop_wker_thread (dmsc_thread_t *dmsc_t)
{
    int32_t count;
    dmsc_t->running = false;

    if (!IS_ERR_OR_NULL(dmsc_t->ctl_wq)) {
        //if (waitqueue_active(dmsc_t->ctl_wq)) {
        wake_up_interruptible_all(dmsc_t->ctl_wq);
        //}
    } else {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
        kill_proc(dmsc_t->kthread_ts->pid, SIGKILL,  0);
#else
        kill_pid(find_vpid(dmsc_t->kthread_ts->pid), SIGKILL, 0);
#endif
    }

    count = 0;
    while (pid_alive(dmsc_t->kthread_ts)) {
        msleep(100);
        count++;

        if (count != 0 && count % 10 == 0) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN fail stop worker thread %s "
                    "(wait count %d)\n", dmsc_t->t_name, count);
        }
    }

}

/*
 * stop_worker_thread: remove thread from pool and stop a worker thread
 * @dmsc_t: dmsc thread data structure get from create_worker_thread
 *
 * Return: stop thread success or not
 *         0 : stop success
 *         ~0: stop fail
 */
int32_t stop_worker_thread (dmsc_thread_t *dmsc_t)
{
    int32_t ret;

    ret = 0;
    if (IS_ERR_OR_NULL(dmsc_t)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN wrong para for stop worker thread\n");
        DMS_WARN_ON(true);
        return -EINVAL;
    }

    if (THREAD_WORKER == dmsc_t->t_type) {
        rm_from_thread_pool(dmsc_t);
        _stop_wker_thread(dmsc_t);
        discoC_mem_free(dmsc_t);
    } else {
        ret = -EINVAL;
        dms_printk(LOG_LVL_WARN, "DMSC WARN try to stop non-worker %d\n", dmsc_t->t_type);
    }

    return ret;
}

/*
 * create_worker_thread: create a worker thread and add to thread pool
 * @name: thread name
 * @wq: a waitqueue for thread the sleep on when no requests for this thread
 * @data: user's private data
 * @t_fn: thread function the thread want to execute
 *
 * Return: A dmsc_thread_t that user can stop thread with this return data structure
 *
 * NOTE: thread function's input is a dmsc_thread_t, user need translate thread function
 *       into dmsc_thread_t and get their private data from dmsc_thread_t->usr_data
 */
dmsc_thread_t *create_worker_thread (int8_t *name, wait_queue_head_t *wq, void *data,
        int32_t (*t_fn)(void *data))
{
    dmsc_thread_t *dmsc_t;

    dmsc_t = NULL;
    do {
        if (IS_ERR_OR_NULL(name) || IS_ERR_OR_NULL(t_fn)) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN wrong para for create worker thread\n");
            DMS_WARN_ON(true);
            break;
        }

        dmsc_t = discoC_mem_alloc(sizeof(dmsc_thread_t), GFP_KERNEL);
        if (IS_ERR_OR_NULL(dmsc_t)) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN alloc mem for worker thread %s fail \n", name);
            DMS_WARN_ON(true);
            break;
        }

        INIT_LIST_HEAD(&(dmsc_t->list));
        dmsc_t->in_pool = false;
        dmsc_t->t_type = THREAD_WORKER;
        memcpy(dmsc_t->t_name, name, strlen(name));
        dmsc_t->ctl_wq = wq;
        dmsc_t->running = true;
        dmsc_t->thread_id = atomic_add_return(1, &thread_id_cnt);
        dmsc_t->usr_data = data;
        dmsc_t->thread_fn = t_fn;
        init_thread_state(dmsc_t);
        dmsc_t->last_handle_req = 0;

        dmsc_t->kthread_ts = kthread_run(dmsc_t->thread_fn, dmsc_t, dmsc_t->t_name);

        if (IS_ERR_OR_NULL(dmsc_t->kthread_ts)) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN create kthread for worker thread %s fail\n", name);
            discoC_mem_free(dmsc_t);
            break;
        }

        add_to_thread_pool(dmsc_t);

    } while (0);

    return dmsc_t;
}

/*
 * stop_all_threads: leverage stop function to stop all worker thread and
 *     socket rx thread
 */
static void stop_all_threads (void)
{
    dmsc_thread_t *cur, *next;

    down_write(&thd_pool_lock);
    list_for_each_entry_safe (cur, next, &thd_pool, list) {
        if (IS_ERR_OR_NULL(cur)) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN thread pool list broken\n");
            DMS_WARN_ON(true);
            break;
        }

        if (THREAD_WORKER == cur->t_state) {
            _stop_wker_thread(cur);
        } else {
            stop_sk_rx_thread_no_free(cur);
        }
    }
    up_write(&thd_pool_lock);
}

/*
 * free_all_thread_item: free all thread data structure in thread pool
 *
 * NOTE: call guarantee all thread is stopped
 */
static void free_all_thread_item (void)
{
    dmsc_thread_t *cur, *next;
    bool list_bug;

    list_bug = false;
    down_write(&thd_pool_lock);
    list_for_each_entry_safe (cur, next, &thd_pool, list) {
        if (IS_ERR_OR_NULL(cur)) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN thread pool list broken\n");
            DMS_WARN_ON(true);
            break;
        }

        if (dms_list_del(&cur->list)) {
            list_bug = true;
        } else {
            discoC_mem_free(cur);
        }
    }
    up_write(&thd_pool_lock);

    if (list_bug) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN list_del chk fail %s %d\n",
                __func__, __LINE__);
        DMS_WARN_ON(true);
        msleep(1000);
    }
}

static void init_thread_performance_item (thd_perf_item_t *p_item)
{
    if (p_item == NULL) {
        return;
    }

    p_item->Total_process_time = 0;
    p_item->Total_time_period_between_2_requests = 0;
    atomic64_set(&p_item->Total_process_cnt, 0);
    p_item->Last_request_start_time = -1;
    p_item->Process_Sample_Start_Time = 0;
    p_item->Process_Sample_End_Time = 0;
    p_item->Process_Sample_IOPS = 0;
    p_item->Process_Sample_TP = 0;
    p_item->Process_Sample_Bytes = 0;
    atomic64_set(&p_item->Process_Sample_CNT, 0);
}

static void add_data_to_thread_performance_data_item(
                              thd_perf_item_t *p_item, int64_t start_time,
                              int64_t end_time, uint64_t num_of_bytes,
                              int64_t time_period)
{
    int64_t tmp_u_sec = 0;
    uint64_t tmp_time_period = 0;

    p_item->Total_process_time += time_period;
    atomic64_inc(&p_item->Total_process_cnt);

    if (p_item->Last_request_start_time > -1) {
        if (start_time > p_item->Last_request_start_time) {
            tmp_u_sec = start_time - p_item->Last_request_start_time;
        }
    }

    if (start_time >= p_item->Last_request_start_time) {
        p_item->Last_request_start_time = start_time;
    }

    p_item->Total_time_period_between_2_requests += tmp_u_sec;

    p_item->Process_Sample_End_Time = end_time;
    p_item->Process_Sample_Bytes +=  (num_of_bytes >> BIT_LEN_KBYTES);
    atomic64_inc(&p_item->Process_Sample_CNT);

    if (atomic64_read(&p_item->Process_Sample_CNT) %
            MAX_NUM_OF_SAMPLE_THREAD_PERFORMANCE == 0) {
        p_item->Process_Sample_Start_Time = start_time;
        atomic64_set(&p_item->Process_Sample_CNT, 1);
        p_item->Process_Sample_Bytes = (num_of_bytes >> BIT_LEN_KBYTES);
    } else if (atomic64_read(&p_item->Process_Sample_CNT) %
                MAX_NUM_OF_SAMPLE_THREAD_PERFORMANCE == (MAX_NUM_OF_SAMPLE_THREAD_PERFORMANCE -
                        1)) {
        if (p_item->Process_Sample_End_Time > p_item->Process_Sample_Start_Time) {
            tmp_time_period = jiffies_to_msecs(p_item->Process_Sample_End_Time -
                                                p_item->Process_Sample_Start_Time);
        } else {
            tmp_time_period = 0;
        }

        if (tmp_time_period > 0) {
            p_item->Process_Sample_IOPS = MAX_NUM_OF_SAMPLE_THREAD_PERFORMANCE * 1000L /
                                          tmp_time_period;
            p_item->Process_Sample_TP = p_item->Process_Sample_Bytes * 1000L /
                                        tmp_time_period;
        } else {
            p_item->Process_Sample_IOPS = 0;
            p_item->Process_Sample_TP = 0;
        }
    }
}

void clean_thread_performance_data(void)
{
    mutex_lock(&dmsc_thd_perf.main_thread_data_lock);
    mutex_lock(&dmsc_thd_perf.nn_worker_data_lock);
    mutex_lock(&dmsc_thd_perf.nn_receivor_data_lock);
    mutex_lock(&dmsc_thd_perf.dn_worker_data_lock);
    mutex_lock(&dmsc_thd_perf.dn_receivor_data_lock);
    mutex_lock(&dmsc_thd_perf.dn_response_worker_data_lock);
    mutex_lock(&dmsc_thd_perf.nn_commit_worker_data_lock);

    init_thread_performance_item(&dmsc_thd_perf.main_thread);
    init_thread_performance_item(&dmsc_thd_perf.main_thread_read);
    init_thread_performance_item(&dmsc_thd_perf.main_thread_write);

    init_thread_performance_item(&dmsc_thd_perf.NN_worker);
    init_thread_performance_item(&dmsc_thd_perf.NN_worker_cache_hit);
    init_thread_performance_item(&dmsc_thd_perf.NN_worker_cache_miss);

    init_thread_performance_item(&dmsc_thd_perf.NN_receivor);

    init_thread_performance_item(&dmsc_thd_perf.DN_worker);
    init_thread_performance_item(&dmsc_thd_perf.DN_worker_read);
    init_thread_performance_item(&dmsc_thd_perf.DN_worker_write);

    init_thread_performance_item(&dmsc_thd_perf.DN_receivor);
    init_thread_performance_item(&dmsc_thd_perf.DN_receivor_read);
    init_thread_performance_item(&dmsc_thd_perf.DN_receivor_write);

    init_thread_performance_item(&dmsc_thd_perf.DN_Response_worker);
    init_thread_performance_item(
        &dmsc_thd_perf.DN_Response_worker_read);
    init_thread_performance_item(
        &dmsc_thd_perf.DN_Response_worker_write);

    init_thread_performance_item(&dmsc_thd_perf.NN_commit_worker);

    mutex_unlock(&dmsc_thd_perf.nn_commit_worker_data_lock);
    mutex_unlock(&dmsc_thd_perf.dn_response_worker_data_lock);
    mutex_unlock(&dmsc_thd_perf.dn_receivor_data_lock);
    mutex_unlock(&dmsc_thd_perf.dn_worker_data_lock);
    mutex_unlock(&dmsc_thd_perf.nn_receivor_data_lock);
    mutex_unlock(&dmsc_thd_perf.nn_worker_data_lock);
    mutex_unlock(&dmsc_thd_perf.main_thread_data_lock);
}

void add_thread_performance_data(int thread_type, int thread_sub_type,
                                 int64_t start_time, int64_t end_time,
                                 uint64_t num_of_bytes)
{
    int64_t time_period_u_sec = 0;
    thd_perf_item_t *p_item = NULL;

    if (start_time > end_time) {
        dms_printk(LOG_LVL_INFO,
                "DMSC INFO jiffies64 maybe overflow: "
                "start time %lld end time %lld\n",
                start_time, end_time);
        return;
    }

    time_period_u_sec = jiffies_to_usecs(end_time - start_time);

    if (thread_type == THD_T_Main) {
        mutex_lock(&dmsc_thd_perf.main_thread_data_lock);
        add_data_to_thread_performance_data_item(&dmsc_thd_perf.main_thread,
                start_time, end_time, num_of_bytes, time_period_u_sec);

        if (thread_sub_type == THD_SubT_Main_Read) {
            p_item = &dmsc_thd_perf.main_thread_read;
        } else {
            p_item = &dmsc_thd_perf.main_thread_write;
        }

        p_item->Total_process_time += time_period_u_sec;
        atomic64_inc(&p_item->Total_process_cnt);
        mutex_unlock(&dmsc_thd_perf.main_thread_data_lock);
    } else if (thread_type == THD_T_NNReq_Worker) {
        mutex_lock(&dmsc_thd_perf.nn_worker_data_lock);

        add_data_to_thread_performance_data_item(&dmsc_thd_perf.NN_worker,
                start_time, end_time, num_of_bytes, time_period_u_sec);

        if (thread_sub_type == THD_SubT_NNReq_CH) {
            p_item = &dmsc_thd_perf.NN_worker_cache_hit;
        } else {
            p_item = &dmsc_thd_perf.NN_worker_cache_miss;
        }

        p_item->Total_process_time += time_period_u_sec;
        atomic64_inc(&p_item->Total_process_cnt);
        mutex_unlock(&dmsc_thd_perf.nn_worker_data_lock);
    } else if (thread_type == THD_T_NNAck_Receivor) {
        mutex_lock(&dmsc_thd_perf.nn_receivor_data_lock);
        add_data_to_thread_performance_data_item(&dmsc_thd_perf.NN_receivor,
                start_time, end_time, num_of_bytes, time_period_u_sec);
        mutex_unlock(&dmsc_thd_perf.nn_receivor_data_lock);
    } else if (thread_type == THD_T_DNReq_Worker) {
        mutex_lock(&dmsc_thd_perf.dn_worker_data_lock);
        add_data_to_thread_performance_data_item(&dmsc_thd_perf.DN_worker,
                start_time, end_time, num_of_bytes, time_period_u_sec);

        if (thread_sub_type == THD_SubT_DNReq_Read) {
            p_item = &dmsc_thd_perf.DN_worker_read;
        } else {
            p_item = &dmsc_thd_perf.DN_worker_write;
        }

        p_item->Total_process_time += time_period_u_sec;
        atomic64_inc(&p_item->Total_process_cnt);

        mutex_unlock(&dmsc_thd_perf.dn_worker_data_lock);
    } else if (thread_type == THD_T_DNAck_Receivor) {
        mutex_lock(&dmsc_thd_perf.dn_receivor_data_lock);
        add_data_to_thread_performance_data_item(&dmsc_thd_perf.DN_receivor,
                start_time, end_time, num_of_bytes, time_period_u_sec);

        if (thread_sub_type == THD_SubT_DNReq_Read) {
            p_item = &dmsc_thd_perf.DN_receivor_read;
        } else {
            p_item = &dmsc_thd_perf.DN_receivor_write;
        }

        p_item->Total_process_time += time_period_u_sec;
        atomic64_inc(&p_item->Total_process_cnt);
        mutex_unlock(&dmsc_thd_perf.dn_receivor_data_lock);
    } else if (thread_type == THD_T_DNAck_Handler) {
        mutex_lock(&dmsc_thd_perf.dn_response_worker_data_lock);
        add_data_to_thread_performance_data_item(
            &dmsc_thd_perf.DN_Response_worker, start_time, end_time, num_of_bytes,
            time_period_u_sec);

        if (thread_sub_type == THD_SubT_DNReq_Read) {
            p_item = &dmsc_thd_perf.DN_Response_worker_read;
        } else {
            p_item = &dmsc_thd_perf.DN_Response_worker_write;
        }

        p_item->Total_process_time += time_period_u_sec;
        atomic64_inc(&p_item->Total_process_cnt);
        mutex_unlock(&dmsc_thd_perf.dn_response_worker_data_lock);
    } else if (thread_type == THD_T_NNCIReport_Worker) {
        mutex_lock(&dmsc_thd_perf.nn_commit_worker_data_lock);
        add_data_to_thread_performance_data_item(
            &dmsc_thd_perf.NN_commit_worker, start_time, end_time, num_of_bytes,
            time_period_u_sec);
        mutex_unlock(&dmsc_thd_perf.nn_commit_worker_data_lock);
    } else {
        dms_printk(LOG_LVL_INFO,
                    "DMS-CLIENTNODE INFO %s: unknown thread type %d for performance data\n",
                    __func__, thread_type);
    }
}

int Get_Thread_Performance_Data(int thread_type, uint64_t *total_time,
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
                                uint64_t *sub_type_1_total_cnt)
{
    thd_perf_item_t *p_item = NULL;
    thd_perf_item_t *p_item_sub_0 = NULL;
    thd_perf_item_t *p_item_sub_1 = NULL;
    int ret = 0;

    if (thread_type == THD_T_Main) {
        mutex_lock(&dmsc_thd_perf.main_thread_data_lock);
        p_item = &dmsc_thd_perf.main_thread;
        p_item_sub_0 = &dmsc_thd_perf.main_thread_read;
        p_item_sub_1 = &dmsc_thd_perf.main_thread_write;
    } else if (thread_type == THD_T_NNReq_Worker) {
        mutex_lock(&dmsc_thd_perf.nn_worker_data_lock);
        p_item = &dmsc_thd_perf.NN_worker;
        p_item_sub_0 = &dmsc_thd_perf.NN_worker_cache_hit;
        p_item_sub_1 = &dmsc_thd_perf.NN_worker_cache_miss;
    } else if (thread_type == THD_T_NNAck_Receivor) {
        mutex_lock(&dmsc_thd_perf.nn_receivor_data_lock);
        p_item = &dmsc_thd_perf.NN_receivor;
    } else if (thread_type == THD_T_DNReq_Worker) {
        mutex_lock(&dmsc_thd_perf.dn_worker_data_lock);
        p_item = &dmsc_thd_perf.DN_worker;
        p_item_sub_0 = &dmsc_thd_perf.DN_worker_read;
        p_item_sub_1 = &dmsc_thd_perf.DN_worker_write;
    } else if (thread_type == THD_T_DNAck_Receivor) {
        mutex_lock(&dmsc_thd_perf.dn_receivor_data_lock);
        p_item = &dmsc_thd_perf.DN_receivor;
        p_item_sub_0 = &dmsc_thd_perf.DN_receivor_read;
        p_item_sub_1 = &dmsc_thd_perf.DN_receivor_write;
    } else if (thread_type == THD_T_DNAck_Handler) {
        mutex_lock(&dmsc_thd_perf.dn_response_worker_data_lock);
        p_item = &dmsc_thd_perf.DN_Response_worker;
        p_item_sub_0 = &dmsc_thd_perf.DN_receivor_read;
        p_item_sub_1 = &dmsc_thd_perf.DN_receivor_write;
    } else if (thread_type == THD_T_NNCIReport_Worker) {
        mutex_lock(&dmsc_thd_perf.nn_commit_worker_data_lock);
        p_item = &dmsc_thd_perf.NN_commit_worker;
    } else {
        return -1;
    }

    if (p_item != NULL) {
        *total_time = p_item->Total_process_time;
        *total_cnt = atomic64_read(&p_item->Total_process_cnt);
        *total_period_times = p_item->Total_time_period_between_2_requests;

        *sample_start_time = p_item->Process_Sample_Start_Time;
        *sample_end_time = p_item->Process_Sample_End_Time;
        *total_sample_bytes = p_item->Process_Sample_Bytes;
        *total_sample_cnt = atomic64_read(&p_item->Process_Sample_CNT);

        *prev_iops = p_item->Process_Sample_IOPS;
        *prev_tp = p_item->Process_Sample_TP;
        ret = 0;
    } else {
        *total_time = 0;
        *total_cnt = 0;
        *total_period_times = 0;
        *sample_start_time = 0;
        *sample_end_time = 0;
        *total_sample_bytes = 0;
        *total_sample_cnt = 0;
        *prev_iops = 0;
        *prev_tp = 0;
        ret = -1;
    }


    if (p_item_sub_0 != NULL) {
        *sub_type_0_total_time = p_item_sub_0->Total_process_time;
        *sub_type_0_total_cnt = atomic64_read(&p_item_sub_0->Total_process_cnt);
    } else {
        *sub_type_0_total_time = 0;
        *sub_type_0_total_cnt = 0;
    }

    if (p_item_sub_1 != NULL) {
        *sub_type_1_total_time = p_item_sub_1->Total_process_time;
        *sub_type_1_total_cnt = atomic64_read(&p_item_sub_1->Total_process_cnt);
    } else {
        *sub_type_1_total_time = 0;
        *sub_type_1_total_cnt = 0;
    }

    if (thread_type == THD_T_Main) {
        mutex_unlock(&dmsc_thd_perf.main_thread_data_lock);
    } else if (thread_type == THD_T_NNReq_Worker) {
        mutex_unlock(&dmsc_thd_perf.nn_worker_data_lock);
    } else if (thread_type == THD_T_NNAck_Receivor) {
        mutex_unlock(&dmsc_thd_perf.nn_receivor_data_lock);
    } else if (thread_type == THD_T_DNReq_Worker) {
        mutex_unlock(&dmsc_thd_perf.dn_worker_data_lock);
    } else if (thread_type == THD_T_DNAck_Receivor) {
        mutex_unlock(&dmsc_thd_perf.dn_receivor_data_lock);
    } else if (thread_type == THD_T_DNAck_Handler) {
        mutex_unlock(&dmsc_thd_perf.dn_response_worker_data_lock);
    } else if (thread_type == THD_T_NNCIReport_Worker) {
        mutex_unlock(&dmsc_thd_perf.nn_commit_worker_data_lock);
    } else {
        return -1;
    }

    return ret;
}

static void init_thread_perf_data (void)
{
    init_thread_performance_item(&dmsc_thd_perf.main_thread);
    init_thread_performance_item(&dmsc_thd_perf.main_thread_read);
    init_thread_performance_item(&dmsc_thd_perf.main_thread_write);
    mutex_init(&dmsc_thd_perf.main_thread_data_lock);

    init_thread_performance_item(&dmsc_thd_perf.NN_worker);
    init_thread_performance_item(&dmsc_thd_perf.NN_worker_cache_hit);
    init_thread_performance_item(&dmsc_thd_perf.NN_worker_cache_miss);
    mutex_init(&dmsc_thd_perf.nn_worker_data_lock);

    init_thread_performance_item(&dmsc_thd_perf.NN_receivor);
    mutex_init(&dmsc_thd_perf.nn_receivor_data_lock);

    init_thread_performance_item(&dmsc_thd_perf.DN_worker);
    init_thread_performance_item(&dmsc_thd_perf.DN_worker_read);
    init_thread_performance_item(&dmsc_thd_perf.DN_worker_write);
    mutex_init(&dmsc_thd_perf.dn_worker_data_lock);

    init_thread_performance_item(&dmsc_thd_perf.DN_receivor);
    init_thread_performance_item(&dmsc_thd_perf.DN_receivor_read);
    init_thread_performance_item(&dmsc_thd_perf.DN_receivor_write);
    mutex_init(&dmsc_thd_perf.dn_receivor_data_lock);

    init_thread_performance_item(&dmsc_thd_perf.DN_Response_worker);
    init_thread_performance_item(
        &dmsc_thd_perf.DN_Response_worker_read);
    init_thread_performance_item(
        &dmsc_thd_perf.DN_Response_worker_write);
    mutex_init(&dmsc_thd_perf.dn_response_worker_data_lock);

    init_thread_performance_item(&dmsc_thd_perf.NN_commit_worker);
    mutex_init(&dmsc_thd_perf.nn_commit_worker_data_lock);
}

/*
 * init_thread_manager: initialize thread manager - pool, pool lock, id counter
 */
void init_thread_manager (void)
{
    INIT_LIST_HEAD(&thd_pool);
    init_rwsem(&thd_pool_lock);
    atomic_set(&thread_id_cnt, 0);
    init_thread_perf_data();
}

/*
 * release_thread_manager: release thread manager,
 *             try to stop all running threads and free thread data structure
 */
void release_thread_manager (void)
{
    stop_all_threads();
    free_all_thread_item();
}

uint32_t show_thread_info (int8_t *buffer, int32_t buff_len)
{
    uint32_t len, limit;
    uint64_t thread_total_time, thread_total_cnt, thread_total_period_time;
    uint64_t thread_sample_start_time, thread_sample_end_time;
    uint64_t thread_sample_bytes, thread_sample_cnt, thread_prev_iops;
    uint64_t thread_prev_tp;
    uint64_t thread_sub_0_total_time, thread_sub_0_total_cnt;
    uint64_t thread_sub_1_total_time, thread_sub_1_total_cnt;
    uint64_t thread_avg_latency, thread_sub_0_avg_latency;
    uint64_t thread_sub_1_avg_latency;
    uint64_t thread_curr_iops, thread_curr_tp, thread_sample_time;

    len = 0;
    limit = buff_len - 80;
    thread_sample_time = 0;

    if (dms_client_config->show_performance_data) {
        len+=sprintf(buffer+len, "\n**************** Thread Performance "
                "information ****************\n");
        /****** Main thread information ******/
        Get_Thread_Performance_Data(THD_T_Main, &thread_total_time,
                &thread_total_cnt, &thread_total_period_time,
                &thread_sample_start_time, &thread_sample_end_time,
                &thread_sample_bytes, &thread_sample_cnt, &thread_prev_iops,
                &thread_prev_tp, &thread_sub_0_total_time,
                &thread_sub_0_total_cnt, &thread_sub_1_total_time,
                &thread_sub_1_total_cnt);

        thread_avg_latency = do_divide(thread_total_time, thread_total_cnt);

        thread_sub_0_avg_latency = do_divide(thread_sub_0_total_time,
                thread_sub_0_total_cnt);

        thread_sub_1_avg_latency = do_divide(thread_sub_1_total_time,
                thread_sub_1_total_cnt);

        len += sprintf(buffer + len, "Main thread: Avg latency %llu (total time: "
                "%llu total cnt: %llu) Avg Read latency: %llu (read total time: "
                "%llu read total cnt: %llu) Avg Write latency: %llu (write "
                "total time: %llu write total cnt: %llu)\n",
                thread_avg_latency, thread_total_time, thread_total_cnt,
                thread_sub_0_avg_latency, thread_sub_0_total_time,
                thread_sub_0_total_cnt, thread_sub_1_avg_latency,
                thread_sub_1_total_time, thread_sub_1_total_cnt);

        if (thread_sample_end_time > thread_sample_start_time) {
            thread_sample_time =
                    jiffies_to_msecs(thread_sample_end_time -
                            thread_sample_start_time) / 1000L;
        } else {
            thread_sample_time = 0;
        }

        thread_curr_iops = do_divide(thread_sample_cnt, thread_sample_time);
        thread_curr_tp = do_divide(thread_sample_bytes, thread_sample_time);

        len += sprintf(buffer + len, "Main thread: Prev IOPs %llu Prev TP %llu "
                "Current IOPs %llu Current TP %llu\n", thread_prev_iops,
                thread_prev_tp, thread_curr_iops, thread_curr_tp);

        if (thread_total_cnt > 1) {
            thread_total_period_time =
                    thread_total_period_time / (thread_total_cnt - 1);
        } else {
            thread_total_period_time = 0;
        }

        len += sprintf(buffer + len, "Main thread: Avg time period between 2 "
                "requests %llu\n", thread_total_period_time);

        /****** NN worker thread information ******/
        Get_Thread_Performance_Data(THD_T_NNReq_Worker, &thread_total_time,
                &thread_total_cnt, &thread_total_period_time,
                &thread_sample_start_time,
                &thread_sample_end_time, &thread_sample_bytes,
                &thread_sample_cnt, &thread_prev_iops, &thread_prev_tp,
                &thread_sub_0_total_time, &thread_sub_0_total_cnt,
                &thread_sub_1_total_time, &thread_sub_1_total_cnt);

        thread_avg_latency = do_divide(thread_total_time, thread_total_cnt);
        thread_sub_0_avg_latency = do_divide(thread_sub_0_total_time,
                        thread_sub_0_total_cnt);
        thread_sub_1_avg_latency = do_divide(thread_sub_1_total_time,
                        thread_sub_1_total_cnt);

        len += sprintf(buffer + len, "\nNN worker thread: Avg latency %llu "
                "(total time: %llu total cnt: %llu) Avg Cache hit latency %llu "
                "(total time: %llu total cnt %llu) Avg Cache miss latency %llu "
                "(total time %llu total cnt %llu)\n",
                thread_avg_latency, thread_total_time, thread_total_cnt,
                thread_sub_0_avg_latency, thread_sub_0_total_time,
                thread_sub_0_total_cnt, thread_sub_1_avg_latency,
                thread_sub_1_total_time, thread_sub_1_total_cnt);

        if (thread_sample_end_time > thread_sample_start_time) {
            thread_sample_time =
                    jiffies_to_msecs(thread_sample_end_time -
                            thread_sample_start_time) / 1000L;
        } else {
            thread_sample_time = 0;
        }

        thread_curr_iops = do_divide(thread_sample_cnt, thread_sample_time);
        thread_curr_tp = do_divide(thread_sample_bytes, thread_sample_time);

        len += sprintf(buffer + len, "NN worker thread: Prev IOPs %llu "
                "Prev TP %llu Current IOPs %llu Current TP %llu\n",
                thread_prev_iops, thread_prev_tp, thread_curr_iops,
                thread_curr_tp);

        if (thread_total_cnt > 1) {
            thread_total_period_time =
                    thread_total_period_time / (thread_total_cnt - 1);
        } else {
            thread_total_period_time = 0;
        }

        len += sprintf(buffer + len, "NN worker thread: Avg time period between 2 "
                "requests %llu\n", thread_total_period_time);

        /****** NN receiver thread information ******/
        Get_Thread_Performance_Data(THD_T_NNAck_Receivor, &thread_total_time,
                &thread_total_cnt, &thread_total_period_time,
                &thread_sample_start_time, &thread_sample_end_time,
                &thread_sample_bytes, &thread_sample_cnt, &thread_prev_iops,
                &thread_prev_tp, &thread_sub_0_total_time,
                &thread_sub_0_total_cnt, &thread_sub_1_total_time,
                &thread_sub_1_total_cnt);

        thread_avg_latency = do_divide(thread_total_time,
                thread_total_cnt);

        len += sprintf(buffer + len, "\nNN receiver thread: Avg latency %llu "
                "(total time: %llu total cnt: %llu)\n", thread_avg_latency,
                thread_total_time, thread_total_cnt);

        if (thread_sample_end_time > thread_sample_start_time) {
            thread_sample_time = jiffies_to_msecs(thread_sample_end_time -
                    thread_sample_start_time) / 1000L;
        } else {
            thread_sample_time = 0;
        }

        thread_curr_iops = do_divide(thread_sample_cnt, thread_sample_time);
        thread_curr_tp = do_divide(thread_sample_bytes, thread_sample_time);

        len += sprintf(buffer + len, "NN receiver thread: Prev IOPs %llu Prev "
                "TP %llu Current IOPs %llu Current TP %llu\n",
                thread_prev_iops, thread_prev_tp, thread_curr_iops,
                thread_curr_tp);

        if (thread_total_cnt > 1) {
            thread_total_period_time = thread_total_period_time /
                    (thread_total_cnt - 1);
        } else {
            thread_total_period_time = 0;
        }

        len += sprintf(buffer + len, "NN receiver thread: Avg time period between 2 "
                "requests %llu\n", thread_total_period_time);

        /****** DN worker thread information ******/
        Get_Thread_Performance_Data(THD_T_DNReq_Worker, &thread_total_time,
                &thread_total_cnt, &thread_total_period_time,
                &thread_sample_start_time, &thread_sample_end_time,
                &thread_sample_bytes, &thread_sample_cnt, &thread_prev_iops,
                &thread_prev_tp, &thread_sub_0_total_time,
                &thread_sub_0_total_cnt, &thread_sub_1_total_time,
                &thread_sub_1_total_cnt);

        thread_avg_latency = do_divide(thread_total_time,
                        thread_total_cnt);
        thread_sub_0_avg_latency = do_divide(thread_sub_0_total_time,
                thread_sub_0_total_cnt);
        thread_sub_1_avg_latency = do_divide(thread_sub_1_total_time,
                thread_sub_1_total_cnt);


        len += sprintf(buffer + len, "\nDN worker thread: Avg latency %llu "
                "(total time: %llu total cnt: %llu) Avg Read latency %llu "
                "(total time: %llu total cnt %llu) Avg Write latency %llu "
                "(total time %llu total cnt %llu)\n",
                thread_avg_latency, thread_total_time, thread_total_cnt,
                thread_sub_0_avg_latency, thread_sub_0_total_time,
                thread_sub_0_total_cnt, thread_sub_1_avg_latency,
                thread_sub_1_total_time, thread_sub_1_total_cnt);

        if (thread_sample_end_time > thread_sample_start_time) {
            thread_sample_time = jiffies_to_msecs(thread_sample_end_time -
                    thread_sample_start_time) / 1000L;
        } else {
            thread_sample_time = 0;
        }

        thread_curr_iops = do_divide(thread_sample_cnt, thread_sample_time);
        thread_curr_tp = do_divide(thread_sample_bytes, thread_sample_time);

        len += sprintf(buffer + len, "DN worker thread: Prev IOPs %llu "
                "Prev TP %llu Current IOPs %llu Current TP %llu\n",
                thread_prev_iops, thread_prev_tp, thread_curr_iops,
                thread_curr_tp);

        if (thread_total_cnt > 1) {
            thread_total_period_time =
                    thread_total_period_time / (thread_total_cnt - 1);
        } else {
            thread_total_period_time = 0;
        }

        len += sprintf(buffer + len, "DN worker thread: Avg time period between 2 "
                "requests %llu\n", thread_total_period_time);

        /****** DN receiver thread information ******/
        Get_Thread_Performance_Data(THD_T_DNAck_Receivor, &thread_total_time,
                &thread_total_cnt, &thread_total_period_time,
                &thread_sample_start_time, &thread_sample_end_time,
                &thread_sample_bytes, &thread_sample_cnt, &thread_prev_iops,
                &thread_prev_tp, &thread_sub_0_total_time,
                &thread_sub_0_total_cnt, &thread_sub_1_total_time,
                &thread_sub_1_total_cnt);

        thread_avg_latency = do_divide(thread_total_time,
                thread_total_cnt);
        thread_sub_0_avg_latency = do_divide(thread_sub_0_total_time,
                thread_sub_0_total_cnt);
        thread_sub_1_avg_latency = do_divide(thread_sub_1_total_time,
                thread_sub_1_total_cnt);

        len += sprintf(buffer + len, "\nDN receiver thread: Avg latency %llu "
                "(total time: %llu total cnt: %llu) Avg Read latency %llu "
                "(total time: %llu total cnt %llu) Avg Write latency %llu "
                "(total time %llu total cnt %llu)\n",
                thread_avg_latency, thread_total_time, thread_total_cnt,
                thread_sub_0_avg_latency, thread_sub_0_total_time,
                thread_sub_0_total_cnt, thread_sub_1_avg_latency,
                thread_sub_1_total_time, thread_sub_1_total_cnt);

        if (thread_sample_end_time > thread_sample_start_time) {
            thread_sample_time =
                    jiffies_to_msecs(thread_sample_end_time -
                            thread_sample_start_time) / 1000L;
        } else {
            thread_sample_time = 0;
        }

        thread_curr_iops = do_divide(thread_sample_cnt, thread_sample_time);
        thread_curr_tp = do_divide(thread_sample_bytes, thread_sample_time);

        len += sprintf(buffer + len, "DN receiver thread: Prev IOPs %llu "
                "Prev TP %llu Current IOPs %llu Current TP %llu\n",
                thread_prev_iops, thread_prev_tp, thread_curr_iops,
                thread_curr_tp);

        if (thread_total_cnt > 1) {
            thread_total_period_time =
                    thread_total_period_time / (thread_total_cnt - 1);
        } else {
            thread_total_period_time = 0;
        }
        len += sprintf(buffer + len, "DN worker thread: Avg time period between 2 "
                "requests %llu\n", thread_total_period_time);

        /****** DN response thread information ******/
        Get_Thread_Performance_Data(THD_T_DNAck_Handler,
                &thread_total_time, &thread_total_cnt, &thread_total_period_time,
                &thread_sample_start_time, &thread_sample_end_time,
                &thread_sample_bytes, &thread_sample_cnt, &thread_prev_iops,
                &thread_prev_tp, &thread_sub_0_total_time,
                &thread_sub_0_total_cnt, &thread_sub_1_total_time,
                &thread_sub_1_total_cnt);

        thread_avg_latency = do_divide(thread_total_time,
                thread_total_cnt);
        thread_sub_0_avg_latency = do_divide(thread_sub_0_total_time,
                thread_sub_0_total_cnt);
        thread_sub_1_avg_latency = do_divide(thread_sub_1_total_time,
                thread_sub_1_total_cnt);

        len+=sprintf(buffer+len, "\nDN response worker thread: Avg latency %llu "
                "(total time: %llu total cnt: %llu) Avg Read latency %llu "
                "(total time: %llu total cnt %llu) Avg Write latency %llu "
                "(total time %llu total cnt %llu)\n", thread_avg_latency,
                thread_total_time, thread_total_cnt, thread_sub_0_avg_latency,
                thread_sub_0_total_time, thread_sub_0_total_cnt,
                thread_sub_1_avg_latency, thread_sub_1_total_time,
                thread_sub_1_total_cnt);

        if (thread_sample_end_time > thread_sample_start_time) {
            thread_sample_time =
                    jiffies_to_msecs(thread_sample_end_time -
                            thread_sample_start_time) / 1000L;
        } else {
            thread_sample_time = 0;
        }

        thread_curr_iops = do_divide(thread_sample_cnt, thread_sample_time);
        thread_curr_tp = do_divide(thread_sample_bytes, thread_sample_time);

        len += sprintf(buffer + len, "DN response worker thread: Prev IOPs %llu "
                "Prev TP %llu Current IOPs %llu Current TP %llu\n",
                thread_prev_iops, thread_prev_tp, thread_curr_iops,
                thread_curr_tp);

        if (thread_total_cnt > 1) {
            thread_total_period_time =
                    thread_total_period_time / (thread_total_cnt - 1);
        } else {
            thread_total_period_time = 0;
        }
        len += sprintf(buffer + len, "DN response worker thread: Avg time "
                "period between 2 requests %llu\n", thread_total_period_time);

        /****** NN commit thread information ******/
        Get_Thread_Performance_Data(THD_T_NNCIReport_Worker,
                &thread_total_time, &thread_total_cnt, &thread_total_period_time,
                &thread_sample_start_time, &thread_sample_end_time,
                &thread_sample_bytes, &thread_sample_cnt,
                &thread_prev_iops, &thread_prev_tp, &thread_sub_0_total_time,
                &thread_sub_0_total_cnt, &thread_sub_1_total_time,
                &thread_sub_1_total_cnt);

        thread_avg_latency = do_divide(thread_total_time, thread_total_cnt);


        len += sprintf(buffer + len, "\nNN commit thread: Avg latency %llu "
                "(total time: %llu total cnt: %llu)\n", thread_avg_latency,
                thread_total_time, thread_total_cnt);

        if (thread_sample_end_time > thread_sample_start_time) {
            thread_sample_time =
                    jiffies_to_msecs(thread_sample_end_time -
                            thread_sample_start_time) / 1000L;
        } else {
            thread_sample_time = 0;
        }

        thread_curr_iops = do_divide(thread_sample_cnt, thread_sample_time);
        thread_curr_tp = do_divide(thread_sample_bytes, thread_sample_time);

        len += sprintf(buffer + len, "NN commit thread: Prev IOPs %llu "
                "Prev TP %llu Current IOPs %llu Current TP %llu\n",
                thread_prev_iops, thread_prev_tp, thread_curr_iops,
                thread_curr_tp);

        if (thread_total_cnt > 1) {
            thread_total_period_time =
                    thread_total_period_time / (thread_total_cnt - 1);
        } else {
            thread_total_period_time = 0;
        }
        len += sprintf(buffer + len, "NN commit thread: Avg time period "
                "between 2 requests %llu\n", thread_total_period_time);
    }

    return len;
}
