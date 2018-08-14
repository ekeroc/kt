/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * DNAck_ErrorInjection_private.h
 *
 * Change DNAck from normal response to different error types for testing
 *
 */

#ifndef DISCODN_CLIENT_DNACK_ERRORINJECTION_PRIVATE_H_
#define DISCODN_CLIENT_DNACK_ERRORINJECTION_PRIVATE_H_

#define DNACK_EIJ_STR  "CL.EITestItem=[+|-|m] [IP] [Port] [read/write] [timeout|exception|hbid_err] [1-10] [Seq|random]"

typedef enum {
    ErrDNNotError,
    ErrDNTimeout,
    ErrDNException,
    ErrDNHBIDNotMatch,
} DNAck_Error_t;

typedef enum {
    ChkTypeSequential,
    CkhTypeRandom,
} DNAck_Check_t;

typedef struct DNAck_ErrorInjection_Item {
    uint32_t errType;
    uint32_t errPercentage;
    uint32_t err_chkType;
    struct list_head entry_dntarget;
} DNAck_EIJ_item_t;

typedef struct DNAck_ErrorInjection_Target {
    uint32_t dn_ipaddr;
    uint32_t dn_port;
    uint32_t rwdir;
    atomic_t receive_cnt;
    struct list_head eij_item_pool;
    struct list_head entry_eijpool;
    uint32_t percentage_all;
} DNAck_EIJ_target_t;

typedef struct DNAck_ErrorInject_manager {
    struct list_head eij_target_pool;
    struct mutex eij_target_plock;
    atomic_t num_eij_target;
    atomic_t num_eij_item;
} DNAck_EIJ_MGR_t;

DNAck_EIJ_MGR_t g_dnAck_mgr;

static int8_t *get_ip(int8_t *cmd_buffer, int32_t *len, uint32_t *src_ip);
static int8_t *get_port(int8_t *cmd_buffer, int32_t *len, uint32_t *src_port);
static int8_t *get_rw(int8_t *cmd_buffer, int32_t *len, uint32_t *rw);
static int8_t *get_ErrType(int8_t *cmd_buffer, int32_t *len, uint32_t *errtype);
static int8_t *get_per(int8_t *cmd_buffer, int32_t *len, uint32_t *per);
static int8_t *get_ChkType(int8_t *cmd_buffer, int32_t *len, uint32_t *chktype);


#endif /* DISCODN_CLIENT_DNACK_ERRORINJECTION_PRIVATE_H_ */
