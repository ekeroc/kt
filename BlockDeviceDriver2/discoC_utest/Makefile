# Makefile
.PHONY:clean

discoC_utest-objs := common.o discoC_utest_ctrlintf.o discoC_utest_cmd_handler.o discoC_utest_manager.o
discoC_utest-objs += discoC_utest_module_init.o

obj-m := discoC_utest.o

all:
	$(MAKE) -C/lib/modules/"$(shell uname -r)"/build M=$(PWD)
	
clean:
	rm -rf *.o *.ko *.symvers *.mod.c .*.cmd Module.markers modules.order .tmp_versions version.h
