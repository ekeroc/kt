/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * payload_request_fsm.h
 *
 * Provide a simple finite state machine for payload request
 *
 */

#ifndef PAYLOAD_MANAGER_PAYLOAD_REQUEST_FSM_H_
#define PAYLOAD_MANAGER_PAYLOAD_REQUEST_FSM_H_

typedef enum {
    PLREQ_E_InitDone,      //When finish request initialization, use this event to change state waiting IO done
    PLREQ_E_CICaller,      //A thread use this event to gain the right to commit caller request
    PLREQ_E_TOFireChk,     //timeout thread use this event to gain the right to check metadata
    PLREQ_E_TOFireChkDone, //timeout thread finsih the metadata checking and use this event to change payload request state back to normal
} PLREQ_FSM_TREvent_t;

typedef enum {
    PLREQ_A_START,
    PLREQ_A_NoAction,     //Caller do nothing when trigger a state transition
    PLREQ_A_RWData,       //Caller allowed to read/write data
    PLREQ_A_DO_CICaller,  //Caller allowed to commit request to caller
    PLREQ_A_DO_TOFireChk, //Caller allowed to check metadata at timeout mechanism
    PLREQ_A_DO_WaitData,  //Caller should start waiting DNClient finish processing
    PLREQ_A_END,
    PLREQ_A_UNKNOWN = PLREQ_A_END,
} PLREQ_FSM_action_t;

typedef enum {
    PLREQ_S_START,
    PLREQ_S_INIT,          //initial state when payload request be created
    PLREQ_S_WAIT_IO_DONE,  //payload request is waiting DNClient return ack
    PLREQ_S_CI_CALLER,     //some thread is commit request to caller
    PLREQ_S_TO_Check,      //timeout thread is checking metadata
    PLREQ_S_END,
    PLREQ_S_UNKNOWN = PLREQ_S_END,
} PLREQ_FSM_state_t;

typedef struct discoC_payload_request_fsm {
    rwlock_t st_lock;            //read/write lock to protect state
    PLREQ_FSM_state_t st_plReq;  //payload request state
} PLReq_fsm_t;

extern PLREQ_FSM_action_t PLReq_update_state(PLReq_fsm_t *plRStat, PLREQ_FSM_TREvent_t event);
extern int8_t *PLReq_get_action_str(PLREQ_FSM_action_t action);
extern PLREQ_FSM_state_t PLReq_get_state(PLReq_fsm_t *plRStat);
extern int8_t *PLReq_get_state_str(PLReq_fsm_t *plRStat);
extern void PLReq_init_fsm(PLReq_fsm_t *plRStat);

#endif /* PAYLOAD_MANAGER_PAYLOAD_REQUEST_FSM_H_ */
