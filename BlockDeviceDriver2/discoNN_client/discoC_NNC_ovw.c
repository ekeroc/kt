/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_NNC_ovw.c
 *
 */

#include <linux/version.h>
#include <linux/blkdev.h>
#include <net/sock.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include "../common/discoC_sys_def.h"
#include "../common/common_util.h"
#include "../common/dms_kernel_version.h"
#include "../common/discoC_mem_manager.h"
#include "../common/thread_manager.h"
#include "../metadata_manager/metadata_manager.h"
#include "../io_manager/discoC_IOSegment_Request.h"
#include "../io_manager/discoC_IO_Manager.h"
#include "../vdisk_manager/volume_manager.h"
#include "discoC_NNC_ovw.h"
#include "discoC_NNC_ovw_private.h"
#include "../connection_manager/conn_manager_export.h"
#include "../housekeeping.h"
#include "../config/dmsc_config.h"
#include "discoC_NNC_Manager_api.h"

static int32_t _gen_nnovw_conn_hb_pkt (int8_t *pkt_buffer, int32_t max_buff_size)
{
    int8_t *ptr;
    uint64_t tmp_ptr_ll;
    int32_t pkt_len;
    uint16_t tmp_ptr_s;

    if (IS_ERR_OR_NULL(pkt_buffer)) {
        return -EINVAL;
    }

    ptr = pkt_buffer;
    pkt_len = 0;

    tmp_ptr_ll = htonll(MAGIC_NUM);
    memcpy(ptr, &tmp_ptr_ll, sizeof(uint64_t));
    ptr += sizeof(uint64_t);
    pkt_len += sizeof(uint64_t);

    tmp_ptr_s = (uint16_t)htons(DMS_NN_REQUEST_CMD_TYPE_OVW_UPDATE_HB);
    memcpy(ptr, &tmp_ptr_s, sizeof(uint16_t));
    ptr += sizeof(uint16_t);
    pkt_len += sizeof(uint16_t);

    tmp_ptr_s = (uint16_t)htons(DMS_NN_CMD_OVW_SUB_TYPE_REQUEST);
    memcpy(ptr, &tmp_ptr_s, sizeof(uint16_t));
    ptr+= sizeof(uint16_t);
    pkt_len += sizeof(uint16_t);

    tmp_ptr_ll = htonll(1L); //dummy volume id
    memcpy(ptr, &tmp_ptr_ll, sizeof(uint64_t));
    ptr += sizeof(uint64_t);
    pkt_len += sizeof(uint64_t);

    tmp_ptr_ll = htonll(MAGIC_NUM_END);
    memcpy(ptr, &tmp_ptr_ll, sizeof(uint64_t));
    ptr+= sizeof(uint64_t);
    pkt_len += sizeof(uint64_t);

    return pkt_len;
}

bool ovw_get (volume_device_t *dev, uint64_t LBID)
{
    int8_t data;
    uint64_t myf_ops;
    volume_ovw_t *ovw_data;

    if (unlikely(IS_ERR_OR_NULL(dev))) {
        dms_printk(LOG_LVL_INFO, "DMSC INFO null dev when get ovw\n");
        return false;
    }

    ovw_data = &dev->ovw_data;
    if (!ovw_data->ovw_init) {
        dms_printk(LOG_LVL_INFO, "DMSC INFO ovw not ready when get ovw\n");
        return false;
    }

    if (unlikely(IS_ERR_OR_NULL(ovw_data->ovw_memory))) {
        dms_printk(LOG_LVL_INFO,
                "DMSC INFO overwritten memory is NULL for volume id: %d\n",
                dev->vol_id);
        return false;
    }

    down_read(&ovw_data->ovw_lock);
    myf_ops = (LBID >> BIT_LEN_BYTE); // do / 8
    data = ovw_data->ovw_memory[myf_ops];
    up_read(&ovw_data->ovw_lock);

    //return (test_bit(LBID%8, (const volatile void *)&data) != 0);
    return (test_bit(LBID & BYTE_MOD_NUM, (const volatile void *)&data) != 0); // do % 8
}

bool ovw_set (volume_device_t *dev, uint64_t LBID, char bitvalue)
{
    int8_t data;
    uint64_t myf_ops;
    volume_ovw_t *ovw_data;

    if (unlikely(IS_ERR_OR_NULL(dev))) {
        dms_printk(LOG_LVL_INFO, "DMSC INFO null dev when set ovw\n");
        return false;
    }

    ovw_data = &dev->ovw_data;
    if (!ovw_data->ovw_init) {
        dms_printk(LOG_LVL_INFO, "DMSC INFO ovw not ready when set ovw\n");
        return false;
    }

    down_write(&ovw_data->ovw_lock);
    myf_ops = (LBID >> BIT_LEN_BYTE);
    data = ovw_data->ovw_memory[myf_ops];
    if (bitvalue) {
        set_bit(LBID & BYTE_MOD_NUM, (volatile void *)&data);
    } else {
        clear_bit(LBID & BYTE_MOD_NUM, (volatile void *)&data);
    }

    ovw_data->ovw_memory[myf_ops] = data;
    up_write(&ovw_data->ovw_lock);

    return true;
}

bool ovw_clear (volume_device_t *dev)
{
    uint64_t myf_ops;
    uint64_t size;
    uint64_t capacity;
    char * ptr = NULL;
    volume_ovw_t *ovw_data;

    if (unlikely(IS_ERR_OR_NULL(dev))) {
        dms_printk(LOG_LVL_INFO, "DMSC INFO null dev when clear ovw\n");
        return false;
    }

    ovw_data = &dev->ovw_data;
    if (!ovw_data->ovw_init) {
        dms_printk(LOG_LVL_INFO, "DMSC INFO ovw not ready when clear ovw\n");
        return false;
    }

    ptr = ovw_data->ovw_memory;
    capacity = dev->capacity;
    size = (((uint64_t)capacity >> BIT_LEN_DMS_LB_SIZE) >> 3); // / 8 bytes = >> 3
    size = PAGE_ALIGN(size);

    myf_ops = 0;
    down_write(&ovw_data->ovw_lock);
    while (myf_ops < size) {
        memcpy(ptr, blank_buffer, OVW_BUF_SIZE*SIZEOF_CHAR);
        ptr += OVW_BUF_SIZE;
        myf_ops += OVW_BUF_SIZE;
    }
    up_write(&ovw_data->ovw_lock);

    return true;
}

int32_t set_vol_ovw_range (volume_device_t *vol, uint64_t lbid_s, uint32_t lbid_len,
        int8_t val)
{
    uint32_t i;

    for (i = 0; i < lbid_len; i++) {
        if (!ovw_set(vol, lbid_s, val)) {
            return -EINVAL;
        }
        lbid_s++;
    }

    return 0;
}

static ovw_req * find_ovw_req (uint64_t volid,
        int8_t set_req_state, int8_t state, int8_t *old_state)
{
    ovw_req * cur, *next, *found;

    if (!nn_ovw_manager_ready) {
        return NULL;
    }

    found = NULL;
    mutex_lock(&ovwreq_working_queue_lock);
    list_for_each_entry_safe (cur, next, &ovwreq_working_queue.list, list) {
        if (unlikely(IS_ERR_OR_NULL(cur))) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN ovw working q broken\n");
            DMS_WARN_ON(true);
            break;
        }

        if (cur->vol_id != volid) {
            continue;
        }

        found = cur;
        mutex_lock(&found->req_state_lock);
        *old_state = found->ovw_req_state;

        if (set_req_state && *old_state != OVW_REQ_STATE_REQ_CANCEL) {
            if (state == OVW_REQ_STATE_REQ_CANCEL) {
                if (*old_state == OVW_REQ_STATE_REQ_OVW_START ||
                        *old_state == OVW_REQ_STATE_REQ_SEND_TO_NN ||
                        *old_state == OVW_REQ_STATE_REQ_PROCESS_RESPONSE_DONE) {
                    found->ovw_ptr = NULL;
                }

                if (*old_state == OVW_REQ_STATE_REQ_OVW_START) {
                    atomic_dec(&ovwreq_num_of_req_wait_for_send);
                }
            }

            if (*old_state == OVW_REQ_STATE_REQ_SEND_TO_NN) {
                atomic_dec(&ovwreq_num_of_req_send_to_nn);
            }

            found->ovw_req_state = state;
        }

        mutex_unlock(&found->req_state_lock);
        break;
    }

    mutex_unlock(&ovwreq_working_queue_lock);

	return found;
}

static ovw_req *create_ovw_req (uint64_t volid, int8_t *ovw_mem_ptr)
{
    ovw_req *ovw_req;

    ovw_req = discoC_mem_alloc(SIZEOF_OVW_REQ, GFP_NOWAIT | GFP_ATOMIC);

    if (IS_ERR_OR_NULL(ovw_req)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN: cannot generate ovw request\n");
        DMS_WARN_ON(true);
        return NULL;
    }

    ovw_req->req_header.begin_magic_number = htonll(MAGIC_NUM);
    ovw_req->req_header.req_cmd_type =
            htons(DMS_NN_REQUEST_CMD_TYPE_OVERWRITTEN);
    ovw_req->req_header.sub_type = htons(DMS_NN_CMD_OVW_SUB_TYPE_REQUEST);
    ovw_req->req_header.vol_id_byte_order = htonll(volid);
    ovw_req->req_header.end_magic_number = htonll(MAGIC_NUM_END);
    ovw_req->ovw_ptr = ovw_mem_ptr;
    ovw_req->ovw_req_state = OVW_REQ_STATE_REQ_OVW_START;
    ovw_req->vol_id = volid;
    mutex_init(&ovw_req->req_state_lock);
    INIT_LIST_HEAD(&ovw_req->list);

    return ovw_req;
}

static void add_ovw_req (uint64_t volid, int8_t *ovw_mem_ptr)
{
	int8_t old_state;
	ovw_req * ovw_req;

	if (!nn_ovw_manager_ready || (find_ovw_req(volid, 0,
	        OVW_REQ_STATE_REQ_OVW_START, &old_state) != NULL)) {
	    return;
	}

	ovw_req = create_ovw_req(volid, ovw_mem_ptr);
	if(IS_ERR_OR_NULL(ovw_req)) {
	    return;
	}

	mutex_lock(&ovwreq_working_queue_lock);
	atomic_inc(&ovwreq_working_queue_size);
	atomic_inc(&ovwreq_num_of_req_wait_for_send);
    if (dms_list_add_tail(&ovw_req->list, &ovwreq_working_queue.list)) {
        dms_printk(LOG_LVL_WARN, "DMSC WARN list_add_tail chk fail %s %d\n",
                __func__, __LINE__);
        DMS_WARN_ON(true);
        msleep(1000);
    }

	mutex_unlock(&ovwreq_working_queue_lock);

	if (atomic_read(&ovwreq_working_queue_size) > 0	&&
	        atomic_read(&ovwreq_num_of_req_send_to_nn) <=
	        max_ovw_request_can_be_send_out
			&& atomic_read(&ovwreq_num_of_req_wait_for_send) > 0 &&
			waitqueue_active(&ovwreq_working_queue_wq)) {
	    wake_up_interruptible_all(&ovwreq_working_queue_wq);
	}
}

static bool set_ovw_by_ptr (int8_t *ovw_mem_ptr, uint64_t LBID,
        int8_t bitvalue)
{
	uint64_t myf_ops;
	int8_t data;

	if (IS_ERR_OR_NULL(ovw_mem_ptr)) {
	    return false;
	}

	myf_ops = (LBID >> BIT_LEN_BYTE);
	data = ovw_mem_ptr[myf_ops];
	if (bitvalue) {
	    set_bit(LBID & BYTE_MOD_NUM, (volatile void *)&data);
	} else {
	    clear_bit(LBID & BYTE_MOD_NUM, (volatile void *)&data);
	}

	ovw_mem_ptr[myf_ops]=data;
	return true;
}

static int32_t _NNOVW_connected_post_fn (discoC_conn_t *conn, bool do_req_retry)
{
    int32_t ret;

    ret = 0;

    // create the name node socket receiver thread
    if (dms_client_config->clientnode_running_mode == discoC_MODE_SelfTest ||
            dms_client_config->clientnode_running_mode == discoC_MODE_SelfTest_NN_Only) {
        return ret;
    }

    conn->skrx_thread_ds = create_sk_rx_thread("dms_ovw_rcv", conn,
            nn_ovw_response_handler);
    if (IS_ERR_OR_NULL(conn->skrx_thread_ds)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN kthread dms_ovw_rcv creation fail\n");
        DMS_WARN_ON(true);
    }

    if (do_req_retry) {
        add_hk_task(TASK_NN_OVW_CONN_BACK_REQ_RETRY, (uint64_t *)conn);
    }

    return ret;
}

static void init_ovw_conn (uint32_t nnSrv_ipadr, uint32_t nnSrv_port)
{
    int8_t str_ipaddr[IPV4ADDR_NM_LEN];

    if (dms_client_config->NNSimulator_on == false) {
        ovw_cpe = connMgr_create_conn(nnSrv_ipadr,
                nnSrv_port, _NNOVW_connected_post_fn, _gen_nnovw_conn_hb_pkt,
                CONN_IMPL_SOCK);
    } else {
        ovw_cpe = connMgr_create_conn(nnSrv_ipadr,
                nnSrv_port, _NNOVW_connected_post_fn, _gen_nnovw_conn_hb_pkt,
                CONN_IMPL_NNOVW_SIMULAOR);
    }

    if (unlikely(IS_ERR_OR_NULL(ovw_cpe))) {
        ccma_inet_aton(nnSrv_ipadr, str_ipaddr, IPV4ADDR_NM_LEN);
		dms_printk(LOG_LVL_WARN,
		        "DMSC WARN Fail to get connection for %s:%d\n",
		        str_ipaddr, nnSrv_port);
	}
}

static ovw_req *get_ovw_req (void)
{
    ovw_req *cur, *next, *found;
    bool should_release;

    should_release = false;
    found = NULL;

    mutex_lock(&ovwreq_working_queue_lock);
    list_for_each_entry_safe (cur, next, &ovwreq_working_queue.list, list) {
        should_release = false;
        if (unlikely(IS_ERR_OR_NULL(cur))) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN ovw working q broken\n");
            DMS_WARN_ON(true);
            found = NULL;
            break;
        }

        found = cur;
        mutex_lock(&cur->req_state_lock);
        if (found->ovw_req_state == OVW_REQ_STATE_REQ_OVW_START) {
            should_release = false;
            found->ovw_req_state = OVW_REQ_STATE_REQ_SEND_TO_NN;
            atomic_inc(&ovwreq_num_of_req_send_to_nn);
            atomic_dec(&ovwreq_num_of_req_wait_for_send);
        } else if (found->ovw_req_state == OVW_REQ_STATE_REQ_CANCEL) {
            should_release = true;
            if (dms_list_del(&found->list)) {
                dms_printk(LOG_LVL_WARN, "DMSC WARN list_del chk fail %s %d\n",
                        __func__, __LINE__);
                DMS_WARN_ON(true);
                msleep(1000);
                continue;
            } else {
                atomic_dec(&ovwreq_working_queue_size);
            }
        } else {
            found = NULL;
        }

        mutex_unlock(&cur->req_state_lock);
        if (!IS_ERR_OR_NULL(found)) {
            break;
        }
	}

    mutex_unlock(&ovwreq_working_queue_lock);

    if (should_release) {
        if (!IS_ERR_OR_NULL(found->ovw_ptr)) {
            vfree(found->ovw_ptr);
            found->ovw_ptr = NULL;
        }

        discoC_mem_free(found);
        found = NULL;
    }

    return found;
}

static int nn_ovw_request_worker (void *thread_data)
{
    ovw_req *ovw_req;
    int32_t num_of_bytes_send;
    dmsc_thread_t *t_data = (dmsc_thread_t *)thread_data;

    while (t_data->running) {
        wait_event_interruptible(ovwreq_working_queue_wq,
                (atomic_read(&ovwreq_working_queue_size) > 0 &&
                atomic_read(&ovwreq_num_of_req_send_to_nn) <=
                                               max_ovw_request_can_be_send_out
                && atomic_read(&ovwreq_num_of_req_wait_for_send) > 0)
                || !t_data->running);

		ovw_req = get_ovw_req();

		//TODO: if ovw_cpe is null, we should create new one
		if (IS_ERR_OR_NULL(ovw_req) || IS_ERR_OR_NULL(ovw_cpe)) {
		    continue;
		}

		if (connMgr_get_connState(ovw_cpe) != SOCK_CONN) {
		    continue;
		}

		dms_printk(LOG_LVL_INFO, "DMSC INFO get ovw start for vol: %llu\n",
		                ovw_req->vol_id);
		num_of_bytes_send = ovw_cpe->connOPIntf.tx_send(ovw_cpe, &(ovw_req->req_header),
		        SIZEOF_CHAR*NN_OVW_REQUEST_SIZE, true, false);

		if (num_of_bytes_send != SIZEOF_CHAR*NN_OVW_REQUEST_SIZE) {
		    dms_printk(LOG_LVL_WARN,
		            "DMSC WARN conn send in-complete ovw req\n");
		    ovw_cpe->connOPIntf.resetConn(ovw_cpe);
		}
    }

    dms_printk(LOG_LVL_INFO, "DMSC INFO Ready to exit ovw worker\n");
	return 0;
}

static void init_namenode_ovw_threads (void)
{
    if (IS_ERR_OR_NULL(create_worker_thread("dms_ovw_worker",
            &ovwreq_working_queue_wq, NULL,
            nn_ovw_request_worker))) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN kthread dms_ovw_worker creation fail\n");
        DMS_WARN_ON(true);
    }
}

static void rm_ovw_req (ovw_req * req)
{
    ovw_req *cur, *next;

    mutex_lock(&ovwreq_working_queue_lock);
    list_for_each_entry_safe (cur, next, &ovwreq_working_queue.list, list) {
        if (IS_ERR_OR_NULL(cur)) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN ovw list broken when remove\n");
            DMS_WARN_ON(true);
            break;
        }

        if (cur == req) {
            if (dms_list_del(&req->list)) {
                dms_printk(LOG_LVL_WARN, "DMSC WARN list_del chk fail %s %d\n",
                        __func__, __LINE__);
                DMS_WARN_ON(true);
                msleep(1000);
                break;
            }

            atomic_dec(&ovwreq_working_queue_size);
            break;
        }
    }

    mutex_unlock(&ovwreq_working_queue_lock);
}


static void change_byteorder_nn_ovw_response (ovw_res *ovw_response_header)
{
    ovw_response_header->begin_magic_number =
	        ntohll(ovw_response_header->begin_magic_number);
	ovw_response_header->end_magic_number =
	        ntohll(ovw_response_header->end_magic_number);
	ovw_response_header->error_code =
	        ntohs(ovw_response_header->error_code);
	ovw_response_header->len_in_byte =
	        ntohll(ovw_response_header->len_in_byte);
	ovw_response_header->req_cmd_type =
	        ntohs(ovw_response_header->req_cmd_type);
	ovw_response_header->sub_type =
	        ntohs(ovw_response_header->sub_type);
	ovw_response_header->vol_id =
	        ntohll(ovw_response_header->vol_id);
}

static void release_all_ovw_request (void)
{
    ovw_req *cur, *next, *found;

RELEASE_OVW_REQ:

    found = NULL;
    mutex_lock(&ovwreq_working_queue_lock);
    if (!list_empty(&ovwreq_working_queue.list)) {
        list_for_each_entry_safe(cur, next, &ovwreq_working_queue.list, list) {
            if (IS_ERR_OR_NULL(cur)) {
                dms_printk(LOG_LVL_WARN,
                        "DMSC WARN ovw list broken when release\n");
                DMS_WARN_ON(true);
                found = NULL;
                break;
            }

            if (dms_list_del(&cur->list)) {
                dms_printk(LOG_LVL_WARN, "DMSC WARN list_del chk fail %s %d\n",
                        __func__, __LINE__);
                DMS_WARN_ON(true);
                msleep(1000);
                continue;
            }

            found = cur;
            break;
        }
    }
    mutex_unlock(&ovwreq_working_queue_lock);

    if (!IS_ERR_OR_NULL(found)) {
        if (!IS_ERR_OR_NULL(found->ovw_ptr)) {
            vfree(found->ovw_ptr);
            found->ovw_ptr = NULL;
        }

        discoC_mem_free(found);
        found = NULL;
        goto RELEASE_OVW_REQ;
    }
}

void rel_nnovw_manager (void) {
    nn_ovw_manager_ready = false;

    if ((dms_client_config->clientnode_running_mode == discoC_MODE_SelfTest) ||
        (dms_client_config->clientnode_running_mode == discoC_MODE_SelfTest_NN_Only)) {
        return;
    }

	release_all_ovw_request();
	if (waitqueue_active(&ovwreq_working_queue_wq)) {
		wake_up_interruptible_all(&ovwreq_working_queue_wq);
	}
}

int nn_ovw_response_handler (void *thread_data)
{
    struct connection_sock_wrapper *sk_wapper;
    ovw_res ovw_response_header;
    ovw_req *ovw_req = NULL;
    int32_t actual_receive_packet_size, set_ovw, recv_fail;
    int32_t expected_packet_recv_size, i;
    int64_t bytes_for_recv;
    uint64_t startLB, LBCnt, j, cnt;
    int8_t *payload_ptr;
    int8_t old_state;
    dmsc_thread_t *rx_thread_data;
    discoC_conn_t *goal;

    allow_signal(SIGKILL);

    rx_thread_data = (dmsc_thread_t *)thread_data;
    goal = (discoC_conn_t *)rx_thread_data->usr_data;
    if (IS_ERR_OR_NULL(goal)) {
        dms_printk(LOG_LVL_ERR,
                "DMSC ERROR nn ovw conn pool item is NULL\n");
        return 0;
    }

    sk_wapper = get_sock_wrapper(goal);
    if (IS_ERR_OR_NULL(sk_wapper)) {
        dms_printk(LOG_LVL_WARN,
                "DMSC WARN fail to get sk_wapper from %s:%d\n",
                goal->ipv4addr_str, goal->port);
        return 0;
    }

    while (!kthread_should_stop()) {
        memset(ovw_socket_recv_buffer, 0, SIZEOF_CHAR*OVW_SOCKET_RECV_BUFFER_SIZE);
        set_ovw = 0;
        old_state = 0;
        recv_fail = 0;

        actual_receive_packet_size = ovw_cpe->connOPIntf.rx_rcv(sk_wapper,
                &ovw_response_header, SIZEOF_OVW_RES);
        if (actual_receive_packet_size <= 0) {
            dms_printk(LOG_LVL_INFO,
                    "DMSC INFO fail to receive ovw response from namenode\n");
            if (actual_receive_packet_size == 0) {
                //TODO: why we need a msleep here?
                //      reason: when connection close, rx threads won't
                //              exist but retry to rcv and cause CPU high loading
                //
                msleep(1000);
            }
            continue;
        }

        change_byteorder_nn_ovw_response(&ovw_response_header);
        if (ovw_response_header.begin_magic_number != MAGIC_NUM ||
                ovw_response_header.end_magic_number != 0xeeeeeeeeeeeeeeeell) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN The data between CN-DN may lose order\n");
            //TODO Should reset connection
            continue;
        }

        ovw_req = find_ovw_req(ovw_response_header.vol_id, 1,
                OVW_REQ_STATE_REQ_PROCESS_RESPONSE, &old_state);

        //TODO : change magic number to meaningful words
        if (IS_ERR_OR_NULL(ovw_req)) {
            set_ovw = 0;
        } else {
            if (old_state == OVW_REQ_STATE_REQ_CANCEL) {
                set_ovw = 1;
            } else {
                set_ovw = 2;
            }
        }

        bytes_for_recv = ovw_response_header.len_in_byte;
        while (bytes_for_recv > 0) {
            recv_fail = 0;
            if (bytes_for_recv > OVW_SOCKET_RECV_BUFFER_SIZE) {
                expected_packet_recv_size = OVW_SOCKET_RECV_BUFFER_SIZE;
            } else {
                expected_packet_recv_size = bytes_for_recv;
            }

            actual_receive_packet_size =
                    ovw_cpe->connOPIntf.rx_rcv(sk_wapper, ovw_socket_recv_buffer,
                            expected_packet_recv_size*SIZEOF_CHAR);
            if (actual_receive_packet_size <= 0 ||
                    actual_receive_packet_size != expected_packet_recv_size) {

                if (actual_receive_packet_size == 0) {
                    //TODO: why we need a msleep here?
                    //      reason: when connection close, rx threads won't
                    //              exist but retry to rcv and cause CPU high loading
                    //
                    msleep(1000);
                }

                recv_fail = 1;
                break;
            }

            if (set_ovw == 2) {
                payload_ptr = ovw_socket_recv_buffer;
                cnt = (int32_t)actual_receive_packet_size /
                        ((int32_t)NN_OVW_RESPONSE_PAYLOAD_UNIT_SIZE);
                for (i = 0; i < cnt; i++) {
                    startLB = *((uint64_t*)payload_ptr);
                    LBCnt = *((uint64_t*)(payload_ptr + NN_OVW_RESPONSE_PAYLOAD_UNIT_SIZE/2));
                    startLB = ntohll(startLB);
                    LBCnt = ntohll(LBCnt);
                    for (j = 0; j < LBCnt; j++) {
                        set_ovw_by_ptr(ovw_req->ovw_ptr, startLB+j, 1);
                    }
                    payload_ptr+=NN_OVW_RESPONSE_PAYLOAD_UNIT_SIZE;
                }
            }

            bytes_for_recv -= ((long long)OVW_SOCKET_RECV_BUFFER_SIZE);
        }

        if (set_ovw == 0) {
            goto wakeup_wq;
        }

        if (set_ovw == 2) {
            mutex_lock(&ovw_req->req_state_lock);
            if (ovw_req->ovw_req_state != OVW_REQ_STATE_REQ_CANCEL) {
                if (recv_fail) {
                    ovw_req->ovw_req_state = OVW_REQ_STATE_REQ_OVW_START;
                    mutex_unlock(&ovw_req->req_state_lock);
                    goto wakeup_wq;
                } else {
                    ovw_req->ovw_req_state = OVW_REQ_STATE_REQ_PROCESS_RESPONSE_DONE;
                }
            } else {
                set_ovw = 1;
            }
            mutex_unlock(&ovw_req->req_state_lock);
        }

        dms_printk(LOG_LVL_INFO, "DMSC INFO get ovw done for vol: %llu\n",
                ovw_req->vol_id);
        set_vol_ovw_read_state(ovw_req->vol_id, false);
        rm_ovw_req(ovw_req);
        if (set_ovw == 1) {
            if (!IS_ERR_OR_NULL(ovw_req->ovw_ptr)) {
                vfree(ovw_req->ovw_ptr);
                ovw_req->ovw_ptr = NULL;
            }
        }
        discoC_mem_free(ovw_req);
        ovw_req = NULL;

wakeup_wq:
        if (atomic_read(&ovwreq_working_queue_size) > 0	&&
                atomic_read(&ovwreq_num_of_req_send_to_nn) <=
                    max_ovw_request_can_be_send_out &&
                atomic_read(&ovwreq_num_of_req_wait_for_send) > 0 &&
                waitqueue_active(&ovwreq_working_queue_wq)) {
            wake_up_interruptible_all(&ovwreq_working_queue_wq);
        }
    }

    deRef_conn_sock(sk_wapper);
	return 0;
}

void retry_nn_ovw_req (discoC_conn_t *goal)
{
    ovw_req *cur, *next;

    mutex_lock(&ovwreq_working_queue_lock);
    list_for_each_entry_safe (cur, next, &ovwreq_working_queue.list, list) {
        if (IS_ERR_OR_NULL(cur)) {
            dms_printk(LOG_LVL_WARN,
                    "DMSC WARN ovw list broken when retry\n");
            DMS_WARN_ON(true);
            break;
        }

        mutex_lock(&cur->req_state_lock);
        if (cur->ovw_req_state == OVW_REQ_STATE_REQ_SEND_TO_NN) {
            cur->ovw_req_state = OVW_REQ_STATE_REQ_OVW_START;
            atomic_dec(&ovwreq_num_of_req_send_to_nn);
            atomic_inc(&ovwreq_num_of_req_wait_for_send);
        }
        mutex_unlock(&cur->req_state_lock);
	}

    mutex_unlock(&ovwreq_working_queue_lock);

    if (atomic_read(&ovwreq_working_queue_size) > 0	&&
            atomic_read(&ovwreq_num_of_req_send_to_nn) <=
                max_ovw_request_can_be_send_out &&
            atomic_read(&ovwreq_num_of_req_wait_for_send) > 0 &&
            waitqueue_active(&ovwreq_working_queue_wq)) {
        wake_up_interruptible_all(&ovwreq_working_queue_wq);
    }
}

void init_nnovw_manager ()
{
    if (dms_client_config->clientnode_running_mode ==
            discoC_MODE_SelfTest ||
        dms_client_config->clientnode_running_mode ==
                   discoC_MODE_SelfTest_NN_Only) {
        return;
    }

    nn_ovw_manager_ready = true;
	memset(blank_buffer, 0, OVW_BUF_SIZE*SIZEOF_CHAR);
	mutex_init(&ovwreq_working_queue_lock);
	atomic_set(&ovwreq_working_queue_size, 0);
	atomic_set(&ovwreq_num_of_req_send_to_nn, 0);
	atomic_set(&ovwreq_num_of_req_wait_for_send, 0);
	INIT_LIST_HEAD(&ovwreq_working_queue.list);

	init_waitqueue_head(&ovwreq_working_queue_wq);

	init_ovw_conn(dms_client_config->mds_ipaddr, namenanode_ovw_port);
	init_namenode_ovw_threads();
}

void release_ovw (volume_device_t *dev)
{
    ovw_req * ovw_req = NULL;
    int8_t old_state;
    volume_ovw_t *ovw_data;

    if (unlikely(IS_ERR_OR_NULL(dev))) {
        dms_printk(LOG_LVL_INFO,
                "DMSC INFO null dev when release volume ovw\n");
        return;
    }

    ovw_data = &dev->ovw_data;
    ovw_data->ovw_init = false;
    ovw_req = find_ovw_req(dev->vol_id, 1, OVW_REQ_STATE_REQ_CANCEL,
            &old_state);
    if (IS_ERR_OR_NULL(ovw_req)) {
        if (!IS_ERR_OR_NULL(ovw_data->ovw_memory)) {
            vfree(ovw_data->ovw_memory);
            ovw_data->ovw_memory = NULL;
        }
    } else {
        if (old_state == OVW_REQ_STATE_REQ_OVW_START ||
                old_state == OVW_REQ_STATE_REQ_SEND_TO_NN ||
                old_state == OVW_REQ_STATE_REQ_PROCESS_RESPONSE_DONE) {
            if (!IS_ERR_OR_NULL(ovw_data->ovw_memory)) {
                vfree(ovw_data->ovw_memory);
                ovw_data->ovw_memory = NULL;
            }
        } else {
            ovw_data->ovw_memory = NULL;
        }

        if (atomic_read(&ovwreq_working_queue_size) > 0	&&
                atomic_read(&ovwreq_num_of_req_send_to_nn) <=
                    max_ovw_request_can_be_send_out &&
                waitqueue_active(&ovwreq_working_queue_wq)) {
            wake_up_interruptible_all(&ovwreq_working_queue_wq);
        }
    }
}

bool init_ovw (volume_device_t *dev, uint32_t volid, uint64_t capacity)
{
    uint64_t myf_ops, size;
    int8_t *ptr;
    volume_ovw_t *ovw_data;

    if (unlikely(IS_ERR_OR_NULL(dev))) {
        dms_printk(LOG_LVL_INFO, "DMSC INFO null dev when init volume ovw\n");
        return false;
    }

    size = ((uint64_t)capacity >> BIT_LEN_DMS_LB_SIZE) >> 3;
    ovw_data = &dev->ovw_data;
    init_rwsem(&(ovw_data->ovw_lock));
    size = PAGE_ALIGN(size);
    ovw_data->ovw_memory = vmalloc(size);
    ptr = ovw_data->ovw_memory;
    if (IS_ERR_OR_NULL(ovw_data->ovw_memory)) {
        dms_printk(LOG_LVL_ERR, "vmalloc allocation failed \n");
        ovw_data->ovw_memory = NULL;
        return false;
    }

    myf_ops = 0;
    while (myf_ops < size) {
        memcpy(ptr, blank_buffer, OVW_BUF_SIZE*SIZEOF_CHAR);
        ptr += OVW_BUF_SIZE;
    	myf_ops += OVW_BUF_SIZE;
    }

    ovw_data->ovw_init = true;
    ovw_data->ovw_reading = true;

    add_ovw_req(volid, ovw_data->ovw_memory);

    return true;
}
