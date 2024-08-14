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

#include <linux/component.h>
#include <linux/clk.h>
#include <linux/of_graph.h>
#include <linux/of_platform.h>
#include <linux/of_device.h>

#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_vblank.h>
#include <drm/drm_managed.h>

#include "dc_hw.h"
#include "vs_dc.h"
#include "vs_crtc.h"
#include "vs_plane.h"
#include "vs_writeback.h"
#include "dc_info.h"
#include "dc_proc.h"

#include "vs_drv.h"

static int dc_bind(struct device *dev, struct device *master, void *data)
{
	struct drm_device *drm_dev = data;
	struct vs_dc *dc	   = dev_get_drvdata(dev);
	const struct dc_info *dc_info;
	struct device_node *port;
	struct dc_wb *dc_wb, *dc_wb_tmp;
	struct dc_plane *dc_plane, *dc_plane_tmp;
	struct dc_crtc *dc_crtc, *dc_crtc_tmp;

	int i, ret;

	if (!drm_dev || !dc) {
		dev_err(dev, "devices are not created.\n");
		return -ENODEV;
	}

	dc_info = dc->info;

	ret = vs_drm_iommu_attach_device(drm_dev, dev);
	if (ret < 0) {
		dev_err(dev, "Failed to attached iommu device.\n");
		return ret;
	}

	for (i = 0; i < dc_info->num_display; i++) {
		struct dc_crtc_info *dc_crtc_info = &dc_info->displays[i];
		struct vs_crtc *vs_crtc;

		dc_crtc = drmm_kzalloc(drm_dev, sizeof(*dc_crtc), GFP_KERNEL);
		if (!dc_crtc) {
			ret = -ENOMEM;
			goto err_cleanup_crtcs;
		}

		ret = dc_crtc_init(dc_crtc, dc_crtc_info, &dc->aux_list);
		if (ret)
			goto err_cleanup_crtcs;

		list_add_tail(&dc_crtc->head, &dc->crtc_list);

		vs_crtc	     = &dc_crtc->vs_crtc;
		vs_crtc->dev = dc->dev;
		ret	     = vs_crtc_init(drm_dev, vs_crtc);
		if (ret)
			goto err_cleanup_crtcs;

		ret = dc_crtc_create_prop(dc_crtc);
		if (ret)
			goto err_cleanup_crtcs;

		port = of_graph_get_port_by_id(dev->of_node, i);
		if (!port) {
			dev_err(dev, "no port node found\n");
			ret = -EINVAL;
			goto err_cleanup_crtcs;
		}
		of_node_put(port);
		vs_crtc->base.port = port;
	}

	for (i = 0; i < dc_info->num_plane; i++) {
		struct dc_plane_info *dc_plane_info = &dc_info->planes[i];
		struct vs_plane *vs_plane;

		dc_plane = drmm_kzalloc(drm_dev, sizeof(*dc_plane), GFP_KERNEL);
		if (!dc_plane) {
			ret = -ENOMEM;
			goto err_cleanup_planes;
		}

		ret = dc_plane_init(dc_plane, dc_plane_info, &dc->aux_list);
		if (ret)
			goto err_cleanup_planes;

		list_add_tail(&dc_plane->head, &dc->plane_list);

		vs_plane      = &dc_plane->vs_plane;
		vs_plane->dev = dc->dev;
		ret	      = vs_plane_init(drm_dev, vs_plane);
		if (ret)
			goto err_cleanup_planes;

		ret = dc_plane_create_prop(dc_plane);
		if (ret)
			goto err_cleanup_planes;

		if (vs_plane->base.type == DRM_PLANE_TYPE_PRIMARY)
			set_crtc_primary_plane(drm_dev, &vs_plane->base);

		if (vs_plane->base.type == DRM_PLANE_TYPE_CURSOR)
			set_crtc_cursor_plane(drm_dev, &vs_plane->base);
	}

	for (i = 0; i < dc_info->num_wb; i++) {
		struct dc_wb_info *dc_wb_info = &dc_info->writebacks[i];
		struct vs_wb *vs_wb;

		dc_wb = drmm_kzalloc(drm_dev, sizeof(*dc_wb), GFP_KERNEL);
		if (!dc_wb) {
			ret = -ENOMEM;
			goto err_cleanup_wbs;
		}

		ret = dc_wb_init(dc_wb, dc_wb_info, &dc->aux_list);
		if (ret)
			goto err_cleanup_wbs;

		list_add_tail(&dc_wb->head, &dc->wb_list);

		vs_wb	   = &dc_wb->vs_wb;
		vs_wb->dev = dc->dev;
		ret	   = vs_wb_init(drm_dev, vs_wb);
		if (ret)
			goto err_cleanup_wbs;

		ret = dc_wb_post_create(dc_wb);
		if (ret)
			goto err_cleanup_wbs;

		ret = dc_wb_create_prop(dc_wb);
		if (ret)
			goto err_cleanup_wbs;
	}

	drm_dev->mode_config.min_width	   = dc_info->min_width;
	drm_dev->mode_config.min_height	   = dc_info->min_height;
	drm_dev->mode_config.max_width	   = dc_info->max_width;
	drm_dev->mode_config.max_height	   = dc_info->max_height;
	drm_dev->mode_config.cursor_width  = dc_info->cursor_width;
	drm_dev->mode_config.cursor_height = dc_info->cursor_height;

	vs_drm_update_pitch_alignment(drm_dev, dc_info->pitch_alignment);

	dc->drm_dev = drm_dev;

	return 0;

err_cleanup_wbs:
	list_for_each_entry_safe (dc_wb, dc_wb_tmp, &dc->wb_list, head)
		dc_wb_destroy(&dc_wb->vs_wb);
err_cleanup_planes:
	list_for_each_entry_safe (dc_plane, dc_plane_tmp, &dc->plane_list, head)
		dc_plane_destroy(&dc_plane->vs_plane);
err_cleanup_crtcs:
	list_for_each_entry_safe (dc_crtc, dc_crtc_tmp, &dc->crtc_list, head)
		dc_crtc_destroy(&dc_crtc->vs_crtc);

	vs_drm_iommu_detach_device(drm_dev, dev);

	return ret;
}

static void dc_unbind(struct device *dev, struct device *master, void *data)
{
	struct drm_device *drm_dev = data;

	vs_drm_iommu_detach_device(drm_dev, dev);
}

static const struct component_ops dc_component_ops = {
	.bind	= dc_bind,
	.unbind = dc_unbind,
};

static const struct of_device_id dc_driver_dt_match[] = {
	{.compatible = "verisilicon,dc8000_nano", .data = &dc_8000_nano_info},
	{},
};
MODULE_DEVICE_TABLE(of, dc_driver_dt_match);

static void dc_crtc_handle_vblank(struct vs_dc *dc, struct vs_crtc *vs_crtc, u32 irq_status)
{
	const struct vs_crtc_info *info	  = vs_crtc->info;
	struct drm_crtc_state *crtc_state = vs_crtc->base.state;
	struct dc_wb *dc_wb;
	struct dc_plane *dc_plane;

	if (!((info->vblank_bit | info->underflow_bit) & irq_status))
		return;

	list_for_each_entry (dc_plane, &dc->plane_list, head) {
		if (!(drm_plane_mask(&dc_plane->vs_plane.base) & crtc_state->plane_mask))
			continue;

		if (info->vblank_bit & irq_status)
			dc_plane_vblank(&dc_plane->vs_plane, irq_status);
	}

	list_for_each_entry (dc_wb, &dc->wb_list, head) {
		if (!(drm_connector_mask(&dc_wb->vs_wb.base.base) & crtc_state->connector_mask))
			continue;

		if (info->vblank_bit & irq_status)
			dc_wb_vblank(&dc_wb->vs_wb, irq_status);
	}

	dc_crtc_vblank(vs_crtc, irq_status);
}

static irqreturn_t dc_isr(int irq, void *data)
{
	struct vs_dc *dc = data;
	struct dc_crtc *dc_crtc;
	u32 irq_status;

	irq_status = dc_hw_get_interrupt(dc->hw);

	list_for_each_entry (dc_crtc, &dc->crtc_list, head)
		dc_crtc_handle_vblank(dc, &dc_crtc->vs_crtc, irq_status);

	return IRQ_HANDLED;
}

static struct dc_device *dc_add_device(struct device *dev, const char *name)
{
	struct dc_device *dc_dev;

	dc_dev = devm_kzalloc(dev, sizeof(*dc_dev), GFP_KERNEL);
	if (!dc_dev)
		return ERR_PTR(-ENOMEM);

	dc_dev->dev = dev;
	strncpy(dc_dev->name, name, DC_DEV_NAME_SIZE - 1);
	return dc_dev;
}

static int dc_get_all_devices(struct device *dev)
{
	struct vs_dc *dc = dev_get_drvdata(dev);
	struct dc_device *dc_dev, *tmp;
	struct platform_device *pdev;
	struct device_node *np;
	const char *aux_name;
	void *drvdata;
	int i, ret;

	dc_dev = dc_add_device(dev, DC_HW_DEV_NAME);
	if (IS_ERR(dc_dev)) {
		ret = PTR_ERR(dc_dev);
		goto err_clean;
	}
	list_add_tail(&dc_dev->head, &dc->aux_list);

	i = of_property_count_strings(dev->of_node, "aux-names");
	if (i <= 0)
		return 0;

	while (i--) {
		np = of_parse_phandle(dev->of_node, "aux-devs", i);
		if (!np) {
			ret = -ENODEV;
			goto err_clean;
		}

		pdev = of_find_device_by_node(np);
		of_node_put(np);
		if (!pdev) {
			ret = -ENODEV;
			goto err_clean;
		}
		drvdata = platform_get_drvdata(pdev);
		if (!drvdata) {
			ret = -EPROBE_DEFER;
			goto err_clean;
		}
		of_property_read_string_index(dev->of_node, "aux-names", i, &aux_name);
		dc_dev = dc_add_device(&pdev->dev, aux_name);
		if (IS_ERR(dc_dev)) {
			ret = PTR_ERR(dc_dev);
			goto err_clean;
		}
		list_add_tail(&dc_dev->head, &dc->aux_list);
	}

	return 0;

err_clean:
	list_for_each_entry_safe_reverse (dc_dev, tmp, &dc->aux_list, head) {
		list_del(&dc_dev->head);
		kfree(dc_dev);
	}
	return ret;
}

static void parse_out_bus_list(struct vs_dc *dc)
{
	struct device *dev = dc->dev;
	int ret, i;

	ret = of_property_count_strings(dev->of_node, "out-bus-list");
	if (!ret)
		return;

	WARN_ON(ret > DC_OUT_BUS_MAX_NUM);

	for (i = 0; i < ret; i++) {
		of_property_read_string_index(dev->of_node, "out-bus-list", i,
					      &dc->out_bus_list[i]);
	}
}

static int dc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct vs_dc *dc;
	void __iomem *dc_base;
	int irq, ret;
	const struct dc_info *dc_info;

	dc = devm_kzalloc(dev, sizeof(*dc), GFP_KERNEL);
	if (!dc)
		return -ENOMEM;

	dc->dev = dev;

	dev_set_drvdata(dev, dc);

	dc->apb_clk = devm_clk_get_optional(dev, "apb_clk");
	if (IS_ERR(dc->apb_clk)) {
		dev_err(dev, "failed to get apb_clk source\n");
		return PTR_ERR(dc->apb_clk);
	}

	ret = clk_prepare_enable(dc->apb_clk);
	if (ret < 0) {
		dev_err(dev, "Failed to prepare/enable apb_clk\n");
		return ret;
	}

	dc->axi_clk = devm_clk_get_optional(dev, "axi_clk");
	if (IS_ERR(dc->axi_clk)) {
		dev_err(dev, "failed to get axi_clk source\n");
		ret = PTR_ERR(dc->axi_clk);
		goto err_disable_apb_clk;
	}

	ret = clk_prepare_enable(dc->axi_clk);
	if (ret < 0) {
		dev_err(dev, "failed to prepare/enable axi_clk\n");
		goto err_disable_apb_clk;
	}

	dc->core_clk = devm_clk_get_optional(dev, "core_clk");
	if (IS_ERR(dc->core_clk)) {
		dev_err(dev, "failed to get core_clk source\n");
		ret = PTR_ERR(dc->core_clk);
		goto err_disable_axi_clk;
	}

	ret = clk_prepare_enable(dc->core_clk);
	if (ret < 0) {
		dev_err(dev, "Failed to prepare/enable core_clk\n");
		goto err_disable_axi_clk;
	}

	INIT_LIST_HEAD(&dc->aux_list);
	INIT_LIST_HEAD(&dc->crtc_list);
	INIT_LIST_HEAD(&dc->plane_list);
	INIT_LIST_HEAD(&dc->wb_list);

	ret = dc_get_all_devices(dev);
	if (ret) {
		dev_err(dev, "Failed to get aux devices\n");
		goto err_disable_core_clk;
	}

	parse_out_bus_list(dc);

	dc_info = of_device_get_match_data(dev);

	dc_base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(dc_base)) {
		dev_err(dev, "failed to map addr\n");
		ret = PTR_ERR(dc_base);
		goto err_disable_core_clk;
	}

	dc->hw = dc_hw_create(dc_info->family, dc_base);
	if (IS_ERR(dc->hw)) {
		dev_err(dev, "failed to create dc hw\n");
		ret = PTR_ERR(dc->hw);
		goto err_disable_core_clk;
	}

	irq = platform_get_irq(pdev, 0);
	ret = devm_request_irq(dev, irq, dc_isr, 0, dev_name(dev), dc);
	if (ret < 0) {
		dev_err(dev, "Failed to install irq:%u.\n", irq);
		goto err_disable_core_clk;
	}

	dc->info = dc_info;

	ret = component_add(dev, &dc_component_ops);
	if (!ret)
		return ret;

err_disable_core_clk:
	clk_disable_unprepare(dc->core_clk);
err_disable_axi_clk:
	clk_disable_unprepare(dc->axi_clk);
err_disable_apb_clk:
	clk_disable_unprepare(dc->apb_clk);

	return ret;
}

static int dc_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct vs_dc *dc   = dev_get_drvdata(dev);

	component_del(dev, &dc_component_ops);

	clk_disable_unprepare(dc->core_clk);

	clk_disable_unprepare(dc->axi_clk);

	clk_disable_unprepare(dc->apb_clk);

	kfree(dc->hw);

	dev_set_drvdata(dev, NULL);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static void dc_suspend_resume(struct device *dev, bool suspend)
{
	struct vs_dc *dc = dev_get_drvdata(dev);
	struct drm_plane *drm_plane;
	struct vs_plane *vs_plane;
	struct drm_crtc *drm_crtc;
	struct vs_crtc *vs_crtc;

	drm_for_each_plane (drm_plane, dc->drm_dev) {
		vs_plane = to_vs_plane(drm_plane);
		if (vs_plane->dev != dev)
			continue;
		if (suspend)
			dc_plane_suspend(vs_plane);
		else
			dc_plane_resume(vs_plane);
	}

	drm_for_each_crtc (drm_crtc, dc->drm_dev) {
		vs_crtc = to_vs_crtc(drm_crtc);
		if (vs_crtc->dev != dev)
			continue;
		if (suspend)
			dc_crtc_suspend(vs_crtc);
		else
			dc_crtc_resume(vs_crtc);
	}
}

static int dc_suspend(struct device *dev)
{
	struct vs_dc *dc = dev_get_drvdata(dev);

	dc_suspend_resume(dev, true);

	clk_disable_unprepare(dc->core_clk);

	clk_disable_unprepare(dc->axi_clk);

	clk_disable_unprepare(dc->apb_clk);

	return 0;
}

static int dc_resume(struct device *dev)
{
	int ret;
	struct vs_dc *dc = dev_get_drvdata(dev);

	ret = clk_prepare_enable(dc->apb_clk);
	if (ret < 0) {
		dev_err(dev, "Failed to prepare/enable apb_clk\n");
		return ret;
	}

	ret = clk_prepare_enable(dc->axi_clk);
	if (ret < 0) {
		dev_err(dev, "failed to prepare/enable axi_clk\n");
		goto err_disable_apb_clk;
	}

	ret = clk_prepare_enable(dc->core_clk);
	if (ret < 0) {
		dev_err(dev, "Failed to prepare/enable core_clk\n");
		goto err_disable_axi_clk;
	}

	dc_suspend_resume(dev, false);

	return 0;

err_disable_axi_clk:
	clk_disable_unprepare(dc->axi_clk);
err_disable_apb_clk:
	clk_disable_unprepare(dc->apb_clk);
	return ret;
}
#endif

static const struct dev_pm_ops dc_pm_ops = {SET_SYSTEM_SLEEP_PM_OPS(dc_suspend, dc_resume)};

struct platform_driver dc_platform_driver = {
	.probe	= dc_probe,
	.remove = dc_remove,
	.driver =
		{
			.name		= "vs-dc",
			.of_match_table = of_match_ptr(dc_driver_dt_match),
			.pm		= &dc_pm_ops,
		},
};

MODULE_DESCRIPTION("VeriSilicon DC Driver");
MODULE_LICENSE("GPL");
