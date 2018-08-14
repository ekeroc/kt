/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * space_manager.c
 *
 */
#include <linux/version.h>
#include <linux/kthread.h>
#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/dms_kernel_version.h"
#include "../common/thread_manager.h"
#include "../lib/dms-radix-tree.h"
#include "../common/discoC_mem_manager.h"
#include "metadata_manager.h"

#ifdef DISCO_PREALLOC_SUPPORT

#include "space_chunk_fsm.h"
#include "space_chunk.h"
#include "space_manager_export_api.h"
#include "space_commit_worker.h"
#include "space_chunk_cache.h"
#include "space_storage_pool.h"
#include "space_manager_private.h"
#include "space_manager.h"

int32_t get_tskMemPoolID (void)
{
    return discoC_fsMgr.fstask_mpool_ID;
}

int32_t get_fsChunkMemPoolID (void)
{
    return discoC_fsMgr.fschunk_mpool_ID;
}

int32_t get_asChunkMemPoolID (void)
{
    return discoC_fsMgr.aschunk_mpool_ID;
}

uint32_t generate_fsReq_ID (void)
{
    return atomic_add_return(1, &discoC_fsMgr.fs_ReqID_generator);
}

static void _sm_reportallVolMData_submit_task (struct list_head *tsk_lhead,
        int32_t numtsk)
{
    fs_task_t *cur, *next;

    list_for_each_entry_safe (cur, next, tsk_lhead, entry_taskWQ) {
        list_del(&cur->entry_taskWQ);

        //cur->fstask_end_task_fn = _report_unuse_task_fn;
        //cur->task_data = &mycomp;

        submit_fs_task(&discoC_fsMgr.fs_ci_wker, cur);
    }
}

int32_t sm_report_allvolume_spaceMData (void)
{
    struct list_head tsk_lhead;
    int32_t ret, numtsk;

    ret = 0;

    INIT_LIST_HEAD(&tsk_lhead);
    numtsk = fspool_report_allpool_allocspace(&discoC_fsMgr.fsPooltbl, &tsk_lhead);

    if (numtsk > 0) {
        _sm_reportallVolMData_submit_task(&tsk_lhead, numtsk);
    }

    return ret;
}

int32_t sm_remove_allocspace (uint32_t volumeID, uint64_t lbid_s, uint32_t lb_len)
{
    return fspool_remove_allocspace(&discoC_fsMgr.fsPooltbl, volumeID,
            lbid_s, lb_len);
}

static int32_t _sm_task_returnSpace_endfn (void *arg, int32_t result)
{
    sm_completion_t *mycomp;
    int32_t ret;

    ret = 0;

    mycomp = (sm_completion_t *)arg;

    if (atomic_sub_return(1, &mycomp->counter) == 0) {
        complete(&mycomp->comp_work);
    }

    return ret;
}

static void _sm_reportVolUnuse_submit_task (struct list_head *tsk_lhead,
        int32_t numtsk)
{
    fs_task_t *cur, *next;
    sm_completion_t mycomp;

    COMPLETION_INITIALIZER_ONSTACK(mycomp.comp_work);
    atomic_set(&mycomp.counter, numtsk);

    list_for_each_entry_safe (cur, next, tsk_lhead, entry_taskWQ) {
        list_del(&cur->entry_taskWQ);

        cur->fstask_end_task_fn = _sm_task_returnSpace_endfn;
        cur->task_data = &mycomp;

        submit_fs_task(&discoC_fsMgr.fs_ci_wker, cur);
    }

    wait_for_completion(&mycomp.comp_work);
}

int32_t sm_report_volume_unusespace (uint32_t volumeID)
{
    struct list_head tsk_lhead;
    //fs_task_t *fstsk;
    int32_t ret, numtsk;

    ret = 0;

    INIT_LIST_HEAD(&tsk_lhead);
    numtsk = fspool_report_pool_unusespace(&discoC_fsMgr.fsPooltbl, volumeID,
            &tsk_lhead);

    if (numtsk > 0) {
        _sm_reportVolUnuse_submit_task(&tsk_lhead, numtsk);
    }

    return ret;
}

static int32_t _sm_task_flushVolMD_endfn (void *arg, int32_t result)
{
    sm_completion_t *mycomp;
    int32_t ret;

    ret = 0;

    mycomp = (sm_completion_t *)arg;

    if (atomic_sub_return(1, &mycomp->counter) == 0) {
        complete(&mycomp->comp_work);
    }

    return ret;
}

static void _sm_reportVolMData_submit_task (struct list_head *tsk_lhead,
        int32_t numtsk)
{
    fs_task_t *cur, *next;
    sm_completion_t mycomp;

    COMPLETION_INITIALIZER_ONSTACK(mycomp.comp_work);
    atomic_set(&mycomp.counter, numtsk);

    list_for_each_entry_safe (cur, next, tsk_lhead, entry_taskWQ) {
        list_del(&cur->entry_taskWQ);

        cur->fstask_end_task_fn = _sm_task_flushVolMD_endfn;
        cur->task_data = &mycomp;

        submit_fs_task(&discoC_fsMgr.fs_ci_wker, cur);
    }

    wait_for_completion(&mycomp.comp_work);
}

int32_t sm_report_volume_spaceMData (uint32_t volumeID)
{
    struct list_head tsk_lhead;
    //fs_task_t *fstsk;
    int32_t ret, numtsk;

    ret = 0;

    INIT_LIST_HEAD(&tsk_lhead);
    numtsk = fspool_report_pool_allocspace(&discoC_fsMgr.fsPooltbl, volumeID,
            &tsk_lhead);

    if (numtsk > 0) {
        _sm_reportVolMData_submit_task(&tsk_lhead, numtsk);
    }

    return ret;
}

/*
 * return number of MD bucket for lbid_s lbid_len
 * TODO: implement me
 */
int32_t sm_query_volume_MDCache (metadata_t *arr_MD_bk,
        uint32_t volid, uint64_t lbid_s, uint32_t lbid_len)
{
    return fspool_query_volume_MDCache(&discoC_fsMgr.fsPooltbl,
            arr_MD_bk, volid, lbid_s, lbid_len);
}

void sm_cleanup_volume (uint32_t volumeID)
{
    fspool_cleanup_pool(&discoC_fsMgr.fsPooltbl, volumeID);
}

int32_t sm_preallocReq_endfn (uint32_t volumeID)
{
    return fspool_update_fsSizeRequesting(&discoC_fsMgr.fsPooltbl, volumeID, false);
}

static void _gen_allocSpace_task_data (fs_task_t *fstsk, uint32_t volID,
        uint32_t minSize)
{
    fstsk->MetaData.num_MD_items = 1;
    fstsk->MetaData.num_invalMD_items = 0;

    fstsk->MetaData.usr_ioaddr.volumeID = volID;
    fstsk->MetaData.usr_ioaddr.lbid_start = 0;
    fstsk->MetaData.usr_ioaddr.lbid_len = minSize;
    fstsk->MetaData.usr_ioaddr.metaSrv_ipaddr = 1234567;
    fstsk->MetaData.usr_ioaddr.metaSrv_port = 1234;
}

int32_t sm_prealloc_volume_freespace (uint32_t volumeID, uint32_t minSize,
        sm_task_endfn *cbfn, void *cb_data)
{
    fs_task_t *fstsk;
    int32_t ret;

    if ((ret =
            fspool_update_fsSizeRequesting(&discoC_fsMgr.fsPooltbl, volumeID, true))) {
        return ret;
    }

    fstsk = alloc_fs_task(discoC_fsMgr.fstask_mpool_ID);
    if (IS_ERR_OR_NULL(fstsk) == false) {
        init_fs_task(fstsk, fstask_prealloc_fs, generate_fsReq_ID(), cbfn, cb_data);
        _gen_allocSpace_task_data(fstsk, volumeID, minSize);
        submit_fs_task(&discoC_fsMgr.fs_ci_wker, fstsk);
    } else {
        ret = -ENOMEM;
    }

    return ret;
}

int32_t sm_add_volume_fschunk (uint32_t volumeID, uint64_t chunkID, uint32_t len,
        uint64_t hbid_s, uint16_t numReplica, struct meta_cache_dn_loc *arr_dnLoc)
{
    int8_t ipv4addr_str[IPV4ADDR_NM_LEN];
    fs_chunk_t *chunk;
    int32_t i;

    chunk = fschunk_alloc_chunk(discoC_fsMgr.fschunk_mpool_ID);
    fschunk_init_chunk(chunk, chunkID, len, hbid_s, numReplica);

    for (i = 0; i < numReplica; i++) {
        ccma_inet_aton(arr_dnLoc[i].ipaddr, ipv4addr_str, IPV4ADDR_NM_LEN);
        fschunk_init_chunk_DNLoc(chunk, i, arr_dnLoc[i].ipaddr, arr_dnLoc[i].port,
                arr_dnLoc[i].rbid, arr_dnLoc[i].offset);
    }

    fspool_add_fschunk(&discoC_fsMgr.fsPooltbl, volumeID, chunk);

    return 0;
}

/*
 * Description: will translate fschunk into report state and return max unuse HBID
 *              called when nn send a invalidate command
 */
sm_invalChunk_resp_t sm_invalidate_volume_fschunk (uint32_t volumeID,
        uint64_t chunkID, uint64_t *maxUnuseHBID)
{
    spool_invalChunk_resp_t inval_ret;
    sm_invalChunk_resp_t ret;

    ret = 0;

    inval_ret = fspool_inval_fschunk(&discoC_fsMgr.fsPooltbl, volumeID, chunkID,
            maxUnuseHBID);

    switch (inval_ret) {
    case SPOOL_INVAL_CHUNK_OK:
        ret = SM_INVAL_CHUNK_OK;
        break;
    case SPOOL_INVAL_CHUNK_NoVolume:
        ret = SM_INVAL_CHUNK_NoVolume;
        break;
    case SPOOL_INVAL_CHUNK_NoChunk:
        ret = SM_INVAL_CHUNK_NoChunk;
        break;
    case SPOOL_INVAL_CHUNK_Fail:
        ret = SM_INVAL_CHUNK_Fail;
        break;
    }

    return ret;
}

/*
 * Description: change all free space chunk of a volume into invalidate state
 */
void sm_invalidate_volume (uint32_t volumeID)
{
    fspool_inval_pool(&discoC_fsMgr.fsPooltbl, volumeID);
}

int32_t sm_get_flush_criteria (void)
{
    return FLUSH_NUM_ASCHUNK;
}

sm_allocSpace_resp_t sm_alloc_volume_space (uint32_t volumeID, uint64_t lb_s, uint32_t lb_len,
        metadata_t *arr_MD, ReqCommon_t *callerReq)
{
    spool_allocSpace_resp_t fp_resp;
    sm_allocSpace_resp_t ret;

    fp_resp = fspool_alloc_space(&discoC_fsMgr.fsPooltbl, volumeID, lb_s, lb_len,
            arr_MD, callerReq);

    switch (fp_resp) {
    case SPOOL_ALLOC_SPACE_OK:
        ret = SM_ALLOC_SPACE_OK;
        break;
    case SPOOL_ALLOC_SPACE_NoVolume:
        ret = SM_ALLOC_SPACE_NoVolume;
        break;
    case SPOOL_ALLOC_SPACE_WAIT:
        ret = SM_ALLOC_SPACE_WAIT;
        break;
    case SPOOL_ALLOC_SPACE_NOSPACE:
        ret = SM_ALLOC_SPACE_NOSPACE;
        break;
    case SPOOL_ALLOC_SPACE_Fail:
        ret = SM_ALLOC_SPACE_Fail;
        break;
    default:
        ret = SM_ALLOC_SPACE_Fail;
        break;
    }

    return ret;
}

int32_t free_volume_fsPool (uint32_t volumeID)
{
    int32_t ret;

    ret = 0;

    dms_printk(LOG_LVL_INFO, "DMSC INFO free volume fspool %u\n", volumeID);

    sm_invalidate_volume(volumeID);
    sm_report_volume_unusespace(volumeID);
    sm_cleanup_volume(volumeID);
    sm_report_volume_spaceMData(volumeID);
    fspool_free_pool(&discoC_fsMgr.fsPooltbl, volumeID);

    dms_printk(LOG_LVL_INFO, "DMSC INFO free volume fspool %u DONE\n", volumeID);

    return ret;
}

int32_t create_volume_fsPool (uint32_t volumeID)
{
    freespace_pool_t *fsPool;
    int32_t ret;

    ret = 0;

    dms_printk(LOG_LVL_INFO, "DMSC INFO create volume fspool %u\n", volumeID);

    do {
        fsPool = fspool_create_pool(&discoC_fsMgr.fsPooltbl);
        if (IS_ERR_OR_NULL(fsPool)) {
            ret = -ENOMEM;
            break;
        }

        fspool_init_pool(&discoC_fsMgr.fsPooltbl, fsPool, volumeID);
    } while (0);

    dms_printk(LOG_LVL_INFO, "DMSC INFO create volume fspool %u DONE ret %d\n",
            volumeID, ret);

    return ret;
}

static int32_t _init_space_mgr (fs_manager_t *fsMgr)
{
    int32_t ret;

    ret = 0;
    do {
        atomic_set(&fsMgr->fs_ReqID_generator, 0);

        fsMgr->fstask_mpool_ID = register_mem_pool("fs_task", MAX_NUM_FS_TASK,
                sizeof(fs_task_t), true);
        if (fsMgr->fstask_mpool_ID < 0) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN fail register fs task mem pool %d\n",
                    fsMgr->fstask_mpool_ID);
            return -ENOMEM;
        }
        dms_printk(LOG_LVL_INFO, "DMSC INFO fs manger get task mem pool id %d\n",
                fsMgr->fstask_mpool_ID);

        fsMgr->fschunk_mpool_ID = register_mem_pool("fs_chunk", MAX_NUM_FSCHUNK,
                sizeof(fs_chunk_t), true);
        if (fsMgr->fschunk_mpool_ID < 0) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN fail register fs chunk mem pool %d\n",
                    fsMgr->fschunk_mpool_ID);
        }
        dms_printk(LOG_LVL_INFO, "DMSC INFO fs manger get fs chunk mem pool id %d\n",
                fsMgr->fschunk_mpool_ID);

        fsMgr->aschunk_mpool_ID = register_mem_pool("as_chunk", MAX_NUM_ASCHUNK,
                sizeof(as_chunk_t), true);
        if (fsMgr->aschunk_mpool_ID < 0) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN fail register as chunk mem pool %d\n",
                    fsMgr->aschunk_mpool_ID);
        }
        dms_printk(LOG_LVL_INFO, "DMSC INFO fs manger get as chunk mem pool id %d\n",
                fsMgr->aschunk_mpool_ID);

        fspool_init_table(&fsMgr->fsPooltbl);

        if ((ret = inif_fs_worker(&fsMgr->fs_ci_wker, fsMgr))) {
            dms_printk(LOG_LVL_INFO, "DMSC INFO fail init fs flush worker\n");
            break;
        }
    } while (0);

    return ret;
}

int32_t init_space_manager (void)
{
    int32_t ret;

    ret = 0;

    dms_printk(LOG_LVL_INFO, "DMSC INFO init space manager start\n");

    ret = _init_space_mgr(&discoC_fsMgr);

    dms_printk(LOG_LVL_INFO, "DMSC INFO init space manager DONE ret %d\n", ret);

    return ret;
}

int32_t rel_space_manager (void)
{
    int32_t ret;

    dms_printk(LOG_LVL_INFO, "DMSC INFO stop space manager start\n");

    ret = deregister_mem_pool(discoC_fsMgr.fstask_mpool_ID);

    ret = rel_fs_worker(&discoC_fsMgr.fs_ci_wker);

    fspool_rel_table(&discoC_fsMgr.fsPooltbl);

    ret = deregister_mem_pool(discoC_fsMgr.fschunk_mpool_ID);

    dms_printk(LOG_LVL_INFO, "DMSC INFO stop space manager DONE %d\n", ret);

    return ret;
}

void sm_show_storage_pool (void)
{
    dms_printk(LOG_LVL_INFO, "DMSC INFO show all storage pool:\n");
    fspool_show_table(&discoC_fsMgr.fsPooltbl);
}

int32_t sm_dump_info (int8_t *buffer, uint32_t buff_size)
{
    int32_t len;

    len = 0;

    len += sprintf(buffer, "ReqQ info: (workQ, pendQ) (%u, %u)\n",
            atomic_read(&(discoC_fsMgr.fs_ci_wker.num_flushReq_workq)),
            atomic_read(&(discoC_fsMgr.fs_ci_wker.num_flushReq_pendq)));
    if (len >= buff_size) {
        return len;
    }

    len += sprintf(buffer + len, "Reqs DONE info: (total, alloc, flush, return) (%u, %u, %u, %u)\n",
            discoC_fsMgr.fs_ci_wker.done_total_reqs, discoC_fsMgr.fs_ci_wker.done_prealloc_reqs,
            discoC_fsMgr.fs_ci_wker.done_flush_reqs, discoC_fsMgr.fs_ci_wker.done_return_reqs);
    if (len >= buff_size) {
        return len;
    }

    //TODO: show flush interval and next flush time

    len += fspool_dump_table(&discoC_fsMgr.fsPooltbl, buffer + len, buff_size - len);
    return len;
}
#endif
