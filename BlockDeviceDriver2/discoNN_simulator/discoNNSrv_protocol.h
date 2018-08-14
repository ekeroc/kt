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

typedef struct NNSimulator_ack_packet {
    uint64_t magic_num1;
    uint16_t opcode;
    uint16_t pkt_dataSize;
    int64_t cnReqID;
} __attribute__((__packed__)) NNSmltor_ackpkt_t;

extern void NNClient_MDReqHder_ntoh(CN2NN_getMDReq_hder_t *pkt_hder);
#ifdef DISCO_PREALLOC_SUPPORT
extern void NNClient_FSReportReqHder_ntoh(CN2NN_fsReportMD_hder_t *pkt_hder);
extern void NNClient_FSReportUSHder_ntoh(CN2NN_fsReportFS_hder_t *pkt_hder);
extern void NNClient_FSAllocSpaceHder_ntoh(CN2NN_fsAllocMD_hder_t *pkt_hder);
extern int32_t gen_fsReportAlloc_ackHeader(int8_t *pkt_buffer, uint16_t opcode, uint16_t dataSize);
extern int32_t NNClient_FSReportReq_ntoh(int8_t *pkt_buffer);
#endif
extern int32_t gen_MDReq_ackHeader(int8_t *pkt_buffer, uint16_t opcode, uint16_t dataSize, uint64_t cnReqID);
extern int32_t gen_MDReq_MDBody1(int8_t *pkt_buffer, uint16_t numLoc, uint16_t len_hbids);
extern int32_t gen_MDReq_DNLoc(int8_t *pkt_buffer, int16_t len_ipaddr, int8_t *ipaddr,
        int32_t port, uint16_t num_rlo, uint64_t *phy_rlo);
#endif /* DISCODN_SIMULATOR_DISCODNSRV_PROTOCOL_H_ */
