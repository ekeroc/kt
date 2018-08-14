/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoNNSrv_simulator_private.h
 *
 * This component try to simulate NN
 *
 */

#ifndef DISCODN_SIMULATOR_DISCONNSRV_SIMULATOR_PRIVATE_H_
#define DISCODN_SIMULATOR_DISCONNSRV_SIMULATOR_PRIVATE_H_

#define DEF_NNFC_RATE_SS    32
#define DEF_NNFC_RATE_MAXREQ_SS 640
#define DEF_NNFC_RATE_AgeTime   30
#define DEF_NNFC_RATE_ALLOC     -1
#define DEF_NNFC_RATE_ALLOCMaxReq   4096

#define ACKPKT_BUFFER_SIZE  4096
#define DEF_DN_PORT 20100
#define DEF_DN_IPPrefix "192.168.1."

#define DEF_NUM_REPLICA 2

typedef struct disco_NN_simulator_Manager {
    struct list_head NNSmltor_pool;
    struct mutex NNSmltor_plock;
    int32_t NNSmltor_mpoolID; //memory pool id
    atomic_t num_NNSmltor;
} NNSimulator_Manager_t;

NNSimulator_Manager_t nnSmltorMgr;

#endif /* DISCODN_SIMULATOR_DISCODNSRV_SIMULATOR_PRIVATE_H_ */
