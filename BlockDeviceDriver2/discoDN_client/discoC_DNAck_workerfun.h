/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_DNAck_workerfun.h
 *
 */

#ifndef DISCODN_CLIENT_DISCOC_DNACK_WORKERFUN_H_
#define DISCODN_CLIENT_DISCOC_DNACK_WORKERFUN_H_

struct discoC_DNUData_request;
extern int32_t handle_DNAck_Write(DNUData_Req_t *dn_req, int32_t access_result);
extern void handle_DNAck_Read(DNUData_Req_t *dn_req, data_mem_chunks_t *payload,
        uint16_t dn_resp);
extern void handle_DNAck_IOError(DNUData_Req_t *dn_req, uint16_t dn_resp);
#endif /* DISCODN_CLIENT_DISCOC_DNACK_WORKERFUN_H_ */
