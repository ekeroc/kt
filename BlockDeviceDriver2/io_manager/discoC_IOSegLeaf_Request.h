/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_IOSegLeaf_Request.h
 */

#ifndef IO_MANAGER_DISCOC_IOSEGLEAF_REQUEST_H_
#define IO_MANAGER_DISCOC_IOSEGLEAF_REQUEST_H_

#if 0
typedef enum {
    IOSLeafR_FEA_CIUSER,
} IOSLeafR_fea_t;
#endif

typedef struct discoC_ioSegment_leaf_request {
    ReqCommon_t ioSleafR_comm;          //io seg leaf request also share same request common data members
    struct list_head entry_ioSegReq;    //entry to add to io seg req callee request pool
    ioSeg_req_t *ref_ioSegReq;          //hold reference of caller request

    //TODO define ioSeg Leaf Req state machine
    UserIO_addr_t ioSleafR_uioaddr;     //user domain LBA of ioSeg leaf Request need to handle

    //TODO: Do we need a lock to protect ci user?
    uint8_t *lb_sect_mask;
    metadata_t ioSleafR_MData[MAX_MDATA_VERSION];  //metadata of a io seg leaf request
    atomic_t ioSleafR_MData_ver;                   //metadata version when each time re-query mdata from NN
    uint64_t payload_offset;                       //unit : bytes

    //io remain time
    bool ciUser_result;                            //IO request to commit to user

    struct list_head list_plReqPool;               //pool to hold payload request
    spinlock_t plReq_plock;                        //pool lock of plReqPool
    atomic_t num_plReq;                            //number of payload request in pool
} ioSeg_leafReq_t;

extern ioSeg_leafReq_t *ioSegLeafReq_alloc_request(void);
extern void ioSegLeafReq_free_request(ioSeg_leafReq_t *ioSeg_leafR);
extern void ioSegLeafReq_init_request(ioSeg_leafReq_t *ioSeg_leafR, uint64_t lb_s, ioSeg_req_t *root_ioSR);
extern void ioSegLeafReq_submit_request(ioSeg_leafReq_t *ioS_leafR, bool is_mdcache_on, int32_t volReplica);
#endif /* IO_MANAGER_DISCOC_IOSEGLEAF_REQUEST_H_ */
