#!/bin/sh

#step 1: insert kernel module

insmod ../../unittest_framework/discoC_utest.ko
insmod test_discoC_mem_manager.ko

#step 2: run test
echo "discoC_utest test_discoC_mem_mgr reset" > /proc/discoC_utest/utest_ctrl
echo "discoC_utest test_discoC_mem_mgr run all" > /proc/discoC_utest/utest_ctrl

#step 3: collect test results
cat /proc/discoC_utest/utest_ctrl

#stpe 4: remove kernel module
rmmod test_discoC_mem_manager
rmmod discoC_utest

