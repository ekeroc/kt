/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * common.h
 *
 */

#ifndef COMMON_H_
#define COMMON_H_

#define htonll(x) \
((((x) & 0xff00000000000000LL) >> 56) | \
(((x) & 0x00ff000000000000LL) >> 40) | \
(((x) & 0x0000ff0000000000LL) >> 24) | \
(((x) & 0x000000ff00000000LL) >> 8) | \
(((x) & 0x00000000ff000000LL) << 8) | \
(((x) & 0x0000000000ff0000LL) << 24) | \
(((x) & 0x000000000000ff00LL) << 40) | \
(((x) & 0x00000000000000ffLL) << 56))

#define ntohll(x)   htonll(x)
#define IPADDR_STR_LEN  15

extern uint32_t err_cnt;

#define info_log(fmt, args...) printf(fmt, ##args); syslog(LOG_ERR, fmt, ##args)
#define err_log(fmt, args...) err_cnt++; syslog(LOG_ERR, fmt, ##args)

#define DMS_MEM_READ_NEXT(dms_buf, dms_dest, dms_size) \
            memcpy(dms_dest, dms_buf, dms_size); dms_buf+= dms_size;

#define DMS_MEM_WRITE_NEXT(dms_buf, dms_src, dms_size) \
            memcpy(dms_buf, dms_src, dms_size); dms_buf+= dms_size;

#define LB_SIZE_BIT_LEN 12

typedef struct lb_range {
    uint64_t startLB;
    uint32_t LBlen;
} lb_range_t;

typedef enum disco_comp {
    DISCO_NN,
    DISCO_DN,
    DISCO_CN,
    DISCO_DSS,
    DISCO_WADB,
    DISCO_DEDUP,
    DISCO_RM, //Request manager
    DISCO_COMP_END,
} disco_comp_t;

typedef enum vol_fea_type {
    prot_method,
    share,
    md_cache,
    data_cache,
    rw_perm,
} vol_fea_type_t;

typedef enum disco_cmd_op {
    CMD_SHOW_HELP,

    CMD_CREATE_VOL,
    CMD_CLONE_VOL,
    CMD_DELETE_VOL,
    CMD_EXTEND_VOL,
    CMD_ATTACH_VOL,
    CMD_DETACH_VOL,
    CMD_FORCE_DETACH_VOL,
    CMD_STOP_VOL,
    CMD_START_VOL,
    CMD_TAKE_SNAPSHOT,
    CMD_RESTORE_VOL,
    CMD_TAKE_REMOTE_SNAP,
    CMD_RESTORE_REMOTE_VOL,

    CMD_BLOCK_VOL_IO,
    CMD_CLEAR_VOL_MDCACHE,
    CMD_CLEAR_VOL_DataCACHE,
    CMD_CLEAR_VOL_OVW,
    CMD_SET_VOL_FEA,

    CMD_CLEAN_VOL_IO,
    CMD_GET_VOL_INFO,

    CMD_WRITE_DATA,
    CMD_READ_DATA,
    CMD_CMP_DATA,

    CMD_GET_DN_SPACE,

    CMD_DUMP_VOL_MD,
    CMD_DO_VOL_RR,
    CMD_END,
} disco_cmd_op_t;

typedef struct disco_cmd {
    disco_comp_t cmd_comp;
    disco_cmd_op_t cmd_op;
    uint32_t target_ip;
    int32_t target_port;
    void *cmd_data;
} disco_cmd_t;

void decompose_triple(uint64_t val, int32_t *rbid, int32_t *len,
        int32_t *offset);
uint32_t disco_inet_ntoa(int8_t *ipaddr);
void disco_inet_aton(uint32_t ipaddr, int8_t *buf, int32_t len);

#endif /* COMMON_H_ */
