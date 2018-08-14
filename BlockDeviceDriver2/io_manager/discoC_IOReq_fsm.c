/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_IOReq_fsm.c
 */
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/blkdev.h>
#include <linux/types.h>
#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/dms_kernel_version.h"
#include "../common/discoC_mem_manager.h"
#include "../common/dms_client_mm.h"
#include "../metadata_manager/metadata_manager.h"
#include "discoC_IO_Manager.h"
#include "../vdisk_manager/volume_manager.h"
#include "discoC_IOReq_fsm_private.h"
#include "discoC_IOReq_fsm.h"

static IOR_FSM_action_t _update_ioReqState_Init (IOReq_fsm_t *myStat,
        IOR_FSM_TREvent_t event)
{
    IOR_FSM_action_t action;
    action = IOREQ_A_NoAction;

    switch (event) {
    case IOREQ_E_INIT_DONE:
        myStat->st_ioReq = IOREQ_S_RWUserData;
        action = IOREQ_A_SubmitIO;
        break;
    case IOREQ_E_OvlpOtherIO:
        myStat->st_ioReq = IOREQ_S_OvlpWait;
        action = IOREQ_A_AddOvlpWaitQ;
        break;
    case IOREQ_E_OvlpIODone:
    case IOREQ_E_RWUDataDone:
    case IOREQ_E_ReportMDDone:
    case IOREQ_E_HealDone:
    case IOREQ_E_IOSeg_IODone:
    case IOREQ_E_IOSeg_ReportMDDone:
    default:
        break;
    }

    return action;
}

static IOR_FSM_action_t _update_ioReqState_RWUData (IOReq_fsm_t *myStat,
        IOR_FSM_TREvent_t event)
{
    IOR_FSM_action_t action;
    action = IOREQ_A_NoAction;

    switch (event) {
    case IOREQ_E_INIT_DONE:
    case IOREQ_E_OvlpOtherIO:
    case IOREQ_E_OvlpIODone:
        break;
    case IOREQ_E_RWUDataDone:
        myStat->st_ioReq = IOREQ_S_ReportMD;
        action = IOREQ_A_DO_ReportMD;
        break;
    case IOREQ_E_ReportMDDone:
    case IOREQ_E_HealDone:
        break;
    case IOREQ_E_IOSeg_IODone:
        action = IOREQ_A_IOSegIODoneCIIOR;
        break;
    case IOREQ_E_IOSeg_ReportMDDone:
    default:
        break;
    }

    return action;
}

static IOR_FSM_action_t _update_ioReqState_OvlpWait (IOReq_fsm_t *myStat,
        IOR_FSM_TREvent_t event)
{
    IOR_FSM_action_t action;
    action = IOREQ_A_NoAction;

    switch (event) {
    case IOREQ_E_INIT_DONE:
    case IOREQ_E_OvlpOtherIO:
        break;
    case IOREQ_E_OvlpIODone:
        myStat->st_ioReq = IOREQ_S_RWUserData;
        action = IOREQ_A_SubmitIO;
        break;
    case IOREQ_E_RWUDataDone:
    case IOREQ_E_ReportMDDone:
    case IOREQ_E_HealDone:
    case IOREQ_E_IOSeg_IODone:
    case IOREQ_E_IOSeg_ReportMDDone:
    default:
        break;
    }

    return action;
}

static IOR_FSM_action_t _update_ioReqState_ReportMD (IOReq_fsm_t *myStat,
        IOR_FSM_TREvent_t event)
{
    IOR_FSM_action_t action;
    action = IOREQ_A_NoAction;

    switch (event) {
    case IOREQ_E_INIT_DONE:
    case IOREQ_E_OvlpOtherIO:
    case IOREQ_E_OvlpIODone:
    case IOREQ_E_RWUDataDone:
        break;
    case IOREQ_E_ReportMDDone:
        myStat->st_ioReq = IOREQ_S_SelfHealing;
        action = IOREQ_A_DO_Healing;
        break;
    case IOREQ_E_HealDone:
    case IOREQ_E_IOSeg_IODone:
        break;
    case IOREQ_E_IOSeg_ReportMDDone:
        action = IOREQ_A_IOSegMDReportDoneCIIOR;
        break;
    default:
        break;
    }

    return action;
}

static IOR_FSM_action_t _update_ioReqState_SelfHeal (IOReq_fsm_t *myStat,
        IOR_FSM_TREvent_t event)
{
    IOR_FSM_action_t action;
    action = IOREQ_A_NoAction;

    switch (event) {
    case IOREQ_E_INIT_DONE:
    case IOREQ_E_OvlpOtherIO:
    case IOREQ_E_OvlpIODone:
    case IOREQ_E_RWUDataDone:
    case IOREQ_E_ReportMDDone:
        break;
    case IOREQ_E_HealDone:
        myStat->st_ioReq = IOREQ_S_ReqDone;
        action = IOREQ_A_FreeReq;
        break;
    case IOREQ_E_IOSeg_IODone:
    case IOREQ_E_IOSeg_ReportMDDone:
    default:
        break;
    }

    return action;
}

static IOR_FSM_action_t _update_ioReqState_ReqDone (IOReq_fsm_t *myStat,
        IOR_FSM_TREvent_t event)
{
    IOR_FSM_action_t action;
    action = IOREQ_A_NoAction;

    switch (event) {
    case IOREQ_E_INIT_DONE:
    case IOREQ_E_OvlpOtherIO:
    case IOREQ_E_OvlpIODone:
    case IOREQ_E_RWUDataDone:
    case IOREQ_E_ReportMDDone:
    case IOREQ_E_HealDone:
    case IOREQ_E_IOSeg_IODone:
    case IOREQ_E_IOSeg_ReportMDDone:
    default:
        break;
    }

    return action;
}

static IOR_FSM_action_t _update_ioReqState_lockless (IOReq_fsm_t *myStat,
        IOR_FSM_TREvent_t event) {
    IOR_FSM_action_t action;

    action = IOREQ_A_NoAction;
    switch (myStat->st_ioReq) {
    case IOREQ_S_INIT:
        action = _update_ioReqState_Init(myStat, event);
        break;
    case IOREQ_S_RWUserData:
        action = _update_ioReqState_RWUData(myStat, event);
        break;
    case IOREQ_S_OvlpWait:
        action = _update_ioReqState_OvlpWait(myStat, event);
        break;
    case IOREQ_S_ReportMD:
        action = _update_ioReqState_ReportMD(myStat, event);
        break;
    case IOREQ_S_SelfHealing:
        action = _update_ioReqState_SelfHeal(myStat, event);
        break;
    case IOREQ_S_ReqDone:
        action = _update_ioReqState_ReqDone(myStat, event);
        break;
    default:
        break;
    }

    return action;
}

/*
 * NOTE: caller guarantee nnReqStat not NULL
 */
IOR_FSM_action_t IOReq_update_state (IOReq_fsm_t *ioRStat, IOR_FSM_TREvent_t event)
{
    IOR_FSM_action_t ret;

    write_lock(&ioRStat->st_lock);
    ret = _update_ioReqState_lockless(ioRStat, event);
    write_unlock(&ioRStat->st_lock);

    return ret;
}

int8_t *IOReq_get_action_str (IOR_FSM_action_t action)
{
    if (action > IOREQ_A_START && action < IOREQ_A_END) {
        return ioReqAction_str[action];
    }

    return ioReqAction_str[IOREQ_A_UNKNOWN];
}


IOR_FSM_state_t IOReq_get_state (IOReq_fsm_t *ioRStat)
{
    IOR_FSM_state_t myStat;

    read_lock(&ioRStat->st_lock);
    myStat = ioRStat->st_ioReq;
    read_unlock(&ioRStat->st_lock);

    return myStat;
}

int8_t *IOReq_get_state_fsm_str (IOReq_fsm_t *ioRStat)
{
    int8_t *str;
    IOR_FSM_state_t myStat;

    str = NULL;
    myStat = IOReq_get_state(ioRStat);

    if (myStat > IOREQ_S_START && myStat < IOREQ_S_END) {
        return ioReqStat_str[myStat];
    }

    return ioReqStat_str[IOREQ_S_UNKNOWN];
}

void IOReq_init_fsm (IOReq_fsm_t *ioRStat)
{
    rwlock_init(&ioRStat->st_lock);
    ioRStat->st_ioReq = IOREQ_S_INIT;
}
