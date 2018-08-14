/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * conn_state_fsm.h
 *
 */

#ifndef CONN_STATE_FSM_H_
#define CONN_STATE_FSM_H_

typedef enum {
    CONN_E_Connect,
    CONN_E_Connect_OK,
    CONN_E_Connect_FAIL,
    CONN_E_Connect_Broken,
    CONN_E_STOP,
    CONN_E_STOP_DONE,
    CONN_E_Unknown
} conn_TRevent_t;

typedef enum {
    CONN_A_NoAction,
    CONN_A_DO_Connect,
    CONN_A_DO_Service,
    CONN_A_DO_Retry,
    CONN_A_DO_Close,
    CONN_A_DO_Free,
    CONN_A_Unknown,
} conn_action_t;

extern conn_action_t update_conn_state(connStat_fsm_t *connStat, conn_TRevent_t event);
extern int8_t *get_connStat_action_str(conn_action_t action);
extern conn_stat_t get_conn_state(connStat_fsm_t *connStat);
extern int8_t *get_conn_state_str(connStat_fsm_t *connStat);
extern void init_conn_state_fsm(connStat_fsm_t *connStat);

#endif /* CONN_STATE_FSM_H_ */
