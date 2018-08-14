/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * metadata_manager.h
 *
 */

#ifndef METADATA_MANAGER_H_
#define METADATA_MANAGER_H_

typedef enum {
    MData_VALID = 0,  //metadata status return by nn
    MData_INVALID,    //metadata status return by nn
    MData_VALID_LESS_REPLICA, //when NN return MData_VALID but num of replica in MD < volume replica and RR enable
} MData_state_t;

#ifdef DISCO_ERC_SUPPORT
typedef enum {
    MDPROT_REP = 1,
    MDPROT_ERC,
    MDPROT_NOMD,
} mdata_prot_t;
#endif

typedef enum {
    UData_S_INIT,         //UData: user data status in metadata
    UData_S_READ_OK,
    UData_S_WRITE_OK,
    UData_S_WRITE_MEM_OK,
    UData_S_DISK_FAILURE,
    UData_S_MD_ERR,       //HBID of user data on datanode mismatch with metadata on namenode
    UData_S_READ_NA,      //fail to read user data due to timeout or network N/A
    UData_S_WRITE_FAIL,   //fail to write user data due to timeout or netwrok N/A
    UData_S_OPERATION_ERR,
} UData_state_t;

typedef struct discoC_MetaData_DNLoc {
    uint32_t ipaddr;
    uint32_t port;
    uint64_t rbid_len_offset[MAX_LBS_PER_IOR];
#ifdef DISCO_ERC_SUPPORT
    uint64_t ERC_HBID;
#endif
    uint16_t num_phyLocs;
    UData_state_t udata_status;
    rwlock_t udata_slock;
} metaD_DNLoc_t;

typedef enum {
    MDATA_FEA_UDataCIUser,
    MDATA_FEA_UDataCIMDS,
    MDATA_FEA_TRUE_MD, //means this metadata comes from nn's HO map
    MDATA_FEA_ERC,
    MDATA_FEA_PREALLOC,
} MData_fea_t;

typedef struct discoC_metadata_item
{
    uint16_t num_dnLoc;
    uint32_t num_hbids;
    uint64_t arr_HBID[MAX_LBS_PER_IOR];
    metaD_DNLoc_t udata_dnLoc[MAX_NUM_REPLICA];
#ifdef DISCO_ERC_SUPPORT
    metaD_DNLoc_t *erc_dnLoc;
#endif
    uint16_t mdata_stat;
    uint64_t lbid_s;

    int8_t memack;
    int8_t diskack;
    int8_t diskErr_ack;
    int8_t wfail_connErr;
    int16_t num_replica; //merge with num_dnLoc
    int16_t num_volReplica;
    rwlock_t mdata_slock;
    uint32_t mdata_fea;

#ifdef DISCO_ERC_SUPPORT
    uint16_t blk_md_type;
    uint8_t *erc_buffer;
    struct mutex erc_buff_lock;
    uint16_t erc_cnt_dnresp;
#endif

#ifdef DISCO_PREALLOC_SUPPORT
    uint64_t space_chunkID;
#endif
} metaD_item_t;

typedef struct discoC_metadata
{
    metaD_item_t **array_metadata;
    UserIO_addr_t usr_ioaddr;
    uint16_t num_MD_items;
    uint16_t num_invalMD_items;
} metadata_t;

typedef struct meta_cache_dn_loc {
    uint32_t ipaddr;
    uint32_t port;
    uint32_t rbid;
    uint32_t offset;
} __attribute__((__packed__)) mcache_dnloc_t;

typedef struct meta_cache_item {
    //unsigned long long LBID;
    uint64_t HBID;
    mcache_dnloc_t udata_dnLoc[MAX_NUM_REPLICA]; //datanode location information of user data
    uint16_t num_dnLoc;
} __attribute__((__packed__)) mcache_item_t;

struct Read_DN_selector {
    uint16_t readDN_idx;     //current readDN index in metadata item
    uint32_t readDN_numLB;   //how many LBs already read from this DN
    spinlock_t readDN_lock;  //lock to make sure readDN_idx and readDN_numLB in 1 atomic operation
};

extern void MData_set_ERCProt(metaD_item_t *md_item);
extern void MData_set_TrueMD(metaD_item_t *md_item);
extern bool MData_chk_ERCProt(metaD_item_t *md_item);
extern bool MData_chk_TrueMD(metaD_item_t *md_item);
extern bool MData_chk_ciUser(metaD_item_t *mdItem);
extern void MData_set_ciUser(metaD_item_t *mdItem);

extern bool MData_writeAck_commitResult(metaD_item_t *mdItem, bool *ciUser_ret, int16_t udstatus, bool *ciUser);
extern bool MData_timeout_commitResult(metaD_item_t *mdItem, bool *ciUser_ret, bool *ciUser);
extern bool MData_check_ciUserStatus(metaD_item_t *mdItem, bool *ciUser_ret);

extern void MData_init_DNLoc(metaD_DNLoc_t *dnLoc, uint32_t ipaddr, uint32_t port, uint16_t num_rlo);
extern void MData_update_DNUDataStat(metaD_DNLoc_t *dnLoc, UData_state_t udata_status);
extern bool MData_chk_UDStatus_writefail(metaD_DNLoc_t *dnLoc);
extern bool MData_chk_UDStatus_readfail(metaD_DNLoc_t *dnLoc);
extern int32_t MData_get_num_diskAck(metaD_item_t *mdItem);
extern void MData_wRetry_reset_DNLoc(metaD_item_t *mdItem, int32_t dnLoc_idx);
extern metaD_item_t *MData_alloc_MDItem(void);
extern void MData_free_MDItem(metaD_item_t *mdItem);
extern void MData_init_MDItem(metaD_item_t *md_item, uint64_t lbid_s, uint32_t num_hbids,
        uint16_t num_replica, uint16_t md_stat, bool isERC, bool isTrue, bool isPrealloc);

extern void init_metadata(metadata_t *my_metaD, UserIO_addr_t *ioaddr);
extern void free_metadata(metadata_t *my_metaD);
extern void update_metadata_UDataStat(metadata_t *my_metaD, UData_state_t udata_status);

extern void MDMgr_get_volume_cacheSize(uint64_t volid, uint32_t *cache_size, uint32_t *curr_cache_items);
extern int32_t MDMgr_init_manager(void);
extern int32_t MDMgr_rel_manager(void);

extern void MData_show_MDItem(metaD_item_t *md_item);
extern void MData_show_metadata(metadata_t *mdata, int32_t num_md);
#endif /* METADATA_MANAGER_H_ */
