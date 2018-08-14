/*
 *
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * common.h
 *
 */

#ifndef COMMON_H_
#define COMMON_H_

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
extern long __must_check IS_ERR_OR_NULL(__force const void *ptr);
#endif

#endif /* COMMON_H_ */
