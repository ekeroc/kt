/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_mem_manager_private.h
 *
 */

#ifndef COMMON_DISCOC_MEM_MANAGER_PRIVATE_H_
#define COMMON_DISCOC_MEM_MANAGER_PRIVATE_H_

#define MM_HEADER_SIZE (sizeof(struct list_head))

#define MEM_WAIT_WARN_TIME  60 //if waiting for mem take long time, generate a message every 60 seconds

#define MAX_DISCOC_MEM_POOL 128
#define DISCOC_MEM_POOL_NM_LEN  64
typedef struct discoC_mem_pool {
    int8_t pool_name[DISCOC_MEM_POOL_NM_LEN];  //name of memory pool
    uint32_t pool_size;                        //number of memory chunks in pool
    uint32_t pool_max_size;                    //max number of memory chunks in pool
    atomic_t num_free_items;                   //number of memory chunks not be allocated to user
    struct list_head free_list;                //pool to hold free memory chunks
    struct list_head used_list;                //pool to hold memory chunk allocated to user
    size_t item_size;                          //size of memory chunk
    bool is_mutex_lock;                        //which type of lock to protect pool
    bool is_lmem;                              //whether memory chunk size = 128K
    union {
        struct mutex mlock;
        spinlock_t splock;
    } pool_lock;                               //lock to protect pool (multiple threads access pool)
    wait_queue_head_t mem_res_waitq;           //Linux waitqueue for blocking user threads when no free memory chunks
    bool is_pool_working;                      //whether pool is working
} mem_pool_t;

typedef struct discoC_mempool_manager {
    mem_pool_t arr_mem_pool[MAX_DISCOC_MEM_POOL]; //memory pool array
    atomic_t pool_ID_generator;                   //atomic counter for generating pool ID
    int32_t allc_wait_warn_time;                  //when user's thread block at memory alloc function till time expire, show message and wait again
    atomic_t cnt_mm_misc;                         //counter about how many memory be allocated from discoC_mem_alloc but not yet free
} mem_pool_mgr_t;

mem_pool_mgr_t memPool_mgr;

#endif /* COMMON_DISCOC_MEM_MANAGER_PRIVATE_H_ */
