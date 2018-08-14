/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_mem_manager.c
 *
 */

#include <linux/version.h>
#include <linux/swap.h>
#include <linux/wait.h>
#include "discoC_sys_def.h"
#include "common_util.h"
#include "dms_kernel_version.h"
#include "discoC_mem_manager_private.h"
#include "discoC_mem_manager.h"

/*
 * _get_mem_pool: get memory pool from memory pool manager by pool_id
 * @pool_id: which memory pool want search
 *
 * Return:
 *     mem_pool_t: memory pool
 */
static mem_pool_t *_get_mem_pool (int32_t pool_id)
{
    mem_pool_t *mmpool_item;

    if (pool_id < 0 || pool_id > MAX_DISCOC_MEM_POOL) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN invalid mem pool id %d\n", pool_id);
        return NULL;
    }

    mmpool_item = &memPool_mgr.arr_mem_pool[pool_id];

    return mmpool_item;
}

/*
 * _get_mem_from_pool: Allocate a memory chunk from memory pool
 * @mmpool_item: which memory pool to be allocate memory
 *
 * Return: memory chunk pointer
 */
static void *_get_mem_from_pool (mem_pool_t *mmpool_item)
{
    struct list_head *cur, *next;
    data_mem_item_t *lmem_entry;
    uint8_t *buf;

    buf = NULL;
    if (mmpool_item->is_mutex_lock) {
        mutex_lock(&mmpool_item->pool_lock.mlock);
    } else {
        spin_lock(&mmpool_item->pool_lock.splock);
    }

    list_for_each_safe (cur, next, &mmpool_item->free_list) {
        if (unlikely(IS_ERR_OR_NULL(cur))) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN mem buffer list broken %s\n", mmpool_item->pool_name);
            DMS_WARN_ON(true);
            break;
        }

        list_move_tail(cur, &mmpool_item->used_list);

        if (mmpool_item->is_lmem == false) {
            buf = (void *)((uint8_t *)cur + MM_HEADER_SIZE);
        } else {
            lmem_entry = list_entry(cur, data_mem_item_t, list);
            buf = (void *)lmem_entry;
        }

        atomic_dec(&mmpool_item->num_free_items);

        break;
    }

    if (mmpool_item->is_mutex_lock) {
        mutex_unlock(&mmpool_item->pool_lock.mlock);
    } else {
        spin_unlock(&mmpool_item->pool_lock.splock);
    }

    return buf;
}

/*
 * mem_pool_alloc: Allocate a memory chunk from memory pool
 * @pool_id: which memory pool to be allocate memory
 *
 * Return: memory chunk pointer
 *
 * NOTE: If there is not enough memory chunk, this function will block user thread
 *       until someone return memory chunk to pool (mem_pool_free)
 */
void *mem_pool_alloc (int32_t pool_id)
{
    mem_pool_t *mmpool_item;
    void *mem;
    int32_t ret, cnt;

    mmpool_item = _get_mem_pool(pool_id);
    if (unlikely(mmpool_item == NULL)) {
        return NULL;
    }

    mem = NULL;
    cnt = 0;
ALLOC_WAIT_AGAIN:
    ret = wait_event_interruptible_timeout((mmpool_item->mem_res_waitq),
                    atomic_read(&mmpool_item->num_free_items) > 0 ||
                    !mmpool_item->is_pool_working, memPool_mgr.allc_wait_warn_time);

    if (ret > 0) {
        if (mmpool_item->is_pool_working) {
            mem = _get_mem_from_pool(mmpool_item);
            if (mem == NULL) {
                dms_printk(LOG_LVL_INFO, "DMSC INFO fail alloc mem from pool %d\n",
                        atomic_read(&mmpool_item->num_free_items));
                goto ALLOC_WAIT_AGAIN;
            }
        } else {
            dms_printk(LOG_LVL_INFO,
                    "DMSC IFNO fail alloc mem from pool %s mem pool not working\n",
                    mmpool_item->pool_name);
        }
    } else if (ret == 0) {
        cnt++;
        dms_printk(LOG_LVL_INFO,
                "DMSC IFNO fail alloc mem from pool %s after %d seconds\n",
                mmpool_item->pool_name, cnt*MEM_WAIT_WARN_TIME);
        goto ALLOC_WAIT_AGAIN;
    } else {
        /*
         * NOTE: http://stackoverflow.com/questions/9576604/what-does-erestartsys-used-while-writing-linux-driver
         * -ERESTARTSYS is connected to the concept of a restartable system call.
         * A restartable system call is one that can be transparently re-executed
         * by the kernel when there is some interruption.
         */
        dms_printk(LOG_LVL_WARN,
                        "DMSC WARN mem pool alloc fail to sleep exec again %s\n",
                        mmpool_item->pool_name);
        mem = NULL;
    }

    return mem;
}

/*
 * mem_pool_alloc_nowait: Allocate a memory chunk from memory pool without block when no memory
 * @pool_id: which memory pool to be allocate memory
 *
 * Return: memory chunk pointer or null when no memory
 *
 */
void *mem_pool_alloc_nowait (int32_t pool_id)
{
    mem_pool_t *mmpool_item;

    mmpool_item = _get_mem_pool(pool_id);
    if (unlikely(mmpool_item == NULL)) {
        return NULL;
    }

    return _get_mem_from_pool(mmpool_item);
}

/*
 * _free_mem_to_pool: The actual implmentation of free memory chunk to pool
 * @mem_pool: which memory pool to be return
 * @buf: memory chunk to return to pool
 *
 */
static void _free_mem_to_pool (void *buf, mem_pool_t *mem_pool)
{
    struct list_head *list_item;
    data_mem_item_t *lmem_entry;

    list_item = (struct list_head *)((uint8_t *)buf - MM_HEADER_SIZE);

    if (mem_pool->is_mutex_lock) {
        mutex_lock(&mem_pool->pool_lock.mlock);
    } else {
        spin_lock(&mem_pool->pool_lock.splock);
    }

    if (mem_pool->is_lmem == false) {
        list_move_tail(list_item, &mem_pool->free_list);
    } else {
        lmem_entry = (data_mem_item_t *)buf;
        list_move_tail(&lmem_entry->list, &mem_pool->free_list);
    }

    atomic_inc(&mem_pool->num_free_items);

    if (mem_pool->is_mutex_lock) {
        mutex_unlock(&mem_pool->pool_lock.mlock);
    } else {
        spin_unlock(&mem_pool->pool_lock.splock);
    }
}

/*
 * mem_pool_free: return a memory chunk to memory pool
 * @pool_id: which memory pool to be return
 * @mem: memory chunk to return to pool
 *
 */
void mem_pool_free (int32_t pool_id, void *mem)
{
    mem_pool_t *mmpool_item;

    mmpool_item = _get_mem_pool(pool_id);
    if (unlikely(mmpool_item == NULL)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN fail free mem invalid pool id %d\n", pool_id);
        return;
    }

    _free_mem_to_pool(mem, mmpool_item);
    if (waitqueue_active(&mmpool_item->mem_res_waitq)) {
        wake_up_interruptible_all(&mmpool_item->mem_res_waitq);
    }
}

/*
 * discoC_mem_alloc: wrapper of kmalloc. Alloc a memory for user and increase cnt_mm_misc
 *                   for memory usage tracking
 * @size: memory size user want allocate
 * @flags: which allocation flags for kmalloc
 *
 * Return: memory pointer
 *
 */
void *discoC_mem_alloc (size_t size, gfp_t flags)
{
    void *result;

    result = kmalloc(size, flags);
    if (unlikely(IS_ERR_OR_NULL(result))) {
        dms_printk(LOG_LVL_ERR,
                "DMSC ERR malloc err result = %p size = %ld "
                "system free pages: %llu\n",
                result, size, (uint64_t)nr_free_pages());
        DMS_WARN_ON(true);
        return NULL;
    }

    memset(result, 0, size);
    atomic_inc(&memPool_mgr.cnt_mm_misc);

    return result;
}

/*
 * discoC_mem_free: wrapper of kfree. free memory alloced from discoC_mem_alloc
 * @mem: memory to be freed
 *
 */
void discoC_mem_free (void *mem)
{
    kfree(mem);
    atomic_dec(&memPool_mgr.cnt_mm_misc);
}

/*
 * _alloc_mmpool_items:
 *     alloc memory chunk with size < 128K and add these chunks to memory pool
 * @mmpool_item: target memory pool to be added
 *
 * Return: setup success or fail
 *         0: initialize memory success
 *        ~0: initialize fail
 */
static int32_t _alloc_mmpool_items (mem_pool_t *mmpool_item)
{
    uint8_t *tmp;
    size_t item_size;
    struct list_head *list_item;

    item_size = mmpool_item->item_size + MM_HEADER_SIZE;

    tmp = kmalloc(item_size, GFP_KERNEL);
    if (unlikely(IS_ERR_OR_NULL(tmp))) {
        dms_printk(LOG_LVL_INFO,
                "DMSC INFO fail alloc mem for mem pool %s\n",
                mmpool_item->pool_name);
        return -ENOMEM;
    }

    memset(tmp, 0, item_size);
    list_item = (struct list_head *)(tmp);
    INIT_LIST_HEAD(list_item);
    list_add_tail(list_item, &mmpool_item->free_list);
    mmpool_item->pool_size++;
    atomic_inc(&mmpool_item->num_free_items);

    return 0;
}

/*
 * _alloc_lmmpool_items:
 *     alloc memory chunk with size = 128K and add these chunks to memory pool
 * @mmpool_item: target memory pool to be added
 *
 * Return: setup success or fail
 *         0: initialize memory success
 *        ~0: initialize fail
 */
static int32_t _alloc_lmmpool_items (mem_pool_t *mmpool_item)
{
    data_mem_item_t *item_ptr;

    item_ptr = kmalloc(sizeof(data_mem_item_t), GFP_KERNEL);
    if (IS_ERR_OR_NULL(item_ptr)) {
        dms_printk(LOG_LVL_INFO, "DMSC INFO fail alloc mem for lmem item %s\n",
                mmpool_item->pool_name);
        return -ENOMEM;
    }

    INIT_LIST_HEAD(&item_ptr->list);
    item_ptr->data_ptr = kmalloc(MAX_SIZE_KERNEL_MEMCHUNK, GFP_KERNEL);
    if (unlikely(IS_ERR_OR_NULL(item_ptr->data_ptr))) {
        kfree(item_ptr);
        dms_printk(LOG_LVL_INFO,
                "DMSC INFO fail alloc mem for lmem item %s\n",
                mmpool_item->pool_name);
        return -ENOMEM;
    }

    memset(item_ptr->data_ptr, 0, MAX_SIZE_KERNEL_MEMCHUNK);
    list_add_tail(&item_ptr->list, &mmpool_item->free_list);
    mmpool_item->pool_size++;
    atomic_inc(&mmpool_item->num_free_items);

    return 0;
}

/*
 * _fill_pool_items:
 *     Use kmalloc to allocate enough memory chunk and add them to pool
 * @mmpool_item: target memory pool to be added
 *
 * Return: setup success or fail
 *         0: initialize memory success
 *        ~0: initialize fail
 */
static int32_t _fill_pool_items (mem_pool_t *mmpool_item)
{
    int32_t i, ret;

    ret = 0;

    for (i = 0; i < mmpool_item->pool_max_size; i++) {
        if (mmpool_item->is_lmem) {
            if (_alloc_lmmpool_items(mmpool_item)) {
                dms_printk(LOG_LVL_INFO, "DMSC INFO not enough mem for %s "
                        "expected %d current %d\n",
                        mmpool_item->pool_name, mmpool_item->pool_max_size,
                        mmpool_item->pool_size);
                break;
            }

            continue;
        }

        if (_alloc_mmpool_items(mmpool_item)) {
            dms_printk(LOG_LVL_INFO, "DMSC INFO not enough mem for %s "
                    "expected %d current %d\n",
                    mmpool_item->pool_name, mmpool_item->pool_max_size,
                    mmpool_item->pool_size);
            break;
        }
    }

    if (mmpool_item->pool_size == 0) {
        ret = -ENOMEM;
    }

    return ret;
}

/*
 * _setup_mempool_item:
 *     initialize a memory pool
 *     fill all memory chunk to pool
 * @_setup_mempool_item: target memory pool to be initialized
 * @pool_nm: pool name provided by user
 * @pool_size: number of memory chunks user want
 * @item_size: size of memory chunk
 * @is_mutex: user mutex or spin lock to protect
 *
 * Return: setup success or fail
 *         0: initialize memory success
 *        ~0: initialize fail
 */
static int32_t _setup_mempool_item (mem_pool_t *mmpool_item, int8_t *pool_nm,
        int32_t pool_size, size_t item_size, bool is_mutex)
{
    int32_t ret;

    ret = 0;

    mmpool_item->is_pool_working = false;
    memcpy(mmpool_item->pool_name, pool_nm, strlen(pool_nm));
    mmpool_item->pool_max_size = pool_size;
    mmpool_item->item_size = item_size;
    mmpool_item->is_mutex_lock = is_mutex;
    if (is_mutex) {
        mutex_init(&mmpool_item->pool_lock.mlock);
    } else {
        spin_lock_init(&mmpool_item->pool_lock.splock);
    }

    if (item_size >= MAX_SIZE_KERNEL_MEMCHUNK) {
        mmpool_item->is_lmem = true;
    } else {
        mmpool_item->is_lmem = false;
    }
    init_waitqueue_head(&mmpool_item->mem_res_waitq);

    ret = _fill_pool_items(mmpool_item);

    if (!ret) {
        mmpool_item->is_pool_working = true;
    }

    return ret;
}

/*
 * _alloc_mem_pool: allocate a pool id (a pool item in arr_mem_pool) from memory pool manager
 * @myMgr: memory pool manager
 *
 * Return: a memory pool id
 *        >= 0 a normal pool id
 *        < 0: register fail
 */
static int32_t _alloc_mem_pool (mem_pool_mgr_t *myMgr)
{
    int32_t pool_id;

    pool_id = atomic_add_return(1, &myMgr->pool_ID_generator);
    if (pool_id >= MAX_DISCOC_MEM_POOL) {
        pool_id = -ENOMEM;
    }

    return pool_id;
}

/*
 * register_mem_pool: register to get a memory pool
 * @pool_nm: memory pool name
 * @pool_size: number memory chunk user want
 * @item_size: size of memory chunk
 * @is_mutex: use mutex or spin lock to protect allocation/free in pool
 *
 * Return: >= 0 a normal pool id
 *        < 0: register fail
 */
int32_t register_mem_pool (int8_t *pool_nm, int32_t pool_size, size_t item_size,
        bool is_mutex)
{
    int32_t pool_id, ret;

    pool_id = -ENOMEM;
    do {
        if (IS_ERR_OR_NULL(pool_nm)) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN fail alloc mem pool null name\n");
            pool_id = -EINVAL;
            break;
        }

        if (pool_size <= 0 || item_size <= 0) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN fail alloc mem pool %s "
                    "size %d %d < 0\n",
                    pool_nm, pool_size, (int32_t)item_size);
            pool_id = -EINVAL;
            break;
        }

        pool_id = _alloc_mem_pool(&memPool_mgr);
        if (pool_id < 0) {
            pool_id = -ENOMEM;
            dms_printk(LOG_LVL_WARN, "DMSC WARN fail alloc pool for %s\n", pool_nm);
            break;
        }

        ret = _setup_mempool_item(&memPool_mgr.arr_mem_pool[pool_id],
                pool_nm, pool_size, item_size, is_mutex);
        if (ret) {
            pool_id = -ENOMEM;
            break;
        }
    } while (0);

    dms_printk(LOG_LVL_INFO, "DMSC INFO register mem pool %s %d %d\n",
            pool_nm, pool_size, (int32_t)item_size);
    return pool_id;
}

/*
 * _free_lmem_list: free pool's memory chunk
 * @memlist: memory pool to be handled
 *
 * Return: number of memory chunks be freed
 */
static int32_t _free_mem_list (struct list_head *memlist)
{
    struct list_head *cur, *next;
    int32_t free_cnt;

    free_cnt = 0;
    list_for_each_safe (cur, next, memlist) {
        if (unlikely(IS_ERR_OR_NULL(cur))) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN mem buffer list broken\n");
            DMS_WARN_ON(true);
            break;
        }

        list_del(cur);
        kfree(cur);
        free_cnt++;
    }

    return free_cnt;
}

/*
 * _free_lmem_list: free pool's memory chunk which chunk size 128K (max size kmalloc allow)
 * @memlist: memory pool to be handled
 *
 * Return: number of memory chunks be freed
 */
static int32_t _free_lmem_list (struct list_head *memlist)
{
    data_mem_item_t *cur, *next;
    int32_t free_cnt;

    free_cnt = 0;
    list_for_each_entry_safe (cur, next, memlist, list) {
        if (unlikely(IS_ERR_OR_NULL(cur))) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN mem buffer list broken\n");
            DMS_WARN_ON(true);
            break;
        }

        list_del(&cur->list);
        kfree(cur->data_ptr);
        kfree(cur);
        free_cnt++;
    }

    return free_cnt;
}

/*
 * deregister_mem_pool: deregister a memory pool
 * @pool_id: memory pool id get from register_mem_pool
 *
 * Return: 0: deregister memory pool success
 *        ~0: deregister memory pool fail
 */
int32_t deregister_mem_pool (int32_t pool_id)
{
    mem_pool_t *mmpool_item;
    int32_t ret, cnt_item_free;

    ret = 0;

    mmpool_item = _get_mem_pool(pool_id);
    if (IS_ERR_OR_NULL(mmpool_item)) {
        return -EINVAL;
    }

    dms_printk(LOG_LVL_INFO, "DMSC INFO deregister mem pool %s %d %d\n",
            mmpool_item->pool_name, mmpool_item->pool_size,
            (int32_t)mmpool_item->item_size);

    mmpool_item->is_pool_working = false;

    if (mmpool_item->is_lmem) {
        cnt_item_free = _free_lmem_list(&mmpool_item->free_list);
    } else {
        cnt_item_free = _free_mem_list(&mmpool_item->free_list);
    }
    if (cnt_item_free != mmpool_item->pool_size) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN free cnt(%d) != mem_pool->pool_size(%d)\n",
                cnt_item_free, mmpool_item->pool_size);
        DMS_WARN_ON(true);

        ret = -EPERM;
    }

    if (waitqueue_active(&mmpool_item->mem_res_waitq)) {
        wake_up_interruptible_all(&mmpool_item->mem_res_waitq);
    }

    return ret;
}

/*
 * init_mempool_manager: init a memory pool.
 */
static void _init_mempool_item (mem_pool_t *mmpool_item)
{
    mmpool_item->is_pool_working = false;

    memset(mmpool_item->pool_name, '\0', DISCOC_MEM_POOL_NM_LEN);
    mmpool_item->pool_size = 0;
    mmpool_item->pool_max_size = 0;
    atomic_set(&mmpool_item->num_free_items, 0);
    INIT_LIST_HEAD(&mmpool_item->free_list);
    INIT_LIST_HEAD(&mmpool_item->used_list);
    mmpool_item->item_size = 0;
    mmpool_item->is_mutex_lock = true;
    mmpool_item->is_lmem = false;
    init_waitqueue_head(&mmpool_item->mem_res_waitq);
}

/*
 * init_mempool_manager: init memory pool manager.
 *
 * Return: 0: init success
 *        ~0: init fail
 */
int32_t init_mempool_manager (void)
{
    int32_t i, ret;

    ret = 0;

    dms_printk(LOG_LVL_INFO, "DMSC INFO init mempool manager\n");

    for (i = 0; i < MAX_DISCOC_MEM_POOL; i++) {
        _init_mempool_item(&memPool_mgr.arr_mem_pool[i]);
    }

    atomic_set(&memPool_mgr.pool_ID_generator, -1);
    atomic_set(&memPool_mgr.cnt_mm_misc, 0);
    memPool_mgr.allc_wait_warn_time = MEM_WAIT_WARN_TIME*HZ;

    dms_printk(LOG_LVL_INFO, "DMSC INFO init mempool manager DONE ret %d\n", ret);

    return ret;
}

/*
 * rel_mempool_manager: release memory pool manager.
 *     1. try to release all pre-alloc memory chunk in memory pool
 *     2. check whether any memory chunks be holded by other component
 *     3. check whether any memory chunks that get from disco_mem_alloc hold
 *        by other component but not release
 *
 * Return: 0: release success
 *        ~0: memory leak or free memory twice
 */
int32_t rel_mempool_manager (void)
{
    mem_pool_t *mmpool_item;
    int32_t i, ret_tmp, ret;

    dms_printk(LOG_LVL_INFO, "DMSC INFO release mempool manager\n");

    ret = 0;
    for (i = 0; i < MAX_DISCOC_MEM_POOL; i++) {
        mmpool_item = _get_mem_pool(i);
        if (mmpool_item->is_pool_working) {
            ret_tmp = deregister_mem_pool(i);
            if (ret_tmp) {
                ret = ret_tmp;
            }
        }
    }

    if (atomic_read(&memPool_mgr.cnt_mm_misc) != 0) {
        dms_printk(LOG_LVL_ERR,
                "DMSC ERR mem leak cnt of alloc %d (!= 0)\n",
                atomic_read(&memPool_mgr.cnt_mm_misc));
        DMS_WARN_ON(true);
    }

    dms_printk(LOG_LVL_INFO, "DMSC INFO release mempool manager DONE ret %d\n", ret);

    return ret;
}
