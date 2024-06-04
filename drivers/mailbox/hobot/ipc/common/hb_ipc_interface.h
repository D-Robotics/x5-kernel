/*************************************************************************
 *                     COPYRIGHT NOTICE
 *            Copyright 2021-2023 Horizon Robotics, Inc.
 *                   All rights reserved.
 *************************************************************************/

/**
 * @file hb_ipc_interface.h
 *
 * @NO{S17E09C06I}
 * @ASIL{B}
 */

#ifndef HB_IPC_INTERFACE_H
#define HB_IPC_INTERFACE_H

#define IPC_MBOX_NONE	(-1)/**< ipc unused mailbox, poll the instance*/

/**
 * @enum work_mode_e
 * Define the descriptor of ipc work mode.
 * @NO{S17E09C06}
 */
enum work_mode_e {
	DEFAULT_MODE = 0,/**< ipc default mode*/
	CUSTOM_MODE,/**< ipc custom mode*/
	NUM_WORK_MODE/**< number of the ipc work mode supported*/
};

/**
 * @enum trans_flags_e
 * Define the descriptor of ipc transmission flags.
 * @NO{S17E09C06}
 */
enum trans_flags_e {
	ASYNC_TRANS = 0,/**< ipc asynchronous transmission mode*/
	SYNC_TRANS = 0x1,/**< ipc synchronous transmission mode*/
	SLEEP_WAIT = 0,/**< sleep wait for acknowledgement after sends a meesage*/
	SPIN_WAIT = 0x4,/**< spin wait for acknowledgement after sends a meesage, no sleep*/
	MAX_TRANS_FLAG = 0x10000/**<  max value of transmission flags*/
};

/**
 * @struct ipc_pool_info
 * @brief Define the descriptor of pool.
 * @NO{S17E09C06}
 */
struct ipc_pool_info {
	uint16_t num_bufs;/**< number of buffers*/
	uint32_t buf_size;/**< size of buffers*/
};

/**
 * @struct ipc_channel_info
 * @brief Define the descriptor of channel.
 * @NO{S17E09C06}
 */
struct ipc_channel_info {
	int32_t num_pools;/**< number of pools*/
	struct ipc_pool_info *pools;/**< pointer to pools*/
	void (*recv_callback)(uint8_t *userdata, int32_t instance, int32_t chan_id,
			uint8_t *buf, uint64_t size);/**< receive callback function*/
	uint8_t *userdata;/**< user data of callback function*/
};


/**
 * @struct ipc_instance_info_m1
 * @brief Define the descriptor of custom mode.
 * @NO{S17E09C06}
 */
struct ipc_instance_info_m1 {
	uint64_t local_shm_addr;/**< address of local share memory*/
	uint64_t remote_shm_addr;/**< address of remote share memory*/
	uint32_t shm_size;/**< size of share memory, remote size is equal to local size*/
	int32_t num_chans;/**< number of channel*/
	struct ipc_channel_info *chans;/**< pointer to channels*/

};

/**
 * @struct ipc_instance_info_m0
 * @brief Define the descriptor of default mode.
 * @NO{S17E09C06}
 */
struct ipc_instance_info_m0 {
	 void (*recv_callback)(uint8_t *userdata, int32_t instance, int32_t chan_id,
			uint8_t *buf, uint64_t size);/**< receive callback function*/
	uint8_t *userdata;/**< user data of callback function*/
};

/**
 * @struct ipc_instance_cfg
 * @brief Define the descriptor of ipc instance common information.
 * @NO{S17E09C06}
 */
struct ipc_instance_cfg {
	int32_t mode;/**< work mode, 0 default mode, 1 custom mode*/
	int32_t timeout;/**< blocking time, -1 blocking, 0 non-blocking, >0 blocking until timeout*/
	int32_t trans_flags;/**< transmission flags, 0 asynchnization, 1 synchnization*/
	int32_t mbox_chan_idx;/**< mailbox channel index, -1 unused mailbox, polling instance, >=0 mailbox channel index*/
	union {
		struct ipc_instance_info_m0 def_cfg;/**< information of default mode*/
		struct ipc_instance_info_m1 custom_cfg;/**< information of custom mode*/
	} info;/**< information of work mode*/
};

/**
 * @struct ipc_dev_instance
 * Define the descriptor of ipc device
 * @NO{S17E09C06}
 */
struct ipc_dev_instance {
        int32_t instance;/**< instance id*/
        struct device *dev;/**< deivce pointer*/
        struct ipc_instance_cfg ipc_info;/**< ipc instance information*/
};

/**
 * @NO{S17E09C06I}
 * @ASIL{B}
 * @brief open a ipc instance, and updata instance status to opened
 *
 * @param[in] instance: ipc instance id
 * @param[in] cfg: ipc instance data, including share memory information and mailbox channel index
 *
 * @retval "0": success
 * @retval "!0": failure
 *
 * @callgraph
 * @callergraph
 * @design
 */
int32_t hb_ipc_open_instance(int32_t instance, const struct ipc_instance_cfg *cfg);

/**
 * @NO{S17E09C06I}
 * @ASIL{B}
 * @brief close a ipc instance, and updata instance status to closed
 *
 * @param[in] instance: ipc instance id
 *
 * @retval "0": success
 * @retval "!0": failure
 *
 * @callgraph
 * @callergraph
 * @design
 */
int32_t hb_ipc_close_instance(int32_t instance);

/**
 * @NO{S17E09C06I}
 * @ASIL{B}
 * @brief acquire a local buffer for the specified instance and channel
 *
 * @param[in] instance: ipc instance id
 * @param[in] chan_id: ipc channel id
 * @param[in] size: size of data acquired
 *
 * @param[out] buf: pointer to buffer
 *
 * @retval "0": success
 * @retval "!0": failure
 *
 * @callgraph
 * @callergraph
 * @design
 */
int32_t hb_ipc_acquire_buf(int32_t instance, int32_t chan_id, uint64_t size, uint8_t **buf);

/**
 * @NO{S17E09C06I}
 * @ASIL{B}
 * @brief release a remote buffer for the specified instance and channel
 *
 * @param[in] instance: ipc instance id
 * @param[in] chan_id: ipc channel id
 * @param[in] buf: pointer to buffer
 *
 *
 * @retval "0": success
 * @retval "!0": failure
 *
 * @callgraph
 * @callergraph
 * @design
 */
int32_t hb_ipc_release_buf(int32_t instance, int32_t chan_id, const uint8_t *buf);

/**
 * @NO{S17E09C06I}
 * @ASIL{B}
 * @brief send notification to remote processor
 *
 * @param[in] instance: ipc instance id
 * @param[in] chan_id: ipc channel id
 * @param[in] buf: pointer to buffer
 * @param[in] size: size of data
 *
 *
 * @retval "0": success
 * @retval "!0": failure
 *
 * @callgraph
 * @callergraph
 * @design
 */
int32_t hb_ipc_send(int32_t instance, int32_t chan_id, uint8_t *buf, uint64_t size);

/**
 * @NO{S17E09C06I}
 * @ASIL{B}
 * @brief check the status of remote processor
 *
 * @param[in] instance: ipc instance id
 *
 *
 * @retval "0": remote processor readied
 * @retval "!0": remote processor not ready
 *
 * @callgraph
 * @callergraph
 * @design
 */
int32_t hb_ipc_is_remote_ready(int32_t instance);

/**
 * @NO{S17E09C06I}
 * @ASIL{B}
 * @brief poll a instance, if the instance received message, call receive callback function.
 *
 * @param[in] instance: ipc instance id
 *
 *
 * @retval ">0": number of received messages
 * @retval "0": no message
 * @retval "<0": failure
 *
 * @callgraph
 * @callergraph
 * @design
 */
int32_t hb_ipc_poll_instance(int32_t instance);

#endif /* HB_IPC_INTERFACE_H */
