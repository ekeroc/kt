/*
 * DataNode_Protocol_Stack.c
 *
 *  Created on: 2012/8/16
 *      Author: 990158
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <syslog.h>
#include "DataNode_Protocol_Stack.h"
#include "socket_util.h"
#include "utility.h"

int Receive_DataNode_Request_Header(int sock_fd, struct dn_request_header * header) {
	int ret = 0;

	ret = socket_recvfrom(sock_fd, (char *)header, sizeof(struct dn_request_header));

	syslog(LOG_ERR, "Receive_DataNode_Request_Header start: received %d expected %d\n", ret,  sizeof(struct dn_request_header));

	if(ret != sizeof(struct dn_request_header)) {
		syslog(LOG_ERR, "ERROR: Cannot receive expected size %d (receive %d)\n", sizeof(struct dn_request_header), ret);
		ret = -1;
	} else {
		header->magic_number1 = ntohll(header->magic_number1);
		header->num_of_hbids = ntohs(header->num_of_hbids);
		header->num_of_triplets = ntohs(header->num_of_triplets);
		header->namenode_req_id = ntohl(header->namenode_req_id);
		header->datanode_req_id = ntohl(header->datanode_req_id);
		header->magic_number2 = ntohll(header->magic_number2);

		syslog(LOG_ERR, "Receive_DataNode_Request_Header num_of_hb:%d num_of_triplet:%d cid:%d did:%d\n", header->num_of_hbids,
				header->num_of_triplets, header->namenode_req_id, header->datanode_req_id);

	}

	return ret;
}

int Receive_DataNode_Request_Common_Body(int sock_fd, struct dn_request * req) {
	int ret = 0, expected_size = 0;

	syslog(LOG_ERR, "Receive_DataNode_Request_Common_Body start\n");
	req->hbids = (unsigned long long *)malloc(sizeof(unsigned long long)*req->header.num_of_hbids);
	req->tripltes = (unsigned long long *)malloc(sizeof(unsigned long long)*req->header.num_of_triplets);
	req->masks = (char *)malloc(sizeof(char)*req->header.num_of_hbids);

	expected_size = sizeof(unsigned long long)*req->header.num_of_hbids + sizeof(char)*req->header.num_of_hbids +
			sizeof(unsigned long long)*req->header.num_of_triplets;

	ret = socket_recvfrom(sock_fd, (char*)req->hbids, sizeof(unsigned long long)*req->header.num_of_hbids);
	ret += socket_recvfrom(sock_fd, (char*)req->masks, sizeof(char)*req->header.num_of_hbids);
	ret += socket_recvfrom(sock_fd, (char*)req->tripltes, sizeof(unsigned long long)*req->header.num_of_triplets);

	if(ret != expected_size) {
		free(req->hbids);	free(req->tripltes);	free(req->masks);

		req->hbids = NULL;	req->tripltes = NULL;	req->masks = NULL;
		syslog(LOG_ERR, "Cannot receive expected size %d ret %d\n", expected_size, ret);
		return -1;
	} else
		ret = 0;

	req->payload_num_sectors = Get_Num_Of_Sector_From_Mask(req->masks, req->header.num_of_hbids);
	syslog(LOG_ERR, "num of sector %d\n", req->payload_num_sectors);
	return ret;
}

int Receive_DataNode_Request_Write_After_Get_Old_Old_LOC(int sock_fd, struct dn_request * req) {
	int ret = 0, expected_size = 0, i;

	syslog(LOG_ERR, "Ready to receive old locs : num of old %d\n", req->header.num_of_hbids);
	expected_size = sizeof(struct dn_request_old_loc)*req->header.num_of_hbids;

	req->old_dn_locs = (struct dn_request_old_loc *)malloc(sizeof(struct dn_request_old_loc)*req->header.num_of_hbids);

	ret = socket_recvfrom(sock_fd, (char*)req->old_dn_locs, sizeof(struct dn_request_old_loc)*req->header.num_of_hbids);

	if(ret != expected_size) {
		syslog(LOG_ERR, "Cannot received %d bytes, actully received %d bytes\n", expected_size, ret);
		return -1;
	}
	for(i=0;i<req->header.num_of_hbids;i++) {
		req->old_dn_locs[i].port = ntohl(req->old_dn_locs[i].port);
		req->old_dn_locs[i].old_hbid = ntohll(req->old_dn_locs[i].old_hbid);
		req->old_dn_locs[i].triplet = ntohll(req->old_dn_locs[i].triplet);

		syslog(LOG_ERR, "Old_loc[%d]: hostname:port = %s:%d  old_hb=%llu  triplet=%llu\n", i,
				req->old_dn_locs[i].hostname, req->old_dn_locs[i].port,
				req->old_dn_locs[i].old_hbid, req->old_dn_locs[i].triplet);
	}
	return 0;
}

int Get_Num_Of_Sector_From_Mask(char * masks, int num_of_masks) {
	int ret = 0, i;

	for(i=0;i<num_of_masks;i++) {
		ret+=Calculate_Num_of_1_in_char(masks[i]);
	}

	return ret;
}

int Receive_DataNode_Request_Write_Payload(int sock_fd, struct dn_request * req) {
	int Num_Of_Sector = 0, num_of_bytes = 0, ret = 0;

	syslog(LOG_ERR, "ready to receive num of sect: %d\n", req->payload_num_sectors);
	num_of_bytes = ((int)req->payload_num_sectors)*((int)DMS_LOGICAL_SECTOR_SIZE);

	if(num_of_bytes <= 0)
		return -1;

	syslog(LOG_ERR, "ready to receive num of bytes: %d\n", num_of_bytes);

	req->payload = (char*)malloc(sizeof(char)*num_of_bytes);

	ret = socket_recvfrom(sock_fd, req->payload, num_of_bytes);

	syslog(LOG_ERR, "after receive num of bytes: %d\n", ret);

	if(ret != num_of_bytes) {
		free(req->payload);
		ret = -1;
	}

	syslog(LOG_ERR, "Receive_DataNode_Request_Write_Payload result:%d\n", ret);
	return ret;
}

int Receive_DataNode_WRITE_Request(int sock_fd, struct dn_request * req) {
	int ret = 0;

	syslog(LOG_ERR, "Receive_DataNode_WRITE_Request : ready recv common body\n");
	ret = Receive_DataNode_Request_Common_Body(sock_fd, req);

	syslog(LOG_ERR, "Receive_DataNode_WRITE_Request : DONE recv common body %d\n", ret);

	if(ret < 0)
		ret = -1;
	else {
		ret = Receive_DataNode_Request_Write_Payload(sock_fd, req);
	}

	return ret;
}

int Receive_DataNode_READ_Request(int sock_fd, struct dn_request * req) {
	int ret = 0;

	ret = Receive_DataNode_Request_Common_Body(sock_fd, req);

	return ret;
}

int Receive_DataNode_WRITE_After_Get_Old_Request(int sock_fd, struct dn_request * req) {
	int ret = 0;

	ret = Receive_DataNode_Request_Common_Body(sock_fd, req);

	if(ret < 0)
		return ret;

	ret = Receive_DataNode_Request_Write_After_Get_Old_Old_LOC(sock_fd, req);

	if(ret < 0)
		return ret;

	ret = Receive_DataNode_Request_Write_Payload(sock_fd, req);

	return ret;
}

struct dn_request * Receive_DataNode_Request(int sock_fd) {
	struct dn_request * req = NULL;
		int ret;

	req = (struct dn_request *)malloc(sizeof(struct dn_request));
	ret = Receive_DataNode_Request_Header(sock_fd, &req->header);

	if(ret == -1) {
		syslog(LOG_ERR, "Cannot receive request from clientnode\n");
		free(req);
		return NULL;
	} else {
		req->old_dn_locs = NULL;
		req->payload = NULL;
	}

	syslog(LOG_ERR, "Receive_DataNode_Request cmd: %d\n", req->header.opcode);
	switch(req->header.opcode) {
		case DN_REQUEST_PAYLOAD_WRITE:
			syslog(LOG_ERR, "Ready call Receive_DataNode_WRITE_Request\n");
			ret = Receive_DataNode_WRITE_Request(sock_fd, req);
			break;
		case DN_REQUEST_PAYLOAD_READ:
			ret = Receive_DataNode_READ_Request(sock_fd, req);
			break;
		case DN_REQUEST_PAYLOAD_WRITE_AFTER_GET_OLD:
			ret = Receive_DataNode_WRITE_After_Get_Old_Request(sock_fd, req);
			break;
		default:
			syslog(LOG_ERR, "Receive Unknown command from clientnode : %d\n", req->header.opcode);
			ret = -1;
			break;
	}

	if(ret < 0) {
		free(req);
		syslog(LOG_ERR, "Cannot receive request from clientnode\n");
		req = NULL;
	}

	return req;
}

int Send_DataNode_Ack(int sockfd, struct dn_request * dn_req) {
	int ret = 0;
	unsigned long long m_number = 0;
	unsigned short num_of_sector = 0;
	unsigned int id = 0;
	char redundant_byte = 0;

	syslog(LOG_ERR, "HEADER C: magic num 1: %llu\n", dn_req->header.magic_number1);
	syslog(LOG_ERR, "HEADER C: response type: %d\n", dn_req->response_type);
	syslog(LOG_ERR, "HEADER C: payload_num_sectors: %d\n", dn_req->payload_num_sectors);
	syslog(LOG_ERR, "HEADER C: nn id: %llu\n", dn_req->header.namenode_req_id);
	syslog(LOG_ERR, "HEADER C: dn id: %llu\n", dn_req->header.datanode_req_id);
	syslog(LOG_ERR, "HEADER C: magic num 2: %llu\n", dn_req->header.magic_number2);

	m_number = htonll(dn_req->header.magic_number1);
	ret = socket_sendto(sockfd, (char*)&m_number, sizeof(unsigned long long));
	ret+= socket_sendto(sockfd, (char*)&dn_req->response_type, sizeof(char));

	ret+= socket_sendto(sockfd, &redundant_byte, sizeof(char));
	num_of_sector = htons(dn_req->payload_num_sectors);
	ret+= socket_sendto(sockfd, (char*)&num_of_sector, sizeof(unsigned short));

	ret+= socket_sendto(sockfd, dn_req->response_reserved, sizeof(char)*DN_RESPONSE_RESERVED_BYTES);
	id = htonl(dn_req->header.namenode_req_id);
	ret+= socket_sendto(sockfd, (char*)&id, sizeof(unsigned int));
	id = htonl(dn_req->header.datanode_req_id);
	ret+= socket_sendto(sockfd, (char*)&id, sizeof(unsigned int));
	ret+= socket_sendto(sockfd, (char*)&m_number, sizeof(unsigned long long));

	if(dn_req->response_type == DN_RESPONSE_ACK_READ) {
		syslog(LOG_ERR, "Ready to send payload with sectors: %d (num of bytes %d)\n", (int)dn_req->payload_num_sectors, (int)dn_req->payload_num_sectors*(int)DMS_LOGICAL_SECTOR_SIZE);
		ret+= socket_sendto(sockfd, dn_req->payload, sizeof(char)*((int)dn_req->payload_num_sectors)*((int)DMS_LOGICAL_SECTOR_SIZE));
	}
	return 0;
}
