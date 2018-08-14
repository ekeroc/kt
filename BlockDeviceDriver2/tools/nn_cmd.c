/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * nn_cmd.c
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <syslog.h>
#include "common.h"
#include "list.h"
#include "nn_cmd.h"
#include "nn_cmd_private.h"
#include "discoadm.h"
#include "socket_util.h"
#include "Volume_Manager.h"
#include "Metadata_Manager.h"

static volume_device_t *query_vol_info (uint32_t nnip, int32_t port,
        uint16_t opcode, uint32_t para)
{
    reqpkt_attvol_t pkt_req;
    rsppkt_attvol_t pkt_rsp;
    volume_device_t *vol_inst;
    socket_t sk;
    int32_t rexp_len, rcv_len;
    int8_t ipstr[IPADDR_STR_LEN];

    pkt_req.mnum_s = nncmd_magic_s_netorder;
    pkt_req.opcode = htons(opcode);
    pkt_req.vol_id = htonl(para);

    disco_inet_aton(nnip, ipstr, IPADDR_STR_LEN);

    sk = socket_connect_to(ipstr, port);
    if (sk < 0) {
        info_log("discoadm fail conn nn %s:%d\n", ipstr, port);
        return NULL;
    }

    if (socket_sendto(sk, (int8_t *)(&pkt_req), sizeof(reqpkt_attvol_t)) < 0) {
        info_log("discoadm fail send vol to nn %s:%d\n", ipstr, port);
        return NULL;
    }

    pkt_rsp.vol_cap = 0;
    pkt_rsp.num_replica = 0;
    pkt_rsp.is_share = false;

    rexp_len = sizeof(rsppkt_attvol_t);
    rcv_len = rexp_len;
    if (socket_recvfrom(sk, (int8_t *)(&pkt_rsp), rcv_len) < rexp_len) {
        info_log("discoadm fail rcv qvol info rsp from nn %s:%d\n",
                ipstr, port);
        return NULL;
    }

    pkt_rsp.vol_cap = ntohll(pkt_rsp.vol_cap);
    pkt_rsp.num_replica = ntohl(pkt_rsp.num_replica);
    pkt_rsp.is_share = ntohs(pkt_rsp.is_share);

    vol_inst = create_vol_inst(para, pkt_rsp.vol_cap);

    if (vol_inst != NULL) {
        set_vol_share(vol_inst, pkt_rsp.is_share);
        set_vol_replica(vol_inst, pkt_rsp.num_replica);
    }

    socket_close(sk);

    return vol_inst;
}

static void *_detach_vol_nn (uint32_t nnip, int32_t port,
        uint16_t opcode, uint32_t para)
{
    reqpkt_detvol pkt_req;
    socket_t sk;
    int32_t rexp_len, rcv_len;
    int8_t ipstr[IPADDR_STR_LEN];

    pkt_req.mnum_s = nncmd_magic_s_netorder;
    pkt_req.opcode = htons(opcode);
    pkt_req.vol_id = htonl(para);

    disco_inet_aton(nnip, ipstr, IPADDR_STR_LEN);

    sk = socket_connect_to(ipstr, port);
    if (sk < 0) {
        info_log("discoadm fail conn nn %s:%d\n", ipstr, port);
        return NULL;
    }

    if (socket_sendto(sk, (int8_t *)(&pkt_req), sizeof(reqpkt_attvol_t)) < 0) {
        info_log("discoadm fail send vol to nn %s:%d\n", ipstr, port);
        return NULL;
    }

    socket_close(sk);
}

static uint32_t _vol_op_nn (uint32_t nnip, int32_t port, uint16_t opcode,
        uint64_t para)
{
    reqpkt_nnvolop_t pkt_req;
    rsppkt_nnvolop_t pkt_rsp;
    socket_t sk;
    int32_t rexp_len, rcv_len;
    int8_t ipstr[IPADDR_STR_LEN];


    pkt_req.mnum_s = nncmd_magic_s_netorder;
    pkt_req.opcode = htons(opcode);
    pkt_req.vol_para = htonll(para);
    pkt_rsp.vol_rsp = 0;

    disco_inet_aton(nnip, ipstr, IPADDR_STR_LEN);

    sk = socket_connect_to(ipstr, port);
    if (sk < 0) {
        info_log("discoadm fail conn nn %s:%d\n", ipstr, port);
        return (pkt_rsp.vol_rsp);
    }

    if (socket_sendto(sk, (int8_t *)(&pkt_req), sizeof(reqpkt_nnvolop_t)) < 0) {
        info_log("discoadm fail send vol to nn %s:%d\n", ipstr, port);
        return (pkt_rsp.vol_rsp);
    }

    rexp_len = sizeof(rsppkt_nnvolop_t);
    rcv_len = rexp_len;
    if (socket_recvfrom(sk, (int8_t *)(&pkt_rsp), rcv_len) < rexp_len) {
        info_log("discoadm fail rcv cvol rsp from nn %s:%d\n", ipstr, port);
        return 0;
    }

    pkt_rsp.vol_rsp = ntohll(pkt_rsp.vol_rsp);

    socket_close(sk);

    printf("result: %llu\n", pkt_rsp.vol_rsp);
    return pkt_rsp.vol_rsp;
}

/*
 * ret  true: set nn cmd opt success
 *     false: set nn cmd opt fail
 */
bool set_nncmd_opt (int32_t argc, int8_t *argv[], uint32_t s_index,
        disco_cmd_t *cmd)
{
    uint64_t *volop_data;
    cmdpara_dumpMD_t *voldump_data;
    uint32_t fn_len;
    bool ret;

    ret = true;
    switch (cmd->cmd_op) {
    case CMD_CREATE_VOL:
    case CMD_CLONE_VOL:
    case CMD_DELETE_VOL:
        volop_data = (uint64_t *)malloc(sizeof(uint64_t));
        *volop_data = atoll(argv[s_index]);
        cmd->cmd_data = (void *)volop_data;
        break;
    case CMD_DO_VOL_RR:
    case CMD_DUMP_VOL_MD:
        voldump_data = (cmdpara_dumpMD_t *)malloc(sizeof(cmdpara_dumpMD_t));
        voldump_data->vol_id = atoll(argv[s_index]);

        if (argc == 3 || argc == 5) {
            voldump_data->filename = (int8_t *)malloc(sizeof(int8_t)*DEFAULT_FN_LEN);
            sprintf(voldump_data->filename, "%s_md.dump", argv[s_index]);
            voldump_data->filename[strlen(argv[s_index - 1]) + 8] = '\0';
        } else if (argc == 4 || argc >= 6) {
            fn_len = strlen(argv[s_index]);
            voldump_data->filename = (int8_t *)malloc(sizeof(int8_t)*fn_len);
            strcpy(voldump_data->filename, argv[s_index + 1]);
        }

        if (argc == 3 || argc == 4) {
            voldump_data->startLB = 0;
            voldump_data->LB_len = 0;
        } else if (argc == 5) {
            voldump_data->startLB = atoll(argv[s_index + 1]);
            voldump_data->LB_len = atoll(argv[s_index + 2]);
        } else {
            voldump_data->startLB = atoll(argv[s_index + 2]);
            voldump_data->LB_len = atoll(argv[s_index + 3]);
        }

        cmd->cmd_data = (void *)voldump_data;
        break;
    default:
        ret = false;
        break;
    }

    return ret;
}

static uint32_t get_nnip_by_hostname (int8_t *hname)
{
    struct hostent *nnhost;
    struct sockaddr_in sin;

    nnhost = (struct hostent *)gethostbyname(hname);

    if (nnhost == NULL) {
        return 0;
    }

    memcpy(&sin.sin_addr.s_addr, nnhost->h_addr, nnhost->h_length);

    return (uint32_t)sin.sin_addr.s_addr;
}

static int16_t get_nnvol_opcode (disco_cmd_op_t cmdop)
{
    int16_t ret;

    ret = -1;
    switch (cmdop) {
    case CMD_CREATE_VOL:
        ret = nnvolop_create_vol;
        break;
    case CMD_CLONE_VOL:
        ret = nnvolop_clone_vol;
        break;
    case CMD_DELETE_VOL:
        ret = nnvolop_delete_vol;
        break;
    case CMD_DUMP_VOL_MD:
    case CMD_DO_VOL_RR:
        ret = nnvolop_attach_vol; //TODO should change to query volume information
        break;
    default:
        break;
    }

    return ret;
}

static int32_t dump_md2file (void *data, disco_NN_metadata_t *md_data)
{
    volume_device_t *vol_inst;
    FILE *fp;
    uint32_t i, j, k, rbid, len, offset;
    md_item_t *item;
    int8_t ipstr[IPADDR_STR_LEN];

    vol_inst = (volume_device_t *)data;
    fp = (FILE *)vol_inst->user_data;

    if (md_data->num_md_items == 0) {
        fprintf(fp, "%llu %d NN reply exception\n",
                md_data->md_req->startLB, md_data->md_req->LBlens);
        goto DUMP_DONE;
    }

    for (i = 0; i < md_data->num_md_items; i++) {
        item = &md_data->md_items[i];

        if (item->state == MDLOC_IS_INVALID) {
            continue;
        }

        //if (item->num_replica >= vol_inst->vol_feature.prot_method.replica_factor) {
        //    continue;
        //}

        //if (item->num_replica >= )
        fprintf(fp, "%llu %d %d %d\n",
                item->LB_s, item->LB_len, item->num_replica, item->state);
        fprintf(fp, "   ");
        for (j = 0; j < item->LB_len; j++) {
            fprintf(fp, " %llu", item->HBIDs[j]);
        }
        fprintf(fp, "\n");

        fprintf(fp, "   ");
        for (j = 0; j < item->num_replica; j++) {
            disco_inet_aton(item->dnLocs[j].ipaddr, ipstr, IPADDR_STR_LEN);
            fprintf(fp, "%s %d",
                    ipstr, item->dnLocs[j].port);

            for (k = 0; k < item->dnLocs[j].num_phy_locs; k++) {
                decompose_triple(item->dnLocs[j].phy_locs[k],
                        &rbid, &len, &offset);
                fprintf(fp, " %llu (%d %d %d)",
                        item->dnLocs[j].phy_locs[k], rbid, offset, len);
            }
            fprintf(fp, "\n");
        }
    }

DUMP_DONE:
    (vol_inst->num_reqs)--;

    return 0;
}

static void _dump_vol_md_nn (volume_device_t *vol_inst, int8_t *fn,
        uint64_t LBs, uint64_t LBl)
{
    uint64_t max_LB;
    uint64_t startLB, LBlen;
    uint64_t prg_range;

    max_LB = vol_inst->capacity >> LB_SIZE_BIT_LEN;

    startLB = LBs;
    if (LBl > 0) {
        max_LB = (max_LB >= (LBs + LBl)) ? (LBs + LBl) : max_LB;
    }

    prg_range = 256*1024;
    info_log("discoadm dumpmd volume %d startLB:%d max_LB: %d\n",
            vol_inst->vol_id, LBs, max_LB);
    while (startLB < max_LB) {
        LBlen = (32 >= max_LB - startLB) ? (max_LB - startLB) : 32;
        vol_inst->num_reqs++;

        if (startLB != 0 && startLB % prg_range == 0) {
            info_log("discoadm dumpmd volume %d LB range (%llu %llu) done\n",
                    vol_inst->vol_id, startLB - prg_range, startLB - 1);
        }

        //printf("handle range %llu %llu %d\n", startLB, LBlen, vol_inst->num_reqs);
        if (vol_inst->num_reqs > 128) {
            usleep(1000*1000);
        }

        gen_and_do_mdreq(vol_inst->vol_id, REQ_MD_Q_FOR_READ, startLB, LBlen,
                (void *)vol_inst, dump_md2file);

        startLB += LBlen;
    }

    while (vol_inst->num_reqs > 0) {
        usleep(100*1000);
    }

    info_log("discoadm dumpmd volume %d startLB:%d max_LB: %d done\n",
            vol_inst->vol_id, LBs, max_LB);
}

static int32_t dump_RR2file (void *data, disco_NN_metadata_t *md_data)
{
    return dump_md2file(data, md_data);
}

static int32_t _get_report_req (void *data, disco_NN_metadata_t *md_data)
{
    bool doRR;
    uint32_t i;
    uint16_t vol_replica;
    md_item_t *item;
    volume_device_t *vol_inst;
    struct md_request *md_req;

    md_req = md_data->md_req;
    doRR = false;
    vol_inst = (volume_device_t *)data;

    vol_replica = vol_inst->vol_feature.prot_method.replica_factor;

    for (i = 0; i < md_data->num_md_items; i++) {
        item = &md_data->md_items[i];

        if (item->state == MDLOC_IS_INVALID) {
            continue;
        }

        if (item->num_replica >= vol_replica) {
            continue;
        }

        set_mdreq_stat(md_req, REQ_INIT);
        set_mdreq_op(md_req, REQ_MD_REPORT_WRITE_STATUS);
        md_req->md_rsp_fn = dump_RR2file;
        doRR = true;

        add_mdreq(md_req);
        break;
    }

    if (!doRR) {
        (vol_inst->num_reqs)--;
    }

    return 0;
}

static void _do_vol_RR (volume_device_t *vol_inst, int8_t *fn,
        uint64_t LBs, uint64_t LBl)
{
    uint64_t max_LB;
    uint64_t startLB, LBlen;
    uint64_t prg_range;

    max_LB = vol_inst->capacity >> LB_SIZE_BIT_LEN;

    startLB = LBs;
    if (LBl > 0) {
        max_LB = (max_LB >= (LBs + LBl)) ? (LBs + LBl) : max_LB;
    }

    prg_range = 256*1024;
    info_log("discoadm doRR volume %d startLB:%d max_LB: %d\n",
                vol_inst->vol_id, LBs, max_LB);
    while (startLB < max_LB) {
        LBlen = (32 >= max_LB - startLB) ? (max_LB - startLB) : 32;

        vol_inst->num_reqs++;

        if (startLB != 0 && startLB % prg_range == 0) {
            info_log("discoadm doRR volume %d LB range (%llu %llu) done\n",
                    vol_inst->vol_id, startLB - prg_range, startLB - 1);
        }

        if (vol_inst->num_reqs > 128) {
            usleep(100*1000);
        }

        gen_and_do_mdreq(vol_inst->vol_id, REQ_MD_Q_FOR_READ, startLB, LBlen,
                (void *)vol_inst, _get_report_req);

        startLB += LBlen;
    }

    while (vol_inst->num_reqs > 0) {
        usleep(100*1000);
    }

    info_log("discoadm doRR volume %d startLB:%d max_LB: %d done\n",
            vol_inst->vol_id, LBs, max_LB);
}

static int32_t dump_volInfo2file (volume_device_t *vol_inst)
{
    FILE *fp;

    fp = (FILE *)vol_inst->user_data;
    fprintf(fp, "%d %llu %d\n",
                    vol_inst->vol_id, vol_inst->capacity >> 12,
                    vol_inst->vol_feature.prot_method.replica_factor);
}

uint32_t execute_nn_cmd (disco_cmd_t *cmd)
{
    volume_device_t *vol_inst;
    cmdpara_dumpMD_t *voldump_data;
    cmdpara_doRR_t *volRR_data;
    FILE *fp;

    if (DISCO_NN != cmd->cmd_comp) {
        return 0;
    }

    if (0 == cmd->target_ip) {
        cmd->target_ip = get_nnip_by_hostname(DEFAULT_NN_HOSTNAME);
    }

    switch (cmd->cmd_op) {
    case CMD_CREATE_VOL:
    case CMD_CLONE_VOL:
    case CMD_DELETE_VOL:
        _vol_op_nn(cmd->target_ip, cmd->target_port,
                get_nnvol_opcode(cmd->cmd_op), *((uint64_t *)cmd->cmd_data));
        break;
    case CMD_DUMP_VOL_MD:
        voldump_data = (cmdpara_dumpMD_t *)cmd->cmd_data;
        vol_inst = query_vol_info(cmd->target_ip, cmd->target_port,
                get_nnvol_opcode(cmd->cmd_op), voldump_data->vol_id);

        if (vol_inst != NULL) {
            fp = fopen(voldump_data->filename, "w");
            vol_inst->user_data = (void *)(fp);
            dump_volInfo2file(vol_inst);
            init_md_manager(cmd->target_ip, cmd->target_port);
            _dump_vol_md_nn(vol_inst, voldump_data->filename,
                    voldump_data->startLB, voldump_data->LB_len);
            destroy_md_manager();
            fclose(fp);

            _detach_vol_nn(cmd->target_ip, cmd->target_port,
                    nnvolop_detach_vol, voldump_data->vol_id);

            free(vol_inst);
        }
        break;
    case CMD_DO_VOL_RR:
        volRR_data = (cmdpara_doRR_t *)cmd->cmd_data;
        vol_inst = query_vol_info(cmd->target_ip, cmd->target_port,
                get_nnvol_opcode(cmd->cmd_op), volRR_data->vol_id);

        if (vol_inst != NULL) {
            fp = fopen(volRR_data->filename, "w");
            vol_inst->user_data = (void *)(fp);
            init_md_manager(cmd->target_ip, cmd->target_port);
            _do_vol_RR(vol_inst, volRR_data->filename,
                    volRR_data->startLB, volRR_data->LB_len);
            destroy_md_manager();
            fclose(fp);

            _detach_vol_nn(cmd->target_ip, cmd->target_port,
                    nnvolop_detach_vol, volRR_data->vol_id);

            free(vol_inst);
        }

        break;
    default:
        //TODO: display error message
        break;
    }

    return 0;
}

void free_nncmd_data (disco_cmd_t *cmd)
{
    cmdpara_dumpMD_t *voldump_data;

    switch (cmd->cmd_op) {
    case CMD_DUMP_VOL_MD:
    case CMD_DO_VOL_RR:
        voldump_data = (cmdpara_dumpMD_t *)cmd->cmd_data;
        if (voldump_data->filename != NULL) {
            free(voldump_data->filename);
            voldump_data->filename = NULL;
        }
        break;
    default:
        break;
    }

    free(cmd->cmd_data);
}

/*
 * attach
 * detach
 * force detach
 * stop / start
 * block / unblock
 * clear md cache
 * clear ovw
 * set vol fea
 * extend vol
 * RR auto on/off
 *
 */

int32_t get_nn_port ()
{
    return DEFAULT_NN_PORT;
}

int32_t init_nncmd_module ()
{
    nncmd_magic_s = NN_MAGIC_S;
    nncmd_magic_e = NN_MAGIC_E;

    nncmd_magic_s_netorder = htonll(NN_MAGIC_S);
    nncmd_magic_e_netorder = htonll(NN_MAGIC_E);

    return 0;
}
