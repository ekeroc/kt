/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoadm_private.h
 *
 */

#ifndef DISCOADM_PRIVATE_H_
#define DISCOADM_PRIVATE_H_

#define MIN_PARA    1

typedef enum {
    TK_ERR = -1,
    TK_COMP,
    TK_TARGET,
    TK_OPCODE,
} toke_type_t;

typedef struct opt_token {
    toke_type_t token_type;
    uint32_t token_val;
    uint32_t token_str_len;
    uint8_t *toke_str;
} opt_token_t;

#define HELP_STR    "--help"

#define TARGET_STR  "--target"
#define COMP_STR  "--comp"

#define CREATE_VOL_STR  "--create"
#define CLONE_VOL_STR   "--clone"
#define DEL_VOL_STR     "--delete"
#define ATTACH_VOL_STR  "--attach"
#define DETACH_VOL_STR  "--detach"
#define STOP_VOL_STR    "--stop"
#define START_VOL_STR   "--start"
#define BLOCKIO_STR     "--blockio"
#define CLEAROVW_STR    "--clearovw"
#define DUMPMD_STR      "--dumpmd"
#define DORR_STR      "--doRR"

#define MAX_TOKEN   14
opt_token_t opt_token_arr[MAX_TOKEN] = {
        { TK_COMP, TK_COMP, 0, COMP_STR },
        { TK_TARGET, TK_TARGET, 0, TARGET_STR },

        { TK_OPCODE, CMD_SHOW_HELP, 0, HELP_STR },

        { TK_OPCODE, CMD_CREATE_VOL, 0, CREATE_VOL_STR },
        { TK_OPCODE, CMD_CLONE_VOL, 0, CLONE_VOL_STR },
        { TK_OPCODE, CMD_DELETE_VOL, 0, DEL_VOL_STR },
        { TK_OPCODE, CMD_ATTACH_VOL, 0, ATTACH_VOL_STR },
        { TK_OPCODE, CMD_DETACH_VOL, 0, DETACH_VOL_STR },
        { TK_OPCODE, CMD_STOP_VOL, 0, STOP_VOL_STR },
        { TK_OPCODE, CMD_START_VOL, 0, START_VOL_STR },
        { TK_OPCODE, CMD_BLOCK_VOL_IO, 0, BLOCKIO_STR },
        { TK_OPCODE, CMD_CLEAR_VOL_OVW, 0, CLEAROVW_STR },

        { TK_OPCODE, CMD_DUMP_VOL_MD, 0, DUMPMD_STR },
        { TK_OPCODE, CMD_DO_VOL_RR, 0, DORR_STR },
};

int8_t *comp_arr[] = {
        "NN", "DN", "CN", "DSS", "WADB", "DEDUP", "RM",
};

uint32_t err_cnt;

#endif /* DISCOADM_PRIVATE_H_ */
