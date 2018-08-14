#!/bin/bash

test_objs="tests/tests.o"

function append_backup_file()
{
  tar --append --file=backup.tar "$1"
}

function apply_mock_fun()
{
  echo "" > tests/mock_funs.h

  for test_file in tests/unittests/test_*.c; do
    local all_mock_lines=$(cat ${test_file} | grep '//' | grep 'MOCK: ')

    while read -r mock_line; do
      local mock_file=$(get_mock_file "${mock_line}")
      local orig_fun=$(get_orig_fun "${mock_line}")
      local mock_fun=$(get_mock_fun "${mock_line}")
      local mock_fun_def=$(get_mock_fun_def "${test_file}" "${mock_fun}")

      append_backup_file "${mock_file}"
      echo "#define ${orig_fun} ${mock_fun}" >> tests/mock_define.h
      echo "extern ${mock_fun_def};" >> tests/mock_funs.h
      hook_mock_fun_header "${mock_file}"
    done <<< "${all_mock_lines}"
  done
}

function apply_test_cases()
{
  for test_file in tests/unittests/test_*.c; do
    test_objs="${test_objs} $(echo ${test_file} | sed 's/\.c/\.o/g')"
    hook_test_case "${test_file}"
  done
}

function backup_files()
{
  tar cvf backup.tar Makefile dms_module_init.h tests/tests.c \
    tests/extern_fun.h 
}

function build_testing_kernel()
{
  modify_files_for_unittest
  make nonxen
}

function get_mock_file()
{
  echo "$1" | awk -F "(" {'print $2'} | awk -F ")" {'print $1'}
}

function get_mock_fun()
{
  echo "$1" | awk -F "=>" {'print $2'} | sed 's/ //g'
}

function get_mock_fun_def()
{
  local fun_name=$(echo "$2" | awk -F "(" {'print $1'})
  cat "$1" | grep -v "MOCK" | grep "${fun_name}"
}

function get_orig_fun()
{
  echo "$1" | awk -F "=>" {'print $1'} | \
    awk '{ s = ""; for (i = 4; i <= NF; i++) s = s $i " "; print s }' | \
    sed 's/ //g'
}

function hook_mock_fun_header()
{
  # Hook the mock_define.h to the top
  sed -i '1i#include "tests/mock_define.h"' $1

  # Hook the mock_funs.h to be the last include line
  tac $1 | \
  awk '!p && /#include "/{print "#include \"tests/mock_funs.h\""; p=1} 1' | \
  tac > $1
}

function hook_test_case()
{
  insert_line "unit_log(\"Run test file: $1\\\\n\");"
  all_unittest=$(cat $1 | grep -v '//' | grep 'void test_')
  if [ "${all_unittest}" == "" ]; then
    return 0
  fi

  while read -r test_case_line; do
    echo -e "extern ${test_case_line};" >> tests/extern_fun.h

    local test_case=$(echo "${test_case_line}" | sed 's/void//g')
    if [ "$test_case" != "" ]; then
      insert_line "unit_log(\" -> Run test case: ${test_case}\\\\n\");"
      insert_line "${test_case};"
    fi
  done <<< "$all_unittest"
}

function insert_kernel()
{
  if [ ! -f "ccmablk.ko" ]; then
    echo -e "Can't find the kernel module: ccmablk.ko"
    echo -e
    exit 1
  fi

  insmod ccmablk.ko
}

function insert_line()
{
  local sed_cmd="sed -i '/Hook unittest function here/i $1' tests/tests.c"
  eval ${sed_cmd}
}

function modify_files_for_unittest()
{
  sed -i '/RUN_UNIT_TEST/c\#define RUN_UNIT_TEST' dms_module_init.h
  sed_cmd="sed -i '/obj-m := ccmablk.o/i ccmablk-objs += ${test_objs}' Makefile"
  eval ${sed_cmd}
}

function print_usage()
{
  echo -e
  echo -e "./run_test.sh [--help] [UNIT_TEST]"
  echo -e "  - Run all unit test cases if no arguments given"
  echo -e "  - UNIT_TEST: run the given unit test only"
  echo -e
}

function restore_files()
{
  tar xvf backup.tar
  rm -rf backup.tar
}

function show_test_result()
{
  echo -e
  echo -e
  tail -fn 0 /var/log/messages | grep UNITTESST
}

function start_test()
{
  # The testing will be started when the kernel module is inserted
  insert_kernel
  # Wait for test result to be logged in system log
  sleep 5
  rmmod ccmablk.ko
}

function main()
{
  if [ "$1" == "--help" ]; then
    print_usage
    exit 0
  fi

  cd ..
  backup_files
  apply_mock_fun $1
  apply_test_cases $1
  build_testing_kernel
  # Show test result in background
  show_test_result &
  start_test
  restore_files
  cd tests
}

if [[ $(basename $0) = $(basename ${BASH_SOURCE}) ]]; then
  main $@
fi

