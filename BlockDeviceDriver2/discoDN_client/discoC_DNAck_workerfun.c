/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_DNAck_workerfun.c
 *
 */
#include <linux/version.h>
#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/dms_kernel_version.h"
#include "../common/discoC_mem_manager.h"
#include "../common/thread_manager.h"
#include "../metadata_manager/metadata_manager.h"
#include "discoC_DNC_Manager_export.h"
#include "discoC_DNUData_Request.h"
#include "discoC_DNUDReqPool.h"
#include "discoC_DNC_worker.h"
#include "discoC_DNC_Manager.h"
#include "discoC_DNAck_workerfun.h"

/*
 * handle_DNAck_Write: handle DN Write Request ack
 *          will call callback function in DNRequest to notify caller module
 *          if ack is memory ack / dis-connect -> keep request stay in pool and don't free
 *          else free request after notify caller
 * @dn_req: target DNRequest to be handle
 * @access_result: response type return by datanode
 *
 * Response: ack process result: 0: no error
 *                              ~0: has error
 */
int32_t handle_DNAck_Write (DNUData_Req_t *dn_req, int32_t access_result)
{
    metaD_item_t *md_item;
    ReqCommon_t *dnR_comm;
    int32_t ret;
    bool dofree;

    ret = 0;
    if (IS_ERR_OR_NULL(dn_req)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN null ptr when chk dn ack state\n");
        DMS_WARN_ON(true);
        return -EINVAL;
    }

    dofree = false;
    md_item = dn_req->mdata_item;

    dnR_comm = &dn_req->dnReq_comm;
    switch (access_result) {
    case DNUDRESP_MEM_ACK:
    case DNUDRESP_WRITEANDGETOLD_MEM_ACK:
        ret = dn_req->dnReq_endfn(dn_req->mdata_item, dn_req->usr_pdata,
                NULL, UData_S_WRITE_MEM_OK,
                dnR_comm, dn_req->caller_bkidx, dn_req->dnLoc_idx);
        break;

    case DNUDRESP_DISK_ACK:
    case DNUDRESP_WRITEANDGETOLD_DISK_ACK:
        ret = dn_req->dnReq_endfn(dn_req->mdata_item, dn_req->usr_pdata,
                NULL, UData_S_WRITE_OK,
                dnR_comm, dn_req->caller_bkidx, dn_req->dnLoc_idx);
        dofree = true;
        break;
    case DN_ERR_DISCONNECTED:
        ret = dn_req->dnReq_endfn(dn_req->mdata_item, dn_req->usr_pdata,
                NULL, UData_S_WRITE_FAIL,
                dnR_comm, dn_req->caller_bkidx, dn_req->dnLoc_idx);
        break;
    case DNUDRESP_ERR_IOException:
        ret = dn_req->dnReq_endfn(dn_req->mdata_item, dn_req->usr_pdata,
                NULL, UData_S_DISK_FAILURE,
                dnR_comm, dn_req->caller_bkidx, dn_req->dnLoc_idx);
        dofree = true;
        break;
    default:
        dms_printk(LOG_LVL_WARN,
                "DMS WARN req_id = %u, unknown responseType = %d\n",
                dnR_comm->ReqID, access_result);
        DMS_WARN_ON(true);

        break;
    }

    //TODO: check again, will free fail when DN_ERR_DISCONNECTED?
    //      when conn err, DN client worker thread will call callback function of payload manager
    //      and those callback function will cancal DN request except current one
    //      make sure won't stuck at free fail

    DNUDReq_deref_Req(dn_req);
    if (dofree) {
        DNUDReq_free_Req(dn_req);
    }

    return ret;
}

/*
 * handle_DNAck_Read: handle DN Read Request ack
 *          will call callback function in DNRequest to notify caller module
 *          if ack is dis-connect -> keep request stay in pool and don't free
 *          else free request after notify caller
 * @dn_req: target DNRequest to be handle
 * @payload: user data payload got from DN
 * @access_result: response type return by datanode
 *
 * Response: ack process result: 0: no error
 *                              ~0: has error
 */
void handle_DNAck_Read (DNUData_Req_t *dn_req, data_mem_chunks_t *payload,
        uint16_t dn_resp)
{
    UData_state_t ret;
    bool dofree;

    dofree = false;
    switch (dn_resp) {
    case DNUDRESP_READ_PAYLOAD:
    case DNUDRESP_READ_NO_PAYLOAD:
        ret = UData_S_READ_OK;
        dofree = true;
        break;
    case DNUDRESP_HBIDERR:
        ret = UData_S_MD_ERR;
        dofree = true;
        break;
    case DNUDRESP_ERR_IOException:
    case DNUDRESP_ERR_IOFailLUNOK:
    case DNUDRESP_ERR_LUNFail:
        ret = UData_S_DISK_FAILURE;
        dofree = true;
        break;
    case DN_ERR_DISCONNECTED:
        ret = UData_S_READ_NA;
        break;
    default:
        dms_printk(LOG_LVL_WARN, "DMSC WARN unknown DN resp %d\n", dn_resp);
        ret = UData_S_OPERATION_ERR;
        break;
    }

    dn_req->dnReq_endfn(dn_req->mdata_item, dn_req->usr_pdata,
            payload, ret, &dn_req->dnReq_comm,
            dn_req->caller_bkidx, dn_req->dnLoc_idx);

    //TODO: code review again, will free fail when DN_ERR_DISCONNECTED?
    //      when conn err, DN client worker thread will call callback function of payload manager
    //      and those callback function will cancal DN request except current one
    //      make sure won't stuck at free fail

    DNUDReq_deref_Req(dn_req);
    if (dofree) {
        DNUDReq_free_Req(dn_req);
    }
}

/*
 * handle_DNAck_IOError: handle error code return by Datanode
 * @dn_req: target DNRequest to be handle
 * @dn_resp: response type return by datanode
 *
 */
void handle_DNAck_IOError (DNUData_Req_t *dn_req, uint16_t dn_resp)
{
    if (DNUDREQ_READ == dn_req->dnReq_comm.ReqOPCode) {
        handle_DNAck_Read(dn_req, NULL, dn_resp);
    } else {
        handle_DNAck_Write(dn_req, dn_resp);
    }
}
