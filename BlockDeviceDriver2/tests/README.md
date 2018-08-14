

How to run unittest
===================

```bash
$ ./run_test.sh
```

The above script will patch necessary Makefile, c, and header files to build
the kernel module and start the unit test by inserting the built kernel module.


How to add your test cases
==========================

To add your unit test set, you should add your test cases in a file with the
following rules:

* Add your testing file in `unittest` folder.
* The file name should start with `test_`, like `test_yourtest.c`.
* The test case function name should start with `void test_`, like
  `void test_fun1(void)`. Don't put `{` or comment at the same line and don't
  break the function definition in multiple line (maybe you would like to break
  it due to 80 characters style, but we don't support it now).

Example:
```c
void test_cleanlog_base_op(void)
{
    init_ccrm();
    _record_cleanlog(jiffies_64);
    dump_pending_cleanlog();
    destroy_pending_cleanlog();
}
```


How to mock a function
======================

In your test file, you should add a line as follow to specify a function with
your mocking function. The format is `// MOCK: (file) original function => 
mock function`.

```c
// MOCK: (ccrm.c) sock_xmit(*goal, *buf, len, lock_sock) => mock_sock_xmit(*goal, *buf, len, lock_sock)
int32_t mock_sock_xmit(
    discoC_conn_t *goal, void *buf, size_t len, bool lock_sock)
{
    return CL_PACKET_SIZE_DN;
}
```


Library
=======

To include the library for your testing.

```c
#include "../test_lib.h"
```

* `unit_log(fmt, args...)`: print message


Assertions
==========

TODO

