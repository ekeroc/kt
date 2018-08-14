/*
 * Copyright (C) 2010 - 2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_NNAck_acquireMD.h
 *
 * Provide function to process ack of metadata alloc/query request from namenode
 *
 */

#ifndef DISCONN_CLIENT_DISCOC_NNACK_ACQUIREMDREQ_H_
#define DISCONN_CLIENT_DISCOC_NNACK_ACQUIREMDREQ_H_

extern void NNAckProc_acquireMDReq(int8_t *mem, CN2NN_MDResp_hder_t *resppkt);

#endif /* DISCONN_CLIENT_NN_ACK_ACQUIREMDREQ_H_ */
