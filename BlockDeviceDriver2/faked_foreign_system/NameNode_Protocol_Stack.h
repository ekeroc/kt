#ifndef _NameNode_Protocol_Stack_h_
#define _NameNode_Protocol_Stack_h_

/* Define Namenode Request / Response Operation Code */
#define	NN_REQUEST_METADATA_READ				0
#define	NN_REQUEST_METADATA_WRITE				1
#define	NN_REQUEST_METADATA_OVERWRITE			105
#define	NN_REQUEST_METADATA_WRITE_NO_GET_OLD	106
#define NN_REQUEST_QUERY_VOLUME_INFO			101
#define	NN_REQUEST_CREATE_VOLUME				1001
#define NN_REQUEST_CLONE_VOLUME					1002
#define	NN_REQUEST_METADATA_REPORT				20

#define NN_REQUEST_METADATA_REPORT_SUBOPCODE_WRITE_FAIL	0
#define NN_REQUEST_METADATA_REPORT_SUBOPCODE_READ_FAIL	25
#define NN_REQUEST_METADATA_REPORT_SUBOPCODE_OVW_FAIL	64

#define CCMA_HOSTNAME_LEN		20

struct nn_request_header {
	unsigned long long magic_number;
	short	opcode;
} __attribute__ ((__packed__));

struct nn_response_header {
	short	opcode;
} __attribute__ ((__packed__));

struct nn_request_volume_info {
	struct nn_request_header header;
	int volume_id;
} __attribute__ ((__packed__));

struct nn_response_volume_info {
	struct nn_response_header header;
	unsigned long long volume_capacity;
	int replica_factor;
};

struct nn_request_volume_create {
	struct nn_request_header header;
	unsigned long long volume_capacity;
} __attribute__ ((__packed__));

struct nn_response_volume_create {
	struct nn_response_header header;
	unsigned long long volume_id;
};

struct nn_request_metadata {
	struct nn_request_header header;
	unsigned long long start_LB;
	unsigned long long request_id;
	unsigned long long volume_id;
	unsigned int LB_cnt;
	unsigned long long magic_number;
} __attribute__ ((__packed__));

struct nn_metadata_dn_location {
	unsigned short len_hostname;
	char * hostname;
	int port;
	unsigned short num_of_location_triplets;
	unsigned long long * location_triplets;
};

struct nn_metadata {
	unsigned short num_of_hbids;
	unsigned long long * hbids;
	unsigned short num_of_datanode_location;
	struct nn_metadata_dn_location * dn_location;
	unsigned short location_state;
};

struct nn_response_metadata {
	struct nn_response_header header;
	unsigned long long magic_number;
	unsigned short response_type;
	unsigned short metadata_packet_len;
	unsigned long long request_id;
	unsigned long long namenode_response_id;
	unsigned short num_of_lr;
	struct nn_metadata * nn_metadata;
	unsigned int fc_slowstart_rate;
	unsigned int fc_slowstart_maxreqs;
	unsigned int fc_curr_rate;
	unsigned int fc_curr_maxreqs;
	unsigned int fc_rateAge;
};

struct nn_request_DN_INFO {
	unsigned int packet_size;
	char hostname[CCMA_HOSTNAME_LEN];
	unsigned int port;
	unsigned int num_of_triplets;
	unsigned long long * triplets;
} __attribute__ ((__packed__));

struct nn_request_metadata_write_error_report {
	unsigned long long server_id;
	unsigned int num_of_failed_DNs;
	struct nn_request_DN_INFO * failed_DNs;
	unsigned long long magic_number;
};

struct nn_request_metadata_overwrite_error_report {
	unsigned long long volume_id;
	unsigned int num_of_LBs;
	unsigned int num_of_success_DNs;
	unsigned long long * LBIDs;
	struct nn_request_DN_INFO * success_DNs;
	unsigned long long magic_number;
};

struct nn_request_metadata_read_error_report {
	unsigned long long volume_id;
	unsigned long long lbid;
	char hostname[CCMA_HOSTNAME_LEN];
	unsigned int port;
	unsigned long long magic_number;
} __attribute__ ((__packed__));

struct nn_request_metadata_report {
	struct nn_request_header header;
	unsigned short sub_opcode;
	union {
		struct nn_request_metadata_write_error_report * w_error_report;
		struct nn_request_metadata_overwrite_error_report * ovw_error_report;
		struct nn_request_metadata_read_error_report * read_error_report;
	};
};

#define NN_REQUEST_METADATA_REQUEST_BODY_LEN (sizeof(struct nn_request_metadata) - sizeof(struct nn_request_header))
#define NN_REQUEST_VOLINFO_REQUEST_BODY_LEN (sizeof(struct nn_request_volume_info) - sizeof(struct nn_request_header))
#define NN_REQUEST_VOLCREATE_REQUEST_BODY_LEN (sizeof(struct nn_request_volume_create) - sizeof(struct nn_request_header))

struct nn_request_header * Receive_NameNode_Request(int sock_fd);
int Send_NameName_Response(int sock_fd, struct nn_response_header * nn_response);

int free_nn_metadata_response(struct nn_response_metadata * nn_response);
#endif
