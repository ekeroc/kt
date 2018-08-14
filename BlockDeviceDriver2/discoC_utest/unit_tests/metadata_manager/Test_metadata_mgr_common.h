/*
 *
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * Test_metadata_mgr_common.h
 *
 * Provide common function for testing metadata manager component
 */

#ifndef DISCOC_UTEST_UNIT_TESTS_METADATA_MANAGER_TEST_METADATA_MGR_COMMON_H_
#define DISCOC_UTEST_UNIT_TESTS_METADATA_MANAGER_TEST_METADATA_MGR_COMMON_H_

#define P_MDMGR_UTEST_FAIL(desc) \
        printk(KERN_WARNING "UTest metadata manager %s L%d fail: %s\n", \
        __func__, __LINE__, desc);

#endif /* DISCOC_UTEST_UNIT_TESTS_METADATA_MANAGER_TEST_METADATA_MGR_COMMON_H_ */
