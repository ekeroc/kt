/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_NNC_ovw_private.h
 *
 */


#ifndef _DISCOC_NNC_OVW_PRIVATE_H_
#define _DISCOC_NNC_OVW_PRIVATE_H_

#define OVW_SHIFT 4096
#define OVW_BUF_SIZE 4096

#define max_ovw_request_can_be_send_out 4
#define OVW_SOCKET_RECV_BUFFER_SIZE 4096

int32_t namenanode_ovw_port = 2222;
static int8_t ovw_socket_recv_buffer[OVW_SOCKET_RECV_BUFFER_SIZE];
static int8_t blank_buffer[OVW_BUF_SIZE];
static struct discoC_connection *ovw_cpe;

static ovw_req ovwreq_working_queue;
static atomic_t ovwreq_working_queue_size;
static atomic_t ovwreq_num_of_req_send_to_nn;
static atomic_t ovwreq_num_of_req_wait_for_send;
static struct mutex ovwreq_working_queue_lock;
static wait_queue_head_t ovwreq_working_queue_wq;
static bool nn_ovw_manager_ready = false;

const int SIZEOF_OVW_REQ = sizeof(ovw_req);
const int SIZEOF_OVW_RES = sizeof(ovw_res);
const int SIZEOF_CHAR = sizeof(char);

#endif /* _DISCOC_NNC_OVW_PRIVATE_H_ */
