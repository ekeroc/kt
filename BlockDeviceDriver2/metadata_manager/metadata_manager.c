/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * metadata_manager.c
 *
 */
#include <linux/version.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/blkdev.h>
#include "../common/discoC_sys_def.h"
#include "../common/dms_kernel_version.h"
#include "../common/common_util.h"
#include "../common/discoC_mem_manager.h"
#include "../common/sys_mem_monitor.h"
#include "metadata_manager.h"
#include "metadata_manager_private.h"
#include "../common/cache.h"
#include "metadata_cache.h"
#include "../config/dmsc_config.h"
#include "../connection_manager/conn_manager_export.h"
#include "../discoNN_client/discoC_NNC_Manager_api.h"

/*
 * _chk_ciUser_criteria: check whether meet criteria to commit to user for write IO
 * @mdItem: target metadata item to check
 * @ciUser_ret: IO result to commit to user
 *
 * return: true: caller can commit to user
 *         false: caller cannot commit to user
 */
static bool _chk_ciUser_criteria (metaD_item_t *mdItem, bool *ciUser_ret,
        int32_t minDiskAck)
{
    bool ret;

    ret = false;
    *ciUser_ret = false;

    do {
        if (minDiskAck == 0) {
            if (mdItem->diskack > 0 || mdItem->memack == mdItem->num_replica) {
                ret = true;
                *ciUser_ret = true;
                break;
            }
        }

        if (mdItem->diskack >= minDiskAck ||
                mdItem->diskack >= mdItem->num_replica) {
            ret = true;
            *ciUser_ret = true;
            break;
        }

        if (mdItem->num_replica == mdItem->diskErr_ack) {
            //NOTE: All DN disk IO error, ci to user with false
            //if mdItem->num_dnLoc < vol_replica no need retry since RR will fail eventually
            ret = true;
            break;
        }

        if ((mdItem->diskack + mdItem->diskErr_ack) == mdItem->num_replica) {
            //diskack > 0 but < CommitUser_min_DiskAck and other replica is disk error
            ret = true;
            *ciUser_ret = true;
            break;
        }

    } while (0);

    return ret;
}

/*
 * _chk_ciMDS_criteria: check whether meet criteria to commit to NN for write IO
 * @mdItem: target metadata item to check
 *
 * return: true: caller can commit to NN
 *         false: caller cannot commit to NN
 */
static bool _chk_ciMDS_criteria (metaD_item_t *mdItem)
{
    bool ciMDS;

    ciMDS = false;

    do {
        if (mdItem->diskack == mdItem->num_dnLoc) {
            //all replica got disk ack
            ciMDS = true;
            break;
        }

        if (mdItem->diskack >= 1) {
            if ((mdItem->diskack + mdItem->wfail_connErr) == mdItem->num_dnLoc) {
                //some replica conn N/A before send, commit MDS in early phase
                ciMDS = true;
                break;
            }

            if ((mdItem->diskack + mdItem->diskErr_ack + mdItem->wfail_connErr)
                    == mdItem->num_dnLoc) {
                ciMDS = true;
                break;
            }
        }

        if ((mdItem->diskack + mdItem->diskErr_ack) == mdItem->num_dnLoc) {
            ciMDS = true;
            break;
        }

        if (mdItem->wfail_connErr == mdItem->num_dnLoc) {
            //All connection N/A, keep waiting
            break;
        }
    } while (0);

    return ciMDS;
}

/*
 * MData_writeAck_commitResult: check commit result when got a write IO ack
 * @mdItem: target metadata item to check
 * @ciUser_ret: return value, IO result when commit to user
 * @udstatus: current write ack
 * @ciUser: return value, whether caller should commit to user
 *
 * return: true: caller can commit to NN
 *         false: caller cannot commit to NN
 */
bool MData_writeAck_commitResult (metaD_item_t *mdItem, bool *ciUser_ret,
        int16_t udstatus, bool *ciUser)
{
    bool ciMDS;

    ciMDS = false;
    *ciUser = false;

    write_lock(&mdItem->mdata_slock);

    switch (udstatus) {
    case UData_S_WRITE_MEM_OK:
        mdItem->memack++;
        break;
    case UData_S_WRITE_OK:
        mdItem->diskack++;
        break;
    case UData_S_DISK_FAILURE:
        mdItem->diskErr_ack++;
        break;
    case UData_S_WRITE_FAIL:
        mdItem->wfail_connErr++;
        break;
    default:
        dms_printk(LOG_LVL_WARN, "DMSC WARN unknow udstatus %d\n", udstatus);
        break;
    }

    *ciUser = _chk_ciUser_criteria(mdItem, ciUser_ret,
            dms_client_config->CommitUser_min_DiskAck);

    if (*ciUser) {
        if (test_and_set_bit(MDATA_FEA_UDataCIUser,
                (volatile void *)&(mdItem->mdata_fea)) == false) {
            *ciUser = true;
        } else {
            *ciUser = false;
        }
    }

    ciMDS = _chk_ciMDS_criteria(mdItem);
    if (ciMDS) {
        if (test_and_set_bit(MDATA_FEA_UDataCIMDS,
                (volatile void *)&(mdItem->mdata_fea)) == false) {
            ciMDS = true;

            //NOTE: avoid ci mds but no ci to user
            //      if already ci user, *ciUser will keep same value
            if (test_and_set_bit(MDATA_FEA_UDataCIUser,
                    (volatile void *)&(mdItem->mdata_fea)) == false) {
                *ciUser = true;
            }
        } else {
            ciMDS = false;
        }
    }

    write_unlock(&mdItem->mdata_slock);

    return ciMDS;
}

/*
 * _DNRAck_noData: check whether has other DN worth to wait read ack
 * @mdItem: target metadata item to check
 * @ciUser_ret: return value, IO result when commit to user
 * @num_MDErr: number of replica hbid error (hbid not match)
 * @num_DiskErr: number of replica disk error
 * @num_DNNA: number DN connection error
 *
 * return: true: caller should keep wait DN response
 *         false: caller should giveup waiting
 */
static bool _DNReadAck_check_wait (metaD_item_t *mdItem, bool *ciUser_ret,
        int32_t num_MDErr, int32_t num_DiskErr, int32_t num_DNNA)
{
    bool keep_wait;

    keep_wait = true;
    *ciUser_ret = false;

    do {
        //All replica hbid error
        if (num_MDErr == mdItem->num_dnLoc) {
            *ciUser_ret = true;
            keep_wait = false;
            break;
        }

        //All replica disk error
        if (num_DiskErr == mdItem->num_dnLoc) {
            *ciUser_ret = false;
            keep_wait = false;
            break;
        }

        //All replica not yet response disk ack
        if (num_DNNA == mdItem->num_dnLoc) {
            *ciUser_ret = false;
            keep_wait = true;
            break;
        }

        if ((num_MDErr + num_DiskErr) == mdItem->num_dnLoc) {
            *ciUser_ret = true;
            keep_wait = false;
            break;
        }

        if ((num_MDErr + num_DNNA) == mdItem->num_dnLoc) {
            *ciUser_ret = false;
            keep_wait = true;
            break;
        }

        if ((num_DiskErr + num_DNNA) == mdItem->num_dnLoc) {
            *ciUser_ret = false;
            keep_wait = true;
            break;
        }

        if ((num_MDErr + num_DiskErr + num_DNNA) == mdItem->num_dnLoc) {
            *ciUser_ret = false;
            keep_wait = true;
            break;
        }

    } while (0);

    return keep_wait;
}

/*
 * MData_check_ciUserStatus: check whether has other DN worth to wait read ack
 * @mdItem: target metadata item to check
 * @ciUser_ret: return value, IO result when commit to user
 *
 * return: true: caller should keep wait DN response
 *         false: caller should give up waiting
 */
bool MData_check_ciUserStatus (metaD_item_t *mdItem, bool *ciUser_ret)
{
    metaD_DNLoc_t *dnLoc;
    UData_state_t ud_stat;
    int32_t i, num_MDErr, num_DiskErr, num_DNNA;
    bool keep_wait, no_data;

    keep_wait = true;
    no_data = true;
    num_MDErr = 0;
    num_DiskErr = 0;
    num_DNNA = 0;

    read_lock(&mdItem->mdata_slock);

    for (i = 0; i < mdItem->num_dnLoc; i++) {
        do {
#ifdef DISCO_ERC_SUPPORT
            if (MData_chk_ERCProt(mdItem)) {
                dnLoc = &mdItem->erc_dnLoc[i];
                break;
            }
#endif
            dnLoc = &mdItem->udata_dnLoc[i];
        } while (0);
        read_lock(&dnLoc->udata_slock);
        ud_stat = dnLoc->udata_status;
        read_unlock(&dnLoc->udata_slock);

        if (ud_stat == UData_S_READ_OK) {
            *ciUser_ret = true;
            keep_wait = false;
            no_data = false;
            break;
        }

        if (ud_stat == UData_S_MD_ERR) {
            num_MDErr++;
            continue;
        }

        if (ud_stat == UData_S_DISK_FAILURE) {
            num_DiskErr++;
            continue;
        }

        if (ud_stat == UData_S_READ_NA) {
            num_DNNA++;
            continue;
        }
    }

    read_unlock(&mdItem->mdata_slock);

    if (no_data) {
        keep_wait = _DNReadAck_check_wait(mdItem, ciUser_ret,
                num_MDErr, num_DiskErr, num_DNNA);
    }

    return keep_wait;
}

/*
 * MData_timeout_commitResult: check write IO result when payload request timeout
 * @mdItem: target metadata item to check
 * @ciUser_ret: return value, IO result when commit to user
 * @ciUser: return value, caller should commit to user
 *
 * return: true: caller should commit to NN
 *         false: caller no need commit to NN
 */
bool MData_timeout_commitResult (metaD_item_t *mdItem, bool *ciUser_ret, bool *ciUser)
{
    bool ciMDS;

    ciMDS = false;
    *ciUser = false;

    write_lock(&mdItem->mdata_slock);

    if (mdItem->diskack > 0) {
        *ciUser_ret = true;
    } else {
        *ciUser_ret = false;
    }

    if (test_and_set_bit(MDATA_FEA_UDataCIUser,
            (volatile void *)&(mdItem->mdata_fea)) == false) {
        *ciUser = true;
    }

    if (test_and_set_bit(MDATA_FEA_UDataCIMDS,
            (volatile void *)&(mdItem->mdata_fea)) == false) {
        ciMDS = true;
    }

    write_unlock(&mdItem->mdata_slock);

    return ciMDS;
}

#if 0
/*
 * TODO: volume share ci status
 */
bool get_UData_write_status (metadata_t *myMData)
{
    metaD_item_t *md_item, *error_one;
    int32_t i, ret;
    bool result = true;

    error_one = NULL;
    for (i = 0; i < myMData->num_MD_items; i++) {
        md_item = myMData->array_metadata[i];
        ret = 0;

        if (ret == 0) {
            result = false;
            error_one = md_item;
            break;
        }

        result = result & ret;
    }

    if (!IS_ERR_OR_NULL(error_one)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WANR memack %d diskack %d replica %d\n",
                error_one->memack, error_one->diskack, error_one->num_replica);
    }

    return result;
}
#endif

/*
 * MData_init_DNLoc: initialize data member of metaD_DNLoc_t
 * @dnLoc  : target DN location to init
 * @ipaddr : ip address of DN
 * @port   : port number of DN
 * @num_rlo: how many physical location (rbid-length-offset) in dnLoc
 *
 * NOTE: caller guarantee dnLoc won't be NULL
 */
void MData_init_DNLoc (metaD_DNLoc_t *dnLoc, uint32_t ipaddr, uint32_t port,
        uint16_t num_rlo)
{
    dnLoc->ipaddr = ipaddr;
    dnLoc->port = port;
    dnLoc->num_phyLocs = num_rlo;
    dnLoc->udata_status = UData_S_INIT;
    rwlock_init(&dnLoc->udata_slock);
}

/*
 * MData_update_DNUDataStat: Update user data status in DN location of metadata
 * @dnLoc       : DN location to be updated
 * @udata_status: new status of DN location
 */
void MData_update_DNUDataStat (metaD_DNLoc_t *dnLoc, UData_state_t udata_status)
{
    write_lock(&dnLoc->udata_slock);
    dnLoc->udata_status = udata_status;
    write_unlock(&dnLoc->udata_slock);
}

/*
 * MData_wRetry_reset_DNLoc: when CN-DN connection back and we want to retry write DN request,
 *                           we should restore number of mem ack, disk ack ... etc
 * @mdItem       : target metadata item to check
 * @dnLoc_idx    : which replica in mditem
 */
void MData_wRetry_reset_DNLoc (metaD_item_t *mdItem, int32_t dnLoc_idx)
{
    metaD_DNLoc_t *dnLoc;

    write_lock(&mdItem->mdata_slock);

    do {
#ifdef DISCO_ERC_SUPPORT
        if (MData_chk_ERCProt(mdItem)) {
            dnLoc = &(mdItem->erc_dnLoc[dnLoc_idx]);
            break;
        }
#endif
        dnLoc = &(mdItem->udata_dnLoc[dnLoc_idx]);
    } while (0);

    write_lock(&dnLoc->udata_slock);

    switch (dnLoc->udata_status) {
    case UData_S_WRITE_MEM_OK:
        mdItem->memack--;
        dnLoc->udata_status = UData_S_INIT;
        break;
    case UData_S_WRITE_FAIL:
        mdItem->wfail_connErr--;
        dnLoc->udata_status = UData_S_INIT;
        break;
    default:
        break;
    }
    write_unlock(&dnLoc->udata_slock);

    write_unlock(&mdItem->mdata_slock);
}

/*
 * MData_chk_UDStatus_writefail: check whether write data to target DN loc success
 * @dnLoc : DN location to be wrote
 *
 * Return : true  write fail, caller should report to namenode
 *          false write success
 */
bool MData_chk_UDStatus_writefail (metaD_DNLoc_t *dnLoc)
{
    bool ret;

    ret = true;
    read_lock(&dnLoc->udata_slock);
    if (UData_S_WRITE_OK == dnLoc->udata_status) {
        ret = false;
    }
    read_unlock(&dnLoc->udata_slock);

    return ret;
}

/*
 * MData_chk_UDStatus_readfail: check whether location is failure
 * @dnLoc : DN location to be read
 *
 * Return : true  location has some problems, caller should report to namenode
 *          false read success
 */
bool MData_chk_UDStatus_readfail (metaD_DNLoc_t *dnLoc)
{
    bool ret;

    ret = false;

    read_lock(&dnLoc->udata_slock);
    if (UData_S_DISK_FAILURE == dnLoc->udata_status) {
        ret = true;
    }
    read_unlock(&dnLoc->udata_slock);

    return ret;
}

/*
 * MData_get_num_diskAck: get number of disk in metadata item
 * @mdItem : target mdItem to get value
 *
 * Return : numer of disk ack
 */
int32_t MData_get_num_diskAck (metaD_item_t *mdItem)
{
    int32_t num_diskack;

    num_diskack = 0;
    read_lock(&mdItem->mdata_slock);
    num_diskack = mdItem->diskack;
    read_unlock(&mdItem->mdata_slock);

    return num_diskack;
}

/*
 * MData_alloc_MDItem: allocate a metaD_item_t from memory pool
 *
 * Return: return metaD_item_t
 * NOTE: we don't check whether NULL here for preventing caller check again
 */
metaD_item_t *MData_alloc_MDItem (void)
{
    return (metaD_item_t *)discoC_malloc_pool(mdata_item_mpoolID);
}

/*
 * MData_free_MDItem: return space to memory pool
 * @mdItem: metadata item to be free
 *
 * NOTE: caller guarantee mdItem won't NULL
 */
void MData_free_MDItem (metaD_item_t *mdItem)
{
#ifdef DISCO_ERC_SUPPORT
    if (MData_chk_ERCProt(mdItem)) {
        if (!IS_ERR_OR_NULL(mdItem->erc_dnLoc)) {
            discoC_mem_free(mdItem->erc_dnLoc);
        }

        if (!IS_ERR_OR_NULL(mdItem->erc_buffer)) {
            discoC_mem_free(mdItem->erc_buffer);
        }
    }
#endif
    memset(mdItem, 0, sizeof(metaD_item_t));
    discoC_free_pool(mdata_item_mpoolID, mdItem);
}

void MData_set_ERCProt (metaD_item_t *md_item)
{
    set_bit(MDATA_FEA_ERC, (volatile void *)&(md_item->mdata_fea));
}

void MData_set_TrueMD (metaD_item_t *md_item)
{
    set_bit(MDATA_FEA_TRUE_MD, (volatile void *)&(md_item->mdata_fea));
}

bool MData_chk_ERCProt (metaD_item_t *md_item)
{
    return test_bit(MDATA_FEA_ERC, (const volatile void *)&(md_item->mdata_fea));
}

bool MData_chk_TrueMD (metaD_item_t *md_item)
{
    return test_bit(MDATA_FEA_TRUE_MD, (const volatile void *)&(md_item->mdata_fea));
}

bool MData_chk_ciUser (metaD_item_t *mdItem)
{
    bool ret;

    read_lock(&mdItem->mdata_slock);

    ret = test_bit(MDATA_FEA_UDataCIUser,
            (const volatile void *)&(mdItem->mdata_fea));

    read_unlock(&mdItem->mdata_slock);

    return ret;
}

void MData_set_ciUser (metaD_item_t *mdItem)
{
    write_lock(&mdItem->mdata_slock);

    set_bit(MDATA_FEA_UDataCIUser, (volatile void *)&(mdItem->mdata_fea));

    write_unlock(&mdItem->mdata_slock);
}

/*
 * MData_init_MDItem: fill member of metaD_item_t but no HBID array and datanode information
 * @md_item    : metadata item to be filled (caller guarantee won't NULL)
 * @lbid_s     : start LBID for this metadata item
 * @num_hbids  : how many hbids in this metadata item
 * @num_replica: how many replica in this metadata item
 * @md_stat    : metadata state (MData_state_t)
 * @isERC      : 0 means REP protection 1 means ERC protection
 * @isTrue     : 0 means MD from NN L2O map, 1 means MD from NN H2O map
 * @isPrealloc : 0 means MD from metadata cache or NN, 1 means MD from space manager of CN
 */
void MData_init_MDItem (metaD_item_t *md_item, uint64_t lbid_s, uint32_t num_hbids,
        uint16_t num_replica, uint16_t md_stat, bool isERC, bool isTrue, bool isPrealloc)
{
    md_item->lbid_s = lbid_s;
    md_item->num_hbids = num_hbids;
    md_item->num_dnLoc = num_replica;
    md_item->mdata_stat = md_stat;

    md_item->memack = 0;
    md_item->diskack = 0;
    md_item->diskErr_ack = 0;
    md_item->wfail_connErr = 0;
    md_item->num_replica = num_replica;
    md_item->mdata_fea = 0;
    rwlock_init(&md_item->mdata_slock);

    if (isERC) {
        MData_set_ERCProt(md_item);
    }

    if (isTrue) {
        MData_set_TrueMD(md_item);
    }

    if (isPrealloc) {
        set_bit(MDATA_FEA_PREALLOC, (volatile void *)&(md_item->mdata_fea));
    }

#ifdef DISCO_ERC_SUPPORT
    md_item->erc_dnLoc = NULL;
    md_item->erc_buffer = NULL;
    mutex_init(&md_item->erc_buff_lock);
    md_item->erc_cnt_dnresp = 0;
    md_item->blk_md_type = MDPROT_REP;
#endif
}

void init_metadata (metadata_t *my_metaD, UserIO_addr_t *ioaddr)
{
    memset(my_metaD, 0, sizeof(metadata_t));
    memcpy(&(my_metaD->usr_ioaddr), ioaddr, sizeof(UserIO_addr_t));
}

void free_metadata (metadata_t *my_metaD)
{
    int32_t i;

    if (IS_ERR_OR_NULL(my_metaD->array_metadata)) {
        return;
    }

    for (i = 0; i < my_metaD->num_MD_items; i++) {
        if (IS_ERR_OR_NULL(my_metaD->array_metadata[i])) {
            continue;
        }

        MData_free_MDItem(my_metaD->array_metadata[i]);
        my_metaD->array_metadata[i] = NULL;
    }

    discoC_mem_free(my_metaD->array_metadata);
    my_metaD->array_metadata = NULL;
    my_metaD->num_MD_items = 0;
    my_metaD->num_invalMD_items = 0;
}

void update_metadata_UDataStat (metadata_t *my_metaD, UData_state_t udata_status)
{
    metaD_item_t *loc_ptr;
    int32_t i, j;

    for (i = 0; i < my_metaD->num_MD_items; i++) {
        loc_ptr = my_metaD->array_metadata[i];

        if (unlikely(IS_ERR_OR_NULL(loc_ptr))) {
            continue;
        }

        write_lock(&loc_ptr->mdata_slock);

        clear_bit(MDATA_FEA_UDataCIUser, (volatile void *)&(loc_ptr->mdata_fea));
        clear_bit(MDATA_FEA_UDataCIMDS, (volatile void *)&(loc_ptr->mdata_fea));

        for (j = 0; j < loc_ptr->num_dnLoc; j++) {
            do {
#ifdef DISCO_ERC_SUPPORT
                if (MData_chk_ERCProt(loc_ptr)) {
                    MData_update_DNUDataStat(&(loc_ptr->erc_dnLoc[j]), udata_status);
                    break;
                }
#endif
                MData_update_DNUDataStat(&(loc_ptr->udata_dnLoc[j]), udata_status);
            } while (0);

        }
        write_unlock(&loc_ptr->mdata_slock);
    }
}

void MDMgr_get_volume_cacheSize (uint64_t volid, uint32_t *cache_size,
        uint32_t *num_citem)
{
    cacheMgr_get_cachePool_size(volid, cache_size, num_citem);
}

int32_t MDMgr_init_manager (void)
{
    int32_t ret, pool_size;

    pool_size = dms_client_config->max_nr_io_reqs*MAX_LBS_PER_RQ;
    mdata_item_mpoolID = register_mem_pool("MData_Item",
            pool_size, sizeof(metaD_item_t), true);
    ret = init_metadata_cache();

    return ret;
}

int32_t MDMgr_rel_manager (void)
{
    int32_t ret;

    ret = 0;

    rel_metadata_cache();
    deregister_mem_pool(mdata_item_mpoolID);

    return ret;
}

static void show_MDItem_DNLoc (metaD_DNLoc_t *dnLoc)
{
    int8_t ipv4addr_str[IPV4ADDR_NM_LEN];
    uint32_t rbid, len, offset;
    int32_t i;

    ccma_inet_aton(dnLoc->ipaddr, ipv4addr_str, IPV4ADDR_NM_LEN);
    printk("IPaddr(%s %u) state %u num_rlo %u\n", ipv4addr_str, dnLoc->port,
            dnLoc->udata_status, dnLoc->num_phyLocs);

    for (i = 0; i < dnLoc->num_phyLocs; i++) {
        decompose_triple(dnLoc->rbid_len_offset[i], &rbid, &len, &offset);
        printk(" rlo(%u %u %u)\n", rbid, len, offset);
    }
}

void MData_show_MDItem (metaD_item_t *md_item)
{
    int32_t i;

    printk("\n******** show metadata item state %u *******\n",
            md_item->mdata_stat);
    printk("lbid start %llu volumeReplica %u\n", md_item->lbid_s, md_item->num_replica);
    printk("num hbid %u hbid list:", md_item->num_hbids);

    for (i = 0; i < md_item->num_hbids; i++) {
        printk(" %llu", md_item->arr_HBID[i]);
    }
    printk("\n");

    printk("memack %u diskack %u diskErr_ack %u wfail_connErr %u numReplica %u\n",
            md_item->memack, md_item->diskack, md_item->diskErr_ack,
            md_item->wfail_connErr, md_item->num_replica);
    printk("feature bit 0x%08x\n", md_item->mdata_fea);
    printk("num_dnLoc %u\n", md_item->num_dnLoc);

    for (i = 0; i < md_item->num_dnLoc; i++) {
        printk("udata_dnLoc[%d]: ", i);
        show_MDItem_DNLoc(&md_item->udata_dnLoc[i]);
    }
}

void MData_show_metadata (metadata_t *mdata, int32_t num_md)
{
	int32_t i, j;
	printk("\n*************show array metadata %d*************\n", num_md);

	for (i = 0; i < num_md; i++) {
		printk("*************metadata[%d / %d]*************\n", i, num_md);
		printk("User lbid: %u %llu %u num itmes %d\n", mdata[i].usr_ioaddr.volumeID,
				mdata[i].usr_ioaddr.lbid_start, mdata[i].usr_ioaddr.lbid_len,
                mdata[i].num_MD_items);
		for (j = 0; j < mdata[i].num_MD_items; j++) {
			MData_show_MDItem(mdata[i].array_metadata[j]);
		}
	}
	printk("********************* End *********************\n\n");
}
