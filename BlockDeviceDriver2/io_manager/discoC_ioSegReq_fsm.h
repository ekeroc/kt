/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_ioSegReq_fsm.h
 *
 */

#ifndef IO_MANAGER_IOSEG_REQ_FSM_H_
#define IO_MANAGER_IOSEG_REQ_FSM_H_

extern IOSegREQ_FSM_action_t IOSegReq_update_state(IOSegReq_fsm_t *ioSegRStat, IOSegREQ_FSM_TREvent_t event);
extern int8_t *IOSegReq_get_action_str(IOSegREQ_FSM_action_t action);
extern IOSegREQ_FSM_state_t IOSegReq_get_state(IOSegReq_fsm_t *ioSegRStat);
extern int8_t *IOSegReq_get_state_fsm_str(IOSegReq_fsm_t *ioSegRStat);
extern void IOSegReq_init_fsm(IOSegReq_fsm_t *ioSegRStat);

#endif /* IO_MANAGER_IOSEG_REQ_FSM_H_ */
