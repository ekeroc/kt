KDIR 	 = /lib/modules/$(shell uname -r)/build
CFLAGS += -Wall
CFILES = umain_disk.c ../src/ccmakdbg_disk_if.c ../src/ccmakdbg_disk_qm.c ../src/ccmakdbg_disk_knl.c

obj-m += udisk.o
udisk-objs := $(CFILES:.c=.o)

all:
	make $(CFLAGS) -C $(KDIR) M=$(PWD)

clean:
	make -C $(KDIR) M=$(PWD) clean
