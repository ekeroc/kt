KDIR 	 = /lib/modules/$(shell uname -r)/build
CFLAGS += -Wall
CFILES = umain_cdev.c ../src/kdbg_cdev_cbf_tbl.c

obj-m += ucdev.o
ucdev-objs := $(CFILES:.c=.o)

all:
	make $(CFLAGS) -C $(KDIR) M=$(PWD)

clean:
	make -C $(KDIR) M=$(PWD) clean
