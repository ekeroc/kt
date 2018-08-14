/*
 * NOTE: the max wait cr exe need large than the Linux connect retry timeout
 * sock->ops->connect() timeout value is 10 seconds
 * we need a large value
 */

#ifndef SOCKET_RECONN_WORKER_H_
#define SOCKET_RECONN_WORKER_H_

typedef int32_t (conn_retry_done_fn) (uint32_t ipv4addr, uint32_t port, cs_wrapper_t *discoC_sock);

extern int32_t cancel_conn_retry_task(uint32_t ipv4addr, uint32_t port);
extern int32_t add_conn_retry_task(uint32_t ipv4addr, uint32_t port,
        conn_retry_done_fn *_cr_done_fn, void *user_data);
extern void init_conn_retry_manager(void);
extern void rel_conn_retry_manager(void);

#endif /* SOCKET_RECONN_WORKER_H_ */
