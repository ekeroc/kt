/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * conn_sock_api.h
 *
 */
#ifndef CONN_SOCK_API_H_
#define CONN_SOCK_API_H_

#define SOCK_TIME_TCP_KEEPIDLE  5 //set TCP_KEEPIDLE    5 seconds
#define SOCK_TIME_TCP_KEEPINTVL 1 //set TCP_KEEP_INTVL  1 second
#define SOCK_CNT_TCP_KEEPCNT    5 //set TCP_KEEPCNT     5
#define SOCK_TIME_SO_SNDTIMEO   10 //tx time out 10 seconds
#define SOCK_TIME_SO_RCVTIMEO   10 //rx time out 10 seconds
#define SOCK_TIME_USER_TIMEOUT  60000   //user time out 60 seconds

#define SOCK_CONN_CONNECT_MAX_RETRY     3
#define SOCK_CONN_CONNECT_TIMEOUT    1000 //1 second

#define SOCK_TX_TIMEOUT_MAX_RETRY   5
#define SOCK_TX_TIMEOUT_WAIT_TIME    1000 //each time we sleep 1 second for send side or receiver side release buffer

/*Connection Pool module errors*/
#define EDMS_BASE       1000
#define EDMS_RANGE      100

#define ECP_ID          5
#define ECP_BASE        (EDMS_BASE + ECP_ID * EDMS_RANGE)
#define ECP_INVAL       (ECP_BASE + 1)
#define ECP_SNDTIMEO    (ECP_BASE + 2)
#define ECP_RECVTIMEO   (ECP_BASE + 3)

//TODO: make static in future
extern cs_wrapper_t *alloc_sock_wrapper(uint32_t ipaddr, int32_t port);
extern int32_t sock_connect_target(uint32_t ipaddr, int32_t port,
        struct socket *sock);

extern void free_sock_wrapper(cs_wrapper_t *sk_wrapper);
extern cs_wrapper_t *connect_target(uint32_t ipv4addr, int32_t target_port);
extern int32_t sock_tx_send(struct socket *sock, void *buf, size_t len);
extern int32_t sock_rx_rcv(struct socket *sock, void *buf, int32_t len);
#endif /* CONN_SOCK_API_H_ */
