/*
 * Copyright (C) 2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 * ioreq_ovlp.h
 *
 */

#ifndef IOREQ_OVLP_H_
#define IOREQ_OVLP_H_

typedef enum {
    IOR_ovlp_NoOvlp = 0,
    IOR_ovlp_Ovlp,
    IOR_ovlp_InQ,
    IOR_ovlp_ListBroken
} IOR_ovlp_res_t;

typedef enum {
    IOR_Wait_NoOvlp,
    IOR_Wait_Payload_All_Ovlp,
    IOR_Wait_Payload_Partial_Ovlp,
    IOR_Wait_MD_All_Ovlp,
    IOR_Wait_MD_Partial_Ovlp
} IOR_ovlp_wait_t;

typedef struct ovlp_item {
    io_request_t *process_ior; //Oning io req id
    io_request_t *wait_ior;    //wait in ovlp q io req id
    struct list_head wait_list; //add to cur io req wait_list
    struct list_head wake_list; //add to prev io req wake_list
    uint64_t ovlp_s;
    uint32_t ovlp_l;
    IOR_ovlp_wait_t wait_type;
} ovlp_item_t;

extern IOR_ovlp_res_t ioReq_chk_ovlp(struct list_head *ovlp_q, io_request_t *io_req);
extern int32_t ioReq_get_ovlpReq(struct list_head *ovlp_q,
        struct list_head *buff_list, io_request_t *io_req);
extern void ioReq_rm_all_ovlpReq(io_request_t *io_req);

#endif /* IOREQ_OVLP_H_ */
