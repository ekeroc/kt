/*
 * NN_Matadata_Manager.c
 *
 *  Created on: 2012/8/14
 *      Author: 990158
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include "NN_Metadata_Manager.h"
#include "NN_Volume_Manager.h"

unsigned long long global_hbid;
unsigned long long global_rbid;
unsigned long long global_serverID;

unsigned long long DMS_HBID_Generator() {
	global_hbid++;
	return global_hbid;
}

unsigned long long DMS_rBID_Generator() {
	global_rbid++;
	return global_rbid;
}

unsigned long long DMS_serverID_Generator() {
	global_serverID++;
	return global_serverID;
}

unsigned long long getFreeRealBlock(unsigned long long start_LB, unsigned int num_of_LBs) {
	return DMS_rBID_Generator();
}

struct LocatedRequest ** AllocateMetadata_For_Write(unsigned long long volume_id,
		unsigned long long start_LB, unsigned int num_of_LBs, int * num_of_lrs) {
	struct LocatedRequest ** arr_lr = NULL;
	struct LocatedRequest ** lr_tmp = NULL;

	syslog(LOG_ERR, "AllocateMetadata_For_Write start with volid=%llu start_LB=%llu LBcnt=%d\n", volume_id, start_LB, num_of_LBs);
	arr_lr = (struct LocatedRequest **)malloc(sizeof(struct LocatedRequest *)*2);

	lr_tmp = AllocateMetadata_For_Write_Without_Old(volume_id, start_LB, num_of_LBs, num_of_lrs);
	arr_lr[0] = lr_tmp[0];
	lr_tmp[0] = NULL;
	free(lr_tmp);
	lr_tmp = QueryMetadata_For_Write(volume_id, start_LB, num_of_LBs, num_of_lrs);
	arr_lr[1] = lr_tmp[0];
	lr_tmp[0] = NULL;
	free(lr_tmp);

	*num_of_lrs = 2;

	syslog(LOG_ERR, "AllocateMetadata_For_Write DONE with volid=%llu start_LB=%llu LBcnt=%d\n", volume_id, start_LB, num_of_LBs);
	return arr_lr;
}

struct LocatedRequest ** AllocateMetadata_For_Write_Without_Old(unsigned long long volume_id,
		unsigned long long start_LB, unsigned int num_of_LBs, int * num_of_lrs) {
	struct LocatedRequest ** lr = NULL;
	int i, cnt_lrs = 1;

	syslog(LOG_ERR, "AllocateMetadata_For_Write_Without_Old start with volid=%llu start_LB=%llu LBcnt=%d\n", volume_id, start_LB, num_of_LBs);

	lr = (struct LocatedRequest **)malloc(sizeof(struct LocatedRequest *)*cnt_lrs);
	lr[0] = (struct LocatedRequest *)malloc(sizeof(struct LocatedRequest));
	lr[0]->HBIDs = (unsigned long long *)malloc(sizeof(unsigned long long)*num_of_LBs);
	lr[0]->num_of_hbids = num_of_LBs;
	for(i=0;i<num_of_LBs;i++)
		lr[0]->HBIDs[i] = DMS_HBID_Generator();

	lr[0]->num_dn_locations = DEFAULT_VOLUME_REPLICA_FACTOR;
	lr[0]->dnLocations = (struct DatanodeLocation *)malloc(sizeof(struct DatanodeLocation)*lr[0]->num_dn_locations);

	for(i=0;i<DEFAULT_VOLUME_REPLICA_FACTOR;i++) {
		lr[0]->dnLocations[i].num_of_phy_loc = DEFAULT_NUM_OF_PHYSICAL_LOCATIONS;
		memcpy(lr[0]->dnLocations[i].hostname, DEFAULT_DATANODE_HOSTNAME, DEFAULT_DATANODE_HOSTNAME_LEN);
		lr[0]->dnLocations[i].hostname[DEFAULT_DATANODE_HOSTNAME_LEN] = '\0';
		lr[0]->dnLocations[i].physical_locs = (struct PhysicalLocation *)malloc(sizeof(struct PhysicalLocation)*lr[0]->dnLocations[i].num_of_phy_loc);
		lr[0]->dnLocations[i].physical_locs[0].RBID = getFreeRealBlock(start_LB, num_of_LBs);
		lr[0]->dnLocations[i].physical_locs[0].length = num_of_LBs;
		lr[0]->dnLocations[i].physical_locs[0].offsetInRB = 0;

		if(i==0)
			lr[0]->dnLocations[i].port = DEFAULT_DATANODE_1_PORT;
		else if(i==1)
			lr[0]->dnLocations[i].port = DEFAULT_DATANODE_2_PORT;
		else
			lr[0]->dnLocations[i].port = DEFAULT_DATANODE_3_PORT;
	}

	lr[0]->state = LR_STATE_SUCCESS;
	*num_of_lrs = cnt_lrs;

	syslog(LOG_ERR, "AllocateMetadata_For_Write_Without_Old DONE with volid=%llu start_LB=%llu LBcnt=%d\n", volume_id, start_LB, num_of_LBs);
	return lr;
}

struct LocatedRequest ** QueryMetadata_For_Write(unsigned long long volume_id,
		unsigned long long start_LB, unsigned int num_of_LBs, int * num_of_lrs) {
	struct LocatedRequest ** lr = NULL;

	syslog(LOG_ERR, "QueryMetadata_For_Write start with volid=%llu start_LB=%llu LBcnt=%d\n", volume_id, start_LB, num_of_LBs);

	lr = AllocateMetadata_For_Write_Without_Old(volume_id, start_LB, num_of_LBs, num_of_lrs);

	syslog(LOG_ERR, "QueryMetadata_For_Write DONE with volid=%llu start_LB=%llu LBcnt=%d\n", volume_id, start_LB, num_of_LBs);
	return lr;
}

struct LocatedRequest ** QueryMetadata_For_Read(unsigned long long volume_id,
		unsigned long long start_LB, unsigned int num_of_LBs, int * num_of_lrs) {
	struct LocatedRequest ** lr = NULL;

	syslog(LOG_ERR, "QueryMetadata_For_Read start with volid=%llu start_LB=%llu LBcnt=%d\n", volume_id, start_LB, num_of_LBs);

	lr = AllocateMetadata_For_Write_Without_Old(volume_id, start_LB, num_of_LBs, num_of_lrs);

	syslog(LOG_ERR, "QueryMetadata_For_Read DONE with volid=%llu start_LB=%llu LBcnt=%d\n", volume_id, start_LB, num_of_LBs);
	return lr;
}

void Initialize_DMS_Metadata_Manager() {
	global_hbid = 0;
	global_rbid = 0;
	global_serverID = 0;
}
