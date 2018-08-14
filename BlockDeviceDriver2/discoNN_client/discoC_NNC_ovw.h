/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_NNC_ovw.h
 *
 */

#ifndef _DISCOC_NNC_OVW_H_
#define _DISCOC_NNC_OVW_H_

#define OVW_REQ_STATE_REQ_OVW_START				0
#define OVW_REQ_STATE_REQ_SEND_TO_NN			1
#define OVW_REQ_STATE_REQ_PROCESS_RESPONSE		2
#define OVW_REQ_STATE_REQ_PROCESS_RESPONSE_DONE	3
#define OVW_REQ_STATE_REQ_CANCEL				4

#define	DMS_NN_REQUEST_CMD_TYPE_OVERWRITTEN 	1
#define DMS_NN_REQUEST_CMD_TYPE_OVW_UPDATE_HB	8

#define DMS_NN_CMD_OVW_SUB_TYPE_REQUEST		1
#define DMS_NN_CMD_OVW_SUB_TYPE_RESPONSE	2

#define NN_OVW_REQUEST_SIZE	28
#define NN_OVW_RESPONSE_PAYLOAD_UNIT_SIZE	16  //LBID + number of LBID

struct _OVW_Request_Header {
    uint64_t   begin_magic_number;
    uint16_t   req_cmd_type;
    uint16_t   sub_type;
    uint64_t   vol_id_byte_order;
    uint64_t   end_magic_number;
} __attribute__ ((__packed__));
typedef struct _OVW_Request_Header ovw_req_header;

typedef struct _OVW_Request {
    ovw_req_header    req_header;
    struct list_head  list;
    struct mutex      req_state_lock;
    char              ovw_req_state;
    char *            ovw_ptr;
    uint64_t          vol_id;
} ovw_req;

struct _OVW_Response {
    uint64_t  begin_magic_number;
    uint64_t  vol_id;
    uint16_t  req_cmd_type;
    uint16_t  sub_type;
    uint16_t  error_code;
    uint64_t  len_in_byte;
    uint64_t  end_magic_number;
} __attribute__ ((__packed__));
typedef struct _OVW_Response ovw_res;

struct discoC_connection;
extern int namenanode_ovw_port;

extern bool ovw_set(volume_device_t *dev, uint64_t LBID, char bitvalue);
extern bool ovw_get(volume_device_t *dev, uint64_t LBID);
extern bool ovw_clear(volume_device_t *dev);
extern int32_t set_vol_ovw_range(volume_device_t *vol, uint64_t lbid_s, uint32_t lbid_len, int8_t val);
extern void release_ovw(struct volume_device *dev);
extern bool init_ovw(struct volume_device *dev, uint32_t volid, uint64_t capacity);
extern void retry_nn_ovw_req(struct discoC_connection *goal);
extern void rel_nnovw_manager(void);
extern void init_nnovw_manager(void);
extern int nn_ovw_response_handler(void *sock_data);

#endif /* _DISCOC_NNC_OVW_H_ */



