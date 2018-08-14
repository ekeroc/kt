/*
 *
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_utest_ctrlintf_private.h
 *
 * This module provide a linux-proc based control interface for discoC unit test
 */


#ifndef DISCOC_UTEST_CTRLINTF_PRIVATE_H_
#define DISCOC_UTEST_CTRLINTF_PRIVATE_H_

#define nm_procDir_utest  "discoC_utest"
#define nm_procFN_test  "utest_ctrl"

#define MAX_CMD_LEN 128

struct proc_dir_entry *utest_dir;

#endif /* DISCOC_UTEST_CTRLINTF_PRIVATE_H_ */
