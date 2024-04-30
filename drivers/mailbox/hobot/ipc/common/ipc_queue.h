/*************************************************************************
 *                     COPYRIGHT NOTICE
 *            Copyright 2021-2023 Horizon Robotics, Inc.
 *                   All rights reserved.
 *************************************************************************/

/**
 * @file ipc_queue.h
 *
 * @NO{S17E09C06U}
 * @ASIL{B}
 */

#ifndef IPC_QUEUE_H
#define IPC_QUEUE_H

/**
 * @struct ipc_ring
 * Define the descriptor of memory mapped circular buffer ring.
 * @NO{S17E09C06}
 */
struct ipc_ring {
	volatile uint32_t write;/**< write index, position used to store next byte in the buffer*/
	volatile uint32_t read;/**< read index, read next byte from this position*/
	uint8_t data[];/**< circular buffer*/
};

/**
 * @struct ipc_queue
 * Define the descriptor of Dual-Ring Shared-Memory Lock-Free FIFO Queue.
 * @NO{S17E09C06}
 */
struct ipc_queue {
	uint16_t elem_num;/**< number of elements in queue*/
	uint16_t elem_size;/**<  element size in bytes (8-byte multiple)*/
	struct ipc_ring *push_ring;/**< push buffer ring mapped in local shared memory*/
	struct ipc_ring *pop_ring;/**< pop buffer ring mapped in remote shared memory*/
};

int32_t ipc_queue_init(struct ipc_queue *queue, uint16_t elem_num,
	uint16_t elem_size, uint64_t push_ring_addr, uint64_t pop_ring_addr);
int32_t ipc_queue_push(struct ipc_queue *queue, const void *buf);
int32_t ipc_queue_pop(struct ipc_queue *queue, void *buf);

/**
 * @NO{S17E09C06U}
 * @ASIL{B}
 * @brief queue footprint in local mapped memory
 *
 * @param[in] queue: queue pointer
 *
 * @return size of local mapped memory occupied by queue
 *
 * @callgraph
 * @callergraph
 * @design
 */
static inline uint32_t ipc_queue_mem_size(struct ipc_queue *queue)
{
	/* local ring control room + ring size */
	return (uint32_t)sizeof(struct ipc_ring)
		+ ((uint32_t)queue->elem_num * (uint32_t)queue->elem_size);
}

#endif /* IPC_QUEUE_H */
