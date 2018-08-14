/*
 * NN_Volume_Manager.h
 *
 *  Created on: 2012/8/14
 *      Author: 990158
 */

#ifndef NN_VOLUME_MANAGER_H_
#define NN_VOLUME_MANAGER_H_

#define DEFAULT_VOLUME_REPLICA_FACTOR	3
//#define DEFAULT_VOLUME_REPLICA_FACTOR	1

struct dms_volume_info {
  unsigned long long volume_id;
  unsigned long long volume_capacity;
  short replica_factor;
  struct dms_volume_info *next;
};

void Initialize_DMS_Volume_Manager();
void Release_DMS_Volume_Manager();
struct dms_volume_info * DMS_Query_Volume_Info(unsigned long long vol_id);

#endif /* NN_VOLUME_MANAGER_H_ */
