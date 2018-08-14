/*
 * Copyright (C) 2010-2020 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * discoC_DNC_Manager_private.h
 *
 */

#ifndef DISCODN_CLIENT_DISCOC_DNC_MANAGER_PRIVATE_H_
#define DISCODN_CLIENT_DISCOC_DNC_MANAGER_PRIVATE_H_

#define READBUFF_EXTRA_SIZE 128

#if (DN_PROTOCOL_VERSION == 2) && (CN_CRASH_RECOVERY == 1)
uint64_t ts_cleanLog;
uint32_t ts_chk_bit;
#endif

/*
 * Currently, we won't need ReadMask buffer anymore since clientnode always read a full 4K
 * even user only need particular sector inside a 4K.
 * But we didn't remove from protocol and DN still provide the function.
 * Clientnode prepare a dedicated buffer to indicate that each read requests will read full 4K.
 */
uint8_t *DNProto_ReadMask_buffer;

/*
 * instance of DN client manager
 */
DNClient_MGR_t dnClient_mgr;

#endif /* DISCODN_CLIENT_DISCOC_DNC_MANAGER_PRIVATE_H_ */
