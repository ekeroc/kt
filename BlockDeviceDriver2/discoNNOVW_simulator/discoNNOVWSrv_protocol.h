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

extern void NNOVWSrv_gen_ackpkt(ovw_res *respPkt, uint16_t acktype, uint16_t subopcode,
        uint64_t volID, uint64_t len_bytes);

#endif /* DISCODN_SIMULATOR_DISCODNSRV_PROTOCOL_H_ */
