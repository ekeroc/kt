/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * procfs.c
 *  export /proc/ccma/ entries for debugging & monitoring
 *
 *
 */

#include <linux/version.h>
#include <linux/blkdev.h>
#include <linux/proc_fs.h>
#include <linux/swap.h>
#include <net/sock.h>

#include "../common/dms_kernel_version.h"
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
#include <linux/path.h>
#endif
#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/discoC_mem_manager.h"
#include "../metadata_manager/metadata_manager.h"
#include "../metadata_manager/metadata_cache.h"
#include "../common/dms_client_mm.h"
#include "../discoDN_client/discoC_DNC_Manager_export.h"
#include "../payload_manager/payload_manager_export.h"
#include "../io_manager/discoC_IO_Manager.h"
#include "../io_manager/discoC_IO_Manager_export.h"
#include "../common/thread_manager.h"
#include "../connection_manager/conn_manager_export.h"
#include "../discoNN_client/discoC_NNC_Manager_api.h"
#include "../vdisk_manager/volume_manager.h"
#include "../flowcontrol/FlowControl.h"
#include "../common/cache.h"
#include "dms_info_private.h"
#include "../config/dmsc_config.h"
#ifdef DISCO_PREALLOC_SUPPORT
#include "../metadata_manager/space_manager_export_api.h"
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
static int config_read (char *buffer, char **buffer_location,
                 off_t offset, int nbytes, int *eof, void *data)
#else
static ssize_t config_read (struct file *file, char __user *buffer,
        size_t nbytes, loff_t *ppos)
#endif
{
    uint32_t len, cp_len;
    int8_t *local_buf;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
    int32_t ret;
    static uint32_t finished = 0;

    if (finished) {
        finished = 0;
        return 0;
    }
#endif

    local_buf = discoC_mem_alloc(sizeof(int8_t)*PAGE_SIZE, GFP_NOWAIT | GFP_ATOMIC);
    len = show_configuration(local_buf, PAGE_SIZE);

    cp_len = len > nbytes ? nbytes : len;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
    ret = copy_to_user(buffer, local_buf, cp_len);
    finished = 1;
#else
    memcpy(buffer, local_buf, cp_len);
#endif

    discoC_mem_free(local_buf);

    return cp_len;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
static int config_write (struct file *file, const char *buffer,
                  unsigned long count, void *data)
#else
static ssize_t config_write (struct file *file, const char __user *buffer,
        size_t count, loff_t *ppos)
#endif
{
    int8_t local_buf[128];
    uint32_t cp_len;

    memset(local_buf, 0, 128);
    cp_len = 128 > count ? count : 128;

    if (copy_from_user(local_buf, buffer, cp_len)) {
        return -EFAULT;
    }

    local_buf[cp_len] = '\0';

    dms_printk(LOG_LVL_INFO, "DMSC INFO Add Configuration: %s\n", local_buf);

    if (set_config_item(local_buf, cp_len)) {
        dms_printk(LOG_LVL_INFO, "DMSC INFO Unknown Config item: %s\n",
                local_buf);
    }

    return count;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
const struct file_operations config_proc_fops = {
.owner = THIS_MODULE,
.write = config_write,
.read = config_read,
};
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
static int dms_resources_read (char *buffer, char **buffer_location,
                 off_t offset, int nbytes, int *eof, void *data)
#else
static ssize_t dms_resources_read (struct file *file, char __user *buffer,
        size_t nbytes, loff_t *ppos)
#endif
{
    int32_t len, cp_len;
    int8_t *local_buf;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
    int32_t ret;
    static uint32_t finished = 0;

    if (finished) {
        finished = 0;
        return 0;
    }
#endif

    local_buf = discoC_mem_alloc(sizeof(int8_t)*PAGE_SIZE, GFP_NOWAIT | GFP_ATOMIC);
    len = show_res_info(local_buf, PAGE_SIZE);
    cp_len = len > nbytes ? nbytes : len;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
    ret = copy_to_user(buffer, local_buf, cp_len);
    finished = 1;
#else
    memcpy(buffer, local_buf, cp_len);
#endif

    discoC_mem_free(local_buf);

    return cp_len;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
static int dms_resources_write (struct file *file, const char *buffer,
        unsigned long count, void *data)
#else
static ssize_t dms_resources_write (struct file *file, const char __user *buffer,
        size_t count, loff_t *ppos)
#endif
{
    int8_t local_buf[128];
    uint32_t cp_len;

    memset(local_buf, 0, 128);
    cp_len = 128 > count ? count : 128;
    if (copy_from_user(local_buf, buffer, cp_len)) {
        return -EFAULT;
    }

    dms_printk(LOG_LVL_INFO, "DMSC INFO write val %s to res\n", local_buf);

    if (local_buf[0] == '1') {
        reset_all_res_perf_data();
    }
    dms_printk(LOG_LVL_INFO, "DMSC INFO write val %s to res done\n", local_buf);

    return 1;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
const struct file_operations res_proc_fops = {
.owner = THIS_MODULE,
.write = dms_resources_write,
.read = dms_resources_read,
};
#endif


#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
static int monitor_write (struct file *file, const char *buffer,
        unsigned long count, void *data)
#else
static ssize_t monitor_write (struct file *file, const char __user *buffer,
        size_t count, loff_t *ppos)
#endif
{
    int8_t local_buf[128];
    uint32_t cp_len;

    memset(local_buf, 0, 128);
    cp_len = 128 > count ? count : 128;
    if (copy_from_user(local_buf, buffer, cp_len)) {
        return -EFAULT;
    }

    dms_printk(LOG_LVL_INFO, "DMSC INFO write %s to monitor\n", local_buf);

    if (local_buf[0] == '1') {
        vm_reset_allvol_iocnt();
    }

    dms_printk(LOG_LVL_INFO, "DMSC INFO write %s to monitor done\n", local_buf);
    return 1;
}

static int32_t show_monitor_info (int8_t *buffer, int32_t buff_len)
{
    int32_t len, i, limit;
    int32_t fc_ss_rate, fc_s_max, fc_cur_rate, fc_cur_max;
    int32_t alloc_rate, alloc_max;
    volume_device_t *vol;
    volume_io_cnt_t *vol_io_cnt;
    uint32_t max_cache_size, curr_cache_item;

    limit = buff_len - 80;
    max_cache_size = 0;
    curr_cache_item = 0;

    len = sprintf(buffer, "Free Pages: %ld (ClientNode Running Mode %d)\n",
            (unsigned long)nr_free_pages(),
            dms_client_config->clientnode_running_mode);

    len += sprintf(buffer + len, "Latest Client Request ID: %u\n",
            IOMgr_get_last_ioReqID());
    len += sprintf(buffer + len, "Latest NN ID: %u\n",
    		NNClientMgr_get_last_MDReqID());
    len += sprintf(buffer + len, "Latest Received NN RID: %u\n",
            get_last_rcv_nnReq_ID());
    len += sprintf(buffer + len, "Latest Received DN RID: %u\n", get_last_DNUDataRespID());
    len += sprintf(buffer + len, "Latest DN ID: %u\n", get_last_DNUDataReqID());

    len += sprintf(buffer + len, "Total # of Pending IO Requests: %d "
            "(splitted IOs: %d)\n",
            IOMgr_get_onfly_ioReq(), IOMgr_get_onfly_ioSReq());
    len += sprintf(buffer + len, "Total # of Overlap Pending IO Request: %d\n",
            IOMgr_get_onfly_ovlpioReq());
    len += sprintf(buffer + len, "Total # of NN Requests (cache miss) "
            "wait for send: %d\n",  get_cm_workq_size());
    len += sprintf(buffer + len, "Total # of NN Requests (cache hit) "
                "wait for send: %d\n",  get_ch_workq_size());
    len += sprintf(buffer + len, "Total # of NN Requests wait for response "
            "(cache miss): %d\n", get_num_noResp_nnReq());
    len += sprintf(buffer + len, "Total # of FDN Requests wait for send: %d\n",
            get_num_DNUDataReq_waitSend());
    //len += sprintf(buffer + len, "Total # of FDN Requests wait for recv/process: %d\n",
    //        atomic_read(&num_fdnReq_waitResp));
    len += sprintf(buffer + len, "Total # of FDN wait for finished: %d\n",
    		NNClientMgr_get_num_MDReq());
    len += sprintf(buffer + len, "Total # of DN Requests wait for rece: %d\n",
            get_num_DNUDataReq_waitResp());
    len += sprintf(buffer + len, "Total # of Commit or Wait for free Requests: %d\n",
            IOMgr_get_commit_qSize());
    len += sprintf(buffer + len, "DMSC Runtime Err cnt: %d\n",
            atomic_read(&discoC_err_cnt));

    if (dms_client_config->enable_fc) {
        get_nn_res_rate(&fc_ss_rate, &fc_s_max, &fc_cur_rate, &fc_cur_max,
                        &alloc_rate, &alloc_max);
        len += sprintf(buffer+len, "FlowControl: enable (%d %d %d %d %d %d)\n\n",
                fc_ss_rate, fc_s_max, fc_cur_rate, fc_cur_max,
                alloc_rate, alloc_max);
    } else {
        len+=sprintf(buffer+len, "\n");
    }

    len += connMgr_dump_connStatus(buffer + len, buff_len - len);

    len += sprintf(buffer + len, "io_req/"
            "radix_tree/tree_leaf memory usage: %d %d %d\n",
            get_mem_usage_cnt(MEM_Buf_IOReq),
            atomic_read(&dms_node_count),
            cacheMgr_get_num_treeleaf());

    vol = NULL;
    i = 0;
    while (!IS_ERR_OR_NULL(vol = iterate_all_volumes(vol))) {
        MDMgr_get_volume_cacheSize(vol->vol_id, &max_cache_size, &curr_cache_item);
        vol_io_cnt = &vol->io_cnt;
        if (dms_client_config->enable_fc) {
            len += sprintf(buffer + len, "Drive[%d]: %s%d, lgr_w=%d,"
                    " lgr_r=%d lgr_rq=%d w_rqs=%d err_cnt=%d open_cnt=%d state=%d"
                    " (nn=%d dn=%d) cache %d/%d",
                    i, DEVICE_NAME_PREFIX, vol->vol_id,
                    atomic_read(&vol_io_cnt->linger_wreq_cnt),
                    atomic_read(&vol_io_cnt->linger_rreq_cnt),
                    atomic_read(&vol->lgr_rqs),
                    atomic_read(&vol->cnt_rqs),
                    vol_io_cnt->error_commit_cnt,
                    atomic_read(&vol_io_cnt->disk_open_cnt),
                    vol->vol_state,
                    get_fc_resp_time(vol->vol_fc_group, RES_TYPE_NN),
                    get_fc_resp_time(vol->vol_fc_group, RES_TYPE_DN),
                    curr_cache_item, max_cache_size);
        } else {
            len += sprintf(buffer+len, "Drive[%d]: %s%d, lgr_w=%d,"
                    " lgr_r=%d gr_rq=%d w_rqs=%d err_cnt=%d open_cnt=%d state=%d"
                    " cache %d/%d", i, DEVICE_NAME_PREFIX, vol->vol_id,
                    atomic_read(&vol_io_cnt->linger_wreq_cnt),
                    atomic_read(&vol_io_cnt->linger_rreq_cnt),
                    atomic_read(&vol->lgr_rqs),
                    atomic_read(&vol->cnt_rqs),
                    vol_io_cnt->error_commit_cnt,
                    atomic_read(&vol_io_cnt->disk_open_cnt),
                    vol->vol_state,
                    curr_cache_item, max_cache_size);
        }

        if (vol->ovw_data.ovw_reading) {
            len += sprintf(buffer + len, " getting ovw");
        }
        len += sprintf(buffer + len, "\n");

        i++;
        if (len >= limit) {
            iterate_break(vol);
            break;
        }
    }

    return len;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
static int monitor_read (char *buffer, char **buffer_location,
                 off_t offset, int nbytes, int *eof, void *data)
#else
static ssize_t monitor_read (struct file *file, char __user *buffer,
        size_t nbytes, loff_t *ppos)
#endif
{
    uint32_t len, cp_len;
    int8_t *local_buf;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
    int32_t ret;
    static uint32_t finished = 0;

    if (finished) {
        finished = 0;
        return 0;
    }
#endif

    local_buf = discoC_mem_alloc(sizeof(int8_t)*PAGE_SIZE, GFP_NOWAIT | GFP_ATOMIC);
    len = show_monitor_info(local_buf, PAGE_SIZE);
    cp_len = len > nbytes ? nbytes : len;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
    ret = copy_to_user(buffer, local_buf, cp_len);
    finished = 1;
#else
    memcpy(buffer, local_buf, cp_len);
#endif

    discoC_mem_free(local_buf);
    return cp_len;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
const struct file_operations monitor_proc_fops = {
.owner = THIS_MODULE,
.write = monitor_write,
.read = monitor_read,
};
#endif

static uint32_t show_iolist_info (int8_t *buffer, int32_t buff_len)
{
    uint32_t len, limit;
//    io_request_t *cur, *next, *tmp;
    volume_device_t *vol;
//    volume_io_queue_t *vol_q;
    bool do_break = false;

    len = 0;
    limit = buff_len - 80;

    vol = NULL;
    while (!IS_ERR_OR_NULL(vol = iterate_all_volumes(vol))) {
#if 0
        vol_q = &vol->vol_io_q;
        mutex_lock(&vol_q->io_pending_q_lock);
        list_for_each_entry_safe (cur, next, &vol_q->io_pending_q, list2ioReqPool) {
            if (len >= limit || IS_ERR_OR_NULL(cur)) {
                do_break = true;
                break;
            }

            tmp = cur;
            len += sprintf(buffer+len, "\nioReqID: %d(0x%08x): "
                    "begin_lbid = %llu, lbid_len = %d",
                    tmp->ioReqID, tmp->request_state.state,
                    tmp->begin_lbid, tmp->lbid_len);
        }

        mutex_unlock(&vol_q->io_pending_q_lock);
        if(len >= limit) {
            do_break = true;
            break;
        }
#endif
    }

    if (do_break) {
        iterate_break(vol);
    }

    return len;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
static int io_pending_list_content_read (char *buffer, char **buffer_location,
                 off_t offset, int nbytes, int *eof, void *data)
#else
static ssize_t io_pending_list_content_read (struct file *file, char __user *buffer,
        size_t nbytes, loff_t *ppos)
#endif
{
    uint32_t len, cp_len;
    int8_t *local_buf;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
    int32_t ret;
    static uint32_t finished = 0;

    if (finished) {
        finished = 0;
        return 0;
    }
#endif

    local_buf = discoC_mem_alloc(sizeof(int8_t)*PAGE_SIZE, GFP_NOWAIT | GFP_ATOMIC);
    len = show_iolist_info(local_buf, PAGE_SIZE);
    cp_len = len > nbytes ? nbytes : len;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
    ret = copy_to_user(buffer, local_buf, cp_len);
    finished = 1;
#else
    memcpy(buffer, local_buf, cp_len);
#endif

    discoC_mem_free(local_buf);

    return cp_len;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
const struct file_operations iolist_proc_fops = {
.owner = THIS_MODULE,
.read = io_pending_list_content_read,
};
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
static int fdn_pending_list_content_read (char *buffer, char **buffer_location,
                 off_t offset, int nbytes, int *eof, void *data)
#else
static ssize_t fdn_pending_list_content_read (struct file *file, char __user *buffer,
        size_t nbytes, loff_t *ppos)
#endif
{
    int32_t len, cp_len;
    int8_t *local_buf;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
    int32_t ret;
    static uint32_t finished = 0;

    if (finished) {
        finished = 0;
        return 0;
    }
#endif

    local_buf = discoC_mem_alloc(sizeof(int8_t)*PAGE_SIZE, GFP_NOWAIT | GFP_ATOMIC);
    len = NNClientMgr_show_MDRequest(local_buf, PAGE_SIZE);
    cp_len = len > nbytes ? nbytes : len;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
    ret = copy_to_user(buffer, local_buf, len);
    finished = 1;
#else
    memcpy(buffer, local_buf, cp_len);
#endif

    discoC_mem_free(local_buf);

    return cp_len;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
const struct file_operations fdnlist_proc_fops = {
.owner = THIS_MODULE,
.read = fdn_pending_list_content_read,
};
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
static int thread_state_read (char *buffer, char **buffer_location,
                 off_t offset, int nbytes, int *eof, void *data)
#else
static ssize_t thread_state_read (struct file *file, char __user *buffer,
        size_t nbytes, loff_t *ppos)
#endif
{
    uint32_t len, cp_len;
    int8_t *local_buf;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
    int32_t ret;
    static uint32_t finished = 0;

    if (finished) {
        finished = 0;
        return 0;
    }
#endif

    local_buf = discoC_mem_alloc(sizeof(int8_t)*PAGE_SIZE, GFP_NOWAIT | GFP_ATOMIC);
    len = show_thread_info(local_buf, PAGE_SIZE);
    cp_len = len > nbytes ? nbytes : len;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
    ret = copy_to_user(buffer, local_buf, cp_len);
    finished = 1;
#else
    memcpy(buffer, local_buf, cp_len);
#endif

    discoC_mem_free(local_buf);

    return cp_len;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
static int thread_state_write (struct file *file, const char *buffer,
        unsigned long count, void *data)
#else
static ssize_t thread_state_write (struct file *file, const char __user *buffer,
        size_t count, loff_t *ppos)
#endif
{
    int8_t local_buf[128];
    uint32_t cp_len;

    memset(local_buf, 0, 128);
    cp_len = 128 > count ? count : 128;
    if (copy_from_user(local_buf, buffer, cp_len)) {
        return -EFAULT;
    }
    dms_printk(LOG_LVL_INFO, "DMSC INFO write val %s to thread state\n",
            local_buf);

    if (local_buf[0] == '1') {
        clean_thread_performance_data();
    }

    dms_printk(LOG_LVL_INFO, "DMSC INFO write val %s to thread state done\n",
                local_buf);
    return 1;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
const struct file_operations thread_proc_fops = {
.owner = THIS_MODULE,
.write = thread_state_write,
.read = thread_state_read,
};
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
static int prealloc_read (char *buffer, char **buffer_location,
                 off_t offset, int nbytes, int *eof, void *data)
#else
static ssize_t prealloc_read (struct file *file, char __user *buffer,
        size_t nbytes, loff_t *ppos)
#endif
{
    uint32_t len, cp_len;
    int8_t *local_buf;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
    int32_t ret;
    static uint32_t finished = 0;

    if (finished) {
        finished = 0;
        return 0;
    }
#endif

    local_buf = discoC_mem_alloc(sizeof(int8_t)*PAGE_SIZE*2, GFP_NOWAIT | GFP_ATOMIC);
#ifdef DISCO_PREALLOC_SUPPORT
    len = sm_dump_info(local_buf, PAGE_SIZE);
#else
    len = sprintf(local_buf, "Not support prealloc\n");
#endif
    cp_len = len > nbytes ? nbytes : len;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
    ret = copy_to_user(buffer, local_buf, cp_len);
    finished = 1;
#else
    memcpy(buffer, local_buf, cp_len);
#endif

    discoC_mem_free(local_buf);

    return cp_len;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
static int prealloc_write (struct file *file, const char *buffer,
        unsigned long count, void *data)
#else
static ssize_t prealloc_write (struct file *file, const char __user *buffer,
        size_t count, loff_t *ppos)
#endif
{
    int8_t local_buf[128];
    uint32_t cp_len;

    memset(local_buf, 0, 128);
    cp_len = 128 > count ? count : 128;
    if (copy_from_user(local_buf, buffer, cp_len)) {
        return -EFAULT;
    }

    dms_printk(LOG_LVL_INFO, "DMSC INFO not support write val %s to prealloc\n",
            local_buf);

    return count;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
const struct file_operations prealloc_proc_fops = {
.owner = THIS_MODULE,
.write = prealloc_write,
.read = prealloc_read,
};
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
static volume_device_t *get_ccma_drive_by_file (struct file * filp)
{
    volume_device_t *current_drv;
    int8_t *tmp, *pathname, *ptr_vol_id;
    struct path path_p;
    uint32_t volume_id;

    tmp = (int8_t *)__get_free_page(GFP_TEMPORARY);
    if (!tmp) {
        return NULL;
    }

    current_drv = NULL;
    volume_id = 0;

    path_p = filp->f_path;
    path_get(&filp->f_path);
    pathname = d_path(&path_p, tmp, PAGE_SIZE);
    path_put(&path_p);

    if (IS_ERR(pathname)) {
        free_page((unsigned long)tmp);
        return NULL;
    }

    if(strncmp(pathname, "/proc/ccma/volume/", 18) == 0) {
        ptr_vol_id = pathname + 18;
        sscanf(ptr_vol_id,"%d",&volume_id);
        current_drv = find_volume(volume_id);
    }

    free_page((unsigned long)tmp);
    return current_drv;
}
#else
static volume_device_t *get_ccma_drive_by_file (struct file * filp)
{
    int8_t local_buf[128];
    volume_device_t *current_drv;
    uint32_t volume_id;

    volume_id = 0;
    strncpy(local_buf, filp->f_dentry->d_name.name, filp->f_dentry->d_name.len);
    local_buf[filp->f_dentry->d_name.len] = '\0';

    sscanf(local_buf,"%d",&volume_id);
    current_drv = find_volume(volume_id);
    return current_drv;
}
#endif

//__user
static ssize_t volume_read (struct file *filp, char *buffer,
        size_t count, loff_t *offset)
{
    volume_device_t *current_drv;
    int32_t num_of_read, i, len;
    int8_t *local_buf;
    static uint32_t finished = 0;

    if (finished) {
        finished = 0;
        return 0;
    }

    local_buf = discoC_mem_alloc(sizeof(int8_t)*PAGE_SIZE, GFP_NOWAIT | GFP_ATOMIC);
    num_of_read = 0;
    current_drv = get_ccma_drive_by_file(filp);

    if (!IS_ERR_OR_NULL(current_drv)) {
        num_of_read = show_volume_info(local_buf, current_drv);
    } else {
        dms_printk(LOG_LVL_WARN, "DMSC WARN Cannot find volume\n");
        num_of_read = sprintf(local_buf, "DMSC WARN Cannot find volume\n");
    }

    local_buf[num_of_read] = '\0';

    len = count < num_of_read ? count : num_of_read;
    for (i = 0; i < len; i++) {
        put_user(local_buf[i], buffer + i);
    }

    finished = 1;
    discoC_mem_free(local_buf);

    return len;
}

static ssize_t volume_write (struct file * filp, const char __user *buffer,
        size_t count, loff_t * offset)
{
    //TODO Move to volume_manager.c
    volume_device_t *current_drv;
    uint32_t len1 = strlen("MetaDataCache=");
    uint32_t len2 = strlen("ShareVolume=");
    uint32_t len3 = strlen("1");
    uint32_t len4 = strlen("2");
    uint32_t len5 = strlen("RW_PERMIT=");
    uint32_t len6 = strlen("SEND_RW_DATA=");
    uint32_t len7 = strlen("DO_RW_DATA=");
    int8_t local_buf[100], *src_ptr;
    uint32_t value = -1;

    current_drv = get_ccma_drive_by_file(filp);

    if (!IS_ERR_OR_NULL(current_drv)) {
        //set volume->cache_state
        if (copy_from_user(local_buf, buffer, count)) {
            return -EFAULT;
        }

        src_ptr = local_buf;
        if (strncmp(local_buf, "MetaDataCache=", len1) == 0) {
            src_ptr = src_ptr + len1;
            sscanf(src_ptr,"%d",&value);
            if (value == 0) {
                set_vol_mcache(current_drv, false);
                inval_volume_MDCache_volID(current_drv->vol_id);
            } else if (value == 1) {
                set_vol_mcache(current_drv, true);
            } else {
                dms_printk(LOG_LVL_ERR,
                        "DMSC WARN Unknown value %d for volume metadata cache\n",
                        value);
            }
        } else if (strncmp(local_buf, "ShareVolume=", len2) == 0) {
            src_ptr = src_ptr + len2;
            sscanf(src_ptr,"%d",&value);
            if (value == 1) {
                if (dms_client_config->enable_share_volume) {
                    set_vol_share(current_drv, true);
                    set_vol_mcache(current_drv, false);
                    inval_volume_MDCache_volID(current_drv->vol_id);
                }
            } else if (value == 0) {
                set_vol_share(current_drv, false);
            } else {
                dms_printk(LOG_LVL_WARN,
                        "DMSC WARN Unknown value %d for setting share volume\n",
                        value);
            }
        } else if (strncmp(local_buf, "1", len3) == 0) {
            reset_vol_perf_data(current_drv);
        } else if (strncmp(local_buf, "2", len4) == 0) {
            reset_vol_iocnt(current_drv);
        } else if (strncmp(local_buf, "RW_PERMIT=", len5) == 0) {
            src_ptr = src_ptr + len5;
            sscanf(src_ptr,"%d",&value);
            set_vol_rw_perm(current_drv, value);
        } else if (strncmp(local_buf, "SEND_RW_DATA=", len6) == 0) {
            src_ptr = src_ptr + len6;
            sscanf(src_ptr,"%d",&value);
            if (value == 1) {
                set_vol_send_rw_payload(current_drv, true);
            } else if (value == 0) {
                set_vol_send_rw_payload(current_drv, false);
            } else {
                dms_printk(LOG_LVL_WARN,
                        "DMSC WARN Unknown value %d for setting send_rw_data\n",
                        value);
            }
        } else if (strncmp(local_buf, "DO_RW_DATA=", len7) == 0) {
            src_ptr = src_ptr + len7;
            sscanf(src_ptr, "%d", &value);
            if (value == 1) {
                set_vol_do_rw_IO(current_drv, true);
            } else if (value == 0) {
                set_vol_do_rw_IO(current_drv, false);
            } else {
                dms_printk(LOG_LVL_WARN,
                        "DMSC WARN Unknown value %d for setting do_w_data\n",
                        value);
            }
        } else {
            dms_printk(LOG_LVL_WARN,
                    "DMSC INFO Unknown volume Configuration Setting\n");
        }
    }

    return count;
}

static struct file_operations volume_proc_ops = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
    .owner   = THIS_MODULE,
#endif
    //.open    = volume_open,
    .read    = volume_read,
    .write	 = volume_write
};

int32_t create_vol_proc (uint32_t volume_id)
{
    int8_t buffer[64];
    struct proc_dir_entry *vol_proc;

    if (IS_ERR_OR_NULL(proc_volume)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN fail create vol proc proc_volume is NULL\n");
        DMS_WARN_ON(true);
        return 0;
    }

    memset(buffer, 0, sizeof(char)*64);
    sprintf(buffer, "%u", volume_id);

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
    vol_proc = create_proc_entry(buffer, 0644, proc_volume);

    if (IS_ERR(vol_proc)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN Couldn't create proc entry for volume %d\n",
                volume_id);
        DMS_WARN_ON(true);
        return 0;
    } else {
        vol_proc->proc_fops = &volume_proc_ops;
    }
#else
    vol_proc = proc_create(buffer, 0x0644, proc_volume, &volume_proc_ops);
#endif

    return 1;
}

int32_t delete_vol_proc(uint32_t volume_id)
{
    int8_t buffer[64];

    if (IS_ERR_OR_NULL(proc_volume)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN fail del vol proc proc_volume is NULL\n");
        DMS_WARN_ON(true);
        return 0;
    }

    memset(buffer, 0, sizeof(int8_t)*64);
    sprintf(buffer, "%u", volume_id);
    remove_proc_entry(buffer, proc_volume);
    return 1;
}

void release_procfs (struct proc_dir_entry *proc_ccma)
{
    remove_proc_entry(NAME_PROC_MONITOR, proc_ccma);
    remove_proc_entry(NAME_PROC_IOLIST, proc_ccma);
    remove_proc_entry(NAME_PROC_FDNLIST, proc_ccma);
    remove_proc_entry(NAME_PROC_CONFIG, proc_ccma);
    remove_proc_entry(NAME_PROC_THREAD, proc_ccma);
    remove_proc_entry(NAME_PROC_PREALLOC, proc_ccma);
    remove_proc_entry(NAME_PROC_RES, proc_ccma);
    remove_proc_entry(NAME_PROC_VOL, proc_ccma);
}

static void init_monitor_proc (struct proc_dir_entry *proc_ccma)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
    proc_entry = create_proc_entry(NAME_PROC_MONITOR, 0644, proc_ccma);
    if (IS_ERR_OR_NULL(proc_entry)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN Couldn't create proc monitor entry\n");
        DMS_WARN_ON(true);
    } else {
        proc_entry->read_proc = monitor_read;
        proc_entry->write_proc =monitor_write;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
        proc_entry->owner = THIS_MODULE;
#endif
    }
#else

    proc_entry = proc_create(NAME_PROC_MONITOR, 0x0644, proc_ccma,
            &monitor_proc_fops);
#endif
}

static void init_res_proc (struct proc_dir_entry *proc_ccma)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
    proc_entry = create_proc_entry(NAME_PROC_RES, 0644, proc_ccma);
    if (IS_ERR_OR_NULL(proc_entry)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN Couldn't reseource proc monitor entry\n");
        DMS_WARN_ON(true);
    } else {
        proc_entry->read_proc = dms_resources_read;
        proc_entry->write_proc = dms_resources_write;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
        proc_entry->owner = THIS_MODULE;
#endif
    }
#else

    proc_entry = proc_create(NAME_PROC_RES, 0x0644, proc_ccma,
            &res_proc_fops);
#endif
}

static void init_iolist_proc (struct proc_dir_entry *proc_ccma)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
    proc_entry = create_proc_entry(NAME_PROC_IOLIST, 0644, proc_ccma);
    if (IS_ERR_OR_NULL(proc_entry) ) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN Couldn't create proc io pending list entry\n");
        DMS_WARN_ON(true);
    } else {
        proc_entry->read_proc = io_pending_list_content_read;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
        proc_entry->owner = THIS_MODULE;
#endif
    }
#else

    proc_entry = proc_create(NAME_PROC_IOLIST, 0x0644, proc_ccma,
            &iolist_proc_fops);
#endif
}

static void init_fdnlist_proc (struct proc_dir_entry *proc_ccma)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
    proc_entry = create_proc_entry(NAME_PROC_FDNLIST, 0644, proc_ccma);
    if (IS_ERR_OR_NULL(proc_entry)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN Couldn't create proc fdn pending entry\n");
        DMS_WARN_ON(true);
    } else {
        proc_entry->read_proc = fdn_pending_list_content_read;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
        proc_entry->owner = THIS_MODULE;
#endif
    }
#else

    proc_entry = proc_create(NAME_PROC_FDNLIST, 0x0644, proc_ccma,
            &fdnlist_proc_fops);
#endif
}

static void init_config_proc (struct proc_dir_entry *proc_ccma)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
    proc_entry = create_proc_entry(NAME_PROC_CONFIG, 0644, proc_ccma);
    if (IS_ERR_OR_NULL(proc_entry) ) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN Couldn't create proc config pending entry\n");
        DMS_WARN_ON(true);
    } else {
        proc_entry->read_proc = config_read;
        proc_entry->write_proc = config_write;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
        proc_entry->owner = THIS_MODULE;
#endif
    }
#else

    proc_entry = proc_create(NAME_PROC_CONFIG, 0x0644, proc_ccma,
            &config_proc_fops);
#endif
}

static void init_thread_proc (struct proc_dir_entry *proc_ccma)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
    proc_entry = create_proc_entry(NAME_PROC_THREAD, 0644, proc_ccma);
    if (IS_ERR_OR_NULL(proc_entry)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN Couldn't create proc thread state pending entry\n");
        DMS_WARN_ON(true);
    } else {
        proc_entry->read_proc = thread_state_read;
        proc_entry->write_proc = thread_state_write;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
        proc_entry->owner = THIS_MODULE;
#endif
    }
#else

    proc_entry = proc_create(NAME_PROC_THREAD, 0x0644, proc_ccma,
            &thread_proc_fops);
#endif
}

static void init_prealloc_proc (struct proc_dir_entry *proc_ccma)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
    proc_entry = create_proc_entry(NAME_PROC_THREAD, 0644, proc_ccma);
    if (IS_ERR_OR_NULL(proc_entry)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN fail create proc prealloc\n");
        DMS_WARN_ON(true);
    } else {
        proc_entry->read_proc = prealloc_read;
        proc_entry->write_proc = prealloc_write;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
        proc_entry->owner = THIS_MODULE;
#endif
    }
#else

    proc_entry = proc_create(NAME_PROC_PREALLOC, 0x0644, proc_ccma, &prealloc_proc_fops);
#endif
}

void init_procfs (struct proc_dir_entry *proc_ccma)
{
    //TODO using pre-defined array
    proc_volume = proc_mkdir(NAME_PROC_VOL, proc_ccma);
    init_monitor_proc(proc_ccma);
    init_res_proc(proc_ccma);
    init_iolist_proc(proc_ccma);
    init_fdnlist_proc(proc_ccma);
    init_config_proc(proc_ccma);
    init_thread_proc(proc_ccma);
    init_prealloc_proc(proc_ccma);
}
