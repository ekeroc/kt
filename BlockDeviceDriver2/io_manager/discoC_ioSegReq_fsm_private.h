/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_ioSegReq_fsm_private.h
 *
 */

#ifndef IO_MANAGER_IOSEG_REQ_FSM_PRIVATE_H_
#define IO_MANAGER_IOSEG_REQ_FSM_PRIVATE_H_

int8_t *ioSegReqStat_str[] = {
        "IOSegS unknown", "IOSegS init", "IOSegS Acquire MD Write",
        "IOSegS Write User Data",
        "IOSegS Acquire MD Read", "IOSegS Read User Data",
        "IOSegS Process Acquire TrueMD",
        "IOSegS CI IOSeg", "IOSegS CI IOReq", "IOSegS unknown"
};

int8_t *ioSegReqAction_str[] = {
        "IOSegA Unknown", "IOSegA No action", "IOSegA Retry TR",
        "IOSegA Acquire MD Write", "IOSegA Write User Data",
        "IOSegA Acquire MD Read", "IOSegA Read User Data",
        "IOSReqA Process Acquire TrueMD", "IOSegA Process Acquire TrueMD DONE",
        "IOSegA CI IOSegReq", "IOSegA CI IOReq", "IOSegA Unknown",
};

#endif /* IO_MANAGER_IOSEG_REQ_FSM_PRIVATE_H_ */
