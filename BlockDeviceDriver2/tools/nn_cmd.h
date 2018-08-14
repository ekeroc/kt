/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * nn_cmd.h
 *
 */

#ifndef NN_CMD_H_
#define NN_CMD_H_

#define NN_MAGIC_S  0xa5a5a5a5a5a5a5a5L
#define NN_MAGIC_E  0xa5a5a5a5a5a5a5a5L

typedef enum {
    nnvolop_create_vol = 1001,
    nnvolop_clone_vol,
    nnvolop_attach_vol,
    nnvolop_delete_vol,
    nnvolop_detach_vol,
} nn_volop_t;

typedef struct cmdpara_dumpMD {
    uint64_t vol_id;
    uint64_t startLB;
    uint64_t LB_len;
    int8_t *filename;
} cmdpara_dumpMD_t;

typedef cmdpara_dumpMD_t cmdpara_doRR_t;

int32_t get_nn_port();

uint32_t execute_nn_cmd(disco_cmd_t *cmd);
bool set_nncmd_opt(int32_t argc, int8_t *argv[], uint32_t s_index,
        disco_cmd_t *cmd);
void free_nncmd_data(disco_cmd_t *cmd);
int32_t init_nncmd_module();
#endif /* NN_CMD_H_ */
