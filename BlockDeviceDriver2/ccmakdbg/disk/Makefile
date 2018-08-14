CFILES = src/ccmakdbg_disk_if.c src/ccmakdbg_disk_qm.c src/ccmakdbg_disk_knl.c

obj-m += disk.o
disk-objs := $(CFILES:.c=.o)

install:
	$(MAKE) -C/lib/modules/$(shell uname -r)/build M=$(PWD)	
xen:
	$(MAKE) -C/lib/modules/"$(xen_kernel_version)"/build M=$(PWD)	
nonxen:
	$(MAKE) -C/lib/modules/"$(kernel_version)"/build M=$(PWD)
clean:
	rm -rf *.o *.ko *.symvers *.mod.c .*.cmd Module.markers modules.order .tmp_versions version.h
	(cd src && (rm -rf *.o .*.cmd ));
