// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * drobot-pwm driver
 *
 * Maintainer: Dinggao Pan <dinggao.pan@horizon.cc>
 *
 * Copyright(C) 2024, D-Robotics Co., Ltd. All rights reserved
 *
 */

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>
#include <linux/delay.h>

struct drobot_pwm_chip {
	struct pwm_chip chip;
	struct clk *clk;
	void __iomem *base;
	struct reset_control	*reset;
};

#define PWM_MCR					(0x00)
#define PWM_CHANNEL0_EN				(0x01 << 0)
#define PWM_CHANNEL1_EN				(0x01 << 4)
#define PWM_CHANNEL_LOGIC			(0x3 << 2)

#define PWM_ISR					(0x04)
#define PWM_CHANNEL0_INTR			(0x01 << 0)
#define PWM_CHANNEL1_INTR			(0x01 << 4)
#define PWM_CHANNEL0_INTR_CLR			(0x01 << 3)
#define PWM_CHANNEL1_INTR_CLR			(0x01 << 7)

#define PWM_CTR(x)				(0x10 + (0x20 * (x)))
#define PWM_POLARITY				(0x01 << 0)
#define PWM_RESOLUT8				(0x01 << 4)

#define PWM_CCR(x)				(0x14 + (0x20 * (x)))
#define PWM_CLK_APB				(0x00 << 0)
#define PWM_CLK_PWM				(0x01 << 0)

#define PWM_PW16AR(x)				(0x18 + (0x20 * (x)))
#define PWM_PW8AR(x)				(0x1c + (0x20 * (x)))
#define PWM_PR(x)				(0x20 + (0x20 * (x)))
#define PWM_CR(x)				(0x24 + (0x20 * (x)))
#define PWM_SR(x)				(0x28 + (0x20 * (x)))

#define to_drobot_pwm_chip(_chip) container_of(_chip, struct drobot_pwm_chip, chip)

enum {
	DIV_BY_2,
	DIV_BY_4,
	DIV_BY_8,
	DIV_BY_16,
	DIV_BY_32,
	DIV_BY_64,
	DIV_BY_128,
	DIV_BY_256
} pwm_divisor_reg;

static const struct of_device_id drobot_pwm_dt_ids[] = {
	{ .compatible = "d-robotics,pwm", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, drobot_pwm_dt_ids);

static void drobot_pwm_enable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct drobot_pwm_chip *drobot_pwm = to_drobot_pwm_chip(chip);
	u32 val = 0;
	u8 channel = pwm->hwpwm;

	val = readl(drobot_pwm->base + PWM_MCR);
	if (channel == 0)
		val |= PWM_CHANNEL0_EN;
	else
		val |= PWM_CHANNEL1_EN;
	writel(val, drobot_pwm->base + PWM_MCR);
}

static void drobot_pwm_disable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct drobot_pwm_chip *drobot_pwm = to_drobot_pwm_chip(chip);
	u32 val = 0;
	u8 channel = pwm->hwpwm;

	val = readl(drobot_pwm->base + PWM_MCR);
	if (channel == 0)
		val &= ~PWM_CHANNEL0_EN;
	else
		val &= ~PWM_CHANNEL1_EN;
	writel(val, drobot_pwm->base + PWM_MCR);
}

static void drobot_pwm_duty_set(struct pwm_chip *chip, struct pwm_device *pwm,
			     const struct pwm_state *state, u8 div_reg, u16 prescale)
{
	u64 clk = 0;
	u16 duty = 0;
	u8 channel = pwm->hwpwm;
	u32 duty_cycles = 0;
	struct drobot_pwm_chip *drobot = to_drobot_pwm_chip(chip);

	clk = clk_get_rate(drobot->clk);

	duty_cycles = div64_u64(clk * state->duty_cycle,
		(unsigned long long)NSEC_PER_SEC);

	duty = (duty_cycles >> (div_reg + 1)) / prescale;

	if (duty == 0)
		dev_warn(chip->dev, "Warning! Duty is set as 0, no waveform!\n");

	/* set duty reg */
	writel(duty, drobot->base + PWM_PW16AR(channel));
}

static int drobot_pwm_apply(struct pwm_chip *chip, struct pwm_device *pwm,
			 const struct pwm_state *state)
{
	u64 clk = 0;
	u32 period_cycles = 0;
	u32 val = 0;
	u16 prd = 0;
	u16 prescale = 0;
	u8 clk_sel = 0;
	u8 div_reg = 0;
	u32 prd_prescale = 0; /* prd * prescale */
	u8 channel = pwm->hwpwm;
	struct drobot_pwm_chip *drobot = to_drobot_pwm_chip(chip);

	/* Clear FIFO in multichannel mode. */
	drobot_pwm_disable(chip, pwm);

	clk = clk_get_rate(drobot->clk);

	period_cycles = div64_u64(clk * state->period,
			(unsigned long long)NSEC_PER_SEC);

	while (div_reg <= 0x7) {
		prd_prescale = period_cycles >> (div_reg + 1);
		if (prd_prescale <= 0xffff * 256)
			break;
		div_reg++;
	}
	prescale = prd_prescale / 0xffff;

	if (prd_prescale % 0xffff)
		prescale++;

	if (prescale > 256) {
		prescale = 256;
		dev_warn(chip->dev, "Warning! prescale overflow, set to 256\n");
	}

	prd = (prd_prescale / prescale) - 1;

	clk_sel = PWM_CLK_APB;

	/* set clk reg */
	val = clk_sel | (div_reg << 4) | ((prescale - 1) << 8);
	writel(val, drobot->base + PWM_CCR(channel));

	/* set prd reg */
	writel(prd, drobot->base + PWM_PR(channel));

	/* set duty reg */
	if(state->duty_cycle)
		drobot_pwm_duty_set(chip, pwm, state, div_reg, prescale);

	/* set polarity, mode and repeat bits */
	val = readl(drobot->base + PWM_CTR(channel));
	if (state->polarity == PWM_POLARITY_INVERSED)
		val |= PWM_POLARITY_INVERSED;
	else
		val &= ~PWM_POLARITY_INVERSED;

	writel(val, drobot->base + PWM_CTR(channel));

	if (state->enabled)
		drobot_pwm_enable(chip, pwm);

	return 0;
}

static const struct pwm_ops drobot_pwm_ops = {
	.apply = drobot_pwm_apply,
	.owner = THIS_MODULE,
};

static int drobot_pwm_remove(struct platform_device *pdev)
{
	struct drobot_pwm_chip *drobot = platform_get_drvdata(pdev);
	unsigned int i;

	for (i = 0; i < drobot->chip.npwm; i++)
		pwm_disable(&drobot->chip.pwms[i]);

	pwmchip_remove(&drobot->chip);

	clk_disable_unprepare(drobot->clk);
	pm_runtime_disable(&pdev->dev);

	return 0;
}

static int drobot_pwm_probe(struct platform_device *pdev)
{
	struct drobot_pwm_chip *drobot_pwm = NULL;
	struct resource *res = NULL;
	int ret = 0;

	drobot_pwm = devm_kzalloc(&pdev->dev, sizeof(*drobot_pwm), GFP_KERNEL);
	if (!drobot_pwm)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	drobot_pwm->base = devm_ioremap_resource(&pdev->dev, res);

	if (IS_ERR(drobot_pwm->base))
		return PTR_ERR(drobot_pwm->base);

	drobot_pwm->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(drobot_pwm->clk))
		return PTR_ERR(drobot_pwm->clk);

	ret = clk_prepare_enable(drobot_pwm->clk);
	if (ret < 0) {
		clk_disable_unprepare(drobot_pwm->clk);
		dev_err(&pdev->dev, "failed to enable pwm clock, error %d\n", ret);
		return ret;
	}

	drobot_pwm->reset = devm_reset_control_get_exclusive(&pdev->dev,
						       NULL);
	if (IS_ERR(drobot_pwm->reset))
		return PTR_ERR(drobot_pwm->reset);
	reset_control_assert(drobot_pwm->reset);
	usleep_range(1, 2);
	reset_control_deassert(drobot_pwm->reset);

	drobot_pwm->chip.dev = &pdev->dev;
	drobot_pwm->chip.ops = &drobot_pwm_ops;
	drobot_pwm->chip.npwm = 2;
	drobot_pwm->chip.base = -1;

	ret = pwmchip_add(&drobot_pwm->chip);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to add PWM chip, error %d\n", ret);
		return ret;
	}

	/* When PWM is disable, configure the output to the default value */
	platform_set_drvdata(pdev, drobot_pwm);
	pm_runtime_enable(&pdev->dev);

	dev_info(&pdev->dev, "D-Robotics PWM register done!\n");

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static unsigned long reg_save[][2] = {
	{PWM_MCR,  0},
	{PWM_CTR(0),  0},
	{PWM_CCR(0),  0},
	{PWM_PR(0),  0},
	{PWM_CTR(1), 0},
	{PWM_CCR(1),  0},
	{PWM_CTR(1),  0},
	{PWM_PR(1),  0},
};

static __maybe_unused int drobot_pwm_suspend(struct device *dev)
{
	struct drobot_pwm_chip *drobot_pwm = dev_get_drvdata(dev);
	int i;

	for (i = 0; i < ARRAY_SIZE(reg_save); i++)
		reg_save[i][1] = readl(drobot_pwm->base + reg_save[i][0]);

	clk_disable_unprepare(drobot_pwm->clk);

	return pm_runtime_force_suspend(dev);
}

static __maybe_unused int drobot_pwm_resume(struct device *dev)
{
	struct drobot_pwm_chip *drobot_pwm = dev_get_drvdata(dev);
	int i, ret;

	ret = clk_prepare_enable(drobot_pwm->clk);
	if (ret) {
		clk_disable_unprepare(drobot_pwm->clk);
		dev_err(dev, "failed to enable pwm clock, error %d\n", ret);
		return ret;
	}

	ret = pm_runtime_force_resume(dev);
	if (ret) {
		dev_err(dev, "failed to resume pwm, error %d\n", ret);
		return ret;
	}

	for (i = 0; i < ARRAY_SIZE(reg_save); i++)
		writel(reg_save[i][1], drobot_pwm->base + reg_save[i][0]);

	return 0;
}
#endif

static const struct dev_pm_ops drobot_pwm_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(drobot_pwm_suspend, drobot_pwm_resume)
};

static struct platform_driver drobot_pwm_driver = {
	.driver = {
		.name = "drobot-pwm",
		.of_match_table = drobot_pwm_dt_ids,
		.pm = &drobot_pwm_pm_ops,
	},
	.probe = drobot_pwm_probe,
	.remove = drobot_pwm_remove,
};
module_platform_driver(drobot_pwm_driver);

MODULE_ALIAS("platform:drobot-pwm");
MODULE_DESCRIPTION("D-Robotics PWM Driver");
MODULE_LICENSE("GPL");
