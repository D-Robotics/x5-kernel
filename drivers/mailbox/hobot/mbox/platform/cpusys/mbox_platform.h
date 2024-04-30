/*************************************************************************
 *                     COPYRIGHT NOTICE
 *            Copyright 2021-2023 Horizon Robotics, Inc.
 *                   All rights reserved.
 *************************************************************************/

#ifndef HB_MBOX__H
#define HB_MBOX__H

/*config information in cpusys*/
#define NUM_IPCM (3)/**< ipcm number*/
#define NUM_DATA (7)/**< data number*/
#define NUM_INT (32)/**< ini number*/
#define NUM_MBOX (32)/**< mailbox number*/
#define MAX_CHAN_PER_IPCM (2)/**< channel number per ipcm*/
#define MAX_CHAN_TOTAL (32)/**< channel number total*/
#define MAX_LOCAL_IRQ (16)/**< local irq number*/
#define NUM_USER_DATA (5)/**< user data register number*/
#define BYTES_USER_DATA (20)/**< user data max bytes*/
#define BYTES_TOTAL_DATA (28)/**< user data max bytes*/
#define SIZE_DATA_ID (6)/**< size location in data register*/

/**
 * @NO{S17E09C06U}
 * @ASIL{B}
 * @brief cfg transfer flags to mailbox channel
 *
 * @param[in] chan:  mailbox channel
 * @param[in] trans_flags: trnasfer flags
 *
 * @retval "0": success
 * @retval "!0": failure
 *
 * @callgraph
 * @callergraph
 * @design
 */
int32_t hb_mbox_cfg_trans_flags(struct mbox_chan *chan, int32_t trans_flags);


#endif /* HB_MBOX__H */
