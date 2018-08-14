/*
 * DataNode_Data_Manager.h
 *
 *  Created on: 2012/8/20
 *      Author: 990158
 */

#ifndef DATANODE_DATA_MANAGER_H_
#define DATANODE_DATA_MANAGER_H_


char Process_Payload_Operation_Request(char req_type, unsigned short num_of_hbids, unsigned long long * hbids,
		unsigned char * masks, unsigned short num_of_triplets, unsigned long long * triplets, char * payload,
		unsigned short num_of_sector, struct dn_request_old_loc * old_loc);
#endif /* DATANODE_DATA_MANAGER_H_ */
