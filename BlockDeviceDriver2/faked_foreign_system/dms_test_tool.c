/*
 * dms_test_tool.c
 *
 *  Created on: 2012/8/14
 *      Author: 990158
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <fcntl.h>
#include "socket_util.h"
#include "utility.h"
#define htonll(x) \
((((x) & 0xff00000000000000LL) >> 56) | \
(((x) & 0x00ff000000000000LL) >> 40) | \
(((x) & 0x0000ff0000000000LL) >> 24) | \
(((x) & 0x000000ff00000000LL) >> 8) | \
(((x) & 0x00000000ff000000LL) << 8) | \
(((x) & 0x0000000000ff0000LL) << 24) | \
(((x) & 0x000000000000ff00LL) << 40) | \
(((x) & 0x00000000000000ffLL) << 56))

#define ntohll(x)	htonll(x)

unsigned long long magicNum = htonll(0xa5a5a5a5a5a5a5a5ll);

#define	NUM_OF_REQUEST_SEND	1
#define TIME_INTERVAL_OF_REQUESTS	5

void process_create_volume(char * s_vol_cap) {
	unsigned long long vol_cap = 0, volid, n_vol_cap;
	socket_t sockfd;
	short h_opcode = htons(1001);
	int numbytes;
	char buf[80];
	int i;

	vol_cap = atoll(s_vol_cap);

	sockfd = socket_connect_to("127.0.0.1", 1234);

	if(sockfd < 0) {
		printf("Cannot create socket connection");
		return;
	}

	for(i=0;i<NUM_OF_REQUEST_SEND;i++) {
		n_vol_cap = htonll(vol_cap);

		socket_sendto(sockfd, (char*)(&magicNum), 8);
		socket_sendto(sockfd, (char*)(&h_opcode), 2);
		socket_sendto(sockfd, (char*)(&n_vol_cap), 8);

		numbytes = socket_recvfrom(sockfd, buf, 8);

		unsigned long long * volid_ptr = (unsigned long long *)buf;
		volid = ntohll(*volid_ptr);

		printf("Volume id is %llu\n", volid);
		//sleep(TIME_INTERVAL_OF_REQUESTS);
	}

	socket_close(sockfd);
}

void process_query_volume(char * s_vol_id) {
	unsigned int vol_id = 0, n_vol_id;
	unsigned long long vol_cap = 0;
	socket_t sockfd, replica_factor;
	short h_opcode = htons(101);
	char buf[80];
	int i;

	vol_id = atol(s_vol_id);

	sockfd = socket_connect_to("127.0.0.1", 1234);

	if(sockfd < 0) {
		printf("Cannot create socket connection");
		return;
	}

	n_vol_id = htonl(vol_id);

	for(i=0;i<NUM_OF_REQUEST_SEND;i++) {

		socket_sendto(sockfd, (char*)(&magicNum), 8);
		socket_sendto(sockfd, (char*)(&h_opcode), 2);
		socket_sendto(sockfd, (char*)(&n_vol_id), 4);

		socket_recvfrom(sockfd, buf, sizeof(long)+sizeof(int));
		memcpy(&vol_cap, buf, sizeof(long long));
		memcpy(&replica_factor, buf + sizeof(long long), sizeof(int));
		vol_cap = ntohll(vol_cap);
		replica_factor = ntohl(replica_factor);

		printf("Volume cap is %llu, replica_factor is %d for volid %d\n", vol_cap, replica_factor, vol_id);
	}

	socket_close(sockfd);
}

void parse_namenode_metadata_response(char *buffer) {
	char * buf_ptr = buffer;
	unsigned long long tmp_ll;
	unsigned int tmp_l;
	unsigned short tmp_s;
	char hostname_buf[20];
	int i, j, k;
	int rbid, len, offset, num_of_lr, num_of_hbids, num_of_dn_loc, num_of_triplets;

	memcpy(&tmp_ll, buf_ptr, sizeof(unsigned long long));
	tmp_ll = ntohll(tmp_ll);
	printf("NN Metadata R: client request id = \t\t%llu\n", tmp_ll);
	buf_ptr += sizeof(unsigned long long);

	memcpy(&tmp_ll, buf_ptr, sizeof(unsigned long long));
	tmp_ll = ntohll(tmp_ll);
	printf("NN Metadata R: nn server id = \t\t%llu\n", tmp_ll);
	buf_ptr += sizeof(unsigned long long);

	memcpy(&tmp_s, buf_ptr, sizeof(unsigned short));
	tmp_s = ntohs(tmp_s);
	printf("NN Metadata R: num of locatedReq = \t\t%d\n", tmp_s);
	buf_ptr += sizeof(unsigned short);

	printf("Ready to parse LR : tmp_s %d\n", tmp_s);
	num_of_lr = tmp_s;
	for(i=0;i<num_of_lr;i++) {
		printf("Parse %d \n", i);
		memcpy(&tmp_s, buf_ptr, sizeof(unsigned short));
		tmp_s = ntohs(tmp_s);
		buf_ptr += sizeof(unsigned short);
		printf("NN Metadata R: locatedReq[%d].num_of_hbids = \t\t%d\n", i, tmp_s);
		num_of_hbids = tmp_s;
		for(j=0;j<num_of_hbids;j++) {
			memcpy(&tmp_ll, buf_ptr, sizeof(unsigned long long));
			tmp_ll = ntohll(tmp_ll);
			printf("NN Metadata R: locatedReq[%d].hbid[%d] = \t\t%d\n", i, j, tmp_ll);
			buf_ptr += sizeof(unsigned long long);
		}

		memcpy(&tmp_s, buf_ptr, sizeof(unsigned short));
		tmp_s = ntohs(tmp_s);
		buf_ptr += sizeof(unsigned short);
		printf("NN Metadata R: locatedReq[%d].num_of_dnloc = \t\t%d\n", i, tmp_s);

		num_of_dn_loc = tmp_s;
		for(j=0;j<num_of_dn_loc;j++) {
			memcpy(&tmp_s, buf_ptr, sizeof(unsigned short));
			tmp_s = ntohs(tmp_s);
			buf_ptr += sizeof(unsigned short);
			printf("NN Metadata R: locatedReq[%d].dnloc[%d].len_of_hn = \t\t%d\n", i, j, tmp_s);

			memcpy(&hostname_buf, buf_ptr, sizeof(char)*tmp_s);
			buf_ptr += sizeof(char)*tmp_s;
			printf("NN Metadata R: locatedReq[%d].dnloc[%d].hostname = \t\t%s\n", i, j, hostname_buf);

			memcpy(&tmp_l, buf_ptr, sizeof(unsigned int));
			tmp_l = ntohl(tmp_l);
			buf_ptr += sizeof(unsigned int);
			printf("NN Metadata R: locatedReq[%d].dnloc[%d].port = \t\t%d\n", i, j, tmp_l);

			memcpy(&tmp_s, buf_ptr, sizeof(unsigned short));
			tmp_s = ntohs(tmp_s);
			buf_ptr += sizeof(unsigned short);
			printf("NN Metadata R: locatedReq[%d].dnloc[%d].len_of_phyloc = \t\t%d\n", i, j, tmp_s);

			num_of_triplets = tmp_s;
			for(k=0;k<num_of_triplets;k++) {
				memcpy(&tmp_ll, buf_ptr, sizeof(unsigned long long));
				tmp_ll = ntohll(tmp_ll);
				decompose_triple(tmp_ll, &rbid, &len, &offset);
				printf("NN Metadata R: locatedReq[%d].dnloc[%d].phyloc[k].triplets %llu (r, l, o) = \t\t%llu (%d, %d, %d)\n",
						i, j, k, tmp_ll, rbid, len, offset);
				buf_ptr += sizeof(unsigned long long);
			}
		}

		memcpy(&tmp_s, buf_ptr, sizeof(unsigned short));
		tmp_s = ntohs(tmp_s);
		buf_ptr += sizeof(unsigned short);
		printf("NN Metadata R: locatedReq[%d].hb_state = \t\t%d\n", i, tmp_s);
	}

	printf("Parse Complete\n");
}

void process_metadata_operation(char * s_opcode, char * s_vol_id, char * s_startLB, char * s_LBcnt) {
	unsigned short opcode = 0;
	unsigned long long vol_id, start_LB, req_id;
	unsigned int LB_cnt;
	char buffer[1024];
	unsigned short total_len = 0;
	socket_t sockfd;
	int ret;


	if(s_opcode[0] == 'a' && s_opcode[1] == 'o')
		opcode = 1;
	else if(s_opcode[0] == 'a' && s_opcode[1] == 'n')
		opcode = 106;
	else if(s_opcode[0] == 'q' && s_opcode[1] == 'r')
		opcode = 0;
	else if(s_opcode[0] == 'q' && s_opcode[1] == 'w')
		opcode = 105;

	vol_id = atoll(s_vol_id);
	start_LB = atoll(s_startLB);
	LB_cnt = atol(s_LBcnt);
	req_id = 100;

	printf("process_metadata_operation start opcode=%d volid=%llu startLB=%llu LBcnt=%llu reqid=%llu\n",
				opcode, vol_id, start_LB, LB_cnt, req_id);

	sockfd = socket_connect_to("127.0.0.1", 1234);

	if(sockfd < 0) {
		printf("Cannot create socket connection");
		return;
	}


	opcode = htons(opcode);
	start_LB = htonll(start_LB);
	req_id = htonll(req_id);
	vol_id = htonll(vol_id);
	LB_cnt = htonl(LB_cnt);

	ret = socket_sendto(sockfd, (char*)(&magicNum), 8);
	ret += socket_sendto(sockfd, (char*)(&opcode), 2);
	ret += socket_sendto(sockfd, (char*)(&start_LB), 8);
	ret += socket_sendto(sockfd, (char*)(&req_id), 8);
	ret += socket_sendto(sockfd, (char*)(&vol_id), 8);
	ret += socket_sendto(sockfd, (char*)(&LB_cnt), 4);
	ret += socket_sendto(sockfd, (char*)(&magicNum), 8);

	if(ret != 46)
		printf("Send error\n");

	ret = socket_recvfrom(sockfd, buffer, sizeof(unsigned long long)+sizeof(short));
	memcpy(&total_len, buffer+sizeof(unsigned long long), sizeof(unsigned short));
	total_len = ntohs(total_len);

	printf("Total Packet length is %d\n", total_len);

	//ret = recv(sockfd, buffer, total_len, 0);
	ret = socket_recvfrom(sockfd, buffer, total_len);

	if(ret != total_len)
		printf("Receive Error : Expcected %d actually %d\n", total_len, ret);
	else
		printf("Receive : %d\n", total_len);
	parse_namenode_metadata_response(buffer);

	socket_close(sockfd);
}

void process_metadata_write_report() {
	struct sockaddr_in address;
	int i, ret = 0;
	socket_t sockfd;
	unsigned short opcode = 20, subopcode = 0;
	unsigned long long server_id = 1, n_server_id, triplets = 100, n_triplets;
	unsigned int num_of_failedDNs = 0, n_num_of_failedDNs = 0;
	unsigned int pkt_len = 36, n_pkt_len, port, n_port, num_of_triplets, n_num_of_triplets;
	char hname[20];

	memcpy(hname, "127.0.0.1\0", 10);


	sockfd = socket_connect_to("127.0.0.1", 1234);

	if(sockfd < 0) {
		printf("Cannot create socket connection");
		return;
	}

	ret = socket_sendto(sockfd, (char*)(&magicNum), 8);
	opcode = htons(opcode);
	ret += socket_sendto(sockfd, (char*)(&opcode), 2);
	subopcode = htons(subopcode);
	ret += socket_sendto(sockfd, (char*)(&subopcode), 2);
	n_server_id = htonll(server_id);
	ret += socket_sendto(sockfd, (char*)(&n_server_id), 8);
	num_of_failedDNs = 3;
	n_num_of_failedDNs = htonl(num_of_failedDNs);
	ret += socket_sendto(sockfd, (char*)(&n_num_of_failedDNs), 4);

	for(i=0;i<num_of_failedDNs;i++) {
		n_pkt_len = htonl(pkt_len);
		ret += socket_sendto(sockfd, (char*)(&n_pkt_len), 4);
		ret += socket_sendto(sockfd, hname, 20);
		port = 20009 + i; n_port = htonl(port);
		ret += socket_sendto(sockfd, (char*)&n_port, 4);

		num_of_triplets = 1; n_num_of_triplets = htonl(num_of_triplets);
		ret += socket_sendto(sockfd, (char*)&n_num_of_triplets, 4);
		triplets = triplets + i; n_triplets = htonll(triplets);
		ret += socket_sendto(sockfd, (char*)&triplets, 8);
	}

	ret = socket_sendto(sockfd, (char*)(&magicNum), 8);

	socket_close(sockfd);
}

void process_metadata_query_write_report() {
	struct sockaddr_in address;
	socket_t sockfd;
	int i, ret = 0;
	unsigned short opcode = 20, subopcode = 64;
	unsigned long long vol_id = 1, n_vol_id = 0;
	unsigned int num_of_LB = 16, n_num_of_LB = 0;
	unsigned int num_of_successDNs = 3, n_num_of_successDNs = 0;
	unsigned long long LBID = 100, n_LBID;
	unsigned int pkt_len = 36, n_pkt_len, port, n_port, num_of_triplets, n_num_of_triplets;
	unsigned long long triplets = 0, n_triplets;
	char hname[20];

	memcpy(hname, "127.0.0.1\0", 10);


	sockfd = socket_connect_to("127.0.0.1", 1234);

	if(sockfd < 0) {
		printf("Cannot create socket connection");
		return;
	}

	ret = socket_sendto(sockfd, (char*)(&magicNum), 8);

	opcode = htons(opcode);
	ret += socket_sendto(sockfd, (char*)(&opcode), 2);

	subopcode = htons(subopcode);
	ret += socket_sendto(sockfd, (char*)(&subopcode), 2);

	n_vol_id = htonll(vol_id);
	ret += socket_sendto(sockfd, (char*)(&n_vol_id), 8);

	n_num_of_LB = htonl(num_of_LB);
	ret += socket_sendto(sockfd, (char*)(&n_num_of_LB), 4);

	n_num_of_successDNs = htonl(num_of_successDNs);
	ret += socket_sendto(sockfd, (char*)(&n_num_of_successDNs), 4);

	for(i=0;i<num_of_LB;i++) {
		LBID++;
		n_LBID = htonll(LBID);
		ret += socket_sendto(sockfd, (char*)(&n_LBID), 8);
		printf("Send LBIDs[%d] : %llu\n", i, LBID);
	}

	for(i=0;i<num_of_successDNs;i++) {
		n_pkt_len = htonl(pkt_len);
		ret += socket_sendto(sockfd, (char*)(&n_pkt_len), 4);
		ret += socket_sendto(sockfd, hname, 20);
		port = 20009 + i; n_port = htonl(port);
		ret += socket_sendto(sockfd, (char*)&n_port, 4);

		num_of_triplets = 1; n_num_of_triplets = htonl(num_of_triplets);
		ret += socket_sendto(sockfd, (char*)&n_num_of_triplets, 4);
		triplets = triplets + i; n_triplets = htonll(triplets);
		ret += socket_sendto(sockfd, (char*)&triplets, 8);
	}

	ret = socket_sendto(sockfd, (char*)(&magicNum), 8);

	socket_close(sockfd);
}

void process_metadata_query_read_report() {
	struct sockaddr_in address;
	socket_t sockfd;
	int i = 0, ret = 0, port, n_port;
	unsigned short opcode = 20, subopcode = 25;
	unsigned long long vol_id = 1, n_vol_id, LBID = 1, n_LBID;
	char hname[20];

	memcpy(hname, "127.0.0.1\0", 10);
	sockfd = socket_connect_to("127.0.0.1", 1234);

	if(sockfd < 0) {
		printf("Cannot create socket connection");
		return;
	}

	ret = socket_sendto(sockfd, (char*)(&magicNum), 8);
	opcode = htons(opcode);
	ret += socket_sendto(sockfd, (char*)(&opcode), 2);
	subopcode = htons(subopcode);
	ret += socket_sendto(sockfd, (char*)(&subopcode), 2);
	n_vol_id = htonll(vol_id);
	ret += socket_sendto(sockfd, (char*)(&n_vol_id), 8);

	n_LBID = htonll(LBID);
	ret += socket_sendto(sockfd, (char*)(&n_LBID), 8);

	ret += socket_sendto(sockfd, hname, 20);
	port = 20009 + i; n_port = htonl(port);
	ret += socket_sendto(sockfd, (char*)&n_port, 4);

	ret = socket_sendto(sockfd, (char*)(&magicNum), 8);

	socket_close(sockfd);
}

#define KERNEL_LOGICAL_SECTOR_SIZE 512

void change_byteorder_dn_raw_pkt_new_protocol(unsigned char * pkt){
	unsigned char * ptr = pkt;
	unsigned long long magic_num1 = 0, magic_num2 = 0;
	unsigned char res_type = 0;
	unsigned char num_of_bytes_byte1 = 0;
	unsigned short num_of_bytes_byte2 = 0;
	unsigned int num_of_sector = 0;
	unsigned int cid = 0;
	unsigned int did = 0;

	memcpy(&magic_num1, ptr, sizeof(unsigned char)*8);
	magic_num1 = ntohll(magic_num1);
	ptr = ptr + 8;

	memcpy(&res_type, ptr, sizeof(unsigned char));
	ptr = ptr + 1;

	memcpy(&num_of_bytes_byte1, ptr, sizeof(unsigned char));
	//printk("ACK: ptr=0x%02x byte1=0x%02x\n", *ptr, num_of_bytes_byte1);
	ptr = ptr + 1;
	memcpy(&num_of_bytes_byte2, ptr, sizeof(unsigned char)*2);
	//printk("ACK: ptr[0]=0x%02x ptr[1]=0x%02x byte2=0x%04x\n", *ptr, *(ptr+1), num_of_bytes_byte2);
	ptr = ptr + 2;
	num_of_sector = ntohs(num_of_bytes_byte2) | (num_of_bytes_byte1 << 16);
	//num_of_sector = num_of_sector_byte2 | (num_of_sector_byte1 << 16);
	//printk("ACK: num_of_sector_byte2=%d ntohs(num_of_sector_byte2)=%d num_of_sector_byte1=%d num_of_sector_byte1=%d\n",
	//		num_of_bytes_byte2, ntohs(num_of_bytes_byte2), num_of_bytes_byte1, (num_of_bytes_byte1 << 16));
	ptr = ptr + 4; //for reserved

	memcpy(&cid, ptr, sizeof(unsigned char)*4);
	ptr = ptr + 4;
	cid = ntohl(cid);

	memcpy(&did, ptr, sizeof(unsigned char)*4);
	ptr = ptr + 4;
	did = ntohl(did);

	memcpy(&magic_num2, ptr, sizeof(unsigned char)*8);
	magic_num2 = ntohll(magic_num2);

	printf("ACK: magic num 1 = %ld  magic num 2 = %ld\n", magic_num1, magic_num2);
	printf("ACK: responseType = %d  num_of_sector = %d pkt->payload_size=%ld\n", res_type, num_of_sector, num_of_sector*KERNEL_LOGICAL_SECTOR_SIZE);
	printf("ACK: fdnid 1 = %ld dnid = %ld\n", cid, did);
}

#define DATANODE_ACK_SIZE	32
void process_data_operation(int rw, int subopcode) {
	struct sockaddr_in address;
	char opcode = 0;
	int ret = 0, nnid=100, dnid=101, i;
	short num_of_hb = 16, n_num_of_hb, num_of_triplet = 1, n_num_of_triplets;
	char reserved[3], masks[16];
	unsigned long long startHB = 10, n_startHB, triplets = 1001, n_triplets, oldHB = 2002, n_oldHB;
	char hostname[16] = "127.0.0.1\0      ";
	int port = 20009, n_port;
	char payload[16*KERNEL_LOGICAL_SECTOR_SIZE*8], ack_header[DATANODE_ACK_SIZE];
	socket_t sockfd;

	printf("process_data_operation start\n");
	sockfd = socket_connect_to("127.0.0.1", 20009);

	if(sockfd < 0) {
		printf("Cannot create socket connection");
		return;
	}

	printf("process_data_operation start 1\n");

	n_port = htonl(20009);
	memset(masks, 0xff, sizeof(char)*16);
	if(rw == 0) {//Read
		opcode = 4;
	} else {
		if(subopcode == 0) {//write
			opcode = 3;
		} else { //write after get old
			opcode = 5;
		}
	}

	printf("process_data_operation start 2\n");

	ret = socket_sendto(sockfd, (char*)(&magicNum), 8);
	ret += socket_sendto(sockfd, (char*)(&opcode), 1);
	n_num_of_hb = htons(num_of_hb);
	ret += socket_sendto(sockfd, (char*)(&n_num_of_hb), 2);
	n_num_of_triplets = htons(num_of_triplet);
	ret += socket_sendto(sockfd, (char*)(&n_num_of_triplets), 2);
	ret += socket_sendto(sockfd, reserved, 3);

	nnid = htonl(nnid); dnid = htonl(dnid);
	ret += socket_sendto(sockfd, (char*)(&nnid), 4);
	ret += socket_sendto(sockfd, (char*)(&dnid), 4);

	ret += socket_sendto(sockfd, (char*)(&magicNum), 8);

	for(i=0;i<num_of_hb;i++) {
		startHB++;
		n_startHB = htonll(startHB);
		ret += socket_sendto(sockfd, (char*)(&n_startHB), 8);
	}

	ret += socket_sendto(sockfd, masks, sizeof(char)*num_of_hb);

	for(i=0;i<num_of_triplet;i++) {
		triplets++;
		n_triplets = htonll(triplets);
		ret += socket_sendto(sockfd, (char*)(&n_triplets), 8);
	}

	printf("process_data_operation start 3\n");

	if(rw != 0) {
		if(subopcode == 1) {
			for(i=0;i<num_of_hb;i++) {
				ret += socket_sendto(sockfd, hostname, 16);
				ret += socket_sendto(sockfd, (char*)&n_port, 4);
				oldHB++;
				n_oldHB = htonll(oldHB);
				ret += socket_sendto(sockfd, (char*)&n_oldHB, 8);
				triplets++;
				n_triplets = htonll(triplets);
				ret += socket_sendto(sockfd, (char*)(&n_triplets), 8);
			}
		}


		ret = socket_sendto(sockfd, payload, KERNEL_LOGICAL_SECTOR_SIZE*num_of_hb*8);
		printf("process_data_operation start 3-1 write data: %d (excepted: %d)\n", ret, KERNEL_LOGICAL_SECTOR_SIZE*num_of_hb*8);
	}

	ret = socket_recvfrom(sockfd, ack_header, DATANODE_ACK_SIZE);

	change_byteorder_dn_raw_pkt_new_protocol(ack_header); //Mem ack or read ack

	if(rw == 0) {
		ret = socket_recvfrom(sockfd, payload, KERNEL_LOGICAL_SECTOR_SIZE*num_of_hb);
	} else {
		ret = socket_recvfrom(sockfd, ack_header, DATANODE_ACK_SIZE);
		change_byteorder_dn_raw_pkt_new_protocol(ack_header); //Disk ack
	}

	socket_close(sockfd);
}

int main(int argc, char ** argv) {
	if(argc <= 1) {
		printf("Error format\n");
		return 0;
	}

	if(argv[1][0] == 'c') {
		printf("Test Create Volume\n");
		process_create_volume(argv[2]);
	} else if(argv[1][0] == 'q') {
		printf("Test Query Volume\n");
		process_query_volume(argv[2]);
	} else if(argv[1][0] == 'm') {
		printf("Test Allocate/Query Metadata\n");
		process_metadata_operation(argv[2], argv[3], argv[4], argv[5]);
	} else if(argv[1][0] == 'r' && argv[1][1] == 'w') {
		printf("Test Report Metadata allocate request\n");
		process_metadata_write_report();
	} else if(argv[1][0] == 'r' && argv[1][1] == 'q' && argv[2][0] == 'w') {
		printf("Test Report Metadata query for write request\n");
		process_metadata_query_write_report();
	} else if(argv[1][0] == 'r' && argv[1][1] == 'q' && argv[2][0] == 'r') {
		printf("Test Report Metadata query for read request\n");
		process_metadata_query_read_report();
	} else if(argv[1][0] == 'd' && argv[1][1] == 'w') {
		printf("Test Write Payload\n");
		process_data_operation(1, 0);
	} else if(argv[1][0] == 'd' && argv[1][1] == 'r') {
		printf("Test Read Payload\n");
		process_data_operation(0, 0);
	} else if(argv[1][0] == 'd' && argv[1][1] == 'o') {
		printf("Test Write After Get old Payload\n");
		process_data_operation(1, 1);
	}

	return 0;
}
