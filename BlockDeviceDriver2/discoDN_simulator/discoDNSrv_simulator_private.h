/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoDNSrv_simulator_private.h
 *
 * This component try to simulate DN
 *
 */


#ifndef DISCODN_SIMULATOR_DISCODNSRV_SIMULATOR_PRIVATE_H_
#define DISCODN_SIMULATOR_DISCODNSRV_SIMULATOR_PRIVATE_H_

typedef struct disco_DN_simulator_Manager {
    struct list_head DNSmltor_pool;
    struct mutex DNSmltor_plock;
    int32_t DNSmltor_mpoolID; //memory pool id
    atomic_t num_DNSmltor;
} DNSimulator_Manager_t;

DNSimulator_Manager_t dnSmltorMgr;

#endif /* DISCODN_SIMULATOR_DISCODNSRV_SIMULATOR_PRIVATE_H_ */
