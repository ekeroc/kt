/*
 * Copyright (C) 2010 - 2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * NN_FSReq_ProcFunc.h
 *
 */

#ifndef NN_FSREQ_PROCFUNC_H_
#define NN_FSREQ_PROCFUNC_H_

extern int32_t fsReq_send_ReportMData(NNMData_Req_t *nnMDReq, int8_t *message_buffer);
extern int32_t fsReq_send_AllocMData(NNMData_Req_t *nnMDReq, int8_t *message_buffer);
extern int32_t fsReq_send_ReportFreeSpace(NNMData_Req_t *nnMDReq, int8_t *message_buffer);

extern void fsReq_process_preAllocAck(int8_t *mem);
extern void fsReq_process_ReportMDAck(int8_t *mem);
extern void fsReq_process_ReportFreeAck(int8_t *mem);
#endif /* NN_FSREQ_PROCFUNC_H_ */
