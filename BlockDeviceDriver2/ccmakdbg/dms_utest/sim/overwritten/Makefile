
CFILES = src/dms_utest_sim_ow.c
obj-m = dms_utest_sim_ow.o
dms_utest_sim_ow-objs := $(CFILES:.c=.o)

kernel_version:=$(subst xen,, $(shell uname -r))
kernel_version:=$(strip $(kernel_version))
xen_kernel_version:=$(kernel_version)xen

install:
	$(MAKE) -C/lib/modules/$(shell uname -r)/build M=$(PWD)	
xen:
	$(MAKE) -C/lib/modules/"$(xen_kernel_version)"/build M=$(PWD)
nonxen:
	$(MAKE) -C/lib/modules/"$(kernel_version)"/build M=$(PWD)
clean:
	rm -rf *.o *.ko *.symvers *.mod.c .*.cmd Module.markers modules.order .tmp_versions version.h
	(cd src && (rm -rf *.o .*.cmd ));
