/*
 *
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * Test_test_target.c
 *
 */
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/jiffies.h>
#include "discoC_utest_export.h"
#include "Test_test_target_private.h"
#include "Test_test_target.h"
#include "test_target.h"

//Normal test
static void my_test1 (void)
{
    num_run_test++;
    if (my_fun_A(50) == 0) {
        num_pass_test++;
    }
}

//Error detection test
static void my_test2 (void)
{
    int32_t ret;

    num_run_test++;
    ret = my_fun_A(0);
    if (ret == 0) {
        return;
    }

    if (my_fun_A(101) == 0) {
        return;
    }

    num_pass_test++;
}

//boundary test
static void my_test3 (void)
{
    int32_t ret;

    num_run_test++;
    ret = my_fun_A(1);
    if (ret) {
        return;
    }

    if (my_fun_A(99)) {
        return;
    }

    num_pass_test++;
}

//stress test
static void my_test4 (void)
{
    int64_t t_s, t_e;
    int32_t i;

    num_run_test++;

    t_s = jiffies_64;
    for (i = 0; i < 10000; i++) {
        my_fun_A(1);
    }
    t_e = jiffies_64;

    if (jiffies_to_msecs(t_e - t_s) < 1) {
        num_pass_test++;
    }
}

static void _run_all_test (void)
{
    my_test1();
    my_test2();
    my_test3();
    my_test4();
}

static int32_t reset_my_test (void)
{
    int32_t ret;

    ret = 0;

    printk("Test test target: reset test environment\n");
    num_total_test = 4;
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

    printk("Test test target: configure test with %s\n", cfg_str);

    return ret;
}

/*
 * NOTE: unit test developer can create threads to execute unit test
 *       but remember return test execution status in get_my_test_result
 */
static int32_t run_my_test (int8_t *test_nm)
{
    int32_t ret;

    ret = 0;

    printk("Test test target: test function %s\n", test_nm);

    if (strcmp(test_nm, "all") == 0) {
        _run_all_test();
    } else if (strcmp(test_nm, "test1") == 0) {
        my_test1();
    } else if (strcmp(test_nm, "test2") == 0) {
        my_test2();
    } else if (strcmp(test_nm, "test3") == 0) {
        my_test3();
    } else if (strcmp(test_nm, "test4") == 0) {
        my_test4();
    } else {
        printk("Test test target: invalid test function %s\n", test_nm);
        ret = -EINVAL;
    }

    return ret;
}


static int32_t cancel_my_test (int8_t *test_nm)
{
    int32_t ret;

    ret = 0;

    printk("Test test target: cancel test function %s\n", test_nm);

    return ret;
}

//NOTE: return value defined in discoC_utest_stat_t to indicate whether test is running
static int32_t get_my_test_result (int32_t *total_test, int32_t *run_test, int32_t *pass_test)
{
    int32_t ret;

    ret = 0;

    *total_test = num_total_test;
    *run_test = num_run_test;
    *pass_test = num_pass_test;

    return ret;
}

int32_t init_Test_test_target (void)
{
    reset_my_test();

    return register_utest("test_test_target", reset_my_test, config_my_test,
            run_my_test, cancel_my_test, get_my_test_result);
}

void rel_Test_test_target (void)
{
    deregister_utest("test_test_target");
}
