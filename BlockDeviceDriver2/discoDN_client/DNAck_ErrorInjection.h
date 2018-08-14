/*
 * Copyright (C) 2010-2013 by Cloud Computing Center for Mobile Applications
 * Industrial Technology Research Institute
 *
 * DNAck_ErrorInjection.h
 *
 * Change DNAck from normal response to different error types for testing
 *
 */

#ifndef DISCODN_CLIENT_DNACK_ERRORINJECTION_H_
#define DISCODN_CLIENT_DNACK_ERRORINJECTION_H_

extern int32_t DNAckEIJ_add_item(uint32_t dn_ipaddr, uint32_t dn_port, uint32_t rwdir,
        uint32_t err_type, uint32_t err_per, uint32_t err_chktype);
extern int32_t DNAckEIJ_update_item(uint32_t dn_ipaddr, uint32_t dn_port, uint32_t rwdir,
        uint32_t err_type, uint32_t err_per, uint32_t err_chktype);
extern void DNAckEIJ_rm_item(uint32_t dn_ipaddr, uint32_t dn_port, uint32_t rwdir,
        uint32_t err_type, uint32_t err_per, uint32_t err_chktype);

extern void DNAckEIJ_add_item_str(int8_t *cmd_buffer, int32_t cmd_len);
extern void DNAckEIJ_rm_item_str(int8_t *cmd_buffer, int32_t cmd_len);
extern void DNAckEIJ_update_item_str(int8_t *cmd_buffer, int32_t cmd_len);

extern int32_t DNAckEIJ_meet_error(uint32_t src_ipaddr, int32_t src_port, int32_t rw);
extern int32_t DNAckEIJ_show_errorItem(int8_t *cmd_buffer, int32_t max_len);

extern int8_t *DNAckEIJ_get_help(void);
extern void DNAckEIJ_init_manager(void);
extern void DNAckEIJ_rel_manager(void);

#endif /* DISCODN_CLIENT_DNACK_ERRORINJECTION_H_ */
