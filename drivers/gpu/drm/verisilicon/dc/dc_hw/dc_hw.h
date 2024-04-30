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

#ifndef __DC_HW_H__
#define __DC_HW_H__

/** @addtogroup DC
 *  vs dc hw API.
 *  @ingroup DRIVER
 *
 *  @{
 */

#include "dc_hw_type.h"

/**
 * @brief destroy dc hw instance
 *
 * @param[in] hw dc hw pointer
 */
void dc_hw_destroy(struct dc_hw *hw);

/**
 * @brief create dc hw instance with register base
 *
 * @param[in] family display controller family id
 * @param[in] dc display control register base
 *
 * @return dc_hw pointer on success, error code on failure
 */
struct dc_hw *dc_hw_create(u8 family, void __iomem *dc);

/**
 * @brief init dc hw processor structure with info, no hw initialize
 *
 * @param[in] processor dc hw processor pointer
 * @param[in] hw dc hw pointer
 * @param[in] info processor hw info
 *
 * @return dc_hw_processor pointer on success, error code on failure
 */
int dc_hw_init(struct dc_hw_processor *processor, struct dc_hw *hw,
	       const struct dc_hw_proc_info *info);

/**
 * @brief setup dc hw processor instance, hw initialize
 *
 * @param[in] processor dc hw processor pointer
 *
 * @return 0 on success, error code on failure
 */
int dc_hw_setup(struct dc_hw_processor *processor);

/**
 * @brief update processor properties
 *
 * @param[in] processor dc hw processor pointer
 * @param[in] prop processor property in dc_hw_property
 * @param[in] value data pointer
 */
void dc_hw_update(struct dc_hw_processor *processor, u8 prop, void *value);

/**
 * @brief get processor properties
 *
 * @param[in] processor dc hw processor pointer
 * @param[in] prop processor property in dc_hw_property
 * @param[in] value data pointer
 * @param[in] size max output data size
 *
 * @return status, 0 when succeed
 */
int dc_hw_get(struct dc_hw_processor *processor, u8 prop, void *value, size_t size);

/**
 * @brief disable processor
 *
 * @param[in] processor dc hw processor pointer
 */
void dc_hw_disable(struct dc_hw_processor *processor);

/**
 * @brief apply processor setting and affect on next vblank event
 *
 * @param[in] processor dc hw processor pointer
 */
void dc_hw_commit(struct dc_hw_processor *processor);

/**
 * @brief enable disable vblank interrupt
 *
 * @param[in] processor dc hw processor pointer
 * @param[in] enable true to enable
 */
void dc_hw_enable_vblank(struct dc_hw_processor *processor, bool enable);

/**
 * @brief get VBlank interrupt
 *
 * @param[in] hw dc hw pointer
 *
 * @return interrupt status, bitmask of post-processor
 */
u32 dc_hw_get_interrupt(struct dc_hw *hw);

/** @} */

#endif /* __DC_HW_H__ */
