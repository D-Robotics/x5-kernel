// SPDX-License-Identifier: GPL-2.0

#include <linux/component.h>
#include <linux/clk.h>
#include <linux/of_device.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>
#include <linux/phy/phy.h>
#include <linux/iopoll.h>
#include <linux/phy.h>
#include <linux/phy/phy-mipi-dphy.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/debugfs.h>

#include <drm/drm_bridge.h>
#include <drm/drm_encoder.h>
#include <drm/drm_modes.h>
#include <drm/drm_of.h>
#include <drm/drm_print.h>
#include <drm/drm_panel.h>

#include <drm/drm_file.h>

#include "dw_mipi_csi_hal.h"
#include "dw_mipi_csi_reg.h"

#define CREATE_VPG_FILE(attr) \
	debugfs_create_file("vpg_" #attr, 0644, csi->debugfs_dir, csi, &debugfs_##attr##_fops)

struct csi_display_mode {
	long htotal;
	long vtotal;
	long vrefresh;
};

#define DEFINE_VPG_ATTR_RW(attr)                                                                   \
	static ssize_t debugfs_##attr##_read(struct file *f, char __user *buf, size_t size,        \
					     loff_t *pos)                                          \
	{                                                                                          \
		struct dw_mipi_csi *csi = f->f_inode->i_private;                                   \
		char tmp[32];                                                                      \
		int len;                                                                           \
		if (!csi)                                                                          \
			return -EINVAL;                                                            \
		len = snprintf(tmp, sizeof(tmp), "%u\n", csi->vpg_mode.attr);                      \
		return simple_read_from_buffer(buf, size, pos, tmp, len);                          \
	}                                                                                          \
	static ssize_t debugfs_##attr##_write(struct file *f, const char __user *buf, size_t size, \
					      loff_t *pos)                                         \
	{                                                                                          \
		struct dw_mipi_csi *csi = f->f_inode->i_private;                                   \
		u32 val;                                                                           \
		int ret;                                                                           \
		if (!csi || *pos != 0 || size >= 16)                                               \
			return -EINVAL;                                                            \
		ret = kstrtou32_from_user(buf, size, 0, &val);                                     \
		if (ret)                                                                           \
			return ret;                                                                \
		csi->vpg_mode.attr = val;                                                          \
		return size;                                                                       \
	}                                                                                          \
	static const struct file_operations debugfs_##attr##_fops = {                              \
		.owner	= THIS_MODULE,                                                             \
		.read	= debugfs_##attr##_read,                                                   \
		.write	= debugfs_##attr##_write,                                                  \
		.llseek = seq_lseek,                                                               \
	};

DEFINE_VPG_ATTR_RW(width)
DEFINE_VPG_ATTR_RW(height)
DEFINE_VPG_ATTR_RW(pixel_clock)
DEFINE_VPG_ATTR_RW(hdisplay)
DEFINE_VPG_ATTR_RW(hsync_start)
DEFINE_VPG_ATTR_RW(hsync_end)
DEFINE_VPG_ATTR_RW(htotal)
DEFINE_VPG_ATTR_RW(hskew)
DEFINE_VPG_ATTR_RW(vdisplay)
DEFINE_VPG_ATTR_RW(vsync_start)
DEFINE_VPG_ATTR_RW(vsync_end)
DEFINE_VPG_ATTR_RW(vtotal)
DEFINE_VPG_ATTR_RW(refresh_rate)

#define DEFINE_DPHY_OVERRIDE_RW(field)                                                          \
	static ssize_t debugfs_override_##field##_read(struct file *f, char __user *buf,        \
						       size_t size, loff_t *pos)                \
	{                                                                                       \
		struct dw_mipi_csi *csi = f->f_inode->i_private;                                \
		char tmp[32];                                                                   \
		int len;                                                                        \
		if (!csi)                                                                       \
			return -EINVAL;                                                         \
		len = snprintf(tmp, sizeof(tmp), "%u\n", csi->override_##field);                \
		return simple_read_from_buffer(buf, size, pos, tmp, len);                       \
	}                                                                                       \
	static ssize_t debugfs_override_##field##_write(struct file *f, const char __user *buf, \
							size_t size, loff_t *pos)               \
	{                                                                                       \
		struct dw_mipi_csi *csi = f->f_inode->i_private;                                \
		unsigned long val;                                                              \
		int ret;                                                                        \
		if (!csi || *pos != 0 || size >= 16)                                            \
			return -EINVAL;                                                         \
		ret = kstrtoul_from_user(buf, size, 0, &val);                                   \
		if (ret)                                                                        \
			return ret;                                                             \
		csi->override_##field = (u32)val;                                               \
		pr_info("dw_mipi_csi: override_%s set to %lu\n", #field, val);                  \
		return size;                                                                    \
	}                                                                                       \
	static const struct file_operations debugfs_override_##field##_fops = {                 \
		.owner	= THIS_MODULE,                                                          \
		.read	= debugfs_override_##field##_read,                                      \
		.write	= debugfs_override_##field##_write,                                     \
		.llseek = seq_lseek,                                                            \
	};

DEFINE_DPHY_OVERRIDE_RW(lpx);
DEFINE_DPHY_OVERRIDE_RW(hs_prepare);
DEFINE_DPHY_OVERRIDE_RW(hs_zero);
DEFINE_DPHY_OVERRIDE_RW(hs_trail);
DEFINE_DPHY_OVERRIDE_RW(hs_exit);
DEFINE_DPHY_OVERRIDE_RW(clk_prepare);
DEFINE_DPHY_OVERRIDE_RW(clk_zero);
DEFINE_DPHY_OVERRIDE_RW(clk_trail);
DEFINE_DPHY_OVERRIDE_RW(clk_post);

/**
 * debugfs_clk_continuous_read - Read current continuous clock mode
 * @f: File structure pointer.
 * @buf: Buffer to hold the output.
 * @size: Size of the buffer.
 * @pos: File pointer position.
 *
 * This function reads the current clock mode (continuous or non-continuous).
 */
static ssize_t debugfs_clk_continuous_read(struct file *f, char __user *buf, size_t size,
					   loff_t *pos)
{
	struct dw_mipi_csi *csi = f->f_inode->i_private;
	char tmp[8];
	int len;

	if (!csi)
		return -EINVAL;

	/* Print current clock mode */
	len = snprintf(tmp, sizeof(tmp), "%u\n", csi->clk_continuous ? 1 : 0);
	return simple_read_from_buffer(buf, size, pos, tmp, len);
}

/**
 * debugfs_clk_continuous_write - Set clock mode to continuous or non-continuous
 * @f: File structure pointer.
 * @buf: Buffer holding the new value.
 * @size: Size of the buffer.
 * @pos: File pointer position.
 *
 * This function sets the clock mode to continuous or non-continuous based on user input.
 */
static ssize_t debugfs_clk_continuous_write(struct file *f, const char __user *buf, size_t size,
					    loff_t *pos)
{
	struct dw_mipi_csi *csi = f->f_inode->i_private;
	unsigned long val;
	int ret;

	if (!csi || *pos != 0 || size >= 16)
		return -EINVAL;

	/* Convert input to unsigned integer */
	ret = kstrtoul_from_user(buf, size, 0, &val);
	if (ret)
		return ret;

	if (val > 1)
		return -EINVAL;

	csi->clk_continuous = (val != 0);

	pr_info("dw_mipi_csi: clk_continuous set to %lu\n", val);

	/* Apply the new clock mode */
	dw_mipi_csi_apply_clk_continuous(csi);

	return size;
}

static const struct file_operations debugfs_clk_continuous_fops = {
	.owner	= THIS_MODULE,
	.read	= debugfs_clk_continuous_read,
	.write	= debugfs_clk_continuous_write,
	.llseek = seq_lseek,
};

static ssize_t debugfs_override_enable_read(struct file *f, char __user *buf, size_t size,
					    loff_t *pos)
{
	struct dw_mipi_csi *csi = f->f_inode->i_private;
	char tmp[8];
	int len;

	if (!csi)
		return -EINVAL;

	len = snprintf(tmp, sizeof(tmp), "%u\n", csi->dphy_timing_override_enable);
	return simple_read_from_buffer(buf, size, pos, tmp, len);
}

static ssize_t debugfs_override_enable_write(struct file *f, const char __user *buf, size_t size,
					     loff_t *pos)
{
	struct dw_mipi_csi *csi = f->f_inode->i_private;
	unsigned long val;
	int ret;

	if (!csi || *pos != 0 || size >= 16)
		return -EINVAL;

	ret = kstrtoul_from_user(buf, size, 0, &val);
	if (ret)
		return ret;
	if (val > 1)
		return -EINVAL;

	csi->dphy_timing_override_enable = val;
	pr_info("dw_mipi_csi: dphy_timing_override_enable = %lu\n", val);
	return size;
}

static const struct file_operations debugfs_override_enable_fops = {
	.owner	= THIS_MODULE,
	.read	= debugfs_override_enable_read,
	.write	= debugfs_override_enable_write,
	.llseek = seq_lseek,
};

static ssize_t debugfs_timing_margin_enable_read(struct file *f, char __user *buf, size_t size,
						 loff_t *pos)
{
	struct dw_mipi_csi *csi = f->f_inode->i_private;
	char tmp[16];
	int len;

	if (!csi)
		return -EINVAL;

	len = snprintf(tmp, sizeof(tmp), "%u\n", csi->timing_margin_enable);
	return simple_read_from_buffer(buf, size, pos, tmp, len);
}

static ssize_t debugfs_timing_margin_enable_write(struct file *f, const char __user *buf,
						  size_t size, loff_t *pos)
{
	struct dw_mipi_csi *csi = f->f_inode->i_private;
	unsigned long val;
	int ret;

	if (!csi || *pos != 0 || size >= 16)
		return -EINVAL;

	ret = kstrtoul_from_user(buf, size, 0, &val);
	if (ret)
		return ret;

	csi->timing_margin_enable = (val != 0);
	return size;
}

static const struct file_operations debugfs_timing_margin_enable_fops = {
	.owner	= THIS_MODULE,
	.read	= debugfs_timing_margin_enable_read,
	.write	= debugfs_timing_margin_enable_write,
	.llseek = seq_lseek,
};

static ssize_t debugfs_interlaced_read(struct file *f, char __user *buf, size_t size, loff_t *pos)
{
	struct dw_mipi_csi *csi = f->f_inode->i_private;
	char tmp[8];
	int len;

	if (!csi)
		return -EINVAL;

	len = snprintf(tmp, sizeof(tmp), "%u\n", csi->vpg_mode.interlaced ? 1 : 0);
	return simple_read_from_buffer(buf, size, pos, tmp, len);
}

static ssize_t debugfs_interlaced_write(struct file *f, const char __user *buf, size_t size,
					loff_t *pos)
{
	struct dw_mipi_csi *csi = f->f_inode->i_private;
	u32 val;
	int ret;

	if (!csi || *pos != 0 || size >= 16)
		return -EINVAL;

	ret = kstrtou32_from_user(buf, size, 0, &val);
	if (ret || (val != 0 && val != 1))
		return -EINVAL;

	csi->vpg_mode.interlaced = (val != 0);
	return size;
}

static const struct file_operations debugfs_interlaced_fops = {
	.owner	= THIS_MODULE,
	.read	= debugfs_interlaced_read,
	.write	= debugfs_interlaced_write,
	.llseek = seq_lseek,
};

static ssize_t debugfs_irq_enable_write(struct file *f, const char __user *buf, size_t size,
					loff_t *pos)
{
	struct dw_mipi_csi *csi = f->f_inode->i_private;
	u32 val;
	int ret;

	if (!csi || *pos != 0 || size >= 16)
		return -EINVAL;

	ret = kstrtou32_from_user(buf, size, 0, &val);
	if (ret)
		return ret;

	if (val)
		dw_mipi_csi_hal_irq_enable(csi);
	else
		dw_mipi_csi_hal_irq_disable(csi);

	return size;
}

static const struct file_operations debugfs_irq_enable_fops = {
	.owner	= THIS_MODULE,
	.write	= debugfs_irq_enable_write,
	.llseek = seq_lseek,
};

static const char *const pattern_string[] = {"OFF", "H", "V"};

static u8 dw_mipi_csi_get_pattern(struct dw_mipi_csi *csi)
{
	u32 val = dw_read(csi, CSI_VPG_STATUS);

	if (!(val & VPG_EN))
		return DW_MIPI_CSI_PATTERN_OFF;

	val = dw_read(csi, CSI_VPG_MODE_CFG);

	if (val & VPG_ORIENTATION)
		return DW_MIPI_CSI_PATTERN_H;

	return DW_MIPI_CSI_PATTERN_V;
}

static int dw_mipi_csi_set_pattern(struct dw_mipi_csi *csi, u8 pattern)
{
	return dw_mipi_csi_hal_vpg_init(csi, pattern);
}

static ssize_t debugfs_pattern_read(struct file *f, char __user *buf, size_t size, loff_t *pos)
{
	struct dw_mipi_csi *csi = f->f_inode->i_private;
	char tmp[64];
	int len;
	u8 pattern;

	if (!csi)
		return -EINVAL;

	pattern = dw_mipi_csi_get_pattern(csi);

	len = snprintf(tmp, sizeof(tmp), "%s [OFF, H, V]\n", pattern_string[pattern]);

	return simple_read_from_buffer(buf, size, pos, tmp, len);
}

static ssize_t debugfs_pattern_write(struct file *f, const char __user *buf, size_t size,
				     loff_t *pos)
{
	struct dw_mipi_csi *csi = f->f_inode->i_private;
	char cmd[16]		= {0};

	if (!csi || *pos != 0 || size >= sizeof(cmd))
		return -EINVAL;

	if (copy_from_user(cmd, buf, size))
		return -EFAULT;

	cmd[size] = '\0';

	if (!strncmp(cmd, "OFF", 3))
		return dw_mipi_csi_set_pattern(csi, DW_MIPI_CSI_PATTERN_OFF) ? -EINVAL : size;

	if (!strncmp(cmd, "H", 1))
		return dw_mipi_csi_set_pattern(csi, DW_MIPI_CSI_PATTERN_H) ? -EINVAL : size;

	if (!strncmp(cmd, "V", 1))
		return dw_mipi_csi_set_pattern(csi, DW_MIPI_CSI_PATTERN_V) ? -EINVAL : size;

	return -EINVAL;
}

static const struct file_operations debugfs_pattern_fops = {
	.owner	= THIS_MODULE,
	.read	= debugfs_pattern_read,
	.write	= debugfs_pattern_write,
	.llseek = seq_lseek,
};

static ssize_t debugfs_hsclk_override_read(struct file *f, char __user *buf, size_t size,
					   loff_t *pos)
{
	struct dw_mipi_csi *csi = f->f_inode->i_private;
	char tmp[32];
	int len;

	if (!csi)
		return -EINVAL;

	len = snprintf(tmp, sizeof(tmp), "%u\n", csi->hsclk_override_khz);
	return simple_read_from_buffer(buf, size, pos, tmp, len);
}

static ssize_t debugfs_hsclk_override_write(struct file *f, const char __user *buf, size_t size,
					    loff_t *pos)
{
	struct dw_mipi_csi *csi = f->f_inode->i_private;
	unsigned long val;
	int ret;

	if (!csi || *pos != 0 || size >= 16)
		return -EINVAL;

	ret = kstrtoul_from_user(buf, size, 0, &val);
	if (ret)
		return ret;

	if (val && (val < CSI_HSCLK_MIN_KHZ || val > CSI_HSCLK_MAX_KHZ))
		return -EINVAL;

	csi->hsclk_override_khz = (u32)val;

	pr_info("dw_mipi_csi: hsclk_override_khz = %u\n", csi->hsclk_override_khz);

	return size;
}

static const struct file_operations debugfs_hsclk_override_fops = {
	.owner	= THIS_MODULE,
	.read	= debugfs_hsclk_override_read,
	.write	= debugfs_hsclk_override_write,
	.llseek = seq_lseek,
};

#ifdef CONFIG_HOBOT_CSI_IDI
static ssize_t debugfs_bypass_write(struct file *f, const char __user *buf, size_t size,
				    loff_t *pos)
{
	struct dw_mipi_csi *csi = f->f_inode->i_private;
	struct drm_display_mode drm_mode;
	char cmd[32];
	int ret;

	if (*pos != 0 || size >= 32)
		return -EINVAL;

	ret = strncpy_from_user(cmd, buf, size);
	if (ret < 0)
		return ret;

	cmd[size] = '\0';

	if (!strncmp(cmd, "OFF", 3))
		csi->bypass_en = false;
	else if (!strncmp(cmd, "ON", 2))
		csi->bypass_en = true;
	else
		return -EINVAL;

	if (!csi->bypass_en)
		goto out;

	csi_get_display_mode(&csi->connector, &drm_mode);

	if (csi_idi_mode_set(csi, &drm_mode))
		return -EINVAL;

out:
	return size;
}

static ssize_t debugfs_bypass_read(struct file *f, char __user *buf, size_t size, loff_t *pos)
{
	struct dw_mipi_csi *csi = f->f_inode->i_private;
	char status[32];

	snprintf(status, sizeof(status), "%s [%s, %s]\n", csi->bypass_en ? "ON" : "OFF", "ON",
		 "OFF");

	return simple_read_from_buffer(buf, size, pos, status, strlen(status));
}

static const struct file_operations debugfs_bypass_fops = {
	.owner	= THIS_MODULE,
	.read	= debugfs_bypass_read,
	.write	= debugfs_bypass_write,
	.llseek = seq_lseek,
};

#endif

static ssize_t debugfs_lanes_read(struct file *f, char __user *buf, size_t size, loff_t *pos)
{
	struct dw_mipi_csi *csi = f->f_inode->i_private;
	char tmp[32];
	int len;

	if (!csi)
		return -EINVAL;

	len = snprintf(tmp, sizeof(tmp), "%u [0~4]\n", csi->lanes);

	return simple_read_from_buffer(buf, size, pos, tmp, len);
}

static ssize_t debugfs_lanes_write(struct file *f, const char __user *buf, size_t size, loff_t *pos)
{
	struct dw_mipi_csi *csi = f->f_inode->i_private;
	unsigned long val;
	int ret;

	if (!csi || *pos != 0 || size >= 16)
		return -EINVAL;

	ret = kstrtoul_from_user(buf, size, 0, &val);
	if (ret)
		return ret;

	if (val > 4)
		return -EINVAL;

	csi->lanes = (u8)val;

	return size;
}

static const struct file_operations debugfs_lanes_fops = {
	.owner	= THIS_MODULE,
	.read	= debugfs_lanes_read,
	.write	= debugfs_lanes_write,
	.llseek = seq_lseek,
};

static ssize_t debugfs_bus_fmt_read(struct file *f, char __user *buf, size_t size, loff_t *pos)
{
	struct dw_mipi_csi *csi = f->f_inode->i_private;
	char tmp[64];
	int len;

	if (!csi)
		return -EINVAL;

	len = snprintf(tmp, sizeof(tmp), "%u [%d~%d]\n", csi->bus_fmt, DW_BUS_FMT_RGB565_1,
		       DW_BUS_FMT_COUNT - 1);

	return simple_read_from_buffer(buf, size, pos, tmp, len);
}

static ssize_t debugfs_bus_fmt_write(struct file *f, const char __user *buf, size_t size,
				     loff_t *pos)
{
	struct dw_mipi_csi *csi = f->f_inode->i_private;
	unsigned long val;
	int ret;

	if (!csi || *pos != 0 || size >= 16)
		return -EINVAL;

	ret = kstrtoul_from_user(buf, size, 0, &val);
	if (ret)
		return ret;

	if (val < DW_BUS_FMT_RGB565_1 || val >= DW_BUS_FMT_COUNT)
		return -EINVAL;

	csi->bus_fmt = (u32)val;

	return size;
}

static const struct file_operations debugfs_bus_fmt_fops = {
	.owner	= THIS_MODULE,
	.read	= debugfs_bus_fmt_read,
	.write	= debugfs_bus_fmt_write,
	.llseek = seq_lseek,
};

static ssize_t debugfs_ipi_mode_read(struct file *f, char __user *buf, size_t size, loff_t *pos)
{
	struct dw_mipi_csi *csi = f->f_inode->i_private;
	char tmp[64];
	int len;

	if (!csi)
		return -EINVAL;

	len = snprintf(tmp, sizeof(tmp), "%u [0:auto 1:cut-through 2:store-forward]\n",
		       csi->ipi_mode_override);

	return simple_read_from_buffer(buf, size, pos, tmp, len);
}

static ssize_t debugfs_ipi_mode_write(struct file *f, const char __user *buf, size_t size,
				      loff_t *pos)
{
	struct dw_mipi_csi *csi = f->f_inode->i_private;
	unsigned long val;
	int ret;

	if (!csi || *pos != 0 || size >= 16)
		return -EINVAL;

	ret = kstrtoul_from_user(buf, size, 0, &val);
	if (ret)
		return ret;

	if (val > IPI_MODE_FORCE_STORE_FORWARD)
		return -EINVAL;

	csi->ipi_mode_override = (u8)val;

	pr_info("dw_mipi_csi: ipi_mode_override set to %lu\n", val);

	return size;
}

static const struct file_operations debugfs_ipi_mode_fops = {
	.owner	= THIS_MODULE,
	.read	= debugfs_ipi_mode_read,
	.write	= debugfs_ipi_mode_write,
	.llseek = seq_lseek,
};

static void csi_dump_mode(struct seq_file *s, const char *name, struct csi_mode_info *mode)
{
	if (!mode) {
		seq_printf(s, "%s: <null>\n", name);
		return;
	}

	seq_printf(s, "%s:\n", name);
	seq_printf(s, "  Resolution: %ux%u\n", mode->width, mode->height);
	seq_printf(s, "  Pixel clock: %u kHz\n", mode->pixel_clock);
	seq_printf(s, "  Refresh rate: %u Hz\n", mode->refresh_rate);
	seq_printf(s, "  Horizontal: display=%u start=%u end=%u total=%u skew=%u\n", mode->hdisplay,
		   mode->hsync_start, mode->hsync_end, mode->htotal, mode->hskew);
	seq_printf(s, "  Vertical: display=%u start=%u end=%u total=%u\n", mode->vdisplay,
		   mode->vsync_start, mode->vsync_end, mode->vtotal);
	seq_printf(s, "  Interlaced: %s\n", mode->interlaced ? "true" : "false");
}

static void csi_dump_dphy(struct seq_file *s, struct phy_configure_opts_mipi_dphy *dphy)
{
	if (!dphy) {
		seq_puts(s, "D-PHY config: <null>\n");
		return;
	}

	seq_puts(s, "D-PHY config:\n");
	seq_printf(s, "  clk_miss: %u\n", dphy->clk_miss);
	seq_printf(s, "  clk_post: %u\n", dphy->clk_post);
	seq_printf(s, "  clk_pre: %u\n", dphy->clk_pre);
	seq_printf(s, "  clk_prepare: %u\n", dphy->clk_prepare);
	seq_printf(s, "  clk_settle: %u\n", dphy->clk_settle);
	seq_printf(s, "  clk_term_en: %u\n", dphy->clk_term_en);
	seq_printf(s, "  clk_trail: %u\n", dphy->clk_trail);
	seq_printf(s, "  clk_zero: %u\n", dphy->clk_zero);
	seq_printf(s, "  d_term_en: %u\n", dphy->d_term_en);
	seq_printf(s, "  eot: %u\n", dphy->eot);
	seq_printf(s, "  hs_exit: %u\n", dphy->hs_exit);
	seq_printf(s, "  hs_prepare: %u\n", dphy->hs_prepare);
	seq_printf(s, "  hs_settle: %u\n", dphy->hs_settle);
	seq_printf(s, "  hs_skip: %u\n", dphy->hs_skip);
	seq_printf(s, "  hs_trail: %u\n", dphy->hs_trail);
	seq_printf(s, "  hs_zero: %u\n", dphy->hs_zero);
	seq_printf(s, "  init: %u\n", dphy->init);
	seq_printf(s, "  lpx: %u\n", dphy->lpx);
	seq_printf(s, "  ta_get: %u\n", dphy->ta_get);
	seq_printf(s, "  ta_go: %u\n", dphy->ta_go);
	seq_printf(s, "  ta_sure: %u\n", dphy->ta_sure);
	seq_printf(s, "  wakeup: %u\n", dphy->wakeup);
	seq_printf(s, "  hs_clk_rate: %lu\n", dphy->hs_clk_rate);
	seq_printf(s, "  lp_clk_rate: %lu\n", dphy->lp_clk_rate);
	seq_printf(s, "  lanes: %u\n", dphy->lanes);
}

static int debugfs_info_show(struct seq_file *s, void *data)
{
	struct dw_mipi_csi *csi = s->private;
	u8 pattern;

	if (!csi)
		return -EINVAL;

	/* Dump basic info */
	seq_puts(s, "=== CSI Basic Info ===\n");
	seq_printf(s, "Lanes: %u\n", csi->lanes);
	seq_printf(s, "Bus format: %u\n", csi->bus_fmt);
	seq_printf(s, "Lane link rate: %u\n", csi->lane_link_rate);

	/* Dump mode and vpg_mode */
	seq_puts(s, "\n=== Mode ===\n");
	csi_dump_mode(s, "mode", &csi->mode);

	seq_puts(s, "\n=== VPG Mode ===\n");
	csi_dump_mode(s, "vpg_mode", &csi->vpg_mode);

	/* Dump IPI packet config */
	seq_puts(s, "\n=== IPI Packet Config ===\n");
	seq_printf(s, "IPI Mode: %u\n", csi->pkt_conf.ipi_mode);
	seq_printf(s, "VC: %u\n", csi->pkt_conf.vc);
	seq_printf(s, "Frame Num Mode: %u\n", csi->pkt_conf.frame_num_mode);
	seq_printf(s, "Frame Max: %u\n", csi->pkt_conf.frame_max);
	seq_printf(s, "Line Num Mode: %u\n", csi->pkt_conf.line_num_mode);
	seq_printf(s, "Line Start: %u\n", csi->pkt_conf.line_start);
	seq_printf(s, "Line Step: %u\n", csi->pkt_conf.line_step);
	seq_printf(s, "Send Start: %u\n", csi->pkt_conf.send_start);
	seq_printf(s, "HSYNC Packet Enabled: %s\n", csi->pkt_conf.hsync_pkt_en ? "true" : "false");

	/* Dump DPHY config */
	seq_puts(s, "\n=== D-PHY Config ===\n");
	csi_dump_dphy(s, &csi->dphy_cfg);

	/* Dump VPG pattern mode */
	seq_puts(s, "\n=== VPG Pattern ===\n");
	pattern = dw_mipi_csi_get_pattern(csi);
	seq_printf(s, "Pattern: %s\n",
		   (pattern == DW_MIPI_CSI_PATTERN_OFF) ? "OFF" :
		   (pattern == DW_MIPI_CSI_PATTERN_H)	? "H" :
		   (pattern == DW_MIPI_CSI_PATTERN_V)	? "V" :
							  "UNKNOWN");

	/* Dump override config */
	seq_puts(s, "\n=== Override Config ===\n");
	seq_printf(s, "IPI Mode Override: %u\n", csi->ipi_mode_override);
	seq_printf(s, "HSCLK Override: %u kHz\n", csi->hsclk_override_khz);
	seq_printf(s, "Clock Continuous: %u\n", csi->clk_continuous);
	seq_printf(s, "Clock Multiplier: %u\n", csi->clk_multiplier);
	seq_printf(s, "Timing Margin Enabled: %s\n", csi->timing_margin_enable ? "true" : "false");
	seq_printf(s, "DPHY Timing Override Enabled: %s\n",
		   csi->dphy_timing_override_enable ? "true" : "false");

	/* Dump timing override */
	seq_puts(s, "\n=== Timing Override ===\n");
	seq_printf(s, "LPX: %u\n", csi->override_lpx);
	seq_printf(s, "HS Prepare: %u\n", csi->override_hs_prepare);
	seq_printf(s, "HS Zero: %u\n", csi->override_hs_zero);
	seq_printf(s, "HS Trail: %u\n", csi->override_hs_trail);
	seq_printf(s, "HS Exit: %u\n", csi->override_hs_exit);
	seq_printf(s, "CLK Prepare: %u\n", csi->override_clk_prepare);
	seq_printf(s, "CLK Zero: %u\n", csi->override_clk_zero);
	seq_printf(s, "CLK Trail: %u\n", csi->override_clk_trail);
	seq_printf(s, "CLK Post: %u\n", csi->override_clk_post);

	return 0;
}

static int debugfs_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, debugfs_info_show, inode->i_private);
}

static const struct file_operations debugfs_info_fops = {
	.owner	 = THIS_MODULE,
	.open	 = debugfs_info_open,
	.read	 = seq_read,
	.llseek	 = seq_lseek,
	.release = single_release,
};

void dw_mipi_csi_debugfs_init(struct dw_mipi_csi *csi)
{
	char name[32];

	/* Create top-level directory under /sys/kernel/debug/ */
	if (!csi->debugfs_dir) {
		snprintf(name, sizeof(name), "csi-bridge-%s", dev_name(csi->dev));
		csi->debugfs_dir = debugfs_create_dir(name, NULL);
	}

	if (!csi->debugfs_dir)
		return;

	/* Create individual files */
	csi->pattern_file =
		debugfs_create_file("pattern", 0644, csi->debugfs_dir, csi, &debugfs_pattern_fops);
#ifdef CONFIG_HOBOT_CSI_IDI
	csi->bypass_file =
		debugfs_create_file("bypass", 0644, csi->debugfs_dir, csi, &debugfs_bypass_fops);
#endif
	csi->lanes_file =
		debugfs_create_file("lanes", 0644, csi->debugfs_dir, csi, &debugfs_lanes_fops);
	csi->bus_fmt_file =
		debugfs_create_file("bus_fmt", 0644, csi->debugfs_dir, csi, &debugfs_bus_fmt_fops);
	csi->info_file =
		debugfs_create_file("info", 0444, csi->debugfs_dir, csi, &debugfs_info_fops);

	CREATE_VPG_FILE(width);
	CREATE_VPG_FILE(height);
	CREATE_VPG_FILE(pixel_clock);
	CREATE_VPG_FILE(hdisplay);
	CREATE_VPG_FILE(hsync_start);
	CREATE_VPG_FILE(hsync_end);
	CREATE_VPG_FILE(htotal);
	CREATE_VPG_FILE(hskew);
	CREATE_VPG_FILE(vdisplay);
	CREATE_VPG_FILE(vsync_start);
	CREATE_VPG_FILE(vsync_end);
	CREATE_VPG_FILE(vtotal);
	CREATE_VPG_FILE(refresh_rate);
	debugfs_create_file("vpg_interlaced", 0644, csi->debugfs_dir, csi,
			    &debugfs_interlaced_fops);
	debugfs_create_file("irq_enable", 0200, csi->debugfs_dir, csi, &debugfs_irq_enable_fops);
	debugfs_create_file("hsclk_override_khz", 0644, csi->debugfs_dir, csi,
			    &debugfs_hsclk_override_fops);
	debugfs_create_file("ipi_mode", 0644, csi->debugfs_dir, csi, &debugfs_ipi_mode_fops);
	debugfs_create_file("clk_continuous", 0644, csi->debugfs_dir, csi,
			    &debugfs_clk_continuous_fops);

	debugfs_create_file("override_lpx", 0644, csi->debugfs_dir, csi,
			    &debugfs_override_lpx_fops);
	debugfs_create_file("override_hs_prepare", 0644, csi->debugfs_dir, csi,
			    &debugfs_override_hs_prepare_fops);
	debugfs_create_file("override_hs_zero", 0644, csi->debugfs_dir, csi,
			    &debugfs_override_hs_zero_fops);
	debugfs_create_file("override_hs_trail", 0644, csi->debugfs_dir, csi,
			    &debugfs_override_hs_trail_fops);
	debugfs_create_file("override_hs_exit", 0644, csi->debugfs_dir, csi,
			    &debugfs_override_hs_exit_fops);
	debugfs_create_file("override_clk_prepare", 0644, csi->debugfs_dir, csi,
			    &debugfs_override_clk_prepare_fops);
	debugfs_create_file("override_clk_zero", 0644, csi->debugfs_dir, csi,
			    &debugfs_override_clk_zero_fops);
	debugfs_create_file("override_clk_trail", 0644, csi->debugfs_dir, csi,
			    &debugfs_override_clk_trail_fops);
	debugfs_create_file("override_clk_post", 0644, csi->debugfs_dir, csi,
			    &debugfs_override_clk_post_fops);

	debugfs_create_file("dphy_timing_override_enable", 0644, csi->debugfs_dir, csi,
			    &debugfs_override_enable_fops);
	debugfs_create_file("timing_margin_enable", 0644, csi->debugfs_dir, csi,
			    &debugfs_timing_margin_enable_fops);
}

void dw_mipi_csi_debugfs_fini(struct dw_mipi_csi *csi)
{
	if (!csi->debugfs_dir)
		return;

	debugfs_remove_recursive(csi->debugfs_dir);
	csi->debugfs_dir  = NULL;
	csi->pattern_file = NULL;
#ifdef CONFIG_HOBOT_CSI_IDI
	csi->bypass_file = NULL;
#endif
	csi->lanes_file	  = NULL;
	csi->bus_fmt_file = NULL;
	csi->info_file	  = NULL;
}
