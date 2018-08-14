/*
 * DISCO Client Driver main function
 *
 * Copyright (C) 2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * drv_chrdev.c
 *
 */

#ifndef _DRV_CHRDEV_H_
#define _DRV_CHRDEV_H_

extern bool is_drv_running(bool gain_ref);
extern int32_t init_chrdev(discoC_drv_t *mydrv);
extern void rel_chrdev(discoC_drv_t *mydrv);

#endif /* DRV_CHRDEV_H_ */
