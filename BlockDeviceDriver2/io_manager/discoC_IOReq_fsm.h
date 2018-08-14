/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_IOReq_fsm.h
 */

#ifndef IO_MANAGER_IOREQ_FSM_H_
#define IO_MANAGER_IOREQ_FSM_H_

extern IOR_FSM_action_t IOReq_update_state(IOReq_fsm_t *ioRStat, IOR_FSM_TREvent_t event);
extern int8_t *IOReq_get_action_str(IOR_FSM_action_t action);
extern IOR_FSM_state_t IOReq_get_state(IOReq_fsm_t *ioRStat);
extern int8_t *IOReq_get_state_fsm_str(IOReq_fsm_t *ioRStat);
extern void IOReq_init_fsm(IOReq_fsm_t *ioRStat);

#endif /* IO_MANAGER_IOREQ_FSM_H_ */
