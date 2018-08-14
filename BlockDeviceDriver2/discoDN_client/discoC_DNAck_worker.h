/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_DNAck_worker.h
 */

#ifndef _DISCOC_DNACK_WORKER_H_
#define _DISCOC_DNACK_WORKER_H_

typedef struct discoC_DNAck_item {
    discoC_conn_t *conn_entry;      //where connection that ack item received from
    CN2DN_UDRWResp_hder_t pkt;      //packet header of ack item
    data_mem_chunks_t payload;      //read payload of ack item
    struct list_head entry_ackpool; //entry to add to ack pool
    uint64_t receive_time;          //when this ack be received from socket
    bool in_dnAck_workq;            //whether ack in ack worker queue
} DNAck_item_t;

extern DNAck_item_t *DNAckMgr_alloc_ack(void);
extern void DNAckMgr_free_ack(DNAck_item_t *ackItem);
extern void DNAckMgr_init_ack(DNAck_item_t *ackItem, discoC_conn_t *conn);
extern void DNAckMgr_submit_ack(DNAck_item_t *param);
extern int32_t DNAckMgr_init_manager(void);
extern int32_t DNAckMgr_rel_manager(void);

#endif /* _DISCOC_DNACK_WORKER_H_ */


