/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_DN_protocol.c
 *
 * This component aim for paring CN - DN protocol
 *
 */

#ifndef DISCOC_DN_PROTOCOL_H_
#define DISCOC_DN_PROTOCOL_H_

#if (DN_PROTOCOL_VERSION == 1)
typedef struct discoCN2DN_UDataReq_pkt_header {
    uint64_t magic_num1;   //magic number to indicate packet header start
    uint8_t opcode;        //opcode : discoC_DNC_Manager.h: DNUDReq_opcode_t
    uint16_t num_hbids;    //how many 4K data inside request to be sent
    uint16_t num_triplets; //how many physical location for storing 4K data
    uint8_t reserved[3];
    uint32_t nnreq_id;     //caller ID (payload request ID)
    uint32_t dnreq_id;     //DNRequst ID
    uint64_t magic_num2;   //magic number id indicate packet header end
} __attribute__((__packed__)) CN2DN_UDRWReq_hder_t;
#else
typedef struct disco_datanode_req_packet {
    uint32_t magic_num_s;
    uint32_t pkt_size;
    uint16_t opcode;
    uint16_t num_hbids;
    uint16_t num_triplets;
    uint2_t reserved[2];
    uint32_t nnreq_id;
    uint32_t dnreq_id;
    //uint32_t magic_num_e; //magic end won't at header end
} __attribute__((__packed__)) CN2DN_UDRWReq_hder_t;
#endif

#if (DN_PROTOCOL_VERSION == 1)
typedef struct discoCN2DN_UDataResp_pkt_header {
    uint64_t magic_num1;  //magic number to indicate packet header start
    uint8_t responseType; //opcode : discoC_DNC_Manager.h: DNUDResp_opcode_t / DNUDResp_err_code_t
    uint32_t num_of_lb;   //how many 4K data inside request to be received
    int8_t reserved[3];
    uint32_t pReqID;      //caller ID of DN request
    uint32_t dnid;        //DN Request ID
    uint64_t magic_num2;  //magic number id indicate packet header end
} __attribute__((__packed__)) CN2DN_UDRWResp_hder_t;
#else
struct disco_datanode_ack_packet {
    uint32_t magic_num1;
    uint16_t responseType;
    uint16_t num_of_lb;
    uint32_t num_sects;
    uint32_t pReqID;
    uint32_t dnid;
    uint32_t magic_num2;
} __attribute__((__packed__)) CN2DN_UDRWResp_hder_t;
#endif

extern uint8_t *DNProto_ReadMask_buffer;

extern void DNProto_gen_ReqPacket(DNUData_Req_t *dnreq, int32_t *packet_len, int8_t *result);
extern void DNProto_ReqAck_ntoh(CN2DN_UDRWResp_hder_t *pkt);
extern int32_t DNProto_gen_HBPacket(int8_t *pkt_buffer, int32_t max_buff_size);

#endif /* DISCOC_DN_PROTOCOL_H_ */
