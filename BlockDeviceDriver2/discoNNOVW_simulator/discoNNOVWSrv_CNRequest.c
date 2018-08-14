/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoNNOVWSrv_CNRequest.c
 *
 * This component try to simulate NN OVW
 *
 */

#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/dms_kernel_version.h"
#include "../common/discoC_mem_manager.h"
#include "discoNNOVWSrv_CNRequest.h"

CNOVWReq_t *CNOVWReq_alloc_request (int32_t mpoolID)
{
    return discoC_malloc_pool(mpoolID);
}

void CNOVWReq_init_request (CNOVWReq_t *cnReq, uint16_t opcode, uint16_t subopcode,
        uint64_t volumeID)
{
    INIT_LIST_HEAD(&cnReq->list_CNOVWReq_pool);
    cnReq->opcode = opcode;
    cnReq->subopcode = subopcode;
    cnReq->volumeID = volumeID;
}

void CNOVWReq_free_request (int32_t mpool, CNOVWReq_t *cnReq)
{
    if (unlikely(IS_ERR_OR_NULL(cnReq))) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN NNOVWSmltor try free null CNReq\n");
        return;
    }

    discoC_free_pool(mpool, cnReq);
}
