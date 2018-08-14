/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * socket_free_worker.h
 *
 * For release socket safely, we using a central thread to release all kernel socket.
 * In some case, a cs_wrapper_t be referenced and stuck at kernel API.
 * When someone free cs_wrapper and set is as NULL, the thread that hold the refernece
 * won't become NULL and cause crash. When the reference count <= 0, we can free it
 */

#ifndef CONNECTION_MANAGER_SOCKET_FREE_WORKER_H_
#define CONNECTION_MANAGER_SOCKET_FREE_WORKER_H_

extern void enq_release_sock(cs_wrapper_t *sk_wraper, sk_release_mgr_t *mymgr);
extern int32_t init_sock_release_mgr(sk_release_mgr_t *mymgr);
extern void rel_sock_release_mgr(sk_release_mgr_t *mymgr);


#endif /* CONNECTION_MANAGER_SOCKET_FREE_WORKER_H_ */
