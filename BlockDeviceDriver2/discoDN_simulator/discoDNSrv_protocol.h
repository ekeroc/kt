/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoDNSrv_protocol.c
 *
 * This component provide the socket protocol parsing and packing of DN simulator
 *
 */

#ifndef DISCODN_SIMULATOR_DISCODNSRV_PROTOCOL_H_
#define DISCODN_SIMULATOR_DISCODNSRV_PROTOCOL_H_

extern void DNClient_ReqHder_ntoh(CN2DN_UDRWReq_hder_t *pkt_hder);
extern void DNSrv_gen_ackpkt(CN2DN_UDRWResp_hder_t *pkt, uint8_t acktype, uint32_t num_of_lb,
        uint32_t pReqID, uint32_t dnReqID);

#endif /* DISCODN_SIMULATOR_DISCODNSRV_PROTOCOL_H_ */
