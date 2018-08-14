/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoNNOVWSrv_simulator_private.h
 *
 * This component try to simulate NN OVW
 *
 */

#ifndef DISCODN_SIMULATOR_DISCONNOVWSRV_SIMULATOR_PRIVATE_H_
#define DISCODN_SIMULATOR_DISCONNOVWSRV_SIMULATOR_PRIVATE_H_

typedef struct disco_NNOVW_simulator_Manager {
    struct list_head NNOVWSmltor_pool;
    struct mutex NNOVWSmltor_plock;
    int32_t NNOVWSmltor_mpoolID; //memory pool id
    atomic_t num_NNOVWSmltor;
} NNOVWSimulator_Manager_t;

NNOVWSimulator_Manager_t ovwSmltorMgr;

#endif /* DISCODN_SIMULATOR_DISCONNOVWSRV_SIMULATOR_PRIVATE_H_ */
