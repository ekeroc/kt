/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * nn_cmd.h
 */

#ifndef CN_CMD_H_
#define CN_CMD_H_

typedef enum {
    cnvolop_attach_vol = 3,
    cnvolop_detach_vol,
    cnvolop_reset_ovw = 11,
    cnvolop_reset_mdcache,
    cnvolop_block_io,
    cnvolop_unblock_io,
    cnvolop_set_share,
    cnvolop_stop_vol,
    cnvolop_start_vol,
} cn_volop_t;

/*
 * For holding the parameter list of user's block IO command
 * doBlock = true  when user want to block IO
 *         = false when user want to unblock IO
 *
 */
typedef struct cmdpara_blockIO {
    bool doBlock;
    uint64_t volid;
} cmdpara_blockIO_t;

typedef struct cmdpara_covw {
    bool clearAll;
    uint64_t volid;
    lb_range_t lb_range;
} cmdpara_covw_t;

bool set_cncmd_opt(int32_t argc, int8_t *argv[], uint32_t s_index,
        disco_cmd_t *cmd);
uint32_t execute_cn_cmd(disco_cmd_t *cmd);
void free_cncmd_data(disco_cmd_t *cmd);

int32_t get_cn_port();
int32_t init_cncmd_module();

#endif /* CN_CMD_H_ */
