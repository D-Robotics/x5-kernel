/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024, VeriSilicon Holdings Co., Ltd. All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __VS_DC_H__
#define __VS_DC_H__

/** @addtogroup DC
 *  vs dc data types.
 *  @ingroup DRIVER
 *
 *  @{
 */

#define DC_HW_DEV_NAME	   "dc"
#define DC_OUT_BUS_MAX_NUM 2
/**
 * vs dc structure
 */
struct vs_dc {
	/** vs display controller instance. */
	struct dc_hw *hw;
	/** DRM device. */
	struct drm_device *drm_dev;
	/** DC device. */
	struct device *dev;
	/** clock handler of core clock. */
	struct clk *core_clk;
	/** clock handler of AXI clock. */
	struct clk *axi_clk;
	/** clock handler of APB clock. */
	struct clk *apb_clk;
	/** dc info matches the driver. */
	const struct dc_info *info;

	const char *out_bus_list[DC_OUT_BUS_MAX_NUM];
	/** aux device list. */
	struct list_head aux_list;
	/** dc crtc list. */
	struct list_head crtc_list;
	/** dc plane list. */
	struct list_head plane_list;
	/** dc wb list. */
	struct list_head wb_list;
};

extern struct platform_driver dc_platform_driver;

/** @} */

#endif /* __VS_DC_H__ */
