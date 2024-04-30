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

#ifndef __VS_WRITEBACK_H_
#define __VS_WRITEBACK_H_

/** @addtogroup DRM
 *  vs drm writeback API.
 *  @ingroup DRIVER
 *
 *  @{
 */

#include <drm/drm_writeback.h>

/**
 * vs writeback configuration structure
 */
struct vs_wb_info {
	/** color format list for writeback port, defined in drm_fourcc.h */
	const u32 *wb_formats;
	/** number of writeback format */
	u16 num_wb_formats;
	/** crtc name of this writeback connector */
	const char *const *crtc_names;
	/** number of possible crtcs */
	u8 num_crtcs;
};

struct vs_wb;

/**
 * vs writeback function structure
 */
struct vs_wb_funcs {
	void (*destroy)(struct vs_wb *vs_wb);

	void (*commit)(struct vs_wb *vs_wb, struct drm_connector_state *state);

	struct drm_connector_state *(*create_state)(struct vs_wb *vs_wb);
	void (*destroy_state)(struct drm_connector_state *state);
	struct drm_connector_state *(*duplicate_state)(struct vs_wb *vs_wb);
	void (*print_state)(struct drm_printer *p, const struct drm_connector_state *state);

	int (*set_prop)(struct vs_wb *vs_wb, struct drm_connector_state *state,
			struct drm_property *property, uint64_t val);
	int (*get_prop)(struct vs_wb *vs_wb, const struct drm_connector_state *state,
			struct drm_property *property, uint64_t *val);
};

/**
 * vs writeback connector structure
 */
struct vs_wb {
	struct drm_writeback_connector base;
	/** belong to which display controller device. */
	struct device *dev;
	const struct vs_wb_info *info;
	const struct vs_wb_funcs *funcs;
};

#if IS_ENABLED(CONFIG_VERISILICON_WRITEBACK)
/**
 * @brief      initialize new vs writeback connector
 *
 * @param[in]  drm_dev drm device
 * @param[in]  vs_wb vs writeback object
 *
 */
int vs_wb_init(struct drm_device *drm_dev, struct vs_wb *vs_wb);

#else
static inline int vs_wb_init(struct drm_device *drm_dev, struct vs_wb *vs_wb)
{
	return 0;
}

#endif

static inline struct vs_wb *to_vs_wb(struct drm_writeback_connector *connector)
{
	return container_of(connector, struct vs_wb, base);
}

/** @} */

#endif /* __VS_WRITEBACK_H_ */
