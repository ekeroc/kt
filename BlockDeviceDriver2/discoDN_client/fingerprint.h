/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * fingerprint.h
 *
 */

#ifndef FINGERPRINT_H_
#define FINGERPRINT_H_

uint32_t get_fingerprint(fp_method_t fp_method, int8_t *buf_ptr, DNUData_Req_t *dnreq);

#endif /* FINGERPRINT_H_ */
