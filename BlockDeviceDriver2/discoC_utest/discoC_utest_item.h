/*
 *
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_utest_item.h
 *
 */

#ifndef DISCOC_UTEST_ITEM_H_
#define DISCOC_UTEST_ITEM_H_

#define UTEST_NM_LEN    64
typedef struct discoC_utest_item {
    int8_t utest_item_nm[UTEST_NM_LEN];
    uint32_t utest_id;
    utest_reset *utest_reset_fn;
    utest_config *utest_config_fn;
    utest_run *utest_run_fn;
    utest_cancel *utest_cancel_fn;
    utest_get_result *utest_getRes_fn;

    struct list_head list2pool;
    atomic_t utItem_refcnt;
} utest_item_t;

extern utest_item_t *reference_utest_item(int8_t *utest_nm);
extern void dereference_utest_item(utest_item_t *myItem);
extern int32_t show_all_utest(int8_t *buffer, int32_t buff_size);
#endif /* DISCOC_UTEST_ITEM_H_ */
