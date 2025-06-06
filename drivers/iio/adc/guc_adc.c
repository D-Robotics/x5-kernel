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
#include <linux/iio/sysfs.h>
#include <linux/iio/buffer_impl.h>
#include <linux/iio/iio-opaque.h>
#include <linux/iio/trigger.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/nvmem-consumer.h>
#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>

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
#define GUC_NOR_DMA_MASK	      GENMASK(3, 0)
#define GUC_FIFO_HALF_FULL_DMA_EN BIT(2)

#define GUC_CTRL_NOR_STS2	   0x0a34
#define GUC_NOR_INT		   BIT(31)
#define GUC_NOR_INT_ALL_DISABLE (0x0u)
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

#define GUC_ADC_TIMEOUT_US   10000
#define FIFO_HALF_FULL_DEPTH (32)
#define GUC_ADC_FIFO_VALID_BITS (16u)
#define GUC_ADC_FIFO_VALID_LEN 10
#define GUC_ADC_CHANNEL_NUM 10
#define GUC_ADC_MAX_VAL ((1 << GUC_ADC_FIFO_VALID_LEN) - 1)
#define GUC_ADC_FIFO_VALID_BYTES (GUC_ADC_FIFO_VALID_BITS / BITS_PER_BYTE)

/* adc calibration information in eFuse */
#define IGAV04_ADC_EFUSE_BYTES	      6
#define IGAV04_ADC_EFUSE_CALIB_MASK   GENMASK(27, 22)
#define IGAV04_ADC_EFUSE_CALIB_OFFSET 22
#define IGAV04_ADC_EFUSE_TRIM_MASK    GENMASK(31, 28)
#define IGAV04_ADC_EFUSE_TRIM_OFFSET  28

#define IGAV04_ADC_EFUSE_NEG_FLAG BIT(5)
#define IGAV04_ADC_EFUSE_NEG_MASK GENMASK(5, 0)

#define GUC_FULL_FIFO_DEPTH   (64)
#define GUC_FIFO_WIDTH_OFFSET (1)
#define GUC_FIFO_SIZE	      ((GUC_FULL_FIFO_DEPTH << GUC_FIFO_WIDTH_OFFSET))
#define GUC_HALF_FIFO_SIZE    (((GUC_FIFO_SIZE) >> 1))
#define GUC_DMA_BUFFER_SIZE   (((GUC_FIFO_SIZE) << 1))

static unsigned long cycle_period = 100; // Default time period 100ms
int switch_bit = 1;
static struct iio_buffer_setup_ops guc_adc_buffer_setup_ops;

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
	u32 last_val;
	unsigned int bufi;
	struct dma_chan *dma_chan;
	phys_addr_t phys_addr;
	u8 *rx_buf;
	dma_addr_t rx_dma_buf;
	unsigned int rx_buf_sz;
	unsigned int rx_block_sz;
	const struct iio_chan_spec *work_chan;
	struct notifier_block nb;
	int32_t calibration_offset;
	u32 trimming_value;
	struct reset_control *reset;
	u32 skip_data_sz;
	struct dma_private_flag dma_flag;
	u32 irq;
	u16 *sample_buffer;
	u32 sample_nums;
	u32 sample_rate;
	rwlock_t adc_lock;
	struct hrtimer timer;
	int channle_stat[GUC_ADC_CHANNEL_NUM];
	int len;
	int cur_channel;
	struct iio_dev *indio_dev;
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
			 .storagebits = GUC_ADC_FIFO_VALID_BITS,                                                  \
			 .endianness  = IIO_CPU,                                             \
		 },                                                                          \
	}

static const struct iio_chan_spec igav04a_adc_iio_channels[] = {
	GUC_ADC_CHANNEL(0, "adc0", 10), GUC_ADC_CHANNEL(1, "adc1", 10),
	GUC_ADC_CHANNEL(2, "adc2", 10), GUC_ADC_CHANNEL(3, "adc3", 10),
	GUC_ADC_CHANNEL(4, "adc4", 10), GUC_ADC_CHANNEL(5, "adc5", 10),
	GUC_ADC_CHANNEL(6, "adc6", 10), GUC_ADC_CHANNEL(7, "adc7", 10),
};

static const struct of_device_id guc_adc_match[] = {
	{
		.compatible = "guc,igav04a",
	},
	{},
};
MODULE_DEVICE_TABLE(of, guc_adc_match);

/**
 * @brief Check if adc is busy
 *
 * @param[in] info: guc_adc driver struct
 *
 * @retval true: adc is busy
 * @retval false: adc is idle
 */
static inline bool guc_adc_is_busy(struct guc_adc *info)
{
	bool is_busy;
	read_lock_bh(&info->adc_lock);
	is_busy = readl_relaxed(info->regs + GUC_CTRL_NOR_CTRL2);
	read_unlock_bh(&info->adc_lock);
	return is_busy;
}

/**
 * @brief Reset adc fifo
 *
 * @param[in] info: guc_adc driver struct
 *
 * @retval * void
 */
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

/**
 * @brief Reset adc controller
 *
 * @param[in] info: guc_adc driver struct
 *
 * @retval * void
 */
static void guc_adc_controller_reset(struct guc_adc *info)
{
	writel_relaxed(0x00, info->regs + GUC_CTRL_TOP_CTRL17);
	udelay(10);
	writel_relaxed(0x01, info->regs + GUC_CTRL_TOP_CTRL17);
}
/*
 * ADC normal mode interrupt type enable.
 */
static inline void guc_adc_nor_irq_set(struct guc_adc *info, u32 mask)
{
	writel_relaxed(mask, info->regs + GUC_CTRL_NOR_CTRL7);
}

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
static void guc_adc_nor_channel_enable(struct guc_adc *info, int channel,
				       bool enable)
{
	u32 flag;

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
static inline void guc_adc_nor_mode(struct guc_adc *info, enum convert_mode mode)
{
	writel_relaxed(mode, info->regs + GUC_CTRL_NOR_CTRL4);
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

static inline u32 guc_adc_nor_fifo_read(struct guc_adc *info)
{
	u32 val;
	int mid;
	val = (readl_relaxed(info->regs + GUC_CTRL_NOR_STS1) & SAMPLE_MASK);
	mid = val >> SAMPLE_OFFSET;

	if (likely(mid > info->calibration_offset)) {
		val = mid - info->calibration_offset;
		val = (val > GUC_ADC_MAX_VAL) ? GUC_ADC_MAX_VAL : val;
	} else {
		val = 0;
	};

	return val;
}

/*
 * Enable normal ADC controller from sw reg.
 */
static inline void guc_adc_nor_start(struct guc_adc *info, bool enable)
{
	write_lock(&info->adc_lock);
	writel_relaxed(enable ? 0x01u : 0x00u, info->regs + GUC_CTRL_NOR_CTRL2);
	write_unlock(&info->adc_lock);
}

/*
 * Enable normal ADC DMA interrupt.
 */
static void guc_adc_dma_irq_enable(struct guc_adc *info, bool enable)
{
	int enable_irq;

	enable_irq = readl_relaxed(info->regs + GUC_CTRL_NOR_CTRL9);
	if (enable)
		enable_irq |= (GUC_NOR_FIFO_FULL_DMA_REQ);
	else
		enable_irq &= ~(GUC_NOR_DMA_MASK);
	writel_relaxed(enable_irq, info->regs + GUC_CTRL_NOR_CTRL9);
}

/*
 * Trigger conversion.
 */
static int guc_adc_conversion(struct guc_adc *info, struct iio_chan_spec const *chan)
{
	int ret = 0;
	unsigned long tmflag;

	reinit_completion(&info->completion);
	guc_adc_controller_reset(info);
	guc_adc_nor_fifo_reset(info);
	guc_adc_nor_irq_set(info, (u32)GUC_NOR_SAMPLE_DONE_INT_EN);
	guc_adc_fifo_read_enable(info, true);
	guc_adc_nor_channel_enable(info, chan->channel, true);
	guc_adc_nor_start(info, true);

	tmflag = wait_for_completion_timeout(&info->completion, msecs_to_jiffies(GUC_ADC_TIMEOUT_US));
	if (!tmflag) {
		ret = -ETIMEDOUT;
	}

	guc_adc_nor_start(info, false); /* timeout or normal, adc should be stopped */
	guc_adc_nor_channel_enable(info, chan->channel, false);
	guc_adc_nor_irq_set(info, (u32)~(GUC_NOR_INT_MASK));
	guc_adc_fifo_read_enable(info, false);

	return ret;
}

/*
 * Read raw data.
 */
static int guc_adc_read_raw(struct iio_dev *indio_dev, struct iio_chan_spec const *chan, int *val,
			    int *val2, long mask)
{
	int ret = -EINVAL;
	struct guc_adc *info = iio_priv(indio_dev);

	/* read raw mode, switch adc to single mode */
	guc_adc_nor_mode(info, SINGLE);
	if (mask == IIO_CHAN_INFO_RAW) {
		ret = -EBUSY;
		if (!iio_buffer_enabled(indio_dev)) {
			mutex_lock(&indio_dev->mlock);
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
	struct iio_dev *indio_dev = dev_id;
	struct guc_adc *info = iio_priv(indio_dev);
	register u16 *buffer = info->sample_buffer;
	register u32 push_steps = indio_dev->scan_bytes / GUC_ADC_FIFO_VALID_BYTES;
	int buf_len  = FIFO_HALF_FULL_DEPTH - (FIFO_HALF_FULL_DEPTH % push_steps);
	u32 val, i;
	val = readl_relaxed(info->regs + GUC_CTRL_NOR_STS0);
	if (val & GUC_NOR_FIFO_EMPTY) {
		goto exit;
	}

	val = readl_relaxed(info->regs + GUC_CTRL_NOR_STS2);
	if (val & GUC_NOR_FIFO_HALF_FULL_INT) {
		for (i = 0; i < buf_len; i++) {
			*buffer++ = guc_adc_nor_fifo_read(info);
		}
		buffer = info->sample_buffer;
		for (i = 0; i < buf_len / push_steps; i++) {
			iio_push_to_buffers(indio_dev, buffer);
			buffer += push_steps;
		}
	} else if (val & GUC_NOR_SAMPLE_DONE_INT) {
		info->last_val = guc_adc_nor_fifo_read(info);
		complete(&info->completion);
	}
exit:
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
		guc_adc_nor_mode(info, SINGLE);
		/* voltage measure mode */
		val = readl_relaxed(info->regs + GUC_TOP_ANA_CTRL1);
		val &= ~BIT(8);
		writel_relaxed(val, info->regs + GUC_TOP_ANA_CTRL1);
		/* set control signals from software */
		writel_relaxed(0x1, info->regs + GUC_CTRL_NOR_CTRL0);
		/* initial internal circuit */
		writel_relaxed(0x33, info->regs + GUC_TOP_ANA_CTRL1);
	}
	return ret;
}

static int guc_adc_dma_request(struct device *dev, struct iio_dev *indio_dev)
{
	struct guc_adc *info = iio_priv(indio_dev);
	struct dma_slave_config config;
	int ret;

	info->dma_chan = dma_request_chan(dev, "rx");
	if (IS_ERR(info->dma_chan)) {
		ret = PTR_ERR(info->dma_chan);
		if (ret != -ENODEV)
			return dev_err_probe(dev, ret, "DMA channel request failed with\n");

		/* DMA is optional: fall back to IRQ mode */
		info->dma_chan = NULL;
		return 0;
	}

	info->rx_buf = dma_alloc_coherent(info->dma_chan->device->dev, GUC_DMA_BUFFER_SIZE,
					  &info->rx_dma_buf, GFP_KERNEL);
	if (!info->rx_buf) {
		ret = -ENOMEM;
		goto err_release;
	}

	/* Configure DMA channel to read data register */
	memset(&config, 0, sizeof(config));
	config.src_addr = (dma_addr_t)info->phys_addr;
	config.src_addr += GUC_CTRL_NOR_STS1;
	config.src_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
	config.dst_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
	config.src_msize = 64;
	config.dst_msize = 64;

	ret = dmaengine_slave_config(info->dma_chan, &config);
	if (ret)
		goto err_free;

	return 0;

err_free:
	dma_free_coherent(info->dma_chan->device->dev, GUC_DMA_BUFFER_SIZE, info->rx_buf,
			  info->rx_dma_buf);
err_release:
	dma_release_channel(info->dma_chan);
	return ret;
}

/*
 * Move data read from FIFO to buffer.
 */
static void guc_adc_dma_buffer_done(void *data)
{
	struct iio_dev *indio_dev = data;
	struct guc_adc *info	  = iio_priv(indio_dev);
	u16 buffer[FIFO_HALF_FULL_DEPTH];
	u32 i, offset;
	static int flag = 0;
	static int reset = 0;
	static int cur_channel = -1;
	u16 mark = 1 << GUC_ADC_FIFO_VALID_LEN;
	u16 *buf;

	if (reset ^ switch_bit){
		reset = switch_bit;
		cur_channel = -1;
	}

	if(cur_channel != info->cur_channel) {
		cur_channel = info->cur_channel;
		flag = 1;
		info->skip_data_sz = 0;
	}

	// drop first fifo data
	if (info->skip_data_sz < GUC_FIFO_SIZE) {
		info->skip_data_sz += info->rx_block_sz;
		goto exit;
	}

	buf    = (u16 *)&info->rx_buf[info->bufi];
	offset = indio_dev->scan_bytes / GUC_ADC_FIFO_VALID_BYTES;
	memcpy(buffer, buf, info->rx_block_sz);

	for (i = 0; i < info->rx_block_sz / GUC_ADC_FIFO_VALID_BYTES; i++) {
		if (likely((buffer[i] >> SAMPLE_OFFSET) > info->calibration_offset)) {
			buffer[i] = (buffer[i] >> SAMPLE_OFFSET) - info->calibration_offset;
			buffer[i] = (buffer[i] > GUC_ADC_MAX_VAL) ? GUC_ADC_MAX_VAL : buffer[i];
		} else {
			buffer[i] = 0;
		}
	}
	if (flag && (info->len > 1)) {
		flag = 0;
		buffer[0] = mark + info->cur_channel;
	}
	for (i = 0; i < info->rx_block_sz / indio_dev->scan_bytes; i++) {
		iio_push_to_buffers(indio_dev, &buffer[i * offset]);
	}
exit:
	info->bufi += info->rx_block_sz;
	if (info->bufi >= info->rx_buf_sz) {
		info->bufi = 0;
	}
}

/*
 * Configure DMA.
 */
static int guc_adc_dma_start(struct iio_dev *indio_dev)
{
	struct guc_adc *info = iio_priv(indio_dev);
	struct dma_async_tx_descriptor *desc;
	dma_cookie_t cookie;
	int ret, period_len;

	if (!info->dma_chan)
		return 0;

	info->dma_chan->private = (void *)(&info->dma_flag);

	info->rx_block_sz = indio_dev->scan_bytes * (GUC_HALF_FIFO_SIZE / indio_dev->scan_bytes);
	info->rx_buf_sz	  = info->rx_block_sz * (GUC_DMA_BUFFER_SIZE / info->rx_block_sz);
	dev_dbg(&indio_dev->dev, "%s size=%d watermark=%d\n", __func__, info->rx_buf_sz,
		info->rx_buf_sz / 2);

	period_len		    = info->rx_block_sz;
	info->dma_flag.is_block_tfr = true;
	/* Prepare a DMA cyclic transaction */
	desc = dmaengine_prep_dma_cyclic(info->dma_chan, info->rx_dma_buf, info->rx_buf_sz,
					 period_len, DMA_DEV_TO_MEM, DMA_PREP_INTERRUPT);
	if (!desc)
		return -EBUSY;

	desc->callback	     = guc_adc_dma_buffer_done;
	desc->callback_param = indio_dev;

	cookie = dmaengine_submit(desc);
	ret    = dma_submit_error(cookie);
	if (ret) {
		dmaengine_terminate_sync(info->dma_chan);
		return ret;
	}

	/* Issue pending DMA requests */
	dma_async_issue_pending(info->dma_chan);

	return 0;
}

/*
 * Switch channel.
 */
static enum hrtimer_restart iio_hrtime_handler(struct hrtimer *timer)
{
	struct guc_adc *info;
	static int channel = 0;
	static int index = 0;
	static int reset = 0;
	int ret = 0;
	info = container_of(timer, struct guc_adc, timer);

	if (!(reset ^ switch_bit)) {
		dmaengine_terminate_async(info->dma_chan);
		guc_adc_nor_start(info, false);
		guc_adc_fifo_read_enable(info, false);
		guc_adc_dma_irq_enable(info, false);
		guc_adc_nor_channel_enable(info, channel, false);
		guc_adc_controller_reset(info);
		guc_adc_nor_fifo_reset(info);
		ret = guc_adc_dma_start(info->indio_dev);
		if (ret) {
			dev_err(&info->indio_dev->dev, "Can't start dma\n");
			return ret;
		}
	} else {
		index = 0;
		reset  = switch_bit;
	}

	index = (index  +  info->len) % (info->len);
	channel = info->channle_stat[index];
	info->cur_channel = channel;
	index = index + 1;
	guc_adc_fifo_read_enable(info, true);
	guc_adc_dma_irq_enable(info, true);
	guc_adc_nor_channel_enable(info, channel, true);
	guc_adc_nor_start(info, true);
	hrtimer_forward_now(timer, ktime_set(0, cycle_period * 1000000));
	return HRTIMER_RESTART;
}

static int guc_adc_cpu_postenable(struct iio_dev *indio_dev)
{
	int i;
	struct guc_adc *info = iio_priv(indio_dev);
	unsigned long chan_num = 0;
	info->bufi	   = 0;
	info->skip_data_sz = 0;
	chan_num = *indio_dev->active_scan_mask;
	/* use iio framework, switch adc to scan mode */
	guc_adc_controller_reset(info);
	guc_adc_nor_mode(info, SCAN);
	info->sample_nums = 0;
	guc_adc_nor_fifo_reset(info);
	guc_adc_nor_irq_set(info, (u32)GUC_NOR_FIFO_HALF_FULL_INT_EN);
	guc_adc_fifo_read_enable(info, true);

	for_each_set_bit (i, indio_dev->active_scan_mask, indio_dev->masklength) {
		guc_adc_nor_channel_enable(info, (&indio_dev->channels[i])->channel, true);
	}
	guc_adc_nor_start(info, true);
	return 0;
}

static int guc_adc_dma_postenable(struct iio_dev *indio_dev)
{
	int ret, i;
	struct guc_adc *info = iio_priv(indio_dev);
	const struct iio_chan_spec *chan;
	info->bufi	   = 0;
	info->skip_data_sz = 0;
	/* use iio framework, switch adc to scan mode */
	guc_adc_controller_reset(info);
	guc_adc_nor_mode(info, SCAN);
	info->sample_nums = 0;
	guc_adc_nor_fifo_reset(info);
	ret = guc_adc_dma_start(indio_dev);
	if (ret) {
		dev_err(&indio_dev->dev, "Can't start dma\n");
		return ret;
	}

	for_each_set_bit (i, indio_dev->active_scan_mask, indio_dev->masklength) {
		chan = &indio_dev->channels[i];
		info->channle_stat[info->len] = chan->channel;
		info->len++;
	}

	if (info->len > 1) {
		hrtimer_init(&info->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		info->timer.function = iio_hrtime_handler;
		hrtimer_start(&info->timer, ktime_set(0, 0), HRTIMER_MODE_REL);
	} else {
		guc_adc_dma_irq_enable(info, true);
		guc_adc_fifo_read_enable(info, true);
		info->cur_channel = chan->channel;
		guc_adc_nor_channel_enable(info, chan->channel, true);
		guc_adc_nor_start(info, true);
	}
	return 0;
}

static int guc_adc_cpu_predisable(struct iio_dev *indio_dev)
{
	int i;
	struct guc_adc *info = iio_priv(indio_dev);

	guc_adc_nor_start(info, false);
	guc_adc_fifo_read_enable(info, false);
	guc_adc_nor_irq_set(info, (u32)~(GUC_NOR_INT_MASK));
	for_each_set_bit (i, indio_dev->active_scan_mask, indio_dev->masklength) {
		guc_adc_nor_channel_enable(info, (&indio_dev->channels[i])->channel, false);
	}
	return 0;
}

static int guc_adc_dma_predisable(struct iio_dev *indio_dev)
{
	int i;
	struct guc_adc *info = iio_priv(indio_dev);
	switch_bit = ~switch_bit;
	if (info->len > 1)
		hrtimer_cancel(&info->timer);
	dmaengine_terminate_sync(info->dma_chan);
	guc_adc_nor_start(info, false);
	guc_adc_fifo_read_enable(info, false);
	guc_adc_dma_irq_enable(info, false);
	memset(info->channle_stat, 0, sizeof(info->channle_stat));
	info->len = 0;
	for_each_set_bit (i, indio_dev->active_scan_mask, indio_dev->masklength) {
		guc_adc_nor_channel_enable(info, (&indio_dev->channels[i])->channel, false);
	}
	return 0;
}

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

static ssize_t cycle_period_show(struct device *dev, struct device_attribute *attr, char *buf) {
	return sprintf(buf, "%lu\n", cycle_period);
}

static ssize_t cycle_period_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t len) {
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct iio_buffer *buffer = indio_dev->buffer;
	unsigned long new_timeout;
	int ret;

	if (!list_empty(&(buffer->buffer_list))) {
		dev_err(dev, "ERROR: period is only allowed to be configured when no ADC channel is enabled\n");
		return -EBUSY;
	}
	ret = kstrtoul(buf, 10, &new_timeout);
	if (ret) {
		if (ret == -EINVAL)
			dev_err(dev, "ERROR: Only base 10 number is allowed!\n");
		return ret;
	}

	cycle_period = new_timeout;
	return len;
}

static DEVICE_ATTR(cycle_period, S_IWUSR | S_IRUGO, cycle_period_show, cycle_period_store);

static int guc_adc_set_ops(struct iio_dev *indio_dev) {
	struct guc_adc *info = iio_priv(indio_dev);

	if (info->dma_chan) {
		guc_adc_buffer_setup_ops.postenable = guc_adc_dma_postenable;
		guc_adc_buffer_setup_ops.predisable = guc_adc_dma_predisable;
	} else {
		guc_adc_buffer_setup_ops.postenable = guc_adc_cpu_postenable;
		guc_adc_buffer_setup_ops.predisable = guc_adc_cpu_predisable;
	}
	return 0;
}

static int guc_adc_get_sample_rate(struct guc_adc *info)
{
	int ret = 0;
	u32 sample_rate = info->sample_rate;

	switch (sample_rate) {
		case 260000:
			info->sample_rate = 1560000;
			break;
		case 210000:
			info->sample_rate = 1250000;
			break;
		case 130000:
			info->sample_rate = 780000;
			break;
		case 100000:
			info->sample_rate = 630000;
			break;
		default:
			ret = -EINVAL;
			break;
	}
	return ret;
}

static int guc_adc_probe(struct platform_device *pdev)
{
	int ret, i;
	u32 irq, val;
	cpumask_t cpumask;
	struct guc_adc *info;
	struct iio_dev *indio_dev;
	struct resource *res;
	struct resource *res_adc;

	indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(*info));
	if (!indio_dev) {
		dev_err(&pdev->dev, "failed allocating iio device\n");
		return -ENOMEM;
	}
	info = iio_priv(indio_dev);
	info->indio_dev = indio_dev;
	platform_set_drvdata(pdev, indio_dev);

	/* setup sample buffer */
	info->sample_nums   = 0;
	info->sample_buffer = kmalloc_array(FIFO_HALF_FULL_DEPTH, GUC_ADC_FIFO_VALID_BYTES, GFP_KERNEL);
	if (!info->sample_buffer) {
		return -ENOMEM;
	}
	ret = device_property_read_u32(&pdev->dev, "guc,passwd", &info->passwd);
	if (ret) {
		dev_err(&pdev->dev, "invalid or missing value for guc,passwd\n");
		return ret;
	}
	info->regs = devm_platform_get_and_ioremap_resource(pdev, 0, &res_adc);
	if (IS_ERR(info->regs)) {
		return PTR_ERR(info->regs);
	}
	/* if we plan to use DMA, we need the physical address of the regs */
	info->phys_addr = res_adc->start;
	ret = guc_adc_dma_request(&pdev->dev, indio_dev);
	if (ret < 0)
 		return ret;

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
	if (info->dma_chan) {
		ret = guc_adc_get_sample_rate(info);
		if (ret) {
			dev_err(&pdev->dev, "Sample rate is not within the allowed range\n");
			return ret;
		}
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
	guc_adc_set_ops(indio_dev);
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

	init_completion(&info->completion);
	info->nb.notifier_call = guc_adc_volt_notify;
	rwlock_init(&info->adc_lock);

	ret = devm_iio_device_register(&pdev->dev, indio_dev);
	if (ret) {
		dev_err(&pdev->dev, "could not register iio (adc)");
		return ret;
	}

	if (info->dma_chan) {
		ret = device_create_file(&indio_dev->dev, &dev_attr_cycle_period);
		if (ret) {
			dev_err(&pdev->dev, "Failed to create device file cycle_period");
			return ret;
		}
	}
	dev_info(&pdev->dev, "GUC Initialize Finish\n");
	return ret;
}

static int guc_adc_remove(struct platform_device *pdev)
{
	u32 i;
	struct device *dev	  = &pdev->dev;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct guc_adc *info	  = iio_priv(indio_dev);

	guc_adc_nor_start(info, false);
	for (i = 0; i < indio_dev->num_channels; ++i) {
		guc_adc_nor_channel_enable(info, (&indio_dev->channels[i])->channel, false);
	}
	guc_adc_nor_irq_set(info, (u32)GUC_NOR_INT_ALL_DISABLE);
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

	if (guc_adc_is_busy(info)) {
		return -EBUSY;
	}

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
