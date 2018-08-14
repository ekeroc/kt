/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_comm_main.c
 *
 * initialize all common components
 * release all common components
 *
 */

#ifndef DISCOC_COMM_MAIN_H_
#define DISCOC_COMM_MAIN_H_

extern void set_common_loglvl(log_lvl_t mylevel);
extern int32_t init_discoC_common(int32_t num_of_ioreq);
extern void release_discoC_common(void);

#endif /* DISCOC_COMM_MAIN_H_ */
