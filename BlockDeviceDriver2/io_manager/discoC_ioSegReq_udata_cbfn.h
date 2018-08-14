/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_ioSegReq_udata_cbfn.h
 *
 * Provide callback function for payload request when request finish
 */

#ifndef IO_MANAGER_DISCOC_IOSEGREQ_UDATA_CBFN_H_
#define IO_MANAGER_DISCOC_IOSEGREQ_UDATA_CBFN_H_

extern void ioSReq_read_cpData(void *user_data, metaD_item_t *mdItem,
        uint64_t payload_off, data_mem_chunks_t *payload);
extern void ioSReq_partialW_read_cpData(void *user_data, metaD_item_t *mdItem,
        uint64_t payload_off, data_mem_chunks_t *payload);
extern int32_t ioSReq_ReadPL_endfn(void *user_data, ReqCommon_t *calleeReq,
        uint64_t lb_s, uint32_t lb_len, bool ioRes, int32_t numFailDN);
extern int32_t ioSReq_PartialW_ReadPL_endfn(void *user_data, ReqCommon_t *calleeReq,
        uint64_t lb_s, uint32_t lb_len, bool ioRes, int32_t numFailDN);

extern int32_t ioSReq_WritePL_ciUser(void *user_data, int32_t numLB, bool ioRes);
extern int32_t ioSReq_WritePL_endfn(void *user_data, ReqCommon_t *calleeReq,
        uint64_t lb_s, uint32_t lb_len, bool ioRes, int32_t numFailDN);


#endif /* IO_MANAGER_DISCOC_IOSEGREQ_UDATA_CBFN_H_ */
