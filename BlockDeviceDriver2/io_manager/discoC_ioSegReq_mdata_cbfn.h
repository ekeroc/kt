/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_ioSegReq_mdata_cbfn.h
 *
 * Provide callback function for metadata request when request finish
 */

#ifndef IO_MANAGER_DISCOC_IOSEGREQ_MDATA_CBFN_H_
#define IO_MANAGER_DISCOC_IOSEGREQ_MDATA_CBFN_H_

extern int32_t ioSReq_reportMD_endfn(metadata_t *MData, void *private_data, ReqCommon_t *calleeReq, int32_t result);
extern int32_t ioSReq_setovw_fn(void *user_data, uint64_t lb_s, uint32_t lb_len);
extern int32_t ioSReq_acquireMD_endfn(metadata_t *MData, void *private_data, ReqCommon_t *calleeReq, int32_t result);
extern int32_t ioSReq_queryTrueMD(void *myReq, uint64_t lbid_s, uint32_t lb_len);

#ifdef DISCO_PREALLOC_SUPPORT
extern int32_t ioSReq_preallocMD_endfn(void *pdata, int32_t result);
#endif
#endif /* IO_MANAGER_DISCOC_IOSEGREQ_MDATA_CBFN_H_ */
