/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * mclient.h
 *
 */

#ifndef _MCLIENT_H_
#define _MCLIENT_H_

extern int32_t dms_chdev_fd;
extern int32_t clientnode_running_mode;
extern int32_t g_nn_VO_port;

extern struct hostent *g_nn_host;
extern int8_t *g_namenode_address;
extern uint32_t gdef_nnSrvIPAddr;
extern int8_t gdef_nnSrvName[MAX_LEN_NNSRV_NM];

extern int32_t g_nnSimulator_on;

#endif /* _MCLIENT_H_ */
