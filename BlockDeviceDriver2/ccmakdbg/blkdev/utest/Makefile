KDIR 	 = /lib/modules/$(shell uname -r)/build
CFLAGS += -Wall
CFILES = umain_blkdev.c ../src/kdbg_blkdev_if.c ../src/kdbg_blkdev_qm.c ../src/kdbg_blkdev_knl.c

obj-m += ublkdev.o
ublkdev-objs := $(CFILES:.c=.o)

all:
	make $(CFLAGS) -C $(KDIR) M=$(PWD)

clean:
	make -C $(KDIR) M=$(PWD) clean
