/*
 * Faked_DataNode.c
 *
 *  Created on: 2012/8/16
 *      Author: 990158
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <signal.h>
#include <pthread.h>
#include <signal.h>
#include "socket_util.h"
#include "Faked_DataNode.h"
#include "DataNode_Protocol_Stack.h"
#include "DataNode_Data_Manager.h"

int DataNode_Service_port	= DEFAULT_DATANODE_SERVICE_PORT;
int server_socket_fd;

void sighandler(int sig) {
	syslog(LOG_ERR, "Catch signal, terminate faked DN service\n");
	if(server_socket_fd != -1) {
		shutdown(server_socket_fd, SHUT_RDWR);
		close(server_socket_fd);
	}

	exit(1);
}

void register_signal_handler() {
	signal(SIGINT, &sighandler);
	signal(SIGTERM, &sighandler);
}

int Initialize_Faked_DataNode() {
	server_socket_fd = 0;

	return 0;
}

int Process_DataNode_Request(int sock_fd) {
	struct dn_request * dn_req = NULL;
	int ret = 0;

	syslog(LOG_ERR, "Process_DataNode_Request start, ready to receive request\n");
	dn_req = Receive_DataNode_Request(sock_fd);

	if(dn_req == NULL)
		return -1;

	syslog(LOG_ERR, "Process_DataNode_Request pretest\n");
	syslog(LOG_ERR, "The header num of sector is %d\n", dn_req->payload_num_sectors);

	syslog(LOG_ERR, "Process_DataNode_Request cmd: %d\n", dn_req->header.opcode);
	switch(dn_req->header.opcode) {
		case DN_REQUEST_PAYLOAD_WRITE:
			dn_req->response_type = DN_RESPONSE_ACK_MEM_WRITE;
			syslog(LOG_ERR, "Ready to send memory ack for WRITE PAYLOAD\n");
			Send_DataNode_Ack(sock_fd, dn_req);
			break;
		case DN_REQUEST_PAYLOAD_READ:
			syslog(LOG_ERR, "ready to handle DN_REQUEST_PAYLOAD_READ request: num of bytes %d (%d sectors)\n", ((int)dn_req->payload_num_sectors)*((int)DMS_LOGICAL_SECTOR_SIZE),
					(int)dn_req->payload_num_sectors);
			dn_req->payload = (char*)malloc(sizeof(char)*((int)dn_req->payload_num_sectors)*((int)DMS_LOGICAL_SECTOR_SIZE));
			memset(dn_req->payload, 0, sizeof(char)*((int)dn_req->payload_num_sectors)*((int)DMS_LOGICAL_SECTOR_SIZE));

			break;
		case DN_REQUEST_PAYLOAD_WRITE_AFTER_GET_OLD:
			dn_req->response_type = DN_RESPONSE_ACK_MEM_WRITE_AND_GET_OLD;
			syslog(LOG_ERR, "Ready to send memory ack for WRITE after get old PAYLOAD\n");
			Send_DataNode_Ack(sock_fd, dn_req);
			break;
		default:
			syslog(LOG_ERR, "unknown request %d\n", dn_req->header.opcode);
			dn_req->response_type = -1;
			break;
	}

	if(dn_req->response_type != -1) {
		syslog(LOG_ERR, "Ready to process payload_operation_req: num of sector %d\n", dn_req->payload_num_sectors);
		dn_req->response_type = Process_Payload_Operation_Request(dn_req->header.opcode, dn_req->header.num_of_hbids, dn_req->hbids,
				dn_req->masks, dn_req->header.num_of_triplets, dn_req->tripltes, dn_req->payload, dn_req->payload_num_sectors,
				dn_req->old_dn_locs);

		syslog(LOG_ERR, "ACK is %d\n", dn_req->response_type);
		Send_DataNode_Ack(sock_fd, dn_req);
	}

	free(dn_req->hbids);
	free(dn_req->masks);
	if(dn_req->old_dn_locs != NULL)
		free(dn_req->old_dn_locs);
	free(dn_req->tripltes);
	if(dn_req->payload != NULL)
		free(dn_req->payload);
	free(dn_req);

	return ret;
}

void dn_thread_function(void * arg) {
	int sockfd = *((int*)arg);
	int ret = 0;

	while(ret >= 0) {
		ret = Process_DataNode_Request(sockfd); //Should create a thread to handle
	}

	syslog(LOG_ERR, "nn_thread_function exit loop\n");
}

int Run_Faked_DataNode() {
	int clientfd, ret;
	struct sockaddr_in client_addr;
	int addrlen = 0;
	pthread_attr_t attr;
	pthread_t id;

	syslog(LOG_ERR, "Ready to create socket server with port=%d\n", DataNode_Service_port);
	server_socket_fd = create_socket_server(DataNode_Service_port);

	if(server_socket_fd < 0)
		return -1;

	while(1) {
		addrlen = sizeof(client_addr);
		memset(&client_addr, 0, sizeof(struct sockaddr_in));
		clientfd = socket_accept(server_socket_fd, (struct sockaddr*)&client_addr, &addrlen);
		syslog(LOG_ERR, "Got a client connection, ready to process this\n");

		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

		ret = pthread_create(&id, &attr, (void*)dn_thread_function, (void *)&clientfd);
		if(ret == -1) {
			syslog(LOG_ERR, "Socket server create pthread error! exist\n");
			exit(1);
		}
	}
}

int main(int argc, char ** argv) {

	if(argc > 1) {
		DataNode_Service_port = atol(argv[1]);
	}


	syslog(LOG_ERR, "start Faked Datanode services at port %d\n", DataNode_Service_port);

	register_signal_handler();
	Run_Faked_DataNode();
	return 0;
}
