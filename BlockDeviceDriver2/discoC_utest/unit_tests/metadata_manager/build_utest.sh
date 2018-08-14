#!/bin/sh

kernM_makefn="Makefile_test_metadata_manager"
kern_header_root_dir="/lib/modules/"
tmp_dir=build-module-temp

function clean_env()
{
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

function gen_makefile()
{
    sudo /bin/rm -rf Makefile
    sudo /bin/cp ${kernM_makefn} Makefile

    kverN_str=${1}
    gen_build_kmodule ${kverN_str}
}

function build_utest_km()
{
    tmp_kverN_str=${1}
    printf "\n--------> space_chunk_cache build all utest with ${tmp_kverN_str}\n"

    /bin/cp -f ../../../discoC_utest_export.h ./

    make all
    err=$?

    if [ ${err} -ne 0 ]; then
        printf "--------> discoC kernel module build fail: kernel version ${kverN_str}\n"
        exit 1
    fi

    /bin/mv test_metadata_manager.ko "${tmp_dir}/test_metadata_manager-${kverN_str}.ko"
}

function clean_utest_km()
{
    make clean
}

if [ $# -ne 2 ]
then
    action="build"
    kverN_str="`uname -r`"
else
    action=${1}
    kverN_str=${2}
fi

echo "action = ${action} kvernStr = ${kverN_str}"

clean_env
gen_makefile ${kverN_str}

if [ "${action}" == "clean" ]; then
    clean_utest_km
else
    build_utest_km ${kverN_str}

    clean_utest_km
  
    /bin/mv ${tmp_dir}/* ./
    rm -rf ${tmp_dir}
fi

