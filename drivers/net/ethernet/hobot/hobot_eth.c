/*
 *  Copyright (C) 2020 Hobot Robotics.
 *
 * This code from  Synopsys DWC Ethernet Quality-of-Service v4.10a linux driver
 *
 *  This is a driver for the Synopsys DWC Ethernet QoS IP version 4.10a (GMAC).
 *  This version introduced a lot of changes which breaks backwards
 *  compatibility the non-QoS IP from Synopsys (used in the ST Micro drivers).
 *  Some fields differ between version 4.00a and 4.10a, mainly the interrupt
 *  bit fields. The driver could be made compatible with 4.00, if all relevant
 *  HW erratas are handled.
 *
 *  The GMAC is highly configurable at synthesis time. This driver has been
 *  developed for a subset of the total available feature set. Currently
 *  it supports:
 *  - TSO
 *  - Checksum offload for RX and TX.
 *  - RGMII phy interface.
 *  - The statistics module.
 *  - Single RX and TX queue.
 *
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms and conditions of the GNU General Public License,
 *  version 2, as published by the Free Software Foundation.
 */

#include <linux/clk.h>
#include <linux/ethtool.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/stat.h>
#include <linux/types.h>

#include <linux/delay.h>
#include <linux/etherdevice.h>
#include <linux/mm.h>
#include <linux/netdevice.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include <linux/dma-mapping.h>
#include <linux/mii.h>
#include <linux/phy.h>
#include <linux/vmalloc.h>

#include <linux/bitrev.h>
#include <linux/crc32.h>
#include <linux/device.h>

#include <linux/clocksource.h>
#include <linux/if_vlan.h>
#include <linux/interrupt.h>
#include <linux/ip.h>
#include <linux/net_tstamp.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_mdio.h>
#include <linux/of_net.h>
#include <linux/pm_runtime.h>
#include <linux/sysfs.h>
#include <linux/tcp.h>
#include <linux/timer.h>
#ifdef CONFIG_HOBOT_DIAG
#include <soc/hobot/diag.h>
#endif
#include <uapi/linux/if_arp.h>
#include <linux/gpio/consumer.h>
#include <linux/reset.h>

#include "hobot_eth.h"
#include "hobot_reg.h"
#include "hobot_tsn.h"

#define TSO_MAX_BUFF_SIZE (SZ_16K - 1)

#define DRIVER_NAME "st_gmac"

#define HOBOT_COAL_TIMER(x) (jiffies + usecs_to_jiffies(x))

#define xj3_reg_read(priv, reg) \
    readl_relaxed(((void __iomem *)((priv)->ioaddr)) + (reg))

#define xj3_reg_write(priv, reg, val) \
    writel_relaxed((val), ((void __iomem *)((priv)->ioaddr)) + (reg))

//#define HOBOT_USE_IRQ_SPLIT 1

static int eth_err_flag;
static bool tsn_enabled = true;

static void xj3_refill_work(struct work_struct *work);

static int xj3_mdio_read(struct mii_bus *bus, int mii_id, int phyreg) {
    struct net_device *ndev = bus->priv;
    struct xj3_priv *priv = netdev_priv(ndev);
    u32 regval;
    int i;
    int data;

    dev_dbg(priv->device, "%s, reg:0x%x\n", __func__, phyreg);
    regval = DWCEQOS_MDIO_PHYADDR(mii_id) | DWCEQOS_MDIO_PHYREG(phyreg) |
             DWCEQOS_MAC_MDIO_ADDR_CR(priv->csr_val) |
             DWCEQOS_MAC_MDIO_ADDR_GB | DWCEQOS_MAC_MDIO_ADDR_GOC_READ;
    xj3_reg_write(priv, REG_DWCEQOS_MAC_MDIO_ADDR, regval);

    for (i = 0; i < 5; ++i) {
        usleep_range(64, 128);
        if (!(xj3_reg_read(priv, REG_DWCEQOS_MAC_MDIO_ADDR) &
              DWCEQOS_MAC_MDIO_ADDR_GB))
            break;
    }

    data = xj3_reg_read(priv, REG_DWCEQOS_MAC_MDIO_DATA);
    if (i == 5) {
        netdev_warn(ndev, "MDIO read timed out\n");
        data = 0xffff;
    }

    dev_dbg(priv->device, "%s, reg:0x%x, data:0x%x\n", __func__, phyreg,
            data & 0xffff);
    return data & 0xffff;
}

static int xj3_mdio_write(struct mii_bus *bus, int mii_id, int phyreg,
                          u16 value) {
    struct net_device *ndev = bus->priv;
    struct xj3_priv *priv = netdev_priv(ndev);
    u32 regval;
    int i;

    xj3_reg_write(priv, REG_DWCEQOS_MAC_MDIO_DATA, value);

    regval = DWCEQOS_MDIO_PHYADDR(mii_id) | DWCEQOS_MDIO_PHYREG(phyreg) |
             DWCEQOS_MAC_MDIO_ADDR_CR(priv->csr_val) |
             DWCEQOS_MAC_MDIO_ADDR_GB | DWCEQOS_MAC_MDIO_ADDR_GOC_WRITE;
    xj3_reg_write(priv, REG_DWCEQOS_MAC_MDIO_ADDR, regval);

    for (i = 0; i < 5; ++i) {
        usleep_range(64, 128);
        if (!(xj3_reg_read(priv, REG_DWCEQOS_MAC_MDIO_ADDR) &
              DWCEQOS_MAC_MDIO_ADDR_GB))
            break;
    }
    if (i == 5) netdev_warn(priv->dev, "MDIO write timed out\n");
    return 0;
}
static int xj3_dt_phy(struct plat_config_data *plat, struct device_node *np,
                      struct device *dev) {
    bool mdio = true;
    static const struct of_device_id need_mdio_ids[] = {
        {.compatible = "snps,dwc-qos-ethernet-4.10a"},
        {},
    };

    plat->phy_node = of_parse_phandle(np, "phy-handle", 0);
    if (plat->phy_node) dev_info(dev, "found phy-handle subnode\n");

    dev_dbg(dev, "%s, plat->phy_node: %p\n", __func__, plat->phy_node);

    if (!plat->phy_node && of_phy_is_fixed_link(np)) {
        if ((of_phy_register_fixed_link(np) < 0)) return -ENODEV;

        dev_dbg(dev, "Found fixed-link subnode\n");
        plat->phy_node = of_node_get(np);
        mdio = false;
    }

    if (of_match_node(need_mdio_ids, np)) {
        plat->mdio_node = of_get_child_by_name(np, "mdio");
    } else {
        for_each_child_of_node(np, plat->mdio_node) {
            if (of_device_is_compatible(plat->mdio_node, "snps,dwmac-mdio"))
                break;
        }
    }

    if (plat->mdio_node) {
        mdio = true;
        plat->phy_reset_extra_us = 0;
        of_property_read_u32(plat->mdio_node, "reset_extra_us",
                             &plat->phy_reset_extra_us);
    }
    if (mdio)
        plat->mdio_bus_data =
            devm_kzalloc(dev, sizeof(struct xj3_mdio_bus_data), GFP_KERNEL);

    return 0;
}

static void xj3_mtl_setup(struct platform_device *pdev,
                          struct plat_config_data *plat) {
    struct device_node *q_node;
    struct device_node *rx_node;
    struct device_node *tx_node;
    u8 queue = 0;

    plat->rx_queues_to_use = 1;
    plat->tx_queues_to_use = 1;

    plat->rx_queues_cfg[0].mode_to_use = MTL_QUEUE_DCB;
    plat->tx_queues_cfg[0].mode_to_use = MTL_QUEUE_DCB;

    rx_node = of_get_child_by_name(pdev->dev.of_node, "snps,mtl-rx-config");
    if (!rx_node) {
        dev_info(&pdev->dev, "%s, snps,mtl-rx-config rx-node is NULL\n",
                 __func__);
        return;
    }

    tx_node = of_get_child_by_name(pdev->dev.of_node, "snps,mtl-tx-config");
    if (!tx_node) return;

    if (of_property_read_u32(rx_node, "snps,rx-queues-to-use",
                             &plat->rx_queues_to_use))
        plat->rx_queues_to_use = 1;

    if (of_property_read_u32(tx_node, "snps,tx-queues-to-use",
                             &plat->tx_queues_to_use))
        plat->tx_queues_to_use = 1;

    dev_info(&pdev->dev, "%s, plat->rx-queues-to-use:%d, and tx:%d\n", __func__,
             plat->rx_queues_to_use, plat->tx_queues_to_use);

    if (of_property_read_bool(rx_node, "snps,rx-sched-sp"))
        plat->rx_sched_algorithm = MTL_RX_ALGORITHM_SP;
    else if (of_property_read_bool(rx_node, "snps,rx-sched-wsp"))
        plat->rx_sched_algorithm = MTL_RX_ALGORITHM_WSP;
    else
        plat->rx_sched_algorithm = MTL_RX_ALGORITHM_SP;

    for_each_child_of_node(rx_node, q_node) {
        if (queue >= plat->rx_queues_to_use) break;

        if (of_property_read_bool(q_node, "snps,dcb-algorithm"))
            plat->rx_queues_cfg[queue].mode_to_use = MTL_QUEUE_DCB;
        else if (of_property_read_bool(q_node, "snps,avb-algorithm"))
            plat->rx_queues_cfg[queue].mode_to_use = MTL_QUEUE_AVB;
        else
            plat->rx_queues_cfg[queue].mode_to_use = MTL_QUEUE_DCB;

        if (of_property_read_u32(q_node, "snps,map-to-dma-channel",
                                 &plat->rx_queues_cfg[queue].chan))
            plat->rx_queues_cfg[queue].chan = queue;

        if (of_property_read_u32(q_node, "snps,priority",
                                 &plat->rx_queues_cfg[queue].prio)) {
            plat->rx_queues_cfg[queue].prio = 0;
            plat->rx_queues_cfg[queue].use_prio = false;
        } else {
            plat->rx_queues_cfg[queue].use_prio = true;
        }

        if (of_property_read_bool(q_node, "snps,route-avcp"))
            plat->rx_queues_cfg[queue].pkt_route = PACKET_AVCPQ;
        else if (of_property_read_bool(q_node, "snps,route-ptp"))
            plat->rx_queues_cfg[queue].pkt_route = PACKET_PTPQ;
        else if (of_property_read_bool(q_node, "snps,route-dcbcp"))
            plat->rx_queues_cfg[queue].pkt_route = PACKET_DCBCPQ;
        else
            plat->rx_queues_cfg[queue].pkt_route = 0x0;

        queue++;
    }

    if (of_property_read_u32(tx_node, "snps,tx-queues-to-use",
                             &plat->tx_queues_to_use))
        plat->tx_queues_to_use = 1;

    if (of_property_read_bool(tx_node, "snps,tx-sched-wrr"))
        plat->tx_sched_algorithm = MTL_TX_ALGORITHM_WRR;
    else if (of_property_read_bool(tx_node, "snps,tx-sched-sp"))
        plat->tx_sched_algorithm = MTL_TX_ALGORITHM_SP;
    else
        plat->tx_sched_algorithm = 0x0;

    queue = 0;

    for_each_child_of_node(tx_node, q_node) {
        if (queue >= plat->tx_queues_to_use) break;

        if (of_property_read_u32(q_node, "snps,weigh",
                                 &plat->tx_queues_cfg[queue].weight))
            plat->tx_queues_cfg[queue].weight = 0x10 + queue;

        if (of_property_read_bool(q_node, "snps,dcb-algorithm")) {
            plat->tx_queues_cfg[queue].mode_to_use = MTL_QUEUE_DCB;
        } else if (of_property_read_bool(q_node, "snps,avb-algorithm")) {
            plat->tx_queues_cfg[queue].mode_to_use = MTL_QUEUE_AVB;

            if (of_property_read_u32(q_node, "snps,send_slope",
                                     &plat->tx_queues_cfg[queue].send_slope))
                plat->tx_queues_cfg[queue].send_slope = 0x0;

            if (of_property_read_u32(q_node, "snps,idle_slope",
                                     &plat->tx_queues_cfg[queue].idle_slope))
                plat->tx_queues_cfg[queue].idle_slope = 0x0;

            if (of_property_read_u32(q_node, "snps,high_credit",
                                     &plat->tx_queues_cfg[queue].high_credit))
                plat->tx_queues_cfg[queue].high_credit = 0x0;

            if (of_property_read_u32(q_node, "snps,low_credit",
                                     &plat->tx_queues_cfg[queue].low_credit))
                plat->tx_queues_cfg[queue].low_credit = 0x0;

        } else {
            plat->tx_queues_cfg[queue].mode_to_use = MTL_QUEUE_DCB;
        }

        if (of_property_read_u32(q_node, "snps,priority",
                                 &plat->tx_queues_cfg[queue].prio)) {
            plat->tx_queues_cfg[queue].prio = 0;
            plat->tx_queues_cfg[queue].use_prio = false;
        } else {
            plat->tx_queues_cfg[queue].use_prio = true;
        }

        queue++;
    }
    of_node_put(rx_node);
    of_node_put(tx_node);
    of_node_put(q_node);
}

static struct xj3_axi *xj3_axi_setup(struct platform_device *pdev) {
    struct device_node *np;
    struct xj3_axi *axi;

    np = of_parse_phandle(pdev->dev.of_node, "snps,axi-config", 0);
    if (!np) return NULL;

    axi = devm_kzalloc(&pdev->dev, sizeof(*axi), GFP_KERNEL);
    if (!axi) {
        of_node_put(np);
        return ERR_PTR(-ENOMEM);
    }

    axi->axi_lpi_en = of_property_read_bool(np, "snps,lpi_en");
    axi->axi_xit_frm = of_property_read_bool(np, "snps,xit_frm");
    axi->axi_kbbe = of_property_read_bool(np, "snps,axi_kbbe");
    axi->axi_fb = of_property_read_bool(np, "snps,axi_fb");
    axi->axi_mb = of_property_read_bool(np, "snps,axi_mb");
    axi->axi_rb = of_property_read_bool(np, "snps,axi_rb");

    if (of_property_read_u32(np, "snps,wr_osr_lmt", &axi->axi_wr_osr_lmt))
        axi->axi_wr_osr_lmt = 1;

    if (of_property_read_u32(np, "snps,rd_osr_lmt", &axi->axi_rd_osr_lmt))
        axi->axi_rd_osr_lmt = 1;

    of_property_read_u32_array(np, "snps,blen", axi->axi_blen, AXI_BLEN);

    of_node_put(np);
    return axi;
}

struct plat_config_data *xj3_probe_config_dt(struct platform_device *pdev,
                                             const char **mac) {
    struct device_node *np = pdev->dev.of_node;
    struct plat_config_data *plat;
    struct xj3_dma_cfg *dma_cfg;
    int ret;
    struct resource *res;
    u32 val;
    plat = devm_kzalloc(&pdev->dev, sizeof(*plat), GFP_KERNEL);
    if (!plat) return ERR_PTR(-ENOMEM);

    res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
    if (!res) {
        dev_err(&pdev->dev, "%s, get plat resouce failed\n", __func__);
        goto err_out;
    }
    plat->sub_addr = devm_ioremap(&pdev->dev, res->start, resource_size(res));
    if (IS_ERR(plat->sub_addr)) {
        dev_err(&pdev->dev, "%s, error ioremap\n", __func__);
        goto err_out;
    }

    ret = of_get_mac_address(np, (u8 *)*mac);
    if (!ret) {
        dev_err(&pdev->dev, "missing get mac address\n");
		goto err_out;
    }

    ret = of_get_phy_mode(np,  &plat->interface);
    if (ret) {
        dev_err(&pdev->dev, "missing required property 'phy-mode'\n");
		goto err_out;
    }

    if (plat->interface == PHY_INTERFACE_MODE_RMII) {
        // set enet rmii and clk turn on
        val = ioread32(plat->sub_addr);
        val &= 0xFFFFFFF0;
        val |= 0xC;
        iowrite32(val, plat->sub_addr);
    } else {
        // set enet rgmii and clk turn on
        val = ioread32(plat->sub_addr);
        val &= 0xFFFFFFF0;
        val |= 0x9;
        iowrite32(val, plat->sub_addr);
    }

    if (of_property_read_u32(np, "max-speed", &plat->max_speed))
        plat->max_speed = -1;

    plat->bus_id = of_alias_get_id(np, "ethernet");
    if (plat->bus_id < 0) plat->bus_id = 0;

    plat->phy_addr = -1;

    if (of_property_read_u32(np, "snps,phy-addr", &plat->phy_addr) == 0)
        dev_info(&pdev->dev, "snps, phy-addr property is deprecated\n");

    if (xj3_dt_phy(plat, np, &pdev->dev)) return ERR_PTR(-ENODEV);

    /*plat->pinctrl = devm_pinctrl_get(&pdev->dev);
    if (IS_ERR(plat->pinctrl)) {
        dev_warn(&pdev->dev, "pinctrl get error\n");
        //return ERR_PTR(-EIO);
    } else {
        plat->pin_eth_mux = pinctrl_lookup_state(plat->pinctrl, "eth_state");
        if (IS_ERR(plat->pin_eth_mux)) {
            dev_err(&pdev->dev, "pins_eth_mux in pinctrl state error\n");
            return ERR_PTR(-EIO);
        }

        pinctrl_select_state(plat->pinctrl, plat->pin_eth_mux);
    }*/

    of_property_read_u32(np, "tx-fifo-depth", &plat->tx_fifo_size);
    of_property_read_u32(np, "rx-fifo-depth", &plat->rx_fifo_size);

    plat->force_sf_dma_mode =
        of_property_read_bool(np, "snps,force_sf_dma_mode");
    plat->en_tx_lpi_clockgating =
        of_property_read_bool(np, "snps,en-tx-lpi-clockgating");

    plat->maxmtu = JUMBO_LEN;

    plat->has_gmac4 = 1;
    plat->pmt = 1;
    plat->tso_en = of_property_read_bool(np, "snps,tso");

    dma_cfg = devm_kzalloc(&pdev->dev, sizeof(*dma_cfg), GFP_KERNEL);
    if (!dma_cfg) return ERR_PTR(-ENOMEM);
    plat->dma_cfg = dma_cfg;

    of_property_read_u32(np, "snps,pbl", &dma_cfg->pbl);
    if (!dma_cfg->pbl) dma_cfg->pbl = DEFAULT_DMA_PBL;

    of_property_read_u32(np, "snps,txpbl", &dma_cfg->txpbl);
    of_property_read_u32(np, "snps,rxpbl", &dma_cfg->rxpbl);
    dma_cfg->pblx8 = !of_property_read_bool(np, "snps,no-plb-x8");
    dma_cfg->aal = of_property_read_bool(np, "snps,aal");
    dma_cfg->fixed_burst = of_property_read_bool(np, "snps,fixed-burst");
    dma_cfg->mixed_burst = of_property_read_bool(np, "snps,mixed-burst");

    plat->force_thresh_dma_mode =
        of_property_read_bool(np, "snps,force_thresh_dma_mode");

    if (plat->force_thresh_dma_mode) {
        plat->force_sf_dma_mode = 0;
    }

    of_property_read_u32(np, "snps,ps-speed", &plat->mac_port_sel_speed);

    plat->axi = xj3_axi_setup(pdev);

    xj3_mtl_setup(pdev, plat);
    plat->xj3_mac_pre_div_clk = devm_clk_get(&pdev->dev, "eth0_pre_clk");
    if (IS_ERR(plat->xj3_mac_pre_div_clk)) {
        dev_info(&pdev->dev, "mac pre div clk clock not found\n");
        goto err_out;
    }
    ret = clk_prepare_enable(plat->xj3_mac_pre_div_clk);
    if (ret) {
        dev_info(&pdev->dev, "unable to enable mac pre div clk\n");
        goto err_out;
    }

    plat->xj3_mac_div_clk = devm_clk_get(&pdev->dev, "eth0_clk");
    if (IS_ERR(plat->xj3_mac_div_clk)) {
        dev_info(&pdev->dev, "mac div clk not found\n");
        goto err_mac_div_clk;
    }

    ret = clk_prepare_enable(plat->xj3_mac_div_clk);
    if (ret) {
        dev_info(&pdev->dev, "unable to enable mac div clk\n");
        goto err_mac_div_clk;
    }

    plat->xj3_phy_ref_clk = devm_clk_get(&pdev->dev, "eth0_ref");
    if (IS_ERR(plat->xj3_phy_ref_clk)) {
        dev_info(&pdev->dev, "eth0_ref clk not found\n");
        goto err_eth0_ref_clk;
    }

    ret = clk_prepare_enable(plat->xj3_phy_ref_clk);
    if (ret) {
        dev_info(&pdev->dev, "unable to enable eth0_ref clk\n");
        goto err_eth0_ref_clk;
    }

    plat->xj3_pclk = devm_clk_get(&pdev->dev, "eth0_pclk");
    if (IS_ERR(plat->xj3_pclk)) {
        dev_info(&pdev->dev, "eth0_pclk clk not found\n");
        goto err_eth0_rgmii_clk;
    }

    ret = clk_prepare_enable(plat->xj3_pclk);
    if (ret) {
        dev_info(&pdev->dev, "unable to enable eth0_rgmii clk\n");
        goto err_eth0_rgmii_clk;
    }

    plat->clk_ptp_ref = devm_clk_get(&pdev->dev, "sys_div_pclk");
    if (IS_ERR(plat->clk_ptp_ref)) {
        dev_info(&pdev->dev, "ethernet: ptp cloock not found\n");
        goto err_ptp_ref;
    }
    ret = clk_prepare_enable(plat->clk_ptp_ref);
    if (ret) {
        dev_info(&pdev->dev, "unable to enable ptp clk\n");
        goto err_ptp_ref;
    }

    plat->clk_ptp_rate = clk_get_rate(plat->clk_ptp_ref);
    dev_info(&pdev->dev, "%s, clk_ptp_rate:%d\n", __func__, plat->clk_ptp_rate);
    plat->cdc_delay = 2 * ((1000000000ULL) / plat->clk_ptp_rate);

	plat->phyreset = devm_gpiod_get_optional(&pdev->dev, "phyreset", GPIOD_OUT_HIGH);
	if (!IS_ERR_OR_NULL(plat->phyreset)) {
		gpiod_set_value(plat->phyreset, 0);
		msleep(20);
		gpiod_set_value(plat->phyreset, 1);
		msleep(100);
	}

    plat->rst = devm_reset_control_get_optional(&pdev->dev, "enet_rst");
	if (!IS_ERR(plat->rst)) {
        reset_control_assert(plat->rst);
        udelay(1);
        reset_control_deassert(plat->rst);
	}

    return plat;
err_ptp_ref:
    clk_disable_unprepare(plat->xj3_mac_div_clk);

err_eth0_rgmii_clk:
    clk_disable_unprepare(plat->xj3_pclk);
err_eth0_ref_clk:
    clk_disable_unprepare(plat->xj3_phy_ref_clk);
err_mac_div_clk:

    clk_disable_unprepare(plat->xj3_mac_pre_div_clk);

err_out:
    return ERR_PTR(-EPROBE_DEFER);
}

static int xj3_get_hw_features(void __iomem *ioaddr,
                               struct dma_features *dma_cap) {
    struct xj3_priv *priv = container_of(ioaddr, struct xj3_priv, ioaddr);
    u32 hw_cap = readl(ioaddr + GMAC_HW_FEATURE0);

    /*  MAC HW feature0 */
    dma_cap->mbps_10_100 = (hw_cap & GMAC_HW_FEAT_MIISEL);
    dma_cap->mbps_1000 = (hw_cap & GMAC_HW_FEAT_GMIISEL) >> 1;
    dma_cap->half_duplex = (hw_cap & GMAC_HW_FEAT_HDSEL) >> 2;
    dma_cap->hash_filter = (hw_cap & GMAC_HW_FEAT_VLHASH) >> 4;
    dma_cap->multi_addr = (hw_cap & GMAC_HW_FEAT_ADDMAC) >> 18;
    dma_cap->pcs = (hw_cap & GMAC_HW_FEAT_PCSSEL) >> 3;
    dma_cap->sma_mdio = (hw_cap & GMAC_HW_FEAT_SMASEL) >> 5;
    dma_cap->pmt_remote_wake_up = (hw_cap & GMAC_HW_FEAT_RWKSEL) >> 6;
    dma_cap->pmt_magic_frame = (hw_cap & GMAC_HW_FEAT_MGKSEL) >> 7;
    /* MMC */
    dma_cap->rmon = (hw_cap & GMAC_HW_FEAT_MMCSEL) >> 8;
    dev_dbg(priv->device, "%s, rmod:%d\n", __func__, dma_cap->rmon);
    /* IEEE 1588-2008 */
    dma_cap->atime_stamp = (hw_cap & GMAC_HW_FEAT_TSSEL) >> 12;
    /* 802.3az - Energy-Efficient Ethernet (EEE) */
    dma_cap->eee = (hw_cap & GMAC_HW_FEAT_EEESEL) >> 13;
    /* TX and RX csum */
    dma_cap->tx_coe = (hw_cap & GMAC_HW_FEAT_TXCOSEL) >> 14;
    dma_cap->rx_coe = (hw_cap & GMAC_HW_FEAT_RXCOESEL) >> 16;

    dev_dbg(priv->device, "%s, tx_coe:%d, rx_coe:%d\n", __func__,
            dma_cap->tx_coe, dma_cap->rx_coe);
    /* MAC HW feature1 */
    hw_cap = readl(ioaddr + GMAC_HW_FEATURE1);
    dma_cap->av = (hw_cap & GMAC_HW_FEAT_AVSEL) >> 20;
    dma_cap->tsoen = (hw_cap & GMAC_HW_TSOEN) >> 18;
    /* RX and TX FIFO sizes are encoded as log2(n / 128). Undo that by
     * shifting and store the sizes in bytes.
     */

    dma_cap->tx_fifo_size = 128 << ((hw_cap & GMAC_HW_TXFIFOSIZE) >> 6);
    dma_cap->rx_fifo_size = 128 << ((hw_cap & GMAC_HW_RXFIFOSIZE) >> 0);

    dev_info(priv->device, "%s, tx_fifo_size:%d\n", __func__,
             dma_cap->tx_fifo_size);
    dev_info(priv->device, "%s, rx_fifo_size:%d\n", __func__,
             dma_cap->rx_fifo_size);
    /* MAC HW feature2 */
    hw_cap = readl(ioaddr + GMAC_HW_FEATURE2);
    /* TX and RX number of channels */
    dma_cap->number_rx_channel = ((hw_cap & GMAC_HW_FEAT_RXCHCNT) >> 12) + 1;
    dma_cap->number_tx_channel = ((hw_cap & GMAC_HW_FEAT_TXCHCNT) >> 18) + 1;
    /* TX and RX number of queues */
    dma_cap->number_rx_queues = ((hw_cap & GMAC_HW_FEAT_RXQCNT) >> 0) + 1;
    dma_cap->number_tx_queues = ((hw_cap & GMAC_HW_FEAT_TXQCNT) >> 6) + 1;

    dma_cap->pps_out_num = (hw_cap & GMAC_HW_FEAT_PPSOUTNUM) >> 24;

    /*get HW feature3 */
    hw_cap = readl(ioaddr + GMAC_HW_FEATURE3);
    dma_cap->asp = (hw_cap & GMAC_HW_FEAT_ASP) >> 28;
    dma_cap->frpsel = (hw_cap & GMAC_HW_FEAT_FRPSEL) >> 10;
    dma_cap->frpes = (hw_cap & GMAC_HW_FEAT_FRPES) >> 13;
    dma_cap->tbssel = (hw_cap & GMAC_HW_FEAT_TBSSEL) >> 27;
    dma_cap->fpesel = (hw_cap & GMAC_HW_FEAT_FPESEL) >> 26;
    dma_cap->estwid = (hw_cap & GMAC_HW_FEAT_ESTWID) >> 20;
    dma_cap->estdep = (hw_cap & GMAC_HW_FEAT_ESTDEP) >> 17;
    dma_cap->estsel = (hw_cap & GMAC_HW_FEAT_ESTSEL) >> 16;
    dma_cap->tsn = dma_cap->fpesel | dma_cap->estsel;
	if (!tsn_enabled)
		dma_cap->tsn = 0;
    /* IEEE 1588-2002 */
    dma_cap->time_stamp = 0;

    switch (dma_cap->estwid) {
        default:
        case 0x0:
            dma_cap->estwid = 0;
            break;
        case 0x1:
            dma_cap->estwid = 16;
            break;
        case 0x2:
            dma_cap->estwid = 20;
            break;
        case 0x3:
            dma_cap->estwid = 24;
            break;
    }

    switch (dma_cap->estdep) {
        default:
        case 0x0:
            dma_cap->estdep = 0;
            break;
        case 0x1:
            dma_cap->estdep = 64;
            break;
        case 0x2:
            dma_cap->estdep = 128;
            break;
        case 0x3:
            dma_cap->estdep = 256;
            break;
        case 0x4:
            dma_cap->estdep = 512;
            break;
    }

    switch (dma_cap->frpes) {
        default:
            dma_cap->frpes = 0;
            break;
        case 0x0:
            dma_cap->frpes = 64;
            break;
        case 0x1:
            dma_cap->frpes = 128;
            break;
        case 0x2:
            dma_cap->frpes = 256;
            break;
    }

    return 1;
}

static int xj3_hw_init(struct xj3_priv *priv) {
    if (priv->plat->force_no_tx_coe)
        priv->plat->tx_coe = 0;
    else
        priv->plat->tx_coe = priv->dma_cap.tx_coe;

    if (!priv->plat->force_no_rx_coe) {
        priv->plat->rx_coe = priv->dma_cap.rx_coe;

        if (priv->dma_cap.rx_coe_type2)
            priv->plat->rx_coe = STMMAC_RX_COE_TYPE2;
        else if (priv->dma_cap.rx_coe_type1)
            priv->plat->rx_coe = STMMAC_RX_COE_TYPE1;
    } else {
        priv->plat->rx_coe = STMMAC_RX_COE_NONE;
    }

    priv->dev->priv_flags |= IFF_UNICAST_FLT;
    priv->hw_cap_support = xj3_get_hw_features(priv->ioaddr, &priv->dma_cap);

    if (priv->plat->force_thresh_dma_mode) priv->plat->tx_coe = 0;

    if (priv->dma_cap.tsoen) dev_info(priv->device, "TSO supported\n");
    return 0;
}

static void xj3_mdio_set_csr(struct xj3_priv *priv) {
    int rate = clk_get_rate(priv->plat->clk_ptp_ref);

    if (rate <= 35000000)
        priv->csr_val = DWCEQOS_MAC_MDIO_ADDR_CR_35;
    else if (rate <= 60000000)
        priv->csr_val = DWCEQOS_MAC_MDIO_ADDR_CR_60;
    else if (rate <= 100000000)
        priv->csr_val = DWCEQOS_MAC_MDIO_ADDR_CR_100;
    else if (rate <= 150000000)
        priv->csr_val = DWCEQOS_MAC_MDIO_ADDR_CR_150;
    else if (rate <= 250000000)
        priv->csr_val = DWCEQOS_MAC_MDIO_ADDR_CR_250;
    else if (rate <= 300000000)
        priv->csr_val = 0x5;
    else if (rate <= 500000000)
        priv->csr_val = 6;
    else if (rate <= 800000000)
        priv->csr_val = 7;
}

static int xj3_mdio_reset(struct mii_bus *bus)
{
    struct net_device *ndev = bus->priv;
    struct xj3_priv *priv = netdev_priv(ndev);
    if (priv->plat->phy_reset_extra_us > 0) {
        udelay(priv->plat->phy_reset_extra_us);
    }
    return 0;
}

static int xj3_mdio_register(struct xj3_priv *priv) {
    int err = 0;
    struct net_device *ndev = priv->dev;
    struct device_node *mdio_node = priv->plat->mdio_node;
    struct device *dev = ndev->dev.parent;
    struct mii_bus *new_bus;
    struct resource res;
    struct xj3_mdio_bus_data *mdio_bus_data = priv->plat->mdio_bus_data;

    if (!mdio_bus_data) return 0;

    new_bus = mdiobus_alloc();
    if (!new_bus) return -ENOMEM;

    new_bus->name = "hobot-mac-mdio";
    new_bus->read = &xj3_mdio_read;
    new_bus->write = &xj3_mdio_write;
    new_bus->reset = &xj3_mdio_reset;

    of_address_to_resource(dev->of_node, 0, &res);
    snprintf(new_bus->id, MII_BUS_ID_SIZE, "%s-%llx", new_bus->name,
             (unsigned long long)res.start);

    new_bus->priv = ndev;
    new_bus->parent = priv->device;

    if (mdio_node)
        err = of_mdiobus_register(new_bus, mdio_node);
    else
        err = mdiobus_register(new_bus);

    if (err != 0) {
        dev_info(priv->device, "Cannot register the MDIO bus\n");
        goto err_out;
    }

    priv->mii = new_bus;
    return 0;
err_out:

    mdiobus_free(new_bus);
    return err;
}

#define MAX_FRAME_SIZE 1522
#define MIN_FRAME_SIZE 64

static int xj3_est_write(struct xj3_priv *priv, u32 reg, u32 val,
                         bool is_gcla) {
    u32 control = 0x0;
    int timeout = 5;

    writel(val, priv->ioaddr + MTL_EST_GCL_DATA);
    control |= reg;
    control |= is_gcla ? 0x0 : MTL_EST_GCRR;
    control |= 0x0;

    writel(control, priv->ioaddr + MTL_EST_GCL_CONTROL);

    control |= MTL_EST_SRWO;
    writel(control, priv->ioaddr + MTL_EST_GCL_CONTROL);

    while (--timeout) {
        udelay(1000);
        if (readl(priv->ioaddr + MTL_EST_GCL_CONTROL) & MTL_EST_SRWO) continue;
        break;
    }

    if (!timeout) {
        dev_info(priv->device, "failed to write EST reg control 0x%x\n",
                 control);
        return -ETIMEDOUT;
    }
    return 0;
}

static u32 xj3_get_hw_tstamping(struct xj3_priv *priv) {
    return readl(priv->ioaddr + PTP_TCR);
}

static u32 xj3_get_ptp_period(struct xj3_priv *priv, u32 ptp_clock) {
    void __iomem *ioaddr = priv->ioaddr;
    u64 data;
    u32 value = readl(ioaddr + PTP_TCR);

    if (value & PTP_TCR_TSCFUPDT)
        data = (1000000000ULL / 50000000);
    else
        data = (1000000000ULL / ptp_clock);

    return (u32)data;
}

static u32 xj3_get_ptp_subperiod(struct xj3_priv *priv, u32 ptp_clock) {
    u32 value = readl(priv->ioaddr + PTP_TCR);
    u64 data;

    if (value & PTP_TCR_TSCFUPDT) return 0;

    data = (1000000000ULL * 1000ULL / ptp_clock);
    return data - xj3_get_ptp_period(priv, ptp_clock) * 1000;
}

static u32 xj3_config_sub_second_increment(struct xj3_priv *priv, u32 ptp_clock,
                                           int gmac4)  //, u32 *ssinc)
{
    void __iomem *ioaddr = priv->ioaddr;
    u32 value = readl(ioaddr + PTP_TCR);
    u32 subns, ns;
    u64 tmp;

    ns = xj3_get_ptp_period(priv, ptp_clock);
    subns = xj3_get_ptp_subperiod(priv, ptp_clock);

    if (!(value & PTP_TCR_TSCTRLSSR)) {
        tmp = ns * 1000;
        ns = DIV_ROUND_CLOSEST(tmp - (tmp % 465), 465);
        subns = DIV_ROUND_CLOSEST((tmp * 256) - (465 * ns * 256), 465);
    } else {
        subns = DIV_ROUND_CLOSEST(subns * 256, 1000);
    }

    ns &= PTP_SSIR_SSINC_MASK;
    subns &= PTP_SSIR_SNSINC_MASK;

    value = ns;
    value <<= GMAC4_PTP_SSIR_SSINC_SHIFT;
    value |= subns << GMAC4_PTP_SSIR_SNSINC_SHIFT;

    writel(value, ioaddr + PTP_SSIR);
    return ns;
}

static int xj3_config_addend(struct xj3_priv *priv, u32 addend) {
    void __iomem *ioaddr = priv->ioaddr;
    u32 value;
    int limit;

    writel(addend, ioaddr + PTP_TAR);
    value = readl(ioaddr + PTP_TCR);
    value |= PTP_TCR_TSADDREG;
    writel(value, ioaddr + PTP_TCR);

    limit = 10;
    while (limit--) {
        if (!(readl(ioaddr + PTP_TCR) & PTP_TCR_TSADDREG)) break;
        mdelay(10);
    }

    if (limit < 0) return -EBUSY;

    return 0;
}

static int xj3_init_systime(struct xj3_priv *priv, u32 sec, u32 nsec) {
    void __iomem *ioaddr = priv->ioaddr;
    int limit;
    u32 value;

    writel(sec, ioaddr + PTP_STSUR);
    writel(nsec, ioaddr + PTP_STNSUR);

    value = readl(ioaddr + PTP_TCR);
    value |= PTP_TCR_TSINIT;
    writel(value, ioaddr + PTP_TCR);

    dev_dbg(priv->device, "%s,and sec:0x%x, and nsec:0x%x\n", __func__, sec,
            nsec);
    dev_dbg(priv->device,
            "%s, PTP_STNSUR(0xb14):0x%x, and  PTP_TCR(0xb00):0x%x\n", __func__,
            readl(ioaddr + PTP_STNSUR), readl(ioaddr + PTP_TCR));
    limit = 10;
    while (limit--) {
        if (!(readl(ioaddr + PTP_TCR) & PTP_TCR_TSINIT)) break;
        mdelay(10);
    }

    if (limit < 0) return -EBUSY;
    return 0;
}

static void xj3_config_hw_tstamping(struct xj3_priv *priv, u32 data) {
    writel(data, priv->ioaddr + PTP_TCR);
}

static int xj3_est_init(struct net_device *ndev, struct xj3_priv *priv,
                        struct xj3_est_cfg *cfg, unsigned int estsel,
                        unsigned int estdep, unsigned int estwid, bool enable,
                        struct timespec64 *now) {
    void __iomem *ioaddr = priv->ioaddr;
    u32 control, real_btr[2];

    u8 ptov = 0;
    int i;

    if (!estsel || !estdep || !estwid || !cfg) return -EINVAL;

    if (cfg->gcl_size > estdep) {
        dev_info(priv->device, "%s, Invalid EST configuration supplied\n",
                 __func__);
        return -EINVAL;
    }

    control = readl(ioaddr + MTL_EST_CONTROL);
    control &= ~MTL_EST_EEST;
    writel(control, ioaddr + MTL_EST_CONTROL);

    if (!enable) return -EINVAL;

    real_btr[0] = cfg->btr_offset[0] + (u32)now->tv_nsec;
    real_btr[1] = cfg->btr_offset[1] + (u32)now->tv_sec;

#define EST_WRITE(__a, __b, __c)                                 \
    do {                                                         \
        if (xj3_est_write(priv, __a, __b, __c)) goto write_fail; \
    } while (0);

    EST_WRITE(MTL_EST_BTR_LOW, real_btr[0], false);
    EST_WRITE(MTL_EST_BTR_HIGH, real_btr[1], false);

    EST_WRITE(MTL_EST_CTR_LOW, cfg->ctr[0], false);
    EST_WRITE(MTL_EST_CTR_HIGH, cfg->ctr[1], false);
    EST_WRITE(MTL_EST_TER, cfg->ter, false);
    EST_WRITE(MTL_EST_LLR, cfg->gcl_size, false);

    for (i = 0; i < cfg->gcl_size; i++) {
        u32 reg = (i << MTL_EST_ADDR_OFFSET) & MTL_EST_ADDR;
        dev_info(priv->device, "%s, %d gcl:0x%x\n", __func__, i, cfg->gcl[i]);
        EST_WRITE(reg, cfg->gcl[i], true);
    }

    if (priv->plat->clk_ptp_rate) {
        ptov = (1000000000ULL) / priv->plat->clk_ptp_rate;
        ptov *= 6;
    }

    control = readl(ioaddr + MTL_EST_CONTROL);
    control &= ~PTOV;

    control |= MTL_EST_EEST | (ptov << 24);
    writel(control, ioaddr + MTL_EST_CONTROL);

    control |= MTL_EST_SSWL;
    writel(control, ioaddr + MTL_EST_CONTROL);

    return 0;
write_fail:
    dev_info(priv->device, "%s:Failed to write EST config\n", __func__);
    return -ETIMEDOUT;
}

#if 0
static void xj3_est_intr_config(struct xj3_priv *priv)
{

	writel(0x1F,priv->ioaddr + 0xc70);
}

#endif

static int xj3_est_configuration(struct xj3_priv *priv) {
    struct timespec64 now;
    u32 control, sec_inc;
    int ret = -EINVAL;
    u64 temp;
    unsigned long flags;
    //	xj3_est_intr_config(priv);
    if (!(priv->dma_cap.time_stamp || priv->adv_ts)) {
        dev_info(priv->device, "%s, No HW time stamping: Disabling EST\n",
                 __func__);
        priv->hwts_tx_en = 0;
        priv->hwts_rx_en = 0;
        priv->est_enabled = false;
        writel(0, priv->ioaddr + MTL_EST_CONTROL);

        return -EINVAL;
    }

    control = 0;
    xj3_config_hw_tstamping(priv, control);

    priv->hwts_tx_en = 1;
    priv->hwts_rx_en = 1;

    control = PTP_TCR_TSENA | PTP_TCR_TSENALL | PTP_TCR_TSCTRLSSR;
    xj3_config_hw_tstamping(priv, control);

    sec_inc =
        xj3_config_sub_second_increment(priv, priv->plat->clk_ptp_rate, 0);
    temp = div_u64(1000000000ULL, sec_inc);

    temp = (u64)(temp << 32);
    priv->default_addend = div_u64(temp, priv->plat->clk_ptp_rate);

    spin_lock_irqsave(&priv->ptp_lock, flags);
    xj3_config_addend(priv, priv->default_addend);

    ktime_get_real_ts64(&now);

    xj3_init_systime(priv, (u32)now.tv_sec, now.tv_nsec);
    spin_unlock_irqrestore(&priv->ptp_lock, flags);

    control =
        PTP_TCR_TSENA | PTP_TCR_TSINIT | PTP_TCR_TSENALL | PTP_TCR_TSCTRLSSR;
    xj3_config_hw_tstamping(priv, control);

    ret = xj3_est_init(priv->dev, priv, &priv->plat->est_cfg,
                       priv->dma_cap.estsel, priv->dma_cap.estdep,
                       priv->dma_cap.estwid, priv->plat->est_en, &now);

    if (ret) {
        priv->est_enabled = false;

    } else {
        priv->est_enabled = true;
    }

    return ret;
}

static int xj3_pps_init(struct net_device *ndev, struct xj3_priv *priv,
                        int index, struct stmmac_pps_cfg *cfg) {
    void __iomem *ioaddr = priv->ioaddr;
    u32 value;

    if ((cfg->ctrl_cmd & ~PPSCTRL_PPSCMD) != 0) return -EINVAL;

    if ((cfg->trgtmodsel & ~(TRGTMODSEL0 >> 5)) != 0) return -EINVAL;

    if ((cfg->target_time[0] & ~(TTSL0)) != 0) return -EINVAL;

    if (!cfg->enable && index) return -EINVAL;

    value = readl(ioaddr + MAC_PPS_CONTROL);
    value &= ~GENMASK(((index + 1) * 8) - 1, index * 8);
    value |= cfg->trgtmodsel << ((index * 8) + 5);
    if (index == 0) value |= cfg->enable << 4;

    writel(value, ioaddr + MAC_PPS_CONTROL);
    writel(cfg->target_time[1], ioaddr + MAC_PPSx_TARGET_TIME_SEC(index));
    if (readl(ioaddr + MAC_PPSx_TARGET_TIME_NSEC(index)) & TRGTBUSY0)
        return -EBUSY;

    writel(cfg->target_time[0], ioaddr + MAC_PPSx_TARGET_TIME_NSEC(index));
    writel(cfg->interval, ioaddr + MAC_PPSx_INTERVAL(index));
    writel(cfg->width, ioaddr + MAC_PPSx_WIDTH(index));

    value |= cfg->ctrl_cmd << (index * 8);
    writel(value, ioaddr + MAC_PPS_CONTROL);

    dev_info(priv->device, "Enableing %s PPS for output %d\n",
             cfg->enable ? "Flexible" : "Fixed", index);

    return 0;
}

static int xj3_pps_configuration(struct xj3_priv *priv, int index) {
    struct stmmac_pps_cfg *cfg;
    int ret = -EINVAL;

    if (index >= priv->dma_cap.pps_out_num) return -EINVAL;

    cfg = &priv->plat->pps_cfg[index];
    ret = xj3_pps_init(priv->dev, priv, index, cfg);
    return ret;
}

static void xj3_tsn_fp_configure(struct xj3_priv *priv) {
    u32 control;
    u32 value;

    writel(0x10001000, priv->ioaddr + MTL_FPE_Advance);

    value = readl(priv->ioaddr + GMAC_INT_EN);
    value |= (GMAC_INT_FPEIE_EN);
    writel(value, priv->ioaddr + GMAC_INT_EN);

    control = readl(priv->ioaddr + GMAC_Ext_CONFIG);
    control &= ~(1 << 16);
    writel(control, priv->ioaddr + GMAC_Ext_CONFIG);

    value = 1;
    value |= (3 << 8);
    writel(value, priv->ioaddr + 0xc90);

    value = readl(priv->ioaddr + 0xa4);
    value |= (1 << 24);
    writel(value, priv->ioaddr + 0xa4);

    control = readl(priv->ioaddr + GMAC_FPE_CTRL_STS);
    control |= GMAC_FPE_EFPE;
    writel(control, priv->ioaddr + GMAC_FPE_CTRL_STS);
}

int xj3_tsn_capable(struct net_device *ndev) {
    struct xj3_priv *priv = netdev_priv(ndev);
    return priv->tsn_ready == 1;
}

int xj3_tsn_link_configure(struct net_device *ndev, enum sr_class class,
                           u16 framesize, u16 vid, u8 add_link, u8 pcp_hi,
                           u8 pcp_lo) {
    struct xj3_priv *priv = netdev_priv(ndev);
    u32 mode_to_use;
    u32 port_rate;
    u32 queue;
    int err;
    s32 bw;

    dev_info(priv->device, "%s,and into here\n", __func__);

    if (!xj3_tsn_capable(ndev)) {
        dev_info(priv->device, "%s: NIC not capable\n", __func__);
        return -EINVAL;
    }

    if (framesize > MAX_FRAME_SIZE || framesize < MIN_FRAME_SIZE) {
        dev_info(priv->device, "%s: framesize (%u) must be [%d,%d]\n", __func__,
                 framesize, MIN_FRAME_SIZE, MAX_FRAME_SIZE);
        return -EINVAL;
    }

    if (add_link && !priv->tsn_vlan_added) {
        rtnl_lock();
        dev_info(priv->device, "%s: adding VLAN %u to HW filter on device:%s\n",
                 __func__, vid, ndev->name);

        err = vlan_vid_add(ndev, htons(ETH_P_8021Q), vid);
        if (err != 0)
            dev_info(priv->device, "%s: error adding vlan %u, res=%d\n",
                     __func__, vid, err);
        rtnl_unlock();

        priv->pcp_hi = pcp_hi & 0x7;
        priv->pcp_lo = pcp_lo & 0x7;
        priv->tsn_vlan_added = 1;
    }

    if (priv->plat->interface == PHY_INTERFACE_MODE_SGMII) {
        port_rate = PORT_RATE_SGMII;
    } else {
        port_rate = PORT_RATE_GMII;
    }

    switch (class) {
        case SR_CLASS_A:
            queue = AVB_CLASSA_Q;
            bw = AVB_CLASSA_BW;
            break;
        case SR_CLASS_B:
            queue = AVB_CLASSB_Q;
            bw = AVB_CLASSB_BW;
            break;
        default:
            dev_info(priv->device,
                     "%s: xj3 tsn unkown traffic-class, aborting config\n",
                     __func__);
            return -EINVAL;
    }

    mode_to_use = priv->plat->tx_queues_cfg[queue].mode_to_use;
    if (mode_to_use != MTL_QUEUE_AVB) {
        dev_info(priv->device, "%s: xj3 tsn :queue %d is no AVB /TSN\n",
                 __func__, queue);
        return -EINVAL;
    }

    if (priv->dma_cap.tsn) {
        if (priv->dma_cap.fpesel && priv->plat->fp_en) {
            xj3_tsn_fp_configure(priv);
        }
    }

    return 0;
}

u16 xj3_tsn_select_queue(struct net_device *ndev, struct sk_buff *skb,
                         struct net_device *sb_dev) {
    struct xj3_priv *priv = netdev_priv(ndev);

    if (!priv) return -EINVAL;

    if (xj3_tsn_capable(ndev)) {
        switch (vlan_get_protocol(skb)) {
            case htons(ETH_P_TSN):
                if (skb->priority == priv->pcp_hi) return AVB_CLASSA_Q;

                if (skb->priority == priv->pcp_lo) return AVB_CLASSB_Q;

                if (skb->priority == 0x1) return AVB_PTPCP_Q;

                return AVB_BEST_EFF_Q;

            case htons(ETH_P_1588):
                return AVB_PTPCP_Q;

            default:
                return AVB_BEST_EFF_Q;
        }
    }

    return -EINVAL;
}

static void xj3_set_speed(struct xj3_priv *priv) {
    struct net_device *ndev = priv->dev;
    struct phy_device *phydev = ndev->phydev;
    u32 regval;
    // int target = 0;
    // int rate_L1 = 0;
    // int rate_L2 = 0;

    regval = xj3_reg_read(priv, REG_DWCEQOS_MAC_CFG);
    regval &= ~(DWCEQOS_MAC_CFG_PS | DWCEQOS_MAC_CFG_FES | DWCEQOS_MAC_CFG_DM);

    if (phydev->duplex) regval |= DWCEQOS_MAC_CFG_DM;

    if (phydev->speed == SPEED_10) {
        regval |= DWCEQOS_MAC_CFG_PS;
    } else if (phydev->speed == SPEED_100) {
        regval |= DWCEQOS_MAC_CFG_PS | DWCEQOS_MAC_CFG_FES;
    } else if (phydev->speed != SPEED_1000) {
        dev_info(priv->device, "Unknown PHY speed %d\n", phydev->speed);
        return;
    }

    /*
     * FIXME: no need re-config clock path for x5 chip in link up stage.
     * Just keep below setting for enet is okay. They are clock source, but not actual rgmii bus clock...
     * rgmii bus clock will automatic adjust the frequency.
     *  root@buildroot:~$ cat /sys/kernel/debug/clk/clk_summary | grep enet
     *  enet_rgmii_clk                 1        1        0   125000000          0     0  50000         Y
     *  enet_ref_clk                   1        1        0    50000000          0     0  50000         Y
     *  enet_ptp_bclk                  2        2        0   200000000          0     0  50000         Y
     *  enet_axi_clk                   1        1        0   400000000          0     0  50000         Y
     *     enet_pclk                   1        1        0   200000000          0     0  50000         Y
     */
#if 0
    if (phydev->speed == SPEED_10) {
        /*target = 12500000;
        rate_L1 = clk_round_rate(priv->plat->xj3_mac_pre_div_clk, target);
        clk_set_rate(priv->plat->xj3_mac_pre_div_clk, rate_L1);*/

        target = 2500000;
        rate_L2 = clk_round_rate(priv->plat->xj3_mac_div_clk, target);
        clk_set_rate(priv->plat->xj3_mac_div_clk, rate_L2);
    } else if (phydev->speed == SPEED_100) {
        /*target = 125000000;
        rate_L1 = clk_round_rate(priv->plat->xj3_mac_pre_div_clk, target);
        clk_set_rate(priv->plat->xj3_mac_pre_div_clk, rate_L1);*/

        target = 25000000;
        rate_L2 = clk_round_rate(priv->plat->xj3_mac_div_clk, target);
        clk_set_rate(priv->plat->xj3_mac_div_clk, rate_L2);

    } else if (phydev->speed == SPEED_1000) {
        /*target = 125000000;
        rate_L1 = clk_round_rate(priv->plat->xj3_mac_pre_div_clk, target);
        clk_set_rate(priv->plat->xj3_mac_pre_div_clk, rate_L1);*/

        target = 125000000;
        rate_L2 = clk_round_rate(priv->plat->xj3_mac_div_clk, target);
        clk_set_rate(priv->plat->xj3_mac_div_clk, rate_L2);
    }
#endif

    xj3_reg_write(priv, REG_DWCEQOS_MAC_CFG, regval);
}

static void xj3_set_rx_flow_ctrl(struct xj3_priv *priv, bool enable) {
    u32 regval;

    regval = xj3_reg_read(priv, REG_DWCEQOS_MAC_RX_FLOW_CTRL);
    if (enable)
        regval |= DWCEQOS_MAC_RX_FLOW_CTRL_RFE;
    else
        regval &= ~DWCEQOS_MAC_RX_FLOW_CTRL_RFE;

    xj3_reg_write(priv, REG_DWCEQOS_MAC_RX_FLOW_CTRL, regval);
}

static void xj3_set_tx_flow_ctrl(struct xj3_priv *priv, bool enable) {
    u32 regval;

    regval = xj3_reg_read(priv, REG_DWCEQOS_MTL_RXQ0_OPER);
    if (enable)
        regval |= DWCEQOS_MTL_RXQ_EHFC;
    else
        regval &= ~DWCEQOS_MTL_RXQ_EHFC;

    xj3_reg_write(priv, REG_DWCEQOS_MTL_RXQ0_OPER, regval);
}

static void xj3_link_up(struct xj3_priv *priv) {
    u32 regval;

    regval = xj3_reg_read(priv, REG_DWCEQOS_MAC_LPI_CTRL_STATUS);
    regval |= DWCEQOS_MAC_LPI_CTRL_STATUS_PLS;
    xj3_reg_write(priv, REG_DWCEQOS_MAC_LPI_CTRL_STATUS, regval);
}

static void xj3_link_down(struct xj3_priv *priv) {
    u32 regval;

    regval = xj3_reg_read(priv, REG_DWCEQOS_MAC_LPI_CTRL_STATUS);
    regval &= ~DWCEQOS_MAC_LPI_CTRL_STATUS_PLS;
    xj3_reg_write(priv, REG_DWCEQOS_MAC_LPI_CTRL_STATUS, regval);
}
static void xj3_set_mac(void __iomem *ioaddr, bool enable);
static void xj3_adjust_link(struct net_device *ndev) {
    struct xj3_priv *priv = netdev_priv(ndev);
    struct phy_device *phydev = ndev->phydev;
    int status_change = 0;

    if (phydev->link) {
        if ((priv->speed != phydev->speed) ||
            (priv->duplex != phydev->duplex)) {
            xj3_set_speed(priv);

            priv->speed = phydev->speed;
            priv->duplex = phydev->duplex;
            status_change = 1;
            dev_dbg(priv->device, "%s, priv_speed;%d, phydev_speed:%d\n",
                    __func__, priv->speed, phydev->speed);
            dev_dbg(priv->device, "%s, priv_duplex;%d, phydev_duplex:%d\n",
                    __func__, priv->duplex, phydev->duplex);
        }

        if (priv->pause) {
            priv->flow_ctrl_rx = phydev->pause || phydev->asym_pause;
            priv->flow_ctrl_tx = phydev->pause || phydev->asym_pause;
        }

        if (priv->flow_ctrl_rx != priv->flow_current_rx) {
            if (priv->dma_cap.av) {
                priv->flow_ctrl_rx = false;
            }

            xj3_set_rx_flow_ctrl(priv, priv->flow_ctrl_rx);
            priv->flow_current_rx = priv->flow_ctrl_rx;
        }

        if (priv->flow_ctrl_tx != priv->flow_current_tx) {
            if (priv->dma_cap.av) {
                priv->flow_ctrl_tx = false;
            }
            xj3_set_tx_flow_ctrl(priv, priv->flow_ctrl_tx);
            priv->flow_current_tx = priv->flow_ctrl_tx;
        }
    }

    if (phydev->link != priv->link) {
        priv->link = phydev->link;
        status_change = 1;
    }

    if (status_change) {
        if (phydev->link) {
            netif_carrier_on(priv->dev);
            netif_trans_update(priv->dev);
            xj3_link_up(priv);
            xj3_set_mac(priv->ioaddr, true);
        } else {
            netif_carrier_off(priv->dev);
            xj3_link_down(priv);
            xj3_set_mac(priv->ioaddr, false);
        }
        dev_dbg(priv->device, "%s, speed:%s, duplex:%s\n", __func__,
                phy_speed_to_str(phydev->speed),
                phy_duplex_to_str(phydev->duplex));

        phy_print_status(phydev);
    }
}

static int xj3_init_phy(struct net_device *ndev) {
    struct xj3_priv *priv = netdev_priv(ndev);
    u32 tx_cnt = priv->plat->tx_queues_to_use;
    struct phy_device *phydev;
    int interface = priv->plat->interface;

    priv->link = false;
    priv->speed = SPEED_UNKNOWN;
    priv->duplex = DUPLEX_UNKNOWN;

    if (priv->plat->phy_node) {
        phydev = of_phy_connect(ndev, priv->plat->phy_node, &xj3_adjust_link, 0,
                                interface);

        if (!phydev) {
            dev_info(priv->device, "no phy founded\n");
            return -1;
        }
    } else {
        dev_info(priv->device, "No PHY configured\n");
        return -ENODEV;
    }
    phydev->mac_managed_pm=1;
    phy_init_hw(phydev);
    phy_attached_info(phydev);

    *phydev->supported &= *PHY_GBIT_FEATURES | SUPPORTED_Pause | SUPPORTED_Asym_Pause;

    if (tx_cnt > 1) {
        *phydev->supported &= ~(SUPPORTED_1000baseT_Half | SUPPORTED_100baseT_Half |
              SUPPORTED_10baseT_Half);
        if (priv->dma_cap.tsn) {
            *phydev->advertising &= ~(ADVERTISED_Pause | ADVERTISED_Asym_Pause);
        }
    }

    priv->pause = AUTONEG_ENABLE;
    return 0;
}

static void xj3_free_rx_buffer(struct xj3_priv *priv, u32 queue, int i) {
    struct xj3_rx_queue *rx_q = &priv->rx_queue[queue];

    if (rx_q->rx_skbuff[i]) {
        dma_unmap_single(priv->device, rx_q->rx_skbuff_dma[i], priv->dma_buf_sz,
                         DMA_FROM_DEVICE);
        dev_kfree_skb_any(rx_q->rx_skbuff[i]);
    }

    rx_q->rx_skbuff[i] = NULL;
}

static void dma_free_rx_skbufs(struct xj3_priv *priv, u32 queue) {
    int i;

    for (i = 0; i < DMA_RX_SIZE; i++) xj3_free_rx_buffer(priv, queue, i);
}

static void free_dma_rx_desc_resources(struct xj3_priv *priv) {
    u32 rx_count = priv->plat->rx_queues_to_use;
    u32 queue;

    for (queue = 0; queue < rx_count; queue++) {
        struct xj3_rx_queue *rx_q = &priv->rx_queue[queue];

        dma_free_rx_skbufs(priv, queue);

        if (!priv->extend_desc)
            dma_free_coherent(priv->device,
                              DMA_RX_SIZE * sizeof(struct dma_desc),
                              rx_q->dma_rx, rx_q->dma_rx_phy);
        else
            dma_free_coherent(priv->device,
                              DMA_RX_SIZE * sizeof(struct dma_desc),
                              rx_q->dma_erx, rx_q->dma_rx_phy);

        kfree(rx_q->rx_skbuff_dma);
        kfree(rx_q->rx_skbuff);
    }
}

static int alloc_dma_rx_desc_resources(struct xj3_priv *priv) {
    u32 rx_count = priv->plat->rx_queues_to_use;
    int ret = -ENOMEM;
    u32 queue;

    for (queue = 0; queue < rx_count; queue++) {
        struct xj3_rx_queue *rx_q = &priv->rx_queue[queue];

        rx_q->queue_index = queue;
        rx_q->priv_data = priv;
        rx_q->rx_skbuff_dma =
            kmalloc_array(DMA_RX_SIZE, sizeof(dma_addr_t), GFP_KERNEL);
        if (!rx_q->rx_skbuff_dma) goto err_dma;

        rx_q->rx_skbuff =
            kmalloc_array(DMA_RX_SIZE, sizeof(struct sk_buff *), GFP_KERNEL);
        if (!rx_q->rx_skbuff) goto err_dma;

        if (priv->extend_desc) {
            rx_q->dma_erx = dma_alloc_coherent(
                priv->device, DMA_RX_SIZE * sizeof(struct dma_ext_desc),
                &rx_q->dma_rx_phy, GFP_KERNEL);
            if (!rx_q->dma_erx) goto err_dma;
        } else {
            rx_q->dma_rx = dma_alloc_coherent(
                priv->device, DMA_RX_SIZE * sizeof(struct dma_desc),
                &rx_q->dma_rx_phy, GFP_KERNEL);
            if (!rx_q->dma_rx) goto err_dma;
        }
    }

    return 0;
err_dma:
    free_dma_rx_desc_resources(priv);
    return ret;
}

static void xj3_free_tx_buffer(struct xj3_priv *priv, u32 queue, int i) {
    struct xj3_tx_queue *tx_q = &priv->tx_queue[queue];

    if (tx_q->tx_skbuff_dma[i].buf) {
        if (tx_q->tx_skbuff_dma[i].map_as_page)
            dma_unmap_page(priv->device, tx_q->tx_skbuff_dma[i].buf,
                           tx_q->tx_skbuff_dma[i].len, DMA_TO_DEVICE);
        else
            dma_unmap_single(priv->device, tx_q->tx_skbuff_dma[i].buf,
                             tx_q->tx_skbuff_dma[i].len, DMA_TO_DEVICE);
    }

    if (tx_q->tx_skbuff[i]) {
        dev_kfree_skb_any(tx_q->tx_skbuff[i]);
        tx_q->tx_skbuff[i] = NULL;
        tx_q->tx_skbuff_dma[i].buf = 0;
        tx_q->tx_skbuff_dma[i].map_as_page = false;
    }
}

static void dma_free_tx_skbufs(struct xj3_priv *priv, u32 queue) {
    int i;

    for (i = 0; i < DMA_TX_SIZE; i++) {
        xj3_free_tx_buffer(priv, queue, i);
    }
}

static void xj3_set_tx_tail_ptr(void __iomem *ioaddr, u32 tail_ptr, u32 chan) {
    writel(tail_ptr, ioaddr + DMA_CHAN_TX_END_ADDR(chan));
}

static void free_dma_tx_desc_resources(struct xj3_priv *priv) {
    u32 tx_count = priv->plat->tx_queues_to_use;
    u32 queue;

    for (queue = 0; queue < tx_count; queue++) {
        struct xj3_tx_queue *tx_q = &priv->tx_queue[queue];

        dma_free_tx_skbufs(priv, queue);

        if (!priv->extend_desc)
            dma_free_coherent(priv->device,
                              DMA_TX_SIZE * sizeof(struct dma_desc),
                              tx_q->dma_tx, tx_q->dma_tx_phy);
        else
            dma_free_coherent(priv->device,
                              DMA_TX_SIZE * sizeof(struct dma_ext_desc),
                              tx_q->dma_etx, tx_q->dma_tx_phy);

        tx_q->dma_tx_phy = 0;
        tx_q->tx_tail_addr = 0;
        writel(tx_q->dma_tx_phy, priv->ioaddr + DMA_CHAN_TX_BASE_ADDR(queue));
        xj3_set_tx_tail_ptr(priv->ioaddr, tx_q->tx_tail_addr, queue);
        kfree(tx_q->tx_skbuff_dma);
        kfree(tx_q->tx_skbuff);
    }
}

static int alloc_dma_tx_desc_resources(struct xj3_priv *priv) {
    u32 tx_count = priv->plat->tx_queues_to_use;
    int ret = -ENOMEM;
    u32 queue;

    for (queue = 0; queue < tx_count; queue++) {
        struct xj3_tx_queue *tx_q = &priv->tx_queue[queue];

        tx_q->queue_index = queue;
        tx_q->priv_data = priv;

        tx_q->tx_skbuff_dma = kmalloc_array(
            DMA_TX_SIZE, sizeof(*tx_q->tx_skbuff_dma), GFP_KERNEL);
        if (!tx_q->tx_skbuff_dma) goto err_dma;

        tx_q->tx_skbuff =
            kmalloc_array(DMA_TX_SIZE, sizeof(struct sk_buff *), GFP_KERNEL);
        if (!tx_q->tx_skbuff) goto err_dma;

        if (priv->extend_desc) {
            tx_q->dma_etx = dma_alloc_coherent(
                priv->device, DMA_TX_SIZE * sizeof(struct dma_ext_desc),
                &tx_q->dma_tx_phy, GFP_KERNEL| __GFP_ZERO);
            if (!tx_q->dma_etx) goto err_dma;
        } else {
            tx_q->dma_tx = dma_alloc_coherent(
                priv->device, DMA_TX_SIZE * sizeof(struct dma_desc),
                &tx_q->dma_tx_phy, GFP_KERNEL| __GFP_ZERO);
            if (!tx_q->dma_tx) goto err_dma;
        }
    }

    return 0;

err_dma:
    free_dma_tx_desc_resources(priv);

    return ret;
}

static int alloc_dma_desc_resources(struct xj3_priv *priv) {
    int ret;
    ret = alloc_dma_rx_desc_resources(priv);
    if (ret) return ret;

    ret = alloc_dma_tx_desc_resources(priv);
    return ret;
}

static int xj3_init_rx_buffers(struct xj3_priv *priv, struct dma_desc *p, int i,
                               u32 queue) {
    struct xj3_rx_queue *rx_q = &priv->rx_queue[queue];
    struct sk_buff *skb;

    skb = __netdev_alloc_skb_ip_align(priv->dev, priv->dma_buf_sz, GFP_KERNEL);
    if (!skb) {
        dev_info(priv->device, "%s, Rx init failed: skb is NULL\n", __func__);
        return -ENOMEM;
    }

    rx_q->rx_skbuff[i] = skb;
    rx_q->rx_skbuff_dma[i] = dma_map_single(priv->device, skb->data,
                                            priv->dma_buf_sz, DMA_FROM_DEVICE);
    if (dma_mapping_error(priv->device, rx_q->rx_skbuff_dma[i])) {
        dev_info(priv->device, "%s, DMA mapping error\n", __func__);
        dev_kfree_skb_any(skb);
        return -EINVAL;
    }

    p->des0 = cpu_to_le32(rx_q->rx_skbuff_dma[i]);

    return 0;
}

static void xj3_clear_rx_descriptors(struct xj3_priv *priv, u32 queue) {
    struct xj3_rx_queue *rx_q = &priv->rx_queue[queue];
    int i;
    struct dma_desc *p;

    for (i = 0; i < DMA_RX_SIZE; i++) {
        if (priv->extend_desc) {
            p = &rx_q->dma_erx[i].basic;

        } else {
            p = &rx_q->dma_rx[i];
            p->des3 = cpu_to_le32(RDES3_OWN | RDES3_BUFFER1_VALID_ADDR);
            p->des3 |= cpu_to_le32(RDES3_INT_ON_COMPLETION_EN);
        }
    }
}

static void xj3_clear_tx_descriptors(struct xj3_priv *priv, u32 queue) {
    struct xj3_tx_queue *tx_q = &priv->tx_queue[queue];
    int i;
    struct dma_desc *p;

    for (i = 0; i < DMA_TX_SIZE; i++) {
        if (priv->extend_desc) {
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

static int init_dma_rx_desc_rings(struct net_device *ndev) {
    struct xj3_priv *priv = netdev_priv(ndev);
    u32 rx_count = priv->plat->rx_queues_to_use;
    u32 queue;
    int ret = -ENOMEM;
    int i;
    priv->dma_buf_sz = DEFAULT_BUFSIZE;

    for (queue = 0; queue < rx_count; queue++) {
        struct xj3_rx_queue *rx_q = &priv->rx_queue[queue];

        for (i = 0; i < DMA_RX_SIZE; i++) {
            struct dma_desc *p;

            if (priv->extend_desc)
                p = &((rx_q->dma_erx + i)->basic);
            else
                p = rx_q->dma_rx + i;

            ret = xj3_init_rx_buffers(priv, p, i, queue);
            if (ret) goto err_init_rx_buffers;
        }

		rx_q->reschedule_fill_flag = 0;
		rx_q->refill_rx = 0;
        rx_q->cur_rx = 0;
        rx_q->dirty_rx = (unsigned int)(i - DMA_RX_SIZE);

        xj3_clear_rx_descriptors(priv, queue);
    }

    return 0;

err_init_rx_buffers:
    while (queue >= 0) {
        while (--i >= 0) xj3_free_rx_buffer(priv, queue, i);

        if (queue == 0) break;
        i = DMA_RX_SIZE;
        queue--;
    }
    return ret;
}

static int init_dma_tx_desc_rings(struct net_device *ndev) {
    struct xj3_priv *priv = netdev_priv(ndev);
    u32 tx_count = priv->plat->tx_queues_to_use;
    u32 queue;
    int i;

    for (queue = 0; queue < tx_count; queue++) {
        struct xj3_tx_queue *tx_q = &priv->tx_queue[queue];

        for (i = 0; i < DMA_TX_SIZE; i++) {
            struct dma_desc *p;

            if (priv->extend_desc)
                p = &((tx_q->dma_etx + i)->basic);
            else
                p = tx_q->dma_tx + i;

            p->des0 = 0;
            p->des1 = 0;
            p->des2 = 0;
            p->des3 = 0;

            tx_q->tx_skbuff_dma[i].buf = 0;
            tx_q->tx_skbuff_dma[i].map_as_page = false;
            tx_q->tx_skbuff_dma[i].len = 0;
            tx_q->tx_skbuff_dma[i].last_segment = false;
            tx_q->tx_skbuff[i] = NULL;
        }

        tx_q->dirty_tx = 0;
        tx_q->cur_tx = 0;
        netdev_tx_reset_queue(netdev_get_tx_queue(priv->dev, queue));
    }

    return 0;
}

static void xj3_clear_descriptors(struct xj3_priv *priv) {
    u32 rx_queue_count = priv->plat->rx_queues_to_use;
    u32 tx_queue_count = priv->plat->tx_queues_to_use;
    u32 queue;

    for (queue = 0; queue < rx_queue_count; queue++)
        xj3_clear_rx_descriptors(priv, queue);

    for (queue = 0; queue < tx_queue_count; queue++)
        xj3_clear_tx_descriptors(priv, queue);
}

static int init_dma_desc_rings(struct net_device *ndev) {
    struct xj3_priv *priv = netdev_priv(ndev);
    int ret;

    ret = init_dma_rx_desc_rings(ndev);
    if (ret) return ret;

    ret = init_dma_tx_desc_rings(ndev);

    xj3_clear_descriptors(priv);

    return ret;
}

static int xj3_dma_reset(void __iomem *ioaddr) {
    u32 value = readl(ioaddr + DMA_BUS_MODE);
    int limit;

    value |= DMA_BUS_MODE_SFT_RESET;
    writel(value, ioaddr + DMA_BUS_MODE);
    limit = 10;
    while (limit--) {
	/*The DMA_BUS_MODE_SFT_RESET must be read at least 4 CSR clock cycles after it is written to 1.*/
        mdelay(10);
        if (!(readl(ioaddr + DMA_BUS_MODE) & DMA_BUS_MODE_SFT_RESET)) break;
	}
    if (limit < 0) return -EBUSY;

    return 0;
}

static void xj3_dma_init(void __iomem *ioaddr, struct xj3_dma_cfg *dma_cfg,
                         u32 dma_tx, u32 dma_rx, int atds) {
    u32 value = readl(ioaddr + DMA_SYS_BUS_MODE);

    if (dma_cfg->fixed_burst) value |= DMA_SYS_BUS_FB;
    if (dma_cfg->mixed_burst) value |= DMA_SYS_BUS_MB;
    if (dma_cfg->aal) value |= DMA_SYS_BUS_AAL;

    writel(value, ioaddr + DMA_SYS_BUS_MODE);
}

static void xj3_init_rx_chan(void __iomem *ioaddr, struct xj3_dma_cfg *dma_cfg,
                             u32 dma_rx_phy, u32 chan) {
    u32 value;
    u32 rxpl = dma_cfg->rxpbl ?: dma_cfg->pbl;

    value = readl(ioaddr + DMA_CHAN_RX_CONTROL(chan));
    value = value | (rxpl << DMA_BUS_MODE_RPBL_SHIFT);
    writel(value, ioaddr + DMA_CHAN_RX_CONTROL(chan));

    writel(dma_rx_phy, ioaddr + DMA_CHAN_RX_BASE_ADDR(chan));
}

static void xj3_set_rx_tail_ptr(void __iomem *ioaddr, u32 tail_ptr, u32 chan) {
    writel(tail_ptr, ioaddr + DMA_CHAN_RX_END_ADDR(chan));
}

static void xj3_init_chan(void __iomem *ioaddr, struct xj3_dma_cfg *dma_cfg,
                          u32 chan) {
    u32 value;

    value = readl(ioaddr + DMA_CHAN_CONTROL(chan));
    if (dma_cfg->pblx8) value |= value | DMA_BUS_MODE_PBL;

    writel(value, ioaddr + DMA_CHAN_CONTROL(chan));
    writel(DMA_CHAN_INTR_DEFAULT_MASK, ioaddr + DMA_CHAN_INTR_ENA(chan));
}

static void xj3_init_tx_chan(void __iomem *ioaddr, struct xj3_dma_cfg *dma_cfg,
                             u32 dma_tx_phy, u32 chan) {
    u32 value;

    u32 txpbl = dma_cfg->txpbl ?: dma_cfg->pbl;

    value = readl(ioaddr + DMA_CHAN_TX_CONTROL(chan));
    value = value | (txpbl << DMA_BUS_MODE_PBL_SHIFT);
    writel(value, ioaddr + DMA_CHAN_TX_CONTROL(chan));
    writel(dma_tx_phy, ioaddr + DMA_CHAN_TX_BASE_ADDR(chan));
}

static void xj3_set_dma_axi(void __iomem *ioaddr, struct xj3_axi *axi) {
    u32 value = readl(ioaddr + DMA_SYS_BUS_MODE);
    int i;

    if (axi->axi_lpi_en) value |= DMA_AXI_EN_LPI;

    if (axi->axi_xit_frm) value |= DMA_AXI_LPI_XIT_FRM;

    value &= ~DMA_AXI_WR_OSR_LMT;
    value |= (axi->axi_wr_osr_lmt & 0xf) << 24;

    value &= ~DMA_AXI_RD_OSR_LMT;
    value |= (axi->axi_rd_osr_lmt & 0xf) << 16;

    for (i = 0; i < AXI_BLEN; i++) {
        switch (axi->axi_blen[i]) {
            case 256:
                value |= DMA_AXI_BLEN256;
                break;
            case 128:
                value |= DMA_AXI_BLEN128;
                break;
            case 64:
                value |= DMA_AXI_BLEN64;
                break;
            case 32:
                value |= DMA_AXI_BLEN32;
                break;
            case 16:
                value |= DMA_AXI_BLEN16;
                break;
            case 8:
                value |= DMA_AXI_BLEN8;
                break;
            case 4:
                value |= DMA_AXI_BLEN4;
                break;
        }
    }

    writel(value, ioaddr + DMA_SYS_BUS_MODE);
}

static int xj3_init_dma_engine(struct xj3_priv *priv) {
    int ret;
    u32 rx_channel_count = priv->plat->rx_queues_to_use;
    u32 tx_channel_count = priv->plat->tx_queues_to_use;
    struct xj3_rx_queue *rx_q;
    struct xj3_tx_queue *tx_q;
    u32 chan = 0;

    ret = xj3_dma_reset(priv->ioaddr);
    if (ret) {
        dev_info(priv->device, "%s: Failed to reset dma\n", __func__);
        return ret;
    }

    xj3_dma_init(priv->ioaddr, priv->plat->dma_cfg, 0, 0, 0);

    for (chan = 0; chan < rx_channel_count; chan++) {
        rx_q = &priv->rx_queue[chan];
        xj3_init_rx_chan(priv->ioaddr, priv->plat->dma_cfg, rx_q->dma_rx_phy,
                         chan);

        rx_q->rx_tail_addr =
            rx_q->dma_rx_phy + (DMA_RX_SIZE * sizeof(struct dma_desc));
        xj3_set_rx_tail_ptr(priv->ioaddr, rx_q->rx_tail_addr, chan);
    }

    for (chan = 0; chan < tx_channel_count; chan++) {
        tx_q = &priv->tx_queue[chan];

        xj3_init_chan(priv->ioaddr, priv->plat->dma_cfg, chan);
        xj3_init_tx_chan(priv->ioaddr, priv->plat->dma_cfg, tx_q->dma_tx_phy,
                         chan);
        tx_q->tx_tail_addr =
            tx_q->dma_tx_phy;  // + (DMA_TX_SIZE *sizeof(struct dma_desc));
        xj3_set_tx_tail_ptr(priv->ioaddr, tx_q->tx_tail_addr, chan);
    }

    xj3_set_dma_axi(priv->ioaddr, priv->plat->axi);
    return 0;
}

static void xj3_set_umac_addr(void __iomem *ioaddr, unsigned char *addr,
                              unsigned int reg_n) {
    unsigned int high, low;
    unsigned long data;

    high = GMAC_ADDR_HIGH(reg_n);
    low = GMAC_ADDR_LOW(reg_n);

    data = (addr[5] << 8) | addr[4];
    data |= (STMMAC_CHAN0 << GMAC_HI_DCS_SHIFT);
    writel(data | GMAC_HI_REG_AE, ioaddr + high);

    data = (addr[3] << 24) | (addr[2] << 16) | (addr[1] << 8) | addr[0];
    writel(data, ioaddr + low);
}

static void xj3_core_init(struct xj3_priv *priv, int mtu) {
    void __iomem *ioaddr = priv->ioaddr;

    u32 value = readl(ioaddr + GMAC_CONFIG);

    value |= GMAC_CORE_INIT;

    if (mtu > 1500) value |= GMAC_CONFIG_2K;
    if (mtu > 2000) value |= GMAC_CONFIG_JE;

    if (priv->link) {
        value |= GMAC_CONFIG_TE;
        value |= priv->speed;
    }

    writel(value, ioaddr + GMAC_CONFIG);

    value = GMAC_INT_DEFAULT_MASK;
    value |= GMAC_INT_PMT_EN | GMAC_PCS_IRQ_DEFAULT;

    writel(value, ioaddr + GMAC_INT_EN);
}

static void xj3_set_tx_queue_weight(struct xj3_priv *priv) {
    u32 tx_count = priv->plat->tx_queues_to_use;
    u32 weight;
    u32 queue;
    u32 value;

    for (queue = 0; queue < tx_count; queue++) {
        weight = priv->plat->tx_queues_cfg[queue].weight;

        value = readl(priv->ioaddr + MTL_TXQX_WEIGHT_BASE_ADDR(queue));
        value &= ~MTL_TXQ_WEIGHT_ISCQW_MASK;
        value |= weight & MTL_TXQ_WEIGHT_ISCQW_MASK;
        writel(value, priv->ioaddr + MTL_TXQX_WEIGHT_BASE_ADDR(queue));
    }
}

static void xj3_pro_rx_algo(struct xj3_priv *priv, u32 rx_alg) {
    void __iomem *ioaddr = priv->ioaddr;

    u32 value = readl(ioaddr + MTL_OPERATION_MODE);
    value &= ~MTL_OPERATION_RAA;

    switch (rx_alg) {
        case MTL_RX_ALGORITHM_SP:
            value |= MTL_OPERATION_RAA_SP;
            break;
        case MTL_RX_ALGORITHM_WSP:
            value |= MTL_OPERATION_RAA_WSP;
            break;
        default:
            break;
    }

    writel(value, ioaddr + MTL_OPERATION_MODE);
}

static void xj3_pro_tx_algo(struct xj3_priv *priv, u32 tx_alg) {
    void __iomem *ioaddr = priv->ioaddr;
    u32 value = readl(ioaddr + MTL_OPERATION_MODE);

    value &= ~MTL_OPERATION_SCHALG_MASK;
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
            break;
    }

    writel(value, ioaddr + MTL_OPERATION_MODE);
}

static void xj3_dma_config_cbs(struct xj3_priv *priv, int send_slope,
                               u32 idle_slope, u32 high_credit, int low_credit,
                               u32 queue) {
    void __iomem *ioaddr = priv->ioaddr;
    u32 value;

    value = readl(ioaddr + MTL_ETSX_CTRL_BASE_ADDR(queue));
    value |= MTL_ETS_CTRL_AVALG;
    value |= MTL_ETS_CTRL_CC;
    writel(value, ioaddr + MTL_ETSX_CTRL_BASE_ADDR(queue));

    value = readl(ioaddr + MTL_SEND_SLP_CREDX_BASE_ADDR(queue));
    value &= ~MTL_SEND_SLP_CRED_SSC_MASK;
    value |= send_slope & MTL_SEND_SLP_CRED_SSC_MASK;
    writel(value, ioaddr + MTL_SEND_SLP_CREDX_BASE_ADDR(queue));

    value = readl(ioaddr + MTL_TXQX_WEIGHT_BASE_ADDR(queue));
    value &= ~MTL_TXQ_WEIGHT_ISCQW_MASK;
    value |= idle_slope & MTL_TXQ_WEIGHT_ISCQW_MASK;
    writel(value, ioaddr + MTL_TXQX_WEIGHT_BASE_ADDR(queue));

    value = readl(ioaddr + MTL_HIGH_CREDX_BASE_ADDR(queue));
    value &= ~MTL_HIGH_CRED_HC_MASK;
    value |= high_credit & MTL_HIGH_CRED_HC_MASK;
    writel(value, ioaddr + MTL_HIGH_CREDX_BASE_ADDR(queue));

    value = readl(ioaddr + MTL_LOW_CREDX_BASE_ADDR(queue));
    value &= ~MTL_HIGH_CRED_LC_MASK;
    value |= low_credit & MTL_HIGH_CRED_LC_MASK;
    writel(value, ioaddr + MTL_LOW_CREDX_BASE_ADDR(queue));

/*this will result rx errors, so may be test */
#if 0
	priv->hwts_tx_en = 1;
    priv->hwts_rx_en = 1;


    ktime_get_real_ts64(&now);
    sec_inc = xj3_config_sub_second_increment(priv,
                                              priv->plat->clk_ptp_rate,
                                              0);
    temp = div_u64(1000000000ULL, sec_inc);

    temp = (u64)(temp << 32);
    priv->default_addend = div_u64(temp, priv->plat->clk_ptp_rate);
    xj3_config_addend(priv, priv->default_addend);
    xj3_init_systime(priv, (u32)now.tv_sec, now.tv_nsec);

    control = PTP_TCR_TSENA | PTP_TCR_TSINIT|PTP_TCR_TSENALL|PTP_TCR_TSCTRLSSR;
    xj3_config_hw_tstamping(priv, control);

#endif
    value = readl(ioaddr + MTL_ETSX_CTRL_BASE_ADDR(queue));
    value |= (1 << 4);
    writel(value, ioaddr + MTL_ETSX_CTRL_BASE_ADDR(queue));
}

static u32 xj3_config_cbs(struct xj3_priv *priv) {
    u32 tx_queues_count = priv->plat->tx_queues_to_use;
    u32 queues_av = 0;
    u32 mode_to_use;
    u32 queue;

    /*queue 0 is for legacy triffc*/
    for (queue = 1; queue < tx_queues_count; queue++) {
        mode_to_use = priv->plat->tx_queues_cfg[queue].mode_to_use;

        if (mode_to_use == MTL_QUEUE_DCB)
            continue;
        else
            queues_av++;

        xj3_dma_config_cbs(priv, priv->plat->tx_queues_cfg[queue].send_slope,
                           priv->plat->tx_queues_cfg[queue].idle_slope,
                           priv->plat->tx_queues_cfg[queue].high_credit,
                           priv->plat->tx_queues_cfg[queue].low_credit, queue);
    }
    return queues_av;
}

static void xj3_rx_queue_dma_chan_map(struct xj3_priv *priv) {
    u32 rx_queues_count = priv->plat->rx_queues_to_use;
    u32 queue;
    u32 chan;
    void __iomem *ioaddr = priv->ioaddr;
    u32 value;

    for (queue = 0; queue < rx_queues_count; queue++) {
        chan = priv->plat->rx_queues_cfg[queue].chan;

        if (queue < 4)
            value = readl(ioaddr + MTL_RXQ_DMA_MAP0);
        else
            value = readl(ioaddr + MTL_RXQ_DMA_MAP1);

        if (queue == 0 || queue == 4) {
            value &= ~MTL_RXQ_DMA_Q04MDMACH_MASK;
            value |= MTL_RXQ_DMA_Q04MDMACH(chan);
        } else {
            value &= ~MTL_RXQ_DMA_QXMDMACH_MASK(queue);
            value |= MTL_RXQ_DMA_QXMDMACH(chan, queue);
        }

        if (queue < 4)
            writel(value, ioaddr + MTL_RXQ_DMA_MAP0);
        else
            writel(value, ioaddr + MTL_RXQ_DMA_MAP1);
    }
}

static void xj3_mac_enable_rx_queues(struct xj3_priv *priv) {
    u32 rx_queues_count = priv->plat->rx_queues_to_use;
    int queue;
    u8 mode;
    void __iomem *ioaddr = priv->ioaddr;
    u32 value;

    for (queue = 0; queue < rx_queues_count; queue++) {
        mode = priv->plat->rx_queues_cfg[queue].mode_to_use;

        value = readl(ioaddr + GMAC_RXQ_CTRL0);
        value &= GMAC_RX_QUEUE_CLEAR(queue);

        if (mode == MTL_QUEUE_AVB)
            value |= GMAC_RX_AV_QUEUE_ENABLE(queue);
        else if (mode == MTL_QUEUE_DCB)
            value |= GMAC_RX_DCB_QUEUE_ENABLE(queue);

        writel(value, ioaddr + GMAC_RXQ_CTRL0);
    }
}

static void xj3_mac_config_rx_queues_prio(struct xj3_priv *priv) {
    void __iomem *ioaddr = priv->ioaddr;
    u32 rx_count = priv->plat->rx_queues_to_use;
    u32 queue;
    u32 prio;
    u32 value;
    u32 base_reg;

    for (queue = 0; queue < rx_count; queue++) {
        if (!priv->plat->rx_queues_cfg[queue].use_prio) continue;

        prio = priv->plat->rx_queues_cfg[queue].prio;
        base_reg = (queue < 4) ? GMAC_RXQ_CTRL2 : GMAC_RXQ_CTRL3;
        value = readl(ioaddr + base_reg);
        value &= ~GMAC_RXQCTRL_PSRQX_MASK(queue);
        value |= (prio << GMAC_RXQCTRL_PSRQX_SHIFT(queue)) &
                 GMAC_RXQCTRL_PSRQX_MASK(queue);
        writel(value, ioaddr + base_reg);
    }
}

static void xj3_mac_config_tx_queues_prio(struct xj3_priv *priv) {
    u32 tx_count = priv->plat->tx_queues_to_use;
    u32 queue;
    u32 prio;
    void __iomem *ioaddr = priv->ioaddr;
    u32 base_reg;
    u32 value;

    for (queue = 0; queue < tx_count; queue++) {
        if (!priv->plat->tx_queues_cfg[queue].use_prio) continue;

        prio = priv->plat->tx_queues_cfg[queue].prio;

        base_reg = (queue < 4) ? GMAC_TXQ_PRTY_MAP0 : GMAC_TXQ_PRTY_MAP1;
        value = readl(ioaddr + base_reg);
        value &= ~GMAC_TXQCTRL_PSTQX_MASK(queue);
        value |= (prio << GMAC_TXQCTRL_PSTQX_SHIFT(queue)) &
                 GMAC_TXQCTRL_PSTQX_MASK(queue);
        writel(value, ioaddr + base_reg);
    }
}

static void xj3_mac_config_rx_queues_routing(struct xj3_priv *priv) {
    u32 rx_count = priv->plat->rx_queues_to_use;
    u32 queue;
    u8 packet;
    u32 value;
    void __iomem *ioaddr = priv->ioaddr;

    static const struct xj3_rx_routing route_possibilities[] = {
        {GMAC_RXQCTRL_AVCPQ_MASK, GMAC_RXQCTRL_AVCPQ_SHIFT},
        {GMAC_RXQCTRL_PTPQ_MASK, GMAC_RXQCTRL_PTPQ_SHIFT},
        {GMAC_RXQCTRL_DCBCPQ_MASK, GMAC_RXQCTRL_DCBCPQ_SHIFT},
        {GMAC_RXQCTRL_UPQ_MASK, GMAC_RXQCTRL_UPQ_SHIFT},
        {GMAC_RXQCTRL_MCBCQ_MASK, GMAC_RXQCTRL_MCBCQ_SHIFT},
    };

    for (queue = 0; queue < rx_count; queue++) {
        if (priv->plat->rx_queues_cfg[queue].pkt_route == 0x0) continue;

        packet = priv->plat->rx_queues_cfg[queue].pkt_route;

        value = readl(ioaddr + GMAC_RXQ_CTRL1);
        value &= ~route_possibilities[packet - 1].reg_mask;
        value |= (queue << route_possibilities[packet - 1].reg_shift) &
                 route_possibilities[packet - 1].reg_mask;

        if (packet == PACKET_AVCPQ) {
            value &= ~GMAC_RXQCTRL_TACPQE;
            value |= 0x1 << GMAC_RXQCTRL_TACPQE_SHIFT;
        } else if (packet == PACKET_MCBCQ) {
            value &= ~GMAC_RXQCTRL_MCBCQEN;
            value |= 0x1 << GMAC_RXQCTRL_MCBCQEN_SHIFT;
        }

        value |= 1 << 24;
        writel(value, ioaddr + GMAC_RXQ_CTRL1);
    }
}

static void xj3_configure_tsn(struct xj3_priv *priv) {
    u32 tx_cnt = priv->plat->tx_queues_to_use;
    u32 queues_av = 0;

    priv->sra_idleslope_res = 0;
    priv->srb_idleslope_res = 0;
    priv->tsn_vlan_added = 0;
    priv->tsn_ready = 0;

    if (!priv->dma_cap.av) return;

    if (tx_cnt > 1) queues_av = xj3_config_cbs(priv);

    if (queues_av < MIN_AVB_QUEUES) {
        dev_info(priv->device, "Not enuough queues for AVB (only %d)\n",
                 queues_av);
        return;
    }
    /*disable tx and rx flow control*/

    priv->tsn_ready = 1;
}

static void xj3_mtl_config(struct xj3_priv *priv) {
    u32 rx_queues_count = priv->plat->rx_queues_to_use;
    u32 tx_queues_count = priv->plat->tx_queues_to_use;

    if (tx_queues_count > 1) {
        xj3_set_tx_queue_weight(priv);
        xj3_pro_tx_algo(priv, priv->plat->tx_sched_algorithm);
    }

    if (rx_queues_count > 1)
        xj3_pro_rx_algo(priv, priv->plat->rx_sched_algorithm);

    //	if (tx_queues_count > 1)
    //		xj3_config_cbs(priv);

    /*add by dhw*/
    xj3_configure_tsn(priv);

    xj3_rx_queue_dma_chan_map(priv);

    xj3_mac_enable_rx_queues(priv);

    if (rx_queues_count > 1) {
        xj3_mac_config_rx_queues_prio(priv);
        xj3_mac_config_rx_queues_routing(priv);
    }

    if (tx_queues_count > 1) {
        xj3_mac_config_tx_queues_prio(priv);
    }
}

static int xj3_rx_ipc_enable(struct xj3_priv *priv) {
    void __iomem *ioaddr = priv->ioaddr;
    u32 value = readl(ioaddr + GMAC_CONFIG);
    if (priv->rx_csum)
        value |= GMAC_CONFIG_IPC;
    else
        value &= ~GMAC_CONFIG_IPC;

    value |= (1 << 21);
    writel(value, ioaddr + GMAC_CONFIG);

    value = readl(ioaddr + GMAC_CONFIG);
    return !!(value & GMAC_CONFIG_IPC);
}

static void xj3_set_mac(void __iomem *ioaddr, bool enable) {
    u32 value = readl(ioaddr + MAC_CTRL_REG);

    if (enable)
        value |= MAC_ENABLE_RX | MAC_ENABLE_TX;
    else
        value &= ~(MAC_ENABLE_TX | MAC_ENABLE_RX);
    writel(value, ioaddr + MAC_CTRL_REG);
}

static void xj3_dma_rx_chan_op_mode(void __iomem *ioaddr, int mode, u32 chan,
                                    int rxfifosz, u8 qmode) {
    unsigned int rqs = (rxfifosz / 256) - 1;
    u32 mtl_rx_op, mtl_rx_int;

    mtl_rx_op = readl(ioaddr + MTL_CHAN_RX_OP_MODE(chan));
    mtl_rx_op |= MTL_OP_MODE_RSF;
    mtl_rx_op &= ~MTL_OP_MODE_RQS_MASK;
    mtl_rx_op |= rqs << MTL_OP_MODE_RQS_SHIFT;

    if (rxfifosz >= 4096 && qmode != MTL_QUEUE_AVB) {
        unsigned int rfd, rfa;
        mtl_rx_op |= MTL_OP_MODE_EHFC;
        switch (rxfifosz) {
            case 4096:
                rfd = 0x03;
                rfa = 0x01;
                break;
            case 8192:
                rfd = 0x06;
                rfa = 0x0a;
                break;
            case 16384:
                rfd = 0x06;
                rfa = 0x12;
                break;
            default:
                rfd = 0x06;
                rfa = 0x1e;
                break;
        }

        mtl_rx_op &= ~MTL_OP_MODE_RFD_MASK;
        mtl_rx_op |= rfd << MTL_OP_MODE_RFD_SHIFT;
        mtl_rx_op &= ~MTL_OP_MODE_RFA_MASK;
        mtl_rx_op |= rfa << MTL_OP_MODE_RFA_SHIFT;
    }

    writel(mtl_rx_op, ioaddr + MTL_CHAN_RX_OP_MODE(chan));
    mtl_rx_int = readl(ioaddr + MTL_CHAN_INT_CTRL(chan));
    writel(mtl_rx_int | MTL_RX_OVERFLOW_INT_EN,
           ioaddr + MTL_CHAN_INT_CTRL(chan));
}

static void xj3_dma_tx_chan_op_mode(void __iomem *ioaddr, int mode, u32 chan,
                                    int fifosz, u8 qmode) {
    u32 mtl_tx_op = readl(ioaddr + MTL_CHAN_TX_OP_MODE(chan));  // 0xd00
    unsigned int tqs = fifosz / 256 - 1;

    mtl_tx_op |= MTL_OP_MODE_TSF;

    mtl_tx_op &= ~MTL_OP_MODE_TXQEN_MASK;
    if (qmode != MTL_QUEUE_AVB)
        mtl_tx_op |= MTL_OP_MODE_TXQEN;
    else
        mtl_tx_op |= MTL_OP_MODE_TXQEN_AV;

    mtl_tx_op &= ~MTL_OP_MODE_TQS_MASK;
    mtl_tx_op |= tqs << MTL_OP_MODE_TQS_SHIFT;
    writel(mtl_tx_op, ioaddr + MTL_CHAN_TX_OP_MODE(chan));
}

static void xj3_dma_operation_mode(struct xj3_priv *priv) {
    u32 rx_count = priv->plat->rx_queues_to_use;
    u32 tx_count = priv->plat->tx_queues_to_use;
    int rxfifosz = priv->plat->rx_fifo_size;
    int txfifosz = priv->plat->tx_fifo_size;

#ifdef HOBOT_USE_IRQ_SPLIT
    u32 value = readl(priv->ioaddr + DMA_BUS_MODE);
#endif

    u32 rxmode = 0;
    u32 chan;
    u8 qmode;

    if (rxfifosz == 0) rxfifosz = priv->dma_cap.rx_fifo_size;

    if (txfifosz == 0) txfifosz = priv->dma_cap.tx_fifo_size;

    rxfifosz /= rx_count;
    txfifosz /= tx_count;

    for (chan = 0; chan < rx_count; chan++) {
        rxmode = SF_DMA_MODE;
        qmode = priv->plat->rx_queues_cfg[chan].mode_to_use;
        xj3_dma_rx_chan_op_mode(priv->ioaddr, rxmode, chan, rxfifosz, qmode);
    }

    for (chan = 0; chan < tx_count; chan++) {
        qmode = priv->plat->tx_queues_cfg[chan].mode_to_use;
        xj3_dma_tx_chan_op_mode(priv->ioaddr, SF_DMA_MODE, chan, txfifosz,
                                qmode);
    }

#ifdef HOBOT_USE_IRQ_SPLIT
    value |= 0x1 << 16;
    writel(value, priv->ioaddr + DMA_BUS_MODE);
#endif
}

static void xj3_mmc_setup(struct xj3_priv *priv) {
    unsigned int mode = MMC_CNTRL_RESET_ON_READ | MMC_CNTRL_COUNTER_RESET |
                        MMC_CNTRL_PRESET | MMC_CNTRL_FULL_HALF_PRESET;

    priv->ptpaddr = priv->ioaddr + PTP_GMAC4_OFFSET;
    priv->mmcaddr = priv->ioaddr + MMC_GMAC4_OFFSET;

    hobot_mmc_intr_all_mask(priv->mmcaddr);

    if (priv->dma_cap.rmon) {
        hobot_mmc_ctrl(priv->mmcaddr, mode);
        memset(&priv->mmc, 0, sizeof(struct hobot_counters));
    } else {
        netdev_info(priv->dev, "No MAC Management Counters available\n");
    }
}

static void xj3_start_rx_dma(struct xj3_priv *priv, u32 chan) {
    void __iomem *ioaddr = priv->ioaddr;
    u32 value = readl(ioaddr + DMA_CHAN_RX_CONTROL(chan));

    value |= DMA_CONTROL_SR;

    writel(value, ioaddr + DMA_CHAN_RX_CONTROL(chan));

    value = readl(ioaddr + GMAC_CONFIG);
    value |= GMAC_CONFIG_RE;
    writel(value, ioaddr + GMAC_CONFIG);
}

static void xj3_start_tx_dma(struct xj3_priv *priv, u32 chan) {
    void __iomem *ioaddr = priv->ioaddr;
    u32 value = readl(ioaddr + DMA_CHAN_TX_CONTROL(chan));

    value |= DMA_CONTROL_ST;
    writel(value, ioaddr + DMA_CHAN_TX_CONTROL(chan));

    value = readl(ioaddr + GMAC_CONFIG);
    value |= GMAC_CONFIG_TE;
    writel(value, ioaddr + GMAC_CONFIG);
}

static void xj3_start_all_dma(struct xj3_priv *priv) {
    u32 rx_count = priv->plat->rx_queues_to_use;
    u32 tx_count = priv->plat->tx_queues_to_use;
    u32 chan = 0;

    for (chan = 0; chan < rx_count; chan++) xj3_start_rx_dma(priv, chan);

    for (chan = 0; chan < tx_count; chan++) xj3_start_tx_dma(priv, chan);
}

static void xj3_set_rings_length(struct xj3_priv *priv) {
    u32 rx_count = priv->plat->rx_queues_to_use;
    u32 tx_count = priv->plat->tx_queues_to_use;
    u32 chan;
    void __iomem *ioaddr = priv->ioaddr;
    u32 len;

    for (chan = 0; chan < tx_count; chan++) {
        len = DMA_TX_SIZE - 1;
        writel(len, ioaddr + DMA_CHAN_TX_RING_LEN(chan));
    }

    for (chan = 0; chan < rx_count; chan++) {
        len = DMA_RX_SIZE - 1;
        writel(len, ioaddr + DMA_CHAN_RX_RING_LEN(chan));
    }
}

static int xj3_set_time(struct ptp_clock_info *ptp,
                        const struct timespec64 *ts) {
    struct xj3_priv *priv = container_of(ptp, struct xj3_priv, ptp_clock_ops);
    unsigned long flags;

    spin_lock_irqsave(&priv->ptp_lock, flags);

    dev_info(priv->device, "%s, tv_sec:0x%llx, tv_nesc:0x%lx\n", __func__,
             ts->tv_sec, ts->tv_nsec);

    xj3_init_systime(priv, ts->tv_sec, ts->tv_nsec);
    spin_unlock_irqrestore(&priv->ptp_lock, flags);
    return 0;
}

static u64 xj3_get_systime(struct xj3_priv *priv) {
    u64 ns;

    ns = readl(priv->ioaddr + PTP_STNSR);
    ns += readl(priv->ioaddr + PTP_STSR) * 1000000000ULL;
    return ns;
}

static int xj3_get_time(struct ptp_clock_info *ptp, struct timespec64 *ts) {
    struct xj3_priv *priv = container_of(ptp, struct xj3_priv, ptp_clock_ops);
    unsigned long flags;
    u64 ns;

    spin_lock_irqsave(&priv->ptp_lock, flags);

    ns = xj3_get_systime(priv);

    spin_unlock_irqrestore(&priv->ptp_lock, flags);

    *ts = ns_to_timespec64(ns);
    return 0;
}

static int xj3_adjust_systime(struct xj3_priv *priv, u32 sec, u32 nsec,
                              int add_sub, int gmac) {
    u32 value;
    int limit;

    if (add_sub) {
        sec = -sec;

        value = readl(priv->ioaddr + PTP_TCR);
        if (value & PTP_TCR_TSCTRLSSR)
            nsec = (PTP_DIGITAL_ROLLOVER_MODE - nsec);
        else
            nsec = (PTP_BINARY_ROLLOVER_MODE - nsec);
    }

    writel(sec, priv->ioaddr + PTP_STSUR);
    value = (add_sub << PTP_STNSUR_ADDSUB_SHIFT) | nsec;
    writel(value, priv->ioaddr + PTP_STNSUR);

    value = readl(priv->ioaddr + PTP_TCR);
    value |= PTP_TCR_TSUPDT;
    writel(value, priv->ioaddr + PTP_TCR);

    limit = 10;
    while (limit--) {
        if (!(readl(priv->ioaddr + PTP_TCR) & PTP_TCR_TSUPDT)) break;
        mdelay(10);
    }

    if (limit < 0) return -EBUSY;

    return 0;
}

static int xj3_adjust_time(struct ptp_clock_info *ptp, s64 delta) {
    struct xj3_priv *priv = container_of(ptp, struct xj3_priv, ptp_clock_ops);
    unsigned long flags;
    u32 sec, nsec;
    u32 quotient, reminder;
    int neg_adj = 0;

    if (delta < 0) {
        neg_adj = 1;
        delta = -delta;
    }

    quotient = div_u64_rem(delta, 1000000000ULL, &reminder);
    sec = quotient;
    nsec = reminder;

    spin_lock_irqsave(&priv->ptp_lock, flags);
    xj3_adjust_systime(priv, sec, nsec, neg_adj, 1);
    spin_unlock_irqrestore(&priv->ptp_lock, flags);

    return 0;
}

static int xj3_flex_pps_config(struct xj3_priv *priv, int index,
                               struct xj3_pps_cfg *cfg, bool enable,
                               u32 sub_second_inc, u32 systime_flags) {
    u32 tnsec = readl(priv->ioaddr + MAC_PPSx_TARGET_TIME_NSEC(index));
    u32 val = readl(priv->ioaddr + MAC_PPS_CONTROL);
    u64 period;

    if (!cfg->available) return -EINVAL;

    if (tnsec & TRGTBUSY0) return -EBUSY;

    if (!sub_second_inc || !systime_flags) return -EINVAL;

    val &= ~PPSx_MASK(index);
    if (!enable) {
        val |= PPSCMDx(index, 0x5);
        writel(val, priv->ioaddr + MAC_PPS_CONTROL);
        return 0;
    }

    val |= PPSCMDx(index, 0x2);
    val |= TRGTMODSELx(index, 0x2);
    val |= PPSEN0;
    writel(cfg->start.tv_sec, priv->ioaddr + MAC_PPSx_TARGET_TIME_SEC(index));

    if (!(systime_flags & PTP_TCR_TSCTRLSSR))
        cfg->start.tv_nsec = (cfg->start.tv_nsec * 1000) / 465;

    writel(cfg->start.tv_nsec, priv->ioaddr + MAC_PPSx_TARGET_TIME_NSEC(index));

    period = cfg->period.tv_sec * 1000000000;
    period += cfg->period.tv_nsec;
    do_div(period, sub_second_inc);

    if (period <= 1) return -EINVAL;

    writel(period - 1, priv->ioaddr + MAC_PPSx_INTERVAL(index));

    period >>= 1;
    if (period <= 1) return -EINVAL;

    writel(period - 1, priv->ioaddr + MAC_PPSx_WIDTH(index));
    writel(val, priv->ioaddr + MAC_PPS_CONTROL);
    return 0;
}

static int xj3_ptp_enable(struct ptp_clock_info *ptp,
                          struct ptp_clock_request *rq, int on) {
    struct xj3_priv *priv = container_of(ptp, struct xj3_priv, ptp_clock_ops);
    struct xj3_pps_cfg *cfg;
    int ret = -EOPNOTSUPP;

    unsigned long flags;

#if 1
    switch (rq->type) {
        case PTP_CLK_REQ_PEROUT:
            cfg = &priv->pps[rq->perout.index];
            cfg->start.tv_sec = rq->perout.start.sec;
            cfg->start.tv_nsec = rq->perout.start.nsec;
            cfg->period.tv_sec = rq->perout.period.sec;
            cfg->period.tv_nsec = rq->perout.period.nsec;
            spin_lock_irqsave(&priv->ptp_lock, flags);
            ret =
                xj3_flex_pps_config(priv, rq->perout.index, cfg, on,
                                    priv->sub_second_inc, priv->systime_flags);
            spin_unlock_irqrestore(&priv->ptp_lock, flags);
            break;
        default:
            break;
    }

#endif
    return ret;
}

static int xj3_adjust_freq(struct ptp_clock_info *ptp, s32 ppb) {
    struct xj3_priv *priv = container_of(ptp, struct xj3_priv, ptp_clock_ops);
    unsigned long flags;

    u32 diff, addend;
    int neg_adj = 0;
    u64 adj;

    if (ppb < 0) {
        neg_adj = 1;
        ppb = -ppb;
    }

    addend = priv->default_addend;
    adj = addend;
    adj *= ppb;

    diff = div_u64(adj, 1000000000ULL);
    addend = neg_adj ? (addend - diff) : (addend + diff);

    spin_lock_irqsave(&priv->ptp_lock, flags);
    xj3_config_addend(priv, addend);
    spin_unlock_irqrestore(&priv->ptp_lock, flags);
    return 0;
}

static struct ptp_clock_info xj3_ptp_clock_ops = {
    .owner = THIS_MODULE,
    .name = "xj3_ptp_clock",
    .max_adj = 300000000,  // 62500000,
    .n_alarm = 0,
    .n_ext_ts = 0,
    .n_per_out = 0,
    .n_pins = 0,
    .pps = 0,
    .adjfreq = xj3_adjust_freq,
    .adjtime = xj3_adjust_time,
    .gettime64 = xj3_get_time,
    .settime64 = xj3_set_time,
    .enable = xj3_ptp_enable,
};

static void xj3_ptp_register(struct xj3_priv *priv) {
    int i;

    for (i = 0; i < priv->dma_cap.pps_out_num; i++) {
        if (i >= XJ3_PPS_MAX) break;

        priv->pps[i].available = true;
    }

    xj3_ptp_clock_ops.n_per_out = priv->dma_cap.pps_out_num;

    spin_lock_init(&priv->ptp_lock);

    priv->ptp_clock_ops = xj3_ptp_clock_ops;
    priv->ptp_clock = ptp_clock_register(&priv->ptp_clock_ops, priv->device);

    if (IS_ERR(priv->ptp_clock)) {
        dev_info(priv->device, "ptp_clock_register failed by hobot\n");
        priv->ptp_clock = NULL;
    } else if (priv->ptp_clock) {
        dev_info(priv->device, "registered PTP clock by hobot successfully\n");
    }
}

static int xj3_init_ptp(struct xj3_priv *priv) {
    if (!(priv->dma_cap.time_stamp || priv->dma_cap.atime_stamp))
        return -EOPNOTSUPP;

    priv->adv_ts = 1;

    if (priv->dma_cap.time_stamp) {
        dev_info(priv->device, "IEEE 1588-2002 Timestamp supported ");
        dev_info(priv->device, "by Hobot Ethernet Network Card\n");
    }

    if (priv->adv_ts) {
        dev_info(priv->device, "IEEE 1588-2008 Advanced Timestamp ");
        dev_info(priv->device, "supported by Hobot Ethernet Network Card\n");
    }

    priv->hwts_tx_en = 0;
    priv->hwts_rx_en = 0;

    xj3_ptp_register(priv);

    return 0;
}

static int xj3_hw_setup(struct net_device *ndev, bool init_ptp) {
    struct xj3_priv *priv = netdev_priv(ndev);
    u32 rx_cnt = priv->plat->rx_queues_to_use;
    u32 tx_cnt = priv->plat->tx_queues_to_use;
    int ret;
    u32 chan;

    ret = xj3_init_dma_engine(priv);
    if (ret < 0) {
        dev_info(priv->device, "%s, DMA engine initilization failed\n",
                 __func__);
        return ret;
    }

    xj3_set_umac_addr(priv->ioaddr, ndev->dev_addr, 0);

    xj3_core_init(priv, ndev->mtu);

    xj3_mtl_config(priv);

    ret = xj3_rx_ipc_enable(priv);
    if (!ret) {
        dev_dbg(priv->device, "RX IPC checksum offload disabled\n");
        priv->plat->rx_coe = STMMAC_RX_COE_NONE;
        priv->rx_csum = 0;
    }

    xj3_set_mac(priv->ioaddr, true);

    xj3_dma_operation_mode(priv);

    xj3_mmc_setup(priv);

    if (init_ptp) {
        ret = clk_prepare_enable(priv->plat->clk_ptp_ref);
        if (ret < 0)
            dev_err(priv->device, "failed to enable PTP reference clock\n");
        ret = xj3_init_ptp(priv);
        if (ret == -EOPNOTSUPP) {
            dev_err(priv->device, "PTP not supported by HW\n");
        } else if (ret) {
            dev_err(priv->device, "PTP init failed\n");
        }
    }

    if (priv->use_riwt) {
        priv->rx_riwt = MAX_DMA_RIWT;
        hobot_dma_rx_watchdog(priv, MAX_DMA_RIWT, rx_cnt);
    }
    xj3_est_configuration(priv);
    xj3_set_rings_length(priv);
    xj3_start_all_dma(priv);
    /* disable mac rx&tx and enable these after link up. */
    xj3_set_mac(priv->ioaddr, false);
    if (priv->tso) {
        u32 value;
        for (chan = 0; chan < tx_cnt; chan++) {
            value = readl(priv->ioaddr + DMA_CHAN_TX_CONTROL(chan));
            writel(value | DMA_CONTROL_TSE,
                   priv->ioaddr + DMA_CHAN_TX_CONTROL(chan));
        }
    }

    return 0;
}

static void free_dma_desc_resources(struct xj3_priv *priv) {
    free_dma_rx_desc_resources(priv);
    free_dma_tx_desc_resources(priv);
}

static void xj3_enable_all_queues(struct xj3_priv *priv) {
    u32 rx_cnt = priv->plat->rx_queues_to_use;
    u32 queue;

    for (queue = 0; queue < rx_cnt; queue++) {
        struct xj3_rx_queue *rx_q = &priv->rx_queue[queue];
        napi_enable(&rx_q->napi);
    }
}

static void xj3_start_all_queues(struct xj3_priv *priv) {
    u32 tx_cnt = priv->plat->tx_queues_to_use;
    u32 queue;

    for (queue = 0; queue < tx_cnt; queue++)
        netif_tx_start_queue(netdev_get_tx_queue(priv->dev, queue));
}

#ifdef CONFIG_HOBOT_DIAG
static void dwceqos_diag_process(u32 errsta, uint32_t regval) {
    u8 sta;
    u8 envgen_timing;
    u8 env_data[8];
    u32 int_status;
	static uint8_t last_status = DiagEventStaUnknown;

    int_status = regval;
#if IS_ENABLED(CONFIG_HOBOT_DIAG_INJECT)
	diag_inject_val(ModuleDiag_eth, EventIdEthDmaBusErr, &int_status);
	if (int_status & (DMA_CHAN_STATUS_TPS | DMA_CHAN_STATUS_FBE))
		errsta = 1;
#endif
    if (errsta) {
        sta = DiagEventStaFail;
        envgen_timing = DiagGenEnvdataWhenErr;
        /* no channel & error type & reg size = 4 */
        env_data[0] = 0xFF;
        env_data[1] = 0xFF;
        env_data[2] = 0x00;
        env_data[3] = 0x04;
        env_data[4] = (u8)(int_status & 0xFF);
        env_data[5] = (u8)((int_status >> 8) & 0xFF);
        env_data[6] = (u8)((int_status >> 16) & 0xFF);
        env_data[7] = (u8)((int_status >> 24) & 0xFF);
        if (diag_send_event_stat_and_env_data(
                DiagMsgPrioHigh, ModuleDiag_eth, EventIdEthDmaBusErr, sta,
                envgen_timing, (uint8_t *)env_data, 8) < 0) {
            pr_debug("eth: event %d snd diag msg with env data error\n",
                     EventIdEthDmaBusErr);
        }
    } else if (last_status != DiagEventStaSuccess) {
        sta = DiagEventStaSuccess;
        if (diag_send_event_stat(DiagMsgPrioHigh, ModuleDiag_eth,
                                 EventIdEthDmaBusErr, sta) < 0) {
            pr_debug("eth: event %d snd diag msg with env data error\n",
                     EventIdEthDmaBusErr);
        }
    }
	last_status = !errsta ? DiagEventStaSuccess : DiagEventStaFail;
}
#endif

static int xj3_dma_interrupt(struct xj3_priv *priv, struct xj3_extra_stats *x,
                             u32 chan) {
    int ret = 0;
    int err = 0;
    void __iomem *ioaddr = priv->ioaddr;
    u32 intr_status = readl(ioaddr + DMA_CHAN_STATUS(chan));  // 0x1160

#if 0
    u32 intr_en = readl(ioaddr + DMA_CHAN_INTR_ENA(chan));
    if (intr_status)
    	dev_info(priv->device, "%s, chan:%d, intr_en:0x%x, intr_status:0x%x\n", \
            __func__, chan, intr_en, intr_status);
#endif
    if ((intr_status & DMA_CHAN_STATUS_AIS) || priv->use_riwt) {
        if (intr_status & DMA_CHAN_STATUS_RBU) {
            x->rx_buf_unav_irq++;
            ret |= handle_rx;
        }
        if (intr_status & DMA_CHAN_STATUS_RPS) x->rx_process_stopped_irq++;

        if (intr_status & DMA_CHAN_STATUS_RWT) {
            dev_info(priv->device, "rx watchdog irq\n");
            x->rx_watchdog_irq++;
        }
        if (intr_status & DMA_CHAN_STATUS_ETI) x->tx_early_irq++;

        if (intr_status & DMA_CHAN_STATUS_TPS) {
            x->tx_process_stopped_irq++;
            ret = tx_hard_error;
            dev_err(priv->device, "tx_process_stopped\n");
            err = 1;
        }

        if (intr_status & DMA_CHAN_STATUS_FBE) {
            x->fatal_bus_error_irq++;
            ret = tx_hard_error;
            dev_err(priv->device, "tx_fatal_bus_error\n");
            err = 1;
        }
    }

    if ((intr_status & DMA_CHAN_STATUS_NIS) || priv->use_riwt) {
        x->normal_irq_n++;
        if (intr_status & DMA_CHAN_STATUS_RI) {
            x->rx_normal_irq_n++;
            ret |= handle_rx;
        }

        if (intr_status & DMA_CHAN_STATUS_TI) {
            x->tx_normal_irq_n++;
            ret |= handle_tx;
        }

        if (intr_status & DMA_CHAN_STATUS_ERI) x->rx_early_irq++;
    }
    writel((intr_status), ioaddr + DMA_CHAN_STATUS(chan));

    if (eth_err_flag == 1) {
        err = 1;
        eth_err_flag = 0;
    }

#ifdef CONFIG_HOBOT_DIAG
    dwceqos_diag_process(err, intr_status);
#endif
    return ret;
}

static void xj3_disable_dma_irq(struct xj3_priv *priv, u32 chan) {
    writel(0, priv->ioaddr + DMA_CHAN_INTR_ENA(chan));
}

static void xj3_init_tx_desc(struct dma_desc *p, u32 cnt) {
    p->des0 = 0;
    p->des1 = 0;
    p->des2 = 0;
    p->des3 = 0;
}

static void xj3_stop_tx_dma(struct xj3_priv *priv, u32 chan) {
    void __iomem *ioaddr = priv->ioaddr;

    u32 value = readl(ioaddr + DMA_CHAN_TX_CONTROL(chan));

    value &= ~DMA_CONTROL_ST;
    writel(value, ioaddr + DMA_CHAN_TX_CONTROL(chan));

    value = readl(ioaddr + GMAC_CONFIG);
    value &= ~GMAC_CONFIG_TE;
    writel(value, ioaddr + GMAC_CONFIG);
}

static void xj3_tx_err(struct xj3_priv *priv, u32 chan) {
    struct xj3_tx_queue *tx_q = &priv->tx_queue[chan];
    int i;
    unsigned long flags;

    spin_lock_irqsave(&priv->state_lock, flags);
    if (test_bit(HOBOT_DOWN, &priv->state)) {
        spin_unlock_irqrestore(&priv->state_lock, flags);
        return;
    }
    spin_unlock_irqrestore(&priv->state_lock, flags);

    netif_tx_stop_queue(netdev_get_tx_queue(priv->dev, chan));
    xj3_stop_tx_dma(priv, chan);
    dma_free_tx_skbufs(priv, chan);

    for (i = 0; i < DMA_TX_SIZE; i++)
        if (priv->extend_desc) {
            xj3_init_tx_desc(&tx_q->dma_etx[i].basic, (i == DMA_TX_SIZE - 1));
        } else {
            xj3_init_tx_desc(&tx_q->dma_tx[i], (i == DMA_TX_SIZE - 1));
        }

    tx_q->dirty_tx = 0;
    tx_q->cur_tx = 0;

    netdev_tx_reset_queue(netdev_get_tx_queue(priv->dev, chan));
    xj3_start_tx_dma(priv, chan);
    priv->dev->stats.tx_errors++;
    netif_tx_wake_queue(netdev_get_tx_queue(priv->dev, chan));
}

static inline void xj3_pcs_isr(struct xj3_priv *priv, void __iomem *ioaddr,
                               u32 reg, unsigned int intr_status,
                               struct xj3_extra_stats *x) {
    u32 val = readl(ioaddr + GMAC_AN_STATUS(reg));

    if (intr_status & PCS_ANE_IRQ) {
        x->irq_pcs_ane_n++;
        if (val & GMAC_AN_STATUS_ANC)
            dev_info(priv->device, "pcs: ANE process completed\n");
    }

    if (intr_status & PCS_LINK_IRQ) {
        x->irq_pcs_link_n++;
        if (val & GMAC_AN_STATUS_LS)
            dev_info(priv->device, "pcs: link up\n");
        else
            dev_info(priv->device, "pcs: Link down\n");
    }
}

static void xj3_phystatus(struct xj3_priv *priv, struct xj3_extra_stats *x) {
    u32 status;
    void __iomem *ioaddr = priv->ioaddr;
    unsigned long flags;

    status = readl(ioaddr + GMAC_PHYIF_CONTROL_STATUS);
    x->irq_rgmii_n++;

    if (status & GMAC_PHYIF_CTRLSTATUS_LNKSTS) {
        int speed_value;

        x->pcs_link = 1;

        speed_value = ((status & GMAC_PHYIF_CTRLSTATUS_SPEED) >>
                       GMAC_PHYIF_CTRLSTATUS_SPEED_SHIFT);

        if (speed_value == GMAC_PHYIF_CTRLSTATUS_SPEED_125)
            x->pcs_speed = SPEED_1000;
        else if (speed_value == GMAC_PHYIF_CTRLSTATUS_SPEED_25)
            x->pcs_speed = SPEED_100;
        else
            x->pcs_speed = SPEED_10;

        spin_lock_irqsave(&priv->state_lock, flags);
        clear_bit(HOBOT_DOWN, &priv->state);

        spin_unlock_irqrestore(&priv->state_lock, flags);
        x->pcs_duplex = ((status & GMAC_PHYIF_CTRLSTATUS_LNKMOD) >>
                         GMAC_PHYIF_CTRLSTATUS_LNKMOD_MASK);
        netif_carrier_on(priv->dev);
        dev_info(priv->device, "Link is Up - %d/%s \n", (int)x->pcs_speed,
                 x->pcs_duplex ? "Full" : "Half");

    } else {
        x->pcs_link = 0;
        spin_lock_irqsave(&priv->state_lock, flags);
        set_bit(HOBOT_DOWN, &priv->state);
        spin_unlock_irqrestore(&priv->state_lock, flags);
        netif_carrier_off(priv->dev);
        dev_info(priv->device, "Link is Down\n");
    }

#if 0
	dev_info(priv->device, "%s, and mac_phy_status_reg-0xf8: 0x%x\n", \
         __func__, readl(ioaddr + 0xf8));

	dev_info(priv->device, "%s, and mac_AN_stauts_reg-0xe4: 0x%x\n", \
        __func__, readl(ioaddr + 0xe4));
#endif
}

static int xj3_host_irq_status(struct xj3_priv *priv,
                               struct xj3_extra_stats *x) {
    void __iomem *ioaddr = priv->ioaddr;

    u32 intr_status = readl(ioaddr + GMAC_INT_STATUS);
    u32 intr_enable = readl(ioaddr + GMAC_INT_EN);
    int ret = 0;

    intr_status &= intr_enable;
    if (intr_status & mmc_tx_irq) {
        x->mmc_tx_irq_n++;
        dev_info(priv->device, "%s, mmc_tx_irq_n: %ld\n", __func__,
                 x->mmc_tx_irq_n);
    }
    if (intr_status & mmc_rx_csum_offload_irq) {
        x->mmc_rx_csum_offload_irq_n++;
        dev_info(priv->device, "%s, mmc_rx_csum_offload_irq_n: %ld\n", __func__,
                 x->mmc_rx_csum_offload_irq_n);
    }
    if (intr_status & pmt_irq) {
        readl(ioaddr + GMAC_PMT);
        x->irq_receive_pmt_irq_n++;

        dev_info(priv->device, "%s, irq receive pmt irq\n", __func__);
    }

    if (intr_status & mac_fpeis) {
        u32 value = readl(ioaddr + GMAC_FPE_CTRL_STS);

        dev_info(priv->device,
                 "%s, Frame preemption interrupt, fpe_ctrl_sts:0x%x\n",
                 __func__, value);

        dev_info(priv->device, "%s, frg cntr:%d, hlod:%d\n", __func__,
                 readl(ioaddr + MMC_TX_FPE_Frg_Cntr),
                 readl(ioaddr + MMC_TX_HOLD_Req_Cntr));

        writel(value, ioaddr + GMAC_FPE_CTRL_STS);
    }
    xj3_pcs_isr(priv, ioaddr, GMAC_PCS_BASE, intr_status, x);

    if (intr_status & PCS_RGSMIIIS_IRQ) {
        xj3_phystatus(priv, x);
    }

    return ret;
}

static int xj3_host_mtl_irq_status(struct xj3_priv *priv, u32 chan) {
    void __iomem *ioaddr = priv->ioaddr;
    u32 mtl_irq_status;
    int ret = 0;

    mtl_irq_status = readl(ioaddr + MTL_INT_STATUS);
    if (mtl_irq_status & MTL_INT_QX(chan)) {
        u32 status = readl(ioaddr + MTL_CHAN_INT_CTRL(chan));
        if (status & MTL_RX_OVERFLOW_INT) {
            writel(status | MTL_RX_OVERFLOW_INT,
                   ioaddr + MTL_CHAN_INT_CTRL(chan));
            ret = CORE_IRQ_MTL_RX_OVERFLOW;
        }

        if (status & MTL_ABPSIS_INT) {
            if (readl(ioaddr + MTL_ETSX_STATUS_BASE_ADDR(chan)))
                writel(status, ioaddr + MTL_CHAN_INT_CTRL(chan));
        }
    }

    if (mtl_irq_status & MTL_ESTIS) {
        u32 status = readl(ioaddr + 0xc58);
        dev_dbg(priv->device, "%s, MTL EST intterupt here, status:0x%x\n",
                __func__, status);

        if (status & MTL_STATUS_CGSN) {
            dev_info(priv->device, "est current gcl slot num:%ld\n",
                     ((status & MTL_STATUS_CGSN) >> 16) & 0xf);
        }

        if (status & MTL_STATUS_BTRL) {
            dev_info(priv->device, "btr error loop count:%ld\n",
                     ((status & MTL_STATUS_BTRL) >> 8) & 0xf);
        }

        if (status & MTL_STATUS_SWOL)
            dev_info(priv->device, "gate control list %ld own by sofware\n",
                     (status & MTL_STATUS_SWOL >> 7) & 0x1);
        else
            dev_info(priv->device, "gcl 0 own by software\n");

        if (status & MTL_STATUS_CGCE) {
            dev_info(priv->device, "constant gate control error\n");
        }
        if (status & MTL_STATUS_HLBS)
            dev_info(priv->device, "head of line blocking due scheduling\n");

        if (status & MTL_STATUS_HLBF) {
            u32 queue = readl(ioaddr + MTL_EST_Frm_Size_Error);
            u32 frame = readl(ioaddr + MTL_EST_Frm_Size_Capture);

            dev_info(priv->device,
                     "head of line bocking due to frame size, and queue:%d\n",
                     queue);
            dev_info(priv->device,
                     "HOF block frame size:%d, and the queue:%d\n",
                     frame & 0x3fff, (frame >> 16));

            writel(queue, ioaddr + MTL_EST_Frm_Size_Error);
        }
        if (status & MTL_STATUS_BTRE) dev_info(priv->device, "BTR error\n");

        if (status & MTL_STATUS_SWLC)
            dev_info(priv->device, "switch to S/W owned list complete\n");

        writel(status, ioaddr + 0xc58);
    }

    writel(mtl_irq_status, ioaddr + MTL_INT_STATUS);
    return ret;
}

static irqreturn_t xj3_interrupt(int irq, void *dev_id) {
    struct net_device *ndev = (struct net_device *)dev_id;
    struct xj3_priv *priv = netdev_priv(ndev);
    u32 rx_cnt = priv->plat->rx_queues_to_use;
    u32 tx_cnt = priv->plat->tx_queues_to_use;
    u32 queues_count;
    u32 queue;
    int mtl_status = 0;
    int status;
    u32 chan;

    queues_count = (rx_cnt > tx_cnt) ? rx_cnt : tx_cnt;

#if 0
    status = readl(priv->ioaddr + 0x1008);
    dev_info(priv->device, "%s, dma interrupt status(0x1008):0x%x\n",
             __func__, status);
    status = readl(priv->ioaddr + 0xc20);
    dev_info(priv->device, "%s, mtl interrupt_status(0xc20):0x%x\n",
             __func__, status);
    status = readl(priv->ioaddr + 0xb0);
    dev_info(priv->device, "%s, mac interrupt status(0xb0):0x%x\n",
             __func__, status);
#endif
    status = xj3_host_irq_status(priv, &priv->xstats);

    if (status) {
        if (status & CORE_IRQ_TX_PATH_IN_LPI_MODE)
            priv->tx_path_in_lpi_mode = true;

        if (status & CORE_IRQ_TX_PATH_EXIT_LPI_MODE)
            priv->tx_path_in_lpi_mode = false;
    }

    for (queue = 0; queue < queues_count; queue++) {
        struct xj3_rx_queue *rx_q = &priv->rx_queue[queue];

        mtl_status = status | xj3_host_mtl_irq_status(priv, queue);

        if (mtl_status & CORE_IRQ_MTL_RX_OVERFLOW) {
            if (!rx_q->reschedule_fill_flag) {
                xj3_set_rx_tail_ptr(priv->ioaddr, rx_q->rx_tail_addr, queue);
            }
        }
    }

    for (chan = 0; chan < tx_cnt; chan++) {
        struct xj3_rx_queue *rx_q = &priv->rx_queue[chan];
        status = xj3_dma_interrupt(priv, &priv->xstats, chan);

#if 0
        if (status)
    	    dev_info(priv->device, "%s, and status:0x%x, handler_rx:0x%x,"
                     "handle_tx:0x%x\n", __func__, status,
                     handle_rx, handle_tx);
#endif
        if (likely((status & handle_rx)) || (status & handle_tx) ||
            (mtl_status & CORE_IRQ_MTL_RX_OVERFLOW)) {
            if (napi_schedule_prep(&rx_q->napi)) {
                xj3_disable_dma_irq(priv, chan);
                __napi_schedule(&rx_q->napi);
            }
        }

        if (status & tx_hard_error) {
            xj3_tx_err(priv, chan);
        }
    }

    return IRQ_HANDLED;
}

static int xj3_ioctl_get_qmode(struct xj3_priv *priv, void __user *data) {
    u32 tx_qcount = priv->plat->tx_queues_to_use;
    struct xj3_qmode_cfg mode;

    u8 qmode;

    if (copy_from_user(&mode, data, sizeof(mode))) return -EINVAL;

    if (mode.queue_idx >= tx_qcount) return -EINVAL;

    qmode = priv->plat->tx_queues_cfg[mode.queue_idx].mode_to_use;
    switch (qmode) {
        case MTL_QUEUE_DCB:
            mode.queue_mode = STMMAC_QMODE_DCB;
            break;
        case MTL_QUEUE_AVB:
            mode.queue_mode = STMMAC_QMODE_AVB;
            break;
        default:
            return -EINVAL;
    }

    if (copy_to_user(data, &mode, sizeof(mode))) return -EFAULT;
    return 0;
}

static int xj3_set_est(struct xj3_priv *priv, void __user *data) {
    struct xj3_est_cfg *cfg = &priv->plat->est_cfg;
    struct xj3_est_cfg *est;
    int ret = 0;

    est = kzalloc(sizeof(*est), GFP_KERNEL);
    if (!est) return -ENOMEM;

    if (copy_from_user(est, data, sizeof(*est))) {
        ret = -EFAULT;
        goto out_free;
    }

    if (est->gcl_size > STMMAC_EST_GCL_MAX_ENTRIES) {
        ret = -EINVAL;
        goto out_free;
    }

    if (est->enabled) {
        cfg->btr_offset[0] = est->btr_offset[0];
        cfg->btr_offset[1] = est->btr_offset[1];
        cfg->ctr[0] = est->ctr[0];
        cfg->ctr[1] = est->ctr[1];
        cfg->ter = est->ter;
        cfg->gcl_size = est->gcl_size;

        memcpy(cfg->gcl, est->gcl, cfg->gcl_size * sizeof(*cfg->gcl));
    } else {
        cfg->btr_offset[0] = 0;
        cfg->btr_offset[1] = 0;
        cfg->ctr[0] = 0;
        cfg->ctr[1] = 0;
        cfg->ter = 0;
        cfg->gcl_size = 0;
        memset(cfg->gcl, 0, sizeof(cfg->gcl));
    }

    priv->plat->est_en = est->enabled;
    ret = xj3_est_configuration(priv);
    if (!est->enabled) ret = 0;

out_free:
    kfree(est);
    return ret;
}

static void xj3_est_read(struct xj3_priv *priv, u32 reg, bool is_gcla) {
    u32 control = 0x0;
    int timeout = 15;

    control = readl(priv->ioaddr + MTL_EST_GCL_CONTROL);
    control |= reg;
    control |= is_gcla ? 0x0 : MTL_EST_GCRR;
    control |= (1 << 1);
    control |= MTL_EST_SRWO;
    writel(control, priv->ioaddr + MTL_EST_GCL_CONTROL);

    while (--timeout) {
        udelay(1000);
        if (readl(priv->ioaddr + MTL_EST_GCL_CONTROL) & MTL_EST_SRWO) continue;
        break;
    }

    dev_dbg(priv->device, "%s, reg:0x%x, value:0x%x\n", __func__, reg,
            readl(priv->ioaddr + MTL_EST_GCL_DATA));
}

static int xj3_get_est(struct xj3_priv *priv, void __user *data) {
    struct xj3_est_cfg *est;
    int ret = 0;
    int i;

    est = kzalloc(sizeof(*est), GFP_KERNEL);
    if (!est) return -ENOMEM;

    est->enabled = priv->est_enabled;
    est->estwid = priv->dma_cap.estwid;
    est->estdep = priv->dma_cap.estdep;
    est->btr_offset[0] = priv->plat->est_cfg.btr_offset[0];
    est->btr_offset[1] = priv->plat->est_cfg.btr_offset[1];
    est->ctr[0] = priv->plat->est_cfg.ctr[0];
    est->ctr[1] = priv->plat->est_cfg.ctr[1];
    est->ter = priv->plat->est_cfg.ter;
    est->gcl_size = priv->plat->est_cfg.gcl_size;
    memcpy(est->gcl, priv->plat->est_cfg.gcl,
           est->gcl_size * sizeof(*est->gcl));

    xj3_est_read(priv, MTL_EST_BTR_LOW, false);
    xj3_est_read(priv, MTL_EST_BTR_HIGH, false);

    xj3_est_read(priv, MTL_EST_CTR_LOW, false);
    xj3_est_read(priv, MTL_EST_CTR_HIGH, false);
    xj3_est_read(priv, MTL_EST_TER, false);
    xj3_est_read(priv, MTL_EST_LLR, false);

    for (i = 0; i < 5; i++) {
        u32 reg = (i << MTL_EST_ADDR_OFFSET) & MTL_EST_ADDR;
        xj3_est_read(priv, reg, true);
    }

    if (copy_to_user(data, est, sizeof(*est))) {
        ret = -EFAULT;
        goto out_free;
    }

out_free:
    kfree(est);
    return ret;
}

static int xj3_ioctl_get_pps(struct xj3_priv *priv, void __user *data) {
    u32 ptp_period = xj3_get_ptp_period(priv, priv->plat->clk_ptp_rate);
    struct xj3_ioctl_pps_cfg pps;
    struct stmmac_pps_cfg *cfg;
    bool dig;

    memset(&pps, 0, sizeof(pps));

    if (copy_from_user(&pps, data, sizeof(pps))) return -EFAULT;

    if (pps.index >= XJ3_PPS_MAX) return -EINVAL;

    if (pps.index >= priv->dma_cap.pps_out_num) return -EINVAL;

    cfg = &priv->plat->pps_cfg[pps.index];
    dig = xj3_get_hw_tstamping(priv) & PTP_TCR_TSCTRLSSR;
    pps.enabled = cfg->enable;
    pps.ctrl_cmd = cfg->ctrl_cmd;

    if (pps.enabled) {
        pps.trgtmodsel = cfg->trgtmodsel;
        pps.target_time[0] = cfg->target_time[0];
        pps.target_time[1] = cfg->target_time[1];
        pps.interval = cfg->interval * ptp_period;
        pps.width = cfg->width * ptp_period;
    } else {
        pps.freq = 0x1 << pps.ctrl_cmd;
        pps.freq = 0x1 << pps.ctrl_cmd;
        if (dig) pps.freq /= 2;
    }

    if (copy_from_user(data, &pps, sizeof(pps))) return -EFAULT;

    return 0;
}

static int xj3_ioctl_set_pps(struct xj3_priv *priv, void __user *data) {
    u32 ptp_period = xj3_get_ptp_period(priv, priv->plat->clk_ptp_rate);
    struct xj3_ioctl_pps_cfg pps;
    struct stmmac_pps_cfg *cfg;

    memset(&pps, 0, sizeof(pps));

    if (copy_from_user(&pps, data, sizeof(pps))) return -EFAULT;
    if (pps.index >= XJ3_PPS_MAX) return -EINVAL;
    if (pps.index >= priv->dma_cap.pps_out_num) return -EINVAL;

    cfg = &priv->plat->pps_cfg[pps.index];

    cfg->enable = pps.enabled;
    cfg->ctrl_cmd = pps.ctrl_cmd;
    cfg->trgtmodsel = pps.trgtmodsel;
    cfg->target_time[0] = pps.target_time[0];
    cfg->target_time[1] = pps.target_time[1];
    cfg->interval = pps.interval / ptp_period;
    cfg->width = pps.width / ptp_period;

    return xj3_pps_configuration(priv, pps.index);
}

static int xj3_extension_ioctl(struct xj3_priv *priv, void __user *data) {
    u32 tx_cnt = priv->plat->tx_queues_to_use;
    int txfifosz = priv->plat->tx_fifo_size;
    struct xj3_qmode_cfg mode;
    struct xj3_cbs_cfg cbs;
    u32 txmode = 0;
    u8 qmode;
    u32 cmd;

    if (!capable(CAP_NET_ADMIN)) return -EPERM;

    if (copy_from_user(&cmd, data, sizeof(cmd))) return -EFAULT;

    if (txfifosz == 0) txfifosz = priv->dma_cap.tx_fifo_size;

    txfifosz /= tx_cnt;
    pr_err("cmd %d",cmd);
    switch (cmd) {

        case STMMAC_GET_QMODE:
            return xj3_ioctl_get_qmode(priv, data);

        case STMMAC_SET_QMODE:
            if (copy_from_user(&mode, data, sizeof(mode))) return -EFAULT;
            if (mode.queue_idx >= tx_cnt) return -EINVAL;

            txmode = SF_DMA_MODE;

            switch (mode.queue_mode) {
                case STMMAC_QMODE_DCB:
                    qmode = MTL_QUEUE_DCB;
                    break;
                case STMMAC_QMODE_AVB:
                    qmode = MTL_QUEUE_AVB;
                    break;
                default:
                    return -EINVAL;
            }

            priv->plat->tx_queues_cfg[mode.queue_idx].mode_to_use = qmode;

            xj3_dma_tx_chan_op_mode(priv->ioaddr, txmode, mode.queue_idx,
                                    txfifosz, qmode);

            break;

        case STMMAC_SET_CBS:
            if (copy_from_user(&cbs, data, sizeof(cbs))) return -EFAULT;
            if (cbs.queue_idx >= tx_cnt) return -EINVAL;

            qmode = priv->plat->tx_queues_cfg[cbs.queue_idx].mode_to_use;
            if (qmode != MTL_QUEUE_AVB) return -EINVAL;

            priv->plat->tx_queues_cfg[cbs.queue_idx].send_slope =
                cbs.send_slope;
            priv->plat->tx_queues_cfg[cbs.queue_idx].idle_slope =
                cbs.idle_slope;
            priv->plat->tx_queues_cfg[cbs.queue_idx].high_credit =
                cbs.high_credit;
            priv->plat->tx_queues_cfg[cbs.queue_idx].low_credit =
                cbs.low_credit;
            priv->plat->tx_queues_cfg[cbs.queue_idx].percentage =
                cbs.percentage;

            xj3_dma_config_cbs(priv, cbs.send_slope, cbs.idle_slope,
                               cbs.high_credit, cbs.low_credit, cbs.queue_idx);
            break;
        case STMMAC_GET_CBS:
            if (copy_from_user(&cbs, data, sizeof(cbs))) return -EFAULT;
            if (cbs.queue_idx >= tx_cnt) return -EINVAL;

            cbs.send_slope =
                priv->plat->tx_queues_cfg[cbs.queue_idx].send_slope;
            cbs.idle_slope =
                priv->plat->tx_queues_cfg[cbs.queue_idx].idle_slope;
            cbs.high_credit =
                priv->plat->tx_queues_cfg[cbs.queue_idx].high_credit;
            cbs.low_credit =
                priv->plat->tx_queues_cfg[cbs.queue_idx].low_credit;
            cbs.percentage =
                priv->plat->tx_queues_cfg[cbs.queue_idx].percentage;

            if (copy_to_user(data, &cbs, sizeof(cbs))) return -EFAULT;
            break;

        case STMMAC_GET_EST:
            return xj3_get_est(priv, data);

        case STMMAC_SET_EST:
            return xj3_set_est(priv, data);
        case STMMAC_GET_PPS:
            return xj3_ioctl_get_pps(priv, data);
        case STMMAC_SET_PPS:
            return xj3_ioctl_set_pps(priv, data);

        default:
            return -EINVAL;
    }

    return 0;
}

static int xj3_tx_status(struct net_device_stats *data,
                         struct xj3_extra_stats *x, struct dma_desc *p,
                         void __iomem *ioaddr) {
    struct net_device_stats *stats = (struct net_device_stats *)data;
    unsigned int tdes3;
    int ret = tx_done;

    tdes3 = le32_to_cpu(p->des3);

    if (tdes3 & TDES3_OWN) return tx_dma_own;

    if (!(tdes3 & TDES3_LAST_DESCRIPTOR)) {
        return tx_not_ls;
    }
    if (tdes3 & TDES3_ERROR_SUMMARY) {
        if (tdes3 & TDES3_JABBER_TIMEOUT) {
            x->tx_jabber++;
        }
        if (tdes3 & TDES3_PACKET_FLUSHED) {
            x->tx_frame_flushed++;
        }

        if (tdes3 & TDES3_LOSS_CARRIER) {
            x->tx_losscarrier++;
            stats->tx_carrier_errors++;
        }

        if (tdes3 & TDES3_NO_CARRIER) {
            x->tx_carrier++;
            stats->tx_carrier_errors++;
        }

        if ((tdes3 & TDES3_LATE_COLLISION) ||
            (tdes3 & TDES3_EXCESSIVE_COLLISION)) {
            stats->collisions += (tdes3 & TDES3_COLLISION_COUNT_MASK) >>
                                 TDES3_COLLISION_COUNT_SHIFT;
        }

        if (tdes3 & TDES3_UNDERFLOW_ERROR) {
            x->tx_underflow++;
        }

        if (tdes3 & TDES3_IP_HDR_ERROR) {
            x->tx_ip_header_error++;
        }

        if (tdes3 & TDES3_PAYLOAD_ERROR) {
            x->tx_payload_error++;
        }
        ret = tx_err;
    }

    if (tdes3 & TDES3_DEFERRED) {
        x->tx_deferred++;
    }

    return ret;
}

static int xj3_get_tx_timestamp_status(struct dma_desc *p) {
    if (le32_to_cpu(p->des3) & TDES3_CONTEXT_TYPE) return 0;

    if (le32_to_cpu(p->des3) & TDES3_TIMESTAMP_STATUS) return 1;

    return 0;
}

static void xj3_get_tx_hwstamp(struct xj3_priv *priv, struct dma_desc *p,
                               struct sk_buff *skb) {
    struct skb_shared_hwtstamps shhwtstamp;
    u64 ns;
    int status;

    if (!priv->hwts_tx_en) return;

    if (!skb || !((skb_shinfo(skb)->tx_flags & SKBTX_IN_PROGRESS))) return;

    status = xj3_get_tx_timestamp_status(p);
    if (status) {
        ns = le32_to_cpu(p->des0);
        ns += le32_to_cpu(p->des1) * 1000000000ULL;
        memset(&shhwtstamp, 0, sizeof(struct skb_shared_hwtstamps));
        shhwtstamp.hwtstamp = ns_to_ktime(ns);
        skb_tstamp_tx(skb, &shhwtstamp);
    }

    return;
}

static inline u32 xj3_tx_avail(struct xj3_priv *priv, u32 queue) {
    struct xj3_tx_queue *tx_q = &priv->tx_queue[queue];
    u32 avail;

    /*  dev_info(priv->device, "%s, dirty_tx:0x%x, cur_tx:0x%x\n", __func__, \
            tx_q->dirty_tx, tx_q->cur_tx);
    */
    if (tx_q->dirty_tx > tx_q->cur_tx)
        avail = tx_q->dirty_tx - tx_q->cur_tx - 1;
    else
        avail = DMA_TX_SIZE - tx_q->cur_tx + tx_q->dirty_tx - 1;
    return avail;
}

static void xj3_tx_clean(struct xj3_priv *priv, u32 queue) {
    struct xj3_tx_queue *tx_q = &priv->tx_queue[queue];
    unsigned int bytes_compl = 0, pkts_compl = 0;
    unsigned int entry;

    netif_tx_lock(priv->dev);

    priv->xstats.tx_clean++;
    entry = tx_q->dirty_tx;

    while (entry != tx_q->cur_tx) {
        struct sk_buff *skb = tx_q->tx_skbuff[entry];
        struct dma_desc *p;
        int status;

        if (priv->extend_desc)
            p = (struct dma_desc *)(tx_q->dma_etx + entry);
        else
            p = tx_q->dma_tx + entry;

        status =
            xj3_tx_status(&priv->dev->stats, &priv->xstats, p, priv->ioaddr);
        if (status & tx_dma_own) {
            break;
        }

        dma_rmb();

        if (!(status & tx_not_ls)) {
            if (status & tx_err) {
                priv->dev->stats.tx_errors++;
                dev_err(priv->device, "%s: entry %d, des:0x%x 0x%x 0x%x 0x%x\n",
                        __func__, entry, p->des0, p->des1, p->des2, p->des3);
            } else {
                priv->dev->stats.tx_packets++;
                priv->xstats.tx_pkt_n++;
            }
            xj3_get_tx_hwstamp(priv, p, skb);
        }

        if (tx_q->tx_skbuff_dma[entry].buf) {
            if (tx_q->tx_skbuff_dma[entry].map_as_page)
                dma_unmap_page(priv->device, tx_q->tx_skbuff_dma[entry].buf,
                               tx_q->tx_skbuff_dma[entry].len, DMA_TO_DEVICE);
            else
                dma_unmap_single(priv->device, tx_q->tx_skbuff_dma[entry].buf,
                                 tx_q->tx_skbuff_dma[entry].len, DMA_TO_DEVICE);

            tx_q->tx_skbuff_dma[entry].buf = 0;
            tx_q->tx_skbuff_dma[entry].len = 0;
            tx_q->tx_skbuff_dma[entry].map_as_page = false;
        }

        tx_q->tx_skbuff_dma[entry].last_segment = false;
        tx_q->tx_skbuff_dma[entry].is_jumbo = false;

        if (skb != NULL) {
            pkts_compl++;
            bytes_compl += skb->len;
            dev_consume_skb_any(skb);
            tx_q->tx_skbuff[entry] = NULL;
        }

        p->des2 = 0;
        p->des3 = 0;
        entry = XJ3_GET_ENTRY(entry, DMA_TX_SIZE);
    }

    tx_q->dirty_tx = entry;

    netdev_tx_completed_queue(netdev_get_tx_queue(priv->dev, queue), pkts_compl,
                              bytes_compl);

    if (netif_tx_queue_stopped(netdev_get_tx_queue(priv->dev, queue)) &&
        xj3_tx_avail(priv, queue) > STMMAC_TX_THRESH) {
        dev_info(priv->device, "%s: restart transmit\n", __func__);
        netif_tx_wake_queue(netdev_get_tx_queue(priv->dev, queue));
    }

    if (tx_q->dirty_tx != tx_q->cur_tx) {
        mod_timer(&priv->txtimer, HOBOT_COAL_TIMER(priv->tx_coal_timer));
    }

    netif_tx_unlock(priv->dev);
}

static void hobot_tx_timer_arm(struct xj3_priv *priv) {
    mod_timer(&priv->txtimer, HOBOT_COAL_TIMER(priv->tx_coal_timer));
}

static void hobot_tx_timer(struct timer_list *t) {
    struct xj3_priv *priv = from_timer(priv, t, txtimer);
    u32 tx_queues_count = priv->plat->tx_queues_to_use;
    u32 queue;

    for (queue = 0; queue < tx_queues_count; queue++) xj3_tx_clean(priv, queue);
}

static void hobot_init_intr_coalesce(struct xj3_priv *priv) {
    priv->tx_coal_frames = HOBOT_TX_FRAMES;
    priv->tx_coal_timer = HOBOT_COAL_TX_TIMER;
    priv->rx_coal_frames = HOBOT_RX_FRAMES;

    timer_setup(&priv->txtimer, hobot_tx_timer, 0);
    // init_timer(&priv->txtimer);
    priv->txtimer.expires = HOBOT_COAL_TIMER(priv->tx_coal_timer);
    // priv->txtimer.data = (unsigned long)priv;
    // priv->txtimer.function = hobot_tx_timer;
    add_timer(&priv->txtimer);
}

static int xj3_open(struct net_device *ndev) {
    struct xj3_priv *priv = netdev_priv(ndev);
    int ret;

    ret = xj3_init_phy(ndev);
    if (ret < 0) {
        dev_info(priv->device, "%s, init phy error \n", __func__);
        return ret;
    }

    priv->dma_buf_sz = 3000;
    /*reset mss when open*/
    priv->mss = 0;

    ret = alloc_dma_desc_resources(priv);
    if (ret < 0) {
        dev_info(priv->device, "%s, DMA descriptor alloc failed\n", __func__);
        goto dma_desc_error;
    }

    ret = init_dma_desc_rings(ndev);
    if (ret < 0) {
        dev_info(priv->device, "%s: DMA desc rings initalize failed\n",
                 __func__);
        goto init_error;
    }

    ret = xj3_hw_setup(ndev, true);
    if (ret < 0) {
        dev_info(priv->device, "%s, Hw setup failed\n", __func__);
        goto init_error;
    }

    enable_irq(ndev->irq);
#ifdef HOBOT_USE_IRQ_SPLIT
    enable_irq(priv->tx_irq);
    enable_irq(priv->rx_irq);
#endif

    hobot_init_intr_coalesce(priv);
    phy_start(ndev->phydev);
    xj3_enable_all_queues(priv);
    xj3_start_all_queues(priv);

    dev_info(priv->device, "%s: successfully\n", __func__);
    return 0;

init_error:
    free_dma_desc_resources(priv);
dma_desc_error:

    if (ndev->phydev) phy_disconnect(ndev->phydev);

    return ret;
}

static void xj3_stop_all_queues(struct xj3_priv *priv) {
    u32 tx_cnt = priv->plat->tx_queues_to_use;
    u32 queue;

    for (queue = 0; queue < tx_cnt; queue++)
        netif_tx_stop_queue(netdev_get_tx_queue(priv->dev, queue));
}

static void xj3_disable_all_queues(struct xj3_priv *priv) {
    u32 rx_cnt = priv->plat->rx_queues_to_use;
    u32 queue;

    for (queue = 0; queue < rx_cnt; queue++) {
        struct xj3_rx_queue *rx_q = &priv->rx_queue[queue];

        napi_disable(&rx_q->napi);
    }
}

static void xj3_stop_rx_dma(struct xj3_priv *priv, u32 chan) {
    void __iomem *ioaddr = priv->ioaddr;

    u32 value = readl(ioaddr + DMA_CHAN_RX_CONTROL(chan));

    value &= ~DMA_CONTROL_SR;
    writel(value, ioaddr + DMA_CHAN_RX_CONTROL(chan));

    value = readl(ioaddr + GMAC_CONFIG);
    value &= ~GMAC_CONFIG_RE;
    writel(value, ioaddr + GMAC_CONFIG);
}

static void xj3_stop_all_dma(struct xj3_priv *priv) {
    u32 rx_cnt = priv->plat->rx_queues_to_use;
    u32 tx_cnt = priv->plat->tx_queues_to_use;
    u32 chan = 0;

    for (chan = 0; chan < rx_cnt; chan++) xj3_stop_rx_dma(priv, chan);

    for (chan = 0; chan < tx_cnt; chan++) xj3_stop_tx_dma(priv, chan);
}

static void hobot_ptp_unregister(struct xj3_priv *priv) {
    if (priv->ptp_clock) {
        ptp_clock_unregister(priv->ptp_clock);
        priv->ptp_clock = NULL;
    }
}

static void hobot_release_ptp(struct xj3_priv *priv) {
    if (priv->plat->clk_ptp_ref) clk_disable_unprepare(priv->plat->clk_ptp_ref);

    hobot_ptp_unregister(priv);
}

static int xj3_release(struct net_device *ndev) {
    struct xj3_priv *priv = netdev_priv(ndev);
	u32 rx_count = priv->plat->rx_queues_to_use;
	struct xj3_rx_queue *rx_q;
	u32 i;

    if (ndev->phydev) {
        phy_stop(ndev->phydev);
        phy_disconnect(ndev->phydev);
    }

    /* disable_irq: disable an irq and wait for completion
     * so after disable_irq , no interrupt happen.
     */
    disable_irq(ndev->irq);
#ifdef HOBOT_USE_IRQ_SPLIT
    disable_irq(priv->tx_irq);
    disable_irq(priv->rx_irq);
#endif

    xj3_stop_all_queues(priv);

    /* xj3_disable_all_queues->napi_disable ,napi_disable will
     * disable an napi poll and wait for completion.
     * so after this , no poll will be called.
     */
    xj3_disable_all_queues(priv);

	/* cancel all queues delayed work */
	for (i = 0; i < rx_count; i++) {
		rx_q = &priv->rx_queue[i];
		cancel_delayed_work_sync(&rx_q->work);
	}

    del_timer_sync(&priv->txtimer);

    xj3_stop_all_dma(priv);
    xj3_set_mac(priv->ioaddr, false);

    /* free resource is safe , because no interrupt and poll here. */
    free_dma_desc_resources(priv);

    netif_carrier_off(ndev);
    hobot_release_ptp(priv);
    return 0;
}

static void xj3_prepare_tx_desc(struct dma_desc *p, int is_fs, int len,
                                bool csum_flags, int tx_own, bool ls,
                                unsigned int tot_pkt_len) {
    unsigned int tdes3 = le32_to_cpu(p->des3);

    p->des2 |= cpu_to_le32(len & TDES2_BUFFER1_SIZE_MASK);

    tdes3 |= tot_pkt_len & TDES3_PACKET_SIZE_MASK;

    if (is_fs)
        tdes3 |= TDES3_FIRST_DESCRIPTOR;
    else
        tdes3 &= ~TDES3_FIRST_DESCRIPTOR;

    if (csum_flags)
        tdes3 |= (TX_CIC_FULL << TDES3_CHECKSUM_INSERTION_SHIFT);
    else
        tdes3 &= ~(TX_CIC_FULL << TDES3_CHECKSUM_INSERTION_SHIFT);

    if (ls)
        tdes3 |= (TDES3_LAST_DESCRIPTOR);
    else
        tdes3 &= ~(TDES3_LAST_DESCRIPTOR);

    if (tx_own) tdes3 |= TDES3_OWN;

    if (is_fs & tx_own) dma_wmb();

    p->des3 = cpu_to_le32(tdes3);
}

static void xj3_enable_tx_timestamp(struct dma_desc *p) {
    p->des2 |= cpu_to_le32(TDES2_TIMESTAMP_ENABLE);
}

static void xj3_set_mss(struct dma_desc *p, unsigned int mss) {
    p->des0 = 0;
    p->des1 = 0;
    p->des2 = cpu_to_le32(mss);
    p->des3 = cpu_to_le32(TDES3_CONTEXT_TYPE | TDES3_CTXT_TCMSSV);
}

static void xj3_prepare_tso_tx_desc(struct dma_desc *p, int is_fs, int len1,
                                    int len2, bool tx_own, bool ls,
                                    unsigned int tcphdrlen,
                                    unsigned int tcppayloadlen) {
    unsigned int tdes3 = le32_to_cpu(p->des3);

    if (len1) p->des2 |= cpu_to_le32((len1 & TDES2_BUFFER1_SIZE_MASK));

    if (len2)
        p->des2 |= cpu_to_le32((len2 << TDES2_BUFFER2_SIZE_MASK_SHIFT) &
                               TDES2_BUFFER2_SIZE_MASK);

    if (is_fs) {
        tdes3 |= TDES3_FIRST_DESCRIPTOR | TDES3_TCP_SEGMENTATION_ENABLE |
                 ((tcphdrlen << TDES3_HDR_LEN_SHIFT) & TDES3_SLOT_NUMBER_MASK) |
                 ((tcppayloadlen & TDES3_TCP_PKT_PAYLOAD_MASK));

    } else {
        tdes3 &= ~TDES3_FIRST_DESCRIPTOR;
    }

    if (ls)
        tdes3 |= TDES3_LAST_DESCRIPTOR;
    else
        tdes3 &= ~TDES3_LAST_DESCRIPTOR;

    if (tx_own) tdes3 |= TDES3_OWN;

    if (is_fs && tx_own) dma_wmb();

    p->des3 = cpu_to_le32(tdes3);
}

static void xj3_tso_allocator(struct xj3_priv *priv, unsigned int des,
                              int total_len, bool last_segment, u32 queue) {
    struct xj3_tx_queue *tx_q = &priv->tx_queue[queue];
    struct dma_desc *desc;
    u32 buff_size;
    int tmp_len;

    tmp_len = total_len;

#if 0
	dev_info(priv->device, "%s, and tmp_len:%d, total_len:%d\n",
             __func__, tmp_len, total_len);
#endif

    while (tmp_len > 0) {
        tx_q->cur_tx = XJ3_GET_ENTRY(tx_q->cur_tx, DMA_TX_SIZE);
        desc = tx_q->dma_tx + tx_q->cur_tx;
        desc->des0 = cpu_to_le32(des + (total_len - tmp_len));
        buff_size = tmp_len >= TSO_MAX_BUFF_SIZE ? TSO_MAX_BUFF_SIZE : tmp_len;

        xj3_prepare_tso_tx_desc(
            desc, 0, buff_size, 0, 1,
            (last_segment) && (tmp_len <= TSO_MAX_BUFF_SIZE), 0, 0);
        tmp_len -= TSO_MAX_BUFF_SIZE;
    }
}

static netdev_tx_t xj3_tso_xmit(struct sk_buff *skb, struct net_device *ndev) {
    struct dma_desc *desc, *first, *mss_desc = NULL;
    struct xj3_priv *priv = netdev_priv(ndev);
    int nfrags = skb_shinfo(skb)->nr_frags;
    u32 queue = skb_get_queue_mapping(skb);
    unsigned int first_entry, des;
    struct xj3_tx_queue *tx_q;
    int tmp_pay_len = 0;
    u32 pay_len, mss;
    u8 proto_hdr_len;
    int i;

    tx_q = &priv->tx_queue[queue];

    proto_hdr_len = skb_transport_offset(skb) + tcp_hdrlen(skb);

    dev_dbg(priv->device, "%s, and transport offset:%d, tcphdrlen:%d\n",
            __func__, skb_transport_offset(skb), tcp_hdrlen(skb));

    if (unlikely(xj3_tx_avail(priv, queue) <
                 (((skb->len - proto_hdr_len) / TSO_MAX_BUFF_SIZE + 1)))) {
        if (!netif_tx_queue_stopped(netdev_get_tx_queue(ndev, queue))) {
            netif_tx_stop_queue(netdev_get_tx_queue(priv->dev, queue));
            dev_info(priv->device, "tx ring full when queue awake\n");
        }
#ifdef CONFIG_HOBOT_DMC_CLK
        clear_bit(HOBOT_IN_XMIT, &priv->state);
#endif
        return NETDEV_TX_BUSY;
    }

    pay_len = skb_headlen(skb) - proto_hdr_len;
    mss = skb_shinfo(skb)->gso_size;
    if (mss != priv->mss) {
        mss_desc = tx_q->dma_tx + tx_q->cur_tx;
        xj3_set_mss(mss_desc, mss);
        priv->mss = mss;
        tx_q->cur_tx = XJ3_GET_ENTRY(tx_q->cur_tx, DMA_TX_SIZE);
    }

    first_entry = tx_q->cur_tx;
    desc = tx_q->dma_tx + first_entry;
    first = desc;

    des = dma_map_single(priv->device, skb->data, skb_headlen(skb),
                         DMA_TO_DEVICE);
    if (dma_mapping_error(priv->device, des)) goto dma_map_err;

    tx_q->tx_skbuff_dma[first_entry].buf = des;
    tx_q->tx_skbuff_dma[first_entry].len = skb_headlen(skb);

    first->des0 = cpu_to_le32(des);

    if (pay_len) first->des1 = cpu_to_le32(des + proto_hdr_len);

    tmp_pay_len = pay_len - TSO_MAX_BUFF_SIZE;

    xj3_tso_allocator(priv, des, tmp_pay_len, (nfrags == 0), queue);

    for (i = 0; i < nfrags; i++) {
        const skb_frag_t *frag = &skb_shinfo(skb)->frags[i];

        des = skb_frag_dma_map(priv->device, frag, 0, skb_frag_size(frag),
                               DMA_TO_DEVICE);
        if (dma_mapping_error(priv->device, des)) goto dma_map_err;

        xj3_tso_allocator(priv, des, skb_frag_size(frag), (i == nfrags - 1),
                          queue);

        tx_q->tx_skbuff_dma[tx_q->cur_tx].buf = des;
        tx_q->tx_skbuff_dma[tx_q->cur_tx].len = skb_frag_size(frag);
        tx_q->tx_skbuff[tx_q->cur_tx] = NULL;
        tx_q->tx_skbuff_dma[tx_q->cur_tx].map_as_page = true;
    }

    tx_q->tx_skbuff_dma[tx_q->cur_tx].last_segment = true;
    tx_q->tx_skbuff[tx_q->cur_tx] = skb;

    tx_q->cur_tx = XJ3_GET_ENTRY(tx_q->cur_tx, DMA_TX_SIZE);
    if (unlikely(xj3_tx_avail(priv, queue) <= (MAX_SKB_FRAGS + 1))) {
        netif_tx_stop_queue(netdev_get_tx_queue(priv->dev, queue));
    }

    ndev->stats.tx_bytes += skb->len;
    priv->xstats.tx_tso_frames++;
    priv->xstats.tx_tso_nfrags += nfrags;

    priv->tx_count_frames += nfrags + 1;
    if (likely(priv->tx_coal_frames > priv->tx_count_frames)) {
        mod_timer(&priv->txtimer, HOBOT_COAL_TIMER(priv->tx_coal_timer));
    } else {
        priv->tx_count_frames = 0;
        desc->des2 |= cpu_to_le32(TDES2_INTERRUPT_ON_COMPLETION);
        priv->xstats.tx_set_ic_bit++;
    }

    skb_tx_timestamp(skb);

    if (unlikely((skb_shinfo(skb)->tx_flags & SKBTX_HW_TSTAMP) &&
                 priv->hwts_tx_en)) {
        skb_shinfo(skb)->tx_flags |= SKBTX_IN_PROGRESS;
        xj3_enable_tx_timestamp(first);
    }

    xj3_prepare_tso_tx_desc(first, 1, proto_hdr_len, pay_len, 1,
                            tx_q->tx_skbuff_dma[first_entry].last_segment,
                            tcp_hdrlen(skb) / 4, (skb->len - proto_hdr_len));

    if (mss_desc) {
        dma_wmb();
        mss_desc->des3 |= cpu_to_le32(TDES3_OWN);
    }
    dma_wmb();

    netdev_tx_sent_queue(netdev_get_tx_queue(ndev, queue), skb->len);
    tx_q->tx_tail_addr = tx_q->dma_tx_phy + (tx_q->cur_tx * sizeof(*desc));
    xj3_set_tx_tail_ptr(priv->ioaddr, tx_q->tx_tail_addr, queue);

    hobot_tx_timer_arm(priv);
#ifdef CONFIG_HOBOT_DMC_CLK
    clear_bit(HOBOT_IN_XMIT, &priv->state);
#endif
    return NETDEV_TX_OK;

dma_map_err:
    dev_kfree_skb(skb);
    priv->dev->stats.tx_dropped++;
#ifdef CONFIG_HOBOT_DMC_CLK
    clear_bit(HOBOT_IN_XMIT, &priv->state);
#endif
    return NETDEV_TX_OK;
}

static netdev_tx_t xj3_xmit(struct sk_buff *skb, struct net_device *ndev) {
    struct xj3_priv *priv = netdev_priv(ndev);
    unsigned int nopaged_len = skb_headlen(skb);
    int nfrags = skb_shinfo(skb)->nr_frags;
    int entry;
    unsigned int first_entry;
    u32 queue = skb_get_queue_mapping(skb);
    struct xj3_tx_queue *tx_q;
    struct dma_desc *desc, *first;
    unsigned int des, enh_desc;
    int i, csum_insert = 0, is_jumbo = 0;
    struct timespec64 now;
#ifdef CONFIG_HOBOT_DMC_CLK
    set_bit(HOBOT_IN_XMIT, &priv->state);
#endif
    tx_q = &priv->tx_queue[queue];
    if (skb_is_gso(skb) && priv->tso) {
        if (skb_shinfo(skb)->gso_type & (SKB_GSO_TCPV4 | SKB_GSO_TCPV6)) {
            dev_dbg(priv->device,
                    "len:%d, gso:%d, priv->tso:%d, shinfo->gso_type:0x%x\n",
                    skb->len, skb_is_gso(skb), priv->tso,
                    skb_shinfo(skb)->gso_type);
            skb_set_queue_mapping(skb, 0);
            return xj3_tso_xmit(skb, ndev);
        }
    }

    if ((xj3_tx_avail(priv, queue) < nfrags + 1)) {
        if (!netif_tx_queue_stopped(netdev_get_tx_queue(ndev, queue))) {
            netif_tx_stop_queue(netdev_get_tx_queue(priv->dev, queue));
            dev_info(priv->device, "%s and tx stop queue\n", __func__);
        }
#ifdef CONFIG_HOBOT_DMC_CLK
        clear_bit(HOBOT_IN_XMIT, &priv->state);
#endif
        return NETDEV_TX_BUSY;
    }

    entry = tx_q->cur_tx;
    first_entry = entry;

    dev_dbg(priv->device, "%s,tx queue:%d, entry:%d\n", __func__, queue, entry);
    csum_insert = (skb->ip_summed == CHECKSUM_PARTIAL);

    if (priv->extend_desc)
        desc = (struct dma_desc *)(tx_q->dma_etx + entry);
    else
        desc = tx_q->dma_tx + entry;

    first = desc;

    enh_desc = priv->plat->enh_desc;

    if (enh_desc) {
        dev_info(priv->device, "%s, and enh_desc", __func__);
    }

    for (i = 0; i < nfrags; i++) {
        const skb_frag_t *frag = &skb_shinfo(skb)->frags[i];
        int len = skb_frag_size(frag);
        bool last_segment = (i == (nfrags - 1));

        entry = XJ3_GET_ENTRY(entry, DMA_TX_SIZE);

        if (priv->extend_desc)
            desc = (struct dma_desc *)(tx_q->dma_etx + entry);
        else
            desc = tx_q->dma_tx + entry;

        des = skb_frag_dma_map(priv->device, frag, 0, len, DMA_TO_DEVICE);
        if (dma_mapping_error(priv->device, des)) {
            goto dma_map_err;
        }

        tx_q->tx_skbuff[entry] = NULL;
        tx_q->tx_skbuff_dma[entry].buf = des;

        desc->des0 = cpu_to_le32(des);

        tx_q->tx_skbuff_dma[entry].map_as_page = true;
        tx_q->tx_skbuff_dma[entry].len = len;
        tx_q->tx_skbuff_dma[entry].last_segment = last_segment;

        xj3_prepare_tx_desc(desc, 0, len, csum_insert, 1, last_segment,
                            skb->len);
    }

    tx_q->tx_skbuff[entry] = skb;
    entry = XJ3_GET_ENTRY(entry, DMA_TX_SIZE);
    tx_q->cur_tx = entry;

    if (netif_msg_pktdata(priv)) {
        void *tx_head;

        if (priv->extend_desc)
            tx_head = (void *)tx_q->dma_etx;
        else
            tx_head = (void *)tx_q->dma_tx;

        dev_info(priv->device, "frame to be transmited\n");
    }

    if (xj3_tx_avail(priv, queue) <= (MAX_SKB_FRAGS + 1)) {  // MAX_SKB_FRAGS:17
        dev_info(priv->device, "%s, stop transmited packets\n", __func__);
        netif_tx_stop_queue(netdev_get_tx_queue(priv->dev, queue));
    }

    ndev->stats.tx_bytes += skb->len;
    priv->tx_count_frames += nfrags + 1;

    if (likely(priv->tx_coal_frames > priv->tx_count_frames)) {
        mod_timer(&priv->txtimer, HOBOT_COAL_TIMER(priv->tx_coal_timer));

    } else {
        priv->tx_count_frames = 0;
        desc->des2 |= cpu_to_le32(TDES2_INTERRUPT_ON_COMPLETION);
        priv->xstats.tx_set_ic_bit++;
    }

    skb_tx_timestamp(skb);

    if (!is_jumbo) {
        bool last_segment = (nfrags == 0);
        des =
            dma_map_single(priv->device, skb->data, nopaged_len, DMA_TO_DEVICE);
        if (dma_mapping_error(priv->device, des)) goto dma_map_err;

        tx_q->tx_skbuff_dma[first_entry].buf = des;
        first->des0 = cpu_to_le32(des);

        tx_q->tx_skbuff_dma[first_entry].len = nopaged_len;
        tx_q->tx_skbuff_dma[first_entry].last_segment = last_segment;

        if ((skb_shinfo(skb)->tx_flags & SKBTX_HW_TSTAMP) && priv->hwts_tx_en) {
            skb_shinfo(skb)->tx_flags |= SKBTX_IN_PROGRESS;
            xj3_enable_tx_timestamp(first);

            ktime_get_real_ts64(&now);
        }

        xj3_prepare_tx_desc(first, 1, nopaged_len, csum_insert, 1, last_segment,
                            skb->len);
        dma_wmb();
    }

    netdev_tx_sent_queue(netdev_get_tx_queue(ndev, queue), skb->len);
    tx_q->tx_tail_addr = tx_q->dma_tx_phy + (tx_q->cur_tx * sizeof(*desc));
    xj3_set_tx_tail_ptr(priv->ioaddr, tx_q->tx_tail_addr, queue);

    hobot_tx_timer_arm(priv);
    netif_trans_update(ndev);
#ifdef CONFIG_HOBOT_DMC_CLK
    clear_bit(HOBOT_IN_XMIT, &priv->state);
#endif
    return NETDEV_TX_OK;

dma_map_err:
    dev_kfree_skb(skb);
    priv->dev->stats.tx_dropped++;
    return NETDEV_TX_OK;
}

static int xj3_ptp_get_ts_config(struct net_device *ndev, struct ifreq *rq) {
    struct xj3_priv *priv = netdev_priv(ndev);
    struct hwtstamp_config *config = &priv->tstamp_config;

    if (!(priv->dma_cap.time_stamp || priv->dma_cap.atime_stamp))
        return -EOPNOTSUPP;

    return copy_to_user(rq->ifr_data, config, sizeof(*config)) ? -EFAULT : 0;
}

static int xj3_ptp_set_ts_config(struct net_device *ndev, struct ifreq *ifr) {
    struct xj3_priv *priv = netdev_priv(ndev);
    struct hwtstamp_config config;
    struct timespec64 now;
    u64 temp = 0;
    u32 ptp_v2 = 0;
    u32 tstamp_all = 0;
    u32 ptp_over_ipv4_udp = 0;
    u32 ptp_over_ipv6_udp = 0;
    u32 ptp_over_ethernet = 0;
    u32 snap_type_sel = 0;
    u32 ts_master_en = 0;
    u32 ts_event_en = 0;
    u32 sec_inc = 0;
    u32 value = 0;
    bool xmac;
    unsigned long flags;
    xmac = 1;

    if (!(priv->dma_cap.time_stamp || priv->adv_ts)) {
        dev_info(priv->device, "No support for HW time stamping\n");
        priv->hwts_tx_en = 0;
        priv->hwts_rx_en = 0;
        return -EOPNOTSUPP;
    }

    if (copy_from_user(&config, ifr->ifr_data, sizeof(config))) return -EFAULT;

    if (config.flags) return -EINVAL;

    if (config.tx_type != HWTSTAMP_TX_OFF && config.tx_type != HWTSTAMP_TX_ON)
        return -ERANGE;

    if (priv->adv_ts) {
        switch (config.rx_filter) {
            case HWTSTAMP_FILTER_NONE:
                config.rx_filter = HWTSTAMP_FILTER_NONE;
                break;
            case HWTSTAMP_FILTER_PTP_V1_L4_EVENT:
                config.rx_filter = HWTSTAMP_FILTER_PTP_V1_L4_EVENT;
                snap_type_sel = PTP_GMAC4_TCR_SNAPTYPSEL_1;
                ptp_over_ipv4_udp = PTP_TCR_TSIPV4ENA;
                ptp_over_ipv6_udp = PTP_TCR_TSIPV6ENA;
                break;
            case HWTSTAMP_FILTER_PTP_V1_L4_SYNC:
                config.rx_filter = HWTSTAMP_FILTER_PTP_V1_L4_SYNC;
                ts_event_en = PTP_TCR_TSEVNTENA;
                ptp_over_ipv4_udp = PTP_TCR_TSIPV4ENA;
                ptp_over_ipv6_udp = PTP_TCR_TSIPV6ENA;
                break;

            case HWTSTAMP_FILTER_PTP_V1_L4_DELAY_REQ:
                config.rx_filter = HWTSTAMP_FILTER_PTP_V1_L4_DELAY_REQ;
                ts_master_en = PTP_TCR_TSMSTRENA;
                ts_event_en = PTP_TCR_TSEVNTENA;
                ptp_over_ipv4_udp = PTP_TCR_TSIPV4ENA;
                ptp_over_ipv6_udp = PTP_TCR_TSIPV6ENA;
                break;
            case HWTSTAMP_FILTER_PTP_V2_L4_EVENT:
                config.rx_filter = HWTSTAMP_FILTER_PTP_V2_L4_EVENT;
                ptp_v2 = PTP_TCR_TSVER2ENA;
                snap_type_sel = PTP_GMAC4_TCR_SNAPTYPSEL_1;
                ptp_over_ipv4_udp = PTP_TCR_TSIPV4ENA;
                ptp_over_ipv6_udp = PTP_TCR_TSIPV6ENA;
                break;
            case HWTSTAMP_FILTER_PTP_V2_L4_SYNC:
                config.rx_filter = HWTSTAMP_FILTER_PTP_V2_L4_SYNC;
                ptp_v2 = PTP_TCR_TSVER2ENA;
                ts_event_en = PTP_TCR_TSEVNTENA;
                ptp_over_ipv4_udp = PTP_TCR_TSIPV4ENA;
                ptp_over_ipv6_udp = PTP_TCR_TSIPV6ENA;
                break;

            case HWTSTAMP_FILTER_PTP_V2_L4_DELAY_REQ:
                config.rx_filter = HWTSTAMP_FILTER_PTP_V2_L4_DELAY_REQ;
                ptp_v2 = PTP_TCR_TSVER2ENA;
                ts_master_en = PTP_TCR_TSMSTRENA;
                ptp_v2 = PTP_TCR_TSVER2ENA;
                ptp_over_ipv4_udp = PTP_TCR_TSIPV4ENA;
                ptp_over_ipv6_udp = PTP_TCR_TSIPV6ENA;
                break;
            case HWTSTAMP_FILTER_PTP_V2_EVENT:
                config.rx_filter = HWTSTAMP_FILTER_PTP_V2_EVENT;
                ptp_v2 = PTP_TCR_TSVER2ENA;
                snap_type_sel = PTP_GMAC4_TCR_SNAPTYPSEL_1;
                ptp_over_ipv4_udp = PTP_TCR_TSIPV4ENA;
                ptp_over_ipv6_udp = PTP_TCR_TSIPV6ENA;
                ptp_over_ethernet = PTP_TCR_TSIPENA;
                break;

            case HWTSTAMP_FILTER_PTP_V2_SYNC:
                config.rx_filter = HWTSTAMP_FILTER_PTP_V2_SYNC;
                ptp_v2 = PTP_TCR_TSVER2ENA;
                ts_event_en = PTP_TCR_TSEVNTENA;
                ptp_over_ipv4_udp = PTP_TCR_TSIPV4ENA;
                ptp_over_ipv6_udp = PTP_TCR_TSIPV6ENA;
                ptp_over_ethernet = PTP_TCR_TSIPENA;
                break;

            case HWTSTAMP_FILTER_PTP_V2_DELAY_REQ:
                config.rx_filter = HWTSTAMP_FILTER_PTP_V2_SYNC;
                ts_master_en = PTP_TCR_TSMSTRENA;
                ptp_v2 = PTP_TCR_TSVER2ENA;
                ts_event_en = PTP_TCR_TSEVNTENA;
                ptp_over_ipv4_udp = PTP_TCR_TSIPV4ENA;
                ptp_over_ipv6_udp = PTP_TCR_TSIPV6ENA;
                ptp_over_ethernet = PTP_TCR_TSIPENA;
                break;
            case HWTSTAMP_FILTER_NTP_ALL:
            case HWTSTAMP_FILTER_ALL:
                config.rx_filter = HWTSTAMP_FILTER_ALL;
                tstamp_all = PTP_TCR_TSENALL;
                break;
            default:
                return -ERANGE;
        }

    } else {
        switch (config.rx_filter) {
            case HWTSTAMP_FILTER_NONE:
                config.rx_filter = HWTSTAMP_FILTER_NONE;
                break;
            default:
                config.rx_filter = HWTSTAMP_FILTER_PTP_V1_L4_EVENT;
                break;
        }
    }

    priv->hwts_rx_en = ((config.rx_filter == HWTSTAMP_FILTER_NONE) ? 0 : 1);
    priv->hwts_tx_en = config.tx_type == HWTSTAMP_TX_ON;

    if (!priv->hwts_tx_en && !priv->hwts_rx_en) {
        xj3_config_hw_tstamping(priv, 0);
    } else {
        value =
            (PTP_TCR_TSENA | PTP_TCR_TSCFUPDT | PTP_TCR_TSCTRLSSR | tstamp_all |
             ptp_v2 | ptp_over_ethernet | ptp_over_ipv6_udp |
             ptp_over_ipv4_udp | ts_event_en | ts_master_en | snap_type_sel);

        value |= (1 << 19) | (1 << 14);
        value &= ~(1 << 17);
        dev_dbg(priv->device, "%s,and set value is 0x%x\n", __func__, value);
        xj3_config_hw_tstamping(priv, value);

        sec_inc = xj3_config_sub_second_increment(
            priv, priv->plat->clk_ptp_rate, xmac);

        temp = div_u64(1000000000ULL, sec_inc);
        priv->sub_second_inc = sec_inc;
        priv->systime_flags = value;
        temp = (u64)(temp << 32);
        priv->default_addend = div_u64(temp, priv->plat->clk_ptp_rate);

        spin_lock_irqsave(&priv->ptp_lock, flags);
        xj3_config_addend(priv, priv->default_addend);

        ktime_get_real_ts64(&now);

        xj3_init_systime(priv, (u32)now.tv_sec, now.tv_nsec);
        spin_unlock_irqrestore(&priv->ptp_lock, flags);
    }

    memcpy(&priv->tstamp_config, &config, sizeof(config));
    return copy_to_user(ifr->ifr_data, &config, sizeof(config)) ? -EFAULT : 0;
}

static int xj3_ioctl(struct net_device *ndev, struct ifreq *rq, int cmd) {
    struct xj3_priv *priv = netdev_priv(ndev);
    int ret = -EOPNOTSUPP;

    if (!netif_running(ndev)) return -EINVAL;

    switch (cmd) {
        case SIOCGMIIPHY:
        case SIOCGMIIREG:
        case SIOCSMIIREG:
            if (!ndev->phydev)
                return -EINVAL;
            ret = phy_mii_ioctl(ndev->phydev, rq, cmd);
            break;
        case SIOCSTIOCTL:
            if (!priv || !rq) return -EINVAL;

            ret = xj3_extension_ioctl(priv, rq->ifr_data);
            break;
        case SIOCGHWTSTAMP:
            return xj3_ptp_get_ts_config(ndev, rq);
        case SIOCSHWTSTAMP:
            return xj3_ptp_set_ts_config(ndev, rq);
        default:
            break;
    }

    return ret;
}

static void xj3_set_rx_mode(struct net_device *ndev) {
    struct xj3_priv *priv = netdev_priv(ndev);
    void __iomem *ioaddr = priv->ioaddr;
    unsigned int value = 0;

    if (ndev->flags & IFF_PROMISC) {
        value = GMAC_PACKET_FILTER_PR;
    } else if ((ndev->flags & IFF_ALLMULTI) ||
               (netdev_mc_count(ndev) > HASH_TABLE_SIZE)) {
        value = GMAC_PACKET_FILTER_PM;
        writel(0xffffffff, ioaddr + GMAC_HASH_TAB_0_31);
        writel(0xffffffff, ioaddr + GMAC_HASH_TAB_32_63);
    } else if (!netdev_mc_empty(ndev)) {
        u32 mc_filter[2];
        struct netdev_hw_addr *ha;

        value = GMAC_PACKET_FILTER_HMC;
        memset(mc_filter, 0, sizeof(mc_filter));
        netdev_for_each_mc_addr(ha, ndev) {
            int bit_nr = (bitrev32(~crc32_le(~0, ha->addr, 6)) >> 26);
            mc_filter[bit_nr >> 5] |= (1 << (bit_nr & 0x1F));
        }

        writel(mc_filter[0], ioaddr + GMAC_HASH_TAB_0_31);
        writel(mc_filter[1], ioaddr + GMAC_HASH_TAB_32_63);
    }

    if (netdev_uc_count(ndev) > GMAC_MAX_PERFECT_ADDRESSES) {
        value |= GMAC_PACKET_FILTER_PR;
    } else if (!netdev_uc_empty(ndev)) {
        int reg = 1;
        struct netdev_hw_addr *ha;

        netdev_for_each_uc_addr(ha, ndev) {
            xj3_set_umac_addr(priv->ioaddr, ha->addr, reg);
            reg++;
        }
    }

    writel(value, ioaddr + GMAC_PACKET_FILTER);
}
static int xj3_dev_siocdevprivate(struct net_device *dev,
						      struct ifreq *ifr,
						      void __user *data, int cmd) {
    struct xj3_priv *priv = netdev_priv(dev);
    struct xj3_cbs_cfg cbs;
    u32 tx_cnt = priv->plat->tx_queues_to_use;
    int ret = -EOPNOTSUPP;
    u32 user_cmd;
    u8 qmode;

    data = ifr->ifr_data;
    if (!netif_running(dev)) return -EINVAL;
    if (cmd == SIOCSTIOCTL) {
        if (!priv || !ifr) return -EINVAL;
        if (copy_from_user(&user_cmd, data, sizeof(user_cmd))) return -EFAULT;

        switch (user_cmd) {
            case STMMAC_GET_EST:
                return xj3_get_est(priv, data);

            case STMMAC_SET_EST:
                return xj3_set_est(priv, data);

            case STMMAC_SET_CBS:
                if (copy_from_user(&cbs, data, sizeof(cbs))) return -EFAULT;
                if (cbs.queue_idx >= tx_cnt) return -EINVAL;

                qmode = priv->plat->tx_queues_cfg[cbs.queue_idx].mode_to_use;
                if (qmode != MTL_QUEUE_AVB) return -EINVAL;

                priv->plat->tx_queues_cfg[cbs.queue_idx].send_slope =
                    cbs.send_slope;
                priv->plat->tx_queues_cfg[cbs.queue_idx].idle_slope =
                    cbs.idle_slope;
                priv->plat->tx_queues_cfg[cbs.queue_idx].high_credit =
                    cbs.high_credit;
                priv->plat->tx_queues_cfg[cbs.queue_idx].low_credit =
                    cbs.low_credit;
                priv->plat->tx_queues_cfg[cbs.queue_idx].percentage =
                    cbs.percentage;

                xj3_dma_config_cbs(priv, cbs.send_slope, cbs.idle_slope,
                                cbs.high_credit, cbs.low_credit, cbs.queue_idx);
                ret = 0;
                break;
            case STMMAC_GET_CBS:
                if (copy_from_user(&cbs, data, sizeof(cbs))) return -EFAULT;
                if (cbs.queue_idx >= tx_cnt) return -EINVAL;

                cbs.send_slope =
                    priv->plat->tx_queues_cfg[cbs.queue_idx].send_slope;
                cbs.idle_slope =
                    priv->plat->tx_queues_cfg[cbs.queue_idx].idle_slope;
                cbs.high_credit =
                    priv->plat->tx_queues_cfg[cbs.queue_idx].high_credit;
                cbs.low_credit =
                    priv->plat->tx_queues_cfg[cbs.queue_idx].low_credit;
                cbs.percentage =
                    priv->plat->tx_queues_cfg[cbs.queue_idx].percentage;

                if (copy_to_user(data, &cbs, sizeof(cbs))) return -EFAULT;
                ret = 0;
                break;
        }
    }

    return ret;
}
static const struct net_device_ops xj3_netdev_ops = {
    .ndo_open = xj3_open,
    .ndo_stop = xj3_release,
    .ndo_start_xmit = xj3_xmit,
    .ndo_do_ioctl = xj3_ioctl,
    .ndo_eth_ioctl = xj3_ioctl,
    .ndo_set_rx_mode = xj3_set_rx_mode,
    .ndo_set_mac_address = eth_mac_addr,
    .ndo_siocdevprivate = xj3_dev_siocdevprivate,
    .ndo_tsn_capable = xj3_tsn_capable,
    .ndo_tsn_link_configure = xj3_tsn_link_configure,
    .ndo_select_queue = xj3_tsn_select_queue,
};

static int xj3_get_rx_status(void *data, struct xj3_extra_stats *x,
                             struct dma_desc *p) {
    struct net_device_stats *stats = (struct net_device_stats *)data;
    struct xj3_priv *priv = container_of(x, struct xj3_priv, xstats);
    unsigned int rdes0 = le32_to_cpu(p->des0);
    unsigned int rdes1 = le32_to_cpu(p->des1);
    unsigned int rdes2 = le32_to_cpu(p->des2);
    unsigned int rdes3 = le32_to_cpu(p->des3);

    int message_type;
    int ret = good_frame;

    dev_dbg(priv->device,
            "%s,and rdes0:0x%x, rdes1:0x%x, rdes2:0x%x, rdes3:0x%x\n", __func__,
            rdes0, rdes1, rdes2, rdes3);

    if (rdes3 & RDES3_OWN) return dma_own;

    if (!(rdes3 & RDES3_LAST_DESCRIPTOR)) {
        return discard_frame;
    }

    if (rdes3 & RDES3_ERROR_SUMMARY) {
        if (rdes3 & RDES3_GIANT_PACKET) {
            stats->rx_length_errors++;
        }
        if (rdes3 & RDES3_OVERFLOW_ERROR) {
            x->rx_gmac_overflow++;
        }

        if (rdes3 & RDES3_RECEIVE_WATCHDOG) {
            x->rx_watchdog++;
        }
        if (rdes3 & RDES3_RECEIVE_ERROR) {
            x->rx_mii++;
        }

        if (rdes3 & RDES3_CRC_ERROR) {
            x->rx_crc_errors++;
            stats->rx_crc_errors++;
        }

        if (rdes3 & RDES3_DRIBBLE_ERROR) {
            x->dribbling_bit++;
        }
        stats->rx_errors++;
        ret = discard_frame;
    }

    message_type = (rdes1 & ERDES4_MSG_TYPE_MASK) >> 8;

    dev_dbg(priv->device, "%s, and message_type:0x%x\n", __func__,
            message_type);
    dev_dbg(priv->device, "%s,payloadtype:0x%x\n", __func__, rdes1 & 0x7);

    if (rdes1 & RDES1_IP_HDR_ERROR) {
        x->ip_hdr_err++;
    }

    if (rdes1 & RDES1_IP_CSUM_BYPASSED) {
        x->ip_csum_bypassed++;
    }

    if (rdes1 & RDES1_IPV4_HEADER) {
        x->ipv4_pkt_rcvd++;
    }

    if (rdes1 & RDES1_IPV6_HEADER) {
        x->ipv6_pkt_rcvd++;
    }

    if (message_type == RDES_EXT_NO_PTP) {
        x->no_ptp_rx_msg_type_ext++;
    } else if (message_type == RDES_EXT_SYNC) {
        x->ptp_rx_msg_type_sync++;
    } else if (message_type == RDES_EXT_DELAY_REQ) {
        x->ptp_rx_msg_type_delay_req++;
    } else if (message_type == RDES_EXT_FOLLOW_UP) {
        x->ptp_rx_msg_type_follow_up++;
    } else if (message_type == RDES_EXT_DELAY_RESP) {
        x->ptp_rx_msg_type_delay_resp++;
    } else if (message_type == RDES_EXT_PDELAY_REQ) {
        x->ptp_rx_msg_type_pdelay_req++;
    } else if (message_type == RDES_EXT_PDELAY_RESP) {
        x->ptp_rx_msg_type_pdelay_resp++;
    } else if (message_type == RDES_EXT_PDELAY_FOLLOW_UP) {
        x->ptp_rx_msg_type_pdelay_follow_up++;
    } else if (message_type == RDES_PTP_ANNOUNCE) {
        x->ptp_rx_msg_type_announce++;
    } else if (message_type == RDES_PTP_MANAGEMENT) {
        x->ptp_rx_msg_type_management++;
    } else if (message_type == RDES_PTP_PKT_RESERVED_TYPE) {
        x->ptp_rx_msg_pkt_reserved_type++;
    }

    if (rdes1 & RDES1_PTP_PACKET_TYPE) {
        x->ptp_frame_type++;
    }

    if (rdes1 & RDES1_PTP_VER) {
        x->ptp_ver++;
    }
    if (rdes1 & RDES1_TIMESTAMP_DROPPED) x->timestamp_dropped++;

    if (rdes2 & RDES2_SA_FILTER_FAIL) {
        x->sa_rx_filter_fail++;
        ret = discard_frame;
    }

    if (rdes2 & RDES2_L3_FILTER_MATCH) x->l3_filter_match++;

    if (rdes2 & RDES2_L4_FILTER_MATCH) x->l4_filter_match++;

    if ((rdes2 & RDES2_L3_L4_FILT_NB_MATCH_MASK) >>
        RDES2_L3_L4_FILT_NB_MATCH_SHIFT)
        x->l3_l4_filter_no_match++;

    dev_dbg(priv->device, "%s,and return ret:%d\n", __func__, ret);
    return ret;
}

static int xj3_rx_check_timestamp(void *desc) {
    struct dma_desc *p = (struct dma_desc *)desc;
    unsigned int rdes0 = le32_to_cpu(p->des0);
    unsigned int rdes1 = le32_to_cpu(p->des1);
    unsigned int rdes3 = le32_to_cpu(p->des3);

    u32 own, ctxt;
    int ret = 1;

    own = rdes3 & RDES3_OWN;
    ctxt =
        ((rdes3 & RDES3_CONTEXT_DESCRIPTOR) >> RDES3_CONTEXT_DESCRIPTOR_SHIFT);

    if (likely(!own && ctxt)) {
        if ((rdes0 == 0xffffffff) && (rdes1 == 0xffffffff))
            ret = -EINVAL;
        else
            ret = 0;
    }

    return ret;
}

static int xj3_get_rx_timestamp_status(void *desc, void *next_desc, u32 ats) {
    struct dma_desc *p = (struct dma_desc *)desc;
    int ret = -EINVAL;

    if (likely(le32_to_cpu(p->des3) & RDES3_RDES1_VALID)) {
        if (likely(le32_to_cpu(p->des1) & RDES1_TIMESTAMP_AVAILABLE)) {
            int i = 0;

            do {
                ret = xj3_rx_check_timestamp(next_desc);
                if (ret < 0) goto exit;

                i++;
            } while ((ret == 1) && (i < 10));

            if (i == 10) ret = -EBUSY;
        }
    }

exit:
    if (likely(ret == 0)) return 1;
    return 0;
}

static void xj3_get_rx_hwtstamp(struct xj3_priv *priv, struct dma_desc *p,
                                struct dma_desc *np, struct sk_buff *skb) {
    struct skb_shared_hwtstamps *shhwtstamp = NULL;
    struct dma_desc *desc = p;
    u64 ns;

    if (!priv->hwts_rx_en) return;

    desc = np;

    if (xj3_get_rx_timestamp_status(p, np, priv->adv_ts)) {
        dev_dbg(priv->device, "get rx hwstamp: des0:0x%x, des1:0x%x\n",
                desc->des0, desc->des1);
        ns = le32_to_cpu(desc->des0);
        ns += le32_to_cpu(desc->des1) * 1000000000ULL;
        shhwtstamp = skb_hwtstamps(skb);
        memset(shhwtstamp, 0, sizeof(struct skb_shared_hwtstamps));
        shhwtstamp->hwtstamp = ns_to_ktime(ns);
    } else {
        dev_dbg(priv->device, "cannot get RX hw timestamp\n");
    }
}

static void xj3_rx_vlan(struct net_device *ndev, struct sk_buff *skb) {
    struct ethhdr *ehdr;
    u16 vlanid;

    if ((ndev->features & NETIF_F_HW_VLAN_CTAG_RX) == NETIF_F_HW_VLAN_CTAG_RX &&
        !__vlan_get_tag(skb, &vlanid)) {
        ehdr = (struct ethhdr *)skb->data;
        memmove(skb->data + VLAN_HLEN, ehdr, ETH_ALEN * 2);
        skb_pull(skb, VLAN_HLEN);
        dev_dbg(&ndev->dev, "%s, vlanid:0x%x\n", __func__, vlanid);
        __vlan_hwaccel_put_tag(skb, htons(ETH_P_8021Q), vlanid);
    }
}

static inline u32 xj3_rx_dirty(struct xj3_priv *priv, u32 queue) {
    struct xj3_rx_queue *rx_q = &priv->rx_queue[queue];
    u32 dirty;

    if (rx_q->dirty_rx <= rx_q->cur_rx)
        dirty = rx_q->cur_rx - rx_q->dirty_rx;
    else
        dirty = DMA_RX_SIZE - rx_q->dirty_rx + rx_q->cur_rx;

    return dirty;
}

static bool xj3_try_fill_recv(struct xj3_priv *priv, u32 queue)
{
	struct xj3_rx_queue *rx_q = &priv->rx_queue[queue];
	u32 i, refill_rx = rx_q->refill_rx;
	int ret;

	for (i = refill_rx; i < DMA_RX_SIZE; i++) {
		struct dma_desc *p;

		if (priv->extend_desc)
			p = &((rx_q->dma_erx + i)->basic);
		else
			p = rx_q->dma_rx + i;

		if (rx_q->rx_skbuff[i]) {
			rx_q->refill_rx = 0;
			return true;
		}

		ret = xj3_init_rx_buffers(priv, p, i, queue);
		if (ret) {
			rx_q->refill_rx = i;
			return false;
		}
	}
	rx_q->refill_rx = 0;
	xj3_clear_rx_descriptors(priv, queue);

	return true;
}

static void xj3_refill_work(struct work_struct *work) {
	struct delayed_work *delayed_work = container_of(work,
			struct delayed_work, work);
	struct xj3_rx_queue *rx_q = container_of(delayed_work, struct xj3_rx_queue,
			work);
	u32 queue = rx_q->queue_index;
	struct xj3_priv *priv = container_of(rx_q, struct xj3_priv,
			rx_queue[queue]);
	bool still_empty = true;

	dev_info(priv->device,
			"The ring buffer has been emptied, Now go to reallocate it.\n");

	// start try to fill
	still_empty = !xj3_try_fill_recv(priv, queue);
	if (still_empty) {
		// HZ/2: reference resources virtio-net
		schedule_delayed_work(&rx_q->work, HZ/2);
		return;
	}

	rx_q->reschedule_fill_flag = 0;

	// write rx tail addr
	xj3_set_rx_tail_ptr(priv->ioaddr, rx_q->rx_tail_addr, queue);

	dev_info(priv->device, "refill finish!\n");
}

static inline void xj3_rx_refill(struct xj3_priv *priv, u32 queue) {
    struct xj3_rx_queue *rx_q = &priv->rx_queue[queue];
    int dirty = xj3_rx_dirty(priv, queue);
    unsigned int entry = rx_q->dirty_rx;

    int bfsize = priv->dma_buf_sz;

    while (dirty-- > 0) {
        struct dma_desc *p;
        bool use_rx_wd;

        if (priv->extend_desc)
            p = (struct dma_desc *)(rx_q->dma_erx + entry);
        else
            p = rx_q->dma_rx + entry;

        if (!rx_q->rx_skbuff[entry]) {
            struct sk_buff *skb;
            skb = netdev_alloc_skb_ip_align(priv->dev, bfsize);
            if (!skb) {
                rx_q->rx_zeroc_thresh = STMMAC_RX_THRESH;
                if (net_ratelimit()) {
                    dev_info(priv->device, "%s: fail to alloc skb entry%d\n",
                             __func__, entry);
                }

                break;
            }

            rx_q->rx_skbuff[entry] = skb;
            rx_q->rx_skbuff_dma[entry] = dma_map_single(
                priv->device, skb->data, bfsize, DMA_FROM_DEVICE);
            if (dma_mapping_error(priv->device, rx_q->rx_skbuff_dma[entry])) {
                dev_info(priv->device, "%s: Rx DMA map failed\n", __func__);
                dev_kfree_skb(skb);
                break;
            }

            p->des0 = cpu_to_le32(rx_q->rx_skbuff_dma[entry]);
            p->des1 = 0;

            if (rx_q->rx_zeroc_thresh > 0) rx_q->rx_zeroc_thresh--;

            dev_dbg(priv->device, "%s: refill entry: #%d\n", __func__, entry);
        }

        rx_q->rx_count_frames++;
        rx_q->rx_count_frames %= priv->rx_coal_frames;
        use_rx_wd = rx_q->rx_count_frames > 0;
        if (!priv->use_riwt) use_rx_wd = false;

        dev_dbg(priv->device, "%s, use_rx_wd:%d, rx_count:%d, rx_coal:%d\n",
                __func__, use_rx_wd, rx_q->rx_count_frames,
                priv->rx_coal_frames);

        dma_wmb();
        if (!use_rx_wd) p->des3 = cpu_to_le32(RDES3_INT_ON_COMPLETION_EN);
        p->des3 |= cpu_to_le32(RDES3_OWN | RDES3_BUFFER1_VALID_ADDR);

        entry = XJ3_GET_ENTRY(entry, DMA_RX_SIZE);
    }

    rx_q->dirty_rx = entry;

    rx_q->rx_tail_addr =
        rx_q->dma_rx_phy + (rx_q->dirty_rx * sizeof(struct dma_desc));
    xj3_set_rx_tail_ptr(priv->ioaddr, rx_q->rx_tail_addr, queue);
}

static int xj3_rx_packet(struct xj3_priv *priv, int limit, u32 queue) {
    struct xj3_rx_queue *rx_q = &priv->rx_queue[queue];
    unsigned int entry = rx_q->cur_rx;
    int coe = priv->rx_csum;
    unsigned int next_entry;
    unsigned int count = 0;
    struct net_device *ndev = priv->dev;
	struct sk_buff *skb;

	/* Exit if refill work is executing */
	if (unlikely(rx_q->reschedule_fill_flag))
		return count;

    while (count < limit) {
        int status;
        struct dma_desc *p, *np;

        if (priv->extend_desc)
            p = (struct dma_desc *)(rx_q->dma_erx + entry);
        else
            p = rx_q->dma_rx + entry;

        status = xj3_get_rx_status(&priv->dev->stats, &priv->xstats, p);

        if (status & dma_own) break;

        skb = rx_q->rx_skbuff[entry];
        if (!skb) {
            dev_err(priv->device, "%s: inconsistent Rx chain\n",
					priv->dev->name);
			rx_q->reschedule_fill_flag = 1;
			schedule_delayed_work(&rx_q->work, 0);
			priv->xstats.rx_pkt_n += count;
			return count;
        }
        count++;
        rx_q->cur_rx = XJ3_GET_ENTRY(rx_q->cur_rx, DMA_RX_SIZE);
        next_entry = rx_q->cur_rx;

        dev_dbg(priv->device, "%s, cur_rx:%d\n", __func__, rx_q->cur_rx);
        if (priv->extend_desc)
            np = (struct dma_desc *)(rx_q->dma_erx + next_entry);
        else
            np = rx_q->dma_rx + next_entry;

        prefetch(np);

        if (priv->extend_desc) {
            // so we need add code here
        }

        if (status == discard_frame) {
            if (priv->hwts_rx_en && !priv->extend_desc) {
                dma_unmap_single(priv->device, rx_q->rx_skbuff_dma[entry],
                                 priv->dma_buf_sz, DMA_FROM_DEVICE);
                dev_kfree_skb_any(rx_q->rx_skbuff[entry]);
                rx_q->rx_skbuff[entry] = NULL;
            }
        } else {
            int frame_len;
            unsigned int des;

            des = le32_to_cpu(p->des0);
            frame_len = (le32_to_cpu(p->des3) & RDES3_PACKET_SIZE_MASK);
            if (frame_len > priv->dma_buf_sz) {
                dev_info(priv->device, "len %d larger than size (%d)\n",
                         frame_len, priv->dma_buf_sz);
                priv->dev->stats.rx_length_errors++;
                break;
            }

            if (!(ndev->features & NETIF_F_RXFCS)) {
                frame_len -= 4;
            }
            //			if (status != llc_snap)
            //				frame_len -= ETH_FCS_LEN;
            prefetch(skb->data - NET_IP_ALIGN);
            rx_q->rx_skbuff[entry] = NULL;
            rx_q->rx_zeroc_thresh++;
            skb_put(skb, frame_len);
            dma_unmap_single(priv->device, rx_q->rx_skbuff_dma[entry],
                             priv->dma_buf_sz, DMA_FROM_DEVICE);

            xj3_get_rx_hwtstamp(priv, p, np, skb);
            xj3_rx_vlan(priv->dev, skb);

            skb->protocol = eth_type_trans(skb, priv->dev);

            if (!coe)
                skb_checksum_none_assert(skb);
            else
                skb->ip_summed = CHECKSUM_UNNECESSARY;
            napi_gro_receive(&rx_q->napi, skb);
            priv->dev->stats.rx_packets++;
            priv->dev->stats.rx_bytes += frame_len;
        }
        entry = next_entry;
    }

    dev_dbg(priv->device, "%s, count:%d, cur_rx:%d\n", __func__, count,
            rx_q->cur_rx);
    xj3_rx_refill(priv, queue);
    priv->xstats.rx_pkt_n += count;
    return count;
}

static inline void xj3_enable_dma_irq(struct xj3_priv *priv, u32 chan) {
    writel(DMA_CHAN_INTR_DEFAULT_MASK, priv->ioaddr + DMA_CHAN_INTR_ENA(chan));
}

static int xj3_poll(struct napi_struct *napi, int budget) {
    struct xj3_rx_queue *rx_q = container_of(napi, struct xj3_rx_queue, napi);
    struct xj3_priv *priv = rx_q->priv_data;

    u32 tx_count = priv->plat->tx_queues_to_use;
    u32 chan = rx_q->queue_index;
    int work_done = 0;
    u32 queue;

    priv->xstats.napi_poll++;

    for (queue = 0; queue < tx_count; queue++) {
        xj3_tx_clean(priv, queue);
    }

    work_done = xj3_rx_packet(priv, budget, rx_q->queue_index);
    /*
     * we can enable dma irq only if return value of napi_complete_done is true.
     * if the return value of napi_complete_done is false,we do nothing here and
     * the poll will be called later or schduled again.
     */
    if ((work_done < budget) && napi_complete_done(napi, work_done))
        xj3_enable_dma_irq(priv, chan);

    return work_done;
}

static ssize_t phy_addr_store(struct device *dev, struct device_attribute *attr,
                              const char *buf, size_t len) {
    struct net_device *ndev = to_net_dev(dev);
    struct xj3_priv *priv = netdev_priv(ndev);

    if (!strncmp(buf, "0", len - 1)) {
        priv->sysfs_phy_addr = 0;
    } else if (!strncmp(buf, "e", len - 1)) {
        priv->sysfs_phy_addr = 0xe;
    } else {
        priv->sysfs_phy_addr = 0;
    }
    dev_info(priv->device, "%s, and sysfs_phy_addr:0x%x\n", __func__,
             priv->sysfs_phy_addr);
    return len;
}

static ssize_t phy_addr_show(struct device *dev, struct device_attribute *attr,
                             char *buf) {
    struct net_device *ndev = to_net_dev(dev);
    struct xj3_priv *priv = netdev_priv(ndev);

    size_t ret = 0;

    ret = snprintf(buf, sizeof(priv->sysfs_phy_addr) * 2 + 2, "%x\n",
                   priv->sysfs_phy_addr);
    return ret;
}

static DEVICE_ATTR_RW(phy_addr);

static ssize_t phy_reg_show(struct device *dev, struct device_attribute *attr,
                            char *buf) {
    struct net_device *ndev = to_net_dev(dev);
    struct xj3_priv *priv = netdev_priv(ndev);

    size_t ret = 0;

    dev_info(priv->device, "%s, reg0: 0x%x\n", __func__,
             xj3_mdio_read(priv->mii, priv->sysfs_phy_addr, 0));
    dev_info(priv->device, "%s, reg1: 0x%x\n", __func__,
             xj3_mdio_read(priv->mii, priv->sysfs_phy_addr, 1));
    dev_info(priv->device, "%s, reg2: 0x%x\n", __func__,
             xj3_mdio_read(priv->mii, priv->sysfs_phy_addr, 2));
    dev_info(priv->device, "%s, reg9: 0x%x\n", __func__,
             xj3_mdio_read(priv->mii, priv->sysfs_phy_addr, 9));
    dev_info(priv->device, "%s, reg10: 0x%x\n", __func__,
             xj3_mdio_read(priv->mii, priv->sysfs_phy_addr, 10));

    return ret;
}

static DEVICE_ATTR_RO(phy_reg);

static ssize_t dump_rx_desc_show(struct device *dev,
                                 struct device_attribute *attr, char *buf) {
    struct net_device *ndev = to_net_dev(dev);
    struct xj3_priv *priv = netdev_priv(ndev);
    u32 queue;
    int i;

    for (queue = 0; queue < 1; queue++) {
        struct xj3_rx_queue *rx_q = &priv->rx_queue[queue];

        dev_info(priv->device, "%s, cur_rx:%d\n", __func__, rx_q->cur_rx);

        for (i = 0; i < DMA_RX_SIZE; i++) {
            struct dma_desc *p;

            p = rx_q->dma_rx + i;
            dev_info(priv->device, "%d, des3:0x%x\n", i, p->des3);
        }
    }

    return 0;
}
static DEVICE_ATTR_RO(dump_rx_desc);

static ssize_t tx_desc_show(struct device *dev, struct device_attribute *attr,
                            char *buf) {
    struct net_device *ndev = to_net_dev(dev);
    struct xj3_priv *priv = netdev_priv(ndev);
    u32 queue;
    int i;

    for (queue = 0; queue < 1; queue++) {
        struct xj3_tx_queue *tx_q = &priv->tx_queue[queue];

        dev_info(priv->device, "%s, cur_rx:%d\n", __func__, tx_q->cur_tx);

        for (i = 0; i < DMA_RX_SIZE; i++) {
            struct dma_desc *p;

            p = tx_q->dma_tx + i;
            if (p)
                dev_info(priv->device,
                         "%d, des0:0x%x, des1: 0x%x, des2:0x%x, des3:0x%x\n", i,
                         p->des0, p->des1, p->des2, p->des3);
        }
    }

    return 0;
}
static DEVICE_ATTR_RO(tx_desc);

static struct attribute *phy_reg_attrs[] = {

    &dev_attr_phy_reg.attr,
    &dev_attr_phy_addr.attr,
    &dev_attr_dump_rx_desc.attr,
    &dev_attr_tx_desc.attr,
    NULL,
};

static struct attribute_group phy_reg_group = {
    .name = "phy_reg",
    .attrs = phy_reg_attrs,

};

static void hobot_reset_queues_param(struct xj3_priv *priv) {
    u32 rx_cnt = priv->plat->rx_queues_to_use;
    u32 tx_cnt = priv->plat->tx_queues_to_use;
    u32 queue;

    for (queue = 0; queue < rx_cnt; queue++) {
        struct xj3_rx_queue *rx_q = &priv->rx_queue[queue];

        rx_q->cur_rx = 0;
        rx_q->dirty_rx = 0;
    }

    for (queue = 0; queue < tx_cnt; queue++) {
        struct xj3_tx_queue *tx_q = &priv->tx_queue[queue];

        tx_q->cur_tx = 0;
        tx_q->dirty_tx = 0;
    }
}

#ifdef CONFIG_HOBOT_DMC_CLK

static int xj3_dfs_dma_reset(void __iomem *ioaddr) {
    u32 value = readl(ioaddr + DMA_BUS_MODE);
    int limit;

    value |= DMA_BUS_MODE_SFT_RESET;
    writel(value, ioaddr + DMA_BUS_MODE);
    limit = 10;
    while (limit--) {
    /*The DMA_BUS_MODE_SFT_RESET must be read at least 4 CSR clock cycles after it is written to 1.*/
        udelay(2);
        if (!(readl(ioaddr + DMA_BUS_MODE) & DMA_BUS_MODE_SFT_RESET)) break;
	}
    if (limit < 0) return -EBUSY;

    return 0;
}

static int xj3_dfs_init_dma_engine(struct xj3_priv *priv) {
    int ret;
    u32 rx_channel_count = priv->plat->rx_queues_to_use;
    u32 tx_channel_count = priv->plat->tx_queues_to_use;
    struct xj3_rx_queue *rx_q;
    struct xj3_tx_queue *tx_q;
    u32 chan = 0;

    ret = xj3_dfs_dma_reset(priv->ioaddr);
    if (ret) {
        dev_info(priv->device, "%s: Failed to reset dma\n", __func__);
        return ret;
    }

    xj3_dma_init(priv->ioaddr, priv->plat->dma_cfg, 0, 0, 0);

    for (chan = 0; chan < rx_channel_count; chan++) {
        rx_q = &priv->rx_queue[chan];
        xj3_init_rx_chan(priv->ioaddr, priv->plat->dma_cfg, rx_q->dma_rx_phy,
                         chan);

        rx_q->rx_tail_addr =
            rx_q->dma_rx_phy + (DMA_RX_SIZE * sizeof(struct dma_desc));
        xj3_set_rx_tail_ptr(priv->ioaddr, rx_q->rx_tail_addr, chan);
    }

    for (chan = 0; chan < tx_channel_count; chan++) {
        tx_q = &priv->tx_queue[chan];

        xj3_init_chan(priv->ioaddr, priv->plat->dma_cfg, chan);
        xj3_init_tx_chan(priv->ioaddr, priv->plat->dma_cfg, tx_q->dma_tx_phy,
                         chan);
        tx_q->tx_tail_addr =
            tx_q->dma_tx_phy;  // + (DMA_TX_SIZE *sizeof(struct dma_desc));
        xj3_set_tx_tail_ptr(priv->ioaddr, tx_q->tx_tail_addr, chan);
    }

    xj3_set_dma_axi(priv->ioaddr, priv->plat->axi);
    return 0;
}

static inline bool is_hobot_eth_dma_stop(struct xj3_priv *priv) {
    u32 value;
    value = readl((void __iomem *)priv->dev->base_addr + DMA_DEBUG_STATUS);
    value = (value >> 12) & 0xf;
    return !!(value == DMA_DEBUG_STATUS_STOP);
}

static int dfs_eth_suspend(struct device *dev);
static int dfs_eth_resume(struct device *dev);

static void xj3_dfs_clear_rx_descriptors(struct xj3_priv *priv, u32 queue) {
    struct xj3_rx_queue *rx_q = &priv->rx_queue[queue];
    struct dma_desc *p;
    int dirty = xj3_rx_dirty(priv, queue);
    unsigned int entry = rx_q->dirty_rx;
    while (dirty-- > 0) {
        p = rx_q->dma_rx + entry;
        p->des3 = cpu_to_le32(RDES3_OWN | RDES3_BUFFER1_VALID_ADDR);
        p->des3 |= cpu_to_le32(RDES3_INT_ON_COMPLETION_EN);
    }
}

static int xj3_dfs_hw_setup(struct net_device *ndev) {
    struct xj3_priv *priv = netdev_priv(ndev);
    int ret;
    ret = xj3_dfs_init_dma_engine(priv);
    if (ret < 0) {
        dev_info(priv->device, "%s, DMA engine initilization failed\n",
                 __func__);
        return ret;
    }
    xj3_set_umac_addr(priv->ioaddr, ndev->dev_addr, 0);
    xj3_core_init(priv, ndev->mtu);
    xj3_mtl_config(priv);
    ret = xj3_rx_ipc_enable(priv);
    if (!ret) {
        dev_dbg(priv->device, "RX IPC checksum offload disabled\n");
        priv->plat->rx_coe = STMMAC_RX_COE_NONE;
        priv->rx_csum = 0;
    }
    xj3_set_mac(priv->ioaddr, true);
    xj3_dma_operation_mode(priv);
    xj3_mmc_setup(priv);
    if (priv->use_riwt) {
        priv->rx_riwt = MAX_DMA_RIWT;
        hobot_dma_rx_watchdog(priv, MAX_DMA_RIWT, 1);
    }
    xj3_set_rings_length(priv);
    xj3_start_all_dma(priv);
    if (priv->tso) {
        u32 value;
        value = readl(priv->ioaddr + DMA_CHAN_TX_CONTROL(0));
        writel(value | DMA_CONTROL_TSE,
                priv->ioaddr + DMA_CHAN_TX_CONTROL(0));
    }
    return 0;
}


static int dfs_eth_suspend(struct device *dev) {
    struct net_device *ndev = dev_get_drvdata(dev);
    struct xj3_priv *priv = netdev_priv(ndev);
    struct xj3_rx_queue *rx_q = &priv->rx_queue[0];
    /*DFS_READ_STATE_TIMES : 读取dma stop位的次数 共20us */
#define DFS_READ_STATE_TIMES 20
    u32 times = DFS_READ_STATE_TIMES;
    int ret;
    if (!ndev) {
        dev_info(priv->device, "%s, and ndev is NULL\n", __func__);
        return 0;
    }
    if (!netif_running(ndev)) {
        return 0;
    }
    /*when in xmit return -EBUSY*/
    if (test_bit(HOBOT_IN_XMIT, &priv->state)) {
        return -EBUSY;
    }
    ret = test_bit(NAPI_STATE_SCHED, &rx_q->napi.state);
    if (ret) {
        return -EBUSY;
    }
    napi_synchronize(&rx_q->napi);
    if (try_to_del_timer_sync(&priv->txtimer) < 0) {
        return -EBUSY;
    }
    xj3_stop_all_dma(priv);
    priv->link = 0;
    priv->speed = 0;
    priv->duplex = DUPLEX_UNKNOWN;
    /*
     *在xj3_stop_all_dma后补充一次dma状态的判断，因为根据databook，
     *关闭dma rx 和tx后，其并不会立刻停止传输，而是等待当前已经开启的传输完成才停止。
     *此处可循环查询dma状态是否处于stop状态，如果没有，则等待一会继续查询，建议使用
     *us为单位进行查询，因为最大包不超过2k Bytes，fifo与ddr之间的传输最大不超过20us。
     *如果仍然因为未知原因失败，则建议调用dfs恢复接口，并返回-EBASY。
     */
    while (times > 0) {
        udelay(1);
        if (is_hobot_eth_dma_stop(priv)) {
            break;
        }
        times--;
    }
    if (times == 0) {
        dfs_eth_resume(dev);
        return -EBUSY;
    }
    dev_dbg(priv->device, "%s, successed\n", __func__);
    return 0;
}

static int dfs_eth_resume(struct device *dev) {
    struct net_device *ndev = dev_get_drvdata(dev);
    struct xj3_priv *priv = netdev_priv(ndev);
    if (!netif_running(ndev)) {
        return 0;
    }
    hobot_reset_queues_param(priv);
    priv->mss = 0;
    xj3_dfs_clear_rx_descriptors(priv, 0);
    xj3_dfs_hw_setup(ndev);
    hobot_init_intr_coalesce(priv);
    xj3_set_rx_mode(ndev);
    netdev_tx_reset_queue(netdev_get_tx_queue(priv->dev, 0));
    xj3_set_speed(priv);
    dev_dbg(priv->device, "%s, successed\n", __func__);
    return 0;
}

int eth_dfs_call(struct hobot_dpm *dpm, unsigned long val, int state) {
    struct net_device *ndev = dev_get_drvdata(dpm->dev);
    struct xj3_priv *priv = netdev_priv(ndev);
    struct xj3_tx_queue *tx_q = &priv->tx_queue[0];
    int res = 0;
    u32 value, dma_dbg_sta_tx, dma_dbg_sta_rx;
    value = readl((void __iomem *)priv->dev->base_addr + DMA_DEBUG_STATUS);
    dma_dbg_sta_rx = (value >> 8) & 0xf;
    dma_dbg_sta_tx = (value >> 12) & 0xf;
    if (val == HB_BUS_SIGNAL_START) {
        if ((dma_dbg_sta_rx == DMA_DEBUG_STATUS_RUN_WRP
            || dma_dbg_sta_rx == DMA_DEBUG_STATUS_STOP)
            && (dma_dbg_sta_tx == DMA_DEBUG_STATUS_SUSPEND
            || dma_dbg_sta_tx == DMA_DEBUG_STATUS_STOP)
            && (tx_q->dirty_tx == tx_q->cur_tx)) {
            res = dfs_eth_suspend(dpm->dev);
        } else {
	        res = -EBUSY;
	    }
    } else if (val == HB_BUS_SIGNAL_END) {
        res = dfs_eth_resume(dpm->dev);
    }
    return res;
}
#endif

static int xj3_dvr_probe(struct device *device,
                         struct plat_config_data *plat_dat,
                         struct xj3_resource *xj3_res) {
    struct net_device *ndev = NULL;
    struct xj3_priv *priv;
    int ret = 0;
    int timeout = 4;
    u32 queue;
    u8 mac_addr[ETH_ALEN];

    writel(DMA_BUS_MODE_SFT_RESET, xj3_res->addr + DMA_BUS_MODE);
    while (--timeout) {
        udelay(1);
        if (readl(xj3_res->addr + DMA_BUS_MODE) & DMA_BUS_MODE_SFT_RESET)
            continue;
        break;
    }

    ndev = alloc_etherdev_mqs(sizeof(struct xj3_priv), MTL_MAX_TX_QUEUES,
                              MTL_MAX_RX_QUEUES);
    if (!ndev) return -ENOMEM;

    SET_NETDEV_DEV(ndev, device);
    priv = netdev_priv(ndev);
    priv->device = device;
    priv->dev = ndev;

    hobot_set_ethtool_ops(priv);
    priv->plat = plat_dat;
    priv->ioaddr = xj3_res->addr;
    priv->dev->base_addr = (unsigned long)xj3_res->addr;

    priv->dev->irq = xj3_res->irq;

#ifdef HOBOT_USE_IRQ_SPLIT
    priv->tx_irq = xj3_res->tx_irq;
    priv->rx_irq = xj3_res->rx_irq;
    strcpy(priv->txirq_name, "eth-tx");
    strcpy(priv->rxirq_name, "eth-rx");
#endif
    if (xj3_res->mac) {
        // ether_addr_copy(ndev->dev_addr, xj3_res->mac);
        eth_hw_addr_set(ndev,xj3_res->mac);
        printk("set mac address %02x:%02x:%02x:%02x:%02x:%02x:",
               ndev->dev_addr[0], ndev->dev_addr[1], ndev->dev_addr[2],
               ndev->dev_addr[3], ndev->dev_addr[4], ndev->dev_addr[5]);
        printk("(using dtb adress)\n");
    } else {
        get_random_bytes(mac_addr, ETH_ALEN);
        mac_addr[0] = 0x00;
        mac_addr[1] = 0x11;
        mac_addr[2] = 0x22;
        eth_hw_addr_set(ndev, mac_addr);

        printk("set mac address %02x:%02x:%02x:%02x:%02x:%02x:",
               ndev->dev_addr[0], ndev->dev_addr[1], ndev->dev_addr[2],
               ndev->dev_addr[3], ndev->dev_addr[4], ndev->dev_addr[5]);
        printk("(using random mac adress)\n");
    }

    xj3_set_umac_addr(priv->ioaddr, ndev->dev_addr, 0);
    dev_set_drvdata(device, priv->dev);
    ret = xj3_hw_init(priv);
    if (ret) goto err_hw_init;

    if (priv->dma_cap.tsn && priv->plat->tx_queues_to_use > 3) {
        xj3_configure_tsn(priv);
    }

    netif_set_real_num_rx_queues(ndev, priv->plat->rx_queues_to_use);
    netif_set_real_num_tx_queues(ndev, priv->plat->tx_queues_to_use);

    ndev->netdev_ops = &xj3_netdev_ops;

    ndev->hw_features = NETIF_F_SG;

#if 1
    if (priv->plat->tso_en) {
        ndev->hw_features |= NETIF_F_TSO | NETIF_F_TSO6;
        priv->tso = true;
        dev_info(priv->device, "TSO feature enabled\n");
    }
    if (priv->dma_cap.tx_coe)
        ndev->hw_features |= NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM;

    if (priv->dma_cap.rx_coe) ndev->hw_features |= NETIF_F_RXCSUM;
#endif
    ndev->hw_features |= NETIF_F_RXFCS;

    ndev->features = ndev->hw_features | NETIF_F_HIGHDMA;

    ndev->watchdog_timeo = DWCEQOS_TX_TIMEOUT * HZ;

#ifdef XJ3_VLAN_TAG_USED
    ndev->features |= NETIF_F_HW_VLAN_CTAG_RX;
#endif

    xj3_mdio_set_csr(priv);

    ndev->min_mtu = ETH_ZLEN - ETH_HLEN;
    ndev->max_mtu = JUMBO_LEN;

    if ((priv->plat->maxmtu < ndev->max_mtu) &&
        (priv->plat->maxmtu >= ndev->min_mtu))
        ndev->max_mtu = priv->plat->maxmtu;
    else if (priv->plat->maxmtu < ndev->min_mtu)
        dev_info(priv->device,
                 "waring: maxmtu having invalid value by Network\n");

    priv->use_riwt = 1;
    dev_info(priv->device, "Enable Rx Mitigation via HW Watchdog Timer\n");

    for (queue = 0; queue < priv->plat->rx_queues_to_use; queue++) {
        struct xj3_rx_queue *rx_q = &priv->rx_queue[queue];
		INIT_DELAYED_WORK(&rx_q->work, xj3_refill_work);

        netif_napi_add_weight(ndev, &rx_q->napi, xj3_poll,
                       (12 * priv->plat->rx_queues_to_use));
    }

    spin_lock_init(&priv->lock);
    spin_lock_init(&priv->state_lock);

    ret = xj3_mdio_register(priv);
    if (ret < 0) {
        dev_info(priv->device, "MDIO bus error register\n");
        goto err_mdio_reg;
    }

    ret = devm_request_irq(priv->device, ndev->irq, &xj3_interrupt, 0,
                           ndev->name, ndev);
    if (ret < 0) {
        dev_info(priv->device, "%s: error allocating the IRQ\n", __func__);
        goto irq_error;
    }
    disable_irq(ndev->irq);

#ifdef HOBOT_USE_IRQ_SPLIT
    ret = devm_request_irq(priv->device, priv->tx_irq, &xj3_interrupt, 0,
                           priv->txirq_name, ndev);
    if (ret < 0) {
        dev_info(priv->device, "%s: error allocating the IRQ(%s):%d\n",
                 __func__, priv->txirq_name, priv->tx_irq);
        goto irq_error;
    }
    disable_irq(priv->tx_irq);
    ret = devm_request_irq(priv->device, priv->rx_irq, &xj3_interrupt, 0,
                           priv->rxirq_name, ndev);
    if (ret < 0) {
        dev_info(priv->device, "%s: error allocating the IRQ(%s):%d\n",
                 __func__, priv->rxirq_name, priv->rx_irq);
        goto irq_error;
    }
    disable_irq(priv->rx_irq);
#endif

    ndev->sysfs_groups[0] = &phy_reg_group;

    ret = register_netdev(ndev);
    if (ret) {
        dev_info(priv->device, "error register network device\n");
        goto err_netdev_reg;
    }
#ifdef CONFIG_HOBOT_DMC_CLK
    priv->eth_dfs.dpm_call = eth_dfs_call;
    priv->eth_dfs.priority = DPM_PRI_LEVEL2;
    hobot_dfs_register(&priv->eth_dfs, priv->device);
#endif
    return 0;

irq_error:
err_netdev_reg:
    mdiobus_unregister(priv->mii);
    priv->mii->priv = NULL;
    mdiobus_free(priv->mii);
    priv->mii = NULL;
err_mdio_reg:
    for (queue = 0; queue < priv->plat->rx_queues_to_use; queue++) {
        struct xj3_rx_queue *rx_q = &priv->rx_queue[queue];
        netif_napi_del(&rx_q->napi);
    }
err_hw_init:
    free_netdev(ndev);
    return ret;
}

static int xj3_eth_dwmac_config_dt(struct platform_device *pdev,
                                   struct plat_config_data *plat_dat) {
    struct device_node *np = pdev->dev.of_node;
    u32 burst_map = 0;
    u32 bit_index = 0;
    u32 a_index = 0;

    if (!plat_dat->axi) {
        plat_dat->axi = kzalloc(sizeof(struct xj3_axi), GFP_KERNEL);
        if (!plat_dat->axi) return -ENOMEM;
    }

    plat_dat->axi->axi_lpi_en = of_property_read_bool(np, "snps,en-lpi");
    if (of_property_read_u32(np, "snps,write-requests",
                             &plat_dat->axi->axi_wr_osr_lmt))
        plat_dat->axi->axi_wr_osr_lmt = 1;
    else
        plat_dat->axi->axi_wr_osr_lmt--;

    if (of_property_read_u32(np, "snps,read-requests",
                             &plat_dat->axi->axi_rd_osr_lmt))
        plat_dat->axi->axi_rd_osr_lmt = 1;
    else
        plat_dat->axi->axi_rd_osr_lmt--;

    of_property_read_u32(np, "snps,burst-map", &burst_map);

    for (bit_index = 0; bit_index < 7; bit_index++) {
        if (burst_map & (1 << bit_index)) {
            switch (bit_index) {
                case 0:
                    plat_dat->axi->axi_blen[a_index] = 4;
                    break;
                case 1:
                    plat_dat->axi->axi_blen[a_index] = 8;
                    break;
                case 2:
                    plat_dat->axi->axi_blen[a_index] = 16;
                    break;
                case 3:
                    plat_dat->axi->axi_blen[a_index] = 32;
                    break;
                case 4:
                    plat_dat->axi->axi_blen[a_index] = 64;
                    break;
                case 5:
                    plat_dat->axi->axi_blen[a_index] = 128;
                    break;
                case 6:
                    plat_dat->axi->axi_blen[a_index] = 256;
                    break;
                default:
                    break;
            }
            a_index++;
        }
    }

    plat_dat->has_gmac4 = 1;
    plat_dat->dma_cfg->aal = 1;
    plat_dat->pmt = 1;
    plat_dat->fp_en = true;
    plat_dat->est_en = false;
    return 0;
}

#ifdef CONFIG_HOBOT_DIAG
static void hobot_eth_diag_test(void *p, size_t len) { eth_err_flag = 1; }
#endif

static int hobot_eth_probe(struct platform_device *pdev) {
    struct plat_config_data *plat_dat;

    struct resource *res;
    struct xj3_resource xj3_res;
    int ret = -ENXIO;

    memset(&xj3_res, 0, sizeof(struct xj3_resource));
    xj3_res.irq = platform_get_irq_byname(pdev, "mac-irq");

#ifdef HOBOT_USE_IRQ_SPLIT
    xj3_res.tx_irq = platform_get_irq_byname(pdev, "tx-irq");
    xj3_res.rx_irq = platform_get_irq_byname(pdev, "rx-irq");
#endif

    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!res) {
        dev_info(&pdev->dev, "%s, get plat resouce failed\n", __func__);
        ret = -ENXIO;
        goto err_get_res;
    }
    xj3_res.addr = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(xj3_res.addr)) {
        dev_info(&pdev->dev, "%s, error ioremap\n", __func__);
        ret = PTR_ERR(xj3_res.addr);
        goto err_get_res;
    }

    plat_dat = xj3_probe_config_dt(pdev, &xj3_res.mac);
    if (IS_ERR(plat_dat)) {
        return PTR_ERR(plat_dat);
    }

    ret = xj3_eth_dwmac_config_dt(pdev, plat_dat);
    if (ret) goto remove;

    ret = xj3_dvr_probe(&pdev->dev, plat_dat, &xj3_res);
    if (ret) goto remove;
#ifdef CONFIG_HOBOT_DIAG
    if (diag_register(ModuleDiag_eth, EventIdEthDmaBusErr, 4,
			DIAG_MSG_INTERVAL_MIN, DIAG_MSG_INTERVAL_MAX,
			hobot_eth_diag_test) < 0)
        dev_err(&pdev->dev, "eth diag register fail\n");
#endif
    dev_info(&pdev->dev, "%s: probe sucessfully\n", __func__);
    return ret;
remove:

err_get_res:

    return ret;
}

static int hgb_remove(struct platform_device *pdev) {
    struct net_device *ndev = platform_get_drvdata(pdev);
    struct xj3_priv *priv = netdev_priv(ndev);

    dev_dbg(&pdev->dev, "%s\n", __func__);
    xj3_stop_all_dma(priv);
    xj3_set_mac(priv->ioaddr, false);
    netif_carrier_off(ndev);
    unregister_netdev(ndev);
    clk_disable_unprepare(priv->plat->xj3_phy_ref_clk);
    clk_disable_unprepare(priv->plat->xj3_pclk);
    clk_disable_unprepare(priv->plat->xj3_mac_div_clk);
    clk_disable_unprepare(priv->plat->xj3_mac_pre_div_clk);
    clk_disable_unprepare(priv->plat->clk_ptp_ref);

    if (priv->mii) {
        mdiobus_unregister(priv->mii);
        priv->mii->priv = NULL;
        mdiobus_free(priv->mii);
        priv->mii = NULL;
    }
    free_netdev(ndev);
    of_node_put(priv->plat->phy_node);
    of_node_put(priv->plat->mdio_node);
    dev_info(priv->device, "%s, successufully exit\n", __func__);
#ifdef CONFIG_HOBOT_DMC_CLK
    hobot_dfs_unregister(&priv->eth_dfs);
#endif
    return 0;
}

static const struct of_device_id hgb_of_match[] = {
    {
        .compatible = "snps,dwc-qos-ethernet-4.10a",
    },
    {}};

MODULE_DEVICE_TABLE(of, hgb_of_match);

#ifdef CONFIG_PM_SLEEP
static int hobot_eth_suspend(struct device *dev) {
    struct net_device *ndev = dev_get_drvdata(dev);
    struct xj3_priv *priv = netdev_priv(ndev);
    u32 rx_cnt = priv->plat->rx_queues_to_use;
    u32 queue;

    if (!ndev) {
        dev_info(priv->device, "%s, and ndev is NULL\n", __func__);
        return 0;
    }

    if (!netif_running(ndev)) {
        return 0;
    }

    if (ndev->phydev) {
        phy_stop(ndev->phydev);
        /* stop phy state machine sync, so no xj3_adjust_link happen after this. */
        phy_stop_machine(ndev->phydev);
    }
    netif_device_detach(ndev);
    netif_carrier_off(ndev);

    for (queue = 0; queue < rx_cnt; queue++) {
        struct xj3_rx_queue *rx_q = &priv->rx_queue[queue];
        napi_synchronize(&rx_q->napi);
    }

    xj3_stop_all_queues(priv);
    xj3_disable_all_queues(priv);

    xj3_stop_all_dma(priv);
    xj3_link_down(priv);

    xj3_set_mac(priv->ioaddr, false);
    del_timer_sync(&priv->txtimer);

    clk_disable_unprepare(priv->plat->xj3_phy_ref_clk);
    clk_disable_unprepare(priv->plat->xj3_pclk);
    clk_disable_unprepare(priv->plat->xj3_mac_div_clk);
    clk_disable_unprepare(priv->plat->xj3_mac_pre_div_clk);
    clk_disable_unprepare(priv->plat->clk_ptp_ref);

    priv->link = 0;
    priv->speed = 0;
    priv->duplex = DUPLEX_UNKNOWN;

    return 0;
}

static int hobot_eth_resume(struct device *dev) {
    struct net_device *ndev = dev_get_drvdata(dev);
    struct xj3_priv *priv = netdev_priv(ndev);
    u32 val;

    if (!netif_running(ndev)) {
        return 0;
    }

    rtnl_lock();

    if (priv->plat->interface == PHY_INTERFACE_MODE_RMII) {
        // set enet rmii and clk turn on
        val = ioread32(priv->plat->sub_addr);
        val &= 0xFFFFFFF0;
        val |= 0xC;
        iowrite32(val, priv->plat->sub_addr);
    } else {
        // set enet rgmii and clk turn on
        val = ioread32(priv->plat->sub_addr);
        val &= 0xFFFFFFF0;
        val |= 0x9;
        iowrite32(val, priv->plat->sub_addr);
    }

    clk_prepare_enable(priv->plat->xj3_mac_pre_div_clk);
    clk_prepare_enable(priv->plat->xj3_mac_div_clk);
    clk_prepare_enable(priv->plat->xj3_phy_ref_clk);
    clk_prepare_enable(priv->plat->xj3_pclk);
    clk_prepare_enable(priv->plat->clk_ptp_ref);
	if (!IS_ERR_OR_NULL(priv->plat->phyreset)) {
		gpiod_set_value(priv->plat->phyreset, 0);
		msleep(20);
		gpiod_set_value(priv->plat->phyreset, 1);
		msleep(100);
	}
    hobot_reset_queues_param(priv);

    priv->mss = 0;
    xj3_clear_descriptors(priv);
    xj3_hw_setup(ndev, false);
    hobot_init_intr_coalesce(priv);
    xj3_set_rx_mode(ndev);
    xj3_enable_all_queues(priv);
    xj3_start_all_queues(priv);

    rtnl_unlock();
    netif_device_attach(ndev);

    if (ndev->phydev) {
        phy_init_hw(ndev->phydev);
        phy_start(ndev->phydev);
    }

    return 0;
}

static SIMPLE_DEV_PM_OPS(hobot_pm_ops, hobot_eth_suspend, hobot_eth_resume);
#endif

static struct platform_driver hgb_driver = {
    .probe = hobot_eth_probe,
    .remove = hgb_remove,
    .driver =
        {
            .name = DRIVER_NAME,
#ifdef CONFIG_PM_SLEEP
            .pm = &hobot_pm_ops,
#endif
            .of_match_table = hgb_of_match,
        },

};

static int tsn_enabled_set(const char *val, const struct kernel_param *kp) {
	int ret;

	ret = param_set_bool(val, kp);
	if (ret < 0)
		return ret;
	return 0;
}


static struct kernel_param_ops tsn_enabled_param_ops = {
	.set =		tsn_enabled_set,
	.get =		param_get_bool,
};

module_param_cb(tsn_enabled, &tsn_enabled_param_ops, &tsn_enabled, 0644);
module_platform_driver(hgb_driver);
MODULE_LICENSE("GPL v2");
