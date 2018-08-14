/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoNNSrv_simulator.h
 *
 * This component try to simulate NN
 *
 */

#ifndef DISCODN_SIMULATOR_DISCONNOVWSRV_SIMULATOR_H_
#define DISCODN_SIMULATOR_DISCONNOVWSRV_SIMULATOR_H_

typedef enum {
    TXNNOVWResp_Send_INIT,
    TXNNOVWResp_Send_Hder_DONE,
    TXNNOVWResp_Send_Body_DONE,
} TXNNOVWResp_Send_State_t;

typedef struct disco_NNOVW_simulator {
    uint32_t ipaddr;
    uint32_t port;
    struct list_head list_NNOVWtbl;
    struct list_head CNOVWReq_pool;
    spinlock_t CNOVWReq_plock;
    atomic_t CNOVWReq_qsize;
    wait_queue_head_t CNOVWRXThd_waitq;

    CNOVWReq_t *tx_pendCNOVWReq;
    atomic_t tx_pendState;
    bool isRunning;
    bool isInPool;
} NNOVWSimulator_t;

extern int32_t discoNNOVWSrv_rx_rcv(NNOVWSimulator_t *nnovwSmltor, int32_t pkt_len, int8_t *packet);
extern int32_t discoNNOVWSrv_tx_send(NNOVWSimulator_t *nnovwSmltor, int32_t pkt_len, int8_t *packet);

extern void stop_NNOVWSmltor(NNOVWSimulator_t *nnovwSmltor);
extern NNOVWSimulator_t *create_NNOVWSmltor(uint32_t ipaddr, uint32_t port);
extern void free_NNOVWSmltor(NNOVWSimulator_t *nnovwSmltor);
#endif /* DISCODN_SIMULATOR_DISCONNOVWSRV_SIMULATOR_H_ */
