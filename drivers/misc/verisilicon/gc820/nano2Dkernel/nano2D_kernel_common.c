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

#include "nano2D_kernel_common.h"

n2d_error_t n2d_handle_construct(n2d_kernel_t *kernel, n2d_handle_bitmap_t **bitmap)
{
	n2d_error_t error	     = N2D_SUCCESS;
	n2d_uint32_t size	     = 1024;
	n2d_handle_bitmap_t *_bitmap = N2D_NULL;
	n2d_pointer pointer	     = N2D_NULL;

	ONERROR(n2d_kernel_os_allocate(kernel->os, sizeof(n2d_handle_bitmap_t), &pointer));
	if (!pointer)
		return N2D_OUT_OF_MEMORY;
	_bitmap = (n2d_handle_bitmap_t *)pointer;

	/*allocate*/
	ONERROR(n2d_kernel_os_allocate(kernel->os, size * sizeof(n2d_uint32_t), &pointer));
	if (!pointer)
		return N2D_OUT_OF_MEMORY;
	n2d_kernel_os_memory_fill(kernel->os, pointer, 0, size * sizeof(n2d_uint32_t));
	_bitmap->page = (n2d_uint32_t *)pointer;

	_bitmap->free = _bitmap->capacity = size * 32;
	_bitmap->next_handle		  = 0;
	_bitmap->kernel			  = kernel;
	*bitmap				  = _bitmap;

	return N2D_SUCCESS;

on_error:

	if (_bitmap && _bitmap->page)
		n2d_kernel_os_free(kernel->os, _bitmap->page);

	if (_bitmap)
		n2d_kernel_os_free(kernel->os, _bitmap);

	/* Return the status. */
	return error;
}

n2d_error_t n2d_handle_destroy(n2d_handle_bitmap_t *bitmap)
{
	n2d_error_t error    = N2D_SUCCESS;
	n2d_kernel_t *kernel = bitmap->kernel;

	if (bitmap->page) {
		ONERROR(n2d_kernel_os_free(kernel->os, bitmap->page));
		bitmap->page = N2D_NULL;
	}
	if (bitmap) {
		ONERROR(n2d_kernel_os_free(kernel->os, bitmap));
		bitmap = N2D_NULL;
	}

on_error:
	/* Return the status. */
	return error;
}

n2d_error_t n2d_handle_get(n2d_handle_bitmap_t *bitmap, n2d_uint32_t *handle)
{
	n2d_error_t error = N2D_SUCCESS;
	n2d_uint32_t pos = 0, n = 0, i = 0;

	pos = bitmap->next_handle;
	n   = pos >> 5;
	i   = pos & 31;

	while (bitmap->page[n] & (1u << i)) {
		pos++;

		if (pos == bitmap->capacity) {
			/*wrap to the begin.*/
			pos = 0;
		}

		n = pos >> 5;
		i = pos & 31;
	}

	bitmap->page[n] |= (1u << i);

	*handle = pos + 1;

	bitmap->next_handle = (pos + 1) % bitmap->capacity;
	--(bitmap->free);

	/* Return the status. */
	return error;
}

n2d_error_t n2d_handle_free(n2d_handle_bitmap_t *bitmap, n2d_uint32_t handle)
{
	n2d_error_t error = N2D_SUCCESS;
	n2d_uint32_t pos = handle - 1, n = 0, i = 0;

	n = pos >> 5;
	i = pos & 31;

	if (pos >= bitmap->capacity || ((bitmap->page[n] & (1u << i)) == 0)) {
		ONERROR(N2D_INVALID_ARGUMENT);
	}

	/* clear handle */
	bitmap->page[n] &= ~(1u << i);

	++bitmap->free;

on_error:
	/* Return the status. */
	return error;
}

/* hash map */
n2d_error_t n2d_hash_map_construct(n2d_kernel_t *kernel, n2d_int32_t size,
				   n2d_hash_map_t **hash_map)
{
	n2d_error_t error   = N2D_SUCCESS;
	n2d_hash_map_t *map = N2D_NULL;

	ONERROR(n2d_kernel_os_allocate(kernel->os, sizeof(n2d_hash_map_t), (n2d_pointer)&map));
	ONERROR(n2d_kernel_os_allocate(kernel->os, size * sizeof(n2d_hash_node_t *),
				       (n2d_pointer)&map->hash_array));
	ONERROR(n2d_kernel_os_memory_fill(kernel->os, map->hash_array, 0,
					  size * sizeof(n2d_hash_node_t *)));

	if (map == N2D_NULL || map->hash_array == N2D_NULL)
		return N2D_OUT_OF_MEMORY;

	map->size   = size;
	map->kernel = kernel;
	*hash_map   = map;
	return error;

on_error:
	if (map && map->hash_array)
		n2d_kernel_os_free(kernel->os, map->hash_array);
	if (map)
		n2d_kernel_os_free(kernel->os, map);
	return error;
}

n2d_error_t n2d_hash_map_destroy(n2d_hash_map_t *hash_map)
{
	n2d_error_t error    = N2D_SUCCESS;
	n2d_kernel_t *kernel = hash_map->kernel;
	n2d_int32_t i	     = 0;

	for (i = 0; i < hash_map->size; i++) {
		n2d_hash_node_t *node = hash_map->hash_array[i];
		n2d_hash_node_t *prev = N2D_NULL;

		while (node != N2D_NULL) {
			prev = node;
			node = node->next;
			n2d_kernel_os_free(kernel->os, prev);
		}
	}

	n2d_kernel_os_free(kernel->os, hash_map->hash_array);
	n2d_kernel_os_free(kernel->os, hash_map);

	return error;
}

n2d_error_t n2d_hash_map_insert(n2d_hash_map_t *hash_map, n2d_uint32_t key, n2d_uintptr_t data1,
				n2d_uintptr_t data2)
{
	n2d_error_t error     = N2D_SUCCESS;
	n2d_kernel_t *kernel  = hash_map->kernel;
	n2d_hash_node_t *node = N2D_NULL;
	n2d_int32_t index     = 0;

	ONERROR(n2d_kernel_os_allocate(kernel->os, sizeof(n2d_hash_node_t), (n2d_pointer)&node));
	if (node == N2D_NULL) {
		return N2D_OUT_OF_MEMORY;
	}
	ONERROR(n2d_kernel_os_memory_fill(kernel->os, node, 0, sizeof(n2d_hash_node_t)));

	node->key   = key;
	node->data1 = data1;
	node->data2 = data2;
	node->next  = N2D_NULL;
	index	    = key % hash_map->size;

	if (hash_map->hash_array[index] == N2D_NULL) {
		hash_map->hash_array[index] = node;
	} else {
		/* insert to head */
		node->next		    = hash_map->hash_array[index];
		hash_map->hash_array[index] = node;
	}
on_error:
	return error;
}

// n2d_error_t n2d_hash_map_remove(n2d_hash_map_t *hash_map, n2d_uint32_t key)
// {
// 	n2d_error_t error     = N2D_SUCCESS;
// 	n2d_int32_t index     = key % hash_map->size;
// 	n2d_hash_node_t *node = hash_map->hash_array[index];
// 	n2d_hash_node_t *prev = N2D_NULL;
// 	n2d_kernel_t *kernel  = hash_map->kernel;

// 	if (node == N2D_NULL) {
// 		return N2D_INVALID_ARGUMENT;
// 	}

// 	while (node) {
// 		if (key == node->key) {
// 			if (node == hash_map->hash_array[index]) {
// 				hash_map->hash_array[index] = node->next;
// 				ONERROR(n2d_kernel_os_free(kernel->os, node));
// 			} else {
// 				prev->next = node->next;
// 				ONERROR(n2d_kernel_os_free(kernel->os, node));
// 			}
// 			return N2D_SUCCESS;
// 		}
// 		prev = node;
// 		node = node->next;
// 	}
// on_error:
// 	return error;
// }

n2d_error_t n2d_hash_map_get(n2d_hash_map_t *hash_map, n2d_uint32_t key, n2d_uintptr_t *data1,
			     n2d_uintptr_t *data2)
{
	n2d_error_t error     = N2D_SUCCESS;
	n2d_int32_t index     = key % hash_map->size;
	n2d_hash_node_t *node = hash_map->hash_array[index];

	while (node != N2D_NULL) {
		if (key == node->key) {
			*data1 = node->data1;
			*data2 = node->data2;
			return N2D_SUCCESS;
		}
		node = node->next;
	}

	error = N2D_NOT_FOUND;

	return error;
}

n2d_error_t n2d_hash_map_get_remove(n2d_hash_map_t *hash_map, n2d_uint32_t key,
				    n2d_uintptr_t *data1, n2d_uintptr_t *data2)
{
	n2d_error_t error     = N2D_SUCCESS;
	n2d_int32_t index     = key % hash_map->size;
	n2d_hash_node_t *node = hash_map->hash_array[index];
	n2d_hash_node_t *prev = node;
	n2d_kernel_t *kernel  = hash_map->kernel;

	if (node == N2D_NULL) {
		return N2D_INVALID_ARGUMENT;
	}

	while (node) {
		if (key == node->key) {
			if (data1)
				*data1 = node->data1;
			if (data2)
				*data2 = node->data2;
			if (node == hash_map->hash_array[index]) {
				hash_map->hash_array[index] = node->next;
				ONERROR(n2d_kernel_os_free(kernel->os, node));
			} else {
				prev->next = node->next;
				ONERROR(n2d_kernel_os_free(kernel->os, node));
			}
			return N2D_SUCCESS;
		}
		prev = node;
		node = node->next;
	}
on_error:
	return error;
}

__maybe_unused static n2d_error_t n2d_hash_map_print(n2d_hash_map_t *hash_map)
{
	n2d_error_t error = N2D_SUCCESS;
	n2d_int32_t i	  = 0;

	for (i = 0; i < hash_map->size; i++) {
		n2d_hash_node_t *node = hash_map->hash_array[i];

		n2d_kernel_os_print("array[%d] (key data):\n", i);
		while (node != N2D_NULL) {
			n2d_kernel_os_print("(%d %ld %ld) ", node->key, node->data1, node->data2);
			node = node->next;
		}
		n2d_kernel_os_print("\n");
	}

	return error;
}
