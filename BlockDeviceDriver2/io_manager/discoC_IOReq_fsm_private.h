/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_IOReq_fsm_private.h
 */


#ifndef IO_MANAGER_IOREQ_FSM_PRIVATE_H_
#define IO_MANAGER_IOREQ_FSM_PRIVATE_H_

int8_t *ioReqStat_str[] = {
        "IOReqS unknown",
        "IOReqS init",
        "IOReqS Ovlp Wait Other IO",
        "IOReqS R/W User Data",
        "IOReqS Report MData",
        "IOReqS Self Healing",
        "IOReqS Request Done",
        "IOReqS unknown"
};

int8_t *ioReqAction_str[] = {
        "IOReqA Unknown", "IOReqA No action",
        "IOReqA Add Ovlp Wait Queue",
        "IOReqA Submit IO",
        "IOReqA DO Report Metadata",
        "IOReqA DO Healing",
        "IOReqA DO Free Request",

        "IOReqA IOSegIODone CI IOReq",
        "IOReqA IOSegMDReportDone CI IOReq",
        "IOReqA Unknown",
};

#endif /* IO_MANAGER_IOREQ_FSM_PRIVATE_H_ */
