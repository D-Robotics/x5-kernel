// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Driver for GUC IGAADCV04A ADC
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/clk.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/reset.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/iio/buffer.h>
#include <linux/iio/iio.h>
#include <linux/iio/iio-opaque.h>
#include <linux/iio/trigger.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/nvmem-consumer.h>

/* IP Top Registers */
#define GUC_TOP_PWD_CTRL    0x0000
#define GUC_TOP_PWD_STS	    0x0004
#define GUC_TOP_PR_CTRL	    0x0008
#define GUC_TOP_ANA_CTRL0   0x0010
#define GUC_TOP_ANA_CTRL1   0x0014
#define GUC_TOP_ANA_CTRL2   0x0018
#define GUC_TOP_ANA_CTRL3   0x001c
#define GUC_TOP_ANA_CTRL4   0x0024
#define GUC_TOP_ANA_CTRL5   0x0028
#define GUC_TOP_ANA_STS0    0x0040
#define GUC_TOP_ANA_STS1    0x0048
#define GUC_TOP_ANA_STS2    0x004c
#define GUC_TOP_ANA_RSV_OUT 0x0058
#define GUC_TOP_DIG_CTRL0   0x0400
#define GUC_TOP_ANA_CTRL6   0x0468
#define GUC_TOP_ANA_CTRL7   0x0474
#define GUC_TOP_ANA_STS3    0x0488
#define GUC_TOP_ANA_STS4    0x048c
#define GUC_TOP_ANA_STS5    0x0490
#define GUC_TOP_ANA_STS6    0x049c

/* ADC Controller Registers */
#define GUC_CTRL_TOP_CTRL0  0x0804
#define TRIM_MASK	    GENMASK(3, 0)
#define GUC_CTRL_TOP_CTRL1  0x0808
#define GUC_CTRL_TOP_CTRL2  0x0820
#define GUC_CTRL_TOP_STS0   0x0824
#define GUC_NOR_OVERFLOW    BIT(3)
#define GUC_NOR_FULL	    BIT(2)
#define GUC_NOR_HALFFULL    BIT(1)
#define GUC_NOR_EMPTY	    BIT(0)
#define GUC_CTRL_TOP_STS1   0x0828
#define GUC_CTRL_TOP_CTRL3  0x082c
#define GUC_CTRL_TOP_CTRL4  0x0830
#define GUC_CTRL_TOP_CTRL5  0x0840
#define GUC_CTRL_TOP_CTRL6  0x0844
#define GUC_CTRL_TOP_CTRL7  0x0848
#define GUC_CTRL_TOP_CTRL8  0x084c
#define GUC_CTRL_TOP_CTRL9  0x0850
#define GUC_CTRL_TOP_CTRL10 0x085c
#define GUC_CTRL_TOP_CTRL11 0x0858
#define GUC_CTRL_TOP_CTRL12 0x085c
#define GUC_CTRL_TOP_CTRL13 0x0860
#define GUC_CTRL_TOP_CTRL14 0x0864
#define GUC_CTRL_TOP_CTRL15 0x0868
#define GUC_CTRL_TOP_CTRL16 0x086c
#define H_THR_OFFSET	    18
#define L_THR_OFFSET	    2

#define GUC_CTRL_TOP_STS2      0x0900
#define SAMPLE_MASK	       GENMASK(11, 2)
#define SAMPLE_OFFSET	       2
#define GUC_CTRL_TOP_OFFSET    2
#define GUC_CTRL_TOP_STS3      0x0904
#define GUC_CTRL_TOP_STS4      0x0908
#define GUC_CTRL_TOP_STS5      0x090c
#define GUC_CTRL_TOP_STS6      0x0910
#define GUC_CTRL_TOP_STS7      0x0914
#define GUC_CTRL_TOP_STS8      0x0918
#define GUC_CTRL_TOP_STS9      0x091c
#define GUC_CTRL_TOP_STS10     0x0920
#define GUC_CTRL_TOP_STS11     0x0924
#define GUC_CTRL_TOP_STS12     0x0928
#define GUC_CTRL_TOP_STS13     0x092c
#define GUC_CTRL_TOP_CTRL17    0x09c0
#define GUC_CTRL_TOP_CTRL18    0x09c4
#define GUC_CTRL_TOP_STS14     0x09c8
#define GUC_CTRL_NOR_CTRL0     0x0a00
#define GUC_CTRL_NOR_CTRL1     0x0a04
#define GUC_CTRL_NOR_CTRL2     0x0a08
#define GUC_CTRL_NOR_CTRL3     0x0a14
#define GUC_CTRL_NOR_CTRL4     0x0a18
#define GUC_CTRL_NOR_CTRL5     0x0a20
#define GUC_CTRL_NOR_STS0      0x0a24
#define GUC_NOR_FIFO_OVERFLOW  BIT(3)
#define GUC_NOR_FIFO_FULL      BIT(2)
#define GUC_NOR_FIFO_HALF_FULL BIT(1)
#define GUC_NOR_FIFO_EMPTY     BIT(0)

#define GUC_CTRL_NOR_CTRL6	      0x0a28
#define GUC_CTRL_NOR_STS1	      0x0a2c
#define GUC_CTRL_NOR_CTRL7	      0x0a30
#define GUC_NOR_SAMPLE_ERR_INT_EN     BIT(5)
#define GUC_NOR_FIFO_OVERFLOW_INT_EN  BIT(4)
#define GUC_NOR_FIFO_FULL_INT_EN      BIT(3)
#define GUC_NOR_FIFO_HALF_FULL_INT_EN BIT(2)
#define GUC_NOR_FIFO_NOT_EMPTY_INT_EN BIT(1)
#define GUC_NOR_SAMPLE_DONE_INT_EN    BIT(0)
#define GUC_NOR_INT_MASK	      GENMASK(5, 0)

#define GUC_CTRL_NOR_STS2	   0x0a34
#define GUC_NOR_INT		   BIT(31)
#define GUC_NOR_SAMPLE_ERR_INT	   BIT(5)
#define GUC_NOR_FIFO_OVERFLOW_INT  BIT(4)
#define GUC_NOR_FIFO_FULL_INT	   BIT(3)
#define GUC_NOR_FIFO_HALF_FULL_INT BIT(2)
#define GUC_NOR_FIFO_NOT_EMPTY_INT BIT(1)
#define GUC_NOR_SAMPLE_DONE_INT	   BIT(0)

#define GUC_CTRL_NOR_STS3	       0x0a38
#define GUC_CTRL_NOR_CTRL8	       0x0a3c
#define GUC_CTRL_NOR_CTRL9	       0x0a40
#define GUC_NOR_FIFO_FULL_DMA_REQ      BIT(3)
#define GUC_NOR_FIFO_HALF_FULL_DMA_REQ BIT(2)
#define GUC_NOR_FIFO_NOT_EMPTY_DMA_REQ BIT(1)
#define GUC_NOR_SAMPLE_DONE_DMA_REQ    BIT(0)

#define GUC_CTRL_NOR_STS4  0x0a80
#define GUC_NOR_SAMPLE_ERR BIT(0)

#define GUC_CTRL_NOR_STS5 0x0a84
#define EXPECTED_CHS_MASK GENMASK(4, 0)

#define GUC_CTRL_NOR_STS6 0x0a88
#define ACTUAL_CHS_MASK	  GENMASK(4, 0)

#define GUC_CTRL_NOR_STS7 0x0a8c
#define ACTUAL_DATA_MASK  GENMASK(11, 2)

#define GUC_CTRL_INJ_CTRL0 0x0b00
#define GUC_CTRL_INJ_CTRL1 0x0b04
#define GUC_CTRL_INJ_CTRL2 0x0b08
#define GUC_CTRL_INJ_CTRL3 0x0b14
#define GUC_CTRL_INJ_CTRL4 0x0b20
#define GUC_CTRL_INJ_STS0  0x0b24
#define GUC_CTRL_INJ_CTRL5 0x0b28
#define GUC_CTRL_INJ_STS1  0x0b2c
#define GUC_CTRL_INJ_CTRL6 0x0b30
#define GUC_CTRL_INJ_STS2  0x0b34
#define GUC_CTRL_INJ_STS3  0x0b38
#define GUC_CTRL_INJ_CTRL7 0x0b3c
#define GUC_CTRL_INJ_CTRL8 0x0b40
#define GUC_CTRL_INJ_STS4  0x0b80
#define GUC_CTRL_INJ_STS5  0x0b84
#define GUC_CTRL_INJ_STS6  0x0b88
#define GUC_CTRL_INJ_STS7  0x0b8c

#define IGA_ADC_MAX_CHANNELS 8
#define GUC_ADC_SLEEP_US     1000
#define GUC_ADC_TIMEOUT_US   10000

/* adc calibration information in eFuse */
#define IGAV04_ADC_EFUSE_BYTES	      6
#define IGAV04_ADC_EFUSE_CALIB_MASK   GENMASK(27, 22)
#define IGAV04_ADC_EFUSE_CALIB_OFFSET 22
#define IGAV04_ADC_EFUSE_TRIM_MASK    GENMASK(31, 28)
#define IGAV04_ADC_EFUSE_TRIM_OFFSET  28

#define IGAV04_ADC_EFUSE_NEG_FLAG BIT(5)
#define IGAV04_ADC_EFUSE_NEG_MASK GENMASK(5, 0)

struct guc_adc_data {
	struct device *dev;
	const struct iio_chan_spec *channels;
	int num_channels;
};

enum convert_mode {
	SCAN = 0,
	SINGLE,
};

struct guc_adc {
	void __iomem *regs;
	void __iomem *passwd_reg;
	struct clk *clk;
	struct clk *pclk;
	struct completion completion;
	struct regulator *vref;
	int uv_vref;
	int passwd;
	const struct guc_adc_data *data;
	u32 last_val;
	const struct iio_chan_spec *work_chan;
	struct notifier_block nb;
	int32_t calibration_offset;
	u32 trimming_value;
	enum convert_mode guc_convert_mode;
	struct reset_control *reset;
	u32 irq;
	u16 *sample_buffer;
	u32 sample_nums;
	u32 sample_rate;
};

#define GUC_ADC_CHANNEL(_index, _id, _res)                                                   \
	{                                                                                    \
		.type = IIO_VOLTAGE, .indexed = 1, .channel = _index,                        \
		.info_mask_separate	  = BIT(IIO_CHAN_INFO_RAW),                          \
		.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE), .datasheet_name = _id, \
		.scan_index = _index,                                                        \
		.scan_type  = {                                                              \
			 .sign	      = 'u',                                                 \
			 .realbits    = _res,                                                \
			 .storagebits = 16,                                                  \
			 .endianness  = IIO_CPU,                                             \
		 },                                                                          \
	}

static const struct iio_chan_spec igav04a_adc_iio_channels[] = {
	GUC_ADC_CHANNEL(0, "adc0", 10), GUC_ADC_CHANNEL(1, "adc1", 10),
	GUC_ADC_CHANNEL(2, "adc2", 10), GUC_ADC_CHANNEL(3, "adc3", 10),
	GUC_ADC_CHANNEL(4, "adc4", 10), GUC_ADC_CHANNEL(5, "adc5", 10),
	GUC_ADC_CHANNEL(6, "adc6", 10), GUC_ADC_CHANNEL(7, "adc7", 10),
};

static const struct guc_adc_data igav04a_adc_data = {
	.channels     = igav04a_adc_iio_channels,
	.num_channels = ARRAY_SIZE(igav04a_adc_iio_channels),
};

static const struct of_device_id guc_adc_match[] = {
	{
		.compatible = "guc,igav04a",
		.data	    = &igav04a_adc_data,
	},
	{},
};
MODULE_DEVICE_TABLE(of, guc_adc_match);

/*
 * Abort sample when sample error.
 */
static inline void guc_adc_error_abort(struct guc_adc *info, bool abort)
{
	writel_relaxed(abort ? 1 : 0, info->regs + GUC_CTRL_TOP_CTRL18);
}

/*
 * Enable sample channel(n).
 */
static void guc_adc_nor_channel_enable(struct guc_adc *info, struct iio_chan_spec const *chan,
				       bool enable)
{
	u32 flag, channel = chan->channel;

	flag = readl_relaxed(info->regs + GUC_CTRL_NOR_CTRL3);
	if (enable)
		flag |= BIT(channel);
	else
		flag &= ~BIT(channel);
	writel_relaxed(flag, info->regs + GUC_CTRL_NOR_CTRL3);
}

/*
 * Sample mode choose: 0:scan 1:single.
 */
static void guc_adc_nor_mode(struct guc_adc *info)
{
	u32 flag;

	flag = readl_relaxed(info->regs + GUC_CTRL_NOR_CTRL4);

	if (info->guc_convert_mode == SINGLE)
		flag |= BIT(0);
	else
		flag &= ~BIT(0); /* scan mode */

	writel_relaxed(flag, info->regs + GUC_CTRL_NOR_CTRL4);
}

/*
 * Normal FIFO read enable.
 */
static inline void guc_adc_fifo_read_enable(struct guc_adc *info, bool enable)
{
	u32 val;

	val = readl_relaxed(info->regs + GUC_CTRL_NOR_CTRL6);

	if (enable)
		val |= BIT(0);
	else
		val &= ~BIT(0);

	writel_relaxed(val, info->regs + GUC_CTRL_NOR_CTRL6);
}

static void guc_adc_nor_fifo_read(struct guc_adc *info)
{
	u32 val;

	val = readl_relaxed(info->regs + GUC_CTRL_NOR_STS1) & SAMPLE_MASK;

	info->last_val = (val >> SAMPLE_OFFSET);
	info->last_val -= info->calibration_offset;
}

/*
 * Enable normal ADC controller from sw reg.
 */
static inline void guc_adc_nor_start(struct guc_adc *info, bool enable)
{
	writel_relaxed(enable ? 0x01u : 0x00u, info->regs + GUC_CTRL_NOR_CTRL2);
}

/*
 * Trigger conversion.
 */
static int guc_adc_conversion(struct guc_adc *info, struct iio_chan_spec const *chan)
{
	int ret = 0;
	unsigned long tmflag;

	reinit_completion(&info->completion);
	guc_adc_nor_channel_enable(info, chan, true);
	guc_adc_nor_start(info, true);

	tmflag = wait_for_completion_timeout(&info->completion, msecs_to_jiffies(GUC_ADC_TIMEOUT_US));
	if (!tmflag) {
		ret = -ETIMEDOUT;
	}

	guc_adc_nor_channel_enable(info, chan, false);
	guc_adc_nor_start(info, false); /* timeout or normal, adc should be stopped */
	return ret;
}

static void guc_adc_nor_fifo_reset(struct guc_adc *info)
{
	u32 val;

	/* assert reset signal */
	val = readl_relaxed(info->regs + GUC_CTRL_NOR_CTRL5);
	val &= ~BIT(0);
	writel_relaxed(val, info->regs + GUC_CTRL_NOR_CTRL5);
	/* de-assert reset signal */
	val |= BIT(0);
	writel_relaxed(val, info->regs + GUC_CTRL_NOR_CTRL5);
}

/*
 * Read raw data.
 */
static int guc_adc_read_raw(struct iio_dev *indio_dev, struct iio_chan_spec const *chan, int *val,
			    int *val2, long mask)
{
	int ret = -EINVAL;
	struct guc_adc *info = iio_priv(indio_dev);

	if (mask == IIO_CHAN_INFO_RAW) {
		ret = -EBUSY;
		if (!iio_buffer_enabled(indio_dev)) {
			mutex_lock(&indio_dev->mlock);
			guc_adc_nor_fifo_reset(info);
			ret = guc_adc_conversion(info, chan);
			if (!ret) {
				*val = info->last_val;
				ret  = IIO_VAL_INT;
			}
			mutex_unlock(&indio_dev->mlock);
		}
	} else if (mask == IIO_CHAN_INFO_SCALE) {
		*val  = info->uv_vref / 1000;
		*val2 = chan->scan_type.realbits;
		ret   = IIO_VAL_FRACTIONAL_LOG2;
	}
	return ret;
}

static const struct iio_info guc_adc_iio_info = {
	.read_raw = guc_adc_read_raw,
};

/*
 * ADC normal mode interrupt type enable.
 */
static void guc_adc_nor_irq_enable(struct guc_adc *info, bool enable)
{
	u32 val;
	val = readl_relaxed(info->regs + GUC_CTRL_NOR_CTRL7);
	if (enable) {
		val |= (GUC_NOR_SAMPLE_DONE_INT_EN);
	} else {
		val &= ~(GUC_NOR_SAMPLE_DONE_INT_EN);
	}
	writel_relaxed(val, info->regs + GUC_CTRL_NOR_CTRL7);
}

/*
 * Normal mode irq handler, read sample data from FIFO.
 */
static irqreturn_t guc_adc_isr(int irq, void *dev_id)
{
	return IRQ_WAKE_THREAD;
}

/* get the adc nvmem cell data */
static int adc_nvmem_cell_data(struct platform_device *pdev, const char *cell_name, u32 *val)
{
	void *buf;
	size_t len;
	struct nvmem_cell *cell;

	if (!pdev)
		return -EINVAL;

	cell = nvmem_cell_get(&pdev->dev, cell_name);
	if (IS_ERR(cell))
		return PTR_ERR(cell);

	buf = nvmem_cell_read(cell, &len);
	if (IS_ERR(buf)) {
		nvmem_cell_put(cell);
		return PTR_ERR(buf);
	}

	memcpy(val, buf, min(len, sizeof(u32)));
	kfree(buf);
	nvmem_cell_put(cell);

	return 0;
}

/*
 * Move data read from FIFO to buffer.
 */
static irqreturn_t guc_adc_trigger_handler(int irq, void *p)
{
	return IRQ_HANDLED;
}

/*
 * Move data read from FIFO to buffer.
 */
static irqreturn_t guc_adc_thread_irq(int irq, void *dev_id)
{
	u32 i, val;
	struct iio_dev *indio_dev = dev_id;
	struct guc_adc *info	  = iio_priv(indio_dev);
	val = readl_relaxed(info->regs + GUC_CTRL_NOR_STS2);
	if (val & GUC_NOR_SAMPLE_DONE_INT) {
		if (iio_buffer_enabled(indio_dev) && indio_dev->active_scan_mask) {
			for_each_set_bit (i, indio_dev->active_scan_mask, indio_dev->masklength) {
				info->sample_buffer[info->sample_nums++] =
					((readl_relaxed(info->regs + GUC_CTRL_TOP_STS2 + i * 4)) >> 2);
			}

			iio_push_to_buffers(indio_dev, info->sample_buffer);
			iio_trigger_notify_done(indio_dev->trig);
			info->sample_nums = 0;
		} else {
			guc_adc_fifo_read_enable(info, true);
			guc_adc_nor_fifo_read(info);
			guc_adc_fifo_read_enable(info, false);
			complete(&info->completion);
		}
	}
	writel_relaxed(0x01u, info->regs + GUC_CTRL_NOR_CTRL8);
	return IRQ_HANDLED;
}

static void guc_adc_regulator_disable(void *data)
{
	regulator_disable(((struct guc_adc *)data)->vref);
}

static void guc_adc_clk_disable(void *data)
{
	struct guc_adc *info = data;

	clk_disable_unprepare(info->clk);
	clk_disable_unprepare(info->pclk);
}

static int guc_adc_volt_notify(struct notifier_block *nb, unsigned long event, void *data)
{
	struct guc_adc *info = container_of(nb, struct guc_adc, nb);

	if (event & REGULATOR_EVENT_VOLTAGE_CHANGE) {
		info->uv_vref = (unsigned long)data;
	}
	return NOTIFY_OK;
}

static void guc_adc_regulator_unreg_notifier(void *data)
{
	struct guc_adc *info = data;

	regulator_unregister_notifier(info->vref, &info->nb);
}

static int guc_adc_hw_init(struct guc_adc *info)
{
	int ret = -1;
	u32 val;

	writel_relaxed(info->passwd, info->passwd_reg);
	writel_relaxed(info->passwd, info->regs + GUC_TOP_PWD_CTRL);
	val = (readl_relaxed(info->regs + GUC_TOP_PWD_STS) & BIT(0));
	if (!val) { /* password check okay */
		ret = 0;
		writel_relaxed(info->trimming_value, info->regs + GUC_CTRL_TOP_CTRL0);
		guc_adc_error_abort(info, false);
		guc_adc_nor_mode(info);
		/* voltage measure mode */
		val = readl_relaxed(info->regs + GUC_TOP_ANA_CTRL1);
		val &= ~BIT(8);
		writel_relaxed(val, info->regs + GUC_TOP_ANA_CTRL1);
		/* set control signals from software */
		writel_relaxed(0x1, info->regs + GUC_CTRL_NOR_CTRL0);
		/* initial internal circuit */
		writel_relaxed(0x33, info->regs + GUC_TOP_ANA_CTRL1);
		guc_adc_nor_irq_enable(info, true);
		init_completion(&info->completion);
		info->nb.notifier_call = guc_adc_volt_notify;
	}
	return ret;
}

static int guc_adc_postenable(struct iio_dev *indio_dev)
{
	int i;
	struct guc_adc *info = iio_priv(indio_dev);
	info->sample_nums = 0;
	guc_adc_nor_fifo_reset(info);
	for_each_set_bit (i, indio_dev->active_scan_mask, indio_dev->masklength) {
		guc_adc_nor_channel_enable(info, &indio_dev->channels[i], true);
	}
	guc_adc_nor_start(info, true);
	return 0;
}

static int guc_adc_predisable(struct iio_dev *indio_dev)
{
	int i;
	struct guc_adc *info = iio_priv(indio_dev);
	guc_adc_nor_start(info, false);
	for_each_set_bit (i, indio_dev->active_scan_mask, indio_dev->masklength) {
		guc_adc_nor_channel_enable(info, &indio_dev->channels[i], false);
	}
	return 0;
}

static const struct iio_buffer_setup_ops guc_adc_buffer_setup_ops = {
	.postenable = guc_adc_postenable,
	.predisable = guc_adc_predisable,
};

static int guc_adc_set_trigger(struct platform_device *pdev)
{
	int ret;
	struct device *dev	  = &pdev->dev;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);

	indio_dev->trig =
		devm_iio_trigger_alloc(dev, "%s-dev%d", indio_dev->name, iio_device_id(indio_dev));
	if (!indio_dev->trig) {
		dev_err(dev, "failed to allocate trigger\n");
		return -ENOMEM;
	}
	iio_trigger_set_drvdata(indio_dev->trig, indio_dev);
	ret = devm_iio_trigger_register(dev, indio_dev->trig);
	if (ret) {
		dev_err(dev, "failed to register trigger: %d\n", ret);
		return ret;
	}
	return 0;
}

static int guc_adc_probe(struct platform_device *pdev)
{
	int ret, i;
	u32 irq, val;
	cpumask_t cpumask;
	struct guc_adc *info;
	struct iio_dev *indio_dev;
	struct resource *res;

	indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(*info));
	if (!indio_dev) {
		dev_err(&pdev->dev, "failed allocating iio device\n");
		return -ENOMEM;
	}
	info = iio_priv(indio_dev);
	platform_set_drvdata(pdev, indio_dev);

	/* setup sample buffer */
	info->sample_nums   = 0;
	info->sample_buffer = kmalloc(ARRAY_SIZE(igav04a_adc_iio_channels) * sizeof(u16), GFP_KERNEL);
	if (!info->sample_buffer) {
		return -ENOMEM;
	}
	ret = device_property_read_u32(&pdev->dev, "guc,passwd", &info->passwd);
	if (ret) {
		dev_err(&pdev->dev, "invalid or missing value for guc,passwd\n");
		return ret;
	}
	info->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(info->regs)) {
		return PTR_ERR(info->regs);
	}
	res		 = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	info->passwd_reg = ioremap(res->start, resource_size(res));
	if (!info->passwd_reg) {
		dev_err(&pdev->dev, "unable to memory map registers\n");
		return -ENXIO;
	}
	/* setup clock */
	info->clk = devm_clk_get(&pdev->dev, "adc-clk");
	if (IS_ERR(info->clk)) {
		return dev_err_probe(&pdev->dev, PTR_ERR(info->clk), "failed to get adc clock\n");
	}
	ret = device_property_read_u32(&pdev->dev, "guc,sample-rate", &info->sample_rate);
	if (ret) {
		dev_err(&pdev->dev, "invalid or missing value for guc,sample_rate\n");
		return ret;
	}
	ret = clk_set_rate(info->clk, info->sample_rate << 4);
	if (ret) {
		dev_err(&pdev->dev, "failed to set sample rate\n");
		return ret;
	}
	ret = clk_prepare_enable(info->clk);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to enable converter clock\n");
		return ret;
	}
	info->pclk = devm_clk_get(&pdev->dev, "adc-pclk");
	if (IS_ERR(info->pclk)) {
		return dev_err_probe(&pdev->dev, PTR_ERR(info->pclk), "failed to get adc pclock\n");
	}
	ret = clk_prepare_enable(info->pclk);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to enable converter pclock\n");
		return ret;
	}

	ret = devm_add_action_or_reset(&pdev->dev, guc_adc_clk_disable, info);
	if (ret) {
		dev_err(&pdev->dev, "failed to register devm action, %d\n", ret);
		return ret;
	}

	/* regulator init */
	info->vref = devm_regulator_get(&pdev->dev, "vref");
	if (IS_ERR(info->vref))
		return dev_err_probe(&pdev->dev, PTR_ERR(info->vref), "failed to get regulator\n");

	ret = regulator_enable(info->vref);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to enable vref regulator\n");
		return ret;
	}

	ret = devm_add_action_or_reset(&pdev->dev, guc_adc_regulator_disable, info);
	if (ret) {
		dev_err(&pdev->dev, "failed to register devm action, %d\n", ret);
		return ret;
	}

	ret = regulator_get_voltage(info->vref);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to regulator_get_voltage\n");
		return ret;
	}
	info->uv_vref = ret;

	if (device_property_read_bool(&pdev->dev, "guc,scan"))
		info->guc_convert_mode = SCAN;
	else
		info->guc_convert_mode = SINGLE;

	/* nvmem value is optional */
	ret = adc_nvmem_cell_data(pdev, "adc-offset", &val);
	if (ret < 0) {
		info->calibration_offset = 0;
	} else {
		info->calibration_offset =
			(val & IGAV04_ADC_EFUSE_CALIB_MASK) >> IGAV04_ADC_EFUSE_CALIB_OFFSET;
	}
	if (info->calibration_offset & IGAV04_ADC_EFUSE_NEG_FLAG)
		info->calibration_offset |= ~IGAV04_ADC_EFUSE_NEG_MASK;

	dev_info(&pdev->dev, "ADC calibration: %d\n", info->calibration_offset);

	ret = adc_nvmem_cell_data(pdev, "adc-trimming", &val);
	if (ret < 0)
		info->trimming_value = 0;
	else
		info->trimming_value =
			(val & IGAV04_ADC_EFUSE_TRIM_MASK) >> IGAV04_ADC_EFUSE_TRIM_OFFSET;
	dev_info(&pdev->dev, "ADC trimming: %d\n", info->trimming_value);

	info->reset = devm_reset_control_get_exclusive(&pdev->dev,
						       "guc-adc");
	if (IS_ERR(info->reset))
		return PTR_ERR(info->reset);

	reset_control_assert(info->reset);
	usleep_range(10, 20);
	reset_control_deassert(info->reset);
	ret = guc_adc_hw_init(info);
	if (ret) {
		dev_err(&pdev->dev, "failed to init hardware %d\n", ret);
		return ret;
	}

	/* setup interrupt */
	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		return dev_err_probe(&pdev->dev, irq, "failed to get irq\n");
	}
	ret = devm_request_threaded_irq(&pdev->dev, irq, guc_adc_isr, guc_adc_thread_irq, IRQF_ONESHOT, dev_name(&pdev->dev),
		       indio_dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed requesting irq %d\n", irq);
		return ret;
	}

	cpumask_clear(&cpumask);
	for_each_cpu(i, cpu_online_mask) {
		if (i != 0) {
			cpumask_set_cpu(i, &cpumask);
		}
	}
	irq_set_affinity(irq, &cpumask);

	info->irq		= irq;
	indio_dev->name		= dev_name(&pdev->dev);
	indio_dev->info		= &guc_adc_iio_info;
	indio_dev->modes	= INDIO_DIRECT_MODE;
	indio_dev->channels	= igav04a_adc_iio_channels;
	indio_dev->num_channels = ARRAY_SIZE(igav04a_adc_iio_channels);
	ret = guc_adc_set_trigger(pdev);
	if (ret) {
		dev_err(&pdev->dev, "failed to set trigger\n");
		return ret;
	}
	ret = devm_iio_triggered_buffer_setup(&indio_dev->dev, indio_dev, iio_pollfunc_store_time,
					      guc_adc_trigger_handler, &guc_adc_buffer_setup_ops);
	if (ret) {
		dev_err(&pdev->dev, "failed to devm_iio_triggered_buffer_setup\n");
		return ret;
	}

	ret = regulator_register_notifier(info->vref, &info->nb);
	if (ret) {
		dev_err(&pdev->dev, "failed to regulator_register_notifier\n");
		return ret;
	}

	ret = devm_add_action_or_reset(&pdev->dev, guc_adc_regulator_unreg_notifier, info);
	if (ret) {
		dev_err(&pdev->dev, "failed to unreg regulator notifier\n");
		return ret;
	}

	dev_info(&pdev->dev, "GUC Initialize Finish\n");
	return devm_iio_device_register(&pdev->dev, indio_dev);
}

static int guc_adc_remove(struct platform_device *pdev)
{
	u32 i;
	struct device *dev	  = &pdev->dev;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct guc_adc *info	  = iio_priv(indio_dev);

	guc_adc_nor_start(info, false);
	for (i = 0; i < indio_dev->num_channels; ++i) {
		guc_adc_nor_channel_enable(info, &indio_dev->channels[i], false);
	}
	guc_adc_nor_irq_enable(info, false);
	guc_adc_clk_disable(info);
	kfree(info->sample_buffer);
	info->sample_buffer = NULL;
	if (info->vref) {
		regulator_unregister_notifier(info->vref, &info->nb);
		regulator_disable(info->vref);
	}
	if (indio_dev->trig) {
		iio_trigger_unregister(indio_dev->trig);
		iio_trigger_free(indio_dev->trig);
	}
	iio_triggered_buffer_cleanup(indio_dev);
	if (info->irq >= 0) {
		devm_free_irq(dev, info->irq, indio_dev);
	}
	devm_remove_action(dev, guc_adc_regulator_unreg_notifier, info);
	devm_remove_action(dev, guc_adc_regulator_disable, info);
	devm_remove_action(dev, guc_adc_clk_disable, info);
	iio_device_unregister(indio_dev);
	iio_device_free(indio_dev);
	return 0;
}

static int guc_adc_suspend(struct device *dev)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct guc_adc *info	  = iio_priv(indio_dev);

	clk_disable_unprepare(info->clk);
	clk_disable_unprepare(info->pclk);
	regulator_disable(info->vref);
	return 0;
}

static int guc_adc_resume(struct device *dev)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct guc_adc *info	  = iio_priv(indio_dev);
	int ret;

	ret = regulator_enable(info->vref);
	if (ret) {
		return ret;
	}
	ret = clk_prepare_enable(info->clk);
	ret = clk_prepare_enable(info->pclk);
	guc_adc_hw_init(info);
	return ret;
}

static DEFINE_SIMPLE_DEV_PM_OPS(guc_adc_pm_ops, guc_adc_suspend, guc_adc_resume);

static struct platform_driver guc_adc_driver = {
	.probe	= guc_adc_probe,
	.remove = guc_adc_remove,
	.driver =
		{
			.name		= "guc",
			.of_match_table = guc_adc_match,
			.pm		= pm_sleep_ptr(&guc_adc_pm_ops),
		},
};

module_platform_driver(guc_adc_driver);

MODULE_AUTHOR("D-Robotics");
MODULE_DESCRIPTION("ADC Driver for GUC");
MODULE_LICENSE("GPL v2");
