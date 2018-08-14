/*
 * DISCO Client Driver main function
 *
 * Copyright (C) 2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * drv_blkdev.c
 *
 */
#include <linux/version.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/blkdev.h>
#include "common/discoC_sys_def.h"
#include "common/common_util.h"
#include "common/common.h"
#include "drv_fsm.h"
#include "common/dms_kernel_version.h"
#include "common/discoC_mem_manager.h"
#include "common/thread_manager.h"
#include "metadata_manager/metadata_manager.h"
#include "common/dms_client_mm.h"
#include "discoDN_client/discoC_DNC_Manager_export.h"
#include "payload_manager/payload_manager_export.h"
#include "io_manager/discoC_IO_Manager.h"
#include "drv_main.h"
#include "config/dmsc_config.h"
#include "vdisk_manager/volume_manager.h"
#include "io_manager/discoC_IO_Manager_export.h"

static struct request *dms_fetch_request (struct request_queue *q)
{
    struct request *req;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,39)
    if (blk_queue_plugged(q)) {
        return NULL;
    }
#endif

    req = blk_peek_request(q);
    if (IS_ERR_OR_NULL((void *)req)) {
        return NULL;
    }

    if (!blk_fs_request(req)) {
        // end_request(rq, 0);
        // dms_end_request_all(rq, -EIO);  <== need it anymore?
        return NULL;
    }

    return req;
}

static dms_rq_t *init_dms_rq (struct request *rq)
{
    dms_rq_t *dmsrq;

    dmsrq = IOMgr_alloc_dmsrq();

    if (unlikely(IS_ERR_OR_NULL(dmsrq))) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN fail alloc mem for dms rq\n");
        return NULL;
    }

    dmsrq->result = true;
    dmsrq->rq = rq;
    atomic_set(&dmsrq->num_ref_ior, 0);
    INIT_LIST_HEAD(&dmsrq->list);
    return dmsrq;
}

//TODO: Should we block forever??
static bool dms_io_request_w_bar (volume_device_t *drv)
{
#define MAX_RETRY_CNT 5
    uint32_t cnt;
    bool ret = true;

    //dms_printk(LOG_LVL_INFO, "DMSC INFO handle flush rq volume %d\n",
    //        drv->vol_id);
    cnt = 0;
    while (!wait_vol_io_done(drv, true)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN handle flush cmds try cnt %d, still no finish\n", cnt++);
        if (cnt > MAX_RETRY_CNT) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN hit max try cnt %d\n", cnt);
            ret = false;
            break;
        }
        // blk_requeue_request(q, rq);
    }
    //dms_printk(LOG_LVL_INFO, "DMSC INFO handle flush rq volume %d done\n",
    //            drv->vol_id);

    return ret;
}

/*
 * read/write request routine handler.
 */
void dms_io_request_fn (struct request_queue *q)
{
    struct request *rq;
    dms_rq_t *dmsrq;
    volume_device_t *vol;
    bool flush_ret;

    if (IS_ERR_OR_NULL((void *)q)) {
        return;
    }

    while ((rq = dms_fetch_request(q))) {
        blk_start_request(rq);

        vol = (volume_device_t *)rq->rq_disk->private_data;

        if (unlikely(IS_ERR_OR_NULL(vol))) {
            dms_end_request(rq, false, blk_rq_bytes(rq));
            continue;
        }

        atomic_inc(&vol->lgr_rqs);
        spin_unlock_irq(q->queue_lock);

        if (dms_need_flush(rq)) {
            atomic_dec(&vol->lgr_rqs);
            flush_ret = dms_io_request_w_bar(vol);
            spin_lock_irq(q->queue_lock);
            dms_end_request_all(rq, flush_ret);
            continue;
        }

        if (vol_no_perm_to_io(vol, rq_data_dir(rq))) {
            commit_user_request(rq, false);
            continue;
        }

        /*
         *dms_printk(LOG_LVL_DEBUG,
         *       "DMSC DEBUG get rq (%llu %d) (process: %s)\n",
         *       blk_rq_pos(rq), blk_rq_sectors(rq), current->comm);
         */

        dmsrq = init_dms_rq(rq);

        if (unlikely(IS_ERR_OR_NULL(dmsrq))) {
            commit_user_request(rq, false);
            spin_lock_irq(q->queue_lock);
            continue;
        }

        /*
         * NOTE: Should we block threads for avoiding get too many rqs
         *       when dms performance down?
         *       or we should use blk_stop_queue and blk_start_queue to control
         */
        if ((dms_client_config->req_merge_enable == false) ||
                (rq_data_dir(rq) == KERNEL_IO_READ &&
                atomic_read(&vol->cnt_rqs) == 0)) {
            /*
             * NOTE: It might cause some issue:
             *     Requests might out of order when direct IO.
             *     For example: write io comes for LB 10, 11, be buffer for
             *     waiting for requests. read LB 10 11 come after and be
             *     handled directly.
             *     Page base IO won!|t has such issue, because direct page
             *         won!|t be kicked out unless we commit to user
             *
             *     TODO: cnt_rqs might be decreased after dms_io_request finish
             *           all write IOs.
             *
             */
            atomic_set(&dmsrq->num_ref_ior, 1);
            IOMgr_submit_IOReq(&dmsrq, 0, 1, blk_rq_pos(rq), blk_rq_sectors(rq));
        } else {
            enq_dmsrq_vol_list(vol, dmsrq);
        }

        spin_lock_irq(q->queue_lock);
    }
}

int32_t init_blkdev (discoC_drv_t *mydrv)
{
    int32_t ret;

    ret = 0;
    ret = register_blkdev(0, mydrv->blkdev_name_prefix);
    if (ret < 0) {
        //dms_printk(LOG_LVL_ERR,
        //        "DMSC ERROR, unable to get major number, ret %d\n", ret);
        //DMS_WARN_ON(true);
        //TODO : should release all data
        return -ENOMEM;
    }

    mydrv->blkdev_major_num = ret;
    dms_printk(LOG_LVL_INFO, "DMSC INFO, blkdev_major_num = %d\n", ret);

    return ret;
}

void rel_blkdev (discoC_drv_t *mydrv)
{
    unregister_blkdev(mydrv->blkdev_major_num, mydrv->blkdev_name_prefix);
}

