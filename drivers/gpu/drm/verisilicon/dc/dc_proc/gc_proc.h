/* SPDX-License-Identifier: GPL-2.0 */
/****************************************************************************
 *
 *    Copyright (c) 2001 - 2024 by VeriSilicon Holdings Co., Ltd.
 *    All rights reserved.
 *
 *    The material in this file is confidential and contains trade secrets
 *    of VeriSilicon. This is proprietary information owned by VeriSilicon.
 *    No part of this work may be disclosed, reproduced, copied, transmitted,
 *    or used in any way for any purpose, without the express written
 *    permission of VeriSilicon.
 *
 ****************************************************************************/

#ifndef __GC_PROC_H__
#define __GC_PROC_H__

/** @addtogroup DC
 *  vs dc processor data and API.
 *  @ingroup DRIVER
 *
 *  @{
 */

/**
 * display controller plane feature
 */
enum gpu_plane_feature {
	GPU_PLANE_IN	   = BIT(0),
	GPU_PLANE_OUT	   = BIT(1),
	GPU_PLANE_SCALE	   = BIT(2),
	GPU_PLANE_ROTATION = BIT(3),
};

/**
 * hw plane configuration
 */
struct gpu_plane_info {
	/** supported feature with enum gpu_feature. */
	u32 features;
	/** processor id. */
	u8 id;
	u32 fourcc;
};

/**
 * gpu plane proc state
 */
struct gpu_plane_proc_state {
	struct dc_proc_state base;
	struct dc_plane_state *parent;
};

static inline struct gpu_plane_proc_state *to_gpu_plane_proc_state(struct dc_proc_state *state)
{
	return container_of(state, struct gpu_plane_proc_state, base);
}

extern const struct dc_proc_funcs gpu_plane_funcs;

#define GC_HW_NAME "vs-n2d"

/** @} */

#endif /* __GC_PROC_H__ */
