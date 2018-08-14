/*
 *
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * Test_metadata_manager_mock_fn.c
 *
 * mock non-necessary function
 *
 * NOTE: if you need this function at future, prevent compile it
 */
#include <linux/kernel.h>
#include <linux/version.h>

/*
 * TODO: use marco to wrap
 */

/** common module **/
bool is_sys_free_mem_low (void)
{
    return false;
}

/** metadata manager **/
int32_t get_asChunkMemPoolID (void)
{
    return 0;
}


