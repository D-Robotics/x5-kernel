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

#include <linux/ethtool.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/net_tstamp.h>
#include <linux/netdevice.h>
#include <linux/phy.h>
#include <linux/stat.h>

#include "hobot_reg.h"
#include "hobot_eth.h"

#define MAX_DRIVER_NAME_SIZE 20
#define MAX_VERSION_SIZE 20

static void xj3_get_drv_info(struct net_device *ndev,
                             struct ethtool_drvinfo *ed) {
    const struct xj3_priv *priv = netdev_priv(ndev);

    snprintf(ed->driver, MAX_DRIVER_NAME_SIZE, "%s",
             priv->device->driver->name);
    snprintf(ed->version, MAX_VERSION_SIZE, "%s", DRIVER_VERSION);
}

static void xj3_get_strings(struct net_device *ndev, u32 stringset, u8 *data) {
    size_t i;
    u8 *p = data;
    struct xj3_priv *priv = netdev_priv(ndev);

    switch (stringset) {
        case ETH_SS_STATS:
#if 1
            if (priv->dma_cap.rmon) {
                for (i = 0; i < STMMAC_MMC_STATS_LEN; i++) {
                    memcpy(p, hobot_mmc[i].stat_name, ETH_GSTRING_LEN);
                    p += ETH_GSTRING_LEN;
                }
            }
#endif
            for (i = 0; i < STMMAC_STATS_LEN; i++) {
                memcpy(p, stmmac_gstrings_stats[i].stat_name, ETH_GSTRING_LEN);
                p += ETH_GSTRING_LEN;
            }

            break;

        default:
            WARN_ON(1);
            break;
    }
}

static int xj3_get_sset_count(struct net_device *ndev, int sset) {
    struct xj3_priv *priv = netdev_priv(ndev);
    int len;

    switch (sset) {
        case ETH_SS_STATS:
            len = STMMAC_STATS_LEN;
#if 1
            if (priv->dma_cap.rmon) len += STMMAC_MMC_STATS_LEN;
#endif
            return len;
        default:
            return -EOPNOTSUPP;
    }
}

static void xj3_get_ethtool_stats(struct net_device *ndev,
                                  struct ethtool_stats *dummy, u64 *data) {
    struct xj3_priv *priv = netdev_priv(ndev);
    int i, j = 0;

    if (priv->dma_cap.rmon) {
        hobot_mmc_read(priv->mmcaddr, &priv->mmc);

        for (i = 0; i < STMMAC_MMC_STATS_LEN; i++) {
            char *p;
            p = (char *)priv + hobot_mmc[i].stat_offset;
            data[j++] = (hobot_mmc[i].sizeof_stat == sizeof(u64)) ? (*(u64 *)p)
                                                                  : (*(u32 *)p);
        }
    }
    for (i = 0; i < STMMAC_STATS_LEN; i++) {
        char *p = (char *)priv + stmmac_gstrings_stats[i].stat_offset;
        data[j++] = (stmmac_gstrings_stats[i].sizeof_stat == sizeof(u64))
                        ? (*(u64 *)p)
                        : (*(u32 *)p);
    }
}

static int xj3_ethtool_get_regs_len(struct net_device *ndev) {
    return REG_SPACE_SIZE;
}

static void xj3_dump_mac_regs(void __iomem *ioaddr, u32 *reg_space) {
    int i;

    for (i = 0; i < GMAC_REG_NUM; i++) reg_space[i] = readl(ioaddr + i * 4);
}

static void __xj3_dump_dma_ch_regs(void __iomem *ioaddr, u32 channel,
                                   u32 *reg_space) {
    reg_space[DMA_CHAN_CONTROL(channel) / 4] =
        readl(ioaddr + DMA_CHAN_CONTROL(channel));
    reg_space[DMA_CHAN_TX_CONTROL(channel) / 4] =
        readl(ioaddr + DMA_CHAN_TX_CONTROL(channel));

    reg_space[DMA_CHAN_RX_CONTROL(channel) / 4] =
        readl(ioaddr + DMA_CHAN_RX_CONTROL(channel));

    reg_space[DMA_CHAN_TX_BASE_ADDR(channel) / 4] =
        readl(ioaddr + DMA_CHAN_TX_BASE_ADDR(channel));
    reg_space[DMA_CHAN_RX_BASE_ADDR(channel) / 4] =
        readl(ioaddr + DMA_CHAN_RX_BASE_ADDR(channel));

    reg_space[DMA_CHAN_TX_END_ADDR(channel) / 4] =
        readl(ioaddr + DMA_CHAN_TX_END_ADDR(channel));
    reg_space[DMA_CHAN_RX_END_ADDR(channel) / 4] =
        readl(ioaddr + DMA_CHAN_RX_END_ADDR(channel));

    reg_space[DMA_CHAN_TX_RING_LEN(channel) / 4] =
        readl(ioaddr + DMA_CHAN_TX_RING_LEN(channel));
    reg_space[DMA_CHAN_RX_RING_LEN(channel) / 4] =
        readl(ioaddr + DMA_CHAN_RX_RING_LEN(channel));

    reg_space[DMA_CHAN_INTR_ENA(channel) / 4] =
        readl(ioaddr + DMA_CHAN_INTR_ENA(channel));

    reg_space[DMA_CHAN_RX_WATCHDOG(channel) / 4] =
        readl(ioaddr + DMA_CHAN_RX_WATCHDOG(channel));

    reg_space[DMA_CHAN_SLOT_CTRL_STATUS(channel) / 4] =
        readl(ioaddr + DMA_CHAN_SLOT_CTRL_STATUS(channel));

    reg_space[DMA_CHAN_CUR_TX_DESC(channel) / 4] =
        readl(ioaddr + DMA_CHAN_CUR_TX_DESC(channel));
    reg_space[DMA_CHAN_CUR_RX_DESC(channel) / 4] =
        readl(ioaddr + DMA_CHAN_CUR_RX_DESC(channel));

    reg_space[DMA_CHAN_CUR_TX_BUF_ADDR(channel) / 4] =
        readl(ioaddr + DMA_CHAN_CUR_TX_BUF_ADDR(channel));
    reg_space[DMA_CHAN_CUR_RX_BUF_ADDR(channel) / 4] =
        readl(ioaddr + DMA_CHAN_CUR_RX_BUF_ADDR(channel));

    reg_space[DMA_CHAN_STATUS(channel) / 4] =
        readl(ioaddr + DMA_CHAN_STATUS(channel));
}

static void xj3_dump_dma_ch_regs(void __iomem *ioaddr, u32 *reg_space) {
    int i;

    for (i = 0; i < DMA_CHANNEL_NB_MAX; i++)
        __xj3_dump_dma_ch_regs(ioaddr, i, reg_space);
}

static void xj3_ethtool_gregs(struct net_device *ndev,
                              struct ethtool_regs *regs, void *space) {
    u32 *reg_space = (u32 *)space;
    struct xj3_priv *priv = netdev_priv(ndev);
    int i;

    memset(reg_space, 0x0, REG_SPACE_SIZE);
    xj3_dump_mac_regs(priv->ioaddr, reg_space);

    for (i = 0; i < 14; i++) {
        reg_space[ETHTOOL_DMA_OFFSET + i] =
            readl(priv->ioaddr + DMA_BUS_MODE + i * 4);
    }

    xj3_dump_dma_ch_regs(priv->ioaddr, reg_space);
}

static int xj3_get_ts_info(struct net_device *dev,
                           struct ethtool_ts_info *info) {
    struct xj3_priv *priv = netdev_priv(dev);

    if ((priv->dma_cap.time_stamp || priv->dma_cap.atime_stamp)) {
        info->so_timestamping =
            SOF_TIMESTAMPING_TX_SOFTWARE | SOF_TIMESTAMPING_TX_HARDWARE |
            SOF_TIMESTAMPING_RX_SOFTWARE | SOF_TIMESTAMPING_RX_HARDWARE |
            SOF_TIMESTAMPING_SOFTWARE | SOF_TIMESTAMPING_RAW_HARDWARE;

        if (priv->ptp_clock) info->phc_index = ptp_clock_index(priv->ptp_clock);
        info->tx_types = (1 << HWTSTAMP_TX_OFF) | (1 << HWTSTAMP_TX_ON);
        info->rx_filters = ((1 << HWTSTAMP_FILTER_NONE) |
                            (1 << HWTSTAMP_FILTER_PTP_V1_L4_EVENT) |
                            (1 << HWTSTAMP_FILTER_PTP_V1_L4_SYNC) |
                            (1 << HWTSTAMP_FILTER_PTP_V1_L4_DELAY_REQ) |
                            (1 << HWTSTAMP_FILTER_PTP_V2_L4_EVENT) |
                            (1 << HWTSTAMP_FILTER_PTP_V2_L4_SYNC) |
                            (1 << HWTSTAMP_FILTER_PTP_V2_L4_DELAY_REQ) |
                            (1 << HWTSTAMP_FILTER_PTP_V2_EVENT) |
                            (1 << HWTSTAMP_FILTER_PTP_V2_SYNC) |
                            (1 << HWTSTAMP_FILTER_PTP_V2_DELAY_REQ) |
                            (1 << HWTSTAMP_FILTER_ALL));
        return 0;
    } else {
        return ethtool_op_get_ts_info(dev, info);
    }
}

static u32 hobot_riwt2usec(u32 riwt, struct xj3_priv *priv) {
    unsigned long clk = clk_get_rate(priv->plat->xj3_mac_div_clk);

    if (!clk) return 0;

    return (riwt * 256) / (clk / 1000000);
}

static int hobot_get_coalesce(struct net_device *dev,
                              struct ethtool_coalesce *ec,
                              struct kernel_ethtool_coalesce *kernel_coal,
			                  struct netlink_ext_ack *extack) {
    struct xj3_priv *priv = netdev_priv(dev);

    ec->tx_coalesce_usecs = priv->tx_coal_timer;
    ec->tx_max_coalesced_frames = priv->tx_coal_frames;

    if (priv->use_riwt) {
        ec->rx_coalesce_usecs = hobot_riwt2usec(priv->rx_riwt, priv);
        ec->rx_max_coalesced_frames = priv->rx_coal_frames;
    }
    return 0;
}

static u32 hobot_usec2riwt(u32 usec, struct xj3_priv *priv) {
    unsigned long clk = clk_get_rate(priv->plat->xj3_mac_div_clk);

    if (!clk) return 0;

    return (usec * (clk / 1000000)) / 256;
}

void hobot_dma_rx_watchdog(struct xj3_priv *priv, u32 riwt, u32 number) {
    u32 chan;

    for (chan = 0; chan < number; chan++) {
        writel(riwt, priv->ioaddr + DMA_CHAN_RX_WATCHDOG(chan));
    }
}
EXPORT_SYMBOL_GPL(hobot_dma_rx_watchdog);

static int hobot_set_coalesce(struct net_device *dev,
                              struct ethtool_coalesce *ec,
                              struct kernel_ethtool_coalesce *kernel_coal,
			                  struct netlink_ext_ack *extack) {
    struct xj3_priv *priv = netdev_priv(dev);
    u32 rx_cnt = priv->plat->rx_queues_to_use;
    unsigned int rx_riwt;

    /* Check not supported parameters  */
    if ((ec->rx_coalesce_usecs_irq) || (ec->rx_max_coalesced_frames_irq) ||
        (ec->tx_coalesce_usecs_irq) || (ec->use_adaptive_rx_coalesce) ||
        (ec->use_adaptive_tx_coalesce) || (ec->pkt_rate_low) ||
        (ec->rx_coalesce_usecs_low) || (ec->rx_max_coalesced_frames_low) ||
        (ec->tx_coalesce_usecs_high) || (ec->tx_max_coalesced_frames_low) ||
        (ec->pkt_rate_high) || (ec->tx_coalesce_usecs_low) ||
        (ec->rx_coalesce_usecs_high) || (ec->rx_max_coalesced_frames_high) ||
        (ec->tx_max_coalesced_frames_irq) || (ec->stats_block_coalesce_usecs) ||
        (ec->tx_max_coalesced_frames_high) || (ec->rate_sample_interval))
        return -EOPNOTSUPP;

    if (priv->use_riwt && (ec->rx_coalesce_usecs > 0)) {
        rx_riwt = hobot_usec2riwt(ec->rx_coalesce_usecs, priv);

        if ((rx_riwt > MAX_DMA_RIWT) || (rx_riwt < MIN_DMA_RIWT))
            return -EINVAL;

        priv->rx_riwt = rx_riwt;
        hobot_dma_rx_watchdog(priv, priv->rx_riwt, rx_cnt);
    }

    if ((ec->tx_coalesce_usecs == 0) && (ec->tx_max_coalesced_frames == 0))
        return -EINVAL;

    if ((ec->tx_coalesce_usecs > HOBOT_MAX_COAL_TX_TICK) ||
        (ec->tx_max_coalesced_frames > HOBOT_TX_MAX_FRAMES))
        return -EINVAL;

    priv->tx_coal_frames = ec->tx_max_coalesced_frames;
    priv->tx_coal_timer = ec->tx_coalesce_usecs;
    priv->rx_coal_frames = ec->rx_max_coalesced_frames;
    return 0;
}

static const struct ethtool_ops hobot_ethtool_ops = {
    .supported_coalesce_params =  ETHTOOL_COALESCE_USECS
                                | ETHTOOL_COALESCE_MAX_FRAMES,
    .get_drvinfo = xj3_get_drv_info,
    .get_link = ethtool_op_get_link,
    .get_link_ksettings = phy_ethtool_get_link_ksettings,
    .set_link_ksettings = phy_ethtool_set_link_ksettings,
    .get_strings = xj3_get_strings,
    .get_ethtool_stats = xj3_get_ethtool_stats,
    .get_sset_count = xj3_get_sset_count,
    .get_regs_len = xj3_ethtool_get_regs_len,
    .get_regs = xj3_ethtool_gregs,
    .get_ts_info = xj3_get_ts_info,
    .get_coalesce = hobot_get_coalesce,
    .set_coalesce = hobot_set_coalesce,
};

void hobot_set_ethtool_ops(struct xj3_priv *priv) {
    priv->dev->ethtool_ops = &hobot_ethtool_ops;
}
EXPORT_SYMBOL_GPL(hobot_set_ethtool_ops);
