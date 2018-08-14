/*
 *
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * Test_space_chunk_cache_find.c
 *
 * Consider following to design input space for lookup metadata
 * Layer 1: LBID : hit or miss to form 1 or multiple segments
 * Layer 2: HBID : ignore (We won't need aggregate metadata into start-length base on HBID
 * Layer 3: DN - number of replica
 *               which DN list
 * Layer 4: whether same RBID
 * Layer 5: whether has continuous at offset
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include "discoC_sys_def.h"
#include "common_util.h"
#include "Test_metadata_mgr_common.h"
#include "dms-radix-tree.h"
#include "metadata_manager.h"
#include "Test_space_chunk_cache_find_private.h"
#include "Test_space_chunk_cache_find.h"

#ifdef DISCO_PREALLOC_SUPPORT
#include "dmsc_config.h"
#include "discoC_mem_manager.h"
#include "space_chunk_fsm.h"
#include "space_chunk.h"
#include "space_manager_export_api.h"
#include "space_chunk_cache.h"

extern int32_t mdata_item_mpoolID;

static int32_t _check_aschunk_ptr (as_chunk_t **res_chunk, int32_t len,
        as_chunk_t *expected)
{
    int32_t ret, i;

    ret = 0;

    for (i = 0; i < len; i++) {
        if (res_chunk[i] != expected) {
            ret = -EFAULT;
            break;
        }
    }
    return ret;
}

int32_t test_smcc_lookup_1 (void)
{
    as_chunk_t *res_chunk[32];
    sm_cache_pool_t cpool;
    uint64_t lb_s;
    int32_t ret, rel_ret, which_bk, lb_len, hit_ret;
    bool free_pool;

    test_num_run_smscc_find++;

    ret = 0;
    which_bk = 99;
    free_pool = false;

    do {
        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 10;
        lb_len = 12;

        memset(res_chunk, 0, sizeof(as_chunk_t *)*32);
        hit_ret = smcache_find_aschunk(&cpool, lb_s, lb_len, res_chunk);
        if (hit_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 0) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 0) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }
        if (_check_aschunk_ptr(res_chunk, lb_len, NULL)) {
            P_MDMGR_UTEST_FAIL("res chunk not null\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 0) { P_MDMGR_UTEST_FAIL("expect free 0 bk\n"); break; }

        test_num_pass_smscc_find++;

    } while (0);

    if (free_pool) {
        smcache_rel_cache(&cpool);
    }

    return ret;
}

int32_t test_smcc_lookup_2 (void)
{
    as_chunk_t *res_chunk[32];
    sm_cache_pool_t cpool;
    uint64_t lb_s;
    int32_t ret, rel_ret, which_bk, lb_len, hit_ret;
    bool free_pool;

    ret = 0;
    test_num_run_smscc_find++;
    which_bk = 99;
    free_pool = false;

    do {
        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 120;
        lb_len = 32;

        memset(res_chunk, 0, sizeof(as_chunk_t *)*32);
        hit_ret = smcache_find_aschunk(&cpool, lb_s, lb_len, res_chunk);
        if (hit_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 0) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 0) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }
        if (_check_aschunk_ptr(res_chunk, lb_len, NULL)) {
            P_MDMGR_UTEST_FAIL("res chunk not null\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 0) { P_MDMGR_UTEST_FAIL("expect free 0 bk\n"); break; }

        test_num_pass_smscc_find++;
    } while (0);

    if (free_pool) {
        smcache_rel_cache(&cpool);
    }

    return ret;
}

int32_t test_smcc_lookup_3_1 (void)
{
    sm_cache_pool_t cpool;
    as_chunk_t mychunk1, *res_chunk[32];
    uint64_t lb_s;
    int32_t ret, add_ret, hit_ret, rel_ret, which_bk, lb_len;
    bool free_pool = true;

    ret = 0;
    test_num_run_smscc_find++;
    which_bk = 99;

    do {
        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 127;
        lb_len = 1;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        memset(res_chunk, 0, sizeof(as_chunk_t *)*32);
        lb_s = (which_bk << BIT_SHIFT_BKSIZE);
        lb_len = 32;
        hit_ret = smcache_find_aschunk(&cpool, lb_s, lb_len, res_chunk);
        if (hit_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 1) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }
        if (_check_aschunk_ptr(res_chunk, lb_len, NULL)) {
            P_MDMGR_UTEST_FAIL("res chunk not null\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) { P_MDMGR_UTEST_FAIL("expect free 0 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE);
        lb_len = 5;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        memset(res_chunk, 0, sizeof(as_chunk_t *)*32);
        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 120;
        lb_len = 8;
        hit_ret = smcache_find_aschunk(&cpool, lb_s, lb_len, res_chunk);
        if (hit_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 5) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }
        if (_check_aschunk_ptr(res_chunk, lb_len, NULL)) {
            P_MDMGR_UTEST_FAIL("res chunk not null\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) { P_MDMGR_UTEST_FAIL("expect free 0 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 88;
        lb_len = 32;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        memset(res_chunk, 0, sizeof(as_chunk_t *)*32);
        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 20;
        lb_len = 20;
        hit_ret = smcache_find_aschunk(&cpool, lb_s, lb_len, res_chunk);
        if (hit_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 32) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }
        if (_check_aschunk_ptr(res_chunk, lb_len, NULL)) {
            P_MDMGR_UTEST_FAIL("res chunk not null\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) { P_MDMGR_UTEST_FAIL("expect free 0 bk\n"); break; }
        /* ----------------- end --------------- */

        test_num_pass_smscc_find++;
    } while (0);

    if (free_pool) {
        smcache_rel_cache(&cpool);
    }

    return ret;
}

int32_t test_smcc_lookup_3_2 (void)
{
    sm_cache_pool_t cpool;
    as_chunk_t mychunk1, *res_chunk[32];
    uint64_t lb_s;
    int32_t ret, add_ret, hit_ret, rel_ret, which_bk, lb_len;
    bool free_pool = true;

    ret = 0;
    test_num_run_smscc_find++;
    which_bk = 99;

    do {
        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE);
        lb_len = 32;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        memset(res_chunk, 0, sizeof(as_chunk_t *)*32);
        lb_s = (which_bk << BIT_SHIFT_BKSIZE);
        lb_len = 32;
        hit_ret = smcache_find_aschunk(&cpool, lb_s, lb_len, res_chunk);
        if (hit_ret != 32) {
            P_MDMGR_UTEST_FAIL("find MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 32) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }
        if (_check_aschunk_ptr(res_chunk, lb_len, &mychunk1)) {
            P_MDMGR_UTEST_FAIL("res chunk not null\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) { P_MDMGR_UTEST_FAIL("expect free 0 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 96;
        lb_len = 32;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        memset(res_chunk, 0, sizeof(as_chunk_t *)*32);
        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 96;
        lb_len = 32;
        hit_ret = smcache_find_aschunk(&cpool, lb_s, lb_len, res_chunk);
        if (hit_ret != 32) {
            P_MDMGR_UTEST_FAIL("find MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 32) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }
        if (_check_aschunk_ptr(res_chunk, lb_len, &mychunk1)) {
            P_MDMGR_UTEST_FAIL("res chunk not null\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) { P_MDMGR_UTEST_FAIL("expect free 0 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 66;
        lb_len = 32;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        memset(res_chunk, 0, sizeof(as_chunk_t *)*32);
        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 66;
        lb_len = 32;
        hit_ret = smcache_find_aschunk(&cpool, lb_s, lb_len, res_chunk);
        if (hit_ret != 32) {
            P_MDMGR_UTEST_FAIL("find MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 32) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }
        if (_check_aschunk_ptr(res_chunk, lb_len, &mychunk1)) {
            P_MDMGR_UTEST_FAIL("res chunk not null\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) { P_MDMGR_UTEST_FAIL("expect free 0 bk\n"); break; }
        /* ----------------- end --------------- */

        test_num_pass_smscc_find++;
    } while (0);

    if (free_pool) {
        smcache_rel_cache(&cpool);
    }

    return ret;
}

int32_t test_smcc_lookup_3_3 (void)
{
    sm_cache_pool_t cpool;
    as_chunk_t mychunk1, *res_chunk[32];
    uint64_t lb_s;
    int32_t ret, add_ret, hit_ret, rel_ret, which_bk, lb_len;
    bool free_pool = true;

    ret = 0;
    test_num_run_smscc_find++;
    which_bk = 99;

    do {
        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE);
        lb_len = 32;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        memset(res_chunk, 0, sizeof(as_chunk_t *)*32);
        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 16;
        lb_len = 32;
        hit_ret = smcache_find_aschunk(&cpool, lb_s, lb_len, res_chunk);
        if (hit_ret != 16) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 32) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }
        if (_check_aschunk_ptr(res_chunk, 16, &mychunk1)) {
            P_MDMGR_UTEST_FAIL("res chunk not null\n"); free_pool = true; break;
        }
        if (_check_aschunk_ptr(&(res_chunk[16]), 16, NULL)) {
            P_MDMGR_UTEST_FAIL("res chunk not null\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) { P_MDMGR_UTEST_FAIL("expect free 0 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 96;
        lb_len = 32;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        memset(res_chunk, 0, sizeof(as_chunk_t *)*32);
        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 90;
        lb_len = 10;
        hit_ret = smcache_find_aschunk(&cpool, lb_s, lb_len, res_chunk);
        if (hit_ret != 4) {
            P_MDMGR_UTEST_FAIL("find MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 32) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }
        if (_check_aschunk_ptr(res_chunk, 6, NULL)) {
            P_MDMGR_UTEST_FAIL("res chunk not null\n"); free_pool = true; break;
        }
        if (_check_aschunk_ptr(&(res_chunk[6]), 4, &mychunk1)) {
            P_MDMGR_UTEST_FAIL("res chunk not null\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) { P_MDMGR_UTEST_FAIL("expect free 0 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 88;
        lb_len = 5;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        memset(res_chunk, 0, sizeof(as_chunk_t *)*32);
        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 90;
        lb_len = 10;
        hit_ret = smcache_find_aschunk(&cpool, lb_s, lb_len, res_chunk);
        if (hit_ret != 3) {
            P_MDMGR_UTEST_FAIL("find MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 5) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }
        if (_check_aschunk_ptr(res_chunk, 3, &mychunk1)) {
            P_MDMGR_UTEST_FAIL("res chunk not null\n"); free_pool = true; break;
        }
        if (_check_aschunk_ptr(&(res_chunk[3]), 7, NULL)) {
            P_MDMGR_UTEST_FAIL("res chunk not null\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        test_num_pass_smscc_find++;
    } while (0);

    if (free_pool) {
        smcache_rel_cache(&cpool);
    }

    return ret;
}

int32_t test_smcc_lookup_3_4 (void)
{
    sm_cache_pool_t cpool;
    as_chunk_t mychunk1, mychunk2, *res_chunk[32];
    uint64_t lb_s;
    int32_t ret, add_ret, hit_ret, rel_ret, which_bk, lb_len;
    bool free_pool = true;

    ret = 0;
    test_num_run_smscc_find++;
    which_bk = 99;

    do {
        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE);
        lb_len = 25;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 25;
        lb_len = 7;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        memset(res_chunk, 0, sizeof(as_chunk_t *)*32);
        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 16;
        lb_len = 32;
        hit_ret = smcache_find_aschunk(&cpool, lb_s, lb_len, res_chunk);
        if (hit_ret != 16) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 32) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }
        if (_check_aschunk_ptr(res_chunk, 9, &mychunk1)) {
            P_MDMGR_UTEST_FAIL("res chunk not null\n"); free_pool = true; break;
        }
        if (_check_aschunk_ptr(&(res_chunk[9]), 7, &mychunk2)) {
            P_MDMGR_UTEST_FAIL("res chunk not null\n"); free_pool = true; break;
        }
        if (_check_aschunk_ptr(&(res_chunk[16]), 16, NULL)) {
            P_MDMGR_UTEST_FAIL("res chunk not null\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) { P_MDMGR_UTEST_FAIL("expect free 0 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 96;
        lb_len = 10;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 106;
        lb_len = 22;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        memset(res_chunk, 0, sizeof(as_chunk_t *)*32);
        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 90;
        lb_len = 20;
        hit_ret = smcache_find_aschunk(&cpool, lb_s, lb_len, res_chunk);
        if (hit_ret != 14) {
            P_MDMGR_UTEST_FAIL("find MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 32) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }
        if (_check_aschunk_ptr(res_chunk, 6, NULL)) {
            P_MDMGR_UTEST_FAIL("res chunk not null\n"); free_pool = true; break;
        }
        if (_check_aschunk_ptr(&(res_chunk[6]), 10, &mychunk1)) {
            P_MDMGR_UTEST_FAIL("res chunk not null\n"); free_pool = true; break;
        }
        if (_check_aschunk_ptr(&(res_chunk[16]), 4, &mychunk2)) {
            P_MDMGR_UTEST_FAIL("res chunk not null\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) { P_MDMGR_UTEST_FAIL("expect free 0 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 88;
        lb_len = 3;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 91;
        lb_len = 2;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        memset(res_chunk, 0, sizeof(as_chunk_t *)*32);
        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 85;
        lb_len = 16;
        hit_ret = smcache_find_aschunk(&cpool, lb_s, lb_len, res_chunk);
        if (hit_ret != 5) {
            P_MDMGR_UTEST_FAIL("find MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 5) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }
        if (_check_aschunk_ptr(res_chunk, 3, NULL)) {
            P_MDMGR_UTEST_FAIL("res chunk not null\n"); free_pool = true; break;
        }
        if (_check_aschunk_ptr(&(res_chunk[3]), 3, &mychunk1)) {
            P_MDMGR_UTEST_FAIL("res chunk not null\n"); free_pool = true; break;
        }
        if (_check_aschunk_ptr(&(res_chunk[6]), 2, &mychunk2)) {
            P_MDMGR_UTEST_FAIL("res chunk not null\n"); free_pool = true; break;
        }
        if (_check_aschunk_ptr(&(res_chunk[8]), 8, NULL)) {
            P_MDMGR_UTEST_FAIL("res chunk not null\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        test_num_pass_smscc_find++;
    } while (0);

    if (free_pool) {
        smcache_rel_cache(&cpool);
    }

    return ret;
}

static void _test_init_metadata (metadata_t *arr_MD_bk, int32_t len)
{
    int32_t i;

    for (i = 0; i < len; i++) {
        arr_MD_bk[i].array_metadata = NULL;
        arr_MD_bk[i].num_MD_items = 0;
        arr_MD_bk[i].num_invalMD_items = 0;
        arr_MD_bk[i].usr_ioaddr.volumeID = 0;
        arr_MD_bk[i].usr_ioaddr.lbid_start = 0;
        arr_MD_bk[i].usr_ioaddr.lbid_len = 0;
    }
}

static int32_t _test_cmp_metadata_dummy (metadata_t *arr_MD_bk, int32_t len)
{
    int32_t i, ret;

    ret = 0;

    for (i = 0; i < len; i++) {
        if (arr_MD_bk[i].array_metadata != NULL) { ret = -EFAULT; break; }
        if (arr_MD_bk[i].num_MD_items != 0) { ret = -EFAULT; break; }
        if (arr_MD_bk[i].num_invalMD_items != 0) { ret = -EFAULT; break; }
        if (arr_MD_bk[i].usr_ioaddr.volumeID != 0) { ret = -EFAULT; break; }
        if (arr_MD_bk[i].usr_ioaddr.lbid_start != 0) { ret = -EFAULT; break; }
        if (arr_MD_bk[i].usr_ioaddr.lbid_len != 0) { ret = -EFAULT; break; }
    }

    return ret;
}

static int32_t _test_cmp_metadata_noMDItem (metadata_t *MD_bk, uint32_t volID,
        uint64_t lb_s, int32_t len)
{
    int32_t ret;

    ret = 0;

    do {
        if (MD_bk->array_metadata != NULL) { ret = -EFAULT; break; }
        if (MD_bk->num_MD_items != 0) { ret = -EFAULT; break; }
        if (MD_bk->num_invalMD_items != 0) { ret = -EFAULT; break; }
        if (MD_bk->usr_ioaddr.volumeID != volID) { ret = -EFAULT; break; }
        if (MD_bk->usr_ioaddr.lbid_start != lb_s) { ret = -EFAULT; break; }
        if (MD_bk->usr_ioaddr.lbid_len != len) { ret = -EFAULT; break; }
    } while (0);

    return ret;
}

static int32_t _test_cmp_metadata (metadata_t *MD_bk, uint32_t volID,
        uint64_t lb_s, int32_t len, uint32_t num_items)
{
    int32_t ret;

    ret = 0;

    do {
        if (MD_bk->array_metadata == NULL) { ret = -EFAULT; break; }
        if (MD_bk->num_MD_items != num_items) { ret = -EFAULT; break; }
        if (MD_bk->num_invalMD_items != 0) { ret = -EFAULT; break; }
        if (MD_bk->usr_ioaddr.volumeID != volID) { ret = -EFAULT; break; }
        if (MD_bk->usr_ioaddr.lbid_start != lb_s) { ret = -EFAULT; break; }
        if (MD_bk->usr_ioaddr.lbid_len != len) { ret = -EFAULT; break; }
    } while (0);

    return ret;
}

static int32_t _test_cmp_MDItem (metaD_item_t *mdItem, uint64_t e_chunkID,
        uint64_t e_hbid_s, uint32_t e_numHBID, uint64_t lbid_s, uint16_t e_numDNLoc)
{
    int32_t ret, i;

    ret = 0;

    do {
        if (mdItem->num_hbids != e_numHBID) { ret = -EFAULT; break; }
        if (mdItem->lbid_s != lbid_s) { ret = -EFAULT; break; }
        if (mdItem->num_dnLoc != e_numDNLoc) { ret = -EFAULT; break; }
        if (mdItem->num_replica != e_numDNLoc) { ret = -EFAULT; break; }
        for (i = 0; i < e_numHBID; i++) {
            if (mdItem->arr_HBID[i] != e_hbid_s + i) { ret = -EFAULT; break; }
        }
    } while (0);
    return ret;
}

static int32_t _test_cmp_MDItem_seg (metaD_item_t *mdItem, uint64_t e_chunkID,
        uint64_t *e_hbid_s, uint32_t *e_numHBID, uint32_t hbid_seg,
        uint64_t lbid_s, uint32_t lb_len,
        uint16_t e_numDNLoc)
{
    int32_t ret, i, j, cnt;

    ret = 0;

    do {
        if (mdItem->num_hbids != lb_len) { ret = -EFAULT; break; }
        if (mdItem->lbid_s != lbid_s) { ret = -EFAULT; break; }
        if (mdItem->num_dnLoc != e_numDNLoc) { ret = -EFAULT; break; }
        if (mdItem->num_replica != e_numDNLoc) { ret = -EFAULT; break; }
        cnt = 0;
        for (i = 0; i < hbid_seg; i++) {
            for (j = 0; j < e_numHBID[i]; j++) {
                if (mdItem->arr_HBID[cnt++] != e_hbid_s[i] + j) { ret = -EFAULT; break; }
            }
        }
    } while (0);
    return ret;
}

static int32_t _test_cmp_DNLoc (metaD_DNLoc_t *dnLoc, uint32_t ipaddr, uint32_t port,
        uint16_t numPhy, uint64_t *rbid_len_off)
{
    int32_t ret, i;

    ret = 0;

    do {
        if (dnLoc->ipaddr != ipaddr) { ret = -EFAULT; break; }
        if (dnLoc->port != port) { ret = -EFAULT; break; }
        if (dnLoc->num_phyLocs != numPhy) { ret = -EFAULT; break; }
        for (i = 0; i < numPhy; i++) {
            if (dnLoc->rbid_len_offset[i] != rbid_len_off[i]) { ret = -EFAULT; break; }
        }
    } while (0);

    return ret;
}

//all LBID miss
int32_t test_smcc_lookupMD_1 (void)
{
    metadata_t arr_MD_bk[32];
    sm_cache_pool_t cpool;
    uint64_t lb_s;
    int32_t ret, num_md, rel_ret, which_bk, lb_len;
    bool free_pool = true;

    ret = 0;
    which_bk = 99;
    free_pool = false;

    test_num_run_smscc_find++;
    smcache_init_cache(&cpool);

    do {
        _test_init_metadata(arr_MD_bk, 32);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 10;
        lb_len = 12;

        num_md = smcache_query_MData(&cpool, 1234, lb_s, lb_len, arr_MD_bk);
        if (num_md != 1) {
            P_MDMGR_UTEST_FAIL("query MD err\n"); free_pool = true; break;
        }
        if (_test_cmp_metadata_noMDItem(arr_MD_bk, 1234, lb_s, lb_len)) {
            P_MDMGR_UTEST_FAIL("query MD err\n"); free_pool = true; break;
        }
        if (_test_cmp_metadata_dummy(&(arr_MD_bk[1]), 31)) {
            P_MDMGR_UTEST_FAIL("query MD err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 0) { P_MDMGR_UTEST_FAIL("expect free 0 bk\n"); break; }

        test_num_pass_smscc_find++;
    } while (0);

    if (free_pool) {
        smcache_rel_cache(&cpool);
    }

    return ret;
}

//all LBID hit: all metadata come from 1 aschunk
int32_t test_smcc_lookupMD_2_1 (void)
{
    metadata_t arr_MD_bk[32];
    metaD_item_t *mdItem;
    as_chunk_t mychunk1;
    sm_cache_pool_t cpool;
    uint64_t lb_s, rlo;
    int32_t add_ret, ret, num_md, rel_ret, which_bk, lb_len;
    bool free_pool = true;

    ret = 0;
    test_num_run_smscc_find++;
    which_bk = 99;
    free_pool = false;

    smcache_init_cache(&cpool);

    do {
        smcache_init_cache(&cpool);

        _test_init_metadata(arr_MD_bk, 32);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE);
        lb_len = 32;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 2);
        aschunk_init_chunk_DNLoc(&mychunk1, 0, ccma_inet_ntoa("10.10.100.1"),
                20100, 1234, 100);
        aschunk_init_chunk_DNLoc(&mychunk1, 1, ccma_inet_ntoa("10.10.100.2"),
                20100, 2345, 200);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE);
        lb_len = 32;

        num_md = smcache_query_MData(&cpool, 1234, lb_s, lb_len, arr_MD_bk);
        if (num_md != 1) {
            P_MDMGR_UTEST_FAIL("query MD err\n"); free_pool = true; break;
        }
        if (_test_cmp_metadata_dummy(&(arr_MD_bk[1]), 31)) {
            P_MDMGR_UTEST_FAIL("query MD err\n"); free_pool = true; break;
        }

        if (_test_cmp_metadata(arr_MD_bk, 1234, lb_s, lb_len, 1)) {
            P_MDMGR_UTEST_FAIL("query MD err\n"); free_pool = true; break;
        }

        mdItem = arr_MD_bk[0].array_metadata[0];
        if (_test_cmp_MDItem(mdItem, 101, 666, lb_len, lb_s, 2)) {
            P_MDMGR_UTEST_FAIL("query MD err\n"); free_pool = true; break;
        }

        rlo = compose_triple(1234, lb_len, 100);
        if (_test_cmp_DNLoc(&(mdItem->udata_dnLoc[0]),
                ccma_inet_ntoa("10.10.100.1"), 20100, 1, &rlo)) {
            P_MDMGR_UTEST_FAIL("query MD err\n"); free_pool = true; break;
        }
        rlo = compose_triple(2345, lb_len, 200);
        if (_test_cmp_DNLoc(&(mdItem->udata_dnLoc[1]),
                ccma_inet_ntoa("10.10.100.2"), 20100, 1, &rlo)) {
            P_MDMGR_UTEST_FAIL("query MD err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) { P_MDMGR_UTEST_FAIL("expect free 0 bk\n"); break; }

        free_metadata(arr_MD_bk);
        test_num_pass_smscc_find++;
    } while (0);

    if (free_pool) {
        smcache_rel_cache(&cpool);
        free_metadata(arr_MD_bk);
    }

    return ret;
}

//all LBID hit: all metadata come from 2 aschunk, aschunk is continuous in Logical and Physical location
int32_t test_smcc_lookupMD_2_2 (void)
{
    metadata_t arr_MD_bk[32];
    metaD_item_t *mdItem;
    as_chunk_t mychunk1, mychunk2;
    sm_cache_pool_t cpool;
    uint64_t lb_s, rlo;
    int32_t add_ret, ret, num_md, rel_ret, which_bk, lb_len;
    bool free_pool = true;

    ret = 0;
    test_num_run_smscc_find++;
    which_bk = 99;
    free_pool = false;

    smcache_init_cache(&cpool);

    do {
        smcache_init_cache(&cpool);

        _test_init_metadata(arr_MD_bk, 32);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE);
        lb_len = 16;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 2);
        aschunk_init_chunk_DNLoc(&mychunk1, 0, ccma_inet_ntoa("10.10.100.1"),
                20100, 1234, 100);
        aschunk_init_chunk_DNLoc(&mychunk1, 1, ccma_inet_ntoa("10.10.100.2"),
                20100, 2345, 200);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 16;
        lb_len = 16;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666 + 16, 2);
        aschunk_init_chunk_DNLoc(&mychunk2, 0, ccma_inet_ntoa("10.10.100.1"),
                20100, 1234, 100 + 16);
        aschunk_init_chunk_DNLoc(&mychunk2, 1, ccma_inet_ntoa("10.10.100.2"),
                20100, 2345, 200 + 16);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE);
        lb_len = 32;

        num_md = smcache_query_MData(&cpool, 1234, lb_s, lb_len, arr_MD_bk);
        if (num_md != 1) {
            P_MDMGR_UTEST_FAIL("query MD err\n"); free_pool = true; break;
        }
        if (_test_cmp_metadata_dummy(&(arr_MD_bk[1]), 31)) {
            P_MDMGR_UTEST_FAIL("query MD err\n"); free_pool = true; break;
        }

        if (_test_cmp_metadata(arr_MD_bk, 1234, lb_s, lb_len, 1)) {
            P_MDMGR_UTEST_FAIL("query MD err\n"); free_pool = true; break;
        }

        mdItem = arr_MD_bk[0].array_metadata[0];
        if (_test_cmp_MDItem(mdItem, 101, 666, lb_len, lb_s, 2)) {
            P_MDMGR_UTEST_FAIL("query MD err\n"); free_pool = true; break;
        }

        rlo = compose_triple(1234, lb_len, 100);
        if (_test_cmp_DNLoc(&(mdItem->udata_dnLoc[0]),
                ccma_inet_ntoa("10.10.100.1"), 20100, 1, &rlo)) {
            P_MDMGR_UTEST_FAIL("query MD err\n"); free_pool = true; break;
        }
        rlo = compose_triple(2345, lb_len, 200);
        if (_test_cmp_DNLoc(&(mdItem->udata_dnLoc[1]),
                ccma_inet_ntoa("10.10.100.2"), 20100, 1, &rlo)) {
            P_MDMGR_UTEST_FAIL("query MD err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) { P_MDMGR_UTEST_FAIL("expect free 0 bk\n"); break; }

        free_metadata(arr_MD_bk);
        test_num_pass_smscc_find++;
    } while (0);

    if (free_pool) {
        smcache_rel_cache(&cpool);
        free_metadata(arr_MD_bk);
    }

    return ret;
}

//all LBID hit: all metadata come from 2 aschunk, aschunk is continuous in Logical but not Physical location
int32_t test_smcc_lookupMD_2_3 (void)
{
    metadata_t arr_MD_bk[32];
    metaD_item_t *mdItem;
    as_chunk_t mychunk1, mychunk2;
    sm_cache_pool_t cpool;
    uint64_t lb_s, rlo[2], hbid_seg_s[2];
    uint32_t hbid_seg_len[2];
    int32_t add_ret, ret, num_md, rel_ret, which_bk, lb_len;
    bool free_pool = true;

    ret = 0;
    test_num_run_smscc_find++;
    which_bk = 99;
    free_pool = false;

    smcache_init_cache(&cpool);

    do {
        smcache_init_cache(&cpool);

        _test_init_metadata(arr_MD_bk, 32);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE);
        lb_len = 16;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 2);
        aschunk_init_chunk_DNLoc(&mychunk1, 0, ccma_inet_ntoa("10.10.100.1"),
                20100, 1234, 100);
        aschunk_init_chunk_DNLoc(&mychunk1, 1, ccma_inet_ntoa("10.10.100.2"),
                20100, 2345, 200);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 16;
        lb_len = 16;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666 + 101, 2);
        aschunk_init_chunk_DNLoc(&mychunk2, 0, ccma_inet_ntoa("10.10.100.1"),
                20100, 1234, 100 + 101);
        aschunk_init_chunk_DNLoc(&mychunk2, 1, ccma_inet_ntoa("10.10.100.2"),
                20100, 2345, 200 + 101);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE);
        lb_len = 32;

        num_md = smcache_query_MData(&cpool, 1234, lb_s, lb_len, arr_MD_bk);
        if (num_md != 1) {
            P_MDMGR_UTEST_FAIL("query MD err\n"); free_pool = true; break;
        }
        if (_test_cmp_metadata_dummy(&(arr_MD_bk[1]), 31)) {
            P_MDMGR_UTEST_FAIL("query MD err\n"); free_pool = true; break;
        }

        if (_test_cmp_metadata(arr_MD_bk, 1234, lb_s, lb_len, 1)) {
            P_MDMGR_UTEST_FAIL("query MD err\n"); free_pool = true; break;
        }

        mdItem = arr_MD_bk[0].array_metadata[0];
        hbid_seg_s[0] = 666;        hbid_seg_len[0] = 16;
        hbid_seg_s[1] = 666 + 101;  hbid_seg_len[1] = 16;
        if (_test_cmp_MDItem_seg(mdItem, 101, hbid_seg_s, hbid_seg_len, 2,
                lb_s, lb_len, 2)) {
            P_MDMGR_UTEST_FAIL("query MD err\n"); free_pool = true; break;
        }

        rlo[0] = compose_triple(1234, 16, 100);
        rlo[1] = compose_triple(1234, 16, 100 + 101);
        if (_test_cmp_DNLoc(&(mdItem->udata_dnLoc[0]),
                ccma_inet_ntoa("10.10.100.1"), 20100, 2, rlo)) {
            P_MDMGR_UTEST_FAIL("query MD err\n"); free_pool = true; break;
        }
        rlo[0] = compose_triple(2345, 16, 200);
        rlo[1] = compose_triple(2345, 16, 200 + 101);
        if (_test_cmp_DNLoc(&(mdItem->udata_dnLoc[1]),
                ccma_inet_ntoa("10.10.100.2"), 20100, 2, rlo)) {
            P_MDMGR_UTEST_FAIL("query MD err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) { P_MDMGR_UTEST_FAIL("expect free 0 bk\n"); break; }

        free_metadata(arr_MD_bk);
        test_num_pass_smscc_find++;
    } while (0);

    if (free_pool) {
        smcache_rel_cache(&cpool);
        free_metadata(arr_MD_bk);
    }

    return ret;
}

#if 0
int32_t test_smcc_lookupMD_2_4 (void)
{
    int32_t ret;

    ret = 0;
    test_num_run_smscc_find++;

    //test_num_pass_smscc_find++;

    return ret;
}

int32_t test_smcc_lookupMD_2_5 (void)
{
    int32_t ret;

    ret = 0;
    test_num_run_smscc_find++;

    //test_num_pass_smscc_find++;

    return ret;
}

int32_t test_smcc_lookupMD_2_6 (void)
{
    int32_t ret;

    ret = 0;
    test_num_run_smscc_find++;

    //test_num_pass_smscc_find++;

    return ret;
}
#endif

#if 0
int32_t test_smcc_lookupMD_3_1 (void)
{
    int32_t ret;

    ret = 0;
    test_num_run_smscc_find++;

    //test_num_pass_smscc_find++;

    return ret;
}

int32_t test_smcc_lookupMD_3_2 (void)
{
    int32_t ret;

    ret = 0;
    test_num_run_smscc_find++;

    //test_num_pass_smscc_find++;

    return ret;
}

int32_t test_smcc_lookupMD_3_3 (void)
{
    int32_t ret;

    ret = 0;
    test_num_run_smscc_find++;

    //test_num_pass_smscc_find++;

    return ret;
}

int32_t test_smcc_lookupMD_3_4 (void)
{
    int32_t ret;

    ret = 0;
    test_num_run_smscc_find++;

    //test_num_pass_smscc_find++;

    return ret;
}
#endif

static void _create_MD_mem_pool (void)
{
    int32_t pool_size;

    pool_size = dms_client_config->max_nr_io_reqs*MAX_LBS_PER_RQ;
    mdata_item_mpoolID = register_mem_pool("MData_Item", pool_size, sizeof(metaD_item_t), true);
}

static void _rel_MD_mem_pool (void)
{
    deregister_mem_pool(mdata_item_mpoolID);
}

int32_t test_smcc_lookup (void)
{
    int32_t ret;

    ret = 0;
    test_num_all_smscc_find += 10;

    ret = init_dmsc_config(0);
    init_mempool_manager();
    _create_MD_mem_pool();

    test_smcc_lookup_1();
    test_smcc_lookup_2();
    test_smcc_lookup_3_1();
    test_smcc_lookup_3_2();
    test_smcc_lookup_3_3();
    test_smcc_lookup_3_4();

    test_smcc_lookupMD_1(); //50

    test_smcc_lookupMD_2_1(); //51
    test_smcc_lookupMD_2_2();
    test_smcc_lookupMD_2_3();
    //test_smcc_lookupMD_2_4();
    //test_smcc_lookupMD_2_5();
    //test_smcc_lookupMD_2_6();

    //test_smcc_lookupMD_3_1();
    //test_smcc_lookupMD_3_2();
    //test_smcc_lookupMD_3_3();
    //test_smcc_lookupMD_3_4();

    _rel_MD_mem_pool();
    rel_mempool_manager();
    release_dmsc_config();

    return ret;
}

#endif

int32_t test_smscc_find_run (void)
{
    int32_t ret;

    ret = 0;

    do {
#ifdef DISCO_PREALLOC_SUPPORT
        ret = 0;
        test_smcc_lookup();
#else
        ret = 0;
#endif
    } while (0);

    return ret;
}

void test_smscc_find_reset (void)
{
    test_num_all_smscc_find = 0;
    test_num_run_smscc_find = 0;
    test_num_pass_smscc_find = 0;
}

void test_smscc_find_get_result (uint32_t *total, uint32_t *run, uint32_t *passed)
{
    *total = test_num_all_smscc_find;
    *run = test_num_run_smscc_find;
    *passed = test_num_pass_smscc_find;
}

void test_smscc_find_init (void)
{
    test_smscc_find_reset();
}
