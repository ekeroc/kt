/*
 *
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * Test_cache.c
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/slab.h>
#include "common_util.h"
#include "Test_cache_private.h"
#include "discoC_utest_export.h"
#include "../config/dmsc_config.h"
#include "cache.h"
#include "Test_cache_pool.h"

atomic_t discoC_err_cnt;

static int8_t *utest_nm = "test_cache";

# define P_BASKET_INT(b, size) \
        for (i = 0; i < size; i++) { \
            if (b[i] != NULL) { \
                printk("data:%d blkid:%lld\n", *(int16_t*)(((struct tree_leaf*)b[i])->data), (((struct tree_leaf*)b[i])->blkid)); \
            }}

#if 1
static int32_t normal_test (void) {
    num_total_test += 4;

    num_run_test++;
    if (test_cache_pool_normal() == 0) {
        num_pass_test++;
    }

    num_run_test++;
    if (test_cache_pool_single_volMaxCap() == 0) {
        num_pass_test++;
    }

    num_run_test++;
    if (test_cache_pool_multiple_vol() == 0) {
        num_pass_test++;
    }

    num_run_test++;
    if (test_cache_pool_ranout() == 0) {
        num_pass_test++;
    }
    return 0;
}
#else
static int32_t normal_test (void) {
    int32_t ret, i;
    uint32_t cache_size, curr_cache_items, pool1;
    int16_t id_gen = 0;
    int16_t* data_ptr[10];
    void* basket[10];

    num_run_test++;

#ifdef ENABLE_PAYLOAD_CACHE_FOR_DEBUG
    printk(KERN_INFO "ENABLE_PAYLOAD_CACHE_FOR_DEBUG is defined");
#endif

    ret = cacheMgr_init_manager(simple_free_fn);
    if ( ret < 0 ) {
        P_UTEST_FAIL("initialize cache manager fail");
        goto NORMAL_OUT;
    }

    // Create cpool with 4 cache item
    pool1 = id_gen++;
    ret = cacheMgr_create_pool(pool1, 4096 * 4);
    if (ret != 0) {
        P_UTEST_FAIL("Create new cache pool fail");
        goto NORMAL_OUT;
    }

    // Fill in data
    for (i = 0; i < 10; i++) {
        data_ptr[i] = kmalloc(sizeof(int16_t), GFP_KERNEL);
        *data_ptr[i] = i;
    }
    memset(basket, 0, 10*sizeof(void *));

    // Add item into cache pool
    // index:      0 , 1 , 2 ,3
    // cache item: 1 , 2 , 3 ,4
    ret = cache_update(pool1, 0, 4, (void **)data_ptr);
    if ( ret != 0 ) {
        P_UTEST_FAIL("Cache update1 fail");
        goto NORMAL_OUT;
    }

    // Make pool full and kick pld cache
    // index:      1 , 3 , 4
    // cache item: 2 , 5 , 6
    ret = cache_update(pool1, 3, 2, (void **)data_ptr+5);
    if ( ret != 0 ) {
        P_UTEST_FAIL("Cache update2 fail");
        goto NORMAL_OUT;
    }

    // Could only find index 1 ,3
    ret = cache_find (pool1, 0, 4, basket);
    if (ret != 2) {
        P_UTEST_FAIL("Cache find fail");
        goto NORMAL_OUT;
    }

    // Manually delete
    for (i = 0; i < 4; i++) {
        if (basket[i] != NULL) {
            tree_leaf_set_delete((struct tree_leaf *)basket[i]);
        }
    }

    // Release deleted cache
    ret = cache_put (pool1, 0, 4, basket);
    if (ret != 0) {
        P_UTEST_FAIL("Cache put fail");
        goto NORMAL_OUT;
    }

    // Should find only 1 cache
    ret = cache_find (pool1, 0, 10, basket);
    if (ret != 1) {
        P_UTEST_FAIL("Cache find fail");
   //     P_BASKET_INT(basket, 10)
        goto NORMAL_OUT;
    }

    cacheMgr_get_cachePool_size(pool1, &cache_size, &curr_cache_items);
    printk("cpool 0 size: %d curr: %d\n", cache_size, curr_cache_items);

    printk("Total number of leaf in cpool manager: %d\n",
            cacheMgr_get_num_treeleaf());

    ret =  cacheMgr_rel_pool(pool1);
    if ( ret != 0 ) {
        P_UTEST_FAIL("rm cpool fail");
        goto NORMAL_OUT_LAST;
    }

    cacheMgr_rel_manager();

    num_pass_test++;

    return 0;

NORMAL_OUT:

    ret =  cacheMgr_rel_pool(pool1);
    if ( ret != 0 ) {
        P_UTEST_FAIL("rm cpool fail");
    }

NORMAL_OUT_LAST:

    cacheMgr_rel_manager();
    return ret;
}
#endif

static int32_t reset_my_test (void)
{
    int32_t ret;

    ret = 0;

    printk("UTest %s: reset test environment\n", utest_nm);
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

    *total_test = num_total_test;
    *run_test = num_run_test;
    *pass_test = num_pass_test;

    return ret;
}

int32_t init_test_cache (void)
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

    // Change dms printk loglevel
    discoC_log_lvl = LOG_LVL_DEBUG;

    return register_utest(utest_nm, reset_my_test, config_my_test,
            run_my_test, cancel_my_test, get_my_test_result);
}

void rel_test_cache (void)
{
    deregister_utest(utest_nm);
}
