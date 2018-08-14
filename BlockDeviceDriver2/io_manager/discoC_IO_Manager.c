/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_IO_Manager.c
 */
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/types.h>
#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/dms_kernel_version.h"
#include "../common/thread_manager.h"
#include "../common/dms_client_mm.h"
#include "../common/discoC_mem_manager.h"
#include "../metadata_manager/metadata_manager.h"
#include "discoC_IOSegment_Request.h"
#include "discoC_IO_Manager.h"
#include "discoC_IO_Manager_private.h"
#include "discoC_IO_Manager_export.h"
#include "../vdisk_manager/volume_manager.h"
#include "io_worker_manager.h"
#include "../config/dmsc_config.h"
#include "discoC_IODone_worker.h"
#include "discoC_IOReqPool.h"
#include "discoC_IORequest.h"
#include "discoC_IOSegLeaf_Request.h"
#include "../flowcontrol/FlowControl.h"
#include "../discoNN_client/discoC_NNC_Manager_api.h"
#ifdef ENABLE_PAYLOAD_CACHE_FOR_DEBUG
#include "../common/cache.h"
#endif

int32_t IOMgr_get_onfly_ioReq (void)
{
    return atomic_read(&discoCIO_Mgr.cnt_onfly_ioReq);
}

int32_t IOMgr_get_onfly_ovlpioReq (void)
{
    return atomic_read(&discoCIO_Mgr.cnt_onfly_ovlpioReq);
}

int32_t IOMgr_get_onfly_ioSReq (void)
{
    return atomic_read(&discoCIO_Mgr.cnt_split_io_req);
}

int32_t IOMgr_get_commit_qSize (void)
{
    return IORFWkerMgr_workq_size();
}

int32_t IOMgr_get_IOWbuffer_mpoolID (void)
{
    return discoCIO_Mgr.io_wBuffer_mpool_ID;
}

uint32_t IOMgr_get_last_ioReqID (void)
{
    return (uint32_t)atomic_read(&discoCIO_Mgr.ioReqID_gentor);
}

uint32_t IOMgr_alloc_ioReqID (void)
{
    return (uint32_t)atomic_add_return(1, &discoCIO_Mgr.ioReqID_gentor);
}

dms_rq_t *IOMgr_alloc_dmsrq (void)
{
    return (dms_rq_t *)discoC_malloc_pool(discoCIO_Mgr.io_dmsrq_mpool_ID);
}

void IOMgr_free_dmsrq (dms_rq_t *rq)
{
    discoC_free_pool(discoCIO_Mgr.io_dmsrq_mpool_ID, rq);
}

static bool _IOM_chk_ioblock_long (uint64_t t_start) {
    uint64_t t_end = jiffies_64;

    if (time_after64(t_end, t_start)) {
        if (jiffies_to_msecs(t_end - t_start) >= IOREQ_BLOCK_LIMITATION) {
            return true;
        }
    }

    return false;
}

/*
 * execute the Flow Control
 */
static void _IOM_block_IO (dms_io_block_code_t fc_type, volume_device_t *drv,
        int32_t wait_time)
{
    int32_t i;

    switch (fc_type) {
    case DMS_IO_BLK_MEM_DEF:    /* Memory Deficiency */
        sleep_to_wait_mem();
        dms_printk(LOG_LVL_DEBUG,
                "DMSC DEBUG io blk caused by 7/8 mem buffer cong\n");
        break;

    case DMS_IO_BLK_ATTACH: /* Another dev is being attached */
        for (i = 0; i < MAX_TIME_OUT_ATTACH; i++) {
            do_flow_control(WAIT_TYPE_FIX_TIME, wait_time);
            if (is_vol_yield_io(drv) == false) {
                break;
            }
        }
        set_volume_not_yield(drv);
        dms_printk(LOG_LVL_INFO, "DMSC INFO io blk caused by attach\n");
        break;

    case DMS_IO_BLK_NN_BLOCK:   /* NN is taking Sanpshot */
        while (is_vol_block_io(drv)) {
            do_flow_control(WAIT_TYPE_FIX_TIME, wait_time);
        }

        dms_printk(LOG_LVL_INFO,
                "DMSC start IO by rcv non-block cmd from nn %d\n",
                drv->vol_id);

        break;
    default:
        dms_printk(LOG_LVL_WARN, "DMSC WARN unknow do fc_type %d\n", fc_type);
        DMS_WARN_ON(true);
        break;
    }
}

/*
 * NOTE: No matter what kind of fc is proceeding, the final action is
 *       block current volume's IO for a while, so we don't need check
 *       each kinds of situation.
 */
static io_request_t *_IOM_alloc_ioReq (volume_device_t *drv,
        dms_io_block_code_t *block_code,
        bool do_block, int32_t *wait_time)
{
    io_request_t *io_req;

    io_req = NULL;

    do {
        if (do_block == false) {
            io_req = (struct io_request *)ccma_malloc_pool(MEM_Buf_IOReq);
            *wait_time = 0;
            *block_code = DMS_IO_BLK_TOGO;
            break;
        }

        //TODO: If read op, don't need block
        if (is_vol_block_io(drv)) {
            dms_printk(LOG_LVL_INFO, "DMSC INFO volume %d rcv block command\n",
                    drv->vol_id);
            *wait_time = BASIC_WAIT_UNIT;
            *block_code = DMS_IO_BLK_NN_BLOCK;
            break;
        }

        /* Another dev is being attached */
        if (is_vol_yield_io(drv)) {
            dms_printk(LOG_LVL_INFO, "DMSC INFO volume %d io yield\n",
                    drv->vol_id);
            *wait_time = BASIC_WAIT_UNIT;
            *block_code = DMS_IO_BLK_ATTACH;
            break;
        }

        *wait_time = 0;
        io_req = (struct io_request *)ccma_malloc_dofc(MEM_Buf_IOReq);
        if (IS_ERR_OR_NULL(io_req)) {
            dms_printk(LOG_LVL_DEBUG,
                    "DMSC DEBUG volume %d io stop due to 7/8 mem buffer cong\n",
                    drv->vol_id);
            *block_code = DMS_IO_BLK_MEM_DEF;
        } else {
            *block_code = DMS_IO_BLK_TOGO;
        }
    } while (0);

    return io_req;
}

static void _init_IOR_fail_free_allRQ (dms_rq_t **dmsrqs,
        uint32_t index_s, uint32_t num_rqs)
{
    int32_t i;

    for (i = index_s; i < num_rqs; i++) {
        dmsrqs[i]->result = false;
        if (atomic_sub_return(1, &dmsrqs[i]->num_ref_ior) == 0) {
            commit_user_request(dmsrqs[i]->rq, false);
            dmsrqs[i]->rq = NULL;
            IOMgr_free_dmsrq(dmsrqs[i]);
            dmsrqs[i] = NULL;
        }
    }
}

static io_request_t *_submit_alloc_IOR (volume_device_t *drv,
        dms_rq_t **dmsrqs, uint32_t index_s, priority_type *req_prior)
{
    io_request_t *io_req;
    uint64_t io_req_start_time;
    int32_t wait_time;
    dms_io_block_code_t io_block_ctl;
    bool do_block;

    io_req = NULL;
    /*
     * For volume who is not in 'Attaching' mode, a.k.a., is in 'Ready' mode,
     * we need to honor possible Flow Control(s).
     */
    //TODO : Don't read state directly
    do_block = (drv->vol_state != Vol_Attaching);
    io_req_start_time = jiffies_64;
    while ((io_req = _IOM_alloc_ioReq(drv, &io_block_ctl, do_block,
                                             &wait_time)) == NULL) {
        _IOM_block_IO(io_block_ctl, drv, wait_time);

        if (_IOM_chk_ioblock_long(io_req_start_time)) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN io req block too long type %d\n", io_block_ctl);

            if (DMS_IO_BLK_NN_BLOCK == io_block_ctl) {
                unblock_volume_io(drv->vol_id);
                dms_printk(LOG_LVL_WARN,
                        "DMSC WARN rollback to unblock and allow IO\n");
            } else {
                break;
            }
        }
    }

    *req_prior = do_block? PRIORITY_LOW : PRIORITY_HIGH;
    return io_req;
}

void IOMgr_submit_IOReq (dms_rq_t **dmsrqs,
        uint32_t index_s, uint32_t num_rqs,
        uint64_t sect_s, uint32_t sect_len)
{
    volume_device_t *drv;
    io_request_t *io_req;
    int32_t ret_code;
    priority_type req_prior;

    drv = (volume_device_t *)dmsrqs[index_s]->rq->rq_disk->private_data;
    io_req = _submit_alloc_IOR(drv, dmsrqs, index_s, &req_prior);
    if (unlikely(IS_ERR_OR_NULL(io_req))) {
        //NOTE: means no available memory buffer and linux cannot alloc
        //      a memory for io request.
        dms_printk(LOG_LVL_WARN, "DMSC WARN fail to get io req buf\n");
        DMS_WARN_ON(true);

        _init_IOR_fail_free_allRQ(dmsrqs, index_s, num_rqs);
        return;
    }

    ret_code = ioReq_init_request(io_req, drv, req_prior, dmsrqs,
            index_s, num_rqs, sect_s, sect_len);
    if (ret_code) {
        _init_IOR_fail_free_allRQ(dmsrqs, index_s, num_rqs);
    } else {
        if (ioReq_add_ReqPool(io_req)) {
            ioReq_gen_ioSegReq(io_req);
            ioReq_submit_request(io_req);
        }
    }
}

void commit_user_request (struct request *rq, bool result)
{
    spinlock_t *lck;
    volume_device_t *vol;
    unsigned long flags;

    if (unlikely(IS_ERR_OR_NULL(rq))) {
        return;
    }

    vol = (volume_device_t *)rq->rq_disk->private_data;
    lck = rq->q->queue_lock;

    spin_lock_irqsave(lck, flags);
    dms_end_request(rq, result, blk_rq_bytes(rq));
    atomic_dec(&vol->lgr_rqs);
    spin_unlock_irqrestore(lck, flags);
}

int32_t create_volumeIOReq_queue (uint32_t volumeID)
{
    volIOReqPool_Mgr_t *volIOQ_mgr;
    volIOQ_mgr = volIOReqQMgr_get_manager();

    return volIOReqQMgr_add_pool(volumeID, volIOQ_mgr);
}

static void _clear_all_IOReq (struct list_head *buff_list)
{
    io_request_t *cur, *next;

    list_for_each_entry_safe (cur, next, buff_list, list2ioReqPool) {
        list_del(&cur->list2ioReqPool);
        ioReq_commit_user(cur, false);
        ioReq_end_request(cur);
    }
}

int32_t clear_volumeIOReq_queue (uint32_t volumeID)
{
    struct list_head buff_list;
    int32_t ret;
    volIOReqPool_Mgr_t *volIOQ_mgr;
    volIOQ_mgr = volIOReqQMgr_get_manager();

    ret = volIOReqQMgr_clear_pool(volumeID, volIOQ_mgr, &buff_list);

    if (ret < 0) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN fail clear vol %u IO queue\n",
                volumeID);
    } else if (ret > 0) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN linger IOReq in volume IO queue %u\n",
                volumeID);
        _clear_all_IOReq(&buff_list);
    }

    return ret;
}
void remove_volumeIOReq_queue (uint32_t volumeID)
{
    volIOReqPool_Mgr_t *volIOQ_mgr;
    volIOQ_mgr = volIOReqQMgr_get_manager();

    volIOReqQMgr_rm_pool(volumeID, volIOQ_mgr);
}

void IOMgr_rel_manager (void)
{
    volIOReqPool_Mgr_t *volIOQ_mgr;

    volIOQ_mgr = volIOReqQMgr_get_manager();
    volIOReqQMgr_rel_manager(volIOQ_mgr);
    IORFWkerMgr_rel_manager();
    deregister_mem_pool(discoCIO_Mgr.ioSeg_leafR_mpool_ID);
    deregister_mem_pool(discoCIO_Mgr.ioSeg_mpool_ID);
    deregister_mem_pool(discoCIO_Mgr.io_wBuffer_mpool_ID);
    deregister_mem_pool(discoCIO_Mgr.io_dmsrq_mpool_ID);
}

void IOMgr_init_manager (void)
{
    volIOReqPool_Mgr_t *volIOQ_mgr;
    int32_t num_req_size;

    atomic_set(&discoCIO_Mgr.cnt_onfly_ovlpioReq, 0);
    atomic_set(&discoCIO_Mgr.cnt_onfly_ioReq, 0);
    atomic_set(&discoCIO_Mgr.cnt_split_io_req, 0);
    atomic_set(&discoCIO_Mgr.ioReqID_gentor, 0);

    num_req_size = dms_client_config->max_nr_io_reqs*MAX_LBS_PER_RQ;
    discoCIO_Mgr.ioSeg_mpool_ID = register_mem_pool("IOSegRequest",
            num_req_size, sizeof(ioSeg_req_t), true);
    if (discoCIO_Mgr.ioSeg_mpool_ID < 0) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN fail register io seg mem pool %d\n",
                discoCIO_Mgr.ioSeg_mpool_ID);
        return;
    }

    discoCIO_Mgr.ioSeg_leafR_mpool_ID = register_mem_pool("IOSeg_leafReq",
            num_req_size, sizeof(ioSeg_leafReq_t), true);
    if (discoCIO_Mgr.ioSeg_leafR_mpool_ID < 0) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN fail register ioSeg leaf mem pool %d\n",
                discoCIO_Mgr.ioSeg_leafR_mpool_ID);
        return;
    }

    num_req_size = dms_client_config->max_nr_io_reqs + IOREQ_WBUFFER_EXTRA;
    discoCIO_Mgr.io_wBuffer_mpool_ID = register_mem_pool("IO_WriteBuffer",
            num_req_size, MAX_SIZE_KERNEL_MEMCHUNK, true);
    if (discoCIO_Mgr.io_wBuffer_mpool_ID < 0) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN fail register io write buffer mem pool %d\n",
                discoCIO_Mgr.io_wBuffer_mpool_ID);
        return;
    }

    num_req_size = DEFAULT_MAX_NUM_OF_VOLUMES*MAX_NUM_KERNEL_RQS + EXTRA_BUFF_SIZE;
    discoCIO_Mgr.io_dmsrq_mpool_ID = register_mem_pool("IO_DMSRQ",
            num_req_size, sizeof(dms_rq_t), true);
    if (discoCIO_Mgr.io_dmsrq_mpool_ID < 0) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN fail register io rq buffer mem pool %d\n",
                discoCIO_Mgr.io_dmsrq_mpool_ID);
        return;
    }

    atomic_set(&discoCIO_Mgr.ioSegID_gentor, 0);
    atomic_set(&discoCIO_Mgr.ioSleafR_ID_gentor, 0);

    volIOQ_mgr = volIOReqQMgr_get_manager();
    volIOReqQMgr_init_manager(volIOQ_mgr);

    init_ioworker_manager(&iowker_Mgr, NUM_DMS_IO_THREADS);
    IORFWkerMgr_init_manager();
}
