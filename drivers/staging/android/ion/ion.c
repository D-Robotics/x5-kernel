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
#include <hobot_ion_iommu.h>

#define ION_DRIVER_API_VERSION_MAJOR	1
#define ION_DRIVER_API_VERSION_MINOR	0

#define ION_MODULE_TYPE_INTERNAL   0x0		/**< the internal module type.*/
#define ION_MODULE_TYPE_BPU        0x1U		/**< the module type of BPU */

#define ION_MODULE_TYPE_VPU        0x2U		/**< the module type of VPU */

#define ION_MODULE_TYPE_JPU        0x3U		/**< the module type of JPU */

#define ION_MODULE_TYPE_GPU        0x4U		/**< the module type of GPU */

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
	struct list_head hb_list;		/**< ion iovmm map list*/
	struct kref ref;			/**< the reference count.*/
	struct ion_device *dev;		/**< ion device structure.*/
	struct ion_buffer * buffer;	/**< ion buffer */
	struct rb_node hb_node;		/**< ion share pool rbtree node.*/
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
	struct rb_node hb_node;		/**< the rbtree node.*/
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
 * @struct ion_share_group_handle
 * @brief Define the ion share group handle
 * @NO{S21E04C01I}
 */
struct ion_share_group_handle {
	struct kref ref;			/**< the reference.*/
	struct ion_device *dev;		/**< the ion device.*/
	int id;						/**< the ion share handle id.*/
	struct rb_node hb_node;		/**< the rbtree node.*/
	int32_t client_cnt;			/**< the client reference count.*/
	int32_t share_id[ION_MAX_BUFFER_NUM * ION_MAX_SUB_BUFFER_NUM];
};

struct ion_group_data {
	struct ion_client *client;
	int32_t group_id;
	int32_t import_cnt;
	struct rb_node hb_node;
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
//coverity[misra_c_2012_rule_8_9_violation:SUPPRESS], ## violation reason SYSSW_V_8.9_03
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

//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
static int32_t ion_share_pool_notify(struct ion_device *dev,
	int32_t share_id, int32_t import_cnt, int32_t timeout, int32_t retry);

//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
int hb_expand_files(struct files_struct *files, unsigned int nr);

bool ion_buffer_cached(struct ion_buffer *buffer)
{
	return !((buffer->flags & (unsigned long)ION_FLAG_CACHED) == 0UL);
}

/* this function should only be called while dev->lock is held */
static void ion_buffer_add(struct ion_device *dev,
			   struct ion_buffer *buffer)
{
	struct rb_node **p = &dev->buffers.rb_node;
	struct rb_node *parent = NULL;
	struct ion_buffer *entry;

	while (*p != NULL) {
		parent = *p;
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
		entry = rb_entry(parent, struct ion_buffer, hb_node);

		//coverity[misra_c_2012_rule_11_4_violation:SUPPRESS], ## violation reason SYSSW_V_11.4_01
		if ((uint64_t)buffer < (uint64_t)entry) {
			p = &(*p)->rb_left;
		//coverity[misra_c_2012_rule_11_4_violation:SUPPRESS], ## violation reason SYSSW_V_11.4_01
		} else if ((uint64_t)buffer > (uint64_t)entry) {
			p = &(*p)->rb_right;
		} else {
			(void)pr_err("%s: buffer already found.", __func__);
			BUG();
		}
	}

	rb_link_node(&buffer->hb_node, parent, p);
	rb_insert_color(&buffer->hb_node, &dev->buffers);
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
//coverity[HIS_VOCF:SUPPRESS], ## violation reason SYSSW_V_VOCF_01
//coverity[HIS_LEVEL:SUPPRESS], ## violation reason SYSSW_V_LEVEL_01
static struct ion_buffer *ion_buffer_create(struct ion_heap *heap,
				     struct ion_device *dev,
				     unsigned long len,
				     unsigned long align,
				     unsigned long flags)
{
	struct ion_buffer *buffer;
	int ret;

	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	buffer = (struct ion_buffer *)kzalloc(sizeof(*buffer), GFP_KERNEL);
	if (buffer == NULL) {
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		return (struct ion_buffer *)ERR_PTR(-ENOMEM);
	}

	buffer->heap = heap;
	buffer->flags = flags & (~(unsigned long)ION_FLAG_USE_POOL);
	buffer->dev = dev;
	buffer->size = len;
	buffer->priv_buffer = NULL;
	kref_init(&buffer->ref);

	ret = heap->ops->allocate(heap, buffer, len, align, flags);

	if (ret) {
		if ((heap->flags & ION_HEAP_FLAG_DEFER_FREE) == 0UL) {
			//coverity[misra_c_2012_rule_15_1_violation:SUPPRESS], ## violation reason SYSSW_V_15.1_01
			goto err2;
		}

		(void)ion_heap_freelist_drain(heap, 0);
		ret = heap->ops->allocate(heap, buffer, len, align,
					  flags);
		if (ret) {
			(void)pr_err("%s: ion heap alloc failed[%d]\n", __func__, ret);
			//coverity[misra_c_2012_rule_15_1_violation:SUPPRESS], ## violation reason SYSSW_V_15.1_01
			goto err2;
		}
	}

	if (buffer->hb_sg_table == NULL) {
		WARN_ONCE(1, "This heap needs to set the sgtable");
		ret = -EINVAL;
		//coverity[misra_c_2012_rule_15_1_violation:SUPPRESS], ## violation reason SYSSW_V_15.1_01
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
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	return (struct ion_buffer *)ERR_PTR(ret);
}

//coverity[HIS_VOCF:SUPPRESS], ## violation reason SYSSW_V_VOCF_01
void ion_buffer_destroy(struct ion_buffer *buffer)
{
#if IS_ENABLED(CONFIG_HOBOT_IOMMU)
	struct ion_iovm_map *iovm_map;
	struct ion_iovm_map *tmp;
#endif
	if (buffer->kmap_cnt > 0) {
		(void)pr_warn_once("%s: buffer still mapped in the kernel\n",
			     __func__);
		buffer->heap->ops->unmap_kernel(buffer->heap, buffer);
	}
#if IS_ENABLED(CONFIG_HOBOT_IOMMU)
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
	list_for_each_entry_safe(iovm_map, tmp, &buffer->iovas, list) { /* PRQA S 3673,0497,2810,1020,3432,0306,1021 */
		(void)hobot_iovmm_unmap_sg(iovm_map->dev, iovm_map->iova, iovm_map->size);
		list_del(&iovm_map->list);
		kfree(iovm_map);
	}
#endif

	buffer->heap->ops->free(buffer);

	kfree(buffer);
}

//coverity[HIS_VOCF:SUPPRESS], ## violation reason SYSSW_V_VOCF_01
static void _ion_buffer_destroy(struct kref *hb_kref)
{
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
	struct ion_buffer *buffer = container_of(hb_kref, struct ion_buffer, ref);
	struct ion_heap *heap = buffer->heap;
	struct ion_device *dev = buffer->dev;

	if (buffer->priv_buffer != NULL) {
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		struct ion_buffer *buffer2 = (struct ion_buffer *)buffer->priv_buffer;

		(void)pr_info("ready to free buffer2, the size is %lx, the heap type is %d, the heap name is %s\n",
					buffer2->size, buffer2->heap->type, buffer2->heap->name);
		mutex_lock(&dev->buffer_lock);
		rb_erase(&buffer2->hb_node, &dev->buffers);
		mutex_unlock(&dev->buffer_lock);
		buffer->size = buffer->size - buffer2->size;

		if (heap->flags & ION_HEAP_FLAG_DEFER_FREE)
			ion_heap_freelist_add(heap, buffer2);
		else
			ion_buffer_destroy(buffer2);
	}

	mutex_lock(&dev->buffer_lock);
	rb_erase(&buffer->hb_node, &dev->buffers);
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
	if (buffer->handle_count == 0) {
		struct task_struct *task;

		task = current->group_leader;
		//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
		//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
		(void)get_task_comm(buffer->task_comm, task);
		buffer->hb_pid = task_pid_nr(task);
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

	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	share_hd = (struct ion_share_handle *)kzalloc(sizeof(struct ion_share_handle), GFP_KERNEL);
	if (share_hd == NULL) {
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		return (struct ion_share_handle *)ERR_PTR(-ENOMEM);
	}

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
static void ion_share_handle_destroy(struct kref *hb_kref)
{
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
	struct ion_share_handle *share_hd = container_of(hb_kref, struct ion_share_handle, ref);
	struct ion_device *dev = share_hd->dev;
	struct ion_buffer *buffer = share_hd->buffer;

	(void)idr_remove(&dev->shd_idr, (unsigned long)share_hd->id);
	//coverity[misra_c_2012_rule_11_4_violation:SUPPRESS], ## violation reason SYSSW_V_11.4_01
	if (!RB_EMPTY_NODE(&share_hd->hb_node))
		rb_erase(&share_hd->hb_node, &dev->share_buffers);

	(void)ion_buffer_put(buffer);

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
	if (share_hd->client_cnt == 0) {
		(void)pr_warn("%s: Double removing share handle(share id %d) detected! bailing...\n",
			__func__, share_hd->id);
	} else {
		share_hd->client_cnt--;
	}
	wake_up_interruptible(&share_hd->client_cnt_wait_q);
	mutex_unlock(&share_hd->share_hd_lock);
}

static struct ion_group_data *ion_group_data_create(struct ion_client *client, int32_t group_id)
{
	struct ion_group_data *group_data;

	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	group_data = (struct ion_group_data *)kzalloc(sizeof(*group_data), GFP_KERNEL);
	if (group_data == NULL) {
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		return (struct ion_group_data *)ERR_PTR(-ENOMEM);
	}

	group_data->client = client;
	group_data->group_id = group_id;
	group_data->import_cnt = 1;

	return group_data;
}

//coverity[HIS_LEVEL:SUPPRESS], ## violation reason SYSSW_V_LEVEL_01
static int32_t ion_group_data_add(struct ion_client *client, struct ion_group_data *group_data)
{
	struct rb_node *parent = NULL;
	struct rb_node **p;
	struct ion_group_data *entry;

	p = &client->group_datas.rb_node;
	while (*p != NULL) {
		parent = *p;
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
		entry = rb_entry(parent, struct ion_group_data, hb_node);

		if (group_data->group_id < entry->group_id)
			p = &(*p)->rb_left;
		else if (group_data->group_id > entry->group_id)
			p = &(*p)->rb_right;
		else
			WARN(true, "%s: group id already found.", __func__);
	}
	rb_link_node(&group_data->hb_node, parent, p);
	rb_insert_color(&group_data->hb_node, &client->group_datas);

	return 0;
}

static void ion_group_data_destroy(struct ion_group_data *group_data)
{
	struct ion_client *client = group_data->client;

	//coverity[misra_c_2012_rule_11_4_violation:SUPPRESS], ## violation reason SYSSW_V_11.4_01
	if (!RB_EMPTY_NODE(&group_data->hb_node))
		rb_erase(&group_data->hb_node, &client->group_datas);

	kfree(group_data);
}

static void ion_group_data_get(struct ion_group_data *group_data)
{
	struct ion_client *client;

	client = group_data->client;
	group_data->import_cnt++;
}

static void ion_group_data_put(struct ion_group_data *group_data)
{
	struct ion_client *client;

	if (group_data->import_cnt == 0) {
		pr_warn("%s: Double group data(group id %d) detected! bailing...\n",
			__func__, group_data->group_id);
		return;
	}

	client = group_data->client;
	mutex_lock(&client->group_lock);
	group_data->import_cnt--;
	if (group_data->import_cnt == 0) {
		ion_group_data_destroy(group_data);
	}
	mutex_unlock(&client->group_lock);
}

static void ion_group_data_put_nolock(struct ion_group_data *group_data)
{
	struct ion_client *client;

	if (group_data->import_cnt == 0) {
		pr_warn("%s: Double group data(group id %d) detected! bailing...\n",
			__func__, group_data->group_id);
		return;
	}

	client = group_data->client;
	group_data->import_cnt--;
	if (group_data->import_cnt == 0) {
		ion_group_data_destroy(group_data);
	}
}

static struct ion_group_data *ion_group_data_lookup(struct ion_client *client, int32_t group_id)
{
	struct rb_node *n = client->group_datas.rb_node;

	while (n != NULL) {
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
		struct ion_group_data *entry = rb_entry(n, struct ion_group_data, hb_node);

		if (group_id < entry->group_id)
			n = n->rb_left;
		else if (group_id > entry->group_id)
			n = n->rb_right;
		else
			return entry;
	}

	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	return ERR_PTR(-EINVAL);
}

static struct ion_share_group_handle *ion_share_handle_group_create(struct ion_device *dev)
{
	struct ion_share_group_handle *share_group_hd;

	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	share_group_hd = (struct ion_share_group_handle *)kzalloc(sizeof(struct ion_share_group_handle), GFP_KERNEL);
	if (share_group_hd == NULL) {
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		return (struct ion_share_group_handle *)ERR_PTR(-ENOMEM);
	}

	kref_init(&share_group_hd->ref);
	share_group_hd->dev = dev;
	share_group_hd->client_cnt++;

	return share_group_hd;
}

static void ion_share_handle_group_destroy(struct kref *hb_kref)
{
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
	struct ion_share_group_handle *share_group_hd = container_of(hb_kref, struct ion_share_group_handle, ref);
	struct ion_device *dev = share_group_hd->dev;

	idr_remove(&dev->group_idr, (unsigned long)share_group_hd->id);
	//coverity[misra_c_2012_rule_11_4_violation:SUPPRESS], ## violation reason SYSSW_V_11.4_01
	if (!RB_EMPTY_NODE(&share_group_hd->hb_node))
		rb_erase(&share_group_hd->hb_node, &dev->share_groups);

	kfree(share_group_hd);
}

//coverity[HIS_LEVEL:SUPPRESS], ## violation reason SYSSW_V_LEVEL_01
static int32_t ion_share_handle_group_add(struct ion_device *dev, struct ion_share_group_handle *share_group_hd)
{
	struct rb_node *parent = NULL;
	struct rb_node **p;
	struct ion_share_group_handle *entry;
	int id;

	id = idr_alloc(&dev->group_idr, share_group_hd, 1, 0, GFP_KERNEL);
	if (id < 0) {
		pr_err("%s: failed alloc idr [%d]\n", __func__, id);
		return id;
	}
	share_group_hd->id = id;

	p = &dev->share_groups.rb_node;
	while (*p != NULL) {
		parent = *p;
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
		entry = rb_entry(parent, struct ion_share_group_handle, hb_node);

		//coverity[misra_c_2012_rule_11_4_violation:SUPPRESS], ## violation reason SYSSW_V_11.4_01
		if ((uint64_t)share_group_hd < (uint64_t)entry)
			p = &(*p)->rb_left;
		//coverity[misra_c_2012_rule_11_4_violation:SUPPRESS], ## violation reason SYSSW_V_11.4_01
		else if ((uint64_t)share_group_hd > (uint64_t)entry)
			p = &(*p)->rb_right;
		else
			WARN(true, "%s: ion share group handle already found.", __func__);
	}
	rb_link_node(&share_group_hd->hb_node, parent, p);
	rb_insert_color(&share_group_hd->hb_node, &dev->share_groups);

	return 0;
}

static void ion_share_handle_group_get(struct ion_share_group_handle *share_group_hd)
{
	kref_get(&share_group_hd->ref);
}

static int ion_share_handle_group_put(struct ion_share_group_handle *share_group_hd)
{
	struct ion_device *dev = share_group_hd->dev;
	int ret;

	mutex_lock(&dev->share_group_lock);
	ret = kref_put(&share_group_hd->ref, ion_share_handle_group_destroy);
	mutex_unlock(&dev->share_group_lock);

	return ret;
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
static int32_t ion_share_handle_get_share_info(struct ion_share_handle *share_hd)
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

	if (handle->import_consume_cnt == 0) {
		(void)pr_warn("%s: Double unimport handle(share id %d) detected! bailing...\n",
			__func__, handle->share_id);
		return;
	}

	handle->import_consume_cnt--;
	if (share_hd != NULL) {
		mutex_lock(&share_hd->consume_cnt_lock);
		if (share_hd->consume_cnt == 0) {
			(void)pr_warn("%s: Double removing share handle(share id %d) detected! bailing...\n",
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

	if (handle->import_cnt == 0) {
		(void)pr_warn("%s: Double unimport handle(share id %d) detected! bailing...\n",
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

	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	handle = (struct ion_handle *)kzalloc(sizeof(*handle), GFP_KERNEL);
	if (handle == NULL) {
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		return (struct ion_handle *)ERR_PTR(-ENOMEM);
	}
	kref_init(&handle->ref);
	//coverity[misra_c_2012_rule_11_4_violation:SUPPRESS], ## violation reason SYSSW_V_11.4_01
	RB_CLEAR_NODE(&handle->hb_node);
	handle->client = client;
	ion_buffer_get(buffer);
	ion_buffer_add_to_handle(buffer);
	handle->buffer = buffer;
	handle->import_cnt++;
	handle->import_consume_cnt = 0;

	return handle;
}

static void ion_handle_kmap_put(struct ion_handle *handle);

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
static void ion_handle_destroy(struct kref *hb_kref)
{
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
	struct ion_handle *handle = container_of(hb_kref, struct ion_handle, ref);
	struct ion_client *client = handle->client;
	struct ion_buffer *buffer = handle->buffer;
	struct ion_share_handle *share_hd;
	struct ion_device *dev = client->dev;

	if (handle->share_id != 0) {
		mutex_lock(&dev->share_lock);
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
		share_hd = idr_find(&dev->shd_idr, (unsigned long)handle->share_id);
		mutex_unlock(&dev->share_lock);
		if (IS_ERR_OR_NULL(share_hd)) {
			(void)pr_err("%s: find ion share handle failed [%ld].\n",
					__func__, PTR_ERR(share_hd));
		} else {
			while (handle->import_cnt) {
				ion_handle_import_put(handle);
			}
			while (handle->import_consume_cnt) {
				ion_handle_import_consume_put(handle);
			}
			(void)ion_share_handle_put(share_hd);
		}
	}
	mutex_lock(&buffer->lock);
	while (handle->kmap_cnt)
		ion_handle_kmap_put(handle);
	mutex_unlock(&buffer->lock);

	(void)idr_remove(&client->handle_idr, (unsigned long)handle->id);
	//coverity[misra_c_2012_rule_11_4_violation:SUPPRESS], ## violation reason SYSSW_V_11.4_01
	if (!RB_EMPTY_NODE(&handle->hb_node))
		rb_erase(&handle->hb_node, &client->handles);

	ion_buffer_remove_from_handle(buffer);
	(void)ion_buffer_put(buffer);

	kfree(handle);
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
	if (kref_read(&handle->ref) + 1U == 0U) {
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		return (struct ion_handle*)ERR_PTR(-EOVERFLOW);
	}
	ion_handle_get(handle);
	return handle;
}

static int ion_handle_put_nolock(struct ion_handle *handle)
{
	int ret;

	ret = kref_put(&handle->ref, ion_handle_destroy);

	return ret;
}

int ion_handle_put(struct ion_handle *handle)
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

	while (n != NULL) {
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
		struct ion_handle *entry = rb_entry(n, struct ion_handle, hb_node);

		//coverity[misra_c_2012_rule_11_4_violation:SUPPRESS], ## violation reason SYSSW_V_11.4_01
		if ((uint64_t)buffer < (uint64_t)entry->buffer)
			n = n->rb_left;
		//coverity[misra_c_2012_rule_11_4_violation:SUPPRESS], ## violation reason SYSSW_V_11.4_01
		else if ((uint64_t)buffer > (uint64_t)entry->buffer)
			n = n->rb_right;
		else
			return entry;
	}
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	return (struct ion_handle *)ERR_PTR(-EINVAL);
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

	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
	handle = idr_find(&client->handle_idr, (unsigned long)id);
	if (handle != NULL)
		return ion_handle_get_check_overflow(handle);

	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	return (struct ion_handle *)ERR_PTR(-EINVAL);
}
//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
EXPORT_SYMBOL(ion_handle_get_by_id_nolock);

static struct ion_handle *ion_handle_get_by_id(struct ion_client *client,
						int id)
{
	struct ion_handle *handle;

	mutex_lock(&client->lock);
	handle = ion_handle_get_by_id_nolock(client, id);
	mutex_unlock(&client->lock);

	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	return (handle != NULL) ? handle : (struct ion_handle *)ERR_PTR(-EINVAL);
}

static bool ion_handle_validate(struct ion_client *client,
				struct ion_handle *handle)
{
	WARN_ON(!mutex_is_locked(&client->lock));
	return idr_find(&client->handle_idr, (unsigned long)handle->id) == handle;
}

//coverity[HIS_LEVEL:SUPPRESS], ## violation reason SYSSW_V_LEVEL_01
static int ion_handle_add(struct ion_client *client, struct ion_handle *handle)
{
	int id;
	struct rb_node **p = &client->handles.rb_node;
	struct rb_node *parent = NULL;
	struct ion_handle *entry;

	id = idr_alloc(&client->handle_idr, handle, 1, 0, GFP_KERNEL);
	if (id < 0) {
		(void)pr_err("%s: failed alloc idr [%d]\n", __func__, id);
		return id;
	}

	handle->id = id;

	while (*p != NULL) {
		parent = *p;
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
		entry = rb_entry(parent, struct ion_handle, hb_node);

		//coverity[misra_c_2012_rule_11_4_violation:SUPPRESS], ## violation reason SYSSW_V_11.4_01
		if ((uint64_t)handle->buffer < (uint64_t)entry->buffer)
			p = &(*p)->rb_left;
		//coverity[misra_c_2012_rule_11_4_violation:SUPPRESS], ## violation reason SYSSW_V_11.4_01
		else if ((uint64_t)handle->buffer > (uint64_t)entry->buffer)
			p = &(*p)->rb_right;
		else
			WARN(true, "%s: buffer already found.", __func__);
	}

	rb_link_node(&handle->hb_node, parent, p);
	rb_insert_color(&handle->hb_node, &client->handles);

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
//coverity[HIS_LEVEL:SUPPRESS], ## violation reason SYSSW_V_LEVEL_01
static int ion_share_handle_add(struct ion_device *dev, struct ion_share_handle * share_hd,
				struct ion_handle *handle)
{
	struct rb_node *parent = NULL;
	struct rb_node **p;
	struct ion_share_handle *entry;
	int id;

	id = idr_alloc(&dev->shd_idr, share_hd, 1, 0, GFP_KERNEL);
	if (id < 0) {
		(void)pr_err("%s: failed alloc idr [%d]\n", __func__, id);
		return id;
	}
	share_hd->id = id;
	share_hd->buffer->share_id = id;
	handle->share_id = id;
	handle->sh_hd = share_hd;

	p = &dev->share_buffers.rb_node;
	while (*p != NULL) {
		parent = *p;
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
		entry = rb_entry(parent, struct ion_share_handle, hb_node);

		//coverity[misra_c_2012_rule_11_4_violation:SUPPRESS], ## violation reason SYSSW_V_11.4_01
		if ((uint64_t)share_hd < (uint64_t)entry)
			p = &(*p)->rb_left;
		//coverity[misra_c_2012_rule_11_4_violation:SUPPRESS], ## violation reason SYSSW_V_11.4_01
		else if ((uint64_t)share_hd > (uint64_t)entry)
			p = &(*p)->rb_right;
		else
			WARN(true, "%s: share handle already found.", __func__);
	}
	rb_link_node(&share_hd->hb_node, parent, p);
	rb_insert_color(&share_hd->hb_node, &dev->share_buffers);

	return 0;
}

static int32_t ion_check_import_phys(struct ion_buffer *buffer, struct ion_share_handle_data *data)
{
	int32_t ret = 0;
	size_t len = 0, in_size = data->size;
	phys_addr_t phys_addr = 0, in_phys = data->phys_addr;
	phys_addr_t phys_addr_end = 0,  in_phys_end = 0;

	if (in_size == 0UL) {
		(void)pr_err("%s(%d): invalid size parameter\n", __func__, __LINE__);
		return -EINVAL;
	}

	ret = buffer->heap->ops->phys(buffer->heap, buffer, &phys_addr, &len);

	phys_addr_end = phys_addr + len;
	in_phys_end = in_phys + in_size;

	//1. whether warp
	if (phys_addr_end > phys_addr) {
		//2. contigious buffer check
		if ((in_phys < phys_addr) || (in_phys_end <= phys_addr) ||
			(in_phys_end > phys_addr_end) ||
			(in_phys >= phys_addr_end)) {
			(void)pr_err("%s(%d): Invalid import buffer physical address 0x%llx, size %lu.,Should be 0x%llx and %lu.\n",
				__func__, __LINE__, in_phys, in_size, phys_addr, len);
			return -EINVAL;
		}
	} else if (buffer->priv_buffer != NULL) {
		//3. sg warp buffer check，normal buffer will not warp
		if (((in_phys < phys_addr) && (in_phys >= phys_addr_end)) ||
		((in_phys_end <= phys_addr) && (in_phys_end > phys_addr_end))) {
			(void)pr_err("%s(%d): Invalid import buffer physical address 0x%llx, size %lu. sg buffer,"
				"Should be 0x%llx and %lu.\n", __func__, __LINE__, in_phys, in_size, phys_addr, len);
			return -EINVAL;
		}
	} else {
		//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
		pr_debug("%s:%d check import phys success\n", __func__, __LINE__);
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
//coverity[misra_c_2012_rule_8_7_violation:SUPPRESS], ## violation reason SYSSW_V_8.7_01
struct ion_handle *ion_import_dma_buf_with_shareid(struct ion_client *client, int32_t share_id)
{
	struct ion_buffer *buffer;
	struct ion_handle *handle;
	struct ion_device *dev;
	struct ion_share_handle *share_hd;
	int ret;

	if (client == NULL) {
		(void)pr_err("%s(%d): client struct is NULL\n", __func__, __LINE__);
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		return (struct ion_handle *)ERR_PTR(-EINVAL);
	}
	dev = client->dev;

	mutex_lock(&dev->share_lock);
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
	share_hd = idr_find(&dev->shd_idr, (unsigned long)share_id);
	if (IS_ERR_OR_NULL(share_hd)) {
		mutex_unlock(&dev->share_lock);
		(void)pr_err("%s: find ion share handle failed [%ld].\n",
				__func__, PTR_ERR(share_hd));
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		return (share_hd != NULL) ? (struct ion_handle *)ERR_CAST(share_hd): (struct ion_handle *)ERR_PTR(-EINVAL);
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
		(void)ion_share_handle_put(share_hd);
		(void)ion_buffer_put(buffer);
		return handle;
	}

	handle = ion_handle_create(client, buffer);
	if (IS_ERR(handle)) {
		mutex_unlock(&client->lock);
		(void)ion_share_handle_put(share_hd);
		(void)ion_buffer_put(buffer);
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
		(void)ion_handle_put(handle);
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		//coverity[misra_c_2012_directive_4_7_violation], ## violation reason SYSSW_V_4.7_01
		handle = (struct ion_handle *)ERR_PTR(ret);
		(void)ion_share_handle_put(share_hd);
		(void)ion_buffer_put(buffer);
		return handle;
	}
	(void)ion_buffer_put(buffer);

	return handle;
}
//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
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
static struct ion_handle *ion_share_handle_import_dma_buf(struct ion_client *client,
					struct ion_share_handle_data *data)
{
	struct ion_buffer *buffer;
	struct ion_handle *handle;
	struct ion_device *dev = client->dev;
	struct ion_share_handle *share_hd;
	int share_id = data->sh_handle;
	int ret;

	mutex_lock(&dev->share_lock);
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
	share_hd = idr_find(&dev->shd_idr, (unsigned long)share_id);
	if (IS_ERR_OR_NULL(share_hd)) {
		mutex_unlock(&dev->share_lock);
		(void)pr_err("%s: find ion share handle failed [%ld].\n",
				__func__, PTR_ERR(share_hd));
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		return (share_hd != NULL) ? (struct ion_handle *)ERR_CAST(share_hd): (struct ion_handle *)ERR_PTR(-EINVAL);
	}
	buffer = share_hd->buffer;
	data->flags = (int64_t)buffer->flags;
	ion_buffer_get(buffer);
	ion_share_handle_get(share_hd);
	mutex_unlock(&dev->share_lock);

	//check the physical addr
	ret = ion_check_import_phys(buffer, data);
	if (ret != 0) {
		(void)pr_err("%s:Invalid import buffer physical address check failed\n", __func__);
		(void)ion_share_handle_put(share_hd);
		(void)ion_buffer_put(buffer);
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		return (struct ion_handle *)ERR_PTR(-EINVAL);
	}

	mutex_lock(&client->lock);
	/* if a handle exists for this buffer just take a reference to it */
	handle = ion_handle_lookup(client, buffer);
	if (!IS_ERR(handle)) {
		ion_handle_get(handle);
		ion_handle_import_get(handle);
		mutex_unlock(&client->lock);
		(void)ion_share_handle_put(share_hd);
		(void)ion_buffer_put(buffer);
		return handle;
	}

	handle = ion_handle_create(client, buffer);
	if (IS_ERR(handle)) {
		mutex_unlock(&client->lock);
		(void)ion_share_handle_put(share_hd);
		(void)ion_buffer_put(buffer);
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
		(void)ion_handle_put(handle);
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		//coverity[misra_c_2012_directive_4_7_violation], ## violation reason SYSSW_V_4.7_01
		handle = (struct ion_handle *)ERR_PTR(ret);
		(void)ion_share_handle_put(share_hd);
		(void)ion_buffer_put(buffer);
		return handle;
	}

	(void)ion_buffer_put(buffer);
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
//coverity[HIS_VOCF:SUPPRESS], ## violation reason SYSSW_V_VOCF_01
static struct ion_buffer *ion_sg_buffer_create(struct ion_heap *heap,
				     struct ion_device *dev,
				     unsigned long len,
				     unsigned long align,
				     unsigned long flags)
{
	struct ion_buffer *buffer[ION_SRAM_SG_NUM];
	int32_t ret, i;
	uint64_t avail_mem = 0, max_contigous = 0, res_mem = 0;
	struct sg_table *table;
	struct page *hb_page[ION_SRAM_SG_NUM];
	uint32_t heap_id_mask = 0;
	size_t size[ION_SRAM_SG_NUM];
	struct scatterlist *sg;
	uint32_t j = 0;

	ret = get_carveout_info(heap, &avail_mem, &max_contigous);
	if (avail_mem == 0U) {
		(void)pr_err("%s: ion sram heap avail memory is %llx\n", __func__, avail_mem);
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		return (struct ion_buffer *)ERR_PTR(-ENOMEM);
	}

	buffer[ION_SRAM_SG_FIR] = ion_buffer_create(heap, dev, max_contigous, align, flags);
	if (IS_ERR_OR_NULL(buffer[ION_SRAM_SG_FIR])) {
		(void)pr_err("%s: ion alloc memory from sram for scatterlist failed [%pK]\n", __func__, buffer[ION_SRAM_SG_FIR]);
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		return (struct ion_buffer *)ERR_PTR(-ENOMEM);
	}

	res_mem = len - max_contigous;
	buffer[ION_SRAM_SG_SEC] = ion_buffer_create(heap, dev, res_mem, align, flags);
	if (IS_ERR_OR_NULL(buffer[ION_SRAM_SG_SEC])) {
		heap_id_mask |= (((uint32_t)ION_HEAP_CARVEOUT_MASK | (uint32_t)ION_HEAP_TYPE_DMA_MASK) | (uint32_t)ION_HEAP_TYPE_CMA_RESERVED_MASK);
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
		plist_for_each_entry(heap, &dev->heaps, hb_node) {
			if ((((uint32_t)1U << (uint32_t)heap->type) & heap_id_mask) == 0U)
				continue;
			buffer[ION_SRAM_SG_SEC] = ion_buffer_create(heap, dev, res_mem, align, flags);
			if (!IS_ERR_OR_NULL(buffer[ION_SRAM_SG_SEC]))
				break;
		}
		if (IS_ERR_OR_NULL(buffer[ION_SRAM_SG_SEC])) {
			(void)pr_err("%s: ion alloc memory from other heap failed [%pK]\n", __func__, buffer[ION_SRAM_SG_SEC]);
			(void)ion_buffer_put(buffer[ION_SRAM_SG_FIR]);
			//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
			return (struct ion_buffer *)ERR_PTR(-ENOMEM);
		}
		(void)pr_info("alloc from other heap success, the size is %lx, the heap type is %d, the heap name is %s\n",
					buffer[ION_SRAM_SG_SEC]->size, buffer[ION_SRAM_SG_SEC]->heap->type, buffer[ION_SRAM_SG_SEC]->heap->name);
	}

	for (i = 0; i < ION_SRAM_SG_NUM; i++) {
		hb_page[i] = sg_page(buffer[i]->hb_sg_table->sgl);
		size[i] = buffer[i]->size;
	}

	sg_free_table(buffer[ION_SRAM_SG_FIR]->hb_sg_table);

	//coverity[misra_c_2012_directive_4_12_violation], ## violation reason SYSSW_V_4.12_02
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	table = (struct sg_table *)kmalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (table == NULL) {
		(void)ion_buffer_put(buffer[ION_SRAM_SG_SEC]);
		(void)ion_buffer_put(buffer[ION_SRAM_SG_FIR]);
		(void)pr_err("%s: alloc table structure failed\n", __func__);
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		return (struct ion_buffer *)ERR_PTR(-ENOMEM);
	}

	ret = sg_alloc_table(table, ION_SRAM_SG_NUM, GFP_KERNEL);
	if (ret) {
		kfree(table);
		(void)ion_buffer_put(buffer[ION_SRAM_SG_SEC]);
		(void)ion_buffer_put(buffer[ION_SRAM_SG_FIR]);
		(void)pr_err("%s: sg alloc table failed\n", __func__);
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		return (struct ion_buffer *)ERR_PTR(-ENOMEM);
	}

	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
	for_each_sg(table->sgl, sg, table->nents, j) {
		sg_set_page(sg, hb_page[j], (uint32_t)size[j], 0);
	}

	buffer[ION_SRAM_SG_FIR]->priv_virt = table;
	buffer[ION_SRAM_SG_FIR]->hb_sg_table = table;
	buffer[ION_SRAM_SG_FIR]->priv_buffer = (void *)buffer[ION_SRAM_SG_SEC];
	buffer[ION_SRAM_SG_FIR]->size = ((uint64_t)size[ION_SRAM_SG_FIR] + (uint64_t)size[ION_SRAM_SG_SEC]);

	//the seconed buffer handle count is always 1
	ion_buffer_add_to_handle(buffer[ION_SRAM_SG_SEC]);

	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
	for_each_sg(buffer[ION_SRAM_SG_FIR]->hb_sg_table->sgl, sg, table->nents, j) {
		(void)pr_info("%d:the alloc size is %x\n", j, sg->length);
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
//coverity[HIS_CCM:SUPPRESS], ## violation reason SYSSW_V_CCM_01
//coverity[HIS_STMT:SUPPRESS], ## violation reason SYSSW_V_STMT_01
//coverity[HIS_VOCF:SUPPRESS], ## violation reason SYSSW_V_VOCF_01
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
	uint32_t type = 0;
	struct ion_share_handle * share_hd;
	struct ion_buffer *private_buffer;

	if (client == NULL) {
		(void)pr_err("%s: client cannot be null\n", __func__);
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		return (struct ion_handle *)ERR_PTR(-EINVAL);
	}

	dev = client->dev;

	/*
	 * traverse the list of heaps available in this system in priority
	 * order.  If the heap type is supported by the client, and matches the
	 * request of the caller allocate from it.  Repeat until allocate has
	 * succeeded or all heaps have been tried
	 */
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	len = PAGE_ALIGN(len);

	if (len == 0UL) {
		(void)pr_err("%s: len invalid\n", __func__);
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		return (struct ion_handle *)ERR_PTR(-EINVAL);
	}

	down_read(&dev->lock);
	type = flags >> 16;
	flags = flags & 0xffffU;
	/* bpu default not use cma heap */
	if (type == 0U) {
		heap_id_mask = ION_HEAP_TYPE_CMA_RESERVED_MASK;
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
		plist_for_each_entry(heap, &dev->heaps, hb_node) {
			/* if the caller didn't specify this heap id */
			if ((((uint32_t)1U << (uint32_t)heap->type) & heap_id_mask) != 0U) {
				heap_march = 1;
			}
		}
		/* if no cma reserved heap, use cma*/
		if (heap_march == 0) {
			heap_id_mask &= ~(uint32_t)ION_HEAP_TYPE_CMA_RESERVED_MASK;
			heap_id_mask |= (uint32_t)ION_HEAP_TYPE_DMA_MASK;
		}
	} else {
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
		plist_for_each_entry(heap, &dev->heaps, hb_node) {
			/* if the caller didn't specify this heap id */
			if ((((uint32_t)1U << (uint32_t)heap->type) & heap_id_mask) != 0U) {
				heap_march = 1;
			}
		}
		/* if no carveout heap, use cma */
		if (heap_march == 0) {
			heap_id_mask |= (uint32_t)ION_HEAP_TYPE_DMA_MASK;
		}
	}
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
	plist_for_each_entry(heap, &dev->heaps, hb_node) {
		/* if the caller didn't specify this heap id */
		if ((((uint32_t)1U << (uint32_t)heap->type) & heap_id_mask) == 0U)
			continue;
		buffer = ion_buffer_create(heap, dev, len, align, flags);
		if (IS_ERR(buffer) && ((((uint32_t)1U << (uint32_t)heap->type) & (uint32_t)ION_HEAP_TYPE_CUSTOM_MASK) != 0U)) {
			if ((flags & (uint32_t)ION_FLAG_USE_POOL) != 0U) {
				pr_err("%s: alloc from sram heap for pool failed, sg not support for memory pool now\n", __func__);
				up_read(&dev->lock);
				//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
				return (struct ion_handle *)ERR_PTR(-ENOMEM);
			}

			pr_info("%s: alloc from sram heap failed, ready to alloc sg\n", __func__);
			buffer = ion_sg_buffer_create(heap, dev, len, align, flags);
		} else {
			break;
		}
		if (IS_ERR(buffer)) {
			pr_err("%s: buffer create failed\n", __func__);
			up_read(&dev->lock);
			//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
			return (struct ion_handle *)ERR_PTR(-ENOMEM);
		}
		//initial the child buffer paramters
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		private_buffer = (struct ion_buffer *)buffer->priv_buffer;
		private_buffer->private_flags = type;
		//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
		//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
		get_task_comm(private_buffer->task_comm, current->group_leader);
		private_buffer->hb_pid = task_pid_nr(current);
	}
	/* if carveout/cma reserved can't alloc the mem, try use cma*/
	if ((buffer == NULL) || IS_ERR(buffer)) {
		if ((heap_id_mask & (uint32_t)ION_HEAP_CARVEOUT_MASK) > 0U) {
			//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
			pr_debug("Retry alloc carveout 0x%lxByte from cma reserved heap\n", len);
			last_heap_id_mask = heap_id_mask;
			heap_id_mask &= ~(uint32_t)ION_HEAP_CARVEOUT_MASK;
			heap_id_mask |= (uint32_t)ION_HEAP_TYPE_CMA_RESERVED_MASK;
		} else if ((heap_id_mask & (uint32_t)ION_HEAP_TYPE_CMA_RESERVED_MASK) > 0U) {
			//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
			pr_debug("Retry alloc cma reserved 0x%lxByte from carveout heap\n", len);
			last_heap_id_mask = heap_id_mask;
			heap_id_mask &= ~(uint32_t)ION_HEAP_TYPE_CMA_RESERVED_MASK;
			heap_id_mask |= (uint32_t)ION_HEAP_CARVEOUT_MASK;
		} else if ((heap_id_mask & (uint32_t)ION_HEAP_TYPE_DMA_MASK) > 0U) {
			//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
			pr_debug("Retry alloc cma  0x%lxByte from cma reserved heap\n", len);
			last_heap_id_mask = heap_id_mask;
			heap_id_mask &= ~(uint32_t)ION_HEAP_TYPE_DMA_MASK;
			heap_id_mask |= (uint32_t)ION_HEAP_TYPE_CMA_RESERVED_MASK;
		} else {
			//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
			pr_debug("Not find replace heap\n");
		}
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
		plist_for_each_entry(heap, &dev->heaps, hb_node) {
			/* if the caller didn't specify this heap id */
			if ((((uint32_t)1U << (uint32_t)heap->type) & heap_id_mask) == 0U)
				continue;
			buffer = ion_buffer_create(heap, dev, len, align, flags);
			if (!IS_ERR(buffer))
				break;
		}
	}

	if ((buffer == NULL) || IS_ERR(buffer)) {
		if ((last_heap_id_mask & (uint32_t)ION_HEAP_TYPE_CMA_RESERVED_MASK) > 0U) {
			//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
			pr_debug("Retry cma reserved alloc 0x%lxByte from cma heap\n", len);
			heap_id_mask &= ~(uint32_t)ION_HEAP_CARVEOUT_MASK;
			heap_id_mask |= (uint32_t)ION_HEAP_TYPE_DMA_MASK;
		} else if ((last_heap_id_mask & (uint32_t)ION_HEAP_CARVEOUT_MASK) > 0U) {
			//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
			pr_debug("Retry alloc carveout 0x%lxByte from cma heap\n", len);
			heap_id_mask &= ~(uint32_t)ION_HEAP_TYPE_CMA_RESERVED_MASK;
			heap_id_mask |= (uint32_t)ION_HEAP_TYPE_DMA_MASK;
		} else if ((last_heap_id_mask & (uint32_t)ION_HEAP_TYPE_DMA_MASK) > 0U) {
			//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
			pr_debug("Retry alloc carveout 0x%lxByte from carveout heap\n", len);
			heap_id_mask &= ~(uint32_t)ION_HEAP_TYPE_CMA_RESERVED_MASK;
			heap_id_mask |= (uint32_t)ION_HEAP_CARVEOUT_MASK;
		} else {
			//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
			pr_debug("Not find replace heap\n");
		}
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
		plist_for_each_entry(heap, &dev->heaps, hb_node) {
			/* if the caller didn't specify this heap id */
			if ((((uint32_t)1U << (uint32_t)heap->type) & heap_id_mask) == 0U)
				continue;
			buffer = ion_buffer_create(heap, dev, len, align, flags);
			if (!IS_ERR(buffer))
				break;
		}
	}
	up_read(&dev->lock);

	if (buffer == NULL) {
		(void)pr_err("%s: buffer create failed\n", __func__);
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		return (struct ion_handle *)ERR_PTR(-ENODEV);
	}

	if (IS_ERR(buffer)) {
		(void)pr_err("%s: buffer create error[%ld]\n",
				__func__, PTR_ERR(buffer));
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		return (struct ion_handle *)ERR_CAST(buffer);
	}
	buffer->private_flags = type;

	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	(void)get_task_comm(buffer->task_comm, current->group_leader);
	buffer->hb_pid = task_pid_nr(current);

	handle = ion_handle_create(client, buffer);

	/*
	 * ion_buffer_create will create a buffer with a ref_cnt of 1,
	 * and ion_handle_create will take a second reference, drop one here
	 */
	(void)ion_buffer_put(buffer);

	if (IS_ERR(handle)) {
		(void)pr_err("%s: handle create error[%ld]\n",
				__func__, PTR_ERR(handle));
		return handle;
	}

	share_hd = ion_share_handle_create(dev, buffer);
	if (IS_ERR(share_hd)) {
		(void)ion_handle_put(handle);
		(void)pr_err("%s: handle create error[%ld]\n",
				__func__, PTR_ERR(share_hd));
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		return (struct ion_handle *)ERR_CAST(share_hd);
	}

	mutex_lock(&dev->share_lock);
	ret = ion_share_handle_add(dev, share_hd, handle);
	mutex_unlock(&dev->share_lock);
	if (ret) {
		(void)pr_err("%s: share handle add failed[%d]\n", __func__, ret);
		(void)ion_share_handle_put(share_hd);
		(void)ion_handle_put(handle);
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		//coverity[misra_c_2012_directive_4_7_violation], ## violation reason SYSSW_V_4.7_01
		handle = (struct ion_handle *)ERR_PTR(ret);
		return handle;
	}

	mutex_lock(&client->lock);
	ret = ion_handle_add(client, handle);
	mutex_unlock(&client->lock);
	if (ret) {
		(void)pr_err("%s: handle add failed[%d]\n", __func__, ret);
		(void)ion_handle_put(handle);
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		handle = (struct ion_handle *)ERR_PTR(ret);
	}

	return handle;
}
//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
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
	struct ion_share_pool_buf *buf_map;
	struct ion_share_pool_buf *tmp;
	struct list_head buf_list;
	struct rb_root *share_pool_buffers;

	INIT_LIST_HEAD(&buf_list);

	share_pool_buffers = &dev->share_pool_buffers;

	for (n = rb_first(share_pool_buffers); n != NULL; n = rb_next(n)) {
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
		entry = rb_entry(n, struct ion_share_pool_buf, hb_node);
		if (entry->client == client) {
			INIT_LIST_HEAD(&entry->hb_list);
			list_add(&entry->hb_list, &buf_list);
		}
	}

	list_for_each_entry_safe(buf_map, tmp, &buf_list, hb_list) { /* PRQA S 3673,0497,2810,1020,3432,0306,1021 */
		list_del(&buf_map->hb_list);
		rb_erase(&buf_map->hb_node, share_pool_buffers);
		kfree(buf_map);
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
	while (*p != NULL) {
		parent = *p;
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
		entry = rb_entry(parent, struct ion_share_pool_buf, hb_node);

		if (share_pool_buf->share_id < entry->share_id) {
			p = &(*p)->rb_left;
		} else if (share_pool_buf->share_id > entry->share_id) {
			p = &(*p)->rb_right;
		} else {
			(void)pr_info("ion_share_pool_buf_add %d failed\n", share_pool_buf->share_id);
			return -EINVAL;
		}
	}
	rb_link_node(&share_pool_buf->hb_node, parent, p);
	rb_insert_color(&share_pool_buf->hb_node, &dev->share_pool_buffers);

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

	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	share_pool_buf = (struct ion_share_pool_buf *)kzalloc(sizeof(struct ion_share_pool_buf), GFP_KERNEL);
	if (share_pool_buf == NULL) {
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		return (struct ion_share_pool_buf *)ERR_PTR(-ENOMEM);
	}

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
static void ion_share_pool_buf_destroy(struct kref *hb_kref)
{
	struct ion_share_pool_buf *share_pool_buf =
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
		container_of(hb_kref, struct ion_share_pool_buf, ref);
	struct ion_device *dev = share_pool_buf->dev;

	//coverity[misra_c_2012_rule_11_4_violation:SUPPRESS], ## violation reason SYSSW_V_11.4_01
	if (!RB_EMPTY_NODE(&share_pool_buf->hb_node))
		rb_erase(&share_pool_buf->hb_node, &dev->share_pool_buffers);

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

	while (n != NULL) {
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
		entry = rb_entry(n, struct ion_share_pool_buf, hb_node);
		if (share_id < entry->share_id)
			n = n->rb_left;
		else if (share_id > entry->share_id)
			n = n->rb_right;
		else
			return entry;
	}
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
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
//coverity[HIS_CCM:SUPPRESS], ## violation reason SYSSW_V_CCM_01
//coverity[HIS_LEVEL:SUPPRESS], ## violation reason SYSSW_V_LEVEL_01
//coverity[HIS_VOCF:SUPPRESS], ## violation reason SYSSW_V_VOCF_01
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
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
	share_hd = idr_find(&dev->shd_idr, (unsigned long)share_id);
	if (IS_ERR_OR_NULL(share_hd)) {
		mutex_unlock(&dev->share_lock);
		(void)pr_err("%s: find ion share handle failed [%ld].\n",
				__func__, PTR_ERR(share_hd));
		return -EINVAL;
	}
	mutex_unlock(&dev->share_lock);

	// share_pool_wait_q timeout last notify
	if (retry > 0) {
		idx = share_pool_sync_idx(share_hd);
		if (idx >= 0 && idx < MAXSYNCNUM) {
			//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
			ret = wait_event_interruptible_timeout(share_hd->share_pool_sync[idx].share_pool_wait_q,
				share_hd->share_pool_sync[idx].share_pool_cnt == 1,	msecs_to_jiffies(timeout));
			if (ret == 0) {
				(void)pr_info("%s:%d, pid: %d, timeout\n", __func__, __LINE__, current->pid);
				return -ETIMEDOUT;
			} else if (ret < 0) {
				(void)pr_err("%s:%d, pid: %d, failed %d\n", __func__, __LINE__, current->pid, ret);
				return ret;
			} else {
				//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
				pr_debug("%s:%d wake up\n", __func__, __LINE__);
			}

			share_hd->share_pool_sync[idx].share_pool_cnt = 0;
			share_hd->share_pool_sync[idx].thread_pid = -1;
			(void)test_and_clear_bit(idx, share_hd->bitmap);
			return 0;
		}
	}

	if (timeout > 0) {
		mutex_lock(&dev->share_lock);
		idx = (int32_t)find_first_zero_bit(share_hd->bitmap, (unsigned long)MAXSYNCNUM);
		if (idx < 0 || idx >= MAXSYNCNUM) {
			mutex_unlock(&dev->share_lock);
			(void)pr_err("%s:%d cannot find sync wq, bitmap %lx\n", __func__, __LINE__, share_hd->bitmap[0]);
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
	ptr[1] = (int16_t)idx;
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
	ret = kfifo_in_spinlocked(&share_pool_buf->client->hb_fifo,
		buf, (size_t)SPBUFNUM * sizeof(int32_t), &share_pool_buf->client->fifo_lock);
	if ((size_t)ret != (size_t)SPBUFNUM * sizeof(int32_t)) {
		mutex_unlock(&dev->share_lock);
		(void)pr_err("%s:%d kfifo overflow.\n", __func__, __LINE__);
		return -EINVAL;
	}

	share_pool_buf->client->wq_cnt = 1;
	wake_up_interruptible(&share_pool_buf->client->wq);
	mutex_unlock(&dev->share_lock);

	if (timeout > 0) {
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
		ret = wait_event_interruptible_timeout(share_hd->share_pool_sync[idx].share_pool_wait_q,
			share_hd->share_pool_sync[idx].share_pool_cnt == 1, msecs_to_jiffies(timeout));
		if (ret == 0) {
			//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
			pr_debug("%s:%d timeout\n", __func__, __LINE__);
			return -ETIMEDOUT;
		} else if (ret < 0) {
			//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
			pr_debug("%s:%d failed %d\n", __func__, __LINE__, ret);
			return ret;
		} else {
			//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
			pr_debug("%s:%d wake up\n", __func__, __LINE__);
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
static int32_t ion_share_pool_wake_up_ref_cnt(struct ion_client *client,
	struct ion_share_pool_data *data)
{
	struct ion_device *dev = client->dev;
	struct ion_share_handle *share_hd;
	int32_t share_id = data->sh_handle;
	int32_t idx = data->import_cnt;

	mutex_lock(&dev->share_lock);
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
	share_hd = idr_find(&dev->shd_idr, (unsigned long)share_id);
	if (IS_ERR_OR_NULL(share_hd)) {
		mutex_unlock(&dev->share_lock);
		(void)pr_err("%s: find ion share handle failed [%ld].\n",
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
		WARN(true, "%s: invalid handle passed to free.\n", __func__);
		return;
	}
	if ((handle->share_id != 0) && (handle->sh_hd != NULL)) {
		ion_handle_import_put(handle);
	}
	(void)ion_handle_put_nolock(handle);
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
	if (client == NULL) {
		(void)pr_err("%s: client cannot be null\n", __func__);
		return;
	}

	if (handle == NULL) {
		(void)pr_err("%s: handle cannot be null\n", __func__);
		return;
	}

	BUG_ON(client != handle->client);

	mutex_lock(&client->lock);
	ion_free_nolock(client, handle);
	mutex_unlock(&client->lock);
}
//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
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
//coverity[HIS_VOCF:SUPPRESS], ## violation reason SYSSW_V_VOCF_01
int ion_phys(struct ion_client *client, int handle_id,
	     phys_addr_t *addr, size_t *len)
{
	struct ion_buffer *buffer;
	int ret;
	struct ion_handle *handle;
	struct mutex *hb_lock;

	if (client == NULL) {
		(void)pr_err("%s: client cannot be null\n", __func__);
		return -EINVAL;
	}

	if (addr == NULL) {
		(void)pr_err("%s: addr cannot be null\n", __func__);
		return -EINVAL;
	}

	if (len == NULL) {
		(void)pr_err("%s: len cannot be null\n", __func__);
		return -EINVAL;
	}

	hb_lock = &client->lock;
	mutex_lock(hb_lock);

	handle = ion_handle_get_by_id_nolock(client, handle_id);
	if (IS_ERR(handle)) {
		mutex_unlock(hb_lock);
		(void)pr_err("%s: find ion handle failed [%ld].",
				__func__, PTR_ERR(handle));
		return (int)PTR_ERR(handle);
	}

	buffer = handle->buffer;

	if (buffer->heap->ops->phys == NULL) {
		(void)pr_err("%s: ion_phys is not implemented by this heap (name=%s, type=%d).\n",
			__func__, buffer->heap->name, buffer->heap->type);
		(void)ion_handle_put_nolock(handle);
		mutex_unlock(hb_lock);
		return -ENODEV;
	}
	ret = buffer->heap->ops->phys(buffer->heap, buffer, addr, len);
	(void)ion_handle_put_nolock(handle);
	mutex_unlock(hb_lock);
	return ret;
}
//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
EXPORT_SYMBOL(ion_phys);

//coverity[HIS_LEVEL:SUPPRESS], ## violation reason SYSSW_V_LEVEL_01
static void *ion_buffer_kmap_get(struct ion_buffer *buffer)
{
	void *vaddr;

	if (buffer->kmap_cnt) {
		buffer->kmap_cnt++;
		return buffer->vaddr;
	}
	vaddr = buffer->heap->ops->map_kernel(buffer->heap, buffer);
	if (WARN_ONCE(vaddr == NULL,
		      "heap->ops->map_kernel should return ERR_PTR on error"))
		return (void *)ERR_PTR(-EINVAL);
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
	if (buffer->kmap_cnt == 0) {
		buffer->heap->ops->unmap_kernel(buffer->heap, buffer);
		buffer->vaddr = NULL;
	}
}

static void ion_handle_kmap_put(struct ion_handle *handle)
{
	struct ion_buffer *buffer = handle->buffer;

	if (handle->kmap_cnt == 0U) {
		WARN(true, "%s: Double unmap detected! bailing...\n", __func__);
		return;
	}
	handle->kmap_cnt--;
	if (handle->kmap_cnt == 0U)
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
//coverity[misra_c_2012_rule_8_7_violation:SUPPRESS], ## violation reason SYSSW_V_8.7_01
void *ion_map_kernel(struct ion_client *client, struct ion_handle *handle)
{
	struct ion_buffer *buffer;
	void *vaddr;
	struct mutex *hb_lock;

	if (client == NULL) {
		(void)pr_err("%s: client cannot be null\n", __func__);
		return ERR_PTR(-EINVAL);
	}

	if (handle == NULL) {
		(void)pr_err("%s: handle cannot be null\n", __func__);
		return ERR_PTR(-EINVAL);
	}

	hb_lock = &client->lock;
	mutex_lock(hb_lock);
	if (!ion_handle_validate(client, handle)) {
		(void)pr_err("%s: invalid handle passed to map_kernel.\n",
		       __func__);
		mutex_unlock(hb_lock);
		return ERR_PTR(-EINVAL);
	}

	buffer = handle->buffer;

	if (handle->buffer->heap->ops->map_kernel == NULL) {
		(void)pr_err("%s: map_kernel is not implemented by this heap.\n",
		       __func__);
		mutex_unlock(hb_lock);
		return ERR_PTR(-ENODEV);
	}

	mutex_lock(&buffer->lock);
	vaddr = ion_handle_kmap_get(handle);
	mutex_unlock(&buffer->lock);
	mutex_unlock(hb_lock);
	return vaddr;
}
//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
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
//coverity[misra_c_2012_rule_8_7_violation:SUPPRESS], ## violation reason SYSSW_V_8.7_01
void ion_unmap_kernel(struct ion_client *client, struct ion_handle *handle)
{
	struct ion_buffer *buffer;

	if (client == NULL) {
		(void)pr_err("%s: client cannot be null\n", __func__);
		return;
	}

	if (handle == NULL) {
		(void)pr_err("%s: handle cannot be null\n", __func__);
		return;
	}

	mutex_lock(&client->lock);
	if (!ion_handle_validate(client, handle)) {
		(void)pr_err("%s: invalid handle passed to unmap_kernel.\n",
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
//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
EXPORT_SYMBOL(ion_unmap_kernel);

//coverity[HIS_CCM:SUPPRESS], ## violation reason SYSSW_V_CCM_01
static uint32_t hobot_ion_get_alloc_flags(uint64_t user_flags, int64_t flags)
{
	uint32_t alloc_flags = 0;

	switch (user_flags) {
		case (uint64_t)HB_MEM_USAGE_HW_CIM:
			alloc_flags |= ((uint32_t)ION_MODULE_TYPE_INTERNAL << ION_MODULE_TYPE_BIT_SHIFT) |
				((uint32_t)ION_MEM_TYPE_CIM << ION_MEM_TYPE_BIT_SHIFT);
			break;
		case (uint64_t)HB_MEM_USAGE_HW_PYRAMID:
			alloc_flags |= ((uint32_t)ION_MODULE_TYPE_INTERNAL << ION_MODULE_TYPE_BIT_SHIFT) |
				((uint32_t)ION_MEM_TYPE_PYRAMID << ION_MEM_TYPE_BIT_SHIFT);
			break;
		case (uint64_t)HB_MEM_USAGE_HW_GDC:
			alloc_flags |= ((uint32_t)ION_MODULE_TYPE_INTERNAL << ION_MODULE_TYPE_BIT_SHIFT) |
				((uint32_t)ION_MEM_TYPE_GDC << ION_MEM_TYPE_BIT_SHIFT);
			break;
		case (uint64_t)HB_MEM_USAGE_HW_STITCH:
			alloc_flags |= ((uint32_t)ION_MODULE_TYPE_INTERNAL << ION_MODULE_TYPE_BIT_SHIFT) |
				((uint32_t)ION_MEM_TYPE_STITCH << ION_MEM_TYPE_BIT_SHIFT);
			break;
		case (uint64_t)HB_MEM_USAGE_HW_OPTICAL_FLOW:
			alloc_flags |= ((uint32_t)ION_MODULE_TYPE_INTERNAL << ION_MODULE_TYPE_BIT_SHIFT) |
				((uint32_t)ION_MEM_TYPE_OPTICAL_FLOW << ION_MEM_TYPE_BIT_SHIFT);
			break;
		case (uint64_t)HB_MEM_USAGE_HW_BPU:
			alloc_flags |= ((uint32_t)ION_MODULE_TYPE_BPU << ION_MODULE_TYPE_BIT_SHIFT) |
				((uint32_t)ION_MEM_TYPE_BPU << ION_MEM_TYPE_BIT_SHIFT);
			break;
		case (uint64_t)HB_MEM_USAGE_HW_ISP:
			alloc_flags |= ((uint32_t)ION_MODULE_TYPE_INTERNAL << ION_MODULE_TYPE_BIT_SHIFT) |
				((uint32_t)ION_MEM_TYPE_ISP << ION_MEM_TYPE_BIT_SHIFT);
			break;
		case (uint64_t)HB_MEM_USAGE_HW_DISPLAY:
			alloc_flags |= ((uint32_t)ION_MODULE_TYPE_INTERNAL << ION_MODULE_TYPE_BIT_SHIFT) |
				((uint32_t)ION_MEM_TYPE_DISPLAY << ION_MEM_TYPE_BIT_SHIFT);
			break;
		case (uint64_t)HB_MEM_USAGE_HW_VIDEO_CODEC:
			alloc_flags |= ((uint32_t)ION_MODULE_TYPE_INTERNAL << ION_MODULE_TYPE_BIT_SHIFT) |
				((uint32_t)ION_MEM_TYPE_VIDEO_CODEC << ION_MEM_TYPE_BIT_SHIFT);
			break;
		case (uint64_t)HB_MEM_USAGE_HW_JPEG_CODEC:
			alloc_flags |= ((uint32_t)ION_MODULE_TYPE_INTERNAL << ION_MODULE_TYPE_BIT_SHIFT) |
				((uint32_t)ION_MEM_TYPE_JPEG_CODEC << ION_MEM_TYPE_BIT_SHIFT);
			break;
		case (uint64_t)HB_MEM_USAGE_HW_VDSP:
			alloc_flags |= ((uint32_t)ION_MODULE_TYPE_INTERNAL << ION_MODULE_TYPE_BIT_SHIFT) |
				((uint32_t)ION_MEM_TYPE_VDSP << ION_MEM_TYPE_BIT_SHIFT);
			break;
		case (uint64_t)HB_MEM_USAGE_HW_GDC_OUT:
			alloc_flags |= ((uint32_t)ION_MODULE_TYPE_INTERNAL << ION_MODULE_TYPE_BIT_SHIFT) |
				((uint32_t)ION_MEM_TYPE_GDC_OUT << ION_MEM_TYPE_BIT_SHIFT);
			break;
		case (uint64_t)HB_MEM_USAGE_HW_IPC:
			alloc_flags |= ((uint32_t)ION_MODULE_TYPE_INTERNAL << ION_MODULE_TYPE_BIT_SHIFT) |
				((uint32_t)ION_MEM_TYPE_IPC << ION_MEM_TYPE_BIT_SHIFT);
			break;
		case (uint64_t)HB_MEM_USAGE_HW_PCIE:
			alloc_flags |= ((uint32_t)ION_MODULE_TYPE_INTERNAL << ION_MODULE_TYPE_BIT_SHIFT) |
				((uint32_t)ION_MEM_TYPE_PCIE << ION_MEM_TYPE_BIT_SHIFT);
			break;
		case (uint64_t)HB_MEM_USAGE_HW_YNR:
			alloc_flags |= ((uint32_t)ION_MODULE_TYPE_INTERNAL << ION_MODULE_TYPE_BIT_SHIFT) |
				((uint32_t)ION_MEM_TYPE_YNR << ION_MEM_TYPE_BIT_SHIFT);
			break;
		default:
			if ((uint64_t)flags >> PRIVATE_USER_MODULE_MEM_BIT_SHIFT != 0UL) {
				alloc_flags |= (uint32_t)(((uint64_t)flags >> PRIVATE_USER_MODULE_MEM_BIT_SHIFT) << ION_MEM_TYPE_BIT_SHIFT);
			} else {
				alloc_flags |= ((uint32_t)ION_MODULE_TYPE_INTERNAL << ION_MODULE_TYPE_BIT_SHIFT) |
					((uint32_t)ION_MEM_TYPE_OTHER << ION_MEM_TYPE_BIT_SHIFT);
			}
			break;
	}

	return alloc_flags;
}

//coverity[HIS_LEVEL:SUPPRESS], ## violation reason SYSSW_V_LEVEL_01
static uint32_t hobot_ion_get_heap_id_mask(uint64_t heap_flags)
{
	uint32_t heap_id_mask;

	if ((uint64_t)heap_flags == (uint64_t)HB_MEM_USAGE_PRIV_HEAP_DMA) {/* PRQA S 2995,2991 */
		heap_id_mask = (uint32_t)ION_HEAP_TYPE_DMA_MASK;
	} else if (((uint64_t)heap_flags & (uint64_t)HB_MEM_USAGE_PRIV_HEAP_RESERVED) != 0UL) {
		heap_id_mask = (uint32_t)ION_HEAP_CARVEOUT_MASK;
	} else if (((uint64_t)heap_flags & (uint64_t)HB_MEM_USAGE_PRIV_HEAP_SRAM) != 0UL) {
		heap_id_mask = (uint32_t)ION_HEAP_TYPE_CUSTOM_MASK;
	} else if (((uint64_t)heap_flags & (uint64_t)HB_MEM_USAGE_PRIV_HEAP_2_RESERVED) != 0UL) {
		heap_id_mask = (uint32_t)ION_HEAP_TYPE_CMA_RESERVED_MASK;
	} else if (((uint64_t)heap_flags & (uint64_t)HB_MEM_USAGE_PRIV_HEAP_SRAM_LIMIT) != 0UL) {
		heap_id_mask = (uint32_t)ION_HEAP_TYPE_SRAM_LIMIT_MASK;
	} else if (((uint64_t)heap_flags & (uint64_t)HB_MEM_USAGE_PRIV_HEAP_INLINE_ECC) != 0UL) {
		heap_id_mask = (uint32_t)ION_HEAP_TYPE_INLINE_ECC_MASK;
	} else {
		heap_id_mask = (uint32_t)ION_HEAP_TYPE_DMA_MASK;
	}

	return heap_id_mask;
}

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
//coverity[misra_c_2012_rule_8_7_violation:SUPPRESS], ## violation reason SYSSW_V_8.7_01
uint32_t hbmem_flag_to_ion_flag(int64_t flags, uint32_t *heap_id_mask, int32_t *port)
{
	uint32_t alloc_flags = 0;
	uint64_t user_flags = ((uint64_t)flags & (uint64_t)HB_MEM_USAGE_TRIVIAL_MASK);
	uint32_t heap_mask;
	uint64_t heap_flags = (uint64_t)flags & (uint64_t)HB_MEM_USAGE_PRIV_MASK;
	int32_t map_prot = PROT_NONE;
	uint64_t port_flags = ((uint64_t)flags & (uint64_t)HB_MEM_USAGE_CPU_READ_MASK) |
		((uint64_t)flags & (uint64_t)HB_MEM_USAGE_CPU_WRITE_MASK);

	if (((uint64_t)user_flags & (uint64_t)HB_MEM_USAGE_CACHED) != 0UL) {
		alloc_flags |= ((uint32_t)ION_FLAG_CACHED | (uint32_t)ION_FLAG_CACHED_NEEDS_SYNC);
	}

	if (((uint64_t)user_flags & (uint64_t)HB_MEM_USAGE_MAP_INITIALIZED) != 0UL) {
		alloc_flags |= (uint32_t)ION_FLAG_INITIALIZED;
	} else if (((uint64_t)user_flags & (uint64_t)HB_MEM_USAGE_MAP_UNINITIALIZED) != 0UL) {
		alloc_flags |= (uint32_t)ION_FLAG_UNINITIALIZED;
	} else {
		//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
		pr_debug("%s:%d Use uninitialized default\n", __func__, __LINE__);
	}

/* 	if (((uint64_t)user_flags & (uint64_t)HB_MEM_USAGE_MEM_POOL) != 0) {
		alloc_flags |= (uint32_t)ION_FLAG_USE_POOL;
	} */

	// alloc flag's high 16 bit meaning:
	// xxxx (vpu/jpu type id)xxxx xx(vpu/jpu instance id)xx xxxx(vpu/jpu memory
	// type)
	//alloc_flags |= (ionType << 12 | instIndex << 6 | memoryType) << 16;

	// now we can only set the module type to bpu to make the heap mask work
	// we should fix this
	//get the ion alloc flags
	user_flags = ((uint64_t)flags & (uint64_t)HB_MEM_USAGE_HW_MASK);
	alloc_flags |= hobot_ion_get_alloc_flags(user_flags, flags);

	//get the heap id mask
	heap_mask = hobot_ion_get_heap_id_mask(heap_flags);
	*heap_id_mask = heap_mask;

	//get the port for alloc buffer
	if (((port_flags & (uint64_t)HB_MEM_USAGE_CPU_READ_OFTEN) != 0UL)) {
		//coverity[misra_c_2012_rule_10_8_violation:SUPPRESS], ## violation reason SYSSW_V_10.8_01
		map_prot = (int32_t)((uint32_t)map_prot | (uint32_t)PROT_READ);
	}
	if ((port_flags & (uint64_t)HB_MEM_USAGE_CPU_WRITE_OFTEN) != 0UL) {
		//coverity[misra_c_2012_rule_10_8_violation:SUPPRESS], ## violation reason SYSSW_V_10.8_01
		map_prot = (int32_t)((uint32_t)map_prot | (uint32_t)PROT_WRITE);
	}

	*port = map_prot;

	return alloc_flags;
}
//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
EXPORT_SYMBOL(hbmem_flag_to_ion_flag);

//coverity[HIS_CCM:SUPPRESS], ## violation reason SYSSW_V_CCM_01
static uint64_t hobot_ion_get_module_flag(uint32_t type)
{
	uint64_t module_flag = 0;

	switch(type) {
		case (uint32_t)ION_MEM_TYPE_CIM:
		{
			module_flag = module_flag | (uint64_t)HB_MEM_USAGE_HW_CIM;
			break;
		}
		case (uint32_t)ION_MEM_TYPE_PYRAMID:
		{
			module_flag = module_flag | (uint64_t)HB_MEM_USAGE_HW_PYRAMID;
			break;
		}
		case (uint32_t)ION_MEM_TYPE_GDC_OUT:
		{
			module_flag = module_flag | (uint64_t)HB_MEM_USAGE_HW_GDC_OUT;
			break;
		}
		case (uint32_t)ION_MEM_TYPE_GDC:
		{
			module_flag = module_flag | (uint64_t)HB_MEM_USAGE_HW_GDC;
			break;
		}
		case (uint32_t)ION_MEM_TYPE_STITCH:
		{
			module_flag = module_flag | (uint64_t)HB_MEM_USAGE_HW_STITCH;
			break;
		}
		case (uint32_t)ION_MEM_TYPE_OPTICAL_FLOW:
		{
			module_flag = module_flag | (uint64_t)HB_MEM_USAGE_HW_OPTICAL_FLOW;
			break;
		}
		case (uint32_t)ION_MEM_TYPE_BPU:
		{
			module_flag = module_flag | (uint64_t)HB_MEM_USAGE_HW_BPU;
			break;
		}
		case (uint32_t)ION_MEM_TYPE_ISP:
		{
			module_flag = module_flag | (uint64_t)HB_MEM_USAGE_HW_ISP;
			break;
		}
		case (uint32_t)ION_MEM_TYPE_DISPLAY:
		{
			module_flag = module_flag | (uint64_t)HB_MEM_USAGE_HW_DISPLAY;
			break;
		}
		case (uint32_t)ION_MEM_TYPE_VIDEO_CODEC:
		{
			module_flag = module_flag | (uint64_t)HB_MEM_USAGE_HW_VIDEO_CODEC;
			break;
		}
		case (uint32_t)ION_MEM_TYPE_JPEG_CODEC:
		{
			module_flag = module_flag | (uint64_t)HB_MEM_USAGE_HW_JPEG_CODEC;
			break;
		}
		case (uint32_t)ION_MEM_TYPE_VDSP:
		{
			module_flag = module_flag | (uint64_t)HB_MEM_USAGE_HW_VDSP;
			break;
		}
		case (uint32_t)ION_MEM_TYPE_IPC:
		{
			module_flag = module_flag | (uint64_t)HB_MEM_USAGE_HW_IPC;
			break;
		}
		case (uint32_t)ION_MEM_TYPE_PCIE:
		{
			module_flag = module_flag | (uint64_t)HB_MEM_USAGE_HW_PCIE;
			break;
		}
		case (uint32_t)ION_MEM_TYPE_YNR:
		{
			module_flag = module_flag | (uint64_t)HB_MEM_USAGE_HW_YNR;
			break;
		}
		default:
			//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
			pr_debug("%s:%d default use other\n", __func__, __LINE__);
			break;
	}

	return module_flag;
}

static uint64_t hobot_ion_get_heap_mask(uint32_t heap_id_mask)
{
	uint64_t heap_mask = 0;

	switch(heap_id_mask) {
		case (uint32_t)ION_HEAP_CARVEOUT_MASK:
		{
			heap_mask = heap_mask | (uint64_t)HB_MEM_USAGE_PRIV_HEAP_RESERVED;
			break;
		}
		case (uint32_t)ION_HEAP_TYPE_CUSTOM_MASK:
		{
			heap_mask = heap_mask | (uint64_t)HB_MEM_USAGE_PRIV_HEAP_SRAM;
			break;
		}
		case (uint32_t)ION_HEAP_TYPE_CMA_RESERVED_MASK:
		{
			heap_mask = heap_mask | (uint64_t)HB_MEM_USAGE_PRIV_HEAP_2_RESERVED;
			break;
		}
		case (uint32_t)ION_HEAP_TYPE_DMA_MASK:
		{
			heap_mask = heap_mask | (uint64_t)HB_MEM_USAGE_PRIV_HEAP_DMA;
			break;
		}
		case (uint32_t)ION_HEAP_TYPE_SRAM_LIMIT_MASK:
		{
			heap_mask = heap_mask | (uint64_t)HB_MEM_USAGE_PRIV_HEAP_SRAM_LIMIT;
			break;
		}
		case (uint32_t)ION_HEAP_TYPE_INLINE_ECC_MASK:
		{
			heap_mask = heap_mask | (uint64_t)HB_MEM_USAGE_PRIV_HEAP_INLINE_ECC;
			break;
		}
		default:
			//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
			pr_debug("%s:%d default use cma heap\n", __func__, __LINE__);
			break;
	}

	return heap_mask;
}

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
//coverity[misra_c_2012_rule_8_7_violation:SUPPRESS], ## violation reason SYSSW_V_8.7_01
int64_t ion_flag_to_hbmem_flag(uint32_t ion_flag, uint32_t heap_id_mask, int64_t pool_flag, int64_t prot)
{
	uint64_t flags = 0, module_flag = 0, heap_mask = 0;
	uint32_t type = 0;
	//ion_flag<-->ion_flag in driver
		//get cache
	if (((ion_flag & (uint32_t)ION_FLAG_CACHED) != 0U) && ((ion_flag & (uint32_t)ION_FLAG_CACHED_NEEDS_SYNC)) != 0U) {
		flags = flags | (uint64_t)HB_MEM_USAGE_CACHED;
	}
		//get initial
	if ((ion_flag & (uint32_t)ION_FLAG_INITIALIZED) != 0U) {
		flags = flags | (uint64_t)HB_MEM_USAGE_MAP_INITIALIZED;
	} else if ((ion_flag & (uint32_t)ION_FLAG_UNINITIALIZED) != 0U) {
		flags = flags | (uint64_t)HB_MEM_USAGE_MAP_UNINITIALIZED;
	} else {
		//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
		pr_debug("%s:%d Use uninitialized default\n", __func__, __LINE__);
	}
		//get module type
	type = (ion_flag >> ION_MEM_TYPE_BIT_SHIFT) & (uint32_t)ION_MEM_TYPE_MASK;
	module_flag = hobot_ion_get_module_flag(type);
	flags = flags | module_flag;

	//heap_id_mask
	heap_mask = hobot_ion_get_heap_mask(heap_id_mask);
	flags = flags | heap_mask;

	//pool_flag is from @mem_usage_t, whether use memory pool
	//prot if from @mem_usage_t，read and write flag
	if (0UL != ((uint64_t)prot & (uint64_t)PROT_READ)) {
		flags = flags | (uint64_t)HB_MEM_USAGE_CPU_READ_OFTEN;
	}
	if (0UL != ((uint64_t)prot & (uint64_t)PROT_WRITE)) {
		flags = flags | (uint64_t)HB_MEM_USAGE_CPU_WRITE_OFTEN;
	}

	if (0UL != (uint64_t)pool_flag)
		flags = flags | ((uint64_t)HB_MEM_USAGE_MEM_POOL);

	return (int64_t)flags;
}
//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
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
//coverity[HIS_VOCF:SUPPRESS], ## violation reason SYSSW_V_VOCF_01
static int ion_debug_client_show(struct seq_file *s, void *unused)
{
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	struct ion_client *client = (struct ion_client *)s->private;
	struct rb_node *n;
	size_t total_size = 0;

	seq_printf(s, "%16.16s: %16.16s : %16.16s : %16.16s : %16.16s : %16.16s : %16.16s : %16.20s\n",
		   "heap_name", "size_in_bytes", "handle refcount", "handle import",
		   "buffer ptr", "buffer refcount", "buffer share id",
		   "buffer share count");

	mutex_lock(&client->lock);
	for (n = rb_first(&client->handles); n != NULL; n = rb_next(n)) {
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
		struct ion_handle *handle = rb_entry(n, struct ion_handle,
						     hb_node);
		struct ion_buffer *buffer = handle->buffer;
		uint32_t handle_ref, buffer_ref;

		handle_ref = kref_read(&handle->ref);
		buffer_ref = kref_read(&handle->buffer->ref);

		if (handle->buffer->priv_buffer != NULL) {
			//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
			buffer = (struct ion_buffer *)handle->buffer->priv_buffer;
			//first print the sram information
			seq_printf(s, "%16.16s: %16zx : %16d : %16d : %16pK : %16d: %16d : %16d\n",
				handle->buffer->heap->name,
				handle->buffer->size - buffer->size,
				handle_ref,
				handle->import_cnt,
				handle->buffer,
				buffer_ref,
				handle->share_id, handle->sh_hd->client_cnt);
			//then print the other heap information
		}

		seq_printf(s, "%16.16s: %16zx : %16d : %16d : %16pK : %16d: %16d : %16d\n",
			buffer->heap->name,
			buffer->size,
			handle_ref,
			handle->import_cnt,
			buffer,
			buffer_ref,
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
static int ion_debug_client_open(struct inode *hb_inode, struct file *hb_file)
{
	return single_open(hb_file, ion_debug_client_show, hb_inode->i_private);
}

//coverity[misra_c_2012_rule_8_9_violation:SUPPRESS], ## violation reason SYSSW_V_8.9_02
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
	struct rb_node *hb_node;

	for (hb_node = rb_first(root); hb_node != NULL; hb_node = rb_next(hb_node)) {
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
		struct ion_client *client = rb_entry(hb_node, struct ion_client,
						hb_node);

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
//coverity[HIS_VOCF:SUPPRESS], ## violation reason SYSSW_V_VOCF_01
//coverity[HIS_LEVEL:SUPPRESS], ## violation reason SYSSW_V_LEVEL_01
struct ion_client *ion_client_create(struct ion_device *dev,
				     const char *name)
{
	struct ion_client *client;
	struct task_struct *task;
	struct rb_node **p;
	struct rb_node *parent = NULL;
	struct ion_client *entry;
	pid_t hb_pid;
	int32_t ret;

	if (dev == NULL) {
		(void)pr_err("%s: Dev cannot be null\n", __func__);
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		return (struct ion_client *)ERR_PTR(-EINVAL);
	}

	if (name == NULL) {
		(void)pr_err("%s: Name cannot be null\n", __func__);
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		return (struct ion_client *)ERR_PTR(-EINVAL);
	}

	(void)get_task_struct(current->group_leader);
	task_lock(current->group_leader);
	hb_pid = task_pid_nr(current->group_leader);
	/* don't bother to store task struct for kernel threads,
	 * they can't be killed anyway
	 */
	if (current->group_leader->flags & (uint32_t)PF_KTHREAD) {
		put_task_struct(current->group_leader);
		task = NULL;
	} else {
		task = current->group_leader;
	}
	task_unlock(current->group_leader);

	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	client = (struct ion_client *)kzalloc(sizeof(*client), GFP_KERNEL);
	if (client == NULL) {
		//coverity[misra_c_2012_rule_15_1_violation:SUPPRESS], ## violation reason SYSSW_V_15.1_01
		goto err_put_task_struct;
	}

	client->dev = dev;
	client->handles = RB_ROOT;
	idr_init(&client->handle_idr);
	mutex_init(&client->lock);
	mutex_init(&client->group_lock);
	client->group_datas = RB_ROOT;
	client->task = task;
	client->hb_pid = hb_pid;
	client->name = kstrdup(name, GFP_KERNEL);
	if (client->name == NULL) {
		//coverity[misra_c_2012_rule_15_1_violation:SUPPRESS], ## violation reason SYSSW_V_15.1_01
		goto err_free_client;
	}

	down_write(&dev->lock);
	client->display_serial = ion_get_client_serial(&dev->clients, name);
	client->display_name = kasprintf(
		GFP_KERNEL, "%s-%d", name, client->display_serial);
	if (client->display_name == NULL) {
		up_write(&dev->lock);
		//coverity[misra_c_2012_rule_15_1_violation:SUPPRESS], ## violation reason SYSSW_V_15.1_01
		goto err_free_client_name;
	}
	p = &dev->clients.rb_node;
	while (*p != NULL) {
		parent = *p;
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
		entry = rb_entry(parent, struct ion_client, hb_node);

		//coverity[misra_c_2012_rule_11_4_violation:SUPPRESS], ## violation reason SYSSW_V_11.4_01
		if ((uint64_t)client < (uint64_t)entry)
			p = &(*p)->rb_left;
		//coverity[misra_c_2012_rule_11_4_violation:SUPPRESS], ## violation reason SYSSW_V_11.4_01
		else if ((uint64_t)client > (uint64_t)entry)
			p = &(*p)->rb_right;
		else
			WARN(true, "%s: client already found.", __func__);
	}
	rb_link_node(&client->hb_node, parent, p);
	rb_insert_color(&client->hb_node, &dev->clients);

	client->debug_root = debugfs_create_file(client->display_name, 0664,
						dev->clients_debug_root,
						client, &debug_client_fops);
	if (client->debug_root == NULL) {
		char buf[256], *hb_path;

		hb_path = dentry_path(dev->clients_debug_root, buf, 256);
		(void)pr_err("Failed to create client debugfs at %s/%s\n",
			hb_path, client->display_name);
	}
	init_waitqueue_head(&client->wq);
	client->wq_cnt = 0;
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
	ret = kfifo_alloc(&client->hb_fifo, (size_t)FIFONUM * sizeof(u32), GFP_KERNEL);
	if (ret) {
		up_write(&dev->lock);
		(void)pr_err("%s: failed to create kfifo\n", __func__);
		//coverity[misra_c_2012_rule_15_1_violation:SUPPRESS], ## violation reason SYSSW_V_15.1_01
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
	if (task != NULL)
		put_task_struct(current->group_leader);
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	return (struct ion_client *)ERR_PTR(-ENOMEM);
}
//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
EXPORT_SYMBOL(ion_client_create);

static void ion_group_data_unregister_all(struct ion_client *client)
{
	struct ion_device *dev;
	struct rb_node *n;

	dev = client->dev;
	mutex_lock(&client->group_lock);
	while ((n = rb_first(&client->group_datas)) != NULL) {
		struct ion_group_data *group_data;
		struct ion_share_group_handle *share_group_hd;

		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
		group_data = rb_entry(n, struct ion_group_data, hb_node);
		mutex_lock(&dev->share_group_lock);
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
		share_group_hd = idr_find(&dev->group_idr, (unsigned long)group_data->group_id);
		mutex_unlock(&dev->share_group_lock);
		if (IS_ERR_OR_NULL(share_group_hd)) {
			pr_err("%s: find ion share handle group failed, group id [%d].\n",
					__func__, group_data->group_id);
			continue;
		}
		while (group_data->import_cnt > 0) {
			group_data->import_cnt--;
			ion_share_handle_group_put(share_group_hd);
		}
		ion_group_data_destroy(group_data);
	}
	mutex_unlock(&client->group_lock);
}

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
//coverity[HIS_VOCF:SUPPRESS], ## violation reason SYSSW_V_VOCF_01
void ion_client_destroy(struct ion_client *client)
{
	struct ion_device *dev;
	struct rb_node *n;


	if (client == NULL) {
		(void)pr_err("%s: client cannot be null\n", __func__);
		return;
	}

	dev = client->dev;

	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
	pr_debug("%s: %d\n", __func__, __LINE__);
	down_write(&dev->lock);
	rb_erase(&client->hb_node, &dev->clients);
	up_write(&dev->lock);

	/* After this completes, there are no more references to client */
	debugfs_remove_recursive(client->debug_root);

	mutex_lock(&client->lock);
	while ((n = rb_first(&client->handles)) != NULL) {
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
		struct ion_handle *handle = rb_entry(n, struct ion_handle,
						     hb_node);
		ion_share_pool_notify(dev, handle->share_id, -1, 0, 0);
		ion_handle_destroy(&handle->ref);
	}
	mutex_unlock(&client->lock);

	ion_group_data_unregister_all(client);
	mutex_lock(&dev->share_lock);
	share_pool_unregister_all(client);
	mutex_unlock(&dev->share_lock);
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
	kfifo_free(&client->hb_fifo);

	idr_destroy(&client->handle_idr);
	if (client->task != NULL)
		put_task_struct(client->task);
	kfree(client->display_name);
	kfree(client->name);
	kfree(client);
}
//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
EXPORT_SYMBOL(ion_client_destroy);

/**
 * ion_sg_table - get an sg_table for the buffer
 *
 * NOTE: most likely you should NOT being using this API.
 * You should be using Ion as a DMA Buf exporter and using
 * the sg_table returned by dma_buf_map_attachment.
 */
//coverity[misra_c_2012_rule_8_7_violation:SUPPRESS], ## violation reason SYSSW_V_8.7_01
struct sg_table *ion_sg_table(struct ion_client *client,
			      struct ion_handle *handle)
{
	struct ion_buffer *buffer;
	struct sg_table *table;

	if (client == NULL) {
		pr_err("%s: client cannot be null\n", __func__);
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		return (struct sg_table *)ERR_PTR(-EINVAL);
	}

	if (handle == NULL) {
		pr_err("%s: handle cannot be null\n", __func__);
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		return (struct sg_table *)ERR_PTR(-EINVAL);
	}

	mutex_lock(&client->lock);
	if (!ion_handle_validate(client, handle)) {
		(void)pr_err("%s: invalid handle passed to map_dma.\n",
		       __func__);
		mutex_unlock(&client->lock);
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		return (struct sg_table *)ERR_PTR(-EINVAL);
	}
	buffer = handle->buffer;
	table = buffer->hb_sg_table;
	mutex_unlock(&client->lock);
	return table;
}
//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
EXPORT_SYMBOL(ion_sg_table);

static struct sg_table *dup_sg_table(struct sg_table *table)
{
	struct sg_table *new_table;
	int ret;
	uint32_t i;
	struct scatterlist *sg, *new_sg;

	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	new_table = (struct sg_table *)kzalloc(sizeof(*new_table), GFP_KERNEL);
	if (new_table == NULL) {
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		return (struct sg_table *)ERR_PTR(-ENOMEM);
	}

	ret = sg_alloc_table(new_table, table->orig_nents, GFP_KERNEL);
	if (0 != ret) {
		kfree(new_table);
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		return (struct sg_table *)ERR_PTR(-ENOMEM);
	}

	new_sg = new_table->sgl;
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
	for_each_sgtable_sg(table, sg, i) {
		(void)memcpy(new_sg, sg, sizeof(*sg));
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
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	struct ion_buffer *buffer = (struct ion_buffer *)dmabuf->priv;

	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	a = (struct ion_dma_buf_attachment *)kzalloc(sizeof(*a), GFP_KERNEL);
	if (a == NULL)
		return -ENOMEM;

	table = dup_sg_table(buffer->hb_sg_table);
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
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	struct ion_dma_buf_attachment *a = (struct ion_dma_buf_attachment *)attachment->priv;
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	struct ion_buffer *buffer = (struct ion_buffer *)dmabuf->priv;

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

#if defined(CONFIG_DROBOT_LITE_MMU)
	ret = ion_iommu_map_ion_sgtable(attachment->dev, table, direction, 0);
#else
	ret = dma_map_sgtable(attachment->dev, table, direction, 0);
#endif
	if (ret)
		return ERR_PTR(ret);

	return table;
}

static void ion_unmap_dma_buf(struct dma_buf_attachment *attachment,
			      struct sg_table *table,
			      enum dma_data_direction direction)
{
#if defined(CONFIG_DROBOT_LITE_MMU)
	ion_iommu_unmap_ion_sgtable(attachment->dev, table, direction);
#else
	dma_unmap_sgtable(attachment->dev, table, direction, 0);
#endif
}

int ion_mmap(struct ion_buffer *buffer, struct vm_area_struct *vma)
{
	int ret = 0;

	if (buffer->heap->ops->map_user == NULL) {
		(void)pr_err("%s: this heap does not define a method for mapping to userspace\n",
		       __func__);
		return -EINVAL;
	}

	if ((buffer->flags & (uint32_t)ION_FLAG_CACHED) == 0U)
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	mutex_lock(&buffer->lock);
	/* now map it to userspace */
	ret = buffer->heap->ops->map_user(buffer->heap, buffer, vma);
	mutex_unlock(&buffer->lock);

	if (ret)
		(void)pr_err("%s: failure mapping buffer to userspace\n",
		       __func__);

	return ret;
}
EXPORT_SYMBOL(ion_mmap);

static int ion_mmap_mem(struct file *hb_file, struct vm_area_struct *vma)
{
	int32_t ret = 0;
	size_t size = vma->vm_end - vma->vm_start;
	phys_addr_t offset = (phys_addr_t)vma->vm_pgoff << PAGE_SHIFT;

	ret = ion_check_in_heap_carveout(offset, size);
	if (ret != 0) {
		pr_err("Invalid paddr:%#llx, size = %#lx, not in ion heap range\n", offset, size);
		return -EPERM;
	}

	/* It's illegal to wrap around the end of the physical address space. */
	if (offset + (phys_addr_t)size - 1U < offset) {
		pr_err("Invalid paddr:%#llx, size = %#lx, address winding\n", offset, size);
		return -EINVAL;
	}

	//coverity[misra_c_2012_rule_10_8_violation:SUPPRESS], ## violation reason SYSSW_V_10.8_01
	if (hb_file->f_flags & (uint32_t)O_SYNC)
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	/* Remap-pfn-range will mark the range VM_IO */
	if (remap_pfn_range(vma,
		vma->vm_start, vma->vm_pgoff, size, vma->vm_page_prot))
		return -EAGAIN;

	return 0;
}

static int ion_dma_buf_mmap(struct dma_buf *dmabuf, struct vm_area_struct *vma)
{
       return ion_mmap(dmabuf->priv, vma);
}

static void ion_dma_buf_release(struct dma_buf *dmabuf)
{
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	struct ion_buffer *buffer = (struct ion_buffer *)dmabuf->priv;

	(void)ion_buffer_put(buffer);
}

static int ion_dma_buf_begin_cpu_access(struct dma_buf *dmabuf,
					enum dma_data_direction direction)
{
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	struct ion_buffer *buffer = (struct ion_buffer *)dmabuf->priv;
	void *vaddr;
	struct ion_dma_buf_attachment *a;
	int ret = 0;

	/*
	 * TODO: Move this elsewhere because we don't always need a vaddr
	 */
	if (buffer->heap->ops->map_kernel != NULL) {
		mutex_lock(&buffer->lock);
		vaddr = ion_buffer_kmap_get(buffer);
		if (IS_ERR(vaddr)) {
			ret = (int)PTR_ERR(vaddr);
			//coverity[misra_c_2012_rule_15_1_violation:SUPPRESS], ## violation reason SYSSW_V_15.1_01
			goto unlock;
		}
		mutex_unlock(&buffer->lock);
	}

	mutex_lock(&buffer->lock);
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
	list_for_each_entry(a, &buffer->attachments, list)
		dma_sync_sgtable_for_cpu(a->dev, a->table, direction);

unlock:
	mutex_unlock(&buffer->lock);
	return ret;
}

static int ion_dma_buf_end_cpu_access(struct dma_buf *dmabuf,
					  enum dma_data_direction direction)
{
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	struct ion_buffer *buffer = (struct ion_buffer *)dmabuf->priv;
	struct ion_dma_buf_attachment *a;

	if (buffer->heap->ops->map_kernel != NULL) {
		mutex_lock(&buffer->lock);
		ion_buffer_kmap_put(buffer);
		mutex_unlock(&buffer->lock);
	}

	mutex_lock(&buffer->lock);
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
	list_for_each_entry(a, &buffer->attachments, list)
		dma_sync_sgtable_for_device(a->dev, a->table, direction);
	mutex_unlock(&buffer->lock);

	return 0;
}

static const struct dma_buf_ops ion_dma_buf_ops = {
	.map_dma_buf = ion_map_dma_buf,
	.unmap_dma_buf = ion_unmap_dma_buf,
	.mmap = ion_dma_buf_mmap,
	.release = ion_dma_buf_release,
	.attach = ion_dma_buf_attach,
	.detach = ion_dma_buf_detach,
	.begin_cpu_access = ion_dma_buf_begin_cpu_access,
	.end_cpu_access = ion_dma_buf_end_cpu_access,
};

//coverity[misra_c_2012_rule_8_7_violation:SUPPRESS], ## violation reason SYSSW_V_8.7_01
struct dma_buf *ion_share_dma_buf(struct ion_client *client,
						struct ion_handle *handle)
{
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	struct ion_buffer *buffer;
	struct dma_buf *dmabuf;
	bool valid_handle;

	if (client == NULL) {
		(void)pr_err("%s: client cannot be null\n", __func__);
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		return (struct dma_buf *)ERR_PTR(-EINVAL);
	}

	if (handle == NULL) {
		(void)pr_err("%s: handle cannot be null\n", __func__);
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		return (struct dma_buf *)ERR_PTR(-EINVAL);
	}

	mutex_lock(&client->lock);
	valid_handle = ion_handle_validate(client, handle);
	if (!valid_handle) {
		WARN(true, "%s: invalid handle passed to share.\n", __func__);
		mutex_unlock(&client->lock);
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		return (struct dma_buf *)ERR_PTR(-EINVAL);
	}
	buffer = handle->buffer;
	ion_buffer_get(buffer);
	mutex_unlock(&client->lock);

	exp_info.ops = &ion_dma_buf_ops;
	exp_info.size = buffer->size;
	exp_info.flags = O_RDWR;
	exp_info.priv = buffer;

	dmabuf = dma_buf_export(&exp_info);
	if (IS_ERR(dmabuf)) {
		(void)ion_buffer_put(buffer);
		return dmabuf;
	}

	return dmabuf;
}
//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
EXPORT_SYMBOL(ion_share_dma_buf);

//coverity[misra_c_2012_rule_8_7_violation:SUPPRESS], ## violation reason SYSSW_V_8.7_01
int ion_share_dma_buf_fd(struct ion_client *client, struct ion_handle *handle)
{
	struct dma_buf *dmabuf;
	int hb_fd;

	if (client == NULL) {
		(void)pr_err("%s: client cannot be null\n", __func__);
		return -EINVAL;
	}

	if (handle == NULL) {
		(void)pr_err("%s: handle cannot be null\n", __func__);
		return -EINVAL;
	}

	dmabuf = ion_share_dma_buf(client, handle);
	if (IS_ERR(dmabuf)) {
		(void)pr_err("%s: Got buf error[%ld].", __func__, PTR_ERR(dmabuf));
		return (int)PTR_ERR(dmabuf);
	}

	hb_fd = dma_buf_fd(dmabuf, O_CLOEXEC);
	if (hb_fd < 0) {
		(void)pr_err("%s: Got buf fd error[%d].", __func__, hb_fd);
		dma_buf_put(dmabuf);
	}

	return hb_fd;
}
//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
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
//coverity[misra_c_2012_rule_8_7_violation:SUPPRESS], ## violation reason SYSSW_V_8.7_01
struct ion_handle *ion_import_dma_buf(struct ion_client *client, int hb_fd)
{
	struct dma_buf *dmabuf;
	struct ion_buffer *buffer;
	struct ion_handle *handle;
	struct ion_share_handle *share_hd;
	int32_t share_id;
	struct ion_device *dev;
	int ret;

	if (client == NULL) {
		(void)pr_err("%s: client cannot be null\n", __func__);
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		return (struct ion_handle *)ERR_PTR(-EINVAL);
	}

	dev = client->dev;
	dmabuf = dma_buf_get(hb_fd);
	if (IS_ERR(dmabuf)) {
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		return (struct ion_handle *)ERR_CAST(dmabuf);
	}
	/* if this memory came from ion */

	if (dmabuf->ops != &ion_dma_buf_ops) {
		(void)pr_err("%s: can not import dmabuf from another exporter\n",
		       __func__);
		dma_buf_put(dmabuf);
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		return (struct ion_handle *)ERR_PTR(-EINVAL);
	}
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	buffer = (struct ion_buffer *)dmabuf->priv;

	share_id = buffer->share_id;
	mutex_lock(&dev->share_lock);
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
	share_hd = idr_find(&dev->shd_idr, (unsigned long)share_id);
	if (IS_ERR_OR_NULL(share_hd)) {
		mutex_unlock(&dev->share_lock);
		(void)pr_err("%s: find ion share handle failed [%ld].\n",
				__func__, PTR_ERR(share_hd));
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		return (share_hd != NULL) ? (struct ion_handle *)ERR_CAST(share_hd): (struct ion_handle *)ERR_PTR(-EINVAL);
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
		(void)ion_share_handle_put(share_hd);
		//coverity[misra_c_2012_rule_15_1_violation:SUPPRESS], ## violation reason SYSSW_V_15.1_01
		goto end;
	}

	handle = ion_handle_create(client, buffer);
	if (IS_ERR(handle)) {
		mutex_unlock(&client->lock);
		(void)ion_share_handle_put(share_hd);
		//coverity[misra_c_2012_rule_15_1_violation:SUPPRESS], ## violation reason SYSSW_V_15.1_01
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
		(void)ion_handle_put(handle);
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		//coverity[misra_c_2012_directive_4_7_violation], ## violation reason SYSSW_V_4.7_01
		handle = ERR_PTR(ret);
		(void)ion_share_handle_put(share_hd);
	}

end:
	(void)ion_buffer_put(buffer);
	dma_buf_put(dmabuf);
	return handle;
}
//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
EXPORT_SYMBOL(ion_import_dma_buf);

static int ion_sync_for_device(struct ion_client *client, int hb_fd)
{
	struct dma_buf *dmabuf;
	struct ion_buffer *buffer;

	dmabuf = dma_buf_get(hb_fd);
	if (IS_ERR(dmabuf))
		return (int)PTR_ERR(dmabuf);

	/* if this memory came from ion */
	if (dmabuf->ops != &ion_dma_buf_ops) {
		(void)pr_err("%s: can not sync dmabuf from another exporter\n",
		       __func__);
		dma_buf_put(dmabuf);
		return -EINVAL;
	}
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	buffer = (struct ion_buffer *)dmabuf->priv;

	// here we do nothing;
	//dma_sync_sg_for_device(NULL, buffer->hb_sg_table->sgl,
	//		       buffer->hb_sg_table->nents, DMA_BIDIRECTIONAL);
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
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
	share_hd = idr_find(&dev->shd_idr, (unsigned long)share_id);
	if (IS_ERR_OR_NULL(share_hd)) {
		mutex_unlock(&dev->share_lock);
		(void)pr_err("%s: find ion share handle failed [%ld].\n",
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
		(void)ion_share_handle_put(share_hd);
		(void)ion_buffer_put(buffer);
		return -EINVAL;
	}
	handle->import_consume_cnt++;
	mutex_unlock(&client->lock);

	mutex_lock(&share_hd->consume_cnt_lock);
	share_hd->consume_cnt++;
	mutex_unlock(&share_hd->consume_cnt_lock);
	(void)ion_share_handle_put(share_hd);
	(void)ion_buffer_put(buffer);

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
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
	share_hd = idr_find(&dev->shd_idr, (unsigned long)share_id);
	if (IS_ERR_OR_NULL(share_hd)) {
		mutex_unlock(&dev->share_lock);
		(void)pr_err("%s: find ion share handle failed [%ld].\n",
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
		(void)ion_share_handle_put(share_hd);
		(void)ion_buffer_put(buffer);
		return -EINVAL;
	}
	if (handle->import_consume_cnt == 0) {
		(void)pr_warn("%s: Double unimport handle(share id %d) detected! bailing...\n",
			__func__, handle->share_id);
		mutex_unlock(&client->lock);
		(void)ion_share_handle_put(share_hd);
		(void)ion_buffer_put(buffer);
		return -EINVAL;
	}
	handle->import_consume_cnt--;
	mutex_unlock(&client->lock);

	mutex_lock(&share_hd->consume_cnt_lock);
	share_hd->consume_cnt--;
	wake_up_interruptible(&share_hd->consume_cnt_wait_q);

	mutex_unlock(&share_hd->consume_cnt_lock);
	(void)ion_share_handle_put(share_hd);
	(void)ion_buffer_put(buffer);

	return ret;
}

/* fix up the cases where the ioctl direction bits are incorrect */
//coverity[HIS_VOCF:SUPPRESS], ## violation reason SYSSW_V_VOCF_01
static unsigned int ion_ioctl_dir(unsigned int cmd)
{
	switch (cmd) {
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	case ION_IOC_SYNC:
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	case ION_IOC_FREE:
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	case ION_IOC_CUSTOM:
		return _IOC_WRITE;
	default:
		//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
		//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
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
static int32_t ion_share_pool_register_buf(struct ion_client *client,
		struct ion_share_pool_data *data)
{
	struct ion_buffer *buffer;
	struct ion_device *dev = client->dev;
	struct ion_share_handle *share_hd;
	int share_id = data->sh_handle;
	struct ion_share_pool_buf *share_pool_buf;

	mutex_lock(&dev->share_lock);
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
	share_hd = idr_find(&dev->shd_idr, (unsigned long)share_id);
	if (IS_ERR_OR_NULL(share_hd)) {
		mutex_unlock(&dev->share_lock);
		(void)pr_err("%s: find ion share handle failed [%ld].\n",
				__func__, PTR_ERR(share_hd));
		return -EINVAL;
	}
	buffer = share_hd->buffer;
	share_pool_buf =
		ion_share_pool_buf_create(dev, buffer, client, data->sh_fd, share_id);
	if (IS_ERR(share_pool_buf)) {
		mutex_unlock(&dev->share_lock);
		(void)pr_err("%s: share pool buf create error[%ld]\n",
				__func__, PTR_ERR(share_pool_buf));
		return -EINVAL;
	}

	(void)ion_share_pool_buf_add(dev, share_pool_buf);
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
static int32_t ion_share_pool_unregister_buf(struct ion_client *client,
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

	(void)ion_share_pool_buf_put(share_pool_buf);

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
static int32_t ion_share_pool_get_ref_cnt(
	struct ion_client *client, int share_id, int32_t *import_cnt)
{
	struct ion_buffer *buffer;
	struct ion_device *dev = client->dev;
	struct ion_share_handle *share_hd;
	struct ion_handle *handle;

	mutex_lock(&dev->share_lock);

	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
	share_hd = idr_find(&dev->shd_idr, (unsigned long)share_id);
	if (IS_ERR_OR_NULL(share_hd)) {
		mutex_unlock(&dev->share_lock);
		(void)pr_err("%s: find ion share handle %d failed [%ld].\n",
				__func__, share_id, PTR_ERR(share_hd));
		return -EINVAL;
	}

	buffer = share_hd->buffer;
	handle = ion_handle_lookup(client, buffer);
	if (IS_ERR(handle)) {
		mutex_unlock(&dev->share_lock);
		(void)pr_err("%s: find ion handle failed.\n",	__func__);
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
//coverity[HIS_VOCF:SUPPRESS], ## violation reason SYSSW_V_VOCF_01
static int32_t ion_share_pool_monitor_ref_cnt(struct ion_client *client,
		struct ion_share_pool_data *data)
{
	int32_t ret;
	int32_t buf[SPBUFNUM] = {0, };

	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
	ret = kfifo_out_spinlocked(
		&client->hb_fifo, buf, (size_t)SPBUFNUM * sizeof(int32_t), &client->fifo_lock);
	if (ret > 0) {
		data->sh_fd = buf[SPBUFFD];
		data->sh_handle = buf[SPBUFID];
		data->import_cnt = buf[SPBUFREFCNT];
		client->wq_cnt = 0;
		return 0;
	}

	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
	ret = (int32_t) wait_event_interruptible(client->wq, client->wq_cnt != 0);
	if (ret != 0) {
		(void)pr_info("%s failed %d\n", __func__, ret);
		return ret;
	}

	client->wq_cnt = 0;
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
	ret = kfifo_out_spinlocked(
		&client->hb_fifo, buf, (size_t)SPBUFNUM * sizeof(int32_t), &client->fifo_lock);
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
static int32_t ion_share_pool_wake_up_monitor(struct ion_client *client)
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
	struct rb_node *hb_node;
	struct rb_node *n;
	int idx = 0;
	int max_idx = data->num > MAX_PROCESS_INFO ?  MAX_PROCESS_INFO : data->num;

	down_write(&dev->lock);
	data->num = 0;
	for (hb_node = rb_first(&dev->clients); hb_node != NULL; hb_node = rb_next(hb_node)) {
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
		struct ion_client *client = rb_entry(hb_node, struct ion_client, hb_node);

		mutex_lock(&client->lock);
		for (n = rb_first(&client->handles); n != NULL; n = rb_next(n)) {
			//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
			//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
			//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
			struct ion_handle *handle = rb_entry(n, struct ion_handle, hb_node);
			if (handle->share_id == share_id) {
				data->hb_pid[idx++] = client->hb_pid;
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

//coverity[HIS_VOCF:SUPPRESS], ## violation reason SYSSW_V_VOCF_01
static int32_t ion_buf_group_register(struct ion_client *client, struct ion_register_buf_group_data *register_buf_data)
{
	int32_t ret = 0, group_id;
	struct ion_device *dev;
	struct ion_share_group_handle *share_group_hd;
	struct ion_group_data *group_data;

	dev = client->dev;
	group_id = register_buf_data->group_id;
	if (group_id > 0) {
		mutex_lock(&dev->share_group_lock);
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
		share_group_hd = idr_find(&dev->group_idr, (unsigned long)group_id);
		if (IS_ERR_OR_NULL(share_group_hd)) {
			mutex_unlock(&dev->share_group_lock);
			pr_err("%s: find ion share handle group failed, group id [%d].\n",
					__func__, group_id);
			return (share_group_hd != NULL) ? (int32_t)PTR_ERR(share_group_hd): (-EINVAL);
		}
		ion_share_handle_group_get(share_group_hd);
		mutex_unlock(&dev->share_group_lock);

		mutex_lock(&client->group_lock);
		group_data = ion_group_data_lookup(client, group_id);
		mutex_unlock(&client->group_lock);
		if (!IS_ERR_OR_NULL(group_data)) {
			ion_group_data_get(group_data);
			return 0;
		}
	} else if (group_id == 0) {
		//first register will create ion share handle group
		share_group_hd = ion_share_handle_group_create(dev);
		if (IS_ERR(share_group_hd)) {
			pr_err("%s: share handle  group create error[%ld]\n",
					__func__, PTR_ERR(share_group_hd));
			return (int32_t)PTR_ERR(share_group_hd);
		}

		mutex_lock(&dev->share_group_lock);
		ret = ion_share_handle_group_add(dev, share_group_hd);
		mutex_unlock(&dev->share_group_lock);
		if (ret != 0) {
			pr_err("%s: share handle group add failed[%d]\n", __func__, ret);
			ion_share_handle_group_put(share_group_hd);
			return ret;
		}
	} else {
		pr_err("%s: invalid group id [%d].\n",
				__func__, group_id);
		return -EINVAL;
	}

	group_data = ion_group_data_create(client, share_group_hd->id);
	if (IS_ERR(group_data)) {
		pr_err("%s: Group data create error[%ld]\n",
				__func__, PTR_ERR(group_data));
		ion_share_handle_group_put(share_group_hd);
		return (int32_t)PTR_ERR(group_data);
	}

	mutex_lock(&client->group_lock);
	ret = ion_group_data_add(client, group_data);
	if (ret != 0) {
		pr_err("%s: group data add failed[%d]\n", __func__, ret);
		mutex_unlock(&client->group_lock);
		ion_group_data_put(group_data);
		ion_share_handle_group_put(share_group_hd);
		return ret;
	}
	mutex_unlock(&client->group_lock);

	register_buf_data->group_id = share_group_hd->id;

	return ret;
}

static int32_t ion_buf_group_unregister(struct ion_client *client,
					struct ion_register_buf_group_data *register_buf_data)
{
	int32_t ret = 0, group_id;
	struct ion_device *dev;
	struct ion_share_group_handle *share_group_hd;
	struct ion_group_data *group_data;

	dev = client->dev;
	group_id = register_buf_data->group_id;
	mutex_lock(&dev->share_group_lock);
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
	share_group_hd = idr_find(&dev->group_idr, (unsigned long)group_id);
	if (IS_ERR_OR_NULL(share_group_hd)) {
		mutex_unlock(&dev->share_group_lock);
		pr_err("%s: find ion share handle group failed, group id [%d].\n",
				__func__, group_id);
		return (share_group_hd != NULL) ? (int32_t)PTR_ERR(share_group_hd): (-EINVAL);
	}
	mutex_unlock(&dev->share_group_lock);

	mutex_lock(&client->group_lock);
	group_data = ion_group_data_lookup(client, group_id);
	if (IS_ERR_OR_NULL(group_data)) {
		mutex_unlock(&client->group_lock);
		pr_err("%s: find group_data failed, group id [%d].\n",
				__func__, group_id);
		return (group_data != NULL) ? (int32_t)PTR_ERR(group_data): (-EINVAL);
	}
	ion_group_data_put_nolock(group_data);
	ion_share_handle_group_put(share_group_hd);
	mutex_unlock(&client->group_lock);

	return ret;
}

//coverity[HIS_LEVEL:SUPPRESS], ## violation reason SYSSW_V_LEVEL_01
static int32_t ion_check_import_group_phys(struct ion_client *client,
					   struct ion_import_buf_group_data *import_buf_data)
{
	int32_t ret = 0, i;
	struct ion_device *dev;
	uint32_t bitmap;

	dev = client->dev;
	bitmap = import_buf_data->bitmap;
	for (i = 0; i < ION_MAX_SUB_BUFFER_NUM * ION_MAX_BUFFER_NUM; i++) {
		if ((bitmap & ((uint32_t)(1u) << ((uint32_t)i / (uint32_t)ION_MAX_SUB_BUFFER_NUM))) != 0U) {
			struct ion_buffer *buffer;
			struct ion_share_handle *share_hd;
			size_t len = 0, in_size = import_buf_data->len[i];
			phys_addr_t phys_addr = 0, in_phys = import_buf_data->paddr[i];

			if (import_buf_data->share_id[i] <= 0) {
				continue;
			}

			mutex_lock(&dev->share_lock);
			//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
			share_hd = idr_find(&dev->shd_idr, (unsigned long)import_buf_data->share_id[i]);
			if (IS_ERR_OR_NULL(share_hd)) {
				mutex_unlock(&dev->share_lock);
				pr_err("%s: find ion share handle failed [%ld], share id:%d, index :%d.\n",
						__func__, PTR_ERR(share_hd), import_buf_data->share_id[i], i);
				return -EINVAL;
			}
			buffer = share_hd->buffer;
			mutex_unlock(&dev->share_lock);

			ion_buffer_get(buffer);
			ret = buffer->heap->ops->phys(buffer->heap, buffer, &phys_addr, &len);

			//1. whether warp
			if (phys_addr + len > phys_addr) {
				//2. contigious buffer check
				if ((in_phys < phys_addr) || ((in_phys + in_size) <= phys_addr) ||
					((in_phys + in_size) > (phys_addr + len)) ||
					(in_phys >= (phys_addr + len))) {
					pr_err("%s(%d): Invalid import buffer physical address 0x%llx, size %lu.,\
						Should be 0x%llx and %lu.\n", __func__, __LINE__, in_phys, in_size,
						phys_addr, len);
					ion_buffer_put(buffer);
					return -EINVAL;
				}
			} else if (buffer->priv_buffer != NULL) {
				//3. sg warp buffer check，normal buffer will not warp
				if (((in_phys < phys_addr) && (in_phys >= (phys_addr + len))) ||
				(((in_phys + in_size) <= phys_addr) && ((in_phys + in_size) > (phys_addr + len)))) {
					pr_err("%s(%d): Invalid import buffer physical address 0x%llx, size %lu.sg \
						buffer, Should be 0x%llx and %lu.\n", __func__, __LINE__, in_phys,
						in_size, phys_addr, len);
					ion_buffer_put(buffer);
					return -EINVAL;
				}
			} else {
				//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
				pr_debug("%s:%d check import phys success\n", __func__, __LINE__);
			}
			ion_buffer_put(buffer);
		}
	}

	return ret;
}

static int32_t ion_buf_group_release(struct ion_client *client, struct ion_import_buf_group_data *import_buf_data,
				     int32_t buffer_index)
{
	int32_t ret = 0, i;
	struct ion_device *dev;
	uint32_t bitmap;

	dev = client->dev;
	bitmap = import_buf_data->bitmap;
	for (i = 0; i < buffer_index; i++) {
		if ((bitmap & ((uint32_t)(1u) << ((uint32_t)i / (uint32_t)ION_MAX_SUB_BUFFER_NUM))) != 0U) {
			struct ion_buffer *buffer;
			struct ion_handle *handle;
			struct ion_share_handle *share_hd;

			if (import_buf_data->share_id[i] <= 0) {
				continue;
			}

			mutex_lock(&dev->share_lock);
			//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
			share_hd = idr_find(&dev->shd_idr, (unsigned long)import_buf_data->share_id[i]);
			if (IS_ERR_OR_NULL(share_hd)) {
				mutex_unlock(&dev->share_lock);
				pr_err("%s: find ion share handle failed [%ld].\n",
						__func__, PTR_ERR(share_hd));
				continue;
			}
			buffer = share_hd->buffer;
			ion_share_handle_get(share_hd);
			ion_buffer_get(buffer);
			mutex_unlock(&dev->share_lock);

			mutex_lock(&client->lock);
			/* if a handle exists for this buffer just take a reference to it */
			handle = ion_handle_lookup(client, buffer);
			if (IS_ERR(handle)) {
				mutex_unlock(&client->lock);
				ion_share_handle_put(share_hd);
				ion_buffer_put(buffer);
				pr_err("%s: find ion handle failed [%ld].\n",
						__func__, PTR_ERR(handle));
				continue;
			}
			mutex_unlock(&client->lock);
			ion_free(client, handle);
			ion_share_handle_put(share_hd);
			ion_buffer_put(buffer);
		}
	}

	return ret;
}

static int32_t ion_buf_group_import(struct ion_client *client, struct ion_import_buf_group_data *import_buf_data)
{
	int32_t ret = 0, i;
	uint32_t bitmap = import_buf_data->bitmap;

	ret = ion_check_import_group_phys(client, import_buf_data);
	if (ret != 0) {
		pr_err("%s(%d): check import group phys failed\n", __func__, __LINE__);

		return -EINVAL;
	}

	for (i = 0; i < ION_MAX_BUFFER_NUM * ION_MAX_SUB_BUFFER_NUM; i++) {
		if ((bitmap & ((uint32_t)(1u) << ((uint32_t)i / (uint32_t)ION_MAX_SUB_BUFFER_NUM))) != 0U) {
			struct ion_handle *handle;

			if (import_buf_data->share_id[i] <= 0) {
				continue;
			}

			handle = ion_import_dma_buf_with_shareid(client, import_buf_data->share_id[i]);
			if (IS_ERR(handle)) {
				//release handle
				ion_buf_group_release(client, import_buf_data, i);
				ret = (int32_t)PTR_ERR(handle);
				return ret;
			}
		}
	}

	return ret;
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
//coverity[HIS_VOCF:SUPPRESS], ## violation reason SYSSW_V_VOCF_01
//coverity[HIS_CALLS:SUPPRESS], ## violation reason SYSSW_V_CALLS_01
//coverity[HIS_LEVEL:SUPPRESS], ## violation reason SYSSW_V_LEVEL_01
//coverity[HIS_STMT:SUPPRESS], ## violation reason SYSSW_V_STMT_01
//coverity[HIS_CCM:SUPPRESS], ## violation reason SYSSW_V_CCM_01
static long ion_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	struct ion_client *client = (struct ion_client *)filp->private_data;
	struct ion_device *dev = client->dev;
	struct ion_handle *cleanup_handle = NULL;
	int ret = 0;
	unsigned int dir;

	union {
		struct ion_fd_data ion_fd;
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
		struct ion_register_buf_group_data register_group_data;
		struct ion_import_buf_group_data import_group_data;
	} data;
	(void)memset(&data, 0x00, sizeof(data));

	dir = ion_ioctl_dir(cmd);

	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	//coverity[misra_c_2012_rule_10_8_violation:SUPPRESS], ## violation reason SYSSW_V_10.8_01
	if ((uint64_t)_IOC_SIZE(cmd) > sizeof(data))
		return -EINVAL;

	if (dir & _IOC_WRITE) {
		//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
		//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_11.6_01
		//coverity[misra_c_2012_rule_10_8_violation:SUPPRESS], ## violation reason SYSSW_V_10.8_01
		if (copy_from_user(&data, (void __user *)arg, (uint64_t)_IOC_SIZE(cmd)))
			return -EFAULT;
	}

	switch (cmd) {
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
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
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	case ION_IOC_FREE:
	{
		struct ion_handle *handle;

		mutex_lock(&client->lock);
		handle = ion_handle_get_by_id_nolock(client,
						     (int)data.handle.handle);
		if (IS_ERR(handle)) {
			mutex_unlock(&client->lock);
			return PTR_ERR(handle);
		}
		(void)ion_share_pool_notify(dev, handle->share_id, -1, SPWQTIMEOUT, 0);
		ion_free_nolock(client, handle);
		(void)ion_handle_put_nolock(handle);
		mutex_unlock(&client->lock);
		break;
	}
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	case ION_IOC_SHARE:
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	case ION_IOC_MAP:
	{
		struct ion_handle *handle;

		handle = ion_handle_get_by_id(client, (int)data.handle.handle);
		if (IS_ERR(handle)) {
			(void)pr_err("%s: find ion handle failed [%ld].",
					__func__, PTR_ERR(handle));
			return PTR_ERR(handle);
		}
		data.ion_fd.hb_fd = ion_share_dma_buf_fd(client, handle);
		(void)ion_handle_put(handle);
		if (data.ion_fd.hb_fd < 0)
			ret = data.ion_fd.hb_fd;
		break;
	}
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	case ION_IOC_IMPORT:
	{
		struct ion_handle *handle;

		handle = ion_import_dma_buf(client, data.ion_fd.hb_fd);
		if (IS_ERR(handle))
			ret = (int)PTR_ERR(handle);
		else
			data.handle.handle = handle->id;
		break;
	}
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	case ION_IOC_SYNC:
	{
		ret = ion_sync_for_device(client, data.ion_fd.hb_fd);
		break;
	}
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	case ION_IOC_CUSTOM:
	{
		if (dev->custom_ioctl == NULL)
			return -ENOTTY;
		ret = (int32_t)dev->custom_ioctl(client, data.custom.cmd,
						data.custom.arg);
		break;
	}
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	case ION_IOC_IMPORT_SHARE_ID:
	{
		struct ion_handle *handle;

		ret = ion_share_pool_notify(dev, data.share_hd.sh_handle, 1, SPWQTIMEOUT, 1);
		if (ret != 0) {
			(void)pr_err("%s:%d failed ret[%d].", __func__, __LINE__, ret);
			break;
		}

		handle = ion_share_handle_import_dma_buf(client, &data.share_hd);
		if (IS_ERR(handle)) {
			ret = (int)PTR_ERR(handle);
		} else {
			data.share_hd.handle = handle->id;
		}
		break;
	}
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	case ION_IOC_GET_SHARE_INFO:
	{
		struct ion_handle *handle;

		mutex_lock(&client->lock);
		handle = ion_handle_get_by_id_nolock(client, (int)data.share_info.handle);
		if (IS_ERR(handle)) {
			mutex_unlock(&client->lock);
			(void)pr_err("%s:%d find ion handle failed [%ld].",
					__func__, __LINE__, PTR_ERR(handle));
			return PTR_ERR(handle);
		}

		if ((handle->share_id != 0) && (handle->sh_hd != NULL)) {
			data.share_info.cur_client_cnt = ion_share_handle_get_share_info(handle->sh_hd);
		} else {
			(void)pr_err("%s:%d invalid handle with share id %d and ptr %pK.",
					__func__, __LINE__, handle->share_id, handle->sh_hd);
			ret = -EINVAL;
		}
		(void)ion_handle_put_nolock(handle);
		mutex_unlock(&client->lock);
		break;
	}
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	case ION_IOC_WAIT_SHARE_ID:
	{
		struct ion_handle *handle;
		struct ion_share_handle * share_hd;

		mutex_lock(&client->lock);
		handle = ion_handle_get_by_id_nolock(client, (int)data.share_info.handle);
		if (IS_ERR(handle)) {
			mutex_unlock(&client->lock);
			(void)pr_warn("%s:%d find ion handle failed [%ld].",
					__func__, __LINE__, PTR_ERR(handle));
			return PTR_ERR(handle);
		}
		if ((handle->share_id == 0) || (handle->sh_hd == NULL)) {
			mutex_unlock(&client->lock);
			(void)pr_err("%s:%d invalid handle with share id %d and ptr %pK.",
					__func__, __LINE__, handle->share_id, handle->sh_hd);
			return -EINVAL;
		}
		ion_share_handle_get(handle->sh_hd);
		share_hd = handle->sh_hd;
		mutex_unlock(&client->lock);

		if (data.share_info.timeout > 0) {
			//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
			ret = (int32_t) wait_event_interruptible_timeout(share_hd->client_cnt_wait_q,
				share_hd->client_cnt <= data.share_info.target_client_cnt,
				msecs_to_jiffies(data.share_info.timeout));
			if (ret > 0) {
				ret = 0;
			} else if (ret == 0) {
				ret = -ETIME;
			} else {
				//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
				pr_debug("%s:%d failed\n", __func__, __LINE__);
			}
		} else if (data.share_info.timeout < 0) {
			//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
			ret = (int32_t) wait_event_interruptible(share_hd->client_cnt_wait_q,
				share_hd->client_cnt <= data.share_info.target_client_cnt);
		} else {
			//do nothing
			;
		}
		data.share_info.cur_client_cnt = share_hd->client_cnt;

		(void)ion_share_handle_put(share_hd);
		(void)ion_handle_put(handle);
		break;
	}
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	case ION_IOC_SHARE_POOL_REGISTER:
	{
		ret = ion_share_pool_register_buf(client, &data.share_hd_fd);
		break;
	}
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	case ION_IOC_SHARE_POOL_UNREGISTER:
	{
		ret = ion_share_pool_unregister_buf(client, &data.share_hd_fd);
		break;
	}
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	case ION_IOC_SHARE_POOL_GET_REF_CNT:
	{
		ret = ion_share_pool_get_ref_cnt(
			client, data.share_hd_fd.sh_handle, &data.share_hd_fd.import_cnt);
		break;
	}
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	case ION_IOC_SHARE_POOL_MONITOR_REF_CNT:
	{
		//wait for import count of share fd changing
		ret = ion_share_pool_monitor_ref_cnt(client, &data.share_hd_fd);
		break;
	}
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
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
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	case ION_IOC_GET_BUFFER_PROCESS_INFO:
	{
		ret = ion_get_buffer_process_info(dev, &data.process_info);
		break;
	}
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	case ION_IOC_SHARE_AND_PHY:
	{
		struct ion_handle *handle;

		handle = ion_handle_get_by_id(client, (int)data.handle.handle);
		if (IS_ERR(handle)) {
			(void)pr_err("%s: find ion handle failed [%ld].",
					__func__, PTR_ERR(handle));
			return PTR_ERR(handle);
		}
		data.share_phy_data.hb_fd = ion_share_dma_buf_fd(client, handle);
		(void)ion_handle_put(handle);
		if (data.share_phy_data.hb_fd < 0) {
			ret = data.share_phy_data.hb_fd;
			break;
		}

		ret = ion_phys(client, (int)data.share_phy_data.handle,
					&data.share_phy_data.paddr, &data.share_phy_data.len);
		break;
	}
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	case ION_IOC_INC_CONSUME_CNT:
	{
		ret = ion_share_handle_import_consume_cnt(client, &data.share_hd);
		if (ret != 0) {
			(void)pr_err("%s:%d import consume cnt failed [%d].",
					__func__, __LINE__, ret);
		}
		break;
	}
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	case ION_IOC_DEC_CONSUME_CNT:
	{
		ret = ion_share_handle_free_consume_cnt(client, &data.share_hd);
		if (ret != 0) {
			(void)pr_err("%s:%d free consume cnt failed [%d].",
					__func__, __LINE__, ret);
		}
		break;
	}
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	case ION_IOC_GET_CONSUME_INFO:
	{
		struct ion_handle *handle;

		mutex_lock(&client->lock);
		handle = ion_handle_get_by_id_nolock(client, (int)data.consume_info.handle);
		if (IS_ERR(handle)) {
			mutex_unlock(&client->lock);
			(void)pr_err("%s:%d find ion handle failed [%ld].",
					__func__, __LINE__, PTR_ERR(handle));
			return PTR_ERR(handle);
		}

		if ((handle->share_id != 0) && (handle->sh_hd != NULL)) {
			data.consume_info.cur_consume_cnt = ion_share_handle_get_consume_info(handle->sh_hd);
		} else {
			(void)pr_err("%s:%d invalid handle with share id %d and ptr %pK.",
					__func__, __LINE__, handle->share_id, handle->sh_hd);
			ret = -EINVAL;
		}
		(void)ion_handle_put_nolock(handle);
		mutex_unlock(&client->lock);
		break;
	}
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	case ION_IOC_WAIT_CONSUME_STATUS:
	{
		struct ion_handle *handle;
		struct ion_share_handle * share_hd;

		mutex_lock(&client->lock);
		handle = ion_handle_get_by_id_nolock(client, (int)data.consume_info.handle);
		if (IS_ERR(handle)) {
			mutex_unlock(&client->lock);
			(void)pr_warn("%s:%d find ion handle failed [%ld].",
					__func__, __LINE__, PTR_ERR(handle));
			return PTR_ERR(handle);
		}
		if ((handle->share_id == 0) || (handle->sh_hd == NULL)) {
			mutex_unlock(&client->lock);
			(void)pr_err("%s:%d invalid handle with share id %d and ptr %pK.",
					__func__, __LINE__, handle->share_id, handle->sh_hd);
			return -EINVAL;
		}
		ion_share_handle_get(handle->sh_hd);
		share_hd = handle->sh_hd;
		mutex_unlock(&client->lock);

		if (data.consume_info.timeout > 0) {
			//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
			ret = (int32_t) wait_event_interruptible_timeout(share_hd->consume_cnt_wait_q,
				share_hd->consume_cnt <= data.consume_info.target_consume_cnt,
				msecs_to_jiffies(data.consume_info.timeout));
			if (ret > 0) {
				ret = 0;
			} else if (ret == 0) {
				ret = -ETIME;
			} else {
				//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
				pr_debug("%s:%d failed\n", __func__, __LINE__);
			}
		} else if (data.consume_info.timeout < 0) {
			//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
			ret = (int32_t) wait_event_interruptible(share_hd->consume_cnt_wait_q,
				share_hd->consume_cnt <= data.consume_info.target_consume_cnt);
		} else {
			//do nothing
			;
		}
		data.consume_info.cur_consume_cnt = share_hd->consume_cnt;

		(void)ion_share_handle_put(share_hd);
		(void)ion_handle_put(handle);
		break;
	}
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	case ION_IOC_GET_VERSION_INFO:
	{
		data.version_info.major = ION_DRIVER_API_VERSION_MAJOR;
		data.version_info.minor = ION_DRIVER_API_VERSION_MINOR;
		break;
	}
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	case ION_IOC_REGISTER_GRAPH_BUF_GROUP:
	{
		ret = ion_buf_group_register(client, &data.register_group_data);
		break;
	}
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	case ION_IOC_UNREGISTER_GRAPH_BUF_GROUP:
	{
		ret = ion_buf_group_unregister(client, &data.register_group_data);
		break;
	}
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	case ION_IOC_IMPORT_GRAPH_BUF_GROUP:
	{
		ret = ion_buf_group_import(client, &data.import_group_data);
		break;
	}
	//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	case ION_IOC_FREE_GRAPH_BUF_GROUP:
	{
		ret = ion_buf_group_release(client, &data.import_group_data,
					    ION_MAX_BUFFER_NUM * ION_MAX_SUB_BUFFER_NUM);
		break;
	}
	default:
		return -ENOTTY;

	}

	if (dir & _IOC_READ) {
		//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
		//coverity[misra_c_2012_rule_10_1_violation:SUPPRESS], ## violation reason SYSSW_V_10.1_01
		//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_11.6_01
		//coverity[misra_c_2012_rule_10_8_violation:SUPPRESS], ## violation reason SYSSW_V_10.8_01
		if (copy_to_user((void __user *)arg, &data, (uint64_t)_IOC_SIZE(cmd))) {
			if (cleanup_handle != NULL)
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
static int ion_release(struct inode *hb_inode, struct file *hb_file)
{
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	struct ion_client *client = (struct ion_client *)hb_file->private_data;

	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
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
static int ion_open(struct inode *hb_inode, struct file *hb_file)
{
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	struct miscdevice *miscdev = (struct miscdevice *)hb_file->private_data;
	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
	struct ion_device *dev = container_of(miscdev, struct ion_device, dev);
	struct ion_client *client;
	char debug_name[64];

	//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
	pr_debug("%s: %d\n", __func__, __LINE__);
	(void)snprintf(debug_name, 64, "%u", task_pid_nr(current->group_leader));
	client = ion_client_create(dev, debug_name);
	if (IS_ERR(client))
		return (int)PTR_ERR(client);
	hb_file->private_data = client;
	(void)hb_expand_files(current->files, (uint32_t)(rlimit(RLIMIT_NOFILE) - 1U));

	return 0;
}

//coverity[misra_c_2012_rule_8_9_violation:SUPPRESS], ## violation reason SYSSW_V_8.9_02
static const struct file_operations ion_fops = {
	.owner          = THIS_MODULE,
	.open			= ion_open,
	.release		= ion_release,
	.mmap			= ion_mmap_mem,
	.unlocked_ioctl = ion_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= ion_ioctl,
#endif
};

#ifdef CONFIG_PCIE_HOBOT_EP_FUN_AI
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
		if (IS_ERR(handle)) {
			ret = PTR_ERR(handle);
		}
		else {
			ion_rev_data->data.handle.handle = handle->id;
		}
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
			pr_err("%s:%d invalid handle with share id %d and ptr %pK.",
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
			pr_err("%s:%d invalid handle with share id %d and ptr %pK.",
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
			} else {
				pr_debug("%s:%d failed\n", __func__, __LINE__);
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
	case ION_IOC_GET_BUFFER_PROCESS_INFO:
	{
		struct ion_client *client = (struct ion_client*)ion_rev_data->client;
		struct ion_device *dev = client->dev;

		ret = ion_get_buffer_process_info(dev, &ion_rev_data->data.process_info);
		break;
	}
	case ION_IOC_SHARE_AND_PHY:
	{
		struct ion_handle *handle;
		struct ion_client *client = (struct ion_client*)ion_rev_data->client;

		handle = ion_handle_get_by_id(client, ion_rev_data->data.handle.handle);
		if (IS_ERR(handle)) {
			pr_err("%s: find ion handle failed [%ld].",
					__func__, PTR_ERR(handle));
			return PTR_ERR(handle);
		}
		ion_rev_data->data.share_phy_data.hb_fd = ion_share_dma_buf_fd(client, handle);
		ion_handle_put(handle);
		if (ion_rev_data->data.share_phy_data.hb_fd < 0)
			ret = ion_rev_data->data.share_phy_data.hb_fd;

		ret = ion_phys(client, (long)ion_rev_data->data.share_phy_data.handle,
					&ion_rev_data->data.share_phy_data.paddr, &ion_rev_data->data.share_phy_data.len);
		break;
	}
	case ION_IOC_INC_CONSUME_CNT:
	{
		struct ion_client *client = (struct ion_client*)ion_rev_data->client;

		ret = ion_share_handle_import_consume_cnt(client, &ion_rev_data->data.share_hd);
		if (ret != 0) {
			pr_err("%s:%d import consume cnt failed [%d].",
					__func__, __LINE__, ret);
		}
		break;
	}
	case ION_IOC_DEC_CONSUME_CNT:
	{
		struct ion_client *client = (struct ion_client*)ion_rev_data->client;

		ret = ion_share_handle_free_consume_cnt(client, &ion_rev_data->data.share_hd);
		if (ret != 0) {
			pr_err("%s:%d free consume cnt failed [%d].",
					__func__, __LINE__, ret);
		}
		break;
	}
	case ION_IOC_GET_CONSUME_INFO:
	{
		struct ion_handle *handle;
		struct ion_client *client = (struct ion_client*)ion_rev_data->client;

		mutex_lock(&client->lock);
		handle = ion_handle_get_by_id_nolock(client, ion_rev_data->data.consume_info.handle);
		if (IS_ERR(handle)) {
			mutex_unlock(&client->lock);
			pr_err("%s:%d find ion handle failed [%ld].",
					__func__, __LINE__, PTR_ERR(handle));
			return PTR_ERR(handle);
		}

		if ((handle->share_id != 0) && (handle->sh_hd != NULL)) {
			ion_rev_data->data.consume_info.cur_consume_cnt = ion_share_handle_get_consume_info(handle->sh_hd);
		} else {
			pr_err("%s:%d invalid handle with share id %d and ptr %pK.",
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
		struct ion_client *client = (struct ion_client*)ion_rev_data->client;

		mutex_lock(&client->lock);
		handle = ion_handle_get_by_id_nolock(client, ion_rev_data->data.consume_info.handle);
		if (IS_ERR(handle)) {
			mutex_unlock(&client->lock);
			pr_warn("%s:%d find ion handle failed [%ld].",
					__func__, __LINE__, PTR_ERR(handle));
			return PTR_ERR(handle);
		}
		if ((handle->share_id == 0) || (handle->sh_hd == NULL)) {
			mutex_unlock(&client->lock);
			pr_err("%s:%d invalid handle with share id %d and ptr %pK.",
					__func__, __LINE__, handle->share_id, handle->sh_hd);
			return -EINVAL;
		}
		ion_share_handle_get(handle->sh_hd);
		share_hd = handle->sh_hd;
		mutex_unlock(&client->lock);

		if (ion_rev_data->data.consume_info.timeout > 0) {
			ret = (int32_t) wait_event_interruptible_timeout(share_hd->consume_cnt_wait_q,
				share_hd->consume_cnt <= ion_rev_data->data.consume_info.target_consume_cnt,
				msecs_to_jiffies(ion_rev_data->data.consume_info.timeout));
			if (ret > 0) {
				ret = 0;
			} else if (ret == 0) {
				ret = -ETIME;
			} else {
				pr_debug("%s:%d failed\n", __func__, __LINE__);
			}
		} else if (ion_rev_data->data.consume_info.timeout < 0) {
			ret = (int32_t) wait_event_interruptible(share_hd->consume_cnt_wait_q,
				share_hd->consume_cnt <= ion_rev_data->data.consume_info.target_consume_cnt);
		}
		ion_rev_data->data.consume_info.cur_consume_cnt = share_hd->consume_cnt;

		ion_share_handle_put(share_hd);
		ion_handle_put(handle);
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
	for (n = rb_first(&client->handles); n != NULL; n = rb_next(n)) {
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
		struct ion_handle *handle = rb_entry(n,
						     struct ion_handle,
						     hb_node);
		struct ion_buffer *buffer = handle->buffer;
		if (buffer->priv_buffer != NULL) {
			//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
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

//coverity[HIS_CCM:SUPPRESS], ## violation reason SYSSW_V_CCM_01
//coverity[HIS_VOCF:SUPPRESS], ## violation reason SYSSW_V_VOCF_01
static int ion_heap_show(struct ion_heap *heap, struct seq_file *s,
				      void *unused)
{
	struct ion_device *dev = heap->dev;
	struct rb_node *n;
	size_t total_size = 0;
	size_t heap_total_size = 0;
	size_t total_orphaned_size = 0;
	uint32_t moduletype = 0;
	uint32_t datatype = 0;
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
	for (n = rb_first(&dev->clients); n != NULL; n = rb_next(n)) {
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
		struct ion_client *client = rb_entry(n, struct ion_client,
						     hb_node);
		size_t size = ion_debug_heap_total(client, heap->id);

		if (size == 0UL)
			continue;
		if (client->task != NULL) {
			char task_comm[TASK_COMM_LEN];

			//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
			//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
			(void)get_task_comm(task_comm, client->task);
			seq_printf(s, "%16s %16s %16u %16zu\n", heap->name, task_comm,
				   client->hb_pid, size);
		} else {
			seq_printf(s, "%16s %16s %16u %16zu\n", heap->name, client->name,
				   client->hb_pid, size);
		}
	}
	up_read(&dev->lock);

	seq_puts(s, "-------------------------------------------------------------------------\n");
	seq_puts(s, "allocations (info is from last known client):\n");
	mutex_lock(&dev->buffer_lock);
	for (n = rb_first(&dev->buffers); n != NULL; n = rb_next(n)) {
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
		struct ion_buffer *buffer = rb_entry(n, struct ion_buffer,
							hb_node);
		//the second scatterlist buffer, NULL is no matter
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		struct ion_buffer *priv_buffer = (struct ion_buffer *)buffer->priv_buffer;
		size_t size = buffer->size;
		if (priv_buffer != NULL) {
			size -= priv_buffer->size;	//the fisrt buffer should sub the second buffer size
		}

		if (buffer->heap->id != heap->id)
			continue;
		total_size += size;

		moduletype = (uint32_t)(buffer->private_flags >> 12);
		datatype = (uint32_t)(buffer->private_flags & 0xfffU);
		if (moduletype == 0U) {
			uint32_t datatype_num = (uint32_t)(sizeof(_vio_data_type)/sizeof(_vio_data_type[1]));
			if (datatype >= datatype_num ) datatype = datatype_num - 1U;
			ptrtype = _vio_data_type[datatype];
		} else if (moduletype == ION_MODULE_TYPE_BPU) {
			ptrtype = "bpu";
		} else if (moduletype == ION_MODULE_TYPE_VPU) {
			uint32_t vpuid = datatype >> 6U;
			uint32_t vputype = datatype & 0x3fU;
			(void)snprintf(iontype, sizeof(iontype), "vpuchn%d_%d", vpuid, vputype);
			ptrtype = iontype;
		} else if (moduletype == ION_MODULE_TYPE_JPU) {
			uint32_t jpuid = datatype >> 6U;
			uint32_t jputype = datatype & 0x3fU;
			(void)snprintf(iontype, sizeof(iontype), "jpuchn%d_%d", jpuid, jputype);
			ptrtype = iontype;
		} else if (moduletype == ION_MODULE_TYPE_GPU) {
			ptrtype = "gpu";
		} else {
			ptrtype = "other";
		}

		if (buffer->handle_count == 0) {
			seq_printf(s, "%16s %16u %16s %16zu %d orphaned\n",
				buffer->task_comm, buffer->hb_pid,
				ptrtype,
				size, buffer->kmap_cnt);
				/* atomic_read(&buffer->ref.refcount)); */
			total_orphaned_size += size;
		} else {
			seq_printf(s, "%16s %16u %16s %16zu %d\n",
				buffer->task_comm, buffer->hb_pid,
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

	if (heap->debug_show != NULL)
		(void)heap->debug_show(heap, s, unused);

	return 0;
}

static int ion_debug_heap_show(struct seq_file *s, void *unused)
{
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	struct ion_heap *heap = (struct ion_heap *)s->private;

	return ion_heap_show(heap, s, unused);
}

static int ion_debug_heap_open(struct inode *hb_inode, struct file *hb_file)
{
	return single_open(hb_file, ion_debug_heap_show, hb_inode->i_private);
}

static ssize_t ion_debug_config_write(struct file *filp, const char __user *buf, size_t len, loff_t *off)
{
	char value[32];

	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_11.6_01
	if(copy_from_user(value, buf, len)) {
		(void)printk("read data from user failed\n");
		return -EFAULT;
	}

	(void)printk("%16s\n", value);

	return (ssize_t)len;
}

//coverity[misra_c_2012_rule_8_9_violation:SUPPRESS], ## violation reason SYSSW_V_8.9_02
static const struct file_operations debug_heap_fops = {
	.open = ion_debug_heap_open,
	.write = ion_debug_config_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int ion_debug_all_heap_show(struct seq_file *s, void *unused)
{
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	struct ion_device *dev = (struct ion_device *)s->private;
	struct ion_heap *heap;
	int ret = 0;

	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
	plist_for_each_entry(heap, &dev->heaps, hb_node) {
		ret = ion_heap_show(heap, s, unused);
	}

	return 0;
}

static int ion_debug_all_heap_open(struct inode *hb_inode, struct file *hb_file)
{
	return single_open(hb_file, ion_debug_all_heap_show, hb_inode->i_private);
}

//coverity[misra_c_2012_rule_8_9_violation:SUPPRESS], ## violation reason SYSSW_V_8.9_02
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
		objs = heap->hb_shrinker.count_objects(&heap->hb_shrinker, &sc);
		sc.nr_to_scan = objs;
	}

	heap->hb_shrinker.scan_objects(&heap->hb_shrinker, &sc);
	return 0;
}

static int debug_shrink_get(void *data, u64 *val)
{
	struct ion_heap *heap = data;
	struct shrink_control sc;
	int objs;

	sc.gfp_mask = GFP_HIGHUSER;
	sc.nr_to_scan = 0;

	objs = heap->hb_shrinker.count_objects(&heap->hb_shrinker, &sc);
	*val = objs;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(debug_shrink_fops, debug_shrink_get,
			debug_shrink_set, "%llu\n");
#endif

//coverity[HIS_VOCF:SUPPRESS], ## violation reason SYSSW_V_VOCF_01
void ion_device_add_heap(struct ion_device *dev, struct ion_heap *heap)
{
	struct dentry *debug_file;

	if ((heap->ops->allocate == NULL) || (heap->ops->free == NULL) || (heap->ops->map_dma == NULL) ||
	    (heap->ops->unmap_dma == NULL))
		(void)pr_err("%s: can not add heap with invalid ops struct.\n",
		       __func__);

	spin_lock_init(&heap->free_lock);
	heap->free_list_size = 0;

	if (heap->flags & ION_HEAP_FLAG_DEFER_FREE)
		(void)ion_heap_init_deferred_free(heap);

	if (((heap->flags & ION_HEAP_FLAG_DEFER_FREE) != 0UL) || (heap->ops->shrink != NULL))
		ion_heap_init_shrinker(heap);

	heap->dev = dev;
	down_write(&dev->lock);
	heap->id = (uint32_t)heap->type;
	/*
	 * use negative heap->id to reverse the priority -- when traversing
	 * the list later attempt higher id numbers first
	 */
	plist_node_init(&heap->hb_node, -(int32_t)heap->id);
	plist_add(&heap->hb_node, &dev->heaps);
	debug_file = debugfs_create_file(heap->name, 0664,
					dev->heaps_debug_root, heap,
					&debug_heap_fops);

	if (debug_file == NULL) {
		char buf[256], *hb_path;

		hb_path = dentry_path(dev->heaps_debug_root, buf, 256);
		(void)pr_err("Failed to create heap debugfs at %s/%s\n",
			hb_path, heap->name);
	}

#ifdef DEBUG_HEAP_SHRINKER
	if (heap->shrinker.count_objects && heap->shrinker.scan_objects) {
		char debug_name[64];

		snprintf(debug_name, 64, "%s_shrink", heap->name);
		debug_file = debugfs_create_file(
			debug_name, 0644, dev->debug_root, heap,
			&debug_shrink_fops);
		if (!debug_file) {
			char buf[256], *hb_path;

			hb_path = dentry_path(dev->debug_root, buf, 256);
			pr_err("Failed to create heap shrinker debugfs at %s/%s\n",
				hb_path, debug_name);
		}
	}
#endif
	up_write(&dev->lock);
}
//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
EXPORT_SYMBOL(ion_device_add_heap);

void ion_device_del_heap(struct ion_device *dev, struct ion_heap *heap)
{
	struct dentry *debug_file;

	debug_file = debugfs_lookup(heap->name, dev->heaps_debug_root);
	if (debug_file != NULL) {
		debugfs_remove(debug_file);
	}

	plist_del(&heap->hb_node, &dev->heaps);
}
//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
EXPORT_SYMBOL(ion_device_del_heap);

static int _ion_buf_show(struct ion_device *dev, struct seq_file *s, int32_t share_id)
{
	struct rb_node *hb_node;
	struct rb_node *n;

	down_write(&dev->lock);
	for (hb_node = rb_first(&dev->clients); hb_node != NULL; hb_node = rb_next(hb_node)) {
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
		struct ion_client *client = rb_entry(hb_node, struct ion_client, hb_node);
		mutex_lock(&client->lock);
		for (n = rb_first(&client->handles); n != NULL; n = rb_next(n)) {
			//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
			//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
			//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
			struct ion_handle *handle = rb_entry(n, struct ion_handle, hb_node);
			if (handle->share_id == share_id) {
				seq_printf(s, "%20.20s, %20.20s, %5d, %8d\n",
					client->name, client->display_name, client->hb_pid, share_id);
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
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	struct ion_device *dev = (struct ion_device *)s->private;
	struct rb_node *hb_node;
	struct ion_share_handle * share_hd;
	int32_t *share_id;
	int32_t total_num = 0, i = 0;

	mutex_lock(&dev->share_lock);
	for (hb_node = rb_first(&dev->share_buffers); hb_node != NULL; hb_node = rb_next(hb_node)) {
		total_num++;
	}
	mutex_unlock(&dev->share_lock);

	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	share_id = (int32_t *)kzalloc(sizeof(int32_t) * (size_t)total_num, GFP_ATOMIC);
	if (share_id == NULL) {
		return -ENOMEM;
	}

	mutex_lock(&dev->share_lock);
	for (hb_node = rb_first(&dev->share_buffers); hb_node != NULL; hb_node = rb_next(hb_node)) {
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
		share_hd = rb_entry(hb_node, struct ion_share_handle, hb_node);
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
		(void)_ion_buf_show(dev, s, share_id[i]);
	}

	kfree(share_id);

	return 0;
}

static int ion_buf_open(struct inode *hb_inode, struct file *hb_file)
{
	return single_open(hb_file, ion_buf_show, hb_inode->i_private);
}

//coverity[misra_c_2012_rule_8_9_violation:SUPPRESS], ## violation reason SYSSW_V_8.9_02
static const struct file_operations ion_buf_fops = {
	.open = ion_buf_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

//coverity[HIS_VOCF:SUPPRESS], ## violation reason SYSSW_V_VOCF_01
struct ion_device *ion_device_create(long (*custom_ioctl)
				     (struct ion_client *client,
				      unsigned int cmd,
				      unsigned long arg))
{
	struct ion_device *idev;
	int ret;
	struct dentry *debug_file;

	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	idev = (struct ion_device *)kzalloc(sizeof(*idev), GFP_KERNEL);
	if (idev == NULL) {
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		return (struct ion_device *)ERR_PTR(-ENOMEM);
	}

	idev->dev.minor = MISC_DYNAMIC_MINOR;
	idev->dev.name = "ion";
	idev->dev.fops = &ion_fops;
	idev->dev.parent = NULL;
	ret = misc_register(&idev->dev);
	if (ret) {
		(void)pr_err("ion: failed to register misc device.\n");
		kfree(idev);
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
		return (struct ion_device *)ERR_PTR(ret);
	}

	idev->debug_root = debugfs_create_dir("ion", NULL);
	if (idev->debug_root == NULL) {
		(void)pr_err("ion: failed to create debugfs root directory.\n");
		//coverity[misra_c_2012_rule_15_1_violation:SUPPRESS], ## violation reason SYSSW_V_15.1_02
		goto debugfs_done;
	}
	idev->heaps_debug_root = debugfs_create_dir("heaps", idev->debug_root);
	if (idev->heaps_debug_root == NULL) {
		(void)pr_err("ion: failed to create debugfs heaps directory.\n");
		//coverity[misra_c_2012_rule_15_1_violation:SUPPRESS], ## violation reason SYSSW_V_15.1_02
		goto debugfs_done;
	}
	idev->clients_debug_root = debugfs_create_dir("clients",
						idev->debug_root);
	if (idev->clients_debug_root == NULL)
		(void)pr_err("ion: failed to create debugfs clients directory.\n");

	debug_file = debugfs_create_file("all_heap_info", 0664,
					idev->heaps_debug_root, idev,
					&debug_all_heap_fops);
	if (debug_file == NULL) {
		char buf[256], *hb_path;

		hb_path = dentry_path(idev->heaps_debug_root, buf, 256);
		(void)pr_err("Failed to create heap debugfs at %s all_heap_info\n", hb_path);
	}

	idev->ion_buf_debug_file = debugfs_create_file("ion_buf",
						0644, idev->debug_root, idev, &ion_buf_fops);

debugfs_done:

	idev->custom_ioctl = custom_ioctl;
	idev->buffers = RB_ROOT;
	mutex_init(&idev->buffer_lock);
	init_rwsem(&idev->lock);
	plist_head_init(&idev->heaps);
	idev->clients = RB_ROOT;
	idev->share_buffers = RB_ROOT;
	idev->share_pool_buffers = RB_ROOT;
	idev->share_groups = RB_ROOT;
	idr_init(&idev->shd_idr);
	idr_init(&idev->group_idr);
	mutex_init(&idev->share_lock);
	mutex_init(&idev->share_group_lock);
	return idev;
}
//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
EXPORT_SYMBOL(ion_device_create);

void ion_device_destroy(struct ion_device *dev)
{
	idr_destroy(&dev->shd_idr);
	idr_destroy(&dev->group_idr);
	misc_deregister(&dev->dev);
	debugfs_remove(dev->ion_buf_debug_file);
	debugfs_remove_recursive(dev->debug_root);
	/* XXX need to free the heaps and clients ? */
	kfree(dev);
}
//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
EXPORT_SYMBOL(ion_device_destroy);
