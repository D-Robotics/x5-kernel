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
int32_t get_carveout_info(struct ion_heap *heap, uint32_t *avail, uint32_t *max_contigous)
{
	struct ion_carveout_heap *carveout_heap = container_of(heap, struct ion_carveout_heap, heap);
	struct gen_pool_chunk *chunk;
	struct gen_pool *pool = carveout_heap->pool;
	uint32_t avail_mem = 0, max_range = 0;
	uint32_t start_bit, end_bit, index, i, max_range_bit = 0;
	int32_t order;
	order = pool->min_alloc_order;

	rcu_read_lock();
	list_for_each_entry_rcu(chunk, &pool->chunks, next_chunk) {
		avail_mem += atomic_long_read(&chunk->avail);
		if (max_range > atomic_long_read(&chunk->avail))
			continue;

		start_bit = 0;
		end_bit = (chunk->end_addr - chunk->start_addr + 1) >> order;

		index = find_next_zero_bit(chunk->bits, end_bit, start_bit);
		while(index < end_bit) {
			//find next set bit
			i = find_next_bit(chunk->bits, end_bit, index);
			//compare and get the max range
			max_range_bit = max_range_bit > (i - index) ? max_range_bit : (i - index);
			max_range = max_range_bit << order;
			//get the next range
			index = find_next_zero_bit(chunk->bits, end_bit, i);
		}
	}
	rcu_read_unlock();
	*avail = avail_mem;
	*max_contigous = max_range;
	return 0;
}

phys_addr_t ion_carveout_allocate(struct ion_heap *heap,
				      unsigned long size,
				      unsigned long align)
{
	struct ion_carveout_heap *carveout_heap =
		container_of(heap, struct ion_carveout_heap, heap);
	unsigned long offset = gen_pool_alloc(carveout_heap->pool, size);

	if (!offset) {
		pr_debug("%s: alloc buffer from pool failed\n", __func__);
		return ION_CARVEOUT_ALLOCATE_FAIL;
	}

	return offset;
}

void ion_carveout_free(struct ion_heap *heap, phys_addr_t addr,
		       unsigned long size)
{
	struct ion_carveout_heap *carveout_heap =
		container_of(heap, struct ion_carveout_heap, heap);

	if (addr == ION_CARVEOUT_ALLOCATE_FAIL)
		return;
	gen_pool_free(carveout_heap->pool, addr, size);
}

static int ion_carveout_heap_phys(struct ion_heap *heap,
				  struct ion_buffer *buffer,
				  phys_addr_t *addr, size_t *len)
{
	struct sg_table *table = buffer->priv_virt;
	struct page *page = sg_page(table->sgl);
	phys_addr_t paddr = PFN_PHYS(page_to_pfn(page));

	*addr = paddr;
	*len = buffer->size;
	return 0;
}

static int ion_carveout_heap_allocate(struct ion_heap *heap,
				      struct ion_buffer *buffer,
				      unsigned long size, unsigned long align,
				      unsigned long flags)
{
	struct sg_table *table;
	phys_addr_t paddr;
	int ret;

	if (align > PAGE_SIZE) {
		pr_err("%s: align should <= page size\n", __func__);
		return -EINVAL;
	}

	table = kmalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!table)
		return -ENOMEM;

	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret) {
		pr_err("%s: sg alloc table failed\n", __func__);
		goto err_free;
	}

	paddr = ion_carveout_allocate(heap, size, align);
	if (paddr == ION_CARVEOUT_ALLOCATE_FAIL) {
		ret = -ENOMEM;
		goto err_free_table;
	}

	sg_set_page(table->sgl, pfn_to_page(PFN_DOWN(paddr)), size, 0);
	buffer->priv_virt = table;
	buffer->sg_table = table;

	if (buffer->flags & ION_FLAG_INITIALIZED) {
		ion_heap_buffer_zero_ex(table, buffer->flags);
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
	struct sg_table *table = buffer->priv_virt;
	struct page *page = sg_page(table->sgl);
	phys_addr_t paddr = PFN_PHYS(page_to_pfn(page));

	//ion_heap_buffer_zero(buffer);

	if (ion_buffer_cached(buffer)) {
		//dma_sync_sg_for_device(NULL, table->sgl, table->nents,
		//					DMA_BIDIRECTIONAL);
		dcache_clean_inval_poc((unsigned long)phys_to_virt(paddr), (unsigned long)phys_to_virt(paddr) + buffer->size);
	}

	ion_carveout_free(heap, paddr, buffer->size);
	sg_free_table(table);
	kfree(table);
}

static struct sg_table *ion_carveout_heap_map_dma(struct ion_heap *heap,
						  struct ion_buffer *buffer)
{
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

	struct page *page;
	size_t size;

	page = pfn_to_page(PFN_DOWN(heap_data->base));
	size = heap_data->size;

	ret = ion_heap_pages_zero(page, size, pgprot_writecombine(PAGE_KERNEL));
	//ret = ion_heap_pages_zero(page, size, pgprot_noncached(PAGE_KERNEL));
	if (ret)
		return ERR_PTR(ret);

	carveout_heap = kzalloc(sizeof(*carveout_heap), GFP_KERNEL);
	if (!carveout_heap)
		return ERR_PTR(-ENOMEM);

	carveout_heap->pool = gen_pool_create(PAGE_SHIFT, -1);
	if (!carveout_heap->pool) {
		kfree(carveout_heap);
		return ERR_PTR(-ENOMEM);
	}
	carveout_heap->base = heap_data->base;
	gen_pool_add(carveout_heap->pool, carveout_heap->base, heap_data->size,
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

        struct page *page;
        size_t size;

        page = pfn_to_page(PFN_DOWN(heap_data->base));
        size = heap_data->size;

        ret = ion_heap_pages_zero(page, size, pgprot_writecombine(PAGE_KERNEL));
        if (ret)
                return ERR_PTR(ret);

        carveout_heap = kzalloc(sizeof(*carveout_heap), GFP_KERNEL);
        if (!carveout_heap)
                return ERR_PTR(-ENOMEM);

        carveout_heap->pool = gen_pool_create(PAGE_SHIFT, -1);
        if (!carveout_heap->pool) {
                kfree(carveout_heap);
                return ERR_PTR(-ENOMEM);
        }
        carveout_heap->base = heap_data->base;
        gen_pool_add(carveout_heap->pool, carveout_heap->base, heap_data->size,
                     -1);
        carveout_heap->heap.ops = &carveout_heap_ops;
        carveout_heap->heap.type = heap_data->type;

        return &carveout_heap->heap;
}

void ion_carveout_heap_destroy(struct ion_heap *heap)
{
	struct ion_carveout_heap *carveout_heap =
	     container_of(heap, struct  ion_carveout_heap, heap);

	gen_pool_destroy(carveout_heap->pool);
	kfree(carveout_heap);
	carveout_heap = NULL;
}

