/***
 *                     COPYRIGHT NOTICE
 *            Copyright 2024 D-Robotics, Inc.
 *                   All rights reserved.
 ***/
#ifndef SOUND_SOC_HOBOT_HOBOT_I2S_PLATFORM_H_
#define SOUND_SOC_HOBOT_HOBOT_I2S_PLATFORM_H_

#include <linux/module.h>
#include <linux/dmaengine.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <linux/dma-mapping.h>
#include <sound/dmaengine_pcm.h>

#define	I2S_STATE_STOP 	(0x0)
#define	I2S_STATE_PAUSE 	(0x1)
#define	I2S_STATE_START 	(0x2)
#define I2S_STATE_TX_MASK ((1 << STAT_FIFO_EMPTY) || (1 << STAT_FIFO_AEMPTY))
#define I2S_STATE_RX_MASK ((1 << STAT_FIFO_FULL) || (1 << STAT_FIFO_AEMPTY) || \
							(1 << STAT_RFIFO_FULL) || (1 << STAT_RFIFO_AFULL))

#define HOBOT_I2S_PERIOD_BYTES_MIN (16u *8u) //16ms * 8KHz * 8bits * 1channel
#define HOBOT_I2S_PERIOD_BYTES_MAX (48u * 96u * 4u * 16u) //48ms * 96KHz * 32bits * 16channel
#define HOBOT_I2S_PERIOD_COUNT_MIN (2u)
#define HOBOT_I2S_PERIOD_COUNT_MAX (8u)
#define HOBOT_I2S_BUFFER_BYTES_MAX (128 * 1024)
//2.25MBytes dma will alloc 2 * HOBOT_I2S_BUFFER_BYTES_MAX

#define TSTAMP_MASK (0x3)

#define I2S_PROCESS_MAX	(5)
#define HOBOT_SIGI_I2S_NUM	(2)

extern const struct snd_pcm_hardware hobot_pcm_hardware;

#define HOBOT_I2S_BITS_TO_BYTES (3)
#define HOBOT_PDMA_ALIGN (8)
#define HOBOT_I2S_RX_FILTER "rx"
#define HOBOT_I2S_TX_FILTER "tx"

#define HOBOT_I2S_DMA_BIT_MASK  (32u)
#define HOBOT_I2S_BITS_TO_BYTES (3)

typedef enum x5_i2s_width {
        HOBOT_I2S_WIDTH_8_BIT = 1,
        HOBOT_I2S_WIDTH_16_BIT,
        HOBOT_I2S_WIDTH_24_BIT,
        HOBOT_I2S_WIDTH_32_BIT
} x5_i2s_width;

/**
 * @struct x5_dmadata
 * substream runtime's private data structure
 */
struct x5_dmadata {
	struct dma_chan *dma_chan[4];
	struct tasklet_struct tasklet;
	struct device *dev;
	spinlock_t dma_lock;
	uint32_t slave_id;
	uint32_t word_width;
	uint32_t addr_width;
	int32_t stream; /* SNDRV_PCM_STREAM_PLAYBACK or SNDRV_PCM_STREAM_CAPTURE */
	struct snd_pcm_substream *substream;
	void __iomem *regs;
	dma_addr_t dma_addr;
	uint32_t dma_xfer_cnt;
	snd_pcm_uframes_t buffer_size;
	uint32_t periods;
	snd_pcm_uframes_t period_size;
	int32_t frags;
	struct dma_slave_config config;
	int32_t channel;
	int64_t done_periods;
	snd_pcm_format_t format;

	char *tmp_buf;
	char *out_le;
	char *out_3le;
};

/**
 * @NO{S14E01C01U}
 * @ASIL{B}
 * @brief get device descriptor form substream structure
 *
 * @param[in] substream pcm substream descriptor
 *
 * @return device descriptor
 *
 * @callgraph
 * @callergraph
 * @data_read
 * @data_updated i2s->ms_mode
 * @design
 */
static inline struct device * substream2dev(const struct snd_pcm_substream *substream)
{
	return substream->pcm->card->dev;
}

extern int32_t hobot_snd_platform_dma_start(const struct x5_dmadata *dmadata);
extern int32_t hobot_snd_platform_dma_stop(struct x5_dmadata *dmadata);
extern int32_t hobot_snd_platform_data_init(const struct snd_pcm_substream *substream);
extern int32_t hobot_pcm_preallocate_dma_buffer(const struct snd_pcm *pcm, const int32_t stream);
extern void hobot_pcm_free_dma_buffers(const struct snd_pcm *pcm);
extern int32_t x5_i2s_platform_probe(struct platform_device *pdev, struct snd_soc_component_driver *component);
extern int32_t x5_i2s_platform_remove(struct platform_device *pdev);

#endif //SOUND_SOC_HOBOT_HOBOT_I2S_PLATFORM_H_
