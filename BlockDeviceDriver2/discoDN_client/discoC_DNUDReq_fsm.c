/*
 * Copyright (C) 2010-2020 by Division F / ICL
 * Industrial Technology Research Institute
 *
 * discoC_DNUDReq_fsm.c
 *
 * Define datanode request finite state
 */
#include <linux/version.h>
#include <linux/blkdev.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <net/sock.h>
#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/dms_kernel_version.h"
#include "../common/discoC_mem_manager.h"
#include "../metadata_manager/metadata_manager.h"
#include "discoC_DNC_Manager_export.h"
#include "discoC_DNUData_Request.h"
#include "discoC_DNUDReq_fsm_private.h"

/*
 * _update_dnReqState_lockless: request a state change from old state "init"
 * @dnRStat: target state to be changed
 * @event: event that trigger state change
 *
 * Return: return the action that caller should do
 */
static DNUDREQ_FSM_action_t _update_dnReqS_Init (DNUDReq_fsm_t *dnRStat,
        DNUDREQ_FSM_TREvent_t event)
{
    DNUDREQ_FSM_action_t action;
    action = DNREQ_A_NoAction;

    switch (event) {
    case DNREQ_E_InitDone:
        dnRStat->st_dnReq = DNREQ_S_WAIT_SEND;
        action = DNREQ_A_DO_ADD_WORKQ;
        break;
    case DNREQ_E_PeekWorkQ:
    case DNREQ_E_SendDone:
    case DNREQ_E_FindReqOK:
    case DNREQ_E_FindReqOK_MemAck:
    case DNREQ_E_ConnBack:
    case DNREQ_E_RetryChkDone:
    case DNREQ_E_Cancel:
    default:
        break;
    }

    return action;
}

/*
 * _update_dnReqState_lockless: request a state change from old state "wait for send"
 * @dnRStat: target state to be changed
 * @event: event that trigger state change
 *
 * Return: return the action that caller should do
 */
static DNUDREQ_FSM_action_t _update_dnReqS_WaitSend (DNUDReq_fsm_t *dnRStat,
        DNUDREQ_FSM_TREvent_t event)
{
    DNUDREQ_FSM_action_t action;
    action = DNREQ_A_NoAction;

    switch (event) {
    case DNREQ_E_InitDone:
        break;
    case DNREQ_E_PeekWorkQ:
        dnRStat->st_dnReq = DNREQ_S_SENDING;
        action = DNREQ_A_DO_SEND;
        break;
    case DNREQ_E_SendDone:
    case DNREQ_E_FindReqOK:
    case DNREQ_E_FindReqOK_MemAck:
    case DNREQ_E_ConnBack:
    case DNREQ_E_RetryChkDone:
        break;
    case DNREQ_E_Cancel:
        dnRStat->st_dnReq = DNREQ_S_CANCEL_REQ;
        action = DNREQ_A_DO_CANCEL_REQ;
        break;
    default:
        break;
    }

    return action;
}

/*
 * _update_dnReqState_lockless: request a state change from old state "sending"
 * @dnRStat: target state to be changed
 * @event: event that trigger state change
 *
 * Return: return the action that caller should do
 */
static DNUDREQ_FSM_action_t _update_dnReqS_Sending (DNUDReq_fsm_t *dnRStat,
        DNUDREQ_FSM_TREvent_t event)
{
    DNUDREQ_FSM_action_t action;
    action = DNREQ_A_NoAction;

    switch (event) {
    case DNREQ_E_InitDone:
    case DNREQ_E_PeekWorkQ:
        break;
    case DNREQ_E_SendDone:
        dnRStat->st_dnReq = DNREQ_S_WAIT_RESP;
        action = DNREQ_A_DO_WAIT_RESP;
        break;
    case DNREQ_E_FindReqOK:
    case DNREQ_E_FindReqOK_MemAck:
    case DNREQ_E_ConnBack:
    case DNREQ_E_RetryChkDone:
        break;
    case DNREQ_E_Cancel:
        dnRStat->st_dnReq = DNREQ_S_CANCEL_REQ;
        action = DNREQ_A_DO_CANCEL_REQ;
        break;
    default:
        break;
    }

    return action;
}

/*
 * _update_dnReqState_lockless: request a state change from old state "wait resp"
 * @dnRStat: target state to be changed
 * @event: event that trigger state change
 *
 * Return: return the action that caller should do
 */
static DNUDREQ_FSM_action_t _update_dnReqS_WaitResp (DNUDReq_fsm_t *dnRStat,
        DNUDREQ_FSM_TREvent_t event)
{
    DNUDREQ_FSM_action_t action;
    action = DNREQ_A_NoAction;

    switch (event) {
    case DNREQ_E_InitDone:
    case DNREQ_E_PeekWorkQ:
    case DNREQ_E_SendDone:
        break;
    case DNREQ_E_FindReqOK_MemAck:
        action = DNREQ_A_DO_PROCESS_RESP;
        break;
    case DNREQ_E_FindReqOK:
        dnRStat->st_dnReq = DNREQ_S_PROCESS_RESP;
        action = DNREQ_A_DO_PROCESS_RESP;
        break;
    case DNREQ_E_ConnBack:
        dnRStat->st_dnReq = DNREQ_S_RETRY_REQ;
        action = DNREQ_A_DO_RETRY_REQ;
        break;
    case DNREQ_E_RetryChkDone:
        break;
    case DNREQ_E_Cancel:
        dnRStat->st_dnReq = DNREQ_S_CANCEL_REQ;
        action = DNREQ_A_DO_CANCEL_REQ;
        break;
    default:
        break;
    }

    return action;
}

/*
 * _update_dnReqState_lockless: request a state change from old state "process resp"
 * @dnRStat: target state to be changed
 * @event: event that trigger state change
 *
 * Return: return the action that caller should do
 */
static DNUDREQ_FSM_action_t _update_dnReqS_PResp (DNUDReq_fsm_t *dnRStat,
        DNUDREQ_FSM_TREvent_t event)
{
    DNUDREQ_FSM_action_t action;
    action = DNREQ_A_NoAction;

    switch (event) {
    case DNREQ_E_InitDone:
    case DNREQ_E_PeekWorkQ:
    case DNREQ_E_SendDone:
    case DNREQ_E_FindReqOK_MemAck:
    case DNREQ_E_FindReqOK:
    case DNREQ_E_ConnBack:
    case DNREQ_E_RetryChkDone:
    case DNREQ_E_Cancel:
    default:
        break;
    }

    return action;
}

/*
 * _update_dnReqState_lockless: request a state change from old state "retry"
 * @dnRStat: target state to be changed
 * @event: event that trigger state change
 *
 * Return: return the action that caller should do
 */
static DNUDREQ_FSM_action_t _update_dnReqS_Retry (DNUDReq_fsm_t *dnRStat,
        DNUDREQ_FSM_TREvent_t event)
{
    DNUDREQ_FSM_action_t action;
    action = DNREQ_A_NoAction;

    switch (event) {
    case DNREQ_E_InitDone:
    case DNREQ_E_PeekWorkQ:
    case DNREQ_E_SendDone:
    case DNREQ_E_FindReqOK_MemAck:
    case DNREQ_E_FindReqOK:
    case DNREQ_E_ConnBack:
        break;
    case DNREQ_E_RetryChkDone:
        dnRStat->st_dnReq = DNREQ_S_WAIT_SEND;
        action = DNREQ_A_DO_ADD_WORKQ;
        break;
    case DNREQ_E_Cancel:
        dnRStat->st_dnReq = DNREQ_S_CANCEL_REQ;
        action = DNREQ_A_DO_RETRY_CANCEL_REQ;
        break;
    default:
        break;
    }

    return action;
}

/*
 * _update_dnReqState_lockless: request a state change from old state "cancel"
 * @dnRStat: target state to be changed
 * @event: event that trigger state change
 *
 * Return: return the action that caller should do
 */
static DNUDREQ_FSM_action_t _update_dnReqS_Cancel (DNUDReq_fsm_t *dnRStat,
        DNUDREQ_FSM_TREvent_t event)
{
    DNUDREQ_FSM_action_t action;
    action = DNREQ_A_NoAction;

    switch (event) {
    case DNREQ_E_InitDone:
    case DNREQ_E_PeekWorkQ:
    case DNREQ_E_SendDone:
    case DNREQ_E_FindReqOK_MemAck:
    case DNREQ_E_FindReqOK:
    case DNREQ_E_ConnBack:
    case DNREQ_E_RetryChkDone:
    case DNREQ_E_Cancel:
    default:
        break;
    }

    return action;
}

/*
 * _update_dnReqState_lockless: request a state change without lock protection
 * @dnRStat: target state to be changed
 * @event: event that trigger state change
 *
 * Return: return the action that caller should do
 */
static DNUDREQ_FSM_action_t _update_dnReqState_lockless (DNUDReq_fsm_t *myStat,
        DNUDREQ_FSM_TREvent_t event) {
    DNUDREQ_FSM_action_t action;

    action = DNREQ_A_NoAction;
    switch (myStat->st_dnReq) {
    case DNREQ_S_INIT:
        action = _update_dnReqS_Init(myStat, event);
        break;
    case DNREQ_S_WAIT_SEND:
        action = _update_dnReqS_WaitSend(myStat, event);
        break;
    case DNREQ_S_SENDING:
        action = _update_dnReqS_Sending(myStat, event);
        break;
    case DNREQ_S_WAIT_RESP:
        action = _update_dnReqS_WaitResp(myStat, event);
        break;
    case DNREQ_S_PROCESS_RESP:
        action = _update_dnReqS_PResp(myStat, event);
        break;
    case DNREQ_S_RETRY_REQ:
        action = _update_dnReqS_Retry(myStat, event);
        break;
    case DNREQ_S_CANCEL_REQ:
        action = _update_dnReqS_Cancel(myStat, event);
        break;
    case DNREQ_S_START:
    case DNREQ_S_END:
    default:
        break;
    }

    return action;
}

/*
 * DNUDReq_update_state: send a transmit event to fsm state to request a permission for doing something
 * @dnRStat: target state to be changed
 * @event: event that trigger state change
 *
 * Return: return the action that caller should do
 */
DNUDREQ_FSM_action_t DNUDReq_update_state (DNUDReq_fsm_t *dnRStat,
        DNUDREQ_FSM_TREvent_t event)
{
    DNUDREQ_FSM_action_t ret;

    write_lock(&dnRStat->st_lock);
    ret = _update_dnReqState_lockless(dnRStat, event);
    write_unlock(&dnRStat->st_lock);

    return ret;
}

/*
 * DNUDReq_get_action_str: return action in string for user reading
 * @action: action in DNUDREQ_FSM_action_t type
 *
 * Return: action in string type
 */
int8_t *DNUDReq_get_action_str (DNUDREQ_FSM_action_t action)
{
    if (action > DNREQ_A_START && action < DNREQ_A_END) {
        return dnReqAction_str[action];
    }

    return dnReqAction_str[DNREQ_A_UNKNOWN];
}

/*
 * DNUDReq_get_state: get state with lock protection
 * @dnRStat: target state to be get
 *
 * Return: DNUDREQ_FSM_state_t type that represent state
 */
DNUDREQ_FSM_state_t DNUDReq_get_state (DNUDReq_fsm_t *dnRStat)
{
    DNUDREQ_FSM_state_t myStat;

    read_lock(&dnRStat->st_lock);
    myStat = dnRStat->st_dnReq;
    read_unlock(&dnRStat->st_lock);

    return myStat;
}

/*
 * DNUDReq_get_state_str: return state in string for user reading
 * @dnRStat: target state to be show
 *
 * Return: string that represent state
 */
int8_t *DNUDReq_get_state_str (DNUDReq_fsm_t *dnRStat)
{
    int8_t *str;
    DNUDREQ_FSM_state_t myStat;

    str = NULL;
    myStat = DNUDReq_get_state(dnRStat);

    if (myStat > DNREQ_S_START && myStat < DNREQ_S_END) {
        return dnReqStat_str[myStat];
    }

    return dnReqStat_str[DNREQ_S_UNKNOWN];
}

/*
 * DNUDReq_init_fsm: initialize a request fsm
 * @dnRStat: target state to be initialized
 */
void DNUDReq_init_fsm (DNUDReq_fsm_t *dnRStat)
{
    rwlock_init(&dnRStat->st_lock);
    dnRStat->st_dnReq = DNREQ_S_INIT;
}
