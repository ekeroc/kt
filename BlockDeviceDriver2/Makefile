# Makefile
.PHONY:clean

#################################
#	Global 	configs		#
#################################

kernel_version:=$(subst xen,, $(shell uname -r))
kernel_version:=$(strip $(kernel_version))
xen_kernel_version:=$(kernel_version)xen

xenDir="/lib/modules/$(xen_kernel_version)/build"

CloudOS2_0_Ubuntu_kernel_version:=3.2.0-23-generic
CloudOS2_0_CentOS_kernel_version:=3.2.0-rc1
CloudOS1_0_CentOS_kernel_version:=2.6.18-194.8.1.el5

kernel_module_root_dir:="/lib/modules/"
kernel_module_list:=${shell find ${kernel_module_root_dir} -mindepth 1 -maxdepth 1 -type d -print}
tempDir:=build-module-temp
#################################
#	daemon 	configs		#
#################################

ifeq ($(kernel_version), $(CloudOS2_0_Ubuntu_kernel_version))
	DMS_C_CFLAGS= -Wall -g $(DMS_C_LIB) -lpthread
	DMS_C_MYFLAGS = -DDMSC_USER_DAEMON=1 -DCLOUDOS_VERSION=2
else
	ifeq ($(kernel_version), $(CloudOS2_0_CentOS_kernel_version))
		DMS_C_INCLUDE= -I/usr/include
		DMS_C_CFLAGS= -Wall -g $(DMS_C_INCLUDE) $(DMS_C_LIB) -lpthread
		DMS_C_MYFLAGS = -DDMSC_USER_DAEMON=1 -DCLOUDOS_VERSION=2
	else
		DMS_C_INCLUDE= -I/usr/include
		DMS_C_CFLAGS= -Wall -g $(DMS_C_INCLUDE) $(DMS_C_LIB) -lpthread
		DMS_C_MYFLAGS = -DDMSC_USER_DAEMON=1 -DCLOUDOS_VERSION=1
	endif
endif

DMS_C_SRCS = config/dmsc_config.c user_daemon/ipcsocket.c user_daemon/mclient.c
DMS_C_OBJS = dmsc_config.o ipcsocket.o mclient.o

##### CCMA Faked Node for testing #####
CCMA_FAKED_NODE_SUBDIRS  = faked_foreign_system

##### CCMA tools #####
CCMA_TOOLS_SUBDIRS  = tools

#################################
#	ccmablk  objs			    #
#################################
ccmablk-objs := lib/md5.o lib/sha1.o lib/dms-radix-tree.o 
ccmablk-objs += config/dmsc_config.o
ccmablk-objs += common/common_util.o common/thread_manager.o common/dms_client_mm.o common/discoC_mem_manager.o
ccmablk-objs += common/sys_mem_monitor.o common/cache.o
ccmablk-objs += common/discoC_comm_main.o
ccmablk-objs += connection_manager/conn_sock_api.o connection_manager/socket_reconn_worker.o
ccmablk-objs += connection_manager/socket_free_worker.o connection_manager/conn_hb_worker.o
ccmablk-objs += connection_manager/conn_state_fsm.o connection_manager/conn_mgr_impl.o connection_manager/conn_manager.o 

ccmablk-objs += discoDN_simulator/discoDNSrv_protocol.o discoDN_simulator/discoDNSrv_CNRequest.o 
ccmablk-objs += discoDN_simulator/conn_intf_DNSimulator_impl.o discoDN_simulator/discoDNSrv_simulator.o

ccmablk-objs += discoDN_client/discoC_DN_protocol.o discoDN_client/fingerprint.o discoDN_client/discoC_DNAck_workerfun.o 
ccmablk-objs += discoDN_client/discoC_DNAck_worker.o discoDN_client/discoC_DNUDReq_fsm.o discoDN_client/discoC_DNUData_Request.o
ccmablk-objs += discoDN_client/discoC_DNUDReqPool.o discoDN_client/discoC_DNC_worker.o discoDN_client/discoC_DNC_workerfun.o 
ccmablk-objs += discoDN_client/discoC_DNC_Manager.o discoDN_client/DNAck_ErrorInjection.o

ccmablk-objs += payload_manager/payload_request_fsm.o payload_manager/payload_request.o payload_manager/payload_ReqPool.o 
ccmablk-objs += payload_manager/Read_Replica_Selector.o payload_manager/payload_Req_IODone.o payload_manager/payload_manager.o

ccmablk-objs += discoNN_simulator/discoNNSrv_protocol.o discoNN_simulator/discoNNSrv_CNRequest.o 
ccmablk-objs += discoNN_simulator/conn_intf_NNSimulator_impl.o discoNN_simulator/discoNNSrv_simulator.o

ccmablk-objs += discoNNOVW_simulator/discoNNOVWSrv_protocol.o discoNNOVW_simulator/discoNNOVWSrv_CNRequest.o 
ccmablk-objs += discoNNOVW_simulator/conn_intf_NNOVWSimulator_impl.o discoNNOVW_simulator/discoNNOVWSrv_simulator.o

ccmablk-objs += discoNN_client/discoC_NN_protocol.o discoNN_client/discoC_NNC_workerfunc.o discoNN_client/discoC_NNClient.o
ccmablk-objs += discoNN_client/discoC_NNMData_Request.o discoNN_client/discoC_NNMDReqPool.o discoNN_client/discoC_NNC_ovw.o
ccmablk-objs += discoNN_client/NN_FSReq_ProcFunc.o discoNN_client/discoC_NNC_workerfunc_reportMD.o discoNN_client/discoC_NNAck_acquireMD.o discoNN_client/discoC_NNAck_reportMD.o 
ccmablk-objs += discoNN_client/discoC_NNMDReq_fsm.o discoNN_client/discoC_NNC_worker.o discoNN_client/discoC_NNC_Manager.o

ccmablk-objs += metadata_manager/metadata_cache.o metadata_manager/metadata_manager.o
ccmablk-objs += metadata_manager/space_chunk_fsm.o metadata_manager/space_chunk.o metadata_manager/space_chunk_cache.o
ccmablk-objs += metadata_manager/space_storage_pool.o metadata_manager/space_commit_worker.o metadata_manager/space_manager.o

ccmablk-objs += io_manager/discoC_IOReqPool.o io_manager/ioreq_ovlp.o io_manager/io_worker_manager.o
ccmablk-objs += io_manager/discoC_IOSegLeaf_Request.o io_manager/discoC_ioSegReq_fsm.o io_manager/discoC_IOSegment_Request.o 
ccmablk-objs += io_manager/discoC_ioSegReq_mdata_cbfn.o io_manager/discoC_ioSegReq_udata_cbfn.o 
ccmablk-objs += io_manager/discoC_IODone_worker.o io_manager/discoC_IOReq_copyData_func.o io_manager/discoC_IOReq_fsm.o io_manager/discoC_IORequest.o 
ccmablk-objs += io_manager/discoC_IO_Manager.o
 
ccmablk-objs += info_monitor/dms_info.o info_monitor/dms_sysinfo.o vdisk_manager/volume_manager.o
ccmablk-objs += flowcontrol/FlowControl.o housekeeping.o SelfTest.o
ccmablk-objs += drv_fsm.o drv_chrdev.o drv_blkdev.o drv_main.o

ccmablk-objs += dms_module_init.o
obj-m := ccmablk.o

all:
	if [ -d $(tempDir) ]; then \
		rm -rf $(tempDir);  \
	fi
	mkdir $(tempDir)	

	$(foreach dir, $(kernel_module_list), $(MAKE) -C$(dir)/build M=$(PWD) modules; cp ccmablk.ko $(tempDir)/ccmablk-$(shell basename $(dir)).ko; $(MAKE) clean;)

	cp $(tempDir)/* .
	rm -rf $(tempDir)
ant_all:
	if [ -d $(tempDir) ]; then \
		rm -rf $(tempDir);  \
	fi
	mkdir $(tempDir)

	$(foreach dir, $(kernel_module_list), $(MAKE) -C$(dir)/build M=$(ccmablk_dir) modules; cp ccmablk.ko $(tempDir)/ccmablk-$(shell basename $(dir)).ko; $(MAKE) clean;)

	cp $(tempDir)/* .
	rm -rf $(tempDir)
xen:
	echo "Try to build xen kernel $(xenDir)\n"
	if [ -d $(xenDir) ]; then \
		$(MAKE) -C/lib/modules/"$(xen_kernel_version)"/build M=$(PWD); \
	else \
		echo "Sorry, no xen kernel directory\n"; \
	fi
nonxen:
	$(MAKE) -C/lib/modules/"$(kernel_version)"/build M=$(PWD)

clean:
	rm -rf *.o *.ko *.symvers *.mod.c .*.cmd Module.markers modules.order .tmp_versions version.h
	rm -rf lib/*.o
	rm -rf config/*.o
	rm -rf common/*.o
	rm -rf connection_manager/*.o
	rm -rf discoDN_client/*.o
	rm -rf discoNN_client/*.o
	rm -rf metadata_manager/*.o
	rm -rf payload_manager/*.o
	rm -rf io_manager/*.o
	rm -rf BlockDeviceDaemon
	(cd $(CCMA_FAKED_NODE_SUBDIRS) && make clean)

#################################
#	daemon on user space	#
#################################

c:
	gcc -c $(DMS_C_SRCS) $(DMS_C_MYFLAGS) $(DMS_C_INCLUDE)
	gcc -o "BlockDeviceDaemon" $(DMS_C_OBJS) $(DMS_C_CFLAGS)
	#(cd $(CCMA_FAKED_NODE_SUBDIRS) && make )
	#(cd $(CCMA_TOOLS_SUBDIRS) && make )	

