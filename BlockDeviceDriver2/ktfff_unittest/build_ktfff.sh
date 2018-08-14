#!/bin/bash

KTFFF_DIR=`dirname "$(readlink -f "$0")"`
KTF_MK=$KTFFF_DIR/Makefile_test.mk

# usrD_makefn="Makefile_usrDaemon"
kernM_makefn="Makefile_kernModule"

tmp_dir=build-module-temp
kern_header_root_dir="/lib/modules/"
kern_header_dir=`find ${kern_header_root_dir} -mindepth 1 -maxdepth 1 -type d -print`

subcomp="lib config common connection_manager"
subcomp="${subcomp} discoDN_simulator discoDN_client payload_manager"
subcomp="${subcomp} discoNNOVW_simulator discoNN_simulator discoNN_client metadata_manager"
subcomp="${subcomp} io_manager"

mkfn_list="${subcomp} main"

major_num=0
minor_num=0
release_num=0
patch_num=0

function clean_env()
{
    sudo /bin/rm -rf BlockDeviceDaemon
    sudo /bin/rm -rf *.ko
    # sudo /bin/rm -rf Makefile
    sudo /bin/rm -rf build-module-temp
   
    # mkdir -p ${tmp_dir}
}

function write_kmodule_mkfn()
{
    printf "${1}" >> $KTFFF_DIR/Makefile_test.mk
}

function gen_build_kmodule_comp()
{
    for dir in ${mkfn_list}
    do
        filename="$KTFFF_DIR/../mk/discoC.${dir}.mk"
        while read -r line
        do
            write_kmodule_mkfn "${line}"
            write_kmodule_mkfn "\n"
        done < ${filename}
    done
}

function gen_build_kmodule()
{
    kver_str=$1

    gen_build_kmodule_comp

    write_kmodule_mkfn "all:\n"
    write_kmodule_mkfn "\t"

    write_kmodule_mkfn "\$(MAKE) -C/lib/modules/${kver_str}/build M=\$(PWD)\n"
    write_kmodule_mkfn "\n"

    write_kmodule_mkfn "clean:\n"
    write_kmodule_mkfn "\t"
    write_kmodule_mkfn "rm -rf *.o *.ko *.symvers *.mod.c .*.cmd Module.markers modules.order .tmp_versions version.h\n"
    for dir in ${subcomp}
    do
        write_kmodule_mkfn "\t"
        write_kmodule_mkfn "rm -rf ${dir}/*.o\n"
    done
    write_kmodule_mkfn "\n"
}

function build_kern_module()
{
    kverN_arr=($kern_header_dir)

    for (( idx=0; idx<${#kverN_arr[@]}; idx++ ))
    do
        # sudo /bin/rm -rf Makefile
        sudo /bin/cp $KTFFF_DIR/../${kernM_makefn} $KTFFF_DIR/Makefile_test.mk

        fullpath="${kverN_arr[${idx}]}"
        kverN_str=`echo ${fullpath#${kern_header_root_dir}}`

        gen_build_kmodule ${kverN_str}
    done
}

# Replace kernel module name && Filter out mock file
function gen_kern_module_mk()
{
    for test_file in $KTFFF_DIR/unittests/*.c; do
        # Get all mock files in test
        local all_mock_files=`grep "MOCK_FILE" ${test_file} | awk '{print $3}'`
        all_mock_files=${all_mock_files//.c/.o}

        # Remove mock file from markfile
        for mock_file in $all_mock_files; do
            sed -i "s/ $mock_file//g" $KTF_MK
        done

        # Maybe need get the mk/makeifle's objs name
        kern_module_name=`basename "$test_file" .c`
        sed -i "s/ccmablk/$kern_module_name/g" $KTF_MK
    done
}



export LANG=en_US

clean_env
# build_user_daemon

build_kern_module
replace_kernel_module_name
# /bin/mv ${tmp_dir}/* ./
# rm -rf ${tmp_dir}

# cd discoC_utest
# bash build_discoC_utest.sh
# cd ..

