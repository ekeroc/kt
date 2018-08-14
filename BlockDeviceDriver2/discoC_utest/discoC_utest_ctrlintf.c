/*
 *
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_utest_ctrlintf.c
 *
 * This module provide a linux-proc based control interface for discoC unit test
 */

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include "common.h"
#include "discoC_utest_ctrlintf_private.h"
#include "discoC_utest_ctrlintf.h"
#include "discoC_utest_cmd_handler.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
static int ctrlintf_read (char *buffer, char **buffer_location,
                 off_t offset, int nbytes, int *eof, void *data)
#else
static ssize_t ctrlintf_read (struct file *file, char __user *buffer,
        size_t nbytes, loff_t *ppos)
#endif
{
    uint32_t len, cp_len;
    int8_t *local_buf;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
    int32_t ret;
    static uint32_t finished = 0;

    if (finished) {
        finished = 0;
        return 0;
    }
#endif

    local_buf = kmalloc(sizeof(int8_t)*PAGE_SIZE, GFP_NOWAIT | GFP_ATOMIC);
    len = get_test_cmd_result(local_buf, PAGE_SIZE);
    cp_len = len > nbytes ? nbytes : len;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
    ret = copy_to_user(buffer, local_buf, cp_len);
    finished = 1;
#else
    memcpy(buffer, local_buf, cp_len);
#endif

    kfree(local_buf);

    return cp_len;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
static int ctrlintf_write (struct file *file, const char *buffer,
        unsigned long count, void *data)
#else
static ssize_t ctrlintf_write (struct file *file, const char __user *buffer,
        size_t count, loff_t *ppos)
#endif
{
    int8_t local_buf[MAX_CMD_LEN];
    uint32_t cp_len;

    memset(local_buf, '\0', MAX_CMD_LEN);
    cp_len = MAX_CMD_LEN > count ? count : MAX_CMD_LEN;
    if (copy_from_user(local_buf, buffer, cp_len)) {
        return -EFAULT;
    }

    if (exec_test_cmd(local_buf, cp_len)) {
        printk("DMSC UTEST fail execute test command: %s\n", local_buf);
    }

    return count;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
const struct file_operations utest_ctrlintf_fops = {
.owner = THIS_MODULE,
.write = ctrlintf_write,
.read = ctrlintf_read,
};
#endif

int32_t init_utest_ctrlintf (void)
{
    struct proc_dir_entry *proc_entry;
    int32_t ret;

    printk("DMSC UTEST init discoC unit test control interface\n");
    ret = 0;
    utest_dir = proc_mkdir(nm_procDir_utest, NULL);

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
    proc_entry = create_proc_entry(nm_procFN_test, 0, utest_dir);
    if (!IS_ERR_OR_NULL(proc_entry)) {
        proc_entry->read_proc = ctrlintf_read;
        proc_entry->write_proc = ctrlintf_write;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
        proc_entry->owner = THIS_MODULE;
#endif
    } else {
        ret = -ENOMEM;
    }
#else
    proc_entry = proc_create(nm_procFN_test, 0, utest_dir, &utest_ctrlintf_fops);
    if (IS_ERR_OR_NULL(proc_entry)) {
        ret = -ENOMEM;
    }
#endif

    printk("DMSC UTEST init discoC unit test control interface DONE ret %d\n", ret);
    return ret;
}

void rel_utest_ctrlintf (void)
{
    printk("DMSC UTEST release discoC unit test control interface\n");

    remove_proc_entry(nm_procFN_test, utest_dir);
    remove_proc_entry(nm_procDir_utest, NULL);

    printk("DMSC UTEST release discoC unit test control interface DONE\n");
}
