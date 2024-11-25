/***
 *                     COPYRIGHT NOTICE
 *            Copyright 2024 D-Robotics, Inc.
 *                   All rights reserved.
 ***/
#define pr_fmt(fmt) "hobot alsa: dma: " fmt
#include "./hobot-platform-x5.h"

/*E1: Dma Engine's API, so no return*/
/**
 * @NO{S14E01C01I}
 * @ASIL{B}
 * @brief pcm dma irq callback function
 * update hardware pointer
 *
 * @param[in] arg private data of runtime
 *
 * @return None
 *
 * @callgraph
 * @callergraph
 * @data_read NULL
 * @data_updated
 * @design
 */
static void hobot_pcm_dma_complete_cyclic(void *arg)/*PRQA S ALL*/ /*dma's API*/
{
	struct x5_dmadata *dmadata = (struct x5_dmadata *)arg;
	struct snd_pcm_runtime *runtime = dmadata->substream->runtime;
	if (runtime->status->state != SNDRV_PCM_STATE_RUNNING && runtime->status->state != SNDRV_PCM_STATE_DRAINING) {
		dev_dbg(substream2dev(dmadata->substream),/*PRQA S ALL*/ /*qac-9.7.0-1861,3344,3432,4403,4542,4543,4558*/
				"I2sdma: dma cyclic stoped\n");
		return;
	}
	dmadata->done_periods++;

        if ((dmadata->channel > 2) && (dmadata->done_periods % (dmadata->channel / 2) != 0))
                return;

	dmadata->dma_xfer_cnt++;
        if (dmadata->dma_xfer_cnt >= dmadata->periods)
                dmadata->dma_xfer_cnt = 0;

	snd_pcm_period_elapsed(dmadata->substream);
}

/**
 * @NO{S14E01C01U}
 * @ASIL{B}
 * @brief prepare cyclic dma transmission and submit callback function
 *
 * @param[in] dmadata private data of runtime
 *
 * @return 0 for success, < 0 for failure
 *
 * @callgraph
 * @callergraph
 * @data_read NULL
 * @data_updated
 * @design
 */
static int32_t hobotaclc_dma_submit_cyclic(const struct x5_dmadata *dmadata)
{
	struct dma_async_tx_descriptor *desc;
	uint32_t word_width = dmadata->word_width >> (uint32_t)(HOBOT_I2S_BITS_TO_BYTES);
	uint32_t period_size = dmadata->period_size / word_width * dmadata->addr_width;
	uint32_t buffer_size = dmadata->buffer_size / word_width * dmadata->addr_width;
	dma_addr_t new_addr;
	for (int i = 0; i < dmadata->channel / 2; i++) {
		new_addr = dmadata->dma_addr  + dmadata->dma_xfer_cnt * period_size + i * buffer_size;
		desc = dmaengine_prep_dma_cyclic(dmadata->dma_chan[i],
					new_addr,
					buffer_size,
					period_size,
					(dmadata->substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ?
					DMA_MEM_TO_DEV : DMA_DEV_TO_MEM,
					(uint64_t)(DMA_PREP_INTERRUPT) | (uint64_t)(DMA_CTRL_ACK));
		if (desc == NULL) {
			dev_err(substream2dev(dmadata->substream), "I2sdma: cannot prepare slave dma\n");
			return -1;
		}
		desc->callback = hobot_pcm_dma_complete_cyclic;
		desc->callback_param = (void *)(dmadata);/*PRQA S ALL*/ /*qac-9.7.0-0311*/

		dmaengine_submit(desc);

		dma_async_issue_pending(dmadata->dma_chan[i]);
	}

	return 0;
}

/**
 * @NO{S14E01C01U}
 * @ASIL{B}
 * @brief start pcm dma's transmission
 * prepare and pending mission on cyclic dma
 *
 * @param[in] dmadata private data of runtime
 *
 * @return 0 for success, < 0 for failure
 *
 * @callgraph
 * @callergraph
 * @data_read NULL
 * @data_updated
 * @design
 */
int32_t hobot_snd_platform_dma_start(const struct x5_dmadata *dmadata)
{
	int32_t ret;
	ret = hobotaclc_dma_submit_cyclic(dmadata);
	if (ret < 0) {
		dev_err(substream2dev(dmadata->substream), "I2sdma: cannot submit slave dma\n");
		return ret;
	}
	//for (int i = 0; i < dmadata->channel / 2; i++)
	//	dma_async_issue_pending(dmadata->dma_chan[i]);
	return 0;
}

/**
 * @NO{S14E01C01U}
 * @ASIL{B}
 * @brief stop pcm dma's transmission
 *
 * @param[in] dmadata private data of runtime
 *
 * @return 0 for success, < 0 for failure
 *
 * @callgraph
 * @callergraph
 * @data_read NULL
 * @data_updated
 * @design
 */
int32_t hobot_snd_platform_dma_stop(struct x5_dmadata *dmadata)
{
	int32_t ret;
	for (int i = 0; i < dmadata->channel / 2; i++) {
		ret = dmaengine_pause(dmadata->dma_chan[i]);
		if (ret < 0)
			dev_dbg(substream2dev(dmadata->substream), /*PRQA S ALL*/ /*qac-9.7.0-1861,3344,3432,4403,4542,4543,4558*/
				"I2sdma: cannot pause slave dma\n");
		ret = dmaengine_terminate_async(dmadata->dma_chan[i]);
		if (ret != 0)
			dev_warn(substream2dev(dmadata->substream),
				"I2sdma: cannot terminate slave dma\n");
	}
	return 0;
}

/**
 * @NO{S14E01C01U}
 * @ASIL{B}
 * @brief alloc a x5_dmadata structure as runtime'sprivate_data
 *
 * @param[in] substream pcm substream descriptor
 *
 * @return 0 for success, < 0 for failure
 *
 * @callgraph
 * @callergraph
 * @data_read NULL
 * @data_updated i2s J6 pcm controller descriptor
 * @design
 */
int32_t hobot_snd_platform_data_init(const struct snd_pcm_substream * substream)
{
	struct x5_dmadata *dmadata;
	dmadata = (struct x5_dmadata *)devm_kzalloc(substream->pcm->card->dev,
				sizeof(struct x5_dmadata), GFP_ATOMIC);
	if (dmadata == NULL) {
		dev_err(substream2dev(substream), "BUG: Can't allocate dmadata\n");
		return -ENOMEM;
	}
	substream->runtime->private_data = (void *)dmadata;
	spin_lock_init(&dmadata->dma_lock);/*PRQA S ALL*/ /*qac-9.7.0-3200,0662,3469*/
	return 0;
}

/**
 * @NO{S14E01C01U}
 * @ASIL{B}
 * @brief alloc hardware buffer for pcm dma
 *
 * @param[in] pcm pcm descriptor
 * @param[in] stream direction
 *
 * @return 0 for success, < 0 for failure
 *
 * @callgraph
 * @callergraph
 * @data_read NULL
 * @data_updated i2s J6 pcm controller descriptor
 * @design
 */
int32_t hobot_pcm_preallocate_dma_buffer(const struct snd_pcm *pcm, const int32_t stream)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buf = &substream->dma_buffer;
	size_t size = hobot_pcm_hardware.buffer_bytes_max + hobot_pcm_hardware.buffer_bytes_max;
	buf->dev.type = SNDRV_DMA_TYPE_VMALLOC;
	buf->dev.dev = pcm->card->dev;
	buf->private_data = NULL;
	if (size == 0) {
		dev_err(pcm->card->dev, "BUG: buffer_bytes_max is 0!!!\n");
		return -ENOMEM;
	}
	buf->area = devm_kzalloc(pcm->card->dev, size, GFP_DMA | GFP_KERNEL);
	if (buf->area == NULL) {
		dev_err(pcm->card->dev, "BUG: Failed to dma_alloc_coherent\n");
		return -ENOMEM;
	}
	buf->addr = dma_map_single(pcm->card->dev, (void *)buf->area, size, DMA_BIDIRECTIONAL);
	buf->bytes = size;
	return 0;
}

/*E1: alsa's api, so no return*/
/**
 * @NO{S14E01C01U}
 * @ASIL{B}
 * @brief free hardware buffer for pcm dma, at stage of remove module
 *
 * @param[in] pcm pcm descriptor
 *
 * @return 0 for success, < 0 for failure
 *
 * @callgraph
 * @callergraph
 * @data_read NULL
 * @data_updated i2s J6 pcm controller descriptor
 * @design
 */
void hobot_pcm_free_dma_buffers(const struct snd_pcm *pcm)
{
	struct snd_pcm_substream *substream;
	struct snd_dma_buffer *buf;
	int32_t stream;
	for (stream = 0; stream <= SNDRV_PCM_STREAM_LAST; stream++) {
		substream = pcm->streams[stream].substream;
		if (substream == NULL)
			continue;
		buf = &substream->dma_buffer;
		if (buf->area == NULL)
			continue;
		dma_unmap_single(pcm->card->dev, buf->addr, buf->bytes, DMA_BIDIRECTIONAL);
		devm_kfree(pcm->card->dev, buf->area);
		buf->area = NULL;
	}
}

/* Module information */
// PRQA S ALL ++
MODULE_AUTHOR("zhen01.zhang");
MODULE_DESCRIPTION("X5 platform DMA module");
MODULE_LICENSE("GPL");
// PRQA S ALL --
