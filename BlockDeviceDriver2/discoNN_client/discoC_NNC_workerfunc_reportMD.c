/*
 * Copyright (C) 2010 - 2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 *  discoC_NNC_workerfunc_reportMD.c
 *
 */


#include <linux/version.h>
#include <linux/delay.h>
#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/dms_kernel_version.h"
#include "../common/discoC_mem_manager.h"
#include "../common/thread_manager.h"
#include "../metadata_manager/metadata_manager.h"
#include "../metadata_manager/metadata_cache.h"
#include "../connection_manager/conn_manager_export.h"
#include "discoC_NNC_Manager_api.h"
#include "../config/dmsc_config.h"
#include "discoC_NNC_worker.h"
#include "../flowcontrol/FlowControl.h"
#include "discoC_NNClient.h"
#include "discoC_NNMData_Request.h"
#include "discoC_NNC_workerfunc_reportMD.h"
#include "discoC_NN_protocol.h"
//#include "discoC_NNC_ovw.h"

/*
 * _nnc_send_MDReport_packet: check connection state and send packet
 * @nn_conn: connection to check
 * @msg: packet buffer
 * @pkt_len: how many bytes in buffer to sent
 *
 * Return: whether send success
 *         0: success
 *        ~0: fail
 */
static int32_t _nnc_send_MDReport_packet (discoC_conn_t *nn_conn,
        int8_t *msg, int32_t pkt_len)
{
    conn_intf_fn_t *connIntf;
    int32_t retcode, s;

    if (unlikely(IS_ERR_OR_NULL(nn_conn))) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN fail send MData report null conn\n");
        return -ENETUNREACH;
    }

    connIntf = &(nn_conn->connOPIntf);
    retcode = 0;
    s = 0;
    if (connMgr_get_connState(nn_conn) == SOCK_CONN) {
        if (unlikely(dms_client_config->clientnode_running_mode ==
                     discoC_MODE_SelfTest ||
                     dms_client_config->clientnode_running_mode ==
                     discoC_MODE_SelfTest_NN_Only)) {
            s = pkt_len;
        } else {
            s = connIntf->tx_send(nn_conn, msg, pkt_len, true, false);
        }

        if (s != pkt_len) {
            retcode = -ENETRESET;
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN conn send in-complete commit req (%d %d)\n",
                    s, pkt_len);
            connIntf->resetConn(nn_conn);
        }
    } else {
        retcode = -ENETUNREACH;
        //add to connection retry entry
        connIntf->reconnect(nn_conn);
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN invalid conn before send commit msg\n");
    }

    return retcode;
}

/*
 * _nnc_chk_write_fail: check whether report user data access status at write IO
 * @nnMDReq: target metadata request that has metadata needed be report
 *
 * Case 1: RR on, if number of replica in metadata < volume replica, report
 * Case 2: RR off, if number of replica in metadata = number of alive DN, don't report
 * Case 3: Some DN write fail (IO exception, connection broken, etc)
 *
 * Return: whether report DN access result
 *         true: report to namenode
 *        false: no need report
 */
static bool _nnc_chk_write_fail (metadata_t *my_MData)
{
    metaD_item_t *my_MD_item;
    metaD_DNLoc_t *dnLoc;
    int32_t i, j, num_lrs;
    bool doReport;

    doReport = false;
    num_lrs = my_MData->num_MD_items;

    for (i = 0; i < num_lrs; i++) {
        //TODO: should check == MData_VALID_LESS_REPLICA
        if (my_MData->array_metadata[i]->mdata_stat != MData_VALID) {
            doReport = true;
            break;
        }

        my_MD_item = my_MData->array_metadata[i];
        for (j = 0; j < my_MD_item->num_dnLoc; j++) {
            dnLoc = &(my_MD_item->udata_dnLoc[j]);
            if (MData_chk_UDStatus_writefail(dnLoc)) {
                doReport = true;
                break;
            }
        }

        if (doReport) {
            break;
        }
    }

    return doReport;
}

/*
 * NNClient_reportMD_writeErr: report metadata access with disk error of write IO
 *     1. find out which DN got disk IO error from datanode ack
 *     2. packet these information into packet
 *     3. send packet to namenode
 * @nnMDReq: target metadata request that has metadata needed be report
 * @message_buffer: memory buffer provide by caller to prepare packet
 *
 *  Return: 0: send success or nothing to send
 *         ~0: has something to send but send fail
 */
int32_t NNClient_reportMD_writeErr (NNMData_Req_t *nnMDReq, int8_t *message_buffer)
{
    UserIO_addr_t *ioaddr;
    int32_t total_pkt_len = 0, msg_buf_size = 0, size_of_1_LHO = 0;
    int8_t *msg_buf;
    int32_t ret;

    ret = 0;
    if (IS_ERR_OR_NULL(nnMDReq) ||
            IS_ERR_OR_NULL(nnMDReq->MetaData->array_metadata)) {
        return -EINVAL;
    }

    ioaddr = &nnMDReq->nnMDReq_ioaddr;
    msg_buf = message_buffer;
    if (_nnc_chk_write_fail(nnMDReq->MetaData)) {
        dms_printk(LOG_LVL_INFO,
                "DMSC fail write DN or replica not enough, "
                "invalidate cache volume %u MDReq ID %u\n",
                ioaddr->volumeID, nnMDReq->nnMDReq_comm.ReqID);
        inval_volume_MDCache_LBID(ioaddr->volumeID, ioaddr->lbid_start, ioaddr->lbid_len);

        size_of_1_LHO = sizeof(uint64_t) + sizeof(uint32_t) + sizeof(uint64_t) +
                        (sizeof(CN2NN_ciMDReq_dnLoc_pkt_t) + sizeof(uint64_t)) *
                        MAX_NUM_REPLICA;
                        // (DN info + triplet)*MAX_NUM_REPLICA
        msg_buf_size = sizeof(CN2NN_ciMDReq_hder_t) + size_of_1_LHO * ioaddr->lbid_len;

        if (unlikely(msg_buf_size > MD_CI_BUFFER_SIZE)) {
            dms_printk(LOG_LVL_INFO,
                    "DMSC INFO msg_buf_size over (%d) "
                    "MD_CI_BUFFER_SIZE\n", msg_buf_size);
            msg_buf = discoC_mem_alloc(msg_buf_size, GFP_NOWAIT | GFP_ATOMIC);

            if (IS_ERR_OR_NULL(msg_buf)) {
                dms_printk(LOG_LVL_WARN,
                        "DMSC ERROR: Fail alloc mem for commit msg buf\n");
                DMS_WARN_ON(true);
                return -ENOMEM;
            }
        }

        memset(msg_buf, 0, sizeof(int8_t) * msg_buf_size);

        //malloc msg_buf and fill-in commit_namenode data
        total_pkt_len = NNProto_gen_ciMDReq_WriteIO_pkt(ioaddr->volumeID,
                nnMDReq->nnMDReq_comm.ReqID, nnMDReq->MetaData, msg_buf);

        if (total_pkt_len > 0) {
            ret = _nnc_send_MDReport_packet(nnMDReq->nnSrv_client->nnSrv_conn,
                    msg_buf, total_pkt_len);
        }

        //free msg_buf
        if (unlikely(msg_buf_size > MD_CI_BUFFER_SIZE)) {
            discoC_mem_free(msg_buf);
            msg_buf = NULL;
        }
    }

    return ret;
}

/*
 * NNClient_reportMD_readErr: report metadata access with disk error of read IO
 *     1. find out which DN got disk IO error from datanode ack
 *     2. packet these information into packet
 *     3. send packet to namenode
 * @nnMDReq: target metadata request that has metadata needed be report
 * @message_buffer: memory buffer provide by caller to prepare packet
 *
 *  Return: 0: send success or nothing to send
 *         ~0: has something to send but send fail
 */
int32_t NNClient_reportMD_readErr (NNMData_Req_t *nnMDReq, int8_t *message_buffer)
{
    UserIO_addr_t *ioaddr;
    int8_t *msg_buf;
    int32_t msg_buf_size, size_of_1_LHO, total_pkt_len;
    int32_t ret;

    ret = 0;
    if (unlikely(IS_ERR_OR_NULL(nnMDReq))) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN try to commit null nn ptr\n");
        DMS_WARN_ON(true);
        return ret;
    }

    ioaddr = &nnMDReq->nnMDReq_ioaddr;
    msg_buf = message_buffer;

    dms_printk(LOG_LVL_INFO,
            "DMSC INFO commit read error reqID %u volid %u\n",
            nnMDReq->nnMDReq_comm.ReqID, ioaddr->volumeID);
    inval_volume_MDCache_LBID(ioaddr->volumeID, ioaddr->lbid_start, ioaddr->lbid_len);

    /*
     * Lego: The worst case of msg buffer size will be each 1 LHO contains
     * only 1 LBID with 3 fail dn locations
     */
    size_of_1_LHO = sizeof(uint64_t) + sizeof(uint32_t) + sizeof(uint64_t) + sizeof(uint32_t) +
                    (sizeof(CN2NN_ciMDReq_dnLoc_pkt_t) + sizeof(uint64_t)) * MAX_NUM_REPLICA;
    msg_buf_size = sizeof(CN2NN_ciMDReq_hder_t) + size_of_1_LHO * ioaddr->lbid_len;

    if (msg_buf_size > MD_CI_BUFFER_SIZE) {
        msg_buf = discoC_mem_alloc(msg_buf_size, GFP_NOWAIT | GFP_ATOMIC);

        if (unlikely(IS_ERR_OR_NULL(msg_buf))) {
            dms_printk(LOG_LVL_INFO,
                    "DMSC WARN fail alloc mem for commit read nn\n");
            DMS_WARN_ON(true);
            return -ENOMEM;
        }
    }

    memset(msg_buf, 0, sizeof(int8_t) * msg_buf_size);

    total_pkt_len = NNProto_gen_ciMDReq_ReadIO_pkt(ioaddr->volumeID,
            nnMDReq->nnMDReq_comm.ReqID, nnMDReq->MetaData, msg_buf);

    NNProto_show_ciMDReq_WriteIO_pkt(LOG_LVL_DEBUG, msg_buf);

    if (total_pkt_len > 0) {
        _nnc_send_MDReport_packet(nnMDReq->nnSrv_client->nnSrv_conn,
                msg_buf, total_pkt_len);
    } else {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN packet size <= 0 when ci read error %u\n",
                ioaddr->volumeID);
        DMS_WARN_ON(true);
    }

    if (msg_buf_size > MD_CI_BUFFER_SIZE) {
        discoC_mem_free(msg_buf);
        msg_buf = NULL;
    }

    return ret;
}

