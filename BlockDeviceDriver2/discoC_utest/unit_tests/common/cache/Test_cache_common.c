/*
 *
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * Test_cache_common.c
 *
 * Provide common function for testing cache component
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/slab.h>

void simple_free_fn (void *metadata_item) {
    kfree(metadata_item);
    return;
}
