/****************************************************************************
 *
 *    The MIT License (MIT)
 *
 *    Copyright (c) 2012 - 2022 Vivante Corporation
 *
 *    Permission is hereby granted, free of charge, to any person obtaining a
 *    copy of this software and associated documentation files (the "Software"),
 *    to deal in the Software without restriction, including without limitation
 *    the rights to use, copy, modify, merge, publish, distribute, sublicense,
 *    and/or sell copies of the Software, and to permit persons to whom the
 *    Software is furnished to do so, subject to the following conditions:
 *
 *    The above copyright notice and this permission notice shall be included in
 *    all copies or substantial portions of the Software.
 *
 *    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *    DEALINGS IN THE SOFTWARE.
 *
 *****************************************************************************
 *
 *    The GPL License (GPL)
 *
 *    Copyright (C) 2012 - 2022 Vivante Corporation
 *
 *    This program is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU General Public License
 *    as published by the Free Software Foundation; either version 2
 *    of the License, or (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software Foundation,
 *    Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *****************************************************************************
 *
 *    Note: This software is released under dual MIT and GPL licenses. A
 *    recipient may use this file under the terms of either the MIT license or
 *    GPL License. If you wish to use only one license not the other, you can
 *    indicate your decision by deleting one of the above license notices in your
 *    version of this file.
 *
 *****************************************************************************/

#ifndef _nano2d_kernel_common_h_
#define _nano2d_kernel_common_h_

#include "nano2D_kernel.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct n2d_hash_node {
	n2d_uint32_t key;
	n2d_uintptr_t data1;
	n2d_uintptr_t data2;
	struct n2d_hash_node *next;
} n2d_hash_node_t;

typedef struct n2d_hash_map {
	n2d_int32_t size;
	n2d_kernel_t *kernel;
	n2d_hash_node_t **hash_array;
} n2d_hash_map_t;

typedef struct n2d_handle_bitmap {
	n2d_uint32_t free;
	n2d_uint32_t next_handle;
	n2d_uint32_t capacity;
	n2d_kernel_t *kernel;
	n2d_uint32_t *page;
} n2d_handle_bitmap_t;

typedef struct n2d_list_head {
	struct n2d_list_head *next; /* next in chain */
	struct n2d_list_head *prev; /* previous in chain */
} n2d_list_head_t;

/* bitmap */
n2d_error_t n2d_handle_construct(n2d_kernel_t *kernel, n2d_handle_bitmap_t **bitmap);

n2d_error_t n2d_handle_destroy(n2d_handle_bitmap_t *bitmap);

n2d_error_t n2d_handle_get(n2d_handle_bitmap_t *bitmap, n2d_uint32_t *handle);

n2d_error_t n2d_handle_free(n2d_handle_bitmap_t *bitmap, n2d_uint32_t handle);

/* hash map */
n2d_error_t n2d_hash_map_construct(n2d_kernel_t *kernel, n2d_int32_t size,
				   n2d_hash_map_t **hash_map);

n2d_error_t n2d_hash_map_destroy(n2d_hash_map_t *hash_map);

n2d_error_t n2d_hash_map_insert(n2d_hash_map_t *hash_map, n2d_uint32_t key, n2d_uintptr_t data1,
				n2d_uintptr_t data2);

// n2d_error_t n2d_hash_map_remove(n2d_hash_map_t *hash_map, n2d_uint32_t key);

n2d_error_t n2d_hash_map_get(n2d_hash_map_t *hash_map, n2d_uint32_t key, n2d_uintptr_t *data1,
			     n2d_uintptr_t *data2);

n2d_error_t n2d_hash_map_get_remove(n2d_hash_map_t *hash_map, n2d_uint32_t key,
				    n2d_uintptr_t *data1, n2d_uintptr_t *data2);

n2d_error_t n2d_hash_map_iterate_keys(n2d_hash_map_t *hash_map,
										n2d_uint32_t *keys,
										n2d_uint32_t *count,
										n2d_uint32_t max_count);

/* list */
/*
 * This is a simple doubly linked list implementation.
 */
/* Initialise a static list */
#define N2D_LIST_HEAD(name) struct n2d_list_head name = {&(name), &(name)}

/* Initialise a list head to an empty list */
#define N2D_INIT_LIST_HEAD(p)    \
	do {                     \
		(p)->next = (p); \
		(p)->prev = (p); \
	} while (0)

static gcmINLINE void n2d_list_add(struct n2d_list_head *new_entry, struct n2d_list_head *list)
{
	struct n2d_list_head *list_next = list->next;

	list->next	= new_entry;
	new_entry->prev = list;
	new_entry->next = list_next;
	list_next->prev = new_entry;
}

static gcmINLINE void n2d_list_add_tail(struct n2d_list_head *new_entry, struct n2d_list_head *list)
{
	struct n2d_list_head *list_prev = list->prev;

	list->prev	= new_entry;
	new_entry->next = list;
	new_entry->prev = list_prev;
	list_prev->next = new_entry;
}

static gcmINLINE void n2d_list_del(struct n2d_list_head *entry)
{
	struct n2d_list_head *list_next = entry->next;
	struct n2d_list_head *list_prev = entry->prev;

	list_next->prev = list_prev;
	list_prev->next = list_next;
}

static gcmINLINE void n2d_list_del_init(struct n2d_list_head *entry)
{
	n2d_list_del(entry);
	entry->next = entry->prev = entry;
}

static gcmINLINE int n2d_list_empty(struct n2d_list_head *entry)
{
	return (entry->next == entry);
}

#define n2d_list_entry(entry, type, member) \
	((type *)((char *)(entry) - (unsigned long)(&((type *)NULL)->member)))

#define n2d_list_for_each(itervar, list) \
	for (itervar = (list)->next; itervar != (list); itervar = itervar->next)

#define n2d_list_for_each_entry(pos, head, member)                                             \
	for (pos = n2d_list_entry((head)->next, typeof(*pos), member); &pos->member != (head); \
	     pos = n2d_list_entry(pos->member.next, typeof(*pos), member))

#define n2d_list_for_each_safe(itervar, save_var, list)                                \
	for (itervar = (list)->next, save_var = (list)->next->next; itervar != (list); \
	     itervar = save_var, save_var = save_var->next)

#define n2d_list_for_each_entry_safe(pos, n, head, member)                 \
	for (pos = n2d_list_entry((head)->next, typeof(*pos), member),     \
	    n	 = n2d_list_entry(pos->member.next, typeof(*pos), member); \
	     &pos->member != (head);                                       \
	     pos = n, n = n2d_list_entry(n->member.next, typeof(*n), member))

#ifdef __cplusplus
}
#endif

#endif /* _nano2d_kernel_common_h_ */
