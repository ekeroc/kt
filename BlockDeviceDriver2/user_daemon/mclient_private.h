/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * mclient_private.h
 *
 */

#ifndef _MCLIENT_PRIVATE_H_
#define MCLIENT_PRIVATE_H_

#define NUM_STARTUP_PARAM   2
#define NNAddr_local  "localhost"
#define NNAddr_default  "dms.ccma.itri"
#define NamenodeConfigFile  "/usr/cloudos/dms/conf/core-site.xml"
#define MAX_CHECK_DMS_SERVICES_CNT  1800    //30 min
#define DEFAULT_NN_PORT 1234

int8_t *dms_config_file = "/usr/cloudos/dms/conf/core-site.xml";
int8_t *shutdown_chk_file = "/usr/cloudos/dms/discoC_running.tag";

pthread_mutex_t rel_res_lock = PTHREAD_MUTEX_INITIALIZER;

int32_t dms_chdev_fd;
int32_t clientnode_running_mode = 0;
int32_t g_nn_VO_port;
struct hostent *g_nn_host;
int8_t *g_namenode_address;

uint32_t gdef_nnSrvIPAddr;
int8_t gdef_nnSrvName[MAX_LEN_NNSRV_NM];
int32_t g_nnSimulator_on;
int8_t g_nnSimulator_ipaddr[] = "192.168.100.1";

#endif /* _MCLIENT_PRIVATE_H_ */
