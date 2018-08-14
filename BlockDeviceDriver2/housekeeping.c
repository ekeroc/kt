/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * housekeeping.c
 *
 */

#include <linux/version.h>
#include <linux/blkdev.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <net/sock.h>
#include <linux/swap.h>
#include "common/discoC_sys_def.h"
#include "common/common_util.h"
#include "common/dms_kernel_version.h"
#include "common/discoC_mem_manager.h"
#include "metadata_manager/metadata_manager.h"
#include "common/dms_client_mm.h"
#include "discoDN_client/discoC_DNC_Manager_export.h"
#include "payload_manager/payload_manager_export.h"
#include "io_manager/discoC_IO_Manager.h"
#include "housekeeping.h"
#include "housekeeping_private.h"
#include "common/thread_manager.h"
#include "connection_manager/conn_manager_export.h"
#include "discoNN_client/discoC_NNC_Manager_api.h"
#include "vdisk_manager/volume_manager.h"
#include "discoNN_client/discoC_NNC_ovw.h"
#include "config/dmsc_config.h"
#include "flowcontrol/FlowControl.h"

static void release_task_data (dmsc_task_t *tsk)
{
    if (IS_ERR_OR_NULL(tsk)) {
        DMS_WARN_ON(true);
        dms_printk(LOG_LVL_WARN, "DMSC WARN free a null task\n");
        return;
    }

#if 0
    switch (tsk->task_type) {
        case TASK_CONN_RETRY:
        case TASK_DN_CONN_BACK_REQ_RETRY:
        case TASK_NN_CONN_BACK_REQ_RETRY:
        case TASK_NN_OVW_CONN_BACK_REQ_RETRY:
        case TASK_NN_CONN_RESET:
        case TASK_DN_CONN_RESET:
            tsk->task_data = NULL;
            discoC_mem_free(tsk);
            break;

        default:
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN unknown task type : %d\n", tsk->task_type);
            discoC_mem_free(tsk->task_data);
            break;
    }
#endif

    tsk->task_data = NULL;
}

static dmsc_task_t *dequeue_hk_task (void)
{
    dmsc_task_t *first, *cur, *next;
    bool list_bug;

    first = NULL;
	list_bug = false;
    spin_lock(&hk_task_qlock);

    list_for_each_entry_safe (cur, next, &hk_task_queue, list_hkq) {
        if (IS_ERR_OR_NULL(cur)) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN hk list broken\n");
            DMS_WARN_ON(true);
            break;
        }

        first = cur;
        if (dms_list_del(&first->list_hkq)) {
            list_bug = true;
            continue;
        }

        atomic_dec(&num_hk_tasks);
        break;
    }

    spin_unlock(&hk_task_qlock);

    if (unlikely(list_bug)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN list_del chk fail %s %d\n",
                __func__, __LINE__);
        DMS_WARN_ON(true);
        msleep(1000);
    }

    return first;
}

static void _post_req_retry (discoC_conn_t *conn)
{
    set_res_available(conn->ipaddr, conn->port, true);
    dms_printk(LOG_LVL_INFO, "DMSC INFO set res (%s:%d) as available\n",
            conn->ipv4addr_str, conn->port);
}

static int hk_worker (void *thread_data)
{
    dmsc_task_t *c_task;
    discoC_conn_t *myConn;
    dmsc_thread_t *t_data = (dmsc_thread_t *)thread_data;
    int32_t ret;

    while (t_data->running) {
        ret = wait_event_interruptible(*(t_data->ctl_wq),
                atomic_read(&num_hk_tasks) > 0 || !(t_data->running));

        //DMS_WARN_ON(ret == -ERESTARTSYS);

        c_task = dequeue_hk_task();

        if (IS_ERR_OR_NULL(c_task)) {
            continue;
        }

        switch (c_task->task_type) {
        case TASK_CONN_RETRY:
            dms_printk(LOG_LVL_INFO,
                    "DMSC INFO do conn retry task\n");
            myConn = (discoC_conn_t *)c_task->task_data;
            myConn->connOPIntf.reconnect(myConn);
            dms_printk(LOG_LVL_INFO,
                    "DMSC INFO do conn retry task DONE\n");
            break;

        case TASK_DN_CONN_BACK_REQ_RETRY:
            myConn = (discoC_conn_t *)c_task->task_data;
            dms_printk(LOG_LVL_INFO,
                    "DMSC INFO do an DN conn back req retry %s:%d\n",
                    myConn->ipv4addr_str, myConn->port);
            DNClientMgr_resend_DNUDReq(myConn->ipaddr, myConn->port);
            dms_printk(LOG_LVL_INFO,
                    "DMSC INFO do an DN conn back req retry DONE\n");
            _post_req_retry((discoC_conn_t *)c_task->task_data);

            break;

        case TASK_NN_CONN_BACK_REQ_RETRY:
            myConn = (discoC_conn_t *)c_task->task_data;
            dms_printk(LOG_LVL_INFO,
                    "DMSC INFO do an NN conn back req retry %s:%d\n",
                    myConn->ipv4addr_str, myConn->port);
            NNClientMgr_retry_NNMDReq(myConn->ipaddr, myConn->port);
            dms_printk(LOG_LVL_INFO,
                    "DMSC INFO do an NN conn back req retry DONE\n");
            _post_req_retry((discoC_conn_t *)c_task->task_data);

            break;

        case TASK_NN_OVW_CONN_BACK_REQ_RETRY:
            dms_printk(LOG_LVL_INFO,
                    "DMSC INFO do an NN OVW conn back req retry\n");
            retry_nn_ovw_req((discoC_conn_t *)c_task->task_data);
            dms_printk(LOG_LVL_INFO,
                    "DMSC INFO do an NN OVW conn back req retry DONE\n");
            _post_req_retry((discoC_conn_t *)c_task->task_data);

            break;

        case TASK_NN_CONN_RESET:
        case TASK_DN_CONN_RESET:
            dms_printk(LOG_LVL_INFO, "DMSC INFO do an conn reset\n");
            myConn = (discoC_conn_t *)c_task->task_data;
            myConn->connOPIntf.resetConn((discoC_conn_t *)c_task->task_data);
            dms_printk(LOG_LVL_INFO, "DMSC INFO do an conn reset DONE\n");
            break;

        default:
            dms_printk(LOG_LVL_WARN, "DMSC WARN unknown task type : %d\n",
                    c_task->task_type);
            DMS_WARN_ON(true);
            break;
        }

        c_task->task_data = NULL;
        discoC_mem_free(c_task);
        c_task = NULL;
    }

    return 0;
}

bool add_hk_task (task_type_t task_type, uint64_t *task_data)
{
    dmsc_task_t *new_task;
    bool list_bug;

    if (!dmsc_hk_working) {
        return false;
    }

    new_task = (dmsc_task_t *)discoC_mem_alloc(sizeof(dmsc_task_t),
            GFP_NOWAIT | GFP_ATOMIC);
    if (IS_ERR_OR_NULL(new_task)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN fail alloc mem for hk task: type=%d\n", task_type);
        DMS_WARN_ON(true);
        return false;
    }

    list_bug = false;
    new_task->task_type = task_type;
    new_task->task_data = task_data;
    INIT_LIST_HEAD(&new_task->list_hkq);

    spin_lock(&hk_task_qlock);
    if (dms_list_add_tail(&new_task->list_hkq, &hk_task_queue)) {
        list_bug = true;
    } else {
        atomic_inc(&num_hk_tasks);
    }
    spin_unlock(&hk_task_qlock);

    if (atomic_read(&num_hk_tasks) > 0) {
        if (waitqueue_active(&hk_task_waitq)) {
            wake_up_interruptible_all(&hk_task_waitq);
        }
    }

    if (list_bug) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN list_add_tail chk fail %s %d\n",
                __func__, __LINE__);
        DMS_WARN_ON(true);
        //Lego: Cannot sleep here, bcz sock_rx_err handle function works at softirq
    }

    return true;
}

static void init_hk_thread (void)
{
    if (IS_ERR_OR_NULL(create_worker_thread("dms_hk_worker", &hk_task_waitq,
            NULL, hk_worker))) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN kthread dms_hk_worker creation fail\n");
        DMS_WARN_ON(true);
        return;
    }
}

void init_housekeeping_res (void)
{
    dmsc_hk_working = true;
    spin_lock_init(&hk_task_qlock);
    INIT_LIST_HEAD(&hk_task_queue);
    atomic_set(&num_hk_tasks, 0);
    init_waitqueue_head(&hk_task_waitq);
    init_hk_thread();
}

void rel_housekeeping_res (void)
{
    dmsc_task_t *cur, *next, *tmp;
    bool list_bug;

    dmsc_hk_working = false;
    list_bug = false;

release_hk_res:

    tmp = NULL;
    spin_lock(&hk_task_qlock);

    list_for_each_entry_safe (cur, next, &hk_task_queue, list_hkq) {
        if (IS_ERR_OR_NULL(cur)) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN housekeeping task queue list broken\n");
            DMS_WARN_ON(true);
            break;
        }

        if (dms_list_del(&cur->list_hkq)) {
            list_bug = true;
            continue;
        }
        atomic_dec(&num_hk_tasks);
        tmp = cur;

        break;
    }

    spin_unlock(&hk_task_qlock);

    if (!IS_ERR_OR_NULL(tmp)) {
        release_task_data(tmp);
        discoC_mem_free(tmp);
        tmp = NULL;
        goto release_hk_res;
    }

    if (list_bug) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN list_add_tail chk fail %s %d\n",
                __func__, __LINE__);
        DMS_WARN_ON(true);
        msleep(1000);
    }
}

static bool _is_mem_nervous (int64_t mTotal, int64_t mFree, int64_t mBuff,
        int64_t mCached, int64_t mDirty)
{
    //int64_t consum, limit;

#if 0
    consum = mDirty;
    limit = (dirty_reserved_KB*dms_client_config->cLinux_cache_cong)/100;
    if (mDirty >= limit) { //Heavy IO now
        if (mFree <= minFree_KB) {
            return true;
        }
    }
#else
    //consum = (mBuff + mCached + mDirty);
    //limit = (mTotal*dms_client_config->cLinux_cache_cong)/100;
    //limit = ((mTotal - 12*1024*1024)*dms_client_config->cLinux_cache_cong)/100;
    //dms_printk(LOG_LVL_DEBUG, "DMSC DEBUG (Buff Cached Dirty consum total limit) = (%lld %lld %lld %lld %lld %lld)\n",
    //        mBuff, mCached, mDirty, consum, mTotal, limit);
    //if (consum >= limit) {
    //    return true;
    //}
    if (mFree < dms_client_config->freeMem_cong) {
        if (mCached > mDirty) {
            if ((mCached - mDirty) >= dms_client_config->freeMem_cong) {
                return true;
            } else {
                return false;
            }
        } else {
            return false;
        }
    }
#endif

    return false;
}

#if 1
//Lego: need linux kernel support export swapper_space
static bool read_memInfo (void)
{
    struct sysinfo i;
    int64_t mTotal, mFree, mBuffers, mCached, mDirty, cached;
    bool doClean;

    doClean = false;

    si_meminfo(&i);

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 9, 0)
    //cached = global_page_state(NR_FILE_PAGES) -
    //                          total_swapcache_pages - i.bufferram;
    cached = global_page_state(NR_FILE_PAGES) - i.bufferram;
    //Lego: Now the observation shows SwapCached is 0, so the workaround
    //Unknown symbol swapper_space
#else
    //cached = global_page_state(NR_FILE_PAGES) -
    //        total_swapcache_pages() - i.bufferram;
    cached = global_page_state(NR_FILE_PAGES) - i.bufferram;
#endif
    mTotal = KBytes(i.totalram);
    mFree = KBytes(i.freeram);
    mBuffers = KBytes(i.bufferram);
    mCached = KBytes(cached);
    mDirty = KBytes(global_page_state(NR_FILE_DIRTY));

    dms_printk(LOG_LVL_DEBUG, "DMSC DEBUG (total, free, buffers, cached, dirty) = "
            "(%lld, %lld, %lld, %lld, %lld)\n",
            mTotal, mFree, mBuffers, mCached, mDirty);
    doClean = _is_mem_nervous(mTotal, mFree, mBuffers, mCached, mDirty);

    return doClean;
}
#else
static int64_t get_items (int8_t *name, int32_t name_len, int8_t *buffer,
        int32_t buff_len)
{
    int8_t line_buf[80];
    int64_t value;
    int8_t *cur_ptr;
    int32_t len_skip, len;
    bool line_start;

    line_start = true;

    cur_ptr = buffer;
    len_skip = 0;
    value = 0;
    while (true) {
        cur_ptr = strchr(buffer, '\n');
        if (IS_ERR_OR_NULL(cur_ptr)) {
            value = 0;
            break;
        }

        len = cur_ptr - buffer;
        len_skip += len;

        if (len_skip >= buff_len) {
            break;
        }

        strncpy(line_buf, buffer, len - 2);
        if (strncmp(line_buf, name, name_len) == 0) {
            sscanf(line_buf + name_len + 1, "%lld\n", &value);
            break;
        }
        buffer = cur_ptr + 1;
    }

    return value;
}

static bool read_memInfo (void)
{
    struct file *fp;
    int64_t mTotal, mFree, mBuffers, mCached, mDirty;
    bool doClean;

    fp = NULL;
    doClean = false;
    fp = filp_open(MEMINFO_FN, O_RDWR, 0);
    if (!IS_ERR_OR_NULL(fp)) {
        fp->f_pos = 0;

        fp->f_op->read(fp, memInfo_buffer, PAGE_SIZE, &fp->f_pos);

        mTotal = get_items(mInfo_Total, strlen(mInfo_Total), memInfo_buffer,
            PAGE_SIZE);
        mFree = get_items(mInfo_Free, strlen(mInfo_Free), memInfo_buffer,
            PAGE_SIZE);
        mBuffers = get_items(mInfo_Buffers, strlen(mInfo_Buffers), memInfo_buffer,
            PAGE_SIZE);
        mCached = get_items(mInfo_Cached, strlen(mInfo_Cached), memInfo_buffer,
            PAGE_SIZE);
        mDirty = get_items(mInfo_Dirty, strlen(mInfo_Dirty), memInfo_buffer,
            PAGE_SIZE);

        doClean = _is_mem_nervous(mTotal, mFree, mBuffers, mCached, mDirty);
        filp_close(fp, NULL);
    }

    return doClean;
}
#endif

#if 0
//kernle not export function drop_pagecache()
static void clear_cache (void)
{
    bool doClean;

    doClean = read_memInfo();
    if (doClean) {
        drop_pagecache();
    }
}
#else
static void clear_cache (void)
{
    mm_segment_t oldfs;
    int8_t __user buf[1];
    struct file *fp = NULL;
    bool doClean;

    oldfs = get_fs();
    set_fs(KERNEL_DS);

    doClean = read_memInfo();

    if (doClean) {
        fp = filp_open(DROP_CACHE_FN, O_RDWR, 0);
        if (!IS_ERR_OR_NULL(fp)) {
            fp->f_pos = 0;
            //buf[0] = '3';
            buf[0] = '1';
            fp->f_op->write(fp, buf, 1, &fp->f_pos);
            filp_close(fp, NULL);
        }

        //fp = filp_open(PAGECACHE_FN, O_RDWR | O_CREAT, 0);
        //fp->f_pos = 0;
        //buf[0] = '1';
        //fp->f_op->write(fp, buf, 1, &fp->f_pos);
        //filp_close(fp, NULL);
    }

    set_fs(oldfs);
}
#endif

static int32_t cleancache_worker (void *thread_data)
{
    int64_t ret_msleep = 0;
    int64_t time_to_sleep = 0;
    dmsc_thread_t *t_data = (dmsc_thread_t *)thread_data;
    allow_signal(SIGKILL); //lego: for reset

    time_to_sleep = dms_client_config->cLinux_cache_period;

    while (t_data->running) {

        ret_msleep = msleep_interruptible(time_to_sleep);
        if (!t_data->running) {
            dms_printk(LOG_LVL_INFO,
                    "DMSC INFO DRIVER is shutdown leave it\n");
            return 0;
        }

        if (ret_msleep > 0) {
            /*
             * be wake up very frequently, might cause some high CPU loading,
             * we sleep 100 milli-seconds without any interruptible.
             */
            if (ret_msleep < 10) {
                msleep(100);
                time_to_sleep = time_to_sleep - 100;
            }

            time_to_sleep = time_to_sleep - ret_msleep;
        }

        if (ret_msleep == 0 || time_to_sleep <= 0) {
            if (dms_client_config->cLinux_cache) {
                clear_cache();
            }
            time_to_sleep = dms_client_config->cLinux_cache_period;
        }
    }

    return 0;
}

#if 1
//Reference proc_misc.c meminfo_read_proc
static int64_t _get_phyMemKB (void)
{
    struct sysinfo i;
    int64_t total_mem;

    si_meminfo(&i);
    total_mem = KBytes(i.totalram);

    if (total_mem <= 0) {
        total_mem = DEFAULT_MACHINE_TOTAL_MEM; //Lego: Default cloudos machine setting
    }

    //Lego: cloudos will reserve total*5% + 12G memory to DOM-0
    total_mem = ((total_mem - CLOUDOS_DOM0_RESERV)*100) / DIRTY_RATIO;
    return total_mem;
}
#else
static int64_t _get_phyMemKB (void)
{
    int64_t mTotal;
    mm_segment_t oldfs;
    struct file *fp;

    oldfs = get_fs();
    set_fs(KERNEL_DS);

    fp = filp_open(MEMINFO_FN, O_RDWR, 0);
    fp->f_pos = 0;

    fp->f_op->read(fp, memInfo_buffer, PAGE_SIZE, &fp->f_pos);

    mTotal = get_items(mInfo_Total, strlen(mInfo_Total), memInfo_buffer,
                PAGE_SIZE);

    filp_close(fp, NULL);
    set_fs(oldfs);

    if (mTotal <= 0) {
        mTotal = DEFAULT_MACHINE_TOTAL_MEM; //Lego: Default cloudos machine setting
    }

    //Lego: cloudos will reserve total*5% + 12G memory to DOM-0
    mTotal = ((mTotal - CLOUDOS_DOM0_RESERV) / DIRTY_RATIO;
    return mTotal;
}
#endif

void initial_cleancache_thread (void) {
    total_phyMemKB = _get_phyMemKB();
    dirty_reserved_KB = (total_phyMemKB*DIRTY_RATIO) / 100;
    //minFree_KB = 512*1024;

    dms_printk(LOG_LVL_INFO,
            "DMSC INFO Total phy mem: %lld reserve for dirty %lld\n",
            total_phyMemKB, dirty_reserved_KB);

    //TODO: total_phyMemKB might < 0 since DOM-0 reserved < 12G
    if (total_phyMemKB > 0 && dirty_reserved_KB > 0) {
        if (IS_ERR_OR_NULL(create_worker_thread("dms_ccache_wker", NULL, NULL,
                cleancache_worker))) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN kthread dms_sk_hb_wker creation fail\n");
            DMS_WARN_ON(true);
            return;
        }
    } else {
        dms_printk(LOG_LVL_INFO,
                "DMSC INFO Low mem environment not start cleancache_worker\n");
    }
}
