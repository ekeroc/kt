#!/bin/bash

usrD_makefn="Makefile_usrDaemon"
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
    sudo /bin/rm -rf Makefile
    sudo /bin/rm -rf build-module-temp
   
    mkdir -p ${tmp_dir}
}

function build_user_daemon()
{
    sudo /bin/cp ${usrD_makefn} Makefile
    make clean
    make
    err=$?
  
    if [ ${err} -ne 0 ]; then
        echo "--------> discoC User Daemon build fail\n"
        exit 1
    fi

    mv BlockDeviceDaemon build-module-temp

    make clean
}

function write_kmodule_mkfn()
{
    printf "${1}" >> Makefile
}

function gen_build_kmodule_comp()
{
    for dir in ${mkfn_list}
    do
        filename="mk/discoC.${dir}.mk"
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

function check_kern_ver()
{
    kernel_ver=$1

    kver_tk_arr=(${kernel_ver//./ })
    major_num=${kver_tk_arr[0]}
    minor_num=${kver_tk_arr[1]}
    release_num_str=${kver_tk_arr[2]}
    rel_tk_arr=(${release_num_str//-/ })
    
    release_num=${rel_tk_arr[0]}

    if [ ${#rel_tk_arr[@]} -eq 0 ]; then
        patch_num=0
    else
        patch_num=${rel_tk_arr[1]}
    fi

    if [ $major_num -ne 4 ]; then
        return
    fi

    if [ $minor_num -ne 4 ]; then
        return
    fi

    if [ $release_num -ne 0 ]; then
        return
    fi

    if [ $patch_num -le 83 ]; then
        return
    fi
    
    printf "\n--------> Patch discoC kernel module for kernel version ${major_num}.${minor_num}.${release_num}-${patch_num}\n"

    cd common/
    patch -i ubuntu_4_4_0_87.patch
    cd ..
}

function restore_patch()
{
    if [ $major_num -ne 4 ]; then
        return
    fi

    if [ $minor_num -ne 4 ]; then
        return
    fi

    if [ $release_num -ne 0 ]; then
        return
    fi

    if [ $patch_num -le 83 ]; then
        return
    fi
    
    printf "\n--------> Restore patch disoC kernel module to formal kernel version\n"
    cd common/
    patch -i linux_formal.patch
    cd ..
}

function build_kern_module()
{
    kverN_arr=($kern_header_dir)

    for (( idx=0; idx<${#kverN_arr[@]}; idx++ ))
    do
        sudo /bin/rm -rf Makefile
        sudo /bin/cp ${kernM_makefn} Makefile

        fullpath="${kverN_arr[${idx}]}"
        kverN_str=`echo ${fullpath#${kern_header_root_dir}}`

        check_kern_ver ${kverN_str}

        gen_build_kmodule ${kverN_str}

        make clean

        printf "\n--------> ready build discoC kernel module with ${kverN_str}\n"

        make all
        err=$?

        restore_patch

        if [ ${err} -ne 0 ]; then
            printf "--------> discoC kernel module build fail: kernel version ${kverN_str}\n"
            exit 1
        else
            /bin/mv ccmablk.ko "${tmp_dir}/ccmablk-${kverN_str}.ko"
            make clean
        fi
    done
}

export LANG=en_US

clean_env
build_user_daemon

build_kern_module

/bin/mv ${tmp_dir}/* ./
rm -rf ${tmp_dir}

cd discoC_utest
bash build_discoC_utest.sh
cd ..

