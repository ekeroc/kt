/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_IOReq_copyData_func.h
 */

#ifndef IO_MANAGER_DISCOC_IOREQ_COPYDATA_FUNC_H_
#define IO_MANAGER_DISCOC_IOREQ_COPYDATA_FUNC_H_

extern void ioReq_WCopyData_bio2mem(io_request_t *ioreq);
extern void ioReq_RCopyData_mem2bio(io_request_t *io_req, uint64_t startlb, uint32_t lb_len, uint64_t payload_off,
        data_mem_chunks_t *payload);
extern void ioReq_RCopyData_mem2wbuff(io_request_t *ioReq, uint64_t startlb, uint32_t lb_len,
        uint64_t payload_off, data_mem_chunks_t *payload);
extern void ioReq_RCopyData_zero2bio (io_request_t *ioreq, uint64_t startLB, uint32_t LB_cnt);
#endif /* IO_MANAGER_DISCOC_IOREQ_COPYDATA_FUNC_H_ */
