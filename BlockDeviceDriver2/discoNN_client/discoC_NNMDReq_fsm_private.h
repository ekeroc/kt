/*
 * Copyright (C) 2010 - 2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_NNMDReq_fsm_private.h
 *
 * Define state machine operation of NN Metadata Request
 *
 */

#ifndef DISCONN_CLIENT_DISCOC_NNMDREQ_FSM_PRIVATE_H_
#define DISCONN_CLIENT_DISCOC_NNMDREQ_FSM_PRIVATE_H_

int8_t *nnReqStat_str[] = {
        "unknown", "init", "wait_send", "sending",  "wait_resp",
        "process_resp", "timeout_handle", "retry_req", "unknown",
};

int8_t *nnReqAction_str[] = {
        "Unknown", "No action", "Do Add WorkerQ", "Do Send Req", "Do Wait Resp", "Process Resp",
        "Do timeout process", "Do retry req", "Unknown",
};

int8_t *nnCIReqStat_str[] = {
        "CIReq unknown", "CIReq init", "CIReq wait_send", "CIReq sending",
        "CIReq wait_resp", "CIReq process_resp", "CIReq retry_req", "CIReq unknown",
};

int8_t *nnCIReqAction_str[] = {
        "CIReq Unknown", "CIReq No action", "CIReq Do Add WorkerQ",
        "CIReq Do Send Req", "CIReq Do Wait Resp", "CIReq Process Resp",
        "CIReq Do retry req", "Unknown",
};

#endif /* DISCONN_CLIENT_DISCOC_NNMDREQ_FSM_PRIVATE_H_ */
