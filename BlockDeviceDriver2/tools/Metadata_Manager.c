/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * Metadata_Manager.c
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <syslog.h>
#include <pthread.h>
#include <string.h>
#include "list.h"
#include "common.h"
#include "socket_util.h"
#include "Metadata_Manager_private.h"
#include "Metadata_Manager.h"
#include "nn_cmd.h"

static void _free_md(disco_NN_metadata_t *md);

static uint32_t get_mdreq_id ()
{
    return g_mdreq_id++;
}

static void free_md_req (mdreq_t *md_req)
{
    free(md_req);
}

void set_mdreq_stat (mdreq_t *md_req, mdreq_stat stat)
{
    md_req->stat = stat;
}

void set_mdreq_op (mdreq_t *md_req, mdreq_op_t op)
{
    md_req->op = op;
}

static mdreq_t *create_md_req (uint64_t vol_id, mdreq_op_t op,
        uint64_t startLB, uint32_t LBlens,
        void *user_data, int32_t (*md_rsp_fn)(void *data, disco_NN_metadata_t *md_data))
{
    mdreq_t *md_req;

    md_req = (mdreq_t *)malloc(sizeof(mdreq_t));

    if (md_req == NULL) {
        return md_req;
    }

    md_req->vol_id = vol_id;
    md_req->req_id = get_mdreq_id();
    md_req->startLB = startLB;
    md_req->LBlens = LBlens;
    md_req->user_data = user_data;
    md_req->md_rsp_fn = md_rsp_fn;

    INIT_LIST_HEAD(&md_req->list_pq);
    INIT_LIST_HEAD(&md_req->list_wq);

    md_req->op = op;
    md_req->stat = REQ_INIT;

    return md_req;
}

static void add_to_pendq (mdreq_t *md_req)
{
    pthread_mutex_lock(&mdreq_pq_lock);
    list_add_tail(&md_req->list_pq, &mdreq_pendq);
    num_req_pq++;
    pthread_mutex_unlock(&mdreq_pq_lock);
}

static mdreq_t *query_pendq (uint64_t reqid)
{
    mdreq_t *mdreq, *cur, *next;

    mdreq = NULL;

    pthread_mutex_lock(&mdreq_pq_lock);
    list_for_each_entry_safe (cur, next, &mdreq_pendq, list_pq) {
        if (cur == NULL) {
            break;
        }

        if (cur->req_id == reqid) {
            mdreq = cur;
            break;
        }
    }
    pthread_mutex_unlock(&mdreq_pq_lock);

    return mdreq;
}

static void rm_from_pendq (mdreq_t *md_req)
{
    pthread_mutex_lock(&mdreq_pq_lock);
    list_del(&md_req->list_pq);
    num_req_pq--;
    md_req->stat = REQ_DONE;
    pthread_mutex_unlock(&mdreq_pq_lock);
}

static void enq_to_workq (mdreq_t *md_req)
{
    pthread_mutex_lock(&mdreq_wq_lock);
    list_add_tail(&md_req->list_wq, &mdreq_workq);
    num_req_wq++;
    md_req->stat = REQ_WAIT_FOR_HANDLE;
    pthread_mutex_unlock(&mdreq_wq_lock);

    /*
     * TODO wakeup worker thread
     */

    if (num_req_wq >= MAX_NUM_NN_REQ) {
        usleep(100*1000);
    }
}

static mdreq_t *deq_from_workq ()
{
    mdreq_t *mdreq, *cur, *next;

    mdreq = NULL;

    //TODO Change to wait and wakeup mode
    while (num_req_wq <= 0 && running) {
        usleep(100*1000);
    }

    pthread_mutex_lock(&mdreq_wq_lock);
    list_for_each_entry_safe (cur, next, &mdreq_workq, list_wq) {
        if (cur == NULL) {
            break;
        }

        mdreq = cur;
        break;
    }

    if (mdreq != NULL) {
        list_del(&mdreq->list_wq);
        num_req_wq--;
        mdreq->stat = REQ_WAIT_FOR_SEND;
    }

    pthread_mutex_unlock(&mdreq_wq_lock);

    return mdreq;
}

bool gen_and_do_mdreq (uint64_t vol_id, mdreq_op_t op,
        uint64_t startLB, uint32_t LBlen,
        void *user_data, int32_t (*md_rsp_fn)(void *data, disco_NN_metadata_t *md_data))
{
    mdreq_t *md_req;

    md_req = create_md_req(vol_id, op, startLB, LBlen, user_data, md_rsp_fn);

    if (md_req == NULL) {
        return false;
    }

    add_to_pendq(md_req);
    enq_to_workq(md_req);

    return true;
}

bool add_mdreq (mdreq_t *md_req)
{
    add_to_pendq(md_req);
    enq_to_workq(md_req);

    return true;
}

static uint32_t gen_mdreq_pkt (mdreq_t *mdreq, uint8_t *buffer)
{
    mdreq_pkt_t pkt;

    pkt.rw = htons(mdreq->op);
    pkt.LB_cnt = htonl(mdreq->LBlens);
    pkt.nnreq_id = htonll(mdreq->req_id);
    pkt.startLB = htonll(mdreq->startLB);
    pkt.vol_id = htonll(mdreq->vol_id);
    pkt.magic_number1 = mnum_net_order;
    pkt.magic_number2 = mnum_net_order;

    memcpy(buffer, &pkt, sizeof(mdreq_pkt_t));

    return sizeof(mdreq_pkt_t);
}

static uint32_t _get_phyloc_pkt (dn_loc_t *dnloc, uint8_t *buffer)
{
#define REPORT_IPADDR_STR_LEN   20
    uint64_t net64;
    uint32_t pkt_size, net32, i;
    uint8_t *ptr;
    int8_t ipstr[REPORT_IPADDR_STR_LEN];

    pkt_size = 0;
    ptr = buffer;

    memset(ipstr, 0, REPORT_IPADDR_STR_LEN);
    disco_inet_aton(dnloc->ipaddr, ipstr, IPADDR_STR_LEN);
    DMS_MEM_WRITE_NEXT(ptr, ipstr, REPORT_IPADDR_STR_LEN);
    pkt_size += REPORT_IPADDR_STR_LEN;

    net32 = htonl(dnloc->port);
    DMS_MEM_WRITE_NEXT(ptr, &(net32), sizeof(uint32_t));
    pkt_size += sizeof(uint32_t);

    net32 = htonl(dnloc->num_phy_locs);
    DMS_MEM_WRITE_NEXT(ptr, &(net32), sizeof(uint32_t));
    pkt_size += sizeof(uint32_t);

    for (i = 0; i < dnloc->num_phy_locs; i++) {
        net64 = htonl(dnloc->phy_locs[i]);
        DMS_MEM_WRITE_NEXT(ptr, &(net64), sizeof(uint64_t));
        pkt_size += sizeof(uint64_t);
    }

    return pkt_size;
}

static uint32_t _gen_item_pkt (md_item_t *item, uint8_t *buffer)
{
    uint64_t net_64;
    uint32_t pkt_size, net_32, i, dnloc_size;
    uint8_t *ptr;

    pkt_size = 0;
    ptr = buffer;

    net_64 = htonll(item->LB_s);
    DMS_MEM_WRITE_NEXT(ptr, &(net_64), sizeof(uint64_t));
    pkt_size += sizeof(uint64_t);

    net_32 = htonl(item->LB_len);
    DMS_MEM_WRITE_NEXT(ptr, &(net_32), sizeof(uint32_t));
    pkt_size += sizeof(uint32_t);

    for (i = 0; i < item->LB_len; i++) {
        net_64 = htonll(item->HBIDs[i]);
        DMS_MEM_WRITE_NEXT(ptr, &(net_64), sizeof(uint64_t));
        pkt_size += sizeof(uint64_t);
    }

    net_32 = htonl(item->num_replica);
    DMS_MEM_WRITE_NEXT(ptr, &(net_32), sizeof(uint32_t));
    pkt_size += sizeof(uint32_t);

    for (i = 0; i < item->num_replica; i++) {
        dnloc_size = _get_phyloc_pkt(&(item->dnLocs[i]), ptr);
        ptr += dnloc_size;
        pkt_size += dnloc_size;
    }
    return pkt_size;
}

static uint32_t gen_items_pkt (mdreq_t *mdreq, uint8_t *buffer,
        int32_t *numItems)
{
    disco_NN_metadata_t *mdata;
    md_item_t *item;
    uint32_t pkt_size, i, vol_replica, item_size;
    uint8_t *ptr;

    *numItems = 0;
    pkt_size = 0;
    ptr = buffer;
    mdata = mdreq->metadata;
    vol_replica = get_volume_replica(mdreq->req_id);

    for (i = 0; i < mdata->num_md_items; i++) {
        item = &(mdata->md_items[i]);
        if (item->state == MDLOC_IS_INVALID ||
                item->num_replica >= vol_replica) {
            continue;
        }

        (*numItems)++;
        item_size = _gen_item_pkt(item, ptr);
        ptr += item_size;
        pkt_size += item_size;
    }

    *numItems = htonl(*numItems);
    return pkt_size;
}

static uint32_t gen_mdreport_pkt (mdreq_t *mdreq, uint8_t *buffer)
{
    md_cihd_t hder;
    uint32_t pkt_size, pre_hd_size, items_size;
    uint8_t *ptr;

    pkt_size = sizeof(md_cihd_t);
    memset(&hder, 0, pkt_size);
    pre_hd_size = pkt_size - sizeof(uint64_t);
    ptr = buffer;

    hder.mnum_s = mnum_net_order;
    hder.vol_id = htonll(mdreq->vol_id);
    hder.req_id = htonll(mdreq->req_id);
    hder.op = htons(mdreq->op);
    hder.numItems = 0;
    hder.mnum_e = mnum_net_order;

    ptr += pre_hd_size;

    items_size = gen_items_pkt(mdreq, ptr, &(hder.numItems));
    ptr += items_size;
    pkt_size += items_size;

    memcpy(buffer, &hder, pre_hd_size);
    memcpy(ptr, &hder.mnum_e, sizeof(uint64_t));


    return pkt_size;
}

int mdreq_worker (void *data)
{
    mdreq_t *mdreq;
    socket_t *nn_sk;
    uint32_t pkt_len;
    uint8_t sock_buffer[4096];
    bool NoWaitRsp;

    nn_sk = (socket_t *)data;

    while (running) {
        mdreq = deq_from_workq();

        if (mdreq == NULL) {
            continue;
        }

        NoWaitRsp = false;

        switch (mdreq->op) {
        case REQ_MD_Q_FOR_READ:
        case REQ_MD_Q_TRUE_FOR_READ:
        case REQ_MD_ALLOC_AND_GETOLD:
        case REQ_MD_ALLOC:
        case REQ_MD_Q_FOR_WRITE:
            pkt_len = gen_mdreq_pkt(mdreq, sock_buffer);
            break;
        case REQ_MD_REPORT_READ_STATUS:
            NoWaitRsp = true;
            break;
        case REQ_MD_REPORT_WRITE_STATUS:
            pkt_len = gen_mdreport_pkt(mdreq, sock_buffer);
            NoWaitRsp = true;
            break;
        case REQ_MD_REPORT_WRITE_STATUS_AND_REPLY:
            break;
        default:
            NoWaitRsp = true;
            break;
        }

        //printf("discoadm ready to send range %llu %d\n", mdreq->startLB,
        //        mdreq->LBlens);
        if (socket_sendto(*nn_sk, sock_buffer, pkt_len) < 0) {
            info_log("discoadm fail send vol to nn\n");
        }

        if (NoWaitRsp) {
            rm_from_pendq(mdreq);
            mdreq->md_rsp_fn(mdreq->user_data, mdreq->metadata);
            _free_md(mdreq->metadata);
            free_md_req(mdreq);
        }
    }

    return 0;
}

static int32_t _parse_dnloc (int8_t *buffer, dn_loc_t *dnloc)
{
    uint64_t triplet;
    int32_t ptr_move, i;
    int16_t len_hname;
    int8_t *ptr, hname[20];

    ptr_move = 0;
    ptr = buffer;

    DMS_MEM_READ_NEXT(ptr, &(len_hname), sizeof(uint16_t));
    ptr_move += sizeof(uint16_t);
    len_hname = ntohs(len_hname);

    DMS_MEM_READ_NEXT(ptr, hname, len_hname);
    ptr_move += len_hname;
    hname[len_hname] = '\0';
    dnloc->ipaddr = disco_inet_ntoa(hname);

    DMS_MEM_READ_NEXT(ptr, &(dnloc->port), sizeof(uint32_t));
    ptr_move += sizeof(uint32_t);
    dnloc->port = ntohl(dnloc->port);

    DMS_MEM_READ_NEXT(ptr, &(dnloc->num_phy_locs), sizeof(uint16_t));
    ptr_move += sizeof(uint16_t);
    dnloc->num_phy_locs = ntohs(dnloc->num_phy_locs);

    dnloc->phy_locs = (uint64_t *)malloc(sizeof(uint64_t)*dnloc->num_phy_locs);
    for (i = 0; i < dnloc->num_phy_locs; i++) {
        DMS_MEM_READ_NEXT(ptr, &(dnloc->phy_locs[i]), sizeof(uint64_t));
        ptr_move += sizeof(uint64_t);
        dnloc->phy_locs[i] = ntohll(dnloc->phy_locs[i]);
    }

    return ptr_move;
}

static int32_t _parse_md_item (int8_t *buffer, md_item_t *md_item,
        uint64_t startLB)
{
    int32_t i, ptr_move, hblen, dnloc_shift;
    int8_t *ptr;

    ptr = buffer;
    ptr_move = 0;

    md_item->LB_s = startLB;

    DMS_MEM_READ_NEXT(ptr, &(hblen), sizeof(uint16_t));
    md_item->LB_len = ntohs(hblen);
    hblen = md_item->LB_len;
    ptr_move += sizeof(uint16_t);

    md_item->HBIDs = (uint64_t *)malloc(sizeof(uint64_t)*hblen);
    memcpy(md_item->HBIDs, ptr, sizeof(uint64_t)*hblen);
    ptr += (sizeof(uint64_t)*hblen);
    ptr_move += (sizeof(uint64_t)*hblen);

    for (i = 0; i < hblen; i++) {
        md_item->HBIDs[i] = ntohll(md_item->HBIDs[i]);
    }

    DMS_MEM_READ_NEXT(ptr, &(md_item->num_replica), sizeof(uint16_t));
    md_item->num_replica = ntohs(md_item->num_replica);
    ptr_move += sizeof(uint16_t);

    for (i = 0; i < md_item->num_replica; i++) {
        dnloc_shift = _parse_dnloc(ptr, &md_item->dnLocs[i]);
        ptr += dnloc_shift;
        ptr_move += dnloc_shift;
    }

    DMS_MEM_READ_NEXT(ptr, &(md_item->state), sizeof(uint16_t));
    md_item->state = ntohs(md_item->state);

    ptr_move += sizeof(uint16_t);

    return ptr_move;
}

static disco_NN_metadata_t *_parse_mdrsp (int8_t *buffer, int32_t buff_len)
{
    disco_NN_metadata_t *mdata;
    uint64_t startLB;
    int32_t i, ptr_move, num_items;
    int8_t *ptr;

    mdata = (disco_NN_metadata_t *)malloc(sizeof(disco_NN_metadata_t));
    ptr = buffer;

    DMS_MEM_READ_NEXT(ptr, &(mdata->req_id), sizeof(int64_t));
    mdata->req_id = ntohll(mdata->req_id);

    DMS_MEM_READ_NEXT(ptr, &(mdata->rsp_id), sizeof(int64_t));
    mdata->rsp_id = ntohll(mdata->rsp_id);

    DMS_MEM_READ_NEXT(ptr, &(mdata->num_md_items), sizeof(int16_t));
    mdata->num_md_items = ntohs(mdata->num_md_items);

    mdata->md_req = query_pendq(mdata->req_id);

    if (mdata->md_req == NULL) {
        return mdata;
    }

    if (buff_len < 18 || mdata->num_md_items == 0) {
        return mdata;
    }

    startLB = mdata->md_req->startLB;
    num_items = mdata->num_md_items;
    if (num_items > 0) {
        mdata->md_items = (md_item_t *)malloc(sizeof(md_item_t)*num_items);
        for (i = 0; i < num_items; i++) {
            ptr_move = _parse_md_item(ptr, &(mdata->md_items[i]), startLB);
            startLB = startLB + mdata->md_items[i].LB_len;
            ptr += ptr_move;
        }
    }

    //TODO: flow control value

    return mdata;
}

static void _free_md (disco_NN_metadata_t *md)
{
    uint32_t i, j;

    for (i = 0; i < md->num_md_items; i++) {
        free(md->md_items[i].HBIDs);
        for (j = 0; j < md->md_items[i].num_replica; j ++) {
            free(md->md_items[i].dnLocs[j].phy_locs);
        }
    }

    free(md->md_items);
    free(md);
}

static void handle_mdrsp (int8_t *buffer, int32_t buff_len)
{
    disco_NN_metadata_t *md;
    mdreq_t *mdreq;
    uint64_t req_id;

    md = _parse_mdrsp(buffer, buff_len);
    mdreq = md->md_req;
    mdreq->metadata = md;

    if (mdreq != NULL) {
        rm_from_pendq(mdreq);
        mdreq->md_rsp_fn(mdreq->user_data, md);

        if (REQ_DONE == mdreq->stat) {
            free_md_req(mdreq);
            if (md != NULL) {
                _free_md(md);
            }
        } // else { md_rsp_fn change state and keep do something }
    } else {
        err_log("discoadm fail find md req for md resp %llu\n", md->req_id);
        _free_md(md);
    }
}

int mdrsp_worker (void *data)
{
    socket_t *nn_sk;
    mdrsp_pkt_t rsp_hder;
    int32_t rcv_len, rexp_len;
    int8_t buffer[8192];

    nn_sk = (socket_t *)data;

    while (running) {
        rcv_len = sizeof(mdrsp_pkt_t);
        rexp_len = rcv_len;
        if (socket_recvfrom(*nn_sk, (int8_t *)(&rsp_hder), rcv_len) < rexp_len) {
            info_log("discoadm fail rcv md rsp from nn\n");
            //TODO: if errno is 0 and recv return 0, it will has some issue
            //      handle connection broken
            continue;
        }

        rsp_hder.magic_number1 = ntohll(rsp_hder.magic_number1);
        rsp_hder.rsp_type = ntohs(rsp_hder.rsp_type);
        rsp_hder.total_bytes = ntohs(rsp_hder.total_bytes);

        if (rsp_hder.magic_number1 != mnum_net_order ||
                rsp_hder.total_bytes == 0) {
            info_log("discoadm fail get right packet\n");
            continue;
        }

        rexp_len = rsp_hder.total_bytes;
        if (socket_recvfrom(*nn_sk, buffer, rsp_hder.total_bytes) < rexp_len) {
            info_log("discoadm fail get all packet\n");
            continue;
        }

        if (RESPONSE_METADATA_REQUEST == rsp_hder.rsp_type) {
            handle_mdrsp(buffer, rexp_len);
        }
    }

    return 0;
}

void destroy_md_manager ()
{
    running = false;

    if (nn_sock > -1) {
        socket_close(nn_sock);
    }
}

socket_t conn_to_nn (uint32_t nnip, int32_t port)
{
    socket_t sk;
    int8_t ipstr[IPADDR_STR_LEN];

    disco_inet_aton(nnip, ipstr, IPADDR_STR_LEN);

    sk = socket_connect_to(ipstr, port);
    if (sk < 0) {
        info_log("discoadm fail conn nn %s:%d\n", ipstr, port);
    }

    return sk;
}

void init_md_manager (uint32_t nnip, int32_t port)
{
    int32_t ret;

    g_mdreq_id = 0;
    num_req_wq = 0;
    num_req_pq = 0;
    INIT_LIST_HEAD(&mdreq_workq);
    INIT_LIST_HEAD(&mdreq_pendq);
    mnum_net_order = htonll(MAGIC_NUM_MDREQ);

    nn_sock = conn_to_nn(nnip, port);

    /*
     * create thread
     */
    running = true;
    ret = pthread_create(&mdreq_wker_thread, &req_worker_attr, (void*)mdreq_worker,
                    (void *)(&(nn_sock)));
    if (ret != 0) {
        err_log("DMSC WARN fail create thread mdreq_worker!\n");
    }

    ret = pthread_create(&mdrsp_wker_thread, &rsp_worker_attr, (void*)mdrsp_worker,
                    (void *)(&(nn_sock)));
    if (ret != 0) {
        err_log("DMSC WARN fail create thread mdrsp_worker!\n");
    }
}


