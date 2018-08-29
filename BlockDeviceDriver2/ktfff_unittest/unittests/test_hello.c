/*
sudo make clean && \                       
sudo make && \
sudo rmmod test && \
sudo insmod test.ko && \
ktfrun --gtest_output=xml && \
xsltproc ~/gtest2html/gtest2html.xslt test_detail.xml > test_detail.html
*/

#include <linux/module.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <net/sock.h>
#include "ktf.h"
#include "../lib/fff.h"
#include "../../common/discoC_sys_def.h"
#include "../../common/common_util.h"
#include "../../common/dms_kernel_version.h"
#include "../../common/common.h"
#include "../../common/discoC_mem_manager.h"
#include "../../common/thread_manager.h"
#include "../../metadata_manager/metadata_manager.h"
#include "../../metadata_manager/metadata_cache.h"
#include "../../common/dms_client_mm.h"
#include "../../discoDN_client/discoC_DNC_Manager_export.h"
#include "../../payload_manager/payload_manager_export.h"
#include "../../io_manager/discoC_IO_Manager.h"
#include "../../connection_manager/conn_manager_export.h"
#include "../../drv_fsm.h"
#include "../../drv_main.h"
#include "../../drv_main_api.h"
#include "../../config/dmsc_config.h"
#include "../../vdisk_manager/volume_manager.h"
#include "../../vdisk_manager/volume_operation_api.h"
#include "../../flowcontrol/FlowControl.h"
#ifdef DISCO_PREALLOC_SUPPORT
#include "../../metadata_manager/space_manager_export_api.h"
#endif
#include "../../drv_chrdev.h"
#include "../../dms_module_init.h"
#include "../../drv_main_private.h"


MODULE_LICENSE("GPL");
KTF_INIT();

DEFINE_FFF_GLOBALS;

// MOCK_FILE: drv_main.c
FAKE_VALUE_FUNC(int32_t, init_drv_component);
FAKE_VALUE_FUNC(int32_t, rel_drv_component);


void setup(void)
{
	RESET_FAKE(init_drv_component);
	RESET_FAKE(rel_drv_component);
}

TEST(examples, hello_ok)
{	
	setup();
	init_drv_component_fake.return_val = true;
	ASSERT_TRUE(init_drv_component());
	ASSERT_INT_EQ(init_drv_component_fake.call_count, 1);
}


TEST(examples, hello_fail)
{	
	setup();
	rel_drv_component_fake.return_val = false;
	ASSERT_TRUE(rel_drv_component());
}

static void add_tests(void)
{
	ADD_TEST(hello_ok);
	ADD_TEST(hello_fail);
}

static int __init hello_init(void)
{
	// int32_t running_mode = 0;
	// init_drv(running_mode);
	add_tests();
	tlog(T_INFO, "hello: loaded");
	return 0;
}

static void __exit hello_exit(void)
{
	KTF_CLEANUP();
	tlog(T_INFO, "hello: unloaded");
	// release_drv();
}


module_init(hello_init);
module_exit(hello_exit);
