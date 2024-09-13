/***************************************************************************
 *                      COPYRIGHT NOTICE
 *             Copyright (c) 2019 - 2023 Horizon Robotics, Inc.
 *                     All rights reserved.
 ***************************************************************************/
#include <linux/clk.h>
#include <linux/pm_runtime.h>

#include "hobot_vpu_pm.h"
#include "hobot_vpu_debug.h"

int32_t hb_vpu_clk_get(hb_vpu_dev_t *dev, uint64_t freq)
{
	uint64_t rate = 0, round_rate = 0;
	int32_t ret = 0;

	if (!dev) {
		return -1;
	}

	if (freq <= 0 || freq > MAX_VPU_FREQ) {
		freq = MAX_VPU_FREQ;
	}

	/* 1. vpu pclk */
	dev->vpu_pclk = devm_clk_get(dev->device, VPU_PCLK_NAME);
	if ((!dev->vpu_pclk) || clk_prepare_enable(dev->vpu_pclk)) {
		VPU_ERR_DEV(dev->device, "failed to get clk(%s).\n", VPU_PCLK_NAME);
		dev->vpu_pclk = NULL;
		dev->vpu_aclk = NULL;
		dev->vpu_bclk = NULL;
		dev->vpu_cclk = NULL;
		return -1;
	}

	rate = clk_get_rate(dev->vpu_pclk);
	VPU_DBG_DEV(dev->device, "%s clock is %llu\n", VPU_PCLK_NAME, rate);

	/* 2. vpu aclk */
	dev->vpu_aclk = devm_clk_get(dev->device, VPU_ACLK_NAME);
	if ((!dev->vpu_aclk) || clk_prepare_enable(dev->vpu_aclk)) {
		VPU_ERR_DEV(dev->device, "failed to get clk(%s).\n", VPU_ACLK_NAME);
		clk_disable_unprepare(dev->vpu_pclk);
		dev->vpu_pclk = NULL;
		dev->vpu_aclk = NULL;
		dev->vpu_bclk = NULL;
		dev->vpu_cclk = NULL;
		return -1;
	}

	rate = clk_get_rate(dev->vpu_aclk);
	VPU_DBG_DEV(dev->device, "%s clock is %llu\n", VPU_ACLK_NAME, rate);

	/* 3. vpu bclk */
	dev->vpu_bclk = devm_clk_get(dev->device, VPU_VCPU_BPU_CLK_NAME);
	if (dev->vpu_bclk) {
		rate = clk_get_rate(dev->vpu_bclk);
		if (rate != freq) {
			round_rate = clk_round_rate(dev->vpu_bclk, freq);
			ret = clk_set_rate(dev->vpu_bclk, round_rate);
			if (ret) {
				VPU_ERR_DEV(dev->device, "failed to set clk(%s).\n", VPU_VCPU_BPU_CLK_NAME);
				clk_disable_unprepare(dev->vpu_pclk);
				clk_disable_unprepare(dev->vpu_aclk);
				dev->vpu_pclk = NULL;
				dev->vpu_aclk = NULL;
				dev->vpu_bclk = NULL;
				dev->vpu_cclk = NULL;
				return -1;
			}
		}
	}
	if ((!dev->vpu_bclk) || clk_prepare_enable(dev->vpu_bclk)) {
		VPU_ERR_DEV(dev->device, "failed to get clk(%s).\n", VPU_VCPU_BPU_CLK_NAME);
		clk_disable_unprepare(dev->vpu_pclk);
		clk_disable_unprepare(dev->vpu_aclk);
		dev->vpu_pclk = NULL;
		dev->vpu_aclk = NULL;
		dev->vpu_bclk = NULL;
		dev->vpu_cclk = NULL;
		return -1;
	}

	rate = clk_get_rate(dev->vpu_bclk);
	VPU_DBG_DEV(dev->device, "%s clock is %llu\n", VPU_VCPU_BPU_CLK_NAME, rate);

	/* 4. vpu cclk */
	dev->vpu_cclk = devm_clk_get(dev->device, VPU_VCE_CLK_NAME);
	if (dev->vpu_cclk) {
		rate = clk_get_rate(dev->vpu_cclk);
		if (rate != freq) {
			round_rate = clk_round_rate(dev->vpu_cclk, freq);
			ret = clk_set_rate(dev->vpu_cclk, round_rate);
			if (ret) {
				VPU_ERR_DEV(dev->device, "failed to set clk(%s).\n", VPU_VCE_CLK_NAME);
				clk_disable_unprepare(dev->vpu_pclk);
				clk_disable_unprepare(dev->vpu_aclk);
				clk_disable_unprepare(dev->vpu_bclk);
				dev->vpu_pclk = NULL;
				dev->vpu_aclk = NULL;
				dev->vpu_bclk = NULL;
				dev->vpu_cclk = NULL;
				return -1;
			}
		}
	}
	if ((!dev->vpu_cclk) || clk_prepare_enable(dev->vpu_cclk)) {
		VPU_ERR_DEV(dev->device, "failed to get clk(%s).\n", VPU_VCE_CLK_NAME);
		clk_disable_unprepare(dev->vpu_pclk);
		clk_disable_unprepare(dev->vpu_aclk);
		clk_disable_unprepare(dev->vpu_bclk);
		dev->vpu_pclk = NULL;
		dev->vpu_aclk = NULL;
		dev->vpu_bclk = NULL;
		dev->vpu_cclk = NULL;
		return -1;
	}

	rate = clk_get_rate(dev->vpu_cclk);
	VPU_DBG_DEV(dev->device, "%s clock is %llu\n", VPU_VCE_CLK_NAME, rate);

	return 0;
}

void hb_vpu_clk_put(const hb_vpu_dev_t *dev)
{
	if (!dev)
		return;

	clk_disable_unprepare(dev->vpu_pclk);
	clk_disable_unprepare(dev->vpu_aclk);
	clk_disable_unprepare(dev->vpu_bclk);
	clk_disable_unprepare(dev->vpu_cclk);
}

int32_t hb_vpu_clk_enable(const hb_vpu_dev_t *dev, uint64_t freq)
{
	uint64_t rate = 0, round_rate = 0;
	int32_t ret = 0;

	if (!dev)
		return -1;

	if (freq <= 0 || freq > MAX_VPU_FREQ) {
		freq = MAX_VPU_FREQ;
	}

	/* 1. vpu pclk */
	ret = clk_prepare_enable(dev->vpu_pclk);
	if (ret) {
		VPU_ERR_DEV(dev->device, "failed to enable clk(%s).\n", VPU_PCLK_NAME);
		return -1;
	}

	/* 2. vpu aclk */
	ret = clk_prepare_enable(dev->vpu_aclk);
	if (ret) {
		VPU_ERR_DEV(dev->device, "failed to enable clk(%s).\n", VPU_ACLK_NAME);
		return -1;
	}

	/* 3. vpu bclk */
	rate = clk_get_rate(dev->vpu_bclk);
	if (rate != freq) {
		round_rate = clk_round_rate(dev->vpu_bclk, freq);
		ret = clk_set_rate(dev->vpu_bclk, round_rate);
		if (ret) {
			VPU_ERR_DEV(dev->device, "failed to set clk(%s).\n", VPU_VCPU_BPU_CLK_NAME);
			clk_disable_unprepare(dev->vpu_aclk);
			return -1;
		}
		VPU_DBG_DEV(dev->device, "%s clock is %llu\n", VPU_VCPU_BPU_CLK_NAME, round_rate);
	}

	ret = clk_prepare_enable(dev->vpu_bclk);
	if (ret) {
		clk_disable_unprepare(dev->vpu_aclk);
		VPU_ERR_DEV(dev->device, "failed to enable clk(%s).\n", VPU_VCPU_BPU_CLK_NAME);
		return -1;
	}

	/* 4. vpu cclk */
	rate = clk_get_rate(dev->vpu_cclk);
	if (rate != freq) {
		round_rate = clk_round_rate(dev->vpu_bclk, freq);
		ret = clk_set_rate(dev->vpu_cclk, round_rate);
		if (ret) {
			VPU_ERR_DEV(dev->device, "failed to set clk(%s).\n", VPU_VCE_CLK_NAME);
			clk_disable_unprepare(dev->vpu_aclk);
			clk_disable_unprepare(dev->vpu_bclk);
			return -1;
		}
		VPU_DBG_DEV(dev->device, "%s clock is %llu\n", VPU_VCE_CLK_NAME, round_rate);
	}
	ret = clk_prepare_enable(dev->vpu_cclk);
	if (ret) {
		clk_disable_unprepare(dev->vpu_aclk);
		clk_disable_unprepare(dev->vpu_bclk);
		VPU_ERR_DEV(dev->device, "failed to enable clk(%s).\n", VPU_VCE_CLK_NAME);
		return -1;
	}

	return 0;
}

int32_t hb_vpu_clk_disable(const hb_vpu_dev_t *dev)
{
	if (!dev)
		return -EINVAL;

	clk_disable_unprepare(dev->vpu_pclk);
	clk_disable_unprepare(dev->vpu_aclk);
	clk_disable_unprepare(dev->vpu_bclk);
	clk_disable_unprepare(dev->vpu_cclk);

	return 0;
}

int32_t hb_vpu_pm_ctrl(hb_vpu_dev_t *dev, uint16_t init, uint16_t ops)
{
	int32_t ret = 0;

	if (dev == NULL) {
		VPU_ERR("Invlid null device.\n");
		return -ENODEV;
	}

	if (ops > 0u) {
		if (init > 0u) {
			pm_runtime_set_autosuspend_delay(dev->device, 0);
			pm_runtime_enable(dev->device);
		}

		ret = pm_runtime_get_sync(dev->device);
		if (ret < 0) {
			pm_runtime_put_noidle(dev->device);
			VPU_ERR_DEV(dev->device, "pm runtime get sync failed\n");
			return ret;
		}
	} else {
		if (pm_runtime_active(dev->device))
			ret = pm_runtime_put_sync_suspend(dev->device);

		if (init > 0u)
			pm_runtime_disable(dev->device);
	}
	udelay(100);

	return ret;
}
