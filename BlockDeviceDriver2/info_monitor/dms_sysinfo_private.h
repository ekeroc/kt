/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * procfs_private.h
 *
 */

#ifndef _PROCFS_PRIVATE_H_
#define _PROCFS_PRIVATE_H_

#define MAX_LINE 1
#define VOL_ID_LEN 32

#define NAME_PROC_CCMA  "ccma"
#define NAME_PROC_SYS_MON   "sys_monitor"
#define NAME_PROC_SYS_VOL   "sys_volume"

struct proc_dir_entry *proc_ccma, *proc_sys_monitor, *proc_sys_vol_dir;

#endif /* _PROCFS_PRIVATE_H_ */
