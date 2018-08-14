/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * housekeeping_private.h
 *
 *  Created on: 2013/10/11
 *      Author: bennett
 */


#ifndef _HOUSEKEEPING_PRIVATE_H_
#define _HOUSEKEEPING_PRIVATE_H_

typedef struct _dmsc_task {
    uint16_t task_type;
    struct list_head list_hkq;
    uint64_t * task_data;
} dmsc_task_t;

static struct list_head hk_task_queue;
static atomic_t num_hk_tasks;
static spinlock_t hk_task_qlock;
static wait_queue_head_t hk_task_waitq;

int8_t memInfo_buffer[PAGE_SIZE];
volatile bool dmsc_hk_working;

#define PAGECACHE_FN   "/proc/sys/vm/pagecache"
#define DROP_CACHE_FN   "/proc/sys/vm/drop_caches"
#define MEMINFO_FN  "/proc/meminfo"

#define DEFAULT_MACHINE_TOTAL_MEM   48*1024*1024
#define CLOUDOS_DOM0_RESERV 12*1024*1024
#define DIRTY_RATIO 5

#define mInfo_Total "MemTotal"
#define mInfo_Free "MemFree"
#define mInfo_Buffers "Buffers"
#define mInfo_Cached "Cached"
#define mInfo_Dirty "Dirty"

int64_t total_phyMemKB;
int64_t dirty_reserved_KB;
//int64_t minFree_KB;

#endif /* _HOUSEKEEPING_PRIVATE_H_ */


