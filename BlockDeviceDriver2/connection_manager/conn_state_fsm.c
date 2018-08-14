/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * conn_state_fsm.c
 *
 */

#include <linux/kernel.h>
#include <linux/spinlock.h>
#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/dms_kernel_version.h"
#include "../common/common.h"
#include "../common/thread_manager.h"
#include "conn_manager_export.h"
#include "conn_state_fsm_private.h"
#include "conn_state_fsm.h"

static conn_action_t _update_connInit (connStat_fsm_t *myStat,
        conn_TRevent_t event) {

    conn_action_t action;

    action = CONN_A_NoAction;
    switch (event) {
    case CONN_E_Connect:
        myStat->conn_state = CONN_S_Connecting;
        action = CONN_A_DO_Connect;
        break;
    case CONN_E_Connect_OK:
    case CONN_E_Connect_FAIL:
    case CONN_E_Connect_Broken:
    case CONN_E_STOP:
    case CONN_E_STOP_DONE:
    case CONN_E_Unknown:
    default:
        break;
    }

    return action;
}

static conn_action_t _update_connConning (connStat_fsm_t *myStat,
        conn_TRevent_t event) {

    conn_action_t action;

    action = CONN_A_NoAction;
    switch (event) {
    case CONN_E_Connect:
        break;
    case CONN_E_Connect_OK:
        myStat->conn_state = CONN_S_Connected;
        action = CONN_A_DO_Service;
        break;
    case CONN_E_Connect_FAIL:
        myStat->conn_state = CONN_S_ConnectRetry;
        action = CONN_A_DO_Retry;
        break;
    case CONN_E_Connect_Broken:
        break;
    case CONN_E_STOP:
        myStat->conn_state = CONN_S_Closing;
        action = CONN_A_DO_Close;
        break;
    case CONN_E_STOP_DONE:
    case CONN_E_Unknown:
    default:
        break;
    }

    return action;
}

static conn_action_t _update_connConnected (connStat_fsm_t *myStat,
        conn_TRevent_t event) {

    conn_action_t action;

    action = CONN_A_NoAction;
    switch (event) {
    case CONN_E_Connect:
    case CONN_E_Connect_OK:
    case CONN_E_Connect_FAIL:
        break;
    case CONN_E_Connect_Broken:
        myStat->conn_state = CONN_S_ConnectRetry;
        action = CONN_A_DO_Retry;
        break;
    case CONN_E_STOP:
        myStat->conn_state = CONN_S_Closing;
        action = CONN_A_DO_Close;
        break;
    case CONN_E_STOP_DONE:
    case CONN_E_Unknown:
    default:
        break;
    }

    return action;
}

static conn_action_t _update_connRetry (connStat_fsm_t *myStat,
        conn_TRevent_t event) {

    conn_action_t action;

    action = CONN_A_NoAction;
    switch (event) {
    case CONN_E_Connect:
        break;
    case CONN_E_Connect_OK:
        myStat->conn_state = CONN_S_Connected;
        action = CONN_A_DO_Service;
        break;
    case CONN_E_Connect_FAIL:
    case CONN_E_Connect_Broken:
        break;
    case CONN_E_STOP:
        myStat->conn_state = CONN_S_Closing;
        action = CONN_A_DO_Close;
        break;
    case CONN_E_STOP_DONE:
    case CONN_E_Unknown:
    default:
        break;
    }

    return action;
}

static conn_action_t _update_connClosing (connStat_fsm_t *myStat,
        conn_TRevent_t event) {

    conn_action_t action;

    action = CONN_A_NoAction;
    switch (event) {
    case CONN_E_Connect:
    case CONN_E_Connect_OK:
    case CONN_E_Connect_FAIL:
    case CONN_E_Connect_Broken:
    case CONN_E_STOP:
        break;
    case CONN_E_STOP_DONE:
        myStat->conn_state = CONN_S_End;
        action = CONN_A_DO_Free;
        break;
    case CONN_E_Unknown:
    default:
        break;
    }

    return action;
}

static conn_action_t _update_connStat (connStat_fsm_t *myStat,
        conn_TRevent_t event) {
    conn_action_t action;

    action = CONN_A_NoAction;
    switch (myStat->conn_state) {
    case CONN_S_INIT:
        action = _update_connInit(myStat, event);
        break;
    case CONN_S_Connecting:
        action = _update_connConning(myStat, event);
        break;
    case CONN_S_Connected:
        action = _update_connConnected(myStat, event);
        break;
    case CONN_S_ConnectRetry:
        action = _update_connRetry(myStat, event);
        break;
    case CONN_S_Closing:
        action = _update_connClosing(myStat, event);
        break;
    case CONN_S_End:
    case CONN_S_Unknown:
    default:
        break;
    }

    return action;
}

conn_action_t update_conn_state (connStat_fsm_t *connStat, conn_TRevent_t event)
{
    conn_action_t ret;

    ret = _update_connStat(connStat, event);

    return ret;
}

int8_t *get_connStat_action_str (conn_action_t action)
{
    int8_t *str;

    str = NULL;

    if (action >= CONN_A_NoAction && action < CONN_A_Unknown) {
        return connAction_str[action];
    }

    return connAction_str[CONN_A_Unknown];
}

conn_stat_t get_conn_state (connStat_fsm_t *connStat)
{
    conn_stat_t stat;

    stat = connStat->conn_state;

    return stat;
}

bool is_conn_connected (connStat_fsm_t *connStat)
{
    bool ret;

    ret = false;

    if (CONN_S_Connected == connStat->conn_state) {
        ret = true;
    }

    return ret;
}

int8_t *get_conn_state_str (connStat_fsm_t *connStat)
{
    int8_t *str;
    conn_stat_t stat;

    str = NULL;
    stat = get_conn_state(connStat);

    if (stat >= CONN_S_INIT && stat < CONN_S_Unknown) {
        return connStat_str[stat];
    }

    return connStat_str[CONN_S_Unknown];
}

void init_conn_state_fsm (connStat_fsm_t *connStat)
{
    connStat->conn_state = CONN_S_INIT;
}

