ccmablk-objs += info_monitor/dms_info.o info_monitor/dms_sysinfo.o vdisk_manager/volume_manager.o
ccmablk-objs += flowcontrol/FlowControl.o housekeeping.o SelfTest.o
ccmablk-objs += drv_fsm.o drv_chrdev.o drv_blkdev.o drv_main.o

ccmablk-objs += dms_module_init.o
obj-m := ccmablk.o

