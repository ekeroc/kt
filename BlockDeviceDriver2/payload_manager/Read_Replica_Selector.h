/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * Read_Replica_Selector.h
 *
 */

#ifndef PAYLOAD_MANAGER_READ_REPLICA_SELECTOR_H_
#define PAYLOAD_MANAGER_READ_REPLICA_SELECTOR_H_

extern int32_t PLMgr_Read_selectDN(metaD_item_t *loc, struct Read_DN_selector *dnSel,
        uint32_t max_read_bytes, bool read_rr_on);

#endif /* PAYLOAD_MANAGER_READ_REPLICA_SELECTOR_H_ */
