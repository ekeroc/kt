/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * conn_hb_worker
 *
 */
#ifndef CONN_HB_WORKER_PRIVATE_H_
#define CONN_HB_WORKER_PRIVATE_H_

#if (DN_PROTOCOL_VERSION == 2)
static const int SIZEOF_U64 = sizeof(uint64_t);
static const int SIZEOF_U32 = sizeof(uint32_t);
static const int SIZEOF_U16 = sizeof(uint16_t);
static const int SIZEOF_U8 = sizeof(uint8_t);

#define HB_PACKET_SIZE_DN   28
#if (CN_CRASH_RECOVERY == 1)
#define CL_PACKET_SIZE_DN   36
static int8_t clog_buffer[CL_PACKET_SIZE_DN];
static int8_t *ptr_clog_ts;
#endif
static int8_t hb_buffer_dn[HB_PACKET_SIZE_DN];
#endif

#endif /* CONN_HB_WORKER_PRIVATE_H_ */
