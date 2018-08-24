#!/bin/bash

function unittest_info()
{
    local kerN_ver=$1
    printf "\n======================================================================================\n"
    printf "ktfff_unittest :: kernel version - $kerN_ver Test"
    printf "\n======================================================================================\n\n"
}

function unittest_output()
{   
    local kerN_ver=$1
    local kerN_output_dir=$unit_output_dir/$kerN_ver
    local output=$kerN_output_dir/output.xml
    local report=$kerN_output_dir/report.html

    mkdir -p $kerN_output_dir
    /bin/mv test_detail.xml  $output

    # xml2html
    xsltproc --stringparam kerN_ver $kerN_ver gtest2html.xslt $output > $report
    printf "\n--------------------------------------------------------------------------------------\n"
    printf "Output:\t$output\n"
    printf "Report:\t$report\n\n"
}

function exec_unittest_module()
{
    local kerN_ver_dir=$1
    local cmd=$2
    for test_module in $kerN_ver_dir/*; do
        cmd_err=$(sudo $cmd $test_module 2>&1)
        if [[ $cmd_err = *"File exists"* ]]; then
            sudo rmmod $test_module
            sudo insmod $test_module
        fi
    done
}


function unittest_start()
{   
    local kerN_ver_dir
    for kerN_ver_dir in $unitM_temp_dir/*; do
        local kerN_ver=$(basename $kerN_ver_dir)

        unittest_info $kerN_ver
        exec_unittest_module $kerN_ver_dir insmod
        ktfrun --gtest_output=xml
        exec_unittest_module $kerN_ver_dir rmmod
        unittest_output $kerN_ver
    done
}


function unittest_check()
{
    local cmd_lsit=$@
    local suite_list=()
    
    for cmd in $cmd_lsit; do
        select_test=${cmd##*=}
        if [[ $cmd = *"--test"* ]]; then
            suite_list+=($(get_testcase_file_in_unittests $select_test))
            if [ $? -ne 0 ]; then
                echo "TestCase $select_test not found." >&2
                exit 1
            fi
        elif [[ $cmd = *"--suite"* ]]; then
            suite_list+=($(get_suite_file_in_unittests $select_test))
            if [ $? -ne 0 ]; then
                echo "Suite $select_test not found." >&2
                exit 1
            fi
        else
            echo "Unknown arguments: $cmd" >&2
            usage
        fi
    done
    echo ${suite_list[*]}
}

function get_testcase_file_in_unittests()
{
    local test_case=$1
    for test_file in $KTFFF_DIR/unittests/*.c; do
        if grep -Fq "ADD_TEST($test_case)" $test_file ; then
            echo $test_file
            return 0
        fi
    done
    exit 1
}

function get_suite_file_in_unittests()
{
    local suite_file=$1
    for test_file in $KTFFF_DIR/unittests/*.c; do
        if [[ $test_file = $KTFFF_DIR/unittests/$suite_file ]]; then
            echo $test_file
            return 0
        fi
    done
    exit 1
}
