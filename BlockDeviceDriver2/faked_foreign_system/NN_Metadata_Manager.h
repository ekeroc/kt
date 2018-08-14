/*
 * NN_Metadata_Manager.h
 *
 *  Created on: 2012/8/14
 *      Author: 990158
 */

#ifndef NN_METADATA_MANAGER_H_
#define NN_METADATA_MANAGER_H_

#define LR_STATE_SUCCESS	0
#define LR_STATE_HBNOTFOUND	1

#define DEFAULT_DATANODE_HOSTNAME	"127.0.0.1"
#define DEFAULT_DATANODE_HOSTNAME_LEN	9
#define	DEFAULT_DATANODE_1_PORT		20009
#define	DEFAULT_DATANODE_2_PORT		20010
#define	DEFAULT_DATANODE_3_PORT		20011
#define DEFAULT_NUM_OF_PHYSICAL_LOCATIONS	1

#define CCMA_HOST_NAME_LEN 20

struct PhysicalLocation {
	int RBID;
	int offsetInRB;
	int length;
};

struct DatanodeLocation {
	char hostname[CCMA_HOST_NAME_LEN];
	int port;
	char num_of_phy_loc;
	struct PhysicalLocation * physical_locs;
};

struct LocatedRequest {
	char num_of_hbids;
	char num_dn_locations;
	short state;

	unsigned long long * HBIDs;
	struct DatanodeLocation * dnLocations;
};

void Initialize_DMS_Metadata_Manager();
struct LocatedRequest ** AllocateMetadata_For_Write_Without_Old(unsigned long long volume_id,
		unsigned long long start_LB, unsigned int num_of_LBs, int * num_of_lrs);
struct LocatedRequest ** QueryMetadata_For_Write(unsigned long long volume_id,
		unsigned long long start_LB, unsigned int num_of_LBs, int * num_of_lrs);
struct LocatedRequest ** AllocateMetadata_For_Write(unsigned long long volume_id,
		unsigned long long start_LB, unsigned int num_of_LBs, int * num_of_lrs);
struct LocatedRequest ** QueryMetadata_For_Read(unsigned long long volume_id,
		unsigned long long start_LB, unsigned int num_of_LBs, int * num_of_lrs);

unsigned long long DMS_serverID_Generator();
#endif /* NN_METADATA_MANAGER_H_ */
