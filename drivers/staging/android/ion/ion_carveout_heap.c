/*
 * drivers/staging/android/ion/ion_carveout_heap.c
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
#include <linux/spinlock.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/genalloc.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <asm/cacheflush.h>
#include <linux/ion.h>
#include <linux/rculist.h>


#define ION_CARVEOUT_ALLOCATE_FAIL	-1

struct ion_carveout_heap {
	struct ion_heap heap;
	struct gen_pool *pool;
	phys_addr_t base;
};

//get the avail size and max_contigous
//coverity[HIS_VOCF:SUPPRESS], ## violation reason SYSSW_V_VOCF_01
//coverity[HIS_LEVEL:SUPPRESS], ## violation reason SYSSW_V_LEVEL_01
int32_t get_carveout_info(struct ion_heap *heap, uint64_t *avail, uint64_t *max_contigous)
{
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
	struct ion_carveout_heap *carveout_heap = container_of(heap, struct ion_carveout_heap, heap);
	struct gen_pool_chunk *chunk;
	struct gen_pool *pool = carveout_heap->pool;
	uint64_t avail_mem = 0, max_range = 0;
	uint64_t start_bit, end_bit, index, i, max_range_bit = 0;
	int32_t order;
	order = pool->min_alloc_order;

	rcu_read_lock();
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
	list_for_each_entry_rcu(chunk, &pool->chunks, next_chunk) {
		avail_mem += (uint64_t)atomic_long_read(&chunk->avail);
		if (max_range > (uint64_t)atomic_long_read(&chunk->avail))
			continue;

		start_bit = 0;
		end_bit = (chunk->end_addr - chunk->start_addr + 1U) >> (uint32_t)order;

		index = find_next_zero_bit(chunk->bits, end_bit, start_bit);
		while(index < end_bit) {
			//find next set bit
			i = find_next_bit(chunk->bits, end_bit, index);
			//compare and get the max range
			max_range_bit = max_range_bit > (i - index) ? max_range_bit : (i - index);
			max_range = (uint64_t)max_range_bit << (uint64_t)order;
			//get the next range
			index = find_next_zero_bit(chunk->bits, end_bit, i);
		}
	}
	rcu_read_unlock();
	*avail = avail_mem;
	*max_contigous = max_range;
	return 0;
}

//coverity[misra_c_2012_rule_8_7_violation:SUPPRESS], ## violation reason SYSSW_V_8.7_01
phys_addr_t ion_carveout_allocate(struct ion_heap *heap,
				      unsigned long size,
				      unsigned long align)
{
	struct ion_carveout_heap *carveout_heap =
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
		container_of(heap, struct ion_carveout_heap, heap);
	unsigned long offset = gen_pool_alloc(carveout_heap->pool, size);

	if (offset == 0UL) {
		pr_debug("%s: alloc buffer from pool failed\n", __func__);
		return (phys_addr_t)ION_CARVEOUT_ALLOCATE_FAIL;
	}

	return offset;
}

//coverity[misra_c_2012_rule_8_7_violation:SUPPRESS], ## violation reason SYSSW_V_8.7_01
void ion_carveout_free(struct ion_heap *heap, phys_addr_t addr,
		       unsigned long size)
{
	struct ion_carveout_heap *carveout_heap =
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
		container_of(heap, struct ion_carveout_heap, heap);

	if (addr == (phys_addr_t)ION_CARVEOUT_ALLOCATE_FAIL)
		return;
	gen_pool_free(carveout_heap->pool, addr, size);
}

static int ion_carveout_heap_phys(struct ion_heap *heap,
				  struct ion_buffer *buffer,
				  phys_addr_t *addr, size_t *len)
{
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	struct sg_table *table = buffer->priv_virt;
	struct page *heap_page = sg_page(table->sgl);
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	phys_addr_t paddr = PFN_PHYS(page_to_pfn(heap_page));

	*addr = paddr;
	*len = buffer->size;
	return 0;
}

//coverity[HIS_CCM:SUPPRESS], ## violation reason SYSSW_V_CCM_01
static int ion_carveout_heap_allocate(struct ion_heap *heap,
				      struct ion_buffer *buffer,
				      unsigned long size, unsigned long align,
				      unsigned long flags)
{
	struct sg_table *table;
	struct scatterlist *sg, *tmp_sg;
	unsigned long size_remaining = size;
	phys_addr_t paddr;
	int ret;
	uint32_t i = 1U;

	if (align > PAGE_SIZE) {
		(void)pr_err("%s: align should <= page size\n", __func__);
		return -EINVAL;
	}

	while (size_remaining >= SZ_4G) {
		size_remaining -= (SZ_4G - PAGE_SIZE);
		i++;
	}

	//coverity[misra_c_2012_directive_4_12_violation], ## violation reason SYSSW_V_4.12_02
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	table = kmalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (table == NULL)
		return -ENOMEM;

	ret = sg_alloc_table(table, i, GFP_KERNEL);
	if (ret) {
		(void)pr_err("%s: sg alloc table failed\n", __func__);
		//coverity[misra_c_2012_rule_15_1_violation:SUPPRESS], ## violation reason SYSSW_V_15.1_01
		goto err_free;
	}

	paddr = ion_carveout_allocate(heap, size, align);
	if (paddr == (phys_addr_t)ION_CARVEOUT_ALLOCATE_FAIL) {
		ret = -ENOMEM;
		//coverity[misra_c_2012_rule_15_1_violation:SUPPRESS], ## violation reason SYSSW_V_15.1_01
		goto err_free_table;
	}

	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	sg = table->sgl;
	size_remaining = size;
	while (size_remaining >= SZ_4G) {
		tmp_sg = sg;
		sg_set_page(tmp_sg, pfn_to_page(PFN_DOWN(paddr)), SZ_4G - PAGE_SIZE, 0U);
		paddr += (SZ_4G - PAGE_SIZE);
		size_remaining -= (SZ_4G - PAGE_SIZE);
		sg = sg_next(tmp_sg);
	}
	sg_set_page(sg, pfn_to_page(PFN_DOWN(paddr)), size_remaining, 0U);
	buffer->priv_virt = table;
	buffer->hb_sg_table = table;

	if ((buffer->flags & (uint64_t)ION_FLAG_INITIALIZED) != 0U) {
		(void)ion_heap_buffer_zero_ex(table, buffer->flags);
	}

	return 0;

err_free_table:
	sg_free_table(table);
err_free:
	kfree(table);
	return ret;
}

static void ion_carveout_heap_free(struct ion_buffer *buffer)
{
	struct ion_heap *heap = buffer->heap;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	struct sg_table *table = buffer->priv_virt;
	struct page *heap_page = sg_page(table->sgl);
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	phys_addr_t paddr = PFN_PHYS(page_to_pfn(heap_page));

	//ion_heap_buffer_zero(buffer);

	if (ion_buffer_cached(buffer)) {
		//dma_sync_sg_for_device(NULL, table->sgl, table->nents,
		//					DMA_BIDIRECTIONAL);
		//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_11.6_02
		dcache_clean_inval_poc((unsigned long)phys_to_virt(paddr), (unsigned long)phys_to_virt(paddr) + buffer->size);
	}

	ion_carveout_free(heap, paddr, buffer->size);
	sg_free_table(table);
	kfree(table);
}

static struct sg_table *ion_carveout_heap_map_dma(struct ion_heap *heap,
						  struct ion_buffer *buffer)
{
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	return buffer->priv_virt;
}

static void ion_carveout_heap_unmap_dma(struct ion_heap *heap,
					struct ion_buffer *buffer)
{
}

static struct ion_heap_ops carveout_heap_ops = {
	.allocate = ion_carveout_heap_allocate,
	.free = ion_carveout_heap_free,
	.phys = ion_carveout_heap_phys,
	.map_dma = ion_carveout_heap_map_dma,
	.unmap_dma = ion_carveout_heap_unmap_dma,
	.map_user = ion_heap_map_user,
	.map_kernel = ion_heap_map_kernel,
	.unmap_kernel = ion_heap_unmap_kernel,
};

struct ion_heap *ion_carveout_heap_create(struct ion_platform_heap *heap_data)
{
	struct ion_carveout_heap *carveout_heap;
	int ret;

	struct page *heap_page;
	size_t size;

	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	heap_page = pfn_to_page(PFN_DOWN(heap_data->base));
	size = heap_data->size;

	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	ret = ion_heap_pages_zero(heap_page, size, pgprot_writecombine(PAGE_KERNEL));
	//ret = ion_heap_pages_zero(page, size, pgprot_noncached(PAGE_KERNEL));
	if (ret) {
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		return ERR_PTR(ret);
	}

	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	carveout_heap = kzalloc(sizeof(*carveout_heap), GFP_KERNEL);
	if (carveout_heap == NULL) {
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		return ERR_PTR(-ENOMEM);
	}

	carveout_heap->pool = gen_pool_create(PAGE_SHIFT, -1);
	if (carveout_heap->pool == NULL) {
		kfree(carveout_heap);
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		return ERR_PTR(-ENOMEM);
	}
	carveout_heap->base = heap_data->base;
	(void)gen_pool_add(carveout_heap->pool, carveout_heap->base, heap_data->size,
		     -1);
	carveout_heap->heap.ops = &carveout_heap_ops;
	carveout_heap->heap.type = ION_HEAP_TYPE_CARVEOUT;
	//carveout_heap->heap.flags = ION_HEAP_FLAG_DEFER_FREE;

	return &carveout_heap->heap;
}

struct ion_heap *ion_custom_heap_create(struct ion_platform_heap *heap_data)
{
	struct ion_carveout_heap *carveout_heap;
	int ret;

	struct page *heap_page;
	size_t size;

	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	heap_page = pfn_to_page(PFN_DOWN(heap_data->base));
	size = heap_data->size;

	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	ret = ion_heap_pages_zero(heap_page, size, pgprot_writecombine(PAGE_KERNEL));
	if (ret) {
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		return ERR_PTR(ret);
	}

	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	carveout_heap = kzalloc(sizeof(*carveout_heap), GFP_KERNEL);
	if (carveout_heap == NULL) {
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		return ERR_PTR(-ENOMEM);
	}

	carveout_heap->pool = gen_pool_create(PAGE_SHIFT, -1);
	if (carveout_heap->pool == NULL) {
		kfree(carveout_heap);
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		return ERR_PTR(-ENOMEM);
	}
	carveout_heap->base = heap_data->base;
	(void)gen_pool_add(carveout_heap->pool, carveout_heap->base, heap_data->size,
					-1);
	carveout_heap->heap.ops = &carveout_heap_ops;
	carveout_heap->heap.type = heap_data->type;

	return &carveout_heap->heap;
}

void ion_carveout_heap_destroy(struct ion_heap *heap)
{
	struct ion_carveout_heap *carveout_heap =
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
	     container_of(heap, struct  ion_carveout_heap, heap);

	gen_pool_destroy(carveout_heap->pool);
	kfree(carveout_heap);
	carveout_heap = NULL;
}

