/*
 * Copyright (C) 2010 - 2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * NN_Ack_reportMDReq.c
 *
 * Provide function to process ack of metadata access report from namenode
 *
 */
#include <linux/version.h>
#include <linux/blkdev.h>
#include <net/sock.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/dms_kernel_version.h"
#include "../common/discoC_mem_manager.h"
#include "../metadata_manager/metadata_manager.h"
#include "../connection_manager/conn_manager_export.h"
#include "../common/thread_manager.h"
#include "discoC_NNC_Manager_api.h"
#include "discoC_NN_protocol.h"
#include "discoC_NNAck_reportMD.h"
#include "discoC_NNC_worker.h"
#include "../flowcontrol/FlowControl.h"
#include "discoC_NNClient.h"
#include "discoC_NNMData_Request.h"
#include "discoC_NNC_workerfunc_reportMD.h"
#include "discoC_NNMDReqPool.h"

/*
 * NNAckProc_reportMDReq: Process response of report metadata request from namenode
 *     a. parse response packet to get commit result
 *     b. find corresponding metadata request
 *     c. finish request by notify caller through callback function
 * @mem: response body byte array received from socket by socker receive
 * @resppkt: response header packet receive by socket receiver
 */
void NNAckProc_reportMDReq (int8_t *mem, CN2NN_MDResp_hder_t *resppkt)
{
    NNMData_Req_t *nnMDReq;
    int8_t *curptr;
    int32_t ret;
    int16_t commit_result;

    curptr = mem;
    DMS_MEM_READ_NEXT(curptr, &commit_result, sizeof(int16_t));
    commit_result = ntohs(commit_result);

    nnMDReq = NNMDReq_find_ReqPool((uint32_t)resppkt->request_ID);
    if (unlikely(IS_ERR_OR_NULL(nnMDReq))) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN wait commit resp miss MDCIReq = %llu\n",
                resppkt->request_ID);
        DMS_WARN_ON(true);
        return;
    }

    NNMDReq_cancel_timer(nnMDReq);

    dec_res_rate_max_reqs(nnMDReq->nnSrv_client->nnSrv_resource);
    ret = nnMDReq->nnReq_endfn(nnMDReq->MetaData, nnMDReq->usr_pdata,
            &nnMDReq->nnMDReq_comm, 0);
    NNMDReq_deref_Req(nnMDReq);
    NNMDReq_free_Req(nnMDReq);
}
