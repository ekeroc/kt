/*
 * DISCO Client Driver main function
 *
 * Copyright (C) 2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * drv_main.h
 *
 */

#ifndef _DRV_MAIN_H_
#define _DRV_MAIN_H_

typedef struct dmsc_driver {
    int8_t chrdev_name[64];
    int8_t blkdev_name_prefix[64];
    int32_t chrdev_major_num;
    int32_t blkdev_major_num;
    drv_state_type_t drv_state;
    atomic_t drv_busy_cnt;
    struct mutex drv_lock;
} discoC_drv_t;

extern discoC_drv_t discoC_drv;

extern struct block_device_operations ccma_volume_fops;

#endif /* DRV_MAIN_H_ */
