/*
 * DISCO Client Driver State Machine
 *
 * Copyright (C) 2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * drv_fsm_private.h
 *
 */

#ifndef _DRV_FSM_PRIVATE_H_
#define _DRV_FSM_PRIVATE_H_

#define DRV_MAX_STATE 4
#define DRV_MAX_OP  4

static uint32_t drv_state_transition[DRV_MAX_STATE][DRV_MAX_OP] = {
        {1, 0, 0, 0},
        {1, 2, 1, 1},
        {2, 2, 1, 3},
        {3, 3, 3, 3}
};

#endif /* _DRV_FSM_PRIVATE_H_ */
