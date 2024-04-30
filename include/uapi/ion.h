/*
 * drivers/staging/android/uapi/ion.h
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
/**
 * @file ion.h
 *
 * @NO{S21E04C01I}
 * @ASIL{B}
 */
#ifndef _UAPI_LINUX_ION_H
#define _UAPI_LINUX_ION_H

#include <linux/ioctl.h>
#include <linux/types.h>

typedef int64_t ion_user_handle_t;	/**< ion user handle id */
typedef int32_t ion_share_handle_t;	/**< ion share handle id */

/**
 * enum ion_heap_types - list of all possible types of heaps
 * @ION_HEAP_TYPE_SYSTEM:	 memory allocated via vmalloc
 * @ION_HEAP_TYPE_SYSTEM_CONTIG: memory allocated via kmalloc
 * @ION_HEAP_TYPE_CARVEOUT:	 memory allocated from a prereserved
 *				 carveout heap, allocations are physically
 *				 contiguous
 * @ION_HEAP_TYPE_DMA:		 memory allocated via DMA API
 * @ION_NUM_HEAPS:		 helper for iterating over heaps, a bit mask
 *				 is used to identify the heaps, so only 32
 *				 total heap types are supported
 */
/**
 * list of all possible types of heaps
 */
/**
 * @enum ion_heap_type
 * @brief Define the descriptor of ion heap type.
 * @NO{S21E04C01I}
 */
enum ion_heap_type {
	ION_HEAP_TYPE_SYSTEM,	/**< memory allocated via vmalloc */
	ION_HEAP_TYPE_SYSTEM_CONTIG,	/**< memory allocated via kmalloc */
	ION_HEAP_TYPE_CARVEOUT,	/**< memory allocated from a prereserved 
							 * carveout heap, allocations are physically
							 * contiguous*/
	ION_HEAP_TYPE_CHUNK,	/**< chunk memory */
	ION_HEAP_TYPE_DMA,		/**< memory allocated via DMA API*/
	ION_HEAP_TYPE_CUSTOM,	/**< memory allocaedf from SRAM heap*/
	ION_HEAP_TYPE_CMA_RESERVED,	/**< memory allocated from a prereserved 
								 * carveout heap, allocations are physically
								 * contiguous*/
	ION_HEAP_TYPE_SRAM_LIMIT,	/**< memory allocated from sram limit heap.*/
	ION_HEAP_TYPE_INLINE_ECC,	/**< memory allocated from inline ecc heap.*/
	ION_HEAP_TYPE_DMA_EX,	/**< memory allocated via DMA API*/
	ION_HEAP_TYPE_MAX,
/*
 * must be last so device specific heaps always
 * are at the end of this enum
 */
};

#define ION_HEAP_SYSTEM_MASK		(1 << ION_HEAP_TYPE_SYSTEM)				/**< system heap mask */
#define ION_HEAP_SYSTEM_CONTIG_MASK	(1 << ION_HEAP_TYPE_SYSTEM_CONTIG)		/**< system contigious heap mask */
#define ION_HEAP_CARVEOUT_MASK		(1 << ION_HEAP_TYPE_CARVEOUT)			/**< carveout heap mask */
#define ION_HEAP_CHUNK_MASK		(1 << ION_HEAP_TYPE_CHUNK)					/**< chunk heap mask */
#define ION_HEAP_TYPE_DMA_MASK		(1 << ION_HEAP_TYPE_DMA)				/**< dma heap mask */
#define ION_HEAP_TYPE_CUSTOM_MASK	(1 << ION_HEAP_TYPE_CUSTOM)				/**< custom heap mask */
#define ION_HEAP_TYPE_CMA_RESERVED_MASK	(1 << ION_HEAP_TYPE_CMA_RESERVED)	/**< cma reserved heap mask */
#define ION_HEAP_TYPE_SRAM_LIMIT_MASK	(1 << ION_HEAP_TYPE_SRAM_LIMIT)	/**< limit sram heap mask.*/
#define ION_HEAP_TYPE_INLINE_ECC_MASK	(1 << ION_HEAP_TYPE_INLINE_ECC)		/**< inline ecc heap mask.*/
#define ION_HEAP_TYPE_DMA_EX_MASK	(1 << ION_HEAP_TYPE_DMA_EX)				/**< dma ex heap mask */

#define ION_NUM_HEAP_IDS		(sizeof(unsigned int) * 8)		/**< ion heap ids number*/


/**
 * allocation flags - the lower 16 bits are used by core ion, the upper 16
 * bits are reserved for use by the heaps themselves.
 */
#define ION_FLAG_CACHED 1		/**< cached flag */
#define ION_FLAG_CACHED_NEEDS_SYNC 2	/**< cached needs sync flag */
#define ION_FLAG_USE_POOL		4		/**< use memory pool flag*/
#define ION_FLAG_INITIALIZED 256	/**< buffer alloc with initialized flag*/

#define ION_FLAG_UNINITIALIZED 512	/**< buffer alloc with uninitialized flag*/

/**
 * DOC: Ion Userspace API
 *
 * create a client by opening /dev/ion
 * most operations handled via following ioctls
 *
 */

/**
 * struct ion_allocation_data - metadata passed from userspace for allocations
 * @len:		size of the allocation
 * @align:		required alignment of the allocation
 * @heap_id_mask:	mask of heap ids to allocate from
 * @flags:		flags passed to heap
 * @handle:		pointer that will be populated with a cookie to use to
 *			refer to this allocation
 *
 * Provided by userspace as an argument to the ioctl
 */
/**
 * metadata passed from userspace for allocations
 */
/**
 * @struct ion_allocation_data
 * @brief metadata passed from userspace for allocations
 * @NO{S21E04C01I}
 */
struct ion_allocation_data {
	size_t len;						/**< size of the allocation */
	size_t align;					/**< required alignment of the allocation */
	unsigned int heap_id_mask;		/**< mask of heap ids to allocate from */
	unsigned int flags;				/**< flags passed to heap */
	ion_user_handle_t handle;		/**< pointer that will be populated with a cookie to use to refer to this allocation */
	ion_share_handle_t sh_handle;	/**< pointer that will be populated with a cookie to use to refer to share buffer */
};



#define MAX_HEAP_NAME			32	/**< the max heap name length */

/**
 * struct ion_heap_data - data about a heap
 * @name - first 32 characters of the heap name
 * @type - heap type
 * @heap_id - heap id for the heap
 */
/**
 * data about a heap
 */
/**
 * @struct ion_heap_data
 * @brief data about a heap
 * @NO{S21E04C01I}
 */
struct ion_heap_data {
	char name[MAX_HEAP_NAME];	/**< first 32 characters of the heap name */
	__u32 type;					/**< heap type */
	__u32 heap_id;				/**< heap id for the heap */
	__u32 reserved0;			/**< reserved */
	__u32 reserved1;			/**< reserved */
	__u32 reserved2;			/**< reserved */
};

/**
 * struct ion_heap_query - collection of data about all heaps
 * @cnt - total number of heaps to be copied
 * @heaps - buffer to copy heap data
 */
/**
 * collection of data about all heaps
 */
/**
 * @struct ion_heap_query
 * @brief collection of data about all heaps
 * @NO{S21E04C01I}
 */
struct ion_heap_query {
	__u32 cnt;			/**< Total number of heaps to be copied */
	__u32 reserved0;	/**< align to 64bits */
	__u64 heaps;		/**< buffer to be populated */
	__u32 reserved1;	/**< reserved */
	__u32 reserved2;	/**< reserved */
};

/**
 * struct ion_fd_data - metadata passed to/from userspace for a handle/fd pair
 * @handle:	a handle
 * @fd:		a file descriptor representing that handle
 *
 * For ION_IOC_SHARE or ION_IOC_MAP userspace populates the handle field with
 * the handle returned from ion alloc, and the kernel returns the file
 * descriptor to share or map in the fd field.  For ION_IOC_IMPORT, userspace
 * provides the file descriptor and the kernel returns the handle.
 */
/**
 * metadata passed to/from userspace for a handle/fd pair
 */
/**
 * @struct ion_fd_data
 * @brief metadata passed to/from userspace for a handle/fd pair
 * @NO{S21E04C01I}
 */
struct ion_fd_data {
	ion_user_handle_t handle;	/**< a handle */
	int fd;						/**< a file descriptor representing that handle */
};

/**
 * struct ion_handle_data - a handle passed to/from the kernel
 * @handle:	a handle
 */
/**
 * a handle passed to/from the kernel
 */
/**
 * @struct ion_handle_data
 * @brief a handle passed to/from the kernel
 * @NO{S21E04C01I}
 */
struct ion_handle_data {
	ion_user_handle_t handle;	/**< a handle */
};

/**
 * @struct ion_share_and_phy_data
 * Define the descriptor of ion share and physical data.
 * @NO{S21E04C01I}
 */
struct ion_share_and_phy_data {
	ion_user_handle_t handle;	/**< ion handle address.*/
 	int fd;						/**< share fd of ion buffer.*/
	phys_addr_t paddr;				/**< buffer physical address.*/
	size_t len;				/**< buffer size.*/
	uint64_t reserved;			/**< reserved.*/
};

/**
 * struct ion_share_handle_data - a handle passed to/from the kernel
 * @handle:	a handle
 */
/**
 * ion handle and share handle passed to/from the kernel
 */
/**
 * @struct ion_share_handle_data
 * @brief ion handle and share handle passed to/from the kernel
 * @NO{S21E04C01I}
 */
struct ion_share_handle_data {
	ion_user_handle_t handle;		/**< ion handle */
	int64_t flags;					/**< memory flags */
	ion_share_handle_t sh_handle;	/**< share handle */
	uint64_t phys_addr;				/**< import start physical address.*/
	uint64_t size;					/**< import buffer size.*/
};
/**
 * struct ion_share_pool_data - a handle passed to/from the kernel
 * @handle:	a handle
 */
/**
 * @struct ion_share_pool_data
 * @brief ion share pool data passed to/from the kernel
 * @NO{S21E04C01I}
 */
struct ion_share_pool_data {
	ion_user_handle_t handle;		/**< ion handle id.*/
	int32_t sh_fd;					/**< share fd of ion share handle.*/
	ion_share_handle_t sh_handle;	/**< ion share handle id.*/
	int32_t import_cnt;				/**< imort count.*/
};

/**
 * struct ion_share_info_data - a handle passed to/from the kernel
 * @handle:	a handle
 */
/**
 * handle passed to/from the kernel and get the client cnt
 */
/**
 * @struct ion_share_info_data
 * @brief handle passed to/from the kernel and get the client cnt
 * @NO{S21E04C01I}
 */
struct ion_share_info_data {
	ion_user_handle_t handle;	/**< ion handle */
	int64_t timeout;			/**< wait time */
	int32_t target_client_cnt;	/**< client target count */
	int32_t cur_client_cnt;		/**< client current count */
};

#define MAX_PROCESS_INFO 16		/**< the max process info number*/
/**
 * @struct ion_share_info_data
 * @brief a handle passed to/from the kernel
 * @NO{S21E04C01I}
 */
struct ion_process_info_data {
	int32_t share_id;		/**< share handle id*/
	int32_t num;			/**< the process number of the buffer*/
	int32_t pid[MAX_PROCESS_INFO];	/**< the process id of the buffer*/
};

/**
 * @struct ion_version_info_data
 * Define the descriptor of version info data.
 * @NO{S21E04C02I}
 */
struct ion_version_info_data {
	uint32_t major;		/**< the major version number*/
	uint32_t minor;		/**< the minor version number.*/
};

/**
 * struct ion_custom_data - metadata passed to/from userspace for a custom ioctl
 * @cmd:	the custom ioctl function to call
 * @arg:	additional data to pass to the custom ioctl, typically a user
 *		pointer to a predefined structure
 *
 * This works just like the regular cmd and arg fields of an ioctl.
 */
/**
 * metadata passed to/from userspace for a custom ioctl
 */
/**
 * @struct ion_custom_data
 * @brief metadata passed to/from userspace for a custom ioctl
 * @NO{S21E04C01I}
 */
struct ion_custom_data {
	unsigned int cmd;	/**< the custom ioctl function to call command */
	unsigned long arg;	/**< additional data to pass to the custom ioctl, typically a user pointer to a predefined structure */
};

/**
 * @struct ion_consume_info_data
 * @brief metadata passed to/from userspace for a custom ioctl
 * @NO{S21E04C01I}
 */
struct ion_consume_info_data {
	ion_user_handle_t handle;		/**< ion handle id.*/
	int64_t timeout;				/**< wait time.*/
	int32_t target_consume_cnt;		/**< target consume count.*/
	int32_t cur_consume_cnt;		/**< the current consume count.*/
};

#define ION_CUSTOM_OPEN (4)			/**< the open custom cmd.*/
#define ION_CUSTOM_RELEASE (5)		/**< the release custom cmd.*/
#define ION_SHARE_FD_RELEASE (6)	/**< the share fd release cmd.*/
#define ION_CHECK_IN_HEAP (7)		/**< the check whether alloc from ION custom cmd.*/

#define ION_IOC_MAGIC		'I'		/**< ion driver command magic */

/**
 * DOC: ION_IOC_ALLOC - allocate memory
 *
 * Takes an ion_allocation_data struct and returns it with the handle field
 * populated with the opaque handle for the allocation.
 */
/**
 * @def ION_IOC_ALLOC
 * ion driber alloc buffer command
 */
#define ION_IOC_ALLOC		_IOWR(ION_IOC_MAGIC, 0, \
				      struct ion_allocation_data)

/**
 * DOC: ION_IOC_FREE - free memory
 *
 * Takes an ion_handle_data struct and frees the handle.
 */
#define ION_IOC_FREE		_IOWR(ION_IOC_MAGIC, 1, struct ion_handle_data)	/**< ion driver buffer free command */

/**
 * DOC: ION_IOC_MAP - get a file descriptor to mmap
 *
 * Takes an ion_fd_data struct with the handle field populated with a valid
 * opaque handle.  Returns the struct with the fd field set to a file
 * descriptor open in the current address space.  This file descriptor
 * can then be used as an argument to mmap.
 */
#define ION_IOC_MAP		_IOWR(ION_IOC_MAGIC, 2, struct ion_fd_data)	/**< ion driver map fd command */

/**
 * DOC: ION_IOC_SHARE - creates a file descriptor to use to share an allocation
 *
 * Takes an ion_fd_data struct with the handle field populated with a valid
 * opaque handle.  Returns the struct with the fd field set to a file
 * descriptor open in the current address space.  This file descriptor
 * can then be passed to another process.  The corresponding opaque handle can
 * be retrieved via ION_IOC_IMPORT.
 */
#define ION_IOC_SHARE		_IOWR(ION_IOC_MAGIC, 4, struct ion_fd_data)	/**< ion driver relates buffer with share fd command */

/**
 * DOC: ION_IOC_IMPORT - imports a shared file descriptor
 *
 * Takes an ion_fd_data struct with the fd field populated with a valid file
 * descriptor obtained from ION_IOC_SHARE and returns the struct with the handle
 * filed set to the corresponding opaque handle.
 */
#define ION_IOC_IMPORT		_IOWR(ION_IOC_MAGIC, 5, struct ion_fd_data)	/**< ion driver buffer import command */

/**
 * DOC: ION_IOC_CUSTOM - call architecture specific ion ioctl
 *
 * Takes the argument of the architecture specific ioctl to call and
 * passes appropriate userdata for that ioctl
 */
#define ION_IOC_CUSTOM		_IOWR(ION_IOC_MAGIC, 6, struct ion_custom_data)	/**< ion driver custom command */

/**
 * DOC: ION_IOC_SYNC - syncs a shared file descriptors to memory
 *
 * Deprecated in favor of using the dma_buf api's correctly (syncing
 * will happend automatically when the buffer is mapped to a device).
 * If necessary should be used after touching a cached buffer from the cpu,
 * this will make the buffer in memory coherent.
 */
#define ION_IOC_SYNC		_IOWR(ION_IOC_MAGIC, 7, struct ion_fd_data)	/**< ion driver buffer sync command */

/**
 * DOC: ION_IOC_HEAP_QUERY - information about available heaps
 *
 * Takes an ion_heap_query structure and populates information about
 * available Ion heaps.
 */
/**
 * @def ION_IOC_HEAP_QUERY
 * ion driber heap query command
 */
#define ION_IOC_HEAP_QUERY     _IOWR(ION_IOC_MAGIC, 8, \
					struct ion_heap_query)

#define ION_IOC_IMPORT_SHARE_ID  _IOWR(ION_IOC_MAGIC, 9, struct ion_share_handle_data)		/**< ion driver buffer import with share id command */

#define ION_IOC_GET_SHARE_INFO  _IOWR(ION_IOC_MAGIC, 10, struct ion_share_info_data)		/**< ion driver get share information command */

#define ION_IOC_WAIT_SHARE_ID  _IOWR(ION_IOC_MAGIC, 11, struct ion_share_info_data)			/**< ion driver wait for client ref count command */

#define ION_IOC_SHARE_POOL_REGISTER  _IOWR(ION_IOC_MAGIC, 12, struct ion_share_pool_data)	/**< ion driver register the share pool buffer.*/

#define ION_IOC_SHARE_POOL_UNREGISTER  _IOWR(ION_IOC_MAGIC, 13, struct ion_share_pool_data)	/**< ion driver unregister the share pool buffer.*/

#define ION_IOC_SHARE_POOL_GET_REF_CNT  _IOWR(ION_IOC_MAGIC, 14, struct ion_share_pool_data) /**< ion driver get the share pool buffer reference count.*/

#define ION_IOC_SHARE_POOL_MONITOR_REF_CNT  _IOWR(ION_IOC_MAGIC, 15, struct ion_share_pool_data)	/**< ion driver get the monitor reference count.*/

#define ION_IOC_SHARE_POOL_WAKE_UP_MONITOR  _IOWR(ION_IOC_MAGIC, 16, struct ion_share_pool_data)	/**< ion driver wake up the monitor thread.*/

#define ION_IOC_GET_BUFFER_PROCESS_INFO  _IOWR(ION_IOC_MAGIC, 30, struct ion_process_info_data)		/**< ion driver get process info of the buffer.*/

#define ION_IOC_INC_CONSUME_CNT  _IOWR(ION_IOC_MAGIC, 31, struct ion_share_handle_data)		/**< ion dirver increase the consume count command.*/

#define ION_IOC_DEC_CONSUME_CNT  _IOWR(ION_IOC_MAGIC, 32, struct ion_share_handle_data)		/**< ion driver decrease the consume count command*/

#define ION_IOC_GET_CONSUME_INFO  _IOWR(ION_IOC_MAGIC, 33, struct ion_consume_info_data)	/**< ion driver get the consume count command.*/

#define ION_IOC_WAIT_CONSUME_STATUS  _IOWR(ION_IOC_MAGIC, 34, struct ion_consume_info_data)	/**< ion driver wait for the consume count command.*/

#define ION_IOC_GET_VERSION_INFO  _IOWR(ION_IOC_MAGIC, 35, struct ion_version_info_data)	/**< ion driver get driver version info.*/

#ifdef CONFIG_PCIE_HOBOT_EP_AI
#define ION_IOC_OPEN  _IO(ION_IOC_MAGIC, 17)		/**< ion driver device open command */

#define ION_IOC_FD_RELEASE  _IO(ION_IOC_MAGIC, 18)	/**< ion driver share fd release command*/

#define ION_IOC_RELEASE  _IO(ION_IOC_MAGIC, 19)		/**< ion driver device relase command*/

#define ION_IOC_MAP_KERNEL _IO(ION_IOC_MAGIC, 20)	/**< ion driver map kernel virtual address command */

#define ION_IOC_UNMAP_KERNEL _IO(ION_IOC_MAGIC, 21)	/**< ion driver unmap kernel virtual address commad */
#endif

#define ION_IOC_SHARE_AND_PHY  _IOWR(ION_IOC_MAGIC, 22, struct ion_share_and_phy_data)	/**< ion driver relates buffer with share fd and get physical address command*/

#endif /* _UAPI_LINUX_ION_H */
