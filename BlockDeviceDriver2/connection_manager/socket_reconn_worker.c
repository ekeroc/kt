/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * socket_reconn_worker.c
 *
 * cr task state machine
 *
 * INIT -> WAITING <--> Connecting --> Stop
 *           |
 *           |---> Cancel------------> Stop
 *
 * TODO: We should wrapper the state machine of cr task into independent function
 */

#include <linux/delay.h>
#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/discoC_mem_manager.h"
#include "../common/dms_client_mm.h"
#include "../common/dms_kernel_version.h"
#include "../common/thread_manager.h"
#include "conn_manager_export.h"
#include "conn_state_fsm.h"
#include "conn_manager.h"
#include "conn_sock_api.h"
#include "socket_reconn_worker.h"
#include "socket_reconn_worker_private.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
static void connection_retry_handler (struct work_struct *work)
{
    conn_retry_handler_common((worker_t *)work);
}
#else
static void connection_retry_handler (void *work)
{
    conn_retry_handler_common((worker_t *)work);
}
#endif

static connRetry_task_t *_find_cr_task_lockless (uint32_t ipv4addr, uint32_t port)
{
    connRetry_task_t *cur, *next, *match;

    match = NULL;

    list_for_each_entry_safe (cur, next, &connR_Mgr.connRetry_queue, list_CRPool) {
        if (cur->ipv4addr == ipv4addr && cur->port == port) {
            match = cur;
            break;
        }
    }

    return match;
}

static void _add_cr_task_lockless (connRetry_task_t *cr_tsk)
{
    list_add_tail(&cr_tsk->list_CRPool, &connR_Mgr.connRetry_queue);
    atomic_inc(&connR_Mgr.cnt_cr_tasks);
}

static void _rm_cr_task_lockless (connRetry_task_t *cr_tsk)
{
    list_del(&cr_tsk->list_CRPool);
    atomic_dec(&connR_Mgr.cnt_cr_tasks);
}

static void _schedule_cr_task (connRetry_task_t *cr_tsk)
{
    struct workqueue_struct *curr_workq;
    uint64_t retry_dur_sec;

    retry_dur_sec = 0;
    if (time_after64(jiffies_64, cr_tsk->crTask_start_t)) {
        retry_dur_sec = jiffies_to_msecs(jiffies_64 - cr_tsk->crTask_start_t);
        retry_dur_sec = retry_dur_sec / 1000;
    }

    DMS_INIT_DELAYED_WORK(&(cr_tsk->crTask_work), connection_retry_handler,
            (void *)cr_tsk);
    /*
     *   The return value is nonzero if the work_struct was actually added to queue
     *   (otherwise, it may have already been there and will not be added in second time).
     *
     *   NOTE: queue_delayed_work using local_irq_save, so maybe we should put this
     *         function inside state lock
     */
    curr_workq = connR_Mgr.conn_retry_workq[cr_tsk->crTask_workq_idx];
    queue_delayed_work(curr_workq,
            (delayed_worker_t *)(&(cr_tsk->crTask_work)),
            connR_Mgr.connRetry_INTVL_HZ);
}

static void _conn_retry_success (connRetry_task_t *cr_tsk)
{
    int32_t ret;

    ret = 0;
    if (!IS_ERR_OR_NULL(cr_tsk->cr_done_fn)) {
        ret = cr_tsk->cr_done_fn(cr_tsk->ipv4addr, cr_tsk->port, cr_tsk->connRetry_sock);
    } else {
        dms_printk(LOG_LVL_WARN, "DMSC WARN null sk_retryDone_func %s:%d\n",
                cr_tsk->ipv4addr_str,
                cr_tsk->port);
        ret = -ENXIO;
    }

    down_write(&connR_Mgr.connRetry_qlock);
    _rm_cr_task_lockless(cr_tsk);

    spin_lock(&cr_tsk->crTask_state_lock);
    cr_tsk->crTask_state = CR_STAT_stop;
    spin_unlock(&cr_tsk->crTask_state_lock);

    up_write(&connR_Mgr.connRetry_qlock);

    if (ret) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN fail exec cr done func %s:%d\n",
                cr_tsk->ipv4addr_str, cr_tsk->port);
        free_sock_wrapper(cr_tsk->connRetry_sock);
    }

    cr_tsk->connRetry_sock = NULL;
    discoC_mem_free(cr_tsk);
}

/*
 * Description: if state is connecting, don't do anything until become waiting
 */
static void conn_retry_handler_common (worker_t *retry_data)
{
    connRetry_task_t *cr_tsk;
    struct socket *sock;
    int32_t ret;
    bool stop_retry;

    cr_tsk = (connRetry_task_t *)(retry_data->data);
    if (IS_ERR_OR_NULL(cr_tsk) || IS_ERR_OR_NULL(cr_tsk->connRetry_sock)) {
        dms_printk(LOG_LVL_WARN,
                "DMS INFO CR: null para list when retry conn\n");
        DMS_WARN_ON(true);
        return;
    }

    stop_retry = false;

    down_write(&connR_Mgr.connRetry_qlock);

    do {
        if (!test_bit(BIT_Running,
                (const volatile void *)(&(connR_Mgr.connRetry_mgr_state)))) {
            stop_retry = true;
            break;
        }

        spin_lock(&cr_tsk->crTask_state_lock);
        if (cr_tsk->crTask_state == CR_STAT_Cancel ||
                cr_tsk->crTask_state == CR_STAT_stop) {
            stop_retry = true;
        } else {
            stop_retry = false;
            cr_tsk->crTask_state = CR_STAT_Connecting;
        }
        spin_unlock(&cr_tsk->crTask_state_lock);
    } while (0);

    up_write(&connR_Mgr.connRetry_qlock);

    if (stop_retry) {
        return;
    }

    sock = cr_tsk->connRetry_sock->sock;

    dms_printk(LOG_LVL_INFO, "DMSC INFO conn retry start %s:%d\n",
            cr_tsk->ipv4addr_str, cr_tsk->port);

    ret = sock_connect_target(cr_tsk->ipv4addr, cr_tsk->port, sock);

    if (ret == 0 || ret == -EALREADY) {
        dms_printk(LOG_LVL_INFO,
                "DMSC INFO conn retry connect ok %s:%d\n",
                cr_tsk->ipv4addr_str, cr_tsk->port);
        _conn_retry_success(cr_tsk);
    } else {
        dms_printk(LOG_LVL_DEBUG,
                "DMSC DEBUG conn retry fail re-schedule to workq sk->state = %d\n",
                sock->sk->sk_state);
        _schedule_cr_task(cr_tsk);
        spin_lock(&cr_tsk->crTask_state_lock);
        if (cr_tsk->crTask_state == CR_STAT_Connecting) {
            cr_tsk->crTask_state = CR_STAT_Waiting;
        }
        spin_unlock(&cr_tsk->crTask_state_lock);
    }
}

static connRetry_task_t *_create_cr_task (uint32_t ipv4addr, uint32_t port,
        conn_retry_done_fn *_cr_done_fn, void *user_data)
{
    connRetry_task_t *cr_task;

    do {
        cr_task = discoC_mem_alloc(sizeof(connRetry_task_t), GFP_NOWAIT | GFP_ATOMIC);
        if (IS_ERR_OR_NULL(cr_task)) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN fail alloc mem for cr task\n");
            break;
        }

        memset(cr_task, 0, sizeof(connRetry_task_t));
        ccma_inet_aton(ipv4addr, cr_task->ipv4addr_str, IPV4ADDR_NM_LEN);
        cr_task->ipv4addr = ipv4addr;
        cr_task->port = port;
        INIT_LIST_HEAD(&cr_task->list_CRPool);
        cr_task->connRetry_sock = alloc_sock_wrapper(ipv4addr, port);
        if (IS_ERR_OR_NULL(cr_task->connRetry_sock)) {
            discoC_mem_free(cr_task);
            cr_task = NULL;
            dms_printk(LOG_LVL_WARN, "DMSC WARN fail create sock for cr task "
                    "%s:%d\n", cr_task->ipv4addr_str, port);
            break;
        }

        spin_lock_init(&cr_task->crTask_state_lock);
        cr_task->crTask_state = CR_STAT_INIT;

        /*
         * 20150108 : Lego : why we add an initial here, we encounter an issue
         *   that thread 1 add task to retry queue and then initial it later,
         *        thread 2 execute release_connection_pool (user call shutdown),
         *        and try to stop timer but CPU hang at try_to_del_timer_sync
         */
        DMS_INIT_DELAYED_WORK(&(cr_task->crTask_work), connection_retry_handler,
                (void *)cr_task);
        cr_task->crTask_workq_idx = ipv4addr % NUM_CONNRETRY_WORKQ;
        cr_task->cr_done_fn = _cr_done_fn;
        cr_task->user_data = user_data;
        cr_task->crTask_start_t = jiffies_64;
    } while (0);

    return cr_task;
}

/*
 * this API interface to the functions who want to schedule a connection retry.
 * we lock this function due to SMP and multi-thread env may add duplicate entry.
 */
int32_t add_conn_retry_task (uint32_t ipv4addr, uint32_t port,
        conn_retry_done_fn *_cr_done_fn, void *user_data)
{
    connRetry_task_t *cr_tsk;
    int32_t ret;

    ret = 0;

    if (unlikely(IS_ERR_OR_NULL(_cr_done_fn))) {
        dms_printk(LOG_LVL_WARN,
                    "DMSC WARN null cr done fn for add retry task\n");
        DMS_WARN_ON(true);
        return -EINVAL;
    }

    down_write(&connR_Mgr.connRetry_qlock);

    if (!test_bit(BIT_Running,
            (const volatile void *)(&(connR_Mgr.connRetry_mgr_state)))) {
        dms_printk(LOG_LVL_INFO,
                "DMSC INFO fail add conn retry task manager no running\n");
        up_write(&connR_Mgr.connRetry_qlock);
        return -EPERM;
    }

    if (!IS_ERR_OR_NULL(_find_cr_task_lockless(ipv4addr, port))) {
        dms_printk(LOG_LVL_INFO,
                "DMSC INFO fail add conn retry task conn retrying\n");
        up_write(&connR_Mgr.connRetry_qlock);
        return -EPERM;
    }

    cr_tsk = _create_cr_task(ipv4addr, port, _cr_done_fn, user_data);
    if (IS_ERR_OR_NULL(cr_tsk)) {
        dms_printk(LOG_LVL_INFO, "DMSC INFO fail alloc mem conn retry task\n");
        up_write(&connR_Mgr.connRetry_qlock);
        return -ENOMEM;
    }

    _add_cr_task_lockless(cr_tsk);

    spin_lock(&cr_tsk->crTask_state_lock);
    cr_tsk->crTask_state = CR_STAT_Waiting;
    spin_unlock(&cr_tsk->crTask_state_lock);

    _schedule_cr_task(cr_tsk);
    up_write(&connR_Mgr.connRetry_qlock);

    return ret;
}

static void _cancel_cr_delay_work (connRetry_task_t *cr_task)
{
    worker_t *mywork;

    mywork = &(cr_task->crTask_work);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
    if (!cancel_delayed_work_sync((delayed_worker_t *)mywork)) {
        flush_delayed_work((delayed_worker_t *)mywork);
    }
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 23)
    if (!cancel_delayed_work_sync((delayed_worker_t *)mywork)) {
        flush_workqueue(connR_Mgr.conn_retry_workq[cr_task->crTask_workq_idx]);
    }
#else
    if (cancel_delayed_work((delayed_worker_t*)mywork) == 0) {
        flush_workqueue(connR_Mgr.conn_retry_workq[cr_task->crTask_workq_idx]);
    }
#endif
}

int32_t cancel_conn_retry_task (uint32_t ipv4addr, uint32_t port)
{
    connRetry_task_t *cr_task;
    int32_t ret, i, action;

    ret = 0;

    /*
     * step 1: we must wait the state not at connecting since cr_task might be after
     *         reconn success.
     */
    action = CANCAL_CR_DO_NOTHING;
    for (i = 0; i < CANCEL_CR_CNT_WAIT_RETRY; i++) {
        action = CANCAL_CR_KEEP_RETRY;

        down_write(&connR_Mgr.connRetry_qlock);

        cr_task = _find_cr_task_lockless(ipv4addr, port);
        if (IS_ERR_OR_NULL(cr_task)) {
            action = CANCAL_CR_DO_NOTHING;
        } else {
            spin_lock(&cr_task->crTask_state_lock);
            if (cr_task->crTask_state == CR_STAT_Waiting) {
                cr_task->crTask_state = CR_STAT_Cancel;
                action = CANCAL_CR_FREE_TASK;
                _rm_cr_task_lockless(cr_task);
            } else if (cr_task->crTask_state == CR_STAT_Connecting) {
                action = CANCAL_CR_KEEP_RETRY;
            } else {
                action = CANCAL_CR_DO_NOTHING;
            }
            spin_unlock(&cr_task->crTask_state_lock);
        }

        up_write(&connR_Mgr.connRetry_qlock);

        if (action != CANCAL_CR_KEEP_RETRY) {
            break;
        }

        msleep(CANCAL_CR_TIME_WAIT_PERIOD);
    }

    if (action == CANCAL_CR_FREE_TASK) {
        _cancel_cr_delay_work(cr_task);
        free_sock_wrapper(cr_task->connRetry_sock);
        cr_task->connRetry_sock = NULL;
        discoC_mem_free(cr_task);

        ret = 0;
    } else if (action == CANCAL_CR_KEEP_RETRY) {
        ret = -EBUSY;
        dms_printk(LOG_LVL_WARN, "DMSC WARN fail cancel cr task %d:%d\n",
                ipv4addr, port);
    }


    return ret;
}

void init_conn_retry_manager (void)
{
#define NM_LEN_WORKQ    32
    int8_t nm_workq[NM_LEN_WORKQ];
    int32_t i;

    connR_Mgr.connRetry_mgr_state = 0;
    INIT_LIST_HEAD(&connR_Mgr.connRetry_queue);
    init_rwsem(&connR_Mgr.connRetry_qlock);
    connR_Mgr.connRetry_INTVL_HZ = HZ * CONN_RETRY_INTVL;
    atomic_set(&connR_Mgr.cnt_cr_tasks, 0);

    for (i = 0; i < NUM_CONNRETRY_WORKQ; i++) {
        memset(nm_workq, '\0', NM_LEN_WORKQ);
        sprintf(nm_workq, "dms_connR_workq%d", i);
        connR_Mgr.conn_retry_workq[i] = create_workqueue(nm_workq);
    }

    set_bit(BIT_Running, (volatile void *)(&(connR_Mgr.connRetry_mgr_state)));
}

static void _cancel_all_retry_task (void)
{
    connRetry_task_t *cur, *next;
    int32_t ipv4addr, port;
    bool do_cancel;

PEEK_NEXT_TASK:
    ipv4addr = 0;
    port = 0;
    do_cancel = false;

    down_write(&connR_Mgr.connRetry_qlock);

    list_for_each_entry_safe (cur, next, &connR_Mgr.connRetry_queue, list_CRPool) {
        ipv4addr = cur->ipv4addr;
        port = cur->port;
        do_cancel = true;
    }
    up_write(&connR_Mgr.connRetry_qlock);

    if (do_cancel) {
        if (cancel_conn_retry_task(ipv4addr, port)) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN fail cancel cr task when stop all\n");
        } else {
            goto PEEK_NEXT_TASK;
        }
    }
}

void rel_conn_retry_manager (void)
{
    int32_t i;

    clear_bit(BIT_Running, (volatile void *)(&(connR_Mgr.connRetry_mgr_state)));
    _cancel_all_retry_task();

    for (i = 0; i < NUM_CONNRETRY_WORKQ; i++) {
        if (IS_ERR_OR_NULL(connR_Mgr.conn_retry_workq[i])) {
            continue;
        }

        flush_workqueue(connR_Mgr.conn_retry_workq[i]);
        destroy_workqueue(connR_Mgr.conn_retry_workq[i]);
    }
}
