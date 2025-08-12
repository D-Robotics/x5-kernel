// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2022 Horizon Robotics
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 *
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mfd/hpu3501.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/rtc.h>
#include <linux/bcd.h>
#include <linux/delay.h>
#include <linux/of_irq.h>
#include <linux/pm_wakeup.h>

/* PMIC HPU3501 RTC/ALARM rigisters */
#define HPU3501_RTC_ISR 0X30
#define HPU3501_RTC_INIT BIT(0)
#define HPU3501_ALARM_OPSEL BIT(1)
#define HPU3501_ALARM_EN BIT(2)
#define HPU3501_ALARM_F BIT(5)
#define HPU3501_CALENDAR_INIT_ALLOW BIT(6)

#define HPU3501_RTC_YEAR 0X31
#define HPU3501_RTC_MONTH 0X32
#define HPU3501_RTC_DAY 0X33
#define HPU3501_RTC_HOUR 0X34
#define HPU3501_RTC_MIN 0X35
#define HPU3501_RTC_SEC 0X36
#define HPU3501_ALARM_DAY 0X37
#define HPU3501_ALARM_HOUR 0X38
#define HPU3501_ALARM_MIN 0X39
#define HPU3501_ALARM_SEC 0X3A

#define HPU3501_REG_SC		0x05
#define HPU3501_REG_MN		0x04
#define HPU3501_REG_HR		0x03
#define HPU3501_REG_DM		0x02
#define HPU3501_REG_MO		0x01
#define HPU3501_REG_YR		0x00

struct hpu3501_rtc {
	struct device *dev;
	struct rtc_device *rtc;
	unsigned long long epoch_start;
	u32 opsel;
	int irq;
	struct work_struct irq_work;
	bool suspended;
};

static inline struct device *to_hpu3501_dev(struct device *dev)
{
	return dev->parent;
}

static int hpu3501_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct device *pa_dev = to_hpu3501_dev(dev);
	u8 buff[6];
	int ret;

	ret = hpu3501_reads(pa_dev, HPU3501_RTC_YEAR, sizeof(buff), buff);
	if (ret < 0) {
		dev_err(dev, "reads failed with err %d\n", ret);
		goto err;
	}

	dev_dbg(dev,
		"%s: raw data is year=%02x, mon=%02x, mday=%02x, hr=%02x, min=%02x, sec=%02x\n",
		__func__,
		buff[0], buff[1], buff[2], buff[3],
		buff[4], buff[5]);


	tm->tm_sec = bcd2bin(buff[HPU3501_REG_SC] & 0x7F);
	tm->tm_min = bcd2bin(buff[HPU3501_REG_MN] & 0x7F);
	tm->tm_hour = bcd2bin(buff[HPU3501_REG_HR] & 0x3F); /* rtc hr 0-23 */
	tm->tm_mday = bcd2bin(buff[HPU3501_REG_DM] & 0x3F);
	tm->tm_mon = bcd2bin(buff[HPU3501_REG_MO] & 0x1F) - 1; /* rtc mn 1-12 */
	tm->tm_year = bcd2bin(buff[HPU3501_REG_YR]);
	/* Let's do it heuristically, assuming we are live in 1970...2069. */
	/* Handle the years from 2000 to 2069 */
	if (tm->tm_year <= 69) {
		tm->tm_year += 100;
	}

	dev_dbg(dev, "%s: tm is secs=%d, mins=%d, hours=%d, "
		"mday=%d, mon=%d, year=%dn",
		__func__,
		tm->tm_sec, tm->tm_min, tm->tm_hour,
		tm->tm_mday, tm->tm_mon, tm->tm_year);

	return rtc_valid_tm(tm);

err:
	return ret;
}

static int hpu3501_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct device *pa_dev = to_hpu3501_dev(dev);
	int try_times = 5;
	u8 status;
	u8 buff[6];
	int ret;

	dev_dbg(dev, "%s: secs=%d, mins=%d, hours=%d, "
		"mday=%d, mon=%d, year=%d, wday=%d\n",
		__func__,
		tm->tm_sec, tm->tm_min, tm->tm_hour,
		tm->tm_mday, tm->tm_mon, tm->tm_year, tm->tm_wday);

	/* Let's do it heuristically, assuming we are live in 1970...2069. */
	if (tm->tm_year >= 100) {
		tm->tm_year -= 100;
	}
	buff[HPU3501_REG_YR] = bin2bcd(tm->tm_year);
	buff[HPU3501_REG_MO] = bin2bcd(tm->tm_mon + 1);  /* month, 1 - 12 */
	buff[HPU3501_REG_DM] = bin2bcd(tm->tm_mday);
	buff[HPU3501_REG_HR] = bin2bcd(tm->tm_hour);
	buff[HPU3501_REG_MN] = bin2bcd(tm->tm_min);
	buff[HPU3501_REG_SC] = bin2bcd(tm->tm_sec);

	dev_dbg(dev,
		"%s: raw data is year=%02x, mon=%02x, mday=%02x, hr=%02x, min=%02x, sec=%02x\n",
		__func__,
		buff[0], buff[1], buff[2], buff[3],
		buff[4], buff[5]);

	/* Set RTC into initialization mode */
	ret = hpu3501_set_bits(pa_dev, HPU3501_RTC_ISR, HPU3501_RTC_INIT);
	if (ret < 0) {
		dev_err(dev, "failed to set HPU3501_RTC_INIT\n");
		goto err;
	}

	/* wait for HPU3501_CALENDAR_INIT_ALLOW bit become 1 */
	while (try_times >= 0) {
		udelay(1);

		ret = hpu3501_read(pa_dev, HPU3501_RTC_ISR, &status);
		if (ret < 0) {
			dev_err(dev, "read failed with err %d\n", ret);
			goto err;
		}

		if (HPU3501_CALENDAR_INIT_ALLOW & status)
			break;

		try_times--;
	}

	if (try_times < 0) {
		ret = try_times;
		dev_err(dev, "HPU3501_CALENDAR_INIT_ALLOW status error\n");
		goto err;
	}

	ret = hpu3501_writes(pa_dev, HPU3501_RTC_YEAR, sizeof(buff), buff);
	if (ret < 0) {
		dev_err(dev, "failed to program new time\n");
		goto err;
	}

	/* Set RTC into normal mode */
	ret = hpu3501_clr_bits(pa_dev, HPU3501_RTC_ISR, HPU3501_RTC_INIT);
	if (ret < 0) {
		dev_err(dev, "failed to clear HPU3501_RTC_INIT\n");
		goto err;
	}

	/* waiting for date readable */
	msleep(1000);

err:
	return ret;
}

static int hpu3501_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct hpu3501_rtc *rtc = dev_get_drvdata(dev);
	struct device *pa_dev = to_hpu3501_dev(dev);
	unsigned long seconds;
	u8 buff[4];
	int ret;

	seconds = rtc_tm_to_time64(&alrm->time);

	if (alrm->enabled && seconds < rtc->epoch_start) {
		dev_err(dev, "can't set alarm to requested time\n");
		return -EINVAL;
	}

	buff[0] = bin2bcd(alrm->time.tm_mday);
	buff[1] = bin2bcd(alrm->time.tm_hour);
	buff[2] = bin2bcd(alrm->time.tm_min);
	buff[3] = bin2bcd(alrm->time.tm_sec);

	/* Disable alarm and set alarm registers then enable alarm */
	ret = hpu3501_clr_bits(pa_dev, HPU3501_RTC_ISR, HPU3501_ALARM_EN);
	if (ret < 0) {
		dev_err(dev, "failed to disable ALARM_EN\n");
		goto err;
	}

	ret = hpu3501_writes(pa_dev, HPU3501_ALARM_DAY, sizeof(buff), buff);
	if (ret) {
		dev_err(dev, "programming alarm failed with err %d\n", ret);
		goto err;
	}

	ret = hpu3501_set_bits(pa_dev, HPU3501_RTC_ISR, HPU3501_ALARM_EN);
	if (ret < 0) {
		dev_err(dev, "failed to set ALARM_EN\n");
		goto err;
	}

err:
	return ret;
}

static int hpu3501_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct device *pa_dev = to_hpu3501_dev(dev);
	u8 buff[4];
	int ret;

	ret = hpu3501_reads(pa_dev, HPU3501_ALARM_DAY, sizeof(buff), buff);
	if (ret) {
		dev_err(dev, "read ALARM registers failed with err %d\n", ret);
		return ret;
	}

	alrm->time.tm_year = -1;
	alrm->time.tm_mon = -1;
	alrm->time.tm_mday = bcd2bin(buff[0]);
	alrm->time.tm_hour = bcd2bin(buff[1]);
	alrm->time.tm_min = bcd2bin(buff[2]);
	alrm->time.tm_sec = bcd2bin(buff[3]);

	return 0;
}

static int hpu3501_rtc_alarm_irq_enable(struct device *dev,
					unsigned int enabled)
{
	return 0;
}

static void hpu3501_alarm_work(struct work_struct *work)
{
	struct hpu3501_rtc *rtc =
		container_of(work, struct hpu3501_rtc, irq_work);
	struct device *pa_dev = to_hpu3501_dev(rtc->dev);
	int ret;
	u8 status;

	if(rtc->suspended == true) {
		dev_info(rtc->dev, "device still suspended, skip alarm work\n");
		return;
	}

	/* check alarm bit status then clear alarm flag */
	ret = hpu3501_read(pa_dev, HPU3501_RTC_ISR, &status);
	if (ret < 0)
		dev_err(rtc->dev, "read failed with err %d\n", ret);

	if (!(status & HPU3501_ALARM_F)) {
		dev_err(rtc->dev, "alarm flag not set\n");
	} else {
		ret = hpu3501_set_bits(pa_dev, HPU3501_RTC_ISR,
				       HPU3501_ALARM_F);
		if (ret < 0)
			dev_err(rtc->dev, "failed to set HPU3501_ALARM_F\n");
	}

	rtc_update_irq(rtc->rtc, 1, RTC_IRQF | RTC_AF);
}

static irqreturn_t hpu3501_alarm_irq_handler(int irq, void *data)
{
	struct hpu3501_rtc *rtc = data;

	pm_wakeup_event(rtc->dev, 0);

	schedule_work(&rtc->irq_work);

	return IRQ_HANDLED;
}

static const struct rtc_class_ops hpu3501_rtc_ops = {
	.read_time = hpu3501_rtc_read_time,
	.set_time = hpu3501_rtc_set_time,
	.set_alarm = hpu3501_rtc_set_alarm,
	.read_alarm = hpu3501_rtc_read_alarm,
	.alarm_irq_enable = hpu3501_rtc_alarm_irq_enable,
};

static int hpu3501_rtc_probe(struct platform_device *pdev)
{
	struct device *pa_dev = to_hpu3501_dev(&pdev->dev);
	struct i2c_client *i2c_client = to_i2c_client(pa_dev);
	struct hpu3501_rtc *rtc;
	struct device_node *np;
	int ret;

	/* slave hpu3501 skip rtc, because slave do not support it */
	if (i2c_client->addr == 0x3e || i2c_client->addr == 0x3c) {
		ret = 0;
		goto fail_rtc_register;
	}

	rtc = devm_kzalloc(&pdev->dev, sizeof(*rtc), GFP_KERNEL);
	if (!rtc) {
		ret = -EINVAL;
		goto fail_rtc_register;
	}

	rtc->dev = &pdev->dev;

	if (pa_dev->of_node) {
		np = of_get_child_by_name(pa_dev->of_node, "hpu3501-rtc");
		if (!np) {
			dev_err(rtc->dev, "no 'hpu3501-rtc' subnode in DT");
			ret = -EINVAL;
			goto fail_rtc_register;
		}

		if (of_property_read_u32(np, "opsel", &rtc->opsel)) {
			dev_info(rtc->dev, "no opsel in DT\n");
			ret = -EINVAL;
			goto fail_rtc_register;
		}

		if (rtc->opsel != 0 && rtc->opsel != 1) {
			dev_info(rtc->dev, "opsel = %d, should be 0 or 1\n",
				 rtc->opsel);
			ret = -EINVAL;
			goto fail_rtc_register;
		}

		rtc->irq = irq_of_parse_and_map(np, 0);
		if (rtc->irq <= 0) {
			dev_err(pa_dev,
				"gpio hpu3501 irq request failed,"
				" no interrupts in DT, ret %d\n", ret);
			ret = -EINVAL;
			goto fail_rtc_register;
		}
	} else {
		dev_err(pa_dev, "dts node get failed\n");
		ret = -EINVAL;
		goto fail_rtc_register;
	}

	/* Set epoch start as 00:00:00:01:01:1970 */
	rtc->epoch_start = mktime64(1970, 1, 1, 0, 0, 0);

	platform_set_drvdata(pdev, rtc);
	rtc->rtc = devm_rtc_allocate_device(&pdev->dev);
	if (IS_ERR(rtc->rtc)) {
		ret = PTR_ERR(rtc->rtc);
		dev_err(&pdev->dev, "RTC allocate device: ret %d\n", ret);
		goto fail_rtc_register;
	}

	rtc->rtc->ops = &hpu3501_rtc_ops;

	rtc->suspended = false;

	device_init_wakeup(&pdev->dev, 1);

	ret = devm_request_irq(&pdev->dev, rtc->irq, hpu3501_alarm_irq_handler,
			       IRQF_TRIGGER_FALLING, "hpu3501_alarm_irq", rtc);
	if (ret) {
		dev_err(&pdev->dev, "irq request failed\n");
		goto fail_rtc_register;
	}
	INIT_WORK(&rtc->irq_work, hpu3501_alarm_work);

	ret = devm_rtc_register_device(rtc->rtc);
	if (ret) {
		dev_err(&pdev->dev, "RTC device register: ret %d\n", ret);
		goto fail_rtc_register;
	}

	/* init hpu3501 register HPU3501_RTC_ISR bit 1 */
	if (rtc->opsel == 0) {
		ret = hpu3501_clr_bits(pa_dev, HPU3501_RTC_ISR,
				       HPU3501_ALARM_OPSEL);
		if (ret < 0) {
			dev_err(pa_dev,
				"failed to clear HPU3501_ALARM_OPSEL\n");
			goto fail_rtc_register;
		}
	} else {
		ret = hpu3501_set_bits(pa_dev, HPU3501_RTC_ISR,
				       HPU3501_ALARM_OPSEL);
		if (ret < 0) {
			dev_err(pa_dev, "failed to set HPU3501_ALARM_OPSEL\n");
			goto fail_rtc_register;
		}
	}

fail_rtc_register:
	return ret;
};

static int hpu3501_rtc_remove(struct platform_device *pdev)
{
	return 0;
}

static int __maybe_unused rtc_suspend(struct device *dev)
{
	struct hpu3501_rtc *rtc = dev_get_drvdata(dev);

	rtc->suspended = true;

	return enable_irq_wake(rtc->irq);
}

static int __maybe_unused rtc_resume(struct device *dev)
{
	struct hpu3501_rtc *rtc = dev_get_drvdata(dev);

	disable_irq_wake(rtc->irq);

	rtc->suspended = false;

	schedule_work(&rtc->irq_work);

	return 0;
}

static SIMPLE_DEV_PM_OPS(rtc_pm_ops, rtc_suspend, rtc_resume);

static struct platform_driver hpu3501_rtc_driver = {
	.driver = {
		.name = "hpu3501-rtc",
		.pm = &rtc_pm_ops,
	},
	.probe = hpu3501_rtc_probe,
	.remove = hpu3501_rtc_remove,
};
module_platform_driver(hpu3501_rtc_driver);

MODULE_ALIAS("platform:hpu3501-rtc");
MODULE_DESCRIPTION("HOBOT HPU3501 RTC driver");
MODULE_AUTHOR("chaohang.cheng@horizon.ai");
MODULE_LICENSE("GPL v2");
