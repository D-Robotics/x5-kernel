/*
 *  Copyright (C) 2020 Hobot Robotics.
 * This code from  horizon Ethernet Quality-of-Service v5.10a linux driver
 *
 *  This is a driver for the horizon Ethernet QoS IP version 5.10a (GMAC).
 *
 *  This driver has been developed for a subset of the total available
 *  feature set. Currently
 *  it supports:
 *  - TSO
 *  - Checksum offload for RX and TX.
 *  - Energy efficient ethernet.
 *  - GMII phy interface.
 *  - The statistics module.
 *  - Single RX and TX queue.
 *
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms and conditions of the GNU General Public License,
 *  version 2, as published by the Free Software Foundation.
 */
#define pr_fmt(fmt)    "[ETH]:" fmt

#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/bitrev.h>
#include <linux/crc32.h>
#include <linux/interrupt.h>
#include <linux/of_net.h>
#include <linux/of_address.h>
#include <linux/of_mdio.h>
#include <linux/tcp.h>
#include <linux/reset.h>
#include <linux/phylink.h>
#include <net/pkt_cls.h>
#include "hobot_eth_jplus.h"

#define TSO_MAX_BUFF_SIZE (SZ_16K - 1)

#define DRIVER_NAME "hobot_gmac"

#define reg_writel(priv, v, c)                                                 \
	do {                                                                   \
		writel((v), (priv)->ioaddr + (c));                             \
		if ((priv)->hobot_eth_stl_ops &&                               \
		    (priv)->hobot_eth_stl_ops->stl_update_backup_regs) {       \
			(priv)->hobot_eth_stl_ops->stl_update_backup_regs(     \
				priv->ndev, v, c);                         \
		}                                                              \
		pr_debug("value=0x%x, read_value=0x%x\n", v,            \
				readl((priv)->ioaddr + (c)));                  \
	} while (0)

#define MMC_CNTRL              0x00    /* MMC Control */
#define MMC_RX_INTR            0x04    /* MMC RX Interrupt */
#define MMC_TX_INTR            0x08    /* MMC TX Interrupt */
#define MMC_RX_INTR_MASK       0x0c    /* MMC Interrupt Mask */
#define MMC_TX_INTR_MASK       0x10    /* MMC Interrupt Mask */
#define DEFAULT_MASK       0xffffffffU

/* IPC*/
#define MMC_RX_IPC_INTR_MASK        0x100
#define MMC_RX_IPC_INTR         0x108

/*mdio magic*/
#define DEVADDR_OFFSET 16U
#define MDIO_VALUE_MASK 0xffff
#define DELAY_64US		64
#define DELAY_128US		128
#define MDIO_TIMEOUT_MS		10U

#define QUEUE_WEIGTH_VAL 0x10U


/*bit offset*/
#define BIT_INDEX_0			0
#define BIT_INDEX_1			1
#define BIT_INDEX_2			2
#define BIT_INDEX_3			3
#define BIT_INDEX_4			4
#define BIT_INDEX_5			5
#define BIT_INDEX_6			6
#define BIT_INDEX_7			7
#define BIT_INDEX_8			8
#define BIT_INDEX_10			10
#define BIT_INDEX_12			12
#define BIT_INDEX_13			13
#define BIT_INDEX_14			14
#define BIT_INDEX_16			16
#define BIT_INDEX_17			17
#define BIT_INDEX_18			18
#define BIT_INDEX_20			20
#define BIT_INDEX_24			24
#define BIT_INDEX_26			26
#define BIT_INDEX_27			27
#define BIT_INDEX_28			28
#define BIT_INDEX_32			32

#define EST_WIDTH16			16
#define EST_WIDTH20			20
#define EST_WIDTH24			24

#define BYTES_0				0U
#define BYTES_1				1U
#define BYTES_2				2U
#define BYTES_3				3U
#define BYTES_4				4U
#define BYTES_5				5U
#define BYTES_1500			1500U
#define BYTES_2000			2000U

#define SZ_0				0x0U
#define SZ_3				0x3U
#define SZ_5				0x5U
#define SZ_6				0x6U
#define SZ_7				0x7U
#define SZ_9				0x9U
#define SZ_10				0xaU
#define SZ_11				0xbU
#define SZ_12				0xcU
#define SZ_13				0xdU
#define SZ_14				0xeU
#define SZ_15				0xfU
#define SZ_17				0x11U
#define SZ_18				0x12U
#define SZ_19				0x13U
#define SZ_20				0x14U
#define SZ_21				0x15U
#define SZ_22				0x16U
#define SZ_23				0x17U
#define SZ_24				0x18U
#define SZ_25				0x19U
#define SZ_26				0x1aU
#define SZ_27				0x1bU
#define SZ_28				0x1cU
#define SZ_29				0x1dU
#define SZ_30				0x1eU
#define SZ_31				0x1fU
#define SZ_1000U			1000U
#define SZ_465U				465U
#define SZ_256U				256U
#define SZ_4968				4968
/*timestamp magic*/
#define TIMEOUT_5MS		5
#define PER_SEC_NS		1000000000ULL
#define PTP_FINE_PERIOD	20U


/*mac clk*/
#define CLOCK_125M			125000000
#define CLOCK_25M			25000000
#define CLOCK_2_5M			2500000
#define CLOCK_62_5M			62500000

#define HOBOT_COAL_TIMER(x) (jiffies + usecs_to_jiffies(x))
#define HOBOT_COAL_TX_TIMER	1000
static void hobot_init_timer(struct hobot_priv *priv);
static struct net_device * pndev = NULL;

/**
 * enable_dma_irq
 * @priv: hobot private struct pointer
 * @chan: channel id
 * Description: enable dma irq
 * Return:NA
 */
/* code review E1: do not need to return value */
static inline void enable_dma_irq(const struct hobot_priv *priv, u32 chan)
{
	iowritel(DMA_CHAN_INTR_DEFAULT_MASK, priv->ioaddr, DMA_CHAN_INTR_ENA(chan));
}


/**
 * mdio_read_reg
 * @bus: mii bus pointer
 * @mii_id: phy slave address
 * @phyreg: phy register address
 * Description: use mdio bus to read phy reg data.
 * Return: >=0 success, reg data; otherwise error
 */
static s32 mdio_read_reg(struct mii_bus *bus, s32 mii_id, s32 phyreg)
{
	struct net_device *ndev = (struct net_device *)bus->priv;
	struct hobot_priv *priv = (struct hobot_priv *)netdev_priv(ndev);
	u32 regval;
	s32 data;
	unsigned long timeout_jiffies;
	u32 u_mii_id = (u32)mii_id;
	u32 u_phyreg = (u32)phyreg;

	/*support the phy_id "port:device" Clause 45 address*/
	if (mdio_phy_id_is_c45(mii_id)) {
		u_phyreg |= (u32)MII_ADDR_C45 | ((u32)mdio_phy_id_devad(mii_id) << DEVADDR_OFFSET);
		u_mii_id = mdio_phy_id_prtad(mii_id);
	}

	if ((u_phyreg & (u32)MII_ADDR_C45) != (u32)MII_ADDR_C45) {/*c22*/
		regval = DWCEQOS_MDIO_PHYADDR(u_mii_id) | DWCEQOS_MDIO_PHYREG(u_phyreg) |
			DWCEQOS_MAC_MDIO_ADDR_CR(priv->csr_val) |
			(u32)DWCEQOS_MAC_MDIO_ADDR_GB | (u32)DWCEQOS_MAC_MDIO_ADDR_GOC_READ;
	} else {/*c45*/
		regval = (u32)EQOS_MAC_MDIO_ADDR_C45E | DWCEQOS_MDIO_PHYADDR(u_mii_id) |
			DWCEQOS_MDIO_PHYREG_DEV(u_phyreg) | DWCEQOS_MAC_MDIO_ADDR_CR(priv->csr_val) |
			(u32)DWCEQOS_MAC_MDIO_ADDR_GB | (u32)DWCEQOS_MAC_MDIO_ADDR_GOC_READ;
		iowritel(DWCEQOS_MAC_MDIO_DATA_RA(u_phyreg), priv->ioaddr, REG_DWCEQOS_MAC_MDIO_DATA);
	}
	iowritel(regval, priv->ioaddr, REG_DWCEQOS_MAC_MDIO_ADDR);

	/* Wait read complete */
	timeout_jiffies = jiffies + msecs_to_jiffies(MDIO_TIMEOUT_MS);
	while ((ioreadl(priv->ioaddr, REG_DWCEQOS_MAC_MDIO_ADDR) &
		      DWCEQOS_MAC_MDIO_ADDR_GB) != 0x0U) {
		if (time_is_before_jiffies(timeout_jiffies)) {/*PRQA S 2996*/
			/*avoid scheduling timeouts*/
			regval = ioreadl(priv->ioaddr, REG_DWCEQOS_MAC_MDIO_ADDR);
			if ((regval & DWCEQOS_MAC_MDIO_ADDR_GB) == 0x0U)
				break;
			netdev_err(ndev, "MDIO read timed out\n");
			return MDIO_VALUE_MASK;
		}
		usleep_range(DELAY_64US, DELAY_128US);
	}

	data = ioreadl(priv->ioaddr, REG_DWCEQOS_MAC_MDIO_DATA);
	return data & MDIO_VALUE_MASK;/*PRQA S 4532*/
}


/**
 * mdio_write_reg
 * @bus: mii bus pointer
 * @mii_id: phy slave address
 * @phyreg: phy register address
 * @value: value want to write
 * Description: use mdio bus write data to phy register.
 * Return: 0 success; otherwise error
 */
static s32 mdio_write_reg(struct mii_bus *bus, s32 mii_id, s32 phyreg, u16 value)
{
	struct net_device *ndev = (struct net_device *)bus->priv;
	struct hobot_priv *priv = (struct hobot_priv *)netdev_priv(ndev);
	u32 regval;
	unsigned long timeout_jiffies;
	u32 u_mii_id = (u32)mii_id;
	u32 u_phyreg = (u32)phyreg;
	/*support the phy_id "port:device" Clause 45 address*/
	if (mdio_phy_id_is_c45(mii_id)) {
		u_phyreg |= (u32)MII_ADDR_C45 | ((u32)mdio_phy_id_devad(mii_id) << DEVADDR_OFFSET);
		u_mii_id = mdio_phy_id_prtad(mii_id);
	}

	iowritel(value, priv->ioaddr, REG_DWCEQOS_MAC_MDIO_DATA);
	if ((u_phyreg & MII_ADDR_C45) != MII_ADDR_C45) {/*c22*/
		regval = DWCEQOS_MDIO_PHYADDR(u_mii_id) |
			DWCEQOS_MDIO_PHYREG(u_phyreg) |
			DWCEQOS_MAC_MDIO_ADDR_CR(priv->csr_val) |
			(u32)DWCEQOS_MAC_MDIO_ADDR_GB | (u32)DWCEQOS_MAC_MDIO_ADDR_GOC_WRITE;
	}  else {/*c45*/
			regval = (u32)EQOS_MAC_MDIO_ADDR_C45E |
			DWCEQOS_MDIO_PHYADDR(u_mii_id) |
			DWCEQOS_MDIO_PHYREG_DEV(u_phyreg) |
			DWCEQOS_MAC_MDIO_ADDR_CR(priv->csr_val) |
			(u32)DWCEQOS_MAC_MDIO_ADDR_GB | (u32)DWCEQOS_MAC_MDIO_ADDR_GOC_WRITE;
		iowritel(DWCEQOS_MAC_MDIO_DATA_RA(u_phyreg) | value, priv->ioaddr, REG_DWCEQOS_MAC_MDIO_DATA);
	}
	iowritel(regval, priv->ioaddr, REG_DWCEQOS_MAC_MDIO_ADDR);

	timeout_jiffies = jiffies + msecs_to_jiffies(MDIO_TIMEOUT_MS);
	while ((ioreadl(priv->ioaddr, REG_DWCEQOS_MAC_MDIO_ADDR) &
		      DWCEQOS_MAC_MDIO_ADDR_GB) != 0x0U) {
		if (time_is_before_jiffies(timeout_jiffies)) {/*PRQA S 2996*/
			/*avoid scheduling timeouts*/
			regval = ioreadl(priv->ioaddr, REG_DWCEQOS_MAC_MDIO_ADDR);
			if ((regval & DWCEQOS_MAC_MDIO_ADDR_GB) == 0x0U)
				break;
			netdev_err(ndev, "MDIO write timed out\n");
			return MDIO_VALUE_MASK;
		}
		usleep_range(DELAY_64US, DELAY_128US);
	}

	return 0;
}

/**
 * get_dt_phy
 * @plat: plat_config_data struct pointer
 * @np: device_node struct poniter
 * @dev: device struct pointer
 * Description: get phy config from device tree.
 * Return: 0 success; otherwise error
 */
static s32 get_dt_phy(struct plat_config_data *plat, struct device_node *np, struct device *dev)
{
	bool mdio = (bool)true;

	plat->phy_node = of_parse_phandle(np, "phy-handle", 0);
	if (NULL != plat->phy_node)
		dev_dbg(dev, "found phy-handle subnode\n");/*PRQA S 1294, 0685*/

	if ((NULL == plat->phy_node) && of_phy_is_fixed_link(np)) {
		if ((of_phy_register_fixed_link(np) < 0)) {
			dev_dbg(dev, "do not found fixed-link subnode\n");/*PRQA S 1294, 0685*/
			return -ENODEV;
		}
		dev_dbg(dev, "Found fixed-link subnode\n");/*PRQA S 1294, 0685*/
		plat->phy_node = of_node_get(np);
		mdio = (bool)false;
	}

	plat->mdio_node = of_get_child_by_name(np, "mdio");
	if (NULL != plat->mdio_node) {
		mdio = (bool)true;
	}

	if (mdio) {
		plat->mdio_bus_data = (struct mdio_bus_data *)devm_kzalloc(dev, sizeof(struct mdio_bus_data), GFP_KERNEL);
		if (plat->mdio_bus_data == NULL) {
			dev_err(dev, "%s, out of memory\n", __func__);
			return -ENOMEM;
		}
	}
	return 0;
}



/**
 * get_tx_queue_dt
 * @q_node: device_node struct poniter
 * @plat: plat_config_data struct pointer
 * @queue: queue id
 * Description: get tx queue config from device tree.
 * Return: NA
 */
static void get_tx_queue_dt(struct device_node *q_node, struct plat_config_data *plat, u8 queue)
{
	if (0 != of_property_read_u32(q_node, "snps,weigh",&plat->tx_queues_cfg[queue].weight))
				plat->tx_queues_cfg[queue].weight = QUEUE_WEIGTH_VAL + (u32)queue;

	if (of_property_read_bool(q_node,"snps,dcb-algorithm")) {
		plat->tx_queues_cfg[queue].mode_to_use = MTL_QUEUE_DCB;
	} else if (of_property_read_bool(q_node, "snps,avb-algorithm")){
		plat->tx_queues_cfg[queue].mode_to_use = MTL_QUEUE_AVB;

		if (0 != of_property_read_u32(q_node,"snps,send_slope", &plat->tx_queues_cfg[queue].send_slope))
			plat->tx_queues_cfg[queue].send_slope =0x0;

		if (0 != of_property_read_u32(q_node, "snps,idle_slope", &plat->tx_queues_cfg[queue].idle_slope))
			plat->tx_queues_cfg[queue].idle_slope = 0x0;

		if (0 != of_property_read_u32(q_node, "snps,high_credit",&plat->tx_queues_cfg[queue].high_credit))
			plat->tx_queues_cfg[queue].high_credit = 0x0;

		if (0 != of_property_read_u32(q_node, "snps,low_credit",&plat->tx_queues_cfg[queue].low_credit))
			plat->tx_queues_cfg[queue].low_credit = 0x0;

	} else
		plat->tx_queues_cfg[queue].mode_to_use = MTL_QUEUE_DCB;

	if (0 != of_property_read_u32(q_node, "snps,priority",&plat->tx_queues_cfg[queue].prio)) {
		plat->tx_queues_cfg[queue].prio = 0;
		plat->tx_queues_cfg[queue].use_prio = (bool)false;
	} else {
		plat->tx_queues_cfg[queue].use_prio = (bool)true;
	}

}


/**
 * get_rx_queue_dt
 * @q_node: device_node struct poniter
 * @plat: plat_config_data struct pointer
 * @queue: queue id
 * Description: get rx queue config from device tree.
 * Return: NA
 */
static void get_rx_queue_dt(struct device_node *q_node, struct plat_config_data *plat, u8 queue)
{
	if (of_property_read_bool(q_node, "snps,dcb-algorithm"))
		plat->rx_queues_cfg[queue].mode_to_use = MTL_QUEUE_DCB;
	else if (of_property_read_bool(q_node,"snps,avb-algorithm"))
		plat->rx_queues_cfg[queue].mode_to_use = MTL_QUEUE_AVB;
	else
		plat->rx_queues_cfg[queue].mode_to_use = MTL_QUEUE_DCB;

	if (0 != of_property_read_u32(q_node, "snps,map-to-dma-channel",&plat->rx_queues_cfg[queue].chan))
		plat->rx_queues_cfg[queue].chan = queue;

	if (0 != of_property_read_u32(q_node,"snps,priority",&plat->rx_queues_cfg[queue].prio)) {
		plat->rx_queues_cfg[queue].prio = 0;
		plat->rx_queues_cfg[queue].use_prio = (bool)false;
	} else {
		plat->rx_queues_cfg[queue].use_prio = (bool)true;
	}


	if (of_property_read_bool(q_node,"snps,route-avcp"))
		plat->rx_queues_cfg[queue].pkt_route = (u8)PACKET_AVCPQ;
	else if (of_property_read_bool(q_node,"snps,route-ptp"))
		plat->rx_queues_cfg[queue].pkt_route = (u8)PACKET_PTPQ;
	else if (of_property_read_bool(q_node,"snps,route-dcbcp"))
		plat->rx_queues_cfg[queue].pkt_route = (u8)PACKET_DCBCPQ;
	else
		plat->rx_queues_cfg[queue].pkt_route = 0x0;

}

/**
 * get_mtl_dt
 * @pdev: platform_device struct poniter
 * @plat: plat_config_data struct pointer
 * Description: get mtl config from device tree.
 * called by eth_probe_config_dt
 * Return: 0 success; otherwise error
 */
static s32 get_mtl_dt(const struct platform_device *pdev, struct plat_config_data *plat)
{
	struct device_node *q_node;
	struct device_node *rx_node;
	struct device_node *tx_node;
	u8 queue = 0;
	s32 ret = 0;

	plat->rx_queues_cfg[0].mode_to_use = MTL_QUEUE_DCB;
	plat->tx_queues_cfg[0].mode_to_use = MTL_QUEUE_DCB;

	rx_node = of_get_child_by_name(pdev->dev.of_node,"snps,mtl-rx-config");
	if (NULL == rx_node) {
		dev_err(&pdev->dev, "%s, snps,mtl-rx-config rx-node is NULL\n", __func__);
		return -EINVAL;
	}

	tx_node = of_get_child_by_name(pdev->dev.of_node,"snps,mtl-tx-config");
	if (NULL == tx_node) {
		dev_err(&pdev->dev, "%s, snps,mtl-tx-config tx-node is NULL\n", __func__);
		ret = -EINVAL;
		goto err_rx_node;
	}

	if (0 != of_property_read_u32(rx_node, "snps,rx-queues-to-use", &plat->rx_queues_to_use))
		plat->rx_queues_to_use = 1;

	if (0 != of_property_read_u32(tx_node, "snps,tx-queues-to-use", &plat->tx_queues_to_use))
		plat->tx_queues_to_use = 1;


	if (of_property_read_bool(rx_node,"snps,rx-sched-sp"))
		plat->rx_sched_algorithm = MTL_RX_ALGORITHM_SP;
	else if (of_property_read_bool(rx_node,"snps,rx-sched-wsp"))
		plat->rx_sched_algorithm = MTL_RX_ALGORITHM_WSP;
	else
		plat->rx_sched_algorithm = MTL_RX_ALGORITHM_SP;

	for_each_child_of_node(rx_node, q_node) {
		if (queue >= plat->rx_queues_to_use)
			break;

		get_rx_queue_dt(q_node, plat, queue);
		queue++;
	}

	if (of_property_read_bool(tx_node,"snps,tx-sched-wrr"))
		plat->tx_sched_algorithm = MTL_TX_ALGORITHM_WRR;
	else if (of_property_read_bool(tx_node,"snps,tx-sched-sp"))
		plat->tx_sched_algorithm = MTL_TX_ALGORITHM_SP;
	else
		plat->tx_sched_algorithm = MTL_TX_ALGORITHM_SP;

	queue = 0;

	for_each_child_of_node(tx_node, q_node) {
		if (queue >= plat->tx_queues_to_use)
			break;

		get_tx_queue_dt(q_node, plat, queue);
		queue++;

	}

	of_node_put(q_node);
	of_node_put(tx_node);
err_rx_node:
	of_node_put(rx_node);
	return ret;
}

/**
 * get_clk_dt
 * @pdev: platform_device struct poniter
 * @plat: plat_config_data struct pointer
 * Description: get clk config from device tree.
 * called by eth_probe_config_dt
 * Return: 0 success; otherwise error
 */
static s32 get_clk_dt(struct platform_device *pdev, struct plat_config_data *plat)
{
	s32 ret = 0;

	/* ptp ref clk */
	plat->clk_ptp_ref = devm_clk_get(&pdev->dev, "ptp_ref");
	if (IS_ERR((void *)plat->clk_ptp_ref)) {
		dev_err(&pdev->dev, "ethernet: ptp cloock not found\n");
		goto err_ptp_clk;
	}
	ret = clk_prepare_enable(plat->clk_ptp_ref);
	if (ret < 0) {
		dev_err(&pdev->dev, "unable to enable ptp clk\n");
		goto err_ptp_clk;
	}
	plat->clk_ptp_rate = (u32)clk_get_rate(plat->clk_ptp_ref);
	pr_debug("%s, clk_ptp_rate:%u\n", __func__, plat->clk_ptp_rate); /*PRQA S 1294, 0685*/

	/* apb clk */
	plat->eth_apb_clk = devm_clk_get(&pdev->dev, "apb_clk");
	if (IS_ERR((void *)plat->eth_apb_clk)) {
		dev_err(&pdev->dev, "apb_clk not found\n");
		goto err_apb_clk;
	}
	ret = clk_prepare_enable(plat->eth_apb_clk);
	if (ret < 0) {
		dev_err(&pdev->dev, "unable to enable apb_clk clk\n");
		goto err_apb_clk;
	}

	/* axi clk */
	plat->eth_bus_clk = devm_clk_get(&pdev->dev, "axi_clk");
	if (IS_ERR((void *)plat->eth_bus_clk)) {
		dev_err(&pdev->dev, "axi_clk not found\n");
		goto err_bus_clk;
	}
	ret = clk_prepare_enable(plat->eth_bus_clk);
	if (ret < 0) {
		dev_err(&pdev->dev, "unable to enable axi_clk clk\n");
		goto err_bus_clk;
	}

	/* mac clk */
	plat->eth_mac_clk = devm_clk_get(&pdev->dev, "rgmii_clk");
	if (IS_ERR((void *)plat->eth_mac_clk)) {
		dev_err(&pdev->dev, "rgmii_clk not found\n");
		goto err_mac_clk;
	}
	ret = clk_prepare_enable(plat->eth_mac_clk);
	if (ret < 0) {
		dev_err(&pdev->dev, "unable to enable rgmii_clk\n");
		goto err_mac_clk;
	}

	/* phy ref clk */
	plat->phy_ref_clk = devm_clk_get(&pdev->dev, "ref_clk");
	if (IS_ERR((void *)plat->phy_ref_clk)) {
		dev_err(&pdev->dev, "ref_clk not found\n");
		goto err_ref_clk;
	}
	ret = clk_prepare_enable(plat->phy_ref_clk);
	if (ret < 0) {
		dev_err(&pdev->dev, "unable to enable ref_clk\n");
		goto err_ref_clk;
	}

	return ret;

err_ref_clk:
	clk_disable_unprepare(plat->eth_mac_clk);
err_mac_clk:
	clk_disable_unprepare(plat->eth_bus_clk);
err_bus_clk:
	clk_disable_unprepare(plat->eth_apb_clk);
err_apb_clk:
	clk_disable_unprepare(plat->clk_ptp_ref);
err_ptp_clk:
	return -EINVAL;
}


/**
 * get_burst_map_dt
 * @np: device_node struct poniter
 * @plat: plat_config_data struct pointer
 * Description: get burst map config from device tree.
 * called by get_axi_cfg_dt
 * Return: NA
 */
static void get_burst_map_dt(struct device_node *np, struct plat_config_data *plat)
{
	u32 burst_map = 0;
	u32 bit_index;

	(void)of_property_read_u32(np,"snps,burst-map",&burst_map);
	for (bit_index = 0; bit_index < (u32)AXI_BLEN; bit_index++) {
		if (0U != (burst_map & (1U << bit_index))) {/*PRQA S 1891*/
			switch (bit_index) {
				case BIT_INDEX_0:
					plat->axi->axi_blen[bit_index] = SZ_4;
					break;
				case BIT_INDEX_1:
					plat->axi->axi_blen[bit_index] = SZ_8;
					break;
				case BIT_INDEX_2:
					plat->axi->axi_blen[bit_index] = SZ_16;
					break;
				case BIT_INDEX_3:
					plat->axi->axi_blen[bit_index] = SZ_32;
					break;
				case BIT_INDEX_4:
					plat->axi->axi_blen[bit_index] = SZ_64;
					break;
				case BIT_INDEX_5:
					plat->axi->axi_blen[bit_index] = SZ_128;
					break;
				case BIT_INDEX_6:
					plat->axi->axi_blen[bit_index] = SZ_256;
					break;
				default:
					(void)pr_warn("%s, snps,burst-map %u do not support\n", __func__, burst_map);
					break;
			}
		}
	}

}


/**
 * get_dma_cfg_dt
 * @np: device_node struct poniter
 * @plat: plat_config_data struct pointer
 * @pdev: platform_device struct pointer
 * Description: get dma config form device tree
 * called by eth_probe_config_dt
 * Return: NA
 */
static void get_dma_cfg_dt(struct device_node *np, struct plat_config_data *plat, struct dma_ctrl_cfg *dma_cfg)
{
	plat->dma_cfg = dma_cfg;

	(void)of_property_read_s32(np, "snps,pbl",&dma_cfg->pbl);
	if (0 == dma_cfg->pbl)
		dma_cfg->pbl = DEFAULT_DMA_PBL;

	(void)of_property_read_u32(np, "snps,txpbl", &dma_cfg->txpbl);
	(void)of_property_read_u32(np, "snps,rxpbl", &dma_cfg->rxpbl);
	dma_cfg->pblx8 = !of_property_read_bool(np, "snps,no-plb-x8");
	dma_cfg->aal = of_property_read_bool(np, "snps,aal");
	dma_cfg->fixed_burst = of_property_read_bool(np, "snps,fixed-burst");
	dma_cfg->mixed_burst = of_property_read_bool(np, "snps,mixed-burst");

	plat->force_thresh_dma_mode = of_property_read_bool(np,"snps,force_thresh_dma_mode");
	if (plat->force_thresh_dma_mode) {
		plat->force_sf_dma_mode = (bool)false;
	}
	(void)of_property_read_s32(np, "snps,ps-speed",&plat->mac_port_sel_speed);

}


/**
 * get_axi_cfg_dt
 * @np: device_node struct poniter
 * @plat: plat_config_data struct pointer
 * @pdev: platform_device struct pointer
 * Description: get axi config form device tree
 * called by eth_probe_config_dt
 * Return: NA
 */
static void get_axi_cfg_dt(struct device_node *np, struct plat_config_data *plat, struct platform_device *pdev)
{
	plat->axi->axi_lpi_en = of_property_read_bool(np,"snps,en-lpi");
	if (0 != of_property_read_u32(np,"snps,write-requests",&plat->axi->axi_wr_osr_lmt))
		plat->axi->axi_wr_osr_lmt = 1;
	else
		plat->axi->axi_wr_osr_lmt--;

	if (0 != of_property_read_u32(np,"snps,read-requests",&plat->axi->axi_rd_osr_lmt))
		plat->axi->axi_rd_osr_lmt = 1;
	else
		plat->axi->axi_rd_osr_lmt--;

	get_burst_map_dt(np, plat);
}







/**
 * eth_probe_config_dt
 * @pdev: platform_device struct poniter
 * @mac: mac address
 * Description: get config from device tree
 * Return: >0:struct plat_config_data; <0:error
 */
static struct plat_config_data *eth_probe_config_dt(struct platform_device *pdev, const char **mac)
{
	struct device_node *np = pdev->dev.of_node;
	struct plat_config_data *plat;
	struct dma_ctrl_cfg *dma_cfg;
	void * err_ptr  = ERR_PTR(-EPROBE_DEFER);
	s32 ret;

	plat = (struct plat_config_data *)devm_kzalloc(&pdev->dev, sizeof(*plat), GFP_KERNEL);
	if (NULL == plat) {
		err_ptr = ERR_PTR(-ENOMEM);
		goto err_kalloc;
	}
	ret = of_get_mac_address(np, (u8 *)*mac);
	if (!ret) {
		*mac = NULL;
	}

	of_get_phy_mode(np, (phy_interface_t *)&plat->interface);

	/* PHYLINK automatically parses the phy-handle property */
	plat->phylink_node = np;

	if (0 != of_property_read_u32(np, "max-speed", &plat->max_speed))
		plat->max_speed = 0;


	if (0 != get_dt_phy(plat, np, &pdev->dev)) {
		err_ptr = ERR_PTR(-ENODEV);
		goto free_plat;
	}

	(void)of_property_read_s32(np, "tx-fifo-depth", &plat->tx_fifo_size);
	(void)of_property_read_s32(np, "rx-fifo-depth", &plat->rx_fifo_size);

	plat->force_sf_dma_mode = of_property_read_bool(np, "snps,force_sf_dma_mode");
	plat->en_tx_lpi_clockgating = of_property_read_bool(np, "snps,en-tx-lpi-clockgating");

	plat->maxmtu = JUMBO_LEN;

	plat->tso_en = of_property_read_bool(np, "snps,tso");
	plat->fp_en = of_property_read_bool(np, "snps,fp");

	dma_cfg = (struct dma_ctrl_cfg *)devm_kzalloc(&pdev->dev, sizeof(*dma_cfg), GFP_KERNEL);
	if (NULL == dma_cfg) {
		err_ptr = ERR_PTR(-ENOMEM);
		goto free_plat;
	}

	get_dma_cfg_dt(np, plat, dma_cfg);

	plat->axi = (struct axi_cfg *)devm_kzalloc(&pdev->dev, sizeof(struct axi_cfg), GFP_KERNEL);
	if (NULL == plat->axi) {
		err_ptr = ERR_PTR(-ENOMEM);
		goto free_dma_cfg;
	}
	get_axi_cfg_dt(np, plat, pdev);

	ret = get_mtl_dt(pdev, plat);
	if (ret < 0) {
		dev_err(&pdev->dev, "can not get mtl dtb config\n");
		goto free_axi;
	}
	ret = get_clk_dt(pdev, plat);
	if (ret < 0) {
		dev_err(&pdev->dev, "can not get clk dtb config\n");
		goto free_axi;
	}

	return plat;

free_axi:
	devm_kfree(&pdev->dev, (void *)plat->axi);
	plat->axi = NULL;
free_dma_cfg:
	devm_kfree(&pdev->dev, (void *)dma_cfg);
free_plat:
	devm_kfree(&pdev->dev, (void *)plat);
err_kalloc:
	return (struct plat_config_data *)err_ptr;
}


/**
 * get_platform_resources
 * @pdev: platform_device struct poniter
 * @mac_res: mac_resource struct pointer
 * Description: get platform resources, include irq, ioremap et al.
 * Return: 0 on success, otherwise error.
 */
static s32 get_platform_resources(struct platform_device *pdev, struct mac_resource *mac_res)
{
	s32 ret = -ENODEV;
	struct resource *res;
	mac_res->irq = platform_get_irq(pdev, 0);
	if (mac_res->irq <= 0) {
		dev_err(&pdev->dev, "get irq resouce failed\n");
		goto err_get_res;
	}

	/* wol_irq is optional */
	mac_res->wol_irq = platform_get_irq(pdev, 1);
	if (mac_res->wol_irq <= 0)
		dev_info(&pdev->dev, "No wol_irq resouce.\n");

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (NULL == res) {
		dev_err(&pdev->dev, "get plat resouce failed\n");
		goto err_get_res;
	}
	mac_res->addr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(mac_res->addr)) {
		ret = (s32)PTR_ERR(mac_res->addr);
		dev_err(&pdev->dev, "error ioremap\n");
		goto err_get_res;
	}

	return 0;
err_get_res:
	return ret;
}



/**
 * get_HW_feature_0
 * @ioaddr: ioaddr struct poniter
 * @dma_cap: dma_features struct pointer
 * Description: read MAC feature0 register to get feature0 informations.
 * called by get_HW_features
 * Return: NA
 */
static void get_HW_feature0(const void __iomem *ioaddr, struct dma_features *dma_cap)
{
	u32 hw_cap = ioreadl(ioaddr, GMAC_HW_FEATURE0);

	/*  MAC HW feature0 */
	dma_cap->mbps_10_100 = (hw_cap & (u32)GMAC_HW_FEAT_MIISEL);
	dma_cap->mbps_1000 = (hw_cap & (u32)GMAC_HW_FEAT_GMIISEL) >> 1;
	dma_cap->half_duplex = (hw_cap & (u32)GMAC_HW_FEAT_HDSEL) >> BIT_INDEX_2;
	dma_cap->hash_filter = (hw_cap & (u32)GMAC_HW_FEAT_VLHASH) >> BIT_INDEX_4;
	dma_cap->multi_addr = (hw_cap & (u32)GMAC_HW_FEAT_ADDMAC) >> BIT_INDEX_18;
	dma_cap->pcs = (hw_cap & (u32)GMAC_HW_FEAT_PCSSEL) >> BIT_INDEX_3;
	dma_cap->sma_mdio = (hw_cap & (u32)GMAC_HW_FEAT_SMASEL) >> BIT_INDEX_5;
	dma_cap->pmt_remote_wake_up = (hw_cap & (u32)GMAC_HW_FEAT_RWKSEL) >> BIT_INDEX_6;
	dma_cap->pmt_magic_frame = (hw_cap & (u32)GMAC_HW_FEAT_MGKSEL) >> BIT_INDEX_7;
	/* MMC */
	dma_cap->rmon = (hw_cap & (u32)GMAC_HW_FEAT_MMCSEL) >> BIT_INDEX_8;
	/* IEEE 1588-2008 */
	dma_cap->atime_stamp = (hw_cap & (u32)GMAC_HW_FEAT_TSSEL) >> BIT_INDEX_12;
	/* 802.3az - Energy-Efficient Ethernet (EEE) */
	dma_cap->eee = (hw_cap & (u32)GMAC_HW_FEAT_EEESEL) >> BIT_INDEX_13;
	/* TX and RX csum */
	dma_cap->tx_coe = (hw_cap & (u32)GMAC_HW_FEAT_TXCOSEL) >> BIT_INDEX_14;
	dma_cap->rx_coe =  (hw_cap & (u32)GMAC_HW_FEAT_RXCOESEL) >> BIT_INDEX_16;

}


/**
 * get_HW_feature_1
 * @ioaddr: ioaddr struct poniter
 * @dma_cap: dma_features struct pointer
 * Description: read MAC feature1 register to get feature1 informations.
 * called by get_HW_features
 * Return: NA
 */
static void get_HW_feature1(const void __iomem *ioaddr, struct dma_features *dma_cap)
{
	u32 hw_cap = ioreadl(ioaddr, GMAC_HW_FEATURE1);

	/* MAC HW feature1 */
	dma_cap->av = (hw_cap & (u32)GMAC_HW_FEAT_AVSEL) >> BIT_INDEX_20;
	dma_cap->tsoen = (hw_cap & (u32)GMAC_HW_TSOEN) >> BIT_INDEX_18;
	/* RX and TX FIFO sizes are encoded as log2(n / 128). Undo that by
	 * shifting and store the sizes in bytes.
	 */
	dma_cap->tx_fifo_size = (u32)SZ_128 << ((hw_cap & GMAC_HW_TXFIFOSIZE) >> BIT_INDEX_6);
	dma_cap->rx_fifo_size = (u32)SZ_128 << (hw_cap & GMAC_HW_RXFIFOSIZE);

}

/**
 * get_HW_feature_2
 * @ioaddr: ioaddr struct poniter
 * @dma_cap: dma_features struct pointer
 * Description: read MAC feature2 register to get feature2 informations.
 * called by get_HW_features
 * Return: NA
 */
static void get_HW_feature2(const void __iomem *ioaddr, struct dma_features *dma_cap)
{
	u32 hw_cap = ioreadl(ioaddr, GMAC_HW_FEATURE2);

	/* MAC HW feature2 */

	/* TX and RX number of channels */
	dma_cap->number_rx_channel = ((hw_cap & GMAC_HW_FEAT_RXCHCNT) >> BIT_INDEX_12) + 1U;
	dma_cap->number_tx_channel = ((hw_cap & GMAC_HW_FEAT_TXCHCNT) >> BIT_INDEX_18) + 1U;
	/* TX and RX number of queues */
	dma_cap->number_rx_queues = (hw_cap & GMAC_HW_FEAT_RXQCNT) + 1U;
	dma_cap->number_tx_queues = ((hw_cap & GMAC_HW_FEAT_TXQCNT) >> BIT_INDEX_6) + 1U;

	dma_cap->pps_out_num = (hw_cap & GMAC_HW_FEAT_PPSOUTNUM) >> BIT_INDEX_24;

}

/**
 * get_HW_feature_3
 * @ioaddr: ioaddr struct poniter
 * @dma_cap: dma_features struct pointer
 * Description: read MAC feature3 register to get feature3 informations.
 * called by get_HW_features
 * Return: NA
 */
static void get_HW_feature3(const void __iomem *ioaddr, struct dma_features *dma_cap)
{
	u32 hw_cap = ioreadl(ioaddr, GMAC_HW_FEATURE3);

	/*get HW feature3 */
	dma_cap->asp = (hw_cap & GMAC_HW_FEAT_ASP) >> BIT_INDEX_28;
	dma_cap->frpsel = (hw_cap & (u32)GMAC_HW_FEAT_FRPSEL) >> BIT_INDEX_10;
	dma_cap->frpes = (hw_cap & GMAC_HW_FEAT_FRPES) >> BIT_INDEX_13;

	//dma_cap->tsn_frame_preemption = (hw_cap & GMAC_HW_FEAT_FPESEL) >> 26;
	dma_cap->tbssel = (hw_cap & (u32)GMAC_HW_FEAT_TBSSEL) >> BIT_INDEX_27;
	dma_cap->fpesel = (hw_cap & (u32)GMAC_HW_FEAT_FPESEL) >> BIT_INDEX_26;
	dma_cap->estwid	= (hw_cap & GMAC_HW_FEAT_ESTWID) >> BIT_INDEX_20;
	dma_cap->estdep = (hw_cap & GMAC_HW_FEAT_ESTDEP) >> BIT_INDEX_17;
	//dma_cap->tsn_enh_sched_traffic = (hw_cap & GMAC_HW_FEAT_ESTSEL) >> 16;
	dma_cap->estsel = (hw_cap & (u32)GMAC_HW_FEAT_ESTSEL) >> BIT_INDEX_16;
	//dma_cap->tsn = dma_cap->tsn_frame_preemption | dma_cap->tsn_enh_sched_traffic;
	dma_cap->tsn = dma_cap->fpesel | dma_cap->estsel;
	/* IEEE 1588-2002 */
	dma_cap->time_stamp = 0;

}



/**
 * get_HW_feature
 * @ioaddr: ioaddr struct poniter
 * @dma_cap: dma_features struct pointer
 * Description: read all MAC feature register to get all feature informations.
 * Return: 1:success
 */
static void get_hw_features(const void __iomem *ioaddr, struct dma_features *dma_cap)
{
	/* get HW feature0 */
	get_HW_feature0(ioaddr,dma_cap);
	/* get HW feature1 */
	get_HW_feature1(ioaddr,dma_cap);
	/* get HW feature2 */
	get_HW_feature2(ioaddr,dma_cap);
	/* get HW feature3 */
	get_HW_feature3(ioaddr,dma_cap);

	switch (dma_cap->estwid) {
		case BIT_INDEX_0:
			dma_cap->estwid = 0;
			break;
		case BIT_INDEX_1:
			dma_cap->estwid = EST_WIDTH16;
			break;
		case BIT_INDEX_2:
			dma_cap->estwid = EST_WIDTH20;
			break;
		case BIT_INDEX_3:
			dma_cap->estwid = EST_WIDTH24;
			break;
		default:
			(void)pr_warn("%s, estwid %u is invalid value\n", __func__, dma_cap->estwid);
			break;
	}

	switch (dma_cap->estdep) {
		case BIT_INDEX_0:
			dma_cap->estdep = 0;
			break;
		case BIT_INDEX_1:
			dma_cap->estdep = SZ_64;
			break;
		case BIT_INDEX_2:
			dma_cap->estdep = SZ_128;
			break;
		case BIT_INDEX_3:
			dma_cap->estdep = SZ_256;
			break;
		case BIT_INDEX_4:
			dma_cap->estdep = SZ_512;
			break;
		default:
			(void)pr_warn("%s, estdep %u is invalid value\n", __func__, dma_cap->estdep);
			break;
	}

	switch (dma_cap->frpes) {
		case BIT_INDEX_0:
			dma_cap->frpes = SZ_64;
			break;
		case BIT_INDEX_1:
			dma_cap->frpes = SZ_128;
			break;
		case BIT_INDEX_2:
			dma_cap->frpes = SZ_256;
			break;
		default:
			(void)pr_warn("%s, frpes %u is invalid value\n", __func__, dma_cap->frpes);
			break;
	}
	return;
}


/**
 * eth_cfg_chcek
 * @priv: hobot private struct poniter
 * Description: check eth driver config
 * called by eth_drv_probe
 * Return: NA
 */
static s32 eth_cfg_chcek(struct hobot_priv *priv)
{
	priv->ndev->priv_flags |= (u32)IFF_UNICAST_FLT;
	get_hw_features(priv->ioaddr, &priv->dma_cap);

	priv->plat->pmt = priv->dma_cap.pmt_remote_wake_up;
	priv->mmcaddr = (void *)((u64)priv->ioaddr + MMC_GMAC4_OFFSET);
	/* TXCOE doesn't work in thresh DMA mode */
	if (priv->plat->force_thresh_dma_mode)
		priv->plat->tx_coe = 0;
	else
		priv->plat->tx_coe = priv->dma_cap.tx_coe;

	/* In case of GMAC4 rx_coe is from HW cap register. */
	priv->plat->rx_coe = priv->dma_cap.rx_coe;
	if (0U != priv->dma_cap.rx_coe_type2)
		priv->plat->rx_coe = STMMAC_RX_COE_TYPE2;
	else if (0U != priv->dma_cap.rx_coe_type1)
		priv->plat->rx_coe = STMMAC_RX_COE_TYPE1;


	if (0U != priv->plat->pmt) {
		dev_info(priv->device, "Wake-Up On Lan supported\n");
		device_set_wakeup_capable(priv->device, (bool)true);
	}

	if (0U != priv->dma_cap.tsoen)
		dev_dbg(priv->device, "TSO supported\n");/*PRQA S 1294, 0685*/
	return 0;
}

/**
 * mdio_set_csr
 * @priv: hobot private struct poniter
 * Description: set mdio csr
 * called by eth_drv_probe
 * Return: 0 on success, otherwise error.
 */
static s32 mdio_set_csr(struct hobot_priv *priv)
{
	s32 rate = (s32)clk_get_rate(priv->plat->eth_bus_clk);
	s32 ret = 0;

	if (rate <= CSR_F_35M)
		priv->csr_val = MAC_MDIO_ADDR_CR_DIV_16;
	else if (rate <= CSR_F_60M)
		priv->csr_val = MAC_MDIO_ADDR_CR_DIV_26;
	else if (rate <= CSR_F_100M)
		priv->csr_val = MAC_MDIO_ADDR_CR_DIV_42;
	else if (rate <= CSR_F_150M)
		priv->csr_val = MAC_MDIO_ADDR_CR_DIV_62;
	else if (rate <= CSR_F_250M)
		priv->csr_val = MAC_MDIO_ADDR_CR_DIV_102;
	else if (rate <= CSR_F_300M)
		priv->csr_val = MAC_MDIO_ADDR_CR_DIV_124;
	else if (rate <= CSR_F_500M)
		priv->csr_val = MAC_MDIO_ADDR_CR_DIV_204;
	else if (rate <= CSR_F_800M)
		priv->csr_val = MAC_MDIO_ADDR_CR_DIV_324;
	else
		ret = -1;
	return ret;
}


/**
 * mdio_register
 * @priv: hobot_priv struct poniter
 * Description: register mdio bus
 * Return: 0 on success, otherwise error.
 */
static s32 mdio_register(struct hobot_priv *priv)
{
	s32 err;
	struct net_device *ndev = priv->ndev;
	struct device_node *mdio_node = priv->plat->mdio_node;
	struct device *dev = ndev->dev.parent;
	struct mii_bus *new_bus;
	struct resource res;
	struct mdio_bus_data *mdio_bus_data = priv->plat->mdio_bus_data;

	if (NULL == mdio_bus_data)
		return 0;

	new_bus = mdiobus_alloc();
	if (NULL == new_bus) {
		netdev_err(ndev, "mdiobus_alloc failed\n");
		return  -ENOMEM;
	}
	new_bus->name = "hobot-mac-mdio";
	new_bus->read = &mdio_read_reg;
	new_bus->write = &mdio_write_reg;

	(void)of_address_to_resource(dev->of_node, 0, &res);
	(void)snprintf(new_bus->id, MII_BUS_ID_SIZE, "%s-%llx", new_bus->name, (u64)res.start);


	new_bus->priv = (void *)ndev;
	new_bus->parent = priv->device;

	if (NULL != mdio_node)
		err = of_mdiobus_register(new_bus, mdio_node);
	else
		err = mdiobus_register(new_bus);
	if (err != 0) {
		netdev_err(ndev, "Cannot register the MDIO bus\n");
		goto err_out;
	}

	priv->mii = new_bus;
	return 0;

err_out:
	mdiobus_free(new_bus);
	return  err;
}


/**
 * get_ptp_period
 * @priv: hobot_priv struct poniter
 * @ptp_clock: ptp clock value
 * Description: get ptp system period
 * Return: 0 on success, otherwise error.
 */
static u32 get_ptp_period(const struct hobot_priv *priv, u32 ptp_clock)
{
	void __iomem *ioaddr = priv->ioaddr;
	u64 data;
	u32 value = ioreadl(ioaddr, PTP_TCR);

	if (0U != (value & PTP_TCR_TSCFUPDT))
		data = PTP_FINE_PERIOD;
	else
		data = (PER_SEC_NS / ptp_clock);


	return (u32)data;
}


static u32 get_ptp_subperiod(const struct hobot_priv *priv, u32 ptp_clock)
{
	u32 value = ioreadl(priv->ioaddr, PTP_TCR);
	u32 data;

	if (0U != (value & PTP_TCR_TSCFUPDT))
		return 0;

	data = (u32)(PER_SEC_NS * SZ_1000U / ptp_clock) - ((get_ptp_period(priv, ptp_clock)) * SZ_1000U);
	return data;
}


static u32 config_sub_second_increment(const struct hobot_priv *priv, u32 ptp_clock)
{
	void __iomem *ioaddr = priv->ioaddr;
	u32 value = ioreadl(ioaddr, PTP_TCR);
	u32 subns, ns;
	u64 tmp;

	ns = get_ptp_period(priv, ptp_clock);
	subns = get_ptp_subperiod(priv, ptp_clock);

	if (0U == (value & PTP_TCR_TSCTRLSSR)) {
		tmp = ns * (u64)SZ_1000U;
		ns = DIV_ROUND_CLOSEST(tmp - (tmp % SZ_465U), SZ_465U); /*PRQA S 2997, 2895, 4461, 1891*/
		subns = DIV_ROUND_CLOSEST((tmp * SZ_256U) - (SZ_465U * ns * SZ_256U), SZ_465U); /*PRQA S 2997, 2895, 4461, 1891*/
	} else {
		subns = DIV_ROUND_CLOSEST(subns * SZ_256U, SZ_1000U); /*PRQA S 2997, 2895*/
	}

	ns &= (u32)PTP_SSIR_SSINC_MASK;
	subns &= (u32)PTP_SSIR_SNSINC_MASK;

	value = ns;
	value <<= GMAC4_PTP_SSIR_SSINC_SHIFT;
	value |= subns << GMAC4_PTP_SSIR_SNSINC_SHIFT;

	iowritel(value, ioaddr, PTP_SSIR);
	return ns;
}

static s32 config_addend(const struct hobot_priv *priv, u32 addend)
{
	void __iomem *ioaddr = priv->ioaddr;
	u32 value;

	iowritel(addend, ioaddr, PTP_TAR);
	value = ioreadl(ioaddr, PTP_TCR);
	value |= (u32)PTP_TCR_TSADDREG;
	iowritel(value, ioaddr, PTP_TCR);

	udelay(10);/*PRQA S 2880*/
	if ((ioreadl(ioaddr, PTP_TCR) & PTP_TCR_TSADDREG) != 0x0U) {
		netdev_err(priv->ndev, "%s, timed out\n", __func__);
		return -EBUSY;
	}

	return 0;
}


/**
 * ptp_init_systime
 * @priv: hobot_priv struct poniter
 * @sec:  second value
 * @nsec: nano second value
 * Description: ptp initalize system time
 * Return: 0 on success, otherwise error.
 */
static s32 ptp_init_systime(const struct hobot_priv *priv, u32 sec, u32 nsec)
{
	void __iomem *ioaddr = priv->ioaddr;
	u32 value;


	iowritel(sec, ioaddr, PTP_STSUR);
	iowritel(nsec, ioaddr, PTP_STNSUR);

	value = ioreadl(ioaddr, PTP_TCR);
	value |= (u32)PTP_TCR_TSINIT;
	iowritel(value, ioaddr, PTP_TCR);

	udelay(10);/*PRQA S 2880*/
	if ((ioreadl(ioaddr, PTP_TCR) & PTP_TCR_TSINIT) != 0x0U) {
		netdev_err(priv->ndev, "%s timed out\n", __func__);
		return -EBUSY;
	}

	return 0;
}


/* code review E1: do not need to return value */
static void config_hw_tstamping(const struct hobot_priv *priv, u32 data)
{
	iowritel(data, priv->ioaddr, PTP_TCR);
}



/**
 * est_init
 * @ndev: net_device struct poniter
 * @priv: hobot_priv struct poniter
 * @now:  timespec64 struct pointer
 * Description: initalize est function
 * Return: 0 on success, otherwise error.
 */
static s32 est_init(const struct net_device *ndev, const struct hobot_priv *priv, const struct timespec64 *now)
{
	void __iomem *ioaddr = priv->ioaddr;
	u32 control, real_btr[2];

	u8 ptov = 0;
	u32 i;
	struct est_cfg *cfg = &priv->plat->est_cfg;
	u32 estsel = priv->dma_cap.estsel;
	u32 estdep = priv->dma_cap.estdep;
	u32 estwid = priv->dma_cap.estwid;
	bool enable = priv->plat->est_en;

	if ((0U == estsel) || (0U == estdep) || (0U == estwid) || (NULL == cfg) || (!enable)) {/*PRQA S 2996*/
		netdev_err(ndev, "%s, No est or EST disabled\n", __func__);
		return -EINVAL;
	}
	if (cfg->gcl_size > estdep) {
		netdev_err(ndev, "%s, Invalid EST configuration supplied\n", __func__);
		return -EINVAL;
	}


	control = ioreadl(ioaddr, MTL_EST_CONTROL);
	control &= (u32)~MTL_EST_EEST;
	iowritel(control, ioaddr, MTL_EST_CONTROL);


	real_btr[0] = cfg->btr_offset[0] + (u32)now->tv_nsec;
	real_btr[1] = cfg->btr_offset[1] + (u32)now->tv_sec;


	if (est_write_reg(priv, MTL_EST_BTR_LOW, real_btr[0], (bool)false) < 0) {
		netdev_err(ndev, "%s:est btr low timeout\n", __func__);
		return -ETIMEDOUT;
	}
	if (est_write_reg(priv, (u32)MTL_EST_BTR_HIGH, real_btr[1], (bool)false) < 0) {
		netdev_err(ndev, "%s:est btr high timeout\n", __func__);
		return -ETIMEDOUT;
	}

	if (est_write_reg(priv, MTL_EST_CTR_LOW, cfg->ctr[0], (bool)false) < 0) {
		netdev_err(ndev, "%s:est ctr low timeout\n", __func__);
		return -ETIMEDOUT;
	}
	if (est_write_reg(priv, MTL_EST_CTR_HIGH, cfg->ctr[1], (bool)false) < 0) {
		netdev_err(ndev, "%s:est ctr high timeout\n", __func__);
		return -ETIMEDOUT;
	}
	if (est_write_reg(priv, MTL_EST_TER, cfg->ter, (bool)false) < 0) {
		netdev_err(ndev, "%s:est ter timeout\n", __func__);
		return -ETIMEDOUT;
	}
	if (est_write_reg(priv, MTL_EST_LLR, cfg->gcl_size, (bool)false) < 0) {
		netdev_err(ndev, "%s:est llr timeout\n", __func__);
		return -ETIMEDOUT;
	}

	for (i = 0; i < cfg->gcl_size; i++) {
		u32 reg = (i << MTL_EST_ADDR_OFFSET) & MTL_EST_ADDR;
		pr_debug("%s, %u gcl:0x%x\n", __func__, i, cfg->gcl[i]);/*PRQA S 1294, 0685*/
		if (est_write_reg(priv, reg, cfg->gcl[i], (bool)true) < 0) {
			netdev_err(ndev, "%s:est gcl timeout\n", __func__);
			return -ETIMEDOUT;
		}
	}

	if (0U != priv->plat->clk_ptp_rate) {
		ptov = (u8)(PER_SEC_NS / priv->plat->clk_ptp_rate);
		ptov *= (u8)MTL_EST_PTOV_MUL;
	}

	control = ioreadl(ioaddr, MTL_EST_CONTROL);
	control &= (u32)~MTL_EST_PTOV;
	control |= (u32)MTL_EST_EEST | ((u32)ptov << MTL_EST_PTOV_OFFSET);
	iowritel(control, ioaddr, MTL_EST_CONTROL);

	control |= (u32)MTL_EST_SSWL;
	iowritel(control, ioaddr, MTL_EST_CONTROL);

	return 0;
}


/**
 * est_configuration
 * @priv: hobot_priv struct poniter
 * Description: config est function
 * Return: 0 on success, otherwise error.
 */
static s32 est_configuration(struct hobot_priv *priv)
{
	struct timespec64 now;
	u32 control, sec_inc;
	s32 ret;
	u64 temp;


	if (!((0U != priv->dma_cap.time_stamp) || (0U != priv->adv_ts))) {
		netdev_err(priv->ndev, "%s, No HW time stamping and Disabling EST\n", __func__);
		priv->hwts_tx_en = 0;
		priv->hwts_rx_en = 0;
		priv->est_enabled = (bool)false;
		iowritel(0, priv->ioaddr, MTL_EST_CONTROL);
		return -EINVAL;
	}
	/* use ethtool disable est, est_en=0, go est_error direct to disable est */
	if(!priv->plat->est_en) {
		ret = -EINVAL;
		goto est_err;
	}
	control = 0;
	config_hw_tstamping(priv, control);

	priv->hwts_tx_en = 1;
	priv->hwts_rx_en = 1;

	control = (u32)PTP_TCR_TSENA | (u32)PTP_TCR_TSENALL | (u32)PTP_TCR_TSCTRLSSR;
	config_hw_tstamping(priv, control);

	sec_inc = config_sub_second_increment(priv, priv->plat->clk_ptp_rate);
	temp = div_u64(PER_SEC_NS, sec_inc);

	temp = (u64)(temp << BIT_INDEX_32);
	priv->default_addend = (u32)div_u64(temp, priv->plat->clk_ptp_rate);
	ret = config_addend(priv, priv->default_addend);
	if (ret < 0) {
		netdev_err(priv->ndev, "%s, config addend err\n", __func__);
		goto est_err;
	}
	ktime_get_real_ts64(&now);
	ret = ptp_init_systime(priv, (u32)now.tv_sec, (u32)now.tv_nsec);
	if (ret < 0) {
		netdev_err(priv->ndev, "%s, init ptp time err\n", __func__);
		goto est_err;
	}

	control = (u32)PTP_TCR_TSENA | (u32)PTP_TCR_TSINIT | (u32)PTP_TCR_TSENALL | (u32)PTP_TCR_TSCTRLSSR;
	config_hw_tstamping(priv, control);

	ret = est_init(priv->ndev, priv, &now);
	if (ret < 0) {
		netdev_err(priv->ndev, "%s, init est error", __func__);
		goto est_err;
	}

	priv->est_enabled = (bool)true;
	return 0;

est_err:
	priv->hwts_tx_en = 0;
	priv->hwts_rx_en = 0;
	priv->est_enabled = (bool)false;
	iowritel(0, priv->ioaddr, MTL_EST_CONTROL);
	return ret;
}


/* code review E1: do not need to return value */
static void tsn_fp_configure(const struct hobot_priv *priv)
{
	u32 control;
	u32 value;


	iowritel(MTL_FPE_RADV_HADV, priv->ioaddr, MTL_FPE_Advance);

	value = ioreadl(priv->ioaddr, GMAC_INT_EN);
	value |= (u32)GMAC_INT_FPEIE_EN;
	iowritel(value, priv->ioaddr, GMAC_INT_EN);


	control = ioreadl(priv->ioaddr, GMAC_Ext_CONFIG);
	control &= (u32)~DCRCC;
	iowritel(control, priv->ioaddr, GMAC_Ext_CONFIG);

	value = MTL_FPE_PEC | MTL_FPE_AFSZ;
	iowritel(value, priv->ioaddr, MTL_FPE_CTRL_STS);


	value = ioreadl(priv->ioaddr, GMAC_RXQ_CTRL1);
	value |= (u32)GMAC_RXQCTRL_FPRQ;
	iowritel(value, priv->ioaddr, GMAC_RXQ_CTRL1);



	control = ioreadl(priv->ioaddr, GMAC_FPE_CTRL_STS);
	control |= (u32)GMAC_FPE_EFPE;
	iowritel(control, priv->ioaddr, GMAC_FPE_CTRL_STS);

}

static int eth_netdev_setup_tc(struct net_device *ndev, enum tc_setup_type type,
			   void *type_data)
{
	struct hobot_priv *priv = netdev_priv(ndev);

	switch (type) {
	case TC_SETUP_QDISC_CBS:
		return tc_setup_cbs(priv, type_data);
	case TC_SETUP_QDISC_TAPRIO:
		return tc_setup_taprio(priv, type_data);
	default:
		return -EOPNOTSUPP;
	}
}


static u16 eth_netdev_tsn_select_queue(struct net_device *ndev,
					struct sk_buff *skb,
					struct net_device *sb_dev)
{
	u16 queue;

	unsigned int gso = skb_shinfo(skb)->gso_type;/*PRQA S 3305*/

	if ((gso & (SKB_GSO_TCPV4 | SKB_GSO_TCPV6 | SKB_GSO_UDP_L4))
			&& (ndev->hw_features & (NETIF_F_TSO | NETIF_F_TSO6))) {
		queue = (u16)AVB_BEST_EFF_Q;
	} else if (ndev->num_tc != 0) {
		queue = netdev_pick_tx(ndev, skb, NULL);
	} else {
		switch (vlan_get_protocol(skb)) {
		case htons(ETH_P_TSN): /*PRQA S 0591*/
			if (skb->priority == AVB_CLASSA_Q)
				queue = (u16)AVB_CLASSA_Q;
			else if (skb->priority == AVB_CLASSB_Q)
				queue = (u16)AVB_CLASSB_Q;
			else if (skb->priority == (u32)AVB_PTPCP_Q)
				queue = (u16)AVB_PTPCP_Q;
			else
				queue = (u16)AVB_BEST_EFF_Q;
			break;
		case htons(ETH_P_1588): /*PRQA S 0591*/
			queue = (u16)AVB_PTPCP_Q;
			break;
		default:
			queue = (u16)AVB_BEST_EFF_Q;
			break;
		}
	}
	pr_debug("type=0x%x, skb_prio=%u, queue=%hu, gso=%u, dev->num_tc=%hd\n",/*PRQA S 1294, 0685*/
		htons(vlan_get_protocol(skb)), skb->priority, queue, gso, ndev->num_tc);
	return queue % (u16)(ndev->real_num_tx_queues);
}

/**
 * set_mac_speed
 * @priv: hobot private struct pointer
 * @speed: speed value
 * @duplex: duplex value
 * Description: set mac speed and duplex modle
 * Return: NA
 */
/* code review E1: do not need to return value */
static void set_mac_speed(const struct hobot_priv *priv, int speed, int duplex)
{
	struct net_device *ndev = priv->ndev;
	u32 regval;

	regval = ioreadl(priv->ioaddr, GMAC_CONFIG);
	regval &= (u32)~(GMAC_CONFIG_PS | GMAC_CONFIG_FES | GMAC_CONFIG_DM);

	if (0 != duplex)
		regval |= (u32)GMAC_CONFIG_DM;

	if (speed == SPEED_10) {
		regval |= (u32)GMAC_CONFIG_PS;
	} else if (speed == SPEED_100) {
		regval |= (u32)(GMAC_CONFIG_PS | GMAC_CONFIG_FES);
	} else if (speed == SPEED_1000) {
		// do nothing
	} else {
		netdev_err(ndev, "Unknown PHY speed %d\n", speed);
		return;
	}

	reg_writel(priv, regval, GMAC_CONFIG); /*PRQA S 0497, 1294, 0685*/
}

/**
 * set_rx_flow_ctrl
 * @priv: hobot private struct pointer
 * @enable: enable flag
 * Description: set rx flow control enable or disable
 * Return: NA
 */
/* code review E1: do not need to return value */
static void set_rx_flow_ctrl(const struct hobot_priv *priv, bool enable)
{
	u32 regval;

	regval = ioreadl(priv->ioaddr, REG_DWCEQOS_MAC_RX_FLOW_CTRL);
	if (enable)
		regval |= (u32)DWCEQOS_MAC_RX_FLOW_CTRL_RFE;
	else
		regval &= (u32)~DWCEQOS_MAC_RX_FLOW_CTRL_RFE;

	iowritel(regval, priv->ioaddr, REG_DWCEQOS_MAC_RX_FLOW_CTRL);
}

/**
 * set_tx_flow_ctrl
 * @priv: hobot private struct pointer
 * @enable: enable flag
 * Description: set tx flow control enable or disable
 * Return: NA
 */
/* code review E1: do not need to return value */
static void set_tx_flow_ctrl(struct hobot_priv *priv, bool enable)
{
	u32 regval;

	regval = ioreadl(priv->ioaddr, REG_DWCEQOS_MTL_RXQ0_OPER);
	if (enable)
		regval |= (u32)DWCEQOS_MTL_RXQ_EHFC;
	else
		regval &= (u32)~DWCEQOS_MTL_RXQ_EHFC;

	iowritel(regval, priv->ioaddr, REG_DWCEQOS_MTL_RXQ0_OPER);
}

/**
 * set_link_up
 * @priv: hobot private struct pointer
 * Description: set mac link up
 * Return: NA
 */
/* code review E1: do not need to return value */
static void set_link_up(const struct hobot_priv *priv)
{
	u32 regval;

	regval = ioreadl(priv->ioaddr, REG_DWCEQOS_MAC_LPI_CTRL_STATUS);
	regval |= (u32)DWCEQOS_MAC_LPI_CTRL_STATUS_PLS;
	iowritel(regval, priv->ioaddr, REG_DWCEQOS_MAC_LPI_CTRL_STATUS);
}

/**
 * set_link_down
 * @priv: hobot private struct pointer
 * Description: set mac link down
 * Return: NA
 */
/* code review E1: do not need to return value */
static void set_link_down(const struct hobot_priv *priv)
{
	u32 regval;

	regval = ioreadl(priv->ioaddr, REG_DWCEQOS_MAC_LPI_CTRL_STATUS);
	regval &= (u32)~DWCEQOS_MAC_LPI_CTRL_STATUS_PLS;
	iowritel(regval, priv->ioaddr, REG_DWCEQOS_MAC_LPI_CTRL_STATUS);
}

/**
 * init_phy
 * @ndev: net_device struct pointer
 * Description: init phy framework
 * Return: 0 on success, otherwise error.
 */
static s32 init_phy(struct net_device *ndev)
{
	struct hobot_priv *priv = (struct hobot_priv *)netdev_priv(ndev);
	struct device_node *node;
	int ret = -ENODEV;


	node = priv->plat->phylink_node;
	if (NULL != node) {
		ret = phylink_of_phy_connect(priv->phylink, node, 0);
		if (ret < 0) {
			netdev_err(ndev, "no phy founded\n");
		}
	}
	return ret;
}


/**
 * dma_free_rx_buffer
 * @priv: hobot private struct pointer
 * @queue: queue id
 * @i: skbuff dma id
 * Description: free dma rx buffer
 * Return: NA
 */
/* code review E1: do not need to return value */
static void dma_free_rx_buffer(const struct hobot_priv *priv, u32 queue, s32 i)
{
	struct dma_rx_queue *rx_q  = (struct dma_rx_queue *)&priv->rx_queue[queue];/*PRQA S 0311*/

	if ((NULL != rx_q->rx_skbuff_dma) && (0U != rx_q->rx_skbuff_dma[i])) {
		dma_unmap_single(priv->device, rx_q->rx_skbuff_dma[i], priv->dma_buf_sz, DMA_FROM_DEVICE);
		rx_q->rx_skbuff_dma[i] = 0;
	}

	if ((NULL != rx_q->rx_skbuff) && (NULL != rx_q->rx_skbuff[i])) {
		dev_kfree_skb_any(rx_q->rx_skbuff[i]);
		rx_q->rx_skbuff[i] = NULL;
	}
}


/**
 * dma_free_rx_skbufs
 * @priv: hobot private struct pointer
 * @queue: queue id
 * Description: free dma rx skb buffers
 * Return: NA
 */
/* code review E1: do not need to return value */
static void dma_free_rx_skbufs(const struct hobot_priv *priv, u32 queue)
{
	s32 i;

	for (i = 0; i < DMA_RX_SIZE; i++)
		dma_free_rx_buffer(priv, queue, i);
}


/**
 * free_dma_rx_desc_resources
 * @priv: hobot private struct pointer
 * Description: free dma rx descriptor resources
 * Return: NA
 */
/* code review E1: do not need to return value */
static void free_dma_rx_desc_resources(struct hobot_priv *priv)
{
	u32 rx_count = priv->plat->rx_queues_to_use;
	u32 queue;

	for (queue = 0; queue < rx_count; queue++) {
		struct dma_rx_queue *rx_q = &priv->rx_queue[queue];

		dma_free_rx_skbufs(priv,queue);

		if ((NULL != rx_q->dma_rx) || (NULL != rx_q->dma_erx)) {
			if (0 == priv->extend_desc) {
				dma_free_coherent(priv->device, (u32)DMA_RX_SIZE * sizeof(struct dma_desc),
								(void *)rx_q->dma_rx, rx_q->dma_rx_phy);
				rx_q->dma_rx = NULL;
			} else {
				dma_free_coherent(priv->device, (u32)DMA_RX_SIZE * sizeof(struct dma_desc),
								(void *)rx_q->dma_erx, rx_q->dma_rx_phy);
				rx_q->dma_erx = NULL;
			}
		}
		if (NULL != rx_q->rx_skbuff_dma) {
			kfree((void *)rx_q->rx_skbuff_dma);
			rx_q->rx_skbuff_dma = NULL;
		}
		if (NULL != rx_q->rx_skbuff) {
			kfree((void *)rx_q->rx_skbuff);
			rx_q->rx_skbuff = NULL;
		}
	}
}


/**
 * alloc_dma_rx_desc_resources
 * @priv: hobot private struct pointer
 * Description: alloc dma rx descriptor resources
 * Return: 0 on success, otherwise error.
 */
static s32 alloc_dma_rx_desc_resources(struct hobot_priv *priv)
{
	u32 rx_count = priv->plat->rx_queues_to_use;
	s32 ret = -ENOMEM;
	u32 queue;

	for (queue = 0; queue < rx_count; queue++) {
		struct dma_rx_queue *rx_q = &priv->rx_queue[queue];

		rx_q->queue_index = queue;
		rx_q->priv_data = priv;
		rx_q->rx_skbuff_dma = (dma_addr_t *)kmalloc_array(DMA_RX_SIZE, sizeof(dma_addr_t), GFP_KERNEL);
		if (NULL == rx_q->rx_skbuff_dma)
			goto err_dma;
		memset((void*)rx_q->rx_skbuff_dma, 0x0, DMA_RX_SIZE*sizeof(dma_addr_t));

		rx_q->rx_skbuff = (struct sk_buff**)kmalloc_array(DMA_RX_SIZE, sizeof(struct sk_buff*), GFP_KERNEL);
		if (NULL == rx_q->rx_skbuff)
			goto err_dma;
		memset((void*)rx_q->rx_skbuff, 0x0, DMA_RX_SIZE*sizeof(struct sk_buff*));

		if (0 != priv->extend_desc) {
			rx_q->dma_erx = (struct dma_ext_desc *)dma_alloc_coherent(priv->device,
					(u32)DMA_RX_SIZE * sizeof(struct dma_ext_desc), &rx_q->dma_rx_phy, GFP_KERNEL);
			if (NULL == rx_q->dma_erx)
				goto err_dma;
		} else {
			rx_q->dma_rx = (struct dma_desc *)dma_alloc_coherent(priv->device,
					(u32)DMA_RX_SIZE * sizeof(struct dma_desc), &rx_q->dma_rx_phy, GFP_KERNEL);
			if (NULL == rx_q->dma_rx)
				goto err_dma;
		}
	}

	return 0;
err_dma:
	free_dma_rx_desc_resources(priv);
	return ret;
}


/**
 * dma_free_tx_buffer
 * @priv: hobot private struct pointer
 * @queue: queue id
 * @i: skbuff dma id
 * Description: free dma tx buffer
 * Return: NA
 */
/* code review E1: do not need to return value */
static void dma_free_tx_buffer(struct hobot_priv *priv, u32 queue, s32 i)
{
	struct dma_tx_queue *tx_q  = (struct dma_tx_queue *)&priv->tx_queue[queue];

	if (0U != tx_q->tx_skbuff_dma[i].buf) {
		if (tx_q->tx_skbuff_dma[i].map_as_page)
			dma_unmap_page(priv->device, tx_q->tx_skbuff_dma[i].buf,
					tx_q->tx_skbuff_dma[i].len, DMA_TO_DEVICE);
		else
			dma_unmap_single(priv->device, tx_q->tx_skbuff_dma[i].buf,
					tx_q->tx_skbuff_dma[i].len, DMA_TO_DEVICE);
	}

	if (NULL != tx_q->tx_skbuff[i]) {
		dev_kfree_skb_any(tx_q->tx_skbuff[i]);
		tx_q->tx_skbuff[i] = NULL;
		tx_q->tx_skbuff_dma[i].buf = 0;
		tx_q->tx_skbuff_dma[i].map_as_page = (bool)false;
	}
}


/**
 * dma_free_tx_skbufs
 * @priv: hobot private struct pointer
 * @queue: queue id
 * Description: free dma tx skb buffers
 * Return: NA
 */
/* code review E1: do not need to return value */
static void dma_free_tx_skbufs(struct hobot_priv *priv, u32 queue)
{
	s32 i;

	for (i = 0; i < DMA_TX_SIZE; i++) {
		dma_free_tx_buffer(priv, queue, i);
	}
}

/**
 * free_dma_tx_desc_resources
 * @priv: hobot private struct pointer
 * Description: free dma tx descriptor resources
 * Return: NA
 */
/* code review E1: do not need to return value */
static void free_dma_tx_desc_resources(struct hobot_priv *priv)
{
	u32 tx_count = priv->plat->tx_queues_to_use;
	u32 queue;

	for (queue = 0; queue < tx_count; queue++) {
		struct dma_tx_queue *tx_q  = &priv->tx_queue[queue];

		dma_free_tx_skbufs(priv, queue);

		if (0 == priv->extend_desc)
			dma_free_coherent(priv->device, (u32)DMA_TX_SIZE * sizeof(struct dma_desc),
					(void *)tx_q->dma_tx, tx_q->dma_tx_phy);
		else
			dma_free_coherent(priv->device, (u32)DMA_TX_SIZE * sizeof(struct dma_ext_desc),
					(void *)tx_q->dma_etx, tx_q->dma_tx_phy);
		kfree((void *)tx_q->tx_skbuff_dma);
		tx_q->tx_skbuff_dma = NULL;
		kfree((void *)tx_q->tx_skbuff);
		tx_q->tx_skbuff = NULL;
	}
}


/**
 * alloc_dma_tx_desc_resources
 * @priv: hobot private struct pointer
 * Description: alloc dma rx descriptor resources
 * Return: 0 on success, otherwise error.
 */
static s32 alloc_dma_tx_desc_resources(struct hobot_priv *priv)
{
	u32 tx_count = priv->plat->tx_queues_to_use;
	s32 ret = -ENOMEM;
	u32 queue;

	for (queue = 0; queue < tx_count; queue++) {
		struct dma_tx_queue *tx_q = &priv->tx_queue[queue];

		tx_q->queue_index = queue;
		tx_q->priv_data = priv;

		tx_q->tx_skbuff_dma = (struct tx_info *)kmalloc_array(DMA_TX_SIZE, sizeof(*tx_q->tx_skbuff_dma), GFP_KERNEL);
		if (NULL == tx_q->tx_skbuff_dma)
			goto err_dma;

		tx_q->tx_skbuff = (struct sk_buff **)kmalloc_array(DMA_TX_SIZE, sizeof(struct sk_buff *), GFP_KERNEL);
		if (NULL == tx_q->tx_skbuff)
			goto err_dma;

		if (0 != priv->extend_desc) {
			tx_q->dma_etx = (struct dma_ext_desc *)dma_alloc_coherent(priv->device, (u32)DMA_TX_SIZE * sizeof(struct dma_ext_desc),
								&tx_q->dma_tx_phy, GFP_KERNEL);
			if (NULL == tx_q->dma_etx)
				goto err_dma;
		} else {
			tx_q->dma_tx = (struct dma_desc *)dma_alloc_coherent(priv->device, (u32)DMA_TX_SIZE * sizeof(struct dma_desc),
								&tx_q->dma_tx_phy, GFP_KERNEL);
			if (NULL == tx_q->dma_tx)
				goto err_dma;
		}

	}


	return 0;

err_dma:
	free_dma_tx_desc_resources(priv);

	return ret;
}

/**
 * alloc_dma_desc_resources
 * @priv: hobot private struct pointer
 * Description: alloc dma tx and rx descriptor resources
 * Return: 0 on success, otherwise error.
 */
static s32 alloc_dma_desc_resources(struct hobot_priv *priv)
{
	s32 ret;
	ret = alloc_dma_rx_desc_resources(priv);
	if (ret < 0)
		return ret;

	ret = alloc_dma_tx_desc_resources(priv);

	return ret;
}

/**
 * dma_init_rx_buffers
 * @priv: hobot private struct pointer
 * @p: dma_desc struct pointer
 * @i: which buffers
 * @queue: queue id
 * Description: init dma rx buffers
 * Return: 0 on success, otherwise error.
 */
static s32 dma_init_rx_buffers(struct hobot_priv *priv, struct dma_desc *p, s32 i, u32 queue)
{
	struct dma_rx_queue *rx_q = &priv->rx_queue[queue];
	struct sk_buff *skb;

	skb = __netdev_alloc_skb_ip_align(priv->ndev, priv->dma_buf_sz, GFP_KERNEL);
	if (NULL == skb) {
		netdev_err(priv->ndev, "%s, Rx init failed: skb is NULL\n", __func__);
		return -ENOMEM;
	}

	rx_q->rx_skbuff[i] = skb;
	rx_q->rx_skbuff_dma[i] = dma_map_single(priv->device, skb->data,
					priv->dma_buf_sz, DMA_FROM_DEVICE);
	if (0 != dma_mapping_error(priv->device, rx_q->rx_skbuff_dma[i])) {
		netdev_err(priv->ndev, "%s, DMA mapping error\n", __func__);
		dev_kfree_skb_any(skb);
		rx_q->rx_skbuff[i] = NULL;
		return -EINVAL;
	}

	p->des0 = cpu_to_le32(lower_32_bits(rx_q->rx_skbuff_dma[i]));
	p->des1 = cpu_to_le32(upper_32_bits(rx_q->rx_skbuff_dma[i]));

	return 0;
}

/**
 * clear_dma_rx_descriptors
 * @priv: hobot private struct pointer
 * @queue: queue id
 * Description: clear dma rx descriptors
 * Return:NA
 */
/* code review E1: do not need to return value */
static void clear_dma_rx_descriptors(struct hobot_priv *priv, u32 queue)
{
	struct dma_rx_queue *rx_q = &priv->rx_queue[queue];
	s32 i;
	struct dma_desc *p;

	for (i = 0; i < DMA_RX_SIZE; i++) {
		if (0 == priv->extend_desc) {
			p = &rx_q->dma_rx[i];
			p->des3 = cpu_to_le32(RDES3_OWN | RDES3_BUFFER1_VALID_ADDR);
			p->des3 |= cpu_to_le32(RDES3_INT_ON_COMPLETION_EN);
		}
	}

}

/**
 * clear_dma_tx_descriptors
 * @priv: hobot private struct pointer
 * @queue: queue id
 * Description: clear dma tx descriptors
 * Return:NA
 */
/* code review E1: do not need to return value */
static void clear_dma_tx_descriptors(struct hobot_priv *priv, u32 queue)
{
	struct dma_tx_queue *tx_q = &priv->tx_queue[queue];
	s32 i;
	struct dma_desc *p;

	for (i = 0; i < DMA_TX_SIZE; i++) {

		if (0 != priv->extend_desc) {
			p = &tx_q->dma_etx[i].basic;
			p->des0 = 0;
			p->des1 = 0;
			p->des2 = 0;
			p->des3 = 0;

		} else {
			p = &tx_q->dma_tx[i];
			p->des0 = 0;
			p->des1 = 0;
			p->des2 = 0;
			p->des3 = 0;
		}
	}
}

/**
 * init_dma_rx_desc_rings
 * @ndev: net_device struct pointer
 * Description: init dma rx descriptors rings resources
 * Return:0 on success, otherwise error.
 */
static s32 init_dma_rx_desc_rings(struct net_device *ndev)
{
	struct hobot_priv *priv = (struct hobot_priv *)netdev_priv(ndev);
	u32 rx_count = priv->plat->rx_queues_to_use;
	s32 queue;
	s32 ret;
	s32 i;
	priv->dma_buf_sz = DEFAULT_BUFSIZE;

	for (queue = 0; (u32)queue < rx_count; queue++) {
		struct dma_rx_queue *rx_q = &priv->rx_queue[queue];

		for (i = 0; i < DMA_RX_SIZE; i++) {
			struct dma_desc *p;

			if (0 != priv->extend_desc)
				p = &((rx_q->dma_erx + i)->basic);
			else
				p = rx_q->dma_rx + i;
			ret = dma_init_rx_buffers(priv, p, i, (u32)queue);
			if (ret < 0)
				goto err_init_rx_buffers;
		}

		rx_q->cur_rx = 0;
		rx_q->dirty_rx = 0U;
		clear_dma_rx_descriptors(priv, (u32)queue);
	}

	return 0;

err_init_rx_buffers:
	while (queue >= 0) {
		while (--i >= 0)
			dma_free_rx_buffer(priv, (u32)queue, i);

		i = DMA_RX_SIZE;
		queue--;
	}

	return ret;
}

/**
 * init_dma_tx_desc_rings
 * @ndev: net_device struct pointer
 * Description: init dma tx descriptors rings resources
 * Return:0 on success, otherwise error.
 */
static s32 init_dma_tx_desc_rings(struct net_device *ndev)
{
	struct hobot_priv *priv = (struct hobot_priv *)netdev_priv(ndev);
	u32 tx_count = priv->plat->tx_queues_to_use;
	u32 queue;
	s32 i;

	for (queue = 0; queue < tx_count; queue++) {
		struct dma_tx_queue *tx_q = &priv->tx_queue[queue];

		for (i = 0; i < DMA_TX_SIZE; i++) {
			struct dma_desc *p;

			if (0 != priv->extend_desc)
				p = &((tx_q->dma_etx + i)->basic);
			else
				p = tx_q->dma_tx + i;

			p->des0 = 0;
			p->des1 = 0;
			p->des2 = 0;
			p->des3 = 0;

			tx_q->tx_skbuff_dma[i].buf = 0;
			tx_q->tx_skbuff_dma[i].map_as_page = (bool)false;
			tx_q->tx_skbuff_dma[i].len = 0;
			tx_q->tx_skbuff_dma[i].last_segment = (bool)false;
			tx_q->tx_skbuff[i] = NULL;
		}

		tx_q->dirty_tx = 0;
		tx_q->cur_tx = 0;
		tx_q->mss = 0;
		netdev_tx_reset_queue(netdev_get_tx_queue(priv->ndev,queue));
	}

	return 0;
}


/**
 * clear_dma_descriptors
 * @priv: hobot_priv struct pointer
 * Description: clear dma descriptors resources
 * include tx, rx
 * Return:NA
 */
/* code review E1: do not need to return value */
static void clear_dma_descriptors(struct hobot_priv *priv)
{
	u32 rx_queue_count = priv->plat->rx_queues_to_use;
	u32 tx_queue_count = priv->plat->tx_queues_to_use;
	u32 queue;

	for (queue = 0; queue < rx_queue_count; queue++)
		clear_dma_rx_descriptors(priv, queue);

	for (queue = 0; queue < tx_queue_count; queue++)
		clear_dma_tx_descriptors(priv, queue);
}

/**
 * init_dma_desc_rings
 * @ndev: net_device struct pointer
 * Description: init dma descriptors rings resources
 * include tx, rx
 * Return:0 on success, otherwise error.
 */
static s32 init_dma_desc_rings(struct net_device *ndev)
{
	struct hobot_priv *priv = (struct hobot_priv *)netdev_priv(ndev);
	s32 ret;

	ret = init_dma_rx_desc_rings(ndev);
	if (ret < 0) {
		netdev_err(ndev, "init dma rx rings failed\n");
		return ret;
	}
	ret = init_dma_tx_desc_rings(ndev);
	if (ret < 0) {
		netdev_err(ndev, "init dma tx rings failed\n");
		return ret;
	}
	clear_dma_descriptors(priv);

	return ret;
}

/**
 * reset_queues_param - reset queue parameters
 * code review E1: do not need to return value
 */
static void reset_queues_param(struct hobot_priv *priv)
{
	u32 rx_cnt = priv->plat->rx_queues_to_use;
	u32 tx_cnt = priv->plat->tx_queues_to_use;
	u32 queue;

	for (queue = 0; queue < rx_cnt; queue++) {
		struct dma_rx_queue *rx_q = &priv->rx_queue[queue];

		rx_q->cur_rx = 0;
		rx_q->dirty_rx = 0;
	}

	for (queue = 0; queue < tx_cnt; queue++) {
		struct dma_tx_queue *tx_q = &priv->tx_queue[queue];

		tx_q->cur_tx = 0;
		tx_q->dirty_tx = 0;
		tx_q->mss = 0;
	}
}


static s32 dma_reset(void __iomem *ioaddr)
{
	u32 value;
	unsigned long timeout_jiffies;

	value = ioreadl(ioaddr, DMA_BUS_MODE);
	value |= (u32)DMA_BUS_MODE_SFT_RESET;
	iowritel(value, ioaddr, DMA_BUS_MODE);

	timeout_jiffies = jiffies + msecs_to_jiffies(TIMEOUT_5MS);
	while ((ioreadl(ioaddr, DMA_BUS_MODE) & DMA_BUS_MODE_SFT_RESET) != 0x0U) {
		if (time_is_before_jiffies(timeout_jiffies)) {/*PRQA S 2996*/
			/*avoid scheduling timeouts*/
			value = ioreadl(ioaddr, DMA_BUS_MODE);
			if ((value & DMA_BUS_MODE_SFT_RESET) == 0x0U)
				break;
			return -EBUSY;
		}
		mdelay(1);/*PRQA S 2880, 2877*/
	}

	return 0;
}

/**
 * dma_init
 * @priv: hobot_priv struct pointer
 * @dma_cfg: dma_ctrl_cfg struct pointer
 * Description: init dma resources
 * Return:NA
 */
/* code review E1: do not need to return value */
static void dma_init(const struct hobot_priv *priv, struct dma_ctrl_cfg *dma_cfg)
{
	void __iomem *ioaddr  = priv->ioaddr;
	u32 value = ioreadl(ioaddr, DMA_SYS_BUS_MODE);

	if (dma_cfg->fixed_burst)
		value |= (u32)DMA_SYS_BUS_FB;
	if (dma_cfg->mixed_burst)
		value |= (u32)DMA_SYS_BUS_MB;
	if (dma_cfg->aal)
		value |= (u32)DMA_SYS_BUS_AAL;
	value |= (u32)DMA_SYS_BUS_EAME;

	reg_writel(priv, value, DMA_SYS_BUS_MODE);/*PRQA S 0497, 1294, 0685*/
}


/**
 * init_rx_chan
 * @ioaddr: hobot_priv struct pointer
 * @dma_cfg: dma_ctrl_cfg struct pointer
 * @dma_rx_phy: dma rx bus address
 * @chan: channel id
 * Description: init rx channel
 * Return:NA
 */
/* code review E1: do not need to return value */
static void init_rx_chan(void __iomem *ioaddr, const struct dma_ctrl_cfg *dma_cfg, dma_addr_t dma_rx_phy, u32 chan)
{
	u32 value;
	u32 rxpl = (u32)dma_cfg->rxpbl ?: (u32)dma_cfg->pbl;

	value = ioreadl(ioaddr, DMA_CHAN_RX_CONTROL(chan));
	value |= (u32)(rxpl << DMA_BUS_MODE_RPBL_SHIFT);
	iowritel(value, ioaddr, DMA_CHAN_RX_CONTROL(chan));

	iowritel(upper_32_bits(dma_rx_phy), ioaddr, DMA_CHAN_RX_BASE_HADDR(chan));
	iowritel(lower_32_bits(dma_rx_phy), ioaddr, DMA_CHAN_RX_BASE_ADDR(chan));
}

/* code review E1: do not need to return value */
static void set_rx_tail_ptr(void __iomem *ioaddr, u32 tail_ptr, u32 chan)
{
	iowritel(tail_ptr, ioaddr, DMA_CHAN_RX_END_ADDR(chan));
}
/* code review E1: do not need to return value */
static void set_tx_tail_ptr(void __iomem *ioaddr, u32 tail_ptr, u32 chan)
{
	iowritel(tail_ptr, ioaddr, DMA_CHAN_TX_END_ADDR(chan));
}

/* code review E1: do not need to return value */
static void init_chan(void __iomem *ioaddr, struct dma_ctrl_cfg *dma_cfg, u32 chan)
{
	u32 value;

	value = ioreadl(ioaddr, DMA_CHAN_CONTROL(chan));
	if (dma_cfg->pblx8)
		value |= (u32)DMA_BUS_MODE_PBL;

	iowritel(value, ioaddr, DMA_CHAN_CONTROL(chan));

	iowritel(DMA_CHAN_INTR_DEFAULT_MASK, ioaddr, DMA_CHAN_INTR_ENA(chan));

}

/**
 * init_tx_chan
 * @ioaddr: hobot_priv struct pointer
 * @dma_cfg: dma_ctrl_cfg struct pointer
 * @dma_rx_phy: dma rx bus address
 * @chan: channel id
 * Description: init tx channel
 * Return:NA
 */
/* code review E1: do not need to return value */
static void init_tx_chan(void __iomem *ioaddr, const struct dma_ctrl_cfg *dma_cfg, dma_addr_t dma_tx_phy, u32 chan)
{
	u32 value;

	u32 txpbl = dma_cfg->txpbl ?: dma_cfg->pbl;/*PRQA S 1824*/

	value = ioreadl(ioaddr, DMA_CHAN_TX_CONTROL(chan));
	value |= (u32)(txpbl << DMA_CONTROL_PBL_SHIFT) | (u32)DMA_CONTROL_OSP;
	iowritel(value, ioaddr, DMA_CHAN_TX_CONTROL(chan));
	iowritel(upper_32_bits(dma_tx_phy), ioaddr, DMA_CHAN_TX_BASE_HADDR(chan));
	iowritel(lower_32_bits(dma_tx_phy), ioaddr, DMA_CHAN_TX_BASE_ADDR(chan));
}


/**
 * dma_axi_blen_check
 * @axi: axi_cfgstruct pointer
 * @value: value pointer
 * Description: according axi->axi_blen to set value.
 * called by set_dma_axi
 * Return:NA
 */
static void dma_axi_blen_check(const struct axi_cfg *axi, u32 *value)
{
	s32 i;
	for (i = 0; i < AXI_BLEN; i++) {
		switch (axi->axi_blen[i]) {
		case SZ_256:
			*value |= (u32)DMA_AXI_BLEN256;break;
		case SZ_128:
			*value |= (u32)DMA_AXI_BLEN128;break;
		case SZ_64:
			*value |= (u32)DMA_AXI_BLEN64;break;
		case SZ_32:
			*value |= (u32)DMA_AXI_BLEN32;break;
		case SZ_16:
			*value |= (u32)DMA_AXI_BLEN16;break;
		case SZ_8:
			*value |= (u32)DMA_AXI_BLEN8;break;
		case SZ_4:
			*value |= (u32)DMA_AXI_BLEN4;break;
		default:
			pr_debug("%s: axi_blen %u do not support\n", __func__, axi->axi_blen[i]);/*PRQA S 1294, 0685*/
			break;
		}
	}

}


/**
 * set_dma_axi
 * @priv: hobot_priv struct pointer
 * @axi: axi_cfg struct pointer
 * Description: set dma axi bus
 * Return:NA
 */
/* code review E1: do not need to return value */
static void set_dma_axi(const struct hobot_priv *priv, const struct axi_cfg *axi)
{
	void __iomem *ioaddr = priv->ioaddr;
	u32 value = ioreadl(ioaddr, DMA_SYS_BUS_MODE);

	/*later add this code**/

	if (axi->axi_lpi_en)
		value |= (u32)DMA_AXI_EN_LPI;

	if (axi->axi_xit_frm)
		value |= (u32)DMA_AXI_LPI_XIT_FRM;

	value &= (u32)~DMA_AXI_WR_OSR_LMT;/*PRQA S 1882, 0636, 4501, 0478*/
	value |= (axi->axi_wr_osr_lmt << BIT_INDEX_24)  & DMA_AXI_WR_OSR_LMT;/*PRQA S 1882, 0636, 4501, 0478, 4461, 1891*/

	value &= (u32)~DMA_AXI_RD_OSR_LMT;/*PRQA S 1882, 0636, 4501, 0478*/
	value |= (axi->axi_rd_osr_lmt << BIT_INDEX_16) &  DMA_AXI_RD_OSR_LMT;/*PRQA S 1882, 0636, 4501, 0478, 4461, 1891*/

	dma_axi_blen_check(axi, &value);

	reg_writel(priv, value, DMA_SYS_BUS_MODE); /*PRQA S 0497, 1294, 0685*/
}


/**
 * init_dma_engine
 * @priv: hobot_priv struct pointer
 * Description: sinit dma engine, include dma channel,axi et al.
 * main function, called by hw_setup
 * Return:0 on success, otherwise error.
 */
static s32 init_dma_engine(struct hobot_priv *priv)
{
	s32 ret;
	u32 rx_channel_count = priv->plat->rx_queues_to_use;
	u32 tx_channel_count = priv->plat->tx_queues_to_use;
	struct dma_rx_queue *rx_q;
	struct dma_tx_queue *tx_q;
	u32 chan;

	ret = dma_reset(priv->ioaddr);
	if (ret < 0) {
		netdev_err(priv->ndev, "%s: Failed to reset dma\n", __func__);
		return ret;
	}

	dma_init(priv, priv->plat->dma_cfg);

	for (chan = 0; chan < rx_channel_count; chan++) {
		rx_q = &priv->rx_queue[chan];
		init_rx_chan(priv->ioaddr, priv->plat->dma_cfg, rx_q->dma_rx_phy, chan);

		rx_q->rx_tail_addr = rx_q->dma_rx_phy + (DMA_RX_SIZE * sizeof(struct dma_desc));/*PRQA S 4461*/
		set_rx_tail_ptr(priv->ioaddr, rx_q->rx_tail_addr, chan);
	}


	for (chan = 0; chan < tx_channel_count; chan++) {
		tx_q = &priv->tx_queue[chan];

		init_chan(priv->ioaddr, priv->plat->dma_cfg, chan);
		init_tx_chan(priv->ioaddr, priv->plat->dma_cfg, tx_q->dma_tx_phy, chan);
		tx_q->tx_tail_addr = tx_q->dma_tx_phy + (DMA_TX_SIZE *sizeof(struct dma_desc));/*PRQA S 4461*/
		set_tx_tail_ptr(priv->ioaddr, tx_q->tx_tail_addr, chan);
	}

	if (NULL != priv->plat->axi)
		set_dma_axi(priv, priv->plat->axi);

	return 0;
}


/**
 * set_umac_addr
 * @ioaddr: io address pointer
 * @addr: mac address pointer
 * @reg_n: mac address register
 * Description: set mac address to eth driver
 * Return:NA
 */
/* code review E1: do not need to return value */
static void set_umac_addr(void __iomem *ioaddr, const u8 *addr, u32 reg_n)
{
	u32 high, low;
	u32 data;

	high = GMAC_ADDR_HIGH(reg_n);
	low = GMAC_ADDR_LOW(reg_n);

	data = ((u32)addr[BYTES_5] << (u32)SZ_8) | (u32)addr[BYTES_4];
	iowritel((u32)(data | GMAC_HI_REG_AE), ioaddr, high);

	data = ((u32)addr[BYTES_3] << SZ_24) | ((u32)addr[BYTES_2] << SZ_16) |
			((u32)addr[BYTES_1] << SZ_8) | (u32)addr[BYTES_0];
	iowritel(data, ioaddr, low);
}

/* code review E1: do not need to return value */
static void core_init(const struct hobot_priv *priv, s32 mtu)
{
	void __iomem *ioaddr = priv->ioaddr;

	u32 value = ioreadl(ioaddr, GMAC_CONFIG);

	value |= (u32)GMAC_CORE_INIT;
	if ((u32)mtu > BYTES_1500)
		value |= (u32)GMAC_CONFIG_2K | (u32)GMAC_CONFIG_JE;
	reg_writel(priv, value, GMAC_CONFIG); /*PRQA S 0497, 1294, 0685*/

	value = (u32)GMAC_INT_DEFAULT_MASK;
	value |= (u32)(GMAC_INT_PMT_EN | GMAC_PCS_IRQ_DEFAULT);

	iowritel(value, ioaddr, GMAC_INT_EN);
}

/**
 * set_tx_queue_weight
 * @priv: hobot_priv struct pointer
 * Description: set tx queue weight
 * Return:NA
 */
/* code review E1: do not need to return value */
static void set_tx_queue_weight(const struct hobot_priv *priv)
{
	u32 tx_count = priv->plat->tx_queues_to_use;
	u32 weight;
	u32 queue;
	u32 value;

	for (queue = 0; queue < tx_count; queue++) {
		weight = priv->plat->tx_queues_cfg[queue].weight;

		value = ioreadl(priv->ioaddr, MTL_TXQX_WEIGHT_BASE_ADDR(queue));
		value &= ~MTL_TXQ_WEIGHT_ISCQW_MASK;
		value |= weight & MTL_TXQ_WEIGHT_ISCQW_MASK;
		iowritel(value, priv->ioaddr, MTL_TXQX_WEIGHT_BASE_ADDR(queue));
	}
}

/**
 * pro_rx_algo
 * @priv: hobot_priv struct pointer
 * @rx_alg: receive algorithm
 * Description: set tx queue weight
 * Return:NA
 */
/* code review E1: do not need to return value */
static void pro_rx_algo(const struct hobot_priv *priv, u32 rx_alg)
{
	void __iomem *ioaddr = priv->ioaddr;

	u32 value = ioreadl(ioaddr, MTL_OPERATION_MODE);
	value &= (u32)~MTL_OPERATION_RAA;

	switch (rx_alg) {
	case MTL_RX_ALGORITHM_SP:
		value |= (u32)MTL_OPERATION_RAA_SP;
		break;
	case MTL_RX_ALGORITHM_WSP:
		value |= (u32)MTL_OPERATION_RAA_WSP;
		break;
	default:
		netdev_warn(priv->ndev, "%s, rx_alg %u is invalid value\n", __func__, rx_alg);
		break;
	}

	reg_writel(priv, value, MTL_OPERATION_MODE); /*PRQA S 0497, 1294, 0685*/
}


/**
 * pro_tx_algo
 * @priv: hobot_priv struct pointer
 * @rx_alg: transmit algorithm
 * Description: set tx queue weight
 * Return:NA
 */
/* code review E1: do not need to return value */
static void pro_tx_algo(const struct hobot_priv *priv, u32 tx_alg)
{
	void __iomem *ioaddr = priv->ioaddr;
	u32 value = ioreadl(ioaddr, MTL_OPERATION_MODE);

	value &= ~MTL_OPERATION_SCHALG_MASK;/*PRQA S 0636, 0478, 4501, 4461, 1882*/
	switch (tx_alg) {
	case MTL_TX_ALGORITHM_WRR:
		value |= MTL_OPERATION_SCHALG_WRR;
		break;
	case MTL_TX_ALGORITHM_WFQ:
		value |= MTL_OPERATION_SCHALG_WFQ;
		break;
	case MTL_TX_ALGORITHM_DWRR:
		value |= MTL_OPERATION_SCHALG_DWRR;
		break;
	case MTL_TX_ALGORITHM_SP:
		value |= MTL_OPERATION_SCHALG_SP;
		break;
	default:
		netdev_warn(priv->ndev, "%s, tx_alg %u is invalid value\n", __func__, tx_alg);
		break;
	}

	reg_writel(priv, value, MTL_OPERATION_MODE); /*PRQA S 0497, 1294, 0685*/

}

/* code review E1: do not need to return value */
static void rx_queue_dma_chan_map(const struct hobot_priv *priv)
{
	u32 rx_queues_count = priv->plat->rx_queues_to_use;
	u32 queue;
	u32 chan;
	void __iomem *ioaddr = priv->ioaddr;
	u32 value;

	for (queue = 0; queue < rx_queues_count; queue++) {
		chan = priv->plat->rx_queues_cfg[queue].chan;

		if (queue < MTL_MAX_QUEUES)
			value = ioreadl(ioaddr, MTL_RXQ_DMA_MAP0);
		else
			value = ioreadl(ioaddr, MTL_RXQ_DMA_MAP1);

		if ((queue == 0U) || (queue == MTL_MAX_QUEUES)) {
			value &= ~MTL_RXQ_DMA_Q04MDMACH_MASK;
			value |= MTL_RXQ_DMA_Q04MDMACH(chan);
		} else {
			value &= ~MTL_RXQ_DMA_QXMDMACH_MASK(queue);/*PRQA S 0478, 4461, 1882, 0588, 4501, 0636*/
			value |= MTL_RXQ_DMA_QXMDMACH(chan, queue);
		}

		if (queue < MTL_MAX_QUEUES)
			iowritel(value, ioaddr, MTL_RXQ_DMA_MAP0);
		else
			iowritel(value, ioaddr, MTL_RXQ_DMA_MAP1);
	}
}

/* code review E1: do not need to return value */
static void mac_enable_rx_queues(struct hobot_priv *priv)
{
	u32 rx_queues_count = priv->plat->rx_queues_to_use;
	u32 queue;
	u8 mode;
	void __iomem *ioaddr = priv->ioaddr;
	u32 value;

	for (queue = 0; queue < rx_queues_count; queue++) {
		mode = priv->plat->rx_queues_cfg[queue].mode_to_use;
		value = ioreadl(ioaddr, GMAC_RXQ_CTRL0);
		value &= GMAC_RX_QUEUE_CLEAR(queue);/*PRQA S 4501, 1882, 0478, 0636, 4461*/
		if (mode == (u8)MTL_QUEUE_AVB) //1
			value |= GMAC_RX_AV_QUEUE_ENABLE(queue);/*PRQA S 4461*/
		else if (mode == (u8)MTL_QUEUE_DCB)//0
			value |= GMAC_RX_DCB_QUEUE_ENABLE(queue);/*PRQA S 4461*/
		else
			netdev_warn(priv->ndev, "%s, not support mode :%c\n", __func__, mode);
		iowritel(value, ioaddr, GMAC_RXQ_CTRL0);
	}
}

/**
 * mac_config_rx_queues_prio
 * @priv: hobot_priv struct pointer
 * Description: set rx queues priority
 * Return:NA
 */
/* code review E1: do not need to return value */
static void mac_config_rx_queues_prio(const struct hobot_priv *priv)
{
	void __iomem *ioaddr = priv->ioaddr;
	u32 rx_count = priv->plat->rx_queues_to_use;
	u32 queue;
	u32 prio;
	u32 value;
	u32 base_reg;


	for (queue = 0; queue < rx_count; queue++) {
		if (!priv->plat->rx_queues_cfg[queue].use_prio)
			continue;

		prio = priv->plat->rx_queues_cfg[queue].prio;
		base_reg = (queue < MTL_MAX_QUEUES) ? (u32)GMAC_RXQ_CTRL2 : (u32)GMAC_RXQ_CTRL3;
		value = ioreadl(ioaddr, base_reg);
		value &= ~GMAC_RXQCTRL_PSRQX_MASK(queue);/*PRQA S 1882, 4501, 0478, 0588, 0636, 4461*/
		value |= (prio << GMAC_RXQCTRL_PSRQX_SHIFT(queue)) & GMAC_RXQCTRL_PSRQX_MASK(queue);/*PRQA S 0636, 0588, 4461, 0478, 1882, 1891, 4501*/
		iowritel(value, ioaddr, base_reg);
	}
}

/**
 * mac_config_tx_queues_prio
 * @priv: hobot_priv struct pointer
 * Description: set tx queues priority
 * Return:NA
 */
/* code review E1: do not need to return value */
static void mac_config_tx_queues_prio(const struct hobot_priv *priv)
{
	u32 tx_count = priv->plat->tx_queues_to_use;
	u32 queue;
	u32 prio;
	void __iomem *ioaddr = priv->ioaddr;
	u32 base_reg;
	u32 value;

	for (queue = 0; queue < tx_count; queue++) {
		if (!priv->plat->tx_queues_cfg[queue].use_prio)
			continue;

		prio = priv->plat->tx_queues_cfg[queue].prio;

		base_reg = (queue < MTL_MAX_QUEUES) ? (u32)GMAC_TXQ_PRTY_MAP0 : (u32)GMAC_TXQ_PRTY_MAP1;
		value = ioreadl(ioaddr, base_reg);
		value &= ~GMAC_TXQCTRL_PSTQX_MASK(queue);/*PRQA S 4461, 4501, 0636, 0478, 0588, 1882*/
		value |= (prio << GMAC_TXQCTRL_PSTQX_SHIFT(queue)) & GMAC_TXQCTRL_PSTQX_MASK(queue);/*PRQA S 0636, 0588, 4461, 0478, 1882, 1891, 4501*/
		iowritel(value, ioaddr, base_reg);

	}
}

/* code review E1: do not need to return value */
static void mac_config_rx_queues_routing(const struct hobot_priv *priv)
{
	u32 rx_count = priv->plat->rx_queues_to_use;
	u32 queue;
	u8 packet;
	u32 value;
	void __iomem *ioaddr = priv->ioaddr;

	const struct rxq_routing route_possibilities[] = {
		{ GMAC_RXQCTRL_AVCPQ_MASK, GMAC_RXQCTRL_AVCPQ_SHIFT },/*PRQA S 1882, 4501, 0636, 0478*/
		{ GMAC_RXQCTRL_PTPQ_MASK, GMAC_RXQCTRL_PTPQ_SHIFT },/*PRQA S 1882, 4501, 0636, 0478*/
		{ GMAC_RXQCTRL_DCBCPQ_MASK, GMAC_RXQCTRL_DCBCPQ_SHIFT },/*PRQA S 1882, 4501, 0636, 0478*/
		{ GMAC_RXQCTRL_UPQ_MASK, GMAC_RXQCTRL_UPQ_SHIFT },/*PRQA S 1882, 4501, 0636, 0478*/
		{ GMAC_RXQCTRL_MCBCQ_MASK, GMAC_RXQCTRL_MCBCQ_SHIFT },/*PRQA S 1882, 4501, 0636, 0478*/
	};


	for (queue = 0; queue < rx_count; queue++) {
		if (priv->plat->rx_queues_cfg[queue].pkt_route == 0x0)/*PRQA S 1863*/
			continue;

		packet = priv->plat->rx_queues_cfg[queue].pkt_route;
		value = ioreadl(ioaddr, GMAC_RXQ_CTRL1);
		value &= ~route_possibilities[packet - 1].reg_mask;/*PRQA S 1860*/
		value |= (queue << route_possibilities[packet-1].reg_shift) & route_possibilities[packet - 1].reg_mask;/*PRQA S 1860*/

		if (packet == (u8)PACKET_AVCPQ) {
			value |= (u32)GMAC_RXQCTRL_TACPQE;
		} else if (packet == (u8)PACKET_MCBCQ) {
			value |= (u32)GMAC_RXQCTRL_MCBCQEN;
		} else {
			netdev_warn(priv->ndev, "%s, not support packet mode :%c\n", __func__, packet);
		}
		value |= (u32)GMAC_RXQCTRL_FPRQ;
		iowritel(value, ioaddr, GMAC_RXQ_CTRL1);
	}
}

/**
 * mtl_config
 * @priv: hobot_priv struct pointer
 * Description: set mac transmit layer config
 * include queues priority, algorithm et al.
 * Return:0 on success, otherwise error.
 */
static s32 mtl_config(struct hobot_priv *priv)
{
	u32 rx_queues_count = priv->plat->rx_queues_to_use;
	u32 tx_queues_count = priv->plat->tx_queues_to_use;

	if (tx_queues_count > 1U) {
		set_tx_queue_weight(priv);
		pro_tx_algo(priv, priv->plat->tx_sched_algorithm);
	}

	if (rx_queues_count > 1U)
		pro_rx_algo(priv, priv->plat->rx_sched_algorithm);

	rx_queue_dma_chan_map(priv);
	mac_enable_rx_queues(priv);

	if (rx_queues_count > 1U) {
		mac_config_rx_queues_prio(priv);
		mac_config_rx_queues_routing(priv);
	}

	if (tx_queues_count > 1U) {
		mac_config_tx_queues_prio(priv);
	}

	return 0;
}


static bool rx_ipc_enable(const struct hobot_priv *priv)
{
	void __iomem *ioaddr = priv->ioaddr;
	u32 value = ioreadl(ioaddr, GMAC_CONFIG);
	if (0 != priv->rx_csum)
		value |= (u32)GMAC_CONFIG_IPC;
	else
		value &= (u32)~GMAC_CONFIG_IPC;

	value |= (u32)GMAC_CONFIG_CST;
	reg_writel(priv, value, GMAC_CONFIG);/*PRQA S 0497, 1294, 0685*/

	value = ioreadl(ioaddr, GMAC_CONFIG);
	if (0U != (value & (u32)GMAC_CONFIG_IPC)) {
		return (bool)true;
	}
	return (bool)false;

}

/**
 * enable_mac_transmit
 * @priv: hobot_priv struct pointer
 * @enable: enable flag
 * Description: enable mac transmit function
 * called by eth_mac_link_down/up, eth_suspend et al.
 * Return:NA
 */
/* code review E1: do not need to return value */
static void enable_mac_transmit(const struct hobot_priv *priv, bool enable)
{
	u32 value = ioreadl(priv->ioaddr, GMAC_CONFIG);

	if (enable)
		value |= (u32)(GMAC_CONFIG_RE | GMAC_CONFIG_TE);
	else
		value &= (u32)~(GMAC_CONFIG_TE | GMAC_CONFIG_RE);
	reg_writel(priv, value, GMAC_CONFIG);/*PRQA S 0497, 1294, 0685*/
}

/* code review E1: do not need to return value */
static void dma_rx_chan_op_mode(void __iomem *ioaddr, u32 chan, s32 rxfifosz, u8 qmode)
{
	u32 rqs = ((u32)rxfifosz / (u32)SZ_256) - 1U;
	u32 mtl_rx_op, mtl_rx_int;

	mtl_rx_op = ioreadl(ioaddr, MTL_CHAN_RX_OP_MODE(chan));
	mtl_rx_op |= (u32)MTL_OP_MODE_RSF;
	mtl_rx_op &= ~MTL_OP_MODE_RQS_MASK;/*PRQA S 4461, 4501, 0636, 1882, 0478*/
	mtl_rx_op |= rqs << MTL_OP_MODE_RQS_SHIFT;

	if (rxfifosz >= SZ_4K && qmode != MTL_QUEUE_AVB ) {/*PRQA S 1863*/
		u32 rfd, rfa;
		mtl_rx_op |= (u32)MTL_OP_MODE_EHFC;
		switch (rxfifosz) {
			case SZ_4K:
				rfd = SZ_3;//0x03;
				rfa = SZ_1;//0x01;
				break;
			case SZ_8K:
				rfd = SZ_6;//0x06;
				rfa = SZ_10;//0x0a;
				break;
			case SZ_16K:
				rfd = SZ_6;//0x06;
				rfa = SZ_18;//0x12;
				break;
			default:
				rfd = SZ_6;//0x06;
				rfa = SZ_30;//0x1e;
				break;
		}

		mtl_rx_op &= ~MTL_OP_MODE_RFD_MASK;/*PRQA S 4461, 4501, 0636, 1882, 0478*/
		mtl_rx_op |= rfd << MTL_OP_MODE_RFD_SHIFT;
		mtl_rx_op &= ~MTL_OP_MODE_RFA_MASK;/*PRQA S 4461, 4501, 0636, 1882, 0478*/
		mtl_rx_op |= rfa << MTL_OP_MODE_RFA_SHIFT;
	}

	iowritel(mtl_rx_op, ioaddr, MTL_CHAN_RX_OP_MODE(chan));
	mtl_rx_int = ioreadl(ioaddr, MTL_CHAN_INT_CTRL(chan));
	iowritel((u32)(mtl_rx_int | MTL_RX_OVERFLOW_INT_EN), ioaddr, MTL_CHAN_INT_CTRL(chan));

}

/* code review E1: do not need to return value */
static void dma_tx_chan_op_mode(void __iomem *ioaddr, u32 chan, s32 fifosz, u8 qmode)
{
	u32 mtl_tx_op = ioreadl(ioaddr, MTL_CHAN_TX_OP_MODE(chan));
	u32 tqs = ((u32)fifosz / (u32)SZ_256) - 1U;
	mtl_tx_op &= ~MTL_OP_MODE_TXQEN_MASK;/*PRQA S 4461, 4501, 0636, 1882, 0478*/

	mtl_tx_op |= (u32)MTL_OP_MODE_TSF;
	if (qmode != (u8)MTL_QUEUE_AVB)
		mtl_tx_op |= (u32)MTL_OP_MODE_TXQEN;
	else
		mtl_tx_op |= (u32)MTL_OP_MODE_TXQEN_AV;

	mtl_tx_op &= ~MTL_OP_MODE_TQS_MASK;/*PRQA S 4501, 0478, 1882, 0636, 4461*/
	mtl_tx_op |= tqs << MTL_OP_MODE_TQS_SHIFT;

	iowritel(mtl_tx_op, ioaddr, MTL_CHAN_TX_OP_MODE(chan));

	return;
}

/* code review E1: do not need to return value */
static void dma_operation_mode(const struct hobot_priv *priv)
{
	u32 rx_count = priv->plat->rx_queues_to_use;
	u32 tx_count = priv->plat->tx_queues_to_use;
	s32 rxfifosz = priv->plat->rx_fifo_size;
	s32 txfifosz = priv->plat->tx_fifo_size;
	u32 chan;
	u8 qmode;

	if (rxfifosz == 0)
		rxfifosz = (s32)priv->dma_cap.rx_fifo_size;

	if (txfifosz == 0)
		txfifosz = (s32)priv->dma_cap.tx_fifo_size;

	rxfifosz /= (s32)rx_count;
	txfifosz /= (s32)tx_count;

	for (chan = 0; chan < rx_count; chan++) {
		qmode = priv->plat->rx_queues_cfg[chan].mode_to_use;
		dma_rx_chan_op_mode(priv->ioaddr, chan, rxfifosz,qmode);
	}

	for (chan = 0; chan < tx_count; chan++) {
		qmode = priv->plat->tx_queues_cfg[chan].mode_to_use;
		dma_tx_chan_op_mode(priv->ioaddr, chan, txfifosz, qmode);
	}


}

/**
 * mmc_setup
 * @priv: hobot private struct pointer
 * Description: mmc function setup
 * Return: NA
 */
/* code review E1: do not need to return value */
static void mmc_setup(struct hobot_priv *priv)
{
	unsigned int mode = MMC_CNTRL_RESET_ON_READ | MMC_CNTRL_COUNTER_RESET |
                        MMC_CNTRL_PRESET | MMC_CNTRL_FULL_HALF_PRESET;

	hobot_eth_mmc_intr_all_mask(priv->mmcaddr);
	if (priv->dma_cap.rmon) {
        hobot_eth_mmc_ctrl(priv->mmcaddr, mode);
        memset(&priv->mmc, 0, sizeof(struct hobot_counters));
    } else {
        netdev_info(priv->ndev, "No MAC Management Counters available\n");
    }
}


/**
 * start_rx_dma
 * @priv: hobot private struct pointer
 * @chan: channel id
 * Description: start rx dma
 * called by start all dma
 * Return: NA
 */
/* code review E1: do not need to return value */
static void start_rx_dma(const struct hobot_priv *priv, u32 chan)
{
	void __iomem *ioaddr = priv->ioaddr;
	u32 value = ioreadl(ioaddr, DMA_CHAN_RX_CONTROL(chan));

	value |= (u32)DMA_CONTROL_SR;

	iowritel(value, ioaddr, DMA_CHAN_RX_CONTROL(chan));
}


/**
 * start_tx_dma
 * @priv: hobot private struct pointer
 * @chan: channel id
 * Description: start tx dma
 * called by start all dma
 * Return: NA
 */
/* code review E1: do not need to return value */
static void start_tx_dma(const struct hobot_priv *priv, u32 chan)
{
	void __iomem *ioaddr = priv->ioaddr;
	u32 value = ioreadl(ioaddr, DMA_CHAN_TX_CONTROL(chan));

	value |= (u32)DMA_CONTROL_ST;
	iowritel(value, ioaddr, DMA_CHAN_TX_CONTROL(chan));
}


/**
 * start_all_dma
 * @priv: hobot private struct pointer
 * Description: start all dma
 * called by hw_setup
 * Return: NA
 */
/* code review E1: do not need to return value */
static void start_all_dma(struct hobot_priv *priv)
{
	u32 rx_count = priv->plat->rx_queues_to_use;
	u32 tx_count = priv->plat->tx_queues_to_use;
	u32 chan;

	for (chan = 0; chan < rx_count; chan++)
		start_rx_dma(priv, chan);

	for (chan = 0; chan < tx_count; chan++)
		start_tx_dma(priv, chan);
}


/**
 * set_rings_length
 * @priv: hobot private struct pointer
 * Description: set dma rings length, include tx and rx
 * called by hw_setup
 * Return: NA
 */
/* code review E1: do not need to return value */
static void set_rings_length(const struct hobot_priv *priv)
{
	u32 rx_count = priv->plat->rx_queues_to_use;
	u32 tx_count = priv->plat->tx_queues_to_use;
	u32 chan;
	void __iomem *ioaddr = priv->ioaddr;
	u32 len;

	for (chan = 0; chan < tx_count; chan++){
		len = DMA_TX_SIZE - 1;
		iowritel(len, ioaddr, DMA_CHAN_TX_RING_LEN(chan));
	}

	for (chan = 0; chan < rx_count; chan++) {
		len = DMA_RX_SIZE - 1;
		iowritel(len, ioaddr, DMA_CHAN_RX_RING_LEN(chan));
	}

}



/**
 * eth_ptp_set_time
 * @ptp: ptp_clock_info struct pointer
 * @ts: timespec64 struct pointer
 * Description: set ptp time for eth driver
 * Return: 0 on success, otherwise error.
 */
static s32 eth_ptp_set_time(struct ptp_clock_info *ptp, const struct timespec64 *ts)
{
	struct hobot_priv *priv;
	unsigned long flags;
	s32 ret = 0;
	if (NULL == ptp) {
		pr_err("%s, ptp_clock_info is null\n", __func__);
		return -EINVAL;
	}

	priv = container_of(ptp, struct hobot_priv, ptp_clock_ops);/*PRQA S 2810, 0497*/ /*linux macro*/
	if (!netif_running(priv->ndev)) {
		(void)netdev_err(priv->ndev, "%s, device has not been brought up\n", __func__);
		return -1;
	}

	raw_spin_lock_irqsave(&priv->ptp_lock, flags);/*PRQA S 2996*/ /*linux macro*/
	if (ptp_init_systime(priv, (u32)ts->tv_sec, (u32)ts->tv_nsec) < 0) {
		netdev_err(priv->ndev, "%s, set ptp time err\n", __func__);
		ret = -EBUSY;
	}
	raw_spin_unlock_irqrestore(&priv->ptp_lock, flags);/*PRQA S 2996*/ /*linux macro*/

	return ret;
}



/**
 * ptp_get_systime
 * @priv: hobot private struct pointer
 * Description: get ptp system time
 * Return: ns time
 */
static u64 ptp_get_systime(const struct hobot_priv *priv)
{
	u8 idx = 0u;
	u64 ns;
	u64 sec0, sec1;

	/*The purpose of reading the time in this way is to prevent the time reading
	 * error caused by the carry of the nanosecond.
	 * E.g:
	 * First read the nanosecond value: 999999999 (the time at this moment is 10s + 999999999ns);
	 * Then read the second value: 11 (the time at this moment is 11s + 10ns);
	 * The time returned by the function is: 11s + 999999999ns;
	 */
	sec1 = ioreadl(priv->ioaddr, PTP_STSR);
	do {
		sec0 = sec1;
		ns = ioreadl(priv->ioaddr, PTP_STNSR);
		sec1 = ioreadl(priv->ioaddr, PTP_STSR);
		idx++;
	} while((sec0 != sec1) && (idx < 3u));

	if (sec0 != sec1) {
		netdev_err(priv->ndev, "%s, error:sec0 = %llu, sec1 = %llu, ns = %llu\n",
				__func__, sec0, sec1, ns);
	}

	ns += sec1 * PER_SEC_NS;

	return ns;
}

static s32 eth_ptp_get_time(struct ptp_clock_info *ptp, struct timespec64 *ts)
{
	struct hobot_priv *priv;
	unsigned long flags;
	u64 ns;

	if (NULL == ptp) {
		pr_err("%s, ptp_clock_info is null\n", __func__);
		return -EINVAL;
	}

	priv = container_of(ptp, struct hobot_priv, ptp_clock_ops);/*PRQA S 2810, 0497*/ /*linux macro*/
	if (!netif_running(priv->ndev)) {
		(void)netdev_err(priv->ndev, "%s, device has not been brought up\n", __func__);
		return -1;
	}

	raw_spin_lock_irqsave(&priv->ptp_lock, flags);/*PRQA S 2996*/ /*linux macro*/
	ns = ptp_get_systime(priv);
	raw_spin_unlock_irqrestore(&priv->ptp_lock, flags);/*PRQA S 2996*/ /*linux macro*/
	*ts = ns_to_timespec64((s64)ns);
	return 0;
}

static s32 ptp_adjust_systime(const struct hobot_priv *priv, u32 sec, u32 nsec, s32 add_sub)
{
	u32 value;

	if (0 != add_sub) {
		/* If the new sec value needs to be subtracted with
		 * the system time, then MAC_STSUR reg should be
		 * programmed with (2^32 – <new_sec_value>)
		 */
		sec = -sec;/*PRQA S 3101*/

		value = ioreadl(priv->ioaddr, PTP_TCR);
		if (0 != (value & PTP_TCR_TSCTRLSSR))
			nsec = (PTP_DIGITAL_ROLLOVER_MODE - nsec);
		else
			nsec = (PTP_BINARY_ROLLOVER_MODE - nsec);
	}

	iowritel(sec, priv->ioaddr, PTP_STSUR);
	value = (add_sub << PTP_STNSUR_ADDSUB_SHIFT) | nsec;/*PRQA S 1821, 4532, 2861, 4533*/
	iowritel(value, priv->ioaddr, PTP_STNSUR);

	value = ioreadl(priv->ioaddr, PTP_TCR);
	value |= (u32)PTP_TCR_TSUPDT;
	iowritel(value, priv->ioaddr, PTP_TCR);

	udelay(10);/*PRQA S 2880*/
	if ((ioreadl(priv->ioaddr, PTP_TCR) & PTP_TCR_TSUPDT) != 0x0U) {
		netdev_err(priv->ndev, "%s: adjust eth timestamp timeout\n", __func__);
		return -EBUSY;
	}

	return 0;
}


static s32 eth_ptp_adjust_time(struct ptp_clock_info *ptp, s64 delta)
{
	struct hobot_priv *priv;
	unsigned long flags;
	u32 sec, nsec;
	u32 quotient, reminder;
	s32 neg_adj = 0;
	s32 ret;
	s64 new_delta = delta;

	if (NULL == ptp) {
		pr_err("%s, ptp_clock_info is null\n", __func__);
		return -EINVAL;
	}

	priv = container_of(ptp, struct hobot_priv, ptp_clock_ops);/*PRQA S 2810, 0497*/
	if (!netif_running(priv->ndev)) {
		(void)netdev_err(priv->ndev, "%s, device has not been brought up\n", __func__);
		return -1;
	}

	if (delta < 0) {
		neg_adj = 1;
		new_delta = -delta;
	}

	quotient = (u32)div_u64_rem((u64)new_delta, (u32)PER_SEC_NS, (u32 *)&reminder);
	sec = quotient;
	nsec = reminder;

	raw_spin_lock_irqsave(&priv->ptp_lock, flags); /*PRQA S 2996*/
	ret = ptp_adjust_systime(priv, sec, nsec, neg_adj);
	raw_spin_unlock_irqrestore(&priv->ptp_lock, flags);/*PRQA S 2996*/
	if (ret < 0) {
		netdev_err(priv->ndev, "adjust ptp time err\n");
	}
	return ret;
}
static s32 ptp_flex_pps_config(const struct hobot_priv *priv, s32 index, struct pps_cfg *cfg, bool enable,
					u32 sub_second_inc, u32 systime_flags)
{
	u32 tnsec = ioreadl(priv->ioaddr, (u32)MAC_PPSx_TARGET_TIME_NSEC(index));
	u32 val = ioreadl(priv->ioaddr, MAC_PPS_CONTROL);
	u64 period;

	if (!cfg->available) {
		netdev_err(priv->ndev, "pps cfg is invalid\n");
		return -EINVAL;
	}
	if (0U != (tnsec & (u32)TRGTBUSY0)) {
		netdev_err(priv->ndev, "pps target time register busy\n");
		return -EBUSY;
	}
	if ((0U == sub_second_inc) || (0U == systime_flags)) {
		netdev_err(priv->ndev, "%s:%d, input para error\n", __func__, __LINE__);
		return -EINVAL;
	}
	val &= ~PPSx_MASK(index);/*PRQA S 4534, 4501, 0588, 4461, 0636, 1882, 0478*/
	if (!enable) {
		val |= PPSCMDx(index, PPSCMD_CANCEL);/*PRQA S 4532, 1882, 4534, 0588, 0636, 0478, 4534, 4461, 4501, 1821*/
		iowritel(val, priv->ioaddr, MAC_PPS_CONTROL);
		return 0;
	}
	if (1 == index) {
		netdev_err(priv->ndev, "%s:%d, input para error, reserver for rtc\n", __func__, __LINE__);
		return -EINVAL;
	}
	/*index 0 used for pps timesync, rtc(32k) will select eth0 pps0 source*/
	if (0 != index) {
		val |= TRGTMODSELx(index, TRGTMODSEL_ONLY_ST);/*PRQA S 4532, 1882, 4534, 0588, 0636, 0478, 4534, 4461, 4501, 1821*/
		val |= PPSCMDx(index, PPSCMD_START_TRAIN);/*PRQA S 4532, 1882, 4534, 0588, 0636, 0478, 4534, 4461, 4501, 1821*/
	} else {
		val |= TRGTMODSELx(index, TRGTMODSEL_INT_ST);/*PRQA S 4532, 1882, 4534, 0588, 0636, 0478, 4534, 4461, 4501, 1821*/
		val |= PPSCMDx(index, PPSCMD_START_SINGLE);/*PRQA S 4532, 1882, 4534, 0588, 0636, 0478, 4534, 4461, 4501, 1821*/
	}
	val |= (u32)PPSEN0;
	iowritel((u32)(cfg->start.tv_sec), priv->ioaddr, (u32)MAC_PPSx_TARGET_TIME_SEC(index));

	if (0U == (systime_flags & PTP_TCR_TSCTRLSSR))
		cfg->start.tv_nsec = (cfg->start.tv_nsec * SZ_1000U) / SZ_465U;/*PRQA S 1850*/

	iowritel((u32)(cfg->start.tv_nsec), priv->ioaddr, (u32)MAC_PPSx_TARGET_TIME_NSEC(index));

	period = (u64)cfg->period.tv_sec * PER_SEC_NS;
	period += (u64)cfg->period.tv_nsec;
	do_div(period, sub_second_inc); /*PRQA S 4461*/

	if (period <= 1U) {
		netdev_err(priv->ndev, "%s:%d, input para error\n", __func__, __LINE__);
		return -EINVAL;
	}
	/*find mode do not need to sub 1, this is different from datasheet*/
	if (0U != (ioreadl(priv->ioaddr, PTP_TCR) & PTP_TCR_TSCFUPDT)) {
		iowritel((u32)period, priv->ioaddr, (u32)MAC_PPSx_INTERVAL(index));
	} else {
		iowritel((u32)period - 1U, priv->ioaddr, (u32)MAC_PPSx_INTERVAL(index));
	}
	if (0 != index) {
		period >>= 1;
	} else {
		period /= 1000;// 1/1000 duty cycle
	}
	if (period <= 1U) {
		netdev_err(priv->ndev, "%s:%d, input para error\n", __func__, __LINE__);
		return -EINVAL;
	}
	iowritel((u32)(period - 1U), priv->ioaddr, (u32)MAC_PPSx_WIDTH(index));
	iowritel(val, priv->ioaddr, MAC_PPS_CONTROL);
	return 0;
}

static s32 ptp_aux_ts_trig_config(struct hobot_priv *priv,
				  struct ptp_clock_request *rq, s32 on)
{
	u32 val = 0, bit;

	switch (rq->extts.index) {
	case SZ_0://rtc
		bit = (u32)ATSEN0;
		break;
	case SZ_1://gps
		bit = (u32)ATSEN1;
		break;
	case SZ_2://lidar
		bit = (u32)ATSEN2;
		break;
	case SZ_3://ap
		bit = (u32)ATSEN3;
		break;
	default:
		netdev_err(priv->ndev, "ptp_extts_request index is out of range\n");
		return -EINVAL;
	}

	val |= (u32)ATSFC;
	if (0 == on) {
		val &= ~bit;
	} else {
		val |= bit;
	}
	iowritel(val, priv->ioaddr, MAC_AUX_CONTROL);
	val = ioreadl(priv->ioaddr, GMAC_INT_EN);
	if (0 == on) {
		val &= (u32)~GMAC_INT_TS_EN;
	} else {
		val |= (u32)GMAC_INT_TS_EN;
	}
	iowritel(val, priv->ioaddr, GMAC_INT_EN);

	return 0;
}



/**
* eth_ptp_enable
* @ptp: ptp_clock_info struct pointer
* @rq: ptp_clock_request struct pointer
* @on: enable flag
* Description: enable ptp function
* Return: 0 on success, otherwise error.
*/
static s32 eth_ptp_enable(struct ptp_clock_info *ptp, struct ptp_clock_request *rq, s32 on)
{
	struct hobot_priv *priv;
	struct pps_cfg *cfg;
	s32 ret = -EOPNOTSUPP;

	unsigned long flags;

	if (NULL == ptp) {
		(void)pr_err("%s, ptp_clock_info is null\n", __func__);
		return -EINVAL;
	}

	priv = container_of(ptp, struct hobot_priv, ptp_clock_ops);/*PRQA S 2810, 0497*/
	if (!netif_running(priv->ndev)) {
		(void)netdev_err(priv->ndev, "%s, device has not been brought up\n", __func__);
		return -1;
	}

	switch(rq->type) {
	case PTP_CLK_REQ_PEROUT:
		cfg = &priv->pps[rq->perout.index];
		cfg->start.tv_sec = rq->perout.start.sec;
		cfg->start.tv_nsec = rq->perout.start.nsec;
		cfg->period.tv_sec = rq->perout.period.sec;
		cfg->period.tv_nsec = rq->perout.period.nsec;
		raw_spin_lock_irqsave(&priv->ptp_lock, flags); /*PRQA S 2996*/
		ret = ptp_flex_pps_config(priv,rq->perout.index, cfg, on, priv->sub_second_inc, priv->systime_flags);/*PRQA S 4430*/
		raw_spin_unlock_irqrestore(&priv->ptp_lock, flags);/*PRQA S 2996*/
		break;
	case PTP_CLK_REQ_EXTTS:
		raw_spin_lock_irqsave(&priv->ptp_lock, flags); /*PRQA S 2996*/
		ret = ptp_aux_ts_trig_config(priv, rq, on);
		raw_spin_unlock_irqrestore(&priv->ptp_lock, flags);/*PRQA S 2996*/
		break;
	default:
		netdev_warn(priv->ndev, "%s, rq->type %u do not support\n", __func__, rq->type);
		break;
	}

	return ret;
}


static s32 eth_ptp_adjust_freq(struct ptp_clock_info *ptp, s32 ppb)
{
	struct hobot_priv *priv;
	unsigned long flags;
	u32 diff, addend;
	s32 neg_adj = 0;
	u64 adj;
	s32 ret;

	if (NULL == ptp) {
		(void)pr_err("%s, ptp_clock_info is null\n", __func__);
		return -EINVAL;
	}

	priv = container_of(ptp, struct hobot_priv, ptp_clock_ops);/*PRQA S 2810, 0497*/
	if (!netif_running(priv->ndev)) {
		(void)netdev_err(priv->ndev, "%s, device has not been brought up\n", __func__);
		return -1;
	}

	if (ppb < 0) {
		neg_adj = 1;
		ppb =  -ppb;
	}

	addend = priv->default_addend;
	adj = addend;
	adj *= (u32)ppb;

	diff = (u32)div_u64(adj, (u32)PER_SEC_NS);
	addend = neg_adj ? (addend - diff) : (addend + diff);

	raw_spin_lock_irqsave(&priv->ptp_lock, flags); /*PRQA S 2996*/
	ret = config_addend(priv, addend);
	raw_spin_unlock_irqrestore(&priv->ptp_lock, flags);/*PRQA S 2996*/
	return ret;
}

static struct ptp_clock_info ptp_clock_ops = {
	.owner = THIS_MODULE,
	.name = "hobot_ptp_clock",
	.max_adj = CLOCK_62_5M,
	.n_alarm = 0,
	.n_ext_ts = PPS_MAX,
	.n_per_out = PPS_MAX,
	.n_pins = 0,
	.pps = 1,
	.adjfreq = eth_ptp_adjust_freq,
	.adjtime = eth_ptp_adjust_time,
	.gettime64 = eth_ptp_get_time,
	.settime64 = eth_ptp_set_time,
	.enable = eth_ptp_enable,

};

/**
* hobot_eth_ptp_get_time
* @ts: time of ethernet
* Description: support for other drivers to get ethernet time,
* it is recommended to use ethernet1 for time synchronization
* called by other drivers
* Return: 0 on success, otherwise error.
*/
s32 hobot_eth_ptp_get_time(struct timespec64 *ts)
{
	struct hobot_priv *priv;
	struct net_device *ndev = pndev;

	if (ndev == NULL) {
		(void)pr_err("%s, ndev is NULL\n", __func__);
		return -1;
	}

	priv = (struct hobot_priv *)netdev_priv(ndev);

	if ((ts == NULL)) {
		(void)netdev_err(priv->ndev, "%s, invalid parameter\n", __func__);
		return -1;
	}

	if ((ndev->flags & IFF_UP) == 0) {
		(void)netdev_err(priv->ndev, "%s, ndev is down\n", __func__);
		return -1;
	}

	return priv->ptp_clock_ops.gettime64(&(priv->ptp_clock_ops), ts);
}
EXPORT_SYMBOL(hobot_eth_ptp_get_time);

/**
* ptp_register
* @priv: hobot_priv struct pointer
* Description: register ptp function
* called by eth_init_ptp
* Return: 0 on success, otherwise error.
*/
static s32 ptp_register(struct hobot_priv *priv)
{

	s32 i;

	for (i = 0; i < (s32)priv->dma_cap.pps_out_num; i++) {
		if (i >= PPS_MAX)
			break;

		priv->pps[i].available = (bool)true;
	}

	ptp_clock_ops.n_per_out = (s32)priv->dma_cap.pps_out_num;

	raw_spin_lock_init(&priv->ptp_lock);/*PRQA S 3334*/

	priv->ptp_clock_ops = ptp_clock_ops;
	priv->ptp_clock = ptp_clock_register(&priv->ptp_clock_ops, priv->device);

	if (IS_ERR_OR_NULL((void *)priv->ptp_clock)) {
		netdev_err(priv->ndev, "ptp_clock_register failed by hobot\n");
		priv->ptp_clock = NULL;
		return -1;
	}

	netdev_dbg(priv->ndev, "registered PTP clock by hobot successfully\n"); /*PRQA S 1294, 0685*/
	return 0;
}



/**
* eth_init_ptp
* @priv: hobot_priv struct pointer
* Description: main function for init ptp
* called by eth_drv_function_register
* Return: 0 on success, otherwise error.
*/
static s32 eth_init_ptp(struct hobot_priv *priv)
{
	s32 ret;
	if (!((0U != priv->dma_cap.time_stamp) || (0U != priv->dma_cap.atime_stamp)))
		return -EOPNOTSUPP;

	priv->adv_ts = 1U;

	if (0U != priv->dma_cap.time_stamp)
		netdev_dbg(priv->ndev, "IEEE 1588-2002 Timestamp supported by Hobot Ethernet Network Card\n"); /*PRQA S 1294, 0685*/

	netdev_dbg(priv->ndev, "IEEE 1588-2008 Advanced Timestamp supported by Hobot Ethernet Network Card\n"); /*PRQA S 1294, 0685*/

	priv->hwts_tx_en = 0;
	priv->hwts_rx_en = 0;

	ret = ptp_register(priv);
	if (ret < 0)  {
		netdev_err(priv->ndev, "PTP init failed\n");
	}
	return ret;
}


/**
* hw_setup
* @ndev: net_device struct pointer
* Description: main function for hw setup,
* include dma, mtl, mmc, ring length et al.
* Return: 0 on success, otherwise error.
*/
static s32 hw_setup(const struct net_device *ndev)
{
	struct hobot_priv *priv = (struct hobot_priv *)netdev_priv(ndev);
	u32 tx_cnt = priv->plat->tx_queues_to_use;
	s32 ret;
	u32 chan;


	ret = init_dma_engine(priv);
	if (ret < 0) {
		netdev_err(ndev, "%s, DMA engine initilization failed\n", __func__);
		return ret;
	}

	set_umac_addr(priv->ioaddr, ndev->dev_addr, 0);

	core_init(priv, (s32)ndev->mtu);

	ret = mtl_config(priv);
	if (ret < 0) {
		netdev_err(ndev, "%s, mtl initilization failed\n", __func__);
		return ret;
	}


	if (!rx_ipc_enable(priv)) {
		netdev_dbg(ndev, "RX IPC checksum offload disabled\n"); /*PRQA S 1294, 0685*/
		priv->plat->rx_coe = STMMAC_RX_COE_NONE;
		priv->rx_csum = 0;
	}

	dma_operation_mode(priv);

	mmc_setup(priv);

	set_rings_length(priv);
	start_all_dma(priv);

	if (priv->tso) {
		u32 value;
		for (chan = 0; chan < tx_cnt; chan++) {
			value = ioreadl(priv->ioaddr, DMA_CHAN_TX_CONTROL(chan));
			iowritel(value | (u32)DMA_CONTROL_TSE, priv->ioaddr, DMA_CHAN_TX_CONTROL(chan));
		}
	}

	return 0;
}

/* code review E1: do not need to return value */
static void free_dma_desc_resources(struct hobot_priv *priv)
{
	free_dma_rx_desc_resources(priv);
	free_dma_tx_desc_resources(priv);
}

/* code review E1: do not need to return value */
static void enable_all_napi(struct hobot_priv *priv)
{
	u32 rx_cnt = priv->plat->rx_queues_to_use;
	u32 queue;

	for (queue = 0; queue < rx_cnt; queue++) {
		struct dma_rx_queue *rx_q = &priv->rx_queue[queue];
		napi_enable(&rx_q->napi);
	}
}

/* code review E1: do not need to return value */
static void start_all_queues(const struct hobot_priv *priv)
{
	u32 tx_cnt = priv->plat->tx_queues_to_use;
	u32 queue;

	for (queue = 0; queue < tx_cnt; queue++)
		netif_tx_start_queue(netdev_get_tx_queue(priv->ndev, queue));
}

static s32 dma_interrupt(void __iomem *ioaddr, struct extra_stats *x, u32 chan)
{
	s32 ret = 0;
	u32 value;
	u32 intr_status = ioreadl(ioaddr, DMA_CHAN_STATUS(chan));
	pr_debug("dma_interrupt chan%u intr_status=0x%x\n", chan, intr_status);/*PRQA S 1294, 0685*/

	if (0U != (intr_status & DMA_CHAN_STATUS_AIS)) {
		if (0U != (intr_status & DMA_CHAN_STATUS_RBU)) {
			x->rx_buf_unav_irq++;
			ret |= handle_rx;
		}
		if (0U != (intr_status & DMA_CHAN_STATUS_RPS))
			x->rx_process_stopped_irq++;

		if (0U != (intr_status & DMA_CHAN_STATUS_RWT))
			x->rx_watchdog_irq++;

		if (0U != (intr_status & DMA_CHAN_STATUS_ETI))
			x->tx_early_irq++;

		if (0U != (intr_status & DMA_CHAN_STATUS_TPS)) {
			x->tx_process_stopped_irq++;
			ret = (s32)tx_hard_error;
		}

		if (0U != (intr_status & DMA_CHAN_STATUS_FBE)) {
			x->fatal_bus_error_irq++;
			ret = (s32)tx_hard_error;
		}
	}


	if (0U != (intr_status & DMA_CHAN_STATUS_NIS)) {
		x->normal_irq_n++;
		if (0U != (intr_status & DMA_CHAN_STATUS_RI)) {
			value = ioreadl(ioaddr, DMA_CHAN_INTR_ENA(chan));
			if (0U != (value & DMA_CHAN_INTR_ENA_RIE)) {
				x->rx_normal_irq_n++;
				ret |= handle_rx;/*PRQA S 4532*/
			}
		}

		if (0U != (intr_status & DMA_CHAN_STATUS_TI)) {
			x->tx_normal_irq_n++;
			ret |= handle_tx;/*PRQA S 4532*/
		}

		if (0U != (intr_status & DMA_CHAN_STATUS_ERI))
			x->rx_early_irq++;
	}

	iowritel((intr_status & DMA_CHAN_STATUS_MASK), ioaddr, DMA_CHAN_STATUS(chan));
	return ret;
}

/* code review E1: do not need to return value */
static void disable_dma_irq(const struct hobot_priv *priv, u32  chan)
{
	iowritel(0, priv->ioaddr, DMA_CHAN_INTR_ENA(chan));
}

/* code review E1: do not need to return value */
static void init_tx_desc(struct dma_desc *p)
{
	p->des0 = 0;
	p->des1 = 0;
	p->des2 = 0;
	p->des3 = 0;
}

/* code review E1: do not need to return value */
static void stop_tx_dma(const struct hobot_priv *priv, u32 chan)
{
	void __iomem *ioaddr = priv->ioaddr;

	u32 value = ioreadl(ioaddr, DMA_CHAN_TX_CONTROL(chan));

	value &= (u32)~DMA_CONTROL_ST;
	iowritel(value, ioaddr, DMA_CHAN_TX_CONTROL(chan));
}

/* code review E1: do not need to return value */
static void dma_tx_err(struct hobot_priv *priv, u32 chan)
{
	struct dma_tx_queue *tx_q = &priv->tx_queue[chan];
	s32 i;

	netif_tx_stop_queue(netdev_get_tx_queue(priv->ndev, chan));
	stop_tx_dma(priv, chan);
	dma_free_tx_skbufs(priv, chan);

	for (i = 0; i < DMA_TX_SIZE; i++)
		if (0 != priv->extend_desc) {
			init_tx_desc(&tx_q->dma_etx[i].basic);
		} else {
			init_tx_desc(&tx_q->dma_tx[i]);
		}

	tx_q->dirty_tx = 0;
	tx_q->cur_tx = 0;
	tx_q->mss = 0;

	netdev_tx_reset_queue(netdev_get_tx_queue(priv->ndev, chan));
	start_tx_dma(priv, chan);
	priv->ndev->stats.tx_errors++;
	netif_tx_wake_queue(netdev_get_tx_queue(priv->ndev, chan));

}

/**
 * pcs_isr
 * @ioaddr: ioaddr pointer
 * @reg: register address
 * @intr_status: initrupt pointer
 * @x: extra_stats struct pointer
 * Description: pcs isr function
 * Return: NA
 */
/* code review E1: do not need to return value */
static inline void pcs_isr(const void __iomem *ioaddr, u32 reg, u32 intr_status, struct extra_stats *x)
{
	u32 val = ioreadl(ioaddr, GMAC_AN_STATUS(reg));

	if (0U != (intr_status & PCS_ANE_IRQ)) {
		x->irq_pcs_ane_n++;
		if (0U != (val & GMAC_AN_STATUS_ANC))
			pr_debug("pcs: ANE process completed\n");/*PRQA S 1294, 0685*/
	}

	if (0U != (intr_status & PCS_LINK_IRQ)) {
		x->irq_pcs_link_n++;
		if (0U != (val & GMAC_AN_STATUS_LS))
			pr_debug("pcs: link up\n");/*PRQA S 1294, 0685*/
		else
			pr_debug("pcs: Link down\n");/*PRQA S 1294, 0685*/
	}
}


/**
 * get_phy_ctrl_status
 * @priv: hobot private struct pointer
 * @x: extra_stats struct pointer
 * Description: get phy control status,
 * include speed, duplex, link.
 * Return: NA
 */
/* code review E1: do not need to return value */
static void get_phy_ctrl_status(const struct hobot_priv *priv, struct extra_stats *x)
{
	u32 status;
	void __iomem *ioaddr = priv->ioaddr;

	status = ioreadl(ioaddr, GMAC_PHYIF_CONTROL_STATUS);
	x->irq_rgmii_n++;

	if (0U != (status & GMAC_PHYIF_CTRLSTATUS_LNKSTS)) {
		s32 speed_value;

		x->pcs_link = 1;

		speed_value = ((status & GMAC_PHYIF_CTRLSTATUS_SPEED) >> GMAC_PHYIF_CTRLSTATUS_SPEED_SHIFT);/*PRQA S 0478, 4501, 0636, 1882*/

		if (speed_value == GMAC_PHYIF_CTRLSTATUS_SPEED_125)
			x->pcs_speed = SPEED_1000;
		else if (speed_value == GMAC_PHYIF_CTRLSTATUS_SPEED_25)
			x->pcs_speed = SPEED_100;
		else
			x->pcs_speed = SPEED_10;

		x->pcs_duplex = ((status & GMAC_PHYIF_CTRLSTATUS_LNKMOD ) >> GMAC_PHYIF_CTRLSTATUS_LNKMOD_MASK);
	} else {
		x->pcs_link = 0;
	}
}

/**
 * host_irq_status
 * @priv: hobot_priv struct poniter
 * @x: extra_stats struct pointer
 * Description: get mac interrupt status
 * Return: 0 on success, otherwise error.
 */
static s32 host_irq_status(struct hobot_priv *priv, struct extra_stats *x)
{
	void __iomem *ioaddr = priv->ioaddr;
	u32 value;
	struct ptp_clock_event event;

	u32 intr_status = ioreadl(ioaddr, GMAC_INT_STATUS);
	u32 intr_enable = ioreadl(ioaddr, GMAC_INT_EN);
	s32 ret = 0;
	u32 nanosecond, second;
	u32 ts_irq_state;

	pr_debug("%s, GMAC_INT_STATUS: 0x%x\n", __func__, intr_status);/*PRQA S 1294, 0685*/
	if (0U != (intr_status & (u32)time_stamp_irq)) {
		ts_irq_state = ioreadl(ioaddr, MAC_TS_STATUS);
		if ( (0U != (ts_irq_state & (ATSSTN0|ATSSTN1|ATSSTN2|ATSSTN3))) && (0U != (ts_irq_state & AUXTSTRIG)) ) {
			second = ioreadl(ioaddr, MAC_AUX_TS_SECOND);
			nanosecond = ioreadl(ioaddr, MAC_AUX_TS_NANOSECOND);
			if ((ts_irq_state & ATSNS) > 0U) {/*PRQA S 0478, 0636,4501,1882*/
				value = ioreadl(priv->ioaddr, MAC_AUX_CONTROL);
				value |= (u32)ATSFC;
				iowritel(value, priv->ioaddr, MAC_AUX_CONTROL);
			}
			event.type = (s32)PTP_CLOCK_EXTTS;
			event.index = 1;
			event.timestamp = (u64)(second * PER_SEC_NS) + nanosecond;
			ptp_clock_event(priv->ptp_clock, &event);
		}

		if (0U != (ts_irq_state & TSTARGT0)) {
			event.type = (s32)PTP_CLOCK_PPS;
			if (0 != priv->ptp_clock_ops.pps)
				ptp_clock_event(priv->ptp_clock, &event);
		}
	}
	intr_status &= intr_enable;
	if (0U != (intr_status & (u32)mmc_tx_irq)) {
		x->mmc_tx_irq_n++;
	}
	if (0U != (intr_status & (u32)mmc_rx_csum_offload_irq)) {
		x->mmc_rx_csum_offload_irq_n++;
	}
	if (0U != (intr_status & (u32)pmt_irq)) {
		ioreadl(ioaddr, GMAC_PMT);
		x->irq_receive_pmt_irq_n++;
	}

	if (0U != (intr_status & (u32)mac_fpeis)) {
		value = ioreadl(ioaddr, GMAC_FPE_CTRL_STS);
		iowritel(value, ioaddr, GMAC_FPE_CTRL_STS);
	}
	pcs_isr(ioaddr, GMAC_PCS_BASE, intr_status, x);

	if (0U != (intr_status & PCS_RGSMIIIS_IRQ)) {
		get_phy_ctrl_status(priv, x);
	}

	return ret;
}

/**
 * mtl_est_status
 * @priv: hobot_priv struct poniter
 * Description: get mtl status
 * Return: NA
 */
static void mtl_est_status(const struct hobot_priv *priv)
{
	void __iomem *ioaddr = priv->ioaddr;
	u32 status;
	u32 queue, frame;
	status = ioreadl(ioaddr, MTL_EST_STATUS);
	pr_debug("%s, MTL EST intterupt here, status:0x%x\n", __func__, status);/*PRQA S 1294, 0685*/
	if (0U != (status & MTL_STATUS_CGSN)) {/*PRQA S 1882, 0478, 0636, 4501*/
		pr_debug("est current gcl slot num:%lu\n", ((status & MTL_STATUS_CGSN) >> 16) & 0xf);/*PRQA S 1882, 0478, 0636, 4501, 1294, 0685*/
	}

	if (0U != (status & (u32)MTL_STATUS_BTRL)) {/*PRQA S 1882, 0478, 0636, 4501*/
		pr_debug("btr error loop count:%lu\n", ((status & MTL_STATUS_BTRL) >> 8) & 0xf);/*PRQA S 1294, 0685, 1882, 0478, 0636, 4501*/
	}

	if (0U != (status & MTL_STATUS_SWOL))
		pr_debug("gate control list %lu own by sofware\n", (status & MTL_STATUS_SWOL >> 7)& 0x1);/*PRQA S 1294, 0685*/
	else
		pr_debug("gcl 0 own by software\n");/*PRQA S 1294, 0685*/

	if (0U != (status & MTL_STATUS_CGCE)) {
		pr_debug("constant gate control error\n");/*PRQA S 1294, 0685*/
	}
	if (0U != (status & MTL_STATUS_HLBS))
		pr_debug("head of line blocking due scheduling\n");/*PRQA S 1294, 0685*/

	if (0U != (status & MTL_STATUS_HLBF)) {
		queue = ioreadl(ioaddr, MTL_EST_Frm_Size_Error);
		frame = ioreadl(ioaddr, MTL_EST_Frm_Size_Capture);
		pr_debug("head of line bocking due to frame size, and queue:%d\n", queue);/*PRQA S 1294, 0685*/
		pr_debug("HOF block frame size:%d, and the queue:%d\n", frame & 0x3fff, (frame >> 16));/*PRQA S 1294, 0685*/
		iowritel(queue, ioaddr, MTL_EST_Frm_Size_Error);
	}
	if (0U != (status & MTL_STATUS_BTRE))
		pr_debug("BTR error\n");/*PRQA S 1294, 0685*/

	if (0U != (status & MTL_STATUS_SWLC))
		pr_debug("switch to S/W owned list complete\n");/*PRQA S 1294, 0685*/

	iowritel(status, ioaddr, MTL_EST_STATUS);
}




/**
 * host_mtl_irq_status
 * @priv: hobot_priv struct poniter
 * @chan: channel id
 * Description: get mtl interrupt status
 * Return: 0 on success, otherwise error.
 */
static s32 host_mtl_irq_status(const struct hobot_priv *priv, u32 chan)
{
	void __iomem *ioaddr = priv->ioaddr;
	u32 mtl_irq_status;
	s32 ret = 0;
	u32 status;

	mtl_irq_status = ioreadl(ioaddr, MTL_INT_STATUS);
	if (0U != (mtl_irq_status & MTL_INT_QX(chan))) {
		status = ioreadl(ioaddr, MTL_CHAN_INT_CTRL(chan));
		if (0U != (status & MTL_RX_OVERFLOW_INT)) {
			iowritel(status | (u32)MTL_RX_OVERFLOW_INT, ioaddr, MTL_CHAN_INT_CTRL(chan));
			ret = (s32)CORE_IRQ_MTL_RX_OVERFLOW;
		}
		if (0U != (status & MTL_ABPSIS_INT)) {
			if (ioreadl(ioaddr, MTL_ETSX_STATUS_BASE_ADDR(chan)))
				 iowritel(status, ioaddr, MTL_CHAN_INT_CTRL(chan));
		}
	}


	if (0U != (mtl_irq_status & MTL_ESTIS)) {
		mtl_est_status(priv);
	}
	iowritel(mtl_irq_status, ioaddr, MTL_INT_STATUS);
	return ret;
}



/**
 *  eth_interrupt - main ISR
 *  @irq: interrupt number.
 *  @dev_id: to pass the net device pointer.
 *  Description: this is the main driver interrupt service routine.
 *  It can call:
 *  o DMA service routine (to manage incoming frame reception and transmission
 *    status)
 *  o Core interrupts to manage: remote wake-up, management counter, LPI
 *    interrupts.
 */
static irqreturn_t eth_interrupt(s32 irq, void *dev_id)
{

	struct net_device *ndev = (struct net_device *)dev_id;
	struct hobot_priv *priv = (struct hobot_priv *)netdev_priv(ndev);
	u32 rx_cnt = priv->plat->rx_queues_to_use;
	u32 tx_cnt = priv->plat->tx_queues_to_use;
	u32 queues_count;
	u32 queue;
	u32 mtl_status = 0U;
	u32 status;
	u32 chan;

	if (NULL == dev_id) {
		pr_err("%s, dev_id ptr is null\n", __func__);
		return IRQ_HANDLED;
	}
	/* Check if eth is up */
	if (test_bit(STMMAC_DOWN, &priv->state))
		return IRQ_HANDLED;

	queues_count = (rx_cnt > tx_cnt) ? rx_cnt : tx_cnt;
	(void)host_irq_status(priv, &priv->xstats);

	for (queue = 0; queue < queues_count; queue++) {
		struct dma_rx_queue *rx_q = &priv->rx_queue[queue];
		mtl_status = (u32)host_mtl_irq_status(priv, queue);
		if (0U != (mtl_status & CORE_IRQ_MTL_RX_OVERFLOW)) {
			set_rx_tail_ptr(priv->ioaddr, rx_q->rx_tail_addr, queue);
		}
	}


	for (chan = 0; chan < tx_cnt; chan++) {
		struct dma_rx_queue *rx_q = &priv->rx_queue[chan];
		status = (u32)dma_interrupt(priv->ioaddr, &priv->xstats, chan);
		if (likely((status & handle_rx)) || (status & handle_tx)  || (mtl_status & CORE_IRQ_MTL_RX_OVERFLOW)) {
			if (napi_schedule_prep(&rx_q->napi)) {
				disable_dma_irq(priv, chan);
				__napi_schedule(&rx_q->napi);
			}
		}

		if (0U != (status & (u32)tx_hard_error)) {
			dma_tx_err(priv, chan);
		}
	}

	return IRQ_HANDLED;
}






/**
 * tsn_set_est
 * @priv: hobot_priv struct poniter
 * @data: data pointer from user
 * Description: tsn set est function
 * Return: 0 on success, otherwise error.
 */
static s32 tsn_set_est(struct hobot_priv *priv, void __user *data)
{
	struct est_cfg *cfg = &priv->plat->est_cfg;
	struct est_cfg *est;
	s32 ret;
	bool est_en;

	est = kzalloc(sizeof(*est), GFP_KERNEL);
	if (NULL == est) {
		netdev_err(priv->ndev, "no memory for used\n");
		return -ENOMEM;
	}

	if (copy_from_user(est, data, sizeof(*est))) {
		netdev_err(priv->ndev, "%s, copy from user failed\n", __func__);
		ret = -EFAULT;
		goto out_free;
	}

	if (est->gcl_size > (u32)STMMAC_EST_GCL_MAX_ENTRIES) {
		netdev_err(priv->ndev, "%s, gcl size out of range\n", __func__);
		ret = -EINVAL;
		goto out_free;
	}

	if (0U != est->enabled) {
		cfg->btr_offset[0] = est->btr_offset[0];
		cfg->btr_offset[1] = est->btr_offset[1];
		cfg->ctr[0] = est->ctr[0];
		cfg->ctr[1] = est->ctr[1];
		cfg->ter = est->ter;
		cfg->gcl_size = est->gcl_size;
		(void)memcpy((void *)cfg->gcl, (void *)est->gcl, cfg->gcl_size * sizeof(*cfg->gcl));
		est_en = (bool)true;
	} else {
		cfg->btr_offset[0] = 0;
		cfg->btr_offset[1] = 0;
		cfg->ctr[0] = 0;
		cfg->ctr[1] = 0;
		cfg->ter = 0;
		cfg->gcl_size = 0;
		(void)memset((void *)cfg->gcl, 0, sizeof(cfg->gcl));
		est_en = (bool)false;
	}

	priv->plat->est_en = est_en;
	ret = est_configuration(priv);
	if (0U == est->enabled) {
		ret = 0;
	} else {
		pr_debug("%s, dma_cap.fpesel:%d, plat->fp_en:%d\n", __func__, priv->dma_cap.fpesel, priv->plat->fp_en);/*PRQA S 1294, 0685*/
		if (priv->dma_cap.fpesel && priv->plat->fp_en)
			tsn_fp_configure(priv);
	}

out_free:
	kfree((void *)est);
	return ret;

}

/**
 * tsn_set_est
 * @priv: hobot_priv struct poniter
 * @data: data pointer to user
 * Description: tsn get est config to user
 * Return: 0 on success, otherwise error.
 */
static s32 tsn_get_est(const struct hobot_priv *priv, void __user *data)
{
	s32 ret = 0;
	struct est_cfg * est = (struct est_cfg *)kzalloc(sizeof(*est), GFP_KERNEL);
	if (NULL == est) {
		netdev_err(priv->ndev, "no memory for used\n");
		return -ENOMEM;
	}
	est->enabled = (u32)priv->est_enabled;
	est->estwid = priv->dma_cap.estwid;
	est->estdep = priv->dma_cap.estdep;
	est->btr_offset[0] = priv->plat->est_cfg.btr_offset[0];
	est->btr_offset[1] = priv->plat->est_cfg.btr_offset[1];
	est->ctr[0] = priv->plat->est_cfg.ctr[0];
	est->ctr[1] = priv->plat->est_cfg.ctr[1];
	est->ter = priv->plat->est_cfg.ter;
	est->gcl_size = priv->plat->est_cfg.gcl_size;
	(void)memcpy((void *)est->gcl, (void *)priv->plat->est_cfg.gcl, est->gcl_size * sizeof(*est->gcl));

	if (0U != copy_to_user(data, (void *)est, sizeof(*est))) {
		netdev_err(priv->ndev, "%s, copy to user failed\n", __func__);
		ret = -EFAULT;
		goto out_free;
	}

out_free:
	kfree((void *)est);
	return ret;
}

/**
 * tsn_get_fp
 * @priv: hobot_priv struct poniter
 * @data: data pointer to user
 * Description: tsn get frame preemption config
 * Return: 0 on success, otherwise error.
 */
static s32 tsn_get_fp(const struct hobot_priv *priv, void __user *data)
{
	struct fpe_cfg st_fpe;
	s32 ret = 0;


	st_fpe.cmd = STMMAC_GET_FP;
	st_fpe.enabled = (u32)priv->plat->fp_en;

	if (0U != copy_to_user(data, (void *)&st_fpe, sizeof(st_fpe))) {
		netdev_err(priv->ndev, "%s, copy to user failed\n", __func__);
		ret = -EFAULT;
	}

	return ret;
}

/**
 * tsn_set_fp
 * @priv: hobot_priv struct poniter
 * @data: data pointer from user
 * Description: tsn set frame preemption config
 * Return: 0 on success, otherwise error.
 */
static s32 tsn_set_fp(const struct hobot_priv *priv, const void __user *data)
{
	struct fpe_cfg st_fpe;
	s32 ret = 0;
	u32 control;

	if (0U != copy_from_user((void *)&st_fpe, data, sizeof(st_fpe))) {
		netdev_err(priv->ndev, "%s, copy from user failed\n", __func__);
		return -EFAULT;
	}
	priv->plat->fp_en = (bool)st_fpe.enabled;

	if(0U != st_fpe.enabled) {
		tsn_fp_configure(priv);
	} else {
		control = ioreadl(priv->ioaddr, GMAC_FPE_CTRL_STS);
		control &= (u32)~GMAC_FPE_EFPE;
		iowritel(control, priv->ioaddr, GMAC_FPE_CTRL_STS);
	}
	return ret;
}




/**
 * flt_set_uc
 * @priv: hobot_priv struct poniter
 * @data: data pointer from user
 * Description: add net_device uc(unitcast) mac address for mac filter
 * Return: 0 on success, otherwise error.
 */
static s32 flt_set_uc(const struct hobot_priv *priv, const void __user *data)
{
	struct unicast_cfg ucast_cfg;
	s32 ret = 0;
	s32 i;
	u32 val;

	if (0U != copy_from_user((void *)&ucast_cfg, data, sizeof(ucast_cfg))) {
		netdev_err(priv->ndev, "%s, copy from user failed\n", __func__);
		return -EFAULT;
	}

	if (0U != ucast_cfg.enabled) {
		if ((ucast_cfg.count > (u32)STMMAC_MAC_ADDR_MAX) || (ucast_cfg.count == 0U)) {
			return -EINVAL;
		}
		for (i = 0; i < (s32)ucast_cfg.count; i++) {
			if (!is_unicast_ether_addr(ucast_cfg.addr[i]) )
				return -EINVAL;
		}
	}

	priv->plat->unicast_en = (bool)ucast_cfg.enabled;
	dev_uc_flush(priv->ndev);
	if (0U != ucast_cfg.enabled) {
		for (i = 0; i < (s32)ucast_cfg.count; i++) {
			ret = dev_uc_add_simplify(priv->ndev, ucast_cfg.addr[i]);
			if ( ret < 0 ) {
				goto out_err;
			}
		}
	} else {
		goto out_err;
	}
	return ret;
out_err:
	dev_uc_flush(priv->ndev);
	for (i = 1; i <= STMMAC_MAC_ADDR_MAX; i++) {
		iowritel(0xFFFF, priv->ioaddr, GMAC_ADDR_HIGH(i));/*PRQA S 1820*/
		iowritel(0xFFFFFFFF, priv->ioaddr, GMAC_ADDR_LOW(i));/*PRQA S 1820*/
	}
	val = ioreadl(priv->ioaddr, GMAC_PACKET_FILTER);
	iowritel(val & ~GMAC_PACKET_FILTER_SAF, priv->ioaddr, GMAC_PACKET_FILTER);/*PRQA S 4461*/
	priv->plat->unicast_en = (bool)false;
	return ret;
}


/**
 * flt_get_uc
 * @priv: hobot_priv struct poniter
 * @data: data pointer to user
 * Description: aget net_device uc(unitcast) mac address
 * Return: 0 on success, otherwise error.
 */
static s32 flt_get_uc(const struct hobot_priv *priv, void __user *data)
{
	struct unicast_cfg ucast_cfg = {
		.cmd     = STMMAC_GET_UC,
		.enabled = priv->plat->unicast_en,
	};
	s32 ret = 0;
	s32 i = 0;
	struct netdev_hw_addr *ha;

	netdev_for_each_uc_addr(ha, priv->ndev) {/*PRQA S 2810, 0497*/
		(void)memcpy((void *)ucast_cfg.addr[i], (void *)ha->addr, ETH_ALEN);
		i++;
		if (i >= STMMAC_MAC_ADDR_MAX)
			break;
	}
	ucast_cfg.count = (u32)i;

	if (0U != copy_to_user(data, (void *)&ucast_cfg, sizeof(ucast_cfg))) {
		netdev_err(priv->ndev, "%s, copy to user failed\n", __func__);
		ret = -EFAULT;
	}

	return ret;
}

static s32 do_extension_ioctl(struct hobot_priv *priv, void __user *data)
{
	u32 tx_cnt = priv->plat->tx_queues_to_use;
	struct cbs_cfg cbs;
	u8 qmode;
	u32 cmd;
	s32 ret = 0;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	if (0U != copy_from_user((void *)&cmd, data, sizeof(cmd))) {
		netdev_err(priv->ndev, "%s, copy from user failed\n", __func__);
		return -EFAULT;
	}

	switch (cmd) {
	case STMMAC_SET_CBS:
		if (0U != copy_from_user((void *)&cbs, data, sizeof(cbs))) {
			netdev_err(priv->ndev, "copy from user failed\n");
			return -EFAULT;
		}
		if (cbs.queue_idx >= tx_cnt) {
			netdev_err(priv->ndev, "queue index para error\n");
			return -EINVAL;
		}

		qmode = priv->plat->tx_queues_cfg[cbs.queue_idx].mode_to_use;
		if (qmode != (u8)MTL_QUEUE_AVB) {
			netdev_err(priv->ndev, "queue mode is not avb, so can't config cbs\n");
			return -EINVAL;
		}
		priv->plat->tx_queues_cfg[cbs.queue_idx].send_slope = cbs.send_slope;
		priv->plat->tx_queues_cfg[cbs.queue_idx].idle_slope = cbs.idle_slope;
		priv->plat->tx_queues_cfg[cbs.queue_idx].high_credit = cbs.high_credit;
		priv->plat->tx_queues_cfg[cbs.queue_idx].low_credit = cbs.low_credit;
		priv->plat->tx_queues_cfg[cbs.queue_idx].percentage = cbs.percentage;

		tsn_config_cbs(priv, cbs.send_slope, cbs.idle_slope, cbs.high_credit, cbs.low_credit, cbs.queue_idx);
		break;
	case STMMAC_GET_CBS:
		if (0U != copy_from_user((void *)&cbs, data, sizeof(cbs))) {
			netdev_err(priv->ndev, "copy from user failed\n");
			return -EFAULT;
		}
		if (cbs.queue_idx >= tx_cnt) {
			netdev_err(priv->ndev, "cbs queue index error\n");
			return -EINVAL;
		}

		cbs.send_slope = priv->plat->tx_queues_cfg[cbs.queue_idx].send_slope;
		cbs.idle_slope = priv->plat->tx_queues_cfg[cbs.queue_idx].idle_slope;
		cbs.high_credit = priv->plat->tx_queues_cfg[cbs.queue_idx].high_credit;
		cbs.low_credit = priv->plat->tx_queues_cfg[cbs.queue_idx].low_credit;
		cbs.percentage = priv->plat->tx_queues_cfg[cbs.queue_idx].percentage;

		if (0U != copy_to_user(data, (void *)&cbs, sizeof(cbs))) {
			netdev_err(priv->ndev, "%s, copy to user failed\n", __func__);
			return -EFAULT;
		}
		break;

	case STMMAC_GET_EST:
		ret = tsn_get_est(priv, data);
		break;
	case STMMAC_SET_EST:
		ret = tsn_set_est(priv, data);
		break;
	case STMMAC_SET_SAFETY:
	case STMMAC_GET_SAFETY:
		if (priv->hobot_eth_stl_ops &&
		    priv->hobot_eth_stl_ops->stl_extension_ioctl)
			return priv->hobot_eth_stl_ops->stl_extension_ioctl(
				priv->ndev, data);
		break;
	case STMMAC_GET_FP:
		ret = tsn_get_fp(priv, data);
		break;
	case STMMAC_SET_FP:
		ret = tsn_set_fp(priv, data);
		break;
	case STMMAC_SET_UC:
		ret = flt_set_uc(priv, data);
		break;
	case STMMAC_GET_UC:
		ret = flt_get_uc(priv, data);
		break;
	default:
		netdev_err(priv->ndev, "%s, cmd %u do not support\n", __func__, cmd);
		ret = -EINVAL;
		break;
	}

	return ret;
}


/**
 * stop_all_queues
 * @priv: hobot_priv struct poniter
 * Description: stop all tx queues
 * Return: NA
 */
/* code review E1: do not need to return value */
static void stop_all_queues(const struct hobot_priv *priv)
{
	u32 tx_cnt = priv->plat->tx_queues_to_use;
	u32 queue;

	for (queue = 0; queue < tx_cnt; queue++)
		netif_tx_stop_queue(netdev_get_tx_queue(priv->ndev, queue));
}


/**
 * disable_all_napi
 * @priv: hobot_priv struct poniter
 * Description: disable all napi
 * Return: NA
 */
/* code review E1: do not need to return value */
static void disable_all_napi(struct hobot_priv *priv)
{
	u32 rx_cnt = priv->plat->rx_queues_to_use;
	u32 queue;

	for (queue = 0; queue < rx_cnt; queue++) {
		struct dma_rx_queue *rx_q = &priv->rx_queue[queue];

		napi_disable(&rx_q->napi);
	}
}


/**
 * stop_rx_dma
 * @priv: hobot_priv struct poniter
 * @chan: channel id
 * Description: stop rx dma function
 * Return: NA
 */
/* code review E1: do not need to return value */
static void stop_rx_dma(const struct hobot_priv *priv, u32 chan)
{
	void __iomem *ioaddr = priv->ioaddr;

	u32 value = ioreadl(ioaddr, DMA_CHAN_RX_CONTROL(chan));

	value &= (u32)~DMA_CONTROL_SR;
	iowritel(value, ioaddr, DMA_CHAN_RX_CONTROL(chan));
}


/**
 * stop_all_dma
 * @priv: hobot_priv struct poniter
 * Description: stop all dma function,
 * include tx,rx dma.
 * Return: NA
 */
/* code review E1: do not need to return value */
static void stop_all_dma(const struct hobot_priv *priv)
{
	u32 rx_cnt = priv->plat->rx_queues_to_use;
	u32 tx_cnt = priv->plat->tx_queues_to_use;
	u32 chan;

	for (chan = 0; chan < rx_cnt; chan++)
		stop_rx_dma(priv, chan);

	for (chan = 0; chan < tx_cnt; chan++)
		stop_tx_dma(priv, chan);
}




/**
 * hw_timestamp_init
 * @priv: hobot_priv struct poniter
 * @init_time: timespec64 struct poniter
 * Description: init hardware timestamp function
 * Return: 0 on success, otherwise error.
 */
static s32 hw_timestamp_init(struct hobot_priv *priv, struct timespec64 *init_time)
{
	u32 value;
	struct timespec64 now;
	s32 ret;
	u32 sec_inc;
	u64 temp;

	if (NULL == init_time) {
		value = (u32)(PTP_TCR_TSENA | PTP_TCR_TSCFUPDT | PTP_TCR_TSCTRLSSR);
	} else {
		value = priv->systime_flags;
	}
	config_hw_tstamping(priv, value);

	sec_inc = config_sub_second_increment(priv, priv->plat->clk_ptp_rate);
	temp = div_u64(PER_SEC_NS, sec_inc);
	priv->sub_second_inc = sec_inc;
	priv->systime_flags = value;
	temp = (u64) (temp << SZ_32);
	priv->default_addend = (u32)div_u64(temp, priv->plat->clk_ptp_rate);
	ret = config_addend(priv,  priv->default_addend);
	if (ret < 0) {
		netdev_err(priv->ndev, "%s, config ptp addend err\n", __func__);
		return ret;
	}
	if (NULL == init_time) {
		ktime_get_real_ts64(&now);
	} else {
		now = *init_time;
	}
	ret = ptp_init_systime(priv, (u32)now.tv_sec, (u32)now.tv_nsec);
	if (ret < 0) {
		netdev_err(priv->ndev, "%s, init ptp time err\n", __func__);
		return ret;
	}

	//Flexible pps enable
	value = ioreadl(priv->ioaddr, MAC_PPS_CONTROL);
	value |= (u32)PPSEN0;
	iowritel(value, priv->ioaddr, MAC_PPS_CONTROL);

	return 0;
}
/**
*  eth_netdev_open - open entry point of the driver
*  @dev : pointer to the device structure.
*  Description:
*  This function is the open entry point of the driver.
*  Return value:
*  0 on success and an appropriate (-)ve integer as defined in errno.h
*  file on failure.
*/
static s32 eth_netdev_open(struct net_device *ndev)
{
	struct hobot_priv *priv;
	s32 ret;

	if (NULL == ndev) {
		(void)pr_err("%s, net dev ptr is null\n", __func__);
		return -EINVAL;
	}
	priv = (struct hobot_priv *)netdev_priv(ndev);
	ret = init_phy(ndev);
	if (ret < 0) {
		netdev_err(ndev, "%s, init phy error \n", __func__);
		return ret;
	}


	ret = alloc_dma_desc_resources(priv);
	if (ret < 0) {
		netdev_err(ndev, "%s, DMA descriptor alloc failed\n", __func__);
		goto dma_desc_error;
	}


	ret = init_dma_desc_rings(ndev);
	if (ret < 0) {
		netdev_err(ndev, "%s: DMA desc rings initalize failed\n", __func__);
		goto init_error;
	}

	ret = hw_setup(ndev);
	if (ret < 0) {
		netdev_err(ndev, "%s, Hw setup failed\n", __func__);
		goto init_error;
	}

	ret = hw_timestamp_init(priv, NULL);
	if (ret < 0) {
		netdev_err(ndev, "%s, hw timestamp init failed\n", __func__);
		goto init_error;
	}

	if (priv->hobot_eth_stl_ops && priv->hobot_eth_stl_ops->stl_open) {
		ret = priv->hobot_eth_stl_ops->stl_open(priv->ndev);
		if(ret < 0) {
			netdev_err(ndev, "%s, stl open init failed\n", __func__);
			goto init_error;
		}
	}

	hobot_init_timer(priv);

	if (NULL != priv->phylink) {
		phylink_start(priv->phylink);
		phylink_speed_up(priv->phylink);
	}

	ret = devm_request_irq(priv->device, (u32)ndev->irq, &eth_interrupt, 0, ndev->name, (void *)ndev);
	if (ret < 0) {
		netdev_err(ndev, "error allocating the IRQ\n");
		goto irq_error;
	}

	enable_all_napi(priv);
	start_all_queues(priv);


	return 0;

irq_error:
	if (priv->hobot_eth_stl_ops && priv->hobot_eth_stl_ops->stl_stop) {
		priv->hobot_eth_stl_ops->stl_stop(priv->ndev);
	}
	del_timer_sync(&priv->txtimer);
init_error:
	free_dma_desc_resources(priv);
dma_desc_error:
	if (NULL != priv->phylink) {
		phylink_disconnect_phy(priv->phylink);
	}

	return ret;
}




/**
 *  netdev_stop - close entry point of the driver
 *  @dev : device pointer.
 *  Description:
 *  This is the stop entry point of the driver.
 */
static s32 eth_netdev_stop(struct net_device *ndev)
{
	struct hobot_priv *priv;

	if (NULL == ndev) {
		(void)pr_err("%s, net dev ptr is null\n", __func__);
		return -EINVAL;
	}
	priv = (struct hobot_priv *)netdev_priv(ndev);
	if (NULL != priv->phylink) {
		phylink_stop(priv->phylink);
		phylink_disconnect_phy(priv->phylink);
	}

	stop_all_queues(priv);
	disable_all_napi(priv);

	devm_free_irq(priv->device, ndev->irq, ndev);/*PRQA S 4434*/
	del_timer_sync(&priv->txtimer);
	stop_all_dma(priv);
	free_dma_desc_resources(priv);

	enable_mac_transmit(priv, (bool)false);
	netif_carrier_off(ndev);
	if (priv->hobot_eth_stl_ops && priv->hobot_eth_stl_ops->stl_stop) {
		priv->hobot_eth_stl_ops->stl_stop(priv->ndev);
	}

	return 0;
}

static inline u32 tx_queue_avail(struct hobot_priv *priv, u32 queue)
{
	struct dma_tx_queue *tx_q = &priv->tx_queue[queue];
	u32 avail;

	if (tx_q->dirty_tx > tx_q->cur_tx)
		avail = tx_q->dirty_tx - tx_q->cur_tx - 1U;
	else
		avail = (u32)DMA_TX_SIZE - tx_q->cur_tx + tx_q->dirty_tx - 1U;
	return avail;
}

/* code review E1: do not need to return value */
static void prepare_tx_desc(struct dma_desc *p, s32 is_fs, s32 len, bool csum_flags, s32 tx_own, bool ls, u32 tot_pkt_len)
{
	u32 tdes3 = le32_to_cpu(p->des3);

	p->des2 |= cpu_to_le32(len & TDES2_BUFFER1_SIZE_MASK);/*PRQA S 1882, 0478, 1821, 0636, 4501, 4532*/

	tdes3 |= tot_pkt_len & TDES3_PACKET_SIZE_MASK;/*PRQA S 1882, 0478, 4461, 0636, 4501*/

	if (0 != is_fs)
		tdes3 |= (u32)TDES3_FIRST_DESCRIPTOR;
	else
		tdes3 &= (u32)~TDES3_FIRST_DESCRIPTOR;

	if (csum_flags)
		tdes3 |= ((u32)TX_CIC_FULL << TDES3_CHECKSUM_INSERTION_SHIFT);
	else
		tdes3 &= ~((u32)TX_CIC_FULL << TDES3_CHECKSUM_INSERTION_SHIFT);


	if (ls)
		tdes3 |= (u32)(TDES3_LAST_DESCRIPTOR);
	else
		tdes3 &= (u32)~(TDES3_LAST_DESCRIPTOR);

	if (0 != tx_own)
		tdes3 |= (u32)TDES3_OWN;

	if (0U != ((u32)is_fs & (u32)tx_own))
		dma_wmb();

	p->des3 = cpu_to_le32(tdes3);

}

/* code review E1: do not need to return value */
static void enable_tx_timestamp(struct dma_desc *p)
{
	p->des2 |= cpu_to_le32(TDES2_TIMESTAMP_ENABLE);
}

/* code review E1: do not need to return value */
static void set_mss(struct dma_desc *p, u32 mss)
{
	p->des0 = 0;
	p->des1 = 0;
	p->des2 =cpu_to_le32(mss);
	p->des3 = cpu_to_le32(TDES3_CONTEXT_TYPE | TDES3_CTXT_TCMSSV);
}


/**
 * prepare_tso_tx_desc
 * @p: dma_desc struct poniter
 * @is_fs: is first descriptor or not
 * @len1: length 1
 * @len2: length 2
 * @tx_own: is tx owner this or not
 * @ls: is last descriptor or not
 * @tcphdrlen: tcp header length
 * @tcppayloadlen: tcp packet payload length
 * Description: prepare tso tx descriptor
 * Return:NA
 */
/* code review E1: do not need to return value */
static void prepare_tso_tx_desc(struct dma_desc *p, s32 is_fs, s32 len1, s32 len2, bool tx_own, bool ls,
										u32 tcphdrlen, u32 tcppayloadlen)
{
	u32 tdes3 = le32_to_cpu(p->des3);

	if (0 != len1)
		p->des2 |= cpu_to_le32((len1 & TDES2_BUFFER1_SIZE_MASK));/*PRQA S 1821, 4501, 0478, 0636, 1882, 4532*/

	if (0 != len2)
		p->des2 |= cpu_to_le32((len2 << TDES2_BUFFER2_SIZE_MASK_SHIFT) & TDES2_BUFFER2_SIZE_MASK);/*PRQA S 1821, 4501, 0478, 0636, 1882, 4532, 4533*/

	if (0 != is_fs) {
		tdes3 |= TDES3_FIRST_DESCRIPTOR | ((tcphdrlen << TDES3_HDR_LEN_SHIFT) & TDES3_SLOT_NUMBER_MASK) |/*PRQA S 1891, 4461, 0478, 0636, 1882, 4501*/
			TDES3_TCP_SEGMENTATION_ENABLE | ((tcppayloadlen & TDES3_TCP_PKT_PAYLOAD_MASK));/*PRQA S 0636, 0478, 1882, 4501*/
	} else  {
		tdes3 &= (u32)~TDES3_FIRST_DESCRIPTOR;
	}

	if (ls)
		tdes3 |= (u32)TDES3_LAST_DESCRIPTOR;
	else
		tdes3 &= (u32)~TDES3_LAST_DESCRIPTOR;

	if (tx_own)
		tdes3 |= (u32)TDES3_OWN;

	if ((0 != is_fs) && tx_own)
		dma_wmb();

	p->des3 = cpu_to_le32(tdes3);
}

/**
 * tso_allocator
 * @priv: hobot_priv struct poniter
 * @des: dma address
 * @total_len: totoal length
 * @last_segment: is last segment or not
 * @queue: queue id
 * Description: allocate tso needed, likely descriptor.
 * Return:NA
 */
/* code review E1: do not need to return value */
static void tso_allocator(struct hobot_priv *priv, dma_addr_t des, s32 total_len, bool last_segment, u32 queue)
{
	struct dma_tx_queue *tx_q = &priv->tx_queue[queue];
	struct dma_desc *desc;
	u32 buff_size;
	s32 tmp_len;

	tmp_len = total_len;
	while (tmp_len > 0) {
		tx_q->cur_tx = get_next_entry(tx_q->cur_tx, DMA_TX_SIZE);
		desc = tx_q->dma_tx + tx_q->cur_tx;
		desc->des0 = cpu_to_le32(des + (total_len - tmp_len));/*PRQA S 1820*/
		desc->des1 = cpu_to_le32(upper_32_bits(des + (total_len - tmp_len)));/*PRQA S 1820*/
		buff_size = (tmp_len >= TSO_MAX_BUFF_SIZE) ? (u32)TSO_MAX_BUFF_SIZE : (u32)tmp_len;

		prepare_tso_tx_desc(desc, 0, (s32)buff_size, 0, (bool)true, (last_segment && (tmp_len <= TSO_MAX_BUFF_SIZE)), 0,0);
		tmp_len -= TSO_MAX_BUFF_SIZE;
	}
}



/**
 *  stmmac_tso_xmit - Tx entry point of the driver for oversized frames (TSO)
 *  @skb : the socket buffer
 *  @dev : device pointer
 *  Description: this is the transmit function that is called on TSO frames
 *  (support available on GMAC4 and newer chips).
 *  Diagram below show the ring programming in case of TSO frames:
 *
 *  First Descriptor
 *   --------
 *   | DES0 |---> buffer1 = L2/L3/L4 header
 *   | DES1 |---> TCP Payload (can continue on next descr...)
 *   | DES2 |---> buffer 1 and 2 len
 *   | DES3 |---> must set TSE, TCP hdr len-> [22:19]. TCP payload len [17:0]
 *   --------
 *	|
 *     ...
 *	|
 *   --------
 *   | DES0 | --| Split TCP Payload on Buffers 1 and 2
 *   | DES1 | --|
 *   | DES2 | --> buffer 1 and 2 len
 *   | DES3 |
 *   --------
 *
 * mss is fixed when enable tso, so w/o programming the TDES3 ctx field.
 */
static netdev_tx_t tso_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	struct dma_desc *desc, *first, *mss_desc = NULL;
	struct hobot_priv *priv = (struct hobot_priv *)netdev_priv(ndev);
	s32 nfrags = skb_shinfo(skb)->nr_frags;/*PRQA S 3305*/
	u32 queue = skb_get_queue_mapping(skb);
	u32 first_entry;
	dma_addr_t dma_addr;
	struct dma_tx_queue *tx_q;
	s32 tmp_pay_len;
	u32 pay_len, mss;
	u8 proto_hdr_len;
	s32 i;

	pr_debug("%s, and transport offset:%d, tcphdrlen:%d\n", __func__,/*PRQA S 1294, 0685*/ \
		skb_transport_offset(skb), tcp_hdrlen(skb));
	tx_q = &priv->tx_queue[queue];
	proto_hdr_len = (u8)skb_transport_offset(skb) + (u8)tcp_hdrlen(skb);
	if (unlikely(tx_queue_avail(priv, queue) < (((skb->len - proto_hdr_len) / TSO_MAX_BUFF_SIZE + 1)))) {
		if (!netif_tx_queue_stopped(netdev_get_tx_queue(ndev, queue))) {
			netif_tx_stop_queue(netdev_get_tx_queue(priv->ndev, queue));
			netdev_err(ndev, "%s, tx ring full when queue awake\n", __func__);
		}
		return NETDEV_TX_BUSY;
	}

	pay_len = skb_headlen(skb) - proto_hdr_len;
	mss = skb_shinfo(skb)->gso_size;/*PRQA S 3305*/
	if (mss != priv->tx_queue[queue].mss) {
		mss_desc = tx_q->dma_tx + tx_q->cur_tx;
		set_mss(mss_desc, mss);
		priv->tx_queue[queue].mss = mss;
		tx_q->cur_tx = get_next_entry(tx_q->cur_tx, DMA_TX_SIZE);
	}

	first_entry = tx_q->cur_tx;
	desc = tx_q->dma_tx + first_entry;
	first = desc;

	dma_addr = dma_map_single(priv->device, (void *)skb->data, skb_headlen(skb), DMA_TO_DEVICE);
	if (0 != dma_mapping_error(priv->device, dma_addr))
		goto dma_map_err;

	tx_q->tx_skbuff_dma[first_entry].buf = dma_addr;
	tx_q->tx_skbuff_dma[first_entry].len = skb_headlen(skb);

	first->des0 = cpu_to_le32(dma_addr);
	first->des1 = cpu_to_le32(upper_32_bits(dma_addr));

	//if (pay_len)
	//	first->des1 = cpu_to_le32(des + proto_hdr_len);
	tmp_pay_len = (s32)pay_len;
	dma_addr += proto_hdr_len;
	pay_len = 0;


	tso_allocator(priv, dma_addr, tmp_pay_len, (nfrags == 0), queue);

	for(i = 0; i < nfrags; i++) {
		const skb_frag_t *frag = &skb_shinfo(skb)->frags[i];/*PRQA S 3305*/
		bool last = (bool)false;

		dma_addr = skb_frag_dma_map(priv->device, frag, 0, skb_frag_size(frag), DMA_TO_DEVICE);
		if (0 != dma_mapping_error(priv->device, dma_addr))
			goto dma_map_err;
		if (i == (nfrags - 1))
			last = (bool)true;
		tso_allocator(priv, dma_addr, (s32)skb_frag_size(frag), last, queue);

		tx_q->tx_skbuff_dma[tx_q->cur_tx].buf = dma_addr;
		tx_q->tx_skbuff_dma[tx_q->cur_tx].len = skb_frag_size(frag);
		tx_q->tx_skbuff[tx_q->cur_tx] = NULL;
		tx_q->tx_skbuff_dma[tx_q->cur_tx].map_as_page = (bool)true;
	}


	tx_q->tx_skbuff_dma[tx_q->cur_tx].last_segment = (bool)true;

	tx_q->tx_skbuff[tx_q->cur_tx] = skb;

	tx_q->cur_tx = get_next_entry(tx_q->cur_tx, DMA_TX_SIZE);
	if (unlikely(tx_queue_avail(priv, queue) <= (MAX_SKB_FRAGS + 1))) {
		netif_tx_stop_queue(netdev_get_tx_queue(priv->ndev, queue));
	}

	ndev->stats.tx_bytes += skb->len;
	priv->xstats.tx_tso_frames++;
	priv->xstats.tx_tso_nfrags += (u32)nfrags;

	desc->des2 |= cpu_to_le32(TDES2_INTERRUPT_ON_COMPLETION);
	priv->xstats.tx_set_ic_bit++;


	skb_tx_timestamp(skb);

	if (unlikely((skb_shinfo(skb)->tx_flags & SKBTX_HW_TSTAMP) && priv->hwts_tx_en)) {/*PRQA S 1861, 3305*/
		skb_shinfo(skb)->tx_flags |= SKBTX_IN_PROGRESS;/*PRQA S 1861, 3305*/
		enable_tx_timestamp(first);
	}

	prepare_tso_tx_desc(first, 1, proto_hdr_len, pay_len, 1, tx_q->tx_skbuff_dma[first_entry].last_segment,/*PRQA S 1294*/
					tcp_hdrlen(skb)/4, (skb->len - proto_hdr_len));

	if (NULL != mss_desc) {
		dma_wmb();
		mss_desc->des3 |= cpu_to_le32(TDES3_OWN);
	}

	dma_wmb();

	netdev_tx_sent_queue(netdev_get_tx_queue(ndev, queue), skb->len);
	set_tx_tail_ptr(priv->ioaddr, tx_q->tx_tail_addr, queue);
	mod_timer(&priv->txtimer, HOBOT_COAL_TIMER(HOBOT_COAL_TX_TIMER));

	return NETDEV_TX_OK;

dma_map_err:
	netdev_err(priv->ndev, "Tx DMA map failed\n");
	dev_kfree_skb(skb);
	priv->ndev->stats.tx_dropped++;
	return NETDEV_TX_OK;
}

#ifdef HOBOT_ETH_LOG_DEBUG
static void print_pkt(u8 *buf, s32 len)
{
	pr_debug("%s, len = %d byte, buf addr: 0x%p\n", __FILE__, len, buf);/*PRQA S 1294, 0685*/
	print_hex_dump_bytes("", DUMP_PREFIX_OFFSET, buf, len);
}
#endif

/**
 *  netdev_start_xmit - Tx entry point of the driver
 *  @skb : the socket buffer
 *  @dev : device pointer
 *  Description : this is the tx entry point of the driver.
 *  It programs the chain or the ring and supports oversized frames
 *  and SG feature.
 */
static netdev_tx_t eth_netdev_do_start_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	struct hobot_priv *priv;
	u32 nopaged_len;
	s32 nfrags;
	u32 entry;
	u32 first_entry;
	u32 queue;
	struct dma_tx_queue *tx_q;
	struct dma_desc *desc, *first;
	dma_addr_t dma_addr;
	s32 i;
	struct timespec64 now;
	bool csum_insert;
	bool last_segment;

	if (unlikely((NULL == ndev) || (NULL == skb))) {
		(void)pr_err("%s, para is null\n", __func__);
		return NETDEV_TX_BUSY;
	}
	priv = (struct hobot_priv *)netdev_priv(ndev);
	nopaged_len = skb_headlen(skb);
	nfrags = skb_shinfo(skb)->nr_frags;/*PRQA S 3305*/
	queue = skb_get_queue_mapping(skb);
	if (unlikely(! netif_carrier_ok(ndev))) {
		netdev_dbg(ndev, "%s carrier off\n", __func__);/*PRQA S 1294, 0685*/
		return NETDEV_TX_BUSY;
	}

	pr_debug("nr_frags=%d, skb len:%d, skb is gso:%d, priv->tso:%d, skb_shinfo->gso_type:0x%x\n", /*PRQA S 1294, 0685, 3305*/
		nfrags, skb->len, skb_is_gso(skb), priv->tso, skb_shinfo(skb)->gso_type);
	tx_q = &priv->tx_queue[queue];
	if (skb_is_gso(skb) && priv->tso) {
		if (skb_shinfo(skb)->gso_type &/*PRQA S 3305*/
			(SKB_GSO_TCPV4 | SKB_GSO_TCPV6 | SKB_GSO_UDP_L4)) {
			return tso_xmit(skb, ndev);
		}
	}

	if ((tx_queue_avail(priv, queue) < (u32)(nfrags + 1))) {
		if (!netif_tx_queue_stopped(netdev_get_tx_queue(ndev, queue))) {
			netif_tx_stop_queue(netdev_get_tx_queue(priv->ndev, queue));
			netdev_dbg(ndev, "%s and tx stop queue\n", __func__); /*PRQA S 1294, 0685*/
		}
		return NETDEV_TX_BUSY;
	}

	entry = tx_q->cur_tx;
	first_entry = entry;

	pr_debug("%s,tx queue:%d, entry:%d\n", __func__, queue, entry);/*PRQA S 1294, 0685*/
	csum_insert = (skb->ip_summed == (u8)CHECKSUM_PARTIAL);

	if (0 != priv->extend_desc)
		desc = (struct dma_desc *)(tx_q->dma_etx + entry);
	else
		desc = tx_q->dma_tx + entry;

	first = desc;
	for (i = 0; i < nfrags; i++) {
		const skb_frag_t *frag = &skb_shinfo(skb)->frags[i];/*PRQA S 3305*/
		s32 len = (s32)skb_frag_size(frag);
		last_segment = (i == (nfrags - 1));

		entry = get_next_entry(entry, DMA_TX_SIZE);

		if (0 != priv->extend_desc)
			desc = (struct dma_desc *)(tx_q->dma_etx + entry);
		else
			desc = tx_q->dma_tx + entry;

		dma_addr = skb_frag_dma_map(priv->device, frag, 0, (u32)len, DMA_TO_DEVICE);
		if (0 != dma_mapping_error(priv->device, dma_addr)) {
			netdev_err(priv->ndev, "%s,%d:Tx DMA map failed\n", __func__, __LINE__);
			goto dma_map_err;
		}

		tx_q->tx_skbuff[entry] = NULL;
		tx_q->tx_skbuff_dma[entry].buf = dma_addr;

		desc->des0 = cpu_to_le32(dma_addr);
		desc->des1 = cpu_to_le32(upper_32_bits(dma_addr));

		tx_q->tx_skbuff_dma[entry].map_as_page = (bool)true;
		tx_q->tx_skbuff_dma[entry].len = (u32)len;
		tx_q->tx_skbuff_dma[entry].last_segment = last_segment;

		prepare_tx_desc(desc, 0, len, csum_insert, 1, last_segment, skb->len);

	}

	tx_q->tx_skbuff[entry] = skb;
	entry = get_next_entry(entry, DMA_TX_SIZE);
	tx_q->cur_tx = entry;


	if (tx_queue_avail(priv, queue) <= (MAX_SKB_FRAGS + 1)) {
		netif_tx_stop_queue(netdev_get_tx_queue(priv->ndev, queue));
	}

	ndev->stats.tx_bytes += skb->len;/*PRQA S 2812*/


	desc->des2 |= cpu_to_le32(TDES2_INTERRUPT_ON_COMPLETION);


	skb_tx_timestamp(skb);

	/* last segment */
	last_segment = (nfrags == 0);
	dma_addr = dma_map_single(priv->device, (void *)skb->data, nopaged_len, DMA_TO_DEVICE);
	if (0 != dma_mapping_error(priv->device, dma_addr)) {
		netdev_err(priv->ndev, "%s,%d:Tx DMA map failed\n", __func__, __LINE__);
		goto dma_map_err;
	}
	tx_q->tx_skbuff_dma[first_entry].buf = dma_addr;
	first->des0 = cpu_to_le32(dma_addr);
	first->des1 = cpu_to_le32(upper_32_bits(dma_addr));

	tx_q->tx_skbuff_dma[first_entry].len = nopaged_len;
	tx_q->tx_skbuff_dma[first_entry].last_segment = last_segment;

	if ((skb_shinfo(skb)->tx_flags & SKBTX_HW_TSTAMP) && priv->hwts_tx_en) {/*PRQA S 3305, 1861*/
		skb_shinfo(skb)->tx_flags |= SKBTX_IN_PROGRESS;/*PRQA S 3305, 1861*/
		enable_tx_timestamp(first);
		ktime_get_real_ts64(&now);
	}

	prepare_tx_desc(first, 1, (s32)nopaged_len, csum_insert, 1, last_segment, skb->len);
	dma_wmb();

#ifdef HOBOT_ETH_LOG_DEBUG
	print_pkt(skb->data, skb->len);
#endif

	netdev_tx_sent_queue(netdev_get_tx_queue(ndev, queue), skb->len);

	set_tx_tail_ptr(priv->ioaddr, tx_q->tx_tail_addr, queue);
	netif_trans_update(ndev);
	mod_timer(&priv->txtimer, HOBOT_COAL_TIMER(HOBOT_COAL_TX_TIMER));

	return NETDEV_TX_OK;


dma_map_err:
	dev_kfree_skb(skb);
	priv->ndev->stats.tx_dropped++;
	return NETDEV_TX_OK;
}



/**
 * ptp_get_ts_config
 * @ndev: net_device struct poniter
 * @rq: ifreq struct pointer
 * Description: get ts config
 * Return:0 on success, otherwise error.
 */
static s32 ptp_get_ts_config(struct net_device *ndev, struct ifreq *rq)
{
	struct hobot_priv *priv = (struct hobot_priv *)netdev_priv(ndev);
	struct hwtstamp_config *config = &priv->tstamp_config;

	if (!((0U != priv->dma_cap.time_stamp) || (0U != priv->dma_cap.atime_stamp)))
		return -EOPNOTSUPP;
	if (0U != copy_to_user(rq->ifr_data, (void *)config, sizeof(*config))) {
		netdev_err(priv->ndev, "%s, copy to user failed\n", __func__);
		return -EFAULT;
	}

	return 0;

}



/**
 * ptp_hwtstamp_ctr_config
 * @config: hwtstamp_config struct poniter
 * @ctr_config: hwtstamp_ctr_config struct pointer
 * @ndev: net_device struct pointer
 * Description: according config->rx_filter to config crt_config pointer
 * called by ptp_set_ts_config
 * Return:NA
 */
static void ptp_hwtstamp_ctr_config(struct hwtstamp_config *config, struct hwtstamp_ctr_config *ctr_config, struct net_device *ndev)
{
	switch (config->rx_filter) {
		case (s32)HWTSTAMP_FILTER_PTP_V1_L4_EVENT:
			ctr_config->snap_type_sel = PTP_GMAC4_TCR_SNAPTYPSEL_1;/*PRQA S 0478, 0636, 4501, 1882*/
			ctr_config->ptp_over_ipv4_udp = (u32)PTP_TCR_TSIPV4ENA;
			ctr_config->ptp_over_ipv6_udp = (u32)PTP_TCR_TSIPV6ENA;
			break;
		case (s32)HWTSTAMP_FILTER_PTP_V1_L4_SYNC:
			ctr_config->ts_event_en = (u32)PTP_TCR_TSEVNTENA;
			ctr_config->ptp_over_ipv4_udp = (u32)PTP_TCR_TSIPV4ENA;
			ctr_config->ptp_over_ipv6_udp = (u32)PTP_TCR_TSIPV6ENA;
			break;

		case (s32)HWTSTAMP_FILTER_PTP_V1_L4_DELAY_REQ:
			ctr_config->ts_master_en = (u32)PTP_TCR_TSMSTRENA;
			ctr_config->ts_event_en = (u32)PTP_TCR_TSEVNTENA;
			ctr_config->ptp_over_ipv4_udp = (u32)PTP_TCR_TSIPV4ENA;
			ctr_config->ptp_over_ipv6_udp = (u32)PTP_TCR_TSIPV6ENA;
			break;
		case (s32)HWTSTAMP_FILTER_PTP_V2_L4_EVENT:
			ctr_config->ptp_v2 = (u32)PTP_TCR_TSVER2ENA;
			ctr_config->snap_type_sel = PTP_GMAC4_TCR_SNAPTYPSEL_1;/*PRQA S 1882, 0478, 0636, 4501*/
			ctr_config->ptp_over_ipv4_udp = (u32)PTP_TCR_TSIPV4ENA;
			ctr_config->ptp_over_ipv6_udp = (u32)PTP_TCR_TSIPV6ENA;
			break;
		case (s32)HWTSTAMP_FILTER_PTP_V2_L4_SYNC:
			ctr_config->ptp_v2 = (u32)PTP_TCR_TSVER2ENA;
			ctr_config->ts_event_en = (u32)PTP_TCR_TSEVNTENA;
			ctr_config->ptp_over_ipv4_udp = (u32)PTP_TCR_TSIPV4ENA;
			ctr_config->ptp_over_ipv6_udp = (u32)PTP_TCR_TSIPV6ENA;
			break;

		case (s32)HWTSTAMP_FILTER_PTP_V2_L4_DELAY_REQ:
			ctr_config->ptp_v2 = (u32)PTP_TCR_TSVER2ENA;
			ctr_config->ts_master_en = (u32)PTP_TCR_TSMSTRENA;
			ctr_config->ptp_over_ipv4_udp = (u32)PTP_TCR_TSIPV4ENA;
			ctr_config->ptp_over_ipv6_udp = (u32)PTP_TCR_TSIPV6ENA;
			break;
		case (s32)HWTSTAMP_FILTER_PTP_V2_EVENT:
			ctr_config->ptp_v2 = (u32)PTP_TCR_TSVER2ENA;
			ctr_config->snap_type_sel = PTP_GMAC4_TCR_SNAPTYPSEL_1;/*PRQA S 1882, 0478, 0636, 4501*/
			ctr_config->ptp_over_ipv4_udp = (u32)PTP_TCR_TSIPV4ENA;
			ctr_config->ptp_over_ipv6_udp = (u32)PTP_TCR_TSIPV6ENA;
			ctr_config->ptp_over_ethernet = (u32)PTP_TCR_TSIPENA;
			break;

		case (s32)HWTSTAMP_FILTER_PTP_V2_SYNC:
			ctr_config->ptp_v2 = (u32)PTP_TCR_TSVER2ENA;
			ctr_config->ts_event_en = (u32)PTP_TCR_TSEVNTENA;
			ctr_config->ptp_over_ipv4_udp = (u32)PTP_TCR_TSIPV4ENA;
			ctr_config->ptp_over_ipv6_udp = (u32)PTP_TCR_TSIPV6ENA;
			ctr_config->ptp_over_ethernet = (u32)PTP_TCR_TSIPENA;
			break;

		case (s32)HWTSTAMP_FILTER_PTP_V2_DELAY_REQ:
			config->rx_filter = (s32)HWTSTAMP_FILTER_PTP_V2_SYNC;
			ctr_config->ts_master_en = (u32)PTP_TCR_TSMSTRENA;
			ctr_config->ptp_v2 = (u32)PTP_TCR_TSVER2ENA;
			ctr_config->ts_event_en = (u32)PTP_TCR_TSEVNTENA;
			ctr_config->ptp_over_ipv4_udp = (u32)PTP_TCR_TSIPV4ENA;
			ctr_config->ptp_over_ipv6_udp = (u32)PTP_TCR_TSIPV6ENA;
			ctr_config->ptp_over_ethernet = (u32)PTP_TCR_TSIPENA;
			break;
		case (s32)HWTSTAMP_FILTER_NTP_ALL:
		case (s32)HWTSTAMP_FILTER_ALL:
			config->rx_filter = (s32)HWTSTAMP_FILTER_ALL;
			ctr_config->tstamp_all = (u32)PTP_TCR_TSENALL;
			break;
		default:
			netdev_err(ndev, "%s, not supported ptp config\n", __func__);
		}

}


/**
 * ptp_set_ts_config
 * @ndev: net_device struct pointer
 * @ifr: ifreq struct pointer
 * Description: ptp set ts function config
 * called by eth_netdev_do_ioctl
 * Return:0 on success, otherwise error.
 */
static s32 ptp_set_ts_config(struct net_device *ndev, struct ifreq *ifr)
{
	struct hobot_priv *priv = (struct hobot_priv *)netdev_priv(ndev);
	struct hwtstamp_config config;
	struct hwtstamp_ctr_config ctr_config;
	u64 temp;
	u32 sec_inc;
	u32 value;

	if (!((0U != priv->dma_cap.time_stamp) || (0U != priv->adv_ts))) {
		netdev_err(ndev, "No support for HW time stamping\n");
		priv->hwts_tx_en = 0;
		priv->hwts_rx_en = 0;
		return -EOPNOTSUPP;
	}
	/* initialize struct ctr_config */
	(void)memset((void *)&ctr_config, 0, sizeof(struct hwtstamp_ctr_config));
	if (0U != copy_from_user((void *)&config, ifr->ifr_data, sizeof(config))) {
		netdev_err(ndev, "%s, copy from user failed\n", __func__);
		return -EFAULT;
	}


	pr_debug("%s, config flags: 0x%x, tx_type:0x%x, rx_filter:0x%x\n", __func__, config.flags, /*PRQA S 1294, 0685*/
				config.tx_type, config.rx_filter);

	if (0 != config.flags) {
		netdev_err(ndev, "hwtstamp_config flags para error\n");
		return -EINVAL;
	}

	if ((config.tx_type != (s32)HWTSTAMP_TX_OFF) && (config.tx_type != (s32)HWTSTAMP_TX_ON)) {
		netdev_err(ndev, "hwtstamp_config tx_type para error\n");
		return -EINVAL;
	}


	if (0U != priv->adv_ts) {
		ptp_hwtstamp_ctr_config(&config, &ctr_config, ndev);

	} else {

		switch (config.rx_filter) {
		case (s32)HWTSTAMP_FILTER_NONE:
			config.rx_filter = (s32)HWTSTAMP_FILTER_NONE;
			break;
		default:
			config.rx_filter = (s32)HWTSTAMP_FILTER_PTP_V1_L4_EVENT;
			break;
		}

	}

	priv->hwts_rx_en = ((config.rx_filter == (s32)HWTSTAMP_FILTER_NONE) ? 0 : 1);
	priv->hwts_tx_en = ((config.tx_type == (s32)HWTSTAMP_TX_ON) ? 1 : 0);

	if ((0 == priv->hwts_tx_en) && (0 == priv->hwts_rx_en)) {
		config_hw_tstamping(priv, 0);
	} else {
		value = (u32)(PTP_TCR_TSENA|PTP_TCR_TSCFUPDT|PTP_TCR_TSCTRLSSR|(ctr_config.tstamp_all)|(ctr_config.ptp_v2)|(ctr_config.ptp_over_ethernet)| \
			(ctr_config.ptp_over_ipv6_udp)|(ctr_config.ptp_over_ipv4_udp)|(ctr_config.ts_event_en)|(ctr_config.ts_master_en)|(ctr_config.snap_type_sel));
		value |= (u32)(PTP_TCR_CSC | PTP_TCR_TSEVNTENA);
		value &= (u32)~PTP_TCR_SNAPTYPSEL_2;


		pr_debug("%s,and set value is 0x%x\n", __func__, value); /*PRQA S 1294, 0685*/
		config_hw_tstamping(priv, value);


		sec_inc = config_sub_second_increment(priv,priv->plat->clk_ptp_rate);
		temp = div_u64(PER_SEC_NS, sec_inc);
		priv->sub_second_inc = sec_inc;
		priv->systime_flags = value;
		temp = (u64)(temp << SZ_32);
		priv->default_addend = (u32)div_u64(temp, priv->plat->clk_ptp_rate);
		//priv->default_addend = 0xCCCCB8CD;//div_u64(temp, priv->plat->clk_ptp_rate);
		pr_debug("+++sub_second_inc=%u,default_addend=%u+++\n", priv->sub_second_inc, priv->default_addend);/*PRQA S 1294, 0685*/


		if (config_addend(priv,  priv->default_addend) < 0) {
			netdev_err(priv->ndev, "%s, config addend err\n", __func__);
			return -EBUSY;
		}
	}


	(void)memcpy((void *)&priv->tstamp_config, (void *)&config, sizeof(config));
	if (0U != copy_to_user(ifr->ifr_data, (void *)&config, sizeof(config))) {
		netdev_err(priv->ndev, "%s, copy to user failed\n", __func__);
		return -EFAULT;
	}
	return 0;
}


/**
 *  netdev_do_ioctl - Entry point for the Ioctl
 *  @dev: Device pointer.
 *  @rq: An IOCTL specefic structure, that can contain a pointer to
 *  a proprietary structure used to pass information to the driver.
 *  @cmd: IOCTL command
 *  Description:
 *  Currently it supports HW time stamping.
 */
static s32 eth_netdev_do_ioctl(struct net_device *ndev, struct ifreq *rq, s32 cmd)
{
	struct hobot_priv *priv;
	s32 ret = -EOPNOTSUPP;

	if (NULL == ndev) {
		pr_err("%s, net dev ptr is null\n", __func__);
		return -EINVAL;
	}
	priv = (struct hobot_priv *)netdev_priv(ndev);
	if (!netif_running(ndev))
		return -EINVAL;

	switch (cmd) {
	case SIOCSTIOCTL:
		if ((NULL == priv) || (NULL == rq))
			return -EINVAL;

		ret = do_extension_ioctl(priv, rq->ifr_data);
		break;
	case SIOCGHWTSTAMP:
		ret = ptp_get_ts_config(ndev, rq);
		break;
	case SIOCSHWTSTAMP:
		ret = ptp_set_ts_config(ndev, rq);
		break;
	case SIOCGMIIPHY:
	case SIOCGMIIREG:
	case SIOCSMIIREG:
		if (NULL == priv->phylink) {
			netdev_err(ndev, "no phydev\n");
			return -EINVAL;
		}
		ret = phylink_mii_ioctl(priv->phylink, rq, cmd);
		break;
	default:
		netdev_warn(priv->ndev, "%s, cmd %d do not support\n", __func__, cmd);
		break;
	}

	return ret;
}


/**
 * mac_filter_config
 * @ndev: net_device struct pointer
 * Description: set mac filter config
 * called by eth_netdev_set_rx_mode
 * Return:NA
 */
/* code review E1: do not need to return value */
static void mac_filter_config(const struct net_device *ndev)
{
	struct hobot_priv *priv = (struct hobot_priv *)netdev_priv(ndev);
	u32 high, low;
	u32 data;
	s32 index = 1;
	struct netdev_hw_addr *ha;

	netdev_for_each_uc_addr(ha, ndev) {/*PRQA S 0497*/
		high = GMAC_ADDR_HIGH((u32)index);/*PRQA S 3469*/
		low = GMAC_ADDR_LOW((u32)index);/*PRQA S 3469*/

		data = ((u32)ha->addr[BYTES_5] << SZ_8) | (u32)ha->addr[BYTES_4];
		data |= (u32)(GMAC_HI_REG_AE | GMAC_HI_REG_SA);
		iowritel(data, priv->ioaddr, high);

		data = ((u32)ha->addr[BYTES_3] << SZ_24) | ((u32)ha->addr[BYTES_2] << SZ_16) |
			((u32)ha->addr[BYTES_1] << SZ_8) | (u32)ha->addr[BYTES_0];
		iowritel(data, priv->ioaddr, low);

		index++;
	}

	iowritel(GMAC_PACKET_FILTER_SAF, priv->ioaddr, GMAC_PACKET_FILTER);
}

static int eth_netdev_set_features(struct net_device *ndev,
			       netdev_features_t features)
{
	struct hobot_priv *priv = netdev_priv(ndev);

	/* Keep the COE Type in case of csum is supporting */
	if (features & NETIF_F_RXCSUM)
		priv->rx_csum = priv->plat->rx_coe;
	else
		priv->rx_csum = 0;

	/* No check needed because rx_coe has been set before and it will be
	 * fixed in case of issue.
	 */
	rx_ipc_enable(priv);

	return 0;
}

static netdev_features_t eth_netdev_fix_features(struct net_device *ndev,
					     netdev_features_t features)
{
	struct hobot_priv *priv = netdev_priv(ndev);

	if (priv->plat->rx_coe == STMMAC_RX_COE_NONE)
		features &= ~NETIF_F_RXCSUM;

	if (!priv->plat->tx_coe)
		features &= ~NETIF_F_CSUM_MASK;

	/* Disable tso if asked by ethtool */
	if ((priv->plat->tso_en) && (priv->dma_cap.tsoen)) {
		if (features & NETIF_F_TSO)
			priv->tso = true;
		else
			priv->tso = false;
	}

	return features;
}

/**
 * eth_netdev_set_rx_mode
 * @ndev: net_device struct pointer
 * Description: set rx mode
 * called by eth_resume
 * Return:NA
 */
/* code review E1: do not need to return value */
static void eth_netdev_set_rx_mode(struct net_device *ndev)
{
	struct hobot_priv *priv;
	void __iomem *ioaddr;
	u32 value = 0;
	if (NULL == ndev) {
		pr_err("%s, net dev ptr is null\n", __func__);
		return;
	}
	priv = (struct hobot_priv *)netdev_priv(ndev);
	ioaddr = priv->ioaddr;
	if (!priv->plat->unicast_en) {
		if (0U != (ndev->flags & (u32)IFF_PROMISC)) {
			value = (u32)GMAC_PACKET_FILTER_PR;
		} else if ((ndev->flags & IFF_ALLMULTI) || (netdev_mc_count(ndev) > HASH_TABLE_SIZE)) {
			value = (u32)GMAC_PACKET_FILTER_PM;
			iowritel(0xffffffff, ioaddr, GMAC_HASH_TAB_0_31);
			iowritel(0xffffffff, ioaddr, GMAC_HASH_TAB_32_63);
		} else if (!netdev_mc_empty(ndev)) {
			u32 mc_filter[2];
			struct netdev_hw_addr *ha;

			value = (u32)GMAC_PACKET_FILTER_HMC;
			(void)memset((void *)mc_filter, 0, sizeof(mc_filter));
			netdev_for_each_mc_addr(ha, ndev) {/*PRQA S 2810, 0497*/
				u32 bit_nr = (bitrev32(~crc32_le(~0, ha->addr, 6)) >> 26);/*PRQA S 2890*/
				mc_filter[bit_nr >> 5] |= (1 << (bit_nr & 0x1F));/*PRQA S 1821, 4532*/
			}

			iowritel(mc_filter[0], ioaddr, GMAC_HASH_TAB_0_31);
			iowritel(mc_filter[1], ioaddr, GMAC_HASH_TAB_32_63);
		}

		if (netdev_uc_count(ndev) > GMAC_MAX_PERFECT_ADDRESSES) {
			value |= (u32)GMAC_PACKET_FILTER_PR;
		} else if (!netdev_uc_empty(ndev)) {
			s32 reg = 1;
			struct netdev_hw_addr *ha;

			netdev_for_each_uc_addr(ha, ndev) { /*PRQA S 2810, 0497*/
				set_umac_addr(priv->ioaddr, ha->addr, (u32)reg);
				reg++;
			}
		}
		iowritel(value, ioaddr, GMAC_PACKET_FILTER);
	} else {
		mac_filter_config(ndev);
	}
}




static void hobot_reset_subtask(struct hobot_priv *priv)
{
	if (!test_and_clear_bit(STMMAC_RESET_REQUESTED, &priv->state))/*PRQA S 4424*/
		return;
	if (test_bit(STMMAC_DOWN, &priv->state))
		return;

	netdev_err(priv->ndev, "Reset adapter.\n");

	rtnl_lock();
	netif_trans_update(priv->ndev);
	while (test_and_set_bit(STMMAC_RESETING, &priv->state))/*PRQA S 4424*/
		usleep_range(1000, 2000);

	set_bit(STMMAC_DOWN, &priv->state);/*PRQA S 4424*/
	dev_close(priv->ndev);
	dev_open(priv->ndev, NULL);
	clear_bit(STMMAC_DOWN, &priv->state);/*PRQA S 4424*/
	clear_bit(STMMAC_RESETING, &priv->state);/*PRQA S 4424*/
	rtnl_unlock();
}

static void hobot_service_task(struct work_struct *work)
{
	struct hobot_priv *priv = container_of(work, struct hobot_priv,/*PRQA S 0497*/
			service_task);

	hobot_reset_subtask(priv);
	clear_bit(STMMAC_SERVICE_SCHED, &priv->state);/*PRQA S 4424*/
}




static void hobot_service_event_schedule(struct hobot_priv *priv)
{
	if (!test_bit(STMMAC_DOWN, &priv->state) &&
	    !test_and_set_bit(STMMAC_SERVICE_SCHED, &priv->state))/*PRQA S 4424*/
		queue_work(priv->wq, &priv->service_task);
}


static void hobot_global_err(struct hobot_priv *priv)
{
	netif_carrier_off(priv->ndev);
	set_bit(STMMAC_RESET_REQUESTED, &priv->state);/*PRQA S 4424*/
	hobot_service_event_schedule(priv);
}





/**
 *  eth_tx_timeout
 *  @dev : Pointer to net device structure
 *  @txqueue: the index of the hanging transmit queue
 *  Description: this function is called when a packet transmission fails to
 *   complete within a reasonable time. The driver will mark the error in the
 *   netdev structure and arrange for the device to be reset to a sane state
 *   in order to transmit a new packet.
 */
/*Debug only, Otherwise, the barking will be triggered by mistake*/
static void eth_tx_timeout(struct net_device *ndev, u32 txqueue)
{
	struct hobot_priv *priv;
	if (NULL == ndev) {
		(void)pr_err("%s, net dev ptr is null\n", __func__);
		return;
	}
	priv = netdev_priv(ndev);
	netdev_err(ndev, "%s, tx queue%u timeout\n", __func__, txqueue);
	hobot_global_err(priv);
}

static const struct net_device_ops netdev_ops = {
	.ndo_open = eth_netdev_open,
	.ndo_stop = eth_netdev_stop,
	.ndo_start_xmit = eth_netdev_do_start_xmit,
	.ndo_do_ioctl = eth_netdev_do_ioctl,
	.ndo_setup_tc = eth_netdev_setup_tc,
	.ndo_set_rx_mode = eth_netdev_set_rx_mode,
	.ndo_set_mac_address = eth_mac_addr,
	.ndo_select_queue = eth_netdev_tsn_select_queue,
	.ndo_tx_timeout = eth_tx_timeout,
	.ndo_fix_features = eth_netdev_fix_features,
	.ndo_set_features = eth_netdev_set_features,
};


/**
 * ethtool_get_drv_info
 * @ndev: net_device struct pointer
 * @ed: ethtool_drvinfo struct pointer
 * Description: ethtool get eth driver infomation
 * Return:NA
 */
/* code review E1: do not need to return value */
static void ethtool_get_drv_info(struct net_device *ndev, struct ethtool_drvinfo *ed)
{
	const struct hobot_priv *priv;
	if (NULL == ndev) {
		(void)pr_err("%s, net dev ptr is null\n", __func__);
		return;
	}
	priv = (struct hobot_priv *)netdev_priv(ndev);
	(void)strcpy(ed->driver, priv->device->driver->name);
	(void)strcpy(ed->version, DRIVER_VERSION);
}


/**
 * get_strings
 * @ndev: net_device struct pointer
 * @stringset: string want to get
 * @data: string data get
 * Description: ethtool get eth driver infomation
 * Return:NA
 */
/* code review E1: do not need to return value */
static void get_strings(struct net_device *ndev, u32 stringset, u8 *data)
{
	size_t i;
	struct hobot_priv *priv;
	if (NULL == ndev) {
		(void)pr_err("%s, net dev ptr is null\n", __func__);
		return;
	}
	priv = (struct hobot_priv *)netdev_priv(ndev);
	if (stringset != (u32)ETH_SS_STATS) {
		netdev_err(ndev, "%s, para error\n", __func__);
		return;
	}
    if (priv->dma_cap.rmon) {
        for (i = 0; i < STMMAC_MMC_STATS_LEN; i++) {
            memcpy(data, hobot_mmc[i].stat_name, ETH_GSTRING_LEN);/*PRQA S 1495*/
            data += ETH_GSTRING_LEN;
        }
    }

	for (i = 0; i < GMAC_STATS_LEN; ++i) {
		memcpy(data, gmac_gstrings_stats[i].stat_name, ETH_GSTRING_LEN);/*PRQA S 1495*/
		data += ETH_GSTRING_LEN;
	}
}

static s32 ethtool_get_sset_count(struct net_device *ndev, s32 sset) {
	s32 len;
	struct hobot_priv *priv;
	if (NULL == ndev) {
		(void)pr_err("%s, net dev ptr is null\n", __func__);
		return -EINVAL;
	}
	priv = (struct hobot_priv *)netdev_priv(ndev);
	switch (sset) {
	case (s32)ETH_SS_TEST:
		len = 0;
		break;
	case (s32)ETH_SS_STATS:
		len = GMAC_STATS_LEN;
		if (priv->dma_cap.rmon)
			len += STMMAC_MMC_STATS_LEN;/*PRQA S 1820*/
		break;
	case (s32)ETH_SS_PRIV_FLAGS:
		len = 0;
		break;
	default:
		netdev_warn(ndev, "%s, sset %d do not support\n", __func__, sset);
		len = -EOPNOTSUPP;
		break;
	}
	return len;
}


/**
 * get_ethtool_stats
 * @ndev: net_device struct pointer
 * @dummy: ethtool_stats struct pointer
 * @data: data to get
 * Description: ethtool get ethtools status infomation
 * Return:NA
 */
/* code review E1: do not need to return value */
static void get_ethtool_stats(struct net_device *ndev, struct ethtool_stats *dummy, u64 *data)
{
	struct hobot_priv *priv;
	u32 i, j = 0;
	if (NULL == ndev) {
		(void)pr_err("%s, net dev ptr is null\n", __func__);
		return;
	}
	priv = (struct hobot_priv *)netdev_priv(ndev);
	if (priv->dma_cap.rmon) {

		if (NULL == priv->mmcaddr) {
		(void)pr_err("%s, mmcaddr not readly, please wait a moment\n", __func__);
		return;
	}
        hobot_eth_mmc_read(priv->mmcaddr, &priv->mmc);

        for (i = 0; i < STMMAC_MMC_STATS_LEN; i++) {
            char *p;
            p = (char *)priv + hobot_mmc[i].stat_offset;
            data[j++] = (hobot_mmc[i].sizeof_stat == sizeof(u64)) ? (*(u64 *)p)/*PRQA S 3305, 1823*/
                                                                  : (*(u32 *)p);/*PRQA S 3305*/
        }
    }
	for (i = 0; i < GMAC_STATS_LEN; i++) {
		char *p = (char *)priv + gmac_gstrings_stats[i].stat_offset;
		data[j++] = (gmac_gstrings_stats[i].sizeof_stat == sizeof(u64)) ? (*(u64*)p) : (*(u32*)p);/*PRQA S 3305, 1823*/
	}
}


static s32 ethtool_get_regs_len(struct net_device *ndev)
{

	return REG_SPACE_SIZE;
}

static void get_dma_ch_regs(void __iomem *ioaddr, u32 channel, u32 *reg_space)
{
	u32 index = (channel + 1) * ETHTOOL_DMA_OFFSET;

	reg_space[index++] = ioreadl(ioaddr, DMA_CHAN_CONTROL(channel));
	reg_space[index++] = ioreadl(ioaddr, DMA_CHAN_TX_CONTROL(channel));

	reg_space[index++] = ioreadl(ioaddr, DMA_CHAN_RX_CONTROL(channel));

	reg_space[index++] = ioreadl(ioaddr, DMA_CHAN_TX_BASE_ADDR(channel));
	reg_space[index++] = ioreadl(ioaddr, DMA_CHAN_RX_BASE_ADDR(channel));

	reg_space[index++] = ioreadl(ioaddr, DMA_CHAN_TX_END_ADDR(channel));
	reg_space[index++] = ioreadl(ioaddr, DMA_CHAN_RX_END_ADDR(channel));

	reg_space[index++] = ioreadl(ioaddr, DMA_CHAN_TX_RING_LEN(channel));
	reg_space[index++] = ioreadl(ioaddr, DMA_CHAN_RX_RING_LEN(channel));

	reg_space[index++] = ioreadl(ioaddr, DMA_CHAN_INTR_ENA(channel));

	reg_space[index++] =ioreadl(ioaddr, DMA_CHAN_RX_WATCHDOG(channel));

	reg_space[index++] =ioreadl(ioaddr, DMA_CHAN_SLOT_CTRL_STATUS(channel));

	reg_space[index++] =ioreadl(ioaddr, DMA_CHAN_CUR_TX_DESC(channel));
	reg_space[index++] =ioreadl(ioaddr, DMA_CHAN_CUR_RX_DESC(channel));

	reg_space[index++] =ioreadl(ioaddr, DMA_CHAN_CUR_TX_BUF_ADDR(channel));
	reg_space[index++] =ioreadl(ioaddr, DMA_CHAN_CUR_RX_BUF_ADDR(channel));

	reg_space[index++] = ioreadl(ioaddr, DMA_CHAN_STATUS(channel));
}


/**
 * ethtool_get_regs
 * @ndev: net_device struct pointer
 * @regs: ethtool_regs struct pointer
 * @space: data to get
 * Description: ethtool get registers infomation
 * Return:NA
 */
/* code review E1: do not need to return value */
static void ethtool_get_regs(struct net_device *ndev, struct ethtool_regs *regs, void *space)
{
	u32 *reg_space = (u32 *)space;
	struct hobot_priv *priv;
	u32 i;

	if (NULL == ndev) {
		(void)pr_err("%s, net dev ptr is null\n", __func__);
		return;
	}
	priv = (struct hobot_priv *)netdev_priv(ndev);
	(void)memset((void *)reg_space, 0x0, REG_SPACE_SIZE);

	for (i = 0; i < GMAC_REG_NUM; i++)
		reg_space[i] = ioreadl(priv->ioaddr, (i * SZ_4));
	for (i = 0; i < SZ_6; i++) {
		reg_space[GMAC_REG_NUM + i] = ioreadl(priv->ioaddr, DMA_BUS_MODE + i * SZ_4);
	}

	get_dma_ch_regs(priv->ioaddr, 0, reg_space);
}



/**
 * ethtool_get_regs
 * @dev: net_device struct pointer
 * @info: ethtool_ts_info struct pointer
 * Description: ethtool get timestamp infomation
 * Return:0 on success, otherwise error.
 */
static s32 ethtool_get_ts_info(struct net_device *dev, struct ethtool_ts_info *info)
{
	struct hobot_priv *priv;

	if (NULL == dev) {
		(void)pr_err("%s, net dev ptr is null\n", __func__);
		return -EINVAL;
	}
	priv = (struct hobot_priv *)netdev_priv(dev);
	if ((0U != priv->dma_cap.time_stamp) || (0U != priv->dma_cap.atime_stamp)) {
		info->so_timestamping = (u32)SOF_TIMESTAMPING_TX_SOFTWARE | (u32)SOF_TIMESTAMPING_TX_HARDWARE |
					(u32)SOF_TIMESTAMPING_RX_SOFTWARE | (u32)SOF_TIMESTAMPING_RX_HARDWARE |
					(u32)SOF_TIMESTAMPING_SOFTWARE | (u32)SOF_TIMESTAMPING_RAW_HARDWARE;

		if (NULL != priv->ptp_clock)
			info->phc_index = ptp_clock_index(priv->ptp_clock);
		info->tx_types = (1U << (u32)HWTSTAMP_TX_OFF) | (1U << (u32)HWTSTAMP_TX_ON);
		info->rx_filters = ((1U << (u32)HWTSTAMP_FILTER_NONE) | (1U << (u32)HWTSTAMP_FILTER_PTP_V1_L4_EVENT) |
				 (1U << (u32)HWTSTAMP_FILTER_PTP_V1_L4_SYNC) | (1U << (u32)HWTSTAMP_FILTER_PTP_V1_L4_DELAY_REQ) |
				 (1U << (u32)HWTSTAMP_FILTER_PTP_V2_L4_EVENT) | (1U << (u32)HWTSTAMP_FILTER_PTP_V2_L4_SYNC) |
				 (1U << (u32)HWTSTAMP_FILTER_PTP_V2_L4_DELAY_REQ) | (1U << (u32)HWTSTAMP_FILTER_PTP_V2_EVENT) |
				 (1U << (u32)HWTSTAMP_FILTER_PTP_V2_SYNC) | (1U << (u32)HWTSTAMP_FILTER_PTP_V2_DELAY_REQ) |
				 (1U << (u32)HWTSTAMP_FILTER_ALL));
		return 0;
	}
	return ethtool_op_get_ts_info(dev, info);
}


/**
 * ethtool_get_wol
 * @dev: net_device struct pointer
 * @wol: ethtool_wolinfo struct pointer
 * Description: ethtool get wake on lan infomation
 * Return:NA
 */
/* code review E1: do not need to return value */
static void ethtool_get_wol(struct net_device *netdev,
			         struct ethtool_wolinfo *wol)
{
	struct hobot_priv *priv;
	unsigned long flags;

	if (NULL == netdev) {
		(void)pr_err("%s, net dev ptr is null\n", __func__);
		return;
	}
	priv = (struct hobot_priv *)netdev_priv(netdev);
	spin_lock_irqsave(&priv->lock, flags); /*PRQA S 2996*/
	if (device_can_wakeup(priv->device)) {
		wol->supported = (u32)WAKE_MAGIC | (u32)WAKE_UCAST;
		wol->wolopts = (u32)priv->wolopts;
	}
	spin_unlock_irqrestore(&priv->lock, flags);/*PRQA S 2996*/
}


/**
 * ethtool_set_wol
 * @dev: net_device struct pointer
 * @wol: ethtool_wolinfo struct pointer
 * Description: ethtool set wake on lan config
 * Return:0 on success, otherwise error.
 */
static s32 ethtool_set_wol(struct net_device *netdev, struct ethtool_wolinfo *wol)
{
	struct hobot_priv *priv;
	u32 support = (u32)WAKE_MAGIC | (u32)WAKE_UCAST;
	unsigned long flags;

	if (NULL == netdev) {
		(void)pr_err("%s, net dev ptr is null\n", __func__);
		return -EINVAL;
	}
	priv = (struct hobot_priv *)netdev_priv(netdev);

	/* By default almost all GMAC devices support the WoL via
	 * magic frame but we can disable it if the HW capability
	 * register shows no support for pmt_magic_frame. */
	if (0U == priv->dma_cap.pmt_magic_frame)
		wol->wolopts &= ~((u32)WAKE_MAGIC);

	if (!device_can_wakeup(priv->device))
		return -EINVAL;

	if (0U != (wol->wolopts & ~support))
		return -EINVAL;

	if (0U != wol->wolopts) {
		netdev_info(netdev, "wakeup enable\n");
		(void)device_set_wakeup_enable(priv->device, (bool)true);
	} else {
		(void)device_set_wakeup_enable(priv->device, (bool)false);
	}


	spin_lock_irqsave(&priv->lock, flags);/*PRQA S 2996*/
	priv->wolopts = (s32)wol->wolopts;
	spin_unlock_irqrestore(&priv->lock, flags);/*PRQA S 2996*/

	return 0;
}


static s32 ethtool_set_link_ksettings(struct net_device *netdev, const struct ethtool_link_ksettings *cmd)
{
	struct hobot_priv *priv = (struct hobot_priv *)netdev_priv(netdev);
	return phylink_ethtool_ksettings_set(priv->phylink, cmd);
}


static s32 ethtool_get_link_ksettings(struct net_device *netdev, struct ethtool_link_ksettings *cmd)
{
	struct hobot_priv *priv = (struct hobot_priv *)netdev_priv(netdev);
	return phylink_ethtool_ksettings_get(priv->phylink, cmd);
}

/* ethtool operation functions */
static const struct ethtool_ops ethtool_ops = {
	.get_drvinfo = ethtool_get_drv_info,
	.get_link = ethtool_op_get_link,
	.get_link_ksettings = ethtool_get_link_ksettings,
	.set_link_ksettings = ethtool_set_link_ksettings,
	.get_strings = get_strings,
	.get_sset_count = ethtool_get_sset_count,
	.get_ethtool_stats = get_ethtool_stats,
	.get_regs_len = ethtool_get_regs_len,
	.get_regs = ethtool_get_regs,
	.get_ts_info = ethtool_get_ts_info,
	.get_wol = ethtool_get_wol,
	.set_wol = ethtool_set_wol,
};


/**
 * get_tx_status
 * @data: net_device_stats struct pointer
 * @x: extra_stats struct pointer
 * @p: dma_desc struct pointer
 * Description: get tx descriptor status
 * Return:0 on success, otherwise error.
 */
static s32 get_tx_status(struct net_device_stats *data, struct extra_stats *x, const struct dma_desc *p)
{
	struct net_device_stats *stats = (struct net_device_stats *)data;
	u32 tdes3;
	s32 ret = (s32)tx_done;

	tdes3 = le32_to_cpu(p->des3);

	if (0U != (tdes3 & (u32)TDES3_OWN))
		return (s32)tx_dma_own;

	if (0U == (tdes3 & TDES3_LAST_DESCRIPTOR)) {
		return (s32)tx_not_ls;
	}
	if (0U != (tdes3 & TDES3_ERROR_SUMMARY)) {
		if (0U != (tdes3 & TDES3_JABBER_TIMEOUT)) {
			x->tx_jabber++;
		}
		if (0U != (tdes3 & TDES3_PACKET_FLUSHED)) {
			x->tx_frame_flushed++;
		}

		if (0U != (tdes3 & TDES3_LOSS_CARRIER)) {
			x->tx_losscarrier++;
			stats->tx_carrier_errors++;
		}

		if (0U != (tdes3 & TDES3_NO_CARRIER)) {
			x->tx_carrier++;
			stats->tx_carrier_errors++;
		}

		if ((tdes3 & TDES3_LATE_COLLISION) || (tdes3 & TDES3_EXCESSIVE_COLLISION)) {
			stats->collisions += (tdes3 & TDES3_COLLISION_COUNT_MASK) >> TDES3_COLLISION_COUNT_SHIFT;/*PRQA S 1882, 0478, 0636, 4501*/
		}

		if (0U != (tdes3 & TDES3_UNDERFLOW_ERROR)) {
			x->tx_underflow++;
		}

		if (0U != (tdes3 & TDES3_IP_HDR_ERROR)) {
			x->tx_ip_header_error++;
		}

		if (0U != (tdes3 & TDES3_PAYLOAD_ERROR)) {
			x->tx_payload_error++;
		}

		ret = (s32)tx_err;
	}

	if (0U != (tdes3 & TDES3_DEFERRED)) {
		x->tx_deferred++;
	}

	return ret;
}

/**
 * get_tx_timestamp_status
 * @p: dma_desc struct pointer
 * Description: get tx timestamp status
 * called by get_tx_hwstamp
 * Return:0 on success, otherwise error.
 */
static s32 get_tx_timestamp_status(const struct dma_desc *p)
{
	if (0U != (le32_to_cpu(p->des3) & TDES3_CONTEXT_TYPE))
		return 0;

	if (0U != (le32_to_cpu(p->des3) & TDES3_TIMESTAMP_STATUS))
		return 1;

	return 0;
}

/**
 * get_tx_hwstamp
 * @priv: hobot_priv struct pointer
 * @p: dma_desc struct pointer
 * @skb: sk_buff struct pointer
 * Description: get tx timestamp information
 * called by get_tx_hwstamp
 * Return:NA
 */
/* code review E1: do not need to return value */
static void get_tx_hwstamp(const struct hobot_priv *priv, const struct dma_desc *p, struct sk_buff *skb)
{
	struct skb_shared_hwtstamps shhwtstamp;
	u64 ns;
	s32 status;

	if (0 == priv->hwts_tx_en)
		return;

	if ((NULL == skb) || !((skb_shinfo(skb)->tx_flags & SKBTX_IN_PROGRESS))) {/*PRQA S 3305, 1861*/
		return;
	}
	status = get_tx_timestamp_status(p);
	if (0 != status) {
		ns = le32_to_cpu(p->des0);
		ns += le32_to_cpu(p->des1) * PER_SEC_NS;

		(void)memset((void *)&shhwtstamp, 0, sizeof(struct skb_shared_hwtstamps));
		shhwtstamp.hwtstamp = ns_to_ktime(ns);

		skb_tstamp_tx(skb, &shhwtstamp);
	}

	return;
}


/**
 * tx_res_clean - to manage the transmission completion
 * @priv: driver private structure
 * @queue: TX queue index
 * Description: it reclaims the transmit resources after transmission completes.
 */
/* code review E1: do not need to return value */
static void tx_res_clean(struct hobot_priv *priv, u32 queue)
{
	struct dma_tx_queue *tx_q = &priv->tx_queue[queue];
	u32 bytes_compl = 0, pkts_compl = 0;
	u32 entry;

	__netif_tx_lock_bh(netdev_get_tx_queue(priv->ndev, queue));

	priv->xstats.tx_clean++;
	entry = tx_q->dirty_tx;

	while(entry != tx_q->cur_tx) {
		struct sk_buff *skb = tx_q->tx_skbuff[entry];
		struct dma_desc *p;
		s32 status;

		if (0 != priv->extend_desc) {
			p = (struct dma_desc*)(tx_q->dma_etx + entry);
		} else {
			p = tx_q->dma_tx + entry;
		}
		status = get_tx_status(&priv->ndev->stats, &priv->xstats, p);
		if (0U != ((u32)status & (u32)tx_dma_own)) {
			/* avoid dma transmit stop */
			if (entry < tx_q->cur_tx) {
				/* restart dma transmit */
				set_tx_tail_ptr(priv->ioaddr, tx_q->tx_tail_addr, queue);
			}
			pr_debug("%s, dirty: 0x%x, current: 0x%x, queue:%d, status: 0x%x, and hw own tx dma\n", __func__, entry, tx_q->cur_tx, queue, status);/*PRQA S 1294, 0685*/
			break;
		}

		dma_rmb();

		if (0U == ((u32)status & (u32)tx_not_ls)) {
			if (0U != ((u32)status & (u32)tx_err)) {
				priv->ndev->stats.tx_errors++;
			} else {
				priv->ndev->stats.tx_packets++;
			}
			get_tx_hwstamp(priv, p, skb);
		}

		if (0U != tx_q->tx_skbuff_dma[entry].buf) {
			if (tx_q->tx_skbuff_dma[entry].map_as_page)
				dma_unmap_page(priv->device, tx_q->tx_skbuff_dma[entry].buf, tx_q->tx_skbuff_dma[entry].len, DMA_TO_DEVICE);
			else
				dma_unmap_single(priv->device, tx_q->tx_skbuff_dma[entry].buf, tx_q->tx_skbuff_dma[entry].len, DMA_TO_DEVICE);

			tx_q->tx_skbuff_dma[entry].buf = 0;
			tx_q->tx_skbuff_dma[entry].len = 0;
			tx_q->tx_skbuff_dma[entry].map_as_page = (bool)false;
		}

		tx_q->tx_skbuff_dma[entry].last_segment = (bool)false;
		tx_q->tx_skbuff_dma[entry].is_jumbo = (bool)false;

		if (skb != NULL) {
			pkts_compl++;
			bytes_compl += skb->len;
			dev_consume_skb_any(skb);
			tx_q->tx_skbuff[entry] = NULL;
		}

		p->des2 = 0;
		p->des3 = 0;
		entry = get_next_entry(entry, DMA_TX_SIZE);
	}

	tx_q->dirty_tx = entry;

	netdev_tx_completed_queue(netdev_get_tx_queue(priv->ndev, queue),pkts_compl, bytes_compl);

	if (netif_tx_queue_stopped(netdev_get_tx_queue(priv->ndev,queue)) &&
		(tx_queue_avail(priv, queue) > (u32)STMMAC_TX_THRESH)) {
		netif_tx_wake_queue(netdev_get_tx_queue(priv->ndev, queue));
	}

	/* We still have pending packets, let's call for a new scheduling */
	if (tx_q->dirty_tx != tx_q->cur_tx)
		mod_timer(&priv->txtimer, HOBOT_COAL_TIMER(HOBOT_COAL_TX_TIMER));

	__netif_tx_unlock_bh(netdev_get_tx_queue(priv->ndev, queue));
}

static void hobot_tx_timer(struct timer_list *t) {
	struct hobot_priv *priv = from_timer(priv, t, txtimer);/*PRQA S 0497*/
	u32 tx_queues_count = priv->plat->tx_queues_to_use;
	u32 queue;

	for (queue = 0; queue < tx_queues_count; queue++)
		tx_res_clean(priv, queue);
}

static void hobot_init_timer(struct hobot_priv *priv) {
	timer_setup(&priv->txtimer, hobot_tx_timer, 0);
}

/**
 * rdes3_error_check
 * @data: net_device_stats struct pointer
 * @x: extra_stats struct pointer
 * @p: dma_desc pointer
 * Description: check redes3 error or not
 * Return: >=0,rx frame status
 */
static s32 rdes3_error_check(void *data, struct extra_stats *x, const struct dma_desc *p)
{
	struct net_device_stats *stats = (struct net_device_stats *)data;
	u32 rdes3 = le32_to_cpu(p->des3);
	s32 ret = 0;
	if (0U != (rdes3 & RDES3_GIANT_PACKET)) {

			stats->rx_length_errors++;
		}
		if (0U != (rdes3 & RDES3_OVERFLOW_ERROR)) {

			x->rx_gmac_overflow++;
		}

		if (0U != (rdes3 & RDES3_RECEIVE_WATCHDOG)) {

			x->rx_watchdog++;
		}
		if (0U != (rdes3 & RDES3_RECEIVE_ERROR)) {

			x->rx_mii++;
		}

		if (0U != (rdes3 & RDES3_CRC_ERROR)) {

			x->rx_crc_errors++;
			stats->rx_crc_errors++;
		}

		if (0U != (rdes3 & RDES3_DRIBBLE_ERROR)) {

			x->dribbling_bit++;
		}

	ret = (s32)discard_frame;
	return ret;

}

/**
 * message_type_check
 * @message_type: message type
 * @x: extra_stats struct pointer
 * Description: get rx frame status
 * Return: NA
 */
static void message_type_check(s32 message_type, struct extra_stats *x)
{
	switch (message_type) {
		case RDES_EXT_NO_PTP:
			x->no_ptp_rx_msg_type_ext++;
			break;
		case RDES_EXT_SYNC:
			x->ptp_rx_msg_type_sync++;
			break;
		case RDES_EXT_DELAY_REQ:
			x->ptp_rx_msg_type_delay_req++;
			break;
		case RDES_EXT_FOLLOW_UP:
			x->ptp_rx_msg_type_follow_up++;
			break;
		case RDES_EXT_DELAY_RESP:
			x->ptp_rx_msg_type_delay_resp++;
			break;
		case RDES_EXT_PDELAY_REQ:
			x->ptp_rx_msg_type_pdelay_req++;
			break;
		case RDES_EXT_PDELAY_RESP:
			x->ptp_rx_msg_type_pdelay_resp++;
			break;
		case RDES_EXT_PDELAY_FOLLOW_UP:
			x->ptp_rx_msg_type_pdelay_follow_up++;
			break;
		case RDES_PTP_ANNOUNCE:
			x->ptp_rx_msg_type_announce++;
			break;
		case RDES_PTP_MANAGEMENT:
			x->ptp_rx_msg_type_management++;
			break;
		case RDES_PTP_PKT_RESERVED_TYPE:
			x->ptp_rx_msg_pkt_reserved_type++;
			break;
		default:
			pr_debug("%s,message_type error, checkout!\n", __func__);/*PRQA S 1294, 0685*/
			break;
	}
}

/**
 * get_rx_status
 * @data: net_device_stats struct pointer
 * @x: extra_stats struct pointer
 * @p: dma_desc pointer
 * Description: get rx frame status
 * Return: >=0,rx frame status
 */
static s32 get_rx_status(void *data, struct extra_stats *x, const struct dma_desc *p)
{
	u32 rdes0 = le32_to_cpu(p->des0);
	u32 rdes1 = le32_to_cpu(p->des1);
	u32 rdes2 = le32_to_cpu(p->des2);
	u32 rdes3 = le32_to_cpu(p->des3);
	s32 message_type;
	s32 ret = (s32)good_frame;

	pr_debug("rdes0:0x%x, rdes1:0x%x, rdes2:0x%x, rdes3:0x%x\n", rdes0, rdes1, rdes2, rdes3); /*PRQA S 1294, 0685*/
	if (0U != (rdes3 & RDES3_OWN))
		return (s32)dma_own;
	if (0U != (rdes3 & RDES3_CONTEXT_DESCRIPTOR)) {
		return (s32)discard_frame;
	}
	if (0U == (rdes3 & RDES3_LAST_DESCRIPTOR)) {
		return (s32)discard_frame;
	}
	if (0U != (rdes3 &  RDES3_ERROR_SUMMARY)) {
		ret = rdes3_error_check(data, x, p);
	}

	message_type = (rdes1 & ERDES4_MSG_TYPE_MASK) >> SZ_8;/*PRQA S 1882, 0478, 0636, 4501*/

	if (0U != (rdes1 & RDES1_IP_HDR_ERROR)) {
		x->ip_hdr_err++;
	}

	if (0U != (rdes1 & RDES1_IP_CSUM_BYPASSED)) {
		x->ip_csum_bypassed++;
	}

	if (0U != (rdes1 & RDES1_IPV4_HEADER)) {
		x->ipv4_pkt_rcvd++;
	}

	if (0U != (rdes1 & RDES1_IPV6_HEADER)) {
		x->ipv6_pkt_rcvd++;
	}

	message_type_check(message_type, x);

	if (0U != (rdes1 & RDES1_PTP_PACKET_TYPE)) {
		x->ptp_frame_type++;
	}

	if (0U != (rdes1 & RDES1_PTP_VER)) {
		x->ptp_ver++;
	}
	if (0U != (rdes1 & RDES1_TIMESTAMP_DROPPED))
		x->timestamp_dropped++;

	if (0U != (rdes2 & RDES2_SA_FILTER_FAIL)) {
		x->sa_rx_filter_fail++;
		ret = (s32)discard_frame;
	}

	if (0U != (rdes2 & RDES2_L3_FILTER_MATCH))
		x->l3_filter_match++;

	if (0U != (rdes2 & RDES2_L4_FILTER_MATCH))
		x->l4_filter_match++;

	if (0U != ((rdes2 & RDES2_L3_L4_FILT_NB_MATCH_MASK) >> RDES2_L3_L4_FILT_NB_MATCH_SHIFT))/*PRQA S 1882, 0478, 0636, 4501*/
		x->l3_l4_filter_no_match++;

	return ret;
}

/**
 * rx_check_timestamp
 * @desc: dma_desc struct pointer
 * Description: check rx timestamp
 * Return: NA
 */
static s32 rx_check_timestamp(struct dma_desc *desc)
{
	struct dma_desc *p = desc;
	u32 rdes0 = le32_to_cpu(p->des0);
	u32 rdes1 = le32_to_cpu(p->des1);
	u32 rdes3 = le32_to_cpu(p->des3);

	u32 own, ctxt;
	s32 ret = 1;

	own = rdes3 & (u32)RDES3_OWN;
	ctxt = ((rdes3 & (u32)RDES3_CONTEXT_DESCRIPTOR) >> RDES3_CONTEXT_DESCRIPTOR_SHIFT);

	if (likely((0U == own) && (0U != ctxt))) {
		if ((rdes0 == DEFAULT_MASK) && (rdes1 == DEFAULT_MASK))
			ret = -EINVAL;
		else
			ret = 0;
	}

	return ret;
}


/**
 * get_rx_timestamp_status
 * @desc: dma_desc struct pointer
 * @next_desc: next dma_desc pointer
 * Description: get rx timestamp status
 * Return: 1:success
 */
static s32 get_rx_timestamp_status(struct dma_desc *desc, struct dma_desc *next_desc)
{
	struct dma_desc *p = desc;
	s32 ret = -EINVAL;

	if (likely(le32_to_cpu(p->des3) & RDES3_RDES1_VALID)) {
		if (likely(le32_to_cpu(p->des1) & RDES1_TIMESTAMP_AVAILABLE)) {
			s32 i = 0;
			do {
				ret = rx_check_timestamp(next_desc);
				i++;
			}while ((ret == (s32)SZ_1) && (i < (s32)SZ_10));
		}
	}

	if (likely(ret == 0))
		return 1;
	return 0;
}

/**
 * get_rx_hwtstamp
 * @priv: hobot private struct pointer
 * @p: dma_desc pointer
 * @np: next dma_desc pointer
 * @skb: sk_buff struct pointer
 * Description: get rx timestamp
 * Return: NA
 */
static void get_rx_hwtstamp(const struct hobot_priv *priv, struct dma_desc *p, struct dma_desc *np, struct sk_buff *skb)
{
	struct skb_shared_hwtstamps *shhwtstamp;
	struct dma_desc *desc;
	u64 ns;

	if (0 == priv->hwts_rx_en)
		return;

	desc = np;
	if (0 != get_rx_timestamp_status(p, np)) {
		ns = le32_to_cpu(desc->des0);
		ns += le32_to_cpu(desc->des1) * PER_SEC_NS;
		shhwtstamp = skb_hwtstamps(skb);
		(void)memset((void *)shhwtstamp, 0, sizeof(struct skb_shared_hwtstamps));
		shhwtstamp->hwtstamp = ns_to_ktime(ns);
	} else {
		pr_debug("cannot get RX hw timestamp\n"); /*PRQA S 1294, 0685*/
	}

}

static void rx_vlan(struct net_device *ndev, struct sk_buff *skb)
{
	struct ethhdr *ehdr;
	u16 vlanid;

	if (((ndev->features & NETIF_F_HW_VLAN_CTAG_RX) == NETIF_F_HW_VLAN_CTAG_RX) &&
		(0 == __vlan_get_tag(skb, &vlanid))) {
		ehdr = (struct ethhdr *)skb->data;/*PRQA S 3305*/
		memmove(skb->data + VLAN_HLEN, ehdr, ETH_ALEN*SZ_2);/*PRQA S 1495*/
		(void)skb_pull(skb, VLAN_HLEN);
		__vlan_hwaccel_put_tag(skb, htons(ETH_P_8021Q),vlanid);
	}
}

static inline u32 queue_rx_dirty(const struct hobot_priv *priv, u32 queue)
{
	struct dma_rx_queue *rx_q = (struct dma_rx_queue *)&priv->rx_queue[queue];/*PRQA S 0311*/
	u32 dirty;

	if (rx_q->dirty_rx <= rx_q->cur_rx)
		dirty = rx_q->cur_rx - rx_q->dirty_rx;
	else
		dirty = (u32)DMA_RX_SIZE - rx_q->dirty_rx + rx_q->cur_rx;

	return dirty;
}

/**
 * queue_rx_refill - refill used skb preallocated buffers
 * @priv: driver private structure
 * @queue: RX queue index
 * Description : this is to reallocate the skb for the reception process
 * that is based on zero-copy.
 */
/* code review E1: do not need to return value */
static inline void queue_rx_refill(struct hobot_priv *priv, u32 queue)
{
	struct dma_rx_queue *rx_q = &priv->rx_queue[queue];
	s32 dirty = (s32)queue_rx_dirty(priv, queue);
	u32 entry = rx_q->dirty_rx;


	u32 bfsize = priv->dma_buf_sz;
	while (dirty-- > 0) {
		struct dma_desc *p;
		if (0 != priv->extend_desc)
			p = (struct dma_desc *)(rx_q->dma_erx + entry);
		else
			p = rx_q->dma_rx + entry;

		if (NULL == rx_q->rx_skbuff[entry]) {
			struct sk_buff *skb;
			skb = netdev_alloc_skb_ip_align(priv->ndev, bfsize);
			if (NULL == skb) {
				if(0 != net_ratelimit()) {
					netdev_err(priv->ndev, "%s: fail to alloc skb entry%d\n", __func__, entry);
				}
				break;
			}

			rx_q->rx_skbuff[entry] = skb;
			rx_q->rx_skbuff_dma[entry] = dma_map_single(priv->device, (void *)skb->data,
									bfsize, DMA_FROM_DEVICE);
			if (0 != dma_mapping_error(priv->device, rx_q->rx_skbuff_dma[entry])) {
				netdev_err(priv->ndev, "%s: Rx DMA map failed\n", __func__);
				dev_kfree_skb(skb);
				rx_q->rx_skbuff[entry] = NULL;
				break;
			}

			p->des0 = cpu_to_le32(rx_q->rx_skbuff_dma[entry]);
			p->des1 = cpu_to_le32(upper_32_bits(rx_q->rx_skbuff_dma[entry]));
		}

		dma_wmb();

		p->des3 = cpu_to_le32(RDES3_OWN | RDES3_BUFFER1_VALID_ADDR);
		p->des3 |= cpu_to_le32(RDES3_INT_ON_COMPLETION_EN);

		dma_wmb();
		entry = get_next_entry(entry, DMA_RX_SIZE);
	}

	rx_q->dirty_rx = entry;
}


/**
 * napi_rx_packet - manage the receive process
 * @priv: driver private structure
 * @limit: napi bugget
 * @queue: RX queue index.
 * Description :  this the function called by the napi poll method.
 * It gets all the frames inside the ring.
 */
static s32 napi_rx_packet(struct hobot_priv *priv, s32 limit, u32 queue)
{
	struct dma_rx_queue *rx_q = &priv->rx_queue[queue];
	u32 entry = rx_q->cur_rx;
	u32 coe = priv->rx_csum;
	u32 next_entry;
	s32 count = 0;
	struct net_device *ndev = priv->ndev;


	while (count < limit) {
		s32 status;
		struct dma_desc *p, *np;

		if (0 != priv->extend_desc)
			p = (struct dma_desc *)(rx_q->dma_erx + entry);
		else
			p = rx_q->dma_rx + entry;

		status = get_rx_status((void *)&priv->ndev->stats, &priv->xstats, p);
		if (0U != ((u32)status & (u32)dma_own))
			break;


		count++;
		next_entry = get_next_entry(rx_q->cur_rx, DMA_RX_SIZE);

		if (next_entry == rx_q->dirty_rx) {
			netdev_err(ndev, "rx ring buffer is empty.\n");
			break;
		}

		rx_q->cur_rx = next_entry;
		pr_debug("%s, cur_rx:%d\n", __func__, rx_q->cur_rx); /*PRQA S 1294, 0685*/
		if (0 != priv->extend_desc)
			np = (struct dma_desc *)(rx_q->dma_erx + next_entry);
		else
			np = rx_q->dma_rx + next_entry;

		prefetch((void *)np);


		if ((status == (s32)discard_frame) || (status == (s32)error_frame)) {
			if (0 == priv->hwts_rx_en)
				priv->ndev->stats.rx_errors++;

			if ((0 != priv->hwts_rx_en) && (0 == priv->extend_desc)) {
				dma_unmap_single(priv->device, rx_q->rx_skbuff_dma[entry],
							priv->dma_buf_sz, DMA_FROM_DEVICE);
				dev_kfree_skb_any(rx_q->rx_skbuff[entry]);
				rx_q->rx_skbuff[entry] = NULL;
			}
		} else {
			struct sk_buff *skb;
			s32 frame_len;

			frame_len = (le32_to_cpu(p->des3) & RDES3_PACKET_SIZE_MASK);/*PRQA S 1882, 4501, 0636, 0478*/
			if (frame_len > (s32)priv->dma_buf_sz) {
				netdev_err(ndev, "len %d larger than size (%d)\n", frame_len, priv->dma_buf_sz);
				priv->ndev->stats.rx_length_errors++;
				priv->ndev->stats.rx_errors++;
				break;
			}

			if (0U == (ndev->features & NETIF_F_RXFCS)) {
				frame_len -= (s32)SZ_4;
			}

			skb = rx_q->rx_skbuff[entry];
			if (NULL == skb) {
				netdev_err(ndev, "%s: inconsistent Rx chain\n", priv->ndev->name);
				priv->ndev->stats.rx_dropped++;
				break;
			}

			prefetch(skb->data - NET_IP_ALIGN);
			rx_q->rx_skbuff[entry] = NULL;
			(void)skb_put(skb, (u32)frame_len);
			dma_unmap_single(priv->device, rx_q->rx_skbuff_dma[entry], priv->dma_buf_sz, DMA_FROM_DEVICE);


			get_rx_hwtstamp(priv, p, np, skb);
			rx_vlan(priv->ndev, skb);

			skb->protocol = eth_type_trans(skb, priv->ndev);
			pr_debug("[%s]: and protocol:0x%x, and len:%d\n", __func__, skb->protocol, frame_len);/*PRQA S 1294, 0685*/

			if (0U == coe)
				skb_checksum_none_assert(skb);
			else
				skb->ip_summed = CHECKSUM_UNNECESSARY;
#ifdef HOBOT_ETH_LOG_DEBUG
			print_pkt(skb->data, skb->len);
#endif
			(void)napi_gro_receive(&rx_q->napi, skb);
			priv->ndev->stats.rx_packets++;
			priv->ndev->stats.rx_bytes += (u32)frame_len;
		}

		entry = next_entry;
	}

	queue_rx_refill(priv, queue);
	priv->xstats.rx_pkt_n += (u32)count;

	return count;
}





/**
 *  napi_poll - poll method (NAPI)
 *  @napi : pointer to the napi structure.
 *  @budget : maximum number of packets that the current CPU can receive from
 *	      all interfaces.
 *  Description :
 *  To look at the incoming frames and clear the tx resources.
 */
static s32 eth_napi_poll(struct napi_struct *napi, s32 budget)
{
	struct dma_rx_queue *rx_q;
	struct hobot_priv *priv;
	u32 tx_count;
	u32 chan;
	s32 work_done = 0;
	u32 queue;
	if (NULL == napi) {
		(void)pr_err("%s, napi ptr is null\n", __func__);
		return work_done;
	}

	rx_q = container_of(napi, struct dma_rx_queue, napi);/*PRQA S 2810, 0497*/
	priv = rx_q->priv_data;
	tx_count = priv->plat->tx_queues_to_use;
	chan = rx_q->queue_index;

	priv->xstats.napi_poll++;

	for (queue = 0; queue < tx_count; queue++) {
		tx_res_clean(priv, queue);
	}

	work_done = napi_rx_packet(priv, budget, rx_q->queue_index);

	if ((work_done < budget) && napi_complete_done(napi, work_done)) {
		enable_dma_irq(priv, chan);
	}

	return work_done;

}

/**
 * eth_phylink_validate
 * @config: phylink_config struct pointer
 * @supported: supported pointer
 * @state: phy_interface_t struct pointer
 * Description: set eth phylink frame
 * Return: NA
 */
static void eth_phylink_validate(struct phylink_config *config,
			    unsigned long *supported, struct phylink_link_state *state)
{
	struct hobot_priv *priv = netdev_priv(to_net_dev(config->dev));/*PRQA S 2810, 0497*/
	__ETHTOOL_DECLARE_LINK_MODE_MASK(mac_supported) = { 0, };
	__ETHTOOL_DECLARE_LINK_MODE_MASK(mask) = { 0, };
	int tx_cnt = priv->plat->tx_queues_to_use;
	int max_speed = priv->plat->max_speed;

	phylink_set(mac_supported, 10baseT_Half);
	phylink_set(mac_supported, 10baseT_Full);
	phylink_set(mac_supported, 100baseT_Half);
	phylink_set(mac_supported, 100baseT_Full);
	phylink_set(mac_supported, 1000baseT_Half);
	phylink_set(mac_supported, 1000baseT_Full);
	phylink_set(mac_supported, 1000baseKX_Full);

	phylink_set(mac_supported, Autoneg);
	phylink_set(mac_supported, Pause);
	phylink_set(mac_supported, Asym_Pause);
	phylink_set_port_modes(mac_supported);

	/* Cut down 1G if asked to */
	if ((max_speed > 0) && (max_speed < 1000)) {
		phylink_set(mask, 1000baseT_Full);
		phylink_set(mask, 1000baseX_Full);
	}

	/* Half-Duplex can only work with single queue */
	if (tx_cnt > 1) {
		phylink_set(mask, 10baseT_Half);
		phylink_set(mask, 100baseT_Half);
		phylink_set(mask, 1000baseT_Half);
	}

	linkmode_and(supported, supported, mac_supported);
	linkmode_andnot(supported, supported, mask);

	linkmode_and(state->advertising, state->advertising, mac_supported);
	linkmode_andnot(state->advertising, state->advertising, mask);
}

static void eth_mac_pcs_get_state(struct phylink_config *config, struct phylink_link_state *state)
{
	/* Not Supported */
}
static void eth_mac_an_restart(struct phylink_config *config)
{
	/* Not Supported */
}
static void eth_mac_config(struct phylink_config *config, unsigned int mode,
			      const struct phylink_link_state *state)
{
	/* Not Supported */
}

/**
 * eth_mac_link_down
 * @config: phylink_config struct pointer
 * @mode: mode
 * @interface: phy_interface_t value
 * Description: set mac link down
 * Return: NA
 */
static void eth_mac_link_down(struct phylink_config *config,
				 unsigned int mode, phy_interface_t interface)
{
	struct hobot_priv *priv = netdev_priv(to_net_dev(config->dev));/*PRQA S 2810, 0497*/
	u32 value, chan, tx_cnt, txq_status, rxq_status;
	void __iomem *ioaddr = priv->ioaddr;
	tx_cnt = priv->plat->tx_queues_to_use;

	/* 1.disable dma TX transform */
	for (chan = 0; chan < tx_cnt; chan++) {
		value = ioreadl(ioaddr, DMA_CHAN_TX_CONTROL(chan));
		value &= (u32)~DMA_CONTROL_ST;
		iowritel(value, ioaddr, DMA_CHAN_TX_CONTROL(chan));
	}
	/* 2.disable MAC rx */
	value = ioreadl(ioaddr, GMAC_CONFIG);
	value &= (u32)~GMAC_CONFIG_RE;
	iowritel(value, ioaddr, GMAC_CONFIG);

	/* 3.wait, and check data in tx transform */
	udelay(10);/*PRQA S 2880*/
	for (chan = 0; chan < tx_cnt; chan++) {
		value = ioreadl(ioaddr, MTL_CHAN_TX_DEBUG(chan));
		if(0x1 == ((value & MTL_CHAN_TX_READ_CONTR_STATUS) >> MTL_CHAN_TX_READ_CONTR_SHIFT)) {
			netdev_err(priv->ndev, "%s: tX queue:%d is transform.\n", __func__, chan);
		}
	}
	/* 4. disable MAC tx */
	value = ioreadl(ioaddr, GMAC_CONFIG);
	value &= (u32)~GMAC_CONFIG_TE;
	iowritel(value, ioaddr, GMAC_CONFIG);

	/* 5.check TXQSTS and RXQSTS */
	for (chan = 0; chan < tx_cnt; chan++) {
		rxq_status = ioreadl(ioaddr, MTL_CHAN_RX_DEBUG(chan));
		rxq_status = (rxq_status & MTL_CHAN_RXQSTS) >> MTL_CHAN_RXQSTS_SHIFT;
		txq_status = ioreadl(ioaddr, MTL_CHAN_TX_DEBUG(chan));
		txq_status = (txq_status & MTL_CHAN_TXQSTS) >> MTL_CHAN_TXQSTS_SHIFT;
		if((0 != rxq_status) || (0 != txq_status)) {
			netdev_err(priv->ndev, "%s: tx or rx queue not empty!\n", __func__);
		}
	}
	set_link_down(priv);
}
/**
 * eth_mac_link_up
 * @config: phylink_config struct pointer
 * @phy: phy_device pointer
 * @mode: mode
 * @interface: phy_interface_t value
 * @speed: speed value to set
 * @duplex: duples value to set
 * @tx_pause: tx_pause value to set
 * @rx_pause: rx_pause value to set
 * Description: set mac link up
 * Return: NA
 */
static void eth_mac_link_up(struct phylink_config *config,
			       struct phy_device *phy,
			       unsigned int mode, phy_interface_t interface,
			       int speed, int duplex,
			       bool tx_pause, bool rx_pause)
{
	struct hobot_priv *priv = netdev_priv(to_net_dev(config->dev));/*PRQA S 2810, 0497*/
	u32 value, chan, tx_cnt;
	void __iomem *ioaddr = priv->ioaddr;
	tx_cnt = priv->plat->tx_queues_to_use;
	priv->speed = speed;

	set_mac_speed(priv, speed, duplex);
	set_rx_flow_ctrl(priv, rx_pause);
	set_tx_flow_ctrl(priv, tx_pause);

	netif_trans_update(priv->ndev);
	set_link_up(priv);

	for (chan = 0; chan < tx_cnt; chan++) {
		value = ioreadl(ioaddr, DMA_CHAN_TX_CONTROL(chan));
		value |= (u32)DMA_CONTROL_ST;
		iowritel(value, ioaddr, DMA_CHAN_TX_CONTROL(chan));
	}
	enable_mac_transmit(priv, (bool)true);
}


static const struct phylink_mac_ops eth_phylink_mac_ops = {
	.validate = eth_phylink_validate,
	.mac_pcs_get_state = eth_mac_pcs_get_state,
	.mac_config = eth_mac_config,
	.mac_an_restart = eth_mac_an_restart,
	.mac_link_down = eth_mac_link_down,
	.mac_link_up = eth_mac_link_up,
};




/**
 * eth_phy_setup
 * @priv: hobot private struct pointer
 * Description: setup phy
 * Return: 0 on success, otherwise error
 */
static int eth_phy_setup(struct hobot_priv *priv)
{
	struct fwnode_handle *fwnode = of_fwnode_handle(priv->plat->phylink_node);
	int mode = priv->plat->interface;
	struct phylink *phylink;

	priv->phylink_config.dev = &priv->ndev->dev;
	priv->phylink_config.type = PHYLINK_NETDEV;

	if (!fwnode)
		fwnode = dev_fwnode(priv->device);

	phylink = phylink_create(&priv->phylink_config, fwnode,
				 mode, &eth_phylink_mac_ops);/*PRQA S 4432*/
	if (IS_ERR(phylink)) {
		netdev_err(priv->ndev, "phylink_create fail\n");
		return PTR_ERR(phylink);/*PRQA S 4460*/
	}
	priv->phylink = phylink;
	priv->speed = -1;
	return 0;
}

#ifdef CONFIG_HOBOT_FUSA_DIAG
static s32 hobot_eth_notify_diagnose_init_begin(s32 idx)
{
	s32 ret = 0;
	uint16_t module_id_t;
	switch (idx) {
		case 0:
			module_id_t = (u16)ModuleDiag_eth0;
			break;
		case 1:
			module_id_t = (u16)ModuleDiag_eth1;
			break;
		default:
			ret = -ENODEV;
			break;
	}
	if (ret != 0) {
		(void)pr_err("%s: unknow eth interface=%d\n", __func__, idx);
		return ret;
	}
	ret = diagnose_report_startup_status(module_id_t, (u8)MODULE_STARTUP_BEGIN);
	if (ret != 0)
		(void)pr_err("%s: failed\n", __func__);

	return ret;
}

static s32 hobot_eth_notify_diagnose_init_end(s32 idx)
{
	s32 ret = 0;
	uint16_t module_id_t;
	switch (idx) {
		case 0:
			module_id_t = (u16)ModuleDiag_eth0;
			break;
		case 1:
			module_id_t = (u16)ModuleDiag_eth1;
			break;
		default:
			ret = -ENODEV;
			break;
	}
	if (ret != 0) {
		(void)pr_err("%s: unknow eth interface=%d\n", __func__, idx);
		return ret;
	}
	ret = diagnose_report_startup_status(module_id_t, (u8)MODULE_STARTUP_END);
	if (ret != 0)
		(void)pr_err("%s: failed\n", __func__);

	return ret;
}


static s32 hobot_eth_notify_resume_begin(s32 idx)
{
	s32 ret = 0;
	uint16_t module_id_t;
	switch (idx) {
		case 0:
			module_id_t = (u16)ModuleDiag_eth0;
			break;
		case 1:
			module_id_t = (u16)ModuleDiag_eth1;
			break;
		default:
			ret = -ENODEV;
			break;
	}
	if (ret != 0) {
		(void)pr_err("%s: unknow eth interface=%d\n", __func__, idx);
		return ret;
	}
	ret = diagnose_report_wakeup_status(module_id_t, (uint8_t)MODULE_WAKEUP_BEGIN);
	if (ret != 0)
		(void)pr_err("%s: failed\n", __func__);

	return ret;
}

static s32 hobot_eth_notify_resume_end(s32 idx)
{
	s32 ret = 0;
	uint16_t module_id_t;
	switch (idx) {
		case 0:
			module_id_t = (u16)ModuleDiag_eth0;
			break;
		case 1:
			module_id_t = (u16)ModuleDiag_eth1;
			break;
		default:
			ret = -ENODEV;
			break;
	}
	if (ret != 0) {
		(void)pr_err("%s: unknow eth interface=%d\n", __func__, idx);
		return ret;
	}
	ret = diagnose_report_wakeup_status(module_id_t, (uint8_t)MODULE_WAKEUP_END);
	if (ret != 0)
		(void)pr_err("%s: failed\n", __func__);

	return ret;
}
#endif

static s32 hobot_eth_start_probe(const struct platform_device *pdev)
{
#ifdef CONFIG_HOBOT_FUSA_DIAG
	s32 idx_t;
	idx_t = of_alias_get_id(pdev->dev.of_node, "ethernet");
	if(idx_t < 0) {
		return -1;
	}
	(void)hobot_eth_notify_diagnose_init_begin(idx_t);
#endif

	return 0;
}

static s32 hobot_eth_end_probe(const struct platform_device *pdev)
{
#ifdef CONFIG_HOBOT_FUSA_DIAG
	s32 idx_t;
	idx_t = of_alias_get_id(pdev->dev.of_node, "ethernet");
	if(idx_t < 0) {
		return -1;
	}
	(void)hobot_eth_notify_diagnose_init_end(idx_t);
#endif

	return 0;
}


static s32 hobot_eth_start_resume(const struct device *dev)
{
#ifdef CONFIG_HOBOT_FUSA_DIAG
	s32 idx_t;
	idx_t = of_alias_get_id(dev->of_node, "ethernet");
	if(idx_t < 0) {
		return -1;
	}
	(void)hobot_eth_notify_resume_begin(idx_t);
#endif

	return 0;
}

static s32 hobot_eth_end_sesume(const struct device *dev)
{
#ifdef CONFIG_HOBOT_FUSA_DIAG
	s32 idx_t;
	idx_t = of_alias_get_id(dev->of_node, "ethernet");
	if(idx_t < 0) {
		return -1;
	}
	(void)hobot_eth_notify_resume_end(idx_t);
#endif

	return 0;
}

static void sysfs_display_ring(struct net_device *ndev, struct dma_desc *head, int size, dma_addr_t dma_phy_addr)
{
	s32 i;
	struct dma_desc *p = head;
	dma_addr_t dma_addr;

	for (i = 0; i < size; i++) {
		dma_addr = dma_phy_addr + i * sizeof(*p);/*PRQA S 1820*/
		netdev_info(ndev, "%d [%pad]: 0x%x 0x%x 0x%x 0x%x\n",
			   i, &dma_addr,
			   le32_to_cpu(p->des0), le32_to_cpu(p->des1),
			   le32_to_cpu(p->des2), le32_to_cpu(p->des3));
		p++;
	}
}

static ssize_t dump_rx_desc_show(struct device *dev,
                                 struct device_attribute *attr, char *buf) {
	struct net_device *ndev = to_net_dev(dev);/*PRQA S 0497*/
	struct hobot_priv *priv = (struct hobot_priv *)netdev_priv(ndev);
	u32 rx_count, queue;

	if((NULL == dev) || ((ndev->flags & IFF_UP) == 0)) {
		return 0;
	}

	rx_count = priv->plat->rx_queues_to_use;
	for (queue = 0; queue < rx_count; queue++) {
		struct dma_rx_queue *rx_q = &priv->rx_queue[queue];
		netdev_info(ndev, "RX Queue %d, dirty_rx %u, cur_rx %u:\n", queue, rx_q->dirty_rx, rx_q->cur_rx);
		sysfs_display_ring(ndev, rx_q->dma_rx, DMA_RX_SIZE, rx_q->dma_rx_phy);
	}

	return 0;
}
static DEVICE_ATTR_RO(dump_rx_desc);

static ssize_t dump_tx_desc_show(struct device *dev, struct device_attribute *attr,
                            char *buf) {
	struct net_device *ndev = to_net_dev(dev);/*PRQA S 0497*/
	struct hobot_priv *priv = (struct hobot_priv *)netdev_priv(ndev);
	u32 tx_count, queue;

	if((NULL == dev) || ((ndev->flags & IFF_UP) == 0)) {
		return 0;
	}

	tx_count = priv->plat->tx_queues_to_use;
	for (queue = 0; queue < tx_count; queue++) {
		struct dma_tx_queue *tx_q = &priv->tx_queue[queue];
		netdev_info(ndev, "TX Queue %d, dirty_tx %u, cur_tx %u:\n", queue, tx_q->dirty_tx, tx_q->cur_tx);
		sysfs_display_ring(ndev, tx_q->dma_tx, DMA_TX_SIZE, tx_q->dma_tx_phy);
	}

	return 0;
}
static DEVICE_ATTR_RO(dump_tx_desc);

static struct attribute *desc_attrs[] = {
	&dev_attr_dump_rx_desc.attr,
	&dev_attr_dump_tx_desc.attr,
	NULL,
};


static struct attribute_group desc_group = {
	.name = "descriptors",
	.attrs = desc_attrs,
};

/**
 * eth_drv_function_register
 * @priv: hobot private struct pointer
 * @ndev: net device struct pointer
 * Description: register eth driver function,
 * such as mdio bus, spin lock, phy config an ptp init, et al.
 * Return: 0 on success, otherwise error
 */
static s32 eth_drv_function_register(struct hobot_priv *priv, struct net_device *ndev)
{
	s32 ret;
	u32 queue;
	for (queue = 0; queue < priv->plat->rx_queues_to_use; queue++) {
		struct dma_rx_queue *rx_q = &priv->rx_queue[queue];
		netif_napi_add(ndev, &rx_q->napi, eth_napi_poll);
	}

	spin_lock_init(&priv->lock);/*PRQA S 3334*/

	ret = mdio_register(priv);
	if (ret < 0) {
		netdev_err(ndev, "MDIO bus error register\n");
		goto err_mdio_reg;
	}
	ret = eth_phy_setup(priv);
	if (ret) {
		netdev_err(ndev, "failed to setup phy (%d)\n", ret);
		goto error_phy_setup;
	}
	ndev->sysfs_groups[0] = &desc_group;

	ret = register_netdev(ndev);
	if (ret < 0) {
		netdev_err(ndev, "error register network device\n");
		goto err_netdev_reg;
	}


	ret = eth_init_ptp(priv);
	if (ret < 0) {
		netdev_err(ndev, "error register ptp clock\n");
		goto ptp_error;
	}

	setup_eth_stl_ops(priv);
	if (priv->hobot_eth_stl_ops && priv->hobot_eth_stl_ops->stl_init) {
		ret = priv->hobot_eth_stl_ops->stl_init(priv->ndev);
		if(ret < 0) {
			netdev_err(ndev, "error stl init\n");
			goto ptp_error;
		}
	}
	return 0;

ptp_error:
	unregister_netdev(ndev);
err_netdev_reg:
	phylink_destroy(priv->phylink);
error_phy_setup:
	mdiobus_unregister(priv->mii);
	priv->mii->priv = NULL;
	mdiobus_free(priv->mii);
	priv->mii = NULL;
err_mdio_reg:
	for (queue = 0; queue < priv->plat->rx_queues_to_use; queue++) {
		struct dma_rx_queue *rx_q = &priv->rx_queue[queue];
		netif_napi_del(&rx_q->napi);
	}

	return ret;


}



/**
 * eth_drv_probe
 * @device: device pointer
 * @plat_dat: platform data pointer
 * @res: stmmac resource pointer
 * Description: this is the main probe function used to
 * call the alloc_etherdev, allocate the priv structure.
 * Return:
 * returns 0 on success, otherwise errno.
 */
static s32 eth_drv_probe(struct device *device, struct plat_config_data *plat_dat, struct mac_resource *mac_res)
{
	struct hobot_priv *priv;
	u8 mac_addr[ETH_ALEN];
	s32 ret;

	struct net_device *ndev = alloc_etherdev_mqs(sizeof(struct hobot_priv), MTL_MAX_TX_QUEUES, MTL_MAX_RX_QUEUES);
	if (NULL == ndev)
		return -ENOMEM;

	SET_NETDEV_DEV(ndev, device);
	priv = (struct hobot_priv *)netdev_priv(ndev);
	priv->device = device;
	priv->ndev = ndev;
	ndev->ethtool_ops = &ethtool_ops;
	priv->plat = plat_dat;
	priv->ioaddr = mac_res->addr;
	priv->ndev->base_addr = (unsigned long)mac_res->addr;
	priv->ndev->irq = mac_res->irq;
	priv->wol_irq = mac_res->wol_irq;
	if (!IS_ERR_OR_NULL(mac_res->mac)) {
		eth_hw_addr_set(ndev,mac_res->mac);
	} else 	{
		get_random_bytes(mac_addr, ETH_ALEN);
		mac_addr[0] &= ~SZ_1;		/* clear multicast bit */ /*PRQA S 1851, 4434, 4532*/
		mac_addr[0] |= SZ_2;		/* set local assignment bit (IEEE802) */ /*PRQA S 1861*/
		eth_hw_addr_set(ndev, mac_addr);

		netdev_info(ndev, "(using random mac adress)\n");
	}

	netdev_info(ndev, "devicec MAC addr %pM\n",
			ndev->dev_addr);

	set_umac_addr(priv->ioaddr, ndev->dev_addr, 0);

	dev_set_drvdata(device, (void *)priv->ndev);


	ret = eth_cfg_chcek(priv);
	if (ret < 0)
		goto err_netdev_init;

	(void)netif_set_real_num_rx_queues(ndev, priv->plat->rx_queues_to_use);
	(void)netif_set_real_num_tx_queues(ndev, priv->plat->tx_queues_to_use);

	ndev->netdev_ops = &netdev_ops;

	ndev->hw_features = NETIF_F_SG;

	if((priv->plat->tso_en) && (priv->dma_cap.tsoen)) {
		ndev->hw_features |= NETIF_F_TSO | NETIF_F_TSO6;
		priv->tso = (bool)true;
		netdev_dbg(ndev, "TSO feature enabled\n");
	}
	if (0U != priv->dma_cap.tx_coe) {
		ndev->hw_features |= NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM;
		netdev_dbg(ndev, "TX Checksum insertion supported\n");
	}

	if (0U != priv->dma_cap.rx_coe) {
		ndev->hw_features |= NETIF_F_RXCSUM;
		priv->rx_csum = priv->plat->rx_coe;
		netdev_dbg(ndev, "RX Checksum Offload Engine supported\n");
	}

	ndev->hw_features |= NETIF_F_RXFCS;

	ndev->features = ndev->hw_features | NETIF_F_HIGHDMA;

	ndev->vlan_features = ndev->features;

	ndev->watchdog_timeo = DWCEQOS_TX_TIMEOUT * HZ;

	ret = mdio_set_csr(priv);
	if (ret < 0) {
		netdev_err(ndev, "mac clk is invalid for mdio clk config\n");
		goto err_netdev_init;
	}

	ndev->min_mtu = ETH_ZLEN - ETH_HLEN;
	ndev->max_mtu = JUMBO_LEN;

	if (((u32)priv->plat->maxmtu < ndev->max_mtu) && ((u32)priv->plat->maxmtu >= ndev->min_mtu))
		ndev->max_mtu = (u32)priv->plat->maxmtu;
	else if ((u32)priv->plat->maxmtu < ndev->min_mtu)
		netdev_err(ndev, "waring: maxmtu having invalid value by Network\n");

	/* Allocate workqueue */
	priv->wq = create_singlethread_workqueue("hobot_wq");
	if (!priv->wq) {
		dev_err(priv->device, "failed to create workqueue\n");
		ret = -ENOMEM;
		goto err_netdev_init;
	}
	INIT_WORK(&priv->service_task, hobot_service_task);

	ret = eth_drv_function_register(priv, ndev);
	if (ret < 0) {
		netdev_err(ndev, "error register eth drv function\n");
		goto err_wq_init;
	}

	pndev = ndev;

	return 0;


err_wq_init:
	destroy_workqueue(priv->wq);
err_netdev_init:
	free_netdev(ndev);
	return ret;
}


/**
 * eth_reset
 * @dev: device struct pointer
 * Description: reset eth driver
 * Return: 0 on success, otherwise error
 */
static s32 eth_reset(struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct reset_control * reset_control;
	s32 err;

	reset_control = of_reset_control_array_get_exclusive(np);
	if (IS_ERR((void *)reset_control)) {
		dev_dbg(dev, "get reset control err\n");/*PRQA S 0685, 1294*/
		err = PTR_ERR((void *)reset_control);/*PRQA S 4460*/
		goto err_get_reset;
	}

	err = reset_control_assert(reset_control);
	if (err < 0) {
		dev_dbg(dev, "eth reset assert err\n");/*PRQA S 0685, 1294*/
		goto free_control;
	}
	udelay(100);/*PRQA S 2880*/
	err = reset_control_deassert(reset_control);
	if (err < 0) {
		dev_dbg(dev, "eth reset assert err\n");/*PRQA S 0685, 1294*/
		goto free_control;
	}
free_control:
	reset_control_put(reset_control);
err_get_reset:
	return err;
}




/**
 * eth_probe
 * @pdev: platform_device pointer
 * Description: main probe function for eth driver, used to
 * call the alloc_etherdev, allocate the priv structure.
 * Return: 0 on success, otherwise error
 */
static s32 eth_probe(struct platform_device *pdev)
{
	struct plat_config_data *plat_dat;
	struct mac_resource mac_res;
	s32 ret;
	if (NULL == pdev) {
		(void)pr_err("%s, platform dev is null\n", __func__);
		return -EINVAL;
	}
	(void)hobot_eth_start_probe(pdev);

	(void)memset((void *)&mac_res, 0, sizeof(struct mac_resource));
	ret = get_platform_resources(pdev, &mac_res);
	if(ret < 0) {
		dev_dbg(&pdev->dev, "get platform resources err\n"); /*PRQA S 1294, 0685*/
		goto err_dt;
	}
	plat_dat = eth_probe_config_dt(pdev, &mac_res.mac);
	if (IS_ERR((void *)plat_dat)) {
		ret = PTR_ERR((void *)plat_dat);/*PRQA S 4460*/
		goto err_dt;
	}

	ret = eth_reset(&pdev->dev);
	if (ret < 0)
		goto err_reset;

	ret = eth_drv_probe(&pdev->dev, plat_dat, &mac_res);
	if (ret < 0)
		goto err_probe;
	(void)hobot_eth_end_probe(pdev);

	return 0;

err_probe:
	devm_kfree(&pdev->dev, (void *)plat_dat->axi);
	plat_dat->axi = NULL;
	devm_kfree(&pdev->dev, (void *)plat_dat->dma_cfg);
	plat_dat->dma_cfg = NULL;
	devm_kfree(&pdev->dev, (void *)plat_dat);
	plat_dat = NULL;
err_reset:
	devm_iounmap(&pdev->dev, mac_res.addr);
err_dt:
	return ret;
}


/**
 * eth_remove
 * @pdev: platform_device pointer
 * Description: remove eth drive function, used to
 * free the etherdev, the priv structure.
 * Return: 0 on success, otherwise error
 */
static s32 eth_remove(struct platform_device *pdev)
{
	struct net_device *ndev;
	struct hobot_priv *priv;
	if (NULL == pdev) {
		(void)pr_err("%s, platform dev is null\n", __func__);
		return -EINVAL;
	}
	ndev = (struct net_device *)platform_get_drvdata(pdev);
	priv = (struct hobot_priv *)netdev_priv(ndev);
	stop_all_dma(priv);
	enable_mac_transmit(priv, (bool)false);
	netif_carrier_off(ndev);
	unregister_netdev(ndev);
	phylink_destroy(priv->phylink);
	clk_disable_unprepare(priv->plat->eth_apb_clk);
	clk_disable_unprepare(priv->plat->eth_bus_clk);
	clk_disable_unprepare(priv->plat->eth_mac_clk);
	clk_disable_unprepare(priv->plat->clk_ptp_ref);
	clk_disable_unprepare(priv->plat->phy_ref_clk);

	if (NULL != priv->mii) {
		mdiobus_unregister(priv->mii);
		priv->mii->priv = NULL;
		mdiobus_free(priv->mii);
		priv->mii = NULL;
	}
	destroy_workqueue(priv->wq);
	devm_iounmap(&pdev->dev, priv->ioaddr);
	devm_kfree(&pdev->dev,(void *)priv->plat->axi);
	priv->plat->axi = NULL;
	of_node_put(priv->plat->phy_node);
	of_node_put(priv->plat->mdio_node);
	devm_kfree(&pdev->dev, (void *)priv->plat->dma_cfg);
	priv->plat->dma_cfg = NULL;
	devm_kfree(&pdev->dev, (void *)priv->plat);
	priv->plat = NULL;
	free_netdev(ndev);
	ndev = NULL;
	return 0;
}



 /**
 * mac_pmt
 * @ioaddr: regaddr base
 * @mode: mode
 * Description: this is the function to program the PMT register (for WoL).
 * Return: NA.
 */
static void mac_pmt(void __iomem *ioaddr, unsigned long mode)
{
	u32 pmt = 0;
	u32 config;

	if (mode & WAKE_MAGIC) {
		pr_debug("GMAC: WOL Magic frame\n"); /*PRQA S 1294, 0685*/
		pmt |= (u32)power_down | (u32)magic_pkt_en;
	}
	if (mode & WAKE_UCAST) {
		pr_debug("GMAC: WOL on global unicast\n"); /*PRQA S 1294, 0685*/
		pmt |= (u32)power_down | (u32)global_unicast | (u32)wake_up_frame_en;
	}

	if (0U != pmt) {
		/* The receiver must be enabled for WOL before powering down */
		config = ioreadl(ioaddr, GMAC_CONFIG);
		config |= (u32)GMAC_CONFIG_RE;
		iowritel(config, ioaddr, GMAC_CONFIG);
	}
	iowritel(pmt, ioaddr, GMAC_PMT);
}

/**
 * eth_suspend - suspend callback
 * @dev: device pointer
 * Description: this is the function to suspend the device and it is called
 * by the platform driver to stop the network queue, release the resources,
 * program the PMT register (for WoL), clean and release driver resources.
 */
s32 eth_suspend(struct device *dev)
{
	struct net_device *ndev;
	struct hobot_priv *priv;
	u32 rx_cnt;
	u32 queue;

	if (NULL == dev) {
		(void)pr_err("%s, dev is null\n", __func__);
		return -EINVAL;
	}
	ndev = (struct net_device *)dev_get_drvdata(dev);
	priv = (struct hobot_priv *)netdev_priv(ndev);
	rx_cnt = priv->plat->rx_queues_to_use;
	if (!netif_running(ndev))
		return 0;

	if (NULL != priv->phylink) {
		rtnl_lock();
		phylink_stop(priv->phylink);
		rtnl_unlock();
	}

	priv->suspend_boot_time = ktime_get_boottime();
	priv->suspend_ptp_time = ptp_get_systime(priv);

	netif_device_detach(ndev);
	netif_carrier_off(ndev);
	for (queue = 0; queue < rx_cnt; queue++) {
		struct dma_rx_queue *rx_q = &priv->rx_queue[queue];
		napi_synchronize(&rx_q->napi);
	}
	stop_all_queues(priv);

	disable_all_napi(priv);
	del_timer_sync(&priv->txtimer);

	/* Stop TX/RX DMA */
	stop_all_dma(priv);

	/* Enable Power down mode by programming the PMT regs */
	if (device_may_wakeup(priv->device) && priv->wol_irq > 0) {
		mac_pmt(priv->ioaddr, (u32)priv->wolopts);
		(void)enable_irq_wake((u32)priv->wol_irq);
	} else {
		enable_mac_transmit(priv, (bool)false);
	}

	clk_disable_unprepare(priv->plat->eth_apb_clk);
	clk_disable_unprepare(priv->plat->eth_bus_clk);
	clk_disable_unprepare(priv->plat->eth_mac_clk);
	clk_disable_unprepare(priv->plat->clk_ptp_ref);
	clk_disable_unprepare(priv->plat->phy_ref_clk);

	return 0;
}


/**
 * eth_resume - resume callback
 * @dev: device pointer
 * Description: when resume this function is invoked to setup the DMA and CORE
 * in a usable state.
 */
s32 eth_resume(struct device *dev)
{
	struct net_device *ndev;
	struct hobot_priv *priv;
	unsigned long flags;
	u32 tx_cnt;
	u32 queue;
	s32 ret;
	ktime_t resume_boot_time;
	struct timespec64 resume_ptp_time;
	ktime_t tmp;

	if (NULL == dev) {
		(void)pr_err("%s, dev is null\n", __func__);
		return -EINVAL;
	}
	ndev = (struct net_device *)dev_get_drvdata(dev);
	priv = (struct hobot_priv *)netdev_priv(ndev);
	tx_cnt = priv->plat->tx_queues_to_use;
	if (!netif_running(ndev))
		return 0;
	hobot_eth_start_resume(dev);

	(void)clk_prepare_enable(priv->plat->eth_apb_clk);
	(void)clk_prepare_enable(priv->plat->eth_bus_clk);
	(void)clk_prepare_enable(priv->plat->eth_mac_clk);
	(void)clk_prepare_enable(priv->plat->clk_ptp_ref);
	(void)clk_prepare_enable(priv->plat->phy_ref_clk);

	(void)eth_reset(dev);

	/* Power Down bit, into the PM register, is cleared
	 * automatically as soon as a magic packet or a Wake-up frame
	 * is received. Anyway, it's better to manually clear
	 * this bit because it can generate problems while resuming
	 * from another devices (e.g. serial console).
	 */
	if (device_may_wakeup(priv->device) && priv->wol_irq > 0) {
		spin_lock_irqsave(&priv->lock, flags); /*PRQA S 2996*/
		mac_pmt(priv->ioaddr, 0);
		spin_unlock_irqrestore(&priv->lock, flags);/*PRQA S 2996*/
		(void)disable_irq_wake((u32)priv->wol_irq);
	}

	netif_device_attach(ndev);
	reset_queues_param(priv);

	/* reset private mss value to force mss context settings at
	 * next tso xmit (only used for gmac4).
	 */
	for (queue = 0; queue < tx_cnt; queue++) {
		priv->tx_queue[queue].mss = 0;
		dma_free_tx_skbufs(priv, queue);
		netdev_tx_reset_queue(netdev_get_tx_queue(priv->ndev, queue));
	}

	clear_dma_descriptors(priv);
	(void)hw_setup(ndev);
	hobot_init_timer(priv);
	eth_netdev_set_rx_mode(ndev);
	enable_all_napi(priv);
	start_all_queues(priv);

	resume_boot_time = ktime_get_boottime();
	tmp = priv->suspend_ptp_time +
		(resume_boot_time - priv->suspend_boot_time);
	resume_ptp_time = ktime_to_timespec64(tmp);

	ret = hw_timestamp_init(priv, &resume_ptp_time);
	if (ret < 0) {
		netdev_err(ndev, "%s, hw timestamp init failed\n", __func__);
		return ret;
	}

	if (priv->hobot_eth_stl_ops && priv->hobot_eth_stl_ops->stl_resume) {
		priv->hobot_eth_stl_ops->stl_resume(priv->ndev);
	}

	if (NULL != priv->phylink)
	{
		rtnl_lock();
		phylink_start(priv->phylink);
		rtnl_unlock();
	}
	hobot_eth_end_sesume(dev);

	return 0;
}
const struct dev_pm_ops hobot_pltfr_pm_ops;
SIMPLE_DEV_PM_OPS(hobot_pltfr_pm_ops, eth_suspend, eth_resume);

static const struct of_device_id hobot_of_match[] = {
	{ .compatible = "snps,dwc-qos-ethernet-5.10a", },
	{},
};

MODULE_DEVICE_TABLE(of, hobot_of_match); /*PRQA S 0605*/

static struct platform_driver hobot_driver = {
	.probe = eth_probe,
	.remove = eth_remove,
	.driver = {
		.name = DRIVER_NAME,
		.pm  = &hobot_pltfr_pm_ops,
		.of_match_table = hobot_of_match,
	},

};

module_platform_driver(hobot_driver);/*PRQA S 0605, 0307*/
MODULE_LICENSE("GPL v2");

