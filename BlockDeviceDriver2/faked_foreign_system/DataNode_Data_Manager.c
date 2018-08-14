/*
 * DataNode_Data_Manager.c
 *
 *  Created on: 2012/8/16
 *      Author: 990158
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include "DataNode_Protocol_Stack.h"

int File_Target_Write() {
	return 0;
}

int File_Target_Read() {
	return 0;
}

int Remote_DN_Target_Read() {
	return 0;
}

int Memory_Target_Write() {
	return 0;
}

int Memory_Target_Read() {
	return 0;
}

int NULL_Target_Write() {
	return 0;
}

int NULL_Target_Read() {
	return 0;
}

char Process_Payload_Operation_Request(char req_type, unsigned short num_of_hbids, unsigned long long * hbids,
		unsigned char * masks, unsigned short num_of_triplets, unsigned long long * triplets, char * payload,
		unsigned short num_of_sector, struct dn_request_old_loc * old_loc) {
	char dn_ack = -1;

	syslog(LOG_ERR, "Ready to process %d sectors\n", num_of_sector);
	switch(req_type) {
		case DN_REQUEST_PAYLOAD_WRITE:
			dn_ack = DN_RESPONSE_ACK_DISK_WRITE;
			break;
		case DN_REQUEST_PAYLOAD_READ:
			dn_ack = DN_RESPONSE_ACK_READ;
			syslog(LOG_ERR, "Process_Payload_Operation_Request: read payload sectors %d, bytes %d\n",
					num_of_sector, (int)num_of_sector*(int)DMS_LOGICAL_SECTOR_SIZE);
			break;
		case DN_REQUEST_PAYLOAD_WRITE_AFTER_GET_OLD:
			dn_ack = DN_RESPONSE_ACK_DISK_WRITE_AND_GET_OLD;
			break;
		default:
			dn_ack = DN_RESPONSE_ACK_ERR_IOException;
			break;
	}

	return dn_ack;
}
