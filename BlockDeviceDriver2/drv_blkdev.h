/*
 * DISCO Client Driver main function
 *
 * Copyright (C) 2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * drv_blkdev.c
 *
 */

#ifndef _DRV_BLKDEV_H_
#define _DRV_BLKDEV_H_

extern void dms_io_request_fn(struct request_queue *q);

extern int32_t init_blkdev(discoC_drv_t *mydrv);
extern void rel_blkdev(discoC_drv_t *mydrv);

#endif /* _DRV_BLKDEV_H_ */
