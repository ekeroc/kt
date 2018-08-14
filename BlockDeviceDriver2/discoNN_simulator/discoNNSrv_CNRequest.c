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
#include "discoNNSrv_CNRequest.h"

CNMDReq_t *CNMDReq_alloc_request (int32_t mpoolID)
{
    return discoC_malloc_pool(mpoolID);
}

void CNMDReq_init_request (CNMDReq_t *cnReq, uint16_t opcode, uint64_t cnMDReqID,
        uint64_t volumeID)
{
    INIT_LIST_HEAD(&cnReq->list_CNMDReq_pool);
    cnReq->opcode = opcode;
    cnReq->CNMDReqID = cnMDReqID;
    cnReq->volumeID = volumeID;
}

void CNMDReq_free_request (int32_t mpool, CNMDReq_t *cnReq)
{
    if (unlikely(IS_ERR_OR_NULL(cnReq))) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN DNSmltor try free null CNReq\n");
        return;
    }

    discoC_free_pool(mpool, cnReq);
}
