/*
 * Copyright (C) 2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * volume_manager_private.h
 *
 */

#ifndef _VOLUME_MANAGER_PRIVATE_H_
#define _VOLUME_MANAGER_PRIVATE_H_

#define VOL_HASH_NUM  101

// DEV_DISK_MINORS defined how many followed number from DEV_MINOR_START this disk can use for partition.
#define DEV_MINOR_START    1
#define DEV_DISK_MINORS    16

#define VOL_IOR_RETRY_WQ_NAME   "ioreq_retry"

#define FLUSH_IO_CHECK_TIME_PERIOD 1000

#define MAX_STATE   8
#define MAX_OP      8

//static uint32_t vol_state_transition[MAX_STATE][MAX_OP] = {
//        {1, 1, 2, 3, 4, 5, 6, 7},
//        {0, 2, 2, 4, 4, 5, 6, 7},
//        {0, 1, 1, 3, 3, 6, 6, 7},
//        {0, 3, 4, 3, 4, 5, 6, 7},
//        {0, 1, 2, 1, 2, 5, 6, 7},
//        {0, 1, 2, 1, 2, 5, 6, 7},
//        {0, 6, 5, 6, 5, 5, 6, 7},
//        {0, 1, 2, 3, 4, 5, 7, 7}
//};

static uint32_t vol_state_transition[MAX_STATE][MAX_OP] = {
        {1, 0, 0, 0, 0, 0, 0, 0},
        {1, 2, 1, 3, 1, 1, 6, 1},
        {2, 2, 1, 4, 2, 2, 5, 2},
        {3, 4, 3, 3, 1, 1, 6, 3},
        {4, 4, 3, 4, 2, 2, 5, 4},
        {5, 5, 6, 5, 5, 5, 5, 5},
        {6, 6, 6, 6, 6, 6, 6, 7},
        {7, 7, 7, 7, 7, 7, 7, 7}
};

struct volume_device;

static struct rw_semaphore vol_table_lock[VOL_HASH_NUM];
static struct list_head vol_table[VOL_HASH_NUM]; //TODO Change to hlist
static uint32_t max_attach_volumes;

static struct list_head mnum_free_list;
static struct list_head mnum_using_list;
static struct mutex mnum_list_lock;

static int32_t vol_major_num;

static atomic_t num_of_vol_attached;

static struct volume_gd_param default_vol_gd;

static atomic_t cnt_attach_ongoing;

static bool is_vol_ro(struct volume_device *vol);
static bool is_vol_detachable(struct volume_device *vol);
static bool is_vol_detaching (struct volume_device *vol);
#endif /* _VOLUME_MANAGER_PRIVATE_H_ */
