#!/bin/sh

#step 1: insert kernel module

kver_str="`uname -r`"

/bin/cp -f "test_thread_manager-${kver_str}.ko" "test_thread_manager.ko"

insmod test_thread_manager.ko

#step 2: run test
echo "discoC_utest test_thread_manager reset" > /proc/discoC_utest/utest_ctrl
echo "discoC_utest test_thread_manager run all" > /proc/discoC_utest/utest_ctrl

#step 3: collect test results
cat /proc/discoC_utest/utest_ctrl

#stpe 4: remove kernel module
rmmod test_thread_manager

