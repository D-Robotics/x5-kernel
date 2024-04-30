/*************************************************************************
 *                     COPYRIGHT NOTICE
 *            Copyright 2021 Horizon Robotics, Inc.
 *                   All rights reserved.
 *************************************************************************/

/**
 * @file hb_mbox.c
 *
 * @NO{S17E09C06U}
 * @ASIL{B}
 */

#include "mbox_os.h"
#include "hb_mbox.h"

static int32_t check_param_open(uint8_t ipcm_id, uint8_t chan_id, struct user_chan_cfg *ucfg)
{
	if (ipcm_id >= NUM_IPCM ||
	    chan_id >= NUM_CHAN_PER_IPCM) {
		mbox_err("ipcm_id %d chan_id %d is invalid\n", ipcm_id, chan_id);

		return -EINVAL;
	}

	if (!ucfg) {
		mbox_err("ipcm_id %d chan_id %d ucfg is NULL\n", ipcm_id, chan_id);

		return -EINVAL;
	}

	if (!ucfg->rx_cb) {
		mbox_err("ipcm_id %d chan_id %d rx_cb is NULL\n", ipcm_id, chan_id);

		return -EINVAL;
	}

	if (ucfg->trans_flags >= BIT(MAX_TRANS_FLAGS_BIT)) {
		mbox_err("ipcm_id %d chan_id %d trans_flags %#x is invalid\n",
			  ipcm_id, chan_id, ucfg->trans_flags);

		return -EINVAL;
	}

	if (ucfg->cdev->send_mbox_id >= NUM_MBOX_PER_IPCM ||
	    ucfg->cdev->recv_mbox_id >= NUM_MBOX_PER_IPCM ||
	    ucfg->cdev->local_id >= NUM_INT_PER_IPCM ||
	    ucfg->cdev->remote_id >= NUM_INT_PER_IPCM ||
	    ucfg->cdev->local_ack_link_mode > MBOX_ACK_LINK_DEFAULT) {
		mbox_err("ipcm_id %d chan_id %d cdev is invalid\n", ipcm_id, chan_id);

		return -EINVAL;
	}

	return 0;
}

int32_t hb_mbox_open_chan(uint8_t ipcm_id, uint8_t chan_id, struct user_chan_cfg *ucfg)
{
	int32_t ret;

	ret = check_param_open(ipcm_id, chan_id, ucfg);
	if (ret < 0) {
		mbox_err("ipcm_id %d chan_id %d open paramter is invalid\n",
			  ipcm_id, chan_id);

		return ret;
	}

	ret = os_mbox_open_chan(ipcm_id, chan_id, ucfg);
	if (ret < 0) {
		mbox_err("ipcm_id %d chan_id %d busy: %d\n",
			  ipcm_id, chan_id, ret);
		return ret;
	}

	return 0;
}

int32_t hb_mbox_close_chan(uint8_t ipcm_id, uint8_t chan_id)
{
	int32_t ret;

	if (ipcm_id >= NUM_IPCM ||
	    chan_id >= NUM_CHAN_PER_IPCM) {
		mbox_err("ipcm_id %d chan_id %d is invalid\n", ipcm_id, chan_id);

		return -EINVAL;
	}

	ret = os_mbox_close_chan(ipcm_id, chan_id);
	if (ret < 0) {
		mbox_err("ipcm_id %d chan_id %d close failed: %d\n", ipcm_id, chan_id, ret);

		return ret;
	}

	return 0;
}

int32_t hb_mbox_send(uint8_t ipcm_id, uint8_t chan_id, void *data)
{
	int32_t ret = 0;

	if (ipcm_id >= NUM_IPCM ||
	    chan_id >= NUM_CHAN_PER_IPCM) {
		mbox_err("ipcm_id %d chan_id %d is invalid\n", ipcm_id, chan_id);

		return -EINVAL;
	}

	if (!data) {
		mbox_err("ipcm_id %d chan_id %d data is invalid\n",
			  ipcm_id, chan_id);

		return -EINVAL;
	}

	ret = os_mbox_send(ipcm_id, chan_id, data);
	if (ret < 0) {
		mbox_err("ipcm_id %d chan_id %d send failed: %d\n",
			  ipcm_id, chan_id, ret);

		return ret;
	}

	return 0;
}

int32_t hb_mbox_flush(uint8_t ipcm_id, uint8_t chan_id, uint32_t timeout)
{
	int32_t ret = 0;

	if (ipcm_id >= NUM_IPCM ||
	    chan_id >= NUM_CHAN_PER_IPCM ||
	    !timeout) {
		mbox_err("ipcm_id %d chan_id %d timeout %d is invalid\n",
			  ipcm_id, chan_id, timeout);

		return -EINVAL;
	}
	ret = os_mbox_flush(ipcm_id, chan_id, timeout);
	if (ret < 0) {
		mbox_err("ipcm_id %d chan_id %d send timeout: %d\n",
			  ipcm_id, chan_id, ret);

		return ret;
	}

	return 0;
}

int32_t hb_mbox_peek_data(uint8_t ipcm_id, uint8_t chan_id)
{
	int32_t ret = 0;

	if (ipcm_id >= NUM_IPCM ||
	    chan_id >= NUM_CHAN_PER_IPCM) {
		mbox_err("ipcm_id %d chan_id %d is invalid\n", ipcm_id, chan_id);

		return -EINVAL;
	}

#if (LOCAL_INT_ENABLE)
	mbox_dbg("ipcm_id %d enable local interrupt, no support peek_data API\n", ipcm_id);

	return 0;
#endif
	ret = os_mbox_peek_data(ipcm_id, chan_id);
	if (ret < 0) {
		mbox_err("ipcm_id %d chan_id %d peek data fault: %d\n",
			  ipcm_id, chan_id, ret);
	}

	mbox_dbg("ipcm_id %d chan_id %d peek data result: %d\n", ipcm_id, chan_id, ret);

	return ret;
}

int32_t hb_mbox_init(void)
{
	int32_t ret;

	ret = os_mbox_init();
	if (ret < 0) {
		mbox_err("mbox init failed: %d\n", ret);

		return ret;
	}

	return 0;
}

void hb_mbox_deinit(void)
{
	(void)os_mbox_deinit();
}
