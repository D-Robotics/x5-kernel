/*************************************************************************
 *                     COPYRIGHT NOTICE
 *            Copyright 2021-2023 Horizon Robotics, Inc.
 *                   All rights reserved.
 *************************************************************************/

/**
 * @file mbox_common.h
 *
 * @NO{S17E09C06U}
 * @ASIL{B}
 */

#ifndef MBOX_COMMON__H
#define MBOX_COMMON__H

#define SEND_NOTIFY (1)/**< send notify by send register*/
#define ACK_NOTIFY (2)/**< ack notify by send register*/

uint32_t com_get_src(uint8_t *base, uint8_t mbox_id);
int32_t com_set_src(uint8_t *base, uint8_t mbox_id, uint8_t src_id);
int32_t com_clear_src(uint8_t *base, uint8_t mbox_id, uint8_t src_id);
uint32_t com_get_dst(uint8_t *base, uint8_t mbox_id);
void com_set_dst(uint8_t *base, uint8_t mbox_id, uint8_t dst_id);

#if 0 //reserved
void com_clear_dst(uint8_t *base, uint8_t mbox_id, uint8_t dst_id);
#endif

uint32_t com_get_irq_cfgstatus(uint8_t *base, uint8_t mbox_id);
void com_enable_irq(uint8_t *base, uint8_t mbox_id, uint8_t int_id);
void com_disable_irq(uint8_t *base, uint8_t mbox_id, uint8_t int_id);

#if 0 // reserved
uint32_t com_get_mode(uint8_t *base, uint8_t mbox_id);
#endif

void com_set_mode(uint8_t *base, uint8_t mbox_id, uint32_t mode);
uint32_t com_get_sendreg(uint8_t *base, uint8_t mbox_id);
void com_clear_sendreg(uint8_t *base, uint8_t mbox_id);
void com_tx_notify(uint8_t *base, uint8_t mbox_id);
void com_ack_notify(uint8_t *base, uint8_t mbox_id);
uint32_t com_get_ipcmmis(uint8_t *base, uint8_t int_id, uint8_t dr);
uint32_t com_get_ipcmris(uint8_t *base, uint8_t int_id, uint8_t dr);
uint32_t com_get_mboxdata(uint8_t *base, uint8_t mbox_id, uint8_t dr);
uint8_t *com_get_mboxdata_addr(uint8_t *base, uint8_t mbox_id, uint8_t dr);
uint32_t com_get_mboxdata_level(uint8_t *base, uint8_t mbox_id);

#endif /*MBOX_COMMON__H*/
