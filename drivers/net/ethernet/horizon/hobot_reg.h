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

#ifndef __HOBOT_REG_H_
#define __HOBOT_REG_H_/*PRQA S 0603*/

#include <linux/bitops.h>
#include <linux/kernel.h>



#if IS_ENABLED(CONFIG_VLAN_8021Q)
#include <linux/if_vlan.h>
#endif

#define JUMBO_LEN	9000

/* DAM HW feature register fields */
#define DMA_HW_FEAT_MIISEL	0x00000001	/* 10/100 Mbps Support */
#define DMA_HW_FEAT_GMIISEL	0x00000002	/* 1000 Mbps Support */
#define DMA_HW_FEAT_HDSEL	0x00000004	/* Half-Duplex Support */
#define DMA_HW_FEAT_EXTHASHEN	0x00000008	/* Expanded DA Hash Filter */
#define DMA_HW_FEAT_HASHSEL	0x00000010	/* HASH Filter */
#define DMA_HW_FEAT_ADDMAC	0x00000020	/* Multiple MAC Addr Reg */
#define DMA_HW_FEAT_PCSSEL	0x00000040	/* PCS registers */
#define DMA_HW_FEAT_L3L4FLTREN	0x00000080	/* Layer 3 & Layer 4 Feature */
#define DMA_HW_FEAT_SMASEL	0x00000100	/* SMA(MDIO) Interface */
#define DMA_HW_FEAT_RWKSEL	0x00000200	/* PMT Remote Wakeup */
#define DMA_HW_FEAT_MGKSEL	0x00000400	/* PMT Magic Packet */
#define DMA_HW_FEAT_MMCSEL	0x00000800	/* RMON Module */
#define DMA_HW_FEAT_TSVER1SEL	0x00001000	/* Only IEEE 1588-2002 */
#define DMA_HW_FEAT_TSVER2SEL	0x00002000	/* IEEE 1588-2008 PTPv2 */
#define DMA_HW_FEAT_EEESEL	0x00004000	/* Energy Efficient Ethernet */
#define DMA_HW_FEAT_AVSEL	0x00008000	/* AV Feature */
#define DMA_HW_FEAT_TXCOESEL	0x00010000	/* Checksum Offload in Tx */
#define DMA_HW_FEAT_RXTYP1COE	0x00020000	/* IP COE (Type 1) in Rx */
#define DMA_HW_FEAT_RXTYP2COE	0x00040000	/* IP COE (Type 2) in Rx */
#define DMA_HW_FEAT_RXFIFOSIZE	0x00080000	/* Rx FIFO > 2048 Bytes */


#define DMA_HW_FEAT_RXCHCNT	0x00300000	/* No. additional Rx Channels */
#define DMA_HW_FEAT_TXCHCNT	0x00c00000	/* No. additional Tx Channels */
#define DMA_HW_FEAT_ENHDESSEL	0x01000000	/* Alternate Descriptor */
/* Timestamping with Internal System Time */
#define DMA_HW_FEAT_INTTSEN	0x02000000
#define DMA_HW_FEAT_FLEXIPPSEN	0x04000000	/* Flexible PPS Output */
#define DMA_HW_FEAT_SAVLANINS	0x08000000	/* Source Addr or VLAN */
#define DMA_HW_FEAT_ACTPHYIF	0x70000000	/* Active/selected PHY iface */
#define DEFAULT_DMA_PBL		8




/* MAC HW features0 bitmap */
#define GMAC_HW_FEAT_ADDMAC		BIT(18)
#define GMAC_HW_FEAT_RXCOESEL		BIT(16)
#define GMAC_HW_FEAT_TXCOSEL		BIT(14)
#define GMAC_HW_FEAT_EEESEL		BIT(13)
#define GMAC_HW_FEAT_TSSEL		BIT(12)
#define GMAC_HW_FEAT_MMCSEL		BIT(8)
#define GMAC_HW_FEAT_MGKSEL		BIT(7)
#define GMAC_HW_FEAT_RWKSEL		BIT(6)
#define GMAC_HW_FEAT_SMASEL		BIT(5)
#define GMAC_HW_FEAT_VLHASH		BIT(4)
#define GMAC_HW_FEAT_PCSSEL		BIT(3)
#define GMAC_HW_FEAT_HDSEL		BIT(2)
#define GMAC_HW_FEAT_GMIISEL		BIT(1)
#define GMAC_HW_FEAT_MIISEL		BIT(0)
/* MAC HW features1 bitmap */
#define GMAC_HW_FEAT_AVSEL		BIT(20)
#define GMAC_HW_TSOEN			BIT(18)
#define GMAC_HW_TXFIFOSIZE		0x7c0U//GENMASK(10, 6)
#define GMAC_HW_RXFIFOSIZE		0x1fU//GENMASK(4, 0)

/* MAC HW features2 bitmap */
#define GMAC_HW_FEAT_TXCHCNT		0x3c0000U//GENMASK(21, 18)
#define GMAC_HW_FEAT_RXCHCNT		0xf000U//GENMASK(15, 12)
#define GMAC_HW_FEAT_TXQCNT		0x3c0U//GENMASK(9, 6)
#define GMAC_HW_FEAT_RXQCNT		0xfU//GENMASK(3, 0)
#define GMAC_HW_FEAT_PPSOUTNUM		0x7000000U//GENMASK(26,24)



/*MAC HW features3 bitmap*/
#define GMAC_HW_FEAT_TBSSEL BIT(27)
#define GMAC_HW_FEAT_FPESEL BIT(26)
#define GMAC_HW_FEAT_ESTWID		0x300000U//GENMASK(21,20)
#define GMAC_HW_FEAT_ESTSEL		BIT(16)
#define GMAC_HW_FEAT_ESTDEP		0xe0000U//GENMASK(19,17)
#define	GMAC_HW_FEAT_ASP			0x30000000U//GENMASK(29,28)
#define	GMAC_HW_FEAT_FRPSEL		BIT(10)
#define	GMAC_HW_FEAT_FRPES		0x6000U//GENMASK(14,13)


enum dwmac4_irq_status {
	time_stamp_irq = 0x00001000,
	mmc_rx_csum_offload_irq = 0x00000800,
	mmc_tx_irq = 0x00000400,
	mmc_rx_irq = 0x00000200,
	mmc_irq = 0x00000100,
	pmt_irq = 0x00000010,
	mac_fpeis = 0x00020000,
};


enum packets_types {
	PACKET_AVCPQ = 0x1, /* AV Untagged Control packets */
	PACKET_PTPQ = 0x2, /* PTP Packets */
	PACKET_DCBCPQ = 0x3, /* DCB Control Packets */
	PACKET_UPQ = 0x4, /* Untagged Packets */
	PACKET_MCBCQ = 0x5, /* Multicast & Broadcast Packets */
};

#define MTL_TX_ALGORITHM_WRR	0x0
#define MTL_TX_ALGORITHM_WFQ	0x1
#define MTL_TX_ALGORITHM_DWRR	0x2
#define MTL_TX_ALGORITHM_SP	0x3
#define MTL_RX_ALGORITHM_SP	0x4
#define MTL_RX_ALGORITHM_WSP	0x5

/* RX/TX Queue Mode */
#define MTL_QUEUE_AVB		0x0
#define MTL_QUEUE_DCB		0x1


#define MTL_MAX_RX_QUEUES	8
#define MTL_MAX_TX_QUEUES	8
#define MTL_MAX_QUEUES	4U

#define REG_DWCEQOS_MAC_MDIO_ADDR        0x0200
#define REG_DWCEQOS_MAC_MDIO_DATA        0x0204
#define EQOS_MAC_MDIO_ADDR_C45E    BIT(1)
#define DWCEQOS_MDIO_PHYADDR(x)     (((x) & 0x1fU) << 21U)
#define DWCEQOS_MDIO_PHYREG(x)      (((x) & 0x1fU) << 16U)
#define DWCEQOS_MDIO_PHYREG_DEV(x)      ((x) & 0x1f0000U)
#define DWCEQOS_MAC_MDIO_ADDR_CR(x)      (((x) & 15U) << 8U)
#define DWCEQOS_MAC_MDIO_ADDR_GOC_READ   0x0000000cU
#define DWCEQOS_MAC_MDIO_ADDR_GOC_WRITE  BIT(2)
#define DWCEQOS_MAC_MDIO_ADDR_GB         BIT(0)
#define DWCEQOS_MAC_MDIO_DATA_RA(x)      (((x & 0xffff)) << 16)


#define MAC_MDIO_ADDR_CR_DIV_16      2
#define MAC_MDIO_ADDR_CR_DIV_26      3
#define MAC_MDIO_ADDR_CR_DIV_42      0
#define MAC_MDIO_ADDR_CR_DIV_62     1
#define MAC_MDIO_ADDR_CR_DIV_102     4
#define MAC_MDIO_ADDR_CR_DIV_124     5
#define MAC_MDIO_ADDR_CR_DIV_204     6
#define MAC_MDIO_ADDR_CR_DIV_324     7


#define DEFAULT_DMA_PBL			8


#define SF_DMA_MODE 1


#define GMAC_PCS_BASE			0x000000e0
#define GMAC_PMT			0x000000c0
#define GMAC_PHYIF_CONTROL_STATUS	0x000000f8
#define GMAC_INT_STATUS			0x000000b0



#define GMAC_HW_FEATURE0		0x0000011c
#define GMAC_HW_FEATURE1		0x00000120
#define GMAC_HW_FEATURE2		0x00000124
#define GMAC_HW_FEATURE3		0x00000128


#define STMMAC_RX_COE_NONE	0
#define STMMAC_RX_COE_TYPE1	1
#define STMMAC_RX_COE_TYPE2	2


#define DWCEQOS_MAC_MAC_ADDR_HI_EN       BIT(31)

#define DWCEQOS_TX_TIMEOUT 5

#define DMA_TX_SIZE 512
#define DMA_RX_SIZE 512


#define BUF_SIZE_16KiB 16384
#define BUF_SIZE_8KiB 8192
#define BUF_SIZE_4KiB 4096
#define BUF_SIZE_2KiB 2048

#define	DEFAULT_BUFSIZE	2048


#define GMAC_ADDR_HIGH(reg)		(0x300U + ((reg) * 8U))
#define GMAC_ADDR_LOW(reg)		(0x304U + ((reg) * 8U))



#define REG_DWCEQOS_MAC_RX_FLOW_CTRL     0x0090
#define DWCEQOS_MAC_RX_FLOW_CTRL_RFE BIT(0)
#define REG_DWCEQOS_MTL_RXQ0_OPER        0x0d30
#define DWCEQOS_MTL_RXQ_EHFC             BIT(7)
#define REG_DWCEQOS_MAC_LPI_CTRL_STATUS  0x00d0

#define DWCEQOS_MAC_LPI_CTRL_STATUS_PLS     BIT(17)

#define RDES3_OWN			BIT(31)
#define RDES3_BUFFER1_VALID_ADDR	BIT(24)
#define RDES3_INT_ON_COMPLETION_EN	BIT(30)


#define DMA_BUS_MODE			0x00001000
#define DMA_BUS_MODE_SFT_RESET		BIT(0)
#define DMA_SYS_BUS_MODE		0x00001004
#define DMA_BUS_MODE_FB			BIT(0)
#define DMA_BUS_MODE_MB			BIT(14)
#define DMA_SYS_BUS_AAL			BIT(12)
#define DMA_SYS_BUS_EAME		BIT(11)
#define DMA_BUS_MODE_PBL_SHIFT		16
#define DMA_BUS_MODE_RPBL_SHIFT		16
#define DMA_BUS_MODE_PBL		BIT(16)
#define DMA_BUS_INTM			GENMASK(17,16)

#define DMA_SYS_BUS_FB			BIT(0)
#define DMA_SYS_BUS_MB			BIT(14)

#define DMA_AXI_WR_OSR_LMT		GENMASK(27, 24)
#define DMA_AXI_RD_OSR_LMT		GENMASK(19, 16)
#define DMA_AXI_BLEN256			BIT(7)
#define DMA_AXI_BLEN128			BIT(6)
#define DMA_AXI_BLEN64			BIT(5)
#define DMA_AXI_BLEN32			BIT(4)
#define DMA_AXI_BLEN16			BIT(3)
#define DMA_AXI_BLEN8			BIT(2)
#define DMA_AXI_BLEN4			BIT(1)

/* DMA Tx Channel X Control register defines */
#define DMA_CONTROL_TSE			BIT(12)
#define DMA_AXI_EN_LPI			BIT(31)
#define DMA_AXI_LPI_XIT_FRM		BIT(30)

/* CSR Frequency Access Defines*/
#define CSR_F_35M	35000000
#define CSR_F_60M	60000000
#define CSR_F_100M	100000000
#define CSR_F_150M	150000000
#define CSR_F_250M	250000000
#define CSR_F_300M	300000000
#define CSR_F_500M	500000000
#define CSR_F_800M	800000000

#define	MAC_CSR_H_FRQ_MASK	0x20


/* MDC Clock Selection define*/
#define STMMAC_CSR_60_100M      0x0     /* MDC = clk_scr_i/42 */
#define STMMAC_CSR_100_150M     0x1     /* MDC = clk_scr_i/62 */
#define STMMAC_CSR_20_35M       0x2     /* MDC = clk_scr_i/16 */
#define STMMAC_CSR_35_60M       0x3     /* MDC = clk_scr_i/26 */
#define STMMAC_CSR_150_250M     0x4     /* MDC = clk_scr_i/102 */
#define STMMAC_CSR_250_300M     0x5     /* MDC = clk_scr_i/122 */






#define DMA_CHAN_BASE_ADDR		0x00001100
#define DMA_CHAN_BASE_OFFSET		0x80
#define DMA_CHANX_BASE_ADDR(x)		(DMA_CHAN_BASE_ADDR + \
					(x * DMA_CHAN_BASE_OFFSET))


#define DMA_CHAN_TX_BASE_HADDR(x)	(DMA_CHANX_BASE_ADDR(x) + 0x10)
#define DMA_CHAN_TX_BASE_ADDR(x)	(DMA_CHANX_BASE_ADDR(x) + 0x14)
#define DMA_CHAN_RX_BASE_HADDR(x)	(DMA_CHANX_BASE_ADDR(x) + 0x18)
#define DMA_CHAN_RX_BASE_ADDR(x)	(DMA_CHANX_BASE_ADDR(x) + 0x1c)

#define DMA_CHAN_RX_CONTROL(x)		(DMA_CHANX_BASE_ADDR(x) + 0x8)
#define DMA_CHAN_TX_CONTROL(x)		(DMA_CHANX_BASE_ADDR(x) + 0x4)

#define DMA_CHAN_RX_END_ADDR(x)		(DMA_CHANX_BASE_ADDR(x) + 0x28)

#define DMA_CHAN_TX_END_ADDR(x)		(DMA_CHANX_BASE_ADDR(x) + 0x20)

#define DMA_CHAN_CONTROL(x)		DMA_CHANX_BASE_ADDR(x)
#define DMA_CHAN_INTR_ENA(x)		(DMA_CHANX_BASE_ADDR(x) + 0x34)

#define DMA_CHAN_TX_RING_LEN(x)		(DMA_CHANX_BASE_ADDR(x) + 0x2c)
#define DMA_CHAN_RX_RING_LEN(x)		(DMA_CHANX_BASE_ADDR(x) + 0x30)
#define DMA_CHAN_STATUS(x)		(DMA_CHANX_BASE_ADDR(x) + 0x60)
#define DMA_CHAN_STATUS_MASK		0x3fffc7

#define DMA_CHAN_RX_WATCHDOG(x)		(DMA_CHANX_BASE_ADDR(x) + 0x38)

#define DMA_CHAN_SLOT_CTRL_STATUS(x)	(DMA_CHANX_BASE_ADDR(x) + 0x3c)
#define DMA_CHAN_CUR_TX_DESC(x)		(DMA_CHANX_BASE_ADDR(x) + 0x44)
#define DMA_CHAN_CUR_RX_DESC(x)		(DMA_CHANX_BASE_ADDR(x) + 0x4c)
#define DMA_CHAN_CUR_TX_BUF_ADDR(x)	(DMA_CHANX_BASE_ADDR(x) + 0x54)
#define DMA_CHAN_CUR_RX_BUF_ADDR(x)	(DMA_CHANX_BASE_ADDR(x) + 0x5c)

#define DMA_CHANX_SLOT_STATUS_BASE_ADDR	0x0000113C
#define DMA_CHX_SLOT_FUN_CONTRL_STATUS(x)	(DMA_CHANX_SLOT_STATUS_BASE_ADDR + 0x80 * x)

/* DMA Rx Channel X Control register defines */
#define DMA_CONTROL_SR			BIT(0)

/* DMA Tx Channel X Control register defines */
#define DMA_CONTROL_PBL_SHIFT		16
#define DMA_CONTROL_TSE			BIT(12)
#define DMA_CONTROL_OSP			BIT(4)
#define DMA_CONTROL_ST			BIT(0)



#define DMA_CHAN_INTR_ENA_NIE		BIT(15)
#define DMA_CHAN_INTR_ENA_AIE		BIT(14)
#define DMA_CHAN_INTR_ENA_CDE		BIT(13)
#define DMA_CHAN_INTR_ENA_FBE		BIT(12)
#define DMA_CHAN_INTR_ENA_ERE		BIT(11)
#define DMA_CHAN_INTR_ENA_ETE		BIT(10)
#define	DMA_CHAN_INTR_ENA_RWE		BIT(9)
#define	DMA_CHAN_INTR_ENA_RSE		BIT(8)
#define DMA_CHAN_INTR_ENA_RBUE		BIT(7)
#define DMA_CHAN_INTR_ENA_RIE		BIT(6)
#define DMA_CHAN_INTR_ENA_TBUE		BIT(2)
#define DMA_CHAN_INTR_ENA_TSE		BIT(1)
#define DMA_CHAN_INTR_ENA_TIE		BIT(0)

#define DMA_CHAN_INTR_NORMAL		(DMA_CHAN_INTR_ENA_NIE | \
					 DMA_CHAN_INTR_ENA_RIE | \
					 DMA_CHAN_INTR_ENA_TIE )


#define DMA_CHAN_INTR_ABNORMAL		(DMA_CHAN_INTR_ENA_AIE | \
					 DMA_CHAN_INTR_ENA_FBE | \
					 DMA_CHAN_INTR_ENA_RBUE)
/*
					 DMA_CHAN_INTR_ENA_RWE)

					// DMA_CHAN_INTR_ENA_CDE | \
					// DMA_CHAN_INTR_ENA_RBUE |\
					 //DMA_CHAN_INTR_ENA_RWE)
					 //DMA_CHAN_INTR_ENA_RWE |\
					// DMA_CHAN_INTR_ENA_TBUE)
*/

#define DMA_CHAN_INTR_DEFAULT_MASK	(DMA_CHAN_INTR_NORMAL | \
					DMA_CHAN_INTR_ABNORMAL)


#define DMA_CHAN_STATUS_ERI		BIT(11)
#define DMA_CHAN_STATUS_TI		BIT(0)
#define DMA_CHAN_STATUS_RI		BIT(6)
#define DMA_CHAN_STATUS_NIS		BIT(15)
#define DMA_CHAN_STATUS_FBE		BIT(12)
#define DMA_CHAN_STATUS_TPS		BIT(1)
#define DMA_CHAN_STATUS_ETI		BIT(10)
#define DMA_CHAN_STATUS_RWT		BIT(9)
#define DMA_CHAN_STATUS_RPS		BIT(8)
#define DMA_CHAN_STATUS_RBU		BIT(7)
#define DMA_CHAN_STATUS_AIS		BIT(14)
#define DMA_CHAN_STATUS_TBU		BIT(2)


#define STMMAC_CHAN0	0
/* MAC HW ADDR regs */
#define GMAC_HI_DCS			GENMASK(18, 16)
#define GMAC_HI_DCS_SHIFT		16
#define GMAC_HI_REG_AE			BIT(31)
#define GMAC_HI_REG_SA			BIT(30)

#define GMAC_Ext_CONFIG			0x00000004
#define GMAC_PACKET_FILTER		0x00000008
#define GMAC_HASH_TAB_0_31		0x00000010
#define GMAC_HASH_TAB_32_63		0x00000014




/* MAC config */
#define GMAC_CONFIG_IPC			BIT(27)
#define GMAC_CONFIG_2K			BIT(22)
#define GMAC_CONFIG_CST			BIT(21)
#define GMAC_CONFIG_ACS			BIT(20)
#define GMAC_CONFIG_BE			BIT(18)
#define GMAC_CONFIG_JD			BIT(17)
#define GMAC_CONFIG_JE			BIT(16)
#define GMAC_CONFIG_PS			BIT(15)
#define GMAC_CONFIG_FES			BIT(14)
#define GMAC_CONFIG_DM			BIT(13)
#define GMAC_CONFIG_LM			BIT(12)
#define GMAC_CONFIG_DCRS		BIT(9)
#define GMAC_CONFIG_TE			BIT(1)/* Transmitter Enable */
#define GMAC_CONFIG_RE			BIT(0)/* Receiver Enable */
#define GMAC_CONFIG				0x0000 /* MAC Control */

/* MAC ext config */
#define DCRCC				BIT(16)

/* MAC Packet Filtering */
#define GMAC_PACKET_FILTER_PR		BIT(0)
#define GMAC_PACKET_FILTER_HMC		BIT(2)
#define GMAC_PACKET_FILTER_PM		BIT(4)
#define GMAC_PACKET_FILTER_SAF		BIT(9)
#define GMAC_PACKET_FILTER_RA		BIT(31)
#define GMAC_MAX_PERFECT_ADDRESSES	128
#define HASH_TABLE_SIZE 64


/* Default operating mode of the MAC */
#define GMAC_CORE_INIT (GMAC_CONFIG_JD | GMAC_CONFIG_PS | GMAC_CONFIG_ACS | \
			GMAC_CONFIG_BE | GMAC_CONFIG_DCRS)

#define GMAC_INT_EN			0x000000b4



/*  MAC Interrupt bitmap*/
#define GMAC_INT_RGSMIIS		BIT(0)
#define GMAC_INT_PCS_LINK		BIT(1)
#define GMAC_INT_PCS_ANE		BIT(2)
#define GMAC_INT_PCS_PHYIS		BIT(3)
#define GMAC_INT_PMT_EN			BIT(4)
#define GMAC_INT_LPI_EN			BIT(5)
#define GMAC_INT_FPEIE_EN		BIT(17)
#define GMAC_INT_TS_EN BIT(12)

#define	GMAC_PCS_IRQ_DEFAULT	(GMAC_INT_RGSMIIS | GMAC_INT_PCS_LINK |	\
				 GMAC_INT_PCS_ANE)

#define	GMAC_INT_DEFAULT_MASK	GMAC_INT_PMT_EN

/*FPE*/
#define GMAC_FPE_CTRL_STS	0x00000234
#define FPE_STS_TRSP		BIT(19)
#define FPE_STS_TVER		BIT(18)
#define FPE_STS_RRSP		BIT(17)
#define FPE_STS_RVER		BIT(16)
#define FEP_STS_SRSP		BIT(2)
#define FEP_STS_SVER		BIT(1)
#define GMAC_FPE_EFPE		BIT(0)


#define MMC_TX_FPE_Frg_Cntr	0x000008a8
#define MMC_TX_HOLD_Req_Cntr 	0x000008ac



#define MTL_TXQ_WEIGHT_BASE_ADDR	0x00000d18
#define MTL_TXQ_WEIGHT_BASE_OFFSET	0x40
#define MTL_TXQX_WEIGHT_BASE_ADDR(x)	(MTL_TXQ_WEIGHT_BASE_ADDR + \
					((x) * MTL_TXQ_WEIGHT_BASE_OFFSET))

#define MTL_TXQ_WEIGHT_ISCQW_MASK	0x1fffffU//GENMASK(20, 0)



#define MTL_CHAN_BASE_ADDR		0x00000d00
#define MTL_CHAN_BASE_OFFSET		0x40
#define MTL_CHANX_BASE_ADDR(x)		(MTL_CHAN_BASE_ADDR + \
					(x * MTL_CHAN_BASE_OFFSET))



#define MTL_CHAN_TX_OP_MODE(x)		MTL_CHANX_BASE_ADDR(x)
#define MTL_CHAN_RX_OP_MODE(x)		(MTL_CHANX_BASE_ADDR(x) + 0x30)
#define MTL_CHAN_INT_CTRL(x)		(MTL_CHANX_BASE_ADDR(x) + 0x2c)

#define MTL_CHAN_TX_DEBUG(x)			(MTL_CHANX_BASE_ADDR(x) + 0x08)
#define MTL_CHAN_RX_DEBUG(x)			(MTL_CHANX_BASE_ADDR(x) + 0x38)

#define MTL_CHAN_TX_READ_CONTR_STATUS	0x6U
#define MTL_CHAN_TX_READ_CONTR_SHIFT	0x1U
#define MTL_CHAN_TXQSTS_SHIFT			0x4U
#define MTL_CHAN_TXQSTS					0x10U
#define MTL_CHAN_RXQSTS_SHIFT			0x4U
#define MTL_CHAN_RXQSTS					0x30U


/*  MTL interrupt */
#define MTL_RX_OVERFLOW_INT_EN		BIT(24)
#define MTL_RX_OVERFLOW_INT		BIT(16)
#define	MTL_ABPSIS_INT			BIT(1)


#define MTL_OP_MODE_RSF			BIT(5)
#define MTL_OP_MODE_TXQEN		BIT(3)
#define MTL_OP_MODE_TSF			BIT(1)
#define MTL_OP_MODE_TQS_MASK		GENMASK(24, 16)
#define MTL_OP_MODE_TQS_SHIFT		16




#define MTL_OP_MODE_RQS_MASK		GENMASK(29, 20)
#define MTL_OP_MODE_RQS_SHIFT		20
#define MTL_OP_MODE_RFD_MASK		GENMASK(19, 14)
#define MTL_OP_MODE_RFD_SHIFT		14

#define MTL_OP_MODE_RFA_MASK		GENMASK(13, 8)
#define MTL_OP_MODE_RFA_SHIFT		8

#define MTL_OP_MODE_EHFC		BIT(7)




/*  MTL registers */
#define MTL_OPERATION_MODE		0x00000c00
#define MTL_OPERATION_SCHALG_MASK	GENMASK(6, 5)
#define MTL_OPERATION_SCHALG_WRR	0x0U
#define MTL_OPERATION_SCHALG_WFQ	0x20U
#define MTL_OPERATION_SCHALG_DWRR	0x40U
#define MTL_OPERATION_SCHALG_SP		0x60U
#define MTL_OPERATION_RAA		BIT(2)
#define MTL_OPERATION_RAA_SP		(0x0 << 2)
#define MTL_OPERATION_RAA_WSP		(0x1 << 2)


/* MTL ETS Control register */
#define MTL_ETS_CTRL_BASE_ADDR		0x00000d10
#define MTL_ETS_CTRL_BASE_OFFSET	0x40
#define MTL_ETSX_CTRL_BASE_ADDR(x)	(MTL_ETS_CTRL_BASE_ADDR + \
					((x) * MTL_ETS_CTRL_BASE_OFFSET))



#define MTL_ETS_CTRL_CC			BIT(3)
#define MTL_ETS_CTRL_AVALG		BIT(2)
#define MTL_ETS_CTRL_SLC			BIT(4)

#define MTL_EST_STATUS_CTOV_MASK		GENMASK(23, 12)

#define MTL_ETS_STATUS_BASE_ADDR	0x00000d14
#define MTL_ETS_STATUS_BASE_OFFSET	0x40
#define MTL_ETSX_STATUS_BASE_ADDR(x)	(MTL_ETS_STATUS_BASE_ADDR + ((x) * MTL_ETS_STATUS_BASE_OFFSET))

/* MTL sendSlopeCredit register */
#define MTL_SEND_SLP_CRED_BASE_ADDR	0x00000d1c
#define MTL_SEND_SLP_CRED_OFFSET	0x40
#define MTL_SEND_SLP_CREDX_BASE_ADDR(x)	(MTL_SEND_SLP_CRED_BASE_ADDR + \
					((x) * MTL_SEND_SLP_CRED_OFFSET))

#define MTL_SEND_SLP_CRED_SSC_MASK	0x3fffU;//GENMASK(13, 0)



/* MTL hiCredit register */
#define MTL_HIGH_CRED_BASE_ADDR		0x00000d20
#define MTL_HIGH_CRED_OFFSET		0x40
#define MTL_HIGH_CREDX_BASE_ADDR(x)	(MTL_HIGH_CRED_BASE_ADDR + \
					((x) * MTL_HIGH_CRED_OFFSET))

#define MTL_HIGH_CRED_HC_MASK		0x1fffffffU;//GENMASK(28, 0)



/* MTL loCredit register */
#define MTL_LOW_CRED_BASE_ADDR		0x00000d24
#define MTL_LOW_CRED_OFFSET		0x40
#define MTL_LOW_CREDX_BASE_ADDR(x)	(MTL_LOW_CRED_BASE_ADDR + \
					((x) * MTL_LOW_CRED_OFFSET))

#define MTL_HIGH_CRED_LC_MASK		0x1fffffffU;//GENMASK(28, 0)


#define MTL_QX_INTR_CONTROL_STATUS_BASE_ADDR	0x00000d2c
#define MTL_QX_INTR_BASE_OFFSET			0x0040
#define MTL_QX_INTR_CONTROL_STATUS_ADDR(x)	(MTL_QX_INTR_CONTROL_STATUS_BASE_ADDR + ((x) * MTL_QX_INTR_BASE_OFFSET))



#define MTL_RXQ_DMA_MAP0		0x00000c30 /* queue 0 to 3 */
#define MTL_RXQ_DMA_MAP1		0x00000c34 /* queue 4 to 7 */
#define MTL_RXQ_DMA_Q04MDMACH_MASK	0xfU//GENMASK(3, 0)
#define MTL_RXQ_DMA_Q04MDMACH(x)	((x) << 0)
#define MTL_RXQ_DMA_QXMDMACH_MASK(x)	GENMASK(11 + (8 * ((x) - 1)), 8 * (x))
#define MTL_RXQ_DMA_QXMDMACH(chan, q)	((chan) << (8 * (q)))



#define GMAC_TXQ_PRTY_MAP0		0x98
#define GMAC_TXQ_PRTY_MAP1		0x9C
#define GMAC_RXQ_CTRL0			0x000000a0
#define GMAC_RXQ_CTRL1			0x000000a4
#define GMAC_RXQ_CTRL2			0x000000a8
#define GMAC_RXQ_CTRL3			0x000000ac

/* MAC RX Queue Enable */
#define GMAC_RX_QUEUE_CLEAR(queue)	~(GENMASK(1, 0) << ((queue) * 2))
#define GMAC_RX_AV_QUEUE_ENABLE(queue)	BIT((queue) * 2)
#define GMAC_RX_DCB_QUEUE_ENABLE(queue)	BIT(((queue) * 2) + 1)

/* RX Queues Priorities */
#define GMAC_RXQCTRL_PSRQX_MASK(x)	GENMASK(7 + ((x) * 8), 0 + ((x) * 8))
#define GMAC_RXQCTRL_PSRQX_SHIFT(x)	((x) * 8)

/* TX Queues Priorities */
#define GMAC_TXQCTRL_PSTQX_MASK(x)	GENMASK(7 + ((x) * 8), 0 + ((x) * 8))
#define GMAC_TXQCTRL_PSTQX_SHIFT(x)	((x) * 8)



/* RX Queues Routing */
#define GMAC_RXQCTRL_AVCPQ_MASK		GENMASK(2, 0)
#define GMAC_RXQCTRL_AVCPQ_SHIFT	0
#define GMAC_RXQCTRL_PTPQ_MASK		GENMASK(6, 4)
#define GMAC_RXQCTRL_PTPQ_SHIFT		4
#define GMAC_RXQCTRL_DCBCPQ_MASK	GENMASK(10, 8)
#define GMAC_RXQCTRL_DCBCPQ_SHIFT	8
#define GMAC_RXQCTRL_UPQ_MASK		GENMASK(14, 12)
#define GMAC_RXQCTRL_UPQ_SHIFT		12
#define GMAC_RXQCTRL_MCBCQ_MASK		GENMASK(18, 16)
#define GMAC_RXQCTRL_MCBCQ_SHIFT	16

#define GMAC_RXQCTRL_MCBCQEN		BIT(20)
#define GMAC_RXQCTRL_MCBCQEN_SHIFT	20

#define GMAC_RXQCTRL_TACPQE		BIT(21)
#define GMAC_RXQCTRL_TACPQE_SHIFT	21
#define GMAC_RXQCTRL_FPRQ			BIT(24)


#define STMMAC_RX_COE_NONE	0



/* MMC control register */
/* When set, all counter are reset */
#define MMC_CNTRL_COUNTER_RESET		0x1
#define MMC_CNTRL_RESET_ON_READ		0x4	/* Reset after reading */

#define MMC_CNTRL_PRESET		0x10
#define MMC_CNTRL_FULL_HALF_PRESET	0x20

#define	PTP_GMAC4_OFFSET	0xb00

#define MMC_GMAC4_OFFSET		0x700


#define STMMAC_TX_THRESH	(DMA_TX_SIZE / 4)
#define STMMAC_RX_THRESH	(DMA_RX_SIZE / 4)



/* RDES3 (write back format) */
#define RDES3_PACKET_SIZE_MASK		GENMASK(14, 0)
#define RDES3_ERROR_SUMMARY		BIT(15)
#define RDES3_PACKET_LEN_TYPE_MASK	GENMASK(18, 16)
#define RDES3_DRIBBLE_ERROR		BIT(19)
#define RDES3_RECEIVE_ERROR		BIT(20)
#define RDES3_OVERFLOW_ERROR		BIT(21)
#define RDES3_RECEIVE_WATCHDOG		BIT(22)
#define RDES3_GIANT_PACKET		BIT(23)
#define RDES3_CRC_ERROR			BIT(24)
#define RDES3_RDES0_VALID		BIT(25)
#define RDES3_RDES1_VALID		BIT(26)
#define RDES3_RDES2_VALID		BIT(27)
#define RDES3_LAST_DESCRIPTOR		BIT(28)
#define RDES3_FIRST_DESCRIPTOR		BIT(29)
#define RDES3_CONTEXT_DESCRIPTOR	BIT(30)
#define RDES3_CONTEXT_DESCRIPTOR_SHIFT	30


/* RDES2 (write back format) */
#define RDES2_L3_L4_HEADER_SIZE_MASK	GENMASK(9, 0)
#define RDES2_VLAN_FILTER_STATUS	BIT(15)
#define RDES2_SA_FILTER_FAIL		BIT(16)
#define RDES2_DA_FILTER_FAIL		BIT(17)
#define RDES2_HASH_FILTER_STATUS	BIT(18)
#define RDES2_MAC_ADDR_MATCH_MASK	GENMASK(26, 19)
#define RDES2_HASH_VALUE_MATCH_MASK	GENMASK(26, 19)
#define RDES2_L3_FILTER_MATCH		BIT(27)
#define RDES2_L4_FILTER_MATCH		BIT(28)
#define RDES2_L3_L4_FILT_NB_MATCH_MASK	GENMASK(27, 26)
#define RDES2_L3_L4_FILT_NB_MATCH_SHIFT	26


/* RDES0 (write back format) */
#define RDES0_VLAN_TAG_MASK		GENMASK(15, 0)

/* RDES1 (write back format) */
#define RDES1_IP_PAYLOAD_TYPE_MASK	GENMASK(2, 0)
#define RDES1_IP_HDR_ERROR		BIT(3)
#define RDES1_IPV4_HEADER		BIT(4)
#define RDES1_IPV6_HEADER		BIT(5)
#define RDES1_IP_CSUM_BYPASSED		BIT(6)
#define RDES1_IP_CSUM_ERROR		BIT(7)
#define RDES1_PTP_MSG_TYPE_MASK		GENMASK(11, 8)
#define RDES1_PTP_PACKET_TYPE		BIT(12)
#define RDES1_PTP_VER			BIT(13)
#define RDES1_TIMESTAMP_AVAILABLE	BIT(14)
#define RDES1_TIMESTAMP_AVAILABLE_SHIFT	14
#define RDES1_TIMESTAMP_DROPPED		BIT(15)
#define RDES1_IP_TYPE1_CSUM_MASK	GENMASK(31, 16)


/* Extended Receive descriptor definitions */
#define	ERDES4_IP_PAYLOAD_TYPE_MASK	GENMASK(2, 6)
#define	ERDES4_IP_HDR_ERR		BIT(3)
#define	ERDES4_IP_PAYLOAD_ERR		BIT(4)
#define	ERDES4_IP_CSUM_BYPASSED		BIT(5)
#define	ERDES4_IPV4_PKT_RCVD		BIT(6)
#define	ERDES4_IPV6_PKT_RCVD		BIT(7)
#define	ERDES4_MSG_TYPE_MASK		GENMASK(11, 8)
#define	ERDES4_PTP_FRAME_TYPE		BIT(12)
#define	ERDES4_PTP_VER			BIT(13)
#define	ERDES4_TIMESTAMP_DROPPED	BIT(14)
#define	ERDES4_AV_PKT_RCVD		BIT(16)
#define	ERDES4_AV_TAGGED_PKT_RCVD	BIT(17)
#define	ERDES4_VLAN_TAG_PRI_VAL_MASK	GENMASK(20, 18)
#define	ERDES4_L3_FILTER_MATCH		BIT(24)
#define	ERDES4_L4_FILTER_MATCH		BIT(25)
#define	ERDES4_L3_L4_FILT_NO_MATCH_MASK	GENMASK(27, 26)




/* Extended RDES4 message type definitions */
#define RDES_EXT_NO_PTP			0x0
#define RDES_EXT_SYNC			0x1
#define RDES_EXT_FOLLOW_UP		0x2
#define RDES_EXT_DELAY_REQ		0x3
#define RDES_EXT_DELAY_RESP		0x4
#define RDES_EXT_PDELAY_REQ		0x5
#define RDES_EXT_PDELAY_RESP		0x6
#define RDES_EXT_PDELAY_FOLLOW_UP	0x7
#define RDES_PTP_ANNOUNCE		0x8
#define RDES_PTP_MANAGEMENT		0x9
#define RDES_PTP_SIGNALING		0xa
#define RDES_PTP_PKT_RESERVED_TYPE	0xf



/* Transmit checksum insertion control */
#define	TX_CIC_FULL	3U	/* Include IP header and pseudoheader */

/* TDS3 use for both format (read and write back) */
#define TDES3_OWN			BIT(31)
#define TDES3_OWN_SHIFT			31
#define TDES3_CTXT_TCMSSV		BIT(26)


/* TDES2 (read format) */
#define TDES2_BUFFER1_SIZE_MASK		GENMASK(13, 0)
#define TDES2_VLAN_TAG_MASK		GENMASK(15, 14)
#define TDES2_BUFFER2_SIZE_MASK		GENMASK(29, 16)
#define TDES2_BUFFER2_SIZE_MASK_SHIFT	16
#define TDES2_TIMESTAMP_ENABLE		BIT(30)
#define TDES2_INTERRUPT_ON_COMPLETION	BIT(31)

/* TDES3 (read format) */
#define TDES3_PACKET_SIZE_MASK		GENMASK(14, 0)
#define TDES3_CHECKSUM_INSERTION_MASK	GENMASK(17, 16)
#define TDES3_CHECKSUM_INSERTION_SHIFT	16U
#define TDES3_TCP_PKT_PAYLOAD_MASK	GENMASK(17, 0)
#define TDES3_TCP_SEGMENTATION_ENABLE	BIT(18)
#define TDES3_HDR_LEN_SHIFT		19
#define TDES3_SLOT_NUMBER_MASK		GENMASK(22, 19)
#define TDES3_SA_INSERT_CTRL_MASK	GENMASK(25, 23)
#define TDES3_CRC_PAD_CTRL_MASK		GENMASK(27, 26)



/* TDES3 (write back format) */
#define TDES3_IP_HDR_ERROR		BIT(0)
#define TDES3_DEFERRED			BIT(1)
#define TDES3_UNDERFLOW_ERROR		BIT(2)
#define TDES3_EXCESSIVE_DEFERRAL	BIT(3)
#define TDES3_COLLISION_COUNT_MASK	GENMASK(7, 4)
#define TDES3_COLLISION_COUNT_SHIFT	4
#define TDES3_EXCESSIVE_COLLISION	BIT(8)
#define TDES3_LATE_COLLISION		BIT(9)
#define TDES3_NO_CARRIER		BIT(10)
#define TDES3_LOSS_CARRIER		BIT(11)
#define TDES3_PAYLOAD_ERROR		BIT(12)
#define TDES3_PACKET_FLUSHED		BIT(13)
#define TDES3_JABBER_TIMEOUT		BIT(14)
#define TDES3_ERROR_SUMMARY		BIT(15)
#define TDES3_TIMESTAMP_STATUS		BIT(17)
#define TDES3_TIMESTAMP_STATUS_SHIFT	17


/* TDES3 Common */
#define	TDES3_RS1V			BIT(26)
#define	TDES3_RS1V_SHIFT		26
#define TDES3_LAST_DESCRIPTOR		BIT(28)
#define TDES3_LAST_DESCRIPTOR_SHIFT	28
#define TDES3_FIRST_DESCRIPTOR		BIT(29)
#define TDES3_CONTEXT_TYPE		BIT(30)
#define	TDES3_CONTEXT_TYPE_SHIFT	30




/* EEE and LPI defines */
#define	CORE_IRQ_TX_PATH_IN_LPI_MODE	(1 << 0)
#define	CORE_IRQ_TX_PATH_EXIT_LPI_MODE	(1 << 1)
#define	CORE_IRQ_RX_PATH_IN_LPI_MODE	(1 << 2)
#define	CORE_IRQ_RX_PATH_EXIT_LPI_MODE	(1 << 3)

#define CORE_IRQ_MTL_RX_OVERFLOW	BIT(8)


#define MTL_INT_STATUS			0x00000c20
#define MTL_INT_QX(x)			BIT(x)


/* PCS status and mask defines */
#define	PCS_ANE_IRQ		BIT(2)	/* PCS Auto-Negotiation */
#define	PCS_LINK_IRQ		BIT(1)	/* PCS Link */
#define	PCS_RGSMIIIS_IRQ	BIT(0)	/* RGMII or SMII Interrupt */


#define GMAC_PHYIF_CTRLSTATUS_LNKMOD_MASK	0x1
#define GMAC_PHYIF_CTRLSTATUS_SPEED_125		0x2
#define GMAC_PHYIF_CTRLSTATUS_SPEED_25		0x1


/* SGMII/RGMII status register */
#define GMAC_PHYIF_CTRLSTATUS_TC		BIT(0)
#define GMAC_PHYIF_CTRLSTATUS_LUD		BIT(1)
#define GMAC_PHYIF_CTRLSTATUS_SMIDRXS		BIT(4)
#define GMAC_PHYIF_CTRLSTATUS_LNKMOD		BIT(16)
#define GMAC_PHYIF_CTRLSTATUS_SPEED		GENMASK(18, 17)
#define GMAC_PHYIF_CTRLSTATUS_SPEED_SHIFT	17
#define GMAC_PHYIF_CTRLSTATUS_LNKSTS		BIT(19)
#define GMAC_PHYIF_CTRLSTATUS_JABTO		BIT(20)
#define GMAC_PHYIF_CTRLSTATUS_FALSECARDET	BIT(21)




/* AN Status defines */
#define GMAC_AN_STATUS_LS	BIT(2)	/* Link Status 0:down 1:up */
#define GMAC_AN_STATUS_ANA	BIT(3)	/* Auto-Negotiation Ability */
#define GMAC_AN_STATUS_ANC	BIT(5)	/* Auto-Negotiation Complete */
#define GMAC_AN_STATUS_ES	BIT(8)	/* Extended Status */
#define GMAC_AN_STATUS(x)	(x + 0x4)	/* AN status */

#define	PTP_TCR		0xb00	/* Timestamp Control Reg */
#define	PTP_SSIR	0xb04	/* Sub-Second Increment Reg */
#define PTP_STSR 0xb08 /* System Time Seconds Regr */
#define PTP_STNSR 0xb0c /* System Time Nanoseconds Reg */
#define PTP_STSUR 0xb10 /* System Time Seconds Update Reg */
#define PTP_STNSUR 0xb14 /* System Time Nanoseconds Update Reg */
#define	PTP_TAR		0xb18	/* Timestamp Addend Reg */


#define	PTP_STNSUR_ADDSUB_SHIFT	31
#define	PTP_DIGITAL_ROLLOVER_MODE	0x3B9ACA00	/* 10e9-1 ns */
#define	PTP_BINARY_ROLLOVER_MODE	0x80000000	/* ~0.466 ns */




/* PTP Timestamp control register defines */
#define	PTP_TCR_TSENA		BIT(0)	/* Timestamp Enable */
#define	PTP_TCR_TSCFUPDT		BIT(1)	/* Timestamp Fine/Coarse Update */
#define	PTP_TCR_TSINIT		BIT(2)	/* Timestamp Initialize */
#define	PTP_TCR_TSUPDT		BIT(3)	/* Timestamp Update */
#define	PTP_TCR_TSTRIG		BIT(4)	/* Timestamp Interrupt Trigger Enable */
#define	PTP_TCR_TSADDREG		BIT(5)	/* Addend Reg Update */
#define	PTP_TCR_TSENALL		BIT(8)	/* Enable Timestamp for All Frames */
#define	PTP_TCR_TSCTRLSSR	BIT(9)	/* Digital or Binary Rollover Control */
/* Enable PTP packet Processing for Version 2 Format */
#define	PTP_TCR_TSVER2ENA	BIT(10)
/* Enable Processing of PTP over Ethernet Frames */
#define	PTP_TCR_TSIPENA		BIT(11)
/* Enable Processing of PTP Frames Sent over IPv6-UDP */
#define	PTP_TCR_TSIPV6ENA	BIT(12)
/* Enable Processing of PTP Frames Sent over IPv4-UDP */
#define	PTP_TCR_TSIPV4ENA	BIT(13)
/* Enable Timestamp Snapshot for Event Messages */
#define	PTP_TCR_TSEVNTENA	BIT(14)
/* Enable Snapshot for Messages Relevant to Master */
#define	PTP_TCR_TSMSTRENA	BIT(15)
/* Select PTP packets for Taking Snapshots */
#define	PTP_TCR_SNAPTYPSEL_1	BIT(16)
#define	PTP_TCR_SNAPTYPSEL_2	BIT(17)
#define	PTP_GMAC4_TCR_SNAPTYPSEL_1	GENMASK(17, 16)
/* Enable MAC address for PTP Frame Filtering */
#define	PTP_TCR_TSENMACADDR	BIT(18)
/* Enable MAC address for PTP Frame Filtering */
#define	PTP_TCR_CSC		BIT(19)


#define PTP_SSIR_SSINC_MASK	0xff
#define PTP_SSIR_SNSINC_MASK	0xff
#define	GMAC4_PTP_SSIR_SSINC_SHIFT	16
#define GMAC4_PTP_SSIR_SNSINC_SHIFT	8
/*ethtool*/

#define	MAC_Egress_Timestamp_Latency	0x0000066C
#define	MAC_Igress_Timestamp_Latency	0x000006b8
#define MAC_Timestamp_Ingress_Corr_Nanosecond 0xb58
#define MAC_Timestamp_Egress_Corr_Nanosecond 0xb5c
#define MAC_Timestamp_Ingress_Corr_Subnanosecond 0xb60
#define MAC_Timestamp_Egress_Corr_Subnanosecond 0xb64

#define	MAC_PPS_CONTROL		0x00000b70
#define PPS_MAXIDX(x)		((((x) + 1)*8) - 1)
#define PPS_MINIDX(x)		((x) * 8)

#define PPSx_MASK(x)		GENMASK(PPS_MAXIDX(x), PPS_MINIDX(x))
#define PPSCMDx(x,val)		GENMASK(PPS_MINIDX(x) + 3, PPS_MINIDX(x)) & ((val) << PPS_MINIDX(x))
#define TRGTMODSELx(x,val)	GENMASK(PPS_MAXIDX(x) - 1, PPS_MAXIDX(x) - 2) & ((val) << (PPS_MAXIDX(x) - 2))
#define PPSCMD_START_SINGLE		0x1 //start single plus flexible pps output
#define PPSCMD_START_TRAIN		0x2 //start Pulse Train flexible pps output
#define PPSCMD_CANCEL			0x5 //Cancel flexible pps output
#define TRGTMODSEL_INT_ST		0x2 //interrupt and pps out mode
#define TRGTMODSEL_ONLY_ST		0x3 //pps out mode


#define PPSEN0	BIT(4) //Flexible PPS Output Mode Enable, include 0-3
#define MAC_PPSx_TARGET_TIME_SEC(x)	(0x00000b80 + ((x)*0x10))
#define MAC_PPSx_TARGET_TIME_NSEC(x)	(0x00000b84 + ((x)*0x10))
#define TRGTBUSY0	BIT(31)
#define MAC_PPSx_INTERVAL(x)		(0x00000b88 + ((x)*0x10))
#define MAC_PPSx_WIDTH(x)		(0x00000b8c + ((x)*0x10))

#define MAC_TS_STATUS 0xb20 /* Timestamp Status register */
#define ATSSTN0		BIT(16) /* Auxiliary trigger 0 -- rtc give time*/
#define ATSSTN1		BIT(17) /* Auxiliary trigger 1 -- gps pps*/
#define ATSSTN2		BIT(18) /* Auxiliary trigger 2 -- lidar*/
#define ATSSTN3		BIT(19) /* Auxiliary trigger 3 -- ap*/
#define TSTARGT3	BIT(8) /* Timestamp Target Time Reached PPS3 */
#define TSTARGT2 BIT(6) /* Timestamp Target Time Reached PPS2 */
#define TSTARGT1 BIT(4) /* Timestamp Target Time Reached PPS1 */
#define AUXTSTRIG BIT(2) /* Auxiliary Timestamp Trigger Snapshot */
#define TSTARGT0 BIT(1) /* Timestamp Target Time Reached PPS0 */
#define ATSNS GENMASK(29, 25) /* Number of Auxiliary Timestamp Snapshots */
#define MAC_AUX_CONTROL 0xb40 /* The Auxiliary Timestamp Control register */
#define ATSEN3 BIT(7) /* Auxiliary Snapshot 3 Enable */
#define ATSEN2 BIT(6) /* Auxiliary Snapshot 2 Enable */
#define ATSEN1 BIT(5) /* Auxiliary Snapshot 1 Enable */
#define ATSEN0 BIT(4) /* Auxiliary Snapshot 0 Enable */
#define ATSFC BIT(0) /* Auxiliary Snapshot FIFO Clear */
#define MAC_AUX_TS_SECOND 0xb4c /*Auxiliary Timestamp Seconds register */
#define MAC_AUX_TS_NANOSECOND  0xb48 /*Auxiliary Timestamp nanoseconds register */

#define REG_SPACE_SIZE	0x1168

#define NUM_MAC_REGS	0xbd0


#define ETHTOOL_DMA_OFFSET	61
#define NUM_DWMAC1000_DMA_REGS	23

/* To dump the core regs excluding  the Address Registers */
#define	GMAC_REG_NUM	 55
#define DMA_CHANNEL_NB_MAX 1


#define DMA_RWT	GENMASK(7, 0)
#define DMA_RWT_MAX	0xff
#define	DMA_RWTU	GENMASK(17, 16)
#define	DMA_RWTU_SHIFT	16
#define DMA_RWTU_256	(0x0 << DMA_RWTU_SHIFT)
#define	DMA_RWTU_512	(0x1 << DMA_RWTU_SHIFT)
#define	DMA_RWTU_1024	(0x2 << DMA_RWTU_SHIFT)
#define	DMA_RWTU_2048	(0x3 << DMA_RWTU_SHIFT)

/* automotive safety registers*/
#define MAC_PRESN_TIME_NS		0x00000240
#define MAC_PRESN_TIME_UPDT		0x00000244
#define MAC_DPP_FSM_INT_STATUS		0x00000140
#define FSMPES				BIT(24)
#define ARITES				BIT(19)
#define ATITES				BIT(18)
#define SLVTES				BIT(17)
#define MSTTES				BIT(16)
#define RVCTES				BIT(15)
#define R125ES				BIT(14)
#define T125ES				BIT(13)
#define PTES				BIT(12)
#define ATES				BIT(11)
#define CTES				BIT(10)
#define RTES				BIT(9)
#define TTES				BIT(8)
#define ASRPES				BIT(7)
#define CWPES				BIT(6)
#define ARPES				BIT(5)
#define MTSPES				BIT(4)
#define MPES				BIT(3)
#define RDPES				BIT(2)
#define TPES				BIT(1)
#define ATPES				BIT(0)
#define MAC_AXI_SLV_DPE_ADDR_STATUS	0x00000144
#define ASPEAS				GENMASK(13, 0)
#define MAC_FSM_CONTROL			0x00000148
#define RVCLGRNML			BIT(31)
#define R125LGRNML			BIT(30)
#define T125LGRNML			BIT(29)
#define PLGRNML				BIT(28)
#define ALGRNML				BIT(27)
#define CLGRNML				BIT(26)
#define RLGRNML				BIT(25)
#define TLGRNML				BIT(24)
#define RVCPEIN				BIT(23)
#define R125PEIN			BIT(22)
#define T125PEIN			BIT(21)
#define PPEIN				BIT(20)
#define APEIN				BIT(19)
#define CPEIN				BIT(18)
#define RPEIN				BIT(17)
#define TPEIN				BIT(16)
#define RVCTEIN				BIT(15)
#define R125TEIN			BIT(14)
#define T125TEIN			BIT(13)
#define PTEIN				BIT(12)
#define ATEIN				BIT(11)
#define CTEIN				BIT(10)
#define RTEIN				BIT(9)
#define TTEIN				BIT(8)
#define PRTYEN				BIT(1)
#define TMOUTEN				BIT(0)
#define MAC_FSM_ACT_TIMER		0x0000014c
#define NTMRMD(x) (((x)&0xfU) << 16U)
#define TMR(x) ((x)&0x3ffU)
#define NTMRMD_64MS			4U

#define MTL_DBG_CTL			0x00000c08
#define EIEC				GENMASK(18, 17)
#define EIEC_2BIT BIT(17)
#define EIEC_3BIT BIT(18)
#define EIEE				BIT(16)
#define FIFOSELTSO BIT(13)
#define FIFOSELRX (BIT(12) | BIT(13))
#define FIFOWREN BIT(11)
#define FIFORDEN BIT(10)
#define DEBUGMOD BIT(1)
#define FDBGEN BIT(0)

#define MTL_RXP_CONTROL_STATUS		0x00000ca0
#define RXPI				BIT(31)
#define NPE				GENMASK(23, 16)
#define NVE				GENMASK(7, 0)
#define MTL_RXP_INT_CONTROL_STATUS	0x00000ca4
#define MTL_RXP_DROP_CNT		0x00000ca8
#define MTL_RXP_ERROR_CNT		0x00000cac
#define MTL_RXP_IACC_CTRL_STATUS	0x00000cb0
#define STARTBUSY			BIT(31)
#define RXPEIEC				GENMASK(22, 21)
#define RXPEIEE				BIT(20)
#define WRRDN				BIT(16)
#define ADDR				GENMASK(15,0)
#define MTL_RXP_IACC_DATA		0x00000cb4
#define MTL_ECC_CONTROL			0x00000cc0
#define TSOEE				BIT(4)
#define MRXPEE				BIT(3)
#define MESTEE				BIT(2)
#define MRXEE				BIT(1)
#define MTXEE				BIT(0)

#define MTL_SAFETY_INT_STATUS		0x00000cc4
#define MCSIS				BIT(31)
#define MEUIS				BIT(1)
#define MECIS				BIT(0)
#define MTL_ECC_INT_ENABLE		0x00000cc8
#define RPCEIE				BIT(12)
#define ECEIE				BIT(8)
#define RXCEIE				BIT(4)
#define TXCEIE				BIT(0)
#define MTL_ECC_INT_STATUS		0x00000ccc
#define RPUES				BIT(14)
#define RPAMS				BIT(13)
#define RPCES				BIT(12)
#define EUES				BIT(10)
#define EAMS				BIT(9)
#define ECES				BIT(8)
#define RXUES				BIT(6)
#define RXAMS				BIT(5)
#define RXCES				BIT(4)
#define TXUES				BIT(2)
#define TXAMS				BIT(1)
#define TXCES				BIT(0)
#define MTL_ECC_ERR_STS_RCTL		0x00000cd0
#define CUES				BIT(5)
#define CCES				BIT(4)
#define EMS				GENMASK(3, 1)
#define EESRE				BIT(0)
#define MTL_ECC_ERR_ADDR_STATUS		0x00000cd4
#define EUEAS				GENMASK(30, 16)
#define ECEAS				GENMASK(14, 0)
#define MTL_ECC_ERR_CNTR_STATUS		0x00000cd8
#define EUECS				GENMASK(19, 16)
#define ECECS				GENMASK(7, 0)
#define MTL_DPP_CONTROL			0x00000ce0
#define IPECW				BIT(13)
#define IPEASW				BIT(12)
#define IPERD				BIT(11)
#define IPETD				BIT(10)
#define IPETSO				BIT(9)
#define IPEDDC				BIT(8)
#define IPEMRF				BIT(7)
#define IPEMTS				BIT(6)
#define IPEMC				BIT(5)
#define IPEID				BIT(4)
#define EPSI				BIT(2)
#define OPE				BIT(1)
#define EDPP				BIT(0)

#define DMA_SAFETY_INT_STATUS		0x00001080
#define MSUIS				BIT(29)
#define MSCIS				BIT(28)
#define DEUIS				BIT(1)
#define DECIS				BIT(0)
#define DMA_ECC_INT_ENABLE		0x00001084
#define TCEIE				BIT(0)
#define DMA_ECC_INT_STATUS		0x00001088

#define MTL_FIFO_DEBUG_DATA 0xc10


#define MIN_AVB_QUEUES 3

#define AVB_CLASSA_Q 3
#define AVB_CLASSB_Q 2
#define AVB_PTPCP_Q 1
#define AVB_BEST_EFF_Q 0
#define AVB_CLASSA_MASK 7U
#define AVB_CLASSA_BW 40
#define AVB_CLASSB_BW 20

#define AVB_CBS_SCALE 1024

#define PORT_RATE_SGMII 8
#define PORT_RATE_GMII 4

#define ETHNORMAL_LEN 1500

#define MTL_EST_CONTROL 0x00000c50
#define MTL_EST_PTOV 0xff000000U//GENMASK(31, 24)
#define MTL_EST_PTOV_OFFSET 24U
#define MTL_EST_PTOV_MUL 6
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
#define MTL_EST_ADDR 0xffF00U//GENMASK(19, 8)
#define MTL_EST_ADDR_OFFSET 8
#define MTL_EST_BTR_LOW 0x0U
#define MTL_EST_BTR_HIGH 0x100U
#define MTL_EST_CTR_LOW 0x200U
#define MTL_EST_CTR_HIGH 0x300U
#define MTL_EST_TER 0x400U
#define MTL_EST_LLR 0x500U
#define MTL_EST_ERR0 BIT(20)
#define MTL_EST_DBGB BIT(5)
#define MTL_EST_DBGM BIT(4)
#define MTL_EST_GCRR BIT(2)
#define MTL_EST_R1WO BIT(1)
#define MTL_EST_SRWO BIT(0)
#define MTL_EST_ESTEIEC			GENMASK(23, 22)
#define MTL_EST_ESTEIEE			BIT(21)
#define MTL_EST_ESTEIEC_2BIT		BIT(22)
#define MTL_EST_ESTEIEC_3BIT		BIT(23)

#define MTL_EST_GCL_DATA 0x00000c84

#define MTL_FPE_CTRL_STS 0x00000c90
#define MTL_FPE_PEC      0x300U
#define MTL_FPE_AFSZ     0x1U

#define MTL_FPE_Advance 0x00000c94
#define MTL_FPE_RADV_HADV 0x10001000U
#define MTL_OP_MODE_TXQEN_MASK GENMASK(3, 2)
#define MTL_OP_MODE_TXQEN_AV BIT(2)

#define MTL_EST_Frm_Size_Error 0x00000C64
#define MTL_EST_Frm_Size_Capture 0x00000C68


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


#endif
