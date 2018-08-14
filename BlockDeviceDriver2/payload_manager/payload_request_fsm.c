/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * payload_request_fsm.c
 *
 * Provide a simple finite state machine for payload request
 *
 */
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/dms_kernel_version.h"
#include "../common/discoC_mem_manager.h"
#include "../common/dms_client_mm.h"
#include "../metadata_manager/metadata_manager.h"
#include "payload_manager_export.h"
#include "payload_request_fsm_private.h"
#include "payload_request_fsm.h"

/*
 * _update_PLReqS_TOCheck: transit a state by giving a event and old state is Init
 * @plRStat: target state fsm for update
 * @event: event for trigger state transition
 *
 * Return: an action return to caller. caller should perform operation according action
 */
static PLREQ_FSM_action_t _update_PLReqS_Init (PLReq_fsm_t *myStat,
        PLREQ_FSM_TREvent_t event)
{
    PLREQ_FSM_action_t action;
    action = PLREQ_A_NoAction;
    switch (event) {
    case PLREQ_E_InitDone:
        myStat->st_plReq = PLREQ_S_WAIT_IO_DONE;
        action = PLREQ_A_RWData;
        break;
    case PLREQ_E_CICaller:
    case PLREQ_E_TOFireChk:
    case PLREQ_E_TOFireChkDone:
    default:
        break;
    }

    return action;
}

/*
 * _update_PLReqS_TOCheck: transit a state by giving a event and old state is WaitIO
 * @plRStat: target state fsm for update
 * @event: event for trigger state transition
 *
 * Return: an action return to caller. caller should perform operation according action
 */
static PLREQ_FSM_action_t _update_PLReqS_WaitIO (PLReq_fsm_t *myStat,
        PLREQ_FSM_TREvent_t event)
{
    PLREQ_FSM_action_t action;
    action = PLREQ_A_NoAction;

    switch (event) {
    case PLREQ_E_InitDone:
        break;
    case PLREQ_E_CICaller:
        myStat->st_plReq = PLREQ_S_CI_CALLER;
        action = PLREQ_A_DO_CICaller;
        break;
    case PLREQ_E_TOFireChk:
        myStat->st_plReq = PLREQ_S_TO_Check;
        action = PLREQ_A_DO_TOFireChk;
        break;
    case PLREQ_E_TOFireChkDone:
        break;
    default:
        break;
    }

    return action;
}

/*
 * _update_PLReqS_TOCheck: transit a state by giving a event and old state is CICaller
 * @plRStat: target state fsm for update
 * @event: event for trigger state transition
 *
 * Return: an action return to caller. caller should perform operation according action
 */
static PLREQ_FSM_action_t _update_PLReqS_CICaller (PLReq_fsm_t *myStat,
        PLREQ_FSM_TREvent_t event)
{
    PLREQ_FSM_action_t action;
    action = PLREQ_A_NoAction;

    switch (event) {
    case PLREQ_E_InitDone:
    case PLREQ_E_CICaller:
    case PLREQ_E_TOFireChk:
    case PLREQ_E_TOFireChkDone:
    default:
        break;
    }

    return action;
}

/*
 * _update_PLReqS_TOCheck: transit a state by giving a event and old state is TOCheck
 * @plRStat: target state fsm for update
 * @event: event for trigger state transition
 *
 * Return: an action return to caller. caller should perform operation according action
 */
static PLREQ_FSM_action_t _update_PLReqS_TOCheck (PLReq_fsm_t *myStat,
        PLREQ_FSM_TREvent_t event)
{
    PLREQ_FSM_action_t action;
    action = PLREQ_A_NoAction;

    switch (event) {
    case PLREQ_E_InitDone:
        break;
    case PLREQ_E_CICaller:
        myStat->st_plReq = PLREQ_S_CI_CALLER;
        action = PLREQ_A_DO_CICaller;
        break;
    case PLREQ_E_TOFireChk:
        break;
    case PLREQ_E_TOFireChkDone:
        myStat->st_plReq = PLREQ_S_WAIT_IO_DONE;
        action = PLREQ_A_DO_WaitData;
        break;
    default:
        break;
    }

    return action;
}

/*
 * _update_plReqState_lockless: transit a state by giving a event without lock protection
 * @plRStat: target state fsm for update
 * @event: event for trigger state transition
 *
 * Return: an action return to caller. caller should perform operation according action
 */
static PLREQ_FSM_action_t _update_plReqState_lockless (PLReq_fsm_t *myStat,
        PLREQ_FSM_TREvent_t event) {
    PLREQ_FSM_action_t action;

    action = PLREQ_A_NoAction;

    switch (myStat->st_plReq) {
    case PLREQ_S_INIT:
        action = _update_PLReqS_Init(myStat, event);
        break;
    case PLREQ_S_WAIT_IO_DONE:
        action = _update_PLReqS_WaitIO(myStat, event);
        break;
    case PLREQ_S_CI_CALLER:
        action = _update_PLReqS_CICaller(myStat, event);
        break;
    case PLREQ_S_TO_Check:
        action = _update_PLReqS_TOCheck(myStat, event);
        break;
    case PLREQ_S_START:
    case PLREQ_S_END:
    default:
        break;
    }

    return action;
}

/*
 * PLReq_update_state: transit a state by giving a event
 * @plRStat: target state fsm for update
 * @event: event for trigger state transition
 *
 * Return: an action return to caller. caller should perform operation according action
 */
PLREQ_FSM_action_t PLReq_update_state (PLReq_fsm_t *plRStat,
        PLREQ_FSM_TREvent_t event)
{
    PLREQ_FSM_action_t ret;

    write_lock(&plRStat->st_lock);
    ret = _update_plReqState_lockless(plRStat, event);
    write_unlock(&plRStat->st_lock);

    return ret;
}

/*
 * PLReq_get_action_str: get string representation of an action
 * @action: action in enum format for getting string format
 *
 * Return: string format of an action
 */
int8_t *PLReq_get_action_str (PLREQ_FSM_action_t action)
{
    if (action > PLREQ_A_START && action < PLREQ_A_END) {
        return plReqAction_str[action];
    }

    return plReqAction_str[PLREQ_A_UNKNOWN];
}

/*
 * PLReq_get_state_str: get payload request state
 * @plRStat: target payload request fsm to get state
 *
 * Return: state in enum format
 */
PLREQ_FSM_state_t PLReq_get_state (PLReq_fsm_t *plRStat)
{
    PLREQ_FSM_state_t myStat;

    read_lock(&plRStat->st_lock);
    myStat = plRStat->st_plReq;
    read_unlock(&plRStat->st_lock);

    return myStat;
}

/*
 * PLReq_get_state_str: get state in string representation
 * @plRStat: target payload request fsm to get state
 *
 * Return: string representation of state
 */
int8_t *PLReq_get_state_str (PLReq_fsm_t *plRStat)
{
    int8_t *str;
    PLREQ_FSM_state_t myStat;

    str = NULL;
    myStat = PLReq_get_state(plRStat);

    if (myStat > PLREQ_S_START && myStat < PLREQ_S_END) {
        return plReqStat_str[myStat];
    }

    return plReqStat_str[PLREQ_S_UNKNOWN];
}

/*
 * PLReq_init_fsm: initialize a payload request fsm
 * @plRStat: target payload request fsm initialize
 */
void PLReq_init_fsm (PLReq_fsm_t *plRStat)
{
    rwlock_init(&plRStat->st_lock);
    plRStat->st_plReq = PLREQ_S_INIT;
}
