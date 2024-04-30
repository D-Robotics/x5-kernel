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

#ifndef __VS_SIF_REG_H__
#define __VS_SIF_REG_H__

#define SIF_DMA_CTL		    0
#define IPI0_MIPI_PIXEL_TYPE_MASK   GENMASK(3, 1)
#define IPI0_MIPI_DMA_EN	    BIT(0)
#define IPI0_MIPI_DMA_CONFIG_UPDATE BIT(6)
#define IPI0_IMG_OUT_BADDR_Y	    0x4
#define IPI0_IMG_OUT_BADDR_UV	    0x8
#define IPI0_IMG_OUT_PIX_HSIZE	    0xc
#define IPI0_IMG_OUT_PIX_VSIZE	    0x10
#define IPI0_IMG_OUT_PIX_HSTRIDE    0x14
#define IPI0_IRQ_EN		    0x44
#define IPI0_IRQ_CLR		    0x48
#define IPI0_IRQ_STATUS		    0x4c

/** @} */

#endif /* __VS_SIF_H__ */
