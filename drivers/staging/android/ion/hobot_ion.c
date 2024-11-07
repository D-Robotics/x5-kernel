/*
 * drivers/staging/android/ion/hobot_ion.c
 *
 * Copyright (C) 2020 horizon
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
 * @file hobot_ion.c
 *
 * @NO{S21E04C01U}
 * @ASIL{B}
 */
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/memblock.h>
#include <linux/sizes.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <asm/cacheflush.h>
#include <linux/ion.h>
#include <linux/syscalls.h>
#include <linux/fdtable.h>


#define ION_GET_PHY (0)		/**< get physical custom command */
#define ION_CACHE_INVAL (1)	/**< cache invalidate custom command */
#define ION_CACHE_FLUSH (2)	/**< cache flush custom command */
#define ION_MEMCPY (3)		/**< dma copy custom command */

#if defined(CONFIG_HOBOT_FPGA_J5) || defined(CONFIG_HOBOT_J5)
#define MAX_DMA_NUM	(2)
#else
#define MAX_DMA_NUM	(3)
#endif

/*
 * default size carveout size from
 * cma heap if no carveout set in dts
 */
#define DFT_CMA_CARVEOUT_SIZE	(512UL * 0x100000UL)	/**< carveout heap default size from cma heap if no carveout set in dts*/

#define DFT_CMA_SIZE	(256UL * 0x100000UL)		/**< cma heap defualt size if not set in dts*/

#define DFT_LEAST_CMA_SIZE	(128UL * 0x100000UL)	/**< the least cma heap size*/

#define TRUE		(bool)1
#define FALSE		(bool)0

#define DMA_TRANSFER_TIMEOUT_MS		1000	//1s


/* transfer info with usersapce in hobot custom ioctl */
/**
 * transfer info with usersapce in hobot custom ioctl
 */
/**
 * @struct ion_phy_data
 * @brief transfer info with usersapce in hobot custom ioctl
 * @NO{S21E04C01U}
 */
struct ion_phy_data {
	ion_user_handle_t handle;	/**< ion dst handle handle*/
	ion_user_handle_t src_handle;	/**< ion source handle id*/
	phys_addr_t paddr;			/**< phys address */
	size_t len;					/**< the size */
	uint64_t reserved;			/**< reserved */
};

/* For carveout heap which alloc from cma heap */
/**
 * For carveout heap which alloc from cma heap
 */
/**
 * @struct ion_cma_carveout
 * @brief For carveout heap which alloc from cma heap
 * @NO{S21E04C01U}
 */
struct ion_cma_carveout {
	struct ion_client *i_cc_client;	/**< the cma carveout ion client */
	struct ion_handle *i_cc_handle;	/**< the cma carveout ion handle */
	enum ion_heap_type heap_type;	/**< heap type */
	/* store place in cma heap */
	phys_addr_t start;				/**< the start phys address */
	size_t size;					/**< the heap size */

	bool cc_is_valid;				/**< the vaild flag */
};

/**
 * store ion device, ion heaps and dma channel information
 */
/**
 * @struct hobot_ion
 * @brief store ion device, ion heaps and dma channel information
 * @NO{S21E04C01U}
 */
struct hobot_ion {
	struct ion_device *ion_dev;			/**< the pointer to ion device */
	struct ion_heap **heaps;			/**< the heap range */

	/* use for dma transfer */
	struct mutex dma_lock;				/**< the dma transfer mutex lock */
	struct completion dma_completion;	/**< the completion of dma operation */

	/* For carveout heap which alloc from cma heap */
	struct ion_cma_carveout ion_cc;		/**< carveout heap alloc from cma heap */

	/* For cma carveout heap which alloc from cma heap */
	struct ion_cma_carveout ion_cma_cc;	/**< cma carveout heap alloc from cma heap */
	uint64_t cma_reserved_size;			/**< the cma reserved size */
	uint64_t cma_default_size;			/**< the cma default size */

	int32_t multi_cma;					/**< the number of cma heaps */

	/* store chunk heap start */
	void *chunk_ptr;					/**< store chunk heap start */
};

/**
 * Purpose: Store ion device, ion heaps and dma channel information
 * Value: NA
 * Range: NA
 * Attention: NA
 */
/**
 * store ion device, ion heaps and dma channel information
 */
static struct hobot_ion *hb_ion;

/**
 * Purpose: Ion device, for other driver use hobot ion device in kernel
 * Value: NA
 * Range: NA
 * Attention: NA
 */
/**
 * ion device for other driver use hobot ion device in kernel
 */
//coverity[misra_c_2012_rule_8_4_violation:SUPPRESS], ## violation reason SYSSW_V_8.4_01
//coverity[misra_c_2012_rule_8_7_violation:SUPPRESS], ## violation reason SYSSW_V_8.7_01
struct ion_device *hb_ion_dev;
//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
EXPORT_SYMBOL(hb_ion_dev);

/**
 * Purpose: all hobot ion can support heaps
 * Value: NA
 * Range: NA
 * Attention: NA
 */
/**
 * all hobot ion can support heaps
 */
static struct ion_platform_heap hobot_heaps[] = {
		{
			.id	= (uint32_t)ION_HEAP_TYPE_SYSTEM,
			.type	= ION_HEAP_TYPE_SYSTEM,
			.name	= "system",
		},
		{
			.id	= (uint32_t)ION_HEAP_TYPE_SYSTEM_CONTIG,
			.type	= ION_HEAP_TYPE_SYSTEM_CONTIG,
			.name	= "system_contig",
		},
		{
			.id	= (uint32_t)ION_HEAP_TYPE_CARVEOUT,
			.type	= ION_HEAP_TYPE_CARVEOUT,
			.name	= "carveout",
			.size	= SZ_4M,
		},
		{
			.id	= (uint32_t)ION_HEAP_TYPE_CHUNK,
			.type	= ION_HEAP_TYPE_CHUNK,
			.name	= "chunk",
			.size	= SZ_4M,
			.align	= SZ_64K,
			.priv	= (void *)(SZ_64K),
		},
		{
			.id	= (uint32_t)ION_HEAP_TYPE_DMA,
			.type	= ION_HEAP_TYPE_DMA,
			.name	= "ion_cma",
		},
		{
			.id     = (uint32_t)ION_HEAP_TYPE_CUSTOM,
			.type   = ION_HEAP_TYPE_CUSTOM,
			.name   = "custom",
		},
		{
			.id 	= (uint32_t)ION_HEAP_TYPE_CMA_RESERVED,
			.type	= ION_HEAP_TYPE_CMA_RESERVED,
			.name	= "cma_reserved",
			.size	= SZ_4M,
		},
		{
			.id = (uint32_t)ION_HEAP_TYPE_SRAM_LIMIT,
			.type	= ION_HEAP_TYPE_SRAM_LIMIT,
			.name	= "limit_sram",
		},
		{
			.id = (uint32_t)ION_HEAP_TYPE_INLINE_ECC,
			.type	= ION_HEAP_TYPE_INLINE_ECC,
			.name	= "inline_ecc",
		},
		{
			.id = (uint32_t)ION_HEAP_TYPE_DMA_EX,
			.type	= ION_HEAP_TYPE_DMA_EX,
			.name	= "cma_ex",
		},
};

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief filter the dma device
 *
 * @param[in] chan: the dma chan
 * @param[in] param: the dma index
 *
 * @retval "=true": succeed
 * @retval "=false": failed
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
static bool ion_dma_filter(struct dma_chan *chan, void *param)
{
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	int32_t dma_idx = *(int32_t *)param;
	struct dma_device *dma_dev = chan->device;

	if (dma_idx == dma_dev->dev_id) {
		return TRUE;
	}

	return FALSE;
}

/* code review E1: Linux interface without return value */
/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief complete the completion
 *
 * @param[in] data: the dma chan
 *
 * @return None
 *
 * @callgraph
 * @callergraph
 *
 * @data_read None
 * @data_updated hb_ion->dma_completion
 * @design
 */
static void ion_dma_cb(void *data)
{
	complete(&hb_ion->dma_completion);
}

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief request dma chan
 *
 * @retval "correct_ptr": succeed
 * @retval "=NULL": failed
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
static struct dma_chan *ion_dma_chan_request(void)
{
	struct dma_chan *chan;
	dma_cap_mask_t mask;
	int32_t dma_idx = 0;
	int32_t i;

	dma_cap_zero(mask);
	dma_cap_set(DMA_MEMCPY, mask);
	for (i = (MAX_DMA_NUM - 1); i >= 0; i--) {
		dma_idx = i;
		chan = dma_request_channel(mask, ion_dma_filter, &dma_idx);
		if (chan != NULL) {
			break;
		}
	}

	if (chan == NULL) {
		(void)pr_err("hobot_ion: dma_request_channel failed\n");
		return NULL;
	}
	return chan;
}

/* code review E1: Linux interface without return value */
/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief release the dma chan
 *
 * @param[in] chan: the dma chan
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
static void ion_dma_chan_release(struct dma_chan *chan)
{
	dma_release_channel(chan);
}

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief dma transfer
 *
 * @param[in] dst: destination dma address
 * @param[in] src: source dma address
 * @param[in] len: copy size
 *
 * @retval "=0": succeed
 * @retval "=-14": bad address
 * @retval "=-19": no such device I/O error
 * @retval "=-5": I/O error
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
//coverity[HIS_VOCF:SUPPRESS], ## violation reason SYSSW_V_VOCF_01
static int32_t hobot_ion_dma_transfer(dma_addr_t dst,
		dma_addr_t src, size_t len)
{
	struct dma_async_tx_descriptor *tx;
	struct dma_chan *dma_ch;
	struct dma_device *dma_dev;
	dma_cookie_t cookie;
	int32_t ret;

	if (dst == 0ULL) {
		(void)pr_err("ION DMA transfer dst[0x%llx] error\n", dst);
		return -EFAULT;
	}

	mutex_lock(&hb_ion->dma_lock);
	dma_ch = ion_dma_chan_request();
	if (dma_ch == NULL) {
		mutex_unlock(&hb_ion->dma_lock);
		(void)pr_err("ION DMA transfer no cma channel\n");
		return -ENODEV;
	}

	dma_dev = dma_ch->device;
	reinit_completion(&hb_ion->dma_completion);

	tx = dma_dev->device_prep_dma_memcpy(dma_ch, dst, src, len, 0);
	if (tx == NULL) {
		ion_dma_chan_release(dma_ch);
		mutex_unlock(&hb_ion->dma_lock);
		(void)pr_err("ION DMA transfer device_prep_dma_memcpy failed!!\n");
		return -EIO;
	}

	tx->callback = ion_dma_cb;
	tx->callback_result = NULL;
	tx->callback_param = NULL;

	cookie = dmaengine_submit(tx);
	ret = dma_submit_error(cookie);
	if (ret) {
		(void)dmaengine_terminate_all(dma_ch);
		ion_dma_chan_release(dma_ch);
		mutex_unlock(&hb_ion->dma_lock);
		(void)pr_err("ION DMA transfer dma_submit_error %d\n", cookie);
		return -EIO;
	}

	dma_async_issue_pending(dma_ch);

	ret = (int32_t)wait_for_completion_interruptible_timeout(&hb_ion->dma_completion,
			msecs_to_jiffies(DMA_TRANSFER_TIMEOUT_MS));
	if (ret == 0) {
		(void)pr_err("%s:%d, pid: %d, dma copy timeout\n", __func__, __LINE__, current->pid);
		(void)dmaengine_terminate_all(dma_ch);
		ret = -ETIMEDOUT;
	} else if (ret < 0) {
		(void)pr_err("%s:%d, pid: %d, dma copy failed %d\n", __func__, __LINE__, current->pid, ret);
		(void)dmaengine_terminate_all(dma_ch);
	} else {
		//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
		pr_debug("%s:%d wake up\n", __func__, __LINE__);
		ret = 0;
	}
	ion_dma_chan_release(dma_ch);
	mutex_unlock(&hb_ion->dma_lock);

	return ret;
}

static int32_t hobot_ion_dma_copy_sg2contig(struct ion_buffer *src_buffer, struct ion_buffer *dst_buffer,
		dma_addr_t dst, dma_addr_t src, size_t len)
{
	struct ion_buffer *sg_buffer;
	dma_addr_t src_start_phys_addr1 = 0, src_start_phys_addr2 = 0;
	size_t src_len1 = 0, src_len2 = 0;
	size_t first_len = 0;
	int32_t ret = 0;

	if (IS_ERR(src_buffer)) {
		(void)pr_err("%s: src buffer cannot be null\n", __func__);
		return -EINVAL;
	}

	if (src_buffer->priv_buffer == NULL) {
		(void)pr_err("%s: src buffer is not sg buffer\n", __func__);
		return -EINVAL;
	}

	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	sg_buffer = (struct ion_buffer *)src_buffer->priv_buffer;
	ret = src_buffer->heap->ops->phys(src_buffer->heap, src_buffer, &src_start_phys_addr1, &src_len1);
	ret = sg_buffer->heap->ops->phys(sg_buffer->heap, sg_buffer, &src_start_phys_addr2, &src_len2);

	if ((src >= src_start_phys_addr1) && (src < src_start_phys_addr1 + src_len1)) {
		first_len = src_start_phys_addr1 + src_len1 - src;
		ret = hobot_ion_dma_transfer(dst, src, first_len);
		ret = hobot_ion_dma_transfer(dst + first_len, src_start_phys_addr2, len - first_len);
	} else if ((src >= src_start_phys_addr2) && (src < src_start_phys_addr2 + src_len2)) {
		ret = hobot_ion_dma_transfer(dst, src, len);
	} else {
		return -EINVAL;
	}

	return ret;
}

//coverity[HIS_VOCF:SUPPRESS], ## violation reason SYSSW_V_VOCF_01
static int32_t hobot_ion_dma_copy_contig2sg(struct ion_buffer *src_buffer, struct ion_buffer *dst_buffer,
		dma_addr_t dst, dma_addr_t src, size_t len)
{
	struct ion_buffer *sg_buffer;
	dma_addr_t dst_start_phys_addr1 = 0, dst_start_phys_addr2 = 0;
	size_t dst_len1 = 0, dst_len2 = 0;
	size_t first_len = 0;
	int32_t ret = 0;

	if (IS_ERR(dst_buffer)) {
		(void)pr_err("%s: dst buffer cannot be null\n", __func__);
		return -EINVAL;
	}

	if (dst_buffer->priv_buffer == NULL) {
		(void)pr_err("%s: src buffer is not sg buffer\n", __func__);
		return -EINVAL;
	}

	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	sg_buffer = (struct ion_buffer *)dst_buffer->priv_buffer;
	ret = dst_buffer->heap->ops->phys(dst_buffer->heap, dst_buffer, &dst_start_phys_addr1, &dst_len1);
	ret = sg_buffer->heap->ops->phys(sg_buffer->heap, sg_buffer, &dst_start_phys_addr2, &dst_len2);

	if ((dst >= dst_start_phys_addr1) && (src < dst_start_phys_addr1 + dst_len1)) {
		first_len = dst_start_phys_addr1 + dst_len1 - dst;
		ret = hobot_ion_dma_transfer(dst, src, first_len);
		ret = hobot_ion_dma_transfer(dst_start_phys_addr2, src + first_len, len - first_len);
	} else if ((dst >= dst_start_phys_addr2) && (dst < dst_start_phys_addr2 + dst_len2)) {
		ret = hobot_ion_dma_transfer(dst, src, len);
	} else {
		return -EINVAL;
	}

	return ret;
}

//coverity[HIS_VOCF:SUPPRESS], ## violation reason SYSSW_V_VOCF_01
static int32_t hobot_ion_dma_copy(struct ion_client *client, int32_t dst_handle_id, int32_t src_handle_id,
		dma_addr_t dst, dma_addr_t src, size_t len)
{
	int32_t ret = 0;
	struct ion_buffer *dst_buffer, *src_buffer;
	struct ion_handle *dst_handle, *src_handle;

	if (client == NULL) {
		(void)pr_err("%s: client cannot be null\n", __func__);
		return -EINVAL;
	}

	//1. find the src_handle_id
	mutex_lock(&client->lock);

	dst_handle = ion_handle_get_by_id_nolock(client, dst_handle_id);
	if (IS_ERR(dst_handle)) {
		mutex_unlock(&client->lock);
		(void)pr_err("%s: find ion dst handle failed [%ld].",
				__func__, PTR_ERR(dst_handle));
		//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		return PTR_ERR(dst_handle);
	}
	dst_buffer = dst_handle->buffer;

	src_handle = ion_handle_get_by_id_nolock(client, src_handle_id);
	if (IS_ERR(dst_handle)) {
		mutex_unlock(&client->lock);
		ion_handle_put(dst_handle);
		(void)pr_err("%s: find ion dst handle failed [%ld].",
				__func__, PTR_ERR(dst_handle));
		//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		return PTR_ERR(dst_handle);
	}
	src_buffer = src_handle->buffer;
	mutex_unlock(&client->lock);

	if (src_buffer->priv_buffer == NULL) {
		// the src handle buffer is not alloc from ion or not import
		if (dst_buffer->priv_buffer != NULL) {
			//this buffer is sg buffer
			ret = hobot_ion_dma_copy_contig2sg(src_buffer, dst_buffer, dst, src, len);
		} else {
			//this dst buffer is not sg buffer
			ret = hobot_ion_dma_transfer(dst, src, len);
		}
	} else {
		// the src handle buffer is alloc from ion
		src_buffer = src_handle->buffer;
		ret = hobot_ion_dma_copy_sg2contig(src_buffer, dst_buffer, dst, src, len);
	}
	ion_handle_put(dst_handle);
	ion_handle_put(src_handle);

	return ret;
}

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief create the heap
 *
 * @param[in] ion_cc: the ion cma carveout heap
 * @param[in] size: the ion cma carveout heap size
 * @param[in] heap_id_mask: the heap mask
 *
 * @retval "0": succeed
 * @retval "<0": Linux errno
 * @retval ">0": err ptr
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
//coverity[HIS_VOCF:SUPPRESS], ## violation reason SYSSW_V_VOCF_01
static int32_t ion_cma_carveout_range_create(struct ion_cma_carveout *ion_cc, size_t size,
					unsigned int heap_id_mask)
{
	struct ion_platform_heap *heap_data;
	int32_t ret, i;

	if (!ion_cc->cc_is_valid) {
		return 0;
	}

	ion_cc->i_cc_client = ion_client_create(hb_ion->ion_dev,
			"ion_cma_carveout");
	if (ion_cc->i_cc_client == NULL) {
		(void)pr_err("Create ion cma carveout client failed!!\n");
		return -ENOMEM;
	}

	ion_cc->i_cc_handle = ion_alloc(ion_cc->i_cc_client,
			size, 0x10, heap_id_mask, 0xffffU << 16);
	if ((ion_cc->i_cc_handle == NULL)
			|| IS_ERR(ion_cc->i_cc_handle)) {
		ion_client_destroy(ion_cc->i_cc_client);
		ion_cc->i_cc_handle = NULL;
		ion_cc->i_cc_client = NULL;
		(void)pr_err("Alloc ion cma carveout buffer failed!!\n");
		return -ENOMEM;
	}

	ret = ion_phys(ion_cc->i_cc_client,
			ion_cc->i_cc_handle->id,
			&ion_cc->start, &ion_cc->size);
	if (ret != 0) {
		ion_free(ion_cc->i_cc_client,
				ion_cc->i_cc_handle);
		ion_client_destroy(ion_cc->i_cc_client);
		ion_cc->i_cc_handle = NULL;
		ion_cc->i_cc_client = NULL;
		(void)pr_err("Alloced ion cma carveout buffer get phys failed!!\n");
		return -ENOMEM;
	}

	hobot_heaps[ion_cc->heap_type].base = ion_cc->start;
	hobot_heaps[ion_cc->heap_type].size = ion_cc->size;

	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	for (i = 0; (uint64_t)i < ARRAY_SIZE(hobot_heaps); i++) {
		heap_data = &hobot_heaps[i];

		if (heap_data->type == ion_cc->heap_type &&
							heap_data->base != 0ULL) {
			hb_ion->heaps[i] = ion_heap_create(heap_data);
			if (IS_ERR_OR_NULL(hb_ion->heaps[i])) {
				ion_free(ion_cc->i_cc_client,
						ion_cc->i_cc_handle);
				ion_client_destroy(ion_cc->i_cc_client);
				ion_cc->i_cc_handle = NULL;
				ion_cc->i_cc_client = NULL;
				hobot_heaps[ion_cc->heap_type].base = 0;
				hobot_heaps[ion_cc->heap_type].size = 0;
				(void)pr_err("Create ion cma carveout heap failed!!\n");
				//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
				return PTR_ERR(hb_ion->heaps[i]);
			}
			ion_device_add_heap(hb_ion->ion_dev, hb_ion->heaps[i]);
			break;
		}
	}

	return 0;
}

/* code review E1: Linux interface without return value */
/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief destroy the heap
 *
 * @param[in] ion_cc: the ion cma carveout heap
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
static void ion_cma_carveout_range_discard(struct ion_cma_carveout *ion_cc)
{
	struct ion_platform_heap *heap_data;
	uint32_t i;

	if (!ion_cc->cc_is_valid) {
		return;
	}

	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	for (i = 0U; i < ARRAY_SIZE(hobot_heaps); i++) {
		heap_data = &hobot_heaps[i];
		if (heap_data->type == ion_cc->heap_type &&
							heap_data->base != 0ULL) {
			ion_device_del_heap(hb_ion->ion_dev, hb_ion->heaps[i]);
			ion_heap_destroy(hb_ion->heaps[i]);
			heap_data->base = 0;
			heap_data->size = 0;
		}
	}

	if (ion_cc->i_cc_handle != NULL) {
		ion_free(ion_cc->i_cc_client,
				ion_cc->i_cc_handle);
		ion_cc->i_cc_handle = NULL;
	}

	if (ion_cc->i_cc_client != NULL) {
		ion_client_destroy(ion_cc->i_cc_client);
		ion_cc->i_cc_client = NULL;
	}

	ion_cc->start = 0;
	ion_cc->size = 0;
}

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief get the alloced buffer number of the heap of specified type
 *
 * @param[in] client: the ion client
 * @param[in] type: the heap type
 *
 * @retval num: the buffer number
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
static uint32_t ion_heap_buf_num(struct ion_client *client,
				   uint32_t type)
{
	uint32_t num = 0;
	struct rb_node *n;

	mutex_lock(&client->lock);
	for (n = rb_first(&client->handles); n != NULL; n = rb_next(n)) {
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
		struct ion_handle *handle = rb_entry(n,
						     struct ion_handle,
						     hb_node);
		if ((uint32_t)handle->buffer->heap->type == type) {
			num++;
		}
	}
	mutex_unlock(&client->lock);

	return num;
}

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief resize the sram carveout heap
 *
 * @param[in] heap: the ion heap structure
 * @param[in] size: the devcie atttibute
 *
 * @retval "0": failed
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
static int32_t ion_sram_range_resize(struct ion_heap *heap, size_t size)
{
	uint32_t num = 0;
	struct rb_node *n;
	int32_t ret = 0;
	size_t sram_size = 0, sram_limit_size = 0;

	sram_size = hobot_heaps[ION_HEAP_TYPE_CUSTOM].size;
	sram_limit_size = hobot_heaps[ION_HEAP_TYPE_SRAM_LIMIT].size;
	if (size == sram_limit_size) {
		(void)pr_err("limit resize not allowed, the input size is same as limit heap size\n");
		return -EINVAL;
	}

	if (size >= sram_limit_size + sram_size) {
		(void)pr_err("limit resize not allowed, the input size is oversize\n");
		return -EINVAL;
	}

	for (n = rb_first(&hb_ion->ion_dev->clients); n != NULL; n = rb_next(n)) {
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
		struct ion_client *client = rb_entry(n, struct ion_client, hb_node);
		num = ion_heap_buf_num(client, (uint32_t)heap->type);
		if (num > 0U) {
			(void)pr_err("sram limit heap is using, can't be resize!!!\n");
			return -ENODEV;
		}
	}

	for (n = rb_first(&hb_ion->ion_dev->clients); n != NULL; n = rb_next(n)) {
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
		struct ion_client *client = rb_entry(n, struct ion_client, hb_node);
		num = ion_heap_buf_num(client, (uint32_t)ION_HEAP_TYPE_CUSTOM);
		if (num > 0U) {
			(void)pr_err("sram heap is using, can't be resize!!!\n");
			return -ENODEV;
		}
	}

	ion_device_del_heap(hb_ion->ion_dev, heap);
	ion_heap_destroy(heap);
	ion_device_del_heap(hb_ion->ion_dev, hb_ion->heaps[ION_HEAP_TYPE_CUSTOM]);
	ion_heap_destroy(hb_ion->heaps[ION_HEAP_TYPE_CUSTOM]);

	hobot_heaps[ION_HEAP_TYPE_CUSTOM].size = sram_size + sram_limit_size - size;
	hobot_heaps[ION_HEAP_TYPE_SRAM_LIMIT].size = size;

	hb_ion->heaps[ION_HEAP_TYPE_SRAM_LIMIT] = ion_heap_create(&hobot_heaps[ION_HEAP_TYPE_SRAM_LIMIT]);
	ion_device_add_heap(hb_ion->ion_dev, hb_ion->heaps[ION_HEAP_TYPE_SRAM_LIMIT]);
	hb_ion->heaps[ION_HEAP_TYPE_CUSTOM] = ion_heap_create(&hobot_heaps[ION_HEAP_TYPE_CUSTOM]);
	ion_device_add_heap(hb_ion->ion_dev, hb_ion->heaps[ION_HEAP_TYPE_CUSTOM]);

	return ret;
}

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief show the sram carveout heap size
 *
 * @param[in] dev: the ion device
 * @param[in] attr: the devcie atttibute
 * @param[in] buf: buffer in user space
 *
 * @return the number of bytes read
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
static ssize_t ion_sram_limit_size_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%lu\n", (hb_ion->ion_cc.size / 0x100000UL));
}

static ssize_t ion_sram_limit_size_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t len)
{
	uint32_t tmp_size;
	int32_t ret;
	struct ion_heap *heap;

	//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
	//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
	plist_for_each_entry(heap, &hb_ion->ion_dev->heaps, hb_node) {
		/* if the caller didn't specify this heap id */
		if ((((uint32_t)1U << (uint32_t)heap->type) & (uint32_t)ION_HEAP_TYPE_SRAM_LIMIT_MASK) != 0U) {
			break;
		}
	}

	ret = sscanf(buf, "%du", &tmp_size);
	if (ret < 0) {
		(void)pr_err("get the input data failed\n");
		return (ssize_t)len;
	}

	if (0U == tmp_size) {
		(void)pr_err("the resize can not be 0\n");
		return (ssize_t)len;
	}

	ret = ion_sram_range_resize(heap, (size_t)tmp_size * 0x100000UL);
	if (ret < 0) {
		(void)pr_err("limit sram heap resize failed\n");
		return (ssize_t)len;
	}

	return (ssize_t)len;
}

//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
static DEVICE_ATTR(sram_limit_size, S_IRUGO | S_IWUSR,
		ion_sram_limit_size_show,
		ion_sram_limit_size_store);

static int32_t ion_cma_carveout_range_resize(struct ion_cma_carveout *ion_cc, size_t size)
{
	uint32_t num = 0;
	struct rb_node *n;
	int32_t ret = 0;

	if ((size == ion_cc->size)
			|| (!ion_cc->cc_is_valid)) {
		(void)pr_info("cma carveout resize not allowed(%lu != %lu or carveout valid(%d))!!!\n",
			ion_cc->size, size, ion_cc->cc_is_valid);
		return 0;
	}

	for (n = rb_first(&hb_ion->ion_dev->clients); n != NULL; n = rb_next(n)) {
		//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
		//coverity[misra_c_2012_rule_18_4_violation:SUPPRESS], ## violation reason SYSSW_V_18.4_03
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
		struct ion_client *client = rb_entry(n, struct ion_client,
						     hb_node);
		num = ion_heap_buf_num(client, (uint32_t)ion_cc->heap_type);
		if (num > 0U) {
			(void)pr_err("cma carveout head is using, can't be resize!!!\n");
			return -ENODEV;
		}
	}

	ion_cma_carveout_range_discard(ion_cc);

	if (size > 0ULL) {
		ret = ion_cma_carveout_range_create(ion_cc, size, ION_HEAP_TYPE_DMA_MASK);
	}

	return ret;
}

static ssize_t ion_cma_carveout_size_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%lu\n", (hb_ion->ion_cc.size / 0x100000UL));
}

static ssize_t ion_cma_carveout_size_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t len)
{
	uint32_t tmp_size;
	int32_t ret;

	ret = sscanf(buf, "%du", &tmp_size);
	if (ret < 0) {
		return 0;
	}

	(void)ion_cma_carveout_range_resize(&hb_ion->ion_cc, (size_t)tmp_size * 0x100000UL);

	return (ssize_t)len;
}

//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
static DEVICE_ATTR(cma_carveout_size, 0664,
		ion_cma_carveout_size_show,
		ion_cma_carveout_size_store);

static struct attribute *ion_dev_attrs[] = {
	&dev_attr_cma_carveout_size.attr,
	&dev_attr_sram_limit_size.attr,
	NULL,
};

//coverity[misra_c_2012_rule_8_9_violation:SUPPRESS], ## violation reason SYSSW_V_8.9_02
static struct attribute_group ion_dev_attr_group = {
	.attrs = ion_dev_attrs,
};

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief hobot ion ioctl for get phsical address,flush cache,invalidate cache and dma copy
 *
 * @param[in] client: the ion client
 * @param[in] cmd: the command
 * @param[in] arg: the user data address
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
static long hobot_ion_ioctl(struct ion_client *client,
			    unsigned int cmd, unsigned long arg)
{
	struct ion_phy_data phy_data;
	int32_t ret = 0;

	//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_11.6_01
	if (copy_from_user(&phy_data,
				(void __user *)arg, sizeof(struct ion_phy_data))) {
		(void)pr_err("ION cmd[%d]:copy from user failed!!\n", cmd);
		return -EFAULT;
	}

	switch (cmd) {
		case ION_GET_PHY:
			//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			ret = ion_phys(client, (long)phy_data.handle,
					&phy_data.paddr, &phy_data.len);
			//coverity[misra_c_2012_rule_11_6_violation:SUPPRESS], ## violation reason SYSSW_V_11.6_01
			if (copy_to_user((void __user *)arg,
						&phy_data, sizeof(struct ion_phy_data))) {
				(void)pr_err("ION get phy addr[0x%llx] copy to user failed!!\n",
						phy_data.paddr);
				return -EFAULT;
			}

			break;
		case ION_CACHE_FLUSH:
			if (phy_data.paddr == 0ULL) {
				(void)pr_err("ION cache flush addr[0x%llx] error\n", phy_data.paddr);
				return -EINVAL;
			}

			dma_sync_single_for_device(client->dev->dev.this_device, phy_data.paddr,
					phy_data.len, DMA_TO_DEVICE);
			//dcache_clean_inval_poc((unsigned long)phys_to_virt(phy_data.paddr), (unsigned long)phys_to_virt(phy_data.paddr) + phy_data.len);

			break;
		case ION_CACHE_INVAL:
			if (phy_data.paddr == 0ULL) {
				(void)pr_err("ION cache invalid addr[0x%llx] error\n", phy_data.paddr);
				return -EINVAL;
			}

			dma_sync_single_for_cpu(client->dev->dev.this_device, phy_data.paddr,
					phy_data.len, DMA_FROM_DEVICE);
			//dcache_inval_poc((unsigned long)phys_to_virt(phy_data.paddr), (unsigned long)phys_to_virt(phy_data.paddr) + phy_data.len);

			break;
		case ION_MEMCPY:
			//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			ret = hobot_ion_dma_copy(client, phy_data.handle, phy_data.src_handle, phy_data.paddr,
					phy_data.reserved, phy_data.len);
			break;
		default:
			return -ENOTTY;
	}

	return ret;
}

/**
 * @NO{S21E04C01I}
 * @ASIL{B}
 * @brief get a;; the heap range in ION
 *
 * @param[out] heap_range: the heap range for ion heaps
 * @param[out] num: the heap number
 *
 * @retval "0": success
 * @retval "-EINVAL": invalid parameter
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
//coverity[misra_c_2012_rule_8_7_violation:SUPPRESS], ## violation reason SYSSW_V_8.7_01
//coverity[HIS_VOCF:SUPPRESS], ## violation reason SYSSW_V_VOCF_01
int32_t ion_get_all_heaps_range(struct ion_heap_range *heap_range, int num)
{
	phys_addr_t ion_cma_base = 0, ion_cma_extra_base = 0;
	size_t ion_cma_size = 0, ion_cma_extra_size = 0;
	int idx = 0;

	if (heap_range == NULL) {
		(void)pr_err("Invalid heap range!!\n");
		return -EINVAL;
	}

	if (num < (int32_t)ION_HEAP_TYPE_MAX) {
		(void)pr_err("Invalid heap num:%d!!\n", num);
		return -EINVAL;
	}

	(void)memset((void *)heap_range, 0, sizeof(struct ion_heap_range) * (size_t)num);
	//ion_cma_get_info(hb_ion->ion_dev, &ion_cma_base, &ion_cma_size, ION_HEAP_TYPE_DMA);
	ion_cma_base = hobot_heaps[ION_HEAP_TYPE_DMA].base;
	ion_cma_size = hobot_heaps[ION_HEAP_TYPE_DMA].size;
	heap_range[idx].base = ion_cma_base;
	heap_range[idx++].size = ion_cma_size;

	if ((hb_ion->multi_cma != 0) && (ion_cma_extra_base == 0ULL) && (ion_cma_extra_size == 0ULL)) {
		(void)ion_cma_get_info(hb_ion->ion_dev, &ion_cma_extra_base, &ion_cma_extra_size,
			ION_HEAP_TYPE_DMA_EX);
		heap_range[idx].base = ion_cma_extra_base;
		heap_range[idx++].size = ion_cma_extra_size;
	}

	heap_range[idx].base = hobot_heaps[ION_HEAP_TYPE_CARVEOUT].base;
	heap_range[idx++].size = hobot_heaps[ION_HEAP_TYPE_CARVEOUT].size;

	heap_range[idx].base = hobot_heaps[ION_HEAP_TYPE_CMA_RESERVED].base;
	heap_range[idx++].size = hobot_heaps[ION_HEAP_TYPE_CMA_RESERVED].size;

	heap_range[idx].base = hobot_heaps[ION_HEAP_TYPE_CUSTOM].base;
	heap_range[idx].size = hobot_heaps[ION_HEAP_TYPE_CUSTOM].size;

	return 0;
}
//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
EXPORT_SYMBOL_GPL(ion_get_all_heaps_range);

#ifdef CONFIG_PCIE_HOBOT_EP_FUN_AI

/**
 * @NO{S21E04C01I}
 * @ASIL{B}
 * @brief get the base heap range in ION
 *
 * @param[out] base_addr: the min base address for ion heaps
 * @param[out] total_size: the ion heaps total size
 *
 * @retval "0": success
 * @retval "-EINVAL": invalid parameter
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
int32_t ion_get_heap_range(phys_addr_t *base_addr, size_t *total_size)
{
	int32_t ret = 0;
	if (!base_addr || !total_size) {
		(void)pr_err("Invalid argument!!\n");
		return -EINVAL;
	}
	phys_addr_t min_addr = 0, ion_cma_base = 0;
	size_t size = 0, ion_cma_size = 0;

	//ion_cma_get_info(hb_ion->ion_dev, &ion_cma_base, &ion_cma_size, ION_HEAP_TYPE_DMA);
	ion_cma_base = hobot_heaps[ION_HEAP_TYPE_DMA].base;
	ion_cma_size = hobot_heaps[ION_HEAP_TYPE_DMA].size;
	hobot_heaps[ION_HEAP_TYPE_DMA].base = ion_cma_base;
	hobot_heaps[ION_HEAP_TYPE_DMA].size = ion_cma_size;

	min_addr = hobot_heaps[ION_HEAP_TYPE_CARVEOUT].base;
	size += hobot_heaps[ION_HEAP_TYPE_CARVEOUT].size;

	if(min_addr > hobot_heaps[ION_HEAP_TYPE_CMA_RESERVED].base)
		min_addr = hobot_heaps[ION_HEAP_TYPE_CMA_RESERVED].base;
	size += hobot_heaps[ION_HEAP_TYPE_CMA_RESERVED].size;

	if(min_addr > hobot_heaps[ION_HEAP_TYPE_DMA].base)
		min_addr = hobot_heaps[ION_HEAP_TYPE_DMA].base;
	size += hobot_heaps[ION_HEAP_TYPE_DMA].size;

	pr_info("the carveout PA is %llx, the size is %lx\n",
		hobot_heaps[ION_HEAP_TYPE_CARVEOUT].base, hobot_heaps[ION_HEAP_TYPE_CARVEOUT].size);
	pr_info("the cma carveout PA is %llx, the size is %lx\n",
		hobot_heaps[ION_HEAP_TYPE_CMA_RESERVED].base, hobot_heaps[ION_HEAP_TYPE_CMA_RESERVED].size);
	pr_info("the cma PA is %llx, the size is %lx\n",
		hobot_heaps[ION_HEAP_TYPE_DMA].base, hobot_heaps[ION_HEAP_TYPE_DMA].size);

	*base_addr = min_addr;
	*total_size = size;
	return ret;
}
EXPORT_SYMBOL_GPL(ion_get_heap_range);

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief get the all heap range in ION
 *
 * @param[out] heap_range: the all ion heap range structure
 *
 * @retval "0": success
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
int32_t ion_get_all_heap_range(struct ion_heap_range_data_pac *heap_range)
{
	static phys_addr_t ion_cma_base = 0, ion_cma_extra_base = 0;
	static size_t ion_cma_size = 0, ion_cma_extra_size = 0;

	if (!heap_range) {
		(void)pr_err("Invalid argument!!\n");
		return -EINVAL;
	}

	//ion_cma_get_info(hb_ion->ion_dev, &ion_cma_base, &ion_cma_size, ION_HEAP_TYPE_DMA);
	ion_cma_base = hobot_heaps[ION_HEAP_TYPE_DMA].base;
	ion_cma_size = hobot_heaps[ION_HEAP_TYPE_DMA].size;
	heap_range->heap[ION_HEAP_TYPE_DMA].base = ion_cma_base;
	heap_range->heap[ION_HEAP_TYPE_DMA].size = ion_cma_size;

	if ((hb_ion->multi_cma != 0) && (ion_cma_extra_base == 0) && (ion_cma_extra_size == 0)) {
		ion_cma_get_info(hb_ion->ion_dev, &ion_cma_extra_base, &ion_cma_extra_size,
			ION_HEAP_TYPE_DMA_EX);
	}
	heap_range->heap[ION_HEAP_TYPE_DMA_EX].base = ion_cma_extra_base;
	heap_range->heap[ION_HEAP_TYPE_DMA_EX].size = ion_cma_extra_size;

	heap_range->heap[ION_HEAP_TYPE_CARVEOUT].base = hobot_heaps[ION_HEAP_TYPE_CARVEOUT].base;
	heap_range->heap[ION_HEAP_TYPE_CARVEOUT].size = hobot_heaps[ION_HEAP_TYPE_CARVEOUT].size;

	heap_range->heap[ION_HEAP_TYPE_CMA_RESERVED].base = hobot_heaps[ION_HEAP_TYPE_CMA_RESERVED].base;
	heap_range->heap[ION_HEAP_TYPE_CMA_RESERVED].size = hobot_heaps[ION_HEAP_TYPE_CMA_RESERVED].size;

	heap_range->heap[ION_HEAP_TYPE_CUSTOM].base = hobot_heaps[ION_HEAP_TYPE_CUSTOM].base;
	heap_range->heap[ION_HEAP_TYPE_CUSTOM].size = hobot_heaps[ION_HEAP_TYPE_CUSTOM].size;

	return 0;
}
EXPORT_SYMBOL_GPL(ion_get_all_heap_range);

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief the ion custom cmd operation function
 *
 * @param[in] ion_rev_data: the ion receive data
 *
 * @retval "=0": success
 * @retval "<0": fail
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
int32_t ion_function_custom_operation(struct ion_data_pac *ion_rev_data)
{
	struct ion_handle *cleanup_handle = NULL;
	int32_t ret = 0;

	switch (ion_rev_data->cmd_custom) {
	case ION_CUSTOM_OPEN: {
		struct ion_client *client;

		client = ion_client_create(hb_ion_dev, ion_rev_data->module_name);
		if (IS_ERR(client))
			return PTR_ERR(client);
		ion_rev_data->client = (void *)client;
		ion_rev_data->return_value = ret;
		break;
	}

	case ION_CUSTOM_RELEASE: {
		struct ion_client *client = (struct ion_client*)ion_rev_data->client;

		ion_client_destroy(client);
		ion_rev_data->return_value = ret;
		break;
	}

	case ION_SHARE_FD_RELEASE: {
		ret = close_fd(ion_rev_data->data.fd.fd);
		if (ret < 0)
			pr_err("release the share fd fail!!\n");
		ion_rev_data->return_value = ret;
		break;
	}

	case ION_GET_PHY: {
		struct ion_client *client = (struct ion_client*)ion_rev_data->client;
		struct ion_phy_data_pac phy_data;

		phy_data = ion_rev_data->data.phy_data;
		ret = ion_phys(client, (int)phy_data.handle,
				&phy_data.paddr, &phy_data.len);
		ion_rev_data->data.phy_data.paddr = phy_data.paddr;
		ion_rev_data->data.phy_data.len = phy_data.len;
		ion_rev_data->return_value = ret;
		break;
	}
	case ION_MEMCPY: {
		struct ion_phy_data_pac phy_data;

		phy_data = ion_rev_data->data.phy_data;
		ret = hobot_ion_dma_transfer(phy_data.paddr,
				phy_data.reserved, phy_data.len);
		ion_rev_data->return_value = ret;
		break;
	}
	case ION_CHECK_IN_HEAP: {
		struct ion_phy_data_pac phy_data;

		phy_data = ion_rev_data->data.phy_data;
		ret = ion_check_in_heap_carveout(phy_data.paddr, phy_data.len);
		ion_rev_data->return_value = ret;
		break;
	}
	case ION_IMPORT_WITH_SHARE_ID: {
		struct ion_handle *handle;
		struct ion_client *client = (struct ion_client*)ion_rev_data->client;

		handle = ion_import_dma_buf_with_shareid(client, ion_rev_data->data.share_import_data.share_id);
		if (IS_ERR(handle)) {
			ret = PTR_ERR(handle);
		}
		else {
			ion_rev_data->data.share_import_data.handle = handle->id;
		}
		ion_rev_data->return_value = ret;
		break;
	}
	default:
		return -ENOTTY;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(ion_function_custom_operation);
#endif

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
//coverity[misra_c_2012_rule_8_7_violation:SUPPRESS], ## violation reason SYSSW_V_8.7_01
//coverity[HIS_CCM:SUPPRESS], ## violation reason SYSSW_V_CCM_01
int32_t ion_check_in_heap_carveout(phys_addr_t start, size_t size)
{
	struct ion_platform_heap *cvt = &hobot_heaps[ION_HEAP_TYPE_CARVEOUT];
	struct ion_platform_heap *cma_reserved = &hobot_heaps[ION_HEAP_TYPE_CMA_RESERVED];
	struct ion_platform_heap *custom = &hobot_heaps[ION_HEAP_TYPE_CUSTOM];
	static phys_addr_t ion_cma_base = 0, ion_cma_extra_base = 0;
	static size_t ion_cma_size = 0, ion_cma_extra_size = 0;
	phys_addr_t end_addr = start + size;

	if ((start == 0ULL) || (size == 0UL)) {
		(void)pr_err("invalid input parameter, start: 0x%llx, size: 0x%lx\n", start, size);
		return -EINVAL;
	}

	if ((ion_cma_base == 0ULL) && (ion_cma_size == 0ULL)) {
		//ion_cma_get_info(hb_ion->ion_dev, &ion_cma_base, &ion_cma_size, ION_HEAP_TYPE_DMA);
		ion_cma_base = hobot_heaps[ION_HEAP_TYPE_DMA].base;
		ion_cma_size = hobot_heaps[ION_HEAP_TYPE_DMA].size;
	}

	if ((hb_ion->multi_cma != 0) && (ion_cma_extra_base == 0ULL) && (ion_cma_extra_size == 0ULL)) {
		(void)ion_cma_get_info(hb_ion->ion_dev, &ion_cma_extra_base, &ion_cma_extra_size,
			ION_HEAP_TYPE_DMA_EX);
	}
	pr_debug("%s: Checking:%#llx(%#lx)\n", __func__, start, size);
	pr_debug("%s: cvt:%#llx(%#lx), cam_reserved:%#llx(%#lx), ion_cma:%#llx(%#lx), "
			 "custom:%#llx(%#lx), ion_cma_extra:%#llx(%#lx)\n",
			__func__,
			cvt->base, cvt->size,
			cma_reserved->base, cma_reserved->size,
			ion_cma_base, ion_cma_size,
			custom->base, custom->size,
			ion_cma_extra_base, ion_cma_extra_size);
	if ((start < cvt->base || end_addr > cvt->base + cvt->size)
		&& (start < cma_reserved->base || end_addr > cma_reserved->base + cma_reserved->size)
		&& (start < ion_cma_base || end_addr > ion_cma_base + ion_cma_size)
		&& (start < custom->base || end_addr > custom->base + custom->size)) {
		if ((hb_ion->multi_cma == 0) ||
			(start < ion_cma_extra_base || end_addr > ion_cma_extra_base + ion_cma_extra_size)) {
			return -EINVAL;
		}
	}

	return 0;
}
//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
EXPORT_SYMBOL_GPL(ion_check_in_heap_carveout);

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief get the heap size by heap_id
 *
 * @param[in] heap_id: the heap id
 *
 * @retval "total_size": the heap size
 * @retval "-EINVAL": invalid parameter
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
//coverity[misra_c_2012_rule_8_7_violation:SUPPRESS], ## violation reason SYSSW_V_8.7_01
size_t hobot_get_heap_size(uint32_t heap_id)
{
	size_t total_size = 0;
	uint32_t i = 0;
	struct ion_platform_heap *heap_data;

	if (heap_id > (uint32_t)ION_HEAP_TYPE_DMA_EX)
		//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
		return -EINVAL;

	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01s
	for (i = 0U; i < ARRAY_SIZE(hobot_heaps); i++) {
		heap_data = &hobot_heaps[i];

		if (heap_data->id != heap_id)
			continue;
		total_size = heap_data->size;
	}

	return total_size;
}
//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
EXPORT_SYMBOL_GPL(hobot_get_heap_size);

//coverity[misra_c_2012_rule_8_7_violation:SUPPRESS], ## violation reason SYSSW_V_8.7_01
struct ion_device *hobot_ion_get_ion_device(void)
{
	return hb_ion_dev;
}
//coverity[misra_c_2012_rule_8_6_violation:SUPPRESS], ## violation reason SYSSW_V_8.6_01
//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
EXPORT_SYMBOL_GPL(hobot_ion_get_ion_device);

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief check whether heap range in memory range
 *
 * @param[in] heap_base: the heap start address
 * @param[in] heap_end: the heap end address
 *
 * @retval "0": success
 * @retval "-EINVAL": invalid parameter
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
static int32_t hobot_check_heap_valid(phys_addr_t heap_base, phys_addr_t heap_end)
{
	uint64_t i = 0;
	phys_addr_t block_start, block_end;

	for_each_mem_range(i, &block_start, &block_end) {
		if ((heap_base >= block_start) && (heap_end <= block_end))
			return 0;
	}

	return -EINVAL;
}

//coverity[HIS_LEVEL:SUPPRESS], ## violation reason SYSSW_V_LEVEL_01
static int32_t hobot_ion_get_heap_info(struct device_node *rnode, char *heap_name, int32_t heap_id,
				size_t default_size)
{
	struct device_node *hb_node;
	struct resource ion_pool_reserved;
	const char *status;
	int32_t ret = 0;
	bool need_rnode_get = true, heap_disabled = true;

	hb_node = of_find_compatible_node(rnode, NULL, heap_name);
	while (hb_node != NULL) {
		need_rnode_get = false;
		//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_01
		status = of_get_property(hb_node, "status", NULL);
		pr_debug("%s: %s(%s) status:%s\n", __func__, hb_node->full_name, heap_name, status);
		if ((status == NULL) || (strcmp(status, "okay") == 0)
				|| (strcmp(status, "ok") == 0)) {
			heap_disabled = false;
			if (of_address_to_resource(hb_node, 0, &ion_pool_reserved) == 0) {
				if (hobot_heaps[heap_id].base != 0) {
					pr_alert_once("Multiple %s enabled! The last one will take effect!\n",
									hobot_heaps[heap_id].name);
				}
				hobot_heaps[heap_id].base
					= ion_pool_reserved.start;
				hobot_heaps[heap_id].size
					= resource_size(&ion_pool_reserved);
				(void)pr_info("hobot_ion: Reserved %s MEM start 0x%llx, size 0x%lx\n",
						heap_name,
						hobot_heaps[heap_id].base,
						hobot_heaps[heap_id].size);
				ret = hobot_check_heap_valid(hobot_heaps[heap_id].base,
						hobot_heaps[heap_id].base + hobot_heaps[heap_id].size);
				if (ret < 0) {
					(void)pr_info("Hobot ion-pool not in memory range\n");
					of_node_put(hb_node);
					return -EINVAL;
				}
			}
		}
		of_node_put(hb_node);
		hb_node = of_find_compatible_node(hb_node, NULL, heap_name);
	}

	if (heap_disabled) {
		hobot_heaps[heap_id].size = default_size;
	}

	if (need_rnode_get) {
		(void)of_node_get(rnode);
	}

	return ret;
}

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief prepare the heaps, read the information from dts
 *
 * @param[in] hb_ion: the hobot ion information structure
 *
 * @retval "0": in the carveout heap range
 * @retval "-EINVAL": invalid parameter
 *
 * @data_read None
 * @data_updated None
 *
 * @callgraph
 * @callergraph
 * @design
 */
static int32_t hobot_heaps_prepare(struct hobot_ion *hb_ion_data)
{
	struct device_node *hb_node, *rnode;
	int32_t err = 0, ret = 0;

	if (hb_ion_data == NULL) {
		return -EINVAL;
	}

	/* Allocate a hobot chunk heap */
	hb_ion_data->chunk_ptr = alloc_pages_exact(
			hobot_heaps[ION_HEAP_TYPE_CHUNK].size,
			GFP_KERNEL);
	if (hb_ion_data->chunk_ptr != NULL) {
		hobot_heaps[ION_HEAP_TYPE_CHUNK].base =
			virt_to_phys(hb_ion_data->chunk_ptr);
	} else {
		(void)pr_err("hobot_ion: Could not allocate chunk\n");
	}

	rnode = of_find_node_by_path("/reserved-memory");
	if (rnode == NULL) {
		(void)pr_info("Hobot ION can't find reserved mem for heaps\n");
		return 0;
	}

	ret = hobot_ion_get_heap_info(rnode, "ion-pool", (int32_t)ION_HEAP_TYPE_CMA_RESERVED,
							(size_t)DFT_CMA_CARVEOUT_SIZE);
	if (ret < 0) {
		(void)pr_info("Hobot ion-pool memory range get failed\n");
		return ret;
	}

	ret = hobot_ion_get_heap_info(rnode, "ion-carveout", (int32_t)ION_HEAP_TYPE_CARVEOUT,
							(size_t)DFT_CMA_CARVEOUT_SIZE);
	if (ret < 0) {
		(void)pr_info("Hobot ion-carveout memory range get failed\n");
		return ret;
	}

	hb_node = of_find_compatible_node(rnode, NULL, "shared-dma-pool");
	if (hb_node != NULL) {
		err = of_property_read_u64(hb_node, "reserved-size", &hb_ion_data->cma_reserved_size);
		if (err != 0) {
			hb_ion_data->cma_reserved_size = 0;
		}
		err = of_property_read_u64(hb_node, "default-size", &hb_ion_data->cma_default_size);
		if (err != 0) {
			hb_ion_data->cma_default_size = DFT_CMA_SIZE;
		}
		of_node_put(hb_node);
	} else {
		(void)of_node_get(rnode);
	}

	ret = hobot_ion_get_heap_info(rnode, "ion-sram", (int32_t)ION_HEAP_TYPE_CUSTOM,
							(size_t)0UL);
	if (ret < 0) {
		(void)pr_info("Hobot ion-sram memory range get failed\n");
		return ret;
	}

	ret = hobot_ion_get_heap_info(rnode, "ion-sram-limit", (int32_t)ION_HEAP_TYPE_SRAM_LIMIT,
							(size_t)0UL);
	if (ret < 0) {
		(void)pr_info("Hobot ion-sram-limit memory range get failed\n");
		return ret;
	}

	ret = hobot_ion_get_heap_info(rnode, "ion-inline-ecc", (int32_t)ION_HEAP_TYPE_INLINE_ECC,
							(size_t)0UL);
	if (ret < 0) {
		(void)pr_info("Hobot ion-inline-ecc memory range get failed\n");
		return ret;
	}

	ret = hobot_ion_get_heap_info(rnode, "ion-cma", (int32_t)ION_HEAP_TYPE_DMA,
							(size_t)0UL);
	if (ret < 0) {
		(void)pr_info("Hobot ion-cma memory range get failed\n");
		return ret;
	}

	return 0;
}

/* code review E1: Linux interface without return value */
/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief unprepare the heaps, free the chunk heap
 *
 * @param[in] hb_ion: the hobot ion information structure
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
static void hobot_heaps_unprepare(struct hobot_ion *hb_ion_data)
{
	if (hb_ion_data->chunk_ptr != NULL) {
		free_pages_exact(hb_ion_data->chunk_ptr,
				hobot_heaps[ION_HEAP_TYPE_CHUNK].size);
		hb_ion_data->chunk_ptr = NULL;
	}
}

/* code review E1: Linux interface without return value */
/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief create cma carveout heaps
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
//coverity[HIS_CCM:SUPPRESS], ## violation reason SYSSW_V_CCM_01
//coverity[HIS_LEVEL:SUPPRESS], ## violation reason SYSSW_V_LEVEL_01
static void hobot_ion_cma_carveout_prepare(struct hobot_ion *hb_ion_data)
{
	int32_t ret;
	phys_addr_t ion_cma_base = 0, ion_cma_extra_base = 0;
	size_t ion_cma_size = 0, ion_cma_extra_size = 0;

	if ((hobot_heaps[ION_HEAP_TYPE_CMA_RESERVED].base == 0ULL) ||
		(hobot_heaps[ION_HEAP_TYPE_CARVEOUT].base == 0ULL)) {
		if (ion_cma_get_info(hb_ion_data->ion_dev, &ion_cma_base, &ion_cma_size, ION_HEAP_TYPE_DMA) != 0) {
			(void)pr_err("hobot_ion: fail to find dma heap.\n");
			return;
		}
		if (ion_cma_get_info(hb_ion_data->ion_dev, &ion_cma_extra_base, &ion_cma_extra_size, ION_HEAP_TYPE_DMA_EX) != 0) {
			(void)pr_info("hobot_ion: No dma extra heap.\n");
			hb_ion_data->multi_cma = 0;
			hb_ion_data->ion_dev->multi_cma = 0;
		} else {
			hb_ion_data->multi_cma = 1;
			hb_ion_data->ion_dev->multi_cma = 1;
		}

		if (hb_ion_data->cma_reserved_size == 0) {
			if (hb_ion_data->multi_cma == 0) {
				hb_ion_data->cma_reserved_size = (ion_cma_size > (hobot_heaps[ION_HEAP_TYPE_CARVEOUT].size
					+ hb_ion_data->cma_default_size)) ?
					(ion_cma_size - hobot_heaps[ION_HEAP_TYPE_CARVEOUT].size - hb_ion_data->cma_default_size) :
					((ion_cma_size > hobot_heaps[ION_HEAP_TYPE_CARVEOUT].size) ?
					(ion_cma_size - hobot_heaps[ION_HEAP_TYPE_CARVEOUT].size) : 0);
			} else {
				hb_ion_data->cma_reserved_size = ion_cma_extra_size;
			}
		}
	}
	hb_ion_data->ion_cma_cc.cc_is_valid = FALSE;
	if (hb_ion_data->cma_reserved_size != 0) {
		hb_ion_data->ion_cma_cc.cc_is_valid = TRUE;
		if (hobot_heaps[ION_HEAP_TYPE_CMA_RESERVED].base == 0) {
			hb_ion_data->ion_cma_cc.heap_type = ION_HEAP_TYPE_CMA_RESERVED;
			hobot_heaps[ION_HEAP_TYPE_CMA_RESERVED].size = hb_ion_data->cma_reserved_size;
			(void)pr_info("ion_dummy: user set cma reserved memory, try to alloc from cma\n");

			if (ion_cma_carveout_range_create(&hb_ion_data->ion_cma_cc,
				hobot_heaps[ION_HEAP_TYPE_CMA_RESERVED].size,
				(hb_ion_data->multi_cma == 0) ? ION_HEAP_TYPE_DMA_MASK : ION_HEAP_TYPE_DMA_EX_MASK) == 0) {
				hobot_heaps[ION_HEAP_TYPE_CMA_RESERVED].base = hb_ion_data->ion_cma_cc.start;
				hobot_heaps[ION_HEAP_TYPE_CMA_RESERVED].size = hb_ion_data->ion_cma_cc.size;

				(void)pr_info("ION CMA reserved MEM start 0x%llx, size 0x%lx\n",
						hobot_heaps[ION_HEAP_TYPE_CMA_RESERVED].base,
						hobot_heaps[ION_HEAP_TYPE_CMA_RESERVED].size);
			} else {
				(void)pr_err("hobot_ion: not reserve memory\n");
			}
		}
	}

	hb_ion_data->ion_cc.cc_is_valid = FALSE;
	if (hobot_heaps[ION_HEAP_TYPE_CARVEOUT].size != 0UL) {
		hb_ion_data->ion_cc.cc_is_valid = TRUE;
		if (hobot_heaps[ION_HEAP_TYPE_CARVEOUT].base == 0ULL) {
			hb_ion_data->ion_cc.heap_type = ION_HEAP_TYPE_CARVEOUT;
			(void)pr_info("ion_dummy: not reserve carveout memory, try to alloc from cma\n");

			// we should reserve some space for cma heap, or some test cases may fail
			if (hobot_heaps[ION_HEAP_TYPE_CARVEOUT].size >=  ion_cma_size) {
				hobot_heaps[ION_HEAP_TYPE_CARVEOUT].size = ion_cma_size - (size_t)DFT_LEAST_CMA_SIZE;
			}
			if (ion_cma_carveout_range_create(&hb_ion_data->ion_cc,
				hobot_heaps[ION_HEAP_TYPE_CARVEOUT].size,
				ION_HEAP_TYPE_DMA_MASK) == 0) {
				hobot_heaps[ION_HEAP_TYPE_CARVEOUT].base = hb_ion_data->ion_cc.start;
				hobot_heaps[ION_HEAP_TYPE_CARVEOUT].size = hb_ion_data->ion_cc.size;

				(void)pr_info("ION Carveout MEM start 0x%llx, size 0x%lx\n",
						hobot_heaps[ION_HEAP_TYPE_CARVEOUT].base,
						hobot_heaps[ION_HEAP_TYPE_CARVEOUT].size);

				ret = device_add_group(hb_ion_data->ion_dev->dev.this_device,
						&ion_dev_attr_group);
				if (ret < 0) {
					(void)pr_info("Create ion cma carveout size sys node failed\n");
				}
			} else {
				(void)pr_err("hobot_ion: not reserve memory\n");
			}
		}
	}
}

/* code review E1: Linux interface without return value */
/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief destory cma carveout heaps
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
static void hobot_ion_cma_carveout_unprepare(struct hobot_ion *hb_ion_data)
{
	ion_cma_carveout_range_discard(&hb_ion_data->ion_cma_cc);
	ion_cma_carveout_range_discard(&hb_ion_data->ion_cc);
}

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief hobot ion driver init
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
//coverity[HIS_CCM:SUPPRESS], ## violation reason SYSSW_V_CCM_01
static int32_t __init hobot_ion_init(void)
{
	struct ion_platform_heap *heap_data;
	int32_t i, j, ret = 0;
	phys_addr_t ion_cma_base = 0;
	size_t ion_cma_size = 0;

	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	hb_ion = (struct hobot_ion *)kzalloc(sizeof(struct hobot_ion), GFP_KERNEL);
	if (hb_ion == NULL) {
		(void)pr_err("Hobot ION create failed!!\n");
		return -ENOMEM;
	}

	hb_ion->ion_dev = ion_device_create(hobot_ion_ioctl);
	if (IS_ERR_OR_NULL(hb_ion->ion_dev)) {
		kfree(hb_ion);
		(void)pr_err("Hobot ION device create failed!!\n");
		return -EINVAL;
	}

	hb_ion_dev = hb_ion->ion_dev;

	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	//coverity[misra_c_2012_rule_11_5_violation:SUPPRESS], ## violation reason SYSSW_V_11.5_02
	hb_ion->heaps = (struct ion_heap **)kcalloc(ARRAY_SIZE(hobot_heaps),
			sizeof(struct ion_heap *), GFP_KERNEL);
	if (hb_ion->heaps == NULL) {
		ion_device_destroy(hb_ion->ion_dev);
		kfree(hb_ion);
		(void)pr_err("Hobot ION heaps failed!!\n");
		hb_ion = NULL;
		hb_ion_dev = NULL;
		return -ENOMEM;
	}

	ret = hobot_heaps_prepare(hb_ion);
	if (ret < 0) {
		kfree(hb_ion->heaps);
		ion_device_destroy(hb_ion->ion_dev);
		kfree(hb_ion);
		(void)pr_err("Hobot ION heaps prepare failed!!\n");
		hb_ion = NULL;
		hb_ion_dev = NULL;
		return ret;
	}

	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	for (i = 0; (size_t)i < ARRAY_SIZE(hobot_heaps); i++) {
		heap_data = &hobot_heaps[i];

		if (((heap_data->type == ION_HEAP_TYPE_CARVEOUT)
				|| (heap_data->type == ION_HEAP_TYPE_CHUNK)
				|| (heap_data->type == ION_HEAP_TYPE_CUSTOM)
				|| (heap_data->type == ION_HEAP_TYPE_CMA_RESERVED)
				|| (heap_data->type == ION_HEAP_TYPE_INLINE_ECC)
				|| (heap_data->type == ION_HEAP_TYPE_SRAM_LIMIT)
				|| (heap_data->type == ION_HEAP_TYPE_DMA))
				&& (heap_data->base == 0ULL)) {
			continue;
		}

		if (heap_data->type == ION_HEAP_TYPE_DMA_EX) {
			(void)ion_add_cma_heaps(hb_ion->ion_dev);
			(void)ion_cma_get_info(hb_ion->ion_dev, &ion_cma_base, &ion_cma_size, heap_data->type);
			hobot_heaps[heap_data->type].base = ion_cma_base;
			hobot_heaps[heap_data->type].size = ion_cma_size;
			continue;
		}

		hb_ion->heaps[i] = ion_heap_create(heap_data);
		if (IS_ERR_OR_NULL(hb_ion->heaps[i])) {
			//coverity[misra_c_2012_rule_10_3_violation:SUPPRESS], ## violation reason SYSSW_V_10.3_03
			ret = PTR_ERR(hb_ion->heaps[i]);
			for (j = i - 1; j >= 0; j--) {
				ion_device_del_heap(hb_ion->ion_dev, hb_ion->heaps[j]);
				ion_heap_destroy(hb_ion->heaps[j]);
			}
			(void)ion_del_cma_heaps(hb_ion->ion_dev);
			hobot_heaps_unprepare(hb_ion);
			kfree(hb_ion->heaps);
			ion_device_destroy(hb_ion->ion_dev);
			kfree(hb_ion);
			(void)pr_err("Hobot ION create heaps failed!!\n");
			hb_ion = NULL;
			hb_ion_dev = NULL;
			return -EINVAL;
		}
		ion_device_add_heap(hb_ion->ion_dev, hb_ion->heaps[i]);
	}

	hobot_ion_cma_carveout_prepare(hb_ion);

	ret = sysfs_create_file(&(hb_ion->ion_dev->dev.this_device->kobj),
						&dev_attr_sram_limit_size.attr);
	if (ret < 0) {
		(void)pr_err("Create sys node failed\n");
	}

	mutex_init(&hb_ion->dma_lock);
	init_completion(&hb_ion->dma_completion);

	return 0;
}
//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
subsys_initcall(hobot_ion_init);

/**
 * @NO{S21E04C01U}
 * @ASIL{B}
 * @brief hobot ion driver init
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
static void __exit hobot_ion_exit(void)
{
	uint32_t i;

	hobot_ion_cma_carveout_unprepare(hb_ion);
	ion_device_destroy(hb_ion->ion_dev);

	//coverity[misra_c_2012_rule_10_4_violation:SUPPRESS], ## violation reason SYSSW_V_10.4_01
	for (i = 0U; i < ARRAY_SIZE(hobot_heaps); i++) {
		ion_heap_destroy(hb_ion->heaps[i]);
	}

	(void)ion_del_cma_heaps(hb_ion->ion_dev);
	hobot_heaps_unprepare(hb_ion);
	//device_remove_file(hb_ion->ion_dev->dev.this_device, &dev_attr_sram_limit_size);
	kfree(hb_ion->heaps);
	kfree(hb_ion);
	hb_ion = NULL;
	hb_ion_dev = NULL;
}
//coverity[misra_c_2012_rule_20_7_violation:SUPPRESS], ## violation reason SYSSW_V_20.7_01
__exitcall(hobot_ion_exit);

/* module_init(hobot_ion_init);
module_exit(hobot_ion_exit);
MODULE_VERSION("1.0.0");
MODULE_AUTHOR("Jiahui Zhang <jiahui.zhang@horizon.ai>");
MODULE_DESCRIPTION("hobot ion driver");
MODULE_LICENSE("GPL"); */
