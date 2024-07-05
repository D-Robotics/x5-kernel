// SPDX-License-Identifier: GPL-2.0

#include <linux/module.h>
#include <linux/device.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/devfreq.h>
#include <linux/pm_opp.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/arm-smccc.h>
#include <linux/devfreq-event.h>
#include <linux/notifier.h>

#include "x5-smcc.h"

/* Hardware limitation */
#define X5_DDRC_MAX_FREQ_COUNT 4

extern struct blocking_notifier_head axi_monitor_chain_head;

/*
 * This should be in a 1:1 mapping with devicetree OPPs but
 * firmware provides additional info.
 */
struct x5_ddrc_freq {
	unsigned long rate;
	unsigned long smcarg;
};

struct x5_ddrc {
	struct devfreq_dev_profile profile;
	struct devfreq_simple_ondemand_data ondemand_data;
	struct device *dev;
	struct devfreq_event_dev *edev;
	struct devfreq *devfreq;
	unsigned long rate, target_rate;
	bool userspace_gov;
	int freq_count;
	struct x5_ddrc_freq freq_table[X5_DDRC_MAX_FREQ_COUNT];
};

static struct x5_ddrc_freq *x5_ddrc_find_freq(struct x5_ddrc *priv,
						    unsigned long rate)
{
	struct x5_ddrc_freq *freq;
	int i;

	/*
	 * Firmware reports values in MT/s, so we round-down from Hz
	 * Rounding is extra generous to ensure a match.
	 */
	for (i = 0; i < priv->freq_count; ++i) {
		freq = &priv->freq_table[i];
		if (freq->rate == rate ||
				freq->rate + 1 == rate ||
				freq->rate - 1 == rate)
			return freq;
	}

	return NULL;
}

static void x5_ddrc_smc_set_freq(struct device *dev, unsigned long target_freq)
{
	ktime_t start, end;
	int elapsed_time;
	struct arm_smccc_res res;
	u32 online_cpus = 0;
	int cpu = 0;

	local_irq_disable();

	for_each_online_cpu(cpu) {
		online_cpus |= (1 << (cpu));
	}

	start = ktime_get();
	arm_smccc_smc(HORIZON_SIP_DDR_DVFS_SET, target_freq/1000000, online_cpus, 0, 0, 0, 0, 0, &res);
	end = ktime_get();
	elapsed_time = ktime_to_ns(ktime_sub(end, start));

	local_irq_enable();
	dev_dbg(dev, "dvfs elapsed time: %d ns to : %ld\n", elapsed_time, target_freq);
}

static int x5_ddrc_set_freq(struct device *dev, unsigned long target_rate)
{
	x5_ddrc_smc_set_freq(dev, target_rate);
	return 0;
}

static int x5_ddrc_target(struct device *dev, unsigned long *freq, u32 flags)
{
	struct x5_ddrc *priv = dev_get_drvdata(dev);
	struct dev_pm_opp *opp;
	unsigned long target_rate;
	int ret;

	opp = devfreq_recommended_opp(dev, freq, flags);
	if (IS_ERR(opp))
		return PTR_ERR(opp);

	target_rate = dev_pm_opp_get_freq(opp);
	dev_pm_opp_put(opp);

	if (priv->rate == target_rate)
		return 0;

	/*
	 * update for firmware
	 */
	ret = x5_ddrc_set_freq(dev, target_rate);
	if (ret < 0)
		return ret;

	priv->rate = target_rate;

	blocking_notifier_call_chain(&axi_monitor_chain_head, target_rate, NULL);

	return ret;
}

static int x5_dmcfreq_get_dev_status(struct device *dev,
					 struct devfreq_dev_status *stat)
{
	struct x5_ddrc *priv = dev_get_drvdata(dev);
	struct devfreq_event_data edata;
	int ret = 0;

	ret = devfreq_event_get_event(priv->edev, &edata);
	if (ret < 0)
		return ret;

	stat->current_frequency = priv->rate;
	stat->busy_time = edata.load_count;
	stat->total_time = edata.total_count;

	return ret;
}

static int x5_ddrc_get_initial_freq(struct device *dev, unsigned long *freq)
{
	struct arm_smccc_res res;

	/* get current ddr freq reported by firmware */
	arm_smccc_smc(HORIZON_SIP_DDR_DVFS_GET, 0, 0, 0, 0, 0, 0, 0, &res);
	*freq = res.a0 * 1000000;
	return 0;
}

static int x5_ddrc_get_cur_freq(struct device *dev, unsigned long *freq)
{
	struct x5_ddrc *priv = dev_get_drvdata(dev);
	*freq = priv->rate;
	return 0;
}

static int x5_ddrc_init_freq_info(struct device *dev)
{
	struct x5_ddrc *priv = dev_get_drvdata(dev);
	struct arm_smccc_res res;
	int index;

	/* An error here means DDR DVFS API not supported by firmware */
	arm_smccc_smc(HORIZON_SIP_DVFS_GET_FREQ_COUNT, 0, 0, 0, 0, 0, 0, 0, &res);
	priv->freq_count = res.a0;
	if (priv->freq_count <= 0 ||
			priv->freq_count > X5_DDRC_MAX_FREQ_COUNT)
		return -ENODEV;

	for (index = 0; index < priv->freq_count; ++index) {
		struct x5_ddrc_freq *freq = &priv->freq_table[index];

		arm_smccc_smc(HORIZON_SIP_DVFS_GET_FREQ_INFO, index,
			      0, 0, 0, 0, 0, 0, &res);
		/* Result should be strictly positive */
		if ((long)res.a0 <= 0)
			return -ENODEV;

		freq->rate = res.a0 * 1000000;
		freq->smcarg = index;
	}

	return 0;
}

static int x5_ddrc_check_opps(struct device *dev)
{
	struct x5_ddrc *priv = dev_get_drvdata(dev);
	struct x5_ddrc_freq *freq_info;
	struct dev_pm_opp *opp;
	unsigned long freq;
	int i, opp_count;

	/* Enumerate DT OPPs and disable those not supported by firmware */
	opp_count = dev_pm_opp_get_opp_count(dev);
	if (opp_count < 0)
		return opp_count;

	for (i = 0, freq = 0; i < opp_count; ++i, ++freq) {
		opp = dev_pm_opp_find_freq_ceil(dev, &freq);
		if (IS_ERR(opp)) {
			dev_err(dev, "Failed enumerating OPPs: %ld\n",
				PTR_ERR(opp));
			return PTR_ERR(opp);
		}
		dev_pm_opp_put(opp);

		freq_info = x5_ddrc_find_freq(priv, freq);
		if (!freq_info) {
			dev_dbg(dev, "Disable unsupported OPP %luHz %luMT/s\n",
					freq, freq/1000000);
			dev_pm_opp_disable(dev, freq);
		}
	}

	return 0;
}

static void x5_ddrc_exit(struct device *dev)
{
	return;
}

static __maybe_unused int x5_ddrc_suspend(struct device *dev)
{
	struct x5_ddrc *priv = dev_get_drvdata(dev);
	int ret = 0;

	if(priv->edev) {
		ret = devfreq_event_disable_edev(priv->edev);
		if (ret < 0) {
			dev_err(dev, "failed to disable the devfreq-event devices\n");
			return ret;
		}
	}

	ret = devfreq_suspend_device(priv->devfreq);
	if (ret < 0) {
		dev_err(dev, "failed to suspend the devfreq devices\n");
		return ret;
	}

	return 0;
}

static __maybe_unused int x5_ddrc_resume(struct device *dev)
{
	struct x5_ddrc *priv = dev_get_drvdata(dev);
	int ret = 0;

	if(priv->edev) {
		ret = devfreq_event_enable_edev(priv->edev);
		if (ret < 0) {
			dev_err(dev, "failed to enable the devfreq-event devices\n");
			return ret;
		}
	}

	ret = devfreq_resume_device(priv->devfreq);
	if (ret < 0) {
		dev_err(dev, "failed to resume the devfreq devices\n");
		return ret;
	}
	return ret;
}

static SIMPLE_DEV_PM_OPS(x5_ddrc_pm, x5_ddrc_suspend,
			 x5_ddrc_resume);

static int x5_ddrc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct x5_ddrc *priv;
	struct dev_pm_opp *opp;
	int ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dev = dev;
	platform_set_drvdata(pdev, priv);

	ret = x5_ddrc_init_freq_info(dev);
	if (ret) {
		dev_err(dev, "failed to init firmware freq info: %d\n", ret);
		return ret;
	}

	if (devm_pm_opp_of_add_table(dev)) {
		dev_err(dev, "Invalid operating-points in device tree.\n");
		ret = -EINVAL;
		goto err;
	}

	ret = x5_ddrc_check_opps(dev);
	if (ret < 0)
		goto err;

	priv->userspace_gov = device_property_read_bool(dev, "userspace-gov");
	dev_info(dev, "userspace_gov: %d\n", priv->userspace_gov);
	priv->edev = devfreq_event_get_edev_by_phandle(dev, "devfreq-events", 0);
	if (IS_ERR(priv->edev))
		return -EPROBE_DEFER;

	ret = devfreq_event_enable_edev(priv->edev);
	if (ret < 0) {
		dev_err(dev, "failed to enable devfreq-event devices\n");
		return ret;
	}

	device_property_read_u32(dev, "upthreshold",
		&priv->ondemand_data.upthreshold);
	device_property_read_u32(dev, "downdifferential",
		&priv->ondemand_data.downdifferential);

	dev_dbg(dev, "upthreshold: %d\n", priv->ondemand_data.upthreshold);
        dev_dbg(dev, "downdifferential: %d\n", priv->ondemand_data.downdifferential);

	x5_ddrc_get_initial_freq(dev, &priv->rate);

	opp = devfreq_recommended_opp(dev, &priv->rate, 0);
	if (IS_ERR(opp)) {
		ret = PTR_ERR(opp);
		goto err;
	}

	priv->rate = dev_pm_opp_get_freq(opp);

	dev_pm_opp_put(opp);

	device_property_read_u32(dev, "polling_ms",
		&priv->profile.polling_ms);
        dev_dbg(dev, "polling_ms: %d\n", priv->profile.polling_ms);

	priv->profile.target = x5_ddrc_target;
	priv->profile.get_dev_status	= x5_dmcfreq_get_dev_status,
	priv->profile.exit = x5_ddrc_exit;
	priv->profile.get_cur_freq = x5_ddrc_get_cur_freq;
	priv->profile.initial_freq = priv->rate;
	priv->profile.is_cooling_device = true;

	if (!priv->userspace_gov)
		priv->devfreq = devm_devfreq_add_device(dev, &priv->profile,
						DEVFREQ_GOV_SIMPLE_ONDEMAND,
						&priv->ondemand_data);
	else
		priv->devfreq = devm_devfreq_add_device(dev, &priv->profile,
						DEVFREQ_GOV_PERFORMANCE,
						&priv->ondemand_data);
	if (IS_ERR(priv->devfreq)) {
		ret = PTR_ERR(priv->devfreq);
		dev_err(dev, "failed to add devfreq device: %d\n", ret);
		goto err;
	}

	devm_devfreq_register_opp_notifier(dev, priv->devfreq);

	return 0;

err:
	return ret;
}

static const struct of_device_id x5_ddrc_of_match[] = {
	{ .compatible = "horizon,x5-ddrc", },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, x5_ddrc_of_match);

static struct platform_driver x5_ddrc_platdrv = {
	.probe		= x5_ddrc_probe,
	.driver = {
		.name	= "x5-ddrc-devfreq",
		.pm	= &x5_ddrc_pm,
		.of_match_table = x5_ddrc_of_match,
	},
};
module_platform_driver(x5_ddrc_platdrv);

MODULE_DESCRIPTION("Horizon X5 DDR Controller frequency driver");
MODULE_LICENSE("GPL v2");
