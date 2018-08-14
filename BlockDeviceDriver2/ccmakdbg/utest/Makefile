MODULE_NAME = ccmakdbg 
CFILES = umain_ccmakdbg.c ../src/ccmakdbg_if.c
SUBDIRS = ../cdev ../dms_utest 

obj-m += ccmakdbg.o
ccmakdbg-objs := $(CFILES:.c=.o)
ccmakdbg-objs += ../cdev/cdev.o ../dms_utest/dms_utest.o

kernel_version:=$(subst xen,, $(shell uname -r))
kernel_version:=$(strip $(kernel_version))
xen_kernel_version:=$(kernel_version)xen

install:
	for d in $(SUBDIRS); do \
		(cd $$d && make install); \
    done
	$(MAKE) -C/lib/modules/$(shell uname -r)/build M=$(PWD)	

xen:
	for d in $(SUBDIRS); do \
		(cd $$d && make xen); \
    done
	$(MAKE) -C/lib/modules/"$(xen_kernel_version)"/build M=$(PWD)
	
nonxen:
	for d in $(SUBDIRS); do \
		(cd $$d && make nonxen); \
    done
	$(MAKE) -C/lib/modules/"$(kernel_version)"/build M=$(PWD)

clean:
	rm -rf *.o *.ko *.symvers *.mod.c .*.cmd Module.markers modules.order .tmp_versions version.h
	for d in $(SUBDIRS); do \
		(cd $$d && make clean); \
    done	
