/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * socket_free_worker.h
 *
 * For release socket safely, we using a central thread to release all kernel socket.
 * In some case, a cs_wrapper_t be referenced and stuck at kernel API.
 * When someone free cs_wrapper and set is as NULL, the thread that hold the refernece
 * won't become NULL and cause crash. When the reference count <= 0, we can free it
 */
#include <linux/delay.h>
#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/dms_client_mm.h"
#include "../common/dms_kernel_version.h"
#include "../common/thread_manager.h"
#include "conn_manager_export.h"
#include "conn_state_fsm.h"
#include "conn_manager.h"
#include "conn_sock_api.h"

/*
 * NOTE: This function might be called at socket rx call back threads,
 * we need use spin_lock_irqsave (?)
 */
void enq_release_sock (cs_wrapper_t *sk_wraper, sk_release_mgr_t *mymgr)
{
    unsigned long flag;

    spin_lock_irqsave(&mymgr->sock_rel_plock, flag);
    list_add_tail(&sk_wraper->list_free, &mymgr->sock_rel_pools);
    spin_unlock_irqrestore(&mymgr->sock_rel_plock, flag);

    atomic_inc(&mymgr->num_socks);

    if (waitqueue_active(&mymgr->sock_rel_waitq)) {
        wake_up_interruptible_all(&mymgr->sock_rel_waitq);
    }
}

static cs_wrapper_t *deq_release_sock (sk_release_mgr_t *mymgr)
{
    cs_wrapper_t *sk_wrapper;
    unsigned long flag;

    spin_lock_irqsave(&mymgr->sock_rel_plock, flag);

    sk_wrapper = list_first_entry_or_null(&mymgr->sock_rel_pools, cs_wrapper_t, list_free);

    if (NULL != sk_wrapper) {
        list_del(&sk_wrapper->list_free);
        atomic_dec(&mymgr->num_socks);
    }

    spin_unlock_irqrestore(&mymgr->sock_rel_plock, flag);

    return sk_wrapper;
}

static void _check_sock_ref (cs_wrapper_t *cur_wrapper)
{
    int32_t retry_free;

    retry_free = 0;
    while (atomic_read(&cur_wrapper->sock_ref_cnt) > 0) {
        retry_free++;
        msleep(100);
        if (retry_free % 100 == 0) {
            dms_printk(LOG_LVL_INFO,
                "DMSC INFO someone ref the socket retry_cnt %d ref_cnt %d\n",
                retry_free, atomic_read(&cur_wrapper->sock_ref_cnt));
        }
    }
}

static int sock_rel_worker_fn (void *data)
{
    dmsc_thread_t *t_data;
    sk_release_mgr_t *sk_relMgr;
    cs_wrapper_t *cur_wrapper;
    int32_t ret;

    t_data = (dmsc_thread_t *)data;
    sk_relMgr = (sk_release_mgr_t *)t_data->usr_data;
    atomic_inc(&sk_relMgr->num_running_worker);

    ret = 0;

    dms_printk(LOG_LVL_INFO, "socket release worker start\n");
    while (t_data->running) {
        wait_event_interruptible(sk_relMgr->sock_rel_waitq,
                (atomic_read(&sk_relMgr->num_socks) > 0) || !t_data->running);

        dms_printk(LOG_LVL_INFO, "socket release worker wakeup %d %d\n",
                atomic_read(&sk_relMgr->num_socks), t_data->running);
        cur_wrapper = deq_release_sock(sk_relMgr);
        if (IS_ERR_OR_NULL(cur_wrapper)) {
            dms_printk(LOG_LVL_INFO, "socket release worker get null socket %d %d\n",
                    atomic_read(&sk_relMgr->num_socks), t_data->running);
            continue;
        }

        _check_sock_ref(cur_wrapper);
        free_sock_wrapper(cur_wrapper);
        dms_printk(LOG_LVL_INFO, "socket release worker free socket DONE %d %d\n",
                atomic_read(&sk_relMgr->num_socks), t_data->running);
    }

    dms_printk(LOG_LVL_INFO, "socket release worker stop %d %d\n",
            atomic_read(&sk_relMgr->num_socks), t_data->running);

    atomic_dec(&sk_relMgr->num_running_worker);

    return ret;
}

int32_t init_sock_release_mgr (sk_release_mgr_t *mymgr)
{
    int32_t ret;

    ret = 0;
    dms_printk(LOG_LVL_INFO, "DMSC INFO init socket release manager\n");
    INIT_LIST_HEAD(&mymgr->sock_rel_pools);
    spin_lock_init(&(mymgr->sock_rel_plock));
    atomic_set(&mymgr->num_socks, 0);
    init_waitqueue_head(&mymgr->sock_rel_waitq);
    atomic_set(&mymgr->num_running_worker, 0);

    mymgr->sk_free_worker = create_worker_thread("dms_skrel_wker",
            &mymgr->sock_rel_waitq, mymgr, sock_rel_worker_fn);
    if (IS_ERR_OR_NULL(mymgr->sk_free_worker)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN kthread dms_io_worker creation fail\n");
        DMS_WARN_ON(true);
        ret = -ENOMEM;
    }

    dms_printk(LOG_LVL_INFO, "DMSC INFO init socket release manager DONE\n");

    return ret;
}

static void _free_all_socket (sk_release_mgr_t *mymgr)
{
    cs_wrapper_t *cur_wrapper;

    dms_printk(LOG_LVL_INFO, "DMSC INFO stop socket release manager\n");

    while ((cur_wrapper = deq_release_sock(mymgr)) != NULL) {
        _check_sock_ref(cur_wrapper);

        free_sock_wrapper(cur_wrapper);
        cur_wrapper = NULL;
    }

    if (atomic_read(&mymgr->num_socks) != 0) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN linger socket un-release %d\n",
                atomic_read(&mymgr->num_socks));
        DMS_WARN_ON(true);
    }

    dms_printk(LOG_LVL_INFO, "DMSC INFO stop socket release manager DONE\n");
}

void rel_sock_release_mgr (sk_release_mgr_t *mymgr)
{
    int32_t count;

    if (stop_worker_thread(mymgr->sk_free_worker)) {
        dms_printk(LOG_LVL_WARN, "fail to stop sk_free_worker\n");
        DMS_WARN_ON(true);
    }

    count = 0;
    while (atomic_read(&mymgr->num_running_worker) != 0) {
        msleep(100);
        count++;

        if (count != 0 && count % 10 == 0) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN fail stop sk free worker %d thread running "
                    "(wait count %d)\n",
                    atomic_read(&mymgr->num_running_worker), count);
        }
    }

    _free_all_socket(mymgr);
}
