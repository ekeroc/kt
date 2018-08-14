/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * volume_manager.c
 *
 */
#include <linux/module.h>
#include <linux/rwsem.h>
#include <linux/mutex.h>
#include <linux/blkdev.h>
#include <linux/genhd.h>
#include <linux/hdreg.h> /* HDIO_GETGEO */
#include <linux/delay.h>
#include <net/sock.h>
#include <linux/version.h>
#include <linux/random.h>
#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/common.h"
#include "../common/discoC_mem_manager.h"
#include "volume_manager_private.h"
#include "../common/dms_kernel_version.h"
#include "../metadata_manager/metadata_manager.h"
#include "../metadata_manager/metadata_cache.h"
#include "../common/dms_client_mm.h"
#include "../discoDN_client/discoC_DNC_Manager_export.h"
#include "../payload_manager/payload_manager_export.h"
#include "../io_manager/discoC_IOSegment_Request.h"
#include "../io_manager/discoC_IO_Manager.h"
#include "../io_manager/discoC_IO_Manager_export.h"
#include "../config/dmsc_config.h"
#include "volume_manager.h"
#include "volume_operation_api.h"
#include "../flowcontrol/FlowControl.h"
#include "../drv_fsm.h"
#include "../drv_main.h"
#include "../drv_blkdev.h"
#include "../common/thread_manager.h"
#include "../io_manager/io_worker_manager.h"
#include "../info_monitor/dms_sysinfo.h"
#include "../discoNN_client/discoC_NNC_ovw.h"
#include "../common/thread_manager.h"
#include "../connection_manager/conn_manager_export.h"
#include "../discoNN_client/discoC_NNC_Manager_api.h"
#ifdef DISCO_PREALLOC_SUPPORT
#include "../metadata_manager/space_manager_export_api.h"
#endif

static void set_vol_rw_permission (struct volume_device *vol,
        vol_rw_perm_type_t value);

static int32_t volume_getgeo (struct block_device *bdev,
        struct hd_geometry *geo)
{
    sector_t nsect;
    sector_t cylinders;
    /*
     * We don't have real geometry info, but let's at least return
     * values consistent with the size of the device
     */

    if (IS_ERR_OR_NULL(bdev)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN fail get geo of null block device\n");
        DMS_WARN_ON(true);
        return -ENODEV;
    }

    nsect = get_capacity(bdev->bd_disk);
    cylinders = nsect;

    geo->heads = 0xff;

    geo->sectors = 0x3f;
    sector_div(cylinders, geo->heads * geo->sectors);

    if ((sector_t)(geo->cylinders + 1) * geo->heads * geo->sectors < nsect) {
        geo->cylinders = 0xffff;
    }

    return 0;
}

/**
 * function: volume_blkdev_ioctl()
 * description: ioctl routine.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32)
static int volume_blkdev_ioctl (struct block_device *bdev, fmode_t mode,
                              unsigned int cmd, unsigned long arg)
{
#else
static long volume_blkdev_ioctl (struct file *filp, unsigned int cmd,
                               unsigned long arg)
{
    struct inode *inode;
#endif
    uint64_t size;
    struct gendisk *disk;
    volume_device_t *current_drv;

    dms_printk(LOG_LVL_INFO,
            "DMSC INFO blkdev ioctl: cmd=%d "
            "(BLKFLSBUF=%d BLKGETSIZE=%d BLKSSZGET=%d HDIO_GETGEO=%d)\n", cmd,
               BLKFLSBUF, BLKGETSIZE, BLKSSZGET, HDIO_GETGEO);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32)
    if (IS_ERR_OR_NULL(bdev)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN blkdev ioctl: get null ptr\n");
        return -EFAULT;
    }
    disk = bdev->bd_disk;
#else

    if (IS_ERR_OR_NULL(filp)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN blkdev ioctl: get null ptr\n");
        return -EFAULT;
    }

    inode = filp->f_dentry->d_inode;
    disk = inode->i_bdev->bd_disk;
#endif

    current_drv = disk->private_data;

    if (IS_ERR_OR_NULL(disk) || IS_ERR_OR_NULL(current_drv)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN null ptr for block dev ioctl\n");
        DMS_WARN_ON(true);
        return -EFAULT;
    }

    switch (cmd) {
    case BLKROSET: //(0 = read-write)
        if (IS_ERR_OR_NULL(current_drv)) {
            return -ENODEV;
        }

        set_vol_rw_permission(current_drv, R_ONLY);

        dms_printk(LOG_LVL_INFO, "DMSC INFO Set volume %d as read only\n",
                current_drv->vol_id);
        return 0;

    case BLKROGET: //(0 = read-write)
        return put_user((int32_t)!is_vol_ro(current_drv), (int __user *)arg);
#if 0
    case BLKRASET: //TODO: set read ahead for block device
    case BLKRAGET: //TODO: get read ahead
    case BLKSECTSET: //TODO: set max sectors per request
    case BLKSECTGET: //TODO: get max sectors per request
#endif
    case BLKFLSBUF:
        dms_printk(LOG_LVL_INFO, "DMSC INFO flush dirty pages in blk dev\n");
        /*
         * TODO sample code:
         * https://lkml.org/lkml/2007/1/15/197
         * request_queue_t *q = bdev_get_queue(bdev);
         * struct request *rq = blk_get_request(q, WRITE, GFP_WHATEVER);
         * int ret;
         * ret = blk_execute_rq(q, bdev->bd_disk, rq, 0);
         * blk_put_request(rq);
         * return ret;
         */
        return 0;
    case BLKGETSIZE: /* return device size /512 (long *arg) */
        dms_printk(LOG_LVL_INFO, "DMSC INFO blkdev ioctl get volume size\n");
        if (!arg) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN BLKGETSIZE: null argument\n");
            DMS_WARN_ON(true);
            return -EINVAL; /* NULL pointer: not valid */
        }

        size = (current_drv->capacity >> BIT_LEN_SECT_SIZE);

        if (copy_to_user ((void __user *)arg, &size, sizeof (size))) {
            return -EFAULT;
        }

        return 0;
    case BLKSSZGET:
        dms_printk(LOG_LVL_INFO,
                "DMSC INFO blkdev ioctl get logical blk size\n");

        size = current_drv->logical_sect_size;

        if (copy_to_user((void __user *)arg, &size, sizeof(size))) {
            return -EFAULT;
        }

        return 0;
    case BLKBSZGET:
        dms_printk(LOG_LVL_INFO,
                "DMSC INFO blkdev ioctl get kernel blk size\n");

    	size = current_drv->kernel_sect_size;
        if (copy_to_user((void __user *)arg, &size, sizeof(size))) {
            return -EFAULT;
        }
    	return 0;
    default:
        dms_printk(LOG_LVL_INFO, "DMSC INFO blkdev ioctl unknown ioctl %u, "
                   "(the 21264 cmd is a cdrom ioctl command, don't care!)\n",
                   cmd);
        break;
    }

    return -ENOTTY; /* unknown command */
}

static int32_t block_device_open_common (volume_device_t *volume, fmode_t mode)
{
    int32_t count, open_as_rw, has_perm_to_open;

    if (IS_ERR_OR_NULL(volume)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN open NULL volume ptr fail\n");
        DMS_WARN_ON(true);
        return -EFAULT;
    }

    if (is_vol_detaching(volume)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN volume detaching, reject open volume\n");
        return -EPERM;
    }

    open_as_rw = (mode & FMODE_WRITE) >> 1;
    has_perm_to_open = ~(is_vol_ro(volume) & open_as_rw);

    if (!has_perm_to_open) {
        dms_printk(LOG_LVL_INFO,
                "DMSC INFO user doesn't has permission to open, "
                "reject open volume\n");
        return -EPERM;
    }

    count = atomic_add_return(1, &volume->io_cnt.disk_open_cnt);
    dms_printk(LOG_LVL_DEBUG, "DMSC INFO [%s] opened volume: %s count = %d\n",
            get_current()->comm, volume->disk->disk_name, count);

    return 0;
}

static int32_t block_device_release_common (volume_device_t *volume,
		fmode_t mode)
{
	int32_t count;
	uint32_t volid;

    if (IS_ERR_OR_NULL(volume)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN close NULL volume ptr fail\n");
        DMS_WARN_ON(true);
        return -EFAULT;
    }

    volid = volume->vol_id;
    count = atomic_sub_return(1, &volume->io_cnt.disk_open_cnt);
    dms_printk(LOG_LVL_DEBUG, "DMSC INFO [%s] closed volume: %s%d count = %d\n",
                 get_current()->comm, DEVICE_NAME_PREFIX, volid, count);

    return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32)
int volume_open (struct block_device *bd, fmode_t mode)
{
    if (IS_ERR_OR_NULL(bd)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN open a null block dev\n");
        return -EFAULT;
    }

    return block_device_open_common((volume_device_t*)
                                    bd->bd_disk->private_data, mode);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
int volume_release (struct gendisk *gd, fmode_t mode)
{
    if (IS_ERR_OR_NULL(gd)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN close a null gd\n");
        return -EFAULT;
    }

    return block_device_release_common((volume_device_t*)
                                       gd->private_data, mode);
}
#else
void volume_release (struct gendisk *gd, fmode_t mode)
{
    if (IS_ERR_OR_NULL(gd)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN close a null gd\n");
        return;
    }

    block_device_release_common((volume_device_t*)
                                       gd->private_data, mode);
}
#endif

#else
struct bdev_inode {
    struct block_device bdev;
    struct inode vfs_inode;
};

static inline struct bdev_inode *BDEV_I (struct inode *inode)
{
    return container_of(inode, struct bdev_inode, vfs_inode);
}

int volume_open (struct inode *inode, struct file *file)
{
    volume_device_t* device;

    if (IS_ERR_OR_NULL(inode)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN open a null inode\n");
        return -EFAULT;
    }

    device = (volume_device_t *)(BDEV_I(inode)->bdev.bd_disk->private_data);
    return block_device_open_common(device, file ? file->f_mode : 0);
}

int volume_release (struct inode *inode, struct file *file)
{
    volume_device_t* device;

    if (IS_ERR_OR_NULL(inode)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN close a null inode\n");
        return -EFAULT;
    }

    device = (volume_device_t *)(BDEV_I(inode)->bdev.bd_disk->private_data);
    return block_device_release_common(device, file ? file->f_mode : 0);
}
#endif

static struct block_device_operations dmsc_volume_fops = {
    .owner = THIS_MODULE,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32)
    .ioctl = volume_blkdev_ioctl,
#else
    .unlocked_ioctl = volume_blkdev_ioctl,
#endif
    .getgeo = volume_getgeo,
    .open = volume_open,
    .release = volume_release,
};

static vol_state_type_t update_vol_state (volume_device_t *vol,
        vol_state_op_t vol_op)
{
    vol_state_type_t old_state;

    down_write(&vol->vol_state_lock);
    old_state = vol->vol_state;
    vol->vol_state = vol_state_transition[old_state][vol_op];
    up_write(&vol->vol_state_lock);

    return old_state;
}

static void set_vol_state (volume_device_t *vol, vol_state_type_t vol_stat)
{
    down_write(&vol->vol_state_lock);
    vol->vol_state = vol_stat;
    up_write(&vol->vol_state_lock);
}

static vol_state_type_t get_vol_state (volume_device_t *vol)
{
    vol_state_type_t state;

    down_read(&vol->vol_state_lock);
    state = vol->vol_state;
    up_read(&vol->vol_state_lock);

    return state;
}

static void init_vol_state (volume_device_t *vol)
{
    init_rwsem(&vol->vol_state_lock);
    vol->vol_state = Vol_Attaching;
    vol->is_vol_stop = false;
}

static dev_minor_num_t *alloc_dev_minor_number (void)
{
    dev_minor_num_t *cur, *next, *ret;

    ret = NULL;

    mutex_lock(&mnum_list_lock);

    if (!list_empty(&mnum_free_list)) {
        list_for_each_entry_safe (cur, next, &mnum_free_list, list) {
            if (unlikely(IS_ERR_OR_NULL(cur))) {
                dms_printk(LOG_LVL_WARN,
                        "DMSC WARN dev minor num list broken\n");
                DMS_WARN_ON(true);
                break;
            }

            ret = cur;
            if (dms_list_move_tail(&cur->list, &mnum_using_list)) {
                dms_printk(LOG_LVL_WARN, "DMSC WARN list_move_tail chk fail %s %d\n",
                        __func__, __LINE__);
                DMS_WARN_ON(true);
                msleep(1000);
            }
            break;
        }
    } else {
        dms_printk(LOG_LVL_WARN, "DMSC WARN all minor number ran out\n");
    }

    mutex_unlock(&mnum_list_lock);

    DMS_WARN_ON(IS_ERR_OR_NULL(ret));

    return ret;
}

static void release_dev_minor_number (dev_minor_num_t *mnum)
{
    if (unlikely(IS_ERR_OR_NULL(mnum))) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN release null minor number\n");
        DMS_WARN_ON(true);
        return;
    }

    mutex_lock(&mnum_list_lock);

    if (dms_list_move_tail(&mnum->list, &mnum_free_list)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN list_move_tail chk fail %s %d\n",
                __func__, __LINE__);
        DMS_WARN_ON(true);
        msleep(1000);
    }
    mutex_unlock(&mnum_list_lock);
}

static void free_minor_num_generator (void)
{
    uint32_t free_cnt;
    dev_minor_num_t *cur, *next;

    if (unlikely(!list_empty(&mnum_using_list))) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN someone using minor number\n");
        DMS_WARN_ON(true);
        return;
    }

    free_cnt = 0;
    list_for_each_entry_safe (cur, next, &mnum_free_list, list) {
        if (unlikely(IS_ERR_OR_NULL(cur))) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN dev minor num list broken\n");
            DMS_WARN_ON(true);
            break;
        }

        if (dms_list_del(&cur->list)) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN list_del chk fail %s %d\n",
                            __func__, __LINE__);
                    DMS_WARN_ON(true);
                    msleep(1000);
            continue;
        }

        discoC_mem_free(cur);
        free_cnt++;
    }

    DMS_WARN_ON(free_cnt != max_attach_volumes);
}

static int32_t init_minor_num_generator (uint32_t max_att_vol)
{
    uint32_t i, max_minor_num;
    dev_minor_num_t *mn_item;

    INIT_LIST_HEAD(&mnum_free_list);
    INIT_LIST_HEAD(&mnum_using_list);
    mutex_init(&mnum_list_lock);
    max_minor_num = DEV_MINOR_START + max_att_vol;

    for (i = DEV_MINOR_START; i < max_minor_num; i++) {
        mn_item = discoC_mem_alloc(sizeof(dev_minor_num_t), GFP_KERNEL);

        if (unlikely(IS_ERR_OR_NULL(mn_item))) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN fail to generate dev minor num\n");
            DMS_WARN_ON(true);
            goto ALLOC_MINOR_ERROR;
        }

        INIT_LIST_HEAD(&mn_item->list);
        mn_item->minor_number = i*DEV_DISK_MINORS;
        list_add_tail(&mn_item->list, &mnum_free_list);
    }


    return 0;

ALLOC_MINOR_ERROR:
    free_minor_num_generator();
    return -ENOMEM;
}

static void init_minor_perf_data (perf_minor_data_t *pminor_d)
{
    atomic64_set(&pminor_d->total_cnt, 0);
    pminor_d->total_time = 0;
    pminor_d->wait_mdata_t = 0;
    pminor_d->wait_pdata_t = 0;
}

static void init_main_perf_data (perf_main_data_t *pmain_d)
{
    atomic64_set(&pmain_d->total_cnt, 0);
    pmain_d->total_sectors_done = 0;
    pmain_d->total_sectors_req = 0;
    pmain_d->total_time = 0;
    pmain_d->total_time_user = 0;
}

static void init_vol_perf_data_gut (volume_perf_data_t *vol_pdata)
{
    init_main_perf_data(&vol_pdata->perf_io);

    vol_pdata->total_in_ovlp_time = 0;
    atomic64_set(&vol_pdata->total_in_ovlp_cnt, 0);

    init_main_perf_data(&vol_pdata->perf_read);
    init_minor_perf_data(&vol_pdata->perf_read_cm);
    init_minor_perf_data(&vol_pdata->perf_read_ch);

    init_main_perf_data(&vol_pdata->perf_write);
    init_minor_perf_data(&vol_pdata->perf_write_fw4a);
    init_minor_perf_data(&vol_pdata->perf_write_fw4nano);
    init_minor_perf_data(&vol_pdata->perf_write_fw4nao);
    init_minor_perf_data(&vol_pdata->perf_write_ow4a_cm);
    init_minor_perf_data(&vol_pdata->perf_write_ow4a_ch);
    init_minor_perf_data(&vol_pdata->perf_write_ow4na_cm);
    init_minor_perf_data(&vol_pdata->perf_write_ow4na_ch);
}

static void init_vol_perf_data (volume_device_t *vol)
{
    volume_perf_data_t *vol_pdata;

    vol_pdata = &vol->perf_data;

    mutex_init(&vol_pdata->perf_lock);
    init_vol_perf_data_gut(vol_pdata);
}

void reset_vol_perf_data (volume_device_t *vol)
{
    volume_perf_data_t *vol_pdata;

    vol_pdata = &vol->perf_data;

    mutex_lock(&vol_pdata->perf_lock);
    init_vol_perf_data_gut(vol_pdata);
    mutex_unlock(&vol_pdata->perf_lock);
}

void vm_update_perf_data (io_request_t *io_req)
{
    volume_device_t *vol;
    volume_perf_data_t *vol_pdata;
    perf_main_data_t *p_major;
    perf_minor_data_t *p_minor;
    uint64_t period, period_commit_user;
    uint64_t period_wait_metadata, period_wait_payload;
    ioSeg_req_t *cur_ioSReq, *next_ioSReq, *ioSegReq;
    bool log_timeinfo;

    if (false == dms_client_config->show_performance_data) {
        return;
    }

    if (unlikely(IS_ERR_OR_NULL(io_req) || IS_ERR_OR_NULL(io_req->drive))) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN update vol perf data io req NULL ptr\n");
        DMS_WARN_ON(true);
        return;
    }

    if (time_after64(io_req->t_ioReq_create, io_req->t_ioReq_ciMDS) ||
            time_after64(io_req->t_ioReq_create, io_req->t_ioReq_ciUser)) {
        return;
    }

    vol = io_req->drive;
    vol_pdata = &vol->perf_data;

    period = jiffies_to_usecs(io_req->t_ioReq_ciMDS - io_req->t_ioReq_create);
    period_commit_user = jiffies_to_usecs(io_req->t_ioReq_ciUser - io_req->t_ioReq_create);

    log_timeinfo = false;
    /*
     * TODO: For avoid calculation each io, we should pre-compute warn value,
     *       and modify it when ior_perf_warn be changed.
     *       i.e register an callback function to configuration module
     */
    if (period_commit_user >= dms_client_config->ior_perf_warn*1000000) {
        log_timeinfo = true;
        dms_printk(LOG_LVL_INFO,
                "DMSC INFO PERF ior slow ci user %llu ci nn %llu (secs)\n",
                period_commit_user/1000000, period/1000000);
    }

    mutex_lock(&vol_pdata->perf_lock);
    p_major = &vol_pdata->perf_io;
    p_major->total_time += period;

    if ((int64_t)(p_major->total_time) < 0) {
        // overflow, re-initialize it
        init_vol_perf_data_gut(vol_pdata);
        p_major->total_time = period;
    }

    p_major->total_time_user += period_commit_user;
    p_major->total_sectors_done += (io_req->usrIO_addr.lbid_len << BIT_LEN_DMS_LB_SECTS);
    p_major->total_sectors_req += io_req->nr_ksects;

    atomic64_inc(&p_major->total_cnt);

    if (io_req->is_ovlp_ior) {
        period = jiffies_to_usecs(io_req->t_ioReq_start - io_req->t_ioReq_create);
        vol_pdata->total_in_ovlp_time += period;
        atomic64_inc(&vol_pdata->total_in_ovlp_cnt);

        if (log_timeinfo) {
            dms_printk(LOG_LVL_INFO,
                    "DMSC INFO PERF wait at ovlp q %llu (usecs)\n", period);
        }
    }

    if (KERNEL_IO_READ == io_req->iodir) {
        p_major = &vol_pdata->perf_read;
    } else {
        p_major = &vol_pdata->perf_write;
    }

    p_major->total_time += period;
    p_major->total_time_user += period_commit_user;
    atomic64_inc(&p_major->total_cnt);
    p_major->total_sectors_done += (io_req->usrIO_addr.lbid_len << BIT_LEN_DMS_LB_SECTS);
    p_major->total_sectors_req += io_req->nr_ksects;

    list_for_each_entry_safe (cur_ioSReq, next_ioSReq,
                                 &(io_req->ioSegReq_lPool), list2ioReq) {
        if (IS_ERR_OR_NULL(cur_ioSReq)) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN io req nn list broken when update perf data\n");
            DMS_WARN_ON(true);
            break;
        }

        ioSegReq = cur_ioSReq;
        period_wait_payload = jiffies_to_usecs(
                ioSegReq->t_rcv_DNResp - ioSegReq->t_send_DNReq);
        period_wait_metadata = jiffies_to_usecs(
                ioSegReq->t_rxMDReqAck - ioSegReq->t_txMDReq);

        if (log_timeinfo) {
            dms_printk(LOG_LVL_INFO,
                    "DMSC INFO PERF wait md %llu wait payload %llu (secs) "
                    "rw:%d ch:%d lb_align:%d fw:%d\n",
                    period_wait_metadata/1000000,
                    period_wait_payload/1000000,
                    io_req->iodir, ioSReq_chk_MDCacheHit(ioSegReq),
                    ioSReq_chk_4KAlign(ioSegReq), ioSegReq->mdata_opcode);
        }

        if (KERNEL_IO_READ == io_req->iodir) {
            if (ioSReq_chk_MDCacheHit(ioSegReq) == false) {
                p_minor = &vol_pdata->perf_read_cm;
                p_minor->wait_mdata_t += period_wait_metadata;
            } else {
                p_minor = &vol_pdata->perf_read_ch;
            }
        } else if (REQ_NN_QUERY_MDATA_WRITEIO == ioSegReq->mdata_opcode) {
            if (ioSReq_chk_4KAlign(ioSegReq) &&
                    ioSReq_chk_MDCacheHit(ioSegReq)) {
                p_minor = &vol_pdata->perf_write_ow4a_ch;
            } else if (ioSReq_chk_4KAlign(ioSegReq) &&
                    (ioSReq_chk_MDCacheHit(ioSegReq) == false)) {
                p_minor = &vol_pdata->perf_write_ow4a_cm;
                p_minor->wait_mdata_t += period_wait_metadata;
            } else if (ioSReq_chk_4KAlign(ioSegReq) == false &&
                    ioSReq_chk_MDCacheHit(ioSegReq)) {
                p_minor = &vol_pdata->perf_write_ow4na_ch;
            } else {
                p_minor = &vol_pdata->perf_write_ow4na_cm;
                p_minor->wait_mdata_t += period_wait_metadata;
            }
        } else {
            p_minor = &vol_pdata->perf_write_fw4a;
            p_minor->wait_mdata_t += period_wait_metadata;
        }

        p_minor->total_time += period;
        atomic64_inc(&p_minor->total_cnt);
        p_minor->wait_pdata_t += period_wait_payload;

    }
    mutex_unlock(&vol_pdata->perf_lock);
}

static void init_vol_io_cnt (volume_device_t *vol)
{
    volume_io_cnt_t *vol_cnt;

    vol_cnt = &vol->io_cnt;
    atomic_set(&vol_cnt->disk_open_cnt, 0);
    vol_cnt->error_commit_cnt = 0;
    atomic_set(&vol_cnt->linger_rreq_cnt, 0);
    atomic_set(&vol_cnt->linger_wreq_cnt, 0);
}

static void free_volume (volume_device_t *vol)
{
    uint32_t vol_id;
    uint32_t ret;

    vol_id = vol->vol_id;

    ret = NNClientMgr_deref_NNClient(vol->nnIPAddr, dms_client_config->mds_ioport);
    if (ret == 0) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN fail to deref NNClient volume %u\n",
                vol->vol_id);
    }
    clear_volumeIOReq_queue(vol_id);
    remove_volumeIOReq_queue(vol_id);

#ifdef DISCO_PREALLOC_SUPPORT
    if (free_volume_fsPool(vol_id)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN fail to free volume freespace pool volid %d\n", vol_id);
    }
#endif

    if (free_volume_MDCache(vol_id)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN fail to release volume cache for volid = %d\n",
                vol_id);
    }

#ifdef  ENABLE_PAYLOAD_CACHE_FOR_DEBUG
    payload_volume_release(vol_id);
#endif

    if(IS_ERR_OR_NULL(vol->vol_fc_group) == false) {
        release_fc_group(vol->vol_fc_group);
    }

    delete_sys_vol_proc(vol_id);

    if(IS_ERR_OR_NULL(vol->dev_minor_num) == false) {
        release_dev_minor_number(vol->dev_minor_num);
    }

    if (IS_ERR_OR_NULL(vol->ovw_data.ovw_memory) == false) {
        release_ovw(vol);
    }

    if (IS_ERR_OR_NULL(vol->retry_workq) == false) {
        flush_workqueue(vol->retry_workq);
        destroy_workqueue(vol->retry_workq);
    }

    discoC_mem_free(vol);
    vol = NULL;
}

static int32_t init_volume_device (volume_device_t *volume)
{
    bool ret;
    uint32_t vol_id;
    uint64_t vol_cap;

    vol_id = volume->vol_id;
    vol_cap = volume->capacity;

    do {
        INIT_LIST_HEAD(&volume->active_list);
        INIT_LIST_HEAD(&volume->vol_rqs_list);
        mutex_init(&volume->vol_rqs_lock);
        atomic_set(&volume->cnt_rqs, 0);
        volume->rq_agg_wait_cnt = 0;
        volume->vol_act_stat = Vol_Deactive;
        mutex_init(&volume->vol_act_lock);
        atomic_set(&volume->w_sects, 0);
        atomic_set(&volume->r_sects, 0);
        volume->sect_start = 0;

        atomic_set(&volume->lgr_rqs, 0);

        volume->sects_shift = 0;
        if (init_ovw(volume, vol_id, vol_cap) == false) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN fail allocate ovw memory for volume id %d\n",
                    vol_id);
            DMS_WARN_ON(true);
            ret = false;
            break;
        }

        volume->retry_workq = create_workqueue(VOL_IOR_RETRY_WQ_NAME);
        if (IS_ERR_OR_NULL(volume->retry_workq)) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN fail create wq for volume id %d\n",
                    vol_id);
            DMS_WARN_ON(true);
            ret = false;
            break;
        }

        volume->nnClientID = NNClientMgr_ref_NNClient(volume->nnIPAddr,
                dms_client_config->mds_ioport);
        if (volume->nnClientID == 0) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN fail to ref NNClient volume %u\n", volume->vol_id);
        }

        volume->vol_fc_group = create_fc_group(vol_id);
        if (!IS_ERR_OR_NULL(volume->vol_fc_group)) {
            add_res_to_fc_group_by_hostname(volume->nnIPAddr,
                    dms_client_config->mds_ioport, volume->vol_fc_group, RES_TYPE_NN);
        } else {
            dms_printk(LOG_LVL_INFO,
                    "DMSC INFO fail create fc group volume id %d\n",
                    vol_id);
            DMS_WARN_ON(true);
        }

#ifdef DISCO_PREALLOC_SUPPORT
        if (create_volume_fsPool(vol_id)) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN fail to create volume freespace pool volid %d\n",
                    vol_id);
        }
#endif

        if (create_volume_MDCache(vol_id, vol_cap) == -ENOMEM) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN fail to create volume cache for volid = %d\n",
                    vol_id);
            volume->vol_feature.mdata_cache_enable = false;
            DMS_WARN_ON(true);
        }

#ifdef ENABLE_PAYLOAD_CACHE_FOR_DEBUG
        payload_volume_create(vol_id);
#endif

        create_volumeIOReq_queue(volume->vol_id);
        init_vol_io_cnt(volume);
        init_vol_perf_data(volume);

        ret = true;
    } while (0);

    return ret;
}

//When share volume, 
static void set_vol_feature (volume_device_t *vol, bool is_share,
        bool enable_mcache, vol_rw_perm_type_t rw_perm,
        vol_protect_type_t prot_type, uint32_t replica)
{
    volume_features_t *vol_fea;

    vol_fea = &vol->vol_feature;
    vol_fea->mdata_cache_enable = ~is_share & enable_mcache;
    vol_fea->pdata_cache_enable = false;
    vol_fea->rw_permission = rw_perm;
    vol_fea->prot_method.prot_type = prot_type;
    vol_fea->prot_method.replica_factor = replica;
    vol_fea->send_rw_payload = true;
    vol_fea->do_rw_IO = true;
}

static bool create_vol_gendisk (volume_device_t *vol, volume_gd_param_t *vol_gd)
{
    struct gendisk *gd;
    spinlock_t *vol_qlock;
    struct request_queue *vol_queue;

    vol->disk = NULL;
    gd = alloc_disk(DEV_DISK_MINORS);
    if (IS_ERR_OR_NULL(gd)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN alloc_disk failed\n");
        DMS_WARN_ON(true);
        goto CREATE_GD_FAIL;
    }

    gd->major = vol_major_num;
    gd->first_minor = vol->dev_minor_num->minor_number;
    gd->fops = vol_gd->fop;
    sprintf(gd->disk_name, "%s%d", vol->nnSrv_name, vol->vol_id);
    set_capacity(gd, (vol->capacity >> BIT_LEN_SECT_SIZE));
    gd->flags = GENHD_FL_REMOVABLE;
#ifdef GENHD_FL_NO_PART_SCAN
    gd->flags |= GENHD_FL_NO_PART_SCAN;
#endif

    vol_qlock = &vol->qlock;
    spin_lock_init(vol_qlock);
    gd->queue = blk_init_queue(vol_gd->request_fn, vol_qlock);

    if (IS_ERR_OR_NULL(gd->queue)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN init blk q failed\n");
        DMS_WARN_ON(true);
        goto CREATE_GD_FAIL;
    }

    vol_queue = gd->queue;

    vol_queue->nr_requests = vol_gd->nr_reqs;

    blk_queue_logical_block_size(vol_queue, vol_gd->logical_blk_size);

    // set the max_setors to control the block size
    blk_queue_max_sectors(vol_queue, vol_gd->max_sectors);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
    blk_queue_max_hw_sectors(vol_queue, vol_gd->max_sectors);
#endif
    dms_queue_barrier(vol_queue);     // Support barrier

    gd->private_data = vol;
    vol->queue = gd->queue;
    vol->disk = gd;
    vol->request_fn = vol_gd->request_fn;

    vol->logical_sect_size = vol_gd->logical_blk_size;
    vol->kernel_sect_size = KERNEL_SECTOR_SIZE;
    return true;

CREATE_GD_FAIL:
    if (!IS_ERR_OR_NULL(gd)) {
        kfree(gd); //NOTE : allocate through alloc_disk, don't call discoC_mem_free
    }
    return false;
}

static volume_device_t *find_volume_no_lock (uint32_t vol_id,
        struct list_head *lhead)
{
    volume_device_t *vol, *cur, *next;

    vol = NULL;
    list_for_each_entry_safe (cur, next, lhead, list) {
        if (unlikely(IS_ERR_OR_NULL(cur))) {
            DMS_WARN_ON(true);
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN volume table list broken when find vol id %d\n",
                    vol_id);
            break;
        }

        if (cur->vol_id == vol_id) {
            vol = cur;
            break;
        }
    }

    return vol;
}

static volume_device_t *find_vol_gain_lock (uint32_t vol_id)
{
    struct rw_semaphore *semlock;
    uint32_t hindex;
    struct list_head *lhead;
    volume_device_t *vol;

    hindex = vol_id % VOL_HASH_NUM;
    lhead = &vol_table[hindex];
    semlock = &vol_table_lock[hindex];
    down_read(semlock);
    vol = find_volume_no_lock(vol_id, lhead);

    return vol;
}

static void find_vol_release_lock (uint32_t vol_id)
{
    struct rw_semaphore *semlock;
    uint32_t hindex;

    hindex = vol_id % VOL_HASH_NUM;
    semlock = &vol_table_lock[hindex];
    up_read(semlock);
}

static attach_response_t add_vol_to_table (volume_device_t *vol)
{
    struct list_head *lhead;
    struct rw_semaphore *semlock;
    uint32_t hindex, vol_id;
    volume_device_t *old_vol;
    attach_response_t ret;

    vol_id = vol->vol_id;
    hindex = vol_id % VOL_HASH_NUM;
    lhead = &vol_table[hindex];
    semlock = &vol_table_lock[hindex];

    down_write(semlock);

    old_vol = find_volume_no_lock(vol->vol_id, lhead);

    if (IS_ERR_OR_NULL(old_vol)) {
        do {
            vol->dev_minor_num = alloc_dev_minor_number();

            if (IS_ERR_OR_NULL(vol->dev_minor_num)) {
                dms_printk(LOG_LVL_INFO,
                        "DMSC INFO fail get minor number for vol %d\n", vol_id);
                ret = Attach_Vol_Fail;
                break;
            }

            if (dms_list_add_tail(&vol->list, lhead)) {
                dms_printk(LOG_LVL_WARN,
                        "DMSC WARN list_add_tail chk fail %s %d\n",
                        __func__, __LINE__);
                msleep(1000);
            } else {
                atomic_inc(&num_of_vol_attached);
            }

            ret = Attach_Vol_OK;
        } while (0);
    } else {
        switch (old_vol->vol_state) {
        case Vol_Attaching:
            ret = Attach_Vol_Exist_Attaching;
            break;
        case Vol_Detaching:
            ret = Attach_Vol_Exist_Detaching;
            break;
        case Vol_Detached:
            ret = Attach_Vol_Exist_Detaching;
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN vol %d detached but still at volume table\n",
                    old_vol->vol_id);
            DMS_WARN_ON(true);
            break;
        default:
            ret = Attach_Vol_Exist;
            break;
        }
    }

    up_write(semlock);

    return ret;
}

static bool rm_vol_from_table (uint32_t vol_id)
{
    struct list_head *lhead;
    struct rw_semaphore *semlock;
    uint32_t hindex;
    volume_device_t *vol;
    detach_response_t ret;

    hindex = vol_id % VOL_HASH_NUM;
    lhead = &vol_table[hindex];
    semlock = &vol_table_lock[hindex];

    down_write(semlock);
    vol = find_volume_no_lock(vol_id, lhead);

    if (IS_ERR_OR_NULL(vol)) {
        ret = false;
    } else {
        if (dms_list_del(&vol->list)) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN list_del chk fail %s %d\n",
                    __func__, __LINE__);
            DMS_WARN_ON(true);
            msleep(1000);
        } else {
            atomic_dec(&num_of_vol_attached);
            release_dev_minor_number(vol->dev_minor_num);
            vol->dev_minor_num = NULL;
        }

        ret = true;
    }

    up_write(semlock);

    return ret;
}

volume_device_t *find_volume_for_detach (int32_t vol_id,
        detach_response_t *ret)
{
    volume_device_t *vol;
    struct list_head *lhead;
    struct rw_semaphore *bucket_lock;
    uint32_t hindex;
    vol_state_type_t old_state;

    hindex = vol_id % VOL_HASH_NUM;
    lhead = &vol_table[hindex];
    bucket_lock = &vol_table_lock[hindex];

    down_write(bucket_lock);

    do {
        vol = find_volume_no_lock(vol_id, lhead);
        if (IS_ERR_OR_NULL(vol)) {
            *ret = Detach_Vol_NotExist;
            dms_printk(LOG_LVL_INFO, "DMSC INFO detach a non-exist vol %d\n",
                    vol_id);
            break;
        }

        if (atomic_read(&vol->io_cnt.disk_open_cnt) > 0) {
            dms_printk(LOG_LVL_INFO, "DMSC INFO detach a busy vol %d\n",
                    vol_id);
            *ret = Detach_Vol_Exist_Busy;
            break;
        }

        //TODO: become part of state transition
        if (get_vol_state(vol) == Vol_IO_Blocking) {
            dms_printk(LOG_LVL_INFO,
                    "DMSC INFO detach a io blocking vol %d\n",
                    vol_id);
            *ret = Detach_Vol_Fail;
            break;
        }

        old_state = update_vol_state(vol, Vol_Detach);
        if (Vol_Detaching == old_state || Vol_IO_Block_Detach == old_state) {
            dms_printk(LOG_LVL_INFO, "DMSC INFO detach a detaching vol %d\n",
                    vol_id);
            *ret = Detach_Vol_Exist_Detaching;
            break;
        }

        *ret = Detach_Vol_OK;
    } while (0);

    if (Detach_Vol_OK != *ret) {
        vol = NULL;
    }

    up_write(bucket_lock);

    return vol;
}

static bool is_vol_detaching (volume_device_t *vol)
{
    bool ret;
    vol_state_type_t stat;

    stat = get_vol_state(vol);
    switch (stat) {
    case Vol_Detached:
    case Vol_Detaching:
    case Vol_IO_Block_Detach:
        ret = true;
        break;
    default:
        ret = false;
        break;
    }


    return ret;
}

/* NOTE: If volume in detaching state, return NULL
 *       If user want do something even volume is detaching,
 *       using find_vol_gain_lock and find_vol_release_lock
 */
volume_device_t *find_volume (uint32_t vol_id)
{
    volume_device_t *vol;
    struct list_head *lhead;
    struct rw_semaphore *bucket_lock;
    uint32_t hindex;

    hindex = vol_id % VOL_HASH_NUM;
    lhead = &vol_table[hindex];
    bucket_lock = &vol_table_lock[hindex];

    down_read(bucket_lock);
    vol = find_volume_no_lock(vol_id, lhead);
    if (!IS_ERR_OR_NULL(vol) && is_vol_detaching(vol)) {
        vol = NULL;
    }
    up_read(bucket_lock);

    return vol;
}

static void flush_gendisk_queue (struct request_queue *disk_queue)
{

    unsigned long flags;

    spin_lock_irqsave(disk_queue->queue_lock, flags);
    /*
     * NOTE: stop disk_queue, it has to get lock before call stop queue.
     *       stop block device queue, application will at blocked at
     *       system call read() / write ()
     */
    blk_stop_queue (disk_queue);

    spin_unlock_irqrestore(disk_queue->queue_lock, flags);

    // sync any remained requests
    blk_sync_queue(disk_queue);
}

static void start_gendisk_queue (struct request_queue *disk_queue)
{

    unsigned long flags;

    spin_lock_irqsave(disk_queue->queue_lock, flags);
    blk_start_queue (disk_queue);

    spin_unlock_irqrestore(disk_queue->queue_lock, flags);
}

//TODO: wait_vol_io_done
static bool flush_vol_io_too_long (uint64_t start_t)
{
    uint64_t curr_t, time_period;
    uint32_t flush_timeout;
    bool ret;

    curr_t = jiffies_64;

    flush_timeout = dms_client_config->max_io_flush_wait_time;
    if (time_after64(start_t, curr_t)) {
        ret = false;
    } else {
        time_period = jiffies_to_msecs(curr_t - start_t) / 1000L;
        ret = time_period < flush_timeout ? false : true;
    }

    return ret;
}

bool is_vol_io_done (volume_device_t *vol, bool chk_rqs)
{
    volume_io_cnt_t *io_cnt;

    io_cnt = &vol->io_cnt;

    if (chk_rqs) {
        return ((0 == atomic_read(&io_cnt->linger_rreq_cnt)) &&
                (0 == atomic_read(&io_cnt->linger_wreq_cnt)) &&
                (0 == atomic_read(&vol->lgr_rqs)));
    } else {
        return ((0 == atomic_read(&io_cnt->linger_rreq_cnt)) &&
                (0 == atomic_read(&io_cnt->linger_wreq_cnt)));
    }
}

bool wait_vol_io_done (volume_device_t *vol, bool chk_rqs)
{
    uint64_t start_t;
    bool ret;

    start_t = jiffies_64;
    ret = true;

    while (!is_vol_io_done(vol, chk_rqs)) {
        /*
         * #define FLUSH_IO_LINGER_IO_CHECK_TIME_PERIOD 5
         * wait_event_interruptible_timeout(wq_volume,
         *                         IS_ALL_IO_DONE(volume),
         *                         FLUSH_IO_LINGER_IO_CHECK_TIME_PERIOD *
         *                                                            1000);
         * TODO: change to sleep and wakeup and timeout
         *       checking whether wait to long
         */
        msleep(FLUSH_IO_CHECK_TIME_PERIOD);
        if (flush_vol_io_too_long(start_t)) {
            ret = false;
            dms_printk(LOG_LVL_WARN,
                    "DMSC INFO Takes too long time to wait io finish of vol %d "
                    "(10 min)\n", vol->vol_id);
            break;
        }
    }

    return ret;
}

detach_response_t detach_volume (uint32_t nnIPAddr, uint32_t vol_id)
{
    volume_device_t *vol;
    detach_response_t ret;

    do {
        vol = find_volume_for_detach(vol_id, &ret);
        if (IS_ERR_OR_NULL(vol)) {
            break;
        }

        /* TODO:If state is Vol_IO_Block_Detach, the io cnt will == 0,
         *      So, we should wait for state become Vol_IO_Detaching
         */

        flush_gendisk_queue(vol->queue);

        if (wait_vol_io_done(vol, true) == false) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC Cannot proper flush IO due to some IO cannot be "
                    "removed volume id %d\n", vol_id);
            start_gendisk_queue(vol->queue);
            //TODO: should define detach fail operation
            set_vol_state(vol, Vol_Ready_for_User);
            ret = Detach_Vol_Exist_Busy;
            break;
        }

        /* NOTE: Don't put del_gendisk and put_disk beyond rm_vol_from_table
         *       Avoiding add the disk with the same name to kernel and cause
         *       crash
         */
        del_gendisk(vol->disk);
        put_disk(vol->disk);

        blk_cleanup_queue(vol->queue);

        if (rm_vol_from_table(vol_id) == false) {
            ret = Detach_Vol_NotExist;
            break;
        }

        update_vol_state(vol, Vol_No_Ligner_IO);

        free_volume(vol);
        ret = Detach_Vol_OK;
    } while (0);

    return ret;
}

static void set_vol_yield_io (uint32_t hindex, vol_state_op_t vol_op)
{
#if 0
    //TODO: debug: when enable this, cannot pass parallel-volume-test.sh
    struct list_head *lhead;
    struct rw_semaphore *bucket_lock;
    volume_device_t *cur, *next;

    lhead = &vol_table[hindex];
    bucket_lock = &vol_table_lock[hindex];

    down_read(bucket_lock);
    list_for_each_entry_safe (cur, next, lhead, list) {
        if (unlikely(IS_ERR_OR_NULL(cur))) {
            DMS_WARN_ON(true);
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN volume table list broken\n");
            break;
        }

        update_vol_state(cur, vol_op);
    }
    up_read(bucket_lock);
#endif
}

/* NOTE: an attach command comes, set other volumes for yield io
 *       or attach finish, set other volumes for not yield io
 */
static void set_all_vol_yield_io (vol_state_op_t vol_op)
{
    //TODO: debug: when enable this, cannot pass parallel-volume-test.sh
    uint32_t i;

    switch (vol_op) {
    case Vol_Yield_IO:
    case Vol_NoYield_IO:
        if (Vol_Yield_IO == vol_op) {
            atomic_inc(&cnt_attach_ongoing);
        } else {
            if (atomic_sub_return(1, &cnt_attach_ongoing) > 0) {
                break;
            }
        }
        for (i = 0; i < VOL_HASH_NUM; i++) {
            set_vol_yield_io(i, vol_op);
        }
        break;
    default:
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN pass wroing op code %d to set yield state\n",
                vol_op);
        DMS_WARN_ON(true);
        break;
    }
}

bool is_vol_yield_io (volume_device_t *vol)
{
    vol_state_type_t stat;

    stat = get_vol_state(vol);

    if (Vol_IO_Yielding == stat || Vol_IO_Block_Yield == stat) {
        return true;
    }

    return false;
}

bool is_vol_block_io (volume_device_t *vol)
{
    vol_state_type_t stat;

    stat = get_vol_state(vol);

    if (Vol_IO_Blocking == stat || Vol_IO_Block_Yield == stat ||
            Vol_IO_Block_Detach == stat) {
        return true;
    }

    return false;
}

static void set_vol_rw_permission (volume_device_t *vol,
        vol_rw_perm_type_t value)
{
    vol->vol_feature.rw_permission = value;
}

static bool is_vol_ro (volume_device_t *vol)
{
    return (R_ONLY == vol->vol_feature.rw_permission);
}

void set_vol_send_rw_payload (volume_device_t *vol, bool send_rw_payload)
{
    vol->vol_feature.send_rw_payload = send_rw_payload;
}

bool is_vol_send_rw_payload (volume_device_t *vol)
{
    return vol->vol_feature.send_rw_payload;
}

void set_vol_do_rw_IO (volume_device_t *vol, bool do_rw_IO)
{
    vol->vol_feature.do_rw_IO = do_rw_IO;
}

bool is_vol_do_rw_IO (volume_device_t *vol)
{
    return vol->vol_feature.do_rw_IO;
}

void set_volume_rw_permission (uint32_t vol_id, vol_rw_perm_type_t rw_perm)
{
    volume_device_t *vol;

    vol = find_vol_gain_lock(vol_id);

    if (!IS_ERR_OR_NULL(vol)) {
        set_vol_rw_permission(vol, rw_perm);
    }

    find_vol_release_lock(vol_id);
}

bool is_vol_mcache_enable (volume_device_t *vol)
{
    if (!dms_client_config->enable_metadata_cache) {
        return false;
    }

    return vol->vol_feature.mdata_cache_enable;
}

void set_vol_mcache (volume_device_t *vol, bool enable)
{
    vol->vol_feature.mdata_cache_enable = enable;
}

attach_response_t attach_volume (int8_t *nnSrvNM, uint32_t nnIPaddr,
        uint32_t vol_id, uint64_t capacity,
        bool is_share, bool mcache_enable, vol_rw_perm_type_t rw_perm,
        uint32_t replica, volume_gd_param_t *vol_gd)
{
    volume_device_t *vol;
    uint32_t curr_vols;
    uint16_t random_seed;
    attach_response_t ret;

    curr_vols = atomic_read(&num_of_vol_attached);
    if (curr_vols == max_attach_volumes) {
        dms_printk(LOG_LVL_INFO,
                "DMSC INFO fail to attach vol %d due to reach max volume %d "
                "(current %d)\n",
                vol_id, max_attach_volumes, curr_vols);
        return Attach_Vol_Max_Volumes;
    }

    vol = (volume_device_t *)discoC_mem_alloc(sizeof(volume_device_t),
            GFP_NOWAIT | GFP_ATOMIC);

    do {
        if (unlikely(IS_ERR_OR_NULL(vol))) {
            ret = Attach_Vol_Fail;
            break;
        }

        get_random_bytes(&random_seed, sizeof(uint16_t));
        vol->dnSeltor.readDN_idx = random_seed % replica;
        vol->dnSeltor.readDN_numLB = 0;
        spin_lock_init(&vol->dnSeltor.readDN_lock);

        vol->nnIPAddr = nnIPaddr;
        vol->vol_id = vol_id;
        vol->capacity = capacity;
        init_vol_state(vol);
        INIT_LIST_HEAD(&vol->list);
        strncpy(vol->nnSrv_name, nnSrvNM, MAX_LEN_NNSRV_NM);

        ret = add_vol_to_table(vol);

        if (ret != Attach_Vol_OK) {
            discoC_mem_free(vol);
            break;
        }

        set_vol_feature(vol, is_share, mcache_enable, rw_perm, Vol_Data_Replic,
                replica);

        if (init_volume_device(vol) == false) {
            (void)rm_vol_from_table(vol_id);
            free_volume(vol);
            ret = Attach_Vol_Fail;
            break;
        }

        if (create_vol_gendisk(vol, vol_gd) == false) {
            (void)rm_vol_from_table(vol_id);
            free_volume(vol);
            ret = Attach_Vol_Fail;
            break;
        }

        set_all_vol_yield_io(Vol_Yield_IO);

        add_disk(vol->disk);
        create_sys_vol_proc(vol->vol_id);
        ret = Attach_Vol_OK;
    } while (0);


    return ret;
}

void set_volume_not_yield (volume_device_t *vol)
{
    update_vol_state(vol, Vol_NoYield_IO);
}

void set_volume_state_attach_done (uint32_t nnIPaddr, uint32_t vol_id)
{
    volume_device_t *vol;

    vol = find_vol_gain_lock(vol_id);

    if (!IS_ERR_OR_NULL(vol)) {
        update_vol_state(vol, Vol_Attach_Finish);
    }

    find_vol_release_lock(vol_id);

    set_all_vol_yield_io(Vol_NoYield_IO);
}

static void init_default_volume_gd (void)
{
    default_vol_gd.fop = &dmsc_volume_fops;
    default_vol_gd.request_fn = dms_io_request_fn;
    default_vol_gd.nr_reqs = MAX_NUM_KERNEL_RQS;
    default_vol_gd.logical_blk_size = KERNEL_LOGICAL_SECTOR_SIZE;
    default_vol_gd.max_sectors = MAX_SECTS_PER_RQ;
}

attach_response_t attach_volume_default_gd (int8_t *nnSrvNM,
        uint32_t nnIPaddr, uint32_t vol_id,
        uint64_t capacity, bool is_share, bool mcache_enable,
        vol_rw_perm_type_t rw_perm, uint32_t replica)
{
    return attach_volume(nnSrvNM, nnIPaddr, vol_id, capacity, is_share, mcache_enable, rw_perm,
            replica, &default_vol_gd);
}

void vm_reset_allvol_iocnt (void)
{
    volume_device_t *vol;

    vol = NULL;
    while (IS_ERR_OR_NULL(vol = iterate_all_volumes(vol)) == false) {
        init_vol_io_cnt(vol);
    }
}

void reset_vol_iocnt (volume_device_t *vol)
{
    volume_io_cnt_t *vol_io_cnt;

    vol_io_cnt = &vol->io_cnt;
    atomic_set(&vol_io_cnt->linger_wreq_cnt, 0);
    atomic_set(&vol_io_cnt->linger_rreq_cnt, 0);
}

void vm_dec_iocnt (volume_device_t *vol_dev, uint16_t iorw)
{
    volume_io_cnt_t *vol_io_cnt;

    vol_io_cnt = &vol_dev->io_cnt;

    switch (iorw) {
    case KERNEL_IO_WRITE:
        atomic_dec(&vol_io_cnt->linger_wreq_cnt);
        break;
    case KERNEL_IO_READ:
        atomic_dec(&vol_io_cnt->linger_rreq_cnt);
        break;
    default:
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN unknow rw %d decrease io count\n", iorw);
        DMS_WARN_ON(true);
        break;
    }
}

void vm_inc_iocnt (volume_device_t *vol_dev, uint16_t iorw)
{
    volume_io_cnt_t *vol_io_cnt;
    vol_io_cnt = &vol_dev->io_cnt;

    switch (iorw) {
    case KERNEL_IO_WRITE:
        atomic_inc(&vol_io_cnt->linger_wreq_cnt);
        break;
    case KERNEL_IO_READ:
        atomic_inc(&vol_io_cnt->linger_rreq_cnt);
        break;
    default:
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN unknow rw type: %d\n", iorw);
        DMS_WARN_ON(true);
        break;
    }
}

void set_vol_share (volume_device_t *vol, bool is_share)
{
    vol->vol_feature.share_volume = is_share;
}

bool is_vol_share (volume_device_t *vol)
{
    return vol->vol_feature.share_volume;
}

uint32_t get_volume_replica (volume_device_t *vol)
{
    return vol->vol_feature.prot_method.replica_factor;
}

set_fea_resp_t set_volume_state_share (uint32_t vol_id, bool is_share)
{
    set_fea_resp_t ret;
    volume_device_t *vol;
    bool share_val;

    vol = find_vol_gain_lock(vol_id);

    do {
        if (IS_ERR_OR_NULL(vol)) {
            ret = Set_Fea_Vol_NoExist;
            break;
        }

        share_val = dms_client_config->enable_share_volume & is_share;

        set_vol_share(vol, share_val);
        set_vol_mcache(vol, ~share_val);

        if (share_val) {
            inval_volume_MDCache_volID(vol->vol_id);
        }

        ret = Set_Fea_OK;
    } while (0);

    find_vol_release_lock(vol_id);
    return ret;
}

void unblock_volume_io (uint32_t vol_id)
{
    volume_device_t *vol;

    vol = find_vol_gain_lock(vol_id);
    do {
        if (IS_ERR_OR_NULL(vol)) {
            break;
        }

        (void)update_vol_state(vol, Vol_UnBlock_IO);
    } while (0);

    find_vol_release_lock(vol_id);
}

unblock_io_resp_t set_volume_state_unblock_io (uint32_t vol_id)
{
    unblock_io_resp_t ret;
    volume_device_t *vol;

    vol = find_vol_gain_lock(vol_id);
    do {

        if (IS_ERR_OR_NULL(vol)) {
            ret = UnBlock_IO_Vol_NotExist;
            break;
        }

        (void)update_vol_state(vol, Vol_UnBlock_IO);
        ret = UnBlock_IO_OK;
    } while (0);

    find_vol_release_lock(vol_id);
    return ret;
}

//dms_submit_rw_request check whether volume block
block_io_resp_t set_volume_state_block_io (uint32_t vol_id)
{
    block_io_resp_t ret;
    vol_state_type_t old_state;
    volume_device_t *vol;

    vol = find_vol_gain_lock(vol_id);
    do {
        if (IS_ERR_OR_NULL(vol)) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC cannot block vol %d, vol isn't exist\n", vol_id);
            ret = Block_IO_Vol_NotExist;
            break;
        }

        old_state = update_vol_state(vol, Vol_Block_IO);
        switch (old_state) {
        case Vol_Detaching:
        case Vol_Detached:
            dms_printk(LOG_LVL_WARN,
                    "DMSC cannot block vol %d, vol is detaching\n", vol_id);
            ret = Block_IO_Vol_Detaching;
            break;
        case Vol_Attaching:
            dms_printk(LOG_LVL_WARN,
                    "DMSC cannot block vol %d, vol is attaching\n", vol_id);
            ret = Block_IO_Vol_Attaching;
            break;
        default:
            ret = Block_IO_OK;
            break;
        }

        if (Block_IO_OK != ret) {
            break;
        }

        /*
         * TODO: Should we wait forever ??
         *       Is it a good solution to gain bucket lock to wait all io done?
         */
        if (!wait_vol_io_done(vol, false)) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN Cannot proper flush IO due to "
                    "some IO cannot be removed volume id %d\n", vol_id);
            update_vol_state(vol, Vol_UnBlock_IO);
            ret = Block_IO_Fail;
            break;
        }

        ret = Block_IO_OK;
    } while (0);

    find_vol_release_lock(vol_id);
    return ret;
}

void set_vol_ovw_read_state (uint32_t vol_id, bool reading)
{
    volume_device_t *vol;

    vol = find_vol_gain_lock(vol_id);

    do {
        if (IS_ERR_OR_NULL(vol)) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC fail set ovw read state vol %d isn't exist\n", vol_id);
            break;
        }

        vol->ovw_data.ovw_reading = reading;
    } while (0);

    find_vol_release_lock(vol_id);
}

clear_ovw_resp_t clear_volume_ovw (uint32_t vol_id)
{
    volume_device_t *vol;
    clear_ovw_resp_t resp;

    vol = find_vol_gain_lock(vol_id);

    do {
        if (IS_ERR_OR_NULL(vol)) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC cannot clear ovw vol %d, vol isn't exist\n", vol_id);
            resp = Clear_Vol_NoExist;
            break;
        }

        if (!ovw_clear(vol)) {
            resp = Clear_OVW_Fail;
            break;
        }

        resp = Clear_OVW_OK;
    } while (0);

    find_vol_release_lock(vol_id);
    return resp;
}

//NOTE: if return is not NULL, bucket lock will on
static volume_device_t *get_first_one_vol (uint32_t start_index)
{
    volume_device_t *found;
    uint32_t i;
    struct list_head *lhead;
    struct rw_semaphore *bucket_lock;
    volume_device_t *cur, *next;

    found = NULL;
    for (i = start_index; i < VOL_HASH_NUM; i++) {
        lhead = &vol_table[i];
        bucket_lock = &vol_table_lock[i];

        down_read(bucket_lock);
        if (!list_empty(lhead)) {
            list_for_each_entry_safe (cur, next, lhead, list) {
                if (unlikely(IS_ERR_OR_NULL(cur))) {
                    DMS_WARN_ON(true);
                    dms_printk(LOG_LVL_WARN,
                            "DMSC WARN vol table list broken\n");
                    break;
                }

                found = cur;
                break;
            }
        }

        if (IS_ERR_OR_NULL(found)) {
            up_read(bucket_lock);
        } else {
            break;
        }
    }

    return found;
}

//NOTE: if return is not NULL, bucket lock will on
volume_device_t *iterate_all_volumes (volume_device_t *old_vol)
{
    volume_device_t *found;
    struct rw_semaphore *bucket_lock;
    uint32_t hindex;
    struct list_head *lhead;

    if (IS_ERR_OR_NULL(old_vol)) {
        found = get_first_one_vol(0);
    } else {
        hindex = old_vol->vol_id % VOL_HASH_NUM;
        bucket_lock = &vol_table_lock[hindex];
        lhead = &vol_table[hindex];

        if (list_is_last(&old_vol->list, lhead)) {
            up_read(bucket_lock);
            hindex++;
            if (hindex >= VOL_HASH_NUM) {
                found = NULL;
            } else {
                found = get_first_one_vol(hindex);
            }
        } else {
            found = list_entry((&old_vol->list)->next, volume_device_t, list);
        }
    }

    return found;
}

/* NOTE: User should call this function when using iterate_all_volumes and get
 *       a volumes, and then perform a break;
 */
void iterate_break (volume_device_t *vol)
{
    struct rw_semaphore *bucket_lock;
    uint32_t hindex;

    if (unlikely(IS_ERR_OR_NULL(vol))) {

        dms_printk(LOG_LVL_WARN,
                "DMSC WARN pass NULL ptr to break iterator\n");
        return;
    }

    hindex = vol->vol_id % VOL_HASH_NUM;
    bucket_lock = &vol_table_lock[hindex];
    up_read(bucket_lock);
}

static bool is_vol_detachable (volume_device_t *vol)
{
    bool ret;
    vol_state_type_t stat;

    do {
        if (atomic_read(&vol->io_cnt.disk_open_cnt) > 0) {
            ret = false;
            break;
        }

        stat = get_vol_state(vol);
        if (Vol_Detaching == stat || Vol_IO_Block_Detach == stat) {
            ret = false;
            break;
        }

        ret = true;
    } while (0);

    return ret;
}

static void get_all_vols_detachable (struct list_head *src_list,
        struct list_head *buff_list)
{
    volume_device_t *cur, *next;

    list_for_each_entry_safe (cur, next, src_list, list) {
        if (!is_vol_detachable(cur)) {
            continue;
        }

        if (dms_list_move_tail(&cur->list, buff_list)) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN list_move_tail chk fail %s %d\n",
                    __func__, __LINE__);
            DMS_WARN_ON(true);
            msleep(1000);
        } else {
            atomic_dec(&num_of_vol_attached);
        }
    }
}

static void free_all_vols (struct list_head *buffer_list)
{
    volume_device_t *cur, *next, *vol;
    list_for_each_entry_safe (cur, next, buffer_list, list) {
        vol = cur;

        if (dms_list_del(&cur->list)) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN list_del chk fail %s %d\n",
                    __func__, __LINE__);
            DMS_WARN_ON(true);
            msleep(1000);
            continue;
        }

        flush_gendisk_queue(vol->queue);

        if (!wait_vol_io_done(vol, true)) {
            dms_printk(LOG_LVL_WARN, "DMSC Cannot proper flush IO due to "
                    "some IO cannot be removed volume id %d\n", vol->vol_id);
            //TODO: Reset volume state, recovery gendisk q
        }

        del_gendisk(vol->disk);
        put_disk(vol->disk);

        blk_cleanup_queue(vol->queue);
        free_volume(vol);
    }
}

uint32_t detach_all_volumes (void)
{
    int32_t i, cnt;
    struct rw_semaphore *bucket_lock;
    struct list_head *lhead;
    struct list_head buffer_list;

    INIT_LIST_HEAD(&buffer_list);
    for (i = 0; i < VOL_HASH_NUM; i++) {
        lhead = &vol_table[i];
        bucket_lock = &vol_table_lock[i];
        down_write(bucket_lock);
        get_all_vols_detachable(lhead, &buffer_list);
        up_write(bucket_lock);
    }

    free_all_vols(&buffer_list);
    cnt = atomic_read(&num_of_vol_attached);
    DMS_WARN_ON(cnt > 0);
    return cnt;
}

void set_vol_major_num (int32_t blk_major)
{
    vol_major_num = blk_major;
}

void set_max_attach_vol (uint32_t max_att_vol)
{
    max_attach_volumes = max_att_vol;
}

vol_rw_perm_type_t get_vol_rw_perm (volume_device_t *vol)
{
    return vol->vol_feature.rw_permission;
}

void set_vol_rw_perm (volume_device_t *vol, vol_rw_perm_type_t io_rw)
{
    switch (io_rw) {
    case RW_PERMIT:
    case RW_NO_PERMIT:
    case R_ONLY:
    case W_ONLY:
        vol->vol_feature.rw_permission = io_rw;
        break;
    default:
        DMS_WARN_ON(true);
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN set wrong rw permission for vol %d\n", vol->vol_id);
        break;
    }
}

bool vol_no_perm_to_io (volume_device_t *vol, int32_t io_rw)
{
    switch (vol->vol_feature.rw_permission) {
    case RW_PERMIT:
        return false;
    case RW_NO_PERMIT:
        return true;
    case R_ONLY:
        return (KERNEL_IO_READ == io_rw ? false : true);
    case W_ONLY:
        return (KERNEL_IO_WRITE == io_rw ? false : true);
    default:
        DMS_WARN_ON(true);
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN wrong rw permission for vol %d\n", vol->vol_id);
        break;
    }

    return false;
}

static bool is_vol_stopable (volume_device_t *vol)
{
    vol_state_type_t vol_stat;

    vol_stat = get_vol_state(vol);

    return (Vol_Ready_for_User == vol_stat && !vol->is_vol_stop) ? true : false;
}

static bool is_vol_restartable (volume_device_t *vol)
{
    vol_state_type_t vol_stat;

    vol_stat = get_vol_state(vol);

    return (Vol_Ready_for_User == vol_stat && vol->is_vol_stop) ? true : false;
}

/*
 * NOTE: There is no protection when stop volume is ongoing.
 *       i.e receive other command at the same time
 */
stop_response_t stop_volume (uint32_t vol_id)
{
    volume_device_t *vol;
    stop_response_t ret;

    vol = find_vol_gain_lock(vol_id);

    do {
        if (IS_ERR_OR_NULL(vol)) {
            ret = Stop_Vol_NotExist;
            break;
        }

        if (is_vol_stopable(vol) == false) {
            ret = Stop_Vol_Fail;
            break;
        }

        vol->is_vol_stop = true;

        set_vol_rw_perm(vol, RW_NO_PERMIT);

        /*
         * NOTE: Don't stop block device queue, application will block at
         *       system call read() / write ()
         */

        if (wait_vol_io_done(vol, true) == false) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN io not done, keeping stop, vol %d\n", vol_id);
        }

        if (free_volume_MDCache(vol_id)) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN fail to release volume cache for vol %d\n",
                    vol_id);
        }

        if (IS_ERR_OR_NULL(vol->ovw_data.ovw_memory) == false) {
            /*
             * NOTE: Won't cause crash when receive ovw from NN.
             *       release_ovw will protect
             */
            release_ovw(vol);
        }

        ret = Stop_Vol_OK;
    } while (0);

    find_vol_release_lock(vol_id);

    return ret;
}

restart_response_t restart_volume (uint32_t vol_id)
{
    volume_device_t *vol;
    restart_response_t ret;

    vol = find_vol_gain_lock(vol_id);

    do {
        if (IS_ERR_OR_NULL(vol)) {
            ret = Restart_Vol_NotExist;
            break;
        }

        if (is_vol_restartable(vol) == false) {
            ret = Restart_Vol_Fail;
            break;
        }

        if (init_ovw(vol, vol_id, vol->capacity) == false) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN fail alloc ovw mem for restart vol %d\n",
                    vol_id);
            DMS_WARN_ON(true);
            ret = Restart_Vol_Fail;
            break;
        }

        if (create_volume_MDCache(vol_id, vol->capacity) == -ENOMEM) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN fail to create vol cache for restart vol = %d\n",
                    vol_id);
            DMS_WARN_ON(true);
        }

        set_vol_rw_perm(vol, RW_PERMIT);

        vol->is_vol_stop = false;
        ret = Restart_Vol_OK;
    } while (0);

    find_vol_release_lock(vol_id);

    return ret;
}

vol_active_op_t update_vol_active_stat (volume_device_t *vol,
        vol_active_op_t op)
{
    struct mutex *stat_lock;
    vol_active_stat_t old_stat;
    vol_active_op_t reaction;

    stat_lock = &vol->vol_act_lock;
    reaction = Vol_NoOP;

    mutex_lock(stat_lock);
    old_stat = vol->vol_act_stat;

    do {
        if (old_stat == Vol_Deactive && op == Vol_EnqRqs) {
            if (atomic_read(&vol->cnt_rqs) > 0) {
                vol->vol_act_stat = Vol_Active;
                reaction = Vol_DoEnActiveQ;
            }
            break;
        }

        if (old_stat == Vol_Active && op == Vol_RqsDone) {
            if (atomic_read(&vol->cnt_rqs) == 0) {
                vol->vol_act_stat = Vol_Deactive;
            } else {
                reaction = Vol_DoEnActiveQ;
            }
            break;
        }

        if (old_stat == Vol_Active && op == Vol_WaitForMoreRqs) {
            if (atomic_read(&vol->cnt_rqs) > 0) {
                vol->vol_act_stat = Vol_Waiting;
                reaction = Vol_DoWaiting;
            }
            break;
        }

        if (old_stat == Vol_Waiting && op == Vol_WaitDone) {
            if (atomic_read(&vol->cnt_rqs) > 0) {
                vol->vol_act_stat = Vol_Active;
                reaction = Vol_DoEnActiveQ;
            }
            break;
        }
    } while (0);

    mutex_unlock(stat_lock);

    return reaction;
}

void enq_dmsrq_vol_list (volume_device_t *vol, dms_rq_t *dmsrq)
{
    struct mutex *rqslock;
    uint32_t tmp;

    rqslock = &vol->vol_rqs_lock;

    mutex_lock(rqslock);

    if (dms_list_add_tail(&dmsrq->list, &vol->vol_rqs_list)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN list_add_tail chk fail %s %d\n",
                __func__, __LINE__);
        DMS_WARN_ON(true);
        msleep(1000);
    } else {
        tmp = atomic_add_return(1, &vol->cnt_rqs);

        if (tmp == 1) {
            vol->sect_start = blk_rq_pos(dmsrq->rq);
        }

        if (rq_data_dir(dmsrq->rq) == KERNEL_IO_READ) {
            atomic_add((uint32_t)blk_rq_sectors(dmsrq->rq), &vol->r_sects);
        } else {
            atomic_add((uint32_t)blk_rq_sectors(dmsrq->rq), &vol->w_sects);
        }
    }
    mutex_unlock(rqslock);

    if (update_vol_active_stat(vol, Vol_EnqRqs) == Vol_DoEnActiveQ) {
        enq_vol_active_list(vol);
    }
}

void enq_dmsrq_vol_list_batch (volume_device_t *vol, dms_rq_t **dmsrqs,
        uint32_t len, uint64_t sect_s)
{
    struct mutex *rqslock;
    atomic_t *rw_sects;
    int32_t i;
    uint64_t first_rq_start;

    rqslock = &vol->vol_rqs_lock;

    if (len <= 0) {
        return;
    }

    mutex_lock(rqslock);

    if (rq_data_dir(dmsrqs[len - 1]->rq) == KERNEL_IO_READ) {
        rw_sects = &vol->r_sects;
    } else {
        rw_sects = &vol->w_sects;
    }

    vol->sect_start = sect_s;
    first_rq_start = blk_rq_pos(dmsrqs[0]->rq);
    for (i = len - 1; i >= 0; i--) {
        if (dms_list_add(&dmsrqs[i]->list, &vol->vol_rqs_list)) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN list_add chk fail %s %d\n",
                    __func__, __LINE__);
            DMS_WARN_ON(true);
            msleep(1000);
        } else {
            atomic_inc(&vol->cnt_rqs);
            atomic_add((uint32_t)blk_rq_sectors(dmsrqs[i]->rq), rw_sects);
            dmsrqs[i] = NULL;
        }
    }

    atomic_sub((uint32_t)(sect_s - first_rq_start), rw_sects);
    mutex_unlock(rqslock);
}

void update_vol_rq_list_start (volume_device_t *vol)
{
    struct mutex *rqslock;
    struct list_head *vol_rqlist;
    dms_rq_t *cur, *next;

    rqslock = &vol->vol_rqs_lock;
    vol_rqlist = &vol->vol_rqs_list;

    mutex_lock(rqslock);

    do {
        if (list_empty(vol_rqlist)) {
            break;
        }

        list_for_each_entry_safe (cur, next, &vol->vol_rqs_list, list) {
            vol->sect_start = blk_rq_pos(cur->rq);
            break;
        }
    } while (0);

    mutex_unlock(rqslock);
}

/*
 * return value > 0  : get some rqs for handling
 *              = 0  : no rqs in list
 *              = -1 : has some rqs but should waiting more rqs for merge
 * if first rq is read, we deq it and try to get a serial of read rq until
 *    encounter a write and reach max number of aggreg.
 *
 *    first rq is write and not enough rqs for merge, we give up and wait
 *    for more
 *
 *    first rq is write and has enough rqs for merge, we try to get more
 *
 *    cons: if user issue patterns : w w r r w w
 *                             or  : w r r r r r (with enough read following w)
 *                             lack aggregation and read has to wait...
 */
int32_t deq_dms_rq_list (volume_device_t *vol, dms_rq_t **arr_dmsrqs,
        uint32_t arr_index_s, uint64_t *sect_start)
{
    dms_rq_t *cur, *next;
    struct mutex *rqslock;
    struct list_head *vol_rqlist;
    uint32_t cnt, skip, len_sects;

    cnt = 0;
    rqslock = &vol->vol_rqs_lock;
    vol_rqlist = &vol->vol_rqs_list;

    mutex_lock(rqslock);

    if (list_empty(vol_rqlist)) {
        goto EXIT_DEQ;
    }

    *sect_start = vol->sect_start;

    list_for_each_entry_safe (cur, next, &vol->vol_rqs_list, list) {
        if (dms_list_del(&cur->list)) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN list_del chk fail %s %d\n",
                    __func__, __LINE__);
            DMS_WARN_ON(true);
            msleep(1000);
            continue;
        }

        atomic_dec(&vol->cnt_rqs);
        arr_dmsrqs[arr_index_s++] = cur;

        if (cnt == 0) {
            skip = *sect_start - blk_rq_pos(cur->rq);
            len_sects = blk_rq_sectors(cur->rq) - skip;
        } else {
            len_sects = blk_rq_sectors(cur->rq);
        }

        if (rq_data_dir(cur->rq) == KERNEL_IO_READ) {
            atomic_sub(len_sects, &vol->r_sects);
        } else {
            atomic_sub(len_sects, &vol->w_sects);
        }

        cnt++;
        if (cnt >= MAX_NUM_RQS_MERGE) {
            break;
        }
    }

EXIT_DEQ:
    mutex_unlock(rqslock);

    return cnt;
}

int32_t init_vol_manager (int32_t blk_dev_num, int32_t max_att_vol)
{
    int32_t i, ret;

    ret = 0;
    atomic_set(&cnt_attach_ongoing, 0);
    set_max_attach_vol(max_att_vol);
    atomic_set(&num_of_vol_attached, 0);

    if ((ret = init_minor_num_generator(max_att_vol))) {
       return ret;
    }

    init_default_volume_gd();

    set_vol_major_num(blk_dev_num);

    for (i = 0; i < VOL_HASH_NUM; i++) {
        INIT_LIST_HEAD(&vol_table[i]);
        init_rwsem(&vol_table_lock[i]);
    }

    return ret;
}

void release_vol_manager (void)
{
    (void)detach_all_volumes();
    free_minor_num_generator();
}

uint32_t show_volume_info (int8_t *buffer, volume_device_t *current_drv)
{
    volume_perf_data_t *perf_data;
    perf_main_data_t *p_main;
    perf_minor_data_t *p_minor;
    int32_t num_of_read;
    int8_t *local_buf;
    uint64_t cnt, main_latency;

    local_buf = buffer;

    num_of_read = sprintf(local_buf, "MetaDataCache=%d\n",
            (int32_t)is_vol_mcache_enable(current_drv));
    num_of_read += sprintf(local_buf + num_of_read, "Replica=%d\n",
            (int32_t)get_volume_replica(current_drv));
    num_of_read += sprintf(local_buf + num_of_read, "ShareVolume=%d\n",
            (int32_t)is_vol_share(current_drv));
    num_of_read += sprintf(local_buf + num_of_read, "volume stop=%d "
            "(0: start 1: stop)\n", current_drv->is_vol_stop);
    num_of_read += sprintf(local_buf + num_of_read, "RW_PERMIT=%d "
            "(0: RW_NOPER 1: R_ONLY 2: W_ONLY 3: RW)\n",
            get_vol_rw_perm(current_drv));
    num_of_read += sprintf(local_buf + num_of_read, "SEND_RW_DATA=%d\n",
            is_vol_send_rw_payload(current_drv));
    num_of_read += sprintf(local_buf + num_of_read, "DO_RW_DATA=%d\n",
            is_vol_do_rw_IO(current_drv));

    perf_data = &current_drv->perf_data;

    if (!dms_client_config->show_performance_data) {
        return num_of_read;
    }

    mutex_lock(&perf_data->perf_lock);
    //Show Performance Data:
    num_of_read += sprintf(local_buf + num_of_read,
            "\n**************** Overall IO Request ****************\n");

    p_main = &perf_data->perf_io;
    cnt = atomic64_read(&p_main->total_cnt);

    main_latency = do_divide(p_main->total_time, cnt);
    num_of_read += sprintf(local_buf + num_of_read,
            "Avg overall latency : %llu (total time: %llu cnt %llu "
            "total sectors transfer %llu acturall need %llu)\n",
            main_latency, p_main->total_time, cnt,
            p_main->total_sectors_done, p_main->total_sectors_req);

    main_latency = do_divide(p_main->total_time_user, cnt);
    num_of_read += sprintf(local_buf + num_of_read,
            "Avg overall latency : %llu (total time: %llu cnt %llu "
            "Commit to user)\n",
            main_latency, p_main->total_time_user, cnt);

    cnt = atomic64_read(&perf_data->total_in_ovlp_cnt);

    main_latency = do_divide(perf_data->total_in_ovlp_time, cnt);
    num_of_read += sprintf(local_buf + num_of_read,
            "Avg wait time stay in overlap q : %llu "
            "(total time: %llu cnt %llu)\n",
            main_latency, perf_data->total_in_ovlp_time, cnt);

    num_of_read += sprintf(local_buf + num_of_read,
            "\n**************** Read IO Request ****************\n");
    p_main = &perf_data->perf_read;
    cnt = atomic64_read(&p_main->total_cnt);

    main_latency = do_divide(p_main->total_time, cnt);
    num_of_read += sprintf(local_buf + num_of_read,
            "Avg all read latency : %llu (total time: %llu cnt %llu "
            "total sectors read %llu acturall need %llu)\n",
            main_latency, p_main->total_time,
            cnt, p_main->total_sectors_done,
            p_main->total_sectors_req);

    main_latency = do_divide(p_main->total_time_user, cnt);
    num_of_read += sprintf(local_buf + num_of_read,
            "Avg overall latency : %llu (total time: %llu "
            "cnt %llu Commit to user)\n",
            main_latency,
            p_main->total_time_user, cnt);

    p_minor = &perf_data->perf_read_cm;
    num_of_read += sprintf(local_buf + num_of_read,
            "\n**************** Small Read IO Request "
            "(An IO Request might split into several Small IO) "
            "****************\n");
    cnt = atomic64_read(&p_minor->total_cnt);

    main_latency = do_divide(p_minor->total_time, cnt);
    num_of_read += sprintf(local_buf + num_of_read,
            "Avg cache miss read latency : %llu (total time: %llu "
            "cnt %llu  total wait metadata %llu "
            "total wait payload %llu)\n",
            main_latency,
            p_minor->total_time, cnt,
            p_minor->wait_mdata_t, p_minor->wait_pdata_t);


    p_minor = &perf_data->perf_read_ch;
    cnt = atomic64_read(&p_minor->total_cnt);
    main_latency = do_divide(p_minor->total_time, cnt);
    num_of_read += sprintf(local_buf + num_of_read,
            "Avg cache hit read latency : %llu (total time: %llu "
            "cnt %llu  total wait payload %llu)\n",
            main_latency, p_minor->total_time,
            cnt, p_minor->wait_pdata_t);

    p_main = &perf_data->perf_write;
    num_of_read += sprintf(local_buf + num_of_read,
            "\n**************** Write IO Request ****************\n");
    cnt = atomic64_read(&p_main->total_cnt);

    main_latency = do_divide(p_main->total_time, cnt);
    num_of_read += sprintf(local_buf + num_of_read,
            "Avg all write latency : %llu (total time: %llu "
            "cnt %llu total sectors write %llu acturall need %llu)\n",
            main_latency, p_main->total_time, cnt,
            p_main->total_sectors_done,
            p_main->total_sectors_req);

    main_latency = do_divide(p_main->total_time_user, cnt);
    num_of_read += sprintf(local_buf + num_of_read,
            "Avg overall latency : %llu (total time: %llu "
            "cnt %llu Commit to user)\n",
            main_latency,
            p_main->total_time_user, cnt);

    p_minor = &perf_data->perf_write_fw4a;
    num_of_read += sprintf(local_buf + num_of_read,
            "\n**************** Small Write IO Request "
            "(An IO Request might split into several Small IO) "
            "****************\n");
    cnt = atomic64_read(&p_minor->total_cnt);

    main_latency = do_divide(p_minor->total_time, cnt);
    num_of_read += sprintf(local_buf + num_of_read,
            "Avg first write 4K align latency : %llu "
            "(total time: %llu cnt %llu  total wait metadata %llu "
            "total wait payload %llu)\n",
            main_latency, p_minor->total_time,
            cnt, p_minor->wait_mdata_t, p_minor->wait_pdata_t);

    p_minor = &perf_data->perf_write_fw4nano;
    cnt = atomic64_read(&p_minor->total_cnt);
    main_latency = do_divide(p_minor->total_time, cnt);
    num_of_read += sprintf(local_buf + num_of_read,
            "Avg first write 4K not-align no old loc latency : %llu "
            "(total time: %llu cnt %llu  total wait metadata %llu  "
            "total wait payload %llu)\n",
            main_latency, p_minor->total_time, cnt,
            p_minor->wait_mdata_t, p_minor->wait_pdata_t);

    p_minor = &perf_data->perf_write_fw4nao;
    cnt = atomic64_read(&p_minor->total_cnt);
    main_latency = do_divide(p_minor->total_time, cnt);
    num_of_read += sprintf(local_buf + num_of_read,
            "Avg first write 4K not-align has old loc latency : %llu "
            "(total time: %llu cnt %llu  total wait metadata %llu  "
            "total wait payload %llu)\n",
            main_latency,
            p_minor->total_time, cnt,
            p_minor->wait_mdata_t, p_minor->wait_pdata_t);

    num_of_read += sprintf(local_buf + num_of_read, "\n");

    p_minor = &perf_data->perf_write_ow4a_cm;
    cnt = atomic64_read(&p_minor->total_cnt);
    main_latency = do_divide(p_minor->total_time, cnt);
    num_of_read += sprintf(local_buf + num_of_read,
            "Avg over write 4K align cache miss latency : %llu "
            "(total time: %llu cnt %llu  total wait metadata %llu  "
            "total wait payload %llu)\n",
            main_latency, p_minor->total_time, cnt,
            p_minor->wait_mdata_t, p_minor->wait_pdata_t);

    p_minor = &perf_data->perf_write_ow4a_ch;
    cnt = atomic64_read(&p_minor->total_cnt);
    main_latency = do_divide(p_minor->total_time, cnt);
    num_of_read += sprintf(local_buf + num_of_read,
            "Avg over write 4K align cache hit latency : %llu "
            "(total time: %llu cnt %llu  "
            "total wait payload %llu)\n",
            main_latency, p_minor->total_time, cnt,
            p_minor->wait_pdata_t);
    p_minor = &perf_data->perf_write_ow4na_cm;
    cnt = atomic64_read(&p_minor->total_cnt);
    main_latency = do_divide(p_minor->total_time, cnt);
    num_of_read += sprintf(local_buf + num_of_read,
            "Avg over write 4K not-align cache miss latency : %llu "
            "(total time: %llu cnt %llu  total wait metadata %llu  "
            "total wait payload %llu)\n",
            main_latency,
            p_minor->total_time, cnt,
            p_minor->wait_mdata_t,p_minor->wait_pdata_t);

    p_minor = &perf_data->perf_write_ow4na_ch;
    cnt = atomic64_read(&p_minor->total_cnt);
    main_latency = do_divide(p_minor->total_time, cnt);
    num_of_read += sprintf(local_buf + num_of_read,
            "Avg over write 4K not-align cache hit latency : %llu "
            "(total time: %llu cnt %llu  "
            "total wait payload %llu)\n",
            main_latency, p_minor->total_time, cnt,
            p_minor->wait_pdata_t);
    mutex_unlock(&perf_data->perf_lock);

    return num_of_read;
}

