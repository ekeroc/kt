/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoNNOVWSrv_simulator_export.h
 *
 * This component try to simulate NN OVW Module
 *
 */

#ifndef DISCODN_SIMULATOR_DISCONNOVWSRV_SIMULATOR_EXPORT_H_
#define DISCODN_SIMULATOR_DISCONNOVWSRV_SIMULATOR_EXPORT_H_

extern discoC_conn_t *discoNNOVWSrv_setConn_errfn(discoC_conn_t *conn);

extern void init_NNOVWSimulator_manager(void);
extern void rel_NNOVWSimulator_manager(void);

#endif /* DISCODN_SIMULATOR_DISCONNOVWSRV_SIMULATOR_EXPORT_H_ */
