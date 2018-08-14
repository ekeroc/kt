/*
 *
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * Test_discoC_mem_manager.c
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include "../common_util.h"
#include "../discoC_mem_manager.h"
#include "Test_discoC_mem_manager_private.h"
#include "discoC_utest_export.h"

atomic_t discoC_err_cnt;

#if 0
static void Test_ccma_malloc (void) {
    char *buff;

    buff = ccma_malloc(sizeof(char)*100, GFP_NOWAIT);

    WARN_ON(get_mem_usage_cnt(MEM_Buf_MISC) != 1);

    printk("dmsc current using cnt %d after alloc 1 item\n",
            get_mem_usage_cnt(MEM_Buf_MISC));

    ccma_free(buff);
    WARN_ON(get_mem_usage_cnt(MEM_Buf_MISC) != 0);
    printk("dmsc current using cnt %d after free 1 item\n",
                get_mem_usage_cnt(MEM_Buf_MISC));
}
#endif

int32_t my_test1 (void) {
    int32_t ret;

    num_run_test++;
    do {
        if ((ret = init_mempool_manager())) {
            printk("DMSC UTest fail init mempool manager\n");
            break;
        }

        if ((ret = rel_mempool_manager())) {
            printk("DMSC UTest fail rel mempool manager\n");
            break;
        }

        num_pass_test++;
    } while (0);

    return 0;
}

static int32_t reset_my_test (void)
{
    int32_t ret;

    ret = 0;

    printk("UTest discoC_mem_manager: reset test environment\n");
    num_total_test = 1;
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

    printk("UTest discoC_mem_manager: configure test with %s\n", cfg_str);

    return ret;
}

static void _run_all_test (void)
{
    my_test1();
}

/*
 * NOTE: unit test developer can create threads to execute unit test
 *       but remember return test execution status in get_my_test_result
 */
static int32_t run_my_test (int8_t *test_nm)
{
    int32_t ret;

    ret = 0;

    printk("UTest discoC_mem_manager: run test function %s\n", test_nm);

    if (strcmp(test_nm, "all") == 0) {
        _run_all_test();
    } else if (strcmp(test_nm, "test1") == 0) {
        my_test1();
    } else {
        printk("UTest discoC_mem_manager: invalid test function %s\n", test_nm);
        ret = -EINVAL;
    }

    return ret;
}


static int32_t cancel_my_test (int8_t *test_nm)
{
    int32_t ret;

    ret = 0;

    printk("UTest discoC_mem_manager: cancel test function %s\n", test_nm);

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

int32_t init_test_discoC_mem_mgr (void)
{
    reset_my_test();

    return register_utest("test_discoC_mem_mgr", reset_my_test, config_my_test,
            run_my_test, cancel_my_test, get_my_test_result);
}

void rel_test_discoC_mem_mgr (void)
{
    deregister_utest("test_discoC_mem_mgr");
}
