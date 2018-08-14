/*
 * DISCO Client Driver State Machine
 *
 * Copyright (C) 2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * drv_fsm.c
 *
 */

#include <linux/version.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include "drv_fsm_private.h"
#include "drv_fsm.h"

drv_state_type_t update_drv_state (drv_state_type_t *drvStat, drv_state_op_t drv_op)
{
    drv_state_type_t old_state;

    old_state = *drvStat;
    *drvStat = drv_state_transition[old_state][drv_op];

    return old_state;
}

drv_state_type_t update_drv_state_lock (drv_state_type_t *drvStat,
        struct mutex *mylock, drv_state_op_t drv_op)
{
    drv_state_type_t old_state;

    mutex_lock(mylock);
    old_state = update_drv_state(drvStat, drv_op);
    mutex_unlock(mylock);

    return old_state;
}
