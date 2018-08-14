/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * SelfTest.c
 *
 */

#include <linux/version.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <net/sock.h>
#include "common/discoC_sys_def.h"
#include "common/common_util.h"
#include "common/dms_kernel_version.h"
#include "common/discoC_mem_manager.h"
#include "SelfTest.h"
#include "config/dmsc_config.h"
#include "common/dms_client_mm.h"
#include "metadata_manager/metadata_manager.h"
#include "common/thread_manager.h"
#include "discoDN_client/discoC_DNC_Manager_export.h"
#include "payload_manager/payload_manager_export.h"
#include "io_manager/discoC_IO_Manager.h"
#include "vdisk_manager/volume_manager.h"
#include "connection_manager/conn_manager_export.h"
#include "discoNN_client/discoC_NNC_Manager_api.h"
#include "common/common_util.h"

static struct ErrInjectionTestItem *create_test_item (int8_t err_type,
        uint8_t per, uint8_t chk_type)
{
    struct ErrInjectionTestItem *test_item;

    test_item = discoC_mem_alloc(sizeof(struct ErrInjectionTestItem), GFP_NOWAIT);
    if (IS_ERR_OR_NULL(test_item)) {
        dms_printk(LOG_LVL_INFO,
                "DMSC INFO fail to allocate memory for EI Test item\n");
        return NULL;
    }
    test_item->ErrType = err_type;
    test_item->Percentage = per;
    test_item->check_type = chk_type;
    INIT_LIST_HEAD(&(test_item->list_next_err));

    return test_item;
}

static struct ErrInjectionSource * create_test_source (uint32_t src_ip,
        uint32_t src_port, uint32_t rw)
{
    struct ErrInjectionSource * test_src;

    test_src = discoC_mem_alloc(sizeof(struct ErrInjectionSource), GFP_NOWAIT);
    if (test_src == NULL) {
        dms_printk(LOG_LVL_INFO,
                "DMSC INFO fail to allocate memory for EI Test src\n");
        return NULL;
    }

    test_src->source_ip = src_ip;
    test_src->source_port = src_port;
    test_src->rw = rw;
    INIT_LIST_HEAD(&(test_src->list_next_source));
    INIT_LIST_HEAD(&(test_src->test_item.list_next_err));

    return test_src;
}

void add_err_inject_test_item (struct list_head *src_list,
        struct mutex *src_list_lock, uint32_t src_ip_int, uint32_t src_port,
        uint32_t rw, uint32_t err_type, uint32_t per, uint32_t chk_type)
{
#define Found_And_Forbidden_Add_Test_Item	1
#define Found_And_Test_Item					2
#define Not_Found_And_Add_Source_Item		3
    struct ErrInjectionSource * cur, *next, *new_src;
    struct ErrInjectionTestItem * item_cur, *item_next, *new_item;
    uint32_t check_result = Not_Found_And_Add_Source_Item;
    int32_t total_per = 0;

    dms_printk(LOG_LVL_INFO, "DMSC INFO Try to add EI item: "
            "%d %d %d %d %d %d\n",
            src_ip_int, src_port, rw, err_type, per, chk_type);
    new_src = create_test_source(src_ip_int, src_port, rw);
    if (IS_ERR_OR_NULL(new_src)) {
        return;
    }

    new_item = create_test_item(err_type, per, chk_type);
    if (IS_ERR_OR_NULL(new_item)) {
        discoC_mem_free(new_src);
        new_src = NULL;
        return;
    }

    mutex_lock(src_list_lock);
    check_result = Not_Found_And_Add_Source_Item;
    list_for_each_entry_safe (cur, next, src_list, list_next_source) {
        if (IS_ERR_OR_NULL(cur)) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN EI src list broken\n");
            break;;
        }

	    if (cur->source_ip == src_ip_int &&
	            cur->source_port == src_port && cur->rw == rw) {
	        check_result = Found_And_Test_Item;
	        list_for_each_entry_safe (item_cur, item_next,
	                &(cur->test_item.list_next_err), list_next_err) {
	            if (IS_ERR_OR_NULL(item_cur)) {
	                dms_printk(LOG_LVL_WARN, "DMSC WARN EI item list "
	                        "broken\n");
	                break;
	            }

	            total_per = total_per + item_cur->Percentage;
	            if (item_cur->ErrType == err_type) {
	                check_result = Found_And_Forbidden_Add_Test_Item;
	                break;
	            }
	        }

	        if (check_result == Found_And_Test_Item) {
	            if(total_per + new_item->Percentage < 10) {
	                list_add_tail(&new_item->list_next_err,
	                        &cur->test_item.list_next_err);
	                new_item = NULL;
	            } else {
	                dms_printk(LOG_LVL_INFO,
	                        "DMSC INFO Fail to add, over limit (>= 10) "
	                        "total = %d new_item->Percentage %d\n",
	                        total_per, new_item->Percentage);
	                check_result = Found_And_Forbidden_Add_Test_Item;
	            }
	        }
	        break;
	    }
	}

    if (check_result == Not_Found_And_Add_Source_Item) {
        list_add_tail(&new_src->list_next_source, src_list);
        list_add_tail(&new_item->list_next_err,
                &new_src->test_item.list_next_err);
        new_item = NULL;
        new_src = NULL;
    }
    mutex_unlock(src_list_lock);

    if (!IS_ERR_OR_NULL(new_item)) {
        discoC_mem_free(new_item);
        new_item = NULL;
    }

    if (!IS_ERR_OR_NULL(new_src)) {
        discoC_mem_free(new_src);
        new_src = NULL;
    }
    return;
}

void remove_err_inject_test_item (struct list_head *src_list,
        struct mutex *src_list_lock, uint32_t src_ip_int,
        uint32_t src_port, uint32_t rw, uint32_t err_type, uint32_t per,
        uint32_t chk_type)
{
    struct ErrInjectionSource *cur, *next;
    struct ErrInjectionTestItem *item_cur, *item_next, *item_found;
    bool src_list_empty;

    item_found = NULL;
    src_list_empty = false;
    mutex_lock(src_list_lock);
    list_for_each_entry_safe (cur, next, src_list, list_next_source) {
        if (IS_ERR_OR_NULL(cur)) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN EI src list broken\n");
            break;;
        }

        if (cur->source_ip == src_ip_int && cur->source_port == src_port &&
                cur->rw == rw) {
            list_for_each_entry_safe(item_cur, item_next,
                    &(cur->test_item.list_next_err), list_next_err) {
                if (IS_ERR_OR_NULL(item_cur)) {
                    dms_printk(LOG_LVL_WARN,
                            "DMSC WARN EI item list broken\n");
                    break;
                }

                if (item_cur->ErrType == err_type) {
                    item_found = item_cur;
                    list_del(&item_found->list_next_err);
                    break;
                }
            }
            src_list_empty = list_empty(&(cur->test_item.list_next_err));
            break;
        }
    }

    mutex_unlock(src_list_lock);

    if (!IS_ERR_OR_NULL(item_found)) {
		discoC_mem_free(item_found);
		item_found = NULL;
	}

    if (src_list_empty) {
        remove_err_inject_test_source(src_list, src_list_lock,
                src_ip_int, src_port, rw);
    }
}

void remove_err_inject_test_source (struct list_head *src_list,
        struct mutex *src_list_lock, uint32_t src_ip_int, uint32_t src_port,
        uint32_t rw)
{
    struct ErrInjectionSource *cur, *next, *res_found;
    struct ErrInjectionTestItem *item_cur, *item_next, *item_found;

    res_found = NULL;
    mutex_lock(src_list_lock);
    list_for_each_entry_safe (cur, next, src_list, list_next_source) {
        if (IS_ERR_OR_NULL(cur)) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN EI src list broken\n");
            break;;
        }

        if (cur->source_ip == src_ip_int && cur->source_port == src_port &&
                cur->rw == rw) {
            res_found = cur;
            list_del(&res_found->list_next_source);
            break;
        }
    }
    mutex_unlock(src_list_lock);

    if (!IS_ERR_OR_NULL(res_found)) {
        list_for_each_entry_safe (item_cur, item_next,
                &res_found->test_item.list_next_err, list_next_err) {
            if (IS_ERR_OR_NULL(item_cur)) {
                dms_printk(LOG_LVL_WARN,
                        "DMSC WARN EI item list broken\n");
                break;
            }

            item_found = item_cur;
            list_del(&item_found->list_next_err);
            discoC_mem_free(item_found);
            item_found = NULL;
        }

        discoC_mem_free(res_found);
        res_found = NULL;
    }
}

void modify_err_inject_test_item (struct list_head *src_list,
        struct mutex *src_list_lock, uint32_t src_ip_int,
        uint32_t src_port, uint32_t rw, uint32_t err_type,
        uint32_t per, uint32_t chk_type)
{
    struct ErrInjectionSource *cur, *next;
    struct ErrInjectionTestItem *item_cur, *item_next, *item_found;
    uint8_t total_per = 0;

    item_found = NULL;
    mutex_lock(src_list_lock);
    list_for_each_entry_safe (cur, next, src_list, list_next_source) {
        if (IS_ERR_OR_NULL(cur)) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN EI src list broken\n");
            break;;
        }

        if (cur->source_ip == src_ip_int && cur->source_port == src_port &&
                cur->rw == rw) {
            list_for_each_entry_safe(item_cur, item_next,
                    &(cur->test_item.list_next_err), list_next_err) {
                if (IS_ERR_OR_NULL(item_cur)) {
                    dms_printk(LOG_LVL_WARN,
                            "DMSC WARN EI item list broken\n");
                    break;
                }

                if(item_cur->ErrType == err_type) {
                    item_found = item_cur;
                } else {
                    total_per = total_per + item_cur->Percentage;
                }
            }

            if (!IS_ERR_OR_NULL(item_found)) {
                if ((total_per + per) < 10) {
                    item_found->Percentage = per;
                    item_found->check_type = chk_type;
                }
            }
            break;
        }
    }
    mutex_unlock(src_list_lock);
}

void free_all_err_inject_source (struct list_head *src_list,
        struct mutex *src_list_lock)
{
    struct ErrInjectionSource *cur, *next, *res_found;

FREE_ALL_EIT:
    mutex_lock(src_list_lock);
    res_found = NULL;
    list_for_each_entry_safe (cur, next, src_list, list_next_source) {
        if (IS_ERR_OR_NULL(cur)) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN EI src list broken\n");
            break;
        }

        res_found = cur;
        break;
    }
    mutex_unlock(src_list_lock);

    if (!IS_ERR_OR_NULL(res_found)) {
        remove_err_inject_test_source(src_list, src_list_lock,
                res_found->source_ip, res_found->source_port, res_found->rw);
        goto FREE_ALL_EIT;
    }
}

static int8_t *get_next_token (int8_t *cmd_buffer, int32_t cmd_buf_len,
        int32_t *token_len)
{
    int32_t i, next_start;
    int8_t *ptr_start = NULL, *ptr_end = NULL;
    ptr_start = cmd_buffer;
    ptr_end = cmd_buffer;
    *token_len = 0;

    next_start = cmd_buf_len;
    for (i = 0; i < cmd_buf_len; i++) {
        if (cmd_buffer[i] != ' ') {
            ptr_start = &(cmd_buffer[i]);
            ptr_end = ptr_start;
            next_start = i;
            break;
        }
    }

    for ( i = next_start;i < cmd_buf_len; i++) {
        if (cmd_buffer[i] == ' ') {
            break;
        } else {
            ptr_end++;
            *token_len = *token_len + 1;
        }
    }

	if (ptr_start == ptr_end) {
	    ptr_start = NULL;
	}

	return ptr_start;
}

static int8_t *get_ip (int8_t *cmd_buffer, int32_t *len, uint32_t *src_ip)
{
    int8_t *ptr_start;
    int32_t token_len;

    ptr_start = get_next_token(cmd_buffer, *len, &token_len);

    if (IS_ERR_OR_NULL(ptr_start)) {
        return NULL;
    }
    *src_ip = ccma_inet_ntoa(ptr_start);
    *len = *len - token_len;
    return ptr_start+token_len;
}

static int8_t *get_port (int8_t *cmd_buffer, int32_t *len, uint32_t *src_port)
{
    int8_t *ptr_start;
    int32_t token_len;

    ptr_start = get_next_token(cmd_buffer, *len, &token_len);

    if (IS_ERR_OR_NULL(ptr_start)) {
        return NULL;
    }

    sscanf(ptr_start,"%d",src_port);
    *len = *len - token_len;
    return ptr_start+token_len;
}

static int8_t *get_rw (int8_t *cmd_buffer, int32_t *len, uint32_t *rw)
{
    int8_t *ptr_start;
    int32_t token_len;

    ptr_start = get_next_token(cmd_buffer, *len, &token_len);

    if (IS_ERR_OR_NULL(ptr_start)) {
        return NULL;
    }

    if (strncmp(ptr_start, "read", 4) == 0) {
        *rw = 0;
    } else if (strncmp(ptr_start, "write", 5) == 0) {
        *rw = 1;
    } else {
        return NULL;
    }
    sscanf(ptr_start,"%d", (unsigned int *)rw);
    *len = *len - token_len;
    return ptr_start+token_len;
}

static int8_t *get_ErrType (int8_t *cmd_buffer, int32_t *len,
        uint32_t *errtype)
{
    int8_t *ptr_start;
    int32_t token_len;

    ptr_start = get_next_token(cmd_buffer, *len, &token_len);

    if (IS_ERR_OR_NULL(ptr_start)) {
        return NULL;
    }

    if (strncmp(ptr_start, "exception", 9) == 0) {
        *errtype = ErrDNException;
    } else if (strncmp(ptr_start, "hbid_err", 8) == 0) {
        *errtype = ErrDNHBIDNotMatch;
    } else if (strncmp(ptr_start, "timeout", 7) == 0) {
        *errtype = ErrDNTimeout;
    } else {
        return NULL;
    }
    *len = *len - token_len;
    return ptr_start+token_len;
}

static int8_t *get_per (int8_t *cmd_buffer, int32_t *len, uint32_t *per)
{
    int8_t *ptr_start;
    int32_t token_len;

    ptr_start = get_next_token(cmd_buffer, *len, &token_len);

    if (IS_ERR_OR_NULL(ptr_start)) {
        return NULL;
    }
    sscanf(ptr_start,"%d", (unsigned int *)per);
    *len = *len - token_len;
    return ptr_start+token_len;
}

static int8_t *get_ChkType (int8_t *cmd_buffer, int32_t *len,
        uint32_t *chktype)
{
    int8_t *ptr_start;
    int32_t token_len;

    ptr_start = get_next_token(cmd_buffer, *len, &token_len);
    if (IS_ERR_OR_NULL(ptr_start)) {
        return NULL;
    }

    if (strncmp(ptr_start, "Seq", 3) == 0) {
        *chktype = ChkTypeSequential;
    } else if (strncmp(ptr_start, "random", 6) == 0) {
        *chktype = CkhTypeRandom;
    } else {
        return NULL;
    }
    *len = *len - token_len;
    return ptr_start+token_len;
}

void add_err_inject_test_item_with_string (int8_t *cmd_buffer, int32_t cmd_len,
        struct list_head *src_list, struct mutex *src_list_lock)
{
    uint32_t src_ip = 0, port = 0, rw = 0, err_type = 0, per = 0, chktype = 0;
    int8_t *ptr_next = cmd_buffer;
    int32_t len = cmd_len;

    do {
        ptr_next = get_ip(ptr_next, &len, &src_ip);
        if (IS_ERR_OR_NULL(ptr_next)) {
            break;
        }

        ptr_next = get_port(ptr_next, &len, &port);
        if (IS_ERR_OR_NULL(ptr_next)) {
            break;
        }

        ptr_next = get_rw(ptr_next, &len, &rw);
        if (IS_ERR_OR_NULL(ptr_next)) {
            break;
        }

        ptr_next = get_ErrType(ptr_next, &len, &err_type);
        if (IS_ERR_OR_NULL(ptr_next)) {
            break;
        }

        ptr_next = get_per(ptr_next, &len, &per);
        if (IS_ERR_OR_NULL(ptr_next)) {
            break;
        }

        ptr_next = get_ChkType(ptr_next, &len, &chktype);
        if (IS_ERR_OR_NULL(ptr_next)) {
            break;
        }

        add_err_inject_test_item(src_list, src_list_lock, src_ip, port, rw,
                err_type, per, chktype);
    } while (0);
}

void remove_err_inject_test_item_with_string (int8_t *cmd_buffer,
        int32_t cmd_len, struct list_head *src_list,
        struct mutex *src_list_lock)
{
    uint32_t src_ip = 0, port = 0, rw = 0, err_type, per, chktype;
	int8_t *ptr_next = cmd_buffer;
	int32_t len = cmd_len;

	do {
	    ptr_next = get_ip(ptr_next, &len, &src_ip);
	    if (IS_ERR_OR_NULL(ptr_next)) {
	        break;
	    }

	    ptr_next = get_port(ptr_next, &len, &port);
	    if (IS_ERR_OR_NULL(ptr_next)) {
            break;
	    }

	    ptr_next = get_rw(ptr_next, &len, &rw);
	    if (IS_ERR_OR_NULL(ptr_next)) {
	        break;
	    }

	    ptr_next = get_ErrType(ptr_next, &len, &err_type);
	    if (IS_ERR_OR_NULL(ptr_next)) {
	        break;
	    }

	    ptr_next = get_per(ptr_next, &len, &per);
	    if (IS_ERR_OR_NULL(ptr_next)) {
	        break;
	    }

	    ptr_next = get_ChkType(ptr_next, &len, &chktype);
	    if (IS_ERR_OR_NULL(ptr_next)) {
	        break;
	    }
	    remove_err_inject_test_item(src_list, src_list_lock, src_ip, port, rw,
	            err_type, per, chktype);
	} while (0);
}

void modify_err_inject_test_item_with_string (int8_t *cmd_buffer,
        int32_t cmd_len, struct list_head *src_list,
        struct mutex *src_list_lock)
{
	uint32_t src_ip = 0, port = 0, rw = 0, err_type, per, chktype;
	int8_t *ptr_next = cmd_buffer;
	int32_t len = cmd_len;

	do {
	    ptr_next = get_ip(ptr_next, &len, &src_ip);
	    if (IS_ERR_OR_NULL(ptr_next)) {
	        break;
	    }

	    ptr_next = get_port(ptr_next, &len, &port);
	    if (IS_ERR_OR_NULL(ptr_next)) {
	        break;
	    }

	    ptr_next = get_rw(ptr_next, &len, &rw);
	    if (IS_ERR_OR_NULL(ptr_next)) {
	        break;
	    }

	    ptr_next = get_ErrType(ptr_next, &len, &err_type);
	    if (IS_ERR_OR_NULL(ptr_next)) {
	        break;
	    }

	    ptr_next = get_per(ptr_next, &len, &per);
	    if (IS_ERR_OR_NULL(ptr_next)) {
	        break;
	    }

	    ptr_next = get_ChkType(ptr_next, &len, &chktype);
	    if (IS_ERR_OR_NULL(ptr_next)) {
	        break;
	    }
	    modify_err_inject_test_item(src_list, src_list_lock, src_ip, port, rw,
	            err_type, per, chktype);
	} while (0);
}

//return = -1 : timeout error
//return = 0  : No timeout error and no timeout item
//return > 0  : No timeout error and has timeout item, and return value = range
static int32_t Meet_Timeout_Error (struct ErrInjectionSource *ejSrc,
        int32_t rec_cnt, int32_t start_range)
{
    struct ErrInjectionTestItem *item_cur, *item_next;
    int32_t ret = 0;

    list_for_each_entry_safe (item_cur, item_next,
            &(ejSrc->test_item.list_next_err), list_next_err)
    {
        if(item_cur->ErrType == ErrDNTimeout) {
            if((rec_cnt % 10 >= start_range) &&
                    (rec_cnt % 10 < (start_range + item_cur->Percentage))) {
                ret = -1;
            } else {
                ret = item_cur->Percentage;
            }
            break;
        }
    }

    return ret;
}

static int32_t Meet_Exception_Error(struct ErrInjectionSource *ejSrc,
        int32_t rec_cnt, int32_t start_range)
{
    struct ErrInjectionTestItem *item_cur, *item_next;
    int32_t ret = 0;

    list_for_each_entry_safe (item_cur, item_next,
            &(ejSrc->test_item.list_next_err), list_next_err) {
        if (item_cur->ErrType == ErrDNException) {
            if ((rec_cnt % 10 >= start_range) &&
                    (rec_cnt % 10 < (start_range + item_cur->Percentage))) {
                ret = -1;
            } else {
                ret = item_cur->Percentage;
            }
            break;
        }
    }

    return ret;
}

static int32_t Meet_HBIDErr_Error(struct ErrInjectionSource *ejSrc,
        int32_t rec_cnt, int32_t start_range)
{
    struct ErrInjectionTestItem *item_cur, *item_next;
    int32_t ret = 0;

    list_for_each_entry_safe (item_cur, item_next,
            &(ejSrc->test_item.list_next_err), list_next_err) {
        if (item_cur->ErrType == ErrDNHBIDNotMatch) {
            if ((rec_cnt % 10 >= start_range) &&
                    (rec_cnt % 10 < (start_range + item_cur->Percentage))) {
                ret = -1;
            } else {
                ret = item_cur->Percentage;
            }
            break;
        }
    }

    return ret;
}

int32_t meet_error_inject_test (struct list_head *src_list,
        struct mutex *src_list_lock, uint32_t src_ipaddr, int32_t src_port,
        int32_t rw)
{
	struct ErrInjectionSource *cur, *next;
	int32_t check_value = 0, range = 0, ret = ErrDNNotError;

	mutex_lock(src_list_lock);
	list_for_each_entry_safe (cur, next, src_list, list_next_source) {
	    if (IS_ERR_OR_NULL(cur)) {
	        dms_printk(LOG_LVL_WARN, "DMSC WARN EI src list broken\n");
	        break;
	    }

	    if (cur->source_ip == src_ipaddr && cur->source_port == src_port &&
	            cur->rw == rw) {
	        atomic_inc(&cur->receive_cnt);
	        check_value = Meet_Timeout_Error(cur,
	                atomic_read(&cur->receive_cnt) - 1, 0);
	        if (check_value == -1) {
	            ret = ErrDNTimeout;
	            break;
	        }

	        range = range + check_value;
	        check_value = Meet_Exception_Error(cur,
	                atomic_read(&cur->receive_cnt) - 1, range);
	        if (check_value == -1) {
	            ret = ErrDNException;
	            break;
	        }

	        range = range + check_value;
	        check_value = Meet_HBIDErr_Error(cur,
	                atomic_read(&cur->receive_cnt) - 1, range);
	        if (check_value == -1) {
	            ret = ErrDNHBIDNotMatch;
	            break;
	        }
	    }
	}
	mutex_unlock(src_list_lock);
	return ret;
}

int32_t show_err_inject_test_item (int8_t *cmd_buffer,
        struct list_head *src_list, struct mutex *src_list_lock)
{
    struct ErrInjectionSource *cur, *next;
    struct ErrInjectionTestItem * item_cur, *item_next;
    int32_t len;
    int8_t ip_buffer[16];

    len = 0;
    mutex_lock(src_list_lock);
    list_for_each_entry_safe (cur, next, src_list, list_next_source) {
        if (IS_ERR_OR_NULL(cur)) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN EI src list broken\n");
            break;
        }

        list_for_each_entry_safe (item_cur, item_next,
                &(cur->test_item.list_next_err), list_next_err) {
            if (IS_ERR_OR_NULL(item_cur)) {
                dms_printk(LOG_LVL_WARN,
                        "DMSC WARN EI item list broken\n");
                break;
            }

            ccma_inet_aton(cur->source_ip, ip_buffer, 16);
            len = len + sprintf(cmd_buffer+len, "CL.EITestItem=%s %d ",
                    ip_buffer, cur->source_port);
            if (cur->rw == 0) {
                len = len + sprintf(cmd_buffer+len, "read ");
            } else {
                len = len + sprintf(cmd_buffer+len, "write ");
            }

            if (item_cur->ErrType == ErrDNException) {
                len = len + sprintf(cmd_buffer+len, "exception ");
            } else if(item_cur->ErrType == ErrDNHBIDNotMatch) {
                len = len + sprintf(cmd_buffer+len, "hbid_err ");
            } else {
                len = len + sprintf(cmd_buffer+len, "timeout ");
            }

            len = len + sprintf(cmd_buffer+len, "%d ", item_cur->Percentage);
            if (item_cur->check_type == ChkTypeSequential) {
                len = len + sprintf(cmd_buffer+len, "Seq\n");
            } else {
                len = len + sprintf(cmd_buffer+len, "random\n");
            }
        }
	}
	mutex_unlock(src_list_lock);

	return len;
}
