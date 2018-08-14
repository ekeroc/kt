/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoDNSrv_simulator_export.h
 *
 * This component try to simulate DN
 *
 */

#ifndef DISCODN_SIMULATOR_DISCODNSRV_SIMULATOR_EXPORT_H_
#define DISCODN_SIMULATOR_DISCODNSRV_SIMULATOR_EXPORT_H_

extern discoC_conn_t *discoDNSrv_setConn_errfn(discoC_conn_t *conn);

extern void init_DNSimulator_manager(void);
extern void rel_DNSimulator_manager(void);

#endif /* DISCODN_SIMULATOR_DISCODNSRV_SIMULATOR_EXPORT_H_ */
