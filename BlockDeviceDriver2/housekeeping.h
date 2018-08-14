/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * housekeeping.h
 *
 */
#ifndef _HOUSEKEEPING_H_
#define _HOUSEKEEPING_H_

typedef enum {
    TASK_CONN_RETRY = 0,
    TASK_DN_CONN_BACK_REQ_RETRY,
    TASK_NN_CONN_BACK_REQ_RETRY,
    TASK_NN_OVW_CONN_BACK_REQ_RETRY,
    TASK_NN_CONN_RESET,
    TASK_DN_CONN_RESET,
} task_type_t;


extern bool add_hk_task(task_type_t task_type, uint64_t * task_data);
extern void init_housekeeping_res(void);
extern void rel_housekeeping_res(void);

extern void initial_cleancache_thread(void);
#endif	/* _HOUSEKEEPING_H_ */
