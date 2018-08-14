/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * space_commit_worker.c
 *
 */
#include <linux/version.h>
#include <linux/kthread.h>
#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/dms_kernel_version.h"
#include "../common/thread_manager.h"
#include "../config/dmsc_config.h"
#include "../common/discoC_mem_manager.h"
#include "metadata_manager.h"
#include "../discoNN_client/discoC_NNC_Manager_api.h"

#ifdef DISCO_PREALLOC_SUPPORT
#include "space_manager_export_api.h"
#include "space_commit_worker.h"
#include "space_manager.h"

void set_fsTask_fea (fs_task_t *tsk, uint32_t fea_bit)
{
    set_bit(fea_bit, (volatile void *)&(tsk->fstsk_comm.ReqFeature));
}

void clear_fsTask_fea (fs_task_t *tsk, uint32_t fea_bit)
{
    clear_bit(fea_bit, (volatile void *)&(tsk->fstsk_comm.ReqFeature));
}

bool check_fsTask_fea (fs_task_t *tsk, uint32_t fea_bit)
{
    return test_bit(fea_bit,
            (const volatile void *)&(tsk->fstsk_comm.ReqFeature));
}

bool test_and_set_fsTask_fea (fs_task_t *tsk, uint32_t fea_bit)
{
    return test_and_set_bit(fea_bit,
            (volatile void *)&(tsk->fstsk_comm.ReqFeature));
}

bool test_and_clear_fsTask_fea (fs_task_t *tsk, uint32_t fea_bit)
{
    return test_and_clear_bit(fea_bit,
            (volatile void *)&(tsk->fstsk_comm.ReqFeature));
}

/*
 * TODO: don't linear search
 */
fs_task_t *find_fstask_pendq (fs_ciNN_worker_t *mywker, uint32_t taskID)
{
    fs_task_t *cur, *next, *tsk;

    tsk = NULL;
    mutex_lock(&mywker->fs_task_qlock);

    list_for_each_entry_safe (cur, next, &mywker->fs_task_workq, entry_taskPQ) {
        if (cur->fstsk_comm.ReqID == taskID) {
            tsk = cur;
            break;
        }
    }

    mutex_unlock(&mywker->fs_task_qlock);

    return tsk;
}

void rm_fstask_pendq (fs_ciNN_worker_t *mywker, fs_task_t *mytsk)
{
    mutex_lock(&mywker->fs_task_qlock);

    if (IS_ERR_OR_NULL(mytsk) == false) {
        if (test_and_clear_fsTask_fea(mytsk, TaskFEA_IN_TaskPQ)) {
            list_del(&mytsk->entry_taskPQ);
            atomic_dec(&mywker->num_flushReq_pendq);
        }
    }

    mutex_unlock(&mywker->fs_task_qlock);
}

void free_fs_task (int32_t mpoolID, fs_task_t *mytsk)
{
    discoC_free_pool(mpoolID, mytsk);
}

static int32_t _NNC_MDReq_preallocSpace_endfn (metadata_t *MData, void *private_data,
        ReqCommon_t *calleeReq, int32_t result)
{
    fs_task_t *mytsk;
    int32_t ret;

    ret = 0;

    //mytsk = _find_rm_fstask_pendq(calleeReq->CallerReqID);
    mytsk = (fs_task_t *)private_data;

    rm_fstask_pendq(mytsk->tskWorker, mytsk);

    sm_preallocReq_endfn(MData->usr_ioaddr.volumeID);

    if (IS_ERR_OR_NULL(mytsk->fstask_end_task_fn) == false) {
        mytsk->fstask_end_task_fn(mytsk->task_data, 0);
    }

    mytsk->tskWorker->done_total_reqs++;
    mytsk->tskWorker->done_prealloc_reqs++;

    free_metadata(&mytsk->MetaData);
    free_fs_task(get_tskMemPoolID(), mytsk);

    return ret;
}

static int32_t _NNC_MDReq_flushMD_endfn (metadata_t *MData, void *private_data,
        ReqCommon_t *calleeReq, int32_t result)
{
    fs_task_t *mytsk;
    int32_t ret;

    ret = 0;

    //mytsk = _find_rm_fstask_pendq(calleeReq->CallerReqID);
    mytsk = (fs_task_t *)private_data;

    rm_fstask_pendq(mytsk->tskWorker, mytsk);

    //TODO: put metadata to cache

    //TODO: change state of allocation unit
    sm_remove_allocspace(MData->usr_ioaddr.volumeID, MData->usr_ioaddr.lbid_start,
            MData->usr_ioaddr.lbid_len);

    if (IS_ERR_OR_NULL(mytsk->fstask_end_task_fn) == false) {
        mytsk->fstask_end_task_fn(mytsk->task_data, 0);
    }

    mytsk->tskWorker->done_total_reqs++;
    mytsk->tskWorker->done_flush_reqs++;

    free_metadata(&mytsk->MetaData);
    free_fs_task(get_tskMemPoolID(), mytsk);

    //sm_show_storage_pool();

    return ret;
}

static int32_t _NNC_MDReq_returnSpace_endfn (metadata_t *MData, void *private_data,
        ReqCommon_t *calleeReq, int32_t result)
{
    fs_task_t *mytsk;
    int32_t ret;

    ret = 0;

    //mytsk = _find_rm_fstask_pendq(calleeReq->CallerReqID);
    mytsk = (fs_task_t *)private_data;

    rm_fstask_pendq(mytsk->tskWorker, mytsk);

    if (IS_ERR_OR_NULL(mytsk->fstask_end_task_fn) == false) {
        mytsk->fstask_end_task_fn(mytsk->task_data, 0);
    }

    mytsk->tskWorker->done_total_reqs++;
    mytsk->tskWorker->done_return_reqs++;

    free_metadata(&mytsk->MetaData);
    free_fs_task(get_tskMemPoolID(), mytsk);

    return ret;
}

fs_task_t *alloc_fs_task (int32_t mpoolID)
{
    return (fs_task_t *)discoC_malloc_pool(mpoolID);
}

void init_fs_task (fs_task_t *fstsk, fs_task_type_t tskType, uint32_t tskID,
        sm_task_endfn *tsk_endfn, void *tsk_data)
{
    //if (tskType == fstask_flush_LHO) {
    //    fstsk->fstsk_comm.ReqOPCode = REQ_NN_REPORT_SPACE_MDATA;
    //}
    fstsk->fstsk_comm.ReqOPCode = tskType;
    fstsk->fstsk_comm.ReqID = tskID;
    INIT_LIST_HEAD(&fstsk->entry_taskWQ);
    INIT_LIST_HEAD(&fstsk->entry_taskPQ);
    fstsk->task_data = tsk_data;
    fstsk->fstask_end_task_fn = tsk_endfn;
    fstsk->fstsk_comm.ReqFeature = 0;
    atomic_set(&fstsk->fstsk_comm.ReqRefCnt, 0);
    memset(&(fstsk->MetaData), 0, sizeof(metadata_t));
}

void submit_fs_task (fs_ciNN_worker_t *mywker, fs_task_t *fstsk)
{
    fstsk->tskWorker = mywker;

    mutex_lock(&mywker->fs_task_qlock);

    if (test_and_set_fsTask_fea(fstsk, TaskFEA_IN_TaskWQ) == false) {
        list_add_tail(&fstsk->entry_taskWQ, &mywker->fs_task_workq);
        atomic_inc(&mywker->num_flushReq_workq);
    }

    if (test_and_set_fsTask_fea(fstsk, TaskFEA_IN_TaskPQ) == false) {
        list_add_tail(&fstsk->entry_taskPQ, &mywker->fs_task_pendq);
        atomic_inc(&mywker->num_flushReq_pendq);
    }

    mutex_unlock(&mywker->fs_task_qlock);

    wake_up_interruptible_all(&mywker->flush_waitq);
}

static fs_task_t *_peek_fs_task (fs_ciNN_worker_t *mywker)
{
    fs_task_t *mytsk;

    mutex_lock(&mywker->fs_task_qlock);

    mytsk = list_first_entry_or_null(&mywker->fs_task_workq, fs_task_t,
            entry_taskWQ);
    if (mytsk != NULL) {
        if (test_and_clear_fsTask_fea(mytsk, TaskFEA_IN_TaskWQ)) {
            list_del(&mytsk->entry_taskWQ);
            atomic_dec(&mywker->num_flushReq_workq);
        }
    }

    mutex_unlock(&mywker->fs_task_qlock);

    return mytsk;
}

static int _fs_flush_worker_fn (void *thread_data)
{
    dmsc_thread_t *t_data;
    fs_ciNN_worker_t *mywker;
    fs_task_t *mytsk;
    uint64_t flush_dur_t, curr_t, next_flush_intvl_t, def_fintvl;
    bool do_flush_all;

    t_data = (dmsc_thread_t *)thread_data;
    mywker = (fs_ciNN_worker_t *)t_data->usr_data;

    mywker->last_flush_t = jiffies_64;
    def_fintvl = (dms_client_config->prealloc_flush_intvl*HZ);
    next_flush_intvl_t = def_fintvl;

    while (t_data->running) {
        wait_event_interruptible_timeout(*(t_data->ctl_wq),
                atomic_read(&mywker->num_flushReq_workq) > 0 || !t_data->running,
                next_flush_intvl_t);
        if (!t_data->running) {
            break;
        }

        curr_t = jiffies_64;
        if (time_after64(curr_t, mywker->last_flush_t)) {
            flush_dur_t = curr_t - mywker->last_flush_t;
        } else {
            def_fintvl = (dms_client_config->prealloc_flush_intvl*HZ);
            flush_dur_t = def_fintvl;
        }

        do_flush_all = false;
        if (flush_dur_t >= def_fintvl) {
            do_flush_all = true;
        } else {
            if (time_after64(def_fintvl, flush_dur_t)) {
                next_flush_intvl_t = def_fintvl - flush_dur_t;
            } else {
                do_flush_all = true;
            }
        }

        if (do_flush_all) {
            sm_report_allvolume_spaceMData();
            mywker->last_flush_t = curr_t;
            def_fintvl = (dms_client_config->prealloc_flush_intvl*HZ);
            next_flush_intvl_t = def_fintvl;
        }

        mytsk = _peek_fs_task(mywker);
        if (IS_ERR_OR_NULL(mytsk) == false) {
            switch (mytsk->fstsk_comm.ReqOPCode) {
            case fstask_prealloc_fs:
                discoC_prealloc_freeSpace(&mytsk->MetaData.usr_ioaddr,
                        &mytsk->fstsk_comm, &mytsk->MetaData, mytsk,
                        _NNC_MDReq_preallocSpace_endfn);
                break;
            case fstask_flush_LHO:
                discoC_flush_spaceMetadata(&mytsk->MetaData.usr_ioaddr,
                        &mytsk->fstsk_comm, &mytsk->MetaData, mytsk,
                        _NNC_MDReq_flushMD_endfn);
                break;
            case fstask_report_unuse:
                discoC_report_freeSpace(&mytsk->MetaData.usr_ioaddr,
                        &mytsk->fstsk_comm, &mytsk->MetaData, mytsk,
                        _NNC_MDReq_returnSpace_endfn);
                break;
            }
        }
    }

    return 0;
}

int32_t inif_fs_worker (fs_ciNN_worker_t *mywker, void *wker_data)
{
    int32_t ret;

    ret = 0;

    dms_printk(LOG_LVL_INFO, "DMSC INFO create fs worker\n");

    init_waitqueue_head(&mywker->flush_waitq);
    atomic_set(&mywker->num_flushReq_pendq, 0);
    atomic_set(&mywker->num_flushReq_workq, 0);
    INIT_LIST_HEAD(&mywker->fs_task_workq);
    INIT_LIST_HEAD(&mywker->fs_task_pendq);
    mutex_init(&mywker->fs_task_qlock);
    mywker->worker_data = wker_data;
    //mywker->flush_intvl = (dms_client_config->prealloc_flush_intvl*HZ);

    mywker->done_total_reqs = 0;
    mywker->done_prealloc_reqs = 0;
    mywker->done_flush_reqs = 0;
    mywker->done_return_reqs = 0;

    mywker->flush_wker = create_worker_thread("dms_fs_wker", &mywker->flush_waitq,
            mywker, _fs_flush_worker_fn);
    if (IS_ERR_OR_NULL(mywker->flush_wker)) {
        dms_printk(LOG_LVL_INFO, "DMSC WARN fail create fs worker\n");
        ret = -ESRCH;
    }

    dms_printk(LOG_LVL_INFO, "DMSC INFO create fs worker DONE ret %d\n", ret);

    return ret;
}

int32_t rel_fs_worker (fs_ciNN_worker_t *mywker)
{
    int32_t ret;

    ret = 0;

    if (stop_worker_thread(mywker->flush_wker)) {
        dms_printk(LOG_LVL_WARN, "fail to stop DN Request worker\n");
        DMS_WARN_ON(true);
    }

    return ret;
}
#endif
