/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * common_util.c
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

void decompose_triple (uint64_t val, int32_t *rbid, int32_t *len,
        int32_t *offset)
{
    uint64_t tmp;
    tmp = val;
    *rbid = (int32_t) (val >> 32);
    tmp <<= 32;
    *len = (int32_t) (tmp >> 32) >> 20;
    *offset = (int32_t) (val & 0x00000000000fffffLL);
}

uint32_t disco_inet_ntoa (int8_t *ipaddr)
{
    int32_t a, b, c, d;
    int8_t addr[4];

    sscanf(ipaddr, "%d.%d.%d.%d", &a, &b, &c, &d);
    addr[0] = a;
    addr[1] = b;
    addr[2] = c;
    addr[3] = d;
    return *((uint32_t *)addr);
}

void disco_inet_aton (uint32_t ipaddr, int8_t *buf, int32_t len)
{
    memset(buf, '\0', sizeof(int8_t)*len);

    sprintf(buf, "%d.%d.%d.%d", (ipaddr) & 0xFF,
             (ipaddr >> 8) & 0xFF,
             (ipaddr >> 16) & 0xFF,
             (ipaddr >> 24) & 0xFF);
}
