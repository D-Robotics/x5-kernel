/***​
 *                     COPYRIGHT NOTICE​
 *            Copyright (C) 2022 -2023, Horizon Robotics Co., Ltd.​
 *                   All rights reserved.​
 ***/

#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/highmem.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/of.h>
#include <asm/page.h>
#include <linux/vmalloc.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/memblock.h>

#define BOOTLOADER_LOG_PROC "bootloader_log"
struct log_mem_buf {
	char *start; 	/* the start of the buffer */
	char *end; 		/* the end of the buffer (start + length) */
	uint64_t paddr; /* from dts */
	uint64_t size; 	/* from dts */
	char *vaddr; 	/* vmap indicates the virtual address of the mapping */
};

static int log_show(struct seq_file *m, void *v)
{
	struct log_mem_buf *lmb = m->private;
	char buf[64] = { 0 };
	int buf_len = sizeof(buf) - 4;
	int offset = 0;

	for (offset = 0; offset < lmb->size; offset += buf_len) {
		if (offset + buf_len > lmb->size) {
			buf_len = lmb->size - offset;
		}
		memcpy(buf, lmb->start + offset, buf_len);
		buf[buf_len] = '\0';
		seq_printf(m, "%s", buf);
	}
	return 0;
}

static int log_open(struct inode *inode, struct file *file)
{
	return single_open(file, log_show, pde_data(inode));
}

static int log_release(struct inode *inode, struct file *file)
{
	return single_release(inode, file);
}

static const struct proc_ops log_proc_ops = {
	.proc_open = log_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = log_release,
};

/**
 * @NO{S21E06C01}
 * @ASIL{QM}
 * @brief obtain the location of the bootloader log using dts 
 *        and map it to the virtual virtual address
 *
 * @param[in] pdev: platform device
 *
 * @retval =0: success
 * @retval <0: probe failed
 */
static int bootloader_log_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	struct log_mem_buf *lmb = NULL;
	struct page **pages;
	phys_addr_t page_start;
	int page_count = 0;
	pgprot_t prot;
	int i = 0;

	if (!np) {
		dev_err(dev, "of_node is NULL");
		return -ENODEV;
	}

	lmb = devm_kzalloc(dev, sizeof(struct log_mem_buf), GFP_KERNEL);
	if (!lmb) {
		dev_err(dev, "alloc private structure failed");
		return -ENOMEM;
	}
	dev_set_drvdata(dev, lmb);

	/* check the validity of reserved memory */
	if (of_property_read_u64(np, "membuff-size", &lmb->size)) {
		dev_err(dev, "get membuff-size failed");
		return -ENOENT;
	}
	if (lmb->size > SZ_64K) {
		dev_err(dev, "membuff-size is too large 0x%llx", lmb->size);
		return -EINVAL;
	}
	if (of_property_read_u64(np, "membuff-start", &lmb->paddr)) {
		dev_err(dev, "get membuff-start failed");
		return -ENOENT;
	}
	if (!pfn_valid(lmb->paddr >> PAGE_SHIFT)) {
		dev_err(dev, "invalid pfn 0x%llx", lmb->paddr);
		return -EINVAL;
	}
	dev_info(dev, "log stored in 0x%llx, size 0x%llx", lmb->paddr, lmb->size);

	/* a physical address is mapped to a virtual address */
	page_start = lmb->paddr - offset_in_page(lmb->paddr);
	page_count = DIV_ROUND_UP(lmb->size + offset_in_page(lmb->paddr), PAGE_SIZE);
	pages = kmalloc_array(page_count, sizeof(*pages[0]), GFP_KERNEL);
	if (!pages) {
		dev_err(dev, "failed to allocate array for %u pages", page_count);
		return -ENOMEM;
	}
	for (i = 0; i < page_count; i++) {
		phys_addr_t addr = page_start + i * PAGE_SIZE;
		pages[i] = pfn_to_page(addr >> PAGE_SHIFT);
	}

	prot = pgprot_writecombine(PAGE_KERNEL);
	lmb->vaddr = vmap(pages, page_count, VM_MAP, prot);
	kfree(pages);
	if (!lmb->vaddr) {
		dev_err(dev, "failed to vmap 0x%llx, count 0x%x", page_start, page_count);
		return -ENOMEM;
	}
	lmb->start = lmb->vaddr + offset_in_page(lmb->paddr);

	if (!proc_create_data(BOOTLOADER_LOG_PROC, 0, NULL, &log_proc_ops, lmb)) {
		dev_err(dev, "failed to creat proc %s", BOOTLOADER_LOG_PROC);
		vunmap(lmb->vaddr);
		lmb->vaddr = NULL;
		lmb->start = NULL;
		return -ENOMEM;
	}

	return 0;
}

/**
 * @NO{S21E06C01}
 * @ASIL{QM}
 * @brief release resources and exit
 *
 * @param[in] pdev: platform device
 *
 * @retval =0: success
 * @retval <0: probe failed
 */
static int bootloader_log_remove(struct platform_device *pdev)
{
	struct log_mem_buf *lmb = dev_get_drvdata(&pdev->dev);

	remove_proc_entry(BOOTLOADER_LOG_PROC, NULL);
	if (lmb && lmb->vaddr) {
		lmb->start = NULL;
		vunmap(lmb->vaddr);
		lmb->vaddr = NULL;
	}
	return 0;
}

static const struct of_device_id bootloader_log_of_match[] = {
	{ .compatible = "hobot,j5-uboot-log" },
	{ .compatible = "hobot,j6-uboot-log" },
	{ .compatible = "hobot,uboot-log" },
	{},
};
MODULE_DEVICE_TABLE(of, bootloader_log_of_match);

static struct platform_driver bootloader_log_driver = {
	.driver = {
		.name = "bootloader-log",
		.of_match_table = bootloader_log_of_match,
	},
	.probe = bootloader_log_probe,
	.remove = bootloader_log_remove,
};

module_platform_driver(bootloader_log_driver);
MODULE_DESCRIPTION("bootloader log");
MODULE_LICENSE("GPL v2");
