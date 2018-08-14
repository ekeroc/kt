/*
 *
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * Test_cache_pool.c
 *
 * Test operation of add/rm cacho pool
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include "cache.h"
#include "dmsc_config.h"
#include "Test_cache_common.h"

int32_t test_cache_pool_normal (void)
{
    uint64_t volCap;
    uint32_t pool_size, num_items;
    int32_t ret, vol1ID;

    ret = cacheMgr_init_manager(simple_free_fn);
    if (ret < 0) {
        P_UTEST_FAIL("initialize cache manager fail");
        return ret;
    }

    do {
        vol1ID = 100;
        cacheMgr_get_cachePool_size(vol1ID, &pool_size, &num_items);
        if (pool_size != 0 || num_items != 0) {
            P_UTEST_FAIL("initialize cache manager fail");
            ret = -1;
            break;
        }

        volCap = 1024 * 4096; //4M size
        ret = cacheMgr_create_pool(vol1ID, volCap);
        if (ret) {
            P_UTEST_FAIL("create cache pool fail");
            break;
        }

        cacheMgr_get_cachePool_size(vol1ID, &pool_size, &num_items);
        if (pool_size != 1024 || num_items != 0) {
            P_UTEST_FAIL("wrong pool size");
            ret = -1;
            cacheMgr_rel_pool(vol1ID);
            break;
        }

        if (cacheMgr_get_num_treeleaf() != 0) {
            cacheMgr_rel_pool(vol1ID);
            P_UTEST_FAIL("wrong number tree leaf");
            ret = -1;
            break;
        }

        cacheMgr_rel_pool(vol1ID);
    } while (0);

    cacheMgr_rel_manager();
    return ret;
}

int32_t test_cache_pool_single_volMaxCap (void)
{
    uint64_t volCap;
    uint32_t pool_size, num_items;
    int32_t ret, vol1ID;

    ret = init_dmsc_config(0);
    if (ret) {
        P_UTEST_FAIL("initialize config fail");
        return ret;
    }

    ret = cacheMgr_init_manager(simple_free_fn);
    if (ret < 0) {
        P_UTEST_FAIL("initialize cache manager fail");
        release_dmsc_config();
        return ret;
    }

    do {

        //LEGO: dms_client_config->max_meta_cache_sizes = 33554432
        //LEGO: dms_client_config->max_cap_cache_all_hit = 1073741824

        vol1ID = 100;

        volCap = (dms_client_config->max_meta_cache_sizes) * 4096; //4M size
        ret = cacheMgr_create_pool(vol1ID, volCap);
        if (ret) {
            P_UTEST_FAIL("create cache pool fail");
            break;
        }

        cacheMgr_get_cachePool_size(vol1ID, &pool_size, &num_items);
        if (pool_size != dms_client_config->max_meta_cache_sizes ||
                num_items != 0) {
            P_UTEST_FAIL("wrong pool size");
            ret = -1;
            cacheMgr_rel_pool(vol1ID);
            break;
        }

        if (cacheMgr_get_num_treeleaf() != 0) {
            cacheMgr_rel_pool(vol1ID);
            P_UTEST_FAIL("wrong number tree leaf");
            ret = -1;
            break;
        }

        cacheMgr_rel_pool(vol1ID);

        volCap = (dms_client_config->max_meta_cache_sizes + 1024) * 4096; //4M size
        ret = cacheMgr_create_pool(vol1ID, volCap);
        if (ret) {
            P_UTEST_FAIL("create cache pool fail");
            break;
        }

        cacheMgr_get_cachePool_size(vol1ID, &pool_size, &num_items);
        if (pool_size != dms_client_config->max_meta_cache_sizes || num_items != 0) {
            P_UTEST_FAIL("wrong pool size");
            ret = -1;
            cacheMgr_rel_pool(vol1ID);
            break;
        }

        if (cacheMgr_get_num_treeleaf() != 0) {
            cacheMgr_rel_pool(vol1ID);
            P_UTEST_FAIL("wrong number tree leaf");
            ret = -1;
            break;
        }

        cacheMgr_rel_pool(vol1ID);
    } while (0);

    cacheMgr_rel_manager();
    release_dmsc_config();
    return ret;
}

int32_t test_cache_pool_multiple_vol (void)
{
    uint64_t volCap, numCItems;
    uint32_t pool_size, num_items, expected_items;
    int32_t ret, numvol, i;

    ret = init_dmsc_config(0);
    if (ret) {
        P_UTEST_FAIL("initialize config fail");
        return ret;
    }

    ret = cacheMgr_init_manager(simple_free_fn);
    if (ret < 0) {
        P_UTEST_FAIL("initialize cache manager fail");
        release_dmsc_config();
        return ret;
    }

    do {
        //LEGO: dms_client_config->max_meta_cache_sizes = 33554432
        //LEGO: dms_client_config->max_cap_cache_all_hit = 1073741824

        /***** Test multiple volumes share all *****/
        numvol = 16;
        numCItems = dms_client_config->max_meta_cache_sizes / numvol;
        volCap = numCItems*4096;
        for (i = 1; i <= numvol; i++) {
            ret = cacheMgr_create_pool(i, volCap);
            if (ret) {
                P_UTEST_FAIL("create cache pool fail");
                break;
            }
        }

        for (i = 1; i <= numvol; i++) {
            cacheMgr_get_cachePool_size(i, &pool_size, &num_items);
            if (pool_size != numCItems || num_items != 0) {
                ret = -1;
                P_UTEST_FAIL("wrong pool size");
                break;
            }
        }

        for (i = 1; i <= numvol; i++) {
            cacheMgr_rel_pool(i);
        }

        ret = 0;

        /***** Test multiple volumes share base on capacity *****/
        dms_client_config->max_meta_cache_sizes = 33554432;
        volCap = (dms_client_config->max_meta_cache_sizes * 4096L);
        ret = cacheMgr_create_pool(1, volCap);
        if (ret) {
            P_UTEST_FAIL("create cache pool fail");
            break;
        }
        cacheMgr_get_cachePool_size(1, &pool_size, &num_items);
        if (pool_size != dms_client_config->max_meta_cache_sizes ||
                num_items != 0) {
            ret = -1;
            P_UTEST_FAIL("wrong pool size");
            break;
        }

        volCap = (dms_client_config->max_meta_cache_sizes * 4096L);
        ret = cacheMgr_create_pool(2, volCap);
        if (ret) {
            P_UTEST_FAIL("create cache pool fail");
            break;
        }
        cacheMgr_get_cachePool_size(1, &pool_size, &num_items);
        if (pool_size != (dms_client_config->max_meta_cache_sizes / 2) ||
                num_items != 0) {
            ret = -1;
            P_UTEST_FAIL("wrong pool size");
            break;
        }
        cacheMgr_get_cachePool_size(2, &pool_size, &num_items);
        if (pool_size != (dms_client_config->max_meta_cache_sizes / 2) ||
                num_items != 0) {
            ret = -1;
            P_UTEST_FAIL("wrong pool size");
            break;
        }

        cacheMgr_rel_pool(1);
        cacheMgr_rel_pool(2);

        ret = 0;

        /***** Test multiple volumes share with some are big volume some are small *****/
        dms_client_config->max_meta_cache_sizes = 33554432;
        volCap = (dms_client_config->max_meta_cache_sizes * 4096L);
        ret = cacheMgr_create_pool(1, volCap);
        if (ret) {
            P_UTEST_FAIL("create cache pool fail");
            break;
        }
        cacheMgr_get_cachePool_size(1, &pool_size, &num_items);
        if (pool_size != dms_client_config->max_meta_cache_sizes || num_items != 0) {
            ret = -1;
            P_UTEST_FAIL("wrong pool size");
            break;
        }

        volCap = dms_client_config->max_cap_cache_all_hit;
        ret = cacheMgr_create_pool(2, volCap);
        if (ret) {
            P_UTEST_FAIL("create cache pool fail");
            break;
        }
        expected_items = (dms_client_config->max_cap_cache_all_hit) / 4096;
        cacheMgr_get_cachePool_size(2, &pool_size, &num_items);
        if (pool_size != expected_items || num_items != 0) {
            ret = -1;
            P_UTEST_FAIL("wrong pool size");
            break;
        }

        expected_items = dms_client_config->max_meta_cache_sizes - expected_items;
        cacheMgr_get_cachePool_size(1, &pool_size, &num_items);
        if (pool_size != expected_items || num_items != 0) {
            ret = -1;
            P_UTEST_FAIL("wrong pool size");
            break;
        }

        cacheMgr_rel_pool(1);
        cacheMgr_rel_pool(2);

        ret = 0;
    } while (0);

    cacheMgr_rel_manager();
    release_dmsc_config();
    return ret;
}

int32_t test_cache_pool_ranout (void)
{
    uint64_t volCap;
    uint32_t pool_size, num_items, expected_items;
    int32_t ret, numvol, i, volhasmd;

    ret = init_dmsc_config(0);
    if (ret) {
        P_UTEST_FAIL("initialize config fail");
        return ret;
    }

    ret = cacheMgr_init_manager(simple_free_fn);
    if (ret < 0) {
        P_UTEST_FAIL("initialize cache manager fail");
        release_dmsc_config();
        return ret;
    }

    do {
        //LEGO: dms_client_config->max_meta_cache_sizes = 33554432
        //LEGO: dms_client_config->max_cap_cache_all_hit = 1073741824

        /***** Test multiple volumes share all *****/
        volhasmd = 10;
        dms_client_config->max_meta_cache_sizes =
                (dms_client_config->max_cap_cache_all_hit/4096) * volhasmd;
        numvol = 16;
        volCap = dms_client_config->max_cap_cache_all_hit;
        for (i = 1; i <= numvol; i++) {
            ret = cacheMgr_create_pool(i, volCap);
            if (ret) {
                P_UTEST_FAIL("create cache pool fail");
                break;
            }
        }

        expected_items = (dms_client_config->max_cap_cache_all_hit / 4096);
        for (i = 1; i <= 10; i++) {
            cacheMgr_get_cachePool_size(i, &pool_size, &num_items);
            if (pool_size != expected_items || num_items != 0) {
                P_UTEST_FAIL("wrong pool size");
                ret = -1;
                break;
            }
        }

        for (i = 11; i <= numvol; i++) {
            cacheMgr_get_cachePool_size(i, &pool_size, &num_items);
            if (pool_size != 0 && num_items != 0) {
                ret = -1;
                P_UTEST_FAIL("wrong pool size");
                break;
            }
        }

        for (i = 1; i <= numvol; i++) {
            cacheMgr_rel_pool(i);
        }

        ret = 0;
    } while (0);

    cacheMgr_rel_manager();
    release_dmsc_config();
    return ret;
}
