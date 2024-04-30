/*************************************************************************
 *                     COPYRIGHT NOTICE
 *            Copyright 2021-2023 Horizon Robotics, Inc.
 *                   All rights reserved.
 *************************************************************************/

/**
 * @file hb_ipc.c
 *
 * @NO{S17E09C06U}
 * @ASIL{B}
 */

#include "ipc_os.h"

static int32_t def_mode_check_param(const struct ipc_instance_info_m0 *cfg)
{
	if (cfg == NULL || cfg->recv_callback == NULL) {
		ipc_err("cfg is invalid parameter\n");

		return -EINVAL;
	}

	return 0;
}

static int32_t custom_mode_check_param(const struct ipc_instance_info_m1 *cfg)
{
	struct ipc_channel_info *chan;
	struct ipc_pool_info *pool;
	int32_t i, j;

	if (cfg == NULL) {
		ipc_err("cfg is NULL\n");

		return -EINVAL;
	}

	if ((cfg->local_shm_addr == (uint64_t) NULL) ||
	    (cfg->remote_shm_addr == (uint64_t) NULL)) {
		ipc_err("NULL local or remote address\n");

		return -EINVAL;
	}

	if (cfg->num_chans <= 0 || cfg->num_chans > MAX_NUM_CHAN_PER_INSTANCE) {
		ipc_err("num_chans %d invalid\n", cfg->num_chans);

		return -EINVAL;
	}

	if (cfg->chans == NULL) {
		ipc_err("chans is NULL\n");

		return -EINVAL;
	}

	ipc_dbg("local_shm_addr %#llx shm_size %d num_chans %d\n", cfg->local_shm_addr,cfg->shm_size,cfg->num_chans );

	for (i = 0; i < cfg->num_chans; i++) {
		chan = cfg->chans + i;
		ipc_dbg("num_pools %d\n", chan->num_pools);
		if (chan->num_pools <= 0 || chan->num_pools > MAX_NUM_POOL_PER_CHAN) {
			ipc_err("num_pools %d invalid\n", chan->num_pools);

			return -EINVAL;
		}

		if (chan->pools == NULL) {
			ipc_err("pools is NULL\n");

			return -EINVAL;
		}

		if (chan->recv_callback == NULL) {
			ipc_err("recv_callback is NULL\n");

			return -EINVAL;
		}

		for (j = 0; j < chan->num_pools; j++) {
			pool = chan->pools + j;
			ipc_dbg("num_bufs %d\n", pool->num_bufs);
			if (pool->num_bufs == 0 || pool->num_bufs > MAX_NUM_BUF_PER_POOL) {
				ipc_err("num_bufs %d invalid\n", pool->num_bufs);

				return -EINVAL;
			}
			if (pool->buf_size == 0) {
				ipc_err("buf_size %d invalid\n", pool->buf_size);

				return -EINVAL;
			}
		}
	}

	return 0;
}

static int32_t open_check_param(int32_t instance, const struct ipc_instance_cfg *cfg)
{
	int32_t err;

	if (cfg == NULL) {
		ipc_err("cfg is NULL\n");

		return -EINVAL;
	}
	ipc_dbg("instance %d mode %d timeout %d tflags %d mboxidx %d\n",
		instance, cfg->mode, cfg->timeout, cfg->trans_flags, cfg->mbox_chan_idx);
	if (instance < 0 || instance >= MAX_NUM_INSTANCE) {
		ipc_err("instance %d invalid\n", instance);

		return -EINVAL;
	}

	if (cfg->timeout < 0 || cfg->timeout > MAX_TIMEOUT) {
		ipc_err("timeout %d invalid\n", cfg->timeout);

		return -EINVAL;
	}

	if (cfg->mbox_chan_idx < IPC_MBOX_NONE || cfg->mbox_chan_idx > MAX_MBOX_IDX) {
		ipc_err("mbox_chan_idx %d invalid\n", cfg->mbox_chan_idx);

		return -EINVAL;
	}

	if (cfg->trans_flags < 0 || cfg->trans_flags > MAX_TRANS_FLAG) {
		ipc_err("trans flags %d invalid\n", cfg->trans_flags);

		return -EINVAL;
	}

	if (cfg->mode < 0 || cfg->mode >= NUM_WORK_MODE) {
		ipc_err("work mode %d invalid\n", cfg->mode);

		return -EINVAL;
	}

	if (cfg->mode == DEFAULT_MODE) {
		if (cfg->mbox_chan_idx == IPC_MBOX_NONE) {
			ipc_err("In default mode, mailbox must be used\n");

			return -EINVAL;
		}

		err = def_mode_check_param(&cfg->info.def_cfg);
		if (err != 0) {
			ipc_err("default mode invalid parameter\n");

			return -EINVAL;
		}
	} else if (cfg->mode == CUSTOM_MODE) {
		err = custom_mode_check_param(&cfg->info.custom_cfg);
		if (err != 0) {
			ipc_err("custom mode invalid parameter\n");

			return -EINVAL;
		}
	}

	return 0;
}

int32_t hb_ipc_open_instance(int32_t instance, const struct ipc_instance_cfg *cfg)
{
	int32_t err;

	err = open_check_param(instance, cfg);
	if (err != 0) {
		ipc_err("instance %d invalid param: %d\n", instance, err);

		return err;
	}

	err = ipc_shm_init_instance_cfg(instance, cfg);
	if (err != 0) {
		ipc_err("instance %d init failed: %d\n", instance, err);

		return err;
	}

	ipc_dbg("instance %d open success\n", instance);

	return 0;
}

int32_t hb_ipc_close_instance(int32_t instance)
{
	if (instance < 0 || instance >= MAX_NUM_INSTANCE) {
		ipc_err("instance %d invalid\n", instance);

		return -EINVAL;
	}

	ipc_shm_free_instance(instance);
	ipc_dbg("instance %d close success\n", instance);

	return 0;
}

int32_t  hb_ipc_acquire_buf(int32_t instance, int32_t chan_id, uint64_t size, uint8_t** buf)
{
	if (instance < 0 || instance >= MAX_NUM_INSTANCE ||
	    chan_id < 0 || chan_id > MAX_NUM_CHAN_PER_INSTANCE) {
		ipc_err("instance %d channel %d invalid\n", instance, chan_id);

		return -EINVAL;
	}

	if (buf == NULL) {
		ipc_err("buf is NULL\n");

		return -EINVAL;
	}

	if (size == 0) {
		ipc_err("size %#llx is invalid\n", size);

		return -EINVAL;
	}

	*buf = (uint8_t *)ipc_shm_acquire_buf(instance, chan_id, size);
	if (*buf == NULL) {
		ipc_err("buf acquire failed\n");

		return -ENOMEM;
	}

	ipc_dbg("instance %d channel %d acquire buffer address: 0x%px\n", instance, chan_id, *buf);

	return 0;
}

int32_t hb_ipc_release_buf(int32_t instance, int32_t chan_id, const uint8_t *buf)
{
	int32_t err;

	if (instance < 0 || instance >= MAX_NUM_INSTANCE ||
	    chan_id < 0 || chan_id > MAX_NUM_CHAN_PER_INSTANCE) {
		ipc_err("instance %d channel %d invalid\n", instance, chan_id);

		return -EINVAL;
	}

	if (buf == NULL) {
		ipc_err("buf is NULL\n");

		return -EINVAL;
	}

	err = ipc_shm_release_buf(instance, chan_id, buf);

	if (err != 0) {
		ipc_err("instance %d channel %d buffer release failed: %d\n",
			 instance, chan_id, err);

		return err;
	}

	ipc_dbg("instance %d channel %d buffer release success\n", instance, chan_id);

	return 0;
}

int32_t hb_ipc_send(int32_t instance, int32_t chan_id, uint8_t *buf, uint64_t size)
{
	int32_t err;

	if (instance < 0 || instance >= MAX_NUM_INSTANCE ||
	    chan_id < 0 || chan_id > MAX_NUM_CHAN_PER_INSTANCE) {
		ipc_err("instance %d channel %d invalid\n", instance, chan_id);

		return -EINVAL;
	}

	if (buf == NULL) {
		ipc_err("buf is NULL\n");

		return -EINVAL;
	}

	err = ipc_shm_tx(instance, chan_id, buf, size);
	if (err != 0) {
		ipc_err("instance %d channel %d send message failed: %d\n",
			 instance, chan_id, err);

		return err;
	}

	ipc_dbg("instance %d channel %d send message success\n", instance, chan_id);

	return 0;
}

int32_t hb_ipc_is_remote_ready(int32_t instance)
{
	int32_t err;

	if (instance < 0 || instance >= MAX_NUM_INSTANCE) {
		ipc_err("instance %d invalid\n", instance);

		return -EINVAL;
	}

	err = ipc_shm_is_remote_ready(instance);
	if (err == -EAGAIN) {
		ipc_dbg("instance %d no ready\n", instance);
	} else if (err != 0) {
		ipc_err("instance %d is invalid\n", instance);
	} else {
		ipc_dbg("instance %d is ready\n", instance);
	}

	return err;
}

int32_t hb_ipc_poll_instance(int32_t instance)
{
	int32_t count;

	if (instance < 0 || instance >= MAX_NUM_INSTANCE) {
		ipc_err("instance %d invalid\n", instance);

		return -EINVAL;
	}

	count = ipc_shm_poll_channels(instance);

	if (count == 0) {
		ipc_dbg("instance %d no message\n", instance);
	} else if (count > 0) {
		ipc_dbg("instance %d receive messge: %d\n", instance, count);
	}

	return count;
}
