#include <linux/kernel.h>

#include "test_lib.h"
#include "extern_fun.h"

void unittest(void)
{
    unit_log("Start unittest\n");
unit_log("Run test file: tests/unittests/test_ccrm.c\n");
unit_log(" -> Run test case: test_cleanlog_base_op()\n");
test_cleanlog_base_op();
unit_log(" -> Run test case: test_update_cleanlog_ts()\n");
test_update_cleanlog_ts();
unit_log(" -> Run test case: test_send_cleanlog()\n");
test_send_cleanlog();
unit_log(" -> Run test case: test_update_nonexist_cleanlog()\n");
test_update_nonexist_cleanlog();
unit_log(" -> Run test case: test_cleanlog_response()\n");
test_cleanlog_response();
unit_log(" -> Run test case: test_flush_cleanlog()\n");
test_flush_cleanlog();
    /* Hook unittest function here */
    unit_log("Finish unittest\n");
}
