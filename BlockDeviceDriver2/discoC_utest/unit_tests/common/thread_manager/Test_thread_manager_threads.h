/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * Test_thread_manager_threads.h
 *
 */

#ifndef COMMON_UNIT_TEST_TEST_THREAD_MANAGER_THREADS_H_
#define COMMON_UNIT_TEST_TEST_THREAD_MANAGER_THREADS_H_


int32_t thd_func(void *data)
{
    dmsc_thread_t *dmsc_t = (dmsc_thread_t*) data;

    printk("Thread %s runs\n", dmsc_t->t_name);
    return 0;
}

#endif  /* COMMON_UNIT_TEST_TEST_THREAD_MANAGER_THREADS_H_ */
