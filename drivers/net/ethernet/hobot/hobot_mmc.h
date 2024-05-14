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

#ifndef __HOBOT_MMC_H_
#define __HOBOT_MMC_H_

#define MMC_CNTRL_COUNTER_RESET 0x1
#define MMC_CNTRL_COUNTER_STOP_ROLLOVER 0x2
#define MMC_CNTRL_RESET_ON_READ 0x4
#define MMC_CNTRL_COUNTER_FREEZER 0x8

#define MMC_CNTRL_PRESET 0x10
#define MMC_CNTRL_FULL_HALF_PRESET 0x20
#define MMC_GMAC4_OFFSET 0x700

struct hobot_counters {
    unsigned int mmc_tx_octetcount_gb;
    unsigned int mmc_tx_framecount_gb;
    unsigned int mmc_tx_broadcastframe_g;
    unsigned int mmc_tx_multicastframe_g;
    unsigned int mmc_tx_64_octets_gb;
    unsigned int mmc_tx_65_to_127_octets_gb;
    unsigned int mmc_tx_128_to_255_octets_gb;
    unsigned int mmc_tx_256_to_511_octets_gb;
    unsigned int mmc_tx_512_to_1023_octets_gb;
    unsigned int mmc_tx_1024_to_max_octets_gb;
    unsigned int mmc_tx_unicast_gb;
    unsigned int mmc_tx_multicast_gb;
    unsigned int mmc_tx_broadcast_gb;
    unsigned int mmc_tx_underflow_error;
    unsigned int mmc_tx_singlecol_g;
    unsigned int mmc_tx_multicol_g;
    unsigned int mmc_tx_deferred;
    unsigned int mmc_tx_latecol;
    unsigned int mmc_tx_exesscol;
    unsigned int mmc_tx_carrier_error;
    unsigned int mmc_tx_octetcount_g;
    unsigned int mmc_tx_framecount_g;
    unsigned int mmc_tx_excessdef;
    unsigned int mmc_tx_pause_frame;
    unsigned int mmc_tx_vlan_frame_g;

    /* MMC RX counter registers */
    unsigned int mmc_rx_framecount_gb;
    unsigned int mmc_rx_octetcount_gb;
    unsigned int mmc_rx_octetcount_g;
    unsigned int mmc_rx_broadcastframe_g;
    unsigned int mmc_rx_multicastframe_g;
    unsigned int mmc_rx_crc_error;
    unsigned int mmc_rx_align_error;
    unsigned int mmc_rx_run_error;
    unsigned int mmc_rx_jabber_error;
    unsigned int mmc_rx_undersize_g;
    unsigned int mmc_rx_oversize_g;
    unsigned int mmc_rx_64_octets_gb;
    unsigned int mmc_rx_65_to_127_octets_gb;
    unsigned int mmc_rx_128_to_255_octets_gb;
    unsigned int mmc_rx_256_to_511_octets_gb;
    unsigned int mmc_rx_512_to_1023_octets_gb;
    unsigned int mmc_rx_1024_to_max_octets_gb;
    unsigned int mmc_rx_unicast_g;
    unsigned int mmc_rx_length_error;
    unsigned int mmc_rx_autofrangetype;
    unsigned int mmc_rx_pause_frames;
    unsigned int mmc_rx_fifo_overflow;
    unsigned int mmc_rx_vlan_frames_gb;
    unsigned int mmc_rx_watchdog_error;

/* IPC */
    unsigned int mmc_rx_ipc_intr_mask;
    unsigned int mmc_rx_ipc_intr;
    /* IPv4 */
    unsigned int mmc_rx_ipv4_gd;
    unsigned int mmc_rx_ipv4_hderr;
    unsigned int mmc_rx_ipv4_nopay;
    unsigned int mmc_rx_ipv4_frag;
    unsigned int mmc_rx_ipv4_udsbl;

    unsigned int mmc_rx_ipv4_gd_octets;
    unsigned int mmc_rx_ipv4_hderr_octets;
    unsigned int mmc_rx_ipv4_nopay_octets;
    unsigned int mmc_rx_ipv4_frag_octets;
    unsigned int mmc_rx_ipv4_udsbl_octets;

    /* IPV6 */
    unsigned int mmc_rx_ipv6_gd_octets;
    unsigned int mmc_rx_ipv6_hderr_octets;
    unsigned int mmc_rx_ipv6_nopay_octets;
    unsigned int mmc_rx_ipv6_gd;
    unsigned int mmc_rx_ipv6_hderr;
    unsigned int mmc_rx_ipv6_nopay;

    /* Protocols */
    unsigned int mmc_rx_udp_gd;
    unsigned int mmc_rx_udp_err;
    unsigned int mmc_rx_tcp_gd;
    unsigned int mmc_rx_tcp_err;
    unsigned int mmc_rx_icmp_gd;
    unsigned int mmc_rx_icmp_err;

    unsigned int mmc_rx_udp_gd_octets;
    unsigned int mmc_rx_udp_err_octets;
    unsigned int mmc_rx_tcp_gd_octets;
    unsigned int mmc_rx_tcp_err_octets;
    unsigned int mmc_rx_icmp_gd_octets;
    unsigned int mmc_rx_icmp_err_octets;
};

/*MMC TX COUNTER register*/
/*_GB register  stands for good and bad frames
* _G is for good only
*/

#define MMC_TX_OCTETCOUNT_GB        0x14
#define MMC_TX_FRAMECOUNT_GB        0x18
#define MMC_TX_BROADCASTFRAME_G     0x1c
#define MMC_TX_MULTICASTFRAME_G     0x20
#define MMC_TX_64_OCTETS_GB     0x24
#define MMC_TX_65_TO_127_OCTETS_GB  0x28
#define MMC_TX_128_TO_255_OCTETS_GB 0x2c
#define MMC_TX_256_TO_511_OCTETS_GB 0x30
#define MMC_TX_512_TO_1023_OCTETS_GB    0x34
#define MMC_TX_1024_TO_MAX_OCTETS_GB    0x38
#define MMC_TX_UNICAST_GB       0x3c
#define MMC_TX_MULTICAST_GB     0x40
#define MMC_TX_BROADCAST_GB     0x44
#define MMC_TX_UNDERFLOW_ERROR      0x48
#define MMC_TX_SINGLECOL_G      0x4c
#define MMC_TX_MULTICOL_G       0x50
#define MMC_TX_DEFERRED         0x54
#define MMC_TX_LATECOL          0x58
#define MMC_TX_EXESSCOL         0x5c
#define MMC_TX_CARRIER_ERROR        0x60
#define MMC_TX_OCTETCOUNT_G     0x64
#define MMC_TX_FRAMECOUNT_G     0x68
#define MMC_TX_EXCESSDEF        0x6c
#define MMC_TX_PAUSE_FRAME      0x70
#define MMC_TX_VLAN_FRAME_G     0x74

/* MMC RX counter registers */
#define MMC_RX_FRAMECOUNT_GB        0x80
#define MMC_RX_OCTETCOUNT_GB        0x84
#define MMC_RX_OCTETCOUNT_G     0x88
#define MMC_RX_BROADCASTFRAME_G     0x8c
#define MMC_RX_MULTICASTFRAME_G     0x90
#define MMC_RX_CRC_ERROR        0x94
#define MMC_RX_ALIGN_ERROR      0x98
#define MMC_RX_RUN_ERROR        0x9C
#define MMC_RX_JABBER_ERROR     0xA0
#define MMC_RX_UNDERSIZE_G      0xA4
#define MMC_RX_OVERSIZE_G       0xA8
#define MMC_RX_64_OCTETS_GB     0xAC
#define MMC_RX_65_TO_127_OCTETS_GB  0xb0
#define MMC_RX_128_TO_255_OCTETS_GB 0xb4
#define MMC_RX_256_TO_511_OCTETS_GB 0xb8
#define MMC_RX_512_TO_1023_OCTETS_GB    0xbc
#define MMC_RX_1024_TO_MAX_OCTETS_GB    0xc0
#define MMC_RX_UNICAST_G        0xc4
#define MMC_RX_LENGTH_ERROR     0xc8
#define MMC_RX_AUTOFRANGETYPE       0xcc
#define MMC_RX_PAUSE_FRAMES     0xd0
#define MMC_RX_FIFO_OVERFLOW        0xd4
#define MMC_RX_VLAN_FRAMES_GB       0xd8
#define MMC_RX_WATCHDOG_ERROR       0xdc

/* IPC*/
#define MMC_RX_IPC_INTR_MASK        0x100
#define MMC_RX_IPC_INTR         0x108
/* IPv4*/
#define MMC_RX_IPV4_GD          0x110
#define MMC_RX_IPV4_HDERR       0x114
#define MMC_RX_IPV4_NOPAY       0x118
#define MMC_RX_IPV4_FRAG        0x11C
#define MMC_RX_IPV4_UDSBL       0x120

#define MMC_RX_IPV4_GD_OCTETS       0x150
#define MMC_RX_IPV4_HDERR_OCTETS    0x154
#define MMC_RX_IPV4_NOPAY_OCTETS    0x158
#define MMC_RX_IPV4_FRAG_OCTETS     0x15c
#define MMC_RX_IPV4_UDSBL_OCTETS    0x160

/* IPV6*/
#define MMC_RX_IPV6_GD_OCTETS       0x164
#define MMC_RX_IPV6_HDERR_OCTETS    0x168
#define MMC_RX_IPV6_NOPAY_OCTETS    0x16c

#define MMC_RX_IPV6_GD          0x124
#define MMC_RX_IPV6_HDERR       0x128
#define MMC_RX_IPV6_NOPAY       0x12c

/* Protocols*/
#define MMC_RX_UDP_GD           0x130
#define MMC_RX_UDP_ERR          0x134
#define MMC_RX_TCP_GD           0x138
#define MMC_RX_TCP_ERR          0x13c
#define MMC_RX_ICMP_GD          0x140
#define MMC_RX_ICMP_ERR         0x144

#define MMC_RX_UDP_GD_OCTETS        0x170
#define MMC_RX_UDP_ERR_OCTETS       0x174
#define MMC_RX_TCP_GD_OCTETS        0x178
#define MMC_RX_TCP_ERR_OCTETS       0x17c
#define MMC_RX_ICMP_GD_OCTETS       0x180
#define MMC_RX_ICMP_ERR_OCTETS      0x184

void hobot_mmc_read(void *__iomem addr, struct hobot_counters *mmc);
void hobot_mmc_intr_all_mask(void *__iomem addr);
void hobot_mmc_ctrl(void *__iomem addr, unsigned int mode);


#endif
