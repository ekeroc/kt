/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * common.h
 *
 * Common header file. Shared between user space daemon & kernel module
 *
 */

#ifndef _COMMON_H_
#define _COMMON_H_

#define ChrDev_Major 151
#define ChrDev_Name "chrdev_dmsc"
#define BlkDev_Name_Prefix  "dms"

#define IOCTL_CONFIG_DRV        _IOW(ChrDev_Major,   1, unsigned long)
#define IOCTL_INIT_DRV          _IOW(ChrDev_Major,   2, unsigned long)

#define IOCTL_ATTACH_VOL        _IOW(ChrDev_Major,   6, unsigned long)
#define IOCTL_ATTACH_VOL_FINISH _IOW(ChrDev_Major,   7, unsigned long)
#define IOCTL_DETATCH_VOL       _IOW(ChrDev_Major,   8, unsigned long)
#define IOCTL_RESETOVERWRITTEN  _IOW(ChrDev_Major,   9, unsigned long)
#define IOCTL_INVALIDCACHE      _IOWR(ChrDev_Major, 10, unsigned long)
#define IOCTL_BLOCK_VOLUME_IO   _IOW(ChrDev_Major,  11, unsigned long)
#define IOCTL_UNBLOCK_VOLUME_IO _IOW(ChrDev_Major,  12, unsigned long)
#define IOCTL_SET_VOLUME_SHARE_FEATURE _IOW(ChrDev_Major, 13, unsigned long)
#define IOCTL_DISABLE_VOL_RW    _IOW(ChrDev_Major,  14, unsigned long)

#define IOCTL_STOP_VOL          _IOW(ChrDev_Major,  15, unsigned long)
#define IOCTL_RESTART_VOL       _IOW(ChrDev_Major,  16, unsigned long)
#define IOCTL_RESET_DRIVER _IOR(ChrDev_Major, 41, unsigned long)
#define IOCTL_GET_CN_INFO   _IOWR(ChrDev_Major,  18, unsigned long)

#define IOCTL_INVAL_VOL_FS  _IOW(ChrDev_Major, 19, unsigned long)
#define IOCTL_CLEAN_VOL_FS  _IOW(ChrDev_Major, 20, unsigned long)
#define IOCTL_FLUSH_VOL_FS  _IOW(ChrDev_Major, 21, unsigned long)
#define IOCTL_PREALLOC_CMD  _IOW(ChrDev_Major, 22, unsigned long)

#define INVALIDATE_CACHE_ACK_OK 0
#define INVALIDATE_CACHE_ACK_NOVOLUME 1
#define INVALIDATE_CACHE_ACK_ERR -1

#define ATTACH_FAIL_FOR_USER  -1
#define DETACH_FAIL_FOR_USER  -1

#define SUBOP_INVALIDATECACHE_BYLBIDS	(0)
#define SUBOP_INVALIDATECACHE_BYVOLID	(1)
#define SUBOP_INVALIDATECACHE_BYDNNAME	(2)
#define SUBOP_INVALIDATECACHE_BYHBIDS   (3)

typedef enum {
    SUBOP_PREALLOC_Dummy,
    SUBOP_PREALLOC_AllocFreeSpace,
    SUBOP_PREALLOC_FlushMetadata,
    SUBOP_PREALLOC_ReturnFreeSpace,
} userD_prealloc_subop_t;

#define RESET_DRIVER_OK	0
#define RESET_DRIVER_FAIL	1
#define RESET_DRIVER_BUSY	1000
#define DRIVER_ISNOT_WORKING	1001

#define MAX_LEN_NNSRV_NM    16

typedef enum {
    Attach_Vol_OK = 0,           //attach success
    Attach_Vol_Exist,            //volume already be attached
    Attach_Vol_Exist_Attaching,  //volume is attaching
    Attach_Vol_Exist_Detaching,  //volume is detaching
    Attach_Vol_Max_Volumes,      //exceed max supported volume
    Attach_Vol_Fail,             //other fail
} attach_response_t;

typedef enum {
    Detach_Vol_OK = 0,           //detach success
    Detach_Vol_NotExist,         //volume not exist
    Detach_Vol_Exist_Attaching,  //volume is attaching
    Detach_Vol_Exist_Detaching,  //volume is detaching
    Detach_Vol_Exist_Busy,       //volume is busy (someone open or io not finish)
    Detach_Vol_Fail,             //other fail
} detach_response_t;

/** when volume is at stop state, clientnode will commit error for each IO requests **/
typedef enum {
    Stop_Vol_OK = 0,             //stop volume success
    Stop_Vol_NotExist,           //volume not exist
    Stop_Vol_Exist_Attaching,    //volume is attaching
    Stop_Vol_Exist_Detaching,    //volume is detaching
    Stop_Vol_Fail,               //other fail
} stop_response_t;

/** restart volume will let clientnode handle volume IO in normal way **/
typedef enum {
    Restart_Vol_OK = 0,            //restart volume success
    Restart_Vol_NotExist,          //volume not exist
    Restart_Vol_Exist_Attaching,   //volume is attaching
    Restart_Vol_Exist_Detaching,   //volume is detaching
    Restart_Vol_Fail,              //other fail
} restart_response_t;

/** When clientnode got block volume IO from namenode,
 * clientnode will stop processing IO. User's IO will hang.
 **/
typedef enum {
    Block_IO_OK = 0,         //block volume IO
    Block_IO_Vol_NotExist,   //volume not exist
    Block_IO_Vol_Attaching,  //volume is attaching
    Block_IO_Vol_Detaching,  //volume is detaching
    Block_IO_Fail,           //other fail
} block_io_resp_t;

/** When clientnode got unblock volume IO from namenode,
 * clientnode will start to proce IO.
 **/
typedef enum {
    UnBlock_IO_OK = 0,         //unblock volume IO success
    UnBlock_IO_Vol_NotExist,   //volume not exist
    UnBlock_IO_Fail,           //other fail
} unblock_io_resp_t;

/** set volume feature (whether share ... etc)
 **/
typedef enum {
    Set_Fea_OK = 0,      //set volume feature success
    Set_Fea_Vol_NoExist, //volume not exist
    Set_Fea_Fail,        //other fail
} set_fea_resp_t;

typedef enum {
    Inval_chunk_OK = 0,
    Inval_chunk_Vol_NoExist,
    Inval_chunk_NoExist,
    Inval_chunk_FAIL,
} inval_chunk_resp_t;

typedef enum {
    Clean_chunk_OK = 0,
    Clean_chunk_Vol_NoExist,
    Clean_chunk_NoExist,
    Clean_chunk_FAIL,
} clean_chunk_resp_t;

typedef enum {
    Flush_chunk_OK = 0,
    Flush_chunk_Vol_NoExist,
    Flush_chunk_FAIL,
} flush_chunk_resp_t;

typedef enum {
    Prealloc_cmd_OK = 0,
    Prealloc_cmd_Vol_NoExist,
    Prealloc_cmd_FAIL,
} prealloc_cmd_resp_t;

/*
 * set all bit in overwritten bitmap as 0. received from namenode
 */
typedef enum {
    Clear_OVW_OK = 0,   //clear volume ovw success
    Clear_Vol_NoExist,  //volume not exist
    Clear_OVW_Fail,     //other fail
} clear_ovw_resp_t;

/*
 * User daemon notify kernel driver previous stop is normal shutdown or crash
 * For clientnode crash recovery feature
 */
typedef enum {
    PERV_SHUTDOWN_NORMAL,
    PREV_SHUTDOWN_ABNORMAL
} prev_stop_state_t;

struct dms_volume_info {
    int8_t nnSrv_nm[MAX_LEN_NNSRV_NM];  //namenode's name (reserve for future that 1 CN connect multiple NN
    uint32_t nnSrv_IPV4addr;            //namenode's ipaddr
    uint32_t volumeID; //should unify as 32 bits //volume ID
    uint64_t capacity;                  //volume's capacity (in bytes)
    uint32_t replica_factor;            //volume's number of replica
    int16_t share_volume;               //whether volume is a shared volume
};

typedef struct dms_userD_prealloc_ioctl_cmd {
    uint16_t opcode;
    uint16_t subopcode;
    uint32_t requestID;
    uint64_t volumeID;
    uint64_t chunkID;
    uint64_t max_unuse_hbid;
} userD_prealloc_ioctl_cmd_t;

struct ResetCacheInfo {
    int32_t SubOpCode;
    union {
        struct {  //required when reset volume's metadata cache by LBIDs
            uint64_t VolumeID;  //should unify as 32 bits
            uint32_t NumOfLBID;
            uint64_t *LBID_List;
        } LBIDs;
        struct {  //required when reset volume's metadata cache by volume ID
            uint64_t VolumeID; //should unify as 32 bits
        } VolID;
        struct {  //required when reset all metadata cache by datanode ipaddr
            uint32_t Port;
            uint32_t NameLen;
            int8_t *Name;
        } DNName;
        struct {  //required when reset volume's metadata cache by HBIDs
            uint32_t NumOfHBID;
            uint64_t *HBID_List;
        } HBIDs;
    } u;
};

#define citem_len 128

typedef struct config_para {
    uint32_t item_len;      //configure value string length from user space to kernel
    int8_t item[citem_len]; //configure value in string format from user space to kernel
} config_para_t;

typedef struct register_para {
    uint32_t prev_shutdown_stat; //previous shutdown state from user space to kernel
} reg_para_t;

typedef struct drv_stat_para {
    uint32_t drv_stat;
} drv_stat_para_t;

typedef enum {
    CN_INFO_SUBOP_GetAllCNDNLatencies, //subopcode to get CNDN latency
    CN_INFO_SUBOP_GetAllCNDNQLen,      //subopcode to get CN-DN request queue length in CN
    CN_INFO_SUBOP_GETVolActiveDN,      //subopcode to get CN access which datanode that store volume's data
    CN_INFO_SUBOP_GETAllVols,          //subopcode to get all attached volume
} cn_info_subop_t;

#endif	/* _COMMON_H_ */
