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

#include <linux/of_graph.h>
#include <linux/component.h>
#include <linux/iommu.h>

#include <drm/drm_of.h>
#include <drm/drm_crtc.h>
#include <drm/drm_drv.h>
#include <drm/drm_file.h>
#include <drm/drm_prime.h>
#include <drm/drm_vblank.h>
#include <drm/drm_ioctl.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_writeback.h>
#include <drm/drm_atomic_helper.h>

#include "vs_drv.h"
#include "vs_fb.h"
#include "vs_gem.h"
#include "vs_plane.h"
#include "vs_crtc.h"
#include "vs_simple_enc.h"
#include "dw_mipi_dsi.h"
#include "vs_dc.h"
#include "vs_sif.h"
#ifdef CONFIG_VERISILICON_BT1120
#include "vs_bt1120.h"
#include "bt1120_bridge.h"
#endif

#define DRV_NAME  "vs-drm"
#define DRV_DESC  "VeriSilicon DRM driver"
#define DRV_DATE  "20191101"
#define DRV_MAJOR 1
#define DRV_MINOR 0

#ifdef CONFIG_VERISILICON_GEM_ION
extern struct ion_device *hb_ion_dev;
#endif

static bool has_iommu = true;
static struct platform_driver vs_drm_platform_driver;

static const struct file_operations fops = {
	.owner		= THIS_MODULE,
	.open		= drm_open,
	.release	= drm_release,
	.unlocked_ioctl = drm_ioctl,
	.compat_ioctl	= drm_compat_ioctl,
	.poll		= drm_poll,
	.read		= drm_read,
	.mmap		= vs_gem_mmap,
};

static struct drm_driver vs_drm_driver = {
	.driver_features	   = DRIVER_MODESET | DRIVER_ATOMIC | DRIVER_GEM | DRIVER_RENDER,
	.lastclose		   = drm_fb_helper_lastclose,
	.prime_handle_to_fd	   = drm_gem_prime_handle_to_fd,
	.prime_fd_to_handle	   = drm_gem_prime_fd_to_handle,
	.gem_prime_import	   = vs_gem_prime_import,
	.gem_prime_import_sg_table = vs_gem_prime_import_sg_table,
	.gem_prime_mmap		   = vs_gem_prime_mmap,
	.dumb_create		   = vs_gem_dumb_create,
	.fops			   = &fops,
	.name			   = DRV_NAME,
	.desc			   = DRV_DESC,
	.date			   = DRV_DATE,
	.major			   = DRV_MAJOR,
	.minor			   = DRV_MINOR,
};

int vs_drm_iommu_attach_device(struct drm_device *drm_dev, struct device *dev)
{
	struct vs_drm_private *priv = drm_to_vs_priv(drm_dev);
	int ret;

	if (!has_iommu)
		return 0;

	if (!priv->domain) {
		priv->domain = iommu_get_domain_for_dev(dev);
		if (IS_ERR(priv->domain))
			return PTR_ERR(priv->domain);
		priv->dma_dev = dev;
	}

	ret = iommu_attach_device(priv->domain, dev);
	if (ret) {
		DRM_DEV_ERROR(dev, "Failed to attach iommu device\n");
		return ret;
	}

	return 0;
}

void vs_drm_iommu_detach_device(struct drm_device *drm_dev, struct device *dev)
{
	struct vs_drm_private *priv = drm_to_vs_priv(drm_dev);

	if (!has_iommu)
		return;

	iommu_detach_device(priv->domain, dev);

	if (priv->dma_dev == dev)
		priv->dma_dev = drm_dev->dev;
}

void vs_drm_update_pitch_alignment(struct drm_device *drm_dev, unsigned int alignment)
{
	struct vs_drm_private *priv = drm_to_vs_priv(drm_dev);

	if (alignment > priv->pitch_alignment)
		priv->pitch_alignment = alignment;
}

static const struct drm_mode_config_funcs vs_mode_config_funcs = {
	.fb_create	     = vs_fb_create,
	.output_poll_changed = drm_fb_helper_output_poll_changed,
	.atomic_check	     = drm_atomic_helper_check,
	.atomic_commit	     = drm_atomic_helper_commit,
};

static struct drm_mode_config_helper_funcs vs_mode_config_helpers = {
	.atomic_commit_tail = drm_atomic_helper_commit_tail,
};

static void vs_mode_config_init(struct drm_device *dev)
{
	dev->mode_config.normalize_zpos = true;

	if (dev->mode_config.max_width == 0 || dev->mode_config.max_height == 0) {
		dev->mode_config.min_width  = 0;
		dev->mode_config.min_height = 0;
		dev->mode_config.max_width  = 4096;
		dev->mode_config.max_height = 4096;
	}
	dev->mode_config.funcs		= &vs_mode_config_funcs;
	dev->mode_config.helper_private = &vs_mode_config_helpers;
}

static int vs_drm_bind(struct device *dev)
{
	struct drm_device *drm_dev;
	struct vs_drm_private *priv;
	int ret;

	priv = devm_drm_dev_alloc(dev, &vs_drm_driver, struct vs_drm_private, drm);
	if (IS_ERR(priv))
		return PTR_ERR(priv);

	drm_dev = &priv->drm;
	dev_set_drvdata(dev, drm_dev);

	priv->pitch_alignment = 64;
	priv->dma_dev	      = drm_dev->dev;

#ifdef CONFIG_VERISILICON_GEM_ION
	/**  create ion client*/
	if (!priv->client) {
		priv->client = ion_client_create(hb_ion_dev, "display");
		if (IS_ERR(priv->client)) {
			DRM_DEV_ERROR(dev, "vs ion client creation failed!\n");
			return PTR_ERR(priv->client);
		}
	}
#endif

	drm_mode_config_init(drm_dev);

	/* Now try and bind all our sub-components */
	ret = component_bind_all(dev, drm_dev);
	if (ret)
		goto err_mode;

	vs_mode_config_init(drm_dev);

	ret = drm_vblank_init(drm_dev, drm_dev->mode_config.num_crtc);
	if (ret)
		goto err_bind;

	drm_mode_config_reset(drm_dev);

	drm_kms_helper_poll_init(drm_dev);

	ret = drm_dev_register(drm_dev, 0);
	if (ret)
		goto err_helper;

	return 0;

err_helper:
	drm_kms_helper_poll_fini(drm_dev);
err_bind:
	component_unbind_all(drm_dev->dev, drm_dev);
err_mode:
	drm_mode_config_cleanup(drm_dev);
	dev_set_drvdata(dev, NULL);
	return ret;
}

static void vs_drm_unbind(struct device *dev)
{
	struct drm_device *drm_dev = dev_get_drvdata(dev);
#ifdef CONFIG_VERISILICON_GEM_ION
	struct vs_drm_private *priv = drm_to_vs_priv(drm_dev);
#endif

	drm_dev_unregister(drm_dev);

	drm_kms_helper_poll_fini(drm_dev);

	component_unbind_all(drm_dev->dev, drm_dev);

	drm_mode_config_cleanup(drm_dev);

#ifdef CONFIG_VERISILICON_GEM_ION
	if (priv->client) {
		ion_client_destroy(priv->client);
		priv->client = NULL;
	}
#endif

	dev_set_drvdata(dev, NULL);
}

static const struct component_master_ops vs_drm_ops = {
	.bind	= vs_drm_bind,
	.unbind = vs_drm_unbind,
};

static struct platform_driver *drm_sub_drivers[] = {
#ifdef CONFIG_VERISILICON_WRITEBACK_SIF
	&sif_platform_driver,
#endif

	/* put display control driver at start */
	&dc_platform_driver,

#ifdef CONFIG_VERISILICON_BT1120
	/* bt1120 */
	&bt1120_platform_driver,
	/* bt1120 bridge */
	&bt1120_bridge_driver,
#endif

#ifdef CONFIG_VERISILICON_DW_MIPI_DSI
	&dw_mipi_dsi_driver,
#endif

#ifdef CONFIG_VERISILICON_SIMPLE_ENCODER
	/* encoder */
	&simple_encoder_driver,
#endif
};

#define NUM_DRM_DRIVERS (sizeof(drm_sub_drivers) / sizeof(struct platform_driver *))

static struct device_link *links[CONFIG_VERISILICON_DEV_LINK_CNT];

static int compare_dev(struct device *dev, void *data)
{
	return dev == (struct device *)data;
}

static struct component_match *vs_drm_match_add(struct device *dev)
{
	struct component_match *match = NULL;
	int i, j = 0;

	for (i = 0; i < NUM_DRM_DRIVERS; ++i) {
		struct platform_driver *drv = drm_sub_drivers[i];
		struct device *p	    = NULL, *d;
		void *drvdata;

		while ((d = platform_find_device_by_driver(p, &drv->driver))) {
			put_device(p);

			/*
			 * make sure slave device probe first
			 * master device need to first suspend and
			 * last resume. Then drm_mode_config_helper_resume
			 * can recover all state with slave device resumed.
			 */
			drvdata = dev_get_drvdata(d);
			if (!drvdata)
				return ERR_PTR(-EPROBE_DEFER);

			links[j] = device_link_add(dev, d, DL_FLAG_STATELESS | DL_FLAG_PM_RUNTIME);
			if (!links[j]) {
				put_device(d);
				links[j] = NULL;
				dev_err(dev, "Failed to add device_link to %s\n", dev_name(d));
				goto err_links;
			}

			WARN_ON(j == CONFIG_VERISILICON_DEV_LINK_CNT);

			component_match_add(dev, &match, compare_dev, d);
			p = d;
		}
		put_device(p);
	}

	return match ?: ERR_PTR(-ENODEV);

err_links:
	for (j = j - 1; j >= 0; j--) {
		device_link_del(links[j]);
		links[j] = NULL;
	}

	return ERR_PTR(-ENODEV);
}

static int vs_drm_platform_of_probe(struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct device_node *port;
	bool found = false;
	int i;

	if (!np)
		return -ENODEV;

	for (i = 0;; i++) {
		struct device_node *iommu, *dc_node;

		port = of_parse_phandle(np, "ports", i);
		if (!port)
			break;

		if (of_node_name_eq(port->parent, "ports"))
			dc_node = of_node_get(port->parent->parent);
		else
			dc_node = of_node_get(port->parent);

		of_node_put(port);

		if (!of_device_is_available(dc_node)) {
			of_node_put(dc_node);
			continue;
		}

		iommu = of_parse_phandle(dc_node, "iommus", 0);

		/*
		 * if there is a crtc not support iommu, force set all
		 * crtc use non-iommu buffer.
		 */
		if (!iommu || !of_device_is_available(iommu->parent))
			has_iommu = false;

		found = true;

		of_node_put(iommu);
		of_node_put(dc_node);
	}

	if (i == 0) {
		DRM_DEV_ERROR(dev, "missing 'ports' property\n");
		return -ENODEV;
	}

	if (!found) {
		DRM_DEV_ERROR(dev, "No available DC found.\n");
		return -ENODEV;
	}

	return 0;
}

static int vs_drm_platform_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct component_match *match;
	int ret;

	ret = vs_drm_platform_of_probe(dev);
	if (ret)
		return ret;

	match = vs_drm_match_add(dev);
	if (IS_ERR(match))
		return PTR_ERR(match);

	return component_master_add_with_match(dev, &vs_drm_ops, match);
}

static int vs_drm_platform_remove(struct platform_device *pdev)
{
	int i = 0;

	for (i = 0; i < CONFIG_VERISILICON_DEV_LINK_CNT && links[i]; i++)
		device_link_del(links[i]);

	component_master_del(&pdev->dev, &vs_drm_ops);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int vs_drm_suspend(struct device *dev)
{
	struct drm_device *drm = dev_get_drvdata(dev);

	return drm_mode_config_helper_suspend(drm);
}

static int vs_drm_resume(struct device *dev)
{
	struct drm_device *drm = dev_get_drvdata(dev);

	return drm_mode_config_helper_resume(drm);
}
#endif

static SIMPLE_DEV_PM_OPS(vs_drm_pm_ops, vs_drm_suspend, vs_drm_resume);

static const struct of_device_id vs_drm_dt_ids[] = {
	{
		.compatible = "verisilicon,display-subsystem",
	},

	{/* sentinel */},
};

MODULE_DEVICE_TABLE(of, vs_drm_dt_ids);

static struct platform_driver vs_drm_platform_driver = {
	.probe	= vs_drm_platform_probe,
	.remove = vs_drm_platform_remove,

	.driver =
		{
			.name		= DRV_NAME,
			.of_match_table = vs_drm_dt_ids,
			.pm		= &vs_drm_pm_ops,
		},
};

static int __init vs_drm_init(void)
{
	int ret;

	ret = platform_register_drivers(drm_sub_drivers, NUM_DRM_DRIVERS);
	if (ret)
		return ret;

	ret = platform_driver_register(&vs_drm_platform_driver);
	if (ret)
		platform_unregister_drivers(drm_sub_drivers, NUM_DRM_DRIVERS);

	return ret;
}

static void __exit vs_drm_fini(void)
{
	platform_driver_unregister(&vs_drm_platform_driver);
	platform_unregister_drivers(drm_sub_drivers, NUM_DRM_DRIVERS);
}

module_init(vs_drm_init);
module_exit(vs_drm_fini);

MODULE_DESCRIPTION("VeriSilicon DRM Driver");
MODULE_LICENSE("GPL");
