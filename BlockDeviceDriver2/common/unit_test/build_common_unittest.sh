#!/bin/sh

make clean
cd ../../unittest_framework
make clean
make
build_ret=$?
if [  ${build_ret} -ne 0 ]; then
    echo "fail to build unit test framework"
    exit 1
fi

cp Module.symvers ../common/unit_test/
cp discoC_utest_export.h ../common/unit_test/

cd ../common/unit_test/
rm -rf Makefile
cp Makefile_test_discoC_mem_mgr Makefile
make

rm -rf discoC_utest_export.h

