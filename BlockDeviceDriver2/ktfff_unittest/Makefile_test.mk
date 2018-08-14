# Makefile
.PHONY:clean

#################################
#     Kernel module configs     #
#################################


#################################
#     Kernel module objs        #
#################################


hello-objs := lib/md5.o lib/sha1.o lib/dms-radix-tree.o

hello-objs += config/dmsc_config.o

hello-objs += common/common_util.o common/thread_manager.o common/dms_client_mm.o common/discoC_mem_manager.o
hello-objs += common/sys_mem_monitor.o common/cache.o
hello-objs += common/discoC_comm_main.o

hello-objs += connection_manager/conn_sock_api.o connection_manager/socket_reconn_worker.o
hello-objs += connection_manager/socket_free_worker.o connection_manager/conn_hb_worker.o
hello-objs += connection_manager/conn_state_fsm.o connection_manager/conn_mgr_impl.o connection_manager/conn_manager.o


hello-objs += discoDN_simulator/discoDNSrv_protocol.o discoDN_simulator/discoDNSrv_CNRequest.o
hello-objs += discoDN_simulator/conn_intf_DNSimulator_impl.o discoDN_simulator/discoDNSrv_simulator.o


hello-objs += discoDN_client/discoC_DN_protocol.o discoDN_client/fingerprint.o discoDN_client/discoC_DNAck_workerfun.o
hello-objs += discoDN_client/discoC_DNAck_worker.o discoDN_client/discoC_DNUDReq_fsm.o discoDN_client/discoC_DNUData_Request.o
hello-objs += discoDN_client/discoC_DNUDReqPool.o discoDN_client/discoC_DNC_worker.o discoDN_client/discoC_DNC_workerfun.o
hello-objs += discoDN_client/discoC_DNC_Manager.o discoDN_client/DNAck_ErrorInjection.o

hello-objs += payload_manager/payload_request_fsm.o payload_manager/payload_request.o payload_manager/payload_ReqPool.o
hello-objs += payload_manager/Read_Replica_Selector.o payload_manager/payload_Req_IODone.o payload_manager/payload_manager.o

hello-objs += discoNNOVW_simulator/discoNNOVWSrv_protocol.o discoNNOVW_simulator/discoNNOVWSrv_CNRequest.o
hello-objs += discoNNOVW_simulator/conn_intf_NNOVWSimulator_impl.o discoNNOVW_simulator/discoNNOVWSrv_simulator.o

hello-objs += discoNN_simulator/discoNNSrv_protocol.o discoNN_simulator/discoNNSrv_CNRequest.o
hello-objs += discoNN_simulator/conn_intf_NNSimulator_impl.o discoNN_simulator/discoNNSrv_simulator.o

hello-objs += discoNN_client/discoC_NN_protocol.o discoNN_client/discoC_NNC_workerfunc.o discoNN_client/discoC_NNClient.o
hello-objs += discoNN_client/discoC_NNMData_Request.o discoNN_client/discoC_NNMDReqPool.o discoNN_client/discoC_NNC_ovw.o
hello-objs += discoNN_client/NN_FSReq_ProcFunc.o discoNN_client/discoC_NNC_workerfunc_reportMD.o discoNN_client/discoC_NNAck_acquireMD.o discoNN_client/discoC_NNAck_reportMD.o
hello-objs += discoNN_client/discoC_NNMDReq_fsm.o discoNN_client/discoC_NNC_worker.o discoNN_client/discoC_NNC_Manager.o

hello-objs += metadata_manager/metadata_cache.o metadata_manager/metadata_manager.o
hello-objs += metadata_manager/space_chunk_fsm.o metadata_manager/space_chunk.o metadata_manager/space_chunk_cache.o
hello-objs += metadata_manager/space_storage_pool.o metadata_manager/space_commit_worker.o metadata_manager/space_manager.o

hello-objs += io_manager/discoC_IOReqPool.o io_manager/ioreq_ovlp.o io_manager/io_worker_manager.o
hello-objs += io_manager/discoC_IOSegLeaf_Request.o io_manager/discoC_ioSegReq_fsm.o io_manager/discoC_IOSegment_Request.o
hello-objs += io_manager/discoC_ioSegReq_mdata_cbfn.o io_manager/discoC_ioSegReq_udata_cbfn.o
hello-objs += io_manager/discoC_IODone_worker.o io_manager/discoC_IOReq_copyData_func.o io_manager/discoC_IOReq_fsm.o io_manager/discoC_IORequest.o
hello-objs += io_manager/discoC_IO_Manager.o

hello-objs += info_monitor/dms_info.o info_monitor/dms_sysinfo.o vdisk_manager/volume_manager.o
hello-objs += flowcontrol/FlowControl.o housekeeping.o SelfTest.o
hello-objs += drv_chrdev.o

hello-objs += dms_module_init.o
obj-m := hello.o

all:
	$(MAKE) -C/lib/modules/4.4.0-130-generic/build M=$(PWD)

clean:
	rm -rf *.o *.ko *.symvers *.mod.c .*.cmd Module.markers modules.order .tmp_versions version.h
	rm -rf lib/*.o
	rm -rf config/*.o
	rm -rf common/*.o
	rm -rf connection_manager/*.o
	rm -rf discoDN_simulator/*.o
	rm -rf discoDN_client/*.o
	rm -rf payload_manager/*.o
	rm -rf discoNNOVW_simulator/*.o
	rm -rf discoNN_simulator/*.o
	rm -rf discoNN_client/*.o
	rm -rf metadata_manager/*.o
	rm -rf io_manager/*.o
	