/*
 * Horizon Robotics
 *
 *  Copyright (C) 2020 Horizon Robotics Inc.
 *  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __HOBOT_TSN_H
#define __HOBOT_TSN_H

#define MIN_AVB_QUEUES 3

#define AVB_CLASSA_Q 3
#define AVB_CLASSB_Q 2
#define AVB_PTPCP_Q 1
#define AVB_BEST_EFF_Q 0

#define AVB_CLASSA_BW 40
#define AVB_CLASSB_BW 20

#define AVB_CBS_SCALE 1024

#define PORT_RATE_SGMII 8
#define PORT_RATE_GMII 4

#define ETHNORMAL_LEN 1500

#define GMAC_HW_FEATURES3 0x00000128
#define GMAC_FPE_CTRL_STS 0x00000234

#define GMAC_HW_FEAT_TBSSEL BIT(27)
#define GMAC_HW_FEAT_FPESEL BIT(26)
#define GMAC_HW_FEAT_ESTSEL BIT(16)

#define GMAC_FPE_EFPE BIT(0)

#define MTL_EST_CONTROL 0x00000c50
#define MTL_EST_PTOV GENMASK(31, 24)
#define MTL_EST_PTOV_OFFSET 24
#define MTL_EST_SSWL BIT(1)
#define MTL_EST_EEST BIT(0)

#define MTL_EST_STATUS 0x00000c58
#define MTL_EST_SWOL BIT(7)

#define MTL_ESTIS BIT(18)
#define MTL_STATUS_CGSN GENMASK(19, 16)
#define MTL_STATUS_BTRL GENMASK(11, 8)
#define MTL_STATUS_SWOL BIT(7)
#define MTL_STATUS_CGCE BIT(4)
#define MTL_STATUS_HLBS BIT(3)
#define MTL_STATUS_HLBF BIT(2)
#define MTL_STATUS_BTRE BIT(1)
#define MTL_STATUS_SWLC BIT(0)

#define MTL_EST_GCL_CONTROL 0x00000c80
#define MTL_EST_ADDR GENMASK(19, 8)
#define MTL_EST_ADDR_OFFSET 8
#define MTL_EST_BTR_LOW (0x0 << 8)
#define MTL_EST_BTR_HIGH (0x1 << 8)
#define MTL_EST_CTR_LOW (0x02 << 8)
#define MTL_EST_CTR_HIGH (0x3 << 8)
#define MTL_EST_TER (0x4 << 8)
#define MTL_EST_LLR (0x05 << 8)
#define MTL_EST_GCRR BIT(2)
#define MTL_EST_R1WO BIT(1)
#define MTL_EST_SRWO BIT(0)

#define MTL_EST_GCL_DATA 0x00000c84

#define MTL_FPE_CTRL_STS 0x00000c90
#define MTL_FPE_PEC_MASK GENMASK(15, 8)
#define MTL_FPE_PEC_SHIFT 8

#define MTL_FPE_Advance 0x00000c94

#define MTL_OP_MODE_TXQEN_MASK GENMASK(3, 2)
#define MTL_OP_MODE_TXQEN_AV BIT(2)

#define MTL_EST_Frm_Size_Error 0x00000C64
#define MTL_EST_Frm_Size_Capture 0x00000C68

#define PPSCTRL_PPSCMD GENMASK(3, 0)
#define TTSL0 GENMASK(30, 0)
#define MAC_PPS_CONTROL 0x00000b70

#define MCGREN3 BIT(31)
#define TRGTMODSEL3 BIT(30, 29)
#define PPSCMD3 GENMASK(27, 24)
#define MCGREN2 BIT(23)
#define TRGTMODSEL2 GENMASK(22, 21)
#define PPSCMD2 GENMASK(19, 16)
#define MCGREN1 BIT(15)
#define TRGTMODSEL1 GENMASK(14, 13)
#define PPSCMD1 GENMASK(11, 8)
#define MCRGEN0 BIT(7)
#define TRGTMODSEL0 GENMASK(6, 5)
#define PPSEN0 BIT(4)

#define TRGTBUSY0 BIT(31)

#define PTOV GENMASK(31, 24)

void xj3_init_tsn(struct net_device *ndev);
int xj3_tsn_capable(struct net_device *ndev);
int xj3_tsn_link_configure(struct net_device *ndev, enum sr_class class,
                           u16 framesize, u16 vid, u8 add_link, u8 pcp_hi,
                           u8 pcp_lo);

u16 xj3_tsn_select_queue(struct net_device *ndev, struct sk_buff *skb,
                         struct net_device *sb_dev);

#endif
