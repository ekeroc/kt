/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * dms_client_mm.c
 *
 * The allocation mechanism is mix DMS-Client memory management and
 * kernel management.
 * Using DMS-Client memory management will speedup DMS performance.
 * When memory is not enough, DMS-Client memory management will
 * using kmalloc to create new space.
 *
 * A memory buffer item including:
 * char identifier : identifier = 0xff (mpool_item_mark), means allocated from os
 *                   identifier = 0x00 (mem_os_item_mark), means allocated from DMSC mem buffer
 * list_head       : linux struct list_head
 * data
 *
 * We need memory buffer item as a continuous space.
 */

#include <linux/version.h>
#include <net/sock.h>
#include <linux/swap.h>
#include <linux/delay.h>
#include "discoC_sys_def.h"
#include "common_util.h"
#include "dms_kernel_version.h"
#include "common_util.h"
#include "discoC_mem_manager.h"
#include "dms_client_mm.h"
#include "../metadata_manager/metadata_manager.h"
#include "dms_client_mm_private.h"
#include "../config/dmsc_config.h"
#include "../io_manager/discoC_IOSegment_Request.h"
#include "../io_manager/discoC_IO_Manager.h"
#include "../vdisk_manager/volume_manager.h"

static int32_t free_mpool_items (struct list_head *free_list)
{
    struct list_head *cur, *next;
    int32_t free_cnt;
    uint8_t *mem_data;

    if (unlikely(IS_ERR_OR_NULL(free_list) || list_empty(free_list))) {
        dms_printk(LOG_LVL_ERR,
                "DMSC FATAL ERROR NULL buffer list or list empty\n");
        DMS_WARN_ON(true);
        return 0;
    }

    free_cnt = 0;

    list_for_each_safe (cur, next, free_list) {
        //TODO: If feel comfortable, remove checking for better performance
        if (unlikely(IS_ERR_OR_NULL(cur))) {
            dms_printk(LOG_LVL_ERR,
                    "DMSC FATAL ERROR mem buffer list broken\n");
            DMS_WARN_ON(true);
            break;
        }

        if (dms_list_del(cur)) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN list_del chk fail %s %d\n",
                    __func__, __LINE__);
            DMS_WARN_ON(true);
        }

        mem_data = ((uint8_t*)cur) - DMSC_MM_HEADER_ID_SIZE;
        kfree(mem_data);
        free_cnt++;
    }

    return free_cnt;
}

static int32_t alloc_mpool_items (dmsc_mm_pool_type_t *mem_pool, uint32_t cnt)
{
    int32_t i, ret;
    uint8_t *tmp;
    size_t item_size;
    struct list_head *list_item;

    ret = 0;
    if (unlikely(IS_ERR_OR_NULL(mem_pool))) {
        dms_printk(LOG_LVL_ERR, "DMSC FATAL ERROR NULL mem pool list\n");
        DMS_WARN_ON(true);
        return -EINVAL;
    }

    item_size = mem_pool->item_size + DMSC_MM_HEADER_SIZE;
    for (i = 0; i < cnt; i++) {
        if (mem_pool->pool_size >= mem_pool->pool_max_size) {
            ret = -EPERM;
            break;
        }

        tmp = kmalloc(item_size, GFP_KERNEL);
        if (unlikely(IS_ERR_OR_NULL(tmp))) {
            dms_printk(LOG_LVL_INFO,
                    "DMSC INFO fail alloc mem for mem buffer item\n");
            ret = -EPERM;
            break;
        }

        memset(tmp, 0, item_size);
        *tmp = mpool_item_mark;
        list_item = (struct list_head *)(tmp + DMSC_MM_HEADER_ID_SIZE);
        INIT_LIST_HEAD(list_item);
        if (dms_list_add(list_item, &mem_pool->free_list)) {
            if (!IS_ERR_OR_NULL(tmp)) {
                kfree(tmp);
            }
            dms_printk(LOG_LVL_WARN, "DMSC WARN list_add chk fail %s %d\n",
                    __func__, __LINE__);
            DMS_WARN_ON(true);
            msleep(1000);
            ret = -EPERM;
            break;
        }

        mem_pool->pool_size++;
    }

    return 0;
}

static void client_mm_free_mem_pool (dmsc_mm_pool_type_t *mem_pool)
{
    int32_t free_cnt;

    if (unlikely(!list_empty(&mem_pool->used_list))) {
        dms_printk(LOG_LVL_ERR,
                "DMSC FATAL ERROR mem pool in using\n");
        DMS_WARN_ON(true);
        return;
    }

    free_cnt = free_mpool_items(&mem_pool->free_list);

    if (unlikely(free_cnt != mem_pool->pool_size)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN free cnt(%d) != mem_pool->pool_size(%d)\n",
                free_cnt, mem_pool->pool_size);
        DMS_WARN_ON(true);
    }
}

static int32_t client_mm_init_mem_pool (dmsc_mm_pool_type_t *mem_pool,
        int32_t pool_size, size_t item_size, bool is_mutex, bool is_lmem)
{
    if (unlikely(IS_ERR_OR_NULL(mem_pool) || 0 == pool_size ||
            0 == item_size)) {
        dms_printk(LOG_LVL_ERR, "DMSC FATAL ERROR fail to init mem pool\n");
        DMS_WARN_ON(true);
        return -EPERM;
    }

    mem_pool->pool_max_size = pool_size;
    mem_pool->pool_size = 0;
    atomic_set(&mem_pool->num_items_used, 0);
    INIT_LIST_HEAD(&mem_pool->free_list);
    INIT_LIST_HEAD(&mem_pool->used_list);
    mem_pool->item_size = item_size;
    mem_pool->is_mutex_lock = is_mutex;
    mem_pool->is_lmem = is_lmem;

    if (is_mutex) {
        mutex_init(&mem_pool->pool_lock.mlock);
    } else {
        spin_lock_init(&mem_pool->pool_lock.splock);
    }

    return 0;
}

static void *get_mem_from_os (dmsc_mm_pool_type_t *mem_pool)
{
    uint8_t *buf;
    size_t item_size;

    item_size = mem_pool->item_size + DMSC_MM_HEADER_SIZE;
    buf = kmalloc(item_size, GFP_NOWAIT | GFP_ATOMIC);

    if (unlikely(IS_ERR_OR_NULL(buf))) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN Cannot alloc mem from os\n");
        DMS_WARN_ON(true);
        return NULL;
    }

    memset(buf, 0, item_size);
    *buf = mem_os_item_mark;
    atomic_inc(&mem_pool->num_items_used);
    return (void *)(buf + DMSC_MM_HEADER_SIZE);
}

/* NOTE: return address is data part of memory buffer item
 *       Caller make sure mem_pool is not NULL;
 */
static void *get_mem_from_mpool (dmsc_mm_pool_type_t *mem_pool)
{
    struct list_head *cur, *next, *lfree_head;
    uint8_t *buf;
    bool list_bug;

    list_bug = false;
    lfree_head = &mem_pool->free_list;
    buf = NULL;

    if (mem_pool->is_mutex_lock) {
        mutex_lock(&mem_pool->pool_lock.mlock);
    } else {
        spin_lock(&mem_pool->pool_lock.splock);
    }

    if (unlikely(list_empty(lfree_head))) {
        dms_printk(LOG_LVL_INFO,
                "DMSC INFO ran out of mpool, alloc mem from os\n");
        goto ALLOC_MEM_FROM_OS;
    }

    list_for_each_safe (cur, next, lfree_head) {
        if (unlikely(IS_ERR_OR_NULL(cur))) {
            dms_printk(LOG_LVL_ERR,
                    "DMSC FATAL ERROR mem buffer list broken\n");
            DMS_WARN_ON(true);
            break;
        }

        if (dms_list_move_tail(cur, &mem_pool->used_list)) {
            list_bug = true;
            continue;
        }

        buf = (uint8_t*)cur;
        atomic_inc(&mem_pool->num_items_used);
        break;
    }

ALLOC_MEM_FROM_OS:
    if (mem_pool->is_mutex_lock) {
        mutex_unlock(&mem_pool->pool_lock.mlock);
        if (unlikely(list_bug)) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN list_move_tail chk fail %s %d\n",
                    __func__, __LINE__);
            DMS_WARN_ON(true);
            msleep(1000);
        }
    } else {
        spin_unlock(&mem_pool->pool_lock.splock);
        if (unlikely(list_bug)) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN list_move_tail chk fail %s %d\n",
                    __func__, __LINE__); //Callber might hold spinlock, cannot sleep
        }
    }

    if (unlikely(IS_ERR_OR_NULL(buf))) {
        return get_mem_from_os(mem_pool);
    }

    return (void *)(buf + DMSC_MM_HEADER_LL_SIZE);
}

/* NOTE: 1st parameter is data address of memory buffer item
 *       Caller make sure buf and mem_pool is not NULL;
 */
static void free_mem_to_mpool (void *buf, dmsc_mm_pool_type_t *mem_pool)
{
    struct list_head *list_item;
    uint8_t *start;
    bool list_bug;

    list_bug = false;

    start = (uint8_t *)buf - DMSC_MM_HEADER_SIZE;
    if (*start == mpool_item_mark) {
        if (mem_pool->is_mutex_lock) {
            mutex_lock(&mem_pool->pool_lock.mlock);
        } else {
            spin_lock(&mem_pool->pool_lock.splock);
        }

        list_item = (struct list_head *)(start + DMSC_MM_HEADER_ID_SIZE);
        if (dms_list_move_tail(list_item, &mem_pool->free_list)) {
            list_bug = true;
        } else {
            atomic_dec(&mem_pool->num_items_used);
        }

        if (mem_pool->is_mutex_lock) {
            mutex_unlock(&mem_pool->pool_lock.mlock);
            if (unlikely(list_bug)) {
                dms_printk(LOG_LVL_WARN, "DMSC WARN list_move_tail chk fail %s %d\n",
                        __func__, __LINE__);
                DMS_WARN_ON(true);
                msleep(1000);
            }
        } else {
            spin_unlock(&mem_pool->pool_lock.splock);
            if (unlikely(list_bug)) {
                dms_printk(LOG_LVL_WARN, "DMSC WARN list_move_tail chk fail %s %d\n",
                        __func__, __LINE__);
            }
        }
    } else {
        kfree(start);
        atomic_dec(&mem_pool->num_items_used);
    }
}

static void set_mpool_size (int32_t num_of_ioreq)
{
    IORREQ_POOL_SIZE = num_of_ioreq;
}

static bool is_io_req_congested (void)
{
    uint32_t ioreq_count;

    ioreq_count = atomic_read(&ioreq_pool.num_items_used);
    if (ioreq_count > Num_of_IO_Reqs_Congestion_On) {
        dms_printk(LOG_LVL_DEBUG,
                "DMSC DEBUG mem res busy %d (Num_of_IO_Reqs_Congestion_On: %d)\n",
                ioreq_count, Num_of_IO_Reqs_Congestion_On);

        return true;
    }

    return false;
}

void set_dmsc_mm_not_working (void)
{
    dmsc_mm_working = false;
}

void wake_up_mem_resource_wq (void)
{
    if (waitqueue_active(&mem_resources_wq)) {
        wake_up_interruptible_all(&mem_resources_wq);
    }
}

/*
 * NOTE: Don't using local variable to replace
 * atomic_read(&ioreq_pool.num_items_used) in wait_event_interruptible
 */
void sleep_to_wait_mem (void) {
    int32_t ret;

    ret = wait_event_interruptible(mem_resources_wq,
            atomic_read(&ioreq_pool.num_items_used) <=
            Num_of_IO_Reqs_Congestion_On || !dmsc_mm_working);

    /*
     * NOTE: http://stackoverflow.com/questions/9576604/what-does-erestartsys-used-while-writing-linux-driver
     * -ERESTARTSYS is connected to the concept of a restartable system call.
     * A restartable system call is one that can be transparently re-executed
     * by the kernel when there is some interruption.
     */
    if (-ERESTARTSYS == ret) {
        dms_printk(LOG_LVL_DEBUG,
                "DMSC DEBUG fail to sleep exec again\n");
    }
    //WARN_ON(-ERESTARTSYS == ret);
}

static dmsc_mm_pool_type_t *get_mem_pool (mem_buf_type_t mtype)
{
    switch (mtype) {
    case MEM_Buf_IOReq:
        return (&ioreq_pool);
    default:
        dms_printk(LOG_LVL_INFO,
                "DMSC INFO Unknown mem_typ %d to get mem pool mem\n", mtype);
        DMS_WARN_ON(true);
        break;
    }

    return NULL;
}

void *ccma_malloc_gut (mem_buf_type_t mtype, size_t m_size, gfp_t m_flags)
{
    dmsc_mm_pool_type_t *m_pool;
    void *ret_buff;

        m_pool = get_mem_pool(mtype);
        if (likely(m_pool != NULL)) {
            ret_buff = get_mem_from_mpool(m_pool);
        } else {
            ret_buff = NULL;
        }

    return ret_buff;
}

void *ccma_malloc_dofc (mem_buf_type_t mtype)
{
    if (mtype == MEM_Buf_IOReq && is_io_req_congested()) {
        return NULL;
    }

    return ccma_malloc_gut(mtype, MA_DUMMY_SIZE, MA_DUMMY_FLAG);
}

void ccma_free_gut (mem_buf_type_t mtype, void *buff)
{
    dmsc_mm_pool_type_t *m_pool;

    if (unlikely(buff == NULL)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN try to free a NULL memory space\n");
        WARN_ON(true);
        return;
    }

        m_pool = get_mem_pool(mtype);
        if (likely(m_pool != NULL)) {
            free_mem_to_mpool(buff, m_pool);
        }

        if (MEM_Buf_IOReq == mtype) {
            if (atomic_read(&m_pool->num_items_used) <=
                    Num_of_IO_Reqs_Congestion_On) {
                wake_up_mem_resource_wq();
            }
        }
}

int32_t get_mem_pool_size (mem_buf_type_t mem_type)
{
    dmsc_mm_pool_type_t *m_pool;

        m_pool = get_mem_pool(mem_type);
        if (m_pool != NULL) {
            return m_pool->pool_size;
        }
    return 0;
}

size_t get_mem_item_size (mem_buf_type_t mem_type)
{
    dmsc_mm_pool_type_t *m_pool;

        m_pool = get_mem_pool(mem_type);
        if (m_pool != NULL) {
            return (m_pool->item_size + DMSC_MM_HEADER_SIZE);
        }

    return 0;
}

int32_t get_mem_usage_cnt (mem_buf_type_t mem_type)
{
    dmsc_mm_pool_type_t *m_pool;

        m_pool = get_mem_pool(mem_type);
        if (m_pool != NULL) {
            return atomic_read(&m_pool->num_items_used);
        }

    return 0;
}

//TODO: cong control should including payload buffer
void set_mm_cong_control (int32_t value)
{
    Num_of_IO_Reqs_Congestion_On = value;
}

void client_mm_release (void)
{
    client_mm_free_mem_pool(&ioreq_pool);
}

static int32_t _mm_init_pool (void)
{
    int32_t ret;

    ret = -EINVAL;

    do {
        if (client_mm_init_mem_pool(&ioreq_pool, IORREQ_POOL_SIZE,
                sizeof(struct io_request), true, false)) {
            dms_printk(LOG_LVL_ERR,
                    "DMSC ERROR fail init mem for io req mem buffer\n");
            break;
        }

        ret = 0;
    } while (0);

    return ret;
}

static int32_t _fill_pool_items (void)
{
    int32_t ret, i;

    for (i = 0; i < ioreq_pool.pool_max_size; i++) {
        ret = alloc_mpool_items(&ioreq_pool, 1);
        if (ret && (ioreq_pool.pool_size != ioreq_pool.pool_max_size)) {
            dms_printk(LOG_LVL_INFO,
                    "DMSC INFO not enoguh free mem to ioreq %d %d\n",
                    ioreq_pool.pool_size,
                    ioreq_pool.pool_max_size);
            break;
        }
    }

    return 0;
}

int32_t client_mm_init (int32_t num_of_ioreq)
{
    uint64_t total_mem_IO_size;
    int32_t ret;

    total_mem_IO_size = 0;
    set_mpool_size(num_of_ioreq);

    if ((ret = _mm_init_pool())) {
        return ret;
    }

    if ((ret = _fill_pool_items())) {
        return ret;
    }

    total_mem_IO_size += ioreq_pool.pool_size*sizeof(struct io_request);

    init_waitqueue_head(&mem_resources_wq);
    set_mm_cong_control((ioreq_pool.pool_size*7)/8);

    dmsc_mm_working = true;

    dms_printk(LOG_LVL_INFO,
            "DMSC INFO total memory buffer consumption %llu KB\n",
            total_mem_IO_size);

    return 0;
}
