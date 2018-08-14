/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * md5_private.h
 *
 */

#ifndef MD5_PRIVATE_H_
#define MD5_PRIVATE_H_

typedef struct MD5Context {
    uint32_t buf[4];
    uint32_t bits[2];
    uint8_t in[64];
} MD5_CTX;

static void MD5Init(struct MD5Context *context);
static void MD5Update(struct MD5Context *context, unsigned char const *buf,
                          unsigned len);
static void MD5Final(unsigned char digest[16], struct MD5Context *context);
static void MD5Transform(uint32_t buf[4], uint32_t const in[16]);

#endif /* MD5_PRIVATE_H_ */
