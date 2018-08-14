#!/bin/sh

utfw_kernM_makefn="Makefile_utest_framework"

tmp_dir=utest_output
kern_header_root_dir="/lib/modules/"
kern_header_dir=`find ${kern_header_root_dir} -mindepth 1 -maxdepth 1 -type d -print`

function clean_env()
{
    make clean
    
    sudo /bin/rm -rf *.ko
    sudo /bin/rm -rf ${tmp_dir}
    sudo /bin/rm -rf Makefile
   
    mkdir -p ${tmp_dir}
}

function write_kmodule_mkfn()
{
    printf "${1}" >> Makefile
}

function gen_build_kmodule()
{
    kver_str=$1

    write_kmodule_mkfn "all:\n"
    write_kmodule_mkfn "\t"

    write_kmodule_mkfn "\$(MAKE) -C/lib/modules/${kver_str}/build M=\$(PWD)\n"
    write_kmodule_mkfn "\n"

    write_kmodule_mkfn "clean:\n"
    write_kmodule_mkfn "\trm -rf *.o *.ko *.symvers *.mod.c .*.cmd Module.markers modules.order .tmp_versions version.h\n"
    write_kmodule_mkfn "\n"
}

function build_utest_framework()
{
    sudo /bin/rm -rf Makefile
    sudo /bin/cp ${utfw_kernM_makefn} Makefile

    fullpath="${1}"
    kverN_str=`echo ${fullpath#${kern_header_root_dir}}`
    gen_build_kmodule ${kverN_str}
   
    make clean
 
    printf "\n--------> ready build discoC utest framework kernel module with ${kverN_str}\n" 

    make all
    err=$?

    if [ ${err} -ne 0 ]; then
        printf "--------> discoC kernel module build fail: kernel version ${kverN_str}\n"
        exit 1
    fi

    /bin/mv discoC_utest.ko "${tmp_dir}/discoC_utest-${kverN_str}.ko"
}

function build_utest_comp()
{
    cur_dir=${PWD}

    tmp_kverN_str=`echo ${1#${kern_header_root_dir}}`

    cd unit_tests

    bash build_comp_utest.sh build ${tmp_kverN_str}

    mv *.ko ../${tmp_dir}/

    cd ${cur_dir}
}

function clean_utest_framework()
{
    make clean
}

function build_kern_module()
{
    kverN_arr=($kern_header_dir)

    for (( idx=0; idx<${#kverN_arr[@]}; idx++ ))
    do
        build_utest_framework ${kverN_arr[${idx}]}
        build_utest_comp ${kverN_arr[${idx}]}
        clean_utest_framework
    done

    /bin/cp -f ./utils/* ${tmp_dir}/
}

export LANG=en_US

clean_env
build_kern_module

