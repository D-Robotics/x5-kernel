// SPDX-License-Identifier: GPL-2.0
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

#include <linux/slab.h>

#include "dc_hw.h"
#include "dc_hw_priv.h"
#include "dc_8000_nano.h"

static const char *const dc_hw_property_str[DC_PROP_COUNT] = {
	"PLANE_FB",	      //
	"PLANE_WIN",	      //
	"PLANE_BLEND",	      //
	"PLANE_DEGAMMA",      //
	"PLANE_ASSIGN",	      //
	"PLANE_CURSOR",	      //
	"PLANE_ZPOS",	      //
	"PLANE_GAMUT_CONV",   //
	"DISP_GAMMA",	      //
	"DISP_LCSC0_DEGAMMA", //
	"DISP_LCSC0_CSC",     //
	"DISP_LCSC0_REGAMMA", //
	"DISP_LCSC1_DEGAMMA", //
	"DISP_LCSC1_CSC",     //
	"DISP_LCSC1_REGAMMA", //
	"DISP_SCAN",	      //
	"DISP_OUTPUT",	      //
	"DISP_SECURE",	      //
	"DISP_WRITEBACK_FB",  //
	"DISP_WRITEBACK_POS", //
	"DISP_CRC",	      //
	"DISP_UNDERFLOW",     //
	"DISP_BGCOLOR",	      //
	"DISP_DITHER",	      //
};

static const char *const dc_hw_type_str[] = {
	"PRIMARY_PLANE",
	"OVERLAY_PLANE",
	"CURSOR_PLANE",
	"DISPLAY",
};

static const char *get_property_str(u8 prop)
{
	if (prop >= DC_PROP_COUNT)
		return NULL;

	return dc_hw_property_str[prop];
}

static const char *get_type_str(u8 id)
{
	return dc_hw_type_str[DC_GET_TYPE(id)];
}

void dc_hw_destroy(struct dc_hw *hw)
{
	kfree(hw);
}

struct dc_hw *dc_hw_create(u8 family, void __iomem *dc)
{
	struct dc_hw *hw;
	int ret;

	if (!dc || family >= DC_FAMILY_NUM)
		return ERR_PTR(-EINVAL);

	hw = kzalloc(sizeof(*hw), GFP_KERNEL);
	if (!hw)
		return ERR_PTR(-ENOMEM);

	hw->base = dc;

	if (family == DC_8000_NANO_FAMILY)
		hw->funcs = &dc_8000_nano_funcs;
	WARN_ON(!hw->funcs);

	if (hw->funcs->init) {
		ret = hw->funcs->init(hw);
		if (ret) {
			pr_err("hw init failed with %d\n", ret);
			goto free_hw;
		}
	}

	return hw;

free_hw:
	kfree(hw);
	return ERR_PTR(ret);
}

int dc_hw_init(struct dc_hw_processor *processor, struct dc_hw *hw,
	       const struct dc_hw_proc_info *info)
{
	if (!processor || !hw || !info)
		return -EINVAL;

	processor->hw	 = hw;
	processor->info	 = info;
	processor->funcs = hw->funcs->get_proc_funcs(hw);

	return 0;
}

int dc_hw_setup(struct dc_hw_processor *processor)
{
	if (!processor)
		return -EINVAL;

	if (processor->funcs->init)
		return processor->funcs->init(processor);

	return 0;
}

void dc_hw_update(struct dc_hw_processor *processor, u8 prop, void *value)
{
	const struct dc_hw_proc_info *info;

	if (!processor)
		return;

	info = processor->info;

	pr_debug("update %s-%d:%s\n", get_type_str(info->id), DC_GET_INDEX(info->id),
		 get_property_str(prop));

	if (processor->funcs->update)
		processor->funcs->update(processor, prop, value);
}

int dc_hw_get(struct dc_hw_processor *processor, u8 prop, void *value, size_t size)
{
	if (!processor)
		return -EINVAL;

	if (processor->funcs->get)
		return processor->funcs->get(processor, prop, value, size);

	return -EOPNOTSUPP;
}

void dc_hw_disable(struct dc_hw_processor *processor)
{
	if (!processor)
		return;

	if (processor->funcs->disable)
		processor->funcs->disable(processor);
}

void dc_hw_commit(struct dc_hw_processor *processor)
{
	const struct dc_hw_proc_info *info;

	if (!processor)
		return;

	info = processor->info;

	pr_debug("commit %s-%d\n", get_type_str(info->id), DC_GET_INDEX(info->id));

	if (processor->funcs->commit)
		processor->funcs->commit(processor);
}

void dc_hw_enable_irq(struct dc_hw_processor *processor, u32 irq_bits)
{
	if (!processor)
		return;

	if (processor->funcs->enable_irq)
		processor->funcs->enable_irq(processor, irq_bits);
}

void dc_hw_disable_irq(struct dc_hw_processor *processor, u32 irq_bits)
{
	if (!processor)
		return;

	if (processor->funcs->disable_irq)
		processor->funcs->disable_irq(processor, irq_bits);
}

u32 dc_hw_get_interrupt(struct dc_hw *hw)
{
	if (!hw || !hw->funcs)
		return 0;

	return hw->funcs->get_irq(hw);
}
