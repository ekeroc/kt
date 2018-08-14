/*
 *
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_utest_module_init.c
 *
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/version.h>
#include "discoC_utest_manager.h"

#define VERSION_STR          "1.0.1"
#define COPYRIGHT            "Copyright 2017 Storage team/F division/ICL/ITRI"
#define DRIVER_AUTHOR        "CCMA Storage team"
#define DRIVER_DESC          "DISCO Clientnode unit test manager kernel module"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
#define PARAM_PERMISSION 0644
#else
#define PARAM_PERMISSION ( S_ISUID | S_ISGID | S_IRUSR | S_IRGRP )
#endif


static int __init init_discoC_utest_module (void)
{
    return init_discoC_utest_mgr();
}

static void __exit exit_discoC_utest_module (void) {
    rel_discoC_utest_mgr();
}

module_init(init_discoC_utest_module);
module_exit(exit_discoC_utest_module);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_VERSION(VERSION_STR);
MODULE_INFO(Copyright, COPYRIGHT);
