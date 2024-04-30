// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright(C) 2024, D-Robotics Co., Ltd. All rights reserved
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/hwspinlock.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/pm_runtime.h>

#include "hwspinlock_internal.h"

#define HSEM_MAX_SEMAPHORE 32 /* a total of 32 semaphore */

// If read value is 1, it means semaphore is free and can be granted to the reader;
#define HSEM_READ_LOCKED (1) /* locked */
// Write 1 to set semaphore free (Only if writer is the current owner);
#define HSEM_WRITE_FREE (1) /* set free */

static int drobot_hsem_trylock(struct hwspinlock *lock)
{
	void __iomem *lock_addr = lock->priv;

	/* attempt to acquire the lock by reading its value */
	return (HSEM_READ_LOCKED == (readl(lock_addr) & 0x1));
}

static void drobot_hsem_unlock(struct hwspinlock *lock)
{
	void __iomem *lock_addr = lock->priv;

	/* Release the lock by writing 1 to it */
	writel(HSEM_WRITE_FREE, lock_addr);
}

static void drobot_hsem_relax(struct hwspinlock *lock)
{
	ndelay(50);
}

static const struct hwspinlock_ops drobot_hsem_ops = {
	.trylock = drobot_hsem_trylock,
	.unlock	 = drobot_hsem_unlock,
	.relax	 = drobot_hsem_relax,
};

static int drobot_hsem_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct hwspinlock_device *bank;
	struct hwspinlock *hwlock;
	void __iomem *io_base;
	int i, ret, num_locks = HSEM_MAX_SEMAPHORE;

	if (!node)
		return -ENODEV;

	io_base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(io_base))
		return PTR_ERR(io_base);

	/* no pm needed for HSem but required to comply with hwspilock core */
	pm_runtime_enable(&pdev->dev);

	bank = devm_kzalloc(&pdev->dev, struct_size(bank, lock, num_locks), GFP_KERNEL);
	if (!bank) {
		ret = -ENOMEM;
		goto runtime_err;
	}
	platform_set_drvdata(pdev, bank);

	for (i = 0, hwlock = &bank->lock[0]; i < num_locks; i++, hwlock++)
		hwlock->priv = io_base + sizeof(u32) * i;

	ret = hwspin_lock_register(bank, &pdev->dev, &drobot_hsem_ops, 0, num_locks);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register hwspinlock\n");
		goto runtime_err;
	}

	return 0;

runtime_err:
	pm_runtime_disable(&pdev->dev);
	return ret;
}

static int drobot_hsem_remove(struct platform_device *pdev)
{
	struct hwspinlock_device *bank = platform_get_drvdata(pdev);
	int ret;

	ret = hwspin_lock_unregister(bank);
	if (ret) {
		dev_err(&pdev->dev, "%s failed: %d\n", __func__, ret);
		return ret;
	}

	pm_runtime_disable(&pdev->dev);

	return 0;
}

static const struct of_device_id drobot_hsem_of_match[] = {
	{
		.compatible = "d-robotics,hsem",
	},
	{},
};

MODULE_DEVICE_TABLE(of, drobot_hsem_of_match);

static struct platform_driver drobot_hsem_driver = {
	.probe	= drobot_hsem_probe,
	.remove = drobot_hsem_remove,
	.driver =
		{
			.name		= "drobot_hwsemaphore",
			.owner		= THIS_MODULE,
			.of_match_table = drobot_hsem_of_match,
		},
};

static int __init drobot_hwspinlock_init(void)
{
	return platform_driver_register(&drobot_hsem_driver);
}
/* board init code might need to reserve hwspinlocks for predefined purposes */
postcore_initcall(drobot_hwspinlock_init);

static void __exit drobot_hwspinlock_exit(void)
{
	platform_driver_unregister(&drobot_hsem_driver);
}

module_exit(drobot_hwspinlock_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("D-Robitic Hardware semaphore driver");
