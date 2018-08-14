#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <syslog.h>
#include <pthread.h>
#include "socket_util.h"
#include "Faked_NameNode.h"
#include "NameNode_Protocol_Stack.h"
#include "NN_Volume_Manager.h"
#include "NN_Metadata_Manager.h"
#include "utility.h"

int server_socket_fd;

void sighandler(int sig) {
	syslog(LOG_ERR, "Catch signal, terminate faked NN service\n");
	if(server_socket_fd != -1) {
		shutdown(server_socket_fd, SHUT_RDWR);
		close(server_socket_fd);
	}

	Release_DMS_Volume_Manager();
	exit(1);
}

void register_signal_handler() {
	signal(SIGINT, &sighandler);
	signal(SIGTERM, &sighandler);
}

int Initialize_Faked_NameNode() {
	server_socket_fd = 0;
	Initialize_DMS_Volume_Manager();
	Initialize_DMS_Metadata_Manager();

	return 0;
}

struct nn_response_header * Process_NameNode_Query_Volume_Request(struct nn_request_volume_info * nn_request) {
	struct nn_response_volume_info * nn_response = NULL;
	int ret = 0;
	struct dms_volume_info * vol_info = NULL;
	struct dms_volume_info * a = NULL;

	syslog(LOG_ERR, "Process_NameNode_Query_Volume_Request start vol_id : %d\n", nn_request->volume_id);
	vol_info = DMS_Query_Volume_Info((unsigned long long)nn_request->volume_id);

	nn_response = (struct nn_response_volume_info *)malloc(sizeof(struct nn_response_volume_info));
	nn_response->header.opcode = NN_REQUEST_QUERY_VOLUME_INFO;
	if(vol_info != NULL) {
		nn_response->replica_factor = vol_info->replica_factor;
		nn_response->volume_capacity = vol_info->volume_capacity;
	} else {
		nn_response->replica_factor = 0;
		nn_response->volume_capacity = 0;
	}

	return (struct nn_response_header *)nn_response;
}

struct nn_response_header * Process_NameNode_Create_Volume_Request(struct nn_request_volume_create * nn_request) {
	unsigned long long vol_id = 0;
	int ret = 0;
	struct nn_response_volume_create * nn_response = NULL;

	syslog(LOG_ERR, "Process_NameNode_Create_Volume_Request start\n");
	nn_response = (struct nn_response_volume_create *)malloc(sizeof(struct nn_response_volume_create));

	syslog(LOG_ERR, "Process_NameNode_Create_Volume_Request middle vol_cap=%llu\n", nn_request->volume_capacity);

	vol_id = DMS_Create_Volume(nn_request->volume_capacity);
	nn_response->header.opcode = NN_REQUEST_CREATE_VOLUME;
	nn_response->volume_id = vol_id;

	syslog(LOG_ERR, "Process_NameNode_Create_Volume_Request DONE vol_id=%llu\n", nn_response->volume_id);
	return (struct nn_response_header *)nn_response;
}

int Transform_LR_to_Metadata_Response(struct nn_request_metadata * nn_request,
		struct nn_response_metadata * nn_response, struct LocatedRequest ** lr, int num_of_lrs) {
	int ret = 0, i, j, k;
	struct nn_metadata * ptr_nn_md = NULL;
	struct nn_metadata_dn_location * ptr_dn_loc = NULL;

	syslog(LOG_ERR, "Transform_LR_to_Metadata_Response start\n");

	nn_response->magic_number = 0xa5a5a5a5a5a5a5a5ll;
	nn_response->response_type = 0; //RESPONSE_METADATA_REQUEST
	nn_response->metadata_packet_len = 0;


	nn_response->request_id = nn_request->request_id;
	nn_response->metadata_packet_len+=sizeof(nn_response->request_id);
	nn_response->namenode_response_id = DMS_serverID_Generator();
	nn_response->metadata_packet_len+=sizeof(nn_response->namenode_response_id);

	nn_response->num_of_lr = num_of_lrs;
	nn_response->metadata_packet_len+=sizeof(nn_response->num_of_lr);

	nn_response->fc_curr_maxreqs = 0;
	nn_response->fc_curr_rate = 0;
	nn_response->fc_rateAge = 0;
	nn_response->fc_slowstart_maxreqs = 0;
	nn_response->fc_slowstart_rate = 0;
	nn_response->metadata_packet_len += (sizeof(unsigned int)*5);

	nn_response->nn_metadata = (struct nn_metadata *)malloc(sizeof(struct nn_metadata)*num_of_lrs);

	for(i=0;i<num_of_lrs;i++) {
		ptr_nn_md = &(nn_response->nn_metadata[i]);
		ptr_nn_md->num_of_hbids = lr[i]->num_of_hbids;
		nn_response->metadata_packet_len+=sizeof(ptr_nn_md->num_of_hbids);

		ptr_nn_md->hbids = (unsigned long long *)malloc(sizeof(unsigned long long)*ptr_nn_md->num_of_hbids);
		memcpy(ptr_nn_md->hbids, lr[i]->HBIDs, sizeof(unsigned long long)*ptr_nn_md->num_of_hbids);
		nn_response->metadata_packet_len+=(sizeof(unsigned long long)*ptr_nn_md->num_of_hbids);

		ptr_nn_md->num_of_datanode_location = lr[i]->num_dn_locations;
		nn_response->metadata_packet_len+=sizeof(ptr_nn_md->num_of_datanode_location);

		ptr_nn_md->dn_location = (struct nn_metadata_dn_location *)malloc(sizeof(struct nn_metadata_dn_location)*ptr_nn_md->num_of_datanode_location);

		for(j=0;j<lr[i]->num_dn_locations;j++) {
			ptr_dn_loc = &(ptr_nn_md->dn_location[j]);
			ptr_dn_loc->len_hostname = CCMA_HOSTNAME_LEN;
			nn_response->metadata_packet_len+=sizeof(unsigned short);

			ptr_dn_loc->hostname = (char*)malloc(sizeof(char)*CCMA_HOSTNAME_LEN);
			memcpy(ptr_dn_loc->hostname, lr[i]->dnLocations[j].hostname, ptr_dn_loc->len_hostname);
			nn_response->metadata_packet_len+=ptr_dn_loc->len_hostname;

			ptr_dn_loc->port = lr[i]->dnLocations[j].port;
			nn_response->metadata_packet_len+=sizeof(ptr_dn_loc->port);
			syslog(LOG_ERR, "T_LR_TO_R: hostname:%s port %d\n", ptr_dn_loc->hostname, ptr_dn_loc->port);

			ptr_dn_loc->num_of_location_triplets = lr[i]->dnLocations[j].num_of_phy_loc;
			nn_response->metadata_packet_len+=sizeof(ptr_dn_loc->num_of_location_triplets);

			ptr_dn_loc->location_triplets = (unsigned long long *)malloc(sizeof(unsigned long long)*ptr_dn_loc->num_of_location_triplets);
			for(k=0;k<ptr_dn_loc->num_of_location_triplets;k++) {
				ptr_dn_loc->location_triplets[k] = compose_triple(lr[i]->dnLocations[j].physical_locs[k].RBID,
						lr[i]->dnLocations[j].physical_locs[k].length, lr[i]->dnLocations[j].physical_locs[k].offsetInRB);
				nn_response->metadata_packet_len+=sizeof(unsigned long long);
				syslog(LOG_ERR, "Ready to transform lr[%d]->dnloc[%d].triplets[k] %d %d %d\n", i, j, k,
						lr[i]->dnLocations[j].physical_locs[k].RBID,
						lr[i]->dnLocations[j].physical_locs[k].length, lr[i]->dnLocations[j].physical_locs[k].offsetInRB);
			}
		}

		ptr_nn_md->location_state = lr[i]->state;
		nn_response->metadata_packet_len+=sizeof(unsigned short);
	}

	return ret;
}

struct nn_response_header * Process_NameNode_Metadata_Operation_Request(struct nn_request_metadata * nn_request) {
	struct nn_response_metadata * nn_response = NULL;
	struct LocatedRequest ** lr = NULL;
	int num_of_lrs = 0, i, j, ret;

	nn_response = (struct nn_response_metadata *)malloc(sizeof(struct nn_response_metadata));

	nn_response->header.opcode = nn_request->header.opcode;

	switch(nn_request->header.opcode) {
		case NN_REQUEST_METADATA_READ:
			lr = QueryMetadata_For_Read(nn_request->volume_id, nn_request->start_LB, nn_request->LB_cnt, &num_of_lrs);
			break;
		case NN_REQUEST_METADATA_WRITE:
			lr = AllocateMetadata_For_Write(nn_request->volume_id, nn_request->start_LB, nn_request->LB_cnt, &num_of_lrs);
			break;
		case NN_REQUEST_METADATA_OVERWRITE:
			lr = QueryMetadata_For_Write(nn_request->volume_id, nn_request->start_LB, nn_request->LB_cnt, &num_of_lrs);
			break;
		case NN_REQUEST_METADATA_WRITE_NO_GET_OLD:
			lr = AllocateMetadata_For_Write_Without_Old(nn_request->volume_id, nn_request->start_LB, nn_request->LB_cnt, &num_of_lrs);
			break;
		default:
			break;
	}

	if(lr == NULL) {
		free(nn_response);
		return NULL;
	}

	ret = Transform_LR_to_Metadata_Response((struct nn_request_metadata *)nn_request, nn_response, lr, num_of_lrs);

	for(i=0;i<num_of_lrs;i++) {
		for(j=0;j<lr[i]->num_dn_locations;j++) {
			free(lr[i]->dnLocations[j].physical_locs);
		}
		free(lr[i]->dnLocations);
		lr[i] = NULL;
	}
	free(lr);

	return (struct nn_response_header *)nn_response;
}

int Process_NameNode_Metadata_Report_Request(struct nn_request_metadata_report * nn_request) {
	return 0;
}

int Process_NameNode_Request(int sock_fd) {
	struct nn_request_header * nn_request = NULL;
	struct nn_response_header * nn_response = NULL;
	int ret = 0;

	nn_request = Receive_NameNode_Request(sock_fd);

	if(nn_request == NULL)
		return -1;

	syslog(LOG_ERR, "Process_NameNode_Request cmd: %d\n", nn_request->opcode);
	switch(nn_request->opcode) {
		case NN_REQUEST_METADATA_READ:
		case NN_REQUEST_METADATA_WRITE:
		case NN_REQUEST_METADATA_OVERWRITE:
		case NN_REQUEST_METADATA_WRITE_NO_GET_OLD:
			nn_response = Process_NameNode_Metadata_Operation_Request((struct nn_request_metadata *)nn_request);
			break;
		case NN_REQUEST_QUERY_VOLUME_INFO:
			nn_response = Process_NameNode_Query_Volume_Request((struct nn_request_volume_info *)nn_request);
			break;
		case NN_REQUEST_CREATE_VOLUME:
			nn_response = Process_NameNode_Create_Volume_Request((struct nn_request_volume_create *)nn_request);
			break;
		case NN_REQUEST_METADATA_REPORT:
			ret = Process_NameNode_Metadata_Report_Request((struct nn_request_metadata_report *)nn_request);
			break;
		default:
			syslog(LOG_ERR, "unknown request %d\n", nn_request->opcode);
			return -1;
	}

	if(nn_response != NULL) {
		ret = Send_NameName_Response(sock_fd, nn_response);
	} else {
		if(nn_request->opcode != NN_REQUEST_METADATA_REPORT)
			ret = -1;
	}
	switch(nn_request->opcode) {
		case NN_REQUEST_METADATA_READ:
		case NN_REQUEST_METADATA_WRITE:
		case NN_REQUEST_METADATA_OVERWRITE:
		case NN_REQUEST_METADATA_WRITE_NO_GET_OLD:
			free(nn_request);
			if(nn_response != NULL)
				free_nn_metadata_response((struct nn_response_metadata *)nn_response);
			break;
		case NN_REQUEST_QUERY_VOLUME_INFO:
		case NN_REQUEST_METADATA_REPORT:
			free(nn_request);
			if(nn_response != NULL) //NOTE: Might have issue at free nn_response when REPORT METADATA
				free(nn_response);
			break;
		default:
			if(nn_response != NULL)
				free(nn_response);
			break;
	}
	return ret;
}

void nn_thread_function(void * arg) {
	int sockfd = *((int*)arg);
	int ret = 0;

	while(ret >= 0) {
		ret = Process_NameNode_Request(sockfd); //Should create a thread to handle
	}

	syslog(LOG_ERR, "nn_thread_function exit loop\n");
}

int Run_Faked_NameNode() {
	int clientfd;
	struct sockaddr_in client_addr;
	int addrlen = 0;
	pthread_attr_t attr;
	pthread_t id;
	int ret;

	syslog(LOG_ERR, "Ready to create socket server with port=%d\n", NameNode_Server_Port);
	server_socket_fd = create_socket_server(NameNode_Server_Port);

	if(server_socket_fd < 0)
		return -1;

	while(1) {
		addrlen = sizeof(client_addr);
		memset(&client_addr, 0, sizeof(struct sockaddr_in));
		clientfd = socket_accept(server_socket_fd, (struct sockaddr*)&client_addr, &addrlen);
		syslog(LOG_ERR, "Got a client connection, ready to process this\n");

		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

		ret = pthread_create(&id, &attr, (void*)nn_thread_function, (void *)&clientfd);
		if(ret == -1) {
			syslog(LOG_ERR, "Socket server create pthread error! exist\n");
			exit(1);
		}
	}
}

int main(int argc, char ** argv) {
	syslog(LOG_ERR, "start Faked NameNode services at port %d\n", NameNode_Server_Port);
	register_signal_handler();
	Initialize_Faked_NameNode();

	if(Run_Faked_NameNode() < 0)
		return 0;

	return 0;
}
