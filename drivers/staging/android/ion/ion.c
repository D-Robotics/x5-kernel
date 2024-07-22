/*
 *
 * drivers/staging/android/ion/ion.c
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
 * @file ion.c
 *
 * @NO{S21E04C01U}
 * @ASIL{B}
 */
#include <linux/device.h>
#include <linux/err.h>
#include <linux/file.h>
#include <linux/freezer.h>
#include <linux/fs.h>
#include <linux/anon_inodes.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/memblock.h>
#include <linux/miscdevice.h>
#include <linux/export.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/rbtree.h>
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/debugfs.h>
#include <linux/dma-buf.h>
#include <linux/idr.h>
#include <linux/sched/task.h>
#include <linux/scatterlist.h>
#include <uapi/asm-generic/mman-common.h>
#include <linux/export.h>
#include <linux/ion.h>

#define ION_DRIVER_API_VERSION_MAJOR	1
#define ION_DRIVER_API_VERSION_MINOR	0

#define ION_MODULE_TYPE_INTERNAL   0x0		/**< the internal module type.*/
#define ION_MODULE_TYPE_BPU        0x1		/**< the module type of BPU */

#define ION_MODULE_TYPE_VPU        0x2		/**< the module type of VPU */

#define ION_MODULE_TYPE_JPU        0x3		/**< the module type of JPU */

#define ION_MODULE_TYPE_GPU        0x4		/**< the module type of GPU */

#define PRIVATE_USER_MODULE_MEM_BIT_SHIFT  48	/**< private user module memory shift bits.*/
#define ION_MODULE_TYPE_BIT_SHIFT  28	/**< the shift bits of ion module type.*/

#define SPBUFFD 0					/**< share pool buffer share fd index.*/
#define SPBUFID 1					/**<share pool buffer share handle id index*/
#define SPBUFREFCNT 2				/**<share pool buffer reference count index*/
#define SPBUFNUM 3					/**< share pool buffer num index*/
#define SPWQTIMEOUT 30				/**<wait time.*/
#define FIFONUM (100 * SPBUFNUM)	/**< fifo number.*/

#define ION_MEM_TYPE_BIT_SHIFT     16		/**< the shift bit of the ion memory type.*/
#define ION_MEM_TYPE_MASK          0xFFF	/**< the ion memory type mask.*/

#define ION_SRAM_SG_NUM				2		/**< the sram scatterlist number in sg_table.*/
#define ION_SRAM_SG_FIR			0			/**< the index 0 of sram scatterlist.*/
#define ION_SRAM_SG_SEC			1			/**< the index 0 of sram scatterlist.*/

/**
 * @struct share_pool_monitor_sync
 * @brief Define the share pool monitor sync
 * @NO{S21E04C01I}
 */
struct share_pool_monitor_sync {
	wait_queue_head_t share_pool_wait_q;	/**< the share pool wait queue.*/
	int32_t share_pool_cnt;					/**< the share pool count.*/
	int32_t thread_pid;						/**< the thread id.*/
};

/**
 * @struct ion_share_pool_buf
 * @brief Define the share pool buffer
 * @NO{S21E04C01I}
 */
struct ion_share_pool_buf {
	struct kref ref;			/**< the reference count.*/
	struct ion_device *dev;		/**< ion device structure.*/
	struct ion_buffer * buffer;	/**< ion buffer */
	struct rb_node node;		/**< ion share pool rbtree node.*/
	struct ion_client *client;	/**< ion client structure.*/
	// struct ion_share_handle *share_hd;
	int32_t share_fd;			/**< the share fd of the share pool buffer.*/
	int32_t share_id;			/**< the share if of the share handle.*/
	int32_t sw_ref_cnt;			/**< the reference count.*/
};

#define MAXSYNCNUM 16				/**< max sync num*/

/**
 * @struct ion_share_handle
 * @brief Define the ion share handle
 * @NO{S21E04C01I}
 */
struct ion_share_handle {
	struct kref ref;			/**< the reference.*/
	struct ion_device *dev;		/**< the ion device.*/
	struct ion_buffer * buffer;	/**< the ion buffer.*/
	int id;						/**< the ion share handle id.*/
	struct rb_node node;		/**< the rbtree node.*/
	struct mutex share_hd_lock;	/**< the share handle mutex lock.*/
	wait_queue_head_t client_cnt_wait_q;	/**< the client wait queue for client count.*/
	int32_t client_cnt;			/**< the client reference count.*/
	DECLARE_BITMAP(bitmap, MAXSYNCNUM);	/**< share pool bit map*/
	struct share_pool_monitor_sync share_pool_sync[MAXSYNCNUM];	/**< the share pool monitor sync.*/
	wait_queue_head_t consume_cnt_wait_q;	/**< the client wait queue for consume count.*/
	int32_t consume_cnt;		/**< the consume count.*/
	struct mutex consume_cnt_lock;	/**< the consume count lock.*/
};

/**
 * @enum ion_mem_type
 * @brief Define the ion memory type.
 * @NO{S21E04C01U}
 */
enum ion_mem_type {
	ION_MEM_TYPE_PYRAMID = 0x7,		/**< pryamid ion memory type.*/
	ION_MEM_TYPE_ISP = 0xB,			/**< isp ion memory type.*/
	ION_MEM_TYPE_GDC_OUT,			/**< gdc out ion memory type.*/
	ION_MEM_TYPE_DISPLAY,			/**< display ion memory type.*/
	ION_MEM_TYPE_GDC,				/**< gdc ion memory type.*/
	ION_MEM_TYPE_BPU = 0x11,		/**< bpu ion memory type.*/
	ION_MEM_TYPE_VIDEO_CODEC = 0x13,/**< video codec ion memory type.*/
	ION_MEM_TYPE_CIM = 0x19,		/**< cim ion memory type.*/
	ION_MEM_TYPE_STITCH = 0x1C,		/**< sticth ion memory type.*/
	ION_MEM_TYPE_OPTICAL_FLOW,		/**< optical flow ion memory type.*/
	ION_MEM_TYPE_JPEG_CODEC,		/**< jpeg codec ion memory type.*/
	ION_MEM_TYPE_VDSP,				/**< vdsp ion memory type.*/
	ION_MEM_TYPE_IPC,				/**< ipc ion memory type.*/
	ION_MEM_TYPE_PCIE,				/**< pcie ion memory type.*/
	ION_MEM_TYPE_YNR,				/**< ynr ion memory type.*/
	ION_MEM_TYPE_OTHER,				/**< other ion memory type.*/
};

/**
 * Purpose: ion data name
 * Value: NA
 * Range: NA
 * Attention: NA
 */
/**
 * ion data name
 */
static char *_vio_data_type[] = {
	"ipuds0_other", "ipuds1", "ipuds2", "ipuds3", "ipuds4",
	"ipuus", "pymfb", "pymdata", "siffb", "sifraw",
	"sifyuv", "ispyuv", "gdc", "idu", "gdcfb",
	"pymlayer", "rgn", "bpu0", "bpu1", "vpu",
	"vpu0", "vpu1", "vpu2", "vpu3", "vpu4",
	"cimdma", "ispraw", "ispstat", "stitch", "lkof",
	"jpu", "vdsp", "ipc", "pcie", "ynr",
	"other",
	};

static int32_t ion_share_pool_notify(struct ion_device *dev,
	int32_t share_id, int32_t import_cnt, int32_t timeout, int32_t retry);

bool ion_buffer_cached(struct ion_buffer *buffer)
{
	return !!(buffer->flags & ION_FLAG_CACHED);
}

/* this function should only be called while dev->lock is held */
static void ion_buffer_add(struct ion_device *dev,
			   struct ion_buffer *buffer)
{
	struct rb_node **p = &dev->buffers.rb_node;
	struct rb_node *parent = NULL;
	struct ion_buffer *entry;

	while (*p) {
		parent = *p;
		entry = rb_entry(parent, struct ion_buffer, node);

		if (buffer < entry) {
			p = &(*p)->rb_left;
		} else if (buffer > entry) {
			p = &(*p)->rb_right;
		} else {
			pr_err("%s: buffer already found.", __func__);
			BUG();
		}
	}

	rb_link_node(&buffer->node, parent, p);
	rb_insert_color(&buffer->node, &dev->buffers);
}

/* this function should only be called while dev->lock is held */
/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief create an buffer
 *
 * @param[in] heap: the ion heap structure.
 * @param[in] dev: the ion device structure.
 * @param[in] len: the buffer size
 * @param[in] align: the alloc alignment
 * @param[in] flags: the alloc flags
 *
 * @retval "ion_buffer": the ion buffer structure
 * @retval "ERR_PTR": failed
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
static struct ion_buffer *ion_buffer_create(struct ion_heap *heap,
				     struct ion_device *dev,
				     unsigned long len,
				     unsigned long align,
				     unsigned long flags)
{
	struct ion_buffer *buffer;
	int ret;

	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	if (!buffer)
		return ERR_PTR(-ENOMEM);

	buffer->heap = heap;
	buffer->flags = flags & (~(unsigned long)ION_FLAG_USE_POOL);
	buffer->dev = dev;
	buffer->size = len;
	buffer->priv_buffer = NULL;
	kref_init(&buffer->ref);

	ret = heap->ops->allocate(heap, buffer, len, align, flags);

	if (ret) {
		if (!(heap->flags & ION_HEAP_FLAG_DEFER_FREE))
			goto err2;

		ion_heap_freelist_drain(heap, 0);
		ret = heap->ops->allocate(heap, buffer, len, align,
					  flags);
		if (ret) {
			pr_err("%s: ion heap alloc failed[%d]\n", __func__, ret);
			goto err2;
		}
	}

	if (!buffer->sg_table) {
		WARN_ONCE(1, "This heap needs to set the sgtable");
		ret = -EINVAL;
		goto err1;
	}

	INIT_LIST_HEAD(&buffer->vmas);
	INIT_LIST_HEAD(&buffer->attachments);

#if IS_ENABLED(CONFIG_HOBOT_IOMMU)
	INIT_LIST_HEAD(&buffer->iovas);
#endif
	mutex_init(&buffer->lock);

	mutex_lock(&dev->buffer_lock);
	ion_buffer_add(dev, buffer);
	mutex_unlock(&dev->buffer_lock);
	return buffer;

err1:
	heap->ops->free(buffer);
err2:
	kfree(buffer);
	return ERR_PTR(ret);
}

void ion_buffer_destroy(struct ion_buffer *buffer)
{
#if IS_ENABLED(CONFIG_HOBOT_IOMMU)
	struct ion_iovm_map *iovm_map;
	struct ion_iovm_map *tmp;
#endif
	if (buffer->kmap_cnt > 0) {
		pr_warn_once("%s: buffer still mapped in the kernel\n",
			     __func__);
		buffer->heap->ops->unmap_kernel(buffer->heap, buffer);
	}
#if IS_ENABLED(CONFIG_HOBOT_IOMMU)
	list_for_each_entry_safe(iovm_map, tmp, &buffer->iovas, list) { /* PRQA S 3673,0497,2810,1020,3432,0306,1021 */
		hobot_iovmm_unmap_sg(iovm_map->dev, iovm_map->iova, iovm_map->size);
		list_del(&iovm_map->list);
		kfree(iovm_map);
	}
#endif

	buffer->heap->ops->free(buffer);

	kfree(buffer);
}

static void _ion_buffer_destroy(struct kref *kref)
{
	struct ion_buffer *buffer = container_of(kref, struct ion_buffer, ref);
	struct ion_heap *heap = buffer->heap;
	struct ion_device *dev = buffer->dev;

	if (buffer->priv_buffer != NULL) {
		struct ion_buffer *buffer2 = (struct ion_buffer *)buffer->priv_buffer;

		pr_info("ready to free buffer2, the size is %lx, the heap type is %d, the heap name is %s\n",
					buffer2->size, buffer2->heap->type, buffer2->heap->name);
		mutex_lock(&dev->buffer_lock);
		rb_erase(&buffer2->node, &dev->buffers);
		mutex_unlock(&dev->buffer_lock);
		buffer->size = buffer->size - buffer2->size;

		if (heap->flags & ION_HEAP_FLAG_DEFER_FREE)
			ion_heap_freelist_add(heap, buffer2);
		else
			ion_buffer_destroy(buffer2);
	}

	mutex_lock(&dev->buffer_lock);
	rb_erase(&buffer->node, &dev->buffers);
	mutex_unlock(&dev->buffer_lock);

	if (heap->flags & ION_HEAP_FLAG_DEFER_FREE)
		ion_heap_freelist_add(heap, buffer);
	else
		ion_buffer_destroy(buffer);
}

static void ion_buffer_get(struct ion_buffer *buffer)
{
	kref_get(&buffer->ref);
}

static int ion_buffer_put(struct ion_buffer *buffer)
{
	return kref_put(&buffer->ref, _ion_buffer_destroy);
}

static void ion_buffer_add_to_handle(struct ion_buffer *buffer)
{
	mutex_lock(&buffer->lock);
	buffer->handle_count++;
	mutex_unlock(&buffer->lock);
}

static void ion_buffer_remove_from_handle(struct ion_buffer *buffer)
{
	/*
	 * when a buffer is removed from a handle, if it is not in
	 * any other handles, copy the taskcomm and the pid of the
	 * process it's being removed from into the buffer.  At this
	 * point there will be no way to track what processes this buffer is
	 * being used by, it only exists as a dma_buf file descriptor.
	 * The taskcomm and pid can provide a debug hint as to where this fd
	 * is in the system
	 */
	mutex_lock(&buffer->lock);
	buffer->handle_count--;
	BUG_ON(buffer->handle_count < 0);
	if (!buffer->handle_count) {
		struct task_struct *task;

		task = current->group_leader;
		get_task_comm(buffer->task_comm, task);
		buffer->pid = task_pid_nr(task);
	}
	mutex_unlock(&buffer->lock);
}

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief create an ion share handle for the buffer
 *
 * @param[in] dev: the ion device pointer
 * @param[in] buffer: the buffer pointer
 *
 * @retval "=-12": create the ion share handle failed, lack memory
 * @retval correct_ptr: create the ion share handle succeed
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
static struct ion_share_handle *ion_share_handle_create(struct ion_device *dev,
									struct ion_buffer *buffer)
{
	int i = 0;
	struct ion_share_handle *share_hd;

	share_hd = kzalloc(sizeof(struct ion_share_handle), GFP_KERNEL);
	if (!share_hd)
		return ERR_PTR(-ENOMEM);

	kref_init(&share_hd->ref);
	mutex_init(&share_hd->share_hd_lock);
	share_hd->dev = dev;
	ion_buffer_get(buffer);
	share_hd->buffer = buffer;
	init_waitqueue_head(&share_hd->client_cnt_wait_q);
	mutex_init(&share_hd->consume_cnt_lock);
	init_waitqueue_head(&share_hd->consume_cnt_wait_q);
	share_hd->consume_cnt = 0;
	share_hd->client_cnt++;
	for (i = 0; i < MAXSYNCNUM; i++) {
		init_waitqueue_head(&share_hd->share_pool_sync[i].share_pool_wait_q);
		share_hd->share_pool_sync[i].share_pool_cnt = 0;
		share_hd->share_pool_sync[i].thread_pid = -1;
		(void)test_and_clear_bit(i, share_hd->bitmap);
	}

	return share_hd;
}

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief destroy the ion share handle for the buffer
 *
 * @param[in] kref: the ion share handle reference count
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
static void ion_share_handle_destroy(struct kref *kref)
{
	struct ion_share_handle *share_hd = container_of(kref, struct ion_share_handle, ref);
	struct ion_device *dev = share_hd->dev;
	struct ion_buffer *buffer = share_hd->buffer;

	idr_remove(&dev->idr, share_hd->id);
	if (!RB_EMPTY_NODE(&share_hd->node))
		rb_erase(&share_hd->node, &dev->share_buffers);

	ion_buffer_put(buffer);

	kfree(share_hd);
}

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief increase the ion share handle reference count
 *
 * @param[in] share_hd: the ion share handle pointer
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
static void ion_share_handle_get(struct ion_share_handle *share_hd)
{
	kref_get(&share_hd->ref);
}

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief decrease the ion share handle reference count, if the count becomes to 0, call ion_share_handle_destroy
 *
 * @param[in] share_hd: the ion share handle pointer
 *
 * @retval "=1": decrease the ion share handle reference count and release the ion share handle
 * @retval "=0": only decrease the ion share handle reference count
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
static int ion_share_handle_put(struct ion_share_handle *share_hd)
{
	struct ion_device *dev = share_hd->dev;
	int ret;

	mutex_lock(&dev->share_lock);
	ret = kref_put(&share_hd->ref, ion_share_handle_destroy);
	mutex_unlock(&dev->share_lock);

	return ret;
}

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief increase the ion share handle client reference count
 *
 * @param[in] share_hd: the ion share handle pointer
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
static void ion_share_handle_add_to_handle(struct ion_share_handle *share_hd)
{
	mutex_lock(&share_hd->share_hd_lock);
	share_hd->client_cnt++;
	mutex_unlock(&share_hd->share_hd_lock);
}

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief decrease the ion share handle client reference count and wake up the wait queue
 *
 * @param[in] share_hd: the ion share handle pointer
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
static void ion_share_handle_remove_from_handle(struct ion_share_handle *share_hd)
{
	mutex_lock(&share_hd->share_hd_lock);
	if (!share_hd->client_cnt) {
		pr_warn("%s: Double removing share handle(share id %d) detected! bailing...\n",
			__func__, share_hd->id);
	} else {
		share_hd->client_cnt--;
	}
	wake_up_interruptible(&share_hd->client_cnt_wait_q);
	mutex_unlock(&share_hd->share_hd_lock);
}

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief get the ion share handle client reference count
 *
 * @param[in] share_hd: the ion share handle pointer
 *
 * @return the client reference count
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
int32_t ion_share_handle_get_share_info(struct ion_share_handle *share_hd)
{
	int32_t ret;

	mutex_lock(&share_hd->share_hd_lock);
	ret = share_hd->client_cnt;
	mutex_unlock(&share_hd->share_hd_lock);

	return ret;
}

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief get the ion share handle consume reference count
 *
 * @param[in] share_hd: the ion share handle pointer
 *
 * @return the client consume count
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
static int32_t ion_share_handle_get_consume_info(struct ion_share_handle *share_hd)
{
	int32_t ret;

	mutex_lock(&share_hd->consume_cnt_lock);
	ret = share_hd->consume_cnt;
	mutex_unlock(&share_hd->consume_cnt_lock);

	return ret;
}

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief increase the ion handle import count and ion share handle client reference count
 *
 * @param[in] handle: the ion handle pointer
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
static void ion_handle_import_get(struct ion_handle *handle)
{
	struct ion_share_handle * sh_hd = handle->sh_hd;

	handle->import_cnt++;
	if (sh_hd != NULL) {
		ion_share_handle_add_to_handle(sh_hd);
	}

	return;
}

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief decrease the ion handle import consume count and ion share handle client consume count
 *
 * @param[in] handle: the ion handle pointer
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
static void ion_handle_import_consume_put(struct ion_handle *handle)
{
	struct ion_share_handle * share_hd = handle->sh_hd;

	if (!handle->import_consume_cnt) {
		pr_warn("%s: Double unimport handle(share id %d) detected! bailing...\n",
			__func__, handle->share_id);
		return;
	}

	handle->import_consume_cnt--;
	if (share_hd != NULL) {
		mutex_lock(&share_hd->consume_cnt_lock);
		if (!share_hd->consume_cnt) {
			pr_warn("%s: Double removing share handle(share id %d) detected! bailing...\n",
				__func__, share_hd->id);
		} else {
			share_hd->consume_cnt--;
		}
		wake_up_interruptible(&share_hd->consume_cnt_wait_q);
		mutex_unlock(&share_hd->consume_cnt_lock);
	}
}

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief decrease the ion handle import count and ion share handle client reference count
 *
 * @param[in] handle: the ion handle pointer
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
static void ion_handle_import_put(struct ion_handle *handle)
{
	struct ion_share_handle * sh_hd = handle->sh_hd;

	if (!handle->import_cnt) {
		pr_warn("%s: Double unimport handle(share id %d) detected! bailing...\n",
			__func__, handle->share_id);
		return;
	}
	handle->import_cnt--;
	if (sh_hd != NULL) {
		ion_share_handle_remove_from_handle(sh_hd);
	}

	return;
}

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief create an ion handle for the buffer
 *
 * @param[in] client: the ion client
 * @param[in] buffer: the buffer pointer
 *
 * @retval "=-12" create the ion handle failed, lack memory
 * @retval correct_ptr create the ion handle succeed
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
static struct ion_handle *ion_handle_create(struct ion_client *client,
				     struct ion_buffer *buffer)
{
	struct ion_handle *handle;

	handle = kzalloc(sizeof(*handle), GFP_KERNEL);
	if (!handle)
		return ERR_PTR(-ENOMEM);
	kref_init(&handle->ref);
	RB_CLEAR_NODE(&handle->node);
	handle->client = client;
	ion_buffer_get(buffer);
	ion_buffer_add_to_handle(buffer);
	handle->buffer = buffer;
	handle->import_cnt++;
	handle->import_consume_cnt = 0;

	return handle;
}

static void ion_handle_kmap_put(struct ion_handle *);

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief destroy the ion handle for the buffer
 *
 * @param[in] kref: the kref structure for reference count
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
static void ion_handle_destroy(struct kref *kref)
{
	struct ion_handle *handle = container_of(kref, struct ion_handle, ref);
	struct ion_client *client = handle->client;
	struct ion_buffer *buffer = handle->buffer;
	struct ion_share_handle *share_hd;
	struct ion_device *dev = client->dev;

	if (handle->share_id != 0) {
		mutex_lock(&dev->share_lock);
		share_hd = idr_find(&dev->idr, handle->share_id);
		mutex_unlock(&dev->share_lock);
		if (IS_ERR_OR_NULL(share_hd)) {
			pr_err("%s: find ion share handle failed [%ld].\n",
					__func__, PTR_ERR(share_hd));
		} else {
			while (handle->import_cnt) {
				ion_handle_import_put(handle);
			}
			while (handle->import_consume_cnt) {
				ion_handle_import_consume_put(handle);
			}
			ion_share_handle_put(share_hd);
		}
	}
	mutex_lock(&buffer->lock);
	while (handle->kmap_cnt)
		ion_handle_kmap_put(handle);
	mutex_unlock(&buffer->lock);

	idr_remove(&client->idr, handle->id);
	if (!RB_EMPTY_NODE(&handle->node))
		rb_erase(&handle->node, &client->handles);

	ion_buffer_remove_from_handle(buffer);
	ion_buffer_put(buffer);

	kfree(handle);
}

struct ion_buffer *ion_handle_buffer(struct ion_handle *handle)
{
	return handle->buffer;
}

static void ion_handle_get(struct ion_handle *handle)
{
	kref_get(&handle->ref);
}

/* Must hold the client lock */
/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief Check whether the reference of the handle will overflow
 *
 * @param[in] handle: the ion handle
 *
 * @retval handle: will not overflow
 * @retval others: will overflow
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
static struct ion_handle* ion_handle_get_check_overflow(struct ion_handle *handle)
{
	if (kref_read(&handle->ref) + 1 == 0)
		return ERR_PTR(-EOVERFLOW);
	ion_handle_get(handle);
	return handle;
}

static int ion_handle_put_nolock(struct ion_handle *handle)
{
	int ret;

	ret = kref_put(&handle->ref, ion_handle_destroy);

	return ret;
}

static int ion_handle_put(struct ion_handle *handle)
{
	struct ion_client *client = handle->client;
	int ret;

	mutex_lock(&client->lock);
	ret = ion_handle_put_nolock(handle);
	mutex_unlock(&client->lock);

	return ret;
}

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief lockup the ion handle in client by ion buffer
 *
 * @param[in] client: the ion client structure
 * @param[in] buffer: the ion buffer structure
 *
 * @retval ion_handle: success
 * @retval ERR_PTR: fail
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
static struct ion_handle *ion_handle_lookup(struct ion_client *client,
					    struct ion_buffer *buffer)
{
	struct rb_node *n = client->handles.rb_node;

	while (n) {
		struct ion_handle *entry = rb_entry(n, struct ion_handle, node);

		if (buffer < entry->buffer)
			n = n->rb_left;
		else if (buffer > entry->buffer)
			n = n->rb_right;
		else
			return entry;
	}
	return ERR_PTR(-EINVAL);
}

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief Check whether the reference of the handle will overflow by ion handle id
 *
 * @param[in] client: the ion client structure
 * @param[in] id: the ion handle id
 *
 * @retval handle: will not overflow
 * @retval others: will overflow
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
struct ion_handle *ion_handle_get_by_id_nolock(struct ion_client *client,
						      int id)
{
	struct ion_handle *handle;

	handle = idr_find(&client->idr, id);
	if (handle)
		return ion_handle_get_check_overflow(handle);

	return ERR_PTR(-EINVAL);
}
EXPORT_SYMBOL(ion_handle_get_by_id_nolock);

static struct ion_handle *ion_handle_get_by_id(struct ion_client *client,
						int id)
{
	struct ion_handle *handle;

	mutex_lock(&client->lock);
	handle = ion_handle_get_by_id_nolock(client, id);
	mutex_unlock(&client->lock);

	return handle ? handle : ERR_PTR(-EINVAL);
}

static bool ion_handle_validate(struct ion_client *client,
				struct ion_handle *handle)
{
	WARN_ON(!mutex_is_locked(&client->lock));
	return idr_find(&client->idr, handle->id) == handle;
}

static int ion_handle_add(struct ion_client *client, struct ion_handle *handle)
{
	int id;
	struct rb_node **p = &client->handles.rb_node;
	struct rb_node *parent = NULL;
	struct ion_handle *entry;

	id = idr_alloc(&client->idr, handle, 1, 0, GFP_KERNEL);
	if (id < 0) {
		pr_err("%s: failed alloc idr [%d]\n", __func__, id);
		return id;
	}

	handle->id = id;

	while (*p) {
		parent = *p;
		entry = rb_entry(parent, struct ion_handle, node);

		if (handle->buffer < entry->buffer)
			p = &(*p)->rb_left;
		else if (handle->buffer > entry->buffer)
			p = &(*p)->rb_right;
		else
			WARN(1, "%s: buffer already found.", __func__);
	}

	rb_link_node(&handle->node, parent, p);
	rb_insert_color(&handle->node, &client->handles);

	return 0;
}

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief add the ion share handle to dev and ion handle
 *
 * @param[in] dev: the ion device
 * @param[in] share_hd: the ion share handle
 * @param[in] handle: the ion handle
 *
 * @retval "<0": alloc idr failed
 * @retval "=0": succeed
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
static int ion_share_handle_add(struct ion_device *dev, struct ion_share_handle * share_hd,
				struct ion_handle *handle)
{
	struct rb_node *parent = NULL;
	struct rb_node **p;
	struct ion_share_handle *entry;
	int id;

	id = idr_alloc(&dev->idr, share_hd, 1, 0, GFP_KERNEL);
	if (id < 0) {
		pr_err("%s: failed alloc idr [%d]\n", __func__, id);
		return id;
	}
	share_hd->id = id;
	share_hd->buffer->share_id = id;
	handle->share_id = id;
	handle->sh_hd = share_hd;

	p = &dev->share_buffers.rb_node;
	while (*p) {
		parent = *p;
		entry = rb_entry(parent, struct ion_share_handle, node);

		if (share_hd < entry)
			p = &(*p)->rb_left;
		else if (share_hd > entry)
			p = &(*p)->rb_right;
	}
	rb_link_node(&share_hd->node, parent, p);
	rb_insert_color(&share_hd->node, &dev->share_buffers);

	return 0;
}

static int32_t ion_check_import_phys(struct ion_buffer *buffer, struct ion_share_handle_data *data)
{
	int32_t ret = 0;
	size_t len = 0, in_size = data->size;
	phys_addr_t phys_addr = 0, in_phys = data->phys_addr;

	if (!in_size) {
		pr_err("%s(%d): invalid size parameter\n", __func__, __LINE__);
		return -EINVAL;
	}

	ret = buffer->heap->ops->phys(buffer->heap, buffer, &phys_addr, &len);

	//1. whether warp
	if (phys_addr + len > phys_addr) {
		//2. contigious buffer check
		if ((in_phys < phys_addr) || ((in_phys + in_size) <= phys_addr) ||
			((in_phys + in_size) > (phys_addr + len)) ||
			(in_phys >= (phys_addr + len))) {
			pr_err("%s(%d): Invalid import buffer physical address 0x%llx, size %lu.,Should be 0x%llx and %lu.\n",
				__func__, __LINE__, in_phys, in_size, phys_addr, len);
			return -EINVAL;
		}
	} else if (buffer->priv_buffer) {
		//3. sg warp buffer check，normal buffer will not warp
		if (((in_phys < phys_addr) && (in_phys >= (phys_addr + len))) ||
		(((in_phys + in_size) <= phys_addr) && ((in_phys + in_size) > (phys_addr + len)))) {
			pr_err("%s(%d): Invalid import buffer physical address 0x%llx, size %lu. sg buffer,"
				"Should be 0x%llx and %lu.\n", __func__, __LINE__, in_phys, in_size, phys_addr, len);
			return -EINVAL;
		}
	}

	data->size = len;

	return ret;
}

/**
 * @NO{S21E04C01U}
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
struct ion_handle *ion_import_dma_buf_with_shareid(struct ion_client *client, int32_t share_id)
{
	struct ion_buffer *buffer;
	struct ion_handle *handle;
	struct ion_device *dev;
	struct ion_share_handle *share_hd;
	int ret;

	if (!client) {
		pr_err("%s(%d): client struct is NULL\n", __func__, __LINE__);
		return ERR_PTR(-EINVAL);
	}
	dev = client->dev;

	mutex_lock(&dev->share_lock);
	share_hd = idr_find(&dev->idr, share_id);
	if (IS_ERR_OR_NULL(share_hd)) {
		mutex_unlock(&dev->share_lock);
		pr_err("%s: find ion share handle failed [%ld].\n",
				__func__, PTR_ERR(share_hd));
		return share_hd ? ERR_CAST(share_hd): ERR_PTR(-EINVAL);
	}
	buffer = share_hd->buffer;
	ion_buffer_get(buffer);
	ion_share_handle_get(share_hd);
	mutex_unlock(&dev->share_lock);

	mutex_lock(&client->lock);
	/* if a handle exists for this buffer just take a reference to it */
	handle = ion_handle_lookup(client, buffer);
	if (!IS_ERR(handle)) {
		ion_handle_get(handle);
		ion_handle_import_get(handle);
		mutex_unlock(&client->lock);
		ion_share_handle_put(share_hd);
		ion_buffer_put(buffer);
		return handle;
	}

	handle = ion_handle_create(client, buffer);
	if (IS_ERR(handle)) {
		mutex_unlock(&client->lock);
		ion_share_handle_put(share_hd);
		ion_buffer_put(buffer);
		return handle;
	}

	ret = ion_handle_add(client, handle);
	if (ret == 0) {
		handle->share_id = share_id;
		handle->sh_hd = share_hd;
		ion_share_handle_add_to_handle(handle->sh_hd);
	}

	mutex_unlock(&client->lock);
	if (ret) {
		ion_handle_put(handle);
		handle = ERR_PTR(ret);
		ion_share_handle_put(share_hd);
		ion_buffer_put(buffer);
		return handle;
	}
	ion_buffer_put(buffer);

	return handle;
}
EXPORT_SYMBOL(ion_import_dma_buf_with_shareid);

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief use ion share handle to import dma buffer
 *
 * @param[in] client: the ion client
 * @param[in] data: the data from user space
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
struct ion_handle *ion_share_handle_import_dma_buf(struct ion_client *client,
					struct ion_share_handle_data *data)
{
	struct ion_buffer *buffer;
	struct ion_handle *handle;
	struct ion_device *dev = client->dev;
	struct ion_share_handle *share_hd;
	int share_id = data->sh_handle;
	int ret;

	mutex_lock(&dev->share_lock);
	share_hd = idr_find(&dev->idr, share_id);
	if (IS_ERR_OR_NULL(share_hd)) {
		mutex_unlock(&dev->share_lock);
		pr_err("%s: find ion share handle failed [%ld].\n",
				__func__, PTR_ERR(share_hd));
		return share_hd ? ERR_CAST(share_hd): ERR_PTR(-EINVAL);
	}
	buffer = share_hd->buffer;
	data->flags = buffer->flags;
	ion_buffer_get(buffer);
	ion_share_handle_get(share_hd);
	mutex_unlock(&dev->share_lock);

	//check the physical addr
	ret = ion_check_import_phys(buffer, data);
	if (ret != 0) {
		pr_err("%s:Invalid import buffer physical address check failed\n", __func__);
		ion_share_handle_put(share_hd);
		ion_buffer_put(buffer);
		return ERR_PTR(-EINVAL);
	}

	mutex_lock(&client->lock);
	/* if a handle exists for this buffer just take a reference to it */
	handle = ion_handle_lookup(client, buffer);
	if (!IS_ERR(handle)) {
		ion_handle_get(handle);
		ion_handle_import_get(handle);
		mutex_unlock(&client->lock);
		ion_share_handle_put(share_hd);
		ion_buffer_put(buffer);
		return handle;
	}

	handle = ion_handle_create(client, buffer);
	if (IS_ERR(handle)) {
		mutex_unlock(&client->lock);
		ion_share_handle_put(share_hd);
		ion_buffer_put(buffer);
		return handle;
	}

	ret = ion_handle_add(client, handle);
	if (ret == 0) {
		handle->share_id = share_id;
		handle->sh_hd = share_hd;
		ion_share_handle_add_to_handle(handle->sh_hd);
	}

	mutex_unlock(&client->lock);
	if (ret) {
		ion_handle_put(handle);
		handle = ERR_PTR(ret);
		ion_share_handle_put(share_hd);
		ion_buffer_put(buffer);
		return handle;
	}

	ion_buffer_put(buffer);
	return handle;
}

/* this function should only be called while dev->lock is held */
/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief create an buffer which alloc memory form SRAM have two scatterlist
 *
 * @param[in] heap: the ion heap structure.
 * @param[in] dev: the ion device structure.
 * @param[in] len: the buffer size
 * @param[in] align: the alloc alignment
 * @param[in] flags: the alloc flags
 *
 * @retval "ion_buffer": the ion buffer structure
 * @retval "ERR_PTR": failed
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
static struct ion_buffer *ion_sg_buffer_create(struct ion_heap *heap,
				     struct ion_device *dev,
				     unsigned long len,
				     unsigned long align,
				     unsigned long flags)
{
	struct ion_buffer *buffer[ION_SRAM_SG_NUM];
	int32_t ret, i;
	uint32_t avail_mem = 0, max_contigous = 0, res_mem = 0;
	struct sg_table *table;
	struct page *page[ION_SRAM_SG_NUM];
	uint32_t size[ION_SRAM_SG_NUM], heap_id_mask = 0;
	struct scatterlist *sg;

	ret = get_carveout_info(heap, &avail_mem, &max_contigous);
	if (avail_mem == 0) {
		pr_err("%s: ion sram heap avail memory is %x\n", __func__, avail_mem);
		return ERR_PTR(-ENOMEM);
	}

	buffer[ION_SRAM_SG_FIR] = ion_buffer_create(heap, dev, max_contigous, align, flags);
	if (IS_ERR_OR_NULL(buffer[ION_SRAM_SG_FIR])) {
		pr_err("%s: ion alloc memory from sram for scatterlist failed [%p]\n", __func__, buffer[ION_SRAM_SG_FIR]);
		return ERR_PTR(-ENOMEM);
	}

	res_mem = len - max_contigous;
	buffer[ION_SRAM_SG_SEC] = ion_buffer_create(heap, dev, res_mem, align, flags);
	if (IS_ERR_OR_NULL(buffer[ION_SRAM_SG_SEC])) {
		heap_id_mask |= ((ION_HEAP_CARVEOUT_MASK | ION_HEAP_TYPE_DMA_MASK) | ION_HEAP_TYPE_CMA_RESERVED_MASK);
		plist_for_each_entry(heap, &dev->heaps, node) {
			if (!((1 << heap->type) & heap_id_mask))
				continue;
			buffer[ION_SRAM_SG_SEC] = ion_buffer_create(heap, dev, res_mem, align, flags);
			if (!IS_ERR_OR_NULL(buffer[ION_SRAM_SG_SEC]))
				break;
		}
		if (IS_ERR_OR_NULL(buffer[ION_SRAM_SG_SEC])) {
			pr_err("%s: ion alloc memory from other heap failed [%p]\n", __func__, buffer[ION_SRAM_SG_SEC]);
			ion_buffer_put(buffer[ION_SRAM_SG_FIR]);
			return ERR_PTR(-ENOMEM);
		}
		pr_info("alloc from other heap success, the size is %lx, the heap type is %d, the heap name is %s\n",
					buffer[ION_SRAM_SG_SEC]->size, buffer[ION_SRAM_SG_SEC]->heap->type, buffer[ION_SRAM_SG_SEC]->heap->name);
	}

	for (i = 0; i < ION_SRAM_SG_NUM; i++) {
		page[i] = sg_page(buffer[i]->sg_table->sgl);
		size[i] = buffer[i]->size;
	}

	sg_free_table(buffer[ION_SRAM_SG_FIR]->sg_table);

	table = kmalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!table) {
		ion_buffer_put(buffer[ION_SRAM_SG_SEC]);
		ion_buffer_put(buffer[ION_SRAM_SG_FIR]);
		pr_err("%s: alloc table structure failed\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	ret = sg_alloc_table(table, ION_SRAM_SG_NUM, GFP_KERNEL);
	if (ret) {
		kfree(table);
		ion_buffer_put(buffer[ION_SRAM_SG_SEC]);
		ion_buffer_put(buffer[ION_SRAM_SG_FIR]);
		pr_err("%s: sg alloc table failed\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	for_each_sg(table->sgl, sg, table->nents, i) {
		sg_set_page(sg, page[i], size[i], 0);
	}

	buffer[ION_SRAM_SG_FIR]->priv_virt = table;
	buffer[ION_SRAM_SG_FIR]->sg_table = table;
	buffer[ION_SRAM_SG_FIR]->priv_buffer = (void *)buffer[ION_SRAM_SG_SEC];
	buffer[ION_SRAM_SG_FIR]->size = size[ION_SRAM_SG_FIR] + size[ION_SRAM_SG_SEC];

	//the seconed buffer handle count is always 1
	ion_buffer_add_to_handle(buffer[ION_SRAM_SG_SEC]);

	for_each_sg(buffer[ION_SRAM_SG_FIR]->sg_table->sgl, sg, table->nents, i) {
		pr_info("%d:the alloc size is %x\n", i, sg->length);
	}

	return buffer[ION_SRAM_SG_FIR];
}

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
			     unsigned int flags)
{
	struct ion_handle *handle;
	struct ion_device *dev;
	struct ion_buffer *buffer = NULL;
	struct ion_heap *heap;
	uint32_t last_heap_id_mask = 0;
	int32_t ret;
	int32_t heap_march = 0;
	int32_t type = 0;
	struct ion_share_handle * share_hd;
	struct ion_buffer *private_buffer;

	if (!client) {
		pr_err("%s: client cannot be null\n", __func__);
		return ERR_PTR(-EINVAL);
	}

	dev = client->dev;

	/*
	 * traverse the list of heaps available in this system in priority
	 * order.  If the heap type is supported by the client, and matches the
	 * request of the caller allocate from it.  Repeat until allocate has
	 * succeeded or all heaps have been tried
	 */
	len = PAGE_ALIGN(len);

	if (!len) {
		pr_err("%s: len invalid\n", __func__);
		return ERR_PTR(-EINVAL);
	}

	down_read(&dev->lock);
	type = flags >> 16;
	flags = flags&0xffff;
	/* bpu default not use cma heap */
	if (type == 0) {
		heap_id_mask = ION_HEAP_TYPE_CMA_RESERVED_MASK;
		plist_for_each_entry(heap, &dev->heaps, node) {
			/* if the caller didn't specify this heap id */
			if ((1 << heap->type) & heap_id_mask) {
				heap_march = 1;
			}
		}
		/* if no cma reserved heap, use cma*/
		if (heap_march == 0) {
			heap_id_mask &= ~ION_HEAP_TYPE_CMA_RESERVED_MASK;
			heap_id_mask |= ION_HEAP_TYPE_DMA_MASK;
		}
	} else {
		plist_for_each_entry(heap, &dev->heaps, node) {
			/* if the caller didn't specify this heap id */
			if ((1 << heap->type) & heap_id_mask) {
				heap_march = 1;
			}
		}
		/* if no carveout heap, use cma */
		if (heap_march == 0) {
			heap_id_mask |= ION_HEAP_TYPE_DMA_MASK;
		}
	}
	plist_for_each_entry(heap, &dev->heaps, node) {
		/* if the caller didn't specify this heap id */
		if (!((1 << heap->type) & heap_id_mask))
			continue;
		buffer = ion_buffer_create(heap, dev, len, align, flags);
		if (IS_ERR(buffer) && ((1 << heap->type) & ION_HEAP_TYPE_CUSTOM_MASK)) {
			if ((flags & ION_FLAG_USE_POOL) != 0) {
				pr_err("%s: alloc from sram heap for pool failed, sg not support for memory pool now\n", __func__);
				return ERR_PTR(-ENOMEM);
			}

			pr_info("%s: alloc from sram heap failed, ready to alloc sg\n", __func__);
			buffer = ion_sg_buffer_create(heap, dev, len, align, flags);
		} else {
			break;
		}
		if (IS_ERR(buffer)) {
			pr_err("%s: buffer create failed\n", __func__);
			up_read(&dev->lock);
			return ERR_PTR(-ENOMEM);
		}
		//initial the child buffer paramters
		private_buffer = (struct ion_buffer *)buffer->priv_buffer;
		private_buffer->private_flags = type;
		get_task_comm(private_buffer->task_comm, current->group_leader);
		private_buffer->pid = task_pid_nr(current);
	}
	/* if carveout/cma reserved can't alloc the mem, try use cma*/
	if ((buffer == NULL) || IS_ERR(buffer)) {
		if ((heap_id_mask & ION_HEAP_CARVEOUT_MASK) > 0) {
			pr_debug("Retry alloc carveout 0x%lxByte from cma reserved heap\n", len);
			last_heap_id_mask = heap_id_mask;
			heap_id_mask &= ~ION_HEAP_CARVEOUT_MASK;
			heap_id_mask |= ION_HEAP_TYPE_CMA_RESERVED_MASK;
		} else if ((heap_id_mask & ION_HEAP_TYPE_CMA_RESERVED_MASK) > 0) {
			pr_debug("Retry alloc cma reserved 0x%lxByte from carveout heap\n", len);
			last_heap_id_mask = heap_id_mask;
			heap_id_mask &= ~ION_HEAP_TYPE_CMA_RESERVED_MASK;
			heap_id_mask |= ION_HEAP_CARVEOUT_MASK;
		} else if ((heap_id_mask & ION_HEAP_TYPE_DMA_MASK) > 0) {
			pr_debug("Retry alloc cma  0x%lxByte from cma reserved heap\n", len);
			last_heap_id_mask = heap_id_mask;
			heap_id_mask &= ~ION_HEAP_TYPE_DMA_MASK;
			heap_id_mask |= ION_HEAP_TYPE_CMA_RESERVED_MASK;
		}
		plist_for_each_entry(heap, &dev->heaps, node) {
			/* if the caller didn't specify this heap id */
			if (!((1 << heap->type) & heap_id_mask))
				continue;
			buffer = ion_buffer_create(heap, dev, len, align, flags);
			if (!IS_ERR(buffer))
				break;
		}
	}

	if ((buffer == NULL) || IS_ERR(buffer)) {
		if ((last_heap_id_mask & ION_HEAP_TYPE_CMA_RESERVED_MASK) > 0) {
			pr_debug("Retry cma reserved alloc 0x%lxByte from cma heap\n", len);
			heap_id_mask &= ~ION_HEAP_CARVEOUT_MASK;
			heap_id_mask |= ION_HEAP_TYPE_DMA_MASK;
		} else if ((last_heap_id_mask & ION_HEAP_CARVEOUT_MASK) > 0) {
			pr_debug("Retry alloc carveout 0x%lxByte from cma heap\n", len);
			heap_id_mask &= ~ION_HEAP_TYPE_CMA_RESERVED_MASK;
			heap_id_mask |= ION_HEAP_TYPE_DMA_MASK;
		} else if ((last_heap_id_mask & ION_HEAP_TYPE_DMA_MASK) > 0) {
			pr_debug("Retry alloc carveout 0x%lxByte from carveout heap\n", len);
			heap_id_mask &= ~ION_HEAP_TYPE_CMA_RESERVED_MASK;
			heap_id_mask |= ION_HEAP_CARVEOUT_MASK;
		}
		plist_for_each_entry(heap, &dev->heaps, node) {
			/* if the caller didn't specify this heap id */
			if (!((1 << heap->type) & heap_id_mask))
				continue;
			buffer = ion_buffer_create(heap, dev, len, align, flags);
			if (!IS_ERR(buffer))
				break;
		}
	}
	up_read(&dev->lock);

	if (buffer == NULL) {
		pr_err("%s: buffer create failed\n", __func__);
		return ERR_PTR(-ENODEV);
	}

	if (IS_ERR(buffer)) {
		pr_err("%s: buffer create error[%ld]\n",
				__func__, PTR_ERR(buffer));
		return ERR_CAST(buffer);
	}
	buffer->private_flags = type;

	get_task_comm(buffer->task_comm, current->group_leader);
	buffer->pid = task_pid_nr(current);

	handle = ion_handle_create(client, buffer);

	/*
	 * ion_buffer_create will create a buffer with a ref_cnt of 1,
	 * and ion_handle_create will take a second reference, drop one here
	 */
	ion_buffer_put(buffer);

	if (IS_ERR(handle)) {
		pr_err("%s: handle create error[%ld]\n",
				__func__, PTR_ERR(handle));
		return handle;
	}

	share_hd = ion_share_handle_create(dev, buffer);
	if (IS_ERR(share_hd)) {
		ion_handle_put(handle);
		pr_err("%s: handle create error[%ld]\n",
				__func__, PTR_ERR(share_hd));
		return ERR_CAST(share_hd);
	}

	mutex_lock(&dev->share_lock);
	ret = ion_share_handle_add(dev, share_hd, handle);
	mutex_unlock(&dev->share_lock);
	if (ret) {
		pr_err("%s: share handle add failed[%d]\n", __func__, ret);
		ion_share_handle_put(share_hd);
		ion_handle_put(handle);
		handle = ERR_PTR(ret);
		return handle;
	}

	mutex_lock(&client->lock);
	ret = ion_handle_add(client, handle);
	mutex_unlock(&client->lock);
	if (ret) {
		pr_err("%s: handle add failed[%d]\n", __func__, ret);
		ion_handle_put(handle);
		handle = ERR_PTR(ret);
	}

	return handle;
}
EXPORT_SYMBOL(ion_alloc);

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief unregister all the share memory pool buffer
 *
 * @param[in] client: the ion client
 *
 * @retval "0": succeed
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
static int share_pool_unregister_all(struct ion_client *client)
{
	struct rb_node *n;
	struct ion_share_pool_buf *entry;
	struct ion_device *dev = client->dev;

	for (n = rb_first(&dev->share_pool_buffers); n; n = rb_next(n)) {
		entry = rb_entry(n, struct ion_share_pool_buf, node);
		if (entry->client == client) {
			rb_erase(&entry->node, &dev->share_pool_buffers);
			kfree(entry);
		}
	}

	return 0;
}

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief add the share pool buffer to device rbtree
 *
 * @param[in] dev: the ion device structure
 * @param[in] share_pool_buf: the share pool buffer structure
 *
 * @retval "0": succeed
 * @retval "<0": Linux errno
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
static int ion_share_pool_buf_add(
	struct ion_device *dev,	struct ion_share_pool_buf * share_pool_buf)
{
	struct rb_node *parent = NULL;
	struct rb_node **p;
	struct ion_share_pool_buf *entry;

	p = &dev->share_pool_buffers.rb_node;
	while (*p) {
		parent = *p;
		entry = rb_entry(parent, struct ion_share_pool_buf, node);

		if (share_pool_buf->share_id < entry->share_id) {
			p = &(*p)->rb_left;
		} else if (share_pool_buf->share_id > entry->share_id) {
			p = &(*p)->rb_right;
		} else {
			pr_info("ion_share_pool_buf_add %d failed\n", share_pool_buf->share_id);
			return -EINVAL;
		}
	}
	rb_link_node(&share_pool_buf->node, parent, p);
	rb_insert_color(&share_pool_buf->node, &dev->share_pool_buffers);

	return 0;
}

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief create a share pool buffer
 *
 * @param[in] dev: the ion device structure
 * @param[in] buffer: the ion buffer structure
 * @param[in] client: the ion client structure
 * @param[in] share_fd: the share fd of the ion buffer
 * @param[in] share_id: the share id of the ion share handle structure
 *
 * @retval "0": succeed
 * @retval "<0": Linux errno
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
static struct ion_share_pool_buf *ion_share_pool_buf_create(
		struct ion_device *dev,	struct ion_buffer *buffer,
		struct ion_client *client, int32_t share_fd, int32_t share_id)
{
	struct ion_share_pool_buf *share_pool_buf;

	share_pool_buf = kzalloc(sizeof(struct ion_share_pool_buf), GFP_KERNEL);
	if (!share_pool_buf)
		return ERR_PTR(-ENOMEM);

	kref_init(&share_pool_buf->ref);
	share_pool_buf->dev = dev;
	share_pool_buf->buffer = buffer;
	share_pool_buf->client = client;
	share_pool_buf->share_fd = share_fd;
	share_pool_buf->share_id = share_id;

	return share_pool_buf;
}

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief create a share pool buffer
 *
 * @param[in] kref: the reference structure
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
static void ion_share_pool_buf_destroy(struct kref *kref)
{
	struct ion_share_pool_buf *share_pool_buf =
			container_of(kref, struct ion_share_pool_buf, ref);
	struct ion_device *dev = share_pool_buf->dev;

	if (!RB_EMPTY_NODE(&share_pool_buf->node))
		rb_erase(&share_pool_buf->node, &dev->share_pool_buffers);

	kfree(share_pool_buf);
}

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief decrease the share pool buffer reference
 *
 * @param[in] share_pool_buf: the share pool buffer
 *
 * @retval "0": succeed
 * @retval "<0": Linux errno
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
static int ion_share_pool_buf_put(struct ion_share_pool_buf *share_pool_buf)
{
	struct ion_device *dev = share_pool_buf->dev;
	int ret;
	mutex_lock(&dev->share_lock);
	ret = kref_put(&share_pool_buf->ref, ion_share_pool_buf_destroy);
	mutex_unlock(&dev->share_lock);

	return ret;
}

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief find the share pool buffer in ion device by share id
 *
 * @param[in] dev: the ion device structure
 * @param[in] share_id: the ion share id of the ion share handle
 *
 * @retval "ion_share_pool_buf": succeed
 * @retval "ERR_PTR": fail
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
static struct ion_share_pool_buf *ion_share_pool_buf_lookup(
	struct ion_device *dev, int32_t share_id)
{
	struct ion_share_pool_buf *entry;
	struct rb_node *n = dev->share_pool_buffers.rb_node;

	while (n) {
		entry = rb_entry(n, struct ion_share_pool_buf, node);
		if (share_id < entry->share_id)
			n = n->rb_left;
		else if (share_id > entry->share_id)
			n = n->rb_right;
		else
			return entry;
	}
	return ERR_PTR(-EINVAL);
}

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief find sync index
 *
 * @param[in] share_hd: the ion share handle
 *
 * @retval ">=0": succeed
 * @retval "-1": fail
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
static int32_t share_pool_sync_idx(struct ion_share_handle *share_hd)
{
	int32_t i;

	for (i = 0; i < MAXSYNCNUM; i++) {
		if (share_hd->share_pool_sync[i].thread_pid == current->pid) {
			return i;
		}
	}

	return -1;
}

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief notify for import count updating
 *
 * @param[in] dev: the ion device structure
 * @param[in] share_id: the share id of the ion share hanlde
 * @param[in] import_cnt: the import count
 * @param[in] timeout: the wait time
 * @param[in] retry: the flag for whether retry
 *
 * @retval "=0": succeed
 * @retval "<0": fail
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
static int32_t ion_share_pool_notify(struct ion_device *dev,
	int32_t share_id, int32_t import_cnt, int32_t timeout, int32_t retry)
{
	struct ion_share_pool_buf *share_pool_buf;
	int32_t buf[SPBUFNUM];
	int32_t ret = 0;
	int32_t idx = 0;
	int16_t *ptr = (int16_t *)&(buf[SPBUFREFCNT]);
	struct ion_share_handle *share_hd;

	mutex_lock(&dev->share_lock);
	share_pool_buf = ion_share_pool_buf_lookup(dev, share_id);
	if (IS_ERR_OR_NULL(share_pool_buf)) {
		mutex_unlock(&dev->share_lock);
		return 0;
	}
	share_hd = idr_find(&dev->idr, share_id);
	if (IS_ERR_OR_NULL(share_hd)) {
		mutex_unlock(&dev->share_lock);
		pr_err("%s: find ion share handle failed [%ld].\n",
				__func__, PTR_ERR(share_hd));
		return -EINVAL;
	}
	mutex_unlock(&dev->share_lock);

	// share_pool_wait_q timeout last notify
	if (retry > 0) {
		idx = share_pool_sync_idx(share_hd);
		if (idx >= 0 && idx < MAXSYNCNUM) {
			ret = wait_event_interruptible_timeout(share_hd->share_pool_sync[idx].share_pool_wait_q,
				share_hd->share_pool_sync[idx].share_pool_cnt == 1,	msecs_to_jiffies(timeout));
			if (ret == 0) {
				pr_info("%s:%d, pid: %d, timeout\n", __func__, __LINE__, current->pid);
				return -ETIMEDOUT;
			} else if (ret < 0) {
				pr_err("%s:%d, pid: %d, failed %d\n", __func__, __LINE__, current->pid, ret);
				return ret;
			}

			share_hd->share_pool_sync[idx].share_pool_cnt = 0;
			share_hd->share_pool_sync[idx].thread_pid = -1;
			(void)test_and_clear_bit(idx, share_hd->bitmap);
			return 0;
		}
	}

	if (timeout > 0) {
		mutex_lock(&dev->share_lock);
		idx = find_first_zero_bit(share_hd->bitmap, MAXSYNCNUM);
		if (idx < 0 || idx >= MAXSYNCNUM) {
			mutex_unlock(&dev->share_lock);
			pr_err("%s:%d cannot find sync wq, bitmap %lx\n", __func__, __LINE__, share_hd->bitmap[0]);
			return -EINVAL;
		}
		set_bit(idx, share_hd->bitmap);
		share_hd->share_pool_sync[idx].thread_pid = current->pid;
		share_hd->share_pool_sync[idx].share_pool_cnt = 0;
		mutex_unlock(&dev->share_lock);
	}

	mutex_lock(&dev->share_lock);
	buf[SPBUFFD] = share_pool_buf->share_fd;
	buf[SPBUFID] = share_pool_buf->share_id;
	ptr[0] = (int16_t)import_cnt;
	ptr[1] = idx;
	ret = kfifo_in_spinlocked(&share_pool_buf->client->fifo,
		buf, SPBUFNUM * sizeof(int32_t), &share_pool_buf->client->fifo_lock);
	if (ret != SPBUFNUM * sizeof(int32_t)) {
		mutex_unlock(&dev->share_lock);
		pr_err("%s:%d kfifo overflow.\n", __func__, __LINE__);
		return -EINVAL;
	}

	share_pool_buf->client->wq_cnt = 1;
	wake_up_interruptible(&share_pool_buf->client->wq);
	mutex_unlock(&dev->share_lock);

	if (timeout > 0) {
		ret = wait_event_interruptible_timeout(share_hd->share_pool_sync[idx].share_pool_wait_q,
			share_hd->share_pool_sync[idx].share_pool_cnt == 1, msecs_to_jiffies(timeout));
		if (ret == 0) {
			pr_debug("%s:%d timeout\n", __func__, __LINE__);
			return -ETIMEDOUT;
		} else if (ret < 0) {
			pr_debug("%s:%d failed %d\n", __func__, __LINE__, ret);
			return ret;
		}
		share_hd->share_pool_sync[idx].share_pool_cnt = 0;
		share_hd->share_pool_sync[idx].thread_pid = -1;
		(void)test_and_clear_bit(idx, share_hd->bitmap);
	}

	return 0;
}

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief wake up the reference count wait event
 *
 * @param[in] client: the ion client structure
 * @param[in] data: the ion share pool data from userspace
 *
 * @retval "=0": succeed
 * @retval "<0": fail
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
int32_t ion_share_pool_wake_up_ref_cnt(struct ion_client *client,
	struct ion_share_pool_data *data)
{
	struct ion_device *dev = client->dev;
	struct ion_share_handle *share_hd;
	int32_t share_id = data->sh_handle;
	int32_t idx = data->import_cnt;

	mutex_lock(&dev->share_lock);
	share_hd = idr_find(&dev->idr, share_id);
	if (IS_ERR_OR_NULL(share_hd)) {
		mutex_unlock(&dev->share_lock);
		pr_err("%s: find ion share handle failed [%ld].\n",
				__func__, PTR_ERR(share_hd));
		return -EINVAL;
	}

	if ((idx >=0) && (idx < MAXSYNCNUM)) {
		share_hd->share_pool_sync[idx].share_pool_cnt = 1;
		wake_up_interruptible(&share_hd->share_pool_sync[idx].share_pool_wait_q);
	}
	mutex_unlock(&dev->share_lock);

	return 0;
}

static void ion_free_nolock(struct ion_client *client,
			    struct ion_handle *handle)
{
	bool valid_handle;

	WARN_ON(client != handle->client);

	valid_handle = ion_handle_validate(client, handle);
	if (!valid_handle) {
		WARN(1, "%s: invalid handle passed to free.\n", __func__);
		return;
	}
	if ((handle->share_id != 0) && (handle->sh_hd != NULL)) {
		ion_handle_import_put(handle);
	}
	ion_handle_put_nolock(handle);
}

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
void ion_free(struct ion_client *client, struct ion_handle *handle)
{
	if (!client) {
		pr_err("%s: client cannot be null\n", __func__);
		return;
	}

	if (!handle) {
		pr_err("%s: handle cannot be null\n", __func__);
		return;
	}

	BUG_ON(client != handle->client);

	mutex_lock(&client->lock);
	ion_free_nolock(client, handle);
	mutex_unlock(&client->lock);
}
EXPORT_SYMBOL(ion_free);

//int ion_phys(struct ion_client *client, struct ion_handle *handle,
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
int ion_phys(struct ion_client *client, int handle_id,
	     phys_addr_t *addr, size_t *len)
{
	struct ion_buffer *buffer;
	int ret;
	struct ion_handle *handle;

	if (!client) {
		pr_err("%s: client cannot be null\n", __func__);
		return -EINVAL;
	}

	if (!addr) {
		pr_err("%s: addr cannot be null\n", __func__);
		return -EINVAL;
	}

	if (!len) {
		pr_err("%s: len cannot be null\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&client->lock);

	handle = ion_handle_get_by_id_nolock(client, handle_id);
	if (IS_ERR(handle)) {
		mutex_unlock(&client->lock);
		pr_err("%s: find ion handle failed [%ld].",
				__func__, PTR_ERR(handle));
		return PTR_ERR(handle);
	}

	buffer = handle->buffer;

	if (!buffer->heap->ops->phys) {
		pr_err("%s: ion_phys is not implemented by this heap (name=%s, type=%d).\n",
			__func__, buffer->heap->name, buffer->heap->type);
		ion_handle_put_nolock(handle);
		mutex_unlock(&client->lock);
		return -ENODEV;
	}
	ret = buffer->heap->ops->phys(buffer->heap, buffer, addr, len);
	ion_handle_put_nolock(handle);
	mutex_unlock(&client->lock);
	return ret;
}
EXPORT_SYMBOL(ion_phys);

static void *ion_buffer_kmap_get(struct ion_buffer *buffer)
{
	void *vaddr;

	if (buffer->kmap_cnt) {
		buffer->kmap_cnt++;
		return buffer->vaddr;
	}
	vaddr = buffer->heap->ops->map_kernel(buffer->heap, buffer);
	if (WARN_ONCE(!vaddr,
		      "heap->ops->map_kernel should return ERR_PTR on error"))
		return ERR_PTR(-EINVAL);
	if (IS_ERR(vaddr))
		return vaddr;
	buffer->vaddr = vaddr;
	buffer->kmap_cnt++;
	return vaddr;
}

static void *ion_handle_kmap_get(struct ion_handle *handle)
{
	struct ion_buffer *buffer = handle->buffer;
	void *vaddr;

	if (handle->kmap_cnt) {
		handle->kmap_cnt++;
		return buffer->vaddr;
	}
	vaddr = ion_buffer_kmap_get(buffer);
	if (IS_ERR(vaddr))
		return vaddr;
	handle->kmap_cnt++;
	return vaddr;
}

static void ion_buffer_kmap_put(struct ion_buffer *buffer)
{
	buffer->kmap_cnt--;
	if (!buffer->kmap_cnt) {
		buffer->heap->ops->unmap_kernel(buffer->heap, buffer);
		buffer->vaddr = NULL;
	}
}

static void ion_handle_kmap_put(struct ion_handle *handle)
{
	struct ion_buffer *buffer = handle->buffer;

	if (!handle->kmap_cnt) {
		WARN(1, "%s: Double unmap detected! bailing...\n", __func__);
		return;
	}
	handle->kmap_cnt--;
	if (!handle->kmap_cnt)
		ion_buffer_kmap_put(buffer);
}

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
void *ion_map_kernel(struct ion_client *client, struct ion_handle *handle)
{
	struct ion_buffer *buffer;
	void *vaddr;

	if (!client) {
		pr_err("%s: client cannot be null\n", __func__);
		return ERR_PTR(-EINVAL);
	}

	if (!handle) {
		pr_err("%s: handle cannot be null\n", __func__);
		return ERR_PTR(-EINVAL);
	}

	mutex_lock(&client->lock);
	if (!ion_handle_validate(client, handle)) {
		pr_err("%s: invalid handle passed to map_kernel.\n",
		       __func__);
		mutex_unlock(&client->lock);
		return ERR_PTR(-EINVAL);
	}

	buffer = handle->buffer;

	if (!handle->buffer->heap->ops->map_kernel) {
		pr_err("%s: map_kernel is not implemented by this heap.\n",
		       __func__);
		mutex_unlock(&client->lock);
		return ERR_PTR(-ENODEV);
	}

	mutex_lock(&buffer->lock);
	vaddr = ion_handle_kmap_get(handle);
	mutex_unlock(&buffer->lock);
	mutex_unlock(&client->lock);
	return vaddr;
}
EXPORT_SYMBOL(ion_map_kernel);

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
void ion_unmap_kernel(struct ion_client *client, struct ion_handle *handle)
{
	struct ion_buffer *buffer;

	if (!client) {
		pr_err("%s: client cannot be null\n", __func__);
		return;
	}

	if (!handle) {
		pr_err("%s: handle cannot be null\n", __func__);
		return;
	}

	mutex_lock(&client->lock);
	if (!ion_handle_validate(client, handle)) {
		pr_err("%s: invalid handle passed to unmap_kernel.\n",
		       __func__);
		mutex_unlock(&client->lock);
		return;
	}
	buffer = handle->buffer;
	mutex_lock(&buffer->lock);
	ion_handle_kmap_put(handle);
	mutex_unlock(&buffer->lock);
	mutex_unlock(&client->lock);
}
EXPORT_SYMBOL(ion_unmap_kernel);

/**
 * @NO{S21E04C02U}
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
uint32_t hbmem_flag_to_ion_flag(int64_t flags, uint32_t *heap_id_mask, int32_t *port)
{
	uint32_t alloc_flags = 0;
	int64_t user_flags = ((uint64_t)flags & HB_MEM_USAGE_TRIVIAL_MASK);
	uint32_t heap_mask;
	int64_t heap_flags = (uint64_t)flags & HB_MEM_USAGE_PRIV_MASK;
	int32_t map_prot = PROT_NONE;
	uint64_t port_flags = ((uint64_t)flags & HB_MEM_USAGE_CPU_READ_MASK) |
		((uint64_t)flags & HB_MEM_USAGE_CPU_WRITE_MASK);

	if (((uint64_t)user_flags & HB_MEM_USAGE_CACHED) != 0) {
		alloc_flags |= (ION_FLAG_CACHED | ION_FLAG_CACHED_NEEDS_SYNC);
	}

	if (((uint64_t)user_flags & HB_MEM_USAGE_MAP_INITIALIZED) != 0) {
		alloc_flags |= ION_FLAG_INITIALIZED;
	} else if (((uint64_t)user_flags & HB_MEM_USAGE_MAP_UNINITIALIZED) != 0) {
		alloc_flags |= ION_FLAG_UNINITIALIZED;
	}

/* 	if (((uint64_t)user_flags & HB_MEM_USAGE_MEM_POOL) != 0) {
		alloc_flags |= ION_FLAG_USE_POOL;
	} */

	// alloc flag's high 16 bit meaning:
	// xxxx (vpu/jpu type id)xxxx xx(vpu/jpu instance id)xx xxxx(vpu/jpu memory
	// type)
	//alloc_flags |= (ionType << 12 | instIndex << 6 | memoryType) << 16;

	// now we can only set the module type to bpu to make the heap mask work
	// we should fix this
	//get the ion alloc flags
	user_flags = ((uint64_t)flags & HB_MEM_USAGE_HW_MASK);
	switch (user_flags) {
		case (int64_t)HB_MEM_USAGE_HW_CIM:
			alloc_flags |= ((uint32_t)ION_MODULE_TYPE_INTERNAL << ION_MODULE_TYPE_BIT_SHIFT) |
				((uint32_t)ION_MEM_TYPE_CIM << ION_MEM_TYPE_BIT_SHIFT);
			break;
		case (int64_t)HB_MEM_USAGE_HW_PYRAMID:
			alloc_flags |= ((uint32_t)ION_MODULE_TYPE_INTERNAL << ION_MODULE_TYPE_BIT_SHIFT) |
				((uint32_t)ION_MEM_TYPE_PYRAMID << ION_MEM_TYPE_BIT_SHIFT);
			break;
		case (int64_t)HB_MEM_USAGE_HW_GDC:
			alloc_flags |= ((uint32_t)ION_MODULE_TYPE_INTERNAL << ION_MODULE_TYPE_BIT_SHIFT) |
				((uint32_t)ION_MEM_TYPE_GDC << ION_MEM_TYPE_BIT_SHIFT);
			break;
		case (int64_t)HB_MEM_USAGE_HW_STITCH:
			alloc_flags |= ((uint32_t)ION_MODULE_TYPE_INTERNAL << ION_MODULE_TYPE_BIT_SHIFT) |
				((uint32_t)ION_MEM_TYPE_STITCH << ION_MEM_TYPE_BIT_SHIFT);
			break;
		case (int64_t)HB_MEM_USAGE_HW_OPTICAL_FLOW:
			alloc_flags |= ((uint32_t)ION_MODULE_TYPE_INTERNAL << ION_MODULE_TYPE_BIT_SHIFT) |
				((uint32_t)ION_MEM_TYPE_OPTICAL_FLOW << ION_MEM_TYPE_BIT_SHIFT);
			break;
		case (int64_t)HB_MEM_USAGE_HW_BPU:
			alloc_flags |= ((uint32_t)ION_MODULE_TYPE_BPU << ION_MODULE_TYPE_BIT_SHIFT) |
				((uint32_t)ION_MEM_TYPE_BPU << ION_MEM_TYPE_BIT_SHIFT);
			break;
		case (int64_t)HB_MEM_USAGE_HW_ISP:
			alloc_flags |= ((uint32_t)ION_MODULE_TYPE_INTERNAL << ION_MODULE_TYPE_BIT_SHIFT) |
				((uint32_t)ION_MEM_TYPE_ISP << ION_MEM_TYPE_BIT_SHIFT);
			break;
		case (int64_t)HB_MEM_USAGE_HW_DISPLAY:
			alloc_flags |= ((uint32_t)ION_MODULE_TYPE_INTERNAL << ION_MODULE_TYPE_BIT_SHIFT) |
				((uint32_t)ION_MEM_TYPE_DISPLAY << ION_MEM_TYPE_BIT_SHIFT);
			break;
		case (int64_t)HB_MEM_USAGE_HW_VIDEO_CODEC:
			alloc_flags |= ((uint32_t)ION_MODULE_TYPE_INTERNAL << ION_MODULE_TYPE_BIT_SHIFT) |
				((uint32_t)ION_MEM_TYPE_VIDEO_CODEC << ION_MEM_TYPE_BIT_SHIFT);
			break;
		case (int64_t)HB_MEM_USAGE_HW_JPEG_CODEC:
			alloc_flags |= ((uint32_t)ION_MODULE_TYPE_INTERNAL << ION_MODULE_TYPE_BIT_SHIFT) |
				((uint32_t)ION_MEM_TYPE_JPEG_CODEC << ION_MEM_TYPE_BIT_SHIFT);
			break;
		case (int64_t)HB_MEM_USAGE_HW_VDSP:
			alloc_flags |= ((uint32_t)ION_MODULE_TYPE_INTERNAL << ION_MODULE_TYPE_BIT_SHIFT) |
				((uint32_t)ION_MEM_TYPE_VDSP << ION_MEM_TYPE_BIT_SHIFT);
			break;
		case (int64_t)HB_MEM_USAGE_HW_GDC_OUT:
			alloc_flags |= ((uint32_t)ION_MODULE_TYPE_INTERNAL << ION_MODULE_TYPE_BIT_SHIFT) |
				((uint32_t)ION_MEM_TYPE_GDC_OUT << ION_MEM_TYPE_BIT_SHIFT);
			break;
		case (int64_t)HB_MEM_USAGE_HW_IPC:
			alloc_flags |= ((uint32_t)ION_MODULE_TYPE_INTERNAL << ION_MODULE_TYPE_BIT_SHIFT) |
				((uint32_t)ION_MEM_TYPE_IPC << ION_MEM_TYPE_BIT_SHIFT);
			break;
		case (int64_t)HB_MEM_USAGE_HW_PCIE:
			alloc_flags |= ((uint32_t)ION_MODULE_TYPE_INTERNAL << ION_MODULE_TYPE_BIT_SHIFT) |
				((uint32_t)ION_MEM_TYPE_PCIE << ION_MEM_TYPE_BIT_SHIFT);
			break;
		case (int64_t)HB_MEM_USAGE_HW_YNR:
			alloc_flags |= ((uint32_t)ION_MODULE_TYPE_INTERNAL << ION_MODULE_TYPE_BIT_SHIFT) |
				((uint32_t)ION_MEM_TYPE_YNR << ION_MEM_TYPE_BIT_SHIFT);
			break;
		default:
			if ((uint64_t)flags >> PRIVATE_USER_MODULE_MEM_BIT_SHIFT != 0) {
				alloc_flags |= (uint32_t)(((uint64_t)flags >> PRIVATE_USER_MODULE_MEM_BIT_SHIFT) << ION_MEM_TYPE_BIT_SHIFT);
			} else {
				alloc_flags |= ((uint32_t)ION_MODULE_TYPE_INTERNAL << ION_MODULE_TYPE_BIT_SHIFT) |
					((uint32_t)ION_MEM_TYPE_OTHER << ION_MEM_TYPE_BIT_SHIFT);
			}
			break;
	}

	//get the heap id mask
	if (heap_flags == (int64_t)HB_MEM_USAGE_PRIV_HEAP_DMA) {/* PRQA S 2995,2991 */
		heap_mask = ION_HEAP_TYPE_DMA_MASK;
	} else if (((uint64_t)heap_flags & HB_MEM_USAGE_PRIV_HEAP_RESERVED) != 0) {
		heap_mask = ION_HEAP_CARVEOUT_MASK;
	} else if (((uint64_t)heap_flags & HB_MEM_USAGE_PRIV_HEAP_SRAM) != 0) {
		heap_mask = ION_HEAP_TYPE_CUSTOM_MASK;
	} else if (((uint64_t)heap_flags & HB_MEM_USAGE_PRIV_HEAP_2_RESERVED) != 0) {
		heap_mask = ION_HEAP_TYPE_CMA_RESERVED_MASK;
	} else if (((uint64_t)heap_flags & HB_MEM_USAGE_PRIV_HEAP_SRAM_LIMIT) != 0) {
		heap_mask = ION_HEAP_TYPE_SRAM_LIMIT_MASK;
	} else if (((uint64_t)heap_flags & HB_MEM_USAGE_PRIV_HEAP_INLINE_ECC) != 0) {
		heap_mask = ION_HEAP_TYPE_INLINE_ECC_MASK;
	} else {
		heap_mask = ION_HEAP_TYPE_DMA_MASK;
	}

	*heap_id_mask = heap_mask;

	//get the port for alloc buffer
	if (((port_flags & HB_MEM_USAGE_CPU_READ_OFTEN) != 0)) {
		map_prot = (uint32_t)map_prot | PROT_READ;
	}
	if ((port_flags & HB_MEM_USAGE_CPU_WRITE_OFTEN) != 0) {
		map_prot = (uint32_t)map_prot | PROT_WRITE;
	}

	*port = map_prot;

	return alloc_flags;
}
EXPORT_SYMBOL(hbmem_flag_to_ion_flag);

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
int64_t ion_flag_to_hbmem_flag(uint32_t ion_flag, uint32_t heap_id_mask, int64_t pool_flag, int64_t prot)
{
	int64_t flags = 0;
	uint32_t type = 0;
	//ion_flag<-->ion_flag in driver
		//get cache
	if((ion_flag & ION_FLAG_CACHED) && (ion_flag & ION_FLAG_CACHED_NEEDS_SYNC)) {
		flags = (uint64_t)flags | HB_MEM_USAGE_CACHED;
	}
		//get initial
	if(ion_flag & ION_FLAG_INITIALIZED) {
		flags = (uint64_t)flags | HB_MEM_USAGE_MAP_INITIALIZED;
	} else if(ion_flag & ION_FLAG_UNINITIALIZED) {
		flags = (uint64_t)flags | HB_MEM_USAGE_MAP_UNINITIALIZED;
	}
		//get module type
	type = (ion_flag >> ION_MEM_TYPE_BIT_SHIFT) & ION_MEM_TYPE_MASK;
	switch(type) {
		case ION_MEM_TYPE_CIM:
		{
			flags = (uint64_t)flags | HB_MEM_USAGE_HW_CIM;
			break;
		}
		case ION_MEM_TYPE_PYRAMID:
		{
			flags = (uint64_t)flags | HB_MEM_USAGE_HW_PYRAMID;
			break;
		}
		case ION_MEM_TYPE_GDC_OUT:
		{
			flags = (uint64_t)flags | HB_MEM_USAGE_HW_GDC_OUT;
			break;
		}
		case ION_MEM_TYPE_GDC:
		{
			flags = (uint64_t)flags | HB_MEM_USAGE_HW_GDC;
			break;
		}
		case ION_MEM_TYPE_STITCH:
		{
			flags = (uint64_t)flags | HB_MEM_USAGE_HW_STITCH;
			break;
		}
		case ION_MEM_TYPE_OPTICAL_FLOW:
		{
			flags = (uint64_t)flags | HB_MEM_USAGE_HW_OPTICAL_FLOW;
			break;
		}
		case ION_MEM_TYPE_BPU:
		{
			flags = (uint64_t)flags | HB_MEM_USAGE_HW_BPU;
			break;
		}
		case ION_MEM_TYPE_ISP:
		{
			flags = (uint64_t)flags | HB_MEM_USAGE_HW_ISP;
			break;
		}
		case ION_MEM_TYPE_DISPLAY:
		{
			flags = (uint64_t)flags | HB_MEM_USAGE_HW_DISPLAY;
			break;
		}
		case ION_MEM_TYPE_VIDEO_CODEC:
		{
			flags = (uint64_t)flags | HB_MEM_USAGE_HW_VIDEO_CODEC;
			break;
		}
		case ION_MEM_TYPE_JPEG_CODEC:
		{
			flags = (uint64_t)flags | HB_MEM_USAGE_HW_JPEG_CODEC;
			break;
		}
		case ION_MEM_TYPE_VDSP:
		{
			flags = (uint64_t)flags | HB_MEM_USAGE_HW_VDSP;
			break;
		}
		case ION_MEM_TYPE_IPC:
		{
			flags = (uint64_t)flags | HB_MEM_USAGE_HW_IPC;
			break;
		}
		case ION_MEM_TYPE_PCIE:
		{
			flags = (uint64_t)flags | HB_MEM_USAGE_HW_PCIE;
			break;
		}
		case ION_MEM_TYPE_YNR:
		{
			flags = (uint64_t)flags | HB_MEM_USAGE_HW_YNR;
			break;
		}
		default:
			break;
	}
	//heap_id_mask
	switch(heap_id_mask) {
		case ION_HEAP_CARVEOUT_MASK:
		{
			flags = (uint64_t)flags | HB_MEM_USAGE_PRIV_HEAP_RESERVED;
			break;
		}
		case ION_HEAP_TYPE_CUSTOM_MASK:
		{
			flags = (uint64_t)flags | HB_MEM_USAGE_PRIV_HEAP_SRAM;
			break;
		}
		case ION_HEAP_TYPE_CMA_RESERVED_MASK:
		{
			flags = (uint64_t)flags | HB_MEM_USAGE_PRIV_HEAP_2_RESERVED;
			break;
		}
		case ION_HEAP_TYPE_DMA_MASK:
		{
			flags = (uint64_t)flags | HB_MEM_USAGE_PRIV_HEAP_DMA;
			break;
		}
		case ION_HEAP_TYPE_SRAM_LIMIT_MASK:
		{
			flags = (uint64_t)flags | HB_MEM_USAGE_PRIV_HEAP_SRAM_LIMIT;
			break;
		}
		case ION_HEAP_TYPE_INLINE_ECC_MASK:
		{
			flags = (uint64_t)flags | HB_MEM_USAGE_PRIV_HEAP_INLINE_ECC;
			break;
		}
		default:
			break;
	}
	//pool_flag is from @mem_usage_t, whether use memory pool
	//prot if from @mem_usage_t，read and write flag
	if (prot & PROT_READ) {
		flags = (uint64_t)flags | HB_MEM_USAGE_CPU_READ_OFTEN;
	}
	if (prot & PROT_WRITE) {
		flags = (uint64_t)flags | HB_MEM_USAGE_CPU_WRITE_OFTEN;
	}

	if (pool_flag)
		flags = (uint64_t)flags | ((uint64_t)HB_MEM_USAGE_MEM_POOL);

	return flags;
}
EXPORT_SYMBOL(ion_flag_to_hbmem_flag);

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief show the client alloced buffer
 *
 * @param[in] s: Pointer to the kernel file structure
 * @param[in] unused: a unused pointer
 *
 * @retval "=0": succeed
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
static int ion_debug_client_show(struct seq_file *s, void *unused)
{
	struct ion_client *client = s->private;
	struct rb_node *n;
	size_t total_size = 0;

	seq_printf(s, "%16.16s: %16.16s : %16.16s : %16.16s : %16.16s : %16.16s : %16.16s : %16.20s\n",
		   "heap_name", "size_in_bytes", "handle refcount", "handle import",
		   "buffer ptr", "buffer refcount", "buffer share id",
		   "buffer share count");

	mutex_lock(&client->lock);
	for (n = rb_first(&client->handles); n; n = rb_next(n)) {
		struct ion_handle *handle = rb_entry(n, struct ion_handle,
						     node);
		struct ion_buffer *buffer = handle->buffer;

		if (handle->buffer->priv_buffer != NULL) {
			buffer = (struct ion_buffer *)handle->buffer->priv_buffer;
			//first print the sram information
			seq_printf(s, "%16.16s: %16zx : %16d : %16d : %16pK : %16d: %16d : %16d\n",
				handle->buffer->heap->name,
				handle->buffer->size - buffer->size,
				kref_read(&handle->ref),
				handle->import_cnt,
				handle->buffer,
				kref_read(&handle->buffer->ref),
				handle->share_id, handle->sh_hd->client_cnt);
			//then print the other heap information
		}

		seq_printf(s, "%16.16s: %16zx : %16d : %16d : %16pK : %16d: %16d : %16d\n",
			buffer->heap->name,
			buffer->size,
			kref_read(&handle->ref),
			handle->import_cnt,
			buffer,
			kref_read(&handle->buffer->ref),
			handle->share_id, handle->sh_hd->client_cnt);

		seq_puts(s, "\n");
		total_size += handle->buffer->size;
	}
	mutex_unlock(&client->lock);
	seq_puts(s, "-------------------------------------------------------------------------\n");
	seq_printf(s, "%16s %16zx\n", "total ", total_size);
	seq_puts(s, "-------------------------------------------------------------------------\n");
	return 0;
}

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief open the client debugfs file
 *
 * @param[in] inode: device inode
 * @param[in] file: the kernel file structure
 *
 * @retval "=0": succeed
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
static int ion_debug_client_open(struct inode *inode, struct file *file)
{
	return single_open(file, ion_debug_client_show, inode->i_private);
}

static const struct file_operations debug_client_fops = {
	.open = ion_debug_client_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief Check whether the client with the corresponding name already exists, add label if it exists
 *
 * @param[in] root: ion client inode
 * @param[in] name: client name
 *
 * @retval serial: same client name number
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
static int ion_get_client_serial(const struct rb_root *root,
					const unsigned char *name)
{
	int serial = -1;
	struct rb_node *node;

	for (node = rb_first(root); node; node = rb_next(node)) {
		struct ion_client *client = rb_entry(node, struct ion_client,
						node);

		if (strcmp(client->name, name))
			continue;
		serial = max(serial, client->display_serial);
	}
	return serial + 1;
}

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
				     const char *name)
{
	struct ion_client *client;
	struct task_struct *task;
	struct rb_node **p;
	struct rb_node *parent = NULL;
	struct ion_client *entry;
	pid_t pid;
	int32_t ret;

	if (!dev) {
		pr_err("%s: Dev cannot be null\n", __func__);
		return ERR_PTR(-EINVAL);
	}

	if (!name) {
		pr_err("%s: Name cannot be null\n", __func__);
		return ERR_PTR(-EINVAL);
	}

	get_task_struct(current->group_leader);
	task_lock(current->group_leader);
	pid = task_pid_nr(current->group_leader);
	/* don't bother to store task struct for kernel threads,
	 * they can't be killed anyway
	 */
	if (current->group_leader->flags & PF_KTHREAD) {
		put_task_struct(current->group_leader);
		task = NULL;
	} else {
		task = current->group_leader;
	}
	task_unlock(current->group_leader);

	client = kzalloc(sizeof(*client), GFP_KERNEL);
	if (!client)
		goto err_put_task_struct;

	client->dev = dev;
	client->handles = RB_ROOT;
	idr_init(&client->idr);
	mutex_init(&client->lock);
	client->task = task;
	client->pid = pid;
	client->name = kstrdup(name, GFP_KERNEL);
	if (!client->name)
		goto err_free_client;

	down_write(&dev->lock);
	client->display_serial = ion_get_client_serial(&dev->clients, name);
	client->display_name = kasprintf(
		GFP_KERNEL, "%s-%d", name, client->display_serial);
	if (!client->display_name) {
		up_write(&dev->lock);
		goto err_free_client_name;
	}
	p = &dev->clients.rb_node;
	while (*p) {
		parent = *p;
		entry = rb_entry(parent, struct ion_client, node);

		if (client < entry)
			p = &(*p)->rb_left;
		else if (client > entry)
			p = &(*p)->rb_right;
	}
	rb_link_node(&client->node, parent, p);
	rb_insert_color(&client->node, &dev->clients);

	client->debug_root = debugfs_create_file(client->display_name, 0664,
						dev->clients_debug_root,
						client, &debug_client_fops);
	if (!client->debug_root) {
		char buf[256], *path;

		path = dentry_path(dev->clients_debug_root, buf, 256);
		pr_err("Failed to create client debugfs at %s/%s\n",
			path, client->display_name);
	}
	init_waitqueue_head(&client->wq);
	client->wq_cnt = 0;
	ret = kfifo_alloc(&client->fifo, FIFONUM * sizeof(u32), GFP_KERNEL);
	if (ret) {
		up_write(&dev->lock);
		pr_err("%s: failed to create kfifo\n", __func__);
		goto err_free_client_name;
	}
	spin_lock_init(&client->fifo_lock);
	up_write(&dev->lock);

	return client;

err_free_client_name:
	kfree(client->name);
err_free_client:
	kfree(client);
err_put_task_struct:
	if (task)
		put_task_struct(current->group_leader);
	return ERR_PTR(-ENOMEM);
}
EXPORT_SYMBOL(ion_client_create);

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
void ion_client_destroy(struct ion_client *client)
{
	struct ion_device *dev;
	struct rb_node *n;


	if (!client) {
		pr_err("%s: client cannot be null\n", __func__);
		return;
	}

	dev = client->dev;

	pr_debug("%s: %d\n", __func__, __LINE__);
	down_write(&dev->lock);
	rb_erase(&client->node, &dev->clients);
	up_write(&dev->lock);

	/* After this completes, there are no more references to client */
	debugfs_remove_recursive(client->debug_root);

	mutex_lock(&client->lock);
	while ((n = rb_first(&client->handles))) {
		struct ion_handle *handle = rb_entry(n, struct ion_handle,
						     node);
		ion_share_pool_notify(dev, handle->share_id, -1, 0, 0);
		ion_handle_destroy(&handle->ref);
	}
	mutex_unlock(&client->lock);

	share_pool_unregister_all(client);
	kfifo_free(&client->fifo);

	idr_destroy(&client->idr);
	if (client->task)
		put_task_struct(client->task);
	kfree(client->display_name);
	kfree(client->name);
	kfree(client);
}
EXPORT_SYMBOL(ion_client_destroy);

/**
 * ion_sg_table - get an sg_table for the buffer
 *
 * NOTE: most likely you should NOT being using this API.
 * You should be using Ion as a DMA Buf exporter and using
 * the sg_table returned by dma_buf_map_attachment.
 */
struct sg_table *ion_sg_table(struct ion_client *client,
			      struct ion_handle *handle)
{
	struct ion_buffer *buffer;
	struct sg_table *table;

	mutex_lock(&client->lock);
	if (!ion_handle_validate(client, handle)) {
		pr_err("%s: invalid handle passed to map_dma.\n",
		       __func__);
		mutex_unlock(&client->lock);
		return ERR_PTR(-EINVAL);
	}
	buffer = handle->buffer;
	table = buffer->sg_table;
	mutex_unlock(&client->lock);
	return table;
}
EXPORT_SYMBOL(ion_sg_table);

static struct sg_table *dup_sg_table(struct sg_table *table)
{
	struct sg_table *new_table;
	int ret, i;
	struct scatterlist *sg, *new_sg;

	new_table = kzalloc(sizeof(*new_table), GFP_KERNEL);
	if (!new_table)
		return ERR_PTR(-ENOMEM);

	ret = sg_alloc_table(new_table, table->orig_nents, GFP_KERNEL);
	if (ret) {
		kfree(new_table);
		return ERR_PTR(-ENOMEM);
	}

	new_sg = new_table->sgl;
	for_each_sgtable_sg(table, sg, i) {
		memcpy(new_sg, sg, sizeof(*sg));
		new_sg->dma_address = 0;
		new_sg = sg_next(new_sg);
	}

	return new_table;
}

static void free_duped_table(struct sg_table *table)
{
	sg_free_table(table);
	kfree(table);
}

struct ion_dma_buf_attachment {
	struct device *dev;
	struct sg_table *table;
	struct list_head list;
};

static int ion_dma_buf_attach(struct dma_buf *dmabuf,
			      struct dma_buf_attachment *attachment)
{
	struct ion_dma_buf_attachment *a;
	struct sg_table *table;
	struct ion_buffer *buffer = dmabuf->priv;

	a = kzalloc(sizeof(*a), GFP_KERNEL);
	if (!a)
		return -ENOMEM;

	table = dup_sg_table(buffer->sg_table);
	if (IS_ERR(table)) {
		kfree(a);
		return -ENOMEM;
	}

	a->table = table;
	a->dev = attachment->dev;
	INIT_LIST_HEAD(&a->list);

	attachment->priv = a;

	mutex_lock(&buffer->lock);
	list_add(&a->list, &buffer->attachments);
	mutex_unlock(&buffer->lock);

	return 0;
}

static void ion_dma_buf_detach(struct dma_buf *dmabuf,
			       struct dma_buf_attachment *attachment)
{
	struct ion_dma_buf_attachment *a = attachment->priv;
	struct ion_buffer *buffer = dmabuf->priv;

	mutex_lock(&buffer->lock);
	list_del(&a->list);
	mutex_unlock(&buffer->lock);
	free_duped_table(a->table);

	kfree(a);
}

static struct sg_table *ion_map_dma_buf(struct dma_buf_attachment *attachment,
					enum dma_data_direction direction)
{
	struct ion_dma_buf_attachment *a = attachment->priv;
	struct sg_table *table;
	int ret;

	table = a->table;

	ret = dma_map_sgtable(attachment->dev, table, direction, 0);
	if (ret)
		return ERR_PTR(ret);

	return table;
}

static void ion_unmap_dma_buf(struct dma_buf_attachment *attachment,
			      struct sg_table *table,
			      enum dma_data_direction direction)
{
	//dma_unmap_sgtable(attachment->dev, table, direction, 0);
}

int ion_mmap(struct ion_buffer *buffer, struct vm_area_struct *vma)
{
	int ret = 0;

	if (!buffer->heap->ops->map_user) {
		pr_err("%s: this heap does not define a method for mapping to userspace\n",
		       __func__);
		return -EINVAL;
	}

	if (!(buffer->flags & ION_FLAG_CACHED))
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	mutex_lock(&buffer->lock);
	/* now map it to userspace */
	ret = buffer->heap->ops->map_user(buffer->heap, buffer, vma);
	mutex_unlock(&buffer->lock);

	if (ret)
		pr_err("%s: failure mapping buffer to userspace\n",
		       __func__);

	return ret;
}
EXPORT_SYMBOL(ion_mmap);

static int ion_dma_buf_mmap(struct dma_buf *dmabuf, struct vm_area_struct *vma)
{
       return ion_mmap(dmabuf->priv, vma);
}

static void ion_dma_buf_release(struct dma_buf *dmabuf)
{
	struct ion_buffer *buffer = dmabuf->priv;

	ion_buffer_put(buffer);
}

static int ion_dma_buf_begin_cpu_access(struct dma_buf *dmabuf,
					enum dma_data_direction direction)
{
	struct ion_buffer *buffer = dmabuf->priv;
	void *vaddr;
	struct ion_dma_buf_attachment *a;
	int ret = 0;

	/*
	 * TODO: Move this elsewhere because we don't always need a vaddr
	 */
	if (buffer->heap->ops->map_kernel) {
		mutex_lock(&buffer->lock);
		vaddr = ion_buffer_kmap_get(buffer);
		if (IS_ERR(vaddr)) {
			ret = PTR_ERR(vaddr);
			goto unlock;
		}
		mutex_unlock(&buffer->lock);
	}

	mutex_lock(&buffer->lock);
	list_for_each_entry(a, &buffer->attachments, list)
		dma_sync_sgtable_for_cpu(a->dev, a->table, direction);

unlock:
	mutex_unlock(&buffer->lock);
	return ret;
}

static int ion_dma_buf_end_cpu_access(struct dma_buf *dmabuf,
					  enum dma_data_direction direction)
{
	struct ion_buffer *buffer = dmabuf->priv;
	struct ion_dma_buf_attachment *a;

	if (buffer->heap->ops->map_kernel) {
		mutex_lock(&buffer->lock);
		ion_buffer_kmap_put(buffer);
		mutex_unlock(&buffer->lock);
	}

	mutex_lock(&buffer->lock);
	list_for_each_entry(a, &buffer->attachments, list)
		dma_sync_sgtable_for_device(a->dev, a->table, direction);
	mutex_unlock(&buffer->lock);

	return 0;
}

static const struct dma_buf_ops dma_buf_ops = {
	.map_dma_buf = ion_map_dma_buf,
	.unmap_dma_buf = ion_unmap_dma_buf,
	.mmap = ion_dma_buf_mmap,
	.release = ion_dma_buf_release,
	.attach = ion_dma_buf_attach,
	.detach = ion_dma_buf_detach,
	.begin_cpu_access = ion_dma_buf_begin_cpu_access,
	.end_cpu_access = ion_dma_buf_end_cpu_access,
};

struct dma_buf *ion_share_dma_buf(struct ion_client *client,
						struct ion_handle *handle)
{
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	struct ion_buffer *buffer;
	struct dma_buf *dmabuf;
	bool valid_handle;

	if (!client) {
		pr_err("%s: client cannot be null\n", __func__);
		return ERR_PTR(-EINVAL);
	}

	if (!handle) {
		pr_err("%s: handle cannot be null\n", __func__);
		return ERR_PTR(-EINVAL);
	}

	mutex_lock(&client->lock);
	valid_handle = ion_handle_validate(client, handle);
	if (!valid_handle) {
		WARN(1, "%s: invalid handle passed to share.\n", __func__);
		mutex_unlock(&client->lock);
		return ERR_PTR(-EINVAL);
	}
	buffer = handle->buffer;
	ion_buffer_get(buffer);
	mutex_unlock(&client->lock);

	exp_info.ops = &dma_buf_ops;
	exp_info.size = buffer->size;
	exp_info.flags = O_RDWR;
	exp_info.priv = buffer;

	dmabuf = dma_buf_export(&exp_info);
	if (IS_ERR(dmabuf)) {
		ion_buffer_put(buffer);
		return dmabuf;
	}

	return dmabuf;
}
EXPORT_SYMBOL(ion_share_dma_buf);

int ion_share_dma_buf_fd(struct ion_client *client, struct ion_handle *handle)
{
	struct dma_buf *dmabuf;
	int fd;

	if (!client) {
		pr_err("%s: client cannot be null\n", __func__);
		return -EINVAL;
	}

	if (!handle) {
		pr_err("%s: handle cannot be null\n", __func__);
		return -EINVAL;
	}

	dmabuf = ion_share_dma_buf(client, handle);
	if (IS_ERR(dmabuf)) {
		pr_err("%s: Got buf error[%ld].", __func__, PTR_ERR(dmabuf));
		return PTR_ERR(dmabuf);
	}

	fd = dma_buf_fd(dmabuf, O_CLOEXEC);
	if (fd < 0) {
		pr_err("%s: Got buf fd error[%d].", __func__, fd);
		dma_buf_put(dmabuf);
	}

	return fd;
}
EXPORT_SYMBOL(ion_share_dma_buf_fd);

/**
 * @NO{S21E04C01I}
 * @ASIL{B}
 * @brief import the dma buffer
 *
 * @param[in] client: the ion client structure
 * @param[in] fd: the share fd
 *
 * @retval "correct_pte": succeed
 * @retval "ERR_PTR": invalid parameter
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
struct ion_handle *ion_import_dma_buf(struct ion_client *client, int fd)
{
	struct dma_buf *dmabuf;
	struct ion_buffer *buffer;
	struct ion_handle *handle;
	struct ion_share_handle *share_hd;
	int32_t share_id;
	struct ion_device *dev;
	int ret;

	if (!client) {
		pr_err("%s: client cannot be null\n", __func__);
		return ERR_PTR(-EINVAL);
	}

	dev = client->dev;
	dmabuf = dma_buf_get(fd);
	if (IS_ERR(dmabuf))
		return ERR_CAST(dmabuf);
	/* if this memory came from ion */

	if (dmabuf->ops != &dma_buf_ops) {
		pr_err("%s: can not import dmabuf from another exporter\n",
		       __func__);
		dma_buf_put(dmabuf);
		return ERR_PTR(-EINVAL);
	}
	buffer = dmabuf->priv;

	share_id = buffer->share_id;
	mutex_lock(&dev->share_lock);
	share_hd = idr_find(&dev->idr, share_id);
	if (IS_ERR_OR_NULL(share_hd)) {
		mutex_unlock(&dev->share_lock);
		pr_err("%s: find ion share handle failed [%ld].\n",
				__func__, PTR_ERR(share_hd));
		return share_hd ? ERR_CAST(share_hd): ERR_PTR(-EINVAL);
	}
	ion_buffer_get(buffer);
	ion_share_handle_get(share_hd);
	mutex_unlock(&dev->share_lock);

	mutex_lock(&client->lock);
	/* if a handle exists for this buffer just take a reference to it */
	handle = ion_handle_lookup(client, buffer);
	if (!IS_ERR(handle)) {
		handle = ion_handle_get_check_overflow(handle);
		if (!IS_ERR(handle) && (handle->share_id != 0) && (handle->sh_hd != NULL)) {
			ion_handle_import_get(handle);
		}
		mutex_unlock(&client->lock);
		ion_share_handle_put(share_hd);
		goto end;
	}

	handle = ion_handle_create(client, buffer);
	if (IS_ERR(handle)) {
		mutex_unlock(&client->lock);
		ion_share_handle_put(share_hd);
		goto end;
	}

	ret = ion_handle_add(client, handle);
	if (ret == 0) {
		handle->share_id = share_id;
		handle->sh_hd = share_hd;
		ion_share_handle_add_to_handle(handle->sh_hd);
	}

	mutex_unlock(&client->lock);
	if (ret) {
		ion_handle_put(handle);
		handle = ERR_PTR(ret);
		ion_share_handle_put(share_hd);
	}

end:
	ion_buffer_put(buffer);
	dma_buf_put(dmabuf);
	return handle;
}
EXPORT_SYMBOL(ion_import_dma_buf);

static int ion_sync_for_device(struct ion_client *client, int fd)
{
	struct dma_buf *dmabuf;
	struct ion_buffer *buffer;

	dmabuf = dma_buf_get(fd);
	if (IS_ERR(dmabuf))
		return PTR_ERR(dmabuf);

	/* if this memory came from ion */
	if (dmabuf->ops != &dma_buf_ops) {
		pr_err("%s: can not sync dmabuf from another exporter\n",
		       __func__);
		dma_buf_put(dmabuf);
		return -EINVAL;
	}
	buffer = dmabuf->priv;

	// here we do nothing;
	//dma_sync_sg_for_device(NULL, buffer->sg_table->sgl,
	//		       buffer->sg_table->nents, DMA_BIDIRECTIONAL);
	dma_buf_put(dmabuf);
	return 0;
}

/**
 * @NO{S21E04C01I}
 * @ASIL{B}
 * @brief increase the handle import consume count and share handle client consume count
 *
 * @param[in] client: the ion client structure
 * @param[in] data: the share handle data for user space
 *
 * @retval "-EINVAL": invalid parameter
 * @retval "=0": success
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
static int32_t ion_share_handle_import_consume_cnt(struct ion_client *client,
					struct ion_share_handle_data *data)
{
	struct ion_buffer *buffer;
	struct ion_device *dev = client->dev;
	struct ion_share_handle *share_hd;
	struct ion_handle *handle;
	int share_id = data->sh_handle;
	int32_t ret = 0;

	mutex_lock(&dev->share_lock);
	share_hd = idr_find(&dev->idr, share_id);
	if (IS_ERR_OR_NULL(share_hd)) {
		mutex_unlock(&dev->share_lock);
		pr_err("%s: find ion share handle failed [%ld].\n",
				__func__, PTR_ERR(share_hd));
		return -EINVAL;
	}
	buffer = share_hd->buffer;
	ion_buffer_get(buffer);
	ion_share_handle_get(share_hd);
	mutex_unlock(&dev->share_lock);

	mutex_lock(&client->lock);
	/* if a handle exists for this buffer just take a reference to it */
	handle = ion_handle_lookup(client, buffer);
	if (IS_ERR(handle)) {
		mutex_unlock(&client->lock);
		ion_share_handle_put(share_hd);
		ion_buffer_put(buffer);
		return -EINVAL;
	}
	handle->import_consume_cnt++;
	mutex_unlock(&client->lock);

	mutex_lock(&share_hd->consume_cnt_lock);
	share_hd->consume_cnt++;
	mutex_unlock(&share_hd->consume_cnt_lock);
	ion_share_handle_put(share_hd);
	ion_buffer_put(buffer);

	return ret;
}

/**
 * @NO{S21E04C01I}
 * @ASIL{B}
 * @brief decrease the handle import consume count and share handle client consume count
 *
 * @param[in] client: the ion client structure
 * @param[in] data: the share handle data for user space
 *
 * @retval "-EINVAL": invalid parameter
 * @retval "=0": success
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
static int32_t ion_share_handle_free_consume_cnt(struct ion_client *client,
					struct ion_share_handle_data *data)
{
	struct ion_buffer *buffer;
	struct ion_device *dev = client->dev;
	struct ion_share_handle *share_hd;
	struct ion_handle *handle;
	int share_id = data->sh_handle;
	int32_t ret = 0;

	mutex_lock(&dev->share_lock);
	share_hd = idr_find(&dev->idr, share_id);
	if (IS_ERR_OR_NULL(share_hd)) {
		mutex_unlock(&dev->share_lock);
		pr_err("%s: find ion share handle failed [%ld].\n",
				__func__, PTR_ERR(share_hd));
		return -EINVAL;
	}
	buffer = share_hd->buffer;
	ion_buffer_get(buffer);
	ion_share_handle_get(share_hd);
	mutex_unlock(&dev->share_lock);

	mutex_lock(&client->lock);
	/* if a handle exists for this buffer just take a reference to it */
	handle = ion_handle_lookup(client, buffer);
	if (IS_ERR(handle)) {
		mutex_unlock(&client->lock);
		ion_share_handle_put(share_hd);
		ion_buffer_put(buffer);
		return -EINVAL;
	}
	if (!handle->import_consume_cnt) {
		pr_warn("%s: Double unimport handle(share id %d) detected! bailing...\n",
			__func__, handle->share_id);
		mutex_unlock(&client->lock);
		ion_share_handle_put(share_hd);
		ion_buffer_put(buffer);
		return -EINVAL;
	}
	handle->import_consume_cnt--;
	mutex_unlock(&client->lock);

	mutex_lock(&share_hd->consume_cnt_lock);
	share_hd->consume_cnt--;
	wake_up_interruptible(&share_hd->consume_cnt_wait_q);

	mutex_unlock(&share_hd->consume_cnt_lock);
	ion_share_handle_put(share_hd);
	ion_buffer_put(buffer);

	return ret;
}

/* fix up the cases where the ioctl direction bits are incorrect */
static unsigned int ion_ioctl_dir(unsigned int cmd)
{
	switch (cmd) {
	case ION_IOC_SYNC:
	case ION_IOC_FREE:
	case ION_IOC_CUSTOM:
		return _IOC_WRITE;
	default:
		return _IOC_DIR(cmd);
	}
}

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief register the share memory pool buffer
 *
 * @param[in] client: the ion client
 * @param[in] data: the share pool data form userspace
 *
 * @retval "0": succeed
 * @retval "<0": Linux errno
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
int32_t ion_share_pool_register_buf(struct ion_client *client,
		struct ion_share_pool_data *data)
{
	struct ion_buffer *buffer;
	struct ion_device *dev = client->dev;
	struct ion_share_handle *share_hd;
	int share_id = data->sh_handle;
	struct ion_share_pool_buf *share_pool_buf;

	mutex_lock(&dev->share_lock);
	share_hd = idr_find(&dev->idr, share_id);
	if (IS_ERR_OR_NULL(share_hd)) {
		mutex_unlock(&dev->share_lock);
		pr_err("%s: find ion share handle failed [%ld].\n",
				__func__, PTR_ERR(share_hd));
		return -EINVAL;
	}
	buffer = share_hd->buffer;
	share_pool_buf =
		ion_share_pool_buf_create(dev, buffer, client, data->sh_fd, share_id);
	if (IS_ERR(share_pool_buf)) {
		mutex_unlock(&dev->share_lock);
		pr_err("%s: share pool buf create error[%ld]\n",
				__func__, PTR_ERR(share_pool_buf));
		return -EINVAL;
	}

	ion_share_pool_buf_add(dev, share_pool_buf);
	mutex_unlock(&dev->share_lock);

	return 0;
}

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief unregister the share memory pool buffer
 *
 * @param[in] client: the ion client
 * @param[in] data: the share pool data form userspace
 *
 * @retval "0": succeed
 * @retval "<0": Linux errno
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
int32_t ion_share_pool_unregister_buf(struct ion_client *client,
		struct ion_share_pool_data *data)
{
	struct ion_device *dev = client->dev;
	int share_id = data->sh_handle;
	struct ion_share_pool_buf *share_pool_buf;

	mutex_lock(&dev->share_lock);
	share_pool_buf = ion_share_pool_buf_lookup(dev, share_id);
	if (IS_ERR_OR_NULL(share_pool_buf)) {
		mutex_unlock(&dev->share_lock);
		return -EINVAL;
	}
	mutex_unlock(&dev->share_lock);

	ion_share_pool_buf_put(share_pool_buf);

	return 0;
}

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief get the share memory pool buffer reference count
 *
 * @param[in] client: the ion client
 * @param[in] share_id: the share id of the buffer
 * @param[out] import_cnt: the buffer reference count
 *
 * @retval "0": succeed
 * @retval "<0": Linux errno
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
int32_t ion_share_pool_get_ref_cnt(
	struct ion_client *client, int share_id, int32_t *import_cnt)
{
	struct ion_buffer *buffer;
	struct ion_device *dev = client->dev;
	struct ion_share_handle *share_hd;
	struct ion_handle *handle;

	mutex_lock(&dev->share_lock);

	share_hd = idr_find(&dev->idr, share_id);
	if (IS_ERR_OR_NULL(share_hd)) {
		mutex_unlock(&dev->share_lock);
		pr_err("%s: find ion share handle %d failed [%ld].\n",
				__func__, share_id, PTR_ERR(share_hd));
		return -EINVAL;
	}

	buffer = share_hd->buffer;
	handle = ion_handle_lookup(client, buffer);
	if (IS_ERR(handle)) {
		mutex_unlock(&dev->share_lock);
		pr_err("%s: find ion handle failed.\n",	__func__);
		return -EINVAL;
	}

	*import_cnt = handle->import_cnt;
	mutex_unlock(&dev->share_lock);

	return 0;
}

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief wait the buffer reference updata
 *
 * @param[in] client: the ion client
 * @param[in] data: the share pool data form userspace
 *
 * @retval "0": succeed
 * @retval "<0": Linux errno
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
int32_t ion_share_pool_monitor_ref_cnt(struct ion_client *client,
		struct ion_share_pool_data *data)
{
	int32_t ret;
	int32_t buf[SPBUFNUM];

	ret = kfifo_out_spinlocked(
		&client->fifo, buf, SPBUFNUM * sizeof(int32_t), &client->fifo_lock);
	if (ret > 0) {
		data->sh_fd = buf[SPBUFFD];
		data->sh_handle = buf[SPBUFID];
		data->import_cnt = buf[SPBUFREFCNT];
		client->wq_cnt = 0;
		return 0;
	}

	ret = (int32_t) wait_event_interruptible(client->wq, client->wq_cnt != 0);
	if (ret != 0) {
		pr_info("%s failed %d\n", __func__, ret);
		return ret;
	}

	client->wq_cnt = 0;
	ret = kfifo_out_spinlocked(
		&client->fifo, buf, 3 * sizeof(int32_t), &client->fifo_lock);
	if (ret > 0) {
		data->sh_fd = buf[SPBUFFD];
		data->sh_handle = buf[SPBUFID];
		data->import_cnt = buf[SPBUFREFCNT];
		return 0;
	} else {
		ret = -EINVAL;
	}

	return ret;
}

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief wake up the reference wait event
 *
 * @param[in] client: the ion client
 *
 * @retval "0": succeed
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
int32_t ion_share_pool_wake_up_monitor(struct ion_client *client)
{
	client->wq_cnt = 1;
	wake_up_interruptible(&client->wq);
	return 0;
}

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief get the process info of the buffer
 *
 * @param[in] dev: the ion device
 * @param[in] data: the process info data form userspace
 *
 * @retval "0": succeed
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
static int32_t ion_get_buffer_process_info(struct ion_device *dev,
		struct ion_process_info_data *data)
{
	int32_t share_id = data->share_id;
	struct rb_node *node;
	struct rb_node *n;
	int idx = 0;
	int max_idx = data->num > MAX_PROCESS_INFO ?  MAX_PROCESS_INFO : data->num;

	down_write(&dev->lock);
	data->num = 0;
	for (node = rb_first(&dev->clients); node; node = rb_next(node)) {
		struct ion_client *client = rb_entry(node, struct ion_client, node);

		mutex_lock(&client->lock);
		for (n = rb_first(&client->handles); n; n = rb_next(n)) {
			struct ion_handle *handle = rb_entry(n, struct ion_handle, node);
			if (handle->share_id == share_id) {
				data->pid[idx++] = client->pid;
				data->num = idx;
				if (idx >= max_idx) {
					mutex_unlock(&client->lock);
					up_write(&dev->lock);
					return 0;
				}
				break;
			}
		}
		mutex_unlock(&client->lock);
	}
	up_write(&dev->lock);

	return 0;
}

/**
 * @NO{S21E04C01I}
 * @ASIL{B}
 * @brief the ion driver ioctl
 *
 * @param[in] filp: the filp struct
 * @param[in] cmd: the operation cmd
 * @param[in] arg: dditional data to pass to the ion ioctl
 *
 * @retval "correct_pte": succeed
 * @retval "=-22": invalid parameter
 * @retval "=-14": bad address
 * @retval "=-25": Not a typewriter
 * @retval "=-62": out of time
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
long ion_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct ion_client *client = filp->private_data;
	struct ion_device *dev = client->dev;
	struct ion_handle *cleanup_handle = NULL;
	int ret = 0;
	unsigned int dir;

	union {
		struct ion_fd_data fd;
		struct ion_allocation_data allocation;
		struct ion_handle_data handle;
		struct ion_custom_data custom;
		struct ion_share_handle_data share_hd;
		struct ion_share_info_data share_info;
		struct ion_share_pool_data share_hd_fd;
		struct ion_share_and_phy_data share_phy_data;
		struct ion_process_info_data process_info;
		struct ion_consume_info_data consume_info;
		struct ion_version_info_data version_info;
	} data;
	memset(&data, 0x00, sizeof(data));

	dir = ion_ioctl_dir(cmd);

	if (_IOC_SIZE(cmd) > sizeof(data))
		return -EINVAL;

	if (dir & _IOC_WRITE)
		if (copy_from_user(&data, (void __user *)arg, _IOC_SIZE(cmd)))
			return -EFAULT;

	switch (cmd) {
	case ION_IOC_ALLOC:
	{
		struct ion_handle *handle;

		handle = ion_alloc(client, data.allocation.len,
						data.allocation.align,
						data.allocation.heap_id_mask,
						data.allocation.flags);
		if (IS_ERR(handle))
			return PTR_ERR(handle);

		data.allocation.handle = handle->id;
		data.allocation.sh_handle = handle->share_id;
		cleanup_handle = handle;
		break;
	}
	case ION_IOC_FREE:
	{
		struct ion_handle *handle;

		mutex_lock(&client->lock);
		handle = ion_handle_get_by_id_nolock(client,
						     data.handle.handle);
		if (IS_ERR(handle)) {
			mutex_unlock(&client->lock);
			return PTR_ERR(handle);
		}
		ion_share_pool_notify(dev, handle->share_id, -1, SPWQTIMEOUT, 0);
		ion_free_nolock(client, handle);
		ion_handle_put_nolock(handle);
		mutex_unlock(&client->lock);
		break;
	}
	case ION_IOC_SHARE:
	case ION_IOC_MAP:
	{
		struct ion_handle *handle;

		handle = ion_handle_get_by_id(client, data.handle.handle);
		if (IS_ERR(handle)) {
			pr_err("%s: find ion handle failed [%ld].",
					__func__, PTR_ERR(handle));
			return PTR_ERR(handle);
		}
		data.fd.fd = ion_share_dma_buf_fd(client, handle);
		ion_handle_put(handle);
		if (data.fd.fd < 0)
			ret = data.fd.fd;
		break;
	}
	case ION_IOC_IMPORT:
	{
		struct ion_handle *handle;

		handle = ion_import_dma_buf(client, data.fd.fd);
		if (IS_ERR(handle))
			ret = PTR_ERR(handle);
		else
			data.handle.handle = handle->id;
		break;
	}
	case ION_IOC_SYNC:
	{
		ret = ion_sync_for_device(client, data.fd.fd);
		break;
	}
	case ION_IOC_CUSTOM:
	{
		if (!dev->custom_ioctl)
			return -ENOTTY;
		ret = dev->custom_ioctl(client, data.custom.cmd,
						data.custom.arg);
		break;
	}
	case ION_IOC_IMPORT_SHARE_ID:
	{
		struct ion_handle *handle;

		ret = ion_share_pool_notify(dev, data.share_hd.sh_handle, 1, SPWQTIMEOUT, 1);
		if (ret != 0) {
			pr_err("%s:%d failed ret[%d].", __func__, __LINE__, ret);
			break;
		}

		handle = ion_share_handle_import_dma_buf(client, &data.share_hd);
		if (IS_ERR(handle)) {
			ret = PTR_ERR(handle);
		} else {
			data.share_hd.handle = handle->id;
		}
		break;
	}
	case ION_IOC_GET_SHARE_INFO:
	{
		struct ion_handle *handle;

		mutex_lock(&client->lock);
		handle = ion_handle_get_by_id_nolock(client, data.share_info.handle);
		if (IS_ERR(handle)) {
			mutex_unlock(&client->lock);
			pr_err("%s:%d find ion handle failed [%ld].",
					__func__, __LINE__, PTR_ERR(handle));
			return PTR_ERR(handle);
		}

		if ((handle->share_id != 0) && (handle->sh_hd != NULL)) {
			data.share_info.cur_client_cnt = ion_share_handle_get_share_info(handle->sh_hd);
		} else {
			pr_err("%s:%d invalid handle with share id %d and ptr %p.",
					__func__, __LINE__, handle->share_id, handle->sh_hd);
			ret = -EINVAL;
		}
		ion_handle_put_nolock(handle);
		mutex_unlock(&client->lock);
		break;
	}
	case ION_IOC_WAIT_SHARE_ID:
	{
		struct ion_handle *handle;
		struct ion_share_handle * share_hd;

		mutex_lock(&client->lock);
		handle = ion_handle_get_by_id_nolock(client, data.share_info.handle);
		if (IS_ERR(handle)) {
			mutex_unlock(&client->lock);
			pr_warn("%s:%d find ion handle failed [%ld].",
					__func__, __LINE__, PTR_ERR(handle));
			return PTR_ERR(handle);
		}
		if ((handle->share_id == 0) || (handle->sh_hd == NULL)) {
			mutex_unlock(&client->lock);
			pr_err("%s:%d invalid handle with share id %d and ptr %p.",
					__func__, __LINE__, handle->share_id, handle->sh_hd);
			return -EINVAL;
		}
		ion_share_handle_get(handle->sh_hd);
		share_hd = handle->sh_hd;
		mutex_unlock(&client->lock);

		if (data.share_info.timeout > 0) {
			ret = (int32_t) wait_event_interruptible_timeout(share_hd->client_cnt_wait_q,
				share_hd->client_cnt <= data.share_info.target_client_cnt,
				msecs_to_jiffies(data.share_info.timeout));
			if (ret > 0) {
				ret = 0;
			} else if (ret == 0) {
				ret = -ETIME;
			}
		} else if (data.share_info.timeout < 0) {
			ret = (int32_t) wait_event_interruptible(share_hd->client_cnt_wait_q,
				share_hd->client_cnt <= data.share_info.target_client_cnt);
		}
		data.share_info.cur_client_cnt = share_hd->client_cnt;

		ion_share_handle_put(share_hd);
		ion_handle_put(handle);
		break;
	}
	case ION_IOC_SHARE_POOL_REGISTER:
	{
		ret = ion_share_pool_register_buf(client, &data.share_hd_fd);
		break;
	}
	case ION_IOC_SHARE_POOL_UNREGISTER:
	{
		ret = ion_share_pool_unregister_buf(client, &data.share_hd_fd);
		break;
	}
	case ION_IOC_SHARE_POOL_GET_REF_CNT:
	{
		ret = ion_share_pool_get_ref_cnt(
			client, data.share_hd_fd.sh_handle, &data.share_hd_fd.import_cnt);
		break;
	}
	case ION_IOC_SHARE_POOL_MONITOR_REF_CNT:
	{
		//wait for import count of share fd changing
		ret = ion_share_pool_monitor_ref_cnt(client, &data.share_hd_fd);
		break;
	}
	case ION_IOC_SHARE_POOL_WAKE_UP_MONITOR:
	{
		if (data.share_hd_fd.sh_handle == -1) {
			//when monitor thread destroy in userspace
			ret = ion_share_pool_wake_up_monitor(client);
		} else {
			//wake up the next pid import count updata for the same buffer
			ret = ion_share_pool_wake_up_ref_cnt(client, &data.share_hd_fd);
		}
		break;
	}
	case ION_IOC_GET_BUFFER_PROCESS_INFO:
	{
		ret = ion_get_buffer_process_info(dev, &data.process_info);
		break;
	}
	case ION_IOC_SHARE_AND_PHY:
	{
		struct ion_handle *handle;

		handle = ion_handle_get_by_id(client, data.handle.handle);
		if (IS_ERR(handle)) {
			pr_err("%s: find ion handle failed [%ld].",
					__func__, PTR_ERR(handle));
			return PTR_ERR(handle);
		}
		data.fd.fd = ion_share_dma_buf_fd(client, handle);
		ion_handle_put(handle);
		if (data.fd.fd < 0)
			ret = data.fd.fd;

		ret = ion_phys(client, (long)data.handle.handle,
					&data.share_phy_data.paddr, &data.share_phy_data.len);
		break;
	}
	case ION_IOC_INC_CONSUME_CNT:
	{
		ret = ion_share_handle_import_consume_cnt(client, &data.share_hd);
		if (ret != 0) {
			pr_err("%s:%d import consume cnt failed [%d].",
					__func__, __LINE__, ret);
		}
		break;
	}
	case ION_IOC_DEC_CONSUME_CNT:
	{
		ret = ion_share_handle_free_consume_cnt(client, &data.share_hd);
		if (ret != 0) {
			pr_err("%s:%d free consume cnt failed [%d].",
					__func__, __LINE__, ret);
		}
		break;
	}
	case ION_IOC_GET_CONSUME_INFO:
	{
		struct ion_handle *handle;

		mutex_lock(&client->lock);
		handle = ion_handle_get_by_id_nolock(client, data.consume_info.handle);
		if (IS_ERR(handle)) {
			mutex_unlock(&client->lock);
			pr_err("%s:%d find ion handle failed [%ld].",
					__func__, __LINE__, PTR_ERR(handle));
			return PTR_ERR(handle);
		}

		if ((handle->share_id != 0) && (handle->sh_hd != NULL)) {
			data.consume_info.cur_consume_cnt = ion_share_handle_get_consume_info(handle->sh_hd);
		} else {
			pr_err("%s:%d invalid handle with share id %d and ptr %p.",
					__func__, __LINE__, handle->share_id, handle->sh_hd);
			ret = -EINVAL;
		}
		ion_handle_put_nolock(handle);
		mutex_unlock(&client->lock);
		break;
	}
	case ION_IOC_WAIT_CONSUME_STATUS:
	{
		struct ion_handle *handle;
		struct ion_share_handle * share_hd;

		mutex_lock(&client->lock);
		handle = ion_handle_get_by_id_nolock(client, data.consume_info.handle);
		if (IS_ERR(handle)) {
			mutex_unlock(&client->lock);
			pr_warn("%s:%d find ion handle failed [%ld].",
					__func__, __LINE__, PTR_ERR(handle));
			return PTR_ERR(handle);
		}
		if ((handle->share_id == 0) || (handle->sh_hd == NULL)) {
			mutex_unlock(&client->lock);
			pr_err("%s:%d invalid handle with share id %d and ptr %p.",
					__func__, __LINE__, handle->share_id, handle->sh_hd);
			return -EINVAL;
		}
		ion_share_handle_get(handle->sh_hd);
		share_hd = handle->sh_hd;
		mutex_unlock(&client->lock);

		if (data.consume_info.timeout > 0) {
			ret = (int32_t) wait_event_interruptible_timeout(share_hd->consume_cnt_wait_q,
				share_hd->consume_cnt <= data.consume_info.target_consume_cnt,
				msecs_to_jiffies(data.consume_info.timeout));
			if (ret > 0) {
				ret = 0;
			} else if (ret == 0) {
				ret = -ETIME;
			}
		} else if (data.consume_info.timeout < 0) {
			ret = (int32_t) wait_event_interruptible(share_hd->consume_cnt_wait_q,
				share_hd->consume_cnt <= data.consume_info.target_consume_cnt);
		}
		data.consume_info.cur_consume_cnt = share_hd->consume_cnt;

		ion_share_handle_put(share_hd);
		ion_handle_put(handle);
		break;
	}
	case ION_IOC_GET_VERSION_INFO:
	{
		data.version_info.major = ION_DRIVER_API_VERSION_MAJOR;
		data.version_info.minor = ION_DRIVER_API_VERSION_MINOR;
		break;
	}
	default:
		return -ENOTTY;

	}

	if (dir & _IOC_READ) {
		if (copy_to_user((void __user *)arg, &data, _IOC_SIZE(cmd))) {
			if (cleanup_handle)
				ion_free(client, cleanup_handle);
			return -EFAULT;
		}
	}
	return ret;
}

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief release the ion client
 *
 * @param[in] inode: the device inode
 * @param[in] file: the file struct
 *
 * @retval "=0": succeed
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
static int ion_release(struct inode *inode, struct file *file)
{
	struct ion_client *client = file->private_data;

	pr_debug("%s: %d\n", __func__, __LINE__);
	ion_client_destroy(client);
	return 0;
}

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief open ion device and create the ion client
 *
 * @param[in] inode: the device inode
 * @param[in] file: the file struct
 *
 * @retval "=0": succeed
 * @retval "err_ptr": ion client create failed
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
static int ion_open(struct inode *inode, struct file *file)
{
	struct miscdevice *miscdev = file->private_data;
	struct ion_device *dev = container_of(miscdev, struct ion_device, dev);
	struct ion_client *client;
	char debug_name[64];

	pr_debug("%s: %d\n", __func__, __LINE__);
	snprintf(debug_name, 64, "%u", task_pid_nr(current->group_leader));
	client = ion_client_create(dev, debug_name);
	if (IS_ERR(client))
		return PTR_ERR(client);
	file->private_data = client;

	return 0;
}

static const struct file_operations ion_fops = {
	.owner          = THIS_MODULE,
	.open			= ion_open,
	.release		= ion_release,
	.unlocked_ioctl = ion_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= ion_ioctl,
#endif
};

#ifdef CONFIG_PCIE_HOBOT_EP_AI
/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief the ion opearte function for pac and virtio
 *
 * @param[in] ion_rev_data: the receive data
 *
 * @retval "correct_pte": succeed
 * @retval "=-22": invalid parameter
 * @retval "=-14": bad address
 * @retval "=-25": Not a typewriter
 * @retval "=-62": out of time
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
int32_t ion_function_operation(struct ion_data_pac *ion_rev_data)
{
	struct ion_handle *cleanup_handle = NULL;
	uint32_t cmd = ion_rev_data->cmd;
	int32_t ret = 0;

	if (_IOC_SIZE(cmd) > sizeof(ion_rev_data->data))
		return -EINVAL;

	switch (cmd) {
	case ION_IOC_ALLOC: {
		struct ion_handle *handle;
		struct ion_client *client = (struct ion_client*)ion_rev_data->client;

		handle = ion_alloc(client, ion_rev_data->data.allocation.len,
						ion_rev_data->data.allocation.align,
						ion_rev_data->data.allocation.heap_id_mask,
						ion_rev_data->data.allocation.flags);
		if (IS_ERR(handle))
			return PTR_ERR(handle);
		ion_rev_data->data.allocation.handle = handle->id;
		ion_rev_data->data.allocation.sh_handle = handle->share_id;
		ion_rev_data->return_value = ret;
		cleanup_handle = handle;
		break;
	}
	case ION_IOC_FREE: {
		struct ion_handle *handle;
		struct ion_client *client = (struct ion_client*)ion_rev_data->client;

		mutex_lock(&client->lock);
		handle = ion_handle_get_by_id_nolock(client,
						     ion_rev_data->data.handle.handle);
		if (IS_ERR(handle)) {
			mutex_unlock(&client->lock);
			return PTR_ERR(handle);
		}
		ion_free_nolock(client, handle);
		ion_handle_put_nolock(handle);
		mutex_unlock(&client->lock);
		ion_rev_data->return_value = ret;
		break;
	}
	case ION_IOC_SHARE:
	case ION_IOC_MAP: {
		struct ion_handle *handle;
		struct ion_client *client = (struct ion_client*)ion_rev_data->client;
		handle = ion_handle_get_by_id(client, ion_rev_data->data.fd.handle);
		if (IS_ERR(handle)) {
			pr_err("%s: find ion handle failed [%ld].",
					__func__, PTR_ERR(handle));
			return PTR_ERR(handle);
		}
		ion_rev_data->data.fd.fd = ion_share_dma_buf_fd(client, handle);
		ion_handle_put(handle);
		if (ion_rev_data->data.fd.fd < 0)
			ret = ion_rev_data->data.fd.fd;
		ion_rev_data->return_value = ret;
		break;
	}
	case ION_IOC_CUSTOM: {
		ret = ion_function_custom_operation(ion_rev_data);
		ion_rev_data->return_value = ret;
		break;
	}
	case ION_IOC_IMPORT: {
		struct ion_handle *handle;
		struct ion_client *client = (struct ion_client*)ion_rev_data->client;

		handle = ion_import_dma_buf(client, ion_rev_data->data.fd.fd);
		if (IS_ERR(handle))
			ret = PTR_ERR(handle);
		else
			ion_rev_data->data.handle.handle = handle->id;
		ion_rev_data->return_value = ret;
		break;
	}
	case ION_IOC_SYNC: {
		struct ion_client *client = (struct ion_client*)ion_rev_data->client;
		ret = ion_sync_for_device(client, ion_rev_data->data.fd.fd);
		ion_rev_data->return_value = ret;
		break;
	}
	case ION_IOC_IMPORT_SHARE_ID: {
		struct ion_handle *handle;
		struct ion_client *client = (struct ion_client*)ion_rev_data->client;

		handle = ion_share_handle_import_dma_buf(client, &ion_rev_data->data.share_hd);
		if (IS_ERR(handle))
			ret = PTR_ERR(handle);
		else
			ion_rev_data->data.share_hd.handle = handle->id;
		ion_rev_data->return_value = ret;
		break;
	}
	case ION_IOC_GET_SHARE_INFO: {
		struct ion_handle *handle;
		struct ion_client *client = (struct ion_client*)ion_rev_data->client;

		mutex_lock(&client->lock);
		handle = ion_handle_get_by_id_nolock(client, ion_rev_data->data.share_info.handle);
		if (IS_ERR(handle)) {
			mutex_unlock(&client->lock);
			pr_err("%s:%d find ion handle failed [%ld].",
					__func__, __LINE__, PTR_ERR(handle));
			return PTR_ERR(handle);
		}

		if ((handle->share_id != 0) && (handle->sh_hd != NULL)) {
			ion_rev_data->data.share_info.cur_client_cnt = ion_share_handle_get_share_info(handle->sh_hd);
		} else {
			pr_err("%s:%d invalid handle with share id %d and ptr %p.",
					__func__, __LINE__, handle->share_id, handle->sh_hd);
			ret = -EINVAL;
		}
		ion_handle_put_nolock(handle);
		mutex_unlock(&client->lock);
		ion_rev_data->return_value = ret;
		break;
	}
	case ION_IOC_WAIT_SHARE_ID: {
		struct ion_handle *handle;
		struct ion_share_handle * share_hd;
		struct ion_client *client = (struct ion_client*)ion_rev_data->client;

		mutex_lock(&client->lock);
		handle = ion_handle_get_by_id_nolock(client, ion_rev_data->data.share_info.handle);
		if (IS_ERR(handle)) {
			mutex_unlock(&client->lock);
			pr_warn("%s:%d find ion handle failed [%ld].",
					__func__, __LINE__, PTR_ERR(handle));
			return PTR_ERR(handle);
		}
		if ((handle->share_id == 0) || (handle->sh_hd == NULL)) {
			mutex_unlock(&client->lock);
			pr_err("%s:%d invalid handle with share id %d and ptr %p.",
					__func__, __LINE__, handle->share_id, handle->sh_hd);
			return -EINVAL;
		}
		ion_share_handle_get(handle->sh_hd);
		share_hd = handle->sh_hd;
		mutex_unlock(&client->lock);

		if (ion_rev_data->data.share_info.timeout > 0) {
			ret = (int32_t) wait_event_interruptible_timeout(share_hd->client_cnt_wait_q,
				share_hd->client_cnt <= ion_rev_data->data.share_info.target_client_cnt,
				msecs_to_jiffies(ion_rev_data->data.share_info.timeout));
			if (ret > 0) {
				ret = 0;
			} else if (ret == 0) {
				ret = -ETIME;
			}
		} else if (ion_rev_data->data.share_info.timeout < 0) {
			ret = (int32_t) wait_event_interruptible(share_hd->client_cnt_wait_q,
				share_hd->client_cnt <= ion_rev_data->data.share_info.target_client_cnt);
		}
		ion_rev_data->data.share_info.cur_client_cnt = share_hd->client_cnt;

		ion_share_handle_put(share_hd);
		ion_handle_put(handle);
		ion_rev_data->return_value = ret;
		break;
	}
	default:
		return -ENOTTY;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(ion_function_operation);
#endif

static size_t ion_debug_heap_total(struct ion_client *client,
				   unsigned int id)
{
	size_t size = 0;
	struct rb_node *n;

	mutex_lock(&client->lock);
	for (n = rb_first(&client->handles); n; n = rb_next(n)) {
		struct ion_handle *handle = rb_entry(n,
						     struct ion_handle,
						     node);
		struct ion_buffer *buffer = handle->buffer;
		if (buffer->priv_buffer != NULL) {
			buffer = (struct ion_buffer *)handle->buffer->priv_buffer;
			if (handle->buffer->heap->id == id)
				size += (handle->buffer->size - buffer->size);
		}
		if (buffer->heap->id == id)
			size += buffer->size;
	}
	mutex_unlock(&client->lock);
	return size;
}

static int ion_heap_show(struct ion_heap *heap, struct seq_file *s,
				      void *unused)
{
	struct ion_device *dev = heap->dev;
	struct rb_node *n;
	size_t total_size = 0;
	size_t heap_total_size = 0;
	size_t total_orphaned_size = 0;
	int32_t moduletype = 0;
	int32_t datatype = 0;
	char iontype[32];
	char *ptrtype;

	//heap_total_size = hobot_get_heap_size(heap->id);
	seq_puts(s, "-------------------------------------------------------------------------\n");
	seq_printf(s, "the heap id is %d\n", heap->id);
	seq_puts(s, "-------------------------------------------------------------------------\n");
	heap_total_size = heap->total_size;
	seq_puts(s, "-------------------------------------------------------------------------\n");
	seq_printf(s, "%16s %16s %16zu\n", heap->name, "heap total size", heap_total_size);
	seq_puts(s, "-------------------------------------------------------------------------\n");

	seq_printf(s, "%16s %16s %16s %16s\n", "heap name", "client", "pid", "size");
	seq_puts(s, "-------------------------------------------------------------------------\n");

	down_read(&dev->lock);
	for (n = rb_first(&dev->clients); n; n = rb_next(n)) {
		struct ion_client *client = rb_entry(n, struct ion_client,
						     node);
		size_t size = ion_debug_heap_total(client, heap->id);

		if (!size)
			continue;
		if (client->task) {
			char task_comm[TASK_COMM_LEN];

			get_task_comm(task_comm, client->task);
			seq_printf(s, "%16s %16s %16u %16zu\n", heap->name, task_comm,
				   client->pid, size);
		} else {
			seq_printf(s, "%16s %16s %16u %16zu\n", heap->name, client->name,
				   client->pid, size);
		}
	}
	up_read(&dev->lock);

	seq_puts(s, "-------------------------------------------------------------------------\n");
	seq_puts(s, "allocations (info is from last known client):\n");
	mutex_lock(&dev->buffer_lock);
	for (n = rb_first(&dev->buffers); n; n = rb_next(n)) {
		struct ion_buffer *buffer = rb_entry(n, struct ion_buffer,
							node);
		//the second scatterlist buffer, NULL is no matter
		struct ion_buffer *priv_buffer = (struct ion_buffer *)buffer->priv_buffer;
		size_t size = buffer->size;
		if (priv_buffer != NULL) {
			size -= priv_buffer->size;	//the fisrt buffer should sub the second buffer size
		}

		if (buffer->heap->id != heap->id)
			continue;
		total_size += size;

		moduletype = buffer->private_flags >> 12;
		datatype = buffer->private_flags & 0xfff;
		if (moduletype == 0) {
			uint32_t datatype_num = sizeof(_vio_data_type)/sizeof(_vio_data_type[1]);
			if (datatype >= datatype_num ) datatype = datatype_num - 1;
			ptrtype = _vio_data_type[datatype];
		} else if (moduletype == ION_MODULE_TYPE_BPU) {
			ptrtype = "bpu";
		} else if (moduletype == ION_MODULE_TYPE_VPU) {
			int vpuid = datatype >> 6;
			int vputype = datatype & 0x3f;
			snprintf(iontype, sizeof(iontype), "vpuchn%d_%d", vpuid, vputype);
			ptrtype = iontype;
		} else if (moduletype == ION_MODULE_TYPE_JPU) {
			int jpuid = datatype >> 6;
			int jputype = datatype & 0x3f;
			snprintf(iontype, sizeof(iontype), "jpuchn%d_%d", jpuid, jputype);
			ptrtype = iontype;
		} else if (moduletype == ION_MODULE_TYPE_GPU) {
			ptrtype = "gpu";
		} else {
			ptrtype = "other";
		}

		if (!buffer->handle_count) {
			seq_printf(s, "%16s %16u %16s %16zu %d orphaned\n",
				buffer->task_comm, buffer->pid,
				ptrtype,
				size, buffer->kmap_cnt);
				/* atomic_read(&buffer->ref.refcount)); */
			total_orphaned_size += size;
		} else {
			seq_printf(s, "%16s %16u %16s %16zu %d\n",
				buffer->task_comm, buffer->pid,
				ptrtype,
				size, buffer->kmap_cnt);
		}
	}

	mutex_unlock(&dev->buffer_lock);
	seq_puts(s, "-------------------------------------------------------------------------\n");
	seq_printf(s, "%16s %16zu\n", "total orphaned",
		   total_orphaned_size);
	seq_printf(s, "%16s %16zu\n", "total ", total_size);
	if (heap->flags & ION_HEAP_FLAG_DEFER_FREE)
		seq_printf(s, "%16s %16zu\n", "deferred free",
				heap->free_list_size);
	seq_puts(s, "-------------------------------------------------------------------------\n");

	if (heap->debug_show)
		heap->debug_show(heap, s, unused);

	return 0;
}

static int ion_debug_heap_show(struct seq_file *s, void *unused)
{
	struct ion_heap *heap = s->private;

	return ion_heap_show(heap, s, unused);
}

static int ion_debug_heap_open(struct inode *inode, struct file *file)
{
	return single_open(file, ion_debug_heap_show, inode->i_private);
}

static ssize_t ion_debug_config_write(struct file *filp, const char __user *buf, size_t len, loff_t *off)
{
	char value[32];

	if(copy_from_user(value, buf, len)) {
		printk("read data from user failed\n");
		return -EFAULT;
	}

	printk("%16s\n", value);

	return len;
}

static const struct file_operations debug_heap_fops = {
	.open = ion_debug_heap_open,
	.write = ion_debug_config_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int ion_debug_all_heap_show(struct seq_file *s, void *unused)
{
	struct ion_device *dev = s->private;
	struct ion_heap *heap;
	int ret = 0;

	plist_for_each_entry(heap, &dev->heaps, node) {
		ret = ion_heap_show(heap, s, unused);
	}

	return 0;
}

static int ion_debug_all_heap_open(struct inode *inode, struct file *file)
{
	return single_open(file, ion_debug_all_heap_show, inode->i_private);
}

static const struct file_operations debug_all_heap_fops = {
	.open = ion_debug_all_heap_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

#ifdef DEBUG_HEAP_SHRINKER
static int debug_shrink_set(void *data, u64 val)
{
	struct ion_heap *heap = data;
	struct shrink_control sc;
	int objs;

	sc.gfp_mask = GFP_HIGHUSER;
	sc.nr_to_scan = val;

	if (!val) {
		objs = heap->shrinker.count_objects(&heap->shrinker, &sc);
		sc.nr_to_scan = objs;
	}

	heap->shrinker.scan_objects(&heap->shrinker, &sc);
	return 0;
}

static int debug_shrink_get(void *data, u64 *val)
{
	struct ion_heap *heap = data;
	struct shrink_control sc;
	int objs;

	sc.gfp_mask = GFP_HIGHUSER;
	sc.nr_to_scan = 0;

	objs = heap->shrinker.count_objects(&heap->shrinker, &sc);
	*val = objs;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(debug_shrink_fops, debug_shrink_get,
			debug_shrink_set, "%llu\n");
#endif

void ion_device_add_heap(struct ion_device *dev, struct ion_heap *heap)
{
	struct dentry *debug_file;

	if (!heap->ops->allocate || !heap->ops->free || !heap->ops->map_dma ||
	    !heap->ops->unmap_dma)
		pr_err("%s: can not add heap with invalid ops struct.\n",
		       __func__);

	spin_lock_init(&heap->free_lock);
	heap->free_list_size = 0;

	if (heap->flags & ION_HEAP_FLAG_DEFER_FREE)
		ion_heap_init_deferred_free(heap);

	if ((heap->flags & ION_HEAP_FLAG_DEFER_FREE) || heap->ops->shrink)
		ion_heap_init_shrinker(heap);

	heap->dev = dev;
	down_write(&dev->lock);
	heap->id = (uint32_t)heap->type;
	/*
	 * use negative heap->id to reverse the priority -- when traversing
	 * the list later attempt higher id numbers first
	 */
	plist_node_init(&heap->node, -heap->id);
	plist_add(&heap->node, &dev->heaps);
	debug_file = debugfs_create_file(heap->name, 0664,
					dev->heaps_debug_root, heap,
					&debug_heap_fops);

	if (!debug_file) {
		char buf[256], *path;

		path = dentry_path(dev->heaps_debug_root, buf, 256);
		pr_err("Failed to create heap debugfs at %s/%s\n",
			path, heap->name);
	}

#ifdef DEBUG_HEAP_SHRINKER
	if (heap->shrinker.count_objects && heap->shrinker.scan_objects) {
		char debug_name[64];

		snprintf(debug_name, 64, "%s_shrink", heap->name);
		debug_file = debugfs_create_file(
			debug_name, 0644, dev->debug_root, heap,
			&debug_shrink_fops);
		if (!debug_file) {
			char buf[256], *path;

			path = dentry_path(dev->debug_root, buf, 256);
			pr_err("Failed to create heap shrinker debugfs at %s/%s\n",
				path, debug_name);
		}
	}
#endif
	up_write(&dev->lock);
}
EXPORT_SYMBOL(ion_device_add_heap);

void ion_device_del_heap(struct ion_device *dev, struct ion_heap *heap)
{
	struct dentry *debug_file;

	debug_file = debugfs_lookup(heap->name, dev->heaps_debug_root);
	if (debug_file != NULL) {
		debugfs_remove(debug_file);
	}

	plist_del(&heap->node, &dev->heaps);
}
EXPORT_SYMBOL(ion_device_del_heap);

static int _ion_buf_show(struct ion_device *dev, struct seq_file *s, int32_t share_id)
{
	struct rb_node *node;
	struct rb_node *n;

	down_write(&dev->lock);
	for (node = rb_first(&dev->clients); node; node = rb_next(node)) {
		struct ion_client *client = rb_entry(node, struct ion_client, node);
		mutex_lock(&client->lock);
		for (n = rb_first(&client->handles); n; n = rb_next(n)) {
			struct ion_handle *handle = rb_entry(n, struct ion_handle, node);
			if (handle->share_id == share_id) {
				seq_printf(s, "%20.20s, %20.20s, %5d, %8d\n",
					client->name, client->display_name, client->pid, share_id);
				break;
			}
		}
		mutex_unlock(&client->lock);
	}
	seq_printf(s, "\n");
	up_write(&dev->lock);

	return 0;
}

static int ion_buf_show(struct seq_file *s, void *unused)
{
	struct ion_device *dev = s->private;
	struct rb_node *node;
	struct ion_share_handle * share_hd;
	int32_t *share_id;
	int32_t total_num = 0, i = 0;

	mutex_lock(&dev->share_lock);
	for (node = rb_first(&dev->share_buffers); node; node = rb_next(node)) {
		total_num++;
	}
	mutex_unlock(&dev->share_lock);

	share_id = kzalloc(sizeof(int32_t) * total_num, GFP_ATOMIC);
	if (!share_id) {
		return -ENOMEM;
	}

	mutex_lock(&dev->share_lock);
	for (node = rb_first(&dev->share_buffers); node; node = rb_next(node)) {
		share_hd = rb_entry(node, struct ion_share_handle, node);
		share_id[i++] = share_hd->id;
		if (i >= total_num) {
			break;
		}
	}
	total_num = i;
	mutex_unlock(&dev->share_lock);

	seq_printf(s, "%20.20s, %20.20s, %5s, %8s\n",
		"process name", "display name", "pid", "share_id");

	for (i = 0; i < total_num; i++) {
		_ion_buf_show(dev, s, share_id[i]);
	}

	kfree(share_id);

	return 0;
}

static int ion_buf_open(struct inode *inode, struct file *file)
{
	return single_open(file, ion_buf_show, inode->i_private);
}

static const struct file_operations ion_buf_fops = {
	.open = ion_buf_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

struct ion_device *ion_device_create(long (*custom_ioctl)
				     (struct ion_client *client,
				      unsigned int cmd,
				      unsigned long arg))
{
	struct ion_device *idev;
	int ret;
	struct dentry *debug_file;

	idev = kzalloc(sizeof(*idev), GFP_KERNEL);
	if (!idev)
		return ERR_PTR(-ENOMEM);

	idev->dev.minor = MISC_DYNAMIC_MINOR;
	idev->dev.name = "ion";
	idev->dev.fops = &ion_fops;
	idev->dev.parent = NULL;
	ret = misc_register(&idev->dev);
	if (ret) {
		pr_err("ion: failed to register misc device.\n");
		kfree(idev);
		return ERR_PTR(ret);
	}

	idev->debug_root = debugfs_create_dir("ion", NULL);
	if (!idev->debug_root) {
		pr_err("ion: failed to create debugfs root directory.\n");
		goto debugfs_done;
	}
	idev->heaps_debug_root = debugfs_create_dir("heaps", idev->debug_root);
	if (!idev->heaps_debug_root) {
		pr_err("ion: failed to create debugfs heaps directory.\n");
		goto debugfs_done;
	}
	idev->clients_debug_root = debugfs_create_dir("clients",
						idev->debug_root);
	if (!idev->clients_debug_root)
		pr_err("ion: failed to create debugfs clients directory.\n");

	debug_file = debugfs_create_file("all_heap_info", 0664,
					idev->heaps_debug_root, idev,
					&debug_all_heap_fops);
	if (!debug_file) {
		char buf[256], *path;

		path = dentry_path(idev->heaps_debug_root, buf, 256);
		pr_err("Failed to create heap debugfs at %s all_heap_info\n", path);
	}

	idev->ion_buf_debug_file = debugfs_create_file("ion_buf",
						0666, idev->debug_root, idev, &ion_buf_fops);

debugfs_done:

	idev->custom_ioctl = custom_ioctl;
	idev->buffers = RB_ROOT;
	mutex_init(&idev->buffer_lock);
	init_rwsem(&idev->lock);
	plist_head_init(&idev->heaps);
	idev->clients = RB_ROOT;
	idev->share_buffers = RB_ROOT;
	idev->share_pool_buffers = RB_ROOT;
	idr_init(&idev->idr);
	mutex_init(&idev->share_lock);
	return idev;
}
EXPORT_SYMBOL(ion_device_create);

void ion_device_destroy(struct ion_device *dev)
{
	idr_destroy(&dev->idr);
	misc_deregister(&dev->dev);
	debugfs_remove(dev->ion_buf_debug_file);
	debugfs_remove_recursive(dev->debug_root);
	/* XXX need to free the heaps and clients ? */
	kfree(dev);
}
EXPORT_SYMBOL(ion_device_destroy);
