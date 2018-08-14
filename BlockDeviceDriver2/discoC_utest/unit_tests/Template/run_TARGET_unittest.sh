#!/bin/sh

# Exit on error
set -e

#step 1: insert kernel module

insmod test_TARGET.ko

#step 2: run test
echo "discoC_utest test_TARGET reset" > /proc/discoC_utest/utest_ctrl
echo "discoC_utest test_TARGET run all" > /proc/discoC_utest/utest_ctrl

#step 3: collect test results
cat /proc/discoC_utest/utest_ctrl

#stpe 4: remove kernel module
rmmod test_TARGET
