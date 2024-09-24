/*
 * drivers/staging/android/ion/ion_chunk_heap.c
 *
 * Copyright (C) 2012 Google, Inc.
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
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/genalloc.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/ion.h>

struct ion_chunk_heap {
	struct ion_heap heap;
	struct gen_pool *pool;
	phys_addr_t base;
	unsigned long chunk_size;
	unsigned long size;
	unsigned long allocated;
};

//coverity[HIS_VOCF:SUPPRESS], ## violation reason SYSSW_V_VOCF_01
//coverity[HIS_CCM:SUPPRESS], ## violation reason SYSSW_V_CCM_01
static int ion_chunk_heap_allocate(struct ion_heap *heap,
				      struct ion_buffer *buffer,
				      unsigned long size, unsigned long align,
				      unsigned long flags)
{
	struct ion_chunk_heap *chunk_heap =
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
		container_of(heap, struct ion_chunk_heap, heap);
	struct sg_table *table;
	struct scatterlist *sg;
	int ret, i;
	unsigned long num_chunks;
	unsigned long allocated_size;

	if (align > chunk_heap->chunk_size)
		return -EINVAL;

	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	allocated_size = ALIGN(size, chunk_heap->chunk_size);
	num_chunks = allocated_size / chunk_heap->chunk_size;

	if (allocated_size > chunk_heap->size - chunk_heap->allocated)
		return -ENOMEM;

	//coverity[misra_c_2012_directive_4_12_violation], ## violation reason SYSSW_V_4.12_02
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	table = kmalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (table == NULL)
		return -ENOMEM;
	ret = sg_alloc_table(table, (uint32_t)num_chunks, GFP_KERNEL);
	if (ret) {
		kfree(table);
		return ret;
	}

	sg = table->sgl;
	for (i = 0; (uint32_t)i < num_chunks; i++) {
		unsigned long paddr = gen_pool_alloc(chunk_heap->pool,
						     chunk_heap->chunk_size);
		if (paddr == 0UL) {
			//coverity[misra_c_2012_rule_15_1_violation:SUPPRESS], ## violation reason SYSSW_V_15.1_01
			goto err;
		}
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
		//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
		//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
		sg_set_page(sg, pfn_to_page(PFN_DOWN(paddr)),
				(uint32_t)chunk_heap->chunk_size, 0U);
		sg = sg_next(sg);
	}

	buffer->priv_virt = table;
	buffer->hb_sg_table = table;
	chunk_heap->allocated += allocated_size;
	return 0;
err:
	sg = table->sgl;
	for (i -= 1; i >= 0; i--) {
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
		//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
		//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
		gen_pool_free(chunk_heap->pool, page_to_phys(sg_page(sg)),
			      sg->length);
		sg = sg_next(sg);
	}
	sg_free_table(table);
	kfree(table);
	return -ENOMEM;
}

static void ion_chunk_heap_free(struct ion_buffer *buffer)
{
	struct ion_heap *heap = buffer->heap;
	struct ion_chunk_heap *chunk_heap =
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
		container_of(heap, struct ion_chunk_heap, heap);
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	struct sg_table *table = buffer->priv_virt;
	struct scatterlist *sg;
	uint32_t i;
	unsigned long allocated_size;

	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	allocated_size = ALIGN(buffer->size, chunk_heap->chunk_size);

	(void)ion_heap_buffer_zero(buffer);

	//if (ion_buffer_cached(buffer))
	//	dma_sync_sg_for_device(NULL, table->sgl, table->nents,
	//						DMA_BIDIRECTIONAL);

	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
	for_each_sg(table->sgl, sg, table->nents, i) {
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
		//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
		//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
		gen_pool_free(chunk_heap->pool, page_to_phys(sg_page(sg)),
			      sg->length);
	}
	chunk_heap->allocated -= allocated_size;
	sg_free_table(table);
	kfree(table);
}

static struct sg_table *ion_chunk_heap_map_dma(struct ion_heap *heap,
					       struct ion_buffer *buffer)
{
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	return buffer->priv_virt;
}

static void ion_chunk_heap_unmap_dma(struct ion_heap *heap,
				     struct ion_buffer *buffer)
{
}

//coverity[misra_c_2012_rule_8_9_violation:SUPPRESS], ## violation reason SYSSW_V_8.9_02
static struct ion_heap_ops chunk_heap_ops = {
	.allocate = ion_chunk_heap_allocate,
	.free = ion_chunk_heap_free,
	.map_dma = ion_chunk_heap_map_dma,
	.unmap_dma = ion_chunk_heap_unmap_dma,
	.map_user = ion_heap_map_user,
	.map_kernel = ion_heap_map_kernel,
	.unmap_kernel = ion_heap_unmap_kernel,
};

//coverity[HIS_VOCF:SUPPRESS], ## violation reason SYSSW_V_VOCF_01
struct ion_heap *ion_chunk_heap_create(struct ion_platform_heap *heap_data)
{
	struct ion_chunk_heap *chunk_heap;
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
	chunk_heap = kzalloc(sizeof(*chunk_heap), GFP_KERNEL);
	if (chunk_heap == NULL) {
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		return ERR_PTR(-ENOMEM);
	}

	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_11.6_01
	chunk_heap->chunk_size = (unsigned long)heap_data->priv;
	chunk_heap->pool = gen_pool_create(get_order(chunk_heap->chunk_size) +
					   PAGE_SHIFT, -1);
	if (chunk_heap->pool == NULL) {
		ret = -ENOMEM;
		//coverity[misra_c_2012_rule_15_1_violation:SUPPRESS], ## violation reason SYSSW_V_15.1_01
		goto error_gen_pool_create;
	}
	chunk_heap->base = heap_data->base;
	chunk_heap->size = heap_data->size;
	chunk_heap->allocated = 0;

	(void)gen_pool_add(chunk_heap->pool, chunk_heap->base, heap_data->size, -1);
	chunk_heap->heap.ops = &chunk_heap_ops;
	chunk_heap->heap.type = ION_HEAP_TYPE_CHUNK;
	chunk_heap->heap.flags = ION_HEAP_FLAG_DEFER_FREE;
	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
	pr_debug("%s: base %pa size %zu\n", __func__,
		 &chunk_heap->base, heap_data->size);

	return &chunk_heap->heap;

error_gen_pool_create:
	kfree(chunk_heap);
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	return ERR_PTR(ret);
}

void ion_chunk_heap_destroy(struct ion_heap *heap)
{
	struct ion_chunk_heap *chunk_heap =
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
	     container_of(heap, struct  ion_chunk_heap, heap);

	gen_pool_destroy(chunk_heap->pool);
	kfree(chunk_heap);
	chunk_heap = NULL;
}

