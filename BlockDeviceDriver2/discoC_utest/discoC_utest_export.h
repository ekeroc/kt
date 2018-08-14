/*
 *
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_utest_export.h
 *
 */

#ifndef DISCOC_UTEST_EXPORT_H_
#define DISCOC_UTEST_EXPORT_H_

typedef int32_t (utest_reset)(void);
typedef int32_t (utest_config)(int8_t *cfg_str);
typedef int32_t (utest_run)(int8_t *test_nm);
typedef int32_t (utest_cancel)(int8_t *test_nm);
typedef int32_t (utest_get_result)(int32_t *total_test, int32_t *run_test, int32_t *pass_test);

extern int32_t register_utest(int8_t *utest_nm, utest_reset *reset_fn, utest_config *cfg_fn,
        utest_run *run_fn, utest_cancel *cancel_fn, utest_get_result *get_ret_fn);
extern int32_t deregister_utest(int8_t *utest_nm);

#endif /* DISCOC_UTEST_EXPORT_H_ */
