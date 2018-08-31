#!/bin/bash

major_num=0
minor_num=0
release_num=0
patch_num=0

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
    
    ktfff_event "Patch discoC kernel module for kernel version ${major_num}.${minor_num}.${release_num}-${patch_num}\n"
    cd $KTFFF_DIR/../common/
    patch -i ubuntu_4_4_0_87.patch
    cd $KTFFF_DIR
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
    
    ktfff_event "Restore patch disoC kernel module to formal kernel version\n"
    cd $KTFFF_DIR/../common/
    patch -i linux_formal.patch
    cd $KTFFF_DIR
}