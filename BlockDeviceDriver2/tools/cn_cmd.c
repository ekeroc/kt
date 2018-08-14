/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * cn_cmd.c
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
#include "cn_cmd_private.h"
#include "cn_cmd.h"
#include "socket_util.h"

static uint32_t get_new_ReqID ()
{
    uint32_t newID;

    newID = ReqID_Generator;
    ReqID_Generator++;

    return newID;
}

static bool _set_clearovw_opt (int32_t argc, int8_t *argv[], uint32_t s_index,
        disco_cmd_t *cmd)
{
    cmdpara_covw_t *covw_para;

    bool ret;

    ret = true;

    if (s_index >= argc) {
        return false;
    }

    covw_para = (cmdpara_covw_t *)malloc(sizeof(cmdpara_covw_t));
    covw_para->volid = atoll(argv[s_index]);
    covw_para->clearAll = true;
    s_index++;

    if (s_index < argc) {
        covw_para->lb_range.startLB = atoll(argv[s_index]);
        covw_para->clearAll = false;
        s_index++;

        if (s_index < argc) {
            covw_para->lb_range.LBlen = atol(argv[s_index]);
        } else {
            covw_para->lb_range.LBlen = 1;
        }
    }

    cmd->cmd_data = (void *)covw_para;

    return ret;
}

/*
 * ret  true: set nn cmd opt success
 *     false: set nn cmd opt fail
 */
bool set_cncmd_opt (int32_t argc, int8_t *argv[], uint32_t s_index,
        disco_cmd_t *cmd)
{
    uint64_t *volop_data;
    cmdpara_blockIO_t *blockIO_para;
    bool ret;

    ret = true;

    switch (cmd->cmd_op) {
    case CMD_ATTACH_VOL:
    case CMD_DETACH_VOL:
    case CMD_STOP_VOL:
    case CMD_START_VOL:
        volop_data = (uint64_t *)malloc(sizeof(uint64_t));
        *volop_data = atoll(argv[s_index]);
        cmd->cmd_data = (void *)volop_data;
        break;
    case CMD_CLEAR_VOL_OVW:
        ret = _set_clearovw_opt(argc, argv, s_index, cmd);
        break;
    case CMD_CLEAR_VOL_MDCACHE:
        break;
    case CMD_BLOCK_VOL_IO:
        if ((s_index + 1) < argc) {
            blockIO_para =
                    (cmdpara_blockIO_t *)malloc(sizeof(cmdpara_blockIO_t));
            blockIO_para->volid = atoll(argv[s_index]);

            if (strcasecmp("true", argv[s_index + 1]) == 0) {
                blockIO_para->doBlock = true;
            } else if (strcasecmp("false", argv[s_index + 1]) == 0) {
                blockIO_para->doBlock = false;
            } else {
                free(blockIO_para);
                blockIO_para = NULL;
                ret = false;
            }
            cmd->cmd_data = (void *)blockIO_para;

        } else {
            ret = false;
        }

        break;
    case CMD_SET_VOL_FEA:
        break;
    default:
        ret = false;
        break;
    }

    return ret;
}

static uint32_t _vol_op_cn (uint32_t cnip, int32_t port, uint16_t opcode,
        uint64_t para)
{
    reqpkt_cnvolop_t pkt_req;
    rsppkt_cnvolop_t pkt_rsp;
    socket_t sk;
    int32_t rexp_len, rcv_len;
    int8_t ipstr[IPADDR_STR_LEN];

    pkt_req.pktlen = htons(sizeof(reqpkt_cnvolop_t) - 2 + instid_len);
    pkt_req.opcode = htons(opcode);
    pkt_req.vol_para = htonll(para);
    pkt_rsp.vol_rsp = 0;

    disco_inet_aton(cnip, ipstr, IPADDR_STR_LEN);

    sk = socket_connect_to(ipstr, port);
    if (sk < 0) {
        info_log("discoadm fail conn cn %s:%d\n", ipstr, port);
        return (pkt_rsp.vol_rsp);
    }

    if (socket_sendto(sk, (int8_t *)(&pkt_req), sizeof(reqpkt_cnvolop_t)) < 0) {
        info_log("discoadm fail send vol to cn %s:%d\n", ipstr, port);
        return (pkt_rsp.vol_rsp);
    }

    if (socket_sendto(sk, def_instid, instid_len) < 0) {
        info_log("discoadm fail send vol to cn %s:%d\n", ipstr, port);
        return (pkt_rsp.vol_rsp);
    }

    rexp_len = sizeof(rsppkt_cnvolop_t);
    rcv_len = rexp_len;
    if (socket_recvfrom(sk, (int8_t *)(&pkt_rsp), rcv_len) < rexp_len) {
        info_log("discoadm fail rcv cvol rsp from cn %s:%d\n", ipstr, port);
        return 0;
    }

    pkt_rsp.vol_rsp = ntohl(pkt_rsp.vol_rsp);
    socket_close(sk);

    printf("result: %d\n", pkt_rsp.vol_rsp);
    return pkt_rsp.vol_rsp;
}

static uint32_t _blockIO_op_cn (uint32_t cnip, int32_t port, uint16_t opcode,
        uint64_t para)
{
    reqpkt_cnblockIO_t pkt_req;
    rsppkt_cnblockIO_t pkt_rsp;
    socket_t sk;
    int32_t rexp_len, rcv_len;
    int8_t ipstr[IPADDR_STR_LEN];

    pkt_req.pktlen = htons(sizeof(reqpkt_cnblockIO_t) - 2);
    pkt_req.opcode = htons(opcode);
    pkt_req.reqID = htonl(get_new_ReqID());
    pkt_req.vol_id = htonll(para);
    pkt_rsp.ackcode = 0;
    pkt_rsp.reqID = 0;

    disco_inet_aton(cnip, ipstr, IPADDR_STR_LEN);

    sk = socket_connect_to(ipstr, port);
    if (sk < 0) {
        info_log("discoadm fail conn cn %s:%d\n", ipstr, port);
        return (pkt_rsp.ackcode);
    }

    if (socket_sendto(sk, (int8_t *)(&pkt_req), sizeof(reqpkt_cnblockIO_t)) < 0) {
        info_log("discoadm fail send vol to cn %s:%d\n", ipstr, port);
        return (pkt_rsp.ackcode);
    }

    rexp_len = sizeof(rsppkt_cnblockIO_t);
    rcv_len = rexp_len;
    if (socket_recvfrom(sk, (int8_t *)(&pkt_rsp), rcv_len) < rexp_len) {
        info_log("discoadm fail rcv cvol rsp from cn %s:%d\n", ipstr, port);
        return 0;
    }

    pkt_rsp.ackcode = ntohs(pkt_rsp.ackcode);
    pkt_rsp.reqID = ntohl(pkt_rsp.reqID);
    socket_close(sk);

    printf("result: %d\n", pkt_rsp.ackcode);
    return pkt_rsp.ackcode;
}

static uint32_t _clearovw_op_cn (uint32_t cnip, int32_t port, uint16_t opcode,
        cmdpara_covw_t *para)
{
    reqpkt_cncovw_t pkt_req;
    rsppkt_cncovw_t pkt_rsp;
    socket_t sk;
    int32_t rexp_len, rcv_len;
    int8_t ipstr[IPADDR_STR_LEN];

    if (para->clearAll) {
        pkt_req.pktlen = sizeof(reqpkt_cnblockIO_t) - 2;
    } else {
        pkt_req.pktlen = sizeof(reqpkt_cnblockIO_t) - 2 + sizeof(lb_range_t);
        para->lb_range.startLB = htonll(para->lb_range.startLB);
        para->lb_range.LBlen = htonl(para->lb_range.LBlen);
    }

    pkt_req.pktlen = htons(pkt_req.pktlen);
    pkt_req.opcode = htons(opcode);
    pkt_req.reqID = htonl(get_new_ReqID());
    pkt_req.vol_id = htonll(para->volid);
    pkt_rsp.ackcode = 0;
    pkt_rsp.reqID = 0;

    disco_inet_aton(cnip, ipstr, IPADDR_STR_LEN);

    sk = socket_connect_to(ipstr, port);
    if (sk < 0) {
        info_log("discoadm fail conn cn %s:%d\n", ipstr, port);
        return (pkt_rsp.ackcode);
    }

    if (socket_sendto(sk, (int8_t *)(&pkt_req), sizeof(reqpkt_cncovw_t)) < 0) {
        info_log("discoadm fail send vol to cn %s:%d\n", ipstr, port);
        return (pkt_rsp.ackcode);
    }

    if (socket_sendto(sk, (int8_t *)(&(para->lb_range)), sizeof(lb_range_t)) < 0) {
        info_log("discoadm fail send vol to cn %s:%d\n", ipstr, port);
        return (pkt_rsp.ackcode);
    }

    rexp_len = sizeof(rsppkt_cnblockIO_t);
    rcv_len = rexp_len;
    if (socket_recvfrom(sk, (int8_t *)(&pkt_rsp), rcv_len) < rexp_len) {
        info_log("discoadm fail rcv cvol rsp from cn %s:%d\n", ipstr, port);
        return 0;
    }

    pkt_rsp.ackcode = ntohs(pkt_rsp.ackcode);
    pkt_rsp.reqID = ntohl(pkt_rsp.reqID);
    socket_close(sk);

    printf("result: %d\n", pkt_rsp.ackcode);
    return pkt_rsp.ackcode;
}

static cn_volop_t get_blockIO_opcode (cmdpara_blockIO_t *para)
{
    cn_volop_t op;

    if (para->doBlock) {
        op = cnvolop_block_io;
    } else {
        op = cnvolop_unblock_io;
    }

    return op;
}

static int16_t get_cnvol_opcode (disco_cmd_t *cmd)
{
    int16_t ret;

    ret = -1;
    switch (cmd->cmd_op) {
    case CMD_ATTACH_VOL:
        ret = cnvolop_attach_vol;
        break;
    case CMD_DETACH_VOL:
        ret = cnvolop_detach_vol;
        break;
    case CMD_STOP_VOL:
        ret = cnvolop_stop_vol;
        break;
    case CMD_START_VOL:
        ret = cnvolop_start_vol;
        break;
    case CMD_CLEAR_VOL_OVW:
        ret = cnvolop_reset_ovw;
        break;
    case CMD_CLEAR_VOL_MDCACHE:
        ret = cnvolop_reset_mdcache;
        break;
    case CMD_BLOCK_VOL_IO:
        ret = get_blockIO_opcode((cmdpara_blockIO_t *)(cmd->cmd_data));
        break;
    case CMD_SET_VOL_FEA:
        break;
    default:
        break;
    }

    return ret;
}

uint32_t execute_cn_cmd (disco_cmd_t *cmd)
{
    cmdpara_blockIO_t *blockIO_para;

    if (DISCO_CN != cmd->cmd_comp) {
        return 0;
    }

    if (0 == cmd->target_ip) {
        cmd->target_ip = local_ipaddr;
    }

    switch (cmd->cmd_op) {
    case CMD_ATTACH_VOL:
    case CMD_DETACH_VOL:
    case CMD_STOP_VOL:
    case CMD_START_VOL:
        _vol_op_cn(cmd->target_ip, cmd->target_port,
                get_cnvol_opcode(cmd),
                *((uint64_t *)cmd->cmd_data));
        break;
    case CMD_CLEAR_VOL_OVW:
        _clearovw_op_cn(cmd->target_ip, cmd->target_port, get_cnvol_opcode(cmd),
                (cmdpara_covw_t *)cmd->cmd_data);
        break;
    case CMD_CLEAR_VOL_MDCACHE:
        break;
    case CMD_BLOCK_VOL_IO:
        blockIO_para = (cmdpara_blockIO_t *)cmd->cmd_data;
        _blockIO_op_cn(cmd->target_ip, cmd->target_port,
                get_cnvol_opcode(cmd),
                blockIO_para->volid);
        break;
    case CMD_SET_VOL_FEA:
        break;
    default:
        //TODO: display error message
        break;
    }

    return 0;
}

void free_cncmd_data (disco_cmd_t *cmd)
{
    free(cmd->cmd_data);
}

int32_t get_cn_port ()
{
    return DEFAULT_CN_PORT;
}

int32_t init_cncmd_module ()
{
    local_ipaddr = disco_inet_ntoa(LOCAL_IPADDR_STR);
    instid_len = strlen(def_instid);
    ReqID_Generator = 0;
    return 0;
}
