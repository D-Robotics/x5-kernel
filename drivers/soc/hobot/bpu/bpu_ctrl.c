/*
 * Copyright (C) 2019 Horizon Robotics
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 *
 */
#define pr_fmt(fmt) "bpu: " fmt
#include <asm/delay.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/clk-provider.h>
#include <linux/regulator/consumer.h>
#include <linux/hwspinlock.h>
#include <linux/delay.h>
#if defined(CONFIG_PM_DEVFREQ) && defined(CONFIG_DEVFREQ_THERMAL)
#include <linux/devfreq_cooling.h>
#endif
#include <soc/hobot/bpu_notify.h>
#include <linux/pm_runtime.h>
#include "bpu_ctrl.h"

static int32_t bpu_core_set_clkrate(struct bpu_core *core, uint64_t rate);
static uint8_t bpu_core_type(struct bpu_core *core)
{
	unsigned long flags;
	int32_t ret;

	if (core == NULL) {
		pr_err("BPY Core TYPE on invalid core!\n");
		return 0;
	}

	spin_lock_irqsave(&core->hw_io_spin_lock, flags);
	if (core->hw_io != NULL) {
		if (core->hw_io->ops.cmd!= NULL) {
			ret = core->hw_io->ops.cmd(&core->inst, (uint32_t)TYPE_STATE);
			if (ret < 0) {
				dev_err(core->dev, "Get Invalid Core Type!\n");
				ret = CORE_TYPE_UNKNOWN;
			}
		} else {
			ret = CORE_TYPE_ANY;
		}
	} else {
		dev_err(core->dev, "No BPU HW IO for Core to use!\n");
		ret = CORE_TYPE_UNKNOWN;
	}
	spin_unlock_irqrestore(&core->hw_io_spin_lock, flags);

	return (uint8_t)ret;
}

int32_t bpu_core_pend_on(struct bpu_core *core)
{
	if (core == NULL) {
		pr_err("BPU:Pend on invalid core!\n");
		return -EINVAL;
	}

	atomic_set(&core->pend_flag, 1);

	return 0;
}
EXPORT_SYMBOL(bpu_core_pend_on);/*PRQA S 0779*/ /*PRQA S 0307*/ /* Linux Macro */

int32_t bpu_core_pend_off(struct bpu_core *core)
{
	if (core == NULL) {
		pr_err("BPU:Pend off invalid core!\n");
		return -EINVAL;
	}

	atomic_set(&core->pend_flag, 0);
	bpu_prio_trig_out(core->prio_sched);

	return 0;
}
EXPORT_SYMBOL(bpu_core_pend_off);/*PRQA S 0779*/ /*PRQA S 0307*/ /* Linux Macro */

/*
 * check if bpu core is pending by pend func, wait
 * timeout(jiffes) for release pending.
 */
int32_t bpu_core_is_pending(const struct bpu_core *core)
{
	if (core == NULL) {
		pr_err("BPU:Check invalid core!\n");
		return -EINVAL;
	}

	return atomic_read(&core->pend_flag);
}
/*
 * use to wait bpu leisure (task in hwfifo done,
 * and pending new task to hwfifo, if can wait to
 * leisure, timeout is jiffes)
 */
static int32_t bpu_core_pend_to_leisure(struct bpu_core *core, int32_t timeout)
{
	int32_t core_work_state = 0;
	unsigned long flags;
	int32_t ret;

	if (core == NULL) {
		pr_err("BPU:Pend to leisure invalid core!\n");
		return -EINVAL;
	}

	ret = bpu_core_pend_on(core);
	if (ret != 0) {
		dev_err(core->dev, "Pend on to leisure fail!\n");
		return -EINVAL;
	}

	while (core->running_task_num > 0) {
		spin_lock_irqsave(&core->hw_io_spin_lock, flags);
		if (core->hw_io != NULL) {
			if (core->hw_io->ops.cmd != NULL) {
				core_work_state = core->hw_io->ops.cmd(&core->inst, (uint32_t)UPDATE_STATE);
				if (core_work_state == 0) {
					spin_unlock_irqrestore(&core->hw_io_spin_lock, flags);
					/* if core do not work, just break wait */
					break;
				}
			}
		}
		spin_unlock_irqrestore(&core->hw_io_spin_lock, flags);

		if (timeout > 0) {
			if(wait_for_completion_timeout(
					&core->no_task_comp, (uint32_t)timeout) == 0u) {
				spin_lock_irqsave(&core->hw_io_spin_lock, flags);
				if (core->hw_io != NULL) {
					if (core->hw_io->ops.cmd != NULL) {
						(void)core->hw_io->ops.cmd(&core->inst, (uint32_t)UPDATE_STATE);
						ret = core->hw_io->ops.cmd(&core->inst, (uint32_t)WORK_STATE);
						if (ret == core_work_state) {
							/* if states between wait are same, break to not wait*/
							ret = 0;
							spin_unlock_irqrestore(&core->hw_io_spin_lock, flags);
							break;
						}
					}
				}
				spin_unlock_irqrestore(&core->hw_io_spin_lock, flags);

				core->running_task_num--;
			}
		} else {
			wait_for_completion(&core->no_task_comp);
		}
	}

	return ret;
}

/**
 * bpu_core_clk_on() - enable the clock which bpu core need
 * @core: bpu core, could not be null
 *
 * Before use bpu core to process task, clock need be enabled.
 *
 * Return:
 * * =0			- success
 * * <0			- error code
 */
int32_t bpu_core_clk_on(const struct bpu_core *core)
{
	int32_t ret = 0;

	if (core == NULL) {
		pr_err("To Clock on Invalid BPU Core\n");
		return ret;
	}

	if (core->aclk != NULL) {
		if (!__clk_is_enabled(core->aclk)) {
			ret = clk_prepare_enable(core->aclk);
			if (ret != 0) {
				dev_err(core->dev,
						"bpu core[%d] aclk enable failed\n",
						core->index);
			}
		}
	}

	// if (core->mclk != NULL) {
	// 	if (!__clk_is_enabled(core->mclk)) {
	// 		ret = clk_prepare_enable(core->mclk);
	// 		if (ret != 0) {
	// 			dev_err(core->dev,
	// 					"bpu core[%d] mclk enable failed\n",
	// 					core->index);
	// 		}
	// 	}
	// }
	if (core->pe0clk != NULL) {
		if (!__clk_is_enabled(core->pe0clk)) {
			ret = clk_prepare_enable(core->pe0clk);
			if (ret != 0) {
				dev_err(core->dev,
						"bpu core[%d] pe0clk enable failed\n",
						core->index);
			}
		}
	}
	if (core->pe1clk != NULL) {
		if (!__clk_is_enabled(core->pe1clk)) {
			ret = clk_prepare_enable(core->pe1clk);
			if (ret != 0) {
				dev_err(core->dev,
						"bpu core[%d] pe1clk enable failed\n",
						core->index);
			}
		}
	}
	if (core->pe2clk != NULL) {
		if (!__clk_is_enabled(core->pe2clk)) {
			ret = clk_prepare_enable(core->pe2clk);
			if (ret != 0) {
				dev_err(core->dev,
						"bpu core[%d] pe2clk enable failed\n",
						core->index);
			}
		}
	}
	if (core->pe3clk != NULL) {
		if (!__clk_is_enabled(core->pe3clk)) {
			ret = clk_prepare_enable(core->pe3clk);
			if (ret != 0) {
				dev_err(core->dev,
						"bpu core[%d] pe3clk enable failed\n",
						core->index);
			}
		}
	}

	return ret;
}
EXPORT_SYMBOL(bpu_core_clk_on);/*PRQA S 0779*/ /*PRQA S 0307*/ /* Linux Macro */

/**
 * bpu_core_clk_off() - disable the clock which bpu core need
 * @core: bpu core, could not be null
 *
 * when not need use bpu core to process task, clock need
 * be disabled to reduce power consumption.
 *
 * Return:
 * * =0			- success
 * * <0			- error code
 */
int32_t bpu_core_clk_off(const struct bpu_core *core)
{
	int32_t ret = 0;

	if (core == NULL) {
		return ret;
	}

	if (core->aclk != NULL) {
		if (__clk_is_enabled(core->aclk)) {
			clk_disable_unprepare(core->aclk);
		}
	}

	if (core->pe0clk != NULL) {
		if (__clk_is_enabled(core->pe0clk)) {
			clk_disable_unprepare(core->pe0clk);
		}
	}
	if (core->pe1clk != NULL) {
		if (__clk_is_enabled(core->pe1clk)) {
			clk_disable_unprepare(core->pe1clk);
		}
	}
	if (core->pe2clk != NULL) {
		if (__clk_is_enabled(core->pe2clk)) {
			clk_disable_unprepare(core->pe2clk);
		}
	}
	if (core->pe3clk != NULL) {
		if (__clk_is_enabled(core->pe3clk)) {
			clk_disable_unprepare(core->pe3clk);
		}
	}

	return ret;
}
EXPORT_SYMBOL(bpu_core_clk_off);/*PRQA S 0779*/ /*PRQA S 0307*/ /* Linux Macro */

/**
 * bpu_core_clk_off_quirk() - only disable mclk after bpu reset.
 * @core: bpu core, could not be null
 *
 * when not need use bpu core to process task, clock need
 * be disabled to reduce power consumption.
 *
 * Return:
 * * =0			- success
 * * <0			- error code
 */
int32_t bpu_core_clk_off_quirk(const struct bpu_core *core)
{
	if (core == NULL)
		return -EINVAL;

	if (core->mclk != NULL) {
		if (__clk_is_enabled(core->mclk)) {
			clk_disable_unprepare(core->mclk);
		}
	}

	return 0;
}
EXPORT_SYMBOL(bpu_core_clk_off_quirk);/*PRQA S 0779*/ /*PRQA S 0307*/ /* Linux Macro */

/**
 * bpu_core_power_on() - enable the power which bpu core need
 * @core: bpu core, could not be null
 *
 * Before use bpu core to process task, power need be enabled.
 *
 * Return:
 * * =0			- success
 * * <0			- error code
 */
int32_t bpu_core_power_on(const struct bpu_core *core)
{
	int32_t ret = 0;

	if (core == NULL) {
		pr_err("To power on Invalid BPU Core\n");
		return ret;
	}

	// if (core->regulator != NULL) {
	// 	ret = regulator_enable(core->regulator);
	// 	if (ret != 0) {
	// 		dev_err(core->dev,
	// 				"bpu core[%d] enable error\n", core->index);
	// 	}
	// }

	ret = bpu_core_pm_ctrl(core, 0, 1);
	if (ret < 0) {
		dev_err(core->dev,
				"bpu core[%d] pm enable error\n", core->index);
	}

	return ret;
}
EXPORT_SYMBOL(bpu_core_power_on);/*PRQA S 0779*/ /*PRQA S 0307*/ /* Linux Macro */

/**
 * bpu_core_power_off() - disable the power which bpu core need
 * @core: bpu core, could not be null
 *
 * when not need use bpu core to process task, power need
 * be disabled to reduce power consumption.
 *
 * Return:
 * * =0			- success
 * * <0			- error code
 */
int32_t bpu_core_power_off_quirk(const struct bpu_core *core)
{
	int32_t ret = 0;

	if (core == NULL) {
		return ret;
	}

	ret = bpu_core_pm_ctrl(core, 0, 0);
	if (ret < 0) {
		dev_err(core->dev,
				"bpu core[%d] pm disable error\n", core->index);
	}

	// if (core->regulator != NULL) {
	// 	ret = regulator_disable(core->regulator);
	// 	if (ret != 0) {
	// 		dev_err(core->dev,
	// 				"bpu core[%d] regulator disable failed\n",
	// 				core->index);
	// 	}
	// }

	return ret;
}
EXPORT_SYMBOL(bpu_core_power_off_quirk);/*PRQA S 0779*/ /*PRQA S 0307*/ /* Linux Macro */

/**
 * bpu_core_power_off() - disable the power which bpu core need
 * @core: bpu core, could not be null
 *
 * when not need use bpu core to process task, power need
 * be disabled to reduce power consumption.
 *
 * Return:
 * * =0			- success
 * * <0			- error code
 */
int32_t bpu_core_power_off(const struct bpu_core *core)
{
	int32_t ret = 0;

	// if (core == NULL) {
	// 	return ret;
	// }

	// ret = bpu_core_pm_ctrl(core, 0, 0);
	// if (ret < 0) {
	// 	dev_err(core->dev,
	// 			"bpu core[%d] pm disable error\n", core->index);
	// }

	// if (core->regulator != NULL) {
	// 	ret = regulator_disable(core->regulator);
	// 	if (ret != 0) {
	// 		dev_err(core->dev,
	// 				"bpu core[%d] regulator disable failed\n",
	// 				core->index);
	// 	}
	// }

	return ret;
}
EXPORT_SYMBOL(bpu_core_power_off);/*PRQA S 0779*/ /*PRQA S 0307*/ /* Linux Macro */

/**
 * bpu_core_enable() - enable and init bpu core hardware
 * @core: bpu core, could not be null
 *
 * Enable and init bpu core hardware dependence and
 * necessary software resources related to hardware
 *
 * Return:
 * * =0			- success
 * * <0			- error code
 */
int32_t bpu_core_enable(struct bpu_core *core)
{
	int32_t err, ret;

	if (core == NULL) {
		pr_err("BPU:Enable invalid core!\n");
		return -EINVAL;
	}

	if (core->hw_enabled > 0u) {
		return 0;
	}

	if (core->hw_io != NULL) {
		if (core->hw_io->ops.enable != NULL) {
			ret = core->hw_io->ops.enable(&core->inst);
		} else {
			ret = bpu_core_power_on(core);
			ret += bpu_core_clk_on(core);
		}
	} else {
		dev_err(core->dev, "No BPU HW IO for Core to use!\n");
		ret = -EINVAL;
	}

	core->last_err_point = 0;
	enable_irq(core->irq);
	core->hw_enabled = 1;

	err = bpu_core_pend_off(core);
	if (err != 0) {
		dev_err(core->dev, "Pend off for Disable core failed!\n");
		return err;
	}

	bpu_prio_plug_in(core->prio_sched);
	(void)bpu_notifier_notify((uint64_t)POWER_ON, (void *)&core->index);

	return ret;
}
EXPORT_SYMBOL(bpu_core_enable);/*PRQA S 0779*/ /*PRQA S 0307*/ /* Linux Macro */

static int32_t bpu_core_plug_out(struct bpu_core *core)
{
	int32_t tmp_work_state = 1;
	unsigned long flags;
	int32_t ret = 0;

	if (core == NULL) {
		pr_err("BPU:Disable invalid core!\n");
		return -EINVAL;
	}

	if (core->hotplug > 0u) {
		/*
		 * if need support hotplug on/off, before real
		 * disable, need wait the prio task fifos empty
		 */
		do {
			ret = bpu_prio_wait_empty(core->prio_sched, HZ);
			spin_lock_irqsave(&core->hw_io_spin_lock, flags);
			if (core->hw_io != NULL) {
				if (core->hw_io->ops.cmd != NULL) {
					tmp_work_state = core->hw_io->ops.cmd(&core->inst, (uint32_t)WORK_STATE);
				}
			} else {
				tmp_work_state = 0;
			}
			spin_unlock_irqrestore(&core->hw_io_spin_lock, flags);
		/* if core not work, can exit waiting */
		} while ((ret != 0) && (tmp_work_state > 0));
	}

	return ret;
}

/**
 * bpu_core_disable() - disable and deinit bpu core hardware
 * @core: bpu core, could not be null
 *
 * Disable and deinit bpu core hardware dependence and
 * necessary software resources related to hardware
 *
 * Return:
 * * =0			- success
 * * <0			- error code
 */
int32_t bpu_core_disable(struct bpu_core *core)
{
	int32_t err, ret;

	if (core == NULL) {
		pr_err("BPU:Disable invalid core!\n");
		return -EINVAL;
	}
	if (core->hw_enabled == 0u) {
		return 0;
	}

	(void)bpu_notifier_notify((uint64_t)POWER_OFF, (void *)&core->index);
	bpu_prio_plug_out(core->prio_sched);
	ret = bpu_core_plug_out(core);
	if (ret != 0) {
		bpu_prio_plug_in(core->prio_sched);
		dev_err(core->dev, "Try to pre Plugout core failed!\n");
		return ret;
	}

	core->hw_enabled = 0;
	/*
	 * need wait running fc task done to prevent
	 * BPU or system problem
	 */
	ret = bpu_core_pend_to_leisure(core, HZ);
	if (ret != 0) {
		dev_err(core->dev, "Pend for Disable core failed!\n");
		return ret;
	}
	disable_irq(core->irq);

	if (core->hw_io != NULL) {
		if (core->hw_io->ops.disable != NULL) {
			ret = core->hw_io->ops.disable(&core->inst);
			mdelay((uint64_t)core->power_delay);
		} else {
			ret = bpu_core_clk_off(core);
			ret += bpu_core_power_off(core);
		}
	} else {
		dev_err(core->dev, "No BPU HW IO for Core to use!\n");
		ret = -EINVAL;
	}

	err = bpu_core_pend_off(core);
	if (err != 0) {
		dev_err(core->dev, "Pend off for Disable core failed!\n");
		return err;
	}

	return ret;
}
EXPORT_SYMBOL(bpu_core_disable);/*PRQA S 0779*/ /*PRQA S 0307*/ /* Linux Macro */

/* break E1, no need error return logic inside the function */
static void bpu_core_reset_run_fc_fifo(struct bpu_core *core)
{
	struct bpu_fc tmp_bpu_fc;
	int32_t ret;
	int32_t prio_num, i;

	prio_num = BPU_CORE_PRIO_NUM(core);
	for (i = 0; i < prio_num; i++) {
		while (!kfifo_is_empty(&core->run_fc_fifo[i])) {
			ret = kfifo_get(&core->run_fc_fifo[i], &tmp_bpu_fc);
			if (ret < 1) {
				continue;
			}
			bpu_fc_clear(&tmp_bpu_fc);
		}
		kfifo_reset(&core->run_fc_fifo[i]);
	}
}

/**
 * bpu_core_reset_quirk() - reset bpu core hardware
 * @core: bpu core, could not be null
 *
 * quirk reset method, only do bpu subsys reset
 * and noc idle request.
 *
 * Return:
 * * =0			- success
 * * <0			- error code
 */
int32_t bpu_core_reset_quirk(struct bpu_core *core)
{
	int32_t ret;

	if (core == NULL) {
		pr_err("BPU:Reset invalid core!\n");
		return -EINVAL;
	}

	if (core->rst != NULL) {
		ret = reset_control_assert(core->rst);
		if (ret < 0) {
			dev_err(core->dev, "bpu core reset assert failed\n");
			return ret;
		}
		udelay(1);
		ret = reset_control_deassert(core->rst);
		if (ret < 0) {
			dev_err(core->dev, "bpu core reset deassert failed\n");
			return ret;
		}
	}


	dev_dbg(core->dev, "do %s succeed\n", __func__);
	return 0;
}
EXPORT_SYMBOL(bpu_core_reset_quirk);/*PRQA S 0779*/ /*PRQA S 0307*/ /* Linux Macro */

/**
 * bpu_core_reset() - reset bpu core hardware
 * @core: bpu core, could not be null
 *
 * Reset bpu core hardware dependence and
 * necessary software resources related to hardware
 *
 * Return:
 * * =0			- success
 * * <0			- error code
 */
int32_t bpu_core_reset(struct bpu_core *core)
{
	unsigned long flags;
	int32_t ret;

	if (core == NULL) {
		pr_err("BPU:Reset invalid core!\n");
		return -EINVAL;
	}

	bpu_core_reset_run_fc_fifo(core);
	kfifo_reset(&core->done_fc_fifo);

	spin_lock_irqsave(&core->hw_io_spin_lock, flags);
	if (core->hw_io != NULL) {
		if (core->hw_io->ops.reset != NULL) {
			ret = core->hw_io->ops.reset(&core->inst);
			spin_unlock_irqrestore(&core->hw_io_spin_lock, flags);
			return ret;
		}
	} else {
		spin_unlock_irqrestore(&core->hw_io_spin_lock, flags);
		dev_err(core->dev, "No BPU HW IO for Core to use!\n");
		return -ENODEV;
	}
	spin_unlock_irqrestore(&core->hw_io_spin_lock, flags);

	ret = bpu_core_disable(core);
	if (ret != 0) {
		dev_err(core->dev, "BPU core %d disable failed\n", core->index);
	}
	ret = bpu_core_enable(core);
	if (ret != 0) {
		dev_err(core->dev, "BPU core %d enable failed\n", core->index);
	}

	if (core->rst != NULL) {
		ret = reset_control_assert(core->rst);
		if (ret < 0) {
			dev_err(core->dev, "bpu core reset assert failed\n");
			return ret;
		}
		udelay(1);
		ret = reset_control_deassert(core->rst);
		if (ret < 0) {
			dev_err(core->dev, "bpu core reset deassert failed\n");
			return ret;
		}
	}

	return 0;
}
EXPORT_SYMBOL(bpu_core_reset);/*PRQA S 0779*/ /*PRQA S 0307*/ /* Linux Macro */

/**
 * bpu_core_process_recover() - recover bpu running tasks
 * @core: bpu core, could not be null
 *
 * sometime bpu core hardware need recovery task which runs
 * before(hung or resume)
 *
 * Return:
 * * =0			- success
 * * <0			- error code
 */
int32_t bpu_core_process_recover(struct bpu_core *core)
{
	struct bpu_fc tmp_bpu_fc;
	DECLARE_KFIFO_PTR(recovery_kfifo[MAX_HW_FIFO_NUM], struct bpu_fc);/*PRQA S 1061*/ /*PRQA S 1062*/ /* Linux Macro */
	struct bpu_user *tmp_user;
	unsigned long flags;
	int32_t prio_num;
	int32_t ret;
	int32_t i;

	if (core == NULL) {
		pr_err("TO recovery no bpu core\n");
		return -ENODEV;
	}

	dev_err(core->dev, "TO recovery bpu core%d\n", core->index);
	/* copy run_fc_fifo for recover */
	prio_num = BPU_CORE_PRIO_NUM(core);
	for (i = (int32_t)prio_num - 1; i >= 0; i--) {
		(void)memcpy((void *)&recovery_kfifo[i], (void *)&core->run_fc_fifo[i],
				sizeof(recovery_kfifo[i]));
		while (kfifo_len(&recovery_kfifo[i]) > 0) {
			ret = kfifo_get(&recovery_kfifo[i], &tmp_bpu_fc);
			if (ret < 1) {
				dev_err(core->dev,
						"Get recovery bpu fc failed in BPU Core%d\n",
						core->index);
				return -EINVAL;
			}

			spin_lock_irqsave(&core->spin_lock, flags);
			/* not recovery exit user's task */
			spin_lock(&g_bpu->user_spin_lock);
			tmp_user = bpu_get_user(&tmp_bpu_fc, &core->user_list);
			if (tmp_user == NULL) {
				spin_unlock(&g_bpu->user_spin_lock);
				spin_unlock_irqrestore(&core->spin_lock, flags);
				continue;
			}
			spin_unlock(&g_bpu->user_spin_lock);

			spin_lock(&core->hw_io_spin_lock);
			if (core->hw_io != NULL) {
				if ((core->hw_io->ops.write_fc != NULL) && (tmp_bpu_fc.fc_data != NULL)) {
					ret = core->hw_io->ops.write_fc(&core->inst, &tmp_bpu_fc, 0);
					if (ret < 0) {
						spin_unlock(&core->hw_io_spin_lock);
						spin_unlock_irqrestore(&core->spin_lock, flags);
						dev_err(core->dev, "TO recovery bpu core%d failed\n",
								core->index);
						return ret;
					}
				}
			}
			spin_unlock(&core->hw_io_spin_lock);

			spin_unlock_irqrestore(&core->spin_lock, flags);
		}
	}

	return 0;
}
EXPORT_SYMBOL(bpu_core_process_recover);/*PRQA S 0779*/ /*PRQA S 0307*/ /* Linux Macro */

#if defined(CONFIG_PM_DEVFREQ) && defined(CONFIG_DEVFREQ_THERMAL)
static int32_t bpu_core_raw_set_volt(const struct bpu_core *core, int32_t volt)
{
	int32_t ret = 0;

	if (core == NULL) {
		pr_err("Set invalid bpu core voltage!\n");
		return -ENODEV;
	}

	if (volt <= 0) {
		dev_err(core->dev, "Set invalid value bpu core voltage!\n");
		return -EINVAL;
	}

	if (core->regulator != NULL) {
		ret = regulator_set_voltage(core->regulator,
					volt, INT_MAX);
		if (ret != 0) {
			dev_err(core->dev, "Cannot set voltage %u uV\n", volt);
			return ret;
		}
	}

	return ret;
}

static int32_t bpu_core_set_volt(struct bpu_core *core, int32_t volt)
{
	unsigned long flags;
	int32_t err, ret;

	if (core == NULL) {
		pr_err("BPU:Invalid core set volt!\n");
		return -EINVAL;
	}

	if (volt == 0) {
		ret = bpu_core_disable(core);
		if (ret != 0) {
			dev_err(core->dev, "Diable bpu core%d set volt!\n", core->index);
			return ret;
		}
	}

	ret = bpu_core_pend_to_leisure(core, HZ);
	if (ret != 0) {
		dev_err(core->dev, "Pend for core volt change failed!\n");
		return ret;
	}

	spin_lock_irqsave(&core->hw_io_spin_lock, flags);
	if (core->hw_io != NULL) {
		if (core->hw_io->ops.set_volt != NULL) {
			ret = core->hw_io->ops.set_volt(&core->inst, volt);
		} else {
			spin_unlock_irqrestore(&core->hw_io_spin_lock, flags);
			ret = bpu_core_raw_set_volt(core, volt);
			spin_lock_irqsave(&core->hw_io_spin_lock, flags);
		}
	} else {
		spin_unlock_irqrestore(&core->hw_io_spin_lock, flags);
		dev_err(core->dev, "No BPU HW IO for Core to use!\n");
		return -ENODEV;
	}
	spin_unlock_irqrestore(&core->hw_io_spin_lock, flags);

	err = bpu_core_pend_off(core);
	if (err != 0) {
		dev_err(core->dev, "Pend off from core volt change failed!\n");
		return err;
	}

	return ret;
}
#endif

static int32_t bpu_core_set_clk(struct bpu_core *core, uint64_t rate)
{
	unsigned long flags;
	int32_t err, ret;

	if (core == NULL) {
		pr_err("BPU:Invalid core set volt!\n");
		return -EINVAL;
	}

	ret = bpu_core_pend_to_leisure(core, HZ);
	if (ret != 0) {
		dev_err(core->dev, "Pend for core clk change failed!\n");
		return ret;
	}

	/*
	 * set clock must use special timing and step
	 * or else some unknown exceptions will appear
	 */
	ret = bpu_core_pm_ctrl(core, 0, 1);
	if (ret >= 0) {
		spin_lock_irqsave(&core->hw_io_spin_lock, flags);
		if (core->hw_io != NULL) {
			if (core->hw_io->ops.set_clk!= NULL) {
				ret = core->hw_io->ops.set_clk(&core->inst, rate);
			} else {
				spin_unlock_irqrestore(&core->hw_io_spin_lock, flags);
				ret = bpu_core_set_clkrate(core, rate);
				spin_lock_irqsave(&core->hw_io_spin_lock, flags);
			}
		} else {
			spin_unlock_irqrestore(&core->hw_io_spin_lock, flags);
			bpu_core_pm_ctrl(core, 0, 0);
			dev_err(core->dev, "No BPU HW IO for Core to use!\n");
			return -ENODEV;
		}
		spin_unlock_irqrestore(&core->hw_io_spin_lock, flags);

		bpu_core_pm_ctrl(core, 0, 0);
	}

	if (ret != 0) {
		dev_err(core->dev, "BPU Core set clk to %lld failed!\n", rate);
	}

	err = bpu_core_pend_off(core);
	if (err != 0) {
		dev_err(core->dev, "Pend off from core clk change failed!\n");
		return err;
	}

	return ret;
}

static uint64_t bpu_core_get_clk(const struct bpu_core *core)
{
	uint64_t rate;

	if (core == NULL) {
		pr_err("BPU:Invalid core get clk!\n");
		return 0;
	}

	rate = clk_get_rate(core->mclk);

	return rate;
}

/**
 * bpu_core_ext_ctrl() - bpu core extend control
 * @core: bpu core, could not be null
 * @cmd: ctrl command
 * @data: data pointer
 *
 * interface for some extend bpu core control
 *
 * Return:
 * * =0			- success
 * * <0			- error code
 */
int32_t bpu_core_ext_ctrl(struct bpu_core *core, ctrl_cmd_t cmd, uint64_t *data)
{
	if ((core == NULL) || (data == NULL)) {
		pr_err("BPU:Invalid core/data!\n");
		return -EINVAL;
	}

	if (cmd == CORE_TYPE) {
		*data = bpu_core_type(core);
		return 0;
	}

	if (cmd == SET_CLK) {
		return bpu_core_set_clk(core, *data);
	}

	if (cmd == GET_CLK) {
		if (core->hw_enabled == 0u) {
			*data = 0u;
		} else {
			*data = bpu_core_get_clk(core);
		}
		return 0;
	}

	dev_err(core->dev, "Invalid core cmd!\n");

	return -EINVAL;
}
EXPORT_SYMBOL(bpu_core_ext_ctrl);/*PRQA S 0779*/ /*PRQA S 0307*/ /* Linux Macro */

#if defined(CONFIG_PM_DEVFREQ) && defined(CONFIG_DEVFREQ_THERMAL)
static int32_t bpu_core_bind_set_rate_volt(struct bpu_core *core,
		uint64_t target_rate, uint64_t target_volt)
{
	int32_t err = 0;

	if (core->dvfs->rate == target_rate) {
		if (core->dvfs->volt != target_volt) {
			err = bpu_core_set_volt(core, (int32_t)target_volt);
			if (err != 0) {
				dev_err(core->dev, "Cannot set voltage %llu uV\n",
						target_volt);
				return err;
			}
			core->dvfs->volt = target_volt;
			return err;
		}
	} else {
		/*
		 * To higher rate: need set volt first
		 * To lower rate: need set rate first
		 */
		if (core->dvfs->rate < target_rate) {
			if (core->dvfs->volt != target_volt) {
				err = bpu_core_set_volt(core, (int32_t)target_volt);
				if (err != 0) {
					dev_err(core->dev, "Cannot set voltage %llu uV\n",
							target_volt);
					return err;
				}
			}
		}

		err = bpu_core_set_clk(core, target_rate);
		if (err != 0) {
			dev_err(core->dev, "Cannot set frequency %llu (%d)\n",
					target_rate, err);
			err = bpu_core_set_volt(core, (int32_t)core->dvfs->volt);
			if (err != 0) {
				dev_err(core->dev, "Recovery to old volt failed\n");
			}
			return err;
		}

		if (core->dvfs->rate > target_rate && core->dvfs->volt != target_volt) {
			err = bpu_core_set_volt(core, (int32_t)target_volt);
			if (err != 0) {
				dev_err(core->dev, "Cannot set vol %llu uV\n", target_volt);
				return err;
			}
		}

		core->dvfs->rate = target_rate;
		core->dvfs->volt = target_volt;
	}

	return err;
}

static int bpu_core_set_freq(struct device *dev, unsigned long *freq, u32 flags)
{
	struct bpu_core *core = (struct bpu_core *)dev_get_drvdata(dev);
	struct dev_pm_opp *opp;
	uint64_t rate, target_rate;
	uint64_t target_volt;
	int32_t err;

	opp = devfreq_recommended_opp(dev, freq, flags);
	if (IS_ERR(opp)) {
		err = (int32_t)PTR_ERR(opp);
		return err;
	}
	rate = dev_pm_opp_get_freq(opp);
	target_volt = dev_pm_opp_get_voltage(opp);

	target_rate = (uint64_t)clk_round_rate(core->mclk, rate);
	if (target_rate <= 0u) {
		target_rate = rate;
	}

	err = bpu_core_bind_set_rate_volt(core, target_rate, target_volt);
	dev_pm_opp_put(opp);

	return err;
}

static int bpu_core_get_freq(struct device *dev, unsigned long *freq)
{
	struct bpu_core *core = (struct bpu_core *)dev_get_drvdata(dev);

	if (core != NULL) {
		if (core->dvfs != NULL) {
			core->dvfs->rate = bpu_core_get_clk(core);
			*freq = core->dvfs->rate;
		}
	}

	return 0;
}

/**
 * bpu_core_dvfs_register() - register bpu core to linux dvfs mechanism
 * @core: bpu core, could not be null
 * @name: governor name, could be null
 *
 * Register the bpu core frequency to linux dvfs, set governor to
 * according name parameter, if name is null, default use performance
 *
 * Return:
 * * =0			- success
 * * <0			- error code
 */
int32_t bpu_core_dvfs_register(struct bpu_core *core, const char *name)
{
	struct dev_pm_opp *opp;
	const char *gov_name;
	uint32_t tmp_state;
	int32_t ret;

	if (core == NULL) {
		pr_err("NO BPU Core!!\n");
		return -EINVAL;
	}

	core->dvfs = (struct bpu_core_dvfs *)devm_kzalloc(core->dev,
			sizeof(struct bpu_core_dvfs), GFP_KERNEL);
	if (core->dvfs == NULL) {
		dev_err(core->dev, "Can't alloc BPU dvfs.\n");
		return -ENOMEM;
	}

	if (dev_pm_opp_of_add_table(core->dev) != 0) {
		devm_kfree(core->dev, (void *)core->dvfs);
		core->dvfs = NULL;
		dev_err(core->dev, "Invalid operating-points in devicetree.\n");
		return -EINVAL;
	}
	device_property_read_u32(core->dev, "polling_ms", &core->dvfs->profile.polling_ms);
	core->dvfs->profile.target = bpu_core_set_freq;
	core->dvfs->profile.get_cur_freq = bpu_core_get_freq;
	if (core->mclk != NULL) {
		core->dvfs->profile.initial_freq = bpu_core_get_clk(core);
	} else {
		devm_kfree(core->dev, (void *)core->dvfs);
		core->dvfs = NULL;
		dev_err(core->dev, "No adjustable clock for dvfs.\n");
		return -EINVAL;
	}

	core->dvfs->rate = core->dvfs->profile.initial_freq;
	if (core->regulator != NULL) {
		core->dvfs->volt = (uint32_t)regulator_get_voltage(core->regulator);
	} else {
		dev_info(core->dev, "No adjustable regulator for dvfs, use fake value.\n");
		opp = devfreq_recommended_opp(core->dev, &core->dvfs->profile.initial_freq,
				DEVFREQ_FLAG_LEAST_UPPER_BOUND);
		if (IS_ERR(opp)) {
			devm_kfree(core->dev, (void *)core->dvfs);
			core->dvfs = NULL;
			dev_err(core->dev, "Can't find opp\n");
			return (int32_t)PTR_ERR(opp);
		}
		core->dvfs->volt = dev_pm_opp_get_voltage(opp);
		dev_pm_opp_put(opp);
	}

	if (of_property_read_string(core->dev->of_node, "governor", &gov_name) != 0) {
		if (name != NULL) {
			gov_name = name;
		} else {
			gov_name = "performance";
		}
	}

	core->dvfs->devfreq = devm_devfreq_add_device(core->dev,
			&core->dvfs->profile, gov_name, NULL);
	if (IS_ERR(core->dvfs->devfreq)) {
		core->dvfs->devfreq = NULL;
		dev_err(core->dev, "Can't add dvfs to BPU core.\n");
		return (int32_t)PTR_ERR(core->dvfs->devfreq);
	}
	core->dvfs->profile.max_state = core->dvfs->devfreq->max_state;
	core->dvfs->profile.freq_table = core->dvfs->devfreq->freq_table;

	if (core->dvfs->profile.max_state > 0u) {
		tmp_state = core->dvfs->profile.max_state - 1u;
	} else {
		tmp_state = 0;
	}
	core->dvfs->devfreq->scaling_min_freq = core->dvfs->profile.freq_table[0];
	core->dvfs->devfreq->scaling_max_freq =
		core->dvfs->profile.freq_table[tmp_state];
	core->dvfs->level_num = core->dvfs->profile.max_state;

	ret = devm_devfreq_register_opp_notifier(core->dev, core->dvfs->devfreq);

	core->dvfs->cooling = of_devfreq_cooling_register(core->dev->of_node,
			core->dvfs->devfreq);
	if (IS_ERR(core->dvfs->cooling)) {
		core->dvfs->cooling = NULL;
	}

	core->power_level = 1;

	return ret;
}
EXPORT_SYMBOL(bpu_core_dvfs_register);/*PRQA S 0779*/ /*PRQA S 0307*/ /* Linux Macro */

/**
 * bpu_core_dvfs_unregister() - unregister the bpu core from dvfs mechanism
 * @core: bpu core, could not be null
 * @name: governor name, could be null
 *
 * Unregister the bpu core frequency from linux dvfs, which registed
 * before.
 */
/* break E1, no need care error return when use the function */
void bpu_core_dvfs_unregister(struct bpu_core *core)
{
	if (core == NULL) {
		return;
	}

	if (core->dvfs == NULL) {
		return;
	}

	if (core->dvfs->cooling != NULL) {
		devfreq_cooling_unregister(core->dvfs->cooling);
		core->dvfs->cooling = NULL;
	}

	if (core->dvfs->devfreq != NULL) {
		devm_devfreq_unregister_opp_notifier(core->dev, core->dvfs->devfreq);
		devm_devfreq_remove_device(core->dev, core->dvfs->devfreq);
		core->dvfs->devfreq = NULL;
	}
	dev_pm_opp_of_remove_table(core->dev);

	devm_kfree(core->dev, (void *)core->dvfs);
	core->dvfs = NULL;
}
EXPORT_SYMBOL(bpu_core_dvfs_unregister);/*PRQA S 0779*/ /*PRQA S 0307*/ /* Linux Macro */

/**
 * bpu_core_set_freq_level() - set bpu core work frequency level
 * @core: bpu core, could not be null
 * @level: frequency number
 *
 * bpu core bpu core work frequency level is 0/-n, if level > 0,
 * choose the set dvfs governor.
 *
 * Return:
 * * =0			- success
 * * <0			- error code
 */
int32_t bpu_core_set_freq_level(struct bpu_core *core, int32_t level)
{
	uint64_t wanted = 0u;
	int32_t tmp_state, tmp_level;
	int32_t ret = 0;
	uint32_t i;

	if (core == NULL) {
		pr_err("BPU:Invalid core set freq level!\n");
		return -EINVAL;
	}

	if (core->dvfs == NULL) {
		dev_err(core->dev, "No Adjustable clock to support set freq level!\n");
		return -EINVAL;
	}

	tmp_level = level;
	if (level > 0) {
		/* if level > 0, make the governor to default gover*/
		if (core->dvfs->cooling == NULL) {
			for (i = 0; i < core->dvfs->profile.max_state; i++) {
				ret = dev_pm_opp_enable(core->dev,
						core->dvfs->profile.freq_table[i]);
			}
			core->dvfs->cooling =
				of_devfreq_cooling_register(core->dev->of_node,
						core->dvfs->devfreq);
			if (IS_ERR(core->dvfs->cooling)) {
				core->dvfs->cooling = NULL;
			}
		}
		core->power_level = 1;
	} else {
		/* if level <= 0, user freq set*/
		if (core->dvfs->cooling != NULL) {
			devfreq_cooling_unregister(core->dvfs->cooling);
			core->dvfs->cooling = NULL;
		}

		if ((level <= (-1 * (int32_t)core->dvfs->level_num)) && (level != 0)) {
			dev_err(core->dev,
					"Set BPU core%d freq level(%d) lower then lowest(%d)\n",
					core->index, level, (-1 * (int32_t)core->dvfs->level_num) + 1);
			tmp_level = (-1 * (int32_t)core->dvfs->level_num) + 1;
		}

		if (((int32_t)core->dvfs->profile.max_state + tmp_level) > 0) {
			tmp_state = ((int32_t)core->dvfs->profile.max_state + tmp_level) - 1;
		} else {
			tmp_state = 0;
		}
		for (i = 0; i < core->dvfs->profile.max_state; i++) {
			if (i == (uint32_t)tmp_state) {
				wanted = core->dvfs->profile.freq_table[tmp_state];
				ret = dev_pm_opp_enable(core->dev, wanted);
				if (ret != 0) {
					dev_err(core->dev, "Set BPU core pm opp enable failed\n");
				}
				continue;
			}
			ret = dev_pm_opp_disable(core->dev,
					core->dvfs->profile.freq_table[i]);
			if (ret != 0) {
				dev_err(core->dev, "Set BPU core pm opp disable failed\n");
			}
		}

		mutex_lock(&core->dvfs->devfreq->lock);
		if (wanted < core->dvfs->rate) {
			ret = bpu_core_set_freq(core->dev, (unsigned long *)&wanted, 0);
		} else {
			ret = bpu_core_set_freq(core->dev,
					(unsigned long *)&wanted,
					DEVFREQ_FLAG_LEAST_UPPER_BOUND);
		}
		if (ret != 0) {
			mutex_unlock(&core->dvfs->devfreq->lock);
			dev_err(core->dev,
					"Set BPU core%d set freq level(%d) failed\n",
					core->index, tmp_level);
			return ret;
		}
		mutex_unlock(&core->dvfs->devfreq->lock);
		core->power_level = tmp_level;
	}

	return ret;
}
EXPORT_SYMBOL(bpu_core_set_freq_level);/*PRQA S 0779*/ /*PRQA S 0307*/ /* Linux Macro */
#endif

/**
 * bpu_core_set_limit() - set bpu core priority buffer limit num
 * @core: bpu core, could not be null
 * @limit: buffer number
 *
 * bpu core priority buffer limit determine the granularity of
 * software priority scheduling
 *
 * Return:
 * * =0			- success
 * * <0			- error code
 */
int32_t bpu_core_set_limit(struct bpu_core *core, int32_t limit)
{
	int32_t tmp_limit;
	uint32_t task_cap;
	unsigned long flags;

	if (core == NULL) {
		pr_err("BPU:Invalid core set freq level!\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&core->hw_io_spin_lock, flags);
	if (core->hw_io != NULL) {
		task_cap = core->hw_io->task_capacity;
	} else {
		spin_unlock_irqrestore(&core->hw_io_spin_lock, flags);
		dev_err(core->dev, "No BPU HW IO for Core to use!\n");
		return -ENODEV;
	}
	spin_unlock_irqrestore(&core->hw_io_spin_lock, flags);

	if ((limit < 0) || (limit > (int32_t)task_cap)) {
		tmp_limit = 0;
	} else {
		tmp_limit = limit;
	}

	core->inst.task_buf_limit = tmp_limit;

	return 0;
}
EXPORT_SYMBOL(bpu_core_set_limit);/*PRQA S 0779*/ /*PRQA S 0307*/ /* Linux Macro */

/**
 * bpu_clk_get_rate() - get bpu core clock rate
 * @clk: linux clk struct
 *
 * Return: clock rate value
 */
static int32_t bpu_core_set_clkrate(struct bpu_core *core, uint64_t rate)
{
	uint64_t last_rate;
	int32_t ret = 0;
	struct clk *set_clk;

	if (core == NULL) {
		return -EINVAL;
	}

	set_clk = core->mclk;

	if (set_clk == NULL) {
		return 0;
	}

	last_rate = clk_get_rate(set_clk);

	if (last_rate == rate) {
		return 0;
	}

	ret = clk_set_rate(set_clk, rate);
	if (ret != 0) {
		dev_err(core->dev, "Cannot set frequency %llu (%d)\n",
				rate, ret);
		return ret;
	}

	/* check if rate set success, when not, user need recover volt */
	if (clk_get_rate(set_clk) != rate) {
		dev_err(core->dev,
				"Get wrong frequency, Request %llu, Current %lu\n",
				rate, clk_get_rate(set_clk));
		return -EINVAL;
	}

	return 0;
}

/**
 * bpu_reset_ctrl() - set bpu core reset
 * @rstc: linux reset control struct
 * @val: reset value(1: assert; 2: deassert)
 *
 * Returns 0 on success, -EERROR otherwise.
 */
int32_t bpu_reset_ctrl(struct reset_control *rstc, uint16_t val)
{
	if (val > 0u) {
		return reset_control_assert(rstc);
	}

	return reset_control_deassert(rstc);
}
EXPORT_SYMBOL(bpu_reset_ctrl);/*PRQA S 0779*/ /*PRQA S 0307*/ /* Linux Macro */

/**
 * bpu_core_pm_ctrl() - set bpu core pm control
 * @core: bpu core, could not be null
 * @init: if core pm init/deinit; 1: init/deinit; 0: normal
 * @ops: 0:put; 1: get
 *
 * use the interface to pack the power domain
 *
 * Return:
 * * =0			- success
 * * <0			- error code
 */
int32_t bpu_core_pm_ctrl(const struct bpu_core *core, uint16_t init, uint16_t ops)
{
	int32_t ret = 0;

	if (core == NULL) {
		pr_err("To pm ctrl invalid BPU Core!\n");
		return -EINVAL;
	}

	if (ops > 0u) {
		if (init > 0u) {
			pm_runtime_set_autosuspend_delay(core->dev, 0);
			pm_runtime_enable(core->dev);
		}
		ret = pm_runtime_get_sync(core->dev);
		if (ret < 0) {
			pm_runtime_put_noidle(core->dev);
			dev_err(core->dev, "pm runtime get sync failed\n");
			return ret;
		}
	} else {
		if (pm_runtime_active(core->dev)) {
			ret = pm_runtime_put_sync_suspend(core->dev);
		}
		if (init > 0u) {
			pm_runtime_disable(core->dev);
		}
	}
	udelay(100);

	return ret;
}
EXPORT_SYMBOL(bpu_core_pm_ctrl);/*PRQA S 0779*/ /*PRQA S 0307*/ /* Linux Macro */
