/*
 *
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_utest_cmd_handler.h
 *
 */

#ifndef DISCOC_UTEST_CMD_HANDLER_H_
#define DISCOC_UTEST_CMD_HANDLER_H_

extern int32_t exec_test_cmd(int8_t *test_cmd, int32_t cmd_len);
extern int32_t get_test_cmd_result(int8_t *buffer, int32_t buff_size);
extern int32_t init_cmd_handler(void);
#endif /* DISCOC_UTEST_CMD_HANDLER_H_ */
