/*************************************************************************
 *                     COPYRIGHT NOTICE
 *            Copyright 2021-2023 Horizon Robotics, Inc.
 *                   All rights reserved.
 *************************************************************************/

/**
 * @file ipc_shm.c
 *
 * @NO{S17E09C06U}
 * @ASIL{B}
 */

#include "ipc_os.h"

#define ipc_max(x, y)		(((x) > (y)) ? (x) : (y))

/* magic number to indicate the driver is initialized */
#define IPC_SHM_STATE_READY	0x49504346U/*IPCF*/
#define IPC_SHM_STATE_CLEAR	0u

/**
 * @enum ipc_shm_instance_state
 * @brief Define the used for IPC instance status.
 * @NO{S17E09C06}
 */
enum ipc_shm_instance_state {
	IPC_SHM_INSTANCE_USED = 0u,/**< instance is used*/
	IPC_SHM_INSTANCE_FREE,/**< instance is free and can be used*/
	IPC_SHM_INSTANCE_ERROR/**< there are some errors*/
};

/**
 * @struct ipc_shm_bd
 * Define the descriptor of buffer descriptor (store buffer location and data size)
 * @NO{S17E09C06}
 */
struct ipc_shm_bd {
	int16_t pool_id;/**< index of buffer pool*/
	uint16_t buf_id;/**< index of buffer from buffer pool*/
	uint32_t data_size;/**< size of data written in buffer*/
};

/**
 * @struct ipc_shm_pool
 * Define the descriptor of buffer pool private data
 * @NO{S17E09C06}
 */
struct ipc_shm_pool {
	uint16_t num_bufs;/**< number of buffers in pool*/
	uint32_t buf_size;/**< size of buffers*/
	uint32_t shm_size;/**< size of shared memory mapped by this pool (queue + bufs)*/
	uint64_t local_pool_addr;/**< address of local buffer pool*/
	uint64_t remote_pool_addr;/**< address of remote buffer pool*/
	struct ipc_queue bd_queue;/**< queue containing BDs of free buffers*/
};

/**
 * @struct ipc_shm_channel
 * Define the descriptor of channel private data
 * @NO{S17E09C06}
 */
struct ipc_shm_channel {
	int32_t id;/**< channel id*/
	struct ipc_queue bd_queue;/**< queue containing BDs of sent/received buffers*/
	int32_t num_pools;/**< number of buffer pools*/
	struct ipc_shm_pool pools[MAX_NUM_POOL_PER_CHAN];/**< buffer pools private data*/
	void (*recv_callback)(uint8_t *userdata, int32_t instance, int32_t chan_id,
			uint8_t *buf, uint64_t size);/**< receive callback*/
	uint8_t *userdata;/**< optional receive callback argument*/
};

/**
 * @struct ipc_shm_global
 * Define the descriptor of ipc shm global data shared with remote
 * @NO{S17E09C06}
 */
struct ipc_shm_global {
	uint32_t state;/**< state to indicate whether local is initialized*/
	uint32_t reserve;/**< reserve for 8 bytes align*/
};

/**
 * @struct ipc_shm_priv
 * Define the descriptor of ipc shm private data
 * @NO{S17E09C06}
 */
struct ipc_shm_priv {
	uint32_t shm_size;/**< local/remote shared memory size*/
	int32_t num_chans;/**< number of shared memory channels*/
	struct ipc_shm_channel channels[MAX_NUM_CHAN_PER_INSTANCE];/**< ipc channels private data*/
	struct ipc_shm_global *global;/**< local global data shared with remote*/

	void __iomem *ipc_shm_mask;/**< ipc instance/channel info*/
};

/* ipc shm private data */
static struct ipc_shm_priv ipc_shm_priv_data[MAX_NUM_INSTANCE];

/* get channel without validation (used in internal functions only) */
static inline struct ipc_shm_channel *get_channel_priv(int32_t instance,
		int32_t chan_id)
{
	return &ipc_shm_priv_data[instance].channels[chan_id];
}

/* get channel with validation (can be used in API functions) */
static inline struct ipc_shm_channel *get_channel(int32_t instance,
		int32_t chan_id)
{
	if ((chan_id < 0)
		|| (chan_id >= ipc_shm_priv_data[instance].num_chans)) {
		ipc_err("Channel id outside valid range: 0 - %d\n",
				ipc_shm_priv_data[instance].num_chans);
		return NULL;
	}

	return get_channel_priv(instance, chan_id);
}

/**
 * @NO{S17E09C06U}
 * @ASIL{B}
 * @brief handle Rx for a single channel
 *
 * @param[in] instance: instance id
 * @param[in] chan_id: channel id
 * @param[in] budget: available work budget (number of messages to be processed)
 *
 * @return work done
 *
 * @callgraph
 * @callergraph
 * @design
 */
static int32_t ipc_channel_rx(int32_t instance, int32_t chan_id, int32_t budget)
{
	struct ipc_shm_channel *chan = get_channel_priv(instance, chan_id);
	struct ipc_shm_pool *pool;
	struct ipc_shm_bd bd;
	uint64_t buf_addr;
	int32_t err;
	int32_t work = 0;

	/* managed channels: process incoming BDs in the limit of budget */
	while (work < budget) {
		err = ipc_queue_pop(&chan->bd_queue, &bd);
		if (err != 0) {
			break;
		}
		pool = &chan->pools[bd.pool_id];
		buf_addr = pool->remote_pool_addr +
			(bd.buf_id * pool->buf_size);

		chan->recv_callback((uint8_t *)(chan->userdata), instance, chan->id,
				(uint8_t *)buf_addr, bd.data_size);
		work++;
	}

	return work;
}

/**
 * @NO{S17E09C06U}
 * @ASIL{B}
 * @brief determine if the instance is used or not
 *
 * @param[in] instance: instance id
 *
 * @retval "IPC_SHM_INSTANCE_FREE": instance is free
 * @retval "IPC_SHM_INSTANCE_USED": instance is used
 * @retval "IPC_SHM_INSTANCE_ERROR": instance is errors
 *
 * @callgraph
 * @callergraph
 * @design
 */
static uint8_t ipc_instance_is_free(int32_t instance)
{
	if (instance >= MAX_NUM_INSTANCE || instance < 0)
		return IPC_SHM_INSTANCE_ERROR;

	if (ipc_shm_priv_data[instance].global == NULL)
		return IPC_SHM_INSTANCE_FREE;

	if (ipc_shm_priv_data[instance].global->state == IPC_SHM_STATE_CLEAR)
		return IPC_SHM_INSTANCE_FREE;

	return IPC_SHM_INSTANCE_USED;
}

/**
 * @NO{S17E09C06U}
 * @ASIL{B}
 * @brief shm Rx handler, called from softirq
 *
 * @param[in] instance: instance id
 * @param[in] budget: available work budget (number of messages to be processed)
 *
 * @return work done
 *
 * @callgraph
 * @callergraph
 * @design
 */
static int32_t ipc_shm_rx(int32_t instance, int32_t budget)
{
	int32_t num_chans = ipc_shm_priv_data[instance].num_chans;
	int32_t chan_budget, chan_work = 0;
	int32_t more_work = 1;
	int32_t work = 0;
	int32_t i;
	int32_t mask = readl(ipc_shm_priv_data[instance].ipc_shm_mask);
	int32_t chan_id = mask >> (4 * instance + 1);

	/* fair channel handling algorithm */
	while ((work < budget) && (more_work > 0)) {
		chan_budget = ipc_max(((budget - work) / num_chans), 1);
		more_work = 0;

		for (i = 0; i < num_chans; i++) {
			if (i == chan_id)
				chan_work = ipc_channel_rx(instance, i, chan_budget);
			work += chan_work;

			if (chan_work == chan_budget)
				more_work = 1;
		}
	}

	return work;
}

/**
 * @NO{S17E09C06U}
 * @ASIL{B}
 * @brief initialize one shared memory device
 *
 * @param[in] instance: instance id
 * @param[in] chan_id: channel index
 * @param[in] pool_id: ipc instance id
 * @param[in] local_shm: local channel shared memory address
 * @param[in] remote_shm: remote channel shared memory address
 * @param[in] cfg: channel configuration parameters
 *
 * @retval "0": success
 * @retval "!0": failure
 *
 * @callgraph
 * @callergraph
 * @design
 */
static int32_t ipc_buf_pool_init(int32_t instance, int32_t chan_id, int32_t pool_id,
		uint64_t local_shm, uint64_t remote_shm,
		const struct ipc_pool_info *cfg)
{
	struct ipc_shm_channel *chan = get_channel(instance, chan_id);
	struct ipc_shm_pool *pool = &chan->pools[pool_id];
	struct ipc_shm_bd bd;
	uint32_t queue_mem_size;
	uint16_t i;
	int32_t err;

	if (cfg->num_bufs > MAX_NUM_BUF_PER_POOL) {
		ipc_err("Too many buffers configured in pool. "
				"Increase MAX_NUM_BUF_PER_POOL if needed\n");

		return -EINVAL;
	}

	pool->num_bufs = cfg->num_bufs;
	pool->buf_size = cfg->buf_size;

	/* init pool bd_queue with push ring mapped at the start of local
	 * pool shm and pop ring mapped at start of remote pool shm
	 */
	err = ipc_queue_init(&pool->bd_queue, pool->num_bufs,
		(uint16_t)sizeof(struct ipc_shm_bd), local_shm, remote_shm);
	if (err != 0) {
		ipc_err("pool queue init failed\n");

		return err;
	}

	/* init local/remote buffer pool addrs */
	queue_mem_size = ipc_queue_mem_size(&pool->bd_queue);

	/* init actual local buffer pool addr */
	pool->local_pool_addr = local_shm + queue_mem_size;

	/* init actual remote buffer pool addr */
	pool->remote_pool_addr = remote_shm + queue_mem_size;

	pool->shm_size = queue_mem_size + (cfg->buf_size * cfg->num_bufs);

	/* check if pool fits into shared memory */
	if ((local_shm + pool->shm_size)
			> (ipc_os_get_local_shm(instance)
				+ ipc_shm_priv_data[instance].shm_size)) {
		ipc_err("Not enough shared memory for pool %d from channel %d\n",
				pool_id, chan_id);

		return -ENOMEM;
	}

	/* populate bd_queue with free BDs from remote pool */
	for (i = 0; i < pool->num_bufs; i++) {
		bd.pool_id = (int16_t) pool_id;
		bd.buf_id = i;
		bd.data_size = 0;

		err = ipc_queue_push(&pool->bd_queue, &bd);
		if (err != 0) {
			ipc_err("Unable to init queue with free buffer descriptors "
				"for pool %d of channel %d :%d\n",
				pool_id, chan_id, err);

			return err;
		}
	}

	ipc_dbg("ipc shm pool %d of chan %d initialized\n", pool_id, chan_id);

	return 0;
}

/**
 * @NO{S17E09C06U}
 * @ASIL{B}
 * @brief initialize shared memory IPC channel
 *
 * @param[in] instance: instance id
 * @param[in] chan_id: channel index
 * @param[in] local_shm: local channel shared memory address
 * @param[in] remote_shm: remote channel shared memory address
 * @param[in] cfg: channel configuration parameters
 *
 * @retval "0": success
 * @retval "!0": failure
 *
 * @callgraph
 * @callergraph
 * @design
 */
static int32_t ipc_shm_channel_init(int32_t instance, int32_t chan_id,
		uint64_t local_shm, uint64_t remote_shm,
		const struct ipc_channel_info *cfg)
{
	struct ipc_shm_channel *chan = get_channel(instance, chan_id);
	struct ipc_pool_info *pool_cfg;
	uint64_t local_pool_shm;
	uint64_t remote_pool_shm;
	uint32_t queue_mem_size;
	uint32_t prev_buf_size = 0;
	uint16_t total_bufs = 0;
	int32_t err, i;


	if (cfg == NULL) {
		ipc_err("NULL channel configuration argument\n");

		return -EINVAL;
	}

	if (cfg->recv_callback == NULL) {
		ipc_err("Receive callback not specified\n");

		return -EINVAL;
	}

	if (cfg->pools == NULL) {
		ipc_err("NULL buffer pool configuration argument\n");

		return -EINVAL;
	}

	if ((cfg->num_pools < 1) ||
		(cfg->num_pools > MAX_NUM_POOL_PER_CHAN)) {
		ipc_err("Number of pools must be between 1 and %d\n",
				MAX_NUM_POOL_PER_CHAN);

		return -EINVAL;
	}

	/* save common channel parameters */
	chan->id = chan_id;
	chan->recv_callback = cfg->recv_callback;
	chan->userdata = cfg->userdata;
	chan->num_pools = cfg->num_pools;

	/* check that pools are sorted in ascending order by buf size
	 * and count total number of buffers from all pools
	 */
	for (i = 0; i < chan->num_pools; i++) {
		pool_cfg = &cfg->pools[i];

		if (pool_cfg->buf_size < prev_buf_size) {
			ipc_err("Pools must be sorted in ascending order by buffer size\n");

			return -EINVAL;
		}
		prev_buf_size = pool_cfg->buf_size;
		total_bufs += pool_cfg->num_bufs;
	}

	/* init channel bd_queue with push ring mapped at the start of local
	 * channel shm and pop ring mapped at start of remote channel shm
	 */
	err = ipc_queue_init(&chan->bd_queue, total_bufs,
			     (uint16_t)sizeof(struct ipc_shm_bd),
			     local_shm, remote_shm);
	if (err != 0) {
		ipc_err("channel queue init failed\n");

		return err;
	}

	/* init&map buffer pools after channel bd_queue */
	queue_mem_size = ipc_queue_mem_size(&chan->bd_queue);
	local_pool_shm = local_shm + queue_mem_size;
	remote_pool_shm = remote_shm + queue_mem_size;

	/* check if pool fits into shared memory */
	if ((local_pool_shm)
			> (ipc_os_get_local_shm(instance)
				+ ipc_shm_priv_data[instance].shm_size)) {
		ipc_err("Not enough shared memory for channel %d\n",
				chan_id);

		return -ENOMEM;
	}

	for (i = 0; i < chan->num_pools; i++) {
		err = ipc_buf_pool_init(instance, chan_id, i, local_pool_shm,
				remote_pool_shm, &cfg->pools[i]);
		if (err != 0) {
			ipc_err("pool init failed\n");

			return err;
		}

		/* compute next pool local/remote shm base address */
		local_pool_shm += chan->pools[i].shm_size;
		remote_pool_shm += chan->pools[i].shm_size;
	}

	ipc_dbg("ipc shm channel %d initialized\n", chan_id);

	return 0;
}

/* Get channel local mapped memory size */
static uint32_t get_chan_memmap_size(int32_t instance, int32_t chan_id)
{
	struct ipc_shm_channel *chan = get_channel(instance, chan_id);
	uint32_t size = 0;
	int32_t i;

	if (chan == NULL) {
		ipc_err("Invalid channel parameter\n");

		return -EINVAL;
	}
	/* channels: size of BD queue + size of buf pools */
	size = ipc_queue_mem_size(&chan->bd_queue);
	for (i = 0; i < chan->num_pools; i++) {
		size += chan->pools[i].shm_size;
	}

	return size;
}

/**
 * @NO{S17E09C06U}
 * @ASIL{B}
 * @brief initialize one shared memory device
 *
 * @param[in] instance: ipc instance id
 * @param[in] cfg: configuration parameters
 *
 * @retval "0": success
 * @retval "!0": failure
 *
 * @callgraph
 * @callergraph
 * @design
 */
int32_t ipc_shm_init_instance(int32_t instance,
	const struct ipc_shm_cfg *cfg)
{
	uint64_t local_chan_shm;
	uint64_t remote_chan_shm;
	uint64_t local_shm;
	uint32_t chan_size;
	size_t chan_offset;
	int32_t err, i;

	if (cfg == NULL) {
		ipc_err("cfg is NULL\n");

		return -EINVAL;
	}

	if ((cfg->local_shm_addr == (uint64_t) NULL)
			|| (cfg->remote_shm_addr == (uint64_t) NULL)) {
		ipc_err("NULL local or remote address\n");

		return -EINVAL;
	}

	if ((cfg->num_chans < 1) ||
		(cfg->num_chans > MAX_NUM_CHAN_PER_INSTANCE)) {
		ipc_err("Number of channels must be between 1 and %d\n",
				MAX_NUM_CHAN_PER_INSTANCE);

		return -EINVAL;
	}

	/* init OS specific resources */
	err = ipc_os_init(instance, cfg, ipc_shm_rx);
	if (err != 0) {
		ipc_err("ipc os init failed: %d\n", err);

		return err;
	}

	/* save api params */
	ipc_shm_priv_data[instance].shm_size = cfg->shm_size;
	ipc_shm_priv_data[instance].num_chans = cfg->num_chans;

	/* global data stored at beginning of local shared memory */
	local_shm = ipc_os_get_local_shm(instance);
	ipc_shm_priv_data[instance].global = (struct ipc_shm_global *)local_shm;

	/* init channels */
	chan_offset = sizeof(struct ipc_shm_global);
	local_chan_shm = local_shm + (uint64_t) chan_offset;
	remote_chan_shm = ipc_os_get_remote_shm(instance)
			+ (uint64_t) chan_offset;
	ipc_dbg("initializing channels...\n");
	for (i = 0; i < ipc_shm_priv_data[instance].num_chans; i++) {
		err = ipc_shm_channel_init(instance, i, local_chan_shm,
				remote_chan_shm, &cfg->chans[i]);
		if (err != 0) {
			ipc_err("ipc channel %d init failed: %d\n", i, err);
			ipc_os_free(instance);
			ipc_shm_priv_data[instance].global = NULL;

			return err;
		}

		/* compute next channel local/remote shm base address */
		chan_size = get_chan_memmap_size(instance, i);
		local_chan_shm += chan_size;
		remote_chan_shm += chan_size;
	}

	ipc_shm_priv_data[instance].ipc_shm_mask = cfg->ipc_shm_mask;

	ipc_shm_priv_data[instance].global->state = IPC_SHM_STATE_READY;
	ipc_dbg("ipc shm initialized\n");

	return 0;
}

/**
 * @NO{S17E09C06U}
 * @ASIL{B}
 * @brief release one instances of shared memory device
 *
 * @param[in] instance: ipc instance id
 *
 * @return None
 *
 * @callgraph
 * @callergraph
 * @design
 */
void ipc_shm_free_instance(int32_t instance)
{
	/* check if instance must be free */
	if (ipc_instance_is_free(instance) == IPC_SHM_INSTANCE_USED) {

		/* reset state */
		ipc_shm_priv_data[instance].global->state =
			IPC_SHM_STATE_CLEAR;
		ipc_shm_priv_data[instance].global = NULL;
		ipc_os_free(instance);
	}

	ipc_dbg("ipc shm instance %d released\n", instance);
}

/**
 * @NO{S17E09C06U}
 * @ASIL{B}
 * @brief initialize configuration of custom mode
 *
 * @param[in] instance: ipc instance id
 * @param[in] cfg: configuration parameters
 *
 * @retval "0": success
 * @retval "!0": failure
 *
 * @callgraph
 * @callergraph
 * @design
 */
static int32_t ipc_shm_init_custom_cfg(int32_t instance, const struct ipc_instance_cfg *cfg)
{
	return ipc_shm_init_instance(instance, (struct ipc_shm_cfg *)cfg);
}

/**
 * @NO{S17E09C06U}
 * @ASIL{B}
 * @brief initialize configuration of default mode
 *
 * @param[in] instance: ipc instance id
 * @param[in] cfg: configuration parameters
 *
 * @retval "0": success
 * @retval "!0": failure
 *
 * @callgraph
 * @callergraph
 * @design
 */
static int32_t ipc_shm_init_def_cfg(int32_t instance, const struct ipc_instance_cfg *cfg)
{
	struct ipc_instance_cfg *ipc_cfg;
	struct ipc_channel_info *chan;
	int32_t i, err;

	/**save parameter*/
	ipc_cfg = ipc_os_get_def_info(instance, cfg);
	if (ipc_cfg == NULL) {
		ipc_err("instance %d no support default mode, please use custom mode\n", instance);

		return -EINVAL;
	}

	ipc_cfg->timeout = cfg->timeout;
	ipc_cfg->trans_flags = cfg->trans_flags;
	ipc_cfg->mbox_chan_idx = cfg->mbox_chan_idx;
	for (i = 0; i< ipc_cfg->info.custom_cfg.num_chans; i++) {
		chan = ipc_cfg->info.custom_cfg.chans + i;
		chan->recv_callback = cfg->info.def_cfg.recv_callback;
		chan->userdata = cfg->info.def_cfg.userdata;
	}

	err = ipc_shm_init_custom_cfg(instance, ipc_cfg);

	return err;
}

/**
 * @NO{S17E09C06U}
 * @ASIL{B}
 * @brief initialize user config information
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
int32_t ipc_shm_init_instance_cfg(int32_t instance, const struct ipc_instance_cfg *cfg)
{
	int32_t err;

	if (instance < 0 || instance >= MAX_NUM_INSTANCE || cfg == NULL) {
		ipc_err("instance %d invalid parameter\n", instance);

		return -EINVAL;
	}

	if (cfg->mode == DEFAULT_MODE) {
		err = ipc_shm_init_def_cfg(instance, cfg);
		if (err != 0) {
			ipc_err("default mode init failed: %d\n", err);

			return err;
		}
	} else if (cfg->mode == CUSTOM_MODE) {
		err = ipc_shm_init_custom_cfg(instance, cfg);
		if (err != 0) {
			ipc_err("custom mode init failed: %d\n", err);

			return err;
		}
	} else {
		ipc_err("work mode invalid\n");

		return -EINVAL;
	}

	return 0;
}

/**
 * @NO{S17E09C06U}
 * @ASIL{B}
 * @brief request a buffer for the given channel
 *
 * @param[in] instance: ipc instance id
 * @param[in] chan_id: channel index
 * @param[in] size: required size
 *
 * @return pointer to the buffer base address or NULL if buffer not found
 *
 * @callgraph
 * @callergraph
 * @design
 */
void *ipc_shm_acquire_buf(int32_t instance, int32_t chan_id, size_t size)
{
	struct ipc_shm_channel *chan;
	struct ipc_shm_pool *pool = NULL;
	struct ipc_shm_bd bd = {.pool_id = 0, .buf_id = 0u, .data_size = 0u};
	uint64_t buf_addr;
	int32_t pool_id;

	/* check if instance is valid */
	if (ipc_instance_is_free(instance) != IPC_SHM_INSTANCE_USED) {
		ipc_err("instance %d is unused\n", instance);

		return NULL;
	}

	if (size == 0u) {
		ipc_err("size is 0\n");

		return NULL;
	}

	chan = get_channel(instance, chan_id);

	if (chan == NULL) {
		ipc_err("chan is NULL\n");

		return NULL;
	}

	/* find first non-empty pool that accommodates the requested size */
	for (pool_id = 0; pool_id < chan->num_pools; pool_id++) {
		pool = &chan->pools[pool_id];

		/* check if pool buf size covers the requested size */
		if (size > pool->buf_size)
			continue;

		/* check if pool has any free buffers left */
		if (ipc_queue_pop(&pool->bd_queue, &bd) == 0)
			break;
	}

	if (pool_id == chan->num_pools) {
		ipc_err("No free buffer found in channel %d\n", chan_id);

		return NULL;
	}
	buf_addr = pool->local_pool_addr +
		(uint32_t)(bd.buf_id * pool->buf_size);
	ipc_dbg("ch %d: pool %d: acquired buffer %d with address %#llx\n",
			chan_id, pool_id, bd.buf_id, buf_addr);

	return (void *)buf_addr;
}

/**
 * @NO{S17E09C06U}
 * @ASIL{B}
 * @brief find the pool that owns the specified buffer
 *
 * @param[in] chan: channel pointer
 * @param[in] buf: buffer pointer
 * @param[in] remote: flag telling if buffer is from remote OS
 *
 * @retval "0": success
 * @retval "!0": failure
 *
 * @callgraph
 * @callergraph
 * @design
 */
static int16_t find_pool_for_buf(struct ipc_shm_channel *chan,
		uint64_t buf, int32_t remote)
{
	struct ipc_shm_pool *pool;
	uint64_t addr;
	int16_t pool_id;
	uint32_t pool_size;

	for (pool_id = 0; pool_id < chan->num_pools; pool_id++) {
		pool = &chan->pools[pool_id];

		addr = (remote == 1) ?
				pool->remote_pool_addr : pool->local_pool_addr;
		pool_size = pool->num_bufs * pool->buf_size;

		if ((buf >= addr) && (buf < (addr + pool_size)))
			return pool_id;
	}

	ipc_err("not found buffer\n");

	return -EINVAL;
}

/**
 * @NO{S17E09C06U}
 * @ASIL{B}
 * @brief release a buffer for the given channel
 *
 * @param[in] instance: ipc instance id
 * @param[in] chan_id: channel index
 * @param[in] buf: buffer pointer
 *
 * @retval "0": success
 * @retval "!0": failure
 *
 * @callgraph
 * @callergraph
 * @design
 */
int32_t ipc_shm_release_buf(int32_t instance, int32_t chan_id, const void *buf)
{
	struct ipc_shm_channel *chan;
	struct ipc_shm_pool *pool;
	struct ipc_shm_bd bd;
	int32_t err;

	/* check if instance is valid */
	if (ipc_instance_is_free(instance) != IPC_SHM_INSTANCE_USED) {
		ipc_err("instance %d is unused\n", instance);

		return -EINVAL;
	}

	chan = get_channel(instance, chan_id);
	if ((chan == NULL) || (buf == NULL)) {
		ipc_err("instance %d invlaid parameter\n", instance);

		return -EINVAL;
	}

	/* Find the pool that owns the buffer */
	bd.pool_id = find_pool_for_buf(chan, (uint64_t) buf, 1);
	if (bd.pool_id == -EINVAL) {
		ipc_err("Buffer address %p doesn't belong to channel %d\n",
				buf, chan_id);

		return -EINVAL;
	}

	pool = &chan->pools[bd.pool_id];
	bd.buf_id = (uint16_t)(((uint64_t)buf - pool->remote_pool_addr) /
			pool->buf_size);
	bd.data_size = 0; /* reset size of written data in buffer */

	err = ipc_queue_push(&pool->bd_queue, &bd);
	if (err != 0) {
		ipc_err("Unable to release buffer %d from pool %d from channel %d with address %p :%d\n",
				bd.buf_id, bd.pool_id, chan_id, buf, err);

		return err;
	}

	ipc_dbg("ch %d: pool %d: released buffer %d with address %p\n",
			chan_id, bd.pool_id, bd.buf_id, buf);

	return 0;
}

/**
 * @NO{S17E09C06U}
 * @ASIL{B}
 * @brief send data on given channel and notify remote
 *
 * @param[in] instance: ipc instance id
 * @param[in] chan_id: channel index
 * @param[in] buf: buffer pointer
 * @param[in] size: size of data written in buffer
 *
 * @retval "0": success
 * @retval "!0": failure
 *
 * @callgraph
 * @callergraph
 * @design
 */
int32_t ipc_shm_tx(int32_t instance, int32_t chan_id, void *buf, size_t size)
{
	struct ipc_shm_channel *chan;
	struct ipc_shm_pool *pool;
	struct ipc_shm_bd bd;
	int32_t err;
	int32_t mask;

	mask = chan_id << (4 * instance + 1);

	/* check if instance is used */
	if (ipc_instance_is_free(instance) != IPC_SHM_INSTANCE_USED) {
		ipc_err("instance %d is unused\n", instance);

		return -EINVAL;
	}

	writel(mask, ipc_shm_priv_data[instance].ipc_shm_mask);

	chan = get_channel(instance, chan_id);
	if ((chan == NULL) || (buf == NULL) || (size == 0u)) {
		ipc_err("instance %d invalid parameter\n", instance);

		return -EINVAL;
	}

	/* Find the pool that owns the buffer */
	bd.pool_id = find_pool_for_buf(chan, (uint64_t) buf, 0);
	if (bd.pool_id == -EINVAL) {
		ipc_err("Buffer address %p doesn't belong to channel %d\n",
				buf, chan_id);

		return -EINVAL;
	}

	pool = &chan->pools[bd.pool_id];
	bd.buf_id = (uint16_t) (((uint64_t) buf - pool->local_pool_addr)
			/ pool->buf_size);
	bd.data_size = (uint32_t) size;

	/* push buffer descriptor in queue */
	err = ipc_queue_push(&chan->bd_queue, &bd);
	if (err != 0) {
		ipc_err("Unable to push buffer descriptor in channel queue: %d\n", err);

		return err;
	}

	/* notify remote that data is available */
	err = ipc_os_mbox_notify(instance);
	if (err < 0) {
		ipc_err("send notify failed\n");

		return err;
	}

	return 0;
}

/**
 * @NO{S17E09C06U}
 * @ASIL{B}
 * @brief check whether remote is initialized
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
int32_t ipc_shm_is_remote_ready(int32_t instance)
{
	struct ipc_shm_global *remote_global;

	/* check if instance is used */
	if (ipc_instance_is_free(instance) != IPC_SHM_INSTANCE_USED) {
		ipc_err("instance %d is unused\n", instance);

		return -EINVAL;
	}

	/* global data of remote at beginning of remote shared memory */
	remote_global = (struct ipc_shm_global *)ipc_os_get_remote_shm(
			instance);

	if (remote_global->state != IPC_SHM_STATE_READY) {
		ipc_dbg("instance %d no ready\n", instance);

		return -EAGAIN;
	}

	return 0;
}

/**
 * @NO{S17E09C06U}
 * @ASIL{B}
 * @brief poll the channels for available messages to process
 *
 * @param[in] instance: ipc instance id
 *
 * @retval ">=0": number of messages processed
 * @retval "<0": error code
 *
 * @callgraph
 * @callergraph
 * @design
 */
int32_t ipc_shm_poll_channels(int32_t instance)
{
	struct ipc_shm_global *remote_global;

	/* check if instance is used */
	if (ipc_instance_is_free(instance) != IPC_SHM_INSTANCE_USED) {
		ipc_err("instance %d is unused\n", instance);

		return -EINVAL;
	}

	/* global data of remote at beginning of remote shared memory */
	remote_global = (struct ipc_shm_global *)ipc_os_get_remote_shm(
			instance);

	/* check if remote is ready before polling */
	if (remote_global->state != IPC_SHM_STATE_READY) {
		ipc_err("instance %d no ready\n", instance);

		return -EAGAIN;
	}

	return ipc_os_poll_channels(instance);
}
