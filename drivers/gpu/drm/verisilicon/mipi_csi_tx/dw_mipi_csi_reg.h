/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __DW_MIPI_CSI_REG_H_
#define __DW_MIPI_CSI_REG_H_

#define CSI_VERSION 0x00

#define CSI_RESETN	    0x04
#define RESET		    0
#define POWERUP		    BIT(0)
#define CSI_DATA_SCRAMBLING 0x08

#define CSI_INT_ST_MAIN	 0x20
#define CSI_INT_ST_VPG	 0x24
#define CSI_INT_ST_IDI	 0x28
#define CSI_INT_ST_IPI	 0x2c
#define CSI_INT_ST_PHY	 0x30
#define CSI_INT_MASK_VPG 0x40
#define CSI_INT_MASK_IDI 0x48
#define CSI_INT_MASK_IPI 0x50
#define CSI_INT_MASK_PHY 0x58

#define CSI_VPG_CTRL	       0x80
#define VPG_EN		       BIT(0)
#define CSI_VPG_STATUS	       0x84
#define CSI_VPG_MODE_CFG       0x88
#define VPG_ORIENTATION	       BIT(16)
#define VPG_MODE	       BIT(0)
#define CSI_VPG_PKT_CFG	       0x8c
#define CSI_VPG_PKT_SIZE       0x90
#define VPG_PKT_SIZE(p)	       ((p) & 0x3fff)
#define CSI_VPG_HSA_TIME       0x94
#define CSI_VPG_HBP_TIME       0x98
#define CSI_VPG_HLINE_TIME     0x9c
#define CSI_VPG_VSA_LINES      0xa0
#define CSI_VPG_VBP_LINES      0xa4
#define CSI_VPG_VFP_LINES      0xa8
#define CSI_VPG_ACT_LINES      0xac
#define CSI_VPG_MAX_FRAME_NUM  0xb0
#define CSI_VPG_START_LINE_NUM 0xb4
#define CSI_VPG_STEP_LINE_NUM  0xb8
#define CSI_VPG_BK_LINES       0xbc

#define CSI_PHY_RSTZ	    0xe0
#define PHY_FORCETXSTOPMODE BIT(4)
#define PHY_DISFORCEPLL	    0
#define PHY_ENFORCEPLL	    BIT(3)
#define PHY_DISABLECLK	    0
#define PHY_ENABLECLK	    BIT(2)
#define PHY_RSTZ	    0
#define PHY_UNRSTZ	    BIT(1)
#define PHY_SHUTDOWNZ	    0
#define PHY_UNSHUTDOWNZ	    BIT(0)

#define CSI_PHY_IF_CFG		  0xe4
#define PHY_STOP_WAIT_TIME(cycle) (((cycle) & 0xff) << 8)
#define CPHY_ALP_EN		  BIT(4)
#define N_LANES(n)		  (((n) - 1) & 0x7)

#define CSI_LPCLK_CTRL	       0xe8
#define PHY_TXREQUESTCLKHS_CON BIT(0)

#define CSI_PHY_ULPS_CTRL	 0xec
#define CSI_CLKMGR_CFG		 0xf0
#define TO_CLK_DIVISION(div)	 (((div) & 0xff) << 8)
#define TX_ESC_CLK_DIVISION(div) ((div) & 0xff)

#define CSI_PHY_CAL    0xf8
#define PHY_CAL	       BIT(0)
#define CSI_TO_CNT_CFG 0xfc
#define HSTX_TO_CNT(p) ((p) & 0xffff)

#define CSI_LPCLK_CTRL_CONTINUOUS     0x1 /* Bit to enable continuous clock mode */
#define CSI_LPCLK_CTRL_NON_CONTINUOUS 0x0 /* Bit to disable continuous clock mode */
#define CSI_PHY_SWITCH_TIME	      0x104
#define PHY_LP2HS_TIME(lbcc)	      (((lbcc) & 0x3ff) << 16)
#define PHY_HS2LP_TIME(lbcc)	      ((lbcc) & 0x3ff)

#define CSI_PHY_STATUS		0x110
#define PHY_STOP_STATE_L3_LANE	BIT(12)
#define PHY_STOP_STATE_L2_LANE	BIT(10)
#define PHY_STOP_STATE_L1_LANE	BIT(8)
#define PHY_STOP_STATE_L0_LANE	BIT(6)
#define PHY_STOP_STATE_CLK_LANE BIT(4)
#define PHY_STOP_STATE                                                               \
	(PHY_STOP_STATE_CLK_LANE | PHY_STOP_STATE_L0_LANE | PHY_STOP_STATE_L1_LANE | \
	 PHY_STOP_STATE_L2_LANE | PHY_STOP_STATE_L3_LANE)
#define PHY_LOCK BIT(3)

#define CSI_IPI_PKG_CFG		 0x140
#define CSI_IPI_PIXELS		 0x144
#define CSI_IPI_MAX_FRAME_NUM	 0x148
#define CSI_IPI_START_LINE_NUM	 0x14c
#define CSI_IPI_STEP_LINE_NUM	 0x150
#define CSI_IPI_LINES		 0x154
#define CSI_IPI_DATA_SEND_START	 0x158
#define CSI_IPI_HSA_HBP_PPI_TIME 0x164
#define CSI_IPI_HLINE_PPI_TIME	 0x168
#define CSI_IPI_VSA_LINES	 0x16c
#define CSI_IPI_VBP_LINES	 0x170
#define CSI_IPI_VFP_LINES	 0x174
#define CSI_IPI_ACT_LINES	 0x178
#define CSI_IPI_FB_LINES	 0x17c
#define CSI_IPI_INSERT_CTRL	 0x184

#define TO_CLK_DIV 10

#define PHY_STATUS_TIMEOUT_US	  100000
#define CMD_PKT_STATUS_TIMEOUT_US 20000

#define MAX_LANE_COUNT 4

#define CSI_IPI_MODE_MASK	    GENMASK(17, 16)
#define CSI_IPI_VC_EXT_MASK	    GENMASK(14, 12)
#define CSI_IPI_FRAME_NUM_MODE_MASK BIT(11)
#define CSI_IPI_LINE_NUM_MODE_MASK  GENMASK(10, 9)
#define CSI_IPI_HSYNC_PKT_EN_MASK   BIT(8)
#define CSI_IPI_VC_MASK		    GENMASK(7, 6)
#define CSI_IPI_DATA_TYPE_MASK	    GENMASK(5, 0)

#define CSI_VPG_VC_EXT_MASK	    GENMASK(14, 12)
#define CSI_VPG_FRAME_NUM_MODE_MASK BIT(11)
#define CSI_VPG_LINE_NUM_MODE_MASK  GENMASK(10, 9)
#define CSI_VPG_HSYNC_PKT_EN_MASK   BIT(8)
#define CSI_VPG_VC_MASK		    GENMASK(7, 6)
#define CSI_VPG_DATA_TYPE_MASK	    GENMASK(5, 0)

#define CSI_INT_ST_MT_IPI 0x3C

/* INT_ST_VPG bits */
#define CSI_INT_VPG_PKT_LOST BIT(0)

/* INT_ST_IDI bits */
#define CSI_INT_IDI_FIFO_OVERFLOW BIT(9)
#define CSI_INT_IDI_VC3_ERRL_SEQ  BIT(8)
#define CSI_INT_IDI_VC2_ERRL_SEQ  BIT(7)
#define CSI_INT_IDI_VC1_ERRL_SEQ  BIT(6)
#define CSI_INT_IDI_VC0_ERRL_SEQ  BIT(5)
#define CSI_INT_IDI_VC3_ERRF_SEQ  BIT(4)
#define CSI_INT_IDI_VC2_ERRF_SEQ  BIT(3)
#define CSI_INT_IDI_VC1_ERRF_SEQ  BIT(2)
#define CSI_INT_IDI_VC0_ERRF_SEQ  BIT(1)
#define CSI_INT_IDI_ERRWC	  BIT(0)

/* INT_ST_IPI bits */

/* IPI0: bits [4:0] */
#define CSI_INT_IPI0_ERRPIXEL	    BIT(0)
#define CSI_INT_IPI0_FIFO_OVERFLOW  BIT(1)
#define CSI_INT_IPI0_ERRLINE	    BIT(2)
#define CSI_INT_IPI0_FIFO_UNDERFLOW BIT(3)
#define CSI_INT_IPI0_TRANS_CONFLICT BIT(4)

/* IPI2: bits [11:8] */
#define CSI_INT_IPI2_ERRPIXEL	    BIT(8)
#define CSI_INT_IPI2_FIFO_OVERFLOW  BIT(9)
#define CSI_INT_IPI2_ERRLINE	    BIT(10)
#define CSI_INT_IPI2_FIFO_UNDERFLOW BIT(11)

/* IPI3: bits [16:19] */
#define CSI_INT_IPI3_ERRPIXEL	    BIT(16)
#define CSI_INT_IPI3_FIFO_OVERFLOW  BIT(17)
#define CSI_INT_IPI3_ERRLINE	    BIT(18)
#define CSI_INT_IPI3_FIFO_UNDERFLOW BIT(19)

/* IPI4: bits [24:27] */
#define CSI_INT_IPI4_ERRPIXEL	    BIT(24)
#define CSI_INT_IPI4_FIFO_OVERFLOW  BIT(25)
#define CSI_INT_IPI4_ERRLINE	    BIT(26)
#define CSI_INT_IPI4_FIFO_UNDERFLOW BIT(27)

/* INT_ST_PHY bits */
#define CSI_INT_PHY_TO_HS_TX BIT(0)
#define CSI_INT_PHY_ERR_LP0  BIT(1)
#define CSI_INT_PHY_ERR_LP1  BIT(2)

#define CSI_PHY0_TST_CTRL0 0x114
#define CSI_PHY0_TST_CTRL1 0x118

#define PHY_TESTCLR_BIT	   BIT(0)
#define PHY_TESTCLK_BIT	   BIT(1)
#define PHY_TESTEN_BIT	   BIT(16)
#define PHY_TESTDIN_MASK   GENMASK(7, 0)
#define PHY_TESTDOUT_MASK  GENMASK(15, 8)
#define PHY_TESTDOUT_SHIFT 8

#endif /* __DW_MIPI_CSI_DEBUGFS_H_ */
