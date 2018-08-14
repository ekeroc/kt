/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * dms_client_mm.h
 *
 */
#ifndef _DMS_CLIENT_MM_H_
#define _DMS_CLIENT_MM_H_


#define DEFAULT_IOREQ_POOL_SIZE 4000
#define EXTRA_BUFF_SIZE 128
#define MA_DUMMY_SIZE    0
#define MA_DUMMY_FLAG    0

//NOTE: Make sure consistent with dms_client_mm_private.h
typedef enum {
    MEM_Buf_IOReq = 0,
} mem_buf_type_t;

extern void *ccma_malloc_gut(mem_buf_type_t mtype, size_t m_size,
        gfp_t m_flags);
extern void ccma_free_gut(mem_buf_type_t mtype, void *buffer);

extern void *ccma_malloc_dofc(mem_buf_type_t mtype);

#define ccma_malloc_pool(m_type)\
    ccma_malloc_gut(m_type, MA_DUMMY_SIZE, MA_DUMMY_FLAG)

#define ccma_free_pool(m_type, buffer)\
        ccma_free_gut(m_type, buffer)

extern int32_t get_mem_pool_size(mem_buf_type_t mem_type);
extern size_t get_mem_item_size(mem_buf_type_t mem_type);
extern int32_t get_mem_usage_cnt(mem_buf_type_t mem_type);

extern void set_dmsc_mm_not_working(void);
extern void sleep_to_wait_mem(void);
extern void wake_up_mem_resource_wq(void);

extern void set_mm_cong_control(int32_t value);
extern int32_t client_mm_init(int32_t num_of_ioreq);
extern void client_mm_release(void);
#endif	/* _DMS_CLIENT_MM_H_ */
