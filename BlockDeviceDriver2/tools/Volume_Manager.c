/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * Volume_Manager.c
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <syslog.h>
#include "list.h"
#include "common.h"
#include "Volume_Manager.h"

volume_device_t *cur_vol;

static void add_volume (volume_device_t *vol)
{
    cur_vol = vol;
}

static void rm_volume (volume_device_t *vol)
{
    cur_vol = NULL;
}

volume_device_t *query_volume (uint32_t volid)
{
    return cur_vol;
}

uint16_t get_volume_replica (uint32_t volid)
{
    return cur_vol->vol_feature.prot_method.replica_factor;
}

volume_device_t *create_vol_inst (uint32_t volid, uint64_t vol_cap)
{
    volume_device_t *vol_inst;

    if (vol_cap == 0) {
        info_log("Fail to create vol inst with 0 capacity\n");
        return NULL;
    }

    vol_inst = (volume_device_t *)malloc(sizeof(volume_device_t));

    vol_inst->vol_id = volid;
    vol_inst->capacity = vol_cap;
    INIT_LIST_HEAD(&vol_inst->vol_io_q.io_pending_q);
    vol_inst->num_reqs = 0;
    vol_inst->user_data = NULL;

    add_volume(vol_inst);
    return vol_inst;
}

void free_vol_inst (volume_device_t *vol_inst)
{
    if (vol_inst == NULL) {
        return;
    }

    rm_volume(vol_inst);
    free(vol_inst);
}
