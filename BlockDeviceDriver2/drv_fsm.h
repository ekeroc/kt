/*
 * DISCO Client Driver State Machine
 *
 * Copyright (C) 2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * drv_fsm.h
 *
 */

#ifndef _DRV_FSM_H_
#define _DRV_FSM_H_

typedef enum {
    Drv_init = 0,
    Drv_running, //Devices appears at user space and user can R/W data
    Drv_shuting_down,
    Drv_shutdown,
} drv_state_type_t;

typedef enum {
    Drv_init_done,
    Drv_turn_off,
    Drv_off_fail,
    Drv_all_stop,
} drv_state_op_t;

extern drv_state_type_t update_drv_state(drv_state_type_t *drvStat, drv_state_op_t drv_op);
extern drv_state_type_t update_drv_state_lock(drv_state_type_t *drvStat,
        struct mutex *mylock, drv_state_op_t drv_op);
#endif /* _DRV_FSM_H_ */
