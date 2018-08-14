/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * dms_client_mm_private.h
 *
 */
#ifndef _DMS_CLIENT_MM_PRIVATE_H_
#define _DMS_CLIENT_MM_PRIVATE_H_

#define DMSC_MM_HEADER_ID_SIZE  (sizeof(int8_t))
#define DMSC_MM_HEADER_LL_SIZE  (sizeof(struct list_head))
#define DMSC_MM_HEADER_SIZE (DMSC_MM_HEADER_ID_SIZE + DMSC_MM_HEADER_LL_SIZE)

#define MEM_WAIT_TIMEOUT    100
#define mpool_item_mark     0xff
#define mem_os_item_mark    0x0

typedef struct dmsc_mm_pool_type {
    uint32_t pool_size;
    uint32_t pool_max_size;
    atomic_t num_items_used;
    struct list_head free_list;
    struct list_head used_list;
    size_t item_size;
    bool is_mutex_lock;
    bool is_lmem;
    union {
        struct mutex mlock;
        spinlock_t splock;
    } pool_lock;
} dmsc_mm_pool_type_t;

wait_queue_head_t mem_resources_wq;

dmsc_mm_pool_type_t ioreq_pool;

uint32_t IORREQ_POOL_SIZE;
uint32_t Num_of_IO_Reqs_Congestion_On;

volatile bool dmsc_mm_working;

#endif /* _DMS_CLIENT_MM_PRIVATE_H_ */
