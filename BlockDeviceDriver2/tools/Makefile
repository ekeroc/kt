# Makefile
.PHONY:clean

DMS_C_INCLUDE= -I/usr/include
DMS_C_CFLAGS= -Wall -g $(DMS_C_INCLUDE) -lpthread
DMS_C_MYFLAGS = -DDMSC_USER_DAEMON=1

DMS_C_SRCS = socket_util.c common_util.c Volume_Manager.c Metadata_Manager.c nn_cmd.c cn_cmd.c discoadm.c 
DMS_C_OBJS = socket_util.o common_util.o Volume_Manager.o Metadata_Manager.o nn_cmd.o cn_cmd.o discoadm.o

##### CCMA Faked Node for testing #####
CCMA_FAKED_NODE_SUBDIRS  = faked_foreign_system

#################################
#	daemon on user space	#
#################################

all:
	gcc -c $(DMS_C_SRCS) $(DMS_C_MYFLAGS) $(DMS_C_INCLUDE)
	gcc -o "discoadm" $(DMS_C_OBJS) $(DMS_C_CFLAGS)	

clean:
	rm -rf *.o
