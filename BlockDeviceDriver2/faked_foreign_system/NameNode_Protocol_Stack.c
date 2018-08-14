#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <syslog.h>
#include "NameNode_Protocol_Stack.h"
#include "socket_util.h"
#include "utility.h"

struct nn_request_header * Receive_NameNode_Request_Header(int sock_fd) {
	struct nn_request_header * header = NULL;
	int ret = 0;

	header = (struct nn_request_header *)malloc(sizeof(struct nn_request_header));

	ret = socket_recvfrom(sock_fd, (char *)header, sizeof(struct nn_request_header));

	if(ret != sizeof(struct nn_request_header)) {
		syslog(LOG_ERR, "Receive_NameNode_Request_Header receiver error\n");
		free(header);
		return NULL;
	}

	header->magic_number = ntohll(header->magic_number);
	header->opcode = ntohs(header->opcode);

	return header;
}

struct nn_request_metadata * Receive_NN_Metadata_Request(int sock_fd) {
	struct nn_request_metadata * metadata_req = NULL;
	int ret = 0;

	metadata_req = (struct nn_request_metadata *)malloc(sizeof(struct nn_request_metadata));
	memset( metadata_req, 0x0, sizeof(struct nn_request_metadata));

	ret = socket_recvfrom(sock_fd, (char *)(&(metadata_req->start_LB)), NN_REQUEST_METADATA_REQUEST_BODY_LEN);

	if(ret != NN_REQUEST_METADATA_REQUEST_BODY_LEN) {
		syslog(LOG_ERR, "Receive_NN_Metadata_Request: num of bytes received %d\n", ret);
		free(metadata_req);
		return NULL;
	}

	metadata_req->start_LB = ntohll(metadata_req->start_LB);
	metadata_req->request_id = ntohll(metadata_req->request_id);
	metadata_req->volume_id = ntohll(metadata_req->volume_id);
	metadata_req->LB_cnt = ntohl(metadata_req->LB_cnt);
	metadata_req->magic_number = ntohll(metadata_req->magic_number);

	syslog(LOG_ERR, "REQ: req_id=%llu volid=%llu startLB=%llu LB_cnt=%d\n", metadata_req->request_id,
			metadata_req->volume_id, metadata_req->start_LB, metadata_req->LB_cnt);
	return metadata_req;
}

struct nn_request_volume_info * Receive_NN_Volume_Info_Request(int sock_fd) {
	struct nn_request_volume_info * vol_info_req = NULL;
	int ret = 0;

	vol_info_req = (struct nn_request_volume_info *)malloc(sizeof(struct nn_request_volume_info));

	ret = socket_recvfrom(sock_fd, (char *)(&(vol_info_req->volume_id)), NN_REQUEST_VOLINFO_REQUEST_BODY_LEN);

	if(ret != NN_REQUEST_VOLINFO_REQUEST_BODY_LEN) {
		free(vol_info_req);
		return NULL;
	}

	vol_info_req->volume_id = ntohl(vol_info_req->volume_id);

	return vol_info_req;
}

struct nn_request_volume_create * Receive_NN_Volume_Create_Request(int sock_fd) {
	struct nn_request_volume_create * vol_create_req = NULL;
	int ret = 0;
	vol_create_req = (struct nn_request_volume_create *)malloc(sizeof(struct nn_request_volume_create));

	ret = socket_recvfrom(sock_fd, (char *)(&(vol_create_req->volume_capacity)), NN_REQUEST_VOLCREATE_REQUEST_BODY_LEN);

	if(ret != NN_REQUEST_VOLCREATE_REQUEST_BODY_LEN) {
		free(vol_create_req);
		return NULL;
	}

	vol_create_req->volume_capacity = ntohll(vol_create_req->volume_capacity);

	return vol_create_req;
}

struct nn_request_DN_INFO * Receive_NN_Request_DN_Info(int sock_fd, int num_of_failed_DNs) {
	struct nn_request_DN_INFO * dn_infos = NULL;
	int i, j, ret, num_of_failed_DNs_received = 0, fail = 0;

	dn_infos = (struct nn_request_DN_INFO *)malloc(sizeof(struct nn_request_DN_INFO)*num_of_failed_DNs);

	for(i=0;i<num_of_failed_DNs;i++) {
		dn_infos[i].triplets = NULL;
	}

	syslog(LOG_ERR, "Receive_NN_Request_DN_Info: num of failed dn: %d\n", num_of_failed_DNs);
	for(i=0;i<num_of_failed_DNs;i++) {
		ret = socket_recvfrom(sock_fd, (char*)(&(dn_infos[i])), sizeof(struct nn_request_DN_INFO) - sizeof(unsigned long long *));
		if(ret != (sizeof(struct nn_request_DN_INFO) - sizeof(unsigned long long *))) {
			fail = 1;
			syslog(LOG_ERR, "1 Error Receive expected %d received %d\n", sizeof(struct nn_request_DN_INFO) - sizeof(unsigned long long *),
					ret);
			break;
		}

		dn_infos[i].packet_size = ntohl(dn_infos[i].packet_size);
		dn_infos[i].port = ntohl(dn_infos[i].port);
		dn_infos[i].num_of_triplets = ntohl(dn_infos[i].num_of_triplets);
		syslog(LOG_ERR, "dn_infos[%d] pkt_size=%d hn=%s port=%d num_of_triplets=%d\n", i, dn_infos[i].packet_size, dn_infos[i].hostname,
				dn_infos[i].port, dn_infos[i].num_of_triplets);
		dn_infos[i].triplets = malloc(sizeof(unsigned long long)*dn_infos[i].num_of_triplets);
		ret = socket_recvfrom(sock_fd, (char*)(&(dn_infos[i].triplets[0])), sizeof(unsigned long long)*dn_infos[i].num_of_triplets);
		if(ret != sizeof(unsigned long long)*dn_infos[i].num_of_triplets) {
			fail = 1;
			syslog(LOG_ERR, "2 Error Receive expected %d received %d\n", sizeof(unsigned long long)*dn_infos[i].num_of_triplets,
				ret);
			break;
		}
		for(j=0;j<dn_infos[i].num_of_triplets;j++) {
			dn_infos[i].triplets[j] = ntohll(dn_infos[i].triplets[j]);
			syslog(LOG_ERR, "dn_infos[%d].triplets[%d] %llu\n", i, j, dn_infos[i].triplets[j]);
		}
		num_of_failed_DNs_received++;
	}

	if(fail) {
		//free memory when fail
		for(i=0;i<num_of_failed_DNs_received;i++) {
			if(dn_infos[i].triplets != NULL) {
				free(dn_infos[i].triplets);
			}
		}
		free(dn_infos);
		dn_infos = NULL;
	}

	return dn_infos;
}

struct nn_request_metadata_write_error_report * Receive_NN_M_W_ERROR_Report_Request(int sock_fd) {
	struct nn_request_metadata_write_error_report * report = NULL;
	struct nn_request_DN_INFO * dn_infos = NULL;
	int ret = 0, i, j;

	report = (struct nn_request_metadata_write_error_report *)malloc(sizeof(struct nn_request_metadata_write_error_report));

	ret = socket_recvfrom(sock_fd, (char*)(&(report->server_id)), sizeof(unsigned long long));
	ret += socket_recvfrom(sock_fd, (char*)(&(report->num_of_failed_DNs)), sizeof(unsigned int));

	if(ret != sizeof(unsigned long long) + sizeof(unsigned int)) {
		free(report);
		return NULL;
	}

	report->server_id = ntohll(report->server_id);
	report->num_of_failed_DNs = ntohl(report->num_of_failed_DNs);
	syslog(LOG_ERR, "NN_M_W_REPORT: server id = %llu\n", report->server_id);
	syslog(LOG_ERR, "NN_M_W_REPORT: num_of_failed_DNs = %d\n", report->num_of_failed_DNs);

	if(report->num_of_failed_DNs > 0) {
		dn_infos = Receive_NN_Request_DN_Info(sock_fd, report->num_of_failed_DNs);
		if(dn_infos == NULL) {
			free(report);
			return NULL;
		}
	} else
		dn_infos = NULL;

	report->failed_DNs = dn_infos;
	ret = socket_recvfrom(sock_fd, (char*)(&(report->magic_number)), sizeof(unsigned long long));

	if(ret != sizeof(unsigned long long)) {
		//free memory when fail
		if(dn_infos != NULL) {
			for(i=0;i<report->num_of_failed_DNs;i++) {
				for(j=0;j<dn_infos[i].num_of_triplets;j++) {
					if(dn_infos[i].triplets != NULL) {
						free(dn_infos[i].triplets);
					}
				}
			}
			free(dn_infos);
			dn_infos = NULL;
		}

		free(report);
		return NULL;
	}

	report->magic_number = ntohll(report->magic_number);

	return report;
}

struct nn_request_metadata_overwrite_error_report * Receive_NN_M_OVW_ERROR_Report_Request(int sock_fd) {
	struct nn_request_metadata_overwrite_error_report * report = NULL;
	int ret, i, j;
	struct nn_request_DN_INFO * dn_infos = NULL;

	report = malloc(sizeof(struct nn_request_metadata_overwrite_error_report));

	ret = socket_recvfrom(sock_fd, (char*)(&(report->volume_id)), sizeof(unsigned long long));
	ret += socket_recvfrom(sock_fd, (char*)(&(report->num_of_LBs)), sizeof(unsigned int));
	ret += socket_recvfrom(sock_fd, (char*)(&(report->num_of_success_DNs)), sizeof(unsigned int));

	if(ret != sizeof(unsigned long long) + sizeof(unsigned int)*2) {
		free(report);
		return NULL;
	}

	report->volume_id = ntohll(report->volume_id);
	report->num_of_LBs = ntohl(report->num_of_LBs);
	report->num_of_success_DNs = ntohl(report->num_of_success_DNs);

	syslog(LOG_ERR, "Receive_NN_M_OVW_ERROR_Report_Request: volid=%llu num_of_LB=%d num_of_DNs=%d\n",
				report->volume_id, report->num_of_LBs, report->num_of_success_DNs);

	report->LBIDs = malloc(sizeof(unsigned long long)*report->num_of_LBs);
	ret = socket_recvfrom(sock_fd, (char*)(report->LBIDs), sizeof(unsigned long long)*report->num_of_LBs);

	if(ret != sizeof(unsigned long long)*report->num_of_LBs) {
		free(report->LBIDs);
		free(report);
		return NULL;
	}

	for(i=0;i<report->num_of_LBs;i++) {
		report->LBIDs[i] = ntohll(report->LBIDs[i]);
		syslog(LOG_ERR, "Receive_NN_M_OVW_ERROR_Report_Request: LBID[%d]=%llu\n",
						i, report->LBIDs[i]);
	}

	dn_infos = Receive_NN_Request_DN_Info(sock_fd, report->num_of_success_DNs);
	if(dn_infos == NULL) {
		free(report->LBIDs);
		free(report);

		return NULL;
	}

	report->success_DNs = dn_infos;

	ret = socket_recvfrom(sock_fd, (char*)(&(report->magic_number)), sizeof(unsigned long long));

	if(ret != sizeof(unsigned long long)) {
		//free memory when fail
		for(i=0;i<report->num_of_success_DNs;i++) {
			for(j=0;j<dn_infos[i].num_of_triplets;j++) {
				if(dn_infos[i].triplets != NULL) {
					free(dn_infos[i].triplets);
				}
			}
		}
		free(dn_infos);
		dn_infos = NULL;
		free(report->LBIDs);
		free(report);
		return NULL;
	}

	report->magic_number = ntohll(report->magic_number);

	return report;
}

struct nn_request_metadata_read_error_report * Receive_NN_M_R_ERROR_Report_Request(int sock_fd) {
	struct nn_request_metadata_read_error_report * report = NULL;
	int ret;

	report = malloc(sizeof(struct nn_request_metadata_read_error_report));

	ret = socket_recvfrom(sock_fd, (char*)report, sizeof(struct nn_request_metadata_read_error_report));

	if(ret != sizeof(struct nn_request_metadata_read_error_report)) {
		free(report);
		return NULL;
	} else {
		report->port = ntohl(report->port);
		report->volume_id = ntohll(report->volume_id);
		report->lbid = ntohll(report->lbid);
		report->magic_number = ntohll(report->magic_number);
		syslog(LOG_ERR, "Receive_NN_M_R_ERROR_Report_Request: hn=%s port=%d vol_id=%llu, startLB=%llu\n",
				report->hostname, report->port, report->volume_id, report->lbid);
	}

	return report;
}

struct nn_request_metadata_report * Receive_NN_Metadata_Report_Request(int sock_fd) {
	struct nn_request_metadata_report * metadata_report_req = NULL;
	int ret = 0;

	syslog(LOG_ERR, "Receive_NN_Metadata_Report_Request start\n");
	metadata_report_req = (struct nn_request_metadata_report *)malloc(sizeof(struct nn_request_metadata_report));

	ret = socket_recvfrom(sock_fd, (char *)(&(metadata_report_req->sub_opcode)), sizeof(unsigned short));

	if(ret != sizeof(unsigned short)) {
		free(metadata_report_req);
		return NULL;
	}

	metadata_report_req->sub_opcode = ntohs(metadata_report_req->sub_opcode);

	syslog(LOG_ERR, "Receive_NN_Metadata_Report_Request: subopcode = %d\n", metadata_report_req->sub_opcode);
	switch(metadata_report_req->sub_opcode) {
		case NN_REQUEST_METADATA_REPORT_SUBOPCODE_WRITE_FAIL:
			syslog(LOG_ERR, "Ready to receive body of WRITE FAIL\n");
			metadata_report_req->w_error_report = Receive_NN_M_W_ERROR_Report_Request(sock_fd);
			break;
		case NN_REQUEST_METADATA_REPORT_SUBOPCODE_READ_FAIL:
			syslog(LOG_ERR, "Ready to receive body of READ FAIL\n");
			metadata_report_req->read_error_report = Receive_NN_M_R_ERROR_Report_Request(sock_fd);
			break;
		case NN_REQUEST_METADATA_REPORT_SUBOPCODE_OVW_FAIL:
			syslog(LOG_ERR, "Ready to receive body of OVW FAIL\n");
			metadata_report_req->ovw_error_report = Receive_NN_M_OVW_ERROR_Report_Request(sock_fd);
			break;
		default:
			free(metadata_report_req);
			syslog(LOG_ERR, "Unknown subopcode of Metadata Report Request\n");
			return NULL;
	}
	return metadata_report_req;
}

struct nn_request_header * Receive_NameNode_Request(int sock_fd) {
	struct nn_request_header * req = NULL;
	struct nn_request_header * full_req = NULL;

	syslog(LOG_ERR, "Receive_NameNode_Request start\n");
	req = Receive_NameNode_Request_Header(sock_fd);

	if(req == NULL) {
		syslog(LOG_ERR, "Cannot Receive request from clientnode\n");
		return req;
	}

	syslog(LOG_ERR, "Receive_NameNode_Request cmd: %d\n", req->opcode);
	switch(req->opcode) {
		case NN_REQUEST_METADATA_READ:
		case NN_REQUEST_METADATA_WRITE:
		case NN_REQUEST_METADATA_OVERWRITE:
		case NN_REQUEST_METADATA_WRITE_NO_GET_OLD:
			full_req = (struct nn_request_header *)Receive_NN_Metadata_Request(sock_fd);
			break;
		case NN_REQUEST_QUERY_VOLUME_INFO:
			full_req = (struct nn_request_header *)Receive_NN_Volume_Info_Request(sock_fd);
			break;
		case NN_REQUEST_CREATE_VOLUME:
			full_req = (struct nn_request_header *)Receive_NN_Volume_Create_Request(sock_fd);
			break;
		case NN_REQUEST_CLONE_VOLUME:
			//TODO: Not Support Now
			break;
		case NN_REQUEST_METADATA_REPORT:
			full_req = (struct nn_request_header *)Receive_NN_Metadata_Report_Request(sock_fd);
			break;
		default:
			syslog(LOG_ERR, "Unknow command\n");
			break;
	}

	if(full_req != NULL) {
		full_req->magic_number = req->magic_number;
		full_req->opcode = req->opcode;
	}

	free(req);
	return full_req;
}

int Send_NameNode_Query_Volume_Response(int sock_fd, struct nn_response_volume_info * nn_response) {
	int ret = 0;
	unsigned long long capacity_in_bytes = 0;

	capacity_in_bytes = nn_response->volume_capacity*1024L*1024L;
	capacity_in_bytes = htonll(capacity_in_bytes);
	nn_response->replica_factor = htonl(nn_response->replica_factor);
	ret = socket_sendto(sock_fd, (char*)(&capacity_in_bytes), sizeof(unsigned long long));
	ret = socket_sendto(sock_fd, (char*)(&nn_response->replica_factor), sizeof(unsigned int));

	return ret;
}

int Send_NameNode_Create_Volume_Response(int sock_fd, struct nn_response_volume_create * nn_response) {
	int ret = 0;

	nn_response->volume_id = htonll(nn_response->volume_id);
	ret = socket_sendto(sock_fd, (char*)(&nn_response->volume_id), sizeof(unsigned long long));

	return ret;
}

int Send_NameNode_Metadata_Operation_Response(int sock_fd, struct nn_response_metadata * nn_response) {
	int ret = 0, i, j, k, num_of_lrs = 0, num_of_hbids = 0, num_of_dn_loc = 0, num_of_triplets = 0;
	int packet_len = 0, len_of_hn;
	struct nn_metadata * ptr_nn_md = NULL;
	struct nn_metadata_dn_location * ptr_dn_loc = NULL;
	int rbid, len, offset;

	num_of_lrs = nn_response->num_of_lr;
	packet_len = nn_response->metadata_packet_len;

	syslog(LOG_ERR, "Send_NameNode_Metadata_Operation_Response start: with total packet len %d\n", packet_len);
	syslog(LOG_ERR, "RES: MagicNum: \t\t%llu\n", nn_response->magic_number);
	nn_response->magic_number = htonll(nn_response->magic_number);
	nn_response->response_type = htons(nn_response->response_type);
	nn_response->metadata_packet_len = htons(nn_response->metadata_packet_len);
	syslog(LOG_ERR, "RES: CN Req ID: \t\t%llu\n", nn_response->request_id);
	nn_response->request_id = htonll(nn_response->request_id);
	syslog(LOG_ERR, "RES: NN Server ID: \t\t%llu\n", nn_response->namenode_response_id);
	syslog(LOG_ERR, "RES: Num LRs: \t\t%d\n", num_of_lrs);
	nn_response->namenode_response_id = htonll(nn_response->namenode_response_id);
	num_of_lrs = htons(num_of_lrs);

	ret = socket_sendto(sock_fd, (char*)&(nn_response->magic_number), sizeof(unsigned long long));
	ret = socket_sendto(sock_fd, (char*)&(nn_response->response_type), sizeof(unsigned short));
	ret += socket_sendto(sock_fd, (char*)&(nn_response->metadata_packet_len), sizeof(unsigned short));
	ret += socket_sendto(sock_fd, (char*)&(nn_response->request_id), sizeof(unsigned long long));
	ret += socket_sendto(sock_fd, (char*)&(nn_response->namenode_response_id), sizeof(unsigned long long));
	ret += socket_sendto(sock_fd, (char*)&(num_of_lrs), sizeof(unsigned short));

	for(i=0;i<nn_response->num_of_lr;i++) {
		ptr_nn_md = &(nn_response->nn_metadata[i]);

		num_of_hbids = ptr_nn_md->num_of_hbids;

		syslog(LOG_ERR, "RES: LR[%d].num_of_hbids: \t\t%d\n", i, num_of_hbids);

		num_of_hbids = htons(ptr_nn_md->num_of_hbids);
		ret += socket_sendto(sock_fd, (char*)&(num_of_hbids), sizeof(unsigned short));

		for(j=0;j<ptr_nn_md->num_of_hbids;j++) {
			syslog(LOG_ERR, "RES: LR[%d].hbid[%d]: \t\t%llu\n", i, j, ptr_nn_md->hbids[j]);
			ptr_nn_md->hbids[j] = htonll(ptr_nn_md->hbids[j]);
		}
		ret += socket_sendto(sock_fd, (char*)ptr_nn_md->hbids, sizeof(unsigned long long)*ptr_nn_md->num_of_hbids);

		num_of_dn_loc = ptr_nn_md->num_of_datanode_location;
		syslog(LOG_ERR, "RES: LR[%d].num_of_dn_loc: \t\t%d\n", i, num_of_dn_loc);
		num_of_dn_loc = htons(num_of_dn_loc);
		ret += socket_sendto(sock_fd, (char*)&(num_of_dn_loc), sizeof(unsigned short));

		for(j=0;j<ptr_nn_md->num_of_datanode_location;j++) {
			ptr_dn_loc = &(ptr_nn_md->dn_location[j]);
			syslog(LOG_ERR, "RES: LR[%d].dnloc[%d].len_hn: \t\t%d\n", i, j, ptr_dn_loc->len_hostname);
			len_of_hn = htons(ptr_dn_loc->len_hostname);
			ret += socket_sendto(sock_fd, (char*)&len_of_hn, sizeof(unsigned short));

			syslog(LOG_ERR, "RES: LR[%d].dnloc[%d].hostname: \t\t%s\n", i, j, ptr_dn_loc->hostname);
			ret += socket_sendto(sock_fd, ptr_dn_loc->hostname, sizeof(char)*ptr_dn_loc->len_hostname);

			syslog(LOG_ERR, "RES: LR[%d].dnloc[%d].port: \t\t%d\n", i, j, ptr_dn_loc->port);
			ptr_dn_loc->port = htonl(ptr_dn_loc->port);
			ret += socket_sendto(sock_fd, (char*)&(ptr_dn_loc->port), sizeof(unsigned int));

			syslog(LOG_ERR, "RES: LR[%d].dnloc[%d].num_of_location_triplets: \t\t%d\n", i, j, ptr_dn_loc->num_of_location_triplets);
			num_of_triplets = htons(ptr_dn_loc->num_of_location_triplets);
			ret += socket_sendto(sock_fd, (char*)&num_of_triplets, sizeof(unsigned short));

			for(k=0;k<ptr_dn_loc->num_of_location_triplets;k++) {
				decompose_triple(ptr_dn_loc->location_triplets[k], &rbid, &len, &offset);
				syslog(LOG_ERR, "RES: LR[%d].dnloc[%d].triplets[%d]: \t\t%llu (%d, %d, %d)\n",
						i, j, k, ptr_dn_loc->location_triplets[k], rbid, len, offset);
				ptr_dn_loc->location_triplets[k] = htonll(ptr_dn_loc->location_triplets[k]);
			}

			ret += socket_sendto(sock_fd, (char*)ptr_dn_loc->location_triplets, sizeof(unsigned long long)*ptr_dn_loc->num_of_location_triplets);
		}

		syslog(LOG_ERR, "RES: LR[%d].location_state: \t\t%d\n", i, ptr_nn_md->location_state);
		ptr_nn_md->location_state = htons(ptr_nn_md->location_state);
		ret += socket_sendto(sock_fd, (char*)&(ptr_nn_md->location_state), sizeof(unsigned short));
	}

	nn_response->fc_slowstart_rate = htonl(nn_response->fc_slowstart_rate);
	ret += socket_sendto(sock_fd, (char*)&(nn_response->fc_slowstart_rate), sizeof(unsigned int));
	nn_response->fc_slowstart_maxreqs = htonl(nn_response->fc_slowstart_maxreqs);
	ret += socket_sendto(sock_fd, (char*)&(nn_response->fc_slowstart_maxreqs), sizeof(unsigned int));
	nn_response->fc_curr_rate = htonl(nn_response->fc_curr_rate);
	ret += socket_sendto(sock_fd, (char*)&(nn_response->fc_curr_rate), sizeof(unsigned int));
	nn_response->fc_curr_maxreqs = htonl(nn_response->fc_curr_maxreqs);
	ret += socket_sendto(sock_fd, (char*)&(nn_response->fc_curr_maxreqs), sizeof(unsigned int));
	nn_response->fc_rateAge = htonl(nn_response->fc_rateAge);
	ret += socket_sendto(sock_fd, (char*)&(nn_response->fc_rateAge), sizeof(unsigned int));

	if(ret != (packet_len + sizeof(unsigned long long) + sizeof(unsigned short) + sizeof(unsigned short))) {
		syslog(LOG_ERR, "Send_NameNode_Metadata_Operation_Response Send out %d Expected %d and packet_len = %d\n",
				ret, (int)(packet_len + sizeof(unsigned long long) + sizeof(unsigned short)), packet_len);
		ret = -1;
	}

	syslog(LOG_ERR, "Send_NameNode_Metadata_Operation_Response Middle DONE %d\n", ret);
	return ret;
}

int Send_NameName_Response(int sock_fd, struct nn_response_header * nn_response) {
	int ret = 0;

	switch(nn_response->opcode) {
		case NN_REQUEST_METADATA_READ:
		case NN_REQUEST_METADATA_WRITE:
		case NN_REQUEST_METADATA_OVERWRITE:
		case NN_REQUEST_METADATA_WRITE_NO_GET_OLD:
			ret = Send_NameNode_Metadata_Operation_Response(sock_fd, (struct nn_response_metadata *)nn_response);
			break;
		case NN_REQUEST_QUERY_VOLUME_INFO:
			ret = Send_NameNode_Query_Volume_Response(sock_fd, (struct nn_response_volume_info *)nn_response);
			break;
		case NN_REQUEST_CREATE_VOLUME:
			ret = Send_NameNode_Create_Volume_Response(sock_fd, (struct nn_response_volume_create *)nn_response);
			break;
		case NN_REQUEST_METADATA_REPORT:
			//TODO: No need report metadata
			break;
		default:
			ret = -1;
			break;
	}

	return ret;
}

int free_nn_metadata_response(struct nn_response_metadata * nn_response) {
	int i, j, ret = 0;

	for(i=0;i<nn_response->num_of_lr;i++) {
		for(j=0;j<nn_response->nn_metadata[i].num_of_datanode_location;j++) {
			free(nn_response->nn_metadata[i].dn_location[j].hostname);
			free(nn_response->nn_metadata[i].dn_location[j].location_triplets);
		}
		free(nn_response->nn_metadata[i].dn_location);
		free(nn_response->nn_metadata[i].hbids);
	}

	free(nn_response->nn_metadata);
	free(nn_response);

	return ret;
}

