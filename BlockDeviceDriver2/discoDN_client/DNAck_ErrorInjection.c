/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * DNAck_ErrorInjection.c
 *
 * Change DNAck from normal response to different error types for testing
 *
 */
#include <linux/version.h>
#include <linux/mutex.h>
#include "../common/discoC_sys_def.h"
#include "../common/dms_kernel_version.h"
#include "../common/common_util.h"
#include "../common/discoC_mem_manager.h"
#include "DNAck_ErrorInjection_private.h"
#include "DNAck_ErrorInjection.h"

int8_t *DNAckEIJ_get_help (void) {
    return DNACK_EIJ_STR;
}

static DNAck_EIJ_item_t *_dnAck_create_eij_item (int8_t err_type,
        uint8_t per, uint8_t chk_type)
{
    DNAck_EIJ_item_t *test_item;

    test_item = discoC_mem_alloc(sizeof(DNAck_EIJ_item_t), GFP_NOWAIT);
    if (IS_ERR_OR_NULL(test_item)) {
        dms_printk(LOG_LVL_INFO, "DMSC INFO fail alloc mem EIJ Test item\n");
        return NULL;
    }
    test_item->errType = err_type;
    test_item->errPercentage = per;
    test_item->err_chkType = chk_type;
    INIT_LIST_HEAD(&(test_item->entry_dntarget));

    return test_item;
}

static DNAck_EIJ_target_t *_dnAck_create_target (uint32_t dn_ipaddr,
        uint32_t dn_port, uint32_t rwdir)
{
    DNAck_EIJ_target_t *dn_target;

    dn_target = discoC_mem_alloc(sizeof(DNAck_EIJ_target_t), GFP_NOWAIT);
    if (IS_ERR_OR_NULL(dn_target)) {
        dms_printk(LOG_LVL_INFO, "DMSC INFO fail alloc mem EIJ target\n");
        return NULL;
    }

    dn_target->dn_ipaddr = dn_ipaddr;
    dn_target->dn_port = dn_port;
    dn_target->rwdir = rwdir;
    dn_target->percentage_all = 0;
    INIT_LIST_HEAD(&(dn_target->eij_item_pool));
    INIT_LIST_HEAD(&(dn_target->entry_eijpool));

    return dn_target;
}

static DNAck_EIJ_item_t *_dnAck_find_eijItem (struct list_head *item_pool,
        uint32_t errType)
{
    DNAck_EIJ_item_t *cur, *next, *found;

    found = NULL;
    list_for_each_entry_safe (cur, next, item_pool, entry_dntarget) {
        if (cur->errType == errType) {
            found = cur;
            break;
        }
    }

    return found;
}

#define Found_And_Forbidden_Add_Test_Item   1
#define Found_And_Test_Item                 2

static int32_t _add_EIJ_item_nolock (DNAck_EIJ_target_t *dn_target,
        DNAck_EIJ_item_t *eijitem)
{
    DNAck_EIJ_item_t *found;
    int32_t ret, per;

    found = _dnAck_find_eijItem(&dn_target->eij_item_pool, eijitem->errType);
    if (IS_ERR_OR_NULL(found)) {
        ret = Found_And_Test_Item;

        per = dn_target->percentage_all + eijitem->errPercentage;
        if (per < 10) {
            dn_target->percentage_all = per;
            list_add_tail(&eijitem->entry_dntarget, &dn_target->eij_item_pool);
        } else {
            dms_printk(LOG_LVL_INFO,
                    "DMSC INFO Fail to add, over limit (>= 10) "
                    "total = %d new_item->Percentage %d\n",
                    per, eijitem->errPercentage);
            ret = Found_And_Forbidden_Add_Test_Item;
        }
    } else {
        ret = Found_And_Forbidden_Add_Test_Item;
    }

    return ret;
}

static DNAck_EIJ_target_t *_find_EIJ_target_nolock (uint32_t dn_ipaddr,
        uint32_t dn_port, uint32_t rwdir)
{
    DNAck_EIJ_target_t *cur, *next, *found;
    struct list_head *target_pool;

    target_pool = &(g_dnAck_mgr.eij_target_pool);
    found = NULL;

    list_for_each_entry_safe (cur, next, target_pool, entry_eijpool) {
        if (IS_ERR_OR_NULL(cur)) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN EIJ target pool broken\n");
            break;
        }

        if (cur->dn_ipaddr == dn_ipaddr && cur->dn_port == dn_port &&
                cur->rwdir == rwdir) {
            found = cur;
            break;
        }
    }

    return found;
}

int32_t DNAckEIJ_add_item (uint32_t dn_ipaddr, uint32_t dn_port, uint32_t rwdir,
        uint32_t err_type, uint32_t err_per, uint32_t err_chktype)
{
    DNAck_EIJ_target_t *dn_target, *found_tgt;
    DNAck_EIJ_item_t *eijitem;
    int32_t check_result;

    dms_printk(LOG_LVL_INFO, "DMSC INFO try add EIJ item: "
            "%u %u %u %u %u %u\n",
            dn_ipaddr, dn_port, rwdir, err_type, err_per, err_chktype);

    dn_target = _dnAck_create_target(dn_ipaddr, dn_port, rwdir);
    if (IS_ERR_OR_NULL(dn_target)) {
        return -ENOMEM;
    }

    eijitem = _dnAck_create_eij_item(err_type, err_per, err_chktype);
    if (IS_ERR_OR_NULL(eijitem)) {
        discoC_mem_free(dn_target);
        return -ENOMEM;
    }

    mutex_lock(&g_dnAck_mgr.eij_target_plock);

    found_tgt = _find_EIJ_target_nolock(dn_ipaddr, dn_port, rwdir);
    if (IS_ERR_OR_NULL(found_tgt)) {
        dn_target->percentage_all += eijitem->errPercentage;
        list_add_tail(&eijitem->entry_dntarget, &dn_target->eij_item_pool);
        list_add_tail(&dn_target->entry_eijpool, &g_dnAck_mgr.eij_target_pool);
        eijitem = NULL;
        dn_target = NULL;
    } else {
        check_result = _add_EIJ_item_nolock(found_tgt, eijitem);
        if (check_result == Found_And_Test_Item) {
            eijitem = NULL;
        }
    }

    mutex_unlock(&g_dnAck_mgr.eij_target_plock);

    if (IS_ERR_OR_NULL(eijitem) == false) {
        discoC_mem_free(eijitem);
    }

    if (IS_ERR_OR_NULL(dn_target) == false) {
        discoC_mem_free(dn_target);
    }

    return 0;
}

static DNAck_EIJ_item_t *_rm_EIJ_item_nolock (DNAck_EIJ_target_t *dn_target,
        uint32_t errType)
{
    DNAck_EIJ_item_t *found;

    found = _dnAck_find_eijItem(&(dn_target->eij_item_pool), errType);
    if (IS_ERR_OR_NULL(found) == false) {
        list_del(&found->entry_dntarget);
        dn_target->percentage_all -= found->errPercentage;
    }

    return found;
}

void DNAckEIJ_rm_item (uint32_t dn_ipaddr, uint32_t dn_port, uint32_t rwdir,
        uint32_t err_type, uint32_t err_per, uint32_t err_chktype)
{
    DNAck_EIJ_target_t *found_tgt;
    DNAck_EIJ_item_t *found_item;

    found_item = NULL;
    mutex_lock(&g_dnAck_mgr.eij_target_plock);

    do {
        found_tgt = _find_EIJ_target_nolock(dn_ipaddr, dn_port, rwdir);
        if (found_tgt == NULL) {
            break;
        }

        found_item = _rm_EIJ_item_nolock(found_tgt, err_type);
        if (list_empty(&found_tgt->eij_item_pool) == false) {
            found_tgt = NULL;
            break;
        }

        list_del(&found_tgt->entry_eijpool);
    } while (0);

    mutex_unlock(&g_dnAck_mgr.eij_target_plock);

    if (IS_ERR_OR_NULL(found_item) == false) {
        discoC_mem_free(found_item);
    }

    if (IS_ERR_OR_NULL(found_tgt) == false) {
        discoC_mem_free(found_tgt);
    }
}

static void _target_free_all_item (DNAck_EIJ_target_t *tgt)
{
    DNAck_EIJ_item_t *cur, *next, *item_found;

    list_for_each_entry_safe (cur, next, &(tgt->eij_item_pool), entry_dntarget) {
        item_found = cur;
        list_del(&item_found->entry_dntarget);
        discoC_mem_free(item_found);
    }
}

#if 0
void DNAckEIJ_rm_target (uint32_t dn_ipaddr, uint32_t dn_port, uint32_t rwdir)
{
    DNAck_EIJ_target_t *found_tgt;
    bool rm_all_item;

    rm_all_item = false;

    mutex_lock(&g_dnAck_mgr.eij_target_plock);

    found_tgt = _find_EIJ_target_nolock(dn_ipaddr, dn_port, rwdir);
    if (IS_ERR_OR_NULL(found_tgt) == false) {
        list_del(&found_tgt->entry_eijpool);
        rm_all_item = true;
    }

    mutex_unlock(&g_dnAck_mgr.eij_target_plock);

    if (rm_all_item) {
        _target_free_all_item(found_tgt);
        discoC_mem_free(found_tgt);
    }
}
#endif

int32_t DNAckEIJ_update_item (uint32_t dn_ipaddr, uint32_t dn_port, uint32_t rwdir,
        uint32_t err_type, uint32_t err_per, uint32_t err_chktype)
{
    DNAck_EIJ_target_t *found_tgt;
    DNAck_EIJ_item_t *item_found;
    int32_t ret, total_per;

    ret = 0;

    mutex_lock(&g_dnAck_mgr.eij_target_plock);

    do {
        found_tgt = _find_EIJ_target_nolock(dn_ipaddr, dn_port, rwdir);
        if (IS_ERR_OR_NULL(found_tgt)) {
            ret = -ENOENT;
            break;
        }

        item_found = _dnAck_find_eijItem(&found_tgt->eij_item_pool, err_type);
        if (IS_ERR_OR_NULL(item_found)) {
            ret = -ENOENT;
            break;
        }

        total_per = found_tgt->percentage_all - item_found->errPercentage + err_per;
        if (total_per < 10) {
            item_found->errPercentage = total_per;
            item_found->err_chkType = err_chktype;

            ret = -EPERM;
            break;
        }
    } while (0);

    mutex_unlock(&g_dnAck_mgr.eij_target_plock);

    return ret;
}

void DNAckEIJ_add_item_str (int8_t *cmd_buffer, int32_t cmd_len)
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

        DNAckEIJ_add_item(src_ip, port, rw, err_type, per, chktype);
    } while (0);
}

void DNAckEIJ_rm_item_str (int8_t *cmd_buffer, int32_t cmd_len)
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
        DNAckEIJ_rm_item(src_ip, port, rw, err_type, per, chktype);

    } while (0);
}

void DNAckEIJ_update_item_str (int8_t *cmd_buffer, int32_t cmd_len)
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

        DNAckEIJ_update_item(src_ip, port, rw, err_type, per, chktype);
    } while (0);
}

//return = -1 : timeout error
//return = 0  : No timeout error and no timeout item
//return > 0  : No timeout error and has timeout item, and return value = range
static int32_t Meet_Timeout_Error (DNAck_EIJ_target_t *target,
        int32_t rec_cnt, int32_t start_range)
{
    DNAck_EIJ_item_t *cur, *next;
    int32_t ret = 0;

    list_for_each_entry_safe (cur, next, &(target->eij_item_pool), entry_dntarget) {
        if (cur->errType == ErrDNTimeout) {
            if((rec_cnt % 10 >= start_range) &&
                    (rec_cnt % 10 < (start_range + cur->errPercentage))) {
                ret = -1;
            } else {
                ret = cur->errPercentage;
            }
            break;
        }
    }

    return ret;
}

static int32_t Meet_Exception_Error (DNAck_EIJ_target_t *target,
        int32_t rec_cnt, int32_t start_range)
{
    DNAck_EIJ_item_t *cur, *next;
    int32_t ret = 0;

    list_for_each_entry_safe (cur, next, &(target->eij_item_pool), entry_dntarget) {
        if (cur->errType == ErrDNException) {
            if ((rec_cnt % 10 >= start_range) &&
                    (rec_cnt % 10 < (start_range + cur->errPercentage))) {
                ret = -1;
            } else {
                ret = cur->errPercentage;
            }
            break;
        }
    }

    return ret;
}

static int32_t Meet_HBIDErr_Error (DNAck_EIJ_target_t *target,
        int32_t rec_cnt, int32_t start_range)
{
    DNAck_EIJ_item_t *cur, *next;
    int32_t ret = 0;

    list_for_each_entry_safe (cur, next, &(target->eij_item_pool), entry_dntarget) {
        if (cur->errType == ErrDNHBIDNotMatch) {
            if ((rec_cnt % 10 >= start_range) &&
                    (rec_cnt % 10 < (start_range + cur->errPercentage))) {
                ret = -1;
            } else {
                ret = cur->errPercentage;
            }
            break;
        }
    }

    return ret;
}

int32_t DNAckEIJ_meet_error (uint32_t src_ipaddr, int32_t src_port, int32_t rw)
{
    DNAck_EIJ_target_t *found_tgt;
    int32_t check_value = 0, range = 0, ret = ErrDNNotError;

    mutex_lock(&g_dnAck_mgr.eij_target_plock);

    do {
        found_tgt = _find_EIJ_target_nolock(src_ipaddr, src_port, rw);
        if (IS_ERR_OR_NULL(found_tgt)) {
            break;
        }

        atomic_inc(&found_tgt->receive_cnt);
        check_value = Meet_Timeout_Error(found_tgt,
                atomic_read(&found_tgt->receive_cnt) - 1, 0);
        if (check_value == -1) {
            ret = ErrDNTimeout;
            break;
        }

        range = range + check_value;
        check_value = Meet_Exception_Error(found_tgt,
                atomic_read(&found_tgt->receive_cnt) - 1, range);
        if (check_value == -1) {
            ret = ErrDNException;
            break;
        }

        range = range + check_value;
        check_value = Meet_HBIDErr_Error(found_tgt,
                atomic_read(&found_tgt->receive_cnt) - 1, range);
        if (check_value == -1) {
            ret = ErrDNHBIDNotMatch;
            break;
        }
    } while (0);

    mutex_unlock(&g_dnAck_mgr.eij_target_plock);

    return ret;
}

int32_t DNAckEIJ_show_errorItem (int8_t *cmd_buffer, int32_t max_len)
{
    DNAck_EIJ_target_t *cur, *next;
    DNAck_EIJ_item_t *item_cur, *item_next;
    struct list_head *target_pool;
    int32_t len;
    int8_t ip_buffer[16];

    len = 0;
    target_pool = &(g_dnAck_mgr.eij_target_pool);

    mutex_lock(&g_dnAck_mgr.eij_target_plock);

    list_for_each_entry_safe (cur, next, &(g_dnAck_mgr.eij_target_pool), entry_eijpool) {
        if (IS_ERR_OR_NULL(cur)) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN EIJ pool broken\n");
            break;
        }

        list_for_each_entry_safe (item_cur, item_next,
                &(cur->eij_item_pool), entry_dntarget) {
            if (IS_ERR_OR_NULL(item_cur)) {
                dms_printk(LOG_LVL_WARN, "DMSC WARN EIJ item list broken\n");
                break;
            }

            ccma_inet_aton(cur->dn_ipaddr, ip_buffer, 16);
            len = len + sprintf(cmd_buffer + len, "CL.EITestItem=%s %d ",
                    ip_buffer, cur->dn_port);
            if (cur->rwdir == 0) {
                len = len + sprintf(cmd_buffer+len, "read ");
            } else {
                len = len + sprintf(cmd_buffer+len, "write ");
            }

            if (item_cur->errType == ErrDNException) {
                len = len + sprintf(cmd_buffer+len, "exception ");
            } else if(item_cur->errType == ErrDNHBIDNotMatch) {
                len = len + sprintf(cmd_buffer+len, "hbid_err ");
            } else {
                len = len + sprintf(cmd_buffer+len, "timeout ");
            }

            len = len + sprintf(cmd_buffer+len, "%d ", item_cur->errPercentage);
            if (item_cur->err_chkType == ChkTypeSequential) {
                len = len + sprintf(cmd_buffer+len, "Seq\n");
            } else {
                len = len + sprintf(cmd_buffer+len, "random\n");
            }
        }
    }

    mutex_unlock(&g_dnAck_mgr.eij_target_plock);

    return len;
}

void DNAckEIJ_init_manager (void)
{
    INIT_LIST_HEAD(&g_dnAck_mgr.eij_target_pool);
    mutex_init(&g_dnAck_mgr.eij_target_plock);
    atomic_set(&g_dnAck_mgr.num_eij_target, 0);
    atomic_set(&g_dnAck_mgr.num_eij_item, 0);
}

static void _DNAckEIJ_free_all_target (void)
{
    DNAck_EIJ_target_t *cur, *next, *res_found;

FREE_ALL_EIT:
    mutex_lock(&g_dnAck_mgr.eij_target_plock);

    res_found = NULL;
    list_for_each_entry_safe (cur, next, &g_dnAck_mgr.eij_target_pool, entry_eijpool) {
        if (IS_ERR_OR_NULL(cur)) {
            dms_printk(LOG_LVL_WARN, "DMSC WARN EI src list broken\n");
            break;
        }

        res_found = cur;
        break;
    }

    mutex_unlock(&g_dnAck_mgr.eij_target_plock);

    if (IS_ERR_OR_NULL(res_found) == false) {
        _target_free_all_item(res_found);
        discoC_mem_free(res_found);
        goto FREE_ALL_EIT;
    }
}

void DNAckEIJ_rel_manager (void)
{
    _DNAckEIJ_free_all_target();
}

static int8_t *get_next_token (int8_t *cmd_buffer, int32_t cmd_buf_len,
        int32_t *token_len)
{
    int32_t i, next_start;
    int8_t *ptr_start, *ptr_end;


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

    for (i = next_start; i < cmd_buf_len; i++) {
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
    return (ptr_start + token_len);
}

static int8_t *get_port (int8_t *cmd_buffer, int32_t *len, uint32_t *src_port)
{
    int8_t *ptr_start;
    int32_t token_len;

    ptr_start = get_next_token(cmd_buffer, *len, &token_len);
    if (IS_ERR_OR_NULL(ptr_start)) {
        return NULL;
    }

    sscanf(ptr_start, "%u", src_port);
    *len = *len - token_len;
    return (ptr_start + token_len);
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
    sscanf(ptr_start, "%u", rw);
    *len = *len - token_len;

    return (ptr_start + token_len);
}

static int8_t *get_ErrType (int8_t *cmd_buffer, int32_t *len, uint32_t *errtype)
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

    return (ptr_start + token_len);
}

static int8_t *get_per (int8_t *cmd_buffer, int32_t *len, uint32_t *per)
{
    int8_t *ptr_start;
    int32_t token_len;

    ptr_start = get_next_token(cmd_buffer, *len, &token_len);
    if (IS_ERR_OR_NULL(ptr_start)) {
        return NULL;
    }
    sscanf(ptr_start, "%u", per);
    *len = *len - token_len;

    return (ptr_start + token_len);
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

    return (ptr_start + token_len);
}
