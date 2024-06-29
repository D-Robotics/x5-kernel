/*
 * Copyright (C) 2019 Horizon Robotics
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 *
 */

#define pr_fmt(fmt) "bpu-core: " fmt
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/of_address.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/regulator/consumer.h>
#include <linux/dma-map-ops.h>
#include <linux/reset.h>
#include <linux/platform_device.h>
#include "bpu.h"
#include "bpu_core.h"
#include "bpu_ctrl.h"

#define NAME_LEN		(10)
#define ID_LOOP_GAP		(10)
#define ID_MASK			(0xFFFFFFFFu)
#define STATE_SHIFT		(32u)
#define BYTE_MASK		(0xFFu)

static atomic_t g_bpu_core_fd_idx = ATOMIC_INIT(0);
static int32_t bpu_core_alloc_fifos(struct bpu_core *core);

static uint32_t bpu_reset_bypass = 1;
static uint32_t bpu_clock_quirk = 1;		/* default needs mclk reset */

module_param_named(reset_bypass, bpu_reset_bypass, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(reset_bypass, "Bypass bpu reset");
module_param_named(clock_quirk, bpu_clock_quirk, uint, S_IRUGO|S_IWUSR);
MODULE_PARM_DESC(clock_quirk, "bpu clock quirk setting");

/* tasklet just to do statistics work */
static void bpu_core_tasklet(unsigned long data)
{
	struct bpu_core *core = (struct bpu_core *)data;
	struct bpu_user *tmp_user;
	struct bpu_fc tmp_bpu_fc;
	uint64_t tmp_done_hw_id;
	uint32_t tmp_hw_id, err, prio;
	int32_t lost_report;
	int32_t ret;

	if (core == NULL) {
		return;
	}

	do {
		ret = kfifo_get(&core->done_hw_ids, &tmp_done_hw_id);
		if (ret == 0) {
			return;
		}

		tmp_hw_id = (uint32_t)(tmp_done_hw_id & ID_MASK);
		err = (uint32_t)(tmp_done_hw_id >> STATE_SHIFT);

		do {
			lost_report = 0;
			prio = BPU_CORE_TASK_PRIO(core, tmp_hw_id);
			if (err == 0u) {
				ret = kfifo_peek(&core->run_fc_fifo[prio], &tmp_bpu_fc);
				if (ret < 1) {
					/* maybe a spurious triggering, or delay backpard process */
					bpu_prio_trig_out(core->prio_sched);
					bpu_sched_seed_update();
					return;
				}

				/* to make sure software id matches the hardware id */
				if (tmp_bpu_fc.hw_id > tmp_hw_id) {
					if (((int32_t)tmp_bpu_fc.hw_id - (int32_t)tmp_hw_id) > (BPU_CORE_TASK_ID_MAX(core) / 2)) {
						lost_report = 1;
					} else {
						bpu_prio_trig_out(core->prio_sched);
						bpu_sched_seed_update();
						return;
					}
				} else if (tmp_bpu_fc.hw_id < tmp_hw_id) {
					if ((int32_t)tmp_hw_id - (int32_t)tmp_bpu_fc.hw_id > (BPU_CORE_TASK_ID_MAX(core) - ID_LOOP_GAP)) {
						bpu_prio_trig_out(core->prio_sched);
						bpu_sched_seed_update();
						return;
					} else {
						lost_report = 1;
					}
				}
				kfifo_skip(&core->run_fc_fifo[prio]);

				bpu_prio_trig_out(core->prio_sched);
				/* data has been no use fow hw, so clear data */
				bpu_fc_clear(&tmp_bpu_fc);
			} else {
				ret = kfifo_peek(&core->run_fc_fifo[prio], &tmp_bpu_fc);
				if (ret < 1) {
					bpu_sched_seed_update();
					dev_err(core->dev,
							"bpu core no fc bufferd, when error[%d]\n", ret);
					return;
				}
				tmp_bpu_fc.info.status = (int32_t)err;
			}

			/* update the statistics element */
			bpu_core_update(core, &tmp_bpu_fc);

			tmp_bpu_fc.info.run_c_mask = ((uint64_t)0x1 << (uint32_t)core->index);

			spin_lock(&core->host->user_spin_lock);
			tmp_user = bpu_get_user(&tmp_bpu_fc, &core->user_list);
			if (tmp_user != NULL) {
				if (tmp_user->is_alive > 0u) {
					if (kfifo_is_full(&tmp_user->done_fcs)) {
						dev_err(core->dev, "user[%d] (%d)read data too Slow\n",
								kfifo_len(&tmp_user->done_fcs), (uint32_t)(tmp_user->id & USER_MASK));
						kfifo_skip(&tmp_user->done_fcs);
					}

					ret = kfifo_in(&tmp_user->done_fcs, &tmp_bpu_fc.info, 1);/*PRQA S 4461*/ /* Linux Macro */
					if (ret < 1) {
						spin_unlock(&core->host->user_spin_lock);
						bpu_sched_seed_update();
						dev_err(core->dev, "bpu buffer bind user error\n");
						return;
					}
					wake_up_interruptible(&tmp_user->poll_wait);
				}
				tmp_user->running_task_num--;

				if ((tmp_user->running_task_num <= 0) && (tmp_user->is_alive == 0u)) {
					complete(&tmp_user->no_task_comp);
				}
			}
			spin_unlock(&core->host->user_spin_lock);

			core->running_task_num--;
			if (core->running_task_num <= 0) {
				complete(&core->no_task_comp);
			}

			if (kfifo_is_full(&core->done_fc_fifo)) {
				kfifo_skip(&core->done_fc_fifo);
			}

			ret = kfifo_in(&core->done_fc_fifo, &tmp_bpu_fc, 1);/*PRQA S 4461*/ /* Linux Macro */
			if (ret < 1) {
				bpu_sched_seed_update();
				dev_err(core->dev, "bpu buffer bind user error\n");
				return;
			}
		} while (lost_report != 0);
	} while (!kfifo_is_empty(&core->done_hw_ids));

	/* core ratio will be update by group check */
	bpu_sched_seed_update();
}

void bpu_core_push_done_id(struct bpu_core *core, uint64_t id)
{
	if ((core == NULL) || (id == 0)) {
		return;
	}

	if (kfifo_is_full(&core->done_hw_ids)) {
		kfifo_skip(&core->done_hw_ids);
	}

	kfifo_put(&core->done_hw_ids, id);
}
EXPORT_SYMBOL(bpu_core_push_done_id);

static irqreturn_t bpu_core_irq_handler(int irq, void *dev_id)
{
	struct bpu_core *core = (struct bpu_core *)dev_id;
	uint32_t tmp_hw_id, err;
	uint64_t tmp_done_hw_id;
	int32_t ret;

#ifdef CONFIG_PCIE_HOBOT_EP_AI
	if ((core->inst.reserved[4] > 0) && (core->hw_io != NULL)) {
		/* use as pac */
		ret = core->hw_io->ops.read_fc(&core->inst, &tmp_hw_id, &err);
		return IRQ_HANDLED;
	}
#endif
	if (atomic_read(&core->host->open_counter) == 0) {
		return IRQ_HANDLED;
	}

	spin_lock(&core->spin_lock);
	spin_lock(&core->hw_io_spin_lock);
	if (core->hw_io == NULL) {
		spin_unlock(&core->hw_io_spin_lock);
		spin_unlock(&core->spin_lock);
		dev_err(core->dev, "BPU HW IO has been unregister\n");
		return IRQ_HANDLED;
	}
	ret = core->hw_io->ops.read_fc(&core->inst, &tmp_hw_id, &err);
	spin_unlock(&core->hw_io_spin_lock);
	if (ret <= 0) {
		spin_unlock(&core->spin_lock);
		if (ret < 0) {
			dev_err(core->dev, "BPU read hardware core error\n");
		}
		bpu_prio_trig_out(core->prio_sched);
		return IRQ_HANDLED;
	}
	spin_unlock(&core->spin_lock);
	dev_dbg(core->dev, "BPU Core[%d] irq %d come\n", core->index, tmp_hw_id);/*PRQA S 0685*/ /*PRQA S 1294*/ /* Linux Macro */

	if (tmp_hw_id == BPU_CORE_TASK_ID_MAX(core)) {
		bpu_prio_trig_out(core->prio_sched);
		return IRQ_HANDLED;
	}

	tmp_done_hw_id = ((uint64_t)err << STATE_SHIFT) | (uint64_t)tmp_hw_id;
	bpu_core_push_done_id(core, tmp_done_hw_id);

	tasklet_schedule(&core->tasklet);
	bpu_prio_trig_out(core->prio_sched);

	return IRQ_HANDLED;
}

static long bpu_core_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	struct bpu_user *user = (struct bpu_user *)filp->private_data;
	struct bpu_core *core = (struct bpu_core *)user->host;
	struct bpu_iommu_map tmp_iommu_map;
	struct bpu_fc_run_time tmp_fc_run_time;
	uint32_t ratio;
	uint16_t cap;
	int16_t level;
	uint32_t limit;
	uint64_t tmp_clock, tmp_type, tmp_est_time;
	uint8_t type;
#if defined(CONFIG_PM_DEVFREQ) && defined(CONFIG_DEVFREQ_THERMAL)
	int32_t i;
#endif
	int32_t ret = 0;

	switch (cmd) {
	case BPU_GET_RATIO:/*PRQA S 0591*/ /*PRQA S 4513*/ /* Linux Ioctl CMD */
		ratio = bpu_core_ratio(core);
		if (copy_to_user((void __user *)arg, &ratio, _IOC_SIZE(cmd)) != 0) {/*PRQA S 4491*/ /* Linux Macro */
			dev_err(core->dev, "copy data to userspace failed\n");
			return -EFAULT;
		}
		break;
	case BPU_GET_CAP:/*PRQA S 0591*/ /*PRQA S 4513*/ /* Linux Ioctl CMD */
		/* get the lowest prio fifo size to user */
		ret = mutex_lock_interruptible(&core->mutex_lock);
		if (ret < 0) {
			return ret;
		}
		if (bpu_core_is_online(core) || (core->hotplug > 0u)) {
			cap = (uint16_t)bpu_core_avl_cap(core);
		} else {
			cap = 0;
		}
		mutex_unlock(&core->mutex_lock);
		if (copy_to_user((void __user *)arg, &cap, _IOC_SIZE(cmd)) != 0) {/*PRQA S 4491*/ /* Linux Macro */
			dev_err(core->dev, "copy data to userspace failed\n");
			return -EFAULT;
		}
		break;
	case BPU_SET_POWER:/*PRQA S 0591*/ /*PRQA S 4513*/ /* Linux Ioctl CMD */
		level = 0;
		if (copy_from_user(&level, (void __user *)arg, _IOC_SIZE(cmd)) != 0) {/*PRQA S 4491*/ /* Linux Macro */
			dev_err(core->dev, "copy data from userspace failed\n");
			return -EFAULT;
		}
		if (level <= 0) {
			ret = mutex_lock_interruptible(&core->mutex_lock);
			if (ret < 0) {
				return ret;
			}
			ret = bpu_core_disable(core);
			mutex_unlock(&core->mutex_lock);
			if (ret != 0) {
				dev_err(core->dev, "Disable BPU core%d failed\n", core->index);
				return ret;
			}
		} else {
			ret = mutex_lock_interruptible(&core->mutex_lock);
			if (ret < 0) {
				return ret;
			}
			ret = bpu_core_enable(core);
			mutex_unlock(&core->mutex_lock);
			if (ret != 0) {
				dev_err(core->dev, "Enable BPU core%d failed\n", core->index);
				return ret;
			}
		}
		break;
	case BPU_SET_FREQ_LEVEL:/*PRQA S 0591*/ /*PRQA S 4513*/ /* Linux Ioctl CMD */
		level = 0;
		if (copy_from_user(&level, (void __user *)arg, _IOC_SIZE(cmd)) != 0) {/*PRQA S 4491*/ /* Linux Macro */
			dev_err(core->dev, "copy data from userspace failed\n");
			return -EFAULT;
		}
		ret = mutex_lock_interruptible(&core->mutex_lock);
		if (ret < 0) {
			return ret;
		}
		ret = bpu_core_set_freq_level(core, level);
		mutex_unlock(&core->mutex_lock);
		if (ret != 0) {
			dev_err(core->dev, "Set BPU core%d freq level(%d)failed\n",
					core->index, level);
			return ret;
		}
		break;
	case BPU_GET_FREQ_LEVEL:/*PRQA S 0591*/ /*PRQA S 4513*/ /* Linux Ioctl CMD */
		level = 0;
#if defined(CONFIG_PM_DEVFREQ) && defined(CONFIG_DEVFREQ_THERMAL)

		if (core->dvfs != NULL) {
			for (i = 0; i < (int32_t)core->dvfs->level_num; i++) {
				if (core->dvfs->rate == core->dvfs->profile.freq_table[i]) {
					level = (int16_t)(i - (int32_t)core->dvfs->level_num + 1);
				}
			}
		}
#endif
		if (copy_to_user((void __user *)arg, &level, _IOC_SIZE(cmd)) != 0) {/*PRQA S 4491*/ /* Linux Macro */
			dev_err(core->dev, "copy data to userspace failed\n");
			return -EFAULT;
		}
		break;
	case BPU_GET_FREQ_LEVEL_NUM:/*PRQA S 0591*/ /*PRQA S 4513*/ /* Linux Ioctl CMD */
		level = 1;
#if defined(CONFIG_PM_DEVFREQ) && defined(CONFIG_DEVFREQ_THERMAL)
		if (core->dvfs != NULL) {
			level = (int16_t)core->dvfs->level_num;
		}
#endif
		if (copy_to_user((void __user *)arg, &level, _IOC_SIZE(cmd)) != 0) {/*PRQA S 4491*/ /* Linux Macro */
			dev_err(core->dev, "copy data to userspace failed\n");
			return -EFAULT;
		}
		break;
	case BPU_SET_CLK:/*PRQA S 0591*/ /*PRQA S 4513*/ /* Linux Ioctl CMD */
		tmp_clock = 0;
		if (copy_from_user(&tmp_clock, (void __user *)arg, _IOC_SIZE(cmd)) != 0) {/*PRQA S 4491*/ /* Linux Macro */
			dev_err(core->dev, "copy data from userspace failed\n");
			return -EFAULT;
		}
		ret = mutex_lock_interruptible(&core->mutex_lock);
		if (ret < 0) {
			return ret;
		}
		ret = bpu_core_ext_ctrl(core, SET_CLK, &tmp_clock);
		mutex_unlock(&core->mutex_lock);
		if (ret != 0) {
			dev_err(core->dev, "Set BPU core%d clock(%lld)failed\n",
					core->index, tmp_clock);
			return ret;
		}
		break;
	case BPU_GET_CLK:/*PRQA S 0591*/ /*PRQA S 4513*/ /* Linux Ioctl CMD */
		ret = bpu_core_ext_ctrl(core, GET_CLK, &tmp_clock);
		if (ret != 0) {
			dev_err(core->dev, "Get BPU core%d clock failed\n",
					core->index);
			return ret;
		}
		if (copy_to_user((void __user *)arg, &tmp_clock, _IOC_SIZE(cmd)) != 0) {/*PRQA S 4491*/ /* Linux Macro */
			dev_err(core->dev, "copy data to userspace failed\n");
			return -EFAULT;
		}
		break;
	case BPU_RESET:/*PRQA S 0591*/ /*PRQA S 4513*/ /* Linux Ioctl CMD */
		ret = mutex_lock_interruptible(&core->mutex_lock);
		if (ret < 0) {
			return ret;
		}
		ret = bpu_core_reset(core);
		mutex_unlock(&core->mutex_lock);
		break;
	case BPU_SET_LIMIT:/*PRQA S 0591*/ /*PRQA S 4513*/ /* Linux Ioctl CMD */
		if (copy_from_user(&limit, (void __user *)arg, _IOC_SIZE(cmd)) != 0) {/*PRQA S 4491*/ /* Linux Macro */
			dev_err(core->dev, "%s: copy data failed from userspace\n", __func__);
			return -EFAULT;
		}
		ret = bpu_core_set_limit(core, (int32_t)limit);
		if (ret != 0) {
			dev_err(core->dev, "BPU core set prio limit failed\n");
		}
		break;
	case BPU_IOMMU_MAP:/*PRQA S 0591*/ /*PRQA S 4513*/ /* Linux Ioctl CMD */
		if (copy_from_user(&tmp_iommu_map, (void __user *)arg, _IOC_SIZE(cmd)) != 0) {/*PRQA S 4491*/ /* Linux Macro */
			dev_err(core->dev, "%s: copy data failed from userspace(iommu map)\n", __func__);
			return -EFAULT;
		}

#ifdef CONFIG_ARCH_HOBOT_X5
		ret = mutex_lock_interruptible(&core->mutex_lock);
		if (ret < 0) {
			return ret;
		}

		if (core->hw_enabled == 0u) {
			mutex_unlock(&core->mutex_lock);
			dev_err(core->dev, "Try to mmap when bpu has been powered off\n");
			return -EFAULT;
		}

		if (core->dev->iommu != NULL) {
			ret = ion_iommu_map_ion_phys(core->dev, tmp_iommu_map.raw_addr,
					tmp_iommu_map.size, (dma_addr_t *)&tmp_iommu_map.map_addr, 0);
			if(ret < 0) {
				mutex_unlock(&core->mutex_lock);
				dev_err(core->dev, "BPU core map iommu failed %d [0x%llx, 0x%llx]\n",
						ret, tmp_iommu_map.raw_addr, tmp_iommu_map.size);
				return ret;
			}
		} else {
			tmp_iommu_map.map_addr = tmp_iommu_map.raw_addr;
		}
		mutex_unlock(&core->mutex_lock);
#else
		tmp_iommu_map.map_addr = tmp_iommu_map.raw_addr;
#endif

		if (copy_to_user((void __user *)arg, &tmp_iommu_map, _IOC_SIZE(cmd)) != 0) {/*PRQA S 4491*/ /* Linux Macro */
			dev_err(core->dev, "copy data to userspace failed(iommu map)\n");
			return -EFAULT;
		}
		break;
	case BPU_IOMMU_UNMAP:/*PRQA S 0591*/ /*PRQA S 4513*/ /* Linux Ioctl CMD */
		if (copy_from_user(&tmp_iommu_map, (void __user *)arg, _IOC_SIZE(cmd)) != 0) {/*PRQA S 4491*/ /* Linux Macro */
			dev_err(core->dev, "%s: copy data failed from userspace(iommu unmap)\n", __func__);
			return -EFAULT;
		}
#ifdef CONFIG_ARCH_HOBOT_X5
		ret = mutex_lock_interruptible(&core->mutex_lock);
		if (ret < 0) {
			return ret;
		}
		if ((core->dev->iommu != NULL) && (core->hw_enabled > 0u)) {
			ion_iommu_unmap_ion_phys(core->dev, (dma_addr_t)tmp_iommu_map.map_addr, (size_t)tmp_iommu_map.size);
		}
		mutex_unlock(&core->mutex_lock);
#endif
		break;
	case BPU_CORE_TYPE:/*PRQA S 0591*/ /*PRQA S 4513*/ /* Linux Ioctl CMD */
		ret = bpu_core_ext_ctrl(core, CORE_TYPE, &tmp_type);
		if (ret != 0) {
			dev_err(core->dev, "Get BPU core%d type failed\n",
					core->index);
			return ret;
		}
		type = (uint8_t) (tmp_type & BYTE_MASK);
		if (copy_to_user((void __user *)arg, &type, _IOC_SIZE(cmd)) != 0) {/*PRQA S 4491*/ /* Linux Macro */
			dev_err(core->dev, "copy data to userspace failed\n");
			return -EFAULT;
		}
		break;
	case BPU_FC_RUN_TIME:/*PRQA S 0591*/ /*PRQA S 4513*/ /* Linux Ioctl CMD */
		if (copy_from_user(&tmp_fc_run_time, (void __user *)arg, _IOC_SIZE(cmd))) {/*PRQA S 4491*/ /* Linux Macro */
			pr_err("%s: copy data failed from userspace\n", __func__);
			return -EFAULT;
		}

		ret = bpu_check_fc_run_time(core, &tmp_fc_run_time);
		if (ret < 0) {
			tmp_fc_run_time.run_time = 0;
		}

		if (copy_to_user((void __user *)arg, &tmp_fc_run_time, _IOC_SIZE(cmd)) != 0) {/*PRQA S 4491*/ /* Linux Macro */
			pr_err("copy data to userspace failed\n");
			return -EFAULT;
		}
		break;
	case BPU_EST_TIME:/*PRQA S 0591*/ /*PRQA S 4513*/ /* Linux Ioctl CMD */
		/* user use ext_time to set prio level */
		if (copy_from_user(&tmp_est_time, (void __user *)arg, _IOC_SIZE(cmd))) {/*PRQA S 4491*/ /* Linux Macro */
			pr_err("%s: copy data failed from userspace\n", __func__);
			return -EFAULT;
		}

		tmp_est_time = bpu_core_bufferd_time(core, (uint32_t)tmp_est_time);

		if (copy_to_user((void __user *)arg, &tmp_est_time, _IOC_SIZE(cmd)) != 0) {/*PRQA S 4491*/ /* Linux Macro */
			pr_err("copy data to userspace failed\n");
			return -EFAULT;
		}
		break;
	default:
		dev_err(core->dev, "%s: BPU invalid ioctl argument\n", __func__);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static unsigned int bpu_core_poll(struct file *filp, poll_table *wait)
{
	struct bpu_user *user = (struct bpu_user *)filp->private_data;
	uint32_t mask = 0;

	poll_wait(filp, &user->poll_wait, wait);
	if (mutex_lock_interruptible(&user->mutex_lock) < 0) {
		return mask;
	}

	if (kfifo_len(&user->done_fcs)) {
		mask |= POLLIN | POLLRDNORM;
	}

	mutex_unlock(&user->mutex_lock);

	return mask;
}

static ssize_t bpu_core_read(struct file *filp,
		char __user *buf, size_t len, loff_t *f_pos)
{
	struct bpu_user *user = (struct bpu_user *)filp->private_data;
	struct bpu_core *core = (struct bpu_core *)user->host;

	return bpu_read_with_user(core, user, buf, len);
}

static ssize_t bpu_core_write(struct file *filp,
		const char __user *buf, size_t len, loff_t *f_pos)
{
	struct bpu_user *user = (struct bpu_user *)filp->private_data;
	struct bpu_core *core = (struct bpu_core *)user->host;

	return bpu_write_with_user(core, user, buf, len);
}

int32_t bpu_core_hw_io_resource_alloc(struct bpu_core *core)
{
	uint32_t task_range_size = 0;
	unsigned long flags;
	int32_t i, prio_num = 0;

	if (core == NULL) {
		return -ENODEV;
	}

	if (core->inst.task_range_size != 0) {
		return 0;
	}

	prio_num = BPU_CORE_PRIO_NUM(core);
	/* first open init something files */
	spin_lock_irqsave(&core->hw_io_spin_lock, flags);
	if (core->hw_io != NULL) {
		task_range_size = core->hw_io->task_size * core->hw_io->task_capacity;
		if (core->hw_io->arch == ARCH_BAYES && (core->inst.reserved[3] == 0)) {
			core->inst.reserved[3] = (uint64_t)kmalloc(sizeof(struct semaphore), GFP_ATOMIC);
			if ((void *)core->inst.reserved[3] != NULL) {
				sema_init((struct semaphore *)core->inst.reserved[3], 0);
			}
		}
	} else {
		spin_unlock_irqrestore(&core->hw_io_spin_lock, flags);
		dev_err(core->dev, "No BPU HW IO for Core to use!\n");
		return -ENODEV;
	}
	spin_unlock_irqrestore(&core->hw_io_spin_lock, flags);

	if (task_range_size > 0) {
		core->inst.task_range_size = task_range_size;
		for (i = 0u; i < prio_num; i++) {
			core->inst.task_base[i] = dma_alloc_coherent(core->dev,
					task_range_size + PAGE_SIZE, &core->inst.task_phy_base[i], GFP_KERNEL);
			if (core->inst.task_base[i] == NULL) {
				bpu_core_hw_io_resource_free(core);
				dev_err(core->dev, "bpu core alloc task mem failed\n");
				return -ENOMEM;
			}
		}
	}

	return 0;
}
EXPORT_SYMBOL(bpu_core_hw_io_resource_alloc);

void bpu_core_hw_io_resource_free(struct bpu_core *core)
{
	unsigned long flags;
	int32_t i;

	if (core == NULL) {
		return;
	}

	if (core->inst.task_range_size == 0) {
		return;
	}

	for (i = 0; i < BPU_CORE_PRIO_NUM(core); i++) {
		if (core->inst.task_base[i] != NULL) {
			dma_free_coherent(core->dev, core->inst.task_range_size + PAGE_SIZE,
					(void *)core->inst.task_base[i], core->inst.task_phy_base[i]);
			core->inst.task_base[i] = NULL;
		}
	}

	spin_lock_irqsave(&core->hw_io_spin_lock, flags);
	if (core->inst.reserved[3] != 0) {
		kfree((void *)core->inst.reserved[3]);
		core->inst.reserved[3] = 0;
	}

	spin_unlock_irqrestore(&core->hw_io_spin_lock, flags);
	core->inst.task_range_size = 0;
}
EXPORT_SYMBOL(bpu_core_hw_io_resource_free);

static int32_t bpu_core_check_to_open(struct bpu_core *core)
{
	int32_t ret = 0, prio_num = 0;
	uint32_t i;

	if (atomic_read(&core->open_counter) == 0) {
		if (bpu_core_alloc_fifos(core) < 0) {
			dev_err(core->dev, "No BPU HW IO for Core to use!\n");
			return -ENODEV;
		}

		ret = bpu_core_enable(core);
		if (ret != 0) {
			dev_err(core->dev, "Can't bpu core hw enable failed\n");
			return ret;
		}
		devfreq_resume_device(core->dvfs->devfreq);
		ret = bpu_core_hw_io_resource_alloc(core);
		if (ret < 0) {
			return ret;
		}

		core->prio_sched = bpu_prio_init(core, CONFIG_BPU_PRIO_NUM);
		if (core->prio_sched == NULL) {
			dev_err(core->dev, "Init bpu core prio sched failed\n");
			return -EINVAL;
		}

		prio_num = BPU_CORE_PRIO_NUM(core);
		for (i = 0; i < prio_num; i++) {
			atomic_set(&core->hw_id_counter[i], 1);
			core->buffered_time[i] = 0;
		}

		kfifo_reset(&core->done_hw_ids);
		core->inst.task_buf_limit = 0;
		tasklet_init(&core->tasklet, bpu_core_tasklet, (unsigned long)core);
	} else {
		if (core->hw_enabled == 0u) {
			ret = bpu_core_enable(core);
			if (ret != 0) {
				dev_err(core->dev, "Can't bpu core hw enable failed\n");
				return ret;
			}
			devfreq_resume_device(core->dvfs->devfreq);
		}
	}

	return ret;
}

/* break E1, no return logic inside the function */
static void bpu_core_clear_reset_run_fifos(struct bpu_core *core)
{
	struct bpu_fc tmp_bpu_fc;
	unsigned long flags;
	int32_t i, prio_num;
	int32_t ret;

	spin_lock_irqsave(&core->spin_lock, flags);
	prio_num = BPU_CORE_PRIO_NUM(core);
	for (i = 0; i < prio_num; i++) {
		while (!kfifo_is_empty(&core->run_fc_fifo[i])) {
			ret = kfifo_get(&core->run_fc_fifo[i], &tmp_bpu_fc);
			if (ret < 1) {
				continue;
			}

			spin_unlock_irqrestore(&core->spin_lock, flags);
			bpu_fc_clear(&tmp_bpu_fc);
			spin_lock_irqsave(&core->spin_lock, flags);
		}
		kfifo_reset(&core->run_fc_fifo[i]);
	}
	spin_unlock_irqrestore(&core->spin_lock, flags);
}

/* break E1, no need error return inside the function */
static void bpu_core_make_sure_close(struct bpu_core *core)
{
	unsigned long flags;
	int32_t ret;
#ifdef CONFIG_ARCH_HOBOT_X5
	int32_t i, prio_num;
#endif

	/* release the real bpu core */
	bpu_prio_exit(core->prio_sched);
	core->prio_sched = NULL;

#ifdef CONFIG_ARCH_HOBOT_X5
	if (core->dev->iommu != NULL) {
		ret = ion_iommu_reset_mapping(core->dev);
		if (ret != 0) {
			dev_err(core->dev, "BPU core(%d) iommu reset failed\n", core->index);
		}
	}

	/* after iommu map reset, task map base not valid */
	prio_num = BPU_CORE_PRIO_NUM(core);
	for (i = 0; i < prio_num; i++) {
		core->inst.task_map_base[i] = 0;
	}
#endif
	ret = bpu_core_disable(core);
	if (ret != 0) {
		dev_err(core->dev, "BPU core disable failed\n");
	}
	tasklet_kill(&core->tasklet);

	bpu_core_hw_io_resource_free(core);

	spin_lock_irqsave(&core->spin_lock, flags);
	core->p_run_time = 0;
	core->ratio = 0;
	spin_unlock_irqrestore(&core->spin_lock, flags);

	bpu_core_clear_reset_run_fifos(core);

 	// FIXME: sunrise5 needs bpu core reset, the last close operation needs to reset bpu ip
	if (!bpu_reset_bypass) {
		dev_info(core->dev, "Force bpu reset during %s\n", __func__);

		ret = bpu_core_reset_quirk(core);
		if (ret < 0)
			dev_err(core->dev, "bpu core reset failed\n");
	}

	if (bpu_clock_quirk) {
		//bpu_core_clk_off_quirk(core);
		bpu_core_power_off_quirk(core);
	}
}

static struct bpu_user *bpu_core_create_user(struct bpu_core *core)
{
	struct bpu_user *user;
	unsigned long flags;
	int32_t ret;

	user = (struct bpu_user *)kzalloc(sizeof(struct bpu_user), GFP_KERNEL);
	if (user == NULL) {
		dev_err(core->dev, "Can't bpu user mem\n");
		return NULL;
	}

	user->id = (uint64_t)((uint32_t)task_pid_nr(current->group_leader)
				| ((uint64_t)prandom_u32_max(255) << USER_RANDOM_SHIFT));

	/* init fifo which report to userspace */
	ret = kfifo_alloc(&user->done_fcs, BPU_CORE_RECORE_NUM, GFP_KERNEL);
	if (ret != 0) {
		kfree((void *)user);
		dev_err(core->dev, "Can't bpu user fifo buffer\n");
		return NULL;
	}

	init_waitqueue_head(&user->poll_wait);
	spin_lock_irqsave(&core->spin_lock, flags);
	spin_lock(&core->host->user_spin_lock);
	list_add((struct list_head *)user, &core->user_list);
	spin_unlock(&core->host->user_spin_lock);
	spin_unlock_irqrestore(&core->spin_lock, flags);
	user->host = (void *)core;
	spin_lock_init(&user->spin_lock);/*PRQA S 3334*/ /* Linux Macro */
	mutex_init(&user->mutex_lock);/*PRQA S 3334*/ /* Linux Macro */
	init_completion(&user->no_task_comp);

	return user;
}

/* break E1, no error return logic inside the function */
static void bpu_core_discard_user(struct bpu_user *user)
{
	spin_lock(&((struct bpu_core *)user->host)->host->user_spin_lock);
	list_del((struct list_head *)user);
	spin_unlock(&((struct bpu_core *)user->host)->host->user_spin_lock);
	kfifo_free(&user->done_fcs);
	kfree((void *)user);
}

static int bpu_core_open(struct inode *inode, struct file *filp)
{
	struct bpu_core *core =
		(struct bpu_core *)container_of(filp->private_data, struct bpu_core, miscdev);/*PRQA S 0497*/ /* Linux Macro */
	struct bpu_user *user;
	int32_t ret;

	mutex_lock(&core->mutex_lock);
	ret = bpu_core_check_to_open(core);
	if (ret != 0) {
		mutex_unlock(&core->mutex_lock);
		return ret;
	}

	user = bpu_core_create_user(core);
	if (user == NULL) {
		mutex_unlock(&core->mutex_lock);
		return -ENOMEM;
	}

	user->p_file_private = &filp->private_data;
	user->is_alive = 1;
	user->running_task_num = 0;
	/* replace user the private to store user */
	filp->private_data = (void *)user;

	atomic_inc(&core->open_counter);
	mutex_unlock(&core->mutex_lock);
	return 0;
}

/* break E1, no error return logic inside the function */
static void bpu_core_wait_user_tasks(struct bpu_core *core,
		struct bpu_user *user)
{
	int32_t core_work_state = 1;
	int32_t ret;

	user->is_alive = 0;
	/* wait user running fc done */
	while((user->running_task_num > 0) && (core->hw_enabled > 0u)) {
		if (core->hw_io->ops.cmd != NULL) {
			(void)core->hw_io->ops.cmd(&core->inst, (uint32_t)UPDATE_STATE);
			core_work_state = core->hw_io->ops.cmd(&core->inst, (uint32_t)WORK_STATE);
		}

		/* timeout to prevent bpu hung make system hung*/
		if((wait_for_completion_timeout(&user->no_task_comp, HZ) == 0u)
				&& (core_work_state > 0)) {
			if (core->hw_io->ops.cmd != NULL) {
				(void)core->hw_io->ops.cmd(&core->inst, (uint32_t)UPDATE_STATE);
				ret = core->hw_io->ops.cmd(&core->inst, (uint32_t)WORK_STATE);
				if (ret == core_work_state) {
					/* if states between wait are same, break to not wait*/
					break;
				}
			}
			user->running_task_num--;
		} else {
			break;
		}
	}
}

static int bpu_core_release(struct inode *inode, struct file *filp)
{
	struct bpu_user *user = (struct bpu_user *)filp->private_data;
	struct bpu_core *core = (struct bpu_core *)user->host;
	unsigned long flags;

	bpu_core_wait_user_tasks(core, user);

	mutex_lock(&core->mutex_lock);
	atomic_dec(&core->open_counter);

	if (atomic_read(&core->open_counter) == 0) {
		devfreq_suspend_device(core->dvfs->devfreq);
		bpu_core_make_sure_close(core);
	}

	spin_lock_irqsave(&core->spin_lock, flags);
	bpu_core_discard_user(user);
	filp->private_data = NULL;
	spin_unlock_irqrestore(&core->spin_lock, flags);
	mutex_unlock(&core->mutex_lock);

	return 0;
}

static const struct file_operations bpu_core_fops = {
	.owner		= THIS_MODULE,
	.open		= bpu_core_open,
	.release	= bpu_core_release,
	.read		= bpu_core_read,
	.write		= bpu_core_write,
	.poll		= bpu_core_poll,
	.unlocked_ioctl = bpu_core_ioctl,
	.compat_ioctl = bpu_core_ioctl,
};

#ifdef CONFIG_PM
static int bpu_core_suspend(struct device *dev)
{
	struct bpu_core *core = (struct bpu_core *)dev_get_drvdata(dev);
	uint16_t tmp_hw_state;
	int32_t ret;

	mutex_lock(&core->mutex_lock);
	tmp_hw_state = core->hw_enabled;
	ret = bpu_core_power_off_quirk(core);
	if (ret != 0) {
		mutex_unlock(&core->mutex_lock);
		dev_err(dev, "BPU core%d suspend failed\n", core->index);
		return ret;
	}
	core->hw_enabled = tmp_hw_state;
	mutex_unlock(&core->mutex_lock);

	return 0;
}

static int bpu_core_resume(struct device *dev)
{
	struct bpu_core *core = (struct bpu_core *)dev_get_drvdata(dev);
	int32_t ret;

	mutex_lock(&core->mutex_lock);
	if (core->hw_enabled == 0u) {
		mutex_unlock(&core->mutex_lock);

		return 0;
	}

	core->hw_enabled = 0u;
	ret = bpu_core_enable(core);
	if (ret != 0) {
		mutex_unlock(&core->mutex_lock);
		dev_err(dev, "BPU core%d resume failed\n", core->index);
		return ret;
	}

	ret = bpu_core_process_recover(core);
	if (ret != 0) {
		mutex_unlock(&core->mutex_lock);
		dev_err(dev, "BPU core%d recovery failed\n", core->index);
		return ret;
	}
	mutex_unlock(&core->mutex_lock);
	bpu_prio_trig_out(core->prio_sched);

	return ret;
}

static SIMPLE_DEV_PM_OPS(bpu_core_pm_ops,
			 bpu_core_suspend, bpu_core_resume);
#endif

static int32_t bpu_core_alloc_fifos(struct bpu_core *core)
{
	uint32_t i, j, task_cap;
	unsigned long flags;
	int32_t prio_num, ret;

	if (core == NULL) {
		pr_err("NO BPU Core to alloc fifos\n");
		return -ENODEV;
	}

	if (core->fifo_created > 0) {
		return 0;
	}

	prio_num = BPU_CORE_PRIO_NUM(core);
	task_cap = 0;
	spin_lock_irqsave(&core->hw_io_spin_lock, flags);
	if (core->hw_io != NULL) {
		task_cap = core->hw_io->task_capacity;
	} else {
		spin_unlock_irqrestore(&core->hw_io_spin_lock, flags);
		return -ENODEV;
	}
	spin_unlock_irqrestore(&core->hw_io_spin_lock, flags);

	if ((prio_num == 0) || (task_cap == 0)) {
		return -ENODEV;
	}

	for (i = 0; i < prio_num; i++) {
		ret = kfifo_alloc(&core->run_fc_fifo[i], task_cap, GFP_KERNEL);
		if (ret != 0) {
			for (j = 0; j < i; j++) {
				kfifo_free(&core->run_fc_fifo[j]);
			}
			dev_err(core->dev, "Can't bpu core run fifo buffer\n");
			return ret;
		}
	}

	/* done fc fifo mainly for debug */
	ret = kfifo_alloc(&core->done_fc_fifo, BPU_CORE_RECORE_NUM, GFP_KERNEL);
	if (ret != 0) {
		for (i = 0; i < prio_num; i++) {
			kfifo_free(&core->run_fc_fifo[i]);
		}
		dev_err(core->dev, "Can't bpu core done fifo buffer\n");
		return ret;
	}

	core->fifo_created = 1;
	return ret;
}

/* break E1, no need error return logic inside the function */
static void bpu_core_free_fifos(struct bpu_core *core)
{
	struct bpu_fc tmp_bpu_fc;
	int32_t i, prio_num;
	int32_t ret;

	if (core == NULL) {
		return;
	}

	if (core->fifo_created == 0) {
		return;
	}

	kfifo_free(&core->done_fc_fifo);
	prio_num = BPU_CORE_PRIO_NUM(core);
	for (i = 0; i < prio_num; i++) {
		while (!kfifo_is_empty(&core->run_fc_fifo[i])) {
			ret = kfifo_get(&core->run_fc_fifo[i], &tmp_bpu_fc);
			if (ret < 1) {
				continue;
			}
			bpu_fc_clear(&tmp_bpu_fc);
		}
		kfifo_free(&core->run_fc_fifo[i]);
	}

	core->fifo_created = 0;
}

static void bpu_core_parse_power_dts(struct platform_device *pdev, struct bpu_core *core)
{
	core->regulator = devm_regulator_get_optional(&pdev->dev, "cnn");
	if (IS_ERR(core->regulator)) {
		/* some platform not has regulator, so just report error info */
		core->regulator = NULL;
		dev_dbg(&pdev->dev, "Can't get bpu core regulator\n");/*PRQA S 0685*/ /*PRQA S 1294*/ /* Linux Macro */
	}

	core->aclk = devm_clk_get(&pdev->dev, "cnn_aclk");
	if (IS_ERR(core->aclk) || (core->aclk == NULL)) {
		/* some platform not has aclk, so just report error info */
		core->aclk = NULL;
		dev_dbg(&pdev->dev, "Can't get bpu core aclk\n");/*PRQA S 0685*/ /*PRQA S 1294*/ /* Linux Macro */
	}

	core->mclk = devm_clk_get(&pdev->dev, "cnn_mclk");
	if (IS_ERR(core->mclk) || (core->mclk == NULL)) {
		/* some platform not has mclk, so just report error info */
		core->mclk = NULL;
		dev_dbg(&pdev->dev, "Can't get bpu core mclk\n");/*PRQA S 0685*/ /*PRQA S 1294*/ /* Linux Macro */
	}
	// if (core->mclk != NULL) {
	// 	clk_prepare_enable(core->mclk);
	// 	clk_disable_unprepare(core->mclk);
	// }
	core->pe0clk = devm_clk_get(&pdev->dev, "pe0_mclk");
	if (IS_ERR(core->pe0clk) || (core->pe0clk == NULL)) {
		/* some platform not has mclk, so just report error info */
		core->pe0clk = NULL;
		dev_dbg(&pdev->dev, "Can't get bpu core pe0clk\n");/*PRQA S 0685*/ /*PRQA S 1294*/ /* Linux Macro */
	}
	if (core->pe0clk != NULL) {
		clk_prepare_enable(core->pe0clk);
		clk_disable_unprepare(core->pe0clk);
	}
	core->pe1clk = devm_clk_get(&pdev->dev, "pe1_mclk");
	if (IS_ERR(core->pe1clk) || (core->pe1clk == NULL)) {
		/* some platform not has mclk, so just report error info */
		core->pe1clk = NULL;
		dev_dbg(&pdev->dev, "Can't get bpu core pe1clk\n");/*PRQA S 0685*/ /*PRQA S 1294*/ /* Linux Macro */
	}
	if (core->pe1clk != NULL) {
		clk_prepare_enable(core->pe1clk);
		clk_disable_unprepare(core->pe1clk);
	}
	core->pe2clk = devm_clk_get(&pdev->dev, "pe2_mclk");
	if (IS_ERR(core->pe2clk) || (core->pe2clk == NULL)) {
		/* some platform not has mclk, so just report error info */
		core->pe2clk = NULL;
		dev_dbg(&pdev->dev, "Can't get bpu core pe2clk\n");/*PRQA S 0685*/ /*PRQA S 1294*/ /* Linux Macro */
	}
	if (core->pe2clk != NULL) {
		clk_prepare_enable(core->pe2clk);
		clk_disable_unprepare(core->pe2clk);
	}
	core->pe3clk = devm_clk_get(&pdev->dev, "pe3_mclk");
	if (IS_ERR(core->pe3clk) || (core->pe3clk == NULL)) {
		/* some platform not has mclk, so just report error info */
		core->pe3clk = NULL;
		dev_dbg(&pdev->dev, "Can't get bpu core pe3clk\n");/*PRQA S 0685*/ /*PRQA S 1294*/ /* Linux Macro */
	}
	if (core->pe3clk != NULL) {
		clk_prepare_enable(core->pe3clk);
		clk_disable_unprepare(core->pe3clk);
	}

	core->rst = devm_reset_control_get(&pdev->dev, "cnn_rst");
	if (IS_ERR(core->rst)) {
		/* some platform not has rst, so just report error info */
		core->rst = NULL;
		dev_dbg(&pdev->dev, "Can't get bpu core rst\n");/*PRQA S 0685*/ /*PRQA S 1294*/ /* Linux Macro */
	}
}

static int32_t bpu_core_parse_dts(struct platform_device *pdev, struct bpu_core *core)
{
	struct device_node *np;
	struct resource *resource;
	int32_t ret;

	if (core == NULL) {
		pr_err("NO BPU Core parse DeviceTree\n");
		return -ENODEV;
	}

	if (pdev == NULL) {
		pr_err("BPU Core not Bind Device\n");
		return -ENODEV;
	}

	np = pdev->dev.of_node;
	core->dev = &pdev->dev;

	resource = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (resource == NULL) {
		dev_err(&pdev->dev, "Can't get bpu core resource error\n");
		return -ENODEV;
	}

	core->inst.base = devm_ioremap_resource(&pdev->dev, resource);
	if (IS_ERR(core->inst.base)) {
		dev_err(&pdev->dev, "Can't get bpu core resource failed\n");
		return (int32_t)PTR_ERR(core->inst.base);
	}

	ret = of_property_read_u32(np, "cnn-id", &core->index);/*PRQA S 0432*/ /* Value checked no error */
	if (ret < 0) {
		dev_err(&pdev->dev, "Can't get bpu core index\n");
		return ret;
	}

	ret = of_property_read_u32(np, "power_delay", &core->power_delay);/*PRQA S ALL*/
	if (ret < 0) {
		dev_info(&pdev->dev, "Can't get bpu power delay, use default\n");
		core->power_delay = 600;
	}

	bpu_core_parse_power_dts(pdev, core);

	core->irq = irq_of_parse_and_map(np, 0);
	if (core->irq <= 0u) {
		dev_err(&pdev->dev, "Can't find bpu core irq\n");
		return -EFAULT;
	}

	return 0;
}

static int bpu_core_probe(struct platform_device *pdev)
{
	struct bpu_core *core;
	char name[NAME_LEN];
	int32_t ret;

	core = (struct bpu_core *)kzalloc(sizeof(struct bpu_core), GFP_KERNEL);
	if (core == NULL) {
		dev_err(&pdev->dev, "Can't alloc bpu core mem\n");
		return -ENOMEM;
	}

	core->dev = &pdev->dev;

	ret = bpu_core_pm_ctrl(core, 1, 1);
	if (ret != 0) {
		kfree(core);
		dev_err(&pdev->dev, "BPU Core PM enable failed when probe!\n");
		return ret;
	}

	mutex_init(&core->mutex_lock);/*PRQA S 3334*/ /* Linux Macro */
	spin_lock_init(&core->spin_lock);/*PRQA S 3334*/ /* Linux Macro */
	spin_lock_init(&core->hw_io_spin_lock);/*PRQA S 3334*/ /* Linux Macro */
	INIT_LIST_HEAD(&core->user_list);

	ret = bpu_core_parse_dts(pdev, core);
	if (ret != 0) {
		(void)bpu_core_pm_ctrl(core, 1, 0);
		kfree(core);
		dev_err(&pdev->dev, "BPU Core parse dts failed\n");
		return ret;
	}

	(void)bpu_core_alloc_fifos(core);

	INIT_KFIFO(core->done_hw_ids);
	ret = devm_request_irq(&pdev->dev, core->irq, bpu_core_irq_handler,
			0, NULL, core);
	if (ret != 0) {
		bpu_core_free_fifos(core);
		(void)bpu_core_pm_ctrl(core, 1, 0);
		dev_err(&pdev->dev, "request '%d' for bpu core failed with %d\n",
			core->irq, ret);
		kfree(core);
		return ret;
	}

	disable_irq(core->irq);

	ret = snprintf(name, NAME_LEN, "bpu_core%d", atomic_fetch_add(1, &g_bpu_core_fd_idx));
	if (ret <= 0) {
		dev_err(&pdev->dev, "BPU core%d name create failed\n", core->index);
	}

	core->miscdev.minor	= MISC_DYNAMIC_MINOR;
	core->miscdev.name	= kstrdup(name, GFP_KERNEL);
	core->miscdev.fops	= &bpu_core_fops;

	ret = misc_register(&core->miscdev);
	if (ret != 0) {
		bpu_core_free_fifos(core);
		(void)bpu_core_pm_ctrl(core, 1, 0);
		kfree(core->miscdev.name);
		kfree(core);
		dev_err(&pdev->dev, "Register bpu core device failed\n");
		return ret;
	}

	atomic_set(&core->open_counter, 0);

	atomic_set(&core->pend_flag, 0);
	core->running_task_num = 0;
	init_completion(&core->no_task_comp);

	core->inst.hw_io_ver = BPU_HW_IO_VERSION;
	core->inst.host = core;
	core->inst.index = core->index;

	ret = bpu_core_register(core);
	if (ret != 0) {
		misc_deregister(&core->miscdev);
		kfree(core->miscdev.name);
		bpu_core_free_fifos(core);
		(void)bpu_core_pm_ctrl(core, 1, 0);
		kfree(core);
		dev_err(&pdev->dev, "Register bpu core to bpu failed\n");
		return ret;
	}

	/* 0 is not limit, just by the max fifo length */
	core->inst.task_buf_limit = 0;

	ret = bpu_core_create_sys(core);
	if (ret != 0) {
		dev_err(&pdev->dev, "BPU core registe sys failed\n");
	}

	dev_set_drvdata(&pdev->dev, core);

	ret = bpu_core_dvfs_register(core, NULL);
	if (ret != 0) {
		dev_err(&pdev->dev, "BPU core registe dvfs failed\n");
	}

	devfreq_suspend_device(core->dvfs->devfreq);
	(void)bpu_core_pm_ctrl(core, 0, 0);
	return 0;
}

static int bpu_core_remove(struct platform_device *pdev)
{
	struct bpu_core *core = (struct bpu_core *)dev_get_drvdata(&pdev->dev);

	bpu_core_dvfs_unregister(core);
	bpu_core_discard_sys(core);

	bpu_core_free_fifos(core);

	misc_deregister(&core->miscdev);
	kfree(core->miscdev.name);

	core->inst.host = NULL;
	bpu_core_unregister(core);

	(void)bpu_core_pm_ctrl(core, 1, 0);
	kfree(core);

	return 0;
}

static const struct of_device_id bpu_core_of_match[] = {
	{ .compatible = "hobot,hobot-bpu", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, bpu_core_of_match);

static struct platform_driver bpu_core_platform_driver = {
	.probe	 = bpu_core_probe,
	.remove  = bpu_core_remove,
	.driver  = {
		.name = "bpu-core",
		.of_match_table = bpu_core_of_match,
#ifdef CONFIG_PM
		.pm = &bpu_core_pm_ops,
#endif
#ifdef CONFIG_FAULT_INJECTION_ATTR
		.fault_injection_store = bpu_core_fault_injection_store,
		.fault_injection_show = bpu_core_fault_injection_show,
#endif
	},
};

static int __init bpu_core_init(void)
{
	int32_t ret;

	ret = platform_driver_register(&bpu_core_platform_driver);
	if (ret != 0) {
		pr_err("BPU Core driver register failed\n");
	}

	return ret;
}

static void __exit bpu_core_exit(void)
{
	platform_driver_unregister(&bpu_core_platform_driver);
}

module_init(bpu_core_init);/*PRQA S 0605*/ /* Linux Macro */
module_exit(bpu_core_exit);/*PRQA S 0605*/ /* Linux Macro */
MODULE_DESCRIPTION("Driver for Horizon BPU Process Core");
MODULE_AUTHOR("Zhang Guoying<guoying.zhang@horizon.ai>");
MODULE_LICENSE("GPL v2");
