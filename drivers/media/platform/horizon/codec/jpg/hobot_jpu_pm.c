/***************************************************************************
 *                      COPYRIGHT NOTICE
 *             Copyright (c) 2019 - 2023 Horizon Robotics, Inc.
 *                     All rights reserved.
 ***************************************************************************/
#include <linux/clk.h>
#include <linux/pm_runtime.h>

#include "hobot_jpu_pm.h"
#include "hobot_jpu_debug.h"

int32_t hb_jpu_clk_get(hb_jpu_dev_t *dev, uint64_t freq)
{
	uint64_t rate = 0;

	if (!dev) {
		return -1;
	}

	dev->jpu_pclk = devm_clk_get(dev->device, JPU_JPEG_PCLK_NAME);
	if ((!dev->jpu_pclk) || clk_prepare_enable(dev->jpu_pclk)) {
		JPU_ERR_DEV(dev->device, "failed to get clk(%s).\n", JPU_JPEG_PCLK_NAME);
		dev->jpu_pclk = NULL;
		dev->jpu_aclk = NULL;
		dev->jpu_cclk = NULL;
		return -1;
	}
	rate = clk_get_rate(dev->jpu_pclk);
	JPU_INFO_DEV(dev->device, "%s clock is %llu\n", JPU_JPEG_PCLK_NAME, rate);

	dev->jpu_aclk = devm_clk_get(dev->device, JPU_JPEG_ACLK_NAME);
	if ((!dev->jpu_aclk) || clk_prepare_enable(dev->jpu_aclk)) {
		JPU_ERR_DEV(dev->device, "failed to get clk(%s).\n", JPU_JPEG_ACLK_NAME);
		clk_disable_unprepare(dev->jpu_pclk);
		dev->jpu_pclk = NULL;
		dev->jpu_aclk = NULL;
		dev->jpu_cclk = NULL;
		return -1;
	}

	rate = clk_get_rate(dev->jpu_aclk);
	JPU_INFO_DEV(dev->device, "%s clock is %llu\n", JPU_JPEG_ACLK_NAME, rate);

	dev->jpu_cclk = devm_clk_get(dev->device, JPU_JPEG_CCLK_NAME);
	if ((!dev->jpu_cclk) || clk_prepare_enable(dev->jpu_cclk)) {
		JPU_ERR_DEV(dev->device, "failed to get clk(%s).\n", JPU_JPEG_CCLK_NAME);
		clk_disable_unprepare(dev->jpu_pclk);
		clk_disable_unprepare(dev->jpu_aclk);
		dev->jpu_pclk = NULL;
		dev->jpu_aclk = NULL;
		dev->jpu_cclk = NULL;
		return -1;
	}

	rate = clk_get_rate(dev->jpu_cclk);
	JPU_INFO_DEV(dev->device, "%s clock is %llu\n", JPU_JPEG_CCLK_NAME, rate);

	return 0;
}

void hb_jpu_clk_put(const hb_jpu_dev_t *dev)
{
	if (!dev)
		return;

	clk_disable_unprepare(dev->jpu_pclk);
	clk_disable_unprepare(dev->jpu_aclk);
	clk_disable_unprepare(dev->jpu_cclk);
}

int32_t hb_jpu_clk_enable(const hb_jpu_dev_t *dev, uint64_t freq)
{
	int32_t ret = 0;

	if (!dev)
		return -1;

	ret = clk_prepare_enable(dev->jpu_pclk);
	if (ret) {
		JPU_ERR_DEV(dev->device, "failed to enable clk(%s).\n", JPU_JPEG_PCLK_NAME);
		return -1;
	}

	ret = clk_prepare_enable(dev->jpu_aclk);
	if (ret) {
		clk_disable_unprepare(dev->jpu_pclk);
		JPU_ERR_DEV(dev->device, "failed to enable clk(%s).\n", JPU_JPEG_ACLK_NAME);
		return -1;
	}

	ret = clk_prepare_enable(dev->jpu_cclk);
	if (ret) {
		clk_disable_unprepare(dev->jpu_pclk);
		clk_disable_unprepare(dev->jpu_aclk);
		JPU_ERR_DEV(dev->device, "failed to enable clk(%s).\n", JPU_JPEG_CCLK_NAME);
		return -1;
	}

	return 0;
}

int32_t hb_jpu_clk_disable(const hb_jpu_dev_t *dev)
{
	if (!dev)
		return -EINVAL;

	clk_disable_unprepare(dev->jpu_pclk);
	clk_disable_unprepare(dev->jpu_aclk);
	clk_disable_unprepare(dev->jpu_cclk);

	return 0;
}

int32_t hb_jpu_pm_ctrl(hb_jpu_dev_t *dev, uint16_t init, uint16_t ops)
{
	int32_t ret = 0;

	if (dev == NULL) {
		JPU_ERR("Invlid null device.\n");
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
			JPU_ERR_DEV(dev->device, "pm runtime get sync failed\n");
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
