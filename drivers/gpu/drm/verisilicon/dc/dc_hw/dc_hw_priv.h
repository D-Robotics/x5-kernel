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

#ifndef __DC_HW_PRIV_H__
#define __DC_HW_PRIV_H__

/** @addtogroup DC
 *  vs dc hw private data types.
 *  @ingroup DRIVER
 *
 *  @{
 */

struct dc_hw;
struct dc_hw_processor;

/**
 * struct dc_hw_proc_funcs - dc hw processor operations
 *
 * These functions are generic interface for VeriSilicon
 * display controller, like DC8x00 and DC9000.
 * Description see helper API in dc_hw.h
 */
struct dc_hw_proc_funcs {
	int (*init)(struct dc_hw_processor *processor);
	void (*disable)(struct dc_hw_processor *processor);
	void (*update)(struct dc_hw_processor *processor, u8 prop, void *value);
	int (*get)(struct dc_hw_processor *processor, u8 prop, void *value, size_t size);
	void (*commit)(struct dc_hw_processor *processor);
	void (*enable_irq)(struct dc_hw_processor *processor, u32 irq_bits);
	void (*disable_irq)(struct dc_hw_processor *processor, u32 irq_bits);
};

/**
 * dc hw operations structure
 */
struct dc_hw_funcs {
	/** init hw. */
	int (*init)(struct dc_hw *hw);

	/** get interrupt. */
	u32 (*get_irq)(struct dc_hw *hw);

	/** return processor operations pointer. */
	const struct dc_hw_proc_funcs *(*get_proc_funcs)(struct dc_hw *hw);
};

/**
 * struct dc_hw - dc hw platform instance
 * dc hw platform instance
 */
struct dc_hw {
	/** display controller register base. */
	void __iomem *base;

	/** chip version in enum dc_hw_chip_ver. */
	u8 ver;

	/** pointer of hw operation. */
	const struct dc_hw_funcs *funcs;
};

/** @} */

#endif /* __DC_HW_PRIV_H__ */
