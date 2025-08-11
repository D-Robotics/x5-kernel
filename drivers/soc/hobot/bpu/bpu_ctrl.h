/*
 * Copyright (C) 2019 Horizon Robotics
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 *
 */
#ifndef __BPU_CTRL_H__
#define __BPU_CTRL_H__ /*PRQA S 0603*/ /* Linux header define style */
#include "bpu_core.h"

typedef enum bpu_core_ext_ctrl_cmd {
	CTRL_START = 0,
    CORE_TYPE,
    SET_CLK,
    GET_CLK,
    CTRL_END,
} ctrl_cmd_t;

int32_t bpu_core_pend_on(struct bpu_core *core);
int32_t bpu_core_pend_off(struct bpu_core *core);
int32_t bpu_core_is_pending(const struct bpu_core *core);
int32_t bpu_core_clk_on(const struct bpu_core *core);
int32_t bpu_core_clk_off(const struct bpu_core *core);
int32_t bpu_core_clk_off_quirk(const struct bpu_core *core);
int32_t bpu_core_power_on(const struct bpu_core *core);
int32_t bpu_core_power_off(const struct bpu_core *core);
int32_t bpu_core_power_off_quirk(const struct bpu_core *core);
int32_t bpu_core_enable(struct bpu_core *core);
int32_t bpu_core_disable(struct bpu_core *core);
int32_t bpu_core_reset(struct bpu_core *core);
int32_t bpu_core_reset_quirk(struct bpu_core *core);
int32_t bpu_core_process_recover(struct bpu_core *core);
int32_t bpu_core_set_limit(struct bpu_core *core, int32_t limit);
int32_t bpu_core_ext_ctrl(struct bpu_core *core, ctrl_cmd_t cmd, uint64_t *data);
int32_t bpu_reset_ctrl(struct reset_control *rstc, uint16_t val);
int32_t bpu_core_pm_ctrl(const struct bpu_core *core, uint16_t init, uint16_t ops);

#if defined(CONFIG_PM_DEVFREQ) && defined(CONFIG_DEVFREQ_THERMAL)
int32_t bpu_core_dvfs_register(struct bpu_core *core, const char *name);
void bpu_core_dvfs_unregister(struct bpu_core *core);
int32_t bpu_core_set_freq_level(struct bpu_core *core, int32_t level);
void bpu_core_dvfs_register_delayed(struct work_struct *work);
#else
static inline int32_t bpu_core_dvfs_register(struct bpu_core *core,
		const char *name)
{
	return 0;
}

static inline void bpu_core_dvfs_unregister(struct bpu_core *core)
{
	return;
}

static inline int32_t bpu_core_set_freq_level(struct bpu_core *core,
		int32_t level)
{
	pr_err_ratelimited("BPU:Not support change freq level\n");
	return 0;
}

static inline void bpu_core_dvfs_register_delayed(struct work_struct *work)
{
	return;
}
#endif

#endif
