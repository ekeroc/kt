/*
 *
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_utest_manager_private.h
 *
 */

#ifndef DISCOC_UTEST_MANAGER_PRIVATE_H_
#define DISCOC_UTEST_MANAGER_PRIVATE_H_

typedef struct discoC_utest_manager {
    struct list_head utest_pool;
    uint32_t num_utest_module;
    struct rw_semaphore pool_lock;
    atomic_t utest_ID_gen;
} utest_mgr_t;

utest_mgr_t discoC_utestMgr;

#endif /* DISCOC_UTEST_MANAGER_PRIVATE_H_ */
