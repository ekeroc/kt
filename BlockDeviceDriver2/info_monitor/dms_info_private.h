/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * procfs_private.h
 */

#ifndef _PROCFS_OLD_PRIVATE_H
#define _PROCFS_OLD_PRIVATE_H

#define NAME_PROC_MONITOR   "monitor"
#define NAME_PROC_RES       "resources"
#define NAME_PROC_IOLIST    "io_pending_list_content"
#define NAME_PROC_FDNLIST    "fdn_pending_list_content"
#define NAME_PROC_CONFIG    "config"
#define NAME_PROC_THREAD    "thread_state"
#define NAME_PROC_PREALLOC    "prealloc"

#define NAME_PROC_VOL   "volume"
struct proc_dir_entry *proc_entry;
struct proc_dir_entry *proc_volume;

#endif	/* _PROCFS_OLD_PRIVATE_H */
