/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * payload_request_fsm_private.h
 *
 * Provide a simple finite state machine for payload request
 *
 */

#ifndef PAYLOAD_MANAGER_PAYLOAD_REQUEST_FSM_PRIVATE_H_
#define PAYLOAD_MANAGER_PAYLOAD_REQUEST_FSM_PRIVATE_H_

int8_t *plReqStat_str[] = {
        "PLReq unknown", "PLReq init", "PLReq wait IO DONE", "PLReq Commit Caller",
        "PLReq timeout check", "PLReq unknown",
};

int8_t *plReqAction_str[] = {
        "PLReq Unknown", "PLReq No action", "PLReq RW Data",
        "PLReq Commit Caller", "PLReq timeout check", "PLReq Wait Data",
        "PLReq Unknown",
};

#endif /* PAYLOAD_MANAGER_PAYLOAD_REQUEST_FSM_PRIVATE_H_ */
