/*
 * Copyright (C) 2010 - 2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_NNMDReq_fsm.h
 *
 * Define state machine operation of NN Metadata Request
 *
 */

#ifndef DISCONN_CLIENT_DISCOC_NNMDREQ_FSM_H_
#define DISCONN_CLIENT_DISCOC_NNMDREQ_FSM_H_

//get: same meaning with acquire metadata - get and allocate)
extern int8_t *NNMDgetReq_get_state_str(NNMDReq_fsm_t *nnReqStat);
extern int8_t *NNMDgetReq_get_action_str(NNMDgetReq_FSM_action_t action);
extern NNMDgetReq_FSM_action_t NNMDgetReq_update_state(NNMDReq_fsm_t *nnReqStat, NNMDgetReq_FSM_TREvent_t event);
extern NNMDgetReq_FSM_state_t NNMDgetReq_get_state(NNMDReq_fsm_t *nnReqStat);

//ci: commit (same meaning with report metadata)
extern int8_t *NNMDciReq_get_state_str(NNMDReq_fsm_t *nnReqStat);
extern int8_t *NNMDciReq_get_action_str(NNMDciReq_FSM_action_t action);
extern NNMDciReq_FSM_action_t NNMDciReq_update_state(NNMDReq_fsm_t *nnReqStat, NNMDciReq_FSM_TREvent_t event);
extern NNMDciReq_FSM_state_t NNMDciReq_get_state(NNMDReq_fsm_t *nnReqStat);

extern void NNMDReq_init_fsm(NNMDReq_fsm_t *nnReqStat);

#endif /* DISCONN_CLIENT_DISCOC_NNMDREQ_FSM_H_ */
