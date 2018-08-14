/*
 * Copyright (C) 2010-2020 by Division F / ICL
 * Industrial Technology Research Institute
 *
 * discoC_DNUDReq_fsm.h
 *
 * Define datanode request finite state
 */

#ifndef DISCODN_CLIENT_DISCOC_DNUDREQ_FSM_H_
#define DISCODN_CLIENT_DISCOC_DNUDREQ_FSM_H_

extern DNUDREQ_FSM_action_t DNUDReq_update_state(DNUDReq_fsm_t *dnRStat, DNUDREQ_FSM_TREvent_t event);
extern int8_t *DNUDReq_get_action_str(DNUDREQ_FSM_action_t action);
extern DNUDREQ_FSM_state_t DNUDReq_get_state(DNUDReq_fsm_t *dnRStat);
extern int8_t *DNUDReq_get_state_str(DNUDReq_fsm_t *dnRStat);
extern void DNUDReq_init_fsm(DNUDReq_fsm_t *dnRStat);

#endif /* DISCODN_CLIENT_DISCOC_DNUDREQ_FSM_H_ */
