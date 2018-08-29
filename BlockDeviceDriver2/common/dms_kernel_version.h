/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * dms_kernel_version.h
 *
 *  Created on: 2012/6/8
 *      Author: 990158
 */

#ifndef _DMS_KERNEL_VERSION_H_
#define _DMS_KERNEL_VERSION_H_

#include <linux/version.h>
#include <linux/workqueue.h>
#include <linux/blkdev.h>

#define DMSC_sock_recvmsg(sock,msg_ptr,size,flag)   sock_recvmsg(sock,msg_ptr,flag)

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
#define hlist_for_each_entry_safe_ccma(tpos, pos, n, head, member)      \
        hlist_for_each_entry_safe(tpos, n, head, member)
#else
#define hlist_for_each_entry_safe_ccma(tpos, pos, n, head, member) \
        hlist_for_each_entry_safe(tpos, pos, n, head, member)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0)
#define DMSC_sock_sendmsg(sk,msg,len)   sock_sendmsg(sk,msg)
#define DMSC_iov_iter_init(msg,io_dir,iov_ptr,iov_len,data_len) iov_iter_init(&(msg.msg_iter), io_dir, iov_ptr, iov_len, data_len)
#else
#define DMSC_sock_sendmsg(sk,msg,len)   sock_sendmsg(sk,msg,len)
#define DMSC_iov_iter_init(msg,io_dir,iov_ptr,iov_len,data_len) do{msg.msg_iov = iov_ptr;msg.msg_iovlen = iov_len;}while(0)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)
#define DMSC_procfp_dname(filp) filp->f_path.dentry->d_name.name
#define DMSC_procfp_dname_len(filp) filp->f_path.dentry->d_name.len
#else
#define DMSC_procfp_dname(filp) filp->f_dentry->d_name.name
#define DMSC_procfp_dname_len(filp) filp->f_dentry->d_name.len
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)
typedef struct bio_vec dmsc_bio_vec_t;
#define DMSC_BVLEN(x) x.bv_len
#define DMSC_BVPAGE(x) x.bv_page
#define DMSC_BVOFFSET(x) x.bv_offset
#else
#define DMSC_BVLEN(x) x->bv_len
#define DMSC_BVPAGE(x) x->bv_page
#define DMSC_BVOFFSET(x) x->bv_offset
typedef struct bio_vec* dmsc_bio_vec_t;
#endif

#define blk_queue_get_logical_block_size(q) q->limits.logical_block_size
#define blk_queue_get_max_sectors(q) q->limits.max_sectors
#define blk_queue_get_max_hw_sectors(q) q->limits.max_hw_sectors
#define blk_queue_get_max_segments(q) q->limits.max_segments
#define blk_queue_get_max_segment_size(q) q->limits.max_segment_size
#define blk_queue_max_sectors(q, v) q->limits.max_sectors = v
#define blk_request_flags(req) (req->cmd_flags)
#define dms_end_request(req, status, nbytes)\
  __blk_end_request(req, !status, nbytes)
#define dms_end_request_all(req, status)\
  __blk_end_request_all(req, !status)
#define blk_fs_request(req) (req->cmd_type == REQ_TYPE_FS)

#define DMS_INIT_DELAYED_WORK(worker, fn, user_data)\
  (worker)->data=user_data; INIT_DELAYED_WORK((struct delayed_work*)worker, fn)

typedef struct delayed_work delayed_worker_t;

#else
typedef struct bio_vec* dmsc_bio_vec_t;
#define DMSC_BVLEN(x) x->bv_len
#define DMSC_BVPAGE(x) x->bv_page
#define DMSC_BVOFFSET(x) x->bv_offset

#define blk_queue_get_logical_block_size(q) q->hardsect_size
#define blk_queue_get_max_sectors(q) q->max_sectors
#define blk_queue_get_max_hw_sectors(q) q->max_hw_sectors
#define blk_queue_get_max_segments(q) q->max_hw_segments
#define blk_queue_get_max_segment_size(q) q->max_segment_size
#define blk_queue_logical_block_size(q, v) blk_queue_hardsect_size(q, v)
#define blk_queue_physical_block_size(q, v) blk_queue_hardsect_size(q, v)
#define blk_queue_max_hw_sectors(q, v) q->max_hw_sectors = v
#define blk_queue_max_segments(q, v) blk_queue_max_hw_segments(q, v)
#define blk_request_flags(req) (req->flags)
#define OLD_STATUS(status) ((status>=0)?1:status)
#define blk_rq_pos(rq) (rq->sector)
#define blk_rq_bytes(rq) (rq->nr_sectors*KERNEL_SECTOR_SIZE)
#define blk_rq_sectors(rq) (rq->nr_sectors)
#define blk_start_request(rq) blkdev_dequeue_request(rq)
#define blk_update_request(rq, status, nbytes)\
  end_that_request_first(rq, status, nbytes/KERNEL_SECTOR_SIZE)
#define blk_peek_request(q) elv_next_request(q)
#define dms_end_request(req, status, nbytes)\
  (!end_that_request_first(req, status, nbytes/KERNEL_SECTOR_SIZE)?\
          end_that_request_last(req, status), 0:-1)
#define dms_end_request_all(req, status)\
  (!end_that_request_first(req, status, req->nr_sectors)?\
    end_that_request_last(req, status), 0:-1)
#ifndef rq_for_each_segment
struct req_iterator {
    int i;
    struct bio *bio;
};
#define rq_for_each_segment(bvec, req, iter)\
  rq_for_each_bio(iter.bio, req)\
    bio_for_each_segment(bvec, iter.bio, iter.i)
#endif
typedef struct work_struct delayed_worker_t;

#define DMS_INIT_WORK(worker, fn, user_data)\
  (worker)->data=user_data; INIT_WORK((struct work_struct*)worker, fn, worker)
#define DMS_INIT_DELAYED_WORK(worker, fn, user_data)\
  (worker)->data=user_data; INIT_WORK((struct work_struct*)worker, fn, worker)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37)
#define SUPPORT_WRITE_BARRIER
#define dms_queue_barrier(q) blk_queue_flush(q, REQ_FLUSH)
#define dms_need_flush(req) ((blk_request_flags(req) & REQ_FLUSH) != 0)
#else
#ifdef SUPPORT_WRITE_BARRIER
    #define dms_queue_barrier(q) blk_queue_ordered(q, QUEUE_ORDERED_DRAIN, NULL)
    #define dms_need_flush(req) ((blk_request_flags(req) & REQ_HARDBARRIER) != 0)
#else
    #define dms_queue_barrier(q) do {} while(0)
    #define dms_need_flush(q) (0)
#endif
#endif

typedef struct {
    delayed_worker_t worker;
    void *data;
} worker_t;

#endif /* _DMS_KERNEL_VERSION_H_ */
