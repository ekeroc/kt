/*
 *
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_utest_cmd_handler_private.h
 *
 */

#ifndef DISCOC_UTEST_CMD_HANDLER_PRIVATE_H_
#define DISCOC_UTEST_CMD_HANDLER_PRIVATE_H_

#define discoC_cmd_str  "discoC_utest"
#define discoC_reset_str "reset"
#define discoC_cfg_str "config"
#define discoC_run_str "run"
#define discoC_cancel_str "cancel"
#define discoC_list_utitem  "list_utest"
#define discoC_none_utname  "none"

#define NO_USER_CMD_EXEC    "no user cmd exec"
#define UNKNOWN_USER_CMD    "unknown user cmd"
typedef enum {
    CMD_TYPE_NONE,
    CMD_TYPE_RESET,
    CMD_TYPE_CONFIG,
    CMD_TYPE_RUN,
    CMD_TYPE_CANCEL,
    CMD_TYPE_SHOW_UTEST,
} test_cmd_t;

#define CMD_RESULT_BUFFER_SIZE  1020

int8_t cmd_exec_result[CMD_RESULT_BUFFER_SIZE];
int8_t last_cmd_testnm[UTEST_NM_LEN];
int32_t last_command_resp;
int32_t last_command_type;
struct mutex cmd_lock;

#endif /* DISCOC_UTEST_CMD_HANDLER_PRIVATE_H_ */
