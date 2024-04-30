/*************************************************************************
 *                     COPYRIGHT NOTICE
 *            Copyright 2021-2023 Horizon Robotics, Inc.
 *                   All rights reserved.
 *************************************************************************/

/**
 * @file ipc_shm.h
 *
 * @NO{S17E09C06U}
 * @ASIL{B}
 */

#ifndef IPC_SHM_H
#define IPC_SHM_H

struct ipc_channel_info;
struct ipc_instance_cfg;

/**
 * @struct ipc_shm_cfg
 * Define the descriptor of IPC shm parameters
 * @NO{S17E09C06}
 */
struct ipc_shm_cfg {
	int32_t mode;/**< work mode, 0 default mode, 1 custom mode*/
	int32_t timeout;/**< blocking time, 0 default blocking time, >0 specific blocking time*/
	int32_t trans_flags;/**< transmission flags,,
				* range:[0,MAX_TRANS_FLAG);
				* bit0 = 0, async, bit0 = 1 sync
				* bit2 = 0, hb_ipc_send no sleep, bit2 = 1, hb_ipc_send sleep
				*/
	int32_t mbox_chan_idx;/**< mailbox channel index, -1 unused mailbox, polling instance, >=0 mailbox channel index*/
	uint64_t local_shm_addr;/**< address of local share memory*/
	uint64_t remote_shm_addr;/**< address of remote share memory*/
	uint32_t shm_size;/**< size of share memory, remote size is equal to local size*/
	int32_t num_chans;/**< number of channel*/
	struct ipc_channel_info *chans;/**< pointer to channels*/
};

int32_t ipc_shm_init_instance_cfg(int32_t instance, const struct ipc_instance_cfg *cfg);
int32_t ipc_shm_init_instance(const int32_t instance, const struct ipc_shm_cfg *cfg);
void ipc_shm_free_instance(int32_t instance);
void *ipc_shm_acquire_buf(const int32_t instance, int32_t chan_id, size_t size);
int32_t ipc_shm_release_buf(const int32_t instance, int32_t chan_id, const void *buf);
int32_t ipc_shm_tx(const int32_t instance, int32_t chan_id, void *buf, size_t size);
int32_t ipc_shm_is_remote_ready(const int32_t instance);
int32_t ipc_shm_poll_channels(const int32_t instance);

#endif /* IPC_SHM_H */
