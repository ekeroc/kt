/*
 * Copyright (C) 2010 - 2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_NNMDReq_fsm.c
 *
 * Define state machine operation of NN Metadata Request
 *
 */
#include <linux/version.h>
#include <linux/blkdev.h>
#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/dms_kernel_version.h"
#include "../common/discoC_mem_manager.h"
#include "../metadata_manager/metadata_manager.h"
#include "../connection_manager/conn_manager_export.h"
#include "../common/thread_manager.h"
#include "discoC_NNC_Manager_api.h"
#include "discoC_NNC_worker.h"
#include "../flowcontrol/FlowControl.h"
#include "discoC_NNClient.h"
#include "discoC_NNMData_Request.h"
#include "discoC_NNMDReq_fsm_private.h"

/*
 * _update_nnReqS_Init: request a state change from old state "init request"
 *                            (for acquire metadata request)
 * @nnReqStat: target state to be changed
 * @event: event that trigger state change
 *
 * Return: return the action that caller should do
 */
static NNMDgetReq_FSM_action_t _update_nnReqS_Init (NNMDReq_fsm_t *nnReqStat,
        NNMDgetReq_FSM_TREvent_t event)
{
    NNMDgetReq_FSM_action_t action;
    action = NNREQ_A_NoAction;

    switch (event) {
    case NNREQ_E_InitDone:
        nnReqStat->st_nnReq = NNREQ_S_WAIT_SEND;
        action = NNREQ_A_DO_ADD_WORKQ;
        break;
    case NNREQ_E_PeekWorkQ:
    case NNREQ_E_SendDone:
    case NNREQ_E_FindReqOK:
    case NNREQ_E_TimeoutFire:
    case NNREQ_E_ConnBack:
    case NNREQ_E_RetryChkDone:
    default:
        break;
    }

    return action;
}

/*
 * _update_nnReqS_WaitSend: request a state change from old state "wait sending request"
 *                            (for acquire metadata request)
 * @nnReqStat: target state to be changed
 * @event: event that trigger state change
 *
 * Return: return the action that caller should do
 */
static NNMDgetReq_FSM_action_t _update_nnReqS_WaitSend (NNMDReq_fsm_t *nnReqStat,
        NNMDgetReq_FSM_TREvent_t event)
{
    NNMDgetReq_FSM_action_t action;
    action = NNREQ_A_NoAction;

    switch (event) {
    case NNREQ_E_InitDone:
        break;
    case NNREQ_E_PeekWorkQ:
        nnReqStat->st_nnReq = NNREQ_S_SENDING;
        action = NNREQ_A_DO_SEND;
        break;
    case NNREQ_E_SendDone:
    case NNREQ_E_FindReqOK:
        break;
    case NNREQ_E_TimeoutFire:
        nnReqStat->st_nnReq = NNREQ_S_TIMEOUT_FIRE;
        action = NNREQ_A_DO_TO_PROCESS;
        break;
    case NNREQ_E_ConnBack:
    case NNREQ_E_RetryChkDone:
        break;
    default:
        break;
    }

    return action;
}

/*
 * _update_nnReqS_Sending: request a state change from old state "sending request"
 *                            (for acquire metadata request)
 * @nnReqStat: target state to be changed
 * @event: event that trigger state change
 *
 * Return: return the action that caller should do
 */
static NNMDgetReq_FSM_action_t _update_nnReqS_Sending (NNMDReq_fsm_t *nnReqStat,
        NNMDgetReq_FSM_TREvent_t event)
{
    NNMDgetReq_FSM_action_t action;
    action = NNREQ_A_NoAction;

    switch (event) {
    case NNREQ_E_InitDone:
    case NNREQ_E_PeekWorkQ:
        break;
    case NNREQ_E_SendDone:
        nnReqStat->st_nnReq = NNREQ_S_WAIT_RESP;
        action = NNREQ_A_DO_WAIT_RESP;
        break;
    case NNREQ_E_FindReqOK:
        break;
    case NNREQ_E_TimeoutFire:
        nnReqStat->st_nnReq = NNREQ_S_TIMEOUT_FIRE;
        action = NNREQ_A_DO_TO_PROCESS;
        break;
    case NNREQ_E_ConnBack:
    case NNREQ_E_RetryChkDone:
        break;
    default:
        break;
    }

    return action;
}

/*
 * _update_nnReqS_WaitResp: request a state change from old state "wait response"
 *                            (for acquire metadata request)
 * @nnReqStat: target state to be changed
 * @event: event that trigger state change
 *
 * Return: return the action that caller should do
 */
static NNMDgetReq_FSM_action_t _update_nnReqS_WaitResp (NNMDReq_fsm_t *nnReqStat,
        NNMDgetReq_FSM_TREvent_t event)
{
    NNMDgetReq_FSM_action_t action;
    action = NNREQ_A_NoAction;

    switch (event) {
    case NNREQ_E_InitDone:
    case NNREQ_E_PeekWorkQ:
    case NNREQ_E_SendDone:
        break;
    case NNREQ_E_FindReqOK:
        nnReqStat->st_nnReq = NNREQ_S_PROCESS_RESP;
        action = NNREQ_A_DO_PROCESS_RESP;
        break;
    case NNREQ_E_TimeoutFire:
        nnReqStat->st_nnReq = NNREQ_S_TIMEOUT_FIRE;
        action = NNREQ_A_DO_TO_PROCESS;
        break;
    case NNREQ_E_ConnBack:
        nnReqStat->st_nnReq = NNREQ_S_RETRY_REQ;
        action = NNREQ_A_DO_RETRY_REQ;
        break;
    case NNREQ_E_RetryChkDone:
        break;
    default:
        break;
    }

    return action;
}

/*
 * _update_nnReqS_PResp: request a state change from old state "process response"
 *                            (for acquire metadata request)
 * @nnReqStat: target state to be changed
 * @event: event that trigger state change
 *
 * Return: return the action that caller should do
 */
static NNMDgetReq_FSM_action_t _update_nnReqS_PResp (NNMDReq_fsm_t *nnReqStat,
        NNMDgetReq_FSM_TREvent_t event)
{
    NNMDgetReq_FSM_action_t action;
    action = NNREQ_A_NoAction;

    switch (event) {
    case NNREQ_E_InitDone:
    case NNREQ_E_PeekWorkQ:
    case NNREQ_E_SendDone:
    case NNREQ_E_FindReqOK:
    case NNREQ_E_TimeoutFire:
    case NNREQ_E_ConnBack:
    case NNREQ_E_RetryChkDone:
    default:
        break;
    }

    return action;
}

/*
 * _update_nnReqS_Retry: request a state change from old state "retry request"
 *                            (for acquire metadata request)
 * @nnReqStat: target state to be changed
 * @event: event that trigger state change
 *
 * Return: return the action that caller should do
 */
static NNMDgetReq_FSM_action_t _update_nnReqS_Retry (NNMDReq_fsm_t *nnReqStat,
        NNMDgetReq_FSM_TREvent_t event)
{
    NNMDgetReq_FSM_action_t action;
    action = NNREQ_A_NoAction;

    switch (event) {
    case NNREQ_E_InitDone:
    case NNREQ_E_PeekWorkQ:
    case NNREQ_E_SendDone:
    case NNREQ_E_FindReqOK:
    case NNREQ_E_TimeoutFire:
    case NNREQ_E_ConnBack:
        break;
    case NNREQ_E_RetryChkDone:
        nnReqStat->st_nnReq = NNREQ_S_WAIT_SEND;
        action = NNREQ_A_DO_ADD_WORKQ;
        break;
    default:
        break;
    }

    return action;
}

/*
 * _update_nnReqS_TOFire: request a state change from old state "timeout fire"
 *                            (for acquire metadata request)
 * @nnReqStat: target state to be changed
 * @event: event that trigger state change
 *
 * Return: return the action that caller should do
 */
static NNMDgetReq_FSM_action_t _update_nnReqS_TOFire (NNMDReq_fsm_t *nnReqStat,
        NNMDgetReq_FSM_TREvent_t event)
{
    NNMDgetReq_FSM_action_t action;
    action = NNREQ_A_NoAction;

    switch (event) {
    case NNREQ_E_InitDone:
    case NNREQ_E_PeekWorkQ:
    case NNREQ_E_SendDone:
    case NNREQ_E_FindReqOK:
    case NNREQ_E_TimeoutFire:
    case NNREQ_E_ConnBack:
    case NNREQ_E_RetryChkDone:
    default:
        break;
    }

    return action;
}

/*
 * _update_nnReqState_lockless: request a state change without lock protection
 *       (for acquire metadata request)
 * @myStat: target state to be changed
 * @event: event that trigger state change
 *
 * Return: return the action that caller should do
 */
static NNMDgetReq_FSM_action_t _update_nnReqState_lockless (NNMDReq_fsm_t *myStat,
        NNMDgetReq_FSM_TREvent_t event) {
    NNMDgetReq_FSM_action_t action;

    action = NNREQ_A_NoAction;
    switch (myStat->st_nnReq) {
    case NNREQ_S_INIT:
        action = _update_nnReqS_Init(myStat, event);
        break;
    case NNREQ_S_WAIT_SEND:
        action = _update_nnReqS_WaitSend(myStat, event);
        break;
    case NNREQ_S_SENDING:
        action = _update_nnReqS_Sending(myStat, event);
        break;
    case NNREQ_S_WAIT_RESP:
        action = _update_nnReqS_WaitResp(myStat, event);
        break;
    case NNREQ_S_PROCESS_RESP:
        action = _update_nnReqS_PResp(myStat, event);
        break;
    case NNREQ_S_RETRY_REQ:
        action = _update_nnReqS_Retry(myStat, event);
        break;
    case NNREQ_S_TIMEOUT_FIRE:
        action = _update_nnReqS_TOFire(myStat, event);
        break;
    case NNREQ_S_START:
    case NNREQ_S_END:
    default:
        break;
    }

    return action;
}

/*
 * NNMDgetReq_update_state: send a transmit event to fsm state to request a permission
 *      for doing something (acquire metadata request)
 * @nnReqStat: target state to be changed
 * @event: event that trigger state change
 *
 * Return: return the action that caller should do
 */
NNMDgetReq_FSM_action_t NNMDgetReq_update_state (NNMDReq_fsm_t *nnReqStat,
        NNMDgetReq_FSM_TREvent_t event)
{
    NNMDgetReq_FSM_action_t ret;

    write_lock(&nnReqStat->st_lock);
    ret = _update_nnReqState_lockless(nnReqStat, event);
    write_unlock(&nnReqStat->st_lock);

    return ret;
}

/*
 * _update_nnCIReqS_WaitResp: request a state change from old state "init request"
 *                            (for commit metadata request)
 * @nnReqStat: target state to be changed
 * @event: event that trigger state change
 *
 * Return: return the action that caller should do
 */
static NNMDciReq_FSM_action_t _update_nnCIReqS_Init (NNMDReq_fsm_t *nnReqStat,
        NNMDciReq_FSM_TREvent_t event)
{
    NNMDciReq_FSM_action_t action;
    action = NNCIREQ_A_NoAction;

    switch (event) {
    case NNCIREQ_E_InitDone:
        nnReqStat->st_nnCIReq = NNCIREQ_S_WAIT_SEND;
        action = NNCIREQ_A_DO_ADD_WORKQ;
        break;
    case NNCIREQ_E_PeekWorkQ:
    case NNCIREQ_E_SendDone:
    case NNCIREQ_E_FindReqOK:
    case NNCIREQ_E_ConnBack:
    case NNCIREQ_E_RetryChkDone:
    default:
        break;
    }

    return action;
}

/*
 * _update_nnCIReqS_WaitResp: request a state change from old state "wait sending request"
 *                            (for commit metadata request)
 * @nnReqStat: target state to be changed
 * @event: event that trigger state change
 *
 * Return: return the action that caller should do
 */
static NNMDciReq_FSM_action_t _update_nnCIReqS_WaitSend (NNMDReq_fsm_t *nnReqStat,
        NNMDciReq_FSM_TREvent_t event)
{
    NNMDciReq_FSM_action_t action;
    action = NNCIREQ_A_NoAction;

    switch (event) {
    case NNCIREQ_E_InitDone:
        break;
    case NNCIREQ_E_PeekWorkQ:
        nnReqStat->st_nnCIReq = NNCIREQ_S_SENDING;
        action = NNCIREQ_A_DO_SEND;
        break;
    case NNCIREQ_E_SendDone:
    case NNCIREQ_E_FindReqOK:
    case NNCIREQ_E_ConnBack:
    case NNCIREQ_E_RetryChkDone:
    default:
        break;
    }

    return action;
}

/*
 * _update_nnCIReqS_WaitResp: request a state change from old state "sending request"
 *                            (for commit metadata request)
 * @nnReqStat: target state to be changed
 * @event: event that trigger state change
 *
 * Return: return the action that caller should do
 */
static NNMDciReq_FSM_action_t _update_nnCIReqS_Sending (NNMDReq_fsm_t *nnReqStat,
        NNMDciReq_FSM_TREvent_t event)
{
    NNMDciReq_FSM_action_t action;
    action = NNCIREQ_A_NoAction;

    switch (event) {
    case NNCIREQ_E_InitDone:
    case NNCIREQ_E_PeekWorkQ:
        break;
    case NNCIREQ_E_SendDone:
        nnReqStat->st_nnCIReq = NNCIREQ_S_WAIT_RESP;
        action = NNCIREQ_A_DO_WAIT_RESP;
        break;
    case NNCIREQ_E_FindReqOK:
    case NNCIREQ_E_ConnBack:
    case NNCIREQ_E_RetryChkDone:
    default:
        break;
    }

    return action;
}

/*
 * _update_nnCIReqS_WaitResp: request a state change from old state "wait response"
 *                            (for commit metadata request)
 * @nnReqStat: target state to be changed
 * @event: event that trigger state change
 *
 * Return: return the action that caller should do
 */
static NNMDciReq_FSM_action_t _update_nnCIReqS_WaitResp (NNMDReq_fsm_t *nnReqStat,
        NNMDciReq_FSM_TREvent_t event)
{
    NNMDciReq_FSM_action_t action;
    action = NNCIREQ_A_NoAction;

    switch (event) {
    case NNCIREQ_E_InitDone:
    case NNCIREQ_E_PeekWorkQ:
    case NNCIREQ_E_SendDone:
        break;
    case NNCIREQ_E_FindReqOK:
        nnReqStat->st_nnCIReq = NNCIREQ_S_PROCESS_RESP;
        action = NNCIREQ_A_DO_PROCESS_RESP;
        break;
    case NNCIREQ_E_ConnBack:
        nnReqStat->st_nnCIReq = NNCIREQ_S_RETRY_REQ;
        action = NNCIREQ_A_DO_RETRY_REQ;
        break;
    case NNCIREQ_E_RetryChkDone:
    default:
        break;
    }

    return action;
}

/*
 * _update_nnCIReqS_PResp: request a state change from old state "process response"
 *                         (for commit metadata request)
 * @nnReqStat: target state to be changed
 * @event: event that trigger state change
 *
 * Return: return the action that caller should do
 */
static NNMDciReq_FSM_action_t _update_nnCIReqS_PResp (NNMDReq_fsm_t *nnReqStat,
        NNMDciReq_FSM_TREvent_t event)
{
    NNMDciReq_FSM_action_t action;
    action = NNCIREQ_A_NoAction;

    switch (event) {
    case NNCIREQ_E_InitDone:
    case NNCIREQ_E_PeekWorkQ:
    case NNCIREQ_E_SendDone:
    case NNCIREQ_E_FindReqOK:
    case NNCIREQ_E_ConnBack:
    case NNCIREQ_E_RetryChkDone:
    default:
        break;
    }

    return action;
}

/*
 * _update_nnCIReqS_Retry: request a state change from old state "retry"
 *                         (for commit metadata request)
 * @nnReqStat: target state to be changed
 * @event: event that trigger state change
 *
 * Return: return the action that caller should do
 */
static NNMDciReq_FSM_action_t _update_nnCIReqS_Retry (NNMDReq_fsm_t *nnReqStat,
        NNMDciReq_FSM_TREvent_t event)
{
    NNMDciReq_FSM_action_t action;
    action = NNCIREQ_A_NoAction;

    switch (event) {
    case NNCIREQ_E_InitDone:
    case NNCIREQ_E_PeekWorkQ:
    case NNCIREQ_E_SendDone:
    case NNCIREQ_E_FindReqOK:
    case NNCIREQ_E_ConnBack:
        break;
    case NNCIREQ_E_RetryChkDone:
        nnReqStat->st_nnCIReq = NNCIREQ_S_WAIT_SEND;
        action = NNCIREQ_A_DO_ADD_WORKQ;
        break;
    default:
        break;
    }

    return action;
}

/*
 * _update_nnCIReqState_lockless: request a state change without lock protection
 *       (for commit metadata request)
 * @myStat: target state to be changed
 * @event: event that trigger state change
 *
 * Return: return the action that caller should do
 */
static NNMDciReq_FSM_action_t _update_nnCIReqState_lockless (NNMDReq_fsm_t *myStat,
        NNMDciReq_FSM_TREvent_t event) {
    NNMDciReq_FSM_action_t action;

    action = NNCIREQ_A_NoAction;
    switch (myStat->st_nnCIReq) {
    case NNCIREQ_S_INIT:
        action = _update_nnCIReqS_Init(myStat, event);
        break;
    case NNCIREQ_S_WAIT_SEND:
        action = _update_nnCIReqS_WaitSend(myStat, event);
        break;
    case NNCIREQ_S_SENDING:
        action = _update_nnCIReqS_Sending(myStat, event);
        break;
    case NNCIREQ_S_WAIT_RESP:
        action = _update_nnCIReqS_WaitResp(myStat, event);
        break;
    case NNCIREQ_S_PROCESS_RESP:
        action = _update_nnCIReqS_PResp(myStat, event);
        break;
    case NNCIREQ_S_RETRY_REQ:
        action = _update_nnCIReqS_Retry(myStat, event);
        break;
    case NNCIREQ_S_START:
    case NNCIREQ_S_END:
    default:
        break;
    }

    return action;
}

/*
 * NNMDciReq_update_state: send a transmit event to fsm state to request a permission
 *      for doing something (commit metadata request)
 * @nnReqStat: target state to be changed
 * @event: event that trigger state change
 *
 * Return: return the action that caller should do
 */
NNMDciReq_FSM_action_t NNMDciReq_update_state (NNMDReq_fsm_t *nnReqStat,
        NNMDciReq_FSM_TREvent_t event)
{
    NNMDciReq_FSM_action_t ret;

    write_lock(&nnReqStat->st_lock);
    ret = _update_nnCIReqState_lockless(nnReqStat, event);
    write_unlock(&nnReqStat->st_lock);

    return ret;
}

/*
 * NNMDgetReq_get_action_str: return action of acquire metadata request in string for user reading
 * @action: action in NNMDgetReq_FSM_action_t type
 *
 * Return: action in string type
 */
int8_t *NNMDgetReq_get_action_str (NNMDgetReq_FSM_action_t action)
{
    if (action > NNREQ_A_START && action < NNREQ_A_END) {
        return nnReqAction_str[action];
    }

    return nnReqAction_str[NNREQ_A_UNKNOWN];
}

/*
 * NNMDciReq_get_action_str: return action of commit metadata request in string for user reading
 * @action: action in NNMDciReq_FSM_action_t type
 *
 * Return: action in string type
 */
int8_t *NNMDciReq_get_action_str (NNMDciReq_FSM_action_t action)
{
    if (action > NNCIREQ_A_START && action < NNCIREQ_A_END) {
        return nnCIReqAction_str[action];
    }

    return nnCIReqAction_str[NNCIREQ_A_UNKNOWN];
}

/*
 * NNMDgetReq_get_state: get acquire metadata request state with lock protection
 * @nnReqStat: target state to be get
 *
 * Return: NNMDciReq_FSM_state_t type that represent state
 */
NNMDgetReq_FSM_state_t NNMDgetReq_get_state (NNMDReq_fsm_t *nnReqStat)
{
    NNMDgetReq_FSM_state_t myStat;

    read_lock(&nnReqStat->st_lock);
    myStat = nnReqStat->st_nnReq;
    read_unlock(&nnReqStat->st_lock);

    return myStat;
}

/*
 * NNMDciReq_get_state: get commit metadata request state with lock protection
 * @nnReqStat: target state to be get
 *
 * Return: NNMDciReq_FSM_state_t type that represent state
 */
NNMDciReq_FSM_state_t NNMDciReq_get_state (NNMDReq_fsm_t *nnReqStat)
{
    NNMDciReq_FSM_state_t myStat;

    read_lock(&nnReqStat->st_lock);
    myStat = nnReqStat->st_nnCIReq;
    read_unlock(&nnReqStat->st_lock);

    return myStat;
}

/*
 * NNMDgetReq_get_state_str: return get metadata request state in string for user reading
 * @nnReqStat: target state to be show
 *
 * Return: string that represent state
 */
int8_t *NNMDgetReq_get_state_str (NNMDReq_fsm_t *nnReqStat)
{
    int8_t *str;
    NNMDgetReq_FSM_state_t myStat;

    str = NULL;
    myStat = NNMDgetReq_get_state(nnReqStat);

    if (myStat > NNREQ_S_START && myStat < NNREQ_S_END) {
        return nnReqStat_str[myStat];
    }

    return nnReqStat_str[NNREQ_S_UNKNOWN];
}

/*
 * NNMDciReq_get_state_str: return commit metadata request state in string for user reading
 * @nnReqStat: target state to be show
 *
 * Return: string that represent state
 */
int8_t *NNMDciReq_get_state_str (NNMDReq_fsm_t *nnReqStat)
{
    int8_t *str;
    NNMDciReq_FSM_state_t myStat;

    str = NULL;
    myStat = NNMDciReq_get_state(nnReqStat);

    if (myStat > NNCIREQ_S_START && myStat < NNCIREQ_S_END) {
        return nnCIReqStat_str[myStat];
    }

    return nnCIReqStat_str[NNCIREQ_S_UNKNOWN];
}

/*
 * NNMDReq_init_fsm: initialize a metadata request fsm
 * @nnReqStat: target state to be initialized
 */
void NNMDReq_init_fsm (NNMDReq_fsm_t *nnReqStat)
{
    rwlock_init(&nnReqStat->st_lock);
    nnReqStat->st_nnReq = NNREQ_S_INIT;
    nnReqStat->st_nnCIReq = NNCIREQ_S_INIT;
}

