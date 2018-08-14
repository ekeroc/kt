/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * dmsc_config_private.h
 *
 */

#ifndef _DMSC_CONFIG_PRIVATE_H_
#define _DMSC_CONFIG_PRIVATE_H_

#define MAX_Name_Len     64
#define MAX_Value_Len    64

#define MAX_NUM_CONFIG_ITEM 64
#define CUR_CONFIG_ITEM 39

#if CUR_CONFIG_ITEM > MAX_NUM_CONFIG_ITEM
#error Wrong Num of Configuration item
#endif

//TODO Remove CL.FC.Stop_Time_Interval when refactor Flow control
#define CL_FLOW_CONTROL             "CL.FlowControl"
#define CL_FLOW_CONTROL_DEF_VAL     1

#define CL_READ_LOAD_BALANCE        "CL.Read_Load_Balance"
#define CL_READ_LOAD_BALANCE_DEF_VAL        1

#define CL_READ_NUM_HB_NEXT_DN        "CL.Read_NumHB_NextDN"
#define CL_READ_NUM_HB_NEXT_DN_DEF_VAL        1024

#define CL_METADATA_CACHE           "CL.MetadataCache"
#define CL_METADATA_CACHE_DEF_VAL           1
#define CL_MAX_METADATA_CACHE_SIZE  "CL.MaxMetadataCacheSize"
#define CL_MAX_METADATA_CACHE_SIZE_DEF_VAL  33554432L  //32*1024*1024
#define CL_MAX_CAP_FOR_CACHE_ALL_HIT            "CL.MaxCapForCacheAllHit"
#define CL_MAX_CAP_FOR_CACHE_ALL_HIT_DEF_VAL            1073741824L

#define CL_MIN_DISK_ACK             "CL.MinDiskAck"
#define CL_MIN_DISK_ACK_DEF_VAL             1

#define CL_FP_CALCULATION           "CL.FPCalculation"
#define CL_FP_CALCULATION_DEF_VAL           0

#define CL_DEFAULT_ENABLE_SHARE_VOLUME "CL.ShareVolume"
#define CL_DEFAULT_ENABLE_SHARE_VOLUME_DEF_VAL 0

#define CL_SHOW_PERFORMANCE_DATA "CL.ShowPerformanceData"
#define CL_SHOW_PERFORMANCE_DATA_DEF_VAL 1

#define CL_DEFAULT_LOG_LEVEL    "CL.LogLevel"
#define CL_DEFAULT_LOG_LEVEL_VALUE  "INFO"

#define DMS_SOCKET_TIMEOUT  "Socket.timeoutMS"
#define DMS_SOCKET_TIMEOUT_DEF_VAL  600000

#define CL_SOCKET_HB_INTERVAL   "CL.SockHBInterval"
#define CNT_HB_FOR_1_TIMEOUT    2

#define CL_DEFAULT_DISK_READ_AHEAD  "CL.DISK_ReadAhead"
#define CL_DEFAULT_DISK_READ_AHEAD_DEF_VAL  8192

#define CL_DEF_NNSRV_NM "CL.MDS_NAME"
#define CL_DEF_NNSRV_NM_VAL "dms"

#define DMS_MDS_IP  "CL.MDS_IP"
#define DMS_MDS_IP_DEF_VAL  0

#define DMS_MDS_VO_PORT  "Port.VolumeOperation"
#define DMS_MDS_VO_PORT_DEF_VAL  1236

#define DMS_MDS_IO_PORT  "CL.MDS_IO_Port"
#define DMS_MDS_IO_PORT_DEF_VAL  1234

#define CL_IOREQ_TIMEOUT    "CL.IOReqTimeout"
#define CL_IOREQ_TIMEOUT_DEF_VAL    300

#define CL_IO_FLUSH_TIMEOUT    "CL.MaxIOFlushTimeout"
#define CL_IO_FLUSH_TIMEOUT_DEF_VAL    2*CL_IOREQ_TIMEOUT_DEF_VAL

#define DMS_RR_ENABLE   "Rereplication.enable"
#define DMS_RR_ENABLE_DEF_VAL   "true"

#define CL_WIO_HIGH_RESP   "CL.WIOHighResp"
#define CL_WIO_HIGH_RESP_DEF_VAL   1

#define CL_WIO_HIGH_RESP_WTIME   "CL.WIOHRespWaitTime"
#define CL_WIO_HIGH_RESP_WAITTIME_DEF_VAL   60000 //60 second

#define CL_WIO_DONE_IMMED_CONN_LOSTTIME   "CL.WIOGiveUp_CONN_LOSTIMTE"
#define CL_WIO_DONE_IMMED_CONN_LOSTTIME_DEF_VAL   600000 // 10 min

#define CL_NR_IOREQS   "CL.NR_IOReqs"
#define CL_NR_IOREQS_DEF_VAL   4000

#define CL_REQ_M_WAIT_INTVL    "CL.ReqMergeWaitIntvl"
#define CL_REQ_M_WAIT_INTVL_DEF_VAL    100

#define CL_REQ_M_WAIT_MAX_CNT    "CL.ReqMergeWaitMaxCnt"
#define CL_REQ_M_WAIT_MAX_CNT_DEF_VAL    40

#define CL_REQ_MERGE    "CL.ReqMergeEnable"
#define CL_REQ_MERGE_DEF_VAL    1

#define CL_REQ_ACK_PERF_WARN    "CL.ReqAckPerfWarn"
#define CL_REQ_ACK_PERF_WARN_DEF_VAL    60

#define CL_NNRATE_PERF_WARN    "CL.NNRateWarn"
#define CL_NNRATE_PERF_WARN_DEF_VAL    32

#define CL_NNMAXREQ_PERF_WARN    "CL.NNMaxReqWarn"
#define CL_NNMAXREQ_PERF_WARN_DEF_VAL    64

#define CL_IOR_PERF_WARN    "CL.IORPerfWarn"
#define CL_IOR_PERF_WARN_DEF_VAL    60  //seconds

#define CL_REQ_QDELAY_WARN    "CL.ReqQDelayWarn"
#define CL_REQ_QDELAY_WARN_DEF_VAL    60  //seconds

#define CL_ACTIVE_ClearLinuxCache   "CL.ClearLinuxCache"
#define CL_ACTIVE_ClearLinuxCache_DEF_VAL   1

#define CL_CLinuxCache_Period   "CL.CLinuxCachePeriod"
#define CL_CLinuxCache_Period_DEF_VAL   1000  //unit: milli-sec

#define CL_FreeMem_Cong "CL_FreeMemCong"
#define CL_FreeMem_Cong_DEF_VAL   256*1024  //unit: KBytes

#define CL_NNSimulator_on "CL.NNSimulator"
#define CL_NNSimulator_on_DEF_VAL   0

#define CL_DNSimulator_on "CL.DNSimulator"
#define CL_DNSimulator_on_DEF_VAL   0

#define CL_Prealloc_FlushIntvl    "CL.PreallocFlushInterval"
#define CL_Prealloc_FlushIntval_DEF_VAL 30

#define STR_DEBUG   "DEBUG"
#define STR_INFO    "INFO"
#define STR_WARN    "WARN"
#define STR_ERROR   "ERROR"
#define STR_EMERG   "EMERG"

#define STR_TRUE    "true"
#define STR_FALSE   "false"

typedef enum {
    cl_fc = 0,
    cl_read_bal,
    cl_read_hbs_next_dn,
    cl_mcache,
    cl_mcache_max_items,
    cl_mcache_max_allhit,
    cl_min_disk_ack,
    cl_fp,
    cl_share_vol,
    cl_show_perf,
    cl_log_level,
    cl_sk_timeout,
    cl_hb_interval,
    cl_disk_ra,
    cl_mds_vo_port,
    cl_mds_io_port,
    cl_req_timeout,
    cl_flush_timeout,
    cl_rr,
    cl_high_resp,
    cl_high_resp_wtime,
    cl_wIOdone_conn_lostt,
    cl_nr_ioreqs,
    cl_req_req_merge,
    cl_req_mw_intvl,
    cl_req_mw_maxcnt,
    cl_req_rsp_perf_warn,
    cl_nnrate_warn,
    cl_nnmaxreq_warn,
    cl_ior_perf_warn,
    cl_req_qdelay_warn,
    cl_mds_ip,
    cl_mds_name,
    cl_cLinux_cache, //feature: period clear linux page cache when memory is nervous
    cl_cLinux_cache_period,
    cl_free_mem_cong,
    cl_nnSimulator,
    cl_dnSimulator,
    cl_prealloc_flush_intvl,
    cl_max_config_item_end,
} config_item_index;

typedef enum {
    VAL_TYPE_BOOL = 0,
    VAL_TYPE_INT32,
    VAL_TYPE_INT64,
    VAL_TYPE_STR
} config_val_type_t;

typedef struct config_item {
    int8_t name[MAX_Name_Len];
    int8_t value_str[MAX_Value_Len];
    int64_t value;
    int32_t name_len;
    int32_t value_str_len;
    config_val_type_t val_type;
    void *val_ptr;
} config_item_t;

//TODO Remove CL.FC.Stop_Time_Interval when refactor Flow control
config_item_t config_list[MAX_NUM_CONFIG_ITEM];

#ifndef DMSC_USER_DAEMON
#define CL_EI_TEST  "CL.EITest"
#define CL_EI_TEST_ITEM "CL.EITestItem"
#define CL_EI_TEST_DEF_VAL false

struct configuration *dms_client_config;
#else

char *proc_config_file = "/proc/ccma/config";

#endif


#endif /* _DMSC_CONFIG_PRIVATE_H_ */
