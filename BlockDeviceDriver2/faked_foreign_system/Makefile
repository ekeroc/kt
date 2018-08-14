# Makefile
.PHONY:clean

DMS_NN_SRCS = utility.c socket_util.c NameNode_Protocol_Stack.c NN_Matadata_Manager.c NN_Volume_Manager.c Faked_NameNode.c
DMS_NN_OBJS = utility.o socket_util.o NameNode_Protocol_Stack.o NN_Matadata_Manager.o NN_Volume_Manager.o Faked_NameNode.o

DMS_DN_SRCS = utility.c socket_util.c DataNode_Protocol_Stack.c DataNode_Data_Manager.c Faked_DataNode.c
DMS_DN_OBJS = utility.o socket_util.o DataNode_Protocol_Stack.o DataNode_Data_Manager.o Faked_DataNode.o

DMS_TEST_SRCS = socket_util.c dms_test_tool.c
DMS_TEST_OBJS = socket_util.o dms_test_tool.o

#DMS_C_INCLUDE= -I/usr/include
#DMS_C_LIB=    -Wl,/usr/cloudos/common/lib/rs/libccma_c_rs_api.so

DMS_C_CFLAGS= -Wall -g $(DMS_C_INCLUDE) $(DMS_C_LIB) -lpthread

#################################
#	daemon on user space	#
#################################
all:
	gcc -c $(DMS_NN_SRCS)
	gcc -o "FakeNN" $(DMS_NN_OBJS) -Wall -g -I/usr/include -lpthread
	rm -rf *.o
	gcc -c $(DMS_DN_SRCS)
	gcc -o "FakeDN" $(DMS_DN_OBJS) -Wall -g -I/usr/include -lpthread
	rm -rf *.o
	gcc -c $(DMS_TEST_SRCS)
	gcc -o "dms_tool" $(DMS_TEST_OBJS) -Wall -g -I/usr/include -lpthread
	
clean:
	rm -rf *.o
	rm -rf FakeNN
	rm -rf FakeDN
	rm -rf dms_tool
	
