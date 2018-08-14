#!/bin/sh

/bin/cp -f ../../discoC_utest_export.h ./
cp Makefile_Test_test_target Makefile
make clean
make
