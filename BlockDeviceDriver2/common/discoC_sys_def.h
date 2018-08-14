/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_sysdef.h
 *
 * This file includes lots of constant definition for disco client system.
 * I don't want put lots of definition in common_def.h and I decide to create
 * discoC_sysdef.h and put some constatn definition here.
 *
 * For example: block size in disco client, max number of requests ... etc
 *
 */

#ifndef DISCOC_SYS_DEF_H_
#define DISCOC_SYS_DEF_H_

#ifndef DMSC_USER_DAEMON
#include <linux/list.h>
#include <asm/atomic.h>
#endif

#define DN_PROTOCOL_VERSION 1     //current default CN-DN protocol
//#define DN_PROTOCOL_VERSION 2   //new protocol that support send volume info to datanode

#define CN_CRASH_RECOVERY   0     //not support CN crash recovery
//#define CN_CRASH_RECOVERY 1     //support CN crash recovery

//#define DISCO_ERC_SUPPORT       //support disco ERC

//#define DISCO_PREALLOC_SUPPORT  //support disco prealloc space

/***** Block size (including Linux kernel and DMS) define start *****/
#define MAX_SIZE_KERNEL_MEMCHUNK    128*1024  //max continuous physical space in kernel 128K
#define KERNEL_SECTOR_SIZE          512       //kernel sector size in bytes
#define MAX_NUM_KERNEL_RQS          256       //max number of RQs of linux generic queue

//NOTE 128 sectors, 64K..CN-DN protocol can support to 256K
#define MAX_SECTS_PER_RQ            256       //max number sectors in a RQ (128K)
#define MAX_SIZE_PER_RQ    MAX_SECTS_PER_RQ*KERNEL_SECTOR_SIZE
#if MAX_SECTS_PER_RQ > 256
#error We doesnot support this setting MAX_SECTOR over 256
#endif

#define MAX_SECTS_RQ_MERGE  MAX_SECTS_PER_RQ  //Max secotors can be merged for a clientnode io_request
#define MAX_NUM_RQS_MERGE   MAX_SECTS_PER_RQ  //Max RQs can be merged for a clientnode io_request, 1 rq has 1 sector
//#define MAX_NUM_RQS_MERGE   MAX_NUM_KERNEL_RQS
#define MAX_RQ_AGG_WAIT_CNT  2 //milli-sec    //how many times clientnode try to wait for aggregating more RQs into io_request

//how many chunks to hold user's write data.
//Due to alignment issue, max size of a RQ is 128K, but we need 128K + 4K buffer
#define MAX_NUM_MEM_CHUNK_BUFF (((MAX_SECTS_RQ_MERGE*KERNEL_SECTOR_SIZE) / (MAX_SIZE_KERNEL_MEMCHUNK)) + 1)

//how many volume can be attached in clientnode
#define DEFAULT_MAX_NUM_OF_VOLUMES 256

/*
 * Lego: Don't modify this setting, since xenvbd only support 512 bytes
 * sector size.
 */
#define KERNEL_LOGICAL_SECTOR_SIZE              KERNEL_SECTOR_SIZE
#if KERNEL_LOGICAL_SECTOR_SIZE != 512
#error We doesnot support this setting that KERNEL_LOGICAL_SECTOR_SIZE is \
       not equal to 512
#endif

#define DMS_LB_SIZE                             4096  //4K bytes
#define MAX_LB_PER_MEM_CHUNK    (MAX_SIZE_KERNEL_MEMCHUNK / DMS_LB_SIZE)
#define NUM_SECTS_PER_DMS_LB             (DMS_LB_SIZE/KERNEL_SECTOR_SIZE)
#define MAX_LBS_PER_RQ        (MAX_SECTS_PER_RQ / NUM_SECTS_PER_DMS_LB + 1)
#define MAX_LBS_PER_IOR       (MAX_SECTS_RQ_MERGE / NUM_SECTS_PER_DMS_LB + 1)
#define NUM_BYTES_MAX_LB        ((MAX_LBS_PER_IOR + 7) / 8)
#define MAX_LBS_PER_ALLOCMD_REQ (MAX_SECTS_PER_RQ / NUM_SECTS_PER_DMS_LB)

//following define the bit length of different define value for bit-operation
#define BIT_LEN_DMS_LB_SECTS               3  // DMS LB 4096 bytes = 8 sects
#define BIT_LEN_SECT_SIZE                  9  // *512 bytes = << 9
#define BIT_LEN_DMS_LB_SIZE               12  // *4096 byte = << 12
#define BIT_LEN_MEM_CHUNK                 17  // *128K byte = << 17
#define BIT_LEN_PER_1_MASK                8L
#define BIT_LEN_BYTE                       3
#define BYTE_MOD_NUM                      (8 - 1)
#define BIT_LEN_KBYTES                    10

//when a variable has value 2^n, the mod operation will be a % b => a & (2^n - 1)
#define LEN_MEM_CHUNK_MOD_NUM   (128*1024 - 1)

//max string length to store a ipv4 address in string format
#define IPV4ADDR_NM_LEN  16

//max string length to store namenode's name
#ifndef MAX_LEN_NNSRV_NM
#define MAX_LEN_NNSRV_NM    16
#endif

//size of fingerprint for a 4K data
#define FP_LENGTH 20
//how many bytes required to store fingerprint for all LBs in a io request
#define MAX_SIZE_FPMAP ((MAX_LBS_PER_IOR + BYTE_MOD_NUM) >> BIT_LEN_BYTE)

#define INVAL_REQID    0

//io request operation code
typedef enum {
    KERNEL_IO_READ,
    KERNEL_IO_WRITE
} iorw_dir_t;

//which mode kernel module to run for (deprecated now)
typedef enum {
    discoC_MODE_Normal, //default value, normal mode
    discoC_MODE_SelfTest, //run cn as self test mode that CN won't send requests to NN/DN
    discoC_MODE_Test_FakeNode, //run cn as self test mode that CN send request to faked NN/DN
    discoC_MODE_Test_NN_Only,  //run cn as test NN only, CN commit to user after receive response from namenode
    discoC_MODE_SelfTest_NN_Only, //run cn as selftest NN only, CN commit to user when has metadata. Won't send MD request to NN
} discoC_mode_t;

//io request priority, when priority is high, will add request to head of queue
//so that NN send request thread and DN send request thread will peek request with high priority
typedef enum {
    PRIORITY_HIGH = 0,
    PRIORITY_LOW
} priority_type;

//disco system max replica
#define MAX_NUM_REPLICA 3

//how many times clientnode will query namenode metadata - related to ERC
#define MAX_MDATA_VERSION   2

#define INVAL_RLO_TRIPLET   0xffffffff
#define INVAL_LBID          0xffffffff
#define INVAL_HBID          0xffffffff

#ifndef DMSC_USER_DAEMON
/*
 * In disco system, there are 3 namespace
 * 1. User namespace: which namenode (IP), which volume (volumeID),
 *    start LB (disco logical block), length in LB
 * 2. disco namenode name space: H bid
 * 3. physical name space: Rbid + offset + Len (Rbid = DN ID + Lun ID)
 */
typedef struct discoC_userIO_address {
    uint32_t metaSrv_ipaddr; //namenode's ip
    uint32_t metaSrv_port;   //namenode socket service port
    uint32_t volumeID;       //volume ID
    uint32_t lbid_len;       //number of lb user want R/W
    uint64_t lbid_start;     //start LBA user want R/W
} UserIO_addr_t;

/*
 * A common data structure for all requests in disco clientnode
 * io request, metadata request, payload request, datanode request .... etc
 */
typedef struct discoC_request_common {
    uint32_t ReqID;         //Request ID
    uint32_t CallerReqID;   //Caller Request ID, ie. which upper requests need callee request to complete the request handling
    uint32_t ReqFeature;    //Request feature
    priority_type ReqPrior; //Request priority
    uint16_t ReqOPCode;     //Request operation code
    atomic_t ReqRefCnt;     //Request reference count, how many threads is accessing this request
    uint64_t t_sendReq;     //Request send time: when this request be sent
    uint64_t t_rcvReqAck;   //Request ack receive time: when external component send ack back
} ReqCommon_t;
#endif

#endif /* DISCOC_SYS_DEF_H_ */
