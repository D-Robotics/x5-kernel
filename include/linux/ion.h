/*
 * drivers/staging/android/ion/ion.h
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
#ifndef _ION_H
#define _ION_H

#include <linux/device.h>
#include <linux/dma-direction.h>
#include <linux/kref.h>
#include <linux/mm_types.h>
#include <linux/mutex.h>
#include <linux/rbtree.h>
#include <linux/sched.h>
#include <linux/shrinker.h>
#include <linux/types.h>
#include <linux/miscdevice.h>
#include <linux/kfifo.h>

#include <linux/dma-buf.h>
#include <linux/iommu.h>
#if IS_ENABLED(CONFIG_HOBOT_IOMMU)
#include "../smmu/hobot_iovmm.h"
#endif

#include <uapi/ion.h>

/**
 * @enum mem_pixel_format_t
 * @brief Define the buffer format of graphic buffer.
 * @NO{S21E04C01I}
 */
enum mem_pixel_format_t {
	MEM_PIX_FMT_NONE = -1,	/**< none number.*/

	MEM_PIX_FMT_RGB565,		/**< packed RGB 5:6:5, 16bpp,*/
	MEM_PIX_FMT_RGB24,		/**< packed RGB 8:8:8, 24bpp, RGBRGB...*/
	MEM_PIX_FMT_BGR24,		/**< packed RGB 8:8:8, 24bpp, BGRBGR...*/
	MEM_PIX_FMT_ARGB,		/**< packed ARGB 8:8:8:8, 32bpp, ARGBARGB...*/
	MEM_PIX_FMT_RGBA,		/**< packed RGBA 8:8:8:8, 32bpp, RGBARGBA...*/
	MEM_PIX_FMT_ABGR,		/**< packed ABGR 8:8:8:8, 32bpp, ABGRABGR...*/
	MEM_PIX_FMT_BGRA,		/**< packed BGRA 8:8:8:8, 32bpp, BGRABGRA...*/
	MEM_PIX_FMT_YUV420P,	/**< planar YUV 4:2:0, 12bpp, 1 plane for Y and
							 * 1 plane for the UV components, which are
							 * interleaved (first byte U and the following
							 * byte V).*/
	MEM_PIX_FMT_NV12,		/**< as above, but U and V bytes are swapped */
	MEM_PIX_FMT_NV21,		/**< planar YUV 4:2:2, 16bpp, (1 Cr & Cb sample per 2x1 Y samples) */
	MEM_PIX_FMT_YUV422P,	/**< interleaved chroma (first byte U and the following byte V)
							 * YUV 4:2:2, 16bpp, (1 Cr & Cb sample per 2x1 Y samples)
							 */
	MEM_PIX_FMT_NV16,		/**< * interleaved chroma (first byte V and the following byte U)
							 * YUV 4:2:2, 16bpp, (1 Cr & Cb sample per 2x1 Y samples)
							 */
	MEM_PIX_FMT_NV61,		/**< packed YUV 4:2:2, 16bpp, Y0 Cb Y1 Cr */
	MEM_PIX_FMT_YUYV422,	/**< packed YUV 4:2:2, 16bpp, Y0 Cr Y1 Cb */
	MEM_PIX_FMT_YVYU422,	/**< packed YUV 4:2:2, 16bpp, Cb Y0 Cr Y1 */
	MEM_PIX_FMT_UYVY422,	/**< packed YUV 4:2:2, 16bpp, Cr Y0 Cb Y1 */
	MEM_PIX_FMT_VYUY422,	/**< packed YUV 4:4:4, 24bpp, (1 Cr & Cb sample per 1x1 Y samples) */
	MEM_PIX_FMT_YUV444,		/**< planar YUV 4:4:4, 24bpp, (1 Cr & Cb sample per 1x1 Y samples) */
	MEM_PIX_FMT_YUV444P,	/**< */
	MEM_PIX_FMT_NV24,		/**< */
	MEM_PIX_FMT_NV42,		/**< */
	MEM_PIX_FMT_YUV440P,	/**< planar YUV 4:4:0 (1 Cr & Cb sample per 1x2 Y samples) */
	MEM_PIX_FMT_YUV400,		/**< Gray Y, YUV 4:0:0 */
	MEM_PIX_FMT_RAW8,		/**< raw8 format*/
	MEM_PIX_FMT_RAW10,		/**< raw10 format*/
	MEM_PIX_FMT_RAW12,		/**< raw12 format*/
	MEM_PIX_FMT_RAW14,		/**< raw14 format*/
	MEM_PIX_FMT_RAW16,		/**< raw16 format*/
	MEM_PIX_FMT_RAW20,		/**< raw20 format*/
	MEM_PIX_FMT_RAW24,		/**< raw24 format*/
	MEM_PIX_FMT_TOTAL,		/**< the number of pix format*/
};

/**
 * @enum mem_usage_t
 * @brief Define the usage of buffer. You can combine different usages as a final flags. Undefined usage will be ignored.
 * @NO{S21E04C01I}
 */
enum mem_usage_t {
	HB_MEM_USAGE_CPU_READ_NEVER          = 0x00000000LL,	/**< buffer is never read by cpu */
	HB_MEM_USAGE_CPU_READ_OFTEN          = 0x00000001LL,	/**< buffer is often read by cpu */
	HB_MEM_USAGE_CPU_READ_MASK           = 0x0000000FLL,	/**< mask for the software read and write flag */

	HB_MEM_USAGE_CPU_WRITE_NEVER         = 0x00000000LL,	/**< buffer is never write by cpu */
	HB_MEM_USAGE_CPU_WRITE_OFTEN         = 0x00000010LL,	/**< buffer is often write by cpu, on arm platform *WRITE implies *READ*/
	HB_MEM_USAGE_CPU_WRITE_MASK          = 0x000000F0LL,	/**< mask for the software read and write flag */

	HB_MEM_USAGE_HW_CIM                  = 0x00000100LL,	/**< buffer will be used as camera interface module buffer */
	HB_MEM_USAGE_HW_PYRAMID              = 0x00000200ULL,	/**< buffer will be used as pyramid buffer */
	HB_MEM_USAGE_HW_GDC                  = 0x00000400LL,	/**< buffer will be used as geometric distortion correction buffer */
	HB_MEM_USAGE_HW_STITCH               = 0x00000800LL,	/**< buffer will be used as stitch buffer */
	HB_MEM_USAGE_HW_OPTICAL_FLOW         = 0x00001000ULL,	/**< buffer will be used as optical flow buffer */
	HB_MEM_USAGE_HW_BPU                  = 0x00002000LL,	/**< buffer will be used as bpu buffer */
	HB_MEM_USAGE_HW_ISP                  = 0x00004000LL,	/**< buffer will be used as isp buffer */
	HB_MEM_USAGE_HW_DISPLAY              = 0x00008000LL,	/**< buffer will be used as display buffer */
	HB_MEM_USAGE_HW_VIDEO_CODEC          = 0x00010000LL,	/**< buffer will be used as video codec buffer */
	HB_MEM_USAGE_HW_JPEG_CODEC           = 0x00020000LL,	/**< buffer will be used as jpeg codec buffer */
	HB_MEM_USAGE_HW_VDSP                 = 0x00040000LL,	/** buffer will be used as vdsp buffer */
	HB_MEM_USAGE_HW_GDC_OUT              = 0x00080000LL,	/**< buffer will be used as geometric distortion correction out buffer */
	HB_MEM_USAGE_HW_IPC                  = 0x00100000LL,	/**< buffer will be used as ipc buffer.*/
	HB_MEM_USAGE_HW_PCIE                 = 0x00200000LL,	/**< buffer will be used as pcie buffer.*/
	HB_MEM_USAGE_HW_YNR                  = 0x00400000LL,	/**< buffer will be used as ynr buffer.*/
	HB_MEM_USAGE_HW_MASK                 = 0x00FFFF00LL,	/**< mask for the hw flag */

	HB_MEM_USAGE_MAP_INITIALIZED         = 0x01000000LL,	/**< buffer will be initialized */
	HB_MEM_USAGE_MAP_UNINITIALIZED       = 0x02000000LL,	/**< buffer will not be initialized */
	HB_MEM_USAGE_CACHED                  = 0x04000000LL,	/**< buffer will not be initialized, unchangable */
	HB_MEM_USAGE_GRAPHIC_CONTIGUOUS_BUF  = 0x08000000LL,	/**< graphic buffer will be contiguous */
	HB_MEM_USAGE_MEM_POOL				 = 0x10000000LL,	/**< It only indicates the buffer is a memory pool. Don't use this flag */
	HB_MEM_USAGE_MEM_SHARE_POOL			 = 0x20000000LL,	/* It only indicates the buffer is a share pool. Don't use this flag
															 * to allocate buffer. It's useless.*/
	HB_MEM_USAGE_TRIVIAL_MASK            = 0xFF000000LL,	/**< mask for the trivial flag.*/

	HB_MEM_USAGE_PRIV_HEAP_DMA           = 0x000000000LL,	/**< buffer will be allocated from CMA heap in linux system */
	HB_MEM_USAGE_PRIV_HEAP_RESERVERD     = 0x100000000LL,	/**< buffer will be allocated from Carveout heap in linux system */
	HB_MEM_USAGE_PRIV_HEAP_RESERVED      = 0x100000000LL,	/**< buffer will be allocated from Carveout heap in linux system */
	HB_MEM_USAGE_PRIV_HEAP_SRAM          = 0x200000000LL,	/**< buffer will be allocated from SRAM heap in linux system */
	HB_MEM_USAGE_PRIV_HEAP_2_RESERVERD   = 0x400000000LL,	/**< buffer will be allocated from another Carveout heap in linux syste.*/
	HB_MEM_USAGE_PRIV_HEAP_2_RESERVED    = 0x400000000LL,	/**< buffer will be allocated from another Carveout heap in linux syste.*/
	HB_MEM_USAGE_PRIV_HEAP_SRAM_LIMIT    = 0x800000000LL,	/**< buffer will be allocated from sram limit heap.*/
	HB_MEM_USAGE_PRIV_HEAP_INLINE_ECC    = 0x1000000000LL,	/**< buffer will be allocated from inline ecc heap.*/
	HB_MEM_USAGE_PRIV_MASK               = 0xFF00000000LL,	/**< mask for the private flag */
};

/**
 * struct ion_platform_heap - defines a heap in the given platform
 * @type:	type of the heap from ion_heap_type enum
 * @id:		unique identifier for heap.  When allocating higher numb ers
 *		will be allocated from first.  At allocation these are passed
 *		as a bit mask and therefore can not exceed ION_NUM_HEAP_IDS.
 * @name:	used for debug purposes
 * @base:	base address of heap in physical memory if applicable
 * @size:	size of the heap in bytes if applicable
 * @priv:	private info passed from the board file
 *
 * Provided by the board file.
 */
/**
 * @struct ion_platform_heap
 * @brief defines a heap in the given platform
 * @NO{S21E04C01I}
 */
struct ion_platform_heap {
	enum ion_heap_type type;	/**< type of the heap from ion_heap_type enum */
	unsigned int id;	/**< unique identifier for heap. */
	const char *name;	/**< used for debug purposes */
	phys_addr_t base;	/**< base address of heap in physical memory if applicable */
	size_t size;	/**< size of the heap in bytes if applicable */
	phys_addr_t align;	/**< Physical address alignment */
	void *priv;		/**< private info passed from the board file */
};

/**
 * struct ion_platform_data - array of platform heaps passed from board file
 * @nr:		number of structures in the array
 * @heaps:	array of platform_heap structions
 *
 * Provided by the board file in the form of platform data to a platform device.
 */
/**
 * @struct ion_platform_data
 * @brief Define the ion platform data
 * @NO{S21E04C01I}
 */
struct ion_platform_data {
	int nr;		/**< number of structures in the array */
	struct ion_platform_heap *heaps;	/**< array of platform_heap structions */
};

struct ion_client;

/**
 * struct ion_device - the metadata of the ion device node
 * @dev:		the actual misc device
 * @buffers:		an rb tree of all the existing buffers
 * @buffer_lock:	lock protecting the tree of buffers
 * @lock:		rwsem protecting the tree of heaps and clients
 * @heaps:		list of all the heaps in the system
 * @user_clients:	list of all the clients created from userspace
 */
/**
 * the metadata of the ion device node
 */
/**
 * @struct ion_device
 * @brief Define the ion device
 * @NO{S21E04C01I}
 */
struct ion_device {
	struct miscdevice dev;	/**< the actual misc device */
	struct rb_root buffers;	/**< an rb tree of all the existing buffers */
	struct mutex buffer_lock;	/**< lock protecting the tree of buffers */
	struct rw_semaphore lock;	/**< rwsem protecting the tree of heaps and clients */
	struct plist_head heaps;	/**< list of all the heaps in the system */
	long (*custom_ioctl)(struct ion_client *client, unsigned int cmd,
			     unsigned long arg);	/**< arch specific ioctl function if applicable */
	struct rb_root clients;		/**< an rb tree of all the existing clients */
	struct dentry *debug_root;	/**< debug file root */
	struct dentry *heaps_debug_root;	/**< heaps debug file root */
	struct dentry *clients_debug_root;	/**< clients debug file root */
	struct rb_root share_buffers;	/**< an rb tree of all the share buffers */
	struct mutex share_lock;	/**< share handle mutex lock */
	struct mutex share_group_lock;	/**< share group mutex lock */
	struct idr shd_idr;			/**< ion device idr tree */
	struct idr group_idr;
	struct rb_root share_groups;
	int32_t multi_cma;		/**< the number of cma heaps */
	struct rb_root share_pool_buffers;	/**< ion share pool handle rbtree.*/
	struct dentry *ion_buf_debug_file;	/**< the ion buffer debugfs.*/
};

/**
 * struct ion_client - a process/hw block local address space
 * @node:		node in the tree of all clients
 * @dev:		backpointer to ion device
 * @handles:		an rb tree of all the handles in this client
 * @idr:		an idr space for allocating handle ids
 * @lock:		lock protecting the tree of handles
 * @name:		used for debugging
 * @display_name:	used for debugging (unique version of @name)
 * @display_serial:	used for debugging (to make display_name unique)
 * @task:		used for debugging
 *
 * A client represents a list of buffers this client may access.
 * The mutex stored here is used to protect both handles tree
 * as well as the handles themselves, and should be held while modifying either.
 */
/**
 * a process/hw block local address space
 */
/**
 * @struct ion_client
 * @brief Define the ion client
 * @NO{S21E04C01I}
 */
struct ion_client {
	struct rb_node hb_node;	/**< node in the tree of all clients */
	struct ion_device *dev;	/**< backpointer to ion device */
	struct rb_root handles;	/**< an rb tree of all the handles in this client */
	struct idr handle_idr;			/**< an idr space for allocating handle ids*/
	struct mutex lock;		/**< lock protecting the tree of handles */
	const char *name;		/**< used for debugging */
	char *display_name;		/**< used for debugging (unique version of @name) */
	int display_serial;		/**< used for debugging (to make display_name unique) */
	struct task_struct *task;	/**< used for debugging */
	pid_t hb_pid;				/**< the pid of the client creator */
	struct dentry *debug_root;	/**< debug file root */
	wait_queue_head_t wq;	/**< wait queue.*/
	int wq_cnt;				/**< waiy queue count.*/
	struct kfifo hb_fifo;		/**< data fifo*/
	spinlock_t fifo_lock;	/**< fifo lock*/
	struct mutex group_lock;
	struct rb_root group_datas;
};

/**
 * struct ion_buffer - metadata for a particular buffer
 * @ref:		refernce count
 * @node:		node in the ion_device buffers tree
 * @dev:		back pointer to the ion_device
 * @heap:		back pointer to the heap the buffer came from
 * @flags:		buffer specific flags
 * @private_flags:	internal buffer specific flags
 * @size:		size of the buffer
 * @priv_virt:		private data to the buffer representable as
 *			a void *
 * @priv_phys:		private data to the buffer representable as
 *			an phys_addr_t (and someday a phys_addr_t)
 * @lock:		protects the buffers cnt fields
 * @kmap_cnt:		number of times the buffer is mapped to the kernel
 * @vaddr:		the kernel mapping if kmap_cnt is not zero
 * @dmap_cnt:		number of times the buffer is mapped for dma
 * @sg_table:		the sg table for the buffer if dmap_cnt is not zero
 * @pages:		flat array of pages in the buffer -- used by fault
 *			handler and only valid for buffers that are faulted in
 * @vmas:		list of vma's mapping this buffer
 * @handle_count:	count of handles referencing this buffer
 * @task_comm:		taskcomm of last client to reference this buffer in a
 *			handle, used for debugging
 * @pid:		pid of last client to reference this buffer in a
 *			handle, used for debugging
 */
/**
 * @struct ion_buffer
 * @brief Define the ion buffer
 * @NO{S21E04C01I}
 */
struct ion_buffer {
	struct kref ref;			/**< refernce count */
	union {
		struct rb_node hb_node;	/**< node in the ion_device buffers tree */
		struct list_head list;	/**< list of ion_device buffers */
	};
	struct ion_device *dev;		/**< back pointer to the ion_device */
	struct ion_heap *heap;		/**< back pointer to the heap the buffer came from */
	unsigned long flags;		/**< buffer specific flags */
	unsigned long private_flags;	/**< internal buffer specific flags */
	size_t size;				/**< size of the buffer */
	union {
		void *priv_virt;		/**< private data to the buffer representable as a void * */
		phys_addr_t priv_phys;	/**< private data to the buffer representable as an phys_addr_t (and someday a phys_addr_t) */
	};
	struct mutex lock;			/**< protects the buffers cnt fields */
	int kmap_cnt;				/**< number of times the buffer is mapped to the kernel */
	void *vaddr;				/**< the kernel mapping if kmap_cnt is not zero */
	int dmap_cnt;				/**< number of times the buffer is mapped for dma */
	struct sg_table *hb_sg_table;	/**< the sg table for the buffer if dmap_cnt is not zero */
	struct list_head attachments;	/**< list of this buffer dma buf attachments*/
	struct page **pages;		/**< flat array of pages in the buffer */
	struct list_head vmas;		/**< list of vma's mapping this buffer */
	struct list_head iovas;		/**< list of iovas's mapping this buffer*/
	/* used to track orphaned buffers */
	int handle_count;			/**< count of handles referencing this buffer */
	int32_t share_id;			/**< the share handle id.*/
	char task_comm[TASK_COMM_LEN];	/**< taskcomm of last client to reference this buffer in a handle, used for debugging */
	pid_t hb_pid;					/**< pid of last client to reference this buffer in a handle, used for debugging*/
	void *priv_buffer;			/**< the next buffer address for sram scatterlist.*/
};

/**
 * ion_handle - a client local reference to a buffer
 * @ref:		reference count
 * @client:		back pointer to the client the buffer resides in
 * @buffer:		pointer to the buffer
 * @node:		node in the client's handle rbtree
 * @kmap_cnt:		count of times this client has mapped to kernel
 * @id:			client-unique id allocated by client->idr
 *
 * Modifications to node, map_cnt or mapping should be protected by the
 * lock in the client.  Other fields are never changed after initialization.
 */
/**
 * @struct ion_handle
 * @brief Define the ion handle
 * @NO{S21E04C01I}
 */
struct ion_handle {
	struct kref ref;				/**< reference count */
	struct ion_client *client;		/**< back pointer to the client the buffer resides in */
	struct ion_buffer *buffer;		/**< pointer to the buffer */
	struct rb_node hb_node;			/**< node in the client's handle rbtree */
	unsigned int kmap_cnt;			/**< count of times this client has mapped to kernel */
	int id;							/**< client-unique id allocated by client->idr */
	int share_id;					/**< unique id for share handle */
	int import_cnt;					/**< the import reference of the ion buffer */
	struct ion_share_handle * sh_hd;	/**< back point to the share handle */
	int32_t import_consume_cnt;		/**< the handle import consume count.*/
};

void ion_buffer_destroy(struct ion_buffer *buffer);

/**
 * struct ion_heap_ops - ops to operate on a given heap
 * @allocate:		allocate memory
 * @free:		free memory
 * @phys		get physical address of a buffer (only define on
 *			physically contiguous heaps)
 * @map_dma		map the memory for dma to a scatterlist
 * @unmap_dma		unmap the memory for dma
 * @map_kernel		map memory to the kernel
 * @unmap_kernel	unmap memory to the kernel
 * @map_user		map memory to userspace
 *
 * allocate, phys, and map_user return 0 on success, -errno on error.
 * map_dma and map_kernel return pointer on success, ERR_PTR on
 * error. @free will be called with ION_PRIV_FLAG_SHRINKER_FREE set in
 * the buffer's private_flags when called from a shrinker. In that
 * case, the pages being free'd must be truly free'd back to the
 * system, not put in a page pool or otherwise cached.
 */
/**
 * @struct ion_heap_ops
 * @brief Define the ops to operate on a given heap
 * @NO{S21E04C01I}
 */
struct ion_heap_ops {
	int (*allocate)(struct ion_heap *heap,
			struct ion_buffer *buffer, unsigned long len,
			unsigned long align, unsigned long flags);			/**< allocate memory interface */
	void (*free)(struct ion_buffer *buffer);		/**< free memory interface */
	int (*phys)(struct ion_heap *heap, struct ion_buffer *buffer,
		    phys_addr_t *addr, size_t *len);		/**< function on get physical address of a buffer (only define on physically contiguous heaps)*/
	struct sg_table * (*map_dma)(struct ion_heap *heap,	/**< function on map the memory for dma to a scatterlist */
				     struct ion_buffer *buffer);
	void (*unmap_dma)(struct ion_heap *heap, struct ion_buffer *buffer);	/**< function on unmap the memory for dma */
	void * (*map_kernel)(struct ion_heap *heap, struct ion_buffer *buffer);	/**< function on map memory to the kernel */
	void (*unmap_kernel)(struct ion_heap *heap, struct ion_buffer *buffer);	/**< function on unmap memory to the kernel */
	int (*map_user)(struct ion_heap *mapper, struct ion_buffer *buffer,		/**< function on map memory to userspace */
			struct vm_area_struct *vma);
	int (*shrink)(struct ion_heap *heap, gfp_t gfp_mask, int nr_to_scan);	/**< function on shrink memory */
};

/**
 * heap flags - flags between the heaps and core ion code
 */
#define ION_HEAP_FLAG_DEFER_FREE BIT(0)		/**< ion heap defer free flag.*/

/**
 * private flags - flags internal to ion
 */
/*
 * Buffer is being freed from a shrinker function. Skip any possible
 * heap-specific caching mechanism (e.g. page pools). Guarantees that
 * any buffer storage that came from the system allocator will be
 * returned to the system allocator.
 */
#define ION_PRIV_FLAG_SHRINKER_FREE BIT(0)		/**< ion private shrink free flag.*/

/**
 * struct ion_heap - represents a heap in the system
 * @node:		rb node to put the heap on the device's tree of heaps
 * @dev:		back pointer to the ion_device
 * @type:		type of heap
 * @ops:		ops struct as above
 * @flags:		flags
 * @id:			id of heap, also indicates priority of this heap when
 *			allocating.  These are specified by platform data and
 *			MUST be unique
 * @name:		used for debugging
 * @shrinker:		a shrinker for the heap
 * @free_list:		free list head if deferred free is used
 * @free_list_size	size of the deferred free list in bytes
 * @lock:		protects the free list
 * @waitqueue:		queue to wait on from deferred free thread
 * @task:		task struct of deferred free thread
 * @debug_show:		called when heap debug file is read to add any
 *			heap specific debug info to output
 *
 * Represents a pool of memory from which buffers can be made.  In some
 * systems the only heap is regular system memory allocated via vmalloc.
 * On others, some blocks might require large physically contiguous buffers
 * that are allocated from a specially reserved heap.
 */
/**
 * @struct ion_heap
 * @brief Define the represents a heap in the system
 * @NO{S21E04C01I}
 */
struct ion_heap {
	struct plist_node hb_node;		/**< rb node to put the heap on the device's tree of heaps */
	struct ion_device *dev;		/**< back pointer to the ion_device */
	enum ion_heap_type type;	/**< type of heap */
	struct ion_heap_ops *ops;	/**< ops struct as above */
	unsigned long flags;		/**< flags */
	unsigned int id;			/**< id of heap, also indicates priority of this heap when allocating*/
	const char *name;			/**< heap name, used for debugging*/
	struct shrinker hb_shrinker;	/**< a shrinker for the heap */
	struct list_head free_list;	/**< free list head if deferred free is used */
	size_t free_list_size;		/**< size of the deferred free list in bytes */
	spinlock_t free_lock;		/**< protects the free list */
	wait_queue_head_t waitqueue;	/**< queue to wait on from deferred free thread */
	struct task_struct *task;	/**< task struct of deferred free thread */
	size_t total_size;			/**< the heap total size.*/

	int (*debug_show)(struct ion_heap *heap, struct seq_file *, void *);	/**< called when heap debug file is read to add any heap specific debug info to output*/
};

/**
 * ion_buffer_cached - this ion buffer is cached
 * @buffer:		buffer
 *
 * indicates whether this ion buffer is cached
 */
bool ion_buffer_cached(struct ion_buffer *buffer);


/**
 * ion_device_add_heap - adds a heap to the ion device
 * @dev:		the device
 * @heap:		the heap to add
 */
void ion_device_add_heap(struct ion_device *dev, struct ion_heap *heap);

/**
 * ion_device_del_heap - delete a heap to the ion device
 * @dev:		the device
 * @heap:		the heap to add
 */
void ion_device_del_heap(struct ion_device *dev, struct ion_heap *heap);

/**
 * some helpers for common operations on buffers using the sg_table
 * and vaddr fields
 */
/**
 * ion_heap_map_kernel - map ion buffer to get virtual address
 * @heap:		the heap to map
 * @buffer:		the buffer to map
 */
void *ion_heap_map_kernel(struct ion_heap *heap, struct ion_buffer *buffer);

/**
 * ion_heap_unmap_kernel - unmap ion buffer
 * @heap:		the heap to unmap
 * @buffer:		the buffer to unmap
 */
void ion_heap_unmap_kernel(struct ion_heap *heap, struct ion_buffer *buffer);

struct dma_buf *ion_share_dma_buf(struct ion_client *client, struct ion_handle *handle);

/**
 * ion_heap_map_user - map ion buffer for user
 * @heap:		the heap to map
 * @buffer:		the buffer to map
 * @vma:		the user vm
 */
int ion_heap_map_user(struct ion_heap *heap, struct ion_buffer *buffer,
		      struct vm_area_struct *vma);

/**
 * ion_heap_buffer_zero_ex - make heap buffer zero
 * @buffer:	  the buffer to do zero
 */
int ion_heap_buffer_zero_ex(struct sg_table * table, unsigned long flags);

/**
 * ion_heap_buffer_zero - make heap buffer zero
 * @buffer:		the buffer to do zero
 */
int ion_heap_buffer_zero(struct ion_buffer *buffer);

/**
 * ion_heap_pages_zero - make pages zero
 * @heap_page:  the page to do zero
 * @size:		the page size
 * @pgprot:		the page property
 */
int ion_heap_pages_zero(struct page *heap_page, size_t size, pgprot_t pgprot);

/**
 * ion_heap_init_shrinker
 * @heap:		the heap
 *
 * If a heap sets the ION_HEAP_FLAG_DEFER_FREE flag or defines the shrink op
 * this function will be called to setup a shrinker to shrink the freelists
 * and call the heap's shrink op.
 */
void ion_heap_init_shrinker(struct ion_heap *heap);

/**
 * ion_heap_init_deferred_free -- initialize deferred free functionality
 * @heap:		the heap
 *
 * If a heap sets the ION_HEAP_FLAG_DEFER_FREE flag this function will
 * be called to setup deferred frees. Calls to free the buffer will
 * return immediately and the actual free will occur some time later
 */
int ion_heap_init_deferred_free(struct ion_heap *heap);

/**
 * ion_heap_freelist_add - add a buffer to the deferred free list
 * @heap:		the heap
 * @buffer:		the buffer
 *
 * Adds an item to the deferred freelist.
 */
void ion_heap_freelist_add(struct ion_heap *heap, struct ion_buffer *buffer);

/**
 * ion_heap_freelist_drain - drain the deferred free list
 * @heap:		the heap
 * @size:		amount of memory to drain in bytes
 *
 * Drains the indicated amount of memory from the deferred freelist immediately.
 * Returns the total amount freed.  The total freed may be higher depending
 * on the size of the items in the list, or lower if there is insufficient
 * total memory on the freelist.
 */
size_t ion_heap_freelist_drain(struct ion_heap *heap, size_t size);

/**
 * ion_heap_freelist_shrink - drain the deferred free
 *				list, skipping any heap-specific
 *				pooling or caching mechanisms
 *
 * @heap:		the heap
 * @size:		amount of memory to drain in bytes
 *
 * Drains the indicated amount of memory from the deferred freelist immediately.
 * Returns the total amount freed.  The total freed may be higher depending
 * on the size of the items in the list, or lower if there is insufficient
 * total memory on the freelist.
 *
 * Unlike with @ion_heap_freelist_drain, don't put any pages back into
 * page pools or otherwise cache the pages. Everything must be
 * genuinely free'd back to the system. If you're free'ing from a
 * shrinker you probably want to use this. Note that this relies on
 * the heap.ops.free callback honoring the ION_PRIV_FLAG_SHRINKER_FREE
 * flag.
 */
size_t ion_heap_freelist_shrink(struct ion_heap *heap,
				size_t size);

/**
 * ion_heap_freelist_size - returns the size of the freelist in bytes
 * @heap:		the heap
 */
size_t ion_heap_freelist_size(struct ion_heap *heap);


/**
 * functions for creating and destroying the built in ion heaps.
 * architectures can add their own custom architecture specific
 * heaps as appropriate.
 */
struct ion_heap *ion_heap_create(struct ion_platform_heap *);
void ion_heap_destroy(struct ion_heap *);
struct ion_heap *ion_system_heap_create(struct ion_platform_heap *);
void ion_system_heap_destroy(struct ion_heap *);

struct ion_heap *ion_system_contig_heap_create(struct ion_platform_heap *);
void ion_system_contig_heap_destroy(struct ion_heap *);

struct ion_heap *ion_carveout_heap_create(struct ion_platform_heap *);
void ion_carveout_heap_destroy(struct ion_heap *);

struct ion_heap *ion_chunk_heap_create(struct ion_platform_heap *);
void ion_chunk_heap_destroy(struct ion_heap *);
struct ion_heap *ion_custom_heap_create(struct ion_platform_heap *heap_data);
int ion_cma_get_info(struct ion_device *dev, phys_addr_t *base, size_t *size,
		enum ion_heap_type type);
int ion_add_cma_heaps(struct ion_device *idev);
int ion_del_cma_heaps(struct ion_device *idev);
struct sg_table *ion_sg_table(struct ion_client *client, struct ion_handle *handle);

/**
 * The carveout heap returns physical addresses, since 0 may be a valid
 * physical address, this is used to indicate allocation failed
 */
/**
 * kernel api to allocate/free from carveout -- used when carveout is
 * used to back an architecture specific custom heap
 */
int32_t get_carveout_info(struct ion_heap *heap, uint64_t *avail, uint64_t *max_contigous);
phys_addr_t ion_carveout_allocate(struct ion_heap *heap, unsigned long size,
				      unsigned long align);
void ion_carveout_free(struct ion_heap *heap, phys_addr_t addr,
		       unsigned long size);

/**
 * functions for creating and destroying a heap pool -- allows you
 * to keep a pool of pre allocated memory to use from your heap.  Keeping
 * a pool of memory that is ready for dma, ie any cached mapping have been
 * invalidated from the cache, provides a significant performance benefit on
 * many systems
 */

/**
 * struct ion_page_pool - pagepool struct
 * @high_count:		number of highmem items in the pool
 * @low_count:		number of lowmem items in the pool
 * @high_items:		list of highmem items
 * @low_items:		list of lowmem items
 * @mutex:		lock protecting this struct and especially the count
 *			item list
 * @gfp_mask:		gfp_mask to use from alloc
 * @order:		order of pages in the pool
 * @list:		plist node for list of pools
 * @cached:		it's cached pool or not
 *
 * Allows you to keep a pool of pre allocated pages to use from your heap.
 * Keeping a pool of pages that is ready for dma, ie any cached mapping have
 * been invalidated from the cache, provides a significant performance benefit
 * on many systems
 */
/**
 * @struct ion_page_pool
 * @brief Define the ion page pool
 * @NO{S21E04C01I}
 */
struct ion_page_pool {
	int high_count; /**< number of highmem items in the pool */
	int low_count;	/**< number of lowmem items in the pool */
	bool cached;	/**< it's cached pool or not */
	struct list_head high_items;	/**< list of highmem items */
	struct list_head low_items;		/**< list of lowmem items */
	struct mutex pool_mutex;		/**< lock protecting this struct and especially the count item list */
	gfp_t gfp_mask;			/**< gfp_mask to use from alloc */
	unsigned int order;		/**< order of pages in the pool */
	struct plist_node list;	/**< plist node for list of pools*/
};

struct ion_page_pool *ion_page_pool_create(gfp_t gfp_mask, unsigned int order,
					   bool cached);
void ion_page_pool_destroy(struct ion_page_pool *pool);
struct page *ion_page_pool_alloc(struct ion_page_pool *pool);
void ion_page_pool_free(struct ion_page_pool *pool, struct page *hb_page);

/** ion_page_pool_shrink - shrinks the size of the memory cached in the pool
 * @pool:		the pool
 * @gfp_mask:		the memory type to reclaim
 * @nr_to_scan:		number of items to shrink in pages
 *
 * returns the number of items freed in pages
 */
int ion_page_pool_shrink(struct ion_page_pool *pool, gfp_t gfp_mask,
			 int nr_to_scan);

/**
 * ion_device_create - allocates and returns an ion device
 * @custom_ioctl:	arch specific ioctl function if applicable
 *
 * returns a valid device or -PTR_ERR
 */
struct ion_device *ion_device_create(long (*custom_ioctl)
				     (struct ion_client *client,
				      unsigned int cmd,
				      unsigned long arg));

/**
 * ion_device_destroy - free and device and it's resource
 * @dev:		the device
 */
void ion_device_destroy(struct ion_device *dev);

/**
 * @NO{S21E04C01I}
 * @ASIL{B}
 * @brief create the ion client
 *
 * @param[in] dev: the ion device
 * @param[in] name: the module name
 *
 * @retval "correct_pte": succeed
 * @retval "=-22": invalid parameter
 * @retval "=-12": out of memory
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
struct ion_client *ion_client_create(struct ion_device *dev,
				     const char *name);

/**
 * @NO{S21E04C01I}
 * @ASIL{B}
 * @brief destroy the ion client
 *
 * @param[in] client: the ion client
 *
 * @return None
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
void ion_client_destroy(struct ion_client *client);

/**
 * @NO{S21E04C01I}
 * @ASIL{B}
 * @brief allocate buffer
 *
 * @param[in] client: the ion client
 * @param[in] len: the buffer size
 * @param[in] align: the alignment
 * @param[in] heap_id_mask: the heap id mask
 * @param[in] flags: the allocate flags
 *
 * @retval "correct_pte": succeed
 * @retval "err_ptr": produce error ptr
 * @retval "=-22": invalid parameter
 * @retval "=-19": no such device
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
struct ion_handle *ion_alloc(struct ion_client *client, size_t len,
			     size_t align, unsigned int heap_id_mask,
			     unsigned int flags);

/**
 * @NO{S21E04C01I}
 * @ASIL{B}
 * @brief free buffer
 *
 * @param[in] client: the ion client
 * @param[in] handle: the ion handle
 *
 * @return None
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
void ion_free(struct ion_client *client, struct ion_handle *handle);

int ion_mmap(struct ion_buffer *buffer, struct vm_area_struct *vma);

/**
 * @NO{S21E04C01I}
 * @ASIL{B}
 * @brief get the phys address
 *
 * @param[in] client: the ion client
 * @param[in] handle_id: the ion handle id
 * @param[out] addr: the phys address
 * @param[out] len: the buffer size
 *
 * @retval "=0": succeed
 * @retval "err_ptr": error ion handle
 * @retval "=-22": invalid parameter
 * @retval "=-19": no such device
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
int ion_phys(struct ion_client *client, int handle_id, phys_addr_t *addr, size_t *len);

/**
 * @NO{S21E04C01I}
 * @ASIL{B}
 * @brief map virtual address in kernel
 *
 * @param[in] client: the ion client
 * @param[in] handle: the ion handle
 *
 * @retval "correct_pte": succeed
 * @retval "=-22": invalid parameter
 * @retval "=-19": no such device
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
void *ion_map_kernel(struct ion_client *client, struct ion_handle *handle);

/**
 * @NO{S21E04C01I}
 * @ASIL{B}
 * @brief unmap virtual address in kernel
 *
 * @param[in] client: the ion client
 * @param[in] handle: the ion handle
 *
 * @return None
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
void ion_unmap_kernel(struct ion_client *client, struct ion_handle *handle);

/**
 * @NO{S21E04C01I}
 * @ASIL{B}
 * @brief Check the memory whether it is within the carveout heap range
 *
 * @param[in] start: the start phyiscal address
 * @param[in] size: the memory size
 *
 * @retval "0": in the carveout heap range
 * @retval "-EINVAL": out of the carveout heap range
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
int ion_check_in_heap_carveout(phys_addr_t start, size_t size);

/**
 * hobot_get_heap_size - get the heap total size
 * @paddr:		the heap id
 */
size_t hobot_get_heap_size(uint32_t heap_id);

/**
 * ion_share_dma_buf_fd() - given an ion client, create a dma-buf fd
 * @client:    the client
 * @handle:    the handle
 */
int ion_share_dma_buf_fd(struct ion_client *client, struct ion_handle *handle);

/**
 * ion_import_dma_buf() - import an ion client with a dma-buf fd
 * @client:    the client
 * @fd:        the fd
 */
struct ion_handle *ion_import_dma_buf(struct ion_client *client, int hb_fd);

/**
 * @NO{S21E04C02I}
 * @ASIL{B}
 * @brief get the ion flag by hbmem flag
 *
 * @param[in] flags: the hbmem alloc flag
 * @param[out] heap_id_mask: the alloc heap id mask
 * @param[out] port: the alloc port
 *
 * @retval alloc_flags: succeed
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
uint32_t hbmem_flag_to_ion_flag(int64_t flags, uint32_t *heap_id_mask, int32_t *port);

/**
 * @NO{S21E04C01I}
 * @ASIL{B}
 * @brief transfer the ion flag to hbmem flag
 *
 * @param[in] ion_flag: the ion flag
 * @param[in] heap_id_mask: the heap id mask
 * @param[in] pool_flag: the whether user memory pool flag
 * @param[in] prot: the write/read flag
 *
 * @return "flag": hbmem flag
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
int64_t ion_flag_to_hbmem_flag(uint32_t ion_flag, uint32_t heap_id_mask, int64_t pool_flag, int64_t prot);

struct ion_handle *ion_handle_get_by_id_nolock(struct ion_client *client, int id);

int ion_handle_put(struct ion_handle *handle);

/**
 * @NO{S21E04C01I}
 * @ASIL{B}
 * @brief use ion share handle to import dma buffer with share id
 *
 * @param[in] client: the ion client
 * @param[in] share_id: the share id
 *
 * @retval "correct_pte": succeed
 * @retval "err_ptr": false share_hd
 * @retval "=-22": invalid parameter
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
struct ion_handle *ion_import_dma_buf_with_shareid(struct ion_client *client, int32_t share_id);

/**
 * @NO{S21E04C01I}
 * @ASIL{B}
 * @brief get the ion device
 *
 * @retval "correct_pte": succeed
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
struct ion_device *hobot_ion_get_ion_device(void);

#if IS_ENABLED(CONFIG_HOBOT_IOMMU)
/**
 * @struct ion_iovm_map
 * @brief Define the ion iovmm map information
 * @NO{S21E04C01I}
 */
struct ion_iovm_map {
	struct list_head list;		/**< ion iovmm map list*/
	uint32_t map_cnt;			/**< map count.*/
	struct device *dev;			/**< master device structure.*/
	struct iommu_domain *domain;	/**< the iommu domain of the master device*/
	dma_addr_t iova;			/**< the iova.*/
	size_t size;				/**< map size.*/
};
#endif

/**
 * @struct ion_heap_range
 * @brief the ion heap range
 * @NO{S21E04C01I}
 */
struct ion_heap_range{
	phys_addr_t base;	/**< the heap base physical address.*/
	size_t size;		/**< the heap total size.*/
};
int32_t ion_get_all_heaps_range(struct ion_heap_range *heap_range, int num);

/**
 * @struct ion_phy_data_pac
 * @brief Define the descriptor of ion phy pac data
 * @NO{S21E04C01I}
 */
struct ion_phy_data_pac {
	ion_user_handle_t handle;	/**< ion handle id.*/
	phys_addr_t paddr;			/**< padder of the buffer.*/
	size_t len;					/**< the buffer size.*/
	uint64_t reserved;			/**< reserved.*/
};

#ifdef CONFIG_PCIE_HOBOT_EP_FUN_AI
#define ION_NAME_LEN_PAC	32		/**< pac ion name length*/

struct ion_shareid_import_info_data {
	ion_user_handle_t handle;		/**< ion handle id.*/
	int32_t share_id;		/**< share_id.*/
};

/**
 * @struct ion_data_pac
 * @brief Define the descriptor of ion pac data
 * @NO{S21E04C01I}
 */
struct ion_data_pac{
	uint32_t message_flag;		/**< messahe flag use for handshake,
								 * range:[0,1]; default: 0
								 */
	uint32_t cmd;				/**< command.
								 * range:[0, ); default: 0
								 */
	uint32_t cmd_custom;		/**< custom command.
								 * range:[0, ); default: 0
								 */
	void *client;				/**< pointer of ion client in EP */
	union {
		struct ion_fd_data fd;	/**< ion fd data structure @ion_fd_data*/
		struct ion_allocation_data allocation;	/**< ion alloc data structure @ion_allocation_data*/
		struct ion_handle_data handle;			/**< ion handle data structure @ion_handle_data*/
		struct ion_custom_data custom;			/**< ion custom data structure @ion_custom_data*/
		struct ion_share_handle_data share_hd;	/**< ion share handle data structure @ion_share_handle_data*/
		struct ion_share_info_data share_info;	/**< ion share info data structure @ion_share_info_data*/
		struct ion_phy_data_pac phy_data;		/**< ion phy pac data structure @ion_phy_data_pac*/
		struct ion_share_and_phy_data share_phy_data;
		struct ion_process_info_data process_info;
		struct ion_consume_info_data consume_info;
		struct ion_version_info_data version_info;
		struct ion_shareid_import_info_data share_import_data;
	}data;						/**< union for data within use space and kernel space */
	char module_name[ION_NAME_LEN_PAC];	/**< the module name */
	int32_t return_value;		/**< the return value from EP.
								 * range:( ,0]; default: 0 */
	uint32_t crc_value;			/**< the crc value for ion data
								 * range:[0, ]; default: 0
								 */
};

/**
 * @struct ion_heap_range_data_pac
 * @brief Define the descriptor of ion heap range pac data in RC
 * @NO{S21E04C01I}
 */
struct ion_heap_range_data_pac{
	struct ion_heap_range heap[ION_HEAP_TYPE_DMA_EX + 1];	/**< array for ion heap range */
	dma_addr_t heap_iovas[ION_HEAP_TYPE_DMA_EX + 1];        /**< array for heap iova*/
	uint32_t message_flag;		/**< message flag use for handshake
								 * range:[0,1]; default: 0
								 */
	uint32_t crc_value;			/**< crc value for ion heap range pac data.
								 * range:[0, ]; default: 0
								 */
};

int32_t ion_function_operation(struct ion_data_pac *ion_rev_data);
int32_t ion_function_custom_operation(struct ion_data_pac *ion_rev_data);
int32_t ion_get_heap_range(phys_addr_t *base_addr, size_t *total_size);
int32_t ion_get_all_heap_range(struct ion_heap_range_data_pac *heap_range);
#endif

/**
 * User can get the ion_dma_buf_data using iommu_map interface,
 * and use iommu_unmap to release its related resource.
 */
/**
 * @struct ion_dma_buf_data
 * @brief defines a ion dma buf using by smmu
 * @NO{S21E04C01I}
 */
struct ion_dma_buf_data {
	struct ion_client *client;			/**< back pointer to the ion client */
	struct ion_handle *ion_handle;		/**< back pointer to the ion handle */
	struct dma_buf *dma_buf;			/**< dma buffer in the ion handle */
	struct dma_buf_attachment *attachment;	/**< dma buffer attachment */
	struct sg_table *sg_table;			/**< sg table in the dma buffer */
	dma_addr_t dma_addr;				/**< mapped dma address */
};


#endif /* _ION_H */
