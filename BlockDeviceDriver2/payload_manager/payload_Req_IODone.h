/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * payload_Req_IODone.c
 *
 * Provide function for registering to Datanode client as callback function of
 * datanode R/W requests.
 * When datanode request finish R/W request, it will call callback function.
 *
 * Including: Write DN request, Read DN request, Read DN request of non-4K align write
 */

#ifndef PAYLOAD_MANAGER_PAYLOAD_REQ_IODONE_H_
#define PAYLOAD_MANAGER_PAYLOAD_REQ_IODONE_H_

int32_t PLReq_readDN_endfn(metaD_item_t *mdItem, void *private_data,
        data_mem_chunks_t *payload, int32_t result,
        ReqCommon_t *calleeReq, int32_t bk_idx, int32_t dnLoc_idx);
int32_t PLReq_writeDN_endfn(metaD_item_t *mdItem, void *private_data,
        data_mem_chunks_t *payload, int32_t result,
        ReqCommon_t *calleeReq, int32_t bk_idx, int32_t dnLoc_idx);
#endif /* PAYLOAD_MANAGER_PAYLOAD_REQ_IODONE_H_ */
