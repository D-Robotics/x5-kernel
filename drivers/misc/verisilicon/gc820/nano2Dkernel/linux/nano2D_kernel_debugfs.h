#ifndef _nano2d_kernel_debugfs_h_
#define _nano2d_kernel_debugfs_h_

#ifdef CONFIG_DEBUG_FS
#ifdef MODULE
#include <linux/module.h>
#endif
#include <linux/slab.h>
#include <linux/stddef.h>
#include <linux/errno.h>
#include <linux/vmalloc.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#include "nano2D_types.h"
#include "nano2D_enum.h"

/* Debug fs info */
typedef struct n2d_debugfs_dir {
	struct dentry *root;
	struct list_head node_list;
} n2d_debugfs_dir_t;

typedef struct n2d_debug_info {
	const char *name;
	int (*show)(struct seq_file *m, void *data);
	int (*write)(const char __user *buf, size_t count, void *data);
} n2d_debug_info_t;

typedef struct n2d_debug_node {
	n2d_debug_info_t *info;
	n2d_pointer device;
	struct dentry *entry;
	struct list_head head;
} n2d_debug_node_t;

n2d_error_t n2d_debugfs_dir_init(n2d_debugfs_dir_t *dir, struct dentry *root, const char *name);
n2d_error_t n2d_debugfs_dir_crestefiles(n2d_debugfs_dir_t *dir, n2d_debug_info_t *list, int count,
					n2d_pointer data);
n2d_error_t n2d_debugfs_dir_removefiles(n2d_debugfs_dir_t *dir, n2d_debug_info_t *list, int count);
void n2d_debugfs_dir_deinit(n2d_debugfs_dir_t *dir);

#endif
#endif
