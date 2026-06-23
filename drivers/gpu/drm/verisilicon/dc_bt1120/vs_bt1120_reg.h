/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024, VeriSilicon Holdings Co., Ltd. All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __BT1120_REG_H__
#define __BT1120_REG_H__

#define REG_BT1120_CTL		   0x00
#define REG_BT1120_DMA_BLENGTH_CTL 0x04
#define REG_BT1120_LINE_OFFSET_CTL 0x08
#define REG_BT1120_WORD_OFFSET_CTL 0x0C

#define REG_BT1120_IMG_IN_BADDR_Y_CTL  0x10
#define REG_BT1120_IMG_IN_BADDR_UV_CTL 0x14
#define REG_BT1120_IMG_PIX_HSIZE_CTL   0x18
#define REG_BT1120_IMG_PIX_VSIZE_CTL   0x1C
#define REG_BT1120_IMG_PIX_HSTRIDE_CTL 0x20
#define REG_BT1120_IMG_PIX_ZONE_CTL    0x24

#define REG_BT1120_HSYNC_ZONE_CTL  0x28
#define REG_BT1120_VSYNC_ZONE_CTL  0x2C
#define REG_BT1120_DISP_WIDTH_CTL  0x30
#define REG_BT1120_DISP_HEIGHT_CTL 0x34
#define REG_BT1120_DISP_XZONE_CTL  0x38
#define REG_BT1120_DISP_YZONE_CTL  0x3C

#define REG_BT1120_HFP_RANGE_CTL	 0x40
#define REG_BT1120_SCAN_FORMAT_CTL	 0x44
#define REG_BT1120_INT_HEIGHT_OFFSET_CTL 0x48
#define REG_BT1120_DDR_STORE_FORMAT_CTL	 0x4C

/*
 * Interrupt registers — Horizon X5 BT1120 Specification v0.5 §1.10.21 / §1.10.22.
 *   BT1120_IRQ_EN     offset 0x0050 (fs_irq_en, dma_done_irq_en, buf_underflow_irq_en,
 *				     online_mismatch_irq_en in bits 0–3).
 *   BT1120_IRQ_STATUS offset 0x0054 (sticky status; clear by writing 1 to the bit,
 *				     see vs_bt1120 interrupt handler).
 * Note: The document TOC lists BT1120_IRQ_* at 0x2c/0x30/0x34 — those offsets clash with
 * VSYNC_ZONE_CTL (0x2c), DISP_WIDTH (0x30) in the same spec register table; use 0x50/0x54.
 */
#define REG_BT1120_IRQ_EN_CTL 0x50
#define REG_BT1120_IRQ_STATUS 0x54

#define REG_BT1120_CSC_COEFF	 0x58
#define REG_BT1120_CSC_COEFF_CNT 0xc

#define REG_BT1120_OUTPUT_CRC_FRAME0 0x88
/** OUTPUT_CRC: spec table 0x0088–0x00A4, eight 32-bit words (frame CRC). */
#define REG_BT1120_OUTPUT_CRC_COUNT    8

/** REG_BT1120_CTL. Image format in DDR.
 * 00: YUV422 packed mode.
 * 01: YUV422 semi-planar.
 * 10: YUV420 semi-planar.
 */
#define IMG_FORMAT_SHIFT 1
#define IMG_FORMAT_MASK	 GENMASK(2, 1)

/** REG_BT1120_IMG_PIX_HSTRIDE_CTL.
 * Image horizontal hstride size . Unit is word(8byte,AXI width).(offline).
 */
#define HSTRIDE_UNIT_SIZE 8

/** REG_BT1120_IMG_PIX_ZONE_CTL. bt1120 image pixel location y coordinate(offline). */
#define PIX_Y_SHIFT 16
/** REG_BT1120_HSYNC_ZONE_CTL. bt1120 hsync stop location(offline). */
#define HSYNC_STOP_SHIFT 16
/** REG_BT1120_VSYNC_ZONE_CTL. bt1120 vsync stop location(offline). */
#define VSYNC_STOP_SHIFT 16

/** REG_BT1120_DISP_XZONE_CTL. Display active zone horizontal stop pixel address.
 * (min pixel address is 1)(offline&online).
 */
#define XSTOP_SHIFT 16
/** REG_BT1120_DISP_YZONE_CTL. Display active zone vertical stop pixel address.
 * (min pixel address is 1)(offline&online).
 */
#define YSTOP_SHIFT 16

/** REG_BT1120_IRQ_EN / REG_BT1120_IRQ_STATUS bit 0 — spec fs_irq_en / fs_irq_status */
#define FRAME_START_IRQ_STATUS_MASK BIT(0)
#define FRAME_START_IRQ_EN_MASK		FRAME_START_IRQ_STATUS_MASK

/** Bit 1 — dma_done_irq_en / dma_done_irq_status */
#define DMA_DONE_IRQ_STATUS_MASK BIT(1)
#define DMA_DONE_IRQ_EN_MASK		DMA_DONE_IRQ_STATUS_MASK

/** Bit 2 — buf_underflow_irq_en / buf_underflow_irq_status */
#define BUF_UNDERFLOW_IRQ_STATUS_MASK BIT(2)
#define BUF_UNDERFLOW_IRQ_EN_MASK	BUF_UNDERFLOW_IRQ_STATUS_MASK

/** Bit 3 — online_mismatch_irq_en / online_mismatch_irq_status */
#define ONLINE_MISMATCH_IRQ_STATUS_MASK BIT(3)
#define ONLINE_MISMATCH_IRQ_EN_MASK	ONLINE_MISMATCH_IRQ_STATUS_MASK

/** REG_BT1120_DDR_STORE_FORMAT_CTL.
 * The y store format in DDR, used in packed mode.
 */
#define Y_STORE_FORMAT_SHIFT 1

#endif /* __BT1120_REG_H__ */
