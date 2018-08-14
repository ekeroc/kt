
CFILES = src/dms_utest_main.c
SUBDIRS = modules sim
obj-m := dms_utest.o
dms_utest-objs := $(CFILES:.c=.o)
dms_utest-objs += modules/dms_utest_modules.o	sim/dms_utest_sim.o	

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
	(cd src && (rm -rf *.o .*.cmd ));
	for d in $(SUBDIRS); do \
		(cd $$d && make clean); \
    done	
