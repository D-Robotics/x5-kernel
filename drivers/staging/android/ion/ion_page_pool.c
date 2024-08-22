/*
 * drivers/staging/android/ion/ion_mem_pool.c
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

#include <linux/debugfs.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/swap.h>
#include <linux/ion.h>

#define TRUE		(bool)1
#define FALSE		(bool)0

static inline struct page *ion_page_pool_alloc_pages(struct ion_page_pool *pool)
{
	if (fatal_signal_pending(current))
		return NULL;
	return alloc_pages(pool->gfp_mask, pool->order);
}

static void ion_page_pool_free_pages(struct ion_page_pool *pool,
				     struct page *heap_page)
{
	__free_pages(heap_page, pool->order);
}

static int ion_page_pool_add(struct ion_page_pool *pool, struct page *heap_page)
{
	struct mutex *pool_mutex;

	pool_mutex = &pool->mutex;
	mutex_lock(pool_mutex);
	if (PageHighMem(heap_page)) {
		list_add_tail(&heap_page->lru, &pool->high_items);
		pool->high_count++;
	} else {
		list_add_tail(&heap_page->lru, &pool->low_items);
		pool->low_count++;
	}
	mutex_unlock(pool_mutex);
	return 0;
}

static struct page *ion_page_pool_remove(struct ion_page_pool *pool, bool high)
{
	struct page *heap_page;

	if (high) {
		BUG_ON(pool->high_count == 0);
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
		heap_page = list_first_entry(&pool->high_items, struct page, lru);
		pool->high_count--;
	} else {
		BUG_ON(pool->low_count == 0);
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
		heap_page = list_first_entry(&pool->low_items, struct page, lru);
		pool->low_count--;
	}

	list_del(&heap_page->lru);
	return heap_page;
}

struct page *ion_page_pool_alloc(struct ion_page_pool *pool)
{
	struct page *heap_page = NULL;

	BUG_ON(pool == NULL);

	mutex_lock(&pool->mutex);
	if (pool->high_count)
		heap_page = ion_page_pool_remove(pool, TRUE);
	else if (pool->low_count)
		heap_page = ion_page_pool_remove(pool, FALSE);
	else
		;
	mutex_unlock(&pool->mutex);

	if (heap_page == NULL)
		heap_page = ion_page_pool_alloc_pages(pool);

	return heap_page;
}

void ion_page_pool_free(struct ion_page_pool *pool, struct page *page)
{
	BUG_ON(pool->order != compound_order(page));

	(void)ion_page_pool_add(pool, page);
}

static int ion_page_pool_total(struct ion_page_pool *pool, bool high)
{
	int count = pool->low_count;
	uint32_t return_count = 0;

	if (high)
		count += pool->high_count;

	return_count = (uint32_t)count << pool->order;

	return (int)return_count;
}

int ion_page_pool_shrink(struct ion_page_pool *pool, gfp_t gfp_mask,
			 int nr_to_scan)
{
	int freed = 0;
	bool high;

	if (current_is_kswapd())
		high = TRUE;
	else
		high = !((gfp_mask & __GFP_HIGHMEM) == 0U);

	if (nr_to_scan == 0)
		return ion_page_pool_total(pool, high);

	while (freed < nr_to_scan) {
		struct page *heap_page;

		mutex_lock(&pool->mutex);
		if (pool->low_count) {
			heap_page = ion_page_pool_remove(pool, FALSE);
		} else if (high && (pool->high_count != 0)) {
			heap_page = ion_page_pool_remove(pool, TRUE);
		} else {
			mutex_unlock(&pool->mutex);
			break;
		}
		mutex_unlock(&pool->mutex);
		ion_page_pool_free_pages(pool, heap_page);
		freed += (1 << pool->order);
	}

	return freed;
}

struct ion_page_pool *ion_page_pool_create(gfp_t gfp_mask, unsigned int order,
					   bool cached)
{
	//coverity[misra_c_2012_directive_4_12_violation], ## violation reason SYSSW_V_4.12_02
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	struct ion_page_pool *pool = kmalloc(sizeof(*pool), GFP_KERNEL);

	if (pool == NULL)
		return NULL;
	pool->high_count = 0;
	pool->low_count = 0;
	INIT_LIST_HEAD(&pool->low_items);
	INIT_LIST_HEAD(&pool->high_items);
	pool->gfp_mask = gfp_mask | __GFP_COMP;
	pool->order = order;
	mutex_init(&pool->mutex);
	plist_node_init(&pool->list, (int)order);
	if (cached)
		pool->cached = TRUE;

	return pool;
}

void ion_page_pool_destroy(struct ion_page_pool *pool)
{
	kfree(pool);
}

static int __init ion_page_pool_init(void)
{
	return 0;
}
//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
device_initcall(ion_page_pool_init);
