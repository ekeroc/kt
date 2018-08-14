/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * procfs.h
 *
 */
#ifndef _PROCFS_OLD_H_
#define _PROCFS_OLD_H_

extern void init_procfs(struct proc_dir_entry *proc_ccma);
extern void release_procfs(struct proc_dir_entry *proc_ccma);

extern int32_t create_vol_proc(uint32_t volume_id);
extern int32_t delete_vol_proc(uint32_t volume_id);

#endif /* _PROCFS_OLD_H_ */
