/*
 *
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * Test_metadata_manager_module.c
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/version.h>
#include "Test_metadata_manager.h"

#define VERSION_STR          "1.0.1"
#define COPYRIGHT            "Copyright 2017 F division/ICL/ITRI"
#define DRIVER_AUTHOR        "Storage team"
#define DRIVER_DESC          "Unit test of metadata_manager"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
#define PARAM_PERMISSION 0644
#else
#define PARAM_PERMISSION ( S_ISUID | S_ISGID | S_IRUSR | S_IRGRP )
#endif

static int __init init_test_module (void)
{
    return init_test_metadata_mgr();
}

static void __exit exit_test_module (void)
{
    rel_test_metadata_mgr();
}

module_init(init_test_module);
module_exit(exit_test_module);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_VERSION(VERSION_STR);
MODULE_INFO(Copyright, COPYRIGHT);
