/*
 * drivers/staging/android/ion/ion_cma_heap.c
 *
 * Copyright (C) Linaro 2012
 * Author: <benjamin.gaignard@linaro.org> for ST-Ericsson.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/device.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/cma.h>
#include <linux/scatterlist.h>
#include <linux/highmem.h>

#include <linux/ion.h>

#define ION_CMA_NAME "ion_cma"
#define ION_CMA_EXTRA_NAME "ion_cma_extra"
static struct ion_device *cma_ion_dev = NULL;

struct ion_cma_heap {
	struct ion_heap heap;
	struct cma *sys_cma;
};

#define to_cma_heap(x) container_of(x, struct ion_cma_heap, heap)

/* ION CMA heap operations functions */
static int ion_cma_allocate(struct ion_heap *heap, struct ion_buffer *buffer,
		unsigned long len, unsigned long start_align,
		unsigned long flags)
{
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
	struct ion_cma_heap *cma_heap = to_cma_heap(heap);
	struct sg_table *table;
	struct page *pages;
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	unsigned long size = PAGE_ALIGN(len);
	unsigned long nr_pages = size >> PAGE_SHIFT;
#ifndef CONFIG_HOBOT_XJ3
	unsigned long align = (unsigned long)get_order(size);
#else
	unsigned long align = 0;	//order 0 i.e. 1 page align
#endif
	int ret;

#ifndef CONFIG_HOBOT_XJ3
	if (align > (uint64_t)CONFIG_CMA_ALIGNMENT)
		align = CONFIG_CMA_ALIGNMENT;
#endif

	pages = cma_alloc(cma_heap->sys_cma, nr_pages, align, GFP_KERNEL);
	if (pages == NULL)
		return -ENOMEM;

	/* the page may used for instruction or data which still cached */
	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_11.6_02
	flush_icache_range((unsigned long)page_address(pages),
			//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
			//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_11.6_02
			(unsigned long)(page_address(pages) + size));
	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_11.6_02
	dcache_inval_poc((unsigned long)page_address(pages), (unsigned long)page_address(pages) + size);
	if ((buffer->flags & (uint64_t)ION_FLAG_UNINITIALIZED) == 0UL) {
		if (PageHighMem(pages)) {
			unsigned long nr_clear_pages = nr_pages;
			struct page *heap_page = pages;

			while (nr_clear_pages > 0UL) {
				void *vaddr = kmap_atomic(heap_page);

				(void)memset(vaddr, 0, PAGE_SIZE);
				kunmap_atomic(vaddr);
				heap_page++;
				nr_clear_pages--;
			}
		} else {
			(void)memset(page_address(pages), 0, size);
		}
	}
	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_11.6_02
	dcache_clean_inval_poc((unsigned long)page_address(pages), (unsigned long)page_address(pages) + size);

	//coverity[misra_c_2012_directive_4_12_violation], ## violation reason SYSSW_V_4.12_02
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	table = kmalloc(sizeof(*table), GFP_KERNEL);
	if (table == NULL) {
		//coverity[misra_c_2012_rule_15_1_violation:SUPPRESS], ## violation reason SYSSW_V_15.1_01
		goto err;
	}

	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret) {
		//coverity[misra_c_2012_rule_15_1_violation:SUPPRESS], ## violation reason SYSSW_V_15.1_01
		goto free_mem;
	}

	sg_set_page(table->sgl, pages, (uint32_t)size, 0);

	buffer->priv_virt = pages;
	buffer->sg_table = table;

	return 0;

free_mem:
	kfree(table);
err:
	(void)cma_release(cma_heap->sys_cma, pages, nr_pages);
	return -ENOMEM;
}

static void ion_cma_free(struct ion_buffer *buffer)
{
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
	struct ion_cma_heap *cma_heap = to_cma_heap(buffer->heap);
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	struct page *pages = buffer->priv_virt;
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	unsigned long nr_pages = PAGE_ALIGN(buffer->size) >> PAGE_SHIFT;

	/* release memory */
	(void)cma_release(cma_heap->sys_cma, pages, nr_pages);
	/* release sg table */
	sg_free_table(buffer->sg_table);
	kfree(buffer->sg_table);
}

static int ion_cma_heap_phys(struct ion_heap *heap,
				  struct ion_buffer *buffer,
				  phys_addr_t *addr, size_t *len)
{
	struct sg_table *table = buffer->sg_table;
	struct page *heap_page = sg_page(table->sgl);
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	phys_addr_t paddr = PFN_PHYS(page_to_pfn(heap_page));

	*addr = paddr;
	*len = buffer->size;
	return 0;
}

static struct sg_table *ion_cma_heap_map_dma(struct ion_heap *heap,
					     struct ion_buffer *buffer)
{
	return buffer->sg_table;
}

static void ion_cma_heap_unmap_dma(struct ion_heap *heap,
				   struct ion_buffer *buffer)
{
}

//coverity[misra_c_2012_rule_8_9_violation:SUPPRESS], ## violation reason SYSSW_V_8.9_02
static struct ion_heap_ops ion_cma_ops = {
	.allocate = ion_cma_allocate,
	.free = ion_cma_free,
	.phys = ion_cma_heap_phys,
	.map_dma = ion_cma_heap_map_dma,
	.unmap_dma = ion_cma_heap_unmap_dma,
	.map_user = ion_heap_map_user,
	.map_kernel = ion_heap_map_kernel,
	.unmap_kernel = ion_heap_unmap_kernel,
};

static struct ion_heap *__ion_cma_heap_create(struct cma *sys_cma)
{
	struct ion_cma_heap *cma_heap;

	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	cma_heap = kzalloc(sizeof(*cma_heap), GFP_KERNEL);

	if (cma_heap == NULL) {
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		return ERR_PTR(-ENOMEM);
	}

	cma_heap->heap.ops = &ion_cma_ops;
	/*
	 * get device from private heaps data, later it will be
	 * used to make the link with reserved CMA memory
	 */
	cma_heap->sys_cma = sys_cma;
	if (strcmp(cma_get_name(sys_cma), ION_CMA_NAME) == 0) {
		cma_heap->heap.type = ION_HEAP_TYPE_DMA;
	} else if (strcmp(cma_get_name(sys_cma), ION_CMA_EXTRA_NAME) == 0) {
		cma_heap->heap.type = ION_HEAP_TYPE_DMA_EX;
	} else {
		//do nothing
		;
	}

	return &cma_heap->heap;
}

int ion_cma_get_info(struct ion_device *dev, phys_addr_t *base, size_t *size,
		enum ion_heap_type type)
{
	int32_t found = 0;
	struct ion_heap *heap;
	struct ion_cma_heap *cma_heap;

	if ((type != ION_HEAP_TYPE_DMA) && (type != ION_HEAP_TYPE_DMA_EX)) {
		return -EINVAL;
	}

	down_read(&dev->lock);
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
	plist_for_each_entry(heap, &dev->heaps, node) {
		if (heap->type != type)
			continue;

		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
		cma_heap = to_cma_heap(heap);
		*base = cma_get_base(cma_heap->sys_cma);
		*size = cma_get_size(cma_heap->sys_cma);
		heap->total_size = *size;
		found = 1;
		break;
	}
	up_read(&dev->lock);
	if (found == 0) {
		return -EINVAL;
	}

	return 0;
}
//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
EXPORT_SYMBOL(ion_cma_get_info);

static int __ion_add_cma_heaps(struct cma *sys_cma, void *data)
{
	const char *name;
	struct ion_heap *heap;

	if (cma_ion_dev == NULL)
		return -ENODEV;

	name = cma_get_name(sys_cma);
	if ((strcmp(name, ION_CMA_NAME) != 0) && (strcmp(name, ION_CMA_EXTRA_NAME) != 0))
		return 0;

	heap = __ion_cma_heap_create(sys_cma);
	if (IS_ERR(heap))
		return PTR_ERR(heap);

	heap->name = cma_get_name(sys_cma);

	ion_device_add_heap(cma_ion_dev, heap);

	return 0;
}

static int __ion_del_cma_heaps(struct cma *sys_cma, void *data)
{
	const char *name;
	struct ion_heap *heap;
	struct ion_cma_heap *cma_heap;

	if (cma_ion_dev == NULL)
		return -ENODEV;

	name = cma_get_name(sys_cma);
	if ((strcmp(name, "ion_cma") != 0) && (strcmp(name, "ion_cma_extra") != 0))
		return 0;

	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
	plist_for_each_entry(heap, &cma_ion_dev->heaps, node) {
		if (((heap->type == ION_HEAP_TYPE_DMA) || (heap->type == ION_HEAP_TYPE_DMA_EX)) &&
			(strcmp(name, heap->name) != 0)) {
			plist_del((struct plist_node *)heap, &cma_ion_dev->heaps);
			break;
		}
	}

	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
	cma_heap = to_cma_heap(heap);
	kfree(cma_heap);

	return 0;
}

int ion_add_cma_heaps(struct ion_device *idev)
{
	if (cma_ion_dev == NULL)
		cma_ion_dev = idev;
	else
		return 0;

	(void)cma_for_each_area(__ion_add_cma_heaps, NULL);

	return 0;
}
//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
EXPORT_SYMBOL(ion_add_cma_heaps);

int ion_del_cma_heaps(struct ion_device *idev)
{
	(void)cma_for_each_area(__ion_del_cma_heaps, NULL);
	cma_ion_dev = NULL;

	return 0;
}
//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
EXPORT_SYMBOL(ion_del_cma_heaps);