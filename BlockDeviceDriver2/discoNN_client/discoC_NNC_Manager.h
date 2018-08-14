/*
 * Copyright (C) 2010 - 2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_NNC_Manager.h
 *
 * This header file define data structure and provide API for discoNN_client sub-components
 */

#ifndef DISCONN_CLIENT_DISCOC_NNC_MANAGER_H_
#define DISCONN_CLIENT_DISCOC_NNC_MANAGER_H_

#define NNCLIENT_HBUCKET_SIZE   101

typedef struct discoC_NNClient_manager {
    NNMDataReq_Pool_t MData_ReqPool; //A global request pool for metadata request

    struct list_head nnClientID_hpool[NNCLIENT_HBUCKET_SIZE]; //use ID as key
    struct list_head nnClientIP_hpool[NNCLIENT_HBUCKET_SIZE]; //use IP as key
    rwlock_t nnClient_plock;   //add nnclient less happens, use a global lock is enough
    atomic_t num_nnClient;     //number of nn clients

    discoC_NNClient_t *def_nnClient; //default NNClient for dms (or dms.ccma.itri)
    atomic_t nnClient_ID_gen;        //ID generator for generating ID for NNClient

    int32_t NNMDReq_mpool_ID;        //memory pool for creating metadata request
    atomic_t MDReq_ID_gen;           //ID generator for generating ID for MDRequest
} NNClient_MGR_t;

extern NNClient_MGR_t nnClient_mgr;
extern uint64_t mnum_net_order;

extern uint32_t NNClientMgr_alloc_MDReqID(void);
extern discoC_NNClient_t *NNClientMgr_get_NNClient(uint32_t nnSrvIP, uint32_t nnSrvPort);
extern discoC_NNClient_t *NNClientMgr_get_NNClient_byID(uint32_t nnClientID);
extern discoC_NNClient_t *NNClientMgr_get_defNNClient(void);

//TODO bad place, fixme
extern void rel_nnovw_manager(void);
extern void init_nnovw_manager(void);

#endif /* DISCONN_CLIENT_DISCOC_NNC_MANAGER_H_ */
