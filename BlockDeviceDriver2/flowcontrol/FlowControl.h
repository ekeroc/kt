/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * FlowControl.h
 *
 */
#ifndef _FLOWCONTROL_H_
#define _FLOWCONTROL_H_

typedef enum {
    RES_TYPE_NN,
    RES_TYPE_DN,
} res_type_t;

#define MAX_NN_RES_IN_SPEED_GROUP   1
#define MAX_DN_RES_IN_SPEED_GROUP   20

#define WAIT_TYPE_FLOW_CONTROL    0
#define WAIT_TYPE_FIX_TIME        1
#define WAIT_TYPE_ALL_IO_DONE    2
#define WAIT_TYPE_WAIT_Q        3

#define MAX_NUM_OF_SAMPLE        10000
#define NN_RESPONSE_SUB_TYPE_QUERY_METADATA            0
#define NN_RESPONSE_SUB_TYPE_ALLOC_METADATA_NoOld    1
#define NN_RESPONSE_SUB_TYPE_ALLOC_METADATA_WithOld    2

#define DN_RESPONSE_SUB_TYPE_Read            0
#define DN_RESPONSE_SUB_TYPE_Write            1

#define RESOURCES_IS_FREE_TO_USE    0
#define RESOURCES_IS_NOT_AVAILABLE    1

#define FC_NO_BLOCK    0
#define FC_MAX_RES_REQUESTS    -1
#define    FC_RES_CONNECTION_LOST    -2

typedef struct dms_res_item {
    uint32_t ipaddr;
    uint32_t port;

    res_type_t res_type;
    uint16_t res_id;
    rwlock_t res_lock;

    uint16_t res_state;
    bool is_available;

    struct list_head list_resq;

    /* Following member belong to new flow control mechanism */
    int32_t fc_slow_start_rate; //IOPS for NN, TP for DN
    int32_t fc_start_max_reqs;
    int32_t fc_rate_age_time;   //unit: seconds
    int32_t fc_curr_rate;       //IOPS for NN, TP for DN
    int32_t fc_curr_max_reqs;

    struct mutex alloc_res_lock;
    atomic_t alloc_out_res_rate; //IOPS for NN , TP for DN
    atomic_t alloc_out_res_max; //max # of requests no response
    uint64_t last_access_time;

    /* Following member belong to current flow control mechanism */
    uint64_t *res_response_time;
    uint32_t num_of_req_time_in_count;
    uint64_t total_response_time;
    int32_t last_modified_index;

    /* Following member belong to DN selection */
    atomic64_t number_timeout_occur;

    /* Following member belong to performance measurement */
    long long Total_process_time; //micro-seconds
    atomic64_t Total_process_cnt;

    long long NN_Process_Time_Query_Metadata;  //micro-seconds
    long long NN_Process_Time_Alloc_Metadata_And_NoOld;  //micro-seconds
    long long NN_Process_Time_Alloc_Metadata_And_WithOld;  //micro-seconds
    atomic64_t NN_Process_CNT_Query_Metadata;
    atomic64_t NN_Process_CNT_Alloc_Metadata_And_NoOld;
    atomic64_t NN_Process_CNT_Alloc_Metadata_And_WithOld;
    long long NN_Process_Sample_Start_Time;
    long long NN_Process_Sample_End_Time;
    long long NN_Process_Sample_TP;
    long long NN_Process_Sample_IOPS;
    long long NN_Process_Sample_Bytes;
    atomic64_t NN_Process_Sample_CNT;

    long long DN_Process_Time_Read;   //micro-seconds
    long long DN_Process_Time_Write;  //micro-seconds
    long long DN_Process_Time_Write_MemAck;
    atomic64_t DN_Process_CNT_Read;
    atomic64_t DN_Process_CNT_Write;
    long long DN_Process_Total_Read_Bytes;  //kbytes
    long long DN_Process_Total_Write_Bytes; //kbytes
    long long DN_Process_Sample_Read_Start_Time;
    long long DN_Process_Sample_Read_End_Time;
    long long DN_Process_Sample_Write_Start_Time;
    long long DN_Process_Sample_Write_End_Time;
    long long DN_Process_Sample_Read_TP;
    long long DN_Process_Sample_Read_IOPS;
    long long DN_Process_Sample_Read_Bytes;
    atomic64_t DN_Process_Sample_Read_CNT;
    long long DN_Process_Sample_Write_TP;
    long long DN_Process_Sample_Write_IOPS;
    long long DN_Process_Sample_Write_Bytes;
    atomic64_t DN_Process_Sample_Write_CNT;
} dms_res_item_t;

//TODO: Change dn_speed_group to hash list
typedef struct dms_fc_group {
    struct hlist_node hlist;
    uint64_t volume_id;
    dms_res_item_t *nn_speed_group[MAX_NN_RES_IN_SPEED_GROUP]; //Lego: we might need more
    dms_res_item_t *dn_speed_group[MAX_DN_RES_IN_SPEED_GROUP];

    /* Following member belong to old flow control mechanism */
    struct mutex group_lock;
} dms_fc_group_t;

extern rwlock_t fcRes_qlock;
extern struct list_head fc_reslist;

extern void Get_NN_Avg_Latency_Time(dms_res_item_t *res_item,
        int32_t *all, int32_t *q_metadata, int32_t
        *alloc_no_old, int32_t *alloc_with_old);
extern void Get_DN_Avg_Latency_Time(dms_res_item_t *res_item,
        int32_t *all, int32_t *req_read,
        int32_t *req_write, int32_t * req_write_memack);
extern void Get_NN_Sample_Performance(dms_res_item_t *res_item,
        int32_t *old_tp, int32_t *old_iops,
        int32_t *curr_tp, int32_t *curr_iops);
extern void Get_DN_Sample_Performance(dms_res_item_t *res_item,
        int32_t *old_read_tp, int32_t *old_read_iops,
        int32_t *old_write_tp, int32_t *old_write_iops,
        int32_t *curr_read_tp, int32_t *curr_read_iops,
        int32_t *curr_write_tp, int32_t *curr_write_iops);
extern void reset_all_res_perf_data(void);

extern void set_res_available(uint32_t ipaddr, uint32_t port,
        bool is_available);

extern int32_t get_res_avg_resp_time(dms_res_item_t *res_item);
extern int add_req_response_time(uint64_t ts_start, uint64_t ts_end, uint64_t ts_end_w_memack,
        res_type_t type, uint16_t sub_type,
        uint32_t ipaddr, uint32_t port, uint64_t num_of_bytes);


extern void add_request_timeout_record(uint32_t ipaddr, uint32_t port);
extern void dec_request_timeout_record(uint32_t ipaddr, uint32_t port);

extern int32_t Check_DN_Resources_Is_Available(uint32_t ipaddr, uint32_t port);
extern dms_res_item_t *add_NN_Resource(uint32_t ipaddr, uint32_t port);

extern void dec_res_rate_max_reqs(dms_res_item_t *res_item);
extern void update_res_rate_timeout(dms_res_item_t *res_item);

extern void update_resource_rate(dms_res_item_t *res_item,
        int32_t slow_start_rate, int32_t start_max_reqs,
        int32_t rate_age_time, int32_t curr_rate, int32_t curr_max_reqs);
extern void get_nn_res_rate(int32_t *slow_s_rate, int32_t *slow_s_max,
        int32_t *curr_rate, int32_t *curr_max,
        int32_t *alloc_rate, int32_t *alloc_max);
extern void update_all_resource_rate(uint64_t curr_time);
extern void show_resource_rate(dms_res_item_t *res_item);
extern void alloc_res_no_fc(dms_res_item_t *res_item,
        int32_t num_reqs_need);

extern int32_t should_trigger_fc(dms_res_item_t *res_item, int32_t need_nn_res,
        int64_t io_start_time);
extern void do_flow_control(int16_t STOP_TYPE, int32_t wait_time);


extern void init_flow_control(void);
extern void release_flow_control(void);

extern int32_t get_fc_resp_time(dms_fc_group_t *group, res_type_t type);
extern void release_fc_group(dms_fc_group_t *vol_fc_group);
extern dms_fc_group_t *create_fc_group(uint64_t volumeID);
extern int32_t add_res_to_fc_group_by_hostname(uint32_t ipaddr, uint32_t port,
        dms_fc_group_t *group, uint16_t type);

extern uint32_t show_res_info(int8_t *buffer, int32_t buff_len);
extern uint32_t get_dn_latency_info(int8_t *buffer);
extern uint32_t get_vol_active_dn(int8_t *buffer, dms_fc_group_t *group);
#endif


