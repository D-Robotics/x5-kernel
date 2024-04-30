#ifdef CONFIG_DEBUG_FS
#include "nano2D_kernel_debugfs.h"
#include "nano2D_kernel_os.h"

static int n2d_debugfs_open(struct inode *inode, struct file *file)
{
	n2d_debug_node_t *node = inode->i_private;

	return single_open(file, node->info->show, node);
}

static ssize_t n2d_debugfs_write(struct file *file, const char __user *buf, size_t count,
				 loff_t *pos)
{
	struct seq_file *s     = file->private_data;
	n2d_debug_node_t *node = s->private;
	n2d_debug_info_t *info = node->info;

	if (info->write)
		info->write(buf, count, node);

	return count;
}

static const struct file_operations n2d_debugfs_operations = {
	.owner	 = THIS_MODULE,
	.open	 = n2d_debugfs_open,
	.write	 = n2d_debugfs_write,
	.read	 = seq_read,
	.llseek	 = seq_lseek,
	.release = single_release,
};

n2d_error_t n2d_debugfs_dir_init(n2d_debugfs_dir_t *dir, struct dentry *root, const char *name)
{
	dir->root = debugfs_create_dir(name, root);

	if (!dir->root)
		return N2D_NOT_SUPPORTED;

	INIT_LIST_HEAD(&dir->node_list);

	return N2D_SUCCESS;
}

n2d_error_t n2d_debugfs_dir_crestefiles(n2d_debugfs_dir_t *dir, n2d_debug_info_t *list, int count,
					n2d_pointer data)
{
	int i;
	n2d_debug_node_t *node;
	n2d_error_t error = N2D_SUCCESS;

	for (i = 0; i < count; i++) {
		umode_t mode = 0;

		/* Create a node. */
		node = kzalloc(sizeof(n2d_debug_node_t), GFP_KERNEL);

		if (!node)
			ONERROR(N2D_OUT_OF_MEMORY);

		node->info   = &list[i];
		node->device = data;

		mode |= list[i].show ? 0444 : 0;
		mode |= list[i].write ? 0200 : 0;

		/*VIV: [todo] Bind to a file. clean up when fail. */
		node->entry = debugfs_create_file(list[i].name, mode, dir->root, node,
						  &n2d_debugfs_operations);

		if (!node->entry)
			ONERROR(N2D_OUT_OF_MEMORY);

		list_add(&node->head, &dir->node_list);
	}

on_error:
	if (N2D_IS_ERROR(error))
		n2d_debugfs_dir_removefiles(dir, list, count);

	return error;
}

n2d_error_t n2d_debugfs_dir_removefiles(n2d_debugfs_dir_t *dir, n2d_debug_info_t *list, int count)
{
	int i = 0;
	n2d_debug_node_t *node;
	n2d_debug_node_t *temp;

	list_for_each_entry_safe (node, temp, &dir->node_list, head) {
		for (i = 0; i < count; i++) {
			if (node->info == &list[i]) {
				debugfs_remove(node->entry);
				list_del(&node->head);
				kfree(node);
				break;
			}
		}
	}

	return N2D_SUCCESS;
}

void n2d_debugfs_dir_deinit(n2d_debugfs_dir_t *dir)
{
	debugfs_remove(dir->root);
	dir->root = NULL;
}
#endif
