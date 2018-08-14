/*
 *
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * Test_cache_common.h
 *
 * Provide common function for testing cache component
 */


#ifndef DISCOC_UTEST_UNIT_TESTS_COMMON_CACHE_TEST_CACHE_COMMON_H_
#define DISCOC_UTEST_UNIT_TESTS_COMMON_CACHE_TEST_CACHE_COMMON_H_

#define P_UTEST_FAIL(desc) \
        printk(KERN_WARNING "UTest cache %s L%d fail: %s\n", \
        __func__, __LINE__, desc);

extern void simple_free_fn(void *metadata_item);

#endif /* DISCOC_UTEST_UNIT_TESTS_COMMON_CACHE_TEST_CACHE_COMMON_H_ */
