/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * SelfTest.h
 *
 */

#ifndef _SELFTEST_H_
#define _SELFTEST_H_


#define ErrDNNotError       0
#define ErrDNTimeout        1
#define ErrDNException      2
#define ErrDNHBIDNotMatch   3

#define ChkTypeSequential   1
#define CkhTypeRandom       2

struct ErrInjectionTestItem {
    unsigned int ErrType;
    unsigned int Percentage;
    unsigned int check_type;
    struct list_head list_next_err;
};

struct ErrInjectionSource {
    uint32_t source_ip;
    uint32_t source_port;
    uint32_t rw;
    atomic_t receive_cnt;
    struct ErrInjectionTestItem test_item;
    struct list_head list_next_source;
};
typedef struct ErrInjectionSource err_injection_t;

void add_err_inject_test_item(struct list_head *src_list,
        struct mutex *src_list_lock, uint32_t src_ip_int,
        uint32_t src_port, uint32_t rw, uint32_t err_type, uint32_t per,
        uint32_t chk_type);
void remove_err_inject_test_item(struct list_head *src_list,
        struct mutex *src_list_lock, uint32_t src_ip_int,
        uint32_t src_port, uint32_t rw, uint32_t err_type, uint32_t per,
        uint32_t chk_type);
void remove_err_inject_test_source(struct list_head *src_list,
        struct mutex *src_list_lock, uint32_t src_ip_int, uint32_t src_port,
        uint32_t rw);
void modify_err_inject_test_item(struct list_head *src_list,
        struct mutex *src_list_lock, uint32_t src_ip_int,
        uint32_t src_port, uint32_t rw, uint32_t err_type, uint32_t per,
        uint32_t chk_type);

void add_err_inject_test_item_with_string(int8_t *cmd_buffer, int32_t len,
        struct list_head *src_list, struct mutex *src_list_lock);
void remove_err_inject_test_item_with_string(int8_t *cmd_buffer, int32_t len,
        struct list_head *src_list, struct mutex *src_list_lock);
void modify_err_inject_test_item_with_string(int8_t *cmd_buffer, int32_t len,
        struct list_head *src_list, struct mutex *src_list_lock);
int32_t show_err_inject_test_item(int8_t *cmd_buffer,
        struct list_head *src_list, struct mutex *src_list_lock);
int32_t meet_error_inject_test(struct list_head *src_list,
        struct mutex *src_list_lock, uint32_t src_ipaddr, int32_t src_port,
        int32_t rw);
void free_all_err_inject_source(struct list_head *src_list,
                                struct mutex *src_list_lock);

#endif /* _SELFTEST_H_ */
