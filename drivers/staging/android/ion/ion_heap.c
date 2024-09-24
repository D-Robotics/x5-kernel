/*
 * drivers/staging/android/ion/ion_heap.c
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

#include <linux/err.h>
#include <linux/freezer.h>
#include <linux/kthread.h>
#include <linux/mm.h>
#include <linux/rtmutex.h>
#include <linux/sched.h>
#include <uapi/linux/sched/types.h>
#include <linux/scatterlist.h>
#include <linux/vmalloc.h>
#include <linux/ion.h>
#include <asm/cacheflush.h>

#define HOBOT_ION_MAX_BLOCK		2

//coverity[HIS_VOCF:SUPPRESS], ## violation reason SYSSW_V_VOCF_01
void *ion_heap_map_kernel(struct ion_heap *heap,
			  struct ion_buffer *buffer)
{
	struct scatterlist *sg;
	int j;
	uint32_t i;
	void *vaddr;
	pgprot_t pgprot;
	struct sg_table *table = buffer->hb_sg_table;
	int npages = PAGE_ALIGN(buffer->size) / PAGE_SIZE;
	//coverity[misra_c_2012_directive_4_12_violation], ## violation reason SYSSW_V_4.12_02
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	struct page **pages = vmalloc(sizeof(struct page *) * (size_t)npages);
	struct page **tmp = pages;

	if (pages == NULL)
		return ERR_PTR(-ENOMEM);

	if ((buffer->flags & (uint64_t)ION_FLAG_CACHED) != 0UL)
		pgprot = PAGE_KERNEL;
	else
		pgprot = pgprot_writecombine(PAGE_KERNEL);

	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
	for_each_sg(table->sgl, sg, table->nents, i) {
		//coverity[misra_c_2012_rule_10_8_violation:SUPPRESS], ## violation reason SYSSW_V_10.8_01
		int npages_this_entry = (int)((uint64_t)PAGE_ALIGN(sg->length) / PAGE_SIZE);
		struct page *heap_page = sg_page(sg);

		BUG_ON(i >= (uint32_t)npages);
		for (j = 0; j < npages_this_entry; j++)
			*(tmp++) = heap_page++;
	}
	vaddr = vmap(pages, (uint32_t)npages, VM_MAP, pgprot);
	vfree(pages);

	if (vaddr == NULL)
		return ERR_PTR(-ENOMEM);

	return vaddr;
}

void ion_heap_unmap_kernel(struct ion_heap *heap,
			   struct ion_buffer *buffer)
{
	vunmap(buffer->vaddr);
}

int ion_heap_map_user(struct ion_heap *heap, struct ion_buffer *buffer,
		      struct vm_area_struct *vma)
{
	struct sg_table *table = buffer->hb_sg_table;
	unsigned long addr = vma->vm_start;
	unsigned long offset = vma->vm_pgoff * PAGE_SIZE;
	struct scatterlist *sg;
	uint32_t i;
	int ret;

	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
	for_each_sg(table->sgl, sg, table->nents, i) {
		struct page *heap_page = sg_page(sg);
		unsigned long remainder = vma->vm_end - addr;
		unsigned long len = sg->length;

		if (offset >= sg->length) {
			offset -= sg->length;
			continue;
		} else if (offset) {
			//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
			heap_page += offset / PAGE_SIZE;
			len = sg->length - offset;
			offset = 0;
		} else {
			//do nothing
			;
		}
		len = min(len, remainder);

		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
		//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
		//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_01
		ret = remap_pfn_range(vma, addr, page_to_pfn(heap_page), len,
				      vma->vm_page_prot);

		if (ret) {
			(void)pr_err("%s: remap pfn range failed[%d]\n", __func__, ret);
			return ret;
		}
		addr += len;
		if (addr >= vma->vm_end)
			return 0;
	}
	return 0;
}

static int ion_heap_clear_pages(struct page **pages, int num, pgprot_t pgprot)
{
	void *addr = vmap(pages, (uint32_t)num, VM_MAP, pgprot);

	if (addr == NULL)
		return -ENOMEM;
//	memset(addr, 0, PAGE_SIZE * num);
	vunmap(addr);

	return 0;
}

static int ion_heap_sglist_zero(struct scatterlist *sgl, unsigned int nents,
				pgprot_t pgprot)
{
	int p = 0;
	int ret = 0;
	struct sg_page_iter piter;
#ifdef NO_CACHE
	struct page **page_list;
#else
	struct page *pages[32];
#endif
#ifdef NO_CACHE
	u32 nr_pages = 0;
	size_t size;

	size = sg_dma_len(sgl);
	nr_pages = size / PAGE_SIZE;
	page_list = kmalloc_array(nr_pages, sizeof(*page_list), GFP_KERNEL);
	if (!page_list)
		return -ENOMEM;

	for_each_sg_page(sgl, &piter, nents, 0) {
//		pages[p++] = sg_page_iter_page(&piter);
		page_list[p++] = sg_page_iter_page(&piter);
		//if (p == ARRAY_SIZE(pages)) {
		if (p == nr_pages) {
			//ret = ion_heap_clear_pages(pages, p, pgprot);
			ret = ion_heap_clear_pages(page_list, p, pgprot);
			if (ret)
				return ret;
			p = 0;
		}
	}

#else
	for_each_sg_page(sgl, &piter, nents, 0) {
		pages[p++] = sg_page_iter_page(&piter);
		if ((size_t)p == ARRAY_SIZE(pages)) {
			ret = ion_heap_clear_pages(pages, p, pgprot);
			if (ret)
				return ret;
			p = 0;
		}
	}
#endif

	if (p)
#ifdef NO_CACHE
		ret = ion_heap_clear_pages(page_list, p, pgprot);
#else
		ret = ion_heap_clear_pages(pages, p, pgprot);
#endif
	//kfree(page_list);
//	kfree(pages);
	return ret;
}

//coverity[HIS_VOCF:SUPPRESS], ## violation reason SYSSW_V_VOCF_01
int ion_heap_buffer_zero_ex(struct sg_table * table, unsigned long flags)
{
	pgprot_t pgprot;
	int p = 0;
	struct sg_page_iter piter;
	struct page *pages[32];
	void *addr;

	if ((flags & (uint64_t)ION_FLAG_CACHED) != 0UL) {
		//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
		pgprot = PAGE_KERNEL;
	} else {
		//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
		pgprot = pgprot_writecombine(PAGE_KERNEL);
	}

	for_each_sg_page(table->sgl, &piter, table->nents, 0) {
		pages[p++] = sg_page_iter_page(&piter);
		//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
		if ((uint32_t)p == ARRAY_SIZE(pages)) {
			addr = vmap(pages, (uint32_t)p, VM_MAP, pgprot);
			if (addr == NULL)
				return -ENOMEM;
			(void)memset(addr, 0, PAGE_SIZE * (uint32_t)p);
			dcache_clean_inval_poc((uint64_t)addr, (uint64_t)(addr + (PAGE_SIZE * (uint32_t)p)));
			vunmap(addr);
			p = 0;
		}
	}
	if (p) {
		addr = vmap(pages, (uint32_t)p, VM_MAP, pgprot);
		if (addr == NULL)
			return -ENOMEM;
		(void)memset(addr, 0, PAGE_SIZE * (uint32_t)p);
		dcache_clean_inval_poc((uint64_t)addr, (uint64_t)(addr + (PAGE_SIZE * (uint32_t)p)));
		vunmap(addr);
	}

	return 0;
}

//coverity[HIS_VOCF:SUPPRESS], ## violation reason SYSSW_V_VOCF_01
int ion_heap_buffer_zero(struct ion_buffer *buffer)
{
	struct sg_table *table = buffer->hb_sg_table;
	pgprot_t pgprot;

	if ((buffer->flags & (uint64_t)ION_FLAG_CACHED) != 0UL) {
		//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
		pgprot = PAGE_KERNEL;
	}
	else {
		//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
		pgprot = pgprot_writecombine(PAGE_KERNEL);
		//pgprot = pgprot_noncached(PAGE_KERNEL);
	}

	return ion_heap_sglist_zero(table->sgl, table->nents, pgprot);
}

//coverity[HIS_VOCF:SUPPRESS], ## violation reason SYSSW_V_VOCF_01
int ion_heap_pages_zero(struct page *heap_page, size_t size, pgprot_t pgprot)
{
	struct scatterlist sg[HOBOT_ION_MAX_BLOCK];
	struct scatterlist *tmp_sg, *init_sg;
	unsigned long size_remaining = size;
	uint32_t i = 1U;
	phys_addr_t paddr = PFN_PHYS(page_to_pfn(heap_page));

	while (size_remaining >= SZ_4G) {
		size_remaining -= (SZ_4G - PAGE_SIZE);
		i++;
	}

	sg_init_table(sg, i);
	init_sg = sg;
	size_remaining = size;
	while (size_remaining >= SZ_4G) {
		tmp_sg = init_sg;
		sg_set_page(tmp_sg, pfn_to_page(PFN_DOWN(paddr)), SZ_4G - PAGE_SIZE, 0U);
		paddr += (SZ_4G - PAGE_SIZE);
		size_remaining -= (SZ_4G - PAGE_SIZE);
		init_sg = sg_next(tmp_sg);
	}
	sg_set_page(init_sg, pfn_to_page(PFN_DOWN(paddr)), (uint32_t)size_remaining, 0);
	return ion_heap_sglist_zero(init_sg, i, pgprot);
}

void ion_heap_freelist_add(struct ion_heap *heap, struct ion_buffer *buffer)
{
	spin_lock(&heap->free_lock);
	list_add(&buffer->list, &heap->free_list);
	heap->free_list_size += buffer->size;
	spin_unlock(&heap->free_lock);
	wake_up(&heap->waitqueue);
}

//coverity[misra_c_2012_rule_8_7_violation:SUPPRESS], ## violation reason SYSSW_V_8.7_01
size_t ion_heap_freelist_size(struct ion_heap *heap)
{
	size_t size;

	spin_lock(&heap->free_lock);
	size = heap->free_list_size;
	spin_unlock(&heap->free_lock);

	return size;
}

static size_t _ion_heap_freelist_drain(struct ion_heap *heap, size_t size,
				       bool skip_pools)
{
	struct ion_buffer *buffer;
	size_t total_drained = 0;

	if (ion_heap_freelist_size(heap) == 0U)
		return 0;

	spin_lock(&heap->free_lock);
	if (size == 0U)
		size = heap->free_list_size;

	while (list_empty(&heap->free_list) == 0) {
		if (total_drained >= size)
			break;
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
		buffer = list_first_entry(&heap->free_list, struct ion_buffer,
					  list);
		list_del(&buffer->list);
		heap->free_list_size -= buffer->size;
		if (skip_pools)
			buffer->private_flags |= ION_PRIV_FLAG_SHRINKER_FREE;
		total_drained += buffer->size;
		spin_unlock(&heap->free_lock);
		ion_buffer_destroy(buffer);
		spin_lock(&heap->free_lock);
	}
	spin_unlock(&heap->free_lock);

	return total_drained;
}

size_t ion_heap_freelist_drain(struct ion_heap *heap, size_t size)
{
	return _ion_heap_freelist_drain(heap, size, false);
}

//coverity[misra_c_2012_rule_8_7_violation:SUPPRESS], ## violation reason SYSSW_V_8.7_01
size_t ion_heap_freelist_shrink(struct ion_heap *heap, size_t size)
{
	return _ion_heap_freelist_drain(heap, size, true);
}

static int ion_heap_deferred_free(void *data)
{
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	struct ion_heap *heap = data;

	while (true) {
		struct ion_buffer *buffer;

		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
		wait_event_freezable(heap->waitqueue,
				     ion_heap_freelist_size(heap) > 0U);

		spin_lock(&heap->free_lock);
		if (list_empty(&heap->free_list)) {
			spin_unlock(&heap->free_lock);
			continue;
		}
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
		buffer = list_first_entry(&heap->free_list, struct ion_buffer,
					  list);
		list_del(&buffer->list);
		heap->free_list_size -= buffer->size;
		spin_unlock(&heap->free_lock);
		ion_buffer_destroy(buffer);
	}

	//coverity[misra_c_2012_rule_2_1_violation:SUPPRESS], ## violation reason SYSSW_V_2.1_01
	return 0;
}

int ion_heap_init_deferred_free(struct ion_heap *heap)
{
	INIT_LIST_HEAD(&heap->free_list);
	init_waitqueue_head(&heap->waitqueue);
	heap->task = kthread_run(ion_heap_deferred_free, heap,
				 "%s", heap->name);
	if (IS_ERR(heap->task)) {
		(void)pr_err("%s: creating thread for deferred free failed\n",
		       __func__);
		return PTR_ERR_OR_ZERO(heap->task);
	}
	sched_set_normal(heap->task, 19);

	return 0;
}

static unsigned long ion_heap_shrink_count(struct shrinker *hb_shrinker,
					   struct shrink_control *sc)
{
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
	struct ion_heap *heap = container_of(hb_shrinker, struct ion_heap,
					     hb_shrinker);
	int total = 0;

	//coverity[misra_c_2012_rule_10_8_violation:SUPPRESS], ## violation reason SYSSW_V_10.8_01
	total = (int32_t)(ion_heap_freelist_size(heap) / PAGE_SIZE);

	if (heap->ops->shrink != NULL)
		total += heap->ops->shrink(heap, sc->gfp_mask, 0);

	return (unsigned long)total;
}

static unsigned long ion_heap_shrink_scan(struct shrinker *hb_shrinker,
					  struct shrink_control *sc)
{
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
	struct ion_heap *heap = container_of(hb_shrinker, struct ion_heap,
					     hb_shrinker);
	int freed = 0;
	int to_scan = (int32_t)sc->nr_to_scan;

	if (to_scan == 0)
		return 0;

	/*
	 * shrink the free list first, no point in zeroing the memory if we're
	 * just going to reclaim it. Also, skip any possible page pooling.
	 */
	if (heap->flags & ION_HEAP_FLAG_DEFER_FREE) {
		//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
		//coverity[misra_c_2012_rule_10_8_violation:SUPPRESS], ## violation reason SYSSW_V_10.8_01
		freed = (int32_t)(ion_heap_freelist_shrink(heap, to_scan * PAGE_SIZE) /
				PAGE_SIZE);
	}

	to_scan -= freed;
	if (to_scan <= 0)
		return (unsigned long)freed;

	if (heap->ops->shrink != NULL)
		freed += heap->ops->shrink(heap, sc->gfp_mask, to_scan);
	return (unsigned long)freed;
}

void ion_heap_init_shrinker(struct ion_heap *heap)
{
	heap->hb_shrinker.count_objects = ion_heap_shrink_count;
	heap->hb_shrinker.scan_objects = ion_heap_shrink_scan;
	heap->hb_shrinker.seeks = DEFAULT_SEEKS;
	heap->hb_shrinker.batch = 0;
	//register_shrinker(&heap->hb_shrinker);
	(void)register_shrinker(&heap->hb_shrinker, NULL);
}

struct ion_heap *ion_heap_create(struct ion_platform_heap *heap_data)
{
	struct ion_heap *heap = NULL;

	switch (heap_data->type) {
	case ION_HEAP_TYPE_SYSTEM_CONTIG:
		heap = ion_system_contig_heap_create(heap_data);
		break;
	case ION_HEAP_TYPE_SYSTEM:
		heap = ion_system_heap_create(heap_data);
		break;
	case ION_HEAP_TYPE_CARVEOUT:
		heap = ion_carveout_heap_create(heap_data);
		break;
	case ION_HEAP_TYPE_CHUNK:
		heap = ion_chunk_heap_create(heap_data);
		break;
	case ION_HEAP_TYPE_CUSTOM:
	case ION_HEAP_TYPE_CMA_RESERVED:
	case ION_HEAP_TYPE_SRAM_LIMIT:
	case ION_HEAP_TYPE_INLINE_ECC:
	case ION_HEAP_TYPE_DMA:
		heap = ion_custom_heap_create(heap_data);
		break;
	default:
		(void)pr_err("%s: Invalid heap type %d\n", __func__,
		       heap_data->type);
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		return ERR_PTR(-EINVAL);
	}

	if (IS_ERR_OR_NULL(heap)) {
		(void)pr_err("%s: error creating heap %s type %d base %lld size %zu\n",
		       __func__, heap_data->name, heap_data->type,
		       heap_data->base, heap_data->size);
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		return (heap != NULL) ? heap : ERR_PTR(-EINVAL);
	}

	heap->name = heap_data->name;
	heap->id = heap_data->id;
	heap->total_size = heap_data->size;
	return heap;
}
//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
EXPORT_SYMBOL(ion_heap_create);

void ion_heap_destroy(struct ion_heap *heap)
{
	if (heap == NULL)
		return;

	switch (heap->type) {
	case ION_HEAP_TYPE_SYSTEM_CONTIG:
		ion_system_contig_heap_destroy(heap);
		break;
	case ION_HEAP_TYPE_SYSTEM:
		ion_system_heap_destroy(heap);
		break;
	case ION_HEAP_TYPE_CARVEOUT:
	case ION_HEAP_TYPE_CUSTOM:
	case ION_HEAP_TYPE_CMA_RESERVED:
	case ION_HEAP_TYPE_SRAM_LIMIT:
	case ION_HEAP_TYPE_INLINE_ECC:
		ion_carveout_heap_destroy(heap);
		break;
	case ION_HEAP_TYPE_CHUNK:
		ion_chunk_heap_destroy(heap);
		break;
	default:
		pr_err("%s: Invalid heap type %d\n", __func__,
		       heap->type);
	}
}
//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
EXPORT_SYMBOL(ion_heap_destroy);
