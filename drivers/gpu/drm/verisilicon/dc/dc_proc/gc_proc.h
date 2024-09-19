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
