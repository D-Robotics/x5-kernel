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
#include <linux/device.h>
#include <linux/printk.h>
#include <linux/of_graph.h>
#include <linux/of_platform.h>
#include <linux/of_device.h>
#include <linux/delay.h>
#include <linux/media-bus-format.h>
#if IS_ENABLED(CONFIG_X5_SEAMLESS_DISPLAY)
#include <linux/soc/hobot/x5_seamless_display.h>
#endif

#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_vblank.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_modes.h>
#include <drm/drm_of.h>

#include "vs_drv.h"
#include "vs_gem.h"
#include "bt1120_bridge.h"
#include "vs_bt1120_reg.h"

extern struct device *bt1120_dev;

#define CRTC_NAME "crtc-bt1120"

/*
 * Clear DRM_MODE_FLAG_[NP]HSYNC / [NP]VSYNC nibble (bits 0–3) so we can force
 * BT1120-positive sync polarity; see drm_mode.h.
 */
#define BT1120_DRM_SYNC_POLARITY_MASK	0xFFFFFFF0u

static inline struct bt1120_bridge *bridge_to_bt1120_bridge(struct drm_bridge *bridge)
{
	return container_of(bridge, struct bt1120_bridge, bridge);
}

/*
 * [X5-4518] Cold-boot R/B colour-swap fix — vblank-aligned BT1120 capture
 * re-arm in the kernel.
 *
 * The BT1120 online capture latches its YUV422 pixel-pair phase on the CTL
 * BIT(0) rising edge. A rising edge that lands while active video is
 * streaming latches a RANDOM phase: Cb/Cr swap for every pair, pure red
 * shows as pure blue. This happens (a) on cold boot in U-Boot, and (b)
 * EVERY full modeset later (atomic_disable clears BIT(0), then CRTC enable
 * re-raises it while the new stream is already running) — e.g. Xorg
 * switching 1920x1080 (U-Boot handoff) to 1024x600.
 *
 * U-Boot already does the deterministic fix (x5_bt1120.c: pulse BIT(0)
 * low-for-one-register-write INSIDE vertical blanking). Board-verified
 * kernel-side too: 10x deliberate-break + 10x aligned pulse = all
 * recovered, zero recurrence.
 *
 * This hook runs at atomic_enable (after the CRTC is streaming): wait for
 * DC8000's read-only scan-position register to report y >= vdisplay (i.e.
 * inside vertical blanking), then pulse BIT(0) low→high within the same
 * blanking interval. The low time is ~one register write (~100 ns) — the
 * stream never breaks, the screen never glitches.
 *
 * Oracle retry loop (same shape as U-Boot x5_output_hdmi.c): after the
 * vblank-aligned pulse, poll the ONLINE_MISMATCH sticky status; if the
 * capture phase is still wrong, re-pulse, up to X5_BT1120_CAPT_RETRIES
 * times. Safety net for the headless boot path, where U-Boot's own
 * oracle never ran.
 *
 * Guarded: the whole rearm mechanism (struct field, INIT, schedule,
 * cancel, worker) is compiled only with CONFIG_X5_SEAMLESS_DISPLAY so a
 * non-seamless build has no unused worker and no never-initialised work.
 */
#if IS_ENABLED(CONFIG_X5_SEAMLESS_DISPLAY)
#define X5_BT1120_CAPT_RETRIES	10

/*
 * Mismatch oracle, kernel twin of U-Boot x5_bt1120_capture_is_bad().
 * Unlike U-Boot (clear-then-read), never WRITE IRQ_STATUS: the ISR is
 * active and clears by write-back, so writing would race it. Snapshot
 * the bit, wait ~2 frames, read again — a mismatch at entry or during
 * the window counts.
 */
static bool bt1120_capture_is_bad(struct vs_bt1120 *bt1120)
{
	u32 st;

	st = readl(bt1120->base + REG_BT1120_IRQ_STATUS) &
	     ONLINE_MISMATCH_IRQ_STATUS_MASK;
	msleep(40);
	return !!(st | (readl(bt1120->base + REG_BT1120_IRQ_STATUS) &
			ONLINE_MISMATCH_IRQ_STATUS_MASK));
}

/*
 * Vblank-aligned capture re-arm pulse: wait for the DC8000 scan
 * position (GCREG_DISPLAY_CURRENT_LOCATION, X=[15:0], Y=[27:16], RO)
 * to reach vertical blanking, then pulse CTL BIT(0) low for ~one
 * register write and back — all inside the same blanking interval.
 * Tight poll: jiffies checks add latency between the read and the
 * pulse (60 Hz frame is 16.7 ms, vblank ~0.4 ms).
 */
static int bt1120_capture_pulse_vblank(struct vs_bt1120 *bt1120, u32 vactive)
{
	void __iomem *loc_reg;
	u32 loc, y;
	u32 ctl;
	long timeout;

	loc_reg = ioremap(0x3e000000 + 0x2118, 4);
	if (!loc_reg)
		return -ENOMEM;

	timeout = 2000000; /* ~200 ms worst case at ~100 ns/read */
	while (timeout--) {
		loc = readl(loc_reg);
		y   = (loc >> 16) & 0xFFF;
		if (y > vactive)
			break;
	}
	iounmap(loc_reg);

	if (y <= vactive)
		return -ETIME; /* no vblank observed — leave BIT(0) untouched */

	ctl = readl(bt1120->base + REG_BT1120_CTL);
	writel(ctl & ~BIT(0), bt1120->base + REG_BT1120_CTL); /* ~100 ns low */
	writel(ctl | BIT(0), bt1120->base + REG_BT1120_CTL);
	dev_info(bt1120->dev,
		"bt1120_bridge: vblank-aligned capture re-arm (y=%u > vactive=%u)\n",
		y, vactive);
	return 0;
}

static void bt1120_vblank_rearm_work(struct work_struct *work)
{
	struct bt1120_bridge *bt1120_bridge =
		container_of(work, struct bt1120_bridge, rearm_work.work);
	struct vs_bt1120 *bt1120 = dev_get_drvdata(bt1120_bridge->parent);
	struct drm_crtc *crtc = bt1120_bridge->bridge.encoder ?
				bt1120_bridge->bridge.encoder->crtc : NULL;
	const struct drm_display_mode *mode;
	u32 vactive;
	int roll;

	if (!bt1120 || !bt1120->is_online || !bt1120->base)
		return;

	mode = crtc && crtc->state ? &crtc->state->adjusted_mode : NULL;
	if (!mode)
		return;
	vactive = mode->vdisplay;

	if (bt1120_capture_pulse_vblank(bt1120, vactive)) {
		dev_warn(bt1120->dev,
			 "bt1120_bridge: vblank wait timed out, capture phase unaligned\n");
		return;
	}

	/* safety net: mismatch-oracle retries (same shape as U-Boot) */
	for (roll = 0; roll < X5_BT1120_CAPT_RETRIES; roll++) {
		if (!bt1120_capture_is_bad(bt1120))
			break;
		dev_info(bt1120->dev,
			 "bt1120_bridge: capture bad (roll %d), vblank re-arm...\n",
			 roll);
		bt1120_capture_pulse_vblank(bt1120, vactive);
	}
	dev_info(bt1120->dev,
		 "bt1120_bridge: capture phase ok after %d roll(s)\n", roll);
	if (roll == X5_BT1120_CAPT_RETRIES)
		dev_warn(bt1120->dev,
			 "bt1120_bridge: capture still bad after %d rolls\n",
			 X5_BT1120_CAPT_RETRIES);
}
#endif /* IS_ENABLED(CONFIG_X5_SEAMLESS_DISPLAY) */

#if IS_ENABLED(CONFIG_X5_SEAMLESS_DISPLAY)
static void bt1120_bridge_atomic_enable(struct drm_bridge *bridge,
					struct drm_bridge_state *old_bridge_state)
{
	struct bt1120_bridge *bt1120_bridge = bridge_to_bt1120_bridge(bridge);
	struct vs_bt1120 *bt1120	    = dev_get_drvdata(bt1120_bridge->parent);

	/*
	 * Run the re-arm DEFERRED, not inline: at atomic_enable the CRTC has
	 * barely started scanning the new mode — the DC8000 scan position is
	 * still settling (timing reprogram, pipeline restart), so a pulse
	 * here latches against a half-switched stream. Schedule 500 ms out,
	 * when the mode is running stable — matching the conditions under
	 * which the board experiments (stable stream + vblank pulse) were
	 * verified to deterministically fix the phase.
	 */
	if (bt1120 && bt1120->is_online)
		schedule_delayed_work(&bt1120_bridge->rearm_work,
				      msecs_to_jiffies(500));
}
#endif /* IS_ENABLED(CONFIG_X5_SEAMLESS_DISPLAY) */

static void bt1120_bridge_atomic_disable(struct drm_bridge *bridge,
					 struct drm_bridge_state *old_bridge_state)
{
	struct bt1120_bridge *bt1120_bridge = bridge_to_bt1120_bridge(bridge);
	struct vs_bt1120 *bt1120	    = dev_get_drvdata(bt1120_bridge->parent);

#if IS_ENABLED(CONFIG_X5_SEAMLESS_DISPLAY)
	cancel_delayed_work_sync(&bt1120_bridge->rearm_work);
#endif

	dev_dbg(bt1120_bridge->parent,
		"[X5_DISP] bt1120_bridge atomic_disable: is_online=%d parent=%s\n",
		bt1120->is_online, dev_name(bt1120_bridge->parent));

	if (!bt1120->is_online)
		return;

#if IS_ENABLED(CONFIG_X5_SEAMLESS_DISPLAY)
	/* Align with sii902x/vs_drm: simplefb in /chosen implies U-Boot handoff even if
	 * d-robotics,seamless-display-state is missing. Skip is per-bridge (atomic_t).
	 */
	if (x5_seamless_display_active() || x5_chosen_has_simple_framebuffer()) {
		if (atomic_cmpxchg(&bt1120_bridge->seamless_skip_disable_once, 1, 0) == 1) {
			dev_info(bt1120_bridge->parent,
				 "bt1120_bridge: seamless HDMI handoff, skip first atomic_disable\n");
			return;
		}
	}
#endif

	/* disable bt1120_en*/
	bt1120_disable(bt1120);
}

static struct drm_crtc *drm_encoder_get_crtc(struct drm_encoder *encoder)
{
	struct drm_connector *connector;
	struct drm_device *dev = encoder->dev;
	bool uses_atomic       = false;
	struct drm_connector_list_iter conn_iter;

	drm_connector_list_iter_begin(dev, &conn_iter);
	drm_for_each_connector_iter (connector, &conn_iter) {
		if (!connector->state)
			continue;

		uses_atomic = true;

		if (connector->state->best_encoder != encoder)
			continue;

		drm_connector_list_iter_end(&conn_iter);
		return connector->state->crtc;
	}
	drm_connector_list_iter_end(&conn_iter);

	if (uses_atomic)
		return NULL;

	return encoder->crtc;
}

static void bt1120_bridge_atomic_pre_enable(struct drm_bridge *bridge,
					    struct drm_bridge_state *old_bridge_state)
{
	struct bt1120_bridge *bt1120_bridge    = bridge_to_bt1120_bridge(bridge);
	struct drm_crtc *drm_crtc	       = drm_encoder_get_crtc(bridge->encoder);
	struct drm_display_mode *adjusted_mode = &drm_crtc->state->adjusted_mode;

	dev_dbg(bt1120_bridge->parent,
		"[X5_DISP] bt1120_bridge atomic_pre_enable: crtc=%s %ux%u@%u dotclock_khz=%u htot=%u vtot=%u flags_before=0x%x\n",
		drm_crtc->name, adjusted_mode->hdisplay, adjusted_mode->vdisplay,
		drm_mode_vrefresh(adjusted_mode), adjusted_mode->clock,
		adjusted_mode->htotal, adjusted_mode->vtotal, adjusted_mode->flags);

	/* bt1120 output sync signals are positive active */
	adjusted_mode->flags &= BT1120_DRM_SYNC_POLARITY_MASK;
	adjusted_mode->flags |= DRM_MODE_FLAG_PHSYNC;
	adjusted_mode->flags |= DRM_MODE_FLAG_PVSYNC;

	dev_dbg(bt1120_bridge->parent,
		"[X5_DISP] bt1120_bridge atomic_pre_enable: flags_after=0x%x (PHSYNC|PVSYNC)\n",
		adjusted_mode->flags);
}

static int bt1120_bridge_attach(struct drm_bridge *bridge, enum drm_bridge_attach_flags flags)
{
	struct bt1120_bridge *bt1120_bridge = bridge_to_bt1120_bridge(bridge);

	if (!bridge->encoder) {
		DRM_ERROR("Parent encoder object not found\n");
		return -ENODEV;
	}

	if (!bt1120_bridge->next_bridge)
		return 0;

	return drm_bridge_attach(bridge->encoder, bt1120_bridge->next_bridge, bridge, flags);
}

static void bt1120_bridge_mode_set(struct drm_bridge *bridge, const struct drm_display_mode *mode,
				   const struct drm_display_mode *adjusted_mode)
{
	struct bt1120_bridge *bt1120_bridge = bridge_to_bt1120_bridge(bridge);
	struct vs_bt1120 *bt1120	    = dev_get_drvdata(bt1120_bridge->parent);

	dev_dbg(bt1120_bridge->parent,
		"[X5_DISP] bt1120_bridge mode_set: is_online=%d %ux%u@%u dotclock_khz=%u htot=%u vtot=%u flags=0x%x hsync(%u-%u) vsync(%u-%u)\n",
		bt1120->is_online, mode->hdisplay, mode->vdisplay, drm_mode_vrefresh(mode),
		mode->clock, mode->htotal, mode->vtotal, mode->flags,
		mode->hsync_start, mode->hsync_end, mode->vsync_start, mode->vsync_end);

	bt1120_set_online_configs(bt1120, mode);
}

static u32 *bt1120_bridge_get_input_bus_fmts(struct drm_bridge *bridge,
					     struct drm_bridge_state *bridge_state,
					     struct drm_crtc_state *crtc_state,
					     struct drm_connector_state *conn_state, u32 output_fmt,
					     unsigned int *num_input_fmts)
{
	struct bt1120_bridge *bt1120_bridge = bridge_to_bt1120_bridge(bridge);
	const struct drm_crtc *crtc	    = conn_state->crtc;
	struct vs_bt1120 *bt1120	    = dev_get_drvdata(bt1120_bridge->parent);
	u32 *input_fmts			    = NULL;

	if (!strcmp(crtc->name, CRTC_NAME))
		bt1120->is_online = false;
	else
		bt1120->is_online = true;

	dev_dbg(bt1120_bridge->parent,
		"[X5_DISP] bt1120_bridge get_input_bus_fmts: crtc=%s -> is_online=%d output_fmt=0x%x\n",
		crtc->name, bt1120->is_online, output_fmt);

	switch (output_fmt) {
	case MEDIA_BUS_FMT_FIXED:
	case MEDIA_BUS_FMT_UYVY8_1X16:
	case MEDIA_BUS_FMT_VYUY8_1X16:
	case MEDIA_BUS_FMT_YUYV8_1X16:
	case MEDIA_BUS_FMT_YVYU8_1X16:
		break;
	default:
		*num_input_fmts = 0;
		return NULL;
	}

	input_fmts = kmalloc(sizeof(u32), GFP_KERNEL);
	if (!input_fmts) {
		*num_input_fmts = 0;
		return NULL;
	}

	*num_input_fmts = 1;

	if (bt1120->is_online)
		input_fmts[0] = MEDIA_BUS_FMT_RGB888_1X24;
	else
		input_fmts[0] = MEDIA_BUS_FMT_UYVY8_1X16;

	return input_fmts;
}

static const struct drm_bridge_funcs bt1120_bridge_funcs = {
	.atomic_get_input_bus_fmts = bt1120_bridge_get_input_bus_fmts,
	.attach			   = bt1120_bridge_attach,
	.mode_set		   = bt1120_bridge_mode_set,
	.atomic_pre_enable	   = bt1120_bridge_atomic_pre_enable,
#if IS_ENABLED(CONFIG_X5_SEAMLESS_DISPLAY)
	/* [X5-4518] deferred vblank rearm needs the atomic enable/disable hooks;
	 * without seamless the default no-op helpers are equivalent.
	 */
	.atomic_enable		   = bt1120_bridge_atomic_enable,
#endif
	.atomic_disable		   = bt1120_bridge_atomic_disable,
	.atomic_reset		   = drm_atomic_helper_bridge_reset,
	.atomic_duplicate_state	   = drm_atomic_helper_bridge_duplicate_state,
	.atomic_destroy_state	   = drm_atomic_helper_bridge_destroy_state,
};

static int bt1120_bridge_bind(struct device *dev, struct device *master, void *data)
{
	return 0;
}

static void bt1120_bridge_unbind(struct device *dev, struct device *master, void *data) {}

static const struct component_ops bt1120_bridge_component_ops = {
	.bind	= bt1120_bridge_bind,
	.unbind = bt1120_bridge_unbind,
};

static const struct of_device_id bt1120_bridge_driver_dt_match[] = {
	{.compatible = "verisilicon, bt1120-bridge"},
	{.compatible = "verisilicon, bt1120-bridge-wb"},
	{}};
MODULE_DEVICE_TABLE(of, bt1120_bridge_driver_dt_match);

static int bt1120_bridge_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct bt1120_bridge *bt1120_bridge;
	u32 ret = 0;

	bt1120_bridge = devm_kzalloc(dev, sizeof(*bt1120_bridge), GFP_KERNEL);
	if (!bt1120_bridge)
		return -ENOMEM;

	bt1120_bridge->dev    = dev;
	bt1120_bridge->parent = bt1120_dev;

#if IS_ENABLED(CONFIG_X5_SEAMLESS_DISPLAY)
	atomic_set(&bt1120_bridge->seamless_skip_disable_once, 1);
	INIT_DELAYED_WORK(&bt1120_bridge->rearm_work, bt1120_vblank_rearm_work);
#endif

	dev_set_drvdata(dev, bt1120_bridge);

	/* Get the next bridge in the pipeline. */
	bt1120_bridge->next_bridge = devm_drm_of_get_bridge(dev, dev->of_node, 1, 0);
	if (IS_ERR(bt1120_bridge->next_bridge)) {
		ret = PTR_ERR(bt1120_bridge->next_bridge);

		bt1120_bridge->next_bridge = NULL;

		if (ret == -ENODEV)
			goto out;

		return ret;
	}

out:
	bt1120_bridge->bridge.funcs   = &bt1120_bridge_funcs;
	bt1120_bridge->bridge.of_node = dev->of_node;
	drm_bridge_add(&bt1120_bridge->bridge);

	return component_add(dev, &bt1120_bridge_component_ops);
}

static int bt1120_bridge_remove(struct platform_device *pdev)
{
	struct device *dev		    = &pdev->dev;
	struct bt1120_bridge *bt1120_bridge = dev_get_drvdata(dev);

	drm_bridge_remove(&bt1120_bridge->bridge);

	component_del(dev, &bt1120_bridge_component_ops);
	dev_set_drvdata(dev, NULL);

	return 0;
}

struct platform_driver bt1120_bridge_driver = {
	.probe	= bt1120_bridge_probe,
	.remove = bt1120_bridge_remove,
	.driver =
		{
			.name		= "vs-bt1120-bridge",
			.of_match_table = of_match_ptr(bt1120_bridge_driver_dt_match),
		},
};

MODULE_DESCRIPTION("VeriSilicon BT1120 Bridge Driver");
MODULE_LICENSE("GPL");
