#!/bin/sh

set -e

#step 1: insert kernel module

kver_str="`uname -r`"

/bin/cp -f "test_cache-${kver_str}.ko" "test_cache.ko"

insmod test_cache.ko

#step 2: run test
echo "discoC_utest test_cache reset" > /proc/discoC_utest/utest_ctrl
echo "discoC_utest test_cache run all" > /proc/discoC_utest/utest_ctrl

#step 3: collect test results
cat /proc/discoC_utest/utest_ctrl

#stpe 4: remove kernel module
rmmod test_cache

