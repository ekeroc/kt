/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoadm.c
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <syslog.h>
#include "common.h"
#include "discoadm_private.h"
#include "discoadm.h"
#include "nn_cmd.h"
#include "cn_cmd.h"

static bool show_help ()
{
    printf("disco administration tool help - \n");
    printf("discoadm [--target <nn ip>] --create <volumd cap>\n");
    printf("discoadm [--target <nn ip>] --clone <volumd id>\n");
    printf("discoadm [--target <nn ip>] --delete <volumd id>\n");
    printf("discoadm [--comp <nn/cn>] [--target <cn ip>] --attach <volumd id>\n");
    printf("discoadm [--comp <nn/cn>] [--target <cn ip>] --detach <volumd id>\n");
    printf("discoadm [--comp <nn/cn>] [--target <cn ip>] --stop <volumd id>\n");
    printf("discoadm [--comp <nn/cn>] [--target <cn ip>] --start <volumd id>\n");

    printf("discoadm [--target <cn ip>] --blockio <volumd id> <true/false>\n");
    printf("discoadm [--target <cn ip>] --clearovw <volumd id> [start LB] [LB len]\n");

    printf("discoadm [--comp <nn/cn>] [--target <cn ip>] --dumpmd <volumd id> [filename] [startLB LB_len]\n");
    printf("discoadm [--comp <nn>] [--target <nn ip>] --doRR <volumd id> [filename] [startLB LB_len]\n");
    return false;
}

static toke_type_t get_tk_type (int8_t *tk, uint32_t *tk_index)
{
    uint32_t i;
    toke_type_t ret;

    ret = TK_ERR;

    for (i = 0; i < MAX_TOKEN; i++) {
        if (strcasecmp(tk, opt_token_arr[i].toke_str)) {
            continue;
        }

        ret = opt_token_arr[i].token_type;
        *tk_index = i;
        break;
    }

    return ret;
}

static disco_comp_t _parse_get_comp (int8_t *comp_str)
{
    uint32_t i;
    disco_comp_t ret;

    ret = DISCO_COMP_END;
    for (i = 0; i < DISCO_COMP_END; i++) {
        if (strcasecmp(comp_str, comp_arr[i])) {
            continue;
        }

        ret = (disco_comp_t)i;
    }

    return ret;
}

static void init_cmd (disco_cmd_t *cmd)
{
    cmd->cmd_comp = DISCO_COMP_END;
    cmd->cmd_data = NULL;
    cmd->cmd_op = CMD_END;
    cmd->target_ip = 0;
    cmd->target_port = -1;
}

static int32_t get_serv_port (disco_comp_t comp)
{
    int32_t port;

    port = 0;

    switch (comp) {
    case DISCO_NN:
        port = get_nn_port();
        break;
    case DISCO_DN:
        break;
    case DISCO_CN:
        port = get_cn_port();
        break;
    case DISCO_DSS:
    case DISCO_WADB:
    case DISCO_DEDUP:
    case DISCO_RM:
    case DISCO_COMP_END:
        break;
    default:
        err_log("discoadm tool ERROR wrong comp code %d\n", comp);
        break;
    }

    return port;
}

static disco_comp_t get_default_comp (disco_cmd_op_t cmd_op)
{
    disco_comp_t ret;

    ret = DISCO_CN;
    switch (cmd_op) {
    case CMD_CREATE_VOL:
    case CMD_CLONE_VOL:
    case CMD_DELETE_VOL:
    case CMD_DUMP_VOL_MD:
    case CMD_DO_VOL_RR:
        ret = DISCO_NN;
        break;
    case CMD_TAKE_SNAPSHOT:
    case CMD_RESTORE_VOL:
        ret = DISCO_DSS;
        break;
    case CMD_TAKE_REMOTE_SNAP:
    case CMD_RESTORE_REMOTE_VOL:
        ret = DISCO_WADB;
        break;
    default:
        break;
    }

    return ret;
}

/*
 * ret  true: set nn cmd opt success
 *     false: set nn cmd opt fail
 */
static bool set_cmd_opt (int32_t argc, int8_t *argv[], uint32_t s_index,
        disco_cmd_t *cmd)
{
    bool ret;

    ret = true;
    if (CMD_SHOW_HELP == cmd->cmd_op) {
        cmd->target_port = 0;
        cmd->cmd_comp = DISCO_CN;
        return ret;
    }

    if (cmd->cmd_op <= CMD_SHOW_HELP || cmd->cmd_op >= CMD_END) {
        err_log("discoadm tool ERROR wrong cmd op code %d\n", cmd->cmd_op);
        return false;
    }

    if (DISCO_COMP_END == cmd->cmd_comp) {
        cmd->cmd_comp = get_default_comp(cmd->cmd_op);
    }

    cmd->target_port = get_serv_port(cmd->cmd_comp);

    switch (cmd->cmd_comp) {
    case DISCO_NN:
        ret = set_nncmd_opt(argc, argv, s_index, cmd);
        break;
    case DISCO_CN:
        ret = set_cncmd_opt(argc, argv, s_index, cmd);
        break;
    default:
        ret = false;
        break;
    }

    return ret;
}

/*
 * ret  true: set nn cmd opt success
 *     false: set nn cmd opt fail
 */
static bool parse_cmd (int32_t argc, int8_t *argv[], disco_cmd_t *cmd)
{
    uint32_t i, tk_index;
    toke_type_t tk_type;
    bool ret;

    ret = true;
    init_cmd(cmd);

    i = 1;
    while (i < argc) {
        tk_type = get_tk_type(argv[i], &tk_index);
        if (TK_ERR == tk_type) {
            ret = true;
            break;
        }

        i++;
        switch (tk_type) {
        case TK_COMP:
            cmd->cmd_comp = _parse_get_comp(argv[i]);
            i++;
            break;
        case TK_TARGET:
            cmd->target_ip = disco_inet_ntoa(argv[i]);
            i++;
            break;
        case TK_OPCODE:
            //call each componet to get para
            if (i < argc) {
                cmd->cmd_op = opt_token_arr[tk_index].token_val;
                ret = set_cmd_opt(argc, argv, i, cmd);
                i = argc;
            } else if (i == argc) {
                cmd->cmd_op = opt_token_arr[tk_index].token_val;
                ret = set_cmd_opt(argc, argv, i, cmd);
            } else {

                ret = false;
            }
            break;
        default:
            cmd->cmd_comp = DISCO_COMP_END;
            cmd->cmd_op = CMD_END;
            err_log("discoadm wrong token type %d\n", tk_type);

            i = argc;
            ret = false;
            break;
        }
    }

    return ret;
}

void show_cmd (disco_cmd_t *cmd)
{
    printf("discoadm cmd content -\n");
    printf("             component: %d\n", cmd->cmd_comp);
    printf("                target: %d:%d\n", cmd->target_ip, cmd->target_port);
    printf("                    op: %d\n", cmd->cmd_op);
    //show parameter
}

static bool is_err_cmd (disco_cmd_t *cmd)
{
    bool ret;

    ret = false;

    do {
        if (CMD_END == cmd->cmd_comp) {
            ret = true;
            break;
        }

        if (DISCO_COMP_END == cmd->cmd_comp) {
            ret = true;
            break;
        }

        if (-1 == cmd->target_port) {
            ret = true;
            break;
        }
    } while(0);

    return ret;
}

static void execute_cmd (disco_cmd_t *cmd)
{
    switch (cmd->cmd_op) {
    case CMD_SHOW_HELP:
        show_help();
        break;
    case CMD_CREATE_VOL:
    case CMD_CLONE_VOL:
    case CMD_DELETE_VOL:
        execute_nn_cmd(cmd);
        break;
    case CMD_ATTACH_VOL:
    case CMD_DETACH_VOL:
    case CMD_STOP_VOL:
    case CMD_START_VOL:
    case CMD_DUMP_VOL_MD:
    case CMD_DO_VOL_RR:
        switch (cmd->cmd_comp) {
        case DISCO_NN:
            execute_nn_cmd(cmd);
            break;
        case DISCO_CN:
            execute_cn_cmd(cmd);
            break;
        default:
            //TODO: Display error message
            break;
        }

        break;
    case CMD_CLEAR_VOL_OVW:
    case CMD_CLEAR_VOL_MDCACHE:
    case CMD_BLOCK_VOL_IO:
    case CMD_SET_VOL_FEA:
        execute_cn_cmd(cmd);
        break;
    default:
        //TODO: Display error message
        printf("ready to do command\n");
        show_cmd(cmd);
        break;
    }
}

static void free_cmddata (disco_cmd_t *cmd)
{
    if (cmd->cmd_data == NULL) {
        return;
    }

    switch (cmd->cmd_comp) {
    case DISCO_NN:
        free_nncmd_data(cmd);
        break;
    case DISCO_CN:
        free_cncmd_data(cmd);
        break;
    default:
        free(cmd->cmd_data);
        break;
    }

    cmd->cmd_data = NULL;
}

static void _init_token ()
{
    uint32_t i;

    for (i = 0; i < MAX_TOKEN; i++) {
        opt_token_arr[i].token_str_len = strlen(opt_token_arr[i].toke_str);
    }
}

static void _init_discoadm ()
{
    err_cnt = 0;
}

static void _init_cmd_module ()
{
    init_nncmd_module();
    init_cncmd_module();
}

int32_t main (int32_t argc, int8_t *argv[])
{
    disco_cmd_t cmd;
    uint32_t i, ret;
    bool wrong_format;

    ret = 0;
    wrong_format = false;

    do {
        if (argc <= MIN_PARA) {
            wrong_format = true;
            break;
        }

        _init_discoadm();
        _init_cmd_module();
        _init_token();

        if (!parse_cmd(argc, argv, &cmd)) {
            wrong_format = true;
            break;
        }

        if (is_err_cmd(&cmd)) {
            wrong_format = true;
            break;
        }

        //Perform task
        execute_cmd(&cmd);
        free_cmddata(&cmd);

    } while (0);


    if (wrong_format) {
        printf("Try `discoadm --help' for more information.\n");
    }

    if (err_cnt) {
        printf("discoadm: something wrong with err_cnt %d\n", err_cnt);
    }
    return ret;
}
