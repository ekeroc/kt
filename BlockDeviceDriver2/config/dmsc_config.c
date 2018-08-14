/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * dmsc_config.c
 *
 *
 */
#ifdef DMSC_USER_DAEMON
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <sys/types.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "../common/common_util.h"
#include "../common/common.h"
#include "../user_daemon/ipcsocket.h"
#include "../user_daemon/mclient.h"
#else
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/swap.h>
#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/discoC_comm_main.h"
#include "../SelfTest.h"
#include "../discoDN_client/DNAck_ErrorInjection.h"
#endif

#include "dmsc_config_private.h"
#include "dmsc_config.h"

#ifdef DMSC_USER_DAEMON
static int32_t config_ioctl (int32_t chrdev_fd, int8_t *name, uint64_t val)
{
    int32_t ret;

    config_para_t cpara;

    ret = sprintf(cpara.item, "%s=%llu", name, (long long unsigned int)val);

    if (ret > 0) {
        cpara.item_len = ret;
        ret = ioctl(chrdev_fd, IOCTL_CONFIG_DRV, (unsigned long)(&cpara));
    }

    return ret;
}

static int32_t config_ioctl_str (int32_t chrdev_fd, int8_t *name, int8_t *val)
{
    int32_t ret;
    config_para_t cpara;
    config_para_t *a1;

    ret = sprintf(cpara.item, "%s=%s", name, val);

    if (ret > 0) {
        cpara.item_len = ret;
        ret = ioctl(chrdev_fd, IOCTL_CONFIG_DRV, (unsigned long)(&cpara));
    }

    return ret;
}

static int8_t write_procfs (int8_t *name, uint64_t val)
{
    int8_t buf[100];
    FILE *fp;
    int32_t flag, retry;

    flag = 0;
    retry = 0;
    fp = fopen(proc_config_file,"w");
    sprintf(buf, "%s=%llu", name, (long long unsigned int)val);
    while (retry < 30) {
        if (fp != NULL) {
            fprintf(fp,"%s", buf);
            flag = 1;
            break;
        } else {
            sleep(1);
        }
        retry++;
    }

    if (flag) {
        fclose(fp);
        return 0;
    }

    return 1;
}

static int32_t write_procfs_str (int8_t *name, int8_t *val)
{
    int8_t buf[100];
    FILE *fp;
    int32_t flag, retry;

    flag = 0;
    retry = 0;
    fp = fopen(proc_config_file,"w");
    sprintf(buf, "%s=%s", name, val);
    while (retry < 30) {
        if (fp != NULL) {
            fprintf(fp,"%s", buf);
            flag = 1;
            break;
        } else {
            sleep(1);
        }
        retry++;
    }

    if (flag) {
        fclose(fp);
        return 0;
    }

    return 1;
}

void config_discoC_drv (int32_t chrdev_fd, uint32_t mds_ipaddr)
{
    int32_t retCode;
    config_item_index i;

    retCode = 0;
    for (i = cl_fc; i < cl_max_config_item_end; i++) {
        switch (i) {
        case cl_log_level:
        case cl_mds_name:
            syslog(LOG_ERR, "DMSC INFO config driver (%s:%s)\n",
                    config_list[i].name,
                    config_list[i].value_str);
            //retCode = write_procfs_str(config_list[i].name,
            //        config_list[i].value_str);
            retCode = config_ioctl_str(chrdev_fd, config_list[i].name,
                    config_list[i].value_str);
            break;
        case cl_sk_timeout:
            syslog(LOG_ERR, "DMSC INFO config driver (%s:%ld)\n",
                    config_list[i].name, config_list[i].value);
            //retCode = write_procfs(CL_SOCKET_HB_INTERVAL,
            //        config_list[i].value / (CNT_HB_FOR_1_TIMEOUT));
            retCode = config_ioctl(chrdev_fd, CL_SOCKET_HB_INTERVAL,
                    config_list[i].value / (CNT_HB_FOR_1_TIMEOUT));
            config_list[cl_hb_interval].value =
                    config_list[i].value / (CNT_HB_FOR_1_TIMEOUT);
            break;
        case cl_rr:
            syslog(LOG_ERR, "DMSC INFO config driver (%s:%s)\n",
                    config_list[i].name, config_list[i].value_str);
            //retCode = write_procfs_str(config_list[i].name,
            //        config_list[i].value_str);
            retCode = config_ioctl_str(chrdev_fd, config_list[i].name,
                    config_list[i].value_str);
            break;
        case cl_hb_interval:
        case cl_disk_ra:
        case cl_mds_vo_port:
            break;
        case cl_mds_ip:
            config_list[i].value = mds_ipaddr;
            gdef_nnSrvIPAddr = mds_ipaddr;
        default:
            syslog(LOG_ERR, "DMSC INFO config driver (%s:%ld)\n",
                    config_list[i].name, config_list[i].value);
            //retCode = write_procfs(config_list[i].name, config_list[i].value);
            retCode = config_ioctl(chrdev_fd, config_list[i].name,
                    config_list[i].value);
            break;
        }

        if (retCode) {
            syslog(LOG_ERR, "Fail to config driver with name:%s value:%ld\n",
                    config_list[i].name, config_list[i].value);
        }
    }
}

void config_discoC_UsrD (void)
{
    config_item_index i;
    int32_t nmlen;

    disk_read_ahead = CL_DEFAULT_DISK_READ_AHEAD_DEF_VAL;
    g_nn_VO_port = DMS_MDS_VO_PORT_DEF_VAL;

    for (i = cl_fc; i < cl_max_config_item_end; i++) {
        switch (i) {
        case cl_disk_ra:
            syslog(LOG_ERR, "DMSC INFO config daemon (%s:%ld)\n",
                    config_list[i].name, config_list[i].value);
            disk_read_ahead = config_list[i].value;
            break;
        case cl_mds_vo_port:
            g_nn_VO_port = config_list[i].value;
            break;
        case cl_mds_name:
            memset(gdef_nnSrvName, '\0', MAX_LEN_NNSRV_NM);
            strncpy(gdef_nnSrvName, config_list[i].value_str, MAX_LEN_NNSRV_NM);
            break;
        case cl_nnSimulator:
            g_nnSimulator_on  = config_list[i].value;
            break;
        default:
            break;
        }
    }
}

int32_t set_config_val (int8_t *name_str, int32_t name_len, int8_t *val_str,
        int32_t val_len)
{
    int32_t i, ret = -1;

    for (i = 0 ;i < CUR_CONFIG_ITEM; i++) {
        if (strncmp(name_str, &(config_list[i].name[0]), name_len) == 0) {
            if (VAL_TYPE_STR != config_list[i].val_type) {
                sscanf(val_str, "%ld", &config_list[i].value);
            } else {
                strncpy(&(config_list[i].value_str[0]), val_str, val_len);
                config_list[i].value_str_len = val_len;
            }
            //syslog(LOG_ERR, "DMSC INFO Add config item: "
            //        "name: %s value: %llu\n",
            //        name_str, val_str);
            ret = 0;
            break;
        }
    }

    return ret;
}
#endif

static void set_config_list_item (config_item_index index,
        int8_t *name, int32_t name_len,
        int8_t *val_str, int32_t val_str_len,
        int64_t value, config_val_type_t val_type)
{
    strncpy(&(config_list[index].name[0]), name, name_len);
    config_list[index].name_len = name_len;

    if (!val_str_len) {
        memset(&(config_list[index].value_str[0]), 0, MAX_Value_Len);
    } else {
        strncpy(&(config_list[index].value_str[0]), val_str, val_str_len);
    }
    config_list[index].value_str_len = val_str_len;
    config_list[index].value = value;
    config_list[index].val_type = val_type;
    config_list[index].val_ptr = NULL;
}

void init_config_list (void)
{
    int32_t cnt, val;

    cnt = 0;

    set_config_list_item(cl_fc, CL_FLOW_CONTROL,
            strlen(CL_FLOW_CONTROL), '\0', 0,
            CL_FLOW_CONTROL_DEF_VAL, VAL_TYPE_BOOL);

    set_config_list_item(cl_read_bal, CL_READ_LOAD_BALANCE,
                    strlen(CL_READ_LOAD_BALANCE), '\0', 0,
                    CL_READ_LOAD_BALANCE_DEF_VAL, VAL_TYPE_BOOL);

    set_config_list_item(cl_read_hbs_next_dn, CL_READ_NUM_HB_NEXT_DN,
            strlen(CL_READ_NUM_HB_NEXT_DN), '\0', 0,
            CL_READ_NUM_HB_NEXT_DN_DEF_VAL, VAL_TYPE_INT32);

    set_config_list_item(cl_mcache, CL_METADATA_CACHE,
                    strlen(CL_METADATA_CACHE), '\0', 0,
                    CL_METADATA_CACHE_DEF_VAL, VAL_TYPE_BOOL);

    set_config_list_item(cl_mcache_max_items, CL_MAX_METADATA_CACHE_SIZE,
                    strlen(CL_MAX_METADATA_CACHE_SIZE), '\0', 0,
                    CL_MAX_METADATA_CACHE_SIZE_DEF_VAL, VAL_TYPE_INT64);

    set_config_list_item(cl_mcache_max_allhit, CL_MAX_CAP_FOR_CACHE_ALL_HIT,
                    strlen(CL_MAX_CAP_FOR_CACHE_ALL_HIT), '\0', 0,
                    CL_MAX_CAP_FOR_CACHE_ALL_HIT_DEF_VAL, VAL_TYPE_INT64);

    set_config_list_item(cl_min_disk_ack, CL_MIN_DISK_ACK,
                    strlen(CL_MIN_DISK_ACK), '\0', 0,
                    CL_MIN_DISK_ACK_DEF_VAL, VAL_TYPE_INT32);

    set_config_list_item(cl_fp, CL_FP_CALCULATION,
                    strlen(CL_FP_CALCULATION), '\0', 0,
                    CL_FP_CALCULATION_DEF_VAL, VAL_TYPE_INT32);

    set_config_list_item(cl_share_vol, CL_DEFAULT_ENABLE_SHARE_VOLUME,
                    strlen(CL_DEFAULT_ENABLE_SHARE_VOLUME), '\0', 0,
                    CL_DEFAULT_ENABLE_SHARE_VOLUME_DEF_VAL, VAL_TYPE_BOOL);

    set_config_list_item(cl_show_perf, CL_SHOW_PERFORMANCE_DATA,
                    strlen(CL_SHOW_PERFORMANCE_DATA), '\0', 0,
                    CL_SHOW_PERFORMANCE_DATA_DEF_VAL, VAL_TYPE_BOOL);

    if (strcmp(CL_DEFAULT_LOG_LEVEL_VALUE, STR_DEBUG) == 0) {
        val = LOG_LVL_DEBUG;
    } else if (strcmp(CL_DEFAULT_LOG_LEVEL_VALUE, STR_INFO) == 0) {
        val = LOG_LVL_INFO;
    } else if (strcmp(CL_DEFAULT_LOG_LEVEL_VALUE, STR_WARN) == 0) {
        val = LOG_LVL_WARN;
    } else if (strcmp(CL_DEFAULT_LOG_LEVEL_VALUE, STR_ERROR) == 0) {
        val = LOG_LVL_ERR;
    } else if (strcmp(CL_DEFAULT_LOG_LEVEL_VALUE, STR_EMERG) == 0) {
        val = LOG_LVL_EMERG;
    } else {
        val = LOG_LVL_INFO;
    }
    set_config_list_item(cl_log_level, CL_DEFAULT_LOG_LEVEL,
                    strlen(CL_DEFAULT_LOG_LEVEL),
                    CL_DEFAULT_LOG_LEVEL_VALUE,
                    strlen(CL_DEFAULT_LOG_LEVEL_VALUE),
                    val, VAL_TYPE_STR);

    set_config_list_item(cl_sk_timeout, DMS_SOCKET_TIMEOUT,
                        strlen(DMS_SOCKET_TIMEOUT), '\0', 0,
                        DMS_SOCKET_TIMEOUT_DEF_VAL, VAL_TYPE_INT64);


    set_config_list_item(cl_hb_interval, CL_SOCKET_HB_INTERVAL,
                        strlen(CL_SOCKET_HB_INTERVAL), '\0', 0,
                        DMS_SOCKET_TIMEOUT_DEF_VAL / CNT_HB_FOR_1_TIMEOUT,
                        VAL_TYPE_INT32);

    set_config_list_item(cl_disk_ra, CL_DEFAULT_DISK_READ_AHEAD,
                        strlen(CL_DEFAULT_DISK_READ_AHEAD), '\0', 0,
                        CL_DEFAULT_DISK_READ_AHEAD_DEF_VAL, VAL_TYPE_INT64);

    set_config_list_item(cl_mds_vo_port, DMS_MDS_VO_PORT,
            strlen(DMS_MDS_VO_PORT), '\0', 0,
            DMS_MDS_VO_PORT_DEF_VAL, VAL_TYPE_INT64);

    set_config_list_item(cl_mds_io_port, DMS_MDS_IO_PORT,
            strlen(DMS_MDS_IO_PORT), '\0', 0,
            DMS_MDS_IO_PORT_DEF_VAL, VAL_TYPE_INT64);

    set_config_list_item(cl_req_timeout, CL_IOREQ_TIMEOUT,
            strlen(CL_IOREQ_TIMEOUT), '\0', 0,
            CL_IOREQ_TIMEOUT_DEF_VAL, VAL_TYPE_INT32);

    set_config_list_item(cl_flush_timeout, CL_IO_FLUSH_TIMEOUT,
            strlen(CL_IO_FLUSH_TIMEOUT), '\0', 0,
            CL_IO_FLUSH_TIMEOUT_DEF_VAL, VAL_TYPE_INT32);

    if (strcmp(DMS_RR_ENABLE_DEF_VAL, "true") == 0) {
        val = 1;
    } else {
        val = 0;
    }
    set_config_list_item(cl_rr, DMS_RR_ENABLE, strlen(DMS_RR_ENABLE),
            DMS_RR_ENABLE_DEF_VAL, strlen(DMS_RR_ENABLE_DEF_VAL),
            val, VAL_TYPE_STR);

    set_config_list_item(cl_high_resp, CL_WIO_HIGH_RESP,
                strlen(CL_WIO_HIGH_RESP), '\0', 0,
                CL_WIO_HIGH_RESP_DEF_VAL, VAL_TYPE_BOOL);

    set_config_list_item(cl_high_resp_wtime, CL_WIO_HIGH_RESP_WTIME,
            strlen(CL_WIO_HIGH_RESP_WTIME), '\0', 0,
            CL_WIO_HIGH_RESP_WAITTIME_DEF_VAL, VAL_TYPE_INT32);

    set_config_list_item(cl_wIOdone_conn_lostt, CL_WIO_DONE_IMMED_CONN_LOSTTIME,
            strlen(CL_WIO_DONE_IMMED_CONN_LOSTTIME), '\0', 0,
            CL_WIO_DONE_IMMED_CONN_LOSTTIME_DEF_VAL, VAL_TYPE_INT64);

    set_config_list_item(cl_nr_ioreqs, CL_NR_IOREQS,
                strlen(CL_NR_IOREQS), '\0', 0,
                CL_NR_IOREQS_DEF_VAL, VAL_TYPE_INT64);

    set_config_list_item(cl_req_req_merge, CL_REQ_MERGE,
            strlen(CL_REQ_MERGE), '\0', 0,
            CL_REQ_MERGE_DEF_VAL, VAL_TYPE_BOOL);

    set_config_list_item(cl_req_mw_intvl, CL_REQ_M_WAIT_INTVL,
                strlen(CL_REQ_M_WAIT_INTVL), '\0', 0,
                CL_REQ_M_WAIT_INTVL_DEF_VAL, VAL_TYPE_INT32);

    set_config_list_item(cl_req_mw_maxcnt, CL_REQ_M_WAIT_MAX_CNT,
                strlen(CL_REQ_M_WAIT_MAX_CNT), '\0', 0,
                CL_REQ_M_WAIT_MAX_CNT_DEF_VAL, VAL_TYPE_INT32);

    set_config_list_item(cl_req_rsp_perf_warn, CL_REQ_ACK_PERF_WARN,
            strlen(CL_REQ_ACK_PERF_WARN), '\0', 0,
            CL_REQ_ACK_PERF_WARN_DEF_VAL, VAL_TYPE_INT32);

    set_config_list_item(cl_nnrate_warn, CL_NNRATE_PERF_WARN,
                strlen(CL_NNRATE_PERF_WARN), '\0', 0,
                CL_NNRATE_PERF_WARN_DEF_VAL, VAL_TYPE_INT32);

    set_config_list_item(cl_nnmaxreq_warn, CL_NNMAXREQ_PERF_WARN,
                strlen(CL_NNMAXREQ_PERF_WARN), '\0', 0,
                CL_NNMAXREQ_PERF_WARN_DEF_VAL, VAL_TYPE_INT32);

    set_config_list_item(cl_ior_perf_warn, CL_IOR_PERF_WARN,
            strlen(CL_IOR_PERF_WARN), '\0', 0,
            CL_IOR_PERF_WARN_DEF_VAL, VAL_TYPE_INT32);

    set_config_list_item(cl_req_qdelay_warn, CL_REQ_QDELAY_WARN,
            strlen(CL_REQ_QDELAY_WARN), '\0', 0,
            CL_REQ_QDELAY_WARN_DEF_VAL, VAL_TYPE_INT32);

    set_config_list_item(cl_mds_ip, DMS_MDS_IP,
            strlen(DMS_MDS_IP), '\0', 0,
            DMS_MDS_IP_DEF_VAL, VAL_TYPE_INT32);

    set_config_list_item(cl_mds_name, CL_DEF_NNSRV_NM, strlen(CL_DEF_NNSRV_NM),
            CL_DEF_NNSRV_NM_VAL, strlen(CL_DEF_NNSRV_NM_VAL), 0, VAL_TYPE_STR);

    set_config_list_item(cl_cLinux_cache, CL_ACTIVE_ClearLinuxCache,
                strlen(CL_ACTIVE_ClearLinuxCache), '\0', 0,
                CL_ACTIVE_ClearLinuxCache_DEF_VAL, VAL_TYPE_BOOL);

    set_config_list_item(cl_cLinux_cache_period, CL_CLinuxCache_Period,
                strlen(CL_CLinuxCache_Period), '\0', 0,
                CL_CLinuxCache_Period_DEF_VAL, VAL_TYPE_INT32);

    set_config_list_item(cl_free_mem_cong, CL_FreeMem_Cong,
                strlen(CL_FreeMem_Cong), '\0', 0,
                CL_FreeMem_Cong_DEF_VAL, VAL_TYPE_INT32);

    set_config_list_item(cl_nnSimulator, CL_NNSimulator_on,
                strlen(CL_NNSimulator_on), '\0', 0,
                CL_NNSimulator_on_DEF_VAL, VAL_TYPE_BOOL);
    set_config_list_item(cl_dnSimulator, CL_DNSimulator_on,
                strlen(CL_DNSimulator_on), '\0', 0,
                CL_DNSimulator_on_DEF_VAL, VAL_TYPE_BOOL);
    set_config_list_item(cl_prealloc_flush_intvl, CL_Prealloc_FlushIntvl,
                strlen(CL_Prealloc_FlushIntvl), '\0', 0,
                CL_Prealloc_FlushIntval_DEF_VAL, VAL_TYPE_INT32);
}

#ifndef DMSC_USER_DAEMON
int32_t set_config_item (int8_t *conf_str, int32_t conf_str_len)
{
    config_item_index i;
    int8_t *chr_ptr, *log_str;
    int64_t val, *val64_ptr;
    int32_t len, *val32_ptr, ret;
    bool *valbool_ptr;

    ret = 1;
    if (strncmp(conf_str, CL_EI_TEST_ITEM,
            strlen(CL_EI_TEST_ITEM)) == 0) {
        if (!dms_client_config->enable_err_inject_test) {
            dms_printk(LOG_LVL_INFO, "DMSC INFO Config CL.EITest disable\n");
            return 0;
        }

        len = strlen(CL_EI_TEST_ITEM);
        chr_ptr = conf_str + len + 1;
        switch (*chr_ptr) {
        case '+':
            //NOTE: format : ConfigName=+ConfigValue, we need decrease by 2
            add_err_inject_test_item_with_string(chr_ptr + 1,
                    conf_str_len - len - 2,
                    &dms_client_config->ErrInjectList_Header,
                    &dms_client_config->ErrInjectList_Lock);
            break;
        case '-':
            remove_err_inject_test_item_with_string(chr_ptr + 1,
                    conf_str_len - len - 2,
                    &dms_client_config->ErrInjectList_Header,
                    &dms_client_config->ErrInjectList_Lock);
            break;
        case 'm':
            modify_err_inject_test_item_with_string(chr_ptr + 1,
                    conf_str_len - len - 2,
                    &dms_client_config->ErrInjectList_Header,
                    &dms_client_config->ErrInjectList_Lock);
            break;
        default:
            dms_printk(LOG_LVL_INFO, "DMSC INFO NOT such operation for "
                    "Config CL.EITestItem\n");
            break;
        }

        return 0;
    } else if (strncmp(conf_str, CL_EI_TEST, strlen(CL_EI_TEST))
            == 0) {
        chr_ptr = conf_str + strlen(CL_EI_TEST) + 1;
        sscanf(chr_ptr, "%lld", &val);
        dms_client_config->enable_err_inject_test = (bool)val;
        return 0;
    }

    for (i = cl_fc; i < cl_max_config_item_end; i++) {
        if (strncmp(conf_str, &(config_list[i].name[0]),
                config_list[i].name_len) == 0) {
            chr_ptr = conf_str + config_list[i].name_len + 1;
            switch (i) {
            case cl_log_level:
                if(strncmp(STR_DEBUG, chr_ptr, strlen(STR_DEBUG)) == 0) {
                    dms_client_config->log_level = LOG_LVL_DEBUG;
                    log_str = &(STR_DEBUG[0]);
                    len = strlen(STR_DEBUG);
                } else if(strncmp(STR_INFO, chr_ptr, strlen(STR_INFO)) == 0) {
                    dms_client_config->log_level = LOG_LVL_INFO;
                    log_str = &(STR_INFO[0]);
                    len = strlen(STR_INFO);
                } else if(strncmp(STR_WARN, chr_ptr, strlen(STR_WARN)) == 0) {
                    dms_client_config->log_level = LOG_LVL_WARN;
                    log_str = &(STR_WARN[0]);
                    len = strlen(STR_WARN);
                } else if(strncmp(STR_ERROR, chr_ptr, strlen(STR_ERROR)) == 0) {
                    dms_client_config->log_level = LOG_LVL_ERR;
                    log_str = &(STR_ERROR[0]);
                    len = strlen(STR_ERROR);
                } else if(strncmp(STR_ERROR, chr_ptr, strlen(STR_EMERG)) == 0) {
                    dms_client_config->log_level = LOG_LVL_EMERG;
                    log_str = &(STR_EMERG[0]);
                    len = strlen(STR_EMERG);
                } else {
                    dms_client_config->log_level = LOG_LVL_INFO;
                    log_str = &(STR_INFO[0]);
                    len = strlen(STR_INFO);
                }
                set_common_loglvl(dms_client_config->log_level);
                memcpy(config_list[i].value_str, chr_ptr, len);
                config_list[i].value_str[len] = '\0';
                break;
            case cl_rr:
                if(strncmp(STR_TRUE, chr_ptr, strlen(STR_TRUE)) == 0) {
                    dms_client_config->dms_RR_enabled = true;
                    len = strlen(STR_TRUE);
                    config_list[i].value = true;
                } else {
                    dms_client_config->dms_RR_enabled = false;
                    len = strlen(STR_FALSE);
                    config_list[i].value = false;
                }
                memcpy(config_list[i].value_str, chr_ptr, len);
                config_list[i].value_str[len] = '\0';
                break;
            case cl_disk_ra:
            case cl_mds_vo_port:
            case cl_sk_timeout:
                break;
            default:
                sscanf(chr_ptr, "%lld", &val);
                config_list[i].value = val;
                if (!IS_ERR_OR_NULL(config_list[i].val_ptr)) {
                    switch (config_list[i].val_type) {
                    case VAL_TYPE_BOOL:
                        valbool_ptr = (bool *)config_list[i].val_ptr;
                        *valbool_ptr = (bool)val;
                        break;
                    case VAL_TYPE_INT32:
                        val32_ptr = (int32_t *)config_list[i].val_ptr;
                        *val32_ptr = (int32_t)val;
                        break;
                    case VAL_TYPE_INT64:
                        val64_ptr = (int64_t *)config_list[i].val_ptr;
                        *val64_ptr = (int64_t)val;
                        break;
                    default:
                        break;
                    }
                }

                break;
            }

            ret = 0;
            break;
        }
    }

    return ret;
}

#if 0
static void print_dmsc_config (void)
{
    printk("CL.FlowControl=%d\n", dms_client_config->enable_fc);

    printk("CL.Read_Load_Balance=%d\n", dms_client_config->enable_read_balance);

    printk("CL.MetadataCache=%d\n", dms_client_config->enable_metadata_cache);
    printk("CL.MaxMetadataCacheSize=%lld\n", dms_client_config->max_meta_cache_sizes);
    printk("CL.MaxCapForCacheAllHit=%lld\n", dms_client_config->max_cap_cache_all_hit);

    printk("CL.MinDiskAck=%d\n", dms_client_config->CommitUser_min_DiskAck);

    printk("CL.FPCalculation=%d\n", dms_client_config->fp_method);
    printk("CL.ShareVolume=%d\n", dms_client_config->enable_share_volume);

    printk("CL.ShowPerformanceData=%d\n", dms_client_config->show_performance_data);

    printk("CL.LogLevel=%d\n", dms_client_config->log_level);

    printk("CL.SockHBInterval=%d\n", dms_client_config->dms_socket_keepalive_interval);
    printk("CL.IOReqTimeout=%d\n", dms_client_config->ioreq_timeout);


    printk("Rereplication.enable=%d\n", dms_client_config->dms_RR_enabled);

    printk("CL.WIOHighResp=%d\n", dms_client_config->high_resp_write_io);
    printk("CL.EITest=%d\n", dms_client_config->enable_err_inject_test);
}
#endif

int32_t show_configuration (int8_t *buffer, int32_t buff_len)
{
    config_item_index i;
    int32_t len;
    int8_t ipaddr_str[IPV4ADDR_NM_LEN];

    len = 0;
    for (i = cl_fc; i < cl_max_config_item_end; i++) {
        switch (i) {
        case cl_log_level:
        case cl_rr:
            len += sprintf(buffer + len, "%s=%s\n", config_list[i].name,
                    config_list[i].value_str);
            break;
        case cl_sk_timeout:
        case cl_disk_ra:
        case cl_mds_vo_port:
            break;
        case cl_mds_io_port:
        case cl_mds_ip:
            ccma_inet_aton(config_list[i].value, ipaddr_str, IPV4ADDR_NM_LEN);
            len += sprintf(buffer + len, "%s=%s", config_list[i].name,
                    ipaddr_str);
            len += sprintf(buffer + len, " (namenode ip)\n");
            break;
        case cl_mds_name:
            len += sprintf(buffer + len, "%s=%s\n", config_list[i].name,
                    config_list[i].value_str);
            break;
        case cl_fp:
            len += sprintf(buffer + len, "%s=%lld", config_list[i].name,
                    config_list[i].value);
            len += sprintf(buffer + len, " (0: disable, 1: sha1, 2: md5)\n");
            break;
        default:

            len += sprintf(buffer + len, "%s=%lld\n", config_list[i].name,
                    config_list[i].value);
            break;
        }
    }

    if (dms_client_config->enable_err_inject_test) {
        len += sprintf(buffer + len, "%s=%d (format=%s)\n",
                CL_EI_TEST, dms_client_config->enable_err_inject_test,
                DNAckEIJ_get_help());
    } else {
        len += sprintf(buffer + len, "%s=%d\n",
                CL_EI_TEST, dms_client_config->enable_err_inject_test);
    }

    if (dms_client_config->enable_err_inject_test) {
        len += show_err_inject_test_item(buffer + len,
                                     &dms_client_config->ErrInjectList_Header,
                                     &dms_client_config->ErrInjectList_Lock);
    }

    //print_dmsc_config();
    return len;
}

int32_t init_dmsc_config (int32_t drv_r_mode)
{
    config_item_index i;

    /* NOTE: We cannot replace it with discoC_mem_alloc, since config is first one
     *      be initialized.
     */
    dms_client_config = (config_t *)kmalloc(sizeof(config_t), GFP_KERNEL);
    if (unlikely(IS_ERR_OR_NULL(dms_client_config))) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN alloc mem fail for configuration fail\n");
        DMS_WARN_ON(true);
        return -ENOMEM;
    }

    init_config_list();

    dms_client_config->clientnode_running_mode = drv_r_mode;

    for (i = cl_fc; i < cl_max_config_item_end; i++) {
        switch (i) {
        case cl_fc:
            dms_client_config->enable_fc = (bool)config_list[i].value;
            config_list[i].val_ptr = &(dms_client_config->enable_fc);
            break;
        case cl_read_bal:
            dms_client_config->enable_read_balance = (bool)config_list[i].value;
            config_list[i].val_ptr = &(dms_client_config->enable_read_balance);
            break;
        case cl_read_hbs_next_dn:
            dms_client_config->read_hbs_next_dn = (uint32_t)config_list[i].value;
            config_list[i].val_ptr = &(dms_client_config->read_hbs_next_dn);
            break;
        case cl_mcache:
            dms_client_config->enable_metadata_cache =
                    (bool)config_list[i].value;
            config_list[i].val_ptr =
                    &(dms_client_config->enable_metadata_cache);
            break;
        case cl_mcache_max_items:
            dms_client_config->max_meta_cache_sizes = config_list[i].value;
            config_list[i].val_ptr = &(dms_client_config->max_meta_cache_sizes);
            break;
        case cl_mcache_max_allhit:
            dms_client_config->max_cap_cache_all_hit =
                    config_list[i].value;
            config_list[i].val_ptr =
                    &(dms_client_config->max_cap_cache_all_hit);
            break;
        case cl_min_disk_ack:
            dms_client_config->CommitUser_min_DiskAck = config_list[i].value;
            config_list[i].val_ptr =
                                &(dms_client_config->CommitUser_min_DiskAck);
            break;
        case cl_fp:
            dms_client_config->fp_method = config_list[i].value;
            config_list[i].val_ptr = &(dms_client_config->fp_method);
            break;
        case cl_share_vol:
            dms_client_config->enable_share_volume =
                    (bool)config_list[i].value;
            config_list[i].val_ptr =
                    &(dms_client_config->enable_share_volume);
            break;
        case cl_show_perf:
            dms_client_config->show_performance_data =
                    (bool)config_list[i].value;
            config_list[i].val_ptr =
                    &(dms_client_config->show_performance_data);
            break;
        case cl_log_level:
            dms_client_config->log_level = config_list[i].value;
            config_list[i].val_ptr =
                                &(dms_client_config->log_level);
            set_common_loglvl(dms_client_config->log_level);
            break;
        case cl_sk_timeout:
            break;
        case cl_hb_interval:
            dms_client_config->dms_socket_keepalive_interval =
                    config_list[i].value;
            config_list[i].val_ptr =
                    &(dms_client_config->dms_socket_keepalive_interval);
            break;
        case cl_disk_ra:
        case cl_mds_vo_port:
            break;
        case cl_mds_io_port:
            dms_client_config->mds_ioport = config_list[i].value;
            config_list[i].val_ptr = &(dms_client_config->mds_ioport);
            break;
        case cl_req_timeout:
            dms_client_config->ioreq_timeout = config_list[i].value;
            config_list[i].val_ptr = &(dms_client_config->ioreq_timeout);
            break;
        case cl_flush_timeout:
            dms_client_config->max_io_flush_wait_time = config_list[i].value;
            config_list[i].val_ptr = &(dms_client_config->max_io_flush_wait_time);
            break;
        case cl_rr:
            dms_client_config->dms_RR_enabled = (bool)config_list[i].value;
            config_list[i].val_ptr = &(dms_client_config->dms_RR_enabled);
            break;
        case cl_high_resp:
            dms_client_config->high_resp_write_io = (bool)config_list[i].value;
            config_list[i].val_ptr = &(dms_client_config->high_resp_write_io);
            break;
        case cl_high_resp_wtime:
            dms_client_config->time_wait_other_replica = (uint32_t)config_list[i].value;
            config_list[i].val_ptr = &(dms_client_config->time_wait_other_replica);
            break;
        case cl_wIOdone_conn_lostt:
            dms_client_config->time_wait_IOdone_conn_lost = config_list[i].value;
            config_list[i].val_ptr = &(dms_client_config->time_wait_IOdone_conn_lost);
            break;
        case cl_nr_ioreqs:
            dms_client_config->max_nr_io_reqs = config_list[i].value;
            config_list[i].val_ptr = &(dms_client_config->max_nr_io_reqs);
            break;
        case cl_req_mw_intvl:
            dms_client_config->req_merge_wait_intvl = config_list[i].value;
            config_list[i].val_ptr = &(dms_client_config->req_merge_wait_intvl);
            break;
        case cl_req_mw_maxcnt:
            dms_client_config->req_merge_wait_cnt = config_list[i].value;
            config_list[i].val_ptr = &(dms_client_config->req_merge_wait_cnt);
            break;
        case cl_req_req_merge:
            dms_client_config->req_merge_enable = (bool)config_list[i].value;
            config_list[i].val_ptr = &(dms_client_config->req_merge_enable);
            break;
        case cl_req_rsp_perf_warn:
            dms_client_config->reqrsp_time_thres_show = config_list[i].value;
            config_list[i].val_ptr = &(dms_client_config->reqrsp_time_thres_show);
            break;
        case cl_nnrate_warn:
            dms_client_config->nnrate_warn = config_list[i].value;
            config_list[i].val_ptr = &(dms_client_config->nnrate_warn);
            break;
        case cl_nnmaxreq_warn:
            dms_client_config->nnmaxreq_warn = config_list[i].value;
            config_list[i].val_ptr = &(dms_client_config->nnmaxreq_warn);
            break;
        case cl_ior_perf_warn:
            dms_client_config->ior_perf_warn = config_list[i].value;
            config_list[i].val_ptr = &(dms_client_config->ior_perf_warn);
            break;
        case cl_req_qdelay_warn:
            dms_client_config->req_qdelay_warn = config_list[i].value;
            config_list[i].val_ptr = &(dms_client_config->req_qdelay_warn);
            break;
        case cl_mds_ip:
            dms_client_config->mds_ipaddr = config_list[i].value;
            config_list[i].val_ptr = &(dms_client_config->mds_ipaddr);
            break;
        case cl_mds_name:
            dms_client_config->mds_name = config_list[i].value_str;
            config_list[i].val_ptr = dms_client_config->mds_name;
            break;
        case cl_cLinux_cache:
            dms_client_config->cLinux_cache = (bool)config_list[i].value;
            config_list[i].val_ptr = &(dms_client_config->cLinux_cache);
            break;
        case cl_cLinux_cache_period:
            dms_client_config->cLinux_cache_period = config_list[i].value;
            config_list[i].val_ptr = &(dms_client_config->cLinux_cache_period);
            break;
        case cl_free_mem_cong:
            dms_client_config->freeMem_cong = config_list[i].value;
            config_list[i].val_ptr = &(dms_client_config->freeMem_cong);
            break;
        case cl_nnSimulator:
            dms_client_config->NNSimulator_on = (bool)config_list[i].value;
            config_list[i].val_ptr = &(dms_client_config->NNSimulator_on);
            break;
        case cl_dnSimulator:
            dms_client_config->DNSimulator_on = (bool)config_list[i].value;
            config_list[i].val_ptr = &(dms_client_config->DNSimulator_on);
            break;
        case cl_prealloc_flush_intvl:
            dms_client_config->prealloc_flush_intvl = config_list[i].value;
            config_list[i].val_ptr = &(dms_client_config->prealloc_flush_intvl);
            break;
        default:
            break;
        }
    }

    dms_client_config->enable_err_inject_test = CL_EI_TEST_DEF_VAL;
    INIT_LIST_HEAD(&dms_client_config->ErrInjectList_Header);
    mutex_init(&dms_client_config->ErrInjectList_Lock);

    return 0;
}

void release_dmsc_config (void)
{
    /*
     * TODO: free_all will using discoC_mem_free and mem buffer already be free
     */
    free_all_err_inject_source(&dms_client_config->ErrInjectList_Header,
                               &dms_client_config->ErrInjectList_Lock);
    kfree(dms_client_config);
    dms_client_config = NULL;
}

#endif
