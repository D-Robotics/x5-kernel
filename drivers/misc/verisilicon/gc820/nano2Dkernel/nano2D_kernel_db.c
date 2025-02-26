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

#include "nano2D_kernel_db.h"
#include "nano2D_kernel_common.h"

static n2d_error_t _db_list_destroy(n2d_kernel_t *kernel, n2d_db_process_node_t *db_list)
{
	n2d_error_t error = N2D_SUCCESS;
	n2d_db_process_node_t *next;
	n2d_db_process_node_t *cur = db_list;

	while (cur) {
		next = cur->next;
		if (cur->hash_map)
			n2d_hash_map_destroy(cur->hash_map);
		n2d_kernel_os_free(kernel->os, cur);
		cur = next;
	};

	return error;
}

static n2d_error_t _db_list_find_process_hash_map(n2d_db_process_node_t *db_list,
						  n2d_uint32_t process, n2d_hash_map_t **hash_map,
						  n2d_db_process_node_t **process_node)
{
	n2d_db_process_node_t *next, *cur = db_list;
	n2d_bool_t found = N2D_FALSE;

	if (db_list == N2D_NULL)
		return N2D_NOT_FOUND;

	while (cur) {
		next = cur->next;
		if (cur->process == process) {
			if (hash_map)
				*hash_map = cur->hash_map;
			if (process_node)
				*process_node = cur;
			found = N2D_TRUE;

			break;
		}
		cur = next;
	};

	if (found)
		return N2D_SUCCESS;
	else
		return N2D_NOT_FOUND;
}

n2d_error_t n2d_kernel_db_get_handle(n2d_db_t *db, n2d_uint32_t *handle)
{
	n2d_error_t error    = N2D_SUCCESS;
	n2d_bool_t get_mutex = N2D_FALSE;

	ONERROR(n2d_kernel_os_mutex_acquire(db->kernel->os, db->mutex, N2D_INFINITE));
	get_mutex = N2D_TRUE;

	ONERROR(n2d_handle_get(db->handle_bitmap, handle));

on_error:
	if (get_mutex)
		n2d_kernel_os_mutex_release(db->kernel->os, db->mutex);
	return error;
}

n2d_error_t n2d_kernel_db_free_handle(n2d_db_t *db, n2d_uint32_t handle)
{
	n2d_error_t error    = N2D_SUCCESS;
	n2d_bool_t get_mutex = N2D_FALSE;

	ONERROR(n2d_kernel_os_mutex_acquire(db->kernel->os, db->mutex, N2D_INFINITE));
	get_mutex = N2D_TRUE;

	ONERROR(n2d_handle_free(db->handle_bitmap, handle));

on_error:
	if (get_mutex)
		n2d_kernel_os_mutex_release(db->kernel->os, db->mutex);
	return error;
}

n2d_error_t n2d_db_get_process_allocate_count(n2d_db_t *db, n2d_uint32_t process,
					      n2d_int32_t *count)
{
	n2d_error_t error		    = N2D_SUCCESS;
	n2d_hash_map_t *hash_map	    = N2D_NULL;
	n2d_db_process_node_t *process_node = N2D_NULL;
	n2d_bool_t get_mutex		    = N2D_FALSE;

	ONERROR(n2d_kernel_os_mutex_acquire(db->kernel->os, db->mutex, N2D_INFINITE));
	get_mutex = N2D_TRUE;

	error = _db_list_find_process_hash_map(db->db_list, process, &hash_map, &process_node);
	if (error != N2D_SUCCESS)
		ONERROR(N2D_NOT_FOUND);
	if (count)
		*count = process_node->allocate_count;

on_error:
	if (get_mutex)
		n2d_kernel_os_mutex_release(db->kernel->os, db->mutex);
	return error;
}

n2d_error_t n2d_kernel_db_insert(n2d_db_t *db, n2d_uint32_t process, n2d_uint32_t handle,
				 n2d_db_type_t type, n2d_uintptr_t data)
{
	n2d_error_t error		    = N2D_SUCCESS;
	n2d_hash_map_t *hash_map	    = N2D_NULL;
	n2d_db_process_node_t *process_node = N2D_NULL;
	n2d_bool_t get_mutex		    = N2D_FALSE;

	ONERROR(n2d_kernel_os_mutex_acquire(db->kernel->os, db->mutex, N2D_INFINITE));
	get_mutex = N2D_TRUE;

	error = _db_list_find_process_hash_map(db->db_list, process, &hash_map, &process_node);
	if (error != N2D_SUCCESS)
		ONERROR(N2D_NOT_FOUND);

	ONERROR(n2d_hash_map_insert(hash_map, handle, (n2d_uintptr_t)type, data));
	if (type == N2D_VIDMEM_TYPE)
		process_node->allocate_count++;

on_error:
	if (get_mutex)
		n2d_kernel_os_mutex_release(db->kernel->os, db->mutex);

	return error;
}

n2d_error_t n2d_kernel_db_get(n2d_db_t *db, n2d_uint32_t process, n2d_uint32_t handle,
			      n2d_db_type_t *type, n2d_uintptr_t *data)
{
	n2d_error_t error	 = N2D_SUCCESS;
	n2d_hash_map_t *hash_map = N2D_NULL;
	n2d_uintptr_t data1	 = 0;
	n2d_bool_t get_mutex	 = N2D_FALSE;

	ONERROR(n2d_kernel_os_mutex_acquire(db->kernel->os, db->mutex, N2D_INFINITE));
	get_mutex = N2D_TRUE;

	error = _db_list_find_process_hash_map(db->db_list, process, &hash_map, N2D_NULL);
	if (error != N2D_SUCCESS)
		ONERROR(N2D_NOT_FOUND);
	ONERROR(n2d_hash_map_get(hash_map, handle, &data1, data));
	if (type)
		*type = (n2d_uint32_t)data1;

on_error:
	if (get_mutex)
		n2d_kernel_os_mutex_release(db->kernel->os, db->mutex);
	return error;
}

n2d_error_t n2d_kernel_db_remove(n2d_db_t *db, n2d_uint32_t process, n2d_uint32_t handle)
{
	n2d_error_t error		    = N2D_SUCCESS;
	n2d_hash_map_t *hash_map	    = N2D_NULL;
	n2d_db_process_node_t *process_node = N2D_NULL;
	n2d_uintptr_t data1, data2;
	n2d_db_type_t _type  = N2D_UNKONW_TYPE;
	n2d_bool_t get_mutex = N2D_FALSE;

	ONERROR(n2d_kernel_os_mutex_acquire(db->kernel->os, db->mutex, N2D_INFINITE));
	get_mutex = N2D_TRUE;

	error = _db_list_find_process_hash_map(db->db_list, process, &hash_map, &process_node);
	if (error != N2D_SUCCESS)
		ONERROR(N2D_NOT_FOUND);
	ONERROR(n2d_hash_map_get_remove(hash_map, handle, &data1, &data2));
	_type = (n2d_db_type_t)data1;
	if (_type == N2D_VIDMEM_TYPE)
		process_node->allocate_count--;

on_error:
	if (get_mutex)
		n2d_kernel_os_mutex_release(db->kernel->os, db->mutex);
	return error;
}

n2d_error_t n2d_kernel_db_get_remove(n2d_db_t *db, n2d_uint32_t process, n2d_uint32_t handle,
				     n2d_db_type_t *type, n2d_uintptr_t *data)
{
	n2d_error_t error		    = N2D_SUCCESS;
	n2d_hash_map_t *hash_map	    = N2D_NULL;
	n2d_db_process_node_t *process_node = N2D_NULL;
	n2d_uintptr_t data1, data2;
	n2d_db_type_t _type  = N2D_UNKONW_TYPE;
	n2d_bool_t get_mutex = N2D_FALSE;

	ONERROR(n2d_kernel_os_mutex_acquire(db->kernel->os, db->mutex, N2D_INFINITE));
	get_mutex = N2D_TRUE;

	error = _db_list_find_process_hash_map(db->db_list, process, &hash_map, &process_node);
	if (error != N2D_SUCCESS)
		ONERROR(N2D_NOT_FOUND);
	ONERROR(n2d_hash_map_get_remove(hash_map, handle, &data1, &data2));
	_type = (n2d_db_type_t)data1;
	if (_type == N2D_VIDMEM_TYPE)
		process_node->allocate_count--;
	if (type)
		*type = _type;
	if (data)
		*data = data2;

on_error:
	if (get_mutex)
		n2d_kernel_os_mutex_release(db->kernel->os, db->mutex);
	return error;
}

n2d_error_t n2d_kernel_db_attach_process(n2d_db_t *db, n2d_uint32_t process, n2d_bool_t flag)
{
	n2d_error_t error   = N2D_SUCCESS;
	n2d_pointer pointer = N2D_NULL;
	n2d_db_process_node_t *process_node;
	n2d_bool_t get_mutex = N2D_FALSE;
	n2d_os_t *os	     = db->kernel->os;

	if (!db)
		ONERROR(N2D_INVALID_ARGUMENT);

	ONERROR(n2d_kernel_os_mutex_acquire(os, db->mutex, N2D_INFINITE));
	get_mutex = N2D_TRUE;

	if (flag) {
		error = _db_list_find_process_hash_map(db->db_list, process, N2D_NULL, N2D_NULL);
		if (error == N2D_NOT_FOUND) {
			ONERROR(n2d_kernel_os_allocate(os, sizeof(n2d_db_process_node_t),
						       &pointer));
			if (pointer) {
				process_node		     = (n2d_db_process_node_t *)pointer;
				process_node->process	     = process;
				process_node->allocate_count = 0;
				process_node->hash_map	     = N2D_NULL;
				ONERROR(n2d_hash_map_construct(db->kernel, 32,
							       &process_node->hash_map));
				if (!process_node->hash_map) {
					ONERROR(n2d_kernel_os_free(os, process_node));
					ONERROR(N2D_OUT_OF_MEMORY);
				}

				process_node->next = db->db_list;
				db->db_list	   = process_node;
			}
		}
	} else {
		n2d_db_process_node_t *prev = N2D_NULL;
		n2d_db_process_node_t *cur  = db->db_list;

		while (cur) {
			if (cur->process == process && cur->hash_map) {
				if (cur->allocate_count > 0)
					n2d_kernel_os_print("Warnning: memory leak(%d allocated "
							    "memory need to be freed)\n",
							    cur->allocate_count);
				ONERROR(n2d_hash_map_destroy(cur->hash_map));
				if (cur == db->db_list) {
					db->db_list = cur->next;
				} else {
					prev->next = cur->next;
				}
				n2d_kernel_os_free(os, cur);
				break;
			}
			prev = cur;
			cur  = cur->next;
		};
	}

	n2d_kernel_os_mutex_release(os, db->mutex);

	return error;
on_error:
	if (pointer)
		n2d_kernel_os_free(os, pointer);
	if (get_mutex)
		n2d_kernel_os_mutex_release(os, db->mutex);
	return error;
}

n2d_error_t n2d_kernel_db_construct(n2d_kernel_t *kernel, n2d_db_t **db)
{
	n2d_error_t error   = N2D_SUCCESS;
	n2d_pointer pointer = N2D_NULL;
	n2d_db_t *_db	    = N2D_NULL;

	ONERROR(n2d_kernel_os_allocate(kernel->os, sizeof(n2d_db_t), &pointer));
	ONERROR(n2d_kernel_os_memory_fill(kernel->os, pointer, 0, sizeof(n2d_db_t)));

	_db	     = (n2d_db_t *)pointer;
	_db->kernel  = kernel;
	_db->db_list = N2D_NULL;

	ONERROR(n2d_handle_construct(kernel, &_db->handle_bitmap));
	if (!_db->handle_bitmap)
		ONERROR(N2D_OUT_OF_MEMORY);
	ONERROR(n2d_kernel_os_mutex_create(kernel->os, &_db->mutex));

	/* attach process 0 as basic database */
	ONERROR(n2d_kernel_db_attach_process(_db, 0, N2D_TRUE));

	*db = _db;

	return error;
on_error:
	if (_db) {
		if (_db->handle_bitmap)
			n2d_handle_destroy(_db->handle_bitmap);

		n2d_kernel_os_free(kernel->os, _db);
	}
	return error;
}

n2d_error_t n2d_kernel_db_destroy(n2d_kernel_t *kernel, n2d_db_t *db)
{
	n2d_kernel_db_attach_process(db, 0, N2D_FALSE);

	if (db->handle_bitmap)
		n2d_handle_destroy(db->handle_bitmap);

	if (db->db_list)
		_db_list_destroy(kernel, db->db_list);

	if (db->mutex)
		n2d_kernel_os_mutex_delete(kernel->os, db->mutex);

	if (db)
		n2d_kernel_os_free(kernel->os, db);

	return N2D_SUCCESS;
}


/* n2d_kernel_db_list_all_handles
 * Enumerate all allocated handles for 'process' in DB. Output up to 'max_count' handles into 'handles'.
 * '*handle_count' will be assigned the actual enumerated number.
 */
n2d_error_t n2d_kernel_db_list_all_handles(n2d_db_t *db,
											n2d_uint32_t process,
											n2d_uint32_t *handles,
											n2d_uint32_t *handle_count,
											n2d_uint32_t max_count)
{
	n2d_error_t error = N2D_SUCCESS;
	n2d_hash_map_t *hash_map = N2D_NULL;
	n2d_db_process_node_t *process_node = N2D_NULL;
	n2d_bool_t get_mutex = N2D_FALSE;

	if (!db || !handles || !handle_count)
		return N2D_INVALID_ARGUMENT;

	/* Lock DB. */
	error = n2d_kernel_os_mutex_acquire(db->kernel->os, db->mutex, N2D_INFINITE);
	if (error != N2D_SUCCESS)
		return error;
	get_mutex = N2D_TRUE;

	/* Find the process node and its hash_map. */
	error = _db_list_find_process_hash_map(db->db_list, process, &hash_map, &process_node);
	if (error != N2D_SUCCESS) {
		/* If not found, we can say 0 handle or return error directly. */
		error = N2D_NOT_FOUND;
		goto on_error;
	}

	/* Enumerate all keys (handles) in the process hash_map. */
	error = n2d_hash_map_iterate_keys(hash_map, handles, handle_count, max_count);

on_error:
	if (get_mutex)
		n2d_kernel_os_mutex_release(db->kernel->os, db->mutex);
	return error;
}
