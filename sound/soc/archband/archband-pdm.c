// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright(C) 2024, D-Robotics Co., Ltd. All rights reserved
 */

#include <linux/mfd/syscon.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <linux/rcupdate.h>
#include <linux/pm_runtime.h>
#include "archband-pdm.h"

#define PERIOD_BYTES_MIN 4096
#define BUFFER_BYTES_MAX (3 * 2 * 8 * PERIOD_BYTES_MIN)
#define PERIODS_MIN 2

#define ARB_CHAN_MIN 2
#define ARB_CHAN_MAX 8
#define ARB_FIFO_SIZE 16

static const struct snd_pcm_hardware dw_pcm_hardware = {
	.info = SNDRV_PCM_INFO_INTERLEAVED | SNDRV_PCM_INFO_MMAP |
		SNDRV_PCM_INFO_MMAP_VALID | SNDRV_PCM_INFO_BLOCK_TRANSFER,
	.rates = SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_32000 |
		 SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_44100,
	.rate_min = 16000,
	.rate_max = 48000,
	.formats = SNDRV_PCM_FMTBIT_S24_LE,
	.channels_min = ARB_CHAN_MIN,
	.channels_max = ARB_CHAN_MAX,
	.buffer_bytes_max = BUFFER_BYTES_MAX,
	.period_bytes_min = PERIOD_BYTES_MIN,
	.period_bytes_max = BUFFER_BYTES_MAX / PERIODS_MIN,
	.periods_min = PERIODS_MIN,
	.periods_max = BUFFER_BYTES_MAX / PERIOD_BYTES_MIN,
	.fifo_size = ARB_FIFO_SIZE,
};

#define ARCH_HMCLK_11289		11289600
#define ARCH_HMCLK_12288		12288000

static inline void pdm_write_reg(void __iomem *io_base, int reg, u32 val)
{
	writel(val, io_base + reg);
}

static inline u32 pdm_read_reg(void __iomem *io_base, int reg)
{
	return readl(io_base + reg);
}

static void pdm_update_bits(void __iomem *io_base, int reg, u32 mask, u32 val)
{
	u32 tmp;

	tmp = pdm_read_reg(io_base, reg);
	tmp &= ~mask;
	tmp |= (val & mask);
	pdm_write_reg(io_base, reg, tmp);
}

static void clear_irqs(struct arb_pdm_dev *dev, int ch_num)
{
	int i;

	for (i = 0; i < (ch_num >> 1); i++)
		pdm_write_reg(dev->pdm_base, PDM_WRAPPER_CORE_INTR_STS(i), 0xf);
}

static void arb_pdm_disable(struct arb_pdm_dev *dev)
{
	int i = 0;

	pdm_update_bits(dev->pdm_base, PDM_CTRL_CORE_CONF(0),
			PDM_CTRL_CORE_EN_MASK | PDM_CTRL_ACTIVE_MASK,
			PDM_CTRL_CORE_EN_VAL(0) | PDM_CTRL_ACTIVE_VAL(0));

	// default settings
	pdm_write_reg(dev->pdm_base, PDM_CTRL_CORE_CONF(1), 0x80000444);
	pdm_write_reg(dev->pdm_base, PDM_CTRL_CORE_CONF(2), 0x80000444);
	pdm_write_reg(dev->pdm_base, PDM_CTRL_CORE_CONF(3), 0x80000444);

	for (i = 0; i < ARB_CORE_NUMS; i++)
		pdm_write_reg(dev->pdm_base, PDM_CORE_CONF(i), 0x10101b30);
}

static void pdm_start(struct arb_pdm_dev *dev,
		      struct snd_pcm_substream *substream)
{
	int i;
	u32 val = 0;
	int usecase = dev->config.usecase;

	pdm_write_reg(dev->pdm_base, PDM_WRAPPER_PCM_INTR_EN, dev->use_pio ? 0xf : 0x0);
	pdm_write_reg(dev->pdm_base, PDM_WRAPPER_PCM_CHAN_CTRL, 0xff);

	for (i = 0; i < (dev->config.chan_nr >> 1); i++) {
		pdm_update_bits(dev->pdm_base, PDM_CORE_CONF(i),
				PDM_CONF_CHSET_MASK, PDM_CONF_CHSET_VAL(3));
		pdm_write_reg(dev->pdm_base, PDM_WRAPPER_CORE_INTR_EN(i),
			      dev->use_pio ? 0xf : 0x0);
		pdm_write_reg(dev->pdm_base, PDM_WRAPPER_CORE_DMA_CR(i),
			      dev->use_pio ? 0x7 : 0x17);
		val |= BIT(i);
	}

	pdm_update_bits(dev->pdm_base, PDM_CTRL_CORE_CONF(0),
			PDM_CTRL_CORE_EN_MASK | PDM_CTRL_ACTIVE_MASK |
				PDM_CTRL_USE_CASE_MASK,
			PDM_CTRL_CORE_EN_VAL(1) | PDM_CTRL_ACTIVE_VAL(val) |
				PDM_CTRL_USE_CASE_VAL(usecase));
}

static void pdm_stop(struct arb_pdm_dev *dev,
		     struct snd_pcm_substream *substream)
{
	arb_pdm_disable(dev);
	clear_irqs(dev, dev->config.chan_nr);
}

static int arb_pdm_startup(struct snd_pcm_substream *substream,
			   struct snd_soc_dai *cpu_dai)
{
	struct arb_pdm_dev *dev = snd_soc_dai_get_drvdata(cpu_dai);
	union arb_pdm_snd_dma_data *dma_data = NULL;

	dma_data = &dev->capture_dma_data;
	snd_soc_dai_set_dma_data(cpu_dai, substream, (void *)dma_data);
	return 0;
}

static void arb_pdm_shutdown(struct snd_pcm_substream *substream,
			     struct snd_soc_dai *dai)
{
	snd_soc_dai_set_dma_data(dai, substream, NULL);
}

static int arb_pdm_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params,
			     struct snd_soc_dai *dai)
{
	struct arb_pdm_dev *dev = snd_soc_dai_get_drvdata(dai);
	struct pdm_clk_config_data *config = &dev->config;
	int hmclk_rate;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S24_LE:
		config->data_width = 32;
		break;
	default:
		dev_err(dev->dev, "Unsupported PCM fmt");
		return -EINVAL;
	}

	config->chan_nr = params_channels(params);
	switch (config->chan_nr) {
	case EIGHT_CHANNEL_SUPPORT:
	case SIX_CHANNEL_SUPPORT:
	case FOUR_CHANNEL_SUPPORT:
	case TWO_CHANNEL_SUPPORT:
		break;
	default:
		dev_err(dev->dev, "channel not supported\n");
		return -EINVAL;
	}

	config->sample_rate = params_rate(params);
	if (config->sample_rate == 48000)
		config->usecase = dev->osr ? USE_CASE_48K_OSR : USE_CASE_48K;
	else if (config->sample_rate == 32000)
		config->usecase = dev->osr ? USE_CASE_32K_OSR : USE_CASE_32K;
	else if (config->sample_rate == 16000 && config->data_width == 32)
		config->usecase = dev->osr ? USE_CASE_16K_OSR : USE_CASE_16K;
	else if (config->sample_rate == 441000)
		config->usecase = USE_CASE_44P1K_OSR;
	else
		return -EINVAL;

	if (config->sample_rate == 441000)
		hmclk_rate = ARCH_HMCLK_11289;
	else
		hmclk_rate = ARCH_HMCLK_12288;

	return clk_set_rate(dev->clk, hmclk_rate);
}

static int arb_pdm_prepare(struct snd_pcm_substream *substream,
			   struct snd_soc_dai *dai)
{
	struct arb_pdm_dev *dev = snd_soc_dai_get_drvdata(dai);

	arb_pdm_disable(dev);
	return 0;
}

static unsigned int arm_pcm_rx_32(struct arb_pdm_dev *dev,
				  struct snd_pcm_runtime *runtime,
				  unsigned int rx_ptr, bool *period_elapsed)
{
	u32(*p)[2] = (void *)runtime->dma_area;
	unsigned int period_pos = rx_ptr % runtime->period_size;
	int i, j;

	for (i = 0; i < dev->fifo_th >> 2; i++) {
		for (j = 0; j < dev->config.chan_nr >> 1; j++) {
			p[rx_ptr][0] = ioread32(dev->pdm_base +
						PDM_WRAPPER_CORE_DATA(j));
			p[rx_ptr][1] = ioread32(dev->pdm_base +
						PDM_WRAPPER_CORE_DATA(j));
			period_pos++;
			if (++rx_ptr >= runtime->buffer_size)
				rx_ptr = 0;
		}
	}

	*period_elapsed = period_pos >= runtime->period_size;
	return rx_ptr;
}

static void arb_rx_32(struct arb_pdm_dev *dev, int chan_nr)
{
	struct snd_pcm_substream *substream;
	bool active, period_elapsed;

	rcu_read_lock();

	substream = rcu_dereference(dev->rx_substream);
	active = substream && snd_pcm_running(substream);
	if (active) {
		unsigned int ptr;
		unsigned int new_ptr;

		ptr = READ_ONCE(dev->rx_ptr);
		new_ptr = arm_pcm_rx_32(dev, substream->runtime, ptr,
					&period_elapsed);
		cmpxchg(&dev->rx_ptr, ptr, new_ptr);

		if (period_elapsed)
			snd_pcm_period_elapsed(substream);
	}

	rcu_read_unlock();
}

static unsigned int arm_pcm_rx_16(struct arb_pdm_dev *dev,
				  struct snd_pcm_runtime *runtime,
				  unsigned int rx_ptr, bool *period_elapsed)
{
	u16(*p)[2] = (void *)runtime->dma_area;
	unsigned int period_pos = rx_ptr % runtime->period_size;
	int i, j;

	for (i = 0; i < dev->fifo_th >> 2; i++) {
		for (j = 0; j < dev->config.chan_nr >> 1; j++) {
			p[rx_ptr][0] = ioread32(dev->pdm_base +
						PDM_WRAPPER_CORE_DATA(j));
			p[rx_ptr][1] = ioread32(dev->pdm_base +
						PDM_WRAPPER_CORE_DATA(j));
			period_pos++;
			if (++rx_ptr >= runtime->buffer_size)
				rx_ptr = 0;
		}
	}

	*period_elapsed = period_pos >= runtime->period_size;
	return rx_ptr;
}

static void arb_rx_16(struct arb_pdm_dev *dev, int chan_nr)
{
	struct snd_pcm_substream *substream;
	bool active, period_elapsed;

	rcu_read_lock();

	substream = rcu_dereference(dev->rx_substream);
	active = substream && snd_pcm_running(substream);
	if (active) {
		unsigned int ptr;
		unsigned int new_ptr;

		ptr = READ_ONCE(dev->rx_ptr);
		new_ptr = arm_pcm_rx_16(dev, substream->runtime, ptr,
					&period_elapsed);
		cmpxchg(&dev->rx_ptr, ptr, new_ptr);

		if (period_elapsed)
			snd_pcm_period_elapsed(substream);
	}

	rcu_read_unlock();
}

static irqreturn_t arb_pdm_irq_handler(int irq, void *dev_id)
{
	struct arb_pdm_dev *dev = dev_id;
	u32 tmp;

	/* we use first channel status register */
	tmp = pdm_read_reg(dev->pdm_base, PDM_WRAPPER_CORE_INTR_STS(0));
	clear_irqs(dev, dev->config.chan_nr);

	if ((tmp & BIT(0)) == BIT(0)) {
		if (dev->config.data_width == 32)
			arb_rx_32(dev, dev->config.chan_nr);
		else if (dev->config.data_width == 16)
			arb_rx_16(dev, dev->config.chan_nr);
	}

	return IRQ_HANDLED;
}

static int arb_pdm_trigger(struct snd_pcm_substream *substream, int cmd,
			   struct snd_soc_dai *dai)
{
	struct arb_pdm_dev *dev = snd_soc_dai_get_drvdata(dai);
	int ret = 0;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		pdm_start(dev, substream);
		break;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		pdm_stop(dev, substream);
		break;
	default:
		dev_err(dai->dev, "Unknown trigger command\n");
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int arb_pdm_set_fmt(struct snd_soc_dai *cpu_dai, unsigned int fmt)
{
	int ret = 0;

	switch (fmt & SND_SOC_DAIFMT_CLOCK_PROVIDER_MASK) {
	case SND_SOC_DAIFMT_BP_FP:
		ret = 0;
		break;
	case SND_SOC_DAIFMT_BC_FC:
	case SND_SOC_DAIFMT_BC_FP:
	case SND_SOC_DAIFMT_BP_FC:
	default:
		dev_err(cpu_dai->dev, "Unsupported dai format\n");
		ret = -EINVAL;
		break;
	}
	return ret;
}

#ifdef CONFIG_PM
static int arb_pdm_runtime_suspend(struct device *dev)
{
	struct arb_pdm_dev *dw_dev = dev_get_drvdata(dev);

	clk_disable(dw_dev->clk);
	return 0;
}

static int arb_pdm_runtime_resume(struct device *dev)
{
	struct arb_pdm_dev *dw_dev = dev_get_drvdata(dev);
	int ret;

	ret = clk_enable(dw_dev->clk);
	if (ret)
		return ret;
	return 0;
}

static int arb_pdm_suspend(struct snd_soc_component *component)
{
	struct arb_pdm_dev *dev = snd_soc_component_get_drvdata(component);

	clk_disable(dev->pclk);
	clk_disable(dev->clk);
	return 0;
}

static int arb_pdm_resume(struct snd_soc_component *component)
{
	struct arb_pdm_dev *dev = snd_soc_component_get_drvdata(component);
	int ret;

	ret = clk_enable(dev->pclk);
	if (ret)
		return ret;
	ret = clk_enable(dev->clk);
	if (ret)
		return ret;

	return 0;
}

#else
#define arb_pdm_suspend	NULL
#define arb_pdm_resume	NULL
#endif

static int arb_dmaengine_pcm_open(struct snd_soc_component *component,
				  struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	struct arb_pdm_dev *dev = snd_soc_dai_get_drvdata(cpu_dai);
	union arb_pdm_snd_dma_data *dma_data = NULL;

	dma_data = &dev->capture_dma_data;
	snd_soc_dai_set_dma_data(cpu_dai, substream, (void *)dma_data);
	return 0;
}

static int arb_set_dai_sysclk(struct snd_soc_dai *dai, int clk_id,
			      unsigned int freq, int dir)
{
	struct arb_pdm_dev *dev = snd_soc_dai_get_drvdata(dai);

	dev->config.hmclk_rate = freq;
	return 0;
}

static const struct snd_soc_dai_ops arb_pdm_dai_ops = {
	.startup = arb_pdm_startup,
	.shutdown = arb_pdm_shutdown,
	.set_sysclk = arb_set_dai_sysclk,
	.prepare = arb_pdm_prepare,
	.hw_params = arb_pdm_hw_params,
	.trigger = arb_pdm_trigger,
	.set_fmt = arb_pdm_set_fmt,
};

static const struct snd_soc_component_driver arb_pdm_component = {
	.name = "archband-pdm",
	.suspend = arb_pdm_suspend,
	.resume = arb_pdm_resume,
	.open = arb_dmaengine_pcm_open,
	.legacy_dai_naming = 1,
};

static int arb_configure_dai_by_dt(struct arb_pdm_dev *dev,
				   struct resource *res)
{
	dev->fifo_th = ARB_FIFO_SIZE;
	dev->capture_dma_data.dt.addr = res->start + PDM_WRAPPER_CORE_DATA(0);
	dev->capture_dma_data.dt.addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	dev->capture_dma_data.dt.fifo_size = 16;
	dev->capture_dma_data.dt.maxburst = 16;

	return 0;
}

static int arb_pcm_open(struct snd_soc_component *component,
			struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct arb_pdm_dev *dev =
		snd_soc_dai_get_drvdata(asoc_rtd_to_cpu(rtd, 0));

	snd_soc_set_runtime_hwparams(substream, &dw_pcm_hardware);
	snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);
	runtime->private_data = dev;

	return 0;
}

static int arb_pcm_close(struct snd_soc_component *component,
			 struct snd_pcm_substream *substream)
{
	synchronize_rcu();
	return 0;
}

static int arb_pcm_hw_params(struct snd_soc_component *component,
			     struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *hw_params)
{
	return 0;
}

static int arb_pcm_trigger(struct snd_soc_component *component,
			   struct snd_pcm_substream *substream, int cmd)
{
	struct snd_soc_pcm_runtime *rtd = asoc_substream_to_rtd(substream);
	struct arb_pdm_dev *dev =
		snd_soc_dai_get_drvdata(asoc_rtd_to_cpu(rtd, 0));
	int ret = 0;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			WRITE_ONCE(dev->rx_ptr, 0);
			rcu_assign_pointer(dev->rx_substream, substream);
		}
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
			rcu_assign_pointer(dev->rx_substream, NULL);
		else
			ret = -EINVAL;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static snd_pcm_uframes_t arb_pcm_pointer(struct snd_soc_component *component,
					 struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct arb_pdm_dev *dev = runtime->private_data;
	snd_pcm_uframes_t pos;

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		pos = READ_ONCE(dev->rx_ptr);
	else
		return -EINVAL;

	return pos < runtime->buffer_size ? pos : 0;
}

static int arb_pcm_new(struct snd_soc_component *component,
		       struct snd_soc_pcm_runtime *rtd)
{
	size_t size = dw_pcm_hardware.buffer_bytes_max;

	snd_pcm_set_managed_buffer_all(rtd->pcm, SNDRV_DMA_TYPE_CONTINUOUS,
				       NULL, size, size);
	return 0;
}

static const struct snd_soc_component_driver ah_pcm_component = {
	.open = arb_pcm_open,
	.close = arb_pcm_close,
	.hw_params = arb_pcm_hw_params,
	.trigger = arb_pcm_trigger,
	.pointer = arb_pcm_pointer,
	.pcm_construct = arb_pcm_new,
};

static struct snd_soc_dai_driver arb_dai_driver = {
	.name = "archband-pdm-dai",
	.ops = &arb_pdm_dai_ops,
	.capture.channels_min = ARB_CHAN_MIN,
	.capture.channels_max = ARB_CHAN_MAX,
	.capture.formats = (SNDRV_PCM_FMTBIT_S24_LE),
	.capture.rates = SNDRV_PCM_RATE_16000 | SNDRV_PCM_RATE_32000 |
			 SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_44100,
};

static const struct dev_pm_ops arb_pm_ops = {
	SET_RUNTIME_PM_OPS(arb_pdm_runtime_suspend, arb_pdm_runtime_resume, NULL)
};

static int arb_pclk_en(struct arb_pdm_dev *dev)
{
	dev->pclk = devm_clk_get(dev->dev, "pdm_pclk");

	if (IS_ERR(dev->pclk))
		return PTR_ERR(dev->pclk);

	return clk_prepare_enable(dev->pclk);
}

static int arb_pdm_probe(struct platform_device *pdev)
{
	struct arb_pdm_dev *dev;
	struct resource *res;
	struct device_node *np = pdev->dev.of_node;
	struct snd_soc_dai_driver *arb_pdm_dai = &arb_dai_driver;
	int ret, irq;
	u32 tmp;

	dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	dev->pdm_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(dev->pdm_base))
		return PTR_ERR(dev->pdm_base);

	dev->ctrl_base_reg = syscon_regmap_lookup_by_phandle(np, "arb-syscon");
	if (IS_ERR(dev->ctrl_base_reg))
		ret = PTR_ERR(dev->ctrl_base_reg);
	else
		ret = of_property_read_u32_index(np, "arb-syscon", 1,
						 &dev->config_reg);

	if (ret) {
		dev_err(&pdev->dev, "Failed to get DSP CRM system control\n");
		return ret;
	}

	ret = device_property_read_u32(&pdev->dev, "arb,osr", &tmp);
	if (!ret)
		dev->osr = !!tmp;

	dev->core_nums = ARB_CORE_NUMS;
	dev->dev = &pdev->dev;

	ret = arb_pclk_en(dev);
	if (ret) {
		dev_err(&pdev->dev, "failed to get pdm pclk\n");
		return ret;
	}

	ret = arb_configure_dai_by_dt(dev, res);
	if (ret < 0)
		return ret;

	dev->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(dev->clk))
		return PTR_ERR(dev->clk);

	ret = clk_prepare_enable(dev->clk);
	if (ret < 0)
		return ret;

	dev_set_drvdata(&pdev->dev, dev);
	ret = devm_snd_soc_register_component(&pdev->dev, &arb_pdm_component,
					      arb_pdm_dai, 1);
	if (ret != 0) {
		dev_err(&pdev->dev, "not able to register dai\n");
		return ret;
	}

	irq = platform_get_irq_optional(pdev, 0);
	if (irq >= 0) {
		ret = devm_request_irq(&pdev->dev, irq, arb_pdm_irq_handler, 0,
				       pdev->name, dev);
		if (ret < 0) {
			dev_err(&pdev->dev, "failed to request irq\n");
			return ret;
		}
		dev->use_pio = true;

		ret = devm_snd_soc_register_component(
			&pdev->dev, &ah_pcm_component, NULL, 0);
		if (ret != 0) {
			dev_err(&pdev->dev,
				"not able to devm_snd_soc_register_component dai\n");
			return ret;
		}
	} else {
		dev->use_pio = false;
		ret = devm_snd_dmaengine_pcm_register(&pdev->dev, NULL, 0);
	}

	arb_pdm_disable(dev);
	pm_runtime_enable(&pdev->dev);

	return 0;
}

static int arb_pdm_remove(struct platform_device *pdev)
{
	struct arb_pdm_dev *dw_dev = dev_get_drvdata(&pdev->dev);

	clk_disable_unprepare(dw_dev->pclk);
	pm_runtime_disable(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id arb_pdm_of_match[] = {
	{
		.compatible = "archband,pdm-driver",
	},
	{},
};

MODULE_DEVICE_TABLE(of, arb_pdm_of_match);
#endif

static struct platform_driver arb_pdm_driver = {
	.probe		= arb_pdm_probe,
	.remove		= arb_pdm_remove,
	.driver		= {
		.name	= "archband-pdm",
		.of_match_table = of_match_ptr(arb_pdm_of_match),
		.pm = &arb_pm_ops,
	},
};

module_platform_driver(arb_pdm_driver);
MODULE_DESCRIPTION("ASoC archband pdm driver");
MODULE_LICENSE("GPL");
