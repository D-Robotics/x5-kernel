/*
 *  Copyright (C) 2020 Hobot Robotics.
 * This code from  horizon Ethernet Quality-of-Service v5.10a linux driver
 *
 * horizon Ethernet QoS IP version 5.10a (GMAC).
 *
 * 
 * This is file for ethtool to get mac mmc counters,
 * inlcude tx, rx counters, 
 * IPC, IPV4, IPV6 and others Protocols counters.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/io.h>
#include "hobot_eth_mmc.h"
#include "hobot_eth_jplus.h"
/* define for mmc counters */
#define MMC_CNTRL 0x00
#define MMC_RX_INTR 0x04
#define MMC_TX_INTR 0x08
#define MMC_RX_INTR_MASK 0x0c
#define MMC_TX_INTR_MASK 0x10
#define MMC_DEFAULT_MASK 0xFFFFFFFF


/**
 * hobot_eth_mmc_ctrl
 * @mmcaddr: mmc register address pointer
 * @mode: mode
 * Description: set mmc control register.
 * called by mmc_setup
 * Return: NA
 */
void hobot_eth_mmc_ctrl(void __iomem *mmcaddr, unsigned int mode)
{
    u32 value = ioreadl(mmcaddr, MMC_CNTRL);
	/* get bit0--bit5 */
    value |= (mode & 0x3F);
    iowritel(value, mmcaddr, MMC_CNTRL);

}

/**
 * hobot_eth_mmc_intr_all_mask
 * @mmcaddr: mmc register address pointer
 * Description: set mmc intr mask register.
 * called by mmc_setup
 * Return: NA
 */
void hobot_eth_mmc_intr_all_mask(void __iomem *mmcaddr)
{
	/* set all mask */
    iowritel(MMC_DEFAULT_MASK, mmcaddr, MMC_RX_INTR_MASK);
    iowritel(MMC_DEFAULT_MASK, mmcaddr, MMC_TX_INTR_MASK);
    iowritel(MMC_DEFAULT_MASK, mmcaddr, MMC_RX_IPC_INTR_MASK);
}


/**
 * mmc_read_rx_counter
 * @mmcaddr: mmc register address pointer
 * @mmc: hobot counter pointer
 * Description: read mmc rx counter register.
 * called by mmc_read
 * Return: NA
 */
static void mmc_read_rx_counter(void __iomem *mmcaddr, struct hobot_counters *mmc)
{
	 /* read MMC RX counter registers */
    mmc->mmc_rx_framecount_gb += ioreadl(mmcaddr, MMC_RX_FRAMECOUNT_GB);
    mmc->mmc_rx_octetcount_gb += ioreadl(mmcaddr, MMC_RX_OCTETCOUNT_GB);
    mmc->mmc_rx_octetcount_g += ioreadl(mmcaddr, MMC_RX_OCTETCOUNT_G);
    mmc->mmc_rx_broadcastframe_g += ioreadl(mmcaddr, MMC_RX_BROADCASTFRAME_G);
	/* rx multicast */
    mmc->mmc_rx_multicastframe_g += ioreadl(mmcaddr, MMC_RX_MULTICASTFRAME_G);
    mmc->mmc_rx_crc_error += ioreadl(mmcaddr, MMC_RX_CRC_ERROR);
    mmc->mmc_rx_align_error += ioreadl(mmcaddr, MMC_RX_ALIGN_ERROR);
    mmc->mmc_rx_run_error += ioreadl(mmcaddr, MMC_RX_RUN_ERROR);
    mmc->mmc_rx_jabber_error += ioreadl(mmcaddr, MMC_RX_JABBER_ERROR);
    mmc->mmc_rx_undersize_g += ioreadl(mmcaddr, MMC_RX_UNDERSIZE_G);
    mmc->mmc_rx_oversize_g += ioreadl(mmcaddr, MMC_RX_OVERSIZE_G);
	/* octets */
    mmc->mmc_rx_64_octets_gb += ioreadl(mmcaddr, MMC_RX_64_OCTETS_GB);
    mmc->mmc_rx_65_to_127_octets_gb += ioreadl(mmcaddr, MMC_RX_65_TO_127_OCTETS_GB);
    mmc->mmc_rx_128_to_255_octets_gb += ioreadl(mmcaddr, MMC_RX_128_TO_255_OCTETS_GB);
    mmc->mmc_rx_256_to_511_octets_gb += ioreadl(mmcaddr, MMC_RX_256_TO_511_OCTETS_GB);
    mmc->mmc_rx_512_to_1023_octets_gb += ioreadl(mmcaddr, MMC_RX_512_TO_1023_OCTETS_GB);
    mmc->mmc_rx_1024_to_max_octets_gb += ioreadl(mmcaddr, MMC_RX_1024_TO_MAX_OCTETS_GB);
	/* rx unicast */
    mmc->mmc_rx_unicast_g += ioreadl(mmcaddr, MMC_RX_UNICAST_G);
    mmc->mmc_rx_length_error += ioreadl(mmcaddr, MMC_RX_LENGTH_ERROR);
    mmc->mmc_rx_autofrangetype += ioreadl(mmcaddr, MMC_RX_AUTOFRANGETYPE);
	/* rx_pause_frames */
    mmc->mmc_rx_pause_frames += ioreadl(mmcaddr, MMC_RX_PAUSE_FRAMES);
    mmc->mmc_rx_fifo_overflow += ioreadl(mmcaddr, MMC_RX_FIFO_OVERFLOW);
	/* rx_vlan_frames */
    mmc->mmc_rx_vlan_frames_gb += ioreadl(mmcaddr, MMC_RX_VLAN_FRAMES_GB);
	/* rx_watchdog_error */
    mmc->mmc_rx_watchdog_error += ioreadl(mmcaddr, MMC_RX_WATCHDOG_ERROR);
}



/**
 * mmc_read_tx_counter
 * @mmcaddr: mmc register address pointer
 * @mmc: hobot counter pointer
 * Description: read mmc tx counter register.
 * called by mmc_read
 * Return: NA
 */
static void mmc_read_tx_counter(void __iomem *mmcaddr, struct hobot_counters *mmc)
{
	/* read MMC TX counter registers */
	mmc->mmc_tx_octetcount_gb += ioreadl(mmcaddr, MMC_TX_OCTETCOUNT_GB);
    mmc->mmc_tx_framecount_gb += ioreadl(mmcaddr, MMC_TX_FRAMECOUNT_GB);
    mmc->mmc_tx_broadcastframe_g += ioreadl(mmcaddr, MMC_TX_BROADCASTFRAME_G);
    mmc->mmc_tx_multicastframe_g += ioreadl(mmcaddr, MMC_TX_MULTICASTFRAME_G);
    mmc->mmc_tx_64_octets_gb += ioreadl(mmcaddr, MMC_TX_64_OCTETS_GB);
    mmc->mmc_tx_65_to_127_octets_gb += ioreadl(mmcaddr, MMC_TX_65_TO_127_OCTETS_GB);
    mmc->mmc_tx_128_to_255_octets_gb += ioreadl(mmcaddr, MMC_TX_128_TO_255_OCTETS_GB);
    mmc->mmc_tx_256_to_511_octets_gb += ioreadl(mmcaddr, MMC_TX_256_TO_511_OCTETS_GB);
    mmc->mmc_tx_512_to_1023_octets_gb += ioreadl(mmcaddr, MMC_TX_512_TO_1023_OCTETS_GB);
    mmc->mmc_tx_1024_to_max_octets_gb += ioreadl(mmcaddr, MMC_TX_1024_TO_MAX_OCTETS_GB);
	/* tx unicast */
    mmc->mmc_tx_unicast_gb += ioreadl(mmcaddr, MMC_TX_UNICAST_GB);
	/* tx multicast */
    mmc->mmc_tx_multicast_gb += ioreadl(mmcaddr, MMC_TX_MULTICAST_GB);
	/* tx broadcast */
    mmc->mmc_tx_broadcast_gb += ioreadl(mmcaddr, MMC_TX_BROADCAST_GB);
    mmc->mmc_tx_underflow_error += ioreadl(mmcaddr, MMC_TX_UNDERFLOW_ERROR);
    mmc->mmc_tx_singlecol_g += ioreadl(mmcaddr, MMC_TX_SINGLECOL_G);
    mmc->mmc_tx_multicol_g += ioreadl(mmcaddr, MMC_TX_MULTICOL_G);
    mmc->mmc_tx_deferred += ioreadl(mmcaddr, MMC_TX_DEFERRED);
    mmc->mmc_tx_latecol += ioreadl(mmcaddr, MMC_TX_LATECOL);
    mmc->mmc_tx_exesscol += ioreadl(mmcaddr, MMC_TX_EXESSCOL);
    mmc->mmc_tx_carrier_error += ioreadl(mmcaddr, MMC_TX_CARRIER_ERROR);
    mmc->mmc_tx_octetcount_g += ioreadl(mmcaddr, MMC_TX_OCTETCOUNT_G);
    mmc->mmc_tx_framecount_g += ioreadl(mmcaddr, MMC_TX_FRAMECOUNT_G);
    mmc->mmc_tx_excessdef += ioreadl(mmcaddr, MMC_TX_EXCESSDEF);
	/* tx_pause_frame */
    mmc->mmc_tx_pause_frame += ioreadl(mmcaddr, MMC_TX_PAUSE_FRAME);
	/* tx_vlan_frame */
    mmc->mmc_tx_vlan_frame_g += ioreadl(mmcaddr, MMC_TX_VLAN_FRAME_G);
}




/**
 * hobot_eth_mmc_read
 * @mmcaddr: mmc register address pointer
 * @mmc: hobot counter pointer
 * Description: read all mmc counter register, include tx,rx,IPC,....
 * main read function for called
 * called by get_ethtool_stats in hobot_eth_jplus.c file
 * Return: NA
 */
void hobot_eth_mmc_read(void __iomem *mmcaddr, struct hobot_counters *mmc)
{
	/* MMC TX counter registers */
    mmc_read_tx_counter(mmcaddr, mmc);
    /* MMC RX counter registers */
    mmc_read_rx_counter(mmcaddr, mmc);
    /* IPC */
    mmc->mmc_rx_ipc_intr_mask += ioreadl(mmcaddr, MMC_RX_IPC_INTR_MASK);
    mmc->mmc_rx_ipc_intr += ioreadl(mmcaddr, MMC_RX_IPC_INTR);
    /* IPv4 */
    mmc->mmc_rx_ipv4_gd += ioreadl(mmcaddr, MMC_RX_IPV4_GD);
    mmc->mmc_rx_ipv4_hderr += ioreadl(mmcaddr, MMC_RX_IPV4_HDERR);
    mmc->mmc_rx_ipv4_nopay += ioreadl(mmcaddr, MMC_RX_IPV4_NOPAY);
    mmc->mmc_rx_ipv4_frag += ioreadl(mmcaddr, MMC_RX_IPV4_FRAG);
    mmc->mmc_rx_ipv4_udsbl += ioreadl(mmcaddr, MMC_RX_IPV4_UDSBL);

    mmc->mmc_rx_ipv4_gd_octets += ioreadl(mmcaddr, MMC_RX_IPV4_GD_OCTETS);
    mmc->mmc_rx_ipv4_hderr_octets += ioreadl(mmcaddr, MMC_RX_IPV4_HDERR_OCTETS);
    mmc->mmc_rx_ipv4_nopay_octets += ioreadl(mmcaddr, MMC_RX_IPV4_NOPAY_OCTETS);
    mmc->mmc_rx_ipv4_frag_octets += ioreadl(mmcaddr, MMC_RX_IPV4_FRAG_OCTETS);
    mmc->mmc_rx_ipv4_udsbl_octets += ioreadl(mmcaddr, MMC_RX_IPV4_UDSBL_OCTETS);

    /* IPV6 */
    mmc->mmc_rx_ipv6_gd_octets += ioreadl(mmcaddr, MMC_RX_IPV6_GD_OCTETS);
    mmc->mmc_rx_ipv6_hderr_octets += ioreadl(mmcaddr, MMC_RX_IPV6_HDERR_OCTETS);
    mmc->mmc_rx_ipv6_nopay_octets += ioreadl(mmcaddr, MMC_RX_IPV6_NOPAY_OCTETS);

    mmc->mmc_rx_ipv6_gd += ioreadl(mmcaddr, MMC_RX_IPV6_GD);
    mmc->mmc_rx_ipv6_hderr += ioreadl(mmcaddr, MMC_RX_IPV6_HDERR);
    mmc->mmc_rx_ipv6_nopay += ioreadl(mmcaddr, MMC_RX_IPV6_NOPAY);
    /* Protocols */
    mmc->mmc_rx_udp_gd += ioreadl(mmcaddr, MMC_RX_UDP_GD);
    mmc->mmc_rx_udp_err += ioreadl(mmcaddr, MMC_RX_UDP_ERR);
    mmc->mmc_rx_tcp_gd += ioreadl(mmcaddr, MMC_RX_TCP_GD);
    mmc->mmc_rx_tcp_err += ioreadl(mmcaddr, MMC_RX_TCP_ERR);
    mmc->mmc_rx_icmp_gd += ioreadl(mmcaddr, MMC_RX_ICMP_GD);
    mmc->mmc_rx_icmp_err += ioreadl(mmcaddr, MMC_RX_ICMP_ERR);
	/* Protocols octets */
	/* udp */
    mmc->mmc_rx_udp_gd_octets += ioreadl(mmcaddr, MMC_RX_UDP_GD_OCTETS);
    mmc->mmc_rx_udp_err_octets += ioreadl(mmcaddr, MMC_RX_UDP_ERR_OCTETS);
	/* tcp */
    mmc->mmc_rx_tcp_gd_octets += ioreadl(mmcaddr, MMC_RX_TCP_GD_OCTETS);
    mmc->mmc_rx_tcp_err_octets += ioreadl(mmcaddr, MMC_RX_TCP_ERR_OCTETS);
	/* icmp */
    mmc->mmc_rx_icmp_gd_octets += ioreadl(mmcaddr, MMC_RX_ICMP_GD_OCTETS);
    mmc->mmc_rx_icmp_err_octets += ioreadl(mmcaddr, MMC_RX_ICMP_ERR_OCTETS);
}
