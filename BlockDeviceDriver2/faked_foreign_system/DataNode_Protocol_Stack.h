/*
 * DataNode_Protocol_Stack.h
 *
 *  Created on: 2012/8/16
 *      Author: 990158
 */

#ifndef DATANODE_PROTOCOL_STACK_H_
#define DATANODE_PROTOCOL_STACK_H_

#define	DN_REQUEST_PAYLOAD_WRITE	3
#define DN_REQUEST_PAYLOAD_READ		4
#define DN_REQUEST_PAYLOAD_WRITE_AFTER_GET_OLD	5

#define DN_RESPONSE_ACK_READ		101
#define	DN_RESPONSE_ACK_MEM_WRITE	102
#define	DN_RESPONSE_ACK_DISK_WRITE	103
#define DN_RESPONSE_ACK_MEM_WRITE_AND_GET_OLD	104
#define DN_RESPONSE_ACK_DISK_WRITE_AND_GET_OLD	105
#define	DN_RESPONSE_ACK_ERR_IOException	201
#define	DN_RESPONSE_ACK_ERR_IOFailBUTLUNOKException	202
#define	DN_RESPONSE_ACK_ERR_FailedLUNException	203

#define	DMS_LOGICAL_SECTOR_SIZE	512

#define DN_REQUEST_RESERVED_BYTES	3
#define DN_RESPONSE_RESERVED_BYTES	4

struct dn_request_header {
	unsigned long long magic_number1;
	char opcode;
	unsigned short num_of_hbids;
	unsigned short num_of_triplets;
	char reserved[DN_REQUEST_RESERVED_BYTES];
	unsigned int namenode_req_id;
	unsigned int datanode_req_id;
	unsigned long long magic_number2;
} __attribute__ ((__packed__));

struct dn_request_old_loc {
	char hostname[16];
	int port;
	unsigned long long old_hbid;
	unsigned long long triplet;
} __attribute__ ((__packed__));

struct dn_request {
	struct dn_request_header header;
	unsigned long long * hbids;
	unsigned char * masks;
	unsigned long long * tripltes;
	struct dn_request_old_loc * old_dn_locs;
	char * payload;
	char response_type;
	unsigned short payload_num_sectors;
	char response_reserved[DN_RESPONSE_RESERVED_BYTES];
};

struct dn_request * Receive_DataNode_Request(int sock_fd);
int Send_DataNode_Ack(int sockfd, struct dn_request * dn_req);
#endif /* DATANODE_PROTOCOL_STACK_H_ */
