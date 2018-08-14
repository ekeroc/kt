/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_DNAck_worker.c
 *
 * this module used to control the timer and error handling of the datanode
 * read/write request.
 *
 * rule:
 *  recv ack && !time out:
 *      receive - 1. first disk ack,
 *                2. memory ack == replicaFactor --> commit to user
 *              - 3. disk ack == replicaFactor,  --> commit to NN:   success
 *
 *  time out:       partial success - 1. disk ack < replicaFactor
 *                  retry           - 1. disk ack == 0
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
#include "discoC_DNUDReqPool.h"
#include "discoC_DNC_worker.h"
#include "discoC_DNC_Manager.h"
#include "discoC_DNC_workerfun.h"
#include "discoC_DN_protocol.h"
#include "discoC_DNAck_worker.h"
#include "discoC_DNAck_workerfun.h"
#include "discoC_DNAck_worker_private.h"
#include "../flowcontrol/FlowControl.h"
#include "../housekeeping.h"
#include "../SelfTest.h"
#include "../config/dmsc_config.h"

/*
 * DNAckMgr_alloc_ack: allocate ack from memory pool
 *
 * Return: ack item for caller
 */
DNAck_item_t *DNAckMgr_alloc_ack (void)
{
    return (DNAck_item_t *)discoC_malloc_pool(DNAck_mgr.DNAck_mpool_ID);
}

/*
 * _free_DNAck_read_buffer: return ack payload to memory pool
 * @data: payload to be free
 */
static void _free_DNAck_read_buffer (data_mem_chunks_t *data)
{
    uint32_t i;

    for (i = 0; i < data->num_chunks; i++) {
        discoC_free_pool(DNClientMgr_get_ReadMemPoolID(), data->chunks[i]);
    }
    data->num_chunks = 0;
}

/*
 * DNAckMgr_free_ack: return ack to memory pool
 * @ackItem: ack item to be free
 */
void DNAckMgr_free_ack (DNAck_item_t *ackItem)
{
    data_mem_chunks_t *payload;

    payload = &ackItem->payload;
    if (payload->num_chunks > 0) {
        _free_DNAck_read_buffer(payload);
    }

    discoC_free_pool(DNAck_mgr.DNAck_mpool_ID, ackItem);
}

/*
 * DNAckMgr_init_ack: initialize an ack
 * @ackItem: ack item to be initialized
 * @conn: connection the ack received from
 */
void DNAckMgr_init_ack (DNAck_item_t *ackItem, discoC_conn_t *conn)
{
    ackItem->conn_entry = conn;
    ackItem->in_dnAck_workq = false;
    ackItem->payload.num_chunks = 0;
    INIT_LIST_HEAD(&ackItem->entry_ackpool);
}

/*
 * _handle_dn_miss: add a record to flow control when got a ack but DNRequest already be canceled
 * @param: ack from datanode
 */
static void _handle_dn_miss (DNAck_item_t *param)
{
    uint8_t pkt_response_type;
    CN2DN_UDRWResp_hder_t *pkt;

    /*
     * decrease the timeout cnt , beacuse dn still workable.
     * The timeout maybe comes from too short time to access dn
     */
    pkt = &param->pkt;
    pkt_response_type = pkt->responseType;
    if ((pkt_response_type == DNUDRESP_READ_PAYLOAD ||
            pkt_response_type == DNUDRESP_READ_NO_PAYLOAD ||
            pkt_response_type == DNUDRESP_DISK_ACK ||
            pkt_response_type == DNUDRESP_WRITEANDGETOLD_DISK_ACK) &&
            (!IS_ERR_OR_NULL(param->conn_entry))) {
        dec_request_timeout_record(param->conn_entry->ipaddr,
                param->conn_entry->port);
    }
}

/*
 * return: true  : EIJ type is timeout, no further action
 *         false : EIJ type is not timeout, caller take further action
 */
static bool _handle_eij (CN2DN_UDRWResp_hder_t *pkt, DNUData_Req_t *dn_req)
{
    int32_t err_inject_type, err_inject_rw;
    bool ret;

    if (pkt->responseType == DNUDRESP_READ_PAYLOAD ||
            pkt->responseType == DNUDRESP_READ_NO_PAYLOAD) {
        err_inject_rw = 0;
    } else {
        err_inject_rw = 1;
    }

    err_inject_type =
            meet_error_inject_test(&dms_client_config->ErrInjectList_Header,
                    &dms_client_config->ErrInjectList_Lock,
                    dn_req->ipaddr, dn_req->port, err_inject_rw);

    ret = false;
    switch (err_inject_type) {
    case ErrDNNotError:
        break;
    case ErrDNTimeout:
        ret = true;
        break;
    case ErrDNException:
        pkt->responseType = DNUDRESP_ERR_IOException;
        break;
    case ErrDNHBIDNotMatch:
        pkt->responseType = DNUDRESP_HBIDERR;
        break;
    default:
        dms_printk(LOG_LVL_WARN, "DMSC WARN wrong eij type %d\n",
                err_inject_type);
        DMS_WARN_ON(true);
        break;
    }

    return ret;
}

/*
 * _handle_dn_resp: Call function in ack_workerfunc.c to process dn ack
 * @pkt_response_type: which response type to handle
 * @param: ack from datanode
 * @dn_req: request the belong to ack
 *
 */
static void _handle_dn_resp (uint8_t pkt_response_type,
        DNAck_item_t *param, DNUData_Req_t *dn_req)
{
    ReqCommon_t *dnR_comm;

    dnR_comm = &dn_req->dnReq_comm;
    switch (pkt_response_type) {
    case DNUDRESP_READ_PAYLOAD:
    case DNUDRESP_READ_NO_PAYLOAD:
        handle_DNAck_Read(dn_req, &param->payload, pkt_response_type);
        break;
    case DNUDRESP_MEM_ACK:
    case DNUDRESP_DISK_ACK:
    case DNUDRESP_WRITEANDGETOLD_MEM_ACK:
    case DNUDRESP_WRITEANDGETOLD_DISK_ACK:
        handle_DNAck_Write(dn_req, pkt_response_type);
        break;
    case DNUDRESP_ERR_IOException:
    case DNUDRESP_ERR_IOFailLUNOK:
    case DNUDRESP_ERR_LUNFail:
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN DN disk error (type=%d) dnid=%u payload id=%u\n",
                pkt_response_type, dnR_comm->ReqID, dnR_comm->CallerReqID);

        handle_DNAck_IOError(dn_req, pkt_response_type);
        break;
    case DNUDRESP_HBIDERR:
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN DN hbid err (type=%d) dnid=%u payload id=%u\n",
                pkt_response_type, dnR_comm->ReqID, dnR_comm->CallerReqID);
        handle_DNAck_Read(dn_req, NULL, pkt_response_type);
        break;
    default:
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN req_id = %u, unknown responseType = %d\n",
                dnR_comm->CallerReqID, pkt_response_type);
        DMS_WARN_ON(true);
        break;
    }
}

/*
 * _process_dnAck: process dn ack by find corresponding dn request from request pool
 *                 and process ack for this request
 * @dnAck: target ack to be added to worker queue
 *
 */
static void _process_dnAck (DNAck_item_t *dnAck)
{
    DNUData_Req_t *dn_req;
    ReqCommon_t *dnReq_comm;
    CN2DN_UDRWResp_hder_t *pkt;
    uint64_t t_rcvDNResp_memAck;
    DNUDResp_opcode_t acktype;
    uint16_t ack_subtype;

#if DN_PROTOCOL_VERSION == 1
    if (unlikely(dnAck->pkt.magic_num1 != MAGIC_NUM ||
            dnAck->pkt.magic_num2 != MAGIC_NUM)) {
#else
    if (unlikely(dnAck->pkt.magic_num1 != DN_MAGIC_S ||
            dnAck->pkt.magic_num2 != DN_MAGIC_E)) {
#endif
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN the magic number wrong, packet lost order\n");
        add_hk_task(TASK_DN_CONN_RESET, (uint64_t *)dnAck->conn_entry);
        return;
    }

    pkt = &dnAck->pkt;
    acktype = dnAck->pkt.responseType;

    dn_req = find_DNUDReq_ReqPool(pkt->dnid, acktype);
    if (IS_ERR_OR_NULL(dn_req)) {
        dms_printk(LOG_LVL_INFO,
                "DMSC INFO fail find dn req dnid %u pReqID %u\n",
                pkt->dnid, pkt->pReqID);
        _handle_dn_miss(dnAck);
        return;
    }

    dnReq_comm = &dn_req->dnReq_comm;
    switch (acktype) {
    case DNUDRESP_MEM_ACK:
    case DNUDRESP_WRITEANDGETOLD_MEM_ACK:
        dn_req->t_rcvReqAck_mem = dnAck->receive_time;
        break;
    default:
        dnReq_comm->t_rcvReqAck = dnAck->receive_time;
        break;
    }

    if (acktype == DNUDRESP_READ_PAYLOAD ||
            acktype == DNUDRESP_READ_NO_PAYLOAD ||
            acktype == DNUDRESP_DISK_ACK ||
            acktype == DNUDRESP_WRITEANDGETOLD_DISK_ACK) {

        if (acktype == DNUDRESP_READ_PAYLOAD ||
                acktype == DNUDRESP_READ_NO_PAYLOAD){
            t_rcvDNResp_memAck = 0;
            ack_subtype = DN_RESPONSE_SUB_TYPE_Read;
        } else {
            t_rcvDNResp_memAck = dn_req->t_rcvReqAck_mem;
            ack_subtype = DN_RESPONSE_SUB_TYPE_Write;
        }

        dnReq_comm->t_rcvReqAck = dnAck->receive_time;

        add_req_response_time(dnReq_comm->t_sendReq, dnReq_comm->t_rcvReqAck,
                t_rcvDNResp_memAck, RES_TYPE_DN, ack_subtype,
                dn_req->ipaddr, dn_req->port,
                (dn_req->num_hbids << BIT_LEN_DMS_LB_SIZE));

        if (unlikely(dms_client_config->enable_err_inject_test)) {
            if (_handle_eij(pkt, dn_req)) {
                DNUDReq_deref_Req(dn_req);
                return;
            }

            //_handle_eij might update the pkt->responseType, re-read it
            acktype = pkt->responseType;
        }
    }

    _handle_dn_resp(acktype, dnAck, dn_req);
}

/*
 * DNAckMgr_submit_ack: add an ack to worker queue
 * @dnAck: target ack to be added to worker queue
 *
 */
void DNAckMgr_submit_ack (DNAck_item_t *dnAck)
{
    struct list_head *enq_list;
    spinlock_t *enq_lock;
    int32_t enq_index;

    if (unlikely(dnAck->in_dnAck_workq)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN submit ack twice dnid=%u nnid=%u type=%d\n",
                dnAck->pkt.dnid, dnAck->pkt.pReqID, dnAck->pkt.responseType);
        return;
    }

    //enq_index = param->pkt->dnid % DN_RX_NUM_THREADS;
    enq_index = dnAck->pkt.dnid & DNACK_ENQ_MOD_NUMBER;
    enq_list = &DNAck_mgr.dnAck_queue[enq_index];
    enq_lock = &DNAck_mgr.dnAck_qlock[enq_index];

    spin_lock(enq_lock);
    list_add_tail(&dnAck->entry_ackpool, enq_list);
    dnAck->in_dnAck_workq = true;
    atomic_inc(&DNAck_mgr.dnAck_qsize);
    spin_unlock(enq_lock);

    if (waitqueue_active(&DNAck_mgr.dnAck_waitq)) {
        wake_up_interruptible_all(&DNAck_mgr.dnAck_waitq);
    }
}

/*
 * _peek_DNAck_workq: peek a ack from ack queue
 * @ack_mgr: which ack manager that has queue for peek from
 * @deq_list: which queue that has ack to peek
 * @deq_lock: queue lock
 *
 * Return: ack item from queue
 */
static DNAck_item_t *_peek_DNAck_workq (DNAckWker_Mgr_t *ack_mgr,
        struct list_head *deq_list, spinlock_t *deq_lock)
{
    DNAck_item_t *dnAck;

    dnAck = NULL;

    spin_lock(deq_lock);

    dnAck = list_first_entry_or_null(deq_list, DNAck_item_t, entry_ackpool);
    if (IS_ERR_OR_NULL(dnAck) == false) {

        list_del(&dnAck->entry_ackpool);
        dnAck->in_dnAck_workq = false;
        atomic_dec(&ack_mgr->dnAck_qsize);
    }
    spin_unlock(deq_lock);

    return dnAck;
}

/*
 * DNAck_worker_fn: worker function for worker thread to run
 */
static int32_t DNAck_worker_fn (void *data)
{
    dmsc_thread_t *t_data;
    DNAck_wker_t *wker;
    DNAckWker_Mgr_t *ack_mgr;
    struct list_head *deq_list;
    spinlock_t *deq_lock;
    DNAck_item_t *myack;

    t_data = (dmsc_thread_t *)data;
    wker = (DNAck_wker_t *)t_data->usr_data;
    ack_mgr = (DNAckWker_Mgr_t *)wker->wker_private_data;

    deq_list = &ack_mgr->dnAck_queue[wker->DNAck_handle_qidx];
    deq_lock = &ack_mgr->dnAck_qlock[wker->DNAck_handle_qidx];
    atomic_inc(&ack_mgr->num_DNAckWorker);

    while (t_data->running) {
        wait_event_interruptible(*(t_data->ctl_wq),
                atomic_read(&ack_mgr->dnAck_qsize) > 0 || !t_data->running);
        myack = _peek_DNAck_workq(ack_mgr, deq_list, deq_lock);
        if (unlikely(IS_ERR_OR_NULL(myack))) {
            continue;
        }

        _process_dnAck(myack);
        myack->conn_entry = NULL;
        DNAckMgr_free_ack(myack);
        myack = NULL;
    }

    atomic_dec(&ack_mgr->num_DNAckWorker);

    return 0;
}

/*
 * _create_DNAck_worker: create worker queue and assign the queue index for worker handling
 * @wker: which worker to created
 * @qidx: which queue index for this worker to handle
 * @wker_mgr: ack manager that worker belong to
 * Return: create result 0 : OK
 *                      ~0 : failure
 */
static int32_t _create_DNAck_worker (DNAck_wker_t *wker, int32_t qidx,
        DNAckWker_Mgr_t *wker_mgr)
{
    int8_t nm_thd[MAX_THREAD_NAME_LEN];
    int32_t ret;

    ret = 0;

    wker->DNAck_handle_qidx = qidx;
    wker->wker_private_data = wker_mgr;

    memset(nm_thd, '\0', MAX_THREAD_NAME_LEN);
    sprintf(nm_thd, "dmsDNAck_wker%d", qidx);
    wker->DNAck_wker_thd = create_worker_thread(nm_thd, &wker_mgr->dnAck_waitq,
            (void *)wker, DNAck_worker_fn);
    if (IS_ERR_OR_NULL(wker->DNAck_wker_thd)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN create DNAck fail %s\n", nm_thd);
        ret = -ESRCH;
    }

    return ret;
}

/*
 * DNAckMgr_init_manager: initialize DNAck_mgr
 *                        initialize ack queue in DNAck_mgr
 *                        create all ack worker and register memory pool for ack
 * Return: release result: 0 : OK
 *                        ~0 : failure
 */
int32_t DNAckMgr_init_manager (void)
{
    int32_t ret, i, num_req_size;

    ret = 0;

    dms_printk(LOG_LVL_INFO, "DMSC INFO init DNAck worker manager\n");

    atomic_set(&DNAck_mgr.dnAck_qsize, 0);
    init_waitqueue_head(&DNAck_mgr.dnAck_waitq);
    for (i = 0; i < NUM_DNACK_WORKER; i ++) {
        spin_lock_init(&DNAck_mgr.dnAck_qlock[i]);
        INIT_LIST_HEAD(&DNAck_mgr.dnAck_queue[i]);
    }

    atomic_set(&DNAck_mgr.num_DNAckWorker, 0);

    num_req_size = dms_client_config->max_nr_io_reqs*MAX_LBS_PER_RQ;
    DNAck_mgr.DNAck_mpool_ID = register_mem_pool("DNAck_item", num_req_size,
            sizeof(DNAck_item_t), true);
    if (DNAck_mgr.DNAck_mpool_ID < 0) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN fail register dn ack mem pool %d\n",
                DNAck_mgr.DNAck_mpool_ID);
        return -ENOMEM;
    }
    dms_printk(LOG_LVL_INFO, "DMSC INFO DN Ack manager get mem pool id %d\n",
            DNAck_mgr.DNAck_mpool_ID);

    for (i = 0; i < NUM_DNACK_WORKER; i++) {
        ret = _create_DNAck_worker(&DNAck_mgr.DNCAck_wker[i], i, &DNAck_mgr);
        if (ret) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN DNAck manager fail create ack worker\n");
            break;
        }
    }

    dms_printk(LOG_LVL_INFO, "DMSC INFO init DNAck worker manager DONE\n");
    return ret;
}


/*
 * _clean_all_DNAck: clean all ack in ack queue when release module
 * @dnAckMgr: ack manager that has ack worker queue
 *
 * TODO: implement me
 */
static void _clean_all_DNAck (DNAckWker_Mgr_t *dnAckMgr)
{
    return;
}

/*
 * DNAckMgr_rel_manager: stop all ack worker and deregister ack memory pool
 *                       release DNAck_mgr
 * Return: release result: 0 : OK
 *                        ~0 : failure
 */
int32_t DNAckMgr_rel_manager (void)
{
    int32_t ret, count, num_worker, i;

    ret = 0;

    dms_printk(LOG_LVL_INFO, "DMSC INFO rel DNAck worker manager\n");
    num_worker = atomic_read(&DNAck_mgr.num_DNAckWorker);
    for (i = 0; i < num_worker; i++) {
        if (stop_worker_thread(DNAck_mgr.DNCAck_wker[i].DNAck_wker_thd)) {
            dms_printk(LOG_LVL_WARN, "fail to stop DN Ack worker %d\n", i);
            DMS_WARN_ON(true);
        }
    }

    count = 0;
    while (atomic_read(&DNAck_mgr.num_DNAckWorker) != 0) {
        msleep(100);
        count++;

        if (count == 0 || count % 10 != 0) {
            continue;
        }

        dms_printk(LOG_LVL_WARN,
                "DMSC WARN fail stop DN Ack worker %d thread running "
                "(wait count %d)\n",
                atomic_read(&DNAck_mgr.num_DNAckWorker), count);
    }

    _clean_all_DNAck(&DNAck_mgr);

    ret = deregister_mem_pool(DNAck_mgr.DNAck_mpool_ID);

    dms_printk(LOG_LVL_INFO, "DMSC INFO rel DNAck worker manager DONE\n");

    return ret;
}
