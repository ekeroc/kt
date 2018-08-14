/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_IO_Manager_private.h
 */

#ifndef IO_MANAGER_DISCOC_IO_MANAGER_PRIVATE_H_
#define IO_MANAGER_DISCOC_IO_MANAGER_PRIVATE_H_

#define IOREQ_WBUFFER_EXTRA 128
#define IOREQ_BLOCK_LIMITATION              600000L  //unit mseconds

#define NUM_DMS_IO_THREADS  8

#define MAX_MAIT_TIME_OF_ATTACH_ON_GOING    5000  //unit mseconds
#define BASIC_WAIT_UNIT                     500  //unit mseconds


#define MAX_TIME_OUT_ATTACH     (MAX_MAIT_TIME_OF_ATTACH_ON_GOING / \
                                 BASIC_WAIT_UNIT)

typedef enum
{
    DMS_IO_BLK_TOGO,        /* no flow control is underway */
    DMS_IO_BLK_MEM_DEF,     /* f.c. due to mem deficiency */
    DMS_IO_BLK_ATTACH,      /* f.c. due to another attaching volume */
    DMS_IO_BLK_NN_BLOCK,        /* f.c. due to NN doing snapshot */
} dms_io_block_code_t;

io_manager_t discoCIO_Mgr;

#endif /* IO_MANAGER_DISCOC_IO_MANAGER_PRIVATE_H_ */
