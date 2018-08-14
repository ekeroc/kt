/*
 *
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * Test_metadata_manager.c
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/slab.h>
#include "common_util.h"
#include "Test_metadata_manager_private.h"
#include "discoC_utest_export.h"
#include "../config/dmsc_config.h"
#include "Test_space_chunk_cache.h"

atomic_t discoC_err_cnt;

static int8_t *utest_nm = "test_metadata_manager";

static int32_t normal_test (void) {
    sm_chunkcache_ftest();
    return 0;
}

static int32_t reset_my_test (void)
{
    int32_t ret;

    ret = 0;

    printk("UTest %s: reset test environment\n", utest_nm);

    sm_chunkcache_reset_test();
    //Count manually
    num_total_test = 0;
    num_run_test = 0;
    num_pass_test = 0;


    return ret;
}

/*
 * NTOE: need parse config string
 */
static int32_t config_my_test (int8_t *cfg_str)
{
    int32_t ret;

    ret = 0;

    printk("UTest %s: configure test with %s\n", utest_nm, cfg_str);

    return ret;
}

static void _run_all_test (void)
{
    normal_test();
    //error_test();
}

/*
 * NOTE: unit test developer can create threads to execute unit test
 *       but remember return test execution status in get_my_test_result
 */
static int32_t run_my_test (int8_t *test_nm)
{
    int32_t ret;

    ret = 0;

    printk("UTest %s: run test function %s\n", utest_nm, test_nm);

    if (strcmp(test_nm, "all") == 0) {
        _run_all_test();
    } else if (strcmp(test_nm, "normal") == 0) {
        normal_test();
    } else {
        printk("UTest %s: invalid test function %s\n", utest_nm, test_nm);
        ret = -EINVAL;
    }

    return ret;
}

static int32_t cancel_my_test (int8_t *test_nm)
{
    int32_t ret;

    ret = 0;

    printk("UTest %s: cancel test function %s\n", utest_nm, test_nm);

    return ret;
}

//NOTE: return value defined in discoC_utest_stat_t to indicate whether test is running
static int32_t get_my_test_result (int32_t *total_test, int32_t *run_test, int32_t *pass_test)
{
    int32_t ret;

    ret = 0;

    sm_chunkcache_alltest_get_result(&num_total_test, &num_run_test, &num_pass_test);
    *total_test = num_total_test;
    *run_test = num_run_test;
    *pass_test = num_pass_test;

    return ret;
}

int32_t init_test_metadata_mgr (void)
{
    reset_my_test();

    sm_chunkcache_init_test();

    // Change dms printk loglevel
    discoC_log_lvl = LOG_LVL_DEBUG;

    return register_utest(utest_nm, reset_my_test, config_my_test,
            run_my_test, cancel_my_test, get_my_test_result);
}

void rel_test_metadata_mgr (void)
{
    sm_chunkcache_rel_test();
    deregister_utest(utest_nm);
}
