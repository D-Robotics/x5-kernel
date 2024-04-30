/*************************************************************************
 *                     COPYRIGHT NOTICE
 *            Copyright 2021-2023 Horizon Robotics, Inc.
 *                   All rights reserved.
 *************************************************************************/

/**
 * @file ipc_queue.c
 *
 * @NO{S17E09C06U}
 * @ASIL{B}
 */

#include "ipc_os.h"

/**
 * @NO{S17E09C06U}
 * @ASIL{B}
 * @brief removes element from queue
 *
 * @param[in] queue: queue pointer
 * @param[in] buf: pointer where to copy the removed element
 *
 * @retval "0": success
 * @retval "!0": failure
 *
 * @callgraph
 * @callergraph
 * @design
 */
int32_t ipc_queue_pop(struct ipc_queue *queue, void *buf)
{
	uint32_t write; /* cache write index for thread-safety */
	uint32_t read; /* cache read index for thread-safety */
	void *src;

	if ((queue == NULL) || (buf == NULL)) {
		ipc_err("invalid parameter\n");

		return -EINVAL;
	}

	write = queue->pop_ring->write;

	/* read indexes of push/pop rings are swapped (interference freedom) */
	read = queue->push_ring->read;
	ipc_dbg("%s write(%p) %d, read(%p) %d\n", __FUNCTION__, &queue->pop_ring->write, write, &queue->push_ring->read, read);

	/* check if queue is empty */
	if (read == write) {
		ipc_dbg("no empty buffer: read %d, write %d\n", read, write);

		return -ENOBUFS;
	}

	/* copy queue element in buffer */
	src = &queue->pop_ring->data[read * queue->elem_size];
	memcpy(buf, src, queue->elem_size);

	/* increment read index with wrap around */
	queue->push_ring->read = (read + 1u) % queue->elem_num;
	ipc_dbg("%s read %p = %d", __FUNCTION__, &queue->push_ring->read, queue->push_ring->read);
	return 0;
}

/**
 * @NO{S17E09C06U}
 * @ASIL{B}
 * @brief pushes element into the queue
 *
 * @param[in] queue: queue pointer
 * @param[in] buf: pointer to element to be pushed into the queue
 *
 * @retval "0": success
 * @retval "!0": failure
 *
 * @callgraph
 * @callergraph
 * @design
 */
int32_t ipc_queue_push(struct ipc_queue *queue, const void *buf)
{
	uint32_t write; /* cache write index for thread-safety */
	uint32_t read; /* cache read index for thread-safety */
	void *dst;

	if ((queue == NULL) || (buf == NULL)) {
		ipc_err("invalid parameter\n");

		return -EINVAL;
	}

	write = queue->push_ring->write;

	/* read indexes of push/pop rings are swapped (interference freedom) */
	read = queue->pop_ring->read;

	ipc_dbg("%s write(%p) %d, read(%p) %d\n", __FUNCTION__, &queue->push_ring->write, write, &queue->pop_ring->read, read);

	/* check if queue is full ([write + 1 == read] because of sentinel) */
	if (((write + 1u) % queue->elem_num) == read) {
		ipc_dbg("no empty buffer, write %d, read %d\n", write, read);

		return -ENOMEM;
	}

	/* copy element from buffer in queue */
	dst = &queue->push_ring->data[write * queue->elem_size];
	memcpy(dst, buf, queue->elem_size);

	/* increment write index with wrap around */
	queue->push_ring->write = (write + 1u) % queue->elem_num;
	ipc_dbg("%s write %p = %d", __FUNCTION__, &queue->push_ring->write, queue->push_ring->write);
	return 0;
}

/**
 * @NO{S17E09C06U}
 * @ASIL{B}
 * @brief initializes queue and maps push/pop rings in memory
 *
 * @param[in] queue: queue pointer
 * @param[in] elem_num: number of elements in queue
 * @param[in] elem_size: element size in bytes (8-byte multiple)
 * @param[in] push_ring_addr: local addr where to map the push buffer ring
 * @param[in] pop_ring_addr: remote addr where to map the pop buffer ring
 *
 * @retval "0": success
 * @retval "!0": failure
 *
 * @callgraph
 * @callergraph
 * @design
 */
int32_t ipc_queue_init(struct ipc_queue *queue,
		   uint16_t elem_num, uint16_t elem_size,
		   uint64_t push_ring_addr, uint64_t pop_ring_addr)
{
	if ((queue == NULL) || (push_ring_addr == (uint64_t)NULL) ||
		(pop_ring_addr == (uint64_t)NULL) || (elem_num == 0u) ||
		(elem_size == 0u) || ((elem_size % 8u) != 0u)) {
		ipc_err("invalid parameter\n");

		return -EINVAL;
	}

	/* add 1 sentinel element in queue for lock-free thread-safety */
	queue->elem_num = elem_num + 1u;
	queue->elem_size = elem_size;

	/* map and init push ring in local memory */
	queue->push_ring = (struct ipc_ring *) push_ring_addr;
	queue->push_ring->write = 0;
	queue->push_ring->read = 0;

	/* map pop ring in remote memory (init is done by remote) */
	queue->pop_ring = (struct ipc_ring *) pop_ring_addr;

	return 0;
}
