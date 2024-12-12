/***
 *                     COPYRIGHT NOTICE
 *            Copyright 2024 D-Robotics, Inc.
 *                   All rights reserved.
 ***/

#define pr_fmt(fmt) "hobot alsa: dma: " fmt
#include "archband-pdm.h"
#include "./hobot-platform-x5.h"

#define DMA_BUSWIDTH_4BYTES (4u)
/**
 * snd_pcm_hardware - the gobal variable for setting pcm hardware parameter
 */
struct snd_pcm_hardware hobot_pcm_hardware = {
	.info = (uint32_t)(SNDRV_PCM_INFO_INTERLEAVED) |
			(uint32_t)(SNDRV_PCM_INFO_NONINTERLEAVED) |
			(uint32_t)(SNDRV_PCM_INFO_BLOCK_TRANSFER) |
			(uint32_t)(SNDRV_PCM_INFO_MMAP) |
			(uint32_t)(SNDRV_PCM_INFO_MMAP_VALID) |
			(uint32_t)(SNDRV_PCM_INFO_PAUSE) |
			(uint32_t)(SNDRV_PCM_INFO_RESUME),
	.period_bytes_min = HOBOT_I2S_PERIOD_BYTES_MIN,
	.period_bytes_max = HOBOT_I2S_PERIOD_BYTES_MAX,
	.periods_min = HOBOT_I2S_PERIOD_COUNT_MIN,
	.periods_max = HOBOT_I2S_PERIOD_COUNT_MAX,
	.buffer_bytes_max = HOBOT_I2S_BUFFER_BYTES_MAX,
};

/*E1: To be compatible with memcpy*/
/**
 * @NO{S14E01C01U}
 * @ASIL{B}
 * @brief memcpy for one single frame
 *
 * @param[in] src source address
 * @param[in] size size of format
 * @param[in] dst destination address
 *
 * @return None
 *
 * @callgraph
 * @callergraph
 * @data_read
 * @data_updated
 * @design
 */
static inline void memcpy_inline(void *dst, void *src, uint32_t size)
{
	switch(size) {
	case (uint32_t)(HOBOT_I2S_WIDTH_8_BIT):
		((uint8_t *)dst)[0] = ((uint8_t *)src)[0];
		break;
	case (uint32_t)(HOBOT_I2S_WIDTH_16_BIT):
		((uint16_t *)dst)[0] = ((uint16_t *)src)[0];
		break;
	case (uint32_t)(HOBOT_I2S_WIDTH_32_BIT):
		((uint32_t *)dst)[0] = ((uint32_t *)src)[0];
		break;
	case (uint32_t)(HOBOT_I2S_WIDTH_24_BIT):
		((uint8_t *)dst)[0] = ((uint8_t *)src)[0];
		((uint8_t *)dst)[1] = ((uint8_t *)src)[1];
		((uint8_t *)dst)[2] = ((uint8_t *)src)[2];
		((uint8_t *)dst)[3] = 0u;
		break;
	default:
		pr_err("BUG: word_width:%u not support yet!\n", size);
	}
}

/**
 * @NO{S14E01C01U}
 * @ASIL{B}
 * @brief copy pcm data between nointerleaved and interleaved
 *
 * @param[in] src_addr source address
 * @param[in] src_width size of source data
 * @param[in] dst_addr destination address
 * @param[in] dst_width size of destination data
 * @param[in] count data size
 * @param[in] chn which channel is copying
 * @param[in] channels how many channels of pcm
 * @param[in] stream stream direction
 *
 * @return 0 for success, < 0 for failure
 *
 * @callgraph
 * @callergraph
 * @data_read
 * @data_updated
 * @design
 */
static inline int32_t nointerleaved_copy(const char *src_addr, uint32_t src_width,
									char *dst_addr, uint32_t dst_width, uint32_t count,
									uint32_t chn, uint32_t channels, int32_t stream)
{
	uint32_t i;
	uint32_t step, len, src_num, dst_num;

	if ((dst_addr == NULL) || (src_addr == NULL)) {
		pr_err("BUG: %s has NULL addr\n", __FUNCTION__);
		return -EINVAL;
	}

	if ((channels == 0u) || (dst_width == 0u) || (src_width == 0u)) {
		pr_err("BUG: %s has zero word width\n", __FUNCTION__);
		return -EINVAL;
	}

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		dst_num = chn * dst_width;
		src_num = 0;
		step = channels * dst_width;
		len = sizeof(char) * src_width;
		for (i = 0; i < count; i++) {
			(void)memcpy_inline((void*)(&dst_addr[dst_num]),/*PRQA S ALL*/ /*qac-9.7.0-3200*/
						(void*)(&src_addr[src_num]),
						len);
			dst_num += step;
			src_num += src_width;
		}
	} else {
		src_num = chn * src_width;
		dst_num = 0;
		step = channels * src_width;
		len = sizeof(char) * dst_width;
		for (i = 0; i < count; i++) {
			(void)memcpy_inline((void*)(&dst_addr[dst_num]),/*PRQA S ALL*/ /*qac-9.7.0-3200*/
						(void*)(&src_addr[src_num]),
						len);
			src_num += step;
			dst_num += dst_width;
		}

	}
	return 0;
}

/**
 * @NO{S14E01C01U}
 * @ASIL{B}
 * @brief copy pcm data between pcm buffer and user's interleaved buffer
 *
 * @param[in] substream pcm substream descriptor
 * @param[in] hwoff offset in pcm buffer, count by alsa
 * @param[in] buf user's buffer address
 * @param[in] bytes data size geted form alsa
 *
 * @return 0 for success, < 0 for failure
 *
 * @callgraph
 * @callergraph
 * @data_read
 * @data_updated
 * @design
 */
static inline int32_t copy_usr_interleaved(struct snd_pcm_substream *substream,
											uint64_t hwoff, void *buf, uint64_t bytes, int channel)
{
	char *dma_ptr, *tmp_buf, *out_le, *out_3le;
	struct snd_pcm_runtime *runtime = NULL;
	struct snd_soc_pcm_runtime *rtd = NULL;
	struct snd_soc_dai *cpu_dai = NULL;
	struct device *dev = NULL;
	struct x5_dmadata *dmadata = NULL;
	uint64_t hwoff_real = hwoff;
	dma_addr_t phys;
	int32_t frame_bytes;
	runtime = substream->runtime;
	rtd = (struct snd_soc_pcm_runtime *)substream->private_data;
	cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	dev = cpu_dai->dev;
	int64_t count = 0;
	int32_t acual_bytes;
	size_t buffer_bytes;
	int32_t word_width;
	dmadata = (struct x5_dmadata *)substream->runtime->private_data;
	hwoff_real = hwoff / (dmadata->channel / 2);
	word_width = dmadata->word_width >> (uint32_t)(HOBOT_I2S_BITS_TO_BYTES);
	if (dmadata->format == SNDRV_PCM_FORMAT_S24_3LE) {
		hwoff_real = hwoff_real / word_width * DMA_BUSWIDTH_4BYTES;
	}
	acual_bytes = bytes / (dmadata->channel / 2);
	dma_ptr = runtime->dma_area + hwoff_real + channel * (runtime->dma_bytes / runtime->channels);
	phys = runtime->dma_addr + hwoff_real;
	acual_bytes = acual_bytes / word_width * dmadata->addr_width;
	buffer_bytes = dmadata->buffer_size / word_width * dmadata->addr_width;

	tmp_buf = dmadata->tmp_buf;
	out_le = dmadata->out_le;
	out_3le = dmadata->out_3le;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (copy_from_user((void*)(dma_ptr), (void __user *)buf, bytes) != 0u) {
			dev_err(substream2dev(substream), "BUG: i2sdma: copy_from_user failed\n");
			return -EFAULT;
		}
		dma_sync_single_for_device(dev, phys, bytes, DMA_TO_DEVICE);
	} else {
		for (int i = 0; i < dmadata->channel / 2; i++) {
			dma_sync_single_for_cpu(dev, phys + i * buffer_bytes, acual_bytes, DMA_FROM_DEVICE);
		}

		for (int i = 0; i < dmadata->channel / 2; i++) {
			memcpy(&tmp_buf[i*acual_bytes], &dma_ptr[i*buffer_bytes], acual_bytes);
		}

		frame_bytes = runtime->frame_bits / 8 / (dmadata->channel / 2) / word_width * dmadata->addr_width;
		for (int i = 0; i < acual_bytes;) {
			for (int j = 0; j < dmadata->channel / 2; j++) {
				memcpy(&out_le[count*frame_bytes], &tmp_buf[j*acual_bytes], frame_bytes);
				count++;
			}
			i = i + frame_bytes;
			tmp_buf = tmp_buf + frame_bytes;
		}

		if (dmadata->format == SNDRV_PCM_FORMAT_S24_LE) {
			if (copy_to_user((void __user *)buf, (void*)(out_le), bytes) != 0u) {
				dev_err(substream2dev(substream), "BUG: i2sdma: copy_to_user failed\n");
				return -EFAULT;
			}
		} else {
			for (int i = 0; i < bytes / word_width; i++) {
				memcpy(&out_3le[i*word_width], &out_le[i*DMA_BUSWIDTH_4BYTES], word_width);
			}
			if (copy_to_user((void __user *)buf, (void *)out_3le, bytes)) {
				dev_err(substream2dev(substream), "BUG: i2sdma: copy_to_user failed\n");
				return -EFAULT;
			}
		}
	}

	return 0;
}

/**
 * @NO{S14E01C01U}
 * @ASIL{B}
 * @brief copy pcm data between pcm buffer and user's nointerleaved buffer
 *
 * @param[in] substream pcm substream descriptor
 * @param[in] channel which channel is copying
 * @param[in] hwoff offset in pcm buffer, count by alsa
 * @param[in] buf user's buffer address
 * @param[in] bytes data size geted form alsa
 *
 * @return 0 for success, < 0 for failure
 *
 * @callgraph
 * @callergraph
 * @data_read
 * @data_updated
 * @design
 */
static inline int32_t copy_usr_nointerleaved(struct snd_pcm_substream *substream, int channel,
											uint64_t hwoff, void *buf, uint64_t bytes)
{
	char *dma_ptr;
	uint32_t count, word_width;
	struct snd_pcm_runtime *runtime = NULL;
	struct snd_soc_pcm_runtime *rtd = NULL;
	struct snd_soc_dai *cpu_dai = NULL;
	struct device *dev = NULL;
	struct x5_dmadata *dmadata = NULL;
	uint64_t hwoff_real = 0;
	char * tmp_buf = NULL;
	dma_addr_t phys;
	runtime = substream->runtime;
	rtd = (struct snd_soc_pcm_runtime *)substream->private_data;
	cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	dev = cpu_dai->dev;
	dmadata = (struct x5_dmadata *)substream->runtime->private_data;
	tmp_buf = (char *)(&runtime->dma_area[hobot_pcm_hardware.buffer_bytes_max]);
	word_width = dmadata->word_width >> (uint32_t)(HOBOT_I2S_BITS_TO_BYTES);
	if (bytes % HOBOT_PDMA_ALIGN) {
		hwoff_real = (bytes / HOBOT_PDMA_ALIGN + 1) * HOBOT_PDMA_ALIGN;
		hwoff_real = (hwoff / dmadata->period_size) * hwoff_real;
	}
	dma_ptr = (char *)(&runtime->dma_area[hwoff_real]);
	phys = runtime->dma_addr + hwoff_real;
	count = (uint32_t)bytes / word_width;
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (copy_from_user((void*)(tmp_buf), (void __user *)buf, bytes) != 0u) {
			dev_err(substream2dev(substream), "BUG: i2sdma: copy_from_user failed\n");
			return -EFAULT;
		}
		if (nointerleaved_copy(tmp_buf, word_width, dma_ptr, word_width,
							count, (uint32_t)channel, substream->runtime->channels, substream->stream) < 0)
			return -EFAULT;
		dma_sync_single_for_device(dev, phys, bytes * runtime->channels, DMA_TO_DEVICE);
	} else {
		dma_sync_single_for_cpu(dev, phys, bytes * runtime->channels, DMA_FROM_DEVICE);
		if (nointerleaved_copy(dma_ptr, word_width, tmp_buf, word_width,
							count, (uint32_t)channel, substream->runtime->channels, substream->stream) < 0)
			return -EFAULT;
		if (copy_to_user((void __user *)buf, (void*)(tmp_buf), bytes) != 0u) {
			dev_err(substream2dev(substream), "BUG: i2sdma: copy_to_user failed\n");
			return -EFAULT;
		}
	}
	return 0;
}

/**
 * @NO{S14E01C01I}
 * @ASIL{B}
 * @brief copy data to/from user space
 * this function is be called by pcm_lib
 *
 * @param[in] component pcm component descriptor
 * @param[in] substream pcm substream descriptor
 * @param[in] channel which channel is copying
 * @param[in] hwoff offset in pcm buffer, count by alsa
 * @param[in] buf user's buffer address
 * @param[in] bytes data size geted form alsa
 *
 * @return 0 for success, < 0 for failure
 *
 * @callgraph
 * @callergraph
 * @data_read
 * @data_updated
 * @design
 */
static int32_t x5_pcm_platform_copy_usr(struct snd_soc_component *component,
										struct snd_pcm_substream *substream,/*PRQA S ALL*/ /*alsa's api*/
										int channel, unsigned long hwoff,/*PRQA S ALL*/ /*alsa's api*/
										void *buf, unsigned long bytes)/*PRQA S ALL*/ /*alsa's api*/
{
	struct x5_dmadata *dmadata = NULL;
	struct snd_pcm_runtime *runtime = NULL;
	uint32_t period_size = 0, buffer_size = 0, periods = 0, i = 0, num = 0;
	uint64_t hwoff_t = 0;
	int32_t ret = 0;
	if ((substream == NULL) || (buf == NULL)) {
		pr_err("BUG: NO usefull args for x5_copy_usr\n");/*PRQA S ALL*/ /*qac-9.7.0-1036,1035,3200*/
		return -EINVAL;
	}
	runtime = substream->runtime;
	dmadata = (struct x5_dmadata *)runtime->private_data;
	if (runtime->access == SNDRV_PCM_ACCESS_RW_NONINTERLEAVED) {
		period_size = dmadata->period_size / runtime->channels;
		buffer_size = dmadata->buffer_size / runtime->channels;
		hwoff = hwoff * runtime->channels;
	} else {
		period_size = snd_pcm_lib_period_bytes(substream);
		buffer_size = snd_pcm_lib_buffer_bytes(substream);
		num = hwoff / period_size;
	}
	periods = bytes / period_size;
	for (i = 0; i < periods; i++) {
		hwoff_t = (uint64_t)(hwoff + i * period_size) % buffer_size;
		if (runtime->access == SNDRV_PCM_ACCESS_RW_NONINTERLEAVED) {
			ret = copy_usr_nointerleaved(substream, channel, hwoff_t,
											buf + i * period_size,
											(uint64_t)period_size);
			if (ret < 0)
				break;
		} else {
			ret = copy_usr_interleaved(substream, hwoff_t,
										buf + i * period_size,
										(uint64_t)period_size, channel);
			if (ret < 0)
				break;
		}
	}
	return ret;
}

/**
 * x5_pcm_platform_pointer - called for return data position
 * this function is be called by snd_pcm_period_elapsed
 */
/**
 * @NO{S14E01C01I}
 * @ASIL{B}
 * @brief calculate hardware pointer's offest on pcm buffer
 * this function is be called by snd_pcm_period_elapsed
 *
 * @param[in] component pcm component descriptor
 * @param[in] substream pcm substream descriptor
 *
 * @return The offset of hardware pointer
 *
 * @callgraph
 * @callergraph
 * @data_read
 * @data_updated
 * @design
 */
static snd_pcm_uframes_t
x5_pcm_platform_pointer(struct snd_soc_component *component,
						struct snd_pcm_substream *substream)/*PRQA S ALL*/ /*alsa's API*/
{
	struct x5_dmadata *dmadata;
	snd_pcm_uframes_t frames;
	ssize_t pos;
	if (substream == NULL) {
		pr_err("BUG: NO usefull args for x5_pcm_platform_pointer\n");/*PRQA S ALL*/ /*qac-9.7.0-1036,1035,3200*/
		return -EINVAL;
	}
	dmadata = (struct x5_dmadata *)substream->runtime->private_data;
	pos = (ssize_t)(dmadata->dma_xfer_cnt * snd_pcm_lib_period_bytes(substream));
	frames = (snd_pcm_uframes_t)bytes_to_frames(substream->runtime, pos);
	return frames;
}

/**
 * @NO{S14E01C01I}
 * @ASIL{B}
 * @brief starting, pausing, and stopping the transmission
 * this function is be called by machine driver
 * called after codec and platform's trigger function, before rtd->dai_link
 *
 * @param[in] component pcm component descriptor
 * @param[in] substream pcm substream descriptor
 * @param[in] cmd command like start, pause, stop
 * @param[in] dai alsa soc dai descriptor
 *
 * @return 0 for success, < 0 for failure
 *
 * @callgraph
 * @callergraph
 * @data_read NULL
 * @data_updated
 * @design
 */
static int32_t x5_pcm_platform_trigger(struct snd_soc_component *component,
									   struct snd_pcm_substream *substream,
									   int32_t cmd)/*PRQA S ALL*/ /*alsa's API*/
{
	struct x5_dmadata *dmadata;
	int32_t ret = 0;
	unsigned long flags;/*PRQA S ALL*/ /*for spin_lock_irqsave m3cm-2.4.0-5209*/
	if (substream == NULL) {
		pr_err("BUG: NO usefull args for x5_pcm_platform_trigger\n");/*PRQA S ALL*/ /*qac-9.7.0-1036,1035,3200*/
		return -EINVAL;
	}
	dmadata = (struct x5_dmadata *)substream->runtime->private_data;
	spin_lock_irqsave(&dmadata->dma_lock, flags);/*PRQA S ALL*/ /*qac-9.7.0-3200,1021,3473,2996,3432,1020*/
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
	case SNDRV_PCM_TRIGGER_RESUME:
		ret = hobot_snd_platform_dma_start(dmadata);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		ret = hobot_snd_platform_dma_stop(dmadata);
		break;
	default:
		dev_err(substream2dev(substream),
				"BUG: i2sdma: dma trigger cmd: %d not support\n", cmd);
		ret = -EINVAL;
		break;
	}
	spin_unlock_irqrestore(&dmadata->dma_lock, flags);
	return ret;
}

/**
 * @NO{S14E01C01I}
 * @ASIL{B}
 * @brief Set dma parameters according to J6 pcm substream and parameters,
 * this function is be called by machine driver <soc_pcm_hw_params>
 * The order is rtd->dai_link, codecs, cpudai, platform
 *
 * @param[in] component pcm component descriptor
 * @param[in] substream pcm substream descriptor
 * @param[in] params pcm hardware parameters descriptor
 *
 * @return 0 for success, < 0 for failure
 *
 * @callgraph
 * @callergraph
 * @data_read NULL
 * @data_updated
 * @design
 */
static int32_t x5_pcm_platform_hw_params(struct snd_soc_component *component,
										 struct snd_pcm_substream *substream,
				 						 struct snd_pcm_hw_params *params)/*PRQA S ALL*/ /*alsa's API*/
{
	struct snd_soc_pcm_runtime *rtd = NULL;
	struct snd_soc_dai *cpu_dai = NULL;
	struct snd_pcm_runtime *runtime = NULL;
	struct x5_dmadata *dmadata = NULL;
	struct snd_dmaengine_dai_dma_data *dma_dai_data = NULL;
	struct dma_slave_config config = {0};
	unsigned long flags = 0;/*PRQA S ALL*/ /*for spin_lock_irqsave m3cm-2.4.0-5209*/
	int32_t ret = 0;
	if ((substream == NULL) || (params == NULL)) {
		pr_err("BUG: NO usefull args for x5_pcm_platform_hw_params\n");/*PRQA S ALL*/ /*qac-9.7.0-1036,1035,3200*/
		return -EINVAL;
	}
	rtd = (struct snd_soc_pcm_runtime *)substream->private_data;
	cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	runtime = substream->runtime;
	dmadata = (struct x5_dmadata *)runtime->private_data;
	dma_dai_data = (struct snd_dmaengine_dai_dma_data *)snd_soc_dai_get_dma_data(cpu_dai, substream);
	spin_lock_irqsave(&dmadata->dma_lock, flags);/*PRQA S ALL*/ /*qac-9.7.0-3200,1021,3473,2996,3432,1020*/
	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);
    	runtime->dma_bytes = params_buffer_bytes(params);
	config.direction = (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ?
		DMA_MEM_TO_DEV : DMA_DEV_TO_MEM;
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		config.dst_addr = dma_dai_data->addr;
	else
		config.src_addr = dma_dai_data->addr;
	config.dst_maxburst = dma_dai_data->maxburst;
	config.dst_addr_width = dma_dai_data->addr_width;
	config.device_fc = (bool)false;
	config.src_maxburst = dma_dai_data->maxburst;
	config.src_addr_width = dma_dai_data->addr_width;
	dmadata->dma_addr = runtime->dma_addr;
	dmadata->substream = substream;
	dmadata->addr_width = (uint32_t)(dma_dai_data->addr_width); // Bytes for dma, for 24_LE is 3Bytes
	dmadata->word_width = (uint32_t)(params_physical_width(params)); // bits for data, for 24_LE is 32bits
	dmadata->period_size = params_period_bytes(params) / (params_channels(params) / 2);/*PRQA S ALL*/
	/*qac-9.7.0-4391*/
	dmadata->buffer_size = params_buffer_bytes(params) / (params_channels(params) / 2);
	dmadata->periods = params_periods(params); //buffer_size / period_size
	dmadata->config = config;
	dmadata->stream = substream->stream;
	dmadata->channel = params_channels(params);
	dmadata->format = params_format(params);
	dmadata->done_periods = 0;
	for (int i = 0; i < params_channels(params) / 2 ; i++) {
		if (dmadata->dma_chan[i] != NULL) {
			config.src_addr = dma_dai_data->addr + i*4;
			ret = dmaengine_slave_config(dmadata->dma_chan[i], &config);
		}
	}
	spin_unlock_irqrestore(&dmadata->dma_lock, flags);
	return ret;
}

/**
 * @NO{S14E01C01U}
 * @ASIL{B}
 * @brief set substream's runtime parameters
 * and alloc a x5_dmadata structure as runtime'sprivate_data
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
static inline int32_t x5_pcm_platform_init_runtime(struct snd_pcm_substream *substream)
{
	int32_t ret = 0;

	ret = hobot_snd_platform_data_init(substream);
	if (ret < 0) {
		dev_err(substream2dev(substream),
				"BUG: Failed to hobot_snd_platform_data_init:%d\n", ret);
		return ret;
	}
	return 0;
}

/**
 * @NO{S14E01C01I}
 * @ASIL{B}
 * @brief Alloc and reset some resourses for startup stage
 * this function is be called by machine driver(soc_pcm_open) after cpudai startup
 *
 * @param[in] component pcm component descriptor
 * @param[in] substream pcm substream descriptor
 *
 * @return 0 for success, < 0 for failure
 *
 * @callgraph
 * @callergraph
 * @data_read NULL
 * @data_updated
 * @design
 */
static int32_t x5_pcm_platform_open(struct snd_soc_component *component,
									struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = NULL;
	struct snd_soc_dai *cpu_dai = NULL;
	struct x5_dmadata *dmadata = NULL;
	int32_t ret = 0;
	char chan_name[32];
	struct device *dma_dev;
	//struct snd_pcm_runtime *runtime = substream->runtime;
	if (substream == NULL) {
		pr_err("BUG: NO usefull args for %s\n", __FUNCTION__);/*PRQA S ALL*/ /*qac-9.7.0-1036,1035,3200*/
		return -EINVAL;
	}
	rtd = (struct snd_soc_pcm_runtime *)substream->private_data;
	cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	ret = x5_pcm_platform_init_runtime(substream);
	if (ret < 0) {
		return ret;
	}
	dmadata = (struct x5_dmadata *)substream->runtime->private_data;
	for (int i = 0; i < EIGHT_CHANNEL_SUPPORT / 2; i++) {
		snprintf(chan_name, sizeof(chan_name), "%s%d", HOBOT_I2S_RX_FILTER, i);
		dmadata->dma_chan[i] = dma_request_slave_channel(cpu_dai->dev,
			(substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ? HOBOT_I2S_TX_FILTER : chan_name);
		if (dmadata->dma_chan[i] == NULL) {
			dev_err(substream2dev(substream), "BUG: Failed to request dma slave channel, %ld\n", PTR_ERR(dmadata->dma_chan[i]));
			devm_kfree(substream->pcm->card->dev, (void *)dmadata);
			dmadata = NULL;
			return -ENXIO;
		}
	}

	dma_dev = dmadata->dma_chan[0]->device->dev;
	hobot_pcm_hardware.period_bytes_max = dma_get_max_seg_size(dma_dev);
	ret = snd_soc_set_runtime_hwparams(substream, &hobot_pcm_hardware);
	if (ret < 0) {
		dev_err(substream2dev(substream),
				"BUG: Failed to snd_soc_set_runtime_hwparams:%d\n", ret);
		devm_kfree(substream->pcm->card->dev, (void *)dmadata);
		dmadata = NULL;
		return ret;
	}

	ret = snd_pcm_hw_constraint_integer(substream->runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0) {
		dev_err(substream2dev(substream),
				"BUG: Failed to snd_pcm_hw_constraint_integer:%d\n", ret);
		devm_kfree(substream->pcm->card->dev, (void *)dmadata);
		dmadata = NULL;
		return ret;
	}

	dmadata->tmp_buf = kzalloc(hobot_pcm_hardware.buffer_bytes_max, GFP_KERNEL);
	dmadata->out_le = kzalloc(hobot_pcm_hardware.buffer_bytes_max, GFP_KERNEL);
	dmadata->out_3le = kzalloc(hobot_pcm_hardware.buffer_bytes_max, GFP_KERNEL);
	if (!dmadata->tmp_buf | !dmadata->out_le | !dmadata->out_3le) {
		dev_err(substream2dev(substream), "BUG: kzalloc buf failed\n");
		devm_kfree(substream->pcm->card->dev, (void *)dmadata);
		dmadata = NULL;
		return -ENOMEM;
	}
	return 0;
}

/**
 * @NO{S14E01C01I}
 * @ASIL{B}
 * @brief Stop and free some resourses for shutdown stage
 * this function is be called by machine driver(soc_pcm_open) after cpudai shutdown
 *
 * @param[in] component pcm component descriptor
 * @param[in] substream pcm substream descriptor
 *
 * @return 0 for success, < 0 for failure
 *
 * @callgraph
 * @callergraph
 * @data_read NULL
 * @data_updated
 * @design
 */
int32_t x5_pcm_platform_close(struct snd_soc_component *component,
							  struct snd_pcm_substream *substream)/*PRQA S ALL*/ /*alsa's API*/
{
	struct x5_dmadata *dmadata;
	struct snd_pcm_runtime *runtime = substream->runtime;
	if (substream == NULL) {
		pr_err("BUG: NO usefull args for x5_pcm_platform_close\n");/*PRQA S ALL*/ /*qac-9.7.0-1036,1035,3200*/
		return -EINVAL;
	}
	dmadata = (struct x5_dmadata *)substream->runtime->private_data;
	if (dmadata == NULL) {
		/*PRQA S ALL*/ /*qac-9.7.0-1036,1035,3200*/
		return -EINVAL;
	}
	for (int i = 0; i < runtime->channels / 2; i++) {
		if (dmadata->dma_chan[i] != NULL) {
			dmaengine_synchronize(dmadata->dma_chan[i]);
		}
	}

	for (int i = 0; i < EIGHT_CHANNEL_SUPPORT / 2; i++) {
		if (dmadata->dma_chan[i] != NULL) {
			dma_release_channel(dmadata->dma_chan[i]);
		}
	}

	if (dmadata->tmp_buf)
		kfree(dmadata->tmp_buf);
	if (dmadata->out_le)
		kfree(dmadata->out_le);
	if (dmadata->out_3le)
		kfree(dmadata->out_3le);
	devm_kfree(substream->pcm->card->dev, (void *)dmadata);
	dmadata = NULL; /*PRQA S ALL*/ /*qac-9.7.0-2983*/
	return 0;
}

/*E1: alsa's api, so no return*/
/**
 * @NO{S14E01C01I}
 * @ASIL{B}
 * @brief free some memory resources at remove module stage
 * this function is be called by machine driver(snd_pcm_free)
 *
 * @param[in] component pcm component descriptor
 * @param[in] pcm pcm descriptor
 *
 * @return None
 *
 * @callgraph
 * @callergraph
 * @data_read NULL
 * @data_updated
 * @design
 */
static void x5_pcm_free(struct snd_soc_component *component,
						struct snd_pcm *pcm)/*PRQA S ALL*/ /*alsa's API*/
{
	hobot_pcm_free_dma_buffers(pcm);
}

/**
 * @NO{S14E01C01I}
 * @ASIL{B}
 * @brief alloc some memory resources at insert module stage
 * this function is be called by machine driver(soc_new_pcm)
 *
 * @param[in] component pcm component descriptor
 * @param[in] rtd pcm runtime descriptor
 *
 * @return 0 for success, < 0 for failure
 *
 * @callgraph
 * @callergraph
 * @data_read NULL
 * @data_updated
 * @design
 */
static int32_t x5_pcm_new(struct snd_soc_component *component,
						  struct snd_soc_pcm_runtime *rtd)/*PRQA S ALL*/ /*alsa's API*/
{
	struct snd_card *card = NULL;
	struct snd_pcm *pcm = NULL;
	int32_t ret = 0;
	struct snd_soc_dai *cpu_dai = NULL;
	if (rtd == NULL) {
		pr_err("BUG: NO usefull args for x5_pcm_new\n");/*PRQA S ALL*/ /*qac-9.7.0-1036,1035,3200*/
		return -EINVAL;
	}
	card = rtd->card->snd_card;
	pcm = rtd->pcm;
	cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	ret = dma_coerce_mask_and_coherent(card->dev, (uint64_t)DMA_BIT_MASK(HOBOT_I2S_DMA_BIT_MASK));/*PRQA S ALL*/
	/*qac-9.7.0-3469,1840,3494,1843*/
	if (ret != 0)
		return ret;
	if (pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream != NULL) {
		ret = hobot_pcm_preallocate_dma_buffer(pcm, SNDRV_PCM_STREAM_PLAYBACK);
		if (ret != 0) {
			x5_pcm_free(component, pcm);
			return ret; /*error printed in subfunc*/
		}
	}
	if (pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream != NULL) {
		ret = hobot_pcm_preallocate_dma_buffer(pcm, SNDRV_PCM_STREAM_CAPTURE);
		if (ret != 0) {
			x5_pcm_free(component, pcm);
			return ret; /*error printed in subfunc*/
		}
	}
	return ret;
}

/**
 * @NO{S14E01C01I}
 * @ASIL{B}
 * @brief reset some resources for next transmission
 *
 * @param[in] component pcm component descriptor
 * @param[in] substream pcm substream descriptor
 *
 * @return 0 for success, < 0 for failure
 *
 * @callgraph
 * @callergraph
 * @data_read NULL
 * @data_updated
 * @design
 */
int32_t x5_pcm_platform_prepare(struct snd_soc_component *component,
		       				struct snd_pcm_substream *substream)
{
	struct x5_dmadata *dmadata = NULL;
	if (substream == NULL || substream->runtime == NULL) {
		pr_err("BUG: NO usefull args for %s\n", __FUNCTION__);/*PRQA S ALL*/ /*qac-9.7.0-1036,1035,3200*/
		return -EINVAL;
	}
	dmadata = (struct x5_dmadata *)substream->runtime->private_data;
	dmadata->dma_xfer_cnt = 0;
	return 0;
}

/**
 * x5_soc_platform - the gobal variable for register operation's function.
 */
static const struct snd_soc_component_driver x5_soc_platform = {
	.pcm_construct = x5_pcm_new,
	.pcm_destruct = x5_pcm_free,
	.open = x5_pcm_platform_open,
	.close = x5_pcm_platform_close,
	.trigger = x5_pcm_platform_trigger,
	.pointer = x5_pcm_platform_pointer,
	.prepare = x5_pcm_platform_prepare,
	.hw_params = x5_pcm_platform_hw_params,
	.copy_user = x5_pcm_platform_copy_usr
};

/**
 * @NO{S14E01C01U}
 * @ASIL{B}
 * @brief register pcm dma driver as a alsa component
 *
 * @param[in] dev device descriptor
 *
 * @return 0 for success, < 0 for failure
 *
 * @callgraph
 * @callergraph
 * @data_read NULL
 * @data_updated
 * @design
 */
int32_t x5_i2s_platform_remove(struct platform_device *pdev)/*PRQA S ALL*/
{
	return 0;
}
EXPORT_SYMBOL_GPL(x5_i2s_platform_remove);/*PRQA S ALL*/

/**
 * @NO{S14E01C01I}
 * @ASIL{B}
 * @brief copy x5_soc_platform
 *
 * @param[in] pdev platform device descriptor
 * @param[out] component alsa component driver descriptor
 *
 * @return 0 for success, < 0 for failure
 *
 * @callgraph
 * @callergraph
 * @data_read NULL
 * @data_updated
 * @design
 */

int32_t x5_i2s_platform_probe(struct platform_device *pdev, struct snd_soc_component_driver *component)
{
	*component = x5_soc_platform;
	return 0;
}
EXPORT_SYMBOL_GPL(x5_i2s_platform_probe);/*PRQA S ALL*/

/* Module information */
// PRQA S ALL ++
MODULE_AUTHOR("zhen01.zhang");
MODULE_DESCRIPTION("X5 platform DMA module");
MODULE_LICENSE("GPL");
// PRQA S ALL --
