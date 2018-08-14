/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * Volume_Manager.h
 *
 */

#ifndef VOLUME_MANAGER_H_
#define VOLUME_MANAGER_H_

typedef enum {
    Vol_Data_Replic,
    Vol_Data_Parity,
} vol_protect_type_t;

typedef struct vol_protect_method {
    vol_protect_type_t prot_type;
    uint32_t replica_factor;
} vol_protect_method_t;

typedef enum {
    RW_NO_PERMIT = 0,
    R_ONLY,
    W_ONLY,
    RW_PERMIT,
} vol_rw_perm_type_t;

typedef struct volume_features {
    vol_protect_method_t prot_method;
    bool share_volume;
    bool mdata_cache_enable; //meta data cache enable
    bool pdata_cache_enable; //payload data cache enable
    //bool fc_enable;        //flow control enable, current only global control
    vol_rw_perm_type_t rw_permission;      //0x11 : RW, 0x01: Read only, 0x10: Write only, 0x00:
} volume_features_t;

typedef struct volume_ovw {
    //struct rw_semaphore ovw_lock;
    int8_t *ovw_memory;
    bool ovw_init;
} volume_ovw_t;

typedef struct volume_io_queue {
    struct list_head io_pending_q;
    //struct mutex io_pending_q_lock;
    //struct list_head io_ovlp_q;
} volume_io_queue_t;

typedef struct volume_device {
    uint32_t vol_id;
    uint64_t capacity;

    volume_features_t vol_feature;
    volume_ovw_t ovw_data;

    volume_io_queue_t vol_io_q;

    uint64_t num_reqs;

    void *user_data;
} volume_device_t;

#define set_vol_share(vol, is_share) do { \
     vol->vol_feature.share_volume = is_share;\
} while (0)

#define set_vol_replica(vol, num_replica) do { \
     vol->vol_feature.prot_method.prot_type = Vol_Data_Replic;\
     vol->vol_feature.prot_method.replica_factor = num_replica;\
} while (0)

uint16_t get_volume_replica(uint32_t volid);
volume_device_t *query_volume(uint32_t volid);
volume_device_t *create_vol_inst(uint32_t volid, uint64_t vol_cap);
void free_vol_inst(volume_device_t *vol_inst);
#endif /* VOLUME_MANAGER_H_ */
