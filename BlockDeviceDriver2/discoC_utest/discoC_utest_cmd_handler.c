/*
 *
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_utest_cmd_handler.c
 *
 */
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include "common.h"
#include "discoC_utest_export.h"
#include "discoC_utest_item.h"
#include "discoC_utest_cmd_handler_private.h"

static void _set_last_command (int32_t cmd_type, int32_t cmd_res,
        int8_t *exec_res_str)
{

    mutex_lock(&cmd_lock);

    last_command_type = cmd_type;
    last_command_resp = cmd_res;
    memset(cmd_exec_result, '\0', CMD_RESULT_BUFFER_SIZE);
    strcpy(cmd_exec_result, exec_res_str);

    mutex_unlock(&cmd_lock);
}

static void _rm_head_space (int8_t **cmd, int32_t *cmd_len)
{
    int8_t *cmd_ptr;

    while (*cmd_len > 0) {
        cmd_ptr = *cmd;
        if (*cmd_ptr != ' ') {
            break;
        }

        (*cmd)++;
        (*cmd_len)--;
    }
}

static void _parse_test_cmd (int8_t *cmd, int32_t cmd_len, int8_t *cmd_prefix,
        int8_t *cmd_nm, int8_t *cmd_op, int8_t *cmd_arg)
{
    int8_t *cmd_ptr, *tk_end;
    int32_t remain_len, tk_idx;

    cmd_ptr = cmd;
    remain_len = cmd_len;
    tk_idx = 0;

    while (remain_len > 0) {
        _rm_head_space(&cmd_ptr, &remain_len);
        if (remain_len <= 0) {
            return;
        }

        tk_end = strstr(cmd_ptr, " ");
        if (tk_idx == 0) {
            if (IS_ERR_OR_NULL(tk_end)) {
                strncpy(cmd_prefix, cmd_ptr, strlen(cmd_ptr));
                return;
            } else {
                strncpy(cmd_prefix, cmd_ptr, strlen(cmd_ptr) - strlen(tk_end));
            }
        } else if (tk_idx == 1) {
            if (IS_ERR_OR_NULL(tk_end)) {
                strncpy(cmd_nm, cmd_ptr, strlen(cmd_ptr));
                return;
            } else {
                strncpy(cmd_nm, cmd_ptr, strlen(cmd_ptr) - strlen(tk_end));
            }
        } else if (tk_idx == 2) {
            if (IS_ERR_OR_NULL(tk_end)) {
                strncpy(cmd_op, cmd_ptr, strlen(cmd_ptr));
                return;
            } else {
                strncpy(cmd_op, cmd_ptr, strlen(cmd_ptr) - strlen(tk_end));
            }
        } else {
            strncpy(cmd_arg, cmd_ptr, strlen(cmd_ptr));
            return;
        }

        tk_idx++;
        remain_len -= ((strlen(cmd_ptr) - strlen(tk_end)) - 1);
        cmd_ptr = tk_end + 1;
    }
}

static test_cmd_t _get_cmd_type (int8_t *cmd_op)
{
    test_cmd_t ret;

    ret = CMD_TYPE_NONE;

    do {
        if (strncmp(cmd_op, discoC_reset_str, strlen(discoC_reset_str)) == 0) {
            ret = CMD_TYPE_RESET;
            break;
        }

        if (strncmp(cmd_op, discoC_cfg_str, strlen(discoC_cfg_str)) == 0) {
            ret = CMD_TYPE_CONFIG;
            break;
        }

        if (strncmp(cmd_op, discoC_run_str, strlen(discoC_run_str)) == 0) {
            ret = CMD_TYPE_RUN;
            break;
        }

        if (strncmp(cmd_op, discoC_cancel_str, strlen(discoC_cancel_str)) == 0) {
            ret = CMD_TYPE_CANCEL;
            break;
        }

        if (strncmp(cmd_op, discoC_list_utitem, strlen(discoC_list_utitem)) == 0) {
            ret = CMD_TYPE_SHOW_UTEST;
            break;
        }
    } while (0);

    return ret;
}

/*
 * command format:
 * discoC_utest <test module name> <operation> <other ...>
 */
int32_t exec_test_cmd (int8_t *test_cmd, int32_t cmd_len)
{
    int8_t cmd_prefix[UTEST_NM_LEN], utItem_nm[UTEST_NM_LEN], cmd_op[UTEST_NM_LEN], cmd_arg[256];
    int8_t cmd_exec_ret[256];
    utest_item_t *myUTItem;
    test_cmd_t cmd_type;
    int32_t ret, op_result;

    ret = 0;
    myUTItem = NULL;

    //NOTE: remove LF CR character
    if (test_cmd[cmd_len - 1] == 10 || test_cmd[cmd_len - 1] == 13) {
        test_cmd[cmd_len - 1] = '\0';
        cmd_len--;
    }

    memset(cmd_prefix, '\0', UTEST_NM_LEN);
    memset(utItem_nm, '\0', UTEST_NM_LEN);
    memset(cmd_op, '\0', UTEST_NM_LEN);
    memset(cmd_arg, '\0', 256);

    _parse_test_cmd(test_cmd, cmd_len, cmd_prefix, utItem_nm, cmd_op, cmd_arg);
    if (strcmp(cmd_prefix, discoC_cmd_str)) {
        _set_last_command(CMD_TYPE_NONE, -EINVAL, UNKNOWN_USER_CMD);
        printk("DMSC UTEST unknown command format: %s\n", test_cmd);
        return -EINVAL;
    }

    /*
     * when test item name is none, it's target is manager
     */
    memset(last_cmd_testnm, '\0', UTEST_NM_LEN);
    strcpy(last_cmd_testnm, utItem_nm);
    if (strcmp(discoC_none_utname, utItem_nm)) {
        myUTItem = reference_utest_item(last_cmd_testnm);
        if (IS_ERR_OR_NULL(myUTItem)) {
            _set_last_command(CMD_TYPE_NONE, -EINVAL, UNKNOWN_USER_CMD);
            printk("DMSC UTEST fail search test item: %s\n", test_cmd);
            return -EINVAL;
        }
    }

    cmd_type = _get_cmd_type(cmd_op);
    if (cmd_type == CMD_TYPE_NONE) {
        _set_last_command(CMD_TYPE_NONE, -EINVAL, UNKNOWN_USER_CMD);
        printk("DMSC UTEST unknown command operation: %s\n", test_cmd);
        ret = -EINVAL;
        goto EXEC_CMD_EXIT;
    }

    memset(cmd_exec_ret, '\0', 256);
    op_result = 0;
    switch (cmd_type) {
    case CMD_TYPE_RESET:
        if (!IS_ERR_OR_NULL(myUTItem->utest_reset_fn)) {
            op_result = myUTItem->utest_reset_fn();
        }

        sprintf(cmd_exec_ret, "%s %s %s", discoC_cmd_str, last_cmd_testnm,
                discoC_reset_str);
        _set_last_command(CMD_TYPE_RESET, op_result, cmd_exec_ret);
        break;
    case CMD_TYPE_CONFIG:
        if (!IS_ERR_OR_NULL(myUTItem->utest_config_fn)) {
            op_result = myUTItem->utest_config_fn(cmd_arg);
        }
        sprintf(cmd_exec_ret, "%s %s %s", discoC_cmd_str, last_cmd_testnm,
                discoC_cfg_str);
        _set_last_command(CMD_TYPE_CONFIG, op_result, cmd_exec_ret);
        break;
    case CMD_TYPE_RUN:
        if (!IS_ERR_OR_NULL(myUTItem->utest_run_fn)) {
            op_result = myUTItem->utest_run_fn(cmd_arg);
            sprintf(cmd_exec_ret, "%s %s %s", discoC_cmd_str, last_cmd_testnm,
                    discoC_run_str);
            _set_last_command(CMD_TYPE_RUN, op_result, cmd_exec_ret);
        } else {
            sprintf(cmd_exec_ret, "%s %s %s error", discoC_cmd_str,
                    last_cmd_testnm, discoC_run_str);
            _set_last_command(CMD_TYPE_RUN, -EPERM, cmd_exec_ret);
        }
        break;
    case CMD_TYPE_CANCEL:
        if (!IS_ERR_OR_NULL(myUTItem->utest_cancel_fn)) {
            op_result = myUTItem->utest_cancel_fn(cmd_arg);
        }
        sprintf(cmd_exec_ret, "%s %s %s", discoC_cmd_str, last_cmd_testnm,
                discoC_cancel_str);
        _set_last_command(CMD_TYPE_CANCEL, op_result, cmd_exec_ret);
        break;
    case CMD_TYPE_SHOW_UTEST:
        sprintf(cmd_exec_ret, "%s %s %s", discoC_cmd_str, last_cmd_testnm,
                discoC_list_utitem);
        _set_last_command(CMD_TYPE_SHOW_UTEST, 0, cmd_exec_ret);
        break;
    default:
        _set_last_command(CMD_TYPE_NONE, -EINVAL, UNKNOWN_USER_CMD);
        printk("DMSC UTEST unknown command: %s\n", test_cmd);
        break;
    }

EXEC_CMD_EXIT:
    if (!IS_ERR_OR_NULL(myUTItem)) {
        dereference_utest_item(myUTItem);
    }
    return ret;
}

int32_t get_test_cmd_result (int8_t *buffer, int32_t buff_size)
{
    utest_item_t *myUTItem;
    int32_t len_cp, total_test, run_test, pass_test;

    len_cp = 0;
    switch (last_command_type) {
    case CMD_TYPE_NONE:
        len_cp = sprintf(buffer, "%s %s\n", discoC_cmd_str, NO_USER_CMD_EXEC);
        break;
    case CMD_TYPE_RESET:
    case CMD_TYPE_CONFIG:
    case CMD_TYPE_CANCEL:
        len_cp = sprintf(buffer, "%s %d\n", cmd_exec_result, last_command_resp);
        break;
    case CMD_TYPE_SHOW_UTEST:
        len_cp = show_all_utest(buffer, buff_size);
        break;
    case CMD_TYPE_RUN:
        myUTItem = reference_utest_item(last_cmd_testnm);
        if (IS_ERR_OR_NULL(myUTItem)) {
            len_cp = sprintf(buffer, "%s %s\n", discoC_cmd_str, UNKNOWN_USER_CMD);
            printk("DMSC UTEST unknown test item: %s\n", last_cmd_testnm);
            break;
        }

        if (IS_ERR_OR_NULL(myUTItem->utest_getRes_fn)) {
            len_cp = sprintf(buffer, "%s test item no get result function\n",
                    discoC_cmd_str);
            printk("DMSC UTEST no get result function: %s\n", last_cmd_testnm);
            dereference_utest_item(myUTItem);
            break;
        }

        if (last_command_resp) {
            len_cp = sprintf(buffer, "%s %s %s %d\n", discoC_cmd_str,
                    last_cmd_testnm, discoC_run_str, last_command_resp);
        } else {
            total_test = 0;
            run_test = 0;
            pass_test = 0;
            myUTItem->utest_getRes_fn(&total_test, &run_test, &pass_test);
            len_cp = sprintf(buffer, "%s %s %s %d %d %d\n",
                    discoC_cmd_str, last_cmd_testnm, discoC_run_str,
                    total_test, run_test, pass_test);
        }
        dereference_utest_item(myUTItem);
        break;
    default:
        len_cp = sprintf(buffer, "%s %s\n", discoC_cmd_str, UNKNOWN_USER_CMD);
        break;
    }

    return len_cp;
}

int32_t init_cmd_handler (void)
{
    mutex_init(&cmd_lock);
    last_command_type = CMD_TYPE_NONE;
    last_command_resp = 0;
    memset(cmd_exec_result, '\0', CMD_RESULT_BUFFER_SIZE);
    memset(last_cmd_testnm, '\0', UTEST_NM_LEN);
    strcpy(cmd_exec_result, NO_USER_CMD_EXEC);

    return 0;
}
