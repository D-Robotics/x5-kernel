// SPDX-License-Identifier: GPL-2.0+
/*
 * 8250_dma.c - DMA Engine API support for 8250.c
 *
 * Copyright (C) 2013 Intel Corporation
 */
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial_reg.h>
#include <linux/serial_8250.h>
#include <linux/dma-mapping.h>

#include "8250.h"

#define RX_DMA_SIZE	(1024)

/*
 * Pick a DMA burst/period length from slave config, falling back to
 * fifosize/4 (DW APB UART default) then 16. Used so cyclic RX period and
 * TX chunk size track the FIFO trigger instead of a hardcoded 16.
 */
static size_t serial8250_dma_burst_len(struct uart_8250_port *p, bool rx)
{
	struct uart_8250_dma *dma = p->dma;
	size_t burst = 0;

	if (rx) {
		if (dma->rxconf.src_msize)
			burst = dma->rxconf.src_msize;
		else if (dma->rxconf.src_maxburst)
			burst = dma->rxconf.src_maxburst;
	} else {
		if (dma->txconf.dst_maxburst)
			burst = dma->txconf.dst_maxburst;
		else if (dma->txconf.dst_msize)
			burst = dma->txconf.dst_msize;
	}

	if (!burst && p->port.fifosize)
		burst = p->port.fifosize / 4;
	if (!burst)
		burst = 16;

	return burst;
}

static void serial8250_dma_setup_periods(struct uart_8250_port *p)
{
	struct uart_8250_dma *dma = p->dma;

	dma->rx_period = serial8250_dma_burst_len(p, true);
	dma->tx_burst = serial8250_dma_burst_len(p, false);

	/* Cyclic period must divide the ring buffer length. */
	while (dma->rx_period > 1 && (dma->rx_size % dma->rx_period))
		dma->rx_period >>= 1;

	if (!dma->tx_burst)
		dma->tx_burst = 16;

	dev_dbg(p->port.dev, "dma rx_period=%zu tx_burst=%zu fifosize=%u\n",
		dma->rx_period, dma->tx_burst, p->port.fifosize);
}

static void __dma_tx_complete(void *param)
{
	struct uart_8250_port	*p = param;
	struct uart_8250_dma	*dma = p->dma;
	struct circ_buf		*xmit = &p->port.state->xmit;
	unsigned long	flags;
	int		ret;

	dma_sync_single_for_cpu(dma->txchan->device->dev, dma->tx_addr,
				UART_XMIT_SIZE, DMA_TO_DEVICE);

	spin_lock_irqsave(&p->port.lock, flags);

	dma->tx_running = 0;

	uart_xmit_advance(&p->port, dma->tx_size);

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(&p->port);

	ret = serial8250_tx_dma(p);
	if (ret || !dma->tx_running)
		serial8250_set_THRI(p);

	spin_unlock_irqrestore(&p->port.lock, flags);
}
static void __dma_rx_complete_cyclic(void *param)
{
	struct uart_8250_port	*p = param;
	struct uart_8250_dma	*dma = p->dma;
	struct tty_port		*tty_port = &p->port.state->port;
	struct dma_tx_state	state;
	enum dma_status		dma_status;
	int			count;
	unsigned int head, tail;
	unsigned char *buf = dma->rx_buf;

	/*
	 * New DMA Rx can be started during the completion handler before it
	 * could acquire port's lock and it might still be ongoing. Don't to
	 * anything in such case.
	 */
	dma_status = dmaengine_tx_status(dma->rxchan, dma->rx_cookie, &state);

	if (dma_status == DMA_ERROR)
		return;

	buf += dma->rx_pos;
	head = dma->rx_pos;
	tail = dma->rx_size - state.residue;

	if (tail < head) {
		count = (dma->rx_size - head) + tail;
		tty_insert_flip_string(tty_port, buf, dma->rx_size - head);
		buf = dma->rx_buf;
		tty_insert_flip_string(tty_port, buf, tail);
	} else {
		count = tail - head;
		tty_insert_flip_string(tty_port, buf, count);
	}

	p->port.icount.rx += count;
	dma->rx_pos = tail;

	tty_flip_buffer_push(tty_port);
}

static void dma_rx_complete_cyclic(void *param)
{
	struct uart_8250_port *p = param;
	struct uart_8250_dma *dma = p->dma;
	unsigned long flags;

	spin_lock_irqsave(&p->port.lock, flags);
	if (dma->rx_running)
		__dma_rx_complete_cyclic(p);
	spin_unlock_irqrestore(&p->port.lock, flags);
}

static void __dma_rx_complete(void *param)
{
	struct uart_8250_port	*p = param;
	struct uart_8250_dma	*dma = p->dma;
	struct tty_port		*tty_port = &p->port.state->port;
	struct dma_tx_state	state;
	enum dma_status		dma_status;
	int			count;

	/*
	 * New DMA Rx can be started during the completion handler before it
	 * could acquire port's lock and it might still be ongoing. Don't to
	 * anything in such case.
	 */
	dma_status = dmaengine_tx_status(dma->rxchan, dma->rx_cookie, &state);
	if (dma_status == DMA_IN_PROGRESS)
		return;

	count = dma->rx_size - state.residue;

	tty_insert_flip_string(tty_port, dma->rx_buf, count);
	p->port.icount.rx += count;
	dma->rx_running = 0;

	tty_flip_buffer_push(tty_port);
}

static void dma_rx_complete(void *param)
{
	struct uart_8250_port *p = param;
	struct uart_8250_dma *dma = p->dma;
	unsigned long flags;

	spin_lock_irqsave(&p->port.lock, flags);
	if (dma->rx_running)
		__dma_rx_complete(p);
	spin_unlock_irqrestore(&p->port.lock, flags);
}

int serial8250_tx_dma(struct uart_8250_port *p)
{
	struct uart_8250_dma		*dma = p->dma;
	struct circ_buf			*xmit = &p->port.state->xmit;
	struct dma_async_tx_descriptor	*desc;
	struct uart_port		*up = &p->port;
	size_t				tx_limit;
	int ret;

	if (dma->tx_running) {
		if (up->x_char) {
			dmaengine_pause(dma->txchan);
			uart_xchar_out(up, UART_TX);
			dmaengine_resume(dma->txchan);
		}
		return 0;
	} else if (up->x_char) {
		uart_xchar_out(up, UART_TX);
	}

	if (uart_tx_stopped(&p->port) || uart_circ_empty(xmit)) {
		/* We have been called from __dma_tx_complete() */
		return 0;
	}

	tx_limit = dma->tx_burst ? dma->tx_burst : 16;
	dma->tx_size = CIRC_CNT_TO_END(xmit->head, xmit->tail, UART_XMIT_SIZE);
	dma->tx_size = min_t(unsigned int, dma->tx_size, tx_limit);

	serial8250_do_prepare_tx_dma(p);

	desc = dmaengine_prep_slave_single(dma->txchan,
					   dma->tx_addr + xmit->tail,
					   dma->tx_size, DMA_MEM_TO_DEV,
					   DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (!desc) {
		ret = -EBUSY;
		goto err;
	}

	dma->tx_running = 1;
	desc->callback = __dma_tx_complete;
	desc->callback_param = p;

	dma->tx_cookie = dmaengine_submit(desc);

	dma_sync_single_for_device(dma->txchan->device->dev, dma->tx_addr,
				   UART_XMIT_SIZE, DMA_TO_DEVICE);

	dma_async_issue_pending(dma->txchan);
	serial8250_clear_THRI(p);
	dma->tx_err = 0;

	return 0;
err:
	dma->tx_err = 1;
	return ret;
}

void serial8250_tx_dma_flush(struct uart_8250_port *p)
{
	struct uart_8250_dma *dma = p->dma;

	if (!dma->tx_running)
		return;

	/*
	 * kfifo_reset() has been called by the serial core, avoid
	 * advancing and underflowing in __dma_tx_complete().
	 */
	dma->tx_size = 0;

	/*
	 * We can't use `dmaengine_terminate_sync` because `uart_flush_buffer` is
	 * holding the uart port spinlock.
	 */
	dmaengine_terminate_async(dma->txchan);

	/*
	 * The callback might or might not run. If it doesn't run, we need to ensure
	 * that `tx_running` is cleared so that we can schedule new transactions.
	 * If it does run, then the zombie callback will clear `tx_running` again
	 * and perform a no-op since `tx_size` was cleared above.
	 *
	 * In either case, we ASSUME the DMA transaction will terminate before we
	 * issue a new `serial8250_tx_dma`.
	 */
	dma->tx_running = 0;
}

int serial8250_rx_dma_cyclic(struct uart_8250_port *p)
{
	struct uart_8250_dma		*dma = p->dma;
	struct dma_async_tx_descriptor	*desc;
	size_t				period;

	if (dma->rx_running)
		return 0;

	serial8250_do_prepare_rx_dma(p);

	period = dma->rx_period ? dma->rx_period : 16;
	if (period > dma->rx_size)
		period = dma->rx_size;
	while (period > 1 && (dma->rx_size % period))
		period >>= 1;

	desc = dmaengine_prep_dma_cyclic(dma->rxchan, dma->rx_addr,
					 dma->rx_size, period,
					 DMA_DEV_TO_MEM,
					 DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (!desc)
		return -EBUSY;

	dma->rx_running = 1;
	dma->rx_pos = 0;
	desc->callback = dma_rx_complete_cyclic;
	desc->callback_param = p;

	dma->rx_cookie = dmaengine_submit(desc);

	dma_async_issue_pending(dma->rxchan);

	return 0;
}

int serial8250_rx_dma(struct uart_8250_port *p)
{
	struct uart_8250_dma		*dma = p->dma;
	struct dma_async_tx_descriptor	*desc;

	if (dma->rx_running)
		return 0;

	serial8250_do_prepare_rx_dma(p);

	desc = dmaengine_prep_slave_single(dma->rxchan, dma->rx_addr,
					   dma->rx_size, DMA_DEV_TO_MEM,
					   DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (!desc)
		return -EBUSY;

	dma->rx_running = 1;
	desc->callback = dma_rx_complete;
	desc->callback_param = p;

	dma->rx_cookie = dmaengine_submit(desc);

	dma_async_issue_pending(dma->rxchan);

	return 0;
}

void serial8250_rx_dma_flush(struct uart_8250_port *p)
{
	struct uart_8250_dma *dma = p->dma;
	u16 status;
	int ret;

	if (!dma || !dma->rx_running)
		return;

	if (p->port.cyclic) {
		/*
		 * Cyclic RX only notifies on full periods. Character timeout
		 * means a partial period (and/or bytes still below the DMA
		 * burst watermark) is sitting in hardware. Pause the channel,
		 * push whatever already reached the ring, PIO-drain the UART
		 * FIFO, then resume cyclic RX so DMA stays enabled.
		 */
		ret = dmaengine_pause(dma->rxchan);
		if (ret) {
			/*
			 * Channel may still be draining RBR. Do not touch the
			 * FIFO via PIO or we race the DMA engine. Resume to
			 * clear PAUSED bookkeeping some providers set even on
			 * pause timeout (e.g. dw-axi-dmac -EAGAIN).
			 */
			dev_err_ratelimited(p->port.dev,
					    "cyclic RX DMA pause failed: %d\n",
					    ret);
			dmaengine_resume(dma->rxchan);
			return;
		}

		__dma_rx_complete_cyclic(p);

		status = serial_lsr_in(p);
		if (status & (UART_LSR_DR | UART_LSR_BI))
			serial8250_rx_chars(p, status);

		dmaengine_resume(dma->rxchan);
		return;
	}

	dmaengine_pause(dma->rxchan);
	__dma_rx_complete(p);
	dmaengine_terminate_async(dma->rxchan);
}
EXPORT_SYMBOL_GPL(serial8250_rx_dma_flush);

int serial8250_request_dma(struct uart_8250_port *p)
{
	struct uart_8250_dma	*dma = p->dma;
	phys_addr_t rx_dma_addr = dma->rx_dma_addr ?
				  dma->rx_dma_addr : p->port.mapbase;
	phys_addr_t tx_dma_addr = dma->tx_dma_addr ?
				  dma->tx_dma_addr : p->port.mapbase;
	dma_cap_mask_t		mask;
	struct dma_slave_caps	caps;
	int			ret;
	struct uart_port	*up = &p->port;
	/* Default slave configuration parameters */
	dma->rxconf.direction		= DMA_DEV_TO_MEM;
	dma->rxconf.src_addr_width	= DMA_SLAVE_BUSWIDTH_1_BYTE;
	dma->rxconf.src_addr		= rx_dma_addr + UART_RX;
	if (p->port.cyclic) {
		dma->rxconf.src_msize		= up->fifosize / 4;
		dma->rxconf.dst_msize		= up->fifosize / 4;
	}

	dma->txconf.direction		= DMA_MEM_TO_DEV;
	dma->txconf.dst_addr_width	= DMA_SLAVE_BUSWIDTH_1_BYTE;
	dma->txconf.dst_addr		= tx_dma_addr + UART_TX;
	dma->txconf.src_msize		= 1;
	dma->txconf.dst_msize		= 1;

	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE, mask);

	/* Get a channel for RX */
	dma->rxchan = dma_request_slave_channel_compat(mask,
						       dma->fn, dma->rx_param,
						       p->port.dev, "rx");
	if (!dma->rxchan)
		return -ENODEV;

	/* 8250 rx dma requires dmaengine driver to support pause/terminate */
	ret = dma_get_slave_caps(dma->rxchan, &caps);
	if (ret)
		goto release_rx;
	if (!caps.cmd_pause || !caps.cmd_terminate ||
	    caps.residue_granularity == DMA_RESIDUE_GRANULARITY_DESCRIPTOR) {
		ret = -EINVAL;
		goto release_rx;
	}

	dmaengine_slave_config(dma->rxchan, &dma->rxconf);

	/* Get a channel for TX */
	dma->txchan = dma_request_slave_channel_compat(mask,
						       dma->fn, dma->tx_param,
						       p->port.dev, "tx");
	if (!dma->txchan) {
		ret = -ENODEV;
		goto release_rx;
	}

	/* 8250 tx dma requires dmaengine driver to support terminate */
	ret = dma_get_slave_caps(dma->txchan, &caps);
	if (ret)
		goto err;
	if (!caps.cmd_terminate) {
		ret = -EINVAL;
		goto err;
	}

	dmaengine_slave_config(dma->txchan, &dma->txconf);

	/* RX buffer */
	if (!dma->rx_size)
		dma->rx_size = RX_DMA_SIZE;

	serial8250_dma_setup_periods(p);

	dma->rx_buf = dma_alloc_coherent(dma->rxchan->device->dev, dma->rx_size,
					&dma->rx_addr, GFP_KERNEL);
	if (!dma->rx_buf) {
		ret = -ENOMEM;
		goto err;
	}

	/* TX buffer */
	dma->tx_addr = dma_map_single(dma->txchan->device->dev,
					p->port.state->xmit.buf,
					UART_XMIT_SIZE,
					DMA_TO_DEVICE);
	if (dma_mapping_error(dma->txchan->device->dev, dma->tx_addr)) {
		dma_free_coherent(dma->rxchan->device->dev, dma->rx_size,
				  dma->rx_buf, dma->rx_addr);
		ret = -ENOMEM;
		goto err;
	}
	dma->rx_dma(p);
	if (!p->port.cyclic) {
		serial8250_rx_dma_flush(p);
	}
	dev_dbg_ratelimited(p->port.dev, "got both dma channels\n");

	return 0;
err:
	dma_release_channel(dma->txchan);
release_rx:
	dma_release_channel(dma->rxchan);
	return ret;
}
EXPORT_SYMBOL_GPL(serial8250_request_dma);

void serial8250_release_dma(struct uart_8250_port *p)
{
	struct uart_8250_dma *dma = p->dma;

	if (!dma)
		return;

	/* Release RX resources */
	dmaengine_terminate_sync(dma->rxchan);
	if (p->port.cyclic)
		dma->rx_running = 0;
	dma_free_coherent(dma->rxchan->device->dev, dma->rx_size, dma->rx_buf,
			  dma->rx_addr);
	dma_release_channel(dma->rxchan);
	dma->rxchan = NULL;

	/* Release TX resources */
	dmaengine_terminate_sync(dma->txchan);
	dma_unmap_single(dma->txchan->device->dev, dma->tx_addr,
			 UART_XMIT_SIZE, DMA_TO_DEVICE);
	dma_release_channel(dma->txchan);
	dma->txchan = NULL;
	dma->tx_running = 0;

	dev_dbg_ratelimited(p->port.dev, "dma channels released\n");
}
EXPORT_SYMBOL_GPL(serial8250_release_dma);
