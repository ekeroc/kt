/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoDNSrv_simulator.h
 *
 * This component try to simulate DN
 *
 */

#ifndef DISCODN_SIMULATOR_DISCODNSRV_SIMULATOR_H_
#define DISCODN_SIMULATOR_DISCODNSRV_SIMULATOR_H_

typedef enum {
    TXCNR_Send_INIT,
    TXCNR_Send_RHder_DONE,
    TXCNR_Send_RData_DONE,
    TXCNR_Send_WMemAck_DONE,
    TXCNR_Send_WDiskAck_DONE,
} TXCNR_Send_State_t;

typedef struct disco_DN_simulator {
    uint32_t ipaddr;
    uint32_t port;
    struct list_head list_DNtbl;
    struct list_head CNReq_pool;
    spinlock_t CNReq_plock;
    //dmsc_thread_t *CNReq_wker;
    atomic_t CNReq_qsize;
    wait_queue_head_t CNReq_waitq;

    CNReq_t *rx_pendCNReq;
    uint32_t rx_pendDataSize;

    CNReq_t *tx_pendCNReq;
    atomic_t tx_pendState;
    bool isRunning;
    bool isInPool;
} DNSimulator_t;

extern int32_t discoDNSrv_rx_rcv(DNSimulator_t *dnSmltor, int32_t pkt_len, int8_t *packet);
extern int32_t discoDNSrv_tx_send(DNSimulator_t *dnSmltor, int32_t pkt_len, int8_t *packet);

extern void stop_DNSmltor(DNSimulator_t *dnSmltor);
extern DNSimulator_t *create_DNSmltor(uint32_t ipaddr, uint32_t port);
extern void free_DNSmltor(DNSimulator_t *dnSmltor);
#endif /* DISCODN_SIMULATOR_DISCODNSRV_SIMULATOR_H_ */
