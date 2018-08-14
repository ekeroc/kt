/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * conn_state_fsm_private.h
 *
 */

#ifndef CONN_STATE_FSM_PRIVATE_H_
#define CONN_STATE_FSM_PRIVATE_H_

int8_t *connStat_str[] = {
        "init", "connecting", "connected",  "connection retry",
        "closing", "end", "Unknown"
};

int8_t *connAction_str[] = {
        "No action", "Do Connect", "Open Service", "Do Connection Retry", "Do Close", "Do Free",
        "Unknown"
};

#endif /* CONN_STATE_FSM_PRIVATE_H_ */
