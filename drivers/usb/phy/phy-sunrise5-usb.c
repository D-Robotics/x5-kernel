/*
 * Horizon Robotics
 *
 *  D-Robotics Sunrise5 USB phy driver.
 *
 *  Current status:
 *	Mainly for 5V vbus supply currently.
 *	Other phy cotrol or setting might be added in future.
 *
 *  Copyright (C) 2020 Horizon Robotics Inc.
 *  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/usb/gadget.h>
#include <linux/usb/otg.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/of.h>
#include <linux/gpio/consumer.h>
#include <linux/delay.h>

struct sunrise5_phy_data {
	struct usb_phy phy;
	struct device *dev;
	struct regulator *vbus;
	unsigned long mA;
};

#define phy_to_data(p)		(container_of((p), struct sunrise5_phy_data, phy))

static int surise5_usb_phy_init(struct usb_phy *phy)
{
	// TODO:

	return 0;
}
static void surise5_usb_phy_shutdown(struct usb_phy *phy)
{
	// TODO:
}

static int sunrise5_usb_set_vbus(struct usb_otg *otg, bool enabled)
{
	struct sunrise5_phy_data *data = phy_to_data(otg->usb_phy);
	int ret;

	if (!data || !data->vbus)
		return -ENODEV;

	if (enabled) {
		ret = regulator_enable(data->vbus);
		if (ret)
			dev_err(data->phy.dev,
				"Failed to enable USB VBUS regulator: %d\n", ret);
	} else {
		/* check if regulator is enabled to avoid "unbalanced disables for vbus" issue */
		if (regulator_is_enabled(data->vbus))  {
			ret = regulator_disable(data->vbus);
			if (ret)
				dev_err(data->phy.dev,
					"Failed to disable USB VBUS regulator: %d\n", ret);
		} else {
			dev_dbg(data->phy.dev, "Regulator is already disabled. No need to disable it again.\n");
		}
	}

	return ret;
}

static int sunrise5_usb_phy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sunrise5_phy_data *data;
	enum usb_phy_type type = USB_PHY_TYPE_USB2;
	int err;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->phy.otg = devm_kzalloc(dev, sizeof(*data->phy.otg),
			GFP_KERNEL);
	if (!data->phy.otg) {
		err = -ENOMEM;
		goto otg_nomem;
	}

	data->vbus = devm_regulator_get(dev, "vbus");
	if (IS_ERR(data->vbus)) {
		if (PTR_ERR(data->vbus) == -EPROBE_DEFER) {
			dev_err(dev,
				"Couldn't get regulator \"vbus\".. Deferring probe\n");
			err = -EPROBE_DEFER;
			goto no_regulator;
		}

		data->vbus = NULL;
	}

	data->dev		= dev;
	data->phy.dev		= data->dev;
	data->phy.label		= "sunrise5-usb-phy";
	data->phy.type		= type;
	data->phy.init		= surise5_usb_phy_init;
	data->phy.shutdown	= surise5_usb_phy_shutdown;

	data->phy.otg->state		= OTG_STATE_UNDEFINED;
	data->phy.otg->usb_phy		= &data->phy;
	data->phy.otg->set_vbus		= sunrise5_usb_set_vbus;

	err = usb_add_phy_dev(&data->phy);
	if (err) {
		dev_err(&pdev->dev, "can't register transceiver, err: %d\n",
			err);
		goto add_phy_fail;
	}

	platform_set_drvdata(pdev, data);

	return 0;

add_phy_fail:
	devm_kfree(dev, data->phy.otg);

no_regulator:
otg_nomem:
	devm_kfree(dev, data);

	return err;
}

static int sunrise5_usb_phy_remove(struct platform_device *pdev)
{
	struct sunrise5_phy_data *data = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;

	if (!data || !data->phy.otg)
		return -EINVAL;

	usb_remove_phy(&data->phy);

	devm_kfree(dev, data->phy.otg);
	devm_kfree(dev, data);

	return 0;
}

static const struct of_device_id sunrise5_usbphy_ids[] = {
	{ .compatible = "d-robotics,sunrise5-usb-phy" },
	{ }
};

MODULE_DEVICE_TABLE(of, sunrise5_usbphy_ids);

static struct platform_driver sunrise5_usb_phy_driver = {
	.probe		= sunrise5_usb_phy_probe,
	.remove		= sunrise5_usb_phy_remove,
	.driver		= {
		.name	= "sunrise5_usb_phy",
		.of_match_table = sunrise5_usbphy_ids,
	},
};
module_platform_driver(sunrise5_usb_phy_driver);

MODULE_ALIAS("platform:sunrise5_usb_phy");
MODULE_AUTHOR("D-Robotics Inc.");
MODULE_DESCRIPTION("D-Robotics Sunrise5 USB PHY Driver");
MODULE_LICENSE("GPL");
