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

#include <linux/workqueue.h>
#include <drm/drm_bridge.h>
#include <drm/drm_of.h>
#include <drm/drm_connector.h>

#include "dc_proc.h"
#include "dc_info.h"
#include "vs_sif.h"

struct sif_proc {
	struct dc_proc base;

	struct vs_sif *sif;

	/** isr bottom half */
	struct work_struct work;
};

static inline struct sif_proc *to_sif_proc(struct dc_proc *proc)
{
	return container_of(proc, struct sif_proc, base);
}

static void sif_work(struct work_struct *work)
{
	struct sif_proc *hw_wb			     = container_of(work, struct sif_proc, work);
	struct vs_wb *vs_wb			     = proc_to_vs_wb(&hw_wb->base);
	struct drm_writeback_connector *wb_connector = &vs_wb->base;
	const struct drm_connector *connector	     = &wb_connector->base;
	struct drm_connector_state *state	     = connector->state;
	const struct drm_writeback_job *job	     = state->writeback_job;

	if (!job)
		return;

	sif_set_output(hw_wb->sif, NULL);

	drm_writeback_queue_job(wb_connector, state);
	drm_writeback_signal_completion(wb_connector, 0);
}

static struct dc_proc *vs_sif_create_writeback(struct device *dev, const struct dc_proc_info *info)
{
	struct vs_sif *sif = dev_get_drvdata(dev);
	struct sif_proc *hw_wb;

	hw_wb = kzalloc(sizeof(*hw_wb), GFP_KERNEL);
	if (!hw_wb)
		return ERR_PTR(-ENOMEM);

	hw_wb->sif = sif;

	INIT_WORK(&hw_wb->work, sif_work);
	INIT_LIST_HEAD(&hw_wb->base.head);

	return &hw_wb->base;
}

static int vs_sif_proc_post_create(struct dc_proc *proc)
{
	struct vs_wb *vs_wb	   = proc_to_vs_wb(proc);
	struct drm_connector *conn = &vs_wb->base.base;
	struct drm_encoder *encoder;
	struct drm_bridge *bridge;
	int ret;
	struct sif_proc *hw_wb = to_sif_proc(proc);
	struct vs_sif *sif     = hw_wb->sif;

	drm_connector_for_each_possible_encoder (conn, encoder)
		break;

	/* output port is port 0*/
	bridge = devm_drm_of_get_bridge(sif->dev, sif->dev->of_node, 0, -1);
	if (IS_ERR(bridge)) {
		ret    = PTR_ERR(bridge);
		bridge = NULL;

		if (ret == -ENODEV)
			return 0;

		dev_err(sif->dev, "find bridge failed(err=%d)\n", ret);

		return ret;
	}

	return drm_bridge_attach(encoder, bridge, NULL, DRM_BRIDGE_ATTACH_NO_CONNECTOR);
}

static void vs_sif_proc_commit_wb(struct dc_proc *proc)
{
	struct sif_proc *hw_wb	      = to_sif_proc(proc);
	struct vs_wb *vs_wb	      = proc_to_vs_wb(proc);
	struct drm_writeback_job *job = vs_wb->base.base.state->writeback_job;

	/* make sure sif work is linked to correct wb connector*/
	hw_wb->sif->work = &hw_wb->work;

	if (!job || !job->fb) {
		sif_set_output(hw_wb->sif, NULL);
		return;
	}

	sif_set_output(hw_wb->sif, job->fb);
}

static void vs_sif_proc_destroy_wb(struct dc_proc *proc)
{
	struct sif_proc *hw_wb = to_sif_proc(proc);

	kfree(hw_wb);
}

const struct dc_proc_funcs vs_sif_wb_funcs = {
	.create	     = vs_sif_create_writeback,
	.commit	     = vs_sif_proc_commit_wb,
	.destroy     = vs_sif_proc_destroy_wb,
	.post_create = vs_sif_proc_post_create,
};
