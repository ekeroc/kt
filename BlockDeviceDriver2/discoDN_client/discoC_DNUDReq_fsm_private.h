/*
 * Copyright (C) 2010-2020 by Division F / ICL
 * Industrial Technology Research Institute
 *
 * discoC_DNUDReq_fsm_private.h
 *
 * Define datanode request finite state
 */

#ifndef DISCODN_CLIENT_DISCOC_DNUDREQ_FSM_PRIVATE_H_
#define DISCODN_CLIENT_DISCOC_DNUDREQ_FSM_PRIVATE_H_

int8_t *dnReqStat_str[] = {
        "DNReq unknown", "DNReq init", "DNReq wait_send", "DNReq sending",
        "DNReq wait_resp", "DNReq process_resp", "DNReq retry_req",
        "DNReq cancel_req", "DNReq unknown",
};

int8_t *dnReqAction_str[] = {
        "DNReq Unknown", "DNReq No action", "DNReq Do Add WorkerQ",
        "DNReq Do Send Req", "DNReq Do Wait Resp", "DNReq Process Resp",
        "DBReq Do retry req", "DNReq Cancel Req", "DNReq Retry Cancel Req", "DNReq Unknown",
};

#endif /* DISCODN_CLIENT_DISCOC_DNUDREQ_FSM_PRIVATE_H_ */
