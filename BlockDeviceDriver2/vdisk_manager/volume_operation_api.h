/*
 * Copyright (C) 2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * volume_operation_api.h
 *
 * NOTE: Add this file to put the volume operation API declaration.
 *       These API will share the same response code in common.h
 *       These API declaration won't place at volume_manager.h
 *       We don't want each sub-component include "common.h"
 */

#ifndef VOLUME_OPERATION_API_H_
#define VOLUME_OPERATION_API_H_

extern clear_ovw_resp_t clear_volume_ovw (uint32_t vol_id);
extern set_fea_resp_t set_volume_state_share (uint32_t vol_id, bool is_share);
extern block_io_resp_t set_volume_state_block_io (uint32_t vol_id);
extern unblock_io_resp_t set_volume_state_unblock_io (uint32_t vol_id);
extern detach_response_t detach_volume(uint32_t nnIPaddr, uint32_t vol_id);

extern attach_response_t attach_volume(int8_t *nnSrvNM, uint32_t nnIPaddr, uint32_t vol_id, uint64_t capacity,
        bool is_share, bool mcache_enable, vol_rw_perm_type_t rw_perm,
        uint32_t replica, volume_gd_param_t *vol_gd);
extern attach_response_t attach_volume_default_gd(int8_t *nnSrvNM, uint32_t nnIPaddr, uint32_t vol_id,
        uint64_t capacity, bool is_share, bool mcache_enable,
        vol_rw_perm_type_t rw_perm, uint32_t replica);

extern stop_response_t stop_volume(uint32_t vol_id);
extern restart_response_t restart_volume(uint32_t vol_id);

#endif /* VOLUME_OPERATION_API_H_ */
