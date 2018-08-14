/*
 *
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * Test_thread_manager.c
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include "common_util.h"
#include "Test_thread_manager_private.h"
#include "discoC_utest_export.h"
#include "thread_manager.h"
#include "Test_thread_manager.h"
#include "Test_thread_manager_threads.h"
#include "../config/dmsc_config.h"

#define P_UTEST_FAIL(func, desc) printk("%s%s fail: %s\n",error_msg, func, desc); \
                                    return ret;
#define _BUFFER_SIZE 1024*4

atomic_t discoC_err_cnt;

static int8_t *utest_nm = "test_thread_manager";
static int8_t *error_msg = "UTest test_thread_manager: ";


static int32_t normal_test (void) {
    int32_t ret = -1;
    dmsc_thread_t *th1, *th2;
    int8_t *buffer;
    int i;

    num_run_test++;

    // Allocate buffer for large message
    buffer = kmalloc(_BUFFER_SIZE, GFP_KERNEL);
    if (IS_ERR_OR_NULL(buffer)) {
        P_UTEST_FAIL("normal_test", "kmalloc for buffer fail");
    }


    init_thread_manager();

    th1 = create_worker_thread("th1", NULL, (void *) 1, thd_func);

    if (IS_ERR_OR_NULL(th1)) {
        P_UTEST_FAIL("normal_test", "th1 create fail\n");
    }

    th2 = create_sk_rx_thread("th2", (void *) 1, thd_func);

    if (IS_ERR_OR_NULL(th2)) {
        P_UTEST_FAIL("normal_test", "th2 create fail\n");
    }

    // Cover all type of thread
    add_thread_performance_data(THD_T_Main, THD_SubT_Main_Read,
        jiffies , jiffies+1,10);

    add_thread_performance_data(THD_T_Main, THD_SubT_Main_Write,
        jiffies , jiffies+1,10);
    add_thread_performance_data(THD_T_NNReq_Worker, THD_SubT_NNReq_CH,
        jiffies , jiffies+1,10);
    add_thread_performance_data(THD_T_NNReq_Worker, THD_SubT_NNReq_CM,
        jiffies , jiffies+1,10);
    add_thread_performance_data(THD_T_NNAck_Receivor, -1,
        jiffies , jiffies+1,10);
    add_thread_performance_data(THD_T_DNReq_Worker, THD_SubT_DNReq_Read,
        jiffies , jiffies+1,10);
    add_thread_performance_data(THD_T_DNReq_Worker, THD_SubT_DNReq_Write,
        jiffies , jiffies+1,10);
    add_thread_performance_data(THD_T_DNAck_Receivor, THD_SubT_DNReq_Read,
        jiffies , jiffies+1,10);
    add_thread_performance_data(THD_T_DNAck_Receivor, THD_SubT_DNReq_Write,
        jiffies , jiffies+1,10);
    add_thread_performance_data(THD_T_DNAck_Handler, THD_SubT_DNReq_Read,
        jiffies , jiffies+1,10);
    add_thread_performance_data(THD_T_DNAck_Handler, THD_SubT_DNReq_Write,
        jiffies , jiffies+1,10);
    add_thread_performance_data(THD_T_NNCIReport_Worker, -1,
        jiffies , jiffies+1,10);

    // Sample data for a cycle
    for (i = 0; i <= 10000; i++) {
        add_thread_performance_data(THD_T_Main, THD_SubT_Main_Read,
            jiffies , jiffies+1,10);
    }

    show_thread_info(buffer, _BUFFER_SIZE);

    /*
     * Note: The buffer is too big to show all in printk()
     */
    printk("%s\n", buffer);


    ret = stop_worker_thread(th1);
    if (ret != 0) {
        P_UTEST_FAIL("normal_test", "stop th1 fail");
    }

    stop_sk_rx_thread(th2);

    // Release resource
    release_thread_manager();
    clean_thread_performance_data();
    kfree(buffer);

#ifndef DMSC_USER_DAEMON
    release_dmsc_config();
#endif

    num_pass_test++;

    return 0;
}

static int32_t error_test (void) {

    int32_t ret = -1;
    dmsc_thread_t *th1, *th2;

    num_run_test++;



    // Initialize correctly
    init_thread_manager();

    th1 = create_worker_thread(NULL, NULL, (void *) 1, NULL);
    if (!IS_ERR_OR_NULL(th1)) {
        P_UTEST_FAIL("error_test", "create_worker_thread invalid detect fail");
    }


    th2 = create_sk_rx_thread(NULL, (void *) 1, NULL);
    if (!IS_ERR_OR_NULL(th2)) {
        P_UTEST_FAIL("error_test", "create_sk_rx_thread invalid detect fail");
    }

    ret = stop_worker_thread(NULL);
    if (ret == 0) {
        P_UTEST_FAIL("error_test", "stop_worker_thread invalid detect fail");
    }


    // TODO add_thread_performance_data show_thread_info

    // Release resource
    release_thread_manager();

    num_pass_test++;

    return 0;
}



static int32_t reset_my_test (void)
{
    int32_t ret;

    ret = 0;

    printk("UTest %s: reset test environment\n", utest_nm);
    //Count manually
    num_total_test = 2;
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
    error_test();
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

    *total_test = num_total_test;
    *run_test = num_run_test;
    *pass_test = num_pass_test;

    return ret;
}

int32_t init_test_thread_manager (void)
{
    reset_my_test();


    // Initialize dmsc config
#ifdef DMSC_USER_DAEMON
    P_UTEST_FAIL("Initialization", "Unable to init dmsc_config");
#else
    init_dmsc_config(0);
    //show_configuration(buffer, _BUFFER_SIZE);
    //printk("%s\n", buffer);
#endif

    return register_utest(utest_nm, reset_my_test, config_my_test,
            run_my_test, cancel_my_test, get_my_test_result);
}

void rel_test_thread_manager (void)
{
    deregister_utest(utest_nm);
}
