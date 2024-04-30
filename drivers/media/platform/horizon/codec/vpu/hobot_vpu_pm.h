/***************************************************************************
 *                      COPYRIGHT NOTICE
 *             Copyright (c) 2019 - 2023 Horizon Robotics, Inc.
 *                     All rights reserved.
 ***************************************************************************/
#ifndef HOBOT_VPU_PM_H
#define HOBOT_VPU_PM_H

#include "hobot_vpu_utils.h"

#define MAX_VPU_FREQ (512000000U)

int32_t hb_vpu_clk_get(hb_vpu_dev_t *dev, uint64_t freq);
void hb_vpu_clk_put(const hb_vpu_dev_t *dev);
int32_t hb_vpu_clk_enable(const hb_vpu_dev_t *dev, uint64_t freq);
int32_t hb_vpu_clk_disable(const hb_vpu_dev_t *dev);
int32_t hb_vpu_pm_ctrl(hb_vpu_dev_t *dev, uint16_t init, uint16_t ops);

#endif /* HOBOT_VPU_PM_H */
