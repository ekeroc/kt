/*
 *
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * common.h
 *
 */
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/err.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
long __must_check IS_ERR_OR_NULL (__force const void *ptr)
{
    return !ptr || IS_ERR_VALUE((unsigned long)ptr);
}
#endif
