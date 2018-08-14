/*
 *
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * Test_space_chunk_cache.c
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include "discoC_sys_def.h"
#include "Test_metadata_mgr_common.h"
#include "Test_space_chunk_cache_private.h"
#include "dms-radix-tree.h"
#include "metadata_manager.h"
#include "Test_space_chunk_cache_find.h"

#ifdef DISCO_PREALLOC_SUPPORT
#include "space_chunk_fsm.h"
#include "space_chunk.h"
#include "space_manager_export_api.h"
#include "space_chunk_cache.h"

int32_t test_smcc_init_rel (void)
{
    sm_cache_pool_t cpool;
    int32_t rel_ret;
    bool do_free;

    num_smcc_run_ftest++;
    do_free = false;

    do {
        smcache_init_cache(&cpool);

        if (cpool.num_cache_items != 0) {
            do_free = true;
            break;
        }

        if (cpool.num_cache_buckets != 0) {
            do_free = true;
            break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 0) {
            break;
        }
        num_smcc_pass_ftest++;
    } while (0);

    if (do_free) {
        smcache_rel_cache(&cpool);
    }

    return 0;
}

int32_t test_smcc_simple_add (void)
{
    sm_cache_pool_t cpool;
    as_chunk_t mychunk;
    int32_t rel_ret, add_ret;
    bool do_free;

    num_smcc_run_ftest++;
    do_free = false;

    do {
        smcache_init_cache(&cpool);

        aschunk_init_chunk(&mychunk, 101, 12345, 1, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk);
        if (cpool.num_cache_items != 1) {
            do_free = true;
            break;
        }

        if (cpool.num_cache_buckets != 1) {
            do_free = true;
            break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) {
            break;
        }
        num_smcc_pass_ftest++;
    } while (0);

    if (do_free) {
        smcache_rel_cache(&cpool);
    }

    return 0;
}

int32_t test_smcc_simple_rm (void)
{
    sm_cache_pool_t cpool;
    as_chunk_t mychunk;
    int32_t rel_ret, add_ret;
    bool do_free;

    num_smcc_run_ftest++;
    do_free = false;

    do {
        smcache_init_cache(&cpool);

        aschunk_init_chunk(&mychunk, 101, 12345, 1, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk);
        if (cpool.num_cache_items != 1) {
            do_free = true;
            break;
        }

        if (cpool.num_cache_buckets != 1) {
            do_free = true;
            break;
        }

        add_ret = smcache_rm_aschunk(&cpool, mychunk.lbid_s, mychunk.lb_len);
        if (add_ret) {
            do_free = true;
            break;
        }

        if (cpool.num_cache_items != 0) {
            do_free = true;
            break;
        }

        if (cpool.num_cache_buckets != 0) {
            do_free = true;
            break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 0) {
            break;
        }
        num_smcc_pass_ftest++;
    } while (0);

    if (do_free) {
        smcache_rel_cache(&cpool);
    }

    return 0;
}

/******* add unit test ********/
int32_t test_smcc_add_1 (void) //add LBAs with new bucket within bucket
{
    sm_cache_pool_t cpool;
    as_chunk_t mychunk;
    uint64_t lb_s;
    int32_t ret, add_ret, rel_ret, which_bk, lb_len;
    bool do_free;

    ret = 0;

    num_smcc_run_ftest++;
    which_bk = 99;
    do_free = false;

    do {
        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE);
        lb_len = 32;
        aschunk_init_chunk(&mychunk, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); do_free = true; break;
        }
        if (cpool.num_cache_items != lb_len) {
            P_MDMGR_UTEST_FAIL("total items err\n"); do_free = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); do_free = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 111;
        lb_len = 17;
        aschunk_init_chunk(&mychunk, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); do_free = true; break;
        }
        if (cpool.num_cache_items != lb_len) {
            P_MDMGR_UTEST_FAIL("total items err\n"); do_free = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); do_free = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 73;
        lb_len = 9;
        aschunk_init_chunk(&mychunk, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); do_free = true; break;
        }
        if (cpool.num_cache_items != lb_len) {
            P_MDMGR_UTEST_FAIL("total items err\n"); do_free = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); do_free = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        num_smcc_pass_ftest++;
    } while (0);

    if (do_free) {
        smcache_rel_cache(&cpool);
    }

    return ret;
}

int32_t test_smcc_add_2 (void) //add LBAs with new bucket - cross bucket
{
    sm_cache_pool_t cpool;
    as_chunk_t mychunk;
    uint64_t lb_s;
    int32_t ret, add_ret, rel_ret, which_bk, lb_len;
    bool do_free;

    ret = 0;

    do_free = false;
    num_smcc_run_ftest++;
    which_bk = 99;

    do {
        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 127;
        lb_len = 32;
        aschunk_init_chunk(&mychunk, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); do_free = true; break;
        }
        if (cpool.num_cache_items != lb_len) {
            P_MDMGR_UTEST_FAIL("total items err\n"); do_free = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); do_free = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 97;
        lb_len = 32;
        aschunk_init_chunk(&mychunk, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); do_free = true; break;
        }
        if (cpool.num_cache_items != lb_len) {
            P_MDMGR_UTEST_FAIL("total items err\n"); do_free = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); do_free = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 120;
        lb_len = 16;
        aschunk_init_chunk(&mychunk, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); do_free = true; break;
        }
        if (cpool.num_cache_items != lb_len) {
            P_MDMGR_UTEST_FAIL("total items err\n"); do_free = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); do_free = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        num_smcc_pass_ftest++;
    } while (0);

    if (do_free) {
        smcache_rel_cache(&cpool);
    }

    return ret;
}

int32_t test_smcc_add_3_1 (void) //all offset in bucket no value
{
    sm_cache_pool_t cpool;
    as_chunk_t mychunk1, mychunk2;
    uint64_t lb_s;
    int32_t ret, add_ret, rel_ret, which_bk, lb_len;
    bool free_pool = true;

    num_smcc_run_ftest++;
    ret = 0;

    which_bk = 99;
    do {
        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE);
        lb_len = 1;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 1;
        lb_len = 32;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }
        if (cpool.num_cache_items != (lb_len + 1)) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE);
        lb_len = 1;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 111;
        lb_len = 17;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }
        if (cpool.num_cache_items != (lb_len + 1)) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE);
        lb_len = 1;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 73;
        lb_len = 9;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }
        if (cpool.num_cache_items != (lb_len + 1)) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        num_smcc_pass_ftest++;
    } while (0);

    if (free_pool) {
        smcache_rel_cache(&cpool);
    }

    return ret;
}

int32_t test_smcc_add_3_2 (void) //all offset in bucket has value
{
    sm_cache_pool_t cpool;
    as_chunk_t mychunk1, mychunk2;
    uint64_t lb_s;
    int32_t ret, add_ret, rel_ret, which_bk, lb_len;
    bool free_pool;

    num_smcc_run_ftest++;
    ret = 0;
    free_pool = true;
    which_bk = 99;
    do {
        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE);
        lb_len = 32;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE);
        lb_len = 32;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); break; }
        if (cpool.num_cache_items != lb_len) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 111;
        lb_len = 17;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 111;
        lb_len = 17;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }
        if (cpool.num_cache_items != lb_len) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 73;
        lb_len = 9;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 73;
        lb_len = 9;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }
        if (cpool.num_cache_items != lb_len) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        num_smcc_pass_ftest++;
    } while (0);

    if (free_pool) {
        smcache_rel_cache(&cpool);
    }

    return ret;
}

int32_t test_smcc_add_3_3 (void) //partial offset in bucket has value
{
    sm_cache_pool_t cpool;
    as_chunk_t mychunk1, mychunk2;
    uint64_t lb_s;
    int32_t ret, add_ret, rel_ret, which_bk, lb_len;
    bool free_pool;

    num_smcc_run_ftest++;
    ret = 0;
    free_pool = false;

    which_bk = 99;
    do {
        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 2;
        lb_len = 15;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE);
        lb_len = 32;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }
        if (cpool.num_cache_items != lb_len) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 107;
        lb_len = 18;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 111;
        lb_len = 10;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }
        if (cpool.num_cache_items != 18) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 68;
        lb_len = 9;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 73;
        lb_len = 9;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }
        if (cpool.num_cache_items != 14) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        num_smcc_pass_ftest++;
    } while (0);

    if (free_pool) {
        smcache_rel_cache(&cpool);
    }

    return ret;
}

int32_t test_smcc_add_3 (void) //add LBAs with exist bucket within a bucket
{
    int32_t ret;

    do {
        if ((ret = test_smcc_add_3_1())) {
            break;
        }

        if ((ret = test_smcc_add_3_2())) {
            break;
        }

        if ((ret = test_smcc_add_3_3())) {
            break;
        }
    } while (0);
    ret = 0;

    return ret;
}

//all offset in 1st bucket no value, all offset in 2nd bucket no value
int32_t test_smcc_add_4_1_1 (void)
{
    sm_cache_pool_t cpool;
    as_chunk_t mychunk1, mychunk2, mychunk3;
    uint64_t lb_s;
    int32_t ret, add_ret, rel_ret, which_bk, lb_len;
    bool free_pool;

    ret = 0;

    num_smcc_run_ftest++;
    which_bk = 99;
    free_pool = false;

    do {
        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE);
        lb_len = 1;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE) + 127;
        lb_len = 1;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 127;
        lb_len = 32;
        aschunk_init_chunk(&mychunk3, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk3);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }
        if (cpool.num_cache_items != (lb_len + 2)) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break; }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE);
        lb_len = 1;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE) + 127;
        lb_len = 1;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 97;
        lb_len = 32;
        aschunk_init_chunk(&mychunk3, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk3);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }
        if (cpool.num_cache_items != (lb_len + 2)) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE);
        lb_len = 1;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE) + 127;
        lb_len = 1;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 120;
        lb_len = 16;
        aschunk_init_chunk(&mychunk3, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk3);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }
        if (cpool.num_cache_items != (lb_len + 2)) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        num_smcc_pass_ftest++;
    } while (0);

    if (free_pool) {
        smcache_rel_cache(&cpool);
    }

    return ret;
}

//all offset in 1st bucket no value, all offset in 2nd bucket has value
int32_t test_smcc_add_4_1_2 (void)
{
    sm_cache_pool_t cpool;
    as_chunk_t mychunk1, mychunk2, mychunk3;
    uint64_t lb_s;
    int32_t ret, add_ret, rel_ret, which_bk, lb_len;
    bool free_pool;

    ret = 0;

    num_smcc_run_ftest++;
    which_bk = 99;
    free_pool = false;

    do {
        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE);
        lb_len = 1;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE);
        lb_len = 31;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 127;
        lb_len = 32;
        aschunk_init_chunk(&mychunk3, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk3);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }
        if (cpool.num_cache_items != (lb_len + 1)) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE);
        lb_len = 1;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE);
        lb_len = 1;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 97;
        lb_len = 32;
        aschunk_init_chunk(&mychunk3, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk3);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }
        if (cpool.num_cache_items != (lb_len + 1)) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE);
        lb_len = 1;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE);
        lb_len = 8;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 120;
        lb_len = 16;
        aschunk_init_chunk(&mychunk3, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk3);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }
        if (cpool.num_cache_items != (lb_len + 1)) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        num_smcc_pass_ftest++;
    } while (0);

    if (free_pool) {
        smcache_rel_cache(&cpool);
    }

    return ret;
}

//all offset in 1st bucket no value, partial offset in 2nd bucket has value
int32_t test_smcc_add_4_1_3 (void)
{
    sm_cache_pool_t cpool;
    as_chunk_t mychunk1, mychunk2, mychunk3;
    uint64_t lb_s;
    int32_t ret, add_ret, rel_ret, which_bk, lb_len;
    bool free_pool = true;

    ret = 0;

    num_smcc_run_ftest++;
    which_bk = 99;
    free_pool = false;

    do {
        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE);
        lb_len = 1;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE) + 10;
        lb_len = 10;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 127;
        lb_len = 32;
        aschunk_init_chunk(&mychunk3, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk3);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }
        if (cpool.num_cache_items != (lb_len + 1)) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE);
        lb_len = 1;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE);
        lb_len = 3;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 97;
        lb_len = 32;
        aschunk_init_chunk(&mychunk3, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk3);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }
        if (cpool.num_cache_items != (lb_len + 3)) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE);
        lb_len = 1;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE);
        lb_len = 16;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 120;
        lb_len = 16;
        aschunk_init_chunk(&mychunk3, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk3);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_items != (lb_len + 9)) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        num_smcc_pass_ftest++;
    } while (0);

    if (free_pool) {
        smcache_rel_cache(&cpool);
    }

    return ret;
}

int32_t test_smcc_add_4_1 (void) //all offset in 1st bucket no value
{
    int32_t ret;

    do {
        if ((ret = test_smcc_add_4_1_1())) {
            break;
        }

        if ((ret = test_smcc_add_4_1_2())) {
            break;
        }

        if ((ret = test_smcc_add_4_1_3())) {
            break;
        }
    } while (0);
    ret = 0;

    return ret;
}

//all offset in 1st bucket has value, all offset in 2nd bucket no value
int32_t test_smcc_add_4_2_1 (void)
{
    sm_cache_pool_t cpool;
    as_chunk_t mychunk1, mychunk2, mychunk3;
    uint64_t lb_s;
    int32_t ret, add_ret, rel_ret, which_bk, lb_len;
    bool free_pool;

    ret = 0;

    num_smcc_run_ftest++;
    which_bk = 99;
    free_pool = false;

    do {
        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 127;
        lb_len = 1;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE) + 127;
        lb_len = 1;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 127;
        lb_len = 32;
        aschunk_init_chunk(&mychunk3, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk3);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }
        if (cpool.num_cache_items != (lb_len + 1)) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 97;
        lb_len = 31;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE) + 127;
        lb_len = 1;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 97;
        lb_len = 32;
        aschunk_init_chunk(&mychunk3, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk3);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }
        if (cpool.num_cache_items != (lb_len + 1)) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 120;
        lb_len = 8;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE) + 127;
        lb_len = 1;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 120;
        lb_len = 16;
        aschunk_init_chunk(&mychunk3, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk3);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }
        if (cpool.num_cache_items != (lb_len + 1)) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        num_smcc_pass_ftest++;
    } while (0);

    if (free_pool) {
        smcache_rel_cache(&cpool);
    }

    return ret;
}

//all offset in 1st bucket has value, all offset in 2nd bucket has value
int32_t test_smcc_add_4_2_2 (void)
{
    sm_cache_pool_t cpool;
    as_chunk_t mychunk1, mychunk2, mychunk3;
    uint64_t lb_s;
    int32_t ret, add_ret, rel_ret, which_bk, lb_len;
    bool free_pool;

    ret = 0;

    num_smcc_run_ftest++;
    which_bk = 99;
    free_pool = false;

    do {
        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 127;
        lb_len = 1;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE);
        lb_len = 31;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 127;
        lb_len = 32;
        aschunk_init_chunk(&mychunk3, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk3);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }
        if (cpool.num_cache_items != lb_len) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 97;
        lb_len = 31;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE);
        lb_len = 1;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 97;
        lb_len = 32;
        aschunk_init_chunk(&mychunk3, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk3);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }
        if (cpool.num_cache_items != lb_len) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 120;
        lb_len = 8;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE);
        lb_len = 8;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 120;
        lb_len = 16;
        aschunk_init_chunk(&mychunk3, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk3);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }
        if (cpool.num_cache_items != lb_len) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        num_smcc_pass_ftest++;
    } while (0);

    if (free_pool) {
        smcache_rel_cache(&cpool);
    }

    return ret;
}

//all offset in 1st bucket has value, partial offset in 2nd bucket has value
int32_t test_smcc_add_4_2_3 (void)
{
    sm_cache_pool_t cpool;
    as_chunk_t mychunk1, mychunk2, mychunk3;
    uint64_t lb_s;
    int32_t ret, add_ret, rel_ret, which_bk, lb_len;
    bool free_pool;

    ret = 0;

    num_smcc_run_ftest++;
    which_bk = 99;
    free_pool = false;

    do {
        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 127;
        lb_len = 1;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE) + 10;
        lb_len = 10;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 127;
        lb_len = 32;
        aschunk_init_chunk(&mychunk3, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk3);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }
        if (cpool.num_cache_items != lb_len) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 97;
        lb_len = 31;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE);
        lb_len = 3;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 97;
        lb_len = 32;
        aschunk_init_chunk(&mychunk3, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk3);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }
        if (cpool.num_cache_items != (lb_len + 2)) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 120;
        lb_len = 8;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE);
        lb_len = 16;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 120;
        lb_len = 16;
        aschunk_init_chunk(&mychunk3, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk3);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }
        if (cpool.num_cache_items != (lb_len + 8)) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        num_smcc_pass_ftest++;
    } while (0);

    if (free_pool) {
        smcache_rel_cache(&cpool);
    }

    return ret;
}

int32_t test_smcc_add_4_2 (void) //all offset in 1st bucket has value - cross bucket
{
    int32_t ret;

    do {
        if ((ret = test_smcc_add_4_2_1())) {
            break;
        }

        if ((ret = test_smcc_add_4_2_2())) {
            break;
        }

        if ((ret = test_smcc_add_4_2_3())) {
            break;
        }
    } while (0);
    ret = 0;

    return ret;
}

//partial offset in 1st bucket has value, all offset in 2nd bucket no value
int32_t test_smcc_add_4_3_1 (void)
{
    sm_cache_pool_t cpool;
    as_chunk_t mychunk1, mychunk2, mychunk3;
    uint64_t lb_s;
    int32_t ret, add_ret, rel_ret, which_bk, lb_len;
    bool free_pool;

    ret = 0;

    num_smcc_run_ftest++;
    which_bk = 99;
    free_pool = false;

    do {
        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 123;
        lb_len = 5;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE) + 127;
        lb_len = 1;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 127;
        lb_len = 32;
        aschunk_init_chunk(&mychunk3, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk3);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_items != (lb_len + 5)) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 110;
        lb_len = 10;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE) + 127;
        lb_len = 1;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 97;
        lb_len = 32;
        aschunk_init_chunk(&mychunk3, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk3);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_items != (lb_len + 1)) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 110;
        lb_len = 16;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE) + 127;
        lb_len = 1;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 120;
        lb_len = 16;
        aschunk_init_chunk(&mychunk3, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk3);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_items != (lb_len + 11)) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        num_smcc_pass_ftest++;
    } while (0);

    if (free_pool) {
        smcache_rel_cache(&cpool);
    }

    return ret;
}

//partial offset in 1st bucket has value, all offset in 2nd bucket has value
int32_t test_smcc_add_4_3_2 (void)
{
    sm_cache_pool_t cpool;
    as_chunk_t mychunk1, mychunk2, mychunk3;
    uint64_t lb_s;
    int32_t ret, add_ret, rel_ret, which_bk, lb_len;
    bool free_pool;

    ret = 0;

    num_smcc_run_ftest++;
    which_bk = 99;
    free_pool = false;

    do {
        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 123;
        lb_len = 5;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); break; }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE);
        lb_len = 31;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 127;
        lb_len = 32;
        aschunk_init_chunk(&mychunk3, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk3);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_items != (lb_len + 4)) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 110;
        lb_len = 10;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE);
        lb_len = 1;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 97;
        lb_len = 32;
        aschunk_init_chunk(&mychunk3, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk3);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); break; }
        if (cpool.num_cache_items != lb_len) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break; }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 110;
        lb_len = 16;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE);
        lb_len = 8;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 120;
        lb_len = 16;
        aschunk_init_chunk(&mychunk3, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk3);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_items != (lb_len + 10)) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        num_smcc_pass_ftest++;
    } while (0);

    if (free_pool) {
        smcache_rel_cache(&cpool);
    }

    return ret;
}

//partial offset in 1st bucket has value, partial offset in 2nd bucket has value
int32_t test_smcc_add_4_3_3 (void)
{
    sm_cache_pool_t cpool;
    as_chunk_t mychunk1, mychunk2, mychunk3;
    uint64_t lb_s;
    int32_t ret, add_ret, rel_ret, which_bk, lb_len;
    bool free_pool;

    ret = 0;

    num_smcc_run_ftest++;
    which_bk = 99;

    free_pool = false;

    do {
        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 123;
        lb_len = 5;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE) + 20;
        lb_len = 32;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 127;
        lb_len = 32;
        aschunk_init_chunk(&mychunk3, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk3);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_items != (lb_len + 25)) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 110;
        lb_len = 10;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE);
        lb_len = 3;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 97;
        lb_len = 32;
        aschunk_init_chunk(&mychunk3, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk3);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_items != (lb_len + 2)) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 110;
        lb_len = 16;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE);
        lb_len = 10;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 120;
        lb_len = 16;
        aschunk_init_chunk(&mychunk3, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk3);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_items != (lb_len + 12)) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        num_smcc_pass_ftest++;
    } while (0);

    if (free_pool) {
        smcache_rel_cache(&cpool);
    }

    return ret;
}

int32_t test_smcc_add_4_3 (void) //add LBAs with exist bucket - cross bucket
{
    int32_t ret;

    do {
        if ((ret = test_smcc_add_4_3_1())) {
            break;
        }

        if ((ret = test_smcc_add_4_3_2())) {
            break;
        }

        if ((ret = test_smcc_add_4_3_3())) {
            break;
        }
    } while (0);
    ret = 0;

    return ret;
}

int32_t test_smcc_add_4 (void) //add LBAs with exist bucket - cross bucket
{
    int32_t ret;

    do {
        if ((ret = test_smcc_add_4_1())) {
            break;
        }

        if ((ret = test_smcc_add_4_2())) {
            break;
        }

        if ((ret = test_smcc_add_4_3())) {
            break;
        }
    } while (0);
    ret = 0;

    return ret;
}

//all offset in exist bucket no value
int32_t test_smcc_add_5_1 (void)
{
    sm_cache_pool_t cpool;
    as_chunk_t mychunk2, mychunk3;
    uint64_t lb_s;
    int32_t ret, add_ret, rel_ret, which_bk, lb_len;
    bool free_pool;

    ret = 0;

    num_smcc_run_ftest++;
    which_bk = 99;

    free_pool = false;

    do {
        smcache_init_cache(&cpool);

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE) + 127;
        lb_len = 1;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 127;
        lb_len = 32;
        aschunk_init_chunk(&mychunk3, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk3);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_items != (lb_len + 1)) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE) + 127;
        lb_len = 1;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 97;
        lb_len = 32;
        aschunk_init_chunk(&mychunk3, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk3);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }
        if (cpool.num_cache_items != (lb_len + 1)) { P_MDMGR_UTEST_FAIL("total items err\n"); break; }
        if (cpool.num_cache_buckets != 2) { P_MDMGR_UTEST_FAIL("total bk err\n"); break; }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE) + 127;
        lb_len = 1;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 120;
        lb_len = 16;
        aschunk_init_chunk(&mychunk3, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk3);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }
        if (cpool.num_cache_items != (lb_len + 1)) { P_MDMGR_UTEST_FAIL("total items err\n"); break; }
        if (cpool.num_cache_buckets != 2) { P_MDMGR_UTEST_FAIL("total bk err\n"); break; }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        num_smcc_pass_ftest++;
    } while (0);

    if (free_pool) {
        smcache_rel_cache(&cpool);
    }

    return ret;
}

//all offset in exist bucket has value
int32_t test_smcc_add_5_2 (void)
{
    sm_cache_pool_t cpool;
    as_chunk_t mychunk2, mychunk3;
    uint64_t lb_s;
    int32_t ret, add_ret, rel_ret, which_bk, lb_len;
    bool free_pool;

    ret = 0;

    num_smcc_run_ftest++;
    which_bk = 99;

    free_pool = false;

    do {
        smcache_init_cache(&cpool);

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE);
        lb_len = 31;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 127;
        lb_len = 32;
        aschunk_init_chunk(&mychunk3, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk3);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_items != lb_len) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE);
        lb_len = 1;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 97;
        lb_len = 32;
        aschunk_init_chunk(&mychunk3, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk3);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }
        if (cpool.num_cache_items != lb_len) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE);
        lb_len = 8;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 120;
        lb_len = 16;
        aschunk_init_chunk(&mychunk3, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk3);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }
        if (cpool.num_cache_items != lb_len) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        num_smcc_pass_ftest++;
    } while (0);

    if (free_pool) {
        smcache_rel_cache(&cpool);
    }

    return ret;
}

//partial offset in exist bucket has value
int32_t test_smcc_add_5_3 (void)
{
    sm_cache_pool_t cpool;
    as_chunk_t mychunk2, mychunk3;
    uint64_t lb_s;
    int32_t ret, add_ret, rel_ret, which_bk, lb_len;
    bool free_pool;

    ret = 0;

    num_smcc_run_ftest++;
    which_bk = 99;

    free_pool = false;

    do {
        smcache_init_cache(&cpool);

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE) + 10;
        lb_len = 32;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 127;
        lb_len = 32;
        aschunk_init_chunk(&mychunk3, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk3);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_items != (lb_len + 11)) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE);
        lb_len = 4;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 97;
        lb_len = 32;
        aschunk_init_chunk(&mychunk3, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk3);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }
        if (cpool.num_cache_items != (lb_len + 3)) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE);
        lb_len = 16;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 120;
        lb_len = 16;
        aschunk_init_chunk(&mychunk3, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk3);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }
        if (cpool.num_cache_items != (lb_len + 8)) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        num_smcc_pass_ftest++;
    } while (0);

    if (free_pool) {
        smcache_rel_cache(&cpool);
    }

    return ret;
}

int32_t test_smcc_add_5 (void) //add LBAs with new bucket + exist bucket (cross bucket)
{
    int32_t ret;

    do {
        if ((ret = test_smcc_add_5_1())) {
            break;
        }

        if ((ret = test_smcc_add_5_2())) {
            break;
        }

        if ((ret = test_smcc_add_5_3())) {
            break;
        }
    } while (0);
    ret = 0;

    return ret;
}

//all offset in exist bucket no value
int32_t test_smcc_add_6_1 (void)
{
    sm_cache_pool_t cpool;
    as_chunk_t mychunk1, mychunk3;
    uint64_t lb_s;
    int32_t ret, add_ret, rel_ret, which_bk, lb_len;
    bool free_pool;

    ret = 0;

    num_smcc_run_ftest++;
    which_bk = 99;
    free_pool = false;

    do {
        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE);
        lb_len = 5;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 127;
        lb_len = 32;
        aschunk_init_chunk(&mychunk3, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk3);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_items != (lb_len + 5)) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 30;
        lb_len = 10;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 97;
        lb_len = 32;
        aschunk_init_chunk(&mychunk3, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk3);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_items != (lb_len + 10)) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 5;
        lb_len = 3;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 120;
        lb_len = 16;
        aschunk_init_chunk(&mychunk3, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk3);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_items != (lb_len + 3)) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        num_smcc_pass_ftest++;
    } while (0);

    if (free_pool) {
        smcache_rel_cache(&cpool);
    }

    return ret;
}

//all offset in exist bucket no value
int32_t test_smcc_add_6_2 (void)
{
    sm_cache_pool_t cpool;
    as_chunk_t mychunk1, mychunk3;
    uint64_t lb_s;
    int32_t ret, add_ret, rel_ret, which_bk, lb_len;
    bool free_pool;

    ret = 0;

    num_smcc_run_ftest++;
    which_bk = 99;
    free_pool = false;

    do {
        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 127;
        lb_len = 1;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 127;
        lb_len = 32;
        aschunk_init_chunk(&mychunk3, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk3);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_items != lb_len) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 99;
        lb_len = 29;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 97;
        lb_len = 32;
        aschunk_init_chunk(&mychunk3, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk3);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_items != lb_len) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 118;
        lb_len = 10;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 120;
        lb_len = 16;
        aschunk_init_chunk(&mychunk3, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk3);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_items != (lb_len + 2)) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        num_smcc_pass_ftest++;
    } while (0);

    if (free_pool) {
        smcache_rel_cache(&cpool);
    }

    return ret;
}

//all offset in exist bucket no value
int32_t test_smcc_add_6_3 (void)
{
    sm_cache_pool_t cpool;
    as_chunk_t mychunk1, mychunk3;
    uint64_t lb_s;
    int32_t ret, add_ret, rel_ret, which_bk, lb_len;
    bool free_pool;

    ret = 0;

    num_smcc_run_ftest++;
    which_bk = 99;
    free_pool = false;

    do {
        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 126;
        lb_len = 1;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 126;
        lb_len = 32;
        aschunk_init_chunk(&mychunk3, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk3);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_items != lb_len) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 93;
        lb_len = 10;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 97;
        lb_len = 32;
        aschunk_init_chunk(&mychunk3, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk3);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_items != (lb_len + 4)) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 124;
        lb_len = 4;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 120;
        lb_len = 16;
        aschunk_init_chunk(&mychunk3, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk3);
        if (add_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_items != lb_len) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        num_smcc_pass_ftest++;
    } while (0);

    if (free_pool) {
        smcache_rel_cache(&cpool);
    }

    return ret;
}

int32_t test_smcc_add_6 (void) //add LBAs with exist bucket + new bucket (cross bucket)
{
    int32_t ret;

    do {
        if ((ret = test_smcc_add_6_1())) {
            break;
        }

        if ((ret = test_smcc_add_6_2())) {
            break;
        }

        if ((ret = test_smcc_add_6_3())) {
            break;
        }
    } while (0);
    ret = 0;

    return ret;
}

/******* rm unit test ********/
int32_t test_smcc_rm_1 (void) //rm LBAs with no bucket found - 1 bucket
{
    sm_cache_pool_t cpool;
    uint64_t lb_s;
    int32_t ret, rel_ret, rm_ret, which_bk, lb_len;
    bool free_pool;

    num_smcc_run_ftest++;
    ret = 0;
    which_bk = 99;
    free_pool = false;

    do {
        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 10;
        lb_len = 12;

        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 0) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 0) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 0) { P_MDMGR_UTEST_FAIL("expect free 0 bk\n"); break; }

        num_smcc_pass_ftest++;
    } while (0);

    if (free_pool) {
        smcache_rel_cache(&cpool);
    }

    return ret;
}

int32_t test_smcc_rm_2 (void) //rm LBAs with no bucket found - cross bucket
{
    sm_cache_pool_t cpool;
    uint64_t lb_s;
    int32_t ret, rel_ret, rm_ret, which_bk, lb_len;
    bool free_pool;

    num_smcc_run_ftest++;
    ret = 0;
    which_bk = 99;
    free_pool = false;

    do {
        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 120;
        lb_len = 32;

        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 0) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 0) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 0) { P_MDMGR_UTEST_FAIL("expect free 0 bk\n"); break; }

        num_smcc_pass_ftest++;
    } while (0);

    if (free_pool) {
        smcache_rel_cache(&cpool);
    }

    return ret;
}

//all offset in bucket no value
int32_t test_smcc_rm_3_1 (void)
{
    sm_cache_pool_t cpool;
    as_chunk_t mychunk1;
    uint64_t lb_s;
    int32_t ret, add_ret, rm_ret, rel_ret, which_bk, lb_len;
    bool free_pool = true;

    num_smcc_run_ftest++;
    ret = 0;

    which_bk = 99;
    do {
        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 127;
        lb_len = 1;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE);
        lb_len = 32;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 1) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
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

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 120;
        lb_len = 8;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 5) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
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

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 20;
        lb_len = 20;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 32) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) { P_MDMGR_UTEST_FAIL("expect free 0 bk\n"); break; }
        /* ----------------- end --------------- */

        num_smcc_pass_ftest++;
    } while (0);

    if (free_pool) {
        smcache_rel_cache(&cpool);
    }

    return ret;
}

//all offset in bucket has value
int32_t test_smcc_rm_3_2 (void)
{
    sm_cache_pool_t cpool;
    as_chunk_t mychunk1;
    uint64_t lb_s;
    int32_t ret, add_ret, rm_ret, rel_ret, which_bk, lb_len;
    bool free_pool = true;

    num_smcc_run_ftest++;
    ret = 0;

    which_bk = 99;
    do {
        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE);
        lb_len = 32;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE);
        lb_len = 32;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 0) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 0) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 0) { P_MDMGR_UTEST_FAIL("expect free 0 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 96;
        lb_len = 32;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 96;
        lb_len = 32;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 0) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 0) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 0) { P_MDMGR_UTEST_FAIL("expect free 0 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 66;
        lb_len = 32;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 66;
        lb_len = 32;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 0) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 0) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 0) { P_MDMGR_UTEST_FAIL("expect free 0 bk\n"); break; }
        /* ----------------- end --------------- */

        num_smcc_pass_ftest++;
    } while (0);

    if (free_pool) {
        smcache_rel_cache(&cpool);
    }

    return ret;
}

//partial offset in bucket has value
int32_t test_smcc_rm_3_3 (void)
{
    sm_cache_pool_t cpool;
    as_chunk_t mychunk1;
    uint64_t lb_s;
    int32_t ret, add_ret, rm_ret, rel_ret, which_bk, lb_len;
    bool free_pool = true;

    num_smcc_run_ftest++;
    ret = 0;

    which_bk = 99;
    do {
        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE);
        lb_len = 32;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 16;
        lb_len = 32;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 16) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
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

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 90;
        lb_len = 10;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 28) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
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

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 90;
        lb_len = 10;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 2) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        num_smcc_pass_ftest++;
    } while (0);

    if (free_pool) {
        smcache_rel_cache(&cpool);
    }

    return ret;
}

int32_t test_smcc_rm_3 (void) //rm LBAs with bucket found - 1 bucket
{
    int32_t ret;

    do {
        if ((ret = test_smcc_rm_3_1())) {
            break;
        }

        if ((ret = test_smcc_rm_3_2())) {
            break;
        }

        if ((ret = test_smcc_rm_3_3())) {
            break;
        }
    } while (0);
    ret = 0;

    return ret;
}

//rm LBAs with all bucket found - cross bucket
//all offset in 1st bucket no value, all offset in 2nd bucket no value
int32_t test_smcc_rm_4_1_1 (void)
{
    sm_cache_pool_t cpool;
    as_chunk_t mychunk1, mychunk2;
    uint64_t lb_s;
    int32_t ret, add_ret, rm_ret, rel_ret, which_bk, lb_len;
    bool free_pool = true;

    num_smcc_run_ftest++;
    ret = 0;

    which_bk = 99;
    do {
        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE);
        lb_len = 32;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE) + 127;
        lb_len = 1;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 126;
        lb_len = 32;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 33) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 0 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 77;
        lb_len = 10;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE) + 126;
        lb_len = 2;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 125;
        lb_len = 32;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 12) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 0 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 88;
        lb_len = 5;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE) + 126;
        lb_len = 2;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 125;
        lb_len = 16;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 7) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        num_smcc_pass_ftest++;
    } while (0);

    if (free_pool) {
        smcache_rel_cache(&cpool);
    }

    return ret;
}

//rm LBAs with all bucket found - cross bucket
//all offset in 1st bucket no value, all offset in 2nd bucket has value
int32_t test_smcc_rm_4_1_2 (void)
{
    sm_cache_pool_t cpool;
    as_chunk_t mychunk1, mychunk2;
    uint64_t lb_s;
    int32_t ret, add_ret, rm_ret, rel_ret, which_bk, lb_len;
    bool free_pool = true;

    num_smcc_run_ftest++;
    ret = 0;

    which_bk = 99;
    do {
        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE);
        lb_len = 32;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE);
        lb_len = 30;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 126;
        lb_len = 32;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 32) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) { P_MDMGR_UTEST_FAIL("expect free 0 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 77;
        lb_len = 10;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE);
        lb_len = 29;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 125;
        lb_len = 32;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 10) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
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

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE);
        lb_len = 13;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 125;
        lb_len = 16;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 5) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        num_smcc_pass_ftest++;
    } while (0);

    if (free_pool) {
        smcache_rel_cache(&cpool);
    }

    return ret;
}

//rm LBAs with all bucket found - cross bucket
//all offset in 1st bucket no value, partial offset in 2nd bucket has value
int32_t test_smcc_rm_4_1_3 (void)
{
    sm_cache_pool_t cpool;
    as_chunk_t mychunk1, mychunk2;
    uint64_t lb_s;
    int32_t ret, add_ret, rm_ret, rel_ret, which_bk, lb_len;
    bool free_pool = true;

    num_smcc_run_ftest++;
    ret = 0;

    which_bk = 99;
    do {
        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE);
        lb_len = 32;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE) + 10;
        lb_len = 32;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 126;
        lb_len = 32;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 44) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 0 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 77;
        lb_len = 10;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE) + 28;
        lb_len = 10;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 125;
        lb_len = 32;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 19) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 0 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 88;
        lb_len = 5;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE);
        lb_len = 14;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 125;
        lb_len = 16;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 6) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        num_smcc_pass_ftest++;
    } while (0);

    if (free_pool) {
        smcache_rel_cache(&cpool);
    }

    return ret;
}

int32_t test_smcc_rm_4_1 (void) //all offset in 1st bucket no value
{
    int32_t ret;

    do {
        if ((ret = test_smcc_rm_4_1_1())) {
            break;
        }

        if ((ret = test_smcc_rm_4_1_2())) {
            break;
        }

        if ((ret = test_smcc_rm_4_1_3())) {
            break;
        }
    } while (0);
    ret = 0;

    return ret;
}

//rm LBAs with all bucket found - cross bucket
//all offset in 1st bucket has value, all offset in 2nd bucket no value
int32_t test_smcc_rm_4_2_1 (void)
{
    sm_cache_pool_t cpool;
    as_chunk_t mychunk1, mychunk2;
    uint64_t lb_s;
    int32_t ret, add_ret, rm_ret, rel_ret, which_bk, lb_len;
    bool free_pool = true;

    num_smcc_run_ftest++;
    ret = 0;

    which_bk = 99;
    do {
        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 126;
        lb_len = 2;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE) + 127;
        lb_len = 1;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 126;
        lb_len = 32;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 1) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) { P_MDMGR_UTEST_FAIL("expect free 0 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 124;
        lb_len = 4;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE) + 126;
        lb_len = 2;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 125;
        lb_len = 32;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 3) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 0 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 123;
        lb_len = 5;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE) + 126;
        lb_len = 2;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 125;
        lb_len = 16;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 4) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        num_smcc_pass_ftest++;
    } while (0);

    if (free_pool) {
        smcache_rel_cache(&cpool);
    }

    return ret;
}

//rm LBAs with all bucket found - cross bucket
//all offset in 1st bucket has value, all offset in 2nd bucket has value
int32_t test_smcc_rm_4_2_2 (void)
{
    sm_cache_pool_t cpool;
    as_chunk_t mychunk1, mychunk2;
    uint64_t lb_s;
    int32_t ret, add_ret, rm_ret, rel_ret, which_bk, lb_len;
    bool free_pool = true;

    num_smcc_run_ftest++;
    ret = 0;

    which_bk = 99;
    do {
        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 126;
        lb_len = 2;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE);
        lb_len = 30;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 126;
        lb_len = 32;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 0) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 0) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 0) { P_MDMGR_UTEST_FAIL("expect free 0 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 124;
        lb_len = 4;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE);
        lb_len = 25;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 125;
        lb_len = 32;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 1) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) { P_MDMGR_UTEST_FAIL("expect free 0 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 123;
        lb_len = 5;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE);
        lb_len = 13;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 125;
        lb_len = 16;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 2) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        num_smcc_pass_ftest++;
    } while (0);

    if (free_pool) {
        smcache_rel_cache(&cpool);
    }

    return ret;
}

//rm LBAs with all bucket found - cross bucket
//all offset in 1st bucket has value, partial offset in 2nd bucket has value
int32_t test_smcc_rm_4_2_3 (void)
{
    sm_cache_pool_t cpool;
    as_chunk_t mychunk1, mychunk2;
    uint64_t lb_s;
    int32_t ret, add_ret, rm_ret, rel_ret, which_bk, lb_len;
    bool free_pool = true;

    num_smcc_run_ftest++;
    ret = 0;

    which_bk = 99;
    do {
        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 126;
        lb_len = 2;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE) + 10;
        lb_len = 30;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 126;
        lb_len = 32;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 10) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) { P_MDMGR_UTEST_FAIL("expect free 0 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 124;
        lb_len = 4;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE) + 5;
        lb_len = 10;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 125;
        lb_len = 32;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 1) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) { P_MDMGR_UTEST_FAIL("expect free 0 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 123;
        lb_len = 5;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE) + 12;
        lb_len = 2;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 125;
        lb_len = 16;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 3) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        num_smcc_pass_ftest++;
    } while (0);

    if (free_pool) {
        smcache_rel_cache(&cpool);
    }

    return ret;
}

int32_t test_smcc_rm_4_2 (void) //all offset in 1st bucket has value
{
    int32_t ret;

    do {
        if ((ret = test_smcc_rm_4_2_1())) {
            break;
        }

        if ((ret = test_smcc_rm_4_2_2())) {
            break;
        }

        if ((ret = test_smcc_rm_4_2_3())) {
            break;
        }
    } while (0);
    ret = 0;

    return ret;
}

//rm LBAs with all bucket found - cross bucket
//partial offset in 1st bucket has value, all offset in 2nd bucket no value
int32_t test_smcc_rm_4_3_1 (void)
{
    sm_cache_pool_t cpool;
    as_chunk_t mychunk1, mychunk2;
    uint64_t lb_s;
    int32_t ret, add_ret, rm_ret, rel_ret, which_bk, lb_len;
    bool free_pool = true;

    num_smcc_run_ftest++;
    ret = 0;

    which_bk = 99;
    do {
        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 125;
        lb_len = 2;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE) + 127;
        lb_len = 1;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 126;
        lb_len = 32;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 2) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 0 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 124;
        lb_len = 4;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE) + 126;
        lb_len = 2;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 125;
        lb_len = 32;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 3) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 0 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 123;
        lb_len = 5;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE) + 126;
        lb_len = 2;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 125;
        lb_len = 16;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 4) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        num_smcc_pass_ftest++;
    } while (0);

    if (free_pool) {
        smcache_rel_cache(&cpool);
    }

    return ret;
}

//rm LBAs with all bucket found - cross bucket
//partial offset in 1st bucket has value, all offset in 2nd bucket has value
int32_t test_smcc_rm_4_3_2 (void)
{
    sm_cache_pool_t cpool;
    as_chunk_t mychunk1, mychunk2;
    uint64_t lb_s;
    int32_t ret, add_ret, rm_ret, rel_ret, which_bk, lb_len;
    bool free_pool = true;

    num_smcc_run_ftest++;
    ret = 0;

    which_bk = 99;
    do {
        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 124;
        lb_len = 3;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE);
        lb_len = 30;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 126;
        lb_len = 32;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 2) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) { P_MDMGR_UTEST_FAIL("expect free 0 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 124;
        lb_len = 4;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE);
        lb_len = 29;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 125;
        lb_len = 32;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 1) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) { P_MDMGR_UTEST_FAIL("expect free 0 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 123;
        lb_len = 5;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE);
        lb_len = 13;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 125;
        lb_len = 16;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 2) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        num_smcc_pass_ftest++;
    } while (0);

    if (free_pool) {
        smcache_rel_cache(&cpool);
    }

    return ret;
}

//rm LBAs with all bucket found - cross bucket
//partial offset in 1st bucket has value, partial offset in 2nd bucket has value
int32_t test_smcc_rm_4_3_3 (void)
{
    sm_cache_pool_t cpool;
    as_chunk_t mychunk1, mychunk2;
    uint64_t lb_s;
    int32_t ret, add_ret, rm_ret, rel_ret, which_bk, lb_len;
    bool free_pool = true;

    num_smcc_run_ftest++;
    ret = 0;

    which_bk = 99;
    do {
        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 124;
        lb_len = 3;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE) + 10;
        lb_len = 30;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 126;
        lb_len = 32;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 12) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 0 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 124;
        lb_len = 4;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE) + 5;
        lb_len = 32;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 125;
        lb_len = 32;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 9) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 0 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 123;
        lb_len = 5;
        aschunk_init_chunk(&mychunk1, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk1);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE) + 10;
        lb_len = 10;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 125;
        lb_len = 16;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 9) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 2) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 2) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        num_smcc_pass_ftest++;
    } while (0);

    if (free_pool) {
        smcache_rel_cache(&cpool);
    }

    return ret;
}

int32_t test_smcc_rm_4_3 (void) //partial offset in 1st bucket has value
{
    int32_t ret;

    do {
        if ((ret = test_smcc_rm_4_3_1())) {
            break;
        }

        if ((ret = test_smcc_rm_4_3_2())) {
            break;
        }

        if ((ret = test_smcc_rm_4_3_3())) {
            break;
        }
    } while (0);
    ret = 0;

    return ret;
}

int32_t test_smcc_rm_4 (void) //add LBAs with exist bucket - cross bucket
{
    int32_t ret;

    do {

        if ((ret = test_smcc_rm_4_1())) {
            break;
        }

        if ((ret = test_smcc_rm_4_2())) {
            break;
        }

        if ((ret = test_smcc_rm_4_3())) {
            break;
        }
    } while (0);
    ret = 0;

    return ret;

}

int32_t test_smcc_rm_5_1 (void) //all offset in exist bucket no value
{
    sm_cache_pool_t cpool;
    as_chunk_t mychunk2;
    uint64_t lb_s;
    int32_t ret, add_ret, rel_ret, rm_ret, which_bk, lb_len;
    bool free_pool;

    ret = 0;

    num_smcc_run_ftest++;
    which_bk = 99;

    free_pool = false;

    do {
        smcache_init_cache(&cpool);

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE) + 126;
        lb_len = 2;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 126;
        lb_len = 32;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 2) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE) + 127;
        lb_len = 1;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 100;
        lb_len = 32;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 1) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE) + 127;
        lb_len = 1;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 127;
        lb_len = 32;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 1) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        num_smcc_pass_ftest++;
    } while (0);

    if (free_pool) {
        smcache_rel_cache(&cpool);
    }

    return ret;
}

int32_t test_smcc_rm_5_2 (void) //all offset in exist bucket has value
{
    sm_cache_pool_t cpool;
    as_chunk_t mychunk2;
    uint64_t lb_s;
    int32_t ret, add_ret, rel_ret, rm_ret, which_bk, lb_len;
    bool free_pool;

    ret = 0;

    num_smcc_run_ftest++;
    which_bk = 99;

    free_pool = false;

    do {
        smcache_init_cache(&cpool);

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE);
        lb_len = 30;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 126;
        lb_len = 32;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 0) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 0) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 0) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE);
        lb_len = 1;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 100;
        lb_len = 32;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 0) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 0) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 0) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE);
        lb_len = 31;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 127;
        lb_len = 32;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 0) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 0) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 0) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        num_smcc_pass_ftest++;
    } while (0);

    if (free_pool) {
        smcache_rel_cache(&cpool);
    }

    return ret;
}

int32_t test_smcc_rm_5_3 (void) //partial offset in exist bucket has value
{
    sm_cache_pool_t cpool;
    as_chunk_t mychunk2;
    uint64_t lb_s;
    int32_t ret, add_ret, rel_ret, rm_ret, which_bk, lb_len;
    bool free_pool;

    ret = 0;

    num_smcc_run_ftest++;
    which_bk = 99;

    free_pool = false;

    do {
        smcache_init_cache(&cpool);

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE) + 20;
        lb_len = 30;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 126;
        lb_len = 32;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 20) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE);
        lb_len = 10;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 100;
        lb_len = 32;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 6) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = ((which_bk + 1) << BIT_SHIFT_BKSIZE);
        lb_len = 32;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 127;
        lb_len = 32;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 1) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        num_smcc_pass_ftest++;
    } while (0);

    if (free_pool) {
        smcache_rel_cache(&cpool);
    }

    return ret;
}

int32_t test_smcc_rm_5 (void) //rm LBAs with new bucket + exist bucket (cross bucket)
{
    int32_t ret;

    do {
        if ((ret = test_smcc_rm_5_1())) {
            break;
        }

        if ((ret = test_smcc_rm_5_2())) {
            break;
        }

        if ((ret = test_smcc_rm_5_3())) {
            break;
        }
    } while (0);
    ret = 0;

    return ret;
}

int32_t test_smcc_rm_6_1 (void) //all offset in exist bucket no value
{
    sm_cache_pool_t cpool;
    as_chunk_t mychunk2;
    uint64_t lb_s;
    int32_t ret, add_ret, rel_ret, rm_ret, which_bk, lb_len;
    bool free_pool;

    ret = 0;

    num_smcc_run_ftest++;
    which_bk = 99;

    free_pool = false;

    do {
        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE);
        lb_len = 32;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 126;
        lb_len = 32;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 32) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE);
        lb_len = 1;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 100;
        lb_len = 32;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 1) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 99;
        lb_len = 10;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 127;
        lb_len = 32;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 10) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        num_smcc_pass_ftest++;
    } while (0);

    if (free_pool) {
        smcache_rel_cache(&cpool);
    }

    return ret;
}

int32_t test_smcc_rm_6_2 (void) //all offset in exist bucket has value
{
    sm_cache_pool_t cpool;
    as_chunk_t mychunk2;
    uint64_t lb_s;
    int32_t ret, add_ret, rel_ret, rm_ret, which_bk, lb_len;
    bool free_pool;

    ret = 0;

    num_smcc_run_ftest++;
    which_bk = 99;

    free_pool = false;

    do {
        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 126;
        lb_len = 2;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 126;
        lb_len = 32;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 0) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 0) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 0) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 100;
        lb_len = 28;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 100;
        lb_len = 32;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 0) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 0) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 0) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 127;
        lb_len = 1;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 127;
        lb_len = 10;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 0) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 0) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 0) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        num_smcc_pass_ftest++;
    } while (0);

    if (free_pool) {
        smcache_rel_cache(&cpool);
    }

    return ret;
}

int32_t test_smcc_rm_6_3 (void) //partial offset in exist bucket has value
{
    sm_cache_pool_t cpool;
    as_chunk_t mychunk2;
    uint64_t lb_s;
    int32_t ret, add_ret, rel_ret, rm_ret, which_bk, lb_len;
    bool free_pool;

    ret = 0;

    num_smcc_run_ftest++;
    which_bk = 99;

    free_pool = false;

    do {
        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 125;
        lb_len = 2;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 126;
        lb_len = 32;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 1) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 91;
        lb_len = 28;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 100;
        lb_len = 32;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 9) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        smcache_init_cache(&cpool);

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 123;
        lb_len = 5;
        aschunk_init_chunk(&mychunk2, 101, lb_s, lb_len, 666, 3);
        add_ret = smcache_add_aschunk(&cpool, &mychunk2);
        if (add_ret) { P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break; }

        lb_s = (which_bk << BIT_SHIFT_BKSIZE) + 127;
        lb_len = 10;
        rm_ret = smcache_rm_aschunk(&cpool, lb_s, lb_len);
        if (rm_ret) {
            P_MDMGR_UTEST_FAIL("add MD err\n"); free_pool = true; break;
        }

        if (cpool.num_cache_items != 4) {
            P_MDMGR_UTEST_FAIL("total items err\n"); free_pool = true; break;
        }
        if (cpool.num_cache_buckets != 1) {
            P_MDMGR_UTEST_FAIL("total bk err\n"); free_pool = true; break;
        }

        rel_ret = smcache_rel_cache(&cpool);
        if (rel_ret != 1) { P_MDMGR_UTEST_FAIL("expect free 1 bk\n"); break; }
        /* ----------------- end --------------- */

        num_smcc_pass_ftest++;
    } while (0);

    if (free_pool) {
        smcache_rel_cache(&cpool);
    }

    return ret;
}

int32_t test_smcc_rm_6 (void) //add LBAs with exist bucket + new bucket (cross bucket)
{
    int32_t ret;

    do {
        if ((ret = test_smcc_rm_6_1())) {
            break;
        }

        if ((ret = test_smcc_rm_6_2())) {
            break;
        }

        if ((ret = test_smcc_rm_6_3())) {
            break;
        }
    } while (0);
    ret = 0;

    return ret;
}
#endif

/*
 * normal functionality test
 */
int32_t sm_chunkcache_ftest (void)
{
#ifdef DISCO_PREALLOC_SUPPORT
    num_smcc_total_ftest = 43;
    test_smcc_init_rel();
    test_smcc_simple_add();
    test_smcc_simple_rm();
    test_smcc_add_1();
    test_smcc_add_2();
    test_smcc_add_3();
    test_smcc_add_4();
    test_smcc_add_5();
    test_smcc_add_6();

    test_smcc_rm_1();
    test_smcc_rm_2();
    test_smcc_rm_3();
    test_smcc_rm_4();
    test_smcc_rm_5();
    test_smcc_rm_6();

    test_smscc_find_run();
#endif

    return 0;
}

void sm_chunkcache_ftest_get_result (int32_t *total, int32_t *run, int32_t *fail)
{
    uint32_t find_all, find_run, find_pass;

    test_smscc_find_get_result(&find_all, &find_run, &find_pass);

    num_smcc_total_ftest += find_all;
    num_smcc_run_ftest += find_run;
    num_smcc_pass_ftest += find_pass;

    *total += num_smcc_total_ftest;
    *run += num_smcc_run_ftest;
    *fail += num_smcc_pass_ftest;
}

/*
 * error injection test
 */
int32_t sm_chunkcache_errinjtest (void)
{
    num_smcc_total_eijtest = 0;
    num_smcc_run_eijtest = 0;
    num_smcc_pass_eijtest = 0;

    return 0;
}

void sm_chunkcache_eijtest_get_result (int32_t *total, int32_t *run, int32_t *fail)
{
    *total += num_smcc_total_eijtest;
    *run += num_smcc_run_eijtest;
    *fail += num_smcc_pass_eijtest;
}

int32_t sm_chunkcache_alltest (void)
{
    int32_t ret;

    do {
        if ((ret = sm_chunkcache_ftest())) {
            P_MDMGR_UTEST_FAIL("fail exec cm chunk cache functionality utest\n");
            break;
        }

        if ((ret = sm_chunkcache_errinjtest())) {
            P_MDMGR_UTEST_FAIL("fail exec cm chunk cache error inject utest\n");
            break;
        }

    } while (0);

    return ret;
}

void sm_chunkcache_alltest_get_result (int32_t *total, int32_t *run, int32_t *fail)
{
    sm_chunkcache_ftest_get_result(total, run, fail);
    sm_chunkcache_eijtest_get_result(total, run, fail);
}

int32_t sm_chunkcache_reset_test (void)
{
    int32_t ret;

    ret = 0;

    num_smcc_total_ftest = 0;
    num_smcc_run_ftest = 0;
    num_smcc_pass_ftest = 0;

    num_smcc_total_eijtest = 0;
    num_smcc_run_eijtest = 0;
    num_smcc_pass_eijtest = 0;

    test_smscc_find_reset();

    return ret;
}

int32_t sm_chunkcache_init_test (void)
{
    dms_radix_tree_init();
    sm_chunkcache_reset_test();
    test_smscc_find_init();

    return 0;
}

void sm_chunkcache_rel_test (void)
{
    dms_radix_tree_exit();
    return;
}
