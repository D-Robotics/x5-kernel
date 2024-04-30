/*************************************************************************
 *                     COPYRIGHT NOTICE
 *            Copyright 2021-2023 Horizon Robotics, Inc.
 *                   All rights reserved.
 *************************************************************************/

/**
 * @file hb_mbox.h
 *
 * @NO{S17E09C06I}
 * @ASIL{B}
 */

#ifndef HB_MBOX_H
#define HB_MBOX_H

typedef void (*mbox_cb_t)(void *cb_arg, uint8_t ipcm_id, uint8_t chan_id, void *data);

/**
 * @enum mbox_trans_flags_e
 * Define the descriptor of mbox transmission flags.
 * @NO{S17E09C06}
 */
enum mbox_trans_flags_e {
	MBOX_ASYNC_TRANS = 0,/**< mbox asynchronous transmission mode*/
	MBOX_SYNC_TRANS = 0x1,/**< mbox synchronous transmission mode*/
	MBOX_SLEEP_WAIT = 0,/**< sleep wait for acknowledgement after sends a meesage*/
	MBOX_SPIN_WAIT = 0x4,/**< spin wait for acknowledgement after sends a meesage, no sleep*/
	MBOX_MAX_TRANSMISSION_FLAG = 0x10000/**<  max value of transmission flags*/
};

/**
 * @struct mbox_chan_dev
 * Define the descriptor of mailbox channel device
 * @NO{S17E09C06}
 */
struct mbox_chan_dev {
	uint8_t send_mbox_id;/**< used to send msg, be inited by local core*/
	uint8_t recv_mbox_id;/**< used to recv msg, be inited by remote core*/
	uint8_t local_id;/**< local core id*/
	uint8_t remote_id;/**< remote core id*/
	uint32_t local_ack_link_mode;/**< PL320 mode, including auto ack and auto link*/
	uint32_t irq;/**< local irq*/
};

/**
 * @struct user_chan_cfg
 * Define the descriptor of user channel configuration
 * @NO{S17E09C06}
 */
struct user_chan_cfg {
	uint32_t timeout;/**< waiting ACK time, =0 default time, >0 specific time*/
	uint32_t trans_flags;/**< bit[0]: =0, async, =1, sync;
				  bit[1]: reserved;
				  bit[2]: =0, sleep waiting, =1, spin waiting;
				  bit[31:3]: reserved*/
	mbox_cb_t rx_cb;/**< recevied callback function*/
	void *rx_cb_arg;/**< recevied callback arguments*/
	struct mbox_chan_dev *cdev;/**< mailbox channel device*/
};

/**
 * @NO{S17E09C06I}
 * @ASIL{B}
 * @brief init mailbox, function is non-reentrant.
 *
 * @retval "0": success
 * @retval "<0": failure
 *
 * @callgraph
 * @callergraph
 * @design
 */
int32_t hb_mbox_init(void);

/**
 * @NO{S17E09C06I}
 * @ASIL{B}
 * @brief deinit mailbox, function is non-reentrant.
 *
 * @callgraph
 * @callergraph
 * @design
 */
void hb_mbox_deinit(void);

/**
 * @NO{S17E09C06I}
 * @ASIL{B}
 * @brief open mailbox channel
 *
 * @param[in] ipcm_id: ipcm id
 * @param[in] chan_id: channel id
 * @param[in] ucfg: user configuation
 *
 * @retval "0": success
 * @retval "<0": failure
 *
 * @callgraph
 * @callergraph
 * @design
 */
int32_t hb_mbox_open_chan(uint8_t ipcm_id, uint8_t chan_id, struct user_chan_cfg *ucfg);

/**
 * @NO{S17E09C06I}
 * @ASIL{B}
 * @brief close mailbox channel
 *
 * @param[in] ipcm_id: ipcm id
 * @param[in] chan_id: channel id
 *
 * @retval "0": success
 * @retval "<0": failure
 *
 * @callgraph
 * @callergraph
 * @design
 */
int32_t hb_mbox_close_chan(uint8_t ipcm_id, uint8_t chan_id);

/**
 * @NO{S17E09C06I}
 * @ASIL{B}
 * @brief send notify
 *
 * @param[in] ipcm_id: ipcm id
 * @param[in] chan_id: channel id
 * @param[in] data: send data, no NULL
 *
 * @retval "0": success
 * @retval "<0": failure
 *
 * @callgraph
 * @callergraph
 * @design
 */
int32_t hb_mbox_send(uint8_t ipcm_id, uint8_t chan_id, void *data);

/**
 * @NO{S17E09C06I}
 * @ASIL{B}
 * @brief flush channel, check ack
 *
 * @param[in] ipcm_id: ipcm id
 * @param[in] chan_id: channel id
 * @param[in] timeout: waiting time, 0 default time, >0 specific time
 *
 * @retval "0": success
 * @retval "<0": failure
 *
 * @callgraph
 * @callergraph
 * @design
 */
int32_t hb_mbox_flush(uint8_t ipcm_id, uint8_t chan_id, uint32_t timeout);

/**
 * @NO{S17E09C06I}
 * @ASIL{B}
 * @brief check notify
 *
 * @param[in] ipcm_id: ipcm id
 * @param[in] chan_id: channel id
 * @param[in] timeout: waiting time, 0 default time, >0 specific time
 *
 * @retval ">0": success recevied data
 * @retval "0": no data
 * @retval "<0": failure
 *
 * @callgraph
 * @callergraph
 * @design
 */
int32_t hb_mbox_peek_data(uint8_t ipcm_id, uint8_t chan_id);

#endif /*HB_MBOX_H*/
