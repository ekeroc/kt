/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * FlowControl_private.h
 */

#ifndef _FLOWCONTROL_PRIVATE_H_
#define _FLOWCONTROL_PRIVATE_H_

#define DEFAULT_NUM_REQ_IN_COUNT    10

#define DMS_RESOURCE_NOT_READY  -1
#define DMS_RESOURCE_NOT_AVAILABLE  -2

#define RESOURCES_STAT_DATA_COLLECTING  0
#define RESOURCES_STAT_DATA_READY       1

#define MIN_NN_MAX_REQUESTS_TIMEOUT 20  //unit seconds
#define MIN_NN_FIXED_RATE_TIMEOUT   1
#define NN_RATE_NO_LIMIT            -1

#define DEFAULT_NN_SLOW_START_RATE  128
#define DEFAULT_NN_RATE_AGE_TIME        300 //unit seconds
#define DEFAULT_NN_FIXED_RATE       512
#define DEFAULT_NN_MAX_REQUESTS     2048

#define DEFAULT_DN_SLOW_START_RATE  -1
#define DEFAULT_DN_RATE_AGE_TIME    -1
#define DEFAULT_DN_FIXED_RATE   -1
#define DEFAULT_DN_MAX_REQUESTS     -1

struct list_head fc_reslist;
//NOTE: we cannot change to mutex or semaphore since sk_rx_err_handle will call it
rwlock_t fcRes_qlock;
uint32_t g_res_id;
wait_queue_head_t max_reqs_wq;
atomic_t wait_flag;

#endif /* _FLOWCONTROL_PRIVATE_H_ */
