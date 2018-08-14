/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * common_util.h
 *
 */

#ifndef _COMMON_UTIL_H_
#define _COMMON_UTIL_H_

typedef enum {
    LOG_LVL_DEBUG,
    LOG_LVL_INFO,
    LOG_LVL_WARN,
    LOG_LVL_ERR,
    LOG_LVL_EMERG
} log_lvl_t;

#ifndef DMSC_USER_DAEMON
#include <linux/version.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/err.h>

//#define LINUX_LIST_DEBUG    1
#define KBytes(x) ((x) << (PAGE_SHIFT - 10))

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

#define MAGIC_NUM            0xa5a5a5a5a5a5a5a5ll
#define MAGIC_NUM_END        0xeeeeeeeeeeeeeeeell
#define PKT_MAGIC_NUM_NORDER    htonll(MAGIC_NUM)
#define PKT_MAGIC_NUM_END_NORDER    htonll(MAGIC_NUM_END)

typedef enum {
    cmp_1st_Min,
    cmp_2nd_Min,
    cmp_3rd_Min
} triple_cmp_type_t;

typedef struct request_state_trace {
    uint32_t state;
    spinlock_t state_lock;
} req_state_trace_t;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
extern long __must_check IS_ERR_OR_NULL(__force const void *ptr);
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
#define list_first_entry_or_null(ptr, type, member) \
    (!list_empty(ptr) ? list_first_entry(ptr, type, member) : NULL)
#endif

#define list_first_or_null(head) \
    (!list_empty(head) ? (head->next) : NULL)

extern log_lvl_t discoC_log_lvl;
#define dms_printk(LOG_LEVEL, fmt, args...) if(LOG_LEVEL >= discoC_log_lvl) printk(fmt, ##args)

#define DMS_MEM_READ_NEXT(dms_buf, dms_dest, dms_size) \
            memcpy(dms_dest, dms_buf, dms_size); dms_buf+= dms_size;

extern int32_t dms_list_del(struct list_head *entry);
extern int32_t dms_list_add(struct list_head *new, struct list_head *head);
extern int32_t dms_list_add_tail(struct list_head *new, struct list_head *head);
extern int32_t dms_list_move_tail(struct list_head *new, struct list_head *head);
extern int32_t dms_list_move_head(struct list_head *new, struct list_head *head);

extern uint64_t do_divide(uint64_t dividend, uint64_t divisor);

extern bool test_req_state(req_state_trace_t *req_state, uint32_t bit);
extern bool test_req_state_lock(req_state_trace_t *req_state,
                                        uint32_t bit);
extern bool test_and_set_request_state(req_state_trace_t *req_state, uint32_t bit,
        bool value);
extern void set_request_state(req_state_trace_t *req_state, uint32_t bit,
                              bool value);
extern void set_req_state_lock(req_state_trace_t *req_state,
                                        uint32_t bit, bool value);
extern void clear_request_state(req_state_trace_t *req_state);
extern void init_request_state(req_state_trace_t *req_state);

extern void decompose_triple(uint64_t val, int32_t *rbid, int32_t *len,
        int32_t *offset);
extern uint64_t compose_triple(int32_t rbid, int32_t len, int32_t offset);
extern uint64_t compose_len_off(int32_t len, int32_t offset);

extern uint32_t ccma_inet_ntoa(int8_t *ipaddr);
extern void ccma_inet_aton(uint32_t ipaddr, int8_t *buf, int32_t len);

extern int32_t head_sect_skip(uint8_t mask_val);
extern int32_t tail_sect_skip(uint8_t mask_val);

extern triple_cmp_type_t cmp_triple_int(int32_t num1,
        int32_t num2, int32_t num3);

#define INIT_REQUEST_STATE(__st)                 init_request_state(__st)
#define SET_REQUEST_STATE(__st, __tr, __val)     set_request_state(__st, __tr,\
                                                                   __val)

extern atomic_t discoC_err_cnt;

#define DMS_WARN_ON(cond)   do {\
    if (cond) { atomic_inc(&discoC_err_cnt); }\
    WARN_ON(cond); \
    } while (0)
#endif

#if 0
struct Cache_Operations {
    int32_t (*Invalidate_Payload_Cache_Items)(int8_t type, uint64_t volumeID,
            uint64_t start_LBID,
            uint32_t nr_LBIDs,
            int32_t (*check_condition)(struct payload *, void *));


    //update cache item by a LBID range
    int32_t (*Update_Payload_Cache_Items)(uint64_t volumeID,
                                          uint64_t start_phyLocationID,
                                          uint32_t nr_pl,
                                          struct payload **pl_array);

    int32_t (*Get_Payload_Cache_Items)(uint64_t volumeID,
                                       uint64_t start_phyLocationID,
                                       uint32_t nr_pl,
                                       struct payload **pl_array[]);

    int32_t (*Put_Payload_Cache_Items)(uint64_t volumeID,
                                       uint64_t start_phyLocationID,
                                       uint32_t nr_pl,
                                       struct payload **pl_array[]);
};
#endif

#endif /* _COMMON_UTIL_H_ */


