/*
 * Copyright (C) 2010-2020 by Division F / ICL
 * Industrial Technology Research Institute
 *
 * discoC_DNC_workerfun.h
 *
 */

#ifndef DISCODN_CLIENT_DISCOC_DNCL_WORKERFUNC_H_
#define DISCODN_CLIENT_DISCOC_DNCL_WORKERFUNC_H_

#if (DN_PROTOCOL_VERSION == 1)
#define DATANODE_ACK_SIZE   32
#else
#define DATANODE_ACK_SIZE   20
#endif

#if (DN_PROTOCOL_VERSION == 2)
#define DN_MAGIC_S  0xa5a5a5a5
#define DN_MAGIC_E  0xdeadbeef
#endif

extern void DNCWker_send_DNUDReq(DNUData_Req_t *dnReq, int8_t *packet);
#endif /* DISCODN_CLIENT_DISCOC_DN_WORKERFUNC_H_ */
