/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoNNSrv_simulator_export.h
 *
 * This component try to simulate NN
 *
 */

#ifndef DISCODN_SIMULATOR_DISCONNSRV_SIMULATOR_EXPORT_H_
#define DISCODN_SIMULATOR_DISCONNSRV_SIMULATOR_EXPORT_H_

extern discoC_conn_t *discoNNSrv_setConn_errfn(discoC_conn_t *conn);

extern void init_NNSimulator_manager(void);
extern void rel_NNSimulator_manager(void);

#endif /* DISCODN_SIMULATOR_DISCONNSRV_SIMULATOR_EXPORT_H_ */
