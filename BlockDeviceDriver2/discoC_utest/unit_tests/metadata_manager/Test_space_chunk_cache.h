/*
 *
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * Test_space_chunk_cache.h
 *
 */

#ifndef DISCOC_UTEST_UNIT_TESTS_METADATA_MANAGER_TEST_SPACE_CHUNK_CACHE_H_
#define DISCOC_UTEST_UNIT_TESTS_METADATA_MANAGER_TEST_SPACE_CHUNK_CACHE_H_

extern int32_t sm_chunkcache_ftest(void);
extern void sm_chunkcache_ftest_get_result(int32_t *total, int32_t *run, int32_t *fail);
extern int32_t sm_chunkcache_errinjtest(void);
extern void sm_chunkcache_eijtest_get_result(int32_t *total, int32_t *run, int32_t *fail);
extern int32_t sm_chunkcache_alltest (void);
extern void sm_chunkcache_alltest_get_result(int32_t *total, int32_t *run, int32_t *fail);

extern int32_t sm_chunkcache_reset_test(void);
extern int32_t sm_chunkcache_init_test(void);
extern void sm_chunkcache_rel_test(void);

#endif /* DISCOC_UTEST_UNIT_TESTS_METADATA_MANAGER_TEST_SPACE_CHUNK_CACHE_H_ */
