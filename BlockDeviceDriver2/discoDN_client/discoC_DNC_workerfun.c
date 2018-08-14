/*
 * Copyright (C) 2010-2020 by Division F / ICL
 * Industrial Technology Research Institute
 *
 * discoC_DNC_workerfun.c
 *
 */
#include <linux/version.h>
#include <linux/blkdev.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <net/sock.h>
#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/dms_kernel_version.h"
#include "../common/discoC_mem_manager.h"
#include "../common/thread_manager.h"
#include "../connection_manager/conn_manager_export.h"
#include "../metadata_manager/metadata_manager.h"
#include "discoC_DNC_Manager_export.h"
#include "discoC_DNUData_Request.h"
#include "discoC_DNUDReq_fsm.h"
#include "discoC_DNUDReqPool.h"
#include "discoC_DNC_worker.h"
#include "discoC_DNC_Manager.h"
#include "discoC_DNC_workerfun_private.h"
#include "discoC_DNC_workerfun.h"
#include "discoC_DN_protocol.h"
#include "discoC_DNAck_workerfun.h"
#include "discoC_DNAck_worker.h"
#include "../flowcontrol/FlowControl.h"
#include "../housekeeping.h"
#include "../SelfTest.h"
#include "../config/dmsc_config.h"

/*
 * _DN_connback_post_fn: callback function for connection module when re-conn to target
 * @conn: which connection back to normal
 * @do_req_retry: perform re-send request or not
 *
 * Return: 0: success
 *        ~0: execute post handling fail
 */
static int32_t _DN_connback_post_fn (discoC_conn_t *conn, bool do_req_retry)
{
    int8_t pname[MAX_THREAD_NAME_LEN];
    int32_t ret;

    ret = 0;

    if (unlikely(discoC_MODE_SelfTest ==
            dms_client_config->clientnode_running_mode)) {
        return ret;
    }

    sprintf(pname, "dn_%s_%d", conn->ipv4addr_str, conn->port);
    conn->skrx_thread_ds = create_sk_rx_thread(pname, conn, DNClient_sockrcv_thdfn);
    if (IS_ERR_OR_NULL(conn->skrx_thread_ds)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN fail to create kthread %s of dn rcv\n", pname);
        DMS_WARN_ON(true);
    }

    if (do_req_retry) {
        add_hk_task(TASK_DN_CONN_BACK_REQ_RETRY, (uint64_t *)conn);
    }

    return ret;
}

/*
 * _find_conn_connPool: find connection from connection manager
 * @ipaddr: IP address of target connection to be found
 * @port  : port of target connection to be found
 *
 * Return: connection that match input arguments
 */
static discoC_conn_t *_find_conn_connPool (uint32_t ipaddr, int32_t port)
{
    discoC_conn_t *conn;

    conn = connMgr_get_conn(ipaddr, port);
    if (IS_ERR_OR_NULL(conn) == false) {
        return conn;
    }

    if (dms_client_config->DNSimulator_on == false) {
        conn = connMgr_create_conn(ipaddr, port, _DN_connback_post_fn,
                DNProto_gen_HBPacket, CONN_IMPL_SOCK);
    } else {
        conn = connMgr_create_conn(ipaddr, port, _DN_connback_post_fn,
                DNProto_gen_HBPacket, CONN_IMPL_DN_SIMULATOR);
    }

    if (IS_ERR_OR_NULL(conn)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN get_dn_conn cannot get connection:%s:%d\n",
                conn->ipv4addr_str, port);
        conn = NULL;
    }

    return conn;
}

/*
 * _get_dnConn_lost_time: get the duration from connection lost time until now
 *       Why we need this? For write requests, clientnode has to send multiple replica
 *       to mulitple DNs. When some of DN lost connection, we can mark DN failure immediate
 *       so that clientnode won't try to wait response.
 *
 *       But if the connection just lost for a short time, it might be back soon.
 *       Under this situation, we should not mark DN failure immediately.
 *       We should wait for a while.
 * @conn: which connection to get lost time
 *
 * Return: duration in milli-seconds
 */
static int64_t _get_dnConn_lost_time (discoC_conn_t *conn)
{
    uint64_t curr, period;

    curr = jiffies_64;

    period = 0;
    if (NULL == conn) {  //_find_conn_connPool guarantee a validate connection or NULL
        return period;
    }

    if (conn->conn_lost_time == 0) {
        return period;
    }

    if (time_after64(curr, conn->conn_lost_time)) {
        period = jiffies_to_msecs(curr - conn->conn_lost_time);
    }

    return period;
}

/*
 * _get_DN_connection: get datanode connection.
 * @ipaddr: IP address of target connection to be found
 * @port  : port of target connection to be found
 * @connlost_time: connection lost time for return to caller if connection broken
 *
 * Return: connection that match input arguments
 */
static discoC_conn_t *_get_DN_connection (uint32_t ipaddr, int32_t port,
        int64_t *connlost_time)
{
    int8_t ipv4addr_str[IPV4ADDR_NM_LEN];
    discoC_conn_t *target;

    do {
        target = _find_conn_connPool(ipaddr, port);
        if (unlikely((NULL == target))) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN null conn\n");
            *connlost_time = 0;
            break;
        }

        if (unlikely(IS_ERR_OR_NULL(target->current_sock))) {
            ccma_inet_aton(ipaddr, ipv4addr_str, IPV4ADDR_NM_LEN);
            dms_printk(LOG_LVL_WARN, "DMSC WANR null conn socket %s:%d\n",
                    ipv4addr_str, port);
            *connlost_time = _get_dnConn_lost_time(target);
            target = NULL;
            break;
        }

        if (connMgr_get_connState(target) != SOCK_CONN) {
            set_res_available(ipaddr, port, false);
            target->connOPIntf.reconnect(target);
            *connlost_time = _get_dnConn_lost_time(target);
            target = NULL;
            break;
        }
    } while (0);

    return target;
}

/*
 * _get_DNReadReq_opcode: base on different criteria to get DN Read request opcode
 * @myDNReq: target DNRequest to check
 *
 * Return: DN Request operation code
 */
static uint16_t _get_DNReadReq_opcode (DNUData_Req_t *myDNReq)
{
    uint16_t dnOpcode;
    bool send_rw_payload, do_rw_IO;


    send_rw_payload = DNUDReq_check_feature(myDNReq, DNREQ_FEA_SEND_PAYLOAD);
    do_rw_IO = DNUDReq_check_feature(myDNReq, DNREQ_FEA_DO_RW_IO);
    if ((send_rw_payload == false) && do_rw_IO) {
        dnOpcode = DNUDREQ_READ_NO_PAYLOAD;
    } else if (send_rw_payload && (do_rw_IO == false)) {
        dnOpcode = DNUDREQ_READ_NO_IO;
    } else if ((send_rw_payload == false) && (do_rw_IO == false)) {
        dnOpcode = DNUDREQ_READ_NO_PAYLOAD_IO;
    } else {
        dnOpcode = DNUDREQ_READ;
    }

    return dnOpcode;
}

/*
 * DNCWker_send_readReq: DN Worker send read request
 * @myDNReq: DN Request to be sent
 * @packet : packet array for filling information
 */
static void DNCWker_send_readReq (DNUData_Req_t *myDNReq, int8_t *packet)
{
    int8_t ipv4addr_str[IPV4ADDR_NM_LEN];
    discoC_conn_t *target;
    conn_intf_fn_t *connIntf;
    ReqCommon_t *dnR_comm;
    int64_t connlost_time;
    int32_t packet_len, bytes_tx;
    bool fake_send;

    dnR_comm = &myDNReq->dnReq_comm;
    dnR_comm->ReqOPCode = _get_DNReadReq_opcode(myDNReq);
    DNProto_gen_ReqPacket(myDNReq, &packet_len, packet);

    target = _get_DN_connection(myDNReq->ipaddr, myDNReq->port, &connlost_time);
    if (NULL == target) {
        goto READ_DN_CONN_ERR;
    }

    connIntf = &target->connOPIntf;
    if (unlikely(dms_client_config->clientnode_running_mode ==
            discoC_MODE_SelfTest)) {
        fake_send = true;
    } else {
        fake_send = false;
    }

    dnR_comm->t_sendReq = jiffies_64;

    atomic_inc(&dnClient_mgr.num_DNReq_waitResp);

    bytes_tx = connIntf->tx_send(target, packet, packet_len, true, fake_send);

    if (bytes_tx < 0 || bytes_tx != packet_len) {
        connIntf->resetConn(target);
        //TODO commit to payload with conn err
        goto READ_DN_CONN_ERR;
    }

    if (fake_send) {
        //perform_self_test_for_dn(target, myDNReq, DNUDRESP_READ_PAYLOAD);
        atomic_dec(&dnClient_mgr.num_DNReq_waitResp);
    }

    DNUDReq_deref_Req(myDNReq);
    return;

READ_DN_CONN_ERR:
    ccma_inet_aton(myDNReq->ipaddr, ipv4addr_str, IPV4ADDR_NM_LEN);
    dms_printk(LOG_LVL_INFO,
            "DMSC INFO RFAIL conn invalid try next %s:%d\n",
            ipv4addr_str, myDNReq->port);
    /*
     * NOTE: no call deref since handle_DNAck_Read will deref
     */
    handle_DNAck_Read(myDNReq, NULL, DN_ERR_DISCONNECTED);
}

/*
 * _get_DNReadReq_opcode: base on different criteria to get DN Write request opcode
 * @myDNReq: target DNRequest to check
 *
 * Return: DN Request operation code
 */
static uint16_t _get_DNWriteReq_opcode (DNUData_Req_t *myDNReq)
{
    uint16_t dnOpcode;
    bool send_rw_payload, do_rw_IO;

    send_rw_payload = DNUDReq_check_feature(myDNReq, DNREQ_FEA_SEND_PAYLOAD);
    do_rw_IO = DNUDReq_check_feature(myDNReq, DNREQ_FEA_DO_RW_IO);
    if ((send_rw_payload == false) && do_rw_IO) {
        dnOpcode = DNUDREQ_WRITE_NO_PAYLOAD;
    } else if (send_rw_payload && (do_rw_IO == false)) {
        dnOpcode = DNUDREQ_WRITE_NO_IO;
    } else if ((send_rw_payload == false) && (do_rw_IO == false)) {
        dnOpcode = DNUDREQ_WRITE_NO_PAYLOAD_IO;
    } else {
        dnOpcode = DNUDREQ_WRITE;
    }

    return dnOpcode;
}

/*
 * _send_DNWReq_fail: post operation when send data request but connection broken
 * @dn_req: target DNRequest post handling
 * @connlost_time: time duration from connection broke until now
 */
static void _send_DNWReq_fail (DNUData_Req_t *dn_req, int64_t connlost_time)
{
    if (connlost_time >= dms_client_config->time_wait_IOdone_conn_lost) {
        /*
         * NOTE: no call deref since handle_DNAck_Write will deref
         */
        handle_DNAck_Write(dn_req, DN_ERR_DISCONNECTED);
    } else {
        DNUDReq_deref_Req(dn_req);
    }
}

/*
 * _send_DNWReq_data: send payload of write request
 * @dn_req: target DNRequest post handling
 * @target: connection of target DN
 * @simulate_tx: is this call is simulation or not
 *
 * Return: send result: 0: success
 *                     ~0: send fail
 */
static int32_t _send_DNWReq_data (DNUData_Req_t *dn_req, discoC_conn_t *target,
        bool simulate_tx)
{
    conn_intf_fn_t *connIntf;
    int8_t *payload;
    int32_t ret, seg_index, num_of_send, payload_sz;

    ret = 0;
    connIntf = &target->connOPIntf;

    for (seg_index = 0; seg_index < dn_req->num_UDataSeg; seg_index++) {
        payload = dn_req->UDataSeg[seg_index].iov_base;
        payload_sz = dn_req->UDataSeg[seg_index].iov_len;
        if (unlikely(IS_ERR_OR_NULL(payload))) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN dn_req->UDataSeg[%d].payload is NULL "
                    "(dnid=%u total seg %d)\n",
                    seg_index, dn_req->dnReq_comm.ReqID, dn_req->num_UDataSeg);
            ret = -EINVAL;
            break;
        }

        num_of_send = connIntf->tx_send(target, payload, payload_sz,
                false, simulate_tx);
        if (num_of_send < 0 || num_of_send != payload_sz) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN conn send in-complete write data %s:%d "
                    "DNReq %u %u %u\n",
                    target->ipv4addr_str, dn_req->port,
                    dn_req->dnReq_comm.ReqID, payload_sz, num_of_send);
            ret = -ENETUNREACH;
            break;
        }
    }

    return ret;
}

/*
 * _send_DNWReq_gut: core function to send DN Write request
 * @dn_req: target DNRequest for sending
 * @target: connection of target DN
 * @packet: packet that contain necessary information to sent
 * @packet_len: lenght of packet
 *
 * Return: send result: 0: success
 *                     ~0: send fail
 */
static int32_t _send_DNWReq_gut (DNUData_Req_t *dn_req, discoC_conn_t *target,
        int8_t *packet, int32_t packet_len)
{
    conn_intf_fn_t *connIntf;
    int32_t ret, num_of_send;
    uint16_t dnR_opcode;
    bool tx_simu;  //simulate send request
#if DN_PROTOCOL_VERSION == 2
    uint32_t magic_e;
#endif

    ret = 0;
    if (unlikely(dms_client_config->clientnode_running_mode == discoC_MODE_SelfTest)) {
        tx_simu = true;
    } else {
        tx_simu = false;
    }

    connIntf = &target->connOPIntf;
    do {
        //send request header
        num_of_send = connIntf->tx_send(target, packet, packet_len, false, tx_simu);
        //perform_self_test_for_dn(target, dn_req, DNUDRESP_MEM_ACK); //send mem ack if simulate
        if (num_of_send < 0 || num_of_send != packet_len) {
            dms_printk(LOG_LVL_WARN, "DMS WARN send WReq header %s:%d DNReq %u "
                    "%u %u\n", target->ipv4addr_str, dn_req->port,
                    dn_req->dnReq_comm.ReqID, packet_len, num_of_send);
            ret = -ENETUNREACH;
            break;
        }

        dnR_opcode = dn_req->dnReq_comm.ReqOPCode;
        if (DNUDREQ_WRITE == dnR_opcode ||
                DNUDREQ_WRITE_NO_IO == dnR_opcode) {
            if ((ret = _send_DNWReq_data(dn_req, target, tx_simu))) {
                break;
            }
        }

#if DN_PROTOCOL_VERSION == 2
        //TODO: move to protocol
        magic_e = htonl(DN_MAGIC_E);
        num_of_send = connIntf->tx_send(target, &magic_e, 4, false,
                do_simulation_send);
        if (4 != num_of_send) {
            dms_printk(LOG_LVL_WARN,
                "DMSC WARN send end magic number to %s:%d fail nnid=%u dnid=%u\n",
                hname, dn_req->port, nnreq->nnreq_id, dn_req->dnreq_id);
            ret = -ENETUNREACH;
            break;
        }
#endif

        if (tx_simu) {
            atomic_dec(&dnClient_mgr.num_DNReq_waitResp);
            //perform_self_test_for_dn(target, dn_req, DNUDRESP_DISK_ACK);
            break;
        }
    } while (0);

    return ret;
}

/*
 * DNCWker_send_writeReq: DN worker send write request
 * @dn_req: target DNRequest for sending
 * @packet: packet buffer for filling packet
 *
 * Return: send result: 0: success
 *                     ~0: send fail
 */
static void DNCWker_send_writeReq (DNUData_Req_t *dn_req, int8_t *packet)
{
    int8_t ipv4addr_str[IPV4ADDR_NM_LEN];
    discoC_conn_t *target;
    ReqCommon_t *dnR_comm;
    int64_t connlost_time;
    int32_t packet_len;

    connlost_time = 0;

    dnR_comm = &dn_req->dnReq_comm;
    dnR_comm->ReqOPCode = _get_DNWriteReq_opcode(dn_req);
    DNProto_gen_ReqPacket(dn_req, &packet_len, packet);

    target = _get_DN_connection(dn_req->ipaddr, dn_req->port, &connlost_time);
    if (NULL == target) {
        ccma_inet_aton(dn_req->ipaddr, ipv4addr_str, IPV4ADDR_NM_LEN);
        dms_printk(LOG_LVL_INFO,
                "DMSC INFO WFAIL conn invalid discard %s:%d\n", ipv4addr_str,
                dn_req->port);
        _send_DNWReq_fail(dn_req, connlost_time);
        return;
    }

    dnR_comm->t_sendReq = jiffies_64;

    if (connMgr_lockConn(target)) {
        dms_printk(LOG_LVL_INFO,
                "DMSC INFO WFAIL fail acquire conn lock %s:%d\n",
                target->ipv4addr_str, target->port);
        goto WRITE_DN_CONN_ERR;
    }

    atomic_inc(&dnClient_mgr.num_DNReq_waitResp);

    if (_send_DNWReq_gut(dn_req, target, packet, packet_len)) {
        connMgr_unlockConn(target);
        goto WRITE_DN_CONN_ERR;
    }

    connMgr_unlockConn(target);
    DNUDReq_deref_Req(dn_req);

    return;

WRITE_DN_CONN_ERR:
    connlost_time = _get_dnConn_lost_time(target);
    _send_DNWReq_fail(dn_req, connlost_time);
}

/*
 * DNCWker_send_DNUDReq: request sending function executed by worker
 * @dn_req: target DNRequest for sending
 * @packet: packet buffer for filling packet
 *
 */
void DNCWker_send_DNUDReq (DNUData_Req_t *dnReq, int8_t *pkt_buffer)
{
    ReqCommon_t *dnR_comm;
    DNUDREQ_FSM_action_t action;

    action = DNUDReq_update_state(&dnReq->dnReq_stat, DNREQ_E_SendDone);
    if (DNREQ_A_DO_WAIT_RESP != action) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN send dnReq %u fail TR SendDone action %s\n",
                dnReq->dnReq_comm.ReqID, DNUDReq_get_action_str(action));
    }

    dnR_comm = &dnReq->dnReq_comm;
#if (DN_PROTOCOL_VERSION == 2) && (CN_CRASH_RECOVERY == 1)
    if (ioSReq->ts_dn_logging == 0) {
        ioSReq->ts_dn_logging = jiffies_64;
    }
#endif

    memset(pkt_buffer, 0, DN_PACKET_BUFFER_SIZE);
    switch (dnR_comm->ReqOPCode) {
    case DNUDREQ_READ:
        DNCWker_send_readReq(dnReq, pkt_buffer);
        break;
    case DNUDREQ_WRITE:
        DNCWker_send_writeReq(dnReq, pkt_buffer);
        break;
    default:
        dms_printk(LOG_LVL_WARN, "DMSC WARN unknown dnReq opcode %d\n",
                dnR_comm->ReqOPCode);
        DMS_WARN_ON(true);
    }
}

/*
 * _DNCWker_rcv_readData: receiver receive read payload from socket
 * @connIntf: connection function interface for executing connection related function
 * @expected_size: expected size to be received
 * @payload: memory buffer for hosting payload from datanode
 * @sk_wapper: socket wrapper that contains socket
 *
 * Return: actual sized be received
 */
static int32_t _DNCWker_rcv_readData (conn_intf_fn_t *connIntf, int32_t expected_size,
        data_mem_chunks_t *payload, cs_wrapper_t *sk_wapper)
{
    data_mem_item_t *buf_ptr;
    int32_t ret, rcv_size, conn_ret;
    uint16_t *cnt;

    ret = 0;

    cnt = &(payload->num_chunks);
    while (expected_size > 0) {
        buf_ptr = (data_mem_item_t *)
                discoC_malloc_pool(DNClientMgr_get_ReadMemPoolID());
        if (unlikely(IS_ERR_OR_NULL(buf_ptr))) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN fail alloc mem rcv data\n");
            DMS_WARN_ON(true);
            ret = -ENOMEM;
            break;
        }

        rcv_size = expected_size >= MAX_SIZE_KERNEL_MEMCHUNK ?
                MAX_SIZE_KERNEL_MEMCHUNK : expected_size;
        if ((conn_ret = connIntf->rx_rcv(sk_wapper, buf_ptr->data_ptr, rcv_size)) <= 0) {
            discoC_free_pool(DNClientMgr_get_ReadMemPoolID(), (void *)buf_ptr);
            dms_printk(LOG_LVL_WARN, "DMSC WARN rcv dn data\n");
            if (conn_ret == 0) {
                //TODO: why we need a msleep here?
                //      reason: when connection close, rx threads won't
                //              exist but retry to rcv and cause CPU high loading
                msleep(1000);
            }
            return -EPIPE;
        }

        payload->chunks[*cnt] = buf_ptr;
        payload->chunk_item_size[*cnt] = rcv_size;
        (*cnt)++;
        expected_size -= MAX_SIZE_KERNEL_MEMCHUNK;
    }

    return ret;
}

/*
 * _DNCWker_rcv_dnAck: receiver receive datanode ack
 * @connIntf: connection function interface for executing connection related function
 * @param: ack data structured to holding dn ack
 * @sk_wapper: socket wrapper that contains socket
 *
 * Return: actual sized be received
 */
static int32_t _DNCWker_rcv_dnAck (conn_intf_fn_t *connIntf, DNAck_item_t *param,
        cs_wrapper_t *sk_wapper)
{
    CN2DN_UDRWResp_hder_t *dnAck_pkt;
    data_mem_item_t *buf_ptr;
    data_mem_chunks_t *payload;
    int32_t recv_size, expected_size, rcv_size, ret;
#if DN_PROTOCOL_VERSION == 2
    uint32_t magic_e;
#endif
    uint16_t *cnt;

    dnAck_pkt = &param->pkt;
    payload = &(param->payload);
    recv_size = connIntf->rx_rcv(sk_wapper, dnAck_pkt, DATANODE_ACK_SIZE);
    if (recv_size <= 0) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN rcv dn header\n");
        if (recv_size == 0) {
            //TODO: why we need a msleep here?
            //      reason: when connection close, rx threads won't
            //              exist but retry to rcv and cause CPU high loading
            msleep(1000);
        }

        return -EPIPE;
    }

    ret = 0;
    DNProto_ReqAck_ntoh(dnAck_pkt);

    payload->num_chunks = 0;
    if (dnAck_pkt->responseType == DNUDRESP_READ_PAYLOAD ||
            dnAck_pkt->responseType == DNUDRESP_HBIDERR) {
        if (dnAck_pkt->responseType == DNUDRESP_READ_PAYLOAD) {
            expected_size = (dnAck_pkt->num_of_lb << BIT_LEN_DMS_LB_SIZE);
        } else {
            expected_size = dnAck_pkt->num_of_lb*sizeof(uint64_t); //a hbid has 8 bytes
        }

        ret = _DNCWker_rcv_readData(connIntf, expected_size, payload, sk_wapper);
    } else if (dnAck_pkt->responseType == DNUDRESP_READ_NO_PAYLOAD) {
        //create dummy payload
        expected_size = (dnAck_pkt->num_of_lb << BIT_LEN_DMS_LB_SIZE);

        cnt = &(payload->num_chunks);
        *cnt = 0;
        while (expected_size > 0) {
            buf_ptr = (data_mem_item_t *)
                    discoC_malloc_pool(DNClientMgr_get_ReadMemPoolID());
            rcv_size = expected_size >= MAX_SIZE_KERNEL_MEMCHUNK ?
                    MAX_SIZE_KERNEL_MEMCHUNK : expected_size;
            memset(buf_ptr->data_ptr, 0, MAX_SIZE_KERNEL_MEMCHUNK);
            payload->chunks[*cnt] = buf_ptr;
            payload->chunk_item_size[*cnt] = rcv_size;
            (*cnt)++;
            expected_size -= MAX_SIZE_KERNEL_MEMCHUNK;
        }
    }

#if DN_PROTOCOL_VERSION == 2
    if (sock_rcv(sk_wapper, &magic_e, sizeof(uint8_t)*4) < 4) {
        pkt->magic_num2 = 0;
    } else {
        pkt->magic_num2 = ntohl(magic_e);
    }
#endif

    param->receive_time = jiffies_64;
    return ret;
}

/*
 * DNClient_sockrcv_thdfn: socket receive thread function called by receiver thread
 * @thread_data: thread data
 *
 * Return: thread end result. 0: end success
 *                           ~0: end result
 */
static int32_t DNClient_sockrcv_thdfn (void *thread_data)
{
    dmsc_thread_t *rx_thread_data;
    discoC_conn_t *myConn;
    cs_wrapper_t *sk_wapper;
    DNAck_item_t *ackItem;
    conn_intf_fn_t *connIntf;
    uint8_t response_type;

    allow_signal(SIGKILL); //lego: for reset

    rx_thread_data = (dmsc_thread_t *)thread_data;
    myConn = (discoC_conn_t *)rx_thread_data->usr_data;
    if (IS_ERR_OR_NULL(myConn)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN null conn of DNClient rcv thread\n");
        DMS_WARN_ON(true);
        return 0;
    }

    connIntf = &myConn->connOPIntf;
    sk_wapper = get_sock_wrapper(myConn);
    if (IS_ERR_OR_NULL(sk_wapper)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN fail to get socket wapper from %s:%d\n",
                myConn->ipv4addr_str, myConn->port);
        //TODO: If null, we should re-build conn
        return 0;
    }

    while (kthread_should_stop() == false) {
        ackItem = DNAckMgr_alloc_ack();
        if (unlikely(NULL == ackItem)) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN fail alloc DN ack item\n");
            DMS_WARN_ON(true);
            continue;
        }
        DNAckMgr_init_ack(ackItem, myConn);

        if (_DNCWker_rcv_dnAck(connIntf, ackItem, sk_wapper) < 0) {
            DNAckMgr_free_ack(ackItem);
            continue;
        }

        response_type = ackItem->pkt.responseType;
#if (DN_PROTOCOL_VERSION == 2) && (CN_CRASH_RECOVERY == 1)
        if (ackItem->pkt.responseType == DNUDRESP_CLEAN_LOG) {
            DNAckMgr_free_ack(ackItem);
            param = NULL;
            continue;
        }
#endif

        DNClientMgr_set_last_DNUDataRespID(ackItem->pkt.dnid);

        if (response_type == DNUDRESP_READ_PAYLOAD ||
                response_type == DNUDRESP_READ_NO_PAYLOAD ||
                response_type == DNUDRESP_DISK_ACK ||
                response_type == DNUDRESP_WRITEANDGETOLD_DISK_ACK) {
            atomic_dec(&dnClient_mgr.num_DNReq_waitResp);
        }

        DNAckMgr_submit_ack(ackItem);
    }// while goal->should_run

    deRef_conn_sock(sk_wapper);

    return 0;
}

