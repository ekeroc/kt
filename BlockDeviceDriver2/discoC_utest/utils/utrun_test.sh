#!/bin/sh

#step 1: insert kernel module
kver_str="`uname -r`"

/bin/cp -f "discoC_utest-${kver_str}.ko" "discoC_utest.ko"

insmod discoC_utest.ko

bash utrun_test_discoC_mem_manager.sh
bash utrun_test_thread_manager.sh
bash utrun_test_cache.sh
bash utrun_test_metadata_manager.sh

rmmod discoC_utest

