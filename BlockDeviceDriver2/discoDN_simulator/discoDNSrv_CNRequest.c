/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoDNSrv_CNRequest.c
 *
 * This component try to simulate DN
 *
 */

#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/dms_kernel_version.h"
#include "../common/discoC_mem_manager.h"
#include "../common/thread_manager.h"
#include "../metadata_manager/metadata_manager.h"
#include "../discoDN_client/discoC_DNC_Manager_export.h"
#include "../discoDN_client/discoC_DNUData_Request.h"
#include "../discoDN_client/discoC_DNC_worker.h"
#include "../discoDN_client/discoC_DNC_workerfun.h"
#include "../discoDN_client/discoC_DN_protocol.h"
#include "discoDNSrv_CNRequest.h"

CNReq_t *CNReq_alloc_request (int32_t mpoolID)
{
    return discoC_malloc_pool(mpoolID);
}

void CNReq_init_request (CNReq_t *cnReq, uint16_t opcode, uint16_t numhb,
        uint32_t pReqID, uint32_t DNReqID)
{
    INIT_LIST_HEAD(&cnReq->list_CNReq_pool);
    cnReq->opcode = opcode;
    cnReq->num_hbids = numhb;
    cnReq->pReqID = pReqID;
    cnReq->DNReqID = DNReqID;
}

void CNReq_free_request (int32_t mpool, CNReq_t *cnReq)
{
    if (unlikely(IS_ERR_OR_NULL(cnReq))) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN DNSmltor try free null CNReq\n");
        return;
    }

    discoC_free_pool(mpool, cnReq);
}
