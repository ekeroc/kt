#!/bin/bash
source lib/config.sh
source lib/unittest.sh
source lib/makefile.sh

#####################################################
#               Ktfff Unittest Tool                 #
#                                                   #
#   setup - install ktf framework                   #
#   run - build && run all unittest                 #
#   run <test file> - build && run single unittest  #
#                                                   #
#####################################################

function clean_env()
{
    sudo /bin/rm -rf BlockDeviceDaemon
    sudo /bin/rm -rf *.ko
    sudo /bin/rm -rf Makefile
    sudo /bin/rm -rf $unitM_temp_dir
    sudo /bin/rm -rf $unitMK_temp_dir
    sudo /bin/rm -rf $unit_output_dir
   
    mkdir -p $unitM_temp_dir
    mkdir -p $unitMK_temp_dir
    mkdir -p $unit_output_dir
}

function ktfff_info()
{
    local info_str=$1
    printf "\n[INFO] $info_str"
}

function main()
{
clean_env
# build_user_daemon
build_kern_module
ktfff_info "\nUNNITEST START\n"
unittest_start
ktfff_info "UNNITEST COMPLETE\n"
}

export LANG=en_US
main