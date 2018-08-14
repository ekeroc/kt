/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoDNSrv_protocol.c
 *
 * This component provide the socket protocol parsing and packing of DN simulator
 *
 */

#include "../common/discoC_sys_def.h"
#include "../common/dms_kernel_version.h"
#include "../common/common_util.h"
#include "../common/discoC_mem_manager.h"
#include "../common/common_util.h"
#include "../common/thread_manager.h"
#include "../metadata_manager/metadata_manager.h"
#include "../io_manager/discoC_IOSegment_Request.h"
#include "../io_manager/discoC_IO_Manager.h"
#include "../vdisk_manager/volume_manager.h"
#include "../discoNN_client/discoC_NNC_ovw.h"

int32_t NNOVWSrv_gen_ackpkt (ovw_res *respPkt, uint16_t acktype, uint16_t subopcode,
        uint64_t volID, uint64_t len_bytes)
{
    respPkt->begin_magic_number = htonll(MAGIC_NUM);
    respPkt->end_magic_number = htonll(0xeeeeeeeeeeeeeeeell);
    respPkt->error_code = htons(0);
    respPkt->len_in_byte = htonll(len_bytes);
    respPkt->req_cmd_type = htons(acktype);
    respPkt->sub_type = htons(subopcode);
    respPkt->vol_id = htonll(volID);

    return sizeof(ovw_res);
}
