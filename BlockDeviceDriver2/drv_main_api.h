/*
 * DISCO Client Driver main function
 *
 * Copyright (C) 2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * drv_main_api.h
 *
 */

#ifndef _DRV_MAIN_API_H_
#define _DRV_MAIN_API_H_

extern int32_t init_drv_component(void);
extern int32_t rel_drv_component(void);

extern int32_t init_drv(int32_t run_mode);
extern void release_drv(void);

#endif /* DRV_MAIN_API_H_ */
