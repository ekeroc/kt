#ifndef _TEST_PROC_V2_H_
#define _TEST_PROC_V2_H_

extern void create_sys_vol_proc(uint32_t volume_id);
extern void delete_sys_vol_proc(uint32_t volume_id);
extern void init_sys_procfs(void);
extern void release_sys_procfs(void);

#endif /* _TEST_PROC_FS_H_ */
