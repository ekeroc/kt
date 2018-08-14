/*
 *
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_utest_manager.c
 *
 */
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include "common.h"
#include "discoC_utest_ctrlintf.h"
#include "discoC_utest_export.h"
#include "discoC_utest_item.h"
#include "discoC_utest_cmd_handler.h"
#include "discoC_utest_manager_private.h"
#include "discoC_utest_manager.h"

static uint32_t _get_next_utest_ID (utest_mgr_t *myMgr)
{
    return (uint32_t)atomic_add_return(1, &myMgr->utest_ID_gen);
}

static utest_item_t *_get_utest_pool_lockless (utest_mgr_t *myMgr,
        int8_t *utest_nm)
{
    utest_item_t *cur, *next, *myItem;

    myItem = NULL;
    list_for_each_entry_safe (cur, next, &myMgr->utest_pool, list2pool) {
        if (IS_ERR_OR_NULL(cur)) {
            printk("DMSC UTEST utest pool list broken\n");
            break;
        }

        if (strcmp(cur->utest_item_nm, utest_nm)) {
            continue;
        }

        myItem = cur;
        break;
    }

    return myItem;
}

static int32_t _add_utest_pool (utest_mgr_t *myMgr, utest_item_t *myItem)
{
    int32_t ret;

    ret = 0;

    down_write(&myMgr->pool_lock);

    if (IS_ERR_OR_NULL(_get_utest_pool_lockless(myMgr, myItem->utest_item_nm))) {
        myItem->utest_id = _get_next_utest_ID(myMgr);
        list_add_tail(&myItem->list2pool, &myMgr->utest_pool);
        myMgr->num_utest_module++;
    } else {
        ret = -EEXIST;
    }

    up_write(&myMgr->pool_lock);

    return ret;
}

static void _init_utest_item (utest_item_t *myItem)
{
    memset(myItem->utest_item_nm, '\0', UTEST_NM_LEN);
    INIT_LIST_HEAD(&myItem->list2pool);
    myItem->utest_id = 0;
    myItem->utest_reset_fn = NULL;
    myItem->utest_config_fn = NULL;
    myItem->utest_run_fn = NULL;
    myItem->utest_cancel_fn = NULL;
    myItem->utest_getRes_fn = NULL;
    atomic_set(&myItem->utItem_refcnt, 0);
}

static int32_t _add_utest_item (utest_mgr_t *myMgr, int8_t *utest_nm,
        utest_reset *reset_fn, utest_config *cfg_fn, utest_run *run_fn,
        utest_cancel *cancel_fn, utest_get_result *get_ret_fn)
{
    utest_item_t *myItem;
    int32_t ret, len_cp;

    ret = 0;
    do {
        myItem = kmalloc(sizeof(utest_item_t), GFP_KERNEL);
        if (IS_ERR_OR_NULL(myItem)) {
            printk("DMSC UTEST fail alloc mem for test item %s\n", utest_nm);
            ret = -ENOMEM;
            break;
        }

        _init_utest_item(myItem);
        len_cp = strlen(utest_nm) > UTEST_NM_LEN ? UTEST_NM_LEN : strlen(utest_nm);
        strcpy(myItem->utest_item_nm, utest_nm);
        myItem->utest_reset_fn = reset_fn;
        myItem->utest_config_fn = cfg_fn;
        myItem->utest_run_fn = run_fn;
        myItem->utest_cancel_fn = cancel_fn;
        myItem->utest_getRes_fn = get_ret_fn;

        if (_add_utest_pool(myMgr, myItem)) {
            printk("DMSC UTEST add item fail duplicate item %s\n", utest_nm);
            kfree(myItem);
            ret = -EEXIST;
        }
    } while (0);

    return ret;
}

static utest_item_t *_rm_utest_pool (utest_mgr_t *myMgr, int8_t *utest_nm)
{
    utest_item_t *myItem;

    down_write(&myMgr->pool_lock);

    myItem = _get_utest_pool_lockless(myMgr, utest_nm);
    if (!IS_ERR_OR_NULL(myItem)) {
        list_del(&myItem->list2pool);
        myMgr->num_utest_module--;
    }

    up_write(&myMgr->pool_lock);

    return myItem;
}

static int32_t _free_utest_item (utest_item_t *myItem)
{
#define CHK_REF_INTERVAL    100 //100 milli-second
#define MAX_CHK_CNT         100 //100 * 100 = 10 seconds
    int32_t ret, i;

    ret = 0;

    for (i = 0; i < MAX_CHK_CNT; i++) {
        if (atomic_read(&myItem->utItem_refcnt) == 0) {
            kfree(myItem);
            myItem = NULL;
            break;
        }

        ret = -EPERM;

        if (i != 0 && i % 10 == 0) {
            printk("DMSC UTEST fail free utest item %s be ref wait cnt %d\n",
                    myItem->utest_item_nm, i);
        }
        msleep(CHK_REF_INTERVAL);
    }

    return ret;
}

/*
 * NOTE: currently, we don't check utest running state. Will remove immediately.
 */
static int32_t _rm_utest_item (utest_mgr_t *myMgr, int8_t *utest_nm)
{
    utest_item_t *myItem;
    int32_t ret;

    ret = 0;
    myItem = _rm_utest_pool(myMgr, utest_nm);
    if (!IS_ERR_OR_NULL(myItem)) {
        ret = _free_utest_item(myItem);
    } else {
        ret = -ENOENT;
    }

    return ret;
}

utest_item_t *reference_utest_item (int8_t *utest_nm)
{
    utest_item_t *myItem;

    down_read(&discoC_utestMgr.pool_lock);
    myItem = _get_utest_pool_lockless(&discoC_utestMgr, utest_nm);
    if (!IS_ERR_OR_NULL(myItem)) {
        atomic_inc(&myItem->utItem_refcnt);
    }
    up_read(&discoC_utestMgr.pool_lock);

    return myItem;
}

void dereference_utest_item (utest_item_t *myItem)
{
    down_read(&discoC_utestMgr.pool_lock);
    atomic_dec(&myItem->utItem_refcnt);
    up_read(&discoC_utestMgr.pool_lock);
}

int32_t show_all_utest (int8_t *buffer, int32_t buff_size)
{
    utest_item_t *cur, *next;
    int32_t curr_len;

    curr_len = 0;
    down_read(&discoC_utestMgr.pool_lock);
    list_for_each_entry_safe (cur, next, &discoC_utestMgr.utest_pool, list2pool) {
        if (IS_ERR_OR_NULL(cur)) {
            printk("DMSC UTEST utest pool broken when show all\n");
            break;
        }

        curr_len = sprintf(buffer + curr_len, "%d %s\n", cur->utest_id, cur->utest_item_nm);
        if (buff_size - curr_len <= 80) {
            break;
        }
    }
    up_read(&discoC_utestMgr.pool_lock);

    return curr_len;
}

int32_t register_utest (int8_t *utest_nm, utest_reset *reset_fn, utest_config *cfg_fn,
        utest_run *run_fn, utest_cancel *cancel_fn, utest_get_result *get_ret_fn)
{
    printk("DMSC UTEST add utest item %s\n", utest_nm);

    return _add_utest_item(&discoC_utestMgr, utest_nm, reset_fn, cfg_fn, run_fn,
            cancel_fn, get_ret_fn);
}
EXPORT_SYMBOL(register_utest);

int32_t deregister_utest (int8_t *utest_nm)
{
    printk("DMSC UTEST remove utest item %s\n", utest_nm);
    return _rm_utest_item(&discoC_utestMgr, utest_nm);
}
EXPORT_SYMBOL(deregister_utest);

static int32_t _init_utest_mgr (utest_mgr_t *myMgr)
{
    INIT_LIST_HEAD(&myMgr->utest_pool);
    myMgr->num_utest_module = 0;
    init_rwsem(&myMgr->pool_lock);
    atomic_set(&myMgr->utest_ID_gen, 0);

    return 0;
}

int32_t init_discoC_utest_mgr (void)
{
    int32_t ret;

    printk("DMSC UTEST init discoC unit test manager\n");

    ret = 0;

    do {
        if ((ret = init_utest_ctrlintf())) {
            break;
        }

        if ((ret = init_cmd_handler())) {
            break;
        }

        if ((ret = _init_utest_mgr(&discoC_utestMgr))) {
            break;
        }
    } while (0);

    printk("DMSC UTEST init discoC unit test manager DONE ret %d\n", ret);

    return ret;
}

static void _rel_utest_mgr (utest_mgr_t *myMgr)
{
    utest_item_t *cur, *next;

    down_write(&myMgr->pool_lock);
    list_for_each_entry_safe (cur, next, &myMgr->utest_pool, list2pool) {
        if (IS_ERR_OR_NULL(cur)) {
            printk("DMSC UTEST utest pool broken when rel module\n");
            break;
        }

        list_del(&cur->list2pool);
        myMgr->num_utest_module--;
        kfree(cur);
    }
    up_write(&myMgr->pool_lock);

    if (myMgr->num_utest_module != 0) {
        printk("DMSC UTEST fail clean utest item remaing %d\n",
                myMgr->num_utest_module);
    }
}

void rel_discoC_utest_mgr(void)
{
    printk("DMSC UTEST release discoC unit test manager\n");

    rel_utest_ctrlintf();
    _rel_utest_mgr(&discoC_utestMgr);

    printk("DMSC UTEST release discoC unit test manager DONE\n");
}
