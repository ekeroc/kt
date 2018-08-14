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
#include <linux/slab.h>
#include "discoC_sys_def.h"
#include "common_util.h"
#include "discoC_mem_manager.h"
#include "Test_discoC_mem_manager_private.h"
#include "discoC_utest_export.h"

// Use for different test function error msg
#define PRINT_UTEST_FAIL(s) printk("%s%s\n",error_msg,s);

atomic_t discoC_err_cnt;

static int8_t *utest_nm = "test_discoC_mem_mgr";
static int8_t *log_nm = "discoC_mem_manager";
static int8_t *error_msg = "UTest discoC_mem_manager: fail at ";

static int32_t normal_test (void) {
    void *mem1, *mem2;
    int32_t ret;
    int32_t id1, id2;

    num_run_test++;

    if ((ret = init_mempool_manager())) {
        goto ENORNAL;
    }

    // Pool 1
    if ((id1 = register_mem_pool("pool1", 1, 20, 0)) < 0) {
        goto ENORNAL;
    }
    if (!(mem1 = mem_pool_alloc(id1))) {
        goto ENORNAL;
    }
    mem_pool_free(id1, mem1);

    // Pool 2
    if ((id2 = register_mem_pool("pool2", 3, 10, 1)) < 0) {
        goto ENORNAL;
    }

    if (!(mem2 = mem_pool_alloc(id2))) {
        goto ENORNAL;
    }
    mem_pool_free(id2, mem2);

    if ((ret = rel_mempool_manager())) {
        goto ENORNAL;
    }

    num_pass_test++;

    return 0;

ENORNAL:
    PRINT_UTEST_FAIL("normal_test");
    return -1;
}

static int32_t error_test (void) {
    int32_t ret;
    int32_t id1;
    void *mem1, *mem2;
    struct list_head *list_item;

    num_run_test++;

    // Correctly initialization 
    if ((ret = init_mempool_manager())) {
        goto EERROR;
    }
    if ((id1 = register_mem_pool("Pool1", 3, 20, 0)) < 0) {
        goto EERROR;
    }
    if(!(mem1 = mem_pool_alloc(id1))) {
        goto EERROR;
    }

    // Create error
    if (register_mem_pool(NULL, 1, 20, 0) >= 0 ) { //-EINVAL) {
        goto EERROR;
    }

    if ((ret = register_mem_pool("pool2", 0, 20, 0)) != -EINVAL) {
        goto EERROR;
    }

    if ((ret = register_mem_pool("pool2", 30, 0, 0)) != -EINVAL) {
        goto EERROR;
    }

    // Allocate item error
    if ((mem2 = mem_pool_alloc(-1))) {
        goto EERROR;
    }

    // Free item error
    mem_pool_free(-1, mem1);

    /* FIXME
     * Crash when free NULL memory
     * Check whether the item belongs to the pool

    mem_pool_free(id1, NULL);
    */

    // Deregister error
    if (deregister_mem_pool(-1) != -EINVAL) {
        goto EERROR;
    }

    /*
     * Note
     * This could have memory leak
     */
    if (deregister_mem_pool(id1) != -EPERM) {
        goto EERROR;
    }
    //NOTE: we know the exact implementation of disco_mem_manager
    //      free memory to prevent from memory leak after testing
    list_item = (struct list_head *)((uint8_t *)mem1 - (sizeof(struct list_head)));
    list_del(list_item);
    kfree(list_item);

    /* Close correctly
    mem_pool_free(id1, mem1);

    if((ret = deregister_mem_pool(id1))) {
        goto EERROR;
    }
    */

    if ((ret = rel_mempool_manager())) {
        goto EERROR;
    }

    num_pass_test++;

    return 0;

EERROR:
    PRINT_UTEST_FAIL("error_test");
    return -1;
}

static int32_t reset_my_test (void)
{
    int32_t ret;

    ret = 0;

    printk("UTest %s: reset test environment\n", log_nm);

    // Need to count manually
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

    printk("UTest %s: configure test with %s\n", log_nm, cfg_str);

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

    printk("UTest %s: run test function %s\n", log_nm, test_nm);

    if (strcmp(test_nm, "all") == 0) {
        _run_all_test();
    } else if (strcmp(test_nm, "normal") == 0) {
        normal_test();
    } else {
        printk("UTest %s: invalid test function %s\n", log_nm, test_nm);
        ret = -EINVAL;
    }

    return ret;
}


static int32_t cancel_my_test (int8_t *test_nm)
{
    int32_t ret;

    ret = 0;

    printk("UTest %s: cancel test function %s\n", log_nm, test_nm);

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

    return register_utest(utest_nm, reset_my_test, config_my_test,
            run_my_test, cancel_my_test, get_my_test_result);
}

void rel_test_discoC_mem_mgr (void)
{
    deregister_utest(utest_nm);
}
