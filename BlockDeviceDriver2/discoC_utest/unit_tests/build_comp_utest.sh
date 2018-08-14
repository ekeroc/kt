#!/bin/sh

function build_comp_utest()
{
    printf "\n--------> Ready to build all component utest with ${kverN_str}\n"

    cur_dir=${PWD}
    
    cd common
    bash build_utest.sh build ${kverN_str}
    mv *.ko ../    
    cd ${cur_dir}

    cd config
    bash build_utest.sh build ${kverN_str}
    mv *.ko ../
    cd ${cur_dir}
    
    cd metadata_manager
    bash build_utest.sh build ${kverN_str}
    mv *.ko ../
    cd ${cur_dir}
}

function clean_comp_utest()
{
    printf "\n--------> Ready to clean all component utest with ${kverN_str}\n"

    cur_dir=${PWD}

    cd common
    bash build_utest.sh clean ${kverN_str}
    cd ${cur_dir}

    cd config
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
    clean_comp_utest
else
    build_comp_utest
fi

