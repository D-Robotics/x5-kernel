/*
 * drivers/staging/android/ion/ion_system_heap.c
 *
 * Copyright (C) 2011 Google, Inc.
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

#include <asm/page.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/ion.h>

#define NUM_ORDERS ARRAY_SIZE(orders)

//coverity[misra_c_2012_rule_8_9_violation:SUPPRESS], ## violation reason SYSSW_V_8.9_02
static gfp_t high_order_gfp_flags = (GFP_HIGHUSER | __GFP_ZERO | __GFP_NOWARN |
				     __GFP_NORETRY) & ~__GFP_RECLAIM;
static gfp_t low_order_gfp_flags  = GFP_HIGHUSER | __GFP_ZERO;
static const unsigned int orders[] = {8, 4, 0};

static int order_to_index(unsigned int order)
{
	int i;

	for (i = 0; (uint32_t)i < ARRAY_SIZE(orders); i++)
		if (order == orders[i])
			return i;
	BUG();
	return -1;
}

static inline unsigned int order_to_size(int order)
{
	return (uint32_t)(PAGE_SIZE << (uint32_t)order);
}

struct ion_system_heap {
	struct ion_heap heap;
	struct ion_page_pool *pools[NUM_ORDERS];
};

static struct page *alloc_buffer_page(struct ion_system_heap *heap,
				      struct ion_buffer *buffer,
				      unsigned long order)
{
	struct ion_page_pool *pool = heap->pools[order_to_index((uint32_t)order)];

	return ion_page_pool_alloc(pool);
}


static void free_buffer_page(struct ion_system_heap *heap,
			     struct ion_buffer *buffer, struct page *heap_page)
{
	struct ion_page_pool *pool;
	unsigned int order = compound_order(heap_page);

	/* go to system */
	if (buffer->private_flags & ION_PRIV_FLAG_SHRINKER_FREE) {
		__free_pages(heap_page, order);
		return;
	}

	pool = heap->pools[order_to_index(order)];

	ion_page_pool_free(pool, heap_page);
}


static struct page *alloc_largest_available(struct ion_system_heap *heap,
					    struct ion_buffer *buffer,
					    unsigned long size,
					    unsigned int max_order)
{
	struct page *heap_page;
	int i;

	for (i = 0; (uint32_t)i < ARRAY_SIZE(orders); i++) {
		if (size < order_to_size((int)orders[i]))
			continue;
		if (max_order < orders[i])
			continue;

		heap_page = alloc_buffer_page(heap, buffer, orders[i]);
		if (heap_page == NULL)
			continue;

		return heap_page;
	}

	return NULL;
}

//coverity[HIS_VOCF:SUPPRESS], ## violation reason SYSSW_V_VOCF_01
static int ion_system_heap_allocate(struct ion_heap *heap,
				     struct ion_buffer *buffer,
				     unsigned long size, unsigned long align,
				     unsigned long flags)

{
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
	struct ion_system_heap *sys_heap = container_of(heap,
							struct ion_system_heap,
							heap);
	struct sg_table *table;
	struct scatterlist *sg;
	struct list_head pages;
	struct page *heap_page, *tmp_page;
	int i = 0;
	unsigned long size_remaining = PAGE_ALIGN(size);
	unsigned long size_alloc;
	unsigned int max_order = orders[0];

	if (align > PAGE_SIZE)
		return -EINVAL;

	if (size / PAGE_SIZE > totalram_pages() / 2)
		return -ENOMEM;

	INIT_LIST_HEAD(&pages);
	while (size_remaining > 0U) {
		heap_page = alloc_largest_available(sys_heap, buffer, size_remaining,
						max_order);
		if (heap_page == NULL) {
			//coverity[misra_c_2012_rule_15_1_violation:SUPPRESS], ## violation reason SYSSW_V_15.1_01
			goto free_pages;
		}
		list_add_tail(&heap_page->lru, &pages);
		size_remaining -= page_size(heap_page);

		/* the page may used for instruction or data which still cached */
		size_alloc = page_size(heap_page);
		//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_11.6_02
		flush_icache_range((unsigned long)page_address(heap_page),
				//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03s
				//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_11.6_02
				(unsigned long)(page_address(heap_page) + size_alloc));
		//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_11.6_02
		dcache_inval_poc((unsigned long)page_address(heap_page), (unsigned long)page_address(heap_page) + size_alloc);
		//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_11.6_02
		dcache_clean_inval_poc((unsigned long)page_address(heap_page), (unsigned long)page_address(heap_page) + size_alloc);

		max_order = compound_order(heap_page);
		i++;
	}
	//coverity[misra_c_2012_directive_4_12_violation], ## violation reason SYSSW_V_4.12_02
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	table = kmalloc(sizeof(*table), GFP_KERNEL);
	if (table == NULL) {
		//coverity[misra_c_2012_rule_15_1_violation:SUPPRESS], ## violation reason SYSSW_V_15.1_01
		goto free_pages;
	}

	if (sg_alloc_table(table, i, GFP_KERNEL)) {
		//coverity[misra_c_2012_rule_15_1_violation:SUPPRESS], ## violation reason SYSSW_V_15.1_01
		goto free_table;
	}

	sg = table->sgl;
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
	list_for_each_entry_safe(heap_page, tmp_page, &pages, lru) {
		sg_set_page(sg, heap_page, (uint32_t)page_size(heap_page), 0);
		sg = sg_next(sg);
		list_del(&heap_page->lru);
	}

	buffer->priv_virt = table;
	buffer->sg_table = table;
	return 0;

free_table:
	kfree(table);
free_pages:
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
	list_for_each_entry_safe(heap_page, tmp_page, &pages, lru)
		free_buffer_page(sys_heap, buffer, heap_page);
	return -ENOMEM;
}

static void ion_system_heap_free(struct ion_buffer *buffer)
{
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
	struct ion_system_heap *sys_heap = container_of(buffer->heap,
							struct ion_system_heap,
							heap);
	struct sg_table *table = buffer->sg_table;
	struct scatterlist *sg;
	uint32_t i;

	/* zero the buffer before goto page pool */
	if ((buffer->private_flags & ION_PRIV_FLAG_SHRINKER_FREE) == 0UL)
		(void)ion_heap_buffer_zero(buffer);

	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
	for_each_sgtable_sg(table, sg, i)
		free_buffer_page(sys_heap, buffer, sg_page(sg));
	sg_free_table(table);
	kfree(table);
}

static struct sg_table *ion_system_heap_map_dma(struct ion_heap *heap,
						struct ion_buffer *buffer)
{
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	return buffer->priv_virt;
}

static void ion_system_heap_unmap_dma(struct ion_heap *heap,
				      struct ion_buffer *buffer)
{
}

static int ion_system_heap_shrink(struct ion_heap *heap, gfp_t gfp_mask,
				  int nr_to_scan)
{
	struct ion_page_pool *pool;
	struct ion_system_heap *sys_heap;
	int nr_total = 0;
	int i, nr_freed;
	int only_scan = 0;

	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
	sys_heap = container_of(heap, struct ion_system_heap, heap);

	if (nr_to_scan == 0)
		only_scan = 1;

	for (i = 0; (uint32_t)i < ARRAY_SIZE(orders); i++) {
		pool = sys_heap->pools[i];

		if (only_scan) {
			nr_total += ion_page_pool_shrink(pool,
							 gfp_mask,
							 nr_to_scan);

		} else {
			nr_freed = ion_page_pool_shrink(pool,
							gfp_mask,
							nr_to_scan);
			nr_to_scan -= nr_freed;
			nr_total += nr_freed;
			if (nr_to_scan <= 0)
				break;
		}
	}
	return nr_total;
}

//coverity[misra_c_2012_rule_8_9_violation:SUPPRESS], ## violation reason SYSSW_V_8.9_02
static struct ion_heap_ops system_heap_ops = {
	.allocate = ion_system_heap_allocate,
	.free = ion_system_heap_free,
	.map_dma = ion_system_heap_map_dma,
	.unmap_dma = ion_system_heap_unmap_dma,
	.map_kernel = ion_heap_map_kernel,
	.unmap_kernel = ion_heap_unmap_kernel,
	.map_user = ion_heap_map_user,
	.shrink = ion_system_heap_shrink,
};

static int ion_system_heap_debug_show(struct ion_heap *heap, struct seq_file *s,
				      void *unused)
{
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
	struct ion_system_heap *sys_heap = container_of(heap,
							struct ion_system_heap,
							heap);
	int i;

	for (i = 0; (uint32_t)i < ARRAY_SIZE(orders); i++) {
		struct ion_page_pool *pool = sys_heap->pools[i];

		seq_printf(s, "%d order %u highmem pages in pool = %lu total\n",
			   pool->high_count, pool->order,
			   (PAGE_SIZE << pool->order) * pool->high_count);
		seq_printf(s, "%d order %u lowmem pages in pool = %lu total\n",
			   pool->low_count, pool->order,
			   (PAGE_SIZE << pool->order) * pool->low_count);
	}
	return 0;
}

struct ion_heap *ion_system_heap_create(struct ion_platform_heap *unused)
{
	struct ion_system_heap *heap;
	int i;

	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	heap = kzalloc(sizeof(struct ion_system_heap) +
			sizeof(struct ion_page_pool *) * (size_t)ARRAY_SIZE(orders),
			GFP_KERNEL);
	if (heap == NULL) {
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		return ERR_PTR(-ENOMEM);
	}
	heap->heap.ops = &system_heap_ops;
	heap->heap.type = ION_HEAP_TYPE_SYSTEM;
	heap->heap.flags = ION_HEAP_FLAG_DEFER_FREE;

	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	for (i = 0; (uint32_t)i < NUM_ORDERS; i++) {
		struct ion_page_pool *pool;
		gfp_t gfp_flags = low_order_gfp_flags;

		if (orders[i] > 4U)
			gfp_flags = high_order_gfp_flags;
		pool = ion_page_pool_create(gfp_flags, orders[i], (bool)0);
		if (pool == NULL) {
			//coverity[misra_c_2012_rule_15_1_violation:SUPPRESS], ## violation reason SYSSW_V_15.1_01
			goto destroy_pools;
		}
		heap->pools[i] = pool;
	}

	heap->heap.debug_show = ion_system_heap_debug_show;
	return &heap->heap;

destroy_pools:
	while (i--)
		ion_page_pool_destroy(heap->pools[i]);
	kfree(heap);
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	return ERR_PTR(-ENOMEM);
}

void ion_system_heap_destroy(struct ion_heap *heap)
{
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
	struct ion_system_heap *sys_heap = container_of(heap,
							struct ion_system_heap,
							heap);
	int i;

	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	for (i = 0; (uint32_t)i < NUM_ORDERS; i++)
		ion_page_pool_destroy(sys_heap->pools[i]);
	kfree(sys_heap);
}

static int ion_system_contig_heap_allocate(struct ion_heap *heap,
					   struct ion_buffer *buffer,
					   unsigned long len,
					   unsigned long align,
					   unsigned long flags)
{
	int order = get_order(len);
	struct page *heap_page;
	struct sg_table *table;
	unsigned long i;
	int ret;

	if (align > (PAGE_SIZE << (uint32_t)order))
		return -EINVAL;

	heap_page = alloc_pages(low_order_gfp_flags | __GFP_NOWARN, order);
	if (heap_page == NULL)
		return -ENOMEM;

	split_page(heap_page, (uint32_t)order);

	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	len = PAGE_ALIGN(len);
	for (i = len >> PAGE_SHIFT; i < ((uint64_t)1U << (uint64_t)order); i++) {
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
		__free_page(heap_page + i);
	}

	//coverity[misra_c_2012_directive_4_12_violation], ## violation reason SYSSW_V_4.12_02
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	table = kmalloc(sizeof(*table), GFP_KERNEL);
	if (table == NULL) {
		ret = -ENOMEM;
		//coverity[misra_c_2012_rule_15_1_violation:SUPPRESS], ## violation reason SYSSW_V_15.1_01
		goto free_pages;
	}

	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret) {
		//coverity[misra_c_2012_rule_15_1_violation:SUPPRESS], ## violation reason SYSSW_V_15.1_01
		goto free_table;
	}

	sg_set_page(table->sgl, heap_page, (uint32_t)len, 0);

	buffer->priv_virt = table;
	buffer->sg_table = table;

	/* the page may used for instruction or data which still cached */
	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_11.6_02
	flush_icache_range((unsigned long)page_address(heap_page),
			//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03s
			//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_11.6_02
			(unsigned long)(page_address(heap_page) + len));
	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_11.6_02
	dcache_inval_poc((unsigned long)page_address(heap_page), (unsigned long)page_address(heap_page) + len);
	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_11.6_02
	dcache_clean_inval_poc((unsigned long)page_address(heap_page), (unsigned long)page_address(heap_page) + len);

	return 0;

free_table:
	kfree(table);
free_pages:
	for (i = 0; i < len >> PAGE_SHIFT; i++) {
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
		__free_page(heap_page + i);
	}

	return ret;
}

static void ion_system_contig_heap_free(struct ion_buffer *buffer)
{
	struct sg_table *table = buffer->sg_table;
	struct page *heap_page = sg_page(table->sgl);
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	unsigned long pages = PAGE_ALIGN(buffer->size) >> PAGE_SHIFT;
	unsigned long i;

	for (i = 0; i < pages; i++) {
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
		__free_page(heap_page + i);
	}
	sg_free_table(table);
	kfree(table);
}

static int ion_system_contig_heap_phys(struct ion_heap *heap,
				       struct ion_buffer *buffer,
				       phys_addr_t *addr, size_t *len)
{
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	struct sg_table *table = buffer->priv_virt;
	struct page *heap_page = sg_page(table->sgl);
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	*addr = page_to_phys(heap_page);
	*len = buffer->size;
	return 0;
}

static struct sg_table *ion_system_contig_heap_map_dma(struct ion_heap *heap,
						struct ion_buffer *buffer)
{
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	return buffer->priv_virt;
}

static void ion_system_contig_heap_unmap_dma(struct ion_heap *heap,
					     struct ion_buffer *buffer)
{
}

//coverity[misra_c_2012_rule_8_9_violation:SUPPRESS], ## violation reason SYSSW_V_8.9_02
static struct ion_heap_ops kmalloc_ops = {
	.allocate = ion_system_contig_heap_allocate,
	.free = ion_system_contig_heap_free,
	.phys = ion_system_contig_heap_phys,
	.map_dma = ion_system_contig_heap_map_dma,
	.unmap_dma = ion_system_contig_heap_unmap_dma,
	.map_kernel = ion_heap_map_kernel,
	.unmap_kernel = ion_heap_unmap_kernel,
	.map_user = ion_heap_map_user,
};

struct ion_heap *ion_system_contig_heap_create(struct ion_platform_heap *unused)
{
	struct ion_heap *heap;

	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	heap = kzalloc(sizeof(*heap), GFP_KERNEL);
	if (heap == NULL) {
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		return ERR_PTR(-ENOMEM);
	}
	heap->ops = &kmalloc_ops;
	heap->type = ION_HEAP_TYPE_SYSTEM_CONTIG;
	heap->name = "ion_system_contig_heap";
	return heap;
}

void ion_system_contig_heap_destroy(struct ion_heap *heap)
{
	kfree(heap);
}

 /* device_initcall(ion_system_contig_heap_create); */
