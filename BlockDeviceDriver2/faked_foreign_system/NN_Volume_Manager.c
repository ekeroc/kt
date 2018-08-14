/*
 * NN_Volume_Manager.c
 *
 *  Created on: 2012/8/14
 *      Author: 990158
 */
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include "NN_Volume_Manager.h"

unsigned long long global_volume_id;
struct dms_volume_info *list_dms_volume;
struct dms_volume_info *list_last;

/* dms_volume operation */
void print_all_volume_info() {
	struct dms_volume_info * c = list_dms_volume;

	while(c->next != NULL) {
		c = c->next;
		syslog(LOG_ERR, "print_all_volume_info: list volid %llu\n", c->volume_id);
	}
}

struct dms_volume_info * create_dms_volume(unsigned long long vol_id, unsigned long long vol_cap, int replica_factor) {
	struct dms_volume_info * vol = NULL;

	vol = (struct dms_volume_info *)malloc(sizeof(struct dms_volume_info));

	vol->volume_id = vol_id;
	vol->volume_capacity = vol_cap;
	vol->replica_factor = replica_factor;
	vol->next = NULL;
	return vol;
}

/* operation of list_dms_volume */
void add_volume_to_list(struct dms_volume_info * list_head, struct dms_volume_info * vol) {
	list_last->next = vol;
	list_last = vol;

	print_all_volume_info();
}

struct dms_volume_info * delete_volume_from_list(struct dms_volume_info * list_head, unsigned long long vol_id) {
	struct dms_volume_info * c = list_head;
	struct dms_volume_info * tmp = NULL;

	while(c->next != NULL) {
		if(c->next->volume_id == vol_id) {
			if(c->next->next != NULL) {
				tmp = c->next;
				c->next = tmp->next;
				tmp->next = NULL;
			} else {
				tmp = c->next;
				c->next = NULL;
				list_last = c;
			}
			break;
		} else {
			c = c->next;
		}
	}

	return tmp;
}

void free_all_volume_from_list(struct dms_volume_info * list_head) {
	struct dms_volume_info * c = list_head;
	struct dms_volume_info * tmp = NULL;

	while(c->next != NULL) {
		tmp = c->next;

		if(tmp->next == NULL)
			break;

		c->next = tmp->next;
		tmp->next = NULL;
		free(tmp);
	}

	free(list_head);
}

struct dms_volume_info * query_volume_from_list(struct dms_volume_info * list_head, unsigned long long vol_id) {
	struct dms_volume_info *c = list_head;

	syslog(LOG_ERR, "Ready query_volume_from_list with volid=%llu\n", vol_id);
	while(c->next != NULL) {
		c = c->next;
		syslog(LOG_ERR, "list volid %llu\n", c->volume_id);
		if(c->volume_id == vol_id)  {
			return c;
		}
	}

	return NULL;
}

unsigned long long DMS_Volume_ID_Generator() {
	global_volume_id++;
	return global_volume_id;
}

unsigned long long DMS_Create_Volume(unsigned long long vol_capacity) {
	struct dms_volume_info * vol = NULL;
	unsigned long long vol_id = 0;

	vol_id = DMS_Volume_ID_Generator();
	vol = create_dms_volume(vol_id, vol_capacity, DEFAULT_VOLUME_REPLICA_FACTOR);
	add_volume_to_list(list_dms_volume, vol);
	return vol_id;
}

struct dms_volume_info * DMS_Query_Volume_Info(unsigned long long vol_id) {
	struct dms_volume_info * vinfo = NULL;

	vinfo = query_volume_from_list(list_dms_volume, vol_id);

	return vinfo;
}

#if 0
void Allocate_Metadata_for_Write(unsigned long long vol_id, unsigned long long start_LB, int LB_cnt) {

}

void Query_Metadata_for_ReadWrite(unsigned long long vol_id, unsigned long long start_LB, int LB_cnt) {

}

void Report_Metadata_Status(unsigned long long vol_id) {

}
#endif

void Initialize_DMS_Volume_Manager() {
	global_volume_id = 0;
	list_dms_volume = create_dms_volume(0, 0, 0);
	list_dms_volume->next = NULL;
	list_last = list_dms_volume;
}

void Release_DMS_Volume_Manager() {
	free_all_volume_from_list(list_dms_volume);
}
