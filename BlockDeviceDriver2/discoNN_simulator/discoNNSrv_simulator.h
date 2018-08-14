/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoNNSrv_simulator.h
 *
 * This component try to simulate NN
 *
 */

#ifndef DISCODN_SIMULATOR_DISCONNSRV_SIMULATOR_H_
#define DISCODN_SIMULATOR_DISCONNSRV_SIMULATOR_H_

typedef enum {
    TXNNResp_Send_INIT,
    TXNNResp_Send_Hder_DONE,
    TXNNResp_Send_Body_DONE,
} TXNNResp_Send_State_t;

typedef struct disco_NNSmltor_spaceMgr {
    int8_t discoDN_ipv4addrStr[MAX_NUM_REPLICA][IPV4ADDR_NM_LEN];
    uint32_t discoDN_port[MAX_NUM_REPLICA];
    atomic64_t HBID_generator;
    atomic64_t ChunkID_generator;
    uint16_t num_replica;
    CN2NN_getMDResp_fcpkt_t nnflowCtrl;
} NNSmltor_SpaceMGR_t;

typedef struct disco_NN_simulator {
    uint32_t ipaddr;
    uint32_t port;
    struct list_head list_NNtbl;
    struct list_head CNMDReq_pool;
    spinlock_t CNMDReq_plock;
    atomic_t CNMDReq_qsize;
    wait_queue_head_t CNRXThd_waitq;

    CNMDReq_t *tx_pendCNMDReq;
    atomic_t tx_pendState;
    int32_t ack_dataSize;
    int8_t *ack_pktBuffer;
    bool isRunning;
    bool isInPool;

    NNSmltor_SpaceMGR_t spaceMgr;
} NNSimulator_t;

extern int32_t discoNNSrv_rx_rcv(NNSimulator_t *nnSmltor, int32_t pkt_len, int8_t *packet);
extern int32_t discoNNSrv_tx_send(NNSimulator_t *nnSmltor, int32_t pkt_len, int8_t *packet);

extern void stop_NNSmltor(NNSimulator_t *nnSmltor);
extern NNSimulator_t *create_NNSmltor(uint32_t ipaddr, uint32_t port);
extern void free_NNSmltor(NNSimulator_t *nnSmltor);
#endif /* DISCODN_SIMULATOR_DISCODNSRV_SIMULATOR_H_ */
