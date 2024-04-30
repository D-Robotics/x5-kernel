// SPDX-License-Identifier: GPL-2.0+
/*
 * rewrite Horizon Sunrise 5 AXI Monitor Driver
 *
 * Copyright (C) 2023 Beijing Horizon Robotics Co.,Ltd.
 * Dinggao Pan <dinggao.pan@horizon.cc>
 *
 */
#include <linux/debugfs.h>
#include <linux/stat.h>
#include <linux/seq_buf.h>

#include "horizon_axi_monitor.h"

#define PERM_RW_USR S_IWUSR | S_IRUSR
#define PERM_RW_GRP S_IWGRP | S_IRGRP

#define PERM_RW_USR_GRP PERM_RW_USR | PERM_RW_GRP
#define PERM_RO_USR_GRP S_IRUSR | S_IRGRP

#define PORT_STAT_RESULT_LEN_MAX 512

#if IS_ENABLED(CONFIG_HOBOT_AXI_MON_ADVANCED)
static int rd_count_type_read(void *data, u64 *val)
{
	struct axi_monitor *axi_mon = data;

	return 0;
}

static int rd_count_type_write(void *data, u64 val)
{
	struct axi_monitor *axi_mon = data;

	return 0;
}
DEFINE_DEBUGFS_ATTRIBUTE(rd_cnt_type_fops, &rd_count_type_read, &rd_count_type_write, "%s\n");

static int rd_start_type_read(void *data, u64 *val)
{
	struct axi_monitor *axi_mon = data;

	return 0;
}

static int rd_start_type_write(void *data, u64 val)
{
	struct axi_monitor *axi_mon = data;

	return 0;
}
DEFINE_DEBUGFS_ATTRIBUTE(rd_start_type_fops, &rd_start_type_read, &rd_start_type_write, "%s\n");

static int rd_event_param_read(void *data, u64 *val)
{
	struct axi_monitor *axi_mon = data;

	return 0;
}

static int rd_event_param_write(void *data, u64 val)
{
	struct axi_monitor *axi_mon = data;

	return 0;
}
DEFINE_DEBUGFS_ATTRIBUTE(rd_event_param_fops, &rd_event_param_read, &rd_event_param_write, "%s\n");

static int rd_event_read(void *data, u64 *val)
{
	struct axi_monitor *axi_mon = data;

	return 0;
}

static int rd_event_write(void *data, u64 val)
{
	struct axi_monitor *axi_mon = data;

	return 0;
}
DEFINE_DEBUGFS_ATTRIBUTE(rd_event_fops, &rd_event_read, &rd_event_write, "%s\n");

static int hb_axi_monitor_create_node_dir(struct dentry *root_dir, struct axi_port *cur_port)
{
	struct dentry *port_dir;
	port_dir = debugfs_create_dir(cur_port->name, root_dir);
	if (port_dir == NULL) {
		dev_err(axi_mon->pdev->dev, "Create debufs folder for %s\n", axi_mon->pdev->name);
		return -ENODEV;
	}

	debugfs_create_file("rd_event", PERM_RW_USR_GRP, port_dir, NULL, &rd_event_fops);
	debugfs_create_file("rd_event_param", PERM_RW_USR_GRP, port_dir, NULL, &rd_event_param_fops);
	debugfs_create_file("rd_start_type", PERM_RW_USR_GRP, port_dir, NULL, &rd_start_type_fops);
	debugfs_create_file("rd_count_type", PERM_RW_USR_GRP, port_dir, NULL, &rd_cnt_type_fops);

	cur_port->debugfs_dentry = port_dir;

}

static int status_read(void *data, u64 *val)
{
	struct axi_monitor *axi_mon = data;
	char *status_str;
	status_str
	return 0;
}
DEFINE_DEBUGFS_ATTRIBUTE(status_fops, &status_read, NULL, "%s\n");

static int axi_tfr_timeout_read(void *data, u64 *val)
{
	struct axi_monitor *axi_mon = data;

	return 0;
}

static int axi_tfr_timeout_write(void *data, u64 *val)
{
	struct axi_monitor *axi_mon = data;

	return 0;
}
DEFINE_DEBUGFS_ATTRIBUTE(axi_tfr_timeout_fops, &axi_tfr_timeout_read, &axi_tfr_timeout_write, "%llu\n");
#endif

static int port_stat_show(struct seq_file *s, void *data)
{
	struct axi_monitor *axi_mon = s->private;
	struct list_head *port_entry;
	struct axi_port *port;
	/* TODO: Add proper locking */
	seq_printf(s, "========================================\n");
	seq_printf(s, "Actual monitor time:%llu millisecs\n", axi_mon->actual_time);
	seq_printf(s, "DDR Full bandwidth:%llu Bytes/sec\n", axi_mon->full_bw);
	seq_printf(s, "Port utilization statistics:\n");
	list_for_each(port_entry, &axi_mon->mon_ports_list) {
		port = list_entry(port_entry, struct axi_port, entry);
		seq_printf(s, "========================================\n");
		seq_printf(s, "Port[%s]:\n", port->name);
		seq_printf(s, "\tRead  Bandwidth             : %llu Bytes/sec\n",
				   port->rd_bw);
		seq_printf(s, "\tRead  Bandwidth Utilization : %d%%\n",
				   port->rd_bw_percent);
		seq_printf(s, "\tWrite Bandwidth             : %llu Bytes/sec\n",
				   port->wr_bw);
		seq_printf(s, "\tWrite Bandwidth Utilization : %d%%\n",
				   port->wr_bw_percent);
		seq_printf(s, "========================================\n");
	}
	return 0;
}
DEFINE_SHOW_ATTRIBUTE(port_stat);

static int measure_time_read(void *data, uint64_t *val)
{
	struct axi_monitor *axi_mon = data;
	*val = axi_mon->session_time;
	return 0;
}

static int measure_time_write(void *data, uint64_t val)
{
	struct axi_monitor *axi_mon = data;
	if (axi_mon->busy != BUSY) {
		if (val > axi_mon->session_time_max) {
			dev_warn(&axi_mon->pdev->dev, "%llu exceeds maximum time %llu, using %llu\n",
					 val, axi_mon->session_time_max, axi_mon->session_time_max);
			val = axi_mon->session_time_max;
		}
		axi_mon->session_time = val;
	} else {
		dev_info(&axi_mon->pdev->dev, "Busy, session time not updated\n");
	}
	return 0;
}
DEFINE_DEBUGFS_ATTRIBUTE(measure_time_fops, &measure_time_read, &measure_time_write, "%llu\n");

static int enable_read(void *data, uint64_t *val)
{
	struct axi_monitor *axi_mon = data;
	*val = axi_mon->busy;
	return 0;
}

static int enable_write(void *data, uint64_t val)
{
	struct axi_monitor *axi_mon = data;

	if ((val != 1) && (val != 0)) {
		dev_err(&axi_mon->pdev->dev, "Illegal value:%llu, only 1/0 allowed!\n", val);
		return -EINVAL;
	}

	if ((val & 0x1)^(axi_mon->busy & 0x1)) {
		axi_mon_start(axi_mon, (uint32_t)val);
	}

	return 0;
}
DEFINE_DEBUGFS_ATTRIBUTE(enable_fops, &enable_read, &enable_write, "%llu\n");

int hb_axi_monitor_create_debugfs(struct axi_monitor *axi_mon)
{
	struct dentry *hb_axi_mon_root;
#if IS_ENABLED(CONFIG_HOBOT_AXI_MON_ADVANCED)
	struct axi_port *cur_port;
	int i;
#endif
	hb_axi_mon_root = debugfs_create_dir("axi_mon", NULL);
	if (hb_axi_mon_root == NULL) {
		dev_err(&axi_mon->pdev->dev, "Create debugfs root for %s\n", axi_mon->pdev->name);
		return -ENODEV;
	}

	debugfs_create_file("enable", PERM_RW_USR_GRP, hb_axi_mon_root, axi_mon, &enable_fops);
	debugfs_create_file("measure_time", PERM_RW_USR_GRP, hb_axi_mon_root, axi_mon, &measure_time_fops);
	debugfs_create_file("statistics", PERM_RO_USR_GRP, hb_axi_mon_root, axi_mon, &port_stat_fops);

#if IS_ENABLED(CONFIG_HOBOT_AXI_MON_ADVANCED)
	debugfs_create_file("axi_tfr_timeout", PERM_RW_USR_GRP, hb_axi_mon_root, axi_mon, &axi_tfr_timeout_fops);
	debugfs_create_file("status", PERM_RW_USR_GRP, hb_axi_mon_root, axi_mon, &status_fops);
	for (i = 0; i < axi_mon->ports_num; i++)
	{
		cur_port = ports_list[i];
		if (hb_axi_monitor_create_node_dir(hb_axi_mon_root, cur_port))
		{
			dev_err(&axi_mon->pdev->dev, "Create debugfs dir for %s failed!\n", cur_port->name);
			return -ENODEV;
		}
	}
#endif
	axi_mon->debugfs_root = hb_axi_mon_root;
	return 0;
}
