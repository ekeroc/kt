#!/bin/bash

#####################################################
#               Ktfff Unittest Tool                 #
#                                                   #
#   install - install ktf framework                 #
#   run - build && run all unittest                 #
#   run <test file> - build && run single unittest  #
#                                                   #
#####################################################

KTFFF_DIR=`dirname "$(readlink -f "$0")"`
KTF_MK=$KTFFF_DIR/Makefile
KTF_MK_TMPL=$KTFFF_DIR/Makefile_ktfff

# usrD_makefn="Makefile_usrDaemon"

unitM_temp_dir=$KTFFF_DIR/unittest-module-temp
unitMK_temp_dir=$KTFFF_DIR/unittest-mk-temp
kern_header_root_dir="/lib/modules/"
kern_header_dir=`find ${kern_header_root_dir} -mindepth 1 -maxdepth 1 -type d -print`

subcomp="lib config common connection_manager"
subcomp="${subcomp} discoDN_simulator discoDN_client payload_manager"
subcomp="${subcomp} discoNNOVW_simulator discoNN_simulator discoNN_client metadata_manager"
subcomp="${subcomp} io_manager"

mkfn_list="${subcomp} main"

# major_num=0
# minor_num=0
# release_num=0
# patch_num=0

function clean_env()
{
    sudo /bin/rm -rf BlockDeviceDaemon
    sudo /bin/rm -rf *.ko
    sudo /bin/rm -rf Makefile
    sudo /bin/rm -rf unitM_temp_dir
    sudo /bin/rm -rf unitMK_temp_dir
   
    mkdir -p unitM_temp_dir
    mkdir -p unitMK_temp_dir
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
    local kerN_ver=$1

    write_kmodule_mkfn "all:\n"
    write_kmodule_mkfn "\t"

    write_kmodule_mkfn "\$(MAKE) -C/lib/modules/${kerN_ver}/build \$(EXTRASYMS) M=\$(PWD)\n"
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
        sudo /bin/cp $KTF_MK_TMPL $KTF_MK

        fullpath="${kverN_arr[${idx}]}"
        kerN_ver=`echo ${fullpath#${kern_header_root_dir}}`

        mkdir -p ${unitM_temp_dir}/${kerN_ver}
        mkdir -p ${unitMK_temp_dir}/${kerN_ver}
        make clean
        build_unittest_kerN_module
    done
}

function build_unittest_kerN_module()
{
    for test_file in $KTFFF_DIR/unittests/*.c; do
        kern_module_name=`basename "$test_file" .c`
        gen_build_kmodule_comp
        gen_kern_module_mk $kern_module_name
        gen_build_kmodule ${kerN_ver}
        make all
        /bin/mv $KTF_MK ${unitMK_temp_dir}/${kerN_ver}/$kern_module_name.mk
        # /bin/mv $kern_module_name.ko ${unitM_temp_dir}/${kerN_ver}
    done
}

function exec_unittest_module()
{
    local kerN_ver_dir=$1
    local cmd=$2
    for test_module in $kerN_ver_dir/*; do
        sudo $cmd $test_module            
    done
}

# function unittest_info()
# {
# }

function unittest_start()
{   
    local kerN_ver_dir
    for kerN_ver_dir in $unitM_temp_dir/*; do
        local kerN_ver=$(basename $kerN_ver_dir)
        printf "**************** $kerN_ver unittest starting ****************\n"
        exec_unittest_module $kerN_ver_dir insmod
        ktfrun
        exec_unittest_module $kerN_ver_dir rmmod
    done
}


# function unittest_teardown()
# {
# }

function main()
{
clean_env
# build_user_daemon

build_kern_module
unittest_start
# ktfrun
# /bin/mv ${unitM_temp_dir}/* ./
# rm -rf ${unitM_temp_dir}
}

export LANG=en_US
main


