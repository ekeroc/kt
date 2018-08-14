/*
 *
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * test_target.c
 *
 */
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/errno.h>

int32_t my_fun_A (int32_t val)
{
    int32_t ret;

    ret = 0;

    do {
        if (val > 100) {
            printk("DMSC simple function reject val > 100\n");
            ret = -EINVAL;
            break;
        }

        if (val <= 0) {
            printk("DMSC simple function reject val <= 0\n");
            ret = -EINVAL;
            break;
        }
    } while (0);

    return ret;
}


