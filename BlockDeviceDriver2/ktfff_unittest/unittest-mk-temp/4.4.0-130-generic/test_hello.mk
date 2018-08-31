# Makefile
.PHONY:clean

#################################
#     Kernel module configs     #
#################################
ccflags-y += -Wno-declaration-after-statement

PWD    := $(shell pwd)
KVER = $(shell uname -r)
KTF_DIR = $(HOME)/src/ktf/kernel
KTF_BDIR = $(HOME)/ktfff/build/$(KVER)/ktf/kernel
_DISCO_CN_ROOT := ../
EXTRASYMS := KBUILD_EXTRA_SYMBOLS="$(KTF_BDIR)/Module.symvers"

ccflags-y += -I$(KTF_DIR)

#################################
#     Kernel module objs        #
#################################
test_hello-objs := lib/md5.o lib/sha1.o lib/dms-radix-tree.o

test_hello-objs += config/dmsc_config.o

test_hello-objs += common/common_util.o common/thread_manager.o common/dms_client_mm.o common/discoC_mem_manager.o
test_hello-objs += common/sys_mem_monitor.o common/cache.o
test_hello-objs += common/discoC_comm_main.o

test_hello-objs += connection_manager/conn_sock_api.o connection_manager/socket_reconn_worker.o
test_hello-objs += connection_manager/socket_free_worker.o connection_manager/conn_hb_worker.o
test_hello-objs += connection_manager/conn_state_fsm.o connection_manager/conn_mgr_impl.o connection_manager/conn_manager.o


test_hello-objs += discoDN_simulator/discoDNSrv_protocol.o discoDN_simulator/discoDNSrv_CNRequest.o
test_hello-objs += discoDN_simulator/conn_intf_DNSimulator_impl.o discoDN_simulator/discoDNSrv_simulator.o


test_hello-objs += discoDN_client/discoC_DN_protocol.o discoDN_client/fingerprint.o discoDN_client/discoC_DNAck_workerfun.o
test_hello-objs += discoDN_client/discoC_DNAck_worker.o discoDN_client/discoC_DNUDReq_fsm.o discoDN_client/discoC_DNUData_Request.o
test_hello-objs += discoDN_client/discoC_DNUDReqPool.o discoDN_client/discoC_DNC_worker.o discoDN_client/discoC_DNC_workerfun.o
test_hello-objs += discoDN_client/discoC_DNC_Manager.o discoDN_client/DNAck_ErrorInjection.o

test_hello-objs += payload_manager/payload_request_fsm.o payload_manager/payload_request.o payload_manager/payload_ReqPool.o
test_hello-objs += payload_manager/Read_Replica_Selector.o payload_manager/payload_Req_IODone.o payload_manager/payload_manager.o

test_hello-objs += discoNNOVW_simulator/discoNNOVWSrv_protocol.o discoNNOVW_simulator/discoNNOVWSrv_CNRequest.o
test_hello-objs += discoNNOVW_simulator/conn_intf_NNOVWSimulator_impl.o discoNNOVW_simulator/discoNNOVWSrv_simulator.o

test_hello-objs += discoNN_simulator/discoNNSrv_protocol.o discoNN_simulator/discoNNSrv_CNRequest.o
test_hello-objs += discoNN_simulator/conn_intf_NNSimulator_impl.o discoNN_simulator/discoNNSrv_simulator.o

test_hello-objs += discoNN_client/discoC_NN_protocol.o discoNN_client/discoC_NNC_workerfunc.o discoNN_client/discoC_NNClient.o
test_hello-objs += discoNN_client/discoC_NNMData_Request.o discoNN_client/discoC_NNMDReqPool.o discoNN_client/discoC_NNC_ovw.o
test_hello-objs += discoNN_client/NN_FSReq_ProcFunc.o discoNN_client/discoC_NNC_workerfunc_reportMD.o discoNN_client/discoC_NNAck_acquireMD.o discoNN_client/discoC_NNAck_reportMD.o
test_hello-objs += discoNN_client/discoC_NNMDReq_fsm.o discoNN_client/discoC_NNC_worker.o discoNN_client/discoC_NNC_Manager.o

test_hello-objs += metadata_manager/metadata_cache.o metadata_manager/metadata_manager.o
test_hello-objs += metadata_manager/space_chunk_fsm.o metadata_manager/space_chunk.o metadata_manager/space_chunk_cache.o
test_hello-objs += metadata_manager/space_storage_pool.o metadata_manager/space_commit_worker.o metadata_manager/space_manager.o

test_hello-objs += io_manager/discoC_IOReqPool.o io_manager/ioreq_ovlp.o io_manager/io_worker_manager.o
test_hello-objs += io_manager/discoC_IOSegLeaf_Request.o io_manager/discoC_ioSegReq_fsm.o io_manager/discoC_IOSegment_Request.o
test_hello-objs += io_manager/discoC_ioSegReq_mdata_cbfn.o io_manager/discoC_ioSegReq_udata_cbfn.o
test_hello-objs += io_manager/discoC_IODone_worker.o io_manager/discoC_IOReq_copyData_func.o io_manager/discoC_IOReq_fsm.o io_manager/discoC_IORequest.o
test_hello-objs += io_manager/discoC_IO_Manager.o

test_hello-objs += info_monitor/dms_info.o info_monitor/dms_sysinfo.o vdisk_manager/volume_manager.o
test_hello-objs += flowcontrol/FlowControl.o housekeeping.o SelfTest.o
test_hello-objs += drv_fsm.o drv_chrdev.o drv_blkdev.o

obj-m := test_hello.o

test_hello-objs := $(addprefix $(_DISCO_CN_ROOT), $(test_hello-objs))
test_hello-objs += unittests/test_hello.o

all:
	$(MAKE) -C/lib/modules/4.4.0-130-generic/build $(EXTRASYMS) M=$(PWD)

clean:
	rm -rf *.o *.ko *.symvers *.mod.c .*.cmd Module.markers modules.order .tmp_versions version.h
	rm -rf /home/louis/clientnode-uf/BlockDeviceDriver2/ktfff_unittest/../lib/*.o
	rm -rf /home/louis/clientnode-uf/BlockDeviceDriver2/ktfff_unittest/../config/*.o
	rm -rf /home/louis/clientnode-uf/BlockDeviceDriver2/ktfff_unittest/../common/*.o
	rm -rf /home/louis/clientnode-uf/BlockDeviceDriver2/ktfff_unittest/../connection_manager/*.o
	rm -rf /home/louis/clientnode-uf/BlockDeviceDriver2/ktfff_unittest/../discoDN_simulator/*.o
	rm -rf /home/louis/clientnode-uf/BlockDeviceDriver2/ktfff_unittest/../discoDN_client/*.o
	rm -rf /home/louis/clientnode-uf/BlockDeviceDriver2/ktfff_unittest/../payload_manager/*.o
	rm -rf /home/louis/clientnode-uf/BlockDeviceDriver2/ktfff_unittest/../discoNNOVW_simulator/*.o
	rm -rf /home/louis/clientnode-uf/BlockDeviceDriver2/ktfff_unittest/../discoNN_simulator/*.o
	rm -rf /home/louis/clientnode-uf/BlockDeviceDriver2/ktfff_unittest/../discoNN_client/*.o
	rm -rf /home/louis/clientnode-uf/BlockDeviceDriver2/ktfff_unittest/../metadata_manager/*.o
	rm -rf /home/louis/clientnode-uf/BlockDeviceDriver2/ktfff_unittest/../io_manager/*.o

