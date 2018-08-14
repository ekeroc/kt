/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_ioSegReq_fsm.c
 *
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
#include "discoC_IOSegment_Request.h"
#include "discoC_ioSegReq_fsm_private.h"

static IOSegREQ_FSM_action_t _update_ioSegReqState_Init (IOSegReq_fsm_t *myStat,
        IOSegREQ_FSM_TREvent_t event)
{
    IOSegREQ_FSM_action_t action;
    action = IOSegREQ_A_NoAction;

    switch (event) {
    case IOSegREQ_E_INIT_4KW:
        myStat->st_ioSegReq = IOSegREQ_S_W_AcqMD;
        action = IOSegREQ_A_W_AcqMD;
        break;
    case IOSegREQ_E_W_AcqMD_DONE:
    case IOSegREQ_E_W_ALLPL_DONE:
        break;
    case IOSegREQ_E_INIT_R:
        myStat->st_ioSegReq = IOSegREQ_S_R_AcqMD;
        action = IOSegREQ_A_R_AcqMD;
        break;
    case IOSegREQ_E_R_AcqMD_DONE:
    case IOSegREQ_E_R_ALLPL_DONE:
    case IOSegREQ_E_W_RPhase_DONE:
    case IOSegREQ_E_R_RPhase_DONE:
    case IOSegREQ_E_MDUD_UNSYNC:
    case IOSegREQ_E_ATrueMD_DONE:
    default:
        break;
    }

    return action;
}

static IOSegREQ_FSM_action_t _update_ioSegReqState_WAcqMD (IOSegReq_fsm_t *myStat,
        IOSegREQ_FSM_TREvent_t event)
{
    IOSegREQ_FSM_action_t action;
    action = IOSegREQ_A_NoAction;

    switch (event) {
    case IOSegREQ_E_INIT_4KW:
        break;
    case IOSegREQ_E_W_AcqMD_DONE:
        myStat->st_ioSegReq = IOSegREQ_S_WUD;
        action = IOSegREQ_A_DO_WUD;
        break;
    case IOSegREQ_E_W_ALLPL_DONE:
    case IOSegREQ_E_INIT_R:
    case IOSegREQ_E_R_AcqMD_DONE:
    case IOSegREQ_E_R_ALLPL_DONE:
    case IOSegREQ_E_W_RPhase_DONE:
    case IOSegREQ_E_R_RPhase_DONE:
    case IOSegREQ_E_MDUD_UNSYNC:
    case IOSegREQ_E_ATrueMD_DONE:
    default:
        break;
    }

    return action;
}

static IOSegREQ_FSM_action_t _update_ioSegReqState_WriteUD (IOSegReq_fsm_t *myStat,
        IOSegREQ_FSM_TREvent_t event)
{
    IOSegREQ_FSM_action_t action;
    action = IOSegREQ_A_NoAction;

    switch (event) {
    case IOSegREQ_E_INIT_4KW:
    case IOSegREQ_E_W_AcqMD_DONE:
        break;
    case IOSegREQ_E_W_ALLPL_DONE:
        myStat->st_ioSegReq = IOSegREQ_S_CI2IOR;
        action = IOSegREQ_A_DO_CI2IOREQ;
        break;
    case IOSegREQ_E_INIT_R:
    case IOSegREQ_E_R_AcqMD_DONE:
    case IOSegREQ_E_R_ALLPL_DONE:
    case IOSegREQ_E_W_RPhase_DONE:
    case IOSegREQ_E_R_RPhase_DONE:
    case IOSegREQ_E_MDUD_UNSYNC:
    case IOSegREQ_E_ATrueMD_DONE:
    default:
        break;
    }

    return action;
}

static IOSegREQ_FSM_action_t _update_ioSegReqState_RAcqMD (IOSegReq_fsm_t *myStat,
        IOSegREQ_FSM_TREvent_t event)
{
    IOSegREQ_FSM_action_t action;
    action = IOSegREQ_A_NoAction;

    switch (event) {
    case IOSegREQ_E_INIT_4KW:
    case IOSegREQ_E_W_AcqMD_DONE:
    case IOSegREQ_E_W_ALLPL_DONE:
    case IOSegREQ_E_INIT_R:
        break;
    case IOSegREQ_E_R_AcqMD_DONE:
        myStat->st_ioSegReq = IOSegREQ_S_RUD;
        action = IOSegREQ_A_DO_RUD;
        break;
    case IOSegREQ_E_R_ALLPL_DONE:
    case IOSegREQ_E_W_RPhase_DONE:
    case IOSegREQ_E_R_RPhase_DONE:
    case IOSegREQ_E_MDUD_UNSYNC:
    case IOSegREQ_E_ATrueMD_DONE:
    default:
        break;
    }

    return action;
}

static IOSegREQ_FSM_action_t _update_ioSegReqState_ReadUD (IOSegReq_fsm_t *myStat,
        IOSegREQ_FSM_TREvent_t event)
{
    IOSegREQ_FSM_action_t action;
    action = IOSegREQ_A_NoAction;

    switch (event) {
    case IOSegREQ_E_INIT_4KW:
    case IOSegREQ_E_W_AcqMD_DONE:
    case IOSegREQ_E_W_ALLPL_DONE:
    case IOSegREQ_E_INIT_R:
    case IOSegREQ_E_R_AcqMD_DONE:
        break;
    case IOSegREQ_E_R_ALLPL_DONE:
        myStat->st_ioSegReq = IOSegREQ_S_CI2IOSeg;
        action = IOSegREQ_A_DO_CI2IOSegREQ;
        break;
    case IOSegREQ_E_W_RPhase_DONE:
    case IOSegREQ_E_R_RPhase_DONE:
        break;
    case IOSegREQ_E_MDUD_UNSYNC:
        myStat->st_ioSegReq = IOSegREQ_S_ATrueMD_Proc;
        action = IOSegREQ_A_DO_ATrueMD_PROC;
        break;
    case IOSegREQ_E_ATrueMD_DONE:
    default:
        break;
    }

    return action;
}

static IOSegREQ_FSM_action_t _update_ioSegReqState_ProcAcqTMD (IOSegReq_fsm_t *myStat,
        IOSegREQ_FSM_TREvent_t event)
{
    IOSegREQ_FSM_action_t action;
    action = IOSegREQ_A_NoAction;

    switch (event) {
    case IOSegREQ_E_INIT_4KW:
    case IOSegREQ_E_W_AcqMD_DONE:
    case IOSegREQ_E_W_ALLPL_DONE:
    case IOSegREQ_E_INIT_R:
    case IOSegREQ_E_R_AcqMD_DONE:
        break;
    case IOSegREQ_E_R_ALLPL_DONE:
        action = IOSegREQ_A_RetryTR;
        break;
    case IOSegREQ_E_W_RPhase_DONE:
    case IOSegREQ_E_R_RPhase_DONE:
        break;
    case IOSegREQ_E_MDUD_UNSYNC:
        action = IOSegREQ_A_RetryTR;
        break;
    case IOSegREQ_E_ATrueMD_DONE:
        myStat->st_ioSegReq = IOSegREQ_S_RUD;
        action = IOSegREQ_A_DO_RUD;
        break;
    default:
        break;
    }

    return action;
}

static IOSegREQ_FSM_action_t _update_ioSegReqState_CIIOSeg (IOSegReq_fsm_t *myStat,
        IOSegREQ_FSM_TREvent_t event)
{
    IOSegREQ_FSM_action_t action;
    action = IOSegREQ_A_NoAction;

    switch (event) {
    case IOSegREQ_E_INIT_4KW:
    case IOSegREQ_E_W_AcqMD_DONE:
    case IOSegREQ_E_W_ALLPL_DONE:

    case IOSegREQ_E_INIT_R:
    case IOSegREQ_E_R_AcqMD_DONE:
    case IOSegREQ_E_R_ALLPL_DONE:
        break;
    case IOSegREQ_E_W_RPhase_DONE:
        myStat->st_ioSegReq = IOSegREQ_S_W_AcqMD;
        action = IOSegREQ_A_W_AcqMD;
        break;
    case IOSegREQ_E_R_RPhase_DONE:
        myStat->st_ioSegReq = IOSegREQ_S_CI2IOR;
        action = IOSegREQ_A_DO_CI2IOREQ;
        break;
    case IOSegREQ_E_MDUD_UNSYNC:
    case IOSegREQ_E_ATrueMD_DONE:
    default:
        break;
    }

    return action;
}

static IOSegREQ_FSM_action_t _update_ioSegReqState_CIIOR (IOSegReq_fsm_t *myStat,
        IOSegREQ_FSM_TREvent_t event)
{
    IOSegREQ_FSM_action_t action;
    action = IOSegREQ_A_NoAction;

    switch (event) {
    case IOSegREQ_E_INIT_4KW:
    case IOSegREQ_E_W_AcqMD_DONE:
    case IOSegREQ_E_W_ALLPL_DONE:

    case IOSegREQ_E_INIT_R:
    case IOSegREQ_E_R_AcqMD_DONE:
    case IOSegREQ_E_R_ALLPL_DONE:
    case IOSegREQ_E_W_RPhase_DONE:
    case IOSegREQ_E_R_RPhase_DONE:

    case IOSegREQ_E_MDUD_UNSYNC:
    case IOSegREQ_E_ATrueMD_DONE:
    default:
        break;
    }

    return action;
}

static IOSegREQ_FSM_action_t _update_ioSegReqState_lockless (IOSegReq_fsm_t *myStat,
        IOSegREQ_FSM_TREvent_t event) {
    IOSegREQ_FSM_action_t action;

    action = IOSegREQ_A_NoAction;
    switch (myStat->st_ioSegReq) {
    case IOSegREQ_S_INIT:
        action = _update_ioSegReqState_Init(myStat, event);
        break;
    case IOSegREQ_S_W_AcqMD:
        action = _update_ioSegReqState_WAcqMD(myStat, event);
        break;
    case IOSegREQ_S_WUD:
        action = _update_ioSegReqState_WriteUD(myStat, event);
        break;
    case IOSegREQ_S_R_AcqMD:
        action = _update_ioSegReqState_RAcqMD(myStat, event);
        break;
    case IOSegREQ_S_RUD:
        action = _update_ioSegReqState_ReadUD(myStat, event);
        break;
    case IOSegREQ_S_ATrueMD_Proc:
        action = _update_ioSegReqState_ProcAcqTMD(myStat, event);
        break;
    case IOSegREQ_S_CI2IOSeg:
        action = _update_ioSegReqState_CIIOSeg(myStat, event);
        break;
    case IOSegREQ_S_CI2IOR:
        action = _update_ioSegReqState_CIIOR(myStat, event);
        break;
    default:
        break;
    }

    return action;
}

/*
 * NOTE: caller guarantee nnReqStat not NULL
 */
IOSegREQ_FSM_action_t IOSegReq_update_state (IOSegReq_fsm_t *ioSegRStat,
        IOSegREQ_FSM_TREvent_t event)
{
    IOSegREQ_FSM_action_t ret;

    write_lock(&ioSegRStat->st_lock);
    ret = _update_ioSegReqState_lockless(ioSegRStat, event);
    write_unlock(&ioSegRStat->st_lock);

    return ret;
}

int8_t *IOSegReq_get_action_str (IOSegREQ_FSM_action_t action)
{
    if (action > IOSegREQ_A_START && action < IOSegREQ_A_END) {
        return ioSegReqAction_str[action];
    }

    return ioSegReqAction_str[IOSegREQ_A_UNKNOWN];
}

IOSegREQ_FSM_state_t IOSegReq_get_state (IOSegReq_fsm_t *ioSegRStat)
{
    IOSegREQ_FSM_state_t myStat;

    read_lock(&ioSegRStat->st_lock);
    myStat = ioSegRStat->st_ioSegReq;
    read_unlock(&ioSegRStat->st_lock);

    return myStat;
}

int8_t *IOSegReq_get_state_fsm_str (IOSegReq_fsm_t *ioSegRStat)
{
    int8_t *str;
    IOSegREQ_FSM_state_t myStat;

    str = NULL;
    myStat = IOSegReq_get_state(ioSegRStat);

    if (myStat > IOSegREQ_S_START && myStat < IOSegREQ_S_END) {
        return ioSegReqStat_str[myStat];
    }

    return ioSegReqStat_str[IOSegREQ_S_UNKNOWN];
}

void IOSegReq_init_fsm (IOSegReq_fsm_t *ioSegRStat)
{
    rwlock_init(&ioSegRStat->st_lock);
    ioSegRStat->st_ioSegReq = IOSegREQ_S_INIT;
    ioSegRStat->cist_ioSegReq = IOSegREQ_CI_S_INIT;
}
