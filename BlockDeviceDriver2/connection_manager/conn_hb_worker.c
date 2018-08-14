/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * conn_hb_worker.c
 *
 */

#include <linux/delay.h>
#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/dms_client_mm.h"
#include "../common/dms_kernel_version.h"
#include "../config/dmsc_config.h"
#include "../common/thread_manager.h"
#include "conn_manager_export.h"
#include "conn_state_fsm.h"
#include "conn_manager.h"
#include "conn_sock_api.h"

static void _send_hb_gut (discoC_conn_mgr_t *connMgr, int32_t bk_idx)
{
    discoC_conn_t *cur, *next, *conn;
    int32_t ret, size_to_send;

    down_read(&connMgr->conn_plock[bk_idx]);

    list_for_each_entry_safe (cur, next, &connMgr->conn_pool[bk_idx], list_connPool) {
        conn = cur;
        if (IS_ERR_OR_NULL(conn) || connMgr_get_connState(conn) == SOCK_DISCONN) {
            continue;
        }

        ret = 0;
        size_to_send = 0;

        if (IS_ERR_OR_NULL(conn->gen_hb_pkt_fn)) {
            continue;
        }

        size_to_send = conn->gen_hb_pkt_fn(connMgr->conn_hb_pkt_buffer,
                HB_PKT_BUFFER_SIZE);
        if (size_to_send <= 0) {
            continue;
        }

        ret = conn->connOPIntf.tx_send(conn, connMgr->conn_hb_pkt_buffer, size_to_send,
                true, false);

        if (ret != size_to_send) {
            dms_printk(LOG_LVL_INFO, "DMSC INFO fail send hb %s:%d "
                    "send result: %d socket state: %d\n",
                    conn->ipv4addr_str, conn->port, ret, connMgr_get_connState(conn));
        }
    }

    up_read(&connMgr->conn_plock[bk_idx]);
}

static void send_hb_to_target (discoC_conn_mgr_t *connMgr) {
    int32_t i;

    for (i = 0; i < CONN_POOL_HASH_SIZE; i++) {
        _send_hb_gut(connMgr, i);
    }
}

#if (DN_PROTOCOL_VERSION == 2) && (CN_CRASH_RECOVERY == 1)

static void send_clog_to_target (void) {
    discoC_conn_t *conn_entry, *tmp;
    uint64_t ts_send;
    int32_t ret, size_to_send;

    if (!test_and_clear_bit(0, (volatile void *)&ts_chk_bit)) {
        return;
    }

    ts_send = htonll(ts_cleanLog);
    memcpy(ptr_clog_ts, &ts_send, SIZEOF_U64);
    down_read(&conn_pool_lock);
    list_for_each_entry(tmp, &conn_pool, list_connPool) {
        conn_entry = tmp;

        if (IS_ERR_OR_NULL(conn_entry) ||
                connMgr_get_connState(conn_entry) == SOCK_DISCONN) {
            //TODO: How to handle if lose connection with a DN and re-conn
            continue;
        }

        ret = 0;
        size_to_send = 0;

        switch (conn_entry->conn_owner) {
        case CONN_OWN_DN:
            ret = conn_entry->connOPIntf.tx_send(conn_entry, clog_buffer, CL_PACKET_SIZE_DN, true, false);
            size_to_send = CL_PACKET_SIZE_DN;
            break;
        default:
            break;
        }

         if (ret != size_to_send) {
             dms_printk(LOG_LVL_INFO,
                     "DMSC INFO Fail to send clean log to  %s:%d "
                     "send result: %d socket state: %d\n",
                 conn_entry->ipv4addr_str, conn_entry->port, ret,
                 connMgr_get_connState(conn_entry));
         }
    }
    up_read(&conn_pool_lock);
}

static int32_t sock_hb_worker_fn (void *thread_data)
{
    int64_t ret_msleep;
    int64_t time_to_sleep;
    uint64_t last_hb_time, c_time;
    dmsc_thread_t *t_data = (dmsc_thread_t *)thread_data;
    allow_signal(SIGKILL); //lego: for reset

    time_to_sleep = CLEAN_LOG_INTVL;
    ret_msleep = 0;
    last_hb_time = jiffies_64;
    //time_to_sleep = dms_client_config->dms_socket_keepalive_interval -
    //        ret_msleep;

    while (t_data->running) {

        ret_msleep = msleep_interruptible(time_to_sleep);
        if (!t_data->running) {
            dms_printk(LOG_LVL_INFO,
                    "DMSC INFO DRIVER is shutdown leave it\n");
            return 0;
        }

        if (ret_msleep > 0) {
            /*
             * be wake up very frequently, might cause some high CPU loading,
             * we sleep 100 milli-seconds without any interruptible.
             */
            if (ret_msleep < 10) {
                msleep(100);
                time_to_sleep = time_to_sleep - 100;
            }

            time_to_sleep = time_to_sleep - ret_msleep;
        }

        if (ret_msleep == 0 || time_to_sleep <= 0) {
            send_clog_to_target();
            time_to_sleep = CLEAN_LOG_INTVL;

            c_time = jiffies_64;
            if (time_after64(c_time, last_hb_time)) {
                if (jiffies_to_msecs(c_time - last_hb_time) >=
                        dms_client_config->dms_socket_keepalive_interval) {
                    send_hb_to_target();
                    last_hb_time = c_time;
                }
            }

            time_to_sleep = CLEAN_LOG_INTVL;
        }
    }

    return 0;
}
#else
static int32_t sock_hb_worker_fn (void *thread_data)
{
    discoC_conn_mgr_t *connMgr;
    int64_t ret_msleep, time_to_sleep;
    dmsc_thread_t *t_data;

    allow_signal(SIGKILL); //lego: for reset

    t_data = (dmsc_thread_t *)thread_data;
    connMgr = (discoC_conn_mgr_t *)t_data->usr_data;

    ret_msleep = 0;
    time_to_sleep = dms_client_config->dms_socket_keepalive_interval -
            ret_msleep;

    while (t_data->running) {

        ret_msleep = msleep_interruptible(time_to_sleep);
        if (!t_data->running) {
            dms_printk(LOG_LVL_INFO,
                    "DMSC INFO DRIVER is shutdown leave it\n");
            return 0;
        }

        if (ret_msleep > 0) {
            /*
             * be wake up very frequently, might cause some high CPU loading,
             * we sleep 100 milli-seconds without any interruptible.
             */
            if (ret_msleep < 10) {
                msleep(100);
                time_to_sleep = time_to_sleep - 100;
            }

            time_to_sleep = time_to_sleep - ret_msleep;
        }

        if (!connMgr_running()) {
            time_to_sleep = dms_client_config->dms_socket_keepalive_interval;
            continue;
        }

        if (ret_msleep == 0 || time_to_sleep <= 0) {
            send_hb_to_target(connMgr);
            time_to_sleep = dms_client_config->dms_socket_keepalive_interval;
        }
    }

    return 0;
}
#endif

void create_hb_worker (discoC_conn_mgr_t *connMgr)
{
    connMgr->conn_hb_wker = create_worker_thread("dms_sk_hb_wker", NULL, connMgr,
            sock_hb_worker_fn);
    if (IS_ERR_OR_NULL(connMgr->conn_hb_wker)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN kthread dms_sk_hb_wker creation fail\n");
        DMS_WARN_ON(true);
    }
}

void stop_hb_worker (discoC_conn_mgr_t *connMgr)
{
    if (stop_worker_thread(connMgr->conn_hb_wker)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN fail stop conn hb worker\n");
        DMS_WARN_ON(true);
    }
}
