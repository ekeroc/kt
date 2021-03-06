#!/bin/sh

function build_all_utest()
{
    printf "\n--------> Ready to build common all subcomp utest with ${kverN_str}\n"

    cur_dir=${PWD}
    
    cd discoC_mem_manager
    bash build_utest.sh build ${kverN_str}
    mv *.ko ../
    cd ${cur_dir}

    cd thread_manager
    bash build_utest.sh build ${kverN_str}
    mv *.ko ../
    cd ${cur_dir}

    cd cache
    bash build_utest.sh build ${kverN_str}
    mv *.ko ../
    cd ${cur_dir}    
}

function clean_all_utest()
{
    printf "\n--------> Ready to clean common all subcomp utest with ${kverN_str}\n"

    cur_dir=${PWD}

    cd discoC_mem_manager
    bash build_utest.sh clean ${kverN_str}
    cd ${cur_dir}

    cd thread_manager
    bash build_utest.sh clean ${kverN_str}
    cd ${cur_dir}

    cd cache
    bash build_utest.sh clean ${kverN_str}
    cd ${cur_dir}
}

if [ $# -ne 2 ]
then
    action="build"
    kverN_str="`uname -r`"
else
    action=${1}
    kverN_str=${2}
fi

if [ "${action}" == "clean" ]; then
    clean_all_utest
else
    build_all_utest
fi

