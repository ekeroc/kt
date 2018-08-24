# --- Ktfff variable ----------------------------------------------
KTFFF_DIR=`dirname "$(readlink -f "$0")"`
KTF_MK=$KTFFF_DIR/Makefile
KTF_MK_TMPL=$KTFFF_DIR/Makefile_ktfff

unitM_temp_dir=$KTFFF_DIR/unittest-module-temp
unitMK_temp_dir=$KTFFF_DIR/unittest-mk-temp
unit_output_dir=$KTFFF_DIR/unittest-output
# usrD_makefn="Makefile_usrDaemon"


# --- Install variable --------------------------------------------
INSTALL_DIR=~/src
BUILD_DIR=~/build/`uname -r`


# --- Makefile variable -------------------------------------------
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