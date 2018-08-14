/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * io_worker_manager.c
 */
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/blkdev.h>
#include <linux/types.h>
#include <linux/delay.h>
#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/dms_kernel_version.h"
#include "../common/discoC_mem_manager.h"
#include "../common/thread_manager.h"
#include "../common/dms_client_mm.h"
#include "../config/dmsc_config.h"
#include "../metadata_manager/metadata_manager.h"
#include "discoC_IO_Manager.h"
#include "discoC_IO_Manager_export.h"
#include "../vdisk_manager/volume_manager.h"
#include "io_worker_manager.h"
#include "io_worker_manager_private.h"

void enq_vol_active_list (volume_device_t *vol)
{
    bool do_wakeup;

    do_wakeup = false;

    mutex_lock(&iowker_Mgr.active_vol_lock);
    if (list_empty(&vol->active_list)) {
        do_wakeup = true;
        list_add_tail(&vol->active_list, &iowker_Mgr.active_vol_list);
            atomic_inc(&iowker_Mgr.active_vol_cnt);
    }
    mutex_unlock(&iowker_Mgr.active_vol_lock);

    if (waitqueue_active(&iowker_Mgr.dms_io_wq) && do_wakeup) {
        wake_up_interruptible_all(&iowker_Mgr.dms_io_wq);
    }
}

static volume_device_t *deq_vol_active_list (iowker_manager_t *iowker_mgr)
{
    volume_device_t *cur, *next, *drv;

    drv = NULL;
    mutex_lock(&iowker_mgr->active_vol_lock);

    list_for_each_entry_safe (cur, next, &iowker_mgr->active_vol_list, active_list) {
        drv = cur;
        list_del(&cur->active_list);

        atomic_dec(&iowker_mgr->active_vol_cnt);
        INIT_LIST_HEAD(&cur->active_list);
        break;
    }
    mutex_unlock(&iowker_mgr->active_vol_lock);

    return drv;
}

static bool rq_mergable (struct request *prev_rq, struct request *next_rq)
{
    uint64_t prev_end;

    if (rq_data_dir(prev_rq) != rq_data_dir(next_rq)) {
        return false;
    }

    prev_end = blk_rq_pos(prev_rq) + blk_rq_sectors(prev_rq);

    //NOTE: avoid decrease 1, compare next start
    return (blk_rq_pos(next_rq) == (prev_end));
}

//return value: merge len
static uint32_t get_merge_len (dms_rq_t **rqs,
        uint32_t index_s, uint32_t index_e,
        uint32_t sect_s, uint32_t *sect_len,
        uint32_t *new_index_s)
{
    uint32_t i, merge_len, new_sect_len;
    bool last_rq_multi_ref;
    struct request *rq;
    dms_rq_t *dmsrq;

    merge_len = 1;
    *sect_len = 0;
    last_rq_multi_ref = false;

    do {
        if (index_s > index_e) {
            merge_len = 0;
            break;
        }

        dmsrq = rqs[index_s];
        rq = dmsrq->rq;

        if (atomic_read(&dmsrq->num_ref_ior) == 0) {
            atomic_set(&dmsrq->num_ref_ior, 1);
        }

        *sect_len = blk_rq_sectors(rq) - (sect_s - blk_rq_pos(rq));

        //NOTE: For reduce read latency, we don't merge read
        if (*sect_len == MAX_SECTS_PER_RQ ||
                rq_data_dir(rq) == KERNEL_IO_READ) {
        //if (*sect_len >= MAX_SECTS_RQ_MERGE) {
            break;
        }

        for (i = (index_s + 1); i <= index_e; i++) {
            if (!rq_mergable(rqs[i-1]->rq, rqs[i]->rq)) {
                break;
            }

            new_sect_len = *sect_len + blk_rq_sectors(rqs[i]->rq);
            atomic_set(&rqs[i]->num_ref_ior, 1);
            if (new_sect_len < MAX_SECTS_PER_RQ) {
                *sect_len = new_sect_len;
                merge_len++;
                continue;
            }

            if (new_sect_len > MAX_SECTS_PER_RQ &&
                    *sect_len != MAX_SECTS_PER_RQ) {
                atomic_set(&rqs[i]->num_ref_ior, 2);
                last_rq_multi_ref = true;
            }

            *sect_len = MAX_SECTS_PER_RQ;
            merge_len++;

            break;
        }
    } while (0);

    *new_index_s = index_s + merge_len;
    if (last_rq_multi_ref) {
        *new_index_s -= 1;
    }
    return merge_len;
}

static int32_t do_io_batch (dms_rq_t **rqs, uint32_t num_rqs,
        uint64_t *sect_start, bool force_io)
{
    struct request *rq;
    uint32_t index_s, index_e, merge_len, sect_len, new_index_s, merge_end;
    uint64_t sect_s;
    bool do_new_start;
    int32_t remain_index_s;

    rq = rqs[0]->rq;

    index_s = 0;
    index_e = num_rqs - 1;
    //sect_s = blk_rq_pos(rq);
    sect_s = *sect_start;
    remain_index_s = -1;


    do_new_start = true;
    while (index_s <= index_e) {
        merge_len = get_merge_len(rqs, index_s, index_e, sect_s,
                &sect_len, &new_index_s);
        merge_end = index_s + merge_len - 1;
        if (new_index_s != (merge_end + 1)) {
            do_new_start = false;
        }

        if (rq_data_dir(rqs[index_s]->rq) == KERNEL_IO_WRITE &&
                sect_len < MAX_SECTS_PER_RQ &&
                merge_end == index_e) {
            /*
             * NOTE: Cannot merge more since we reach end of array
             *       and the number of sectors is not sat our requirement.
             *
             *       if not force io, try to wait more data
             *       if force io but not merge from 1st elements of array.
             *           try to wait more
             *
             *       the meaning of force_io is not matter the merge results
             *       from rqs[0] to rqs[merge_len - 1]
             *       is sat MAX_SECTS_PER_RQ or not,
             *       we will force io.
             */

            if (index_s == 0 && num_rqs == MAX_NUM_RQS_MERGE) {
                force_io = true;
                goto DO_IO;
            }

            if (!force_io) {
                *sect_start = sect_s;
                remain_index_s = index_s;
                break;
            }

            if (index_s > 0) {
                *sect_start = sect_s;
                remain_index_s = index_s;
                break;
            }
        }

DO_IO:
        IOMgr_submit_IOReq(rqs, index_s, merge_len, sect_s, sect_len);

        index_s = new_index_s;
        if (do_new_start) {
            if (index_s <= index_e) {
                sect_s = blk_rq_pos(rqs[index_s]->rq);
            }
        } else {
            sect_s += sect_len;
            do_new_start = true;
        }
        sect_len = 0;
    }

    return remain_index_s;
}

static int dms_io_worker (void *thread_data)
{
    iowker_manager_t *iowker_mgr;
    dms_rq_t **dmsrqs;
    volume_device_t *active_vol;
    uint32_t num_rqs;
    int32_t retcode, w_sects, r_sects, m_wait_intvl, m_wait_max_cnt;
    uint64_t sect_start;
    bool force_io;

    dmsc_thread_t *t_data = (dmsc_thread_t *)thread_data;
    iowker_mgr = (iowker_manager_t *)t_data->usr_data;

    dmsrqs = discoC_mem_alloc(sizeof(dms_rq_t *)*MAX_NUM_RQS_MERGE,
            GFP_NOWAIT | GFP_ATOMIC);
    memset(dmsrqs, 0, sizeof(dms_rq_t *)*MAX_NUM_RQS_MERGE);

    num_rqs = 0;

    while (t_data->running) {
        wait_event_interruptible(*(t_data->ctl_wq),
                atomic_read(&iowker_mgr->active_vol_cnt) > 0 ||
                !t_data->running);

        active_vol = deq_vol_active_list(iowker_mgr);

        if (IS_ERR_OR_NULL(active_vol)) {
            continue;
        }

        force_io = false;
        sect_start = 0;
        m_wait_intvl = dms_client_config->req_merge_wait_intvl;
        m_wait_max_cnt = dms_client_config->req_merge_wait_cnt;

DEQ_AGAIN:
        num_rqs = deq_dms_rq_list(active_vol, dmsrqs, 0, &sect_start);

        if (num_rqs <= 0) {
            continue;
        }

        if (active_vol->rq_agg_wait_cnt > 0) {
            force_io = true;
        } else {
            force_io = false;
        }

        retcode = do_io_batch(dmsrqs, num_rqs, &sect_start, force_io);
        if (retcode > -1) {
            enq_dmsrq_vol_list_batch(active_vol, &dmsrqs[retcode],
                    num_rqs - retcode, sect_start);
            active_vol->rq_agg_wait_cnt++;
        } else {
            update_vol_rq_list_start(active_vol);
            active_vol->rq_agg_wait_cnt = 0;
        }

        if (atomic_read(&iowker_mgr->active_vol_cnt) > 0) {
            if (update_vol_active_stat(active_vol, Vol_RqsDone) ==
                    Vol_DoEnActiveQ) {
                enq_vol_active_list(active_vol);
            }
            continue;
        }

        if (retcode > -1) {
            /*
             * NOTE: Original design using timeout mechanism, but it hurt performance
             *       for short waiting, using udelay is better
             */

            w_sects = atomic_read(&active_vol->w_sects);
            r_sects = atomic_read(&active_vol->r_sects);

            if (r_sects > 0 || w_sects >= MAX_SECTS_PER_RQ) {
                active_vol->rq_agg_wait_cnt = 0;
                goto DEQ_AGAIN;
            }

            while (active_vol->rq_agg_wait_cnt < m_wait_max_cnt) {

                udelay(m_wait_intvl);

                w_sects = atomic_read(&active_vol->w_sects);
                r_sects = atomic_read(&active_vol->r_sects);
                if (r_sects > 0 || w_sects >= MAX_SECTS_PER_RQ) {
                    break;
                }

                active_vol->rq_agg_wait_cnt++;
            }

            active_vol->rq_agg_wait_cnt = 1;
            goto DEQ_AGAIN;
        } else {
            active_vol->rq_agg_wait_cnt = 0;
        }

        if (atomic_read(&active_vol->cnt_rqs) > 0) {
            goto DEQ_AGAIN;
        } else {
            if (update_vol_active_stat(active_vol, Vol_RqsDone) ==
                    Vol_DoEnActiveQ) {
                enq_vol_active_list(active_vol);
            }
        }
    }

    discoC_mem_free(dmsrqs);

    return 0;
}

void init_ioworker_manager (iowker_manager_t *iowker_mgr, int32_t num_wker)
{
    int32_t i;

    atomic_set(&iowker_mgr->active_vol_cnt, 0);
    INIT_LIST_HEAD(&iowker_mgr->active_vol_list);
    mutex_init(&iowker_mgr->active_vol_lock);
    init_waitqueue_head(&iowker_mgr->dms_io_wq);
    iowker_mgr->num_io_wker = num_wker;

    for (i = 0; i < iowker_mgr->num_io_wker; i++) {
        iowker_mgr->io_workers[i] = create_worker_thread("dms_io_worker",
                &iowker_mgr->dms_io_wq, iowker_mgr, dms_io_worker);
        if (IS_ERR_OR_NULL(iowker_mgr->io_workers[i])) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN kthread fail create io worker\n");
            DMS_WARN_ON(true);
        }
    }
}
