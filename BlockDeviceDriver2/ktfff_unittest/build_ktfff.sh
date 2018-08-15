#!/bin/bash

KTFFF_DIR=`dirname "$(readlink -f "$0")"`
KTF_MK=$KTFFF_DIR/Makefile

# usrD_makefn="Makefile_usrDaemon"
kernM_makefn="Makefile_ktfff"

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
    sudo /bin/rm -rf Makefile
    sudo /bin/rm -rf build-module-temp
   
    mkdir -p ${tmp_dir}
}

function write_kmodule_mkfn()
{
    printf "${1}" >> $KTFFF_DIR/Makefile
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
    write_kmodule_mkfn "all:\n"
    write_kmodule_mkfn "\t"

    write_kmodule_mkfn "\$(MAKE) -C/lib/modules/${kver_str}/build \$(EXTRASYMS) M=\$(PWD)\n"
    write_kmodule_mkfn "\n"

    write_kmodule_mkfn "clean:\n"
    write_kmodule_mkfn "\t"
    write_kmodule_mkfn "rm -rf *.o *.ko *.symvers *.mod.c .*.cmd Module.markers modules.order .tmp_versions version.h\n"
    for dir in ${subcomp}
    do
        write_kmodule_mkfn "\t"
        write_kmodule_mkfn "rm -rf ${KTFFF_DIR}/../${dir}/*.o\n"
    done
    write_kmodule_mkfn "\n"
}



# Replace kernel module name && Filter out mock file
function gen_kern_module_mk()
{
    kern_module_name=$1

    # Get all mock files in test
    local all_mock_files=`grep "MOCK_FILE" ${test_file} | awk '{print $3}'`
    all_mock_files=${all_mock_files//.c/.o}

    # Remove mock file from markfile
    for mock_file in $all_mock_files; do
        sed -i "s/ $mock_file//g" $KTF_MK
    done

    # Replace kernel module name to test file name
    sed -i "s/ccmablk/$kern_module_name/g" $KTF_MK

    # Remove line if string match dms_module_init.o
    sed -i "/dms_module_init.o/d" $KTF_MK
    write_kmodule_mkfn "$kern_module_name-objs := \$(addprefix \$(_DISCO_CN_ROOT), \$($kern_module_name-objs))\n"
    write_kmodule_mkfn "$kern_module_name-objs += unittests/$kern_module_name.o\n\n"
}

function build_kern_module()
{
    kverN_arr=($kern_header_dir)

    for (( idx=0; idx<${#kverN_arr[@]}; idx++ ))
    do
        sudo /bin/rm -rf Makefile
        sudo /bin/cp $KTFFF_DIR/${kernM_makefn} $KTFFF_DIR/Makefile
        fullpath="${kverN_arr[${idx}]}"
        kverN_str=`echo ${fullpath#${kern_header_root_dir}}`

        build_test_kern_module
        gen_build_kmodule ${kverN_str}

        # make clean
        # make all
        # err=$?

        # if [ ${err} -ne 0 ]; then
        #     printf "--------> discoC kernel module build fail: kernel version ${kverN_str}\n"
        #     exit 1
        # else
        #     /bin/mv ccmablk.ko "${tmp_dir}/ccmablk-${kverN_str}.ko"
        #     make clean
        # fi
    done
}

function build_test_kern_module()
{
    for test_file in $KTFFF_DIR/unittests/*.c; do
        kern_module_name=`basename "$test_file" .c`
        gen_build_kmodule_comp
        gen_kern_module_mk $kern_module_name
    done
}


export LANG=en_US

clean_env
# build_user_daemon

build_kern_module
# /bin/mv ${tmp_dir}/* ./
# rm -rf ${tmp_dir}

# cd discoC_utest
# bash build_discoC_utest.sh
# cd ..

